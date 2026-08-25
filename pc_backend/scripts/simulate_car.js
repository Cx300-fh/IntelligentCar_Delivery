const dgram = require('node:dgram');

const port = Number(process.env.CAR_ROUTE_PORT || 8898);
const socket = dgram.createSocket('udp4');

socket.on('message', (buffer, remote) => {
  const raw = buffer.toString('utf8');
  console.log(`RX ${remote.address}:${remote.port} ${raw}`);
  let action = 'unknown';
  try {
    const message = JSON.parse(raw);
    action = message.type || message.action || 'unknown';
  } catch (error) {
    action = 'invalid';
  }

  const response = Buffer.from(JSON.stringify({
    type: 'status_response',
    ok: action !== 'invalid',
    action,
    message: 'simulated car response',
    map_id: 1,
    target_id: 9,
    current_node: 3,
    route_len: 2,
    pending_station: false
  }), 'utf8');

  socket.send(response, remote.port, remote.address);
});

socket.bind(port, () => {
  console.log(`simulated car listening on 0.0.0.0:${port}`);
});
