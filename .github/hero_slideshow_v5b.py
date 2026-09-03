from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')
marker = '    /* Hero slideshow v5b — runtime mounted Pexels scenes */'
if marker in text:
    raise SystemExit('already applied')

css = r'''

    /* Hero slideshow v5b — runtime mounted Pexels scenes */
    .hero {
      background: #07100d;
    }

    .hero::before {
      inset: 0;
      z-index: -3;
      background:
        radial-gradient(ellipse 52% 66% at 50% 46%, rgba(3,8,6,.46) 0%, rgba(3,8,6,.24) 46%, rgba(3,8,6,.05) 70%, transparent 84%),
        linear-gradient(180deg, rgba(4,8,6,.14) 0%, rgba(4,8,6,.02) 48%, rgba(7,16,13,.46) 100%);
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
      animation: heroSceneCrossfadeV5b 20s ease-in-out infinite;
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

    @keyframes heroSceneCrossfadeV5b {
      0% { opacity: 0; transform: scale(1.045) translate3d(-1.2%,0,0); }
      8% { opacity: 1; }
      42% { opacity: 1; }
      50% { opacity: 0; transform: scale(1.105) translate3d(1.2%,-1%,0); }
      100% { opacity: 0; transform: scale(1.105) translate3d(1.2%,-1%,0); }
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
      width: min(900px,118vw);
      height: 620px;
      transform: translate(-50%,-50%);
      background: radial-gradient(ellipse at center, rgba(3,8,6,.32), rgba(3,8,6,.14) 45%, transparent 72%);
      filter: blur(14px);
      pointer-events: none;
    }

    .hero-title-cn {
      color: #76b900;
      text-shadow:
        0 13px 38px rgba(0,0,0,.50),
        0 0 26px rgba(118,185,0,.10);
    }

    .hero-title-glyph-a,
    .hero-title-glyph-b { color: inherit; }

    .hero-title-mark:hover .hero-title-glyph-a,
    .hero-title-mark:hover .hero-title-glyph-b { color: #91d121; }

    .hero-title-en { margin-top: 34px; }
    .hero-tagline { margin-top: 40px; }
    .hero-cta { margin-top: 28px; }

    .mission-card {
      bottom: 54px;
      background: rgba(5,12,9,.22);
      border-color: rgba(235,241,237,.11);
    }

    @media (max-width: 860px) {
      .hero-bg-a { background-position: 52% 50%; }
      .hero-bg-b { background-position: 56% 48%; }
      .hero-copy::before { width: 130vw; height: 560px; opacity: .92; }
      .hero-title-en { margin-top: 27px; }
      .hero-tagline { margin-top: 34px; }
      .hero-cta { margin-top: 25px; }
    }

    @media (prefers-reduced-motion: reduce) {
      .hero-bg { animation: none !important; transform: none !important; }
      .hero-bg-a { opacity: 1; }
      .hero-bg-b { opacity: 0; }
    }
'''

mount = r'''
<script>
(() => {
  const hero = document.querySelector('.hero');
  if (!hero || hero.querySelector('.hero-visuals')) return;
  const visuals = document.createElement('div');
  visuals.className = 'hero-visuals';
  visuals.setAttribute('aria-hidden', 'true');
  visuals.innerHTML = '<span class="hero-bg hero-bg-a"></span><span class="hero-bg hero-bg-b"></span>';
  hero.prepend(visuals);
})();
</script>
'''

if '  </style>' not in text or '</body>' not in text:
    raise SystemExit('style/body anchors not found')
text = text.replace('  </style>', css + '\n  </style>', 1)
text = text.replace('</body>', mount + '\n</body>', 1)
path.write_text(text, encoding='utf-8')
