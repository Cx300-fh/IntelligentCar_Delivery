# Delivery Car Protocol v1

This document is the source of truth shared by the browser backend and the car.
JSON field names and enum values are stable for protocol version `1`.

## System invariants

- There is one car. `vehicle_id` is the integer `0` in every message and table.
- `screen_phase` is `0..5`; `0` means no order is available for display.
- A real order has `status` in `1..5`; no fake order with status `0` is stored.
- Orders cannot be cancelled. Emergency stop pauses motion without changing order status.
- The server is authoritative for orders, capacity, trips, stop order and state changes.
- The server is authoritative for the road-node path. The car is authoritative for
  executing that path safely and for AprilTag arrival observations.
- Loaded capacity is the number of orders in status `3` or `4`, and must not exceed `5`.
- Each pickup/drop-off operation is confirmed separately, even at a shared physical stop.
- The currently issued next stop is frozen. Replanning may only change later stops.

## Order status

| Value | Code | Meaning |
|---:|---|---|
| 1 | `QUEUED` | Waiting, or travelling to its pickup point |
| 2 | `WAIT_PICKUP_CONFIRM` | Parked at pickup and waiting for this order to be loaded |
| 3 | `DELIVERING` | Loaded in the car and travelling to its destination |
| 4 | `WAIT_DROPOFF_CONFIRM` | Parked at destination and waiting for this order to be removed |
| 5 | `COMPLETED` | Delivery completed |

Allowed transitions are only `1 -> 2 -> 3 -> 4 -> 5`.

## Dispatch state

`UNASSIGNED`, `TO_PICKUP`, `WAIT_PICKUP`, `TO_DROPOFF`,
`WAIT_DROPOFF`, `DONE`.

## Transport

The car initiates a persistent TCP connection to the server. Messages are UTF-8 JSON,
one object per line (NDJSON). Every line ends in `\n`; an implementation must buffer
partial TCP reads and may not assume one `recv()` equals one message.

The first car message after each connection is `hello`. The car then sends a heartbeat
every 2 seconds. The server marks it offline after 6 seconds without traffic.

All messages include:

```json
{
  "protocol_version": 1,
  "type": "message_type",
  "message_id": "unique-id",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:20:31.000Z"
}
```

The receiver deduplicates `message_id`. Commands also carry monotonically increasing
`command_version`; an older command must never replace a newer command.

## Car to server

### `hello`

```json
{
  "protocol_version": 1,
  "type": "hello",
  "message_id": "car-boot-1",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:20:31.000Z",
  "software_version": "1.0.0",
  "last_command_version": 16,
  "map_id": 1,
  "current_node": 3,
  "motion_state": "STOPPED"
}
```

The car remains stopped until the server responds with current state/command data.

### `heartbeat`

Carries `sequence`, `map_id`, `current_node`, `motion_state`, `navigation_state`,
`current_action`, `trip_id`, `stop_id`, `command_version` and optional battery/fault data.
For smooth browser map rendering it may also carry `prev_node`, `next_node`,
`segment_progress` (`0..1`) and `path_index`. These fields are optional; without
them the browser displays the last confirmed `current_node`.

### `command_ack`

```json
{
  "protocol_version": 1,
  "type": "command_ack",
  "message_id": "ack-17",
  "reply_to": "cmd-17",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:20:32.000Z",
  "accepted": true,
  "command_version": 17,
  "path": [3, 4, 9, 13],
  "error": null
}
```

### `arrived`

The car stops before sending this event.

```json
{
  "protocol_version": 1,
  "type": "arrived",
  "message_id": "evt-arrive-8",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:25:00.000Z",
  "trip_id": "trip-id",
  "stop_id": "stop-id",
  "command_version": 17,
  "map_id": 1,
  "node_id": 13,
  "tag_id": 13
}
```

### `user_action`

One message confirms one order only.

```json
{
  "protocol_version": 1,
  "type": "user_action",
  "message_id": "evt-confirm-45",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:26:00.000Z",
  "trip_id": "trip-id",
  "stop_id": "stop-id",
  "order_id": "ORD-20260827-000045",
  "action": "CONFIRM_DROPOFF_TAKEN",
  "expected_status": 4,
  "order_version": 8
}
```

