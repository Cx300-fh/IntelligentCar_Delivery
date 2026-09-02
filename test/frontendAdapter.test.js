const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { MAPS } = require('../mapWeights');

const root = path.join(__dirname, '..');

test('production page preserves the original UI except for adapter display hooks', () => {
  const original = fs.readFileSync(path.join(root, 'UI.html'), 'utf8');
  const production = fs.readFileSync(path.join(root, 'public', 'index.html'), 'utf8')
    .replace("(o.statusLabel || o.status)", 'o.status')
    .replace("${m.statusLabel || m.status}", '${m.status}')
    .replace("${o.statusLabel === '等待装载' ? ICON_LOAD : o.statusLabel === '等待取件' ? ICON_UNLOAD : ''}", '')
    .replace("${m.statusLabel === '等待装载' ? ICON_LOAD : m.statusLabel === '等待取件' ? ICON_UNLOAD : ''}", '')
    .replace('<script src="/backend-adapter.js"></script>\r\n', '');
  assert.equal(production, original);
});

test('frontend adapter has explicit geometry for every THU server edge', () => {
  const adapter = fs.readFileSync(path.join(root, 'public', 'backend-adapter.js'), 'utf8');
  for (const [from, to] of MAPS[1].edges) {
    const key = `${Math.min(from, to)}-${Math.max(from, to)}`;
    assert.match(adapter, new RegExp(`['\"]${key}['\"]\\s*:`));
  }
});

test('production adapter consumes server routes and never calls client shortestPath', () => {
  const adapter = fs.readFileSync(path.join(root, 'public', 'backend-adapter.js'), 'utf8');
  assert.match(adapter, /route_plan/);
  assert.match(adapter, /navigation_progress/);
  assert.match(adapter, /route_nodes/);
  assert.doesNotMatch(adapter, /shortestPath\s*\(/);
});
