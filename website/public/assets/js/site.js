/* NereusSDR website script.
 * No dependencies, no cookies, no analytics. The only network request is to
 * the public GitHub API for the latest release, cached per browser session.
 */
(function () {
  'use strict';

  var root = document.documentElement;
  root.classList.add('js');

  var REDUCED = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  var REPO = 'boydsoftprez/NereusSDR';

  function safe(fn) {
    try {
      fn();
    } catch (err) {
      if (window.console && console.warn) {
        console.warn('[nereussdr]', err);
      }
    }
  }

  function clamp(v, lo, hi) {
    return v < lo ? lo : v > hi ? hi : v;
  }

  /* ------------------------------------------------------------------ */
  /* Reveal on scroll (runs first so content can never stay hidden)     */
  /* ------------------------------------------------------------------ */

  function initReveal() {
    var els = document.querySelectorAll('[data-reveal]');
    if (REDUCED || !('IntersectionObserver' in window)) {
      els.forEach(function (el) { el.classList.add('is-in'); });
      return;
    }
    document.querySelectorAll('.radios, .applets, .dl-grid, .involve').forEach(function (grid) {
      Array.prototype.forEach.call(grid.children, function (child, i) {
        child.style.setProperty('--reveal-delay', ((i % 6) * 0.06).toFixed(2) + 's');
      });
    });
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add('is-in');
          io.unobserve(entry.target);
        }
      });
    }, { rootMargin: '0px 0px -8% 0px', threshold: 0.06 });
    els.forEach(function (el) { io.observe(el); });
  }

  /* ------------------------------------------------------------------ */
  /* Navigation                                                          */
  /* ------------------------------------------------------------------ */

  function initNav() {
    var nav = document.querySelector('[data-nav]');
    var toggle = document.querySelector('[data-nav-toggle]');
    if (!nav) {
      return;
    }
    var ticking = false;
    function onScroll() {
      if (ticking) {
        return;
      }
      ticking = true;
      requestAnimationFrame(function () {
        nav.classList.toggle('is-scrolled', window.scrollY > 8);
        ticking = false;
      });
    }
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();

    if (!toggle) {
      return;
    }
    function setOpen(open) {
      nav.classList.toggle('is-open', open);
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    }
    toggle.addEventListener('click', function () {
      setOpen(!nav.classList.contains('is-open'));
    });
    nav.querySelectorAll('.nav__menu a').forEach(function (a) {
      a.addEventListener('click', function () { setOpen(false); });
    });
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape' && nav.classList.contains('is-open')) {
        setOpen(false);
        toggle.focus();
      }
    });
  }

  /* ------------------------------------------------------------------ */
  /* UTC clock in the status bar (hams live in UTC)                      */
  /* ------------------------------------------------------------------ */

  function initClock() {
    var el = document.querySelector('[data-utc]');
    if (!el) {
      return;
    }
    function tick() {
      el.textContent = new Date().toISOString().slice(11, 19) + ' UTC';
    }
    tick();
    setInterval(tick, 1000);
  }

  /* ------------------------------------------------------------------ */
  /* Operating system: label the download button, mark the right card    */
  /* ------------------------------------------------------------------ */

  function detectOs() {
    var ua = navigator.userAgent || '';
    var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
    if (/iPhone|iPad|iPod|Android/i.test(ua)) {
      return null;
    }
    if (/Mac/i.test(platform) || /Macintosh/i.test(ua)) {
      // iPadOS reports itself as a Mac; it has touch points, a Mac does not.
      return navigator.maxTouchPoints > 1 ? null : 'mac';
    }
    if (/Win/i.test(platform) || /Windows/i.test(ua)) {
      return 'windows';
    }
    if (/Linux|X11|CrOS/i.test(platform + ' ' + ua)) {
      return 'linux';
    }
    return null;
  }

  function initOs() {
    var os = detectOs();
    if (!os) {
      return;
    }
    var names = { mac: 'macOS', windows: 'Windows', linux: 'Linux' };
    var card = document.getElementById('dl-' + os);
    if (card) {
      card.classList.add('is-yours');
    }
    var cta = document.querySelector('[data-os-cta]');
    var label = document.querySelector('[data-os-cta-label]');
    if (cta && label) {
      label.textContent = 'Download for ' + names[os];
      cta.setAttribute('href', '#dl-' + os);
    }
  }

  /* ------------------------------------------------------------------ */
  /* Latest release from GitHub. The page ships with v0.5.2 baked in and */
  /* only switches when every download link can be matched.              */
  /* ------------------------------------------------------------------ */

  var CACHE_KEY = 'nereussdr-latest-release-v1';

  function readCache() {
    try {
      var raw = sessionStorage.getItem(CACHE_KEY);
      if (!raw) {
        return null;
      }
      var c = JSON.parse(raw);
      return c && Date.now() - c.at < 3600000 ? c.data : null;
    } catch (e) {
      return null;
    }
  }

  function writeCache(data) {
    try {
      sessionStorage.setItem(CACHE_KEY, JSON.stringify({ at: Date.now(), data: data }));
    } catch (e) {
      /* storage can be unavailable; the page works without it */
    }
  }

  function fmtDate(iso) {
    var d = new Date(iso);
    if (isNaN(d.getTime())) {
      return null;
    }
    return d.toLocaleDateString('en-GB', { day: 'numeric', month: 'short', year: 'numeric', timeZone: 'UTC' });
  }

  function fmtSize(bytes) {
    return (bytes / 1e6).toFixed(1) + ' MB';
  }

  function applyRelease(rel) {
    if (!rel || !/^v\d+\.\d+\.\d+$/.test(rel.tag) || !Array.isArray(rel.assets)) {
      return;
    }
    var links = document.querySelectorAll('[data-asset]');
    var matches = [];
    for (var i = 0; i < links.length; i++) {
      var suffix = links[i].getAttribute('data-asset');
      var found = null;
      for (var j = 0; j < rel.assets.length; j++) {
        var n = rel.assets[j].n;
        if (n === suffix || n.slice(-(suffix.length + 1)) === '-' + suffix) {
          found = rel.assets[j];
          break;
        }
      }
      if (!found) {
        return; // keep the baked-in links rather than mix versions
      }
      matches.push([links[i], found]);
    }

    var plain = rel.tag.slice(1);
    matches.forEach(function (m) {
      var a = m[0];
      var asset = m[1];
      a.setAttribute('href', asset.u);
      var small = a.querySelector('small');
      if (small) {
        small.textContent = asset.n;
      }
      var size = a.querySelector('.dl__size');
      if (size) {
        size.textContent = fmtSize(asset.s);
      }
    });
    document.querySelectorAll('[data-release-version]').forEach(function (el) {
      el.textContent = rel.tag;
    });
    var date = fmtDate(rel.date);
    if (date) {
      document.querySelectorAll('[data-release-date]').forEach(function (el) {
        el.textContent = date;
        el.setAttribute('datetime', rel.date.slice(0, 10));
      });
    }
    if (/^https:\/\/github\.com\//.test(rel.url)) {
      document.querySelectorAll('[data-release-notes]').forEach(function (el) {
        el.setAttribute('href', rel.url);
      });
    }
    document.querySelectorAll('[data-version-swap]').forEach(function (el) {
      el.textContent = 'NereusSDR-' + plain;
    });
  }

  function initRelease() {
    var cached = readCache();
    if (cached) {
      applyRelease(cached);
      return;
    }
    if (!window.fetch) {
      return;
    }
    fetch('https://api.github.com/repos/' + REPO + '/releases/latest', {
      headers: { Accept: 'application/vnd.github+json' }
    })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        if (!j) {
          return;
        }
        var data = {
          tag: j.tag_name,
          date: j.published_at,
          url: j.html_url,
          assets: (j.assets || []).map(function (a) {
            return { n: a.name, u: a.browser_download_url, s: a.size };
          })
        };
        writeCache(data);
        applyRelease(data);
      })
      .catch(function () { /* offline or rate limited: keep the baked-in release */ });
  }

  /* ------------------------------------------------------------------ */
  /* Tabs and copy buttons                                               */
  /* ------------------------------------------------------------------ */

  function initTabs() {
    var os = detectOs();
    var prefer = { mac: 'v-mac', linux: 'v-linux', windows: 'v-win' }[os];
    document.querySelectorAll('[role="tablist"]').forEach(function (list) {
      var tabs = Array.prototype.slice.call(list.querySelectorAll('[role="tab"]'));
      function select(tab) {
        tabs.forEach(function (t) {
          var on = t === tab;
          t.setAttribute('aria-selected', on ? 'true' : 'false');
          t.tabIndex = on ? 0 : -1;
          var panel = document.getElementById(t.getAttribute('aria-controls'));
          if (panel) {
            panel.hidden = !on;
          }
        });
      }
      tabs.forEach(function (t, i) {
        t.addEventListener('click', function () { select(t); });
        t.addEventListener('keydown', function (e) {
          var k = null;
          if (e.key === 'ArrowRight') { k = (i + 1) % tabs.length; }
          else if (e.key === 'ArrowLeft') { k = (i - 1 + tabs.length) % tabs.length; }
          else if (e.key === 'Home') { k = 0; }
          else if (e.key === 'End') { k = tabs.length - 1; }
          if (k !== null) {
            e.preventDefault();
            tabs[k].focus();
            select(tabs[k]);
          }
        });
      });
      var preferred = tabs.filter(function (t) { return t.getAttribute('aria-controls') === prefer; })[0];
      if (preferred) {
        select(preferred);
      }
    });
  }

  function initCopy() {
    document.querySelectorAll('[data-copy]').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var pre = btn.parentElement.querySelector('pre');
        if (!pre || !navigator.clipboard) {
          return;
        }
        navigator.clipboard.writeText(pre.innerText.trim()).then(function () {
          btn.textContent = 'Copied';
          btn.classList.add('is-done');
          setTimeout(function () {
            btn.textContent = 'Copy';
            btn.classList.remove('is-done');
          }, 1600);
        }, function () {
          btn.textContent = 'Select to copy';
        });
      });
    });
  }

  /* ------------------------------------------------------------------ */
  /* The simulated 40 m band                                             */
  /* ------------------------------------------------------------------ */

  var F0 = 6995000;
  var F1 = 7305000;
  var SPAN = F1 - F0;

  function mulberry32(seed) {
    var a = seed >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) >>> 0;
      var t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function hash2(a, b) {
    var h = (Math.imul(a | 0, 374761393) + Math.imul(b | 0, 668265263)) | 0;
    h = Math.imul(h ^ (h >>> 13), 1274126177);
    h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
  }

  var MORSE = {
    A: '.-', B: '-...', C: '-.-.', D: '-..', E: '.', F: '..-.', G: '--.', H: '....', I: '..',
    J: '.---', K: '-.-', L: '.-..', M: '--', N: '-.', O: '---', P: '.--.', Q: '--.-', R: '.-.',
    S: '...', T: '-', U: '..-', V: '...-', W: '.--', X: '-..-', Y: '-.--', Z: '--..',
    0: '-----', 1: '.----', 2: '..---', 3: '...--', 4: '....-', 5: '.....', 6: '-....',
    7: '--...', 8: '---..', 9: '----.', '?': '..--..', '/': '-..-.'
  };

  // Key-down (1) and key-up (0) per Morse unit, with standard spacing.
  function morseUnits(text, pauseUnits) {
    var out = [];
    var words = text.toUpperCase().split(' ');
    words.forEach(function (word, wi) {
      var chars = word.split('');
      chars.forEach(function (ch, ci) {
        var code = MORSE[ch];
        if (!code) {
          return;
        }
        code.split('').forEach(function (sym, si) {
          var len = sym === '.' ? 1 : 3;
          for (var k = 0; k < len; k++) { out.push(1); }
          if (si < code.length - 1) { out.push(0); }
        });
        if (ci < chars.length - 1) { out.push(0, 0, 0); }
      });
      if (wi < words.length - 1) {
        for (var k = 0; k < 7; k++) { out.push(0); }
      }
    });
    for (var p = 0; p < pauseUnits; p++) { out.push(0); }
    return Uint8Array.from(out);
  }

  // Phrases and breaths for one operator, 30 s at 20 ms resolution.
  // Values are dB relative to the operator's level; -99 is silence.
  // Speech on a waterfall reads as long, lumpy runs with short dips between
  // words and a real gap only when the speaker breathes.
  function voiceEnvelope(rng) {
    var n = 1500;
    var env = new Float32Array(n).fill(-99);
    var i = 0;
    while (i < n) {
      var phrase = Math.round((1.2 + rng() * 3.2) / 0.02);
      var base = -rng() * 5;
      var syl = 0.45 + rng() * 0.35;
      var end = Math.min(n, i + phrase);
      for (var k = 0; i < end; k++, i++) {
        var ripple = -7 * Math.pow(Math.sin(k * syl), 2);
        var dip = (k % 23 === 22 && rng() < 0.5) ? -30 : 0;   // brief word gaps
        var edge = Math.min(k, phrase - 1 - k);
        env[i] = base + ripple + dip + (edge < 3 ? -6 + edge * 2 : 0);
      }
      i += Math.round((0.25 + rng() * 0.65) / 0.02);           // breath
    }
    return env;
  }

  function buildBand() {
    var rng = mulberry32(0x7074);

    // CW, bottom of the band. One station sends a message worth reading.
    var cwTexts = ['TU 5NN', 'CQ TEST', 'R R TNX FER QSO 73', 'QRL?', 'CQ POTA', 'UR RST 579 579', 'GM OM', 'TNX 73 GL', 'CQ CQ CQ'];
    var cw = [{ f: 7028000, wpm: 12, text: 'CQ CQ DE NEREUSSDR K', level: -76, pause: 5 }];
    var cwFreqs = [7004300, 7009100, 7013600, 7018800, 7022400, 7033700, 7039200, 7046500, 7052800];
    cwFreqs.forEach(function (f, i) {
      cw.push({ f: f, wpm: 16 + Math.round(rng() * 14), text: cwTexts[i % cwTexts.length], level: -104 + rng() * 26, pause: 2 + rng() * 7 });
    });
    cw.forEach(function (st) {
      st.unit = 1.2 / st.wpm;
      st.seq = morseUnits(st.text, Math.round(st.pause / st.unit));
      st.phase = rng() * st.seq.length * st.unit;
      st.qsbPeriod = 7 + rng() * 16;
      st.qsbPhase = rng() * 6.283;
    });

    // FT8 around 7.074 MHz, on real 15 s UTC cycles.
    var ft8 = [];
    for (var i = 0; i < 16; i++) {
      ft8.push({ f: 7074000 + 180 + rng() * 2650, even: i % 2 === 0, level: -106 + rng() * 22, id: i });
    }

    // LSB voice. Each channel is a QSO: operators take turns, then it goes quiet.
    var ssbFreqs = [7128500, 7141000, 7152500, 7165000, 7178500, 7190500, 7203000, 7214500, 7225000, 7236400, 7249000, 7261500, 7274000, 7287000];
    var ssb = ssbFreqs.map(function (fc) {
      var count = rng() < 0.3 ? 3 : 2;
      var ops = [];
      for (var k = 0; k < count; k++) {
        ops.push({
          level: -112 + rng() * 42,
          dur: 9 + rng() * 26,
          gap: 1.2 + rng() * 2.5,
          env: voiceEnvelope(rng),
          envPhase: rng() * 30
        });
      }
      if (fc === 7236400) {
        ops[0].level = -66; // the station the demo starts on
        ops[1].level = -84;
      }
      var idle = rng() < 0.35 ? 4 + rng() * 22 : 0;
      var cycle = idle;
      ops.forEach(function (op) { cycle += op.dur + op.gap; });
      return { fc: fc, ops: ops, cycle: cycle, phase: rng() * cycle, qsbPeriod: 9 + rng() * 20, qsbPhase: rng() * 6.283 };
    });

    return { cw: cw, ft8: ft8, ssb: ssb };
  }

  function makeSimulation(band) {
    var bins = 0;
    var lin = null;
    var floorLin = null;

    function resize(n) {
      bins = n;
      lin = new Float32Array(n);
      floorLin = new Float32Array(n);
      for (var i = 0; i < n; i++) {
        var x = i / n;
        var floorDb = -121.5 + 2.4 * (1 - x) + 1.1 * Math.sin(x * 7.1 + 0.6);
        floorLin[i] = Math.pow(10, floorDb / 10);
      }
    }

    function addTone(f, db) {
      var pos = (f - F0) / SPAN * bins - 0.5;
      var i0 = Math.floor(pos);
      var w = pos - i0;
      var p = Math.pow(10, db / 10);
      if (i0 >= 0 && i0 < bins) { lin[i0] += p * (1 - w); }
      if (i0 + 1 >= 0 && i0 + 1 < bins) { lin[i0 + 1] += p * w; }
      if (i0 - 1 >= 0 && i0 - 1 < bins) { lin[i0 - 1] += p * 0.004; }
      if (i0 + 2 >= 0 && i0 + 2 < bins) { lin[i0 + 2] += p * 0.004; }
    }

    function activeOp(ch, t) {
      var x = (t + ch.phase) % ch.cycle;
      for (var k = 0; k < ch.ops.length; k++) {
        var op = ch.ops[k];
        if (x < op.dur) { return op; }
        x -= op.dur;
        if (x < op.gap) { return null; }
        x -= op.gap;
      }
      return null;
    }

    // One waterfall row at time t (seconds since the epoch), as linear power per bin.
    function row(t) {
      var i;
      for (i = 0; i < bins; i++) {
        lin[i] = floorLin[i] * -Math.log(Math.random() + 1e-12);
      }

      band.cw.forEach(function (st) {
        var unit = Math.floor((t + st.phase) / st.unit) % st.seq.length;
        if (st.seq[unit]) {
          addTone(st.f, st.level + 5 * Math.sin(6.283 * t / st.qsbPeriod + st.qsbPhase));
        }
      });

      var slot = Math.floor(t / 15);
      var inSlot = t - slot * 15;
      var even = slot % 2 === 0;
      if (inSlot > 0.5 && inSlot < 13.14) {
        var symbol = Math.floor(t / 0.16);
        band.ft8.forEach(function (st) {
          if (st.even === even && hash2(slot, st.id) > 0.22) {
            var tone = Math.floor(hash2(symbol, st.id + 97) * 8);
            addTone(st.f + tone * 6.25, st.level + (hash2(slot, st.id + 31) - 0.5) * 8);
          }
        });
      }

      // RTTY near 7.0835 MHz, two tones 170 Hz apart.
      if (t % 80 < 48) {
        addTone(7083500, -87);
        addTone(7083330, -89);
      }

      // Someone tuning up a carrier now and then.
      if (t % 53 < 2.6) {
        addTone(7195800, -79);
      }

      var binHz = SPAN / bins;
      band.ssb.forEach(function (ch) {
        var op = activeOp(ch, t);
        if (!op) {
          return;
        }
        var e = op.env[Math.floor((t + op.envPhase) / 0.02) % op.env.length];
        if (e < -50) {
          return;
        }
        var qsb = 4 * Math.sin(6.283 * t / ch.qsbPeriod + ch.qsbPhase);
        var iStart = Math.max(0, Math.floor((ch.fc - 2950 - F0) / binHz));
        var iEnd = Math.min(bins - 1, Math.ceil((ch.fc - 120 - F0) / binHz));
        for (var b = iStart; b <= iEnd; b++) {
          var a = ch.fc - (F0 + (b + 0.5) * binHz);
          if (a < 150 - binHz / 2 || a > 2900 + binHz / 2) {
            continue;
          }
          var shape = a > 450 ? -20 * Math.pow((a - 450) / 2450, 1.15) : -9 * (450 - a) / 300;
          var p = op.level + e + shape + qsb + (Math.random() - 0.5) * 9;
          lin[b] += Math.pow(10, p / 10);
        }
      });

      return lin;
    }

    return { resize: resize, row: row };
  }

  // The waterfall's out-of-box palette, WfColorScheme::Default
  // (src/gui/SpectrumWidget.cpp kDefaultStops).
  var WF_DEFAULT = [
    [0.00, 0x000000], [0.15, 0x000080], [0.30, 0x0040ff], [0.45, 0x00c8ff],
    [0.60, 0x00dc00], [0.80, 0xffff00], [1.00, 0xff0000]
  ];

  function buildLut(stops) {
    var lut = new Uint8ClampedArray(256 * 3);
    for (var i = 0; i < 256; i++) {
      var v = i / 255;
      var k = 0;
      while (k < stops.length - 2 && v > stops[k + 1][0]) { k++; }
      var a = stops[k];
      var b = stops[k + 1];
      var u = clamp((v - a[0]) / (b[0] - a[0]), 0, 1);
      lut[i * 3] = ((a[1] >> 16) & 255) + (((b[1] >> 16) & 255) - ((a[1] >> 16) & 255)) * u;
      lut[i * 3 + 1] = ((a[1] >> 8) & 255) + (((b[1] >> 8) & 255) - ((a[1] >> 8) & 255)) * u;
      lut[i * 3 + 2] = (a[1] & 255) + ((b[1] & 255) - (a[1] & 255)) * u;
    }
    return lut;
  }

  // Filter presets as the flag shows them (VfoWidget::formatFilterWidth).
  var MODES = {
    LSB: { lo: -3000, hi: -100, bw: '2.9K' },
    CWU: { lo: -250, hi: 250, bw: '500' },
    DIGU: { lo: 100, hi: 3100, bw: '3.0K' }
  };

  // The 40 m band edges; the display runs 5 kHz past each so no scale label is clipped.
  var BAND_LO = 7000000;
  var BAND_HI = 7300000;

  function modeFor(f) {
    if (f < 7060000) { return 'CWU'; }
    if (f < 7125000) { return 'DIGU'; }
    return 'LSB';
  }

  // VfoWidget::updateFreqLabel: "7.236.400"
  function fmtVfo(f) {
    var hz = Math.round(f);
    var mhz = Math.floor(hz / 1e6);
    var khz = Math.floor((hz % 1e6) / 1e3);
    var rest = hz % 1e3;
    return mhz + '.' + String(khz).padStart(3, '0') + '.' + String(rest).padStart(3, '0');
  }

  var UI_FONT = 'system-ui, -apple-system, "Segoe UI", "Helvetica Neue", Arial, sans-serif';

  function initPan() {
    var pan = document.querySelector('[data-pan]');
    if (!pan) {
      return;
    }
    var specCanvas = pan.querySelector('[data-spectrum]');
    var wfCanvas = pan.querySelector('[data-waterfall]');
    var specCtx = specCanvas.getContext('2d');
    var wfCtx = wfCanvas.getContext('2d', { alpha: false });
    if (!specCtx || !wfCtx) {
      return;
    }
    var specWrap = specCanvas.parentElement;
    var wfWrap = wfCanvas.parentElement;
    var ruler = pan.querySelector('[data-ruler]');
    var passSpec = pan.querySelector('[data-pass-spec]');
    var passWf = pan.querySelector('[data-pass-wf]');
    var lineEl = pan.querySelector('[data-vfo-line]');
    var triEl = pan.querySelector('[data-vfo-tri]');
    var readout = pan.querySelector('[data-cursor]');
    var inert = pan.querySelector('[data-inert]');
    var vfoEl = pan.querySelector('[data-vfo]');
    var freqOut = vfoEl.querySelector('[data-vfo-freq]');
    var bwOut = vfoEl.querySelector('[data-vfo-bw]');
    var modeOut = vfoEl.querySelector('[data-vfo-mode]');
    var levelCanvas = vfoEl.querySelector('[data-vfo-level]');
    var levelCtx = levelCanvas.getContext('2d');
    var fbtnsEl = pan.querySelector('[data-vfo-fbtns]');
    var dashMode = document.querySelector('[data-dash-mode]');
    var dashBw = document.querySelector('[data-dash-bw]');
    var toggleBtn = inert ? inert.querySelector('.ovl__btn--toggle') : null;

    // Display range: reference level -50 dBm over an 85 dB dynamic range.
    var REF = -50;
    var RANGE = 85;
    var WF_LO = -127;
    var WF_HI = -52;
    var ROW_MS = 55;
    var DBM_STRIP_W = 36;
    var DBM_ARROW_H = 14;

    var lut = buildLut(WF_DEFAULT);
    var band = buildBand();
    var sim = makeSimulation(band);

    var W = 0;
    var specH = 0;
    var bins = 0;
    var wfRows = 0;
    var dpr = 1;
    var scale = 1;
    var rowDb = null;
    var disp = null;
    var prev = null;
    var cur = null;
    var rowImg = null;
    var lastLin = null;

    var running = false;
    var visible = true;
    var raf = 0;
    var lastRowAt = 0;
    var lastFrame = 0;

    var vfo = { f: 7236400, target: 7236400, mode: 'LSB', s: -127 };

    function fx(f) {
      return (f - F0) / SPAN * W;
    }

    function dbToY(db) {
      return (REF - db) / RANGE * specH;
    }

    function paintRow(data, offset, row) {
      var span = WF_HI - WF_LO;
      for (var i = 0; i < bins; i++) {
        var v = (row[i] - WF_LO) / span;
        var idx = v <= 0 ? 0 : v >= 1 ? 255 : (v * 255) | 0;
        var o = offset + i * 4;
        data[o] = lut[idx * 3];
        data[o + 1] = lut[idx * 3 + 1];
        data[o + 2] = lut[idx * 3 + 2];
        data[o + 3] = 255;
      }
    }

    function linToDb(lin, out) {
      for (var i = 0; i < bins; i++) {
        out[i] = 10 * Math.log10(lin[i]);
      }
    }

    // SpectrumWidget::drawFreqScale: centred labels every 25 kHz, two decimals.
    function buildRuler() {
      ruler.textContent = '';
      var step = W < 440 ? 50000 : 25000;
      for (var f = Math.ceil(F0 / step) * step; f < F1; f += step) {
        if (fx(f) < 14 || fx(f) > W - 14) {
          continue; // a centred label this close to the edge would be cut in half
        }
        var s = document.createElement('span');
        s.textContent = (f / 1e6).toFixed(2);
        s.style.left = (fx(f) / W * 100) + '%';
        ruler.appendChild(s);
      }
    }

    function setup() {
      var rect = pan.getBoundingClientRect();
      W = Math.max(1, Math.round(rect.width));
      specH = Math.max(40, Math.round(specWrap.getBoundingClientRect().height));
      dpr = Math.min(window.devicePixelRatio || 1, 2);
      scale = W < 560 ? 0.8 : 1;
      bins = clamp(W, 480, 2400);
      wfRows = Math.max(60, Math.round(wfWrap.getBoundingClientRect().height));

      specCanvas.width = Math.round(W * dpr);
      specCanvas.height = Math.round(specH * dpr);
      wfCanvas.width = bins;
      wfCanvas.height = wfRows;

      if (inert) {
        // SpectrumOverlayPanel starts expanded; on a phone it would cover a
        // fifth of the band, so show its collapsed state there instead.
        var collapsed = W < 640;
        inert.classList.toggle('is-collapsed', collapsed);
        if (toggleBtn) {
          toggleBtn.textContent = collapsed ? '▶' : '◀';
        }
      }

      sim.resize(bins);
      rowDb = new Float32Array(bins);
      disp = new Float32Array(bins);
      prev = new Float32Array(bins);
      cur = new Float32Array(bins);
      rowImg = wfCtx.createImageData(bins, 1);
      buildRuler();
      prefill();
      placeVfo();
      drawSpectrum(1);
    }

    // Fill the whole waterfall straight away so it never starts empty.
    function prefill() {
      var img = wfCtx.createImageData(bins, wfRows);
      var now = Date.now();
      for (var y = wfRows - 1; y >= 0; y--) {
        var lin = sim.row((now - y * ROW_MS) / 1000);
        linToDb(lin, rowDb);
        paintRow(img.data, y * bins * 4, rowDb);
        if (y === 8) {
          disp.set(rowDb);
        } else if (y < 8) {
          for (var i = 0; i < bins; i++) {
            disp[i] += (rowDb[i] - disp[i]) * 0.5;
          }
        }
        if (y === 0) {
          lastLin = Float32Array.from(lin);
        }
      }
      wfCtx.putImageData(img, 0, 0);
      prev.set(disp);
      cur.set(disp);
      meter(lastLin, true);
    }

    function pushRow() {
      var lin = sim.row(Date.now() / 1000);
      linToDb(lin, rowDb);
      prev.set(cur);
      for (var i = 0; i < bins; i++) {
        disp[i] += (rowDb[i] - disp[i]) * 0.5;
      }
      cur.set(disp);
      wfCtx.drawImage(wfCanvas, 0, 0, bins, wfRows - 1, 0, 1, bins, wfRows - 1);
      paintRow(rowImg.data, 0, rowDb);
      wfCtx.putImageData(rowImg, 0, 0);
      lastLin = lin;
      meter(lin, false);
    }

    // SpectrumWidget::drawDbmScale: right-edge strip with the up/down arrows
    // and a label every 10 dB (adaptiveStepDb for an 85 dB range).
    function drawDbmStrip(c) {
      var x0 = W - DBM_STRIP_W;
      c.fillStyle = 'rgba(10, 10, 24, 0.863)';
      c.fillRect(x0, 0, DBM_STRIP_W, specH);
      c.strokeStyle = '#304050';
      c.lineWidth = 1;
      c.beginPath();
      c.moveTo(x0 + 0.5, 0);
      c.lineTo(x0 + 0.5, specH);
      c.stroke();

      var half = DBM_STRIP_W / 2;
      var upCx = x0 + half / 2;
      var dnCx = x0 + half + half / 2;
      var aTop = 2;
      var aBot = DBM_ARROW_H - 2;
      c.fillStyle = '#6080a0';
      c.beginPath();
      c.moveTo(upCx - 5, aBot);
      c.lineTo(upCx + 5, aBot);
      c.lineTo(upCx, aTop);
      c.closePath();
      c.fill();
      c.beginPath();
      c.moveTo(dnCx - 5, aTop);
      c.lineTo(dnCx + 5, aTop);
      c.lineTo(dnCx, aBot);
      c.closePath();
      c.fill();

      var step = RANGE / 5 >= 20 ? 20 : RANGE / 5 >= 10 ? 10 : RANGE / 5 >= 5 ? 5 : 2;
      var labelTop = DBM_ARROW_H + 4;
      c.font = '8px ' + UI_FONT;
      c.textAlign = 'left';
      c.textBaseline = 'middle';
      for (var db = Math.ceil((REF - RANGE) / step) * step; db <= REF; db += step) {
        var y = Math.round(dbToY(db));
        if (y < labelTop || y > specH - 5) {
          continue;
        }
        c.strokeStyle = '#507080';
        c.beginPath();
        c.moveTo(x0, y + 0.5);
        c.lineTo(x0 + 4, y + 0.5);
        c.stroke();
        c.fillStyle = '#80a0b0';
        c.fillText(String(db), x0 + 6, y);
      }
    }

    function drawSpectrum(frac) {
      var c = specCtx;
      c.setTransform(dpr, 0, 0, dpr, 0, 0);
      c.fillStyle = '#0a0a14';
      c.fillRect(0, 0, W, specH);

      // Grid: dotted minor lines every 5 kHz, major lines every 25 kHz and 10 dB.
      var f;
      var x;
      c.lineWidth = 1;
      c.strokeStyle = 'rgba(255, 255, 255, 0.078)';
      c.setLineDash([1, 2]);
      c.beginPath();
      for (f = Math.ceil(F0 / 5000) * 5000; f <= F1; f += 5000) {
        if (f % 25000 === 0) {
          continue;
        }
        x = Math.round(fx(f)) + 0.5;
        c.moveTo(x, 0);
        c.lineTo(x, specH);
      }
      c.stroke();
      c.setLineDash([]);
      c.strokeStyle = 'rgba(255, 255, 255, 0.157)';
      c.beginPath();
      for (var db = Math.ceil((REF - RANGE) / 10) * 10; db <= REF; db += 10) {
        var gy = Math.round(dbToY(db)) + 0.5;
        c.moveTo(0, gy);
        c.lineTo(W, gy);
      }
      for (f = Math.ceil(F0 / 25000) * 25000; f <= F1; f += 25000) {
        x = Math.round(fx(f)) + 0.5;
        c.moveTo(x, 0);
        c.lineTo(x, specH);
      }
      c.stroke();

      // Trace: #00e5ff at 1.5 px over a flat fill at 0.70 x 0.4 alpha.
      var trace = new Path2D();
      for (var i = 0; i < bins; i++) {
        var v = prev[i] + (cur[i] - prev[i]) * frac;
        var tx = (i + 0.5) / bins * W;
        var ty = dbToY(v);
        if (i === 0) {
          trace.moveTo(0, ty);
        }
        trace.lineTo(tx, ty);
      }
      var fill = new Path2D(trace);
      fill.lineTo(W, specH);
      fill.lineTo(0, specH);
      fill.closePath();
      c.fillStyle = 'rgba(0, 229, 255, 0.28)';
      c.fill(fill);
      c.lineWidth = 1.5;
      c.lineJoin = 'round';
      c.strokeStyle = '#00e5ff';
      c.stroke(trace);

      drawDbmStrip(c);
    }

    // The flag's S-meter strip, ported from VfoLevelBar::paintEvent
    // (src/gui/widgets/VfoLevelBar.cpp): tick labels S1..+40 over a 14 px bar,
    // cyan below S9 blending to green above it, dBm readout on the right.
    function drawLevel(dbm) {
      var w = 240;
      var h = 26;
      var ratio = Math.min(window.devicePixelRatio || 1, 3);
      if (levelCanvas.width !== Math.round(w * ratio)) {
        levelCanvas.width = Math.round(w * ratio);
        levelCanvas.height = Math.round(h * ratio);
      }
      var c = levelCtx;
      c.setTransform(ratio, 0, 0, ratio, 0, 0);
      c.clearRect(0, 0, w, h);
      var FLOOR = -130;
      var CEIL = -20;
      var S9 = -73;
      var TOP = 4;
      var TICK_H = 8;
      var DBM_W = 52;
      var barW = w - DBM_W - 2;
      var ticks = [-121, -109, -97, -85, -73, -53, -33];
      var labels = ['S1', '3', '5', '7', '9', '+20', '+40'];
      c.fillStyle = '#6888a0';
      c.font = '8px ' + UI_FONT;
      c.textBaseline = 'middle';
      for (var i = 0; i < ticks.length; i++) {
        var x = Math.floor((ticks[i] - FLOOR) / (CEIL - FLOOR) * (barW - 1));
        c.fillRect(x, TOP + TICK_H - 2, 1, 3);
        c.textAlign = i === 0 ? 'left' : i >= 5 ? 'right' : 'center';
        c.fillText(labels[i], x, TOP + (TICK_H - 2) / 2);
      }
      var barTop = TOP + TICK_H;
      var barH = h - barTop;
      c.fillStyle = '#10101c';
      c.fillRect(0, barTop, barW, barH);
      c.strokeStyle = '#304050';
      c.lineWidth = 1;
      c.strokeRect(0.5, barTop + 0.5, barW - 1, barH - 1);
      var innerW = barW - 2;
      var fillW = Math.floor(clamp((dbm - FLOOR) / (CEIL - FLOOR), 0, 1) * innerW);
      if (fillW > 0) {
        var s9 = (S9 - FLOOR) / (CEIL - FLOOR);
        var g = c.createLinearGradient(1, 0, 1 + innerW, 0);
        g.addColorStop(0, '#00b4d8');
        g.addColorStop(s9, '#00b4d8');
        g.addColorStop(Math.min(s9 + 0.15, 1), '#00d860');
        g.addColorStop(1, '#00d860');
        c.fillStyle = g;
        c.fillRect(1, barTop + 1, fillW, barH - 2);
      }
      c.fillStyle = dbm >= S9 ? '#00d860' : '#00b4d8';
      c.font = 'bold 10px ' + UI_FONT;
      c.textAlign = 'left';
      c.fillText(Math.round(dbm) + ' dBm', barW + 3, barTop + barH / 2);
    }

    // S-meter reading: power summed across the passband, fast attack and slow decay.
    function meter(lin, instant) {
      if (!lin) {
        return;
      }
      var m = MODES[vfo.mode];
      var a = (vfo.f + m.lo - F0) / SPAN * bins;
      var b = (vfo.f + m.hi - F0) / SPAN * bins;
      var i0 = clamp(Math.floor(a), 0, bins - 1);
      var i1 = clamp(Math.ceil(b), i0 + 1, bins);
      var sum = 0;
      for (var i = i0; i < i1; i++) {
        sum += lin[i];
      }
      var p = 10 * Math.log10(sum);
      if (instant) {
        vfo.s = p;
      } else {
        vfo.s += (p - vfo.s) * (p > vfo.s ? 0.55 : 0.12);
      }
      drawLevel(vfo.s);
    }

    function placeVfo() {
      if (!W) {
        return;
      }
      var m = MODES[vfo.mode];
      var xv = fx(vfo.f);
      lineEl.style.transform = 'translateX(' + xv.toFixed(1) + 'px)';
      var x1 = fx(vfo.f + m.lo);
      var x2 = fx(vfo.f + m.hi);
      var pw = Math.max(2, x2 - x1).toFixed(1) + 'px';
      passSpec.style.transform = passWf.style.transform = 'translateX(' + x1.toFixed(1) + 'px)';
      passSpec.style.width = passWf.style.width = pw;

      // VfoWidget::updatePosition: lower-sideband modes hang the flag to the
      // right of the VFO, everything else to the left, flipping at the edges.
      var flagW = 252 * scale;
      var onLeft = !(vfo.mode === 'LSB' || vfo.mode === 'DIGL' || vfo.mode === 'CWL');
      var x;
      if (onLeft) {
        x = xv - flagW;
        if (x < 0) {
          x = xv;
          onLeft = false;
        }
      } else {
        x = xv;
        if (x + flagW > W) {
          x = xv - flagW;
          onLeft = true;
        }
      }
      x = clamp(x, 0, Math.max(0, W - flagW));
      vfoEl.style.transform = 'translateX(' + x.toFixed(1) + 'px) scale(' + scale + ')';

      // VfoWidget::positionFloatingButtons: 22 px left of the flag, or 2 px
      // past its right edge when the flag hangs to the left.
      var bx = onLeft ? x + flagW + 2 * scale : x - 22 * scale;
      bx = clamp(bx, 0, W - 20 * scale);
      fbtnsEl.style.transform = 'translateX(' + bx.toFixed(1) + 'px) scale(' + scale + ')';

      // The VFO triangle hangs from the flag's bottom edge.
      var triTop = Math.min(Math.round(vfoEl.offsetHeight * scale), specH - 10);
      triEl.style.transform = 'translate(' + xv.toFixed(1) + 'px, ' + triTop + 'px)';

      freqOut.textContent = fmtVfo(vfo.f);
    }

    function setTarget(f, mode, snapNow) {
      vfo.target = clamp(Math.round(f), BAND_LO, BAND_HI);
      if (mode !== vfo.mode) {
        vfo.mode = mode;
        modeOut.textContent = mode;
        bwOut.textContent = MODES[mode].bw;
        if (dashMode) {
          dashMode.textContent = mode;
        }
        if (dashBw) {
          dashBw.textContent = ((MODES[mode].hi - MODES[mode].lo) / 1000).toFixed(1) + 'k';
        }
      }
      vfoEl.setAttribute('aria-valuenow', String(vfo.target));
      vfoEl.setAttribute('aria-valuetext', (vfo.target / 1e6).toFixed(6) + ' megahertz, ' + mode);
      if (snapNow || !running) {
        vfo.f = vfo.target;
        placeVfo();
        meter(lastLin, true);
      }
    }

    function tuneTo(f) {
      for (var k = 0; k < band.ssb.length; k++) {
        var ch = band.ssb[k];
        if (f >= ch.fc - 3200 && f <= ch.fc + 500) {
          return setTarget(ch.fc, 'LSB');
        }
      }
      for (var c = 0; c < band.cw.length; c++) {
        if (Math.abs(f - band.cw[c].f) <= 450) {
          return setTarget(band.cw[c].f, 'CWU');
        }
      }
      if (f >= 7073500 && f <= 7077500) {
        return setTarget(7074000, 'DIGU');
      }
      var stepped = clamp(Math.round(f / 100) * 100, BAND_LO, BAND_HI);
      setTarget(stepped, modeFor(stepped));
    }

    function isChrome(target) {
      return vfoEl.contains(target) || (inert && inert.contains(target));
    }

    // Cursor readout: "%.4f MHz", 12 px right of the pointer and above it,
    // flipping at the right and top edges.
    pan.addEventListener('pointermove', function (e) {
      if (isChrome(e.target)) {
        readout.classList.remove('is-on');
        return;
      }
      var rect = pan.getBoundingClientRect();
      var x = clamp(e.clientX - rect.left, 0, rect.width);
      var y = clamp(e.clientY - rect.top, 0, rect.height);
      readout.textContent = ((F0 + x / rect.width * SPAN) / 1e6).toFixed(4) + ' MHz';
      var bw = readout.offsetWidth;
      var bh = readout.offsetHeight;
      var rx = x + 12;
      var ry = y - bh - 4;
      if (rx + bw > rect.width) {
        rx = x - 12 - bw;
      }
      if (ry < 0) {
        ry = y + 16;
      }
      readout.style.transform = 'translate(' + rx.toFixed(0) + 'px, ' + ry.toFixed(0) + 'px)';
      readout.classList.add('is-on');
    });
    pan.addEventListener('pointerleave', function () {
      readout.classList.remove('is-on');
    });
    pan.addEventListener('click', function (e) {
      if (isChrome(e.target)) {
        return;
      }
      var rect = pan.getBoundingClientRect();
      tuneTo(F0 + clamp(e.clientX - rect.left, 0, rect.width) / rect.width * SPAN);
    });

    vfoEl.addEventListener('keydown', function (e) {
      var step = e.shiftKey ? 100 : 1000;
      var t = vfo.target;
      switch (e.key) {
        case 'ArrowRight':
        case 'ArrowUp':
          t += step;
          break;
        case 'ArrowLeft':
        case 'ArrowDown':
          t -= step;
          break;
        case 'PageUp':
          t += 10000;
          break;
        case 'PageDown':
          t -= 10000;
          break;
        case 'Home':
          t = BAND_LO;
          break;
        case 'End':
          t = BAND_HI;
          break;
        default:
          return;
      }
      e.preventDefault();
      t = clamp(t, BAND_LO, BAND_HI);
      setTarget(t, modeFor(t));
    });

    function frame(now) {
      if (!running) {
        return;
      }
      if (now - lastRowAt > 1000) {
        lastRowAt = now - ROW_MS;
      }
      var produced = 0;
      while (now - lastRowAt >= ROW_MS && produced < 4) {
        lastRowAt += ROW_MS;
        pushRow();
        produced++;
      }
      drawSpectrum(clamp((now - lastRowAt) / ROW_MS, 0, 1));

      var dt = now - lastFrame;
      lastFrame = now;
      if (vfo.f !== vfo.target) {
        var d = vfo.target - vfo.f;
        vfo.f = Math.abs(d) < 4 ? vfo.target : vfo.f + d * Math.min(1, dt / 70);
        placeVfo();
      }
      raf = requestAnimationFrame(frame);
    }

    function sync() {
      var should = visible && !document.hidden && !REDUCED;
      if (should && !running) {
        running = true;
        lastRowAt = performance.now();
        lastFrame = lastRowAt;
        raf = requestAnimationFrame(frame);
      } else if (!should && running) {
        running = false;
        cancelAnimationFrame(raf);
        vfo.f = vfo.target;
        placeVfo();
      }
    }

    setup();

    if ('IntersectionObserver' in window) {
      new IntersectionObserver(function (entries) {
        visible = entries[0].isIntersecting;
        sync();
      }).observe(pan);
    }
    document.addEventListener('visibilitychange', sync);
    sync();

    var lastWidth = W;
    var resizeTimer = 0;
    function onResize() {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(function () {
        var w = Math.round(pan.getBoundingClientRect().width);
        if (Math.abs(w - lastWidth) >= 24) {
          lastWidth = w;
          setup();
        } else {
          placeVfo();
        }
      }, 160);
    }
    if ('ResizeObserver' in window) {
      new ResizeObserver(onResize).observe(pan);
    } else {
      window.addEventListener('resize', onResize);
    }
  }

  safe(initReveal);
  safe(initNav);
  safe(initClock);
  safe(initOs);
  safe(initTabs);
  safe(initCopy);
  safe(initRelease);
  safe(initPan);
})();
