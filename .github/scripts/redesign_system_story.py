from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

CSS_MARKER = 'System story tabs v1'
JS_MARKER = 'system story tab controller v1'

if CSS_MARKER in text or JS_MARKER in text:
    raise SystemExit('system story redesign already applied')

# Remove the older long-form storytelling sections so the home page stays compact.
patterns = [
    (r'\n    <section id="loop" class="shell">.*?\n    </section>\n\n(?=    <section id="map")', '\n'),
    (r'\n    <section class="shell">\n      <div class="section-head">\n        <h2>From order to arrival\.</h2>.*?\n    </section>\n\n(?=    <section id="voice")', '\n'),
    (r'\n    <section id="voice" class="shell">.*?\n    </section>\n\n(?=    <section id="system")', '\n'),
]
for pattern, replacement in patterns:
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f'failed to remove legacy section: {pattern[:48]}')

system_html = r'''    <section id="system" class="shell system-story">
      <div class="system-story-head">
        <p class="eyebrow">Designed for the real world</p>
        <h2>Every module answers a real delivery problem.</h2>
        <p class="system-story-lead">Start with the situation, then reveal the engineering response. Switch between six views without turning the homepage into a long technical report.</p>
      </div>

      <div class="story-tabs" role="tablist" aria-label="System design stories">
        <button class="story-tab is-active" id="storyTabOrder" role="tab" aria-selected="true" aria-controls="storyPanelOrder" data-story="order" type="button"><span>01</span>Order</button>
        <button class="story-tab" id="storyTabCargo" role="tab" aria-selected="false" aria-controls="storyPanelCargo" data-story="cargo" type="button"><span>02</span>Cargo</button>
        <button class="story-tab" id="storyTabMove" role="tab" aria-selected="false" aria-controls="storyPanelMove" data-story="move" type="button"><span>03</span>Move</button>
        <button class="story-tab" id="storyTabScale" role="tab" aria-selected="false" aria-controls="storyPanelScale" data-story="scale" type="button"><span>04</span>Scale</button>
        <button class="story-tab" id="storyTabTrust" role="tab" aria-selected="false" aria-controls="storyPanelTrust" data-story="trust" type="button"><span>05</span>Trust</button>
        <button class="story-tab" id="storyTabSystem" role="tab" aria-selected="false" aria-controls="storyPanelSystem" data-story="system" type="button"><span>06</span>System</button>
      </div>

      <div class="story-stage">
        <article class="story-panel is-active" id="storyPanelOrder" role="tabpanel" aria-labelledby="storyTabOrder" data-panel="order">
          <div class="story-problem">
            <span class="story-label">REAL SCENE / ORDER</span>
            <h3>What if ordering is harder than the delivery itself?</h3>
            <p>Campus users may be walking, carrying items, or simply unwilling to operate a complicated console. The interaction has to disappear into the task.</p>
            <div class="story-flow" aria-label="Order flow"><span>Speak or tap</span><i></i><span>Choose pickup</span><i></i><span>Preview route</span><i></i><span>Dispatch</span></div>
          </div>
          <div class="story-feature-grid cols-2">
            <article class="story-feature"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/order-voice.mp4" data-title="Voice ordering" data-copy="Show a user speaking a delivery request and the system turning it into a structured task.">Demo ↗</button></div><h4>Voice ordering</h4><p>Let users express intent naturally instead of navigating a dense control interface.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/order-web.mp4" data-title="Web ordering" data-copy="Show pickup, destination and order creation in the web interface.">Demo ↗</button></div><h4>Web order UI</h4><p>A lightweight browser flow for pickup, destination, order state and control.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/order-route-preview.mp4" data-title="Route preview" data-copy="Show the campus route preview before the user confirms the mission.">Demo ↗</button></div><h4>Route preview</h4><p>Expose the planned path and estimated delivery time before the mission starts.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/order-live-status.mp4" data-title="Live status" data-copy="Show the live map, robot position and order state updating while the car is moving.">Demo ↗</button></div><h4>Live feedback</h4><p>Real-time state keeps the user aware of pickup, navigation and handoff progress.</p></article>
          </div>
        </article>

        <article class="story-panel" id="storyPanelCargo" role="tabpanel" aria-labelledby="storyTabCargo" data-panel="cargo" hidden>
          <div class="story-problem">
            <span class="story-label">REAL SCENE / CARGO</span>
            <h3>Reaching a place does not prove the package is there.</h3>
            <p>Pickup and handoff need physical evidence and clear interaction. Cargo state, user prompts and the compartment itself all become part of the delivery loop.</p>
            <div class="story-flow" aria-label="Cargo flow"><span>Open</span><i></i><span>Load</span><i></i><span>Verify</span><i></i><span>Handoff</span></div>
          </div>
          <div class="story-feature-grid cols-2">
            <article class="story-feature"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/cargo-weight.mp4" data-title="Weight sensing" data-copy="Show the load cell responding when an item is placed into or removed from the cargo area.">Demo ↗</button></div><h4>Weight sensing</h4><p>Track cargo changes physically instead of trusting location alone.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/cargo-bay.mp4" data-title="Cargo compartment" data-copy="Show how an item is placed into the compartment and kept stable during motion.">Demo ↗</button></div><h4>Cargo bay</h4><p>A dedicated carrying space keeps loading, transport and pickup visually understandable.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/cargo-display.mp4" data-title="On-car display" data-copy="Show the display guiding a user through loading or pickup without requiring a phone.">Demo ↗</button></div><h4>On-car display</h4><p>Pickup and drop-off instructions remain visible directly on the vehicle.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/cargo-voice.mp4" data-title="Voice prompts" data-copy="Show the car speaking concise pickup and handoff prompts at the correct stage.">Demo ↗</button></div><h4>Voice prompts</h4><p>Audio cues make loading and handoff understandable even when users are not looking at the screen.</p></article>
          </div>
        </article>

        <article class="story-panel story-panel-move" id="storyPanelMove" role="tabpanel" aria-labelledby="storyTabMove" data-panel="move" hidden>
          <div class="story-problem">
            <span class="story-label">REAL SCENE / MOVE</span>
            <h3>The road changes. The payload changes. Motion has to adapt.</h3>
            <p>Real delivery is not one fixed controller on one clean route. The car has to perceive the road, localize itself, estimate cargo load, change turning behavior, avoid blocked edges and still execute safely.</p>
            <div class="story-flow story-flow-move" aria-label="Motion flow"><span>Perceive</span><i></i><span>Weigh</span><i></i><span>Localize</span><i></i><span>Re-plan</span><i></i><span>Tune</span><i></i><span>Control</span></div>
          </div>
          <div class="story-feature-grid cols-3 move-grid">
            <article class="story-feature"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-camera.mp4" data-title="Camera perception" data-copy="Show the live camera view and lane or boundary extraction while the car is driving.">Demo ↗</button></div><h4>Camera perception</h4><p>Road boundaries and scene information enter the navigation loop continuously.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-apriltag.mp4" data-title="AprilTag localization" data-copy="Show the car recognizing a campus node and updating its current position.">Demo ↗</button></div><h4>AprilTag localization</h4><p>Physical tags anchor the vehicle to real campus graph nodes.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-weight.mp4" data-title="Payload measurement" data-copy="Show different payloads on the load cell and the measured weight changing.">Demo ↗</button></div><h4>Payload measurement</h4><p>The vehicle knows when its mass condition changes instead of assuming an empty-car model.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-weight-turn.mp4" data-title="Weight-adaptive turning" data-copy="Compare the same corner under different loads and show the controller selecting different turning parameters.">Demo ↗</button></div><h4>Weight-adaptive turns</h4><p>Different payload ranges select different turning parameters so cornering remains stable.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>05</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-obstacle.mp4" data-title="Obstacle detection" data-copy="Show a red obstacle entering the camera view and being detected as a blocked road condition.">Demo ↗</button></div><h4>Obstacle detection</h4><p>Visual obstacles become road-level information rather than a purely local reaction.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>06</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-reroute.mp4" data-title="Dynamic rerouting" data-copy="Show the original path becoming blocked and the route changing from the current node.">Demo ↗</button></div><h4>Dynamic rerouting</h4><p>Blocked edges and changing route costs trigger a new path from the current position.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>07</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-navigation.mp4" data-title="Navigation FSM" data-copy="Show the car moving through FOLLOW, TURN, STRAIGHT and ARRIVED navigation states.">Demo ↗</button></div><h4>Navigation FSM</h4><p>Route decisions become explicit follow, turn, straight, U-turn and stop actions.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>08</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-control.mp4" data-title="5 ms motion control" data-copy="Show steering and motor control responding in real time while the car follows the path.">Demo ↗</button></div><h4>5 ms control loop</h4><p>Steering, speed feedback and safety gating execute below the navigation layer.</p></article>
            <article class="story-feature story-feature-shell"><div class="story-feature-top"><span>09</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/move-shell.mp4" data-title="Frame and shell" data-copy="Show the final shell, internal frame, cargo placement and protected electronics in a slow product walkaround.">Demo ↗</button></div><h4>Frame & shell</h4><p>The enclosure protects electronics, stabilizes the cargo layout and turns the prototype into a usable product.</p></article>
          </div>
        </article>

        <article class="story-panel" id="storyPanelScale" role="tabpanel" aria-labelledby="storyTabScale" data-panel="scale" hidden>
          <div class="story-problem">
            <span class="story-label">REAL SCENE / SCALE</span>
            <h3>What happens when several users need one robot?</h3>
            <p>A useful campus delivery system has to coordinate orders, capacity, stops and routes instead of treating every request as an isolated demo.</p>
            <div class="story-flow" aria-label="Scale flow"><span>Orders</span><i></i><span>Schedule</span><i></i><span>Plan</span><i></i><span>Dispatch</span></div>
          </div>
          <div class="story-feature-grid cols-2">
            <article class="story-feature"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/scale-scheduler.mp4" data-title="Dispatch scheduler" data-copy="Show multiple pending orders becoming a single ordered sequence of stops.">Demo ↗</button></div><h4>Dispatch scheduler</h4><p>Turns several user requests into one executable delivery plan.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/scale-insertion.mp4" data-title="Capacity-aware insertion" data-copy="Show a new pickup and drop-off being inserted only when capacity and detour remain feasible.">Demo ↗</button></div><h4>Capacity-aware insertion</h4><p>New pickups and drop-offs are inserted only when load and detour constraints remain feasible.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/scale-route-planner.mp4" data-title="Weighted route planning" data-copy="Show base distance, penalties and blocked edges changing the selected campus route.">Demo ↗</button></div><h4>Weighted route planning</h4><p>Base distance, manual penalties, dynamic conditions and blocked edges shape route cost.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/scale-gateway.mp4" data-title="PC-to-car gateway" data-copy="Show the backend issuing a goto_stop command and receiving live vehicle events.">Demo ↗</button></div><h4>PC ↔ car gateway</h4><p>Persistent commands and live vehicle events keep dispatch tied to physical execution.</p></article>
          </div>
        </article>

        <article class="story-panel" id="storyPanelTrust" role="tabpanel" aria-labelledby="storyTabTrust" data-panel="trust" hidden>
          <div class="story-problem">
            <span class="story-label">REAL SCENE / TRUST</span>
            <h3>Autonomy also means knowing when not to move.</h3>
            <p>Network loss, stale commands, emergency intervention and restart recovery have to end in a predictable physical state, not just an error message.</p>
            <div class="story-flow" aria-label="Trust flow"><span>Detect</span><i></i><span>Inhibit</span><i></i><span>Stop</span><i></i><span>Sync</span><i></i><span>Resume</span></div>
          </div>
          <div class="story-feature-grid cols-2">
            <article class="story-feature"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/trust-link-loss.mp4" data-title="Link-loss safe stop" data-copy="Show network disconnection causing the vehicle to stop and remain inhibited until recovery.">Demo ↗</button></div><h4>Link-loss safe stop</h4><p>A broken connection becomes a motion inhibit, not a silent control failure.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/trust-watchdog.mp4" data-title="Control watchdog" data-copy="Show stale command detection forcing a safe stopped state at the low-level controller.">Demo ↗</button></div><h4>Control watchdog</h4><p>The low-level loop refuses stale or unsafe motion even if higher layers misbehave.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/trust-sync.mp4" data-title="State synchronization" data-copy="Show the car reconnecting, synchronizing state and recovering the current delivery context.">Demo ↗</button></div><h4>State synchronization</h4><p>Reconnect and restart flows recover mission state before motion is allowed again.</p></article>
            <article class="story-feature"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/system/trust-emergency.mp4" data-title="Emergency stop" data-copy="Show an emergency stop immediately removing motion permission and bringing PWM to a safe state.">Demo ↗</button></div><h4>Emergency stop</h4><p>Safety commands bypass the normal user journey and lead directly to a controlled stop.</p></article>
          </div>
        </article>

        <article class="story-panel story-panel-system" id="storyPanelSystem" role="tabpanel" aria-labelledby="storyTabSystem" data-panel="system" hidden>
          <div class="system-final-copy">
            <span class="story-label">EVERYTHING TOGETHER</span>
            <h3>One closed loop from intent to physical delivery.</h3>
            <p>The final architecture only appears after the problems are understood: interaction creates the task, the PC coordinates it, and the vehicle turns commands into perception-aware motion and verified handoff.</p>
            <button class="system-master-demo feature-video-trigger" type="button" data-video="assets/media/campus-demo.mp4" data-title="End-to-end delivery" data-copy="The current integrated campus demonstration from order to physical delivery."><span>▶</span> Watch end-to-end demo</button>
          </div>
          <div class="system-architecture-v2" aria-label="Integrated system architecture">
            <article><span class="system-arch-kicker">HUMAN & INTERACTION</span><h4>Order</h4><p>Voice · Web UI · Route preview · On-car display</p></article>
            <div class="system-arch-link"><span>REST / WebSocket</span></div>
            <article><span class="system-arch-kicker">PC DISPATCH</span><h4>Coordinate</h4><p>Scheduler · Route Planner · Map Weights · Database · Gateway</p></article>
            <div class="system-arch-link"><span>TCP / Events</span></div>
            <article><span class="system-arch-kicker">DELIVERY CAR</span><h4>Execute</h4><p>Weight · Camera · AprilTag · Obstacle · Navigation · 5 ms Control · Shell</p></article>
          </div>
          <div class="system-feedback-line"><span>heartbeat · arrived · user action · state sync · image</span></div>
        </article>
      </div>

      <div class="feature-video-modal" id="featureVideoModal" aria-hidden="true" role="dialog" aria-modal="true" aria-labelledby="featureVideoTitle">
        <button class="feature-video-backdrop" type="button" aria-label="Close video"></button>
        <div class="feature-video-dialog">
          <div class="feature-video-head">
            <div><span class="story-label">FUNCTION DEMO</span><h3 id="featureVideoTitle">Demo</h3><p id="featureVideoCopy"></p></div>
            <button class="feature-video-close" type="button" aria-label="Close video">×</button>
          </div>
          <div class="feature-video-stage">
            <video id="featureVideo" controls playsinline preload="metadata"></video>
            <div class="feature-video-fallback" id="featureVideoFallback" hidden>
              <span>VIDEO SLOT READY</span>
              <strong>Upload the real clip to:</strong>
              <code id="featureVideoPath"></code>
              <p>The player will use it automatically after the file is committed.</p>
            </div>
          </div>
        </div>
      </div>
    </section>
'''

