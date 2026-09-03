from pathlib import Path
import re

path = Path("docs/index.html")
text = path.read_text(encoding="utf-8")

MARKER = "App view shell v1"
if MARKER in text:
    raise SystemExit("app-view restructuring already applied")


def require(pattern, label, flags=re.S):
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"could not find {label}")
    return m

hero_m = require(r'  <header id="top" class="hero">.*?  </header>', "hero")
video_m = require(r'    <section id="video-demo" class="shell demo-video-section">.*?    </section>', "demo section")
map_m = require(r'    <section id="map" class="shell">.*?    </section>', "map section")
system_m = require(r'    <section id="system" class="shell system-story">.*?    </section>', "system section")
contact_m = require(r'    <section id="contact" class="shell open-source contact-section">.*?    </section>', "contact section")

hero = hero_m.group(0)
video = video_m.group(0)
map_section = map_m.group(0)
system = system_m.group(0)
contact = contact_m.group(0).replace('id="contact"', 'id="contact-content"', 1)

# Hero now acts as the Overview view rather than a scroll gateway.
hero = hero.replace('href="#map">进入实时 Demo →</a>', 'href="#demo" data-app-view="demo">进入实时 Demo →</a>')
hero = hero.replace('href="#system">查看系统设计</a>', 'href="#system" data-app-view="system">查看系统设计</a>')
hero = hero.replace('SCROLL TO MORPH', 'CHOOSE A VIEW ABOVE')

# Strip the outer System section. Its content becomes the System app view.
system_inner = re.sub(r'^\s*<section id="system" class="shell system-story">\s*', '', system, count=1)
system_inner = re.sub(r'\s*</section>\s*$', '', system_inner, count=1)

# Turn horizontal System tabs into a persistent left navigation.
system_inner = system_inner.replace(
    '<div class="story-tabs" role="tablist" aria-label="System design stories">',
    '<aside class="system-story-nav" aria-label="System design stories">',
    1,
)
system_inner = system_inner.replace(
    '</div>\n\n      <div class="story-stage">',
    '</aside>\n\n      <div class="system-story-scroll" id="systemStoryScroll">',
    1,
)

# All six stories remain mounted and become scroll sections.
system_inner = re.sub(
    r'(<article[^>]*class="[^"]*story-panel[^"]*"[^>]*)\shidden([^>]*>)',
    r'\1\2',
    system_inner,
)

# Move the existing interactive campus map into Move / dynamic routing.
map_inner = re.sub(r'^\s*<section id="map" class="shell">\s*', '', map_section, count=1)
map_inner = re.sub(r'\s*</section>\s*$', '', map_inner, count=1)
map_embed = f'''\n\n          <div class="move-route-lab">\n            <span class="story-label">DYNAMIC ROUTING ALGORITHM</span>\n{map_inner}\n          </div>\n'''

move_id = system_inner.find('id="storyPanelMove"')
scale_id = system_inner.find('id="storyPanelScale"')
if move_id < 0 or scale_id < 0 or scale_id <= move_id:
    raise SystemExit("could not locate Move/Scale story boundaries")
move_close = system_inner.rfind('</article>', move_id, scale_id)
if move_close < 0:
    raise SystemExit("could not locate Move story closing tag")
system_inner = system_inner[:move_close] + map_embed + system_inner[move_close:]

# Group left navigation and the scrollable story rail into one workspace.
nav_start = system_inner.find('<aside class="system-story-nav"')
modal_start = system_inner.find('<div class="feature-video-modal"')
if nav_start < 0 or modal_start < 0:
    raise SystemExit("could not locate System navigation or video modal")
system_inner = (
    system_inner[:nav_start]
    + '<div class="system-workspace">\n      '
    + system_inner[nav_start:modal_start]
    + '      </div>\n\n      '
    + system_inner[modal_start:]
)

