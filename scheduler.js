const crypto = require('node:crypto');
const { EventEmitter } = require('node:events');
const {
  VEHICLE_ID,
  PROTOCOL_VERSION,
  CAR_CAPACITY,
  MAX_DETOUR_PERCENT,
  ROBOT_SPEED_UNITS_PER_SEC,
  ORDER_STATUS,
  DISPATCH_STATE,
  STOP_STATE,
  STOP_ACTION,
  USER_ACTION
} = require('./domain');
const { codedError, nowIso } = require('./database');

function orderStops(order) {
  return [
    {
      location_id: order.pickup_location_id,
      map_id: Number(order.map_id),
      node_id: Number(order.pickup_node),
      stop_type: STOP_ACTION.PICKUP,
      operations: [{ order_id: order.order_id, action: STOP_ACTION.PICKUP }]
    },
    {
      location_id: order.dropoff_location_id,
      map_id: Number(order.map_id),
      node_id: Number(order.dropoff_node),
      stop_type: STOP_ACTION.DROPOFF,
      operations: [{ order_id: order.order_id, action: STOP_ACTION.DROPOFF }]
    }
  ];
}

function clonePlannedStop(stop) {
  return {
    location_id: stop.location_id,
    map_id: Number(stop.map_id),
    node_id: Number(stop.node_id),
    stop_type: stop.stop_type,
    operations: (stop.operations || []).map((operation) => ({
      order_id: operation.order_id,
      action: operation.action
    }))
  };
}

function mergeAdjacentStops(stops) {
  const merged = [];
  for (const source of stops) {
    const stop = clonePlannedStop(source);
    const previous = merged.at(-1);
    if (previous && previous.location_id === stop.location_id && previous.node_id === stop.node_id) {
      previous.operations.push(...stop.operations);
      const actions = new Set(previous.operations.map((operation) => operation.action));
      previous.stop_type = actions.size > 1 ? 'MIXED' : [...actions][0];
    } else {
      merged.push(stop);
    }
  }
  for (const stop of merged) {
    stop.operations.sort((left, right) => {
      if (left.action === right.action) return left.order_id.localeCompare(right.order_id);
      return left.action === STOP_ACTION.DROPOFF ? -1 : 1;
    });
  }
  return merged;
}

function capacityFeasible(stops, initialLoadedCount) {
  let loaded = Number(initialLoadedCount);
  if (loaded < 0 || loaded > CAR_CAPACITY) return false;
  for (const stop of stops) {
    const operations = [...(stop.operations || [])].sort((left) =>
      left.action === STOP_ACTION.DROPOFF ? -1 : 1
    );
    for (const operation of operations) {
      loaded += operation.action === STOP_ACTION.PICKUP ? 1 : -1;
      if (loaded < 0 || loaded > CAR_CAPACITY) return false;
    }
  }
  return loaded >= 0;
}

function findBestInsertion({ planner, mapId, anchorNode, pendingStops, order, loadedCount }) {
  const base = pendingStops.map(clonePlannedStop);
  const baseCost = planner.routeCost(mapId, anchorNode, base);
  if (!baseCost.reachable) return null;
  const [pickup, dropoff] = orderStops(order);
  let best = null;

  for (let pickupIndex = 0; pickupIndex <= base.length; pickupIndex++) {
    const withPickup = base.map(clonePlannedStop);
    withPickup.splice(pickupIndex, 0, pickup);
    for (let dropoffIndex = pickupIndex + 1; dropoffIndex <= withPickup.length; dropoffIndex++) {
      const candidate = withPickup.map(clonePlannedStop);
      candidate.splice(dropoffIndex, 0, dropoff);
      const merged = mergeAdjacentStops(candidate);
      if (!capacityFeasible(merged, loadedCount)) continue;
      const cost = planner.routeCost(mapId, anchorNode, merged);
      if (!cost.reachable) continue;
      const detourPercent = baseCost.distance === 0
        ? 0
        : ((cost.distance - baseCost.distance) / baseCost.distance) * 100;
      if (base.length > 0 && detourPercent > MAX_DETOUR_PERCENT) continue;
      if (!best || cost.distance < best.distance) {
        best = { stops: merged, distance: cost.distance, detour_percent: Math.max(0, detourPercent) };
      }
    }
  }
  return best;
}

