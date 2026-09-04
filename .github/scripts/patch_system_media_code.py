from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')


def replace_grid(panel_id, boundary_token, new_grid):
    global text
    panel_start = text.index(f'id="{panel_id}"')
    grid_start = text.index('<div class="story-feature-grid', panel_start)
    boundary = text.index(boundary_token, grid_start)
    grid_end = text.rfind('</div>', grid_start, boundary)
    if grid_end < 0:
        raise RuntimeError(f'Could not find grid end for {panel_id}')
    text = text[:grid_start] + new_grid.strip() + text[grid_end + len('</div>'):]


replace_grid('storyPanelOrder', 'id="storyPanelCargo"', r'''
<div class="story-feature-grid cols-3">
  <article class="story-feature is-video"><div class="story-feature-top"><span>01</span><button class="feature-video-trigger" type="button" data-video="assets/media/web_order_ui.mp4" data-title="Web Order UI" data-copy="Real browser interaction covering pickup, destination, route preview, order creation and live task state." aria-label="Play Web Order UI video"></button></div><h4>Web Order UI</h4><p>One real interface carries pickup, destination, route preview and live delivery feedback.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>02</span><button class="feature-code-trigger" type="button" data-code-key="voice" aria-label="Open Voice interaction source code"></button></div><h4>Voice interaction</h4><p>Natural-language requests enter the same task pipeline through the PC voice gateway.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>03</span><button class="feature-code-trigger" type="button" data-code-key="orderLifecycle" aria-label="Open Order lifecycle source code"></button></div><h4>Order lifecycle</h4><p>Validated requests become persistent pickup/drop-off stops before dispatch begins.</p></article>
</div>
''')

replace_grid('storyPanelCargo', 'id="storyPanelMove"', r'''
<div class="story-feature-grid cols-2">
  <article class="story-feature is-static"><div class="story-feature-top"><span>01</span></div><h4>Payload verification</h4><p>Load sensing and the cargo bay provide physical evidence that an item was loaded or removed.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>02</span><button class="feature-code-trigger" type="button" data-code-key="handoff" aria-label="Open On-car handoff source code"></button></div><h4>On-car handoff</h4><p>Screen events, user confirmation and arrival handling close the pickup/drop-off loop on the vehicle.</p></article>
</div>
''')

replace_grid('storyPanelMove', 'id="moveRouteLab"', r'''
<div class="story-feature-grid cols-3 move-grid">
  <article class="story-feature is-code"><div class="story-feature-top"><span>01</span><button class="feature-code-trigger" type="button" data-code-key="perception" aria-label="Open Perception and localization source code"></button></div><h4>Perception & localization</h4><p>Camera processing and AprilTag recognition anchor the car to physical campus nodes.</p></article>
  <article class="story-feature is-static"><div class="story-feature-top"><span>02</span></div><h4>Payload-aware motion</h4><p>Measured payload ranges select different turning parameters so the same corner remains stable under different loads.</p></article>
  <article class="story-feature is-video"><div class="story-feature-top"><span>03</span><button class="feature-video-trigger" type="button" data-video="assets/media/turnaround.mov" data-title="Turn Around" data-copy="Real vehicle demonstration of the dedicated U-turn maneuver." aria-label="Play Turn Around video"></button></div><h4>Turn Around</h4><p>A dedicated multi-stage U-turn behavior handles routes that require the vehicle to reverse its travel direction.</p></article>
  <article class="story-feature is-map"><div class="story-feature-top"><span>04</span><button class="feature-route-trigger" type="button" data-target="moveRouteLab" aria-label="Open interactive obstacle-aware rerouting demo"></button></div><h4>Obstacle-aware rerouting</h4><p>Blocked roads and changing costs trigger a new route from the car's current position.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>05</span><button class="feature-code-trigger" type="button" data-code-key="motionControl" aria-label="Open Navigation and control source code"></button></div><h4>Navigation & 5 ms control</h4><p>Discrete navigation actions are executed below by a real-time steering, motor and safety loop.</p></article>
  <article class="story-feature is-static story-feature-shell"><div class="story-feature-top"><span>06</span></div><h4>Frame & shell</h4><p>The enclosure protects electronics, stabilizes the cargo layout and turns the prototype into a usable product.</p></article>
</div>
''')

