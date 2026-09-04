from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

marker = '''    @media (prefers-reduced-motion: reduce) {
      .app-view-overview::before { opacity: 1; animation: none !important; transform: none !important; }
      .app-view-overview::after { opacity: 0; animation: none !important; transform: none !important; }
    }
'''

patch = '''    @media (prefers-reduced-motion: reduce) {
      .app-view-overview::before { opacity: 1; animation: none !important; transform: none !important; }
      .app-view-overview::after { opacity: 0; animation: none !important; transform: none !important; }
    }

    /* Four-view viewport fill v3
       Every top-level tab owns the full viewport below the fixed navigation. */
    .app-view-demo,
    .app-view-system,
    .app-view-contact {
      min-height: 0;
      background:
        radial-gradient(ellipse at 70% 18%, rgba(118,185,0,.045), transparent 42%),
        linear-gradient(180deg, #0b1510 0%, #07100d 100%);
    }

    .app-view-demo > .app-view-scroll,
    .app-view-contact > .app-view-scroll {
      min-height: 100%;
    }

    .app-view-demo .demo-video-section {
      height: 100%;
      min-height: 100%;
      display: grid;
      grid-template-rows: auto minmax(0, 1fr);
      align-content: stretch;
      gap: 24px;
      padding-top: 38px;
      padding-bottom: 28px;
    }

    .app-view-demo .demo-video-head {
      margin: 0;
    }

    .app-view-demo .demo-video-frame {
      height: 100%;
      min-height: 0;
    }

    .app-view-demo .demo-video-frame video {
      width: 100%;
      height: 100%;
      min-height: 0;
      aspect-ratio: auto;
      object-fit: cover;
    }

    .app-view-system .system-view-frame {
      height: 100%;
      min-height: 0;
    }

    .app-view-contact .contact-section {
      min-height: 100%;
      padding-top: 56px;
      padding-bottom: 28px;
    }

    @media (max-width: 900px) {
      .app-view-demo .demo-video-section {
        height: auto;
        min-height: 100%;
        grid-template-rows: auto auto;
        padding-top: 30px;
        padding-bottom: 24px;
      }

      .app-view-demo .demo-video-frame {
        height: auto;
      }

      .app-view-demo .demo-video-frame video {
        height: auto;
        aspect-ratio: 16 / 9;
      }

      .app-view-contact .contact-section {
        min-height: 100%;
        padding-top: 44px;
        padding-bottom: 24px;
      }
    }
'''

if 'Four-view viewport fill v3' in text:
    raise SystemExit('viewport fill patch already present')
if marker not in text:
    raise SystemExit('Overview reduced-motion marker not found')

text = text.replace(marker, patch, 1)
path.write_text(text, encoding='utf-8')
