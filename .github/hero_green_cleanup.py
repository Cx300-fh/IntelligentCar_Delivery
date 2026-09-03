from pathlib import Path
p=Path('docs/index.html')
s=p.read_text(encoding='utf-8')
old='''      background: linear-gradient(90deg, #76b900, #c5ff55);\n      transform: scaleX(0);\n      transform-origin: left center;\n      box-shadow: 0 0 14px rgba(118,185,0,.45);'''
new='''      background: linear-gradient(90deg, rgba(231,236,232,.42), rgba(231,236,232,.72));\n      transform: scaleX(0);\n      transform-origin: left center;\n      box-shadow: none;'''
if old not in s: raise SystemExit('scroll progress pattern missing')
s=s.replace(old,new,1)
old='''      background: var(--green);\n      transform: scaleX(0);'''
new='''      background: rgba(232,237,233,.62);\n      transform: scaleX(0);'''
if old not in s: raise SystemExit('nav underline pattern missing')
s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8')