replace_grid('storyPanelScale', 'id="storyPanelTrust"', r'''
<div class="story-feature-grid cols-3">
  <article class="story-feature is-code"><div class="story-feature-top"><span>01</span><button class="feature-code-trigger" type="button" data-code-key="scheduler" aria-label="Open Dispatch scheduler source code"></button></div><h4>Dispatch scheduler</h4><p>Multiple orders are merged into one feasible stop sequence with capacity and detour checks.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>02</span><button class="feature-code-trigger" type="button" data-code-key="routePlanner" aria-label="Open Weighted route planner source code"></button></div><h4>Weighted route planner</h4><p>Distance, penalties and blocked edges determine the route that the PC recommends.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>03</span><button class="feature-code-trigger" type="button" data-code-key="gateway" aria-label="Open PC to car gateway source code"></button></div><h4>PC ↔ car gateway</h4><p>NDJSON commands, heartbeats and vehicle events tie dispatch to physical execution.</p></article>
</div>
''')

replace_grid('storyPanelTrust', 'id="storyPanelSystem"', r'''
<div class="story-feature-grid cols-2">
  <article class="story-feature is-code"><div class="story-feature-top"><span>01</span><button class="feature-code-trigger" type="button" data-code-key="safety" aria-label="Open Fail-safe control source code"></button></div><h4>Fail-safe control</h4><p>Safety inhibit, watchdog and PWM shutdown converge on the same predictable stopped state.</p></article>
  <article class="story-feature is-code"><div class="story-feature-top"><span>02</span><button class="feature-code-trigger" type="button" data-code-key="sync" aria-label="Open Link recovery and synchronization source code"></button></div><h4>Link recovery & sync</h4><p>Reconnect restores authoritative mission state before the vehicle is allowed to move again.</p></article>
</div>
''')