# Rebuild the visible page as four isolated app views.
app_main = f'''  <main class="app-shell-main" id="appShell">\n    <section class="app-view app-view-overview is-active" id="overview" data-app-view-panel="overview" aria-hidden="false">\n      <div class="app-view-scroll app-view-scroll-overview">\n{hero}\n      </div>\n    </section>\n\n    <section class="app-view app-view-demo" id="demo" data-app-view-panel="demo" aria-hidden="true">\n      <div class="app-view-scroll">\n{video}\n      </div>\n    </section>\n\n    <section class="app-view app-view-system" id="system" data-app-view-panel="system" aria-hidden="true">\n      <div class="system-view-frame shell">\n{system_inner}\n      </div>\n    </section>\n\n    <section class="app-view app-view-contact" id="contact" data-app-view-panel="contact" aria-hidden="true">\n      <div class="app-view-scroll">\n{contact}\n      </div>\n    </section>\n  </main>'''

main_end = text.find('  </main>', contact_m.end())
if main_end < 0:
    raise SystemExit("could not find main closing tag")
main_end += len('  </main>')
text = text[:hero_m.start()] + app_main + text[main_end:]

# Top navigation becomes an actual view switcher. Scrolling never changes the view.
nav_pattern = re.compile(r'<div class="nav-links" aria-label="Page navigation">.*?</div>', re.S)
nav_html = '''<div class="nav-links app-nav" aria-label="Page navigation">\n        <a class="is-active" href="#overview" data-app-view="overview">Overview</a>\n        <a href="#demo" data-app-view="demo">Demo</a>\n        <a href="#system" data-app-view="system">System</a>\n        <a href="#contact" data-app-view="contact">Contact Us</a>\n      </div>'''
text, count = nav_pattern.subn(nav_html, text, count=1)
if count != 1:
    raise SystemExit("could not replace top navigation")
text = text.replace(
    '<a class="brand" href="#top" aria-label="THU Delivery home">',
    '<a class="brand" href="#overview" data-app-view="overview" aria-label="THU Delivery home">',
    1,
)

