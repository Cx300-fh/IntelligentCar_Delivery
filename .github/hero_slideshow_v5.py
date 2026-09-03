from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')
marker = '    /* Hero slideshow v5 — Pexels scenes + centered hierarchy */'
if marker in text:
    raise SystemExit('already applied')

# Insert the visual slideshow layers inside the hero without touching functional content.
hero_pattern = re.compile(r'(<section[^>]*class="[^"]*\bhero\b[^"]*"[^>]*>)', re.I)
m = hero_pattern.search(text)
if not m:
    raise SystemExit('hero section not found')
visuals = '''\n    <div class="hero-visuals" aria-hidden="true">\n      <span class="hero-bg hero-bg-a"></span>\n      <span class="hero-bg hero-bg-b"></span>\n    </div>'''
text = text[:m.end()] + visuals + text[m.end():]

css = r'''

    /* Hero slideshow v5 — Pexels scenes + centered hierarchy */
    .hero {
      background: #07100d;
    }

    /* Existing pseudo layer becomes only a readability wash; no project-demo image here. */
    .hero::before {
      inset: 0;
      z-index: -3;
      background:
        radial-gradient(ellipse 52% 66% at 50% 46%, rgba(3,8,6,.48) 0%, rgba(3,8,6,.26) 46%, rgba(3,8,6,.06) 70%, transparent 84%),
        linear-gradient(180deg, rgba(4,8,6,.16) 0%, rgba(4,8,6,.03) 48%, rgba(7,16,13,.48) 100%);
      transform: none !important;
      animation: none !important;
      filter: none !important;
    }

    .hero-visuals {
      position: absolute;
      inset: 0;
      z-index: -4;
      overflow: hidden;
      background: #07100d;
      pointer-events: none;
    }

    .hero-bg {
      position: absolute;
      inset: -3%;
      opacity: 0;
      background-repeat: no-repeat;
      background-size: cover;
      will-change: transform, opacity;
      animation: heroSceneCrossfade 20s ease-in-out infinite;
    }

    .hero-bg-a {
      background-image: url("https://images.pexels.com/photos/8566566/pexels-photo-8566566.jpeg?cs=srgb&dl=pexels-kindelmedia-8566566.jpg&fm=jpg");
      background-position: 50% 52%;
      animation-delay: 0s;
    }

    .hero-bg-b {
      background-image: url("https://images.pexels.com/photos/8566632/pexels-photo-8566632.jpeg?cs=srgb&dl=pexels-kindelmedia-8566632.jpg&fm=jpg");
      background-position: 50% 58%;
      animation-delay: -10s;
    }

    @keyframes heroSceneCrossfade {
      0% {
        opacity: 0;
        transform: scale(1.045) translate3d(-1.2%, 0, 0);
      }
      8% { opacity: 1; }
      42% { opacity: 1; }
      50% {
        opacity: 0;
        transform: scale(1.105) translate3d(1.2%, -1%, 0);
      }
      100% {
        opacity: 0;
        transform: scale(1.105) translate3d(1.2%, -1%, 0);
      }
    }

    .hero-copy {
      position: relative;
      z-index: 5;
    }

    .hero-copy::before {
      content: "";
      position: absolute;
      z-index: -1;
      left: 50%;
      top: 46%;
      width: min(900px, 118vw);
      height: 620px;
      transform: translate(-50%, -50%);
      background: radial-gradient(ellipse at center, rgba(3,8,6,.34), rgba(3,8,6,.16) 45%, transparent 72%);
      filter: blur(14px);
      pointer-events: none;
    }

    /* Re-establish the Chinese name as the brand anchor. */
    .hero-title-cn {
      color: #76b900;
      text-shadow:
        0 13px 38px rgba(0,0,0,.50),
        0 0 26px rgba(118,185,0,.10);
    }

    .hero-title-glyph-a,
    .hero-title-glyph-b {
      color: inherit;
    }

    .hero-title-mark:hover .hero-title-glyph-a,
    .hero-title-mark:hover .hero-title-glyph-b {
      color: #91d121;
    }

    /* Move the three rows below 清送 downward without moving the Chinese title itself. */
    .hero-title-en {
      margin-top: 34px;
    }

    .hero-tagline {
      margin-top: 40px;
    }

    .hero-cta {
      margin-top: 28px;
    }

    /* Keep the status layer visually detached from the title stack. */
    .mission-card {
      bottom: 54px;
      background: rgba(5,12,9,.22);
      border-color: rgba(235,241,237,.11);
    }

    @media (max-width: 860px) {
      .hero-bg-a {
        background-position: 52% 50%;
      }

      .hero-bg-b {
        background-position: 56% 48%;
      }

      .hero-copy::before {
        width: 130vw;
        height: 560px;
        opacity: .92;
      }

      .hero-title-en {
        margin-top: 27px;
      }

      .hero-tagline {
        margin-top: 34px;
      }

      .hero-cta {
        margin-top: 25px;
      }
    }

    @media (prefers-reduced-motion: reduce) {
      .hero-bg {
        animation: none !important;
        transform: none !important;
      }

      .hero-bg-a { opacity: 1; }
      .hero-bg-b { opacity: 0; }
    }
'''

text = text.replace('  </style>', css + '\n  </style>', 1)
path.write_text(text, encoding='utf-8')
