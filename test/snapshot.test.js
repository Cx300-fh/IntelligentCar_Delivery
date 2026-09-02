const test = require('node:test');
const assert = require('node:assert/strict');
const os = require('node:os');
const path = require('node:path');
const fs = require('node:fs');
const { AppDatabase } = require('../database');
const { MapWeights } = require('../mapWeights');
const { RoutePlanner } = require('../routePlanner');
const { DispatchScheduler } = require('../scheduler');
const { SnapshotBuilder } = require('../snapshot');

function fixture() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'deliverycar-snapshot-test-'));
  const db = new AppDatabase(path.join(dir, 'test.db'));
  const weights = new MapWeights({ trafficProvider: 'mock' });
  db.syncLocations(weights.getMaps());
  const planner = new RoutePlanner(weights);
  const sent = [];
  const gateway = { send(message) { sent.push(message); return true; } };
  const scheduler = new DispatchScheduler(db, planner, gateway, () => ({}));
  const snapshots = new SnapshotBuilder(db, planner);
  return { db, weights, planner, scheduler, snapshots, sent, close: () => db.close() };
}

function envelope(type, fields = {}) {
  return {
    protocol_version: 1,
    type,
    message_id: `${type}-${Math.random()}`,
    vehicle_id: 0,
    sent_at: new Date().toISOString(),
    ...fields
  };
}

test('snapshot current leg exactly matches the persisted command path', (t) => {
  const f = fixture();
  t.after(f.close);
  f.scheduler.createOrder({
    client_request_id: 'snapshot-current', nickname: '甲',
    pickup_location_id: '1:1', dropoff_location_id: '1:12'
  });
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 9 }));

  const command = f.sent.find((message) => message.type === 'goto_stop');
  const snapshot = f.snapshots.build();
  const current = snapshot.route_plan.legs.find((leg) => leg.is_current);
  assert.deepEqual(current.route_nodes, command.server_suggested_path);
  assert.equal(current.stop_id, command.stop_id);
  assert.equal(current.route_nodes[0], 9);
  assert.equal(current.route_nodes.at(-1), current.node_id);
});

test('snapshot exposes continuous server-planned future legs after pooling', (t) => {
  const f = fixture();
  t.after(f.close);
  f.scheduler.createOrder({
    client_request_id: 'snapshot-first', nickname: '甲',
    pickup_location_id: '1:10', dropoff_location_id: '1:12'
  });
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 9 }));
  const frozenBefore = f.db.getActiveTrip().frozen_stop_id;
  const frozenPathBefore = f.db.getStop(frozenBefore).route_nodes;

  f.scheduler.createOrder({
    client_request_id: 'snapshot-second', nickname: '乙',
    pickup_location_id: '1:10', dropoff_location_id: '1:11'
  });
  const snapshot = f.snapshots.build();
  assert.equal(snapshot.route_plan.frozen_stop_id, frozenBefore);
  assert.deepEqual(snapshot.route_plan.legs[0].route_nodes, frozenPathBefore);
  assert.ok(snapshot.route_plan.legs.length >= 3);
  for (let index = 0; index < snapshot.route_plan.legs.length; index++) {
    const leg = snapshot.route_plan.legs[index];
    assert.equal(leg.reachable, true);
    assert.equal(leg.route_nodes.at(-1), leg.node_id);
    if (index > 0) {
      assert.equal(leg.route_nodes[0], snapshot.route_plan.legs[index - 1].node_id);
    }
  }
});

test('navigation progress is accepted only for the frozen path and command version', (t) => {
  const f = fixture();
  t.after(f.close);
  f.scheduler.createOrder({
    client_request_id: 'snapshot-progress', nickname: '甲',
    pickup_location_id: '1:10', dropoff_location_id: '1:12'
  });
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 9 }));
  const trip = f.db.getActiveTrip();
  const current = f.snapshots.build().route_plan.legs.find((leg) => leg.is_current);
  f.scheduler.handleCarMessage(envelope('heartbeat', {
    sequence: 2,
    map_id: 1,
    current_node: current.route_nodes[0],
    motion_state: 'MOVING',
    stop_id: trip.frozen_stop_id,
    command_version: trip.command_version,
    path_index: 0,
    prev_node: current.route_nodes[0],
    next_node: current.route_nodes[1],
    segment_progress: 0.4
  }));
  assert.equal(f.snapshots.build().navigation_progress.segment_progress, 0.4);

  f.scheduler.handleCarMessage(envelope('heartbeat', {
    sequence: 3,
    map_id: 1,
    current_node: current.route_nodes[0],
    motion_state: 'MOVING',
    stop_id: trip.frozen_stop_id,
    command_version: trip.command_version,
    path_index: 0,
    prev_node: current.route_nodes[0],
    next_node: 999,
    segment_progress: 0.5
  }));
  assert.equal(f.snapshots.build().navigation_progress, null);
});

test('idle snapshot has phase zero and no route plan', (t) => {
  const f = fixture();
  t.after(f.close);
  const snapshot = f.snapshots.build();
  assert.equal(snapshot.screen_phase, 0);
  assert.equal(snapshot.route_plan, null);
  assert.equal(snapshot.navigation_progress, null);
});