css = r'''

    /* ============================================================
       App view shell v1
       Four isolated views; vertical scroll is contained by the active view.
       ============================================================ */
    html, body {
      height: 100%;
      overflow: hidden;
      overscroll-behavior: none;
    }

    body {
      min-height: 100svh;
    }

    .scroll-progress { display: none !important; }

    .app-shell-main {
      height: 100svh;
      padding-top: 64px;
      overflow: hidden;
    }

    .app-view {
      display: none;
      height: calc(100svh - 64px);
      min-height: 0;
      overflow: hidden;
    }

    .app-view.is-active { display: block; }

    .app-view-scroll {
      width: 100%;
      height: 100%;
      min-height: 0;
      overflow-y: auto;
      overflow-x: hidden;
      overscroll-behavior: contain;
      scrollbar-gutter: stable;
    }

    .app-view-scroll-overview { overflow: hidden; }

    .app-nav a {
      position: relative;
      padding: 22px 0 20px;
      color: rgba(225,231,226,.58);
    }

    .app-nav a::after {
      content: "";
      position: absolute;
      left: 0;
      right: 0;
      bottom: 13px;
      height: 1px;
      background: var(--green);
      box-shadow: 0 0 10px rgba(118,185,0,.22);
      transform: scaleX(0);
      transition: transform .22s ease;
    }

    .app-nav a.is-active { color: rgba(245,248,246,.96); }
    .app-nav a.is-active::after { transform: scaleX(1); }

    .app-view-overview .hero {
      min-height: calc(100svh - 64px);
      height: calc(100svh - 64px);
      padding-top: 24px;
      padding-bottom: 24px;
    }

    .app-view-overview .hero-grid {
      min-height: min(640px, calc(100svh - 112px));
    }

    .app-view-demo .demo-video-section {
      min-height: calc(100svh - 64px);
      display: grid;
      align-content: center;
      padding-top: 44px;
      padding-bottom: 44px;
    }

    .app-view-contact .contact-section {
      min-height: calc(100svh - 64px);
      padding-top: 72px;
      padding-bottom: 56px;
    }

    /* System is a workspace: fixed story navigation on the left, scroll rail on the right. */
    .app-view-system {
      background:
        radial-gradient(ellipse at 70% 30%, rgba(118,185,0,.055), transparent 48%),
        linear-gradient(180deg, rgba(13,18,15,.98), rgba(8,11,9,.99));
    }

    .system-view-frame {
      height: 100%;
      min-height: 0;
      display: grid;
      grid-template-rows: auto minmax(0, 1fr);
      gap: 22px;
      padding-top: 28px;
      padding-bottom: 22px;
    }

    .system-view-frame > .system-story-head {
      display: grid;
      grid-template-columns: minmax(0, 760px) minmax(260px, 1fr);
      align-items: end;
      gap: 36px;
      margin: 0;
    }

    .system-view-frame > .system-story-head .eyebrow {
      grid-column: 1 / -1;
      margin: 0 0 -12px;
    }

    .system-view-frame > .system-story-head h2 {
      font-size: clamp(38px, 4.2vw, 58px);
    }

    .system-workspace {
      min-height: 0;
      display: grid;
      grid-template-columns: 168px minmax(0, 1fr);
      gap: 14px;
    }

    .system-story-nav {
      min-height: 0;
      display: flex;
      flex-direction: column;
      align-self: stretch;
      padding: 10px;
      border: 1px solid rgba(241,243,240,.095);
      border-radius: 14px;
      background: rgba(14,20,16,.58);
      backdrop-filter: blur(14px);
      -webkit-backdrop-filter: blur(14px);
    }

    .system-story-nav .story-tab {
      position: relative;
      width: 100%;
      min-height: 62px;
      padding: 0 12px 0 15px;
      border: 0;
      border-bottom: 1px solid rgba(241,243,240,.065);
      background: transparent;
      color: rgba(195,207,198,.46);
      text-align: left;
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 14px;
      letter-spacing: .035em;
      transition: color .2s ease, background .2s ease;
    }

    .system-story-nav .story-tab:last-child { border-bottom: 0; }
    .system-story-nav .story-tab span {
      display: block;
      margin: 0 0 4px;
      color: rgba(118,185,0,.44);
      font-size: 8px;
      letter-spacing: .12em;
    }

    .system-story-nav .story-tab::before {
      content: "";
      position: absolute;
      left: 0;
      top: 14px;
      bottom: 14px;
      width: 2px;
      border-radius: 2px;
      background: var(--green);
      box-shadow: 0 0 12px rgba(118,185,0,.24);
      transform: scaleY(0);
      transition: transform .22s ease;
    }

    .system-story-nav .story-tab::after { display: none; }
    .system-story-nav .story-tab:hover { color: rgba(234,240,236,.78); background: rgba(255,255,255,.018); }
    .system-story-nav .story-tab.is-active { color: rgba(246,249,247,.96); background: rgba(118,185,0,.045); }
    .system-story-nav .story-tab.is-active::before { transform: scaleY(1); }

    .system-story-scroll {
      min-height: 0;
      height: 100%;
      overflow-y: auto;
      overflow-x: hidden;
      scroll-snap-type: y proximity;
      scroll-behavior: smooth;
      overscroll-behavior: contain;
      border: 1px solid rgba(241,243,240,.095);
      border-radius: 16px;
      background:
        linear-gradient(145deg, rgba(20,30,25,.70), rgba(8,13,10,.72));
      box-shadow: inset 0 1px 0 rgba(255,255,255,.025), 0 24px 64px rgba(0,0,0,.16);
    }

    .system-story-scroll > .story-panel {
      min-height: calc(100svh - 248px);
      height: auto;
      scroll-snap-align: start;
      scroll-margin-top: 0;
      border-bottom: 1px solid rgba(241,243,240,.075);
      background: transparent;
      animation: none;
    }

    .system-story-scroll > .story-panel:last-child { border-bottom: 0; }

    .system-story-scroll > .story-panel[hidden] {
      display: grid !important;
    }

    .move-route-lab {
      grid-column: 1 / -1;
      margin-top: 18px;
      padding-top: 28px;
      border-top: 1px solid rgba(241,243,240,.08);
    }

    .move-route-lab > .story-label { margin: 0 0 10px; }
    .move-route-lab .section-head { margin-bottom: 20px; }
    .move-route-lab .section-head h2 { font-size: clamp(30px, 3.5vw, 46px); }
    .move-route-lab .map-lab { margin-bottom: 6px; }
    .move-route-lab .map-stage { min-height: 460px; }

    @media (max-width: 900px) {
      .app-view-scroll-overview { overflow-y: auto; }
      .app-view-overview .hero { height: auto; min-height: calc(100svh - 64px); }
      .system-view-frame { padding-top: 18px; gap: 14px; }
      .system-view-frame > .system-story-head { grid-template-columns: 1fr; gap: 8px; }
      .system-view-frame > .system-story-head .eyebrow { margin-bottom: 0; }
      .system-story-lead { display: none; }
      .system-workspace { grid-template-columns: 1fr; grid-template-rows: auto minmax(0, 1fr); }
      .system-story-nav { flex-direction: row; overflow-x: auto; padding: 6px; }
      .system-story-nav .story-tab { flex: 0 0 112px; min-height: 52px; border-bottom: 0; border-right: 1px solid rgba(241,243,240,.065); }
      .system-story-nav .story-tab::before { left: 12px; right: 12px; top: auto; bottom: 0; width: auto; height: 2px; transform: scaleX(0); }
      .system-story-nav .story-tab.is-active::before { transform: scaleX(1); }
      .system-story-scroll > .story-panel { min-height: calc(100svh - 270px); }
    }

    @media (max-width: 620px) {
      .app-shell-main { padding-top: 58px; }
      .app-view { height: calc(100svh - 58px); }
      .system-view-frame { width: min(100% - 20px, 1180px); }
      .system-view-frame > .system-story-head h2 { font-size: 34px; }
      .move-route-lab .map-stage { min-height: 360px; }
      .app-nav { gap: 14px; font-size: 12px; }
    }
'''

