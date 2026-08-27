# 电脑端后端通信网关

这个目录是部署在电脑上的本地后端，默认同时提供前端页面、REST API、WebSocket 实时推送，以及和小车通信的 UDP 网关。
订单、边权、语音命令和小车状态会持久化到 SQLite，默认路径是 `data/delivery_car.db`。

## 启动

```bash
npm install
npm start
```

浏览器打开 `http://127.0.0.1:3000`。

## 端口

- `HTTP 3000`: 电脑端前端与 REST/WebSocket 服务。
- `UDP 8898`: 后端向小车发送订单、控制、边权 JSON；小车响应会回到后端的 UDP 客户端 socket。
- `UDP 8888`: 后端监听小车 JPEG 图传，协议为 `4字节小端长度 + JPEG数据`。
- `UDP 8899`: 后端监听小车语音命令 JSON，并返回 `command_result`。
- `SQLite`: 默认 `data/delivery_car.db`，可用 `DB_PATH` 覆盖。

## REST API

```http
POST /api/orders
POST /api/control/confirm
POST /api/control/stop
POST /api/control/query
GET  /api/status
GET  /api/status/history?limit=100
GET  /api/maps
GET  /api/edges?map_id=1
POST /api/edges
POST /api/map/refresh-weights
GET  /api/orders
GET  /api/orders/:order_id
GET  /api/voice-logs?limit=100
```

订单示例：

```json
{"order_id":1,"map_id":1,"pickup_node":3,"dropoff_node":9}
```

边权示例：

```json
{"map_id":1,"from":3,"to":4,"manual_penalty":50,"dynamic_penalty":0,"blocked":false}
```

## 联调脚本

没有小车时，可以用模拟小车响应 REST：

```bash
npm run simulate:car
```

另开终端启动后端，再用页面或 REST 提交订单。

模拟图像：

```bash
npm run simulate:image
```

模拟语音命令：

```bash
npm run simulate:voice -- 确认
```

## 外部模型 API 预留

默认语音理解走本地规则。若设置：

```bash
VOICE_MODEL_API_URL=http://127.0.0.1:8000/voice-command npm start
```

后端会先把小车发来的 `voice_command` JSON POST 给外部模型，模型返回 `command_result` 后再转发给小车；模型失败时自动降级到本地规则。

## 小车状态协议

`status_response` 兼容旧字段，并支持新状态字段：

```json
{
  "type": "status_response",
  "ok": true,
  "action": "query_status",
  "message": "status ok",
  "map_id": 1,
  "target_id": 9,
  "current_node": 3,
  "prev_node": 1,
  "next_node": 4,
  "expected_next_node": 4,
  "state": "EXECUTING",
  "nav_action": "FOLLOW",
  "path": [3, 4, 9],
  "path_index": 0,
  "task_active": true,
  "route_len": 2,
  "pending_station": false,
  "pending_text_zh": "",
  "pending_text_en": "",
  "scheduler_event": "ROUTE_UPDATED",
  "order_id": 1
}
```
