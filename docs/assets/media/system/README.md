# THU Delivery — System Demo Video Guide

The System section in `docs/index.html` is already wired to these video slots. Upload an MP4 with the exact filename and the corresponding **Demo** button will play it automatically.

## 1. Visual direction

Match the website hero instead of creating a separate cyberpunk style.

- 16:9, preferably 1920×1080, H.264 MP4.
- Dark charcoal / deep forest-green environment, natural highlights, restrained NVIDIA green accents only in post-production overlays.
- Avoid neon green floods, sci-fi HUDs, fake holograms, exaggerated lens flares, floating code, or invented robot hardware.
- Keep the real THU Delivery vehicle geometry, shell, screen, camera and cargo layout unchanged.
- Use shallow, controlled camera motion: slow dolly, low tracking shot, close mechanical detail, overhead campus route shot.
- For real feature clips, 5–10 seconds is enough. Show **input → response → result** in one shot whenever possible.
- Do not ask a generative video model to render readable UI text. Add labels and diagrams later in the website or in editing.

## 2. Master demo — video-model prompt

Use the real vehicle photo/video as an image-to-video or reference-video input. Text-only generation is not recommended because the model may invent the chassis or sensors.

### Recommended 28–32 s sequence

1. **0–4 s — Need**: a student on campus carries an item and initiates a delivery by voice or a simple web action. Camera stays natural and documentary-like.
2. **4–8 s — Cargo**: close-up of the item being placed into the vehicle; subtle load-cell / weight-change cue; screen or voice confirms pickup.
3. **8–17 s — Move**: low tracking shot of the real robot driving. Brief inserts show camera perception, AprilTag localization and the weight-aware controller selecting a different turning profile for a heavier payload.
4. **17–22 s — Adapt**: a visible obstacle blocks the current road; the robot stops safely; a clean green route overlay changes to an alternate campus path; the robot continues.
5. **22–26 s — Scale & Trust**: cut to a restrained dispatch visualization showing several orders becoming an ordered stop sequence, then a brief network-loss safe-stop / state-recovery moment.
6. **26–32 s — Handoff**: the robot reaches the destination, the user removes the item, and the final frame becomes a quiet product hero shot matching the website first screen.

### Prompt

> Cinematic engineering product film for a real autonomous campus delivery robot. Preserve the exact supplied robot design and proportions; do not invent hardware. Film on a modern university campus with natural architecture and realistic pedestrian scale. Visual language: dark charcoal and deep forest-green shadows, neutral daylight, restrained green technical accents, premium research-lab aesthetic, clean and minimal rather than cyberpunk. Show a complete real-world delivery story: simple voice/web ordering, cargo placed into the compartment, weight sensing, the robot driving with camera perception and AprilTag localization, a heavier payload causing the controller to use a more conservative turning profile, a road obstacle detected and the route rerouted, safe motion control, final destination handoff. Use slow dolly shots, low tracking shots, overhead route shots and precise close-ups of camera, screen, load cell area, wheels and shell. No holograms, no fictional sensors, no unreadable generated text, no excessive neon glow. 16:9, realistic physics, stable geometry, polished university engineering documentary.

## 3. Per-feature clip list

### Order
- `order-voice.mp4` — voice request → structured order.
- `order-web.mp4` — pickup/destination selection → order submission.
- `order-route-preview.mp4` — route preview before confirmation.
- `order-live-status.mp4` — live robot/order state updating.

### Cargo
- `cargo-weight.mp4` — item added/removed → measured weight changes.
- `cargo-bay.mp4` — loading and stable cargo placement.
- `cargo-display.mp4` — on-car screen guiding pickup/drop-off.
- `cargo-voice.mp4` — local voice prompt at pickup/handoff.

### Move
- `move-camera.mp4` — camera image → boundary/road perception.
- `move-apriltag.mp4` — AprilTag detection → node localization.
- `move-weight.mp4` — light/heavy payload measurement.
- `move-weight-turn.mp4` — same turn with different payloads / turning parameters.
- `move-obstacle.mp4` — visual obstacle detection.
- `move-reroute.mp4` — blocked edge → new route → continue driving.
- `move-navigation.mp4` — FOLLOW / TURN / STRAIGHT / ARRIVED state changes.
- `move-control.mp4` — steering + motor response under the 5 ms loop.
- `move-shell.mp4` — slow product walkaround showing shell, frame and protected electronics.

### Scale
- `scale-scheduler.mp4` — multiple orders → ordered stops.
- `scale-insertion.mp4` — capacity/detour-aware insertion of a new order.
- `scale-route-planner.mp4` — weighted edges / blocked route changing the path.
- `scale-gateway.mp4` — PC command sent to car → live event returned.

### Trust
- `trust-link-loss.mp4` — link loss → safe stop.
- `trust-watchdog.mp4` — stale command / watchdog → motion inhibited.
- `trust-sync.mp4` — reconnect → state sync → controlled recovery.
- `trust-emergency.mp4` — emergency stop → safe physical stop.

## 4. Shooting rule for feature videos

Do not make every clip a cinematic montage. Each clip should prove one function.

**Recommended structure:**

`0–2 s: show the real problem/input` → `2–6 s: show the module responding` → `6–9 s: show the physical/system result`.

Examples:

- Weight-adaptive turn: place two visibly different payloads, keep the same corner and camera position, then compare turning behavior or overlay the selected parameter profile in post.
- Obstacle reroute: first show the planned route, place the red obstacle in the active road, show detection/stop, then show the route changing and the car taking the alternative branch.
- Link loss: show the vehicle moving, disconnect the network in a controlled test, show the wheels stop, then reconnect and show state synchronization before movement resumes.