class DispatchScheduler extends EventEmitter {
  constructor(database, routePlanner, gateway, snapshotProvider = null) {
    super();
    this.database = database;
    this.routePlanner = routePlanner;
    this.gateway = gateway;
    this.snapshotProvider = snapshotProvider;
    this.reconciling = false;
  }

  setSnapshotProvider(provider) {
    this.snapshotProvider = provider;
  }

  changed(reason) {
    this.emit('changed', { reason, at: nowIso() });
  }

  createOrder(input) {
    const pickup = this.database.getLocation(input.pickup_location_id);
    const dropoff = this.database.getLocation(input.dropoff_location_id);
    if (!pickup || !dropoff) throw codedError('INVALID_LOCATION', 'pickup or drop-off location does not exist');
    if (Number(pickup.map_id) !== Number(dropoff.map_id)) {
      throw codedError('CROSS_MAP_ORDER', 'pickup and drop-off must be on the same map');
    }
    if (!String(input.client_request_id || '').trim()) throw codedError('INVALID_REQUEST', 'client_request_id is required');
    if (!String(input.nickname || '').trim()) throw codedError('INVALID_REQUEST', 'nickname is required');
    if (String(input.client_request_id).length > 128) throw codedError('INVALID_REQUEST', 'client_request_id is too long');
    if (String(input.nickname).trim().length > 50) throw codedError('INVALID_REQUEST', 'nickname is too long');
    const itemCount = Number(input.item_count == null ? 1 : input.item_count);
    if (!Number.isInteger(itemCount) || itemCount < 1 || itemCount > 999) {
      throw codedError('INVALID_REQUEST', 'item_count must be an integer from 1 to 999');
    }
    const result = this.database.createOrder({
      client_request_id: String(input.client_request_id),
      nickname: String(input.nickname).trim(),
      map_id: Number(pickup.map_id),
      pickup_location_id: pickup.location_id,
      pickup_node: Number(pickup.node_id),
      dropoff_location_id: dropoff.location_id,
      dropoff_node: Number(dropoff.node_id),
      item_summary: String(input.item_summary || ''),
      item_count: itemCount,
      note: String(input.note || '')
    });
    this.reconcile();
    this.changed(result.created ? 'order_created' : 'order_create_replayed');
    return { created: result.created, order: this.database.getOrder(result.order.order_id) };
  }

  reconcile() {
    if (this.reconciling) return;
    this.reconciling = true;
    try {
      const vehicle = this.database.getVehicle();
      if (!vehicle || vehicle.online_status !== 'ONLINE' || vehicle.current_node == null) return;

      let trip = this.database.getActiveTrip();
      if (!trip) {
        const first = this.database.listQueuedOrders(Number(vehicle.current_map_id))[0];
        if (!first) return;
        trip = this.database.createTripForOrder(first);
        trip = this.database.savePlannedStops(trip.trip_id, orderStops(first));
      }

      this.insertQueuedOrders(trip, vehicle);
      trip = this.database.getTrip(trip.trip_id);
      if (!trip.frozen_stop_id) {
        const next = this.database.getNextPendingStop(trip.trip_id);
        if (next) {
          this.issueNextStop(trip, next, vehicle);
        } else {
          this.database.completeTrip(trip.trip_id);
          this.changed('trip_completed');
          this.reconciling = false;
          this.reconcile();
          return;
        }
      }
    } finally {
      this.reconciling = false;
    }
  }

