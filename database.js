const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { DatabaseSync } = require('node:sqlite');
const {
  VEHICLE_ID,
  CAR_CAPACITY,
  ORDER_STATUS,
  DISPATCH_STATE,
  TRIP_STATE,
  STOP_STATE,
  COMMAND_STATUS
} = require('./domain');

function jsonText(value) {
  return JSON.stringify(value ?? null);
}

function parseJson(value, fallback = null) {
  if (value == null || value === '') return fallback;
  try {
    return JSON.parse(value);
  } catch (_error) {
    return fallback;
  }
}

function nowIso() {
  return new Date().toISOString();
}

class AppDatabase {
  constructor(dbPath) {
    this.dbPath = dbPath;
    fs.mkdirSync(path.dirname(dbPath), { recursive: true });
    this.db = new DatabaseSync(dbPath);
    this.db.exec('PRAGMA journal_mode = WAL');
    this.db.exec('PRAGMA foreign_keys = ON');
    this.db.exec('PRAGMA busy_timeout = 3000');
    this.initSchema();
  }

  tableColumns(tableName) {
    return this.db.prepare(`PRAGMA table_info(${tableName})`).all().map((row) => row.name);
  }

  tableExists(tableName) {
    return Boolean(this.db.prepare(
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"
    ).get(tableName));
  }

  withTransaction(callback) {
    this.db.exec('BEGIN IMMEDIATE');
    try {
      const result = callback();
      this.db.exec('COMMIT');
      return result;
    } catch (error) {
      this.db.exec('ROLLBACK');
      throw error;
    }
  }

