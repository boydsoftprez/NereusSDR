# Longpath Handfunke

Die Fernbedienung fürs Telefon. Eine Seite, die Safari über
*Teilen → Zum Home-Bildschirm* zu einer App mit eigenem Symbol macht — ohne
App Store, ohne Xcode, ohne Apple-Entwicklerprogramm.

Sie spricht Longpaths eingebauten TCI-Server. Kein Bauwerkzeug, keine
Abhängigkeiten, nichts zu übersetzen.

## Warum kein natives Programm

Weil ein Browser eine Sendetaste nicht verantworten kann: iOS friert eine
Safari-Seite mitten im Senden ein, ohne `close` zu feuern — die Seite kann
dann nicht mehr zusagen, MOX zu lösen. Empfang, Bedienung und Ton laufen hier;
**Senden gehört in ein natives Programm**, das das Betriebssystem am Leben
hält. Deshalb sind **beide** Sendetasten gesperrt — auch der Abstimmträger.
Der heisst nicht „senden", legt aber einen Dauerträger mit voller Leistung
auf die Antenne. Bis zum 2026-09-30 war er hier bedienbar, während die
SENDEN-Taste daneben ausdrücklich tot war; das war ein Fehler und ist behoben.

Serverseitig ist der Fall abgesichert: der Sendezeit-Deckel
(`TciMaxTransmitSeconds`, Vorgabe 180 s) und der Wachhund des tastenden
Clients werfen einen hängenden Sender ab, ganz gleich welcher Client ihn
getastet hat — seit dem 2026-09-30 auch beim Abstimmträger. Vorher hing
beides allein am `trx:`-Weg, und ein vom Netz getasteter Träger blieb
stehen, wenn der Client verschwand. Diese Zusage stand hier also schon,
bevor sie stimmte; jetzt hält sie.

## Die Haltetasten (seit 2026-11-21, ab Werk AUS)

SENDEN und TUNE sind als **Haltetasten** gebaut (Martins Wahl vom 2026-10-04):
Drücken tastet (`trx:N,true` / `tune:N,true`), Loslassen löst. Die Logik steht
in `sendetaste.js`, geprüft von `pruefe-sendetaste.mjs`.

* **Zwei Riegel.** Im Quelltext `SENDEN_FREIGABE` (`app.js`) — steht auf
  `false`, bis Martin im Chat freigibt; dann ist die Seite so tot wie vorher.
  Dazu die Entsicherung auf der Seite (ENTSICHERN → SCHARF, verfällt nach 15 s).
* **Kein Sendezustand ohne Finger.** Es löst: Loslassen, `pointercancel`,
  Verlust des Zeigers, Seite im Hintergrund, `pagehide`, Fokusverlust,
  Ablehnung durch den Server und eine eigene Grenze von 60 s. Bricht die
  Leitung ab, löst der Server (Besitzer, Wachhund, Sendezeit-Deckel
  180 s) — diese Zusagen bleiben unverändert.
* SENDEN und TUNE schließen einander aus. Der Ton kommt vom Mac.
* Erste Sendung nur an der Dummy-Last.

## Aufbau

| Datei           | Aufgabe |
|-----------------|---------|
| `index.html`    | Die Seite. Enthält nur Gerüst, keine Logik. |
| `stil.css`      | Der Hausstil, 1:1 aus `StyleConstants.h` übernommen. |
| `tci.js`        | Der Draht: WebSocket, Textkanal, Binärrahmen, FFT. |
| `app.js`        | Anzeige und Bedienung. |
| `rx-worklet.js` | Empfangston im Audio-Faden. Hülle um `ton-kern.js`. |
| `ton-kern.js`   | Ringpuffer und Hochtastung. Wird von BEIDEN Tonwegen benutzt — dem Worklet und dem Ersatzweg (siehe unten), damit die Rechnung nicht zweimal dasteht und auseinanderläuft. |
| `attrappe.py`   | Prüfstand: ein TCI-Server aus Pappe, für Läufe ohne Funkgerät. |
| `tls-einrichten.sh` | Legt die Zertifikate an, mit denen die Seite ein sicherer Kontext wird. Ohne sie gibt es am Telefon weder Mikrofon noch AudioWorklet. |
| `kann-das-telefon.html` | Selbstauskunft eines Geräts: sicherer Kontext, AudioWorklet, WebCodecs, Bildschirmmaße. Meldet das Ergebnis an den Webserver, statt es abtippen zu lassen. |

### Zwei Tonwege

`AudioWorklet` ist **[SecureContext]** und fehlt auf einer LAN-Adresse über
`http` — also genau dort, wo das Telefon die Seite holt. Dann greift der
Ersatzweg über `ScriptProcessorNode`. Der läuft im Hauptfaden statt im
Tonfaden und kann unter Last rauh werden; stumm wäre erheblich schlimmer.

Zwei Fallen dabei, beide am echten Gerät gefunden:

* Ein ScriptProcessor **ohne Eingangskanal** wird von Safari nicht getaktet.
  Er bekommt deshalb einen Eingang und eine stille Quelle davor.
