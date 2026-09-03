const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { WebSocketServer, WebSocket } = require('ws');

const config = require('./config');
const { ImageReceiver } = require('./imageReceiver');
const { VoiceGateway } = require('./voiceGateway');
const { MapWeights } = require('./mapWeights');
const { AppDatabase } = require('./database');
const { RoutePlanner } = require('./routePlanner');
const { CarGateway } = require('./carGateway');
const { DispatchScheduler } = require('./scheduler');
const { SnapshotBuilder, decorateOrder } = require('./snapshot');
const { PROTOCOL_VERSION, VEHICLE_ID, ROBOT_SPEED_UNITS_PER_SEC } = require('./domain');

const database = new AppDatabase(config.dbPath);
const mapWeights = new MapWeights(config);
mapWeights.loadConditions(database.loadEdgeConditions());
database.syncLocations(mapWeights.getMaps());

const routePlanner = new RoutePlanner(mapWeights);
const carGateway = new CarGateway(config);
const imageReceiver = new ImageReceiver(config);
const voiceGateway = new VoiceGateway(config);
const snapshotBuilder = new SnapshotBuilder(database, routePlanner);
const scheduler = new DispatchScheduler(database, routePlanner, carGateway, () => snapshotBuilder.build());

const runtimeStatus = {
  started_at: new Date().toISOString(),
  http: `${config.host}:${config.httpPort}`,
  car_tcp: `${config.carTcpHost}:${config.carTcpPort}`,
  image_udp_port: config.imagePort,
  voice_tcp_port: config.voiceModelPort,
  db_path: config.dbPath,
  protocol_version: PROTOCOL_VERSION,
  last_error: null
};

function sendJson(res, statusCode, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(body),
    'access-control-allow-origin': '*',
    'access-control-allow-headers': 'content-type',
    'access-control-allow-methods': 'GET,POST,OPTIONS'
  });
  res.end(body);
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let length = 0;
    req.on('data', (chunk) => {
      length += chunk.length;
      if (length > 1024 * 1024) {
        reject(Object.assign(new Error('request body too large'), { code: 'BODY_TOO_LARGE' }));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => {
      if (chunks.length === 0) return resolve({});
      try {
        resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
      } catch (_error) {
        reject(Object.assign(new Error('invalid JSON body'), { code: 'INVALID_JSON' }));
      }
    });
    req.on('error', reject);
  });
}

function errorStatus(error) {
  if (String(error.code || '').includes('NOT_FOUND')) return 404;
  if (String(error.code || '').includes('CONFLICT') || error.code === 'CAR_FULL') return 409;
  return 400;
}

function serveStatic(req, res) {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = url.pathname === '/' ? '/index.html' : url.pathname;
  const publicRoot = path.resolve(config.publicDir);
  const resolved = path.resolve(publicRoot, `.${pathname}`);
  if (resolved !== publicRoot && !resolved.startsWith(`${publicRoot}${path.sep}`)) {
    sendJson(res, 403, { ok: false, error_code: 'FORBIDDEN', error: 'forbidden' });
    return;
  }
  fs.readFile(resolved, (error, data) => {
    if (error) return sendJson(res, 404, { ok: false, error_code: 'NOT_FOUND', error: 'not found' });
    const contentType = {
      '.html': 'text/html; charset=utf-8',
      '.css': 'text/css; charset=utf-8',
      '.js': 'application/javascript; charset=utf-8',
      '.json': 'application/json; charset=utf-8',
      '.jpg': 'image/jpeg',
      '.jpeg': 'image/jpeg',
      '.png': 'image/png',
      '.svg': 'image/svg+xml'
    }[path.extname(resolved)] || 'application/octet-stream';
    res.writeHead(200, { 'content-type': contentType });
    res.end(data);
  });
}

