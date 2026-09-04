from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

if 'System card navigation v1' in text:
    raise SystemExit('System card navigation patch already applied')

# Make the existing interactive campus map a first-class System demo target.
if '<div class="move-route-lab" id="moveRouteLab"' not in text:
    old = '<div class="move-route-lab">'
    if old not in text:
        raise SystemExit('move route lab not found')
    text = text.replace(old, '<div class="move-route-lab" id="moveRouteLab" tabindex="-1">', 1)

if '<span class="story-label">Dynamic routing algorithm</span>' in text:
    text = text.replace(
        '<span class="story-label">Dynamic routing algorithm</span>',
        '<span class="story-label">Interactive demo / Dynamic routing</span>',
        1,
    )

# Dynamic rerouting opens the embedded interactive map instead of a video modal.
dynamic_pattern = re.compile(
    r'<button class="feature-video-trigger" type="button" '
    r'data-video="assets/media/system/move-reroute\.mp4" '
    r'data-title="Dynamic rerouting" data-copy="[^"]*">Demo ↗</button>'
)
text, dynamic_count = dynamic_pattern.subn(
    '<button class="feature-route-trigger" type="button" data-target="moveRouteLab" '
    'aria-label="Open interactive dynamic routing demo"></button>',
    text,
    count=1,
)
if dynamic_count != 1:
    raise SystemExit(f'expected one dynamic rerouting trigger, changed {dynamic_count}')

# All other feature-card demo buttons become invisible full-card hit targets.
# The existing video modal logic remains unchanged and receives the click normally.
feature_pattern = re.compile(
    r'<button class="feature-video-trigger" type="button"(?P<attrs>[^>]*)>Demo ↗</button>'
)

def replace_feature(match):
    attrs = match.group('attrs')
    title_match = re.search(r'data-title="([^"]+)"', attrs)
    title = title_match.group(1) if title_match else 'function'
    return (
        '<button class="feature-video-trigger" type="button"'
        + attrs
        + f' aria-label="Open {title} demo"></button>'
    )

text, feature_count = feature_pattern.subn(replace_feature, text)
if feature_count < 10:
    raise SystemExit(f'expected many feature demo buttons, changed only {feature_count}')

style_marker = '  </style>\n</head>'
if style_marker not in text:
    raise SystemExit('style marker not found')

css = r'''

    /* ============================================================
       System card navigation v1
       Feature cards are the interaction target; repetitive Demo badges are gone.
       Dynamic rerouting jumps to the embedded interactive campus-map demo.
       ============================================================ */
    .app-view-system .story-feature {
      position: relative;
      cursor: pointer;
    }

    .app-view-system .story-feature .feature-video-trigger,
    .app-view-system .story-feature .feature-route-trigger {
      position: absolute !important;
      inset: 0 !important;
      z-index: 8 !important;
      width: 100% !important;
      height: 100% !important;
      margin: 0 !important;
      padding: 0 !important;
      border: 0 !important;
      border-radius: inherit !important;
      background: transparent !important;
      color: transparent !important;
      font-size: 0 !important;
      opacity: 0 !important;
      cursor: pointer;
    }

    .app-view-system .story-feature .story-feature-top {
      justify-content: flex-start;
    }

    .app-view-system .story-feature::after {
      content: "↗";
      position: absolute;
      right: 17px;
      top: 15px;
      z-index: 3;
      color: rgba(171,196,179,.28);
      font-family: var(--font-ui, system-ui, sans-serif);
      font-size: 14px;
      line-height: 1;
      transform: translate(0, 0);
      transition: color .24s ease, transform .24s cubic-bezier(.2,.75,.2,1);
      pointer-events: none;
    }

    .app-view-system .story-feature:hover::after,
    .app-view-system .story-feature:focus-within::after {
      color: rgba(164,220,75,.82);
      transform: translate(3px, -3px);
    }

    .app-view-system .story-feature:focus-within {
      border-color: rgba(139,201,44,.30) !important;
      box-shadow: 0 0 0 1px rgba(118,185,0,.06), inset 0 1px 0 rgba(255,255,255,.025);
    }

    .app-view-system #moveRouteLab {
      scroll-margin-top: 14px;
      outline: none;
      transition: filter .35s ease;
    }

    .app-view-system #moveRouteLab.is-jump-highlight {
      animation: routeDemoArrival 1.15s ease both;
    }

    @keyframes routeDemoArrival {
      0%   { filter: brightness(1); }
      28%  { filter: brightness(1.12); }
      100% { filter: brightness(1); }
    }
'''
text = text.replace(style_marker, css + style_marker, 1)

script_marker = '\n</body>'
if script_marker not in text:
    raise SystemExit('body closing marker not found')

script = r'''

<script>
(() => {
  const systemScroll = document.getElementById('systemStoryScroll');
  const routeTriggers = [...document.querySelectorAll('.feature-route-trigger[data-target]')];
  if (!systemScroll || !routeTriggers.length) return;

  const jumpToTarget = (trigger) => {
    const target = document.getElementById(trigger.dataset.target || '');
    if (!target) return;
    const top = target.getBoundingClientRect().top
      - systemScroll.getBoundingClientRect().top
      + systemScroll.scrollTop
      - 10;
    systemScroll.scrollTo({ top, behavior: 'smooth' });
    window.setTimeout(() => {
      target.focus({ preventScroll: true });
      target.classList.remove('is-jump-highlight');
      void target.offsetWidth;
      target.classList.add('is-jump-highlight');
      window.setTimeout(() => target.classList.remove('is-jump-highlight'), 1200);
    }, 360);
  };

  routeTriggers.forEach((trigger) => {
    trigger.addEventListener('click', () => jumpToTarget(trigger));
  });
})();
</script>
'''
text = text.replace(script_marker, script + script_marker, 1)

path.write_text(text, encoding='utf-8')
