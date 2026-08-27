const fs = require('node:fs');
const path = require('node:path');
const { DatabaseSync } = require('node:sqlite');

function jsonText(value) {
  return JSON.stringify(value ?? null);
}

class AppDatabase {
  constructor(dbPath) {
    this.dbPath = dbPath;
    fs.mkdirSync(path.dirname(dbPath), { recursive: true });
    this.db = new DatabaseSync(dbPath);
    this.db.exec('PRAGMA journal_mode = WAL');
    this.db.exec('PRAGMA foreign_keys = ON');
    this.initSchema();
  }

  initSchema() {
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS orders (
        order_id INTEGER PRIMARY KEY,
        map_id INTEGER NOT NULL,
        pickup_node INTEGER NOT NULL,
        dropoff_node INTEGER NOT NULL,
        status TEXT NOT NULL DEFAULT 'submitted',
        submitted_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        last_car_response TEXT
      );

      CREATE TABLE IF NOT EXISTS edge_conditions (
        map_id INTEGER NOT NULL,
        from_node INTEGER NOT NULL,
        to_node INTEGER NOT NULL,
        manual_penalty INTEGER NOT NULL DEFAULT 0,
        dynamic_penalty INTEGER NOT NULL DEFAULT 0,
        blocked INTEGER NOT NULL DEFAULT 0,
        updated_at TEXT NOT NULL,
        last_car_response TEXT,
        PRIMARY KEY (map_id, from_node, to_node)
      );

      CREATE TABLE IF NOT EXISTS voice_command_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        text TEXT NOT NULL DEFAULT '',
        action TEXT NOT NULL DEFAULT '',
        source TEXT NOT NULL DEFAULT 'rules',
        request_json TEXT NOT NULL,
        response_json TEXT NOT NULL,
        remote TEXT,
        created_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS car_status_snapshots (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        ok INTEGER,
        action TEXT,
        message TEXT,
        map_id INTEGER,
        target_id INTEGER,
        current_node INTEGER,
        prev_node INTEGER,
        next_node INTEGER,
        expected_next_node INTEGER,
        state TEXT,
        nav_action TEXT,
        task_active INTEGER,
        route_len INTEGER,
        path_index INTEGER,
        pending_station INTEGER,
        scheduler_event TEXT,
        order_id INTEGER,
        status_json TEXT NOT NULL,
        created_at TEXT NOT NULL
      );
    `);
  }

  upsertOrder(order, status = 'submitted', carResponse = null) {
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO orders (
        order_id, map_id, pickup_node, dropoff_node, status,
        submitted_at, updated_at, last_car_response
      )
      VALUES (?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(order_id) DO UPDATE SET
        map_id = excluded.map_id,
        pickup_node = excluded.pickup_node,
        dropoff_node = excluded.dropoff_node,
        status = excluded.status,
        updated_at = excluded.updated_at,
        last_car_response = excluded.last_car_response
    `).run(
      Number(order.order_id),
      Number(order.map_id),
      Number(order.pickup_node),
      Number(order.dropoff_node),
      status,
      now,
      now,
      jsonText(carResponse)
    );
  }

  updateOrderResponse(orderId, status, carResponse) {
    if (!orderId) return;
    this.db.prepare(`
      UPDATE orders
      SET status = COALESCE(?, status),
          updated_at = ?,
          last_car_response = ?
      WHERE order_id = ?
    `).run(status || null, new Date().toISOString(), jsonText(carResponse), Number(orderId));
  }

  listOrders(limit = 100) {
    return this.db.prepare(`
      SELECT * FROM orders
      ORDER BY updated_at DESC
      LIMIT ?
    `).all(Number(limit));
  }

  getOrder(orderId) {
    return this.db.prepare('SELECT * FROM orders WHERE order_id = ?').get(Number(orderId));
  }

  upsertEdgeCondition(edge, carResponse = null) {
    const from = Math.min(Number(edge.from ?? edge.from_node), Number(edge.to ?? edge.to_node));
    const to = Math.max(Number(edge.from ?? edge.from_node), Number(edge.to ?? edge.to_node));
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO edge_conditions (
        map_id, from_node, to_node, manual_penalty, dynamic_penalty,
        blocked, updated_at, last_car_response
      )
      VALUES (?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(map_id, from_node, to_node) DO UPDATE SET
        manual_penalty = excluded.manual_penalty,
        dynamic_penalty = excluded.dynamic_penalty,
        blocked = excluded.blocked,
        updated_at = excluded.updated_at,
        last_car_response = excluded.last_car_response
    `).run(
      Number(edge.map_id),
      from,
      to,
      Number(edge.manual_penalty || 0),
      Number(edge.dynamic_penalty || 0),
      edge.blocked ? 1 : 0,
      now,
      jsonText(carResponse)
    );
  }

  listEdgeConditions(mapId) {
    return this.db.prepare(`
      SELECT
        map_id,
        from_node AS "from",
        to_node AS "to",
        manual_penalty,
        dynamic_penalty,
        blocked,
        updated_at,
        last_car_response
      FROM edge_conditions
      WHERE map_id = ?
      ORDER BY from_node, to_node
    `).all(Number(mapId)).map((row) => ({
      ...row,
      blocked: Boolean(row.blocked)
    }));
  }

  loadEdgeConditions() {
    return this.db.prepare(`
      SELECT
        map_id,
        from_node AS "from",
        to_node AS "to",
        manual_penalty,
        dynamic_penalty,
        blocked,
        updated_at
      FROM edge_conditions
      ORDER BY map_id, from_node, to_node
    `).all().map((row) => ({
      ...row,
      blocked: Boolean(row.blocked)
    }));
  }

  insertVoiceLog({ request, response, remote, source }) {
    this.db.prepare(`
      INSERT INTO voice_command_logs (
        text, action, source, request_json, response_json, remote, created_at
      )
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(
      String(request?.text || ''),
      String(response?.action || ''),
      String(source || response?.source || 'rules'),
      jsonText(request),
      jsonText(response),
      remote || null,
      new Date().toISOString()
    );
  }

  listVoiceLogs(limit = 100) {
    return this.db.prepare(`
      SELECT * FROM voice_command_logs
      ORDER BY id DESC
      LIMIT ?
    `).all(Number(limit));
  }

  insertCarStatus(status) {
    this.db.prepare(`
      INSERT INTO car_status_snapshots (
        ok, action, message, map_id, target_id, current_node,
        prev_node, next_node, expected_next_node, state, nav_action,
        task_active, route_len, path_index, pending_station,
        scheduler_event, order_id, status_json, created_at
      )
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      status.ok == null ? null : (status.ok ? 1 : 0),
      status.action || null,
      status.message || null,
      numberOrNull(status.map_id),
      numberOrNull(status.target_id),
      numberOrNull(status.current_node),
      numberOrNull(status.prev_node),
      numberOrNull(status.next_node),
      numberOrNull(status.expected_next_node),
      status.state || null,
      status.nav_action || null,
      status.task_active == null ? null : (status.task_active ? 1 : 0),
      numberOrNull(status.route_len),
      numberOrNull(status.path_index),
      status.pending_station == null ? null : (status.pending_station ? 1 : 0),
      status.scheduler_event || null,
      numberOrNull(status.order_id),
      jsonText(status),
      new Date().toISOString()
    );
  }

  latestCarStatus() {
    return this.db.prepare(`
      SELECT * FROM car_status_snapshots
      ORDER BY id DESC
      LIMIT 1
    `).get();
  }

  listCarStatusHistory(limit = 100) {
    return this.db.prepare(`
      SELECT * FROM car_status_snapshots
      ORDER BY id DESC
      LIMIT ?
    `).all(Number(limit));
  }

  close() {
    this.db.close();
  }
}

function numberOrNull(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

module.exports = { AppDatabase };
