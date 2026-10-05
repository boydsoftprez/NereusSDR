"""Integration coverage for preview and website publication of authored Markdown."""
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
RENDERER = ROOT / 'docs/manual/build_preview.py'


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.links = []
        self.images = []
        self.ids = set()
        self.styles = []
        self.canonical = []
        self.inline_css = False
        self.icons = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        self.inline_css |= tag == 'style' or 'style' in attrs
        if 'id' in attrs:
            self.ids.add(attrs['id'])
        if tag == 'a':
            self.links.append(attrs.get('href', ''))
        if tag == 'img':
            self.images.append(attrs['src'])
        if tag == 'link':
            target = attrs.get('href')
            if attrs.get('rel') == 'stylesheet':
                self.styles.append(target)
            if attrs.get('rel') == 'canonical':
                self.canonical.append(target)
            if attrs.get('rel') == 'icon':
                self.icons.append(target)


def render(script, output, website=False):
    return subprocess.run([sys.executable, str(script), '--output', str(output)]
                          + (['--website'] if website else []),
                          capture_output=True, text=True)


class RendererTest(unittest.TestCase):
    def test_website_publishes_sources_assets_and_resolved_links(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            sentinel = output / 'index.html'
            sentinel.write_text('Existing home')
            result = render(RENDERER, output, website=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(sentinel.read_text(), 'Existing home')
            pages = {p.relative_to(output).as_posix(): Page(p.read_text())
                     for folder in ('manual', 'guides')
                     for p in (output / folder).glob('*.html')}
            expected = {'manual/index.html'} | {
                'manual/'+p.stem+'.html' for p in (ROOT / 'docs/manual').glob('[0-9][0-9]-*.md')
            } | {'guides/'+name+'.html' for name in (
                'install-core-sbc', 'tx-eq-cfc', 'upgrading-to-2026.10.0', 'remote-access')}
            self.assertEqual(set(pages), expected)
            self.assertEqual(len(pages), 25)
            for route, page in pages.items():
                self.assertEqual(page.canonical, ['https://nereussdr.com/' + route])
                self.assertEqual(page.styles, ['/assets/css/site.css', '/manual/manual.css'])
                self.assertIn('/favicon.ico', page.icons)
                self.assertFalse(page.inline_css)
                self.assertIn('/', page.links)
                for target in page.links + page.images:
                    parsed = urlsplit(target)
                    if parsed.scheme or parsed.netloc or parsed.path == '/':
                        continue
                    path = (output / parsed.path.lstrip('/') if parsed.path.startswith('/')
                            else output / Path(route).parent / unquote(parsed.path))
                    if not parsed.path:
                        path = output / route
                    self.assertTrue(path.exists(), (route, target))
                    relative = path.relative_to(output).as_posix()
                    if parsed.fragment and relative in pages:
                        self.assertIn(unquote(parsed.fragment), pages[relative].ids, (route, target))
            upgrade = pages['guides/upgrading-to-2026.10.0.html']
            self.assertEqual(upgrade.links.count('/#core'), 2)
            self.assertIn('/#download', upgrade.links)
            self.assertIn('/guides/install-core-sbc.html', upgrade.links)
            self.assertIn('/guides/tx-eq-cfc.html', upgrade.links)
            self.assertIn('https://github.com/boydsoftprez/NereusSDR/blob/main/docs/architecture/2026-09-23-rendezvous-v1.md',
                          pages['guides/remote-access.html'].links)
            self.assertFalse((output / 'manual/site.css').exists())
            manifest_path = output / 'manual/source-map.json'
            manifest = json.loads(manifest_path.read_text())
            self.assertEqual(len(manifest['pages']), 25)
            for kind in ('pages', 'images', 'downloads'):
                for entry in manifest[kind]:
                    original = ROOT / entry['source']
                    copied = output / entry['output']
                    self.assertEqual(entry['sha256'], hashlib.sha256(original.read_bytes()).hexdigest())
                    self.assertTrue(copied.exists())
                    self.assertEqual(entry['output_sha256'], hashlib.sha256(copied.read_bytes()).hexdigest())
                    if kind != 'pages' and entry['source'] != 'docs/manual/images/capture-provenance.json':
                        self.assertEqual(copied.read_bytes(), original.read_bytes())
            originals = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'docs/manual/images').rglob('*') if p.is_file()}
            self.assertEqual({entry['source'] for entry in manifest['images']}, originals)
            before = manifest_path.read_bytes()
            self.assertEqual(render(RENDERER, output, website=True).returncode, 0)
            self.assertEqual(before, manifest_path.read_bytes())

    def test_public_provenance_retains_capture_metadata_without_private_origins(self):
        source = ROOT / 'docs/manual/images/capture-provenance.json'
        original_bytes = source.read_bytes()
        original = json.loads(original_bytes)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            result = render(RENDERER, output, website=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            public_bytes = (output / 'manual/images/capture-provenance.json').read_bytes()
            public = json.loads(public_bytes)
            self.assertFalse(b'/Users/' in public_bytes, 'Public provenance contains a private capture origin')
            self.assertNotIn(str(ROOT).encode(), public_bytes)
            self.assertIn('preserved originals', public['source_paths'])
            self.assertIn('full capture origin remains in repository provenance', public['source_paths'])
            self.assertEqual(len(public['captures']), len(original['captures']))
            for before, after in zip(original['captures'], public['captures']):
                self.assertEqual(after['source'], 'docs/manual/images/' + before['file'])
                self.assertTrue((ROOT / after['source']).is_file())
                self.assertEqual({key: value for key, value in after.items() if key != 'source'},
                                 {key: value for key, value in before.items() if key != 'source'})
            self.assertEqual({key: value for key, value in public.items() if key not in ('captures', 'source_paths')},
                             {key: value for key, value in original.items() if key != 'captures'})
            self.assertEqual(source.read_bytes(), original_bytes)
            manifest = json.loads((output / 'manual/source-map.json').read_text())
            entry = next(entry for entry in manifest['images'] if entry['source'] == source.relative_to(ROOT).as_posix())
            self.assertEqual(entry['sha256'], hashlib.sha256(original_bytes).hexdigest())
            self.assertEqual(entry['output_sha256'], hashlib.sha256(public_bytes).hexdigest())
            preview = output / 'preview'
            result = render(RENDERER, preview)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((preview / 'images/capture-provenance.json').read_bytes(), original_bytes)

    def fixture(self, directory):
        root = Path(directory) / 'repo'
        manual = root / 'docs/manual'
        manual.mkdir(parents=True)
        shutil.copyfile(RENDERER, manual / 'build_preview.py')
        (manual / 'images').mkdir()
        (manual / 'images/picture.png').write_bytes(b'unchanged image bytes')
        (manual / 'authoring.md').write_text('# Contributor\n')
        (manual / 'README.md').write_text('# Manual\n\n## Intro\n\n[guide](../guides/tx-eq-cfc.md?q=1#settings)\n[contributor](authoring.md)\n![image](images/picture.png)\n')
        (manual / '01-start.md').write_text('# Start\n\n## Start here\n\n[here](#start-here)\n')
        guides = root / 'docs/guides'
        guides.mkdir()
        for name in ('install-core-sbc', 'tx-eq-cfc', 'upgrading-to-2026.10.0'):
            (guides / (name + '.md')).write_text('# Guide\n\n## Settings\n\n[manual](../manual/01-start.md?view=full#start-here)\n[notes](../manual/authoring.md)\n[code](../../src/?raw=1#part)\n[external](https://example.com/test.md?q=1#part)\n![image](../manual/images/picture.png)\n')
        (root / 'rendezvous').mkdir()
        (root / 'rendezvous/README.md').write_text('# Remote\n')
        (root / 'src').mkdir()
        return manual / 'build_preview.py'

    def test_cross_directory_fragments_queries_downloads_and_missing_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            script = self.fixture(directory)
            output = Path(directory) / 'site'
            result = render(script, output, website=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            manual = Page((output / 'manual/index.html').read_text())
            guide = Page((output / 'guides/tx-eq-cfc.html').read_text())
            self.assertIn('/guides/tx-eq-cfc.html?q=1#settings', manual.links)
            self.assertIn('/manual/01-start.html?view=full#start-here', guide.links)
            self.assertIn('/manual/authoring.md', guide.links)
            self.assertIn('https://github.com/boydsoftprez/NereusSDR/tree/main/src?raw=1#part', guide.links)
            self.assertIn('https://example.com/test.md?q=1#part', guide.links)
            self.assertEqual(guide.images, ['/manual/images/picture.png'])
            self.assertIn('settings', guide.ids)
            start = Page((output / 'manual/01-start.html').read_text())
            self.assertIn('#start-here', start.links)
            self.assertIn('start-here', start.ids)
            (script.parent / 'images/picture.png').unlink()
            result = render(script, output, website=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('picture.png', result.stderr)
            (script.parent / 'images/picture.png').write_bytes(b'unchanged image bytes')
            (script.parent.parent / 'guides/tx-eq-cfc.md').unlink()
            result = render(script, output, website=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('tx-eq-cfc.md', result.stderr)

    def test_modes_choose_their_default_output_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            script = self.fixture(directory)
            root = script.parents[2]
            css = root / 'website/public/assets/css/site.css'
            css.parent.mkdir(parents=True)
            css.write_text('body { color: white; }')
            for flags, index in (([], root / 'build/manual-preview/index.html'),
                                 (['--website'], root / 'website/public/manual/index.html')):
                result = subprocess.run([sys.executable, str(script)] + flags,
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(index.exists())
            self.assertEqual((root / 'build/manual-preview/site.css').read_bytes(), css.read_bytes())

    def test_default_preview_passes_existing_checker(self):
        with tempfile.TemporaryDirectory() as directory:
            result = render(RENDERER, Path(directory))
            self.assertEqual(result.returncode, 0, result.stderr)
            index = Path(directory) / 'index.html'
            self.assertIn('working draft', index.read_text())
            self.assertEqual(Page(index.read_text()).styles, ['site.css', 'manual.css'])
            check = subprocess.run([sys.executable, str(ROOT / 'docs/manual/check_manual.py'),
                                    '--preview', directory], capture_output=True, text=True)
            self.assertEqual(check.returncode, 0, check.stdout + check.stderr)


if __name__ == '__main__':
    unittest.main()
