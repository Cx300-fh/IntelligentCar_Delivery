const dgram = require('node:dgram');
const { EventEmitter } = require('node:events');

class ImageReceiver extends EventEmitter {
  constructor(config) {
    super();
    this.config = config;
    this.socket = dgram.createSocket('udp4');
    this.expectedLength = 0;
    this.lastFrameAt = null;
    this.frameCount = 0;
  }

  start() {
    this.socket.on('message', (buffer, remote) => {
      if (buffer.length === 4) {
        const length = buffer.readUInt32LE(0);
        if (length <= 0 || length > this.config.maxJpegBytes) {
          this.expectedLength = 0;
          this.emit('image_error', {
            type: 'image_error',
            message: `invalid jpeg length: ${length}`,
            remote: `${remote.address}:${remote.port}`
          });
          return;
        }
        this.expectedLength = length;
        return;
      }

      if (this.expectedLength > 0 && buffer.length === this.expectedLength) {
        this.frameCount += 1;
        this.lastFrameAt = new Date().toISOString();
        this.emit('image_frame', {
          type: 'image_frame',
          encoding: 'jpeg',
          frame_count: this.frameCount,
          received_at: this.lastFrameAt,
          remote: `${remote.address}:${remote.port}`,
          bytes: buffer.length,
          data: buffer.toString('base64')
        });
        this.expectedLength = 0;
        return;
      }

      if (buffer.length > 2 && buffer[0] === 0xff && buffer[1] === 0xd8) {
        this.frameCount += 1;
        this.lastFrameAt = new Date().toISOString();
        this.emit('image_frame', {
          type: 'image_frame',
          encoding: 'jpeg',
          frame_count: this.frameCount,
          received_at: this.lastFrameAt,
          remote: `${remote.address}:${remote.port}`,
          bytes: buffer.length,
          data: buffer.toString('base64')
        });
        return;
      }

      this.emit('image_error', {
        type: 'image_error',
        message: `unexpected image packet length: ${buffer.length}`,
        expected_length: this.expectedLength,
        remote: `${remote.address}:${remote.port}`
      });
      this.expectedLength = 0;
    });

    this.socket.bind(this.config.imagePort, () => {
      const address = this.socket.address();
      this.emit('log', `image UDP receiver listening on ${address.address}:${address.port}`);
    });
  }

  getStatus() {
    return {
      frame_count: this.frameCount,
      last_frame_at: this.lastFrameAt,
      expected_length: this.expectedLength
    };
  }

  close() {
    this.socket.close();
  }
}

module.exports = { ImageReceiver };
