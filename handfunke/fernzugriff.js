/**
 * fernzugriff.js — die App von draussen, ohne das eigene Netz.
 *
 * Im eigenen WLAN spricht die App Longpaths TCI-Server direkt an. Von
 * unterwegs geht das nicht: die Adresse ist privat, und daran aendert kein
 * Wunsch etwas. Dieser Weg fuehrt ueber einen Rendezvous -- einen kleinen
 * Vermittlungsdienst, bei dem sich die Station anmeldet und unter dem ein
 * Telefon sie findet. Die Sitzung selbst laeuft NICHT ueber ihn: er
 * stellt nur vor, danach sucht sich WebRTC den Weg (im eigenen Netz
 * direkt, sonst ueber STUN, sonst ueber TURN).
 *
 * Massgeblich ist `docs/architecture/2026-09-23-rendezvous-v1.md` im
 * Upstream, Abschnitte 4.1, 4.4, 5 und 6.3. Wo diese Datei und das
 * Dokument sich widersprechen, hat das Dokument recht.
 *
 * **Was hier NICHT passiert: raten.** Die Gegenstelle laesst vor der
 * Anmeldung kein einziges TCI-Byte durch, und die Unterschrift dafuer
 * kommt aus einem Schluessel, den dieses Telefon nie herausgibt --
 * `extractable: false`, er lebt in IndexedDB und laesst sich auch von
 * einem Skript auf dieser Seite nicht auslesen.
 *
 * Nach aussen sieht `FernLeitung` aus wie ein WebSocket: `onopen`,
 * `onmessage`, `onclose`, `onerror`, `send`, `close`, `binaryType`,
 * `readyState`. Darum musste an `tci.js` fast nichts geaendert werden --
 * nur, woher die Leitung kommt.
 */

// ── Kleinkram ───────────────────────────────────────────────────────────────

