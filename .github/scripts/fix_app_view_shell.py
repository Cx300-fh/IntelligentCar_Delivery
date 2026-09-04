from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

needle = '''    .app-view {
      display: none;
      height: calc(100svh - 64px);
      min-height: 0;
      overflow: hidden;
    }
'''
replacement = '''    .app-view {
      display: none;
      height: calc(100svh - 64px);
      min-height: 0;
      padding: 0;
      overflow: hidden;
    }
'''

if needle not in text:
    raise SystemExit('app-view base block not found')
text = text.replace(needle, replacement, 1)

# Add an explicit late cascade guard so the legacy global `section { padding: ... }`
# can never re-apply spacing to the top-level tab shells.
marker = '''    /* Four-view viewport fill v3
       Every top-level tab owns the full viewport below the fixed navigation. */
'''
guard = '''    /* Top-level app views are layout shells, not content sections.
       Reset legacy global section spacing at the shell boundary. */
    main.app-shell-main > section.app-view {
      padding: 0 !important;
      margin: 0 !important;
    }

    /* Four-view viewport fill v3
       Every top-level tab owns the full viewport below the fixed navigation. */
'''
if marker not in text:
    raise SystemExit('viewport marker not found')
text = text.replace(marker, guard, 1)

path.write_text(text, encoding='utf-8')
