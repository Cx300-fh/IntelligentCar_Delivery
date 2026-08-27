# 智能外卖/物流车系统

面向校园、楼宇和封闭园区场景的智能外卖/物流小车 Demo。系统支持电脑端创建订单、小车按地图节点自主导航、起终点中英文语音/屏幕提示、动态边权避障、途中插单、实时图传、语音模型交互和 SQLite 数据持久化。

硬件平台基于 Loongson 2K300/301 核心板、小车底盘、AprilTag 定位、陶晶驰串口屏、ASRPRO 语音模块和摄像头图传。

## Demo 入口

- 交互式项目展示页：打开 [`docs/index.html`](./docs/index.html)
- 真实电脑端控制台：

```bash
cd pc_backend
npm install
npm start
```

启动后访问：`http://127.0.0.1:3000`

## 系统总览

```text
电脑浏览器
  │ HTTP/WebSocket :3000
  ▼
电脑端 Node.js 后端
  ├─ REST API：订单、边权、控制、状态查询
  ├─ WebSocket：实时状态、图像、语音日志推送
  ├─ SQLite：订单、边权、语音命令、小车状态持久化
  ├─ UDP 8888：接收小车 JPEG 图传
  ├─ UDP 8899：接收语音命令并返回模型/规则解析结果
  └─ UDP 8898：向小车发送订单、确认、停止、边权更新
        │
        ▼
Loongson 智能小车
  ├─ AprilTag 定位
  ├─ Dijkstra 动态路径规划
  ├─ 订单调度与顺路插单
  ├─ 屏幕提示取放货
  ├─ ASRPRO 本地语音播报
  └─ 电机/舵机运动控制
```

## 核心功能

| 模块 | 当前能力 |
| --- | --- |
| 订单创建 | 电脑端创建订单，字段包括 `order_id`、`map_id`、`pickup_node`、`dropoff_node` |
| 订单调度 | 支持单订单、多订单等待队列、顺路插单、不可插入订单延后 |
| 路径规划 | 基于 THU/SUTD 拓扑图，使用动态边权 Dijkstra |
| 动态边权 | `base_distance + manual_penalty + dynamic_penalty`，支持封路 |
| 起终点交互 | 起点提示“请放入物品”，终点提示“请取走物品”，预留英文提示 |
| 语音模块 | ASRPRO 本地播报 ID + UDP 外部模型命令理解 |
| 图像传输 | 小车 UDP 发送 JPEG 图像，电脑端实时展示 |
| 数据库 | SQLite 持久化订单、边权配置、语音命令日志、小车状态快照 |
| 状态协议 | 小车返回导航状态、路径、当前动作、调度事件、取放货状态 |

## 地图

系统当前内置两张拓扑地图，节点 ID 同时作为屏幕目标 ID、AprilTag ID 和 Dijkstra 节点 ID。

### THU 清华地图

- 地图 ID：`MAP_THU = 1`
- 节点范围：`1..14`
- 路口节点：`3, 4, 5, 9, 10, 12`
- 代表节点：紫荆操场、理科楼、图书馆、苏世民书院、东大操场、校医院、学生宿舍、东门、大礼堂、新清华学堂、中央主楼、A 点、照澜院、科技大楼

### SUTD 地图

- 地图 ID：`MAP_SUTD = 2`
- 节点范围：`1..12`
- 路口节点：`1, 2, 3, 4, 5, 6, 10`
- 代表节点：A、B、C、D、E、F、LIB、AUD、SSH、CC、POOL、SRC

## 订单流程

1. 电脑端创建订单：选择地图、起点、终点。
2. 后端通过 UDP `8898` 将订单 JSON 发送给小车。
3. 小车调度器生成站点序列，导航状态机前往起点。
4. 到达起点后，小车屏幕和语音提示：`请放入物品 / Please place the item`。
5. 用户通过屏幕、电脑端或语音确认。
6. 小车前往终点，途中可根据边权变化重规划。
7. 到达终点后，小车屏幕和语音提示：`请取走物品 / Please take the item`。
8. 用户确认后订单完成，状态写入电脑端 SQLite。

## 动态边权与插单

路径成本：

```text
final_cost = base_distance + dynamic_penalty + manual_penalty
```

