#!/usr/bin/env python3
"""Render the authored manual for local review or the static website; does not deploy."""
from pathlib import Path
from markdown_it import MarkdownIt
import argparse, hashlib, html, json, re, shutil
from urllib.parse import quote, unquote, urlsplit, urlunsplit
root = Path(__file__).resolve().parents[2]
source = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description='Render the manual Markdown as static review pages.')
parser.add_argument('--output', type=Path, help='Preview directory, or site root with --website.')
parser.add_argument('--website', action='store_true', help='Render the manual and how-tos into the static site.')
args = parser.parse_args()
site_out = (args.output or (root/'website/public' if args.website else root/'build/manual-preview')).resolve()
out = site_out/'manual' if args.website else site_out
out.mkdir(parents=True, exist_ok=True)
shutil.copytree(source/'images', out/'images', dirs_exist_ok=True)
if args.website and (source/'images/capture-provenance.json').exists():
    # Publication identifies preserved originals; repository provenance retains capture origins.
    provenance = json.loads((source/'images/capture-provenance.json').read_text())
    provenance['source_paths'] = ('Repository-relative paths to the preserved originals; '
                                  'full capture origin remains in repository provenance.')
    for capture in provenance['captures']:
        original = source/'images'/capture['file']
        if not original.is_file() or not original.resolve().is_relative_to(source/'images'):
            raise ValueError(f'Missing or invalid preserved original: {capture["file"]}')
        capture['source'] = original.relative_to(root).as_posix()
    (out/'images/capture-provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')
if not args.website:
    shutil.copyfile(root/'website/public/assets/css/site.css', out/'site.css')
md = MarkdownIt('commonmark').enable('table')
manual_files = [source/'README.md'] + sorted(source.glob('[0-9][0-9]-*.md'))
files = list(manual_files)
routes = {p.resolve(): 'manual/'+('index.html' if p.name=='README.md' else p.stem+'.html') for p in manual_files}
if args.website:
    for name in ('install-core-sbc', 'tx-eq-cfc', 'upgrading-to-2026.10.0'):
        guide = root/'docs/guides'/f'{name}.md'
        files.append(guide)
        routes[guide.resolve()] = f'guides/{name}.html'
    files.append(root/'rendezvous/README.md')
    routes[files[-1].resolve()] = 'guides/remote-access.html'
# Retain contributor records as source downloads, not extra reader chapters.
for document in source.glob('*.md'):
    shutil.copyfile(document, out/document.name)
rendered_sources = {p.resolve() for p in files}
titles = {p.resolve(): p.read_text().splitlines()[0].removeprefix('# ') for p in files}
downloads = {p.resolve(): 'manual/'+p.relative_to(source).as_posix() for p in source.rglob('*.md')}


def website_target(page, target, image=False):
    """Route a local URL by its resolved manuscript source, retaining its suffix."""
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return target
    destination = (root / unquote(parsed.path.lstrip('/')) if parsed.path.startswith('/')
                   else page.parent / unquote(parsed.path)).resolve()
    if not destination.is_relative_to(root):
        raise ValueError(f'Link outside repository in {page.relative_to(root)}: {target}')
    if not destination.exists():
        raise ValueError(f'Missing local target in {page.relative_to(root)}: {target}')
    home_sections = {'one-station-several-ways-to-operate': 'core',
                     'reach-your-station-from-another-network': 'core',
                     'radios-and-downloads': 'download'}
    if destination == root/'README.md' and parsed.fragment in home_sections:
        return urlunsplit(('', '', '/', parsed.query, home_sections[parsed.fragment]))
    if destination in routes:
        path = '/'+routes[destination]
    elif destination in downloads:
        path = '/'+downloads[destination]
    elif destination.is_relative_to(source/'images'):
        path = '/manual/images/'+destination.relative_to(source/'images').as_posix()
    elif image:
        raise ValueError(f'Image outside manual assets in {page.relative_to(root)}: {target}')
    else:
        kind = 'tree' if destination.is_dir() else 'blob'
        path = f'https://github.com/boydsoftprez/NereusSDR/{kind}/main/'+quote(destination.relative_to(root).as_posix())
    routed = urlsplit(path)
    return urlunsplit((routed.scheme, routed.netloc, routed.path, parsed.query, parsed.fragment))


def page_link(page):
    return '/'+routes[page.resolve()] if args.website else ('index.html' if page.name=='README.md' else page.stem+'.html')


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
        if token.type=='heading_open' and token.tag in (('h1','h2','h3') if args.website else ('h2','h3')):
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
                if args.website and child.type in ('link_open', 'image'):
                    attribute = 'href' if child.type=='link_open' else 'src'
                    child.attrSet(attribute, website_target(p, child.attrGet(attribute) or '', image=child.type=='image'))
                elif child.type=='link_open':
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
    nav=''.join(f'<a href="{page_link(q)}"{ " aria-current=page" if q==p else ""}>{html.escape(titles[q.resolve()])}</a>' for q in manual_files)
    navigation=f'<details open><summary>Browse all chapters</summary>{nav}</details>'
    if args.website:
        howtos=''.join(f'<a href="{page_link(q)}"{ " aria-current=page" if q==p else ""}>{html.escape(titles[q.resolve()])}</a>' for q in files[len(manual_files):])
        navigation += f'<details open><summary>How-to guides</summary>{howtos}</details>'
    prev='' if index==0 or p not in manual_files else f'<a href="{page_link(files[index-1])}">Previous chapter</a>'
    nxt='' if index>=len(manual_files)-1 else f'<a href="{page_link(files[index+1])}">Next chapter</a>'
    styles = '<link rel="stylesheet" href="/assets/css/site.css"><link rel="stylesheet" href="/manual/manual.css">' if args.website else '<link rel="stylesheet" href="site.css"><link rel="stylesheet" href="manual.css">'
    metadata = f'<link rel="canonical" href="https://nereussdr.com{page_link(p)}"><link rel="icon" href="/favicon.ico" sizes="any">' if args.website else ''
    brand = '<a class="manual-brand" href="/">NereusSDR</a><a href="/">Home</a>' if args.website else '<p class="manual-brand">NereusSDR</p>'
    page=f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(titles[p.resolve()])} | NereusSDR</title>{metadata}{styles}</head><body><a class="skip-link" href="#content">Skip to chapter</a><div class="manual-shell"><nav class="manual-nav" aria-label="Manual chapters">{brand}<p class="manual-draft">User manual · working draft<br>4 October 2026</p>{navigation}</nav><main id="content">{body}<div class="page-next">{prev}{nxt}</div></main></div></body></html>'
    destination = site_out/routes[p.resolve()] if args.website else out/('index.html' if p.name=='README.md' else p.stem+'.html')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(page)
if args.website:
    def record(path, output):
        return {'source': path.relative_to(root).as_posix(), 'output': output,
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'output_sha256': hashlib.sha256((site_out/output).read_bytes()).hexdigest()}

    manifest = {
        'pages': [record(p, routes[p.resolve()]) for p in files],
        'downloads': [record(p, downloads[p.resolve()]) for p in sorted(source.rglob('*.md'))],
        'images': [record(p, 'manual/images/'+p.relative_to(source/'images').as_posix())
                   for p in sorted((source/'images').rglob('*')) if p.is_file()],
    }
    (out/'source-map.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
print(f'Rendered {len(files)} {"website" if args.website else "manual"} pages to {site_out}')
