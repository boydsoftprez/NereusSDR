#!/usr/bin/env node
// Pruefstand fuer die Haltetasten (SENDEN und TUNE).
//
// Drei Teile:
//   1. die Logik in sendetaste.js, ohne Netz;
//   2. der Quelltext-Waechter: der Riegel SENDEN_FREIGABE steht auf false, und
//      ausser sendetaste.js schickt keine Datei ein `trx:`/`tune:` mit true;
//   3. die Kette gegen die Attrappe (attrappe.py): ohne --sendenfrei wird
//      abgelehnt, mit --sendenfrei kommt true beim Druecken und false beim
//      Loslassen an. Es entsteht nirgends ein Watt.
//
//     node pruefe-sendetaste.mjs

import { Sendetasten, SENDEN, TUNE, ENTSICHERT_MS, HALTE_MAX_MS, ablehnText }
  from './sendetaste.js';
import { readFileSync, readdirSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const hier = dirname(fileURLToPath(import.meta.url));
let fehler = 0;
const pruefe = (name, ok, text = '') => {
  if (ok) { console.log(`  ok    ${name}`); }
  else { console.log(`  FEHLT ${name}${text ? ': ' + text : ''}`); fehler++; }
};

const neu = (frei = true, leitung = true) => {
  const gesendet = [];
  const t = new Sendetasten({
    freigegeben: frei,
    sende: (c) => { if (!leitung) { return false; } gesendet.push(c); return true; },
  });
  return { t, gesendet };
};

console.log('Ohne Freigabe tastet nichts\n');
{
  const { t, gesendet } = neu(false);
  pruefe('nicht entsicherbar', t.entsichern(0) === false);
  pruefe('Druecken -> nicht-freigegeben',
         t.druecken(SENDEN, 0, 1) === 'nicht-freigegeben');
  pruefe('TUNE ebenso', t.druecken(TUNE, 0, 1) === 'nicht-freigegeben');
  pruefe('kein einziger Befehl raus', gesendet.length === 0);
  pruefe('Vorgabe ohne Angabe ist NICHT freigegeben',
         new Sendetasten({ sende: () => true }).freigegeben === false);
}

console.log('\nMit Freigabe, aber gesichert\n');
{
  const { t, gesendet } = neu(true);
  pruefe('gesichert -> kein Tasten', t.druecken(SENDEN, 0, 1000) === 'gesichert');
  pruefe('kein Befehl', gesendet.length === 0);
  t.entsichern(1000);
  pruefe('entsichert -> sendet', t.druecken(SENDEN, 0, 1100) === 'sendet');
  pruefe('trx:0,true ging raus', gesendet[0] === 'trx:0,true');
  pruefe('zweiter Druck waehrend des Haltens: belegt',
         t.druecken(TUNE, 0, 1200) === 'belegt');
  pruefe('TUNE-Befehl ging nicht raus', gesendet.length === 1);
  t.loslassen(2000);
  pruefe('Loslassen -> trx:0,false', gesendet[1] === 'trx:0,false');
  pruefe('danach nichts mehr gehalten', t.halten === null);
}

console.log('\nDie Entsicherung verfaellt\n');
{
  const { t, gesendet } = neu(true);
  t.entsichern(0);
  pruefe('nach der Frist gesichert', t.druecken(SENDEN, 0, ENTSICHERT_MS + 1) === 'gesichert');
  pruefe('takt meldet verriegelt', t.takt(ENTSICHERT_MS + 1) === 'verriegelt');
  pruefe('kein Befehl', gesendet.length === 0);
}

console.log('\nTUNE: gleiche Haltetaste, eigener Befehl\n');
{
  const { t, gesendet } = neu(true);
  t.entsichern(0);
  pruefe('sendet', t.druecken(TUNE, 1, 10) === 'sendet');
  t.loslassen(20);
  pruefe('tune:1,true dann tune:1,false',
         gesendet.join('|') === 'tune:1,true|tune:1,false');
}

console.log('\nKein Sendezustand ohne Finger\n');
{
  // Jeder Weg, auf dem der Finger weg ist, ruft loslassen(): Seite im
  // Hintergrund, pointercancel ... Hier der Kern: loslassen loest, auch
  // wenn es zweimal kommt, und es kommt keine Zustandsleiche zurueck.
  const { t, gesendet } = neu(true);
  t.entsichern(0);
  t.druecken(SENDEN, 0, 10);
  t.loslassen(20); t.loslassen(30);
  pruefe('zweimal loslassen schickt nur EIN false',
         gesendet.filter((c) => c.endsWith('false')).length === 1);

  // Sendezeit: nach HALTE_MAX_MS loest die Seite von selbst.
  const b = neu(true);
  b.t.entsichern(0);
  b.t.druecken(SENDEN, 0, 0);
  pruefe('vor der Grenze: noch gehalten', b.t.takt(HALTE_MAX_MS - 1) === null && b.t.halten !== null);
  pruefe('an der Grenze: Deckel', b.t.takt(HALTE_MAX_MS) === 'deckel');
  pruefe('Deckel schickt false', b.gesendet.at(-1) === 'trx:0,false');
  pruefe('nach dem Deckel gesichert (neu entsichern noetig)',
         b.t.druecken(SENDEN, 0, HALTE_MAX_MS + 1) === 'gesichert');
  pruefe('eigene Grenze liegt unter der des Servers (180 s)', HALTE_MAX_MS < 180000);

  // Leitung weg: es gibt nichts mehr zu senden, alles verriegelt.
  const c = neu(true);
  c.t.entsichern(0);
  c.t.druecken(SENDEN, 0, 0);
  c.t.verbindungWeg();
  pruefe('Leitung weg -> nicht mehr gehalten', c.t.halten === null);
  pruefe('Leitung weg -> verriegelt', c.t.druecken(SENDEN, 0, 1) === 'gesichert');

  // Ohne Leitung gilt Druecken nicht als getastet.
  const d = neu(true, false);
  d.t.entsichern(0);
  pruefe('ohne Leitung -> keine-leitung', d.t.druecken(SENDEN, 0, 1) === 'keine-leitung');
  pruefe('und es wird nichts gehalten', d.t.halten === null);

  const e = neu(true);
  e.t.entsichern(0);
  pruefe('ungueltiger Empfaenger -> nein', e.t.druecken(SENDEN, -1, 1) === 'kein-empfaenger');
  pruefe('unbekannte Art -> nein', e.t.druecken('mox', 0, 1) === 'unbekannt');
  pruefe('Ablehnungstext nennt den Schalter',
         /TCI Server/.test(ablehnText('nicht freigegeben')));
}

console.log('\nQuelltext-Waechter\n');
{
  const app = readFileSync(join(hier, 'app.js'), 'utf8');
  pruefe('SENDEN_FREIGABE steht auf false',
         /const SENDEN_FREIGABE = false;/.test(app));
  const verdaechtig = [];
  for (const f of readdirSync(hier).filter((n) => /\.(js|html)$/.test(n))) {
    if (f === 'sendetaste.js') { continue; }
    const q = readFileSync(join(hier, f), 'utf8');
    // Ein Befehl, der den Sender tastet: trx:/tune: mit true, ausser in
    // Kommentaren und Texten, die nur davon erzaehlen.
    for (const [i, z] of q.split('\n').entries()) {
      if (/^\s*(\/\/|\*|\/\*)/.test(z)) { continue; }
      if (/['"`](trx|tune):[^'"`]*,\s*(true|\$\{)/.test(z)) { verdaechtig.push(`${f}:${i + 1}`); }
    }
  }
  pruefe('ausser sendetaste.js tastet keine Datei', verdaechtig.length === 0,
         verdaechtig.join(', '));
}

// ── Die Kette gegen die Attrappe ───────────────────────────────────────────
const attrappe = (port, extra) => new Promise((ok, nein) => {
  const p = spawn('python3', ['-I', join(hier, 'attrappe.py'), '--port', String(port),
                              '--still', ...extra], { stdio: ['ignore', 'pipe', 'pipe'] });
  let aus = '';
  const fertig = (s) => { aus += s; if (/lausch|listen|bereit|:\d+/i.test(aus)) { ok({ p, aus: () => aus }); } };
  p.stdout.on('data', (d) => fertig(d.toString()));
  p.stderr.on('data', (d) => fertig(d.toString()));
  p.on('error', nein);
  setTimeout(() => ok({ p, aus: () => aus }), 1500);
});

const verbinden = (port) => new Promise((ok, nein) => {
  const ws = new WebSocket(`ws://127.0.0.1:${port}/`);
  const zeilen = [];
  ws.onmessage = (e) => { if (typeof e.data === 'string') {
    zeilen.push(...e.data.split(';').filter(Boolean)); } };
  ws.onopen = () => ok({ ws, zeilen });
  ws.onerror = () => nein(new Error('keine Verbindung'));
});
const warte = (ms) => new Promise((r) => setTimeout(r, ms));

async function kette(port, extra, frei) {
  const a = await attrappe(port, extra);
  try {
    const { ws, zeilen } = await verbinden(port);
    await warte(300);
    const t = new Sendetasten({
      freigegeben: true,
      sende: (c) => { if (ws.readyState !== 1) { return false; } ws.send(c + ';'); return true; },
    });
    t.entsichern(0);
    t.druecken(SENDEN, 0, 1);
    await warte(300);
    t.loslassen(2);
    await warte(300);
    const text = zeilen.join('|');
    if (frei) {
      pruefe('Attrappe: trx:0,true kommt zurueck', /trx:0,true/.test(text), text.slice(-200));
      pruefe('Attrappe: trx:0,false kommt zurueck', /trx:0,false/.test(text));
    } else {
      pruefe('Attrappe ohne Freigabe: tx_err mit Grund', /tx_err:nicht freigegeben/.test(text));
      pruefe('Attrappe ohne Freigabe: KEIN trx:0,true zurueck', !/trx:0,true/.test(text));
    }
    ws.close();
  } finally { a.p.kill(); }
}

console.log('\nKette gegen die Attrappe\n');
try {
  await kette(50197, [], false);
  await kette(50198, ['--sendenfrei'], true);
} catch (e) {
  pruefe('Kette lief', false, String(e));
}

console.log(fehler ? `\n${fehler} FEHLT` : '\nalles ok');
process.exit(fehler ? 1 : 0);