Valid actions are `CONFIRM_PICKUP_LOADED` and `CONFIRM_DROPOFF_TAKEN`.

## Server to car

### `state_sync`

Contains `screen_phase`, `current_order_id`, the display order list, vehicle state,
active trip, current stop and the latest command version. An empty system has
`screen_phase: 0`, `current_order_id: null`, `orders: []`, `active_trip: null`.

### `goto_stop`

```json
{
  "protocol_version": 1,
  "type": "goto_stop",
  "message_id": "cmd-17",
  "vehicle_id": 0,
  "sent_at": "2026-08-27T14:20:31.000Z",
  "trip_id": "trip-id",
  "stop_id": "stop-id",
  "command_version": 17,
  "map_id": 1,
  "target_node": 13,
  "location_id": "1:13",
  "location_name": "ZLY",
  "operations": [
    {"order_id":"ORD-20260827-000045","action":"DROPOFF"}
  ],
  "server_suggested_path": [3, 4, 9, 13],
  "estimated_duration_ms": 4820
}
```

The server chooses both the next physical stop and its road-node path. The car must
execute `server_suggested_path` in order using its existing navigation/control state
machine. Local Dijkstra may validate reachability but may not replace the server path.
The `path` echoed in `command_ack` must therefore equal `server_suggested_path`.

`estimated_duration_ms` is the server's own distance / ROBOT_SPEED_UNITS_PER_SEC estimate
for this leg — informational only (e.g. for a simulator to time its playback); the car's
real motion is still driven by its own navigation stack, not this value.

### Other server commands

- `display_sync`: refresh screen data without changing navigation.
- `hold`: remain stopped at the current stop.
- `emergency_stop`: stop motion; order states remain unchanged.
- `resume`: resume only the latest valid command.
- `event_ack`: acknowledges `arrived` or `user_action`, with accepted/error and the
  authoritative new order status/version where applicable.

## Browser API

- `POST /api/orders`
- `GET /api/orders`
- `GET /api/orders/:order_id`
- `GET /api/snapshot`
- `GET /api/locations`
- `GET /api/vehicle`
- `GET /api/trip`
- `POST /api/admin/emergency-stop`
- `POST /api/admin/resume`

Create-order request:

```json
{
  "client_request_id": "browser-generated-uuid",
  "nickname": "小明",
  "map_id": 1,
  "pickup_location_id": "1:3",
  "dropoff_location_id": "1:9",
  "item_summary": "文件袋",
  "item_count": 1,
  "note": ""
}
```

The server generates `order_id`, `display_no`, initial status `1`, version `1`, trip
assignment and route data. The browser cannot set those fields.

## Browser WebSocket

The backend sends a complete snapshot after connection and after every material change:

```json
{
  "type": "snapshot",
  "snapshot_version": 28,
  "server_time": "2026-08-27T14:20:31.000Z",
  "screen_phase": 3,
  "current_order_id": "ORD-20260827-000045",
  "vehicle": {},
  "orders": [],
  "active_trip": {},
  "route_plan": {
    "trip_id": "trip-id",
    "route_version": 3,
    "frozen_stop_id": "stop-id",
    "legs": [
      {
        "stop_id": "stop-id",
        "sequence": 1,
        "state": "ISSUED",
        "action": "PICKUP",
        "node_id": 13,
        "operations": [{"order_id":"ORD-20260827-000045","action":"PICKUP"}],
        "route_nodes": [3, 4, 9, 13],
        "distance": 312,
        "reachable": true,
        "is_current": true
      }
    ]
  },
  "navigation_progress": {
    "stop_id": "stop-id",
    "command_version": 17,
    "path_index": 1,
    "prev_node": 4,
    "next_node": 9,
    "segment_progress": 0.4,
    "motion_state": "MOVING"
  },
  "locations": []
}
```

The browser ignores snapshots older than the newest `snapshot_version` it has applied.
`route_plan.legs` is the complete server-planned remainder of the active trip. The
current leg is the exact persisted path sent to the car; later legs are server previews
and may change when a new pooled order is inserted. The production browser must never
replace these paths with a client-side shortest-path result.
