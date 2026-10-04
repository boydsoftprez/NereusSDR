#!/usr/bin/env python3
"""Manual preview only; requires markdown-it-py. Does not deploy the site."""
from pathlib import Path
from markdown_it import MarkdownIt
import argparse, html, re, shutil
from urllib.parse import urlsplit
root = Path(__file__).resolve().parents[2]
source = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description='Render the manual Markdown as static review pages.')
parser.add_argument('--output', type=Path, default=root/'build/manual-preview')
out = parser.parse_args().output.resolve()
out.mkdir(parents=True, exist_ok=True)
shutil.copytree(source/'images', out/'images', dirs_exist_ok=True)
shutil.copyfile(root/'website/public/assets/css/site.css', out/'site.css')
md = MarkdownIt('commonmark').enable('table')
files = [source/'README.md'] + sorted(source.glob('[0-9][0-9]-*.md'))
# Retain contributor records as source downloads, not extra reader chapters.
for document in source.glob('*.md'):
    shutil.copyfile(document, out/document.name)
rendered_sources = {p.resolve() for p in files}
titles = {p.name: p.read_text().splitlines()[0].removeprefix('# ') for p in files}
extra = '''
body::after{display:none}body{font-family:system-ui,sans-serif;font-size:17px}
.manual-shell{max-width:1460px;margin:auto;display:grid;grid-template-columns:270px minmax(0,1fr);gap:48px;padding:36px 32px}
.manual-nav{position:sticky;top:24px;align-self:start;max-height:92vh;overflow:auto;padding:8px}
.manual-nav a{display:block;padding:9px 10px;margin:3px 0;text-decoration:none;color:#a7b8ce;border-radius:5px;font-size:14px;line-height:1.45}
.manual-nav a[aria-current=page]{background:#123448;color:#52dcf7;border-left:3px solid #00b4d8}
.manual-brand{font-weight:700;font-size:23px;color:#52dcf7;margin:0 0 8px}.manual-draft{font-size:13px;color:#93a4b8;margin:0 0 20px}
.manual-nav summary,.page-contents summary{cursor:pointer;color:#52dcf7;padding:10px 0;font-weight:600}.page-contents{border:1px solid #205070;border-radius:5px;padding:6px 16px;margin:24px 0}.page-contents ol{padding-left:22px}.page-contents a{font-size:15px}h2,h3{scroll-margin-top:24px}.skip-link{position:absolute;left:12px;top:-100px}.skip-link:focus{top:12px;background:#122638;padding:12px;z-index:5}
main{max-width:1000px;min-width:0}h1{font-size:clamp(28px,3vw,42px);line-height:1.15;margin:12px 0 24px}h2{font-size:26px;line-height:1.3;margin:36px 0 18px;color:#eef6ff}h3{font-size:21px;margin:28px 0 12px}p,li{line-height:1.7}a{color:#52dcf7}strong{color:#eef6ff}
main img{display:block;max-width:100%;height:auto;border:1px solid #205070;border-radius:7px;margin:24px 0 12px;background:#0f0f1a}main img[src*="iphone-"]:not([src*="landscape"]){max-width:min(100%,420px);margin-left:auto;margin-right:auto}table{width:100%;border-collapse:collapse;margin:24px 0;font-size:15px}th,td{text-align:left;border:1px solid #203040;padding:10px 12px;vertical-align:top}th{background:#122638;color:#eef6ff}td:first-child{min-width:110px}code{font-size:.9em;color:#b8e7f2}pre{overflow:auto;padding:16px;background:#080b16;border:1px solid #203040}blockquote{margin:20px 0;padding:4px 18px;border-left:3px solid #ffb800;background:#141a24}.page-next{display:flex;justify-content:space-between;border-top:1px solid #205070;margin-top:50px;padding-top:18px;gap:20px;font-size:15px}
@media(max-width:850px){.manual-shell{display:block;padding:20px}.manual-nav{position:static;max-height:none;margin-bottom:28px;border-bottom:1px solid #203040;padding-bottom:22px}.manual-nav a{display:inline-block;font-size:13px;padding:7px}table{display:block;overflow-x:auto}}
@media print{body{background:white;color:black}.manual-nav,.page-next{display:none}.manual-shell{display:block;padding:0}main{max-width:none}h1,h2,h3,strong{color:black}a{color:#005070}img{break-inside:avoid}}
'''
(out/'manual.css').write_text(extra)
for index,p in enumerate(files):
    tokens = md.parse(p.read_text())
    sections=[]
    used_ids=set()
    for pos,token in enumerate(tokens):
        if token.type=='heading_open' and token.tag in ('h2','h3'):
            label=tokens[pos+1].content
            plain=re.sub(r'[^\w\s-]','',label.lower())
            slug=re.sub(r'\s+','-',plain).strip('-') or 'section'
            anchor=slug
            suffix=1
            while anchor in used_ids:
                anchor=f'{slug}-{suffix}'
                suffix+=1
            used_ids.add(anchor)
            token.attrSet('id',anchor)
            if token.tag=='h2':
                sections.append((anchor,label))
    for token in tokens:
        if token.type=='inline' and token.children:
            for child in token.children:
                if child.type=='link_open':
                    href=child.attrGet('href') or ''
                    parsed = urlsplit(href)
                    if not parsed.scheme and not parsed.netloc and parsed.path.endswith('.md'):
                        destination = (p.parent / parsed.path).resolve()
                        if destination in rendered_sources:
                            child.attrSet('href', href.replace('README.md','index.html').replace('.md','.html'))
    body=md.renderer.render(tokens, md.options, {})
    contents=''.join(f'<li><a href="#{anchor}">{html.escape(label)}</a></li>' for anchor,label in sections)
    if contents:
        body=re.sub(r'(</h1>)',r'\1'+f'<details class="page-contents" open><summary>On this page</summary><ol>{contents}</ol></details>',body,count=1)
    nav=''.join(f'<a href="{"index.html" if q.name=="README.md" else q.stem+".html"}"{ " aria-current=page" if q==p else ""}>{html.escape(titles[q.name])}</a>' for q in files)
    prev='' if index==0 else f'<a href="{"index.html" if files[index-1].name=="README.md" else files[index-1].stem+".html"}">Previous chapter</a>'
    nxt='' if index==len(files)-1 else f'<a href="{files[index+1].stem}.html">Next chapter</a>'
    page=f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(titles[p.name])} | NereusSDR</title><link rel="stylesheet" href="site.css"><link rel="stylesheet" href="manual.css"></head><body><a class="skip-link" href="#content">Skip to chapter</a><div class="manual-shell"><nav class="manual-nav" aria-label="Manual chapters"><p class="manual-brand">NereusSDR</p><p class="manual-draft">User manual · working draft<br>4 October 2026</p><details open><summary>Browse all chapters</summary>{nav}</details></nav><main id="content">{body}<div class="page-next">{prev}{nxt}</div></main></div></body></html>'
    (out/('index.html' if p.name=='README.md' else p.stem+'.html')).write_text(page)
print(f'Rendered {len(files)} manual pages to {out}')
