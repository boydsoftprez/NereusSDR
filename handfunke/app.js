// app.js — Longpath Handfunke.
//
// Die Seite haelt KEINEN eigenen Zustand: alles, was angezeigt wird, kommt aus
// einer Zeile des Servers. Ein Knopfdruck schickt einen Befehl und wartet auf
// die Gegenmeldung, statt die Anzeige vorher umzustellen. Das kostet einen
// Wimpernschlag und erspart die Klasse von Fehlern, bei der das Telefon etwas
// anderes behauptet als das Geraet tut.

import { TciLink, Fft, feld } from './tci.js';
import { Mikrofon, mikrofonMoeglich } from './mikrofon.js';
import { Sammelstelle, zeileLesen, befundLesen, zeitKurz, dupeSatz }
  from './logbuch.js';
import { spotLesen, marken, trefferBei, trefferAufSchrift } from './spots.js';
import { Sendetasten, SENDEN, TUNE, ablehnText } from './sendetaste.js';
import { Zeichenbremse } from './ton-vorrang.js';
import { bildIstAlt } from './aufwachen.js';
import { standLesen, zustandText, fehlerText as rotorFehlerText,
         peilungAus, Sicherung as RotorSicherung,
         DREHT } from './rotor.js';

const $ = (id) => document.getElementById(id);
const link = new TciLink();
// Fuer die Fehlersuche im Pruefbrowser erreichbar machen. Schadet auf dem
// Telefon nicht und erspart beim Nachsehen jedes Mal eine Sonde.
window.__link = link;

// ── Baender: Anfang, Ende, Anzeigename ──────────────────────────────────────
// Nur die Baender, die Longpath auch kennt (IARU R1, Kurzwelle + 6 m).
const BAENDER = [
  { n: '160', von: 1810000,  bis: 2000000,  mitte: 1840000 },
  { n: '80',  von: 3500000,  bis: 3800000,  mitte: 3700000 },
  { n: '40',  von: 7000000,  bis: 7200000,  mitte: 7100000 },
  { n: '30',  von: 10100000, bis: 10150000, mitte: 10120000 },
  { n: '20',  von: 14000000, bis: 14350000, mitte: 14200000 },
  { n: '17',  von: 18068000, bis: 18168000, mitte: 18120000 },
  { n: '15',  von: 21000000, bis: 21450000, mitte: 21250000 },
  { n: '12',  von: 24890000, bis: 24990000, mitte: 24940000 },
  { n: '10',  von: 28000000, bis: 29700000, mitte: 28400000 },
  { n: '6',   von: 50000000, bis: 52000000, mitte: 50150000 },
];

// Filtervorgaben: bewusst NICHT erfunden, sondern dieselben Breiten, die
// Longpath seit #139/#140 aus Thetis uebernommen hat.
const FILTER = {
  ssb:   [[1800, '1.8k'], [2400, '2.4k'], [2700, '2.7k'], [3100, '3.1k'], [3900, '3.9k']],
  cw:    [[100, '100'], [250, '250'], [400, '400'], [800, '800'], [1000, '1.0k']],
  digi:  [[1000, '1.0k'], [1500, '1.5k'], [2400, '2.4k'], [3000, '3.0k']],
  am:    [[5000, '5.0k'], [8000, '8.0k'], [10000, '10k'], [16000, '16k']],
};

// ── Sendefreigabe: der Riegel im Quelltext ──────────────────────────────────
//
// FALSE, bis Martin im Chat ausdruecklich freigibt (erste Sendung nur an der
// Dummy-Last, nie an der Antenne). Solange false, ist die Seite so tot wie
// zuvor: keine Entsicherung, kein `trx:…,true`, kein `tune:…,true`.
const SENDEN_FREIGABE = false;

const state = {
  trx: 0,
  afPct: 70,
  wfRow: 0,
  spec: null,
  specMin: -130, specMax: -30,
  token: '',                 // nur für den Netzweg nötig
  audioRate: 12000,          // ausgehandelt; 48000 wäre Faktor 16 teurer
  spanneHz: 48000,           // gezeigte Bandbreite; 0 = alles
  bildHalten: false,         // Bild einfrieren, solange abgestimmt wird
  wischVersatzPx: 0,         // wie weit der Finger von der Mitte weg ist
  wfBoden: null,             // geglaetteter Rauschboden des Wasserfalls (dBm)
  // null = noch nicht versucht, true = 'playback' gesetzt, false = Behelf.
  // Die drei Zustaende muessen unterscheidbar bleiben: 'behelf' heisst, die
  // Schnittstelle fehlt und der Stummschalter greift wieder — das ist ein
  // Befund. 'noch nicht versucht' ist keiner.
  sitzungGesetzt: null,
  hfAbstand: null,           // geglaettet: staerkster Punkt minus Rauschboden (dB)
  tonTypPruefung: null,      // Nachfassen, falls mulaw8 nicht bestaetigt wird
  tonStartLaeuft: false,     // Riegel gegen doppelten AudioContext
  tonWeg: null,              // 'worklet' | 'scriptprocessor'
  tonFehler: null,           // Text fuer die Fusszeile, wenn kein Ton geht
  hatSpektrumstrom: false,   // Server liefert fertige Bins
  // Zuletzt angezeigter Sendezustand. Nicht die Wahrheit, sondern was
  // auf dem Schirm steht — damit die Zeile nur bei einer FLANKE neu
  // geschrieben wird und nicht sechzigmal je Sekunde.
  sendetGezeigt: false,
  rueckfall: false,          // wir rechnen selbst aus rohem I/Q
  iqRueckfall: null,
  audio: null, node: null,
};

/** Wie viele Hertz das Bild WIRKLICH zeigt.
 *
 *  Eine Stelle, nicht fuenf. Vorher rechnete jede Stelle fuer sich mit
 *  `state.spanneHz` — also mit unserem WUNSCH. Der Server kann davon
 *  abweichen: die Punktzahl ist eine Untergrenze fuer die Zahl der Bins
 *  (bei 373 Punkten und einer 2048er FFT wird aus 6 kHz knapp 8,8 kHz), und
 *  ohne bekannte Abtastrate beschneidet er gar nicht. Dann sitzen
 *  Abstimmstrich, Durchlassband und der Wasserfallversatz daneben, ohne dass
 *  irgendwo etwas davon steht.
 *
 *  Seit `spectrum_span` sagt er es. Reihenfolge: gemeldete Spanne, sonst
 *  volle Breite (I/Q-Rate), sonst unser Wunsch, sonst der Rueckfall.
 */
function bildSpanneHz(rueckfall) {
  const gemeldet = link.st.spektrumSpanneHz;
  if (gemeldet > 0) { return gemeldet; }
  if (gemeldet === 0) { return link.st.iqRate || rueckfall || 0; }
  if (state.spanneHz > 0) { return state.spanneHz; }
  return link.st.iqRate || rueckfall || 0;
}

// ── Kopplung ────────────────────────────────────────────────────────────────
//
// Die Seite weiss, woher sie geladen wurde — und Longpath laeuft auf
// genau diesem Rechner. Die Adresse muss also niemand eintippen.
//
// Das war bis zum 2026-10-01 anders, und es hat einen Abend gekostet: Auf
// dem Telefon stand noch eine alte IP aus einem anderen Netz, und die
// Seite meldete nur "keine Antwort". Ein Rechnername aus dem Heimnetz
// (`...local`) ueberlebt jeden Netzwechsel; eine IP tut das nicht.
//
// Faellt der Name weg (Seite per file:// geoeffnet), bleibt das Feld leer
// und das Kopplungsblatt fragt wie bisher.
function eigenerHost() {
  const h = location.hostname;
  if (!h || h === 'localhost' || h === '127.0.0.1') { return ''; }
  return h;
}

/** Derselbe Rechner, Longpaths Port.
 *
 *  Es gibt nur diese eine Adresse — auch wenn die Bruecke laeuft: die
 *  belegt dieselbe Portnummer auf der Netzadresse, waehrend Longpath
 *  127.0.0.1 haelt. Darum muss hier nichts probiert und nichts
 *  unterschieden werden. */
const PORT_TCI = 50001;

const gespeichert = () => { try { return localStorage.getItem('handfunke.adresse') || ''; } catch (e) { return ''; } };
const merken = (v) => { try { localStorage.setItem('handfunke.adresse', v); } catch (e) {} };
// Das Token wird nur gebraucht, wenn Longpath ins Netz gebunden ist; auf
// demselben Rechner (Loopback) verlangt der Server keines.
const tokenLesen  = () => { try { return localStorage.getItem('handfunke.token') || ''; } catch (e) { return ''; } };
const tokenMerken = (v) => { try { localStorage.setItem('handfunke.token', v); } catch (e) {} };

// ── Von draussen ───────────────────────────────────────────────────────────
//
// Im eigenen Netz steht im Adressfeld ein Rechner. Von unterwegs steht
// dort die Stations-Id: 26 Zeichen aus a-z und 2-7, die Longpath im
// Kopplungsblatt zeigt. Sie ist keine Adresse -- die Station ist darunter
// erreichbar, wo auch immer sie gerade steht.
//
// Der Vermittlungsdienst ist eine Einstellung und keine Eingabe: er
// aendert sich nie, die Id auch nicht. Wer beides zugleich angeben will,
// schreibt `dienst#id`.
// Kommt die Seite selbst von einem Rendezvous-Server, ist der Dienst
// genau dort -- dann muss niemand etwas eintippen ausser der Id.
//
// Das ist der Fall, auf den der ganze Weg hinauslaeuft: der Server
// liefert die Seite mit aus (siehe fernzugriff/SERVER.md). Damit faellt
// dreierlei zusammen -- von draussen erreichbar, sicherer Kontext (und
// damit Mikrofon, AudioWorklet und der Opus-Dekoder), und keine
// Einstellung, die man vergessen kann.
//
// Nur ueber https, und nicht auf einer privaten Adresse: eine Seite von
// 172.30.30.x oder localhost ist der Rechner im Shack, nicht der
// Vermittler.
const PRIVAT = /^(localhost$|127\.|10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.|\[?::1\]?$)/;

function dienstVomOrt() {
  if (location.protocol !== 'https:') { return ''; }
  if (PRIVAT.test(location.hostname)) { return ''; }
  return 'wss://' + location.host + '/';
}

const DIENST_VORGABE = '';
const dienstLesen = () => {
  const vomOrt = dienstVomOrt();
  if (vomOrt) { return vomOrt; }
  try { return localStorage.getItem('handfunke.dienst') || DIENST_VORGABE; }
  catch (e) { return DIENST_VORGABE; }
};
const dienstMerken = (v) => {
  try { localStorage.setItem('handfunke.dienst', v); } catch (e) {}
};

const IST_STATION_ID = /^[a-z2-7]{26}$/;

/**
 * Zerlegt eine Eingabe, die eine Stations-Id enthaelt.
 *
 * Gibt `null` zurueck, wenn gar keine Id darin steckt (dann ist es eine
 * gewoehnliche Adresse), sonst `{adresse}` oder `{fehler}`. Beides zu
 * trennen ist noetig, weil eine Id ohne Dienst ein Fehler ist und nicht
 * still als Rechnername weiterlaufen darf.
 */
function fernadresse(eingabe) {
  let dienst = '', ident = '';
  const raute = eingabe.lastIndexOf('#');
  if (raute >= 0) {
    dienst = eingabe.slice(0, raute).trim();
    ident = eingabe.slice(raute + 1).trim();
  } else if (IST_STATION_ID.test(eingabe)) {
    dienst = dienstLesen();
    ident = eingabe;
  }
  if (!IST_STATION_ID.test(ident)) { return null; }
  if (!dienst) {
    return { fehler: 'Fuer den Zugang von draussen fehlt der Vermittlungsdienst. '
      + 'Einmal als dienst#id eingeben, dann merkt die Seite ihn sich. '
      + '(Kommt die Seite vom Vermittler selbst, entfaellt das.)' };
  }
  if (!/^wss?:\/\//.test(dienst)) { dienst = 'wss://' + dienst; }
  dienstMerken(dienst);
  return { adresse: 'fern:' + dienst + '#' + ident };
}

function starten(adresse) {
  const roh = adresse.trim();
  const fern = fernadresse(roh);
  if (fern) {
    if (fern.fehler) { $('fehler').textContent = fern.fehler; return; }
    merken(roh);
    $('fehler').textContent = '';
    $('koppeln').classList.remove('an');
    link.connect(fern.adresse);
    return;
  }
  const a = roh.replace(/^wss?:\/\//, '');
  if (!a) { $('fehler').textContent = 'Bitte eine Adresse eingeben.'; return; }
  const mitPort = /:\d+$/.test(a) ? a : a + ':50001';
  merken(mitPort);
  state.token = ($('token').value || '').trim().toUpperCase();
  tokenMerken(state.token);
  $('fehler').textContent = '';
  $('koppeln').classList.remove('an');
  // Das Schema folgt dem der Seite, es wird nicht gewaehlt. Eine
  // https-Seite DARF kein ws:// oeffnen — der Browser sperrt das als
  // gemischten Inhalt, ohne dass die Seite davon etwas mitbekommt, und
  // der Fehler sieht aus wie "das Funkgeraet antwortet nicht".
  // Umgekehrt scheitert wss:// an einer Bruecke ohne Zertifikat. Beides
  // ist keine Entscheidung, die der Bediener treffen soll.
  const schema = (location.protocol === 'https:') ? 'wss://' : 'ws://';
  link.connect(schema + mitPort);
}

// ── Raumbild statt Wasserfall ─────────────────────────────────────────────
//
// Dieselbe Ansicht, die Longpath am Mac hat (`src/gui/DssRenderer.cpp`):
// gestaffelte Zeilen statt einer rollenden Flaeche. Sie ersetzt den
// Wasserfall im unteren Feld; die Kurve darueber bleibt, wie sie ist --
// das war Variante C der Entwurfsblaetter.
//
// Die Farben kommen aus derselben Rampe wie der Wasserfall. Zwei
// Farbquellen fuer dieselbe Groesse waeren die sicherste Art, sie
// auseinanderlaufen zu lassen.
let raumbild = null;          // erst beim ersten Einschalten geladen
const raumAn = () => { try { return localStorage.getItem('handfunke.raumbild') === '1'; }
                       catch (e) { return false; } };
const raumMerken = (v) => { try { localStorage.setItem('handfunke.raumbild', v ? '1' : '0'); }
                            catch (e) {} };
let raumZeigen = raumAn();

async function raumbildHolen() {
  if (raumbild) { return raumbild; }
  const { Raumbild } = await import('./raumbild.js');
  raumbild = new Raumbild();
  return raumbild;
}

async function raumUmschalten() {
  raumZeigen = !raumZeigen;
  raumMerken(raumZeigen);
  $('raumknopf').classList.toggle('an', raumZeigen);
  if (raumZeigen) {
    await raumbildHolen();
  } else {
    // Beim Zurueckschalten die Flaeche leeren: sonst steht das letzte
    // Raumbild unter dem anlaufenden Wasserfall.
    wfCtx.clearRect(0, 0, wf.width, wf.height);
  }
  zeichneBild();
}

$('raumknopf').addEventListener('click', (e) => {
  e.preventDefault();
  e.stopPropagation();
  raumUmschalten();
}, { passive: false });
// Die Abstimmgeste liegt darunter -- die Beruehrung darf sie nicht erreichen.
$('raumknopf').addEventListener('touchstart', (e) => e.stopPropagation(), { passive: true });

if (raumZeigen) {
  $('raumknopf').classList.add('an');
  raumbildHolen();
}

// ── Koppeln mit einem Tipp ────────────────────────────────────────────────
//
// Fuer den Zugang von draussen muss dieses Telefon einmal bei der
// Station angemeldet sein. Frueher hiess das: 122 Zeichen Schluessel
// abtippen. Jetzt oeffnet die Station ein Fenster (`fuehren.py
// --koppeln-offen`), und waehrend es offen ist, genuegt dieser Knopf.
//
// Der Knopf zeigt sich nur, wenn wirklich ein Fenster offen ist --
// sonst waere er die meiste Zeit ein Knopf, der nichts tut.
const KOPPEL_PORT = 8790;

function koppelstelle() {
  // Dieselbe Maschine wie die Seite, eigener Port.
  const host = location.hostname || '127.0.0.1';
  return `${location.protocol}//${host}:${KOPPEL_PORT}/koppeln`;
}

async function koppelLage() {
  try {
    const a = await fetch(koppelstelle(), {
      method: 'POST', body: '{}', signal: AbortSignal.timeout(2500),
    });
    return await a.json();
  } catch (e) { return null; }
}

async function koppelnTippen() {
  const m = $('koppelmeldung');
  // WebCrypto braucht einen sicheren Kontext. Ohne ihn gibt es keinen
  // Geraeteschluessel und damit keinen Fernzugriff — das hier zu
  // verschweigen hiesse, den Bediener raten zu lassen, warum nichts
  // passiert.
  if (!window.isSecureContext || !window.crypto?.subtle) {
    m.style.color = 'var(--warn)';
    m.textContent = 'Dafür muss die Seite über https kommen. '
      + 'Über http fehlt dem Telefon der Schlüsselspeicher.';
    return;
  }
  m.style.color = 'var(--text3)';
  m.textContent = 'koppelt …';
  try {
    const { geraeteschluessel } = await import('./fernzugriff.js');
    const ich = await geraeteschluessel();
    const a = await fetch(koppelstelle(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        schluessel: ich.schluesselText,
        name: 'Telefon',
        art: 'phone',
      }),
      signal: AbortSignal.timeout(5000),
    });
    const e = await a.json();
    if (e.ok) {
      m.style.color = 'var(--ok, var(--text3))';
      m.textContent = 'Gekoppelt. Von draußen genügt jetzt die Stations-Id.';
      $('koppelknopf').style.display = 'none';
    } else {
      m.style.color = 'var(--warn)';
      m.textContent = e.grund || 'Das hat nicht geklappt.';
    }
  } catch (e) {
    m.style.color = 'var(--warn)';
    m.textContent = 'Die Station antwortet nicht. Läuft dort ein Koppelfenster?';
  }
}

$('koppelknopf').addEventListener('click', koppelnTippen);

// Einmal beim Laden nachsehen, ob gerade ein Fenster offen ist.
koppelLage().then((l) => {
  if (l && l.offen) {
    $('koppelknopf').style.display = '';
    $('koppelmeldung').textContent =
        `Die Station nimmt dieses Telefon auf (noch ${l.rest} s).`;
  }
});

$('verbinden').addEventListener('click', () => { blattVonHand = false; starten($('adresse').value); });
$('adresse').addEventListener('keydown', (e) => { if (e.key === 'Enter') starten($('adresse').value); });
$('adresse').value = gespeichert() || (eigenerHost() ? eigenerHost() + ':' + PORT_TCI : '');
$('token').value = tokenLesen();

/** Verbindet ohne Eingabe mit dem Rechner, von dem die Seite kam. */
function verbindeSelbst() {
  const host = eigenerHost();
  if (!host) { return false; }
  starten(host + ':' + PORT_TCI);
  return true;
}

// Beim Start: eine gemerkte Adresse hat Vorrang — der Operator hat sie
// bewusst gesetzt. ABER sie kann veraltet sein: eine IP aus einem anderen
// Netz steht nach einem Netzwechsel still im Weg, und die Seite meldete
// bisher nur "keine Antwort" (am 2026-10-01 genau so passiert, mit einer
// 192.168er-Adresse auf dem Telefon, waehrend der Mac im 172.30.30er hing).
//
// Darum: gemerkte Adresse zuerst, aber wenn sie binnen fuenf Sekunden
// nicht zu Longpath fuehrt, still auf den eigenen Rechner umschwenken —
// den, von dem diese Seite kam. Der kann gar nicht falsch sein.
if (gespeichert()) {
  starten(gespeichert());
  const gemerkt = gespeichert();
  const eigen = eigenerHost() ? eigenerHost() + ':' + PORT_TCI : '';
  if (eigen && eigen !== gemerkt) {
    let fertig = false;
    const merkeErfolg = () => { fertig = true; };
    link.addEventListener('ready', merkeErfolg, { once: true });
    setTimeout(() => {
      if (fertig) { return; }
      link.removeEventListener('ready', merkeErfolg);
      starten(eigen);
    }, 5000);
  }
} else {
  verbindeSelbst();
}

// ── Anzeige ─────────────────────────────────────────────────────────────────
function hzText(hz) {
  if (!hz) return ['—', '', ''];
  const s = String(Math.round(hz)).padStart(9, '0');
  return [String(parseInt(s.slice(0, 3), 10)), '.' + s.slice(3, 6), '.' + s.slice(6, 9)];
}

function bandFuer(hz) {
  const b = BAENDER.find(b => hz >= b.von && hz <= b.bis);
  return b ? b.n + ' m' : '';
}

