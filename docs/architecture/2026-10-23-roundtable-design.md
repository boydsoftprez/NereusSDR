# Roundtable

**Date:** 2026-10-23 (America/New_York)
**Author:** J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
**Status:** design approved in conversation, decision by decision, 2026-10-07 to 2026-10-23. JJ called for this spec on 2026-10-23. Reviewed the same day (47 findings), revised, and re-reviewed (9 more, all addressed here). JJ answered the last open question (D25) the same day. Nothing is built.
**Base:** `origin/main` at `83a5a6bcc`. Every file:line below is on that tree.
**Applies to:** a new server program, the Core, the station link, the desktop window, the iPhone and iPad app, and the website.

Mockups this design was settled on: [reporter-brainstorm/](reporter-brainstorm/) (section 15).

---

## 1. Goal

An operator who talks on a frequency wants to know who is hearing them and how well. Roundtable answers that for every mode. Each frequency with people on it is a **room**. While someone talks, every station in the room shows its own raw S-meter reading of the talker, with distance and tuning offset, side by side, never averaged or ranked. People in the room can post short messages about it. When a turn ends it goes into history, and the talker's private archive keeps every session and every station that heard them.

It is the "who is hearing me" question that PSK Reporter, WSPRnet and the Reverse Beacon Network answer for digital modes and CW, asked for voice and every other mode, with the conversation alongside.

The name is Roundtable (JJ, 2026-10-19). It was "NereusSDR Reporter"; that clashed with FreeDV Reporter and PSK Reporter in Spot Hub.

## 2. Non-goals

- Not a band activity viewer or DX cluster. The band view exists to find rooms.
- No account system: no passwords, no email, no sign-up beyond a key (section 7.3). Verification only if abuse forces it (D23).
- No averaging, ranking or "best signal" across stations. Each number stands beside the callsign of the station that measured it.
- No lobby or general chat. A message belongs to a frequency, posted by someone tuned there.
- No automatic hiding of messages by report counts.
- The rendezvous and relay at rv.nereussdr.com do not change.

---

## 3. Decisions

All ruled by JJ. "Superseded" entries are kept so nobody re-asks them. Claude's own calls are not here; they are in section 18, open to veto.

| # | Decision | Ruled | Rejected or replaced |
| --- | --- | --- | --- |
| D1 | Public web page and an in-app view, both | 2026-10-07 | |
| D2 | Real callsign, everything public, one hide switch; reporting on by default | 2026-10-07 | opt-in per item |
| D3 | Raw S-meter as each radio reads it | 2026-10-07 | SNR-relative, calibrated dBm |
| D4 | Messages carry the sender and the sender's reading of the talker; light formatting | 2026-10-07 | |
| D5 | Desktop and iPhone both report and read | 2026-10-07 | |
| D6 | One row per radio (station), not per device | 2026-10-07 | |
| D7 | Readings show even when the talker isn't on NereusSDR; listeners name each turn separately, picking from names used on that frequency or typing one; the server never guesses | 2026-10-09 | Superseded: "publish readings only while a NereusSDR station is keyed" |
| D8 | Rooms are frequencies, grouped by band. A room is the stations close enough to hear each other for the mode; whoever was there first sets its center; each row shows its tuning offset | 2026-10-09 | |
| D9 | Only stations tuned to a frequency post or name turns there; anyone reads | 2026-10-09 | |
| D10 | A double shows both talkers; readings taken during it are kept out of both turns | 2026-10-09 | one wins |
| D11 | The server keeps everything; live is public, the archive is private to the station | 2026-10-07 | |
| D12 | Roundtable runs at the Core, beside FreeDV Reporter, PSK Reporter and the spot sources | 2026-10-20, with D13; confirmed by D20 | Superseded: "the window reports, the Core does not" |
| D13 | Desktop placement: callsign tags on the panadapter, plus a Roundtable panel in the right-hand column that follows the active slice. The panel pops out into its own window that reshapes with its size; at full width it is exactly the separate-window layout; closing the window docks it back | 2026-10-20 | separate window only; panadapter only |
| D14 | A Core with no window or phone connected keeps sending its readings, marked AWAY ("nobody at the radio"); an AWAY station can't post or name turns | 2026-10-20 | stop reporting |
| D15 | Signal history is kept for good; message text is removed after one year | 2026-10-20 | |
| D16 | Moderation: Mute for yourself, plus Report into a review list worked by JJ and helpers he names, who remove messages or block a station from posting; no automatic hiding by report counts | 2026-10-20 | |
| D17 | iPhone like the desktop: tags on the phone panadapter (a tap tunes), a Roundtable line on the VFO flag that opens a bottom sheet (half, tap the handle for full), PTT beside the message box while the sheet is open, a Roundtable page in Tools, no sixth tab | 2026-10-20 | |
| D18 | One spec, server included; server and protocol first in the build order | 2026-10-23 | separate server spec |
| D19 | The server runs on the website droplet | 2026-10-23 | its own droplet (cost); the rv droplet (live remote sessions). Superseded: the first sessions' "a second program on the same host as rv" |
| D20 | Windows and phones send messages, names and reports through their Core; one Roundtable connection per station | 2026-10-23 | each device direct |
| D21 | Hiding covers only what your own station sends. Others' readings of a hidden station still show as an unknown talker, and anyone may name that turn with its callsign. The first-run sheet says so | 2026-10-23 | a private "don't name me" list |
| D22 | Open to other software, with the API published on nereussdr.com | 2026-10-23 | NereusSDR Cores only |
| D23 | Other software gets a key from the website with a self-declared callsign, the same trust as a Core. Logins or verification only if abuse forces it | 2026-10-23 | Logbook of the World certificates |
| D24 | The public API can do everything a Core can: presence, readings, keyed state, messages, naming turns, reports | 2026-10-23 | readings and presence only |
| D25 | Until someone answers the first-run sheet, the Core connects read-only and sends nothing about the station; reporting starts when someone taps Go live on any device. A Core never answered stays read-only, even unattended. Auto-Start defaults on | 2026-10-23 | report at once with the sheet only informing |
| D26 | No admin handling of ordinary users: a lost key is a fresh start, and only the old key can move its own history. JJ manages the helper list and nothing else | 2026-10-23 | JJ moves history between station ids after his own check |
| D27 | The first-run sheet keeps the mockup's words on every platform (11.2 quotes them, from `ios/NereusApp/Roundtable/RoundtableFirstRunSheet.swift`); on the phone it opens at full height so both buttons show without a scroll | 2026-10-25 | this spec's earlier summary of the words; the phone sheet at half height with a scroll |

UX review fixes, all approved by JJ (full text in [reporter-brainstorm/ux-review.md](reporter-brainstorm/ux-review.md)). The details Claude added to each are calls in section 18.

| # | Fix | Ruled |
| --- | --- | --- |
| U1 | A list or link reads a frequency; only a panadapter tag (or Tune A here) tunes. Reading shows "Back to my slice". The radio's own band button still tunes | 2026-10-20 |
| U2 | Phone PTT is press and hold while the Roundtable sheet is open (dashed ring, "Hold"), 24 pt clear of the message box; tap-to-toggle with the sheet closed | 2026-10-20 |
| U3 | While you transmit, readings of you take the room (one TX line, earlier turns wait). Today's keyed meters stay untouched; on the phone the sheet sits below the keyed gauges; with a tuning dial on, the sheet stops above a strip with PTT left and the dial right | 2026-10-20 |
| U4 | Freshness: every reading shows its age and dims past 20 s, its bar stops and the age turns amber; a room dims once nothing in it is newer than 60 s; every Roundtable screen has one connection line (live, reconnecting, offline, connecting); offline greys the band tags, connecting hides them | 2026-10-20 |
| U5 | First-run notice "Roundtable is public", with the hide switch on it and Stay hidden / Go live | 2026-10-21 |
| U6 | Only a message that names you counts as unread (@1); other new messages show only inside the room; opening the room keeps the count, a pill jumps to the mention, it clears once on screen | 2026-10-21 |
| U7 | Shapes as well as colors: filled dot talking, ringed dot unknown, striped diamond doubled, hollow ring quiet | 2026-10-21 |
| U8 | You are cyan everywhere, never red or green | 2026-10-21 |
| U9 | Name a turn after it ends, on every screen; show every name given | 2026-10-21 |
| U10 | Crowded band: overlapping pan tags fold into one +N tag; a tap lists them and a row tunes; your slice's room and rooms naming you never fold; zooming in opens folds | 2026-10-22 |
| U11 | One scroll per panel, message box pinned, a pill for new messages | 2026-10-23 |
| U12 | 11 px (pt) minimum text and 4.5:1 contrast, as the program enforces | 2026-10-23 |
| U13 | Keyboard paths on the desktop and the public page | 2026-10-23 |
| U14 | The phone gets the offset column and the earlier turns list | 2026-10-23 |
| S1 | Phone swipes on the Panadapter tab only, user-mappable in Setup, General, Navigation; defaults left edge RX panel, right edge TX panel, up from the tab bar Roundtable on my frequency (section 11.3) | 2026-10-20 |
| S2 | iPad: left and right edges Off by default; swipe up from the tab bar widens the Roundtable applet (section 11.4) | 2026-10-20 |