function makeEdgeCommand(condition) {
  const vehicle = database.getVehicle();
  const command = {
    protocol_version: PROTOCOL_VERSION,
    type: 'edge_update',
    message_id: crypto.randomUUID(),
    vehicle_id: VEHICLE_ID,
    sent_at: new Date().toISOString(),
    command_version: Number(vehicle.last_command_version || 0) + 1,
    map_id: Number(condition.map_id),
    from: Number(condition.from),
    to: Number(condition.to),
    manual_penalty: Number(condition.manual_penalty || 0),
    dynamic_penalty: Number(condition.dynamic_penalty || 0),
    blocked: Boolean(condition.blocked)
  };
  database.createCommand(command);
  database.updateVehicle({ last_command_version: command.command_version });
  const sent = carGateway.send(command);
  if (sent) database.markCommandSent(command.message_id);
  return { command, sent };
}

async function handleApi(req, res) {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'access-control-allow-origin': '*',
      'access-control-allow-headers': 'content-type',
      'access-control-allow-methods': 'GET,POST,OPTIONS'
    });
    res.end();
    return;
  }

  try {
    if (req.method === 'GET' && url.pathname === '/api/snapshot') {
      return sendJson(res, 200, { ok: true, ...snapshotBuilder.build() });
    }
    if (req.method === 'GET' && url.pathname === '/api/status') {
      return sendJson(res, 200, {
        ok: true,
        runtime: runtimeStatus,
        car_connection: carGateway.getStatus(),
        vehicle: database.getVehicle(),
        image: imageReceiver.getStatus(),
        voice: voiceGateway.getStatus()
      });
    }
    if (req.method === 'GET' && url.pathname === '/api/vehicle') {
      return sendJson(res, 200, { ok: true, vehicle: database.getVehicle(), connection: carGateway.getStatus() });
    }
    if (req.method === 'GET' && url.pathname === '/api/trip') {
      return sendJson(res, 200, { ok: true, active_trip: database.getActiveTrip() });
    }
    if (req.method === 'GET' && url.pathname === '/api/locations') {
      return sendJson(res, 200, { ok: true, locations: database.listLocations() });
    }
    if (req.method === 'GET' && url.pathname === '/api/maps') {
      return sendJson(res, 200, { ok: true, maps: mapWeights.getMaps() });
    }
    if (req.method === 'GET' && url.pathname === '/api/edges') {
      const mapId = Number(url.searchParams.get('map_id') || 1);
      return sendJson(res, 200, { ok: true, map_id: mapId, edges: mapWeights.listEdgeConditions(mapId) });
    }
    if (req.method === 'GET' && url.pathname === '/api/orders') {
      const limit = Number(url.searchParams.get('limit') || 100);
      const includeCompleted = url.searchParams.get('include_completed') !== 'false';
      const locations = new Map(database.listLocations().map((item) => [item.location_id, item]));
      return sendJson(res, 200, {
        ok: true,
        orders: database.listOrders(limit, includeCompleted).map((order) => decorateOrder(order, locations))
      });
    }
    if (req.method === 'GET' && url.pathname.startsWith('/api/orders/')) {
      const orderId = decodeURIComponent(url.pathname.slice('/api/orders/'.length));
      const order = database.getOrder(orderId);
      if (!order) return sendJson(res, 404, { ok: false, error_code: 'ORDER_NOT_FOUND', error: 'order not found' });
      const locations = new Map(database.listLocations().map((item) => [item.location_id, item]));
      return sendJson(res, 200, { ok: true, order: decorateOrder(order, locations) });
    }
    if (req.method === 'GET' && url.pathname === '/api/voice-logs') {
      return sendJson(res, 200, { ok: true, logs: database.listVoiceLogs(Number(url.searchParams.get('limit') || 100)) });
    }
    if (req.method === 'GET' && url.pathname === '/api/status/history') {
      return sendJson(res, 200, { ok: true, history: database.listCarStatusHistory(Number(url.searchParams.get('limit') || 100)) });
    }

    const body = await readJson(req);
    if (req.method === 'POST' && url.pathname === '/api/route-preview') {
      const pickup = database.getLocation(body.pickup_location_id);
      const dropoff = database.getLocation(body.dropoff_location_id);
      if (!pickup || !dropoff) {
        const error = new Error('pickup or drop-off location does not exist');
        error.code = 'INVALID_LOCATION';
        throw error;
      }
      if (Number(pickup.map_id) !== 1 || Number(dropoff.map_id) !== 1) {
        const error = new Error('the demo UI currently supports map_id=1 only');
        error.code = 'UNSUPPORTED_MAP';
        throw error;
      }
      const route = routePlanner.shortestPath(1, pickup.node_id, dropoff.node_id);
      if (!route.reachable) {
        const error = new Error('route is currently unreachable');
        error.code = 'ROUTE_UNREACHABLE';
        throw error;
      }
      const queuedOrderCount = database.listOrders(1000, false).length;
      const travelMinutes = Math.max(1, Math.ceil(route.distance / ROBOT_SPEED_UNITS_PER_SEC / 60));
      const estimatedWaitMinutes = queuedOrderCount * 3;
      return sendJson(res, 200, {
        ok: true,
        map_id: 1,
        pickup_location_id: pickup.location_id,
        dropoff_location_id: dropoff.location_id,
        route_nodes: route.path,
        distance: Math.round(route.distance),
        queued_order_count: queuedOrderCount,
        estimated_wait_minutes: estimatedWaitMinutes,
        estimated_delivery_minutes: estimatedWaitMinutes + travelMinutes
      });
    }
    if (req.method === 'POST' && url.pathname === '/api/orders') {
      const result = scheduler.createOrder(body);
      const locations = new Map(database.listLocations().map((item) => [item.location_id, item]));
      const decorated = decorateOrder(result.order, locations);
      console.log(`[order] ${result.created ? 'created' : 'replayed'} ${decorated.order_id} ${decorated.pickup_location_id} -> ${decorated.dropoff_location_id} status=${decorated.status_code}`);
      return sendJson(res, result.created ? 201 : 200, {
        ok: true,
        created: result.created,
        order: decorated
      });
    }
    if (req.method === 'POST' && (url.pathname === '/api/admin/emergency-stop' || url.pathname === '/api/control/stop')) {
      const command = scheduler.emergencyStop();
      return sendJson(res, 202, { ok: true, command, sent: carGateway.getStatus().connected });
    }
    if (req.method === 'POST' && url.pathname === '/api/admin/resume') {
      const command = scheduler.resume();
      return sendJson(res, 202, { ok: true, command, sent: carGateway.getStatus().connected });
    }
    if (req.method === 'POST' && url.pathname.startsWith('/api/admin/orders/') && url.pathname.endsWith('/force-complete')) {
      const orderId = decodeURIComponent(url.pathname.slice('/api/admin/orders/'.length, -'/force-complete'.length));
      const result = scheduler.forceCompleteOrder(orderId);
      const locations = new Map(database.listLocations().map((item) => [item.location_id, item]));
      console.log(`[admin] force-complete ${orderId} -> status=${result.order.status} changed=${result.changed}`);
      return sendJson(res, 200, { ok: true, changed: result.changed, order: decorateOrder(result.order, locations), stop_completed: result.stop_completed || null });
    }
    if (req.method === 'POST' && url.pathname === '/api/control/query') {
      return sendJson(res, 200, { ok: true, snapshot: snapshotBuilder.build() });
    }
    if (req.method === 'POST' && url.pathname === '/api/control/confirm') {
      return sendJson(res, 409, {
        ok: false,
        error_code: 'ORDER_SPECIFIC_CONFIRM_REQUIRED',
        error: 'confirmation must come from the car with order_id, action and order_version'
      });
    }
    if (req.method === 'POST' && url.pathname === '/api/edges') {
      const condition = mapWeights.setEdgeCondition(Number(body.map_id), body);
      database.upsertEdgeCondition(condition);
      const delivery = makeEdgeCommand(condition);
      scheduler.changed('edge_condition_updated');
      return sendJson(res, 202, { ok: true, condition, ...delivery });
    }
    if (req.method === 'POST' && url.pathname === '/api/map/refresh-weights') {
      const mapId = Number(body.map_id);
      const updates = await mapWeights.refreshDynamicWeights(mapId, body.context || {});
      const results = updates.map((condition) => {
        database.upsertEdgeCondition(condition);
        return { condition, ...makeEdgeCommand(condition) };
      });
      scheduler.changed('map_weights_refreshed');
      return sendJson(res, 202, { ok: true, map_id: mapId, results });
    }
    return sendJson(res, 404, { ok: false, error_code: 'API_NOT_FOUND', error: 'API route not found' });
  } catch (error) {
    runtimeStatus.last_error = { at: new Date().toISOString(), code: error.code || 'ERROR', message: error.message };
    return sendJson(res, errorStatus(error), { ok: false, error_code: error.code || 'BAD_REQUEST', error: error.message });
  }
}

