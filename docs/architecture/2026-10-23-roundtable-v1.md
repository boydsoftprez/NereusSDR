# Roundtable protocol version 1

This is the written specification of Roundtable: the service at
`roundtable.nereussdr.com` that groups stations into rooms by frequency,
shows each station's own S-meter reading of whoever is talking, works out
turns, and carries short messages about each frequency. It is R-RT-1 of the
[Roundtable design](2026-10-23-roundtable-design.md), written out from that
design's sections 7, 8, 9 and 13 and its settled calls (section 18).

Four parties use it:

- **The server**, `roundtable/server/` in this repository (`nereus-roundtable`).
- **A station**: a NereusSDR Core, which signs in with its station identity
  key, or any other program, which signs in with a website key (section 4.2).
  Both speak exactly the same protocol (design D22 to D24).
- **A live reader**: anyone, with no sign-in, reading rooms as they change
  (section 5.5). The public page, a hidden Core and a Core whose first-run
  question is unanswered (design D25) are live readers.
- **A read API client**: anyone, over plain HTTPS `GET` (section 10).

The conformance suite in `roundtable/conformance/v1/` holds the server, a
Core and other software to this document (section 11). Where the server's
code and this document disagree, this document is the authority and the code
is the bug. Where this document and the design disagree, the design wins and
this document is the bug, to be fixed in the same change.

Section numbers are stable. Later revisions add sections; they never
renumber.

## 1. What it does and does not do

Roundtable does six things:

1. **Sign-in.** A station proves it holds its key and is then known by its
   Roundtable station id (section 4).
2. **Rooms.** From each station's slices it places the station in a room: one
   mode family and one center frequency (section 6.3).
3. **Turns.** From who is keyed and who hears a signal it works out turns on
   its own clock: talking, unknown, doubled and quiet (section 6.6).
4. **Readings.** It stores and shows every member's raw S-meter reading of
   the room's signal, beside the callsign of the station that took it, never
   averaged or ranked (section 6.8).
5. **Messages and names.** Members post short messages about the room and
   name turns whose talker was unknown (sections 6.9, 6.10).
6. **Reading.** It sends the band list and rooms to stations and live readers
   as they change, and answers the read API (sections 6.11, 10).

It never publishes a station id, an address, an archive or a mute list
(design 12.1). The archive, the review list, the key page and web sign-in are
web pages (design 11.6, 11.7, 7.3, 7.4); only the parts of them a station
reaches over this protocol (`weblink`, `deleteArchive`) are specified here.
There is no lobby, no chat that is not tied to a frequency, no averaging and
no "best signal".

## 2. Transport

### 2.1 Paths

| Path | Who | What |
| --- | --- | --- |
| `wss://roundtable.nereussdr.com/v1/station` | stations | the station protocol (sections 5.3, 5.4) |
| `wss://roundtable.nereussdr.com/v1/live` | anyone | read-only rooms (section 5.5) |
| `https://roundtable.nereussdr.com/v1/api/...` | anyone | the read API (section 10) |

- TLS on TCP 443. Caddy terminates TLS and forwards by path to the server on
  loopback: the WebSocket listener on port 8720 (`/v1/station`, `/v1/live`)
  and the HTTP listener on port 8721 (`/v1/api/...` and the web pages). The
  server never listens on a public address.
- The opening handshake is RFC 6455's over HTTP/1.1: `GET` with
  `Upgrade: websocket` (in any case), `Connection: Upgrade`,
  `Sec-WebSocket-Key` and `Sec-WebSocket-Version: 13`. WebSockets over
  HTTP/2 (RFC 8441) are not offered; a client whose stack would use HTTP/2
  keeps this connection to HTTP/1.1. A request to a WebSocket path that is
  not an upgrade gets `426`. Any other path on port 8720 gets `404`.
- A self-hosted server has its own host name. Everything here that names
  `roundtable.nereussdr.com` means the server's configured host name.

### 2.2 Frames and sizes

- Each message is one WebSocket text frame holding one JSON object, encoded
  compactly, with a string `type` naming its kind (section 5.1). A binary
  frame is a protocol error.
- The server caps each inbound message at 16384 bytes at the WebSocket
  layer, before anything is decoded. A longer one closes the connection
  with close code 1009. No message a conformant station or live reader sends
  comes near it: the longest is a `post` of 4000 bytes of text (section
  5.2), at most 24000 bytes if every byte were escaped, which a sender does
  not do; a sender writes non-ASCII text as UTF-8, not as escapes, so a
  `post` is at most about 4100 bytes.
- The server never sends a message longer than 1048576 bytes (1 MiB), and a
  station or live reader accepts any message from the server up to that
  size. The largest it sends is a `rooms` list with every live room or a
  `room` view with 50 messages.
- The server encodes compactly, writes non-ASCII characters as UTF-8, and
  writes keys in the order this document lists them. Receivers must not
  depend on key order.

### 2.3 Heartbeat

- The server sends a WebSocket ping every 20 s. A station or live reader
  answers pings as any WebSocket stack does and need not send its own.
- The server closes a connection from which no frame of any kind (a message,
  a pong or a ping) has arrived for 45 s, with close code 1000 and no
  message.
- A station or live reader that has received no frame of any kind (pings
  included) for 45 s treats the connection as lost, closes it and
  reconnects. A Core reconnects with backoff of 1, 2, 5, 10 and 30 s, then
  every 30 s (design 10.1).

### 2.4 Closing

The server closes with these codes. The reason a peer acts on is the close
code, or the `error` message sent just before the close when there is one;
the close frame's reason text is always empty.

| Code | Meaning | `error` first |
| --- | --- | --- |
| 1000 | the heartbeat timed out (2.3), or the server closed a connection it no longer serves | no |
| 1001 | the server is shutting down | no |
| 1009 | a message over the 16384-byte cap (2.2) | no |
| 1013 | too many connections from this network, or in all (section 9.1) | yes, `rate` |
| 4001 | sign-in refused: a key that is not canonical, a signature that does not verify, an unknown website key, or an unsupported version | yes: `bad-sign-in`, or `version` |
| 4002 | protocol error (section 5.1) | no |
| 4003 | replaced: the same station signed in on a newer connection | no |
| 4004 | key revoked: the website key this station signed in with is revoked, at sign-in or while connected | no |
| 4005 | sign-in timed out: no `hello` within 10 s of the connection opening | no |

A station that gets 4001, 4003 or 4004 does not reconnect in a loop: 4003
means another connection of the same station is live; 4001 and 4004 need the
operator (the `error` before 4001 says whether the key or the version is at
fault). A Core shows the reason on its connection line and stops retrying
until its key, its software or its settings change. 4005 is not the
operator's to fix: a Core reconnects after it with its normal backoff
(2.3), as after 1000, 1001 and 1013.

## 3. Roles and the life of a connection

### 3.1 Station

```
server  -> station:  challenge
station -> server:   hello
server  -> station:  welcome
server  -> station:  rooms
station -> server:   presence, reading, post, ... (section 5.3)
server  -> station:  roomsDelta, room, delta, posted, ... (section 5.4)
```

- The server sends `challenge` at once on every `/v1/station` connection it
  accepts. A connection over a connection cap (9.1) gets `error` `rate` in
  place of `challenge`, then close 1013.
- The station's first message must be `hello`. Anything else first is a
  protocol error (4002). No `hello` within 10 s of the connection opening
  closes it with 4005.
- After `welcome` and `rooms` the station may send any station kind except
  `hello` (a second `hello` is a protocol error).
- A station that never sends `presence` is never shown anywhere. That is how
  a hidden Core, or a Core whose first-run question is unanswered, reaches
  the archive: it signs in, sends `weblink` or `deleteArchive` only, and
  closes (design 7.4, 10.2, D25).
- One station id has at most one connection. A newer connection that signs
  in as the same station replaces the older, which is closed with 4003; the
  older connection's rows leave their rooms as when a connection closes
  (section 6.4) and the newer one starts with no presence.

### 3.2 Live reader

- `/v1/live` needs no sign-in and the server sends nothing until asked. A
  live reader sends `watchBand` for the band list and `watch` for up to 4
  rooms (section 5.5), and receives the same `rooms`, `roomsDelta`, `room`
  and `delta` kinds as a station, with no row marked as its own.
- A hidden Core reads Roundtable this way (design 10.2), and so does a Core
  whose first-run question nobody has answered yet: until someone taps Go
  live it opens only `/v1/live` and sends nothing about the station, however
  long it runs unattended (design D25).
- A live connection never carries a station id, a key or anything a station
  sends; it cannot post, name or report. A station kind on `/v1/live` is a
  protocol error (4002), and so is a live kind on `/v1/station`.

### 3.3 Read API client

Plain HTTPS `GET`, no sign-in, JSON answers (section 10). Archives are never
in the read API (design D11).

### 3.4 Browser

The public page is static HTML and script from the same site, and is a live
reader plus a read API client. The archive and the review list open in a
browser signed in with a session cookie, from a one-time link a station asks
for with `weblink` (section 7.9) or from the key page. Those pages are the
web's, outside this protocol.

## 4. Identity values

### 4.1 Station keys, signatures and the Roundtable id

Keys and signatures are the station link's (link section 3.4) and the
rendezvous's (rendezvous section 4.1), with no change:

- A **public key** (`spki`) is strict base64url (no padding, only
  `A-Z a-z 0-9 - _`, the unused low bits of the last character zero, a
  length that is not 1 mod 4) of a canonical 91-byte P-256
  SubjectPublicKeyInfo DER with its point uncompressed: 122 characters.
  Canonical means it loads as a P-256 key and encodes back to the same 91
  bytes.
- A **signature** (`sig`) is ECDSA P-256 over SHA-256, raw `r || s`, 64
  bytes, strict base64url: 86 characters. Not DER. `r` and `s` are each 32
  bytes big-endian and each from 1 to n - 1, n the order of the P-256 group.
  A high-s signature is valid.

A Core signs in with its **station identity key**, the same P-256 key the
station link and the rendezvous use (`station-identity.pem`). Its
**Roundtable id** is:

```
id = base32(SHA-256("NereusSDR roundtable id v1\n" || SPKI DER))[0..26]
```

- The prefix is the 26 ASCII bytes `NereusSDR roundtable id v1` followed by
  one line feed (0x0A): 27 bytes.
- `base32` is RFC 4648 base32 with the standard alphabet, lowercased, with no
  padding. The id is its first 26 characters, so it matches `[a-z2-7]{26}`.
