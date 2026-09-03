from pathlib import Path
import re

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

def sub(pattern, repl, label, flags=0):
    global text
    text2, n = re.subn(pattern, repl, text, count=1, flags=flags)
    if n != 1:
        raise SystemExit(f'{label}: expected 1 match, got {n}')
    text = text2

# 1 / 10: one continuous hero image, controlled left readability, much shallower bottom fade.
sub(r'''    \.hero::before \{\n      content: "";\n      position: absolute;\n      inset: 0;\n      z-index: -3;\n      background:\n        linear-gradient\(90deg, rgba\(12,14,13,\.94\) 0%, rgba\(12,14,13,\.82\) 32%, rgba\(12,14,13,\.25\) 72%\),\n        linear-gradient\(0deg, var\(--bg\) 0%, transparent 34%\),\n        url\("assets/delivery-robot-hero\.png"\) center right / cover no-repeat;\n      transform: scale\(1\.015\);\n    \}''', '''    .hero::before {
      content: "";
      position: absolute;
      inset: 0;
      z-index: -3;
      background:
        linear-gradient(90deg, rgba(4,8,6,.96) 0%, rgba(4,8,6,.72) 35%, rgba(4,8,6,.15) 68%, transparent 100%),
        url("assets/delivery-robot-hero.png") center right / cover no-repeat;
      transform: scale(1.008);
    }''', 'hero background')

sub(r'''    \.hero::after \{\n      content: "";\n      position: absolute;\n      inset: auto 0 0;\n      height: 34%;\n      z-index: -2;\n      background: linear-gradient\(180deg, transparent, var\(--bg\)\);\n    \}''', '''    .hero::after {
      content: "";
      position: absolute;
      inset: auto 0 0;
      height: 12%;
      z-index: -2;
      background: linear-gradient(180deg, transparent, rgba(4,8,6,.62));
      pointer-events: none;
    }''', 'hero bottom fade')

# 11: align navbar and hero content to the same 1280px grid without touching later sections.
sub(r'''    \.hero-grid \{\n      display: grid;\n      grid-template-columns: minmax\(0, 1\.02fr\) minmax\(300px, \.32fr\);\n      gap: 64px;\n      align-items: center;\n      transform: translateY\(52px\);\n    \}''', '''    .hero-grid {
      position: relative;
      display: grid;
      grid-template-columns: minmax(0, 520px) 1fr;
      gap: 72px;
      align-items: center;
      min-height: 640px;
      transform: none;
    }

    .hero > .shell,
    .topbar > .shell {
      width: min(1280px, calc(100% - 96px));
    }

    .hero-copy {
      width: min(100%, 520px);
      position: relative;
      z-index: 5;
    }''', 'hero grid')

# 8 / 9: calmer navigation, slightly larger icon and lighter brand signature.
sub(r'''    \.brand \{\n      display: flex;\n      align-items: center;\n      gap: 12px;\n      text-decoration: none;\n      font-weight: 760;\n      white-space: nowrap;\n    \}''', '''    .brand {
      display: flex;
      align-items: center;
      gap: 11px;
      text-decoration: none;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      font-size: 14px;
      font-weight: 560;
      letter-spacing: .012em;
      white-space: nowrap;
    }''', 'brand')

sub(r'''    \.brand-logo \{\n      width: 35px;\n      height: 35px;''', '''    .brand-logo {
      width: 39px;
      height: 39px;''', 'brand logo')

sub(r'''    \.nav-links \{\n      display: flex;\n      align-items: center;\n      gap: 22px;\n      color: var\(--muted-2\);\n      font-size: 14px;\n    \}''', '''    .nav-links {
      display: flex;
      align-items: center;
      gap: 25px;
      color: rgba(225,231,226,.70);
      font-size: 13.5px;
    }''', 'nav links')