---

## 4. Words used here

- **Station:** one radio's Core, or one station using other software with a website key. Identified by its Roundtable station id (section 7). A desktop, a phone and an iPad on one Core are one station (D6).
- **Room:** one frequency and mode family with stations on it (section 8.1).
- **Member:** a station whose slice has been in a room for the join dwell, or that keyed there (section 8.2).
- **Turn:** one continuous stretch of one talker, or of one double, in a room.
- **Talker states:** talking (a known station keyed), unknown (signal heard, no member keyed), doubled (two members keyed at once), quiet (nobody talking).
- **Reading:** one station's raw S-meter value of the room's signal, with its time.
- **Session:** in the archive, one stretch on the air on one frequency: a station's consecutive talking turns in one room with gaps under 10 minutes (C18).
- **AWAY:** a station reporting with nobody at the radio (D14).
- **Hidden:** a station that has switched Hide my station on (D2, D21).

---

## 5. What exists today

Facts from the scout pass on `83a5a6bcc`, cite-checked by the review.

- **Spot sources on the Core:** `SpotSourceHost` (`src/core/SpotSourceHost.h:89`) starts and stops FreeDV Reporter, PSK Reporter, cluster, RBN and POTA as station sources (`stationSources()`, `SpotSourceHost.cpp:243`; placements `SpotSourceHost.h:111-115`), mirrored read-only as the `spotSources` object. FreeDV Reporter's callsign comes from `FreeDvReporter/Callsign`, then `User/Callsign`, then the Core's `StationCallsign` (`SpotSourceHost.cpp:704-714`, "R-IOS-26: a callsign, never the Core's label"); its grid from `FreeDvReporter/GridSquare`, then `User/GridSquare` (`:716-719`).
- **S-meter per slice:** `SliceMeterPump` (`src/core/meters/SliceMeterPump.h:20-35`) polls at 10 Hz into `SliceModel::signalStrengthDbm` / `signalPeakDbm` / `signalAverageDbm` (`SliceModel.h:410-418`). Each reading carries `meterReadingNs` (`SliceModel.h:728`), on the steady clock, not wall time (`docs/architecture/2026-10-22-rx-meter-stamps.md:23-25`). While the station transmits the pump writes nothing (`rx-meter-stamps.md:37-40`), so `SliceModel` keeps the last value from before TX.
- **The app's S-meter scale:** S0 = -127 dBm, 6 dB per S unit, S9 = -73 dBm, top of scale -13 dBm (S9+60) (`src/core/ControlRanges.h:150-155`).
- **Keyed state:** `MoxController::moxStateChanged(bool)` (`src/core/MoxController.h:1498-1501`), which does not name the slice; mirrored as `txState` (`src/core/session/TransmitStateFacade.h:7-11`). The transmitting slice is in `connectedDevices` as `transmittingOn` (station link spec :3805).
- **Who is at the radio:** `StationServer::authenticatedSessionCount()` (`src/core/session/StationServer.h:1077-1079`) and the mirrored `connectedDevices` object with per-device state `listening` / `away` / `transmitting` (station link spec :3784-3812).
- **Station identity:** one P-256 key, `station-identity.pem` (`src/core/security/StationIdentity.h:11-13`), with `publicKeySpki()`, `sign()`, `verify()` (`:104-114`). It is loaded only by `StationServer` (`StationServer.cpp:2596`), from the directory its caller passes, falling back to `CertificateStore::defaultDirectory()` only when that is empty (`:2548`). On a desktop, Remote Access passes the settings profile directory (`src/gui/GuiDesktopStationRuntime.cpp:89`); `defaultDirectory()` is the reserved daemon profile (`CertificateStore.cpp:255-274`). A desktop running its own radio with Remote Access off does not load it today.
- **The rendezvous may not hold callsigns:** "It never learns a label, a callsign, a pairing code's words or a device's name" (`docs/architecture/2026-09-23-rendezvous-v1.md:48`). Hence a separate program with a separate id (sections 6, 7).
- **Server stack to copy:** plain Python asyncio with Ubuntu 24.04 apt `python3-websockets` and `python3-cryptography`, nothing from pip (`rendezvous/deploy/nereus-rendezvous.service:18-20`); a hardened systemd unit with `MemoryMax`, `DynamicUser`, loopback only; Caddy in front; conformance fixtures in `rendezvous/conformance/v1/` run by `python3 -m pytest rendezvous/tests -q` in CI job `rendezvous` (`.github/workflows/ci.yml:341-364`). The websockets server answers only GET handshakes.
- **Station link:** JSON in WebSocket text frames (`docs/architecture/2026-09-23-station-link-v1.md`). A new feature gets its own `<feature>Version` in `src/core/session/StationCapabilities.h` (link spec 6.3 at :1149), verb rows in `SessionCommandDispatcher.cpp`, and record streams (7.7 at :5724). The doc, `tests/data/link/v1/` and `surface.json` change together (section 17 at :8236-8296). Closest precedent: `docs/architecture/2026-10-07-remote-rotor-control-v1.md`.
- **Spot Hub:** tabs built at `src/gui/SpotHubDialog.cpp:407-416`; PSK Reporter at :414. iPhone: `ios/NereusApp/Spots/SpotHubPage.swift`; FreeDV Reporter in `ios/NereusApp/FreeDV/`.
- **Website:** static files in `website/public/`, rsync deploy by `website/deploy.sh` to `nereus-web:/var/www/nereussdr/`. `website/deploy/setup-server.sh` installs `/etc/caddy/Caddyfile` whole from `website/deploy/Caddyfile` (CSP `connect-src 'self' https://api.github.com` at :35). The website host has no Caddy OOM or memory drop-in (rv's `caddy-override.conf` is rv's only). The droplet has 1 GB of memory and may still host the netbench test server.
- **Grid and distance:** `src/core/GreatCircle` has Maidenhead to position and bearing, no distance. The only distance code is a freedv-gui port (`src/models/FreeDVStationModel.cpp:94-182`). No Python grid code exists.

---

## 6. Architecture

```
  NereusSDR window / iPhone / iPad
        |  station link (new capability stationRoundtableVersion 1)
        v
  Core (nereusd, or a desktop running its own radio)
    RoundtableClient  (beside FreeDVReporterClient, run by SpotSourceHost)
        |  Roundtable protocol v1, wss://roundtable.nereussdr.com/v1/station
        v
  Roundtable server (Python, website droplet, behind Caddy)  <-- other software (same protocol, website key)
        |  SQLite (one file)
        v
  roundtable.nereussdr.com: public page, read API, archive, review list, key page
```

- One Roundtable connection per station, always from the Core (D20). Windows and phones never talk to the Roundtable server; they use the station link to their Core.
- A desktop that runs its own radio (placement `Everything`) runs `RoundtableClient` in-process, as it runs FreeDV Reporter (section 10.1 covers its key).
- Browsers read the public page and its live stream directly. The archive and review list need a signed-in browser (section 7.4).
- Other software speaks the same protocol as a Core, signing in with a website key instead of a station key (D22 to D24).

### 6.1 The server program

- Name `nereus-roundtable`. Source in `roundtable/` at the repository root, laid out like `rendezvous/`: `server/nereus_roundtable/` with modules `__main__`, `config`, `clock` (a fake clock for tests, as the rendezvous has), `transport` (the websockets version differences, as the rendezvous isolates them), `identity`, `protocol`, `rooms`, `turns`, `store`, `live`, `http` (pages and form actions), `web`, `limits`, `grid`, `moderation`, `retention`; then `conformance/v1/`, `tests/`, `deploy/`, `deploy.sh`, `README.md`.
- Python 3 on Ubuntu 24.04 apt packages only: `python3-websockets`, `python3-cryptography`, the standard library's `sqlite3` and `asyncio`. Nothing from pip.
- One process, two loopback listeners: the WebSocket listener on port 8720 (`/v1/station`, `/v1/live`) and a small standard-library asyncio HTTP handler on port 8721 for pages, the read API, and form actions (key page, web sign-in, review actions, undo, Delete my archive). Caddy routes by path to each.
- Live rooms are held in memory; history is written to SQLite in WAL mode (section 6.3).
- The identity helpers (base64url, P-256 SPKI, raw `r||s` signatures) are copied from `rendezvous/server/nereus_rendezvous/identity.py`, with Roundtable's own prefixes (section 7). The conformance crypto fixtures prove the copies agree.
- Grid to position and distance is written fresh in `grid.py` from the Maidenhead definition (6 characters, square center) and the haversine formula, with no code from freedv-gui.