- `base_distance`：地图内置边距离。
- `manual_penalty`：人工配置，例如台阶多、窄路、临时难走。
- `dynamic_penalty`：模拟或真实地图 API 的拥堵、事故、天气等影响。
- `blocked`：封路，Dijkstra 直接跳过该边。

插单策略：

- 默认最大绕路比例：`15%`
- 新订单若能以小绕路插入当前剩余路线，则接受并插入。
- 若新订单终点不适合当前轨迹，则先完成当前任务，新订单进入等待队列。
- 同一订单始终保证先取件、后送达。

## 通信端口

| 方向 | 端口/接口 | 用途 |
| --- | --- | --- |
| 浏览器 -> 电脑后端 | `HTTP 3000` | 前端页面、REST API、WebSocket |
| 电脑后端 -> 小车 | `UDP 8898` | 创建订单、确认、停止、边权更新、查询状态 |
| 小车 -> 电脑后端 | `UDP 8888` | JPEG 图像传输，协议为 `4字节小端长度 + JPEG数据` |
| 小车 -> 电脑后端 | `UDP 8899` | 语音命令文本，电脑端返回结构化动作 |
| 小车屏幕 | `UART5_PIN64 @ 230400` | 陶晶驰串口屏 |
| 小车语音 | `UART3_PIN46 @ 115200` | ASRPRO 播报与识别 |
| 部署传输 | `TCP 22` | SSH/SCP 上传运行 |

## 电脑端 API

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

## 小车状态协议

小车通过 `status_response` 返回状态，兼容基础字段并扩展导航信息：

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
  "scheduler_event": "ROUTE_UPDATED",
  "order_id": 1
}
```

## 数据库

电脑端默认使用 SQLite：`pc_backend/data/delivery_car.db`。

| 表 | 内容 |
| --- | --- |
| `orders` | 订单 ID、地图、起点、终点、状态、最后小车响应 |
| `edge_conditions` | 地图边、人工权重、动态权重、封路状态 |
| `voice_command_logs` | 语音文本、解析动作、规则/模型来源、请求和返回 JSON |
| `car_status_snapshots` | 小车实时状态快照，用于断线后查看最后状态 |

## 项目结构

```text
.
├── README.md                         # 项目展示首页
├── docs/                             # GitHub Pages 静态交互式项目展示页
├── pc_backend/                       # 电脑端后端、前端控制台、数据库
├── Loongson_2K300_301_LIB/
│   ├── main/                         # 小车主程序入口
│   └── user_app/                     # 导航、调度、语音、屏幕、控制等业务代码
├── Doc/                              # 接线、语音模块、通信端口等参考文档
└── MD_Image/                         # README 图片资源
```

## 图片预留位置

后续将真实截图覆盖到以下路径，README 和展示页可直接引用：

| 图片 | 路径 | 内容 |
| --- | --- | --- |
| 电脑端订单创建界面 | `MD_Image/demo/order_creation_ui.png` | 创建订单、边权、实时状态 |
| 小车起点屏幕提示 | `MD_Image/demo/car_screen_pickup.png` | “请放入物品”站点提示 |
| 地图路线展示 | `MD_Image/demo/map_route_demo.png` | THU/SUTD 路径和边权 |
| 系统架构图 | `MD_Image/demo/system_architecture.png` | 浏览器、后端、小车、语音、图传 |

## 当前完成度

- 已完成：订单调度、动态边权路径规划、顺路插单判定。
- 已完成：小车 UDP `8898` 订单/控制/边权入口。
- 已完成：电脑端 Node.js 后端、REST、WebSocket、UDP 图传、语音网关。
- 已完成：SQLite 持久化订单、边权、语音日志和小车状态。
- 已完成：ASRPRO 本地播报 ID 接口和外部模型 UDP 预留。
- 已完成：基础运动控制动作映射。
- 待实车标定：舵机方向、转弯时间、速度参数、语音素材 ID、真实地图 API。

## 快速联调

电脑端：

```bash
cd pc_backend
npm install
npm start
```

无小车模拟：

```bash
npm run simulate:car
npm run simulate:image
npm run simulate:voice -- 确认
```

小车端编译运行需使用 Loongson 交叉编译环境；电脑端可独立运行和模拟联调。
