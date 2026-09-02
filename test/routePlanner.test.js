const test = require('node:test');
const assert = require('node:assert/strict');
const { MapWeights } = require('../mapWeights');
const { RoutePlanner } = require('../routePlanner');

test('THU route preview returns an authoritative reachable path', () => {
  const weights = new MapWeights({ trafficProvider: 'mock' });
  const planner = new RoutePlanner(weights);
  const route = planner.shortestPath(1, 3, 9);
  assert.equal(route.reachable, true);
  assert.equal(route.path[0], 3);
  assert.equal(route.path.at(-1), 9);
  assert.ok(route.distance > 0);
});

test('blocked THU edges are excluded from route preview', () => {
  const weights = new MapWeights({ trafficProvider: 'mock' });
  weights.setEdgeCondition(1, { from: 1, to: 3, blocked: true });
  weights.setEdgeCondition(1, { from: 2, to: 3, blocked: true });
  const planner = new RoutePlanner(weights);
  assert.equal(planner.shortestPath(1, 3, 9).reachable, false);
});

test('pathCost evaluates the exact supplied path without choosing another route', () => {
  const weights = new MapWeights({ trafficProvider: 'mock' });
  const planner = new RoutePlanner(weights);
  const path = [9, 10, 11, 12];
  const cost = planner.pathCost(1, path);
  assert.equal(cost.reachable, true);
  assert.equal(cost.distance, 245);
  weights.setEdgeCondition(1, { from: 10, to: 11, blocked: true });
  assert.equal(planner.pathCost(1, path).reachable, false);
});
