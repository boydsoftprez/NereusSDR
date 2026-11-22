// Desktop-Layout der App (ab ca. 900 px Breite), 2026-11-21.
//
// Martins Blatt: docs/design/handfunke-entwuerfe/2026-11-21-desktop. Unter
// 900 px bleibt „Ein Schirm" unveraendert. Hier wird NICHTS neu gebaut, nur
// umgesetzt: dieselben Elemente (mit denselben Kennungen und Ereignishoerern)
// wandern in ein Raster und kehren bei schmaler Breite an ihren Platz zurueck.
// Neu sind nur die Darstellungen: SWR-Zeiger, Log-Spiegel, Schrittwahl.
// Datenstrom, Station, Server: unveraendert.

const BREITE = '(min-width: 900px)';

export function desktopStarten({ link, $, feinSetzen, logHolen }) {
  const mq = window.matchMedia(BREITE);
  const platz = new Map();           // Element -> Platzhalter (Rueckweg)
  let gitter = null, swrNadel = null, swrWert = null, logZeilen = null, fbText = null, schrittAn = 100;

  const mk = (html) => {
    const d = document.createElement('div');
    d.innerHTML = html.trim();
    return d.firstElementChild;
  };
  const hin = (el, ziel) => {
    if (!el) { return; }
    if (!platz.has(el)) {
      const ph = document.createComment('dt');
      el.parentNode.insertBefore(ph, el);
      platz.set(el, ph);
    }
    ziel.append(el);
  };
  const gruppe = (titel, ...kinder) => {
    const g = mk('<div class="dtGruppe"></div>');
    g.append(...kinder);
    g.setAttribute('aria-label', titel);
    return g;
  };

  function bauen() {
    gitter = mk('<div id="dtGitter"></div>');
    const bar = mk('<div id="dtBar"></div>');
    const links = mk('<div id="dtLinks"></div>');
    const mitte = mk('<div id="dtMitte"></div>');
    const rechts = mk('<div id="dtRechts"></div>');
    gitter.append(bar, links, mitte, rechts);

    // Schrittwahl: neu, stellt nur den Feinschritt der +/- Tasten im Bild.
    const schritt = mk('<div class="chips" id="dtSchritt"></div>');
    for (const [hz, t] of [[10, '10 Hz'], [100, '100 Hz'], [1000, '1 kHz']]) {
      const c = mk('<button type="button" class="chip">' + t + '</button>');
      c.dataset.hz = String(hz);
      c.addEventListener('click', () => {
        schrittAn = hz; feinSetzen(hz);
        schritt.querySelectorAll('.chip').forEach((x) => x.classList.toggle('on', x === c));
        const sp = document.querySelector('.feinschritt span');
        if (sp) { sp.textContent = t; }
      });
      if (hz === 100) { c.classList.add('on'); }
      schritt.append(c);
    }
    bar.append(gruppe('Band', $('bands')), gruppe('Betriebsart', $('modes')),
               gruppe('Filter', $('filters')), gruppe('Schritt', schritt),
               gruppe('NR', $('dsp')));

    // Links: S-Meter (vorhanden), SWR (neu), Log.
    const swr = mk('<div class="schicht dtPnl"><div class="caps">SWR</div>' +
      '<div class="dtMess"><svg viewBox="0 0 132 62" aria-hidden="true">' +
      '<path d="M14 56 A52 52 0 0 1 118 56" fill="none" stroke="#2c2c31" stroke-width="2"/>' +
      '<line id="dtSwrNadel" x1="66" y1="60" x2="66" y2="14" stroke="#7e7e85" stroke-width="2" stroke-linecap="round"/>' +
      '<circle cx="66" cy="60" r="3" fill="#7e7e85"/></svg>' +
      '<div><div class="dtWert" id="dtSwrWert">—</div><div class="dtKlein">nur beim Senden</div></div></div></div>');
    swrNadel = swr.querySelector('#dtSwrNadel'); swrWert = swr.querySelector('#dtSwrWert');
    const log = mk('<div class="schicht dtPnl dtLogPnl"><div class="dtLogKopf"><span class="caps">Log</span></div><div id="dtLogZeilen"></div></div>');
    logZeilen = log.querySelector('#dtLogZeilen');
    hin(document.querySelector('.meter'), links);
    links.append(swr, log);
    hin($('logAuf'), log.querySelector('.dtLogKopf'));

    hin(document.querySelector('.scope'), mitte);

    // Rechts: Kopf, Combined (Frequenz, Filterbreite, Schritt), Regler, Rotor.
    const comb = mk('<div class="schicht dtPnl"><div class="caps">Combined</div>' +
      '<div id="dtComb"></div>' +
      '<div class="dtZeile"><span class="caps">Filterbreite</span><span id="dtFb" class="dtW">—</span></div>' +
      '<div class="dtZeile"><span class="caps">Schritt</span><span id="dtSchr" class="dtW">100 Hz</span></div></div>');
    fbText = comb.querySelector('#dtFb');
    hin(document.querySelector('header'), rechts);
    rechts.append(comb);
    hin(document.querySelector('.vfo'), comb.querySelector('#dtComb'));
    hin(document.querySelector('.sliders'), rechts);
    hin($('rotorFeld'), rechts);
    hin(document.querySelector('.mikrow'), rechts);

    document.body.prepend(gitter);
    // Sendeleiste und Fusszeile bleiben, wo sie sind (fest unten).
  }

  function aufraeumen() {
    for (const [el, ph] of platz) {
      if (ph.parentNode) { ph.parentNode.replaceChild(el, ph); }
    }
    platz.clear();
    if (gitter) { gitter.remove(); gitter = null; }
    document.body.classList.remove('dt');
  }

  function umschalten() {
    if (mq.matches && !gitter) {
      document.body.classList.add('dt');
      bauen();
      logHolen();
    } else if (!mq.matches && gitter) {
      aufraeumen();
    }
  }
  mq.addEventListener('change', umschalten);

  // Werte, die das Raster zusaetzlich zeigt. Gelesen, nie geschrieben.
  setInterval(() => {
    if (!gitter) { return; }
    const f = document.querySelector('#filters .chip.on');
    if (fbText) { fbText.textContent = f ? f.textContent.trim() : '—'; }
    const s = document.getElementById('dtSchr');
    if (s) { s.textContent = schrittAn === 1000 ? '1 kHz' : schrittAn + ' Hz'; }
    const v = link.ready ? link.st.txSwr : null;
    if (swrNadel && swrWert) {
      if (v && v >= 1 && (link.st.mox || link.st.tune)) {
        swrWert.textContent = v.toFixed(1);
        swrNadel.setAttribute('transform', 'rotate(' + (-62 + (Math.min(v, 3) - 1) / 2 * 124) + ' 66 60)');
      } else {
        swrWert.textContent = '—';
        swrNadel.setAttribute('transform', 'rotate(-62 66 60)');
      }
    }
    // Log spiegeln: die Zeilen der Logliste, ohne eigene Abfrage-Logik.
    const quelle = document.getElementById('logListe');
    if (quelle && logZeilen) {
      const zeilen = [...quelle.children].slice(0, 8).map((k) => (k.children.length ? [...k.children].map((c) => c.textContent.trim()).filter(Boolean).join('  ') : k.textContent.trim())).filter(Boolean);
      const neu = zeilen.length ? zeilen.map((t) => '<div class="dtLogZ">' + t.replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c])) + '</div>').join('') : '<div class="dtKlein">noch keine Einträge</div>';
      if (logZeilen.innerHTML !== neu) { logZeilen.innerHTML = neu; }
    }
  }, 500);

  link.addEventListener('ready', () => { if (gitter) { logHolen(); } });
  umschalten();
}
