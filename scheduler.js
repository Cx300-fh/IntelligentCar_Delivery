const crypto = require('node:crypto');
const { EventEmitter } = require('node:events');
const {
  VEHICLE_ID,
  PROTOCOL_VERSION,
  CAR_CAPACITY,
  MAX_DETOUR_PERCENT,
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
      server_suggested_path: route.path
    };
    this.database.createCommand(command);
    this.database.issueStop(trip.trip_id, stop.stop_id, commandVersion, route.path);
    this.sendPersisted(command);
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
      if (message.type === 'hello') return this.handleHello(message);
      if (message.type === 'heartbeat') return this.handleHeartbeat(message);
      if (message.type === 'command_ack') return this.handleCommandAck(message);
      if (message.type === 'arrived') return this.handleArrived(message);
      if (message.type === 'user_action') return this.handleUserAction(message);
      throw codedError('UNSUPPORTED_MESSAGE', `unsupported car message: ${message.type}`);
    } catch (error) {
      if (message?.message_id) this.sendEventAck(message, false, { error_code: error.code || 'BAD_MESSAGE', error: error.message });
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
    this.updateVehicleFromCar(message);
    this.sendStateSync();
    const pending = this.database.listUnackedCommands().at(-1);
    if (pending) this.sendPersisted(pending.payload);
    this.reconcile();
    this.changed('car_hello');
  }

  handleHeartbeat(message) {
    this.updateVehicleFromCar(message);
    this.reconcile();
    this.changed('car_heartbeat');
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
    if (!trip || trip.trip_id !== message.trip_id || trip.frozen_stop_id !== message.stop_id) {
      throw codedError('ACTION_STOP_CONFLICT', 'user action does not match the current stop');
    }
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
      stop_id: message.stop_id,
      payload: message,
      occurred_at: message.sent_at
    });
    this.database.confirmStopOperation(message.stop_id, message.order_id, mapping.stopAction);
    const complete = this.database.isStopFullyConfirmed(message.stop_id);
    if (complete) {
      this.database.completeStop(message.stop_id);
      this.database.updateVehicle({ navigation_state: 'IDLE', current_action: 'STOP' });
    }
    this.sendEventAck(message, true, { order: result.order, stop_completed: complete });
    this.sendStateSync();
    this.changed('user_action_confirmed');
    if (complete) this.reconcile();
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
