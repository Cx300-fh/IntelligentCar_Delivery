from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')


def replace_once(old: str, new: str, label: str):
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected exactly 1 match, got {count}')
    text = text.replace(old, new, 1)

# 1) Weight video in Move.
replace_once(
    '<article class="story-feature is-static"><div class="story-feature-top"><span>02</span></div><h4>Payload-aware motion</h4><p>Measured payload ranges select different turning parameters so the same corner remains stable under different loads.</p></article>',
    '<article class="story-feature is-video"><div class="story-feature-top"><span>02</span><button class="feature-video-trigger" type="button" data-video="assets/media/weight.mov" data-title="Payload Measurement" data-copy="Real load-cell demonstration showing the measured payload changing before motion parameters are selected." aria-label="Play Payload Measurement video"></button></div><h4>Payload measurement</h4><p>Measured payload becomes a real control input instead of an assumed empty-car condition.</p></article>',
    'payload card'
)

# 2) Obstacle now uses the newly uploaded real obstacle video.
replace_once(
    '<article class="story-feature is-map"><div class="story-feature-top"><span>04</span><button class="feature-route-trigger" type="button" data-target="moveRouteLab" aria-label="Open interactive obstacle-aware rerouting demo"></button></div><h4>Obstacle-aware rerouting</h4><p>Blocked roads and changing costs trigger a new route from the car\'s current position.</p></article>',
    '<article class="story-feature is-video"><div class="story-feature-top"><span>04</span><button class="feature-video-trigger" type="button" data-video="assets/media/obstacle.mov" data-title="Obstacle Handling" data-copy="Real vehicle demonstration of obstacle detection and the blocked-road response." aria-label="Play Obstacle Handling video"></button></div><h4>Obstacle handling</h4><p>Detected obstacles are converted into a physical stop or blocked-road condition before replanning.</p></article>',
    'obstacle card'
)

# 3) Navigation inherits the interactive map behavior that Obstacle used before.
replace_once(
    '<article class="story-feature is-code"><div class="story-feature-top"><span>05</span><button class="feature-code-trigger" type="button" data-code-key="motionControl" aria-label="Open Navigation and control source code"></button></div><h4>Navigation & 5 ms control</h4><p>Discrete navigation actions are executed below by a real-time steering, motor and safety loop.</p></article>',
    '<article class="story-feature is-map"><div class="story-feature-top"><span>05</span><button class="feature-route-trigger" type="button" data-target="moveRouteLab" aria-label="Open interactive navigation and rerouting demo"></button></div><h4>Navigation & rerouting</h4><p>The current node, destination, blocked edges and weighted route are exposed as one interactive campus graph.</p></article>',
    'navigation card'
)

# 4) Fix native cursor inside the glass source-code modal and tighten System density.
css = r'''

    /* ============================================================
       System density + modal cursor fix v2
       ============================================================ */
    @media (hover:hover) and (pointer:fine) {
      body.fx-cursor-on.feature-code-open .feature-code-modal,
      body.fx-cursor-on.feature-code-open .feature-code-modal * {
        cursor: auto !important;
      }

      body.fx-cursor-on.feature-code-open .feature-code-modal button,
      body.fx-cursor-on.feature-code-open .feature-code-modal a {
        cursor: pointer !important;
      }
    }

    body.feature-code-open .fx-cursor,
    body.feature-code-open .fx-cursor-dot {
      opacity: 0 !important;
      visibility: hidden !important;
    }

    @media (min-width: 901px) {
      .app-view-system .system-workspace {
        grid-template-columns: 112px minmax(0, 1fr) !important;
        gap: clamp(22px, 2.6vw, 38px) !important;
      }

      .app-view-system .system-story-nav {
        align-self: start !important;
        justify-content: flex-start !important;
        padding: 2px 0 0 !important;
        margin-top: 2px !important;
      }

      .app-view-system .system-story-nav::before {
        top: 8px !important;
        bottom: 8px !important;
      }

      .app-view-system .system-story-nav .story-tab {
        min-height: 47px !important;
        padding: 6px 0 6px 27px !important;
        font-size: 12px !important;
        letter-spacing: .018em !important;
      }

      .app-view-system .system-story-nav .story-tab span {
        margin-bottom: 3px !important;
        font-size: 7px !important;
      }

      .story-panel {
        gap: 24px !important;
      }

      .story-feature-grid {
        gap: 8px !important;
      }

      .story-feature,
      .move-grid .story-feature {
        min-height: 150px !important;
        padding: 14px 15px 15px !important;
      }

      .story-feature-top {
        margin-bottom: 14px !important;
      }

      .story-feature h4 {
        margin-bottom: 8px !important;
      }

      .story-feature p {
        line-height: 1.46 !important;
      }
    }
'''

marker = '  </style>\n</head>'
if 'System density + modal cursor fix v2' not in text:
    if marker not in text:
        raise RuntimeError('style closing marker not found')
    text = text.replace(marker, css + '\n' + marker, 1)

path.write_text(text, encoding='utf-8')
