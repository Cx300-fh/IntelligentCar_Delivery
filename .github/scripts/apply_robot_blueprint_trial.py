from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

BEGIN = '<!-- ROBOT BLUEPRINT TRIAL v1 BEGIN -->'
END = '<!-- ROBOT BLUEPRINT TRIAL v1 END -->'

if BEGIN in text and END in text:
    before, rest = text.split(BEGIN, 1)
    _, after = rest.split(END, 1)
    text = before + after

block = r'''
<!-- ROBOT BLUEPRINT TRIAL v1 BEGIN -->
<style>
  /* Robot Blueprint Trial v1 — reversible experiment based on dcee33b */
  .app-view-system .system-view-frame {
    width: min(1440px, calc(100% - 70px));
    padding-top: 24px;
  }

  .app-view-system .system-story-head {
    margin: 0 0 18px;
  }

  .app-view-system .system-story-head .eyebrow {
    margin-bottom: 8px;
  }

  .app-view-system .system-story-head h2 {
    max-width: 900px;
    margin: 0;
    font-size: clamp(44px, 5.1vw, 72px);
    line-height: .92;
  }

  .app-view-system .system-workspace {
    grid-template-columns: 118px minmax(0, 1fr);
    gap: 22px;
  }

  .app-view-system .story-panel {
    min-height: 0 !important;
    padding: 4px 0 36px !important;
  }

  .app-view-system .story-panel > .story-problem,
  .app-view-system .story-panel > .story-feature-grid,
  .app-view-system .story-panel-system > .system-final-copy,
  .app-view-system .story-panel-system > .system-architecture-v2,
  .app-view-system .story-panel-system > .system-feedback-line {
    display: none !important;
  }

  .app-view-system .system-story-scroll {
    padding: 0 2px 44px 0;
  }

  .robot-blueprint {
    position: relative;
    min-height: 610px;
    padding: 0;
    isolation: isolate;
  }

  .robot-blueprint::before {
    content: '';
    position: absolute;
    inset: 78px 10% 30px;
    background:
      radial-gradient(circle at 50% 46%, rgba(118,185,0,.075), transparent 38%),
      linear-gradient(rgba(118,185,0,.018) 1px, transparent 1px),
      linear-gradient(90deg, rgba(118,185,0,.018) 1px, transparent 1px);
    background-size: auto, 34px 34px, 34px 34px;
    mask-image: linear-gradient(to bottom, transparent, #000 15%, #000 84%, transparent);
    pointer-events: none;
    z-index: -1;
  }

  .bp-context {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    align-items: end;
    gap: 22px;
    margin: 0 0 16px;
    padding: 0 0 14px;
    border-bottom: 1px solid rgba(241,243,240,.085);
  }

  .bp-context-copy {
    min-width: 0;
  }

  .bp-kicker {
    display: block;
    margin-bottom: 5px;
    color: var(--green);
    font-family: "Didot Italic", Didot, "Bodoni 72", Georgia, serif;
    font-style: italic;
    font-size: 13px;
    letter-spacing: .02em;
  }

  .bp-context h3 {
    margin: 0;
    max-width: 820px;
    font-family: "Didot Bold", Didot, "Bodoni 72", Georgia, serif;
    font-size: clamp(26px, 2.3vw, 39px);
    font-weight: 700;
    line-height: 1.03;
    text-transform: none;
  }

  .bp-context p {
    margin: 0;
    max-width: 310px;
    color: rgba(220,226,221,.58);
    font-size: 12px;
    line-height: 1.55;
    text-align: right;
  }

  .bp-layout {
    display: grid;
    grid-template-columns: minmax(178px, .82fr) minmax(390px, 1.6fr) minmax(178px, .82fr);
    gap: clamp(24px, 3vw, 48px);
    align-items: stretch;
    min-height: 500px;
  }

  .bp-column {
    display: flex;
    flex-direction: column;
    justify-content: space-evenly;
    gap: 8px;
    min-width: 0;
    padding: 8px 0;
  }

  .bp-node {
    position: relative;
    width: 100%;
    min-height: 58px;
    padding: 9px 11px 9px 13px;
    border: 0;
    border-left: 1px solid rgba(241,243,240,.13);
    background: linear-gradient(90deg, rgba(241,243,240,.035), transparent 82%);
    color: inherit;
    text-align: left;
    appearance: none;
    transition: border-color .22s ease, transform .22s ease, background .22s ease;
  }

  .bp-column-right .bp-node {
    border-left: 0;
    border-right: 1px solid rgba(241,243,240,.13);
    background: linear-gradient(270deg, rgba(241,243,240,.035), transparent 82%);
    text-align: right;
    padding: 9px 13px 9px 11px;
  }

  .bp-node.is-action {
    cursor: pointer;
  }

  .bp-node.is-action:hover,
  .bp-node.is-action:focus-visible {
    outline: none;
    border-color: rgba(118,185,0,.8);
    background: linear-gradient(90deg, rgba(118,185,0,.10), transparent 84%);
    transform: translateX(3px);
  }

  .bp-column-right .bp-node.is-action:hover,
  .bp-column-right .bp-node.is-action:focus-visible {
    background: linear-gradient(270deg, rgba(118,185,0,.10), transparent 84%);
    transform: translateX(-3px);
  }

  .bp-node::after {
    content: '';
    position: absolute;
    top: 50%;
    left: 100%;
    width: clamp(26px, 3.6vw, 58px);
    height: 1px;
    background: linear-gradient(90deg, rgba(118,185,0,.42), rgba(118,185,0,.06));
    pointer-events: none;
  }

  .bp-column-right .bp-node::after {
    left: auto;
    right: 100%;
    background: linear-gradient(270deg, rgba(118,185,0,.42), rgba(118,185,0,.06));
  }

  .bp-node::before {
    content: '';
    position: absolute;
    top: calc(50% - 3px);
    left: calc(100% + clamp(23px, 3.3vw, 53px));
    width: 6px;
    height: 6px;
    border: 1px solid rgba(118,185,0,.72);
    border-radius: 50%;
    background: #09100c;
    box-shadow: 0 0 0 3px rgba(118,185,0,.035);
    pointer-events: none;
  }

  .bp-column-right .bp-node::before {
    left: auto;
    right: calc(100% + clamp(23px, 3.3vw, 53px));
  }

  .bp-node-type {
    display: block;
    margin-bottom: 2px;
    color: rgba(118,185,0,.72);
    font-size: 8px;
    font-weight: 760;
    letter-spacing: .12em;
    text-transform: uppercase;
  }

  .bp-node strong {
    display: block;
    font-family: "Didot Bold", Didot, "Bodoni 72", Georgia, serif;
    font-size: 15px;
    line-height: 1.05;
    font-weight: 700;
  }

  .bp-node small {
    display: block;
    margin-top: 4px;
    color: rgba(222,228,223,.48);
    font-family: Inter, ui-sans-serif, system-ui, sans-serif;
    font-size: 9px;
    line-height: 1.25;
  }

  .bp-core {
    position: relative;
    min-height: 500px;
    overflow: hidden;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 24px;
    background:
      linear-gradient(180deg, rgba(7,14,10,.06), rgba(7,14,10,.72)),
      url("assets/delivery-robot-hero.png") 50% 62% / cover no-repeat;
    box-shadow: inset 0 1px 0 rgba(255,255,255,.025), 0 28px 70px rgba(0,0,0,.18);
  }

  .bp-core::before {
    content: '';
    position: absolute;
    inset: 0;
    background:
      linear-gradient(90deg, rgba(5,10,7,.50), transparent 24%, transparent 76%, rgba(5,10,7,.50)),
      radial-gradient(circle at 50% 55%, transparent 34%, rgba(5,10,7,.34) 76%);
    pointer-events: none;
  }

  .bp-core::after {
    content: '';
    position: absolute;
    inset: 16px;
    border: 1px solid rgba(118,185,0,.13);
    border-radius: 18px;
    pointer-events: none;
  }

  .bp-core-top {
    position: absolute;
    inset: 18px 20px auto;
    z-index: 2;
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 16px;
  }

  .bp-core-top span,
  .bp-core-bottom span {
    padding: 5px 8px;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 999px;
    background: rgba(7,12,9,.55);
    backdrop-filter: blur(10px);
    color: rgba(231,235,231,.67);
    font-size: 9px;
    letter-spacing: .08em;
  }

  .bp-core-top strong {
    font-family: "Didot Bold", Didot, "Bodoni 72", Georgia, serif;
    font-size: 17px;
    font-weight: 700;
  }

  .bp-core-bottom {
    position: absolute;
    inset: auto 20px 18px;
    z-index: 2;
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 7px;
  }

  .bp-core-center {
    position: absolute;
    left: 50%;
    top: 50%;
    z-index: 2;
    width: 108px;
    height: 108px;
    transform: translate(-50%, -50%);
    border: 1px solid rgba(118,185,0,.35);
    border-radius: 50%;
    box-shadow: 0 0 0 18px rgba(118,185,0,.025), 0 0 60px rgba(118,185,0,.08);
    pointer-events: none;
  }

  .bp-core-center::before,
  .bp-core-center::after {
    content: '';
    position: absolute;
    background: rgba(118,185,0,.30);
  }

  .bp-core-center::before { width: 1px; height: 142px; left: 53px; top: -18px; }
  .bp-core-center::after { height: 1px; width: 142px; top: 53px; left: -18px; }

  .bp-hint {
    position: absolute;
    left: 50%;
    top: calc(50% + 68px);
    z-index: 3;
    transform: translateX(-50%);
    color: rgba(237,241,237,.53);
    font-size: 9px;
    letter-spacing: .11em;
    text-transform: uppercase;
    white-space: nowrap;
  }

  .story-panel-move .move-route-lab {
    margin-top: 28px;
  }

  @media (max-width: 1080px) {
    .app-view-system .system-view-frame { width: min(100% - 38px, 1180px); }
    .bp-layout { grid-template-columns: minmax(150px,.72fr) minmax(320px,1.35fr) minmax(150px,.72fr); gap: 20px; }
    .bp-node::after { width: 24px; }
    .bp-node::before { left: calc(100% + 20px); }
    .bp-column-right .bp-node::before { right: calc(100% + 20px); }
  }

  @media (max-width: 820px) {
    .bp-context { grid-template-columns: 1fr; }
    .bp-context p { text-align: left; max-width: 620px; }
    .bp-layout { grid-template-columns: 1fr; min-height: 0; }
    .bp-core { min-height: 360px; grid-row: 1; }
    .bp-column { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); }
    .bp-node, .bp-column-right .bp-node { text-align: left; border: 1px solid rgba(241,243,240,.09); background: rgba(241,243,240,.025); }
    .bp-node::before, .bp-node::after { display: none; }
    .app-view-system .system-workspace { grid-template-columns: 1fr; }
  }
</style>
<script>
(() => {
  const defs = {
    order: {
      index: '01', title: 'Order', question: 'How does a request become a physical delivery task?',
      note: 'Interaction is only useful when voice, web, route preview and task state converge on the same mission.',
      metrics: ['Voice + Web', 'Route preview', 'Live task state'],
      modules: [
        ['Web Order UI','Real browser flow','video','video:web_order_ui.mp4'],
        ['Voice input','voiceGateway.js','code','code:voice'],
        ['Pickup selection','Origin node','interaction',''],
        ['Destination','Drop-off node','interaction',''],
        ['Route preview','Weighted path','planning','code:routePlanner'],
        ['Order lifecycle','Pickup → Drop-off','code','code:orderLifecycle'],
        ['Dispatch queue','Persistent task','backend','code:scheduler'],
        ['Live status','Task feedback','system','']
      ]
    },
    cargo: {
      index: '02', title: 'Cargo', question: 'How does the robot know that a real package was loaded and handed off?',
      note: 'Cargo handling is a physical interaction loop, not just a navigation state.',
      metrics: ['Load sensing', 'Cargo bay', 'User handoff'],
      modules: [
        ['Load cell','Real measurement','video','video:weight.mp4'],
        ['Payload state','Load / unload evidence','hardware',''],
        ['Cargo bay','Physical compartment','hardware',''],
        ['On-car display','Pickup / handoff prompt','interaction','code:handoff'],
        ['Voice prompt','User guidance','interaction','code:voice'],
        ['Arrival handling','Mission state','code','code:handoff'],
        ['Frame & shell','Protected enclosure','hardware',''],
        ['Weight threshold','Control input','control','']
      ]
    },
    move: {
      index: '03', title: 'Move', question: 'How does one vehicle adapt when the road, payload and required maneuver all change?',
      note: 'Perception, payload sensing, planning and low-level control meet on the same physical platform.',
      metrics: ['5 ms control', 'Tag16h5', 'Weight-aware motion'],
      modules: [
        ['Camera','Road perception','code','code:perception'],
        ['AprilTag','Physical node localization','code','code:perception'],
        ['Load cell','Payload measurement','video','video:weight.mp4'],
        ['Turn profile','Weight-adaptive tuning','control',''],
        ['Turn Around','Dedicated U-turn','video','video:turnaround.mp4'],
        ['Obstacle','Real blocked-road test','video','video:obstacle.mp4'],
        ['Dynamic rerouting','Campus graph','map','map:route'],
        ['Navigation FSM','Follow / Turn / Straight','control',''],
        ['5 ms control','Motor loop','code','code:safety'],
        ['Steering','Direction execution','hardware',''],
        ['Motor drive','PWM output','hardware',''],
        ['Frame & shell','Mechanical platform','hardware','']
      ]
    },
    scale: {
      index: '04', title: 'Scale', question: 'How does one robot coordinate several orders instead of replaying isolated demos?',
      note: 'Scheduling, route cost, capacity and network transport turn the car into a delivery service.',
      metrics: ['Capacity check', 'Weighted graph', 'NDJSON gateway'],
      modules: [
        ['Dispatch scheduler','Stop sequence','code','code:scheduler'],
        ['Capacity check','Feasibility gate','backend','code:scheduler'],
        ['Stop insertion','Best insertion','backend','code:scheduler'],
        ['Detour cost','Schedule penalty','planning','code:scheduler'],
        ['Weighted graph','Distance + penalties','code','code:routePlanner'],
        ['Blocked edges','Dynamic cost','planning','code:routePlanner'],
        ['PC ↔ car gateway','Command / event','code','code:gateway'],
        ['Heartbeat','Connection health','network','code:gateway']
      ]
    },
    trust: {
      index: '05', title: 'Trust', question: 'What physical state should the robot enter when communication or control fails?',
      note: 'Safety is designed as a convergent stopped state, followed by controlled synchronization and resume.',
      metrics: ['Safety inhibit', 'Watchdog', 'State sync'],
      modules: [
        ['Link loss','Connection failure','code','code:sync'],
        ['Watchdog','Stale-command guard','code','code:safety'],
        ['Safety inhibit','Motion gate','code','code:safety'],
        ['PWM zero','Physical stop','control','code:safety'],
        ['Emergency stop','Immediate halt','safety','code:safety'],
        ['Motion permission','Resume gate','safety','code:safety'],
        ['State sync','Authoritative recovery','code','code:sync'],
        ['Reconnect','Controlled resume','network','code:sync']
      ]
    },
    system: {
      index: '06', title: 'System', question: 'How do all of these modules become one closed delivery loop?',
      note: 'The final view exposes the full stack from human intent to a verified physical handoff.',
      metrics: ['Human → PC → Car', 'Feedback loop', 'End-to-end demo'],
      modules: [
        ['Voice','Natural request','code','code:voice'],
        ['Web UI','Order interface','video','video:web_order_ui.mp4'],
        ['Scheduler','Multi-order coordination','code','code:scheduler'],
        ['Route planner','Weighted graph','code','code:routePlanner'],
        ['Gateway','PC ↔ vehicle','code','code:gateway'],
        ['Camera','Perception','code','code:perception'],
        ['AprilTag','Localization','code','code:perception'],
        ['Load cell','Cargo evidence','video','video:weight.mp4'],
        ['Navigation','Mission execution','map','map:route'],
        ['Control','5 ms motor loop','code','code:safety'],
        ['Safety','Stop / sync / resume','code','code:sync'],
        ['Frame','Physical product','hardware','']
      ]
    }
  };

  const esc = (s) => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

  const renderNode = (m) => {
    const [name, meta, type, action] = m;
    const cls = action ? 'bp-node is-action' : 'bp-node';
    const attrs = action ? ` data-bp-action="${esc(action)}" aria-label="Open ${esc(name)}"` : ' aria-disabled="true"';
    return `<button class="${cls}" type="button"${attrs}><span class="bp-node-type">${esc(type)}</span><strong>${esc(name)}</strong><small>${esc(meta)}</small></button>`;
  };

  Object.entries(defs).forEach(([key, def]) => {
    const panel = document.querySelector(`.story-panel[data-panel="${key}"]`);
    if (!panel || panel.querySelector('.robot-blueprint')) return;
    const mid = Math.ceil(def.modules.length / 2);
    const left = def.modules.slice(0, mid).map(renderNode).join('');
    const right = def.modules.slice(mid).map(renderNode).join('');
    const metrics = def.metrics.map(x => `<span>${esc(x)}</span>`).join('');
    const stage = document.createElement('section');
    stage.className = `robot-blueprint robot-blueprint-${key}`;
    stage.innerHTML = `
      <div class="bp-context">
        <div class="bp-context-copy"><span class="bp-kicker">${esc(def.index)} / ${esc(def.title)}</span><h3>${esc(def.question)}</h3></div>
        <p>${esc(def.note)}</p>
      </div>
      <div class="bp-layout">
        <div class="bp-column bp-column-left">${left}</div>
        <div class="bp-core" aria-label="THU Delivery robot blueprint">
          <div class="bp-core-top"><strong>THU Delivery</strong><span>${esc(def.title)} layer</span></div>
          <div class="bp-core-center"></div>
          <div class="bp-hint">Select a linked module</div>
          <div class="bp-core-bottom">${metrics}</div>
        </div>
        <div class="bp-column bp-column-right">${right}</div>
      </div>`;
    panel.insertBefore(stage, panel.firstChild);
  });

  const fireExisting = (selector) => {
    const el = document.querySelector(selector);
    if (el) { el.click(); return true; }
    return false;
  };

  document.addEventListener('click', (event) => {
    const node = event.target.closest('.bp-node[data-bp-action]');
    if (!node) return;
    const action = node.dataset.bpAction || '';
    if (action.startsWith('video:')) {
      const file = action.slice(6);
      const trigger = [...document.querySelectorAll('.feature-video-trigger')]
        .find(el => (el.dataset.video || '').endsWith(file));
      if (trigger) trigger.click();
      return;
    }
    if (action.startsWith('code:')) {
      const key = action.slice(5);
      fireExisting(`.feature-code-trigger[data-code-key="${CSS.escape(key)}"]`);
      return;
    }
    if (action === 'map:route') {
      fireExisting('.feature-route-trigger[data-target="moveRouteLab"]');
    }
  });
})();
</script>
<!-- ROBOT BLUEPRINT TRIAL v1 END -->
'''

if '</body>' not in text:
    raise SystemExit('Could not find </body> in docs/index.html')
text = text.replace('</body>', block + '\n</body>', 1)
path.write_text(text, encoding='utf-8')
