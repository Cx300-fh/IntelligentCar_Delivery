from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

# Main System heading: remove the redundant prefix.
text, n = re.subn(
    r'(<div class="system-story-head">\s*<p class="eyebrow">Designed for the real world</p>\s*<h2>).*?(</h2>)',
    r'\1From Problem To Answer\2',
    text,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit('Could not update main System heading')

replacements = {
    '<button class="story-tab four-tab is-active" type="button" data-four="order"><span>01</span>Order</button>': '<button class="story-tab four-tab is-active" type="button" data-four="order"><span>01</span>Mission: Impossible</button>',
    '<button class="story-tab four-tab" type="button" data-four="system"><span>02</span>System</button>': '<button class="story-tab four-tab" type="button" data-four="system"><span>02</span>Fast &amp; Furious</button>',
    '<button class="story-tab four-tab" type="button" data-four="safety"><span>03</span>Safety</button>': '<button class="story-tab four-tab" type="button" data-four="safety"><span>03</span>No Time to Die</button>',
    '<button class="story-tab four-tab" type="button" data-four="future"><span>04</span>Future Work</button>': '<button class="story-tab four-tab" type="button" data-four="future"><span>04</span>Brand New Car</button>',
    '<span class="four-kicker">01 / Order</span><h3>From request to a dispatchable mission.</h3>': '<span class="four-kicker">01 / Order</span><h3>Mission: Impossible</h3>',
    '<span class="four-kicker">02 / System</span><h3>One robot, coordinated from sensing to dispatch.</h3>': '<span class="four-kicker">02 / System</span><h3>Fast &amp; Furious</h3>',
    '<span class="four-kicker">03 / Safety</span><h3>A safe stop is a designed system state.</h3>': '<span class="four-kicker">03 / Safety</span><h3>No Time to Die</h3>',
    '<span class="four-kicker">04 / Future Work</span><h3>Beyond delivery: new ways to control and scale the robot.</h3>': '<span class="four-kicker">04 / Future Work</span><h3>Brand New Car</h3>',
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f'Missing expected fragment: {old[:80]}')
    text = text.replace(old, new, 1)

# Make the page heading part of the System scroll flow instead of staying outside it.
needle = "  const scroll = document.getElementById('systemStoryScroll');\n  const nav = document.querySelector('.app-view-system .system-story-nav');\n  if (!scroll || !nav) return;\n"
insert = needle + "\n  const pageHead = document.querySelector('.app-view-system .system-story-head');\n  if (pageHead && pageHead.parentElement !== scroll) {\n    scroll.insertBefore(pageHead, scroll.firstChild);\n  }\n"
if needle not in text:
    raise SystemExit('Could not locate four-module System initializer')
text = text.replace(needle, insert, 1)

# Final overrides live after the redesign CSS so they win without disturbing older styles.
marker = '<!-- SYSTEM FOUR MODULES v2 END -->'
style = '''\n<style id="system-cinema-title-pass">\n  .app-view-system .system-workspace {\n    grid-template-columns: 158px minmax(0, 1fr);\n  }\n  .app-view-system .system-story-head {\n    position: static !important;\n    inset: auto !important;\n    transform: none !important;\n    margin: 0 0 30px !important;\n    padding: 0 0 18px !important;\n  }\n  .app-view-system .system-story-head h2 {\n    max-width: none !important;\n    width: max-content;\n    white-space: nowrap;\n    font-size: clamp(38px, 4.25vw, 62px) !important;\n    line-height: .94 !important;\n  }\n  .app-view-system .story-tab.four-tab {\n    min-height: 70px;\n    padding-right: 8px;\n    font-size: 11px;\n    line-height: 1.15;\n  }\n  .app-view-system .story-tab.four-tab span {\n    margin-bottom: 4px;\n  }\n  @media (max-width: 980px) {\n    .app-view-system .system-workspace { grid-template-columns: 142px minmax(0,1fr); }\n    .app-view-system .system-story-head h2 { font-size: clamp(32px, 5.2vw, 50px) !important; }\n  }\n  @media (max-width: 840px) {\n    .app-view-system .system-story-head h2 {\n      width: auto;\n      white-space: normal;\n      text-wrap: balance;\n    }\n  }\n</style>\n'''
if marker not in text:
    raise SystemExit('Missing System four-module end marker')
text = text.replace(marker, style + marker, 1)

path.write_text(text, encoding='utf-8')