- The prefix differs from the rendezvous id's (`NereusSDR rendezvous id
  v1\n`), so the two services never share an id for one key and their
  records cannot be joined (design C20). `crypto/roundtable-id.json` gives
  both ids for each test key.

### 4.2 Website keys and key ids

Other software signs in with a **website key** from the key page
(`roundtable.nereussdr.com/key`, design 7.3):

- A website key is 32 random bytes from the operating system's generator,
  strict base64url without padding: 43 characters. It is shown once.
- Its station id, the **key id**, is:

  ```
  id = base32(SHA-256("NereusSDR roundtable key v1\n" || key))[0..26]
  ```

  where `key` is the 32 raw bytes, not the text, and the prefix is the 27
  ASCII bytes `NereusSDR roundtable key v1` followed by one line feed: 28
  bytes. `base32` and the 26 characters are as in 4.1.
- The server stores only SHA-256 of the 32 raw key bytes (no prefix), never
  the key. On `hello` it hashes the key it is given and looks the hash up.
- One key is one station. A key station's callsign and grid are set on the
  key page; a callsign in its `presence` is ignored (section 5.3.2).
- History moves to a new key only by the old key, on the key page, while
  the old key is still held: "Move my history to a new key" revokes the old
  key and moves its sessions, turns, readings, names and messages to the new
  key's station id in one step (design 7.3, C6, D26). Nothing in this
  protocol moves history, and nobody moves history for someone else.
- A revoked key's connection is closed with 4004 at once, and a `hello` with
  it is refused with 4004.

`crypto/key-id.json` gives keys, their digests, ids and stored hashes, and
key texts that must be refused.

### 4.3 The sign-in transcript

On connect the server sends `challenge` with `nonce`: 32 bytes from the
operating system's generator, new for every connection. A station signing in
with its station key signs:

```
"NereusSDR roundtable hello v1\n" || host || "\n" || nonce
```

- The prefix is the 29 ASCII bytes `NereusSDR roundtable hello v1` followed
  by one line feed: 30 bytes.
- `host` is the ASCII bytes of the host name the station connected to, as it
  appears in the URL, lowercase, with no port and no trailing dot:
  `roundtable.nereussdr.com`, 24 bytes. A Core configured with a self-hosted
  server signs that server's name.
- Then one line feed, then `nonce` as its **32 raw bytes**, not its text.
  For `roundtable.nereussdr.com` the transcript is 30 + 24 + 1 + 32 = 87
  bytes.
- The server verifies the signature over the transcript built with its own
  configured host name and the nonce it sent on this connection. A
  signature over the rendezvous's registration transcript, or over another
  host name, never verifies: the prefix keeps signatures from being replayed
  between the two services, and the host name stops another server passing a
  challenge through to a real one.

A station signing in with a website key sends the key instead and signs
nothing; the TLS connection carries it.

`crypto/hello-signature.json` gives a fixed key, a host, a nonce, the
transcript bytes and valid and invalid signatures.

### 4.4 Row refs and other ids

- **Row ref** (`ref`): when a station's row enters a room opening, the server
  gives it a random 8-character ref from the alphabet `a-z 2-7` (40 bits). It
  is kept while that opening lives and while the row stays; a row that
  leaves and comes back gets a new ref. Refs differ in every room and every
  opening, so they never link a station across rooms (design 7.1). Reports
  on a station name a room and a ref.
- **Turn id**, **message id** and **opening id**: whole numbers from 1, chosen
  by the server, unique across the server and increasing with time.
- **Room id**: `<family>-<center Hz>`, for example `usb-14212000` (section
  6.3). It is reused when a room reopens on the same center; each opening
  has its own opening id.
- Station ids never appear in anything public: the live connection, the read
  API and the public page carry callsigns and row refs (design 12.1). A
  station sees its own id in `welcome`.

### 4.5 Callsigns and grids

- A **callsign** is 3 to 16 characters of `A-Z a-z 0-9 /`, holding at least
  one letter and one digit, not starting or ending with `/` and with no
  `//`. The server stores and sends it upper case. Prefixes and suffixes
  (`VE3/`, `/P`, `/M`) are part of the callsign shown.
- The **base callsign** is the longest of the callsign's `/`-separated parts,
  the first of them on a tie: `W4XYZ` for `VE3/W4XYZ/P`. Mentions, Mute and
  "names used on this frequency" match on the base callsign (design 7.1).
- A **grid** is a Maidenhead locator of 4 or 6 characters: a field of two
  letters `A` to `R`, a square of two digits, and optionally a subsquare of
  two letters `A` to `X`, in any case. The server stores and sends it with
  the field upper case and the subsquare lower case (`EM73tu`).
- A Core's callsign comes from `Roundtable/Callsign`, then `User/Callsign`,
  then the Core's `StationCallsign`; its grid from `Roundtable/GridSquare`,
  then `User/GridSquare` (design 7.1).
- **What a Core sends.** A malformed callsign or grid in `presence` is a
  protocol error (5.1), so a Core checks both against these rules before it
  connects and never sends one the server would refuse:
  - A Core with no callsign, or with a callsign that is not a callsign by
    the rule above (a hyphen, a space, too long), does not connect, and its
    source text on the Spot Hub says why, naming the setting to fix.
  - A grid of 8 characters (an extended locator such as `EM73tu42`) is cut
    to its first 6. A grid that is then not a grid by the rule above (2
    characters, 10, letters out of range, empty) is sent as `null`, and the
    Core still connects.

## 5. Messages

### 5.1 The envelope

Each message is a JSON object with a string `type`. Keys are case
sensitive. For every message the server receives:

