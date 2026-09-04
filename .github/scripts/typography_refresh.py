from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

replacements = {
    '进入实时 Demo →': 'Watch Demo',
    '查看系统设计': 'System Design',
    'Built by five people, connected by one system.': 'Intertwined Roles, Unified Vision',
    'Every module answers a real delivery problem.': 'System: From Problem to Answer',
    '        <p class="system-story-lead">Start with the situation, then reveal the engineering response. Scroll through six linked scenes while the left rail always shows where you are.</p>\n': '',
    'REAL SCENE / ORDER': 'Real scene / Order',
    'REAL SCENE / CARGO': 'Real scene / Cargo',
    'REAL SCENE / MOVE': 'Real scene / Move',
    'REAL SCENE / SCALE': 'Real scene / Scale',
    'REAL SCENE / TRUST': 'Real scene / Trust',
    'DYNAMIC ROUTING ALGORITHM': 'Dynamic routing algorithm',
    'HUMAN & INTERACTION': 'Human & interaction',
    'PC DISPATCH': 'PC dispatch',
    'DELIVERY CAR': 'Delivery car',
}

for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f'missing expected text: {old}')
    text = text.replace(old, new, 1)

if 'Typography hierarchy v1' in text:
    raise SystemExit('typography patch already present')

marker = '  </style>\n</head>'
if marker not in text:
    raise SystemExit('style closing marker not found')

css = r'''

    /* ============================================================
       Typography hierarchy v1
       Prism = primary display titles
       Didot = editorial / secondary hierarchy
       Sans = navigation, UI, descriptions, controls
       ============================================================ */
    @font-face {
      font-family: "DidotEditorial";
      src: url("./assets/fonts/Didot-Bold-1.ttf") format("truetype");
      font-style: normal;
      font-weight: 700;
      font-display: swap;
    }

    @font-face {
      font-family: "DidotEditorial";
      src: url("./assets/fonts/Didot-Italic-2.ttf") format("truetype");
      font-style: italic;
      font-weight: 400;
      font-display: swap;
    }

    /* Major display titles remain Prism. */
    .demo-video-head h2,
    .system-story-head h2,
    .contact-head h2,
    .hero-title-en,
    .hero-title-en * {
      font-family: "Prism", "Arial Narrow", "Helvetica Neue", Arial, sans-serif;
      font-weight: 400;
    }

    /* Editorial secondary headings: mixed case, more contrast, less visual shouting. */
    .app-view-system .story-problem h3,
    .app-view-system .story-feature h4,
    .app-view-system .system-architecture-v2 h4,
    .app-view-system .system-final-copy h3,
    .app-view-contact .team-name,
    .app-view-contact .responsibility-head h3,
    .app-view-contact .contact-links h3 {
      font-family: "DidotEditorial", Didot, "Bodoni MT", "Bodoni 72", Georgia, serif;
      font-weight: 700;
      text-transform: none;
      letter-spacing: -.018em;
    }

    .app-view-system .story-problem h3 {
      font-size: clamp(32px, 3.1vw, 50px);
      line-height: .98;
    }

    .app-view-system .story-feature h4,
    .app-view-system .system-architecture-v2 h4 {
      font-size: clamp(19px, 1.55vw, 24px);
      line-height: 1.02;
    }

    .app-view-system .move-grid .story-feature h4 {
      font-size: clamp(17px, 1.35vw, 21px);
    }

    .app-view-contact .team-name {
      font-size: clamp(20px, 1.55vw, 25px);
      line-height: 1.02;
    }

    /* Italic Didot is reserved for editorial context labels, not UI controls. */
    .app-view-system .story-label,
    .app-view-system .system-arch-kicker,
    .app-view-contact .contact-links-kicker {
      font-family: "DidotEditorial", Didot, "Bodoni MT", "Bodoni 72", Georgia, serif;
      font-style: italic;
      font-weight: 400;
      text-transform: none;
      letter-spacing: .025em;
    }

    .app-view-system .story-label {
      font-size: 14px;
      color: rgba(166,213,95,.86);
    }

    .app-view-system .system-arch-kicker {
      font-size: 12px;
      color: rgba(173,194,180,.65);
    }

    /* UI language stays neutral and legible. */
    .app-nav,
    .hero-cta .btn,
    .feature-video-trigger,
    .story-flow,
    .system-story-nav .story-tab,
    .role-title,
    .role-members,
    .team-bio,
    .story-feature p,
    .story-problem p {
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    /* System header: open, compact, single-column after removing explanatory copy. */
    .app-view-system .system-view-frame > .system-story-head {
      grid-template-columns: minmax(0, 900px);
      justify-content: start;
      align-items: end;
      gap: 0;
      padding: 10px 0 4px;
      margin: 0;
    }

    .app-view-system .system-story-head .eyebrow {
      margin: 0 0 8px;
      font-size: 11px;
      letter-spacing: .09em;
      color: rgba(126,190,25,.82);
    }

    .app-view-system .system-story-head h2 {
      max-width: 900px;
      margin: 0;
      font-size: clamp(44px, 5.3vw, 72px);
      line-height: .9;
    }

    .app-view-system .system-workspace {
      margin-top: 4px;
    }

    @media (max-width: 900px) {
      .app-view-system .system-view-frame > .system-story-head {
        padding-top: 4px;
      }

      .app-view-system .system-story-head h2 {
        font-size: clamp(38px, 8vw, 56px);
      }
    }
'''

text = text.replace(marker, css + marker, 1)
path.write_text(text, encoding='utf-8')