  insertQueuedOrders(trip, vehicle) {
    let currentTrip = this.database.getTrip(trip.trip_id);
    let anchorNode = Number(vehicle.current_node);
    let projectedLoadedCount = this.database.countLoadedOrders();
    if (currentTrip.frozen_stop_id) {
      const frozen = this.database.getStop(currentTrip.frozen_stop_id);
      if (frozen) {
        anchorNode = Number(frozen.node_id);
        for (const operation of frozen.operations.filter((item) => !item.confirmed_at)) {
          const order = this.database.getOrder(operation.order_id);
          if (operation.action === STOP_ACTION.PICKUP && Number(order.status) < ORDER_STATUS.DELIVERING) {
            projectedLoadedCount += 1;
          } else if (operation.action === STOP_ACTION.DROPOFF &&
            (Number(order.status) === ORDER_STATUS.DELIVERING || Number(order.status) === ORDER_STATUS.WAIT_DROPOFF_CONFIRM)) {
            projectedLoadedCount -= 1;
          }
        }
      }
    }
    let pending = currentTrip.stops
      .filter((stop) => stop.state === STOP_STATE.PENDING)
      .map(clonePlannedStop);

    for (const order of this.database.listQueuedOrders(currentTrip.map_id)) {
      if (currentTrip.order_ids.includes(order.order_id)) continue;
      const best = findBestInsertion({
        planner: this.routePlanner,
        mapId: currentTrip.map_id,
        anchorNode,
        pendingStops: pending,
        order,
        loadedCount: projectedLoadedCount
      });
      if (!best) continue;
      this.database.assignOrderToTrip(order.order_id, currentTrip.trip_id);
      currentTrip = this.database.savePlannedStops(currentTrip.trip_id, best.stops);
      pending = currentTrip.stops
        .filter((stop) => stop.state === STOP_STATE.PENDING)
        .map(clonePlannedStop);
    }
  }

  issueNextStop(trip, stop, vehicle) {
    const route = this.routePlanner.shortestPath(trip.map_id, vehicle.current_node, stop.node_id);
    if (!route.reachable) throw codedError('ROUTE_UNREACHABLE', 'next stop is unreachable');
    const commandVersion = Math.max(Number(trip.command_version), Number(vehicle.last_command_version)) + 1;
    const command = {
      protocol_version: PROTOCOL_VERSION,
      type: 'goto_stop',
      message_id: crypto.randomUUID(),
      vehicle_id: VEHICLE_ID,
      sent_at: nowIso(),
      trip_id: trip.trip_id,
      stop_id: stop.stop_id,
      command_version: commandVersion,
      map_id: Number(stop.map_id),
      target_node: Number(stop.node_id),
      location_id: stop.location_id,
      location_name: this.database.getLocation(stop.location_id)?.name || stop.location_id,
      operations: stop.operations.map(({ order_id, action }) => ({ order_id, action })),
      server_suggested_path: route.path,
      estimated_duration_ms: Math.max(1, Math.round((route.distance / ROBOT_SPEED_UNITS_PER_SEC) * 1000))
    };
    this.database.createCommand(command);
    this.database.issueStop(trip.trip_id, stop.stop_id, commandVersion, route.path);
    this.sendPersisted(command);
    console.log(`[dispatch] goto_stop ${stop.location_id} (node ${stop.node_id}) via path [${route.path.join(',')}] distance=${Math.round(route.distance)} ops=${JSON.stringify(stop.operations.map((o) => `${o.action}:${o.order_id}`))}`);
    this.changed('stop_issued');
  }

  sendPersisted(command) {
    if (this.gateway.send(command)) {
      this.database.markCommandSent(command.message_id);
      return true;
    }
    return false;
  }

  sendEventAck(event, accepted, extra = {}) {
    const response = {
      protocol_version: PROTOCOL_VERSION,
      type: 'event_ack',
      message_id: crypto.randomUUID(),
      reply_to: event.message_id,
      vehicle_id: VEHICLE_ID,
      sent_at: nowIso(),
      accepted,
      ...extra
    };
    if (event.type === 'arrived' || event.type === 'user_action') {
      this.database.recordVehicleMessage(event, response);
    }
    this.gateway.send(response);
    return response;
  }

