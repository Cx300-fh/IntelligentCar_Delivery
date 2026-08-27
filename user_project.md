---
name: user_project
description: User developing Loongson 2K300/301 smart car with line following and obstacle avoidance
type: user
---

User is working on a smart car project using Loongson 2K300/301 embedded platform. The project involves:

- **Line following**: Maze algorithm for boundary extraction
- **Obstacle avoidance**: Uses VL53L0X laser distance sensor and IMU (lsm6dsr gyroscope)
- **AprilTag navigation**: Using AprilTag for positioning and navigation
- **Color block detection**: Detecting red and yellow obstacles using RGB thresholding with OpenCV

**Code style preferences:**
- Prefer functions that update global variables instead of returning values (e.g., `detect_obstacle()` returns void and updates `obstacle_detected`, `red_block_detected`, `yellow_block_detected`)
- Use constants for threshold parameters at the top of .cpp files
- Follow existing project conventions for comments and structure

**Key files created/modified:**
- `user_app/obstacle.cpp` / `obstacle.hpp` — New obstacle detection module with color block recognition
- `user_app/observe.cpp` — Added Send_Image() support for color mask images (type 4-6)
