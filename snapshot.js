const { ORDER_STATUS, statusCode } = require('./domain');

const PHASE_CONTENT = Object.freeze({
  1: {
    status_line: '等待前序订单完成',
    question: '此订单尚未开始',
    description: '车辆完成当前配送后将自动处理。',
    action_label: '等待中',
    action: null
  },
  2: {
    status_line: '已到取件点 · 等待装载确认',
    question: '所有物品已放入车内？',
    description: '确认后，车辆会继续前往目的地。',
    action_label: '物品已装好',
    action: 'CONFIRM_PICKUP_LOADED'
  },
  3: {
    status_line: '配送中 · 前往目的地',
    question: '车辆正在配送',
    description: '车辆到达目的地后会停车等待取件。',
    action_label: '配送中',
    action: null
  },
  4: {
    status_line: '已到目的地 · 等待取件确认',
    question: '所有物品已从车内取走？',
    description: '确认后完成此订单，并继续处理下一单。',
    action_label: '物品已取走',
    action: 'CONFIRM_DROPOFF_TAKEN'
  },
  5: {
    status_line: '订单已完成',
    question: '物品已成功送达',
    description: '该订单不需要进一步操作。',
    action_label: '已完成',
    action: null
  }
});

function decorateOrder(order, locationsById) {
  const phase = Number(order.status);
  const base = PHASE_CONTENT[phase];
  const pickup = locationsById.get(order.pickup_location_id);
  const dropoff = locationsById.get(order.dropoff_location_id);
  const content = phase === ORDER_STATUS.QUEUED && order.dispatch_state === 'TO_PICKUP'
    ? {
        ...base,
        status_line: '前往取件点',
        question: '车辆正在前往取件点',
        description: '到达后请将对应物品放入车内。'
      }
    : base;
  return {
    order_id: order.order_id,
    display_no: order.display_no,
    nickname: order.nickname,
    status: phase,
    status_code: statusCode(phase),
    dispatch_state: order.dispatch_state,
    version: Number(order.version),
    map_id: Number(order.map_id),
    pickup: pickup ? {
      location_id: pickup.location_id,
      node_id: Number(pickup.node_id),
      name: pickup.name
    } : null,
    dropoff: dropoff ? {
      location_id: dropoff.location_id,
      node_id: Number(dropoff.node_id),
      name: dropoff.name
    } : null,
    item_summary: order.item_summary,
    item_count: Number(order.item_count),
    note: order.note,
    created_at: order.created_at,
    updated_at: order.updated_at,
    display: {
      d_s_1: content.status_line,
      d_s_2: content.question,
      d_s_3: content.description,
      d_s_4: content.action_label,
      action: content.action
    }
  };
}

class SnapshotBuilder {
  constructor(database, routePlanner = null) {
    this.database = database;
    this.routePlanner = routePlanner;
    this.version = 1;
  }

  bump() {
    this.version += 1;
    return this.version;
  }

  build() {
    const locations = this.database.listLocations();
    const locationsById = new Map(locations.map((location) => [location.location_id, location]));
    const rawOrders = this.database.listOrders(1000, true);
    const orders = rawOrders.map((order) => decorateOrder(order, locationsById));
    const activeTrip = this.database.getActiveTrip();
    const vehicle = this.database.getVehicle();
    const currentOrderId = this.chooseCurrentOrder(activeTrip, rawOrders);
    const current = orders.find((order) => order.order_id === currentOrderId) || null;
    const routePlan = this.buildRoutePlan(activeTrip, vehicle);
    return {
      type: 'snapshot',
      snapshot_version: this.version,
      server_time: new Date().toISOString(),
      screen_phase: current ? current.status : 0,
      current_order_id: current?.order_id || null,
      vehicle,
      orders,
      active_trip: activeTrip,
      route_plan: routePlan,
      navigation_progress: this.buildNavigationProgress(activeTrip, vehicle, routePlan),
      locations
    };
  }