  handleCarMessage(message) {
    try {
      this.validateEnvelope(message);
      if (message.type === 'arrived' || message.type === 'user_action') {
        const previous = this.database.getVehicleMessage(message.message_id);
        if (previous) {
          this.gateway.send({ ...previous.response, message_id: crypto.randomUUID(), sent_at: nowIso() });
          return previous.response;
        }
      }
      if (message.type === 'sync_ack') return this.handleSyncAck(message);
      if (message.type === 'hello') return this.handleHello(message);
      if (message.type === 'heartbeat') return this.handleHeartbeat(message);
      if (message.type === 'command_ack') return this.handleCommandAck(message);
      if (message.type === 'arrived') return this.handleArrived(message);
      if (message.type === 'user_action') return this.handleUserAction(message);
      throw codedError('UNSUPPORTED_MESSAGE', `unsupported car message: ${message.type}`);
    } catch (error) {
      if (message?.message_id) this.sendEventAck(message, false, { error_code: error.code || 'BAD_MESSAGE', error: { code: error.code || 'BAD_MESSAGE', message: error.message } });
      this.emit('error_event', { error, message });
      return null;
    }
  }

  validateEnvelope(message) {
    if (Number(message.protocol_version) !== PROTOCOL_VERSION) throw codedError('PROTOCOL_MISMATCH', 'protocol_version must be 1');
    if (Number(message.vehicle_id) !== VEHICLE_ID) throw codedError('INVALID_VEHICLE', 'vehicle_id must be 0');
    if (!message.message_id || !message.type) throw codedError('INVALID_ENVELOPE', 'message_id and type are required');
  }

  updateVehicleFromCar(message) {
    const current = this.database.getVehicle();
    if (message.sequence != null && Number(message.sequence) < Number(current.last_sequence)) return current;
    return this.database.updateVehicle({
      online_status: 'ONLINE',
      motion_state: message.motion_state || current.motion_state,
      navigation_state: message.navigation_state || current.navigation_state,
      current_action: message.current_action || current.current_action,
      current_map_id: message.map_id == null ? current.current_map_id : Number(message.map_id),
      current_node: message.current_node == null ? current.current_node : Number(message.current_node),
      last_sequence: message.sequence == null ? current.last_sequence : Number(message.sequence),
      last_command_version: Math.max(Number(current.last_command_version), Number(message.last_command_version || message.command_version || 0)),
      software_version: message.software_version || current.software_version,
      battery_percent: message.battery_percent == null ? current.battery_percent : Number(message.battery_percent),
      last_heartbeat_at: nowIso(),
      last_status: message
    });
  }

  handleHello(message) {
    // hello 是每个连接的第一条车端消息，标志一个新的会话与新的序号空间。
    // 车端心跳的 sequence 是进程内计数器（hb_seq_），车端进程重启后从 0 重新计数，
    // 而 last_sequence 持久化在库里。不在这里归零的话，updateVehicleFromCar 的
    // "旧序号丢弃"检查会把新会话的每一条心跳整条丢掉，车的位置与运动状态永久停更
    // ——现场表现是网页上车停在旧位置或 current_node=-1，而链路一切正常。
    this.database.updateVehicle({ last_sequence: 0 });
    this.updateVehicleFromCar(message);
    this.sendStateSync();
    const pending = this.database.listUnackedCommands().at(-1);
    if (pending) this.sendPersisted(pending.payload);
    else this.resendFrozenStopIfCarBehind(message);
    this.reconcile();
    this.changed('car_hello');
  }