  initSchema() {
    if (this.tableExists('orders') && !this.tableColumns('orders').includes('client_request_id')) {
      if (!this.tableExists('orders_legacy_v0')) {
        this.db.exec('ALTER TABLE orders RENAME TO orders_legacy_v0');
      } else {
        this.db.exec('DROP TABLE orders');
      }
    }

    this.db.exec(`
      CREATE TABLE IF NOT EXISTS schema_migrations (
        version INTEGER PRIMARY KEY,
        applied_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS locations (
        location_id TEXT PRIMARY KEY,
        map_id INTEGER NOT NULL,
        node_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        enabled INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
        UNIQUE (map_id, node_id)
      );

      CREATE TABLE IF NOT EXISTS vehicles (
        vehicle_id INTEGER PRIMARY KEY CHECK (vehicle_id = 0),
        online_status TEXT NOT NULL DEFAULT 'OFFLINE',
        motion_state TEXT NOT NULL DEFAULT 'STOPPED',
        navigation_state TEXT NOT NULL DEFAULT 'IDLE',
        current_action TEXT NOT NULL DEFAULT 'STOP',
        current_map_id INTEGER,
        current_node INTEGER,
        active_trip_id TEXT,
        loaded_count INTEGER NOT NULL DEFAULT 0 CHECK (loaded_count BETWEEN 0 AND 5),
        last_sequence INTEGER NOT NULL DEFAULT 0,
        last_command_version INTEGER NOT NULL DEFAULT 0,
        software_version TEXT,
        battery_percent REAL,
        last_heartbeat_at TEXT,
        last_status_json TEXT,
        updated_at TEXT NOT NULL
      );

      INSERT OR IGNORE INTO vehicles (vehicle_id, updated_at)
      VALUES (0, '${nowIso()}');

      CREATE TABLE IF NOT EXISTS trips (
        trip_id TEXT PRIMARY KEY,
        vehicle_id INTEGER NOT NULL DEFAULT 0 CHECK (vehicle_id = 0),
        map_id INTEGER NOT NULL,
        state TEXT NOT NULL,
        route_version INTEGER NOT NULL DEFAULT 1,
        command_version INTEGER NOT NULL DEFAULT 0,
        frozen_stop_id TEXT,
        created_at TEXT NOT NULL,
        started_at TEXT,
        completed_at TEXT,
        updated_at TEXT NOT NULL,
        FOREIGN KEY (vehicle_id) REFERENCES vehicles(vehicle_id)
      );

      CREATE TABLE IF NOT EXISTS order_sequence (
        sequence_no INTEGER PRIMARY KEY AUTOINCREMENT,
        created_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS orders (
        order_id TEXT PRIMARY KEY,
        sequence_no INTEGER NOT NULL UNIQUE,
        display_no TEXT NOT NULL UNIQUE,
        client_request_id TEXT NOT NULL UNIQUE,
        nickname TEXT NOT NULL,
        map_id INTEGER NOT NULL,
        pickup_location_id TEXT NOT NULL,
        pickup_node INTEGER NOT NULL,
        dropoff_location_id TEXT NOT NULL,
        dropoff_node INTEGER NOT NULL,
        item_summary TEXT NOT NULL DEFAULT '',
        item_count INTEGER NOT NULL DEFAULT 1 CHECK (item_count > 0),
        note TEXT NOT NULL DEFAULT '',
        status INTEGER NOT NULL DEFAULT 1 CHECK (status BETWEEN 1 AND 5),
        dispatch_state TEXT NOT NULL DEFAULT 'UNASSIGNED',
        trip_id TEXT,
        version INTEGER NOT NULL DEFAULT 1,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        pickup_arrived_at TEXT,
        pickup_confirmed_at TEXT,
        dropoff_arrived_at TEXT,
        completed_at TEXT,
        FOREIGN KEY (pickup_location_id) REFERENCES locations(location_id),
        FOREIGN KEY (dropoff_location_id) REFERENCES locations(location_id),
        FOREIGN KEY (trip_id) REFERENCES trips(trip_id)
      );

      CREATE INDEX IF NOT EXISTS idx_orders_status_created
      ON orders(status, created_at, sequence_no);

      CREATE INDEX IF NOT EXISTS idx_orders_trip
      ON orders(trip_id, status);

      CREATE TABLE IF NOT EXISTS trip_orders (
        trip_id TEXT NOT NULL,
        order_id TEXT NOT NULL UNIQUE,
        added_at TEXT NOT NULL,
        PRIMARY KEY (trip_id, order_id),
        FOREIGN KEY (trip_id) REFERENCES trips(trip_id),
        FOREIGN KEY (order_id) REFERENCES orders(order_id)
      );

      CREATE TABLE IF NOT EXISTS trip_stops (
        stop_id TEXT PRIMARY KEY,
        trip_id TEXT NOT NULL,
        sequence INTEGER NOT NULL,
        location_id TEXT NOT NULL,
        map_id INTEGER NOT NULL,
        node_id INTEGER NOT NULL,
        stop_type TEXT NOT NULL,
        state TEXT NOT NULL DEFAULT 'PENDING',
        route_nodes_json TEXT,
        created_at TEXT NOT NULL,
        issued_at TEXT,
        arrived_at TEXT,
        completed_at TEXT,
        UNIQUE (trip_id, sequence),
        FOREIGN KEY (trip_id) REFERENCES trips(trip_id),
        FOREIGN KEY (location_id) REFERENCES locations(location_id)
      );

      CREATE TABLE IF NOT EXISTS trip_stop_orders (
        stop_id TEXT NOT NULL,
        order_id TEXT NOT NULL,
        action TEXT NOT NULL CHECK (action IN ('PICKUP', 'DROPOFF')),
        confirmed_at TEXT,
        PRIMARY KEY (stop_id, order_id, action),
        FOREIGN KEY (stop_id) REFERENCES trip_stops(stop_id),
        FOREIGN KEY (order_id) REFERENCES orders(order_id)
      );

      CREATE TABLE IF NOT EXISTS order_events (
        event_id TEXT PRIMARY KEY,
        order_id TEXT NOT NULL,
        event_type TEXT NOT NULL,
        event_source TEXT NOT NULL,
        old_status INTEGER,
        new_status INTEGER,
        vehicle_id INTEGER,
        trip_id TEXT,
        stop_id TEXT,
        payload_json TEXT NOT NULL,
        occurred_at TEXT,
        received_at TEXT NOT NULL,
        FOREIGN KEY (order_id) REFERENCES orders(order_id)
      );

      CREATE TABLE IF NOT EXISTS vehicle_commands (
        message_id TEXT PRIMARY KEY,
        vehicle_id INTEGER NOT NULL DEFAULT 0 CHECK (vehicle_id = 0),
        trip_id TEXT,
        stop_id TEXT,
        command_type TEXT NOT NULL,
        command_version INTEGER NOT NULL,
        payload_json TEXT NOT NULL,
        status TEXT NOT NULL DEFAULT 'PENDING',
        retry_count INTEGER NOT NULL DEFAULT 0,
        created_at TEXT NOT NULL,
        sent_at TEXT,
        acknowledged_at TEXT,
        response_json TEXT
      );

      CREATE TABLE IF NOT EXISTS vehicle_messages (
        message_id TEXT PRIMARY KEY,
        message_type TEXT NOT NULL,
        payload_json TEXT NOT NULL,
        response_json TEXT NOT NULL,
        processed_at TEXT NOT NULL
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
        order_id TEXT,
        status_json TEXT NOT NULL,
        created_at TEXT NOT NULL
      );

      INSERT OR IGNORE INTO schema_migrations(version, applied_at)
      VALUES (1, '${nowIso()}');
    `);

    this.importLegacyOrders();
  }