function modeArt(m) {
  const u = (m || '').toLowerCase();
  if (u.startsWith('cw')) return 'cw';
  if (u.startsWith('digl') || u.startsWith('digu') || u.startsWith('dig')) return 'digi';
  if (u === 'am' || u === 'sam' || u === 'fm' || u === 'nfm') return 'am';
  return 'ssb';
}

let letzterModus = '', letzteModi = '';

function zeichneBedienung() {
  const s = link.st;

  // Betriebsarten — die Liste kommt vom Server (modulations_list), nicht
  // von mir. Sonst zeigt die Seite Betriebsarten, die das Geraet nicht hat.
  const modi = (s.modes.length ? s.modes : ['lsb','usb','cw','digu','am']).slice(0, 5);
  const modiSchl = modi.join(',') + '|' + s.mode[state.trx];
  if (modiSchl !== letzteModi) {
    letzteModi = modiSchl;
    $('modes').innerHTML = '';
    modi.forEach(m => {
      const el = document.createElement('div');
      el.className = 'chip' + (m.toLowerCase() === (s.mode[state.trx] || '').toLowerCase() ? ' on' : '');
      el.textContent = m.toUpperCase();
      el.onclick = () => link.send(`modulation:${state.trx},${m.toLowerCase()}`);
      $('modes').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('modes').appendChild(mehr);
  }

  // Baender — fuenf um das aktuelle herum, damit der Daumen nicht sucht.
  const hz = s.vfo[state.trx][0];
  const idx = Math.max(0, BAENDER.findIndex(b => hz >= b.von && hz <= b.bis));
  const von = Math.min(Math.max(0, idx - 2), BAENDER.length - 5);
  const sicht = BAENDER.slice(von, von + 5);
  const bandSchl = sicht.map(b => b.n).join(',') + '|' + idx;
  if (bandSchl !== $('bands').dataset.schl) {
    $('bands').dataset.schl = bandSchl;
    $('bands').innerHTML = '';
    sicht.forEach(b => {
      const el = document.createElement('div');
      el.className = 'chip' + (hz >= b.von && hz <= b.bis ? ' on' : '');
      el.textContent = b.n;
      el.onclick = () => { link.send(`vfo:${state.trx},0,${b.mitte}`);
        wasserfallSchnitt(); letzteMitteHz = b.mitte; };
      $('bands').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('bands').appendChild(mehr);
  }

  // Filter — die Breiten haengen an der Betriebsart.
  const art = modeArt(s.mode[state.trx]);
  const breite = Math.abs((s.filter[state.trx][1] || 0) - (s.filter[state.trx][0] || 0));
  const fSchl = art + '|' + breite;
  if (fSchl !== $('filters').dataset.schl) {
    $('filters').dataset.schl = fSchl;
    $('filters').innerHTML = '';
    FILTER[art].forEach(([w, name]) => {
      const el = document.createElement('div');
      el.className = 'chip' + (Math.abs(breite - w) < 60 ? ' on' : '');
      el.textContent = name;
      el.onclick = () => {
        // CW liegt um den Tonversatz, Sprache ab 100 Hz — wie am Pult.
        const lo = art === 'cw' ? Math.round(-w / 2) : 100;
        const hi = art === 'cw' ? Math.round(w / 2) : 100 + w;
        link.send(`rx_filter_band:${state.trx},${lo},${hi}`);
      };
      $('filters').appendChild(el);
    });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('filters').appendChild(mehr);
  }

  // Aufbereitung — Zustandsschalter bleiben leise (nur Rand), damit im Feld
  // genau eine Stelle kraeftig ist: die Sendetaste.
  if (!$('dsp').dataset.fertig) {
    $('dsp').dataset.fertig = '1';
    [['NB','rx_nb_enable'],['NR','rx_nr_enable'],['ANF','rx_anf_enable'],['SQL','sql_enable']]
      .forEach(([name, cmd]) => {
        const el = document.createElement('div');
        el.className = 'chip quiet'; el.textContent = name; el.dataset.cmd = cmd;
        el.onclick = () => {
          const an = el.classList.contains('on');
          link.send(`${cmd}:${state.trx},${an ? 'false' : 'true'}`);
          el.classList.toggle('on');   // Der Server bestaetigt nicht jeden
        };                             // dieser Schalter — hier ist die
        $('dsp').appendChild(el);      // Anzeige notgedrungen lokal.
      });
    const mehr = document.createElement('div');
    mehr.className = 'chip more'; mehr.textContent = '…';
    $('dsp').appendChild(mehr);
  }

  // Sendeleistung — der Regler, der bis PR #145 wirkungslos war.
  if (s.drive !== null) {
    $('pwrFill').style.width = s.drive + '%';
    $('pwrKnob').style.left  = s.drive + '%';
    $('pwrVal').textContent  = s.drive + ' %';
  }
  $('afFill').style.width = state.afPct + '%';
  $('afKnob').style.left  = state.afPct + '%';
  $('afVal').textContent  = state.afPct + ' %';
}

function zeichneKopf() {
  const s = link.st;
  const verbunden = link.ws && link.ws.readyState === 1;
  // Dieselbe Lebendigkeitspruefung wie in der Fusszeile: nicht der Socket
  // entscheidet, ob es laeuft, sondern ob Daten kommen.
  const totStill = link.ready && link.letzteDaten
                 && (performance.now() - link.letzteDaten > 3000);
  $('led').className = 'dot' + (!verbunden ? ' off'
                              : (link.ready && !totStill) ? '' : ' wait');
  // Das `device:`-Feld des Init-Bursts ist NICHT das Funkgeraet, sondern
  // Longpaths Tarnkappe: der TCI-Server gibt sich als SunSDR2PRO aus, weil
  // WSJT-X und Hamlib TCI-Audio nur freischalten, wenn der Server sich so
  // meldet (TciEmulateSunSDR2Pro, am Pruefstand 2026-05-11 bestaetigt).
  // Hier ungefiltert angezeigt hat es am 2026-10-01 genau das angerichtet,
  // was es anrichten muss — Betreiber: "ist komischerweise mit sunsdr
  // verbunden … sollte aber mit avelina". Also nur zeigen, was stimmt:
  // die Handfunke spricht mit Longpath. Welches Geraet dort haengt, sagt
  // TCI nicht, und Raten waere schlimmer als Schweigen.
  $('station').textContent = verbunden ? 'LONGPATH' : 'NICHT VERBUNDEN';

  // Wie breit das Bild gerade ist. Ohne diese Angabe weiss man beim Kneifen
  // nicht, wo man gelandet ist — und auch nicht, wie fein die Abstimmung
  // gerade greift (das Raster haengt an der Spanne).
  // ── Der Abstimmstrich gehoert dorthin, wo die Frequenz ist ──────────────
  //
  // Bis 2026-10-01 stand er fest bei 50 % (stil.css, .cursor). Damit zeigte
  // er beim Abstimmen keine Bewegung — der Betreiber: "beim wischen nach
  // rechts und links ändert sich nur die frequenz". Genau so war es: die
  // Zahl lief, das Bild stand, und der Strich blieb stur in der Mitte.
  //
  // Die Bildmitte ist die DDC-Frequenz (dds), nicht die abgestimmte (vfo).
  // Beide fallen nur zusammen, solange nicht innerhalb der DDC-Breite
  // abgestimmt wird. Der Abstand zwischen ihnen, geteilt durch die gezeigte
  // Spanne, ist die Stelle im Bild.
  // ── Der Durchlass als Flaeche ──────────────────────────────────────────
  //
  // Betreiber 2026-10-02: "man sollte die bandbreite nun auch nicht nur als
  // strich sehen". Der Strich sagt, worauf abgestimmt ist; das Band sagt,
  // was davon zu hoeren ist — und ob ein Signal ueberhaupt hineinfaellt.
  //
  // Die Grenzen kommen als Versatz zur abgestimmten Frequenz (rx_filter_band).
  // Bei den unteren Seitenbaendern meldet der Server sie teils positiv; dann
  // liegt der Durchlass in Wahrheit UNTER dem Traeger und muss gespiegelt
  // werden, sonst zeigt das Band auf die falsche Seite.
  {
    const bd = $('durchlass');
    const f  = link.st.filter[state.trx] || [];
    const m  = (link.st.mode[state.trx] || '').toLowerCase();
    const mitteD = link.st.dds[state.trx];
    const vfoD   = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
    const spanneD = bildSpanneHz(0);
    let lo1 = f[0], hi1 = f[1];
    if (bd && mitteD && vfoD && spanneD > 0 &&
        Number.isFinite(lo1) && Number.isFinite(hi1) && hi1 !== lo1) {
      if (lo1 > hi1) { const t = lo1; lo1 = hi1; hi1 = t; }
      const unten = m === 'lsb' || m === 'cwl' || m === 'digl';
      if (unten && lo1 >= 0 && hi1 >= 0) { const a = -hi1; hi1 = -lo1; lo1 = a; }
      const pA = 50 + ((vfoD + lo1 - mitteD) / spanneD) * 100;
      const pB = 50 + ((vfoD + hi1 - mitteD) / spanneD) * 100;
      const links  = Math.max(0, Math.min(100, Math.min(pA, pB)));
      const rechts = Math.max(0, Math.min(100, Math.max(pA, pB)));
      if (rechts - links >= 0.4) {
        bd.style.left  = links.toFixed(2) + '%';
        bd.style.width = (rechts - links).toFixed(2) + '%';
        bd.style.display = 'block';
      } else {
        // Schmaler als ein halbes Prozent: als Flaeche nicht mehr lesbar,
        // und ein Ein-Pixel-Band waere nur ein zweiter Strich neben dem
        // Abstimmstrich. Dann lieber nichts — weiter hineinzoomen hilft.
        bd.style.display = 'none';
      }
    } else if (bd) {
      bd.style.display = 'none';
    }
  }

  {
    const c = document.querySelector('.cursor');
    if (c) {
      // Waehrend des Haltens zeigt der Strich, wohin der Finger faehrt.
      if (state.bildHalten) {
        const b = $('scope').getBoundingClientRect().width || 1;
        // Dieselbe Richtung wie die Frequenz oben: der Zeiger folgt dem Finger.
        const anteil = 50 + (state.wischVersatzPx / b) * 100;
        c.style.left = Math.max(0, Math.min(100, anteil)).toFixed(2) + '%';
        c.style.opacity = '1';
        return;
      }
      const mitte = link.st.dds[state.trx];
      const vfo   = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
      const spanne = bildSpanneHz(0);
      if (mitte && vfo && spanne > 0) {
        // 0 % ist der linke Rand, 100 % der rechte; die Mitte ist 50 %.
        const anteil = 50 + ((vfo - mitte) / spanne) * 100;
        // Am Rand stehenbleiben statt aus dem Bild zu laufen: ausserhalb
        // waere er unsichtbar, und dann waere nicht zu sehen, dass die
        // Frequenz den Ausschnitt verlassen hat.
        c.style.left = Math.max(0, Math.min(100, anteil)).toFixed(2) + '%';
        c.style.opacity = (anteil < 0 || anteil > 100) ? '0.35' : '1';
      } else {
        c.style.left = '50%';
        c.style.opacity = '1';
      }
    }
  }

  const sp = $('spanne');
  if (sp) {
    const hz = bildSpanneHz(0);
    sp.textContent = hz ? (hz >= 1000 ? Math.round(hz / 1000) + ' kHz' : hz + ' Hz') : '';
  }

  const [mhz, khz, hz] = hzText(s.vfo[state.trx][0]);
  $('hz').innerHTML = `${mhz}<span class="khz">${khz}</span><span class="dez">${hz}</span>`;
  $('band').textContent = bandFuer(s.vfo[state.trx][0]);

  // ── Das S-Meter darf nicht weiterzeigen, wenn niemand mehr misst ───────
  //
  // Hier stand nur `if (sm !== null)`, und `sm` behaelt nach einem Abriss
  // seinen letzten Wert. Am 2026-10-03 nachgemessen: Server weg bei 8 s, die
  // Kopfzeile sprang sofort auf "NICHT VERBUNDEN" und die Leuchte ging aus —
  // das S-Meter aber stand noch dreissig Sekunden spaeter auf "S7 · -82 dBm".
  //
  // Es ist das Instrument, das man beim Hoeren dauernd ansieht. Ein
  // eingefrorenes S7 sieht aus wie ein Signal, und genau diese Sorte
  // Zweideutigkeit hat an einem einzigen Tag dreimal Zeit gekostet
  // (schlafender AudioContext, bunter Wasserfall ohne Antenne,
  // Kopplungsblatt ueber intakter Verbindung).
  //
  // Dieselbe Regel wie beim Durchlassband: lieber nichts zeigen als etwas
  // Falsches.
  // Massstab ist `ready`, nicht der Socket: Messwerte kommen erst nach dem
  // Init-Burst. Ein offener Socket ohne Anmeldung liefert keine — und wuerde
  // den letzten Wert stehen lassen.
  const sm = s.smeter[state.trx];
  // Ohne abgestimmte Frequenz gibt es keinen Empfaenger — und damit auch
  // keinen Messwert.
  //
  // Steht TCI, haengt aber kein Funkgeraet dran, meldet Longpath als
  // Rueckfall -140 dBm (TciServer.cpp, rxSensorTimer: WDSP-Konvention fuer
  // "kein Kanal"). Die Seite machte daraus brav "S1 · -140 dBm" — eine
  // Messung, die niemand vorgenommen hat. Am 2026-10-03 live so gesehen,
  // waehrend der Panadapter daneben schon "kein Funkgeraet verbunden"
  // schrieb und die Frequenzanzeige ehrlich "—" zeigte. Drei Anzeigen,
  // zwei Wahrheiten.
  //
  // Geprueft wird die Frequenz, nicht der Zahlenwert des Pegels: -140 ist
  // Longpaths Konvention, ein fremder Server waehlt eine andere. Bei 0 Hz
  // ist dagegen jeder Server gemeint, und kein Funkgeraet steht je auf 0.
  const keinGeraet = !s.vfo[state.trx] || !s.vfo[state.trx][0];
  if (!link.ready || keinGeraet) {
    $('smeter').textContent = '—';
    $('sbar').style.width = '0%';
  } else if (sm !== null) {
    $('smeter').textContent = `${sEinheit(sm)} · ${Math.round(sm)} dBm`;
    // −127 dBm = S1, 6 dB je S-Stufe, S9 = −73, darueber bis +40 dB.
    // Gleiche Skala wie die Beschriftung darunter: S1 = -121 … S9+40 = -33.
    $('sbar').style.width = Math.max(0, Math.min(100, ((sm + 121) / 88) * 100)) + '%';
  }
}

function sEinheit(dbm) {
  // IARU R1 fuer Kurzwelle: S9 = -73 dBm, je S-Stufe 6 dB, also S1 = -121 dBm.
  // (Erst falsch gebaut mit -127 als S1 — das verschob die ganze Skala um
  // zwei Stufen nach oben, -92 dBm stand als S7 statt S6 da.)
  if (dbm >= -73) return 'S9+' + Math.round(dbm + 73);
  return 'S' + Math.max(1, Math.min(9, Math.floor((dbm + 121) / 6) + 1));
}

// ── Panadapter + Wasserfall ─────────────────────────────────────────────────
const pan = $('pan'), panCtx = pan.getContext('2d');
const wf = $('wf'), wfCtx = wf.getContext('2d', { willReadFrequently: false });
const N = 1024;
const fft = new Fft(N);
const spec = new Float32Array(N);
const glatt = new Float32Array(N);        // eigene FFT (Rückfall)
let specServer = new Float32Array(0);     // fertig vom Server (Regelfall)
let hatSpektrum = false;

// ── Die Wasserfallrampe „Gedaempft" ────────────────────────────────────────
//
// Seit 2026-10-08 Wert fuer Wert dieselbe wie in SpectrumWidget.cpp
// (mutedStops): dieselben sieben Stuetzpunkte an denselben Stellen, aus
// denselben Rollen. Betreiber: "die grafik vom longpath software 1:1 auf
// das app in bezug auf farben und hintergrund".
//
// Die blaue Fassung bleibt als EINE Zeile erhalten (WASSERFALL_BLAU) --
// sie wurde fuer das Telefon im Hellen gebaut, und das laesst sich nur
// draussen entscheiden, nicht hier. Was dabei zu bedenken ist:
//
// Warum: am Schreibtisch fuellt der Wasserfall ein halbes Fenster, da traegt
// Grau. Am Telefon ist er eine Handflaeche gross und wird oft im Hellen
// angesehen — dort verschwand alles unterhalb eines starken Traegers in
// einem einzigen gleichfoermigen Grau, obwohl er ein Drittel der Hoehe
// bekommt. Betreiber hat am 2026-10-01 die Richtung „Glas und Tiefe"
// gewaehlt, in der dieser Boden blau ist.
//
// Was NICHT uebernommen wird, obwohl es naheliegt: Longpaths
// WfColorScheme::ClarityBlue. Die ist lesbar, laeuft oben aber ueber Gruen
// und Gelb nach Rot — genau der Regenbogen, den „Gedaempft" 2026-08-15
// bewusst abgeschafft hat, und Rot ist hier der Warnung vorbehalten.
// Geaendert ist also nur die Grundfarbe, nicht der Aufbau: Boden
// verschwindet, Waerme erst oben, Weiss ganz oben.
//
// Die Lage der Stuetzpunkte ist am laufenden Bild nachgezogen: mit dem
// Original lagen die Traeger der Attrappe im graublauen Teil und blieben
// kuehl, obwohl sie das Lauteste im Bild waren. Der warme Ton beginnt
// jetzt bei 0,60 statt 0,78, der Boden bleibt unveraendert dunkel.
//
// Zweiter Durchgang 2026-10-02: die ersten 30 % der Rampe waren praktisch
// schwarz, also eine Totzone von fast einem Drittel. Zusammen mit dem zu hoch
// liegenden Boden (siehe dort) blieb eine leise Station unsichtbar. Der dunkle
// Teil ist jetzt auf 18 % gestaucht und der Boden eine Spur heller, damit man
// SIEHT, dass dort Rauschen ist und nicht etwa nichts ankommt. Der Aufbau
// bleibt: Blau unten, Waerme oben, Weiss ganz oben, kein Rot.
// EINE Zeile, zwei Fassungen. true = die blaue von vor dem 2026-10-08.
const WASSERFALL_BLAU = false;
document.documentElement.dataset.wasserfall = WASSERFALL_BLAU ? 'blau' : 'longpath';

// Longpath, SpectrumWidget.cpp mutedStops: app-bg / panel / border /
// text-inactive / instrument-glow-hi / measured / instrument-face, an den
// Stellen 0 / .30 / .48 / .64 / .78 / .92 / 1.
// Mac-Look (2026-11-21, Foto von Mac-Longpath): Wasserfall blau -> cyan ->
// gruen -> gelb; kein Rot, kein Orange im Bild.
const STOPS_LONGPATH = [[0,[0,0,40]],[.22,[0,18,130]],[.42,[0,90,225]],[.58,[0,195,240]],
                        [.74,[30,222,100]],[.90,[222,232,40]],[1,[255,255,232]]];
const STOPS_BLAU     = STOPS_LONGPATH;
const STOPS = WASSERFALL_BLAU ? STOPS_BLAU : STOPS_LONGPATH;

// Flaechen aus EINER Quelle: die Panadapter-Flaeche steht im Stilblatt
// (--pan-bg, von data-wasserfall geschaltet), der leere Wasserfall ist der
// Boden der Rampe selbst. Zwei Zahlen fuer dieselbe Flaeche laufen sonst
// beim naechsten Feinschliff auseinander.
const PAN_BG = getComputedStyle(document.documentElement)
                 .getPropertyValue('--pan-bg').trim() || '#08080a';
const WF_BG  = 'rgb(' + STOPS[0][1].join(',') + ')';
function rampe(t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  for (let i = 1; i < STOPS.length; i++) {
    if (t <= STOPS[i][0]) {
      const [a, ca] = STOPS[i - 1], [b, cb] = STOPS[i], k = (t - a) / (b - a);
      return [ca[0] + (cb[0]-ca[0])*k, ca[1] + (cb[1]-ca[1])*k, ca[2] + (cb[2]-ca[2])*k];
    }
  }
  return [242, 242, 236];
}

const zeile = wfCtx.createImageData(wf.width, 1);
const wfPuff = document.createElement('canvas');
wfPuff.width = wf.width; wfPuff.height = wf.height;
const wfPuffCtx = wfPuff.getContext('2d');

let letzteIq = 0;
let neueZeilen = 0;      // vom Datenstrom gefuellt, von der Zeichenschleife geleert

// ── Den Wasserfall leeren (2026-10-01) ──────────────────────────────────────
//
// Der Wasserfall ist eine Historie: jede Zeile ist ein Augenblick von
// frueher. Nach einem Frequenzsprung zeigt diese Historie ein Band, auf dem
// man gar nicht mehr steht — sie ist nicht veraltet, sondern FALSCH, und sie
// braucht eine halbe Minute, bis sie nach oben herausgewandert ist.
//
// Betreiber am 2026-10-01: "der wasserfall unten springt nicht sofort auf
// die frequenz". Darauf hin wurde beim Sprung GELOESCHT — und am selben
// Abend kam die Gegenbeschwerde: "jede frequenzaenderung loescht unten den
// panadapter und er laedt wieder neu … man sollte immer etwas sehen".
//
// Beides stimmt, und beides zusammen ergibt die richtige Loesung: Die
// Historie ist nicht wertlos, sie steht nur an der falschen Stelle. Ein
// Pult schiebt sie darum seitlich mit, statt sie wegzuwerfen — ein Traeger
// bleibt dabei unter sich selbst stehen, und man sieht ununterbrochen
// etwas. Nur wo die Zeilen wirklich nichts mehr bedeuten (anderes Band,
// andere Bandbreite), wird eine Trennlinie gezogen statt das Bild
// geleert: oberhalb das Neue, unterhalb das Alte, und der Schnitt
// wandert in ein paar Sekunden von selbst hinaus.

function wasserfallLeeren() {
  try {
    wfCtx.fillStyle = WF_BG;
    wfCtx.fillRect(0, 0, wf.width, wf.height);
  } catch (e) { /* vor dem ersten Zeichnen */ }
  neueZeilen = 0;
}

/** Hertz je Bildpunkt des Wasserfalls — eine Stelle, damit Schieben und
 *  Skalieren nicht auseinanderlaufen. */
function hzProPunkt() {
  const spanne = bildSpanneHz(192000);
  return spanne / wf.width;
}

/** Schiebt die Historie um die Frequenzaenderung zur Seite.
 *
 *  Steigt die Mittenfrequenz um dHz, wandert ein fester Sender im Bild nach
 *  LINKS — darum das Minus. Der frei werdende Rand wird dunkel gefuellt:
 *  dort ist nichts bekannt, und eine Wiederholung waere gelogen.
 *
 *  Weiter als die Bildbreite hat das Schieben keinen Sinn, dann ueberlappt
 *  nichts mehr; in dem Fall ein Schnitt. */
function wasserfallSchieben(dHz) {
  if (!dHz) { return; }
  const dx = Math.round(-dHz / hzProPunkt());
  if (dx === 0) { return; }
  if (Math.abs(dx) >= wf.width) { wasserfallSchnitt(); return; }
  try {
    // Ueber den Zwischenpuffer, wie beim Zeilenschub: ein Canvas, der sich
    // selbst als Quelle zeichnet, schiebt nicht verlaesslich.
    wfPuffCtx.clearRect(0, 0, wf.width, wf.height);
    wfPuffCtx.drawImage(wf, 0, 0);
    wfCtx.fillStyle = WF_BG;
    wfCtx.fillRect(0, 0, wf.width, wf.height);
    wfCtx.drawImage(wfPuff, dx, 0);
  } catch (e) { /* vor dem ersten Zeichnen */ }
}

/** Haelt den Wasserfall unter der Frequenz fest, egal wer sie geaendert hat.
 *
 *  Vier Stellen schoben die Historie frueher selbst — Tipp, Wisch, Band,
 *  Zoom. Das deckte aber nur die eigenen Bedienungen ab: dreht der Operator
 *  am Pult oder an einem anderen Client, kommt die neue Frequenz ueber den
 *  Draht herein, und davon erfuhr der Wasserfall nichts. Darum hier EINE
 *  Stelle, die im Zeichentakt die Mitte vergleicht; sie faengt jede
 *  Aenderung, ganz gleich woher sie kam. */
let letzteMitteHz = null;
function mitteVerfolgen() {
  const v = link.st.vfo[state.trx];
  const jetzt = v ? v[0] : null;
  if (jetzt === null) { return; }
  if (letzteMitteHz === null) { letzteMitteHz = jetzt; return; }
  if (jetzt === letzteMitteHz) { return; }
  wasserfallSchieben(jetzt - letzteMitteHz);
  letzteMitteHz = jetzt;
  // Der Ausschnitt hat sich verschoben: die alten Spotmarken sitzen jetzt
  // falsch, und was vorher ausserhalb lag, kann hereingekommen sein. Nicht
  // sofort nachfragen -- beim Drehen des Knopfes kaeme sonst je
  // Zwischenstand eine eigene Liste.
  spotsBaldHolen();
}

/** Zieht eine Trennlinie statt zu loeschen: oben das Neue, unten das Alte.
 *
 *  Fuer Bandwechsel und Zoom — dort deckt dieselbe Zeile danach etwas
 *  voellig anderes ab, und Schieben hilft nicht. Die Linie sagt, wo der
 *  Schnitt liegt, und das Bild bleibt sichtbar, bis das Alte unten
 *  herausgewandert ist. */
function wasserfallSchnitt() {
  // Das Raumbild ist dieselbe Historie, nur anders gezeigt: was fuer den
  // Wasserfall ein Schnitt ist, ist fuer das Raumbild einer.
  if (raumbild) { raumbild.leeren(); }
  try {
    wfCtx.fillStyle = 'rgba(216,165,95,.55)';   // Bernstein, wie alles Gemessene
    wfCtx.fillRect(0, 0, wf.width, 2);
  } catch (e) { /* vor dem ersten Zeichnen */ }
}
link.addEventListener('iq', (e) => {
  const v = e.detail.vals;
  if (v.length < N * 2) return;
  letzteIq = performance.now();
  // Hoechstens 25 Zeilen je Sekunde: darueber sieht das Auge ohnehin nichts,
  // und ein Schub aus dem Netz soll den Wasserfall nicht durchreissen.
  if (neueZeilen < 3) neueZeilen++;
  fft.spectrum(v.subarray(v.length - N * 2), spec);
  // Traegheit wie am Pult: das Bild soll nicht flackern.
  for (let i = 0; i < N; i++) {
    glatt[i] = hatSpektrum ? glatt[i] * 0.6 + spec[i] * 0.4 : spec[i];
  }
  hatSpektrum = true;
});

function zeichneBild() {
  const W = pan.width, H = pan.height;

  // Ohne Gerät kommt kein I/Q. Das muss dastehen, statt dass der Bediener
  // vor einer leeren Fläche sitzt und rät, ob die Verbindung hängt oder die
  // Station schweigt — am Telefon sieht man den Unterschied sonst nicht.
  const still = !hatSpektrum || (performance.now() - letzteIq > 2000);
  if (still) {
    state.hatSpektrumstrom = false;
    // Auch den gemessenen Rauschabstand vergessen. Er ist geglaettet
    // (0,9/0,1) und braeuchte sonst nach dem Wiederkommen rund zwei Sekunden,
    // um sich vom alten Wert zu loesen — in denen die Fusszeile den Hinweis
    // "nur rauschen" zeigen koennte, waehrend laengst Stationen da sind, oder
    // umgekehrt. Eine Messung, die niemand mehr vornimmt, gehoert verworfen
    // und nicht fortgeschrieben; dieselbe Regel wie beim S-Meter.
    state.hfAbstand = null;
    state.wfBoden = null;
  }
  if (still) {
    panCtx.fillStyle = PAN_BG; panCtx.fillRect(0, 0, W, H);
    wfCtx.fillStyle = WF_BG;  wfCtx.fillRect(0, 0, wf.width, wf.height);
    panCtx.fillStyle = '#7e7e85';
    panCtx.font = '11px -apple-system,system-ui,sans-serif';
    panCtx.textAlign = 'center';
    panCtx.fillText(link.ready ? 'kein Funkgerät verbunden' : 'warte auf Longpath…',
                    W / 2, H / 2 + 4);
    panCtx.textAlign = 'left';
    hatSpektrum = false;
    return;
  }

  // Selbsttaetige Hoehenlage: der Boden auf das 10. Hundertstel, die Decke
  // ueber die Spitze. Sonst muesste der Bediener am Handy Regler suchen.
  // Zwei Quellen, ein Bild: kommt das Spektrum fertig vom Server, ist es
  // schon auf unsere Breite verdichtet; rechnen wir selbst, liegen hier die
  // eigenen FFT-Bins. Die Wahl muss VOR der Höhenlage stehen — sie rechnet
  // schon damit.
  const quelle = state.hatSpektrumstrom ? specServer : glatt;
  const M = quelle.length;
  if (M === 0) { return; }

  // Boden aus dem Mittelwert (das Grundrauschen), Decke aus der Spitze —
  // nicht Minimum/Maximum, sonst bestimmt ein einzelner Einbruch den Boden.
  let summe = 0, max = -Infinity;
  for (let i = 0; i < M; i++) { summe += quelle[i]; if (quelle[i] > max) max = quelle[i]; }
  const min = summe / M;
  state.specMin = state.specMin * 0.9 + (min - 6) * 0.1;
  state.specMax = state.specMax * 0.9 + (max + 8) * 0.1;
  const lo = state.specMin, hi = Math.max(state.specMax, lo + 20);
  const y = (db) => H - ((db - lo) / (hi - lo)) * H;

  panCtx.fillStyle = PAN_BG; panCtx.fillRect(0, 0, W, H);
  panCtx.strokeStyle = 'rgba(138,143,150,.16)'; panCtx.lineWidth = 1;
  for (let i = 1; i < 6; i++) { const yy = Math.round(H*i/6)+.5;
    panCtx.beginPath(); panCtx.moveTo(0, yy); panCtx.lineTo(W, yy); panCtx.stroke(); }
  for (let i = 1; i < 8; i++) { const xx = Math.round(W*i/8)+.5;
    panCtx.beginPath(); panCtx.moveTo(xx, 0); panCtx.lineTo(xx, H); panCtx.stroke(); }

  // Auf die Bildpunkte bringen. Kommt das Spektrum fertig vom Server, ist es
  // schon auf unsere Breite verdichtet und das hier streckt nur noch; rechnen
  // wir selbst, verdichtet es 1024 Bins — über den Spitzenwert, sonst
  // rutschen schmale Träger durch.
  // ── Hoehenlage des Wasserfalls: Rauschboden, nicht Mittelwert ──────────
  //
  // Der Wasserfall sass bisher auf `lo`, und `lo` kommt aus dem MITTELWERT
  // des Spektrums. Auf einem belebten Band zieht jeder starke Traeger diesen
  // Mittelwert nach oben; der echte Rauschboden liegt dann darunter, und
  // eine leise Station ein paar dB ueber dem Rauschen landet trotzdem im
  // schwarzen Teil der Rampe. Betreiber am 2026-10-02: "display unten sehr
  // dunkel, sieht man sehr schwer ob eine station hier ist oder nicht".
  //
  // Ein unteres Perzentil trifft das Rauschen auch dann noch, wenn das halbe
  // Band belegt ist — ein Mittelwert nie. Darauf setzt die Rampe auf, und
  // alles darueber bekommt sofort Farbe. 42 dB statt 55 dazu: der laute
  // Traeger darf ruhig ausbrennen, die leise Station muss man sehen.
  const sortiert = Float32Array.from(quelle).sort();
  const p20 = sortiert[Math.floor(M * 0.20)];
  state.wfBoden = (state.wfBoden === null) ? p20 : state.wfBoden * 0.88 + p20 * 0.12;

  // ── Wie weit ragt das Stärkste über das Rauschen? ──────────────────────
  //
  // Weil der Wasserfall jetzt auf dem GEMESSENEN Rauschboden sitzt, malt er
  // auch reines Rauschen bunt und strukturiert. Das ist beim Hören richtig —
  // aber am 2026-10-03 hat der Betreiber daraus geschlossen, es komme HF an,
  // während in Wahrheit die Antenne fehlte: 7 dB Abstand auf 40 m am Morgen.
  // Früher wäre das schwarz geblieben und hätte für sich gesprochen.
  //
  // Also sagt die Seite es jetzt selbst. Geglättet, weil ein einzelnes Bild
  // zappelt und eine flackernde Warnung schlimmer ist als keine.
  const spitzeDb = sortiert[M - 1];
  const abstandJetzt = spitzeDb - p20;
  state.hfAbstand = (state.hfAbstand === null)
      ? abstandJetzt : state.hfAbstand * 0.9 + abstandJetzt * 0.1;

  const spitze = new Float32Array(W);
  for (let x = 0; x < W; x++) {
    const von = Math.floor(x * M / W);
    const bis = Math.max(von + 1, Math.floor((x + 1) * M / W));
    let m = -Infinity;
    for (let i = von; i < bis && i < M; i++) if (quelle[i] > m) m = quelle[i];
    spitze[x] = m;
  }

  const g = panCtx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(0,150,255,.62)'); g.addColorStop(1, 'rgba(0,70,190,.28)');
  panCtx.beginPath(); panCtx.moveTo(0, H);
  for (let x = 0; x < W; x++) panCtx.lineTo(x, y(spitze[x]));
  panCtx.lineTo(W, H); panCtx.closePath(); panCtx.fillStyle = g; panCtx.fill();

  panCtx.beginPath();
  for (let x = 0; x < W; x++) {
    const yy = y(spitze[x]);
    x ? panCtx.lineTo(x, yy) : panCtx.moveTo(x, yy);
  }
  panCtx.strokeStyle = '#8fd8ff'; panCtx.lineWidth = 1.5; panCtx.stroke();

  panCtx.fillStyle = '#9aa0a8'; panCtx.font = '9px ui-monospace,Menlo,monospace';
  for (let i = 0; i <= 3; i++) {
    const db = Math.round(hi - (hi - lo) * i / 3);
    panCtx.fillText(String(db), 3, 10 + i * (H - 14) / 3);
  }

  // ── Spots ───────────────────────────────────────────────────────────────
  //
  // Messing, weil ein Spot eine MELDUNG ist und keine Messung des eigenen
  // Empfaengers -- aber gedaempft, damit er die Kurve nicht uebertoent. Alte
  // Spots werden blasser (spots.js: deckung); sie verschwinden nicht, das
  // entscheidet Longpath anhand der Lebenszeit.
  //
  // Die Marken werden HIER gerechnet und in `spotMarken` abgelegt, weil der
  // Fingerzeiger dieselben Stellen braucht. Zweimal rechnen hiesse: zwei
  // Rechnungen, die auseinanderlaufen koennen, und dann trifft der Finger
  // etwas anderes, als das Auge sieht.
  {
    const mitteSpot = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : 0;
    spotMarken = mitteSpot
      ? marken(spotZeilen, mitteSpot, bildSpanneHz(192000), W)
      : [];
    if (spotMarken.length) {
      panCtx.font = '9px ui-monospace,Menlo,monospace';
      panCtx.textAlign = 'center';
      // ── Wohin die Beschriftung darf ──────────────────────────────────
      //
      // Am Telefonformat gemessen (2026-10-05): der Panadapter ist 96 px
      // hoch, und der Schrittregler deckt davon oben rechts 118 x 34 ab.
      // Er ist ein HTML-Element UEBER dem Canvas -- das Zeichnen merkt
      // nichts davon, die Marke ist einfach weg. Unten ist auch nichts
      // gewonnen: dort laeuft die Kurve.
      //
      // Also oben, und unter dem Regler ausweichen. Das Rechteck wird
      // GELESEN statt angenommen: eine Zahl, die hier steht und sich dort
      // aendert, ist genau die Art Fehler, die niemandem auffaellt.
      const rPan = pan.getBoundingClientRect();
      let sperrVonX = Infinity, sperrBisY = 0;
      if (rPan.width) {
        const el = document.querySelector('.feinschritt');
        const rr = el ? el.getBoundingClientRect() : null;
        if (rr && rr.width) {
          const k = pan.width / rPan.width;
          sperrVonX = (rr.left - rPan.left) * k;
          sperrBisY = (rr.bottom - rPan.top) * k;
        }
      }
      // Wie hoch die Schrift ueber ihrer Grundlinie steht -- GEMESSEN, nicht
      // geschaetzt. `fillText` zeichnet NACH OBEN von der Grundlinie weg;
      // wer die Grundlinie auf 4 setzt, schiebt den halben Buchstaben aus
      // dem Bild. Genau das ist am 2026-10-05 am Telefon passiert: "die
      // Spots stehen sehr weit oben, so kann man diese nicht komplett
      // sehen". Im Werkzeugbrowser fiel es nicht auf, weil dort mehr Platz
      // ueber dem Panadapter ist.
      const mass = panCtx.measureText('OE5SOS');
      const hoch = Math.ceil(mass.actualBoundingBoxAscent || 7);
      const ersteZeile = hoch + 2;         // zwei Punkte Luft zum Rand
      for (const m of spotMarken) {
        const unterDemRegler = (m.x + 27) > sperrVonX;
        const oben = unterDemRegler ? sperrBisY + ersteZeile : ersteZeile;
        const y = oben + m.reihe * 11;       // Schriftgrundlinie
        panCtx.globalAlpha = m.deckung;
        panCtx.strokeStyle = '#c2924f';
        panCtx.lineWidth = 1;
        panCtx.beginPath();
        panCtx.moveTo(m.x + .5, y + 2);
        panCtx.lineTo(m.x + .5, H);
        panCtx.stroke();
        // Der Schriftzug bekommt einen dunklen Grund, sonst verschwindet er
        // ueber einem hellen Traeger.
        const b = panCtx.measureText(m.ruf).width + 6;
        // Den Kasten festhalten, in DENSELBEN Einheiten, in denen gezeichnet
        // wird. Der Finger prueft spaeter dagegen -- eine zweite Rechnung
        // waere eine zweite Wahrheit.
        m.kasten = { x: m.x - b / 2, y: y - hoch - 1, b, h: hoch + 3 };
        panCtx.globalAlpha = m.deckung * .72;
        panCtx.fillStyle = '#0b0d10';
        // Der dunkle Grund folgt derselben Messung statt einer festen 8.
        panCtx.fillRect(m.x - b / 2, y - hoch - 1, b, hoch + 3);
        panCtx.globalAlpha = m.deckung;
        panCtx.fillStyle = '#d8a55f';
        panCtx.fillText(m.ruf, m.x, y);
      }
      panCtx.globalAlpha = 1;
      panCtx.textAlign = 'left';
    }
  }

  // Raumbild: dieselben Zeilen, nur anders gezeigt. Die Spanne ist die
  // des Wasserfalls, damit beide Ansichten dasselbe Band zeigen.
  if (raumZeigen && raumbild) {
    const rLo = state.wfBoden - 3, rHi = rLo + 42;
    while (neueZeilen > 0) {
      neueZeilen--;
      raumbild.zeileAufnehmen(spitze.subarray ? spitze.subarray(0, W) : spitze);
    }
    raumbild.malen(wfCtx, wf.width, wf.height, {
      bodenDbm: rLo,
      spanneDb: rHi - rLo,
      zKurve: 0.7,
      palette: (db) => rampe((db - rLo) / (rHi - rLo)),
      grund: rampe(0),
    });
    return;
  }

  // Wasserfall: je eingetroffenem Spektrum eine Zeile — nicht je Bild.
  // Der Zwischenpuffer ist noetig, weil ein Canvas, der sich selbst als Quelle
  // zeichnet, nicht verlaesslich schiebt.
  while (neueZeilen > 0) {
    neueZeilen--;
    wfPuffCtx.clearRect(0, 0, wf.width, wf.height);
    wfPuffCtx.drawImage(wf, 0, 0);
    wfCtx.clearRect(0, 0, wf.width, wf.height);
    wfCtx.drawImage(wfPuff, 0, 1);
    // Der Wasserfall bekommt eine EIGENE, engere Spanne. Sobald der Server
    // echte dBm liefert, reicht die Panadapter-Spanne über 85 dB — ein
    // Träger 30 dB über dem Rauschen läge dann im dunklen Drittel der Rampe
    // und wäre kaum zu sehen. Am Pult haben Panadapter und Wasserfall aus
    // demselben Grund getrennte Regler; hier nehmen wir den Rauschboden plus
    // 55 dB, was in der Praxis vom Grundrauschen bis zum lauten Träger reicht.
    const wfLo = state.wfBoden - 3, wfHi = wfLo + 42;
    const d = zeile.data;
    for (let x = 0; x < wf.width; x++) {
      const db = spitze[Math.min(W - 1, Math.floor(x * W / wf.width))];
      const c = rampe((db - wfLo) / (wfHi - wfLo));
      const o = x * 4;
      d[o] = c[0]; d[o+1] = c[1]; d[o+2] = c[2]; d[o+3] = 255;
    }
    wfCtx.putImageData(zeile, 0, 0);
  }
}

// Der AF-Regler des Telefons — mit VERSTAERKUNGSVORRAT.
//
// Bis 2026-09-30 war er `afPct/100`, also 0 bis 1,0: ein reiner Daempfer
// ohne jede Reserve. Das ist fuer eine Fernbedienung der falsche Bereich,
// denn der Ton kommt schon dem AF-Regler AM RECHNER unterworfen an — der
// TCI-Abgriff sitzt hinter WDSPs panel.gain1 (RXA.c:698). Steht der Regler
// am Mac leise, weil der Bediener dort gerade nicht zuhoert, kommt das
// Telefon leise an und konnte nichts dagegen tun.
//
// Jetzt: 70 % bleibt Faktor 1,0 (die gewohnte Stellung klingt wie bisher),
// darunter wird gedaempft, darueber bis Faktor 4 (+12 dB) verstaerkt. Der
// Bereich oberhalb ist bewusst gespreizt, damit das letzte Stueck Weg nicht
// in einem Sprung liegt.
function afFaktor(pct) {
  if (pct <= 70) { return pct / 70; }                 // 0 … 1,0
  return 1 + (pct - 70) / 30 * 3;                     // 1,0 … 4,0
}

// ── Ton ─────────────────────────────────────────────────────────────────────
// iOS gibt Ton erst nach einer Beruehrung frei. Wir versuchen es bei jeder
// Beruehrung erneut, bis es klappt — ohne einen eigenen Knopf dafuer.
// ── Den Stummschalter aushebeln (2026-10-01) ────────────────────────────────
//
// iOS stuft Ton aus einer Webseite als "Ambient" ein, und diese Kategorie
// gehorcht dem kleinen Schieber an der Seite des Telefons: steht er auf
// stumm, bleibt der eingebaute Lautsprecher still — WEB-AUDIO EINGESCHLOSSEN.
// Kopfhoerer sind davon nicht betroffen, iOS gibt dort trotzdem aus.
//
// Genau dieses Bild am 2026-10-01: alle Zahlen richtig (Rahmen kamen an, der
// Puffer war voll, der Tonknoten lief, der Regler stand auf 1,0), nichts zu
// hoeren — aber "mit ohrstöpsel geht es". Von aussen sieht das aus wie ein
// Fehler im Programm und ist keiner.
//
// Spielt die Seite ein <audio>-Element ab, wechselt iOS in die Kategorie
// "Playback", und die ignoriert den Schieber. Das Element traegt eine
// Sekunde Stille und laeuft in Schleife; es ist nicht zu hoeren und kostet
// nichts.
//
// Der Aufruf muss aus einer Beruehrung kommen — deshalb steht er in
// tonStarten() und nicht irgendwo beim Laden.
/** Sagt iOS, dass hier WIEDERGABE stattfindet — nicht Beiwerk.
 *
 *  Ohne diese Ansage legt Safari den Ton in die Kategorie "ambient", und die
 *  gehorcht dem Stummschalter am Geraet: ueber Kopfhoerer hoert man alles,
 *  ueber den Lautsprecher nichts. Genau das war am 2026-10-03 der Fall, und
 *  es war von aussen nicht zu erkennen — die Seite meldete eine tadellose
 *  Kette:
 *
 *      ctx=running  vorrat=3410  ziel=1440  takte=216
 *      af=1  vorDeckel=-33  nachDeckel=-32
 *
 *  Also voller Pegel direkt vor dem Ausgang. Der Ton verliess die Seite und
 *  wurde erst vom Betriebssystem verworfen.
 *
 *  navigator.audioSession gibt es seit iOS 16.4. Wo es fehlt, bleibt der
 *  alte Behelf darunter.
 */
function wiedergabeSitzungSetzen() {
  try {
    if (navigator.audioSession) {
      navigator.audioSession.type = 'playback';
      return true;
    }
  } catch (e) { /* dann eben der Behelf */ }
  return false;
}

/** Der alte Behelf fuer iOS vor 16.4: eine Sekunde Stille in Schleife.
 *
 *  Hier stand `el.volume = 0`, und das war der Fehler — ein Element mit
 *  Lautstaerke null gilt Safari nicht als Wiedergabe und verschiebt die
 *  Kategorie darum NICHT. Die Datei ist ohnehin Stille; die Lautstaerke muss
 *  nicht zusaetzlich auf null stehen, damit nichts zu hoeren ist. Der
 *  Kommentar daneben hat es die ganze Zeit zugegeben ("ohne geht es eben nur
 *  mit Hoerer") — nur las es niemand als Fehlerbeschreibung.
 */
function stummesElementStarten() {
  const el = $('stillhalter');
  if (!el) { return; }
  try {
    const p = el.play();
    if (p && p.catch) { p.catch(() => { /* dann bleibt es beim Hoerer */ }); }
  } catch (e) { /* desgleichen */ }
}

async function tonStarten() {
  // Riegel gegen Doppelstart. `state.node` allein reicht NICHT: zwischen der
  // Pruefung und dem Setzen liegen zwei await-Stellen, und ein einziger Tipp
  // loest ueber 'touchend' UND 'click' zwei Laeufe aus. Ergebnis waren
  // zuverlaessig zwei AudioContexts, von denen der erste nie wieder
  // geschlossen wurde (Durchsicht 2026-09-30).
  if (state.node || state.tonStartLaeuft) { return; }
  state.tonStartLaeuft = true;
  try {
    // ZUERST: iOS in die Playback-Kategorie bringen, BEVOR der AudioContext
    // entsteht. Danach gehorcht der Ton dem Stummschalter nicht mehr.
    // Erst die richtige Schnittstelle, dann der Behelf — beide schaden
    // einander nicht.
    state.sitzungGesetzt = wiedergabeSitzungSetzen();
    stummesElementStarten();

    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) { throw new Error('Dieser Browser kennt keinen AudioContext'); }
    // KEINE Abtastrate vorgeben. Die Rate ist eine Eigenschaft des
    // Ausgabegeraets, nicht unsere Wahl: iOS liefert je nach Hoerer 48000,
    // 44100 oder bei Bluetooth auch 16000, und eine abweichende Vorgabe
    // wird dort teils ignoriert, teils schlaegt die Erzeugung fehl. Wir
    // brauchen sie ohnehin nicht — TonKern rechnet auf ctx.sampleRate um,
    // was immer das ist.
    const ctx = new AC({ latencyHint: 'interactive' });

    // SOFORT aufwecken, noch im Fingertipp — nicht erst am Ende.
    //
    // Weiter unten steht `await ctx.resume()`, und dorthin fuehren zwei
    // `await ctx.audioWorklet.addModule(...)`. iOS raeumt die Berechtigung
    // eines Fingertipps aber mit dem ERSTEN await ab: was danach kommt, gilt
    // nicht mehr als vom Bediener ausgeloest, und `resume()` wird stillschweigend
    // nicht ausgefuehrt. Der Kontext bleibt `suspended`, die Tondaten laufen
    // in den Ring, der Vorrat steht am Anschlag — und zu hoeren ist nichts.
    // Genau das meldete die Seite am 2026-10-02: `ctx=suspended`,
    // `rahmen=11490`, `vorrat=12000`. Betreiber: "ton geht nicht".
    //
    // Hier ist die Berechtigung noch da. Das Ergebnis wird nicht abgewartet
    // (ein await waere genau der Fehler, den diese Zeile vermeidet); das
    // `await ctx.resume()` unten bleibt als zweiter Versuch stehen.
    try { ctx.resume(); } catch (e) { /* zweiter Versuch kommt unten */ }

    const gain = ctx.createGain();
    gain.gain.value = afFaktor(state.afPct);

    let node = null;
    let weg = null;

    if (ctx.audioWorklet) {
      // Der gute Weg: eigener Audio-Faden, stottert nicht, wenn der
      // Hauptfaden den Wasserfall zeichnet.
      await ctx.audioWorklet.addModule('ton-kern.js');   // muss zuerst
      await ctx.audioWorklet.addModule('rx-worklet.js');
      node = new AudioWorkletNode(ctx, 'rx-worklet', {
        outputChannelCount: [2],
        // srcRate ist die ausgehandelte Rate, nicht die des Ausgangs — ohne
        // sie liefe der Ton bei 12 kHz Quelle viermal zu schnell.
        processorOptions: { srcRate: state.audioRate },
      });
      // Leerlauf zaehlen statt wegwerfen. Das Worklet meldet bei jedem 32.
      // leeren Block; ohne diese Zeile war die einzige Diagnose fuer
      // stotternden Ton ein leerer Rumpf, und der Leerlauf musste von Hand
      // ueber den Scheitelfaktor ausgeschlossen werden.
      node.port.onmessage = (e) => {
        if (e.data && e.data.type === 'starved') {
          state.tonLeerlauf = (state.tonLeerlauf || 0) + 32;
          state.tonLeerlaufZuletzt = performance.now();
        }
      };
      weg = 'worklet';
      state.tonImHauptfaden = false;
    } else {
      // ── Rueckfall: ScriptProcessorNode ───────────────────────────────────
      //
      // `AudioWorklet` ist [SecureContext]. Eine Seite, die ueber http von
      // einer LAN-Adresse kommt — also genau der Fall "Telefon im eigenen
      // Netz" —, ist KEIN sicherer Kontext, und dann ist ctx.audioWorklet
      // schlicht undefined. Am 2026-09-30 an http://172.30.30.121:8767
      // gemessen: isSecureContext false, audioWorklet undefined,
      // createScriptProcessor vorhanden.
      //
      // Der ScriptProcessorNode ist veraltet und laeuft im Hauptfaden, kann
      // also unter Last knacken. Fuer 12 kHz Sprache reicht er, und stumm
      // ist erheblich schlimmer als gelegentlich rauh. Sobald die Seite
      // ueber https ausgeliefert wird, greift von selbst wieder der obere
      // Zweig.
      const kern = new TonKern({ ausgabeRate: ctx.sampleRate, srcRate: state.audioRate });
      // 2048 Proben sind bei 48 kHz rund 43 ms — gross genug, dass der
      // Hauptfaden dazwischen zeichnen darf, klein genug, dass die
      // Verzoegerung nicht auffaellt.
      //
      // EIN Eingangskanal, nicht null. Ein ScriptProcessorNode ohne Eingang
      // wird nicht überall getaktet: manche Umsetzungen rufen
      // onaudioprocess nur, wenn etwas hineinfliesst, und Safari gehoert
      // dazu. Der Knoten haengt dann stumm im Graphen, und niemand sieht,
      // warum. Deshalb bekommt er unten eine stille Quelle vorgeschaltet —
      // sie liefert Nullen und dient nur dem Takt.
      state.tonImHauptfaden = true;
      const sp = ctx.createScriptProcessor(2048, 1, 2);
      let leerZaehler = 0;
      sp.onaudioprocess = (ev) => {
        state.tonTakte = (state.tonTakte || 0) + 1;
        const out = ev.outputBuffer;
        const voll = kern.zieh(out.getChannelData(0), out.getChannelData(1), out.length);
        // Fuellstand nach draussen melden: die Bildschleife laeuft auf
        // DEMSELBEN Faden wie dieser Rueckruf und muss ihm ausweichen
        // koennen, bevor es knackt. Zwei Zuweisungen je Block.
        state.tonVorrat = kern.have;
        state.tonZiel = kern.target;
        // Denselben Leerlauf melden wie das Worklet, damit die Fusszeile auf
        // beiden Wegen dasselbe sagt.
        if (!voll && !kern.muted && (++leerZaehler & 31) === 0) {
          state.tonLeerlauf = (state.tonLeerlauf || 0) + 32;
          state.tonLeerlaufZuletzt = performance.now();
        }
      };
      // Die stille Quelle, die den Takt garantiert (siehe oben). Ein
      // ConstantSourceNode mit offset 0 liefert Nullen, kostet nichts und
      // haelt den ScriptProcessor am Laufen.
      let takt = null;
      try {
        takt = ctx.createConstantSource();
        takt.offset.value = 0;
        takt.connect(sp);
        takt.start();
      } catch (e) {
        // Kennt der Browser ConstantSourceNode nicht, tut es auch ein
        // leerer Puffer in Dauerschleife.
        try {
          const leer = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
          const q = ctx.createBufferSource();
          q.buffer = leer; q.loop = true; q.connect(sp); q.start();
          takt = q;
        } catch (e2) { /* dann eben ohne — auf Chrome laeuft es auch so */ }
      }

      // Eine Huelle, die sich nach aussen wie der Worklet-Knoten verhaelt —
      // so kennt der Rest der Seite nur EINEN Weg.
      node = {
        _sp: sp, _kern: kern,
        connect: (z) => sp.connect(z),
        disconnect: () => { try { if (takt) takt.disconnect(); } catch (e) {} sp.disconnect(); },
        port: { postMessage: (m) => {
          if (m.type === 'pcm') { kern.push(m.data, m.channels); }
          else if (m.type === 'mute') { kern.muted = !!m.value; }
          else if (m.type === 'flush') { kern.leeren(); }
          else if (m.type === 'rate') { kern.setRate(m.value); }
        } },
      };
      weg = 'scriptprocessor';
    }

    // ── Begrenzer, damit der Verstärkungsvorrat nicht klirrt ──────────────
    //
    // Seit der AF-Regler bis Faktor 4 verstärkt, kann ein starkes Signal die
    // Vollaussteuerung überschreiten — die AGC des Empfängers liefert in der
    // Regelung Spitzen um 0,98, mal 4 sind das 3,9. Digitales Klirren klingt
    // scheußlich und wird für einen Fehler des Programms gehalten.
    //
    // Der Begrenzer greift erst bei -3 dBFS, arbeitet mit Verhältnis 20:1 und
    // ohne Kniebereich: unterhalb tut er nichts, oberhalb hält er. Das ist
    // kein Klangeingriff, sondern ein Deckel — Kompression im eigentlichen
    // Sinn (Dynamik verdichten) wäre eine Gestaltungsfrage und steht hier
    // ausdrücklich nicht.
    const deckel = ctx.createDynamicsCompressor();
    // Schwelle dicht unter Vollaussteuerung, nicht bei -3 dBFS.
    //
    // Bei -3 griff er auch in der Stellung 70 %, die ausdruecklich als
    // "klingt wie bisher" zugesagt ist: die AGC des Empfaengers liefert in
    // der Regelung Spitzen um 0,98 (-0,2 dBFS), und die liegen ueber -3.
    // Der Deckel zog damit jede Sprachspitze 20:1 zusammen — aus einem
    // Schutz gegen Klirren war unversehens ein Klangeingriff geworden.
    // Gefunden bei der Durchsicht der eigenen Reparatur (2026-09-30).
    //
    // Bei -1 dBFS bleibt die Eins-zu-eins-Stellung unangetastet (dort kann
    // nichts ueber 1,0 kommen, die Quelle ist ja begrenzt) und der Deckel
    // tut genau das, wofuer er da ist: er faengt ab, was der
    // Verstaerkungsvorrat darueber hinaustreibt.
    deckel.threshold.value = -1;
    deckel.knee.value = 0;
    deckel.ratio.value = 20;
    deckel.attack.value = 0.003;
    deckel.release.value = 0.25;

    node.connect(gain);
    gain.connect(deckel);
    deckel.connect(ctx.destination);

    // Zwei Horchposten, damit sich nachweisen laesst, WO ein Ton verschwindet:
    // einer am Regler (vor dem Begrenzer), einer am Ausgang. Die Seite misst
    // sich damit selbst und meldet es — auf einem Telefon ist das der einzige
    // Weg, an diese Zahlen zu kommen.
    try {
      state.mess1 = ctx.createAnalyser(); state.mess1.fftSize = 2048;
      state.mess1.smoothingTimeConstant = 0;
      gain.connect(state.mess1);
      state.mess2 = ctx.createAnalyser(); state.mess2.fftSize = 2048;
      state.mess2.smoothingTimeConstant = 0;
      deckel.connect(state.mess2);
    } catch (e) { /* ohne Messpunkte laeuft es trotzdem */ }
    await ctx.resume();

    state.audio = ctx; state.node = node; state.gain = gain;
    state.tonWeg = weg; state.tonFehler = null;
    // Fuer die Fehlersuche erreichbar (siehe window.__link oben).
    window.__audioCtx = ctx; window.__gain = gain; window.__node = node;
  } catch (e) {
    // Kein Ton ist kein Grund, die Bedienung zu verlieren — aber er darf
    // auch nicht STILL fehlen. Genau das war der Fall: ein console.warn, und
    // die Seite tat weiter, als liefe alles. Die gemessenen Datenraten haben
    // dann ankommende Bytes belegt, nicht hoerbaren Ton.
    state.tonFehler = (window.isSecureContext === false)
      ? 'kein Ton — die Seite laeuft ohne sicheren Kontext'
      : ('kein Ton — ' + (e && e.message ? e.message : e));
    console.warn('Ton nicht verfuegbar:', e);
  } finally {
    state.tonStartLaeuft = false;
  }
}
// Der sichtbare Weg: ein Knopf, der sagt, was er tut. Die Beruehrung
// irgendwo auf der Seite bleibt zusaetzlich — sie schadet nicht und hilft
// dem, der ohnehin tippt.
$('tonAn').addEventListener('click', async () => {
  await tonStarten();
  zeichneTonKnopf();
  // Nach dem Tippen melden: das ist der Augenblick, in dem sich
  // entscheidet, ob die Tonkette steht. Ohne diese Zeile muss man raten,
  // ob der Knopf gedrueckt wurde und was er bewirkt hat.
  setTimeout(() => melde('knopf'), 1500);
});
['touchend', 'click'].forEach(ev =>
  document.addEventListener(ev, async () => {
    // Fangnetz: steht ein fertiger Kontext still, hilft kein neuer Aufbau —
    // `tonStarten` steigt bei vorhandenem `state.node` sofort wieder aus.
    // Ein stiller Kontext braucht genau eines: ein `resume()` im Fingertipp.
    // Darum SYNCHRON und vor jedem await, sonst ist die Berechtigung weg.
    if (state.audio && state.audio.state !== 'running') {
      try { state.audio.resume(); } catch (e) { /* beim naechsten Tipp wieder */ }
    }
    await tonStarten(); zeichneTonKnopf();
  }, { passive: true }));

// Testton: ein Sinus durch DIESELBE Kette (Regler, Begrenzer, Ausgang).
// Hoert man ihn nicht, liegt es an der Kette oder am Geraet — hoert man ihn,
// liegt es an den Daten. Das trennt die beiden Faelle in einem Griff.
// Erreichbar ueber langes Tippen auf die Datenrate oben rechts.
async function testton() {
  await tonStarten();
  const C = state.audio;
  if (!C) { return; }
  try {
    const o = C.createOscillator();
    o.frequency.value = 700;
    const g = C.createGain();
    g.gain.value = 0.25;
    o.connect(g);
    // Bewusst am Regler VORBEI direkt an den Ausgang: so prueft der Ton die
    // Ausgabe des Geraets, nicht unsere Lautstaerkerechnung.
    g.connect(C.destination);
    o.start();
    setTimeout(() => { try { o.stop(); o.disconnect(); g.disconnect(); } catch (e) {} }, 2000);
    melde('testton');
  } catch (e) { /* nichts */ }
}
(() => {
  const r = $('rate');
  if (!r) { return; }
  let halt = null;
  const an = () => { halt = setTimeout(testton, 700); };
  const aus = () => { clearTimeout(halt); };
  ['touchstart', 'mousedown'].forEach(e => r.addEventListener(e, an, { passive: true }));
  ['touchend', 'touchcancel', 'mouseup', 'mouseleave'].forEach(e =>
    r.addEventListener(e, aus, { passive: true }));
})();

// Der Knopf steht da, solange kein Ton laeuft, und verschwindet danach.
function zeichneTonKnopf() {
  const k = $('tonAn');
  if (!k) { return; }
  const laeuft = !!state.node && state.audio && state.audio.state === 'running';
  k.hidden = laeuft;
  if (!laeuft) { k.textContent = state.tonFehler ? 'TON?' : 'TON EIN'; }
}
setInterval(zeichneTonKnopf, 1000);
zeichneTonKnopf();

link.addEventListener('audio', (e) => {
  if (!state.node) return;
  // Kanalzahl aus dem Rahmen ableiten, nicht raten: wir bitten um mono, aber
  // ein Server darf stereo schicken.
  const kanaele = e.detail.channels || 1;
  state.node.port.postMessage(
    { type: 'pcm', data: e.detail.vals, channels: kanaele }, [e.detail.vals.buffer]);
});

// ── Regler ──────────────────────────────────────────────────────────────────
function reglerBinden(trackId, beiWert) {
  const track = $(trackId);
  const setzen = (ev) => {
    const r = track.getBoundingClientRect();
    const x = (ev.touches ? ev.touches[0].clientX : ev.clientX) - r.left;
    beiWert(Math.max(0, Math.min(100, Math.round(x / r.width * 100))));
  };
  track.addEventListener('pointerdown', (e) => { track.setPointerCapture(e.pointerId); setzen(e); });
  track.addEventListener('pointermove', (e) => { if (track.hasPointerCapture(e.pointerId)) setzen(e); });
}
reglerBinden('afTrack', (p) => {
  state.afPct = p;
  if (state.gain) state.gain.gain.value = afFaktor(p);
  zeichneBedienung();
});
reglerBinden('pwrTrack', (p) => link.send(`drive:${state.trx},${p}`));

// ── Abstimmen durch Wischen im Spektrum ─────────────────────────────────────
// Eine Wischbewegung verschiebt die Frequenz um so viele Hertz, wie sie
// Bildpunkte zuruecklegt — bezogen auf die tatsaechliche Abtastrate.
let wischVon = null, wischHz = 0, wischAktiv = false, wischLetzt = 0, wischZiel = null;

// Ab wie vielen Bildpunkten ein Wisch als Wisch gilt. Darunter war es ein
// Tipp, und ein Tipp darf die Frequenz NICHT verstellen: ein Daumen trifft
// nie auf den Punkt genau, und bis 2026-09-30 verstimmte jede Beruehrung des
// Wasserfalls das Geraet (Durchsicht, 3/3 bestaetigt).
const kWischTotzone = 10;
// Nicht bei jedem Bewegungsereignis senden. Ein Telefon liefert 60 bis 120
// davon je Sekunde, und jedes war ein eigener vfo-Befehl ueber die Leitung —
// beim langsamen Ziehen ueber das Band waren das Hunderte in Folge.
const kWischAbstandMs = 60;

$('scope').addEventListener('pointerdown', (e) => {
  wischVon = e.clientX; wischHz = link.st.vfo[state.trx][0];
  wischAktiv = false; wischLetzt = 0; wischZiel = null;
  $('scope').setPointerCapture(e.pointerId);
});
$('scope').addEventListener('pointermove', (e) => {
  if (wischVon === null) return;
  const weg = e.clientX - wischVon;
  if (!wischAktiv) {
    if (Math.abs(weg) < kWischTotzone) return;
    wischAktiv = true;
    // Das Bild festhalten, solange der Finger unten ist.
    //
    // Longpath fuehrt die Bildmitte der Abstimmung nach
    // (RadioModel.cpp: setCenterFrequency(slice->frequency())). Das Bild
    // wandert also MIT, die Mitte bleibt die Mitte — und sichtbar aendert
    // sich nur die Zahl. Betreiber am 2026-10-01: "beim wischen nach rechts
    // und links ändert sich nur die frequenz".
    //
    // Was man beim Abstimmen sehen will, ist das Gegenteil: ein stehendes
    // Bild, durch das der Abstimmstrich faehrt, damit man sieht, WOHIN man
    // faehrt. Solange der Finger unten ist, wird deshalb kein neues Bild
    // angenommen und der Strich wandert stattdessen. Beim Loslassen laeuft
    // es normal weiter.
    state.bildHalten = true;
  }
  // Mit der GEZEIGTEN Spanne rechnen, nicht mit der vollen DDC-Breite.
  // Vorher stand hier immer iqRate (192 kHz): bei 373 Punkten waren das
  // 515 Hz je Bildpunkt, und die Frequenz sprang beim Abstimmen in
  // Halbkilohertz-Schritten. Betreiber am 2026-10-01: "frequenz kann man
  // zwar ändern, aber sehr schlecht".
  const spanne = bildSpanneHz(192000);
  const proPixel = spanne / $('scope').getBoundingClientRect().width;
  // Das Raster folgt der Spanne: wer eng zoomt, will auch fein abstimmen.
  // 10 Hz bei schmaler Sicht, 100 Hz bei breiter — sonst zappelt die
  // Anzeige, ohne dass man das Signal trifft.
  const raster = spanne <= 24000 ? 10 : (spanne <= 96000 ? 50 : 100);
  // PLUS, nicht minus: der Finger zieht den ZEIGER, nicht das Band.
  //
  // Solange das Bild mitwanderte, galt die Karten-Konvention — Finger nach
  // rechts schob das Band nach rechts, man fuhr also abwaerts. Seit das Bild
  // beim Abstimmen steht (bildHalten), zieht man den Zeiger durch ein
  // stehendes Spektrum, und dann muss er dem Finger folgen: rechts ist
  // rechts, und rechts sind die hoeheren Frequenzen.
  //
  // Betreiber am 2026-10-01: "zeiger geht genau seitenverkehrt". Ich hatte
  // beim Umbau die alte Richtung stehen lassen.
  const neu = Math.round((wischHz + weg * proPixel) / raster) * raster;
  wischZiel = neu;
  // Wie weit der Strich vom Bildmittelpunkt weg ist, in Bildpunkten.
  state.wischVersatzPx = weg;
  const jetzt = performance.now();
  if (jetzt - wischLetzt < kWischAbstandMs) return;
  wischLetzt = jetzt;
  link.send(`vfo:${state.trx},0,${neu}`);
});
// ── Kneifen zum Vergroessern (2026-10-01) ───────────────────────────────────
//
// Betreiber: "vergrössern kann ich leider nicht". Es gab schlicht keinen Weg.
//
// Die Spanne wird am SERVER beschnitten (spectrum_start, vierter Wert), nicht
// hier — nur so steigt die Aufloesung wirklich. Schnitte der Browser zu,
// malte er dieselben groben Punkte nur breiter.
const kSpannen = [6000, 12000, 24000, 48000, 96000, 192000];
let kneifVon = 0;        // Fingerabstand beim Beginn der Geste
let kneifSpanne = 0;     // Spanne beim Beginn der Geste
const zeiger = new Map();

function spanneSetzen(hz) {
  const neu = kSpannen.reduce((a, b) =>
    Math.abs(b - hz) < Math.abs(a - hz) ? b : a);
  if (neu === state.spanneHz) { return; }
  state.spanneHz = neu;
  link.send(`spectrum_start:${state.trx},${pan.width},12,${neu}`);
  // Nach einem Zoom decken dieselben Zeilen eine andere Bandbreite ab.
  // Schieben hilft da nicht — ein Schnitt sagt, ab wo der neue Massstab
  // gilt, und laesst das Alte sichtbar hinauswandern.
  wasserfallSchnitt();
  zeichneKopf();
}

// Kneifen gibt es seit dem 2026-10-01 — aber eine Geste, die man nicht sieht,
// gibt es fuer den Bediener nicht: er hat am 2026-10-02 erneut nach dem
// Vergroessern gefragt. Zwei Tasten neben der Spannenanzeige sagen es selbst.
function spanneStufe(richtung) {
  const i = kSpannen.indexOf(state.spanneHz);
  const j = (i < 0 ? kSpannen.indexOf(48000) : i) + richtung;
  if (j < 0 || j >= kSpannen.length) { return; }
  spanneSetzen(kSpannen[j]);
}
$('zoomRein').addEventListener('click', () => spanneStufe(-1));
$('zoomRaus').addEventListener('click', () => spanneStufe(+1));

// ── Feinschritt +/- 100 Hz, rechts oben im Panadapter (2026-10-02) ─────────
//
// Betreiber: "vielleicht sollte ein plus und minus oben rechts im panadapter
// sein um die frequenz um +/- 100 zu verschieben". Wischen trifft auf ein
// paar hundert Hertz genau — fuer das letzte Stueck auf eine Station ist das
// zu grob, und das Eintippblatt ist dafuer zu umstaendlich.
const kFeinHz = 100;
function feinschritt(d) {
  const v = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
  if (!v) { return; }
  link.send(`vfo:${state.trx},0,${v + d}`);
  // Nicht selbst schieben — mitteVerfolgen() tut es, sobald das Geraet die
  // neue Frequenz bestaetigt. Zweimal schieben hiesse doppelt schieben.
}
$('feinAb').addEventListener('click', () => feinschritt(-kFeinHz));
$('feinAuf').addEventListener('click', () => feinschritt(+kFeinHz));
// Die Tasten liegen IM Bild. Ohne das hier begaenne jeder Druck zugleich
// einen Abstimmwisch darunter, und das Bild fröre fuer die Dauer ein.
['pointerdown', 'pointerup', 'pointermove'].forEach(ev =>
  $('feinschritt').addEventListener(ev, (e) => e.stopPropagation()));

$('scope').addEventListener('pointerdown', (e) => {
  zeiger.set(e.pointerId, e.clientX);
  if (zeiger.size === 2) {
    const [a, b] = [...zeiger.values()];
    kneifVon = Math.abs(a - b);
    kneifSpanne = state.spanneHz > 0 ? state.spanneHz : (link.st.iqRate || 48000);
    wischVon = null;              // kein Abstimmen waehrend des Kneifens
    wischAktiv = false;
  }
}, { passive: true });

$('scope').addEventListener('pointermove', (e) => {
  if (!zeiger.has(e.pointerId)) { return; }
  zeiger.set(e.pointerId, e.clientX);
  if (zeiger.size !== 2 || !kneifVon) { return; }
  const [a, b] = [...zeiger.values()];
  const jetzt = Math.abs(a - b);
  if (jetzt < 20) { return; }

  // Bezug ist der ANFANG der Geste, nicht das vorige Ereignis.
  //
  // Hier stand `spanneSetzen(state.spanneHz * (kneifVon / jetzt))` und
  // danach `kneifVon = jetzt`. Damit war das Verhaeltnis bei jedem
  // Mausbericht rund 1,001 — die gerechnete Spanne lag also immer dicht an
  // der aktuellen, rastete auf denselben Wert ein, und `spanneSetzen` stieg
  // bei `neu === state.spanneHz` sofort wieder aus. Weil der Bezugspunkt
  // zugleich nachgezogen wurde, konnte sich auch nichts aufsummieren: die
  // Geste konnte RECHNERISCH nie etwas bewirken, egal wie weit die Finger
  // auseinandergingen. Betreiber am 2026-10-02: "man sollte mit beiden
  // finger auseinanderziehen alles vergroessern koennen".
  //
  // Mit festem Anfangsbezug waechst das Verhaeltnis mit der Geste, und die
  // naechste Raststufe wird erreicht, sobald die Finger weit genug sind.
  //
  // Auseinander = naeher heran = kleinere Spanne.
  spanneSetzen(kneifSpanne * (kneifVon / jetzt));
}, { passive: true });

['pointerup', 'pointercancel', 'pointerleave'].forEach(ev =>
  $('scope').addEventListener(ev, (e) => {
    zeiger.delete(e.pointerId);
    if (zeiger.size < 2) { kneifVon = 0; kneifSpanne = 0; }
  }, { passive: true }));

// Untergrenze knapp unter 160 m, Obergrenze knapp ueber 23 cm. Was
// ausserhalb liegt, ist ein Tippfehler und kein Wunsch — lieber nichts tun
// als das Funkgeraet irgendwohin schicken.
const kQsyMin = 1600000, kQsyMax = 1300000000;

// ── Frequenz eintippen (2026-10-02) ─────────────────────────────────────────
//
// Betreiber: "man sollte durch anklicken der frequenz die genaue frequenz
// aendern koennen". Wischen und Tippen ins Bild treffen auf ein paar hundert
// Hertz genau — fuer eine verabredete Frequenz ist das nichts.
//
// Die Eingabe nimmt alles an, was ein Funker hinschreibt, und raet NICHT:
//   7.134.600  Punkte als Tausender (so steht es am Pult)  -> Hertz
//   7134.6     eine Zahl mit Rest                          -> Kilohertz
//   7.1346     kleine Zahl                                 -> Megahertz
// Mehr als ein Punkt kann kein Komma sein, also sind es Tausender. Bleibt
// ein einzelner Punkt, entscheidet die Groesse — eindeutig, weil kein
// Amateurband bei 7 kHz und keines bei 7 MHz ... als Kilohertz gelesen in
// Reichweite liegt.
function hzAusEingabe(roh) {
  const t = (roh || '').trim().replace(/\s|'/g, '').replace(/,/g, '.');
  if (!t || !/^[0-9.]+$/.test(t)) { return null; }

  // Mehr als ein Punkt kann kein Komma sein — das sind Tausendertrenner, wie
  // sie am Pult stehen. Dann ist die Zahl schon in Hertz und es gibt nichts
  // zu raten.
  if ((t.match(/\./g) || []).length > 1) {
    const hz = parseInt(t.replace(/\./g, ''), 10);
    return (hz >= kQsyMin && hz <= kQsyMax) ? hz : null;
  }

  const z = parseFloat(t);
  if (!isFinite(z) || z <= 0) { return null; }

  // Sonst: die ERSTE Deutung nehmen, die auf einem Funkband landen kann.
  //
  // Eine Schwelle auf die blosse Groesse reicht nicht. Der erste Versuch hier
  // las alles ab 100000 als Hertz — damit wurde aus "144300" (jeder meint
  // 144,300 MHz) sang- und klanglos 144 Kilohertz. Umgekehrt muss "7134600"
  // Hertz bleiben. Beides zugleich kann keine feste Grenze, die Reihenfolge
  // Hz -> kHz -> MHz dagegen schon: sie trifft genau eine davon, weil die
  // anderen weit ausserhalb jedes Bandes liegen.
  for (const faktor of [1, 1000, 1e6]) {
    const hz = Math.round(z * faktor);
    if (hz >= kQsyMin && hz <= kQsyMax) { return hz; }
  }
  return null;
}

// ── Spots im Bild (2026-10-05) ────────────────────────────────────────────
//
// Longpath kennt Cluster, RBN und POTA; die App zeigte davon nichts. Mit
// `spots:<rx>` liefert es die Spots im SICHTBAREN Ausschnitt -- gefiltert
// dort, nicht hier: im SpotModel liegen auch die Hunderte, die RBN in einer
// guten Stunde meldet, und die wuerden dieselbe Steuerleitung verstopfen,
// die Frequenz, Betriebsart und den Abbruch eines Sendewunsches traegt.
//
// EINMALIGE ABFRAGE, kein Abonnement. Gefragt wird beim Verbinden, danach
// alle paar Sekunden, und nach einem Frequenzwechsel einmal zusaetzlich --
// dann hat sich der Ausschnitt verschoben und die alten Marken sitzen falsch.
const spotSammler = new Sammelstelle();
let spotZeilen = [];      // zuletzt vollstaendig empfangene Liste
let spotMarken = [];      // daraus gerechnet, in Canvas-Einheiten
let spotFrist = null;

/** Alle so viele Millisekunden nachfragen, solange die Verbindung steht. */
const kSpotTakt = 12000;
/** Nach einem Frequenzwechsel: nicht sofort, sonst fragt jeder Zwischenstand
 *  beim Drehen des Knopfes eine eigene Liste an. */
const kSpotNachQsy = 700;

function spotsHolen() {
  if (!link.ready) { return; }
  spotSammler.beginnen();
  link.send(`spots:${state.trx}`);
}

function spotsBaldHolen(verzug = kSpotNachQsy) {
  clearTimeout(spotFrist);
  spotFrist = setTimeout(spotsHolen, verzug);
}

link.addEventListener('spotzeile', (e) => {
  spotSammler.zeile(spotLesen((e.detail || {}).args || []));
});

link.addEventListener('spotsende', (e) => {
  const d = e.detail || {};
  if (d.anzahl === null || d.anzahl === undefined) {
    // `spots_err:` heisst "kann ich gerade nicht", nicht "da ist nichts".
    // Die alten Marken stehen lassen waere falsch (sie koennten zu einem
    // anderen Ausschnitt gehoeren), eine leere Liste zeigen auch -- aber
    // leer ist die ehrlichere von beiden: nichts zu behaupten.
    spotSammler.abschluss(-1);
    spotZeilen = [];
    return;
  }
  const liste = spotSammler.abschluss(d.anzahl);
  // null heisst: es fehlen Zeilen. Dann die alte Liste behalten statt eine
  // halbe zu zeigen -- eine kuerzere Liste sieht aus wie ein leereres Band.
  if (liste) { spotZeilen = liste; }
});

setInterval(() => { if (link.ready) { spotsHolen(); } }, kSpotTakt);

// ── QSO eintragen (Entwurf A, Betreiber 2026-10-04) ───────────────────────
//
// Das Blatt schickt NUR Rufzeichen und RST. Frequenz, Betriebsart und Zeit
// setzt Longpath selbst beim Eintragen (TciServer `log_qso:`), und das ist
// Absicht: schickte die Seite sie mit, koennte sie etwas anderes eintragen,
// als das Geraet gerade macht. Die drei Zeilen im Blatt zeigen darum, was
// Longpath nehmen WIRD — sie sind Auskunft, keine Eingabe, und stehen
// deshalb in Messing.
//
// Die Uhr laeuft, solange das Blatt offen ist. Ein Blatt, das die Zeit von
// seinem Oeffnen zeigt, waehrend der Kontakt eine Minute spaeter eingetragen
// wird, behauptet etwas Falsches — dieselbe Regel wie beim S-Meter.
let qsoUhr = null;

/** 14074000 Hz -> "14.074,00 kHz" — Punkt als Tausender, Komma als Dezimal. */
function khzText(hz) {
  const khz = hz / 1000;
  const ganz = Math.floor(khz);
  const rest = Math.round((khz - ganz) * 100);
  const mitPunkt = String(ganz).replace(/\B(?=(\d{3})+(?!\d))/g, '.');
  return `${mitPunkt},${String(rest).padStart(2, '0')} kHz`;
}

function qsoAutomatikZeigen() {
  const v = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
  const m = (link.st.mode[state.trx] || '').toUpperCase();
  // Von Hand gesetzt statt toLocaleString: das liefert je nach Browser ein
  // schmales geschuetztes Leerzeichen als Tausendertrennung, und eine
  // Frequenz mit Leerzeichen liest sich am Funkgeraet falsch. Punkt und
  // Komma wie ueberall sonst in dieser Seite.
  $('qsoFreq').textContent = v ? khzText(v) : '—';
  // ADIF kennt LSB/USB nicht als Betriebsart, sondern als Unterart von SSB.
  // Die Seite zeigt trotzdem, was am Geraet steht — umgerechnet wird erst
  // in Longpath, an einer Stelle.
  $('qsoMode').textContent = m || '—';
  const d = new Date();
  const zz = (n) => String(n).padStart(2, '0');
  $('qsoZeit').textContent =
    `${zz(d.getUTCDate())}.${zz(d.getUTCMonth() + 1)}.${d.getUTCFullYear()}  `
    + `${zz(d.getUTCHours())}:${zz(d.getUTCMinutes())}`;
}

// ── Ins Logbuch sehen (2026-10-04) ────────────────────────────────────────
//
// Zwei Fragen, zwei Befehle: `log_last:` fuer die letzten Kontakte,
// `log_dup:` fuer "hatte ich den schon?". Gesammelt und gedeutet wird in
// logbuch.js — hier steht nur, wann gefragt wird und wo es landet.
//
// WARUM ENTPRELLT: `log_dup:` laeuft in Longpath ueber die ganze Datei
// (gemessen 3,8 ms bei 6,6 MB). Je Tastendruck waere das ein Durchlauf fuer
// jeden Zwischenstand eines Rufzeichens — acht Durchlaeufe fuer OE5SOS, und
// sieben Antworten, die niemand lesen will. 350 ms nach dem letzten Tipper
// reicht; wer fertig getippt hat, wartet keine halbe Sekunde.
const sammler = new Sammelstelle();
let dupeFrist = null;
let dupeOffen = '';      // welches Rufzeichen gerade gefragt ist

function logListeZeigen(liste) {
  const el = $('logListe');
  el.textContent = '';
  if (!liste || !liste.length) {
    const d = document.createElement('div');
    d.className = 'leer';
    d.textContent = '—';
    el.appendChild(d);
    return;
  }
  for (const z of liste) {
    const r = document.createElement('div');
    r.className = 'r';
    const w = document.createElement('span');
    w.className = 'wann';
    w.textContent = zeitKurz(z.datum, z.zeit) || '—';
    const ruf = document.createElement('span');
    ruf.className = 'ruf';
    ruf.textContent = z.ruf;
    const wo = document.createElement('span');
    wo.className = 'wo';
    wo.textContent = [z.band, z.mode].filter(Boolean).join(' ');
    r.append(w, ruf, wo);
    el.appendChild(r);
  }
}

function logListeHolen() {
  if (!link.ready) { return; }
  sammler.beginnen();
  link.send('log_last:10');
}

function dupeZeigen(satz) {
  const el = $('qsoDupe');
  el.textContent = satz.text;
  el.className = `dupe${satz.art ? ' d-' + satz.art : ''}`;
}

function dupeFragen() {
  const ruf = ($('qsoRuf').value || '').trim().toUpperCase();
  clearTimeout(dupeFrist);
  if (ruf.length < 3) {
    // Unter drei Zeichen ist jedes Rufzeichen noch jedes andere. Eine
    // Auskunft darueber waere beliebig.
    dupeOffen = '';
    dupeZeigen({ text: '', art: '' });
    return;
  }
  dupeFrist = setTimeout(() => {
    if (!link.ready) { return; }
    dupeOffen = ruf;
    link.send(`log_dup:${feld(ruf)}`);
  }, 350);
}

link.addEventListener('logzeile', (e) => {
  sammler.zeile(zeileLesen((e.detail || {}).args || []));
});

link.addEventListener('logende', (e) => {
  const d = e.detail || {};
  if (d.anzahl === null || d.anzahl === undefined) {
    // Abgelehnt (z. B. Logbuch aus dem Netz abgeschaltet). Die Liste bleibt
    // leer und sagt, warum — statt still leer zu bleiben und wie ein leeres
    // Logbuch zu wirken.
    sammler.abschluss(-1);
    const el = $('logListe');
    el.textContent = '';
    const x = document.createElement('div');
    x.className = 'leer';
    x.textContent = d.text || 'nicht freigegeben';
    el.appendChild(x);
    return;
  }
  const liste = sammler.abschluss(d.anzahl);
  // null heisst: es fehlen Zeilen. Dann NICHT zeigen — eine kuerzere Liste
  // sieht aus wie ein kuerzeres Logbuch.
  if (liste) { logListeZeigen(liste); }
});

link.addEventListener('logdupe', (e) => {
  const d = e.detail || {};
  if (!d.args) { dupeZeigen({ text: d.text || '', art: '' }); return; }
  const b = befundLesen(d.args);
  // Eine Antwort auf ein Rufzeichen, das inzwischen weitergetippt wurde,
  // gehoert nicht mehr unter das Feld.
  if (!b || b.ruf !== dupeOffen) { return; }
  dupeZeigen(dupeSatz(b));
});

function qsoOeffnen() {
  $('qsoRuf').value = '';
  $('qsoMeldung').textContent = '';
  $('qsoMeldung').className = '';
  $('qsoOk').disabled = false;
  dupeZeigen({ text: '', art: '' });
  dupeOffen = '';
  logListeHolen();
  qsoAutomatikZeigen();
  clearInterval(qsoUhr);
  qsoUhr = setInterval(qsoAutomatikZeigen, 5000);
  $('qsoBlatt').classList.add('an');
  // Erst nach dem Einblenden, sonst bleibt die Tastatur auf iOS zu.
  setTimeout(() => $('qsoRuf').focus(), 50);
}

function qsoSchliessen() {
  clearInterval(qsoUhr); qsoUhr = null;
  $('qsoBlatt').classList.remove('an');
  $('qsoRuf').blur();
}

function qsoEintragen() {
  const ruf = ($('qsoRuf').value || '').trim().toUpperCase();
  if (!ruf) { $('qsoRuf').focus(); return; }
  if (!link.ready) {
    // Nicht senden und so tun als ob: ohne stehende Verbindung kommt der
    // Kontakt nirgends an, und das Blatt sagt es.
    $('qsoMeldung').textContent = 'keine Verbindung — nicht eingetragen';
    $('qsoMeldung').className = 'warn';
    return;
  }
  const rs = ($('qsoRstS').value || '59').trim() || '59';
  const re = ($('qsoRstE').value || '59').trim() || '59';
  $('qsoOk').disabled = true;
  $('qsoMeldung').textContent = 'wird eingetragen…';
  $('qsoMeldung').className = '';
  // Durch feld(): ein Semikolon im Rufzeichen macht aus dem Eintrag
  // einen zweiten Befehl -- und der naechste in der Zeile waere der
  // Sendebefehl. Begruendung bei feld() in tci.js.
  link.send(`log_qso:${feld(ruf)},${feld(rs)},${feld(re)}`);
  // Kommt keine Antwort, bleibt es nicht bei "wird eingetragen" stehen.
  // Eine Anzeige, die ewig auf dem Zwischenstand verharrt, liest sich wie
  // ein Erfolg.
  clearTimeout(qsoEintragen._frist);
  qsoEintragen._frist = setTimeout(() => {
    if (!$('qsoOk').disabled) { return; }
    $('qsoOk').disabled = false;
    $('qsoMeldung').textContent = 'keine Antwort — nicht sicher eingetragen';
    $('qsoMeldung').className = 'warn';
  }, 4000);
}

link.addEventListener('qso', (e) => {
  clearTimeout(qsoEintragen._frist);
  const d = e.detail || {};
  $('qsoOk').disabled = false;
  if (d.ok) {
    $('qsoMeldung').textContent = `${d.text} eingetragen`;
    $('qsoMeldung').className = 'ok';
    $('qsoRuf').value = '';
    dupeZeigen({ text: '', art: '' });
    dupeOffen = '';
    // Der Beleg: der eigene Kontakt steht jetzt als erste Zeile in der
    // Liste. Die Erfolgsmeldung kann das nur behaupten.
    logListeHolen();
    // Offen lassen: im Pile-up kommt der naechste sofort, und ein Blatt,
    // das nach jedem Kontakt zugeht, kostet zwei Tipper je QSO.
    setTimeout(() => $('qsoRuf').focus(), 30);
  } else {
    $('qsoMeldung').textContent = d.text || 'abgelehnt';
    $('qsoMeldung').className = 'warn';
  }
});

$('logAuf').addEventListener('click', qsoOeffnen);
$('qsoAb').addEventListener('click', qsoSchliessen);
$('qsoOk').addEventListener('click', qsoEintragen);
$('qsoRuf').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') { e.preventDefault(); qsoEintragen(); }
});
$('qsoRuf').addEventListener('input', dupeFragen);
$('logFrisch').addEventListener('click', logListeHolen);
// Tippen auf den Grund schliesst, wie beim Frequenz-Blatt.
$('qsoBlatt').addEventListener('click', (e) => {
  if (e.target === $('qsoBlatt')) { qsoSchliessen(); }
});

function qsyOeffnen() {
  const v = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
  $('qsyFeld').value = v ? (v / 1e6).toFixed(5) : '';
  $('qsy').classList.add('an');
  // Erst nach dem Einblenden, sonst bleibt die Tastatur auf iOS zu.
  setTimeout(() => { $('qsyFeld').focus(); $('qsyFeld').select(); }, 50);
}
function qsySchliessen() { $('qsy').classList.remove('an'); $('qsyFeld').blur(); }
function qsySetzen() {
  const hz = hzAusEingabe($('qsyFeld').value);
  if (hz === null) { $('qsyFeld').value = ''; return; }
  link.send(`vfo:${state.trx},0,${hz}`);
  // Wie beim Bandwechsel: ein Schnitt sagt, ab wo der neue Massstab gilt.
  wasserfallSchnitt(); letzteMitteHz = hz;
  qsySchliessen();
}
$('hz').addEventListener('click', qsyOeffnen);
$('qsyOk').addEventListener('click', qsySetzen);
$('qsyAb').addEventListener('click', qsySchliessen);
$('qsyFeld').addEventListener('keydown', (e) => { if (e.key === 'Enter') qsySetzen(); });
$('qsy').addEventListener('click', (e) => { if (e.target === $('qsy')) qsySchliessen(); });

// ── Tippen springt dorthin (2026-10-01) ─────────────────────────────────────
//
// Betreiber: "wenn ich ein signal sehe, vor allem im unteren pandapter
// möchte ich auch dort hinklicken, das funktioniert aber nicht".
//
// Es ging nicht, weil die Totzone (gegen ungewolltes Verstimmen beim
// Anfassen) jeden Tipp verschluckte, ohne ihn je als Befehl zu werten. Das
// war eine halbe Loesung: nicht verstimmen ist richtig, nichts tun ist es
// nicht.
//
// Jetzt: ein kurzer Tipp OHNE Bewegung springt auf die getippte Stelle, ein
// Wisch zieht wie bisher. Beide Canvas liegen im selben Element, also gilt
// das fuer Spektrum und Wasserfall gleichermassen — und gerade im
// Wasserfall sieht man eine Station oft zuerst.
function frequenzAnStelle(clientX) {
  const r = $('scope').getBoundingClientRect();
  if (!r.width) { return null; }
  const spanne = bildSpanneHz(192000);
  const mitte  = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : null;
  if (!mitte) { return null; }
  // Die Bildmitte ist die abgestimmte Frequenz (Longpath fuehrt sie nach).
  const versatz = (clientX - r.left) / r.width - 0.5;
  const raster = spanne <= 24000 ? 10 : (spanne <= 96000 ? 50 : 100);
  return Math.round((mitte + versatz * spanne) / raster) * raster;
}

$('scope').addEventListener('pointerup', (e) => {
  // Kein Wisch gewesen und nur ein Finger im Spiel? Dann war es ein Tipp.
  if (!wischAktiv && zeiger.size <= 1 && wischVon !== null) {
    // Zuerst die Spots: liegt eine Marke unter dem Finger, gilt IHRE
    // Frequenz und nicht die getippte Stelle. Sonst landet man je nach
    // Fingerbreite ein paar hundert Hertz daneben -- genau das, was der
    // Spot verhindern soll. Gerechnet wird in Canvas-Einheiten, weil
    // `spotMarken` beim Zeichnen so entstanden ist.
    const r = $('scope').getBoundingClientRect();
    const xCanvas = r.width
      ? (e.clientX - r.left) / r.width * pan.width : -1;
    // Der Panadapter ist nur der OBERE Teil des Feldes; darunter liegt der
    // Wasserfall. Die y-Umrechnung muss darum auf die Hoehe des Canvas
    // gehen und nicht auf die des ganzen Feldes.
    const rPan2 = pan.getBoundingClientRect();
    const yCanvas = rPan2.height
      ? (e.clientY - rPan2.top) / rPan2.height * pan.height : -1;

    // ── Auf der SCHRIFT: das Rufzeichen ist gemeint, nicht die Frequenz ──
    //
    // Ein Tipp schlaegt es bei QRZ nach, zwei tragen es ins QSO-Blatt. Der
    // einzelne Tipp muss darum kurz warten, ob ein zweiter kommt -- anders
    // ist beides auf einem Telefon nicht zu unterscheiden. 280 ms ist das
    // uebliche Mass; darunter trennt es Doppeltipps von zittrigen Fingern
    // nicht mehr.
    const schrift = (r.width && rPan2.height)
      ? trefferAufSchrift(spotMarken, xCanvas, yCanvas) : null;
    if (schrift) {
      wischVon = null;
      const jetzt = performance.now();
      if (spotTipp.ruf === schrift.ruf && (jetzt - spotTipp.wann) < 280) {
        clearTimeout(spotTipp.frist);
        spotTipp.ruf = null;
        qsoOeffnenMit(schrift.ruf);
      } else {
        spotTipp.ruf = schrift.ruf;
        spotTipp.wann = jetzt;
        clearTimeout(spotTipp.frist);
        spotTipp.frist = setTimeout(() => {
          spotTipp.ruf = null;
          qrzOeffnen(schrift.ruf);
        }, 280);
      }
      return;
    }

    const treffer = r.width ? trefferBei(spotMarken, xCanvas) : null;
    if (treffer) {
      link.send(`vfo:${state.trx},0,${treffer.hz}`);
      spotsBaldHolen();
      wischVon = null;
      return;
    }
    const ziel = frequenzAnStelle(e.clientX);
    if (ziel) {
      const vorher = link.st.vfo[state.trx] ? link.st.vfo[state.trx][0] : ziel;
      link.send(`vfo:${state.trx},0,${ziel}`);
      // Nicht hier schieben: mitteVerfolgen() tut es, sobald das Geraet die
      // neue Frequenz bestaetigt. Zweimal schieben hiesse doppelt schieben.
    }
  }
  // Den letzten Stand nachreichen: durch die Drosselung kann bis zu ein
  // Intervall Wegstrecke ungesendet geblieben sein, und dann steht das
  // Geraet ein Stueck neben dem, wo der Finger losgelassen hat.
  if (wischAktiv && wischZiel !== null) {
    link.send(`vfo:${state.trx},0,${wischZiel}`);
    // Auch hier nicht selbst schieben — siehe mitteVerfolgen().
  }
  wischVon = null; wischAktiv = false; wischZiel = null;
  state.bildHalten = false;
  state.wischVersatzPx = 0;
});
// Abbruch raeumt GENAU SO AUF wie ein Loslassen.
//
// Hier fehlten `bildHalten` und `wischVersatzPx`, und das war kein
// Schoenheitsfehler: iOS feuert pointercancel, sobald es die Lupe aufzieht —
// also genau dann, wenn der Finger etwas laenger auf dem Bild liegt. Danach
// blieb `bildHalten` fuer immer wahr, der spectrum-Horcher stieg bei jedem
// Bild sofort wieder aus, der Wasserfall bekam keine Zeile mehr und die
// Fusszeile meldete, es komme kein Bild. Betreiber am 2026-10-02: "zu langes
// bleiben am cursor im panadapter loescht den wasserfall und es steht, kein
// funkgeraet gefunden". Es war nie das Funkgeraet — es war diese Zeile.
$('scope').addEventListener('pointercancel', () => {
  wischVon = null; wischAktiv = false; wischZiel = null;
  state.bildHalten = false; state.wischVersatzPx = 0;
  zeiger.clear(); kneifVon = 0;
});

// Fangnetz: liegt kein Finger mehr auf dem Bild, darf nichts mehr eingefroren
// sein. Greift auch, wenn ein Ereignis ganz ausbleibt — und ein eingefrorenes
// Bild ist der eine Zustand, aus dem der Bediener nicht von selbst herausfindet.
setInterval(() => {
  if (state.bildHalten && zeiger.size === 0 && wischVon === null) {
    state.bildHalten = false; state.wischVersatzPx = 0;
  }
}, 1000);

// ── Abstimmtraeger: bewusst KEIN Knopf ──────────────────────────────────────
//
// Hier stand bis zum 2026-09-30 ein Klickhorcher, der `tune:N,true` schickte.
// Das war ein Fehler, und zwar ein sendender: der Abstimmtraeger legt einen
// Dauertraeger mit voller Leistung auf die Antenne. Er heisst nur nicht
// „senden", deshalb ist er beim Nachdenken ueber die Sendesperre durchgerutscht
// — waehrend die SENDEN-Taste daneben ausdruecklich tot war.
//
// Dazu kam, was die Durchsicht am selben Tag am Server fand: der tune-Weg war
// der einzige Sendeweg ohne Besitzer, Wachhund und Sendezeit-Deckel (das ist
// jetzt behoben, TciServer.cpp). Ein Telefon, dessen WLAN abreisst oder das
// iOS einfriert, haette den Traeger unbegrenzt stehen lassen.
//
// Die Handfunke ist zum Hoeren und Bedienen. Beide Sendetasten sind tot und
// sehen auch so aus. Soll das Telefon eines Tages senden duerfen, ist das eine
// bewusste Erweiterung mit eigener Zustandsmeldung vom Server und einer
// Sicherung gegen Fehltipp (langes Druecken) — nicht ein Knopf, der still
// funktioniert, weil die Verbindung zufaellig ueber Loopback lief.

// ── Ereignisse vom Draht ────────────────────────────────────────────────────
link.addEventListener('open', () => {
  // Anmelden, falls ein Token hinterlegt ist. Auf Loopback verlangt der Server
  // keines und verwirft die Zeile stillschweigend — schadet also nicht.
  // Aus dem Netz beantwortet er BIS DAHIN nichts, deshalb muss das hier ganz
  // vorne stehen, vor jeder Anforderung.
  if (state.token) { link.send(`auth:${feld(state.token)}`); }

  // ── Tonformat aushandeln, BEVOR der Strom startet ────────────────────────
  //
  // 48 kHz float32 stereo sind 384 kB/s. 12 kHz int16 mono sind 24 — Faktor
  // sechzehn, ohne einen einzigen Codec. Die Bandbreite von 6 kHz reicht für
  // alles, was ein Empfänger an Sprache herausgibt (SSB endet bei 3 kHz), und
  // mono ist richtig, weil hier ohnehin ein Empfänger zu hören ist.
  //
  // Erst danach lohnt Opus — es würde die verbleibenden 24 auf etwa 4 kB/s
  // drücken, kostet aber eine Bibliothek im Browser.
  // ── Messwerte anfordern ──────────────────────────────────────────────────
  //
  // Longpath schickt sie NICHT von selbst: `rxSensorsEnabled` steht ab Werk
  // auf false und wird erst durch diese Zeile wahr (TciClientSession.h:235).
  // Das ist Thetis-getreu — dort verlangt `setRxSensorsEnabled` ebenfalls
  // eine ausdrueckliche Anmeldung. Ohne sie bleibt der Signalbalken leer,
  // und genau das war am echten Geraet zu sehen, bevor diese Zeile stand.
  //
  // 200 ms ist die Server-Vorgabe und zugleich sein unteres Ende der
  // sinnvollen Spanne; schneller braucht ein Balken nicht zu sein, und
  // langsamer wirkt er traege.
  link.send(`rx_sensors_enable:true,200`);
  link.send(`tx_sensors_enable:true,200`);

  link.send(`audio_samplerate:${state.audioRate}`);
  link.send(`audio_stream_channels:1`);
  // mu-law: ein Byte je Abtastung statt zwei. Bei 12 kHz mono sind das
  // 13,3 statt 24,9 kB/s — die Haelfte des gesamten Datenverbrauchs, ohne
  // eine fremde Bibliothek auf einer der beiden Seiten. Was man hoert, ist
  // an echtem Kurzwellenton gemessen gleichwertig (37,7 dB Stoerabstand,
  // unter dem Bandrauschen jedes Empfaengers).
  //
  // Ein fremder Server kennt den Namen nicht. Longpath sagt dann im
  // Protokoll Bescheid und bleibt beim bisherigen Format; frueher fiel es
  // still auf float32 — das Achtfache. Unten wird deshalb geprueft, was
  // wirklich gilt.
  link.send(`audio_stream_sample_type:mulaw8`);

  // Nachfassen: bestaetigt der Server nicht mulaw8, auf int16 zurueck. Ohne
  // das haengt der Ton an der Hoffnung, dass die Gegenseite den Namen kennt.
  // Nachfassen: kommen nach drei Sekunden KEINE mu-law-Rahmen an, auf int16
  // zurueck. Geprueft wird am Binaerkopf (audioTypRahmen), nicht am
  // Text-Echo — das kommt aus dem globalen RadioModel und meldet, was
  // irgendwo eingestellt ist, nicht was diese Verbindung bekommt. Am
  // 2026-09-30 live in die Falle getappt: das Echo sagte int16, die Seite
  // schaltete daraufhin selbst zurueck, und der billige Ton kam nie zum
  // Einsatz, obwohl der Server ihn geliefert haette.
  clearTimeout(state.tonTypPruefung);
  state.tonTypPruefung = setTimeout(() => {
    if (link.st.audioTypRahmen !== null && link.st.audioTypRahmen !== 101) {
      link.send(`audio_stream_sample_type:int16`);
    }
  }, 3000);

  link.send(`audio_start:${state.trx}`);

  // Zuerst das FERTIGE Spektrum: die Punktzahl ist unsere Breite, mehr kann
  // der Schirm nicht zeigen. Gegen rohes I/Q spart das den Faktor sechzig.
  // Ein fremder Server (Thetis, ExpertSDR) kennt den Befehl nicht und
  // verwirft ihn antwortlos — deshalb steht darunter der Rückfall.
  link.send(`spectrum_start:${state.trx},${pan.width},12,${state.spanneHz}`);

  // Rückfall auf rohes I/Q — aber nur, wenn der Server den Befehl gar nicht
  // KENNT, nicht schon dann, wenn gerade keine Bilder kommen.
  //
  // Bis 2026-09-30 hing der Rückfall am Ausbleiben von Daten. Beim Start
  // ohne verbundenes Funkgerät kamen zwei Sekunden lang keine — und das
  // Telefon zog daraufhin rohes I/Q, das Vielfache an Daten, obwohl der
  // Server das fertige Bild sehr wohl liefern konnte, sobald ein Gerät dran
  // war. Am echten Gerät im Log gesehen: „IQ stream subscribed rx 0" direkt
  // nach einem Start, bei dem die Verbindung noch im Aufbau war.
  //
  // Longpath bestätigt das Abonnement mit den geltenden Werten zurück; ein
  // fremder Server (Thetis, ExpertSDR) schweigt dazu. Das ist der richtige
  // Prüfstein.
  clearTimeout(state.iqRueckfall);
  state.iqRueckfall = setTimeout(() => {
    if (!link.st.spektrumBestaetigt) {
      link.send(`iq_start:${state.trx}`);
      state.rueckfall = true;
    }
  }, 2000);
});

// Fertiges Spektrum vom Server — das Telefon rechnet dann gar nichts mehr.
link.addEventListener('spectrum', (e) => {
  const v = e.detail.vals;
  if (!v.length) return;
  // Waehrend des Abstimmens kein neues Bild annehmen — siehe
  // state.bildHalten. Der Zaehler laeuft weiter, damit die Datenrate
  // stimmt; nur das Bild friert ein.
  if (state.bildHalten) { state.hatSpektrumstrom = true; return; }
  state.hatSpektrumstrom = true;
  letzteIq = performance.now();
  if (neueZeilen < 3) neueZeilen++;

  // Die Punktzahl bestimmt der Server (er klemmt unsere Bitte). Also nicht
  // auf N festnageln, sondern nehmen, was kommt.
  if (specServer.length !== v.length) { specServer = new Float32Array(v.length); }
  for (let i = 0; i < v.length; i++) {
    specServer[i] = hatSpektrum ? specServer[i] * 0.6 + v[i] * 0.4 : v[i];
  }
  hatSpektrum = true;
});
link.addEventListener('ready', () => zeichneBedienung());
link.addEventListener('ready', () => spotsBaldHolen(300));

// ── Der Rotor (2026-10-05) ────────────────────────────────────────────────
//
// Das Feld BLEIBT VERSTECKT, bis Longpath einen Rotor meldet. An der QRP
// gibt es keinen, und ein leeres Feld, das nichts kann, ist auf einem
// Telefon reiner Platzverbrauch.
//
// Die Scheibe WAEHLT nur. Gedreht wird erst mit dem Knopf -- dieselbe
// Entscheidung wie bei der Sendetaste: ein Tipp, der sofort einen Mast
// dreht, ist in einer Hosentasche eine schlechte Idee. Und Longpath
// verweigert es ohnehin, solange TciAllowRemoteRotor nicht steht; die
// Ablehnung kommt als `rotor_err:` hier an und wird gezeigt, statt
// verschluckt zu werden.
const rotorSicherung = new RotorSicherung();
let rotorStand = null;
let rotorFeldAn = false;
let rotorTakt = null;

function rotorStricheZeichnen() {
  const g = $('rotorStriche');
  if (!g || g.childElementCount) { return; }
  // Alle 10 Grad ein Strich, alle 30 ein laengerer. Das ist die Teilung
  // jedes Kompasses; eine feinere kann ein Finger ohnehin nicht treffen.
  for (let a = 0; a < 360; a += 10) {
    const haupt = (a % 30) === 0;
    const r1 = haupt ? 78 : 84, r2 = 92;
    const rad = a * Math.PI / 180;
    const l = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    l.setAttribute('x1', (Math.sin(rad) * r1).toFixed(2));
    l.setAttribute('y1', (-Math.cos(rad) * r1).toFixed(2));
    l.setAttribute('x2', (Math.sin(rad) * r2).toFixed(2));
    l.setAttribute('y2', (-Math.cos(rad) * r2).toFixed(2));
    if (haupt) { l.setAttribute('class', 'haupt'); }
    g.appendChild(l);
  }
}

function rotorZeichnen() {
  const nadel = $('rotorNadel'), ziel = $('rotorZiel');
  const alt = rotorStand && !rotorStand.frisch;
  if (rotorStand) {
    const rad = rotorStand.grad * Math.PI / 180;
    nadel.setAttribute('x2', (Math.sin(rad) * 82).toFixed(2));
    nadel.setAttribute('y2', (-Math.cos(rad) * 82).toFixed(2));
    nadel.classList.toggle('alt', !!alt);
    $('rotorGrad').textContent = `${Math.round(rotorStand.grad)}°`;
    $('rotorGrad').classList.toggle('alt', !!alt);
  }
  const z = rotorSicherung.gilt(performance.now()) ? rotorSicherung.ziel : null;
  if (z === null) { ziel.setAttribute('hidden', ''); }
  else {
    const rad = z * Math.PI / 180;
    ziel.setAttribute('x2', (Math.sin(rad) * 92).toFixed(2));
    ziel.setAttribute('y2', (-Math.cos(rad) * 92).toFixed(2));
    ziel.removeAttribute('hidden');
  }
  const knopf = $('rotorDreh');
  knopf.disabled = (z === null);
  knopf.textContent = (z === null) ? 'RICHTUNG WÄHLEN' : `AUF ${z}° DREHEN`;
  const t = zustandText(rotorStand);
  if (!$('rotorMeldung').classList.contains('warn')) {
    $('rotorMeldung').textContent = t;
  }
}

function rotorHolen() { if (link.ready) { link.send(`rotor:${state.trx}`); } }

link.addEventListener('rotor', (e) => {
  const st = standLesen((e.detail || {}).args || []);
  if (!st) { return; }
  rotorStand = st;
  if (!rotorFeldAn) {
    // Erst jetzt zeigen: davor wussten wir nicht, ob es ueberhaupt einen gibt.
    rotorFeldAn = true;
    rotorStricheZeichnen();
    $('rotorFeld').removeAttribute('hidden');
  }
  rotorZeichnen();
});

link.addEventListener('rotorok', () => {
  $('rotorMeldung').classList.remove('warn');
  $('rotorMeldung').textContent = 'ANGENOMMEN';
  // Gleich nachfragen: der Rotor meldet den Zustand `dreht` erst, wenn er
  // wirklich anlaeuft.
  setTimeout(rotorHolen, 400);
});

link.addEventListener('rotorfehler', (e) => {
  const t = ((e.detail || {}).text || '').trim();
  // "kein rotor" ist kein Fehler des Bedieners -- dann gibt es hier einfach
  // keinen, und das Feld bleibt weg.
  if (/kein rotor/i.test(t)) { return; }
  // Nicht nur DASS es abgelehnt wurde, sondern wo der Schalter sitzt.
  // Siehe fehlerText() in rotor.js.
  $('rotorMeldung').textContent = rotorFehlerText(t);
  $('rotorMeldung').classList.add('warn');
  rotorSicherung.verwerfen();
  rotorZeichnen();
});

$('rotorScheibe').addEventListener('pointerup', (e) => {
  const r = $('rotorScheibe').getBoundingClientRect();
  if (!r.width) { return; }
  // In die Koordinaten der Scheibe umrechnen: Mitte ist 0/0, y nach unten.
  const x = (e.clientX - r.left) - r.width / 2;
  const y = (e.clientY - r.top) - r.height / 2;
  // Der Mindestradius ist in Bildpunkten der ANZEIGE, nicht des viewBox --
  // darum auf die tatsaechliche Breite bezogen.
  const g = peilungAus(x, y, r.width * 0.12);
  if (g === null) { return; }
  rotorSicherung.waehle(g, performance.now());
  $('rotorMeldung').classList.remove('warn');
  $('rotorMeldung').textContent = '';
  rotorZeichnen();
});

$('rotorDreh').addEventListener('click', () => {
  const z = rotorSicherung.bestaetige(performance.now());
  if (z === null) { rotorZeichnen(); return; }
  if (!link.ready) {
    $('rotorMeldung').textContent = 'keine Verbindung — nicht gedreht';
    $('rotorMeldung').classList.add('warn');
    return;
  }
  link.send(`rotor_to:${z}`);
  $('rotorMeldung').classList.remove('warn');
  $('rotorMeldung').textContent = 'wird gesendet …';
  rotorZeichnen();
});

// Zwei Sekunden: ein Rotor braucht fuer ein Grad laenger als das, und
// haeufiger zu fragen kostet nur Leitung.
rotorTakt = setInterval(rotorHolen, 2000);
link.addEventListener('ready', () => setTimeout(rotorHolen, 500));

// ── Zurueck aus dem Hintergrund (2026-10-05) ──────────────────────────────
//
// Bis hierher behandelte die Seite das Entsperren GAR NICHT. Was dabei
// wirklich passiert:
//
//   * Der WebSocket ist tot, aber `onclose` kommt erst, wenn das System die
//     Seite wieder auftaut -- und der Wiederholungsabstand ist inzwischen
//     auf 10 s gewachsen. Man sieht sekundenlang nichts, obwohl das
//     Funkgeraet bereit ist.
//   * Der AudioContext ist von iOS angehalten; niemand weckt ihn.
//   * Wasserfall und S-Meter zeigen den Stand von VOR dem Sperren, ohne das
//     zu sagen. Das ist dieselbe Gattung Luege wie ein stehender
//     Wasserfall: er behauptet ein leeres Band, und danach wird nicht
//     gerufen.
//
// Die Abonnements selbst muessen hier nicht erneuert werden -- sie haengen
// am `open`-Ereignis und kommen mit der neuen Verbindung von selbst.
let warVerborgen = false;

function aufwachen() {
  const stille = performance.now() - letzteIq;

  // 1. Das Bild als alt kennzeichnen, BEVOR etwas anderes passiert. Der
  //    Strich ist derselbe, den ein Bandwechsel zieht: oben das Neue, unten
  //    das Alte. Ohne ihn sieht ein zehn Minuten alter Wasserfall aus wie
  //    ein lebendiger.
  if (bildIstAlt(stille)) {
    wasserfallSchnitt();
    state.hatSpektrumstrom = false;
  }

  // 2. Sofort neu verbinden, wenn noetig (Entscheidung in aufwachen.js).
  link.nachDemAufwachen(stille);

  // 3. Den Ton wecken. iOS haelt den AudioContext beim Sperren an, und ein
  //    angehaltener Kontext ruft seinen Rueckruf nie wieder auf -- der Ton
  //    bliebe stumm, bis jemand TON AUS und wieder EIN drueckt.
  if (state.audio && state.audio.state === 'suspended') {
    state.audio.resume().catch(() => { /* ohne Zutun des Bedieners nicht erlaubt */ });
  }

  // 4. Die Spots gehoeren ebenfalls aufgefrischt: waehrend des Schlafens
  //    sind welche abgelaufen und neue gekommen.
  spotsBaldHolen(400);
}

document.addEventListener('visibilitychange', () => {
  if (document.hidden) { warVerborgen = true; return; }
  if (!warVerborgen) { return; }
  warVerborgen = false;
  aufwachen();
});

// Nicht jedes System meldet `visibilitychange` zuverlaessig, wenn die Seite
// aus dem Ruhezustand kommt -- `pageshow` mit `persisted` ist der zweite
// Weg, und `focus` der dritte. Mehrfaches Aufwachen schadet nicht: die
// Entscheidung in aufwachen.js wirft eine lebendige Verbindung nicht weg.
window.addEventListener('pageshow', (e) => { if (e.persisted) { aufwachen(); } });
window.addEventListener('focus', () => { if (warVerborgen) { warVerborgen = false; aufwachen(); } });
link.addEventListener('state', () => {
  // Der Server hat das letzte Wort über die Rate. Weicht sie von unserer
  // Bitte ab, muss das Worklet es erfahren — sonst stimmt die Tonhöhe nicht.
  const r = link.st.audioRate;
  if (r && r !== state.audioRate) {
    state.audioRate = r;
    if (state.node) { state.node.port.postMessage({ type: 'rate', value: r }); }
  }
  zeichneKopf(); zeichneBedienung();
});

// Verbindung weg: Kopplungsblatt zurueckholen, aber erst nach einer Weile —
// ein kurzer Aussetzer soll den Bediener nicht aus der Bedienung werfen.
let wegSeit = 0;
setInterval(() => {
  // „Socket offen" ist KEIN Mass fuer verbunden.
  //
  // Aus dem Netz nimmt Longpath die Verbindung an und schweigt dann, bis
  // `auth:` kommt (TciServer.cpp:1961) — der Socket steht auf 1, waehrend
  // nichts geht. Hier stand genau diese Pruefung, also galt der Zustand als
  // verbunden, `wegSeit` wurde jede Sekunde zurueckgesetzt, und das
  // Kopplungsblatt kam NIE zurueck. Am 2026-10-02 stand das Telefon dadurch
  // vor einer toten Seite ohne jedes Eingabefeld: kein Ton, kein Bild, keine
  // Stelle, an der man das Token haette eintippen koennen.
  //
  // Ueber die Bruecke konnte das nicht auffallen — dort kam jede Verbindung
  // aus Loopback, und Loopback verlangt kein Token.
  //
  // Massstab ist darum `ready`: erst der Init-Burst heisst verbunden.
  const offen = link.ws && link.ws.readyState === 1 && link.ready;
  // Steht der Socket, fehlt aber die Antwort, ist die Lage eindeutig und
  // muss nicht ausgesessen werden. Nur ein echter Abriss bekommt die lange
  // Schonfrist, damit ein kurzer WLAN-Huepfer niemanden aus der Bedienung wirft.
  const frist = (link.ws && link.ws.readyState === 1) ? 6000 : 12000;
  if (!offen && link.wanted) {
    if (!wegSeit) wegSeit = Date.now();
    if (Date.now() - wegSeit > frist) {
      $('koppeln').classList.add('an');
      $('fehler').textContent = koppelGrund();
    }
  } else {
    wegSeit = 0;
    // Und wieder WEG damit, sobald es wieder geht.
    //
    // Hier stand nur `wegSeit = 0`. Die Seite holt sich die Verbindung nach
    // einem Abriss von selbst zurueck (am 2026-10-03 gemessen: Server weg bei
    // 14 s, Blatt kommt bei 28 s, wieder verbunden bei 43 s) — aber das Blatt
    // blieb liegen. Der Bediener saesse vor einem laufenden Empfaenger und
    // einem Blatt, das nach der Adresse fragt, und muesste raten, ob es nun
    // geht. Genau diese Sorte Zweideutigkeit hat heute schon zwei Stunden
    // gekostet.
    //
    // Es gibt keinen Weg, das Blatt absichtlich zu oeffnen; es erscheint nur
    // bei einem Abriss. Darum ist Zumachen bei `ready` immer richtig.
    // Nicht zumachen, wenn der Bediener das Blatt selbst geoeffnet hat
    // (Tipp auf den Namen oben links): er tippt dort gerade ein Token.
    if ($('koppeln').classList.contains('an') && !blattVonHand) {
      $('koppeln').classList.remove('an');
      $('fehler').textContent = '';
    }
  }
}, 1000);

// Das Eingabeblatt gezielt oeffnen -- ein Tipp auf den Namen oben links.
// Bisher kam es nur nach einem Abriss von selbst und ging bei `ready` von
// selbst wieder zu; wer ein neues Token eintragen wollte, bekam es nie
// zu fassen, weil die Seite zwischendurch neu verband (2026-11-21).
let blattVonHand = false;
$('station').addEventListener('click', () => {
  blattVonHand = true;
  $('koppeln').classList.add('an');
  $('adresse').value = gespeichert() || $('adresse').value;
  $('token').value = tokenLesen();
  $('fehler').textContent = 'Neues Token eintragen und auf Verbinden tippen.';
  $('token').focus();
});

// Warum es nicht klappt — in einem Satz, den man auf einem Telefon lesen kann.
//
// Bis 2026-09-30 stand hier eine einzige Meldung, und die war ausgerechnet
// abgeschaltet, sobald ein Token gesetzt war: `if (!link.ready && !state.token)`.
// Also genau im haeufigsten Fall — Token falsch oder veraltet — schwieg die
// Seite und liess den Bediener raten (Durchsicht 2026-09-30).
function koppelGrund() {
  // Der Server trennt mit 1008 und Klartext, wenn dreimal falsch angemeldet
  // wurde. Das ist die eindeutigste Auskunft, die es gibt.
  if (link.schliessCode === 1008) {
    return (link.schliessGrund || 'Abgewiesen')
         + '. Das Token stimmt nicht — neues holen unter Setup → TCI Server.';
  }
  if (state.token && !link.st.angemeldet) {
    return 'Longpath antwortet, nimmt das Token aber nicht an. '
         + 'In Setup → TCI Server ein neues erzeugen und hier eintragen.';
  }
  // Socket steht, aber wir haben gar kein Token zu bieten — der haeufigste
  // Fall nach dem Umstellen auf die Netzadresse. Das ist keine „keine
  // Antwort", sondern eine klare Forderung, und sie gehoert auch so gesagt.
  if (!state.token && link.ws && link.ws.readyState === 1) {
    return 'Longpath ist erreichbar, verlangt aber ein Token. '
         + 'In Setup → TCI Server auf „New" tippen und den Code hier eintragen.';
  }
  if (!state.token) {
    return 'Keine Antwort. Ist Longpath ins Netz gebunden? '
         + 'Dann braucht es das Token aus Setup → TCI Server.';
  }
  return 'Keine Antwort. Laeuft Longpath, und ist die Adresse richtig?';
}

// ── Selbstmeldung (2026-09-30) ──────────────────────────────────────────────
//
// Ein Telefon hat keine Konsole, die jemand lesen koennte, und Vorlesen
// lassen ist muehsam und fehleranfaellig. Also meldet die Seite einmal
// nach dem Start, wie es um ihre Tonkette steht — reine Zustandszahlen,
// kein Inhalt, und nur an den Rechner, von dem sie geladen wurde.
async function melde(anlass) {
  const C = state.audio, K = state.node && state.node._kern;
  const d = {
    sicher: window.isSecureContext,
    // Ob die Wiedergabe-Sitzung gesetzt werden konnte. Ohne sie gehorcht der
    // Ton dem Stummschalter, und das sieht von aussen aus wie "kein Ton",
    // obwohl die ganze Kette tadellos laeuft.
    // Drei Zustaende, nicht zwei. Am 2026-10-03 meldete die Seite beim Start
    // `sitzung=behelf`, und das las sich wie der Fehler vom selben Morgen
    // (Ton gehorcht dem Stummschalter) — dabei war die Sitzung zu diesem
    // Zeitpunkt nur noch gar nicht gesetzt worden; das passiert erst beim
    // Tonstart. Ein Diagnosefeld, das einen Befund meldet, wo keiner ist,
    // schickt die Fehlersuche in die falsche Richtung. Dasselbe Muster, das
    // dieser Tag sechsmal in der Anzeige hatte, hier im eigenen Werkzeug.
    sitzung: state.sitzungGesetzt === null ? 'nicht versucht'
           : state.sitzungGesetzt ? 'playback' : 'behelf',
    weg: state.tonWeg || 'keiner',
    fehler: state.tonFehler || '-',
    ctx: C ? C.state : 'kein ctx',
    ctxRate: C ? C.sampleRate : 0,
    takte: state.tonTakte || 0,          // wurde onaudioprocess je gerufen?
    af: state.gain ? +state.gain.gain.value.toFixed(2) : -1,
    afPct: state.afPct,
    rahmen: link.bytes.audio,
    // Abstand Rauschboden -> staerkster Punkt. Unter 10 dB kommt keine HF an.
    hfdb: state.hfAbstand === null ? -1 : +state.hfAbstand.toFixed(1),
    tonTyp: link.st.audioTypRahmen,
    rate: link.st.audioRate,
    vorrat: K ? K.have : -1,
    ziel: K ? K.target : -1,
    anlauf: K ? K.anlauf : '-',
    leer: K ? K.starved : -1,
    ready: link.ready,
  };
  // Pegel an beiden Horchposten — in dBFS, gerundet.
  const pegel = (an) => {
    if (!an) { return 'kein'; }
    const z = new Float32Array(an.fftSize);
    an.getFloatTimeDomainData(z);
    let s = 0; for (let i = 0; i < z.length; i++) { s += z[i] * z[i]; }
    const rms = Math.sqrt(s / z.length);
    return rms > 0 ? Math.round(20 * Math.log10(rms)) : 'still';
  };
  d.vorDeckel  = pegel(state.mess1);
  d.nachDeckel = pegel(state.mess2);
  d.ausgabe    = (C && C.destination) ? C.destination.channelCount : -1;
  d.anlass = anlass || 'start';
  const q = Object.entries(d).map(([k, v]) => k + '=' + encodeURIComponent(String(v))).join('&');
  try { await fetch('/melde?' + q); } catch (e) {}
}
setTimeout(() => melde('start'), 12000);

// ── Bildschleife ────────────────────────────────────────────────────────────
// ── Was ein Tipp auf ein Rufzeichen bedeutet (2026-10-05) ────────────────
//
// Betreiber am 2026-10-05: einmal tippen schlaegt bei QRZ nach, zweimal
// traegt ins Logblatt ein. Auf dem STRICH bleibt es beim Abstimmen -- dort
// steht die Frequenz, auf der Schrift steht das Rufzeichen.
const spotTipp = { ruf: null, wann: 0, frist: null };

/** QRZ in einem neuen Reiter. Verlaesst die App -- das ist dem Bediener
 *  bewusst, er hat danach gefragt. */
function qrzOeffnen(ruf) {
  if (!ruf) { return; }
  // `noopener` ist kein Beiwerk: ohne das bekaeme die fremde Seite ueber
  // `window.opener` einen Griff auf diese hier.
  window.open(`https://www.qrz.com/db/${encodeURIComponent(ruf)}`,
              '_blank', 'noopener,noreferrer');
}

/** Das QSO-Blatt mit schon eingetragenem Rufzeichen. */
function qsoOeffnenMit(ruf) {
  qsoOeffnen();
  if (!ruf) { return; }
  const feld = $('qsoRuf');
  feld.value = ruf;
  // Die Dupe-Abfrage lebt am `input`-Ereignis -- von Hand gesetzte Werte
  // loesen das nicht aus. Ohne diese Zeile stuende das Rufzeichen da und
  // niemand haette nachgesehen, ob es schon im Logbuch steht.
  feld.dispatchEvent(new Event('input', { bubbles: true }));
}

// Der Ton hat Vorrang vor dem Bild.
//
// Am Telefon ist die Seite kein sicherer Kontext, also laeuft der Ton ueber
// einen ScriptProcessorNode -- im HAUPTFADEN, demselben, der hier zeichnet.
// Wer dort 20 ms lang malt, waehrend der Tonblock faellig ist, bekommt eine
// Luecke. Der Tonkern kann dagegen nichts tun: seine Anlaufsperre, der
// sanfte Gleichlauf und die Notbremse setzen alle voraus, dass er ueberhaupt
// aufgerufen WIRD.
//
// Die Bremse laesst Bilder aus, solange der Vorrat knapp ist -- aber nie mehr
// als 15 am Stueck. Ein Wasserfall, der stehenbleibt, ist von einem toten
// Empfang nicht zu unterscheiden, und das waere der schlimmere Fehler.
const zeichenbremse = new Zeichenbremse();

function schleife(t) {
  mitteVerfolgen();
  if (zeichenbremse.darfZeichnen({
        vorrat: state.tonVorrat,
        ziel: state.tonZiel,
        imHauptfaden: state.tonImHauptfaden === true,
      })) {
    zeichneBild();
  }
  const r = link.tickRates(t);
  // Alles zusammen, was die Leitung kostet — auch das Spektrum, das anfangs
  // fehlte und die Anzeige zu günstig aussehen liess.
  const gesamt = r.iq + r.audio + r.spec + r.text;
  $('rate').textContent = gesamt ? (gesamt + ' kB/s') : '';
  // Kommt kein Bild, steht hier WARUM — nicht nur eine fehlende Zahl.
  //
  // Am 2026-10-01 blieb der Wasserfall schwarz, waehrend Ton und
  // Signalpegel liefen, und nichts auf der Seite sagte, woran es lag
  // (Ursache war: die installierte Longpath-Fassung kannte den Befehl
  // `spectrum_start` gar nicht). Dieselbe Stille entsteht, wenn in
  // Longpath schlicht kein Funkgeraet verbunden ist. Ein leerer Kasten
  // laesst den Operator raten; ein Satz nicht.
  //
  // Kommt ein Bild, aber ohne jedes Signal darin, steht das jetzt auch da —
  // siehe state.hfAbstand. 15 dB als Grenze, aus Messungen vom 2026-10-03
  // und nicht geschaetzt: SunSDR2 QRP ohne Antenne 7/8/10 dB, ANVELINA mit
  // Antenne 28/36/39 dB. Dazwischen liegt eine breite Luecke.
  //
  // Die erste Fassung stand bei 10 dB und schwieg darum ausgerechnet bei
  // exakt 10,0 dB — gemessen am QRP ohne Antenne, bei einem zu 100 %
  // stillen Tonstrom. Eine Kante an der falschen Stelle ist schlimmer als
  // keine.
  const bildLaeuft = r.spec > 0 || r.iq > 0;
  // Waehrend gesendet wird, ist der Empfaenger stumm. "nur rauschen —
  // antenne?" waere dann die falsche Erklaerung fuer etwas, das die
  // Sendezeile daneben schon richtig benennt; dieselbe Regel wie bei
  // "stockt" ohne Tonstrom.
  const sendetGerade = link.ready && (link.st.mox || link.st.tune);
  const nurRauschen = bildLaeuft && !sendetGerade && state.hfAbstand !== null
                      && state.hfAbstand < 15;
  $('fussBild').className = (!bildLaeuft || nurRauschen) ? 'warn' : '';
  $('fussBild').textContent =
      nurRauschen ? ('nur rauschen (' + state.hfAbstand.toFixed(0) + ' dB) — antenne?')
    : r.spec ? (r.spec + ' kB/s bild')
    : r.iq   ? (r.iq + ' kB/s bild (roh)')
    : link.ready ? 'kein bild — funkgerät verbunden?'
    : '';
  // Der Ton bekommt die Wahrheit, nicht nur eine Byte-Zahl: eine Datenrate
  // ohne hoerbaren Ton hat am 2026-09-30 eine ganze Messreihe wertlos
  // gemacht. Faellt der Ton aus, steht das hier — und nicht nur in einer
  // Konsole, die auf einem Telefon niemand sieht.
  // Ueber den Fernweg zaehlt `link.rate.audio` das, was aus dem
  // Opus-Dekoder HERAUSKOMMT (48 kHz Int16), nicht das, was uebertragen
  // wurde. Die Leitung selbst weiss es besser und sagt es hier.
  const fern = link.ws && link.ws.ton === 'opus' ? link.ws : null;
  const tonKbs = fern ? fern.tonKbs : r.audio;
  $('fussTon').textContent = state.tonFehler
    ? state.tonFehler
    : !state.node ? 'Ton aus — auf TON EIN tippen'
    : (tonKbs ? tonKbs + ' kB/s ton' + (fern ? ' (opus)' : '')
                 + (state.tonWeg === 'scriptprocessor' ? ' (ersatzweg)' : '')
               : '');
  // Stottert der Ton gerade, steht das da — sonst sucht man es im Funkgeraet.
  //
  // Nur bei STEHENDER Verbindung: ohne Gegenstelle laeuft die Tonkette
  // selbstverstaendlich leer, und "stockt" waere dann die falsche Erklaerung
  // fuer etwas, das die Fusszeile daneben schon richtig benennt
  // ("getrennt"). Genau so am 2026-09-30 gesehen.
  // Nur melden, wenn ueberhaupt ein Tonstrom laeuft. Ohne Strom laeuft die
  // Kette selbstverstaendlich leer — "stockt" waere dann die falsche
  // Erklaerung fuer "es kommt nichts", und sie stuende dauerhaft da. Die
  // Pruefung auf link.ready allein genuegte nicht: die Verbindung kann
  // stehen, ohne dass Ton abonniert ist.
  if (link.ready && !sendetGerade && r.audio > 0 && !state.tonFehler
      && state.tonLeerlaufZuletzt
      && performance.now() - state.tonLeerlaufZuletzt < 2000) {
    $('fussTon').textContent += ' ⚠ stockt';
  }
  $('fussTon').className = state.tonFehler ? 'warn' : '';
  // Eine Verbindung kann formal offen stehen und trotzdem tot sein — WLAN
  // weg, Rechner im Ruhezustand, Longpath beendet. Der Socket merkt das erst
  // nach Minuten. Bis 2026-09-30 zeigte die Seite derweil gruen und
  // '◆ gekoppelt' (Durchsicht). Also an den DATEN messen, nicht am Socket:
  // der Server schickt Messwerte alle 200 ms, drei Sekunden Stille sind
  // eindeutig.
  const still = link.ready && link.letzteDaten
              && (performance.now() - link.letzteDaten > 3000);
  $('fussStatus').textContent = still ? '◆ keine Daten'
                              : link.ready ? '◆ gekoppelt'
                              : (link.ws && link.ws.readyState === 1) ? '◆ verbinde…' : '◇ getrennt';
  $('fussStatus').className = still ? 'warn' : (link.ready ? 'ok' : '');

  // ── Wenn die Station sendet, darf die Seite das nicht verschweigen ──────
  //
  // `trx:` und `tune:` werden seit dem Anfang mitgelesen (tci.js) und waren
  // bis hierher nirgends verwendet: die Seite WUSSTE, dass gesendet wird,
  // und sagte nichts. Fuer eine Fernbedienung, die nur hoert, ist das die
  // unangenehmste Form von Stille — Wasserfall leer, Ton weg, S-Meter
  // unten, und nichts erklaert es. Genau dieselbe Gattung Fehler wie das
  // eingefrorene S-Meter, nur umgekehrt: dort behauptete die Anzeige etwas
  // Falsches, hier laesst sie etwas Richtiges weg.
  //
  // Seit dem Mikrofon-PTT der SunSDR QRP kann das auch ohne Zutun am Pult
  // passieren: Taste am Mikrofon gedrueckt -> Longpath meldet `trx:0,true`
  // -> hier steht es.
  //
  // Die Zeile sitzt dort, wo sonst "SENDEN / NUR IN DER APP" steht: gleiche
  // Geometrie, gleicher Platz, nur in Messing. Kein Rot — Rot bleibt der
  // Warnung, und Senden ist keine Warnung, sondern ein Zustand.
  //
  // Was hier bewusst NICHT passiert: das S-Meter wird nicht geleert. Ob
  // Longpath waehrend des Sendens weiter echte Empfangswerte meldet, ist
  // nicht geprueft — und das zu pruefen hiesse senden. Ungeprueft etwas
  // ausblenden waere genauso geraten wie es ungeprueft stehenzulassen.
  // `sendetGerade` ist weiter oben in derselben Runde schon bestimmt.
  const txEl = $('tx');
  if (sendetGerade !== state.sendetGezeigt) {
    state.sendetGezeigt = sendetGerade;
    txEl.classList.toggle('sendet', sendetGerade);
    txEl.innerHTML = sendetGerade
      ? (link.st.tune ? 'STATION STIMMT AB <small>EMPFÄNGER STUMM</small>'
                      : 'STATION SENDET <small>EMPFÄNGER STUMM</small>')
      : (SENDEN_FREIGABE ? 'SENDEN <small>HALTEN</small>'
                         : 'SENDEN <small>NUR IN DER APP</small>');
  }
  requestAnimationFrame(schleife);
}
requestAnimationFrame(schleife);

zeichneKopf();
zeichneBedienung();


// ── Die Halteleiste ────────────────────────────────────────────────────────
//
// Entwurf 1, gewaehlt am 2026-10-04: gedrueckt halten nimmt auf, loslassen
// hoert auf. Der Finger ist die Sicherung.
//
// **Sie tastet nicht.** Hier steht kein `trx:` und kein `tune:`, und das
// bleibt so, bis der Betreiber es ausdruecklich freigibt und die Dummy-Last
// dranhaengt. Was sie tut: Mikrofon aufnehmen, TCI-Rahmen bauen, abschicken.
// Longpath verwirft sie, solange dieser Client nicht tastet
// (`m_txAudioActiveClient` in TciServer.cpp), und zaehlt sie als verworfen —
// genau das ist der Beleg, dass die Kette steht, ohne dass ein Watt auf die
// Antenne geht.
//
// Nutzen schon heute: der Pegelbalken. Ohne ihn weiss am Telefon niemand, ob
// das Mikrofon ueberhaupt etwas hoert, und das ist die erste Frage.
{
  const reihe   = $('mikrow');
  const taste   = $('mikhalt');
  const balken  = $('pegelbalken');
  const spitzeE = $('pegelspitze');
  const meldung = $('mikmeldung');

  const mik = new Mikrofon((rahmen) => {
    // Rohe Rahmen gehen am Textkanal vorbei direkt auf den Socket — `send()`
    // haengt ein Semikolon an und ist nur fuer Befehle.
    if (link.ws && link.ws.readyState === 1) { link.ws.send(rahmen); }
  });

  // Erst sagen, DASS es nicht geht, dann eine Taste anbieten. Eine Taste,
  // die beim Druecken nichts tut und nichts erklaert, ist schlimmer als
  // keine. Ueber `http` gibt es `navigator.mediaDevices` gar nicht; dagegen
  // hilft nur ein Zertifikat (handfunke/tls-einrichten.sh).
  // Wo es kein Mikrofon GEBEN kann, steht auch keine Taste dafuer.
  //
  // Erst stand hier eine abgeblendete Taste mit einer Warnung darunter. Das
  // war gut gemeint und im Alltag falsch: ueber `http` — also genau dort, wo
  // das Telefon die Seite holt — kann sie NIE etwas tun. Eine dauerhaft tote
  // Taste im besten Daumenbereich sieht aus wie ein Defekt und nimmt Platz
  // fuer etwas, das dort nicht geht. Dieselbe Ueberlegung wie bei den beiden
  // Sendetasten, die zu einer ruhigen Zeile geschrumpft sind.
  //
  // Der Grund bleibt lesbar — aber als eine Zeile, nicht als Bedienelement,
  // das zum Druecken einlaedt.
  const moeglich = mikrofonMoeglich();
  reihe.hidden = false;
  if (!moeglich) {
    taste.hidden = true;
    balken.parentElement.hidden = true;
    meldung.className = 'mikmeldung warn';
    meldung.textContent = window.isSecureContext === false
      ? 'MIKROFON BRAUCHT EIN ZERTIFIKAT (HTTPS)'
      : 'DIESES GERÄT GIBT KEIN MIKROFON HER';
  }

  let haelt = false;
  let spitzeHalten = 0;

  const anfangen = async (ev) => {
    if (ev) { ev.preventDefault(); }
    if (haelt || !moeglich) { return; }
    haelt = true;
    taste.classList.add('haelt');
    meldung.className = 'mikmeldung';
    meldung.textContent = 'NIMMT AUF …';
    if (!await mik.start()) {
      haelt = false;
      taste.classList.remove('haelt');
      meldung.className = 'mikmeldung warn';
      meldung.textContent = (mik.fehler || 'MIKROFON GING NICHT').toUpperCase();
      return;
    }
    // Zwischen Druck und Antwort kann der Finger laengst wieder weg sein —
    // getUserMedia fragt beim ersten Mal nach Erlaubnis und braucht dann
    // Sekunden. Dann sofort wieder aufhoeren, statt stumm weiterzulaufen.
    if (!haelt) { mik.stop(); }
  };

  const aufhoeren = () => {
    if (!haelt) { return; }
    haelt = false;
    taste.classList.remove('haelt');
    mik.stop();
    balken.style.width = '0%';
    spitzeE.style.left = '0%';
    spitzeHalten = 0;
    meldung.textContent = mik.strecke
      ? `${mik.strecke.rahmen} RAHMEN GESCHICKT · VERWORFEN, WEIL NICHT GETASTET`
      : '';
  };

  // Der Server sagt seit dem 2026-10-04, WARUM er einen Sendewunsch
  // ablehnt. Die Halteleiste tastet noch nicht, aber sobald sie es tut,
  // steht hier der Grund statt eines stummen Nichts — und bis dahin fängt
  // die Zeile jede Ablehnung ab, die aus einer anderen Ecke käme.
  link.addEventListener('txfehler', (e) => {
    const grund = (e.detail && e.detail.text) || 'abgelehnt';
    meldung.className = 'mikmeldung warn';
    meldung.textContent = ('SENDEN ABGELEHNT — ' + grund).toUpperCase();
  });

  taste.addEventListener('pointerdown', anfangen);
  taste.addEventListener('pointerup', aufhoeren);
  taste.addEventListener('pointercancel', aufhoeren);
  taste.addEventListener('pointerleave', aufhoeren);
  // Faellt das Fenster in den Hintergrund, ist niemand mehr da, der
  // loslaesst. Ein Mikrofon, das im Hintergrund weiterlaeuft, waere auf
  // einer Fernbedienung das Letzte, was jemand erwartet.
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) { aufhoeren(); }
  });

  // Der Balken laeuft in eigener Taktung, nicht je Tonblock: 50 Bilder je
  // Sekunde zu zeichnen kostet mehr als es zeigt.
  setInterval(() => {
    if (!haelt || !mik.strecke) { return; }
    const sp = mik.strecke.spitzeAblesen();
    const eff = mik.strecke.effektiv;
    balken.style.width = Math.min(100, eff * 140).toFixed(0) + '%';
    spitzeHalten = Math.max(spitzeHalten * 0.93, sp);
    spitzeE.style.left = Math.min(99, spitzeHalten * 100).toFixed(0) + '%';
    meldung.textContent = sp >= 0.99 ? 'ÜBERSTEUERT — LEISER SPRECHEN'
                                     : 'NIMMT AUF …';
    meldung.className = sp >= 0.99 ? 'mikmeldung warn' : 'mikmeldung';
  }, 100);
}


