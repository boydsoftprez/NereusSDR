#!/usr/bin/env python3
"""Check authored manual links, structure, images and an optional rendered preview."""
from pathlib import Path
from urllib.parse import urlsplit, unquote
from html.parser import HTMLParser
import argparse
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from markdown_it import MarkdownIt

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--preview', type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parent
chapters = sorted(source.glob('[0-9][0-9]-*.md'))
assert [p.name[:2] for p in chapters] == [f'{i:02}' for i in range(1, 21)], 'Expected chapters 01 through 20.'
markdown = MarkdownIt('commonmark').enable('table')
documents = {p.resolve(): markdown.parse(p.read_text()) for p in source.rglob('*.md')}
anchors = {}
for path, tokens in documents.items():
    used = set()
    for pos, token in enumerate(tokens):
        if token.type != 'heading_open':
            continue
        plain = re.sub(r'[^\w\s-]', '', tokens[pos + 1].content.lower())
        slug = re.sub(r'\s+', '-', plain).strip('-') or 'section'
        anchor, suffix = slug, 1
        while anchor in used:
            anchor = f'{slug}-{suffix}'
            suffix += 1
        used.add(anchor)
    anchors[path] = used
link_count = 0
for path in source.rglob('*.md'):
    text = path.read_text()
    assert text.startswith('# '), f'Missing title: {path}'
    assert text.endswith('\n'), f'Missing final newline: {path}'
    assert '\u2014' not in text, f'Em dash: {path}'
    assert all(line == line.rstrip() for line in text.splitlines()), f'Trailing whitespace: {path}'
    for token in markdown.parse(text):
        for child in token.children or []:
            if child.type not in ('link_open', 'image'):
                continue
            target = child.attrGet('href' if child.type == 'link_open' else 'src') or ''
            parsed = urlsplit(target)
            if not parsed.scheme and not parsed.netloc and parsed.path:
                destination = (path.parent / unquote(parsed.path)).resolve()
                assert destination.exists(), f'Broken link in {path.name}: {target}'
                if parsed.fragment and destination in anchors:
                    assert unquote(parsed.fragment) in anchors[destination], f'Broken manuscript section in {path.name}: {target}'
                link_count += 1
            if child.type == 'image':
                assert child.content.strip(), f'Missing image alt text: {path}'
for path in source.glob('images/*.svg'):
    ET.parse(path)
try:
    from PIL import Image
except ImportError:
    print('Raster verification unavailable: install Pillow to check image encoding.')
else:
    for path in source.glob('images/*'):
        if path.suffix in ('.png', '.jpg', '.jpeg'):
            with Image.open(path) as picture:
                picture.verify()
original = source / 'images/desktop-overview-original.png'
digest = hashlib.sha256(original.read_bytes()).hexdigest()
assert digest == 'e50ec094b0bab11de7badcfee4cec29834827c8b32166b8e9de11443ba1a68cc', 'Original capture changed.'
provenance = json.loads((source / 'images/capture-provenance.json').read_text())
for capture in provenance['captures']:
    path = source / 'images' / capture['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == capture['sha256'], f'Original capture changed: {path.name}'
print(f'PASS: 20 chapters, {link_count} local manuscript links/images, titles, whitespace and SVGs; original capture preserved.')
print(f"PASS: {len(provenance['captures'])} preserved screenshot originals match their provenance hashes.")

class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.targets = []
        self.ids = set()

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if 'id' in values:
            assert values['id'] not in self.ids, f'Duplicate rendered ID: {values["id"]}'
            self.ids.add(values['id'])
        key = 'href' if tag in ('a', 'link') else 'src' if tag in ('img', 'script') else None
        if key and key in values:
            self.targets.append(values[key])

if args.preview:
    preview = args.preview.resolve()
    pages = {}
    expected = ['index.html'] + [p.stem + '.html' for p in chapters]
    for name in expected:
        path = preview / name
        assert path.exists(), f'Missing preview page: {name}'
        page = Page()
        page.feed(path.read_text())
        pages[path] = page
    asset_count = 0
    for path, page in pages.items():
        for target in page.targets:
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc:
                continue
            destination = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            assert destination.exists(), f'Broken rendered link in {path.name}: {target}'
            if parsed.fragment and destination in pages:
                assert unquote(parsed.fragment) in pages[destination].ids, f'Broken section link: {path.name}: {target}'
            asset_count += 1
    print(f'PASS: {len(pages)} rendered reader pages and {asset_count} local page/asset/section references.')