export function b64url(roh) {
  const b = new Uint8Array(roh);
  let s = '';
  for (let i = 0; i < b.length; i++) { s += String.fromCharCode(b[i]); }
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

export function unb64url(text) {
  const s = text.replace(/-/g, '+').replace(/_/g, '/');
  const roh = atob(s + '='.repeat((4 - (s.length % 4)) % 4));
  const b = new Uint8Array(roh.length);
  for (let i = 0; i < roh.length; i++) { b[i] = roh.charCodeAt(i); }
  return b;
}

function verkette(...teile) {
  const n = teile.reduce((s, t) => s + t.length, 0);
  const aus = new Uint8Array(n);
  let o = 0;
  for (const t of teile) { aus.set(t, o); o += t.length; }
  return aus;
}

const TEXT = new TextEncoder();

// Die Praefixe sind Teil des Protokolls. Ein Tippfehler hier faellt erst
// auf, wenn die Station schweigt -- und Schweigen ist genau das, was sie
// auch einem Fremden gegenueber tut.
const PRAEFIX_INTRODUCE = TEXT.encode('NereusSDR introduce v1\n');
const PRAEFIX_ANMELDUNG = TEXT.encode('Longpath fernzugriff anmeldung v1\n');

const KANAL = 'longpath-tci';

// ── Opus ──────────────────────────────────────────────────────────────────
//
// Der Ton ist der teuerste Teil der Strecke: 24,9 kB/s roh, 13,3 als
// mu-law, 4,0 als Opus (am 2026-10-28 gemessen). Die Station rechnet ihn
// um, sobald dieses Telefon beim Anmelden sagt, dass es Opus kann.
//
// `AudioDecoder` ist `[SecureContext]` -- auf `http://172.30.30.x` gibt
// es ihn nicht, und genau daran ist Opus im September gescheitert. Von
// einem https-Ort gibt es ihn. Wer ihn nicht hat, bekommt weiter mu-law,
// und zwar ohne dass irgendwo etwas einzustellen waere.
const OPUS_MARKE = 0x4c504f31;   // "LPO1", im Kopf der Station
const OPUS_KOPF = 10;

function kannOpus() {
  return typeof AudioDecoder !== 'undefined' && typeof AudioData !== 'undefined';
}

// ── TURN-Zugangsdaten, aufgehoben ───────────────────────────────────────────
//
// Der Vermittler gibt TURN-Zugangsdaten erst in der Antwort auf eine
// Einfuehrung heraus (Abschnitt 8) -- ein Browser sammelt seine Kandidaten
// aber schon beim Anlegen des Angebots, also BEVOR sie da sind, und kann
// einen TURN-Weg nachtraeglich nur mit einem neuen Angebot (ICE-Neustart)
// aufnehmen, den es hier nicht gibt. Die Zugangsdaten gelten 24 Stunden und
// sind an die Station gebunden; darum werden sie aufgehoben und der NAECHSTE
// Versuch geht damit los (genauso macht es die Station). Im Mobilfunk hinter
// CGNAT ist das der Unterschied zwischen "kommt durch" und "nie".
const TURN_RESERVE_S = 600;

function turnLesen(stationId) {
  try {
    const t = JSON.parse(localStorage.getItem('handfunke.turn.' + stationId) || 'null');
    if (t && t.urls && t.expires - Date.now() / 1000 > TURN_RESERVE_S) { return t; }
  } catch (e) { /* kein Speicher: dann eben ohne */ }
  return null;
}

function turnMerken(stationId, turn) {
  try { localStorage.setItem('handfunke.turn.' + stationId, JSON.stringify(turn)); }
  catch (e) { /* siehe oben */ }
}

// ── Der Geraeteschluessel ───────────────────────────────────────────────────

const DB = 'longpath-fernzugriff';
const LADEN = 'schluessel';

function db() {
  return new Promise((ok, weg) => {
    const a = indexedDB.open(DB, 1);
    a.onupgradeneeded = () => a.result.createObjectStore(LADEN);
    a.onsuccess = () => ok(a.result);
    a.onerror = () => weg(a.error);
  });
}

function ausLaden(d, schluessel) {
  return new Promise((ok, weg) => {
    const a = d.transaction(LADEN, 'readonly').objectStore(LADEN).get(schluessel);
    a.onsuccess = () => ok(a.result);
    a.onerror = () => weg(a.error);
  });
}

function inLaden(d, schluessel, wert) {
  return new Promise((ok, weg) => {
    const a = d.transaction(LADEN, 'readwrite').objectStore(LADEN).put(wert, schluessel);
    a.onsuccess = () => ok();
    a.onerror = () => weg(a.error);
  });
}

/**
 * Den Schluessel dieses Telefons holen, und beim ersten Mal erzeugen.
 *
 * `extractable: false` ist hier kein Zierrat: der Schluessel IST das
 * Telefon. Ein auslesbarer Schluessel in localStorage waere eine Kopie,
 * die jedes Skript auf dieser Seite mitnehmen kann.
 */
export async function geraeteschluessel() {
  const d = await db();
  let paar = await ausLaden(d, 'paar');
  if (!paar) {
    paar = await crypto.subtle.generateKey(
      { name: 'ECDSA', namedCurve: 'P-256' }, false, ['sign', 'verify']);
    await inLaden(d, 'paar', paar);
  }
  const spki = new Uint8Array(await crypto.subtle.exportKey('spki', paar.publicKey));
  const abdruck = new Uint8Array(await crypto.subtle.digest('SHA-256', spki));
  return {
    privat: paar.privateKey,
    spki,
    schluesselText: b64url(spki),   // was beim Koppeln uebergeben wird
    ident: b64url(abdruck),         // die Geraete-Id, 43 Zeichen
  };
}

/**
 * Unterschreiben. WebCrypto liefert ECDSA schon als rohes r||s (64 Byte)
 * -- genau das Format des Protokolls, kein DER-Umweg noetig.
 */
async function unterschreibe(privat, daten) {
  const sig = await crypto.subtle.sign(
    { name: 'ECDSA', hash: 'SHA-256' }, privat, daten);
  if (sig.byteLength !== 64) {
    throw new Error(`Unterschrift ist ${sig.byteLength} Byte lang, erwartet sind 64`);
  }
  return b64url(sig);
}

// ── Die Leitung ─────────────────────────────────────────────────────────────

/**
 * Sieht aus wie ein WebSocket, ist aber eine WebRTC-Verbindung durch den
 * Rendezvous.
 *
 * `url` ist der Dienst (`wss://…`), `stationId` die 26 Zeichen der
 * Station. Beides steht im Kopplungsblatt.
 */
export class FernLeitung extends EventTarget {
  constructor(dienstUrl, stationId) {
    super();
    this.dienstUrl = dienstUrl;
    this.stationId = stationId;
    this.binaryType = 'arraybuffer';
    this.readyState = 0;            // CONNECTING
    this.onopen = this.onmessage = this.onclose = this.onerror = null;
    this.weg = '';                  // 'direkt', 'stun' oder 'relay' -- fuer die Anzeige
    this.ton = '';                  // 'opus' oder 'roh' -- fuer die Fusszeile
    // Was WIRKLICH ueber die Leitung ging. Noetig, weil `tci.js` erst
    // zaehlt, was aus dem Dekoder herauskommt: ein Opus-Block von 4 kB/s
    // wird dort als 96 kB/s Int16 gezaehlt, und die Ersparnis waere
    // nirgends zu sehen.
    this.tonKbs = 0;
    this._tonBytes = 0;
    this._tonSeit = 0;
    this._opus = null;              // der AudioDecoder, wenn Opus laeuft
    this._opusRate = 0;
    this._pc = null;
    this._kanal = null;
    this._ws = null;
    this._angemeldet = false;
    this._los();
  }

  async _los() {
    try {
      await this._verbinde();
    } catch (e) {
      this._fehler(e);
    }
  }

  async _verbinde() {
    const ich = await geraeteschluessel();
    const ws = new WebSocket(this.dienstUrl);
    this._ws = ws;
    ws.binaryType = 'arraybuffer';

    const hallo = await new Promise((ok, weg) => {
      ws.onmessage = (ev) => ok(JSON.parse(ev.data));
      ws.onerror = () => weg(new Error('der Vermittlungsdienst ist nicht erreichbar'));
      ws.onclose = () => weg(new Error('der Vermittlungsdienst hat zugemacht'));
    });
    if (hallo.type !== 'hello') { throw new Error(`erst kam ${hallo.type}, nicht hello`); }

    const stun = (hallo.stun || []).map((u) => ({ urls: u }));
    const aufgehoben = turnLesen(this.stationId);
    this._mitTurn = !!aufgehoben;
    if (aufgehoben) {
      stun.push({ urls: aufgehoben.urls, username: aufgehoben.username,
                  credential: aufgehoben.password });
    }
    const pc = new RTCPeerConnection({ iceServers: stun, iceCandidatePoolSize: 0 });
    this._pc = pc;

    const kanal = pc.createDataChannel(KANAL, { ordered: true });
    kanal.binaryType = 'arraybuffer';
    this._kanal = kanal;
    this._verdrahte(kanal, ich);

    // Kandidaten einzeln nachreichen (trickle): ein Angebot, das auf das
    // Sammeln wartet, kostet im Mobilfunk mehrere Sekunden, bevor
    // ueberhaupt etwas losgeht.
    pc.onicecandidate = (ev) => {
      if (ws.readyState !== WebSocket.OPEN) { return; }
      ws.send(JSON.stringify({
        type: 'candidate',
        // Ein leerer Kandidat ist das Ende der Reihe und wird genauso
        // weitergereicht (Abschnitt 5.2).
        candidate: ev.candidate ? ev.candidate.candidate.replace(/^a=/, '') : '',
      }));
    };

    const angebot = await pc.createOffer();
    await pc.setLocalDescription(angebot);

    const transkript = verkette(
      PRAEFIX_INTRODUCE, TEXT.encode(this.stationId), unb64url(hallo.nonce));

    ws.onmessage = (ev) => this._vomDienst(JSON.parse(ev.data));
    ws.onclose = () => {
      // Sobald die Sitzung steht, ist der Dienst entbehrlich. Vorher ist
      // sein Wegfall das Ende des Versuchs.
      if (!this._angemeldet) { this._fehler(new Error('der Dienst ging weg')); }
    };
    ws.send(JSON.stringify({
      type: 'introduce',
      id: this.stationId,
      device: ich.ident,
      deviceSignature: await unterschreibe(ich.privat, transkript),
      offer: pc.localDescription.sdp,
    }));
  }

  async _vomDienst(m) {
    const pc = this._pc;
    if (m.type === 'answer') {
      // Die TURN-Zugangsdaten kommen hier an. Sie jetzt noch zu setzen
      // hat Wirkung: der Browser sammelt danach weiter und findet den
      // Relay-Weg, auch wenn direkt und STUN nichts ergeben haben.
      //
      // Nachtraeglich setzen hilft dem laufenden Versuch NICHT: ohne neues
      // Angebot sammelt der Browser keine TURN-Kandidaten mehr. Darum werden
      // sie aufgehoben, und lief dieser Versuch ohne sie, kommt nach ein paar
      // Sekunden ein zweiter, der sie von Anfang an hat.
      if (m.turn && m.turn.urls) {
        turnMerken(this.stationId, m.turn);
        if (!this._mitTurn && !this._zweiter) {
          this._zweiter = true;
          this._zweiterTimer = setTimeout(() => this._nocheinmal(), 6000);
        }
      }
      await pc.setRemoteDescription({ type: 'answer', sdp: m.answer });
    } else if (m.type === 'candidate') {
      if (m.candidate) {
        try { await pc.addIceCandidate({ candidate: m.candidate, sdpMLineIndex: 0 }); }
        catch (e) { console.warn('Kandidat verworfen:', e); }
      }
    } else if (m.type === 'error') {
      this._fehler(new Error(`${m.code}: ${m.reason || ''}`));
    } else if (m.type === 'introduction.end') {
      // Das ist der Normalfall, NACHDEM die Sitzung steht. Vorher heisst
      // es: die Station hat nicht geantwortet.
      if (!this._angemeldet) {
        this._fehler(new Error(m.code === 'expired'
          ? 'die Station hat nicht geantwortet — ist dieses Telefon dort gekoppelt?'
          : `die Einfuehrung endete: ${m.code}`));
      }
    }
  }

  /** Den Versuch verwerfen und noch einmal beginnen, jetzt mit TURN. */
  _nocheinmal() {
    // Steht ICE schon (nur die Anmeldung laeuft noch), wird nichts verworfen.
    if (this._angemeldet || this.readyState === 3
        || this._pc?.connectionState === 'connected') { return; }
    // Die alten Handler duerfen die Leitung nicht mehr schliessen.
    if (this._kanal) { this._kanal.onclose = null; this._kanal.onerror = null; }
    if (this._tonkanal) { this._tonkanal.onmessage = null; }
    if (this._ws) { this._ws.onclose = null; this._ws.onmessage = null; }
    try { this._kanal?.close(); } catch (e) {}
    try { this._tonkanal?.close(); } catch (e) {}
    try { this._pc?.close(); } catch (e) {}
    try { this._ws?.close(); } catch (e) {}
    this._pc = this._kanal = this._tonkanal = this._ws = null;
    this._los();
  }

  _verdrahte(kanal, ich) {
    kanal.onmessage = async (ev) => {
      if (!this._angemeldet) {
        await this._anmelden(kanal, ich, ev.data);
        return;
      }
      // Ein Opus-Rahmen wird hier aufgeloest und als gewoehnlicher
      // TCI-Tonrahmen weitergereicht; alles andere geht durch.
      if (this.ton === 'opus' && ev.data instanceof ArrayBuffer
          && this._opusRahmen(ev.data)) {
        return;
      }
      this.onmessage?.({ data: ev.data });
    };
    kanal.onclose = () => this._zu(1006, 'der Kanal ging zu');
    kanal.onerror = (e) => this._fehler(e);
  }

  async _anmelden(kanal, ich, daten) {
    let m;
    try { m = JSON.parse(typeof daten === 'string' ? daten : ''); }
    catch (e) { return; }

    if (m.typ === 'anmelden') {
      const transkript = verkette(PRAEFIX_ANMELDUNG, unb64url(m.aufforderung));
      kanal.send(JSON.stringify({
        typ: 'anmeldung',
        signatur: await unterschreibe(ich.privat, transkript),
        kann: kannOpus() ? ['opus'] : [],
      }));
    } else if (m.typ === 'offen') {
      this.ton = m.ton || 'roh';
      if (this.ton === 'opus') { this._richteOpusEin(); }
      this._angemeldet = true;
      clearTimeout(this._zweiterTimer);
      this.readyState = 1;          // OPEN
      this.weg = await this._welcherWeg();
      // Der Dienst hat seine Arbeit getan. Ihn jetzt zu schliessen ist
      // ausdruecklich vorgesehen (Abschnitt 6.3) -- die Station bekommt
      // `clientLeft` und laesst die Sitzung stehen.
      try { this._ws.close(); } catch (e) {}
      this.onopen?.({});
    } else if (m.typ === 'fehler') {
      this._fehler(new Error(m.grund || 'die Station sagt ab'));
    }
  }

  // ── Der Opus-Weg ────────────────────────────────────────────────────────

  _richteOpusEin() {
    // Der Dekoder wird beim ERSTEN Paket eingerichtet, nicht hier: erst
    // dann steht die Abtastrate fest, und sie steht im Kopf der Station.
    this._opus = null;
    this._opusRate = 0;
  }

  /**
   * Einen Opus-Rahmen der Station in einen gewoehnlichen TCI-Tonrahmen
   * verwandeln, damit `tci.js` nichts davon merkt.
   *
   * **Opus gibt immer 48 kHz zurueck**, gleich womit es gefuettert wurde.
   * Das ist kein Fehler, sondern wie libopus arbeitet, und hier sogar
   * guenstig: der AudioContext laeuft ohnehin auf 48 kHz, das Hochrechnen
   * von 12 entfaellt. Der gebaute Rahmen traegt darum 48000 als Rate.
   */
  _opusRahmen(puffer) {
    const sicht = new DataView(puffer);
    if (sicht.byteLength < OPUS_KOPF || sicht.getUint32(0, false) !== OPUS_MARKE) {
      return false;
    }
    const rate = sicht.getUint16(4, true) * 50;
    const empfaenger = sicht.getUint8(8);
    const paket = new Uint8Array(puffer, OPUS_KOPF);

    // Vor dem Dekodieren zaehlen -- danach ist die Zahl eine andere.
    this._tonBytes += puffer.byteLength;
    const jetzt = performance.now();
    if (!this._tonSeit) { this._tonSeit = jetzt; }
    else if (jetzt - this._tonSeit >= 1000) {
      this.tonKbs = Math.round(this._tonBytes / (jetzt - this._tonSeit));
      this._tonBytes = 0;
      this._tonSeit = jetzt;
    }

    if (this._opus === null || this._opusRate !== rate) {
      this._opusRate = rate;
      this._opus = new AudioDecoder({
        output: (daten) => this._opusAusgabe(daten, empfaenger),
        error: (e) => {
          console.warn('Opus-Dekoder:', e);
          // Nicht die Verbindung abbrechen: ohne Ton laesst sich immer
          // noch sehen und bedienen.
          this._opus = null;
        },
      });
      this._opus.configure({ codec: 'opus', sampleRate: rate, numberOfChannels: 1 });
    }
    try {
      this._opus.decode(new EncodedAudioChunk({
        type: 'key',          // jeder Opus-Rahmen steht fuer sich
        timestamp: 0,
        data: paket,
      }));
    } catch (e) {
      console.warn('Opus-Paket verworfen:', e);
    }
    return true;
  }

  _opusAusgabe(daten, empfaenger) {
    try {
      const n = daten.numberOfFrames;
      const f32 = new Float32Array(n);
      daten.copyTo(f32, { planeIndex: 0, format: 'f32-planar' });
      const i16 = new Int16Array(n);
      for (let i = 0; i < n; i++) {
        const v = f32[i];
        i16[i] = Math.max(-32768, Math.min(32767, Math.round(v * 32767)));
      }
      // Ein gewoehnlicher TCI-Rahmen: 64-Byte-Kopf, Int16, Tonstrom.
      const aus = new ArrayBuffer(64 + i16.byteLength);
      const kopf = new Uint32Array(aus, 0, 16);
      kopf[0] = empfaenger;
      kopf[1] = daten.sampleRate;
      kopf[2] = 0;            // Int16
      kopf[5] = n;
      kopf[6] = 1;            // Tonstrom
      kopf[7] = 1;            // ein Kanal
      new Int16Array(aus, 64).set(i16);
      this.onmessage?.({ data: aus });
    } catch (e) {
      console.warn('Opus-Ausgabe verworfen:', e);
    } finally {
      daten.close();
    }
  }

  /** Welchen Weg ICE genommen hat — fuer die Fusszeile, nicht fuer die Technik. */
  async _welcherWeg() {
    try {
      const bericht = await this._pc.getStats();
      let paar = null;
      bericht.forEach((s) => {
        if (s.type === 'candidate-pair' && s.state === 'succeeded' && s.nominated) { paar = s; }
      });
      if (!paar) { return ''; }
      let art = '';
      bericht.forEach((s) => {
        if (s.id === paar.localCandidateId) { art = s.candidateType; }
      });
      return { host: 'direkt', srflx: 'ueber STUN', prflx: 'ueber STUN', relay: 'ueber Relay' }[art] || art;
    } catch (e) { return ''; }
  }

  // ── Wie ein WebSocket ─────────────────────────────────────────────────────

  send(daten) {
    if (this.readyState !== 1) { throw new Error('die Leitung ist nicht offen'); }
    this._kanal.send(daten);
  }

  close() { this._zu(1000, 'geschlossen'); }

  _fehler(e) {
    console.warn('Fernzugriff:', e && e.message ? e.message : e);
    this.onerror?.(e);
    this._zu(1006, (e && e.message) || 'Fehler');
  }

  _zu(code, grund) {
    if (this.readyState === 3) { return; }
    clearTimeout(this._zweiterTimer);
    this.readyState = 3;            // CLOSED
    try { this._kanal?.close(); } catch (e) {}
    try { this._pc?.close(); } catch (e) {}
    try { this._ws?.close(); } catch (e) {}
    this.onclose?.({ code, reason: grund });
  }
}

/**
 * Eine Leitung oeffnen — im eigenen Netz ein WebSocket, von draussen der
 * Weg ueber den Rendezvous.
 *
 * Die Fernadresse sieht so aus:
 *   fern:wss://rv.example.com/#lwhu2kyjnrfdwvpxvdckao3lb7
 * also Dienst und Station-Id, durch `#` getrennt. Mehr steht nicht im
 * Kopplungsblatt, und mehr braucht es nicht: die Station ist unter ihrer
 * Id erreichbar, wo auch immer sie gerade steht.
 */
export function oeffneLeitung(url) {
  if (!url.startsWith('fern:')) { return new WebSocket(url); }
  const rest = url.slice('fern:'.length);
  const teiler = rest.lastIndexOf('#');
  if (teiler < 0) { throw new Error('der Fernadresse fehlt die Stations-Id nach #'); }
  return new FernLeitung(rest.slice(0, teiler), rest.slice(teiler + 1));
}
