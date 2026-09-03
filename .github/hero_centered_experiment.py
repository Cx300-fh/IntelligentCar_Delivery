from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')
marker = '    /* Hero centered experiment v4 */'
if marker in text:
    raise SystemExit('already applied')

css = r'''

    /* Hero centered experiment v4 */
    .hero {
      min-height: 100svh;
      padding: 88px 0 28px;
      align-items: center;
      background: #07100d;
    }

    .hero::before {
      inset: 0;
      background:
        linear-gradient(180deg, rgba(5,10,8,.18) 0%, rgba(5,10,8,.06) 48%, rgba(7,16,13,.58) 100%),
        radial-gradient(ellipse 48% 68% at 50% 47%, rgba(4,9,7,.54) 0%, rgba(4,9,7,.26) 48%, rgba(4,9,7,.04) 76%, transparent 100%),
        url("assets/delivery-robot-hero.png") 58% center / cover no-repeat;
      transform:
        translate3d(var(--hero-x), var(--hero-y), 0)
        scale(1.025);
      opacity: 1;
    }

    .hero::after {
      height: 11%;
      background: linear-gradient(180deg, transparent, #07100d 96%);
    }

    .hero > .shell {
      width: min(1320px, calc(100% - 96px));
    }

    .hero-grid {
      position: relative;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: calc(100svh - 116px);
      gap: 0;
      text-align: center;
    }

    .hero-copy {
      width: min(100%, 760px);
      max-width: 760px;
      display: flex;
      flex-direction: column;
      align-items: center;
      transform: translateY(-34px);
      text-align: center;
    }

    .hero-title-mark {
      align-items: center;
      gap: 22px;
      margin: 0;
      text-align: center;
    }

    .hero-title-cn {
      justify-content: center;
      margin: 0;
      font-size: clamp(92px, 10.8vw, 150px);
      color: #dfeabf;
      transform-origin: center bottom;
      text-shadow:
        0 14px 42px rgba(0,0,0,.42),
        0 0 30px rgba(216,234,185,.07);
    }

    .hero-title-en {
      justify-content: center;
      margin-top: 18px;
      font-size: clamp(36px, 4.4vw, 60px);
      transform-origin: center center;
    }

    .hero-title-en::after {
      left: 50%;
      transform: translateX(-50%);
      width: 34%;
    }

    .hero-title-mark::before {
      display: none;
    }

    .hero-tagline {
      margin: 30px 0 0;
      color: rgba(241,245,242,.88);
      font-size: clamp(17px, 1.4vw, 20px);
      letter-spacing: .08em;
      text-shadow: 0 2px 18px rgba(0,0,0,.54);
    }

    .hero-cta {
      justify-content: center;
      margin-top: 24px;
      gap: 10px;
    }

    .hero-cta .btn {
      min-width: 142px;
      min-height: 46px;
      padding: 0 20px;
    }

    .mission-card {
      position: absolute !important;
      left: 50%;
      right: auto;
      bottom: 70px;
      width: 226px;
      margin: 0;
      padding: 8px 12px 7px;
      transform:
        translate3d(
          calc(-50% + var(--scroll-card-x) + var(--drag-x)),
          calc(var(--scroll-card-y) + var(--drag-y)),
          0
        )
        rotate(var(--drag-r))
        scale(var(--drag-scale));
      border-color: rgba(233,240,235,.10);
      background: rgba(6,13,10,.24);
      backdrop-filter: blur(8px) saturate(.9);
      -webkit-backdrop-filter: blur(8px) saturate(.9);
      box-shadow: 0 8px 24px rgba(0,0,0,.14);
    }

    .mission-card.is-dragging {
      transform:
        translate3d(
          calc(-50% + var(--scroll-card-x) + var(--drag-x)),
          calc(var(--scroll-card-y) + var(--drag-y)),
          0
        )
        rotate(var(--drag-r))
        scale(var(--drag-scale));
    }

    .route-strip {
      height: 52px;
    }

    .route-strip::before,
    .route-progress {
      top: 20px;
    }

    .stop {
      top: 15px;
    }

    .hero-morph-shape {
      right: 50%;
      top: 20%;
      width: min(50vw, 680px);
      transform:
        translate3d(calc(50% + var(--morph-x)), var(--morph-y), 0)
        rotate(var(--morph-rotate))
        scale(var(--morph-scale-x), var(--morph-scale-y));
      opacity: .055;
    }

    .hero-scroll-hint {
      bottom: 18px;
      opacity: calc(.72 - var(--hero-scroll));
    }

    @media (max-width: 860px) {
      .hero {
        min-height: 100svh;
      }

      .hero::before {
        background:
          linear-gradient(180deg, rgba(5,10,8,.16), rgba(5,10,8,.28) 52%, #07100d 100%),
          radial-gradient(ellipse 72% 60% at 50% 43%, rgba(4,9,7,.52), rgba(4,9,7,.12) 68%, transparent 100%),
          url("assets/delivery-robot-hero.png") 62% top / cover no-repeat;
      }

      .hero > .shell {
        width: min(100% - 34px, 1180px);
      }

      .hero-grid {
        min-height: calc(100svh - 102px);
      }

      .hero-copy {
        transform: translateY(-18px);
      }

      .hero-title-cn {
        font-size: clamp(70px, 22vw, 104px);
      }

      .hero-title-en {
        margin-top: 15px;
        font-size: clamp(31px, 10vw, 46px);
      }

      .mission-card {
        left: 50%;
        right: auto;
        bottom: 50px;
        width: min(220px, calc(100% - 28px));
        margin: 0;
      }

      .hero-morph-shape {
        right: 50%;
        top: 25%;
        width: 82vw;
      }
    }

    @media (max-width: 560px) {
      .hero-copy {
        transform: translateY(-8px);
      }

      .hero-title-mark {
        gap: 17px;
      }

      .hero-title-cn {
        font-size: clamp(64px, 21vw, 90px);
      }

      .hero-title-en {
        margin-top: 13px;
        font-size: clamp(29px, 9vw, 40px);
      }

      .hero-tagline {
        margin-top: 24px;
      }

      .hero-cta {
        width: min(100%, 340px);
      }

      .hero-cta .btn {
        width: 100%;
      }

      .mission-card {
        bottom: 42px;
      }
    }
'''

text = text.replace('  </style>', css + '\n  </style>', 1)
path.write_text(text, encoding='utf-8')
