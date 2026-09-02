# 单车配送调度后端

本目录是中央服务器后端。它负责订单状态、单车容量、配送批次、停靠顺序和道路节点路径；小车严格执行服务器下发的当前路径，并负责运动控制和 AprilTag 到站观测。

正式网页位于 `public/index.html`，由同学交付的地图 UI 接入 REST/WebSocket 后形成；原始交互原型保留为根目录 `UI.html`，之前的旧调试页面保留为 `public/legacy.html`。网页当前只启用 THU 的 `map_id=1`。

## 启动与测试

```bash
npm install
npm run check
npm test
npm start
```

默认网页/API 地址为 `http://127.0.0.1:3000`。若要让局域网其他设备访问，可设置 `HTTP_HOST=0.0.0.0`。公网部署时应在 HTTP 和小车 TCP 入口前配置 VPN、TLS、鉴权与防火墙。

## 端口

- HTTP 3000：REST、浏览器 WebSocket 和静态网页。
- TCP 8898：小车主动连接的 NDJSON 长连接。
- UDP 8888：旧 JPEG 图传入口。
- UDP 8899：旧语音命令入口。
- SQLite：默认 `data/delivery_car.db`。

## 核心规则

- 只有一辆车，`vehicle_id` 固定为整数 `0`。
- 空闲屏幕是 `screen_phase=0`；真实订单状态只允许 `1..5`。
- 状态只能按 `1 → 2 → 3 → 4 → 5` 变化，没有取消功能。
- 状态 3、4 计入车内载量，最多 5 单。
- 服务器按 FIFO 开始新批次，并在容量不超限、取件先于送达、增加路程不超过 15% 时插入新单。
- 当前已下发停靠点冻结；只重排后续未下发停靠点。
- 同一地点可合并停车，但每个订单仍单独确认。
- 小车离线时仍可下单，只排队不发车。

## 主要接口

```text
POST /api/orders
GET  /api/orders
GET  /api/orders/:order_id
GET  /api/snapshot
GET  /api/locations
GET  /api/vehicle
GET  /api/trip
POST /api/route-preview
POST /api/admin/emergency-stop
POST /api/admin/resume
```

下单示例：

```json
{
  "client_request_id": "browser-generated-uuid",
  "nickname": "小明",
  "pickup_location_id": "1:3",
  "dropoff_location_id": "1:9",
  "item_summary": "文件袋",
  "item_count": 1,
  "note": ""
}
```

## 模拟小车

先启动后端，再开另一个终端：

```bash
AUTO_FLOW=1 npm run simulate:car
```

`AUTO_FLOW=1` 会自动确认命令、沿 `server_suggested_path` 模拟到站和两个用户按钮，适合端到端测试。汇报演示可使用 `AUTO_FLOW_DELAY_MS=5000 AUTO_FLOW_TICK_MS=100`。

详细协议见 `docs/protocol-v1.md`，小车改造清单见 `docs/car-porting-requirements.md`。
