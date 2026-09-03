from pathlib import Path
import re

path = Path("docs/index.html")
text = path.read_text(encoding="utf-8")

# Remove the descriptive sentence under Contact Us.
text, n = re.subn(
    r'\n\s*<p class="contact-lead">From motion and control to voice, perception, networking, and interface design, each part of THU Delivery is owned end to end by the team\.</p>',
    '',
    text,
    count=1,
)
if n != 1:
    raise SystemExit(f"contact lead replacement count={n}")

# Remove the green responsibility line above each profile name.
text, n = re.subn(r'\n\s*<span class="team-index">.*?</span>', '', text)
if n != 5:
    raise SystemExit(f"team-index removal count={n}")

# Use English names consistently in profile cards.
replacements = {
    '<img src="assets/team/gan-hongwen.png" alt="甘闳文" loading="lazy">': '<img src="assets/team/gan-hongwen.png" alt="Gan Hongwen" loading="lazy">',
    '<h3 class="team-name zh-name">甘闳文</h3>': '<h3 class="team-name">Gan Hongwen</h3>',
    '<img src="assets/team/li-yuxin.png" alt="李雨欣" loading="lazy">': '<img src="assets/team/li-yuxin.png" alt="Li Yuxin" loading="lazy">',
    '<h3 class="team-name zh-name">李雨欣</h3>': '<h3 class="team-name">Li Yuxin</h3>',
    '<img src="assets/team/pu-renzhi.png" alt="蒲仁智" loading="lazy">': '<img src="assets/team/pu-renzhi.png" alt="Pu Renzhi" loading="lazy">',
    '<h3 class="team-name zh-name">蒲仁智</h3>': '<h3 class="team-name">Pu Renzhi</h3>',
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f"missing expected profile fragment: {old[:60]}")
    text = text.replace(old, new, 1)

# Simplify the work division header.
old_head = '''        <div class="responsibility-head">
          <div>
            <p class="responsibility-kicker">Division of Work</p>
            <h3>Clear ownership across the stack.</h3>
          </div>
          <span class="workstream-count">10 WORKSTREAMS</span>
        </div>'''
new_head = '''        <div class="responsibility-head">
          <h3>Division of Work</h3>
        </div>'''
if old_head not in text:
    raise SystemExit("responsibility heading block not found")
text = text.replace(old_head, new_head, 1)

# Avatar chips for workstream ownership.
avatar = {
    "ghw": '<span class="member-avatar" title="Gan Hongwen" aria-label="Gan Hongwen"><img src="assets/team/gan-hongwen.png" alt=""></span>',
    "lzt": '<span class="member-avatar" title="Lim Zhitong" aria-label="Lim Zhitong"><img src="assets/team/zhi-tong.png" alt=""></span>',
    "lyx": '<span class="member-avatar" title="Li Yuxin" aria-label="Li Yuxin"><img src="assets/team/li-yuxin.png" alt=""></span>',
    "sly": '<span class="member-avatar" title="Saw Liyun" aria-label="Saw Liyun"><img src="assets/team/su-liyun.png" alt=""></span>',
    "prz": '<span class="member-avatar" title="Pu Renzhi" aria-label="Pu Renzhi"><img src="assets/team/pu-renzhi.png" alt=""></span>',
}
roles = [
    ("Motion", ["ghw"]),
    ("Control", ["lyx", "ghw"]),
    ("Voice", ["prz"]),
    ("Weight", ["sly", "lzt"]),
    ("UI Design", ["lzt", "sly", "lyx"]),
    ("Presentation", ["lyx"]),
    ("Net", ["lyx", "ghw"]),
    ("Camera", ["prz", "ghw"]),
    ("Frame", ["lzt"]),
    ("Obstacle", ["sly"]),
]

for role, people in roles:
    pattern = re.compile(
        rf'<div class="responsibility-item"><span class="role-title">{re.escape(role)}</span><p class="role-members">.*?</p></div>'
    )
    avatar_html = ''.join(avatar[p] for p in people)
    replacement = (
        f'<div class="responsibility-item"><span class="role-title">{role}</span>'
        f'<div class="role-members">{avatar_html}</div></div>'
    )
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise SystemExit(f"workstream replacement failed for {role}: {count}")

# Replace text-member styling with portrait chips.
old_css = '''    .role-members {
      margin: 0;
      color: rgba(235,241,237,.82);
      font-size: 13px;
      line-height: 1.55;
    }'''
new_css = '''    .role-members {
      margin: 0;
      min-height: 40px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .member-avatar {
      position: relative;
      display: inline-flex;
      width: 38px;
      height: 38px;
      flex: 0 0 38px;
      overflow: hidden;
      border: 1px solid rgba(154,214,58,.34);
      border-radius: 50%;
      background: rgba(9,17,13,.9);
      box-shadow:
        inset 0 1px 0 rgba(255,255,255,.06),
        0 0 0 2px rgba(118,185,0,.025),
        0 0 16px rgba(118,185,0,.055);
      transition:
        transform .24s cubic-bezier(.2,.75,.2,1),
        border-color .24s ease,
        box-shadow .24s ease;
    }

    .member-avatar img {
      display: block;
      width: 100%;
      height: 100%;
      object-fit: cover;
      object-position: 50% 30%;
      filter: saturate(.92) brightness(.96);
    }

    .member-avatar:hover {
      z-index: 2;
      transform: translateY(-2px) scale(1.08);
      border-color: rgba(166,229,65,.72);
      box-shadow: 0 0 20px rgba(118,185,0,.14);
    }'''
if old_css not in text:
    raise SystemExit("role-members CSS marker not found")
text = text.replace(old_css, new_css, 1)

# Make the simplified heading a clean single title row.
text = text.replace(
    '''    .responsibility-head {
      position: relative;
      z-index: 1;
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 18px;
      padding: 24px 25px 21px;
      border-bottom: 1px solid rgba(215,235,223,.09);
    }''',
    '''    .responsibility-head {
      position: relative;
      z-index: 1;
      display: block;
      padding: 24px 25px 21px;
      border-bottom: 1px solid rgba(215,235,223,.09);
    }''',
    1,
)

path.write_text(text, encoding="utf-8")
