/* Roundtable overlay board: one scenario, drawn on the desktop and on the phones.
   Rules carried from the addendum (../2026-10-29-roundtable-frequency-first/live-reports-addendum.md):
   transmitters never appear among the listeners; readings during a double are combined RF;
   after a double the remaining talker starts with no readings; ended readings keep their age. */
(function () {
  'use strict';
  var N = window.NEREUS;
  var YOU = 'KG4VCF';

  // ---------- Formatting: the app's own rules ----------
  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function sText(dbm) { // RoundtableTagLayout.cpp roundtableSUnitText
    var over = Math.round(dbm + 73);
    if (over < 1) { return 'S' + clamp(Math.round(9 - (-73 - dbm) / 6), 0, 9); }
    return 'S9+' + over;
  }
  function signal(dbm, fmt) {
    var d = String(Math.round(dbm)).replace('-', '−');
    return fmt === 'S' ? sText(dbm) : fmt === 'both' ? sText(dbm) + ' ' + d : d;
  }
  function age(s) { return s < 60 ? Math.floor(s) + 's' : s < 3600 ? Math.floor(s / 60) + 'm' : Math.floor(s / 3600) + 'h'; }
  function mmss(s) { s = Math.max(0, Math.floor(s)); return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0'); }
  function frac(dbm) { return clamp((dbm + 121) / 88, 0.02, 1); } // the flag meter's scale: S1 -121 ... +40 -33
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function freqShort(hz) { var d = hz % 1000 === 0 ? 3 : hz % 100 === 0 ? 4 : 5; return (hz / 1e6).toFixed(d); }

  // ---------- The station list ----------
  var LISTENERS = [
    { c: 'N4RDR', d: -66.7, mi: 179, br: 39 },
    { c: YOU, d: -73.0, mi: 212, br: 135, you: true },
    { c: 'KD8LMN', d: -78.5, mi: 496, br: 12 },
    { c: 'AA4QQ', d: -110.1, mi: 565, br: 50 },
    { c: 'N0SPM', d: -77.6, mi: 872, br: 333 },
    { c: 'W5ABC', d: -88.2, mi: 901, br: 281 },
    { c: 'KC1ABC', d: -94.4, mi: 1103, br: 54 },
    { c: 'G4ABC', d: -90.6, mi: 4209, br: 45 }
  ];
  var S = {
    state: 'talk', look: 'new', fmt: 'both', count: 5, link: 'live',
    t0: Date.now(), playing: false,
    ux: 'one', rx: 'Signal', pan: 'tall'
  };
  // The one-inspector proposal: every Roundtable click opens one inspector, in one place.
  function isOne() { return S.look === 'new' && S.ux === 'one'; }
  var RX_MODES = ['Signal', 'Sig Avg', 'Signal Peak', 'Max Bin'];
  // FlagMeterChoice.swift tag: only Sig Avg and Max Bin print one.
  function rxTag(rx) { return rx === 'Sig Avg' ? 'AV' : rx === 'Max Bin' ? 'MAX' : ''; }
  var offsets = {}; // live wander per listener
  LISTENERS.forEach(function (l) { offsets[l.c] = { v: 0, pk: 0, a: 1 + Math.random() * 3 }; });

  // ---------- States ----------
  var STATES = [
    { id: 'talk', chip: 'One talking', t: 'One station talking', s: 'Who is talking, and how each listener hears them, with no click.' },
    { id: 'double', chip: 'Double', t: 'A double', s: 'Two keyed at once. Both names, and every reading marked combined.' },
    { id: 'double3', chip: 'Three keyed', t: 'Three keyed', s: 'Every keyed station stays named while room allows.' },
    { id: 'after', chip: 'One drops', t: 'One drops out', s: 'Back to the remaining talker. Old combined readings are gone; new ones arrive.' },
    { id: 'you', chip: 'You talking', t: 'You on the air', s: 'Who hears you, in your color.' },
    { id: 'youdbl', chip: 'You doubled', t: 'You are doubling', s: 'You cannot hear it while keyed, so this is the one place you can see it.' },
    { id: 'unknown', chip: 'Unknown', t: 'Unknown signal', s: 'Listeners hear something and no station claims it.' },
    { id: 'one', chip: 'One listener', t: 'One listener', s: 'One valid reading is enough to show.' },
    { id: 'ended', chip: 'Just ended', t: 'Just ended', s: 'Stops moving and says so. Readings keep their age.' },
    { id: 'lastdbl', chip: 'Double ended', t: 'A double just ended', s: 'History, never a live double.' },
    { id: 'old', chip: 'Gone old', t: 'Readings gone old', s: 'Past 20 seconds the bars grey and the age turns amber.' },
    { id: 'quiet', chip: 'Nobody yet', t: 'Nobody has talked', s: 'Only who is here. No empty card.' }
  ];
  var byId = {};
  STATES.forEach(function (x) { byId[x.id] = x; });

  // Build the card's data for a state.
  function model(state, freq, opts) {
    opts = opts || {};
    var m = { state: state, f: freq, talkers: [], kind: 'talk', live: true, reports: [], clock: 0, note: '', count: opts.count || S.count };
    var elapsed = (Date.now() - S.t0) / 1000;
    if (opts.loop) { elapsed = elapsed % 11; }
    var without = function (names) {
      return LISTENERS.filter(function (l) { return names.indexOf(l.c) < 0; }).slice(0, m.count).map(function (l) {
        var o = offsets[l.c];
        return { c: l.c, you: !!l.you, d: l.d + o.v, pk: l.d + o.pk, a: o.a, mi: l.mi, br: l.br };
      });
    };
    switch (state) {
      case 'talk': m.talkers = ['K4ABC']; m.reports = without(['K4ABC']); break;
      case 'double': m.kind = 'dbl'; m.talkers = ['K4ABC', 'N4XYZ']; m.reports = without(m.talkers); m.clock = 3 + elapsed; break;
      case 'double3': m.kind = 'dbl'; m.talkers = ['K4ABC', 'N4XYZ', 'W9ABC']; m.reports = without(m.talkers); m.clock = 5 + elapsed; break;
      case 'after':
        m.talkers = ['K4ABC'];
        // The new turn opens empty; its own readings arrive one by one.
        var r = without(['K4ABC']);
        m.reports = r.slice(0, Math.floor(clamp(elapsed / 0.9, 0, r.length)));
        m.reports.forEach(function (x) { x.a = Math.min(x.a, elapsed); });
        m.note = elapsed < 8 ? 'Double ended ' + Math.floor(elapsed) + 's ago' : '';
        break;
      case 'you': m.kind = 'you'; m.talkers = [YOU]; m.reports = without([YOU]); break;
      case 'youdbl': m.kind = 'dbl'; m.talkers = [YOU, 'K4ABC']; m.reports = without(m.talkers); m.clock = 2 + elapsed; break;
      case 'unknown': m.kind = 'unk'; m.talkers = []; m.reports = without([]); break;
      case 'one': m.talkers = ['K4ABC']; m.reports = without(['K4ABC']).slice(0, 1); break;
      case 'ended': case 'old':
        m.kind = 'last'; m.live = false; m.talkers = ['K4ABC'];
        m.ago = state === 'old' ? 34 + elapsed : 6 + elapsed;
        m.reports = without(['K4ABC']).map(function (x) { x.a = x.a + m.ago; x.old = x.a > 20; return x; });
        break;
      case 'lastdbl':
        m.kind = 'last'; m.wasDouble = true; m.live = false; m.talkers = ['K4ABC', 'N4XYZ'];
        m.ago = 4 + elapsed;
        m.reports = without(m.talkers).map(function (x) { x.a = x.a + m.ago; x.old = x.a > 20; return x; });
        break;
      case 'quiet': m.kind = 'quiet'; m.live = false; m.people = ['K4ABC', 'N4XYZ', 'W9ABC', YOU]; break;
    }
    return m;
  }
  function applyLink(m) {
    m.link = S.link;
    if (S.link === 'recon' || S.link === 'offline') { m.frozen = true; }
    return m;
  }

  // ---------- The proposed card ----------
  // width: 232 desktop, 176 phone, about 132 in the phone's narrow strip.
  function cardHtml(m, o) {
    var narrow = o.w < 150;
    var tight = o.w < 122; // the strip beside the phone's flag: the flag already says the frequency
    var cls = 'rc rc--' + m.kind + (tight ? ' rc--tight' : '');
    if (S.link === 'offline') { cls += ' is-off is-frozen'; }
    if (S.link === 'hidden') { cls += ' is-dim'; }
    if (S.link === 'recon') { cls += ' is-frozen'; }
    var g = m.kind === 'dbl' ? 'dbl' : m.kind === 'you' ? 'you' : m.kind === 'unk' ? 'unk' : m.kind === 'talk' ? 'talk' : 'quiet';
    var h = '<div class="rc__rail"></div><div class="rc__body">';
    var f = tight ? '' : '<span class="rc__f">' + freqShort(m.f) + '</span>';
    if (m.kind === 'dbl') {
      h += '<div class="rc__head"><i class="rcg rcg--dbl"></i><b class="rc__who">' + (m.talkers.length > 2 ? 'Double · ' + m.talkers.length : 'Double') + '</b>' +
        (narrow ? '' : '<span class="rc__clock" data-clock>' + mmss(m.clock) + '</span>') + f + '</div>';
      var shown = o.maxNames ? m.talkers.slice(0, o.maxNames) : m.talkers;
      var row = !narrow && shown.length === 2;
      h += '<div class="rc__names' + (row ? ' rc__names--row' : '') + '">' + shown.map(function (n) {
        return '<span class="rc__name' + (n === YOU ? ' is-you' : '') + '"><i></i>' + (n === YOU ? 'You' : n) + '</span>';
      }).join('') + (m.talkers.length > shown.length ? '<span class="rc__note">+' + (m.talkers.length - shown.length) + ' keyed</span>' : '') + '</div>';
    } else if (m.kind === 'unk') {
      h += '<div class="rc__head"><i class="rcg rcg--unk"></i><b class="rc__who">Unknown</b>' + f + '</div>';
    } else if (m.kind === 'you') {
      h += '<div class="rc__head"><i class="rcg rcg--you"></i><b class="rc__who">You' + (narrow ? '' : ' · on the air') + '</b>' + f + '</div>';
    } else if (m.kind === 'last') {
      h += '<div class="rc__head"><i class="rcg rcg--' + (m.wasDouble ? 'dbl' : 'quiet') + '"' + (m.wasDouble ? ' style="filter:saturate(0.15);opacity:.8"' : '') + '></i><b class="rc__who">' +
        (m.wasDouble ? (narrow ? 'Last double' : 'Last double') : m.talkers[0]) + '</b>' + f + '</div>';
      if (m.wasDouble) { h += '<div class="rc__note">' + m.talkers.join(' + ') + '</div>'; }
    } else {
      h += '<div class="rc__head"><i class="rcg rcg--' + g + '"></i><b class="rc__who">' + m.talkers[0] + '</b>' + f + '</div>';
    }
    if (S.link === 'recon') { h += '<div class="rc__note" style="color:var(--rc-unk)"><i class="rcg rcg--wait" style="width:8px;height:8px;margin-right:4px;vertical-align:-1px"></i>Reconnecting</div>'; }
    if (S.link === 'offline') { h += '<div class="rc__note">Offline since 14:32 UTC</div>'; }
    if (S.link === 'hidden') { h += '<div class="rc__note">You are hidden</div>'; }

    var stale = S.link === 'recon' || S.link === 'offline';
    var cap = stale ? 'Last known' + (m.kind === 'dbl' && !narrow ? ' \u00b7 combined' : '') : m.kind === 'dbl' ? '<b>Combined RF</b>' : m.kind === 'you' ? 'Hearing you' : m.kind === 'last' ? (m.wasDouble ? 'Combined RF' : 'Last heard ' + age(m.ago) + ' ago') : narrow ? 'Heard by' : 'Heard by';
    var rows = m.reports;
    var maxRows = o.maxRows == null ? rows.length : o.maxRows;
    var showRows = rows.slice(0, maxRows);
    if (m.note) { h += '<div class="rc__note" data-note>' + m.note + '</div>'; }
    h += '<div class="rc__cap"><span>' + cap + '</span>' + (showRows.length && !narrow ? '<span>' + (S.fmt === 'S' ? 'S' : S.fmt === 'both' ? 'S / dBm' : 'dBm') + '</span>' : '') + '</div>';
    if (showRows.length) {
      h += '<div class="rc__rows">' + showRows.map(function (r) { return rowHtml(r, narrow, m); }).join('') + '</div>';
    } else if (m.state === 'after') {
      h += '<div class="rc__note">Waiting for readings</div>';
    } else {
      h += '<div class="rc__note">No readings</div>';
    }
    var more = rows.length - showRows.length;
    if (more > 0) { h += '<div class="rc__foot"><span>+' + more + ' more ›</span></div>'; }
    h += '</div>';
    return { cls: cls, html: h };
  }
  function rowHtml(r, narrow, m) {
    var fmt = narrow && S.fmt === 'both' ? 'S' : S.fmt;
    return '<div class="rr' + (r.you ? ' is-you' : '') + (r.old ? ' is-old' : '') + '" data-call="' + esc(r.c) + '" data-a0="' + r.a + '">' +
      '<span class="rr__c">' + (r.label ? esc(r.label) : r.you ? 'You' : esc(r.c)) + '</span>' +
      (m.showPath ? '<span class="rr__p">' + (r.mi ? r.mi.toLocaleString() + ' mi ' + String(r.br).padStart(3, '0') + '\u00b0' : '') + '</span>' : '') +
      '<span class="rr__v" data-fmt="' + fmt + '" data-v>' + signal(r.d, fmt) + '</span>' +
      '<span class="rr__a" data-a>' + age(r.a) + '</span>' +
      '<span class="rr__m"><i style="width:' + (frac(r.d) * 100).toFixed(1) + '%"></i>' + (m.live && !r.old ? '<u style="left:' + (frac(r.pk) * 100).toFixed(1) + '%"></u>' : '') + '</span>' +
      '</div>';
  }
  // How tall the card is for a row count, measured in place.
  function mountCard(host, m, o) {
    var c = cardHtml(m, o);
    host.className = c.cls;
    host.innerHTML = c.html;
    host.style.width = o.w + 'px';
  }

  // ---------- Today's build, by its own rules ----------
  function todayDesktop(m) {
    // RoundtableTagLayout.cpp roundtableTagReportLines + roundtableCochannelRows
    var unit = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'S / dBm' : 'dBm';
    var l = [freqShort(m.f) + ' MHz'];
    var dbl = m.kind === 'dbl';
    if (dbl) {
      l.push('Double' + (m.talkers.length > 2 ? ' (' + m.talkers.length + ')' : '') + ': ' + m.talkers.join(' + '));
    } else if (m.kind === 'unk') { l.push('Unknown signal'); } else if (m.kind === 'quiet') { l = l.concat(m.people.slice(0, 3)); l.push('+1 people ›'); } else if (m.kind === 'last') {
      l.push(m.wasDouble ? 'Last double: ' + m.talkers.join(' + ') : 'Last talker: ' + m.talkers[0]);
    } else { l.push('Talking: ' + m.talkers[0]); }
    if (m.kind !== 'quiet') {
      l.push((dbl ? 'Combined RF · Heard by · ' : 'Heard by     ') + unit + (dbl ? ' · Age' : '    Age'));
      m.reports.forEach(function (r) { l.push(r.c + '  ' + signal(r.d, S.fmt) + '  ' + age(r.a) + (r.old ? ' stale' : '')); });
      l.push('Reports ›');
    }
    return '<div class="rt0 rt0--d" style="--rc-c:' + borderOf(m) + ';width:' + widest(l) + 'px">' + l.map(function (t, i) {
      return '<span class="l' + (i === 0 || i === l.length - 1 ? ' l--dim' : '') + '">' + esc(t) + '</span>';
    }).join('') + '</div>';
  }
  function widest(lines) {
    var w = 208;
    lines.forEach(function (t) { w = Math.max(w, Math.ceil(t.length * 6.3) + 14); });
    return w;
  }
  function todayPhone(m, w) {
    // RoundtableTagLayer.swift cardPlan: reports only at 160 wide or more
    var h = '<div class="f">' + freqShort(m.f) + ' MHz</div>';
    if (m.kind === 'dbl') {
      h += '<div class="c">Combined RF</div><div class="t dbl">Double' + (m.talkers.length > 2 ? ' (' + m.talkers.length + ' TX)' : '') + '</div>';
      m.talkers.forEach(function (t) { h += '<div class="n">' + t + '</div>'; });
    } else if (m.kind === 'unk') { h += '<div class="t unk">Unknown signal</div>'; } else if (m.kind === 'quiet') {
      m.people.slice(0, w < 160 ? 2 : 3).forEach(function (t) { h += '<div class="n" style="font-size:11px">' + t + '</div>'; });
    } else if (m.kind === 'last') {
      h += '<div class="c">' + (m.wasDouble ? 'Last overlap' : 'Last talker') + '</div><div class="t' + (m.wasDouble ? ' dbl' : '') + '">' + (m.wasDouble ? 'Double' : m.talkers[0]) + '</div>';
    } else { h += '<div class="c">Last talker</div><div class="t">' + m.talkers[0] + '</div>'; }
    var rows = w >= 160 && m.reports ? m.reports : [];
    if (rows.length) {
      h += '<div class="h"><span>' + (m.kind === 'dbl' ? 'Combined RF' : 'Heard by') + '</span><span>' + (S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm') + '</span><span>Age</span></div>';
      rows.forEach(function (r) { h += '<div class="r"><span>' + r.c + '</span><span>' + signal(r.d, S.fmt) + '</span><span>' + age(r.a) + '</span></div>'; });
    }
    var omitted = (m.reports || []).length - rows.length;
    h += '<div class="d">' + (omitted > 0 ? '+' + omitted + ' reports ›' : 'Reports ›') + '</div>';
    return '<div class="rt0 rt0--p" style="--rc-c:' + borderOf(m) + ';width:' + w + 'px">' + h + '</div>';
  }
  function borderOf(m) {
    return m.kind === 'dbl' ? '#c25a5c' : m.kind === 'unk' ? '#d8a55f' : m.kind === 'you' ? '#00d4ff' : m.kind === 'talk' ? '#4caf6a' : '#606066';
  }

  // ---------- Other frequencies ----------
  function tagHtml(t) {
    var off = S.link === 'offline' ? ' is-off' : S.link === 'hidden' ? ' is-dim' : '';
    if (S.look === 'today') {
      // Today every keyed station is a green dot on another frequency: a double is not marked.
      return '<div class="rct rct--' + (t.keyed.length ? 'talk' : 'quiet') + off + '">' + t.calls.slice(0, 3).map(function (c) {
        return '<span class="rct__r"><i class="rcg rcg--' + (t.keyed.indexOf(c) >= 0 ? 'talk' : 'quiet') + '"></i>' + c + '</span>';
      }).join('') + '</div>';
    }
    if (t.keyed[0] === '?') {
      return '<div class="rct rct--unk' + off + '"><span class="rct__r"><i class="rcg rcg--unk"></i>Unknown</span></div>';
    }
    if (t.keyed.length > 1) {
      return '<div class="rct rct--dbl' + off + '"><span class="rct__r rct__d"><i class="rcg rcg--dbl"></i>Double</span>' + t.keyed.slice(0, 2).map(function (c) {
        return '<span class="rct__r" style="padding-left:14px">' + c + '</span>';
      }).join('') + '</div>';
    }
    if (t.keyed.length === 1) {
      return '<div class="rct rct--talk' + off + '"><span class="rct__r"><i class="rcg rcg--talk"></i>' + t.keyed[0] + (t.calls.length > 1 ? ' <small>+' + (t.calls.length - 1) + '</small>' : '') + '</span></div>';
    }
    return '<div class="rct rct--quiet' + off + '"><span class="rct__r"><i class="rcg rcg--quiet"></i>' + t.calls[0] + (t.calls.length > 1 ? ' <small>+' + (t.calls.length - 1) + '</small>' : '') + '</span></div>';
  }

// ---------- The flag's level bar: VfoLevelBar.cpp, with the iPhone's source tag (FlagLevelBar.swift) ----------
  function paintLevel(c, w, h, scale, dbm, tag, o) {
    var FLOOR = -130; var CEIL = -20; var S9 = -73;
    c.setTransform(scale, 0, 0, scale, 0, 0);
    if (o.bgFill) { c.fillStyle = o.bgFill; c.fillRect(0, 0, w, h); } else { c.clearRect(0, 0, w, h); }
    var barW = w - o.dbmW - 4;
    var ticks = [-121, -109, -97, -85, -73, -53, -33];
    var labels = ['S1', '3', '5', '7', '9', '+20', '+40'];
    c.fillStyle = o.tick;
    c.font = o.tickFont;
    c.textBaseline = 'middle';
    for (var i = 0; i < ticks.length; i++) {
      var x = Math.floor((ticks[i] - FLOOR) / (CEIL - FLOOR) * (barW - 1));
      c.fillRect(x, o.top + o.tickH - 2, 1, 3);
      c.textAlign = i === 0 ? 'left' : i >= 5 ? 'right' : 'center';
      c.fillText(labels[i], x, o.top + (o.tickH - 2) / 2);
    }
    if (tag) {
      c.textAlign = 'left';
      c.font = o.tagFont || '700 10px system-ui, -apple-system, sans-serif';
      c.fillStyle = o.tagColor || '#dcdce1';
      c.fillText(tag, barW + 4, o.top + (o.tickH - 2) / 2);
    }
    var barTop = o.top + o.tickH;
    var barH = h - barTop;
    c.fillStyle = o.bg;
    c.fillRect(0, barTop, barW, barH);
    c.strokeStyle = o.border;
    c.lineWidth = 1;
    c.strokeRect(0.5, barTop + 0.5, barW - 1, barH - 1);
    var innerW = barW - 2;
    var fillW = Math.floor(clamp((dbm - FLOOR) / (CEIL - FLOOR), 0, 1) * innerW);
    if (fillW > 0) {
      var s9 = (S9 - FLOOR) / (CEIL - FLOOR);
      var g = c.createLinearGradient(1, 0, 1 + innerW, 0);
      g.addColorStop(0, '#00b4d8'); g.addColorStop(s9, '#00b4d8');
      g.addColorStop(Math.min(s9 + 0.15, 1), '#00d860'); g.addColorStop(1, '#00d860');
      c.fillStyle = g;
      c.fillRect(1, barTop + 1, fillW, barH - 2);
    }
    c.font = o.dbmFont;
    c.textAlign = 'left';
    c.fillStyle = dbm > S9 ? '#00d860' : '#00b4d8';
    c.fillText(dbm.toFixed(o.digits).replace('-0', '0') + ' dBm', barW + 4, barTop + barH / 2);
  }
  // The analog S-Meter's face, drawn live inside the capture's bezel. Its source prints as the app's does.
  function paintFace(c, needle, value, rx) {
    var W = 280; var H = 123;
    c.setTransform(2, 0, 0, 2, 0, 0);
    var g = c.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0, '#efe5cb'); g.addColorStop(1, '#e4d7b6');
    c.fillStyle = g;
    c.beginPath(); c.roundRect(0, 0, W, H, 7); c.fill();
    var cx = 140; var cy = 112; var R = 90;
    function ang(d) { // S0 -127 at 146 deg, S9 -73 at 80 deg, +60 -13 at 34 deg
      var a = d <= -73 ? 146 - (clamp(d, -127, -73) + 127) / 54 * 66 : 80 - (clamp(d, -73, -13) + 73) / 60 * 46;
      return a * Math.PI / 180;
    }
    c.lineCap = 'butt';
    c.strokeStyle = '#2a2620'; c.lineWidth = 1.4;
    c.beginPath(); c.arc(cx, cy, R, -ang(-127), -ang(-73), false); c.stroke();
    c.strokeStyle = '#b02a22'; c.lineWidth = 4;
    c.beginPath(); c.arc(cx, cy, R, -ang(-73), -ang(-13), false); c.stroke();
    var marks = [[-121, '1'], [-109, '3'], [-97, '5'], [-85, '7'], [-73, '9'], [-53, '+20'], [-33, '+40'], [-13, '+60']];
    c.textAlign = 'center'; c.textBaseline = 'middle';
    marks.forEach(function (m) {
      var a = ang(m[0]);
      var red = m[0] > -73;
      c.strokeStyle = red ? '#b02a22' : '#2a2620'; c.lineWidth = 1.4;
      c.beginPath(); c.moveTo(cx + Math.cos(a) * (R - 8), cy - Math.sin(a) * (R - 8)); c.lineTo(cx + Math.cos(a) * R, cy - Math.sin(a) * R); c.stroke();
      c.fillStyle = red ? '#b02a22' : '#2a2620';
      c.font = '600 11px Futura, "Avenir Next", system-ui, sans-serif';
      var rr = R - 19;
      c.fillText(m[1], cx + Math.cos(a) * rr + (red ? 9 : 0), cy - Math.sin(a) * rr);
    });
    c.fillStyle = '#2a2620';
    c.font = '600 14px Futura, "Avenir Next", system-ui, sans-serif';
    try { c.letterSpacing = '1px'; } catch (e0) {}
    c.fillText('SIGNAL STRENGTH', cx, 12);
    c.font = '500 10px Futura, "Avenir Next", system-ui, sans-serif';
    c.fillText('S UNITS', cx, 72);
    try { c.letterSpacing = '0px'; } catch (e1) {}
    if (rx !== 'Signal') { c.font = '700 10px system-ui, sans-serif'; c.fillStyle = '#5a4c2c'; c.fillText(rx, cx, 86); }
    var na = ang(needle);
    c.strokeStyle = '#1a1814'; c.lineWidth = 2;
    c.beginPath(); c.moveTo(cx, cy); c.lineTo(cx + Math.cos(na) * (R + 2), cy - Math.sin(na) * (R + 2)); c.stroke();
    c.fillStyle = '#1a1814'; c.beginPath(); c.arc(cx, cy, 8, 0, Math.PI * 2); c.fill();
    c.fillStyle = '#b08a3c'; c.beginPath(); c.arc(cx, cy, 4, 0, Math.PI * 2); c.fill();
    c.fillStyle = '#1a1814';
    c.font = '600 14px Futura, "Avenir Next", system-ui, sans-serif';
    c.textAlign = 'right'; c.fillText(sText(value), cx - 14, 111);
    c.font = '500 12px system-ui, sans-serif';
    c.textAlign = 'left'; c.fillText(String(Math.round(value)).replace('-', '−') + ' dBm', cx + 14, 111);
  }
  var LOCK_OPEN = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><rect x="4" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0"/></svg>';
  var LOCK_SHUT = '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" stroke="currentColor" stroke-width="2" stroke-linecap="round"><rect x="5" y="11" width="14" height="10" rx="2"/><path fill="none" d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>';
  var phoneRxHooks = [];
  function phoneRxChanged() { phoneRxHooks.forEach(function (f) { f(); }); }

  // ---------- The flag line ----------
  function flagLine(st, mine) {
    var m = st;
    var live = S.link === 'live';
    if (S.look === 'today') {
      var n = 5 + (m.talkers ? m.talkers.length : 0);
      if (m.kind === 'dbl') { return { g: 'dbl', t: 'Roundtable · ' + n + ' · Doubled' }; }
      if (m.kind === 'you') { return { g: 'you', t: 'Roundtable · ' + m.reports.length + ' hear you' }; }
      if (m.kind === 'unk') { return { g: 'unk', t: 'Roundtable · ' + n + ' · Unknown' }; }
      if (m.kind === 'talk') { return { g: 'talk', t: 'Roundtable · ' + n + ' · ' + m.talkers[0] }; }
      return { g: 'quiet', t: 'Roundtable · ' + n + ' · Quiet' };
    }
    if (S.link === 'recon') { return { g: 'wait', t: 'Roundtable · Reconnecting' }; }
    if (S.link === 'offline') { return { g: 'off', t: 'Roundtable · Offline' }; }
    var pre = S.link === 'hidden' ? 'Hidden · ' : 'Roundtable · ';
    if (m.kind === 'dbl') {
      return { g: 'dbl', dbl: true, html: '<span class="dbl">Double</span> · ' + m.talkers.slice(0, 2).map(function (t) { return t === YOU ? 'You' : t; }).join(' + ') + (m.talkers.length > 2 ? ' +' + (m.talkers.length - 2) : '') };
    }
    if (m.kind === 'you') { return { g: 'you', t: pre + m.reports.length + ' hear you' }; }
    if (m.kind === 'unk') { return { g: 'unk', t: pre + (m.reports.length + 1) + ' · Unknown' }; }
    if (m.kind === 'talk') { return { g: 'talk', t: pre + (m.reports.length + 1) + ' · ' + m.talkers[0] }; }
    return { g: 'quiet', t: pre + (m.people ? m.people.length : m.reports.length + 1) + ' · Quiet' };
  }
  function flagInner(st) {
    var l = flagLine(st);
    return '<i class="rcg rcg--' + l.g + '"></i>' + (l.html ? '<span class="rt-ft">' + l.html + '</span>' : '<span class="rt-ft">' + esc(l.t) + '</span>') + '<span class="rt-chev chev">›</span>';
  }

  // ---------- One band, one view ----------
  // list[0] is the scenario frequency; the rest stay as they are.
  function View(cfg) {
    this.cfg = cfg;
    this.freqs = cfg.freqs;
    this.activeIdx = 0;
  }
  View.prototype.entry = function (i) { return this.freqs[i]; };
  View.prototype.modelAt = function (i) {
    if (i === 0) { return applyLink(model(S.state, this.freqs[0].hz, { count: S.count })); }
    var e = this.freqs[i];
    var st = e.keyed.length > 1 ? 'double' : e.keyed.length === 1 ? 'talk' : 'quiet';
    var m = model(st, e.hz, { count: 3 });
    if (e.keyed.length) { m.talkers = e.keyed; m.reports = m.reports.filter(function (r) { return e.keyed.indexOf(r.c) < 0; }); }
    if (st === 'quiet') { m.people = e.calls; }
    return applyLink(m);
  };
  View.prototype.keyedAt = function (i) {
    if (i !== 0) { return this.freqs[i].keyed; }
    var m = model(S.state, this.freqs[0].hz);
    return m.live && m.kind !== 'quiet' ? (m.kind === 'unk' ? ['?'] : m.talkers) : [];
  };
  // Other frequencies' tags, including the scenario's when tuned away.
  View.prototype.tagAt = function (i) {
    var e = this.freqs[i];
    if (i !== 0) { return e; }
    var m = model(S.state, e.hz);
    var calls = m.people || m.talkers.concat(m.reports.map(function (r) { return r.c; }));
    return { hz: e.hz, keyed: this.keyedAt(0), calls: calls.length ? calls : ['Unknown'] };
  };

  // Lays out and draws the card and tags. `b` is the band's geometry.
  View.prototype.draw = function (b) {
    var self = this;
    var layer = b.layer;
    layer.innerHTML = '';
    if (S.link === 'off') { return; }
    var m = this.modelAt(this.activeIdx);
    var flagRects = b.flags();
    var foot = b.foot;
    var dialX = b.x(this.freqs[this.activeIdx].hz);
    var host = document.createElement('div');
    var placed = null;

    if (S.look === 'today') {
      placed = this.placeToday(m, b, dialX, flagRects);
      host = placed.el;
    } else if (m.kind === 'quiet' || (isOne() && m.kind === 'unk' && !m.reports.length)) {
      // Nothing to say: a compact tag at its own spot, not an empty card.
      host = null;
    } else {
      placed = this.placeCard(m, b, dialX, flagRects);
      host = placed.el;
    }

    var occupied = [];
    var insRect = isOne() && b.insRect ? b.insRect() : null;
    var hadCard = !!host;
    if (insRect) {
      // The inspector takes the card's place: one box, not two.
      if (host) { host.remove(); host = null; placed = null; }
      occupied.push(insRect);
      if (self.insMine) { layer.appendChild(tick(dialX, insRect.bottom, b.bandTop, borderOf(m))); }
    }
    if (host) {
      layer.appendChild(host);
      occupied.push(placed.rect);
      host.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      host.addEventListener('click', function (e) { e.stopPropagation(); b.openCard(self, m, placed.rect); });
      host.addEventListener('contextmenu', function (e) { e.preventDefault(); e.stopPropagation(); if (b.ctx) { b.ctx(self, self.activeIdx, e); } });
      layer.appendChild(tick(dialX, placed.rect.bottom, b.bandTop, borderOf(m)));
    }

    // Other frequencies along the foot, clear of the card and the flags.
    var rest = [];
    this.freqs.forEach(function (e, i) {
      if (i === self.activeIdx && hadCard) { return; }
      if (b.saidByFlag && b.saidByFlag(e.hz)) { return; }
      var x = b.x(e.hz);
      if (x < 2 || x > b.w - 2) { return; }
      rest.push({ i: i, x: x, t: self.tagAt(i) });
    });
    rest.sort(function (a, c) { return a.x - c.x; });
    if (b.foldGap && S.look !== 'today') { rest = foldNear(rest, b.foldGap, self); }
    rest.forEach(function (r) {
      var w0 = document.createElement('div');
      w0.innerHTML = r.fold ? r.html : tagHtml(r.t);
      var el = w0.firstChild;
      el.style.left = '-999px'; el.style.top = '0px';
      layer.appendChild(el);
      var w = el.offsetWidth; var h = el.offsetHeight;
      // The nearest free spot along the foot. A tag with nowhere clear to go is left off.
      var ideal = clamp(r.x - w / 2, b.left || 4, b.w - w - 4);
      var boxes = occupied.concat(flagRects);
      var blocked = function (x) {
        if (x < (b.left || 4) || x + w > (b.right || b.w - 4)) { return true; }
        return boxes.some(function (q) { return x < q.right + 4 && x + w > q.left - 4 && foot > q.top && foot - h < q.bottom; });
      };
      var spots = [ideal];
      boxes.forEach(function (q) { spots.push(q.right + 4, q.left - 4 - w); });
      var left = null;
      spots.forEach(function (x) { if (!blocked(x) && (left === null || Math.abs(x - ideal) < Math.abs(left - ideal))) { left = x; } });
      if (left === null) { el.remove(); return; }
      el.style.left = left + 'px';
      el.style.top = (foot - h) + 'px';
      occupied.push({ left: left, right: left + w, top: foot - h, bottom: foot });
      layer.appendChild(tick(r.x, foot, b.bandTop, getComputedStyle(el).borderTopColor));
      el.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      if (r.fold ? (isOne() && self.foldIdxs && self.foldIdxs.join() === r.fold.join()) : (self.peekIdx === r.i || (self.insMine && r.i === self.activeIdx))) { el.classList.add('is-peek'); }
      el.addEventListener('click', function (e) {
        e.stopPropagation();
        if (r.fold) { b.fold(self, r.fold, el); return; }
        if (S.look === 'today') { b.tune(self, r.i); } else { b.peek(self, r.i); }
      });
      el.addEventListener('contextmenu', function (e) { e.preventDefault(); e.stopPropagation(); if (b.ctx && !r.fold) { b.ctx(self, r.i, e); } });
      el.setAttribute('role', 'button');
      el.setAttribute('aria-label', r.fold ? r.fold.length + ' frequencies' : (S.look === 'today' ? 'Tune to ' : 'View ') + freqShort(self.freqs[r.i].hz));
    });
    // A quiet active frequency gets the compact tag at its own spot; the loop above drew it.
    this.liveHost = host;
    this.liveModel = m;
    this.liveRows = placed ? placed.rows : 0;
  };
  // Tags too close to read fold into one +N (RoundtableTagLayout).
  function foldNear(rest, gap, view) {
    var out = []; var run = [];
    function flush() {
      if (run.length < 2) { out = out.concat(run); run = []; return; }
      var idx = run.map(function (r) { return r.i; });
      var keyed = run.reduce(function (n, r) { return Math.max(n, r.t.keyed.length); }, 0);
      var k = keyed > 1 ? 'dbl' : keyed === 1 ? 'talk' : 'quiet';
      out.push({ fold: idx, x: run.reduce(function (n, r) { return n + r.x; }, 0) / run.length, t: { keyed: [], calls: [] },
        html: '<div class="rct rct--' + k + ' rct--fold"><span class="rct__r"><i class="rcg rcg--' + k + '"></i>+' + run.length + '</span></div>' });
      run = [];
    }
    rest.forEach(function (r) {
      if (r.i === view.activeIdx) { flush(); out.push(r); return; }
      if (run.length && r.x - run[run.length - 1].x > gap) { flush(); }
      run.push(r);
    });
    flush();
    return out;
  }
  function tick(x, top, bottom, color) {
    var t = document.createElement('div');
    t.className = 'rc-tick';
    t.style.left = Math.round(x) + 'px';
    t.style.top = top + 'px';
    t.style.height = Math.max(0, bottom - top) + 'px';
    t.style.setProperty('--rc-c', color);
    return t;
  }
  function freeStretches(b, flags, top, foot) {
    var blocks = flags.filter(function (q) { return q.bottom > top && q.top < foot; }).sort(function (a, c) { return a.left - c.left; });
    var left = b.left || 4;
    var out = [];
    blocks.forEach(function (q) {
      if (q.left - 4 - left > 0) { out.push({ left: left, right: q.left - 4 }); }
      left = Math.max(left, q.right + 4);
    });
    if ((b.right || b.w - 4) - left > 0) { out.push({ left: left, right: b.right || b.w - 4 }); }
    return out;
  }
  // Two candidate places: under the flag at full width, or beside it, as tall as the pan allows.
  View.prototype.placeCard = function (m, b, dialX, flags) {
    var foot = b.foot;
    var full = b.cardW;
    var measure = function (w, maxRows) {
      var el = document.createElement('div');
      mountCard(el, m, { w: w, maxRows: maxRows });
      el.style.position = 'absolute'; el.style.left = '-2000px'; el.style.top = '0'; el.style.visibility = 'hidden';
      b.layer.appendChild(el);
      var h = el.offsetHeight;
      el.remove();
      return h;
    };
    var fit = function (w, room) {
      for (var n = m.reports.length; n >= 0; n--) {
        if (measure(w, n) <= room) { return n; }
      }
      return -1;
    };
    var flagBottom = 0;
    flags.forEach(function (q) { flagBottom = Math.max(flagBottom, q.bottom); });
    var underRoom = foot - flagBottom - 4;
    var underRows = fit(full, underRoom);
    var bestStretch = null;
    // The phone's side buttons stand above the card, as on the real iPhone, so the strip beside the flag stays usable.
    var stretchTop = b.cardTop ? b.cardTop() : (b.top || 2);
    var stretches = freeStretches(b, flags, stretchTop, foot);
    var considerStretch = function (s, farOnly) {
      var wid = s.right - s.left;
      if (wid < b.minW) { return; }
      if (farOnly && !(b.flipped ? s.right <= dialX : s.left >= dialX)) { return; }
      var d = dialX < s.left ? s.left - dialX : dialX > s.right ? dialX - s.right : 0;
      if (!bestStretch || d < bestStretch.d || (d === bestStretch.d && wid > bestStretch.wid)) { bestStretch = { s: s, d: d, wid: wid }; }
    };
    if (b.sided) { stretches.forEach(function (s) { considerStretch(s, true); }); }
    if (!bestStretch) { stretches.forEach(function (s) { considerStretch(s, false); }); }
    var w = full; var rows = underRows; var left; var useUnder = underRows >= 0;
    if (bestStretch) {
      var bw = Math.min(full, bestStretch.wid);
      var bRows = fit(bw, foot - stretchTop);
      if (!useUnder || bRows > underRows) {
        w = bw; rows = Math.max(0, bRows); useUnder = false;
        left = clamp(dialX - w / 2, bestStretch.s.left, bestStretch.s.right - w);
      }
    }
    if (useUnder) { left = clamp(b.underFlag ? dialX + 4 : dialX - w / 2, b.left || 4, (b.right || b.w - 4) - w); }
    var el = document.createElement('div');
    mountCard(el, m, { w: w, maxRows: rows });
    el.style.position = 'absolute'; el.style.left = left + 'px';
    el.style.zIndex = 4;
    b.layer.appendChild(el);
    var h = el.offsetHeight;
    el.style.top = (foot - h) + 'px';
    el.setAttribute('role', 'button');
    el.setAttribute('aria-label', 'Roundtable live card');
    return { el: el, rows: rows, rect: { left: left, right: left + w, top: foot - h, bottom: foot } };
  };
  View.prototype.placeToday = function (m, b, dialX, flags) {
    var wrap = document.createElement('div');
    var html;
    if (b.isPhone) {
      var freeW = 176;
      var flagBottom = 0;
      flags.forEach(function (q) { flagBottom = Math.max(flagBottom, q.bottom); });
      var stretches = freeStretches(b, flags, b.top || 2, b.foot);
      var best = stretches.sort(function (a, c) { return (c.right - c.left) - (a.right - a.left); })[0];
      if (best) { freeW = Math.min(176, best.right - best.left); }
      html = todayPhone(m, freeW);
    } else {
      html = todayDesktop(m);
    }
    wrap.innerHTML = html;
    var el = wrap.firstChild;
    el.style.position = 'absolute'; el.style.visibility = 'hidden'; el.style.zIndex = 4;
    b.layer.appendChild(el);
    // Today's phone plan: rows go first when the pan is short.
    var room = b.foot - (b.top || 2);
    var extra = 0;
    while (el.offsetHeight > room && el.querySelector('.r')) {
      var rs = el.querySelectorAll('.r');
      rs[rs.length - 1].remove();
      extra++;
    }
    if (extra && el.querySelector('.d')) { el.querySelector('.d').textContent = '+' + extra + ' reports \u203a'; }
    var w = el.offsetWidth; var h = el.offsetHeight;
    var top = b.foot - h;
    var st = freeStretches(b, flags, top, b.foot);
    var pick = null;
    st.forEach(function (s) {
      if (s.right - s.left < w) { return; }
      var d = dialX < s.left ? s.left - dialX : dialX > s.right ? dialX - s.right : 0;
      if (!pick || d < pick.d) { pick = { s: s, d: d }; }
    });
    var left = pick ? clamp(dialX - w / 2, pick.s.left, pick.s.right - w) : 4;
    el.style.left = left + 'px'; el.style.top = top + 'px'; el.style.visibility = 'visible';
    return { el: el, rows: 0, rect: { left: left, right: left + w, top: top, bottom: b.foot } };
  };
  // Move only what changes: values, bars, ages. The card is not rebuilt.
  View.prototype.refreshLive = function () {
    if (!this.liveHost || S.look === 'today') { return; }
    refreshCard(this.liveHost, this.modelAt(this.activeIdx));
  };
  function refreshCard(host, m) {
    if (m.frozen) {
      // Nothing new is arriving, so only the ages move.
      var gone = (Date.now() - S.t0) / 1000;
      host.querySelectorAll('.rr').forEach(function (row) {
        row.querySelector('[data-a]').textContent = age(parseFloat(row.getAttribute('data-a0')) + gone);
      });
      return;
    }
    if (!m.reports) { return; }
    m.reports.forEach(function (r) {
      var row = host.querySelector('.rr[data-call="' + r.c + '"]');
      if (!row) { return; }
      var v = row.querySelector('[data-v]');
      v.textContent = signal(r.d, v.getAttribute('data-fmt'));
      row.querySelector('[data-a]').textContent = age(r.a);
      row.classList.toggle('is-old', !!r.old);
      var i = row.querySelector('.rr__m i');
      if (i) { i.style.width = (frac(r.d) * 100).toFixed(1) + '%'; }
      var u = row.querySelector('.rr__m u');
      if (u) { u.style.left = (frac(r.pk) * 100).toFixed(1) + '%'; }
    });
    var c = host.querySelector('[data-clock]');
    if (c) { c.textContent = mmss(m.clock); }
    var n = host.querySelector('[data-note]');
    if (n && m.note) { n.textContent = m.note; }
  }

  // ---------- Detail body, used by the sheet and the applet ----------
  function detailHtml(m) {
    var title;
    if (m.kind === 'dbl') { title = '<i class="rcg rcg--dbl"></i><span style="color:var(--rc-dbl-text)">Double</span> · ' + m.talkers.map(function (t) { return t === YOU ? 'You' : t; }).join(' + '); }
    else if (m.kind === 'unk') { title = '<i class="rcg rcg--unk"></i>Unknown signal'; }
    else if (m.kind === 'you') { title = '<i class="rcg rcg--you"></i>You, on the air'; }
    else if (m.kind === 'last') { title = '<i class="rcg rcg--quiet"></i>' + (m.wasDouble ? 'Last double · ' + m.talkers.join(' + ') : 'Last talker · ' + m.talkers[0]); }
    else if (m.kind === 'quiet') { title = '<i class="rcg rcg--quiet"></i>Quiet'; }
    else { title = '<i class="rcg rcg--talk"></i>' + m.talkers[0] + ' talking'; }
    var h = '<h4>' + title + '</h4>';
    if (m.reports && m.reports.length) {
      var combined = m.kind === 'dbl' || m.wasDouble;
      h += '<table><tr><th>Heard by</th>' + (combined ? '' : '<th class="r">mi / °T</th>') + '<th class="r">' + (S.fmt === 'S' ? 'S' : S.fmt === 'both' ? 'S / dBm' : 'dBm') + '</th><th class="r">Age</th></tr>';
      m.reports.forEach(function (r) {
        h += '<tr><td class="c' + (r.you ? ' you' : '') + '">' + (r.you ? 'You' : r.c) + '</td>' + (combined ? '' : '<td class="r">' + r.mi.toLocaleString() + ' / ' + String(r.br).padStart(3, '0') + '°</td>') + '<td class="r">' + signal(r.d, S.fmt) + '</td><td class="r">' + age(r.a) + '</td></tr>';
      });
      h += '</table>';
      h += '<div class="fn">' + (combined ? 'Combined RF is not credited to either station and stays out of both turns.' : 'Each row is that station’s own reading. Nearest first.') + '</div>';
    } else if (m.people) {
      h += '<div class="fn">Here now: ' + m.people.map(function (p) { return p === YOU ? 'You' : p; }).join(', ') + '</div>';
    }
    h += '<div class="msg"><small>Nearby chat · within 5 kHz</small><div><b>N4RDR</b> Good copy into Knoxville.</div><div><b>KD8LMN</b> Solid here, thanks.</div></div>';
    return h;
  }
  // ---------- Viewing a frequency without tuning there ----------
  var OFFSETS = [0, 0, 0, 20, -40, 60];
  function hashCall(c) { var h = 7; for (var i = 0; i < c.length; i++) { h = (h * 31 + c.charCodeAt(i)) % 9973; } return h; }
  function peekData(view, i, sliceHz, panDb) {
    var t = view.tagAt(i);
    var e = view.freqs[i];
    var unknown = t.keyed[0] === '?';
    var keyed = unknown ? [] : t.keyed;
    var kind = unknown ? 'unk' : keyed.length > 1 ? 'dbl' : keyed.length === 1 ? 'talk' : 'quiet';
    var now = Date.now() / 1000;
    var calls = t.calls.filter(function (c) { return c !== 'Unknown'; });
    var d = {
      kind: kind, f: e.hz, keyed: keyed, sliceHz: sliceHz, live: kind !== 'quiet', showPath: kind !== 'dbl' && kind !== 'unk',
      frozen: S.link === 'recon' || S.link === 'offline',
      clock: 20 + hashCall(calls.join('') || 'x') % 90 + (now - S.t0 / 1000),
      people: calls.map(function (c) { return { c: c, keyed: keyed.indexOf(c) >= 0, off: OFFSETS[hashCall(c) % 6] }; })
    };
    d.reports = calls.filter(function (c) { return keyed.indexOf(c) < 0 && c !== YOU; }).map(function (c) {
      var h = hashCall(c);
      var w = kind === 'quiet' ? 0 : 3 * Math.sin(now * 0.7 + h);
      var age0 = kind === 'quiet' ? 190 + h % 60 + (now - S.t0 / 1000) : (now + h) % 4;
      return { c: c, d: -68 - h % 38 + w, pk: -68 - h % 38 + 3, a: age0, mi: 150 + (h * 7) % 2400, br: (h * 13) % 360, old: age0 > 20 };
    }).sort(function (a, b) { return a.mi - b.mi; });
    // Your own pan already shows this frequency, so it can say what you see there.
    if (kind !== 'quiet' && panDb != null) {
      d.reports.unshift({ c: 'pan', label: 'Your pan', you: true, d: panDb + 1.5 * Math.sin(now * 1.3), pk: panDb + 2, a: 0 });
    }
    d.ago = d.reports.length ? d.reports[d.reports.length - 1].a : 0;
    return d;
  }
  // Your own frequency, in the same shape as a viewed one.
  function mineData(m) {
    var keyed = m.live && m.kind !== 'quiet' && m.kind !== 'unk' ? m.talkers : [];
    var calls = m.people || m.talkers.concat((m.reports || []).map(function (r) { return r.c; }));
    return {
      mine: true, kind: m.kind, f: m.f, keyed: keyed, sliceHz: m.f, live: m.live && m.kind !== 'quiet',
      showPath: m.kind !== 'dbl' && m.kind !== 'unk' && !m.wasDouble, frozen: m.frozen, wasDouble: m.wasDouble,
      talkers: m.talkers, note: m.note, ago: m.ago || 0,
      clock: m.clock || 78 + (Date.now() - S.t0) / 1000,
      people: calls.map(function (c) { return { c: c, keyed: keyed.indexOf(c) >= 0, off: OFFSETS[hashCall(c) % 6] }; }),
      reports: m.reports || []
    };
  }
  function peekBody(d, keyedNow) {
    var who = d.kind === 'dbl' ? 'Double' : d.kind === 'talk' ? d.keyed[0] : d.kind === 'unk' ? 'Unknown signal' : d.kind === 'you' ? 'You \u00b7 on the air' : d.kind === 'last' ? (d.wasDouble ? 'Last double' : d.talkers[0]) : 'Nobody talking';
    var g = d.kind === 'dbl' ? 'dbl' : d.kind === 'talk' ? 'talk' : d.kind === 'unk' ? 'unk' : d.kind === 'you' ? 'you' : 'quiet';
    var away = (d.f - d.sliceHz) / 1000;
    var h = '<div class="rc__head"><i class="rcg rcg--' + g + '"></i><b class="rc__who">' + esc(who) + '</b>' +
      (d.live ? '<span class="rc__clock" data-clock>' + mmss(d.clock) + '</span>' : '') +
      '<span class="rc__f">' + freqShort(d.f) + '</span></div>';
    h += d.mine ? (d.note ? '<div class="pk-sub" data-note>' + esc(d.note) + '</div>' : '') : '<div class="pk-sub">Viewing, not tuned' + (away ? ' \u00b7 ' + (away > 0 ? '+' : '\u2212') + Math.abs(away).toFixed(1) + ' kHz from slice A' : '') + ' \u00b7 LSB</div>';
    h += peekMiddle(d);
    if (d.mine) { return h; }
    h += '<div class="pk-acts"><button type="button" class="pk-go" data-a="tune"' + (keyedNow || d.f === d.sliceHz ? ' disabled' : '') + '>Tune slice A here</button>' +
      '<button type="button" data-a="open">Messages</button></div>';
    return h;
  }
  function peekMiddle(d) {
    var h = '';
    if (S.link === 'recon') { h += '<div class="rc__note" style="color:var(--rc-unk)">Reconnecting \u00b7 last known</div>'; }
    if (S.link === 'offline') { h += '<div class="rc__note">Offline \u00b7 last known</div>'; }
    h += '<div class="rc__cap"><span>On frequency \u00b7 ' + d.people.length + '</span><span>Tuned</span></div><div class="pk-people">';
    var shownPeople = d.compactPeople ? d.people.filter(function (p) { return p.keyed; }) : d.people;
    var alsoHere = d.compactPeople ? d.people.filter(function (p) { return !p.keyed; }) : [];
    shownPeople.forEach(function (p) {
      h += '<div class="pk-p' + (p.keyed ? ' is-keyed' : '') + '"><i class="rcg rcg--' + (p.keyed ? (d.kind === 'dbl' ? 'dbl' : 'talk') : 'quiet') + '"></i>' +
        '<span class="pk-c">' + esc(p.c === YOU ? 'You' : p.c) + '</span><span class="pk-k">' + (p.keyed ? 'keyed' : '') + '</span>' +
        '<span class="pk-o">' + (p.off ? (p.off > 0 ? '+' : '\u2212') + Math.abs(p.off) + ' Hz' : 'on freq') + '</span></div>';
    });
    // A short pan keeps the readings in view: everyone not keyed folds to one line.
    if (alsoHere.length) { h += '<div class="pk-also">Also here \u00b7 ' + alsoHere.map(function (p) { return esc(p.c === YOU ? 'You' : p.c); }).join(', ') + '</div>'; }
    if (!d.people.length) { h += '<div class="rc__note">Nobody here</div>'; }
    h += '</div>';
    var cap = d.kind === 'dbl' || d.wasDouble ? '<b>Combined RF</b>' : d.kind === 'you' ? 'Hearing you' : d.kind === 'quiet' || d.kind === 'last' ? 'Last heard ' + age(d.ago) + ' ago' : 'Heard by';
    h += '<div class="rc__cap"><span>' + cap + '</span><span>' + (S.fmt === 'S' ? 'S' : S.fmt === 'both' ? 'S / dBm' : 'dBm') + '</span></div>';
    h += d.reports.length ? '<div class="rc__rows">' + d.reports.map(function (r) { return rowHtml(r, false, d); }).join('') + '</div>' : '<div class="rc__note">No readings</div>';
    return h;
  }
  // One inspector: the same box for every frequency, yours or not.
  function inspectorBody(d, keyedNow) {
    var who = d.kind === 'dbl' ? 'Double' : d.kind === 'talk' ? d.keyed[0] : d.kind === 'unk' ? 'Unknown signal' : d.kind === 'you' ? 'You \u00b7 on the air' : d.kind === 'last' ? (d.wasDouble ? 'Last double' : d.talkers[0]) : 'Nobody talking';
    var g = d.kind === 'dbl' ? 'dbl' : d.kind === 'talk' ? 'talk' : d.kind === 'unk' ? 'unk' : d.kind === 'you' ? 'you' : 'quiet';
    var away = (d.f - d.sliceHz) / 1000;
    var h = '<div class="rc__head ins-head"><i class="rcg rcg--' + g + '"></i><b class="rc__who">' + esc(who) + '</b>' +
      (d.live ? '<span class="rc__clock" data-clock>' + mmss(d.clock) + '</span>' : '') +
      '<span class="rc__f">' + freqShort(d.f) + '</span>' + insButtons() + '</div>';
    if (!d.mine) {
      h += '<div class="ins-view"><span>Viewing' + (away ? ' \u00b7 ' + (away > 0 ? '+' : '\u2212') + Math.abs(away).toFixed(1) + ' kHz' : '') + '</span><button type="button" data-a="back">\u2039 Back to my slice</button></div>';
    } else if (d.note) { h += '<div class="pk-sub" data-note>' + esc(d.note) + '</div>'; }
    h += '<div class="ins-scroll">' + peekMiddle(d) + '</div>';
    h += '<div class="pk-acts"><button type="button" class="pk-go" data-a="tune"' + (d.mine ? ' disabled title="Slice A is already here"' : keyedNow ? ' disabled title="Not while you transmit"' : '') + '>Tune slice A here</button>' +
      '<button type="button" data-a="open">Messages</button></div>';
    return h;
  }
  function insButtons() {
    return '<button type="button" class="ins-b" data-a="more" aria-label="More">\u22ef</button><button type="button" class="ins-b" data-a="x" aria-label="Close">\u00d7</button>';
  }
  function peekClass(d) {
    return 'rc rc-peek rc--' + (d.kind === 'quiet' ? 'quiet' : d.kind) + (S.link === 'offline' ? ' is-off is-frozen' : S.link === 'recon' ? ' is-frozen' : '');
  }
  // Read only: only stations tuned to a frequency post there.
  function viewingChat() {
    return '<div class="msg"><small>Messages \u00b7 read only until you tune here</small><div><b>W2ABC</b> Good signal into Albany.</div><div><b>N7ABC</b> Fading here, still solid copy.</div></div>';
  }
  function viewingSend() {
    return '<div class="rc-send"><input aria-label="Message" placeholder="Tune here to post" disabled><button type="button" disabled>Send</button></div>';
  }

  function canSend() { return S.link === 'live'; }
  function sendRow(id) {
    return '<div class="rc-send"><input aria-label="Message" placeholder="' + (canSend() ? 'Message nearby stations' : 'Can’t send right now') + '"' + (canSend() ? '' : ' disabled') + '><button type="button"' + (canSend() ? '' : ' disabled') + '>Send</button></div>';
  }
  function wireSend(root) {
    var btn = root.querySelector('.rc-send button');
    var inp = root.querySelector('.rc-send input');
    if (!btn) { return; }
    var go = function () {
      if (!inp.value.trim() || !canSend()) { return; }
      var msg = root.querySelector('.msg');
      var d = document.createElement('div');
      d.innerHTML = '<b style="color:var(--rc-you)">You</b> ' + esc(inp.value);
      msg.appendChild(d);
      inp.value = '';
    };
    btn.addEventListener('click', go);
    inp.addEventListener('keydown', function (e) { e.stopPropagation(); if (e.key === 'Enter') { go(); } });
  }

  // ---------- Controls ----------
  var views = [];
  var redrawers = [];
  function redrawAll() {
    S.t0 = Date.now();
    redrawers.forEach(function (fn) { fn(); });
    pressAll();
    status();
  }
  function chip(host, label, on, click, cls) {
    var b = document.createElement('button');
    b.type = 'button';
    b.className = 'rcb-chip' + (cls ? ' ' + cls : '');
    b.textContent = label;
    b.addEventListener('click', click);
    b.setAttribute('data-on', on);
    host.appendChild(b);
    return b;
  }
  var bar = {};
  function buildBar() {
    var st = document.getElementById('rcb-states');
    bar.play = chip(st, '▶ Play a QSO', 'play', function () { togglePlay(); }, 'rcb-chip--play');
    STATES.forEach(function (x) {
      chip(st, x.chip, 'state:' + x.id, function () { stopPlay(); S.state = x.id; views.forEach(function (v) { v.backToScenario && v.backToScenario(); }); redrawAll(); });
    });
    var looks = document.getElementById('rcb-looks');
    [['one', 'This proposal'], ['peek', 'On your Mac now'], ['today', 'Before the overlay']].forEach(function (x) {
      chip(looks, x[1], 'look:' + x[0], function () { S.look = x[0] === 'today' ? 'today' : 'new'; S.ux = x[0] === 'one' ? 'one' : 'peek'; redrawAll(); });
    });
    var fm = document.getElementById('rcb-formats');
    [['S', 'S-units'], ['dbm', 'dBm'], ['both', 'Both']].forEach(function (x) {
      chip(fm, x[1], 'fmt:' + x[0], function () { S.fmt = x[0]; redrawAll(); });
    });
    var cn = document.getElementById('rcb-counts');
    [0, 2, 5, 7].forEach(function (n) { chip(cn, String(n), 'count:' + n, function () { S.count = n; redrawAll(); }); });
    var ln = document.getElementById('rcb-links');
    [['live', 'Live'], ['recon', 'Reconnecting'], ['offline', 'Offline'], ['hidden', 'Hidden']].forEach(function (x) {
      chip(ln, x[1], 'link:' + x[0], function () { S.link = x[0]; redrawAll(); });
    });
    var pn = document.getElementById('rcb-pans');
    [['tall', 'Tall'], ['short', 'Short, like yours']].forEach(function (x) {
      chip(pn, x[1], 'pan:' + x[0], function () { S.pan = x[0]; redrawAll(); });
    });
  }
  function pressAll() {
    document.querySelectorAll('#rcb-bar .rcb-chip').forEach(function (b) {
      var a = b.getAttribute('data-on').split(':');
      var v = a[0] === 'state' ? S.state : a[0] === 'look' ? (S.look === 'today' ? 'today' : S.ux) : a[0] === 'pan' ? S.pan : a[0] === 'fmt' ? S.fmt : a[0] === 'count' ? String(S.count) : a[0] === 'link' ? S.link : null;
      if (a[0] !== 'play') { b.setAttribute('aria-pressed', v === a[1] ? 'true' : 'false'); }
    });
  }
  function status() {
    var x = byId[S.state];
    var el = document.getElementById('rcb-status');
    var pre = S.look === 'today' ? '<b>Before the overlay.</b> ' : S.ux === 'peek' ? '<b>On your Mac now.</b> ' : '';
    el.innerHTML = pre + '<b>' + esc(x.t) + '.</b> ' + esc(x.s);
  }

  // Play through one conversation.
  var PLAY = [['talk', 6000], ['double', 6000], ['after', 7000], ['ended', 4000], ['you', 5000], ['youdbl', 5000], ['ended', 4000], ['unknown', 5000], ['quiet', 3000]];
  var playTimer = null;
  var playIdx = 0;
  function step() {
    if (!S.playing) { return; }
    var s = PLAY[playIdx % PLAY.length];
    S.state = s[0];
    redrawAll();
    playIdx++;
    playTimer = setTimeout(step, s[1]);
  }
  function togglePlay() {
    if (S.playing) { stopPlay(); return; }
    S.playing = true;
    bar.play.textContent = '■ Stop';
    views.forEach(function (v) { v.backToScenario && v.backToScenario(); });
    playIdx = 0;
    step();
  }
  function stopPlay() {
    S.playing = false;
    clearTimeout(playTimer);
    if (bar.play) { bar.play.textContent = '▶ Play a QSO'; }
  }

  // ---------- Live wander ----------
  function wander() {
    LISTENERS.forEach(function (l) {
      var o = offsets[l.c];
      o.v = clamp(o.v + (Math.random() - 0.5) * 3.2, -6, 6);
      o.pk = Math.max(o.v, o.pk - 0.5);
      o.a += 0.25;
      if (o.a > 3.6 + Math.random() * 2) { o.a = 0; }
    });
  }

  // ---------- A voice on the air, for the live pans ----------
  function voiceLevel(t, seed) {
    var a = Math.sin(t * 3.1 + seed) * 0.5 + Math.sin(t * 7.3 + seed * 2) * 0.3 + Math.sin(t * 1.3 + seed * 3) * 0.5;
    var phrase = Math.sin(t * 0.55 + seed * 0.7);
    return a < -0.45 || phrase < -0.7 ? -40 : -3 + a * 6;
  }

  // ---------- Desktop ----------
  var CAP = window.RC_DESKTOP_CAPTURE;
  function buildDesktop() {
    var dk = document.getElementById('dk');
    dk.style.backgroundImage = 'url(' + CAP + ')';
    var geo = { x0: 0, x1: 1143, specTop: 30, bandTop: 311, wfTop: 359, wfBot: 803, dialX: 570.6, dialHz: 7185000, hzPerPx: 1000 / 5.96 };
    var hzAt = function (x) { return geo.dialHz + (x - geo.dialX) * geo.hzPerPx; };
    var xAt = function (hz) { return geo.dialX + (hz - geo.dialHz) / geo.hzPerPx; };

    var spec = document.createElement('canvas');
    spec.width = geo.x1 * 2; spec.height = (geo.bandTop - geo.specTop) * 2;
    spec.style.cssText = 'left:0;top:' + geo.specTop + 'px;width:' + geo.x1 + 'px;height:' + (geo.bandTop - geo.specTop) + 'px;cursor:crosshair';
    dk.appendChild(spec);
    var wf = document.createElement('canvas');
    wf.width = geo.x1; wf.height = geo.wfBot - geo.wfTop;
    wf.style.cssText = 'left:0;top:' + geo.wfTop + 'px;width:' + geo.x1 + 'px;height:' + (geo.wfBot - geo.wfTop) + 'px;cursor:crosshair';
    dk.appendChild(wf);
    var wfc = wf.getContext('2d', { alpha: false });
    var spc = spec.getContext('2d');

    // Parts of the capture that sit over the band.
    function crop(x, y, w, h, z) {
      var c = document.createElement('div');
      c.className = 'dk__crop';
      c.style.cssText += 'left:' + x + 'px;top:' + y + 'px;width:' + w + 'px;height:' + h + 'px;z-index:' + (z || 6) + ';background-image:url(' + CAP + ');background-position:' + (-x) + 'px ' + (-y) + 'px';
      dk.appendChild(c);
      return c;
    }
    crop(4, 36, 72, 170);           // the pan's toolbar
    crop(1026, 39, 108, 23);        // CH 0 / TX
    crop(4, 744, 106, 22);          // S B - +
    var flagW = 250;
    var flag = crop(572, 31, flagW, 134, 8);
    var lock = crop(549, 33, 18, 20, 8);
    var marker = document.createElement('div');
    marker.style.cssText = 'position:absolute;top:150px;width:0;height:0;z-index:9;pointer-events:none;border-left:5px solid transparent;border-right:5px solid transparent;border-top:9px solid #1fc4e0';
    dk.appendChild(marker);
    var fline = document.createElement('div');
    fline.className = 'dk__fline rc-fline';
    dk.appendChild(fline);
    var freqEl = document.createElement('div');
    freqEl.className = 'dk__freq';
    dk.appendChild(freqEl);
    var txEl = document.createElement('div');
    txEl.className = 'dk__tx';
    txEl.textContent = 'TX';
    txEl.style.cssText += 'left:1071px;top:41px;width:62px;height:19px';
    dk.appendChild(txEl);

    var layer = document.createElement('div');
    layer.className = 'dk__overlay';
    dk.appendChild(layer);

    var menu = document.createElement('div');
    menu.className = 'rc-menu';
    menu.style.display = 'none';
    dk.appendChild(menu);
    var applet = document.createElement('div');
    applet.className = 'rc-applet';
    dk.appendChild(applet);

    var view = new View({
      freqs: [
        { hz: 7185000 },
        { hz: 7172000, keyed: ['AA5ABC', 'K0ABC'], calls: ['AA5ABC', 'K0ABC', 'W2ABC', 'N7ABC'] },
        { hz: 7159000, keyed: ['W6ABC'], calls: ['W6ABC', 'KC2ABC', 'N3ABC'] },
        { hz: 7196000, keyed: [], calls: ['G0ABC', 'N5ABC'] },
        { hz: 7141000, keyed: ['N2DEF'], calls: ['N2DEF', 'W3DEF'] },
        { hz: 7143000, keyed: [], calls: ['W8DEF', 'K2DEF'] },
        { hz: 7144500, keyed: ['W4DEF', 'K9DEF'], calls: ['W4DEF', 'K9DEF', 'N1DEF'] }
      ]
    });
    views.push(view);
    var slice = { hz: 7185000 };
    view.backToScenario = function () { view.activeIdx = 0; slice.hz = view.freqs[0].hz; };
    var keyedYou = function () { return view.activeIdx === 0 && (S.state === 'you' || S.state === 'youdbl'); };

    var band = {
      layer: layer, w: geo.x1, h: geo.bandTop - 0, top: geo.specTop + 2, foot: geo.bandTop - 8, bandTop: geo.bandTop,
      left: 80, right: geo.x1 - 34,
      cardW: 232, minW: 200, sided: true, x: xAt,
      flipped: false, foldGap: 44,
      insRect: function () { return insRectNow; },
      ctx: function (v, i, e) { openQuick(i, e); },
      fold: function (v, idx, el) { openFold(idx, el); },
      flags: function () {
        var x = xAt(slice.hz);
        return band.flipped ? [{ left: x - flagW - 2, right: x + 24, top: 30, bottom: 192 }]
          : [{ left: x - 24, right: x + flagW + 2, top: 30, bottom: 192 }];
      },
      peek: function (v, i) {
        closeUi();
        if (isOne()) { toggleIns(i); return; }
        v.peekIdx = i; draw(); renderPeek();
      },
      tune: function (v, i) { if (keyedYou()) { return; } v.peekIdx = null; v.insMine = false; v.foldIdxs = null; peekEl.style.display = 'none'; v.activeIdx = i; slice.hz = v.freqs[i].hz; closeUi(); draw(); },
      openCard: function (v, m, rect) {
        if (isOne()) { closeUi(); toggleIns(v.activeIdx); return; }
        openMenu(m, rect);
      }
    };
    var peekEl = document.createElement('div');
    peekEl.style.cssText = 'position:absolute;z-index:12;width:300px;display:none';
    dk.appendChild(peekEl);
    peekEl.addEventListener('click', function (e) { e.stopPropagation(); });
    var insRectNow = null;
    var lastUx = S.ux + S.look;
    var PAN_DB = { 1: -74, 2: -80, 3: -100, 4: -86, 5: -100, 6: -82 };
    function inspecting() { return !!(view.insMine || view.peekIdx != null || view.foldIdxs); }
    function clearIns() { view.insMine = false; view.peekIdx = null; view.foldIdxs = null; insRectNow = null; peekEl.style.display = 'none'; }
    function toggleIns(i) {
      var mine = i === view.activeIdx;
      var same = mine ? view.insMine : view.peekIdx === i;
      clearIns();
      if (!same) { if (mine) { view.insMine = true; } else { view.peekIdx = i; } }
      draw();
    }
    function openFold(idx, el) {
      closeUi();
      if (isOne()) {
        var listShown = view.foldIdxs && view.peekIdx == null && view.foldIdxs.join() === idx.join();
        clearIns();
        if (!listShown) { view.foldIdxs = idx; }
        draw();
        return;
      }
      // Today: a small menu of the folded frequencies.
      menu.innerHTML = '<div class="rc-menu__h">' + idx.length + ' frequencies</div>' + idx.map(function (i) {
        return '<button type="button" data-i="' + i + '">' + freqShort(view.freqs[i].hz) + ' \u00b7 ' + esc(whoAt(i)) + '</button>';
      }).join('');
      menu.style.left = clamp(parseFloat(el.style.left), 4, 1140 - 190) + 'px';
      menu.style.top = Math.max(34, parseFloat(el.style.top) - 110) + 'px';
      menu.style.display = 'block';
      menu.querySelectorAll('[data-i]').forEach(function (b) {
        b.onclick = function (e) { e.stopPropagation(); closeUi(); view.peekIdx = parseInt(b.getAttribute('data-i'), 10); draw(); renderPeek(); };
      });
    }
    function whoAt(i) {
      var t = view.tagAt(i);
      return t.keyed.length > 1 ? 'Double' : t.keyed[0] === '?' ? 'Unknown' : t.keyed[0] || t.calls[0];
    }
    // Where the inspector sits: your card's spot, past the flag from the dial.
    function placeIns(wanted) {
      var dialX = xAt(slice.hz);
      var flags = band.flags();
      var st = freeStretches(band, flags, band.top, band.foot);
      var best = null;
      function consider(s, far) {
        var wid = s.right - s.left;
        if (wid < 260) { return; }
        if (far && !(band.flipped ? s.right <= dialX : s.left >= dialX)) { return; }
        var d = dialX < s.left ? s.left - dialX : dialX > s.right ? dialX - s.right : 0;
        if (!best || d < best.d) { best = { s: s, d: d, wid: wid }; }
      }
      st.forEach(function (s) { consider(s, true); });
      if (!best) { st.forEach(function (s) { consider(s, false); }); }
      if (!best) { return { left: band.left, w: wanted }; }
      var w = Math.min(wanted, best.wid);
      var left = best.s.left >= dialX ? best.s.left : best.s.right <= dialX ? best.s.right - w : clamp(dialX - w / 2, best.s.left, best.s.right - w);
      return { left: left, w: w };
    }
    function wireIns(root, i) {
      var x = root.querySelector('[data-a="x"]');
      if (x) { x.onclick = function (e) { e.stopPropagation(); closeUi(); clearIns(); draw(); }; }
      var more = root.querySelector('[data-a="more"]');
      if (more) { more.onclick = function (e) { e.stopPropagation(); openQuick(i == null ? view.activeIdx : i, e, true); }; }
      var back = root.querySelector('[data-a="back"]');
      if (back) { back.onclick = function (e) { e.stopPropagation(); clearIns(); view.insMine = true; draw(); }; }
      var tune = root.querySelector('[data-a="tune"]');
      if (tune && !tune.disabled) { tune.onclick = function (e) { e.stopPropagation(); band.tune(view, i); }; }
      var open = root.querySelector('[data-a="open"]');
      if (open) { open.onclick = function (e) { e.stopPropagation(); if (i === view.activeIdx || i == null) { openApplet(view.modelAt(view.activeIdx)); } else { openViewing(i); } }; }
    }
    function renderIns() {
      if (!isOne() || !inspecting()) { insRectNow = null; peekEl.style.display = 'none'; return; }
      if (view.peekIdx === view.activeIdx) { view.peekIdx = null; view.insMine = true; }
      var oldScroll = peekEl.querySelector('.ins-scroll');
      var scrollTop = oldScroll ? oldScroll.scrollTop : 0;
      var inner = document.createElement('div');
      var listOnly = view.foldIdxs && view.peekIdx == null && !view.insMine;
      if (listOnly) {
        inner.className = 'rc rc-peek rc-insp rc--quiet';
        inner.innerHTML = '<div class="rc__rail"></div><div class="rc__body"><div class="rc__head ins-head"><b class="rc__who">' + view.foldIdxs.length + ' frequencies</b><span class="rc__f"></span>' +
          '<button type="button" class="ins-b" data-a="x" aria-label="Close">\u00d7</button></div><div class="ins-scroll ins-list">' + view.foldIdxs.map(function (i) {
            var t = view.tagAt(i);
            var k = t.keyed.length > 1 ? 'dbl' : t.keyed.length === 1 ? 'talk' : 'quiet';
            return '<button type="button" class="ins-fr" data-i="' + i + '"><i class="rcg rcg--' + k + '"></i><span class="ins-fr__f">' + freqShort(view.freqs[i].hz) + '</span><span class="ins-fr__w">' + esc(whoAt(i)) + (t.keyed.length > 1 ? ' \u00b7 ' + t.keyed.join(' + ') : '') + '</span><span class="chev">\u203a</span></button>';
          }).join('') + '</div></div>';
      } else {
        var d = view.insMine ? mineData(view.modelAt(view.activeIdx)) : peekOf(view.peekIdx);
        inner.className = peekClass(d) + ' rc-insp';
        inner.innerHTML = '<div class="rc__rail"></div><div class="rc__body">' + inspectorBody(d, keyedYou()) + '</div>';
      }
      peekEl.innerHTML = '';
      peekEl.appendChild(inner);
      peekEl.style.display = 'block';
      peekEl.style.height = 'auto';
      var pl = placeIns(300);
      peekEl.style.width = pl.w + 'px';
      var room = band.foot - band.top;
      if (!listOnly && peekEl.offsetHeight > room) {
        d.compactPeople = true;
        inner.innerHTML = '<div class="rc__rail"></div><div class="rc__body">' + inspectorBody(d, keyedYou()) + '</div>';
      }
      var h = Math.min(room, peekEl.offsetHeight);
      peekEl.style.height = h + 'px';
      peekEl.style.left = pl.left + 'px';
      peekEl.style.top = (band.foot - h) + 'px';
      insRectNow = { left: pl.left, right: pl.left + pl.w, top: band.foot - h, bottom: band.foot };
      var sc = inner.querySelector('.ins-scroll');
      if (sc) {
        sc.scrollTop = scrollTop;
        // Rows below the fold say so with a fade; the wheel scrolls them here, never tunes.
        var more = function () { sc.classList.toggle('has-more', sc.scrollHeight - sc.scrollTop - sc.clientHeight > 2); };
        sc.addEventListener('scroll', more);
        more();
      }
      if (listOnly) {
        inner.querySelectorAll('[data-i]').forEach(function (b) {
          b.onclick = function (e) { e.stopPropagation(); view.peekIdx = parseInt(b.getAttribute('data-i'), 10); draw(); };
        });
        wireIns(inner, null);
      } else { wireIns(inner, view.insMine ? view.activeIdx : view.peekIdx); }
    }
    // Right-click anywhere Roundtable, or the inspector's own \u22ef.
    function openQuick(i, e, fromIns) {
      if (!isOne()) {
        if (i === view.activeIdx) { var r0 = dk.getBoundingClientRect(); var k0 = r0.width / 1512; openMenu(view.modelAt(i), { right: (e.clientX - r0.left) / k0 - 4, top: (e.clientY - r0.top) / k0 }); }
        return;
      }
      var mine = i === view.activeIdx;
      var fmtName = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
      var isOpen = mine ? view.insMine : view.peekIdx === i;
      menu.innerHTML = '<div class="rc-menu__h">' + freqShort(view.freqs[i].hz) + ' MHz</div>' +
        (fromIns ? '' : '<button type="button" data-a="view">' + (isOpen ? 'Close inspector' : 'Open inspector') + '</button>') +
        '<button type="button" data-a="tune"' + (mine ? ' disabled title="Slice A is already here"' : keyedYou() ? ' disabled title="Not while you transmit"' : '') + '>Tune slice A here</button>' +
        '<button type="button" data-a="msg">Messages</button>' +
        '<button type="button" data-a="fmt">Signal: ' + fmtName + ' \u00b7 Change</button>';
      var r = dk.getBoundingClientRect();
      var k = r.width / 1512;
      menu.style.left = Math.min((e.clientX - r.left) / k, 1140 - 190) + 'px';
      menu.style.top = Math.max(34, (e.clientY - r.top) / k) + 'px';
      menu.style.display = 'block';
      var on = function (a, fn) { var b = menu.querySelector('[data-a="' + a + '"]'); if (b) { b.onclick = function (ev) { ev.stopPropagation(); closeUi(); fn(); }; } };
      on('view', function () { toggleIns(i); });
      on('tune', function () { band.tune(view, i); });
      on('msg', function () { if (mine) { openApplet(view.modelAt(view.activeIdx)); } else { openViewing(i); } });
      on('fmt', function () { S.fmt = S.fmt === 'S' ? 'dbm' : S.fmt === 'dbm' ? 'both' : 'S'; redrawAll(); });
    }
    function peekOf(i) { return peekData(view, i, slice.hz, view.keyedAt(i).length ? (PAN_DB[i] || -72) : null); }
    function closePeek() { view.peekIdx = null; view.insMine = false; view.foldIdxs = null; insRectNow = null; peekEl.style.display = 'none'; layer.querySelectorAll('.rct.is-peek').forEach(function (t) { t.classList.remove('is-peek'); }); }
    function renderPeek() {
      var i = view.peekIdx;
      if (i == null || isOne()) { return; }
      var d = peekOf(i);
      var inner = document.createElement('div');
      inner.className = peekClass(d);
      inner.innerHTML = '<div class="rc__rail"></div><div class="rc__body">' + peekBody(d, keyedYou()) + '</div>' +
        '<button type="button" class="pk-x" aria-label="Close">\u00d7</button>';
      peekEl.innerHTML = '';
      peekEl.appendChild(inner);
      peekEl.style.display = 'block';
      var tag = layer.querySelector('.rct.is-peek');
      var tl = tag ? parseFloat(tag.style.left) : xAt(d.f) - 150;
      var tb = tag ? parseFloat(tag.style.top) + tag.offsetHeight : geo.bandTop - 8;
      peekEl.style.left = clamp(tl, 4, geo.x1 - 304) + 'px';
      peekEl.style.top = (tb + 6) + 'px';
      inner.querySelector('.pk-x').onclick = closePeek;
      inner.querySelector('[data-a="tune"]').onclick = function () { closePeek(); band.tune(view, i); };
      inner.querySelector('[data-a="open"]').onclick = function () { openViewing(i); };
    }
    function openViewing(i) {
      var d = peekOf(i);
      applet.classList.add('is-open');
      applet.innerHTML = '<div class="rc-applet__hd">\u22ee\u22ee ROUNDTABLE<span class="sp"></span><button type="button" aria-label="Close Roundtable">\u00d7</button></div>' +
        '<button type="button" class="pk-back">\u2039 Back to my slice</button>' +
        '<div class="rc-detail"><div class="' + peekClass(d) + ' rc-peek--flat"><div class="rc__body">' + peekBody(d, keyedYou()) + '</div></div>' + viewingChat() + '</div>' + viewingSend();
      applet.querySelector('.rc-applet__hd button').onclick = function () { applet.classList.remove('is-open'); };
      applet.querySelector('.pk-back').onclick = function () { closePeek(); draw(); openApplet(view.modelAt(view.activeIdx)); };
      applet.querySelector('[data-a="tune"]').onclick = function () { closePeek(); band.tune(view, i); openApplet(view.modelAt(view.activeIdx)); };
      var more = applet.querySelector('[data-a="open"]');
      more.parentNode.removeChild(more);
    }
    window.addEventListener('keydown', function (e) {
      if (e.key !== 'Escape') { return; }
      if (menu.style.display === 'block' || flagMenuOpen()) { closeUi(); return; }
      closePeek(); draw();
    });
    function closeUi() { menu.style.display = 'none'; closeFlagMenu(); }
    function openMenu(m, rect) {
      var keyed = keyedYou();
      var fmtName = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
      menu.innerHTML = '<div class="rc-menu__h">' + freqShort(m.f) + ' MHz</div>' +
        '<button type="button" data-a="read">Read reports</button>' +
        '<button type="button" data-a="fmt">Signal: ' + fmtName + ' · Change</button>' +
        '<button type="button" data-a="tune"' + (keyed ? ' disabled' : '') + '>Tune slice A here</button>';
      menu.style.left = Math.min(rect.right + 4, 1140 - 190) + 'px';
      menu.style.top = Math.max(34, rect.top) + 'px';
      menu.style.display = 'block';
      menu.querySelector('[data-a="tune"]').onclick = function () { closeUi(); slice.hz = m.f; draw(); };
      menu.querySelector('[data-a="read"]').onclick = function () { closeUi(); openApplet(m); };
      menu.querySelector('[data-a="fmt"]').onclick = function () { closeUi(); S.fmt = S.fmt === 'S' ? 'dbm' : S.fmt === 'dbm' ? 'both' : 'S'; redrawAll(); };
    }
    function openApplet(m) {
      applet.classList.add('is-open');
      applet.innerHTML = '<div class="rc-applet__hd">⋮⋮ ROUNDTABLE<span class="sp"></span><button type="button" aria-label="Close Roundtable">×</button></div>' +
        '<div class="rc-detail">' + detailHtml(m) + '</div>' + sendRow();
      applet.querySelector('button').onclick = function () { applet.classList.remove('is-open'); };
      wireSend(applet);
    }
    dk.addEventListener('click', function (e) {
      if (menu.style.display === 'block' && !menu.contains(e.target)) { menu.style.display = 'none'; }
      if (flagMenuOpen() && !flagMenu.contains(e.target)) { closeFlagMenu(); }
      if (!isOne() && !applet.contains(e.target) && view.peekIdx != null) { closePeek(); draw(); }
    });
    fline.addEventListener('click', function (e) {
      e.stopPropagation();
      if (isOne()) { closeUi(); toggleIns(view.activeIdx); return; }
      openApplet(view.modelAt(view.activeIdx));
    });
    fline.addEventListener('contextmenu', function (e) { e.preventDefault(); e.stopPropagation(); openQuick(view.activeIdx, e); });
// ---- The flag: the ... button, its menu, the meter's source ----
    var specK = 1;
    var panNow = 'tall';
    var lowBand = crop(0, 311, 1143, 48, 3);
    var lowCol = crop(1143, 311, 33, 492, 3);
    lowBand.style.display = 'none'; lowCol.style.display = 'none';
    // Your window's pan is about 230 px tall: the strip and the waterfall come up.
    function applyPan() {
      if (panNow === S.pan) { return; }
      panNow = S.pan;
      var d = S.pan === 'short' ? 51 : 0;
      geo.bandTop = 311 - d; geo.wfTop = 359 - d;
      band.foot = geo.bandTop - 8; band.bandTop = geo.bandTop;
      spec.height = (geo.bandTop - geo.specTop) * 2;
      spec.style.height = (geo.bandTop - geo.specTop) + 'px';
      wf.height = geo.wfBot - geo.wfTop;
      wf.style.top = geo.wfTop + 'px'; wf.style.height = (geo.wfBot - geo.wfTop) + 'px';
      wfMark.style.top = geo.wfTop + 'px'; wfMark.style.height = (geo.wfBot - geo.wfTop) + 'px';
      lowBand.style.display = lowCol.style.display = d ? 'block' : 'none';
      lowBand.style.top = geo.bandTop + 'px'; lowCol.style.top = geo.bandTop + 'px';
      specK = (geo.bandTop - geo.specTop) / 281;
      prefillDone = false;
      prefill();
    }
    var flagHit = document.createElement('div');
    flagHit.className = 'dk__flaghit';
    dk.appendChild(flagHit);
    flagHit.addEventListener('click', function (e) { e.stopPropagation(); closeUi(); });
    flagHit.addEventListener('contextmenu', function (e) { e.preventDefault(); e.stopPropagation(); openFlagMenu(e, 'all'); });
    var moreBtn = document.createElement('button');
    moreBtn.type = 'button';
    moreBtn.className = 'dk__more';
    moreBtn.setAttribute('aria-label', 'More for slice A');
    moreBtn.title = 'More';
    moreBtn.textContent = '⋯';
    dk.appendChild(moreBtn);
    moreBtn.addEventListener('click', function (e) {
      e.stopPropagation();
      if (flagMenuOpen()) { closeFlagMenu(); return; }
      menu.style.display = 'none';
      openFlagMenu(e, 'all');
    });
    var levelCv = document.createElement('canvas');
    levelCv.className = 'dk__level';
    levelCv.width = 488; levelCv.height = 68;
    dk.appendChild(levelCv);
    var txLit = document.createElement('div');
    txLit.className = 'dk__txlit';
    txLit.textContent = 'TX';
    dk.appendChild(txLit);
    var antEl = document.createElement('div');
    antEl.className = 'dk__ant';
    dk.appendChild(antEl);
    var toast = document.createElement('div');
    toast.className = 'dk__toast';
    dk.appendChild(toast);
    var toastTimer = null;
    function say(t) { toast.textContent = t; toast.classList.add('is-on'); clearTimeout(toastTimer); toastTimer = setTimeout(function () { toast.classList.remove('is-on'); }, 2600); }
    var flagMenu = document.createElement('div');
    flagMenu.className = 'fl-menu';
    dk.appendChild(flagMenu);
    var FM = { tx: false, ant: 'ANT1', rate: 192 };
    function flagMenuOpen() { return flagMenu.style.display === 'block'; }
    function closeFlagMenu() { flagMenu.style.display = 'none'; moreBtn.classList.remove('is-lit'); }
    function rxItems() {
      return RX_MODES.map(function (m) {
        return { t: m, c: S.rx === m, f: function () { S.rx = m; pressRx(); } };
      });
    }
    function menuItems(mode) {
      if (mode === 'rx') { return rxItems(); }
      var items = [
        { t: 'Make this the TX slice', c: FM.tx, f: function () { FM.tx = !FM.tx; placeFlagParts(parseFloat(flagHit.style.left), xAt(slice.hz)); } },
        { sep: 1 },
        { t: 'Antenna >', sub: ['ANT1', 'ANT2', 'ANT3'].map(function (a) { return { t: a, c: FM.ant === a, f: function () { FM.ant = a; placeFlagParts(parseFloat(flagHit.style.left), xAt(slice.hz)); } }; }) },
        { t: 'Sample rate >', sub: [48, 96, 192, 384, 768, 1536].map(function (r) { return { t: r + ' kHz', c: FM.rate === r, f: function () { FM.rate = r; say('Sample rate ' + r + ' kHz'); } }; }) }
      ];
      if (isOne()) { items.push({ t: 'S-meter: ' + S.rx + ' >', sub: rxItems(), id: 'rx' }); }
      items.push({ sep: 1 },
        { t: 'Diversity...', f: function () { say('Opens the Diversity dialog, as it does today'); } },
        { t: 'Filter policy...', f: function () { say('Opens the Filter policy dialog, as it does today'); } },
        { sep: 1 },
        { t: 'Remove slice', off: true, tip: 'Slice A always stays' });
      return items;
    }
    function itemEl(it) {
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'fl-it' + (it.c ? ' is-c' : '');
      b.textContent = it.t;
      if (it.off) { b.disabled = true; b.title = it.tip || ''; }
      if (it.f) { b.addEventListener('click', function (e) { e.stopPropagation(); closeFlagMenu(); it.f(); }); }
      return b;
    }
    function fillMenu(host, items) {
      items.forEach(function (it) {
        if (it.sep) { var s = document.createElement('div'); s.className = 'fl-sep'; host.appendChild(s); return; }
        if (!it.sub) { host.appendChild(itemEl(it)); return; }
        var w = document.createElement('div');
        w.className = 'fl-has';
        var b = itemEl(it);
        b.addEventListener('click', function (e) {
          e.stopPropagation();
          var was = w.classList.contains('is-open');
          host.querySelectorAll('.fl-has.is-open').forEach(function (o) { o.classList.remove('is-open'); });
          w.classList.toggle('is-open', !was);
        });
        var sub = document.createElement('div');
        sub.className = 'fl-sub';
        fillMenu(sub, it.sub);
        w.appendChild(b); w.appendChild(sub);
        host.appendChild(w);
      });
    }
    function openFlagMenu(e, mode) {
      flagMenu.innerHTML = '';
      fillMenu(flagMenu, menuItems(mode));
      var r = dk.getBoundingClientRect();
      var k = r.width / 1512;
      var fromBtn = e.currentTarget === moreBtn;
      var x = fromBtn ? parseFloat(moreBtn.style.left) : (e.clientX - r.left) / k;
      var y = fromBtn ? parseFloat(moreBtn.style.top) + 23 : (e.clientY - r.top) / k;
      flagMenu.style.left = Math.min(x, 1512 - 210) + 'px';
      flagMenu.style.top = y + 'px';
      flagMenu.style.display = 'block';
      moreBtn.classList.toggle('is-lit', fromBtn);
    }
    function pressRx() { drawFlagMeter(); phoneRxChanged(); }
    function placeFlagParts(flagLeft, x) {
      flagHit.style.left = flagLeft + 'px';
      moreBtn.style.display = isOne() ? 'grid' : 'none';
      moreBtn.style.left = (band.flipped ? x + 3 : x - 23) + 'px';
      moreBtn.style.top = '55px';
      levelCv.style.left = (flagLeft + 5) + 'px';
      txLit.style.display = FM.tx ? 'block' : 'none';
      txLit.style.left = (flagLeft + 197) + 'px';
      antEl.style.display = FM.ant === 'ANT1' ? 'none' : 'block';
      antEl.style.left = (flagLeft + 9) + 'px';
      antEl.textContent = FM.ant;
    }
    var meterCv = document.createElement('canvas');
    meterCv.className = 'dk__smeter';
    meterCv.width = 560; meterCv.height = 246;
    dk.appendChild(meterCv);
    meterCv.addEventListener('contextmenu', function (e) { e.preventDefault(); e.stopPropagation(); closeUi(); openFlagMenu(e, 'rx'); });
    meterCv.addEventListener('click', function (e) { e.stopPropagation(); closeUi(); });
    var MS = { avg: -112, pk: -112, needle: -112 };
    function meterNow() {
      var t = Date.now() / 1000;
      var idx = view.activeIdx;
      var keyed = keyedYou() ? [] : view.keyedAt(idx);
      var base = idx === 0 ? -72 : idx === 1 ? -76 : idx === 2 ? -83 : -100;
      var raw = keyed.length ? Math.max(-114, base + voiceLevel(t, keyed[0].charCodeAt(0) + idx)) : -114 + Math.random() * 3;
      MS.avg += (raw - MS.avg) * 0.06;
      MS.pk = Math.max(raw, MS.pk - 0.5);
      if (S.rx === 'Sig Avg') { return MS.avg; }
      if (S.rx === 'Signal Peak') { return MS.pk; }
      if (S.rx === 'Max Bin') {
        // The strongest point of the passband on this pan: it reads what the trace shows.
        var dx = xAt(slice.hz);
        var lo = Math.max(0, Math.floor(dx - 2700 / geo.hzPerPx));
        var hi = Math.min(bins - 1, Math.floor(dx));
        var best = -140;
        for (var b0 = lo; b0 <= hi; b0++) { if (cur[b0] > best) { best = cur[b0]; } }
        return best;
      }
      return raw;
    }
    function drawFlagMeter() {
      var v = meterNow();
      paintLevel(levelCv.getContext('2d'), 244, 34, 2, v, isOne() ? rxTag(S.rx) : '', {
        bgFill: '#0a0a0e', tick: '#7f93a6', bg: '#10101c', border: '#304050', top: 3, tickH: 14, dbmW: 60,
        tickFont: '600 10px system-ui, -apple-system, sans-serif', dbmFont: '700 12px system-ui, -apple-system, sans-serif', digits: 0
      });
      MS.needle += (v - MS.needle) * (v > MS.needle ? 0.5 : 0.18);
      paintFace(meterCv.getContext('2d'), MS.needle, v, S.rx);
    }

    function tunePan(e) {
      if (keyedYou()) { return; }
      if (!isOne()) { closePeek(); }
      var r = dk.getBoundingClientRect();
      var k = r.width / 1512;
      var x = (e.clientX - r.left) / k;
      var hz = Math.round(hzAt(x) / 500) * 500;
      slice.hz = hz;
      view.activeIdx = -1;
      for (var i = 0; i < view.freqs.length; i++) {
        if (Math.abs(view.freqs[i].hz - hz) <= 1500) { view.activeIdx = i; slice.hz = view.freqs[i].hz; }
      }
      if (view.activeIdx < 0) { view.activeIdx = 0; slice.hz = hz; view.freqs[0].hz = hz; } // the on-air scene comes along, so the card's placement shows anywhere on the band
      closeUi();
      draw();
    }
    spec.addEventListener('click', tunePan);
    wf.addEventListener('click', tunePan);

    var wfMark = document.createElement('div');
    wfMark.style.cssText = 'position:absolute;top:' + geo.wfTop + 'px;height:' + (geo.wfBot - geo.wfTop) + 'px;width:' + (2700 / geo.hzPerPx) + 'px;background:rgba(0,180,210,0.14);border-right:1px solid rgba(0,212,255,0.85);box-sizing:content-box;pointer-events:none;z-index:2';
    dk.appendChild(wfMark);
    function draw() {
      var x = xAt(slice.hz);
      applyPan();
      wfMark.style.left = (x - 2700 / geo.hzPerPx) + 'px';
      band.flipped = x + flagW > band.right;
      var flagLeft = band.flipped ? x - flagW : x + 1;
      flag.style.left = flagLeft + 'px';
      lock.style.left = (band.flipped ? x + 4 : x - 22) + 'px';
      marker.style.left = (x - 5) + 'px';
      fline.style.left = flagLeft + 'px'; fline.style.top = '165px'; fline.style.width = flagW + 'px';
      fline.innerHTML = flagInner(view.modelAt(view.activeIdx));
      fline.classList.toggle('is-open', isOne() && !!view.insMine);
      placeFlagParts(flagLeft, x);
      freqEl.style.cssText = 'left:' + (flagLeft + 100) + 'px;top:72px;width:138px;height:28px';
      var digits = (slice.hz / 1e6).toFixed(6);
      freqEl.textContent = digits.slice(0, -6 + 3) + '.' + digits.slice(-3);
      freqEl.style.display = slice.hz === 7185000 ? 'none' : 'block';
      txEl.style.display = keyedYou() ? 'block' : 'none';
      renderIns();
      view.draw(band);
      band.x = xAt;
    }
    redrawers.push(function () {
      closeUi();
      if (lastUx !== S.ux + S.look) { lastUx = S.ux + S.look; closePeek(); applet.classList.remove('is-open'); }
      if (S.look === 'today') { closePeek(); }
      draw();
      renderPeek();
      if (applet.classList.contains('is-open') && !applet.querySelector('.pk-back')) { openApplet(view.modelAt(view.activeIdx)); }
    });

    // Live spectrum and waterfall.
    var bins = geo.x1;
    var cur = new Float32Array(bins);
    var floor = new Float32Array(bins);
    for (var i = 0; i < bins; i++) { floor[i] = -105 + 1.5 * Math.sin(i * 0.006 + 1); cur[i] = floor[i]; }
    var lut = new Uint8ClampedArray(256 * 3);
    var stops = [[0, 0x000000], [0.15, 0x000080], [0.30, 0x0040ff], [0.45, 0x00c8ff], [0.60, 0x00dc00], [0.80, 0xffff00], [1, 0xff0000]];
    for (var q = 0; q < 256; q++) {
      var v = q / 255; var k = 0;
      while (k < stops.length - 2 && v > stops[k + 1][0]) { k++; }
      var a = stops[k]; var c = stops[k + 1]; var u = (v - a[0]) / (c[0] - a[0]);
      lut[q * 3] = ((a[1] >> 16) & 255) + (((c[1] >> 16) & 255) - ((a[1] >> 16) & 255)) * u;
      lut[q * 3 + 1] = ((a[1] >> 8) & 255) + (((c[1] >> 8) & 255) - ((a[1] >> 8) & 255)) * u;
      lut[q * 3 + 2] = (a[1] & 255) + ((c[1] & 255) - (a[1] & 255)) * u;
    }
    var rowImg = wfc.createImageData(bins, 1);
    var line = new Float32Array(bins);
    function rowAt(t) {
      for (var b = 0; b < bins; b++) {
        line[b] = floor[b] - 6 * Math.log(Math.random() + 1e-6) * 0.72 - 4;
      }
      view.freqs.forEach(function (e, idx) {
        var keyed = view.keyedAt(idx);
        var base = idx === 0 ? -72 : idx === 1 ? -76 : idx === 2 ? -83 : -100;
        keyed.forEach(function (name, n) {
          var db = base + voiceLevel(t, name.charCodeAt(0) + n * 2.3 + idx);
          var cx = xAt(e.hz);
          var left = Math.floor(cx - 2900 / geo.hzPerPx);
          for (var x = Math.max(0, left); x < Math.min(bins, cx); x++) {
            var aud = (e.hz - hzAt(x));
            var shape = aud > 450 ? -18 * Math.pow((aud - 450) / 2450, 1.15) : -7;
            var s = db + shape + (Math.random() - 0.5) * 9;
            if (s > line[x]) { line[x] = s; }
          }
        });
      });
      return line;
    }
    // Prefill the waterfall.
    var prefillDone = false;
    function pushWf(t) {
      var r = rowAt(t);
      wfc.drawImage(wf, 0, 0, bins, wf.height - 1, 0, 1, bins, wf.height - 1);
      for (var b = 0; b < bins; b++) {
        var val = clamp(Math.round((r[b] + 118) / 50 * 255), 0, 255);
        rowImg.data[b * 4] = lut[val * 3]; rowImg.data[b * 4 + 1] = lut[val * 3 + 1]; rowImg.data[b * 4 + 2] = lut[val * 3 + 2]; rowImg.data[b * 4 + 3] = 255;
      }
      wfc.putImageData(rowImg, 0, 0);
      return r;
    }
    var lastRow = 0;
    function drawSpec(r) {
      var W = spec.width, H = spec.height;
      spc.clearRect(0, 0, W, H);
      spc.fillStyle = '#050507';
      spc.fillRect(0, 0, W, H);
      // grid
      spc.strokeStyle = 'rgba(255,255,255,0.028)';
      spc.lineWidth = 1;
      spc.beginPath();
      for (var gx = 0; gx <= 8; gx++) { var X = Math.round((64 + gx * 148.75) * 2); spc.moveTo(X, 0); spc.lineTo(X, H); }
      for (var gy = 0; gy <= 7; gy++) { var Y = Math.round(gy * 41 * 2 + 30); spc.moveTo(0, Y); spc.lineTo(W, Y); }
      spc.stroke();
      // passband
      var dx = xAt(slice.hz);
      spc.fillStyle = 'rgba(0,160,190,0.16)';
      spc.fillRect((dx - 2700 / geo.hzPerPx) * 2, 0, (2700 / geo.hzPerPx) * 2, H);
      var yOf = function (db) { return ((60 - geo.specTop) + (-50 - db) * (245 / 60) * specK) * 2; };
      for (var b = 0; b < bins; b++) { cur[b] += (r[b] - cur[b]) * 0.55; }
      var grad = spc.createLinearGradient(0, 0, 0, H);
      grad.addColorStop(0, 'rgba(10,120,145,0.55)');
      grad.addColorStop(1, 'rgba(6,62,78,0.45)');
      spc.beginPath();
      spc.moveTo(0, H);
      for (var i2 = 0; i2 < bins; i2++) { spc.lineTo(i2 * 2, yOf(cur[i2])); }
      spc.lineTo(W, H);
      spc.closePath();
      spc.fillStyle = grad;
      spc.fill();
      spc.beginPath();
      for (var i3 = 0; i3 < bins; i3++) { if (i3 === 0) { spc.moveTo(0, yOf(cur[0])); } else { spc.lineTo(i3 * 2, yOf(cur[i3])); } }
      spc.strokeStyle = '#2fb2c6';
      spc.lineWidth = 1;
      spc.stroke();
      // slice line (the marker triangle comes with the flag's crop)
      spc.strokeStyle = 'rgba(0,212,255,0.85)';
      spc.lineWidth = 2;
      spc.beginPath(); spc.moveTo(dx * 2, 0); spc.lineTo(dx * 2, H); spc.stroke();
    }
    function prefill() {
      if (prefillDone) { return; }
      prefillDone = true;
      var now = Date.now() / 1000;
      for (var y = wf.height - 1; y >= 0; y--) { pushWf(now - y * 0.05); }
    }
    function loop(now) {
      if (now - lastRow > 50) {
        lastRow = now;
        var r = pushWf(Date.now() / 1000);
        drawSpec(r);
        drawFlagMeter();
      }
      requestAnimationFrame(loop);
    }
    function fit() {
      var box = document.getElementById('dk-fit');
      var k = Math.min(1, box.clientWidth / 1512);
      dk.style.transform = 'scale(' + k + ')';
      box.style.height = Math.round(869 * k) + 'px';
    }
    window.addEventListener('resize', fit);
    fit();
    prefill();
    draw();
    requestAnimationFrame(loop);
    view.tickLive = function () {
      view.refreshLive();
      if (view.peekIdx != null && peekEl.firstChild) { refreshCard(peekEl.firstChild, peekOf(view.peekIdx)); }
      if (view.insMine && peekEl.firstChild) { refreshCard(peekEl.firstChild, mineData(view.modelAt(view.activeIdx))); }
    };
    view.redrawCard = draw;
  }

  // ---------- Phones ----------
  function buildPhone(p) {
    var frame = p.root.parentElement;
    var kind = frame.getAttribute('data-rc-phone');
    var isPad = kind === 'ipad' || kind === 'ipad-wide';
    var layer = document.createElement('div');
    layer.className = 'rcp';
    p.specWrap.appendChild(layer);
    var hz0 = 7236400;
    var view = new View({
      freqs: [
        { hz: hz0 },
        { hz: 7225000, keyed: ['AA5ABC', 'K0ABC'], calls: ['AA5ABC', 'K0ABC', 'W2ABC'] },
        { hz: 7249000, keyed: ['W6ABC'], calls: ['W6ABC', 'KC2ABC'] }
      ].concat(isPad ? [{ hz: 7261000, keyed: [], calls: ['W2DEF', 'N5ABC', 'K7ABC'] }] : [])
    });
    views.push(view);
    view.backToScenario = function () { view.activeIdx = 0; p.tuneTo(view.freqs[0].hz); };

    var keyedYou = function () { return view.activeIdx === 0 && (S.state === 'you' || S.state === 'youdbl'); };
    var actions = document.createElement('div');
    actions.className = 'rc-actions';
    p.root.appendChild(actions);
    var sheet = document.createElement('section');
    sheet.className = 'rc-psheet';
    p.root.appendChild(sheet);

    var lastUx = S.ux + S.look;
    function closeActions() { actions.classList.remove('is-open'); }
    // The sheet's own \u22ef: what is left once the sheet itself is the reading.
    function openInsActions(m) {
      var fmtName = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
      actions.innerHTML = '<div class="rc-actions__h">' + freqShort(m.f) + ' MHz</div>' +
        '<button type="button" data-a="fmt">Signal: ' + fmtName + ' \u00b7 Change</button>' +
        '<button type="button" data-a="x">Cancel</button>';
      actions.classList.add('is-open');
      actions.querySelector('[data-a="fmt"]').onclick = function () { closeActions(); S.fmt = S.fmt === 'S' ? 'dbm' : S.fmt === 'dbm' ? 'both' : 'S'; redrawAll(); };
      actions.querySelector('[data-a="x"]').onclick = closeActions;
    }
    function openActions(m) {
      var fmtName = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
      actions.innerHTML = '<div class="rc-actions__h">People at ' + freqShort(m.f) + '</div>' +
        '<button type="button" data-a="read">Read reports</button>' +
        '<button type="button" data-a="fmt">Signal: ' + fmtName + ' · Change</button>' +
        '<button type="button" data-a="tune"' + (keyedYou() ? ' disabled' : '') + '>Tune slice A here</button>' +
        '<button type="button" data-a="x">Cancel</button>';
      actions.classList.add('is-open');
      actions.querySelector('[data-a="read"]').onclick = function () { closeActions(); openSheet(m); };
      actions.querySelector('[data-a="fmt"]').onclick = function () { closeActions(); S.fmt = S.fmt === 'S' ? 'dbm' : S.fmt === 'dbm' ? 'both' : 'S'; redrawAll(); };
      actions.querySelector('[data-a="tune"]').onclick = function () { closeActions(); p.tuneTo(m.f); };
      actions.querySelector('[data-a="x"]').onclick = closeActions;
    }
    // ---- iPad: the Roundtable applet in the column ----
    var col = isPad ? p.root.querySelector('.acol') : null;
    var applet = null;
    var earlierOpen = false;
    function columnShowing() { return !!col && col.offsetParent !== null; }
    function padView() { return view.peekIdx != null ? peekOf(view.peekIdx) : mineData(view.modelAt(view.activeIdx)); }
    function padChat(viewing) {
      return '<div class="rci-msg"><small>' + (viewing ? 'MESSAGES \u00b7 READ ONLY UNTIL YOU TUNE HERE' : 'MESSAGES') + '</small>' +
        (viewing ? '<div><b>W2ABC</b> Good signal into Albany.</div><div><b>N7ABC</b> Fading here, still solid copy.</div>' :
          '<div><b>N4RDR</b> Good copy into Knoxville.</div><div><b>KD8LMN</b> Solid here, thanks.</div>') + '</div>';
    }
    function padEarlier() {
      return '<details class="rci-more"' + (earlierOpen ? ' open' : '') + '><summary>EARLIER TURNS</summary>' +
        '<div class="rci-turn"><i class="rcg rcg--talk"></i><span>W9ABC</span><span>2:40 \u00b7 8 heard</span></div>' +
        '<div class="rci-turn"><i class="rcg rcg--dbl"></i><span>K4ABC + N4XYZ</span><span>0:09 \u00b7 combined</span></div>' +
        '<div class="rci-turn"><i class="rcg rcg--you"></i><span>You</span><span>1:10 \u00b7 9 heard</span></div></details>';
    }
    function padHead(d) {
      var digits = (d.f / 1e6).toFixed(3);
      var line = S.link === 'recon' ? ['is-recon', 'Reconnecting \u00b7 last known'] : S.link === 'offline' ? ['is-off', 'Offline since 14:32 UTC \u00b7 last known'] :
        S.link === 'hidden' ? ['is-off', 'You are hidden \u00b7 reading only'] : d.mine ? ['', d.live ? 'Live \u00b7 last reading 1s ago' : d.kind === 'quiet' ? 'Live \u00b7 nobody talking' : 'Live \u00b7 turn ended'] : ['', 'Live \u00b7 viewing \u00b7 slice A stays put'];
      return '<div class="rci-head"><b>' + digits + '.' + String(d.f % 1000).padStart(3, '0') + '</b><em>LSB</em><span>' + d.people.length + ' stations</span></div>' +
        '<div class="rci-live ' + line[0] + '"><i></i>' + line[1] + '</div>';
    }
    function padToday(d) {
      // RoundtableRoomView.swift talker and readings: no meters in the narrow column.
      var clock = mmss(d.clock);
      var h = padHead(d);
      if (d.kind === 'dbl') {
        h += '<div class="rci0-cap">TWO AT ONCE</div><div class="rci0-talk dbl"><i class="rcg rcg--dbl"></i><b>' + d.keyed.join(' + ') + '</b><i class="d">DOUBLED</i></div>' +
          '<div class="rci0-small">for ' + clock + ' \u00b7 each reading is both signals together</div>';
      } else if (d.kind === 'unk') {
        h += '<div class="rci0-talk unk"><i class="rcg rcg--unk"></i><b>Unknown talker</b><span>keyed ' + clock + '</span></div><div class="rci0-small">not on NereusSDR \u00b7 on air ' + clock + '</div>';
      } else if (d.kind === 'talk' || d.kind === 'you') {
        h += '<div class="rci0-talk"><i class="rcg rcg--' + (d.kind === 'you' ? 'you' : 'talk') + '"></i><b>' + (d.kind === 'you' ? 'You' : d.keyed[0]) + '</b><small>EM73</small><span>talking ' + clock + '</span></div>';
      } else {
        h += '<div class="rci0-cap">QUIET</div><div class="rci0-talk quiet"><i class="rcg rcg--quiet"></i><b>Nobody is talking</b></div>';
        if (d.kind === 'last') { h += '<div class="rci0-small">' + (d.wasDouble ? 'Last turn was doubled \u00b7 combined RF (' + d.talkers.join(' + ') + '), ' + age(d.ago) + ' ago' : 'Last talker: ' + d.talkers[0] + ', ' + age(d.ago) + ' ago') + '</div>'; }
      }
      var unit = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
      var wide = col.classList.contains('rt-wide');
      if (d.reports.length) {
        h += '<div class="rci0-h"><span>Heard by</span><span>mi</span><span>Offset</span>' + (wide ? '<span></span>' : '') + '<span>' + unit + '</span><span>Age</span></div>';
        d.reports.forEach(function (r) {
          var bar = wide ? '<span class="rci0-b"><i style="width:' + (frac(r.d) * 100).toFixed(1) + '%"></i></span>' : '';
          h += '<div class="rci0-r' + (r.you ? ' is-you' : '') + (r.old ? ' is-old' : '') + '"><span class="c">' + (r.you ? 'You' : esc(r.label || r.c)) + '</span><span class="p">' + (d.showPath && r.mi ? r.mi.toLocaleString() : '') + '</span><span class="p">' + (r.label ? '' : OFFSETS[hashCall(r.c) % 6]) + '</span>' + bar + '<span>' + signal(r.d, S.fmt) + '</span><span class="a">' + age(r.a) + '</span></div>';
        });
      }
      if (d.kind !== 'quiet' && !d.mine) { h += '<div class="pk-acts" style="padding:6px 10px;margin:0"><button type="button" class="pk-go" data-a="tune"' + (keyedYou() ? ' disabled' : '') + '>Tune slice A here</button></div>'; }
      return h;
    }
    function padMain(d) {
      if (S.look === 'today') { return padToday(d); }
      var k = d.kind === 'quiet' ? 'quiet' : d.kind;
      return padHead(d) + '<div class="rci-rail rci-rail--' + (S.link === 'recon' || S.link === 'offline' ? 'quiet' : k) + '"></div><div class="rci-room"><div class="' + peekClass(d) + ' rc-peek--flat"><div class="rc__body">' + peekBody(d, keyedYou()) + '</div></div></div>';
    }
    function padList() {
      var rows = view.freqs.map(function (e, i) { return i; }).sort(function (a, c) { return view.freqs[a].hz - view.freqs[c].hz; });
      var today = S.look === 'today';
      return '<h5>40M <span>' + rows.length + ' frequencies</span></h5>' + rows.map(function (i) {
        var d = i === view.activeIdx ? mineData(view.modelAt(i)) : peekData(view, i, p.slices[p.active].target, null);
        var mine = i === view.activeIdx;
        var cls = d.kind === 'dbl' ? 'dbl' : d.kind === 'unk' ? 'unk' : d.kind === 'quiet' || d.kind === 'last' ? 'quiet' : 'talk';
        var who = d.kind === 'dbl' ? (today ? 'Doubled' : 'Double') : d.kind === 'unk' ? 'Unknown talker' : d.kind === 'you' ? 'You' : d.kind === 'talk' ? d.keyed[0] : d.kind === 'last' ? (d.wasDouble ? 'Last double' : d.talkers[0]) : 'Quiet';
        var sub = (d.kind === 'dbl' && !today ? d.keyed.map(function (c) { return c === YOU ? 'You' : c; }).join(' + ') : d.people.length + ' stations') + (mine ? ' \u00b7 you' : '');
        return '<button type="button" class="rci-l is-' + cls + (mine ? ' is-mine' : '') + (view.peekIdx === i ? ' is-sel' : '') + '" data-room="' + i + '"><i class="rcg rcg--' + (d.kind === 'you' ? 'you' : cls) + '"></i><span class="rci-l__f">' + (d.f / 1e6).toFixed(3) + '</span><span class="rci-l__w"><span>' + esc(who) + '</span><small>' + esc(sub) + '</small></span></button>';
      }).join('');
    }
    function renderPad() {
      if (!applet) { return; }
      var draftEl = applet.querySelector('.rc-send input');
      var draft = draftEl ? draftEl.value : '';
      var d = padView();
      var viewing = view.peekIdx != null;
      var live = canSend() && !viewing;
      var main = (viewing ? '<button type="button" class="pk-back">\u2039 Back to my slice</button>' : '') + padMain(d) + padEarlier() + padChat(viewing) +
        (viewing ? viewingSend() : sendRow());
      applet.innerHTML = col.classList.contains('rt-wide') ? '<div class="rci-split"><div class="rci-list">' + padList() + '</div><div class="rci-main">' + main + '</div></div>' : main;
      var back = applet.querySelector('.pk-back');
      if (back) { back.onclick = closePeek; }
      var tune = applet.querySelector('[data-a="tune"]');
      if (tune) { var i = view.peekIdx; tune.onclick = function () { band.tune(view, i); closePeek(); }; }
      var open = applet.querySelector('[data-a="open"]');
      if (open) { open.parentNode.removeChild(open); }
      var det = applet.querySelector('details');
      if (det) { det.addEventListener('toggle', function () { earlierOpen = det.open; }); }
      applet.querySelectorAll('[data-room]').forEach(function (b) {
        b.onclick = function () {
          var i2 = parseInt(b.getAttribute('data-room'), 10);
          view.peekIdx = i2 === view.activeIdx ? null : i2;
          renderPad();
          draw();
        };
      });
      if (live) { wireSend(applet); var inp = applet.querySelector('.rc-send input'); if (inp) { inp.value = draft; } }
    }
    function openSheet(m) {
      if (columnShowing()) { view.peekIdx = null; renderPad(); applet.scrollIntoView({ block: 'nearest' }); return; }
      sheet.classList.add('is-open');
      sheet.innerHTML = '<button class="rc-psheet__grab" type="button" aria-label="Sheet handle"></button>' +
        '<div class="rc-psheet__hd">Roundtable <span style="color:#79a8d2;font-weight:600">' + freqShort(m.f) + '</span><button type="button" aria-label="Close">×</button></div>' +
        '<div class="rc-detail">' + detailHtml(m) + '</div>' + sendRow();
      sheet.querySelector('.rc-psheet__hd button').onclick = function () { sheet.classList.remove('is-open'); };
      wireSend(sheet);
    }
    function peekOf(i) { return peekData(view, i, p.slices[p.active].target, view.keyedAt(i).length ? (i === 1 ? -82 : -92) : null); }
    function closePeek() { view.peekIdx = null; view.insMine = false; sheet.classList.remove('is-open', 'is-ins'); sheet.__peek = false; renderPad(); draw(); }
    // One sheet for every frequency. The same thing tapped again closes it.
    function toggleIns(i) {
      closeActions();
      var mine = i === view.activeIdx;
      if (columnShowing()) {
        view.peekIdx = mine || view.peekIdx === i ? null : i;
        renderPad();
        draw();
        return;
      }
      var same = sheet.__peek && (mine ? view.insMine : view.peekIdx === i);
      if (same) { closePeek(); return; }
      view.peekIdx = mine ? null : i;
      view.insMine = mine;
      renderInsSheet();
      draw();
    }
    function renderInsSheet() {
      var mine = !!view.insMine;
      var i = view.peekIdx;
      var d = mine ? mineData(view.modelAt(view.activeIdx)) : peekOf(i);
      var draftEl = sheet.querySelector('.rc-send input');
      var draft = draftEl ? draftEl.value : '';
      sheet.__peek = true;
      sheet.classList.add('is-open', 'is-ins');
      sheet.innerHTML = '<button class="rc-psheet__grab" type="button" aria-label="Sheet handle"></button>' +
        '<div class="rc-psheet__hd">' + (mine ? 'Roundtable <span style="color:#79a8d2;font-weight:600">' + freqShort(d.f) + '</span>' : '<button type="button" class="pk-back">\u2039 Back to my slice</button>') +
        '<span class="sp"></span><button type="button" class="ic" data-a="more" aria-label="More">\u22ef</button><button type="button" class="ic" data-a="x" aria-label="Close">\u00d7</button></div>' +
        '<div class="rc-detail"><div class="' + peekClass(d) + ' rc-peek--flat"><div class="rc__body">' + peekBody(d, keyedYou()) + (mine ? '<div class="pk-acts"><button type="button" class="pk-go" disabled title="Slice A is already here">Tune slice A here</button></div>' : '') + '</div></div>' + (mine ? '<div class="msg"><small>Messages</small><div><b>N4RDR</b> Good copy into Knoxville.</div><div><b>KD8LMN</b> Solid here, thanks.</div></div>' : viewingChat()) + '</div>' + (mine ? sendRow() : '');
      var back = sheet.querySelector('.pk-back');
      if (back) { back.onclick = function () { view.peekIdx = null; view.insMine = true; renderInsSheet(); draw(); }; }
      sheet.querySelector('[data-a="x"]').onclick = closePeek;
      sheet.querySelector('[data-a="more"]').onclick = function () { openInsActions(d); };
      var tune = sheet.querySelector('[data-a="tune"]');
      if (tune && !mine) { tune.onclick = function () { band.tune(view, i); closePeek(); }; }
      var more = sheet.querySelector('[data-a="open"]');
      if (more) { more.parentNode.removeChild(more); }
      if (mine) { wireSend(sheet); var inp = sheet.querySelector('.rc-send input'); if (inp) { inp.value = draft; } }
    }
    function openPeek(i) {
      if (isOne()) { toggleIns(i); return; }
      view.peekIdx = i;
      if (columnShowing()) { renderPad(); return; }
      sheet.__peek = true;
      var d = peekOf(i);
      sheet.classList.add('is-open');
      sheet.innerHTML = '<button class="rc-psheet__grab" type="button" aria-label="Sheet handle"></button>' +
        '<div class="rc-psheet__hd"><button type="button" class="pk-back">\u2039 Back to my slice</button><button type="button" aria-label="Close">\u00d7</button></div>' +
        '<div class="rc-detail"><div class="' + peekClass(d) + ' rc-peek--flat"><div class="rc__body">' + peekBody(d, keyedYou()) + '</div></div>' + viewingChat() + '</div>';
      sheet.querySelector('.pk-back').onclick = closePeek;
      sheet.querySelector('.rc-psheet__hd button:last-child').onclick = closePeek;
      sheet.querySelector('[data-a="tune"]').onclick = function () { band.tune(view, i); closePeek(); };
      var more = sheet.querySelector('[data-a="open"]');
      more.parentNode.removeChild(more);
    }
    if (kind === 'sheet') { setTimeout(function () { if (isOne()) { toggleIns(view.activeIdx); } else { openSheet(view.modelAt(0)); } }, 60); }

    var band = {
      layer: layer, isPhone: true,
      get w() { return p.W; }, get h() { return p.specH; }, get top() { return 2; },
      get foot() { return p.specH - p.bandH - 6; }, get bandTop() { return p.specH - p.bandH; },
      left: kind === 'land' ? 46 : 3, get right() { return p.W - 28; },
      // Landscape has no height to stand under the buttons: the card sits beside them instead.
      cardTop: (isPad || kind === 'land') ? null : function () {
        var bottom = 2;
        var wrap = p.specWrap.getBoundingClientRect();
        var k = wrap.width / (p.specWrap.offsetWidth || wrap.width);
        p.root.querySelectorAll('[data-fbtns]').forEach(function (el) {
          if (el.offsetParent !== null) { bottom = Math.max(bottom, (el.getBoundingClientRect().bottom - wrap.top) / k + 4); }
        });
        return bottom;
      },
      cardW: isPad ? 232 : 176, minW: isPad ? 200 : 92, underFlag: isPad,
      x: function (hz) { return p.fx(hz); },
      flags: function () {
        var out = [];
        var wrap = p.specWrap.getBoundingClientRect();
        var k = wrap.width / (p.specWrap.offsetWidth || wrap.width);
        p.root.querySelectorAll('[data-flag], [data-fbtns]').forEach(function (el) {
          if (el.offsetParent === null) { return; }
          var r = el.getBoundingClientRect();
          out.push({ left: (r.left - wrap.left) / k, right: (r.right - wrap.left) / k, top: (r.top - wrap.top) / k, bottom: (r.bottom - wrap.top) / k });
        });
        return out;
      },
      peek: function (v, i) { closeActions(); if (isOne()) { toggleIns(i); return; } openPeek(i); draw(); },
      saidByFlag: function (hz) {
        return p.slices.some(function (s, i) { return i !== p.active && s.target === hz && !s.flag.hidden; });
      },
      tune: function (v, i) { if (keyedYou()) { return; } v.activeIdx = i; p.tuneTo(v.freqs[i].hz); setTimeout(draw, 40); },
      openCard: function (v, m) {
        if (isOne()) { toggleIns(v.activeIdx); return; }
        openActions(m);
      }
    };
    function flagLines() {
      p.root.querySelectorAll('.rt-fline').forEach(function (d, i) {
        var at = view.activeIdx;
        if (i !== p.active) {
          // The other slice's flag tells what is on its own frequency.
          at = -1;
          view.freqs.forEach(function (e, k) { if (e.hz === p.slices[i].target && k !== view.activeIdx) { at = k; } });
          if (at < 0) { return; }
        }
        var m = view.modelAt(at);
        var html = flagInner(m);
        // The board's own refresh writes this line too, so check it is still ours.
        if (d.__h !== html || !d.querySelector('.rcg')) { d.innerHTML = html; d.__h = html; p.place(); }
        d.classList.toggle('is-dbl', m.kind === 'dbl' && S.look !== 'today');
      });
    }
    // The board's own refresh also writes this line; put ours straight back, before it paints.
    new MutationObserver(function () {
      var d = p.root.querySelectorAll('.rt-fline')[p.active];
      if (d && !d.querySelector('.rcg')) { flagLines(); return; }
      var all = p.root.querySelectorAll('.rt-fline');
      for (var n = 0; n < all.length; n++) { if (!all[n].querySelector('.rcg') && !all[n].__clean) { all[n].__clean = true; flagLines(); return; } }
    }).observe(p.root, { childList: true, subtree: true });
    // Replace the board's old sheet on the flag line with this one. Another slice's flag line views that slice's frequency.
    p.root.addEventListener('click', function (e) {
      var fl = e.target.closest && e.target.closest('.rt-fline');
      if (fl) {
        e.stopPropagation();
        var idx = Array.prototype.indexOf.call(p.root.querySelectorAll('.rt-fline'), fl);
        var at = -1;
        if (idx !== p.active) { view.freqs.forEach(function (q, k) { if (q.hz === p.slices[idx].target && k !== view.activeIdx) { at = k; } }); }
        if (isOne()) { toggleIns(at >= 0 ? at : view.activeIdx); return; }
        if (at >= 0) { openPeek(at); draw(); } else { openSheet(view.modelAt(view.activeIdx)); }
      }
    }, true);
    buildPhoneFlag();
    function buildPhoneFlag() {
      var more = document.createElement('div');
      more.className = 'fl-pmenu';
      p.root.appendChild(more);
      var lists = {};
      var rate = 192;
      var openFor = -1;
      var note = document.createElement('div');
      note.className = 'dk__toast dk__toast--p';
      p.root.appendChild(note);
      var noteTimer = null;
      function say(t) { note.textContent = t; note.classList.add('is-on'); clearTimeout(noteTimer); noteTimer = setTimeout(function () { note.classList.remove('is-on'); }, 2400); }
      function choiceOf() { return S.rx === 'Sig Avg' ? 'Average' : S.rx === 'Max Bin' ? 'Max Bin' : 'Peak'; }
      function close() { more.classList.remove('is-open'); openFor = -1; lists = {}; p.slices.forEach(function (s) { var b = s.fbtns.querySelector('[data-more]'); if (b) { b.classList.remove('is-lit'); } }); }
      function row(t, opts) {
        opts = opts || {};
        return '<button type="button" class="fl-pit' + (opts.c ? ' is-c' : '') + (opts.sub ? ' is-sub' : '') + '"' + (opts.off ? ' disabled' : '') + ' data-k="' + opts.k + '">' + esc(t) + '</button>';
      }
      function render(i) {
        var s = p.slices[i];
        var h = '<div class="fl-ptitle">Slice ' + 'ABCD'.charAt(i) + ' \u00b7 More</div>' + row('Make this the TX slice', { k: 'tx' }) + '<div class="fl-psep"></div>';
        h += row('Antenna >', { k: 'ant' });
        if (lists.ant) { ['ANT1', 'ANT2', 'ANT3'].forEach(function (a) { h += row(a, { k: 'a:' + a, sub: 1, c: s.flag.querySelector('.vfo__ant--rx').textContent === a }); }); }
        h += row('Sample rate >', { k: 'rate' });
        if (lists.rate) { [48, 96, 192, 384, 768, 1536].forEach(function (r) { h += row(r + ' kHz', { k: 'r:' + r, sub: 1, c: rate === r }); }); }
        h += row('S-meter: ' + choiceOf() + ' >', { k: 'sm' });
        if (lists.sm) { [['Peak', 'Signal'], ['Average', 'Sig Avg'], ['Max Bin', 'Max Bin']].forEach(function (c) { h += row(c[0], { k: 'm:' + c[1], sub: 1, c: choiceOf() === c[0] }); }); }
        h += '<div class="fl-psep"></div>' + row('Diversity...', { k: 'div' }) + row('Filter policy...', { k: 'fil', off: true }) + '<div class="fl-psep"></div>' + row('Remove slice', { k: 'rm', off: i === 0 });
        more.innerHTML = h;
      }
      more.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      more.addEventListener('click', function (e) {
        e.stopPropagation();
        var b = e.target.closest('[data-k]');
        if (!b || b.disabled) { return; }
        var k = b.getAttribute('data-k');
        var i = openFor;
        var s = p.slices[i];
        if (k === 'ant' || k === 'rate' || k === 'sm') { var was = lists[k]; lists = {}; lists[k] = !was; render(i); return; }
        if (k.indexOf('a:') === 0) { s.flag.querySelector('.vfo__ant--rx').textContent = k.slice(2); close(); return; }
        if (k.indexOf('r:') === 0) { rate = parseInt(k.slice(2), 10); say('Sample rate ' + rate + ' kHz'); close(); return; }
        if (k.indexOf('m:') === 0) { S.rx = k.slice(2); phoneRxChanged(); close(); return; }
        if (k === 'tx') { if (s.txBtns[0]) { s.txBtns[0].click(); } close(); return; }
        if (k === 'div') { say('Opens Diversity, as it does today'); close(); return; }
        if (k === 'rm') { say('Removes this slice, as it does today'); close(); }
      });
      p.root.addEventListener('pointerdown', function (e) { if (more.classList.contains('is-open') && !more.contains(e.target) && !e.target.closest('[data-more]')) { close(); } });
      p.slices.forEach(function (s, i) {
        s.fbtns.removeAttribute('aria-hidden');
        s.fbtns.innerHTML = '<span class="fb' + (i === 0 ? ' is-dim' : '') + '" title="Close slice">\u2715</span>' +
          '<button type="button" class="fb" data-lock aria-label="Lock">' + LOCK_OPEN + '</button>' +
          '<button type="button" class="fb" data-more aria-label="More for slice">\u2022\u2022\u2022</button>';
        var lock = s.fbtns.querySelector('[data-lock]');
        var stop = function (e) { e.stopPropagation(); };
        lock.addEventListener('pointerdown', stop);
        lock.addEventListener('click', function (e) { e.stopPropagation(); var on = lock.classList.toggle('is-lit'); lock.innerHTML = on ? LOCK_SHUT : LOCK_OPEN; });
        var mb = s.fbtns.querySelector('[data-more]');
        mb.addEventListener('pointerdown', stop);
        mb.addEventListener('click', function (e) {
          e.stopPropagation();
          closeActions();
          if (openFor === i) { close(); return; }
          close();
          openFor = i;
          render(i);
          more.classList.add('is-open');
          mb.classList.add('is-lit');
        });
      });
      // The real side buttons are 44 points across, so the column stands off further.
      var origPlace = p.place;
      p.place = function () {
        origPlace.apply(p, arguments);
        p.slices.forEach(function (s) {
          if (s.fbtns.hidden) { return; }
          var fm = /translate\(([-\d.]+)px, ([-\d.]+)px\)/.exec(s.flag.style.transform || '');
          var bm = /translate\(([-\d.]+)px/.exec(s.fbtns.style.transform || '');
          if (!fm || !bm) { return; }
          var fx = parseFloat(fm[1]);
          var onLeft = parseFloat(bm[1]) > fx;
          var bx = clamp(onLeft ? fx + s.flag.offsetWidth + 2 : fx - 46, 0, p.W - 44);
          s.fbtns.style.transform = 'translate(' + bx.toFixed(1) + 'px, ' + fm[2] + 'px)';
        });
      };
      p.place();
      // The level bar with its tag: the reading follows the source.
      var avg = {};
      var pk = {};
      var origMeters = p.drawMeters;
      p.drawMeters = function () {
        origMeters.apply(p, arguments);
        p.slices.forEach(function (s, i) {
          if (s.flag.hidden) { return; }
          var raw = s.s;
          avg[i] = avg[i] == null ? raw : avg[i] + (raw - avg[i]) * 0.06;
          pk[i] = Math.max(raw, (pk[i] == null ? raw : pk[i]) - 0.5);
          var v = S.rx === 'Sig Avg' ? avg[i] : S.rx === 'Max Bin' ? raw - 12 + Math.random() * 2 : raw;
          paintLevel(s.level.getContext('2d'), s.level._lw, s.level._lh, s.level._scale, v, rxTag(S.rx), {
            tick: '#828288', bg: '#050507', border: '#232329', top: 4, tickH: 8, dbmW: 60, tagColor: '#828288',
            tickFont: '8px system-ui, -apple-system, sans-serif', tagFont: '700 8px system-ui, -apple-system, sans-serif', dbmFont: '700 10px system-ui, -apple-system, sans-serif', digits: 1
          });
        });
      };
      var origFace = p.drawSMeters;
      p.drawSMeters = function () {
        origFace.apply(p, arguments);
        if (S.rx === 'Signal') { return; }
        (p.smeters || []).forEach(function (cv) {
          if (cv.offsetParent === null) { return; }
          var c = cv.getContext('2d');
          c.save();
          c.setTransform(cv._scale || 1, 0, 0, cv._scale || 1, 0, 0);
          c.font = '700 10px system-ui, sans-serif';
          c.textAlign = 'center';
          c.fillStyle = '#a6a6ac';
          c.fillText(S.rx, (cv._lw || cv.offsetWidth) / 2, 12);
          c.restore();
        });
      };
      phoneRxHooks.push(function () { p.drawMeters(); });
    }
    function syncKey() {
      var want = S.state === 'you' || S.state === 'youdbl';
      if (view.activeIdx !== 0) { want = false; }
      if (!!p.txLatched !== want) {
        p.txLatched = want;
        p.updateTx();
      }
    }
    function draw() {
      syncKey();
      view.draw(band);
      flagLines();
    }
    redrawers.push(function () {
      if (lastUx !== S.ux + S.look) { lastUx = S.ux + S.look; closePeek(); sheet.classList.remove('is-open'); }
      if (sheet.__peek) { if (S.look === 'today') { closePeek(); } else if (isOne()) { renderInsSheet(); } else { openPeek(view.peekIdx); } }
      else if (sheet.classList.contains('is-open')) { openSheet(view.modelAt(view.activeIdx)); }
      renderPad();
      draw();
    });
    if (isPad) {
      applet = document.createElement('div');
      applet.className = 'rci';
      var oldBox = col.querySelector('.rt-applet');
      oldBox.parentNode.insertBefore(applet, oldBox.nextSibling);
      var widen = col.querySelector('[data-rt-widen]');
      widen.addEventListener('click', function () { setTimeout(function () { renderPad(); last = ''; }, 0); });
      if (kind === 'ipad-wide') { setTimeout(function () { widen.click(); view.peekIdx = 1; renderPad(); }, 80); }
      p.root.querySelectorAll('[data-col-toggle]').forEach(function (b) {
        b.addEventListener('click', function () {
          setTimeout(function () {
            if (columnShowing()) { sheet.classList.remove('is-open'); sheet.__peek = false; }
            renderPad();
          }, 0);
        });
      });
      renderPad();
    }
    // The real pan moves under the card; follow it.
    var last = '';
    setInterval(function () {
      var fx = p.slices[p.active].f;
      var flags = JSON.stringify(band.flags().map(function (q) { return Math.round(q.left) + ',' + Math.round(q.bottom); }));
      var key = p.W + '|' + fx + '|' + flags;
      if (key !== last) { last = key; draw(); }
      flagLines();
    }, 200);
    // Voices on this band.
    var orig = p.sim.row;
    p.sim.row = function (t) {
      var lin = orig(t);
      view.freqs.forEach(function (e, idx) {
        var keyed = view.keyedAt(idx);
        var base = idx === 0 ? -74 : idx === 1 ? -82 : -92;
        keyed.forEach(function (name, n) {
          p.sim.addVoice(e.hz - n * 30, base + voiceLevel(t, name.charCodeAt(0) + n * 2.3 + idx));
        });
      });
      return lin;
    };
    setTimeout(draw, 120);
    var padTicks = 0;
    view.tickLive = function () {
      view.refreshLive();
      if (sheet.__peek && view.peekIdx != null) { refreshCard(sheet, peekOf(view.peekIdx)); }
      if (sheet.__peek && view.insMine) { refreshCard(sheet, mineData(view.modelAt(view.activeIdx))); }
      if (applet) {
        if (S.look === 'today') { if (++padTicks % 4 === 0) { renderPad(); } }
        else { refreshCard(applet, padView()); }
      }
    };
  }

  // ---------- The web page ----------
  // The real page's stylesheet and markup, filled in here with the page's own class names and wording
  // (roundtable.js renderDirectory, renderMap, renderRoom). Today's build stays exactly that; the proposal adds to it.
  var WEB_BANDS = [['160m', 1800000, 2000000], ['80m', 3500000, 4000000], ['60m', 5330000, 5410000], ['40m', 7000000, 7300000], ['30m', 10100000, 10150000], ['20m', 14000000, 14350000], ['17m', 18068000, 18168000], ['15m', 21000000, 21450000], ['12m', 24890000, 24990000], ['10m', 28000000, 29700000], ['6m', 50000000, 54000000]];
  function webBandOf(hz) { var f = WEB_BANDS.filter(function (b) { return hz >= b[1] && hz <= b[2]; })[0]; return f ? f[0] : 'other'; }
  function webBandLong(b) { return b.replace('m', ' m'); }
  function webGrid(c) { var h = hashCall(c); return String.fromCharCode(69 + h % 8) + String.fromCharCode(73 + (h >> 2) % 8) + (h % 10) + ((h >> 3) % 10); }
  function webDbm(d) { return d.toFixed(1); }
  function webSignal(d, fmt, one) {
    var v = sText(d);
    return fmt === 'S' ? v : fmt === 'both' ? v + (one ? ' ' : '\n') + webDbm(d) : webDbm(d);
  }
  function webFq(hz) {
    var s = String(hz).padStart(7, '0');
    var mhz = s.slice(0, -6); var khz = s.slice(-6, -3); var r = s.slice(-3);
    return mhz + '.' + khz + '<i>.' + r + '</i>';
  }
  function webHz(hz) { return (hz / 1e6).toFixed(6); }
  var webView = new View({
    freqs: [
      { hz: 14212000 },
      { hz: 14205000, keyed: ['AA5ABC', 'K0ABC'], calls: ['AA5ABC', 'K0ABC', 'W2ABC', 'N7ABC'] },
      { hz: 14230000, keyed: ['W6ABC'], calls: ['W6ABC', 'KC2ABC', 'N3ABC'] },
      { hz: 18130000, keyed: [], calls: ['G0ABC', 'N5ABC'] },
      { hz: 7185000, keyed: ['VE3ABC'], calls: ['VE3ABC', 'K9ABC'] },
      { hz: 7196000, keyed: [], calls: ['F4ABC'] },
      { hz: 3790000, keyed: [], calls: ['W4ABC', 'N2ABC'] },
      { hz: 21300000, keyed: ['JA1ABC', 'VK3ABC'], calls: ['JA1ABC', 'VK3ABC', 'ZL2ABC'] }
    ]
  });
  views.push(webView);
  function webData(i) {
    var d = i === 0 ? mineData(webView.modelAt(0)) : peekData(webView, i, webView.freqs[i].hz, null);
    if (S.link === 'hidden') {
      d.people = d.people.filter(function (p) { return p.c !== YOU; });
      d.reports = d.reports.filter(function (r) { return r.c !== YOU; });
    }
    return d;
  }
  function WebFrame(hostId, width, height, startInRoom) {
    var host = document.getElementById(hostId);
    var frame = document.createElement('iframe');
    frame.title = 'Roundtable web page';
    frame.width = width; frame.height = height;
    frame.srcdoc = '<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Roundtable</title><style>' + WEB.css + '</style><style>' + WEB.extra + '</style></head><body class="public rw" data-page="public">' + WEB.body + '</body></html>';
    host.appendChild(frame);
    var fit = function () {
      if (width < 800) { return; }
      var k = Math.min(1, host.clientWidth / width);
      frame.style.transform = 'scale(' + k + ')';
      host.style.height = Math.round(height * k) + 'px';
    };
    window.addEventListener('resize', fit);
    fit();
    var W = { band: null, q: '', quiet: true, sort: 'f', sel: 0, family: 'usb', unit: 'mi', bearing: true, earlier: false, muted: {}, seq: 0 };
    var doc;
    var $ = function (id) { return doc.getElementById(id); };
    var isNew = function () { return S.look !== 'today'; };
    function toast(t) {
      var b = $('toast');
      b.textContent = t;
      b.classList.remove('hidden');
      clearTimeout(toast.t);
      toast.t = setTimeout(function () { b.classList.add('hidden'); }, 1800);
    }
    function groups() {
      var out = [];
      webView.freqs.forEach(function (e, i) {
        var d = webData(i);
        var band = webBandOf(e.hz);
        var st = d.people.map(function (p) { return { c: p.c, keyed: p.keyed, grid: webGrid(p.c) }; });
        out.push({ i: i, hz: e.hz, band: band, d: d, stations: st });
      });
      return out;
    }
    function shownGroups() {
      var q = W.q.toUpperCase();
      var all = groups();
      var list = all.filter(function (g) {
        if (W.band && g.band !== W.band) { return false; }
        g.show = g.stations.filter(function (s) {
          if (!W.quiet && !s.keyed) { return false; }
          if (q && !/^[0-9.]+$/.test(q) && s.c.indexOf(q) < 0) { return false; }
          return true;
        });
        return g.show.length > 0;
      });
      list.sort(W.sort === 'n' ? function (a, b) { return b.stations.length - a.stations.length; } :
        W.sort === 't' ? function (a, b) { return (b.d.live ? 1 : 0) - (a.d.live ? 1 : 0) || a.hz - b.hz; } :
          function (a, b) { return a.hz - b.hz; });
      return list;
    }
    function renderBands() {
      var all = groups();
      var count = function (g) { return g.stations.length; };
      var total = all.reduce(function (n, g) { return n + count(g); }, 0);
      var h = '<button type="button" class="band all' + (!W.band ? ' on' : '') + '" data-band="" aria-pressed="' + !W.band + '" aria-label="All bands, ' + total + ' station slices"><div class="l"><span class="n">All</span><span class="c">' + total + '</span></div><div class="act"></div></button>';
      WEB_BANDS.forEach(function (b) {
        var n = all.filter(function (g) { return g.band === b[0]; }).reduce(function (x, g) { return x + count(g); }, 0);
        h += '<button type="button" class="band' + (W.band === b[0] ? ' on' : '') + '" data-band="' + b[0] + '"' + (n ? '' : ' disabled') + ' aria-pressed="' + (W.band === b[0]) + '" aria-label="' + webBandLong(b[0]) + ', ' + n + ' station slices"><div class="l"><span class="n">' + b[0].replace('m', '') + '</span><span class="c">' + n + '</span></div><div class="act"></div></button>';
      });
      $('band-buttons').innerHTML = h;
      var quiet = all.reduce(function (n, g) { return n + g.stations.filter(function (s) { return !s.keyed; }).length; }, 0);
      $('quiet-count').textContent = quiet;
      $('quiet-chip').classList.toggle('on', W.quiet);
      $('quiet-chip').setAttribute('aria-pressed', String(W.quiet));
      $('sort').value = W.sort;
    }
    function renderList() {
      var keep = $('rows').scrollTop;
      var list = shownGroups();
      var h = '';
      var band = null;
      var live = S.link === 'live' || S.link === 'hidden';
      list.forEach(function (g) {
        if (g.band !== band) { band = g.band; h += '<div class="grp"><b class="bn">' + webBandLong(band) + '</b><span class="gc">People at their reported frequencies</span></div>'; }
        var dbl = isNew() && g.d.kind === 'dbl' && live;
        var unk = isNew() && g.d.kind === 'unk' && live;
        h += '<section class="frequency-group' + (g.i === W.sel ? ' selected' : '') + (dbl ? ' rw-dbl' : '') + '" data-i="' + g.i + '"><button class="frequency-label" type="button"><span class="fq">' + webFq(g.hz) + '</span><span class="md">USB</span>' +
          (dbl ? '<span class="rw-tag dbl"><span class="gl doubled"></span>Double</span>' : '') + (unk ? '<span class="rw-tag unk"><span class="gl unknown"></span>Unknown signal</span>' : '') +
          (g.stations.length > 1 ? '<span class="tag cochannel-label">' + g.stations.length + ' cochannel</span>' : '') + '</button>';
        g.show.forEach(function (s) {
          var kd = s.keyed && live;
          h += '<button class="person-row row' + (kd ? ' keyed' : '') + (kd && dbl ? ' rw-d' : '') + '" type="button" data-i="' + g.i + '" aria-pressed="' + (g.i === W.sel) + '" aria-label="Browse ' + s.c + ' at ' + webHz(g.hz) + ' MHz"><span class="gl ' + (kd ? (dbl ? 'doubled' : 'talking') : 'quiet') + '" aria-hidden="true"></span><span class="person-call">' + esc(s.c) + '</span><span class="person-grid">' + s.grid + '</span><span class="person-state">' + (kd ? 'Keyed' : 'Listening') + '</span><span class="browse-word">Browse ›</span></button>';
        });
        h += '</section>';
      });
      if (!list.length) {
        h = '<div class="empty-list">' + (W.q ? 'No current station matches “' + esc(W.q) + '”. Search a frequency to browse its conversation.' : 'Nobody is publishing a station here right now.') + '</div>';
      }
      $('rows').innerHTML = h;
      $('rows').scrollTop = keep;
      $('shown').textContent = list.reduce(function (n, g) { return n + g.show.length; }, 0) + ' station positions · ' + list.length + ' frequency groups';
    }
    function renderMap() {
      var box = $('bmap');
      var name = W.band || webBandOf(webView.freqs[W.sel].hz);
      var band = WEB_BANDS.filter(function (b) { return b[0] === name; })[0];
      if (!band) { box.classList.add('hidden'); box.innerHTML = ''; return; }
      box.classList.remove('hidden');
      var width = Math.max(240, box.clientWidth - 40);
      var lo = band[1]; var hi = band[2];
      var gs = groups().filter(function (g) { return g.band === name; }).sort(function (a, b) { return a.hz - b.hz; });
      var tagW = Math.min(150, (width - Math.max(0, gs.length - 1) * 6) / Math.max(1, gs.length));
      var right = 0;
      gs.forEach(function (g) { g.anchor = (g.hz - lo) / (hi - lo) * width; g.left = Math.max(right, Math.min(width - tagW, g.anchor - tagW / 2)); right = g.left + tagW + 6; });
      if (right > width + 6) { var edge = width; for (var n = gs.length - 1; n >= 0; n--) { gs[n].left = Math.min(gs[n].left, edge - tagW); edge = gs[n].left - 6; } }
      var h = '<div class="people-map"><div class="map-caption">' + webBandLong(name) + ' · exact frequency ticks · Browse leaves your radio unchanged</div><div class="axis">';
      for (var i = 0; i <= 4; i++) { h += '<span style="left:' + (i * 25) + '%">' + ((lo + (hi - lo) * i / 4) / 1e6).toFixed(3) + '</span>'; }
      h += '</div>';
      gs.forEach(function (g) { h += '<span class="frequency-tick" style="left:' + g.anchor + 'px"></span>'; });
      gs.forEach(function (g) {
        var sel = g.i === W.sel;
        var dbl = isNew() && g.d.kind === 'dbl';
        var calls = g.stations.map(function (s) { return s.c; });
        var inner;
        var t = '<small>' + webHz(g.hz) + ' · USB</small>';
        if (dbl) {
          inner = t + '<b class="rw-dw">Double</b>' + g.d.keyed.slice(0, 2).map(function (c) { return '<b>' + esc(c) + '</b>'; }).join('');
        } else if (sel && calls.length > 1 && g.d.kind !== 'quiet') {
          var cap = g.d.kind === 'dbl' ? 'DOUBLE · ' + g.d.keyed.join(' + ') : g.d.kind === 'unk' ? 'CURRENT SIGNAL · Unknown signal' : g.d.kind === 'last' ? (g.d.wasDouble ? 'LAST OVERLAP · ' + g.d.talkers.join(' + ') : 'LAST HEARD · ' + g.d.talkers[0]) : 'CURRENT TALKER · ' + g.d.keyed[0];
          inner = t + '<b>' + esc(cap) + '</b><small>' + (g.d.kind === 'dbl' ? 'Combined RF · ' : 'Heard by · ') + (S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm') + ' / age</small>' +
            g.d.reports.slice(0, 2).map(function (r) { return '<small class="stack-report">' + esc(r.c) + ' · ' + webSignal(r.d, S.fmt, true).replace(' ', ' / ') + ' / ' + Math.floor(r.a) + 's</small>'; }).join('');
        } else {
          inner = t + calls.slice(0, 3).map(function (c) { return '<b>' + esc(c) + '</b>'; }).join('') + (calls.length > 3 ? '<small>+' + (calls.length - 3) + ' · Browse</small>' : '');
        }
        h += '<button class="maptag' + (sel ? ' selected' : '') + (calls.length > 1 ? ' cochannel' : '') + (dbl ? ' rw-dbl' : '') + '" type="button" data-i="' + g.i + '" style="left:' + g.left + 'px;width:' + tagW + 'px">' + inner + '</button>';
      });
      h += '</div>';
      box.innerHTML = h;
    }
    function pathHtml(r, known) {
      if (!known || !r.mi) { return '<span class="path-value"></span>'; }
      var dist = W.unit === 'km' ? Math.round(r.mi * 1.609344) : r.mi;
      return '<span class="path-value">' + dist.toLocaleString() + ' ' + W.unit + (W.bearing ? '<small>' + String(r.br).padStart(3, '0') + '°T</small>' : '') + '</span>';
    }
    function rowHtml2(r, d) {
      var known = d.kind === 'talk' || d.kind === 'you' || (d.kind === 'last' && !d.wasDouble);
      var off = OFFSETS[hashCall(r.c) % 6] * 11;
      var stale = r.a > 20;
      var p = isNew();
      return '<div class="rd attributed-reading' + (stale ? ' stale' : '') + (p ? ' rw-m' : '') + '" data-c="' + esc(r.c) + '"><span class="c">' + esc(r.c) + '</span>' + pathHtml(r, known) +
        '<span class="o">' + (off > 0 ? '+' : '') + off + '</span><button class="v signal-value" type="button" data-signal aria-label="' + esc(r.c) + ' signal. Change signal format">' + webSignal(r.d, S.fmt, p) + '</button><span class="age">' + Math.floor(r.a) + ' s</span>' +
        (p ? '<span class="rw-meter"><i style="width:' + (frac(r.d) * 100).toFixed(1) + '%"></i>' + (d.live && !stale ? '<u style="left:' + (frac(r.pk) * 100).toFixed(1) + '%"></u>' : '') + '</span>' : '') + '</div>';
    }
    function roomHtml() {
      var i = W.sel;
      var d = webData(i);
      var hz = webView.freqs[i].hz;
      var p = isNew();
      var family = W.family;
      var labels = { lsb: 'LSB', usb: 'USB', digl: 'DIGL', digu: 'DIGU', am: 'AM', fm: 'FM', cw: 'CW', fdvl: 'FreeDV L', fdvu: 'FreeDV U' };
      var conn = S.link === 'recon' ? ['reconnecting', 'Reconnecting · nothing new for 8 s'] : S.link === 'offline' ? ['offline', 'Offline since 14:32 UTC'] : ['live', 'Live · browsing · radio unchanged'];
      var h = '<button class="ph-back" type="button">‹ All people</button><div class="rhead"><div class="l1"><span class="f">' + webFq(hz) + '</span><select class="tag family-select" id="browse-family" aria-label="Browse mode family">' +
        Object.keys(labels).map(function (k) { return '<option value="' + k + '"' + (k === family ? ' selected' : '') + '>' + labels[k] + '</option>'; }).join('') +
        '</select><button class="btn small lnk" id="copy-link" type="button">Copy link</button></div><div id="conn" class="conn ' + conn[0] + '">' + conn[1] + '</div>' +
        '<p class="frequency-range">±5 kHz · ' + webHz(hz - 5000) + '–' + webHz(hz + 5000) + ' MHz · ' + labels[family] + '</p>' +
        '<div class="path-controls"><label>Distance <select id="distance-unit" aria-label="Distance units"><option value="mi"' + (W.unit === 'mi' ? ' selected' : '') + '>Miles (mi)</option><option value="km"' + (W.unit === 'km' ? ' selected' : '') + '>Kilometres (km)</option></select></label><label><input type="checkbox" id="show-bearing"' + (W.bearing ? ' checked' : '') + '> True bearing (°T)</label><label>Signal <select id="signal-format" aria-label="Signal format"><option value="dbm"' + (S.fmt === 'dbm' ? ' selected' : '') + '>dBm</option><option value="S"' + (S.fmt === 'S' ? ' selected' : '') + '>S-units</option><option value="both"' + (S.fmt === 'both' ? ' selected' : '') + '>Both</option></select></label></div></div>';
      h += '<div class="rbody" tabindex="-1" aria-label="Frequency activity and conversation">';
      var offMode = family !== 'usb';
      var kindClass = d.kind === 'dbl' || d.wasDouble ? 'doubled' : d.kind === 'unk' ? 'unknown' : d.kind === 'quiet' ? 'quiet' : 'talking';
      var cap; var when; var name;
      if (offMode || d.kind === 'quiet') {
        cap = 'ACTIVITY'; when = 'at this viewed frequency'; name = 'No attributed turn';
      } else if (d.kind === 'last') {
        cap = d.wasDouble ? 'LAST OVERLAP' : 'LAST HEARD'; when = 'turn ended ' + age(d.ago) + ' ago'; name = d.talkers.join(' + ');
      } else {
        cap = d.kind === 'dbl' ? 'DOUBLE' : d.kind === 'unk' ? 'CURRENT SIGNAL' : 'CURRENT TALKER';
        when = 'on the air' + (p ? ' <span class="rw-clock" data-clock>' + mmss(d.clock) + '</span>' : '');
        name = d.kind === 'dbl' ? d.keyed.join(' + ') : d.kind === 'unk' ? 'Unknown signal' : d.keyed[0];
      }
      if (p && !offMode) { h += '<div class="rw-rail ' + (d.kind === 'dbl' ? 'dbl' : d.kind === 'unk' ? 'unk' : d.kind === 'quiet' || d.kind === 'last' ? 'quiet' : 'talk') + '"></div>'; }
      h += '<div class="cap">' + cap + '<span>' + when + '</span></div><div class="attributed-talker ' + (offMode ? 'quiet' : kindClass) + '"><span class="gl ' + (offMode ? 'quiet' : kindClass) + '" aria-hidden="true"></span><strong>' + esc(name) + '</strong>' + (offMode || d.kind === 'quiet' ? '' : '<span class="mono">' + webHz(hz) + ' MHz</span>') + '</div>';
      if (offMode || d.kind === 'quiet') {
        h += '<div class="none">No attributed turn near this frequency. Presence does not identify an unknown RF signal.</div>';
      } else {
        if (d.kind === 'unk') { h += '<div class="names">No names suggested.</div><button class="btn small name-turn" type="button" disabled>Name unknown turn</button>'; }
        var combined = d.kind === 'dbl' || d.wasDouble;
        if (combined) { h += '<div class="fn">Combined RF is not credited to any individual talker or archive. Distance and bearing are blank.</div>'; }
      }
      if (p && !offMode) {
        var shownPeople = d.people;
        h += '<div class="rw-onfreq"><div class="cap">ON FREQUENCY<span>' + shownPeople.length + ' station' + (shownPeople.length === 1 ? '' : 's') + '</span></div><div class="rw-people">' + shownPeople.map(function (q) {
          var on = q.keyed && (S.link === 'live' || S.link === 'hidden');
          var dd = on && d.kind === 'dbl';
          var off = q.off * 11;
          return '<div class="rw-p' + (on ? ' on' : '') + (dd ? ' d' : '') + '"><span class="gl ' + (on ? (dd ? 'doubled' : 'talking') : 'quiet') + '"></span><span class="c">' + esc(q.c) + '</span><span class="k">' + (on ? 'keyed' : '') + '</span><span class="o">' + (off ? (off > 0 ? '+' : '') + off + ' Hz' : 'on freq') + '</span></div>';
        }).join('') + '</div></div>';
      }
      if (!offMode && d.kind !== 'quiet') {
        var unit = W.unit + (W.bearing ? ' / °T' : '');
        var fmtName = S.fmt === 'S' ? 'S-units' : S.fmt === 'both' ? 'Both' : 'dBm';
        h += '<div class="rw-k-' + (d.kind === 'dbl' || d.wasDouble ? 'dbl' : d.kind === 'unk' ? 'unk' : d.kind === 'last' ? 'last' : 'talk') + '" data-readings><div class="rd reading-head' + (p ? ' rw-mh' : '') + '"><span>' + (p && (d.kind === 'dbl' || d.wasDouble) ? 'Combined RF · heard by' : 'Heard by') + '</span><span>' + unit + '</span><span class="o">Offset Hz</span><button class="signal-heading" id="signal-heading" type="button" data-signal aria-label="Signal format: ' + fmtName + '. Change signal format">Signal · ' + fmtName + '</button><span>Age</span></div>';
        h += d.reports.length ? d.reports.map(function (r) { return rowHtml2(r, d); }).join('') : '<div class="none">No current reporter reading belongs to this turn.</div>';
        h += '</div>';
        h += '<div class="fn">' + (d.kind === 'dbl' || d.wasDouble ? 'These are raw reports of the combined RF, not of an individual transmitter.' : d.kind === 'unk' ? 'Each row is that reporter’s raw reading of Unknown signal. Nearest first. Readings keep their original age after the turn ends.' : 'Each row is that reporter’s raw reading of ' + esc(name) + '. Nearest first; distance and initial true bearing run from talker to reporter. Readings keep their original age after the turn ends.') + '</div>';
      }
      h += '<details class="earlier-turns"' + (W.earlierOpen ? ' open' : '') + '><summary class="cap">EARLIER TURNS<span>tap to open</span></summary>' +
        '<div class="turn" tabindex="0"><span class="t">13:49</span><span class="gl talking"></span><span><b class="c">W9ABC</b><small> · ' + webHz(hz) + ' MHz</small></span><span class="d">2:40</span></div>' +
        '<div class="turn" tabindex="0"><span class="t">13:46</span><span class="gl doubled"></span><span><b class="c doubled">K4ABC + N4XYZ</b><small> · ' + webHz(hz) + ' MHz</small></span><span class="d">0:09</span></div></details>';
      h += '<div class="conversation-tabs"><button class="chip' + (!W.earlier ? ' on' : '') + '" id="current-conversation" type="button" aria-pressed="' + !W.earlier + '">Current conversation</button><button class="chip' + (W.earlier ? ' on' : '') + '" id="earlier-conversation" type="button" aria-pressed="' + W.earlier + '">Earlier browsing</button></div>';
      h += '<div class="cap">' + (W.earlier ? 'RETAINED IN THIS TAB' : 'NEARBY CONVERSATION') + '<span>' + (W.earlier ? 'original frequency and time · checked 14:31 UTC' : 'same family · inclusive 5,000 Hz') + '</span></div>';
      if (W.earlier) {
        h += '<div class="none">Messages you read remain here when you browse another frequency.</div>';
      } else {
        [['N0SPM', 'S8', 140, 'Good signal into Omaha.'], ['N4RDR', 'S9+6', 190, 'Good copy here, 59 in EM85.'], ['VE3XYZ', '', -10, 'Thanks for the QSO, 73']].forEach(function (m) {
          var mute = W.muted[m[0]];
          h += '<div class="msg"><div class="h"><span class="c">' + m[0] + '</span>' + (m[1] ? '<span class="hr">reading ' + m[1] + '</span>' : '') + '<span class="t">' + webHz(hz + m[2]) + ' MHz · 14:30 UTC</span></div><div class="x">' + (mute ? 'Messages from ' + m[0] + ' are muted in this browser.' : esc(m[3])) + '</div><div class="message-actions"><button class="linkbtn" type="button" data-mute="' + m[0] + '">' + (mute ? 'Unmute' : 'Mute') + '</button><button class="linkbtn" type="button" data-copy>Copy</button></div></div>';
        });
      }
      h += '</div><form class="frequency-composer"><div class="audience">Public visitor · read only. Sign in to follow your station.</div><div class="compose-row"><textarea rows="2" placeholder="Messages on this frequency" disabled aria-label="Message on ' + webHz(hz) + ' MHz"></textarea><button class="btn glass" type="submit" disabled>Send</button></div><div class="counter"></div><div class="send-error" role="status"></div></form>';
      return h;
    }
    function renderLink() {
      var st = S.link === 'recon' ? 'reconnecting' : S.link === 'offline' ? 'offline' : 'live';
      doc.querySelector('.page').classList.toggle('roundtable-offline', st !== 'live');
      $('live').className = 'live ' + st;
      $('live-state').textContent = st.toUpperCase();
      var total = groups().reduce(function (n, g) { return n + g.stations.length; }, 0);
      $('live-k').textContent = st === 'live' ? total + ' station slices · people on frequency' : st === 'reconnecting' ? 'nothing new for 8 s' : 'since 14:32 UTC';
      $('bands').classList.toggle('offline', st === 'offline');
      $('bands').classList.remove('connecting');
    }
    function renderRoom() {
      var old = doc.querySelector('.rbody');
      var keep = old ? old.scrollTop : 0;
      $('room').innerHTML = roomHtml();
      var b = doc.querySelector('.rbody');
      if (b) { b.scrollTop = keep; }
    }
    function render() {
      doc.body.classList.toggle('rw', isNew());
      renderBands(); renderList(); renderMap(); renderRoom(); renderLink();
    }
    function select(i) {
      var changed = W.sel !== i;
      W.sel = i;
      if (changed) { W.family = 'usb'; W.earlier = false; var b = doc.querySelector('.rbody'); if (b) { b.scrollTop = 0; } }
      if (win().matchMedia('(max-width:760px)').matches) { doc.body.classList.add('in-room'); }
      render();
    }
    function win() { return frame.contentWindow; }
    function tick() {
      if (!doc) { return; }
      W.seq++;
      var p = isNew();
      if (!p && W.seq % 4 !== 0) { return; }
      if (doc.activeElement && doc.activeElement.tagName === 'SELECT') { return; }
      if (W.family !== 'usb') { return; }
      var d = webData(W.sel);
      var frozen = S.link === 'recon' || S.link === 'offline';
      var gone = (Date.now() - S.t0) / 1000;
      d.reports.forEach(function (r) {
        var row = doc.querySelector('.attributed-reading[data-c="' + r.c + '"]');
        if (!row) { return; }
        if (!frozen) {
          row.querySelector('.v').textContent = webSignal(r.d, S.fmt, p);
          var i = row.querySelector('.rw-meter i');
          if (i) { i.style.width = (frac(r.d) * 100).toFixed(1) + '%'; }
          var u = row.querySelector('.rw-meter u');
          if (u) { u.style.left = (frac(r.pk) * 100).toFixed(1) + '%'; }
        }
        row.querySelector('.age').textContent = Math.floor(frozen ? (row.__a0 == null ? (row.__a0 = r.a) : row.__a0) + gone : r.a) + ' s';
        row.classList.toggle('stale', (frozen ? row.__a0 + gone : r.a) > 20);
      });
      var c = doc.querySelector('[data-clock]');
      if (c && !frozen) { c.textContent = mmss(d.clock); }
    }
    function onClick(e) {
      var t = e.target;
      var a = t.closest('a');
      if (a) { e.preventDefault(); toast('Opens ' + a.textContent.trim() + ' on the real site.'); return; }
      var b = t.closest('[data-band]');
      if (b) {
        W.band = b.getAttribute('data-band') || null;
        var sel = groups();
        if (W.band && sel[W.sel].band !== W.band) { W.sel = sel.filter(function (g) { return g.band === W.band; }).sort(function (x, y) { return x.hz - y.hz; })[0].i; }
        render();
        return;
      }
      if (t.closest('[data-signal]')) {
        S.fmt = S.fmt === 'dbm' ? 'S' : S.fmt === 'S' ? 'both' : 'dbm';
        redrawAll();
        return;
      }
      var row = t.closest('.person-row, .frequency-label, .maptag');
      if (row) {
        var sec = row.getAttribute('data-i') != null ? row : row.closest('[data-i]');
        select(parseInt(sec.getAttribute('data-i'), 10));
        return;
      }
      if (t.closest('.ph-back')) { doc.body.classList.remove('in-room'); return; }
      if (t.closest('#quiet-chip')) { W.quiet = !W.quiet; render(); return; }
      if (t.closest('#copy-link')) { toast('https://roundtable.nereussdr.com/r/' + W.family + '/' + webView.freqs[W.sel].hz); return; }
      if (t.closest('#current-conversation')) { W.earlier = false; renderRoom(); return; }
      if (t.closest('#earlier-conversation')) { W.earlier = true; renderRoom(); return; }
      var mute = t.closest('[data-mute]');
      if (mute) { var c = mute.getAttribute('data-mute'); W.muted[c] = !W.muted[c]; renderRoom(); return; }
      if (t.closest('[data-copy]')) { toast('Message copied.'); return; }
    }
    function onChange(e) {
      var id = e.target.id;
      if (id === 'sort') { W.sort = e.target.value; render(); }
      if (id === 'browse-family') { W.family = e.target.value; renderRoom(); }
      if (id === 'distance-unit') { W.unit = e.target.value; renderRoom(); }
      if (id === 'show-bearing') { W.bearing = e.target.checked; renderRoom(); }
      if (id === 'signal-format') { S.fmt = e.target.value; redrawAll(); }
    }
    function onKey(e) {
      var act = doc.activeElement;
      var input = act && /INPUT|TEXTAREA|SELECT/.test(act.tagName);
      if (e.key === '/' && !input) { e.preventDefault(); $('q').focus(); $('q').select(); return; }
      if (e.key === 'Escape') {
        if (doc.body.classList.contains('in-room')) { doc.body.classList.remove('in-room'); return; }
        if (act === $('q')) { $('q').value = ''; W.q = ''; render(); act.blur(); }
        return;
      }
      if ((e.key === 'ArrowDown' || e.key === 'ArrowUp') && (!input || act === $('q'))) {
        e.preventDefault();
        var list = shownGroups();
        if (!list.length) { return; }
        var at = list.map(function (g) { return g.i; }).indexOf(W.sel);
        var next = list[(at + (e.key === 'ArrowDown' ? 1 : -1) + list.length) % list.length];
        select(next.i);
        return;
      }
      if (e.key === 'Enter' && !input && win().matchMedia('(max-width:760px)').matches) { doc.body.classList.add('in-room'); }
    }
    frame.addEventListener('load', function () {
      doc = frame.contentDocument;
      doc.addEventListener('click', onClick);
      doc.addEventListener('change', onChange);
      doc.addEventListener('keydown', onKey);
      doc.addEventListener('toggle', function (e) { if (e.target.classList && e.target.classList.contains('earlier-turns')) { W.earlierOpen = e.target.open; } }, true);
      doc.getElementById('owner-bar').innerHTML = '<span>Public frequency browsing · no radio controls</span><a class="btn small" href="/key">Sign in</a>';
      $('q').addEventListener('input', function () {
        W.q = $('q').value.trim();
        if (/^[0-9.]+$/.test(W.q)) {
          var hz = parseFloat(W.q) * 1e6;
          var near = groups().sort(function (a, b) { return Math.abs(a.hz - hz) - Math.abs(b.hz - hz); })[0];
          if (near) { W.sel = near.i; }
        }
        render();
      });
      win().addEventListener('resize', renderMap);
      if (startInRoom) { doc.body.classList.add('in-room'); }
      render();
    });
    redrawers.push(function () { if (doc) { render(); } });
    this.tick = tick;
  }
  var WEB = window.RC_WEB;
  function buildWeb() {
    var frames = [new WebFrame('web-wide', 1500, 1000, false), new WebFrame('web-phone', 390, 780, true)];
    webView.tickLive = function () { frames.forEach(function (f) { f.tick(); }); };
  }


  // ---------- The filmstrip and the state sheet ----------
  var liveCells = [];
  function addLive(host, state, o) {
    var el = document.createElement('div');
    host.appendChild(el);
    liveCells.push({ el: el, state: state, o: o });
  }
  function renderCell(c) {
    var m = applyLink(model(c.state, 7185000, { count: c.o.count || S.count, loop: true }));
    if (S.look === 'today') {
      c.el.className = '';
      c.el.style.width = '';
      c.el.innerHTML = c.o.w >= 200 ? todayDesktop(m) : todayPhone(m, c.o.w);
      c.isToday = true;
      return;
    }
    if (m.kind === 'quiet') {
      var t = { keyed: [], calls: m.people };
      c.el.className = '';
      c.el.style.width = '';
      c.el.innerHTML = tagHtml(t);
      var child = c.el.firstChild;
      child.style.position = 'relative';
      return;
    }
    mountCard(c.el, m, { w: c.o.w });
    c.model = m;
  }
  function buildSheets() {
    var rcf = document.getElementById('rcf');
    [['talk', 'K4ABC talking'], ['double', 'N4XYZ keys over'], ['double3', 'W9ABC joins'], ['double', 'W9ABC drops'], ['after', 'N4XYZ drops'], ['ended', 'K4ABC stops'], ['talk', 'K4ABC again']].forEach(function (x) {
      var cell = document.createElement('div');
      cell.className = 'rcf__cell';
      cell.innerHTML = '<div class="rcf__t">' + esc(x[1]) + '</div>';
      var bg = document.createElement('div');
      bg.className = 'rcs__bg';
      cell.appendChild(bg);
      rcf.appendChild(cell);
      addLive(bg, x[0], { w: 186, count: 4 });
    });
    var rcs = document.getElementById('rcs');
    STATES.forEach(function (x) {
      var cell = document.createElement('div');
      cell.className = 'rcs__cell';
      cell.innerHTML = '<div class="rcs__t">' + esc(x.t) + '</div><div class="rcs__s">' + esc(x.s) + '</div><div class="rcs__row"></div>';
      rcs.appendChild(cell);
      var row = cell.querySelector('.rcs__row');
      [[232, 'Desktop'], [176, 'Phone'], [104, 'Phone, beside the flag']].forEach(function (w) {
        var col = document.createElement('div');
        col.innerHTML = '<div class="rcs__lab">' + w[1] + '</div>';
        var bg = document.createElement('div');
        bg.className = 'rcs__bg';
        col.appendChild(bg);
        row.appendChild(col);
        addLive(bg, x.id, { w: w[0], count: w[0] < 150 ? 3 : 4 });
      });
    });
    redrawers.push(function () { liveCells.forEach(renderCell); });
    liveCells.forEach(renderCell);
  }

  // ---------- Go ----------
  buildBar();
  buildDesktop();
  N.phones.forEach(function (p) { if (p.root.parentElement.hasAttribute('data-rc-phone')) { buildPhone(p); } });
  buildWeb();
  buildSheets();
  pressAll();
  status();

  setInterval(function () {
    wander();
    views.forEach(function (v) { if (v.tickLive) { v.tickLive(); } });
    liveCells.forEach(function (c) { if (c.model && S.look !== 'today') { refreshCard(c.el, applyLink(model(c.state, 7185000, { count: c.o.count, loop: true }))); } });
  }, 250);
  // A state that adds rows as it goes ('after') redraws its card.
  setInterval(function () {
    if (S.state === 'after' && (Date.now() - S.t0) < 9000) {
      redrawers.forEach(function (fn) { fn(); });
    } else {
      liveCells.forEach(function (c) { if (c.state === 'after') { renderCell(c); } });
    }
  }, 900);
})();
