from pathlib import Path

p = Path('docs/explore.html')
s = p.read_text(encoding='utf-8')

marker = '/* EXPLORE FULL VIEWPORT CHAPTERS v1 */'
if marker in s:
    raise SystemExit('already applied')

css = r'''

/* EXPLORE FULL VIEWPORT CHAPTERS v1 */
html {
  scroll-snap-type: y proximity;
  scroll-padding-top: 0;
}
body {
  min-height: 100%;
}
body > header,
body > section {
  scroll-snap-align: start;
}

/* The navigation is an overlay, never a layout strip. */
.page-nav {
  background: transparent !important;
  pointer-events: none;
}
.page-nav a {
  pointer-events: auto;
}

/* Film-like chapters occupy the complete browser viewport. */
.hero,
.film,
.poem,
.future {
  width: 100%;
  height: 100svh;
  min-height: 100svh;
  max-height: none;
}

/* Editorial chapters always start as a complete page, but may grow when copy needs it. */
.why-section,
.principles,
.portrait-section,
.closing {
  min-height: 100svh;
}

.why-section {
  display: flex;
  flex-direction: column;
  justify-content: center;
}
.principles,
.portrait-section {
  display: flex;
  align-items: center;
}
.principles-inner,
.portrait-inner {
  width: 100%;
}

/* Give the final chapter a full-page ending instead of a short footer band. */
.closing {
  display: grid;
  place-items: center;
  align-content: center;
  padding-top: clamp(96px, 12vh, 150px);
  padding-bottom: clamp(72px, 10vh, 120px);
}

/* Prevent adjacent chapter colours from leaking into the viewport at snap boundaries. */
.hero,
.film,
.future,
.poem,
.principles,
.portrait-section,
.why-section,
.closing {
  position: relative;
  isolation: isolate;
}

@media (max-width: 760px) {
  html { scroll-snap-type: y proximity; }
  .hero,
  .film,
  .poem,
  .future {
    height: 100svh;
    min-height: 100svh;
  }
  .why-section,
  .principles,
  .portrait-section,
  .closing {
    min-height: 100svh;
  }
}
'''

if '</style>' not in s:
    raise SystemExit('style close not found')
s = s.replace('</style>', css + '\n</style>', 1)
p.write_text(s, encoding='utf-8')