* iOS stuft Web-Audio als *Ambient* ein, und diese Kategorie gehorcht dem
  Stummschalter am Gerät — Kopfhörer ausgenommen. Ein stilles `<audio>`-
  Element bringt die Seite in die Kategorie *Playback*, die ihn ignoriert.

### Der sichere Kontext

Lange war das nur eine Fußnote beim Ton — bis sich herausstellte, dass
derselbe Schalter das Senden vom Telefon ganz verhindert. `getUserMedia`,
der einzige Weg an ein Mikrofon im Browser, ist ebenfalls
**[SecureContext]**. Über `http` ist `navigator.mediaDevices` schlicht
`undefined`; einen Ersatzweg wie beim ScriptProcessor gibt es nicht.

**Das Telefon hat es die ganze Zeit selbst gemeldet**, und niemand hat
hingesehen. In `~/Library/Logs/handfunke-server.log`, Zeile um Zeile:

```
172.30.30.115 … sicher=false … weg=scriptprocessor
127.0.0.1     … sicher=true
```

Am Mac ging alles, denn `127.0.0.1` gilt ohne Zertifikat als sicher. Die
eine Stelle, an der es zählt, ist die, an der niemand eine Konsole hat.

Abhilfe ist ein Zertifikat, und es behebt beides auf einmal — Mikrofon
**und** den Tonweg im eigenen Faden:

```bash
./handfunke/tls-einrichten.sh
```

Das Skript legt eine kleine eigene Zertifizierungsstelle an und darunter
ein Serverzertifikat. Zwei Stufen, damit am Telefon **einmal** etwas
einzurichten ist und nie wieder: das Serverzertifikat darf danach jederzeit
erneuert werden — neue Adresse, Ablauf — ohne dass dort jemand etwas
antippt. Die Schlüssel liegen außerhalb des Quellbaums, unter
`~/Longpath/werkzeug/handfunke-tls`.

Am Telefon, einmalig:

1. `http://<rechner>.local:8771/ca.crt` aufrufen und das Profil laden.
2. **Einstellungen > Allgemein > Info > Zertifikatsvertrauenseinstellungen**
   — „Longpath Handfunke CA" einschalten. Ohne diesen zweiten Schritt ist
   das Profil installiert und trotzdem wirkungslos; iOS sagt das nirgends.
3. Die Seite künftig über `https://<rechner>.local:8772/` aufrufen.
4. `kann-das-telefon.html` muss `isSecureContext = ja` zeigen.

`http` bleibt daneben bestehen — über `https` käme das Telefon ja gar nicht
erst an die Zertifizierungsstelle heran, der es vertrauen soll.

**Die Brücke muss mit.** Eine `https`-Seite darf kein `ws://` mehr öffnen;
der Browser sperrt das als gemischten Inhalt, und der Fehler sieht aus wie
„das Funkgerät antwortet nicht". Longpaths TCI-Server spricht kein TLS und
soll es auch nicht lernen müssen — WSJT-X, N1MM und JTDX sprechen keines.
Deshalb beendet `tci-bruecke.py` das TLS, **auf demselben Port wie bisher**:
welches Protokoll gesprochen wird, steht im ersten Byte (`0x16` für TLS,
`G` für `GET`). Der alte Weg über `ws://` bleibt dadurch unverändert offen.
Die Seite wählt nicht, sie folgt ihrem eigenen Schema.

> **Was die Brücke kostete — und seit 2026-10-07 nicht mehr.** Sie nimmt die
> Verbindung im WLAN an und baut eine **eigene** nach `127.0.0.1` auf. Für
> Longpath kam damit jeder Client der App aus Loopback — und Loopback ist
> dort das Vertrauen selbst: **keine der drei Freigaben griff**
> (`TciAllowRemoteTx`, `TciAllowRemoteRotor`, `TciAllowRemoteLog`), und das
> für jedes Gerät im Heimnetz. An der Station hängt eine Antenne.
>
> Jetzt trägt der Handschlag `X-Longpath-Weitergeleitet`, und Longpath
> behandelt solche Sitzungen bei Senden, Drehen und Loggen wie jede andere
> aus dem Netz.
>
> **Was das heißt, wenn du die Brücke benutzt:** Loggen geht weiter
> (`TciAllowRemoteLog` steht ab Werk auf True). **Den Rotor vom Telefon zu
> drehen braucht ab jetzt `TciAllowRemoteRotor=True`** — bewusst, denn am
> anderen Ende dreht sich ein Mast. Das Token verlangt die Brücke
> weiterhin nicht; wer auch das will, geht den Weg unter **Benutzen**.

### Die eine Regel

**Die Seite hält keinen eigenen Zustand.** Alles, was angezeigt wird, kommt
aus einer Zeile des Servers. Ein Knopfdruck schickt einen Befehl und wartet
auf die Gegenmeldung, statt die Anzeige vorher umzustellen. Das kostet einen
Wimpernschlag und erspart die Klasse von Fehlern, bei der das Telefon etwas
anderes behauptet als das Gerät tut.

Einzige Ausnahme, benannt im Code: die Schalter unter *Aufbereitung*, die der
Server nicht zurückmeldet.

