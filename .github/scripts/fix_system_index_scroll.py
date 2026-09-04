from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

marker_start = '<!-- SYSTEM INDEX + SCROLL FIX v1 BEGIN -->'
marker_end = '<!-- SYSTEM INDEX + SCROLL FIX v1 END -->'

if marker_start in text:
    start = text.index(marker_start)
    end = text.index(marker_end, start) + len(marker_end)
    text = text[:start] + text[end:]

block = r'''
<!-- SYSTEM INDEX + SCROLL FIX v1 BEGIN -->
<style>
  /* The whole System view scrolls as one document. The title is not part of the left index. */
  .app-view-system .system-view-frame {
    height: calc(100svh - 64px);
    overflow-y: auto !important;
    overflow-x: hidden;
    scrollbar-width: thin;
    scrollbar-color: rgba(118,185,0,.22) transparent;
  }
  .app-view-system .system-view-frame::-webkit-scrollbar { width: 6px; }
  .app-view-system .system-view-frame::-webkit-scrollbar-thumb {
    background: rgba(118,185,0,.22);
    border-radius: 999px;
  }

  .app-view-system .system-story-head {
    position: relative !important;
    inset: auto !important;
    width: 100% !important;
    margin: 0 0 28px !important;
    padding: 22px 0 20px !important;
    transform: none !important;
    z-index: 1;
  }
  .app-view-system .system-story-head h2 {
    white-space: nowrap;
    max-width: none !important;
    font-size: clamp(42px, 5.1vw, 76px);
  }

  .app-view-system .system-workspace {
    align-items: start;
    min-height: 0 !important;
  }
  .app-view-system .system-story-nav {
    position: sticky !important;
    top: 22px;
    align-self: start;
    height: max-content;
  }
  .app-view-system .system-story-scroll {
    height: auto !important;
    max-height: none !important;
    min-height: 0 !important;
    overflow: visible !important;
    overscroll-behavior: auto !important;
    padding-bottom: 70px !important;
  }

  @media (max-width: 920px) {
    .app-view-system .system-view-frame { height: calc(100svh - 64px); }
    .app-view-system .system-story-head h2 { white-space: normal; }
    .app-view-system .system-story-nav { position: relative !important; top: auto; }
  }
</style>
<script>
(() => {
  const init = () => {
    const frame = document.querySelector('.app-view-system .system-view-frame');
    const workspace = document.querySelector('.app-view-system .system-workspace');
    const head = document.querySelector('.app-view-system .system-story-head');
    const nav = document.querySelector('.app-view-system .system-story-nav');
    if (!frame || !workspace || !head || !nav) return;

    // Keep the page title above the two-column workspace, never inside the left index.
    if (head.parentElement !== frame || head.nextElementSibling !== workspace) {
      frame.insertBefore(head, workspace);
    }

    const desired = [
      ['order', '01', 'Order'],
      ['system', '02', 'System'],
      ['safety', '03', 'Safety'],
      ['future', '04', 'To-do']
    ];
    desired.forEach(([key, num, label]) => {
      const tab = nav.querySelector(`[data-four="${key}"]`);
      if (!tab) return;
      tab.innerHTML = `<span>${num}</span>${label}`;
      tab.setAttribute('aria-label', label);
    });

    // The frame is now the scrolling surface; keep the left index synchronized with it.
    const tabs = [...nav.querySelectorAll('[data-four]')];
    const sections = [...document.querySelectorAll('.system-four-stage [data-four-panel]')];
    const setActive = (key) => tabs.forEach(tab => {
      const on = tab.dataset.four === key;
      tab.classList.toggle('is-active', on);
      tab.setAttribute('aria-selected', on ? 'true' : 'false');
    });

    tabs.forEach(tab => {
      tab.onclick = () => {
        const section = document.querySelector(`.system-four-stage [data-four-panel="${tab.dataset.four}"]`);
        if (!section) return;
        const targetTop = section.getBoundingClientRect().top - frame.getBoundingClientRect().top + frame.scrollTop - 18;
        frame.scrollTo({ top: targetTop, behavior: 'smooth' });
        setActive(tab.dataset.four);
      };
    });

    let raf = 0;
    frame.addEventListener('scroll', () => {
      if (raf) return;
      raf = requestAnimationFrame(() => {
        raf = 0;
        const marker = frame.getBoundingClientRect().top + 150;
        let current = sections[0];
        let best = Infinity;
        sections.forEach(section => {
          const d = Math.abs(section.getBoundingClientRect().top - marker);
          if (d < best) { best = d; current = section; }
        });
        if (current) setActive(current.dataset.fourPanel);
      });
    }, { passive: true });
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => setTimeout(init, 0));
  } else {
    setTimeout(init, 0);
  }
})();
</script>
<!-- SYSTEM INDEX + SCROLL FIX v1 END -->
'''

text = text.replace('</body>', block + '\n</body>')
path.write_text(text, encoding='utf-8')
