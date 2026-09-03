from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')
marker = 'Overview root full-bleed background v2'
if marker in text:
    raise SystemExit('already patched')

css = r'''

    /* Overview root full-bleed background v2
       The slideshow belongs to the whole Overview view, not the inner Hero. */
    .app-view-overview {
      position: relative;
      isolation: isolate;
      background: #07100d;
    }

    .app-view-overview::before,
    .app-view-overview::after {
      content: "";
      position: absolute;
      inset: 0;
      z-index: 0;
      pointer-events: none;
      background-repeat: no-repeat;
      background-size: cover;
      will-change: opacity, transform;
      animation: overviewRootScene 20s ease-in-out infinite;
    }

    .app-view-overview::before {
      background-image:
        radial-gradient(ellipse 52% 66% at 50% 46%, rgba(3,8,6,.46) 0%, rgba(3,8,6,.24) 46%, rgba(3,8,6,.05) 70%, transparent 84%),
        linear-gradient(180deg, rgba(4,8,6,.14) 0%, rgba(4,8,6,.02) 48%, rgba(7,16,13,.46) 100%),
        url("https://images.pexels.com/photos/8566566/pexels-photo-8566566.jpeg?cs=srgb&dl=pexels-kindelmedia-8566566.jpg&fm=jpg");
      background-position: center, center, 50% 52%;
      animation-delay: 0s;
    }

    .app-view-overview::after {
      background-image:
        radial-gradient(ellipse 52% 66% at 50% 46%, rgba(3,8,6,.46) 0%, rgba(3,8,6,.24) 46%, rgba(3,8,6,.05) 70%, transparent 84%),
        linear-gradient(180deg, rgba(4,8,6,.14) 0%, rgba(4,8,6,.02) 48%, rgba(7,16,13,.46) 100%),
        url("https://images.pexels.com/photos/8566632/pexels-photo-8566632.jpeg?cs=srgb&dl=pexels-kindelmedia-8566632.jpg&fm=jpg");
      background-position: center, center, 50% 58%;
      animation-delay: -10s;
    }

    @keyframes overviewRootScene {
      0% { opacity: 0; transform: scale(1.025) translate3d(-.6%,0,0); }
      8% { opacity: 1; }
      42% { opacity: 1; }
      50% { opacity: 0; transform: scale(1.065) translate3d(.6%,-.4%,0); }
      100% { opacity: 0; transform: scale(1.065) translate3d(.6%,-.4%,0); }
    }

    .app-view-overview .hero {
      position: relative;
      z-index: 2;
      background: transparent !important;
    }

    .app-view-overview .hero-visuals {
      display: none !important;
    }

    .app-view-overview .hero::before {
      z-index: -1;
      background:
        radial-gradient(ellipse 52% 66% at 50% 46%, rgba(3,8,6,.30) 0%, rgba(3,8,6,.12) 48%, transparent 76%) !important;
    }

    .app-view-overview .hero::after {
      z-index: -1;
      height: 14%;
      background: linear-gradient(180deg, transparent, rgba(7,16,13,.34)) !important;
    }

    .app-view-overview .hero-grid,
    .app-view-overview .hero-scroll-hint,
    .app-view-overview .hero-morph-shape {
      position: relative;
      z-index: 3;
    }

    @media (max-width: 860px) {
      .app-view-overview::before { background-position: center, center, 52% 50%; }
      .app-view-overview::after { background-position: center, center, 56% 48%; }
    }

    @media (prefers-reduced-motion: reduce) {
      .app-view-overview::before { opacity: 1; animation: none !important; transform: none !important; }
      .app-view-overview::after { opacity: 0; animation: none !important; transform: none !important; }
    }
'''

needle = '\n  </style>\n</head>'
if needle not in text:
    raise SystemExit('style closing marker not found')
text = text.replace(needle, css + needle, 1)
path.write_text(text, encoding='utf-8')