const server = http.createServer((req, res) => {
  if (req.url.startsWith('/api/')) return void handleApi(req, res);
  serveStatic(req, res);
});
const wss = new WebSocketServer({ server });

function broadcast(payload) {
  const data = JSON.stringify(payload);
  for (const client of wss.clients) {
    if (client.readyState === WebSocket.OPEN) client.send(data);
  }
}

function broadcastSnapshot(reason) {
  snapshotBuilder.bump();
  broadcast({ ...snapshotBuilder.build(), reason });
}

wss.on('connection', (socket) => {
  socket.send(JSON.stringify(snapshotBuilder.build()));
  socket.on('message', (data) => {
    try {
      const message = JSON.parse(data.toString('utf8'));
      if (message.type === 'get_snapshot') socket.send(JSON.stringify(snapshotBuilder.build()));
      else socket.send(JSON.stringify({ type: 'error', error_code: 'READ_ONLY_WEBSOCKET', error: 'use REST for commands' }));
    } catch (_error) {
      socket.send(JSON.stringify({ type: 'error', error_code: 'INVALID_JSON', error: 'invalid WebSocket JSON' }));
    }
  });
});

scheduler.on('changed', ({ reason }) => broadcastSnapshot(reason));
scheduler.on('error_event', ({ error, message }) => {
  runtimeStatus.last_error = { at: new Date().toISOString(), code: error.code || 'CAR_MESSAGE_ERROR', message: error.message };
  broadcast({ type: 'car_error', error_code: error.code || 'CAR_MESSAGE_ERROR', error: error.message, car_message_type: message?.type });
});

