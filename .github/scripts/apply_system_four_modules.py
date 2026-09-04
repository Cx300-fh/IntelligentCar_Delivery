from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

# Remove the previous reversible blueprint experiment if present.
for begin, end in [
    ('<!-- ROBOT BLUEPRINT TRIAL v1 BEGIN -->', '<!-- ROBOT BLUEPRINT TRIAL v1 END -->'),
    ('<!-- SYSTEM FOUR MODULES v2 BEGIN -->', '<!-- SYSTEM FOUR MODULES v2 END -->'),
]:
    if begin in text and end in text:
        before, rest = text.split(begin, 1)
        _, after = rest.split(end, 1)
        text = before + after

block = r'''
<!-- SYSTEM FOUR MODULES v2 BEGIN -->
<style>
  /* Four-module System redesign: Order / System / Safety / Future Work */
  .app-view-system .system-view-frame {
    width: min(1420px, calc(100% - 72px));
    padding-top: 24px;
  }
  .app-view-system .system-story-head {
    margin: 0 0 28px;
    padding: 0 0 18px;
    border-bottom: 1px solid rgba(241,243,240,.07);
  }
  .app-view-system .system-story-head .eyebrow { margin: 0 0 8px; }
  .app-view-system .system-story-head h2 {
    margin: 0;
    max-width: 960px;
    font-size: clamp(42px, 5vw, 70px);
    line-height: .94;
  }
  .app-view-system .system-workspace {
    grid-template-columns: 124px minmax(0, 1fr);
    gap: 30px;
    min-height: 0;
  }
  .app-view-system .system-story-nav {
    align-self: start;
    padding-top: 2px;
  }
  .app-view-system .system-story-scroll {
    padding: 0 4px 64px 0;
  }
  .app-view-system .system-story-scroll > .story-panel {
    display: none !important;
  }
  .app-view-system .story-tab.four-tab {
    min-height: 64px;
    padding: 9px 0 9px 27px;
  }
  .app-view-system .story-tab.four-tab span { font-size: 8px; }

  .system-four-stage { display: block; }
  .four-section {
    position: relative;
    min-height: 560px;
    padding: 4px 0 54px;
    border-bottom: 1px solid rgba(241,243,240,.07);
  }
  .four-section + .four-section { padding-top: 42px; }
  .four-section-head {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(220px, 340px);
    gap: 34px;
    align-items: end;
    margin: 0 0 26px;
  }
  .four-section-head .four-kicker {
    display: block;
    margin-bottom: 7px;
    color: var(--green);
    font-family: "Didot Italic", Didot, "Bodoni 72", Georgia, serif;
    font-style: italic;
    font-size: 13px;
  }
  .four-section-head h3 {
    margin: 0;
    max-width: 760px;
    font-family: "Didot Bold", Didot, "Bodoni 72", Georgia, serif;
    font-size: clamp(30px, 3vw, 46px);
    line-height: 1.02;
    text-transform: none;
  }
  .four-section-head p {
    margin: 0;
    color: rgba(222,228,223,.56);
    font-size: 13px;
    line-height: 1.6;
  }

  /* ORDER — evidence-led, not blueprint */
  .order-layout {
    display: grid;
    grid-template-columns: minmax(0, 1.55fr) minmax(290px, .75fr);
    gap: 18px;
    align-items: stretch;
  }
  .order-video-card {
    position: relative;
    min-height: 390px;
    overflow: hidden;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 22px;
    background: #0a100c;
  }
  .order-video-card video {
    width: 100%;
    height: 100%;
    min-height: 390px;
    object-fit: cover;
    display: block;
    opacity: .88;
  }
  .order-video-card::after {
    content: '';
    position: absolute;
    inset: 0;
    background: linear-gradient(180deg, rgba(5,10,7,.05), rgba(5,10,7,.62));
    pointer-events: none;
  }
  .order-video-meta {
    position: absolute;
    z-index: 2;
    left: 20px;
    right: 20px;
    bottom: 18px;
    display: flex;
    justify-content: space-between;
    align-items: end;
    gap: 20px;
  }
  .order-video-meta strong {
    display: block;
    font-family: "Didot Bold", Didot, "Bodoni 72", Georgia, serif;
    font-size: 23px;
  }
  .order-video-meta span { color: rgba(228,233,228,.58); font-size: 11px; }
  .order-video-meta button {
    border: 1px solid rgba(118,185,0,.45);
    border-radius: 999px;
    padding: 8px 12px;
    background: rgba(7,12,9,.58);
    color: rgba(241,243,240,.8);
    cursor: pointer;
  }
  .order-stack { display: grid; gap: 12px; }
  .evidence-card {
    position: relative;
    min-height: 118px;
    padding: 16px 16px 14px;
    border: 1px solid rgba(241,243,240,.09);
    border-radius: 16px;
    background: linear-gradient(135deg, rgba(241,243,240,.035), rgba(6,12,8,.1));
    cursor: pointer;
    transition: border-color .2s ease, transform .2s ease;
  }
  .evidence-card:hover { border-color: rgba(118,185,0,.38); transform: translateY(-2px); }
  .evidence-card .evidence-type { color: rgba(118,185,0,.72); font-size: 8px; letter-spacing: .14em; text-transform: uppercase; }
  .evidence-card h4 { margin: 7px 0 5px; font-family: "Didot Bold", Didot, Georgia, serif; font-size: 18px; text-transform: none; }
  .evidence-card p { margin: 0; color: rgba(222,228,223,.52); font-size: 11px; line-height: 1.45; }
  .evidence-card::after { content: '↗'; position: absolute; right: 13px; top: 12px; color: rgba(118,185,0,.55); font-size: 12px; }

  /* SYSTEM — the only robot blueprint */
  .system-blueprint {
    position: relative;
    display: grid;
    grid-template-columns: minmax(190px,.75fr) minmax(430px,1.5fr) minmax(190px,.75fr);
    gap: clamp(26px,3.2vw,52px);
    min-height: 500px;
    align-items: stretch;
  }
  .system-blueprint::before {
    content: '';
    position: absolute;
    inset: 0 9% 0;
    background:
      radial-gradient(circle at 50% 48%, rgba(118,185,0,.065), transparent 36%),
      linear-gradient(rgba(118,185,0,.015) 1px, transparent 1px),
      linear-gradient(90deg, rgba(118,185,0,.015) 1px, transparent 1px);
    background-size: auto, 36px 36px, 36px 36px;
    pointer-events: none;
  }
  .system-blueprint-side {
    position: relative;
    z-index: 2;
    display: flex;
    flex-direction: column;
    justify-content: space-evenly;
    gap: 9px;
    padding: 12px 0;
  }
  .system-node {
    position: relative;
    min-height: 70px;
    padding: 10px 12px 10px 14px;
    border: 0;
    border-left: 1px solid rgba(241,243,240,.13);
    background: linear-gradient(90deg, rgba(241,243,240,.034), transparent 82%);
    color: inherit;
    text-align: left;
  }
  .system-blueprint-right .system-node {
    text-align: right;
    border-left: 0;
    border-right: 1px solid rgba(241,243,240,.13);
    background: linear-gradient(270deg, rgba(241,243,240,.034), transparent 82%);
  }
  .system-node.is-action { cursor: pointer; }
  .system-node.is-action:hover { border-color: rgba(118,185,0,.7); }
  .system-node::after {
    content: '';
    position: absolute;
    left: 100%;
    top: 50%;
    width: clamp(28px,4vw,64px);
    height: 1px;
    background: linear-gradient(90deg, rgba(118,185,0,.45), rgba(118,185,0,.05));
  }
  .system-blueprint-right .system-node::after {
    left: auto; right: 100%;
    background: linear-gradient(270deg, rgba(118,185,0,.45), rgba(118,185,0,.05));
  }
  .system-node .node-type { display: block; color: rgba(118,185,0,.72); font-size: 8px; letter-spacing: .13em; text-transform: uppercase; }
  .system-node strong { display: block; margin-top: 3px; font-family: "Didot Bold", Didot, Georgia, serif; font-size: 16px; line-height: 1.05; }
  .system-node small { display: block; margin-top: 4px; color: rgba(222,228,223,.48); font-size: 9px; line-height: 1.3; }

  .system-robot-core {
    position: relative;
    z-index: 1;
    min-height: 500px;
    overflow: hidden;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 24px;
    background:
      linear-gradient(180deg, rgba(6,11,8,.04), rgba(6,11,8,.68)),
      url("assets/delivery-robot-hero.png") center 61% / cover no-repeat;
    box-shadow: 0 28px 70px rgba(0,0,0,.17), inset 0 1px rgba(255,255,255,.02);
  }
  .system-robot-core::before {
    content: '';
    position: absolute;
    inset: 0;
    background: linear-gradient(90deg, rgba(5,10,7,.44), transparent 26%, transparent 74%, rgba(5,10,7,.44));
    pointer-events: none;
  }
  .robot-core-label {
    position: absolute;
    z-index: 2;
    top: 18px;
    left: 50%;
    transform: translateX(-50%);
    text-align: center;
  }
  .robot-core-label span { color: rgba(118,185,0,.72); font-size: 9px; letter-spacing: .13em; text-transform: uppercase; }
  .robot-core-label strong { display: block; margin-top: 3px; font-family: "Didot Bold", Didot, Georgia, serif; font-size: 20px; }
  .robot-core-chips {
    position: absolute;
    z-index: 2;
    left: 18px; right: 18px; bottom: 16px;
    display: flex; flex-wrap: wrap; justify-content: center; gap: 6px;
  }
  .robot-core-chips span {
    padding: 5px 8px;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 999px;
    background: rgba(7,12,9,.58);
    color: rgba(229,234,229,.62);
    font-size: 8px;
    backdrop-filter: blur(10px);
  }

  /* SAFETY — explicit state transition, no robot blueprint */
  .safety-flow {
    display: grid;
    grid-template-columns: repeat(4, minmax(0,1fr));
    gap: 0;
    margin-bottom: 18px;
    border: 1px solid rgba(241,243,240,.09);
    border-radius: 18px;
    overflow: hidden;
  }
  .safety-step { position: relative; padding: 18px 17px; min-height: 110px; background: rgba(241,243,240,.02); }
  .safety-step + .safety-step { border-left: 1px solid rgba(241,243,240,.08); }
  .safety-step span { color: rgba(118,185,0,.68); font-size: 9px; letter-spacing: .12em; }
  .safety-step strong { display: block; margin-top: 7px; font-family: "Didot Bold", Didot, Georgia, serif; font-size: 18px; }
  .safety-step p { margin: 6px 0 0; color: rgba(222,228,223,.48); font-size: 10px; line-height: 1.45; }
  .safety-cards { display: grid; grid-template-columns: repeat(3,minmax(0,1fr)); gap: 13px; }
  .safety-card { min-height: 150px; padding: 16px; border: 1px solid rgba(241,243,240,.09); border-radius: 16px; background: linear-gradient(135deg, rgba(241,243,240,.03), transparent); cursor: pointer; }
  .safety-card:hover { border-color: rgba(118,185,0,.35); }
  .safety-card .evidence-type { color: rgba(118,185,0,.7); font-size: 8px; letter-spacing:.12em; text-transform:uppercase; }
  .safety-card h4 { margin: 10px 0 5px; font-family: "Didot Bold", Didot, Georgia, serif; font-size: 19px; text-transform:none; }
  .safety-card p { margin:0; color:rgba(222,228,223,.5); font-size:11px; line-height:1.5; }

  /* FUTURE WORK — roadmap, with EEG placeholder */
  .future-grid { display: grid; grid-template-columns: minmax(0,1.35fr) repeat(2,minmax(220px,.65fr)); gap: 14px; }
  .future-main,
  .future-card { position: relative; min-height: 250px; overflow:hidden; border:1px solid rgba(241,243,240,.09); border-radius:20px; background:rgba(241,243,240,.022); }
  .future-main { padding: 22px; background: radial-gradient(circle at 24% 70%, rgba(118,185,0,.10), transparent 33%), rgba(241,243,240,.022); }
  .future-main .future-status { display:inline-flex; padding:5px 8px; border:1px solid rgba(118,185,0,.28); border-radius:999px; color:rgba(118,185,0,.8); font-size:8px; letter-spacing:.11em; text-transform:uppercase; }
  .future-main h4,
  .future-card h4 { margin: 16px 0 7px; font-family:"Didot Bold",Didot,Georgia,serif; font-size:clamp(22px,2.2vw,32px); text-transform:none; }
  .future-main p,
  .future-card p { margin:0; max-width:520px; color:rgba(222,228,223,.53); font-size:11px; line-height:1.55; }
  .eeg-wave { position:absolute; left:22px; right:22px; bottom:26px; height:72px; opacity:.7; }
  .eeg-wave svg { width:100%; height:100%; }
  .eeg-wave path { fill:none; stroke:rgba(118,185,0,.72); stroke-width:1.4; vector-effect:non-scaling-stroke; }
  .future-card { padding:20px; }
  .future-card .future-number { color:rgba(118,185,0,.65); font-size:9px; letter-spacing:.12em; }
  .future-card::after { content:''; position:absolute; left:20px; right:20px; bottom:20px; height:1px; background:linear-gradient(90deg,rgba(118,185,0,.45),transparent); }

  .future-note {
    margin-top:14px;
    padding:12px 14px;
    border-left:1px solid rgba(118,185,0,.55);
    color:rgba(222,228,223,.5);
    font-size:11px;
    background:linear-gradient(90deg,rgba(118,185,0,.035),transparent);
  }

  @media (max-width: 1100px) {
    .app-view-system .system-view-frame { width:min(100% - 40px,1180px); }
    .system-blueprint { grid-template-columns:minmax(160px,.7fr) minmax(340px,1.3fr) minmax(160px,.7fr); gap:22px; }
    .system-node::after { width:24px; }
    .four-section-head { grid-template-columns:1fr; }
    .four-section-head p { max-width:640px; }
  }
  @media (max-width: 840px) {
    .app-view-system .system-workspace { grid-template-columns:1fr; }
    .order-layout, .system-blueprint, .future-grid { grid-template-columns:1fr; }
    .system-robot-core { grid-row:1; min-height:360px; }
    .system-blueprint-side { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); }
    .system-node, .system-blueprint-right .system-node { text-align:left; border:1px solid rgba(241,243,240,.09); border-radius:12px; background:rgba(241,243,240,.02); }
    .system-node::after { display:none; }
    .safety-flow, .safety-cards { grid-template-columns:1fr 1fr; }
    .safety-step + .safety-step { border-left:0; border-top:1px solid rgba(241,243,240,.08); }
  }
</style>
<script>
(() => {
  const scroll = document.getElementById('systemStoryScroll');
  const nav = document.querySelector('.app-view-system .system-story-nav');
  if (!scroll || !nav) return;

  // Preserve the existing interactive route lab, then move it into the integrated System section.
  const routeLab = document.getElementById('moveRouteLab');

  nav.innerHTML = `
    <button class="story-tab four-tab is-active" type="button" data-four="order"><span>01</span>Order</button>
    <button class="story-tab four-tab" type="button" data-four="system"><span>02</span>System</button>
    <button class="story-tab four-tab" type="button" data-four="safety"><span>03</span>Safety</button>
    <button class="story-tab four-tab" type="button" data-four="future"><span>04</span>Future Work</button>`;

  const stage = document.createElement('div');
  stage.className = 'system-four-stage';
  stage.innerHTML = `
    <section class="four-section" id="fourOrder" data-four-panel="order">
      <div class="four-section-head"><div><span class="four-kicker">01 / Order</span><h3>From request to a dispatchable mission.</h3></div><p>Keep interaction concrete: one real UI, one voice gateway, one route decision and one persistent order lifecycle.</p></div>
      <div class="order-layout">
        <div class="order-video-card">
          <video src="assets/media/web_order_ui.mp4" muted autoplay loop playsinline preload="metadata"></video>
          <div class="order-video-meta"><div><span>REAL INTERFACE</span><strong>Web Order UI</strong></div><button type="button" data-proxy-video="web_order_ui.mp4">Open video ↗</button></div>
        </div>
        <div class="order-stack">
          <article class="evidence-card" data-proxy-code="voice"><span class="evidence-type">Source code</span><h4>Voice gateway</h4><p>Natural-language requests enter the same task pipeline as the web UI.</p></article>
          <article class="evidence-card" data-proxy-code="routePlanner"><span class="evidence-type">Planning</span><h4>Route preview</h4><p>Weighted paths are computed before the mission is dispatched.</p></article>
          <article class="evidence-card" data-proxy-code="orderLifecycle"><span class="evidence-type">Backend</span><h4>Order lifecycle</h4><p>Pickup and drop-off become persistent mission stops instead of transient UI state.</p></article>
        </div>
      </div>
    </section>

    <section class="four-section" id="fourSystem" data-four-panel="system">
      <div class="four-section-head"><div><span class="four-kicker">02 / System</span><h3>One robot, coordinated from sensing to dispatch.</h3></div><p>This is the only blueprint view: the physical car stays centered while the modules around it reveal real videos, source code and the interactive route graph.</p></div>
      <div class="system-blueprint">
        <div class="system-blueprint-side">
          <button class="system-node is-action" type="button" data-proxy-code="perception"><span class="node-type">Perception</span><strong>Camera + AprilTag</strong><small>Physical-node localization</small></button>
          <button class="system-node is-action" type="button" data-proxy-video="weight.mp4"><span class="node-type">Hardware</span><strong>Load cell</strong><small>Measured payload input</small></button>
          <button class="system-node is-action" type="button" data-proxy-video="turnaround.mp4"><span class="node-type">Motion</span><strong>Turn Around</strong><small>Dedicated U-turn behavior</small></button>
          <button class="system-node is-action" type="button" data-proxy-video="obstacle.mp4"><span class="node-type">Perception</span><strong>Obstacle handling</strong><small>Stop before replanning</small></button>
        </div>
        <div class="system-robot-core">
          <div class="robot-core-label"><span>THU Delivery</span><strong>Integrated Vehicle</strong></div>
          <div class="robot-core-chips"><span>Frame & shell</span><span>Motor drive</span><span>Steering</span><span>On-car display</span><span>Cargo bay</span></div>
        </div>
        <div class="system-blueprint-side system-blueprint-right">
          <button class="system-node is-action" type="button" data-open-route="1"><span class="node-type">Interactive</span><strong>Dynamic rerouting</strong><small>Dijkstra + weighted graph</small></button>
          <button class="system-node is-action" type="button" data-proxy-code="safety"><span class="node-type">Control</span><strong>Navigation + 5 ms loop</strong><small>FSM and motor command</small></button>
          <button class="system-node is-action" type="button" data-proxy-code="scheduler"><span class="node-type">PC backend</span><strong>Dispatch scheduler</strong><small>Capacity + stop insertion</small></button>
          <button class="system-node is-action" type="button" data-proxy-code="gateway"><span class="node-type">Networking</span><strong>PC ↔ Car gateway</strong><small>Commands, heartbeat, events</small></button>
        </div>
      </div>
      <div id="fourRouteMount"></div>
    </section>

    <section class="four-section" id="fourSafety" data-four-panel="safety">
      <div class="four-section-head"><div><span class="four-kicker">03 / Safety</span><h3>A safe stop is a designed system state.</h3></div><p>Safety is shown as a state transition rather than another robot diagram: detect a fault, inhibit motion, stop predictably, then synchronize before resuming.</p></div>
      <div class="safety-flow">
        <div class="safety-step"><span>01</span><strong>Detect</strong><p>Link loss, stale command, obstacle or manual emergency intervention.</p></div>
        <div class="safety-step"><span>02</span><strong>Inhibit</strong><p>Motion permission is removed before another command can propagate.</p></div>
        <div class="safety-step"><span>03</span><strong>Stop</strong><p>PWM and motion outputs converge on one predictable stopped state.</p></div>
        <div class="safety-step"><span>04</span><strong>Recover</strong><p>Reconnect and state sync restore an authoritative mission state.</p></div>
      </div>
      <div class="safety-cards">
        <article class="safety-card" data-proxy-code="safety"><span class="evidence-type">Control.cpp</span><h4>Fail-safe control</h4><p>Watchdog, safety inhibit and PWM shutdown share the same low-level stop path.</p></article>
        <article class="safety-card" data-proxy-code="sync"><span class="evidence-type">Delivery controller</span><h4>Link recovery & sync</h4><p>The vehicle does not resume until mission state is synchronized after reconnect.</p></article>
        <article class="safety-card" data-proxy-video="obstacle.mp4"><span class="evidence-type">Real response</span><h4>Physical stop behavior</h4><p>The obstacle demo provides visible evidence that safety logic ends in a physical stop.</p></article>
      </div>
    </section>

    <section class="four-section" id="fourFuture" data-four-panel="future">
      <div class="four-section-head"><div><span class="four-kicker">04 / Future Work</span><h3>Beyond delivery: new ways to control and scale the robot.</h3></div><p>Future work stays concrete by connecting to prototypes we can actually demonstrate next, rather than listing generic AI ambitions.</p></div>
      <div class="future-grid">
        <article class="future-main">
          <span class="future-status">Video incoming</span>
          <h4>Brain-wave / EEG controlled car</h4>
          <p>Bring the existing EEG interaction prototype into the delivery stack so user intent can become a navigation or motion command without a conventional interface.</p>
          <div class="eeg-wave" aria-hidden="true"><svg viewBox="0 0 800 90" preserveAspectRatio="none"><path d="M0 48 L70 48 L92 28 L110 72 L134 18 L158 62 L182 44 L240 44 L262 22 L278 66 L298 34 L320 50 L390 50 L410 29 L428 70 L450 18 L476 58 L500 46 L570 46 L590 25 L610 68 L632 33 L658 49 L800 49"/></svg></div>
        </article>
        <article class="future-card"><span class="future-number">02</span><h4>Multi-robot dispatch</h4><p>Extend the current scheduler from one vehicle to fleet-level task allocation and route conflict handling.</p></article>
        <article class="future-card"><span class="future-number">03</span><h4>Richer perception</h4><p>Fuse AprilTag localization with more general visual semantics for less instrumented campus environments.</p></article>
      </div>
      <div class="future-note">EEG demo slot is intentionally reserved. When the brain-wave vehicle video is uploaded to assets/media, this block can become a real playable evidence panel without changing the four-module structure.</div>
    </section>`;

  scroll.appendChild(stage);

  if (routeLab) {
    const mount = stage.querySelector('#fourRouteMount');
    mount.appendChild(routeLab);
    routeLab.style.marginTop = '28px';
  }

  const proxyCode = (key) => {
    const target = document.querySelector(`.story-panel .feature-code-trigger[data-code-key="${key}"]`);
    if (target) target.click();
  };
  const proxyVideo = (filename) => {
    const target = [...document.querySelectorAll('.story-panel .feature-video-trigger')]
      .find(el => (el.dataset.video || '').endsWith('/' + filename));
    if (target) target.click();
  };

  stage.addEventListener('click', (e) => {
    const codeEl = e.target.closest('[data-proxy-code]');
    if (codeEl) { proxyCode(codeEl.dataset.proxyCode); return; }
    const videoEl = e.target.closest('[data-proxy-video]');
    if (videoEl) { proxyVideo(videoEl.dataset.proxyVideo); return; }
    const routeEl = e.target.closest('[data-open-route]');
    if (routeEl && routeLab) {
      routeLab.scrollIntoView({ behavior: 'smooth', block: 'start' });
      routeLab.focus({ preventScroll: true });
    }
  });

  const tabs = [...nav.querySelectorAll('[data-four]')];
  const sections = [...stage.querySelectorAll('[data-four-panel]')];
  const setActive = (key) => tabs.forEach(tab => {
    const on = tab.dataset.four === key;
    tab.classList.toggle('is-active', on);
    tab.setAttribute('aria-selected', on ? 'true' : 'false');
  });
  tabs.forEach(tab => tab.addEventListener('click', () => {
    const section = stage.querySelector(`[data-four-panel="${tab.dataset.four}"]`);
    if (section) section.scrollIntoView({ behavior: 'smooth', block: 'start' });
    setActive(tab.dataset.four);
  }));

  let ticking = false;
  const updateActive = () => {
    ticking = false;
    const scRect = scroll.getBoundingClientRect();
    const marker = scRect.top + Math.min(180, scRect.height * .28);
    let current = sections[0];
    let best = Infinity;
    sections.forEach(section => {
      const d = Math.abs(section.getBoundingClientRect().top - marker);
      if (d < best) { best = d; current = section; }
    });
    if (current) setActive(current.dataset.fourPanel);
  };
  scroll.addEventListener('scroll', () => {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(updateActive);
  }, { passive: true });
  updateActive();
})();
</script>
<!-- SYSTEM FOUR MODULES v2 END -->
'''

if '</body>' not in text:
    raise SystemExit('missing </body>')
text = text.replace('</body>', block + '\n</body>', 1)
path.write_text(text, encoding='utf-8')
