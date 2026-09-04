from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

# Browser-safe media sources.
text = text.replace('assets/media/turnaround.mov', 'assets/media/turnaround.mp4')
text = text.replace('assets/media/obstacle.mov', 'assets/media/obstacle.mp4')
text = text.replace('assets/media/weight.mov', 'assets/media/weight.mp4')

# Add a visual field to every System feature card once.
if 'class="feature-visual"' not in text:
    card_pattern = re.compile(r'(<article class="story-feature ([^"]*)">.*?<h4>(.*?)</h4><p>.*?</p>)(</article>)')
    def add_feature_visual(match):
        head, classes, title, close = match.groups()
        return head + '<div class="feature-visual" aria-hidden="true"><span></span><span></span><span></span><span></span><b></b></div>' + close
    text, count = card_pattern.subn(add_feature_visual, text)
    if count < 10:
        raise RuntimeError(f'Expected System feature cards, only enriched {count}')

# Add scene schematics to the problem cards.
problem_visuals = {
    'Order flow': 'order',
    'Cargo flow': 'cargo',
    'Motion flow': 'move',
    'Scale flow': 'scale',
    'Trust flow': 'trust',
}
for aria, kind in problem_visuals.items():
    marker = f'<div class="story-flow'
    # Locate the exact flow tag by aria-label and insert immediately before it.
    pattern = re.compile(r'(\s*<div class="story-flow[^\"]*" aria-label="' + re.escape(aria) + r'">)')
    if f'problem-visual pv-{kind}' not in text:
        visual = f'\n            <div class="problem-visual pv-{kind}" aria-hidden="true"><span></span><span></span><span></span><span></span><span></span><b></b></div>'
        text, count = pattern.subn(visual + r'\1', text, count=1)
        if count != 1:
            raise RuntimeError(f'Could not insert problem visual for {aria}')