css = r'''

    /* ============================================================
       System media + source cards v1
       Three clear interactions: video, source code, interactive map.
       ============================================================ */
    .app-view-system .story-feature .feature-code-trigger {
      position: absolute !important;
      inset: 0 !important;
      z-index: 8 !important;
      width: 100% !important;
      height: 100% !important;
      margin: 0 !important;
      padding: 0 !important;
      border: 0 !important;
      border-radius: inherit !important;
      background: transparent !important;
      color: transparent !important;
      font-size: 0 !important;
      opacity: 0 !important;
      cursor: pointer;
    }

    .app-view-system .story-feature.is-video::after {
      content: "▶";
      font-size: 10px;
      letter-spacing: 0;
    }

    .app-view-system .story-feature.is-code::after {
      content: "</>";
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 10px;
      letter-spacing: -.04em;
    }

    .app-view-system .story-feature.is-map::after { content: "↗"; }

    .app-view-system .story-feature.is-static {
      cursor: default !important;
    }

    .app-view-system .story-feature.is-static::after {
      display: none !important;
    }

    .feature-code-modal {
      position: fixed;
      inset: 0;
      z-index: 2600;
      display: grid;
      place-items: center;
      padding: 28px;
      opacity: 0;
      visibility: hidden;
      pointer-events: none;
      transition: opacity .24s ease, visibility .24s ease;
    }

    .feature-code-modal.is-open {
      opacity: 1;
      visibility: visible;
      pointer-events: auto;
    }

    .feature-code-backdrop {
      position: absolute;
      inset: 0;
      border: 0;
      background: rgba(1,5,3,.68);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      cursor: default;
    }

    .feature-code-dialog {
      position: relative;
      z-index: 1;
      width: min(980px, 94vw);
      max-height: min(780px, 88vh);
      overflow: hidden;
      display: grid;
      grid-template-rows: auto auto minmax(0, 1fr) auto;
      border: 1px solid rgba(198,222,205,.16);
      border-radius: 18px;
      background:
        radial-gradient(ellipse at 75% 0%, rgba(118,185,0,.075), transparent 38%),
        linear-gradient(145deg, rgba(17,27,22,.94), rgba(5,11,8,.96));
      box-shadow: 0 38px 120px rgba(0,0,0,.56), inset 0 1px 0 rgba(255,255,255,.035);
      backdrop-filter: blur(26px);
      -webkit-backdrop-filter: blur(26px);
    }

    .feature-code-windowbar {
      min-height: 48px;
      display: grid;
      grid-template-columns: 94px minmax(0, 1fr) 42px;
      align-items: center;
      gap: 10px;
      padding: 0 14px;
      border-bottom: 1px solid rgba(210,227,215,.08);
      background: rgba(255,255,255,.018);
    }

    .feature-code-traffic { display: flex; gap: 7px; }
    .feature-code-traffic i { width: 10px; height: 10px; border-radius: 50%; opacity: .72; }
    .feature-code-traffic i:nth-child(1) { background: #ff5f57; }
    .feature-code-traffic i:nth-child(2) { background: #febc2e; }
    .feature-code-traffic i:nth-child(3) { background: #28c840; }

    .feature-code-file {
      min-width: 0;
      text-align: center;
      color: rgba(215,225,219,.62);
      font-family: var(--font-ui, system-ui, sans-serif);
      font-size: 11px;
      letter-spacing: .02em;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .feature-code-file strong { color: rgba(241,245,242,.82); font-weight: 570; }
    .feature-code-file span { color: rgba(139,196,52,.68); margin-right: 8px; }

    .feature-code-close {
      width: 30px;
      height: 30px;
      border: 0;
      background: transparent;
      color: rgba(228,235,230,.54);
      font-size: 22px;
      cursor: pointer;
    }

    .feature-code-copy {
      padding: 22px 26px 16px;
      border-bottom: 1px solid rgba(210,227,215,.07);
    }

    .feature-code-copy .story-label { display: block; margin-bottom: 7px; }
    .feature-code-copy h3 { margin: 0; font-size: clamp(25px, 3vw, 38px); }
    .feature-code-copy p {
      max-width: 720px;
      margin: 9px 0 0;
      color: rgba(202,214,206,.58);
      font: 13px/1.65 var(--font-ui, system-ui, sans-serif);
    }

    .feature-code-stage {
      min-height: 220px;
      overflow: auto;
      padding: 22px 26px 28px;
      background:
        linear-gradient(90deg, rgba(118,185,0,.025) 1px, transparent 1px),
        rgba(1,5,3,.34);
      background-size: 42px 100%;
    }

    .feature-code-stage pre { margin: 0; min-width: max-content; }
    .feature-code-stage code {
      color: rgba(222,232,225,.82);
      font: 12.5px/1.72 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      tab-size: 2;
      white-space: pre;
    }

    .feature-code-actions {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      padding: 13px 18px;
      border-top: 1px solid rgba(210,227,215,.08);
      background: rgba(255,255,255,.015);
    }

    .feature-code-actions button,
    .feature-code-actions a {
      min-height: 34px;
      display: inline-flex;
      align-items: center;
      padding: 0 12px;
      border: 1px solid rgba(193,215,200,.13);
      border-radius: 7px;
      background: rgba(255,255,255,.025);
      color: rgba(227,235,230,.72);
      font: 11px/1 var(--font-ui, system-ui, sans-serif);
      text-decoration: none;
      cursor: pointer;
    }

    .feature-code-actions a {
      border-color: rgba(118,185,0,.27);
      color: rgba(186,228,123,.86);
      background: rgba(118,185,0,.055);
    }

    body.feature-code-open { overflow: hidden; }

    @media (max-width: 620px) {
      .feature-code-modal { padding: 12px; }
      .feature-code-dialog { width: 100%; max-height: 91vh; border-radius: 14px; }
      .feature-code-copy, .feature-code-stage { padding-left: 18px; padding-right: 18px; }
      .feature-code-windowbar { grid-template-columns: 72px minmax(0,1fr) 34px; }
    }
'''

if 'System media + source cards v1' not in text:
    text = text.replace('\n  </style>', css + '\n  </style>', 1)

