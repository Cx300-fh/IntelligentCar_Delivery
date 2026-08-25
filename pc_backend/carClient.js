const dgram = require('node:dgram');
const { EventEmitter } = require('node:events');

class CarClient extends EventEmitter {
  constructor(config) {
    super();
    this.config = config;
    this.socket = dgram.createSocket('udp4');
    this.pending = new Map();
    this.lastStatus = {
      connected: false,
      lastSeenAt: null,
      lastMessage: null
    };

    this.socket.on('message', (buffer, remote) => {
      const text = buffer.toString('utf8').trim();
      let message;
      try {
        message = JSON.parse(text);
      } catch (error) {
        this.emit('car_error', {
          type: 'car_error',
          message: 'invalid car json',
          raw: text,
          remote
        });
        return;
      }

      this.lastStatus = {
        connected: true,
        lastSeenAt: new Date().toISOString(),
        remote: `${remote.address}:${remote.port}`,
        lastMessage: message
      };
      this.emit('car_message', message);

      if (message.type === 'status_response') {
        const key = message.action || 'unknown';
        const queue = this.pending.get(key);
        if (queue && queue.length > 0) {
          const pending = queue.shift();
          clearTimeout(pending.timer);
          pending.resolve(message);
        }
      }
    });
  }

  start() {
    this.socket.bind(0, () => {
      const address = this.socket.address();
      this.emit('log', `car UDP client bound on ${address.address}:${address.port}`);
    });
  }

  close() {
    this.socket.close();
  }

  sendCommand(command, options = {}) {
    return new Promise((resolve, reject) => {
      const payload = Buffer.from(JSON.stringify(command), 'utf8');
      const action = command.type || command.action || 'unknown';
      const timeoutMs = options.timeoutMs || this.config.carCommandTimeoutMs;

      const timer = setTimeout(() => {
        const queue = this.pending.get(action);
        if (queue) {
          const index = queue.findIndex((item) => item.reject === reject);
          if (index >= 0) queue.splice(index, 1);
        }
        this.lastStatus.connected = false;
        reject(new Error(`car command timeout: ${action}`));
      }, timeoutMs);

      if (!this.pending.has(action)) this.pending.set(action, []);
      this.pending.get(action).push({ resolve, reject, timer });

      this.socket.send(
        payload,
        this.config.carRoutePort,
        this.config.carIp,
        (error) => {
          if (!error) return;
          clearTimeout(timer);
          const queue = this.pending.get(action);
          if (queue) {
            const index = queue.findIndex((item) => item.reject === reject);
            if (index >= 0) queue.splice(index, 1);
          }
          reject(error);
        }
      );
    });
  }

  submitOrder(order) {
    return this.sendCommand({
      type: 'submit_order',
      order_id: Number(order.order_id),
      map_id: Number(order.map_id),
      pickup_node: Number(order.pickup_node),
      dropoff_node: Number(order.dropoff_node)
    });
  }

  confirm() {
    return this.sendCommand({ type: 'confirm' });
  }

  stop() {
    return this.sendCommand({ type: 'stop' });
  }

  queryStatus() {
    return this.sendCommand({ type: 'query_status' });
  }

  updateEdge(edge) {
    return this.sendCommand({
      type: 'edge_update',
      from: Number(edge.from),
      to: Number(edge.to),
      manual_penalty: Number(edge.manual_penalty || 0),
      dynamic_penalty: Number(edge.dynamic_penalty || 0),
      blocked: Boolean(edge.blocked)
    });
  }
}

module.exports = { CarClient };
