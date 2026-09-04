from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

replacements = {
    '进入实时 Demo →': 'Watch Demo',
    '查看系统设计': 'System Design',
    'Built by five people, connected by one system.': 'Intertwined Roles, Unified Vision',
    'Every module answers a real delivery problem.': 'System: From Problem To Answer',
    '<p class="system-story-lead">Start with the situation, then reveal the engineering response. Scroll through six linked scenes while the left rail always shows where you are.</p>': '',
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f'missing expected text: {old}')
    text = text.replace(old, new, 1)

if 'Typography hierarchy v1' in text:
    raise SystemExit('typography patch already present')

marker = '  </style>\n</head>'
if marker not in text:
    raise SystemExit('style marker missing')

css = r'''

    /* ============================================================
       Typography hierarchy v1
       Prism stays as the primary display face. Didot is used only as an
       editorial secondary layer via locally installed fonts; operational UI
       and body copy remain sans-serif for clarity.
       ============================================================ */
    @font-face {
      font-family: "DidotEditorial";
      src: local("Didot Bold"), local("Didot-Bold"), local("Didot");
      font-style: normal;
      font-weight: 700;
      font-display: swap;
    }

    @font-face {
      font-family: "DidotEditorial";
      src: local("Didot Italic"), local("Didot-Italic"), local("Didot");
      font-style: italic;
      font-weight: 400;
      font-display: swap;
    }

    :root {
      --font-display: "Prism", "Arial Narrow", "Helvetica Neue", Arial, sans-serif;
      --font-editorial: "DidotEditorial", "Didot", "Bodoni 72", "Bodoni MT", Georgia, serif;
      --font-ui: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    /* Preserve primary display headings exactly in the existing brand face. */
    h1,
    h2,
    h3,
    .hero-title-en,
    .system-view-frame > .system-story-head h2,
    .contact-head h2,
    .demo-video-head h2 {
      font-family: var(--font-display) !important;
      font-style: normal;
    }

    /* Editorial layer: secondary module names and human/context copy. */
    .story-feature h4,
    .system-architecture-v2 h4,
    .role-title,
    .feature-video-title {
      font-family: var(--font-editorial) !important;
      font-weight: 700;
      font-style: normal;
      letter-spacing: -.012em;
      text-transform: none !important;
    }

    .team-bio,
    .story-problem > p,
    .contact-links-kicker,
    .responsibility-kicker {
      font-family: var(--font-editorial) !important;
      font-style: italic;
      font-weight: 400;
      letter-spacing: .005em;
    }

    /* Operational text stays neutral and highly legible. */
    .app-nav,
    .btn,
    .story-tab,
    .feature-video-trigger,
    .story-feature p,
    .role-members,
    .team-index,
    .story-label,
    .system-arch-kicker,
    .demo-video-kicker,
    .eyebrow {
      font-family: var(--font-ui) !important;
    }

    /* Reduce the blanket all-caps effect. Uppercase remains only for tiny
       technical micro-labels such as story-label / system-arch-kicker. */
    .eyebrow,
    .demo-video-kicker,
    .role-title,
    .responsibility-kicker,
    .contact-links-kicker {
      text-transform: none !important;
    }

    .eyebrow,
    .demo-video-kicker {
      font-weight: 620;
      letter-spacing: .045em;
    }

    .role-title {
      font-size: clamp(17px, 1.28vw, 20px);
      line-height: 1.08;
    }

    .story-feature h4,
    .system-architecture-v2 h4 {
      line-height: 1.08;
    }

    /* System header: one open hierarchy after removing the explanatory lead. */
    .app-view-system .system-view-frame > .system-story-head {
      grid-template-columns: minmax(0, 1fr) !important;
      align-items: end;
      gap: 0;
      width: min(980px, 100%);
      max-width: 980px;
      padding-bottom: 4px;
    }

    .app-view-system .system-view-frame > .system-story-head .eyebrow {
      margin-bottom: 10px;
    }

    .app-view-system .system-view-frame > .system-story-head h2 {
      max-width: 940px;
      margin: 0;
      line-height: .93;
    }

    .app-view-system .system-story-lead {
      display: none !important;
    }

    @media (max-width: 700px) {
      .app-view-system .system-view-frame > .system-story-head {
        width: 100%;
      }
    }
'''

text = text.replace(marker, css + marker, 1)
path.write_text(text, encoding='utf-8')