# 2 / 6: reduce title competition and remove bright green from title decoration.
sub(r'''    /\* v29 — cleaner brand lockup spacing \*/\n    \.hero-title-mark \{\n      gap: 18px;\n      margin-top: -52px;\n    \}\n\n    \.hero-title-cn \{\n      top: -24px;\n      left: 14px;\n      gap: \.25em;\n      font-size: clamp\(92px, 11\.6vw, 160px\);\n      line-height: \.76;\n      color: #b7dc70;\n    \}\n\n    \.hero-title-en \{\n      margin-top: 16px;\n      font-size: clamp\(46px, 5\.9vw, 84px\);\n      line-height: \.88;\n    \}\n\n    @media \(max-width: 760px\) \{\n      \.hero-title-mark \{\n        gap: 13px;\n        margin-top: -18px;\n      \}\n\n      \.hero-title-cn \{\n        top: -8px;\n        left: 0;\n        gap: \.20em;\n      \}\n\n      \.hero-title-en \{\n        margin-top: 12px;\n      \}\n    \}''', '''    /* v29 — restrained brand lockup */
    .hero-title-mark {
      gap: 22px;
      margin: 0 0 0 0;
    }

    .hero-title-cn {
      top: 0;
      left: 0;
      margin-left: 0;
      gap: .25em;
      font-size: clamp(76px, 8.8vw, 126px);
      line-height: .80;
      color: #d8e3c4;
    }

    .hero-title-en {
      margin-top: 18px;
      font-size: clamp(34px, 4.25vw, 58px);
      line-height: .92;
      letter-spacing: .035em;
    }

    @media (max-width: 760px) {
      .hero-title-mark {
        gap: 17px;
        margin-top: 0;
      }

      .hero-title-cn {
        top: 0;
        left: 0;
        gap: .20em;
        font-size: clamp(64px, 20vw, 88px);
      }

      .hero-title-en {
        margin-top: 14px;
        font-size: clamp(30px, 9vw, 42px);
      }
    }''', 'title sizing and spacing')

# Change the later English title prism from bright green to charcoal/sage neutrals.
sub(r'''      background:\n        linear-gradient\(\n          100deg,\n          #76b900 0%,\n          #76b900 28%,\n          #caff63 43%,\n          #92d61a 55%,\n          #76b900 72%,\n          #76b900 100%\n        \);''', '''      background:
        linear-gradient(
          100deg,
          #cfd7d0 0%,
          #aab7ac 28%,
          #e0e6df 43%,
          #88998c 55%,
          #b8c3ba 72%,
          #cfd7d0 100%
        );''', 'english title color')

sub(r'''      background:\n        linear-gradient\(90deg, #b9ff41, rgba\(118,185,0,0\)\);\n      box-shadow: 0 0 14px rgba\(118,185,0,\.38\);''', '''      background:
        linear-gradient(90deg, rgba(225,232,226,.58), rgba(225,232,226,0));
      box-shadow: none;''', 'english title underline')

sub(r'''    \.hero-title-mark:hover \.hero-title-glyph-a \{\n      color: #b9ff41;''', '''    .hero-title-mark:hover .hero-title-glyph-a {
      color: #edf2e7;''', 'glyph hover a')
sub(r'''    \.hero-title-mark:hover \.hero-title-glyph-b \{\n      color: #b9ff41;''', '''    .hero-title-mark:hover .hero-title-glyph-b {
      color: #edf2e7;''', 'glyph hover b')

# 3 / 4: add project line and two clear CTAs.
sub(r'''      <div>\n        <h1 class="hero-title-mark" id="heroBrandTitle">''', '''      <div class="hero-copy">
        <h1 class="hero-title-mark" id="heroBrandTitle">''', 'hero copy class')

sub(r'''        </h1>\n\n      </div>\n      <aside class="mission-card"''', '''        </h1>
        <p class="hero-tagline">用清送，更轻松</p>
        <div class="hero-actions hero-cta">
          <a class="btn primary" href="#map">进入实时 Demo →</a>
          <a class="btn" href="#system">查看系统设计</a>
        </div>
      </div>
      <aside class="mission-card"''', 'hero tagline and cta')

# Add focused Hero-only text/CTA styling near existing hero actions.
sub(r'''    \.hero-actions \{\n      display: flex;\n      flex-wrap: wrap;\n      gap: 12px;\n      margin-top: 30px;\n    \}''', '''    .hero-actions {
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 30px;
    }

    .hero-tagline {
      margin: 28px 0 0;
      color: rgba(237,241,237,.84);
      font-size: clamp(17px, 1.35vw, 20px);
      font-weight: 480;
      letter-spacing: .05em;
    }

    .hero-cta {
      margin-top: 24px;
      gap: 10px;
    }

    .hero-cta .btn {
      min-height: 44px;
      padding: 0 18px;
      border-radius: 2px;
      font-size: 13px;
      letter-spacing: .015em;
    }

    .hero-cta .btn:not(.primary) {
      border-color: rgba(220,227,222,.30);
      background: rgba(10,14,12,.20);
      backdrop-filter: blur(8px);
    }''', 'tagline css')

# 5: much smaller route card placed bottom-right, tied to robot without covering its main body.
sub(r'''    \.mission-card \{\n      width: min\(100%, 340px\);\n      justify-self: end;\n      align-self: center;\n      transform: translate\(-16px, 48px\);''', '''    .mission-card {
      position: absolute;
      right: 0;
      bottom: 44px;
      width: min(100%, 278px);
      justify-self: end;
      align-self: end;
      transform: none;''', 'mission card position')