carGateway.on('car_message', (message) => scheduler.handleCarMessage(message));
carGateway.on('offline', ({ reason }) => {
  database.updateVehicle({ online_status: 'OFFLINE', motion_state: 'STOPPED' });
  broadcastSnapshot(`car_offline:${reason}`);
});
carGateway.on('log', (message) => console.log(message));
carGateway.on('gateway_error', (error) => console.error('car gateway error:', error));
carGateway.on('car_error', ({ error }) => broadcast({ type: 'car_error', error: error.message }));

imageReceiver.on('log', (message) => console.log(message));
imageReceiver.on('image_frame', (frame) => broadcast(frame));
imageReceiver.on('image_error', (message) => broadcast(message));

voiceGateway.on('log', (message) => console.log(message));
voiceGateway.on('voice_command', (message) => broadcast(message));
voiceGateway.on('voice_result', (message) => {
  database.insertVoiceLog({ request: voiceGateway.lastCommand, response: message, source: message.source, remote: message.remote });
  broadcast(message);
});
voiceGateway.on('voice_error', (message) => broadcast(message));

server.listen(config.httpPort, config.host, () => {
  console.log(`delivery backend ready: http://${config.host}:${config.httpPort}`);
});
carGateway.start();
imageReceiver.start();
voiceGateway.start();

let shuttingDown = false;
function shutdown() {
  if (shuttingDown) return;
  shuttingDown = true;
  carGateway.close();
  imageReceiver.close();
  voiceGateway.close();
  wss.close();
  server.close(() => {
    database.close();
    process.exit(0);
  });
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
