const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { WebSocketServer } = require('ws');

const config = require('./config');
const { CarClient } = require('./carClient');
const { ImageReceiver } = require('./imageReceiver');
const { VoiceGateway } = require('./voiceGateway');
const { MapWeights } = require('./mapWeights');
const { AppDatabase } = require('./database');

const carClient = new CarClient(config);
const imageReceiver = new ImageReceiver(config);
const voiceGateway = new VoiceGateway(config);
const mapWeights = new MapWeights(config);
const database = new AppDatabase(config.dbPath);

mapWeights.loadConditions(database.loadEdgeConditions());

const runtimeStatus = {
  started_at: new Date().toISOString(),
  http_port: config.httpPort,
  car_ip: config.carIp,
  car_route_port: config.carRoutePort,
  image_port: config.imagePort,
  voice_model_port: config.voiceModelPort,
  db_path: config.dbPath,
  last_error: null
};

function sendJson(res, statusCode, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(body)
  });
  res.end(body);
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', () => {
      if (chunks.length === 0) {
        resolve({});
        return;
      }
      try {
        resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
      } catch (error) {
        reject(new Error('invalid json body'));
      }
    });
    req.on('error', reject);
  });
}

function requireNumber(body, key) {
  const value = Number(body[key]);
  if (!Number.isFinite(value)) {
    throw new Error(`missing or invalid field: ${key}`);
  }
  return value;
}

function serveStatic(req, res) {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const pathname = url.pathname === '/' ? '/index.html' : url.pathname;
  const resolved = path.normalize(path.join(config.publicDir, pathname));

  if (!resolved.startsWith(config.publicDir)) {
    sendJson(res, 403, { ok: false, error: 'forbidden' });
    return;
  }

  fs.readFile(resolved, (error, data) => {
    if (error) {
      sendJson(res, 404, { ok: false, error: 'not found' });
      return;
    }

    const ext = path.extname(resolved);
    const contentType = {
      '.html': 'text/html; charset=utf-8',
      '.css': 'text/css; charset=utf-8',
      '.js': 'application/javascript; charset=utf-8',
      '.json': 'application/json; charset=utf-8',
      '.jpg': 'image/jpeg',
      '.jpeg': 'image/jpeg',
      '.png': 'image/png',
      '.svg': 'image/svg+xml'
    }[ext] || 'application/octet-stream';

    res.writeHead(200, { 'content-type': contentType });
    res.end(data);
  });
}

async function handleApi(req, res) {
  const url = new URL(req.url, `http://${req.headers.host}`);

  try {
    if (req.method === 'GET' && url.pathname === '/api/status') {
      sendJson(res, 200, {
        ok: true,
        runtime: runtimeStatus,
        car: carClient.lastStatus,
        database: {
          latest_car_status: database.latestCarStatus()
        },
        image: imageReceiver.getStatus(),
        voice: voiceGateway.getStatus()
      });
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/maps') {
      sendJson(res, 200, {
        ok: true,
        maps: mapWeights.getMaps()
      });
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/edges') {
      const mapId = Number(url.searchParams.get('map_id') || 1);
      sendJson(res, 200, {
        ok: true,
        map_id: mapId,
        edges: mapWeights.listEdgeConditions(mapId)
      });
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/orders') {
      const limit = Number(url.searchParams.get('limit') || 100);
      sendJson(res, 200, {
        ok: true,
        orders: database.listOrders(limit)
      });
      return;
    }

    if (req.method === 'GET' && url.pathname.startsWith('/api/orders/')) {
      const orderId = Number(url.pathname.split('/').pop());
      const order = database.getOrder(orderId);
      sendJson(res, order ? 200 : 404, {
        ok: Boolean(order),
        order,
        error: order ? undefined : 'order not found'
      });
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/voice-logs') {
      const limit = Number(url.searchParams.get('limit') || 100);
      sendJson(res, 200, {
        ok: true,
        logs: database.listVoiceLogs(limit)
      });
      return;
    }

    if (req.method === 'GET' && url.pathname === '/api/status/history') {
      const limit = Number(url.searchParams.get('limit') || 100);
      sendJson(res, 200, {
        ok: true,
        history: database.listCarStatusHistory(limit)
      });
      return;
    }

    const body = await readJson(req);

    if (req.method === 'POST' && url.pathname === '/api/orders') {
      const order = {
        order_id: requireNumber(body, 'order_id'),
        map_id: requireNumber(body, 'map_id'),
        pickup_node: requireNumber(body, 'pickup_node'),
        dropoff_node: requireNumber(body, 'dropoff_node')
      };
      const response = await carClient.submitOrder(order);
      database.upsertOrder(order, response.ok ? 'submitted' : 'rejected', response);
      sendJson(res, 200, { ok: true, car_response: response });
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/control/confirm') {
      const response = await carClient.confirm();
      sendJson(res, 200, { ok: true, car_response: response });
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/control/stop') {
      const response = await carClient.stop();
      sendJson(res, 200, { ok: true, car_response: response });
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/control/query') {
      const response = await carClient.queryStatus();
      sendJson(res, 200, { ok: true, car_response: response });
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/edges') {
      const mapId = requireNumber(body, 'map_id');
      const condition = mapWeights.setEdgeCondition(mapId, {
        from: requireNumber(body, 'from'),
        to: requireNumber(body, 'to'),
        manual_penalty: Number(body.manual_penalty || 0),
        dynamic_penalty: Number(body.dynamic_penalty || 0),
        blocked: Boolean(body.blocked)
      });
      database.upsertEdgeCondition(condition);
      const response = await carClient.updateEdge(condition);
      database.upsertEdgeCondition(condition, response);
      broadcast({
        type: 'edge_update_result',
        condition,
        car_response: response
      });
      sendJson(res, 200, { ok: true, condition, car_response: response });
      return;
    }

    if (req.method === 'POST' && url.pathname === '/api/map/refresh-weights') {
      const mapId = requireNumber(body, 'map_id');
      const updates = await mapWeights.refreshDynamicWeights(mapId, body.context || {});
      const results = [];
      for (const update of updates) {
        database.upsertEdgeCondition(update);
        try {
          const carResponse = await carClient.updateEdge(update);
          database.upsertEdgeCondition(update, carResponse);
          results.push({
            ok: true,
            condition: update,
            car_response: carResponse
          });
        } catch (error) {
          results.push({
            ok: false,
            condition: update,
            error: error.message
          });
        }
      }
      broadcast({
        type: 'edge_update_result',
        map_id: mapId,
        results
      });
      sendJson(res, 200, { ok: true, map_id: mapId, results });
      return;
    }

    sendJson(res, 404, { ok: false, error: 'api route not found' });
  } catch (error) {
    runtimeStatus.last_error = {
      at: new Date().toISOString(),
      message: error.message
    };
    sendJson(res, 400, { ok: false, error: error.message });
  }
}

