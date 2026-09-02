# 前端接口 v1

前端不登录、不按用户分区。`order_id` 全局唯一；`nickname` 仅用于展示，可以重复。前端不得自行修改订单状态。

## 首次加载

1. `GET /api/locations` 获取下单地点选项。
2. `GET /api/snapshot` 获取订单、车辆与当前行程的完整快照。
3. 连接同源 WebSocket；每次收到 `type=snapshot` 时，用较大的 `snapshot_version` 替换本地状态。

## 创建订单

`POST /api/orders`，`Content-Type: application/json`：

| 字段 | 类型 | 必填 | 约束 |
|---|---|---:|---|
| `client_request_id` | string | 是 | 浏览器生成 UUID；重试必须复用，最长 128 |
| `nickname` | string | 是 | 1～50 字符，可重复 |
| `pickup_location_id` | string | 是 | 必须来自 `/api/locations` |
| `dropoff_location_id` | string | 是 | 必须与取件点在同一地图 |
| `item_summary` | string | 否 | 展示用物品摘要 |
| `item_count` | integer | 否 | 1～999，默认 1 |
| `note` | string | 否 | 备注 |

服务器生成 `order_id`、`display_no`、`status`、`dispatch_state`、`version`、`trip_id` 与全部时间字段。相同 `client_request_id` 重试不会生成第二单。

## 路线预览

用户选完地点后可调用 `POST /api/route-preview`：

```json
{
  "pickup_location_id": "1:3",
  "dropoff_location_id": "1:9"
}
```

返回 `route_nodes`、`distance`、`queued_order_count`、`estimated_wait_minutes`
与 `estimated_delivery_minutes`。这些时间只用于 Demo 展示，不作为配送承诺。

## 订单展示字段

每个订单包含：

- 标识：`order_id`、`display_no`、`nickname`。
- 状态：`status`、`status_code`、`dispatch_state`、`version`。
- 路线：`map_id`、`pickup`、`dropoff`。
- 内容：`item_summary`、`item_count`、`note`。
- 屏幕文案：`display.d_s_1`～`display.d_s_4`、`display.action`。

订单 `status` 只有 1～5。无当前订单时，快照顶层为：

```json
{"screen_phase":0,"current_order_id":null,"orders":[]}
```

已有历史完成订单但没有待处理订单时，`screen_phase` 仍为 0，但 `orders` 可以包含状态 5 的历史订单。

## 查询接口

- `GET /api/orders?limit=100&include_completed=true`
- `GET /api/orders/:order_id`
- `GET /api/vehicle`
- `GET /api/trip`
- `GET /api/status`

## 管理操作

- `POST /api/admin/emergency-stop`：只暂停运动，不改变订单状态。
- `POST /api/admin/resume`：恢复最新有效任务。

普通下单页面不应显示取消按钮，也不应提供直接改状态接口。订单的取件、送达确认由小车屏幕发送。
