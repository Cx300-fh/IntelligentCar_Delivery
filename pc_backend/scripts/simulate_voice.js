const dgram = require('node:dgram');

const port = Number(process.env.VOICE_MODEL_PORT || 8899);
const host = process.env.VOICE_HOST || '127.0.0.1';
const text = process.argv.slice(2).join(' ') || '确认';
const socket = dgram.createSocket('udp4');

const payload = Buffer.from(JSON.stringify({
  type: 'voice_command',
  text,
  map_id: 1,
  current_node: 3,
  order_active: true
}), 'utf8');

socket.on('message', (buffer) => {
  console.log(buffer.toString('utf8'));
  socket.close();
});

socket.bind(0, () => {
  socket.send(payload, port, host, () => {
    console.log(`sent voice command "${text}" to ${host}:${port}`);
  });
});