system_pattern = re.compile(r'    <section id="system" class="shell">.*?    </section>\n\n(?=    <section id="contact")', re.S)
text, count = system_pattern.subn(system_html + '\n', text, count=1)
if count != 1:
    raise SystemExit('failed to replace system section')

system_css = r'''

    /* ============================================================
       System story tabs v1
       Same dark / glass / restrained NVIDIA-green language as hero.
       ============================================================ */
    .system-story {
      position: relative;
      isolation: isolate;
      padding-top: 104px;
      padding-bottom: 104px;
    }

    .system-story::before {
      content: "";
      position: absolute;
      z-index: -1;
      left: 50%;
      top: 18%;
      width: min(1120px, 96vw);
      height: 620px;
      transform: translateX(-50%);
      background: radial-gradient(ellipse at center, rgba(118,185,0,.075), rgba(24,70,51,.025) 48%, transparent 74%);
      filter: blur(20px);
      pointer-events: none;
    }

    .system-story-head {
      display: grid;
      grid-template-columns: minmax(0, 760px) minmax(260px, 1fr);
      align-items: end;
      gap: 48px;
      margin-bottom: 34px;
    }

    .system-story-head .eyebrow {
      grid-column: 1 / -1;
      margin-bottom: -24px;
    }

    .system-story-head h2 {
      margin: 0;
      max-width: 760px;
      font-size: clamp(42px, 5.4vw, 72px);
      line-height: .94;
      letter-spacing: -.012em;
    }

    .system-story-lead {
      margin: 0 0 4px;
      color: rgba(197,207,200,.58);
      font-size: 14px;
      line-height: 1.72;
    }

    .story-tabs {
      display: grid;
      grid-template-columns: repeat(6, minmax(0, 1fr));
      gap: 0;
      margin-bottom: 12px;
      border: 1px solid rgba(241,243,240,.10);
      border-radius: 12px;
      background: rgba(15,20,17,.54);
      overflow: hidden;
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
    }

    .story-tab {
      position: relative;
      min-height: 58px;
      padding: 0 15px;
      border: 0;
      border-right: 1px solid rgba(241,243,240,.075);
      background: transparent;
      color: rgba(202,212,205,.54);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 14px;
      letter-spacing: .035em;
      transition: background .24s ease, color .24s ease;
    }

    .story-tab:last-child { border-right: 0; }
    .story-tab span {
      margin-right: 8px;
      color: rgba(118,185,0,.48);
      font-size: 9px;
      vertical-align: 2px;
    }

    .story-tab::after {
      content: "";
      position: absolute;
      left: 18px;
      right: 18px;
      bottom: 0;
      height: 2px;
      background: var(--green);
      box-shadow: 0 0 12px rgba(118,185,0,.22);
      transform: scaleX(0);
      transform-origin: center;
      transition: transform .24s ease;
    }

    .story-tab:hover { color: rgba(239,244,240,.80); background: rgba(255,255,255,.018); }
    .story-tab.is-active { color: rgba(245,248,246,.96); background: rgba(118,185,0,.045); }
    .story-tab.is-active::after { transform: scaleX(1); }

    .story-stage {
      position: relative;
      min-height: 548px;
      border: 1px solid rgba(241,243,240,.105);
      border-radius: 18px;
      background:
        linear-gradient(145deg, rgba(21,31,26,.74), rgba(8,13,10,.70));
      box-shadow: inset 0 1px 0 rgba(255,255,255,.027), 0 26px 70px rgba(0,0,0,.18);
      overflow: hidden;
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
    }

    .story-stage::before {
      content: "";
      position: absolute;
      inset: 0;
      pointer-events: none;
      background:
        radial-gradient(480px circle at 4% 0%, rgba(118,185,0,.065), transparent 66%),
        linear-gradient(115deg, rgba(255,255,255,.025), transparent 28%);
    }

    .story-panel {
      position: relative;
      z-index: 1;
      display: grid;
      grid-template-columns: minmax(290px, .75fr) minmax(0, 1.55fr);
      gap: 34px;
      min-height: 548px;
      padding: 30px;
      animation: systemPanelIn .32s cubic-bezier(.2,.75,.2,1) both;
    }

    .story-panel[hidden] { display: none !important; }

    @keyframes systemPanelIn {
      from { opacity: 0; transform: translateY(5px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .story-problem {
      position: relative;
      display: flex;
      min-height: 100%;
      flex-direction: column;
      justify-content: space-between;
      padding: 26px;
      border: 1px solid rgba(241,243,240,.085);
      border-radius: 14px;
      background:
        linear-gradient(180deg, rgba(8,14,11,.40), rgba(9,16,12,.58)),
        linear-gradient(135deg, rgba(118,185,0,.035), transparent 48%);
      overflow: hidden;
    }

    .story-problem::after {
      content: "";
      position: absolute;
      width: 260px;
      height: 260px;
      right: -130px;
      bottom: -150px;
      border: 1px solid rgba(118,185,0,.08);
      border-radius: 50%;
      box-shadow: 0 0 0 34px rgba(118,185,0,.012), 0 0 0 72px rgba(118,185,0,.008);
    }

    .story-label {
      display: block;
      margin-bottom: 18px;
      color: rgba(142,203,48,.78);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 9px;
      letter-spacing: .13em;
      text-transform: uppercase;
    }

    .story-problem h3,
    .system-final-copy h3 {
      margin: 0 0 18px;
      font-size: clamp(30px, 3.2vw, 44px);
      line-height: .98;
    }

    .story-problem > p,
    .system-final-copy > p {
      color: rgba(200,211,203,.61);
      font-size: 13.5px;
      line-height: 1.75;
    }

    .story-flow {
      position: relative;
      z-index: 2;
      display: flex;
      align-items: center;
      gap: 8px;
      margin-top: 24px;
      color: rgba(228,236,230,.66);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 10px;
      white-space: nowrap;
      overflow-x: auto;
      scrollbar-width: none;
    }
    .story-flow::-webkit-scrollbar { display: none; }
    .story-flow i { width: 16px; height: 1px; flex: 0 0 16px; background: rgba(118,185,0,.30); }
    .story-flow-move { gap: 6px; font-size: 9px; }
    .story-flow-move i { width: 10px; flex-basis: 10px; }

    .story-feature-grid {
      display: grid;
      align-content: start;
      gap: 10px;
    }
    .story-feature-grid.cols-2 { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .story-feature-grid.cols-3 { grid-template-columns: repeat(3, minmax(0, 1fr)); }

    .story-feature {
      position: relative;
      min-height: 190px;
      padding: 17px 17px 18px;
      border: 1px solid rgba(241,243,240,.09);
      border-radius: 12px;
      background: rgba(221,233,225,.026);
      transition: transform .24s cubic-bezier(.2,.75,.2,1), border-color .24s ease, background .24s ease;
      overflow: hidden;
    }
    .move-grid .story-feature { min-height: 143px; }

    .story-feature::after {
      content: "";
      position: absolute;
      inset: auto -40px -80px auto;
      width: 150px;
      height: 150px;
      border-radius: 50%;
      background: radial-gradient(circle, rgba(118,185,0,.045), transparent 68%);
      pointer-events: none;
    }

    .story-feature:hover {
      transform: translateY(-3px);
      border-color: rgba(155,207,87,.22);
      background: rgba(221,233,225,.036);
    }

    .story-feature-top {
      position: relative;
      z-index: 2;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      margin-bottom: 25px;
    }
    .move-grid .story-feature-top { margin-bottom: 15px; }

    .story-feature-top > span {
      color: rgba(118,185,0,.56);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 9px;
      letter-spacing: .12em;
    }

    .feature-video-trigger {
      position: relative;
      z-index: 3;
      border: 1px solid rgba(241,243,240,.105);
      border-radius: 999px;
      background: rgba(255,255,255,.025);
      color: rgba(226,234,228,.58);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 9px;
      letter-spacing: .07em;
      transition: border-color .2s ease, background .2s ease, color .2s ease;
    }
    .story-feature .feature-video-trigger { padding: 6px 9px; }
    .feature-video-trigger:hover { border-color: rgba(118,185,0,.32); background: rgba(118,185,0,.06); color: rgba(239,247,232,.92); }

    .story-feature h4,
    .system-architecture-v2 h4 {
      position: relative;
      z-index: 2;
      margin: 0 0 8px;
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 20px;
      font-weight: 400;
      line-height: 1.02;
    }
    .move-grid .story-feature h4 { font-size: 17px; }

    .story-feature p {
      position: relative;
      z-index: 2;
      margin: 0;
      color: rgba(194,205,197,.57);
      font-size: 11.5px;
      line-height: 1.58;
    }

    .story-feature-shell {
      border-color: rgba(118,185,0,.15);
      background:
        linear-gradient(135deg, rgba(118,185,0,.045), rgba(221,233,225,.024));
    }

    .story-panel-system {
      grid-template-columns: minmax(260px, .62fr) minmax(0, 1.7fr);
      align-items: center;
    }

    .system-final-copy { padding: 10px 6px; }

    .system-master-demo {
      display: inline-flex;
      align-items: center;
      gap: 9px;
      min-height: 42px;
      margin-top: 10px;
      padding: 0 14px;
      border-radius: 7px;
      color: rgba(244,248,245,.91);
      background: rgba(118,185,0,.08);
      border-color: rgba(118,185,0,.28);
    }
    .system-master-demo span { color: var(--green); }

    .system-architecture-v2 {
      display: grid;
      grid-template-columns: 1fr 92px 1fr 92px 1fr;
      align-items: stretch;
      gap: 0;
    }

    .system-architecture-v2 article {
      min-height: 250px;
      padding: 24px;
      border: 1px solid rgba(241,243,240,.09);
      border-radius: 14px;
      background: rgba(220,232,224,.025);
    }

    .system-architecture-v2 article:nth-of-type(2) {
      border-color: rgba(118,185,0,.16);
      background: rgba(118,185,0,.035);
    }

    .system-arch-kicker {
      display: block;
      min-height: 28px;
      margin-bottom: 55px;
      color: rgba(129,193,34,.68);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 8px;
      line-height: 1.35;
      letter-spacing: .12em;
    }

    .system-architecture-v2 h4 { font-size: 24px; }
    .system-architecture-v2 p { margin: 0; color: rgba(194,205,197,.58); font-size: 11.5px; line-height: 1.65; }

    .system-arch-link { display: grid; place-items: center; position: relative; }
    .system-arch-link::before { content: ""; width: 100%; height: 1px; background: rgba(118,185,0,.25); }
    .system-arch-link::after { content: ""; position: absolute; right: -1px; width: 5px; height: 5px; border-top: 1px solid rgba(118,185,0,.55); border-right: 1px solid rgba(118,185,0,.55); transform: rotate(45deg); }
    .system-arch-link span { position: absolute; padding: 4px 6px; background: #101612; color: rgba(189,207,193,.48); font-family: "Prism", "Arial Narrow", sans-serif; font-size: 7px; letter-spacing: .05em; }

    .system-feedback-line {
      position: absolute;
      left: 35%;
      right: 30px;
      bottom: 24px;
      text-align: center;
      pointer-events: none;
    }
    .system-feedback-line::before { content: ""; display: block; height: 1px; margin-bottom: 6px; background: linear-gradient(90deg, transparent, rgba(118,185,0,.18), transparent); }
    .system-feedback-line span { color: rgba(174,192,179,.38); font-family: "Prism", "Arial Narrow", sans-serif; font-size: 7px; letter-spacing: .08em; }

    .feature-video-modal {
      position: fixed;
      inset: 0;
      z-index: 1500;
      display: grid;
      place-items: center;
      padding: 24px;
      opacity: 0;
      visibility: hidden;
      transition: opacity .18s ease, visibility .18s ease;
    }
    .feature-video-modal.is-open { opacity: 1; visibility: visible; }
    .feature-video-backdrop { position: absolute; inset: 0; border: 0; background: rgba(3,7,5,.78); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); }
    .feature-video-dialog { position: relative; width: min(980px, 100%); max-height: min(86vh, 760px); overflow: auto; border: 1px solid rgba(241,243,240,.13); border-radius: 16px; background: linear-gradient(145deg, rgba(19,29,24,.98), rgba(7,12,9,.985)); box-shadow: 0 34px 100px rgba(0,0,0,.52), inset 0 1px 0 rgba(255,255,255,.035); }
    .feature-video-head { display: flex; align-items: flex-start; justify-content: space-between; gap: 28px; padding: 22px 22px 18px; border-bottom: 1px solid rgba(241,243,240,.08); }
    .feature-video-head .story-label { margin-bottom: 8px; }
    .feature-video-head h3 { margin: 0 0 6px; font-size: 28px; }
    .feature-video-head p { margin: 0; color: rgba(194,205,197,.58); font-size: 12px; line-height: 1.55; }
    .feature-video-close { width: 36px; height: 36px; flex: 0 0 36px; border: 1px solid rgba(241,243,240,.10); border-radius: 50%; background: rgba(255,255,255,.025); color: rgba(241,243,240,.70); font-size: 23px; line-height: 1; }
    .feature-video-stage { position: relative; min-height: 360px; background: #050806; }
    .feature-video-stage video { display: block; width: 100%; max-height: 66vh; aspect-ratio: 16 / 9; object-fit: contain; background: #050806; }
    .feature-video-fallback { min-height: 420px; padding: 52px 24px; place-content: center; text-align: center; }
    .feature-video-fallback:not([hidden]) { display: grid; }
    .feature-video-fallback > span { margin-bottom: 14px; color: rgba(118,185,0,.68); font-family: "Prism", "Arial Narrow", sans-serif; font-size: 9px; letter-spacing: .12em; }
    .feature-video-fallback strong { margin-bottom: 12px; font-family: "Prism", "Arial Narrow", sans-serif; font-size: 24px; font-weight: 400; }
    .feature-video-fallback code { display: inline-block; width: fit-content; max-width: 100%; margin: 0 auto 12px; padding: 8px 11px; border: 1px solid rgba(118,185,0,.18); border-radius: 6px; background: rgba(118,185,0,.045); overflow-wrap: anywhere; }
    .feature-video-fallback p { color: rgba(194,205,197,.48); font-size: 12px; }

    body.feature-video-open { overflow: hidden; }

    @media (max-width: 1080px) {
      .system-story-head { grid-template-columns: 1fr; gap: 18px; }
      .system-story-head .eyebrow { margin-bottom: 0; }
      .story-panel, .story-panel-system { grid-template-columns: 1fr; }
      .story-problem { min-height: 280px; }
      .system-feedback-line { display: none; }
    }

    @media (max-width: 820px) {
      .story-tabs { display: flex; overflow-x: auto; scrollbar-width: none; }
      .story-tabs::-webkit-scrollbar { display: none; }
      .story-tab { flex: 0 0 118px; }
      .story-feature-grid.cols-3 { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .system-architecture-v2 { grid-template-columns: 1fr; gap: 8px; }
      .system-arch-link { min-height: 34px; }
      .system-arch-link::before { width: 1px; height: 100%; }
      .system-arch-link::after { right: auto; bottom: -1px; transform: rotate(135deg); }
      .system-arch-kicker { margin-bottom: 28px; }
      .system-architecture-v2 article { min-height: 180px; }
    }

    @media (max-width: 620px) {
      .system-story { padding-top: 74px; padding-bottom: 74px; }
      .system-story-head h2 { font-size: clamp(38px, 12vw, 54px); }
      .story-stage { border-radius: 14px; }
      .story-panel { padding: 12px; gap: 12px; }
      .story-problem { min-height: 320px; padding: 20px; }
      .story-feature-grid.cols-2, .story-feature-grid.cols-3 { grid-template-columns: 1fr; }
      .story-feature, .move-grid .story-feature { min-height: 132px; }
      .story-feature-top { margin-bottom: 14px; }
      .feature-video-modal { padding: 10px; }
      .feature-video-stage { min-height: 240px; }
      .feature-video-fallback { min-height: 300px; }
    }

    @media (prefers-reduced-motion: reduce) {
      .story-panel { animation: none; }
      .story-feature, .story-tab { transition: none; }
    }
'''