sub(r'''      padding: 20px 22px 20px;''', '''      padding: 15px 17px 14px;''', 'mission card padding')
sub(r'''    \.route-strip \{\n      position: relative;\n      height: 106px;''', '''    .route-strip {
      position: relative;
      height: 84px;''', 'route card height')
sub(r'''      width: 36px;\n      height: 36px;''', '''      width: 28px;
      height: 28px;''', 'route stop size')
sub(r'''      top: 22px;''', '''      top: 25px;''', 'route stop top')
sub(r'''      gap: 12px;\n      color: var\(--muted-2\);\n      font-size: 12px;''', '''      gap: 9px;
      color: rgba(220,228,222,.72);
      font-size: 10px;''', 'route labels')

# Later v30 transform must no longer reintroduce a base offset.
sub(r'''          calc\(-16px \+ var\(--scroll-card-x\) \+ var\(--drag-x\)\),\n          calc\(48px \+ var\(--scroll-card-y\) \+ var\(--drag-y\)\),''', '''          calc(var(--scroll-card-x) + var(--drag-x)),
          calc(var(--scroll-card-y) + var(--drag-y)),''', 'mission v30 transform')

# 7: localization ring becomes barely-there, slower and less green.
sub(r'''      border: 1px solid rgba\(159, 226, 56, \.34\);''', '''      border: 1px solid rgba(205, 221, 210, .10);''', 'ring border')
sub(r'''      --morph-opacity: \.26;''', '''      --morph-opacity: .10;''', 'ring opacity')
sub(r'''        radial-gradient\(circle at 54% 48%, rgba\(118,185,0,\.09\), transparent 42%\),\n        linear-gradient\(135deg, rgba\(112,164,255,\.025\), rgba\(118,185,0,\.025\)\);''', '''        radial-gradient(circle at 54% 48%, rgba(210,225,214,.028), transparent 42%),
        linear-gradient(135deg, rgba(205,220,210,.012), rgba(205,220,210,.008));''', 'ring fill')
sub(r'''        0 0 70px rgba\(118,185,0,\.045\);''', '''        0 0 56px rgba(210,225,214,.018);''', 'ring shadow')
sub(r'''      border: 1px dashed rgba\(217,255,181,\.16\);''', '''      border: 1px dashed rgba(220,230,223,.075);''', 'inner ring')
sub(r'''      animation: morphRingRotate 18s linear infinite;''', '''      animation: morphRingRotate 36s linear infinite;''', 'ring speed')
sub(r'''      background: #aaf332;\n      box-shadow:\n        0 0 12px rgba\(170,243,50,\.9\),\n        0 0 28px rgba\(118,185,0,\.42\);\n      animation: morphNodePulse 1\.9s ease-in-out infinite;''', '''      background: rgba(200,218,205,.55);
      box-shadow: 0 0 12px rgba(205,222,210,.12);
      animation: morphNodePulse 4.6s ease-in-out infinite;''', 'ring node')
sub(r'''          morph\.style\.setProperty\("--morph-opacity", lerp\(\.26, \.10, e\)\.toFixed\(3\)\);''', '''          morph.style.setProperty("--morph-opacity", lerp(.10, .055, e).toFixed(3));''', 'ring js opacity')

# 6: scroll indicator loses brand green; it is now a neutral cue.
sub(r'''        linear-gradient\(180deg, rgba\(190,247,92,\.15\), rgba\(190,247,92,\.82\)\);''', '''        linear-gradient(180deg, rgba(224,231,226,.10), rgba(224,231,226,.46));''', 'scroll cue color')

# 8: four-item navigation only.
sub(r'''      <div class="nav-links" aria-label="Page navigation">\n        <a href="#video-demo">Demo</a>\n        <a href="#loop">Loop</a>\n        <a href="#map">Dynamic Map</a>\n        <a href="#voice">Voice</a>\n        <a href="#system">System</a>\n        <a href="#source">Open Source</a>\n      </div>''', '''      <div class="nav-links" aria-label="Page navigation">
        <a href="#top">Overview</a>
        <a href="#video-demo">Demo</a>
        <a href="#system">System</a>
        <a href="#source">Open Source</a>
      </div>''', 'navigation html')

# Keep mobile aligned and avoid card covering the robot on narrow screens.
sub(r'''    @media \(max-width: 940px\) \{\n      \.nav-links \{ display: none; \}''', '''    @media (max-width: 940px) {
      .nav-links { display: none; }
      .hero > .shell,
      .topbar > .shell { width: min(100% - 34px, 1280px); }
      .hero-grid { min-height: 680px; }
      .mission-card { right: 0; bottom: 24px; }''', 'tablet hero alignment')

path.write_text(text, encoding='utf-8')
print('hero refinement v2 applied')