  importLegacyOrders() {
    if (!this.tableExists('orders_legacy_v0')) return;
    const imported = this.db.prepare(
      'SELECT 1 FROM schema_migrations WHERE version = 2'
    ).get();
    if (imported) return;

    this.withTransaction(() => {
      const rows = this.db.prepare('SELECT * FROM orders_legacy_v0 ORDER BY order_id').all();
      for (const row of rows) {
        const mapId = Number(row.map_id || 1);
        const pickupNode = Number(row.pickup_node);
        const dropoffNode = Number(row.dropoff_node);
        const pickupLocation = `${mapId}:${pickupNode}`;
        const dropoffLocation = `${mapId}:${dropoffNode}`;
        const createdAt = row.submitted_at || nowIso();
        this.ensureLocation(pickupLocation, mapId, pickupNode, pickupLocation);
        this.ensureLocation(dropoffLocation, mapId, dropoffNode, dropoffLocation);
        const sequence = Number(this.db.prepare(
          'INSERT INTO order_sequence(created_at) VALUES (?)'
        ).run(createdAt).lastInsertRowid);
        const orderId = String(row.order_id);
        this.db.prepare(`
          INSERT OR IGNORE INTO orders (
            order_id, sequence_no, display_no, client_request_id, nickname,
            map_id, pickup_location_id, pickup_node, dropoff_location_id,
            dropoff_node, status, dispatch_state, created_at, updated_at
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?)
        `).run(
          orderId, sequence, String(sequence).padStart(3, '0'), `legacy-${orderId}`,
          '历史订单', mapId, pickupLocation, pickupNode, dropoffLocation, dropoffNode,
          DISPATCH_STATE.UNASSIGNED, createdAt, row.updated_at || createdAt
        );
      }
      this.db.prepare(
        'INSERT INTO schema_migrations(version, applied_at) VALUES (2, ?)'
      ).run(nowIso());
    });
  }

  syncLocations(maps) {
    this.withTransaction(() => {
      for (const map of Object.values(maps || {})) {
        for (const [nodeId, name] of Object.entries(map.nodes || {})) {
          this.ensureLocation(`${map.id}:${nodeId}`, map.id, Number(nodeId), name);
        }
      }
    });
  }

  ensureLocation(locationId, mapId, nodeId, name) {
    this.db.prepare(`
      INSERT INTO locations(location_id, map_id, node_id, name, enabled)
      VALUES (?, ?, ?, ?, 1)
      ON CONFLICT(location_id) DO UPDATE SET
        map_id=excluded.map_id, node_id=excluded.node_id, name=excluded.name
    `).run(String(locationId), Number(mapId), Number(nodeId), String(name));
  }

  listLocations() {
    return this.db.prepare(`
      SELECT location_id, map_id, node_id, name, enabled
      FROM locations WHERE enabled = 1 ORDER BY map_id, node_id
    `).all().map((row) => ({ ...row, enabled: Boolean(row.enabled) }));
  }

  getLocation(locationId) {
    return this.db.prepare(
      'SELECT * FROM locations WHERE location_id = ? AND enabled = 1'
    ).get(String(locationId));
  }

  createOrder(input) {
    return this.withTransaction(() => {
      const existing = this.db.prepare(
        'SELECT * FROM orders WHERE client_request_id = ?'
      ).get(String(input.client_request_id));
      if (existing) return { created: false, order: existing };

      const createdAt = nowIso();
      const sequence = Number(this.db.prepare(
        'INSERT INTO order_sequence(created_at) VALUES (?)'
      ).run(createdAt).lastInsertRowid);
      const datePart = createdAt.slice(0, 10).replaceAll('-', '');
      const displayNo = String(sequence).padStart(3, '0');
      const orderId = `ORD-${datePart}-${String(sequence).padStart(6, '0')}`;

      this.db.prepare(`
        INSERT INTO orders (
          order_id, sequence_no, display_no, client_request_id, nickname,
          map_id, pickup_location_id, pickup_node, dropoff_location_id,
          dropoff_node, item_summary, item_count, note, status,
          dispatch_state, version, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, 1, ?, ?)
      `).run(
        orderId, sequence, displayNo, String(input.client_request_id),
        String(input.nickname), Number(input.map_id), String(input.pickup_location_id),
        Number(input.pickup_node), String(input.dropoff_location_id),
        Number(input.dropoff_node), String(input.item_summary || ''),
        Number(input.item_count || 1), String(input.note || ''),
        DISPATCH_STATE.UNASSIGNED, createdAt, createdAt
      );
      this.insertOrderEvent({
        event_id: `server-create-${orderId}`,
        order_id: orderId,
        event_type: 'ORDER_CREATED',
        event_source: 'WEB',
        old_status: null,
        new_status: ORDER_STATUS.QUEUED,
        payload: input,
        occurred_at: createdAt
      });
      return { created: true, order: this.getOrder(orderId) };
    });
  }