style_close = '\n  </style>'
if style_close not in text:
    raise SystemExit('style close not found')
text = text.replace(style_close, system_css + style_close, 1)

system_js = r'''

    // system story tab controller v1
    (() => {
      const tabs = [...document.querySelectorAll('.story-tab')];
      const panels = [...document.querySelectorAll('.story-panel')];
      if (!tabs.length || !panels.length) return;

      const activate = (tab) => {
        const key = tab.dataset.story;
        tabs.forEach((item) => {
          const active = item === tab;
          item.classList.toggle('is-active', active);
          item.setAttribute('aria-selected', active ? 'true' : 'false');
          item.tabIndex = active ? 0 : -1;
        });
        panels.forEach((panel) => {
          const active = panel.dataset.panel === key;
          panel.hidden = !active;
          panel.classList.toggle('is-active', active);
        });
      };

      tabs.forEach((tab, index) => {
        tab.tabIndex = tab.classList.contains('is-active') ? 0 : -1;
        tab.addEventListener('click', () => activate(tab));
        tab.addEventListener('keydown', (event) => {
          if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
          event.preventDefault();
          let next = index;
          if (event.key === 'ArrowLeft') next = (index - 1 + tabs.length) % tabs.length;
          if (event.key === 'ArrowRight') next = (index + 1) % tabs.length;
          if (event.key === 'Home') next = 0;
          if (event.key === 'End') next = tabs.length - 1;
          tabs[next].focus();
          activate(tabs[next]);
        });
      });

      const modal = document.getElementById('featureVideoModal');
      const video = document.getElementById('featureVideo');
      const title = document.getElementById('featureVideoTitle');
      const copy = document.getElementById('featureVideoCopy');
      const fallback = document.getElementById('featureVideoFallback');
      const pathText = document.getElementById('featureVideoPath');
      const closeButtons = modal ? [...modal.querySelectorAll('.feature-video-close, .feature-video-backdrop')] : [];
      let lastTrigger = null;

      const closeModal = () => {
        if (!modal) return;
        modal.classList.remove('is-open');
        modal.setAttribute('aria-hidden', 'true');
        document.body.classList.remove('feature-video-open');
        if (video) {
          video.pause();
          video.removeAttribute('src');
          video.load();
          video.hidden = false;
        }
        if (fallback) fallback.hidden = true;
        if (lastTrigger) lastTrigger.focus({ preventScroll: true });
      };

      document.querySelectorAll('.feature-video-trigger').forEach((trigger) => {
        trigger.addEventListener('click', () => {
          if (!modal || !video) return;
          lastTrigger = trigger;
          const src = trigger.dataset.video || '';
          title.textContent = trigger.dataset.title || 'Function demo';
          copy.textContent = trigger.dataset.copy || '';
          pathText.textContent = src;
          fallback.hidden = true;
          video.hidden = false;
          video.pause();
          video.src = src;
          video.load();
          modal.classList.add('is-open');
          modal.setAttribute('aria-hidden', 'false');
          document.body.classList.add('feature-video-open');
          setTimeout(() => modal.querySelector('.feature-video-close')?.focus(), 0);
          video.play().catch(() => {});
        });
      });

      video?.addEventListener('error', () => {
        video.hidden = true;
        fallback.hidden = false;
      });
      video?.addEventListener('loadeddata', () => {
        video.hidden = false;
        fallback.hidden = true;
      });
      closeButtons.forEach((button) => button.addEventListener('click', closeModal));
      document.addEventListener('keydown', (event) => {
        if (event.key === 'Escape' && modal?.classList.contains('is-open')) closeModal();
      });
    })();
'''

script_close_index = text.rfind('\n  </script>')
if script_close_index < 0:
    raise SystemExit('script close not found')
text = text[:script_close_index] + system_js + text[script_close_index:]

# Refresh page description to match the expanded physical system.
text = text.replace(
    'Autonomous campus delivery system with dynamic routing, voice interaction, UDP gateway, and persistent mission tracing.',
    'Autonomous campus delivery system with voice interaction, weight-aware motion, obstacle rerouting, dispatch scheduling, cargo verification, and safety control.'
)

path.write_text(text, encoding='utf-8')
print('system story redesign applied')
