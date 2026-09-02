# 小车端改造要求（交给小车代码窗口）

本文件只定义小车端要实现的边界，不在本后端改动中修改小车工程。消息完整字段与示例以 `protocol-v1.md` 为准。

## 连接线程/任务

- 小车作为 TCP 客户端主动连接中央服务器 `TCP 8898`。
- 使用 UTF-8 NDJSON，一行一个 JSON；必须缓存半包并拆分粘包，不能假设一次 `recv()` 就是一条消息。
- 连接成功首先发送 `hello`，随后每 2 秒发送 `heartbeat`；断线自动重连，并保持车停止。
- 收发网络任务应与导航/控制循环、屏幕交互循环并行，不能在主运动控制循环内阻塞等待网络。

## 收到服务器消息

- `state_sync`：用服务器权威状态恢复订单屏幕、当前行程、当前停靠点及版本。
- `display_sync`：只刷新显示，不改变导航。
- `goto_stop`：校验 `vehicle_id=0` 和递增的 `command_version`，保存 `trip_id/stop_id/target_node`，回 `command_ack`。
- `hold`：保持停车。
- `emergency_stop`：立即安全停车，但不要自行改变订单状态。
- `resume`：只恢复最新有效命令。
- `edge_update`：更新本地边权，用于路径安全校验和故障检测；不得自行改变已下发路径。
- `event_ack`：按 `reply_to` 结束相应事件重发；若拒绝，显示错误并保持停车。

`server_suggested_path` 是当前任务的唯一权威道路节点序列。小车必须按顺序执行；现有 Dijkstra 可用于检查相邻节点和可达性，但不得换成另一条路。运动仍由现有导航状态机和底盘控制执行。

## 发给服务器

- `hello`：软件版本、地图、当前位置、上次命令版本。
- `heartbeat`：单调递增 `sequence`、当前位置、运动/导航状态、当前动作、行程/停靠点、命令版本、电量/故障。为网页平滑显示车辆，推荐额外发送 `prev_node`、`next_node`、`segment_progress`（0～1）与 `path_index`；旧版不发送时网页停在最新确认节点。
- `command_ack`：是否接受命令，`path` 必须回显已接受的 `server_suggested_path`。
- `arrived`：AprilTag 判定到站并停车后发送；必须带当前 `trip_id`、`stop_id`、导航命令版本、`map_id/node_id/tag_id`。
- `user_action`：每按一次只确认一单，带 `order_id`、动作、`expected_status` 和屏幕当前 `order_version`。

## 屏幕规则

- `screen_phase=0` 显示无订单；不得在本地创建状态 0 的假订单。
- 状态 2 的按钮发 `CONFIRM_PICKUP_LOADED`。
- 状态 4 的按钮发 `CONFIRM_DROPOFF_TAKEN`。
- 状态 1、3、5 没有可改变订单状态的按钮。
- 同一地点有多单时依次展示/确认；先处理服务器 `operations` 中的 `DROPOFF`，再处理 `PICKUP`。
- 按钮发送后先禁用；收到成功 `event_ack/state_sync` 才刷新，超时用同一个 `message_id` 重发。

## 本地持久化与安全

- 至少持久化 `last_command_version`、当前 `trip_id/stop_id`、待确认事件及其 `message_id`。
- 旧版本命令不得覆盖新版本命令；重复 `message_id` 不得重复执行运动或按钮动作。
- 网络断开、JSON 无效、状态冲突、到站被服务器拒绝时保持停车并报告故障。
- 不在小车端决定先送哪单；小车只执行服务器冻结的下一个物理停靠点。
