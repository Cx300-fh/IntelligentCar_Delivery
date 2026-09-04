const fs = require('fs');
const path = 'docs/index.html';
let s = fs.readFileSync(path, 'utf8');

const oldNote = '<div class="future-note">EEG demo slot is intentionally reserved. When the brain-wave vehicle video is uploaded to assets/media, this block can become a real playable evidence panel without changing the four-module structure.</div>';
const newCta = `<div class="system-explore-more">
  <span class="system-explore-kicker">Beyond the system</span>
  <a class="system-explore-link" href="explore.html">
    <span>Explore More</span><b aria-hidden="true">&nearr;</b>
  </a>
  <p>Design origin, human stories, and the thinking behind THU Delivery.</p>
</div>`;

if (!s.includes(oldNote)) throw new Error('EEG placeholder note not found');
s = s.replace(oldNote, newCta);

const marker = '<!-- EXPLORE MORE CTA v1 -->';
if (!s.includes(marker)) {
  const css = `
${marker}
<style>
  .system-explore-more {
    width: min(760px, 100%);
    margin: 72px auto 8px;
    padding: 44px 20px 18px;
    text-align: center;
    border-top: 1px solid rgba(241,243,240,.10);
  }
  .system-explore-kicker {
    display: block;
    margin-bottom: 14px;
    color: rgba(118,185,0,.72);
    font-size: 10px;
    letter-spacing: .18em;
    text-transform: uppercase;
  }
  .system-explore-link {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 18px;
    min-height: 54px;
    padding: 0 28px;
    border: 1px solid rgba(118,185,0,.55);
    border-radius: 999px;
    background: linear-gradient(135deg, rgba(118,185,0,.13), rgba(241,243,240,.025));
    box-shadow: inset 0 1px 0 rgba(255,255,255,.04), 0 18px 60px rgba(0,0,0,.18);
    color: rgba(246,249,247,.96);
    text-decoration: none;
    font-size: 15px;
    font-weight: 650;
    letter-spacing: .025em;
    transition: transform .25s ease, border-color .25s ease, background .25s ease, box-shadow .25s ease;
  }
  .system-explore-link b {
    color: var(--green);
    font-size: 17px;
    font-weight: 400;
  }
  .system-explore-link:hover {
    transform: translateY(-2px);
    border-color: rgba(118,185,0,.88);
    background: linear-gradient(135deg, rgba(118,185,0,.20), rgba(241,243,240,.04));
    box-shadow: 0 20px 70px rgba(0,0,0,.25), 0 0 28px rgba(118,185,0,.08);
  }
  .system-explore-more p {
    max-width: 500px;
    margin: 14px auto 0;
    color: rgba(222,228,223,.46);
    font-size: 11px;
    line-height: 1.6;
  }
</style>
`;
  if (!s.includes('</body>')) throw new Error('Missing body close tag');
  s = s.replace('</body>', css + '\n</body>');
}

fs.writeFileSync(path, s, 'utf8');