style_marker = '\n  </style>'
if style_marker not in text:
    raise SystemExit("style closing marker missing")
text = text.replace(style_marker, css + style_marker, 1)

# Replace old click-to-hide System tab controller with scroll-linked navigation,
# while preserving the existing feature-video modal behavior.
controller_pattern = re.compile(
    r'    // system story tab controller v1\n    \(\(\) => \{.*?    \}\)\(\);\n\n  </script>',
    re.S,
)

controller = r'''    // top-level app view controller v1
    (() => {
      const links = [...document.querySelectorAll('[data-app-view]')];
      const panels = [...document.querySelectorAll('[data-app-view-panel]')];
      const allowed = new Set(['overview', 'demo', 'system', 'contact']);

      const setView = (key, updateHash = true) => {
        if (!allowed.has(key)) key = 'overview';
        panels.forEach((panel) => {
          const active = panel.dataset.appViewPanel === key;
          panel.classList.toggle('is-active', active);
          panel.setAttribute('aria-hidden', active ? 'false' : 'true');
        });
        links.forEach((link) => {
          link.classList.toggle('is-active', link.dataset.appView === key);
        });
        if (updateHash && location.hash !== `#${key}`) {
          history.pushState(null, '', `#${key}`);
        }
        window.dispatchEvent(new CustomEvent('appviewchange', { detail: { key } }));
      };

      links.forEach((link) => {
        link.addEventListener('click', (event) => {
          event.preventDefault();
          setView(link.dataset.appView || 'overview');
        });
      });

      const fromHash = () => {
        const key = location.hash.replace('#', '');
        setView(allowed.has(key) ? key : 'overview', false);
      };
      window.addEventListener('hashchange', fromHash);
      fromHash();
    })();

    // System story scroll controller v2
    (() => {
      const nav = document.querySelector('.system-story-nav');
      const scroll = document.getElementById('systemStoryScroll');
      const tabs = nav ? [...nav.querySelectorAll('.story-tab')] : [];
      const panels = scroll ? [...scroll.querySelectorAll(':scope > .story-panel')] : [];
      if (!nav || !scroll || !tabs.length || !panels.length) return;

      let activeKey = '';
      let ticking = false;

      const setActive = (key) => {
        if (!key || key === activeKey) return;
        activeKey = key;
        tabs.forEach((tab) => {
          const active = tab.dataset.story === key;
          tab.classList.toggle('is-active', active);
          tab.setAttribute('aria-selected', active ? 'true' : 'false');
        });
      };

      const updateFromScroll = () => {
        ticking = false;
        const rootRect = scroll.getBoundingClientRect();
        const anchor = rootRect.top + Math.min(190, rootRect.height * .28);
        let current = panels[0];
        for (const panel of panels) {
          const rect = panel.getBoundingClientRect();
          if (rect.top <= anchor && rect.bottom > rootRect.top + 60) current = panel;
        }
        setActive(current.dataset.panel || '');
      };

      const requestUpdate = () => {
        if (ticking) return;
        ticking = true;
        requestAnimationFrame(updateFromScroll);
      };

      tabs.forEach((tab) => {
        tab.addEventListener('click', () => {
          const target = panels.find((panel) => panel.dataset.panel === tab.dataset.story);
          if (!target) return;
          scroll.scrollTo({ top: target.offsetTop, behavior: 'smooth' });
          setActive(tab.dataset.story || '');
        });
      });

      // Wheel over the left rail still advances the right System story rail.
      nav.addEventListener('wheel', (event) => {
        if (Math.abs(event.deltaY) < 1) return;
        event.preventDefault();
        scroll.scrollBy({ top: event.deltaY, behavior: 'auto' });
      }, { passive: false });

      scroll.addEventListener('scroll', requestUpdate, { passive: true });
      window.addEventListener('resize', requestUpdate);
      window.addEventListener('appviewchange', (event) => {
        if (event.detail?.key === 'system') requestAnimationFrame(updateFromScroll);
      });
      updateFromScroll();

      const modal = document.getElementById('featureVideoModal');
      const video = document.getElementById('featureVideo');
      const title = document.getElementById('featureVideoTitle');
      const copy = document.getElementById('featureVideoCopy');
      const fallback = document.getElementById('featureVideoFallback');
      const pathText = document.getElementById('featureVideoPath');
      const closeButtons = modal ? [...modal.querySelectorAll('.feature-video-close, .feature-video-backdrop')] : [];
      let lastTrigger = null;

      const closeModal = () => {
        if (!modal) return;
        modal.classList.remove('is-open');
        modal.setAttribute('aria-hidden', 'true');
        document.body.classList.remove('feature-video-open');
        if (video) {
          video.pause();
          video.removeAttribute('src');
          video.load();
          video.hidden = false;
        }
        if (fallback) fallback.hidden = true;
        if (lastTrigger) lastTrigger.focus({ preventScroll: true });
      };

      document.querySelectorAll('.feature-video-trigger').forEach((trigger) => {
        trigger.addEventListener('click', () => {
          if (!modal || !video) return;
          lastTrigger = trigger;
          const src = trigger.dataset.video || '';
          title.textContent = trigger.dataset.title || 'Function demo';
          copy.textContent = trigger.dataset.copy || '';
          pathText.textContent = src;
          fallback.hidden = true;
          video.hidden = false;
          video.pause();
          video.src = src;
          video.load();
          modal.classList.add('is-open');
          modal.setAttribute('aria-hidden', 'false');
          document.body.classList.add('feature-video-open');
          setTimeout(() => modal.querySelector('.feature-video-close')?.focus(), 0);
          video.play().catch(() => {});
        });
      });

      video?.addEventListener('error', () => {
        video.hidden = true;
        fallback.hidden = false;
      });
      video?.addEventListener('loadeddata', () => {
        video.hidden = false;
        fallback.hidden = true;
      });
      closeButtons.forEach((button) => button.addEventListener('click', closeModal));
      document.addEventListener('keydown', (event) => {
        if (event.key === 'Escape' && modal?.classList.contains('is-open')) closeModal();
      });
    })();

  </script>'''

text, count = controller_pattern.subn(controller, text, count=1)
if count != 1:
    raise SystemExit("could not replace System story controller")

path.write_text(text, encoding="utf-8")
