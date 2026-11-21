// Die Sendetasten der App — die reinen Teile.
//
// Kein DOM, kein WebSocket. Hier steht, was am Telefon still falsch sein
// kann, wenn am anderen Ende ein Sender steht:
//
//   1. **Es gibt keinen Sendezustand ohne Finger.** Haltetaste, Martins Wahl
//      vom 2026-10-04 (docs/design/2026-10-04-sendetaste, Entwurf 1):
//      Druecken tastet, Loslassen loest. Verlaesst die Seite den Vordergrund,
//      reisst die Verbindung ab, kommt ein pointercancel, ist die Zeit um —
//      es wird geloest. Die Seite ZUSAGT das nicht allein: iOS friert eine
//      Seite mitten im Halten ein, ohne etwas zu feuern. Fuer genau diesen
//      Fall sitzen im Longpath-Server Wachhund, Besitzer und Sendezeit-Deckel
//      (TciServer.cpp); dieser Code verlaesst sich darauf und ersetzt sie nicht.
//
//   2. **Ohne Entsicherung tastet nichts.** Zwei Riegel:
//        a) `freigegeben` — ein Schalter im QUELLTEXT (app.js, SENDEN_FREIGABE).
//           Steht er auf false, ist die Taste nicht entsicherbar und es
//           verlaesst nie ein `trx:`/`tune:` mit `true` diese Seite. Er wird
//           erst nach Martins ausdruecklicher Freigabe im Chat umgelegt.
//        b) die Entsicherung auf der Seite: ein bewusster Griff, der nach
//           einigen Sekunden wieder verfaellt. Ein Fehltipp in der Tasche sendet
//           nicht.
//
//   3. **Ein Weg zur Zeit.** SENDEN (`trx:`) und TUNE (`tune:`) schliessen
//      einander aus; wer eine hat, bekommt die andere nicht.
//
//   4. **Eigene Zeitgrenze unter der des Servers.** Der Server wirft bei
//      180 s ab. Die Seite loest vorher (60 s) und verlangt einen neuen Druck.

export const ENTSICHERT_MS = 15000;   // Entsicherung verfaellt
export const HALTE_MAX_MS  = 60000;   // laengstes Halten, dann wird geloest

export const SENDEN = 'trx', TUNE = 'tune';

export class Sendetasten {
  /**
   * @param {object} o
   * @param {boolean} o.freigegeben  Quelltext-Riegel; false = nie tasten.
   * @param {(cmd:string)=>boolean} o.sende  schickt einen Befehl; false, wenn
   *        die Leitung nicht steht (dann gilt auch nichts als getastet).
   */
  constructor({ freigegeben = false, sende,
                entsichertMs = ENTSICHERT_MS, haltMaxMs = HALTE_MAX_MS } = {}) {
    this.freigegeben = freigegeben === true;
    this.sende = sende;
    this.entsichertMs = entsichertMs;
    this.haltMaxMs = haltMaxMs;
    this.entsichertBis = 0;
    this.halten = null;      // null | { art, rx, seit }
  }

  /** Gibt es die Taste ueberhaupt als scharf schaltbar? */
  get moeglich() { return this.freigegeben; }

  entsichert(jetzt) { return this.freigegeben && jetzt < this.entsichertBis; }

  /** Der bewusste Griff. Ohne Freigabe im Quelltext: nein. */
  entsichern(jetzt) {
    if (!this.freigegeben) { return false; }
    this.entsichertBis = jetzt + this.entsichertMs;
    return true;
  }

  verriegeln() { this.entsichertBis = 0; }

  /** Taste gedrueckt. Gibt 'sendet' oder den Grund des Nein zurueck. */
  druecken(art, rx, jetzt) {
    if (art !== SENDEN && art !== TUNE) { return 'unbekannt'; }
    if (!this.freigegeben)          { return 'nicht-freigegeben'; }
    if (!this.entsichert(jetzt))    { return 'gesichert'; }
    if (this.halten)                { return 'belegt'; }
    if (!Number.isInteger(rx) || rx < 0) { return 'kein-empfaenger'; }
    if (!this.sende(`${art}:${rx},true`)) { return 'keine-leitung'; }
    this.halten = { art, rx, seit: jetzt };
    return 'sendet';
  }

  /**
   * Loslassen — und jeder Fall, der wie Loslassen behandelt wird
   * (pointercancel, Seite im Hintergrund, Zeit um, Server lehnt ab).
   * Das `false` geht auch dann raus, wenn der Zustand schon geloest scheint:
   * ein doppeltes `false` schadet nie, ein fehlendes schon.
   */
  loslassen(jetzt = 0) {
    const h = this.halten;
    this.halten = null;
    if (h) {
      this.sende(`${h.art}:${h.rx},false`);
      // Wer gerade gesendet hat, soll nicht sofort neu entsichern muessen,
      // aber auch nicht ewig scharf bleiben: ab jetzt wieder die volle Frist.
      if (jetzt) { this.entsichertBis = jetzt + this.entsichertMs; }
    }
    return h !== null;
  }

  /** Die Leitung ist weg: es gibt nichts mehr zu senden. Der Server loest
   *  selbst (onClientDisconnected). Alles verriegelt. */
  verbindungWeg() {
    this.halten = null;
    this.entsichertBis = 0;
  }

  /** Regelmaessig aufrufen (alle ~250 ms). */
  takt(jetzt) {
    if (this.halten && jetzt - this.halten.seit >= this.haltMaxMs) {
      this.loslassen(0);
      this.entsichertBis = 0;   // nach dem Deckel: neu entsichern
      return 'deckel';
    }
    if (!this.halten && this.entsichertBis && jetzt >= this.entsichertBis) {
      this.entsichertBis = 0;
      return 'verriegelt';
    }
    return null;
  }
}

/** Text unter der Taste nach einer Ablehnung vom Server. */
export function ablehnText(roh) {
  const t = (roh || '').trim();
  if (/nicht freigegeben/i.test(t)) {
    return 'NICHT FREIGEGEBEN — Setup › CAT & Network › TCI Server';
  }
  return t ? t.toUpperCase() : 'ABGELEHNT';
}
