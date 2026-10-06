'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const script = fs.readFileSync(path.join(__dirname, 'public/assets/js/site.js'), 'utf8');
const prefix = 'https://github.com/boydsoftprez/NereusSDR/releases/download/v2026.10.1/';

function updateDownloads(assets) {
  const source = { href: 'built-in-source', getAttribute: () => 'source.tar.gz',
    setAttribute(name, value) { this[name] = value; }, querySelector: () => null };
  const desktop = { href: 'built-in-desktop', getAttribute: () => 'macOS-intel.dmg',
    setAttribute(name, value) { this[name] = value; }, querySelector: () => null };
  const release = { tag: 'v2026.10.1', date: '2026-10-06T00:00:00Z',
    url: 'https://github.com/boydsoftprez/NereusSDR/releases/tag/v2026.10.1', assets };
  const document = {
    documentElement: { classList: { add() {} } },
    querySelector: () => null,
    querySelectorAll: (selector) => selector === '[data-asset]' ? [source, desktop] : [],
    getElementById: () => null,
  };
  vm.runInNewContext(script, {
    document, URL, navigator: { userAgent: '', platform: '' },
    window: { addEventListener() {} },
    sessionStorage: { getItem: () => JSON.stringify({ at: Date.now(), data: release }) },
  });
  return [source.href, desktop.href];
}

function asset(name, url = prefix + name) {
  return { n: name, u: url, s: 1000 };
}

const desktop = asset('NereusSDR-2026.10.1-macOS-intel.dmg');

test('the source link selects NereusSDR even when a dependency archive comes first', () => {
  assert.deepEqual(updateDownloads([
    asset('fftw-ubuntu-source.tar.gz'),
    asset('NereusSDR-2026.10.1-source.tar.gz'), desktop,
  ]), [prefix + 'NereusSDR-2026.10.1-source.tar.gz', desktop.u]);
});

test('a release with only a dependency source archive keeps all built-in downloads', () => {
  assert.deepEqual(updateDownloads([asset('fftw-ubuntu-source.tar.gz'), desktop]),
    ['built-in-source', 'built-in-desktop']);
});

test('a product asset pointing outside the repository keeps all built-in downloads', () => {
  assert.deepEqual(updateDownloads([
    asset('NereusSDR-2026.10.1-source.tar.gz', 'https://example.com/source.tar.gz'), desktop,
  ]), ['built-in-source', 'built-in-desktop']);
});
