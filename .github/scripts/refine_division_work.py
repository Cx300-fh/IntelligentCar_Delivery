from pathlib import Path

path = Path("docs/index.html")
text = path.read_text(encoding="utf-8")

MARKER = "Division work cards v2"
if MARKER in text:
    raise SystemExit("division refinement already applied")

bios = {
    "Gan Hongwen": "I make the car move reliably and keep its networked sensing connected.",
    "Lim Zhitong": "I shape the frame and turn system logic into a clear interface.",
    "Li Yuxin": "I connect control, networking, UI, and presentation into one coherent system.",
    "Saw Liyun": "I focus on route weighting, obstacle handling, and interaction details.",
    "Pu Renzhi": "I build voice interaction and camera perception for a more natural delivery experience.",
}

for name, bio in bios.items():
    old = f'<span class="member-avatar" title="{name}" aria-label="{name}">'
    new = (
        f'<span class="member-avatar" tabindex="0" aria-label="{name}" '
        f'data-name="{name}" data-bio="{bio}">'
    )
    count = text.count(old)
    if count == 0:
        raise SystemExit(f"avatar markup not found for {name}")
    text = text.replace(old, new)

css = r'''

    /* ============================================================
       Division work cards v2
       Readable translucent role tiles + member bio hover cards.
       ============================================================ */
    .responsibility-panel {
      padding-bottom: 18px;
      overflow: visible;
    }

    .responsibility-head {
      padding: 29px 29px 18px;
      border-bottom: 0;
    }

    .responsibility-head h3 {
      font-size: clamp(30px, 3vw, 42px);
      line-height: 1;
      letter-spacing: .015em;
    }

    .responsibility-grid {
      display: grid;
      grid-template-columns: repeat(5, minmax(0, 1fr));
      gap: 10px;
      padding: 0 18px;
    }

    .responsibility-item,
    .responsibility-item:nth-child(2n),
    .responsibility-item:nth-child(5n),
    .responsibility-item:nth-last-child(-n + 2),
    .responsibility-item:nth-last-child(-n + 5) {
      position: relative;
      min-height: 134px;
      padding: 17px 16px 16px;
      border: 1px solid rgba(215,235,223,.105) !important;
      border-radius: 14px;
      background:
        linear-gradient(145deg, rgba(28,43,35,.43), rgba(8,16,12,.31));
      box-shadow:
        inset 0 1px 0 rgba(255,255,255,.025),
        0 12px 30px rgba(0,0,0,.10);
      overflow: visible;
      transition:
        transform .26s cubic-bezier(.2,.75,.2,1),
        border-color .26s ease,
        background .26s ease,
        box-shadow .26s ease;
    }

    .responsibility-item::after {
      content: "";
      position: absolute;
      inset: 0;
      border-radius: inherit;
      pointer-events: none;
      background: radial-gradient(220px circle at 22% 0%, rgba(118,185,0,.055), transparent 64%);
      opacity: .65;
    }

    .responsibility-item:hover {
      transform: translateY(-3px);
      border-color: rgba(156,207,92,.22) !important;
      background:
        linear-gradient(145deg, rgba(32,49,39,.50), rgba(9,18,13,.37));
      box-shadow:
        inset 0 1px 0 rgba(255,255,255,.035),
        0 17px 36px rgba(0,0,0,.16);
    }

    .role-title {
      position: relative;
      z-index: 1;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      min-height: 34px;
      margin: 0;
      padding: 7px 10px;
      border: 1px solid rgba(213,230,219,.105);
      border-radius: 8px;
      background: rgba(211,229,216,.045);
      color: rgba(240,246,242,.92);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 16px;
      line-height: 1;
      letter-spacing: .035em;
      text-transform: none;
      white-space: nowrap;
    }

    .role-title::before {
      content: "";
      width: 5px;
      height: 5px;
      flex: 0 0 5px;
      border-radius: 50%;
      background: rgba(145,205,48,.92);
      box-shadow: 0 0 9px rgba(118,185,0,.28);
    }

    .role-members {
      position: relative;
      z-index: 2;
      min-height: 46px;
      margin-top: 18px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .member-avatar {
      width: 46px;
      height: 46px;
      flex: 0 0 46px;
      cursor: help;
      border-color: rgba(166,214,102,.30);
      box-shadow:
        inset 0 1px 0 rgba(255,255,255,.055),
        0 0 0 2px rgba(118,185,0,.02);
      outline: none;
    }

    .member-avatar:hover,
    .member-avatar:focus-visible {
      z-index: 5;
      transform: translateY(-3px) scale(1.08);
      border-color: rgba(180,227,113,.70);
      box-shadow:
        0 0 0 3px rgba(118,185,0,.055),
        0 10px 24px rgba(0,0,0,.24),
        0 0 18px rgba(118,185,0,.10);
    }

    .member-bio-tooltip {
      position: fixed;
      z-index: 1200;
      width: min(292px, calc(100vw - 24px));
      padding: 13px 14px 14px;
      border: 1px solid rgba(184,220,155,.20);
      border-radius: 12px;
      background:
        linear-gradient(145deg, rgba(17,29,23,.97), rgba(7,14,10,.97));
      box-shadow:
        0 22px 56px rgba(0,0,0,.38),
        inset 0 1px 0 rgba(255,255,255,.035);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      pointer-events: none;
      opacity: 0;
      visibility: hidden;
      transform: translateY(4px) scale(.98);
      transition: opacity .15s ease, transform .15s ease, visibility .15s ease;
    }

    .member-bio-tooltip.is-visible {
      opacity: 1;
      visibility: visible;
      transform: translateY(0) scale(1);
    }

    .member-bio-tooltip .bio-name {
      display: block;
      margin-bottom: 6px;
      color: rgba(243,248,244,.96);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 17px;
      line-height: 1.05;
    }

    .member-bio-tooltip .bio-copy {
      display: block;
      color: rgba(199,214,204,.68);
      font-size: 12px;
      line-height: 1.58;
    }

    @media (max-width: 1040px) {
      .responsibility-grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }
    }

    @media (max-width: 560px) {
      .responsibility-head {
        padding: 24px 18px 16px;
      }

      .responsibility-grid {
        grid-template-columns: 1fr;
        padding: 0 12px;
      }

      .responsibility-item,
      .responsibility-item:nth-child(2n),
      .responsibility-item:nth-child(5n),
      .responsibility-item:nth-last-child(-n + 2),
      .responsibility-item:nth-last-child(-n + 5) {
        min-height: 118px;
      }

      .role-title { font-size: 17px; }
    }
'''

