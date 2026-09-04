from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

old_shell = '<article class="story-feature is-static story-feature-shell"><div class="story-feature-top"><span>06</span></div><h4>Frame & shell</h4><p>The enclosure protects electronics, stabilizes the cargo layout and turns the prototype into a usable product.</p><div class="feature-visual" aria-hidden="true"><span></span><span></span><span></span><span></span><b></b></div></article>'
new_shell = '<article class="story-feature is-video story-feature-shell"><div class="story-feature-top"><span>06</span><button class="feature-video-trigger" type="button" data-video="assets/media/frame.mp4" data-title="Frame & Shell" data-copy="Real vehicle walkthrough of the enclosure, cargo layout and physical packaging." aria-label="Play Frame and Shell video"></button></div><h4>Frame & shell</h4><p>The enclosure protects electronics, stabilizes the cargo layout and turns the prototype into a usable product.</p><div class="feature-visual" aria-hidden="true"><span></span><span></span><span></span><span></span><b></b></div></article>'
if old_shell in text:
    text = text.replace(old_shell, new_shell, 1)

old_chips = '<div class="robot-core-chips"><span>Frame & shell</span><span>Motor drive</span><span>Steering</span><span>On-car display</span><span>Cargo bay</span></div>'
new_chips = '<div class="robot-core-chips"><button type="button" class="robot-chip-action" data-proxy-video="frame.mp4">Frame & shell</button><span>Motor drive</span><span>Steering</span><span>On-car display</span><span>Cargo bay</span></div>'
if old_chips in text:
    text = text.replace(old_chips, new_chips, 1)

old_future = '<article class="future-main">\n          <span class="future-status">Video incoming</span>'
new_future = '<article class="future-main eeg-future-card" data-eeg-video="assets/media/eeg-demo.mp4" tabindex="-1">\n          <span class="future-status">Video incoming</span>'
if old_future in text:
    text = text.replace(old_future, new_future, 1)

start = '<!-- FRAME + EEG + INDEX LOCK v1 BEGIN -->'
end = '<!-- FRAME + EEG + INDEX LOCK v1 END -->'
if start in text:
    a = text.index(start)
    b = text.index(end, a) + len(end)
    text = text[:a] + text[b:]

block = r'''
<!-- FRAME + EEG + INDEX LOCK v1 BEGIN -->
<style>
  /* Keep the functional System index visible for the whole System document. */
  .app-view-system .system-story-nav {
    position: sticky !important;
    top: 24px !important;
    align-self: flex-start !important;
    height: max-content !important;
    z-index: 12 !important;
  }
  .app-view-system .system-workspace { align-items: stretch !important; }

  .robot-core-chips .robot-chip-action {
    appearance: none;
    border: 1px solid rgba(241,243,240,.10);
    border-radius: 999px;
    background: rgba(7,12,9,.58);
    color: rgba(229,234,229,.68);
    padding: 5px 8px;
    font: inherit;
    font-size: 8px;
    cursor: pointer;
    backdrop-filter: blur(10px);
    transition: border-color .2s ease, color .2s ease, transform .2s ease;
  }
  .robot-core-chips .robot-chip-action:hover {
    border-color: rgba(118,185,0,.45);
    color: #f1f3f0;
    transform: translateY(-1px);
  }

  .eeg-future-card.is-video-ready {
    cursor: pointer;
    border-color: rgba(118,185,0,.24);
  }
  .eeg-future-card.is-video-ready:hover {
    border-color: rgba(118,185,0,.5);
    box-shadow: 0 18px 54px rgba(0,0,0,.18), inset 0 1px rgba(255,255,255,.025);
  }
  .eeg-future-card.is-video-ready .future-status {
    background: rgba(118,185,0,.08);
  }
</style>
<script>
(() => {
  const init = () => {
    const card = document.querySelector('.eeg-future-card[data-eeg-video]');
    if (!card) return;
    const src = card.dataset.eegVideo;
    const status = card.querySelector('.future-status');

    fetch(src, { method: 'HEAD', cache: 'no-store' }).then(res => {
      if (!res.ok) return;
      card.classList.add('is-video-ready');
      card.tabIndex = 0;
      if (status) status.textContent = 'Watch demo ↗';
    }).catch(() => {});

    const open = () => {
      if (!card.classList.contains('is-video-ready')) return;
      const modal = document.getElementById('featureVideoModal');
      const video = document.getElementById('featureVideo');
      const title = document.getElementById('featureVideoTitle');
      const copy = document.getElementById('featureVideoCopy');
      const fallback = document.getElementById('featureVideoFallback');
      const path = document.getElementById('featureVideoPath');
      if (!modal || !video) return;
      if (title) title.textContent = 'Brain-wave / EEG controlled car';
      if (copy) copy.textContent = 'Forward motion, turning and emergency avoidance driven by the EEG control prototype.';
      if (path) path.textContent = src;
      if (fallback) fallback.hidden = true;
      video.hidden = false;
      video.pause();
      video.src = src;
      video.load();
      modal.classList.add('is-open');
      modal.setAttribute('aria-hidden', 'false');
      document.body.classList.add('feature-video-open');
      video.play().catch(() => {});
    };

    card.addEventListener('click', open);
    card.addEventListener('keydown', e => {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); open(); }
    });
  };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
</script>
<!-- FRAME + EEG + INDEX LOCK v1 END -->
'''

text = text.replace('</body>', block + '\n</body>')
path.write_text(text, encoding='utf-8')
