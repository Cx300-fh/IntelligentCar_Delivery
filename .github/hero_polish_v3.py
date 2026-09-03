from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

old = '''          <div class="stop active">Order</div>\n          <div class="stop active">Pickup</div>\n          <div class="stop">Dropoff</div>'''
new = '''          <div class="stop done">Order</div>\n          <div class="stop current">Pickup</div>\n          <div class="stop pending">Dropoff</div>'''
if old not in text:
    raise SystemExit('route stop markup not found')
text = text.replace(old, new, 1)

marker = '    /* Hero polish v3 — scene HUD, hierarchy, CTA balance */'
if marker in text:
    raise SystemExit('hero polish v3 already applied')

css = r'''

    /* Hero polish v3 — scene HUD, hierarchy, CTA balance */
    .hero {
      background: #07100d;
    }

    .hero::before {
      background:
        linear-gradient(90deg, rgba(7,16,13,.96) 0%, rgba(7,16,13,.72) 35%, rgba(7,16,13,.15) 68%, transparent 100%),
        url("assets/delivery-robot-hero.png") center right / cover no-repeat;
    }

    .hero::after {
      height: 10%;
      background: linear-gradient(180deg, transparent, rgba(7,16,13,.88));
    }

    .hero-copy {
      transform: translateY(-64px);
    }

    .hero-title-mark {
      gap: 24px;
    }

    .hero-title-cn {
      font-size: clamp(88px, 10vw, 142px);
      color: #dbe8b9;
      text-shadow:
        0 12px 34px rgba(0,0,0,.40),
        0 0 24px rgba(199,219,158,.08);
    }

    .hero-title-glyph-a,
    .hero-title-glyph-b {
      color: inherit;
    }

    .hero-title-mark:hover .hero-title-glyph-a,
    .hero-title-mark:hover .hero-title-glyph-b {
      color: #edf2d7;
    }

    .hero-title-en {
      margin-top: 20px;
      font-size: clamp(33px, 4vw, 56px);
      opacity: .92 !important;
    }

    .hero-tagline {
      margin-top: 27px;
      color: rgba(232,238,233,.82);
    }

    .hero-cta {
      margin-top: 23px;
      gap: 10px;
    }

    .hero-cta .btn {
      min-height: 46px;
      padding: 0 20px;
      border-radius: 6px;
      font-size: 13px;
      font-weight: 680;
      box-shadow: none;
    }

    .hero-cta .btn.primary {
      border-color: #8ab742;
      background: #8ab742;
      color: #07100d;
    }

    .hero-cta .btn.primary:hover {
      border-color: #9bc653;
      background: #9bc653;
      transform: translateY(-1px);
    }

    .hero-cta .btn:not(.primary) {
      border-color: rgba(235,241,237,.44);
      background: rgba(10,20,16,.34);
      color: rgba(241,245,242,.92);
      backdrop-filter: blur(8px);
    }

    .hero-cta .btn:not(.primary):hover {
      border-color: rgba(243,247,244,.62);
      background: rgba(226,235,229,.10);
      color: #f4f7f5;
      transform: translateY(-1px);
    }

    .mission-card {
      position: absolute !important;
      right: -28px;
      bottom: 6px;
      width: 238px;
      margin: 0;
      padding: 10px 13px 9px;
      border: 1px solid rgba(229,237,231,.12);
      border-radius: 8px;
      background:
        linear-gradient(180deg, rgba(12,21,17,.48), rgba(8,15,12,.28));
      -webkit-backdrop-filter: blur(10px) saturate(.9);
      backdrop-filter: blur(10px) saturate(.9);
      box-shadow: 0 10px 28px rgba(0,0,0,.16);
      animation: none;
      overflow: visible;
    }

    .mission-card::before {
      display: none;
    }

    .mission-card:hover {
      border-color: rgba(231,239,233,.20);
      box-shadow: 0 12px 34px rgba(0,0,0,.18);
    }

    .mission-card.is-dragging {
      --drag-scale: 1.012;
      border-color: rgba(231,239,233,.24);
      box-shadow: 0 16px 42px rgba(0,0,0,.24);
    }

    .route-strip {
      height: 58px;
      margin: 0;
    }

    .route-strip::before {
      left: 9%;
      right: 9%;
      top: 22px;
      height: 1px;
      background: rgba(226,234,228,.20);
    }

    .route-progress {
      left: 9%;
      top: 22px;
      width: 41%;
      height: 1px;
      background: rgba(118,185,0,.58);
      box-shadow: none;
      animation: none;
    }

    .stop {
      top: 17px;
      gap: 9px;
      color: rgba(213,222,216,.58);
      font-size: 9px;
      letter-spacing: .02em;
    }

    .stop::before {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      border: 1px solid rgba(224,233,227,.34);
      background: rgba(8,15,12,.72);
      box-shadow: none;
    }

    .stop.done::before {
      border-color: rgba(170,187,175,.62);
      background: rgba(156,176,162,.62);
    }

    .stop.done {
      color: rgba(193,205,197,.58);
    }

    .stop.current::before {
      border-color: #76b900;
      background: #76b900;
      box-shadow: 0 0 10px rgba(118,185,0,.32);
    }

    .stop.current {
      color: rgba(237,242,238,.88);
    }

    .stop.pending::before {
      border-color: rgba(220,230,223,.38);
      background: rgba(7,16,13,.45);
    }

    .stop:nth-child(2) { left: 9%; }
    .stop:nth-child(3) { left: 50%; }
    .stop:nth-child(4) { left: 91%; }

    @media (max-width: 860px) {
      .hero-copy {
        transform: translateY(-22px);
      }

      .hero-title-mark {
        gap: 19px;
      }

      .hero-title-cn {
        font-size: clamp(70px, 21vw, 96px);
      }

      .hero-title-en {
        margin-top: 16px;
      }

      .mission-card {
        position: relative !important;
        right: auto;
        bottom: auto;
        width: min(238px, 100%);
        margin: 54px 0 0 auto;
      }
    }

    @media (max-width: 560px) {
      .hero-copy {
        transform: translateY(-10px);
      }

      .hero-cta .btn {
        min-height: 46px;
        padding: 0 18px;
      }
    }
'''

text = text.replace('  </style>', css + '\n  </style>', 1)
path.write_text(text, encoding='utf-8')