  // 车端进程重启会丢掉当前任务（本地 command_version 归零），而服务器这边命令早已
  // ACK、当前停靠点已 frozen。此时两条重发路径同时失效：listUnackedCommands 是空的
  // （命令已确认），reconcile 又因 frozen_stop_id 存在而跳过下发。结果是永久死锁——
  // 后端一直等车到站，车在原地 READY 待命，两边都不报错。
  // 协议让 hello 携带 last_command_version 正是为了让服务器识别车端落后，这里据此
  // 重新下发当前冻结停靠点（issueNextStop 会把 command_version 递增，不会被判 STALE）。
  resendFrozenStopIfCarBehind(message) {
    const trip = this.database.getActiveTrip();
    if (!trip || !trip.frozen_stop_id) return;
    // 判据是"车端此刻是否真的在跑这个停靠点"，而不是版本号是否落后。
    // 车端重启会从 delivery_state.json 恢复 command_version，却按设计不恢复任务执行
    // （重启后原地定位、不自行运动）。只比版本号的话两边版本一致、任务其实已经丢了，
    // 后端一直等车到站、车在原地待命，谁也不报错。心跳的 trip_id/stop_id 才是直接证据
    // （车端 has_trip = !cur_trip_.empty()）。
    // trip_id 字段缺失（undefined）说明这条心跳没提供任务信息，不能据此断定车没在跑；
    // 只有车明确报了 trip_id 才拿来比对——显式 null 表示车端确实无任务，仍要重发。
    // 真实车端每条心跳都带这个字段（has_trip = !cur_trip_.empty()），实车行为不变。
    const tripKnown = message.trip_id !== undefined;
    const tripMatches = tripKnown
      ? String(message.trip_id || '') === String(trip.trip_id)
      : true;
    // 车端可能“知道”这个 trip 却并没有在执行它：重启后 state_sync 会把
    // trip/stop 上下文恢复回来，但按设计不恢复导航任务（原地定位待命）。
    // 只比 trip_id/stop_id 会把这种情况误判成“在跑”，于是永不重发，
    // 车永远停在原地——2026-09-03 实测：车端重启后定位到节点10、
    // 心跳报的 trip/stop 与服务器完全一致，但 nav 是空的，两边干耗着都不报错。
    // navigation_state 才是执行与否的直接证据：IDLE/LOCATING 都是停着的。
    // 字段缺失时 navIdle=false，退回原来的判据，不会误重发。
    const navState = String(message.navigation_state || '').toUpperCase();
    const navIdle = navState === 'IDLE' || navState === 'LOCATING';
    const executing = tripMatches &&
                      String(message.stop_id || '') === String(trip.frozen_stop_id) &&
                      !navIdle;
    if (executing) return;
    // 车端刚重启时先发 hello、之后才完成 AprilTag 定位，这期间 current_node 无效。
    // 拿无效起点去规划会直接抛 ROUTE_UNREACHABLE，所以等定位好再下发（心跳会再次触发）。
    const vehicle = this.database.getVehicle();
    if (!vehicle || !(Number(vehicle.current_node) > 0)) return;
    // 节流：车端未 ack 前每 2s 的心跳都会走到这里，不限流会不断抬高 command_version。
    if (this.lastResendAt && Date.now() - this.lastResendAt < 5000) return;
    this.lastResendAt = Date.now();
    const stop = this.database.getStop(trip.frozen_stop_id);
    if (!stop) return;
    // 已经到站的停靠点不重发：车到站后在等屏幕确认，此时它可能已不再上报 trip_id。
    if (stop.arrived_at) return;
    console.log('[resend] car is not executing stop ' + stop.location_id + ' (car trip=' + (message.trip_id || 'none') + ', car cmd_ver=' + (message.command_version ?? '?') + ', trip at v' + trip.command_version + '); re-issuing');
    this.issueNextStop(trip, stop, vehicle);
  }

  // 车端收到 state_sync 后回 sync_ack（reply_to / snapshot_version / accepted / error）。
  // accepted=false 表示车拒绝了这份快照——车端同时会进 FAULT 并停车，是必须暴露的
  // 信号：静默吞掉的话现场只会看到"车不动"而后端一片干净，无从查起。
  handleSyncAck(message) {
    if (message.accepted === false) {
      const error = codedError(message.error?.code || 'SYNC_REJECTED',
        message.error?.message || 'car rejected state_sync');
      this.emit('error_event', { error, message });
    }
    return null;
  }

  handleHeartbeat(message) {
    this.updateVehicleFromCar(message);
    this.reconcile();
    this.resendFrozenStopIfCarBehind(message);
    this.changed('car_heartbeat');
    this.sendHeartbeatAck();
  }