css = r'''

    /* ============================================================
       System visual storytelling + density v3
       ============================================================ */
    @media (min-width: 901px) {
      .app-view-system .system-story-scroll > .story-panel {
        min-height: clamp(500px, 61vh, 610px) !important;
        padding: 8px 0 30px !important;
        align-items: stretch !important;
      }

      .app-view-system .story-problem {
        min-height: 100% !important;
        height: 100%;
        display: flex;
        flex-direction: column;
        padding: 24px 26px 20px !important;
      }

      .app-view-system .story-feature-grid {
        height: 100%;
        align-self: stretch;
        grid-auto-rows: 1fr;
        align-content: stretch !important;
      }

      .app-view-system .story-feature {
        height: 100%;
        min-height: 0 !important;
        display: flex;
        flex-direction: column;
        overflow: hidden;
        padding: 14px 15px 0 !important;
      }

      .app-view-system .move-grid .story-feature {
        min-height: 0 !important;
      }
    }

    .feature-visual {
      position: relative;
      isolation: isolate;
      flex: 1 1 auto;
      min-height: 92px;
      margin: 16px -15px 0;
      overflow: hidden;
      border-top: 1px solid rgba(219,232,223,.07);
      background:
        linear-gradient(rgba(118,185,0,.026) 1px, transparent 1px),
        linear-gradient(90deg, rgba(118,185,0,.026) 1px, transparent 1px),
        radial-gradient(circle at 72% 42%, rgba(118,185,0,.09), transparent 46%),
        rgba(4,10,7,.24);
      background-size: 18px 18px, 18px 18px, auto, auto;
    }

    .feature-visual b {
      position: absolute;
      left: 14px;
      bottom: 10px;
      color: rgba(185,202,190,.38);
      font: 600 8px/1 Inter, ui-sans-serif, system-ui, sans-serif;
      letter-spacing: .16em;
    }

    .is-video .feature-visual b::after { content: "REAL VIDEO"; }
    .is-code .feature-visual b::after { content: "SOURCE CODE"; }
    .is-map .feature-visual b::after { content: "INTERACTIVE MAP"; }
    .is-static .feature-visual b::after { content: "HARDWARE"; }

    /* Video: viewfinder + play mark + timing bars. */
    .is-video .feature-visual::before {
      content: "";
      position: absolute;
      width: 58px;
      height: 40px;
      right: 20px;
      top: 18px;
      border: 1px solid rgba(171,219,91,.34);
      border-radius: 7px;
      box-shadow: 0 0 24px rgba(118,185,0,.06);
    }

    .is-video .feature-visual::after {
      content: "▶" !important;
      position: absolute !important;
      right: 42px !important;
      top: 30px !important;
      width: auto !important;
      height: auto !important;
      color: rgba(157,216,64,.88) !important;
      background: none !important;
      border: 0 !important;
      font-size: 13px !important;
      opacity: 1 !important;
      transform: none !important;
    }

    .is-video .feature-visual span {
      position: absolute;
      bottom: 27px;
      width: 2px;
      border-radius: 3px;
      background: rgba(144,205,49,.52);
    }
    .is-video .feature-visual span:nth-child(1) { left: 48%; height: 14px; }
    .is-video .feature-visual span:nth-child(2) { left: 52%; height: 26px; }
    .is-video .feature-visual span:nth-child(3) { left: 56%; height: 19px; }
    .is-video .feature-visual span:nth-child(4) { left: 60%; height: 31px; }

    /* Code: terminal glyph made from syntax lines. */
    .is-code .feature-visual::before {
      content: "</>";
      position: absolute;
      right: 18px;
      top: 14px;
      color: rgba(150,210,58,.52);
      font: 500 13px/1 ui-monospace, SFMono-Regular, Menlo, monospace;
    }
    .is-code .feature-visual span {
      position: absolute;
      left: 18px;
      height: 2px;
      border-radius: 4px;
      background: linear-gradient(90deg, rgba(214,229,218,.36), rgba(118,185,0,.16));
    }
    .is-code .feature-visual span:nth-child(1) { top: 23px; width: 46%; }
    .is-code .feature-visual span:nth-child(2) { top: 35px; left: 30px; width: 58%; }
    .is-code .feature-visual span:nth-child(3) { top: 47px; left: 30px; width: 37%; }
    .is-code .feature-visual span:nth-child(4) { top: 59px; width: 51%; }

    /* Map: route trace and nodes. */
    .is-map .feature-visual::before {
      content: "";
      position: absolute;
      left: 25px;
      right: 28px;
      top: 46%;
      height: 1px;
      background: linear-gradient(90deg, rgba(118,185,0,.18), rgba(155,215,60,.72), rgba(118,185,0,.18));
      transform: rotate(-4deg);
      transform-origin: center;
    }
    .is-map .feature-visual span {
      position: absolute;
      top: 40%;
      width: 9px;
      height: 9px;
      border-radius: 50%;
      border: 1px solid rgba(163,219,76,.6);
      background: #08100b;
      box-shadow: 0 0 0 4px rgba(118,185,0,.025);
    }
    .is-map .feature-visual span:nth-child(1) { left: 18%; top: 52%; }
    .is-map .feature-visual span:nth-child(2) { left: 38%; top: 38%; }
    .is-map .feature-visual span:nth-child(3) { left: 59%; top: 44%; background: var(--green); }
    .is-map .feature-visual span:nth-child(4) { left: 78%; top: 27%; }

    /* Hardware: layered enclosure / sensor geometry. */
    .is-static .feature-visual::before,
    .is-static .feature-visual::after {
      content: "";
      position: absolute;
      right: 22px;
      border: 1px solid rgba(176,215,114,.24);
      border-radius: 9px;
      transform: skewY(-4deg);
    }
    .is-static .feature-visual::before { top: 19px; width: 76px; height: 45px; }
    .is-static .feature-visual::after { top: 31px; right: 34px; width: 76px; height: 45px; border-color: rgba(118,185,0,.18); }
    .is-static .feature-visual span:nth-child(1) {
      position: absolute; right: 52px; top: 42px; width: 8px; height: 8px; border-radius: 50%; background: rgba(118,185,0,.76);
      box-shadow: 0 0 18px rgba(118,185,0,.18);
    }

    /* Problem-side scene diagram fills the previously empty lower field. */
    .problem-visual {
      position: relative;
      flex: 1 1 auto;
      min-height: 105px;
      margin: 20px 0 16px;
      overflow: hidden;
      border-top: 1px solid rgba(218,232,223,.06);
      border-bottom: 1px solid rgba(218,232,223,.055);
      background:
        radial-gradient(circle at 72% 48%, rgba(118,185,0,.08), transparent 35%),
        linear-gradient(90deg, transparent, rgba(118,185,0,.025), transparent);
    }

    .problem-visual b::after {
      position: absolute;
      left: 0;
      bottom: 9px;
      color: rgba(190,206,195,.34);
      font: 600 8px/1 Inter, ui-sans-serif, system-ui, sans-serif;
      letter-spacing: .15em;
    }
    .pv-order b::after { content: "INTENT → TASK"; }
    .pv-cargo b::after { content: "LOAD → VERIFY"; }
    .pv-move b::after { content: "SENSE → PLAN → MOVE"; }
    .pv-scale b::after { content: "REQUESTS → SCHEDULE"; }
    .pv-trust b::after { content: "FAULT → SAFE STATE"; }

    .pv-order::before, .pv-move::before, .pv-scale::before {
      content: "";
      position: absolute;
      left: 7%; right: 8%; top: 47%;
      height: 1px;
      background: linear-gradient(90deg, rgba(118,185,0,.1), rgba(151,212,54,.65), rgba(118,185,0,.1));
    }
    .pv-order span, .pv-move span, .pv-scale span {
      position: absolute;
      top: calc(47% - 5px);
      width: 10px; height: 10px;
      border-radius: 50%;
      border: 1px solid rgba(157,215,68,.58);
      background: #08100b;
    }
    .pv-order span:nth-child(1), .pv-move span:nth-child(1), .pv-scale span:nth-child(1) { left: 8%; }
    .pv-order span:nth-child(2), .pv-move span:nth-child(2), .pv-scale span:nth-child(2) { left: 29%; }
    .pv-order span:nth-child(3), .pv-move span:nth-child(3), .pv-scale span:nth-child(3) { left: 50%; background: var(--green); }
    .pv-order span:nth-child(4), .pv-move span:nth-child(4), .pv-scale span:nth-child(4) { left: 71%; }
    .pv-order span:nth-child(5), .pv-move span:nth-child(5), .pv-scale span:nth-child(5) { left: 90%; }
    .pv-move span:nth-child(2) { top: 29%; }
    .pv-move span:nth-child(4) { top: 61%; }

    .pv-cargo::before {
      content: ""; position: absolute; left: 13%; top: 25%; width: 68px; height: 48px;
      border: 1px solid rgba(170,216,93,.38); border-radius: 8px;
      box-shadow: inset 0 -12px 0 rgba(118,185,0,.035);
    }
    .pv-cargo::after {
      content: ""; position: absolute; right: 13%; top: 49%; width: 38%; height: 4px; border-radius: 4px;
      background: linear-gradient(90deg, rgba(118,185,0,.2), rgba(118,185,0,.8));
      box-shadow: 0 -12px 0 rgba(213,227,217,.08), 0 12px 0 rgba(213,227,217,.05);
    }

    .pv-trust::before {
      content: ""; position: absolute; left: 15%; top: 18%; width: 58px; height: 66px;
      clip-path: polygon(50% 0, 92% 15%, 84% 68%, 50% 100%, 16% 68%, 8% 15%);
      border: 1px solid rgba(118,185,0,.5); background: rgba(118,185,0,.06);
    }
    .pv-trust::after {
      content: ""; position: absolute; left: 42%; right: 10%; top: 49%; height: 1px;
      background: repeating-linear-gradient(90deg, rgba(118,185,0,.6) 0 8px, transparent 8px 14px);
    }

    .story-flow {
      margin-top: 0 !important;
    }

    @media (max-width: 900px) {
      .feature-visual { min-height: 82px; }
      .problem-visual { min-height: 88px; }
    }
'''

marker = '  </style>\n</head>'
if 'System visual storytelling + density v3' not in text:
    if marker not in text:
        raise RuntimeError('style closing marker not found')
    text = text.replace(marker, css + '\n' + marker, 1)

path.write_text(text, encoding='utf-8')