// ── Die Haltetasten: SENDEN und TUNE ───────────────────────────────────────
//
// Entwurf 1 (Haltetaste), gewaehlt 2026-10-04: Druecken tastet, Loslassen
// loest. Es gibt keinen Sendezustand ohne Finger. Die Logik steht in
// sendetaste.js und ist dort geprueft (pruefe-sendetaste.mjs); hier nur
// die Verdrahtung. Der Ton kommt vom Mac, das Telefon tastet nur.
//
// Ohne SENDEN_FREIGABE passiert hier NICHTS: die Entsicherung bleibt
// verborgen, beide Tasten behalten `.locked` (pointer-events:none).
{
  const tx = $('tx'), tune = $('tune'), sicher = $('txsicher');
  const taste = new Sendetasten({
    freigegeben: SENDEN_FREIGABE,
    sende: (cmd) => { if (link.ws && link.ws.readyState === 1) { link.send(cmd); return true; }
                      return false; },
  });
  const jetzt = () => performance.now();

  const zeigen = () => {
    const scharf = taste.entsichert(jetzt());
    sicher.classList.toggle('scharf', scharf);
    sicher.textContent = scharf ? 'SCHARF' : 'ENTSICHERN';
    for (const el of [tx, tune]) { el.classList.toggle('locked', !scharf); }
    const h = taste.halten;
    tx.classList.toggle('haelt', !!h && h.art === SENDEN);
    tune.classList.toggle('haelt', !!h && h.art === TUNE);
  };

  const loesen = () => { taste.loslassen(jetzt()); zeigen(); };

  if (taste.moeglich) {
    sicher.hidden = false;
    tx.innerHTML = 'SENDEN <small>HALTEN</small>';
    tune.innerHTML = 'TUNE <small>HALTEN</small>';
    sicher.addEventListener('click', () => {
      if (taste.entsichert(jetzt())) { taste.verriegeln(); } else { taste.entsichern(jetzt()); }
      zeigen();
    });

    const halten = (el, art) => {
      el.addEventListener('pointerdown', (ev) => {
        ev.preventDefault();
        try { el.setPointerCapture(ev.pointerId); } catch (_) { /* egal */ }
        taste.druecken(art, state.trx, jetzt());
        zeigen();
      });
      // Jeder Weg, auf dem der Finger weg ist, loest.
      for (const t of ['pointerup', 'pointercancel', 'lostpointercapture', 'contextmenu']) {
        el.addEventListener(t, (ev) => { ev.preventDefault(); loesen(); });
      }
    };
    halten(tx, SENDEN);
    halten(tune, TUNE);

    // Niemand mehr da, der loslassen koennte: Hintergrund, Sperren, Wegwischen.
    document.addEventListener('visibilitychange', () => { if (document.hidden) { loesen(); } });
    window.addEventListener('pagehide', loesen);
    window.addEventListener('blur', loesen);
    // Leitung weg -> verriegeln. Der Server loest selbst beim Trennen.
    link.addEventListener('state', () => {
      if (!(link.ws && link.ws.readyState === 1)) { taste.verbindungWeg(); zeigen(); }
    });
    // Lehnt der Server ab, wird geloest und der Grund gesagt.
    link.addEventListener('txfehler', (e) => {
      taste.loslassen(0); taste.verriegeln(); zeigen();
      const m = $('mikmeldung');
      if (m) { m.className = 'mikmeldung warn';
               m.textContent = 'SENDEN ABGELEHNT \u2014 ' + ablehnText(e.detail && e.detail.text); }
    });
    setInterval(() => { if (taste.takt(jetzt())) { zeigen(); } }, 250);
  }
}


