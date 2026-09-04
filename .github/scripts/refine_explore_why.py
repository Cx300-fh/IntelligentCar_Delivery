from pathlib import Path

p = Path('docs/explore.html')
s = p.read_text(encoding='utf-8')

start = s.index('<section class="essay">')
end = s.index('</section>', start) + len('</section>')

new_section = '''<section class="essay why-section" id="why">
  <div class="why-intro">
    <div class="section-no">01 · Why 清送</div>
    <div class="why-title-wrap">
      <h2 class="serif why-title">The best interface is sometimes the one that disappears.</h2>
    </div>
  </div>

  <div class="why-row why-row-a">
    <div class="why-left why-cn-statement">
      <span class="why-index">01</span>
      <p>“清送”这个名字，对我们而言不只是一个项目名。</p>
      <p>它意味着一种克制：路径可以复杂，控制可以精密，但来到使用者面前时，事情应该重新变得简单。</p>
    </div>
    <div class="why-right why-body-block">
      <span class="why-label">People before interface</span>
      <p>一个抱着箱子的学生，不应该为了叫车腾出双手；一个赶时间的人，不应该理解调度器；一个第一次见到机器人的人，也不应该先读说明书。</p>
      <p>语音、网页、称重、自动重规划、断联停车——它们不是为了让系统看起来更复杂，而是为了让复杂性退到人的背后。</p>
    </div>
  </div>

  <div class="why-row why-row-b">
    <blockquote class="why-left why-quote serif">
      “所谓智能，不是让人适应机器，而是让机器更懂得人的不便。”
      <small>Our design premise</small>
    </blockquote>
    <div class="why-right why-close">
      <span class="why-index">02</span>
      <p>因此我们不断问同一个问题：</p>
      <p class="why-close-em">这一项技术，究竟解决了谁的什么麻烦？</p>
      <p>如果答案不够具体，它就不应该占据页面，也不应该占据车上的空间。</p>
    </div>
  </div>
</section>'''

s = s[:start] + new_section + s[end:]

marker = '/* WHY QINGSONG ALTERNATING v2 */'
if marker not in s:
    css = '''
/* WHY QINGSONG ALTERNATING v2 */
.why-section{padding-top:clamp(90px,10vw,150px);padding-bottom:clamp(110px,13vw,190px)}
.why-intro{display:grid;grid-template-columns:minmax(150px,.62fr) minmax(0,1.38fr);gap:clamp(46px,9vw,150px);align-items:start;margin-bottom:clamp(88px,10vw,150px)}
.why-section .section-no{position:sticky;top:100px;padding-top:9px;font-size:10px;letter-spacing:.22em;color:rgba(23,23,20,.48)}
.why-title-wrap{max-width:820px}
.why-title{margin:0;font-family:Didot,"Bodoni 72","Times New Roman",Georgia,serif;font-size:clamp(58px,6.6vw,100px)!important;line-height:.92!important;letter-spacing:-.042em!important;font-weight:400!important;text-wrap:balance}
.why-row{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));column-gap:clamp(18px,3vw,46px);align-items:start;border-top:1px solid rgba(23,23,20,.13);padding-top:clamp(34px,4vw,56px)}
.why-row + .why-row{margin-top:clamp(100px,12vw,170px)}
.why-left{grid-column:1 / span 5}.why-right{grid-column:7 / span 6}
.why-index{display:block;margin-bottom:26px;font-family:Inter,ui-sans-serif,system-ui,sans-serif;font-size:10px;line-height:1;letter-spacing:.2em;color:rgba(95,112,64,.78)}
.why-label{display:block;margin-bottom:22px;font-family:Inter,ui-sans-serif,system-ui,sans-serif;font-size:10px;letter-spacing:.18em;text-transform:uppercase;color:rgba(23,23,20,.43)}
.why-cn-statement{font-family:"Songti SC","STSong","Noto Serif CJK SC",serif}
.why-cn-statement p{margin:0!important;color:var(--ink)!important;font-size:clamp(23px,2.1vw,32px)!important;line-height:1.72!important;letter-spacing:.015em}
.why-cn-statement p + p{margin-top:26px!important;color:rgba(23,23,20,.68)!important;font-size:clamp(18px,1.45vw,22px)!important}
.why-body-block{padding-top:2px;max-width:620px}
.why-body-block p{margin:0!important;font-family:Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;font-size:clamp(16px,1.18vw,19px)!important;line-height:1.95!important;color:rgba(23,23,20,.68)!important}
.why-body-block p + p{margin-top:24px!important}
.why-quote{margin:0!important;padding:0!important;border:0!important;font-family:Didot,"Bodoni 72","Songti SC","STSong",Georgia,serif;font-size:clamp(34px,3.8vw,58px)!important;line-height:1.24!important;letter-spacing:-.018em;color:var(--ink)}
.why-quote small{display:block;margin-top:28px;font-family:Inter,ui-sans-serif,system-ui,sans-serif;font-size:10px;letter-spacing:.2em;text-transform:uppercase;color:rgba(23,23,20,.42)}
.why-close{max-width:560px;padding-top:4px}
.why-close p{margin:0!important;font-size:clamp(16px,1.2vw,19px)!important;line-height:1.9!important;color:rgba(23,23,20,.62)!important}
.why-close .why-close-em{margin:18px 0!important;font-family:"Songti SC","STSong","Noto Serif CJK SC",serif;font-size:clamp(25px,2.25vw,36px)!important;line-height:1.55!important;color:var(--ink)!important}
@media(max-width:760px){.why-intro{grid-template-columns:1fr;gap:22px;margin-bottom:72px}.why-section .section-no{position:static}.why-row{grid-template-columns:1fr;row-gap:54px}.why-left,.why-right{grid-column:1}.why-row + .why-row{margin-top:86px}.why-title{font-size:clamp(46px,13vw,72px)!important}.why-cn-statement p{font-size:24px!important}.why-quote{font-size:36px!important}}
'''
    s = s.replace('</style>', css + '\n</style>', 1)

p.write_text(s, encoding='utf-8')