  // 车端保活：car_gateway.hpp 的 connection_timeout_ms=6000，车端 6 秒内收不到
  // 任何服务器消息就判掉线 -> 停车 -> 指数退避重连（用于识别 TCP 半开连接，是
  // 已实车验证的安全特性，不能取消）。车端 delivery_protocol.cpp 早已实现
  // heartbeat_ack 的解析、delivery_controller.cpp 按"保活，无业务动作"处理，
  // 只是服务器侧一直没发。这里补上：每个心跳回一条，链路才能持续。
  sendHeartbeatAck() {
    const snapshot = this.snapshotProvider ? this.snapshotProvider() : {};
    const vehicle = this.database.getVehicle();
    const trip = this.database.getActiveTrip();
    this.gateway.send({
      protocol_version: PROTOCOL_VERSION,
      type: 'heartbeat_ack',
      message_id: crypto.randomUUID(),
      vehicle_id: VEHICLE_ID,
      sent_at: nowIso(),
      // 本后端无 server_epoch 概念；车端该字段必填但只做纪元变化检测，
      // 与 state_sync 保持一致发空串即可（空=不启用检测）。
      server_epoch: '',
      snapshot_version: Number(snapshot.snapshot_version || 0),
      latest_command_version: Math.max(Number(trip?.command_version || 0), Number(vehicle?.last_command_version || 0))
    });
  }

  handleCommandAck(message) {
    const command = this.database.getCommand(message.reply_to);
    if (!command) throw codedError('COMMAND_NOT_FOUND', 'reply_to command does not exist');
    if (Number(message.command_version) !== Number(command.command_version)) {
      throw codedError('COMMAND_VERSION_CONFLICT', 'command_version does not match');
    }
    this.database.acknowledgeCommand(message.reply_to, Boolean(message.accepted), message);
    this.changed('command_acknowledged');
  }

  handleArrived(message) {
    const trip = this.database.getActiveTrip();
    if (!trip || trip.trip_id !== message.trip_id || trip.frozen_stop_id !== message.stop_id) {
      throw codedError('ARRIVAL_TARGET_CONFLICT', 'arrival does not match the frozen stop');
    }
    const stop = this.database.getStop(message.stop_id);
    if (!stop || Number(stop.map_id) !== Number(message.map_id) || Number(stop.node_id) !== Number(message.node_id)) {
      throw codedError('ARRIVAL_NODE_CONFLICT', 'arrival map/node does not match the issued stop');
    }
    if (Number(message.command_version) !== Number(trip.command_version)) {
      throw codedError('COMMAND_VERSION_CONFLICT', 'arrival command_version is stale');
    }
    this.database.markStopArrived(stop.stop_id);
    const updatedOrders = [];
    for (const operation of stop.operations) {
      const order = this.database.getOrder(operation.order_id);
      if (operation.action === STOP_ACTION.PICKUP && Number(order.status) === ORDER_STATUS.QUEUED) {
        updatedOrders.push(this.database.transitionOrder({
          event_id: `arrived:${stop.stop_id}:${order.order_id}:pickup`,
          order_id: order.order_id,
          expected_status: ORDER_STATUS.QUEUED,
          new_status: ORDER_STATUS.WAIT_PICKUP_CONFIRM,
          dispatch_state: DISPATCH_STATE.WAIT_PICKUP,
          event_type: 'PICKUP_ARRIVED',
          trip_id: trip.trip_id,
          stop_id: stop.stop_id,
          payload: message,
          occurred_at: message.sent_at
        }).order);
      } else if (operation.action === STOP_ACTION.DROPOFF && Number(order.status) === ORDER_STATUS.DELIVERING) {
        updatedOrders.push(this.database.transitionOrder({
          event_id: `arrived:${stop.stop_id}:${order.order_id}:dropoff`,
          order_id: order.order_id,
          expected_status: ORDER_STATUS.DELIVERING,
          new_status: ORDER_STATUS.WAIT_DROPOFF_CONFIRM,
          dispatch_state: DISPATCH_STATE.WAIT_DROPOFF,
          event_type: 'DROPOFF_ARRIVED',
          trip_id: trip.trip_id,
          stop_id: stop.stop_id,
          payload: message,
          occurred_at: message.sent_at
        }).order);
      }
    }
    this.database.updateVehicle({ motion_state: 'STOPPED', navigation_state: 'ARRIVED', current_action: 'WAIT_USER', current_map_id: stop.map_id, current_node: stop.node_id });
    this.sendEventAck(message, true, { stop_id: stop.stop_id, orders: updatedOrders.map((order) => ({ order_id: order.order_id, status: order.status, version: order.version })) });
    this.sendStateSync();
    this.changed('stop_arrived');
  }

