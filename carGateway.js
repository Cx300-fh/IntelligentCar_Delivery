const net = require('node:net');
const { EventEmitter } = require('node:events');

class CarGateway extends EventEmitter {
  constructor(config) {
    super();
    this.config = config;
    this.server = null;
    this.socket = null;
    this.buffer = '';
    this.lastSeenAt = null;
    this.remote = null;
    this.heartbeatTimer = null;
  }

  start() {
    this.server = net.createServer((socket) => this.accept(socket));
    this.server.on('error', (error) => this.emit('gateway_error', error));
    this.server.listen(this.config.carTcpPort, this.config.carTcpHost, () => {
      this.emit('log', `car TCP server listening on ${this.config.carTcpHost}:${this.config.carTcpPort}`);
    });
    this.heartbeatTimer = setInterval(() => this.checkHeartbeat(), 1000);
    this.heartbeatTimer.unref();
  }

  accept(socket) {
    if (this.socket && !this.socket.destroyed) this.socket.destroy();
    this.socket = socket;
    this.buffer = '';
    this.lastSeenAt = Date.now();
    this.remote = `${socket.remoteAddress}:${socket.remotePort}`;
    socket.setKeepAlive(true, 2000);
    socket.setNoDelay(true);
    socket.setEncoding('utf8');
    socket.on('data', (chunk) => this.onData(socket, chunk));
    socket.on('close', () => this.onClose(socket));
    socket.on('error', (error) => this.emit('car_error', { error, remote: this.remote }));
    this.emit('connection', { remote: this.remote });
  }

  onData(socket, chunk) {
    if (socket !== this.socket) return;
    this.lastSeenAt = Date.now();
    this.buffer += chunk;
    if (Buffer.byteLength(this.buffer, 'utf8') > this.config.carMaxMessageBytes) {
      this.emit('car_error', { error: new Error('car message buffer exceeded limit'), remote: this.remote });
      socket.destroy();
      return;
    }
    let newline;
    while ((newline = this.buffer.indexOf('\n')) >= 0) {
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      if (!line) continue;
      let message;
      try {
        message = JSON.parse(line);
      } catch (error) {
        this.emit('car_error', { error: new Error('invalid NDJSON from car'), raw: line, remote: this.remote });
        continue;
      }
      this.emit('car_message', message);
    }
  }

  onClose(socket) {
    if (socket !== this.socket) return;
    const remote = this.remote;
    this.socket = null;
    this.buffer = '';
    this.remote = null;
    this.emit('offline', { reason: 'socket_closed', remote });
  }

  checkHeartbeat() {
    if (!this.socket || this.lastSeenAt == null) return;
    if (Date.now() - this.lastSeenAt <= this.config.carOfflineTimeoutMs) return;
    const socket = this.socket;
    this.emit('offline', { reason: 'heartbeat_timeout', remote: this.remote });
    socket.destroy();
  }

  send(message) {
    if (!this.socket || this.socket.destroyed || !this.socket.writable) return false;
    this.socket.write(`${JSON.stringify(message)}\n`);
    return true;
  }

  getStatus() {
    return {
      connected: Boolean(this.socket && !this.socket.destroyed),
      remote: this.remote,
      last_seen_at: this.lastSeenAt ? new Date(this.lastSeenAt).toISOString() : null
    };
  }

  close() {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    if (this.socket && !this.socket.destroyed) this.socket.destroy();
    if (this.server) this.server.close();
  }
}

module.exports = { CarGateway };