### 6.2 Hosting (D19)

- **Host:** the website droplet (`nereus-web`). The Roundtable site block lives in `roundtable/deploy/roundtable.caddy`. The website's `website/deploy/Caddyfile` gains one line, `import /etc/caddy/sites/*.caddy`, so the website's setup script, which installs its Caddyfile whole, keeps the Roundtable block on every run. h1/h2 only, as today.
- **DNS:** `roundtable` A and AAAA records at Namecheap, set by JJ (C10).
- **Memory:** `nereus-roundtable.service` gets `MemoryMax=256M`, `MemorySwapMax=0`, `OOMScoreAdjust=500` (C12). The plan also adds to nereus-web the Caddy drop-in rv already has (`Restart=on-failure`, `OOMScoreAdjust=-100`, a `GOMEMLIMIT`), which the website host lacks today. Under pressure the kernel stops Roundtable first, systemd restarts it, and Caddy and the website keep running. If measured use needs more than 256 MB, the droplet goes up one size step (JJ's call then) before the cap rises.
- **Isolation:** `DynamicUser`, state in `/var/lib/nereus-roundtable/`, `ProtectSystem=strict`, listening on loopback only and making no outbound connections, the same sandbox options as `nereus-rendezvous.service`.
- **Backups (C12):** a nightly timer writes a consistent snapshot with SQLite's online backup API to `/var/lib/nereus-roundtable/backup/roundtable-YYYY-MM-DD.db`, keeping 2. JJ's own machine pulls the newest each night over SSH (a pull, so the droplet holds no credentials for anything else) with `roundtable/deploy/pull-backup.sh`, which also keeps only 7. So anything removed (text past a year, a removed message, a deleted archive) is gone from every copy within 7 days. Snapshots stop, with a journal warning, while free disk is under 15%.
- **Disk watch:** a daily journal line gives the database size and free disk, and warns under 20% free (as rv's data-use timer warns). Readings are never deleted to make room; JJ decides then (resize or move).
- **Moving later:** everything is the database file plus the config file. Moving is: stop the service, copy both, run `setup-server.sh` on the new host, change the DNS record. Cores and apps reach it only by name, so no app update is needed. A home host needs a fixed address with TCP 443 forwarded, or a tunnel.
- **Every server step needs JJ's yes**, as with rv and the website.

### 6.3 Storage

SQLite, one file, WAL mode. Tables (shapes; the plan finishes the column lists):

- `stations`: station id, kind (`core` or `key`), current callsign, grid, first seen, last seen, posting block until (null, a time, or forever), not counted (null, a time, or forever; section 12.3).
- `keys`: key hash (SHA-256, never the key), station id, created, revoked. No address: the issuance count per address group is held in memory only (a restart resets it).
- `rooms`: opening id (a new one each time a room opens), room id (`<family>-<center Hz>`, reused when a room reopens on the same center), band, mode family, center Hz, opened, last active, closed. Turns and messages hang on the opening id; links and the read API use the room id (8.1).
- `turns`: turn id, opening id, kind (talking, unknown, doubled), talker station id and callsign at the time, second talker id and callsign (doubles), start, end.
- `turn_readings`: turn id, station id, callsign at the time, offset Hz, distance km, first and last second, and two packed tracks: readings at one byte a second (dBm + 160 in 1 dB steps, so -160 to +94 dBm; 255 = no reading that second), and signal present at one bit a second. About 135 bytes for a two-minute turn per station (C11).
- `turn_names`: turn id, naming station id, callsign at the time, name given, time.
- `messages`: message id, opening id, station id, callsign at the time, time, text (null once removed, deleted or past a year), the sender's reading of the talker at posting time, removed by (null or helper station id), deleted by sender.
- `reports`, `review_actions`, `helpers`: the review list and its log.
- `sessions`: built from talking turns for the archive (station id, room, start, end, turn ids).

Write rule: a turn's tracks are held in memory and flushed every 30 seconds and at the turn's end, so a restart loses at most 30 seconds of one open turn.

Growth: at the load the server is tested to (section 14, R-RT-9: 500 stations in 150 rooms), with every room busy 12 hours a day and a turn a minute, about 108,000 turns a day with 3 to 4 reading stations each: about 45 MB a day, 16 GB a year. The droplet holds the database plus two snapshots (6.2), so disk use is about three times the database. That load is a ceiling for testing, not a forecast; real use will start far lower. The disk watch (6.2) warns long before space runs out, and JJ decides then (resize, move, or thin old reading tracks by a later decision).

### 6.4 Retention (D15)

- A daily job at 04:00 UTC sets `text` to null on every message older than 365 days; screens show "Message removed after a year" in its place. The row, its sender, time and reading stay.
- Readings, turns, names and sessions are never deleted, except by Delete my archive (section 13, C9).
- Backups age out within 7 days (6.2), and the delete confirm says so.

---

## 7. Identity and keys

### 7.1 Stations on a NereusSDR Core

- The Core signs in with its existing station identity key. Its Roundtable station id is derived from that key with Roundtable's own prefix: `base32(SHA-256("NereusSDR roundtable id v1\n" || SPKI DER))[0..26]`, lowercase, the same shape as a rendezvous id but a different value. The two services never share an id, so their records can't be joined (rendezvous-v1.md:48). The archive still opens with the key the operator already holds (C20).
- Station ids are never in public data: the live stream, read API and public page carry callsigns and row refs, not station ids (12.1).
- **Row refs:** when a station joins a room opening, the server gives its row there a random 8-character ref, kept while that opening lives and different in every room and every opening, so refs never link a station across rooms. Two stations using one callsign in a room show as `W4XYZ` and `W4XYZ (2)`, in order of joining. Report on a station and Name refer to a row ref or a turn id; messages by message id. Mute is by base callsign, kept on the Core, and covers every station using that callsign. Helpers see station ids only on the review list (11.7), where Block and Stop counting act.
- Callsign: `Roundtable/Callsign`, then `User/Callsign`, then the Core's `StationCallsign`, as FreeDV Reporter (`SpotSourceHost.cpp:704-714`). Grid: `Roundtable/GridSquare`, then `User/GridSquare`. A Core whose callsign is empty does not connect; its Spot Hub tab says why.
- Callsigns are normalized to upper case. Prefixes and suffixes (`/P`, `/M`, `VE3/`) are kept as part of the callsign shown; mentions and "names used on this frequency" match on the base callsign (without them).

### 7.2 Sign-in proof

- On connect the server sends `challenge` with a 32-byte random nonce.
- A Core answers `hello` with its SPKI and a raw P-256 signature over `"NereusSDR roundtable hello v1\n" || host || "\n" || nonce`, where `host` is the server name the Core connected to (`roundtable.nereussdr.com`). The prefix differs from the rendezvous's, so a signature can't be replayed to the other service, and the host name stops another server passing the challenge through.
- The server derives the station id from the SPKI, verifies the signature, and accepts with `welcome`.

### 7.3 Stations using other software (D22, D23)

- The key page, `roundtable.nereussdr.com/key`, takes a callsign and a grid and issues a key: 32 random bytes, base64url, shown once. The server stores only its SHA-256. At most 3 keys a day per address group (C7).
- The station id for a key is `base32(SHA-256("NereusSDR roundtable key v1\n" || key))[0..26]`.
- The program answers `hello` with the key in place of the SPKI and signature, over TLS.
- One key is one station. Its callsign and grid are changed on the key page, signed in with the key; a callsign in `presence` from a key station is ignored.
- **A lost key:** a new key is a new station. History moves to a new key only by the old key, while still held: "Move my history to a new key" on the key page revokes the old key and moves everything in one step (C6, D26). Nobody moves history for someone else, and never by callsign. What moves: sessions, turns, readings, names and messages. A key lost outright means a fresh archive; the old one stays closed.
- The key page has no email, no password and no captcha. If abuse grows, verification is added then (D23); the stations table has room for a "verified" mark without changing the protocol.

### 7.4 Signing in on the web

The archive and the review list open in a browser, which holds no station key.

- **From the app:** the Archive button (desktop panel menu, phone Roundtable page, Spot Hub tab) asks the Core for a sign-in link (`roundtable.webSignIn`). The Core sends `weblink` on its station socket; the server answers `weblinkOk` with a one-time link that works once, within 5 minutes. The app opens it; the server swaps it for a session cookie (`Secure`, `HttpOnly`, `SameSite=Strict`, 30 days) and redirects, so the token leaves the address bar.
- **With a website key:** the key page signs the browser in with the key.
- **Hidden stations:** the Core opens a brief station connection that sends `hello` and `weblink` only, no presence, and closes (section 10.2).
- Every state-changing web action checks the `Origin` header and a per-session token (CSRF). The archive page has "Sign out everywhere", which ends every browser session of that station.
- The public page's "Your station key" button explains both ways in.

### 7.5 Helpers

- JJ's station id is set in the server config as the first helper. JJ adds and removes helpers on the review list page by station id, picked from a list that shows callsign and station id together; never by callsign alone. Every helper change and action is logged (section 11.7).

---

## 8. Rooms, turns and readings

### 8.1 Rooms (D8)

- A room is one mode family and one center frequency. A station's slice joins the room of its family whose center is nearest, if within the family's width (between two equally near, the lower center); otherwise it starts a new room centered on its own frequency.
- **The frequency a station sends** is the one an operator would quote on the air: the VFO frequency in NereusSDR (the suppressed carrier on SSB, the carrier on AM and FM, the signal on CW). The API page states this per mode for other software.
- **Families and widths (C1):** LSB; USB; DIGL; DIGU; AM and SAM (one); FM; CW (CWL and CWU one); FreeDV and RADE (one, by sideband: lower and upper separate). Widths: sideband, digital, FreeDV ±1.5 kHz; AM ±3 kHz; FM ±6 kHz; CW ±300 Hz. Opposite sidebands never share a room, since an LSB and a USB receiver on one dial frequency hear different signals.
- **Room ids** are stable strings, `<family>-<center Hz>` (for example `usb-14212000`). Links: `roundtable.nereussdr.com/r/<family>/<center Hz>`; the read API uses the same pair.
- The center stays fixed while the room lives; it never re-centers when its first station leaves.
- A room lives while it has members and closes 10 minutes after the last leaves (C18). On a server restart, rooms active in the last 10 minutes are reloaded from the database with their centers and ids, and stations rejoin them.
- A room link opens the live room if there is one, otherwise the most recent closed room with that id, shown quiet with its earlier turns. It never shows "not found" for a room that has existed.
- A room whose members are all AWAY and where nobody has talked for 2 hours is listed folded with the quiet rooms (C18).

### 8.2 Joining

- A station becomes a member once its slice has stayed in the room for 10 seconds, or at once when it keys there (C2). Until then its row shows "joining", it is not in the room's count, and it can't post or name.
- A station leaves when its slice moves out of the room, its connection closes, or nothing has come from it for 30 seconds (C18).
- One station, one row (D6). If two of its slices are in one room, the server uses the first listed in its `presence` (the Core lists the active slice first) and ignores readings from the other.

### 8.3 What a station sends

- **Presence**, on change (coalesced to at most one a second, so tuning never trips a limit) and every 10 s: per slice (up to 4), a slice id, frequency, mode, filter width and keyed; once, its grid, callsign (Cores only) and whether anyone is at the radio (D14).
- **Readings**, once a second per slice that is a member and not keyed. A Core sends the slice's `signalStrengthDbm`, the value the operator's own meter shows (D3), and only when `meterReadingNs` has advanced since the last one sent. It never sends while that slice is keyed (the stored value is the last from before TX). The wire time is Unix epoch milliseconds, converted by the Core from its wall clock at the moment of reading. Other software sends what its meter reads, in dBm.
- **Signal present**, with each reading (C3): true when the reading is at least 10 dB above the station's own floor for that slice. The floor is the 10th percentile of the last 60 seconds of readings taken while signal present was false; while it is true, the floor holds. After tuning to a new frequency, signal present stays false for the first 10 seconds while a floor forms. The station decides, because only it knows its own noise.
- **What the server refuses**, each with `error` and a plain reason, never closing the socket (protocol errors close it): a reading for a slice not in the station's presence; a reading on a slice whose presence says keyed; readings from a station that is joining (dropped silently, not refused); a `post` or `name` from a non-member or an AWAY station. Readings over the limit are dropped, with one `error rate` a minute at most; other kinds over the limit are refused.

### 8.4 Turns (D7, D10)

The server works out turns per room from presence and readings, on its own clock:

- **Talking:** a member is keyed. A turn opens credited to that station. If an unknown turn is open, it ends and the talking turn starts.
- **Doubled:** a second member keys while one is keyed. The talking turn ends and a doubled turn starts, showing both callsigns. When one unkeys, the doubled turn ends and a talking turn for the other starts. Readings received during a doubled turn are stored against it but never shown as either talker's reading and never enter either talker's archive (D10).
- **Unknown:** no member keyed and at least one member reports signal present. A turn opens with no talker. Listeners may name it (8.6).
- **Quiet:** no member keyed and no member reporting signal present for 3 seconds (C4). The open turn ends.
- A known talker unkeying ends their turn. Screens show it as ending; if the same talker keys again within 3 seconds the turn continues (VOX and short pauses), otherwise it is final.
- **Long keys:** a slice reported keyed for 10 unbroken minutes is treated as unkeyed until it reports unkeyed and keys again, and the server logs it (C21). This stops a stuck or false keyed state holding a room.
- **Digital families (DIGL, DIGU):** many stations transmit at once at different audio offsets, so there are no doubles. Each keyed member's transmission is its own turn; readings received while two or more members are keyed are kept out of all of them. With no member keyed, unknown turns work as in every other family, so readings of stations not on NereusSDR still show (D7). An S-meter reads the whole passband, so in a busy digital room an unknown turn may hold several stations; the public page's notes say so (C19).
- Two unknown stations doubling can't be told apart on S-meters; it shows as one unknown turn. The public page's notes say so.

### 8.5 What each reading shows

- The station's callsign; its distance to the talker (from both grids, computed by the server; blank when either grid is missing or the talker is unknown); its offset from the room center in Hz (its frequency minus the center, rounded to 10 Hz); its reading as an S-unit on the app's scale (section 5: S9 = -73 dBm, 6 dB per S unit, then S9+dB), converted by each screen from dBm; its age; AWAY when it is.
- Readings are listed nearest first; stations with no distance follow in callsign order. Never by strength (D3).

### 8.6 Naming turns (D7, D21, U9)

- A station may name an unknown turn while it is a member of that room (D9), during the turn or within 24 hours after it ends, if it was also a member at some time during the turn (C5). A room that closed and reopened on the same center counts as the same room for this. An AWAY or blocked station can't. Names are picked from those used on that frequency in the last 24 hours, or typed.
- Every name given is kept and shown with its count: "named W4XYZ by 3"; when names differ, each with its count. No winner. A station has one name per turn and may change it.
- A talking or doubled turn can't be named.
- Hiding does not stop others naming a turn with a hidden station's callsign (D21).
- Names never move a turn into any station's archive. An unknown turn belongs to no archive, so a false name can't fill someone's history (C5).
- "Report a wrong name" on a turn opens Report with that reason (12.2).

---

## 9. Roundtable protocol v1

The protocol between a station (a Core or other software) and the server. It is public (D22) and published on the website (9.7). The plan's first task writes it out as the numbered wire spec `docs/architecture/2026-10-23-roundtable-v1.md`, in the style of `2026-09-23-rendezvous-v1.md` (stable section numbers, the document is the authority, conformance fixtures), from this section.

### 9.1 Transport

- `wss://roundtable.nereussdr.com/v1/station` for stations; `wss://roundtable.nereussdr.com/v1/live` for anyone, read-only; HTTPS `GET /v1/api/...` for read-only queries.
- WebSocket text frames, one compact JSON object each, with a string `type`. Envelope rules copied from the rendezvous (its section 5.1): unknown keys are ignored, duplicate keys are a protocol error, unknown kinds from the server are ignored by clients; the server closes the connection on an unknown kind from a client.
- Heartbeat: the server pings every 20 s; a client that has heard nothing for 45 s reconnects, and the server closes a connection silent for 45 s.

### 9.2 Station to server

| Kind | Purpose |
| --- | --- |
| `hello` | Sign in: protocol version, SPKI and signature, or key; software name and version |
| `presence` | Slices (id, frequency, mode, filter width, keyed), grid, callsign (Cores), at the radio |
| `reading` | Slice id, dBm, Unix ms, signal present |
| `post` | A message to a room the station is a member of: text, client message id |
| `delete` | Delete one of the station's own messages |
| `name` | Name an unknown turn |
| `report` | Report a message, a turn's name, or a station's readings and keyed state; optional reason |
| `read` / `unread` | Start and stop reading another room (U1); at most 4 at once |
| `resync` | Ask again for a full `room` or `rooms` after a gap |
| `mentionsSeen` | Mentions read up to a time |
| `weblink` | Ask for a one-time web sign-in link (7.4) |
| `deleteArchive` | Delete my archive (section 13), with a confirm field |
| `hidden` | The last thing sent on going hidden; the server ends the station's presence at once |

### 9.3 Server to station

| Kind | Purpose |
| --- | --- |
| `challenge` | The sign-in nonce |
| `welcome` | Accepted: station id, limits in force, server time, unread mentions since the last `mentionsSeen` |
| `rooms` | The band list, full, with a generation number: every live room with id, state, talker, member count, message count, last activity |
| `roomsDelta` | Changes to the band list (add, change, remove per room), generation plus one; at most every 2 s |
| `room` | Full view of one room the station is in or reading, with a generation: center, family, members and their readings, the current turn, earlier turns (last 30 minutes), messages (last 50) |
| `delta` | A change to one room (member in or out, reading, turn change, message, name), generation plus one |
| `posted` | Result for a `post`: accepted with message id, or refused with a plain reason |
| `mention` | A message naming the station's callsign |
| `notice` | Plain operator text: blocked, not counted, report looked at, version ending |
| `weblinkOk` | The one-time link |
| `error` | A refused request, with a code and plain reason |

On a gap in generations a client sends `resync` and gets a full `rooms` or `room`. Older messages in a room come from `GET /v1/api/room/<family>/<center Hz>/messages?before=<id>`.

### 9.4 Live (read-only)

- `/v1/live` takes `watchBand` (a band or all), `watch` / `unwatch` (a room, at most 4 at once) and `resync`, and sends `rooms`, `roomsDelta`, `room` and `delta` as above. No sign-in.
- `/v1/api/rooms`, `/v1/api/room/<family>/<center Hz>`, `/v1/api/room/<family>/<center Hz>/messages`, `/v1/api/callsign/<callsign>` (live rows using that callsign: one flat entry per room row with its callsign, room and row ref, never grouped by station, so refs are not linked across rooms) answer JSON.
- Archives are never in the read API (D11).

### 9.5 Limits (C7)

Per station unless stated; sent in `welcome`; refused or dropped as in 8.3.

| What | Limit |
| --- | --- |
| Readings | 2 a second per slice |
| Presence | 1 a second |
| Messages | 6 a minute, 60 an hour |
| Message length | 500 Unicode code points and 10 lines |
| Names | 10 a minute |
| Reports | 10 an hour |
| Slices in rooms | 4 |
| Station connections | 1 per station id (a new one replaces the old); 8 per address group |
| Keys issued | 3 a day per address group |
| Read API | 60 requests a minute per address group |
| Live sockets | 4 per address group; 500 in all (what R-RT-9 proves) |

An address group is one IPv4 address or one IPv6 /64. The server learns the client address from `X-Forwarded-For`, trusted only from loopback, which the Roundtable Caddy block sets. Addresses stay in memory only (and the key issuance count for 24 hours), never in a log.

### 9.6 Versions

- The protocol is `v1`. A later version adds new paths (`/v2/...`); `v1` keeps working for at least six months after its successor ships (C8). The dates are on the API page, and a `notice` tells stations still on the older version.

### 9.7 The API page

- `nereussdr.com/roundtable/api.html`, generated from the wire spec's Markdown by `docs/manual/build_preview.py`, as the manual is (C10).
- The conformance fixtures in `roundtable/conformance/v1/` (crypto, messages, refusals, scripted sessions, turn scripts) are published with it so other software can test against them.

---

## 10. The Core

### 10.1 RoundtableClient

- `src/core/RoundtableClient.{h,cpp}`, a sibling of `FreeDVReporterClient`, using `QWebSocket`, owned by `RadioModel`, and added to `SpotSourceHost::stationSources()` so it appears in the `spotSources` object and starts and stops with the existing `spots.connect` path (Start and Auto-Start, as FreeDV Reporter). Auto-Start defaults on (D2); nothing is sent until the first-run answer (D25).
- It reads slice frequency, mode, filter and `signalStrengthDbm` with `meterReadingNs` from `SliceModel`, keyed state from `MoxController::moxStateChanged` together with the transmitting slice (`transmittingOn` in `connectedDevices`, or the local TX slice on a desktop running its own radio), and who is at the radio from `StationServer` (section 5).
- "At the radio" is true when any authenticated device session is `listening` or `transmitting`, or when this is a desktop running its own radio. The away choice (C15) decides whether an unattended Core keeps reporting (default) or stops.
- **The key:** on a Core it uses the `StationIdentity` that `StationServer` loads. On a desktop running its own radio with Remote Access off, `RoundtableClient` calls `StationIdentity::loadOrCreate` on the directory Remote Access would use, the settings profile directory that `GuiDesktopStationRuntime` passes as `securityDirectory` (`GuiDesktopStationRuntime.cpp:89`), never `defaultDirectory()`, so the station keeps one id when Remote Access is turned on later.
- It keeps on the Core, in AppSettings under `Roundtable/` (PascalCase keys, booleans "True"/"False"): the first-run answer, Hide my station, the away choice, highlight mentions, the mute list, mention read state, the callsign and grid overrides. Every device on the station sees the same (U5, U6).
- It reconnects with backoff (1, 2, 5, 10, 30 s, then every 30 s) and tells devices its link state for the connection line (U4).
- Mentions: it takes the server's `mention` and `welcome` lists, and also matches its base callsign in messages of rooms it shows, so a hidden station (no station connection) still gets @N.

### 10.2 Hidden (D2, D21)

- Hidden means nothing about the station's radio leaves the Core: no presence, readings, keyed state, posts or names. The only exception is the brief sign-in connection below, which carries the sign-in and the software name and is never shown publicly. On going hidden, `RoundtableClient` sends `hidden` as its last message, closes the station connection, and opens a read-only `/v1/live` connection, so the operator still sees every room. A Core that starts hidden connects only to `/v1/live`.
- The station's own reading shows on its own screens marked NOT SENT. Posting and naming are off, with "You are hidden" on the connection line.
- For the archive and Delete my archive while hidden, the Core opens a brief station connection that sends `hello` then `weblink` or `deleteArchive` only, no presence (7.4).
- Turns and readings already public stay in history; hiding stops what is sent from then on.
- Going live again reconnects as a station.

### 10.3 Station link

- New capability `stationRoundtableVersion` 1 in `StationCapabilities.h`, appended after the existing ones.
- Mirrored object `roundtable`: link state (connecting, live, reconnecting since, offline since, hidden), first-run answered, mention counts per room id, each slice's room id.
- Record streams: `roundtableRooms` (the band list; capacity 500; record kind `room`) and `roundtableRoom:<room id>` (one room; capacity 400; record kinds `member`, `reading`, `turn`, `name`, `message`), subscribed with the existing `records.subscribe` verb. Subscribing to a room's stream only delivers what the Core already receives; `roundtable.read` is what makes the Core ask the server for another room.
- Verbs: `roundtable.post`, `roundtable.delete`, `roundtable.name`, `roundtable.report`, `roundtable.read`, `roundtable.unread`, `roundtable.setHidden`, `roundtable.answerFirstRun`, `roundtable.mute`, `roundtable.unmute`, `roundtable.markMentionRead`, `roundtable.webSignIn` (returns the one-time link), `roundtable.deleteArchive`.
- Settings through the station link's settings write path (`StationServer::applySettingsWrite`, then `SettingsProxyServer::applyInboundWrite`), with a `Roundtable` Station prefix added to `SettingsScope.cpp` (the per-computer preferences stay OperatorLocal exceptions there). The Core keys: `Roundtable/AutoStart`, `Roundtable/AwayChoice`, `Roundtable/HighlightMentions`, `Roundtable/Callsign`, `Roundtable/GridSquare`.
- The link spec, `tests/data/link/v1/` and `surface.json` change together, per link spec section 17. Operator wording passes `OperatorWording::isPlain`.

---

## 11. Screens

Every screen uses the 2026.10.1 workspace look (desktop and web) or the app's workspace look (iPhone and iPad, PR 603, merged 2026-10-23). Roundtable's states sit on the workspace roles: talking green `#4caf6a`, unknown amber `#d8a55f`, doubled red `#c25a5c`, you cyan `#00d4ff` (U8), shapes as in U7. Text is never under 11 px (pt) and holds 4.5:1 against its ground, except disabled controls (U12).

### 11.1 Rules on every screen

- A list or link reads a room; only a panadapter tag or Tune A here tunes (U1). Reading another room shows a bar: "Reading 7.249 · Tune A here · Back to my slice". Tuning anywhere returns to following the slice.
- One connection line under each room header: "Live · last reading 1 s ago", "Reconnecting · nothing new for 12 s", "Offline since 21:04 UTC", "Connecting", or "You are hidden" (U4). It is Roundtable's link from the Core; the toolbar's radio pill is unchanged. While not live, Send is off and the draft stays; nothing is queued. Offline greys the band tags as last known; connecting hides them.
- Each reading shows its age; past 20 s the row turns grey (about 5:1, not faded), its bar stops and the age turns amber. A room turns grey in lists once nothing in it is newer than 60 s (U4).
- Only a message naming your callsign counts as unread: @1, @2 on the flag line and in room lists, as a cyan badge. Opening the room does not clear it; a pill above the box ("@1 A message names you ↓") jumps to it; it clears once that message has been on screen for 2 seconds; the message keeps its cyan mark. Read on one device is read on all (U6). Other new messages show only inside the room ("4, 2 new").
- Each room panel is one scroll with the message box pinned. While you read further up, arrivals wait below and an "N new" pill above the box counts them; a mention pill comes first. At the newest message the view follows. A draft stays while messages arrive. New messages carry a blue edge and "new" until on screen for 2 seconds (U11).
- While you transmit, readings of you take the room: one TX line ("You · 9 hear you"), then every station's reading of you; earlier turns and messages wait below (U3).
- An empty room: "Nobody else here yet. Your readings of the next talker will appear here."
- Messages: 500 characters (Unicode code points), with a counter from 400; a failed send keeps the text with "Not sent · Try again"; ⋯ on your own message offers Delete ("Deleted by sender" shows in its place); ⋯ on others' offers Mute, Report, Copy. A draft belongs to its room and is still there when you come back.
- Formatting: bold, italic, and links (http and https only, shown with their full address). Text is cleaned when stored and again when shown (12.4).
- AWAY stations show AWAY on readings, members and the room header, with "can't post or name turns".
- Mute folds that station's messages away for you only; nobody is told. Report and Mute are also on a reading row.

### 11.2 Desktop (D13)

- **Panadapter tags:** one per room along the foot of the spectrum, clear of the VFO flags: the state glyph, the talker's callsign (or Unknown, Doubled, Quiet) and the member count. A click tunes the active slice there. Crowded tags fold into +N (U10): a fold holds two or more; a lone neighbor moves aside; folds widen only if tags still don't fit; your slice's room and rooms that name you never fold; zooming in opens folds, no separate zoom setting. A click on +N lists the rooms inside by frequency; a row tunes.
- **VFO flag line:** "Roundtable · 9 · K4ABC" with the state glyph, @N when a message names you, "9 hear you" while keyed, the link state when not live. A click opens the panel.
- **Panel:** a right-column applet (`RoundtableApplet`, in `AppletPanelWidget` with the gradient title bar and grip), following the active slice. Order: room header and connection line, talker, readings (Station, km, Offset, Reading, Age), earlier turns (last 30 minutes, Name on unknown turns), messages, box. Menu: Archive, Settings, Pop out.
- **Pop-out:** the arrow in the panel header opens it in its own window (new: applets can't pop out today). Under 600 px wide one column; 600 to 980 two columns (room list and room with messages); wider, three columns (room list 250 px, room, messages 320 px), exactly the separate-window layout; Wide preset 1080 x 600. It remembers its size and screen, as floated pans do. Closing it docks the panel back.
- **Keyboard (U13):**
  - Docked panel: Tab moves into the panel; up and down move through earlier turns and messages; Enter on an unknown turn opens Name; Escape, when reading another room, goes back to your slice, otherwise leaves the panel.
  - Pop-out list: / searches; up and down move through rooms (each reads as you land); Enter goes into the room, whose earlier turns are a list the arrows walk; Escape steps back: room to list, then clears the search, then back to your slice.
- **Spot Hub tab:** "Roundtable", after PSK Reporter (`SpotHubDialog.cpp:414`), laid out like the FreeDV tab.
  - On the Core (every device sees the same): the server group (Auto-Start, Start, connection state); Hide my station, with the preview of how others see you; the away choice; highlight messages that name me; the callsign and grid overrides; the mute list; Archive; Delete my archive (behind a confirm saying it can't be undone and that backups age out within 7 days).
  - This computer only: how many earlier turns to show, and whether the panel opens on the readings or the messages.
- **First run:** the first time the station opens Roundtable, a sheet "Roundtable is public", in the mockup's words (D27): "Roundtable shows who hears whom, live, to everyone on it and on its web page. With your station on, they see:", then "Your callsign CALL and grid GRID"; "Your radio's reading of each talker, and how far you are from them"; "Every station's reading of you while you talk"; "Messages you post on your frequency"; then "Your archive of who heard you stays private to you."; Hide my station ("You still see everything. Your station and your readings are not sent. Others can still hear you, and may name you when you talk.", D21); Stay hidden and Go live, the switch moving the main button between them; and at its foot "Asked once for this station, on any of its devices. Change it any time in Roundtable settings." Asked once per station; another device that opens Roundtable afterwards shows a one-time note instead.

### 11.3 iPhone (D17)

- Tags on the phone panadapter (a tap tunes), folding as on the desktop.
- The VFO flag's Roundtable line opens a bottom sheet at half height; a tap on the handle makes it full height. Half: talker and readings with the Offset column (U14). Full: readings, earlier turns with Name (U14), messages, box, in one scroll.
- PTT with the sheet open: smaller, beside the message box, 24 pt clear, press and hold, dashed ring, "Hold" (U2). Sheet closed: tap to toggle, as today.
- Keyed (U3): today's keyed gauges on the waterfall stay; the sheet drops below them and shows the TX line and readings of you. With a tuning dial on (knob or thumbwheel), the sheet stops above a 92 pt strip with PTT left and the dial right (knob drawn at 76 pt); keyed with a dial, the message box steps aside and the draft is kept.
- Tools gets a Roundtable page after FreeDV Reporter: band chips with counts, one row per room (state, who, members, messages, @N), your slice's room marked; a row reads (U1).
- Swipes (S1): Panadapter tab only; mapped in Setup, General, Navigation, beside the tuning dial, saved on the phone, with Reset to defaults (lit once one is changed, dimmed until then). Choices: Off, RX panel, TX panel, Roundtable (my frequency), Roundtable (all frequencies), Spot Hub, Tools. 18 pt edge zones and the tab bar; a drag that starts anywhere else moves the band. Off gives the edge back to dragging the band. Remapping changes the swipe only; the toolbar buttons stay RX left and TX right. Pages slide from the swipe's side; swipe back or tap the band to close; a short drag springs back; nothing while a sheet or panel is up; the TX panel by swipe never keys.
- First run: the same sheet as the desktop, in the same words. It opens the Roundtable sheet at full height, so Stay hidden and Go live show without a scroll (D27).
- Source: `ios/NereusApp/Roundtable/` (sheet, Tools page, first-run sheet, flag line) and the link client in `ios/NereusKit`.

### 11.4 iPad

- A Roundtable applet in the applet column, second after the S-Meter by default (the grip reorders it): room header, connection line, talker, readings, earlier turns, messages, box. The column is one scroll with the box pinned to its foot while the applet is in view (U11).
- The VFO flag's Roundtable line scrolls the column to the applet.
- Swipe up from the tab bar, or ⇔ Widen, widens the column to the room list plus the room; the same swipe or ⇔ Narrow puts it back (S2).
- Edge swipes default Off (the RX and TX panels already sit in the column); the same Setup choices turn them on, saved per device (S2).
- With the column hidden, the iPad uses the phone's sheet. The first-run sheet is a centered form sheet.

### 11.5 Public page (D1)

`roundtable.nereussdr.com`: static HTML and script served from the same site, reading `/v1/live` (same origin, so the website's CSP is not touched).

- Dense 30 px rows grouped by band; the open room beside the list.
- Band buttons with room counts and a small talking/unknown bar; picking a band shows the band map (each room at its place, stem height = members).
- Quiet rooms fold under each band with a count.
- Search across talker, readers and posters; typing a frequency jumps to it.
- Each row: frequency, mode, state and talker, time, members, a strip of one bar per station's reading nearest first, the last message, age.
- Sort: Frequency (default), Most stations, Newest activity.
- The room pane: header, "updated N s ago", talker, readings, earlier turns with names, messages; read only, with "To post or name a turn, tune this frequency in NereusSDR or other Roundtable software".
- Copy link per room (8.1).
- Keyboard (U13): /, arrows, Enter, Escape steps back (room, then search, then band).
- Notes include that two unknown stations doubling show as one unknown turn.
- The NereusSDR brand mark, as on the website.

### 11.6 Archive

Opened from the panel menu, the phone's Roundtable page, the Spot Hub tab, or the public page after signing in (7.4).

- One row per session: band, frequency, start, length, how many stations reported you.
- A session opens to one row per station that reported you: callsign, distance, WATCHED (how long it kept reporting you, from its signal-present track, not how long you talked), and its own raw readings on a timeline. Amber hatching marks time you were keyed and that station had lost you; plain gaps are time you were not keyed. A row opens that station's readings over the session.
- Timelines show the 12 stations that heard you longest, then "Show all 34" (C17).
- Empty sessions are kept: "Nobody reported you." Neutral wording, never "failed".
- An hourly histogram of how many stations got you.
- AWAY rows marked. Readings from doubled turns are not here (D10).
- Message text past a year shows "Message removed after a year"; the session and readings stay.

### 11.7 Review list

On the public site behind web sign-in, helpers only (7.5).

- Reports oldest first, with a count of those older than a day. Each shows the message, turn or station reported, the reason if given, the room, and how many reported it.
- Actions on a message: Remove message (text off every screen and the archive; the room shows "Message removed").
- Actions on a station: Block from posting, for a set time or for good (stops messages and naming); Stop counting this station, for a set time or for good (its readings, presence and keyed state stop showing live and stop crediting turns; its stored rows are kept and marked, so the signal record is never edited; C21).
- Leave it closes the report with no change. The reporter is told only that it was looked at.
- If another helper acted while you were looking, the action is refused and the page shows what they did.
- Undo for 10 minutes after an action, stated beside the button.
- The helper log shows every action and every helper change, by whom.

---

## 12. Privacy, safety and moderation

### 12.1 What is public

Callsign and grid; presence (each room you are in, your offset, AWAY); each station's readings of each talker, with distance; turns and their names; messages; the software name and version from `hello`, shown only on the rows of a station that is present in a room. Never public: station ids (rows carry callsigns and row refs, 7.1), addresses, the archive, the mute list.

### 12.2 Reports

- On a message: Spam, Abuse, Off topic, Other, or no reason (C16).
- On a turn: Wrong name.
- On a station (from a reading row): False readings or keyed state.
- The reported station is not told who reported. Reports never hide anything by count (D16).

### 12.3 Abuse controls

- Rate and address limits (9.5) for everyone.
- The server's own checks (8.3, 8.4): readings only from members, for slices in presence, never from a keyed slice; 10-minute long-key limit.
- Helpers: Block from posting; Stop counting this station (11.7).
- Revoking a website key ends its connection and refuses the key.
- The server log holds no message text, no addresses and no full ids: at most the first 6 characters of a station id, as the rendezvous logs (rendezvous spec 9.4). Caddy keeps no access log for the site, as for rv and the website.

### 12.4 Text safety

- On write: Unicode NFC; control characters other than newline removed; 500 code points and 10 lines.
- On display, everywhere: formatting from a fixed set (bold, italic, http and https links shown with their address); everything else plain text. The web pages set a strict CSP (`default-src 'self'`, no inline script) and escape all text.

---

## 13. Error handling

- **Server unreachable:** the Core retries with backoff; devices show Reconnecting then Offline (U4); band tags grey out as last known; nothing is queued.
- **Server restart:** rooms active in the last 10 minutes reload with their ids and centers (8.1); open turns end at the restart and new ones open as readings return; stations resync.
- **A gap in generations:** the client sends `resync` (9.3).
- **Clock:** turns use server time. A reading's Unix ms is used only for its age; a reading stamped more than 5 s from server time is taken at its arrival time.
- **Post refused** (rate, blocked, not a member, AWAY, not live): the text stays, with the plain reason under the box.
- **Callsign change on a Core:** the station id is unchanged; new turns, readings and messages carry the new callsign; old ones keep the callsign stored with them (6.3).
- **Database failing or full:** live rooms keep running from memory; posts are refused with "Roundtable can't save messages right now"; the journal logs an error.
- **Memory cap reached:** systemd restarts the service; stations reconnect and resync (6.2).
- **Delete my archive (C9):** removes the station's sessions, its talking turns and the readings others made of them, and clears the text of its messages. Its readings of other talkers stay in their archives, under the callsign stored with them. The confirm says it can't be undone and that backups age out within 7 days.

---

## 14. Requirements and verification

New requirement family `R-RT`. Evidence: Software (unit, conformance, offscreen captures), Integration (Core and server together, headless browser, load), Device (JJ's radios; pending until run on the bench).

| ID | Requirement | Evidence |
| --- | --- | --- |
| R-RT-1 | The wire spec `2026-10-23-roundtable-v1.md` exists and its conformance fixtures pass in the server's runner and the Core's | Software |
| R-RT-2 | Sign-in: a Core's signature over the Roundtable prefix and host is accepted; a rendezvous-prefixed signature, a signature naming another host, and a revoked key are refused; the Roundtable id differs from the rendezvous id for the same key | Software |
| R-RT-3 | Rooms form by family, nearest center and width (C1); opposite sidebands never share; ids survive a restart; links resolve to the live or last room | Software |
| R-RT-4 | Joining: member after 10 s or at once on keying; "joining" before; one row per station with two slices in a room | Software |
| R-RT-5 | Turns follow 8.4 on scripted presence and readings: talking, unknown to talking, doubled in and out, quiet after 3 s, same-talker rejoin under 3 s, the 10-minute long key, digital families | Software |
| R-RT-6 | Readings: a Core sends `signalStrengthDbm` once a second only when the stamp advanced and the slice is unkeyed; Unix ms on the wire; signal present per C3 including the held floor and the 10 s warm-up | Software |
| R-RT-7 | The server refuses or drops input as in 8.3, with plain reasons, and enforces 9.5 including address groups behind Caddy | Software |
| R-RT-8 | Naming: members of the room now who were members during the turn, during or within 24 h; every name kept with counts; talking and doubled turns refuse; names never put a turn in an archive | Software |
| R-RT-9 | Load: the server stays under 256 MB and under half of one vCPU with 500 stations in 150 rooms sending readings once a second and 500 live viewers, for 30 minutes on a 1 GB droplet image beside the website's Caddy | Integration |
| R-RT-10 | Retention clears text older than 365 days; backups keep 2 on the droplet and 7 on JJ's machine; the snapshot opens and matches the live counts; snapshots stop under 15% free disk | Software, Integration |
| R-RT-11 | Text safety: a corpus of script, HTML and link tricks stores clean and renders inert on every screen and the web page | Software, Integration |
| R-RT-12 | AWAY: shown when no device is at the radio; an AWAY station can't post or name; the away choice "stop" stops reporting | Software |
| R-RT-13 | Hidden: going hidden sends `hidden` last and reads anonymously; starting hidden never signs in as a station; own reading NOT SENT; posting off; the archive still reachable | Software |
| R-RT-14 | First run: asked once per station; the answer kept on the Core; another device shows the one-time note; nothing sent before the answer (D25) | Software |
| R-RT-15 | The station link surface (capability, object, streams and record kinds, verbs, settings) matches the link spec and its fixtures | Software |
| R-RT-16 | Desktop: tags, folds, flag line, panel, pop-out tiers and memory, keyboard in panel and pop-out, Spot Hub tab, first run match section 11.2 and the mockups in offscreen captures at 1x and the Windows pixel density | Software |
| R-RT-17 | iPhone and iPad: flag line, sheet, PTT hold, keyed layout, dial strip, Tools page, swipes and Setup rows, iPad applet, widen and flag-line scroll match section 11.3, 11.4 and board.html | Software |
| R-RT-18 | Every screen meets U4, U6, U11 and U12 (ages and greying, mention counts and clearing, one scroll with the pill, 11 px and 4.5:1 except disabled controls) | Software, Integration |
| R-RT-19 | Public page with 200 rooms: band list, map, folds, search, sort, room pane, copy link, keyboard, live updates | Integration |
| R-RT-20 | Web sign-in: one-time links work once within 5 minutes; the token leaves the address bar; CSRF checks refuse a cross-site action; Sign out everywhere ends every session | Integration |
| R-RT-21 | Review list: report kinds, remove, block, stop counting (live hidden, stored rows kept), leave, helper conflict, 10-minute undo, helper changes by station id only, the log | Integration |
| R-RT-22 | Keys: issuance limit per address group; callsign change on the key page; history move by the old key only, never by callsign or by a helper | Software |
| R-RT-23 | Delete my archive removes what 13 says and keeps the station's readings of others | Software |
| R-RT-24 | A test client using a website key appears, reports, posts, names and reports exactly as a Core does | Integration |
| R-RT-25 | The API page on nereussdr.com is generated from the wire spec, and its published fixtures match the repository's | Software |
| R-RT-26 | Two Cores on JJ's bench on one frequency: JJ talks on one; the other's reading appears on the desktop, phone and public page within 2 s; AWAY shows when no device is connected | Device |

---

## 15. Mockups

In [reporter-brainstorm/](reporter-brainstorm/), committed with this spec:

| File | What it settles |
| --- | --- |
| `mockups/board.html` | iPhone and iPad on the app's workspace look: band, transmitting, first run, freshness, mentions, new messages, crowded band, swipes, Tools, naming, iPad. Pictures in `board-shots/` |
| `mockups/placement.html` | Desktop: pan tags, flag line, panel, pop-out tiers |
| `mockups/phone.html`, `mockups/ipad.html` | The first phone and iPad studies; board.html wins where they differ |
| `mockups/web.html` | Public page |
| `mockups/archive.html` | Archive |
| `mockups/review.html` | Review list |
| `mockups/settings.html` | Spot Hub tab |
| `mockups/frequencies.html`, `mockups/first-pass/` | The first sessions' mockups, superseded |
| `ux-review.md` | The review and fixes U1 to U14 |
| `HANDOFF.md`, `UI-REFERENCE.md` | The first sessions' research and decisions |

Where a mockup and this spec disagree, this spec wins and the mockup is the bug.

---

## 16. Build order

One plan covers all of it, in this order (D18):

1. Wire spec `2026-10-23-roundtable-v1.md` and conformance fixtures.
2. Server: identity, rooms, turns, readings, names, messages, limits, storage, retention, live stream, read API, web sign-in, key page, moderation; tests, including the load test (R-RT-9).
3. Server deploy: setup script, unit, Caddy site file and the website Caddyfile import line, the Caddy drop-in for nereus-web, backup timer and pull script, disk watch; the CI job.
4. Core: `RoundtableClient`, its key on a local-radio desktop, settings, hidden, AWAY, mentions; the station link capability, object, streams, verbs, settings, fixtures.
5. Desktop: tags and folds, flag line, panel, pop-out, keyboard, Spot Hub tab, first run.
6. iPhone and iPad: flag line, sheet, PTT hold, keyed layout, Tools page, swipes and Setup rows, iPad applet, first run.
7. Web: public page, archive, review list, key page, API page on nereussdr.com.
8. Deploy to the website droplet (JJ's yes for each server step), DNS (JJ), bench test (R-RT-26).

---

## 17. License and attribution

- All new code is NereusSDR's own. No Thetis, AetherSDR or freedv-gui code is ported. Grid and distance math is written from the public Maidenhead definition and the haversine formula, not from `FreeDVStationModel.cpp` (a freedv-gui port).
- The identity helpers copied from `rendezvous/` are our own code.
- If the plan finds a reason to reuse ported code, that file's header rules apply in the same commit (CLAUDE.md, HOW-TO-PORT.md).

---

## 18. Settled calls, listed for veto

Claude's calls. Each can change without touching section 3.

| # | Call |
| --- | --- |
| C1 | Room families and widths (8.1): LSB, USB, DIGL, DIGU, CW, FreeDV/RADE by sideband each separate; AM with SAM; FM. Widths ±1.5 kHz (sideband, digital, FreeDV), ±3 kHz AM, ±6 kHz FM, ±300 Hz CW. Opposite sidebands never share, because they hear different signals |
| C2 | A station joins after 10 s in a room, or at once on keying; "joining" until then |
| C3 | Signal present: at least 10 dB above the station's own floor, the 10th percentile of 60 s of readings taken while not present, held while present; 10 s warm-up after tuning; decided at the station |
| C4 | A turn ends after 3 s of quiet; same-talker gaps under 3 s join |
| C5 | Naming an unknown turn needs membership of that room now (D9) and at some time during the turn, for up to 24 h after; names from the last 24 h offered; names never put a turn in an archive |
| C6 | A lost key is a new station; history moves only by the old key (JJ's move between station ids dropped by D26) |
| C7 | Limits and address groups in 9.5 |
| C8 | An old protocol version keeps working at least six months after its successor |
| C9 | Delete my archive: removes your sessions, talking turns and others' readings of them, clears your message text, keeps your readings of others |
| C10 | Addresses: `roundtable.nereussdr.com`; API page `nereussdr.com/roundtable/api.html` |
| C11 | SQLite in WAL mode. Readings stored at one byte a second per station per turn (1 dB steps) plus a signal-present bit track, flushed every 30 s. This replaces the "per-turn summaries" first proposed for hosting, because the archive timeline and WATCHED need the per-second track |
| C12 | Memory cap 256 MB; a Caddy drop-in on nereus-web; nightly snapshot, 2 kept on the droplet and 7 on JJ's machine; snapshots stop under 15% free disk; disk warning under 20% |
| C13 | Formatting: bold, italic, http/https links shown with their address; 500 code points and 10 lines |
| C14 | The archive opens through a one-time link from the Core's station socket; 30-day browser session; Sign out everywhere |
| C15 | Settings: a Spot Hub tab after PSK Reporter; the away choice defaults to keep reporting marked AWAY; mention highlighting on; Delete my archive exists; what lives on the Core versus this computer as in 11.2 |
| C16 | Report reasons: Spam, Abuse, Off topic, Other, none; Wrong name on turns; False readings or keyed state on stations |
| C17 | Public page: readings nearest first, quiet rooms folded, band map once a band is picked, Newest activity sort; archive timelines capped at 12 stations with "Show all" |
| C18 | Timings: earlier turns 30 minutes; 50 messages on opening; a mention and a "new" mark clear after 2 s on screen; a session joins turns under 10 minutes apart; a room closes 10 minutes after its last member; a station leaves after 30 s silent; presence every 10 s; heartbeat 20 s / 45 s; Core backoff 1, 2, 5, 10, 30 s; skew rule 5 s; retention job 04:00 UTC; all-AWAY rooms quiet after 2 hours fold |
| C19 | Digital families have no doubles; each keyed member's transmission is its own turn; readings while two or more members are keyed are kept out; unknown turns stay, so D7 holds there too |
| C20 | The Roundtable station id is derived from the same station key with Roundtable's own prefix, not the rendezvous id the first sessions assumed, so the two services' records can't be joined |
| C21 | Server checks against false readings and keyed state (readings only from members and unkeyed slices; 10-minute long key); Report on a station; helpers can Stop counting a station, keeping its stored rows. Stop counting adds a helper power to D16's list (remove, block) |
| C22 | Ruled by JJ 2026-10-23: now D25 |
| C23 | From the UX fixes: while not live, Send is off and drafts stay, nothing queued; the connection line is Roundtable's own, the radio pill unchanged; the first-run answer is per station, kept on the Core, other devices show a note, the switch moves the main button, iPad form sheet; read on one device is read on all, the desktop flag works the same; a fold holds two or more and widens only if needed; stale rows grey at about 5:1 instead of faded; mention badge cyan; iPad message box pinned at the column foot |
| C24 | The Core sends `signalStrengthDbm`, the meter the operator sees, rather than the average |
| C25 | The frequency sent is the VFO frequency (the carrier on SSB, AM and FM, the signal on CW), stated per mode on the API page |
| C26 | Two slices of one station in one room: the first listed in `presence` counts (the Core lists the active slice first) |
| C27 | Web sign-in links last 5 minutes and work once; `roomsDelta` at most every 2 s; station link stream capacities 500 (band list) and 400 (one room) |
| C28 | Room ids are reused when a room reopens on the same center, with a new opening id each time; row refs are per opening (7.1); Mute is by base callsign |

---

## 19. Out of scope

- Logins, email, passwords, callsign verification (D23: only if abuse forces it, by a later decision).
- A lobby or chat not tied to a frequency.
- Averaging, ranking or a "best signal" view.
- Telling two unknown talkers apart in a double.
- Changes to the rendezvous or the relay.
- Clients for other software written by us: other software builds its own against the API.
- Translations of Roundtable text.
