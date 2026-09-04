from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

old = 'data-proxy-code="safety"><span class="node-type">Control</span><strong>Navigation + 5 ms loop</strong><small>FSM and motor command</small></button>'
new = 'data-campus-demo="1"><span class="node-type">Video</span><strong>Navigation + 5 ms loop</strong><small>Real campus navigation demo</small></button>'
if old not in text:
    raise SystemExit('Navigation node pattern not found')
text = text.replace(old, new, 1)

marker_start = '<!-- NAVIGATION CAMPUS DEMO v1 BEGIN -->'
marker_end = '<!-- NAVIGATION CAMPUS DEMO v1 END -->'
if marker_start in text:
    start = text.index(marker_start)
    end = text.index(marker_end, start) + len(marker_end)
    text = text[:start] + text[end:]

block = r'''
<!-- NAVIGATION CAMPUS DEMO v1 BEGIN -->
<style>
  .nav-demo-modal {
    position: fixed;
    inset: 0;
    z-index: 160;
    display: none;
    place-items: center;
    padding: 28px;
  }
  .nav-demo-modal.is-open { display: grid; }
  .nav-demo-backdrop {
    position: absolute;
    inset: 0;
    background: rgba(2,6,4,.74);
    backdrop-filter: blur(18px);
  }
  .nav-demo-window {
    position: relative;
    width: min(1060px, 94vw);
    max-height: 88vh;
    overflow: hidden;
    border: 1px solid rgba(241,243,240,.13);
    border-radius: 22px;
    background: rgba(8,13,10,.94);
    box-shadow: 0 35px 100px rgba(0,0,0,.55);
  }
  .nav-demo-topbar {
    height: 48px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 14px 0 18px;
    border-bottom: 1px solid rgba(241,243,240,.08);
  }
  .nav-demo-topbar span { color: rgba(235,240,235,.68); font-size: 12px; }
  .nav-demo-close {
    width: 32px;
    height: 32px;
    border: 0;
    border-radius: 50%;
    background: rgba(241,243,240,.06);
    color: #f1f3f0;
    cursor: pointer !important;
  }
  .nav-demo-window video {
    display: block;
    width: 100%;
    max-height: calc(88vh - 48px);
    background: #050706;
    object-fit: contain;
  }
  body.nav-demo-open { overflow: hidden; }
  body.nav-demo-open, body.nav-demo-open * { cursor: auto !important; }
  body.nav-demo-open button, body.nav-demo-open .system-node { cursor: pointer !important; }
</style>
<div class="nav-demo-modal" id="navDemoModal" aria-hidden="true">
  <div class="nav-demo-backdrop"></div>
  <div class="nav-demo-window" role="dialog" aria-modal="true" aria-labelledby="navDemoTitle">
    <div class="nav-demo-topbar"><span id="navDemoTitle">Navigation · Campus Demo</span><button class="nav-demo-close" type="button" aria-label="Close video">×</button></div>
    <video src="assets/media/campus-demo.mp4" controls muted playsinline preload="metadata"></video>
  </div>
</div>
<script>
(() => {
  const modal = document.getElementById('navDemoModal');
  const video = modal?.querySelector('video');
  if (!modal || !video) return;
  let lastTrigger = null;
  const open = (trigger) => {
    lastTrigger = trigger;
    modal.classList.add('is-open');
    modal.setAttribute('aria-hidden','false');
    document.body.classList.add('nav-demo-open');
    video.currentTime = 0;
    video.play().catch(() => {});
    setTimeout(() => modal.querySelector('.nav-demo-close')?.focus(), 0);
  };
  const close = () => {
    video.pause();
    modal.classList.remove('is-open');
    modal.setAttribute('aria-hidden','true');
    document.body.classList.remove('nav-demo-open');
    lastTrigger?.focus({preventScroll:true});
  };
  document.addEventListener('click', (e) => {
    const trigger = e.target.closest('[data-campus-demo]');
    if (trigger) { open(trigger); return; }
    if (e.target.closest('.nav-demo-close') || e.target.classList.contains('nav-demo-backdrop')) close();
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && modal.classList.contains('is-open')) close();
  });
})();
</script>
<!-- NAVIGATION CAMPUS DEMO v1 END -->
'''

text = text.replace('</body>', block + '\n</body>')
path.write_text(text, encoding='utf-8')
