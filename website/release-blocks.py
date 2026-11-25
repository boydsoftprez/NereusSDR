#!/usr/bin/env python3
"""Switch the home page's prepared release copy on or off.

Copy written ahead of a release waits in hidden blocks, so the page can be
reviewed and merged before the work it describes has shipped:

    <template data-pending="786">...</template>         shown once PR 786 is on
    <template data-pending="775 776">...</template>     shown once either is on
    <template data-pending="cut">...</template>         shown at the release cut
    <!-- cut:retire --> ... <!-- /cut:retire -->        removed at the release cut

A browser never shows a <template>, so a block left in place is invisible.

    website/release-blocks.py list            what is waiting
    website/release-blocks.py on 786 780      show the blocks for merged PRs
    website/release-blocks.py off 797         delete the blocks for PRs that wait
    website/release-blocks.py cut 2026.11.2   at the release: retire the old
                                              copy, show the "cut" blocks, write
                                              the version, move the download
                                              links; refuses while a PR block
                                              is undecided

After `cut`, publish the release, then run bake-release.py for the date and
file sizes, as before.
"""
import argparse, re, sys
from pathlib import Path

PAGE = Path(__file__).resolve().parent / 'public/index.html'
OPEN = re.compile(r'<template data-pending="([^"]+)">')
CLOSE = '</template>'


def blocks(page):
    """Every pending block as (start, content_start, content_end, end, keys), nesting aware."""
    found, stack, at = [], [], 0
    while True:
        o = OPEN.search(page, at)
        c = page.find(CLOSE, at)
        if c < 0 and not o:
            break
        if o and (c < 0 or o.start() < c):
            stack.append((o.start(), o.end(), o.group(1).split()))
            at = o.end()
        else:
            if not stack:
                sys.exit(f'Unmatched {CLOSE} at offset {c}')
            start, body, keys = stack.pop()
            found.append((start, body, c, c + len(CLOSE), keys))
            at = c + len(CLOSE)
    if stack:
        sys.exit('A <template data-pending> block is not closed')
    return found


def rewrite(page, wanted, unwrap):
    """Unwrap (or delete) the innermost matching blocks first, until none match."""
    while True:
        hits = [b for b in blocks(page) if wanted(b[4])]
        inner = [b for b in hits if not any(o is not b and b[0] < o[0] < b[3] for o in hits)]
        if not inner:
            return page
        start, body, end_body, end, _ = inner[-1]
        page = page[:start] + (page[body:end_body] if unwrap else '') + page[end:]


parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument('command', choices=['list', 'on', 'off', 'cut'])
parser.add_argument('args', nargs='*')
parser.add_argument('--page', type=Path, default=PAGE)
opts = parser.parse_intermixed_args()
page = opts.page.read_text()

if opts.command == 'list':
    counts = {}
    for b in blocks(page):
        counts[' '.join(b[4])] = counts.get(' '.join(b[4]), 0) + 1
    for key in sorted(counts):
        print(f'{key}: {counts[key]} block(s)')
    print(f'retire markers: {page.count("<!-- cut:retire -->")}')
    sys.exit(0)

if opts.command in ('on', 'off'):
    prs = set(opts.args)
    if not prs or 'cut' in prs:
        sys.exit('Name the PR numbers; the cut blocks change only with `cut`')
    page = rewrite(page, lambda keys: bool(prs & set(keys)), unwrap=opts.command == 'on')
    opts.page.write_text(page)
    print(f'{opts.command} {" ".join(sorted(prs))}')
    sys.exit(0)

# cut
if len(opts.args) != 1 or not re.fullmatch(r'\d{4}\.\d{1,2}\.\d+', opts.args[0]):
    sys.exit('Usage: release-blocks.py cut <version>, for example 2026.11.2')
version = opts.args[0]
undecided = sorted({' '.join(b[4]) for b in blocks(page) if b[4] != ['cut']})
if undecided:
    sys.exit('Still undecided, run `on` or `off` first: ' + ', '.join(undecided))
old = re.search(r'data-release-version>v(\d{4}\.\d{1,2}\.\d+)<', page)
if not old:
    sys.exit('The page names no current version')
old = old.group(1)
if old == version:
    sys.exit(f'The page is already on {version}')

page, retired = re.subn(r'<!-- cut:retire -->.*?<!-- /cut:retire -->\n?', '', page, flags=re.S)
page = rewrite(page, lambda keys: keys == ['cut'], unwrap=True)
page = page.replace('{{VERSION}}', version)

# The release list keeps every earlier version; only the new entry is "latest".
history = re.search(r'<ol class="releases".*?</ol>', page, flags=re.S)
if not history:
    sys.exit('No release list on the page')
items = history.group(0).split('<li', 2)
if len(items) < 3 or f'>v{version}<' not in items[1]:
    sys.exit(f'The release list does not start with v{version}')
rest = items[2].replace(' class="is-latest"', '').replace('<time data-release-history ', '<time ')
new_history = items[0] + '<li' + items[1] + '<li' + rest

before, after = page[:history.start()], page[history.end():]
swap = lambda text: text.replace(old, version)
page = swap(before) + new_history + swap(after)
if f'v{version}/' not in page:
    sys.exit('No download link moved to the new version')
opts.page.write_text(page)
print(f'cut {old} -> {version}: {retired} retired block(s); now publish, then bake-release.py v{version}')
