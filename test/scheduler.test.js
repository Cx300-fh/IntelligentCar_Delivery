const test = require('node:test');
const assert = require('node:assert/strict');
const os = require('node:os');
const path = require('node:path');
const fs = require('node:fs');
const { AppDatabase } = require('../database');
const { MapWeights } = require('../mapWeights');
const { RoutePlanner } = require('../routePlanner');
const { DispatchScheduler, capacityFeasible } = require('../scheduler');

function fixture() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'deliverycar-test-'));
  const db = new AppDatabase(path.join(dir, 'test.db'));
  const weights = new MapWeights({ trafficProvider: 'mock' });
  db.syncLocations(weights.getMaps());
  const sent = [];
  const gateway = { send(message) { sent.push(message); return true; } };
  const scheduler = new DispatchScheduler(db, new RoutePlanner(weights), gateway, () => ({}));
  return { db, scheduler, sent, close: () => db.close() };
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

test('offline order queues, then full lifecycle is strictly 1 to 5', (t) => {
  const f = fixture();
  t.after(f.close);
  const orderId = f.scheduler.createOrder({
    client_request_id: 'flow-1', nickname: '小明',
    pickup_location_id: '1:1', dropoff_location_id: '1:3'
  }).order.order_id;
  assert.equal(f.db.getOrder(orderId).dispatch_state, 'UNASSIGNED');
  assert.equal(f.sent.length, 0);

  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 1, motion_state: 'STOPPED' }));
  let trip = f.db.getActiveTrip();
  let stop = f.db.getStop(trip.frozen_stop_id);
  f.scheduler.handleCarMessage(envelope('arrived', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, command_version: trip.command_version,
    map_id: 1, node_id: 1, tag_id: 1
  }));
  assert.equal(f.db.getOrder(orderId).status, 2);
  let order = f.db.getOrder(orderId);
  f.scheduler.handleCarMessage(envelope('user_action', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, order_id: orderId,
    action: 'CONFIRM_PICKUP_LOADED', expected_status: 2, order_version: order.version
  }));
  assert.equal(f.db.getOrder(orderId).status, 3);

  trip = f.db.getActiveTrip();
  stop = f.db.getStop(trip.frozen_stop_id);
  f.scheduler.handleCarMessage(envelope('arrived', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, command_version: trip.command_version,
    map_id: 1, node_id: 3, tag_id: 3
  }));
  assert.equal(f.db.getOrder(orderId).status, 4);
  order = f.db.getOrder(orderId);
  f.scheduler.handleCarMessage(envelope('user_action', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, order_id: orderId,
    action: 'CONFIRM_DROPOFF_TAKEN', expected_status: 4, order_version: order.version
  }));
  assert.equal(f.db.getOrder(orderId).status, 5);
  assert.equal(f.db.getActiveTrip(), null);
  assert.equal(f.db.getVehicle().loaded_count, 0);
});

test('new order never changes the frozen current stop', (t) => {
  const f = fixture();
  t.after(f.close);
  const first = f.scheduler.createOrder({
    client_request_id: 'first', nickname: '甲',
    pickup_location_id: '1:1', dropoff_location_id: '1:3'
  }).order;
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 1 }));
  const before = f.db.getActiveTrip();
  const frozen = f.db.getStop(before.frozen_stop_id);
  assert.deepEqual(frozen.operations.map((item) => item.order_id), [first.order_id]);

  f.scheduler.createOrder({
    client_request_id: 'second', nickname: '乙',
    pickup_location_id: '1:1', dropoff_location_id: '1:3'
  });
  const after = f.db.getActiveTrip();
  assert.equal(after.frozen_stop_id, before.frozen_stop_id);
  assert.deepEqual(f.db.getStop(after.frozen_stop_id).operations.map((item) => item.order_id), [first.order_id]);
  assert.ok(after.stops.some((stop) => stop.state === 'PENDING' && stop.operations.some((op) => op.order_id !== first.order_id)));
});

test('capacity rule allows five but rejects six loaded orders', () => {
  const pickups = (count) => [{
    operations: Array.from({ length: count }, (_, index) => ({ order_id: String(index), action: 'PICKUP' }))
  }];
  assert.equal(capacityFeasible(pickups(5), 0), true);
  assert.equal(capacityFeasible(pickups(6), 0), false);
});

test('wrong AprilTag/node arrival is rejected without changing order', (t) => {
  const f = fixture();
  t.after(f.close);
  const order = f.scheduler.createOrder({
    client_request_id: 'bad-arrival', nickname: '甲',
    pickup_location_id: '1:3', dropoff_location_id: '1:5'
  }).order;
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 1 }));
  const trip = f.db.getActiveTrip();
  const stop = f.db.getStop(trip.frozen_stop_id);
  f.scheduler.handleCarMessage(envelope('arrived', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, command_version: trip.command_version,
    map_id: 1, node_id: 4, tag_id: 4
  }));
  assert.equal(f.db.getOrder(order.order_id).status, 1);
  assert.equal(f.sent.at(-1).type, 'event_ack');
  assert.equal(f.sent.at(-1).accepted, false);
});

test('replayed user action returns the stored acknowledgement', (t) => {
  const f = fixture();
  t.after(f.close);
  const orderId = f.scheduler.createOrder({
    client_request_id: 'dedupe', nickname: '甲',
    pickup_location_id: '1:1', dropoff_location_id: '1:3'
  }).order.order_id;
  f.scheduler.handleCarMessage(envelope('hello', { map_id: 1, current_node: 1 }));
  const trip = f.db.getActiveTrip();
  const stop = f.db.getStop(trip.frozen_stop_id);
  f.scheduler.handleCarMessage(envelope('arrived', {
    trip_id: trip.trip_id, stop_id: stop.stop_id, command_version: trip.command_version,
    map_id: 1, node_id: 1
  }));
  const order = f.db.getOrder(orderId);
  const action = envelope('user_action', {
    message_id: 'same-action-id', trip_id: trip.trip_id, stop_id: stop.stop_id,
    order_id: orderId, action: 'CONFIRM_PICKUP_LOADED', expected_status: 2,
    order_version: order.version
  });
  f.scheduler.handleCarMessage(action);
  f.scheduler.handleCarMessage(action);
  assert.equal(f.db.getOrder(orderId).status, 3);
  assert.equal(f.sent.at(-1).accepted, true);
});
