# IntelligentCar Delivery

Autonomous campus delivery, from order to destination.

面向校园、楼宇和封闭园区场景的智能外卖/物流小车系统。电脑端创建订单，小车基于 THU/SUTD 拓扑地图导航，途中根据动态边权重规划路线，并通过屏幕、语音、图传和 SQLite 留下完整任务轨迹。

硬件基于 Loongson 2K300/301 小车平台，集成 AprilTag 定位、串口屏、ASRPRO 语音模块和摄像头图传。

## Project Website

- GitHub Pages: <https://cx300-fh.github.io/IntelligentCar_Delivery/>
- Static entry in repository: [`docs/index.html`](./docs/index.html)

如果 Pages 地址短时间内仍是 404，请在 GitHub 仓库 `Settings -> Pages` 确认 Source 为 `Deploy from a branch`，Branch 为 `main / docs`，并等待 GitHub Pages 完成构建。

## What It Shows

- Order lifecycle: create order, pickup prompt, route execution, dynamic reroute, delivery confirmation.
- Dynamic routing: `base_distance + dynamic_penalty + manual_penalty` with blocked-edge support.
- Two maps: THU `MAP_THU=1` with 14 nodes, SUTD `MAP_SUTD=2` with 12 nodes.
- Voice flow: ASRPRO local playback plus UDP command understanding through the PC gateway.
- Communication gateway: browser, PC backend, UDP vehicle protocol, image stream, voice model channel.
- Persistence: orders, edge conditions, voice command logs, and vehicle status snapshots in SQLite.

## Run The PC Console

```bash
cd pc_backend
npm install
npm start
```

Then open:

```text
http://127.0.0.1:3000
```

## Main Ports

| Interface | Port / Channel | Purpose |
| --- | --- | --- |
| Browser to PC backend | HTTP / WebSocket `3000` | Order console, REST API, live status |
| PC backend to vehicle | UDP `8898` | Orders, confirm, stop, edge updates, status query |
| Vehicle image stream to PC | UDP `8888` | JPEG frames, `4-byte little-endian length + JPEG data` |
| Vehicle voice gateway to PC | UDP `8899` | Voice command JSON and structured command result |
| Vehicle screen | UART5 `230400` | Station and route display |
| Vehicle voice module | UART3 `115200` | ASRPRO playback and recognition |
| Deployment transfer | TCP `22` | SSH / SCP |

## Current Status

Completed:

- Order scheduler with active order, waiting queue, station confirmation, and on-route insertion checks.
- Dynamic weighted Dijkstra for THU and SUTD maps.
- Vehicle-side UDP route server for order/control/edge messages.
- PC-side Node.js gateway with REST, WebSocket, UDP image receiver, UDP voice gateway, and SQLite persistence.
- ASRPRO local playback event hooks and external voice-model UDP protocol.
- Static GitHub Pages project website in `docs/`.

Remaining real-car work:

- Servo direction and turning-time calibration.
- Final speed parameters for the physical route.
- Real ASRPRO voice material ID verification.
- Real map API credentials and provider replacement.
- Final classroom/demo screenshots for the PC console and car display.