modal = r'''

    <div class="feature-code-modal" id="featureCodeModal" aria-hidden="true" role="dialog" aria-modal="true" aria-labelledby="featureCodeTitle">
      <button class="feature-code-backdrop" type="button" aria-label="Close source code"></button>
      <div class="feature-code-dialog">
        <div class="feature-code-windowbar">
          <div class="feature-code-traffic" aria-hidden="true"><i></i><i></i><i></i></div>
          <div class="feature-code-file"><span id="featureCodeBranch"></span><strong id="featureCodePath"></strong></div>
          <button class="feature-code-close" type="button" aria-label="Close source code">×</button>
        </div>
        <div class="feature-code-copy">
          <span class="story-label">Source / Implementation</span>
          <h3 id="featureCodeTitle">Source code</h3>
          <p id="featureCodeDescription"></p>
        </div>
        <div class="feature-code-stage"><pre><code id="featureCodeText"></code></pre></div>
        <div class="feature-code-actions">
          <button id="featureCodeCopy" type="button">Copy snippet</button>
          <a id="featureCodeSource" href="#" target="_blank" rel="noopener">Open source ↗</a>
        </div>
      </div>
    </div>
'''

if 'id="featureCodeModal"' not in text:
    contact_marker = '\n    <section class="app-view app-view-contact"'
    text = text.replace(contact_marker, modal + contact_marker, 1)