style_end = "\n  </style>\n</head>"
if style_end not in text:
    raise SystemExit("style closing marker not found")
text = text.replace(style_end, css + style_end, 1)

js = r'''

  <script>
    (() => {
      const avatars = [...document.querySelectorAll('.responsibility-panel .member-avatar[data-bio]')];
      if (!avatars.length) return;

      const tooltip = document.createElement('div');
      tooltip.className = 'member-bio-tooltip';
      tooltip.setAttribute('role', 'tooltip');
      tooltip.setAttribute('aria-hidden', 'true');
      tooltip.innerHTML = '<strong class="bio-name"></strong><span class="bio-copy"></span>';
      document.body.appendChild(tooltip);

      const nameEl = tooltip.querySelector('.bio-name');
      const copyEl = tooltip.querySelector('.bio-copy');

      function place(anchor) {
        const r = anchor.getBoundingClientRect();
        const pad = 12;
        const width = Math.min(292, window.innerWidth - pad * 2);
        tooltip.style.width = `${width}px`;
        const measured = tooltip.getBoundingClientRect();
        let left = r.left + r.width / 2 - width / 2;
        left = Math.max(pad, Math.min(window.innerWidth - width - pad, left));
        let top = r.top - measured.height - 12;
        if (top < pad) top = r.bottom + 12;
        tooltip.style.left = `${left}px`;
        tooltip.style.top = `${top}px`;
      }

      function show(anchor) {
        nameEl.textContent = anchor.dataset.name || '';
        copyEl.textContent = anchor.dataset.bio || '';
        tooltip.setAttribute('aria-hidden', 'false');
        tooltip.classList.add('is-visible');
        requestAnimationFrame(() => place(anchor));
      }

      function hide() {
        tooltip.classList.remove('is-visible');
        tooltip.setAttribute('aria-hidden', 'true');
      }

      avatars.forEach((avatar) => {
        avatar.addEventListener('mouseenter', () => show(avatar));
        avatar.addEventListener('mouseleave', hide);
        avatar.addEventListener('focus', () => show(avatar));
        avatar.addEventListener('blur', hide);
      });

      window.addEventListener('scroll', hide, { passive: true });
      window.addEventListener('resize', hide);
    })();
  </script>
'''

body_end = "\n</body>"
if body_end not in text:
    raise SystemExit("body closing marker not found")
text = text.replace(body_end, js + body_end, 1)

path.write_text(text, encoding="utf-8")