## Benutzen

1. In Longpath: **Setup → CAT & Network → TCI Server** einschalten und
   *Bind interface* auf die **konkrete WLAN-Adresse** stellen — nicht auf
   Loopback, sonst kommt das Telefon nicht hin.
2. Dort unter *Remote access*: **Token** → `New` erzeugt einen
   Kopplungscode aus acht Zeichen (`ABCD-EFGH`). Beim Eintippen sind
   Bindestrich und Groß-/Kleinschreibung egal.
3. Ebendort **Allowed pages**: die Adresse eintragen, von der die Seite
   kommt, etwa `http://192.168.1.10:8767`. Ohne das weist der Server sie ab —
   WebSocket kennt keine Gleiche-Herkunft-Regel, deshalb prüft Longpath sie
   selbst (`TciServer::start`). Native Clients wie WSJT-X schicken keine
   Herkunft und sind davon nicht betroffen.
4. Am Telefon die Seite aufrufen, Adresse und Code eingeben,
   *Zum Home-Bildschirm*.
5. **Einmal auf `TON EIN` tippen.** iOS gibt Ton erst nach einer Berührung
   frei; der Knopf verschwindet, sobald er läuft.

### Bedienung

* **Tippen** im Spektrum oder Wasserfall springt auf die Stelle.
* **Wischen** stimmt ab. Dabei friert das Bild ein und der Zeiger wandert —
  so sieht man, wohin man fährt. Longpath führt die Bildmitte sonst der
  Abstimmung nach, und dann ändert sich sichtbar nur die Zahl.
* **Kneifen** vergrößert, von 192 bis 6 kHz. Der Ausschnitt wird am Server
  genommen, bevor verdichtet wird — nur so steigt die Auflösung wirklich
  (bei 6 kHz sind es 16 Hz je Bildpunkt statt 515).
* **Lange auf die Datenrate tippen** spielt einen Testton direkt an den
  Ausgang, am Lautstärkeregler vorbei. Trennt „das Gerät gibt nichts aus"
  von „unsere Daten taugen nicht".

### Was es kostet

Gemessen am echten Gerät (ANVELINA Pro 3, 12 kHz mono mu-law):

| | kB/s |
|---|---|
| Ton | 12,9 |
| Bild (373 Punkte) | 4,2 |
| Messwerte | 0,5 |
| **gesamt** | **17,6** — rund 63 MB je Stunde |

Roher I/Q wäre an derselben Stelle über 400 kB/s.

## Die blaue Fassung

Panadapter und Wasserfall tragen seit dem 2026-10-08 Longpaths eigene
Flächen, Wert für Wert (`app-bg` #08080a, Rampe wie `mutedStops` in
`SpectrumWidget.cpp`). Die blaue Fassung davor — für das Telefon im
Hellen gebaut — bleibt als eine Zeile in `app.js`:

```js
const WASSERFALL_BLAU = false;   // true = die blaue Fassung
```

Sie schaltet die Rampe und über `data-wasserfall` auch die Fläche im
Stilblatt. Blätter und Messung: `docs/design/2026-10-08-app-farben-wie-longpath/`.

## Ohne Funkgerät prüfen

```
python3 handfunke/attrappe.py         # lauscht auf 127.0.0.1:50099
python3 -m http.server 8766 -d handfunke
```

Die Attrappe liefert Init-Burst, ein Spektrum mit sechs Trägern (einer setzt
aus, damit der Wasserfall etwas zu erzählen hat), Empfangston und ein
wanderndes S-Meter. Sie ersetzt Longpath nicht — sie prüft die Naht.

## Was noch fehlt

* **Senden.** Beide Sendetasten sind tot, auch `TUNE` — der Abstimmträger
  heißt nicht so, legt aber einen Dauerträger auf die Antenne. Soll das
  Telefon je senden dürfen, braucht es eine eigene Zustandsmeldung vom
  Server und eine Sicherung gegen Fehltipp.
* **Ein Symbol für den Startbildschirm.** Entwürfe liegen unter
  `docs/design/2026-09-30-handfunke-symbol.html`.
* **Zwei Zuhörer am selben Empfänger** teilen sich seit 2026-09-30 den Ton
  richtig (eigener Vorrat je Sitzung), aber `spectrum_start` für Empfänger 2
  wird abgelehnt — es gibt nur einen Spektrum-Abgriff.
* **Opus** würde den Ton von 13 auf 4 kB/s drücken. Serverseitig wäre es zu
  haben (libopus hängt wegen RADE ohnehin an der Verknüpfungszeile), im
  Browser scheiterte es bisher daran, dass WebCodecs' `AudioDecoder`
  ebenfalls [SecureContext] ist — und eine WASM-Fremddatei für 9 kB/s zu
  teuer wäre. **Mit dem Zertifikat fällt dieser Grund weg.** Der dritte
  Posten, den derselbe Schalter aufhält; nachgesehen wurde er nie, weil er
  wie eine Eigenheit von WebCodecs aussah und nicht wie ein gemeinsamer
  Nenner. Geprüft am Gerät ist er noch nicht.
