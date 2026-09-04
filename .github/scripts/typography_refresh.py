from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

# Required copy from the requested content pass.
required = [
    'Watch Demo',
    'System Design',
    'Intertwined Roles, Unified Vision',
    'System: From Problem To Answer',
]
for item in required:
    if item not in text:
        raise SystemExit(f'missing required copy: {item}')

# Reduce all-caps noise in contextual labels while preserving major Prism titles.
label_replacements = {
    'REAL SCENE / ORDER': 'Real scene / Order',
    'REAL SCENE / CARGO': 'Real scene / Cargo',
    'REAL SCENE / MOVE': 'Real scene / Move',
    'REAL SCENE / SCALE': 'Real scene / Scale',
    'REAL SCENE / TRUST': 'Real scene / Trust',
    'DYNAMIC ROUTING ALGORITHM': 'Dynamic routing algorithm',
    'HUMAN & INTERACTION': 'Human & interaction',
    'PC DISPATCH': 'PC dispatch',
    'DELIVERY CAR': 'Delivery car',
    'FUNCTION DEMO': 'Function demo',
}
for old, new in label_replacements.items():
    if old in text:
        text = text.replace(old, new)

if 'Typography hierarchy v2' in text:
    raise SystemExit('Typography hierarchy v2 already present')

marker = '  </style>\n</head>'
if marker not in text:
    raise SystemExit('style closing marker not found')

css = r'''

    /* ============================================================
       Typography hierarchy v2
       Primary title  -> Prism (unchanged)
       Editorial bold -> Didot Bold
       Editorial note -> Didot Italic
       UI / body      -> neutral sans
       ============================================================ */
    @font-face {
      font-family: "DidotWeb";
      src:
        url("./assets/fonts/Didot-Bold-1.ttf") format("truetype"),
        local("Didot Bold"),
        local("Didot-Bold"),
        local("Didot");
      font-style: normal;
      font-weight: 700;
      font-display: swap;
    }

    @font-face {
      font-family: "DidotWeb";
      src:
        url("./assets/fonts/Didot-Italic-2.ttf") format("truetype"),
        local("Didot Italic"),
        local("Didot-Italic"),
        local("Didot");
      font-style: italic;
      font-weight: 400;
      font-display: swap;
    }

    :root {
      --font-display: "Prism", "Arial Narrow", "Helvetica Neue", Arial, sans-serif;
      --font-editorial: "DidotWeb", "DidotEditorial", Didot, "Bodoni 72", "Bodoni MT", Georgia, serif;
      --font-ui: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    /* Main titles stay exactly in the original display language. */
    h1,
    h2,
    h3,
    .hero-title-en,
    .demo-video-head h2,
    .system-story-head h2,
    .contact-head h2 {
      font-family: var(--font-display) !important;
      font-style: normal !important;
    }

    /* Bold Didot marks the second information tier, never the main title tier. */
    .story-feature h4,
    .system-architecture-v2 h4,
    .role-title,
    .feature-video-title,
    .team-name {
      font-family: var(--font-editorial) !important;
      font-style: normal !important;
      font-weight: 700 !important;
      text-transform: none !important;
      letter-spacing: -.014em;
    }

    .team-name {
      font-size: clamp(20px, 1.55vw, 25px);
      line-height: 1.03;
    }

    /* Italic Didot is used sparingly for editorial context and scene markers. */
    .story-label,
    .system-arch-kicker,
    .contact-links-kicker,
    .responsibility-kicker {
      font-family: var(--font-editorial) !important;
      font-style: italic !important;
      font-weight: 400 !important;
      text-transform: none !important;
      letter-spacing: .018em;
    }

    .story-label {
      font-size: 13px;
      color: rgba(158,207,83,.84);
    }

    .system-arch-kicker {
      font-size: 12px;
    }

    /* Paragraphs and operational UI return to sans for clarity. */
    .team-bio,
    .story-problem > p,
    .story-feature p,
    .role-members,
    .story-flow,
    .feature-video-trigger,
    .story-tab,
    .btn,
    .app-nav,
    .team-index,
    .demo-video-kicker,
    .eyebrow {
      font-family: var(--font-ui) !important;
      font-style: normal !important;
    }

    .eyebrow,
    .demo-video-kicker {
      text-transform: none !important;
      font-weight: 620;
      letter-spacing: .045em;
    }

    /* Keep the System header compact after removing the explanatory paragraph. */
    .app-view-system .system-view-frame > .system-story-head {
      grid-template-columns: minmax(0, 1fr) !important;
      width: min(960px, 100%);
      max-width: 960px;
      gap: 0 !important;
      align-items: end;
      padding: 6px 0 8px;
      margin: 0;
    }

    .app-view-system .system-story-head .eyebrow {
      margin: 0 0 9px;
    }

    .app-view-system .system-story-head h2 {
      max-width: 920px;
      margin: 0;
      line-height: .92;
    }

    .app-view-system .system-story-lead {
      display: none !important;
    }

    .app-view-system .system-workspace {
      margin-top: 8px;
    }
'''

text = text.replace(marker, css + marker, 1)
path.write_text(text, encoding='utf-8')