- Every key its kind lists must be present, with the field kind section 5.2
  gives, or the message is a protocol error. A key listed as conditional
  (`report`'s) must be present exactly when its condition holds.
- A key its kind does not list is ignored, at any depth (inside a slice
  object too).
- These are protocol errors: a kind the server does not know, a kind of the
  other path (a live kind on `/v1/station`, a station kind on `/v1/live`), a
  server kind, a binary frame, JSON that does not parse, a top level that is
  not an object, a missing or non-string `type`, a key that appears twice in
  one object (listed or not), `NaN` or `Infinity`, and a string in a listed
  key holding a lone surrogate.
- A whole number is a JSON number written without a fraction or an
  exponent. `true` and `false` are not numbers; `7.0` and `"7"` are not whole
  numbers. A number (`dbm`) may have a fraction but no exponent.
- A protocol error closes the connection with 4002 and no `error` message.

A string's length is counted in bytes of its UTF-8 encoding unless a field
kind says code points.

**At a station or live reader.** A station or live reader decodes what the
server sends by the same tables with the same tolerance: a key it does not
know, in a kind or object it knows, is ignored; a kind it does not know, a
code it does not know in `error`, `posted` or `notice`, and a message it
cannot decode are logged and ignored (an unknown code is treated as the
generic form of its message). The server relies on this to add keys and
kinds later (section 12).

### 5.2 Field kinds

| Kind | Meaning |
| --- | --- |
| `version` | a whole number from 1 to 65535 |
| `sid` | a station id: exactly 26 characters of `a-z` and `2-7` (4.1, 4.2) |
| `spki` | strict base64url of exactly 91 bytes, 122 characters (4.1); whether it is a canonical P-256 key is checked after decoding |
| `sig` | strict base64url of exactly 64 bytes, 86 characters |
| `nonce` | strict base64url of exactly 32 bytes, 43 characters |
| `websiteKey` | strict base64url of exactly 32 bytes, 43 characters (4.2) |
| `software` | a string of 1 to 64 bytes with no control character: the program's name, for example `NereusSDR` |
| `softwareVersion` | a string of 1 to 32 bytes with no control character, for example `26.10.3` |
| `callsign` | a callsign (4.5) |
| `grid` | a grid (4.5) |
| `bool` | `true` or `false` |
| `sliceId` | a whole number from 0 to 255: the station's own id for one slice |
| `hz` | a whole number from 1 to 99999999999: a frequency in Hz |
| `filterHz` | a whole number from 1 to 1000000: a filter width in Hz |
| `mode` | one of `LSB` `USB` `DIGL` `DIGU` `AM` `SAM` `FM` `CWL` `CWU` `FDVL` `FDVU`, upper case (6.1) |
| `family` | one of `lsb` `usb` `digl` `digu` `am` `fm` `cw` `fdvl` `fdvu` (6.1) |
| `band` | one of the band names of section 6.2, or `other` |
| `roomId` | `<family>-<center Hz>`: a family, a hyphen, and a whole number from 1 to 99999999999 written in decimal with no leading zero |
| `dbm` | a JSON number from -160 to 94 inclusive, fraction allowed: an S-meter reading in dBm |
| `unixMs` | a whole number from 0 to 9007199254740991: Unix epoch milliseconds |
| `id` | a whole number from 1 to 9007199254740991: a turn, message or opening id |
| `gen` | a whole number from 1 to 9007199254740991: a generation (6.11) |
| `count` | a whole number from 0 to 9007199254740991 |
| `nth` | a whole number from 1 to 65535: which station of one callsign in one room, in joining order (6.8) |
| `km` | a whole number from 0 to 20038, or `null` |
| `offsetHz` | a whole number from -1000000 to 1000000 |
| `rowRef` | exactly 8 characters of `a-z` and `2-7` (4.4) |
| `text` | a string of 1 to 4000 bytes: a message as sent (6.10) |
| `clientId` | a string of 1 to 64 bytes of printable ASCII (0x21 to 0x7E): the sender's own id for one `post` |
| `reason` | a string of 1 to 1024 bytes: plain words a screen may show as sent |
| `code` | 1 to 32 characters of `a-z` and `-`: an error, refusal or notice code |
| `about` | 1 to 32 ASCII letters, the `type` of the message an `error` answers, or `null` |
| `retry` | a whole number from 0 to 2147483647: milliseconds to wait; 0 means no advice |
| `weblinkUrl` | a string of 1 to 512 bytes of printable ASCII starting `https://` |
| `state` | one of `talking` `unknown` `doubled` `quiet` |
| `turnKind` | one of `talking` `unknown` `doubled` |
| `messageState` | one of `shown` `removed` `deleted` `expired` |
| `reportReason` | per `report` target (5.3.7) |

Where a kind is written "or `null`" the key is present with `null`. Arrays
hold only the kind named.

**Objects.** The server's messages carry these objects. Each lists its keys;
an object missing one is a message a receiver cannot decode.

| Object | Keys |
| --- | --- |
| slice (from a station) | `id` (`sliceId`), `hz` (`hz`), `mode` (`mode`), `filterHz` (`filterHz`), `keyed` (`bool`) |
| summary | `id` (`roomId`), `band` (`band`), `family` (`family`), `hz` (`hz`, the center), `state` (`state`), `talkers` (array of `callsign`), `members` (`count`), `messages` (`count`), `last` (`unixMs`), `folded` (`bool`), `start` (`unixMs` or `null`), `heard` (`count`), `readings` (array of `dbm`, at most 12), `lastMessage` (last message or `null`), `ended` (`unixMs` or `null`) |
| last message | `callsign` (`callsign`), `nth` (`nth`), `t` (`unixMs`), `state` (`messageState`), `text` (the first 80 code points of the message's `text` when `state` is `shown`, otherwise `null`) |
| member | `ref` (`rowRef`), `callsign` (`callsign`), `nth` (`nth`), `grid` (`grid` or `null`), `km` (`km`), `offsetHz` (`offsetHz`), `joining` (`bool`), `away` (`bool`), `keyed` (`bool`), `software` (`software`), `softwareVersion` (`softwareVersion`), `reading` (reading or `null`), `you` (`bool`) |
| reading | `dbm` (`dbm`), `t` (`unixMs`), `present` (`bool`), `turn` (`id` or `null`) |
| turn | `id` (`id`), `kind` (`turnKind`), `talkers` (array of talker), `start` (`unixMs`), `end` (`unixMs` or `null`), `ending` (`bool`), `names` (array of name count), `heard` (`count`), `mine` (`callsign` or `null`) |
| talker | `ref` (`rowRef` or `null`), `callsign` (`callsign`), `nth` (`nth`) |
| name count | `name` (`callsign`), `count` (`count`, at least 1) |
| message | `id` (`id`), `ref` (`rowRef` or `null`), `callsign` (`callsign`), `nth` (`nth`), `t` (`unixMs`), `state` (`messageState`), `text` (`text` when `state` is `shown`, otherwise `null`), `dbm` (`dbm` or `null`) |
| mention | `room` (`roomId`), `message` (message) |
| limits | `readingsPerSecond`, `presencePerSecond`, `messagesPerMinute`, `messagesPerHour`, `messageCodePoints`, `messageLines`, `namesPerMinute`, `reportsPerHour`, `slices`, `reads`, each a `count` (section 9.1) |

What each value means is section 6.

### 5.3 Station to server

| Kind | Keys | Section |
| --- | --- | --- |
| `hello` | `v` (`version`), `software` (`software`), `softwareVersion` (`softwareVersion`), and either `spki` (`spki`) with `sig` (`sig`), or `key` (`websiteKey`) | 5.3.1 |
| `presence` | `callsign` (`callsign` or `null`), `grid` (`grid` or `null`), `atRadio` (`bool`), `slices` (array of 0 to 4 slice objects) | 5.3.2 |
| `reading` | `slice` (`sliceId`), `dbm` (`dbm`), `t` (`unixMs`), `present` (`bool`) | 5.3.3 |
| `post` | `room` (`roomId`), `text` (`text`), `clientId` (`clientId`) | 5.3.4 |
| `delete` | `msg` (`id`) | 5.3.5 |
| `name` | `turn` (`id`), `name` (`callsign`) | 5.3.6 |
| `report` | `target` (`message`, `turn` or `station`), `reason` (`reportReason`), and by target `msg` (`id`), `turn` (`id`), or `room` (`roomId`) with `ref` (`rowRef`) | 5.3.7 |
| `read` | `room` (`roomId`) | 5.3.8 |
| `unread` | `room` (`roomId`) | 5.3.9 |
| `resync` | `room` (`roomId` or `null`) | 5.3.10 |
| `mentionsSeen` | `t` (`unixMs`) | 5.3.11 |
| `weblink` | none | 5.3.12 |
| `deleteArchive` | `confirm` (a string of 1 to 64 bytes) | 5.3.13 |
| `hidden` | none | 5.3.14 |

Every request the server refuses is answered with `error` (5.4.11) naming
its `type` in `about`, or for `post` with `posted` (5.4.7); the connection
stays. A request the server accepts is answered only where a section below
says so; otherwise its effect shows in the views (section 6.11).

After `hidden` (5.3.14) every station kind except `weblink` and
`deleteArchive` is refused with `error` `hidden`.

#### 5.3.1 hello

```
{"type":"hello","v":1,"software":"NereusSDR","softwareVersion":"26.10.3","spki":"<spki>","sig":"<sig>"}
{"type":"hello","v":1,"software":"Example Logger","softwareVersion":"3.1","key":"<websiteKey>"}
```

- `v` is the protocol version the station speaks, 1 here. Any other value is
  answered with `error` `version` and the close 4001.
- `software` and `softwareVersion` name the program; they are public on the
  station's rows while it is present in a room (design 12.1).
- With `spki` and `sig`: the server checks the key is a canonical P-256 key,
  derives the Roundtable id (4.1), and verifies `sig` over the transcript of
  4.3. A key that is not canonical, or a signature that does not verify, is
  answered with `error` `bad-sign-in` and the close 4001.
- With `key`: the server looks up its hash (4.2). An unknown key is answered
  with `error` `bad-sign-in` and the close 4001; a revoked key closes with
  4004.
- `spki` and `key` together, neither, or `spki` without `sig`, is a protocol
  error.
- On success the server answers `welcome` and then `rooms`. A newer
  connection for the same station id replaces an older one (3.1).

#### 5.3.2 presence

```
{"type":"presence","callsign":"KG4VCF","grid":"EM73","atRadio":true,"slices":[{"id":0,"hz":14212000,"mode":"USB","filterHz":2700,"keyed":false}]}
```

- Sent on every change, coalesced to at most one a second, and every 10 s
  while nothing changes (design 8.3). Each `presence` replaces the last
  whole: a slice not listed has left.
- `slices`: up to 4, ids unique within the array (a repeated id is a
  protocol error). A Core lists its active slice first (6.4) and leaves out
  slices in a mode with no family (6.1). `hz` is the frequency an operator
  would quote on the air for that mode (6.1). `keyed` is true while that
  slice is transmitting.
- `callsign`: a Core sends its callsign (4.5); `null` from a Core is refused
  with `error` `not-allowed` and the presence is not applied. A key station
  may send a callsign or `null`; either is ignored and the key page's
  callsign is used.
- `grid`: the station's grid, or `null` for none. For a key station `null`
  means the key page's grid. A Core sends only what 4.5 allows.
- `atRadio`: whether anyone is at the radio (design D14, 10.1). False marks
  the station AWAY (6.4).
- Refused: over the presence limit (`error` `rate`, with `retryAfterMs`; a
  Core then sends its current presence again after that time).

#### 5.3.3 reading

```
{"type":"reading","slice":0,"dbm":-91.5,"t":1800000000123,"present":true}
```

- One slice's S-meter reading: `dbm` as the station's own meter shows it
  (a Core sends `signalStrengthDbm`, design C24), `t` the Unix time in ms
  when it was taken, `present` the station's signal-present decision (6.5).
- Checked in this order. A reading for a slice not in the station's latest
  `presence` (or before any `presence`) is refused with `error`
  `unknown-slice`. A reading for a slice whose latest `presence` says keyed
  is refused with `error` `slice-keyed`. A reading from a slice that is
  joining (6.4), or from a slice that is not the station's counted slice in
  its room, is dropped silently. A reading over the limit is dropped, with
  at most one `error` `rate` a minute per station.
- Otherwise it is accepted silently and shows in the room (6.7, 6.11).

#### 5.3.4 post

```
{"type":"post","room":"usb-14212000","text":"Good copy here, **S7** in Boston.","clientId":"m-1800000003000-1"}
```

- A message to a room in which the station's counted slice is a member.
- Always answered with `posted` (5.4.7) carrying the same `clientId`.
  Refusals, checked in this order: `hidden`; `not-member` (the station is not
  a member of a live room with this id, whether or not the room exists);
  `away`; `blocked`; `rate`; `too-long` (over 500 code points or 10 lines
  after cleaning, 6.10); `not-allowed` (nothing left after cleaning);
  `server` (the database cannot save it, design 13).
- On success the message shows in the room (a `delta` to every viewer, the
  sender included) and as `mention` to every station it names (6.10).

#### 5.3.5 delete

```
{"type":"delete","msg":981}
```

- Deletes one of the station's own messages: its text goes, "Deleted by
  sender" shows in its place (`state` `deleted`). Refused: `not-found` (no
  such message), `not-allowed` (another station's message). Deleting a
  message already deleted does nothing. A member need not still be in the
  room.

#### 5.3.6 name

```
{"type":"name","turn":4407,"name":"W4XYZ"}
```

- Names an unknown turn (6.9). A station has one name per turn; a second
  `name` replaces its first.
- Refusals, checked in this order: `hidden`; `not-found` (no such turn);
  `bad-turn` (a talking or doubled turn, or more than 24 hours after the turn
  ended); `not-member` (not now a member of a live room with the turn's room
  id); `away`; `blocked`; `not-allowed` (the station was not a member of
  that room at any time during the turn); `rate`.
- On success the turn's `names` change in the room's views.

#### 5.3.7 report

```
{"type":"report","target":"message","msg":975,"reason":"spam"}
{"type":"report","target":"turn","turn":4407,"reason":"wrongName"}
{"type":"report","target":"station","room":"usb-14212000","ref":"k3m7q2xa","reason":"falseReadings"}
```

| `target` | Keys it adds | `reason` |
| --- | --- | --- |
| `message` | `msg` (`id`) | `spam`, `abuse`, `offTopic`, `other` or `null` |
| `turn` | `turn` (`id`) | `wrongName` or `null` |
| `station` | `room` (`roomId`), `ref` (`rowRef`) | `falseReadings` or `null` |

- Puts the item on the review list (design 11.7, 12.2). There is no free
  text. Any signed-in station may report. The reported station is never told
  who reported; reports never hide anything by count (design D16).
- Refused: `not-found` (no such message or turn, or no such row in that
  live room), `rate`. A second report of one item by one station is
  accepted and counts once. A `reason` not in its target's row, or a target's
  key missing, is a protocol error.
- Answered by nothing on success. When a helper closes the report the
  reporter gets `notice` `report-closed`.

#### 5.3.8 read

```
{"type":"read","room":"lsb-7188000"}
```

- Starts reading another room without tuning to it (design U1): the server
  answers with a full `room` view (5.4.5) and then its `delta`s. A station
  reads at most 4 rooms besides those its slices are in.
- Refused: `not-found` (no live room with that id), `not-allowed` (already
  reading 4). Reading a room already read, or one the station is in, sends
  its full view again.

#### 5.3.9 unread

```
{"type":"unread","room":"lsb-7188000"}
```

Stops reading a room. Answered by nothing, never refused; a room the station
is in stays in its view.

#### 5.3.10 resync

```
{"type":"resync","room":null}
{"type":"resync","room":"usb-14212000"}
```

- After a gap in generations (6.11): `room` `null` asks for a full `rooms`,
  a room id for a full `room` view of a room in the station's view. Each
  starts its generation again at 1.
- Refused: `not-found` (the room is not in the station's view).

#### 5.3.11 mentionsSeen

```
{"type":"mentionsSeen","t":1800000003000}
```

Every mention of the station in a message posted at or before `t` is seen;
the next `welcome` lists only later ones. Answered by nothing. Read on one
device is read on all, because the Core sends this for all of them (design
U6).

#### 5.3.12 weblink

```
{"type":"weblink"}
```

Asks for a one-time web sign-in link (design 7.4). Answered with
`weblinkOk` (5.4.10). Allowed with no `presence` and after `hidden`.

#### 5.3.13 deleteArchive

```
{"type":"deleteArchive","confirm":"delete my archive"}
```

- Deletes the station's archive (design 13, C9): its sessions, its talking
  turns and the readings others made of them, and the text of its messages.
  Its readings of other talkers stay, under the callsign stored with them.
- `confirm` must be exactly `delete my archive`; any other string is refused
  with `error` `not-allowed`. The station's software asks the operator to
  confirm first, saying it cannot be undone and that backups age out within
  7 days.
- Answered with `notice` `archive-deleted` once done. Allowed with no
  `presence` and after `hidden`.

#### 5.3.14 hidden

```
{"type":"hidden"}
```

- The last thing a station sends on going hidden (design 10.2). The server
  ends the station's presence at once: its rows leave every room (a `delta`
  to every other viewer), its open talking turn ends at that moment with no
  3-second wait, and its joining slices are dropped.
- From then on the server sends that connection nothing but answers
  (`error`, `weblinkOk`, `notice`), and refuses every station kind except
  `weblink` and `deleteArchive` with `error` `hidden`. The station closes the
  connection and reads as a live reader.

### 5.4 Server to station

| Kind | Keys | Section |
| --- | --- | --- |
| `challenge` | `nonce` (`nonce`) | 5.4.1 |
| `welcome` | `station` (`sid`), `kind` (`core` or `key`), `callsign` (`callsign` or `null`), `grid` (`grid` or `null`), `time` (`unixMs`), `limits` (limits), `mentions` (array of mention) | 5.4.2 |
| `rooms` | `gen` (`gen`), `rooms` (array of summary) | 5.4.3 |
| `roomsDelta` | `gen` (`gen`), `add` (array of summary), `change` (array of summary), `remove` (array of `roomId`) | 5.4.4 |
| `room` | `gen` (`gen`), `id` (`roomId`), `opening` (`id`), `band` (`band`), `family` (`family`), `hz` (`hz`), `members` (array of member), `now` (array of turn), `earlier` (array of turn), `messages` (array of message) | 5.4.5 |
| `delta` | `room` (`roomId`), `gen` (`gen`), `members` (array of member), `left` (array of `rowRef`), `turns` (array of turn), `messages` (array of message), `closed` (`bool`) | 5.4.6 |
| `posted` | `clientId` (`clientId`), `ok` (`bool`), `msg` (`id` or `null`), `code` (`code` or `null`), `reason` (`reason` or `null`) | 5.4.7 |
| `mention` | `room` (`roomId`), `message` (message) | 5.4.8 |
| `notice` | `code` (`code`), `text` (`reason`), `until` (`unixMs` or `null`) | 5.4.9 |
| `weblinkOk` | `url` (`weblinkUrl`), `expires` (`unixMs`) | 5.4.10 |
| `error` | `code` (`code`), `reason` (`reason`), `about` (`about`), `retryAfterMs` (`retry`) | 5.4.11 |

#### 5.4.1 challenge

```
{"type":"challenge","nonce":"<nonce>"}
```

First on every `/v1/station` connection the server accepts; a connection
over a cap gets `error` `rate` instead, then close 1013 (3.1, 9.1). The
nonce is signed in `hello` (4.3); a key station ignores it.

#### 5.4.2 welcome

```
{"type":"welcome","station":"<sid>","kind":"core","callsign":null,"grid":null,"time":1800000000000,"limits":{"readingsPerSecond":2,"presencePerSecond":1,"messagesPerMinute":6,"messagesPerHour":60,"messageCodePoints":500,"messageLines":10,"namesPerMinute":10,"reportsPerHour":10,"slices":4,"reads":4},"mentions":[]}
```

- `station`: the station's own id. `kind`: `core` for a station key, `key`
  for a website key. `callsign` and `grid`: the key page's, for a key
  station; `null` for a Core, which sends its own.
- `time`: the server's clock, Unix ms. A station may use it to see its own
  clock's offset; turns never depend on station clocks (6.6).
- `limits`: the limits in force (9.1). A receiver ignores limit keys it does
  not know.
- `mentions`: messages naming the station posted after its last
  `mentionsSeen`, oldest first, at most the newest 50 (6.10).

#### 5.4.3 rooms

```
{"type":"rooms","gen":1,"rooms":[{"id":"usb-14212000","band":"20m","family":"usb","hz":14212000,"state":"talking","talkers":["K4ABC"],"members":2,"messages":2,"last":1800000004000,"folded":false,"start":1800000001000,"heard":1,"readings":[-81.5],"lastMessage":{"callsign":"W4XYZ","nth":1,"t":1800000003000,"state":"shown","text":"Good copy, 57 here"},"ended":null}]}
```

The band list, full: every live room (to a live reader, every live room of
its watched band), in ascending order of center frequency, with `gen` 1.
Sent after `welcome`, after `watchBand`, and in answer to `resync` with
`room` `null`. What each summary key means is section 6.11.

#### 5.4.4 roomsDelta

```
{"type":"roomsDelta","gen":2,"add":[...],"change":[...],"remove":["lsb-7188000"]}
```

Changes to the band list since the last `rooms` or `roomsDelta`, with
`gen` one more than the last: rooms that opened (`add`), rooms whose summary
changed (`change`, the whole new summary), rooms that closed (`remove`). Sent
at most every 2 s, cut from one band list the server builds each 2 s for
every connection (6.11). A room appears in at most one of
the three arrays. Changes collected for one `roomsDelta` are merged against
the band list the connection last received: a room that opened and closed
since then is in none of the arrays; a room that closed and opened again on
the same id is in `change` with its new summary; a room that opened and then
changed is in `add` with its latest summary; a room that changed and then
closed is in `remove` only.

#### 5.4.5 room

```
{"type":"room","gen":1,"id":"usb-14212000","opening":17,"band":"20m","family":"usb","hz":14212000,"members":[...],"now":[...],"earlier":[...],"messages":[...]}
```

A full view of one room, with `gen` 1. `members`: every row, members and
joining, in joining order. `now`: the open turns (at most one, except in
digital families, 6.6). `earlier`: turns that ended in the last 30 minutes,
newest first. `messages`: the last 50 messages of the room id (any opening),
oldest first; older ones come from the read API (10.4). Sent when the
station's slice enters a live room, when a room it is in opens, in answer to
`read`, and in answer to `resync` for that room.

#### 5.4.6 delta

```
{"type":"delta","room":"usb-14212000","gen":2,"members":[...],"left":["w2c6tz7e"],"turns":[...],"messages":[],"closed":false}
```

One change to one room in a connection's view, with `gen` one more than the
last for that room. `members`, `turns` and `messages` are whole objects that
replace the receiver's object with the same `ref` or `id`, or are added when
it has none. `left` lists refs whose rows left. `closed` true means the room
closed (6.3); it is the last `delta` for that room. A turn that leaves the
30-minute `earlier` window is not announced; receivers drop it themselves.

#### 5.4.7 posted

```
{"type":"posted","clientId":"m-1800000003000-1","ok":true,"msg":981,"code":null,"reason":null}
{"type":"posted","clientId":"c2","ok":false,"msg":null,"code":"too-long","reason":"A message can be up to 500 characters and 10 lines."}
```

The answer to every `post`, before the `delta` the message causes. With
`ok` true, `msg` is the new message's id and `code` and `reason` are `null`;
with `ok` false, `msg` is `null` and `code` and `reason` say why (section 8).
A refused post's text stays in the sender's box with the reason under it
(design 13).

#### 5.4.8 mention

```
{"type":"mention","room":"usb-14212000","message":{"id":990,"ref":"w2c6tz7e","callsign":"W4XYZ","nth":1,"t":1800000003000,"state":"shown","text":"KG4VCF are you copying K4ABC?","dbm":-91.5}}
```

A message naming the station's base callsign (6.10), sent as it is posted,
whether or not the station has that room in view.

#### 5.4.9 notice

```
{"type":"notice","code":"blocked","text":"A Roundtable helper has blocked this station from posting and naming turns.","until":1800086400000}
```

Plain operator text from the server. `until` is when it stops applying, or
`null`. Codes in section 8.4.

#### 5.4.10 weblinkOk

```
{"type":"weblinkOk","url":"https://roundtable.nereussdr.com/signin?t=<43 characters>","expires":1800000300000}
```

The one-time sign-in link: `https://<host>/signin?t=` and a token of 32
random bytes, strict base64url (43 characters). It works once, until
`expires`, 5 minutes after it was made (design C27). Opening it swaps the
token for a session cookie (`Secure`, `HttpOnly`, `SameSite=Strict`, 30
days) and redirects, so the token leaves the address bar (design 7.4).

#### 5.4.11 error

```
{"type":"error","code":"not-found","reason":"That room, turn or message is not here.","about":"read","retryAfterMs":0}
```

A refused request, with `about` the `type` of the message refused (`null`
when the error answers no message, as for the connection caps). `retryAfterMs`
is set for `rate` (at least 1) and is otherwise 0. Section 8.

### 5.5 Live

| From | Kind | Keys | Answer |
| --- | --- | --- | --- |
| live reader | `watchBand` | `band` (`band` or `all`) | `rooms` for that band (or all), then its `roomsDelta`s; a new `watchBand` replaces the old |
| live reader | `watch` | `room` (`roomId`) | `room`, then its `delta`s; at most 4 at once (`error` `not-allowed` beyond), `error` `not-found` for no live room |
| live reader | `unwatch` | `room` (`roomId`) | nothing |
| live reader | `resync` | `room` (`roomId` or `null`) | `rooms` (for the watched band) or `room`, generation 1; `error` `not-found` when not watched |
| server | `rooms`, `roomsDelta`, `room`, `delta`, `error` | as 5.4 | |

```
{"type":"watchBand","band":"20m"}
{"type":"watch","room":"usb-14212000"}
```

A live reader's views are a station's with every `you` false and every
`mine` `null`. The band list's summaries carry what a room's row shows
(6.11), so a page fills its rows from `rooms` and `roomsDelta` alone.
Unknown kinds and station kinds close the connection with 4002.

## 6. Rooms, turns and readings

### 6.1 Modes and families

| Mode sent | Family | Width (either side of center) | The frequency sent |
| --- | --- | --- | --- |
| `LSB` | `lsb` | 1500 Hz | the suppressed carrier (the VFO) |
| `USB` | `usb` | 1500 Hz | the suppressed carrier (the VFO) |
| `DIGL` | `digl` | 1500 Hz | the suppressed carrier (the VFO) |
| `DIGU` | `digu` | 1500 Hz | the suppressed carrier (the VFO) |
| `AM`, `SAM` | `am` | 3000 Hz | the carrier |
| `FM` | `fm` | 6000 Hz | the carrier |
| `CWL`, `CWU` | `cw` | 300 Hz | the signal (NereusSDR's VFO on CW) |
| `FDVL` (FreeDV or RADE, lower sideband) | `fdvl` | 1500 Hz | the suppressed carrier (the VFO) |
| `FDVU` (FreeDV or RADE, upper sideband) | `fdvu` | 1500 Hz | the suppressed carrier (the VFO) |

- These are design C1 and C25. Opposite sidebands never share a room: an LSB
  and a USB receiver on one dial frequency hear different signals.
- `digl` and `digu` are the **digital families** (6.6).
- A slice in any other mode (DSB, SPEC, DRM and the like) has no family and
  is left out of `presence`.
- The frequency sent is the one an operator would quote on the air. Other
  software sends the same for its mode.

### 6.2 Bands

A room's band is the band holding its center frequency, edges included:

| Band | From Hz | To Hz |
| --- | --- | --- |
| `2200m` | 135700 | 137800 |
| `630m` | 472000 | 479000 |
| `160m` | 1800000 | 2000000 |
| `80m` | 3500000 | 4000000 |
| `60m` | 5250000 | 5450000 |
| `40m` | 7000000 | 7300000 |
| `30m` | 10100000 | 10150000 |
| `20m` | 14000000 | 14350000 |
| `17m` | 18068000 | 18168000 |
| `15m` | 21000000 | 21450000 |
| `12m` | 24890000 | 24990000 |
| `10m` | 28000000 | 29700000 |
| `6m` | 50000000 | 54000000 |
| `4m` | 70000000 | 71000000 |
| `2m` | 144000000 | 148000000 |
| `1.25m` | 222000000 | 225000000 |
| `70cm` | 420000000 | 450000000 |
| `33cm` | 902000000 | 928000000 |
| `23cm` | 1240000000 | 1300000000 |

Every other frequency is band `other`. The table is the union of the IARU
regions' allocations, so a room never depends on where its stations are.

### 6.3 Rooms

- A **room** is one family and one center frequency (design D8). Its id is
  `<family>-<center Hz>` (`usb-14212000`); its link is
  `roundtable.nereussdr.com/r/<family>/<center Hz>`.
- **Placing a slice.** A slice of a family is placed in the room, live or
  candidate (below), of its family whose center is nearest its frequency, if
  the distance is at most the family's width (6.1); between two equally
  near, the lower center. If there is none, it forms a new candidate room
  centered on its own frequency. A slice stays in its room while its family
  is unchanged and its frequency stays within the room's width of the
  center; otherwise it leaves and is placed again.
- **Candidate rooms.** A room is a candidate until one of its slices becomes
  a member (6.4). A candidate is not listed, has no view and no id in use,
  and vanishes when its last slice leaves. So a station tuning across a band
  makes no rooms. The server sends nothing about a candidate, so a slice in
  one has no room id yet: a Core shows it as joining with room id `null`.
  When a slice in a candidate becomes a member (after 10 s, or at once on
  keying, 6.4) the room **opens**: it gets a new opening id and is listed,
  and the station gets its view.
- The center is fixed while the room lives. It never moves when its first
  station leaves (design 8.1).
- A room **lives** while it has members, and **closes** 10 minutes after its
  last member left with none since (design C18). A room with no members is
  still live, listed and joinable during those 10 minutes. On closing it
  gets `delta` `closed` true to its viewers and `roomsDelta` `remove`; any
  slices still joining it stay, in a candidate on the same center.
- A room that opens on a center whose id was used before reuses the id with
  a new opening id (design C28). Turns and messages belong to the opening;
  views, links and the read API use the room id, and naming (6.9) counts all
  openings of one id as one room.
- **Restart.** On a server restart, rooms active in the last 10 minutes are
  reloaded from the database with their ids and centers, as live rooms with
  no members; stations rejoin them (design 13). A reloaded room's close timer
  starts at the restart: one no member rejoins closes 10 minutes after it.
- **Folded.** A room whose members are all AWAY, which has been open at
  least 2 hours and in which no turn started in the last 2 hours, is listed
  folded with the quiet rooms (summary `folded` true, design C18).

### 6.4 Joining and leaving

- A slice that enters a live room or a candidate is **joining**. It becomes
  a **member** once it has stayed in that room for 10 s, or at once when its
  `presence` says keyed there (design C2). Until then its row shows
  "joining", it is not counted in `members`, and the station cannot post or
  name there; its readings are dropped silently.
- A station's slice **leaves** when it moves out of the room (6.3), when a
  `presence` no longer lists it, when the station's connection closes, when
  the station sends `hidden`, or when nothing (no message; pongs do not
  count) has come from the station for 30 s (design C18). The connection
  itself stays open in the last case, and the next `presence` places the
  slices again, joining.
- **One station, one row** (design D6, C26). If two of a station's slices are
  in one room, the one listed first in its `presence` is its **counted
  slice** there; the other has no row, and its readings are dropped
  silently.
- **AWAY.** A station whose last `presence` had `atRadio` false is AWAY: its
  rows show it, and it cannot post or name (design D14). It is still a
  member, its readings still count, and its keying still opens turns.
- **Not counted.** A station a helper stopped counting (design 11.7) is
  treated by the turn rules as absent: its readings, presence and keyed
  state do not show live and credit no turn, and are stored marked. The
  station is told by `notice` `not-counted`.

### 6.5 What a station sends

These are normative for a Core and are what other software should do.

- **Presence**: as 5.3.2, on change, and every 10 s while nothing changes.
  The limit's window is half-open (9.1), so a Core spaces its presence
  messages at least 1100 ms apart, which keeps clock jitter from tripping
  it.
- **Callsign and grid**: only values the server accepts, as 4.5 says: no
  connection without a valid callsign, an 8-character grid cut to 6, any
  other grid that is not 4 or 6 characters sent as `null`.
- **Readings**: once a second for each slice that is a member and not keyed.
  A Core sends the slice's `signalStrengthDbm`, only when its
  `meterReadingNs` has advanced since the last one sent, and never while the
  slice is keyed. `t` is the Unix time in ms from the Core's wall clock at
  the moment of reading (design 8.3, R-RT-6).
- **Signal present** (design C3): `present` is true when the reading is at
  least 10 dB above the station's own floor for that slice. The floor is the
  10th percentile of the last 60 s of readings taken while `present` was
  false; while `present` is true the floor holds. After tuning to a new
  frequency `present` stays false for the first 10 s while a floor forms.
  The station decides, because only it knows its own noise.

### 6.6 Turns

The server works out turns per room from presence and readings, on its own
clock. A reading's `t` is used only for its age (6.8).

**Keyed members.** A member is keyed while its counted slice's latest
`presence` says keyed, except a slice reported keyed for 10 unbroken
minutes: from then it is treated as unkeyed until a `presence` reports it
unkeyed, and it can then key again (design C21). The server logs such a long
key.

**Signal present.** A member reports signal present when a reading with
`present` true arrives from it. `L` is the arrival time of the room's latest
such reading.

**In every family except the digital ones:**

- **Talking.** When a member keys and no other member is keyed, a talking
  turn opens credited to it. An open unknown turn ends at that moment.
- **Ending.** When the talker unkeys, its turn shows `ending` true. If the
  same talker keys again within 3 s of unkeying (before 3000 ms have
  passed), the turn continues and `ending` goes back to false. Otherwise the
  turn ends, with its end at the moment it unkeyed (design C4).
- **Doubled.** When a member keys while another member is keyed (or while
  the talking turn of another is ending), the open talking turn ends at that
  moment (if it was ending, at the moment its talker unkeyed) and, if two or
  more members are now keyed, a doubled turn opens listing them in keying
  order. A member keying during a doubled turn is added to it. When only one
  member is left keyed the doubled turn ends at that moment and a talking
  turn for that member opens at the same moment. If none is left keyed, the
  doubled turn ends at that moment (design D10).
- **Unknown.** When a reading with `present` true arrives from a member while
  no member is keyed and no turn is open, an unknown turn opens with no
  talker at that reading's arrival. Present readings while a talking turn is
  ending open nothing.
- **Quiet.** An open unknown turn ends when no member is keyed and no
  present reading has arrived for 3 s (at `L` + 3000 ms); its end is `L`.
- **A talker leaving.** When a talker's row leaves the room for any reason
  (its connection closes or is replaced, 30 s with nothing from the station,
  its slice moves out of the room or is no longer listed, `hidden`), it is
  no longer keyed there, at that moment, with no 3-second wait: a talking
  turn of its that is open and not ending ends at that moment; one already
  ending ends at once with its end at the unkey, as it would have. In a
  doubled turn the talker's leaving counts as its unkeying, and the doubled
  rules above apply at that moment. A station that comes back (a reconnect,
  or its slice tuned back) joins again as a new row, and keying then opens a
  new turn; it never continues the old one.
- A room whose last member leaves ends its open turn at that moment, with no
  wait. Open turns end at a server restart.

**In the digital families (`digl`, `digu`)** there are no doubles (design
C19):

- Each keyed member has its own talking turn, opening when it keys, ending as
  above (with the same 3-second continuation, and at once when its talker
  leaves), whoever else is keyed.
- An unknown turn opens and ends as above, only while no member is keyed and
  no talking turn is open.

**Moments.** At one moment, timers due at or before it fire first, in the
order they fall due, then the messages read at that moment, in the order
read. So a talker keying again exactly 3000 ms after unkeying starts a new
turn, and a slice that entered a room exactly 10 s before a moment is a
member before any message read at that moment is applied.

### 6.7 Readings and turns

- A reading accepted from a member is stored with the turn it belongs to at
  its arrival:
  - in other families, the one open turn (talking, ending, doubled or
    unknown);
  - in digital families, the turn of the only keyed member; with two or more
    members keyed, no turn; with none keyed, the one open turn if there is
    exactly one (ending or unknown), otherwise none.
- A turn that ends at an earlier moment than some of its readings (the
  3-second rules) drops those readings: a reading belongs to a turn only
  when it arrived from its start to its end. So readings taken while a
  talker's turn was ending stay with it only if the turn continued.
- Readings stored with a doubled turn are never shown as either talker's
  reading and never enter either talker's archive (design D10). Readings
  stored with no turn are kept for the station's own record only.
- A reading stamped more than 5 s from the server's clock is taken as
  stamped at its arrival (design 13). A reading's age on every screen is the
  viewer's now less `t`; past 20 s it greys (design U4).
- Storage: one byte a second per station per turn (dBm + 160 in 1 dB steps,
  so -160 to +94 dBm; 255 is no reading that second) and one signal-present
  bit a second, flushed every 30 s and at the turn's end (design 6.3, C11).

### 6.8 What each row shows

- `callsign` with `nth`: two stations with one callsign in one room show as
  `W4XYZ` and `W4XYZ (2)` in joining order (design 7.1); `nth` is that
  number and a screen adds " (2)" when it is over 1. A row keeps its `nth`
  for as long as it stays; rows are never renumbered, so when the first
  `W4XYZ` leaves, `W4XYZ (2)` stays `(2)`, and a row joining later takes the
  lowest number no present row with that callsign holds.
- `km`: the great-circle distance from the row's grid to the talker's grid,
  when the room has exactly one talking turn open (ending included), both
  grids are known, and the row is not the talker's; otherwise `null`. Each
  grid stands for the center of its square (4 characters: 2 degrees of
  longitude by 1 of latitude) or subsquare (6 characters: 5 by 2.5 minutes).
  The distance is the haversine formula on a sphere of radius 6371 km,
  rounded to the nearest whole km.
- `offsetHz`: the row's counted slice frequency less the room's center,
  rounded to the nearest 10 Hz, halves away from zero.
- `reading`: the latest reading accepted from the row's counted slice since
  it entered the room, with `turn` the turn it is stored with (or `null`).
  A screen converts `dbm` to S-units on the app's scale (S9 = -73 dBm, 6 dB
  per S-unit, then S9 plus dB; design 8.5).
- Readings are listed nearest first; rows with no distance follow in
  callsign order. Never by strength (design D3). The server sends rows in
  joining order; ordering for display is the screen's.
- `away`, `joining`, `keyed`, `software`, `softwareVersion` as sent.
  `you` is true on the station's own row in views sent to it.
- A turn's `heard` (it feeds the talker's "9 hear you" line) counts the stations
  whose latest reading stored with that turn (6.7) has `present` true,
  whether or not their rows are still in the room. A station counts once
  whatever its number of readings. A doubled turn's `heard` is always 0,
  since its readings are never shown as either talker's (6.7).

### 6.9 Naming turns

- A station may name an unknown turn while it is a member of that room (any
  opening of the room id), during the turn or within 24 hours after it ends,
  if it was also a member of that room at some time during the turn (design
  C5). An AWAY or blocked station cannot.
- Every name given is kept. A turn's `names` lists each distinct name with
  how many stations gave it, most first, then by name; there is no winner
  (design U9). `mine` is the name the receiving station gave, or `null`.
- A talking or doubled turn cannot be named. Hiding does not stop others
  naming a turn with a hidden station's callsign (design D21).
- Names never move a turn into any station's archive (design C5).
- Screens offer the names used on that frequency in the last 24 hours (by
  base callsign) or a typed one.

### 6.10 Messages and mentions

- **Cleaning** (design 12.4), in this order: CR LF and lone CR become LF;
  every other control character (Unicode category Cc) is removed; these
  invisible and direction-changing format characters are removed: U+061C
  (Arabic letter mark), U+200B (zero width space), U+200E and U+200F
  (direction marks), U+202A to U+202E (direction embeddings and overrides),
  U+2060 (word joiner), U+2066 to U+2069 (direction isolates) and U+FEFF
  (byte order mark); Unicode NFC, after the removals, so the stored text is
  NFC; leading and trailing white space is removed. U+200C and U+200D (zero width non-joiner
  and joiner) stay, since emoji sequences and some scripts need them. Then
  the text must be 1 to 500 code points and at most 10 lines (9 LFs); more is
  `too-long`, nothing is `not-allowed`. The server stores the cleaned text
  and screens clean it again when shown.
- **Formatting**, the only markup: `**bold**`, `*italic*`, and links: a run
  starting `http://` or `https://` up to the next white space or the next
  format character (Unicode category Cf, such as a U+200D left after
  cleaning) is a link, shown with its full address. Everything else is plain text. Screens escape
  all text (design C13).
- A message carries `dbm`: the sender's latest reading in that room, if it is
  stored with a turn open when the message was posted; otherwise `null`
  (design D4).
- `state`: `shown`; `removed` by a helper ("Message removed"); `deleted` by
  its sender ("Deleted by sender"); `expired` once its text is cleared a year
  after posting ("Message removed after a year", design 6.4). `text` is
  `null` unless `shown`.
- `ref` is the sender's row ref when the message was posted in the current
  opening and that row is still in the room; otherwise `null`.
- **Mentions.** A message mentions a station when one of its words, the runs
  of `A-Z a-z 0-9 /` in its text with an optional leading `@`, has the same
  base callsign (4.5) as the station's current callsign, ignoring case. The
  server sends `mention` to every connected station it mentions except the
  sender, and keeps it for the next `welcome` (5.4.2).

### 6.11 Views and generations

- **What a connection views.** A station views every live room one of its
  slices is in (joining or member) and every room it reads. A live reader
  views the band list of its watched band and the rooms it watches. A
  connection receives `room` when a room enters its view and `delta` for
  every change after; a room leaves a station's view without a message when
  the station's own row's ref appears in `left` (the station left it) and it
  does not read it, or when `closed` is true.
- **Summary keys.** `state` is `talking` when a talking turn is open
  (ending included; in digital families, one or more), `doubled` or `unknown`
  when such a turn is open, otherwise `quiet`. `talkers`: the open turns'
  talkers' callsigns (none for unknown). `members`: members, not joining.
  `messages`: messages posted in the current opening. `last`: the latest of
  the room's opening, a row entering or leaving, a reading, a turn opening or
  ending, a message. `folded`: 6.3. `start`: the start of the first open
  turn (the order of the room view's `now`), `null` when none is open.
  `heard`: that turn's `heard`, 0 when none is open. `readings`: for that
  turn, the `dbm` of each member's latest reading stored with it (6.7), the
  turn's talkers left out, nearest first and rows with no `km` after them in
  callsign order (6.8), at most 12; empty when no turn is open.
  `lastMessage`: the room id's newest message in any opening (the room
  view's newest), `null` when it has none; its `text` is the first 80 code
  points of the message's `text`. `ended`: the end of the newest turn that
  ended in the current opening since the server started, `null` when none
  has (a quiet row's "quiet N min" counts from it, otherwise from `last`).
  A change to any of these is a summary change and goes in the room's
  `change` entry.
- **Generations.** Each connection has one generation for its band list and
  one for each room in its view. A full `rooms` or `room` starts its
  generation at 1; each `roomsDelta` or `delta` carries the next. The server
  never skips one while it serves the connection normally; it may drop
  changes to a connection whose outbound queue is full (9.1), and the next
  one it sends then shows a gap. A receiver that sees a gap sends `resync`
  (design 9.3).
- **Pacing.** A room's `delta` is sent at once, one for each message or timer
  that changes the room. Band-list changes are collected for the server, not
  per connection: when no band-list tick has run in the last 2000 ms, a tick
  runs at once; otherwise one runs 2000 ms after the previous tick (design
  C27). A tick builds the band list once and sends every connection that
  views a band list one `roomsDelta` cut from it (its watched band, or all)
  with the changes since what the connection last received; a connection
  with no change gets none. A connection that received `rooms` after the
  previous tick and less than 2000 ms ago gets its changes at the next
  tick, which then runs 2000 ms later even with nothing new; so its first
  `roomsDelta` comes at most 4000 ms after its `rooms`.
- **Order.** For one message or timer, a connection receives, in this order:
  the answer to its own request (`posted`, `error`, `weblinkOk`, `notice`),
  then a `roomsDelta` if one is due, then any `room` views, then `delta`s,
  then `mention`.

## 7. Flows

### 7.1 Sign-in with a station key

```
server  -> station:  {"type":"challenge","nonce":<nonce>}
station -> server:   {"type":"hello","v":1,"software":"NereusSDR","softwareVersion":"26.10.3","spki":<spki>,"sig":<sig over 4.3>}
server  -> station:  {"type":"welcome","station":<id of spki>,"kind":"core",...}
server  -> station:  {"type":"rooms","gen":1,"rooms":[...]}
```

A rendezvous-prefixed signature, a signature naming another host, another
nonce or a flipped bit is answered with `error` `bad-sign-in` and closes the
connection with 4001 (R-RT-2). No `hello` within 10 s closes it with 4005.

### 7.2 Sign-in with a website key

```
server  -> station:  {"type":"challenge","nonce":<nonce>}
station -> server:   {"type":"hello","v":1,"software":"Example Logger","softwareVersion":"3.1","key":<websiteKey>}
server  -> station:  {"type":"welcome","station":<key id>,"kind":"key","callsign":"W4XYZ","grid":"EM73",...}
server  -> station:  {"type":"rooms","gen":1,"rooms":[...]}
```

An unknown key is answered with `error` `bad-sign-in` and closes with 4001;
a revoked key closes with 4004.

### 7.3 Joining a room

```
station -> server:   presence (slice 0 on 14.212 USB)           t = 0     joining a candidate
station -> server:   reading                                     t = 1 s   dropped silently
server  -> station:  roomsDelta add usb-14212000                 t = 10 s  member; the room opens
server  -> station:  room usb-14212000, gen 1
station -> server:   reading, once a second                                each shows as a delta
station -> server:   presence, every 10 s and on change
```

A station joining a room that is already live gets its `room` view at once,
its own row `joining` true, and a `delta` when it becomes a member.

### 7.4 A talking turn

```
A -> server:  presence, slice keyed           turn opens: talking, A
B -> server:  reading                         stored with A's turn; B's row shows it, with km to A
A -> server:  presence, slice unkeyed         ending true
                3 s pass                      the turn ends at A's unkey
```

Each step is a `delta` to every viewer: the turn object in `turns`, the
changed rows in `members`.

### 7.5 An unknown turn and naming it

```
B -> server:  reading, present true           unknown turn opens (no talker)
C -> server:  reading, present true           stored with it
                3 s with no present reading   the turn ends at the last present reading
B -> server:  name, turn, "W4XYZ"             names: W4XYZ by 1
C -> server:  name, turn, "W4XYZ"             names: W4XYZ by 2
```

### 7.6 A double

```
A keyed                 talking A
B keys                  talking A ends; doubled A, B opens
C's readings            stored with the doubled turn, shown under it, never as A's or B's
A unkeys                doubled ends; talking B opens at the same moment
```

In `digl` and `digu` there is no double: A's and B's turns run side by side,
and readings while both are keyed belong to neither (6.6, 6.7).

### 7.7 Resync

```
server  -> station:  roomsDelta gen 3    (the station last saw gen 1)
station -> server:   {"type":"resync","room":null}
server  -> station:  {"type":"rooms","gen":1,"rooms":[...]}
```

The same with a room id for a room's view.

### 7.8 Going hidden

```
station -> server:  {"type":"hidden"}         rows leave at once (delta left to everyone else)
station:            closes the station connection
station -> server:  opens /v1/live, watchBand all, watch the rooms it shows
```

A Core that starts hidden opens only `/v1/live`, and so does a Core whose
first-run question is unanswered (design D25). For the archive while hidden
or unanswered it opens a brief station connection: `hello`, then `weblink`
or `deleteArchive`, never `presence`, then closes (design 10.2).

### 7.9 Web sign-in link

```
station -> server:  {"type":"weblink"}
server  -> station: {"type":"weblinkOk","url":"https://roundtable.nereussdr.com/signin?t=<token>","expires":<now + 5 min>}
browser -> web:     GET /signin?t=<token>   once; sets the session cookie and redirects
```

### 7.10 Deleting the archive

```
station -> server:  {"type":"deleteArchive","confirm":"delete my archive"}
server  -> station: {"type":"notice","code":"archive-deleted","text":"Your Roundtable archive has been deleted. Backups age out within 7 days.","until":null}
```

## 8. Errors

### 8.1 Where errors go

- A refused station or live request gets `error` (5.4.11), or `posted` with
  `ok` false for `post` (5.4.7); the connection stays.
- A protocol error (5.1) closes with 4002 and no message. A refused sign-in
  gets `error` `bad-sign-in` (a bad key, signature or website key) or
  `version` (a version the server does not speak), then closes with 4001.
  A revoked website key closes with 4004 and no message; no `hello` in time
  closes with 4005 and no message. A connection over a cap gets `error`
  `rate` with `about` `null` in place of `challenge`, then close 1013.
- `reason` is the exact text below, which a screen may show as sent. No
  reason names a source, a file or a code.

### 8.2 Codes and reasons

| Code | Reason | Cause |
| --- | --- | --- |
| `not-member` | "Tune to this frequency and stay for 10 seconds before you post or name a turn." | `post`, `name` from a station that is not a member of the room (6.4) |
| `away` | "Nobody is at the radio, so this station can't post or name turns." | `post`, `name` from an AWAY station |
| `hidden` | "This station is hidden, so nothing more is taken from it." | any kind but `weblink` and `deleteArchive` after `hidden` |
| `blocked` | "This station has been blocked from posting and naming turns." | `post`, `name` from a station a helper blocked |
| `rate` | "Too many at once. Try again in a moment." | over a per-station limit (9.1); `retryAfterMs` until allowed |
| `rate` | "Too many connections from this network. Try again shortly." | the per-address-group connection caps; `retryAfterMs` 5000, then close 1013 |
| `rate` | "Roundtable is busy. Try again shortly." | the total live-socket cap; `retryAfterMs` 5000, then close 1013 |
| `too-long` | "A message can be up to 500 characters and 10 lines." | `post` over 500 code points or 10 lines after cleaning |
| `unknown-slice` | "That reading is for a slice this station has not listed." | `reading` for a slice not in the latest `presence` |
| `slice-keyed` | "A slice can't send readings while it is transmitting." | `reading` for a slice whose `presence` says keyed |
| `bad-turn` | "Only a turn with an unknown talker can be named, up to 24 hours after it ends." | `name` on a talking or doubled turn, or too late |
| `not-allowed` | "You can name a turn only if you were on this frequency during it." | `name` from a member who was not a member during the turn |
| `not-allowed` | "You can delete only your own messages." | `delete` of another station's message |
| `not-allowed` | "You can read up to 4 other frequencies at once." | a fifth `read` |
| `not-allowed` | "You can watch up to 4 frequencies at once." | a fifth `watch` |
| `not-allowed` | "To delete your archive, confirm with the exact words asked for." | `deleteArchive` with other words |
| `not-allowed` | "This station needs a callsign before it can send its presence." | `presence` with `callsign` `null` from a Core |
| `not-allowed` | "There is nothing to send." | `post` with nothing left after cleaning |
| `not-found` | "That room, turn or message is not here." | `read`, `watch`, `resync`, `delete`, `name`, `report` naming nothing the server holds or the connection views |
| `version` | "This version of the Roundtable protocol is not supported. Updating the software may help." | `hello` with `v` other than 1; then close 4001 |
| `bad-sign-in` | "Roundtable did not accept this station's key or signature." | `hello` with a key that is not canonical, a signature that does not verify or an unknown website key; then close 4001 |
| `server` | "Roundtable can't save messages right now." | `post` while the database cannot save (design 13) |

A receiver treats a code it does not know as a refusal it cannot act on
further, and shows the reason.

### 8.3 Close codes

Section 2.4.

### 8.4 Notices

| Code | Text | `until` |
| --- | --- | --- |
| `blocked` | "A Roundtable helper has blocked this station from posting and naming turns." | when it ends, or `null` for good |
| `not-counted` | "A Roundtable helper has stopped counting this station's readings and keyed state." | when it ends, or `null` for good |
| `report-closed` | "A report you made has been looked at." | `null` |
| `version-ending` | "This version of Roundtable stops working on the date shown. Updating the software keeps it working." | the date it stops (section 12) |
| `archive-deleted` | "Your Roundtable archive has been deleted. Backups age out within 7 days." | `null` |

## 9. Limits, storage and logs

### 9.1 Limits

Per station unless stated; the station's limits are sent in `welcome`
(design 9.5, C7). Every value is configurable; these are the defaults.

| What | Limit | `limits` key | Over it |
| --- | --- | --- | --- |
| Readings | 2 a second per slice | `readingsPerSecond` | dropped; at most one `error` `rate` a minute |
| Presence | 1 a second | `presencePerSecond` | refused, `error` `rate` |
| Messages | 6 a minute and 60 an hour | `messagesPerMinute`, `messagesPerHour` | refused, `posted` `rate` |
| Message length | 500 Unicode code points and 10 lines | `messageCodePoints`, `messageLines` | refused, `posted` `too-long` |
| Names | 10 a minute | `namesPerMinute` | refused, `error` `rate` |
| Reports | 10 an hour | `reportsPerHour` | refused, `error` `rate` |
| Slices in rooms | 4 | `slices` | a fifth slice in `presence` is a protocol error |
| Rooms read | 4 besides the station's own | `reads` | refused, `error` `not-allowed` |
| Station connections | 1 per station id (a new one replaces the old); 8 per address group | | `error` `rate`, close 1013 |
| Website keys issued | 3 a day per address group | | the key page says so |
| Read API | 60 requests a minute per address group | | `429` (10.6) |
| Live sockets | 4 per address group; 500 in all | | `error` `rate`, close 1013 |
| Rooms watched per live socket | 4 | | refused, `error` `not-allowed` |
| Inbound message | 16384 bytes | | close 1009 |
| Outbound queue per connection | 1024 messages or 4194304 bytes (4 MiB) | | room `delta`s and band-list changes are dropped (a gap, 6.11), except a `delta` with `closed` true and a room's removal from the band list, which are always queued; past twice that the connection is closed with 1000 |

- **Windows.** A limit of N a second, a minute, an hour or a day counts in
  a sliding window of that length on the server's clock: a request is
  refused when N were accepted in the window, with `retryAfterMs` until the
  oldest leaves it. A refused request is not counted. The window is
  half-open: a request at time T counts those accepted after T less the
  window's length, so with 1 a second a presence exactly 1000 ms after the
  last is accepted and one at 999 ms is refused. Senders leave a margin (a
  Core spaces presence at least 1100 ms apart, 6.5).
- **Address groups.** One IPv4 address (an IPv4-mapped IPv6 address counts as
  its IPv4 address), or one IPv6 /64 (design 9.5). Connections are counted
  at connect, before sign-in.
- **The client address** is the peer's, or, when the peer is loopback (Caddy),
  the last address in `X-Forwarded-For`, which the Roundtable Caddy site
  sets. Addresses stay in memory only, never in a file or a log (design
  9.5, 12.3).

### 9.2 Timers

| Timer | Value | Section |
| --- | --- | --- |
| `hello` after connect | 10 s, else close 4005 | 3.1 |
| Ping interval; silence before close | 20 s; 45 s | 2.3 |
| Presence repeat (station side) | 10 s | 6.5 |
| A station silent, its rows leave | 30 s | 6.4 |
| Join dwell | 10 s | 6.4 |
| Same-talker continuation; quiet | 3 s; 3 s | 6.6 |
| Long key | 10 minutes | 6.6 |
| Reading skew | 5 s | 6.7 |
| Earlier turns in a view | 30 minutes | 5.4.5 |
| Messages in a view | 50 | 5.4.5 |
| Naming after a turn ends | 24 hours | 6.9 |
| Room closes after its last member | 10 minutes | 6.3 |
| All-AWAY rooms fold after | 2 hours | 6.3 |
| `roomsDelta` pacing | 2 s | 6.11 |
| Web sign-in link | 5 minutes, once | 5.4.10 |
| Archive session gap (design C18) | 10 minutes | design 4 |
| Message text cleared | 365 days, daily at 04:00 UTC | 9.3 |

### 9.3 Storage

- Live rooms are held in memory; history is written to one SQLite file in
  WAL mode (design 6.3): stations (id, kind, callsign, grid, first and last
  seen, posting block, not counted), keys (the key hash, never the key; no
  address), rooms and openings, turns, turn readings (6.7), turn names,
  messages, reports and the review log, helpers, and archive sessions.
- A turn's reading tracks are flushed every 30 s and at its end, so a
  restart loses at most 30 s of one open turn.
- No address is ever stored. The key-issuance count per address group is
  held in memory only, for 24 hours (a restart resets it).
- A daily job at 04:00 UTC clears the text of every message older than 365
  days (`state` `expired`); the row, its sender, time and reading stay.
  Readings, turns, names and sessions are never deleted, except by
  `deleteArchive` (design 6.4).

### 9.4 Logs

Logs go to standard error only; under systemd that is the journal. A log
line may name an event (signed in, replaced, revoked, a room opened or
closed, a long key, an error code, a database error) with a room id and at
most the first 6 characters of a station id. It never holds an address, a
whole station id, a callsign, message text, a website key or its hash, a
nonce, a signature, a public key or a sign-in token. The websockets
library's own log records, which can carry addresses, are switched off
(design 12.3).

## 10. Read API

### 10.1 General

- `GET` (and `HEAD`) only, over HTTPS, no sign-in. Any other method gets
  `405`. Answers are compact JSON, `Content-Type: application/json;
  charset=utf-8`, `Cache-Control: no-store`, and
  `Access-Control-Allow-Origin: *` so other software's pages may read it.
- Objects are those of section 5.2, as a live reader receives them (`you`
  false, `mine` `null`). Every answer has `time`, the server's clock in
  Unix ms.
- Archives, station ids and addresses are never in it (design D11, 12.1).
- 60 requests a minute per address group (9.1).

### 10.2 Rooms

`GET /v1/api/rooms` and `GET /v1/api/rooms?band=<band>`

```
{"time":1800000004000,"rooms":[<summary>, ...]}
```

Every live room, or those of one band (`band` as 6.2, or `other`), in
ascending order of center frequency. An unknown band is `400`.

### 10.3 One room

`GET /v1/api/room/<family>/<center Hz>`

```
{"time":1800000004000,"live":true,"closed":null,"room":{"id":"usb-14212000","opening":17,"band":"20m","family":"usb","hz":14212000,"members":[...],"now":[...],"earlier":[...],"messages":[...]}}
```

- The live room's view, as `room` (5.4.5) without `type` and `gen`.
- A room id that is not live answers its most recent closed opening, quiet:
  `live` false, `closed` the Unix ms it closed, `members` and `now` empty,
  `earlier` its turns of the 30 minutes before it closed, `messages` the last
  50 of the room id. A room id that has existed never answers "not found"
  (design 8.1).
- A room id that has never existed is `404`; a family or frequency that is
  not a valid room id is `400`.

### 10.4 Older messages

`GET /v1/api/room/<family>/<center Hz>/messages?before=<id>&limit=<n>`

```
{"time":1800000004000,"messages":[<message>, ...]}
```

- Messages of every opening of the room id, newest first, with ids less
  than `before` (all when `before` is absent), at most `limit` (1 to 50,
  default 50). `before` and `limit` are whole numbers in decimal; anything
  else is `400`. A room id that has never existed is `404`.

### 10.5 A callsign

`GET /v1/api/callsign/<callsign>`

```
{"time":1800000004000,"callsign":"W4XYZ","rows":[{"callsign":"W4XYZ/P","room":"usb-14212000","ref":"w2c6tz7e"},{"callsign":"W4XYZ","room":"lsb-7188000","ref":"q7h2m4ka"}]}
```

- The live rows whose callsign has the same base callsign as the one asked
  for (4.5): one flat entry per row in a live room, members and joining,
  each with the row's `callsign`, its `room` and its `ref`. Rows are ordered
  by room id, then ref. Nothing says which rows belong to one station, so
  the answer never links refs across rooms (design 7.1, 9.4).
- `callsign` is the asked-for callsign as the server normalizes it. A
  string that is not a callsign is `400`.
- A path segment cannot hold `/`, so a callsign with a prefix or suffix is
  sent with each `/` written as `%2F` (`/v1/api/callsign/VE3%2FW4XYZ`). Since
  rows are matched on the base callsign, asking for the base callsign alone
  (`/v1/api/callsign/W4XYZ`) gives the same answer, and clients may do that
  instead. An unencoded `/` in the path is `404`.

### 10.6 Errors

`{"error":<code>,"reason":<reason>}` with the status:

| Status | `error` | `reason` |
| --- | --- | --- |
| 400 | `bad-request` | "That request could not be read." |
| 404 | `not-found` | "That room, turn or message is not here." |
| 405 | `not-allowed` | "Only GET and HEAD are supported." |
| 429 | `rate` | "Too many at once. Try again in a moment." (with `Retry-After` in whole seconds, at least 1) |
| 503 | `server` | "Roundtable can't answer right now." |

## 11. Conformance

`roundtable/conformance/v1/` is the machine-readable half of this document.
It follows the rendezvous suite (rendezvous section 10) wherever that fits.
Runners: the server's (`roundtable/tests/`), the Core's (`RoundtableClient`
tests) and other software's own. The fixtures are published with the API
page (design 9.7) so other software can test against them.

### 11.1 The files

- `manifest.json`: `{"roundtableVersions":[1],"fixtures":[{"id","file","kind"}]}`,
  `kind` one of `crypto`, `control`, `session` and `turns`. Every file under
  `crypto/`, `control/`, `sessions/` and `turns/` is listed once; ids are
  unique.
- `crypto/*.json`: derivation and signature vectors (11.2).
- `control/*.json`: one message each (11.3).
- `sessions/*.json`: one scripted exchange each (11.4).
- `turns/*.json`: one scripted room each, checked against the turn rules
  (11.5).

`crypto/` holds two fixed test P-256 keys, each with its private scalar
(`privateKeyHex`, 32 bytes big-endian) so a runner can sign with it in its
own tests. They were generated only to produce these vectors and are used by
nothing else: no Core, station, server or website key holds them, and they
protect nothing. Every key that signs during a session fixture is made at
run time.

### 11.2 Crypto vectors

| File | Holds | A runner checks |
| --- | --- | --- |
| `base64url.json` | the rendezvous suite's strict base64url cases | its decoder accepts exactly the valid texts, to those bytes |
| `p256-spki.json` | the rendezvous suite's SPKI cases | its key check accepts exactly the canonical key |
| `roundtable-id.json` | `prefixHex`, `rendezvousPrefixHex`, and per key: `privateKeyHex`, `publicKey`, `spkiHex`, `digestHex`, `id`, `rendezvousDigestHex`, `rendezvousId` | SHA-256 of the prefix and the key's DER is `digestHex` and the Roundtable id is `id` (4.1); the rendezvous id is `rendezvousId` and differs from `id` |
| `key-id.json` | `prefixHex` and `cases`: `{"key","keyHex","digestHex","id","keyHashHex","valid":true}` or `{"key","valid":false,"why"}` | each valid key decodes to `keyHex`, its key id is `id` (4.2) and its stored hash is `keyHashHex`; each invalid key text is refused |
| `hello-signature.json` | `key` and `otherKey` (`fixed`, `privateKeyHex`, `publicKey`, and `id` for `key`), `host`, `nonce`, `transcriptHex`, and `cases`: `{"name","publicKey","signature","valid","why"}` | the transcript of `host` and `nonce` is `transcriptHex` (4.3); a case is valid exactly when its key is canonical and its signature, strict base64url of 64 raw bytes, verifies over the transcript: `signed` and its high-s twin are; the rendezvous registration transcript, the right host and nonce under a rendezvous prefix, another host, the host with a port, no host, another nonce, the nonce's text, a flipped bit, another key, a key claimed for another's signature, DER, padding, 63 bytes, `r` of 0, `r` of n and `s` of n are not |

### 11.3 Control fixtures

`{"from":"station"|"live"|"server","to":"station"|"live","wire":{<the message>},"decodes":true|false}`.
`to` is present only when `from` is `"server"`. A fixture whose message
cannot be written as a JSON value (a duplicate key, `NaN`, a top level that
is not an object) gives it as text in `wireText` instead of `wire`.

- The receiving end decodes `wire` (or `wireText`). When `decodes` is true it
  encodes the result again and compares it with `wire` as JSON values, key
  order ignored, on the keys the kind and its objects list only.
- The server's runner decodes every fixture with the server's decoder for
  that direction, and also sends each refusal from a station or live reader
  to the running server, which must close the connection with 4002 (from a
  station, after signing in). A station's runner decodes the fixtures `to`
  `"station"` and encodes those `from` `"station"`; a live reader's does the
  same for `"live"`.
- `decodes` false on a message from the server means the receiver logs and
  ignores it (5.1), and carries on.

The fixtures cover every kind in each direction it travels, each with at
least one accepted and one refused message: the 14 station kinds, the 11
server kinds to a station, the 4 live kinds and the 5 server kinds to a live
reader; messages carrying keys and limits no kind lists, which decode;
unknown codes in `error`, `posted` and `notice`, which decode; and unknown
kinds, kinds of the other path, a server kind sent by a station, a `type`
that is a number or missing, duplicate keys, `NaN` and a top level that is
an array, which are refused.

The 16384-byte inbound cap is enforced before decoding and a lone surrogate
cannot be written portably in a JSON file, so the server's runner checks
those against the running server itself.

### 11.4 Session fixtures

`{"runs":[<runners>],"setup":{...},"steps":[<steps>]}`. No other key is
allowed.

**Runners.** `"service"` (the server's runner, which plays every
connection against the real server built from `setup`) and `"core"` (the
Core's runner, which plays the server towards its Core on the connection
named `station`). A fixture that runs `core` only holds server messages the
real server would not send in that order (a skipped generation).

**Connections.** A step names a connection: `station`, `station2`, ... on
`/v1/station`; `live`, `live2`, ... on `/v1/live`. A core runner plays only
`station`.

**Steps.** One of:

- `{"connect":"<conn>"}` or `{"connect":"<conn>","address":"<ip>"}`: the
  connection opens; the server's runner sends `address` (default
  `192.0.2.1`) as `X-Forwarded-For`.
- `{"from":"server","to":"<conn>","message":{...}}`: the server sends this
  to that connection. The server's runner matches it against the next
  message the connection received; a core runner sends it, filled, when
  `to` is `station`.
- `{"from":"<conn>","role":"behaviour"|"scripted","message":{...}}`: the
  connection sends this. A behaviour message is one the station under test
  must produce (a core runner drives its Core to it and matches what it
  sends); a scripted one is a message a conformant Core would not send then,
  which a core runner skips. The server's runner sends both, filled.
- `{"advanceMs":N}`: time moves N ms. The server's runner waits until the
  server has read everything sent so far, then moves the server's clock,
  firing its timers in order (6.6, "Moments").
- `{"disconnect":"<conn>"}`: that end closes its connection.
- `{"expectClosed":"<conn>","code":<close code>}`: the server has closed the
  connection with that code, with no message between the last one listed
  and the close.
- `{"expectSilent":"<conn>"}`: nothing more has arrived at that connection
  (the server's runner waits until the server has read everything and
  emptied its queues). A core runner checks instead that its Core sends
  nothing within 1 s of real time.

**`setup`**, absent keys taking these defaults:

| Key | Default |
| --- | --- |
| `host` | `"roundtable.conformance.invalid"`: the server's host name, signed in `hello` |
| `wallClock` | 1800000000000: the Unix ms when the fixture starts; it moves only with `advanceMs` |
| `keys` | `[]`: website keys issued before the fixture, each `{"name","callsign","grid","revoked"}` (`revoked` default false); the runner makes each key at run time |
| `limits` | `{}`: overrides of the `limits` values of 9.1 |

**Placeholders.** As the rendezvous suite's (rendezvous section 10.4), from
link section 16.1: `"$any"`, `"$string"`, `"$string:<name>"`, `"$int"`,
`"$int:<name>"`, `"$capture:<name>"` and `"$ref:<name>"` (equal to the value
recorded under `<name>`). Roundtable's own:

| Placeholder | Fill | Match |
| --- | --- | --- |
| `"$b64:<n>:<name>"` | `<n>` random bytes, base64url; recorded | strict base64url of exactly `<n>` bytes; recorded |
| `"$key:<k>:spki"`, `"$key:<k>:id"` | the base64url SPKI, or the Roundtable id, of station key `<k>` (made at run time) | equal to that value |
| `"$hello:<k>:<nonce>:<case>"` | a signature by station key `<k>` over the transcript of 4.3 with `setup` `host` and the nonce recorded as `<nonce>`; `<case>` is `signed`, `otherNonce` (a random nonce), `otherHost` (`roundtable.example.invalid`), `rendezvousPrefix` (the rendezvous registration transcript of the nonce) or `flippedBit` (the last bit of `s` flipped) | for `signed`, a signature that verifies so |
| `"$websiteKey:<w>"`, `"$websiteKey:<w>:id"` | the key `<w>` of `setup` `keys`, or its key id | equal to that value |
| `"$rowRef:<name>"` | | exactly 8 characters of `a-z 2-7`; recorded |
| `"$weblink:<name>"` | | `https://` and `setup` `host`, then `/signin?t=` and strict base64url of 32 bytes; recorded |

A core runner's station key is its Core's: at the behaviour `hello`,
`"$key:<k>:spki"` records `<k>` as the key the Core sent, and
`"$hello:<k>:<nonce>:signed"` checks the Core's signature verifies.

**The fixtures.**

| Fixture | Runs | What it holds |
| --- | --- | --- |
| `signin-spki` | service, core | challenge, `hello` with a station key, `welcome`, `rooms` |
| `signin-key` | service | sign-in with a website key: `welcome` `kind` `key` with the key page's callsign and grid |
| `signin-key-revoked` | service | a revoked key: close 4004 |
| `signin-key-unknown` | service | a key never issued: `error` `bad-sign-in`, close 4001 |
| `signin-rendezvous-prefix`, `signin-other-host`, `signin-other-nonce`, `signin-flipped-bit` | service | each bad signature: `error` `bad-sign-in`, close 4001 |
| `signin-version` | service | `v` 2: `error` `version`, close 4001 |
| `signin-timeout` | service | nothing at 9999 ms; close 4005 at 10000 ms |
| `connection-cap` | service | 8 station connections from one address each get `challenge`; the 9th gets `error` `rate` (`about` `null`, `retryAfterMs` 5000) and close 1013; another address still gets `challenge` |
| `signin-presence-first` | service | `presence` before `hello`: close 4002 |
| `signin-replaced` | service | the same key on a second connection: `welcome` there, close 4003 on the first |
| `unknown-kind-closes` | service | an unknown kind after sign-in: close 4002 |
| `reading-while-joining-dropped` | service | a reading from a joining slice: dropped, nothing sent |
| `reading-unknown-slice` | service | `error` `unknown-slice` |
| `reading-keyed-slice` | service | the room opens at 10 s; keying opens a talking turn (a `delta`); a reading from the keyed slice: `error` `slice-keyed` |
| `post-not-member` | service | a post from a joining station: `posted` `not-member` |
| `post-away` | service | an AWAY member's post: `posted` `away` |
| `post-cleaned` | service | a post with a zero width space, a direction override inside a link, CR LF, a control character and a kept zero width joiner: stored and sent cleaned (6.10) |
| `post-accepted` | service | `posted` ok, the message `delta`, and the band-list change, with the room's new `lastMessage`, held to the next 2 s tick |
| `hidden-ends-presence` | service | `hidden`: the row leaves a live reader's view at once; nothing more to the station; then `presence` gets `error` `hidden` |
| `resync` | service | `resync` answers a full `rooms` with `gen` 1; a room not in view is `not-found` |
| `resync-after-gap` | core | a skipped generation: the Core sends `resync` and takes the full list |
| `read-room` | service | a second station reads a room it is not in, unreads it, and reads one that is not live (`not-found`) |
| `weblink` | service, core | `weblinkOk` with a link expiring in 5 minutes |
| `delete-archive` | service | other words: `not-allowed`; the exact words: `notice` `archive-deleted` |
| `live-watch-not-found` | service | a live reader's band list, a watch of no live room (`not-found`), a station kind on live (close 4002) |

### 11.5 Turn fixtures

`{"runs":["service"],"setup":{"stations":[...]},"steps":[...]}`. No other key
is allowed. They hold the rules of 6.4, 6.6 and 6.7 (R-RT-4, R-RT-5).

- `setup` holds `stations`, each `{"name","callsign","grid"}` (`name` an
  upper-case letter), and may hold `host` and `wallClock` as 11.4. The
  runner signs each station in with a key it makes, at time 0, and sends
  nothing else for it but the steps.
- Steps, one key each:
  - `{"presence":{"station":"A","atRadio":true,"slices":[<slice objects>]}}`:
    station A sends `presence` with its `setup` callsign and grid, these
    slices and `atRadio` (default true).
  - `{"reading":{"station":"B","slice":0,"dbm":-91.5,"present":true}}`:
    station B sends `reading`, `t` the current wall clock.
  - `{"advanceMs":N}`: as 11.4.
  - `{"expectMembers":{"A":"member","B":"joining","C":"absent"}}`: each named
    station's counted slice is a member of a live room, joining one (live or
    candidate), or in none.
  - `{"expectTurns":[<turn>, ...]}`: every turn the server holds for the
    fixture's rooms, open and ended, in order of start, then of talker name.
    Each turn is `{"kind":"talking","talker":"A","startMs":10000,"endMs":null}`,
    `{"kind":"unknown","talker":null,...}` or
    `{"kind":"doubled","talkers":["A","B"],...}`, with `startMs` and `endMs`
    in ms from the fixture's start (`endMs` `null` while open), and
    optionally `ending` (6.6) and `readings`, a map from station name to the
    number of readings stored with the turn (6.7; a station not in the map
    has none). An optional key absent is not checked.
- The server's runner reads turns and membership from the server's own
  state through a test hook, not from the wire.

| Fixture | What it holds |
| --- | --- |
| `talking` | a talking turn, its readings (present or not), `ending` on unkey, and its end 3 s later at the unkey |
| `unknown-then-talking` | an unknown turn ended by a member keying; the talking turn starts at that moment |
| `quiet-after-3s` | an unknown turn open at 2999 ms after the last present reading and ended at 3000 ms, its end at that reading; a present-false reading after it dropped; a second unknown turn |
| `same-talker-continues` | a rekey at 2999 ms continues the turn, keeping the readings taken while ending; a rekey at 3000 ms starts a new turn |
| `waiting-reading-dropped` | a reading while ending dropped when the turn ends; it opens no unknown turn; the next present reading does |
| `doubled` | talking A, then doubled A and B with readings kept out of both, then talking B |
| `long-key` | a key held 10 minutes treated as unkeyed, ending at 10 minutes; still-keyed presence opens nothing; unkey and key again opens a new turn |
| `digital` | in `digu`, two keyed members each with their own turn, readings while both are keyed in neither, readings with one keyed in its turn, and an unknown turn after |
| `join-dwell` | joining until 10 s, readings while joining dropped, keying during the dwell making a member at once |
| `two-slices-one-room` | a station's second slice in one room: its readings dropped |
| `talker-leaves` | a keyed talker's slice no longer listed ends its turn at that moment; coming back and keying opens a new turn; 30 s of silence from a keyed talker ends its turn when its row leaves |
| `key-in-candidate` | keying in a candidate room makes the slice a member at once, opening the room and a talking turn; readings while joining dropped |

### 11.6 Running the server's runner

```
python3 -m pytest roundtable/tests -q -p no:cacheprovider
```

The authoritative run is Ubuntu 24.04's Python 3.12 with apt
`python3-websockets` and `python3-cryptography`, which is also what CI
installs; it also runs on macOS's Python 3.9 with websockets 15.
`roundtable/server/nereus_roundtable/transport.py` isolates the websockets
differences, as the rendezvous's does.

## 12. Versions and changing the protocol

- **This is version 1**, on the `/v1/` paths, `hello` `v` 1. A later version
  adds new paths (`/v2/...`); `/v1/` keeps working for at least six months
  after its successor ships (design C8). The dates are on the API page, and
  stations still on the older version get `notice` `version-ending` with the
  date in `until`.
- **The document, the fixtures and the server change together**, in the same
  change, and the server's runner passes. A change to a kind a Core uses
  reaches the Core's runner before it ships.
- **Version 1 adds, never changes.** The server may add keys to its
  messages and objects, new kinds, new codes and new limit keys, relying on
  receivers ignoring what they do not know (5.1); the extra-key control
  fixtures hold every runner to that. It never changes or removes a key, a
  kind, a value's meaning, a code or a reason text version 1 defines.
- **A station sends only what version 1 lists** to a version 1 server; the
  server closes on a kind it does not know.
- **Nothing sensitive stored or logged** (9.3, 9.4) is part of the wire's
  contract, not an implementation detail.
- The API page, `nereussdr.com/roundtable/api.html`, is generated from this
  document by `docs/manual/build_preview.py`, and publishes
  `roundtable/conformance/v1/` beside it (design 9.7, R-RT-25).