const server = http.createServer((req, res) => {
  if (req.url.startsWith('/api/')) {
    handleApi(req, res);
    return;
  }
  serveStatic(req, res);
});

const wss = new WebSocketServer({ server });

function broadcast(payload) {
  const data = JSON.stringify(payload);
  for (const client of wss.clients) {
    if (client.readyState === client.OPEN) {
      client.send(data);
    }
  }
}

function broadcastLog(message) {
  console.log(message);
  broadcast({
    type: 'log',
    message,
    at: new Date().toISOString()
  });
}

wss.on('connection', (socket) => {
  socket.send(JSON.stringify({
    type: 'status',
    runtime: runtimeStatus,
    car: carClient.lastStatus,
    database: {
      latest_car_status: database.latestCarStatus()
    },
    image: imageReceiver.getStatus(),
    voice: voiceGateway.getStatus()
  }));

  socket.on('message', async (data) => {
    let message;
    try {
      message = JSON.parse(data.toString('utf8'));
    } catch (error) {
      socket.send(JSON.stringify({ type: 'error', error: 'invalid websocket json' }));
      return;
    }

    try {
      if (message.type === 'submit_order') {
        socket.send(JSON.stringify({
          type: 'status_response',
          car_response: await carClient.submitOrder(message)
        }));
      } else if (message.type === 'confirm') {
        socket.send(JSON.stringify({
          type: 'status_response',
          car_response: await carClient.confirm()
        }));
      } else if (message.type === 'stop') {
        socket.send(JSON.stringify({
          type: 'status_response',
          car_response: await carClient.stop()
        }));
      } else {
        socket.send(JSON.stringify({ type: 'error', error: `unsupported websocket type: ${message.type}` }));
      }
    } catch (error) {
      socket.send(JSON.stringify({ type: 'error', error: error.message }));
    }
  });
});

carClient.on('log', broadcastLog);
carClient.on('car_message', (message) => {
  if (message.type === 'status_response') {
    database.insertCarStatus(message);
    if (message.order_id) {
      database.updateOrderResponse(message.order_id, message.ok ? 'active' : 'error', message);
    }
  }
  broadcast(message);
});
carClient.on('car_error', (message) => broadcast(message));

imageReceiver.on('log', broadcastLog);
imageReceiver.on('image_frame', (frame) => broadcast(frame));
imageReceiver.on('image_error', (message) => broadcast(message));

voiceGateway.on('log', broadcastLog);
voiceGateway.on('voice_command', (message) => broadcast(message));
voiceGateway.on('voice_result', (message) => {
  database.insertVoiceLog({
    request: voiceGateway.lastCommand,
    response: message,
    source: message.source,
    remote: message.remote
  });
  broadcast(message);
});
voiceGateway.on('voice_error', (message) => broadcast(message));

server.listen(config.httpPort, config.host, () => {
  console.log(`PC gateway ready: http://${config.host}:${config.httpPort}`);
  console.log(`car route UDP target: ${config.carIp}:${config.carRoutePort}`);
});

carClient.start();
imageReceiver.start();
voiceGateway.start();

function shutdown() {
  carClient.close();
  imageReceiver.close();
  voiceGateway.close();
  database.close();
  server.close(() => process.exit(0));
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
