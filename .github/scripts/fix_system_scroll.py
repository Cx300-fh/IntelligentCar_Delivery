from pathlib import Path

path = Path('docs/index.html')
text = path.read_text(encoding='utf-8')

old_copy = 'Start with the situation, then reveal the engineering response. Switch between six views without turning the homepage into a long technical report.'
new_copy = 'Start with the situation, then reveal the engineering response. Scroll through six linked scenes while the left rail always shows where you are.'
if old_copy not in text:
    raise SystemExit('System lead copy not found')
text = text.replace(old_copy, new_copy, 1)

old_scroll = "scroll.scrollTo({ top: target.offsetTop, behavior: 'smooth' });"
new_scroll = "const targetTop = target.getBoundingClientRect().top - scroll.getBoundingClientRect().top + scroll.scrollTop;\n          scroll.scrollTo({ top: targetTop, behavior: 'smooth' });"
if old_scroll not in text:
    raise SystemExit('System tab scroll call not found')
text = text.replace(old_scroll, new_scroll, 1)

text = '\n'.join(line.rstrip() for line in text.splitlines()) + '\n'
path.write_text(text, encoding='utf-8')