js = r'''

<script>
(() => {
  const samples = {
    voice: {
      title: 'Voice interaction', branch: 'pc_backend', path: 'voiceGateway.js',
      description: 'The PC gateway accepts a voice command, resolves it through an external model when configured, and falls back to deterministic rules.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/pc_backend/voiceGateway.js',
      code: `async resolveVoiceCommand(message) {
  if (this.config.externalModelUrl) {
    try {
      return await this.callExternalModel(message);
    } catch (error) {
      this.emit('voice_error', {
        type: 'voice_error',
        message: 'external model failed, fallback to rules: ' + error.message
      });
    }
  }
  return this.resolveByRules(message);
}`
    },
    orderLifecycle: {
      title: 'Order lifecycle', branch: 'pc_backend', path: 'scheduler.js',
      description: 'A request is validated, persisted as an order, then immediately enters scheduler reconciliation.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/pc_backend/scheduler.js',
      code: `createOrder(input) {
  const pickup = this.database.getLocation(input.pickup_location_id);
  const dropoff = this.database.getLocation(input.dropoff_location_id);
  if (!pickup || !dropoff) {
    throw codedError('INVALID_LOCATION', 'pickup or drop-off location does not exist');
  }
  const result = this.database.createOrder({
    client_request_id: String(input.client_request_id),
    nickname: String(input.nickname).trim(),
    map_id: Number(pickup.map_id),
    pickup_node: Number(pickup.node_id),
    dropoff_node: Number(dropoff.node_id)
  });
  this.reconcile();
  this.changed(result.created ? 'order_created' : 'order_create_replayed');
  return { created: result.created, order: this.database.getOrder(result.order.order_id) };
}`
    },
    handoff: {
      title: 'On-car handoff', branch: 'delivery-refactor', path: 'Loongson_2K300_301_LIB/user_app/delivery_controller.cpp',
      description: 'The vehicle coordinator consumes screen events in the same loop that handles server messages, arrival observation and heartbeat.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/delivery-refactor/Loongson_2K300_301_LIB/user_app/delivery_controller.cpp',
      code: `void DeliveryController::Process(void)
{
  if (!cfg_.enabled || gw_ == NULL) return;

  ServerMessage m;
  while (gw_->Poll_Server_Message(&m)) {
    Handle_Message(m);
  }

  Process_Screen_Events();
  Handle_Link_Change();
  Observe_Arrival();
}`
    },
    perception: {
      title: 'Perception & localization', branch: 'delivery-refactor', path: 'Loongson_2K300_301_LIB/user_app/Tag_Scan.cpp',
      description: 'The AprilTag detector anchors physical observations to campus node IDs while throttling detection for real-time execution.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/delivery-refactor/Loongson_2K300_301_LIB/user_app/Tag_Scan.cpp',
      code: `void Tag_Scan_Init(void)
{
  tf = tag16h5_create();
  td = apriltag_detector_create();
  apriltag_detector_add_family(td, tf);
  td->quad_decimate = 2.0f;
  td->quad_sigma = 0.0f;
  td->nthreads = 1;
  td->refine_edges = 1;
}

void Tag_Scan_Process(void)
{
  detect_skip++;
  if (detect_skip < DETECT_INTERVAL) return;
  detect_skip = 0;
  // detect tag id, angle and image-space center ...
}`
    },
    motionControl: {
      title: 'Navigation & 5 ms control', branch: 'delivery-refactor', path: 'Loongson_2K300_301_LIB/user_app/control.cpp',
      description: 'Navigation publishes a command snapshot; the low-level loop consumes it behind an independent safety-inhibit boundary.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/delivery-refactor/Loongson_2K300_301_LIB/user_app/control.cpp',
      code: `void Control_Publish_Command(bool motion_permitted, uint8_t stop_mode,
                             int32_t action, int32_t follow_left_cmd,
                             double speed_cmd, double ele_cmd)
{
  ControlCommandSnapshot cmd;
  cmd.generation       = ++g_cmd_generation;
  cmd.motion_permitted = motion_permitted;
  cmd.stop_mode        = stop_mode;
  cmd.action           = action;
  cmd.follow_left      = follow_left_cmd;
  cmd.target_speed     = speed_cmd;
  cmd.ele_current      = ele_cmd;
  g_cmd_channel.publish(cmd);
}`
    },
    scheduler: {
      title: 'Dispatch scheduler', branch: 'pc_backend', path: 'scheduler.js',
      description: 'Insertion only survives when capacity remains feasible; the scheduler then searches pickup/drop-off positions for the lowest feasible detour.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/pc_backend/scheduler.js',
      code: `function capacityFeasible(stops, initialLoadedCount) {
  let loaded = Number(initialLoadedCount);
  if (loaded < 0 || loaded > CAR_CAPACITY) return false;
  for (const stop of stops) {
    const operations = [...(stop.operations || [])].sort((left) =>
      left.action === STOP_ACTION.DROPOFF ? -1 : 1
    );
    for (const operation of operations) {
      loaded += operation.action === STOP_ACTION.PICKUP ? 1 : -1;
      if (loaded < 0 || loaded > CAR_CAPACITY) return false;
    }
  }
  return loaded >= 0;
}`
    },
    routePlanner: {
      title: 'Weighted route planner', branch: 'pc_backend', path: 'routePlanner.js',
      description: 'The PC graph excludes blocked edges and adds manual/dynamic penalties before shortest-path search.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/pc_backend/routePlanner.js',
      code: `for (const [from, to, storedDistance] of map.edges) {
  const condition = this.mapWeights.getEdgeCondition(mapId, from, to);
  if (condition.blocked) continue;

  const weight = this.mapWeights.baseDistance(
    mapId, from, to, storedDistance
  ) + Math.max(0, Number(condition.manual_penalty || 0))
    + Math.max(0, Number(condition.dynamic_penalty || 0));

  graph.get(from).push({ to, weight });
  graph.get(to).push({ to: from, weight });
}`
    },
    gateway: {
      title: 'PC ↔ car gateway', branch: 'pc_backend', path: 'carGateway.js',
      description: 'A persistent TCP link transports newline-delimited JSON and converts heartbeat expiry into an explicit offline event.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/pc_backend/carGateway.js',
      code: `checkHeartbeat() {
  if (!this.socket || this.lastSeenAt == null) return;
  if (Date.now() - this.lastSeenAt <= this.config.carOfflineTimeoutMs) return;
  const socket = this.socket;
  this.emit('offline', { reason: 'heartbeat_timeout', remote: this.remote });
  socket.destroy();
}

send(message) {
  if (!this.socket || this.socket.destroyed || !this.socket.writable) return false;
  this.socket.write(JSON.stringify(message) + '\\n');
  return true;
}`
    },
    safety: {
      title: 'Fail-safe control', branch: 'delivery-refactor', path: 'Loongson_2K300_301_LIB/user_app/control.cpp',
      description: 'Any higher-layer failure can set an inhibit bit; safe shutdown explicitly zeros motor PWM and recenters steering.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/delivery-refactor/Loongson_2K300_301_LIB/user_app/control.cpp',
      code: `void Safety_Inhibit_Set(uint32_t reason_bits)
{
  g_inhibit_reason.fetch_or(reason_bits, std::memory_order_acq_rel);
}

void Control_Safe_Shutdown(void)
{
  current_speed = 0;
  left_speed = right_speed = 0;
  left_motor_duty = right_motor_duty = 0;
  motor_pwm1.atim_pwm_set_duty(0);
  motor_pwm2.atim_pwm_set_duty(0);
  gpio1.gpio_level_set(GPIO_LOW);
  gpio2.gpio_level_set(GPIO_LOW);
  servo_pwm.gtim_pwm_set_duty(SERVO_MID);
}`
    },
    sync: {
      title: 'Link recovery & sync', branch: 'delivery-refactor', path: 'Loongson_2K300_301_LIB/user_app/delivery_controller.cpp',
      description: 'A dropped link pauses navigation; reconnect starts a new authenticated/synchronized session rather than silently resuming motion.',
      source: 'https://github.com/Cx300-fh/IntelligentCar_Delivery/blob/delivery-refactor/Loongson_2K300_301_LIB/user_app/delivery_controller.cpp',
      code: `void DeliveryController::Handle_Link_Change(void)
{
  bool up = gw_->Is_Link_Up();
  if (up == last_link_up_) return;
  last_link_up_ = up;

  if (up) {
    synced_ = false;
    Send_Hello();
  } else {
    synced_ = false;
    if (state_ == DELIVERY_NAVIGATING) nav_fsm.pause_task();
    if (state_ != DELIVERY_EMERGENCY && state_ != DELIVERY_FAULT) {
      state_ = DELIVERY_WAIT_CONN;
    }
  }
}`
    }
  };

  const modal = document.getElementById('featureCodeModal');
  if (!modal) return;
  const title = document.getElementById('featureCodeTitle');
  const description = document.getElementById('featureCodeDescription');
  const branch = document.getElementById('featureCodeBranch');
  const filePath = document.getElementById('featureCodePath');
  const code = document.getElementById('featureCodeText');
  const source = document.getElementById('featureCodeSource');
  const copyButton = document.getElementById('featureCodeCopy');
  let lastTrigger = null;

  const close = () => {
    modal.classList.remove('is-open');
    modal.setAttribute('aria-hidden', 'true');
    document.body.classList.remove('feature-code-open');
    if (lastTrigger) lastTrigger.focus({ preventScroll: true });
  };

  document.querySelectorAll('.feature-code-trigger[data-code-key]').forEach((trigger) => {
    trigger.addEventListener('click', () => {
      const sample = samples[trigger.dataset.codeKey];
      if (!sample) return;
      lastTrigger = trigger;
      title.textContent = sample.title;
      description.textContent = sample.description;
      branch.textContent = sample.branch;
      filePath.textContent = sample.path;
      code.textContent = sample.code;
      source.href = sample.source;
      copyButton.textContent = 'Copy snippet';
      modal.classList.add('is-open');
      modal.setAttribute('aria-hidden', 'false');
      document.body.classList.add('feature-code-open');
      setTimeout(() => modal.querySelector('.feature-code-close')?.focus(), 0);
    });
  });

  modal.querySelector('.feature-code-backdrop')?.addEventListener('click', close);
  modal.querySelector('.feature-code-close')?.addEventListener('click', close);
  copyButton?.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(code.textContent || '');
      copyButton.textContent = 'Copied';
      setTimeout(() => copyButton.textContent = 'Copy snippet', 1000);
    } catch (_) {
      copyButton.textContent = 'Copy failed';
    }
  });

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && modal.classList.contains('is-open')) close();
  });
})();
</script>
'''

if 'const samples = {' not in text:
    text = text.replace('\n</body>', js + '\n</body>', 1)

# Basic validation
required = [
    'assets/media/web_order_ui.mp4',
    'assets/media/turnaround.mov',
    'data-code-key="voice"',
    'data-code-key="scheduler"',
    'data-code-key="routePlanner"',
    'data-code-key="gateway"',
    'id="featureCodeModal"',
    '<h4>Turn Around</h4>',
    '<h4>Obstacle-aware rerouting</h4>'
]
for token in required:
    if token not in text:
        raise RuntimeError(f'Missing expected token: {token}')

# Ensure old hypothetical video slots were actually removed from the simplified grids.
for obsolete in [
    'assets/media/system/order-web.mp4',
    'assets/media/system/move-navigation.mp4',
    'assets/media/system/scale-scheduler.mp4',
    'assets/media/system/trust-watchdog.mp4'
]:
    if obsolete in text:
        raise RuntimeError(f'Obsolete feature slot still present: {obsolete}')

path.write_text(text, encoding='utf-8')
