from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')
marker = 'Overview hero viewport fill fix v1'
if marker in text:
    raise SystemExit('already patched')

css = r'''

    /* Overview hero viewport fill fix v1 */
    .app-view-overview .hero {
      width: 100%;
      height: 100%;
      min-height: 100%;
      padding: 0;
      align-items: stretch;
    }

    .app-view-overview .hero-grid {
      width: min(1320px, calc(100% - 96px));
      height: 100%;
      min-height: 100%;
      padding-top: 24px;
      padding-bottom: 24px;
      box-sizing: border-box;
    }

    .app-view-overview .hero-visuals {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      min-height: 100%;
    }

    .app-view-overview .hero-bg {
      inset: -2px;
      width: auto;
      height: auto;
      background-size: cover;
    }

    .app-view-overview .hero::before {
      inset: 0;
    }

    @media (max-width: 900px) {
      .app-view-overview .hero {
        min-height: 100%;
      }

      .app-view-overview .hero-grid {
        width: min(100% - 34px, 1180px);
        min-height: 100%;
      }
    }
'''

needle = '\n  </style>\n</head>'
if needle not in text:
    raise SystemExit('style closing marker not found')
text = text.replace(needle, css + needle, 1)
path.write_text(text, encoding='utf-8')
