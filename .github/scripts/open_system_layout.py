from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

if 'System open canvas v1' in text:
    raise SystemExit('System open canvas patch already present')

marker = '  </style>\n</head>'
if marker not in text:
    raise SystemExit('style closing marker not found')

css = r'''

    /* ============================================================
       System open canvas v1
       Open composition: no enclosing workspace card, left rail acts only
       as a lightweight chapter indicator while content owns the canvas.
       ============================================================ */
    .app-view-system .system-view-frame {
      width: min(1380px, calc(100% - 72px));
      max-width: none;
      padding-top: 28px;
      padding-bottom: 0;
      gap: 18px;
    }

    .app-view-system .system-workspace {
      min-height: 0;
      grid-template-columns: 142px minmax(0, 1fr);
      gap: clamp(34px, 4vw, 68px);
      align-items: stretch;
      overflow: visible;
    }

    .app-view-system .system-story-nav {
      position: relative;
      align-self: stretch;
      justify-content: center;
      gap: 0;
      padding: 8px 0;
      margin: 0;
      overflow: visible;
      border: 0 !important;
      border-radius: 0 !important;
      background: transparent !important;
      box-shadow: none !important;
      backdrop-filter: none !important;
      -webkit-backdrop-filter: none !important;
    }

    .app-view-system .system-story-nav::before {
      content: "";
      position: absolute;
      left: 8px;
      top: 28px;
      bottom: 28px;
      width: 1px;
      background: linear-gradient(
        180deg,
        transparent,
        rgba(205,224,211,.12) 10%,
        rgba(205,224,211,.12) 90%,
        transparent
      );
      pointer-events: none;
    }

    .app-view-system .system-story-nav .story-tab {
      position: relative;
      display: block;
      width: 100%;
      min-height: 64px;
      padding: 10px 0 10px 30px;
      border: 0 !important;
      border-radius: 0 !important;
      background: transparent !important;
      box-shadow: none !important;
      color: rgba(208,218,211,.42);
      text-align: left;
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 13px;
      line-height: 1.05;
      letter-spacing: .025em;
      transition:
        color .26s ease,
        transform .26s cubic-bezier(.2,.75,.2,1),
        opacity .26s ease;
    }

    .app-view-system .system-story-nav .story-tab span {
      display: block;
      margin: 0 0 5px;
      color: rgba(118,185,0,.48);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 8px;
      line-height: 1;
      letter-spacing: .14em;
      transition: color .26s ease;
    }

    .app-view-system .system-story-nav .story-tab::before {
      content: "";
      position: absolute;
      left: 4px;
      top: 50%;
      width: 9px;
      height: 9px;
      right: auto;
      bottom: auto;
      border: 1px solid rgba(170,199,177,.26);
      border-radius: 50%;
      background: #0a130e;
      box-shadow: 0 0 0 4px rgba(8,15,11,.86);
      transform: translateY(-50%) scale(.72);
      transform-origin: center;
      opacity: .68;
      transition:
        transform .26s cubic-bezier(.2,.75,.2,1),
        border-color .26s ease,
        background .26s ease,
        box-shadow .26s ease,
        opacity .26s ease;
    }

    .app-view-system .system-story-nav .story-tab::after {
      display: none !important;
    }

    .app-view-system .system-story-nav .story-tab:hover {
      color: rgba(236,241,238,.74);
      background: transparent !important;
      transform: translateX(3px);
    }

    .app-view-system .system-story-nav .story-tab.is-active {
      color: rgba(248,250,249,.98);
      background: transparent !important;
      transform: translateX(5px);
    }

    .app-view-system .system-story-nav .story-tab.is-active span {
      color: rgba(151,213,54,.92);
    }

    .app-view-system .system-story-nav .story-tab.is-active::before {
      border-color: rgba(167,223,77,.82);
      background: var(--green);
      box-shadow:
        0 0 0 4px rgba(8,15,11,.92),
        0 0 18px rgba(118,185,0,.22);
      transform: translateY(-50%) scale(1);
      opacity: 1;
    }

    .app-view-system .system-story-scroll {
      min-height: 0;
      height: 100%;
      padding: 0 clamp(8px, 1.5vw, 22px) 0 0;
      border: 0 !important;
      border-radius: 0 !important;
      background: transparent !important;
      box-shadow: none !important;
      backdrop-filter: none !important;
      -webkit-backdrop-filter: none !important;
      scrollbar-width: thin;
      scrollbar-color: rgba(118,185,0,.20) transparent;
    }

    .app-view-system .system-story-scroll::-webkit-scrollbar {
      width: 6px;
    }

    .app-view-system .system-story-scroll::-webkit-scrollbar-track {
      background: transparent;
    }

    .app-view-system .system-story-scroll::-webkit-scrollbar-thumb {
      border-radius: 999px;
      background: rgba(118,185,0,.18);
    }

    .app-view-system .system-story-scroll > .story-panel {
      min-height: calc(100svh - 236px);
      padding: 18px 0 52px;
      border-bottom: 1px solid rgba(218,232,223,.07);
      background: transparent !important;
    }

    .app-view-system .system-story-scroll > .story-panel:last-child {
      border-bottom: 0;
    }

    .app-view-system .story-problem {
      border-color: rgba(215,231,221,.09);
      background:
        radial-gradient(420px circle at 0% 0%, rgba(118,185,0,.055), transparent 68%),
        linear-gradient(145deg, rgba(18,27,22,.48), rgba(8,13,10,.34));
      box-shadow: inset 0 1px 0 rgba(255,255,255,.018);
    }

    .app-view-system .story-feature {
      background:
        linear-gradient(145deg, rgba(19,29,23,.52), rgba(9,14,11,.40));
      border-color: rgba(215,231,221,.085);
      box-shadow: inset 0 1px 0 rgba(255,255,255,.018);
    }

    @media (max-width: 1080px) {
      .app-view-system .system-view-frame {
        width: min(100% - 36px, 1240px);
      }

      .app-view-system .system-workspace {
        grid-template-columns: 122px minmax(0, 1fr);
        gap: 26px;
      }
    }

    @media (max-width: 900px) {
      .app-view-system .system-workspace {
        grid-template-columns: 1fr;
        grid-template-rows: auto minmax(0, 1fr);
        gap: 12px;
      }

      .app-view-system .system-story-nav {
        flex-direction: row;
        justify-content: flex-start;
        overflow-x: auto;
        overflow-y: hidden;
        padding: 0 0 8px;
        scrollbar-width: none;
      }

      .app-view-system .system-story-nav::-webkit-scrollbar {
        display: none;
      }

      .app-view-system .system-story-nav::before {
        left: 0;
        right: 0;
        top: auto;
        bottom: 3px;
        width: auto;
        height: 1px;
      }

      .app-view-system .system-story-nav .story-tab {
        flex: 0 0 auto;
        width: auto;
        min-width: 104px;
        min-height: 48px;
        padding: 5px 16px 7px 0;
        margin-right: 12px;
      }

      .app-view-system .system-story-nav .story-tab::before {
        left: 0;
        top: auto;
        bottom: -1px;
        width: 7px;
        height: 7px;
        transform: translateY(50%) scale(.72);
      }

      .app-view-system .system-story-nav .story-tab:hover,
      .app-view-system .system-story-nav .story-tab.is-active {
        transform: none;
      }

      .app-view-system .system-story-nav .story-tab.is-active::before {
        transform: translateY(50%) scale(1);
      }

      .app-view-system .system-story-scroll {
        padding-right: 0;
      }
    }

    @media (max-width: 620px) {
      .app-view-system .system-view-frame {
        width: min(100% - 20px, 1180px);
        padding-top: 16px;
      }

      .app-view-system .system-story-scroll > .story-panel {
        padding-top: 10px;
        padding-bottom: 34px;
      }
    }
'''

text = text.replace(marker, css + marker, 1)
path.write_text(text, encoding='utf-8')
