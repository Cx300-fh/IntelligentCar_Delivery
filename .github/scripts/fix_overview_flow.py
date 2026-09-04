from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

old = '''    .app-view-overview .hero-grid,
    .app-view-overview .hero-scroll-hint,
    .app-view-overview .hero-morph-shape {
      position: relative;
      z-index: 3;
    }
'''

new = '''    .app-view-overview .hero-grid {
      position: relative;
      z-index: 3;
    }

    .app-view-overview .hero-scroll-hint {
      position: absolute !important;
      left: 50%;
      bottom: 18px;
      z-index: 4;
      transform: translateX(-50%);
    }

    .app-view-overview .hero-morph-shape {
      position: absolute !important;
      z-index: 1;
    }

    .app-view-overview .hero::after {
      display: none !important;
    }
'''

if old not in text:
    raise SystemExit('target conflicting Overview block not found')

text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