  handleUserAction(message) {
    const trip = this.database.getActiveTrip();
    if (!trip) {
      throw codedError('ACTION_STOP_CONFLICT', 'user action does not match the current stop (no active trip)');
    }
    // 车端重启后丢失 stop 上下文：cur_trip_/cur_stop_ 仅在收到 goto_stop 时赋值，
    // 而等确认期间 frozen_stop_id 已冻结、reconcile 不再下发命令，车端上报的
    // trip_id/stop_id 为空串（序列化无条件输出，非 null），按"必须精确回显"校验
    // 会被永久拒绝——双向死锁。服务器是权威方：字段为空时按活跃 trip 推断，
    // 非空但不匹配仍然拒绝，下层 expected_status/order_version 状态机校验不变。
    if (message.trip_id && trip.trip_id !== message.trip_id) {
      throw codedError('ACTION_STOP_CONFLICT', 'user action does not match the current stop (trip mismatch)');
    }
    if (message.stop_id && trip.frozen_stop_id !== message.stop_id) {
      throw codedError('ACTION_STOP_CONFLICT', 'user action does not match the current stop (stop mismatch)');
    }
    const stopId = message.stop_id || trip.frozen_stop_id;
    const mapping = {
      [USER_ACTION.CONFIRM_PICKUP_LOADED]: {
        stopAction: STOP_ACTION.PICKUP,
        expected: ORDER_STATUS.WAIT_PICKUP_CONFIRM,
        next: ORDER_STATUS.DELIVERING,
        dispatch: DISPATCH_STATE.TO_DROPOFF,
        eventType: 'PICKUP_CONFIRMED'
      },
      [USER_ACTION.CONFIRM_DROPOFF_TAKEN]: {
        stopAction: STOP_ACTION.DROPOFF,
        expected: ORDER_STATUS.WAIT_DROPOFF_CONFIRM,
        next: ORDER_STATUS.COMPLETED,
        dispatch: DISPATCH_STATE.DONE,
        eventType: 'DROPOFF_CONFIRMED'
      }
    }[message.action];
    if (!mapping) throw codedError('INVALID_USER_ACTION', 'unknown user action');
    if (Number(message.expected_status) !== mapping.expected) throw codedError('STATUS_CONFLICT', 'expected_status does not match action');
    const result = this.database.transitionOrder({
      event_id: message.message_id,
      order_id: message.order_id,
      expected_status: mapping.expected,
      expected_version: message.order_version,
      new_status: mapping.next,
      dispatch_state: mapping.dispatch,
      event_type: mapping.eventType,
      trip_id: trip.trip_id,
      stop_id: stopId,
      payload: message,
      occurred_at: message.sent_at
    });
    this.database.confirmStopOperation(stopId, message.order_id, mapping.stopAction);
    const complete = this.database.isStopFullyConfirmed(stopId);
    if (complete) {
      this.database.completeStop(stopId);
      this.database.updateVehicle({ navigation_state: 'IDLE', current_action: 'STOP' });
    }
    this.sendEventAck(message, true, { order: result.order, stop_completed: complete });
    this.sendStateSync();
    this.changed('user_action_confirmed');
    if (complete) this.reconcile();
  }

