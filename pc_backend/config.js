const path = require('node:path');

function intEnv(name, fallback) {
  const value = Number.parseInt(process.env[name], 10);
  return Number.isFinite(value) ? value : fallback;
}

module.exports = {
  httpPort: intEnv('HTTP_PORT', 3000),
  host: process.env.HTTP_HOST || '127.0.0.1',
  publicDir: process.env.PUBLIC_DIR || path.join(__dirname, 'public'),

  carIp: process.env.CAR_IP || '10.99.90.240',
  carRoutePort: intEnv('CAR_ROUTE_PORT', 8898),
  carCommandTimeoutMs: intEnv('CAR_COMMAND_TIMEOUT_MS', 1200),

  imagePort: intEnv('IMAGE_PORT', 8888),
  maxJpegBytes: intEnv('MAX_JPEG_BYTES', 512 * 1024),

  voiceModelPort: intEnv('VOICE_MODEL_PORT', 8899),
  externalModelUrl: process.env.VOICE_MODEL_API_URL || '',
  externalModelTimeoutMs: intEnv('VOICE_MODEL_API_TIMEOUT_MS', 1500),

  trafficProvider: process.env.TRAFFIC_PROVIDER || 'mock'
};