// ── Mac-Look: Zeigerinstrument, Rotorzeile, ON AIR ──────────────────────────
// Die Zahlen schreibt weiter die Schleife oben nach #smeter/#sbar/#rotorGrad;
// hier werden sie nur in das neue Instrument uebersetzt. Kein zweiter Weg zu
// den Messwerten, damit beide Anzeigen nie auseinanderlaufen.
{
  const striche = $('gaugeStriche');
  for (let i = 0; i <= 8; i++) {
    const w = (-62 + i * 15.5) * Math.PI / 180;
    const l = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    l.setAttribute('x1', (66 + 46 * Math.sin(w)).toFixed(1));
    l.setAttribute('y1', (60 - 46 * Math.cos(w)).toFixed(1));
    l.setAttribute('x2', (66 + 54 * Math.sin(w)).toFixed(1));
    l.setAttribute('y2', (60 - 54 * Math.cos(w)).toFixed(1));
    l.setAttribute('stroke', i > 6 ? '#a86b6d' : '#7e7e85');
    l.setAttribute('stroke-width', '1.5');
    striche.appendChild(l);
  }
  const rotorFeld = $('rotorFeld');
  $('rotorKopf').addEventListener('click', () => {
    const offen = rotorFeld.classList.toggle('offen');
    $('rotorChev').textContent = offen ? '▾' : '▸';
  });
  setInterval(() => {
    const t = ($('smeter').textContent || '—').split('·');
    $('sval').textContent = (t[0] || '—').trim();
    $('sdb').textContent = (t[1] || '').trim();
    const w = parseFloat($('sbar').style.width) || 0;
    $('gaugeNadel').setAttribute('transform', 'rotate(' + (-62 + 1.24 * w) + ' 66 60)');
    $('rotorKopfGrad').textContent = ($('rotorGrad').textContent || '').trim();
    $('onAir').classList.toggle('an', !!(link.ready && (link.st.mox || link.st.tune)));
  }, 250);
}
