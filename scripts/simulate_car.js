const net = require('node:net');
const crypto = require('node:crypto');

const host = process.env.CAR_SERVER_HOST || '127.0.0.1';
const port = Number(process.env.CAR_TCP_PORT || 8898);
const autoFlow = process.env.AUTO_FLOW === '1';
const autoFlowDelayMs = Math.max(250, Number(process.env.AUTO_FLOW_DELAY_MS || 250));
const autoFlowTickMs = Math.max(50, Number(process.env.AUTO_FLOW_TICK_MS || 100));
let sequence = 0;
let commandVersion = 0;
let mapId = Number(process.env.CAR_MAP_ID || 1);
let currentNode = Number(process.env.CAR_START_NODE || 1);
let currentTrip = null;
let currentStop = null;
let movement = null;
let movementTimer = null;
let buffer = '';
const acted = new Set();

const socket = net.createConnection({ host, port });

function send(type, fields = {}) {
  const message = {
    protocol_version: 1,
    type,
    message_id: crypto.randomUUID(),
    vehicle_id: 0,
    sent_at: new Date().toISOString(),
    ...fields
  };
  socket.write(`${JSON.stringify(message)}\n`);
  console.log('TX', message);
}

function heartbeat() {
  send('heartbeat', {
    sequence: ++sequence,
    map_id: mapId,
    current_node: currentNode,
    motion_state: movement ? 'MOVING' : 'STOPPED',
    navigation_state: movement ? 'EXECUTING' : (currentStop ? 'ARRIVED' : 'IDLE'),
    current_action: movement ? 'FOLLOW_ROUTE' : (currentStop ? 'WAIT_USER' : 'STOP'),
    trip_id: currentTrip,
    stop_id: currentStop,
    command_version: commandVersion,
    battery_percent: 88,
    ...(movement ? {
      prev_node: movement.prevNode,
      next_node: movement.nextNode,
      segment_progress: movement.segmentProgress,
      path_index: movement.pathIndex
    } : {})
  });
}

function normalizedPath(message) {
  const targetNode = Number(message.target_node);
  let path = Array.isArray(message.server_suggested_path)
    ? message.server_suggested_path.map(Number).filter(Number.isFinite)
    : [];
  if (!path.length || path[0] !== currentNode) path.unshift(currentNode);
  if (path[path.length - 1] !== targetNode) path.push(targetNode);
  return path.filter((node, index) => index === 0 || node !== path[index - 1]);
}

function sendArrival(message) {
  currentNode = Number(message.target_node);
  movement = null;
  movementTimer = null;
  send('arrived', {
    trip_id: message.trip_id,
    stop_id: message.stop_id,
    command_version: commandVersion,
    map_id: mapId,
    node_id: currentNode,
    tag_id: currentNode
  });
}

function startAutoMove(message) {
  if (movementTimer) clearTimeout(movementTimer);
  const path = normalizedPath(message);
  // The server computes this from the real route distance and the shared
  // ROBOT_SPEED_UNITS_PER_SEC constant, so the simulated drive always takes
  // as long as the distance actually warrants instead of a flat per-leg time.
  const duration = Math.max(200, Number(message.estimated_duration_ms) || autoFlowDelayMs);
  if (path.length < 2) {
    movementTimer = setTimeout(() => sendArrival(message), duration);
    return;
  }

  const startedAt = Date.now();
  const segmentCount = path.length - 1;
  const tick = () => {
    const overallProgress = Math.min(1, (Date.now() - startedAt) / duration);
    const scaledProgress = overallProgress * segmentCount;
    const pathIndex = Math.min(segmentCount - 1, Math.floor(scaledProgress));
    const segmentProgress = overallProgress >= 1 ? 1 : scaledProgress - pathIndex;
    movement = {
      path,
      pathIndex,
      prevNode: path[pathIndex],
      nextNode: path[pathIndex + 1],
      segmentProgress
    };
    currentNode = segmentProgress >= 1 ? movement.nextNode : movement.prevNode;
    heartbeat();
    if (overallProgress >= 1) {
      sendArrival(message);
      return;
    }
    movementTimer = setTimeout(tick, autoFlowTickMs);
  };
  tick();
}

function handle(message) {
  console.log('RX', message);
  if (message.type === 'goto_stop') {
    commandVersion = Number(message.command_version);
    mapId = Number(message.map_id);
    currentTrip = message.trip_id;
    currentStop = message.stop_id;
    send('command_ack', {
      reply_to: message.message_id,
      accepted: true,
      command_version: commandVersion,
      path: message.server_suggested_path || [],
      error: null
    });
    if (autoFlow) {
      startAutoMove(message);
    }
  }
  if (message.type === 'state_sync' && autoFlow) {
    const order = (message.orders || []).find((item) =>
      item.order_id === message.current_order_id && (item.status === 2 || item.status === 4)
    );
    if (!order || acted.has(`${order.order_id}:${order.status}`)) return;
    acted.add(`${order.order_id}:${order.status}`);
    setTimeout(() => {
      send('user_action', {
        trip_id: message.active_trip?.trip_id,
        stop_id: message.active_trip?.frozen_stop_id,
        order_id: order.order_id,
        action: order.status === 2 ? 'CONFIRM_PICKUP_LOADED' : 'CONFIRM_DROPOFF_TAKEN',
        expected_status: order.status,
        order_version: order.version
      });
      currentStop = null;
    }, autoFlowDelayMs);
  }
}

socket.setEncoding('utf8');
socket.on('connect', () => {
  send('hello', {
    software_version: 'simulator-1.0',
    last_command_version: commandVersion,
    map_id: mapId,
    current_node: currentNode,
    motion_state: 'STOPPED'
  });
  setInterval(heartbeat, 2000).unref();
});
socket.on('data', (chunk) => {
  buffer += chunk;
  let newline;
  while ((newline = buffer.indexOf('\n')) >= 0) {
    const line = buffer.slice(0, newline).trim();
    buffer = buffer.slice(newline + 1);
    if (line) handle(JSON.parse(line));
  }
});
socket.on('error', (error) => console.error(error.message));
socket.on('close', () => process.exit(0));
