from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

# Remove the static demo metadata row completely.
text = text.replace('        <div class="demo-video-meta">THU campus · 37 s · 16:9</div>\n', '', 1)

marker = '    /* Hide nonessential demo/map metadata while preserving JS hooks */'
if marker not in text:
    css = '''\n\n    /* Hide nonessential demo/map metadata while preserving JS hooks */\n    .route-summary,\n    .map-side .metric-grid {\n      display: none !important;\n    }\n'''
    text = text.replace('  </style>', css + '\n  </style>', 1)

path.write_text(text, encoding='utf-8')
