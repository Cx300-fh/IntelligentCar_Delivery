const dgram = require('node:dgram');
const { EventEmitter } = require('node:events');

class VoiceGateway extends EventEmitter {
  constructor(config) {
    super();
    this.config = config;
    this.socket = dgram.createSocket('udp4');
    this.lastCommand = null;
    this.lastResult = null;
  }

  start() {
    this.socket.on('message', async (buffer, remote) => {
      const raw = buffer.toString('utf8').trim();
      let message;
      try {
        message = JSON.parse(raw);
      } catch (error) {
        this.emit('voice_error', {
          type: 'voice_error',
          message: 'invalid voice json',
          raw,
          remote: `${remote.address}:${remote.port}`
        });
        return;
      }

      this.lastCommand = {
        ...message,
        received_at: new Date().toISOString(),
        remote: `${remote.address}:${remote.port}`
      };
      this.emit('voice_command', this.lastCommand);

      const result = await this.resolveVoiceCommand(message);
      const source = result.source || 'rules';
      this.lastResult = {
        ...result,
        source,
        sent_at: new Date().toISOString(),
        remote: `${remote.address}:${remote.port}`
      };

      const payload = Buffer.from(JSON.stringify(result), 'utf8');
      this.socket.send(payload, remote.port, remote.address);
      this.emit('voice_result', this.lastResult);
    });

    this.socket.bind(this.config.voiceModelPort, () => {
      const address = this.socket.address();
      this.emit('log', `voice model UDP gateway listening on ${address.address}:${address.port}`);
    });
  }

  async resolveVoiceCommand(message) {
    if (this.config.externalModelUrl) {
      try {
        return await this.callExternalModel(message);
      } catch (error) {
        this.emit('voice_error', {
          type: 'voice_error',
          message: `external model failed, fallback to rules: ${error.message}`
        });
      }
    }
    return this.resolveByRules(message);
  }

  async callExternalModel(message) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), this.config.externalModelTimeoutMs);
    try {
      const response = await fetch(this.config.externalModelUrl, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(message),
        signal: controller.signal
      });
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      const result = await response.json();
      return this.withSource(this.normalizeModelResult(result), 'external_model');
    } finally {
      clearTimeout(timer);
    }
  }

  withSource(result, source) {
    return {
      ...result,
      source
    };
  }

  normalizeModelResult(result) {
    if (!result || typeof result !== 'object') {
      return this.invalidResult();
    }
    if (result.type === 'command_result' && result.action) {
      return result;
    }
    if (result.action) {
      return {
        type: 'command_result',
        ...result
      };
    }
    return this.invalidResult();
  }

  resolveByRules(message) {
    const text = String(message.text || '').trim();
    const normalized = text.toLowerCase();

    if (!text) return this.invalidResult();

    if (/(确认|好了|已放入|放好了|已取走|取走了|confirm|ok|done|placed|taken)/i.test(text)) {
      return this.withSource({ type: 'command_result', action: 'confirm' }, 'rules');
    }

    if (/(停止|暂停|取消|急停|stop|pause|cancel)/i.test(text)) {
      return this.withSource({ type: 'command_result', action: 'stop' }, 'rules');
    }

    if (/(状态|到哪|查询|进度|status|where|progress)/i.test(text)) {
      return this.withSource({ type: 'command_result', action: 'query_status' }, 'rules');
    }

    if (/(放入|取件|pickup|place)/i.test(text)) {
      return this.withSource({
        type: 'command_result',
        action: 'speak',
        speak_event: 'pickup_arrived'
      }, 'rules');
    }

    if (/(取走|送达|终点|dropoff|take|arrived)/i.test(text)) {
      return this.withSource({
        type: 'command_result',
        action: 'speak',
        speak_event: 'dropoff_arrived'
      }, 'rules');
    }

    const order = this.tryParseOrder(normalized, message.map_id);
    if (order) {
      return this.withSource({
        type: 'command_result',
        action: 'submit_order',
        ...order
      }, 'rules');
    }

    return this.invalidResult();
  }

  tryParseOrder(text, defaultMapId) {
    const match = text.match(/order\s+(\d+)\s+map\s+(\d+)\s+from\s+(\d+)\s+to\s+(\d+)/i);
    if (match) {
      return {
        order_id: Number(match[1]),
        map_id: Number(match[2]),
        pickup_node: Number(match[3]),
        dropoff_node: Number(match[4])
      };
    }

    const cnMatch = text.match(/订单\s*(\d+).*?(?:地图|map)\s*(\d+).*?(?:起点|from)\s*(\d+).*?(?:终点|to)\s*(\d+)/i);
    if (cnMatch) {
      return {
        order_id: Number(cnMatch[1]),
        map_id: Number(cnMatch[2]),
        pickup_node: Number(cnMatch[3]),
        dropoff_node: Number(cnMatch[4])
      };
    }

    const simple = text.match(/from\s+(\d+)\s+to\s+(\d+)/i);
    if (simple && defaultMapId) {
      return {
        order_id: Date.now() % 100000,
        map_id: Number(defaultMapId),
        pickup_node: Number(simple[1]),
        dropoff_node: Number(simple[2])
      };
    }

    return null;
  }

  invalidResult() {
    return this.withSource({
      type: 'command_result',
      action: 'speak',
      speak_event: 'invalid_command'
    }, 'rules');
  }

  getStatus() {
    return {
      last_command: this.lastCommand,
      last_result: this.lastResult,
      external_model_enabled: Boolean(this.config.externalModelUrl)
    };
  }

  close() {
    this.socket.close();
  }
}

module.exports = { VoiceGateway };