  buildRoutePlan(activeTrip, vehicle) {
    if (!activeTrip || !this.routePlanner || vehicle?.current_node == null) return null;
    const remainingStops = (activeTrip.stops || [])
      .filter((stop) => stop.state !== 'COMPLETED')
      .sort((left, right) => Number(left.sequence) - Number(right.sequence));
    if (remainingStops.length === 0) return null;

    let anchorNode = Number(vehicle.current_node);
    const legs = [];
    for (const stop of remainingStops) {
      const isCurrent = stop.stop_id === activeTrip.frozen_stop_id;
      let route;
      if (isCurrent && Array.isArray(stop.route_nodes) && stop.route_nodes.length > 0) {
        const path = stop.route_nodes.map(Number);
        const cost = this.routePlanner.pathCost(activeTrip.map_id, path);
        route = { reachable: cost.reachable, distance: cost.distance, path };
      } else {
        route = this.routePlanner.shortestPath(activeTrip.map_id, anchorNode, stop.node_id);
      }
      const routeNodes = isCurrent && Array.isArray(route.path)
        ? route.path.map(Number)
        : (route.reachable ? route.path.map(Number) : []);
      legs.push({
        stop_id: stop.stop_id,
        sequence: Number(stop.sequence),
        state: stop.state,
        action: stop.stop_type,
        node_id: Number(stop.node_id),
        operations: (stop.operations || []).map(({ order_id, action }) => ({ order_id, action })),
        route_nodes: routeNodes,
        distance: route.reachable ? Math.round(Number(route.distance)) : null,
        reachable: Boolean(route.reachable),
        is_current: isCurrent
      });
      anchorNode = Number(stop.node_id);
    }
    return {
      trip_id: activeTrip.trip_id,
      route_version: Number(activeTrip.route_version),
      frozen_stop_id: activeTrip.frozen_stop_id || null,
      legs
    };
  }

  buildNavigationProgress(activeTrip, vehicle, routePlan) {
    if (!activeTrip?.frozen_stop_id || !routePlan) return null;
    const status = vehicle?.last_status;
    if (!status || status.stop_id !== activeTrip.frozen_stop_id) return null;
    if (Number(status.command_version) !== Number(activeTrip.command_version)) return null;
    const currentLeg = routePlan.legs.find((leg) => leg.is_current);
    if (!currentLeg) return null;

    const base = {
      stop_id: activeTrip.frozen_stop_id,
      command_version: Number(activeTrip.command_version),
      path_index: null,
      prev_node: null,
      next_node: null,
      segment_progress: null,
      motion_state: vehicle.motion_state
    };
    if (vehicle.motion_state !== 'MOVING') return base;

    const pathIndex = Number(status.path_index);
    const prevNode = Number(status.prev_node);
    const nextNode = Number(status.next_node);
    const segmentProgress = Number(status.segment_progress);
    const path = currentLeg.route_nodes;
    if (!Number.isInteger(pathIndex) || pathIndex < 0 || pathIndex >= path.length - 1) return null;
    if (path[pathIndex] !== prevNode || path[pathIndex + 1] !== nextNode) return null;
    if (!Number.isFinite(segmentProgress) || segmentProgress < 0 || segmentProgress > 1) return null;
    return {
      ...base,
      path_index: pathIndex,
      prev_node: prevNode,
      next_node: nextNode,
      segment_progress: segmentProgress
    };
  }

  chooseCurrentOrder(activeTrip, orders) {
    if (activeTrip?.frozen_stop_id) {
      const stop = activeTrip.stops.find((item) => item.stop_id === activeTrip.frozen_stop_id);
      const waiting = stop?.operations.find((operation) => !operation.confirmed_at);
      if (waiting) return waiting.order_id;
    }
    if (activeTrip) {
      for (const orderId of activeTrip.order_ids) {
        const order = orders.find((item) => item.order_id === orderId);
        if (order && Number(order.status) !== ORDER_STATUS.COMPLETED) return orderId;
      }
    }
    return orders.find((order) => Number(order.status) !== ORDER_STATUS.COMPLETED)?.order_id || null;
  }
}

module.exports = { SnapshotBuilder, decorateOrder, PHASE_CONTENT };
