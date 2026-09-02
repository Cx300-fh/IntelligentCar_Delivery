const path = require('node:path');

function intEnv(name, fallback) {
  const value = Number.parseInt(process.env[name], 10);
  return Number.isFinite(value) ? value : fallback;
}

module.exports = {
  httpPort: intEnv('HTTP_PORT', 3000),
  host: process.env.HTTP_HOST || '127.0.0.1',
  publicDir: process.env.PUBLIC_DIR || path.join(__dirname, 'public'),
  dbPath: process.env.DB_PATH || path.join(__dirname, 'data', 'delivery_car.db'),

  carTcpHost: process.env.CAR_TCP_HOST || '0.0.0.0',
  carTcpPort: intEnv('CAR_TCP_PORT', 8898),
  carOfflineTimeoutMs: intEnv('CAR_OFFLINE_TIMEOUT_MS', 6000),
  carMaxMessageBytes: intEnv('CAR_MAX_MESSAGE_BYTES', 64 * 1024),

  imagePort: intEnv('IMAGE_PORT', 8888),
  maxJpegBytes: intEnv('MAX_JPEG_BYTES', 512 * 1024),

  voiceModelPort: intEnv('VOICE_MODEL_PORT', 8899),
  externalModelUrl: process.env.VOICE_MODEL_API_URL || '',
  externalModelTimeoutMs: intEnv('VOICE_MODEL_API_TIMEOUT_MS', 1500),

  trafficProvider: process.env.TRAFFIC_PROVIDER || 'mock'
};