  listOrders(limit = 100, includeCompleted = true) {
    const where = includeCompleted ? '' : 'WHERE status <> 5';
    return this.db.prepare(`
      SELECT * FROM orders ${where}
      ORDER BY sequence_no ASC LIMIT ?
    `).all(Math.max(1, Math.min(1000, Number(limit) || 100)));
  }

  listQueuedOrders(mapId = null) {
    if (mapId == null) {
      return this.db.prepare(`
        SELECT * FROM orders
        WHERE status = 1 AND dispatch_state = 'UNASSIGNED'
        ORDER BY created_at, sequence_no
      `).all();
    }
    return this.db.prepare(`
      SELECT * FROM orders
      WHERE status = 1 AND dispatch_state = 'UNASSIGNED' AND map_id = ?
      ORDER BY created_at, sequence_no
    `).all(Number(mapId));
  }

  getOrder(orderId) {
    return this.db.prepare('SELECT * FROM orders WHERE order_id = ?').get(String(orderId));
  }

  countLoadedOrders() {
    return Number(this.db.prepare(
      'SELECT COUNT(*) AS count FROM orders WHERE status IN (3, 4)'
    ).get().count);
  }

  insertOrderEvent(event) {
    this.db.prepare(`
      INSERT INTO order_events (
        event_id, order_id, event_type, event_source, old_status, new_status,
        vehicle_id, trip_id, stop_id, payload_json, occurred_at, received_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      String(event.event_id), String(event.order_id), String(event.event_type),
      String(event.event_source || 'SERVER'), numberOrNull(event.old_status),
      numberOrNull(event.new_status), numberOrNull(event.vehicle_id),
      event.trip_id || null, event.stop_id || null, jsonText(event.payload),
      event.occurred_at || null, nowIso()
    );
  }

  getOrderEvent(eventId) {
    return this.db.prepare('SELECT * FROM order_events WHERE event_id = ?').get(String(eventId));
  }

  transitionOrder(params) {
    return this.withTransaction(() => {
      const duplicate = this.getOrderEvent(params.event_id);
      if (duplicate) {
        return { duplicate: true, order: this.getOrder(duplicate.order_id), event: duplicate };
      }
      const order = this.getOrder(params.order_id);
      if (!order) throw codedError('ORDER_NOT_FOUND', 'order not found');
      if (Number(order.status) !== Number(params.expected_status)) {
        throw codedError('STATUS_CONFLICT', `expected status ${params.expected_status}, actual ${order.status}`);
      }
      if (params.expected_version != null && Number(order.version) !== Number(params.expected_version)) {
        throw codedError('VERSION_CONFLICT', `expected version ${params.expected_version}, actual ${order.version}`);
      }
      if (Number(params.new_status) === ORDER_STATUS.DELIVERING && this.countLoadedOrders() >= CAR_CAPACITY) {
        throw codedError('CAR_FULL', 'car loaded capacity is full');
      }

      const timestampColumn = {
        [ORDER_STATUS.WAIT_PICKUP_CONFIRM]: 'pickup_arrived_at',
        [ORDER_STATUS.DELIVERING]: 'pickup_confirmed_at',
        [ORDER_STATUS.WAIT_DROPOFF_CONFIRM]: 'dropoff_arrived_at',
        [ORDER_STATUS.COMPLETED]: 'completed_at'
      }[Number(params.new_status)];
      const now = nowIso();
      let sql = `
        UPDATE orders SET status = ?, dispatch_state = ?, version = version + 1,
          updated_at = ?`;
      if (timestampColumn) sql += `, ${timestampColumn} = ?`;
      sql += ' WHERE order_id = ?';
      const values = [Number(params.new_status), String(params.dispatch_state), now];
      if (timestampColumn) values.push(now);
      values.push(String(params.order_id));
      this.db.prepare(sql).run(...values);

      this.insertOrderEvent({
        event_id: params.event_id,
        order_id: params.order_id,
        event_type: params.event_type,
        event_source: params.event_source || 'CAR',
        old_status: order.status,
        new_status: params.new_status,
        vehicle_id: VEHICLE_ID,
        trip_id: params.trip_id || order.trip_id,
        stop_id: params.stop_id,
        payload: params.payload,
        occurred_at: params.occurred_at
      });
      this.refreshLoadedCount();
      return { duplicate: false, order: this.getOrder(params.order_id) };
    });
  }

  refreshLoadedCount() {
    const count = this.countLoadedOrders();
    this.db.prepare(`
      UPDATE vehicles SET loaded_count = ?, updated_at = ? WHERE vehicle_id = 0
    `).run(count, nowIso());
    return count;
  }

  getVehicle() {
    const row = this.db.prepare('SELECT * FROM vehicles WHERE vehicle_id = 0').get();
    return row ? { ...row, last_status: parseJson(row.last_status_json) } : null;
  }

  updateVehicle(patch) {
    const current = this.getVehicle();
    const next = { ...current, ...patch, updated_at: nowIso() };
    this.db.prepare(`
      UPDATE vehicles SET
        online_status=?, motion_state=?, navigation_state=?, current_action=?,
        current_map_id=?, current_node=?, active_trip_id=?, loaded_count=?,
        last_sequence=?, last_command_version=?, software_version=?, battery_percent=?,
        last_heartbeat_at=?, last_status_json=?, updated_at=?
      WHERE vehicle_id=0
    `).run(
      next.online_status || 'OFFLINE', next.motion_state || 'STOPPED',
      next.navigation_state || 'IDLE', next.current_action || 'STOP',
      numberOrNull(next.current_map_id), numberOrNull(next.current_node),
      next.active_trip_id || null, Number(next.loaded_count || 0),
      Number(next.last_sequence || 0), Number(next.last_command_version || 0),
      next.software_version || null, numberOrNull(next.battery_percent),
      next.last_heartbeat_at || null, jsonText(next.last_status || patch.last_status),
      next.updated_at
    );
    return this.getVehicle();
  }

  createTripForOrder(order) {
    return this.withTransaction(() => {
      const tripId = crypto.randomUUID();
      const now = nowIso();
      this.db.prepare(`
        INSERT INTO trips(trip_id, vehicle_id, map_id, state, created_at, updated_at)
        VALUES (?, 0, ?, ?, ?, ?)
      `).run(tripId, Number(order.map_id), TRIP_STATE.PLANNED, now, now);
      this.assignOrderToTrip(order.order_id, tripId);
      return this.getTrip(tripId);
    });
  }

  assignOrderToTrip(orderId, tripId) {
    const order = this.getOrder(orderId);
    if (!order) throw codedError('ORDER_NOT_FOUND', 'order not found');
    this.db.prepare(`
      INSERT INTO trip_orders(trip_id, order_id, added_at) VALUES (?, ?, ?)
    `).run(String(tripId), String(orderId), nowIso());
    this.db.prepare(`
      UPDATE orders SET trip_id=?, dispatch_state=?, updated_at=? WHERE order_id=?
    `).run(String(tripId), DISPATCH_STATE.TO_PICKUP, nowIso(), String(orderId));
  }

  getActiveTrip() {
    const row = this.db.prepare(`
      SELECT * FROM trips WHERE state <> 'COMPLETED'
      ORDER BY created_at LIMIT 1
    `).get();
    return row ? this.getTrip(row.trip_id) : null;
  }

  getTrip(tripId) {
    const row = this.db.prepare('SELECT * FROM trips WHERE trip_id=?').get(String(tripId));
    if (!row) return null;
    return {
      ...row,
      order_ids: this.db.prepare(
        'SELECT order_id FROM trip_orders WHERE trip_id=? ORDER BY added_at'
      ).all(String(tripId)).map((item) => item.order_id),
      stops: this.getTripStops(tripId)
    };
  }

  getTripStops(tripId) {
    const stops = this.db.prepare(`
      SELECT * FROM trip_stops WHERE trip_id=? ORDER BY sequence
    `).all(String(tripId));
    const operationQuery = this.db.prepare(`
      SELECT order_id, action, confirmed_at FROM trip_stop_orders
      WHERE stop_id=?
      ORDER BY CASE action WHEN 'DROPOFF' THEN 0 ELSE 1 END, order_id
    `);
    return stops.map((stop) => ({
      ...stop,
      route_nodes: parseJson(stop.route_nodes_json, []),
      operations: operationQuery.all(stop.stop_id)
    }));
  }

  getStop(stopId) {
    const row = this.db.prepare(
      'SELECT trip_id FROM trip_stops WHERE stop_id=?'
    ).get(String(stopId));
    if (!row) return null;
    return this.getTripStops(row.trip_id).find((stop) => stop.stop_id === String(stopId)) || null;
  }

  savePlannedStops(tripId, plannedStops) {
    this.withTransaction(() => {
      const existing = this.getTripStops(tripId);
      const preserved = existing.filter((stop) => stop.state !== STOP_STATE.PENDING);
      const preservedIds = new Set(preserved.map((stop) => stop.stop_id));
      const pendingIds = existing.filter((stop) => !preservedIds.has(stop.stop_id)).map((stop) => stop.stop_id);
      for (const stopId of pendingIds) {
        this.db.prepare('DELETE FROM trip_stop_orders WHERE stop_id=?').run(stopId);
        this.db.prepare('DELETE FROM trip_stops WHERE stop_id=?').run(stopId);
      }

      let sequence = preserved.reduce((max, stop) => Math.max(max, Number(stop.sequence)), 0);
      for (const stop of plannedStops) {
        if (stop.stop_id && preservedIds.has(stop.stop_id)) continue;
        sequence++;
        const stopId = stop.stop_id || crypto.randomUUID();
        this.db.prepare(`
          INSERT INTO trip_stops(
            stop_id, trip_id, sequence, location_id, map_id, node_id,
            stop_type, state, route_nodes_json, created_at
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `).run(
          stopId, String(tripId), sequence, String(stop.location_id),
          Number(stop.map_id), Number(stop.node_id), String(stop.stop_type),
          STOP_STATE.PENDING, jsonText(stop.route_nodes || []), nowIso()
        );
        for (const operation of stop.operations || []) {
          this.db.prepare(`
            INSERT INTO trip_stop_orders(stop_id, order_id, action)
            VALUES (?, ?, ?)
          `).run(stopId, String(operation.order_id), String(operation.action));
        }
      }
      this.db.prepare(`
        UPDATE trips SET route_version=route_version+1, updated_at=? WHERE trip_id=?
      `).run(nowIso(), String(tripId));
    });
    return this.getTrip(tripId);
  }

  getNextPendingStop(tripId) {
    const row = this.db.prepare(`
      SELECT stop_id FROM trip_stops
      WHERE trip_id=? AND state='PENDING' ORDER BY sequence LIMIT 1
    `).get(String(tripId));
    return row ? this.getTripStops(tripId).find((stop) => stop.stop_id === row.stop_id) : null;
  }

  issueStop(tripId, stopId, commandVersion, routeNodes) {
    const now = nowIso();
    this.withTransaction(() => {
      this.db.prepare(`
        UPDATE trip_stops SET state=?, issued_at=?, route_nodes_json=? WHERE stop_id=?
      `).run(STOP_STATE.ISSUED, now, jsonText(routeNodes || []), String(stopId));
      this.db.prepare(`
        UPDATE trips SET state=?, frozen_stop_id=?, command_version=?,
          started_at=COALESCE(started_at, ?), updated_at=? WHERE trip_id=?
      `).run(TRIP_STATE.ACTIVE, String(stopId), Number(commandVersion), now, now, String(tripId));
      this.db.prepare(`
        UPDATE vehicles SET active_trip_id=?, last_command_version=?, updated_at=? WHERE vehicle_id=0
      `).run(String(tripId), Number(commandVersion), now);
    });
  }

  markStopArrived(stopId) {
    const now = nowIso();
    const result = this.db.prepare(`
      UPDATE trip_stops SET state=?, arrived_at=COALESCE(arrived_at, ?)
      WHERE stop_id=? AND state IN ('ISSUED', 'WAITING_CONFIRM')
    `).run(STOP_STATE.WAITING_CONFIRM, now, String(stopId));
    if (Number(result.changes) === 0) {
      throw codedError('STOP_STATE_CONFLICT', 'stop is not issued or waiting for confirmation');
    }
    const stop = this.db.prepare('SELECT trip_id FROM trip_stops WHERE stop_id=?').get(String(stopId));
    if (stop) {
      this.db.prepare(`
        UPDATE trips SET state=?, updated_at=? WHERE trip_id=?
      `).run(TRIP_STATE.WAITING_CONFIRM, now, stop.trip_id);
    }
  }

  confirmStopOperation(stopId, orderId, action) {
    const operation = this.db.prepare(`
      SELECT confirmed_at FROM trip_stop_orders
      WHERE stop_id=? AND order_id=? AND action=?
    `).get(String(stopId), String(orderId), String(action));
    if (!operation) throw codedError('STOP_OPERATION_NOT_FOUND', 'order action is not part of this stop');
    if (operation.confirmed_at) return false;
    this.db.prepare(`
      UPDATE trip_stop_orders SET confirmed_at=?
      WHERE stop_id=? AND order_id=? AND action=? AND confirmed_at IS NULL
    `).run(nowIso(), String(stopId), String(orderId), String(action));
    return true;
  }

  isStopFullyConfirmed(stopId) {
    return Number(this.db.prepare(`
      SELECT COUNT(*) AS count FROM trip_stop_orders
      WHERE stop_id=? AND confirmed_at IS NULL
    `).get(String(stopId)).count) === 0;
  }

  completeStop(stopId) {
    return this.withTransaction(() => {
      const stop = this.db.prepare('SELECT * FROM trip_stops WHERE stop_id=?').get(String(stopId));
      if (!stop) throw codedError('STOP_NOT_FOUND', 'stop not found');
      const now = nowIso();
      this.db.prepare(`
        UPDATE trip_stops SET state=?, completed_at=? WHERE stop_id=?
      `).run(STOP_STATE.COMPLETED, now, String(stopId));
      this.db.prepare(`
        UPDATE trips SET state=?, frozen_stop_id=NULL, updated_at=? WHERE trip_id=?
      `).run(TRIP_STATE.ACTIVE, now, stop.trip_id);
      return this.getTrip(stop.trip_id);
    });
  }

  completeTrip(tripId) {
    const now = nowIso();
    this.withTransaction(() => {
      this.db.prepare(`
        UPDATE trips SET state=?, frozen_stop_id=NULL, completed_at=?, updated_at=? WHERE trip_id=?
      `).run(TRIP_STATE.COMPLETED, now, now, String(tripId));
      this.db.prepare(`
        UPDATE vehicles SET active_trip_id=NULL, updated_at=? WHERE vehicle_id=0
      `).run(now);
    });
  }

  setTripPaused(tripId, paused) {
    if (!tripId) return null;
    this.db.prepare(`
      UPDATE trips SET state=?, updated_at=? WHERE trip_id=? AND state <> 'COMPLETED'
    `).run(paused ? TRIP_STATE.PAUSED : TRIP_STATE.ACTIVE, nowIso(), String(tripId));
    return this.getTrip(tripId);
  }

  createCommand(command) {
    this.db.prepare(`
      INSERT OR IGNORE INTO vehicle_commands(
        message_id, vehicle_id, trip_id, stop_id, command_type,
        command_version, payload_json, status, created_at
      ) VALUES (?, 0, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      String(command.message_id), command.trip_id || null, command.stop_id || null,
      String(command.type), Number(command.command_version || 0), jsonText(command),
      COMMAND_STATUS.PENDING, nowIso()
    );
    return this.getCommand(command.message_id);
  }

  getCommand(messageId) {
    const row = this.db.prepare('SELECT * FROM vehicle_commands WHERE message_id=?').get(String(messageId));
    return row ? { ...row, payload: parseJson(row.payload_json) } : null;
  }

  listUnackedCommands() {
    return this.db.prepare(`
      SELECT * FROM vehicle_commands
      WHERE status IN ('PENDING','SENT')
      ORDER BY command_version, created_at
    `).all().map((row) => ({ ...row, payload: parseJson(row.payload_json) }));
  }

  markCommandSent(messageId) {
    this.db.prepare(`
      UPDATE vehicle_commands SET status=?, sent_at=?, retry_count=retry_count+1
      WHERE message_id=? AND status <> 'ACKED'
    `).run(COMMAND_STATUS.SENT, nowIso(), String(messageId));
  }

  acknowledgeCommand(messageId, accepted, response) {
    this.db.prepare(`
      UPDATE vehicle_commands SET status=?, acknowledged_at=?, response_json=?
      WHERE message_id=?
    `).run(
      accepted ? COMMAND_STATUS.ACKED : COMMAND_STATUS.REJECTED,
      nowIso(), jsonText(response), String(messageId)
    );
  }

  getVehicleMessage(messageId) {
    const row = this.db.prepare(
      'SELECT * FROM vehicle_messages WHERE message_id=?'
    ).get(String(messageId));
    return row ? {
      ...row,
      payload: parseJson(row.payload_json),
      response: parseJson(row.response_json)
    } : null;
  }

  recordVehicleMessage(message, response) {
    this.db.prepare(`
      INSERT OR IGNORE INTO vehicle_messages(
        message_id, message_type, payload_json, response_json, processed_at
      ) VALUES (?, ?, ?, ?, ?)
    `).run(
      String(message.message_id), String(message.type), jsonText(message),
      jsonText(response), nowIso()
    );
    return this.getVehicleMessage(message.message_id);
  }

  upsertEdgeCondition(edge, carResponse = null) {
    const from = Math.min(Number(edge.from ?? edge.from_node), Number(edge.to ?? edge.to_node));
    const to = Math.max(Number(edge.from ?? edge.from_node), Number(edge.to ?? edge.to_node));
    const now = nowIso();
    this.db.prepare(`
      INSERT INTO edge_conditions (
        map_id, from_node, to_node, manual_penalty, dynamic_penalty,
        blocked, updated_at, last_car_response
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(map_id, from_node, to_node) DO UPDATE SET
        manual_penalty=excluded.manual_penalty,
        dynamic_penalty=excluded.dynamic_penalty,
        blocked=excluded.blocked,
        updated_at=excluded.updated_at,
        last_car_response=excluded.last_car_response
    `).run(
      Number(edge.map_id), from, to, Number(edge.manual_penalty || 0),
      Number(edge.dynamic_penalty || 0), edge.blocked ? 1 : 0,
      now, jsonText(carResponse)
    );
  }

  listEdgeConditions(mapId) {
    return this.db.prepare(`
      SELECT map_id, from_node AS "from", to_node AS "to", manual_penalty,
        dynamic_penalty, blocked, updated_at, last_car_response
      FROM edge_conditions WHERE map_id=? ORDER BY from_node, to_node
    `).all(Number(mapId)).map((row) => ({ ...row, blocked: Boolean(row.blocked) }));
  }

  loadEdgeConditions() {
    return this.db.prepare(`
      SELECT map_id, from_node AS "from", to_node AS "to", manual_penalty,
        dynamic_penalty, blocked, updated_at
      FROM edge_conditions ORDER BY map_id, from_node, to_node
    `).all().map((row) => ({ ...row, blocked: Boolean(row.blocked) }));
  }

  insertVoiceLog({ request, response, remote, source }) {
    this.db.prepare(`
      INSERT INTO voice_command_logs(
        text, action, source, request_json, response_json, remote, created_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(
      String(request?.text || ''), String(response?.action || ''),
      String(source || response?.source || 'rules'), jsonText(request),
      jsonText(response), remote || null, nowIso()
    );
  }

  listVoiceLogs(limit = 100) {
    return this.db.prepare(`
      SELECT * FROM voice_command_logs ORDER BY id DESC LIMIT ?
    `).all(Number(limit));
  }

  insertCarStatus(status) {
    this.db.prepare(`
      INSERT INTO car_status_snapshots(
        ok, action, message, map_id, target_id, current_node, prev_node,
        next_node, expected_next_node, state, nav_action, task_active,
        route_len, path_index, pending_station, scheduler_event, order_id,
        status_json, created_at
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      status.ok == null ? null : (status.ok ? 1 : 0), status.action || null,
      status.message || null, numberOrNull(status.map_id), numberOrNull(status.target_id),
      numberOrNull(status.current_node), numberOrNull(status.prev_node),
      numberOrNull(status.next_node), numberOrNull(status.expected_next_node),
      status.state || status.navigation_state || null, status.nav_action || status.current_action || null,
      status.task_active == null ? null : (status.task_active ? 1 : 0),
      numberOrNull(status.route_len), numberOrNull(status.path_index),
      status.pending_station == null ? null : (status.pending_station ? 1 : 0),
      status.scheduler_event || null, status.order_id || null, jsonText(status), nowIso()
    );
  }

  latestCarStatus() {
    return this.db.prepare('SELECT * FROM car_status_snapshots ORDER BY id DESC LIMIT 1').get();
  }

  listCarStatusHistory(limit = 100) {
    return this.db.prepare(`
      SELECT * FROM car_status_snapshots ORDER BY id DESC LIMIT ?
    `).all(Number(limit));
  }

  close() {
    this.db.close();
  }
}

function codedError(code, message) {
  const error = new Error(message);
  error.code = code;
  return error;
}

function numberOrNull(value) {
  if (value == null || value === '') return null;
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

module.exports = { AppDatabase, codedError, parseJson, nowIso };