  // 管理兜底：把卡住的订单强制推进到 COMPLETED（典型场景：车端重启丢失 stop
  // 上下文导致确认被拒且无法自愈）。仍走 transitionOrder 状态机写入；订单在
  // 活跃 trip 各未完成 stop 里的挂起操作一并确认，避免 reconcile 再次派发它。
  forceCompleteOrder(orderId) {
    const order = this.database.getOrder(orderId);
    if (!order) throw codedError('ORDER_NOT_FOUND', 'order not found');
    if (Number(order.status) === ORDER_STATUS.COMPLETED) {
      return { order, changed: false, stop_completed: null };
    }
    const trip = this.database.getActiveTrip();
    const result = this.database.transitionOrder({
      event_id: `force-complete:${orderId}:${nowIso()}`,
      order_id: orderId,
      expected_status: Number(order.status),
      new_status: ORDER_STATUS.COMPLETED,
      dispatch_state: DISPATCH_STATE.DONE,
      event_type: 'ORDER_FORCE_COMPLETED',
      trip_id: trip ? trip.trip_id : null,
      stop_id: trip ? trip.frozen_stop_id : null,
      payload: { manual: true, via: 'admin api' },
      occurred_at: nowIso()
    });
    let stopCompleted = null;
    if (trip) {
      for (const stop of trip.stops) {
        if (stop.state === 'COMPLETED') continue;
        const op = (stop.operations || []).find((item) => item.order_id === orderId && !item.confirmed_at);
        if (!op) continue;
        this.database.confirmStopOperation(stop.stop_id, orderId, op.action);
        if (this.database.isStopFullyConfirmed(stop.stop_id)) {
          this.database.completeStop(stop.stop_id);
          stopCompleted = stop.stop_id;
        }
      }
    }
    if (stopCompleted) {
      this.database.updateVehicle({ navigation_state: 'IDLE', current_action: 'STOP' });
    }
    this.sendStateSync();
    this.changed('order_force_completed');
    this.reconcile();
    return { order: result.order, changed: true, stop_completed: stopCompleted };
  }

  sendStateSync() {
    const snapshot = this.snapshotProvider ? this.snapshotProvider() : {};
    this.gateway.send({
      ...snapshot,
      protocol_version: PROTOCOL_VERSION,
      type: 'state_sync',
      message_id: crypto.randomUUID(),
      vehicle_id: VEHICLE_ID,
      sent_at: nowIso()
    });
  }

  emergencyStop() {
    const trip = this.database.getActiveTrip();
    if (trip) this.database.setTripPaused(trip.trip_id, true);
    const vehicle = this.database.updateVehicle({ motion_state: 'STOPPED', navigation_state: 'PAUSED', current_action: 'EMERGENCY_STOP' });
    const command = this.makeControlCommand('emergency_stop', trip, vehicle);
    this.database.createCommand(command);
    this.sendPersisted(command);
    this.changed('emergency_stop');
    return command;
  }

  resume() {
    const trip = this.database.getActiveTrip();
    if (trip) this.database.setTripPaused(trip.trip_id, false);
    const vehicle = this.database.updateVehicle({ navigation_state: 'IDLE', current_action: 'STOP' });
    const command = this.makeControlCommand('resume', trip, vehicle);
    this.database.createCommand(command);
    this.sendPersisted(command);
    this.changed('resume');
    return command;
  }

  makeControlCommand(type, trip, vehicle) {
    return {
      protocol_version: PROTOCOL_VERSION,
      type,
      message_id: crypto.randomUUID(),
      vehicle_id: VEHICLE_ID,
      sent_at: nowIso(),
      trip_id: trip?.trip_id || null,
      // 车端 Handle_Resume 要求 trip_id 与 stop_id 双双匹配本地当前任务，否则回
      // ERR_VERSION_CONFLICT "resume target mismatch" 并且不打任何日志——两边都静默，
      // 表现为 resume 下发成功(sent=true)却始终解不开急停。hold/emergency_stop 走
      // 同一个构造函数，一并带上。
      stop_id: trip?.frozen_stop_id || null,
      command_version: Math.max(Number(trip?.command_version || 0), Number(vehicle.last_command_version || 0)) + 1
    };
  }
}

module.exports = {
  DispatchScheduler,
  orderStops,
  mergeAdjacentStops,
  capacityFeasible,
  findBestInsertion
};
