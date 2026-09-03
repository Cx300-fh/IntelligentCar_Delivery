from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')


def replace_once(old, new, label):
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{label}: expected exactly 1 match, got {count}')
    text = text.replace(old, new, 1)


replace_once('''    .hero-grid {
      display: grid;
      grid-template-columns: minmax(0, .92fr) minmax(290px, .42fr);
      gap: 48px;
      align-items: end;
      transform: translateY(64px);
    }''','''    .hero-grid {
      display: grid;
      grid-template-columns: minmax(0, 1.02fr) minmax(300px, .32fr);
      gap: 64px;
      align-items: center;
      transform: translateY(52px);
    }''','hero grid')

replace_once('''    .mission-card {
      width: min(100%, 360px);
      justify-self: end;
      align-self: center;
      transform: translate(-10px, 54px);
      border: 1px solid rgba(241,243,240,.14);
      border-radius: 18px;
      background:
        linear-gradient(145deg, rgba(24,28,25,.86), rgba(13,16,14,.72));
      backdrop-filter: blur(22px);
      box-shadow:
        0 24px 80px rgba(0,0,0,.34),
        inset 0 1px 0 rgba(255,255,255,.035);
      padding: 20px 20px 18px;
      overflow: hidden;
      position: relative;
    }''','''    .mission-card {
      width: min(100%, 340px);
      justify-self: end;
      align-self: center;
      transform: translate(-16px, 48px);
      border: 1px solid rgba(241,243,240,.14);
      border-radius: 18px;
      background:
        linear-gradient(145deg, rgba(24,28,25,.86), rgba(13,16,14,.72));
      backdrop-filter: blur(22px);
      box-shadow:
        0 24px 80px rgba(0,0,0,.34),
        inset 0 1px 0 rgba(255,255,255,.035);
      padding: 20px 22px 20px;
      overflow: hidden;
      position: relative;
    }''','mission card base')

replace_once('''    .route-strip {
      position: relative;
      height: 92px;
      margin: 16px 0 0;
    }''','''    .route-strip {
      position: relative;
      height: 106px;
      margin: 0;
    }''','route strip')

replace_once('''      left: 15px;
      right: 15px;
      top: 38px;''','''      left: 10%;
      right: 10%;
      top: 38px;''','route base line')

replace_once('''      left: 15px;
      top: 38px;
      height: 2px;
      width: 72%;''','''      left: 10%;
      top: 38px;
      height: 2px;
      width: 40%;''','route progress line')

replace_once('''    .stop:nth-child(2) { left: 8%; }
    .stop:nth-child(3) { left: 43%; }
    .stop:nth-child(4) { left: 78%; }''','''    .stop:nth-child(2) { left: 10%; }
    .stop:nth-child(3) { left: 50%; }
    .stop:nth-child(4) { left: 90%; }''','route stop positions')

replace_once('''    /* v29 — cleaner brand lockup spacing */
    .hero-title-mark {
      gap: 14px;
      margin-top: -78px;
    }

    .hero-title-cn {
      top: -30px;
      left: 14px;
      gap: .25em;
      font-size: clamp(88px, 11.3vw, 156px);
      line-height: .76;
    }

    .hero-title-en {
      margin-top: 12px;
      line-height: .88;
    }

    @media (max-width: 760px) {
      .hero-title-mark {
        gap: 9px;
        margin-top: -18px;
      }

      .hero-title-cn {
        top: -8px;
        left: 0;
        gap: .20em;
      }

      .hero-title-en {
        margin-top: 8px;
      }
    }''','''    /* v29 — cleaner brand lockup spacing */
    .hero-title-mark {
      gap: 18px;
      margin-top: -52px;
    }

    .hero-title-cn {
      top: -24px;
      left: 14px;
      gap: .25em;
      font-size: clamp(92px, 11.6vw, 160px);
      line-height: .76;
      color: #b7dc70;
    }

    .hero-title-en {
      margin-top: 16px;
      font-size: clamp(46px, 5.9vw, 84px);
      line-height: .88;
    }

    @media (max-width: 760px) {
      .hero-title-mark {
        gap: 13px;
        margin-top: -18px;
      }

      .hero-title-cn {
        top: -8px;
        left: 0;
        gap: .20em;
      }

      .hero-title-en {
        margin-top: 12px;
      }
    }''','final title spacing')

replace_once('''    .hero-title-mark::after {
      content: "AUTONOMOUS CAMPUS LOGISTICS";''','''    .hero-title-mark::after {
      content: "";
      display: none;''','remove hover side label')

replace_once('''          calc(-10px + var(--scroll-card-x) + var(--drag-x)),
          calc(54px + var(--scroll-card-y) + var(--drag-y)),''','''          calc(-16px + var(--scroll-card-x) + var(--drag-x)),
          calc(48px + var(--scroll-card-y) + var(--drag-y)),''','mission transform')

replace_once('''    .mission-top[data-drag-handle="true"] {
      cursor: grab;
      user-select: none;
    }

    .mission-card.is-dragging .mission-top[data-drag-handle="true"] {
      cursor: grabbing;
    }''','''    .route-strip[data-drag-handle="true"] {
      cursor: grab;
      user-select: none;
    }

    .mission-card:hover {
      border-color: rgba(177,235,79,.30);
      box-shadow:
        0 28px 88px rgba(0,0,0,.38),
        0 0 0 1px rgba(118,185,0,.05),
        inset 0 1px 0 rgba(255,255,255,.04);
    }

    .mission-card.is-dragging .route-strip[data-drag-handle="true"] {
      cursor: grabbing;
    }''','drag handle and hover')

replace_once('''        <div class="hero-actions">
          <a class="btn primary" href="#map">Watch Demo</a>
          <a class="btn" href="#system">Explore System</a>
          <a class="btn" href="https://github.com/Cx300-fh/IntelligentCar_Delivery">GitHub</a>
        </div>
        <div class="hero-motion-cue" aria-hidden="true">
          <span></span>
          LIVE CAMPUS ROUTING
        </div>''','''''','remove hero buttons and cue')

replace_once('''      <aside class="mission-card" aria-label="Live mission preview">
        <div class="mission-top" data-drag-handle="true">
          <span class="mission-title">Live mission</span>
          <span class="mission-drag-hint" aria-hidden="true"><i></i><i></i><i></i></span>
          <span class="status-pill">Route adaptive</span>
        </div>
        <div class="route-strip" aria-hidden="true">''','''      <aside class="mission-card" aria-label="Route progress preview">
        <div class="route-strip" data-drag-handle="true" aria-hidden="true">''','remove mission header')

replace_once('''            if (target.closest(".mission-card")) return "DRAG";
            if (target.closest(".demo-video-frame")) return "VIEW";''','''            if (target.closest("#heroBrandTitle")) return "MOVE";
            if (target.closest(".mission-card")) return "DRAG";
            if (target.closest(".demo-video-frame")) return "VIEW";''','cursor title label')

replace_once('''        title.style.setProperty("--brand-x", `${(nx * 8).toFixed(2)}px`);
        title.style.setProperty("--brand-y", `${(ny * 5).toFixed(2)}px`);
        title.style.setProperty("--brand-rx", `${(nx * .8).toFixed(2)}deg`);''','''        title.style.setProperty("--brand-x", `${(nx * 10).toFixed(2)}px`);
        title.style.setProperty("--brand-y", `${(ny * 6).toFixed(2)}px`);
        title.style.setProperty("--brand-rx", `${(nx * 1.0).toFixed(2)}deg`);''','title pointer response')

path.write_text(text, encoding='utf-8')
print('hero refinement applied')
