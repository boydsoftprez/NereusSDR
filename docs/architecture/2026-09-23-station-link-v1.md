# Station link version 1

This is the written specification of the link between a NereusSDR station
(the Core, `nereusd`) and a client (the desktop remote window, the iPhone
app). It is requirement R-IOS-01 of the
[iPhone app plan](2026-09-23-iphone-app-plan.md). Both clients implement
from this document, and the conformance suite in `tests/data/link/v1/`
holds both of them to it.

Every value here is a claim about the station's code, read from the source
named beside it. The tables are not written by hand:
`scripts/render-link-tables.py` renders them from
`tests/data/link/v1/surface.json`, which `tst_link_surface_manifest`
captures from the code and guards against drift (section 16). CI runs
`python3 scripts/render-link-tables.py --check` in the compliance job, so a
document that no longer matches the surface fails the build.

Section numbers are stable. Later revisions add sections; they never
renumber.

## 1. The link and its normative parts

The link is one control connection per client: a TLS WebSocket carrying
JSON messages, or, for a device the rendezvous introduced, a data channel
carrying the same messages (section 20). Media (display rows and audio)
travels on a separate encrypted media connection that the control
connection negotiates.

This document states the connection-level rules itself (transport,
identity, the connect sequence, versions, the envelope of every message,
liveness and ending) and indexes the parts that are specified in their own
files. Each of those files is normative for its part and is still
maintained there:

| Part | Document |
| --- | --- |
| Media control (`media.control` operations, the media peer, display subscriptions, receiver audio, the headphones mix, the audio clock) | [2026-09-20-remote-media-control-v1.md](2026-09-20-remote-media-control-v1.md) |
| Display codec (the NSDC v1 frames on the media connection) | [2026-09-20-display-codec-v1.md](2026-09-20-display-codec-v1.md) |
| Display extras (the subscription fields that ask the Core for the peak blobs, peak hold, noise floor, waterfall levels, normalise, calibration and averaging, and the NSDX v1 datagram beside each NSDC frame) | [2026-09-23-display-extras-v1.md](2026-09-23-display-extras-v1.md) |
| Notch control (the `notches` object and the `notch.*` commands) | [2026-09-23-remote-notch-control-v1.md](2026-09-23-remote-notch-control-v1.md) |
| Accessory control (the `tuner`, `amplifier`, `rfkit`, `stationTci`, `accessoryData` and `accessorySettings` objects, the 4O3A, RF-Kit, station TCI, accessory record and device settings commands and refusals) | [2026-09-23-remote-accessory-control-v1.md](2026-09-23-remote-accessory-control-v1.md) |

The design authority behind all of them is the
[remote daemon architecture design](2026-07-28-remote-daemon-architecture-design.md),
the [station identity and pairing design](2026-08-02-remote-station-identity-and-pairing-design.md)
and the [R3 plan](2026-09-20-remote-daemon-r3-plan.md).

## 2. Transport

- The station listens with a `QWebSocketServer` in secure mode
  (`StationServer.cpp`, `listen()`), so every connection is a WebSocket
  over TLS (`wss://`). The server starts from Qt's default TLS
  configuration (`QSslConfiguration::defaultConfiguration()`) and sets the
  minimum explicitly: `setProtocol(QSsl::TlsV1_2OrLater)`, TLS 1.2 or later,
  whatever Qt's default becomes. `tst_link_version` reads it back from the
  listener (`StationServer::tlsConfiguration()`), and offers the listener a
  client that speaks only TLS 1.1 (at OpenSSL security level 0): the
  station refuses it and serves a TLS 1.2 client. The test first shows
  that same client finishing a TLS 1.1 handshake with a listener of its
  own, which differs from the station's in two settings (protocol TLS 1.0
  or later, and OpenSSL security level 0), so the client can speak TLS
  1.1. What the test does not show is which of the station's two settings
  refuses it: OpenSSL 3 at its default security level refuses TLS 1.0 and
  1.1 whatever the protocol setting says, so on such a build either the
  explicit minimum or the library's default would refuse this client. The
  station does not ask for
  a client certificate (`setPeerVerifyMode(QSslSocket::VerifyNone)`); a
  client proves who it is with its own device key (section 3.5) or, on a
  Core upgraded from before paired devices, the token (section 3.3).
- The control port is set by `remote_port` in `nereusd.conf`
  (`DaemonConfig.h`). With neither `remote_port` nor `remote_bind` in the
  file (or no file), the station listens on TCP 47910
  (`DaemonConfig::kDefaultRemotePort`) on every interface, IPv4 and IPv6
  (`remote_bind` empty: `QHostAddress::Any`, `DaemonApp::listenerAddressFor`),
  and announces itself (section 14). A file that sets either key keeps the
  meaning it had before this default: the key it leaves out is 0 (off) or
  `127.0.0.1` (`kExplicitConfigRemotePort`, `kExplicitConfigRemoteBind`), and
  a `remote_port` that is not a number leaves the listener off.
  `remote_port = 0` turns the listener and the announcement off.
- The opening request (the HTTP upgrade) is read by the station before
  Qt sees it (`StationOpeningGate`, in front of the `QWebSocketServer`).
  The station routes nothing by `Host`, so it accepts any `Host` in these
  forms: an IPv6 address in brackets, with or without a port
  (`[2001:db8::1]:47910`, `[::1]`); an IPv6 address without brackets, with
  or without a port (`2001:db8::1:47910`, `::1`, what Apple's
  `NWProtocolWebSocket` sends for an IPv6 URL); an IPv4 address with or
  without a port; and a host name (labels of 1 to 63 letters, digits, `-`
  and `_`, split by single dots, none starting or ending with `-`), with or
  without a port. An IPv6 zone (`%en0`) is dropped. A value
  without brackets that reads both as an address and as an address and a
  port (`::1:8080`) is taken as an address; the station reads nothing from
  it either way. Qt itself cannot read an unbracketed IPv6 `Host` and
  answers such a request with nothing, so the station writes the `Host`
  back in brackets before handing the request on.
- A request the station cannot read gets `400 Bad Request` with
  `Connection: close` and a one-line plain text body, and the connection
  closes: no `Host`, more than one `Host`, a `Host` in none of the forms
  above (or one Qt's URL parser finds no host in), a request line other
  than `GET <path> HTTP/1.1`, a header line that is not `name: value`
  with a token name (a folded line, one starting with a space or tab,
  included), a control character other than a tab in a value, more than
  100 header lines, a first `Upgrade` header that is not exactly
  `websocket`, a first `Connection` header without `Upgrade`, a first
  `Sec-WebSocket-Key` that is not 16 bytes, or a `Sec-WebSocket-Version`
  that is missing or is not a comma-separated list of numbers. A head
  longer than 8192 bytes (`kMaxRequestHeadBytes`) gets the same. These
  follow how Qt reads the request, so that each request the station
  passes on is one Qt answers. A `Sec-WebSocket-Version` number the
  station does not speak (`8`) is Qt's to answer: its own `400`, then the
  close. If Qt closes a request without writing anything, for any other
  reason, the station writes the same `400` first. No complete opening
  request ends without an answer; one never finished is closed at the
  deadline below.
- The whole opening, from the TCP accept through TLS to the `101`, must
  finish within 10 s (`StationServer::kDefaultOpeningDeadlineMs`, Qt's own
  default handshake timeout); a connection that has not opened by then is
  closed. `tst_station_ws_host` sends each form over TLS to the real
  listener.
- Messages travel as WebSocket text frames. Each text message is one JSON
  object, encoded compactly, with a string `type` key naming its kind
  (`SessionMessages::encode`). The station ignores binary frames: only
  `textMessageReceived` is connected (`SessionTransport.cpp`).
- Heartbeats are WebSocket ping and pong control frames, not JSON messages
  (section 12.1). The ping payload is the constant `nereus` and is never
  inspected.
- The station closes a connection with close code 1000 (normal) and a
  reason text; the reason a client acts on always arrives first in a
  `session.end` or `auth.result` message (section 12.4).

## 3. Station identity

### 3.1 The certificate

The station makes its own certificate the first time it runs and keeps it
(`CertificateStore.cpp`):

- self-signed, subject and issuer common name `nereusd`;
- an RSA key of 3072 bits (`kRsaKeyBits`), signed with SHA-256
  (`X509_sign(..., EVP_sha256())`);
- not before: one day before creation (`kNotBeforeSkewSeconds`,
  `-60 * 60 * 24` seconds);
- not after: `60 * 60 * 24 * 365 * 10` seconds after creation, which is
  3650 days (`kValiditySeconds`, "10 years"); both are offsets from the
  moment of creation (`X509_gmtime_adj`), so the whole validity span is
  3651 days;
- a random positive 63-bit serial number.

There is no renewal. A new certificate changes the pin, and every paired
client has to pair again.

### 3.2 The pin

The pin is the SHA-256 digest of the whole certificate in DER form,
written as 32 uppercase hexadecimal byte pairs joined by colons: 95
characters, for example `AB:12:...:FF` (`CertificateStore::fingerprintSha256()`,
`formatFingerprint` in both `CertificateStore.cpp` and `StationClient.cpp`).
It is what `openssl x509 -fingerprint -sha256` prints without its label.

A client compares the pin with the certificate the station presents before
it sends its token. The desktop client compares case-insensitively (it
upper-cases the pin it holds, `StationClient.cpp`), refuses to dial with
no pin unless its operator explicitly allowed an unpinned link, and refuses
a pinned link whose address carries no TLS. The station prints the pin with the
token at first run; the LAN announcement carries it too (section 14).

### 3.3 The token

- A station no longer creates a token. A Core upgraded from before paired
  devices keeps the one it has in `station-token` in its profile directory:
  32 random bytes (256 bits), written as base64url without padding, 43
  characters (`TokenStore.cpp`). The token is loaded, never generated.
- While it is active the Core counts as claimed (`DeviceStore::isClaimed`),
  so no stranger can pair with it, and windows that sign in with it keep
  working. A window that sends its `device` block with the token enrols its
  key in the same step (section 3.5) and signs in by key from then on.
- Retiring the token (`TokenStore::retire`: the file is deleted) ends it for
  good. A paired device retires it with `station.retireToken` (section 9.1),
  which the Core refuses until at least one device is paired; the Core then
  ends every connection still signed in by token, including one that also
  enrolled its device key (section 12.4). On a Core with no token (a new one, or one whose token was retired)
  a sign-in with a token, empty or not, is refused with `auth.result`
  accepted false, "This Core uses paired devices. Pair this device first.",
  `retryable` false, `code` `pairingRequired`, and the token's limiter is
  not consulted.
- After 5 consecutive failed checks (`kDefaultMaxFailuresPerLockout`) the
  station refuses every token check, including a correct token, for 60 s
  (`kDefaultLockoutMs` 60000). That lockout is global for the token, not per
  peer; it never refuses a device key (section 3.5).
- The station never logs a candidate token.

### 3.4 The identity key

The Core has its own identity key, separate from the TLS key
(`StationIdentity`, R-IOS-08; the pairing design, section 3.1):

- ECDSA P-256, created on the first start and kept in
  `station-identity.pem` (PKCS#8, PEM) in the Core's profile directory, mode
  0600, written atomically; reused on every later start. A file that exists
  but does not hold a P-256 private key is refused and never replaced, and
  the Core then does not listen. The first start prints the key file's path
  and the TLS pin to standard output (not the log), with the prompt to back
  the file up: losing it means every paired device pairs again
  (`StationServer::formatFirstRunBanner`).
- A public key travels as base64url, without padding, of its
  SubjectPublicKeyInfo DER: 91 bytes for a P-256 key with its point
  uncompressed. A key's fingerprint is SHA-256 of that DER. A signature is
  ECDSA P-256 over SHA-256, raw `r || s`, 64 bytes, base64url. The station
  accepts only a canonical 91-byte P-256 key (`StationIdentity::isP256Spki`)
  and strict base64url (no padding, only `A-Z a-z 0-9 - _`, the unused bits
  of the last character zero).
- The certificate binding is the identity key's signature over
  `"NereusSDR cert-binding v1\n" || SHA-256(TLS certificate DER)`
  (`StationIdentity::certBinding`). The SHA-256 is the pin's 32 bytes
  (section 3.2).
- The station's `hello` carries `identity`
  `{"publicKey": <key>, "certBinding": <binding>}` and `challenge`, 32
  random bytes from the operating system's generator, base64url, new for
  every connection (`DeviceAuthenticator::newChallenge`), and declares
  `features.deviceAuth` 1. A device that holds the Core's key checks the
  binding against the certificate its connection presents before it signs
  anything; a mismatch is the client's own `identityChanged` (section
  12.4).

### 3.5 Device sign-in

Each paired device holds its own P-256 key. The Core keeps the paired
devices in `paired-devices.json` in its profile directory, mode 0600,
written atomically (`DeviceStore`): each device's `id` (the fingerprint of
its key), key, name, kind (`phone`, `tablet` or `computer`), when it was
paired and last seen, its last address, whether it was enrolled through
the token, and its short name (below; `""` until a sign-in brings one, and
absent from a list written before it). A file that cannot be read fails closed: no device signs in, the
file is never overwritten, and the Core counts as claimed.

A device signs in with `auth.request`, `token` `""`, and `device`
`{"id", "publicKey", "name", "kind", "signature"}` (all strings;
`SessionDeviceBlock`), plus an optional string `shortName`, where
`signature` is its key's signature over the transcript
(`DeviceAuthenticator::transcript`):

```
"NereusSDR device-auth v1\n" || challenge (32 bytes)
  || SHA-256(TLS certificate DER) || SHA-256(Core identity SPKI DER)
  || SHA-256(device SPKI DER)
```

The station admits it (`DeviceAuthenticator::verify`) only when the block
is well formed, the `id` is the fingerprint of `publicKey`, the signature
verifies over this connection's challenge and this Core's certificate and
key, and the device is in the paired devices with that key. A client sends
`device` only to a station whose `hello` declares `deviceAuth` 1.

**The short name.** `shortName` is the device's own short name, for the
places a screen has room for one word; a client sends it at every sign-in,
with no gate. It is at most 32 bytes of UTF-8 (`shortNameMaxBytes`,
`DeviceStore::kMaxShortNameBytes`), counted as the 64-byte name cap is, and
it is validated as a name is (`DeviceStore::isValidShortName`: not empty
after trimming, no control, format, line-separator or paragraph-separator
characters, valid UTF-8). It is the operator's own words, so it is not held
to the Core's wording rules: "Grant's iPhone" passes. It sits outside the
signed transcript, as `name` does. The Core stores it with the device and
replaces it at each sign-in that carries a usable one; an absent or
unusable one changes nothing and refuses nothing. When present it must be a
string, or the message is malformed. Where the Core sends a device's name
and short name (the `devices` and `connectedDevices` objects, section 7.1)
it numbers them on collisions and gives a device with no usable short name
its kind's word ("Phone", "Tablet", "Computer"). A token sign-in that enrols its key
(below) stores it too. The desktop sends the computer's short host name,
trimmed to the cap (`ClientDeviceIdentity::machineShortName`). Pairing does
not carry it: `pair.start`'s `device` block and the code-mode box keep
`{"publicKey", "name", "kind"}`, and a device signs in straight after
pairing.

**The name at sign-in** (slice control plan Task 8b). The Core also
replaces the device's stored `name` at every sign-in whose `name` is
usable (`DeviceStore::isValidName`, at most 64 bytes of UTF-8, validated
as above); an unusable one changes nothing and refuses nothing. So a
device's name on the Core is the one it last signed in with, for a device
paired by code or by a pairing request as much as for one that enrolled
through the token: a phone's name follows what it sends at sign-in, and
renaming the phone renames it on the Core at its next sign-in. The desktop
names a window run with a profile other than the default "<host>
(<profile>)", its name and short name both
(`ClientDeviceIdentity::machineName(profile)` and
`machineShortName(profile)`), so two profiles on one computer, which are
two devices with two keys, are told apart; the default profile's names are
unchanged.

| Case | `auth.result` reason | `retryable` | `code` |
| --- | --- | --- | --- |
| Admitted | `""`, accepted true | false | none |
| A well-formed proof from a key the Core has not paired | "This device is not paired with this Core. Pair it first." | false | `deviceNotPaired` |
| A bad signature, one over another connection's challenge, one binding another certificate or another Core's key, an `id` that is not its key's fingerprint, a malformed block | "This device could not prove it is paired with this Core." | false | `deviceProofFailed` |
| Rate limited (below) | "The Core is refusing sign-ins from this device for a while after too many failed ones. Try again later." | true | none |

Each refusal is followed by the close, as for a token (section 12.4).

**Rate limits.** Failed device sign-ins are counted per source address and
per device id: 10 within 60 s (`kMaxFailures`, `kWindowMs`) refuse that
address, or that id, for 60 s (`kLockoutMs`). A failed proof counts
against the address and the relay introduction only, never the id it
names: it did not come from that id's key, and counting it would let
anyone who knows a paired device's id lock the device out. A proved key
the Core has not paired counts against its id as well. A sign-in refused
while limited is not counted again. A connection through the relay has no
address of its own (`SessionTransport::peerAddress` is empty there), so the
limit applies per relay introduction (and per id, for proved keys). The device limiter
never consults the token's (or, later, the pairing code's), and they never
consult it: wrong tokens do not lock out a device's key, and failed device
sign-ins do not lock out the token.

**Enrolment through the token.** On a Core upgraded with a token, a window
that sends its `device` block together with the right `token` is admitted
by the token (the desktop sends it when the Core's hello carries its
identity and `deviceAuth`, its binding verifies for the certificate, and
the connection's pin was checked; iPhone app plan Task 18); its block must still prove its key over this connection's
transcript (`DeviceAuthenticator::verifyPossession`), or the sign-in is
refused `deviceProofFailed` and nothing is enrolled. A key not yet paired is
added as kind `computer`, `enrolledThroughToken` true, named by its block
(or "Computer" when the name is not usable). From then on the window signs
in with its key alone, nothing typed.

**Last seen.** Every authenticated connection of a paired device records
the time and the peer's address (empty over the relay, never the relay's).

### 3.6 Pairing

A device that is not paired pairs on a connection of its own (iPhone app
plan Task 14; the pairing design, sections 4.2, 4.3 and 4.5). After the
station's `hello` it sends its own `hello` and then `pair.start` in place of
`auth.request`, and only to a station whose `hello` declares
`features.pairing` 1 (`SpakeExchange::isAvailable()` and a usable identity
key, the same condition as `deviceAuth`). The station declares it; it never
asks it of a client. Nothing on a pairing connection signs in: the
connection ends after `pair.accept`, the station's `pair.confirm` or
`pair.fail`, with no `session.end`, and the device then signs in by key on
a new connection (section 3.5). An `auth.request` after `pair.start`, or a
`pair.*` message out of turn, ends the connection with `session.end`
`protocolError`. The handshake deadline (section 12.2) applies.

**The pairing window** (`PairingWindow`) is the station's, not any
connection's:

- `OpenUnclaimed`: the Core has no paired device and no active token
  (`DeviceStore::isClaimed` false). Open with no timer, but with the
  attempt ceiling below. One tap pairs, and so does the code.
- `ClosedClaimed`: the Core is claimed and the window is shut. Nothing
  pairs.
- `OpenReopened`: reopened on a claimed Core, from the Core's console or
  by a paired device (`pairing.open`, section 9.1). Only the code pairs.
  It closes after one successful pairing, on `pairing.close`, or 10
  minutes after it opened (`PairingWindow::kReopenedLifetimeMs`).
- `ClosedUnclaimed`: an unclaimed Core whose window the attempt ceiling
  closed. Nothing pairs until the console reopens it (`nereusd pairing
  open`); no device is paired to do it, so physical access decides.

**The attempt ceiling.** Five codes burned in a row on a direct
connection (`PairingWindow::kMaxConsecutiveFailures`) close any open
window, reopened or unclaimed. A closed window opens again only from the
Core's console or, on a claimed Core, from a paired device
(`pairing.open`). Reopening starts afresh: no failures counted and no wait,
so the code is there at once.

**Codes burned through the rendezvous** (a pairing mailbox, section 19)
never count toward the ceiling and never close the window (the operator's
ruling of 2026-09-26). Five of them in a row pause pairing through the
rendezvous instead: 1 minute (`PairingWindow::kFirstServicePauseMs`), then
twice as long each time it is hit again with no pairing in between, at
most 60 minutes (`PairingWindow::kMaxServicePauseMs`). While paused, a
mailbox's `pair.start` gets `pair.fail` with `retryAfterMs` the time left,
before any code is taken, so it burns nothing. Pairing on a direct
connection stays open throughout. A pairing, or reopening the window, ends
the pause and starts over at 1 minute. A code burned either way rotates
after the same wait.

**The service shut** (the operator's ruling of 2026-09-30). The Core also
counts codes burned through the rendezvous in total, however far apart;
a pause never resets the count. The 20th
(`PairingWindow::kMaxServiceFailuresTotal`) shuts pairing through the
rendezvous: every mailbox `pair.start` then gets `pair.fail` with
`retryAfterMs` 0, before any code is taken, and waiting does not help.
Pairing on a direct connection keeps working. Only a pairing or a
reopening at the Core (its console's `pairing open`, or Add a device in
the Core's own window, or the reset of an unclaimed window) clears the
count; a paired device's `pairing.open` does not. The count and the shut
are kept in the Core's settings (`PairingServiceFailuresTotal`,
`PairingServiceShut`), so a restart of the run (a radio change) or of the
Core keeps pairing through the rendezvous shut. The Core's window and
`pairing show` say so in plain words.

The first pairing closes an unclaimed window, for good. `devices.revoke`
never removes the last device while no token is active (section 9.1), so a
claimed Core becomes unclaimed again only through its console's `reset
--unclaimed --yes`, and its window then opens.

**The code** is a number and two words, `<nameplate>-<word>-<word>`, for
example `7-anvil-harbor`. The words come from
`resources/pairing-words-v1.txt`: 256 lowercase words of 4 to 7 letters,
no two within one edit of each other and no two alike in sound. The
number is the rendezvous nameplate: while the Core is registered with the
rendezvous (section 19) it holds a nameplate there while its pairing window
is open and shows that number. A Core that has never held one picks a
number from 1 to 99 (`PairingWindow::kLocalNameplateMax`). A Core that
loses the rendezvous keeps showing the number it last held, since the code
still pairs on a direct connection (the number is part of the password, not
an address there), and shows the new number when it registers again and
claims one; a change makes a new code unless the code is being tried.
Both ends normalise a typed code before they use it
(`PairingCode::normalise`). Normalising lowercases and trims, drops any
leading zeros of the number, and joins the three parts with single
hyphens, whatever separators were typed. It refuses anything that is not
a number from 1 to 999999 and two words of the list. The normalised text,
as UTF-8, is the password.

The code is given on request by the Core's console (`nereusd pairing
show`, over its owner-only control socket), on its status page while the
Core is unclaimed, and (from iPhone app plan Task 49) on the Remote Access
page of a desktop running the Core. It is never printed to standard output
and never logged: on a packaged Core both land in the journal. The link carries it only to a connection signed in with
a paired device's own key: in the `devices` object's `pairingCode` and in
`pairing.open`'s result (section 7.1). A connection signed in with the
token receives `""` in `pairingCode`, even when its hello declares
`deviceAuth`, and its `pairing.open` is refused (section 9.1). A
fixture writes the code as `"$string"`, never literally.

**Single use, and the wait.** The station commits to the code when the
device's step 1 arrives (`PairingWindow::takeCode`). From then on the code
is either paired with or burned: a wrong code, a `pair.fail` from the
device, or the connection ending first all burn it. Only one exchange
holds the code at a time, and a second one's step 1 is refused. After a
burned code the next one appears after 5 s. The wait doubles after each
consecutive failure (`kFirstRetryMs`; 5, 10, 20 and 40 s, since the fifth
burn closes the window), and a successful pairing resets it. While no code is shown, `pair.start` in
code mode is refused, and `retryAfterMs` says when the next one appears.

**The messages** (every binary value is base64url without padding):

| Kind | From | Keys |
| --- | --- | --- |
| `pair.start` | device | `mode` `"lan"` or `"code"`; `device` `{"publicKey", "name", "kind"}`: the device's P-256 key as in section 3.4, a name, and `phone`, `tablet` or `computer` |
| `pair.accept` | station | `identity` `{"publicKey", "certBinding"}` as in the hello (section 3.4); `label`, the Core's label (`devices`' `stationLabel`) |
| `pair.spake` | both | `step`, 0 to 3; `data`, that step's bytes |
| `pair.confirm` | both | `box`: a 24-byte nonce, then the XChaCha20-Poly1305 (IETF) ciphertext and tag |
| `pair.fail` | station, and the device after its own step 3 fails | `reason`, plain words; `retryAfterMs`, a whole number of milliseconds, 0 to 2147483647 (0: now, or not with this Core as it stands) |

**One tap** (`mode` `"lan"`). The station adds the device when the Core
is unclaimed (`OpenUnclaimed`), `pairing_lan_click` is `allow`, and the
connection's address is on one of the Core's directly connected networks
(`StationServer::isOnDirectNetwork`: a loopback address, or one inside the
subnet of an address of a running interface). A relayed connection has no
address of its own, so it never qualifies. The station then sends
`pair.accept` and ends the connection.

One tap trusts every network the Core's computer is on, not only its LAN:
a VPN or overlay it has joined (ZeroTier, Tailscale, WireGuard, a Docker
bridge) is a running interface too, and a loopback address counts, so a
local reverse proxy or SSH tunnel qualifies. Anyone on any of them can
claim an unclaimed Core with one tap. `pairing_lan_click = deny` is the
remedy: every device then pairs with the code (the sample configuration,
`packaging/nereusd.conf.sample`, says so).

**The code** (`mode` `"code"`), SPAKE2+EE over libsodium
(`SpakeExchange`). Its fixed values are the client identity
`"nereussdr-device-v1"` and the server identity `"nereussdr-station-v1"`.
Password hashing uses libsodium's default algorithm (Argon2id) with
`crypto_pwhash_OPSLIMIT_INTERACTIVE` and
`crypto_pwhash_MEMLIMIT_INTERACTIVE`:

1. Station: `pair.spake` step 0, 36 bytes: the hash parameters and salt.
   The station hashes each code once, when the first code-mode pairing
   asks for it, on a worker thread (`StationServer::startPairingHash`), so
   the hash never holds up the Core's event loop or another device's
   connection; step 0 goes out when the hash is back.
2. Device: checks that step 0 names exactly those parameters
   (`crypto_spake_validate_public_data`) before it hashes the code, then
   sends step 1, 32 bytes. The station takes the code here.
3. Station: step 2, 64 bytes, once step 1 is a valid point
   (`crypto_core_ed25519_is_valid_point`: canonical, on the curve, on the
   main subgroup, not of small order); one that is not is a malformed
   step 1 and burns the code.
4. Device: step 3, 32 bytes. When the codes differ, the device's step 3
   fails; it sends `pair.fail` instead, and the station burns the code and
   answers with its own `pair.fail`. Otherwise the device sends step 3 and
   then its `pair.confirm`, whose box is sealed with the shared key
   `client_sk` around the compact JSON `{"publicKey", "name", "kind"}`.
5. Station: checks step 3 (spake2-ee's step 4). A mismatch burns the code
   and sends `pair.fail`. When the window has closed since step 1 (closed,
   its ten minutes over, or closed and reopened), it burns the code and
   sends `pair.fail` as for a closed window. It opens the device's box. The box's
   `publicKey` must be the one `pair.start` named, and the box's `name`
   and `kind` win over the plain ones for a new device. The station adds a new device and
   answers with its own `pair.confirm`: a box sealed with `server_sk`
   around `{"identity": {"publicKey", "certBinding"}, "label"}`. Then it
   ends the connection.

Code pairing also confirms an already paired key, through the same complete
SPAKE exchange and current pairing window. In that case the existing device
record is preserved, including its saved name and dates. Removing that device
during the exchange invalidates confirmation, even if the same key is added
again before confirmation. The client learns the Core identity only from the
authenticated confirmation, then connects and signs in by device key on a new
ordinary connection. A pairing mailbox never admits a session. This lets a
saved phone use only the current code without a manual address or an
unauthenticated identity shortcut.

Whoever carries these messages (the rendezvous, later) learns nothing it
could test guesses against. Each exchange is one guess, and the code burns
after it.

| Refusal | `pair.fail` reason | `retryAfterMs` |
| --- | --- | --- |
| No pairing on this Core (no identity key or cryptography) | "This Core cannot pair new devices." | 0 |
| A key that is not a P-256 key, or an unusable name or kind (in `pair.start` or the box), or a box that does not open or names another key | "The Core could not read this device's details. Update this app." | 0 (the wait, from the box on) |
| The key is paired already in one-tap mode | "This device is already paired with this Core. Connect to it instead." | 0 |
| An existing device is removed during code confirmation | "This device was removed from the Core while pairing. Open pairing again to reconnect it." | current route backoff |
| The window is closed (at `pair.start`, or at the confirm step when it closed after the exchange took the code, which burns it) | "This Core is not taking new devices. Open pairing on the Core or on a paired device first." | 0 |
| One tap on a claimed Core | "One tap pairs only a Core with no paired devices. Use the pairing code the Core shows." | 0 |
| One tap with `pairing_lan_click = deny` | "This Core pairs only with its code. Use the pairing code the Core shows." | 0 |
| One tap from off the Core's networks | "One tap works only on the Core's own network. Use the pairing code the Core shows." | 0 |
| Another exchange holds the code | "Another device is pairing with this Core right now. Try again shortly." | 5000 |
| Pairing through the rendezvous is paused (five wrong codes in a row through it) | "The Core has paused pairing from outside its network after several wrong codes. Try again later, or pair on the Core's own network." | the pause left |
| Pairing through the rendezvous is shut (20 wrong codes in total through it) | "The Core has turned off pairing from outside its network after too many wrong codes. Pair on the Core's own network, or reopen pairing at the Core." | 0 |
| No code shown yet (the wait) | "The Core is waiting before it shows a new pairing code. Try again when the new code appears." | until the next code |
| The code changed between step 0 and step 1 | "The pairing code changed. Enter the code the Core shows now." | 0, or the wait |
| A malformed step 1 | "The pairing code was not accepted. A new code will appear on the Core." | the wait |
| A wrong code (either side) | "The pairing code was not right. A new code will appear on the Core." | the wait |
| The device could not be saved | "The Core could not save this device. Try again." | 0, or the wait once the code was taken |

`nereus_pairing_peer` (`tests/tools/`) is the station's side of one
pairing over standard input and output, one message per line, for the
cross-implementation tests. It prints `{"type":"peer.ready","code",
"certSha256"}` first and `{"type":"peer.done","paired","devices"}` last.
Its options are `--lan-deny`, `--address <ip>` (default `127.0.0.1`) and
`--claimed` (a device paired first, the window reopened). The live
exchange cannot be scripted in a fixture, so this tool proves it.

## 4. Messages

Every message is `{"type": "<kind>", ...}`. The table lists every kind, the
keys a message of that kind must carry (removing one makes the station's
decoder refuse the message) and the keys it may carry, each with the JSON
type the station writes it as. The key sets are every key
`SessionMessages::encode` can write for the kind, conditional ones
included: `tst_link_surface_manifest` reads the encoder's source and fails
when a key it can write is missing here, or a key here is one it never
writes.

<!-- surface:messageKinds -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Kind (`type`) | Required keys (JSON type) | Optional keys (JSON type) |
| --- | --- | --- |
| `auth.request` | `token` (string), `type` (string) | `device` (object) |
| `auth.result` | `accepted` (boolean), `reason` (string), `type` (string) | `code` (string), `retryable` (boolean) |
| `capabilities` | `properties` (array), `type` (string) | none |
| `command.invoke` | `args` (array), `id` (number), `type` (string), `verb` (string) | none |
| `command.result` | `accepted` (boolean), `affected` (array), `id` (number), `reason` (string), `type` (string), `verb` (string) | `values` (array) |
| `confirm.request` | `affected` (array), `expiresInMs` (number), `id` (number), `kind` (string), `reason` (string), `type` (string) | `change` (object), `choices` (array), `forCommandId` (number), `forSettingsKey` (string), `forWriteId` (number), `holder` (object) |
| `delta` | `key` (string), `properties` (array), `type` (string) | none |
| `hello` | `major` (number), `minor` (number), `peer` (string), `settingsSchema` (number), `type` (string) | `challenge` (string), `features` (object), `identity` (object), `majors` (array) |
| `media.control` | `payload` (object), `type` (string) | none |
| `notice` | `id` (number), `kind` (string), `reason` (string), `secondsAgo` (number), `takeBack` (boolean), `type` (string) | `byDeviceId` (string), `byKind` (string), `byName` (string), `byShortName` (string), `bySource` (string), `change` (object), `slices` (array) |
| `object.create` | `class` (string), `key` (string), `properties` (array), `type` (string) | none |
| `object.destroy` | `class` (string), `key` (string), `type` (string) | none |
| `pair.accept` | `identity` (object), `label` (string), `type` (string) | none |
| `pair.confirm` | `box` (string), `type` (string) | none |
| `pair.fail` | `reason` (string), `retryAfterMs` (number), `type` (string) | none |
| `pair.spake` | `data` (string), `step` (number), `type` (string) | none |
| `pair.start` | `device` (object), `mode` (string), `type` (string) | none |
| `path.join` | `ticket` (string), `type` (string) | none |
| `path.switch` | `type` (string) | none |
| `property.result` | `key` (string), `results` (array), `type` (string), `writeId` (number) | none |
| `property.write` | `key` (string), `properties` (array), `type` (string) | `writeId` (number) |
| `record.batch` | `generation` (number), `removes` (array), `reset` (boolean), `stream` (string), `type` (string), `upserts` (array) | none |
| `schema` | `class` (string), `fields` (array), `type` (string) | none |
| `session.end` | `reason` (string), `type` (string) | `code` (string), `retryable` (boolean), `secondsAgo` (number), `takenOverBy` (string), `takenOverById` (string) |
| `session.held` | `devices` (array), `revision` (number), `type` (string) | `placeFreed` (object), `placeTaken` (object) |
| `session.takeover` | `deviceId` (string), `revision` (number), `type` (string) | none |
| `settings.reject` | `key` (string), `properties` (array), `type` (string) | `reason` (string) |
| `settings.remove` | `key` (string), `properties` (array), `type` (string) | none |
| `settings.snapshot` | `properties` (array), `type` (string) | none |
| `settings.value` | `key` (string), `origin` (string), `properties` (array), `type` (string) | none |
| `settings.write` | `key` (string), `origin` (string), `properties` (array), `type` (string) | none |
| `snapshot.complete` | `type` (string) | none |
| `station.metrics.v1` | `payload` (object), `type` (string) | none |

<!-- /surface -->

### 4.1 Property entries

Capabilities, settings, object properties, deltas, command arguments and
command result values all carry the same entry:

```json
{"ordinal": 0, "name": "frequency", "kind": "f64", "value": 14074000.0}
```

`kind` is one of `bool`, `i64`, `f64`, `utf8` or `enum`
(`kWireKindNames`, `SessionMessages.cpp`). An `i64` or `enum` value is a
JSON number that must be a whole number inside the 64-bit signed range;
JSON numbers are doubles, so values are exact up to 2^53. A `utf8` value is
a string and a `bool` value is `true` or `false`. No value is coerced from
another JSON type: a mismatch refuses the whole message. `ordinal` is the
property's position in its class's schema (section 7); capabilities,
settings and command arguments carry 0.

### 4.2 Numbers that are not finite

JSON has no NaN and no infinity. An `f64` value that is not finite travels
as one of three strings: `"nan"`, `"inf"` or `"-inf"` (`kFloatNan`,
`kFloatPosInf`, `kFloatNegInf` in `SessionMessages.cpp`). Every other
string in an `f64` entry is refused. The case is common: `SliceModel`'s
`snrDb` is NaN until a RADE signal is decoded.

## 5. Connecting

### 5.1 The connect sequence

1. The client opens the WebSocket and checks the pin (section 3.2).
2. The station sends `hello` first, as soon as it accepts the connection,
   before the client has sent anything (`StationServer::acceptTransport`).
   It names every link major the station supports (section 6.1), so a
   client can pick one, or leave without having sent its token. The one
   exception: a station already holding its limit of connections (24,
   `kMaxConcurrentPeers`, section 15) sends no `hello`. The first and only
   message on the new connection is `session.end` "The Core already has
   as many connections as it allows. Try again shortly.", `retryable`
   true, and the station closes it. A client handles a `session.end` in
   place of the `hello` as it would any other (fixture
   `connection-limit`).
3. The client answers with its own `hello`, naming the major it chose,
   then `auth.request`: a paired device's `device` block with `token` `""`
   (section 3.5), or the token of a Core upgraded from before paired
   devices, with or without the window's `device` block to enrol
   (`StationClient.cpp` sends the token after its pin check, and to a Core
   it paired with only its `device` block, after checking the Core's
   identity and certificate binding). A device
   that is not paired sends `pair.start` instead, and the connection
   pairs and ends (section 3.6).
4. The station sends `auth.result`. On success it decides who is let in
   (`StationServer::admit`, iPhone app plan Task 71): up to four devices
   hold places on one Core at once (`kMaxDeviceSessions`, section 12.3). A
   device that already holds a place, live or away (section 12.4), is let
   in at once and its older connection ends with `session.end` "This device
   connected again.", `retryable` false, `code` `sameDevice`; with a place
   free the device is let in. At capacity, an authenticated paired device
   that declared both `sessionHolder` 1 and `deviceAuth` 1 at minor 11
   receives `session.held` in place of `capabilities` (below). A window
   without that gate is told "The Core is full. Update NereusSDR to take a device's place, or try again later." with `retryable` true, no code. No ordinary sign-in ends another
   device's session. A device let in gets, in this order
   (`StationServer::promoteToSession`):
   - `capabilities` (section 6);
   - `settings.snapshot`: every station-scoped setting (section 8);
   - one `schema` per mirrored class, in the order the classes are first
     watched (`MirrorView::attach`);
   - one `object.create` per object, each with its full property set: the
     fixed objects first, then one per slice the device owns and, to a
     device with `sessionHolderVersion` 1, one `marker:<id>` per slice of
     another device (section 7.1, iPhone app plan Task 73);
   - `snapshot.complete`.

   While held, the device receives no capabilities, settings, mirrored
   objects, records or media. Its connect deadline pauses; the original
   question expires after 60 seconds, including across list refreshes.
   `session.held` carries the four devices, a revision, and optional
   `placeTaken` or `placeFreed`. Entries use the `connectedDevices` names
   and slice descriptions, plus `from` and `replaceable`. Away devices
   appear first, longest away first; then present devices by actual
   command/write inactivity, with the on-air device last. A hosting desktop
   appears but cannot be chosen. The client answers with `session.takeover`
   `{deviceId, revision}`; an empty id cancels. A stale revision or hosting
   choice returns the current list without extending the deadline. A free
   place admits the first waiting device. A successful choice first
   confirms the selected holder is unkeyed and MOX is off, then ends and
   removes that device and admits the waiting device. `session.end` for the
   replaced connection carries `takenOverBy`, `takenOverById` and
   `secondsAgo` with code `takenOver`.
   `StationServer` owns the first-asked queue, original deadline, operation
   serial and displayed-list revision. `DeviceSessionRegistry` owns the
   admitted/away/hosting places, actual activity order, and place-taken
   and place-freed records kept until sign-in, revoke or restart.

   These go to the device let in alone, from its own view of the station's
   state (iPhone app plan Task 72): another device's session receives none
   of them, and what the station had still to send another device is sent
   to it as before.
5. From then on the station sends `delta`, `object.create` and
   `object.destroy` as its state changes, and the client may send property
   writes, settings writes and commands. With several devices on one Core,
   each message goes where section 12.4's routing says.

A `hello` carries `major`, `minor`, `settingsSchema` (the sender's settings
schema version) and `peer` (a name for the sending program). A difference
in `settingsSchema` is logged by both ends and is not a refusal. It may
also carry `majors` and `features` (sections 6.1 and 6.2); the station's
`hello` always carries both, and `identity` and `challenge` (section 3.4)
when its identity key is usable.

The station refuses, with `session.end`, `retryable` false and `code`
`protocolError`: a second `hello` ("This app started connecting twice on one
connection."), `auth.request` before `hello` or a second `auth.request`
("This app sent its pairing token out of order."), and any other message
before authentication ("This app sent a request before the Core had
accepted its pairing token.").

### 5.2 Resends during a session

- **A changed transmit permission** (iPhone app plan Task 34). When a
  session's `txPermitted` changes (its `snapshot.complete` went out, the
  holder of transmit changed, the Core's `remote_transmit` changed), the
  station sends it `capabilities` again. It does not resend
  `settings.snapshot`.

- **A late radio.** A station can accept a client before its radio is
  found. When the radio connects, the station sends `capabilities` and
  `settings.snapshot` again, one event-loop turn later, to every session
  let in when the radio arrived, each with its own capabilities
  (`currentRadioChanged` in the `StationServer` constructor). Settings are scoped to the connected radio,
  so the snapshot is merged into the client's settings, not a replacement.
- **A changed display allowance.** When the station's display budget
  changes, it sends `capabilities` again with the new allowance
  (`publishDisplayBudgetCapabilities`), when media is available to the
  session. It does not resend `settings.snapshot` in this case.

A client treats every `capabilities` message as a whole new set.

## 6. Versions and capabilities

### 6.1 The version rule

Both ends send a major and a minor in `hello`. The minor is
`kSessionProtocolMinor` 11 (`SessionMessages.h`) and stays there: a
feature added since carries its own capability version (section 6.3), and
one the station must know about before capabilities are sent is declared
in `features` (section 6.2).

**Majors, both ways.** Each end supports its own major and the one before
it (`kSupportedSessionMajors` in `LinkVersion.h`; today `[1]`, since there
is no major before 1), and the two ends agree the highest major both
support (`LinkVersion::agreeMajor`). Only ends two or more majors apart
share none.

- `majors` in `hello` is the sender's supported majors, oldest first, an
  array of whole numbers from 0 to 65535, never empty. A `hello` without it
  stands for `[major]`, which is what every peer built before the key
  existed sends.
- The station's `hello` has `major` set to the **oldest** major it supports
  and `majors` to its whole list. A client built before `majors` existed
  reads only `major` and leaves unless it is the one major it speaks, so
  the oldest is the value every client the station can still serve
  accepts. A client that reads `majors` ignores `major`. For example a
  station supporting `[1, 2]` sends `"major": 1, "majors": [1, 2]`.
- The client picks the highest major in both its list and the station's,
  and sends it as `major` in its own `hello`, with its own list as
  `majors`. The station accepts a client `major` that is in its own list,
  and the session runs at that major. With no shared major, the desktop
  client disconnects without sending its `hello` or its token and without
  retrying, and shows the station's wording (below).
- The station refuses any other `major` with `session.end`, `retryable`
  false, and a reason naming both sides' newest versions and the side to
  update (`SessionEndReasons::versionRefused(station majors, client
  majors)`), for example "This Core runs link version 1 and this app runs
  version 3. Update the Core." The reason is plain words and an app shows
  it as sent.
- An equal major agrees the lower of the two minors
  (`std::min(kSessionProtocolMinor, message.protocolMinor)`), and each end
  keeps to what the agreed minor allows.

| Station supports | App supports | Agreed |
| --- | --- | --- |
| `[1]` | `[1]` | 1 |
| `[1, 2]` | `[2, 3]` | 2 |
| `[3, 4]` | `[2, 3]` | 3 |
| `[2, 3]` | `[1]` | none: refused, "Update this app." |
| `[1]` | `[2, 3]` | none: refused, "Update the Core." |

A second major does not exist yet. The station and the desktop client take
their lists as a constructor argument, so the negotiation is tested with
injected lists (`tst_link_version`), and a debug build of `nereusd` takes
`--test-link-majors <list>` (for example `1,2`), which replaces the list
the station advertises and accepts, so an app's version screens can be
tried against a real station. A release build of `nereusd` refuses to
start with that option: "--test-link-majors works only in a debug build of
nereusd."

**Declared features.** `features` in `hello` is an object from a feature
name to a whole-number version from 0 to 2147483647; names are not empty.
A `hello` without it declares none. A receiver ignores a name it does not
know. What the station must know about the app before capabilities are
sent (device authentication, pairing, the takeover question, Setup
descriptions) is declared here, and asked with
`StationServer::peerDeclares(peer, feature, minVersion)`; the desktop
client asks `StationClient::stationDeclares(feature, minVersion)`. The station
declares `deviceAuth` 1 (section 3.5), `pairing` 1 (section 3.6) and
`sessionHolder` 1 (below) when its identity key is usable.

**`remoteTx` 1** (iPhone app plan Tasks 34 and 35): the client understands
remote transmit: `txPermitted`, the transmit refusals, `tx.setTxSlice` and
keying (`tx.key`, `tx.unkey`, `tx.tune`, `tx.twoTone`, section 18.6). A peer that declares it at minor 11 is sent
`remoteTxVersion` (section 6.3); `txPermitted` is true only for a peer that
declares it. It is also sent `txStateVersion` and the `txState` object
(iPhone app plan Task 39, section 18.8). The station does not declare it.
The desktop's remote window declares it (desktop remote transmit): its
MOX, TUNE, two-tone, microphone, VOX and TCI programs transmit through the
Core, and its transmit meters read `txState`.

**`vax` 1** (iPhone app plan Task 25, R-IOS-18): the client shows the VAX
channels of the computer the Core runs on (its VAX tool). A peer that
declares it at minor 11 is sent `vaxVersion` (section 6.3) and, when that is
1, the `vax` object (section 7.1). The station does not declare it. The
desktop's remote window declares it: its VAX applet runs that computer's
own VAX channels (R-R3-44) and, below them, shows the Core computer's in a
"Station computer" section from this object, subscribing to `vaxLevels`
only while the applet is shown.

**`txEqCurve` 1** (R-IOS-13, R-R3-49): the client reads the TX EQ
parametric curve in the form section 7.1 documents ("The TX EQ curve"). A
peer that declares it at minor 11 is sent `txEqCurveVersion` (section 6.3)
and `transmit`'s `txEqCurve`; a peer that does not sees exactly the wire it
was built for, with neither. The station does not declare it. The
desktop's remote window does not declare it either: its TX EQ dialog reads
`txEqParaEqData` itself.

**`diversityPattern` 1** (phone wire batch): the client draws the Diversity
dialog's sensitivity pattern from the Core's samples (section 7.1, "The
diversity pattern"). A peer that declares it at minor 11 is sent
`diversityPatternVersion` (section 6.3) and each slice's
`diversityPattern`; a peer that does not sees exactly the wire it was
built for, with neither. The station does not declare it. The desktop's
remote window does not declare it either: its Diversity dialog draws the
pattern itself from the same function.

**`logCategoryList` 1** (phone wire batch): the client names the Core's
logging categories by the labels the Core's Support dialog shows. A peer
that declares it at minor 11 is sent `logCategoryListVersion` (section 6.3)
and `radio`'s `logCategoryList` (section 7.1); a peer that does not sees
exactly the wire it was built for, with neither. The station does not
declare it, and the desktop's remote window does not: it lists its own
build's categories.

**`radioModels` 1** (phone wire batch): the client names each radio the
Core can see by its model, and offers the models that radio can run as. A
peer that declares it at minor 11 is sent `radioModelsVersion` (section
6.3) and, when that is 1, `modelLabel` and `models` on each `stationRadios`
record (section 7.7); a peer that does not sees exactly the wire it was
built for, with neither. The station does not declare it. The desktop's
remote window declares it too: its Manage Radios model choice is the
record's `models`, and on a Core that sends none the choice is disabled
with its reason.

**`radeStatus` 1** (RADE on the phone's VFO flag): the client shows the
RADE decoder's sync and frequency offset as the desktop's VFO flag does
(section 7.1, "The RADE status"). A peer that declares it at minor 11 is
sent `radeStatusVersion` (section 6.3) and each slice's `radeSynced` and
`radeFreqOffsetHz`; a peer that does not sees exactly the wire it was
built for, with none of them. The station does not declare it. The
desktop's remote window declares it too: its VFO flag and RADE applet show
the Core's sync, SNR and offset as a local window's do, and it keeps the
last offset for each fresh SNR, since the Core sends the offset only when
it moves.

**`alexLpf` 1** (R-R3-46, R-R3-49): the client shows which Alex-1 low-pass
filter the Core's radio is using, as the desktop's Alex-1 Filters tab
lights one lamp. A peer that declares it is sent `radio`'s `alexLpfBits`
(section 7.1, "The Alex-1 low-pass in use"); a peer that does not sees
exactly the wire it was built for, without it. The station does not
declare it. The desktop's remote window declares it: its lamps show the
Core's low-pass.

**`paTransmitBand` 1** (R-R3-49, the PA on-the-air lock): the client
opens and locks the PA gain row the Core holds on the air. A peer that
declares it at minor 11 is sent `paTransmitBandVersion` (section 6.3) and
`radio`'s `paTransmitBand` (section 7.1); a peer that does not sees
exactly the wire it was built for, with neither. The station does not
declare it. The desktop's remote window declares it too: while the Core
is on the air its PA Gain page opens the Core's held band and locks the
rest, as a local window's does, even when the transmit slice is retuned
to another band while keyed. On a Core that sends neither, the window
opens the row for its own transmit slice's band, as before.

**`levelCalibration` 1** (Level Cal): the client shows the Core's level
calibration run. A peer that declares it is sent `radio`'s
`levelCalRunning`, `levelCalPercent`, `levelCalMessage` and
`levelCalSucceeded` (section 7.1); a peer that does not sees exactly the
wire it was built for, without them. The station does not declare it.
The desktop's remote window declares it: Setup > Hardware > Calibration
shows the run's progress and its result as a local window does.

**`txEqCurve` 2** (R-IOS-13, R-R3-49): the client also changes the curve,
with `txEq.setCurve` and `txEq.resetCurve` (section 9.1). A peer that
declares it at minor 11 is sent `txEqCurveVersion` 2 and `transmit`'s
`txEqCurve` as at 1; one that declares 1 is sent exactly what it was sent
before. The desktop's remote window declares neither: its TX EQ dialog
writes `txEqParaEqData` itself.

**`cfcProfile` 1** (R-R3-49, `transmitSettingsVersion` 15): the client
reads the CFC dialog's band editor in the form section 7.1 documents ("The
CFC band editor"). A peer that declares it at minor 11 is sent
`transmit`'s `cfcProfile`; a peer that does not sees exactly the wire it
was built for, without it. Changing the editor needs no declaration:
`cfc.setProfile` is taken from any peer at minor 11 offered
`transmitSettingsVersion` 15. The station does not declare it. The
desktop's remote window does not declare it either: it derives the same
form from `cfcParaEqData` itself, and its CFC dialog sends
`cfc.setProfile` on a Core that offers version 15.

**`coreAddresses` 1** (the phone's direct addresses, 2026-09-29): the
client dials the Core at the addresses the Core itself reports, so a phone
away from home can reach a Core's global IPv6 address directly. A peer
that declares it at minor 11 together with `deviceAuth` 1, and signs in
with a paired device's own key (section 3.5), is sent
`coreAddressesVersion` (section 6.3) and the `devices` object's
`coreAddresses` (section 7.1). Any other peer sees exactly the wire it was
built for, with neither: one that does not declare it, and a window signed
in with the token whatever it declares. The station does not declare it,
and the desktop's remote window does not: it dials the address it was
given and the ones it last reached.

**`adcAttenuators` 1** (R-R3-46, R-R3-11): the client shows each slice the
step attenuator of the ADC that slice is on. A peer that declares it at
minor 11, on a Core that offers `stepAtt`, is sent `adcAttenuatorVersion`
(section 6.3) and `stepAtt`'s `rx2AttenuationDb`, `rx2SliceMask` and
RX2's own enable and auto-attenuate settings (section 7.1); a peer that
does not sees exactly the wire it was built for, with neither, and every
slice reads `attenuationDb`. The station does not declare it; the
desktop's remote window does.
**`paProfiles` 1** (R-R3-49, R-IOS-18): the client reads and changes the
Core's PA Gain profiles. A peer that declares it at minor 11 is sent
`paProfileVersion` (section 6.3) and the read-only `paProfiles` object, and
the Core takes its `paProfile.*` verbs; a peer that does not sees none of
them. The desktop's remote window does not declare it: its PA Gain page
reads and writes the profile settings through the settings proxy.

**`txInhibitReason` 1** (HL2 I/O board fault): the client shows why the
Core holds transmit off. A peer that declares it at minor 11 is sent
`txInhibitReasonVersion` (section 6.3) and `radio`'s `txInhibitReason`
(section 7.1); a peer that does not sees exactly the wire it was built
for, with neither, and shows its general transmit inhibit wording. The
station does not declare it; the desktop's remote window does.

**`mediaDirect` 1** (the direct media ladder): the client moves media off
the media tunnel onto a direct connection. A peer with media that declares
it at minor 11 is sent `mediaDirectVersion` and `mediaStunUrls` (section
6.3), and the Core takes the `mediaDirectVersion` field of the media
`replace` operation (the remote media control document, "Replacing the
media connection"); a peer that does not sees exactly the wire it was built
for, with neither, and its `replace` keeps its three fields. The station
does not declare it; the desktop's remote window does.

**`rx2Attenuator` 1** (Level Cal 2): the client reads RX2's own input
control from the catalogue's `board` (`rx2Attenuator`, `rx2PreampItems`,
`rx2AttenuatorReason`). A peer that declares it is sent
`rx2AttenuatorVersion` (section 6.3); a peer that does not sees exactly the
capabilities it was built for. Neither the station nor the desktop's remote
window declares it; the phone does.

**`radioMic` 1** (the radio codec lane): the client reads whether the
radio's own mic input can be chosen, and the note that goes with it, from
the catalogue's `board` (`radioMic`, `radioMicNote`). A peer that declares
it is sent `radioMicVersion` (section 6.3); a peer that does not sees
exactly the capabilities it was built for. Neither the station nor the
desktop's remote window declares it; the phone does.

**`rxFilterLowPass` 1** (shared-input filters, ruling (d)): the client
shows why the receive low-pass on a shared input is set for another slice.
A peer that declares it is sent `rxFilterLowPassVersion` (section 6.3) and
the `radio` object's `rxFilter0LowPassReason` and `rxFilter0LowPassSlice`
(section 7); a peer that does not sees exactly the wire it was built for,
with neither property, and a change to the two alone sends that peer no
`radio` delta at all. The station does not declare it; the desktop's
remote window does.

**`radeReason` 1** (RADE reason): the client shows why a slice in RADE has
no working RADE decoder. A peer that declares it is sent
`radeReasonVersion` (section 6.3) and each slice's `radeReason` (section
7, "The RADE reason"); a peer that does not sees exactly the wire it was
built for, with no `radeReason`, and a change to it alone sends that peer
no slice delta. The station does not declare it; the desktop's remote
window does.

**`sessionHolder` 1** (iPhone app plan Task 71; the several-devices
design, ruling 10.1): the Core admits up to four devices at once (section
5.1). A client declares it only together with `deviceAuth` 1 or later, and
the station treats `sessionHolder` without `deviceAuth` as not declared.
A client that declares it receives `sessionHolderVersion` in its
capabilities (section 6.3) and the `connectedDevices` object (section
7.1), and may send `session.leave` (section 9.1). A client that does not
sees exactly the wire it was built for. The desktop client declares `deviceAuth` 1
when it holds its own device key (`device-identity.pem` in its profile
directory, `ClientDeviceIdentity`; it always does unless that file cannot
be read) and sends `{}` otherwise (iPhone app plan Task 18). Signing in
with that key to a Core it paired with, it also declares `sessionHolder` 1
(iPhone app plan Task 78), so it is asked, told and may take transmit as
any device; a token sign-in does not, since it cannot know the id the Core
numbers a token window by. **`sliceAccess` 1** (slice control and shared listening plan Task 4):
listening to another device's slice and taking or releasing control of
one. A client declares it only together with `sessionHolder` 1 (and so
`deviceAuth` 1); the station treats it as not declared otherwise. A client
that declares it receives `sliceAccessVersion` in its capabilities
(section 6.3), the `SliceAccess` objects (section 7.1), the slices it has
joined as `slice:<id>` and every other as `marker:<id>` (section 7.5), and
may send the four `slice.*` access verbs (section 9.1). A client that does
not sees exactly the wire it was built for. The desktop client declares it
whenever it declares `sessionHolder`. A client's
`deviceAuth` 1
(or later) also asks for the `devices` object and its commands (section
7.1): the station sends them to no other peer, so a window that declares
nothing sees exactly the wire it was built for. A client never sends a
message kind or verb the station has not advertised, in `features` or in
its capabilities.

**`band2m` 1** (R-IOS-26, R-R3-49; JJ's ruling 2026-09-28): the client
knows 2 m as a band of its own. Band numbers on the link are the desktop's
`Band` values: 0 160 m to 10 6 m, 11 GEN, 12 WWV, 13 XVTR, 14 to 26 the
short-wave broadcast bands, and 27 2 m (144.0 to 148.0 MHz, both ends
included, as Thetis's band tables have it). 2 m was added after every
existing band, so no number moved. A peer that declares `band2m` 1 at
minor 11 is sent `band2mVersion` 1 (section 6.3) and the link as this
document describes it. Any other peer sees exactly the wire it was built
for (section 17), in which 2 m is part of GEN: every band number that
would be 27 reads 11 (a slice's and a marker's `band`, `rxFilter0Band`,
`rxFilter1Band`, `bandOutputsBand`, a `band` key in any record, notice,
confirmation, device list or other JSON the Core sends), the catalogue's
`bands` and Setup's antenna rows leave 2 m out, the per-band watts maps
leave out `2m`, and the three per-band antenna lists carry 14 entries.
The Core takes the 14-entry lists and 14-key maps such a peer writes, and
keeps its 2 m value. `BandLinkFit::forPeerWithout2m` (`BandLinkFit.h`)
does the fitting; `tst_band_link_fit` holds it to this paragraph. The
desktop declares `band2m` 1. It never sends band 27, a 2 m antenna entry
or a `2m` watts key to a Core without `band2mVersion` 1
(`BandLinkFit::forStationWithout2m`); its 2 m band button is greyed with
"This Core does not have the 2 m band. Updating the Core adds it."
Existing settings are not moved: a value an operator saved for GEN while
tuned to 2 m stays GEN's, and 2 m starts from its own defaults (the band
stack seed 144.200 MHz USB, Thetis clsBandStackManager.cs:2160
[v2.10.3.15]).

### 6.2 The two-key feature gate

A feature is available only when both keys hold: the agreed minor is at
least the minor the feature arrived in, **and** the station advertises the
feature's capability version at a value at least the version the feature
needs (at least 1 for its first version). The desktop client applies the
gate in one place per feature; for example `remoteRfKitControlAvailable()`
is `agreedMinor >= kRadioIdentitySessionProtocolMinor` (11) and
`remoteRfKitControlVersion >= 2` (`StationClient.cpp`). The station applies
the minor half again on its side and refuses a gated command from an older
peer with a plain reason (section 9.3).

What the several-devices design sends in place of `capabilities` (the
fifth device's question, which a later version adds) is gated by the hello
feature `sessionHolder` alone, in both ends' `hello`, since no capability
has arrived by then; everything else it brings uses the two keys above,
with `sessionHolderVersion`.

Two gates in the table of section 9 need more than one row can say:

- The PureSignal action verbs (`ps3.off`, `ps3.single`, `ps3.automatic`,
  `ps3.applyCurrent`, `ps3.twoTone`, `ps3.saveCorrection`,
  `ps3.restoreCorrection`) need `psAlgorithmVersion` **equal to** 3, not at
  least 3 (`StationClient.cpp`, `setRemoteCapabilities(... psAlgorithmVersion == 3 ...)`).
- `nnr.applyModelSelection` needs both `nnrVersion` at least 1 and
  `dspAssetVersion` at least 1 (`requestApplyNnrModels`). The table records
  only `dspAssetVersion`.

### 6.3 Per-feature capability versions

The minor stops at 11. A feature added since then gets its own
`<feature>Version` capability instead of a new minor, and later revisions
of that feature raise its version. The table gives the value a station
sends with every feature switched on: media, telemetry, an enforced
display budget, the Core owning its accessories, a station TCI server, the
step attenuator, the Alex antennas and the HL2 I/O board, and a peer at
agreed minor 11 (`StationServer::buildCapabilities`). It is generated from
the `value` of each entry of `surface.json`'s `capabilities` list, which
`tst_link_surface_manifest` captures from a live station, so a version
change shows as surface drift and as a change to this table.

<!-- surface:capabilityVersions -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Capability | Value with every feature on |
| --- | --- |
| `txPermitted` | false |
| `remoteMediaVersion` | 1 |
| `remoteWidebandDisplayVersion` | 1 |
| `remoteAudioStatusVersion` | 1 |
| `spectrumGrantVersion` | 2 |
| `remoteDisplayBudgetVersion` | 1 |
| `remoteCtunVersion` | 1 |
| `stationTelemetryVersion` | 6 |
| `remoteTgxlConfigVersion` | 1 |
| `remoteFourO3AControlVersion` | 1 |
| `wdspVersion` | 210 |
| `wdspCompatibilityVersion` | 1 |
| `nnrVersion` | 1 |
| `psAlgorithmVersion` | 3 |
| `propertyResultVersion` | 1 |
| `dspAssetVersion` | 4 |
| `psDisplayVersion` | 1 |
| `notchControlVersion` | 2 |
| `audioProfileVersion` | 1 |
| `audioClockVersion` | 1 |
| `receiverAudioVersion` | 1 |
| `headphonesMixVersion` | 1 |
| `radioHardwareVersion` | 13 |
| `remotePgxlControlVersion` | 4 |
| `remoteRfKitControlVersion` | 4 |
| `stationTciVersion` | 2 |
| `accessoryDataVersion` | 3 |
| `remoteTgxlControlVersion` | 4 |
| `stationIdentityVersion` | 1 |
| `deviceAdminVersion` | 1 |
| `pairingVersion` | 1 |
| `stationCatalogVersion` | 1 |
| `displayExtrasVersion` | 4 |
| `transmitSettingsVersion` | 15 |
| `bandSelectVersion` | 1 |
| `meterReadingsVersion` | 1 |
| `dspInfoVersion` | 1 |
| `recordStreamVersion` | 2 |
| `stationRadiosVersion` | 0 |
| `txDisplayVersion` | 0 |
| `displayClockVersion` | 1 |
| `controlChannelVersion` | 1 |
| `txMonitorAudioVersion` | 1 |
| `stationFreedvVersion` | 2 |
| `mediaReplaceVersion` | 1 |
| `controlSwitchVersion` | 1 |
| `supportBundleVersion` | 1 |
| `sessionHolderVersion` | 1 |
| `remoteTxVersion` | 2 |
| `txStateVersion` | 2 |
| `txReadingsVersion` | 3 |
| `mediaTunnelVersion` | 1 |
| `mediaRelayRoutingVersion` | 1 |
| `settingsHygieneVersion` | 2 |
| `settingsBackupVersion` | 1 |
| `remoteIqVersion` | 1 |
| `txModMonitorVersion` | 1 |
| `setupDescriptionVersion` | 1 |
| `miniDisplayVersion` | 1 |
| `accessoryTxVersion` | 1 |
| `radioAntennaRowsVersion` | 1 |
| `vaxVersion` | 1 |
| `txEqCurveVersion` | 2 |
| `band2mVersion` | 1 |
| `diversityPatternVersion` | 1 |
| `logCategoryListVersion` | 1 |
| `radioModelsVersion` | 0 |
| `coreAddressesVersion` | 1 |
| `audioQualityVersion` | 1 |
| `stationTciSettingsVersion` | 1 |
| `adcAttenuatorVersion` | 1 |
| `paProfileVersion` | 1 |
| `radeStatusVersion` | 1 |
| `txInhibitReasonVersion` | 1 |
| `paTransmitBandVersion` | 1 |
| `sliceAccessVersion` | 3 |
| `mediaDirectVersion` | 1 |
| `rx2AttenuatorVersion` | 1 |
| `radioMicVersion` | 2 |
| `rxFilterLowPassVersion` | 1 |
| `radeReasonVersion` | 1 |

<!-- /surface -->

When a feature is off, its version is 0:

- `remoteMediaVersion`, `remoteWidebandDisplayVersion`,
  `remoteAudioStatusVersion`, `spectrumGrantVersion`, `audioProfileVersion`,
  `audioClockVersion`, `receiverAudioVersion`, `headphonesMixVersion`,
  `displayClockVersion`: 0 unless media is enabled.
- `remoteDisplayBudgetVersion`: 0 unless media and budget enforcement are
  on and a budget has been computed.
- `stationTelemetryVersion`: 0 unless telemetry is enabled. At 4 the
  radio section also carries, for a peer at agreed minor 11, the Core's
  PA readings and radio link quality (section 10); at 5 (parity Task 14)
  also the Core's Hermes Lite 2 link (`hl2*`, section 10).
- `remoteTgxlConfigVersion`, `remoteFourO3AControlVersion`: 0 unless the
  Core owns its accessories.
- `wdspVersion`, `wdspCompatibilityVersion`, `nnrVersion`,
  `psAlgorithmVersion`, `dspAssetVersion`: 0 on a station built without
  WDSP. `psDisplayVersion` also needs media.
- `remoteCtunVersion`, `propertyResultVersion` and `notchControlVersion`
  are never 0.
- `dspAssetVersion`: 1 carries the NNR model assets (`dspAssets.*`,
  `nnr.applyModelSelection`); 2 adds the Core's NR3 model
  (`dspAssets.selectNr3Model`, with `nr3ModelAsset`, `nr3ModelStatus` and
  `nr3Runnable` on `DspAssetService`); 3 adds `dfnrRunnable` and
  `dfnrModelStatus` on `DspAssetService`: whether the Core can run DFNR
  (the build has it and its DeepFilterNet model file is there and loaded)
  and, when it cannot, the plain reason. While it is false a window shows
  DFNR disabled and refuses turning it on with the reason. 4 adds
  `mnrRunnable` and `mnrStatus` on `DspAssetService`, the same pair for
  MNR, which runs only on a Mac Core: a window on any computer shows MNR
  disabled with `mnrStatus` while `mnrRunnable` is false. These pairs are
  the one source a window has for which noise reduction its Core runs: on
  the VFO flag, its quick controls, Setup > DSP > NR/ANF and DSP > NR,
  never by the window's own build (a Mac window on a Linux Core offers no
  MNR, and a Linux window on a Mac Core does). On a Core below 3 (for
  DFNR) or below 4 (for MNR), which does not send the pair, a window shows
  that filter disabled with "This Core does not say which noise reduction
  it can run. Updating the Core may help." and refuses turning it on. BNR carries no pair and is not offered: no build has it, so no
  window shows a BNR control. A BNR selection (`activeNr` 6, which keeps
  its value) is refused by the Core and by a window with a plain reason,
  and a slice holding one turns off.
- `notchControlVersion`: 2. At 1 the Core owns the notch list (the
  `notches` object, `notch.add`, `notch.move`, `notch.setActive` and
  `notch.delete`); 2 adds `notch.addAtSlice`, the desktop's +TNF on a
  slice (section 9.1). The four earlier verbs need 1, so a client that
  compares the version as a minimum reads 2 exactly as it read 1. It is
  sent at every minor, as before; the notch verbs need agreed minor 5.
- `spectrumGrantVersion`: 2 with media. At 1 a spectrum `context`
  reports what the Core granted the endpoint (`grantedFftSize`,
  `grantedTier`, `requestedPixels`, `grantedPixels`, `limit`); 2 (parity
  Task 17, R-R3-01) adds the `subscribe` field `decimation`, a whole
  number 1 to 16 applied to the endpoint's engine (the remote media
  control document, "Display subscriptions"). A client that compares the
  version as a minimum reads 2 as it read 1; a window told less than 2
  does not send `decimation`, and a Core refuses it from a peer below the
  grant minor as a request it cannot read. An endpoint beside another on
  its engine runs at the engine's decimation, and its context's `limit` is
  then `shared` (no new field; a client that never sends `decimation` asks
  for 1 and is told `shared` beside a decimated neighbour).
- `radioHardwareVersion`: sent only at agreed minor 11. 0 without the step
  attenuator bound; 1 with it; 2 with the Alex antennas too; 4 with the HL2
  I/O board too: the `ioBoard` object, `setAlexRxAntenna` (which needs 3)
  and the filter policy command `setAlexBpfMode` (which needs 4); 5 (group
  B fix wave) with `rxOutOnTx` on `alexAntennas` two-way (RX bypass on TX,
  the VFO flag's BYPS; the Core applies it through its AlexController,
  which clears `ext1OutOnTx` and `ext2OutOnTx`); 6 (parity Task 12) with
  the rest of the transmit half of `alexAntennas` two-way: `txAntennas`,
  `blockTxAnt2`, `blockTxAnt3`, `ext1OutOnTx`, `ext2OutOnTx` and
  `rxOutOverride`, each applied through the Core's AlexController as the
  local Antenna Control tab applies it (a TX antenna on a port blocked for
  transmit is kept and settles with a reason; Block TX moves a band on that
  port back to Ant 1; Ext 1 or Ext 2 on TX clears the other two). These
  seven writes have no on-air rule, on the Core or in a local window, as in
  Thetis: an antenna change while the radio transmits goes out on the TX
  routing, and a relay flag reaches the relays at the next MOX edge. A
  station no longer sends 3, 4 or 5; a client compares the version as a
  minimum (section 6.2), so 6 serves `setAlexRxAntenna`, `setAlexBpfMode`
  and `rxOutOnTx` too. 6 also carries `setAlexTxAntenna` (`band` i64,
  `antenna` i64; parity mini-round): one band's TX antenna, applied
  through the Core's AlexController as the local grid applies it, so an
  edit cannot put back a TX antenna the Core changed on another band in
  between, which a whole `txAntennas` list built before that change does.
  A client at 6 sends one band's TX antenna with it; the Core still takes
  a whole `txAntennas` list from a window that sends one. A port blocked for transmit is refused with "An
  antenna blocked for transmit cannot be a band's TX antenna.", an antenna
  outside 1 to 3 with "Antennas are numbered 1 to 3."; like the list, it
  has no on-air rule. 7 (parity Task 14) adds HL2 Options' I2C Control
  tool and Pin Control through the Core: `requestIoBoardI2c` and
  `setIoBoardOutput` (section 9.1), the board's output pins as `outputs`
  on `ioBoard`, and the Alex tab's three transmit high-pass switches
  (HPF Bypass on TX, HPF Bypass on PureSignal, Disable 6m LNA on TX)
  taken from a window, on and off the air (section 8.1). A station no
  longer sends 6; 7 serves every earlier version's command and property.
  8 adds the Alex Filters tabs' receive filter rows: each row's Bypass,
  Start and End for the Alex-1 high-pass ladder (`hardware/<mac>/alex/hpf/`),
  the Alex-1 band-pass bank (`alex/bpf1/`) and the Alex-2 bank
  (`alex2/hpf/`), and the Alex-2 master bypass
  (`alex2/master/bypass55MhzBpf`). The Core applies them to its radio at
  once, on and off the air, as Thetis's per-row setters re-select the
  high-pass with no MOX check: the receive high-pass is the first row whose
  edges hold the frequency (its bypass sends 0x20), and no row gives the
  bypass. A value never saved is Thetis's shipped default, so on the
  high-pass ladder a frequency below 1.8 MHz is bypassed. A window whose
  Core offers less than 8 shows the rows disabled with the reason. A
  station no longer sends 7; 8 serves every earlier version's command and
  property.
  9 (parity ruling C4) adds `setRadioSampleRate` (section 9.1), the
  radio's sample rate from a window as a local window changes it. A
  station no longer sends 8; 9 serves every earlier version's command and
  property. A window of a station at 8 or lower changes the rate with each
  of its receivers' `requestSliceSampleRate`, as before, and says so on
  the rate box ("This Core changes the sample rate of this window's
  receivers only. Updating the Core may help.").
  10 adds the Alex-1 Filters tab's low-pass rows and "6m/ByPass on RX":
  each row's Start and End (`hardware/<mac>/alex/lpf/<band>/start` and
  `/end`, `<band>` one of `160m`, `80m`, `40m`, `20m`, `15m`, `10m`,
  `6m`, in MHz) and the bypass (`alex/master/lpfBypass`, `True` or
  `False`). The Core selects the low-pass from them as Thetis's
  setAlexLPF does: the first row, in the order 20m, 40m, 80m, 160m, 6m,
  10m, 15m, whose edges hold the transmit frequency (edges included),
  and the 6m filter when none does; while not keyed with the bypass
  checked, the receive word carries the 6m filter whatever the frequency.
  An edge is stored, on and off the air, and takes effect at the next
  selection (a tune, a key edge or the bypass), as Thetis's spinners do;
  the bypass re-selects at once while not keyed. The rows are transmit
  hardware, so a Core set to receive only takes them only from a peer
  offered 10. The bypass is shown disabled with its reason on the radios
  Thetis hides it on (ANAN-8000DLE, ANAN-7000DLE, ANAN-G2, ANAN-G2 1K and
  Anvelina Pro 3), and is off there. A peer that declared `alexLpf` 1
  (section 6.2) is sent the radio's `alexLpfBits` (section 7.1), the
  low-pass in use. A window whose Core offers less than 10 shows the rows
  and the bypass disabled with "This Core cannot change the low-pass
  filter rows for this app. Updating the Core may help.". A station no
  longer sends 9; 10 serves every earlier version's command and property.
  11 sends HL2 Options' Enable CL2, CL2 frequency (1 to 200 MHz) and
  External 10 MHz to the Core's Hermes Lite 2 when a window saves them
  (`hardware/<mac>/hl2/cl2Enable`, `cl2FreqMHz`, `ext10MHz`, section 8),
  on and off the air, as mi0bot's handlers do with no MOX check; the Core
  sends the options that are on again when its radio connects. No command
  or property is added. A station no longer sends 10; 11 serves every
  earlier version's command and property. A window of a station at 10 or
  lower shows the three rows disabled with "This Core cannot change its
  radio's clock settings for this app. Updating the Core may help.", since
  that Core stores them without sending them.
  12 (Level Cal) adds `resetLevelCalibration` (section 9.1), Setup >
  Hardware > Calibration's level calibration Reset from a window, and
  applies a window's `RX1_MeterCalOffsetDb` or `RX1_DisplayCalOffsetDb`
  (section 8; the Core moves the value to the connected model's entry of
  `RxMeterCalOffsetDbByRadio` or `RxDisplayCalOffsetDbByRadio`, which keep
  one calibration per radio model as Thetis's
  `rx_meter_cal_offset_by_radio` does: `|` joined in HPSDRModel order, an
  empty entry reading that model's default) to the Core's meter and TCI `calibration_ex` when it is
  written or removed, on and off the air, as Thetis's setters and its
  reset have no MOX check. 12 also carries `startLevelCalibration` and
  `cancelLevelCalibration` (section 9.1), the Core's run of Thetis's
  `CalibrateLevel` on a slice, and `stepAtt`'s `rx2PreampMode` (section
  7.1, RX2's own preamp mode); the number was extended, not raised, as no
  Core shipped 12 without them. A station no longer sends 11; 12 serves
  every earlier version's command and property. A window of a station at
  11 or lower shows Reset disabled with "This Core cannot reset the level
  calibration for this app. Updating the Core may help.", Start
  disabled with "This Core cannot run the level calibration for this app.
  Updating the Core may help." and, for a slice on the other ADC, the
  preamp choice disabled with "This Core cannot change the preamp of this
  slice's receiver input for this app. Updating the Core may help.".
- `radioAntennaRowsVersion`: optional and appended after
  `accessoryTxVersion` only at agreed minor 11 for a peer that declared
  `radioAntennaRows` exactly 1, while the Core has a connected radio with
  a canonical MAC and a bound Alex controller on an Alex-filter board,
  and its existing complete antenna path offers `radioHardwareVersion`
  6 or later. The Core applies that same readiness check to each new verb.
  Version 1 adds `setAlexRxAntennaForRadio` and
  `setAlexTxAntennaForRadio` (section 9.1). Missing, malformed and unknown
  versions are unusable. Older peers receive byte-for-byte the previous
  capability shape and keep the existing one-band verbs.
- `vaxVersion` (iPhone app plan Task 25, R-IOS-18): optional and appended
  after `radioAntennaRowsVersion`, only at agreed minor 11 for a peer
  whose hello declared `vax` 1; any other peer receives the previous
  capability shape. 1 on a Core whose own audio engine publishes VAX
  devices (a Core the desktop hosts) and keeps record streams, 0 otherwise
  (nereusd publishes none, R-R3-44). At 1 the Core sends that peer the `vax`
  object (section 7.1), takes its writes (section 7.3), and keeps the
  `vaxLevels` record stream (section 7.7).
- `txEqCurveVersion` (R-IOS-13, R-R3-49): optional, sent only at agreed
  minor 11 to a peer whose hello declared `txEqCurve` 1, while the Core
  has a radio model, after `vaxVersion` (after `radioAntennaRowsVersion`
  or `accessoryTxVersion` when those are absent) and before
  `coreBuildInfo`.
  At 1, `transmit` carries `txEqCurve`, the TX EQ dialog's parametric
  curve as read-only JSON (section 7.1, "The TX EQ curve"). The Core
  derives it from `txEqParaEqData`, which stays as it is. There is no
  write: the curve is changed at the Core. A peer that did not declare
  the feature is sent neither this entry nor the property. An app on a
  Core that sends no entry shows the curve disabled with "This Core does
  not send the TX EQ curve. Updating the Core may help."
  At 2, sent to a peer whose hello declared `txEqCurve` 2 (a peer that
  declared 1 is sent 1), the Core also takes `txEq.setCurve` and
  `txEq.resetCurve` from that peer (section 9.1): an app changes the
  curve in the same shape it reads it. An app on a Core that sends 1
  shows the curve read-only with "This Core cannot change the TX EQ curve
  from here. Updating the Core may help."
- `band2mVersion`: optional and appended after
  `txEqCurveVersion` (after `radioAntennaRowsVersion` when the others are
  absent), only at agreed minor 11 for a peer that
  declared `band2m` 1 (section 6.1). 1: the Core sends 2 m as band 27, the
  catalogue's `bands` with its 2 m button, the per-band watts maps with
  `2m` and the per-band antenna lists with 15 entries, and takes band 27
  in `slice.selectBand`, `setAlexRxAntenna`, `setAlexTxAntenna` and their
  `ForRadio` forms. A peer sent no entry sees 2 m as GEN.
- `diversityPatternVersion` (phone wire batch): optional, sent only at
  agreed minor 11 to a peer whose hello declared `diversityPattern` 1,
  while the Core has a radio model, after `band2mVersion` (or after the
  entry before it when that is absent) and before `coreBuildInfo`. At 1 every
  `slice:<id>` carries `diversityPattern`, the Diversity dialog's
  sensitivity pattern as read-only JSON (section 7.1, "The diversity
  pattern"). A peer that did not declare the feature is sent neither this
  entry nor the property. An app on a Core that sends no entry shows its
  pattern disabled with "This Core does not send the diversity pattern.
  Updating the Core may help."
- `logCategoryListVersion` (phone wire batch): optional, sent only at
  agreed minor 11 to a peer whose hello declared `logCategoryList` 1, while
  the Core has a radio model, after `diversityPatternVersion` (or after the
  entry before it when that is absent) and before `coreBuildInfo`. At 1
  `radio` carries `logCategoryList` (section 7.1), every logging category
  the Core keeps with its label. A peer that did not declare the feature
  is sent neither this entry nor the property. An app on a Core that sends
  no entry names each category by its id from `logCategories`.
- `radioModelsVersion` (phone wire batch): optional, sent only at agreed
  minor 11 to a peer whose hello declared `radioModels` 1, while the Core
  has a radio model, after `logCategoryListVersion` (or after the entry
  before it when that is absent) and before `coreBuildInfo`. 1 on a Core
  at `stationRadiosVersion` 1, 0 otherwise. At 1 each `stationRadios`
  record also carries `modelLabel` and `models` (section 7.7). A peer that
  did not declare the feature is sent neither this entry nor those fields.
  An app on a Core that sends 0 or no entry shows the model number it has
  no name for as "Unknown model" and offers no model choice, with "This
  Core does not say which models this radio can run as. Updating the Core
  may help." on the disabled choice.
- `coreAddressesVersion` (the phone's direct addresses): optional, sent
  only at agreed minor 11 to a peer whose hello declared `coreAddresses` 1
  and `deviceAuth` 1 and that signed in with a paired device's own key,
  while the Core has a radio model and sends the `devices` object
  (`deviceAdminVersion` 1), after `radioModelsVersion` (or after the entry
  before it when that is absent) and before `coreBuildInfo`
  (`StationServer::peerGetsCoreAddresses`). At 1 `devices` carries
  `coreAddresses` (section 7.1), where the device can dial this Core. A
  peer that did not declare the feature, or signed in with the token, is
  sent neither this entry nor the property. An app on a Core that sends no
  entry dials the addresses it already keeps (section 21.1).
- `audioQualityVersion`: optional and appended last, after
  `coreAddressesVersion` (after the last entry before it when that is absent),
  only at agreed minor 11 for a peer that declared `audioQuality` 1 while
  the Core offers media (iPhone app plan Task 23). 1: the device's `audio`
  control may carry `opusBitrate`, one of the catalogue's
  `audio.opusProfiles` (remote media control v1, "Per-device audio
  quality"). A peer sent no entry gets the Core's `audio_bitrate` as
  before.
- `stationTciSettingsVersion`: optional and appended last, after
  `audioQualityVersion` (after the last entry before it when that is
  absent), only at agreed minor 11 for a peer that declared
  `stationTciSettings` 1 on a Core at `stationTciVersion` 2 (JJ's ruling of
  2026-09-28). 1: `stationTci` carries the rest of the Core's TCI server
  settings as read-only properties (`rateLimitMs`,
  `cwBecomesCwuAbove10mhz`, `iqSwap`, `alwaysStreamIq`,
  `audioBlockSamples`, `txChannel` 0 Left 1 Right 2 Both,
  `rxSensorIntervalMs`, `txSensorIntervalMs`,
  `forgetRx2VfoBOnDisconnect`, `useRx1VfoaForRx2Vfoa`,
  `copyRx2VfobToVfoa`; ranges in the remote accessory control document),
  and the Core takes `setStationTciSettings` with any of them. A peer that
  did not declare the feature is sent neither this entry nor those
  properties.
- `adcAttenuatorVersion` (R-R3-46, R-R3-11): optional, sent only at agreed
  minor 11 to a peer whose hello declared `adcAttenuators` 1, while the
  Core offers `stepAtt` (`radioHardwareVersion` 1 or more), after
  `stationTciSettingsVersion` (or after the last entry before it when
  that is absent) and before `coreBuildInfo`.
  At 1, `stepAtt` carries `rx2AttenuationDb`, `rx2SliceMask`,
  `rx2StepAttEnabled`, `rx2AutoAttEnabled`, `rx2AutoAttUndo` and
  `rx2AutoAttUndoDelayMs` (section
  7.1). A peer that did not declare the feature is sent neither this entry
  nor the two properties. Without it a client shows every slice
  `attenuationDb`, as before.
- `paProfileVersion` (R-R3-49, R-IOS-18): optional, sent only at agreed
  minor 11 to a peer whose hello declared `paProfiles` 1, while the Core
  has its own PA profile bank, after `adcAttenuatorVersion` (or after the
  last entry before it when that is absent) and before
  `coreBuildInfo`. At 1 the Core sends `paProfiles` (read-only):
  `json`, the PA Gain page's profiles (`names`, the ones its combo lists;
  `active`; `factory`; `bands`, the active profile's 14 rows in Band order,
  each `{band, gain, adjust[9], maxPower, useMax}` at one decimal place),
  and `revision`, moving by one each time `json` changes. It takes
  `paProfile.select {name}`, `.new {name}`, `.copy {name}`, `.delete
  {name}`, `.reset {}`, `.setGain {band, value}`, `.setAdjust {band, step,
  value}`, `.setMaxPower {band, value}` and `.setUseMax {band, on}`, each as
  the desktop's page does it (Setup description version 14 gives the
  ranges and words). While the radio is on the air, Select, New, Copy,
  Delete and Reset are locked. The four value verbs (`.setGain`,
  `.setAdjust`, `.setMaxPower` and `.setUseMax`) are allowed only for the
  current transmitting band and only from the device that holds transmit;
  another band, an unknown transmitting band or a non-holder is refused.
  Setup description version 20 publishes these locks per control and per
  table row, making only the holder's transmitting-band row available;
  earlier negotiated versions retain the closed version 14 rows. The
  Core's capability, receive-only, transmit-permission and value-range
  checks still apply, with the reasons it gives the desktop's own PA
  profile writes. None keys the radio.
- `radeStatusVersion` (RADE on the phone's VFO flag): optional, sent only
  at agreed minor 11 to a peer whose hello declared `radeStatus` 1, while
  the Core has a radio model, after `paProfileVersion` (or after the
  last entry before it when that is absent) and before `coreBuildInfo`. At 1
  every `slice:<id>` carries `radeSynced` and `radeFreqOffsetHz` (section
  7.1, "The RADE status"). A peer that did not declare the feature is sent
  neither this entry nor the properties. An app on a Core that sends no
  entry shows the RADE row from `snrDb` alone, as it did before.
- `txInhibitReasonVersion` (HL2 I/O board fault): optional, sent only at
  agreed minor 11 to a peer whose hello declared `txInhibitReason` 1, while
  the Core has a radio model, after `radeStatusVersion` (or after the last
  entry before it when that is absent) and before `coreBuildInfo`. At 1
  `radio` carries `txInhibitReason` (section 7.1). A peer that did not
  declare the feature is sent neither this entry nor the property. An app
  on a Core that sends no entry names a held transmit inhibit with its own
  general wording.
- `paTransmitBandVersion` (R-R3-49, the PA on-the-air lock): optional,
  sent only at agreed minor 11 to a peer whose hello declared
  `paTransmitBand` 1, while the Core has a radio model, after
  `txInhibitReasonVersion` (or after the last entry before it when that is
  absent) and before `coreBuildInfo`. At 1 `radio` carries
  `paTransmitBand` (section 7.1). A peer that did not declare the feature
  is sent neither this entry nor the property. An app on a Core that sends
  no entry opens the PA row for its own transmit slice's band.
- `sliceAccessVersion`: optional and appended after
  `paTransmitBandVersion` (after the last entry before it when that is
  absent) and before `coreBuildInfo`, only at agreed
  minor 11 and only to a peer whose hello declared `sliceAccess` 1 with
  `sessionHolder` 1 and `deviceAuth` 1; any other peer is sent no entry
  (and reads 0), so its capabilities, its `slice:` and `marker:` objects
  and its verbs are today's. Two-key gate (section 6.2): agreed minor 11
  and `sliceAccessVersion` 1 or more. 1 on a Core that runs its radio:
  the `SliceAccess` object per slice (`access:<id>`, section 7.1), each
  slice the device has joined as `slice:<id>` and every other as
  `marker:<id>` (section 7.5), the verbs `slice.listen`,
  `slice.stopListening`, `slice.takeControl`, `slice.release` and
  `slice.setListenLevel` (section 9.1), `setActiveSliceById` on any joined slice, and the
  `controlTaken` notice. A Core without the feature, and a peer that did
  not declare it, refuse the verbs: "Update this app to listen to and take
  slices on this Core." to the peer, "This Core cannot share slices
  between devices." from a Core that cannot. 2 (take-over parity) adds
  Take it back on the `controlTaken` notice (section 7.4): the notice
  offers it (`takeBack` true) and `notice.takeBack {id}` takes control of
  the slice back. The value sent is the lower of the Core's version and
  the one the peer's hello declared (`sliceAccess` 1 reads 1, `sliceAccess`
  2 reads 2, `sliceAccess` 3 reads 3), so a peer that declared 1 sees
  exactly what it saw before; nothing is renumbered and `coreBuildInfo`
  stays last. An app that declared 2 on a Core that sends 1 shows Take it
  back on the card disabled, with the reason "This Core cannot give
  control back from here. Updating the Core may help." 3 (core-slice
  take-over, JJ 2026-09-30) lets a device take the Core's own slice
  (`controllerDeviceId` `station`) with nobody at the Core's desktop
  (section 7.1, `slice.takeControl`): at 3 an app offers Take control on
  every slice as it does on any device's, and the Core's answer is the
  take's own. Below 3 the Core refuses that take in its words, "Slice
  <letter> is run by the Core itself, so control of it cannot pass to this
  device.", and an app shows Take control on a slice whose controller is
  `station` disabled, never hidden, with those words.
- `remotePgxlControlVersion`, `remoteRfKitControlVersion`,
  `remoteTgxlControlVersion`: sent only at agreed minor 11, and 0 unless
  the Core owns its accessories. `remotePgxlControlVersion` 3 adds the
  Power Genius's own settings, and 4 its OPERATE and STANDBY, the Core's
  Scan LAN for it and its saved address (`setPgxlOperate`, `scanPgxlLan`,
  `setPgxlAddress`, section 9.1, parity Task 9), and `remoteTgxlControlVersion` 1 the Tuner
  Genius's (the `accessorySettings` object and the device settings
  commands, section 9.1); `remoteTgxlControlVersion` 2 adds the Tuner
  Genius's antenna, operate and bypass (`setTgxlAntenna`,
  `setTgxlOperate`, `setTgxlBypass`, section 9.1), 3 has
  `setTgxlOperate` with `on` true put the tuner in operate whole (bypass
  off and operate on, from the one command), and 4 adds the relay nudge,
  the Core's Scan LAN and the saved address (`moveTgxlRelay`,
  `scanTgxlLan`, `setTgxlAddress`, section 9.1, parity Task 8);
  `remoteRfKitControlVersion` 3 adds `resetRfKitError`, the RF-Kit
  amplifier's Reset amp error, and 4 its OPERATE and STANDBY, antenna,
  TCI mode and saved address (`setRfKitOperate`, `setRfKitAntenna`,
  `setRfKitTciMode`, `setRfKitAddress`, section 9.1, parity Task 10).
  `remoteTgxlControlVersion` is followed by `stationIdentityVersion`.
- `stationTciVersion`: sent only at agreed minor 11, and 0 unless the Core
  runs a station TCI server. Version 2 adds its four read-only option
  properties, the `tciClients` record stream, `setStationTciOptions`, and
  `disconnectStationTciClient`. The Core keeps the options; a window does
  not write `stationTci` properties.
- `accessoryDataVersion`: sent only at agreed minor 11, and 0 unless the
  Core owns its accessories. At 1 the station sends the read-only
  `accessoryData` object (fault histories, connection counters, the
  transmit interlock and the Power Genius output limit) and accepts
  `setTxInterlockPolicy`, `setPgxlPowerCap` and `clearAccessoryFaults`.
  At 2 (parity Task 10) the object also carries the RF-Kit's connection
  counts (`rfkitConnectedSinceMs`, `rfkitPollsOk`, `rfkitPollsFailed`,
  `rfkitReconnectCount`, `rfkitLastPollMs`), appended after the older
  properties, which keep their ordinals. At 3 (group B fix wave) it also
  carries the RF-Kit's average response time (`rfkitRttAvgMs`), appended
  the same way.

- `stationIdentityVersion`: sent only at agreed minor 11. 1 when the
  Core has its identity key and signs devices in by key (section 3.5);
  0 when that key is unusable. A client learns the same before
  capabilities from the hello's `features.deviceAuth`.
- `deviceAdminVersion`: sent only at agreed minor 11. 1 when the
  Core's identity key is usable (the same condition as
  `stationIdentityVersion`): the `devices` object (section 7.1) and
  `devices.revoke`, `station.rename`, `station.acknowledgeKeyBackup` and
  `station.retireToken` (section 9.1), for a peer whose hello declares
  `deviceAuth`. 0 otherwise.
- `pairingVersion`: sent only at agreed minor 11, last. 1 when the Core
  pairs devices (the hello's `features.pairing`, section 3.6): the
  `devices` object's `pairingWindowOpen` and `pairingCode`, and
  `pairing.open` and `pairing.close` (section 9.1), for the same peers as
  `deviceAdminVersion`. 0 otherwise.
- `stationCatalogVersion`: sent only at agreed minor 11. 1 on every
  Core that has it: the read-only `catalog` object (section 7.4) goes to
  every peer at minor 11. A Core from before it sends neither the entry
  nor the object.
- `displayExtrasVersion`: sent only at agreed minor 11. 2 while the
  Core's media is enabled. At 1 a `subscribe` operation (section 11) may
  carry the display extras fields, and the Core sends an NSDX datagram
  beside each NSDC frame of an endpoint that asks for a section
  ([display extras v1](2026-09-23-display-extras-v1.md)); 2 adds the
  media control operation `clarity-retune`, Clarity's Re-tune for one
  endpoint ([remote media control
  v1](2026-09-20-remote-media-control-v1.md), "Clarity re-tune"). The
  extras need 1, so a client that compares the version as a minimum reads
  2 as it read 1. 0 otherwise; a Core from before it sends no entry and
  refuses the fields as keys it cannot read.
- `transmitSettingsVersion`: sent only at agreed minor 11, and 0 on a
  station with no radio model. At 1 a receive-only Core takes a
  `property.write` on `transmit` of any property except the keying set
  (`mox`, `tune`, `voxEnabled`, `twoToneActive`), and a `settings.write`
  or `settings.remove` of a DSP > Options TX key
  (`DspOptions<Setting><Mode>Tx`), while its radio is off the air, and
  applies it at once (a DSP > Options TX key reaches the Core's TX channel
  when the TX slice's mode is in its group). Version 1 covers the
  `transmit` properties mirrored today: `power`, `micGain`, `filterLow`,
  `filterHigh`, `lineInGain`, `userDigOut`, `pureSig`,
  `forceAttwhenPSAoff`, `forceAttwhenPowerChangesWhenPSAon`,
  `forceAttwhenPowerChangesWhenPSAonAndDecreased`, `antiVoxTauMs`,
  `antiVoxRun` and `paSettingsBypass`. At 2 it also covers the TX and
  Phone/CW applets' settings on `transmit`: `tunePower`, `voxThresholdDb`,
  `voxHangTimeMs`, `monEnabled`, `monitorVolume`, `txLevelerOn`,
  `txEqEnabled`, `cfcEnabled`, `cpdrOn`, `cpdrLevelDb`, `amCarrierLevel`,
  `dexpEnabled` and `micGainDb`, each refused outside its range with the
  range in plain words (section 7.3); the read-only `tunePowerForTxBand`
  and `tuneDrivePowerSource`; and the command `setTunePowerForTxBand`
  (section 9.1). At 3 it also covers the radio's microphone input on
  `transmit` (Setup > Audio > TX Input): `micBoost`, `micXlr`,
  `micTipRing`, `micBias`, `micPttDisabled`, `lineIn` and `lineInBoost`,
  `lineInBoost` refused outside its range (section 7.3); the Core's TX
  profiles, the read-only `activeTxProfile` and `txProfilesJson`; and the
  commands `txProfile.select`, `txProfile.save`, `txProfile.delete` and
  `rade.resetVocoder` (section 9.1). At 4 it also covers the TX EQ, CFC,
  phase rotator, CESSB, leveler and ALC settings on `transmit` (the TX EQ
  and CFC dialogs, Setup > DSP > CFC and AGC/ALC's TX Leveler and TX ALC):
  `txEqUseLegacy`, `txEqPreamp`, `txEqBandsJson`, `txEqFreqsJson`,
  `txEqNc`, `txEqMp`, `txEqCtfmode`, `txEqWintype`, `txEqParaEqData`,
  `cfcCompressionJson`, `cfcEqFreqJson`, `cfcPostEqBandGainJson`,
  `cfcPostEqEnabled`, `cfcPostEqGainDb`, `cfcPrecompDb`, `cfcParaEqData`,
  `phaseRotatorEnabled`, `phaseRotatorFreqHz`, `phaseRotatorStages`,
  `phaseReverseEnabled`, `cessbOn`, `txLevelerMaxGain`, `txLevelerDecay`,
  `txAlcMaxGain` and `txAlcDecay`, each refused outside its range and a
  band array refused whole (section 7.3). The TX EQ dialog's settings
  (`txEqEnabled` and the nine `txEq` properties above) are taken on the
  air too, as a local window changes them while transmitting (section
  7.3). At 5 it also covers Setup >
  Transmit > Power, Transmit > DEXP/VOX and Test > Two-Tone IMD: on
  `transmit`, `tuneDrivePowerSource` becomes two-way,
  `powerByBandJson` and `tunePowerByBandJson` are sent (the Core's own,
  outbound), and `dexpAttackTimeMs`,
  `dexpDetectorTauMs`, `dexpExpansionRatioDb`, `dexpHighCutHz`,
  `dexpHysteresisRatioDb`, `dexpLookAheadEnabled`, `dexpLookAheadMs`,
  `dexpLowCutHz`, `dexpReleaseTimeMs`, `dexpSideChannelFilterEnabled`,
  `antiVoxGainDb`, `twoToneFreq1`, `twoToneFreq2`, `twoToneLevel`,
  `twoTonePower`, `twoTonePulsed`, `twoToneInvert`, `twoToneFreq2Delay`
  and `twoToneDrivePowerSource`, each refused outside its range and a band
  map refused whole (section 7.3); on `stepAtt`, `attOnTxEnabled`,
  `attOnTxValue` and `forceAttWhenPsOff`; and the Power page's SWR
  Protection and External TX Inhibit keys (`SwrProtectionEnabled`,
  `SwrProtectionLimit`, `SwrTuneProtectionEnabled`, `TunePowerSwrIgnore`,
  `WindBackPowerSwr`, `TxInhibitMonitorEnabled`,
  `TxInhibitMonitorReversed`), taken while the radio is off the air
  (section 8), the SWR Protection keys applied to the Core's SWR
  protection at once. At 6 it also covers Setup > PA: the PA profiles
  (`hardware/<mac>/pa/...`: PA Gain's profiles, per-band gains, adjust
  matrix and max power) and the PA forward-power table
  (`hardware/<mac>/paCalibration/...`: the Watt Meter page), taken while
  the radio is off the air (section 8) and applied to the Core's PA
  profiles and calibration at once. PA Gain's auto-calibrate sweep keys
  the radio and waits for remote transmit. At 7 it also covers PureSignal
  arming: the commands `ps3.single`, `ps3.automatic`, `ps3.applyCurrent`
  and `ps3.restoreCorrection` (section 9.1), and a `property.write` on
  `pureSignalSettings`, which the Core then applies to its PureSignal at
  once instead of only keeping it (section 7.3). Arming keys nothing, so a
  receive-only Core permits PureSignal: its `pureSignal` object's
  `canActuate` says whether PureSignal is ready on the Core's radio, not
  whether this peer may transmit (`txPermitted`). `ps3.twoTone` with
  `enabled` true keys the radio and waits for remote transmit. Each is
  refused while the radio is on the air (section 7.3). At 8 it also covers
  Setup > Hardware Config's OC Outputs and Calibration: the OC transmit
  pins (`hardware/<mac>/oc/tx/...`: the HF and SWL TX matrices and their
  resets), taken while the radio is off the air; and the OC pin actions
  (`hardware/<mac>/oc/actions/...`), TX Display Cal and Volts/Amps
  Calibration (`hardware/<mac>/cal/txDisplayOffset`, `cal/paSens`,
  `cal/paOffset` and the Calibration tab's copies under
  `hardware/<mac>/paCalibration/cal/`), taken on and off the air, as Thetis
  changes them while transmitting (section 8). The Core applies each to its
  OC matrix or calibration at once; an OC matrix change, and the N2ADR
  switch on its HL2 (which now applies its whole preset, transmit pins
  included), wait until the radio is back on receive. The keying set stays refused on a receive-only
  Core, on and off the air, and so do raw settings writes of
  `hardware/<mac>/tx/...`, `powerByBand` and `tunePowerByBand` (the
  `transmit` object owns them). A window whose Core sends 0 keeps its
  transmit settings unavailable. A peer below agreed minor 11 is never
  offered it, and a receive-only Core refuses its transmit writes and DSP >
  Options TX keys as before. `transmitSettingsVersion` is followed by
  `bandSelectVersion`. Version 9 also offers General Region: `BandPlanRegion`
  is an integer ID 0..23 in the desktop combo order. Core validates the value,
  checks the complete signed TX passband (including XIT), and refuses writes
  and removals while on air, including at shared-setting confirmation. Removing
  the key restores United States (8). Legacy `Region` text is not migrated.
  Extended transmit is not enabled by this capability. Version 10 (iPhone
  app plan Task 40) adds `transmit.micMuted` (bool, bidirectional, appended
  after `voxEnabled`): true while the Core's mic is muted, the inverse of
  Thetis's chkMicMute, whose checked state means the mic is in use. A write
  takes the same gates as `micGainDb` (a permitted session on a transmit
  Core, on or off the air; a receive-only Core's settings writer off the
  air only) and never keys. Muting sets the Core's mic preamp to 0.0 and
  unmuting restores the mic level, as Thetis's setAudioMicGain does
  (console.cs:28856-28868 [v2.10.3.15]). It is never saved: the mic starts
  in use after every Core start.
  Version 11 offers Transmit > Power's Disable HF PA: `DisableHfPa` ("True" or
  "False", any other value refused) becomes a station setting, taken on and
  off the air as Thetis applies it with no MOX check. The Core applies it
  to its radio at once: the PA disable bit (Protocol 1 bank 10 C3 bit 7,
  and C2 bit 3 cleared on the Hermes Lite 2; Protocol 2 general packet
  byte 58 cleared, and the Alex T/R relay left open while keyed) and its
  SWR protection, which a high SWR no longer trips. On a Hermes or an
  Atlas kit it stays off whatever is saved, and a window shows the box
  disabled with the reason. A window whose Core offers less than 11 shows
  the box disabled with the transmit settings reason.
  Version 12 (addendum G-42) adds General
  Options' Extended as the Core's settings key `ExtendedTransmit` ("True" or
  "False"; absent means off). The Core's transmit gate reads it at every
  key: while it is "True" the band, filter-edge and US 60 m mode checks are
  skipped, as Thetis's CheckValidTXFreq returns true while Extended is on
  (console.cs:6780 [v2.10.3.15]). A `settings.write` or `settings.remove`
  of it is taken only from a session the station transmit gate permits
  (otherwise refused with that gate's words, for example "This Core is set
  to receive only."), never while the radio is on the air ("The radio is
  on the air. Try again when it stops."), and a write that is neither
  "True" nor "False" is refused "Extended transmit is either on or off."
  The Core's value reaches every device as `settings.value`. The older
  per-device key `ExtendedTxAllowed` stays each app's own
  (`settings.reject`, as before) and never turns Extended on.
  Version 13 (remote parity on the air)
  takes on the air everything a local window changes while transmitting
  (no TX applet, Phone/CW, TX EQ, CFC, PureSignal, Two-Tone or Setup
  transmit control is greyed under MOX, as in Thetis): every `transmit`
  property but the keying set, `stepAtt`'s three transmit settings, the DSP
  > Options TX, Power and PA keys (the DSP > Options TX and PA applies to
  the TX channel and PA profiles wait for receive; the SWR protection keys
  apply at once, as the local page and Thetis apply them), a `pureSignalSettings`
  write, and the commands `setTunePowerForTxBand`, `txProfile.select`,
  `txProfile.save`, `txProfile.delete`, `rade.resetVocoder`, the PureSignal
  arming verbs and `tx.twoTonePreset`. They are taken from the peers that
  may change them off the air (a receive-only Core's peer offered the
  transmit settings; a permitted session on a Core that allows remote
  transmit) and key nothing. Still refused on the air, as in a local
  window: the OC transmit pins (Thetis greys them under MOX) and General
  Region. A window on an older Core keeps its transmit settings disabled on
  the air with "The radio is on the air. Try again when it stops."
  Version 14 adds General Options' Prevent transmitting on a different
  band as the Core's settings key `PreventTxOnDifferentBandToRx` ("True"
  or "False"; absent means off, Thetis's default at console.cs:20843
  [v2.10.3.15]). It was each app's own key and is now the station's: the
  Core's own saved value carries over, and a remote computer's old value
  is no longer read. The Core's transmit gate reads it at every key: while
  it is "True" a key is refused when the slice about to transmit is not
  the device's active (listening) slice and is on a different band from it;
  a key on the active slice is never refused, and other slices parked on
  other bands, or held by other devices, do not count. Thetis refuses only
  when it transmits split on VFO B and VFO B's band differs from the RX band
  (console.cs:29451-29465 [v2.10.3.15]); a station has no split, so a
  transmitting slice other than the active one stands for VFO B. The
  check runs after the mode allow-list and before the US 60 m and band
  edge checks, and its refusal keeps the band plan refusal (no new code)
  with the sentence "Transmit would be on <band> while another slice you
  have open is on <band>, and Setup is set to prevent transmitting on a
  different band." Writes and removals take the same gates as
  `ExtendedTransmit` (a permitted session, never on the air), and a value
  that is neither "True" nor "False" is refused "Prevent transmitting on a
  different band is either on or off." A window whose Core offers less
  than 14 shows the box disabled with "This Core does not have Prevent
  transmitting on a different band. Update the Core to use it."
  Version 15 adds the CFC dialog's band editor: `transmit`'s read-only
  `cfcProfile` (to a peer that declared `cfcProfile` 1) and the command
  `cfc.setProfile`, which applies every band's frequency, compression,
  post-EQ gain and Q, the range, the pre-compression and the post-EQ gain
  at once, against the revision the app last saw (section 7.1, "The CFC
  band editor"). A window on a Core that offers less than 15 keeps writing
  `cfcParaEqData` and the two gains as property writes, as before.
- `bandSelectVersion`: sent only at agreed minor 11, and 0 on a
  station with no radio model. At 1 the Core takes `slice.selectBand`
  (section 9.1), a device's band button for a slice, for the bands the
  catalogue's `bands` lists (section 7.4). An app keeps its band buttons
  greyed on a Core that sends 0 or no entry. It is followed by
  `meterReadingsVersion`.
- `meterReadingsVersion`: sent only at agreed minor 11, and 0 on a
  station whose radio model runs no meter pump (no radio model, or a
  window's own). At 1 each slice carries the Core's ADC and AGC readings
  (`adcPeakDbfs`, `adcAverageDbfs`, `agcGainDb`, `agcPeakDb`,
  `agcAverageDb`, section 7.1), refreshed at the Core's meter pump rate as
  `signalPeakDbm` is, and the Multimeter polling delay (`MultimeterDelayMs`,
  section 8) sets that rate at once when a window writes it. A window's ADC
  Peak, ADC Average, AGC Gain, AGC Peak and AGC Average meters read them;
  on a Core that sends 0 or no entry those meters show no reading, never a
  frozen value. It is followed by `dspInfoVersion`.
- `dspInfoVersion`: sent only at agreed minor 11, and 0 on a station
  with no radio model of its own (a window's). At 1 the Core says how
  long its last DSP Options apply took, as `radio`'s
  `dspOptionsLastApplyMs`, and each slice's receiver's narrowest notch, as
  the slice's `minNotchWidthHz` (section 7.1), and it takes
  `dsp.filterResponse`, the filter graph's curve (section 9.1). Which
  noise reduction the Core runs is `dspAssetVersion`'s (3 and 4 above).
  On a Core that sends 0 or no entry a window shows the high-resolution
  filter graph box disabled with "This Core does not send its filter
  curve. Updating the Core may help.". It is followed by
  `recordStreamVersion`.
- `recordStreamVersion` (parity Task 19, the iPhone app plan's Task 21):
  sent only at agreed minor 11, and 0 on a station with no radio
  model of its own (a window's). At 1 the Core takes `records.subscribe`
  and `records.unsubscribe` and sends `record.batch` (section 7.7) for its
  `spots` stream and each station source's `spotConsole:<source>` stream,
  sends the read-only `spotSources` object (section 7.1) and takes
  `spots.connect`, `spots.disconnect`, `spots.sendCommand` and
  `spots.clearAll` (section 9.1). The Core runs the station's spot sources
  (DX cluster, RBN, POTA, PSK Reporter) itself, starting those whose
  Auto-Connect or Auto-Start is on in its settings with no window; each
  computer runs its own WSJT-X and SpotCollector listeners. On a Core that
  sends 0 or no entry a window shows the station's sources' buttons
  disabled with "This Core does not run its spot sources for this app.
  Updating the Core may help.". At 2 (spot resolved mode, the iPhone app
  plan's Task 62) each `spots` record also carries `resolvedMode`
  (section 7.7): the mode a click on that spot puts a slice in, as the
  desktop picks it (`SpotModeResolver::dspModeForSpot`, the Core calling
  the desktop's own resolver: the spot's own mode, else a mode word
  first or last in its comment, else the band segment at its frequency;
  a FreeDV spot is RADE on the band's default sideband). The Core sends
  2 wherever it sent 1. Everything 1 brings is unchanged, and a record
  from an older Core has no `resolvedMode`, so an app shows its Auto
  mode for spots disabled there. This is a revision of the `spots`
  record this capability defines, so it raises this version rather than
  adding one (section 6.3). It is followed by `stationRadiosVersion`.
- `stationRadiosVersion` (parity Task 21, the iPhone app plan's Task 25):
  sent only at agreed minor 11, after `recordStreamVersion` in the minor-11
  block (`sessionHolderVersion` and the remote transmit entries follow
  it), and 1 only on a Core that chooses
  its own radio (`nereusd`; a Core a desktop window hosts sends 0). At 1
  the Core sends the `stationRadios` record stream (section 7.7) and
  `radio`'s `stationRadioWaiting` (section 7.1), and takes
  `station.selectRadio`, `station.rescanRadios`, `station.setRadioModel`
  and `station.forgetRadio` (section 9.1). Its choice of radio, in order:
  a radio chosen from an app (saved in the Core's own settings once it has
  connected, kept across restarts), then `radio_mac` from `nereusd.conf`, then the one
  radio in sight when exactly one is visible; otherwise it waits for a
  choice. It never picks the first radio found. On a Core that sends 0 or
  no entry a window shows This Core's Change radio disabled with "This
  Core does not let this app change its radio. Updating the Core may
  help.".
- `settingsHygieneVersion` (remote-window parity Task 24): optional trailing
  minor-11 capability, 1 when the Core owns a local radio model. At 1 the
  Core takes `station.validateSettings` and `station.forgetSettings`, each
  with the connected radio's canonical
  upper-case colon-separated `mac`. Validation reads the Core's settings
  and board capabilities. Forget requires a paired-device key
  sign-in and is refused while the station is on the air. Both reject
  a stale or different MAC at execution. This is separate from
  `station.forgetRadio`, which changes saved-radio membership.
  A successful result carries exactly two typed `values`: `mac` (utf8) and
  `issuesJson` (utf8), a compact array of at most 32 issues with string
  `severity` (`info`, `warning` or `critical`), `key`, `summary`, `detail`
  and `fixActionId`. The JSON is at most 64 KiB, with fields bounded to
  256, 256, 1024 and 64 UTF-8 bytes respectively. A malformed response
  is unavailable rather than a valid empty issue list. A window built
  against an older Core disables these controls with a plain reason.
  Version 2 (G-38, JJ's ruling 2026-09-28) adds `station.repairSettings`
  with the same `mac` argument and the same result values: it runs the
  repair a local window's Repair Invalid Settings runs (out-of-range
  values clamped to the board, settings for hardware the board does not
  have removed, then re-validated). Like Forget it requires a paired-device
  key sign-in and is refused on the air. A Core sends 2 only to a peer that
  declared `settingsHygiene` 2 and 1 to a peer that declared 1; a peer
  that declared 1 is refused `station.repairSettings`. A window on a Core
  at version 1 keeps Repair Invalid Settings disabled with "Repair invalid
  settings is not available on this Core. Updating the Core may help.".
- `setupDescriptionVersion` 3: the `setupDescription` hello declaration is
  negotiated at minor 11 and capped at 3. A peer declaring 1 or 2 retains
  its earlier projected Setup description, including an empty Diagnostics
  category. Version 3 adds only the closed Settings Validation panel in
  `setup.diagnostics`; it does not confer Settings Hygiene authority. That
  panel requires the separate `settingsHygiene:1` hello declaration and
  `settingsHygieneVersion:1` capability. Its Re-validate and Forget actions
  use only the existing `station.validateSettings` and
  `station.forgetSettings` commands with the current canonical MAC. Its
  Repair Invalid Settings action (formerly a disabled Reset to Defaults)
  carries its own gate, `settingsHygieneVersion:2`, and uses
  `station.repairSettings`; below that gate it is disabled with its reason. The panel's result, lifetime,
  confirmation and refusal rules are in the Setup-description contract.
- `settingsBackupVersion`: optional minor-11 capability, 1 only for an
  authenticated enrolled-device-key peer that declared `settingsBackup` 1
  and a Core with a local radio model. It is advertised during capability
  exchange, before `snapshot.complete`; its commands require that marker
  and a still-admitted paired session. Version 1 offers **export only**.
  It does not authorize settings import or include device identity keys,
  private keys, pairing records, or trust files.
- `txDisplayVersion` (remote-window parity Task 28, A11): sent only at
  agreed minor 11, after `stationRadiosVersion` in the minor-11 block
  (`displayClockVersion`, `controlChannelVersion`, then
  `sessionHolderVersion` and the remote transmit entries follow it), and
  1 only while media is on and the Core has a TX analyzer (a Core that
  runs its own DSP); 0 otherwise. At 1 a window may add
  `txDisplayVersion` to its media `start`; then its subscribes may carry
  the transmit window (`txMinDbm`, `txMaxDbm`), every context it is sent
  carries `transmit`, and while the Core is keyed each of its displays on
  the pan hosting the transmitting slice shows the transmit analyzer's
  display instead of the receiver's (the media document's "Transmit
  display"). A window that does not declare it gets today's wire: no
  `transmit`, and receive frames while keyed. (`txState`'s `highSwr` and
  `swrWindBackLatched`, section 18.8, came with this version but do not
  depend on it: every Core that sends `txState` sends them.) Version 2
  (remote-window parity Task 30, A12) is sent under the same condition
  (media on and a TX analyzer) and adds, with no new message: the Core
  applies a window's `settings.write` or `settings.remove` of one of Setup
  > Display > TX Display's nine analyzer keys to its TX analyzer at once
  (section 8.1). A window on a Core that sends 0, 1 or no entry shows those
  nine controls disabled with "This Core does not apply transmit display
  settings from this app. Updating the Core may help.", and keeps the
  page's other groups (the TX waterfall levels, palette, low colour and
  gradient, the TX grid), which are its own. A media `start` still
  declares 1: the transmit display itself did not change. Version 3
  (remote-window parity Task 31, A11) is sent under the same condition and
  adds display duplex (DUP): a window whose Core sends 3 declares 3 in its
  media `start`, and then its `subscribe` may carry `duplex` (boolean,
  absent false); while the Core is keyed an endpoint with `duplex` true is
  not a viewer of the transmit display, keeps its receive frames and is
  sent contexts with `transmit` false, and the device's DUP decides whether
  the Core turns noise blanking off while that device's key is down (the
  media document's "Transmit display", "Display duplex (version 3)"). With
  it the Core calibrates what it sends while keyed as Thetis's RX1Offset
  does (the TX Display Cal Offset on transmit frames; on a `duplex`
  endpoint's receive frames also the receive calibration without its
  preamp and the transmit attenuator applied). No message is added. A window on a Core that sends 0, 1, 2 or no entry shows
  its DUP controls (View > Display duplex (DUP) and the container DUP
  button) disabled with "This Core does not show the receiver while
  transmitting for this app. Updating the Core may help." and its
  transmitting pan behaves as DUP off.
- `miniDisplayVersion`: at agreed minor 11, sent as 1 only to a peer whose
  hello declared `miniDisplay: 1` while media is available. The peer may
  declare version 1 in its media `start` and add `displayRole: "mini"` to a
  display `subscribe`; absent role retains pan behavior. See the media
  control document's "Receiver mini display endpoints". This capability is
  appended after all existing minor-11 fields and omitted for old peers.
- `displayClockVersion` (R-R3-21, R-R3-08): sent only at agreed minor 11,
  after `txDisplayVersion`, and 1 whenever media is on. At 1 every
  display frame's `producerTimestamp` and the `clock-echo`'s `t1`, `t2` and
  `capturedNs` are one Core clock, so a window presents its spectrum and
  waterfall on its audio's playout clock (the remote media control
  document, "Display on the audio's clock"). No message changes shape. On
  a Core that sends 0 or no entry a window draws each display frame on
  arrival, with no delay; its waterfall row queue, gap blending and repeats
  while waiting for a keyframe still apply. Capabilities are read by name:
  entries added later move the position of those after them.
- `controlChannelVersion` (the iPhone app plan's Task 28 fix wave,
  R-IOS-16): sent only at agreed minor 11, after `displayClockVersion` in
  the minor-11 block (`txMonitorAudioVersion`, `stationFreedvVersion`,
  `mediaReplaceVersion`, `controlSwitchVersion`, `relayAllowed`,
  `supportBundleVersion`, then optional `sessionHolderVersion` and remote
  transmit entries, followed by `mediaTunnelVersion` and
  `mediaRelayRoutingVersion`), and 1
  on a Core with a bound
  certificate (section
  3.4). At 1 the Core answers an introduction through the rendezvous with
  this link over a data channel (section 20). A device records the value
  with the paired Core only from an authenticated ordinary-session snapshot.
  A recorded 0 suppresses the service path, with "Update the Core to reach
  it from anywhere.", for five minutes from that observation. A later
  authenticated 0 renews that time; a 1 clears it. The observation time is
  optional in the saved Core record: a legacy 0 without it, a malformed or
  future time, and an expired time make the 0 unknown for connection
  discovery, so the device tries the service again. A real change of local
  network generation invalidates the negative once; duplicate callbacks
  and ordinary retries do not renew it. A successful code pairing to the
  same Core identity also invalidates the old negative. A failed or
  unanswered probe never records or renews 0. The Core identity and
  certificate binding checks (sections 3.4 and 21.1), the relay setting,
  and the existing bounded race and retry schedule still apply. The
  rendezvous registration does not carry this capability (that would change
  the rendezvous version). A device that has had no session with the Core
  yet (one that paired through a mailbox) has no value recorded and tries;
  its first authenticated snapshot records it.
- `txMonitorAudioVersion` (remote-window parity Task 32, R-IOS-13,
  R-R3-49): sent only at agreed minor 11, after `controlChannelVersion` in
  the minor-11 block (`stationFreedvVersion`, then this block's Task 29
  entries and the later minor-11 extensions follow it), and 1 while media is on and
  the Core runs its own
  radio model; 0 otherwise. At 1 a window may add `txMonitorAudioVersion`
  to its media `start` and then send `monitor-audio` with the route it
  wants for the transmit monitor (`speakers`, its main stream;
  `headphones`, its headphones stream, or its main stream when it did not
  declare `headphonesMixVersion`; or `none`), answered by one
  `monitor-audio-context` naming the route as applied (the media
  document's "Transmit monitor (monitor-audio)"). While the radio is on
  the air, MON is on and that window's device holds transmit (`txState`'s
  holder), the Core adds MON at `monitorVolume` to that stream; no other
  device's stream carries it, and while a remote device holds transmit the
  Core's own outputs leave MON out. `monitor-audio` is taken on and off the
  air: it reaches neither the radio nor any device. A window that does not
  declare it gets today's wire and no monitor. A window on a Core that
  sends 0 or no entry shows its MON output pair (SPEAKERS, PHONES) disabled
  with "This Core does not send the transmit monitor. Updating the Core
  may help."; MON itself still turns the Core's monitor on.
- `stationFreedvVersion` (the iPhone app plan's Task 22 and remote-window
  parity Task 20, R-IOS-26, R-R3-49): sent only at agreed minor 11, after
  `txMonitorAudioVersion` in the minor-11 block (`mediaReplaceVersion`,
  `controlSwitchVersion`, `relayAllowed` and the later minor-11 extensions
  follow it), and 2
  whenever `recordStreamVersion` is at least 1 (the Core runs FreeDV Reporter
  itself); 0
  otherwise. At 1 FreeDV Reporter is one of the Core's station sources
  (source name `freedvReporter`): the Core registers with its own
  callsign, grid square and status message, never its label (section
  9.1, "The Core's FreeDV Reporter"), starts it with no window when its
  `FreeDvAutoStart` is on, lists its own RADE slice and shows or hides the
  station as that slice enters or leaves RADE. The Core sends the
  `freedvStations` record stream and the FreeDV Reporter console as
  `spotConsole:freedvReporter` (section 7.7), FreeDV Reporter's state in
  `spotSources` (`freedvReporterState`, `freedvReporterText`,
  `freedvReporterHidden`; section 7.1), and takes `spots.connect` and
  `spots.disconnect` for `freedvReporter` and `freedv.setMessage`,
  `freedv.sendQsy` and `freedv.setHidden` (section 9.1). A window
  subscribes to the two streams only when it sees at least 1, and runs no
  FreeDV Reporter connection of its own. At 2 (the iPhone app plan's Task
  63) each `freedvStations` record also carries `band` (section 7.7): the
  band the desktop's FreeDV Reporter band filter finds for the station's
  frequency (`Band::bandFromFrequency`, the Core calling the desktop's own
  lookup), numbered as the `spots` record numbers its band. Everything 1
  brings is unchanged, and a record from an older Core has no `band`, so
  an app shows its band filter and "follow the radio" by band disabled
  there. This is a revision of the `freedvStations` record this
  capability defines, so it raises this version rather than adding one
  (section 6.3). On a Core that sends 0 or no entry a
  window shows its FreeDV tab's Start and "Hide my station" and the FreeDV
  Reporter dialog's Send QSY, Send and Clear disabled with "This Core does
  not run FreeDV Reporter for this app. Updating the Core may help.".
- `mediaReplaceVersion` (the iPhone app plan's Task 29, R-IOS-16): sent
  only at agreed minor 11, after `stationFreedvVersion`, and 1 whenever
  media is on for that device (as `txDisplayVersion`), 0 otherwise. At 1 the Core takes the media `replace` operation (the
  remote media control document, "Replacing the media connection"). On a
  Core that sends 0 or no entry a device keeps its media connection when
  its session moves (section 21.2), and starts media again only if that
  connection fails.
- `controlSwitchVersion` (Task 29, R-IOS-16): sent only at agreed minor
  11, after `mediaReplaceVersion`, and 1 on every Core that has it. At 1
  the Core takes `session.pathTicket` and `path.join` and moves a session
  to another connection (section 21.2). On a Core that sends 0 or no entry
  a device keeps its session on the path the race chose until it ends.
- `relayAllowed` (Task 29, R-IOS-16): a `bool`, sent only at agreed minor
  11, after `controlSwitchVersion`: false when `nereusd.conf` has `relay =
  deny`, true otherwise (a Core with no rendezvous says true, the
  setting's default). A device records it with the paired Core at each
  sign-in, leaves the relay out of its races while it is false, and says
  so in its attempt record (section 21.1).
- `supportBundleVersion` (remote-window parity Task 22 and the iPhone app
  plan's Task 25, R-R3-49, R-IOS-18): sent only at agreed minor 11, after
  `relayAllowed` in the minor-11 block (`sessionHolderVersion` and the
  remote transmit entries follow it), and 1 on every Core with a radio
  model. At 1 the Core takes `support.collect` (its support bundle) and
  `support.setLogCategories` (section 9.1), sends the `coreLog` record
  stream (section 7.7) and `radio`'s `logCategories` (section 7.1). A
  window's Tools > Support Bundle... then carries the Core's bundle beside
  this computer's, its logging checkboxes turn the Core's categories on
  and off, and Setup > Diagnostics > Logs shows the Core's recent log
  above this computer's. On a Core that sends 0 or no entry a window shows
  those checkboxes disabled and the Core's log in their place with "This
  Core does not share its log or support bundle with this app. Updating
  the Core may help.", and its bundle carries `core/unavailable.txt` with
  that reason. Neither verb reaches the radio, so each is answered on and
  off the air, as a window at the Core does both while keyed; the Core
  writes the bundle on a worker thread and logs through a queue no
  logging thread waits on, so neither can stall the radio.
- `txModMonitorVersion` (R-IOS-13, R-R3-49; the iPhone app plan's Task 39,
  parity row A10): sent only at agreed minor 11, appended after `remoteIqVersion`
  and all earlier optional entries, preserving existing positions; 1 whenever `recordStreamVersion`
  is at least 1 (the Core runs its own AM modulation analyzers); 0 otherwise. At 1
  the Core sends the AM Mod Monitor's readings on the `txAmModulation`
  (the transmit I/Q it sends its radio) and `txAmModulationFeedback` (the
  PureSignal feedback receiver, the PA's output) record streams (section
  7.7), takes `txModMonitor.reset` (section 9.1), and applies a window's
  `ModMon/FbStream` (section 8.1). A window subscribes to one of the two
  streams only while its Mod Monitor is shown, and again after each
  snapshot. On a Core that sends 0 or no entry a window shows its Mod
  Monitor, docked or popped out, disabled with "This Core does not send
  the modulation monitor. Updating the Core may help.", its readings
  blank; with no Core connected it says "Connect to the Core to see the
  modulation monitor.".

- `mediaTunnelVersion` (Task 29 step 2b): 1 when media is on, after every
  previously emitted minor-11 entry (including conditional transmit
  readings). This unpublished field was ordered here at trunk integration
  so existing field positions stay unchanged. A media `start` may declare
  version 1 even if the session
  currently uses the rendezvous data channel. The media connection uses the
  direct WebSocket tunnel only while that session path carries binary
  messages; a later path move can therefore use the declared tunnel on its
  replacement. Its tag-2 payload is a 16-byte RFC 4122 `connectionId` UUID
  followed by the unchanged ICE-agent datagram.
- `mediaRelayRoutingVersion` (Task 29 step 2b): 1 when media is on, after
  `mediaTunnelVersion`. A media `start` declaring version 1 enables the same
  UUID prefix on the web-relay leg's tag-2 payload. An older peer omitting
  the declaration keeps the original raw tag-2 payload. Replacement
  inherits the start's mode and retains its exact three-field shape. A
  legacy raw leg currently carrying media cannot overlap a replacement on
  that leg, so the Core refuses that move before changing the live route.
  The UUID selects a local media generation; DTLS still authenticates the
  peer. A routed payload may hold at most 1484 bytes of agent datagram,
  preserving the relay's existing 1501-byte frame limit and outer tags.
  Each claimed local loopback route pins the source address and port of its
  first well-formed agent datagram. Later datagrams from another local
  source are dropped rather than retargeting the return route; a new agent
  needs a new claim. This avoids accidental or stale local traffic changing
  a live route. A local process that races the first datagram is not
  authenticated by this pin; ICE/DTLS retains its peer checks.

- `sessionHolderVersion`: sent only at agreed minor 11, after
  `supportBundleVersion` and before the trailing media-floor versions, and only to
  a peer whose hello declared `sessionHolder` 1 with `deviceAuth` 1; any
  other peer is sent no entry (and reads 0), so its capabilities are
  today's. 1: up to four devices at once, the `connectedDevices` object
  (section 7.1), `session.leave` (section 9.1), and sharing the radio's
  receivers: `confirm.request` and `notice`, and the verbs
  `confirm.proceed`, `confirm.cancel` and `notice.takeBack` (section 7.5).
  The table above shows the value a declaring peer is sent.
- While several devices are on a Core, media and telemetry go to one of
  them (section 11): any other is sent `remoteMediaVersion`,
  `remoteWidebandDisplayVersion`, `remoteAudioStatusVersion`,
  `spectrumGrantVersion`, `audioProfileVersion`, `audioClockVersion`,
  `receiverAudioVersion`, `headphonesMixVersion`, `psDisplayVersion`,
  `displayExtrasVersion` and `stationTelemetryVersion` as 0 and no display
  budget. A Core with one device on it sends that device what the table
  says.

- `remoteTxVersion`: sent only at agreed minor 11, before the trailing
  media-floor versions, and only to a
  peer whose hello declared `remoteTx` 1; any other peer is sent no entry
  (and reads 0). 1: `txPermitted` is the station transmit gate's answer
  for that session, `tx.setTxSlice` and the keying verbs `tx.key`,
  `tx.unkey`, `tx.tune` and `tx.twoTone` (section 9.1), and the refusals
  of section 18. 2 (iPhone app plan Task 77): taking transmit, `tx.take`
  (with `sessionHolderVersion` 1), its `takeTransmit` question and
  `transmitTaken` notice (sections 7.5 and 18.9), the radio's own PTT
  taking transmit, the transmitter's settings held by the holder, and
  `tx.tunerTune`, the Tuner Genius autotune (section 18.6). A client sends
  `tx.take` or `tx.tunerTune` only to a Core that sent 2 or more. The table
  above shows the value a declaring peer is sent.
- `txRefusalCode`, `txRefusalReason`, `txRefusalFix` (utf8, desktop remote
  transmit): sent right after `remoteTxVersion` and only with it. While
  `txPermitted` is false they are the gate's refusal for that session
  (section 18.3: its code, its sentence as the operator reads it, and its
  fix or empty); while it is true all three are empty. A client shows the
  sentence on its disabled transmit controls as sent.
- `txStateVersion` (iPhone app plan Task 39): sent after `remoteTxVersion`
  and its three `txRefusal` entries, and only with them. 1: the Core sends
  the read-only `txState` object (section 18.8) to that peer; any other
  peer never sees it or its schema.
- `txReadingsVersion` (remote-window parity Task 33, R-R3-49, R-R3-32):
  sent right after `txStateVersion` and only with it. 2 on a Core with its
  own radio model and record streams (`recordStreamVersion` at least 1), 0
  otherwise. At 1 `txState` also carries `forwardAdcRaw` and
  `reflectedAdcRaw`, the radio's raw forward and reflected power readings,
  and `compressionDb`, the COMP reading (section 18.8), and the Core keeps the `txCfcCompression` record stream,
  the CFC bar chart's data (section 7.7). A window's PA Values page and
  CFC dialog show their transmit readings from these; on a Core that sends
  0 or no entry each shows "This Core does not send this reading. Updating
  the Core may help.", never a 0. At 2 the Core also sends its own scaled
  raw forward power (W) and forward/reverse ADC voltage (V), each as outbound
  f64. An older client using the version-1 fields keeps its existing behavior.
  At 3 (A9, iPhone app plan Task 39) `txState` also carries the seven stage
  readings a local window's container meters show, `eqDb`, `levelerDb`,
  `levelerGainDb`, `cfcDb`, `cfcGainDb`, `alcGainDb` and `alcGroupDb`
  (section 18.8). A window below 3 shows each of those meters disabled
  with "This Core does not send this reading. Updating the Core may
  help.", never hidden and never a 0.

`txPermitted` (iPhone app plan Task 34) is true only for a session the
station transmit gate permits (section 18.1): false until
`snapshot.complete` has been sent to it, false for a peer whose hello did
not declare `remoteTx`, false for every peer while the Core's
`remote_transmit` is deny, and false while another device holds transmit.
A session learns a change from a new `capabilities` message (section 5.2);
nothing else is sent again. A peer that declared `remoteTx` is also sent a
new `capabilities` when only the refusal changes (for example from
"Transmit is changing hands." to "<holder> has the transmitter."). An older window that reads the flag without
declaring `remoteTx` therefore never sees it true.

**The direct media ladder.** A client with media at agreed minor 11 that
declared `mediaDirect` 1 (section 6.1) is sent two more entries, after
`sliceAccessVersion` (or after the last entry before it when that is
absent) and before `coreBuildInfo`:
`mediaDirectVersion`, an `i64`, 1 on every Core that has it, and
`mediaStunUrls`, a `utf8` compact JSON array of the Core's STUN servers
(the ones its rendezvous gave it; `[]` when it has none). Only `stun:` and
`stuns:` URLs ever appear: never a TURN server, a credential or a token (a
URL carrying `@` or `?` is dropped). The Core sends `capabilities` again
when the list changes. A client uses the list for the ICE of its first
media connection and of a direct replace, falls back to the last STUN
server its rendezvous gave it when the list is empty or absent, and never
stores it. At `mediaDirectVersion` 1 the Core takes the media `replace`
operation with `mediaDirectVersion` 1: a direct-only connection that
gathers STUN and host candidates and neither the media tunnel nor the
relay. A peer that did not declare the feature is sent neither entry.

**RX2's input control.** A client that declared `rx2Attenuator` 1 is sent
`rx2AttenuatorVersion`, an `i64`, 1, after the direct media ladder (or after
the last entry before it when that is absent) and before `coreBuildInfo`.
At 1 the catalogue's `board` carries `rx2Attenuator`, `rx2PreampItems` and
`rx2AttenuatorReason` (section "Catalogue"). A peer that did not declare
the feature is sent no entry; the catalogue keys are sent to every peer,
and an app that does not know them ignores them.

**The radio's mic input.** A client that declared `radioMic` 1 is sent
`radioMicVersion`, an `i64`, 1, after `rx2AttenuatorVersion` (or after the
last entry before it when that is absent) and before `coreBuildInfo`. At 1
the catalogue's `board` carries `radioMic` and `radioMicNote` (section
"Catalogue"). A peer that did not declare the feature is sent no entry; the
catalogue keys are sent to every peer, and an app that does not know them
ignores them.

**The receive low-pass on a shared input.** A client that declared
`rxFilterLowPass` 1 is sent `rxFilterLowPassVersion`, an `i64`, 1, after
`radioMicVersion` (or after the last entry before it when that is absent)
and before `coreBuildInfo`. At 1 the `radio` object carries
`rxFilter0LowPassReason` and `rxFilter0LowPassSlice` (section 7). A peer
that did not declare the feature is sent no entry and neither property.

**The RADE reason.** A client that declared `radeReason` 1 is sent
`radeReasonVersion`, an `i64`, 1, after `rxFilterLowPassVersion` (or after
the last entry before it when that is absent) and before `coreBuildInfo`.
At 1 each `slice:<id>` object carries `radeReason` (section 7). A peer
that did not declare the feature is sent no entry and no `radeReason`.

**Core executable identity.** A client at agreed minor 11 may declare
`coreBuildInfo` 1. After authentication, a Core with a known product version
appends one optional `coreBuildInfo` utf8 capability after all existing
entries. Its value is compact JSON with required string `productVersion`
(1–128 UTF-8 bytes) and string `sourceTag` (0–1024 UTF-8 bytes); the whole
JSON is at most 4096 bytes, and neither string may contain control
characters. Clients ignore unknown JSON keys. Invalid, duplicate, or absent
entries mean the Core's identity is unavailable, without affecting the
session. An empty source tag means this executable is untagged; it does not
prove the executable was a release build. These fields identify the Core
binary, separately from the radio's `firmwareVersion` and settings schema.

### 6.4 The capabilities message

`capabilities` carries property entries (section 4.1) in the order below.
The display budget entries (`displayApplicationBytesPerSecond`,
`spectrumSampleUnitsPerSecond`, `displayBudgetGeneration`,
`remotePs3DisplaySubscribed`, `displayBudgetReason`) are present only with a
usable budget, and `displayBudgetReason` only at agreed minor 11. The radio
identity entries from `hpsdrModel` onwards are present only at agreed minor
11, and `sessionHolderVersion`, last, only for a peer that declared
`sessionHolder` (section 6.1); `vaxVersion` only for a peer that declared
`vax` (section 6.1); `txEqCurveVersion` only for a peer that
declared `txEqCurve` (section 6.1); `remoteTxVersion` and the three
`txRefusal` entries after it only for a peer that declared `remoteTx`;
`diversityPatternVersion` only for a peer that declared
`diversityPattern`; `logCategoryListVersion` only for a peer that declared
`logCategoryList`; `radioModelsVersion` only for a peer that declared
`radioModels`; `coreAddressesVersion` only for a device signed in with its
own key that declared `coreAddresses`; `adcAttenuatorVersion` only for
a peer that declared `adcAttenuators`; `paProfileVersion` only for a
peer that declared `paProfiles` (section 6.1); `radeStatusVersion` only
for a peer that declared `radeStatus`; `txInhibitReasonVersion` only for a
peer that declared `txInhibitReason` (section 6.1); `paTransmitBandVersion` only for a
peer that declared `paTransmitBand`; `radeReasonVersion` only for a peer
that declared `radeReason`. A client ignores a capability it does not know
(`StationCapabilities::fromUpdates`).

**Each device's share of the display budget** (iPhone app plan Task 76; the
several-devices design, ruling 9.3 and design ruling 9.3a). The Core has one
display budget, its total: the display allowance its configuration sets, or
its computed ceiling, lowered by the load governor when the Core computer is
short of processing time. Every admitted session with media is given its own
share of that total (`DisplayBudgetSplit`), and its capabilities carry that
share in the budget entries: `displayApplicationBytesPerSecond` and
`spectrumSampleUnitsPerSecond` are the device's own, and
`displayBudgetGeneration` is the device's own generation. A share that does
not change keeps its generation; one that does takes the total's generation
when that is newer, otherwise the device's last plus one, so a device alone
on the Core sees exactly the generations it saw before shares existed. When
a device is admitted or leaves, the total changes, or a device's displays
ask for more or less, every device whose share or reason changed is sent
`capabilities` again. The rules of the
split: the PureSignal display's charge comes off the total once and belongs
to the device that subscribed to it (`remotePs3DisplaySubscribed` is true
for that device only); a network device holding transmit gets its whole
request, the rest is shared among the others; with transmit unheld, held by
the station device or held by a device that is away, every device gets an
equal share, and a device asking for less leaves the difference to the
rest (max-min fair). A device's request is what its displays ask for: the
sum of its display subscriptions' charges at the frame rate it subscribed
at and the pixels it subscribed at, clamped to what its window can carry
(the receiver's bins in that window), before the budget clamps them, a
subscription refused for the
budget included, until the display is closed, asked for again, or 10 s
pass after its refusal without either (the media control document's
display budget section), and never
less than one useful pan (256 pixels at 10 frames a second with its wide
plane). What that guarantees: with no network device holding transmit
while present, each device's share is at least the smaller of one useful
pan and an equal part of the total; the Core's own cut for a busy computer
never takes the total below PureSignal's display plus one useful pan per
device, so under that cut each device keeps one pan. Only a display
allowance configured below one pan per device can leave each device less.
Beside a device holding transmit, the others share only what its request
leaves, which can be nothing useful (a share of 1) while it asks for the
whole total. What no device asks for is shared equally among them as room to grow,
so a device alone has the whole total. A subscription is admitted against the share the
device has once it asks for it (for the transmit holder, its whole
request), not the share it had. **Transmit joins here:** until the transmit
holder exists (Task 34) transmit counts as unheld; Task 34 names the holder.
The Core hands each device a share, never a frame rate: each client plans
its own displays inside its share, and a share too small for one pan at 256
pixels and 10 frames a second suspends that device's display, pane and
slice kept. Every client, holding transmit or not, first subscribes its
displays at the pixels and frame rate the operator wants, since its request
is what gives it its share. A subscription refused because it does not fit
(reason "The Core's display limit has no room left.") is the answer: the
`capabilities` with the device's new share arrive before the refusal, and
the client plans inside that share and subscribes again. It asks for what it
wants again when that grows (a pane added, widened or sped up; a pane
widened by a resize asks once its width has held for 200 ms, not at every
step of the resize) and when the
transmit holder changes (`txState`'s `holderEpoch` or `holderAway` moves: it
takes transmit, the holder changes, or a holder lets go or goes away), never
merely because a new generation arrived. Audio is never split and never cut
when the Core runs short.

`displayBudgetReason` says which of the Core's limits is short:

| Value | Meaning |
| --- | --- |
| `none` | the device's share is not below its request, and nothing is cut |
| `coreBusy` | the governor has cut the total, and the device is alone on the Core (or its share still covers its request) |
| `sharedConnection` | another device is admitted and this device's share is below its request; the governor has cut nothing (the devices share what the Core sends) |
| `sharedProcessing` | another device is admitted and this device's share is below its request; the governor's cut is in force (the Core computer is short of processing time) |

A device's request is its demand as above (what its displays ask for, at
least one useful pan). Because every client first asks for what the
operator wants, a device that wants more than its share while another
device is admitted hears `sharedConnection` or `sharedProcessing`; one
whose share covers all it asks for hears `none` or `coreBusy`.

`sharedConnection` and `sharedProcessing` go only to a device that declared
`sessionHolder` (section 6.1). Any other device is told `none` in place of
`sharedConnection` and `coreBusy` in place of `sharedProcessing`, so an
older window sees only the values it was built for.

<!-- surface:capabilities -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Order | Name | Wire kind |
| --- | --- | --- |
| 1 | `stationName` | `utf8` |
| 2 | `radioModel` | `utf8` |
| 3 | `firmwareVersion` | `utf8` |
| 4 | `macAddress` | `utf8` |
| 5 | `board` | `i64` |
| 6 | `radioConnected` | `bool` |
| 7 | `effectiveMaxSlices` | `i64` |
| 8 | `boardMaxSlices` | `i64` |
| 9 | `userDdcCount` | `i64` |
| 10 | `pureSignalPresent` | `bool` |
| 11 | `txPermitted` | `bool` |
| 12 | `remoteMediaVersion` | `i64` |
| 13 | `remoteWidebandDisplayVersion` | `i64` |
| 14 | `remoteAudioStatusVersion` | `i64` |
| 15 | `spectrumGrantVersion` | `i64` |
| 16 | `remoteDisplayBudgetVersion` | `i64` |
| 17 | `remoteCtunVersion` | `i64` |
| 18 | `stationTelemetryVersion` | `i64` |
| 19 | `remoteTgxlConfigVersion` | `i64` |
| 20 | `remoteFourO3AControlVersion` | `i64` |
| 21 | `wdspVersion` | `i64` |
| 22 | `wdspCompatibilityVersion` | `i64` |
| 23 | `nnrVersion` | `i64` |
| 24 | `psAlgorithmVersion` | `i64` |
| 25 | `propertyResultVersion` | `i64` |
| 26 | `dspAssetVersion` | `i64` |
| 27 | `psDisplayVersion` | `i64` |
| 28 | `notchControlVersion` | `i64` |
| 29 | `audioProfileVersion` | `i64` |
| 30 | `audioClockVersion` | `i64` |
| 31 | `receiverAudioVersion` | `i64` |
| 32 | `headphonesMixVersion` | `i64` |
| 33 | `settingsSchemaVersion` | `i64` |
| 34 | `displayApplicationBytesPerSecond` | `i64` |
| 35 | `spectrumSampleUnitsPerSecond` | `i64` |
| 36 | `displayBudgetGeneration` | `i64` |
| 37 | `remotePs3DisplaySubscribed` | `bool` |
| 38 | `displayBudgetReason` | `utf8` |
| 39 | `hpsdrModel` | `i64` |
| 40 | `radioProtocol` | `i64` |
| 41 | `radioAddress` | `utf8` |
| 42 | `radioHardwareVersion` | `i64` |
| 43 | `remotePgxlControlVersion` | `i64` |
| 44 | `remoteRfKitControlVersion` | `i64` |
| 45 | `stationTciVersion` | `i64` |
| 46 | `accessoryDataVersion` | `i64` |
| 47 | `remoteTgxlControlVersion` | `i64` |
| 48 | `stationIdentityVersion` | `i64` |
| 49 | `deviceAdminVersion` | `i64` |
| 50 | `pairingVersion` | `i64` |
| 51 | `stationCatalogVersion` | `i64` |
| 52 | `displayExtrasVersion` | `i64` |
| 53 | `transmitSettingsVersion` | `i64` |
| 54 | `bandSelectVersion` | `i64` |
| 55 | `meterReadingsVersion` | `i64` |
| 56 | `dspInfoVersion` | `i64` |
| 57 | `recordStreamVersion` | `i64` |
| 58 | `stationRadiosVersion` | `i64` |
| 59 | `txDisplayVersion` | `i64` |
| 60 | `displayClockVersion` | `i64` |
| 61 | `controlChannelVersion` | `i64` |
| 62 | `txMonitorAudioVersion` | `i64` |
| 63 | `stationFreedvVersion` | `i64` |
| 64 | `mediaReplaceVersion` | `i64` |
| 65 | `controlSwitchVersion` | `i64` |
| 66 | `relayAllowed` | `bool` |
| 67 | `supportBundleVersion` | `i64` |
| 68 | `sessionHolderVersion` | `i64` |
| 69 | `remoteTxVersion` | `i64` |
| 70 | `txRefusalCode` | `utf8` |
| 71 | `txRefusalReason` | `utf8` |
| 72 | `txRefusalFix` | `utf8` |
| 73 | `txStateVersion` | `i64` |
| 74 | `txReadingsVersion` | `i64` |
| 75 | `mediaTunnelVersion` | `i64` |
| 76 | `mediaRelayRoutingVersion` | `i64` |
| 77 | `settingsHygieneVersion` | `i64` |
| 78 | `settingsBackupVersion` | `i64` |
| 79 | `remoteIqVersion` | `i64` |
| 80 | `txModMonitorVersion` | `i64` |
| 81 | `setupDescriptionVersion` | `i64` |
| 82 | `miniDisplayVersion` | `i64` |
| 83 | `accessoryTxVersion` | `i64` |
| 84 | `radioAntennaRowsVersion` | `i64` |
| 85 | `vaxVersion` | `i64` |
| 86 | `txEqCurveVersion` | `i64` |
| 87 | `band2mVersion` | `i64` |
| 88 | `diversityPatternVersion` | `i64` |
| 89 | `logCategoryListVersion` | `i64` |
| 90 | `radioModelsVersion` | `i64` |
| 91 | `coreAddressesVersion` | `i64` |
| 92 | `audioQualityVersion` | `i64` |
| 93 | `stationTciSettingsVersion` | `i64` |
| 94 | `adcAttenuatorVersion` | `i64` |
| 95 | `paProfileVersion` | `i64` |
| 96 | `radeStatusVersion` | `i64` |
| 97 | `txInhibitReasonVersion` | `i64` |
| 98 | `paTransmitBandVersion` | `i64` |
| 99 | `sliceAccessVersion` | `i64` |
| 100 | `mediaDirectVersion` | `i64` |
| 101 | `mediaStunUrls` | `utf8` |
| 102 | `rx2AttenuatorVersion` | `i64` |
| 103 | `radioMicVersion` | `i64` |
| 104 | `rxFilterLowPassVersion` | `i64` |
| 105 | `radeReasonVersion` | `i64` |
| 106 | `coreBuildInfo` | `utf8` |

<!-- /surface -->

## 7. Objects and properties

### 7.1 Classes, schemas and keys

A `schema` message names a class and its fields: each field's ordinal, name
and wire kind. The station mirrors these classes; a property's direction is
`outbound` (station to client only; a write is refused), `bidirectional`
(the client may write it) or `constantSnapshot` (sent in `object.create`
only, never in a delta, never written; `SliceModel`'s `sliceIndex` is one).
An enum property lists the values its domain allows.

<!-- surface:mirrorClasses -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

**AccessoryDataModel** (48 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `faultRevision` | `i64` | outbound |  |
| 1 | `pgxlFaults` | `utf8` | outbound |  |
| 2 | `tgxlFaults` | `utf8` | outbound |  |
| 3 | `rfkitFaults` | `utf8` | outbound |  |
| 4 | `pgxlConnectedSinceMs` | `i64` | outbound |  |
| 5 | `pgxlLastRttMs` | `i64` | outbound |  |
| 6 | `pgxlKeepaliveMissed` | `i64` | outbound |  |
| 7 | `pgxlReconnectCount` | `i64` | outbound |  |
| 8 | `pgxlFramesIn` | `i64` | outbound |  |
| 9 | `pgxlFramesOut` | `i64` | outbound |  |
| 10 | `pgxlBytesIn` | `i64` | outbound |  |
| 11 | `pgxlBytesOut` | `i64` | outbound |  |
| 12 | `pgxlLastFrameMs` | `i64` | outbound |  |
| 13 | `pgxlFaultsSession` | `i64` | outbound |  |
| 14 | `tgxlConnectedSinceMs` | `i64` | outbound |  |
| 15 | `tgxlLastRttMs` | `i64` | outbound |  |
| 16 | `tgxlKeepaliveMissed` | `i64` | outbound |  |
| 17 | `tgxlReconnectCount` | `i64` | outbound |  |
| 18 | `tgxlFramesIn` | `i64` | outbound |  |
| 19 | `tgxlFramesOut` | `i64` | outbound |  |
| 20 | `tgxlBytesIn` | `i64` | outbound |  |
| 21 | `tgxlBytesOut` | `i64` | outbound |  |
| 22 | `tgxlLastFrameMs` | `i64` | outbound |  |
| 23 | `tgxlFaultsSession` | `i64` | outbound |  |
| 24 | `interlockMode` | `enum` | outbound | 0, 1, 2 |
| 25 | `interlockGraceMs` | `i64` | outbound |  |
| 26 | `interlockSwrGateEnabled` | `bool` | outbound |  |
| 27 | `interlockSwrGateMax` | `f64` | outbound |  |
| 28 | `powerCapEnabled` | `bool` | outbound |  |
| 29 | `powerCapW` | `i64` | outbound |  |
| 30 | `powerCapExceeded` | `bool` | outbound |  |
| 31 | `powerCapAlertText` | `utf8` | outbound |  |
| 32 | `powerCapAlertCount` | `i64` | outbound |  |
| 33 | `tuneMemory` | `utf8` | outbound |  |
| 34 | `autoTuneMemoryRecall` | `bool` | outbound |  |
| 35 | `tgxlAntenna1Label` | `utf8` | outbound |  |
| 36 | `tgxlAntenna2Label` | `utf8` | outbound |  |
| 37 | `tgxlAntenna3Label` | `utf8` | outbound |  |
| 38 | `rfkitAntenna1Label` | `utf8` | outbound |  |
| 39 | `rfkitAntenna2Label` | `utf8` | outbound |  |
| 40 | `rfkitAntenna3Label` | `utf8` | outbound |  |
| 41 | `rfkitAntenna4Label` | `utf8` | outbound |  |
| 42 | `rfkitConnectedSinceMs` | `i64` | outbound |  |
| 43 | `rfkitPollsOk` | `i64` | outbound |  |
| 44 | `rfkitPollsFailed` | `i64` | outbound |  |
| 45 | `rfkitReconnectCount` | `i64` | outbound |  |
| 46 | `rfkitLastPollMs` | `i64` | outbound |  |
| 47 | `rfkitRttAvgMs` | `i64` | outbound |  |

**AccessorySettingsModel** (21 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `pgxlNickname` | `utf8` | outbound |  |
| 1 | `pgxlBiasMode` | `utf8` | outbound |  |
| 2 | `pgxlFanMode` | `utf8` | outbound |  |
| 3 | `pgxlLedIntensity` | `i64` | outbound |  |
| 4 | `pgxlNetworkKnown` | `bool` | outbound |  |
| 5 | `pgxlDhcp` | `bool` | outbound |  |
| 6 | `pgxlAddress` | `utf8` | outbound |  |
| 7 | `pgxlNetmask` | `utf8` | outbound |  |
| 8 | `pgxlGateway` | `utf8` | outbound |  |
| 9 | `pgxlAnswer` | `utf8` | outbound |  |
| 10 | `pgxlAnswerAccepted` | `bool` | outbound |  |
| 11 | `pgxlAnswerCount` | `i64` | outbound |  |
| 12 | `tgxlNickname` | `utf8` | outbound |  |
| 13 | `tgxlNetworkKnown` | `bool` | outbound |  |
| 14 | `tgxlDhcp` | `bool` | outbound |  |
| 15 | `tgxlAddress` | `utf8` | outbound |  |
| 16 | `tgxlNetmask` | `utf8` | outbound |  |
| 17 | `tgxlGateway` | `utf8` | outbound |  |
| 18 | `tgxlAnswer` | `utf8` | outbound |  |
| 19 | `tgxlAnswerAccepted` | `bool` | outbound |  |
| 20 | `tgxlAnswerCount` | `i64` | outbound |  |

**AlexAntennaFacade** (10 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `rxAntennas` | `utf8` | bidirectional |  |
| 1 | `rxOnlyAntennas` | `utf8` | bidirectional |  |
| 2 | `useTxAntennaForRx` | `bool` | bidirectional |  |
| 3 | `txAntennas` | `utf8` | bidirectional |  |
| 4 | `blockTxAnt2` | `bool` | bidirectional |  |
| 5 | `blockTxAnt3` | `bool` | bidirectional |  |
| 6 | `rxOutOnTx` | `bool` | bidirectional |  |
| 7 | `ext1OutOnTx` | `bool` | bidirectional |  |
| 8 | `ext2OutOnTx` | `bool` | bidirectional |  |
| 9 | `rxOutOverride` | `bool` | bidirectional |  |

**AmplifierModel** (20 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `connectionPhase` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7 |
| 1 | `configuredHost` | `utf8` | outbound |  |
| 2 | `configuredPort` | `i64` | outbound |  |
| 3 | `connectionError` | `utf8` | outbound |  |
| 4 | `deviceModel` | `utf8` | outbound |  |
| 5 | `deviceSerial` | `utf8` | outbound |  |
| 6 | `deviceVersion` | `utf8` | outbound |  |
| 7 | `deviceNickname` | `utf8` | outbound |  |
| 8 | `present` | `bool` | outbound |  |
| 9 | `state` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7 |
| 10 | `deviceState` | `utf8` | outbound |  |
| 11 | `operate` | `bool` | outbound |  |
| 12 | `transmitting` | `bool` | outbound |  |
| 13 | `forwardPowerW` | `f64` | outbound |  |
| 14 | `swr` | `f64` | outbound |  |
| 15 | `temperatureC` | `f64` | outbound |  |
| 16 | `mainsVoltageV` | `f64` | outbound |  |
| 17 | `drainCurrentA` | `f64` | outbound |  |
| 18 | `efficiencyText` | `utf8` | outbound |  |
| 19 | `bandFollow` | `enum` | outbound | 0, 1, 2, 3 |

**ConnectedDevicesFacade** (3 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `listJson` | `utf8` | outbound |  |
| 1 | `revision` | `i64` | outbound |  |
| 2 | `deviceLimit` | `i64` | outbound |  |

**DspAssetService** (12 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `nnrStandardAsset` | `utf8` | outbound |  |
| 1 | `nnrPremiumAsset` | `utf8` | outbound |  |
| 2 | `nnrModelSelectionPending` | `bool` | outbound |  |
| 3 | `nnrModelStatus` | `utf8` | outbound |  |
| 4 | `selectionRevision` | `i64` | outbound |  |
| 5 | `nr3ModelAsset` | `utf8` | outbound |  |
| 6 | `nr3ModelStatus` | `utf8` | outbound |  |
| 7 | `nr3Runnable` | `bool` | outbound |  |
| 8 | `dfnrModelStatus` | `utf8` | outbound |  |
| 9 | `dfnrRunnable` | `bool` | outbound |  |
| 10 | `mnrStatus` | `utf8` | outbound |  |
| 11 | `mnrRunnable` | `bool` | outbound |  |

**IoBoardHl2Facade** (4 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `detected` | `bool` | outbound |  |
| 1 | `hardwareVersion` | `i64` | outbound |  |
| 2 | `registers` | `utf8` | outbound |  |
| 3 | `outputs` | `i64` | outbound |  |

**NotchModel** (4 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `listJson` | `utf8` | outbound |  |
| 1 | `revision` | `i64` | outbound |  |
| 2 | `globalEnabled` | `bool` | bidirectional |  |
| 3 | `autoIncrease` | `bool` | bidirectional |  |

**PaProfilesFacade** (2 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `json` | `utf8` | outbound |  |
| 1 | `revision` | `i64` | outbound |  |

**PanadapterModel** (4 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `centerFrequency` | `f64` | bidirectional |  |
| 1 | `bandwidth` | `f64` | bidirectional |  |
| 2 | `dBmFloor` | `i64` | bidirectional |  |
| 3 | `dBmCeiling` | `i64` | bidirectional |  |

**PureSignalSessionFacade** (6 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `available` | `bool` | outbound |  |
| 1 | `canActuate` | `bool` | outbound |  |
| 2 | `twoToneOn` | `bool` | outbound |  |
| 3 | `statusJson` | `utf8` | outbound |  |
| 4 | `lastActionError` | `utf8` | outbound |  |
| 5 | `displayGeneration` | `i64` | outbound |  |

**PureSignalSettings** (10 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `autoCalEnabled` | `bool` | bidirectional |  |
| 1 | `runCalibrationProcessing` | `bool` | bidirectional |  |
| 2 | `autoAttenuate` | `bool` | bidirectional |  |
| 3 | `quickAttenuate` | `bool` | bidirectional |  |
| 4 | `moxDelaySeconds` | `f64` | bidirectional |  |
| 5 | `loopDelaySeconds` | `f64` | bidirectional |  |
| 6 | `requestedTxDelayNs` | `f64` | bidirectional |  |
| 7 | `hardwarePeakOverrideEnabled` | `bool` | bidirectional |  |
| 8 | `hardwarePeakOverride` | `f64` | bidirectional |  |
| 9 | `lastLoadError` | `utf8` | outbound |  |

**RadioModel** (37 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `settingsSaveError` | `utf8` | outbound |  |
| 1 | `receiveLayoutRestoreState` | `utf8` | outbound |  |
| 2 | `receiveLayoutRestoreMessage` | `utf8` | outbound |  |
| 3 | `name` | `utf8` | outbound |  |
| 4 | `model` | `utf8` | outbound |  |
| 5 | `version` | `utf8` | outbound |  |
| 6 | `connected` | `bool` | outbound |  |
| 7 | `rfKitEnabled` | `bool` | outbound |  |
| 8 | `fourO3AEnabled` | `bool` | outbound |  |
| 9 | `fourO3AListening` | `bool` | outbound |  |
| 10 | `fourO3AListenerError` | `utf8` | outbound |  |
| 11 | `rxFilter0Mode` | `i64` | outbound |  |
| 12 | `rxFilter0Effective` | `i64` | outbound |  |
| 13 | `rxFilter0Band` | `i64` | outbound |  |
| 14 | `rxFilter0Reason` | `utf8` | outbound |  |
| 15 | `rxFilter1Mode` | `i64` | outbound |  |
| 16 | `rxFilter1Effective` | `i64` | outbound |  |
| 17 | `rxFilter1Band` | `i64` | outbound |  |
| 18 | `rxFilter1Reason` | `utf8` | outbound |  |
| 19 | `transmitting` | `bool` | outbound |  |
| 20 | `txInhibited` | `bool` | outbound |  |
| 21 | `bandOutputsByte` | `i64` | outbound |  |
| 22 | `bandOutputsBand` | `i64` | outbound |  |
| 23 | `bandOutputsKeyed` | `bool` | outbound |  |
| 24 | `dspOptionsLastApplyMs` | `i64` | outbound |  |
| 25 | `stationRadioWaiting` | `utf8` | outbound |  |
| 26 | `logCategories` | `utf8` | outbound |  |
| 27 | `logCategoryList` | `utf8` | constantSnapshot |  |
| 28 | `txInhibitReason` | `utf8` | outbound |  |
| 29 | `alexLpfBits` | `i64` | outbound |  |
| 30 | `paTransmitBand` | `i64` | outbound |  |
| 31 | `levelCalRunning` | `bool` | outbound |  |
| 32 | `levelCalPercent` | `i64` | outbound |  |
| 33 | `levelCalMessage` | `utf8` | outbound |  |
| 34 | `levelCalSucceeded` | `bool` | outbound |  |
| 35 | `rxFilter0LowPassReason` | `utf8` | outbound |  |
| 36 | `rxFilter0LowPassSlice` | `i64` | outbound |  |

**RfKitModel** (30 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `connectionPhase` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7 |
| 1 | `configuredHost` | `utf8` | outbound |  |
| 2 | `configuredPort` | `i64` | outbound |  |
| 3 | `connectionError` | `utf8` | outbound |  |
| 4 | `deviceModel` | `utf8` | outbound |  |
| 5 | `deviceSerial` | `utf8` | outbound |  |
| 6 | `deviceVersion` | `utf8` | outbound |  |
| 7 | `deviceNickname` | `utf8` | outbound |  |
| 8 | `present` | `bool` | outbound |  |
| 9 | `operate` | `bool` | outbound |  |
| 10 | `forwardPowerW` | `f64` | outbound |  |
| 11 | `reflectedPowerW` | `f64` | outbound |  |
| 12 | `swr` | `f64` | outbound |  |
| 13 | `temperatureC` | `f64` | outbound |  |
| 14 | `voltageV` | `f64` | outbound |  |
| 15 | `currentA` | `f64` | outbound |  |
| 16 | `operationalInterface` | `utf8` | outbound |  |
| 17 | `antennaPresentMask` | `i64` | outbound |  |
| 18 | `antennaDisabledMask` | `i64` | outbound |  |
| 19 | `activeAntennaNumber` | `i64` | outbound |  |
| 20 | `activeAntennaExternal` | `bool` | outbound |  |
| 21 | `tunerMode` | `enum` | outbound | 0, 1, 2, 3, 4 |
| 22 | `tunerSetup` | `utf8` | outbound |  |
| 23 | `tunerInductanceNh` | `i64` | outbound |  |
| 24 | `tunerCapacitancePf` | `i64` | outbound |  |
| 25 | `tunerFrequencyKhz` | `i64` | outbound |  |
| 26 | `tunerSegmentKhz` | `i64` | outbound |  |
| 27 | `bandFollow` | `enum` | outbound | 0, 1, 2, 3 |
| 28 | `bandFollowAddress` | `utf8` | outbound |  |
| 29 | `bandFollowPort` | `i64` | outbound |  |

**SetupDescription** (12 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `general` | `utf8` | outbound |  |
| 1 | `hardware` | `utf8` | outbound |  |
| 2 | `audio` | `utf8` | outbound |  |
| 3 | `dsp` | `utf8` | outbound |  |
| 4 | `display` | `utf8` | outbound |  |
| 5 | `transmit` | `utf8` | outbound |  |
| 6 | `appearance` | `utf8` | outbound |  |
| 7 | `catNetwork` | `utf8` | outbound |  |
| 8 | `test` | `utf8` | outbound |  |
| 9 | `diagnostics` | `utf8` | outbound |  |
| 10 | `revision` | `i64` | outbound |  |
| 11 | `pa` | `utf8` | outbound |  |

**SliceAccess** (8 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `sliceId` | `i64` | constantSnapshot |  |
| 1 | `incarnation` | `i64` | constantSnapshot |  |
| 2 | `controllerDeviceId` | `utf8` | outbound |  |
| 3 | `controlRevision` | `i64` | outbound |  |
| 4 | `listenerDeviceIds` | `utf8` | outbound |  |
| 5 | `activeRxDeviceIds` | `utf8` | outbound |  |
| 6 | `txSelected` | `bool` | outbound |  |
| 7 | `onAir` | `bool` | outbound |  |

**SliceMarker** (14 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `sliceId` | `i64` | constantSnapshot |  |
| 1 | `ownerDeviceId` | `utf8` | outbound |  |
| 2 | `ownerName` | `utf8` | outbound |  |
| 3 | `ownerShortName` | `utf8` | outbound |  |
| 4 | `ownerKind` | `utf8` | outbound |  |
| 5 | `ownerAway` | `bool` | outbound |  |
| 6 | `frequency` | `f64` | outbound |  |
| 7 | `dspMode` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 |
| 8 | `filterLow` | `i64` | outbound |  |
| 9 | `filterHigh` | `i64` | outbound |  |
| 10 | `txSlice` | `bool` | outbound |  |
| 11 | `band` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 |
| 12 | `streamIndex` | `i64` | outbound |  |
| 13 | `psPaused` | `bool` | outbound |  |

**SliceModel** (154 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `outputRoute` | `enum` | bidirectional | 0, 1 |
| 1 | `frequency` | `f64` | bidirectional |  |
| 2 | `dspMode` | `enum` | bidirectional | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 |
| 3 | `filterLow` | `i64` | bidirectional |  |
| 4 | `filterHigh` | `i64` | bidirectional |  |
| 5 | `agcMode` | `enum` | bidirectional | 0, 1, 2, 3, 4, 5 |
| 6 | `stepHz` | `i64` | bidirectional |  |
| 7 | `afGain` | `i64` | bidirectional |  |
| 8 | `rfGain` | `i64` | bidirectional |  |
| 9 | `rxAntenna` | `utf8` | bidirectional |  |
| 10 | `txAntenna` | `utf8` | bidirectional |  |
| 11 | `active` | `bool` | outbound |  |
| 12 | `txSlice` | `bool` | outbound |  |
| 13 | `sliceIndex` | `i64` | constantSnapshot |  |
| 14 | `band` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 |
| 15 | `signalStrengthDbm` | `f64` | outbound |  |
| 16 | `signalPeakDbm` | `f64` | outbound |  |
| 17 | `signalAverageDbm` | `f64` | outbound |  |
| 18 | `stationAutoAgcNoiseFloorDbm` | `f64` | outbound |  |
| 19 | `stationAutoAgcNoiseFloorValid` | `bool` | outbound |  |
| 20 | `stationAutoAgcNoiseFloorGeneration` | `i64` | outbound |  |
| 21 | `chainIndex` | `i64` | outbound |  |
| 22 | `ddcIndex` | `i64` | outbound |  |
| 23 | `streamIndex` | `i64` | outbound |  |
| 24 | `streamCtunPinned` | `bool` | outbound |  |
| 25 | `streamEpoch` | `i64` | outbound |  |
| 26 | `shiftOffsetHz` | `f64` | outbound |  |
| 27 | `panKey` | `utf8` | bidirectional |  |
| 28 | `sampleRateHz` | `i64` | outbound |  |
| 29 | `diversityEnabled` | `bool` | bidirectional |  |
| 30 | `diversityPhaseDeg` | `f64` | bidirectional |  |
| 31 | `diversityGainDb` | `f64` | bidirectional |  |
| 32 | `diversityFineNullEnabled` | `bool` | bidirectional |  |
| 33 | `widebandExtensionRequested` | `bool` | outbound |  |
| 34 | `psPaused` | `bool` | outbound |  |
| 35 | `locked` | `bool` | bidirectional |  |
| 36 | `muted` | `bool` | bidirectional |  |
| 37 | `audioPan` | `f64` | bidirectional |  |
| 38 | `ssqlEnabled` | `bool` | bidirectional |  |
| 39 | `ssqlThresh` | `f64` | bidirectional |  |
| 40 | `amsqEnabled` | `bool` | bidirectional |  |
| 41 | `amsqThresh` | `f64` | bidirectional |  |
| 42 | `fmsqEnabled` | `bool` | bidirectional |  |
| 43 | `fmsqThresh` | `f64` | bidirectional |  |
| 44 | `agcThreshold` | `i64` | bidirectional |  |
| 45 | `agcHang` | `i64` | bidirectional |  |
| 46 | `agcSlope` | `i64` | bidirectional |  |
| 47 | `agcAttack` | `i64` | bidirectional |  |
| 48 | `agcDecay` | `i64` | bidirectional |  |
| 49 | `autoAgcEnabled` | `bool` | bidirectional |  |
| 50 | `autoAgcOffset` | `f64` | bidirectional |  |
| 51 | `agcFixedGain` | `i64` | bidirectional |  |
| 52 | `agcHangThreshold` | `i64` | bidirectional |  |
| 53 | `agcMaxGain` | `i64` | bidirectional |  |
| 54 | `ritEnabled` | `bool` | bidirectional |  |
| 55 | `ritHz` | `i64` | bidirectional |  |
| 56 | `xitEnabled` | `bool` | bidirectional |  |
| 57 | `xitHz` | `i64` | bidirectional |  |
| 58 | `nbMode` | `enum` | bidirectional | 0, 1, 2 |
| 59 | `activeNr` | `enum` | bidirectional | 0, 1, 2, 3, 4, 5, 6, 7, 8 |
| 60 | `nnrModelSlot` | `i64` | bidirectional |  |
| 61 | `nnrMaskFloorDb` | `f64` | bidirectional |  |
| 62 | `nnrPosition` | `enum` | bidirectional | 0, 1 |
| 63 | `nnrAlpha` | `f64` | bidirectional |  |
| 64 | `nnrAlphaKneeDb` | `f64` | bidirectional |  |
| 65 | `nnrTauSeconds` | `f64` | bidirectional |  |
| 66 | `nnrMaxGainDb` | `f64` | bidirectional |  |
| 67 | `nnrAttackMs` | `f64` | bidirectional |  |
| 68 | `nnrReleaseMs` | `f64` | bidirectional |  |
| 69 | `nnrAvailable` | `bool` | outbound |  |
| 70 | `nnrReady` | `bool` | outbound |  |
| 71 | `nnrRunning` | `bool` | outbound |  |
| 72 | `nnrStandardAvailable` | `bool` | outbound |  |
| 73 | `nnrPremiumAvailable` | `bool` | outbound |  |
| 74 | `nnrRateSupported` | `bool` | outbound |  |
| 75 | `nnrActualModelSlot` | `i64` | outbound |  |
| 76 | `nnrDspRateHz` | `i64` | outbound |  |
| 77 | `nnrNetworkRateHz` | `i64` | outbound |  |
| 78 | `nnrDelaySamples` | `i64` | outbound |  |
| 79 | `nnrProfilingAvailable` | `bool` | outbound |  |
| 80 | `nnrLatencyMs` | `f64` | outbound |  |
| 81 | `nnrTestMode` | `i64` | outbound |  |
| 82 | `nnrOutputMode` | `i64` | outbound |  |
| 83 | `nnrModelSource` | `utf8` | outbound |  |
| 84 | `nnrStatus` | `utf8` | outbound |  |
| 85 | `nnrLastError` | `utf8` | outbound |  |
| 86 | `nnrLimit` | `i64` | outbound |  |
| 87 | `nr1Taps` | `i64` | bidirectional |  |
| 88 | `nr1Delay` | `i64` | bidirectional |  |
| 89 | `nr1Gain` | `f64` | bidirectional |  |
| 90 | `nr1Leakage` | `f64` | bidirectional |  |
| 91 | `nr1Position` | `enum` | bidirectional | 0, 1 |
| 92 | `nr2GainMethod` | `enum` | bidirectional | 0, 1, 2, 3 |
| 93 | `nr2NpeMethod` | `enum` | bidirectional | 0, 1, 2 |
| 94 | `nr2TrainT1` | `f64` | bidirectional |  |
| 95 | `nr2TrainT2` | `f64` | bidirectional |  |
| 96 | `nr2AeFilter` | `bool` | bidirectional |  |
| 97 | `nr2Position` | `enum` | bidirectional | 0, 1 |
| 98 | `nr2Post2Run` | `bool` | bidirectional |  |
| 99 | `nr2Post2Level` | `f64` | bidirectional |  |
| 100 | `nr2Post2Factor` | `f64` | bidirectional |  |
| 101 | `nr2Post2Rate` | `f64` | bidirectional |  |
| 102 | `nr2Post2Taper` | `i64` | bidirectional |  |
| 103 | `nr3Position` | `enum` | bidirectional | 0, 1 |
| 104 | `nr3UseDefaultGain` | `bool` | bidirectional |  |
| 105 | `nr4Reduction` | `f64` | bidirectional |  |
| 106 | `nr4Smoothing` | `f64` | bidirectional |  |
| 107 | `nr4Whitening` | `f64` | bidirectional |  |
| 108 | `nr4Rescale` | `f64` | bidirectional |  |
| 109 | `nr4PostThresh` | `f64` | bidirectional |  |
| 110 | `nr4Algo` | `enum` | bidirectional | 0, 1, 2 |
| 111 | `dfnrAttenLimit` | `f64` | bidirectional |  |
| 112 | `dfnrPostFilterBeta` | `f64` | bidirectional |  |
| 113 | `bnrStrength` | `f64` | bidirectional |  |
| 114 | `mnrStrength` | `f64` | bidirectional |  |
| 115 | `mnrOversub` | `f64` | bidirectional |  |
| 116 | `mnrFloor` | `f64` | bidirectional |  |
| 117 | `mnrAlpha` | `f64` | bidirectional |  |
| 118 | `mnrBias` | `f64` | bidirectional |  |
| 119 | `mnrGsmooth` | `f64` | bidirectional |  |
| 120 | `snbEnabled` | `bool` | bidirectional |  |
| 121 | `anfEnabled` | `bool` | bidirectional |  |
| 122 | `nb1Threshold` | `i64` | bidirectional |  |
| 123 | `nb1TransitionMs` | `f64` | bidirectional |  |
| 124 | `nb1LeadMs` | `f64` | bidirectional |  |
| 125 | `nb1LagMs` | `f64` | bidirectional |  |
| 126 | `nb2Mode` | `i64` | bidirectional |  |
| 127 | `snbK1` | `f64` | bidirectional |  |
| 128 | `snbK2` | `f64` | bidirectional |  |
| 129 | `snbOutputBandwidthHz` | `i64` | bidirectional |  |
| 130 | `apfEnabled` | `bool` | bidirectional |  |
| 131 | `apfTuneHz` | `i64` | bidirectional |  |
| 132 | `binauralEnabled` | `bool` | bidirectional |  |
| 133 | `fmCtcssMode` | `i64` | bidirectional |  |
| 134 | `fmCtcssValueHz` | `f64` | bidirectional |  |
| 135 | `fmOffsetHz` | `i64` | bidirectional |  |
| 136 | `fmTxMode` | `enum` | bidirectional | 0, 1, 2 |
| 137 | `fmReverse` | `bool` | bidirectional |  |
| 138 | `diglOffsetHz` | `i64` | bidirectional |  |
| 139 | `diguOffsetHz` | `i64` | bidirectional |  |
| 140 | `rttyMarkHz` | `i64` | bidirectional |  |
| 141 | `rttyShiftHz` | `i64` | bidirectional |  |
| 142 | `snrDb` | `f64` | outbound |  |
| 143 | `lastRadeRxCallsign` | `utf8` | outbound |  |
| 144 | `adcPeakDbfs` | `f64` | outbound |  |
| 145 | `adcAverageDbfs` | `f64` | outbound |  |
| 146 | `agcGainDb` | `f64` | outbound |  |
| 147 | `agcPeakDb` | `f64` | outbound |  |
| 148 | `agcAverageDb` | `f64` | outbound |  |
| 149 | `minNotchWidthHz` | `f64` | outbound |  |
| 150 | `diversityPattern` | `utf8` | outbound |  |
| 151 | `radeSynced` | `bool` | outbound |  |
| 152 | `radeFreqOffsetHz` | `f64` | outbound |  |
| 153 | `radeReason` | `utf8` | outbound |  |

**SpotSourceHost** (11 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `dxClusterState` | `utf8` | outbound |  |
| 1 | `dxClusterText` | `utf8` | outbound |  |
| 2 | `rbnState` | `utf8` | outbound |  |
| 3 | `rbnText` | `utf8` | outbound |  |
| 4 | `potaState` | `utf8` | outbound |  |
| 5 | `potaText` | `utf8` | outbound |  |
| 6 | `pskReporterState` | `utf8` | outbound |  |
| 7 | `pskReporterText` | `utf8` | outbound |  |
| 8 | `freedvReporterState` | `utf8` | outbound |  |
| 9 | `freedvReporterText` | `utf8` | outbound |  |
| 10 | `freedvReporterHidden` | `bool` | outbound |  |

**StationCatalog** (2 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `json` | `utf8` | outbound |  |
| 1 | `revision` | `i64` | outbound |  |

**StationDevicesFacade** (10 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `listJson` | `utf8` | outbound |  |
| 1 | `revision` | `i64` | outbound |  |
| 2 | `stationLabel` | `utf8` | outbound |  |
| 3 | `claimed` | `bool` | outbound |  |
| 4 | `tokenActive` | `bool` | outbound |  |
| 5 | `keyBackupAcknowledged` | `bool` | outbound |  |
| 6 | `keyPath` | `utf8` | outbound |  |
| 7 | `pairingWindowOpen` | `bool` | outbound |  |
| 8 | `pairingCode` | `utf8` | outbound |  |
| 9 | `coreAddresses` | `utf8` | outbound |  |

**StationTciModel** (20 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `enabled` | `bool` | outbound |  |
| 1 | `port` | `i64` | outbound |  |
| 2 | `listening` | `bool` | outbound |  |
| 3 | `stationAddress` | `utf8` | outbound |  |
| 4 | `error` | `utf8` | outbound |  |
| 5 | `emulateExpertSdr3` | `bool` | outbound |  |
| 6 | `emulateSunSdr2Pro` | `bool` | outbound |  |
| 7 | `cwluBecomesCw` | `bool` | outbound |  |
| 8 | `sendInitialState` | `bool` | outbound |  |
| 9 | `rateLimitMs` | `i64` | outbound |  |
| 10 | `cwBecomesCwuAbove10mhz` | `bool` | outbound |  |
| 11 | `iqSwap` | `bool` | outbound |  |
| 12 | `alwaysStreamIq` | `bool` | outbound |  |
| 13 | `audioBlockSamples` | `i64` | outbound |  |
| 14 | `txChannel` | `i64` | outbound |  |
| 15 | `rxSensorIntervalMs` | `i64` | outbound |  |
| 16 | `txSensorIntervalMs` | `i64` | outbound |  |
| 17 | `forgetRx2VfoBOnDisconnect` | `bool` | outbound |  |
| 18 | `useRx1VfoaForRx2Vfoa` | `bool` | outbound |  |
| 19 | `copyRx2VfobToVfoa` | `bool` | outbound |  |

**StationVax** (18 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `ch1Slices` | `utf8` | outbound |  |
| 1 | `ch2Slices` | `utf8` | outbound |  |
| 2 | `ch3Slices` | `utf8` | outbound |  |
| 3 | `ch4Slices` | `utf8` | outbound |  |
| 4 | `ch1RxGain` | `f64` | bidirectional |  |
| 5 | `ch2RxGain` | `f64` | bidirectional |  |
| 6 | `ch3RxGain` | `f64` | bidirectional |  |
| 7 | `ch4RxGain` | `f64` | bidirectional |  |
| 8 | `ch1Muted` | `bool` | bidirectional |  |
| 9 | `ch2Muted` | `bool` | bidirectional |  |
| 10 | `ch3Muted` | `bool` | bidirectional |  |
| 11 | `ch4Muted` | `bool` | bidirectional |  |
| 12 | `ch1Device` | `utf8` | outbound |  |
| 13 | `ch2Device` | `utf8` | outbound |  |
| 14 | `ch3Device` | `utf8` | outbound |  |
| 15 | `ch4Device` | `utf8` | outbound |  |
| 16 | `txSlice` | `utf8` | outbound |  |
| 17 | `txGain` | `f64` | bidirectional |  |

**StepAttenuatorFacade** (25 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `enabled` | `bool` | bidirectional |  |
| 1 | `attenuationDb` | `i64` | bidirectional |  |
| 2 | `preampMode` | `i64` | bidirectional |  |
| 3 | `rx1Preamp` | `bool` | bidirectional |  |
| 4 | `autoAttEnabled` | `bool` | bidirectional |  |
| 5 | `autoAttMode` | `i64` | bidirectional |  |
| 6 | `autoAttUndo` | `bool` | bidirectional |  |
| 7 | `autoAttUndoDelayMs` | `i64` | bidirectional |  |
| 8 | `autoAttHoldMs` | `i64` | bidirectional |  |
| 9 | `minDb` | `i64` | outbound |  |
| 10 | `maxDb` | `i64` | outbound |  |
| 11 | `autoAttApplied` | `bool` | outbound |  |
| 12 | `overloadAdc0` | `i64` | outbound |  |
| 13 | `overloadAdc1` | `i64` | outbound |  |
| 14 | `adcLinked` | `bool` | outbound |  |
| 15 | `attOnTxEnabled` | `bool` | bidirectional |  |
| 16 | `attOnTxValue` | `i64` | bidirectional |  |
| 17 | `forceAttWhenPsOff` | `bool` | bidirectional |  |
| 18 | `rx2AttenuationDb` | `i64` | bidirectional |  |
| 19 | `rx2SliceMask` | `i64` | outbound |  |
| 20 | `rx2StepAttEnabled` | `bool` | bidirectional |  |
| 21 | `rx2AutoAttEnabled` | `bool` | bidirectional |  |
| 22 | `rx2AutoAttUndo` | `bool` | bidirectional |  |
| 23 | `rx2AutoAttUndoDelayMs` | `i64` | bidirectional |  |
| 24 | `rx2PreampMode` | `i64` | bidirectional |  |

**TransmitModel** (89 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `mox` | `bool` | outbound |  |
| 1 | `tune` | `bool` | outbound |  |
| 2 | `power` | `i64` | bidirectional |  |
| 3 | `micGain` | `f64` | bidirectional |  |
| 4 | `pureSig` | `bool` | bidirectional |  |
| 5 | `filterLow` | `i64` | bidirectional |  |
| 6 | `filterHigh` | `i64` | bidirectional |  |
| 7 | `lineInGain` | `i64` | bidirectional |  |
| 8 | `userDigOut` | `i64` | bidirectional |  |
| 9 | `forceAttwhenPSAoff` | `bool` | bidirectional |  |
| 10 | `forceAttwhenPowerChangesWhenPSAon` | `bool` | bidirectional |  |
| 11 | `forceAttwhenPowerChangesWhenPSAonAndDecreased` | `bool` | bidirectional |  |
| 12 | `antiVoxTauMs` | `i64` | bidirectional |  |
| 13 | `antiVoxRun` | `bool` | bidirectional |  |
| 14 | `paSettingsBypass` | `bool` | bidirectional |  |
| 15 | `tunePower` | `i64` | bidirectional |  |
| 16 | `voxThresholdDb` | `i64` | bidirectional |  |
| 17 | `voxHangTimeMs` | `i64` | bidirectional |  |
| 18 | `monEnabled` | `bool` | bidirectional |  |
| 19 | `monitorVolume` | `f64` | bidirectional |  |
| 20 | `txLevelerOn` | `bool` | bidirectional |  |
| 21 | `txEqEnabled` | `bool` | bidirectional |  |
| 22 | `cfcEnabled` | `bool` | bidirectional |  |
| 23 | `cpdrOn` | `bool` | bidirectional |  |
| 24 | `cpdrLevelDb` | `i64` | bidirectional |  |
| 25 | `amCarrierLevel` | `i64` | bidirectional |  |
| 26 | `dexpEnabled` | `bool` | bidirectional |  |
| 27 | `micGainDb` | `i64` | bidirectional |  |
| 28 | `tunePowerForTxBand` | `i64` | outbound |  |
| 29 | `tuneDrivePowerSource` | `enum` | bidirectional | 0, 1, 2 |
| 30 | `micBoost` | `bool` | bidirectional |  |
| 31 | `micXlr` | `bool` | bidirectional |  |
| 32 | `micTipRing` | `bool` | bidirectional |  |
| 33 | `micBias` | `bool` | bidirectional |  |
| 34 | `micPttDisabled` | `bool` | bidirectional |  |
| 35 | `lineIn` | `bool` | bidirectional |  |
| 36 | `lineInBoost` | `f64` | bidirectional |  |
| 37 | `activeTxProfile` | `utf8` | outbound |  |
| 38 | `txProfilesJson` | `utf8` | outbound |  |
| 39 | `txEqUseLegacy` | `bool` | bidirectional |  |
| 40 | `txEqPreamp` | `i64` | bidirectional |  |
| 41 | `txEqBandsJson` | `utf8` | bidirectional |  |
| 42 | `txEqFreqsJson` | `utf8` | bidirectional |  |
| 43 | `txEqNc` | `i64` | bidirectional |  |
| 44 | `txEqMp` | `bool` | bidirectional |  |
| 45 | `txEqCtfmode` | `i64` | bidirectional |  |
| 46 | `txEqWintype` | `i64` | bidirectional |  |
| 47 | `txEqParaEqData` | `utf8` | bidirectional |  |
| 48 | `cfcCompressionJson` | `utf8` | bidirectional |  |
| 49 | `cfcEqFreqJson` | `utf8` | bidirectional |  |
| 50 | `cfcPostEqBandGainJson` | `utf8` | bidirectional |  |
| 51 | `cfcPostEqEnabled` | `bool` | bidirectional |  |
| 52 | `cfcPostEqGainDb` | `i64` | bidirectional |  |
| 53 | `cfcPrecompDb` | `i64` | bidirectional |  |
| 54 | `cfcParaEqData` | `utf8` | bidirectional |  |
| 55 | `phaseRotatorEnabled` | `bool` | bidirectional |  |
| 56 | `phaseRotatorFreqHz` | `i64` | bidirectional |  |
| 57 | `phaseRotatorStages` | `i64` | bidirectional |  |
| 58 | `phaseReverseEnabled` | `bool` | bidirectional |  |
| 59 | `cessbOn` | `bool` | bidirectional |  |
| 60 | `txLevelerMaxGain` | `i64` | bidirectional |  |
| 61 | `txLevelerDecay` | `i64` | bidirectional |  |
| 62 | `txAlcMaxGain` | `i64` | bidirectional |  |
| 63 | `txAlcDecay` | `i64` | bidirectional |  |
| 64 | `powerByBandJson` | `utf8` | outbound |  |
| 65 | `tunePowerByBandJson` | `utf8` | outbound |  |
| 66 | `dexpAttackTimeMs` | `f64` | bidirectional |  |
| 67 | `dexpDetectorTauMs` | `f64` | bidirectional |  |
| 68 | `dexpExpansionRatioDb` | `f64` | bidirectional |  |
| 69 | `dexpHighCutHz` | `f64` | bidirectional |  |
| 70 | `dexpHysteresisRatioDb` | `f64` | bidirectional |  |
| 71 | `dexpLookAheadEnabled` | `bool` | bidirectional |  |
| 72 | `dexpLookAheadMs` | `f64` | bidirectional |  |
| 73 | `dexpLowCutHz` | `f64` | bidirectional |  |
| 74 | `dexpReleaseTimeMs` | `f64` | bidirectional |  |
| 75 | `dexpSideChannelFilterEnabled` | `bool` | bidirectional |  |
| 76 | `antiVoxGainDb` | `i64` | bidirectional |  |
| 77 | `twoToneFreq1` | `i64` | bidirectional |  |
| 78 | `twoToneFreq2` | `i64` | bidirectional |  |
| 79 | `twoToneLevel` | `f64` | bidirectional |  |
| 80 | `twoTonePower` | `i64` | bidirectional |  |
| 81 | `twoTonePulsed` | `bool` | bidirectional |  |
| 82 | `twoToneInvert` | `bool` | bidirectional |  |
| 83 | `twoToneFreq2Delay` | `i64` | bidirectional |  |
| 84 | `twoToneDrivePowerSource` | `enum` | bidirectional | 0, 1, 2 |
| 85 | `voxEnabled` | `bool` | bidirectional |  |
| 86 | `micMuted` | `bool` | bidirectional |  |
| 87 | `txEqCurve` | `utf8` | outbound |  |
| 88 | `cfcProfile` | `utf8` | outbound |  |

**TransmitState** (44 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `keyed` | `bool` | outbound |  |
| 1 | `tuning` | `bool` | outbound |  |
| 2 | `twoTone` | `bool` | outbound |  |
| 3 | `txSliceId` | `i64` | outbound |  |
| 4 | `keyedByName` | `utf8` | outbound |  |
| 5 | `keyedByKind` | `utf8` | outbound |  |
| 6 | `keyedTrigger` | `utf8` | outbound |  |
| 7 | `keyedSinceMs` | `i64` | outbound |  |
| 8 | `timeOutRemainingSeconds` | `i64` | outbound |  |
| 9 | `forwardPowerWatts` | `f64` | outbound |  |
| 10 | `reflectedPowerWatts` | `f64` | outbound |  |
| 11 | `swr` | `f64` | outbound |  |
| 12 | `alcDb` | `f64` | outbound |  |
| 13 | `micLevelDb` | `f64` | outbound |  |
| 14 | `txEnding` | `bool` | outbound |  |
| 15 | `stopReason` | `utf8` | outbound |  |
| 16 | `stopText` | `utf8` | outbound |  |
| 17 | `stopSerial` | `i64` | outbound |  |
| 18 | `holderDeviceId` | `utf8` | outbound |  |
| 19 | `holderName` | `utf8` | outbound |  |
| 20 | `holderShortName` | `utf8` | outbound |  |
| 21 | `holderKind` | `utf8` | outbound |  |
| 22 | `holderSource` | `utf8` | outbound |  |
| 23 | `holderForSeconds` | `i64` | outbound |  |
| 24 | `holderEpoch` | `i64` | outbound |  |
| 25 | `holderAway` | `bool` | outbound |  |
| 26 | `holderTransferring` | `bool` | outbound |  |
| 27 | `keyedForSeconds` | `i64` | outbound |  |
| 28 | `stopEpoch` | `i64` | outbound |  |
| 29 | `highSwr` | `bool` | outbound |  |
| 30 | `swrWindBackLatched` | `bool` | outbound |  |
| 31 | `forwardAdcRaw` | `i64` | outbound |  |
| 32 | `reflectedAdcRaw` | `i64` | outbound |  |
| 33 | `compressionDb` | `f64` | outbound |  |
| 34 | `forwardRawPowerWatts` | `f64` | outbound |  |
| 35 | `forwardAdcVolts` | `f64` | outbound |  |
| 36 | `reflectedAdcVolts` | `f64` | outbound |  |
| 37 | `eqDb` | `f64` | outbound |  |
| 38 | `levelerDb` | `f64` | outbound |  |
| 39 | `levelerGainDb` | `f64` | outbound |  |
| 40 | `cfcDb` | `f64` | outbound |  |
| 41 | `cfcGainDb` | `f64` | outbound |  |
| 42 | `alcGainDb` | `f64` | outbound |  |
| 43 | `alcGroupDb` | `f64` | outbound |  |

**TunerModel** (21 properties)

| Ordinal | Property | Wire kind | Direction | Enum values |
| --- | --- | --- | --- | --- |
| 0 | `connectionPhase` | `enum` | outbound | 0, 1, 2, 3, 4, 5, 6, 7 |
| 1 | `configuredHost` | `utf8` | outbound |  |
| 2 | `configuredPort` | `i64` | outbound |  |
| 3 | `connectionError` | `utf8` | outbound |  |
| 4 | `deviceModel` | `utf8` | outbound |  |
| 5 | `deviceSerial` | `utf8` | outbound |  |
| 6 | `deviceVersion` | `utf8` | outbound |  |
| 7 | `deviceNickname` | `utf8` | outbound |  |
| 8 | `relayC1` | `i64` | outbound |  |
| 9 | `relayL` | `i64` | outbound |  |
| 10 | `relayC2` | `i64` | outbound |  |
| 11 | `isOperate` | `bool` | outbound |  |
| 12 | `isBypass` | `bool` | outbound |  |
| 13 | `isTuning` | `bool` | outbound |  |
| 14 | `antennaA` | `i64` | outbound |  |
| 15 | `hasAntennaSwitch` | `bool` | outbound |  |
| 16 | `isPresent` | `bool` | outbound |  |
| 17 | `hasDirectConnection` | `bool` | outbound |  |
| 18 | `tgxlIp` | `utf8` | outbound |  |
| 19 | `fwdPower` | `f64` | outbound |  |
| 20 | `swr` | `f64` | outbound |  |

<!-- /surface -->

Objects are addressed by key. Slices are `slice:<id>` and are created and
destroyed during the session.

<!-- surface:objectKeys -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Object key | Class |
| --- | --- |
| `radio` | `RadioModel` |
| `pureSignalSettings` | `PureSignalSettings` |
| `dspAssets` | `DspAssetService` |
| `notches` | `NotchModel` |
| `stepAtt` | `StepAttenuatorFacade` |
| `alexAntennas` | `AlexAntennaFacade` |
| `ioBoard` | `IoBoardHl2Facade` |
| `pureSignal` | `PureSignalSessionFacade` |
| `transmit` | `TransmitModel` |
| `tuner` | `TunerModel` |
| `amplifier` | `AmplifierModel` |
| `rfkit` | `RfKitModel` |
| `stationTci` | `StationTciModel` |
| `accessoryData` | `AccessoryDataModel` |
| `accessorySettings` | `AccessorySettingsModel` |
| `devices` | `StationDevicesFacade` |
| `connectedDevices` | `ConnectedDevicesFacade` |
| `txState` | `TransmitState` |
| `vax` | `StationVax` |
| `catalog` | `StationCatalog` |
| `paProfiles` | `PaProfilesFacade` |
| `setup` | `SetupDescription` |
| `spotSources` | `SpotSourceHost` |
| `pan:<i>` | `PanadapterModel` |
| `slice:<id>` | `SliceModel` |
| `marker:<id>` | `SliceMarker` |
| `access:<id>` | `SliceAccess` |

<!-- /surface -->

Notes on the keys:

- **`pan:<i>`.** The station watches every panadapter its model holds, but
  no code path in `nereusd` adds one: panadapters live in the window, and
  the Core's spectrum travels on display endpoints (media control). A real
  Core therefore sends no `pan:<i>` objects, and a write to one changes
  nothing; the key stays unused (the several-devices design, ruling 5.12).
  A device's pans are its own: `addSliceOnPan`'s `panId` is the device's
  own key, so two devices' "pan-0" are two pans. The surface records one
  so the key pattern is known.
- **Minor 11 objects.** `stepAtt`, `alexAntennas` and `ioBoard` are sent
  only to a peer at agreed minor 11 and only while `radioHardwareVersion` is
  at least 1, 2 and 3 respectively. `amplifier` and `rfkit` are sent only at
  minor 11 on a Core that owns its accessories, `accessoryData` only at
  minor 11 while `accessoryDataVersion` is at least 1, `accessorySettings` only at
  minor 11 while `remotePgxlControlVersion` is at least 3 or
  `remoteTgxlControlVersion` at least 1, `stationTci` only at
  minor 11 on a Core that runs a station TCI server, `catalog` only at
  minor 11 while `stationCatalogVersion` is at least 1, and `spotSources`
  only at minor 11 while `recordStreamVersion` is at least 1
  (`StationServer::sendToSession`). An older peer never sees their schema
  either.
- **`spotSources`** (parity Task 19, `recordStreamVersion` 1). The Core's
  spot sources, read-only (a write is refused "The Core's spot sources
  change only from their Connect and Start buttons."). For each station
  source, `<source>State` (utf8: `off`, `connecting`, `connected` or
  `error`) and `<source>Text` (utf8: the error, or what the source is
  doing, such as "Polling api.pota.app"; may be empty), for `dxCluster`,
  `rbn`, `pota` and `pskReporter`: `dxClusterState`, `dxClusterText`,
  `rbnState`, `rbnText`, `potaState`, `potaText`, `pskReporterState`,
  `pskReporterText`. They follow the Core's clients, whoever started them
  (a window, the Core's own start, a dropped connection). A window's Spot
  Hub Cluster, RBN, POTA and PSK Reporter tabs show them and clear them
  when the session ends. With `stationFreedvVersion` 1 FreeDV Reporter has
  the same shape, after them: `freedvReporterState` and
  `freedvReporterText` (`off` with the plain reason when the Core's own
  start could not register, such as "Enter your callsign and grid square
  in Spot Hub first."; `connecting` with "Lost the connection; trying
  again in N s" while it reconnects; `error` with the connection's
  error), then `freedvReporterHidden` (bool: "Hide my station" is on, set
  only through `freedv.setHidden`). A window's FreeDV tab and the phone's
  Spot Hub FreeDV row show them. These three are sent after
  `pskReporterText`, so an older peer's property ordinals are unchanged,
  and are sent to every peer that gets `spotSources`; one that does not
  know them ignores them.
- **`ioBoard` `outputs`.** The HL2 I/O board's output pins, one bit per
  output (o0 in bit 0), as the Core last read them back from the board's
  output register (169 at 0x1d on I2C bus 1): after `setIoBoardOutput`,
  after a `requestIoBoardI2c` write or read of that register, and when a
  window opens HL2 Options (mi0bot reads it back on the tab's Enter and
  after each pin click). 0 until the Core has read it. `outbound`, from
  `radioHardwareVersion` 7 (parity Task 14); HL2 Options' output strip
  shows it in both windows.
- **`slice:<id>` TX mark.** `txSlice` (bool, outbound) is true only while
  the slice is the transmit slice and its owner is
  `txState.holderDeviceId` (the several-devices design, ruling 5.4a;
  iPhone app plan Task 77), and false otherwise, whatever the Core binds:
  with transmit unheld no slice shows TX, and a slice the radio's own PTT
  transmits on shows none (its owner reads that from `txState`: `keyed`,
  `holderSource` `radioPtt`, `txSliceId` its own slice). A change of
  holder sends the changed values, on `slice:` and `marker:` alike.
- **`slice:<id>` minimum notch width.** `minNotchWidthHz` (f64, outbound,
  no WRITE; parity Task 16, `dspInfoVersion` 1) is the narrowest notch the
  slice's receiver can make, in Hz, as the Core's own TNF page reads it
  (WDSP's `RXANBPGetMinNotchWidth`, which moves with the channel's filter
  size and rate), 0 while the slice has no receiver. A window's TNF page
  shows it, and its pans check the notch width presets against it and
  draw the notch dent with it.
- **`slice:<id>` diversity pattern.** `diversityPattern` (utf8, outbound,
  no WRITE, declared last in `SliceModel`; `diversityPatternVersion` 1) is
  the sensitivity pattern the Diversity dialog draws for the slice, below
  ("The diversity pattern"). Sent only to a peer that declared
  `diversityPattern` 1.
- **`slice:<id>` RADE status.** `radeSynced` (bool) and `radeFreqOffsetHz`
  (f64), outbound, no WRITE, declared after `diversityPattern` in
  `SliceModel`; `radeStatusVersion` 1. The RADE decoder's sync and its
  frequency offset in Hz, below ("The RADE status"). Sent only to a peer
  that declared `radeStatus` 1.
- **`slice:<id>` RADE reason.** `radeReason` (utf8, outbound, no WRITE,
  declared last in `SliceModel`; `radeReasonVersion` 1) is why the slice is
  in RADE with no working RADE decoder, empty when it has one or is not in
  RADE, below ("The RADE reason"). Sent only to a peer that declared
  `radeReason` 1.
- **`slice:<id>` ADC and AGC readings.** `adcPeakDbfs`, `adcAverageDbfs`,
  `agcGainDb`, `agcPeakDb` and `agcAverageDb` (f64, outbound, no WRITE;
  parity Task 15) are the Core's receive meters for the slice's receiver,
  the ones a container meter binds to ADC Peak, ADC Average, AGC Gain, AGC
  Peak and AGC Average. The Core's meter pump reads them from WDSP each
  tick as Thetis's CalculateRXMeter does (`RXA_ADC_PK`, `RXA_ADC_AV`,
  `RXA_AGC_GAIN`, `RXA_AGC_PK`, `RXA_AGC_AV`), with no calibration offset;
  AGC Gain is Thetis's reading, 0 minus `RXA_AGC_GAIN`, which a local window
  shows too. -400 is no reading: before the radio link is up, with no
  receiver for the slice, and before WDSP has measured a block. While the
  radio is on the air the pump leaves them where they were. They reach a
  window whatever `meterReadingsVersion` says; a window reads them only when
  it is at least 1 (section 6.3). A window never writes them back.
- **Max Bin is measured by the client, not sent.** The S-meter's Max Bin
  mode (and a container meter bound to Max Bin) reads the strongest point
  of the slice's passband on the slice's own panadapter, from the trace as
  the client displays it, after the detector and averaging: the peak of
  that pan's display between the slice's frequency plus its filter low and
  high edges. A local window takes exactly this reading from its own
  spectrum (`SpectrumWidget::peakDbmInSlicePassband`, the 2026-05-22 bench
  fix: the raw FFT bins read 12 to 17 dB below the trace the operator
  sees), and a remote window takes it from the Core's spectrum as it
  displays it (`MeterPoller::panMaxBinSource`). So a local window, a remote
  window and the phone all read the same value for the same trace, and no
  Core has a better value to send: a headless Core (`nereusd`) runs no
  display of its own and never sets Max Bin up. The link carries no Max Bin
  property. An app needs that pan's display subscribed (the media
  document's "Display subscriptions") to have the reading; without it the
  Max Bin meter shows no reading with its reason, never 0.
- **`devices`.** Sent only to a peer at agreed minor 11 whose hello
  declares `deviceAuth` 1 or later, while `deviceAdminVersion` is 1
  (`StationServer::sendToSession`). A window that declares nothing (today's
  desktop) and an older peer never see it or its schema. Every property is
  `outbound`; the object changes only through its six commands (section
  9.1), and a write to it is refused as any `outbound` write is. Its
  properties (`StationDevicesFacade`):
  - `listJson` (`utf8`): a JSON array of the paired devices in pairing
    order, each `{id, name, shortName, kind, pairedAt, lastSeen,
    connected}`: `id` is the device's key fingerprint (SHA-256 of its
    public key's DER) in base64url, `kind` is `phone`, `tablet` or
    `computer`, the two times are ISO 8601 UTC (`""` when never seen), and
    `connected` says whether that device holds a session now. `name` and
    `shortName` are numbered as `connectedDevices` numbers them (below), so
    one device reads the same on both lists; `shortName` is the one the
    device last signed in with (section 3.5), or its kind's word ("Phone",
    "Tablet", "Computer") when it sent none usable.
  - `revision` (`i64`): moves by one with every change to the object, from
    0 to 2^32 - 1 and then round to 0; compare by serial-number
    arithmetic.
  - `stationLabel` (`utf8`): the Core's label as displayed (section 8.2's
    `StationLabel`, or `StationCallsign` until the first rename; `""`
    when neither gives one).
  - `claimed` (`bool`): a device is paired, or the token is active
    (`DeviceStore::isClaimed`).
  - `tokenActive` (`bool`): the old pairing token still signs in (section
    3.3).
  - `keyBackupAcknowledged` (`bool`): the operator confirmed a backup of
    this Core's identity key (`station.acknowledgeKeyBackup`); false on a
    new Core and again after the key is replaced.
  - `keyPath` (`utf8`): where the identity key file is on the Core.
  - `pairingWindowOpen` (`bool`, `pairingVersion` 1): the pairing window
    is open (section 3.6).
  - `pairingCode` (`utf8`, `pairingVersion` 1): the current pairing code,
    `""` while the window is closed and while no code is shown. Sent only
    to a connection signed in with a paired device's own key; any other
    connection receives `""` (`StationServer::withPairingCodeFor`).
  - `coreAddresses` (`utf8`, `coreAddressesVersion` 1): where a device can
    dial this Core's control listener, as compact JSON
    `{"addresses":["[2001:db8:1:0:211:22ff:fe33:4455]:47910","203.0.113.7:47910"]}`
    (`CoreAddresses::toJson`). Each entry is `[<IPv6>]:<port>` or
    `<IPv4>:<port>`, the port the listener actually holds (`remote_port`,
    section 2); IPv6 entries first, then IPv4, each family in address
    order, at most 8 (`CoreAddresses::kMaxAddresses`). It lists only
    addresses on the Core's own interfaces that are up and running
    (`StationNetwork::localEntries`) and that the listener serves (a
    listener bound to one address lists at most that one, and a loopback
    listener none): each stable global IPv6 address (global unicast,
    2000::/3, the rule ICE uses for a usable address; never link-local,
    never a unique local address, never a temporary privacy address, which
    the system marks as not for DNS, and never a deprecated one, whose
    preferred lifetime has run out after a renumbering; on Linux the
    kernel's own temporary and deprecated flags in `/proc/net/if_inet6`
    are applied too, `CoreAddresses::withKernelFlags`, so a Core with six
    privacy addresses beside its stable EUI-64 one lists that one alone),
    with its zone dropped; and each IPv4 address on an interface that the public
    internet routes to (none of RFC 6890's special-purpose blocks: private,
    shared 100.64/10, loopback, link-local, documentation, benchmarking,
    multicast, reserved). The Core names no address it has not got on an
    interface: a public IPv4 address that belongs to a router in front of
    it (NAT, port forwarding) is not listed, and no address comes from STUN.
    `{"addresses":[]}` while the Core does not listen or has none. The Core
    reads its interfaces when its listener opens and again every 5 s
    (`CoreAddresses::kRefreshIntervalMs`, as often as the LAN announcement,
    section 14.1), so a DHCP or SLAAC renumbering reaches every device
    holding it within that time as a `delta` carrying `coreAddresses`
    alone: it has its own change signal and never moves `revision`.
    Declared last in `StationDevicesFacade`, so every earlier ordinal
    stays. A write is refused as any `outbound` property's is. Sent only to
    a connection signed in with a paired device's own key whose hello
    declared `coreAddresses` 1 (`StationServer::peerGetsCoreAddresses`); a
    window signed in with the token never receives it, nor its schema
    field, whatever it declares. **Privacy.** A Core's global addresses
    say where its operator is, so they travel only to devices the operator
    paired, only over a link the device has signed in on, whose TLS (or,
    through the rendezvous, DTLS, section 20) the relay and the rendezvous
    cannot read. They are never in the LAN announcement or Bonjour
    (section 14), never sent to the rendezvous, never logged, and nothing
    unauthenticated asks for them. A device keeps them with the paired
    Core and dials them as direct rungs (section 21.1).
- **`connectedDevices`** (iPhone app plan Task 71;
  `ConnectedDevicesFacade`): who is on the Core, the list a device's
  Devices page reads for "Connected now". Sent only at agreed minor 11 to
  a peer whose hello declared `sessionHolder` 1 with `deviceAuth` 1
  (`sessionHolderVersion` 1, section 6.3); any other peer never sees it or
  its schema. Every property is `outbound`:
  - `listJson` (`utf8`): a JSON array, one entry per device that holds a
    place, live or away, in the order they were let in:
    `{deviceId, name, shortName, kind, paired, hostsCore, revocable, state,
    holdsTransmit, lastActivitySeconds, connectedForSeconds,
    awayForSeconds, transmittingForSeconds, listeningOn}`. `deviceId` is
    the `devices` object's `id` for a paired device, and `token:<n>` for a
    window signed in with the older token and no key (`paired` false),
    named "Computer at <its address>" with the short name "Computer".
    `kind` is `phone`, `tablet` or `computer`, or `station` for a hosting
    desktop's own window (`hostsCore` true). `revocable` is false for a
    hosting desktop's window and for a token window. `state` is
    `listening`, or `away` for a device whose link dropped without leaving
    and that still holds its place (section 12.4). `holdsTransmit` is true
    for the device that holds transmit, keyed or not; `state` is
    `transmitting` and `transmittingForSeconds` how long while it is on the
    air (section 18.2). `transmittingOn` (iPhone app plan Task 77) is its
    transmit slice, `{sliceId, letter, band, mode}`, while it is on the
    air, and absent otherwise. `listeningOn` (iPhone app plan Task 73) lists every slice
    the device owns, an away device's included, each `{sliceId, letter,
    band, mode}` (`letter` "A" for slice 0; `band` and `mode` the values
    the slice's own `band` and `dspMode` carry); a slice's frequency is on
    its `marker:<id>`, so tuning does not change the list. Slices the Core
    holds for a device that has left show only on their markers.
    `lastActivitySeconds` counts from the device's last command, property
    write or settings write, never a heartbeat, and moves at most once a
    minute; `connectedForSeconds` from when the device took its place;
    `awayForSeconds` from when it went away, 0 while it is not.
  - `revision` (`i64`): moves by one with every change to the list, from 0
    to 2^32 - 1 and then round to 0; compare by serial-number arithmetic.
    The list changes, and is sent again, only when something in it other
    than time passing changes.
  - `deviceLimit` (`i64`): 4.

  **Names.** A device's name is the one it paired with, or the one the
  Core gives a token window; its short name is the one it signs in with
  (section 3.5), or its kind's word. When two devices carry the same name,
  the one paired later gets the next free number ("iPhone", "iPhone 2");
  short names are numbered on their own collisions the same way ("Phone",
  "Phone 2"). The order is the paired devices in pairing order, then a
  hosting desktop the Core has not paired, then token windows in the order
  they connected. Names and short names are the operator's own words: the
  Core checks them as names (section 3.5), never against its own wording
  rules.

  **The one clock convention.** Every time the Core sends about a session,
  a device, transmit, a notice or an end is a duration in whole seconds,
  measured on the Core's own monotonic clock when the message is encoded,
  named `...ForSeconds`, `...Seconds` or `secondsAgo`. No wall-clock time
  from the Core reaches these screens, so a Core whose clock is wrong still
  counts right. A duration inside an object is measured again whenever
  that object or property is sent (its `object.create`, a `delta` on any
  change), and not otherwise: an app counts on from its own receipt.
  `devices`' `pairedAt` and `lastSeen` stay ISO 8601 dates, since they
  outlive a session and a restart.
- **`txState`** (iPhone app plan Task 39; `TransmitState`,
  `txStateVersion` 1): the Core's transmitter, its meters and why it last
  stopped a transmission, section 18.8. Sent only at agreed minor 11 to a
  peer whose hello declared `remoteTx` 1; every property is `outbound`.
  With `txReadingsVersion` 1 (parity Task 33) it also carries the radio's
  raw forward and reflected power readings, `forwardAdcRaw` and
  `reflectedAdcRaw` (i64), appended after `swrWindBackLatched`, and the COMP
  reading, `compressionDb` (f64), after them.
  At `txReadingsVersion` 2, `forwardRawPowerWatts`, `forwardAdcVolts`, and
  `reflectedAdcVolts` (f64) follow `compressionDb`. The Core evaluates the
  existing `PaTelemetryScaling` curves for its current hardware model and
  raw ADC samples; a client need not reproduce those board curves.
  At `txReadingsVersion` 3, `eqDb`, `levelerDb`, `levelerGainDb`, `cfcDb`,
  `cfcGainDb`, `alcGainDb` and `alcGroupDb` (f64) follow
  `reflectedAdcVolts`.
- **`vax`** (iPhone app plan Task 25, R-IOS-18; `StationVax`,
  `vaxVersion` 1): the VAX channels of the computer the Core runs on, as its
  VAX applet shows them (`VaxApplet.cpp`). Sent only at agreed minor 11 to a
  peer whose hello declared `vax` 1, on a Core with `vaxVersion` 1. For
  each channel 1 to 4: `ch<N>Slices` (`utf8`, outbound), the letters of the
  slices whose VAX channel it is, in slice order (`AB`), empty when none;
  `ch<N>RxGain` (`f64`, bidirectional), its receive level, 0 to 1;
  `ch<N>Muted` (`bool`, bidirectional); `ch<N>Device` (`utf8`, outbound),
  the device name the applet shows. Then `txSlice` (`utf8`, outbound), the
  transmit slice's letter, empty when none, and `txGain` (`f64`,
  bidirectional), the level of VAX used as the microphone, 0 to 1. The
  Core sends the four groups channel by channel within each group (the
  four `Slices`, then the four `RxGain`, and so on). A change made on that
  computer (its applet) reaches every such peer. The meters travel as the
  `vaxLevels` record stream (section 7.7), not as properties.
- **`catalog`.** The values the Core owns and an app draws its controls
  from (section 7.4). Both properties are `outbound`
  (`StationCatalog`): `json` (`utf8`), the catalogue, and `revision`
  (`i64`), which moves by one each time `json` changes, from 0 to 2^32 - 1
  and then round to 0; compare by serial-number arithmetic. A write to
  either is refused as any `outbound` write is. Today's desktop window
  holds no object for the key and drops it, as it does any class it does
  not know.
- **`transmit`.** Every property is `bidirectional` on the wire, but a
  receive-only Core takes only the transmit settings: at
  `transmitSettingsVersion` 1, a write of any property except `mox`,
  `tune`, `voxEnabled` and `twoToneActive`, and only while its radio is
  off the air (section 7.3). The Core also publishes its real transmit
  state as `radio`'s `transmitting` (outbound); a window reads "on the
  air" from it, the mirrored `transmit.tune`, and `pureSignal`'s
  `twoToneOn`. `radio`'s `txInhibited` (bool, outbound) is the Core's TX
  inhibit (its `TxInhibitMonitor`); a window's TX indicator shows it. It
  needs no capability: a window that does not know it ignores it. A window
  clears its copy of `transmitting` and `txInhibited` when the session
  ends.
- **`radio` DSP facts** (parity Task 16, `dspInfoVersion` 1). Outbound,
  no WRITE. Which noise reduction the Core runs is not here: it is
  `DspAssetService`'s `dfnrRunnable` and `mnrRunnable` (section 6.3).
  `dspOptionsLastApplyMs` (`i64`) is how long the Core's last DSP Options
  apply took (a channel rebuild after a buffer size, filter size, filter
  type or sample rate change), in milliseconds, 0 before any; a window's
  Setup > DSP > Options "Time to last change" shows it. It reaches a window
  whatever `dspInfoVersion` says; a window reads it only when it is at
  least 1 (section 6.3).
- **`radio`'s `stationRadioWaiting`** (the fix wave after parity Tasks 19
  and 21; `stationRadiosVersion` 1). Outbound, no WRITE, `utf8`: why the
  Core has no radio, in plain words ("The Core can see more than one
  radio. Choose which one it runs.", "The Core is waiting for its radio to
  appear on the network.", "The Core cannot see a radio on its network.",
  or that its radio is in use by another program), empty while it has a
  radio or is connecting one. This Core shows it when the Core has no
  radio. A Core that does not choose its own radio always sends it empty;
  a window that does not know it ignores it, and clears its copy when the
  session ends.
- **`radio`'s `logCategories`** (remote-window parity Task 22;
  `supportBundleVersion` 1). Outbound, no WRITE, `utf8`: the ids of the
  Core's logging categories that are on (the Support dialog's list, for
  example `nereus.discovery,nereus.connection`), joined by commas in the
  dialog's order, empty when none is. It follows every change, made at the
  Core or with `support.setLogCategories`. A window's Support dialog shows
  it on its logging checkboxes; the window's own logging is its own and
  never follows it. A window that does not know it ignores it, and clears
  its copy when the session ends.
- **`radio`'s `logCategoryList`** (phone wire batch;
  `logCategoryListVersion` 1). Constant snapshot, no WRITE, `utf8`,
  declared after `logCategories` in `RadioModel`: every logging category the Core keeps, in
  its Support dialog's order, with the label that dialog's checkbox shows,
  as compact JSON `{"categories":[{"id":"nereus.discovery","label":"Discovery"},...]}`
  (`LogManager::categoryListJson`). `id` is the id `logCategories` and
  `support.setLogCategories` carry; `label` is plain operator words. The
  list is fixed for the life of the Core process, so it travels in the
  snapshot only, never in a delta; a Core with a new category lists it
  under its own label, so an app shows a category it was not built with.
  A reader ignores a key it does not know. A write is refused "The Core
  sets this itself; it cannot be changed from here." Sent only to a peer
  that declared `logCategoryList` 1. An app's Logs page lists these, each
  on or off as `logCategories` says, and switches them with
  `support.setLogCategories`.
- **`radio`'s `txInhibitReason`** (HL2 I/O board fault;
  `txInhibitReasonVersion` 1). Outbound, no WRITE, `utf8`, ordinal 28 in
  `RadioModel`: why the Core holds transmit off, in the words its own
  window shows, or empty when it gives no particular reason. It is set
  while a Hermes Lite 2 I/O board reports a fault, as "I/O Board: Fault
  Code N" with the board's fault code (mi0bot-Thetis `console.cs`
  UpdateIOBoard), and clears when the board next reads no fault; `radio`'s
  `txInhibited` is true for as long as it is set. A fault that appears
  while the radio is on the air drops MOX, TUNE and two-tone, as the Thetis
  TX inhibit does. A window shows MOX, TUNE and two-tone disabled with this
  reason and the TX badge with "Transmit blocked." and the reason; when it
  is empty and `txInhibited` is true it uses its general transmit inhibit
  wording. A write is refused "The Core sets this itself; it cannot be
  changed from here." Sent only to a peer that declared `txInhibitReason`
  1; a window clears its copy when the session ends.
- **`radio`'s `paTransmitBand`** (R-R3-49; `paTransmitBandVersion` 1).
  Outbound, no WRITE, `i64`, declared last in `RadioModel`: the PA band
  index (the PA Gain page's row order: 0 for 160 m through 10 for 6 m,
  13 for the transverter row; -1 when the transmit band has no PA row,
  such as general coverage) the Core's PA on-the-air lock holds
  (`RadioModel::paOnAirBandIndex`). It is the Core's transmit band, which
  holds while the radio is on the air: Thetis moves the band it adjusts
  only from its TXBand setter, and that setter returns while MOX, so a
  transmit slice retuned to another band while keyed does not move it.
  It follows every change of the Core's transmit band. While the Core is
  on the air, the PA row with this index is the one the Core accepts
  edits for; the rest are refused with the on-the-air reason. A write is
  refused "The Core sets this itself; it cannot be changed from here."
  Sent only to a peer that declared `paTransmitBand` 1. A window clears
  its copy when the session ends and falls back to its own transmit
  slice's band.
- **`radio`'s level calibration run** (Level Cal; `radioHardwareVersion`
  12). Outbound, no WRITE, declared last in `RadioModel` after
  `paTransmitBand`: `levelCalRunning` (bool, true while a run holds the
  receiver), `levelCalPercent` (i64, 0 to 100, Thetis's progress bar),
  `levelCalMessage` (utf8, how the last run ended, such as "Level
  calibration finished." or why it stopped; empty while running) and `levelCalSucceeded` (bool, the last run
  stored new offsets). A write is refused "The Core sets this itself; it
  cannot be changed from here." Sent only to a peer that declared
  `levelCalibration` 1. A window clears its copies when the session ends.
- **`transmit` at `transmitSettingsVersion` 2.** Each property carries its
  setter's type: `tunePower` (i64, the fixed tune power Setup uses, 0 to
  100 W, 0 to 99 on a Hermes Lite 2), `voxThresholdDb` (i64, -80 to 0 dB),
  `voxHangTimeMs` (i64, 1 to 2000 ms), `monEnabled` (bool), `monitorVolume`
  (f64, 0.0 to 1.0; a window's slider shows it as 0 to 100),
  `txLevelerOn`, `txEqEnabled`, `cfcEnabled`, `cpdrOn` (bool; `cpdrOn` is
  PROC), `cpdrLevelDb` (i64, 0 to 20 dB), `amCarrierLevel` (i64, 0 to 100
  percent), `dexpEnabled` (bool) and `micGainDb` (i64, -50 to 70 dB).
  `tunePowerForTxBand` (i64, outbound) is the tune power for the band the
  Core transmits on (its transmit slice's band, as TUNE reads it), which
  the TX applet's Tune Power slider shows; it follows the transmit slice
  across bands. `tuneDrivePowerSource` (enum: 0 the drive slider, 1 the
  tune slider, 2 the fixed tune power) is where TUNE takes its power from.
  At version 2 both change only through `setTunePowerForTxBand`, and
  `tuneDrivePowerSource` is outbound; at version 5 it is two-way (Setup >
  Transmit > Power's Tune group), and `tunePowerForTxBand` stays outbound. None of these
  keys the radio. The MON output choice (speakers or phones) is not on the
  link: it is each window's own audio routing.
- **`transmit` at `transmitSettingsVersion` 3.** The radio's microphone
  input, each under its setter's name and type: `micBoost` (bool, the
  +20 dB mic boost), `micXlr` (bool, XLR rather than the 3.5 mm jack on a
  radio with both), `micTipRing` (bool, true when the tip is the mic),
  `micBias` (bool), `micPttDisabled` (bool, true when the mic's PTT is
  ignored), `lineIn` (bool, Line In rather than Mic In) and `lineInBoost`
  (f64, the Line In gain, -34.5 to 12.0 dB). The mic source is not among
  them. `activeTxProfile` (utf8, outbound) is the Core's active TX
  profile, and `txProfilesJson` (utf8, outbound) its TX profiles as a JSON
  array of names in the Core's order (for example
  `["AM","Default","Default DX"]`); a station with no radio sends `""` and
  `[]`. Both change only through the `txProfile.*` commands (or at the
  Core). TX and mic profiles are one set. A window never keeps its own
  copy of the Core's profiles: its profile combos and Setup > Audio > TX
  Profile show these two and ask the Core. None of these keys the radio.
- **`transmit` at `transmitSettingsVersion` 4.** The TX EQ, CFC, phase
  rotator, CESSB, leveler and ALC settings, each under its setter's name
  and type. `txEqUseLegacy` (bool, default true) is the TX EQ dialog's
  Legacy EQ box: true, the ten-band EQ reaches the TX channel; false, the
  parametric curve in `txEqParaEqData` does. The Core applies the curve
  itself, whichever window changed it, and it is saved with the TX profile
  (Thetis's `EQUseLegacy`). `txEqPreamp` (i64, -12 to 15 dB),
  `txEqBandsJson` (utf8, the ten band gains, each -12 to 15 dB),
  `txEqFreqsJson` (utf8, the ten band centres, each 10 to 22000 Hz),
  `txEqNc` (i64, 32 to 8192), `txEqMp` (bool), `txEqCtfmode` (i64, 0
  peaking or 1 notch), `txEqWintype` (i64, 0 Blackman-Harris or 1 Hann),
  `txEqParaEqData` (utf8, the parametric curve as Thetis saves it: gzip,
  then base64url, of the curve's JSON; empty for none). `cfcCompressionJson`
  (utf8, the ten compression levels, each 0 to 16 dB), `cfcEqFreqJson`
  (utf8, the ten band centres, each 0 to 20000 Hz),
  `cfcPostEqBandGainJson` (utf8, the ten post-EQ gains, each -24 to 24 dB),
  `cfcPostEqEnabled` (bool), `cfcPostEqGainDb` (i64, -24 to 24 dB),
  `cfcPrecompDb` (i64, 0 to 16 dB), `cfcParaEqData` (utf8, as
  `txEqParaEqData`). A non-empty `txEqParaEqData` or `cfcParaEqData` the
  Core cannot read as a curve is refused ("The Core could not read that
  equalizer curve. Save the curve again and retry.") and changes nothing. `phaseRotatorEnabled` (bool), `phaseRotatorFreqHz`
  (i64, 10 to 2000 Hz), `phaseRotatorStages` (i64, 2 to 16),
  `phaseReverseEnabled` (bool), `cessbOn` (bool), `txLevelerMaxGain` (i64,
  0 to 20 dB), `txLevelerDecay` (i64, 1 to 5000 ms), `txAlcMaxGain` (i64, 0
  to 120 dB) and `txAlcDecay` (i64, 1 to 50 ms). Each ten-value array is a
  compact JSON array of ten whole numbers in band order (for example
  `[-12,-12,-12,-1,1,4,9,12,-10,-10]`); an array of any other length, or
  with a value that is not a whole number or is out of range, is refused
  whole and changes nothing. None of these keys the radio.
- **`transmit` at `transmitSettingsVersion` 5.** Setup > Transmit > Power,
  Transmit > DEXP/VOX and Test > Two-Tone IMD, each under its setter's
  name and its getter's type. `tuneDrivePowerSource` becomes two-way.
  `powerByBandJson` and `tunePowerByBandJson` (utf8) are the per-band power
  and tune power in whole watts, a compact JSON object keyed by the app's
  band key for the 15 bands (`160m`, `80m`, `60m`, `40m`, `30m`, `20m`,
  `17m`, `15m`, `12m`, `10m`, `6m`, `GEN`, `WWV`, `XVTR`, `2m`); the Core
  writes its keys in its own order, and a window reads it as an object. A
  peer without `band2mVersion` 1 is sent the 14 without `2m` (section
  6.1). Both maps are outbound: the Core's RF and Tune sliders own them,
  through `power` and `tunePowerForTxBand`, which pass the Core's on-air
  checks, so a peer's write of either map is refused ("The Core sets
  this itself; it cannot be changed from here.") and changes nothing. No Setup page writes them.
  A window built before this change still sends `powerByBandJson` when
  its RF slider moves; each such write is refused, and its `power` write
  for the same move is still taken, so the Core's power follows the
  slider and the Core's own map reaches the window. With more than one
  slice, such a window's recall for its own active slice can still send a
  `power` read from another band's slot; the Core takes it as the power
  of its transmit band, so the most it can change is that one band's
  value. A current window and the phone send `power` only for an RF
  slider move. The Core keeps each
  value a whole number from 0 to 100 W (tune power 0 to 99 on a Hermes
  Lite 2).
  `dexpAttackTimeMs` (f64, 2 to 100 ms), `dexpDetectorTauMs` (f64, 1 to 100
  ms), `dexpExpansionRatioDb` (f64, 0.0 to 30.0 dB), `dexpHighCutHz` and
  `dexpLowCutHz` (f64, 100 to 10000 Hz, the VOX trigger filter),
  `dexpHysteresisRatioDb` (f64, 0.0 to 10.0 dB), `dexpLookAheadEnabled`
  (bool), `dexpLookAheadMs` (f64, 10 to 999 ms), `dexpReleaseTimeMs` (f64, 2
  to 1000 ms), `dexpSideChannelFilterEnabled` (bool), `antiVoxGainDb` (i64,
  -60 to 60 dB). `twoToneFreq1` and `twoToneFreq2` (i64, -20000 to 20000
  Hz), `twoToneLevel` (f64, -96 to 0 dB), `twoTonePower` (i64, 0 to 100
  percent), `twoTonePulsed` and `twoToneInvert` (bool), `twoToneFreq2Delay`
  (i64, 0 to 1000 ms) and `twoToneDrivePowerSource` (enum, as
  `tuneDrivePowerSource`). The two-tone settings are read when a two-tone
  test starts; the test itself (`twoToneActive`) and Enable VOX
  (`voxEnabled`) stay in the keying set. None of these keys the radio. The
  Core's runtime SWR foldback (Thetis's `NetworkIO.SWRProtect`) is not a
  setting and is not on the link.
- **`stepAtt` at `transmitSettingsVersion` 5.** Setup > Transmit > Power's
  `attOnTxEnabled` (bool, ATT on TX), `attOnTxValue` (i64, the ATT on TX
  value in dB for the Core's transmit band, from the Core's attenuator
  minimum to 31: 0 to 31, -28 to 31 on a Hermes Lite 2) and
  `forceAttWhenPsOff` (bool, Force ATT on Tx to 31 when PS-A is off),
  declared after `adcLinked`. The Core applies each through its step
  attenuator, as the local page does; `attOnTxValue` also follows
  PureSignal's AutoAtt. They are transmit settings: a receive-only Core
  takes them from a peer offered `transmitSettingsVersion` while its radio
  is off the air (section 7.3). Changing `attOnTxValue` with ATT on TX on
  sets the radio's TX attenuator; it keys nothing.
- **`stepAtt`, one attenuator per receive ADC** (R-R3-46, R-R3-11;
  `adcAttenuatorVersion` 1). The Core keeps a step attenuator per receive
  ADC, as Thetis keeps RX1's and RX2's and sends each to the ADC its
  receiver uses. `attenuationDb` is the attenuator of the ADC slice A is on
  (Thetis RX1's), with the band memory that follows slice A's band.
  `rx2AttenuationDb` (i64, two-way, the same range as `attenuationDb`) is
  the other ADC's own attenuator (Thetis RX2's), with its own band memory
  following the band of the lowest-numbered slice on that ADC.
  `rx2SliceMask` (i64, outbound) has bit n set for each slice n on the
  other ADC: those slices read and set `rx2AttenuationDb`, every other
  slice `attenuationDb`. It is 0 when every slice is on slice A's ADC, on a
  one-ADC radio (the Hermes Lite 2 among them), and while diversity links
  the two ADCs, when both carry `attenuationDb` and a write of either
  property sets both. Each slice's S-meter readings and each stream's
  spectrum frames already carry the offset of their own ADC, so a client
  adds nothing. RX2 has its own step attenuator enable
  (`rx2StepAttEnabled`, bool, two-way) and its own auto-attenuate settings
  (`rx2AutoAttEnabled`, `rx2AutoAttUndo`, bool, and `rx2AutoAttUndoDelayMs`,
  i64, whole seconds in ms, all two-way; Thetis's `_rx2_step_att_enabled`,
  `_auto_att_rx2`, `_auto_att_undo_rx2`, `_auto_att_hold_delay_rx2`, default
  off, off, 5 s), saved by the Core for the radio. RX2's auto-attenuate runs
  on the other ADC's overload with its own history; with its undo off a
  raise stays when the overload clears. While every slice is on slice A's
  ADC, or diversity links them, the two enables are one: a write of either
  sets both. Declared after `forceAttWhenPsOff`; sent only to a peer
  that declared `adcAttenuators` 1. At `radioHardwareVersion` 12
  `rx2PreampMode` (i64, two-way, declared last, the same gate) is RX2's own
  preamp mode (Thetis `RX2PreampMode`, console.cs:19413-19520
  [v2.10.3.15]), a `PreampMode` integer like `preampMode`, which the slices
  on the other ADC show and set. The Core takes only the items its radio's
  RX2 list offers (Thetis `comboRX2Preamp`: `0dB`, `-10dB`, `-20dB`,
  `-30dB` on the two-ADC ANAN models, the Saturn boards and the Red
  Pitaya; `0dB`, `-20dB` on the rest) and otherwise settles "This radio
  does not offer that preamp setting.". With RX2's step attenuator off it
  sets the other ADC's attenuator (0, 10, 20 or 30 dB) on the models
  Thetis lists, and on an HPSDR the second receiver's preamp bit. While
  every slice is on slice A's ADC it follows `preampMode`, and each
  follows the other, as Thetis links the two on one ADC. The Core keeps it
  per band of the other ADC, like `rx2AttenuationDb`, and saves it.
- **`transmit`'s `txEqCurve`** (R-IOS-13, R-R3-49; `txEqCurveVersion`
  1). Outbound, `utf8`, declared last in `TransmitModel`: the parametric
  curve of the TX EQ dialog, read from `txEqParaEqData` into a documented
  form, below ("The TX EQ curve"). Sent only to a peer that declared
  `txEqCurve` 1.
- **`transmit`'s `cfcProfile`** (R-R3-49; `transmitSettingsVersion`
  15). Outbound, `utf8`, declared after `txEqCurve` in `TransmitModel`:
  the CFC dialog's band editor, read from `cfcParaEqData` (or, when that is
  empty or unreadable, the ten-band values) into a documented form, below
  ("The CFC band editor"). Sent only to a peer that declared `cfcProfile`
  1.
- **Whose each slice is** (iPhone app plan Task 73; the several-devices
  design, rulings 5.1 to 5.6). With several devices on one Core every
  slice has an owner: the device that made it, the device that adopted it
  (the first device let in while no other is on the Core takes every slice
  nobody owns, and a device alone on the Core takes a slice the Core makes
  itself, such as when its radio arrives late), or the station itself, which runs a slice **held for** a
  device that has left while no other device was on the Core, until that
  device signs in again. A session receives its own slices as `slice:<id>`
  objects and nothing else of any other slice. A device let in that owns
  no slice gets one at once, at the frequency of the Core's current
  slice and on its receiver; with every slice in use it starts with none.
  Owners live at the Core: no `slice:` property changed. Slice letters
  come from one pool (`slice:<id>` is letter 'A' + id on every device), a
  slice keeps its letter for its whole life, and a slice restored for a
  device takes its old letter when it is free.
- **`marker:<id>`** (`SliceMarker`, `sessionHolderVersion` 1). One per
  slice, sent to every session with the feature except the one whose
  slice it is (for a held slice, the device it is held for); an older
  window never receives one or its schema. Every property is `outbound`:
  `sliceId` (`constantSnapshot`; its letter is 'A' + `sliceId`),
  `ownerDeviceId` (the owner's id as `connectedDevices` names it, or the
  id of the device it is held for; `""` for a slice nobody owns),
  `ownerName` and `ownerShortName` (numbered as below), `ownerKind`
  (`phone`, `tablet`, `computer`, or `station` for a slice nobody owns),
  `ownerAway` (the owner is away, or the slice is held for it), and
  `frequency`, `dspMode`, `filterLow`, `filterHigh`, `txSlice`, `band`,
  `streamIndex` (the receiver it sits on, -1 when none; a screen shows
  "Receiver `streamIndex` + 1") and `psPaused`, each as the slice's own.
  `txSlice` is true only while the slice is the transmit slice and its
  owner is `txState.holderDeviceId` (the several-devices design, ruling
  5.4a; iPhone app plan Task 77), as on a `slice:` object. A marker has no colour on the wire: a client draws it in
  its letter's colour. A write to a marker is refused (section 7.3).
  To a session with `sliceAccessVersion` 1 the rule is by membership
  instead: a marker for every slice it has not joined (section 7.5).
- **`access:<id>`** (`SliceAccess`, `sliceAccessVersion` 1). One per live
  slice, made with the slice and destroyed with it, sent only to a session
  with the feature (its schema too); any other session never receives one.
  Every property is `outbound`: `sliceId` and `incarnation`
  (`constantSnapshot`): which slice of that letter this is, never 0 and
  never reused while the Core runs, so a command naming it never reaches
  the letter made again after a close); `controllerDeviceId` (the
  controller's id as `connectedDevices` names it, `""` for none, `station`
  for the Core's own operating position); `controlRevision` (1 when made,
  one more on each change of controller); `listenerDeviceIds` (utf8, a
  JSON array of every joined device's id, the controller first, then in
  join order); `activeRxDeviceIds` (utf8, a JSON array of the devices
  whose active receive slice this is); `txSelected` (the marker's
  `txSlice` rule, ruling 5.4a); `onAir` (the slice is transmitting now).
- **A change of owner** (a device adopting slices nobody owned, a slice
  passing to the station for a device that left, a held slice returning,
  a take or a release) reaches each session as `object.destroy` of the
  form it had and `object.create` of the form it has now (`slice:<id>` to
  `marker:<id>`, or back); so does a join or a leave for a session with
  `sliceAccessVersion` 1. A session whose form did not change (a
  controller that stays on as a listener after a take, a listener that
  takes control) is sent nothing but the `access:<id>` delta. A session
  first sent an object of a class after its connect-time burst (its first
  marker, when a second device arrives) is sent that class's `schema`
  just before it.
- **Unknown classes.** A client that receives a schema for a class it does
  not know records the difference and drops that class's objects and
  deltas.
- **Band outputs (`radio`).** `bandOutputsByte`, `bandOutputsBand` and
  `bandOutputsKeyed` are the band-output (open collector) byte the Core's
  radio connection composed into the packet that carries it (Protocol 1
  bank 0, Protocol 2 high-priority byte 1401), the band index it was chosen
  for, and whether the transmitter was keyed. They are outbound only and
  change together (`RadioModel::bandOutputsChanged`). `bandOutputsBand` is
  -1 until the Core has composed a byte (no radio, or not yet connected),
  and the byte is then 0. A window shows these pins, never a byte of its
  own; a Core built before them never sends them, so a client shows no
  pins until all three have arrived with a band of 0 or more. No
  capability value gates them: an older client ignores the unknown
  properties (section 7.1's schema carries them), and a newer client
  reads the absence itself.
- **The Alex-1 low-pass in use (`radio`).** `alexLpfBits` (int, outbound
  only, read-only) is the Alex-1 low-pass the Core's radio connection
  last selected, as Thetis's SetAlexLPFBits names them: 0x01 20m (30/20m
  row), 0x02 40m (60/40m), 0x04 80m, 0x08 160m, 0x10 6m, 0x20 10m (12/10m),
  0x40 15m (17/15m); -1 before any selection (no radio, or not yet
  connected). It is sent only to a peer that declared `alexLpf` 1
  (section 6.2). A window lights the lamp of the row it names and never
  writes it; a raw write is refused.
- **The receive low-pass held for another slice (`radio`).** When two or
  more slices are counted on the first receiver input (a slice on another
  device that is away is not counted), the Core sets the receive low-pass
  for the slice with the highest receiver centre frequency (on the Hermes
  Lite 2, the N2ADR pins of that slice's band; a slice outside the bands
  the pins cover counts only when no other does), as Thetis does for RX1
  and RX2 together. `rxFilter0LowPassReason` (utf8, outbound only,
  read-only) says so in operator words, naming that slice and the slices
  that share the input with less protection, for example "The receive
  low-pass filter is set for slice B on 20m, the highest band on this
  receiver input. Slice A on 80m shares the input, so it has less
  protection from strong signals on higher bands." It is empty when one
  slice is counted, when every counted slice uses the same low-pass, when
  the radio has no receive low-pass, when 6m/ByPass on receive is on, or
  on the Hermes Lite 2 when the band-pass is bypassed (the N2ADR pins are
  then all off, so no low-pass is set: Force bypass or wideband), or on
  the Hermes Lite 2 when the pins sent are all off because the band of
  the slice they follow has none set (the chain is then reported bypassed
  and `rxFilter0Reason` names that slice). In Auto
  on the Hermes Lite 2, slices whose pins differ get the pins of the
  highest slice, and the reason names it. When the N2ADR board's
  broadcast-band high-pass (pin 7) is off because a counted slice's own
  pins lack it (a slice on 160 m in the N2ADR preset), the reason adds a
  sentence naming that slice, and that sentence can be the whole reason
  when no slice is held.
  `rxFilter0LowPassSlice` (int, outbound only, read-only) is the id of the
  slice the low-pass is set for, -1 when the reason is empty or names only
  the high-pass. Both are
  sent only to a peer that declared `rxFilterLowPass` 1 (section 6.2). A
  window shows the reason beside the WIDE reason (`rxFilter0Reason`) and
  never writes either; a raw write is refused. A window of a Core that
  never sends them shows the WIDE reason alone. They are optional: a
  chain's filter state is shown once its mode, effective state, band and
  reason have arrived, with or without them.

#### The TX EQ curve (`txEqCurve`)

R-IOS-13, R-R3-49; `txEqCurveVersion` 1. `txEqParaEqData` holds the TX EQ
dialog's parametric curve as Thetis saves it: gzip, then base64url, of
Thetis's own JSON. `transmit`'s `txEqCurve` is the same curve in a form
NereusSDR owns and documents here, so an app draws it without gzip or
Thetis's JSON. The Core derives it from `txEqParaEqData` every time that
changes (`ParaEqCurve::txEqCurveJson`, `ParaEqCurve.cpp`) and sends it in
the same delta. The writer of `txEqParaEqData` gets the new curve in the
side-effect `delta` that follows its `property.result`.

- **Read-only.** `txEqCurve` is outbound. A write to it is refused as any
  outbound property is ("The Core sets this itself; it cannot be changed
  from here."). Version 1 has no write path. The curve is changed at the
  Core, or by writing `txEqParaEqData`. At version 2 an app changes it
  with `txEq.setCurve` and `txEq.resetCurve` ("Changing the curve",
  below).
- **Who gets it.** Only a peer at agreed minor 11 whose hello declared
  `txEqCurve` 1. Its `TransmitModel` schema, its `transmit` snapshot and
  its deltas carry the field. Every other peer's carry none of it, and a
  delta that would carry only the curve is not sent to them.
- **What it is.** One JSON object, compact. Key order is not significant.
  The Core writes the keys in sorted order. A reader ignores a key it does
  not know.

| Key | JSON type | Units and range | Meaning |
| --- | --- | --- | --- |
| `state` | string | `saved`, `default` or `unavailable` | What `txEqParaEqData` holds (below). Always present. An app treats a value it does not know as `unavailable` |
| `parametric` | boolean | | The dialog's Use Q Factors. True: each point is a bell of width `q`. False: straight lines between the points, and `q` is unused |
| `preampDb` | number | dB, -24 to 24, in 0.1 dB steps | The curve's preamp (Thetis's global gain), added to the whole curve |
| `minHz` | number | Hz, finite, below `maxHz` | The curve's low end. The dialog's Low offers 0 to 20000 Hz |
| `maxHz` | number | Hz, finite, above `minHz` | The curve's high end. The dialog's High offers 0 to 20000 Hz |
| `points` | array | 2 to 256 points | The curve's points, in the order below. The dialog offers 5, 10 and 18 bands |

Each point:

| Key | JSON type | Units and range | Meaning |
| --- | --- | --- | --- |
| `frequencyHz` | number | Hz, `minHz` to `maxHz`, in 0.001 Hz steps (a point moved for spacing may fall between) | The point's centre |
| `gainDb` | number | dB, -24 to 24, in 0.1 dB steps | The point's gain |
| `q` | number | 0.2 to 20, in 0.01 steps | The bell's Q (used when `parametric` is true) |

- **`state`.**
  - `saved`: `txEqParaEqData` holds a curve, shown as the dialog shows it.
  - `default`: `txEqParaEqData` is empty, as every factory TX profile
    saves it. The keys hold the flat curve the Core applies in its place,
    Thetis's defaults: ten points from 0 to 4000 Hz, point `i` (0 to 9) at
    (`i` / 9) × 4000 Hz as a double (444.4444444444444 for point 1), each
    0 dB with Q 4, `parametric` true, `preampDb` 0. The desktop dialog
    shows the same.
  - `unavailable`: `txEqParaEqData` holds a value the Core cannot read as
    a curve. The object is `{"state":"unavailable"}` and nothing else. The
    Core applies the flat default curve in its place, as Thetis does, and
    the desktop dialog shows it. An app shows the curve as unavailable,
    not as that default, since the saved value is not what the operator
    chose. A write of such a value is refused (section 7.1, `transmit` at
    `transmitSettingsVersion` 4), so it arises only from a value saved at
    the Core.
- **How a saved curve is read.** As Thetis's transmit path reads it
  (`PointsFromJson`): each value clamped to the ranges above, the
  frequency rounded to 0.001 Hz, the gain and the preamp to 0.1 dB and Q to
  0.01, with round half to even. The first point is moved to `minHz`, the
  last to `maxHz`. A field missing from the saved JSON reads as 0 (false
  for Use Q Factors), as Thetis's JSON reader leaves it; a Q of 0 then
  clamps to 0.2. The desktop dialog loads the value through the same
  reader, so it always shows exactly `txEqCurve`.
- **Ordering.** The points are in the order the dialog draws them
  (Thetis's panel ordering, with the TX panel's settings):
  1. Sorted by `frequencyHz`, lowest first. Two points at the same
     frequency keep their saved order.
  2. Each point clamped into `minHz` to `maxHz`, then the first point set
     to `minHz` and the last to `maxHz`.
  3. With three points or more, the points between are kept a spacing
     `s` apart: `s` is 5 Hz, or (`maxHz` - `minHz`) / (count - 1) when
     that is smaller. Point `i` (1 to count - 2) is first clamped into
     `minHz` + `s` × `i` to `maxHz` - `s` × (count - 1 - `i`). Then,
     lowest first, a point closer than `s` above the one before it moves
     up to it plus `s`. Then, highest first, a point closer than `s` below
     the one after it moves down to it minus `s`. Last, the first and last
     are set to `minHz` and `maxHz` again.

  An app draws the points as sent and never reorders them. The Core hands
  WDSP the saved order without the spacing. For every curve the dialog or
  Thetis saved, the two are the same.
- **Drawing it.** The dialog's line at frequency `f`, from `minHz` to
  `maxHz` on a -24 to 24 dB scale, is the response at `f` plus
  `preampDb`:
  - `parametric` false: at or below the first point, its `gainDb`; at or
    above the last, its `gainDb`; between two neighbouring points, a
    straight line between their gains.
  - `parametric` true: the sum over the points of
    `gainDb` × exp(-0.5 × ((`f` - `frequencyHz`) / σ)²), where
    σ = `w` / 2.3548200450309493 and `w` = (`maxHz` - `minHz`) / (`q` × 3),
    never less than (`maxHz` - `minHz`) / 6000.
- **Whether it is on the air.** The curve is the dialog's parametric
  panel whatever the other settings say. It reaches the transmitter when
  `txEqEnabled` is true and `txEqUseLegacy` is false. With `txEqUseLegacy`
  true the ten-band legacy EQ (`txEqPreamp`, `txEqBandsJson`,
  `txEqFreqsJson`) reaches it instead.

**Worked example.** A five-band curve saved by the dialog, Use Q Factors
on. `txEqParaEqData` is gzip, then base64url, of this JSON:

```json
{"band_count":5,"frequency_max_hz":3000,"frequency_min_hz":50,
 "global_gain_db":-2.5,"parametric_eq":true,
 "points":[{"frequency_hz":50,"gain_db":-6,"q":1.5},
           {"frequency_hz":300,"gain_db":3,"q":2},
           {"frequency_hz":1200,"gain_db":-1.5,"q":4},
           {"frequency_hz":2400,"gain_db":4,"q":3},
           {"frequency_hz":3000,"gain_db":0,"q":1}]}
```

`txEqCurve` is (one line on the wire, wrapped here):

```json
{"maxHz":3000,"minHz":50,"parametric":true,"points":[
 {"frequencyHz":50,"gainDb":-6,"q":1.5},{"frequencyHz":300,"gainDb":3,"q":2},
 {"frequencyHz":1200,"gainDb":-1.5,"q":4},{"frequencyHz":2400,"gainDb":4,"q":3},
 {"frequencyHz":3000,"gainDb":0,"q":1}],"preampDb":-2.5,"state":"saved"}
```

The line the dialog draws, to 0.01 dB: -7.04 dB at 50 Hz, -3.51 dB at
300 Hz, -4.00 dB at 1200 Hz, 1.50 dB at 2400 Hz and -2.50 dB at 3000 Hz.
At 2400 Hz the one point's bell gives its 4 dB, and the preamp takes 2.5
dB off. `tst_para_eq_curve` holds these values. The same points saved out
of order, for example 600 Hz before 598 Hz, come back sorted with the
second moved to 603 Hz.

**Changing the curve** (`txEqCurveVersion` 2). `txEq.setCurve`
(`curveJson` utf8) takes a curve in the shape above: `parametric`,
`preampDb`, `minHz`, `maxHz` and `points`, each point with `frequencyHz`,
`gainDb` and `q`. Any other key (`state` included) is ignored, so an app
may send back the curve it was shown with its edits. The Core takes what
the local TX EQ dialog lets an operator choose (Thetis's TX EQ panel) and
refuses anything else whole, changing nothing:

| What | The dialog's choice | Refused with |
| --- | --- | --- |
| The JSON | an object with the keys above, `parametric` a boolean and every other value a number | "The TX EQ curve was not understood." |
| Points | 5, 10 or 18 (the 5-band, 10-band and 18-band buttons; there is no adding or removing a single point) | "Choose a curve of 5, 10 or 18 points." |
| `minHz`, `maxHz` | 0 to 20000 Hz (Low and High), `maxHz` at least 1000 Hz above `minHz` once each is rounded to 0.001 Hz | "Choose a low and a high end from 0 to 20000 Hz, the high end at least 1000 Hz above the low end." |
| `preampDb` | -24 to 24 dB | "Choose a curve preamp from -24 to 24 dB." |
| `frequencyHz` | `minHz` to `maxHz` | "Choose each point's frequency between the curve's low and high ends." |
| `gainDb` | -24 to 24 dB | "Choose each point's gain from -24 to 24 dB." |
| `q` | 0.2 to 20 | "Choose each point's Q from 0.2 to 20." |

A curve it takes is rounded as a saved curve is read (the frequencies and
ends to 0.001 Hz, the gains and the preamp to 0.1 dB, Q to 0.01, round half
to even) and put in the order above: sorted, the first point moved to
`minHz` and the last to `maxHz`, the rest spaced. So an app that moves the
first or last point away from an end sees it back at the end, as in the
dialog, where the two end points are fixed to the range. The Core saves
the result as Thetis saves the panel's points (`SaveToJsonFromPoints`, then
gzip and base64url) and writes it to `txEqParaEqData`, so the desktop
dialog, TX profiles and Thetis-format settings keep the one saved curve.

`txEq.resetCurve` (no arguments) is the dialog's Reset button, which is
not Thetis's defaults: the preamp goes to 0 and every point to 0 dB with
Q 4, evenly spread from `minHz` to `maxHz`, keeping the curve's number of
points, its range and `parametric`. On a Core whose `txEqParaEqData` is
empty or unreadable the dialog holds the defaults above (ten points, 0 to
4000 Hz), so its Reset gives that flat curve, saved.

Each is that peer's own write of `txEqParaEqData` (section 7.3), under
every rule such a write meets: a receive-only Core takes it from a peer
offered `transmitSettingsVersion`; a Core that allows remote transmit takes
it from a session permitted to transmit; like the local dialog's edits it
is taken while the radio is on the air (section 7.3); it never keys. A refused write is refused with that write's
reason ("The radio is on the air. Try again when it stops.", "Transmit
configuration is unavailable on this receive-only Core.", or the transmit
gate's words). A taken one is followed first by the side-effect `delta`
carrying the new `txEqParaEqData` and `txEqCurve`, then by an accepted
`command.result` whose `curve` (utf8) is the `txEqCurve` the Core now
holds. Every other peer that gets `txEqCurve` gets the same `delta`. The
active TX profile is not saved by either verb: as with an edit in the
local dialog, `txProfile.save` saves it.

#### The CFC band editor (`cfcProfile`)

R-R3-49; `transmitSettingsVersion` 15. `cfcParaEqData` holds the CFC
dialog's two curves (compression and post-EQ) as Thetis saves them: gzip,
then base64url, of Thetis's own JSON for each curve joined by `<SEP>`
(frmCFCConfig.cs:492-557 [v2.10.3.15]). `transmit`'s `cfcProfile` is the
band editor in a form NereusSDR owns and documents here, one row per band,
so an app shows and edits it without gzip or Thetis's JSON. The Core
derives it every time `cfcParaEqData`, `cfcPrecompDb`, `cfcPostEqGainDb`
or the ten-band values change (`CfcProfile::publishedJson`,
`CfcProfile.cpp`) and sends it in the same delta.

- **Read-only.** `cfcProfile` is outbound; a write to it is refused as any
  outbound property is. It is changed with `cfc.setProfile` (below), at
  the Core, or by writing `cfcParaEqData`.
- **Who gets it.** Only a peer at agreed minor 11 whose hello declared
  `cfcProfile` 1. Every other peer's schema, snapshot and deltas carry
  none of it.
- **What it is.** One JSON object, compact, keys in sorted order. Key
  order is not significant; a reader ignores a key it does not know.

| Key | JSON type | Units and range | Meaning |
| --- | --- | --- | --- |
| `state` | string | `saved` or `legacy` | `saved`: `cfcParaEqData` holds curves the Core reads. `legacy`: it is empty or unreadable, and the keys hold the ten-band values an older profile keeps, shown as the desktop dialog shows them. An app treats a value it does not know as `legacy` |
| `revision` | string | 16 hex digits | The first 16 hex digits of the SHA-256 of this object's compact JSON without `revision` and `state`. Equal values give an equal revision on every host. `cfc.setProfile` takes it back as `expectedRevision` |
| `parametric` | boolean | | The dialog's Use Q check box (both curves, frmCFCConfig.cs:378 [v2.10.3.15]). True: each band is a bell of width Q. False: straight lines, and the Qs are unused |
| `minHz` | number | Hz, 0 to 20000 | The range's low end (the dialog's Low) |
| `maxHz` | number | Hz, 0 to 20000, at least 1000 above `minHz` | The range's high end (the dialog's High) |
| `precompDb` | number | dB, 0 to 16, in 0.1 dB steps | The pre-compression (`cfcPrecompDb`, which carries it rounded to a whole dB) |
| `postEqGainDb` | number | dB, -24 to 24, in 0.1 dB steps | The post-EQ gain (`cfcPostEqGainDb`, rounded to a whole dB) |
| `bands` | array | 5, 10 or 18 rows | The bands, lowest frequency first (the dialog's 5, 10 and 18-band buttons) |

Each band:

| Key | JSON type | Units and range | Meaning |
| --- | --- | --- | --- |
| `frequencyHz` | number | Hz, `minHz` to `maxHz`, in 0.001 Hz steps | The band's centre, one frequency for both curves (frmCFCConfig.cs:217-230 [v2.10.3.15]). The first band sits at `minHz` and the last at `maxHz` |
| `compressionDb` | number | dB, 0 to 16, in 0.1 dB steps | The band's compression |
| `compressionQ` | number | 0.2 to 20, in 0.01 steps | The compression bell's Q (used when `parametric` is true) |
| `postEqGainDb` | number | dB, -24 to 24, in 0.1 dB steps | The band's post-EQ gain |
| `postEqQ` | number | 0.2 to 20, in 0.01 steps | The post-EQ bell's Q (used when `parametric` is true) |

The ranges are the dialog's own controls (frmCFCConfig.Designer.cs
[v2.10.3.15]: Low and High 440-458 and 625-643, a band's frequency
261-274, compression 210-224, post-EQ gain 557-571, the Qs 112-130 and
595-613, pre-compression 401-415, post-EQ gain 330-344), and Low and
High are kept 1000 Hz apart as the dialog keeps them
(frmCFCConfig.cs:120-140 [v2.10.3.15]). A `legacy` editor has ten bands,
Q 4, `parametric` true and the range 0 to 4000 Hz widened to cover every
band (frmCFCConfig.cs:89-99 [v2.10.3.15]).

**Changing the editor** (`transmitSettingsVersion` 15). `cfc.setProfile`
(`profileJson` utf8, `expectedRevision` utf8) takes an editor in the
shape above: `bands`, `minHz`, `maxHz`, `parametric`, `precompDb` and
`postEqGainDb`, each band with all five keys. Any other key (`state` and
`revision` included) is ignored, so an app may send back the editor it
was shown with its edits. The Core first compares `expectedRevision` with
its own `cfcProfile`'s `revision`; when they differ it refuses "The CFC
settings changed on the Core. Check the new values and try again." and
changes nothing, so an app never overwrites a change it has not seen. It
then takes what the local CFC dialog lets an operator choose and refuses
anything else whole, changing nothing:

| What | The dialog's choice | Refused with |
| --- | --- | --- |
| The command | the two arguments above, both utf8 | "The CFC settings were not understood." |
| The JSON | an object with the keys above, `parametric` a boolean and every other value a number | "The CFC settings were not understood." |
| Bands | 5, 10 or 18 | "Choose 5, 10 or 18 bands." |
| `minHz`, `maxHz` | 0 to 20000 Hz, `maxHz` at least 1000 Hz above `minHz` once each is rounded to 0.001 Hz | "Choose a low and a high end from 0 to 20000 Hz, the high end at least 1000 Hz above the low end." |
| `precompDb` | 0 to 16 dB | "Choose a pre-compression from 0 to 16 dB." |
| `postEqGainDb` | -24 to 24 dB | "Choose a post-EQ gain from -24 to 24 dB." |
| `frequencyHz` | 0 to 20000 Hz; a band between the first and last strictly inside the range | "Choose each band's frequency between the low and high ends." |
| Band order | each band above the one before it, once rounded | "Keep each band's frequency above the one before it." |
| `compressionDb` | 0 to 16 dB | "Choose each band's compression from 0 to 16 dB." |
| `postEqGainDb` (a band's) | -24 to 24 dB | "Choose each band's post-EQ gain from -24 to 24 dB." |
| `compressionQ`, `postEqQ` | 0.2 to 20 | "Choose each Q from 0.2 to 20." |

An editor it takes is rounded as the Core keeps it (frequencies and ends
to 0.001 Hz, dB to 0.1, Q to 0.01), the first band moved to `minHz` and
the last to `maxHz` as the dialog keeps its end bands, and one frequency
and the Use Q choice applied to both curves. Unlike `txEq.setCurve` the
bands are not sorted: an editor out of order is refused. The Core saves
it as the dialog saves (`CfcProfile::encode`) and writes it to
`cfcParaEqData`, so the desktop dialog, TX profiles and Thetis-format
settings keep the one saved value; `cfcPrecompDb`, `cfcPostEqGainDb` and
the ten-band values follow from that write as they do from the dialog's.

It is that peer's own write of `cfcParaEqData` (section 7.3), under every
rule such a write meets: a receive-only Core takes it from a peer offered
`transmitSettingsVersion`; a Core that allows remote transmit takes it
from a session permitted to transmit; like the local dialog's edits it is
taken while the radio is on the air; it never keys. A refused write is
refused with that write's reason. A taken one is followed first by the
side-effect `delta` carrying the new `cfcParaEqData`, `cfcPrecompDb`,
`cfcPostEqGainDb`, any ten-band values that moved and, to a peer that
declared `cfcProfile` 1, `cfcProfile`; then by an accepted
`command.result` whose `profile` (utf8) is the `cfcProfile` the Core now
holds. The active TX profile is not saved: `txProfile.save` saves it.

### 7.2 Deltas

The station collects property changes and sends them at most every 50 ms
(`kDefaultDeltaFlushMs`), one `delta` per object carrying the latest value
of each changed property. It collects them for each device's session
separately (`MirrorView`, iPhone app plan Task 72), so a device that
signs in never takes another device's pending changes with it. The desktop client collects its own writes on
the same 50 ms period (`kDefaultWriteFlushMs`).

### 7.3 Property writes and property.result

A client changes a `bidirectional` property with `property.write`: the
object `key`, a list of property entries and, from a client that
negotiated results, a nonzero `writeId`.

The station applies each entry and refuses, with a reason, a duplicate
property in one write, an unknown property or a wrong wire kind, an
`outbound` property, a read-only object (`ioBoard`, `amplifier`, `rfkit`,
`stationTci`, `accessoryData`, `accessorySettings`), a `stepAtt` or `alexAntennas` write from a peer below minor
11 or while the Core has no controller behind it, a raw write of `radio`'s
`rfKitEnabled` (changed only with `setRfKitEnabled`), the keying
`transmit` properties (`mox`, `tune`, `voxEnabled`, `twoToneActive`) on a
receive-only station, whether or not the station mirrors them ("Transmit
configuration is unavailable on this receive-only Core."), any
`transmit` property on a receive-only station from a peer below agreed
minor 11 (it was never offered `transmitSettingsVersion`; the same
reason), on a Core below `transmitSettingsVersion` 13 any other
`transmit` property on a receive-only station while its radio is on the
air ("The radio is on the air. Try again when it stops.": keyed through
its `MoxController` from any source, a hardware PTT included, until the
hand-back to receive ends; TUNE on; or the two-tone test running; at 13
they are taken on the air as a local window takes them), a
`transmit` setting outside its setter's range, with the range ("Choose a
tune power from 0 to 100 W.", "Choose a VOX level from -80 to 0 dB.",
"Choose a VOX delay from 1 to 2000 ms.", "Choose a monitor level from 0.0
to 1.0.", "Choose a PROC level from 0 to 20 dB.", "Choose an AM carrier
level from 0 to 100 percent.", "Choose a mic level from -50 to 70 dB.",
"Choose a Line In gain from -34.5 to 12.0 dB.", "Choose a TX EQ preamp
from -12 to 15 dB.", "Choose a TX EQ Nc from 32 to 8192.", "Choose a TX EQ
cutoff of 0 (peaking) or 1 (notch).", "Choose a TX EQ window of 0
(Blackman-Harris) or 1 (Hann).", "Choose a CFC pre-compression from 0 to 16
dB.", "Choose a CFC post-EQ gain from -24 to 24 dB.", "Choose a phase
rotator frequency from 10 to 2000 Hz.", "Choose from 2 to 16 phase rotator
stages.", "Choose a leveler maximum gain from 0 to 20 dB.", "Choose a
leveler decay from 1 to 5000 ms.", "Choose an ALC maximum gain from 0 to
120 dB.", "Choose an ALC decay from 1 to 50 ms."; a ten-value array of
the wrong length or with a value out of range is refused whole: "Choose
ten TX EQ band levels, each from -12 to 15 dB.", "Choose ten TX EQ band
centres, each from 10 to 22000 Hz.", "Choose ten CFC compression levels,
each from 0 to 16 dB.", "Choose ten CFC band centres, each from 0 to 20000
Hz.", "Choose ten CFC post-EQ band levels, each from -24 to 24 dB.";
at `transmitSettingsVersion` 5, "Choose a DEXP attack time from 2 to 100
ms.", "Choose a DEXP detector time from 1 to 100 ms.", "Choose a DEXP
release time from 2 to 1000 ms.", "Choose a DEXP expansion ratio from 0.0
to 30.0 dB.", "Choose a DEXP hysteresis ratio from 0.0 to 10.0 dB.",
"Choose a look-ahead time from 10 to 999 ms.", "Choose a VOX trigger
filter cut from 100 to 10000 Hz.", "Choose an anti-VOX gain from -60 to 60
dB.", "Choose a tone frequency from -20000 to 20000 Hz.", "Choose a
two-tone level from -96 to 0 dB.", "Choose a two-tone power from 0 to 100
percent.", "Choose a second tone delay from 0 to 1000 ms.", and a band map
refused whole: "Choose a power from 0 to 100 W for each band.",
"Choose a tune power from 0 to 100 W for each band."; a Hermes
Lite 2 says "Choose a tune power from 0 to 99." and "Choose a tune power
from 0 to 99 for each band."), `stepAtt`'s `attOnTxEnabled`,
`attOnTxValue` and `forceAttWhenPsOff` on a receive-only station from a
peer not offered `transmitSettingsVersion` (the receive-only reason) or,
below version 13, while its radio is on the air (the on-air reason), and
`attOnTxValue`
outside the Core's range ("Choose an ATT on TX value from 0 to 31 dB.", on
a Hermes Lite 2 from -28), a `pureSignalSettings` write from a peer
offered `transmitSettingsVersion` 7 while the radio is on the air (the
on-air reason; from such a peer an accepted write is applied to the Core's
PureSignal at once, and from any other peer it is kept and applied when
PureSignal next starts, as before), and
DSP settings from a peer that did not negotiate them
(`StationServer::handlePropertyWrite`). The on-air check is read once for
the whole write, before anything in it is applied.
A write to an `outbound` property is refused before anything is applied,
with the reason "The Core sets this itself; it cannot be changed from
here.", unless one of the earlier, more specific refusals above applies
first; this covers the station's own readings, such as a slice's signal
strength, as well as properties changed through a command.

`alexAntennas`' transmit properties (`txAntennas`, `blockTxAnt2`,
`blockTxAnt3`, `rxOutOnTx`, `ext1OutOnTx`, `ext2OutOnTx`, `rxOutOverride`,
two-way from `radioHardwareVersion` 5 and 6) are not held while the radio
is on the air, and a receive-only station takes them from any peer that
was offered the object: they key nothing, and Thetis changes them while
transmitting (section 6.3). A `txAntennas` list that asks for a port
blocked for transmit keeps that band's antenna and settles with "An
antenna blocked for transmit cannot be a band's TX antenna." A current
window changes one band's TX antenna with the `setAlexTxAntenna` command
instead (section 6.3), so its edit cannot put back another band the Core
changed; the whole-list write stays for a window that sends it.

For a peer offered `radioAntennaRowsVersion` 1, the two `ForRadio` verbs
carry the current radio's canonical MAC along with one band's existing
antenna arguments. The Core checks the exact argument types and current
connected MAC before it classifies a change, when a held confirmation
resumes, and on the RadioModel owner thread immediately before applying
the one-band Alex operation. An edit for another or disconnected radio
changes nothing. The existing shared-setting questions, transmit holder
rules, blocked-port refusal and same-band ordering still apply. Different
bands remain independent; no whole-list replay is involved.

After the whole batch, the station reads each property back. When the
agreed minor is at least 5 (`kDspControlSessionProtocolMinor`) and the
write carried a `writeId`, it answers with `property.result`: the same
`key` and `writeId` and one result per property, `{property, accepted,
reason, hasValue, value}`, where `value` is the value the station kept. A
write the station kept at a different value is not accepted, and its reason
says why. The desktop client reads results only when `propertyResultVersion`
is at least 1 (`propertyResultsAvailable()`).

Side effects of a write on other properties go back as a `delta`. A peer
that did not negotiate results, or wrote without a `writeId`, also gets its
requested properties back in that `delta`. Both go to the writer alone.

**Echo per writer** (iPhone app plan Task 72; the several-devices design,
ruling 5.7). What a write changes, the property written and any side effect
on another object (two slices on one receiver share its noise blanker, so
a change to one's blanker changes the other's), never comes back to the
writer as a `delta`: the writer has its `property.result` and the readback
above. Every other device's session receives each change as an ordinary
`delta`. With one device on the Core nothing on its wire changes. The
station's tests `tst_station_multi_session` and `tst_mirror_view` hold it
to this; the several-client fixtures of later tasks (`two-devices` and
after) carry it on the wire.

**Another device's slice** (iPhone app plan Task 73; the several-devices
design, ruling 5.9). A device changes only its own slices. A
`property.write` to a `slice:<id>` the writer does not own, or to any
`marker:<id>`, is refused before anything is read or applied: every
property answers not accepted, with no value, and the reason "That slice
belongs to <the owner's name>. It can be changed only there." (the name as
`connectedDevices` numbers it; "the Core" for a slice nobody owns). Nothing
else comes back. The commands `removeSlice`, `setActiveSliceById`,
`nnr.setDiagnostics`, `nnr.resetTuning`, `nnr.tryAgain`, `notch.add`,
`notch.addAtSlice`, `slice.selectBand`, `requestSliceSampleRate`,
`requestStreamCentre` and `requestStreamCtunPinned` naming a slice that is not the requester's
(another device's, one nobody owns, or one held for a device) are refused
with the same reason, whatever receivers are in use (section 9.1). On the
requester's own slice, a C-Tune centre change or pin and a slice's band
change follow the receiver rules of section 7.5, and a sample-rate change
the shared-setting rules of section 7.6.

**Waiting for you to confirm** (iPhone app plan Task 74; the
several-devices design, section 7.3). A write that would disturb another
device, from a device with `sessionHolderVersion` 1, is not applied. Its
`property.result` answers every property not accepted, with the value the
Core keeps and the reason "Waiting for you to confirm.", and a
`confirm.request` follows with `forWriteId` naming the write (section
7.5). Today that is a band change of the anchor's slice on a receiver
another device shares. A window without the feature is never asked: the
same write is refused with "This change would affect <names>. Update
NereusSDR to confirm changes that affect other devices.", the names as
`connectedDevices` numbers them.

A receiver property the station accepted reads back with its new value at
once: the station's receive DSP applies it afterwards, on its own thread,
never in the thread that answers the write (R-R3-39). So the result and
the `delta` show the value the station kept, not a value WDSP has already
reached. A change WDSP itself still refuses afterwards (a noise-reduction
model that is not loaded) comes back later as a `delta`: the slice's
noise-reduction status carries the reason, and a refused noise-reduction
selection returns to the one before it.

A property write never keys the transmitter: `transmit`'s `mox` and
`tune` travel from the Core only (outbound), and a write of either is
refused ("Use the transmit button."); a device keys with the keying verbs
(section 18.6),
and the transmit safety gates stay at the station (section 18). With
`remote_transmit` allow, the other `transmit` properties and the
transmit-side settings keys are written only by a session `txPermitted`
allows (refused otherwise with the gate's sentence); while another
device's holder is on the air, a change to the transmit path is refused
with the on-air sentence (section 18.4).

#### The diversity pattern (`diversityPattern`)

Phone wire batch; `diversityPatternVersion` 1. The desktop's Diversity
dialog draws a sensitivity pattern (a polar lobe) for its slice from the
slice's frequency, diversity phase and gain. The Core computes the same
pattern with the same function the dialog's radar draws from
(`DiversityPattern`, `src/core/DiversityPattern.cpp`, ported from Thetis
`DiversityForm.CalcVrms`) and sends it on each slice as
`diversityPattern`, so an app draws exactly what the desktop draws without
the formula.

- **Read-only.** A write is refused as any outbound property is ("The
  Core sets this itself; it cannot be changed from here."). The pattern
  moves when the slice's `frequency`, `diversityPhaseDeg` or
  `diversityGainDb` moves; a writer of one of those gets the new pattern
  in the side-effect `delta` that follows its `property.result`, and every
  other declaring peer in the same `delta` as the input that moved it.
- **When it is sent.** Only when a rounded sample changes: a tuning step
  small enough to leave every sample where it was sends no pattern.
- **Who gets it.** Only a peer at agreed minor 11 whose hello declared
  `diversityPattern` 1. Its `SliceModel` schema, its slice snapshots and
  its deltas carry the field. Every other peer's carry none of it, and a
  delta that would carry only the pattern is not sent to them.
- **Which slice.** Every slice carries its own. The desktop dialog shows
  the first slice (Slice A); an app shows the same slice to match it.
- **What it is.** One JSON object, compact, keys in sorted order. A
  reader ignores a key it does not know.

| Key | JSON type | Units and range | Meaning |
| --- | --- | --- | --- |
| `crossFire` | boolean | | Thetis's cross-fire term (adds half a turn to the second antenna). The desktop has no switch for it: always false |
| `points` | array of numbers | 120 numbers, 0 to 1, each to 3 places | Relative sensitivity at bearing `i` × `stepDeg` degrees, `i` from 0, bearing 0 north, clockwise; each divided by the largest, so the peak is 1 (all near 0 are sent as they are) |
| `spacingMeters` | number | metres | The antenna spacing the pattern assumes. The desktop has no setting for it: 5.5 |
| `stepDeg` | number | degrees | The bearing step between points: 3 |

The frequency, phase and gain the pattern was computed from are the
slice's own `frequency` (Hz), `diversityPhaseDeg` and `diversityGainDb`,
sent in the same `delta` when they move it. The desktop has no antenna
orientation setting; bearing 0 is the radar's north.

- **Drawing it.** The desktop draws point `i` at radius `points[i]` ×
  0.85 of its circle's radius, at bearing `i` × `stepDeg` (north up,
  clockwise), joins the points into a closed polygon and fills it. The
  steering handle sits at bearing `diversityPhaseDeg` on the same 0.85
  radius.
- **The values** (the Core computes them; an app does not). For bearing
  θ: `phi` = (π when `crossFire`, else 0) + cos(θ + phase in radians);
  over 20 steps `i` of one RF cycle at the slice frequency `f`,
  v1 = sin(2π `i` / 20) and v2 = sin(2π `i` / 20 + `phi` - 2π
  `spacingMeters` `f` / 299792458) × 10^(gain dB / 20); the sensitivity is
  the mean of (v1 + v2)² × 5.5, and `points` is it divided by the largest
  of the 120.

**Worked example.** Slice at 14.2 MHz, `diversityPhaseDeg` 0,
`diversityGainDb` 0: `points[0]` (north) is 1, `points[30]` (east) 0.518,
`points[60]` (south) 0.069 and `points[90]` (west) 0.518.

#### The RADE status (`radeSynced`, `radeFreqOffsetHz`)

`radeStatusVersion` 1. While a slice is in RADE (`dspMode` `RADE_U` or
`RADE_L`) the desktop's VFO flag shows one RADE row built from the RADE
decoder: the last speaker's callsign (or "RADE"), a sync dot, the SNR and
the frequency offset (`VfoWidget::setRadeSynced`, `setRadeSnrLabel`,
`setRadeFreqOffset`). The slice already carries `lastRadeRxCallsign` and
`snrDb`; these two carry the rest, from the same decoder readings the
desktop's flag reads (`RadeChannel::syncChanged` and
`freqOffsetChanged`, from `rade_sync()` and `rade_freq_offset()`).

- **`radeSynced`** (bool). True while the slice's RADE decoder holds
  sync. It follows the decoder's own changes, and is false while the
  slice is not in RADE, after a RADE sideband change, and whenever the
  slice's decoder is replaced, until the new decoder reports sync. The
  Core keeps `snrDb` as it was when sync is lost (its end-of-over reports
  read it); the desktop's flag stops showing it at that moment, and an
  app does the same by reading `radeSynced`.
- **`radeFreqOffsetHz`** (f64, Hz). The decoder's frequency offset,
  signed, as reported. The decoder reports it only while it holds sync,
  so after sync is lost this holds the last value; it is 0 until the
  first report.
- **Read-only.** A write of either is refused as any outbound property is
  ("The Core sets this itself; it cannot be changed from here.").
- **When they are sent.** When they change, no faster than the decoder
  reports (each value it reports is one the desktop's flag repaints for),
  in the same `delta` as `snrDb` when they change together.
- **Who gets them.** Only a peer at agreed minor 11 whose hello declared
  `radeStatus` 1. Its `SliceModel` schema, its slice snapshots and its
  deltas carry the fields. Every other peer's carry none of them, and a
  delta that would carry only these is not sent to them.

**Showing them as the desktop's flag does.** The row shows only while the
slice is in RADE. The prefix is `lastRadeRxCallsign`, or "RADE" while it
is empty.

| State | Row |
| --- | --- |
| `radeSynced` true and `snrDb` a number | prefix, a filled dot, the SNR, then the offset: `KG4VCF ● 12dB +38Hz` |
| otherwise | prefix, a hollow dot, three hyphens: `RADE ○ ---` |

- **SNR.** `snrDb` cut to a whole number toward zero, then `dB` with no
  space (12.9 shows `12dB`, -3.7 shows `-3dB`).
- **Offset.** One space after the SNR, then `+` when `radeFreqOffsetHz` is
  0 or more (a negative number carries its own `-`), the offset cut to a
  whole number toward zero, then `Hz` with no space (38.7 shows `+38Hz`,
  -12.4 shows `-12Hz`, and -0.4 shows `0Hz`: no sign, since it is below 0
  but cuts to 0).
- **Dot color.** Filled: yellow `#e0e040` when the SNR shown is below 5 dB
  (compared before cutting), green `#00ff88` from 5 dB up. Hollow: gray
  `#505050`.

#### The RADE reason (`radeReason`)

`radeReasonVersion` 1. A slice in RADE with no working RADE decoder plays
silence, never its sideband. `radeReason` says why, in a plain sentence
the Core writes (`RadioModel::radeStartReason`); it is empty while the
slice decodes, while it is not in RADE, before its receiver runs and while
a saved layout waits to be placed.

| Cause | `radeReason` (slice A shown) |
| --- | --- |
| The decoder could not be created | RADE could not start on slice A: its RADE decoder could not be created. |
| The decoder was created and did not start | RADE could not start on slice A: its RADE decoder did not start. |
| The configured RADE model file is not there | RADE could not start on slice A: the RADE model file was not found. |
| A saved layout's RADE receiver is not running | RADE could not start on slice A: that receiver is not running. |
| A saved layout started RADE on another slice, which decodes | RADE could not start on slice A: slice B is already decoding RADE. |
| Anything else that leaves it without a decoder | RADE is not decoding on slice A. Change slice A to another mode and back to RADE to start it. |

- **When it changes.** When the slice's decoder starts or fails to, when
  its mode changes, and when the slice list or a route changes. Leaving
  RADE clears it. Nothing restarts a decoder by itself when the cause
  clears: the reason stays until the slice's mode changes, and when the
  slice decoding RADE for a saved layout leaves RADE, the other slice's
  reason becomes the last row's.
- **Read-only.** A write is refused as any outbound property is.
- **Who gets it.** Only a peer whose hello declared `radeReason` 1. Its
  `SliceModel` schema, its slice snapshots and its deltas carry it. Every
  other peer's carry none, and a delta that would carry only it is not
  sent to them.

**Showing it as the desktop's flag does.** While `radeReason` is not
empty the RADE row reads the prefix, a hollow gray dot and `off`
(`RADE ○ off`), and the row's tooltip is the reason. The row stays shown.

**The `vax` object** (iPhone app plan Task 25, `vaxVersion` 1). The Core
takes a write of `ch<N>RxGain`, `ch<N>Muted` or `txGain` only from a peer
that declared `vax` 1, on a Core at `vaxVersion` 1 (otherwise "Update this
app to change VAX on the Core's computer."). A level outside 0 to 1, or not
a number, is refused "A VAX level goes from 0 to 1." `txGain` is taken only
from a peer the station transmit gate permits (section 18.1) and is
otherwise refused with the gate's reason. A receive level and a mute apply
at once, on or off the air, as the applet's controls do; none keys the
radio. Each accepted write is applied to the Core's audio engine and saved
under the applet's own keys (`audio/Vax<N>/RxGain` and `audio/Vax<N>/Muted`
as `0.000`-style and `True`/`False` text, `audio/TxGain`), so the applet on
that computer shows it and it outlives a restart. The other properties are
the Core's.

### 7.5 Receivers several devices share

iPhone app plan Task 74 (the several-devices design, sections 6.1 to 6.4,
7.3 and 7.4). Up to four devices share the radio's receivers. A slice
joins any receiver whose window covers its frequency, whoever claimed it,
as it always has; two devices' slices can therefore share one receiver.

**The anchor.** The device whose slice claimed a receiver anchors it.
When the anchor's last slice leaves the receiver, or its control passes to
nobody, the anchor passes to the device whose slice has been on it
longest; nobody is asked or told. When
the last slice leaves, the receiver is free. A shared receiver's window
does not follow a slice's tuning inside it (the slice moves only its own
shift, as several slices on one receiver always have).

**The C-Tune pin** of a shared receiver is its anchor's.
`requestStreamCtunPinned` from another device is refused with "This
panadapter shows <anchor's name>'s receiver. Its C-Tune setting is
<anchor's name>'s.", or while the anchor is not connected "This
panadapter shows the receiver of <anchor's name>, which is not connected
now. Its C-Tune setting stays with <anchor's name> until it is back or
its three minutes are up.".
A pin lasts through its anchor's link dropping and its coming back as the
same device (its `streamCtunPinned` is as it left it); it ends when the
anchor leaves for good (`session.leave`, a token window's end, the end of
its 180 s, revocation).

**The anchor moves its panadapter.** A `requestStreamCentre` from the
anchor that would leave another device's slice outside the new window is
held: its `command.result` is not accepted, with the reason "Waiting for
you to confirm." and `values` holding `phase` `needsConfirmation`, and a
`confirm.request` of kind `panMove` follows. One that would leave the
anchor's own slice outside is refused as before. On proceed the window
moves; each other device's slice outside it moves to another receiver
(one whose window covers it, or a free one) or, with none free, closes,
and its device is told (`notice` `sliceMoved` or `sliceClosed`).

**The anchor changes band.** A `property.write` of the `frequency` of one
of the anchor's slices, outside its receiver's window, while another
device's slice shares the receiver, is the same question (`panMove`),
held as section 7.3 says, with `change` `{label, from, to}`:
"Receiver <n>" and the bands, "20 m" and "40 m". On proceed the receiver
follows the anchor's slice, centred on its new frequency; each other
device's slice the new window no longer covers moves or closes, as above;
one it still covers stays. When the anchor has another slice of its own
on the receiver that the new window would not cover, the change is not a
pan move: the slice leaves for another receiver as it always has (design
ruling 6.5a). Cancel changes nothing. With nobody else on the receiver,
nothing changes from before.

**A device that does not anchor moves its panadapter** by taking it,
with its slices there, to a free receiver centred where it asked; the
anchor is not disturbed and nobody is asked. Its slices must fit the new
window, as C-Tune requires.

**Taking a receiver** (the several-devices design, section 6.4). A
request that needs a receiver (`addSlice`, `addSliceOnPan`, a retune out
of a window, a panadapter move by a device that does not anchor) and is
refused because every receiver is in use is answered with the refusal,
whose words now end "The radio's receivers are in use by <names>.", and,
to a device with the feature, a `confirm.request` of kind `takeReceiver`
with one choice per receiver in use. On proceed with a choice the Core
checks again, closes every other device's slice on that receiver (even
one the new window would cover), tells each owner (`notice`
`receiverTaken`, with Take it back), and applies the held request on the
freed receiver. The taker's own slices there stay when the new window
covers them; a receiver where the taker's own slice would be left outside
is offered with `takeable` false and `why` "Your slice E would close.",
and a receiver the taker's own panadapter uses cannot give it a new one
(`takeable` false, "Your panadapter already uses this receiver."). When
the slice cap, not the receivers, is full, the question is `takeSlice`,
one choice per slice of another device, and taking one closes only that
slice (`notice` `sliceTaken`, with Take it back). A receiver or slice free
by the time of the answer is used without taking anything.

**Take it back.** `notice.takeBack {id}` asks the same question the other
way: a `takeReceiver` whose first choice is the receiver the taker now
holds, then any receiver free by then (for a slice, a `takeSlice` with the
taker's slice and, when one is free, a choice with `sliceId` -1). Its own
`command.result` is "Waiting for you to confirm." with `phase`
`needsConfirmation`. On proceed the Core closes what the choice names
(telling its owner, with Take it back) and recreates the device's closed
slices at their frequencies, modes and panadapters, with their settings.
Once taken back, a notice cannot be taken back again ("That can no longer
be taken back."). For a `controlTaken` notice (`sliceAccessVersion` 2)
there is no question: `notice.takeBack {id}` is `slice.takeControl` the
other way, with the `sliceId`, `incarnation` and `controlRevision` the
notice's slice entry carries, under the same checks and the same
transmit rules (below). Its result is the take's own: accepted with the
slice's new `controlRevision` in `values` and the objects it reached, or
refused in the take's words (the slice transmitting, closed, or its
control moved on since the notice). A notice whose slice was closed or
whose control moved on is then forgotten ("That can no longer be taken
back." after). The device that lost control this way is sent its own
`controlTaken` notice, with Take it back. A peer below
`sliceAccessVersion` 2 is sent `takeBack` false on `controlTaken` and its
`notice.takeBack` for one is refused ("That can no longer be taken
back.").

**Listening and control** (`sliceAccessVersion` 1; the slice control
and shared listening design, docs/architecture/2026-09-28-slice-control-
and-listening-design.md). Each slice has one controller (its owner) or
none, and any number of listeners; the controller is always one of them.
A device sees and hears every slice it has joined and changes only the
one it controls: a write or a slice verb from a listener is refused with
"Slice <letter> is controlled by <name>. Take control to change it."
("Nobody controls slice <letter>. Take control to change it." with no
controller). A session with the feature receives `slice:<id>` for every
slice it has joined and `marker:<id>` for every other; a join or a leave
swaps them as a change of owner does (section 7.1). The `access:<id>`
object says who controls and who listens. The verbs (section 9.1):

- `slice.listen {sliceId, incarnation}` joins the slice. Nothing is
  allocated: no slice, receiver or DDC, so it works with every receiver
  and the slice cap in use. Already joined is accepted and changes
  nothing. Its result carries the slice's `controlRevision`.
- `slice.stopListening {sliceId, incarnation}` leaves it. From the
  controller it is refused, "You control slice <letter>. Use Release to
  leave it."; not joined, it is accepted and changes nothing. When it was
  the device's active receive slice, the next slice the device has joined
  (in creation order) becomes it. A slice left with no controller and no
  listener closes, the Core's last slice included (the Core then has no
  slice; see "A Core with no slice" below).
- `slice.takeControl {sliceId, incarnation, controlRevision}` makes the
  device its controller in one change: no slice is closed or made, and
  its receiver, channel and audio stay. Every slice can be taken (JJ,
  2026-09-30): the one refusal is while the slice is transmitting (the
  transmit slice of a holder on the air, or the one a keyed radio holds),
  checked when the take is applied: "Slice <letter> is transmitting. Take
  control once it stops." The former controller stays a listener (its
  `slice:<id>` stays; it is sent `notice` `controlTaken`) and every other
  listener stays, when it can listen: a session with the feature, the
  Core's own position, or a device away within its 3 minutes (it finds
  itself a listener when it returns). A former controller that cannot (a
  session without the feature, or a device neither here nor away) loses
  the slice: it leaves it, is sent no `controlTaken`, and an older window
  left with no slice ends (`takenOver`), as when a take closes an older
  window's last slice. A slice the
  Core keeps for a device may be taken. A hosting desktop's own slice
  passes like any device's (take-over parity): the desktop is the station
  device, stays on as a listener, and is told with `controlTaken` and Take
  it back. With nobody at the Core's desktop (a Core with no hosting
  desktop window, such as a headless Core: no desktop takes the station
  device's notices), the Core's own slice is taken at once
  (`sliceAccessVersion` 3): no question and nobody told; the station
  device stays joined as a listener. To a session below
  `sliceAccessVersion` 3 that one take is refused as before: "Slice
  <letter> is run by the Core itself, so control of it cannot pass to this
  device." When the
  former controller holds transmit on the slice, the transmit flag moves
  to another of its slices, or with none transmit is released; its
  remembered transmit choice no longer names the slice, and the new
  controller's transmit binding prefers its other slices until it picks
  this one with `tx.setTxSlice`. Its result carries the new
  `controlRevision`.
- `slice.release {sliceId, incarnation, controlRevision}`, from the
  controller only ("Only the device that controls slice <letter> can
  release it."): the controller is cleared and it leaves. The slice stays
  for its other listeners with no controller (nobody adopts it, not even a
  device alone on the Core; `slice.takeControl` does), refused while it
  transmits ("Slice <letter> is transmitting. Release it once it
  stops."), or closes when nobody else is on it, the Core's last slice
  included. A `removeSlice` from a controller of a slice others listen
  to acts as this release, from an older window too, and so does its
  `removeSlice` of the Core's last slice.

  A Core with no slice: zero slices is a valid idle state. No receiver
  streams (Protocol 1 still streams its first receiver, as the protocol
  needs one), no slice is active, nothing is bound for transmit and every
  key is refused (`noTransmitSlice`, section 18). `addSlice` and
  `addSliceOnPan` make a working slice again, and a device let in with no
  slice is given one (section 7.1). Its layout is not saved, so after a
  restart, or when the radio connects with no slice, the Core makes one
  Slice A that nobody owns.
- `slice.setListenLevel {sliceId, incarnation, level, muted}` sets the
  device's own listening level (0 to 1) and mute for a slice it listens
  to without controlling it. It changes only what that device hears: the
  slice's AF gain and mute, which its controller sets, are untouched, and
  two listeners of one slice each hear it at their own level. A new
  listener, and a former controller that stays on as a listener, start at
  the slice's AF gain at that moment. From a device not listening to the
  slice it is refused, "You are not listening to slice <letter>. Listen in
  first."; a level outside 0 to 1, or not a number, is refused as
  unreadable. Each listened slice joins the device's audio centered at
  this level. The Core applies each slice's AF gain in its own mixer (the
  operator's audio at the AF gain, each listener's at its own level), not
  in the receive channel, so the VAX bus and slice taps carry the slice
  at a fixed level whatever the AF gain, and stay audible at AF 0.
- `setActiveSliceById` from a session with the feature makes any slice it
  has joined its active receive slice; only a slice it controls also
  becomes its active slice (ruling 5.10). From an older window, today's
  rule.

Each verb names the slice by `sliceId` and `incarnation` (the
`access:<id>` object's): a slice closed and its letter made again is
refused, "That slice has closed. Choose it again from the list.". Take
and release carry the `controlRevision` the device saw: of two devices
that saw the same one, the first is applied and the other refused,
"Someone else changed who controls slice <letter>. Look again and try
once more.".

**Listeners, the refused Add and the take questions** (`sliceAccessVersion`
1, slice control plan Task 9). An `addSlice` or `addSliceOnPan` from a
session with the feature, refused because every receiver or the slice cap
is in use, carries `usableSlices` in its `values` (`utf8`): a JSON array,
one entry per live slice in id order, `{sliceId, incarnation, letter,
controllerDeviceId}` (`controllerDeviceId` empty for a slice nobody
controls), so the device can offer to listen to one with `slice.listen`.
The `takeReceiver` or `takeSlice` question that follows is unchanged. In
a question to a session with the feature (a `panMove`, `takeReceiver` or
`takeSlice`, Take it back's included) each slice named, in `affected` and
in a choice, also carries `listenerDeviceIds`, an array of the device ids
that listen to it, its controller first. On proceed the Core also asks
again when a slice the answer would close or move has a listener the
question did not name; a listener who left since is no reason to ask
again. When the answer closes a slice, each of its listeners other than
its controller and the device that answered is sent `notice`
`sliceClosed` (no Take it back) naming the slice and who closed it:
"<name> took the receiver slice <letter> was on. You were listening to
it.", "<name> took slice <letter>, which you were listening to.", or for
a pan move "<name> moved their panadapter. Slice <letter>, which you were
listening to, closed: no receiver was free.". A session without the
feature gets none of these.

**An older window** (a session without `sessionHolderVersion` 1) is never
asked: it gets the refusal only, naming the devices involved. When a take
closes its last slice its session ends: "<taker's name> took the receiver
this app was using. Update NereusSDR to share the Core.", not retryable,
`code` `takenOver`. With no slice for it at sign-in it is refused,
retryable: "All the radio's slices are in use. Try again when another
device closes one." when the slice cap is full, "All the radio's
receivers are in use. Try again when another device frees one." otherwise
(section 12.4).

**`confirm.request`** (Core to device, to a session with
`sessionHolderVersion` 1 only): `id` (number, unique on this Core), `kind`
(`panMove`, `takeReceiver`, `takeSlice`, `sharedSetting` (section 7.6),
`takeTransmit` (section 18.9)), `reason` ("Waiting for you to
confirm."), `affected` (array), `expiresInMs` (60000, `confirmExpiryMs` in
section 15), and, optionally, `change` `{label, from, to}` (absent for a
take), `choices` (a take of a receiver or a slice), `holder` (a
`takeTransmit` only, section 18.9), and `forCommandId`, `forWriteId` or
`forSettingsKey` naming the held change. `affected`
has one entry per disturbed device, `{deviceId, deviceName,
deviceShortName, state, holdsTransmit, slices}`, each slice `{sliceId,
letter, frequencyHz, band, mode, adc, streamIndex, effect}`: `state` is
`listening` or `away`, `mode` the slice's `dspMode` value, `band` its
`Band` value, `adc` its receiver's ADC from 0, `streamIndex` its receiver
from 0 (shown as "Receiver `streamIndex` + 1", "ADC `adc` + 1"), `effect`
`moves` or `closes` for a pan move, and for a shared setting also
`changes` (keeps receiving, differently) or `pausesWhileTransmitting`
(section 7.6). A take's `affected` is empty: its choices say
what each would close. A `takeReceiver` choice is `{choice, streamIndex,
adc, centreHz, rateHz, anchorName, slices, devices, takeable, why}`, its
slices `{sliceId, letter, deviceId, deviceName, frequencyHz, mode, band,
txSlice}` and its devices `{deviceId, name, shortName, state,
lastActivitySeconds}`; a free receiver (offered only by Take it back) has
no slices. A `takeSlice` choice is `{choice, sliceId, letter, deviceId,
deviceName, deviceShortName, state, frequencyHz, mode, band, txSlice,
streamIndex, adc, takeable, why}`.

**Answers** (section 9.1): `confirm.proceed {id, choice}` (`choice` -1
for a `panMove` or a `sharedSetting`) or `confirm.cancel {id}`. Nothing a
device sent before its answer changes anything. A device has one open
question; a new one replaces it, and its session ending drops it. A
question expires 60 s after it is sent (`expiresInMs`): a later
`confirm.proceed` is refused "That question has expired. Make the change
again." and changes nothing (iPhone app plan Task 75). On proceed the Core computes
what the change reaches again: when that names a device or an effect the
operator was not shown, the proceed is answered "Waiting for you to
confirm." with `phase` `needsConfirmation`, a new `confirm.request`
follows, and nothing is applied. Otherwise the change is applied exactly
as the original request would have been, and the proceed's
`command.result` carries the readback (ruling 7.4a): for a property
write, `objectKey` and the settled value of every property the write
named in `values` (its side effects on the written object reach the
writer as the usual `delta`); for a settings write, `settingsKey` and
`value` (the stored value) in `values`; for a command, the original
command's own `affected` and `values` (a `requestSliceSampleRate`, which
the Core runs on a later turn, answers the proceed when it has run). The change reaches every other session as a
`delta`. A proceed that is refused ("That question is no longer open.
Make the change again.", "That choice is not in the list. Make the change
again.", "What this change reaches has changed. Make the change again.")
carries no readback. The question is always sent after the answer to the
request that raised it. A `sharedSetting` proceed is also refused "That
setting changed since you asked. Make the change again." when what the
change acts on moved since the question was asked, whoever moved it.
Every slice a question names (the written slice, a `sliceId` argument, the
slices a move carries) must still be the asking device's at proceed. A
question whose slice closes or passes to another owner is dropped at once,
since the Core hands the lowest free id to the next slice; its later
`confirm.proceed` is refused as changed ("That setting changed since you
asked. Make the change again." for a `sharedSetting`, "What this change
reaches has changed. Make the change again." for any other kind) and
changes nothing, and its `confirm.cancel` is accepted.

**`notice`** (Core to device, `sessionHolderVersion` 1 only): `id`,
`kind`, `reason`, `secondsAgo` (whole seconds since it happened, measured
when sent), `takeBack` (boolean), and optionally `byDeviceId`, `byName`,
`byShortName`, `byKind`, `bySource` (`device`, or `radioPtt` when the
radio's own PTT took transmit, its names then "Radio" and its kind
`station`) naming who did it,
`slices` `[{sliceId, letter, frequencyHz, mode, band}]` (closed ones
included) and `change`. Kinds here: `sliceMoved` and `sliceClosed` (no
Take it back; `sliceClosed` also to a closed slice's listeners, above), `receiverTaken` and `sliceTaken` (Take it back),
`settingChanged` (section 7.6: `change`, who, no Take it back),
`transmitTaken` (section 18.9: who took transmit, Take it back),
`controlTaken` (`sliceAccessVersion` 1: another device took control of
the device's slice, which it still listens to; who, the slice: "<taker's
name> took control of slice <letter>. You are still listening."; with
`sliceAccessVersion` 2, Take it back, and its slice entry adds
`incarnation` and `controlRevision`, the slice's control revision after
the take, so `slices` is `[{sliceId, letter, frequencyHz, mode, band,
incarnation, controlRevision}]`; a peer below 2 is sent `takeBack` false
and the entry without `incarnation` and `controlRevision`, exactly as
before),
`graceEnded`, `slicesNotRestored`, `antennaKept` and `tuneEnded` (about
the device's own state: no `by` keys, no Take it back). `tuneEnded`
(iPhone app plan Task 77 fix round 4) tells a device that its accepted
`tx.tunerTune` ended without keying because the Power Genius did not go
to standby for it (it never reported standby within 1.5 s, or was put
back in operate during the wait): "The amplifier did not go to standby
for tuning. Put it in standby or disconnect it in Setup, then tune
again." `graceEnded`, "You were away for more than 3
minutes. Your slices are back.", goes right after `snapshot.complete` to a
device let in after its 3 minutes ran out, its `slices` listing any saved
slice that could not be restored (then its words are "You were away for
more than 3 minutes. <n> of your slices could not be restored: all the
radio's receivers are in use."); otherwise `slicesNotRestored`, "<n> of
your slices could not be restored: all the radio's receivers are in
use.", reports those. An away device's notices wait and follow its
`snapshot.complete`; after its 3 minutes they still arrive, after
`graceEnded`, with `takeBack` false. The Core keeps them until the device
returns, is removed, or the Core restarts. A session without
`sessionHolderVersion` 1 is sent neither kind.

While a device is on the air, the rules above that would move or close
its transmit slice are refused instead (the several-devices design, ruling
7.4): that refusal arrives with the transmit holder (a later task).

### 7.6 Settings that affect every device

iPhone app plan Task 75 (the several-devices design, sections 7.1 to 7.4,
rulings 5.11a, 6.1 and 7.1 to 7.8). Some settings belong to the radio,
not to one slice, so a change to one reaches every slice that listens
through what it touches, whoever owns it. The changes are in two tiers
(the several-devices design, ruling 7.1a, the operator's ruling of
2026-09-28), the last column below:

- **Asks first.** A change that can take another device's reception away,
  or reaches the transmitter, and would disturb another **connected**
  device's slices (or, once transmit has a holder, the holder, when it
  touches the transmitter) is held and asked, as section 7.5 describes,
  with `kind` `sharedSetting`. Only connected devices are asked about:
  `affected` never names an away device, and a change that would disturb
  only away devices applies at once.
- **Applies at once and tells.** A small adjustment applies at once, as
  the requester's own change would, and each device it disturbs is told
  (the notice below). Nobody is asked, and a window without
  `sessionHolderVersion` 1 makes it too.

Either way every disturbed device is told once the change has applied,
an away device when it returns (section 7.5). One that disturbs nobody,
or sets the value already there, applies at once as before and tells
nobody. The requester's own slices never count, nor do slices nobody owns.
A change that touches rows of both tiers at once asks.

| Change | Arrives as | What it reaches | Tier |
| --- | --- | --- | --- |
| Sample rate | `requestSliceSampleRate` | Protocol 1: every receiver (the radio's data flow stops); Protocol 2: that receiver. Each other device's slice the narrower window leaves out moves to another receiver or, with none free, closes (the Core's own plan); it closes only once the change is certain, so a change refused after Confirm closes nothing and tells nobody | Asks first |
| Attenuator, preamp, automatic attenuator | `stepAtt` writes (`attenuationDb`, `enabled`, `preampMode`, `autoAtt...`) | ADC0's receivers; `attenuationDb` the receivers of the ADC slice A is on | Applies at once and tells |
| The other ADC's attenuator | `stepAtt` `rx2AttenuationDb`, `rx2StepAttEnabled`, `rx2AutoAtt...` | the receivers of the other ADC in use (both ADCs' while diversity links them) | Applies at once and tells |
| ADC1 preamp | `stepAtt` `rx1Preamp` | ADC1's receivers | Applies at once and tells |
| Receive antenna | a slice's `rxAntenna`; `alexAntennas` `rxAntennas`, `rxOnlyAntennas`, `useTxAntennaForRx`; `setAlexRxAntenna` | every receiver on a 1-ADC board; on a 2-ADC board ADC0's (ANT1 to ANT3) and, for a receive-only input, ADC1's; a slice's own write also its receiver's other slices | Asks first |
| Receive filter policy | `setAlexBpfMode` | the receivers on that filter chain | Applies at once and tells |
| PureSignal | `pureSignalSettings` writes, `transmit` `pureSig`, `ps3.off`, `ps3.single`, `ps3.automatic`, `ps3.applyCurrent`, `ps3.restoreCorrection` | on a 1-ADC board every receiver, `pausesWhileTransmitting`; the transmitter | Asks first |
| Diversity | a slice's `diversityEnabled`, `diversityPhaseDeg`, `diversityGainDb`, `diversityFineNullEnabled` | receiver 0 on a 2-ADC board, every receiver on a 1-ADC board | Asks first |
| A shared receiver's noise blanker | a slice's `nbMode`, `nb1Threshold`, `nb1TransitionMs`, `nb1LeadMs`, `nb1LagMs`, `nb2Mode` | that receiver's slices | Applies at once and tells |
| Notches | `notch.add`, `notch.move`, `notch.setActive`, `notch.delete`; `notches` `globalEnabled`, `autoIncrease` | every slice whose passband overlaps the notch (the notches, for the two switches) | Applies at once and tells |
| Receive options | `settings.write` of the receive `DspOptions...Rx` keys (buffer size, filter size, filter type, per mode group) | every receiver | Applies at once and tells |
| Transmit antenna | a slice's `txAntenna` | the transmitter | Asks first |
| The amplifier, interlock, power limit | `amplifier` `operate`; `configurePgxl`, `disconnectPgxl`, `setPgxlConnectionSettings`, `setPgxlName`, `setPgxlHardware`, `setPgxlNetwork`, `savePgxlSettings`, `setTxInterlockPolicy`, `setPgxlPowerCap`, `configureRfKit`, `disconnectRfKit`, `setRfKitEnabled`; `settings.write` of `PGXL_...` | the transmitter | Asks first |
| 4O3A on or off | `setFourO3AEnabled` | as the tuner: ADC0's receivers on a 2-ADC board, every receiver on a 1-ADC board; the transmitter | Asks first |
| The tuner, the RF-Kit amplifier's antenna | `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass`, `configureTgxl`, `disconnectTgxl`, `setTgxlName`, `setTgxlNetwork`, `saveTgxlSettings`; `settings.write` of `TGXL_...` and `RfKit_...` | ADC0's receivers on a 2-ADC board, every receiver on a 1-ADC board; the transmitter | Asks first |
| The radio | `station.selectRadio` (parity Task 21) | every receiver and the transmitter; `change` "Radio", the Core's radio's name, the chosen radio's name | Asks first |

A verb naming another device's slice is refused first, as section 7.3
says. The words of `change` are the Core's: "Attenuator, ADC 1", "0 dB",
"20 dB"; "Sample rate, Receiver 1", "192 kHz", "96 kHz"; "Noise blanker,
Receiver 1", "Off", "NB"; "Diversity phase", "0 degrees", "45 degrees";
"Diversity gain", "0 dB", "6 dB"; "Diversity fine null", "Off", "On"; "4O3A
amplifier and tuner", "Off", "On". A `settings.write` held this way is answered by
`settings.reject` with "Waiting for you to confirm." and the Core's value
(section 8.1), and its `confirm.request` carries `forSettingsKey`.

A change that applies at once is answered as it always was (its
`property.result`, `settings.value` or `command.result`), and its notices
follow once it has applied: a sample rate's when its result arrives, the
radio's when the Core answers it.

On proceed the change applies as the original request would have, the
answer carrying the readback (section 7.5). Whether asked or applied at
once, each disturbed device is sent a `notice` of kind `settingChanged`: who, `change`, `secondsAgo`,
the slices of its it reached (`slices`), `takeBack` false, and a
`reason` such as "iPhone changed Attenuator, ADC 1 from 0 dB to 20 dB."
followed, where a slice moved, closed or pauses, by "Your slice B moved
to another receiver.", "Your slice B closed: no receiver was free." or
"Your slice B pauses while the radio transmits.". A new write from the
requester to the same thing cancels its open question. A window without
`sessionHolderVersion` 1 is never asked: its change on a row that asks
first is refused "This change would affect <names>. Update NereusSDR to
confirm changes that affect other devices.", naming the connected devices
it would disturb.

**The receive antenna stays put.** Band tracking re-applies a band's
receive antenna when a slice crosses into it. While another device has a
slice on a receiver fed by the ADC that antenna relay feeds (every
receiver on a 1-ADC board, ADC0's on a 2-ADC board), the antenna stays
where it is instead: the tuning goes ahead with no question, and the
device tuning is sent a `notice` of kind `antennaKept`, "The antenna stays
on ANT1 while <other device's short name> listens on it.", with the
slice in `slices` and no `by` keys. It stays when a transmission ends
too; the band's transmit antenna still applies at key-down. Once the
other device's slices have left that ADC, the next band crossing switches
as always; nothing switches on its own when they leave.

**Transmit.** A change that touches the transmitter disturbs the
transmit holder (listed with `holdsTransmit` true), and while the holder
is on the air a change to the transmit path, a Protocol 1 sample rate, or
a change that would move or close the holder's transmit slice is refused
with "<holder's short name> is on the air. Try again when they stop."
(ruling 7.4). Both arrive with the transmit holder (a later task); until
then nobody holds transmit, so a change that touches only the
transmitter disturbs nobody and applies at once, and `state` is never
`transmitting`.

### 7.4 The catalogue

The `catalog` object's `json` is one JSON object (RFC 8259, UTF-8,
compact) holding the values the Core owns and an app shows: the modes, the
Core's filter presets, the tune steps, the AGC, receive and gauge ranges,
Setup > Display's controls, the noise-reduction quick controls, the radio's
capabilities and transmit ranges, the band buttons, the band plans, the waterfall
palettes, the slice colours and the Core's tools (`StationCatalog`, spec section 4.10). An app
draws its controls from it and carries no table of its own, so a Hermes
Lite 2 and an ANAN-G2 each get their own. It is the same for every device
connected to the Core; nothing in it is per device.

**When it changes.** The Core keeps it current: it builds it at start, reads
it again as a session's snapshot is first sent, and rebuilds it, at most
once per turn of its event loop, when a filter preset or the CW pitch
changes in its settings (from its own computer or a window, section 8),
when its band plan data is read again, when its own band plan changes (its
View > Band Plan, or a device's `settings.write` or `settings.remove` of
`BandPlanName`, section 8.1), and when its radio changes (a radio
found after the session began included). A rebuild
that changes nothing leaves `revision` alone; one that changes anything
moves it by one, so the three settings of one preset move it once. The new
value reaches a connected client as a `delta` (section 7.2).

**Size.** At most 256 KiB of `json` for the largest radio
(`StationCatalog::kMaxJsonBytes`); today's are about 71 KiB, most of it the
band plans and their spots.

**Units and forms.** A key names its unit (`Hz`, `Db`, `Dbm`, `W`);
numbers are JSON numbers and a whole value is written without a fraction.
Colours are `#RRGGBB`, upper case. Labels are the desktop's own words,
shown as sent. A key an app does not know is ignored; an app given an
empty `json` (the stand-in of section 16.3) has no catalogue yet.

The object has exactly these sixteen keys (a Core from before
`noiseReduction` sends fifteen, and one from before `display` fourteen; an
app detects each by its presence, as it does `board`'s `transmit`,
`rx1Preamp` and `relays`):

| Key | Holds |
| --- | --- |
| `modes` | `[{id, label, sideband}]`: the 14 modes, `id` the slice's `dspMode` value 0 to 13, `label` its name (`LSB`, `USB`, ..., `RADE-U`, `RADE-L`), `sideband` `lower`, `upper` or `both` |
| `filterPresets` | `{<mode label>: [{slot, label, lowHz, highHz}]}`: each mode's presets from the Core's store, slot 0 first (`F1`), edges signed as a slice's `filterLow` and `filterHigh`; a mode has 1 to 10 |
| `tuneSteps` | `[{hz, label}]`: the step list, smallest first, as the slice's `stepHz` takes it; `label` like `500 Hz`, `1 kHz`, `2.5 kHz`, `1 MHz` |
| `agc` | `{modes: [{id, label}], thresholdDb: {min, max, step}}`: the AGC modes an operator picks (`id` the slice's `agcMode`: `Off`, `Long`, `Slow`, `Med`, `Fast`), and AGC-T's range for `agcThreshold`. The Modes tab's AGC section shows no other range; a later one arrives as another `{min, max, step}` key named after the setting it bounds |
| `receive` | `{afGain, ssqlThresh, amsqThresh, fmsqThresh}`, each `{min, max, step}` for the slice setting of that name, as the desktop's own control holds it: `afGain` 0 to 100 in the AF slider's units, `ssqlThresh` 0 to 100 in the SQL slider's units, `amsqThresh` and `fmsqThresh` -160 to 0 dB; every step 1 |
| `meters` | The gauges an app draws (below) |
| `display` | Setup > Display's FFT, rendering and waterfall controls (below) |
| `noiseReduction` | The VFO flag's noise-reduction quick controls, slot by slot (below) |
| `board` | The radio (below) |
| `bands` | `[{id, label}]`: the desktop's per-pan BAND grid, in its order (160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6, 2, WWV); `id` is the band as `slice.selectBand` takes it (0 for 160 m to 10 for 6 m, 27 for 2 m, 12 for WWV) and `label` is the button's text; 2 m follows 6 m (R-IOS-26), and a peer without `band2mVersion` 1 is sent the grid without it (section 6.1). The desktop draws its grid from the same table, so the two cannot differ |
| `bandPlans` | `[{id, name, default, active, segments: [{lowHz, highHz, label, licence, lowestClass, colour}], spots: [{hz, label}]}]`: every bundled plan, `id` its file's name (`arrl-us`), `default` true on ARRL (US) alone, `active` true on the Core's own plan alone (the plan settings `BandPlanName` names; a Core from before `active` sends none, and an app then reads `BandPlanName`); `spots` are the plan's marked frequencies, as its file lists them, `hz` the frequency in whole hertz and `label` the file's text (the desktop's strip draws each as a dot, without its label); `licence` lists the licence classes (`E,G`), empty for a beacon or no transmit; `lowestClass` is the lowest class the segment allows, as the desktop's band-plan strip names it after the label (`PHONE General`): `Tech` when `licence` holds T, else `General` when it holds G, `Extra` when it is exactly `E`, and empty otherwise |
| `palettes` | `[{id, name, stops: [{at, colour}]}]`: the waterfall palettes, `id` the desktop's palette number, `at` from 0 to 1 to three places, lowest first. The Custom palette is each computer's own and is not listed |
| `sliceColours` | `[colour]`: slice A's colour first, one for each slice the radio allows |
| `tools` | `[{id, label, where, offered}]`: the desktop's Tools menu in its order, `where` `station` (works at the Core) or `both`; MIDI Mapping and Macro Buttons are not listed |
| `radioItems` | `[{id, label, offered}]`: Manage Radios, Antenna Setup, Transverters and Protocol Info, in the desktop's Radio-menu order |
| `audio` | `{opusProfiles: [{bitrate, bandwidthHz}]}`: the Opus profiles the station has measured, in order (24000 bit/s with 8000 Hz, 48000 bit/s with 20000 Hz); a device told `audioQualityVersion` 1 asks for one as its `opusBitrate` (remote media control v1, "Per-device audio quality") |

`meters`:

| Key | Holds |
| --- | --- |
| `sMeter` | `{minDbm, s9Dbm, maxDbm, dbPerSUnit, redFromDbm, sUnits: [{label, dbm}], overS9: [{label, dbm}]}`: S0 at -127 dBm, 6 dB an S-unit, S9 at -73 dBm, then `+10` to `+60` every 10 dB up to -13 dBm, red from S9 |
| `micLevel` | `{minDb, maxDb, yellowFromDb, redFromDb}`: -40 to +10 dB, yellow from -10, red from 0 |
| `rfPower` | `{minW, maxW, ratedW, redFromW}`: red from the PA rating, full scale 20% past it |
| `swr` | `{min, max, redFrom}`: 1.0 to 3.0, red from 2.5 |

`board`:

| Key | Holds |
| --- | --- |
| `model` | The radio model, as `hpsdrModel` in capabilities |
| `productLabel` | Its name (`ANAN-G2`, `Hermes Lite 2`) |
| `maxSlices` | The slices it allows |
| `attenuator` | `{min, max, step}` in dB for the step attenuator, or `null` without one |
| `preampItems` | `[{id, label}]`: the preamp choices, `id` the `stepAtt` object's `preampMode` |
| `rxAntennas`, `txAntennas` | The main antenna ports (`ANT1` upwards) |
| `rxOnlyInputs` | The receive-only inputs by the product's own labels (`BYPS`, `EXT1`, `XVTR` on an ANAN-G2), empty without them |
| `sampleRates` | The receive rates in Hz the radio offers on the protocol it runs |
| `pureSignal` | Whether it has PureSignal |
| `paRatingW` | Its PA rating in watts |
| `micJack` | Whether it has a microphone input of its own |
| `transmit` | `{power, tunePowerForTxBand, tunePower, micGainDb}`: the transmit controls' ranges on this radio (below) |
| `rx1Preamp` | Whether it has the RX applet's RX1 preamp toggle (the dual-ADC boards; `rx1Preamp` true is refused elsewhere) |
| `relays` | `{rxOutOnTx, ext1OutOnTx, ext2OutOnTx, rxOutOverride}`: the antenna relays it has (below) |
| `rx2Attenuator` | `{min, max, step}` in dB for RX2's own input attenuator, 0 to 31 in 1 dB steps on the radios with a second ADC (ANAN-100D, 200D, OrionMKII, 7000D, 8000D, Anvelina Pro3, G2, G2 1K), or `null`. An app draws a slider and writes `stepAtt`'s `rx2AttenuationDb` with `rx2StepAttEnabled` true (`adcAttenuatorVersion` 1) |
| `rx2PreampItems` | `[{id, label}]`: RX2's preamp choices where RX2's input has two states instead (the HPSDR's second Mercury: `0dB`, `-20dB`), `id` the `stepAtt` object's `rx2PreampMode`; empty elsewhere |
| `rx2AttenuatorReason` | Why RX2 has no input control of its own, in operator words, when `rx2Attenuator` is `null` and `rx2PreampItems` is empty (RX2 shares RX1's input, or the radio's second input is not known); `null` otherwise |
| `radioMic` | Whether the operator can choose the radio's own mic input (Radio Mic) on this radio. True on every board with a mic jack, and on the Hermes Lite 2, which takes mic audio through its audio add-on board; false on the receive-only kits |
| `radioMicNote` | What the radio's mic input needs, in operator words, when it depends on hardware the radio cannot report: on the Hermes Lite 2, "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite 2 sends no mic audio."; `null` otherwise |

`board.transmit`: each key is `{min, max, step}` in the property's own
units, as the desktop's control ranges it (the TX applet's RF Power and Tune
sliders, Setup > Transmit > Power's fixed tune spinbox, the Phone/CW
applet's mic level). The three power keys add `shown: {min, max, decimals,
unit, rounding, endSnap}`: what the control shows at `min` and at `max`,
linear in between (`shown.min + (value - min) x (shown.max - shown.min) /
(max - min)`), to `decimals` places, `unit` `""`, `W` or `dB`. Before that,
a value between the steps is taken to a step as the desktop does: first
`endSnap` (`{below, above}` or `null`) makes a value below `below` the
control's `min` and one above `above` its `max`; then `rounding` takes it to
a step, `halfEven` the nearest step with a half to the even step (`min +
step x roundHalfEven((value - min) / step)`), `down` the step at or below
it (`min + step x floor((value - min) / step)`), `none` no step. On a Hermes
Lite 2 these are mi0bot's readouts: RF Power `halfEven` with `endSnap` 4
and 87 (a drive of 3 shows -7.5 dB, 87 shows -0.5 dB), Tune `halfEven`
with `endSnap` 3 and 96 (2 shows -16.5 dB, 97 shows 0.0), and the fixed
tune spinbox `down` (C#'s integer division: 5 shows -16.0 dB); elsewhere RF
Power is `halfEven` and the tune keys `none`, all without `endSnap`.
The PA profile changes none of it.

| Key | Property | ANAN-G2 | Hermes Lite 2 |
| --- | --- | --- | --- |
| `power` | `power` | 0 to 100 step 1, shown as the number | 0 to 90 step 6, shown -7.5 to 0 dB, 1 place, a drive between steps at the nearest step |
| `tunePowerForTxBand` | `tunePowerForTxBand` (verb `setTunePowerForTxBand`) | 0 to 100 step 1, shown as the number | 0 to 99 step 3, shown -16.5 to 0 dB, 1 place, a value between steps at the nearest step |
| `tunePower` | `tunePower` | 0 to 100 step 1, shown 0 to 100 W | 0 to 99 step 3, shown -16.5 to 0 dB, 1 place, a value between steps at the step below |
| `micGainDb` | `micGainDb` | -40 to +10 dB step 1 | -40 to +10 dB step 1 |

The Core accepts what the desktop accepts: `power` has no range check (the
desktop's TX applet slider holds it in range, and `powerByBandJson` spans 0
to 100 on every board); `tunePowerForTxBand` and `tunePower` outside 0 to 99
on a Hermes Lite 2 (0 to 100 elsewhere) are refused; `micGainDb` is
accepted from -50 to +70 dB on every radio, wider than the control shows.

`board.relays`: the desktop hides a control for a relay the radio lacks,
and an app does the same from these. `rxOutOnTx` is true when the radio has
RX out on TX (the RX bypass relay, the VFO flag's BYPS button: the board's
relay and the product's control, `rxOutOnTxPresent`); `ext1OutOnTx` and
`ext2OutOnTx` are the Ext-on-TX switches' labels as Setup > Antenna Control
shows them (`Ext 1 on Tx`, `Ext 2 on Tx`; a G2E's Ext 2 is `Rx BYPASS on
Tx`), `null` where the radio has none; `rxOutOverride` is whether it has
the RX out override. An ANAN-G2 has Ext 1 and Ext 2 on TX only; a Hermes
Lite 2 has none of them.

`noiseReduction`: `{nr1, nr2, nr3, nr4, dfnr, mnr, nnr}`, each a list of
the slot's quick controls in the VFO flag's order, from the one table the
popups, NNR's controls and a new slice's values read (`ControlRanges.h`),
so they cannot differ. It is the same on every radio. Each entry names the
slice property it writes (`property`, a two-way `SliceModel` property) and
the control's `label`, and by `kind`:

- `slider`: `min`, `max`, `step` in the control's own units; the property
  is `value x scale`; the readout is `value / divide` to `decimals` places
  followed by `suffix`; `default` is a new slice's value in the property's
  units; `reset` is the control value its Reset restores, or `null` where
  it has none (DFNR's and MNR's popups and NNR's "Reset tuning" have one).
- `switch`: `default` true or false.
- `choice`: `options: [{id, label}]`, `id` the property's value; `default`
  the default `id`; `reset` the `id` Reset restores, or `null`.

The Core refuses none of the NR1 to NR4, DFNR and MNR values; NNR's values
outside their range are refused.

| Slot | Controls |
| --- | --- |
| `nr1` | Taps 1 to 1024 (64), Delay 1 to 1023 (16), Gain 1 to 1000 x 1e-6 (100), Leak 1 to 1000 x 1e-3 (100): Thetis's NR spinboxes and conversion; Position Pre-AGC or Post-AGC (Post-AGC) |
| `nr2` | Gain Method Linear, Log, Gamma, Trained (Gamma); NPE Method OSMS, MMSE, NSTAT (OSMS); AE Filter (on); Noise post proc (off); Factor 0 to 100 in 0.1 steps (15); Rate 0 to 100 in 0.1 steps (5) |
| `nr3` | Position (Post-AGC); Use fixed gain for input samples (on) |
| `nr4` | Reduction 0 to 20 dB (10), Smoothing 0 to 100% (0), Whitening 0 to 100% (0), Rescale 0 to 12 dB (2), SNRthresh -10 to +10 dB (-10); all numeric increments 1, shown with one decimal; Algo 1, 2 or 3 (Algo 1) |
| `dfnr` | Attenuation Limit 0 to 100 dB (100, Reset 100), Post-Filter Beta 0 to 100 x 0.01 shown to 2 places (0, Reset 0) |
| `mnr` | Strength 0 to 200 x 0.01 shown as % (1.0, Reset 100), Aggressiveness 1 to 1000 (4, Reset 4), Floor 0 to 2000 x 0.001 shown with `m` (0.05, Reset 50), Alpha 0 to 100 x 0.01 (0.92, Reset 92), Bias 0 to 100 x 0.1 (1.2, Reset 12), Gsmooth 0 to 100 x 0.01 (0.70, Reset 70) |
| `nnr` | Model Standard or Premium (Standard, kept by Reset); Suppression -50 to -10 dB step 0.01 (-25); Position (Post-AGC); Alpha 0 to 4 step 0.01 (1); Alpha knee 0 to 40 dB step 0.1 (10); Noise time 0.05 to 30 s step 0.05 (2); Maximum gain 0 to 24 dB step 0.1 (12); Attack and Release 0 to 500 ms step 0.1 (0); each Reset to its default |

`display`: `{controls, binWidth, fftPlan}`, the desktop's Spectrum Defaults
and Waterfall Defaults controls from the one table the desktop page reads
(`ControlRanges.h`), so the two cannot differ. It is the same on every radio.

`controls` is a list, in the pages' order, of `{settingsKey, scope,
subscribe, page, group, label, kind, default}` and, by `kind`, either
`options: [{value, label}]` or `min`, `max`, `step`, `unit`, `decimals`:

- `settingsKey`: the desktop's settings key for pan 0 (`null` for
  Decimation, which the desktop keeps in its engines, not its settings).
- `scope`: `station` for the Core's own settings (`DisplayFftSize`,
  `DisplayFftWindow`, `DisplayHzPerBinTarget`, `DisplaySpectrumFps`), which
  a device reads and writes with `settings.write` (section 8); `device` for
  each device's own value, which it keeps per pan and sends in its spectrum
  subscription.
- `subscribe`: the spectrum subscription field the value reaches (remote
  media control v1): `fftSize` (by the rule below), `windowType`, `fps`,
  `trace.detector`, `trace.averageMode`, `waterfall.detector`,
  `waterfall.averageMode`, `averageTimeMs` and `waterfallAverageTimeMs`
  (display extras v1, `displayExtrasVersion`), and `decimation`
  (`spectrumGrantVersion` 2).
- `page` and `group`: the desktop page and group box it sits in
  (`Spectrum Defaults` / `Fast Fourier Transform` or `Rendering`;
  `Waterfall Defaults` / `Display`); `label` is the control's own text.
- `kind`: `choice` (a list; `value` is what is stored and sent), or
  `slider` (a range). A slider with `options` steps through them in order:
  the FFT Size slider's seven sizes, 4096 to 262144, each a power of two.
- `default`: the value the desktop applies when nothing is stored.
- The Hz/bin Target adds `offValue` 0 and `offLabel` `Off`: the control
  shows Off at 0.

| Control | Key | Scope | Kind and range | Default |
| --- | --- | --- | --- | --- |
| Size | `DisplayFftSize` | station | slider over 4096, 8192, ..., 262144 | 4096 |
| Window | `DisplayFftWindow` | station | Rectangular, Blackman-Harris 4T, Hann, Flat-Top, Hamming, Kaiser, Blackman-Harris 7T (0 to 6) | 1 |
| Hz/bin Target | `DisplayHzPerBinTarget` | station | 0 to 200 step 0.5, 2 places, `Hz/bin`, 0 is Off | 0 |
| FPS | `DisplaySpectrumFps` | station | 10 to 60 step 1, `fps` | 30 |
| Spectrum Detector | `DisplaySpectrumDetector` | device | Peak, Rosenfell, Average, Sample, RMS (0 to 4) | 0 |
| Spectrum Averaging | `DisplaySpectrumAveraging` | device | None, Recursive, Time Window, Log Recursive (0 to 3) | 3 |
| Spectrum Avg Time | `DisplaySpectrumAverageTimeMs` | device | 10 to 9999 step 10, `ms` | 30 |
| Decimation | none | device | 1 to 16 step 1 | 1 |
| WF Detector | `DisplayWaterfallDetector` | device | Peak, Rosenfell, Average, Sample (0 to 3) | 0 |
| WF Averaging | `DisplayWaterfallAveraging` | device | None, Recursive, Time Window, Log Recursive (0 to 3) | 0 |
| WF Avg Time | `DisplayWaterfallAverageTimeMs` | device | 10 to 9999 step 10, `ms` | 120 |

`binWidth` is `{label, decimals}`, the readout beside the FFT size: `Bin
Width (Hz)`, the pan's sample rate over its FFT size, to 3 places (the
rate is one of the board's `sampleRates`, so an ANAN-G2 and a Hermes Lite
2 read different widths for one size; the rule is the same).

`fftPlan` is `{minFftSize, maxFftSize}` (1024 and 262144), the sizes a pan
asks for, as a desktop remote window asks (its `plannedFftSize`). With
`round(x)` the smallest power of two from `minFftSize` up that is at least
`x`, and no more than `maxFftSize`:

    wanted = sampleRateHz x pixels / spanHz
    if DisplayHzPerBinTarget > 0: wanted = max(wanted, sampleRateHz / DisplayHzPerBinTarget)
    fftSize = max(round(DisplayFftSize), round(wanted))

and the subscription's `tier` is `fine` when `fftSize` is above
`round(DisplayFftSize)`, else `wide`. So the Size slider is the floor, and a
Hz/bin target holds the bin width at or below the target at any zoom
(`pixels` and `spanHz` are the subscription's own). The width the pan gets
is the sample rate over the size the Core grants (its spectrum context's
`grantedFftSize`), which may be less than asked.

`offered` is the Core's (iPhone app plan Task 25, D41). It starts from
the unbuilt features list the desktop hides by (`UnbuiltFeatureList.h`:
CWX, Memory Manager, CAT Control and Transverters are not offered until
built), then follows the radio and the Core: Spot Hub, FreeDV Reporter,
TX Equalizer, Network Diagnostics, Support Bundle, Manage Radios and
Protocol Info always; PureSignal when the radio has it (the board's
`hasPureSignal`); Diversity when it has a diversity receiver
(`hasDiversityReceiver`); TCI Server when the Core runs its own station
TCI server (a Core with `stationTciVersion` 1 or later, whether or not the
server is switched on, so an app can switch it on); VAX Audio when the
station computer publishes VAX devices (a Core the desktop hosts; a
headless Core publishes none); Antenna Setup when the radio has Alex and
at least three antenna inputs, the desktop's own rule. The catalogue's
revision moves when the radio or the station TCI server's state changes.
An app shows only offered items, in their place. The two catalogue
fixtures (section 16.3) hold an ANAN-G2's and a Hermes Lite 2's catalogue
in full.

### 7.7 Record streams

Records that come and go (spots, console lines) travel as record streams,
not as properties (parity Task 19; the iPhone app plan's Task 21; remote
design section 6.1a). A stream is a list of records, each an `id` (a
string, unique in its stream) with `fields` (a JSON object), newest last,
bounded by the stream's capacity. With `recordStreamVersion` 1 or later
the Core keeps:

| Stream | Capacity | Record |
| --- | --- | --- |
| `spots` | 500 | One spot the Core holds (its SpotModel: the station sources' spots, and FreeDV Reporter's once the Core runs it), `id` its index: `timeUtc` (string, ISO 8601 UTC), `frequencyHz` (number, whole Hz), `call`, `mode`, `source` (the source's label: `Cluster`, `RBN`, `POTA`, `PSK`, `FreeDV`), `spotter`, `comment` (strings), `band` (number, the Band as the catalogue's `bands` numbers it: 11 for GEN, 27 for 2 m; a peer without `band2mVersion` 1 reads 11 for 2 m, section 6.1), `dxccColour` (string, `#rrggbb`, empty when the Core does not colour it) and `dxccPriority` (number: 4 a new DXCC entity, 3 a new band, 2 a new mode, 1 worked before, 0 not known or colouring off); with `recordStreamVersion` 2, `resolvedMode` (number, the slice's `dspMode` value 0 to 13 a click on the spot selects, as the desktop resolves it: `CWU` 4 or `CWL` 3 for CW by the 10 MHz rule, `USB` 1, `LSB` 0, `DIGU` 7, `DIGL` 9, `AM` 6, `SAM` 10, `FM` 5 (NFM too), `RADE_U` 12 or `RADE_L` 13 for a FreeDV spot; absent when the resolver has none: the spot or its comment names a mode it does not map, or names none and the spot is below 1.8 MHz or in a band's digital segment, whose inferred `DIGU` the resolver's table does not map) |
| `spotConsole:<source>` | 200 | One console line of a station source (`dxCluster`, `rbn`, `pota`, `pskReporter`, and with `stationFreedvVersion` 1 `freedvReporter`), `id` a rising number: `line` (string). A command typed from any device shows as `> <command>` |
| `freedvStations` | 1000 | With `stationFreedvVersion` 1: one station FreeDV Reporter lists, as the Core hears it, `id` its FreeDV Reporter session id: the FreeDV Reporter dialog's 14 columns, `callsign`, `gridSquare` (strings), `distanceKm` and `headingDeg` (numbers, from the Core's own grid square; 0 with `headingCardinal` empty while either grid square is not known), `headingCardinal` (string, `N` to `NNW`), `version` (string), `frequencyHz` (number, whole Hz, 0 not known), `txMode` (string), `status` (string: `Active`, `TX` or `RX Only`), `userMessage` (string), `lastTxUtc` (string, ISO 8601 UTC, empty when never), `lastRxCallsign`, `lastRxMode` (strings), `snrDb` (number, -99 not known) and `lastUpdateUtc` (string, ISO 8601 UTC, empty when not known); then `transmitting` (boolean), `receivingFrom` (string: whom its latest receive report heard, the last callsign it named, while that report stands; empty once a frequency change clears it), `messageChangedAtMs` (number, the Core's clock in ms since the epoch when `userMessage` last changed, 0 never) and `lastRxUtc` (string, ISO 8601 UTC, when its latest receive report came, empty when none stands); with `stationFreedvVersion` 2, `band` (number, the Band as the `spots` record numbers it: 0 160 m, 1 80 m, 2 60 m, 3 40 m, 4 30 m, 5 20 m, 6 17 m, 7 15 m, 8 12 m, 9 10 m, 10 6 m, 11 GEN for a frequency outside those bands, 12 WWV within 5 kHz of 2.5, 5, 10, 15, 20 or 25 MHz, 27 2 m (144 to 148 MHz; a peer without `band2mVersion` 1 reads 11, section 6.1); each band's edges belong to it; absent while `frequencyHz` is 0). The list starts again (a reset) each time the Core's connection to FreeDV Reporter connects or ends |
| `coreLog` | 200 | With `supportBundleVersion` 1: one line of the Core's log as its log file has it (`[HH:mm:ss.zzz] INF: text`, addresses already shortened), `id` its number in the Core's log (rising): `line` (string). Keys, tokens and pairing codes are removed as the support bundle removes them. The Core reads its log every 250 ms while a peer follows the stream, and only then; its first backlog is the newest lines at the first subscribe |
| `txCfcCompression` | 1 | With `txReadingsVersion` 1: the CFC display, one record, `id` `"0"`, replaced each time the Core reads new data: `atMs` (number, when the Core read it, in milliseconds on its own monotonic clock) and `binsDbTenths` (string: the 1025 values of the CFC compression display, each rounded to a tenth of a dB, as little-endian int16 tenths, in base64). Bin `i` is `i * 48000 / 1024` Hz; a chart draws the bins over its own frequency range as the local CFC dialog does (Thetis's frmCFCConfig `timerTick`: `binsPerHz` = 1025 / 48000). The Core reads the display every 50 ms, Thetis's interval, only while at least one peer subscribes and its radio is on the air with CFC on, and sends a record only when WDSP says new data is ready |
| `stationRadios` | 64 | With `stationRadiosVersion` 1: one radio the Core can see, the Core's radio first, `id` its MAC in upper case: `id` and `mac` (strings, the same), `name` (string, as the radio reports itself), `model` (number, the `hpsdrModel` the Core runs it as: its saved override, else its board's), `address` (string, its IP address, empty when not known), `protocol` (number, 1 or 2) and `inUse` (boolean, true for the Core's radio). With `radioModelsVersion` 1, and only for a peer that declared `radioModels` 1, also `modelLabel` (string, the name Setup shows for `model`, `displayName` in `HpsdrModel.h`: for example "ANAN-G2 1K") and `models` (array, every model this radio's board can run as, in the desktop model choice's order, each `{"model": number, "label": string}`; the list `station.setRadioModel` accepts, `compatibleModels` in `HardwareProfile.cpp`, Thetis's board check; a board that presents as one model lists one). The list is what the Core's last scan found, with the Core's radio; a radio stays listed after it drops off until a scan misses it |
| `vaxLevels` | 1 | With `vaxVersion` 1: the VAX meters of the computer the Core runs on, one record, `id` `"0"`: `ch1Level` to `ch4Level` and `txLevel` (numbers, 0 to 1, each rounded to a thousandth, as the applet's meters read them: `AudioEngine::vaxRxLevel` and `vaxTxLevel`) and `atMs` (number, the Core's clock in milliseconds when it read them). The Core reads the meters 5 times a second, only while at least one peer subscribes (a device with its VAX tool open), and sends a record only when a meter moved |
| `txAmModulation` | 1 | With `txModMonitorVersion` 1: the AM Mod Monitor's readings of the transmit I/Q the Core sends its radio (its TX tap, `AmModulationAnalyzer`), one record, `id` `0`, present only while the radio is keyed in AM, SAM or DSB (the transmit slice's mode) and a peer subscribes: `atMs` (number, the Core's clock in ms since the epoch when it read them), `posPeakPct` and `negPeakPct` (numbers, the positive and negative peak modulation in percent since the Core's previous record: the largest of the reads it merged), `posHoldPct` and `negHoldPct` (numbers, the peaks held 1.5 s, then falling as the Core's analyzer lets them fall), `carrierLevel` (number, the carrier in linear envelope units, 0 to 1 at the radio's full scale), `carrierDbfs` (number, that in dB, -120 with no carrier), `carrierPresent`, `carrierLow` and `carrierHigh` (booleans: a carrier is measured, below 0.05, above 0.98), `scopeRateHz` (number, the rate of the scope's points) and `scopePctTenths` (string: the envelope trace, oldest first, each point's percent modulation in tenths as a little-endian int16, in base64; at most 512 points, a longer trace reduced by keeping the largest-magnitude point of each group). Asymmetry is `posHoldPct` minus `negHoldPct` |
| `txAmModulationFeedback` | 1 | With `txModMonitorVersion` 1: the same record for the PureSignal feedback receiver (the PA's output as the radio samples it), on the receiver `ModMon/FbStream` names (section 8.1); present under the same rule, and measured only while PureSignal's feedback runs on the Core's radio |

A peer asks with `records.subscribe {stream, backlog}` (section 9.1); the
Core answers, then sends at once a `record.batch` with `reset` true
carrying the newest `backlog` records (at most the capacity), oldest
first. After that it sends only what changes, to that peer only: upserts
(a new record, or a changed one) and removes, merged by `id` between sends
and sent at most every 50 ms (`deltaFlushMs`). A record removed while a
change to it waits is always sent as a remove (the peer may hold it); a
peer ignores a remove for an `id` it does not hold. `records.unsubscribe
{stream}` stops them. A subscription ends with its connection.

```json
{"type": "record.batch", "stream": "spots", "generation": 3, "reset": false,
 "upserts": [{"id": "12", "fields": {"call": "JA1ABC", "frequencyHz": 14025000}}],
 "removes": ["9"]}
```

`generation` starts at 1 and rises when the Core clears the stream (Clear
All Spots, or `spots.clearAll`). A batch with `reset` true replaces the
peer's copy with its upserts; `removes` is then empty. **Bounded for a
slow peer:** what waits for one peer never grows past the stream's
capacity; when it would, the waiting changes are dropped and the peer is
sent a reset instead, carrying the newest records up to its backlog, so
the oldest records are the ones lost, never queued without limit
(`RecordStream`).

**The AM Mod Monitor's streams** (`txModMonitorVersion` 1). The Core
spends nothing on them until a peer subscribes: with no subscriber it
reads neither analyzer and feeds neither (its TX tap is off the transmit
channel and the feedback fork is off). While a peer subscribes to a
stream, the Core reads that stream's analyzer every 33 ms (the Mod
Monitor's own refresh) while the radio is keyed in AM, SAM or DSB, and
upserts the one record; records go out in the 50 ms flush, so a record
carries the largest window peaks of the reads since the last one sent. At
the key, or when a stream is first watched during one, the Core starts
the analyzer afresh. When the key ends, the mode leaves AM, SAM and DSB,
or the last subscriber leaves, the Core stops feeding it and removes the
record: a window then shows no carrier. `txModMonitor.reset {source}`
(section 9.1; 0 `txAmModulation`, 1 `txAmModulationFeedback`) starts that
analyzer afresh for everyone watching, as a local window's RESET does its
own. A window watches one stream (its Mod Monitor's TX I/Q or PA FB
choice), backlog 1, only while its Mod Monitor is shown, and again after
each snapshot; it puts each record through the local Mod Monitor's own
display (bars, holds, flashers, lamps), so it shows what a local window
shows for the same I/Q, and drops the readings when the session ends.

A window subscribes to `spots` (backlog 500) and to each station source's
console (backlog 200) once its snapshot is complete, again on every
reconnect; with `stationFreedvVersion` 1 also to `freedvStations`
(backlog 1000), which replaces its FreeDV Reporter list (the dialog shows
the Core's stations with the Core's distance and heading), and to
`spotConsole:freedvReporter`. With `txReadingsVersion` 1 a window
subscribes to `txCfcCompression` (backlog 1) while its CFC dialog is shown
(again on a reconnect while it stays shown) and unsubscribes when the
dialog closes, which stops the Core's reads when no other peer wants them.
It shows the Core's spots in its panadapters and its Spot List
beside its own WSJT-X and SpotCollector spots, in the colours and
lifetimes of the Core's Spot Hub settings, and drops them when the session
ends. A reset of a console stream (each subscribe's backlog) replaces that
console in the Spot Hub rather than adding to it, so a reconnect never
repeats the backlog; a `spots` reset replaces only the Core's spots.

A window subscribes to `coreLog` (backlog 200) only while its Support
dialog or its Setup > Diagnostics > Logs page is showing, again on every
reconnect while one is, and leaves it when the last one closes: the
Core's debug categories can write thousands of lines a second. Refresh
subscribes again, which reads the backlog again as a reset.

## 8. The settings proxy

Settings are a flat space of string keys and string values, separate from
the object mirror. Each key is either station-scoped (kept by the Core,
proxied to the client) or operator-local (kept by the client's own
computer, never sent). `classifySettingsKey` (`SettingsScope.cpp`) decides:

1. Keys the Core owns by code (below) are station-scoped, before any table.
2. A trailing `_<digits>` (a panadapter index) is ignored when matching.
3. Exact exception keys, then prefixes, then whole keys; the first match
   wins. A key that matches nothing is operator-local. No station prefix
   and no operator-local prefix start one another, so the table's order
   between the two scopes does not change a result.

<!-- surface:settingsScope -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Tier | Key or prefix | Scope |
| --- | --- | --- |
| 1. exact key (exception) | `TciLogWindowGeometry` | operatorLocal |
| 1. exact key (exception) | `TciLogWindowAutoScroll` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/ColumnWidths` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/SortColumn` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/SortAscending` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/VisibleColumns` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/ColumnFilters` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/BandFilter` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/DistanceMiles` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/DirectionAsCardinal` | operatorLocal |
| 1. exact key (exception) | `FreeDvReporter/FrequencyAsKhz` | operatorLocal |
| 2. prefix | `hardware/` | station |
| 2. prefix | `Slice` | station |
| 2. prefix | `Vfo` | station |
| 2. prefix | `PGXL_` | station |
| 2. prefix | `TGXL_` | station |
| 2. prefix | `RfKit_` | station |
| 2. prefix | `StationTci_` | station |
| 2. prefix | `DxCluster` | station |
| 2. prefix | `Rbn` | station |
| 2. prefix | `Pota` | station |
| 2. prefix | `PskReporter` | station |
| 2. prefix | `FreeDv` | station |
| 2. prefix | `User/` | station |
| 2. prefix | `Notch` | station |
| 2. prefix | `DspOptions` | station |
| 2. prefix | `Nb` | station |
| 2. prefix | `Snb` | station |
| 2. prefix | `Rade` | station |
| 2. prefix | `filters/` | station |
| 2. prefix | `DisplayGridMax_` | station |
| 2. prefix | `DisplayGridMin_` | station |
| 2. prefix | `Tci` | operatorLocal |
| 2. prefix | `radios/` | operatorLocal |
| 2. prefix | `ConnectionTargets/` | operatorLocal |
| 2. prefix | `RemoteVax/` | operatorLocal |
| 2. prefix | `SpotCollector` | operatorLocal |
| 2. prefix | `Wsjtx` | operatorLocal |
| 3. whole key | `DisplayFftSize` | station |
| 3. whole key | `DisplayFftWindow` | station |
| 3. whole key | `DisplayHzPerBinTarget` | station |
| 3. whole key | `DisplaySpectrumFps` | station |
| 3. whole key | `DisplayTxFftSize` | station |
| 3. whole key | `DisplayTxWindowType` | station |
| 3. whole key | `DisplayTxPanDetector` | station |
| 3. whole key | `DisplayTxPanAveraging` | station |
| 3. whole key | `DisplayTxPanAvTimeMs` | station |
| 3. whole key | `DisplayTxPanNormalize` | station |
| 3. whole key | `DisplayTxWfDetector` | station |
| 3. whole key | `DisplayTxWfAveraging` | station |
| 3. whole key | `DisplayTxWfAvTimeMs` | station |
| 3. whole key | `audio/DspRate` | station |
| 3. whole key | `audio/DspBlockSize` | station |
| 3. whole key | `BandPlanName` | station |
| 3. whole key | `BandPlanRegion` | station |
| 3. whole key | `ExtendedTransmit` | station |
| 3. whole key | `PreventTxOnDifferentBandToRx` | station |
| 3. whole key | `Region` | station |
| 3. whole key | `CWPitch` | station |
| 3. whole key | `Nr3ModelPath` | station |
| 3. whole key | `StationCallsign` | station |
| 3. whole key | `RX1_MeterCalOffsetDb` | station |
| 3. whole key | `RX1_DisplayCalOffsetDb` | station |
| 3. whole key | `RxMeterCalOffsetDbByRadio` | station |
| 3. whole key | `RxDisplayCalOffsetDbByRadio` | station |
| 3. whole key | `RX1_PreampOffsetsDb` | station |
| 3. whole key | `PeripheralsMigrationDone` | station |
| 3. whole key | `SwrProtectionEnabled` | station |
| 3. whole key | `SwrProtectionLimit` | station |
| 3. whole key | `SwrTuneProtectionEnabled` | station |
| 3. whole key | `TunePowerSwrIgnore` | station |
| 3. whole key | `TxInhibitMonitorEnabled` | station |
| 3. whole key | `TxInhibitMonitorReversed` | station |
| 3. whole key | `WindBackPowerSwr` | station |
| 3. whole key | `DisableHfPa` | station |
| 3. whole key | `MultimeterDelayMs` | station |
| 3. whole key | `NetworkWatchdogEnabled` | station |
| 3. whole key | `MoxTimeOutEnabled` | station |
| 3. whole key | `MoxTimeOutSeconds` | station |
| 3. whole key | `PingTimeOutEnabled` | station |
| 3. whole key | `PingTimeOutSeconds` | station |
| 3. whole key | `PingTimeOutHost` | station |
| 3. whole key | `RemoteMoxTimeOutEnabled` | station |
| 3. whole key | `RemoteMoxTimeOutSeconds` | station |
| 3. whole key | `RxOnly` | station |
| 3. whole key | `ModMon/FbStream` | station |
| 3. whole key | `ExtendedTxAllowed` | operatorLocal |

<!-- /surface -->

### 8.1 Snapshot and write-through

- `settings.snapshot` carries every station-scoped key the Core holds for
  the connected radio: the `hardware/<mac>/` keys of the connected radio,
  `hardware/oc/`, every other station-scoped key, and the Core's
  profile-seeded marker when present (`SettingsProxyServer::buildSnapshot`). Values are `utf8` entries named
  by their key. Booleans are the strings `"True"` and `"False"`.
- `settings.write` carries one key, its value and an `origin` tag. The
  origin tag is the writing client's session identifier, not a sequence
  number. The station writes the value to its own store and sends
  `settings.value` for the key with the same `origin`, so the writer can
  recognise its own echo. It sends that `settings.value` to every device's
  session, each of which holds every station-scoped key; only the writer
  finds its own `origin` in it (iPhone app plan Task 72). A change the Core makes itself goes out as
  `settings.value` with an empty origin. A removal goes out as
  `settings.value` with no property entry.
- `settings.remove` removes a station-scoped key; the station ignores (and
  logs) a remove of an operator-local key.
- A refused write or remove gets `settings.reject`: the key, the station's
  own value as the property entry when it has one, and a `reason`. The
  client puts that value back. It goes to the session that wrote, and to
  no other.

A `settings.write` that would disturb another device (the several-devices
design's section 7.1 list, section 7.6 here) is held as section 7.3 says:
`settings.reject` with the reason "Waiting for you to confirm." and the
Core's value, then a `confirm.request` with `forSettingsKey`; its proceed
carries `settingsKey` and `value` as its readback (section 7.5). A
`settings.remove` returns its key to the default, live, so it is checked
exactly as a write of the default is: held and asked the same way, with
`change.to` "Default", and its proceed carries `settingsKey` alone as its
readback, since the key is gone.

A slice's own keys, `Slice<N>/...`, are written and removed only by the
device that owns slice N (the several-devices design, ruling 5.9). From
any other device, and for an id no live slice holds, they get
`settings.reject` with the Core's value and the reason "That slice belongs
to <the owner's name>. It can be changed only there." ("the Core" when
nobody owns it).

The station refuses a write to a key outside the station scope ("Each app
keeps this setting itself; the Core does not store it."), to another radio's `hardware/<mac>/` keys ("These settings
are for a radio this Core is not connected to."), an out-of-range
`SwrProtectionLimit` ("Choose an SWR protection limit from 1.0 to 5.0."), a
`BandPlanName` that names none of its bundled plans ("This Core does not have
that band plan."), and transmit-side keys on a receive-only
station ("Transmit configuration is unavailable on this receive-only
Core."). At `transmitSettingsVersion` 1 a receive-only station takes the
DSP > Options TX keys (`DspOptions<Setting><Mode>Tx`,
`StationServer::isTransmitSettingKeyAcceptedOffAir`) while its radio is off
the air, and below version 13 refuses a write or remove of one while it is
on the air ("The radio is on the air. Try again when it stops."), handing
back its own value. At 13 it takes them on the air as a local window does,
and applies them to the TX channel once the radio is back on receive. At `transmitSettingsVersion` 5 the same holds for Setup >
Transmit > Power's SWR Protection keys (`SwrProtectionEnabled`,
`SwrProtectionLimit`, `SwrTuneProtectionEnabled`, `TunePowerSwrIgnore`,
`WindBackPowerSwr`) and External TX Inhibit keys
(`TxInhibitMonitorEnabled`, `TxInhibitMonitorReversed`), on any peer. A
value the page's own control cannot hold is refused with the Core's value
handed back: `TunePowerSwrIgnore` outside 5 to 50 ("Choose a tune power
to ignore from 5 to 50 W."), a box that is not `True` or `False` ("The
Core expected this box to be on or off."). A taken SWR Protection key, or
its removal, applies to the Core's SWR protection at once (a removal
returns the default: off, limit 2.0, tune power to ignore 35 W). A taken
External TX Inhibit key is stored on the Core, whose TX inhibit gate
follows it (the receiver and transmit gaps plan, Task 13). At
`meterReadingsVersion` 1 a taken `MultimeterDelayMs` (Setup > Display >
Multimeter > Polling delay), or its removal, sets the Core's meter pump
rate at once, clamped to 10 to 2000 ms (a removal returns the 100 ms
default); it only changes how often the Core reads its meters, so it is
taken on and off the air, as a local window changes it. At
`txModMonitorVersion` 1 a taken `ModMon/FbStream` (the AM Mod Monitor's
feedback receiver, `0` to `4`, rx1 by default), or its removal, sets the
receiver the Core's feedback analyzer listens to at once, as the local
applet's receiver box does; text that is not a number returns rx1. It
changes only what the Core measures, never the radio, so it is taken on
and off the air. The applet's other keys (`ModMon/Source`,
`ModMon/PosFlashPct`, `ModMon/NegFlashPct`, `ModMon/VintageMeters`) are
each window's own. At
`txDisplayVersion` 2 a taken Setup > Display > TX Display analyzer key
(`DisplayTxFftSize`, `DisplayTxWindowType`, `DisplayTxPanDetector`,
`DisplayTxPanAveraging`, `DisplayTxPanAvTimeMs`, `DisplayTxPanNormalize`,
`DisplayTxWfDetector`, `DisplayTxWfAveraging`, `DisplayTxWfAvTimeMs`), or
its removal, applies to the Core's TX analyzer at once through the setter
the local page calls (a removal returns the key's default: FFT size 32768,
window 4 (Hamming), detectors and averaging 0, panadapter time 30 ms,
normalize `False`, waterfall time 120 ms). It changes only the Core's
transmit display, never the radio, so it is taken on and off the air and
on a receive-only Core, as a local window changes it while keyed (Thetis
sets the analyzer from these controls with no MOX check). A value the page's
own control could not hold takes what the analyzer's setter makes of it (a
number held to its range, an FFT size to the slider position the page
shows, text that is not a number leaves the setting as it was) and the Core
writes that value back to the key, so every window shows what the analyzer
runs; the Core writes back only that key. At
`transmitSettingsVersion` 6 the same off-air rule holds for Setup > PA's
keys, `hardware/<mac>/pa/...` (the PA profiles: the profile list
`pa/profile/_names`, each profile `pa/profile/<name>`, and the active
profile `pa/profile/active`) and `hardware/<mac>/paCalibration/...` (the
PA forward-power table, `boardClass` and `calPoint1` to `calPoint10`),
from a peer at agreed minor 11. A taken key, or its removal, applies to
the Core's PA profiles or calibration at once, and a window reloads its
copies of both when those keys change. At `transmitSettingsVersion` 8 the
same off-air rule holds for Hardware Config's OC transmit pins,
`hardware/<mac>/oc/tx/<band>/pin<n>` (the HF and SWL TX matrices, and the
resets, which write them), from a peer at agreed minor 11; its OC pin
actions, `hardware/<mac>/oc/actions/pin<n>/action`, and its TX Display Cal
and Volts/Amps Calibration, `hardware/<mac>/cal/txDisplayOffset`,
`cal/paSens` and `cal/paOffset` with the Calibration tab's copies
`hardware/<mac>/paCalibration/cal/{txDisplayOffset,paSens,paOffset,paDefaultRestored,logVoltsAmps}`,
are taken on the air too, because Thetis changes them while transmitting
(its TX pin boxes alone are greyed while MOX is on, unless OC hot switching
is allowed, which NereusSDR does not build). On a station that allows remote
transmit these keys are a permitted session's (section 18), and the rule is
the change's, not the holder's: an OC transmit pin still waits while the
radio is on the air, whoever holds transmit, and the pin actions and the
calibration are still taken on the air. A taken OC key applies to the
Core's OC matrix, which the radio's codec reads for every frame, once the
radio is back on receive; a taken calibration key applies to the Core's
calibration at once. The N2ADR switch (`hardware/<mac>/hl2IoBoard/n2adrFilter`)
on a Core's HL2 applies its whole preset, transmit pins included, likewise
once the radio is on receive. User Dig Out is the `transmit` object's
`userDigOut` (version 1). The rest of the transmit-side hardware keys stay
refused: the HL2's TX buffer latency and PTT hang
(`hl2/{pttHangMs,txLatencyMs}`) and the Alex TX low-pass band edges
(`alex/lpf/...`), which both windows hide until they are applied; and
the OC hot switching and external PA keys. The Alex high-pass switches
for transmit (`alex/master/{hpfBypassOnTx,hpfBypassOnPs,disable6mLnaOnTx}`)
are taken from a peer at agreed minor 11 offered `radioHardwareVersion` 7
(parity Task 14) and applied to the Core's radio at once, because Thetis's
setters (console.cs `DisableHPFonTX`, `DisableHPFonPS`, `Disable6mLNAonTX`)
apply them with no MOX check, as it does the TX antennas; neither window
greys them on the air. Clearing HPF Bypass on PureSignal feedback asks
the desktop's IMD warning first, on the desktop and on a peer that reads the
description row's `confirm` and `confirmWhen:false` (setup description
section on the Alex filters); the Core takes the write as it comes. They follow the TX antennas' rule (section 18.4):
the device holding transmit changes them on the air, another device's change
waits ("<holder's short name> is on the air. Try again when they stop."), and
off the air another device's change is asked of the holder (the
several-devices design, table 7.1, "Transmit antenna"). From an older
peer they stay refused with the transmit reason, and a window whose Core
does not offer 7 shows them disabled with "This Core cannot change these
high-pass switches for this app. Updating the Core may help."

Setup > Display > Grid & Scales' dB Max and dB Min per band,
`DisplayGridMax_<band>` and `DisplayGridMin_<band>` (`<band>` the band's
key name, `20m`, `GEN`), are station-scoped (parity ruling C12): the
Core's per-band values win. A taken key, or its removal, reaches the
Core's panadapters at once (`RadioModel::applyPanGridSetting`); a pan on
that band takes the new range (a removal returns the band's default,
-40 and -140 dB), which reaches every window through the pan's mirrored
`dBmFloor` and `dBmCeiling`. A window's pan never applies its own per-band
values on a band crossing; the Core's pan applies its own and the window
shows them. A window re-reads a band when its key arrives, and every band
on a snapshot. The change is to the display only, so it is taken on and
off the air, as a local window changes it. The rest of the `Display`
family, `DisplayGridStep` included, stays each window's own.

### 8.2 Keys the Core owns by code

These families are not in the tables above: code, not a table, decides
them (`isModelOwnedDspSettingsKey` and its helpers, `SettingsScope.cpp`).
They change only through their objects and commands; a raw
`settings.write` or `settings.remove` of one is refused with
`settings.reject` and the station's own value.

| Family | Changed through | Refusal reason |
| --- | --- | --- |
| `dspAssets/...` | `dspAssets.*` commands | generic (below) |
| `NotchCount`, `Notch<N>Center`, `Notch<N>Width`, `Notch<N>Active`, `NotchGlobalEnabled`, `NotchAutoIncrease` | `notches` object, `notch.*` commands | "This Core keeps its own notch list. Update this app to change notches." |
| `hardware/<mac>/options/stepAtt/...`, `.../autoAtt/...`, `.../preamp/...` | `stepAtt` object | "This Core keeps its own attenuator and preamp settings. Update this app to change them." |
| `hardware/<mac>/alex/antenna/...` | `alexAntennas` object, `setAlexRxAntenna` | "This Core keeps its own antenna settings. Update this app to change them." |
| `Nr3ModelPath` | `dspAssets.selectNr3Model` | "This Core keeps its own NR3 models. Update this app to choose one." |
| `hardware/<mac>/puresignal/...` | `pureSignalSettings` object, `ps3.*` commands | generic (below) |
| `hardware/<mac>/slices/<n>/nnr/...` | slice properties, `nnr.*` commands | generic (below) |
| `StationLabel` | `station.rename` | "This Core keeps its own name. Update this app to rename it." |
| `StationKeyBackupAcknowledged` | `station.acknowledgeKeyBackup` | generic (below) |

Matching is case-insensitive. The reasons in the table are plain operator
wording. `StationLabel` is empty until the first rename, and while it is
empty the Core's label follows `StationCallsign` (`StationLabel::current`).
`StationKeyBackupAcknowledged` holds the fingerprint of the identity key
that was acknowledged, so a replaced key asks again. A remove of either is
refused with the table's reason too. The generic families give "The Core changes these settings only
through their own controls." for a write, and "Change these settings with
their own controls on this Core." for a remove.

## 9. Commands

### 9.1 Invoke and result

A client asks the station to act with `command.invoke`: a `verb`, an `id`
and `args`, a list of property entries. The station answers each with
`command.result`: the same `verb` and `id`, `accepted`, `reason` (empty on
success), `affected` (the object keys the command changed) and, for
commands that return data, `values`, a list of property entries. Every
`command.result`, including a later one (a PureSignal action's later
phases, a sample-rate change answered on a later turn), goes to the session
that sent the `command.invoke`, and to no other. Each client counts its
own ids, so two devices may use the same `id` at once: the station tells
their commands apart by the session that sent each, never by `verb` and
`id` alone. A file a device is
sending with `dspAssets.beginImport` belongs to that device's session: it is
cancelled when that session ends, and another device leaving never touches
it. For the
`nnr.*`, `ps3.*` and `dspAssets.*` families the `id` must be a whole number
from 1 to 4294967295 or the message is refused.

A PureSignal action can answer more than once: each result carries a
`phase` value of `accepted`, `pending`, `completed` or `failed`, and the
last is `completed` or `failed`. The phase travels in the result's
`values`, as a `utf8` property entry named `phase` (ordinal 0), beside any
other values the action returns: `session-verbs-ps3` shows it
(`{"kind": "utf8", "name": "phase", "ordinal": 0, "value": "accepted"}`,
then `"completed"` for `ps3.off`, and `"failed"` for `ps3.saveCorrection`).
A PureSignal request refused before the action starts (one with arguments
it does not take) answers once, with no `values` and so no phase.

Every handler requires exactly the arguments listed, and an extra or
missing argument is refused. The one exception is `setPgxlHardware`, whose
three arguments are marked optional: it takes exactly one of them per
command (`biasMode` "ClassA" or "ClassAB", `fanMode` "Auto", "Quiet" or
"Continuous", or `ledIntensity` 0 to 100), and none or more than one is
refused.

`slice.listen`, `slice.stopListening`, `slice.takeControl`,
`slice.release` and `slice.setListenLevel` (`sliceAccessVersion` 1,
section 7.5) name the slice by
`sliceId` and `incarnation`, each a whole number of 0 or more (read from
the `access:<id>` object); take and release add the `controlRevision`
the device saw, and `slice.setListenLevel` adds `level` (`f64`, 0 to 1)
and `muted` (`bool`). `slice.listen` and `slice.takeControl` return the slice's
`controlRevision` afterwards in `values` (`i64`), and each accepted verb
names `slice:<id>` and `access:<id>` in `affected`. From a peer without
the feature they are refused before they are read (section 6.3). A
refused `addSlice` or `addSliceOnPan` from a peer with the feature
carries `usableSlices` (`utf8`, a JSON array of `{sliceId, incarnation,
letter, controllerDeviceId}`) in its `values` (section 7.5).

<!-- surface:commands -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Verb | Arguments | Capability | Capability version | Minimum minor |
| --- | --- | --- | --- | --- |
| `addSlice` | `initialPanId` utf8 | none | 0 | 0 |
| `removeSlice` | `sliceId` i64 | none | 0 | 0 |
| `requestSliceSampleRate` | `sliceId` i64, `rateHz` i64 | none | 0 | 0 |
| `addSliceOnPan` | `panId` utf8 | none | 0 | 0 |
| `setActiveSliceById` | `sliceId` i64 | none | 0 | 0 |
| `slice.selectBand` | `sliceId` i64, `band` i64 | `bandSelectVersion` | 1 | 11 |
| `requestStreamCtunPinned` | `sliceId` i64, `pinned` bool | `remoteCtunVersion` | 1 | 2 |
| `requestStreamCentre` | `sliceId` i64, `centreHz` f64 | `remoteCtunVersion` | 1 | 2 |
| `configureTgxl` | `host` utf8, `port` i64 | `remoteTgxlConfigVersion` | 1 | 4 |
| `disconnectTgxl` | none | `remoteTgxlConfigVersion` | 1 | 4 |
| `setFourO3AEnabled` | `enabled` bool | `remoteFourO3AControlVersion` | 1 | 4 |
| `configurePgxl` | `host` utf8, `port` i64 | `remotePgxlControlVersion` | 2 | 11 |
| `disconnectPgxl` | none | `remotePgxlControlVersion` | 2 | 11 |
| `setPgxlConnectionSettings` | `autoReconnect` bool, `keepaliveSec` i64, `pingSec` i64 | `remotePgxlControlVersion` | 2 | 11 |
| `setPgxlName` | `name` utf8 | `remotePgxlControlVersion` | 3 | 11 |
| `setPgxlHardware` | `biasMode` utf8 (optional), `fanMode` utf8 (optional), `ledIntensity` i64 (optional) | `remotePgxlControlVersion` | 3 | 11 |
| `setPgxlNetwork` | `dhcp` bool, `address` utf8, `netmask` utf8, `gateway` utf8 | `remotePgxlControlVersion` | 3 | 11 |
| `savePgxlSettings` | none | `remotePgxlControlVersion` | 3 | 11 |
| `readPgxlSettings` | none | `remotePgxlControlVersion` | 3 | 11 |
| `setPgxlOperate` | `on` bool | `remotePgxlControlVersion` | 4 | 11 |
| `scanPgxlLan` | none | `remotePgxlControlVersion` | 4 | 11 |
| `setPgxlAddress` | `host` utf8, `port` i64 | `remotePgxlControlVersion` | 4 | 11 |
| `setTgxlName` | `name` utf8 | `remoteTgxlControlVersion` | 1 | 11 |
| `setTgxlNetwork` | `dhcp` bool, `address` utf8, `netmask` utf8, `gateway` utf8 | `remoteTgxlControlVersion` | 1 | 11 |
| `saveTgxlSettings` | none | `remoteTgxlControlVersion` | 1 | 11 |
| `readTgxlSettings` | none | `remoteTgxlControlVersion` | 1 | 11 |
| `setTgxlAntenna` | `port` i64 | `remoteTgxlControlVersion` | 2 | 11 |
| `setTgxlOperate` | `on` bool | `remoteTgxlControlVersion` | 2 | 11 |
| `setTgxlBypass` | `on` bool | `remoteTgxlControlVersion` | 2 | 11 |
| `moveTgxlRelay` | `relay` i64, `direction` i64 | `remoteTgxlControlVersion` | 4 | 11 |
| `scanTgxlLan` | none | `remoteTgxlControlVersion` | 4 | 11 |
| `setTgxlAddress` | `host` utf8, `port` i64 | `remoteTgxlControlVersion` | 4 | 11 |
| `setTunePowerForTxBand` | `watts` i64 | `transmitSettingsVersion` | 2 | 11 |
| `txProfile.select` | `name` utf8 | `transmitSettingsVersion` | 3 | 11 |
| `txProfile.save` | `name` utf8 | `transmitSettingsVersion` | 3 | 11 |
| `txProfile.delete` | `name` utf8 | `transmitSettingsVersion` | 3 | 11 |
| `rade.resetVocoder` | none | `transmitSettingsVersion` | 3 | 11 |
| `txEq.setCurve` | `curveJson` utf8 | `txEqCurveVersion` | 2 | 11 |
| `txEq.resetCurve` | none | `txEqCurveVersion` | 2 | 11 |
| `cfc.setProfile` | `profileJson` utf8, `expectedRevision` utf8 | `transmitSettingsVersion` | 15 | 11 |
| `paProfile.select` | `name` utf8 | `paProfileVersion` | 1 | 11 |
| `paProfile.new` | `name` utf8 | `paProfileVersion` | 1 | 11 |
| `paProfile.copy` | `name` utf8 | `paProfileVersion` | 1 | 11 |
| `paProfile.delete` | `name` utf8 | `paProfileVersion` | 1 | 11 |
| `paProfile.reset` | none | `paProfileVersion` | 1 | 11 |
| `paProfile.setGain` | `band` i64, `value` f64 | `paProfileVersion` | 1 | 11 |
| `paProfile.setAdjust` | `band` i64, `step` i64, `value` f64 | `paProfileVersion` | 1 | 11 |
| `paProfile.setMaxPower` | `band` i64, `value` f64 | `paProfileVersion` | 1 | 11 |
| `paProfile.setUseMax` | `band` i64, `on` bool | `paProfileVersion` | 1 | 11 |
| `configureRfKit` | `host` utf8, `port` i64 | `remoteRfKitControlVersion` | 2 | 11 |
| `disconnectRfKit` | none | `remoteRfKitControlVersion` | 2 | 11 |
| `setRfKitEnabled` | `enabled` bool | `remoteRfKitControlVersion` | 2 | 11 |
| `resetRfKitError` | none | `remoteRfKitControlVersion` | 3 | 11 |
| `setRfKitOperate` | `on` bool | `remoteRfKitControlVersion` | 4 | 11 |
| `setRfKitAntenna` | `port` i64 | `remoteRfKitControlVersion` | 4 | 11 |
| `setRfKitTciMode` | none | `remoteRfKitControlVersion` | 4 | 11 |
| `setRfKitAddress` | `host` utf8, `port` i64 | `remoteRfKitControlVersion` | 4 | 11 |
| `amp.operate` | none | `accessoryTxVersion` | 1 | 11 |
| `amp.standby` | none | `accessoryTxVersion` | 1 | 11 |
| `tuner.tune` | none | `accessoryTxVersion` | 1 | 11 |
| `tuner.operate` | `on` bool | `accessoryTxVersion` | 1 | 11 |
| `tuner.bypass` | `on` bool | `accessoryTxVersion` | 1 | 11 |
| `tuner.antenna` | `port` i64 | `accessoryTxVersion` | 1 | 11 |
| `rfkit.operate` | none | `accessoryTxVersion` | 1 | 11 |
| `rfkit.standby` | none | `accessoryTxVersion` | 1 | 11 |
| `rfkit.antenna` | `port` i64 | `accessoryTxVersion` | 1 | 11 |
| `setStationTci` | `enabled` bool, `port` i64 | `stationTciVersion` | 1 | 11 |
| `setStationTciOptions` | `emulateExpertSdr3` bool, `emulateSunSdr2Pro` bool, `cwluBecomesCw` bool, `sendInitialState` bool | `stationTciVersion` | 2 | 11 |
| `disconnectStationTciClient` | `id` utf8 | `stationTciVersion` | 2 | 11 |
| `setStationTciSettings` | `rateLimitMs` i64 (optional), `cwBecomesCwuAbove10mhz` bool (optional), `iqSwap` bool (optional), `alwaysStreamIq` bool (optional), `audioBlockSamples` i64 (optional), `txChannel` i64 (optional), `rxSensorIntervalMs` i64 (optional), `txSensorIntervalMs` i64 (optional), `forgetRx2VfoBOnDisconnect` bool (optional), `useRx1VfoaForRx2Vfoa` bool (optional), `copyRx2VfobToVfoa` bool (optional) | `stationTciSettingsVersion` | 1 | 11 |
| `setTxInterlockPolicy` | `mode` i64, `graceMs` i64, `swrGateEnabled` bool, `swrGateMax` f64 | `accessoryDataVersion` | 1 | 11 |
| `setPgxlPowerCap` | `enabled` bool, `watts` i64 | `accessoryDataVersion` | 1 | 11 |
| `clearAccessoryFaults` | `device` utf8 | `accessoryDataVersion` | 1 | 11 |
| `requestIoBoardProbe` | none | `radioHardwareVersion` | 2 | 11 |
| `setAlexRxAntenna` | `band` i64, `antenna` i64, `rxOnly` bool | `radioHardwareVersion` | 3 | 11 |
| `setAlexRxAntennaForRadio` | `mac` utf8, `band` i64, `antenna` i64, `rxOnly` bool | `radioAntennaRowsVersion` | 1 | 11 |
| `setAlexBpfMode` | `chain` i64, `mode` i64 | `radioHardwareVersion` | 4 | 11 |
| `tx.setTxSlice` | `sliceId` i64 | `remoteTxVersion` | 1 | 11 |
| `tx.setMicSource` | `source` utf8 | `radioMicVersion` | 2 | 11 |
| `tx.key` | `trigger` utf8 | `remoteTxVersion` | 1 | 11 |
| `tx.unkey` | `epoch` i64 | `remoteTxVersion` | 1 | 11 |
| `tx.tune` | `on` bool | `remoteTxVersion` | 1 | 11 |
| `tx.twoTone` | `on` bool | `remoteTxVersion` | 1 | 11 |
| `tx.twoTonePreset` | `name` utf8 | `setupDescriptionVersion` | 1 | 11 |
| `tx.keepalive` | `sequence` i64, `epoch` i64 | `remoteTxVersion` | 1 | 11 |
| `tx.take` | `holderEpoch` i64 (optional), `shownKeyed` bool (optional) | `remoteTxVersion` | 2 | 11 |
| `tx.tunerTune` | `on` bool | `remoteTxVersion` | 2 | 11 |
| `setAlexTxAntenna` | `band` i64, `antenna` i64 | `radioHardwareVersion` | 6 | 11 |
| `setAlexTxAntennaForRadio` | `mac` utf8, `band` i64, `antenna` i64 | `radioAntennaRowsVersion` | 1 | 11 |
| `requestIoBoardI2c` | `bus` i64, `address` i64, `register` i64, `write` bool, `value` i64 | `radioHardwareVersion` | 7 | 11 |
| `setIoBoardOutput` | `pin` i64, `on` bool | `radioHardwareVersion` | 7 | 11 |
| `setRadioSampleRate` | `rateHz` i64 | `radioHardwareVersion` | 9 | 11 |
| `resetLevelCalibration` | none | `radioHardwareVersion` | 12 | 11 |
| `startLevelCalibration` | `levelDbm` f64, `frequencyHz` f64, `sliceId` i64 | `radioHardwareVersion` | 12 | 11 |
| `cancelLevelCalibration` | none | `radioHardwareVersion` | 12 | 11 |
| `dsp.filterResponse` | `sliceId` i64, `highResolution` bool | `dspInfoVersion` | 1 | 11 |
| `records.subscribe` | `stream` utf8, `backlog` i64 | `recordStreamVersion` | 1 | 11 |
| `records.unsubscribe` | `stream` utf8 | `recordStreamVersion` | 1 | 11 |
| `spots.connect` | `source` utf8 | `recordStreamVersion` | 1 | 11 |
| `spots.disconnect` | `source` utf8 | `recordStreamVersion` | 1 | 11 |
| `spots.sendCommand` | `source` utf8, `text` utf8 | `recordStreamVersion` | 1 | 11 |
| `spots.clearAll` | none | `recordStreamVersion` | 1 | 11 |
| `txModMonitor.reset` | `source` i64 | `txModMonitorVersion` | 1 | 11 |
| `station.selectRadio` | `mac` utf8 | `stationRadiosVersion` | 1 | 11 |
| `station.rescanRadios` | none | `stationRadiosVersion` | 1 | 11 |
| `station.setRadioModel` | `mac` utf8, `model` i64 | `stationRadiosVersion` | 1 | 11 |
| `station.forgetRadio` | `mac` utf8 | `stationRadiosVersion` | 1 | 11 |
| `station.validateSettings` | `mac` utf8 | `settingsHygieneVersion` | 1 | 11 |
| `station.forgetSettings` | `mac` utf8 | `settingsHygieneVersion` | 1 | 11 |
| `station.repairSettings` | `mac` utf8 | `settingsHygieneVersion` | 2 | 11 |
| `freedv.setMessage` | `text` utf8 | `stationFreedvVersion` | 1 | 11 |
| `freedv.sendQsy` | `callsign` utf8, `frequencyHz` i64 | `stationFreedvVersion` | 1 | 11 |
| `freedv.setHidden` | `on` bool | `stationFreedvVersion` | 1 | 11 |
| `support.collect` | none | `supportBundleVersion` | 1 | 11 |
| `support.setLogCategories` | `categories` utf8 | `supportBundleVersion` | 1 | 11 |
| `nnr.setDiagnostics` | `sliceId` i64, `testMode` i64, `outputMode` i64 | `nnrVersion` | 1 | 5 |
| `nnr.resetTuning` | `sliceId` i64 | `nnrVersion` | 1 | 5 |
| `nnr.tryAgain` | `sliceId` i64 | `nnrVersion` | 1 | 11 |
| `nnr.applyModelSelection` | `revision` i64 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.list` | none | `dspAssetVersion` | 1 | 5 |
| `dspAssets.beginImport` | `kind` i64, `label` utf8, `size` i64, `hash` utf8, `radioIdentity` utf8 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.chunk` | `transferId` utf8, `offset` i64, `data` utf8 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.finishImport` | `transferId` utf8 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.cancelImport` | `transferId` utf8 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.export` | `id` utf8, `offset` i64 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.selectNnrModel` | `slot` i64, `id` utf8 | `dspAssetVersion` | 1 | 5 |
| `dspAssets.selectNr3Model` | `id` utf8 | `dspAssetVersion` | 2 | 5 |
| `ps3.subscribeDisplay` | `enabled` bool | `psDisplayVersion` | 1 | 1 |
| `ps3.off` | none | `psAlgorithmVersion` | 3 | 5 |
| `ps3.single` | none | `psAlgorithmVersion` | 3 | 5 |
| `ps3.automatic` | none | `psAlgorithmVersion` | 3 | 5 |
| `ps3.applyCurrent` | none | `psAlgorithmVersion` | 3 | 5 |
| `ps3.twoTone` | `enabled` bool | `psAlgorithmVersion` | 3 | 5 |
| `ps3.saveCorrection` | `label` utf8 | `psAlgorithmVersion` | 3 | 5 |
| `ps3.restoreCorrection` | `assetId` utf8 | `psAlgorithmVersion` | 3 | 5 |
| `notch.add` | `sliceId` i64, `centreHz` f64, `widthHz` f64 | `notchControlVersion` | 1 | 5 |
| `notch.move` | `id` i64, `centreHz` f64, `widthHz` f64 | `notchControlVersion` | 1 | 5 |
| `notch.setActive` | `id` i64, `active` bool | `notchControlVersion` | 1 | 5 |
| `notch.delete` | `id` i64 | `notchControlVersion` | 1 | 5 |
| `notch.addAtSlice` | `sliceId` i64 | `notchControlVersion` | 2 | 5 |
| `devices.revoke` | `id` utf8 | `deviceAdminVersion` | 1 | 11 |
| `station.rename` | `label` utf8 | `deviceAdminVersion` | 1 | 11 |
| `station.acknowledgeKeyBackup` | none | `deviceAdminVersion` | 1 | 11 |
| `station.retireToken` | none | `deviceAdminVersion` | 1 | 11 |
| `pairing.open` | none | `pairingVersion` | 1 | 11 |
| `pairing.close` | none | `pairingVersion` | 1 | 11 |
| `session.leave` | none | `sessionHolderVersion` | 1 | 11 |
| `confirm.proceed` | `id` i64, `choice` i64 | `sessionHolderVersion` | 1 | 11 |
| `confirm.cancel` | `id` i64 | `sessionHolderVersion` | 1 | 11 |
| `notice.takeBack` | `id` i64 | `sessionHolderVersion` | 1 | 11 |
| `session.pathTicket` | none | `controlSwitchVersion` | 1 | 11 |
| `slice.listen` | `sliceId` i64, `incarnation` i64 | `sliceAccessVersion` | 1 | 11 |
| `slice.stopListening` | `sliceId` i64, `incarnation` i64 | `sliceAccessVersion` | 1 | 11 |
| `slice.takeControl` | `sliceId` i64, `incarnation` i64, `controlRevision` i64 | `sliceAccessVersion` | 1 | 11 |
| `slice.release` | `sliceId` i64, `incarnation` i64, `controlRevision` i64 | `sliceAccessVersion` | 1 | 11 |
| `slice.setListenLevel` | `sliceId` i64, `incarnation` i64, `level` f64, `muted` bool | `sliceAccessVersion` | 1 | 11 |
| `station.settingsExport.begin` | none | `settingsBackupVersion` | 1 | 11 |
| `station.settingsExport.read` | `transferId` utf8, `offset` i64 | `settingsBackupVersion` | 1 | 11 |
| `station.settingsExport.cancel` | `transferId` utf8 | `settingsBackupVersion` | 1 | 11 |

<!-- /surface -->

The table's capability columns are the gate the desktop client applies
before sending (section 6.2).

These command groups need a sentence beyond the table:

- **Paired Core settings export** (`settingsBackupVersion` 1). The client
  declares `settingsBackup` 1 in its hello. Each command requires agreed
  minor 11, key sign-in as an enrolled device, completed admission and
  `snapshot.complete`; a token session cannot export. `begin` has no args
  and is refused while the station is on the air. It snapshots the Core's
  own local AppSettings XML once, without reading the remote SettingsProxy,
  then returns exactly `transferId` (32 lowercase hex UUID characters),
  `byteLength` (i64, 1 through 16 MiB) and `sha256` (64 lowercase hex
  characters). `read` takes that ID and the exact next zero-based byte
  offset, returning exactly `transferId`, `offset` and `data` (canonical
  base64 of at most 256 KiB raw XML). The final chunk retires the snapshot;
  duplicate, reordered and cross-session reads fail. `cancel` takes the ID
  and retires only that session's matching transfer; repeating it is safe.
  Argument and result field counts, names, ordinals, wire kinds, JSON types
  and encoded text are checked strictly. The client reports completed XML
  only after exact length, SHA-256 and XML structure validation. No partial
  XML is published. A Core holds at most one immutable snapshot per peer,
  four total and 64 MiB raw total; transfers end after 30 seconds without
  a valid read or 120 seconds overall, and on disconnect, session
  replacement or revocation. A congested control transport (at least
  1 MiB queued) refuses a new snapshot or chunk with a small busy result.
  The client bounds each reply by 10 seconds and the operation by 120
  seconds. This protocol exports settings only; restoration is not a
  version-1 command.

- **Slices** (iPhone app plan Task 73). With several devices on one Core,
  `addSlice` and `addSliceOnPan` make a slice the asking device owns;
  `setActiveSliceById` makes one of the asking device's own slices its
  active slice, and a slice's `active` means "its owner's active slice", so
  each device sees one active slice among its own and another device's
  choice never moves it. The station's own duties that exist once per
  radio (the FreeDV Reporter's frequency, TCI's per-slice broadcasts)
  follow the most recent choice by any device. `removeSlice`,
  `setActiveSliceById`, `nnr.*`, `notch.add`, `notch.addAtSlice`,
  `slice.selectBand`, `requestSliceSampleRate`, `requestStreamCentre` and
  `requestStreamCtunPinned` naming a slice that is not the requester's are
  refused (section 7.3).

- **A slice's band buttons.** `slice.selectBand` (`sliceId`, `band`,
  both `i64`; `bandSelectVersion` 1, agreed minor 11) does what a band
  button of the desktop's per-pan BAND grid does for that slice: the Core
  runs its own band change (`RadioModel::onBandButtonClicked`), so the
  slice gets back the frequency, mode, filter and the rest it last had on
  that band, or the band's starting frequency and mode on a first visit,
  and saves what it leaves for the band it left. `band` is an `id` from
  the catalogue's `bands` (section 7.4). A band the slice is already on
  is accepted and changes nothing, as on the desktop. `accepted` names
  the slice (`slice:<id>`) in `affected`; the change reaches it in the
  next `delta`. The refusals: "That receiver is no longer on the Core."
  (an unknown `sliceId`), "The Core has no band button for that band."
  (a `band` the catalogue does not list), the desktop's own reason for a
  locked slice ("Band 40m ignored: the slice is locked. Unlock it to
  change bands."), and "The request to change band was not understood."
  (arguments it does not take). The desktop offers every grid band on
  every radio and changes band while the radio is on the air, so the
  Core refuses neither. A peer below agreed minor 11 gets "Update this
  app to change bands on this Core.", and a Core that sends
  `bandSelectVersion` 0 answers "This Core cannot change bands for an
  app." A remote desktop window sends it too, for the slice its band
  button belongs to (a pan's BAND flyout acts on that pan's slice, the RX
  applet and a container on theirs), so its band changes use the Core's
  band memory as a band button at the Core does; to a Core at
  `bandSelectVersion` 0 it writes the slice's frequency and mode as
  before.
- **A notch at a slice.** `notch.addAtSlice` (`sliceId` `i64`;
  `notchControlVersion` 2, agreed minor 5) does what the desktop's +TNF
  button does for that slice: the Core puts a notch of 200 Hz
  (`NotchModel::kDefaultNotchWidthHz`) at the slice's demodulated
  frequency (the VFO, RIT and the DIGU/DIGL click-tune offset) moved by
  the middle of its receive filter (Thetis TNFAdd with
  notchSidebandShift), through `RadioModel::addTnfForSlice`, the one
  function the desktop's button calls too. The add is `notch.add`'s, so
  its rules and refusals are too (the notch control document): an
  unknown `sliceId` is refused with "That receiver is not on this Core",
  a second press on the same signal with "A notch already exists within
  10 Hz", and arguments it does not take with "This notch change is not
  one this Core understands.". An accepted result is `notch.add`'s:
  `affected` `["notches"]` and the values `revision` and `id`. A peer
  below agreed minor 5 gets "Update this app to change notches on this
  Core.".
- **The filter policy.** `setAlexBpfMode` sets one receive filter chain's
  filter policy (`chain` 0 or 1; `mode` 0 Auto, 1 Force filter, 2 Force
  bypass), the call the Core's own filter policy dialog makes on Apply.
  The Core saves it for its radio and publishes each chain's state on the
  `radio` object (`rxFilter0Mode` to `rxFilter1Reason`) in a `delta`. The
  policy picks the receive band-pass filter only, so a receive-only Core
  applies it too.
- **Record streams** (parity Task 19, `recordStreamVersion` 1).
  `records.subscribe` (`stream` utf8, `backlog` i64, 0 or more) and
  `records.unsubscribe` (`stream`) start and stop the Core sending a
  stream to this connection (section 7.7); subscribing again replaces the
  backlog. Unsubscribing from a stream not subscribed to is taken.
  Refusals: "The Core does not keep that list." for an unknown stream,
  "The Core could not read this request.", "This Core does not send its
  spots or console lines." without the feature, and "Update this app to
  see the Core's spots." below minor 11.
- **The Core's radio** (parity Task 21, `stationRadiosVersion` 1; This
  Core's Change radio). `station.selectRadio` (`mac` utf8) makes a radio
  the Core's: the choice is the Core's pending choice until that radio
  connects, and only then saved (a Core that restarts before then starts
  from the last radio that connected, or `radio_mac`); the Core restarts
  its run on that radio, which comes up with its own receive layout,
  per-radio settings, capabilities and catalogue, as on the Core's next
  start. An accepted change is answered on the turn the Core restarts, not
  at once: the chooser's `station.selectRadio` answer (or, after the
  confirm step, its `confirm.proceed` answer) and the other devices'
  `settingChanged` notices go out then, in that order, and every
  connection ends after them with `session.end` "The Core is switching
  to <radio name>. This app reconnects by itself.", `retryable` true, code
  `radioChanging` (section 12.4), and each app reconnects by itself; the
  Core's console commands keep answering through the restart. The change ends (another
  choice is taken again) when the radio connects, when its connect fails
  or its link is lost, when a scan does not find it, or two seconds after
  its connect starts (the radio connections' own connect bound) with none
  of those; the Core keeps trying the chosen radio. A key that arrives
  while the Core changes its radio is refused ("The Core is changing its
  radio. Try again when it has finished.", code `notReady`), and were the
  radio on the air all the same when the change runs, the change is
  dropped and nothing keyed is torn down: the chooser's answer is then
  refused with the on-air reason ("The radio is on the air. Try again when
  it stops."), no device is told the radio changed, and no session ends. It asks
  the other devices first (section 7.6), since it reaches every slice and
  the transmitter. Choosing the Core's radio again is taken and changes
  nothing. `station.rescanRadios` looks for radios again (the list
  follows as `stationRadios` records). `station.setRadioModel` (`mac`,
  `model` i64) sets the model the Core runs that radio as, one its board
  allows, saved for that radio and applied at its next connect (the
  local Connection panel's model override). `station.forgetRadio` (`mac`)
  removes a radio's saved entry, its model and a choice of it, and takes
  it off the list until a scan finds it. Each is refused while the Core's
  radio is on the air ("The radio is on the air. Try again when it
  stops."), and to a connection signed in with the pairing token rather
  than its own device key ("Change the Core's radio from a paired
  device.", as `pairing.open`; a desktop window signs in with its key
  once its first sign-in has enrolled it). Other refusals: "The Core cannot see that radio. Scan again,
  then choose it.", "That radio is in use by another program.", "The Core
  is changing its radio. Try again when it has finished." (a choice while
  the last one is still being made), "That model does not match this
  radio.", "The Core is using this radio. Choose another radio first."
  (forgetting the Core's radio, the radio it is reconnecting to or
  waiting for, or a choice still pending), "This Core does not change its radio
  from this app." and "The Core could not read this request.".
- **The Core's spot sources** (parity Task 19, `recordStreamVersion` 1).
  `spots.connect` and `spots.disconnect` (`source` utf8: `dxCluster`,
  `rbn`, `pota` or `pskReporter`) start and stop one of the Core's
  station sources from its saved Spot Hub settings (the DX cluster and
  RBN log in with `DxClusterCallsign` or `RbnCallsign`, else
  `User/Callsign`; POTA polls every `PotaPollInterval` seconds; PSK
  Reporter reports every five minutes as `PskReporter/Callsign` and
  `PskReporter/GridSquare`, else `User/*`); a window saves the tab's
  settings (section 8) before it sends them. `spots.sendCommand`
  (`source`, `text` utf8) types a line into the DX cluster's or RBN's
  console; every device's console shows it as `> <text>`.
  `spots.clearAll` clears every spot the Core holds (a `spots` reset; a
  window's copy of the Core's radios, a stream of its own, is untouched).
  None reaches the radio, so each is answered on and off the air, and
  `records.subscribe` and `records.unsubscribe` likewise: the record and
  spot verbs are exempt from the on-the-air rule (ruling M4 of the fix
  wave after parity Tasks 19 and 21; refusing a subscribe on the air would
  blank a reconnecting window's spots). None is on the several-devices
  list (section 7.6). Stopping a source that is
  not running is taken. Refusals: "The Core does not run that spot
  source.", "WSJT-X and SpotCollector listen on each computer, not on the
  Core.", "Enter your callsign in Spot Hub first.", "Enter your callsign
  and grid square in Spot Hub first.", "The DX cluster is not
  connected.", "The Reverse Beacon Network is not connected.", "Only the
  DX cluster and the Reverse Beacon Network take typed commands.", "Type a
  command first." and "The Core could not read this request.". A window
  shows a refusal on the source's tab. With `stationFreedvVersion` 1
  `source` may also be `freedvReporter`: `spots.connect` registers with
  qso.freedv.org as the Core's callsign (`FreeDvReporter/Callsign`, else
  `User/Callsign`, else `StationCallsign`; never the Core's label,
  R-IOS-26), grid square (`FreeDvReporter/GridSquare`, else
  `User/GridSquare`) and status message (`FreeDvReporter/Message`), and
  is refused "Enter your callsign and grid square in Spot Hub first."
  without a callsign and a grid square; `spots.disconnect` ends the
  connection.
- **The Core's FreeDV Reporter** (the iPhone app plan's Task 22 and
  remote-window parity Task 20, `stationFreedvVersion` 1).
  `freedv.setMessage` (`text` utf8) sends the status message listed beside
  the station (empty clears it), kept for the next connection when not
  connected now. `freedv.sendQsy` (`callsign` utf8, `frequencyHz` i64)
  asks the station listed with that callsign (compared without case; with
  two, the one heard from last) to move to that frequency; the operator's
  own radio is tuned by the device, as locally. `freedv.setHidden` (`on`
  bool) is "Hide my station": the Core saves it (`FreeDvReporter/Hidden`,
  Station scope) and the station is listed only while it is not hidden
  and the slice the Core lists is in RADE (the first slice in RADE, else
  the Core's active slice), as freedv-gui hides its reporter in analog
  mode unless the operator hid it. The saved status messages
  (`FreeDvReporter/SavedMessages`) are a Station setting (section 8).
  None reaches the radio, so each is answered on and off the air, and
  none is on the several-devices list (section 7.6). Refusals: "The Core
  does not run FreeDV Reporter.", "FreeDV Reporter is not connected on
  the Core.", "Enter a frequency for the QSY request first.", "Choose a
  station for the QSY request first.", "<callsign> is not on FreeDV
  Reporter now." and "The Core could not read this request.". A window
  shows a refusal on its FreeDV tab and, while it is open, beside the
  FreeDV Reporter dialog.
- **The Core's support bundle and logging** (remote-window parity Task 22
  and the iPhone app plan's Task 25, `supportBundleVersion` 1).
  `support.collect` (no arguments) asks for the Core's support bundle. The
  Core reads what it needs and writes the bundle on a worker thread, so the
  answer comes on a later turn, to the session that asked; one bundle is
  made at a time. An accepted result carries the value `bundle` (utf8): a
  ZIP archive in base64, at most 2 MiB before base64
  (`SupportBundle::kMaxCoreBundleBytes`), holding `system-info.json` (the
  Core's versions), `radio-info.json` (the Core's radio: model, firmware,
  protocol, the MAC and IP address shortened to their last part),
  `enabled-categories.txt`, `telemetry.json` (the newest telemetry the Core
  measured and its age in ms, or a note that it has none), `settings.xml`
  (the Core's settings with only reviewed nonsecret values), `nereusd.conf`
  (when the Core runs from one, with only reviewed nonsecret values),
  `collection-limits.json` (input caps and omissions), and
  the Core's newest logs (`nereussdr.log`, then `nereussdr-1.log` and
  `nereussdr-2.log`, each as much of its newest end as fits in the 2 MiB).
  Credential-store and private-key files are never included. Unknown and
  freeform settings values are redacted, as are unreviewed daemon settings.
  Logs remove the Core's known live token and pairing code, multiline PEM
  private keys, lines naming credentials, and recognizable key or code
  patterns. An unknown secret in otherwise innocent text cannot be inferred
  by a sanitizer. Each log contributes at most its newest 8 MiB of input;
  settings and daemon config input are each capped at 1 MiB. ZIP decoding
  also limits path names, entry count and expanded bytes.
  `support.setLogCategories` (`categories` utf8, the ids of the
  categories to turn on joined by commas; empty turns every one off) turns
  on exactly those of the Core's logging categories and every other one
  off, ignores an id the Core does not keep, saves the choice on the Core,
  and `radio`'s `logCategories` follows. Neither reaches the radio, so
  each is answered on and off the air; neither is on the several-devices
  list (section 7.6). Refusals: "The Core is already making a support
  bundle. Try again in a moment.", "The Core could not make its support
  bundle." and "The Core could not read this request.". A window shows a
  refusal in its Support dialog; a bundle it could not get is written
  with the reason in `core/unavailable.txt`.
- **The AM Mod Monitor's reset** (R-IOS-13, R-R3-49, `txModMonitorVersion`
  1). `txModMonitor.reset` (`source` i64: 0 the transmit I/Q, 1 the PA
  feedback) starts the Core's analyzer for that source afresh (its carrier
  estimate, peaks, hold and scope), the local Mod Monitor's RESET; every
  device watching that stream sees it (section 7.7). It reaches neither
  the radio nor the key, so it is answered on and off the air and is not
  on the several-devices list (section 7.6). Refusals: "The Core could not
  read this request." (a source other than 0 or 1, or a missing or wrong
  argument), "This Core does not send the modulation monitor. Updating
  the Core may help." without the feature, and "Update this app to see
  the Core's modulation monitor." below minor 11. A window subscribes and
  unsubscribes with `records.subscribe` and `records.unsubscribe` (above).
- **The filter graph's curve** (parity Task 16, `dspInfoVersion` 1).
  `dsp.filterResponse` asks the Core for the high-resolution filter graph's
  curve for the receiver of `sliceId`, computed from that receiver's
  channel as the Core's own filter graph computes it (the channel's filter
  edges and rate, WDSP's `fir_bandpass` taps, a 4096-point FFT). An
  accepted result carries the `values` `startHz` and `stepHz` (f64) and
  `magnitudesDbJson` (utf8, a JSON array): the magnitude at `startHz` +
  k * `stepHz` for each k from 0, in dB with 0 at the peak and -120 at
  the floor, rounded to 0.001 dB (today 2049 values from 0 Hz to half the
  channel's rate). A window resamples them to its graph's width as it
  would its own channel's curve. With `highResolution` false no curve is
  wanted: the result is accepted with `stepHz` 0 and an empty array. A
  window asks while Setup > DSP > Options > "High-resolution filter
  characteristics in filter graph" is on, one request at a time, and asks
  again when the slice's filter, mode or minimum notch width (which moves
  with the rate) changes. It is a read: it reaches no radio, so it is
  answered on and off the air, for any slice. Refusals: "The Core has no
  such slice.", "The Core's receiver for this slice is not running.",
  "The Core cannot work out this filter's curve.", and "The Core could not
  read this request." for arguments it does not take. A window shows no
  notice for a refusal; its graph keeps the plain passband.
- **The HL2 I/O board's I2C tool and output pins** (parity Task 14,
  `radioHardwareVersion` 7). `requestIoBoardI2c` is HL2 Options' I2C
  Control tool: one read (`write` false) or write (`write` true) on the
  Core's radio's I2C bus (`bus` 1, the HL2's daughterboard bus; `address`
  0 to 0x7F; `register` and `value` 0 to 255; `value` is ignored on a
  read). The Core runs it as its own tool does
  (`RadioModel::requestIoBoardI2c`). A read is answered when the radio
  answers, with `values` `value` (i64: the four bytes the radio returned,
  C1 << 24 | C2 << 16 | C3 << 8 | C4, so the register's own byte is
  `value` & 0xFF), or refused "The radio did not answer the I2C request."
  once 21 ms pass after the read went out unanswered (mi0bot's 20
  one-millisecond polls after the first) or the radio goes away; the
  answer comes on a later turn, named for the session that asked. A write
  is answered once it is queued, and a write to the output register (169
  at 0x1d) is read back into `ioBoard` `outputs`. `setIoBoardOutput` is
  Pin Control: output `pin` (0 to 7) `on` or off, written to the output
  register and read back into `outputs`, as mi0bot's strip click does; it
  needs the board found ("The radio's I/O board was not found."). A write
  and an output pin are refused while the radio is on the air, with "The
  radio is on the air. Try again when it stops.", because they reach the
  I/O board and the N2ADR filter board in the transmit path; a read is
  not. Other refusals: "The radio is not connected, so its I2C bus cannot
  be reached.", "Only a Hermes Lite 2 has this I2C bus.", "Only I2C bus 1
  can be reached.", "Choose an I2C address from 0x00 to 0x7F.", "Choose a
  register and a value from 0x00 to 0xFF.", "The I/O board's outputs are
  numbered 0 to 7.". A local window's tool follows the same rules.
- **The radio's sample rate** (parity ruling C4, `radioHardwareVersion`
  9). `setRadioSampleRate` (`rateHz` i64) is Setup > Hardware > Radio
  Info's sample rate from a window: the change a local window makes
  (`RadioModel::setSampleRateLiveAsync`), every receiver and the radio's
  own rate, which receivers opened later take, without a reconnect. It
  is a radio-wide change (the several-devices design, 7.1): with another
  device's slices on the Core it is held and asked (section 7.3; `change`
  "Sample rate", from and to in kHz, every slice's effect `changes`), and
  its proceed is answered when the change has finished, as a slice's rate
  change is. Like the Core's radio verbs it is for a paired device ("Change
  the radio's sample rate from a paired device." to a window signed in with
  the pairing token), and it waits while the radio is on the air, whoever
  holds transmit ("The radio is on the air. Try again when it stops."),
  checked again at the proceed, since the change stops the radio's data
  flow. It runs on a later turn and is answered when the change finishes,
  `affected` naming the slices whose rate moved (none for the rate the
  radio is at). Other refusals: "The radio is not connected, so its sample
  rate cannot change.", "This radio cannot run at that sample rate." (a
  rate outside the radio's list for its protocol), "The sample rate did
  not change. Try again.", and "The Core could not read this request." for
  arguments it does not take. The window also saves the rate on the Core
  as the radio's default for its next connect (`hardware/<mac>/radioInfo/
  sampleRate`, section 8), as before.
- **The level calibration reset** (Level Cal, `radioHardwareVersion`
  12). `resetLevelCalibration` (no arguments) is Setup > Hardware >
  Calibration's level calibration Reset from a window, after the window
  has asked "Do you want to reset Level Calibration back to defaults?"
  (Thetis setup.cs:24332-24341 [v2.10.3.15]). The Core runs Thetis's
  `ResetLevelCalibration` (console.cs:46868-46886 [v2.10.3.15]): it
  removes `RxMeterCalOffsetDbByRadio` and `RxDisplayCalOffsetDbByRadio`
  (and the one-value `RX1_MeterCalOffsetDb` and `RX1_DisplayCalOffsetDb`
  of earlier builds), so every model reads its defaults again, and each
  window is sent the removals (`settings.value` with no entry). Like Thetis it has no MOX
  check and no paired-device rule. "The Core could not read this
  request." answers arguments it does not take. The display offset feeds
  only TCI `calibration_ex`; the panadapter follows the meter offset, as
  Thetis's does (console.cs:12311 [v2.10.3.15]).
- **The level calibration run** (Level Cal, `radioHardwareVersion` 12).
  `startLevelCalibration` (`levelDbm` f64, the generator's level;
  `frequencyHz` f64, its frequency; `sliceId` i64, the slice to calibrate,
  -1 for the Core's active slice) runs Thetis's `CalibrateLevel`
  (console.cs:9856-10232 [v2.10.3.15]) on the Core. The slice stands in
  for Thetis's RX1 and VFO A: the Core saves its frequency, RIT and mode,
  the phone receive buffer, the step attenuators and the preamp setting,
  tunes to the carrier in AM with a 16384 buffer and the step attenuators
  off, searches the spectrum for the peak, averages 50 readings for each
  preamp setting the radio has, stores each setting's offset and the
  meter and display offsets, and puts everything back. It is taken only
  from a paired device and only while the radio is off the air ("Calibrate
  the receive level from a paired device.", or the Core's on-the-air
  reason). The run's own refusals come back in the result: "Turn the
  radio on before calibrating the receive level.", "Stop transmitting
  before calibrating the receive level.", "Level calibration is already
  running.", "Open a slice before calibrating the receive level." and
  "The slice to calibrate is not open."; a run that stops early sets
  `levelCalMessage`. `cancelLevelCalibration` (no arguments) stops a run
  and puts the receiver back; it is taken from any window and is accepted
  when nothing runs. "The Core could not read this request." answers
  arguments either verb does not take.
- **The amp's and tuner's own settings.** `setPgxlName`,
  `setPgxlHardware`, `setPgxlNetwork`, `savePgxlSettings` and
  `readPgxlSettings`, and `setTgxlName`, `setTgxlNetwork`,
  `saveTgxlSettings` and `readTgxlSettings`, change or read the Power
  Genius's and the Tuner Genius's own settings, the ones their Advanced
  pages show. `accepted` means the Core sent the request to the device; the
  device's answer arrives later on the read-only `accessorySettings`
  object (`pgxlAnswer`, `pgxlAnswerAccepted` and `pgxlAnswerCount`, and
  the `tgxl` trio), with the settings the device last reported. A save
  restarts the device. None of them keys a transmitter.
- **The tuner's antenna, operate and bypass.** `setTgxlAntenna` (`port`
  1, 2 or 3), `setTgxlOperate` and `setTgxlBypass` (`on`) switch the
  Core's Tuner Genius through its own tuner model, sending the tuner the
  line a local window's Tuner Genius applet sends. `accepted` means the
  command left for the tuner; the tuner's report arrives on the `tuner`
  object (`antennaA`, `isOperate`, `isBypass`). They key nothing, so a
  receive-only Core takes them, but each is refused while the radio is on
  the air (MOX, TUNE or two-tone, or the hand-back to receive after MOX),
  with no tuner admitted, and for the
  antenna on a tuner with no antenna switch or a port outside 1 to 3.
  The reasons are in
  [remote accessory control version 1](2026-09-23-remote-accessory-control-v1.md).
- **The tuner's relays, Scan LAN and saved address.** `moveTgxlRelay`
  (`relay` 0 C1, 1 L, 2 C2; `direction` -1 or 1) moves one matching relay
  one step through the Core's tuner model, sending the line a local
  window's relay bar sends on a mouse-wheel step; the tuner's report
  arrives on `tuner` (`relayC1`, `relayL`, `relayC2`). `scanTgxlLan` has
  the Core listen for Tuner Genius announcements for three seconds; its
  `command.result` is sent when the window ends, with `values` holding
  `devicesJson` (utf8, a JSON array of
  `{"address","port","model","serial","nickname"}`). `setTgxlAddress`
  (`host`, `port`) saves the tuner's address for the Core's radio without
  dialling, with `configureTgxl`'s checks. None keys anything, so a
  receive-only Core takes them, but each is refused while the radio is
  on the air ("The radio is on the air. Try again when it stops."); the
  nudge also with no tuner admitted. The reasons are in the remote
  accessory control document.
- **The amp's OPERATE, Scan LAN and saved address.** `setPgxlOperate`
  (`on` bool) sends the Core's Power Genius the line a local window's
  applet OPERATE sends, `operate=1` or `operate=0`, through the Core's own
  connection; `accepted` means the line left, and the amp's report arrives
  on `amplifier` (`state`, `deviceState`, `operate`). `scanPgxlLan` and
  `setPgxlAddress` (`host`, `port`) are `scanTgxlLan` and
  `setTgxlAddress` for the Power Genius: Power Genius announcements only,
  `values` `devicesJson` when the three seconds end, and `PGXL_ManualIp`
  and `PGXL_ManualPort` saved without dialling with `configurePgxl`'s
  checks. None keys anything, so a receive-only Core takes them, but each
  is refused while the radio is on the air ("The radio is on the air. Try
  again when it stops."); `setPgxlOperate` also while the Core is not
  connected to the amp, and (iPhone app plan Task 77 fix round 3) while a
  Tuner Genius cycle runs or the Core's tuner reports its sweep ("The
  tuner is tuning. Try again when it finishes."), and (fix round 4) while
  the amp is still switching from an earlier operate command ("The
  amplifier is still switching. Try again in a moment."), except `on`
  false while an `operate=1` is unconfirmed. A `property.write` of `amplifier` `operate` stays
  refused. The reasons are in the remote accessory control document.
- **The RF-Kit's OPERATE, antenna, TCI mode and saved address.**
  `setRfKitOperate` (`on` bool), `setRfKitAntenna` (`port` i64, internal
  antenna 1 to 4) and `setRfKitTciMode` (no arguments) send the Core's
  admitted RF2K-S the REST request a local window's applet OPERATE, ANT
  button and RF-Kit page's "Set amp to TCI mode" send (`PUT /operate-mode`,
  `PUT /antennas/active`, `PUT /operational-interface`); `accepted` means
  the request left, and the amp's report arrives on `rfkit` (`operate`,
  `activeAntennaNumber`, `operationalInterface`). An antenna the amp lists
  as disabled, or does not list once it has listed its antennas, is
  refused. `setRfKitAddress` (`host`, `port`) saves `RfKit_ManualIp` and
  `RfKit_ManualPort` without dialling with `configureRfKit`'s address
  checks. None keys anything, so a receive-only Core takes them, but each
  is refused while the radio is on the air ("The radio is on the air. Try
  again when it stops."); the first three also while the Core is not
  connected to the amp. The reasons are in the remote accessory control
  document.
- **The Tune Power slider.** `setTunePowerForTxBand` (`watts`, 0 to 100,
  0 to 99 on a Hermes Lite 2) does what the TX applet's Tune Power slider
  does in a local window: it sets the tune power for the band the Core
  transmits on and sets the tune drive source to the tune slider, so TUNE
  uses that power. The Core reports both on `transmit`
  (`tunePowerForTxBand`, `tuneDrivePowerSource`). It keys nothing, so a
  receive-only Core takes it. Below `transmitSettingsVersion` 13 it is
  refused while the radio is on the air ("The radio is on the air. Try
  again when it stops."); at 13 it is taken on the air from a peer that may
  change the transmit settings, as the local slider moves keyed. Like
  `txProfile.select` it is the holder's while transmit is held (ruling
  7.7): from another device it is refused with the holder's words. It is
  refused outside the
  tune power range ("Choose a tune power from 0 to 100 W.", or "Choose a
  tune power from 0 to 99." on a Hermes Lite 2), and when not understood
  ("The request to change the tune power was not understood."). A peer
  below agreed minor 11 gets "Update this app to change the tune power on
  this Core."
- **TX profiles and the RADE vocoder.** `txProfile.select` (`name`)
  applies the named TX profile on the Core as the TX applet's profile
  combo does in a local window: its settings become the Core's transmit
  settings and it becomes the active profile. `txProfile.save` (`name`)
  saves the Core's current transmit settings under the name, as Setup >
  Audio > TX Profile's Save... does: a new name adds a profile, the name
  of an existing one overwrites that profile only, and a comma in the name
  becomes `_`. `txProfile.delete` (`name`) deletes the profile; deleting
  the active one makes another active, as locally. The Core reports its
  active profile and list on `transmit` (`activeTxProfile`,
  `txProfilesJson`). `rade.resetVocoder` (no arguments) clears the RADE
  transmit vocoder of the Core's active slice, as the RADE applet's Reset
  vocoder does. None keys the radio, so a receive-only Core takes them.
  Below `transmitSettingsVersion` 13 each is refused while the radio is on
  the air ("The radio is on the air. Try again when it stops."); at 13 each
  is taken on the air from a peer that may change the transmit settings, as
  the local profile combo and Reset vocoder work keyed. Each is the
  holder's while transmit is held (ruling 7.7): from a device that does not
  hold it, while another does (the Core's own key included), it is refused
  with the holder's words. The other refusals: "There is no
  transmit profile called <name>." (select and delete), "Give the transmit
  profile a name." (save with a blank name), "The Core has no radio to
  keep transmit profiles for." (save before the Core has a radio), "It is
  not possible to delete the last remaining TX profile." (the local page's
  own words), "RADE is not running on the Core's active slice." (reset
  with no RADE channel), "The request for the transmit profile was not
  understood." and "The request to reset the RADE vocoder was not
  understood." A peer below agreed minor 11 gets "Update this app to
  change transmit profiles on this Core."
- **The TX EQ curve.** `txEq.setCurve` (`curveJson`) and
  `txEq.resetCurve` (no arguments) change the TX EQ dialog's parametric
  curve, and are its Reset, from a peer offered `txEqCurveVersion` 2
  (section 7.1, "Changing the curve"). Each is that peer's own write of
  `transmit`'s `txEqParaEqData` and meets every rule that write meets
  (section 7.3); its answer carries that write's reason when refused, and
  `curve`, the curve the Core kept in the `txEqCurve` form, when taken.
  Other refusals: the curve's own ("Choose a curve of 5, 10 or 18
  points." and the rest, section 7.1), "The request to reset the TX EQ
  curve was not understood." (a reset with arguments), "The Core cannot
  change its transmit settings." (a Core with no radio model of its own).
  A peer not offered version 2 gets "Update this app to change the TX EQ
  curve on this Core."
- **The CFC band editor.** `cfc.setProfile` (`profileJson`,
  `expectedRevision`) applies the CFC dialog's whole band editor at once
  from a peer offered `transmitSettingsVersion` 15 (section 7.1, "The CFC
  band editor"). It is that peer's own write of `transmit`'s
  `cfcParaEqData` and meets every rule that write meets (section 7.3); its
  answer carries that write's reason when refused, and `profile`, the
  editor the Core kept in the `cfcProfile` form, when taken. Other
  refusals: a revision the Core has moved past ("The CFC settings changed
  on the Core. Check the new values and try again."), the editor's own
  ("Choose 5, 10 or 18 bands." and the rest, section 7.1), "The Core
  cannot change its transmit settings." (a Core with no radio model of its
  own, or one that offers less than 15). A peer below agreed minor 11 gets
  "Update this app to change the CFC settings on this Core."
- **PureSignal arming.** `ps3.single` (Single Cal), `ps3.automatic`
  (Automatic, and PS-A on), `ps3.applyCurrent` (Apply current correction)
  and `ps3.restoreCorrection` (Restore a saved correction) arm PureSignal
  on the Core as a local window's PureSignal controls do. Arming keys
  nothing: it sets the calibration engine's state, and the correction runs
  only while the radio transmits. A Core at `transmitSettingsVersion` 7
  takes them from a peer at agreed minor 11 while its radio is off the air
  and refuses them while it is on the air ("The radio is on the air. Try
  again when it stops."); at version 13 it takes them on the air too from a
  peer that may change the transmit settings, as the local PureSignal
  dialog arms keyed. Any other peer gets "PureSignal cannot be run
  from a remote window." as before, and so does `ps3.twoTone` with
  `enabled` true from every peer: the two-tone test keys the radio and
  waits for remote transmit. `ps3.off`, `ps3.twoTone` with `enabled` false
  and `ps3.saveCorrection` are taken as before. A taken action answers
  `accepted`, then `completed` or `failed` with the Core's reason (section
  9.1, the `phase` value), and the Core's `pureSignal` and
  `pureSignalSettings` objects follow in a `delta`.
- **One TCI switch.** The Core keeps one TCI switch and port for its own
  TCI server (`setStationTci`, the `stationTci` object). A desktop window
  connected to a Core with `stationTciVersion` 1 shows that switch and
  port as its own and changes them with `setStationTci`; an app does the
  same. The Core keeps the switch when the client leaves, so its server
  keeps running for the RF-Kit amplifier and for other apps. The wire is
  unchanged from `stationTciVersion` 1.
- **The Core's devices.** Taken only from a peer at agreed minor 11 whose
  hello declares `deviceAuth`, the peers the `devices` object goes to;
  from any other the station refuses them with "Update this app to manage
  this Core's paired devices." (`StationServer::onTransportText`).
  `devices.revoke` removes the paired device whose `id` (as in `listJson`)
  it names; the Core ends that device's connection (section 12.4), and a
  device may revoke itself, its connection ending just after the result.
  The last paired device is refused while no token is active, "Pair
  another device first, or reset this Core from its own computer.": its
  removal would leave the Core unclaimed, and only the console's reset
  does that (section 3.6). A device enrolled through the token
  (section 3.5) is refused while the token is active, "Stop accepting the
  pairing token first, then remove this computer.": the token would enrol
  it again at its next sign-in.
  `station.rename` stores a label (section 8.2): a callsign of letters,
  digits and `/`, then optionally `/` and up to 32 letters, digits, `-` or
  `_`; any other is refused with a reason that states the rule.
  `station.acknowledgeKeyBackup` records the operator's backup of the
  identity key. `station.retireToken` retires the token (section 3.3); it
  is refused, "Pair a device with this Core first, so a device can still
  sign in once the pairing token stops working.", until a device is paired,
  and accepted with nothing to do on a Core without a token. Each accepted
  one names `devices` in `affected`; its change reaches the object in the
  next `delta`.
- **The pairing window.** `pairing.open` and `pairing.close`
  (`pairingVersion` 1) go to the same peers as the device commands, and
  from any other the station refuses them with "Update this app to pair
  new devices with this Core.". `pairing.open` reopens the window on a
  claimed Core (nothing to do while it is open) and answers with `values`
  holding `code` (`utf8`), the current code. Only a connection signed in
  with a paired device's key may send it: one signed in with the token is
  refused with "Open pairing from a paired device or from the Core's
  console." (section 3.6). `pairing.close` closes a reopened window, from
  either kind of connection. Either one with arguments is refused ("The
  request to open pairing was not understood.", "The request to close
  pairing was not understood.").
- **Leaving on purpose.** `session.leave` (`sessionHolderVersion` 1, iPhone
  app plan Task 71) ends the device's session with no away time: its
  place is free at once (section 12.4). After the accepted result the
  station closes the connection, with no `session.end`; the client closes
  its end too. With arguments it is refused, "The request to leave the
  Core was not understood.". From a peer without `sessionHolderVersion` 1
  it is refused as a verb the station does not route is (section 9.2),
  and the connection stays up.
- **Answering the Core's questions** (`sessionHolderVersion` 1, iPhone
  app plan Task 74; section 7.5). `confirm.proceed {id, choice}` applies
  the change a `confirm.request` held (`choice` -1 when the kind has none,
  a choice's `choice` for a take); `confirm.cancel {id}` drops it and
  changes nothing; `notice.takeBack {id}` asks a `receiverTaken` or
  `sliceTaken` notice's take the other way, answered "Waiting for you to
  confirm." with `values` `phase` `needsConfirmation` and a
  `confirm.request`. A missing or renamed argument is refused, "The Core
  could not read this request."; an id that names no open question, "That
  question is no longer open. Make the change again.". From a peer without
  `sessionHolderVersion` 1 each is refused as a verb the station does not
  route (section 9.2).
- **Receiver requests from several devices** (section 7.5).
  `requestStreamCtunPinned` from a device that does not anchor the
  receiver is refused; `requestStreamCentre` from the anchor that would
  leave another device's slice outside is held and asked, from another
  device it takes the panadapter to a free receiver. `addSlice`,
  `addSliceOnPan` and those refused because every receiver (or slice) is
  in use name the devices holding them, and a device with the feature is
  then asked to take one.

### 9.2 Unknown verbs

A verb the station does not route gets `command.result` with `accepted`
false and the reason "The Core does not know this request. Updating the
Core may help." (or "The Core does not know this PureSignal action." and
"The Core does not know this request." inside those families). The
connection stays up.

### 9.3 Verbs from an older peer

The station refuses a gated verb from a peer whose agreed minor is below
the verb's minimum, with a plain reason such as "Update this app to set up
the RF-Kit amplifier on this Core." (`StationServer::onTransportText`).

## 10. Telemetry

When the agreed minor is at least 3 and `stationTelemetryVersion` is at
least 1, the station sends one `station.metrics.v1` message a second
(`DaemonTelemetryController::kSamplePeriodMs` 1000) after
`snapshot.complete`. Its `payload` is an object of observations; telemetry
is never a command. The client drops a message whose `sequence` is not
higher than the last, or whose `sampledElapsedMs` went backwards, and
ignores host fields unless the agreed minor is at least 10 and the version
at least 2, and receiver fields unless the minor is at least 11 and the
version at least 3, and the radio section's PA readings and link quality
unless the minor is at least 11 and the version at least 4, and its HL2
link unless the minor is at least 11 and the version at least 5. A message
over 16 KiB is refused.

At version 4 (remote-window parity Task 6) the radio section carries, each
absent when the radio has none or has not reported it and all absent while
the radio is not connected: `paVolts` (the PA drain volts, user ADC0),
`supplyVolts`, `paCurrentAmps` and `paTemperatureCelsius` (the Core's
`RadioModel::paReadings()`), `packetLossPercent` (lost over received plus
lost in the last 5 seconds, from the sequence errors Thetis counts, one
per mismatch), `jitterMs` (RFC 3550 section 6.4.1 interarrival jitter of
the lowest active receive stream), `packetGapMs` (the longest interval
between two datagrams from the radio in the last second), `sampleRateHz`
and `udpPacketsSeen` (datagrams from the radio since it connected). Volts,
amps, jitter and gap are finite and not negative, the loss is 0 to 100, a
temperature is not below absolute zero, and the counts are whole numbers.
A window shows each as the Core's, and one that is absent or out of date
as unavailable, never 0.

At version 5 (remote-window parity Task 14) the radio section also carries
the Core's Hermes Lite 2 link, from the bandwidth monitor its own HL2 I/O
tab, Radio Status and Diagnostics > Connection Quality read, only while
the radio is connected and only on a radio that has the monitor (the HL2):
`hl2RxBytesPerSecond` and `hl2TxBytesPerSecond` (bytes per second received
from the radio on EP6 and sent to it on EP2, finite and not negative),
`hl2Throttled` (bool: the LAN link is throttled) and `hl2SequenceGaps` (EP6
sequence gaps since the radio connected, a whole number). The throttle
event count is not sent: a remote window shows whether the link is
throttled now and says the count is not sent. Those three surfaces show
the Core's figures "from the Core", and unavailable, never 0, when absent
or out of date.

At version 6 (minor 11), the optional `radio.connectionAgeMs` is the
Core model's monotonic milliseconds since its current radio connection
entered Connected. It is independent of every client's
`sampledElapsedMs` and disappears on disconnect, link loss or replacement.
`radio.radioUdpBasePort` is observed on the live connection thread: P1's
actual destination port or P2's outbound base/control port. P2 stream roles
use other ports, so this is not a universal media port. Both fields are
absent when the radio is not connected; an out-of-date connection reply
also omits the port.

`radio.adcOverloads` is an optional array of at most three entries with
unique `adc` indices 0–2, limited to the connected board's actual ADC
count. An entry appears only after a valid status frame for that ADC and
contains `eventsSinceConnection`, the number of observed transitions from
unknown or explicitly clear into overload. It may carry `statusAgeMs`,
`lastOverloadAgeMs` (age of the latest positive status, only after an
event), and `overloaded` (true or false only when its latest explicit
status is no older than 3000 ms). A missing `overloaded` is unknown,
never measured clear; silence does not clear a bit. P1 status case 0x00
observes only ADC0, P1 case 0x20 observes ADC0–2, and P2 high-priority
status observes its three decoded bits. Counts reset with the radio
connection, not with a client session or a local dialog's Reset. There is
no Core reset command for these observations. Clients use these fields
only at minor 11 and `stationTelemetryVersion` 6; older peers receive
their prior telemetry shape. Malformed present values are refused.

<!-- surface:telemetry -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

Message kind `station.metrics.v1`.

| `stationTelemetryVersion` | Minimum minor | Field paths | Field paths added |
| --- | --- | --- | --- |
| 1 | 3 | 15 | `audio.active`, `audio.contextGeneration`, `audio.encodeFailuresPerSecond`, `audio.encodedPacketsPerSecond`, `audio.sendAcceptedPerSecond`, `audio.sendRejectedPerSecond`, `audio.sourceDropsPerSecond`, `audio.sourceFramesPerSecond`, `radio.connected`, `radio.rttAgeMs`, `radio.rttMs`, `radio.rxMbps`, `radio.txMbps`, `sampledElapsedMs`, `sequence` |
| 2 | 10 | 22 | `host.hottestZoneCelsius`, `host.hottestZoneName`, `host.memoryAvailableKiB`, `host.memoryTotalKiB`, `host.processCpuPercent`, `host.processResidentKiB`, `host.systemCpuPercent` |
| 3 | 11 | 26 | `receivers[].inputDelayMs`, `receivers[].loadPercent`, `receivers[].skippedInputMs`, `receivers[].sliceId` |
| 4 | 11 | 35 | `radio.jitterMs`, `radio.paCurrentAmps`, `radio.paTemperatureCelsius`, `radio.paVolts`, `radio.packetGapMs`, `radio.packetLossPercent`, `radio.sampleRateHz`, `radio.supplyVolts`, `radio.udpPacketsSeen` |
| 5 | 11 | 39 | `radio.hl2RxBytesPerSecond`, `radio.hl2SequenceGaps`, `radio.hl2Throttled`, `radio.hl2TxBytesPerSecond` |
| 6 | 11 | 46 | `radio.adcOverloads[].adc`, `radio.adcOverloads[].eventsSinceConnection`, `radio.adcOverloads[].lastOverloadAgeMs`, `radio.adcOverloads[].overloaded`, `radio.adcOverloads[].statusAgeMs`, `radio.connectionAgeMs`, `radio.radioUdpBasePort` |

<!-- /surface -->

## 11. Media control

`media.control` carries one operation object in `payload`, with an `op`
key naming it. The operations, their exact keys and their sequencing are
specified in
[remote media control version 1](2026-09-20-remote-media-control-v1.md);
the table lists the keys each operation carries as the code builds and
checks them. A whole `media.control` message over 128 KiB is refused. The
station accepts media control from each admitted session, for its own
media, after `snapshot.complete`, when media is available (iPhone app plan
Task 76; the several-devices design, ruling 9.1). Every admitted device has
its own media: its own media connection, display endpoints (at most 8),
receiver streams (at most 4) and headphones mix, and one device's
`media.control` never reaches another's. A device subscribes displays, and
asks for receiver streams, only for its own slices: a `subscribe` naming
another device's slice gets an `allocation-result` (or `rejected`) "That
slice belongs to another device.", and a `receiver-audio` naming one is
answered as a slice that is not there (`slice-removed`). When a slice
passes to another device, the old owner's displays on it retire as a
removed slice's do (reason "slice removed", on the `allocation-result`
for a budget-aware app, else `rejected`), and its receiver stream on it
stops as `slice-removed`: that device's view has destroyed the slice. A
receiver's
spectrum is computed once for everyone watching it: two devices' pans on
one receiver share its engine, which runs at the largest size and highest
rate any of them was granted, and a grant limited by that engine says
`sharedEngine` as it does between one device's pans. Each device hears its
own slices: its main audio (the speakers' mix, or its whole program for an
app without the headphones mix) and its headphones mix sum only its own
slices, each with its own gain, pan, mute and route; the Core's own output
plays only the station device's slices. A device that leaves ends its own
media and no other's. Telemetry (`station.metrics.v1`) goes to every
session that negotiated it, each on its own sequence. **Transmit joins
here:** the transmit holder's mix carries the transmit monitor
(`txMonitorAudioVersion`, remote-window parity Task 32; the media
document's "Transmit monitor (monitor-audio)"). The `subscribe` fields that
come with `displayExtrasVersion` (`peakBlobs`, `activePeakHold`,
`noiseFloor`, `waterfallLevels`, `normalize`, `calibrationOffsetDb`,
`averageTimeMs`, `waterfallAverageTimeMs`), their ranges and what the Core sends for them are
specified in [display extras v1](2026-09-23-display-extras-v1.md).

<!-- surface:mediaControl -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

Message kind `media.control`; the operation's keys sit in `payload`.

Client to station:

| Operation (`op`) | Capability | Fields always present | Fields present with | Object-valued fields |
| --- | --- | --- | --- | --- |
| `audio` | `remoteMediaVersion` | `connectionId`, `enabled`, `op`, `revision` | `opusBitrate` with audioProfileVersion, audioQualityVersion; `profile` with audioProfileVersion, remoteAudioStatusVersion | none |
| `candidate` | `remoteMediaVersion` | `candidate`, `connectionId`, `mid`, `op` | none | none |
| `clarity-retune` | `displayExtrasVersion` | `connectionId`, `endpointId`, `op` | none | none |
| `clock-probe` | `audioClockVersion` | `connectionId`, `id`, `op`, `t0` | none | none |
| `description` | `remoteMediaVersion` | `connectionId`, `op`, `sdp`, `type` | none | none |
| `headphones-audio` | `headphonesMixVersion` | `connectionId`, `enabled`, `op`, `profile`, `revision` | none | none |
| `iq-stream` | `remoteIqVersion` | `connectionId`, `enabled`, `op`, `revision`, `sliceId` | none | none |
| `keyframe` | `remoteMediaVersion` | `connectionId`, `contextGeneration`, `endpointId`, `op` | none | none |
| `monitor-audio` | `txMonitorAudioVersion` | `connectionId`, `op`, `revision`, `route` | none | none |
| `receiver-audio` | `receiverAudioVersion` | `connectionId`, `enabled`, `op`, `profile`, `revision`, `sliceId` | none | none |
| `replace` | `mediaReplaceVersion` | `connectionId`, `op`, `replaces` | `mediaDirectVersion` with mediaDirectVersion | none |
| `start` | `remoteMediaVersion` | `connectionId`, `op` | `audioProfileVersion` with audioProfileVersion; `headphonesMixVersion` with headphonesMixVersion; `mediaRelayRoutingVersion` with mediaRelayRoutingVersion; `mediaTunnelVersion` with mediaTunnelVersion; `miniDisplayVersion` with miniDisplayVersion; `receiverAudioVersion` with receiverAudioVersion; `remoteIqVersion` with remoteIqVersion; `remoteTxVersion` with remoteTxVersion; `txMonitorAudioVersion` with txMonitorAudioVersion | none |
| `subscribe` | `remoteMediaVersion` | `centreHz`, `connectionId`, `endpointId`, `fftSize`, `fps`, `framesPerLine`, `maxDbm`, `minDbm`, `op`, `pixels`, `revision`, `sliceId`, `spanHz`, `tier`, `trace`, `waterfall`, `wideSpanFactor`, `windowType` | `activePeakHold` with displayExtrasVersion; `averageTimeMs` with displayExtrasVersion; `calibrationOffsetDb` with displayExtrasVersion; `decimation` with spectrumGrantVersion; `displayRole` with miniDisplayVersion; `extendedView` with remoteWidebandDisplayVersion; `noiseFloor` with displayExtrasVersion; `normalize` with displayExtrasVersion; `peakBlobs` with displayExtrasVersion; `waterfallAverageTimeMs` with displayExtrasVersion; `waterfallLevels` with displayExtrasVersion | `activePeakHold`: {enabled, fallDbPerSec, holdMs, onTx}; `noiseFloor`: {enabled, fastAttack, shiftDb}; `peakBlobs`: {count, fallDbPerSec, holdMs, insideOnly}; `trace`: {averageAlpha, averageMode, detector}; `waterfall`: {averageAlpha, averageMode, detector}; `waterfallLevels`: {highDbm, lowDbm, mode, offsetDb} |
| `unsubscribe` | `remoteMediaVersion` | `connectionId`, `endpointId`, `op` | `revision` with remoteDisplayBudgetVersion | none |

Station to client:

| Operation (`op`) | Capability | Fields always present | Fields present with | Object-valued fields |
| --- | --- | --- | --- | --- |
| `allocation-result` | `remoteDisplayBudgetVersion` | `accepted`, `acceptedRevision`, `applicationBytesPerSecond`, `budgetGeneration`, `connectionId`, `endpointId`, `messagesPerSecond`, `op`, `reason`, `revision`, `spectrumSampleUnitsPerSecond` | none | none |
| `audio-context` | `remoteMediaVersion` | `connectionId`, `enabled`, `firstSequence`, `firstTimestamp`, `generation`, `op`, `revision`, `ssrc` | `encoder` with enabled=true, remoteAudioStatusVersion; `opusBitrateRefusal` with audioProfileVersion, audioQualityVersion, opusBitrateRefused, profile=opus, remoteAudioStatusVersion; `profile` with audioProfileVersion, remoteAudioStatusVersion; `profileRefusal` with audioProfileVersion, profile=opus, profileRefused, remoteAudioStatusVersion; `reason` with enabled=false, remoteAudioStatusVersion | `encoder`: {audioBandwidthHz, channels, codec, frameSamples, sampleRate, targetBitrate} or {bitsPerSample, channels, codec, frameSamples, payloadType, sampleRate} |
| `candidate` | `remoteMediaVersion` | `candidate`, `connectionId`, `mid`, `op` | none | none |
| `clock-echo` | `audioClockVersion` | `capturedNs`, `connectionId`, `generation`, `id`, `op`, `rtpTimestamp`, `t0`, `t1`, `t2` | none | none |
| `context` | `remoteMediaVersion` | `centreHz`, `connectionId`, `contextGeneration`, `endpointId`, `fps`, `framesPerLine`, `maxDbm`, `minDbm`, `op`, `revision`, `sampleRateHz`, `sourceCentreHz`, `sourceStream`, `spanHz`, `traceSamples`, `waterfallSamples`, `wideCentreHz`, `wideSamples`, `wideSpanHz` | `grantedFftSize` with spectrumGrantVersion; `grantedPixels` with spectrumGrantVersion; `grantedTier` with spectrumGrantVersion; `limit` with spectrumGrantVersion; `requestedPixels` with spectrumGrantVersion; `wideband` with remoteWidebandDisplayVersion | `wideband`: {active, adcRateHz, available, filterChainIndex, geometryRateBasis, highHz, levelReference, lowHz, physicalAdcIndex, sourceGeneration, version} or {active, available, version} |
| `description` | `remoteMediaVersion` | `connectionId`, `op`, `sdp`, `type` | none | none |
| `headphones-audio-context` | `headphonesMixVersion` | `connectionId`, `enabled`, `firstSequence`, `firstTimestamp`, `generation`, `op`, `profile`, `revision`, `ssrc` | `encoder` with enabled=true; `profileRefusal` with profile=opus, profileRefused; `reason` with enabled=false | `encoder`: {audioBandwidthHz, channels, codec, frameSamples, sampleRate, targetBitrate} or {bitsPerSample, channels, codec, frameSamples, payloadType, sampleRate} |
| `iq-stream-context` | `remoteIqVersion` | `connectionId`, `enabled`, `generation`, `op`, `reason`, `revision`, `sampleRateHz`, `sliceId` | none | none |
| `monitor-audio-context` | `txMonitorAudioVersion` | `connectionId`, `op`, `revision`, `route` | none | none |
| `noise-floor` | `remoteMediaVersion` | `connectionId`, `contextGeneration`, `endpointId`, `floorDbm`, `op`, `revision` | none | none |
| `receiver-audio-context` | `receiverAudioVersion` | `connectionId`, `enabled`, `firstSequence`, `firstTimestamp`, `generation`, `op`, `profile`, `revision`, `sliceId`, `ssrc` | `encoder` with enabled=true; `profileRefusal` with profile=opus, profileRefused; `reason` with enabled=false | `encoder`: {audioBandwidthHz, channels, codec, frameSamples, sampleRate, targetBitrate} or {bitsPerSample, channels, codec, frameSamples, payloadType, sampleRate} |
| `rejected` | `remoteMediaVersion` | `connectionId`, `endpointId`, `op`, `reason`, `revision` | none | none |
| `replace` | `mediaReplaceVersion` | `connectionId`, `op`, `replaces` | none | none |

<!-- /surface -->

## 12. Liveness, deadlines, caps and ending

### 12.1 Heartbeat

Each end sends a WebSocket ping every 20 s (`kDefaultHeartbeatIntervalMs`
20000, the same on both ends). A pong clears the count. At a tick where 2
pings (`kDefaultMaxMissedPongs`) are still unanswered, the station declares
the link dead and ends it with "This app stopped answering, so the Core
closed the connection.", `retryable` true. The
round-trip time of a pong is recorded for diagnostics only; a slow pong is
never a missed one.

### 12.2 The connect deadline

The whole connect sequence must finish within 30 s
(`kStationHandshakeDeadlineMs` 30000) of the WebSocket opening. The
station ends a connection that has not reached `snapshot.complete` by then
with "This app did not finish connecting to the Core in time.", `retryable`
true. The desktop client
runs the same deadline on its side.

### 12.3 Caps

- The station caps each inbound message and frame at 1 MiB
  (`StationServer::kMaxIncomingMessageBytes`), the client at 8 MiB
  (`StationClient::kMaxIncomingMessageBytes`). Both are applied to the
  socket before any frame is read.
- `media.control` messages are capped at 128 KiB and `station.metrics.v1`
  at 16 KiB, when encoded and when decoded.
- Openings (connections whose TLS or upgrade has not finished) are
  counted apart from connections: at most 64 at once
  (`StationServer::kMaxUnfinishedOpenings`) and 2 from one address or IPv6 /64
  (`kMaxHandshakesPerAddress`, counted as below), each until it opens, is
  refused or reaches the 10 s opening deadline (section 2). A new
  connection at either limit closes the oldest unfinished opening (from
  its own address first, then the oldest of all) and takes its place, so
  openings that never finish do not keep a device out, and a client that
  redials while its own abandoned dials are still opening gets its newest
  dial through. The total is 64, not the 24 connections below, because it
  is what a flood must fill to push a real device's opening out: with 2
  per address that takes 64 connects from at least 32 addresses or /64s
  within the device's own opening time. Each opening costs one file
  descriptor; `nereusd` runs under the default limit of 1024
  (`packaging/nereusd.service.in` sets no `LimitNOFILE`), so 64 openings
  and the 24 connections stay far below it.
- The station accepts at most 24 connections at once
  (`kMaxConcurrentPeers`), counting every socket, signed in, connecting or
  pairing: four devices each reconnecting with an old socket not yet
  noticed dead and four racing attempts, and a fifth device's four. The
  next one gets `session.end` "The Core already has as many connections
  as it allows. Try again shortly.", `retryable` true, before any `hello`,
  because a reconnecting client meets it while its own dead sockets
  drain.
- At most 4 devices hold places at once (`kMaxDeviceSessions`, iPhone app
  plan Task 71): sessions let in, devices away in their 3 minutes, and a
  hosting desktop's own window. Connections still connecting and pairing
  connections take no place.
- Of those, one address may hold at most 2 that are still connecting
  (their snapshot not yet sent; `kMaxHandshakesPerAddress`), so one host
  cannot hold every slot by redialling within the connect deadline. An
  IPv4 address counts by itself, and an IPv4-mapped IPv6 address counts
  as its IPv4 address. Any other IPv6 address counts by its /64 prefix,
  because one host can dial from every address in its /64; a household on
  one /64 therefore shares the 2, as one behind IPv4 NAT does. Signed-in
  sessions are not counted. The next one from that address or /64 gets
  the same `session.end`, `retryable` true. A connection
  with no address of its own (the relay's) is not counted by address.

### 12.4 Ending, admission and retryable

`session.end` carries a `reason` and `retryable`. `auth.result` carries
`retryable` too. A client redials only after a retryable end; after one
that is not retryable it stops and tells the operator.

Both may carry `code`, a stable token for the end (`SessionEndCode` in
`SessionMessages.h`): every permanent end the station sends has one. A
client reads the code where it is present and falls back to the reason text
for an older station, which sends none; an older client ignores the key.
The reason stays what the operator reads. A `code` on the wire is a
non-empty string; an empty one is refused like any mistyped key.

| Cause | Message | `retryable` | `code` |
| --- | --- | --- | --- |
| Wrong token | `auth.result` accepted false, "The Core did not accept this app's pairing token. Check the token saved for this Core.", then the station closes | false | `wrongToken` |
| Token checks locked out (section 3.3) | `auth.result` accepted false, "The Core is refusing pairing tokens for a while after too many wrong ones. Try again later." | true | none |
| A token on a Core without one (section 3.3) | `auth.result` accepted false, "This Core uses paired devices. Pair this device first." | false | `pairingRequired` |
| A device the Core has not paired (section 3.5) | `auth.result` accepted false, "This device is not paired with this Core. Pair it first." | false | `deviceNotPaired` |
| A device sign-in that does not prove itself (section 3.5) | `auth.result` accepted false, "This device could not prove it is paired with this Core." | false | `deviceProofFailed` |
| Device sign-ins limited (section 3.5) | `auth.result` accepted false, "The Core is refusing sign-ins from this device for a while after too many failed ones. Try again later." | true | none |
| No shared major (section 6.1) | `session.end` naming both sides' versions and the side to update | false | `linkVersion` |
| Message the station cannot decode (section 13) | `session.end` "The Core could not read a message from this app." | false | `protocolError` |
| Out-of-order handshake (section 5.1) | `session.end` | false | `protocolError` |
| The same device connected again (section 5.1): its older connection | `session.end` "This device connected again." | false | `sameDevice` |
| Held device cancels or its original 60 seconds expire (section 5.1) | `session.end` "The Core already has four devices connected." | false | `coreFull` |
| A device's place is taken by a held device (section 5.1) | `session.end` "<taker's name> took this device's place on the Core." with `takenOverBy`, `takenOverById`, `secondsAgo` | false | `takenOver` |
| Every place on the Core is taken (section 5.1), an older window | `session.end` "The Core is full. Update NereusSDR to take a device's place, or try again later." | true | none |
| A window without the several-devices feature, with no slice for it at sign-in (section 7.5) | `session.end` "All the radio's slices are in use. Try again when another device closes one." or "All the radio's receivers are in use. Try again when another device frees one." | true | none |
| Such a window's last slice taken by another device (section 7.5) | `session.end` "<taker's name> took the receiver this app was using. Update NereusSDR to share the Core." | false | `takenOver` |
| Connection limit reached | `session.end` | true | none |
| Connect deadline expired | `session.end` | true | none |
| Heartbeat timeout | `session.end` | true | none |
| Station shutting down | `session.end` "The Core is shutting down." | true | none |
| The Core changes its radio (`station.selectRadio`, section 9.1): every connection, after what was sent to it | `session.end` "The Core is switching to <radio name>. This app reconnects by itself." | true | `radioChanging` |
| The connection's device was removed (`devices.revoke`, the Core's console, a reset) | `session.end` "This device was removed from the Core." | false | `deviceRemoved` |
| A connection signed in by token when the token is retired (`station.retireToken`) | `session.end` "This Core uses paired devices. Pair this device first." | false | `pairingRequired` |

A device's connection ends whatever removed the device
(`DeviceStore::deviceRemoved`): the command, the Core's console or a reset.
The connection that asked is ended just after its own `command.result`.

A pairing connection (section 3.6) ends after `pair.accept`, the
station's `pair.confirm` or `pair.fail`, with no `session.end`: the reason
a client acts on is `pair.fail`'s.

One code is defined for an end the station never sends:
`identityChanged`, which a client uses for its own end when the Core's
certificate binding or identity key is not the one it paired with. The
desktop client ends that way before it sends its own `hello`: a saved Core
whose `hello` shows no identity or another key, or whose `certBinding` does
not verify for the certificate the connection presented, is refused and
never trusted silently. A new certificate whose binding verifies is
accepted without a question, whatever pin was saved.

The one end the station sends today with `takenOver` is an older
window's last slice taken (section 7.5), with its own words above; the
fifth device's takeover, which will also carry it, is a later version's. The takeover and version
reasons are worded in one place,
`src/core/session/SessionEndReasons.{h,cpp}`: "Another app at
*address:port* connected to the Core and took over. Connect again to take
it back." and "This Core runs link version *N* and this app runs version
*M*. Update the Core." (or "Update this app." when the app is the older
side). An app that offers its own next steps for these two (take the Core
back, check for updates) tells them apart by the `code` (`takenOver`,
`linkVersion`) and falls back to these exact words for an older station.
The desktop client reads the code (`SessionEndReasons::read`, iPhone app
plan Task 18): `takenOver`, `linkVersion`, `deviceRemoved` (and
`deviceNotPaired`, the same notice: pair this computer again),
`pairingRequired`, and its own `identityChanged` each choose the window's
stop message, whatever the words; the words give only the other app's
address and the two versions where they are present. It reads the words
(`SessionEndReasons::parse`) only for an end that carries no code, from a
Core older than the code.

**Several devices** (iPhone app plan Task 71). Up to four devices hold
places at once, and no sign-in ever ends another device's session. A
device's own newer connection replaces its older one, which ends with
`sameDevice`, not retryable, so the two do not trade places.

`radioChanging` is the one retryable end with a code: the Core restarts its
run on another radio (the operator's ruling of 2026-09-26). An app takes it
as a reconnect, not a failure; the desktop window says "Core changing
radio, reconnecting" with the Core's words and "This window reconnects by
itself when the Core is back." until the next session is up. A paired
device whose session ends without `session.leave` (a lost link, the
heartbeat's end, a closed socket, an app its system stopped) is **away**
for 3 minutes (`graceMs` 180000), keeping its place; signing in again
within them is the same-device case, let in with no question. When the 3
minutes end its place is freed and the Core keeps that its time ran out
until the device next signs in, is removed, or the Core restarts. A window
signed in with the token and no key, and a device that leaves with
`session.leave` (section 9.1), frees its place at once; so does removing a
device, away or not. The heartbeat timeout stays retryable: that end is
what starts a device's 3 minutes.

**What happens to a device's slices** (iPhone app plan Task 73; the
several-devices design, rulings 4.11, 4.12 and 5.2; slice control plan
Task 8). An away device's slices keep running, its own, their markers
`ownerAway`, and it stays a listener of what it listened to. When its 3
minutes end (for that absence only: a device that came back, or dropped
again since, keeps its claims), or it leaves with `session.leave`, every
claim it has goes at once. A slice it controlled that another device
listens to keeps running with no controller for that listener; one
nobody else is on closes, and the Core saves it for the device's return,
with the slice's own settings, the Core's last slice included (the Core
then has no slice). It leaves every slice it only listened to, and one
with nobody left closes. No slice is held for it. A token window's
claims go the same way when its session ends; its slices are not saved
(it cannot be recognised again). Removing a device, and a fifth device
taking its place, release its claims the same way; removing it also
forgets what was saved for it. Slices held for a device (a layout
restored after a restart, ruling 5.3) are released with its other claims.
While a device is away within its 3 minutes, another device may take
control of its idle slice (`slice.takeControl`); the away device stays a
listener and, back, finds it so. A device's slices while it is away, or
held for it, never count in the receive filter choice and are never
named in a question about a change that would affect other devices
(section 7.3); such a change reaching only them applies at once. When a
device is let in, the Core returns the slices held for it, restores its
saved slices where they fit (a receiver window that covers each or a
free receiver; its old letter when free, else the lowest free), never
over a slice that still exists, keeps any that do not fit for next time,
and, if it still owns none, gives it one (section 7.1).

**Routing with several devices** (iPhone app plan Task 72; the
several-devices design, ruling 5.8). Each session has its own view of the
station's state, with its own pending changes and its own connect-time
burst (section 5.1), each message fitted to what that session negotiated:
its minor and its capabilities, as before.

| Message | Goes to |
| --- | --- |
| `command.result`, `property.result`, `settings.reject` | the session that asked, only |
| `confirm.request`, `notice` | the one device they are for, only with `sessionHolderVersion` 1; a notice for an away device waits for its return (section 7.5) |
| `delta`, `object.create`, `object.destroy` | every session holding the object; a device's own write is not echoed to it (section 7.3). A `slice:<id>` only to the session of the device that owns it; a `marker:<id>` to every session with `sessionHolderVersion` 1 but that one (section 7.1) |
| `settings.value` | every session, with the writer's `origin` (section 8.1) |
| a write's readback of its side effects on the written object | the writer, only (section 7.3) |
| `capabilities` | each session its own, with its own share of the display budget (section 6.4) |
| `media.control`, media, `station.metrics.v1` | each session its own (section 11) |

The desktop client redials on the schedule 1, 2, 5, 10, 30 and 60 s, then
stays at 60 s (`kReconnectBackoffSteps` in `StationClient.cpp`), and starts
the schedule over after a connection that reached `snapshot.complete` (and,
with media, after the media connection is established).

## 13. Unknown message kinds

The two ends treat a kind they do not know differently:

- **A client** logs a message it cannot decode, or a kind meant for the
  other direction, and ignores it (`StationClient::onTransportText`). A
  newer station's new kind therefore costs an older client nothing.
- **The station** ends the connection when it cannot decode a message: an
  unknown `type`, a missing or mistyped required key, or an oversized
  `media.control`. It sends `session.end` "The Core could not read a
  message from this app." with
  `retryable` false (`StationServer::onTransportText`). A known kind meant
  for the other direction is logged and ignored.

So a client never sends a message kind, or a verb or property the station
has not advertised (section 6.2). An unknown verb does not end the
connection (section 9.2), but the gate keeps a client from relying on
that.

## 14. Discovery

A station that listens makes itself known on its local networks two ways,
independent of each other (iPhone app plan Task 16; D36; the pairing
design, section 6): the LAN announcement, which desktops listen for, and
Bonjour, which the iPhone app browses for, because iOS lets an app receive
custom multicast only with a permission Apple grants on request. Both
follow one rule: they go out only on addresses the listener serves. A
listener bound to loopback only is neither announced nor advertised
(`stationLanListenerServesAddress` in `StationLanAnnouncer.cpp`,
`dnsSdInterfaceForListener` in `DnsSdAdvertiser.cpp`). Discovery is never
trust: a client pins what it finds (section 3.2) or pairs (section 3.6).

Neither carries the Core's global addresses: those reach only a device
signed in with its own key (`coreAddresses`, section 7.1).

Both carry the same six facts about the Core, and both change when one
does (`DaemonApp::updateStationAnnouncement`):

- **identity**: the identity fingerprint, SHA-256 of the identity key
  (section 3.4);
- **label**: the Core's label as displayed (`StationLabel`, renamed by
  `station.rename`, section 9.1), or empty when the Core has none. A list
  shows the label, or the Core name when it is empty;
- **claimed**: whether the Core has a paired device or an active token
  (`DeviceStore::isClaimed`);
- **pairing**: how the Core takes a new device right now, from its pairing
  window (section 3.6): `click` while `OpenUnclaimed` with
  `pairing_lan_click` allowed (one tap on this network pairs; the code does
  too), `code` while `OpenUnclaimed` with it denied or while
  `OpenReopened`, and `closed` while `ClosedClaimed` or when the Core does
  not pair (`pairingVersion` 0);
- **devices** (iPhone app plan Task 71): how many devices hold a place on
  the Core, 0 to 4, counted as section 12.3 counts them
  (`StationServer::devicesConnectedForDiscovery`); a Core no device has
  claimed sends 0. A number only: who is on the Core reaches paired,
  signed-in devices alone (`connectedDevices`, section 7.1). A Core
  reached through the rendezvous or the relay has no announcement, so its
  count shows only after sign-in.
- **radio** (iPhone app plan Task 25, R-IOS-16): the station's radio,
  `connected` while it is connected (the same as Radio connected),
  `waiting` while the station waits for a radio to be chosen (it sees
  none, or more than one with nothing saved to pick between them;
  `StationRadios::choose`, `RadioModel::stationRadioWaiting`), and
  `offline` otherwise. A list shows "Waiting for a radio" before a device
  connects. A Core reached through the rendezvous or the relay says so only
  after sign-in (the `stationRadios` stream).

### 14.1 The LAN announcement

(`StationLanAnnouncement.h`, `StationLanAnnouncer.cpp`)

- UDP to port 47910, to the multicast groups 239.255.42.99 (IPv4) and
  `ff12::4e52:5344` (IPv6), from one source address of each family on each
  eligible interface, with a multicast hop limit of 1 and multicast
  loopback off;
- once when it starts, then every 5 s (`kStationLanAnnouncementIntervalMs`
  5000); a listener forgets a station it has not heard for 15 s
  (`kStationLanCacheTtlMs`);
- a listener takes datagrams of at most 512 bytes
  (`kStationLanMaxDatagramBytes`); with the fields below a schema-2
  datagram is at most 481 (`kStationLanMaxSchema2DatagramBytes`; 479 before
  the device count, 480 before the radio state), so no field is ever cut
  short.

A station sends schema 2 only (`kStationLanAnnouncementSchema`). A listener
reads schema 1 and schema 2, so a Core from before schema 2 is still found.
The datagram is binary, in this order; schema 1 ends after the radio MAC:

| Field | Size | Value |
| --- | --- | --- |
| Magic | 4 bytes | ASCII `NRSC` |
| Schema | 1 byte | 1 or 2 |
| Service | 1 byte | 1: the control WebSocket over TLS (`kStationLanWssControlService`) |
| Control port | 2 bytes | big-endian, not 0 |
| Pin | 95 bytes | the certificate pin (section 3.2), uppercase |
| Core name length | 1 byte | 1 to 128 |
| Core name | that many bytes | UTF-8, no control characters: `core_name`, or the host name |
| Radio connected | 1 byte | 0 or 1 |
| Radio name length | 1 byte | 0 to 128; at least 1 when the radio is connected |
| Radio name | that many bytes | UTF-8, no control characters |
| Radio MAC | 17 bytes | uppercase hex pairs joined by colons; `00:00:00:00:00:00` only when no radio is connected |
| Claimed | 1 byte | schema 2: 0 or 1 |
| Identity | 32 bytes | schema 2: the identity fingerprint, raw |
| Label length | 1 byte | schema 2: 0 to 65 (`kStationLanMaxLabelBytes`) |
| Label | that many bytes | schema 2: ASCII letters, digits, `/`, `_` and `-` (a callsign of up to 32, `/`, a suffix of up to 32) |
| Pairing | 1 byte | schema 2: 0 `closed`, 1 `click`, 2 `code` |
| Devices connected | 1 byte | schema 2, appended by iPhone app plan Task 71: 0 to 4 (`kStationLanMaxDevicesConnected`), the places taken; a station always sends it. A reader that never sees it (a datagram from an older Core) takes the count as not known and shows none |
| Radio | 1 byte | schema 2, appended by iPhone app plan Task 25 after Devices connected, which it needs: 0 `offline`, 1 `connected`, 2 `waiting` (`StationLanRadio`); 1 exactly when Radio connected is 1, and a datagram where they disagree is refused. A station always sends it. A reader that never sees it (an older Core), or sees a value it does not know (a later Core's state), takes the state as not known and lists the Core as before |

**Schema 2 extends by appending.** A reader ignores any bytes after the
schema-2 fields it knows. It still refuses a datagram that is too short
for those fields, or larger than 512 bytes, and applies every other rule
in this section. A writer only ever appends a new field after the last
one, and each new field states the value a reader that never sees it
assumes. Schema 1 does not extend: bytes after a schema-1 datagram's
radio MAC are refused.

A listener refuses a datagram with another magic, service or schema, a
field that fails these rules, or, in schema 1, any bytes left over. It dials
`wss://<source address>:<control port>`, with the IPv6 scope when the
address is link-local, and pins the announced pin. It keeps one entry per
endpoint (pin, source address and scope, interface, control port). A
schema-1 datagram for an endpoint that already sent schema 2 updates only
the fields schema 1 carries; it never clears the identity, label, claimed
state or pairing (`StationLanCache::ingest`).

A schema-1 datagram for an endpoint that already sent schema 2 does not
clear its device count either, and keeps its radio state while schema 1's
Radio connected agrees with it; when it no longer does, the state is not
known.

The conformance vectors `media/lan-announcement.bin` (schema 1),
`media/lan-announcement-2.bin` (schema 2, from a Core before the device
count) and `media/lan-announcement-2-devices.bin` (the same datagram with
the count, 2), `media/lan-announcement-2-radio.bin` (the same datagram
with the radio state, `connected`) and `media/lan-announcement-2-waiting.bin`
(the same Core waiting for a radio: no radio name, the unknown MAC,
`waiting`) (section 16.4) are datagrams the
station's own encoder wrote, with their decoded fields, `schema` among
them, in the `.expect.json` beside each (`devicesConnected` and `radio`
only where the datagram carries them). `media/lan-announcement-2-trailing.bin`
is the `lan-announcement-2-radio` datagram with five bytes appended after
the radio state, as a later field would be; its expectation holds the same
fields and `ignoredTrailingBytes` 5. `tst_link_conformance_media`
decodes each and encodes the fields again, so a change to this layout
fails there until the vectors, and this table, move with it.

### 14.2 Bonjour

(`DnsSdAdvertiser.h`)

The station registers one DNS-SD service:

- service type `_nereus-station._tcp` (`kDnsSdServiceType`), domain
  `local.`, on the listener's port;
- on every interface for a listener on every address, or on the interface
  holding the address a listener is bound to; never for a loopback
  listener;
- instance name: the label, or the Core name when there is no label,
  without control characters and cut to 63 bytes of UTF-8 at a character
  boundary (`dnsSdInstanceName`), or `NereusSDR Core` when nothing is left.
  Bonjour renames it when another service holds the name, so a client reads
  the Core's label from the TXT record's `name`, not from the instance
  name;
- a TXT record of seven entries, in this order:

| Key | Value |
| --- | --- |
| `v` | `1`, this record's version |
| `id` | the first 22 characters (`kDnsSdIdentityPrefixChars`) of the identity fingerprint in base64url without padding (RFC 4648 section 5) |
| `claimed` | `0` or `1` |
| `pair` | `click`, `code` or `closed` |
| `name` | the label, possibly empty; at most 65 characters |
| `devices` | iPhone app plan Task 71: `0` to `4`, how many devices hold a place on the Core (section 14); `0` on a Core no device has claimed. `v` stays `1`: an older client ignores the key |
| `radio` | iPhone app plan Task 25: `connected`, `offline` or `waiting`, the station's radio (section 14). `v` stays `1`: an older client ignores the key, and a client takes a value it does not know as not known |

A client ignores a key it does not know, so a newer station still lists,
and treats a record whose `v` is not `1` as one it cannot read. `id` names
the Core in a list; it proves nothing, and a client that pairs or signs in
checks the whole identity key the station's `hello` carries (section 3.5).

The station updates the TXT record in place when a fact changes, and
registers again when the instance name changes. Each platform uses what it
ships: `dns_sd.h` on macOS (`DnsSdAdvertiserApple.cpp`), the Avahi daemon
over D-Bus on Linux (`DnsSdAdvertiserAvahi.cpp`, with Qt's D-Bus module),
and `DnsServiceRegister` from `dnsapi.dll` on Windows 10 1903 and later
(`DnsSdAdvertiserWindows.cpp`). Where Bonjour is not available (no
avahi-daemon, an older Windows), the station still announces over the LAN
datagram and logs once that iPhones and iPads will not find it by
themselves (`DnsSdAdvertiser::unavailableText`).

The conformance vector `media/dnssd-txt.bin` (section 16.4) is the TXT
record the station's encoder writes for the Core of
`media/lan-announcement-2-radio.bin`, each entry preceded by its length in one
byte (RFC 6763 section 6.1), with the service type and the entries as
strings in `media/dnssd-txt.expect.json`.

## 15. Limits

The limits both ends keep. Three come from constants the surface test
cannot link, so `tst_link_surface_manifest` reads their source lines back:
`kMaxEndpoints` (8) in `src/core/session/media/DaemonMediaController.cpp`;
the endpoint width minimum 1 and the frame rate bounds 1 and 60, literals in
the subscribe check `exactInt(...)` in the same file (the frame rate
maximum also equals `kMaximumSpectrumDisplayFramesPerSecond` in
`DisplayBudget.h`); and `kReconnectBackoffSteps` in
`src/core/session/StationClient.cpp`, in units of
`StationClient::kDefaultReconnectBackoffUnitMs` (1000 ms).

<!-- surface:limits -->
<!-- Generated by scripts/render-link-tables.py from tests/data/link/v1/surface.json. Do not edit by hand. -->

| Limit | Value | Unit | Source in the code |
| --- | --- | --- | --- |
| `clientInboundMessageBytes` | 8388608 | bytes | StationClient::kMaxIncomingMessageBytes |
| `clientReconnectBackoffMs` | 1000, 2000, 5000, 10000, 30000, 60000 | ms | StationClient.cpp kReconnectBackoffSteps x StationClient::kDefaultReconnectBackoffUnitMs |
| `confirmExpiryMs` | 60000 | ms | ConfirmStep::kExpiryMs |
| `connectDeadlineMs` | 30000 | ms | kStationHandshakeDeadlineMs |
| `controlChannelChunkBytes` | 61440 | bytes | ControlFraming::kMaxChunkBytes |
| `deltaFlushMs` | 50 | ms | StationServer::kDefaultDeltaFlushMs |
| `endpointFps` | 1 to 60 | frames per second | DaemonMediaController.cpp handleSubscribe literal 1; kMaximumSpectrumDisplayFramesPerSecond |
| `endpointPixels` | 1 to 4096 | pixels | DaemonMediaController.cpp handleSubscribe literal 1; SpectrumEndpoint::kMaxPixels |
| `graceMs` | 180000 | ms | DeviceSessionRegistry::kGraceMs |
| `heartbeatIntervalMs` | 20000 | ms | StationServer::kDefaultHeartbeatIntervalMs |
| `lanAnnouncementMaxBytes` | 481 | bytes | kStationLanMaxSchema2DatagramBytes |
| `maxDeviceSessions` | 4 | count | StationServer::kMaxDeviceSessions |
| `maxDisplayEndpoints` | 8 | count | DaemonMediaController.cpp kMaxEndpoints |
| `maxHandshakesPerAddress` | 2 | count | StationServer::kMaxHandshakesPerAddress |
| `maxPeers` | 24 | count | StationServer::kMaxConcurrentPeers |
| `mediaControlBytes` | 131072 | bytes | kMaxMediaControlBytes |
| `mediaReplaceDrainMs` | 2000 | ms | DaemonMediaController::kReplaceDrainMs |
| `mediaReplaceOverlapMs` | 1000 | ms | DaemonMediaController::kReplaceOverlapMs |
| `missedPongs` | 2 | count | StationServer::kDefaultMaxMissedPongs |
| `pathOldCloseMs` | 5000 | ms | SwitchableTransport::kOldCloseMs |
| `pathSwitchDeadlineMs` | 10000 | ms | SwitchableTransport::kSwitchDeadlineMs |
| `pathTicketLifetimeMs` | 10000 | ms | StationServer::kPathTicketLifetimeMs |
| `pathTicketMaxChars` | 128 | characters | kMaxPathTicketChars |
| `pathUpgradeRetryMs` | 5000, 30000, 120000, 300000 | ms | PathRacer::kUpgradeRetryMs |
| `raceIpv4DelayMs` | 250 | ms | PathRacer::kIpv4DelayMs |
| `serviceAnswerDeadlineMs` | 10000 | ms | RendezvousDialer::kAnswerDeadlineMs |
| `shortNameMaxBytes` | 32 | bytes | DeviceStore::kMaxShortNameBytes |
| `stationInboundMessageBytes` | 1048576 | bytes | StationServer::kMaxIncomingMessageBytes |
| `takeoverAnswerMs` | 60000 | ms | StationServer::kTakeoverAnswerMs |
| `telemetryBytes` | 16384 | bytes | kMaxStationTelemetryBytes |

<!-- /surface -->

## 16. Conformance

`tests/data/link/v1/` holds the machine-readable half of this document.
Both the station's tests and the app's tests read it; nothing in it is
bundled into the app. The station's runners are
`tst_link_conformance_control`, `tst_link_conformance_session` and
`tst_link_conformance_media`, over the shared loader, placeholder matcher
and script player in `tests/LinkFixtures.{h,cpp}`. The app runs the same
files against its own client.

### 16.1 The files

- `surface.json` is the link's surface: the nine sections this document's
  tables render (`messageKinds`, `capabilities`, `mirrorClasses`,
  `objectKeys`, `commands`, `settingsScope`, `telemetry`, `mediaControl`,
  `limits`). `tst_link_surface_manifest` captures it from the code and
  fails when the committed file differs; `tst_link_surface_manifest_regen`
  writes it again (`NEREUS_LINK_REGEN_OUT=tests/data/link/v1`). Each
  `capabilities` entry carries the `value` a station with every feature on
  sends; section 6.3 is rendered from it.
- `manifest.json` lists the fixtures:
  `{"linkMajors":[1],"fixtures":[{"id":"<fixture id>","file":"<path under v1/>","kind":"control"|"session"|"media"|"framing","requires":{"<feature>":<version>}}]}`.
  `requires` names the capability versions a fixture exercises, and is
  `{}` for a fixture every major-1 station passes. Each version is a
  minimum: a station passes the fixture's requirement when it advertises
  that capability at the version named **or later**. A verb's own gate
  can be stricter, and the fixture then holds a station to that gate too:
  `session-verbs-ps3` requires `psAlgorithmVersion` 3, and the PureSignal
  action verbs it invokes need `psAlgorithmVersion` **equal to** 3
  (section 6.2), so a station advertising 4 meets the requirement but not
  those verbs' gate. `linkMajors` is the
  link majors the suite covers, whole numbers from 1 to 65535, oldest
  first, without repeats; the station's runners check each against the
  majors the station supports (`LinkVersion::supportedMajors()`). Each runner runs its fixtures once per
  major in it, against a station that offers that major, and fails when
  this station does not offer it. Every file under
  `control/`, `sessions/`, `media/` and `framing/` is listed once; a media
  entry names
  its `.bin`, and its `.expect.json` sits beside it.
- `control/*.json`: `{"from":"station"|"client","wire":{<the exact message>},"decodes":true|false}`.
- `sessions/*.json`:
  `{"runs":[<ends>],"stationSetup":{<the fake radio model and settings>},"steps":[<steps>]}`.
  `runs` names the ends that run the fixture, each once: `"station"` (the
  station's runner, which runs every fixture) and `"app"` (an app's
  runner, section 16.3). A step is one of
  `{"from":"station","message":{<a message>}}`,
  `{"from":"client","role":"behaviour"|"scripted","message":{<a message>}}`,
  `{"advanceMs":N}` or `{"expectClosed":{"retryable":true|false}}`.
  No other key is allowed at either level.
- `framing/*.json` (iPhone app plan Task 28): the chunking and heartbeat
  bytes of the control data channel (section 20), as
  `{"receiver":"station"|"client","message":[<pieces>],"frames":[<frames>],"encodes":true|false,"outcome":"delivered"|"refused","replies":["<hex>",...]}`.
  `message` is the session message's bytes, its pieces joined in order:
  `{"text":"<string>"}` is the string's UTF-8 bytes and
  `{"repeat":"<string>","times":N}` those bytes N times (N a whole number of
  at least 1); no piece is empty. `frames` are the data-channel messages, in
  order: `{"chunk":"more"|"last","from":A,"length":L}` is the chunk whose
  first byte is `0x01` (more) or `0x02` (last) followed by the message's
  bytes from offset A for L bytes, and `{"hex":"<lower-case hex>"}` is a
  message of exactly those bytes (possibly none). A fixture with
  `encodes` true holds chunks only, and a sender cutting `message` must
  make exactly `frames`. Every fixture is also fed, frame by frame, to a
  receiver with the inbound cap of the end `receiver` names (section 12.3):
  with `outcome` `"delivered"` it must deliver exactly one message, equal to
  `message`, with nothing left over; with `"refused"` it must end the
  connection, having delivered nothing. Either way it must answer the pings
  among `frames` with exactly `replies`, in order, each a pong's bytes in
  lower-case hex. No other key is allowed at either level. The manifest
  lists each with `"kind": "framing"`.
- `media/*.bin` with `*.expect.json`: the bytes of one packet exactly as
  it travels, and `{"codec":<codec>,"expect":{<decoded values>}}`, where
  `<codec>` is `nsdc1`, `nsdx1` (the display extras datagram), `ps3d`,
  `opus`, `nrsc1` (the LAN announcement of section 14.1) or `dnssd-txt`
  (the Bonjour TXT record of section 14.2). `expect` may hold `"after": ["<fixture id>", ...]` for a
  codec whose decoder keeps state (section 16.4).

A message in a fixture may hold placeholders in place of a value. Each is
a JSON string:

| Placeholder | Matches | Station message | Client message |
| --- | --- | --- | --- |
| `"$any"` | any value; the key must be present | yes | no |
| `"$string"` | any string | yes | yes, filled with `"conformance"` |
| `"$string:<name>"` | any string, recorded under `<name>` | yes, but not in a fixture for the app | yes, filled with `"conformance"` and recorded |
| `"$int"` | any whole number | yes | yes, filled with `0` |
| `"$int:<name>"` | any whole number, recorded under `<name>` | yes, but not in a fixture for the app | yes, filled with the next number of the fixture's counter (1 first, then 2, ...) and recorded |
| `"$int:<name>:<min>:<max>"` | a whole number from `<min>` to `<max>` (each a whole number in JSON syntax), recorded under `<name>` | yes, but not in a fixture for the app | yes, filled as `"$int:<name>"`; a counter value outside the range is a malformed fixture |
| `"$object"` | any JSON object | yes, but not in a fixture for the app | yes, filled with `{}` |
| `"$majors"` | only as the `majors` of a `hello`: a non-empty array of whole numbers from 0 to 65535, ascending, without repeats, that holds the same message's `major` | yes, but not in a fixture for the app | yes, filled with `[major]`, the message's own (filled) `major` alone |
| `"$capture:<name>"` | any value, recorded under `<name>` | yes | no |
| `"$uuid:<name>"` | only a client's media connection id (a `media.control` payload's `connectionId`, section 11): a canonical UUID, lower case, without braces, recorded under `<name>` | no | yes, filled with a new random UUID of that form at each fill and recorded; an app's runner records the UUID its client chose for the connection |
| `"$ref:<name>"` | the value recorded under `<name>`, compared the same way | yes | yes, filled with the recorded value |
| `"$ref:device:<n>"`, `"$ref:device:self"` | only in a fixture for the station alone: the id (base64url key fingerprint) of a device the station runner paired at run time, `<n>` from 1 to `otherPairedDevices` in the order it paired them, `self` its own (`pairedDevice`); recorded before the client connects | yes | yes, by the station's runner |
| `"$within:<t>:<v>"` | a number no further than `<t>` from `<v>`; `<t>` and `<v>` are each exactly a JSON number (RFC 8259 section 6: no `+`, no leading `.`, no `inf` or `nan`, no spaces), `<t>` at least 0; any other text is a malformed fixture | yes | no |
| `"$device:<case>"` | only as `auth.request`'s `device` | no | yes, by the station's runner: the device block (section 3.5, with `shortName` "Conformance") of the runner's own device, whose key it makes at run time, signing the transcript of the challenge recorded as `challenge`; `<case>` is `signed` (that transcript), `otherChallenge` (a challenge of the runner's own in its place) or `otherCertificate` (another certificate's SHA-256 in place of the station's). An app's runner fills none: in a fixture for the app only `"$device:signed"` appears, in a behaviour step, and it is checked (section 16.3) |

A number the station's DSP measures is written `"$within:<t>:<v>"`, with
the tolerance stated, never `"$any"`; a counter whose value depends on
timing is `"$int"`, not pinned.

**JSON inside a string.** Where the station sends a string that holds
JSON (a `utf8` property such as `connectedDevices`' `listJson`), a
fixture may write `{"$json": <expectation>}` in the string's place: an
object whose one key is `"$json"`. It matches a string that parses as
exactly one JSON value (RFC 8259: an object, an array, a string, a
number, `true`, `false` or `null`, with whitespace around it allowed),
and that value must match `<expectation>` by this section's rules: the
placeholders above stand inside it, a `{"$json": ...}` may nest inside
it, object keys match in any order and arrays in order and length. A
name recorded inside it is recorded for the whole fixture run, so a
`"$ref:<name>"` inside or outside a string refers to it. A value that is
not a string fails as a wrong type; a string that does not parse, or
holds nothing or more than one value, fails with the fixture, the step,
the path and "not JSON" (the station's runner reports
`<fixture>: step <n> (station <type>): <path>: not JSON (<why>), got
<the string>`); an object holding `"$json"` beside another key is a
malformed fixture. The path inside the string is the string's path
followed by `($json)`, as `$.properties[0].value($json)[1].state`.
`{"$json": ...}` stands only in a station message: in a client message
it is a malformed fixture, and neither runner fills one there. In a
fixture for the app, its expectation holds only what a station message
there may hold (section 16.3), and an app's runner sends it to its
client as the string of the expectation filled as that section fills a
station message, written as compact JSON (no whitespace, keys in any
order).

Matching is by value: objects must have the same keys and arrays the same
length; numbers compare by value, so `1` and `1.0` are equal. A name is
recorded once per fixture run; a later placeholder with the same name
records it again. `token` is recorded before the first step (section
16.3). Where a message is sent rather than matched, "filled" is the value
the sender puts in the placeholder's place.

**Who fills what.** Only the station's runner fills a client message's
placeholders: it sends every client step itself. Its counter starts at 1
in each fixture and advances once for each `"$int:<name>"` it fills, in
step order. An app's runner fills no client placeholder: in a behaviour
step it records what its client chose under each name, and in a fixture
for the app a scripted step holds no placeholder except `"$ref:token"`
(checked), so it has nothing to fill. Each runner therefore records every
name the fixture uses, the station's runner from its counter and fills,
an app's runner from its client; the values can differ between the two
runs, and the station messages that follow refer to them only through
`"$ref:<name>"`, which each runner resolves from its own record.

### 16.2 Control fixtures

Each end decodes `wire`; when `decodes` is true it encodes the result
again and the two JSON objects compare equal after parsing, key order
ignored. The fixtures cover every message kind in each direction it
travels: thirteen from the client (`hello`, `auth.request`, `command.invoke`,
`media.control`, `property.write`, `settings.write`, `settings.remove`,
`pair.start`, `pair.spake`, `pair.confirm`, `pair.fail`, and `path.join`
and `path.switch`, section 21.2) and
twenty-three from the station (the sixteen before pairing, `pair.accept`,
`pair.spake`, `pair.confirm`, `pair.fail`, `confirm.request` and
`notice`, section 7.5, and `path.switch`), with a `delta` carrying `"nan"` and `"-inf"`
(section 4.2). The client's `hello` has two fixtures: an older app's,
without `majors` or `features`, and one declaring both; `auth.request` has
three: a token, a device sign-in with its `device` block (section 3.5), and
one whose block carries `shortName`.
The station's `hello` carries `majors`, `features` (`deviceAuth` 1 and
`pairing` 1),
`identity` and `challenge`, from a station supporting `[1, 2]`, so `major`
is 1, the oldest (section 6.1). `auth.result` and `session.end` each have a
second fixture carrying a `code` (section 12.4). The refusals are: a
`media.control` over its 128 KiB cap and a `station.metrics.v1` over its
16 KiB cap (each an otherwise valid message padded past the cap), a
missing required key (`command.invoke` without `id`), a wrong type (an
`f64` entry holding `true`), an `f64` string other than the three of
section 4.2, a `hello` major above 65535, a `hello` with an empty `majors`,
a `hello` declaring a feature version that is not a whole number, a
`hello` whose `identity` is not an object, an `auth.request` whose `device`
lacks `signature`, one whose `shortName` is not a string, a `session.end` with an empty `code`, a `pair.start`
whose `mode` is neither `lan` nor `code`, a `pair.spake` step outside 0 to
3, a `pair.fail` whose `retryAfterMs` is not a whole number, a `notice`
whose `takeBack` is not a boolean, a `path.join` whose `ticket` is not a
string, is empty or is longer than 128 characters (`kMaxPathTicketChars`),
and an unknown `type`. The pairing fixtures carry placeholders for keys, shares
and boxes; the live exchange is proved by `nereus_pairing_peer` (section
3.6).

The two transport caps (1 MiB into the station, 8 MiB into the desktop
client, section 12.3) are enforced by the WebSocket layer before any
message is decoded, so a decoder fixture cannot hold them; `surface.json`
records them under `limits`.

### 16.3 Session fixtures

A session fixture is a script of one connection between a station and a
client. Two runners play it, one from each end.

- **The station's runner** builds the station `stationSetup` describes,
  opens a client connection to it over an in-process transport, and walks
  the steps. It sends each client message, placeholders filled (section
  16.1), whatever its role, and matches each station message against the
  next one the station sent, in arrival order. It runs every fixture.
- **An app's runner** runs the fixtures whose `runs` names `"app"`. It
  plays the station: it gives its client each station message, filled as
  below, and holds the client to the client steps by role. It ignores
  `stationSetup`, which only says how the station's runner builds its
  station.

**Roles.** Every client step has a `role`:

- `"behaviour"`: the client under test must produce this message. An
  app's runner drives its client to it (connect, invoke this verb with
  these arguments, write this property) and matches what the client sends
  against the step, placeholders allowed; the placeholders' names record
  what the client chose (its ids, its `origin`), and the station messages
  that follow refer to them with `$ref`. The client must send each
  behaviour message in order and nothing else while the steps run; a
  runner may set its client up to do nothing on its own beyond the
  connect sequence.
  A behaviour step's arguments and property values are literals, never
  placeholders: they are what the runner tells its client to send.
- **What the runner drives.** An app's runner drives its client's link
  layer (the part that sends and receives link messages), not its screens.
  That layer must send what it is told whatever the mirrored properties
  say: the summarised stand-ins (below) give, for example, a `pureSignal`
  object with `canActuate` false, and a link layer that refused to send a
  PureSignal verb on that account would fail behaviour steps a real
  station would answer. Deciding what to offer the operator from those
  properties belongs above the link layer.
- `"scripted"`: a message a conformant client never sends (an unknown
  kind or verb, a verb with an argument renamed, a write to a property the
  station sets itself or to one that does not exist, a write without a
  `writeId`, an operator-local setting, an older app's `hello`). An app's
  runner does not wait for its client and sends nothing for the step; it
  goes on, and the station's answers that follow are given to the client
  as if the client had sent the message. They hold the client to handling
  an answer to something it did not send. In a fixture for the app a
  scripted message holds no placeholder but `"$ref:token"`, and its
  `id` or `writeId` is a literal from 1000 up; an app's runner keeps its
  client's own ids below 1000 while a fixture runs, so an answer to a
  scripted message never carries an id the client used.

**Several clients** (iPhone app plan Task 71). A fixture may play other
clients beside its own, each signing in as a paired device:
`stationSetup.otherClients` is `[{"name": "b", "device": 1, "features":
{...}, "shortName": "..."}]`, where `device` is `n` (the paired device
`"$ref:device:<n>"` names, so `otherPairedDevices` pairs at least `n`) or
`"self"` (the runner's own device), `features` is what that client's
`hello` declares and `shortName`, when given, what its sign-in carries.
A client step may carry `"client": "<name>"` and a station step
`"to": "<name>"`; absent means the fixture's own client. Two more steps:
`{"connect": "<name>"}` runs that client's whole connect sequence (its
`hello`, its sign-in, and its messages up to `snapshot.complete`, taken
without matching), and `{"close": "<name>"}` closes its connection;
`expectClosed` may carry `"client"` too. The station's messages are
matched per client, in that client's own arrival order. `otherConnections`
stays for sockets that never sign in. With `stationSetup.deferOwnConnection`
true the fixture's own client is not connected before the first step: a
step `{"openOwnConnection": true}`, exactly once, connects it, and its
sign-in deadline starts there, so a fixture can pass more virtual time than
that deadline before its own client signs in; no step of the own client may
come before it, and such a fixture runs on the station alone. **An app's runner** plays only its
own client: it skips other clients' steps and the station messages sent
to them, and a `connect` or `close` step names nothing it plays. A fixture
where the own client shares the Core names the features its `hello`
declares as a literal (`{"deviceAuth": 1, "sessionHolder": 1}`), not
`"$object"`, since what the station sends it depends on them; an app's
client declares them.

**The radio's own PTT** (iPhone app plan Task 77). A step `{"radioPtt":
true}` or `{"radioPtt": false}` sets the level of the station radio's own
PTT input (its microphone or a footswitch), as its status frames report it;
the station's runner presses or releases it and lets the station act. A
fixture with such a step runs on the station alone (`"runs":
["station"]`); an app has no radio to press. `stationSetup.unkeyWalkMs`
(0 to 5000, with `transmitReady`) makes the radio's walk from transmit
back to receive take that many real milliseconds, so a transfer that
unkeys a holder is still running when the next message arrives. A verb
whose arguments are all optional (`tx.take`) may be sent with none.

**The Core's addresses** (the phone's direct addresses).
`stationSetup.coreListener` `{"address", "port"}` and
`stationSetup.coreInterfaces` put the Core's control listener and its
interfaces in place of the runner's computer's, before the client
connects; the address is read as `remote_bind` is (`"::"` is every
address, both families). Each interface address is `{"ip"}` with, if
any, a whole-number `prefix`, `temporary` true for a privacy address and
`deprecated` true for a renumbered prefix's. A step `{"coreInterfaces":
[...]}` replaces them (a renumbering), and the station reads them again
as its 5 s timer would. A fixture with such a step runs on the station
alone.

**Which fixtures run on the app.** A fixture whose client behaviour no
app can adopt runs on the station only (`"runs": ["station"]`): an older
app's `hello` (`major-refused`, `lower-minor`), made-up majors or features
(`version-*`), a client that answers no ping (`heartbeat-missed`) or never
sends its token (`connect-deadline`), the lockout, which needs other
clients (`lockout`), an older window meeting a full Core (`older-window`)
and a device's own connection replaced (`same-device-again`), and the device proofs
that fail (`device-other-challenge`, `device-other-certificate`), which
hold the Core's verification to a block a conformant client never sends.
Their client steps are all `scripted`. The device sign-in fixtures where
the app is the thing under test run on both ends: `device-sign-in` (the
app's device signs this connection's transcript and is admitted),
`device-not-paired` (the app's well-formed sign-in is refused
`deviceNotPaired` and the app handles the refusal and its code) and
`pairing-required` (the app's token sign-in is refused `pairingRequired`,
handled the same way). `tst_link_conformance_session` checks that a fixture
marked for the app holds nothing a conformant client could not send, and
nothing an app's runner could not send its client:

- its `hello` and `auth.request` are behaviour: the `hello` has `majors`
  `"$majors"` (an app supporting its own major and the one before sends
  both, section 6.1) and `features` `"$object"`, with `peer`
  `"$string"` and `settingsSchema` `"$int"`; the token is `"$ref:token"`
  or, for a refused token, `"$string"`; a device sign-in is `token` `""`
  with `device` `"$device:signed"`, after a station `hello` whose
  `challenge` is `"$capture:challenge"`;
- a behaviour `command.invoke` has `id` `"$int:<name>:1:4294967295"` for
  the `nnr.*`, `ps3.*` and `dspAssets.*` verbs and
  `"$int:<name>:0:4294967295"` for the rest (section 9.1), a verb from
  `commands` with exactly its arguments, names and kinds in order, all
  literal, and its gating capability advertised in the last
  `capabilities` message before it at the version `commands` gives (at
  least that version; exactly it for `psAlgorithmVersion`, section 6.2),
  at an agreed minor of at least the verb's `minMinor`;
- a behaviour `property.write` has `writeId` `"$int:<name>:1:4294967295"`
  (a `writeId` is never 0 on the wire) and writes
  only properties of the object's class (from `mirrorClasses`, the class
  named by the key's `object.create`) that are not outbound, with their
  ordinals and kinds;
- a behaviour `settings.write` or `settings.remove` names a station-scoped
  key (section 8), and a write's `origin` is `"$string:<name>"`;
- a behaviour message is of a kind a client sends (section 16.2);
- a behaviour `media.control`'s `connectionId` is the app's own:
  `"$uuid:<name>"` where it makes a connection, `"$ref:<name>"` after;
- a scripted message holds no placeholder but `"$ref:token"`, and a
  scripted `id` or `writeId` is a literal from 1000 up;
- a station message holds no `"$capture:<name>"` (except the station
  `hello`'s `challenge` as `"$capture:challenge"`), `"$uuid:<name>"`, `"$string:<name>"`,
  `"$int:<name>"` (ranged or not), `"$object"` or `"$majors"`, and
  `"$any"` only as a summarised snapshot (below); what an app's runner
  sends for each placeholder a station message may hold is defined
  below.

**Filling a station message (an app's runner).** An app's runner sends
its client each station message with `"$string"` as `""`, `"$int"` as `0`,
`"$ref:<name>"` as the recorded value, `"$within:<t>:<v>"` as `<v>` and
`{"$json": <expectation>}` as the compact JSON text of `<expectation>`
filled by these same rules (section 16.1),
except in the station `hello`:

- `identity` is a test station identity the runner makes at run time (a
  P-256 key, section 3.4): `publicKey` is its key and `certBinding` its
  binding of the certificate SHA-256 the runner's transport reports to its
  client for this connection, so a client that checks the binding before
  signing finds it good.
- `challenge` is 32 random bytes of the runner's own, base64url, new for
  each run; written `"$capture:challenge"`, it is also recorded as
  `challenge`.

**The app's device (an app's runner).** In a behaviour `auth.request`,
`"$device:signed"` is a check, not a fill: the block the app's client
sent must be well formed (section 3.5; a `shortName`, when present, a
usable short name), its `id` the fingerprint of its
`publicKey`, a canonical P-256 key, and its `signature` must verify over
this connection's transcript: the challenge recorded as `challenge`, the
certificate SHA-256 the runner's transport reported, the test station
identity's key and the block's own key. The runner accepts the app's key
as paired: `stationSetup`'s `pairedDevice`, like the rest of
`stationSetup`, means nothing to it, and the station messages that follow
say whether the sign-in was admitted. For the station's runner,
`"$device:<case>"` keeps its fill (section 16.1), whatever the step's
role.

- **The token.** `"$ref:token"` in a client message is the station's
  token. The station's runner reads it from the station at run time; an
  app's runner gives its client a token of its own choosing and records it
  as `token` before the first step. No fixture holds a token.
- **Time.** Time moves only through `{"advanceMs":N}`. The station's
  runner keeps a virtual clock over the station's own timers (the connect
  deadline, the heartbeat and the 50 ms delta flush) and fires each when
  its virtual time comes. A `delta` that waits for the flush follows an
  `{"advanceMs":50}` step. An app's runner moves its client's own clock by
  the same amount, if the client keeps one. No runner sleeps.
- **Closing.** `{"expectClosed":{"retryable":R}}` holds, on the station,
  when the station has closed the connection, every message it sent before
  closing is listed in the steps, and `R` equals `retryable` of the last
  `session.end` or `auth.result` it sent. An authentication refusal sends
  `auth.result` and closes with no `session.end` (section 12.4). An app's
  runner closes the connection there (close code 1000); the client must
  send nothing after it, and must not reconnect on its own when `R` is
  false.
- **The end.** A fixture that does not end in `expectClosed` ends with the
  connection open. Messages the station sends after the last step are not
  checked: on `connect-connectable` the radio keeps sending deltas.
- **The heartbeat.** Pings and pongs are WebSocket frames, not messages
  (section 12.1), so no step holds one. The in-process transport answers a
  ping the way a WebSocket stack does. `heartbeat-answered` passes three
  heartbeat intervals with the link up; `heartbeat-missed` has a client
  that never answers, and the station ends the session on the third
  interval, `retryable` true (section 12.1).
- **Summarised snapshots.** Outside `connect-connectable` and the two
  catalogue fixtures, whose `catalog` object is in full (and where a
  fixture needs a value from it, as `verbs-nnr` needs `dspAssets`'
  revision), a `schema` message's `fields` and an `object.create`
  message's `properties` are `"$any"`: `connect-connectable` and
  `surface.json`'s `mirrorClasses` hold their content. An app's runner
  sends, in their place, the class's properties from `mirrorClasses`: for
  `fields`, each as `{"ordinal","name","kind"}`; for `properties`, each as
  `{"kind","name","ordinal","value"}` with this stand-in value by kind:
  `bool` `false`, `i64` `0`, `f64` `0`, `utf8` `""`, and `enum` the first
  of its `enumValues`. The client must accept the stand-ins; the steps that
  follow do not depend on them.

`stationSetup` holds:

| Key | Meaning | Default |
| --- | --- | --- |
| `board` | the static radio's model: `"hermesLite2"`, a Hermes Lite 2 on Protocol 1, or `"ananG2"`, an ANAN-G2 on Protocol 2 (same MAC); only with `"radio": "static"` | `"hermesLite2"` |
| `radio` | `"static"`: a model reporting a connected Hermes Lite 2 (MAC `AA:BB:CC:DD:EE:01`, or the `board` below) with no radio behind it, so nothing changes on its own; `"connectable"`: a model connected to the fake Protocol 1 radio, receive processing and all; a receive-only station permits PureSignal (`transmitSettingsVersion` 7), so the runner waits for PureSignal's readiness (`canActuate`) to come on, and the fake radio does not stream on its own (one frame every 500 ms keeps the link alive, far short of a receiver block): before the client attaches the runner feeds it until the receiver has measured its first block (each slice's ADC peak leaves the no-reading value), so the readings hold to that block until the fixture ends | `"static"` |
| `slices` | slices before the client connects | 1 |
| `panadapters` | panadapters before the client connects | 0 |
| `coreAccessories` | the Core owns its accessories: the `tuner`, `amplifier`, `rfkit`, `accessoryData` and `accessorySettings` objects and the accessory commands | false |
| `stationTci` | the Core runs a station TCI server (it stays off) | false |
| `stepAttenuator` | a step attenuator controller is bound, so the radio hardware objects are offered | false |
| `media` | media is enabled | false |
| `mediaController` | with `media`, the Core's media controllers run as `nereusd` runs them (one per session, made as its media starts), so a client's `media.control` is answered; each media connection's transport is a stand-in that offers one description (`"v=0"` alone, matched as `"$string"`) and never sends a candidate or connects, so the Core's media control traffic is the same on every computer. Without it a `media.control` reaches nothing | false |
| `priorFailedAuthentications` | other clients that each sent a wrong token before this one connects | 0 |
| `clientAnswersPings` | the client's transport answers the station's pings | true |
| `otherClients` | other clients the runner plays beside its own, each signing in as a paired device (above) | none |
| `otherConnections` | other clients connected before this one, still connecting and sending nothing; 24 puts the station at its connection limit | 0 |
| `lanScanWindowMs` | how long the Core's `scanTgxlLan` and `scanPgxlLan` listen, in milliseconds from 1; the scan fixtures set 150, since their answer is matched as any text | 3000 |
| `token` | `"active"`: a Core upgraded from before paired devices, with a pairing token (made at run time) that `"$ref:token"` names; `"none"`: a new Core, without one (section 3.3) | `"active"` |
| `receivers` | iPhone app plan Task 74: the static radio's receivers, 192 kHz wide each, sized before its slices are made, so slices bind to them and the rules of section 7.5 apply; only with `"radio": "static"` | none |
| `maxSlices` | with `receivers`, the slice cap | 5 |
| `alexRxAntennas` | iPhone app plan Task 75: the static radio's receive antenna per band, 14 numbers 1 to 3 in `Band` order (160 m to XVTR), and band tracking on (the per-band antenna switch of section 7.6) as though a radio were connected; only with `"radio": "static"` | none (no band tracking) |
| `otherPairedDevices` | that many devices besides the runner's own are paired before the client connects, their keys made at run time and never written in a fixture; their ids are `"$ref:device:1"` onwards; an app's runner ignores it | 0 |
| `coreListener`, `coreInterfaces` | the phone's direct addresses: the Core's control listener (`{"address", "port"}`, the address read as `remote_bind` is) and its interfaces (above), in place of the runner's computer's; an app's runner ignores them | the Core does not listen: `coreAddresses` is `{"addresses":[]}` |
| `deferOwnConnection` | the fixture's own client connects at its `openOwnConnection` step instead of before the first step (above); station fixtures only | false |
| `pairedDevice` | the station runner's own device (its key made at run time, the one `"$device:<case>"` signs with) is paired with the station before the client connects; an app's runner ignores it, as it ignores all of `stationSetup`, and accepts its app's key | false |

The station runner starts every fixture from an empty settings profile,
and the bundled NR3 model files count as absent, so a fixture reads the
same on every machine.

| Fixture | What it holds the station to |
| --- | --- |
| `connect-connectable` | The whole connect sequence to `snapshot.complete` on a connected radio with one slice, every message in full, except: PureSignal's `statusJson` (`"$string"`, it carries a capture time) and `displayGeneration` (`"$int"`, a counter whose value depends on timing), the catalogue's `json` (`"$string"`, held in full by the catalogue fixtures), and the slice's live meter readings, which the receiver measures: `signalStrengthDbm`, `signalPeakDbm`, `signalAverageDbm`, `adcPeakDbfs`, `adcAverageDbfs`, `agcGainDb`, `agcPeakDb` and `agcAverageDb`, each `"$within:0.5:<v>"`, within 0.5 of the receiver's first measured block of the fake radio's tone (the snapshot carries the live readings, never the no-reading value of -400 or the -399.02 an unmeasured receiver shows) |
| `wrong-token` | `auth.result` refused, `retryable` false, `code` `wrongToken`, then the close |
| `pairing-required` | On a new Core (no token), a token sign-in is refused "This Core uses paired devices. Pair this device first.", `retryable` false, `code` `pairingRequired`; on the app, the app handles that refusal and does not reconnect |
| `device-sign-in` | On a new Core, the paired device signs this connection's transcript (`"$capture:challenge"` from the `hello`, then `"$device:signed"`) and is admitted: the whole connect sequence to `snapshot.complete`, summarised; on the app, the app's own device signs and its block is checked |
| `device-not-paired` | A well-formed device sign-in from a key the Core has not paired: `code` `deviceNotPaired`, then the close; on the app, the app's device signs, its block is checked, and the app handles the refusal |
| `device-other-challenge`, `device-other-certificate` | The paired device signs another challenge, or another certificate: `code` `deviceProofFailed`, then the close |
| `devices` | On a new Core with the runner's device and two others paired, a device that declares `deviceAuth` receives the `devices` object in its snapshot (`listJson` and `keyPath` as `"$string"`, since they carry run-time ids, pairing and sign-in times and a path); a rename with a renamed argument and with a label outside the rule is refused, then a rename is stored (`settings.value` `StationLabel`) and the object's next `delta` carries it; raw `settings.write` and `settings.remove` of `StationLabel` are refused; the key backup is acknowledged; another device is revoked; `station.retireToken` with no token is accepted; then the device revokes itself: `command.result`, `session.end` `deviceRemoved`, the close. Runs on the station alone |
| `devices-not-offered` | A window at minor 11 that declares no features receives no `devices` object, and `station.rename` is refused "Update this app to manage this Core's paired devices." Runs on the station alone |
| `devices-pairing` | On the same Core as `devices`, `pairing.open` and `pairing.close` each with a renamed argument are refused; `pairing.open` is accepted with `values` `code` as `"$string"`, and the object's next `delta` has `pairingWindowOpen` true and `pairingCode` `"$string"`; `pairing.close` is accepted and the next `delta` has them false and `""`. Runs on the station alone |
| `core-addresses` | On a new Core listening on every address at port 47910, with a link-local, a unique local, a stable global and a temporary IPv6 address and a private and a public IPv4 address, a device signed in with its own key that declares `deviceAuth` and `coreAddresses` gets `coreAddressesVersion` 1 and a `devices` object whose `coreAddresses` lists the stable IPv6 address and the public IPv4 address alone, each with the port; its interfaces renumber (the old IPv6 prefix deprecated, a new one added) and the next `delta` on `devices` carries `coreAddresses` alone, with the new IPv6 address; a write of it is refused. Runs on the station alone |
| `core-addresses-token` | On an upgraded Core with the same interfaces, a window signed in with the token that declares `deviceAuth` and `coreAddresses` gets neither `coreAddressesVersion` nor `coreAddresses` (not in the schema, not in the object); the renumbering sends it nothing, so the next messages are a rename's own answer, and the `devices` delta after the rename carries no `coreAddresses`. Runs on the station alone |
| `devices-retire-token-refused`, `devices-retire-token` | On an upgraded Core, a token connection that declares `deviceAuth` receives the object with `tokenActive` true; `station.retireToken` is refused with no device paired, and with one paired it is accepted and the connection ends: `session.end` `pairingRequired`, `retryable` false. Run on the station alone |
| `catalog-anan-g2`, `catalog-hermes-lite-2` | The connect sequence to `snapshot.complete` on the static radio as an ANAN-G2 and as a Hermes Lite 2: the capabilities in full, and the `catalog` object with its `json` in full and `revision` 1 (section 7.4). The two differ exactly where the radios do: the board's model, name, attenuator (0 to 31 against -28 to 31), sample rates (six against four), antennas (three plus three receive-only against one plus none), PA rating and microphone input, transmit ranges (`board.transmit`: the HL2's power and tune in dB) and relays (the G2's Ext 1 and Ext 2 on TX), and the RF power gauge its rating scales; `display` and `noiseReduction` are the same on both, and neither has the RX1 preamp. On a Core that has not changed its plan, ARRL (US) is both `default` and `active`, and every plan carries its file's `spots` |
| `settings-band-plan` | Two devices on a new Core: the fixture's device writes `BandPlanName` "IARU Region 1", and both devices get `settings.value` for it, then one catalogue `delta` each (`revision` 2) whose `json` marks `iaru-region1` alone `active` (`default` stays on ARRL (US)); a write of a plan the Core does not have gets `settings.reject` with the Core's value "IARU Region 1" and "This Core does not have that band plan.", to the writer alone; a `settings.remove` reaches both devices as an absent value, and the next `delta` (`revision` 3) marks ARRL (US) `active` again. The writing device's two deltas carry the catalogue's `json` in full; the other device's carry `active` and `default` for each plan and `"$any"` for the rest |
| `monitor-audio` | The transmit monitor (the media document's "Transmit monitor (monitor-audio)"), with `mediaController`: a media `start` declaring `txMonitorAudioVersion` 1 and `headphonesMixVersion` 1 is answered with the Core's `description` (`type` `offer`, `sdp` as `"$string"`); `monitor-audio` `route` `headphones` is answered with `monitor-audio-context` `route` `headphones` and the request's `revision`, and `route` `none` with `route` `none`. A second `start` on a new connection, declaring `txMonitorAudioVersion` without `headphonesMixVersion`, gets its own `description`, and `route` `headphones` is answered `route` `speakers` (its main stream carries MON). Connection ids are `"$uuid:<name>"`, revisions `"$int:<name>:1:4294967295"`. Runs on the station and the app |
| `monitor-audio-undeclared` | With `mediaController`, a `start` without `txMonitorAudioVersion` is answered with its `description`; a `monitor-audio` (scripted: a conformant client does not send it) gets no answer: the next message the Core sends is the `command.result` of the `ps3.subscribeDisplay` sent after it. Runs on the station alone |
| `connection-limit` | With twenty-four other connections still connecting, the station sends no `hello`: `session.end` "The Core already has as many connections as it allows. Try again shortly.", `retryable` true, then the close |
| `lockout` | After five wrong tokens from other clients, the right token is refused as rate limited, `retryable` true |
| `major-refused` | An older app's `hello` (no `majors`) with major 2 gets `session.end` "This Core runs link version 1 and this app runs version 2. Update the Core.", `retryable` false, `code` `linkVersion` |
| `version-declares` | A `hello` with `majors` `[1]` and a declared feature is accepted, and authentication follows |
| `version-app-one-ahead` | An app supporting `[1, 2]` chooses 1, the highest it shares with the station, and is accepted |
| `version-app-two-ahead` | An app supporting `[2, 3]` that sends major 3 gets `session.end` "This Core runs link version 1 and this app runs version 3. Update the Core.", `retryable` false, `code` `linkVersion` |
| `lower-minor` | A `hello` with minor 4 agrees minor 4: the capabilities without the minor-11 entries, and a minor-11 verb refused with a plain reason |
| `same-device-again` | The device signs in again on another connection (`otherClients` `"self"`): the older connection ends with `session.end` "This device connected again.", `retryable` false, `code` `sameDevice`, and the newer one is let in with no question. Runs on the station alone |
| `place-freed` | The device (`otherClients` `"self"`) signs in and drops; its 3 minutes run out, freeing its place; four other devices fill the Core; 20 s later the device's own connection opens (`deferOwnConnection`) and signs in: it is let through `auth.result` and sent `session.held` with `placeFreed` `{secondsAgo: 20}`. Runs on the station alone |
| `older-window` | A window that signs in by key but predates several devices is let in first and owns the Core's slice; when a device with the feature is let in with a slice of its own, the older window is sent no marker for it, only the `devices` list moving. Four devices then fill the Core; a window from before paired devices (the token, no features) is let through `auth.result` and then turned away: `session.end` "The Core is full. Update NereusSDR to take a device's place, or try again later.", `retryable` true, no code. Runs on the station alone |
| `connected-devices` | A device that declares `sessionHolder` receives `connectedDevices` in its snapshot (`deviceLimit` 4, `revision` 1), and a `delta` of it (with one of `devices`) each time another device is let in, including a window that declares only `deviceAuth` and so never receives the object itself, and when one drops; each device let in gets a slice of its own, which reaches this device as a marker; `listJson` is a `{"$json": ...}` of the list's shape (section 16.1): each entry's keys with its literal name, short name, kind, flags, state and `listeningOn` (`{sliceId, letter, band, mode}`), its `deviceId` `"$string"` and its three durations `"$int"`, since ids are made at run time and a fixture for the app holds no capture |
| `short-name` | Another device signs in with a short name, drops, and signs in again with a new one: each change moves `connectedDevices`' and `devices`' revisions, the new short name replacing the old in both lists and on its slice's marker |
| `grace-return` | Another device drops and is away; a minute later nothing has been sent about it but its slice's marker turning `ownerAway`; it signs in again within its 3 minutes and is let in with no question, the list sent again and its marker back; it drops again, and when its 3 minutes end with this device still on the Core its slice closes (the marker's `object.destroy`) and is saved for its return |
| `two-devices` | Another device holds the Core's slice; this device is let in with a slice of its own (`slice:1`) and the other's as `marker:0`, naming its owner, with the `SliceMarker` schema; the other device is sent `marker:1` for this device's slice, and when this device tunes its slice the other sees the marker move, never the slice |
| `slice-access` | This device declares `sliceAccess` with `sessionHolder` (slice control plan Task 4): its capabilities end with `sliceAccessVersion` 1, its burst carries the `SliceAccess` schema and `access:0` (the incarnation and control revision recorded, controller and only listener this device, its active receive slice, not transmitting). `slice.listen` on its own slice is accepted and changes nothing, returning the revision; `slice.stopListening` is refused "You control slice A. Use Release to leave it."; `slice.takeControl` with the revision seen is accepted with no change; `slice.release` with another revision is refused "Someone else changed who controls slice A. Look again and try once more."; each verb with a renamed argument is refused "The Core could not read this request.". Runs on the station alone |
| `foreign-write-refused` | This device holds slice 0 and another device slice 1: a `property.write` to `slice:1` and to `marker:1`, and `removeSlice` and `setActiveSliceById` naming slice 1, are refused "That slice belongs to Other device 1. It can be changed only there.", with no value sent back and nothing changed; its own slice it may make active |
| `held-for-device` | Another device, alone on the Core, leaves with `session.leave`: nobody else is on its slice, so the slice closes and is saved for it (the Core has no slice). This device, let in meanwhile, gets a slice of its own; the other device signs in again and its saved slice is restored under the lowest free letter, which this device sees as a marker with `ownerAway` false |
| `verbs-tx-set-tx-slice` | On a Core with `remote_transmit` allow (stationSetup `remoteTransmit`), a device that declares `remoteTx` is sent `txPermitted` false in its first `capabilities` and true in the `capabilities` sent again after `snapshot.complete`, each with `remoteTxVersion` 1 and the `txRefusal` entries (`notReady` first, empty once permitted); `tx.setTxSlice` with an argument it does not take is refused "The Core could not read this request."; with `sliceId` while nobody holds transmit it is refused "Take transmit on this device first." with the values `refusalCode` `notHolder` and `refusalFix` `takeTransmit`. Runs on the station alone |
| `unheld-key` | On a Core with `remote_transmit` allow whose radio can key (stationSetup `transmitReady`) and whose device's media carries a microphone line (stationSetup `microphoneLine`), one device that declares `remoteTx`: each keying verb with an argument it does not take is refused "The Core could not read this request."; a program's `tx.key {trigger:"tci"}` on unheld transmit is refused `programNeedsTransmit` and nobody takes transmit; a person's `tx.key {trigger:"screen"}` takes it and keys (epoch 1; `transmitting` true, the device's entry transmitting); `tx.unkey {epoch:1}` unkeys; the same program's key then keys (epoch 2) and `tx.unkey {epoch:2}` unkeys; `tx.tune {on:true}` tunes (epoch 3, `transmit`'s `tune` true) and `{on:false}` ends it; `tx.twoTone {on:true}` on a radio with no transmit channel is refused "The two-tone test could not start on the Core." Each key and unkey also moves `txState` (section 18.8): `keyed`, who keyed and how (`keyedTrigger` `screen`, `tci`, `tune`), `keyedSinceMs` on the runner's virtual clock and 180 s left for the phone. Runs on the station and the app |
| `key-without-microphone` | On the same Core as `unheld-key` but with no microphone line for the device (stationSetup `microphoneLine` absent): a person's `tx.key {trigger:"screen"}` is refused `micNotReady`, "This device's microphone is not connected to the Core. Wait a moment and try again.", and nothing keys; a program's key on unheld transmit is still refused `programNeedsTransmit` first. Runs on the station and the app |
| `tx-keepalive` | On the same Core as `unheld-key`: `tx.keepalive` with an argument it does not take is refused "The Core could not read this request."; a keepalive while nothing of the device's is watched is accepted and changes nothing; the device keys (epoch 1) and sends keepalives 50 ms after the key and then 300 ms apart (sequences 2 to 4, epoch 1), each accepted, and the key stays on; then none for 450 ms: the watchdog stops transmitting (`transmitting` false, the device's entry no longer transmitting). `txState` follows the key (180 s left, 179 a second later), and the watchdog's stop is its stop: `stopReason` `linkLost`, `stopSerial` 1, "The link to Conformance device went quiet, so the Core stopped transmitting." Runs on the station and the app |
| `grace-transmit-held` | Two devices that declare `remoteTx`: the other device keys and this one's permission goes false (`capabilities` sent again) and its `tx.key` is refused "Other device 1 has the transmitter."; the holder's link drops: this device is sent `capabilities` again with the refusal "Transmit is changing hands. Try again in a moment." and then "Other device 1 has the transmitter.", the Core stops transmitting at once (`transmitting` false) and the holder, away, still holds transmit (this device's `tx.key` is refused naming it); the holder signs in again a minute later, nothing keys until its own `tx.key` (epoch 2), and its `tx.unkey {epoch:2}` unkeys. `txState` follows each key, and the holder's dropped link is its stop: `stopReason` `linkLost`, `stopSerial` 1, "The link to Other device 1 went quiet, so the Core stopped transmitting." Runs on the station and the app |
| `on-air-refusals` | While another device (short name "Tablet B") is keyed, this device's Protocol 1 rate change, `ps3.off`, its slice's `rxAntenna` and `transmit`'s `pureSig` are each refused "Tablet B is on the air. Try again when they stop." (commands with `refusalCode` `holderOnAir` and `refusalFix` `takeTransmit`). Runs on the station alone |
| `take-transmit` | Two devices that declare `remoteTx` and `sessionHolder` (Task 77): the other device's `tx.take` on unheld transmit is taken at once (`holderEpoch` 1) and the transmit flag moves to its own slice (this device's `slice:0` `txSlice` false, the other's marker true); this device's `tx.take {}` is refused "Waiting for you to confirm." with `phase` `needsConfirmation` and a `confirm.request` `takeTransmit` whose `holder` names "Other device 1", `state` `listening`, `keyed` false; `confirm.proceed` takes it (`holderEpoch` 2, the flag back on `slice:0`) and the other device gets `transmitTaken` "Conformance device took transmit." with Take it back; this device's `tx.take` again is accepted and changes nothing; the other device's `tx.take {holderEpoch:2, shownKeyed:false}` takes at once. Runs on the station and the app |
| `take-transmit-keyed` | The other device takes transmit; this device's `tx.take` is asked (holder `listening`); the other device keys; this device's proceed is then refused "Waiting for you to confirm." and asked again, red (`state` `transmitting`, `keyed` true); the second proceed unkeys the other device before this one holds, unkeyed (`holderEpoch` 2, `transmitting` false, `stopReason` `takenOver`, "Conformance device took transmit, so the Core stopped transmitting."), and the other device is told. Runs on the station and the app |
| `transfer-refuses-keys` | With the unkey walk slowed (stationSetup `unkeyWalkMs` 300), the other device keys; this device's `tx.take {holderEpoch:1, shownKeyed:true}` starts the transfer; the other device's `tx.key` during it is refused `changingHands` "Transmit is changing hands. Try again in a moment."; the take's answer arrives when the transfer ends (`holderEpoch` 2). Runs on the station alone |
| `radio-ptt-takes-transmit` | This device keys; the radio's own PTT is pressed (step `radioPtt` true): transmit is taken without a question, this device is told (`transmitTaken`, `byName` "Radio", `bySource` `radioPtt`, `byKind` `station`), every device's `txState` names "Radio" with `holderSource` `radioPtt` and `stopReason` `takenOver`, and this device's `slice:0` shows `txSlice` false while the radio transmits on it; this device's `tx.take` is asked red (holder `source` `radioPtt`, `state` `transmitting`) and taken back; the PTT still held (its level again) takes nothing; this device keys (epoch 3) and the PTT's release unkeys nothing. Runs on the station alone |
| `tx-mark` | `txSlice` is true only on the holder's own transmit slice, on `slice:` and `marker:` alike: this device takes transmit (its `slice:0` true, the other's view of `marker:0` true); the other device takes it at once as shown (`tx.take {holderEpoch:1, shownKeyed:false}`) and each view hears the changed values (`slice:0` and `marker:0` false, the other's `slice:1` and this device's `marker:1` true). Runs on the station and the app |
| `take-during-grace` | The other device takes transmit and its link closes; 40 s later this device's `tx.take` is asked without red (holder `state` `away`, `awayForSeconds` 40, `keyed` false) and the proceed takes it, nothing to unkey; the other device signs in again 20 s later (client `back`), does not hold transmit, and its `transmitTaken` notice (`secondsAgo` 20, Take it back) follows its `snapshot.complete`. Runs on the station and the app |
| `unheld-key-carriers` | On unheld transmit, `ps3.twoTone {enabled:true}` on a radio with no transmit channel is refused "The two-tone test could not start on the Core." and `tx.tunerTune {on:true}` with no Tuner Genius "No Tuner Genius is connected to the Core.", and neither takes transmit (this device's `tx.key` then takes it at `holderEpoch` 1); `tx.take` and `tx.tunerTune` with an argument they do not take are refused "The Core could not read this request." Runs on the station and the app |
| `share-receiver` | Another device's slice shares this device's receiver (this device anchors it); a C-Tune move that would leave it outside is answered "Waiting for you to confirm." with `phase` `needsConfirmation`, then a `confirm.request` `panMove` naming the other device and its slice's effect `moves`; `confirm.proceed` with a renamed argument is refused, with its own arguments it is accepted; the other device's slice moves to the free receiver and it is told (`notice` `sliceMoved`, no Take it back) |
| `anchor-band-change` | The same two devices; this device retunes its slice from 20 m to 40 m: the write is answered "Waiting for you to confirm.", then `panMove` with `change` "Receiver 1", "20 m", "40 m"; `confirm.cancel` with a renamed argument is refused, with its own it changes nothing; the write again, `confirm.proceed`: its result carries `objectKey` and `frequency`, the receiver follows this device's slice and the other device's slice moves, the other device told |
| `non-anchor-pan-move` | The other device moves its panadapter on this device's receiver while this device holds the second receiver: refused, naming this device, and asked to take one (`takeReceiver`); it cancels; this device removes its second slice and the other device's move goes to the free receiver, nobody asked |
| `take-receiver` | Each device holds one receiver; this device's `addSliceOnPan` is refused naming the other and asked `takeReceiver` (its own receiver `takeable` false); proceed takes the other's receiver: the other device's slice closes and it is told `receiverTaken` with Take it back; `notice.takeBack` with a renamed argument is refused; with its own it asks the other way, and proceed closes this device's new slice (told `receiverTaken`) and restores the other's |
| `take-slice` | The slice cap (2) full with a receiver free: `addSlice` is refused and asked `takeSlice`; proceed closes the other device's slice, which is told `sliceTaken`. Runs on the station alone |
| `grace-expired` | Another device drops; after its 180 s its slice closes; it signs in again and `graceEnded` follows its `snapshot.complete`, `secondsAgo` from when its time ran out |
| `older-window-taken-over` | A window without the feature holds the second receiver; this device takes it: the window's session ends `takenOver`, not retryable. Runs on the station alone |
| `older-window-no-slice` | The slice cap full, a window without the feature signs in: `session.end` "All the radio's slices are in use. Try again when another device closes one.", retryable. Runs on the station alone |
| `shared-setting-confirm` | iPhone app plan Task 75, ruling 7.1a: another device's slice on the HL2's one ADC, which the receive antenna relay feeds (a change that asks first): the `alexAntennas` `useTxAntennaForRx` write is answered "Waiting for you to confirm." with the Core's value, then a `confirm.request` `sharedSetting` with `change` "Receive on the transmit antenna", "Off", "On" and `affected` naming the other device (`state` `listening`) and its slice's `mode`, `adc`, `streamIndex` and effect `changes`; `confirm.proceed` carries the readback (`objectKey`, `useTxAntennaForRx`), the other device is told (`notice` `settingChanged` with who, `change` and `secondsAgo`, no Take it back) and sent the `delta` |
| `shared-setting-notice` | Ruling 7.1a: the same two devices; this device's preamp (a change that applies at once): the `stepAtt` write is taken at once (`property.result` accepted, `preampMode` 1), nobody is asked, and the other device is told (`notice` `settingChanged`, "Preamp, ADC 1", "Off", "On") and sent the `delta`. Runs on the station alone |
| `confirm-grew` | The same question; before it is answered the other device opens a second slice on the ADC: `confirm.proceed` is answered "Waiting for you to confirm." with `phase` `needsConfirmation` and a new `confirm.request` naming both its slices, nothing applied; `confirm.cancel` of the new one. Runs on the station alone |
| `confirm-target-changed` | The same question; the other device makes the same receive antenna change (asked, it goes ahead, this device is told `settingChanged`); this device's `confirm.proceed` is refused "That setting changed since you asked. Make the change again.", nothing more applied. Runs on the station alone |
| `older-window-shared-setting` | A window without the feature makes the same receive antenna change while a device with the feature listens on the ADC: refused "This change would affect Other device 1. Update NereusSDR to confirm changes that affect other devices.", nothing applied, never asked. Runs on the station alone |
| `antenna-kept` | An ANAN-G2 with ANT2 on 40 m and ANT3 on 80 m (`alexRxAntennas`); the other device's slice listens on the ADC on another receiver; this device tunes from 20 m to 40 m: the tuning goes ahead and this device is told `antennaKept`, "The antenna stays on ANT1 while Tablet B listens on it.", no `by` keys; the other device leaves, its slice closes, and this device's next crossing (to 80 m) switches the antenna (its slice's `rxAntenna` becomes ANT3), with no notice. Runs on the station alone |
| `verbs-session-leave` | `session.leave` with an argument is refused, "The request to leave the Core was not understood."; without, it is accepted and the station closes the connection with no `session.end`. Runs on the station alone |
| `heartbeat-answered`, `heartbeat-missed` | The heartbeat, above |
| `path-ticket` | A signed-in session asks `session.pathTicket` and is given a `ticket` (`utf8`, any text) and `expiresInMs` 10000 (section 21.2; `controlSwitchVersion` 1) |
| `path-join-unknown-ticket` | A new connection's `hello` followed by `path.join` with a ticket no session holds gets `session.end` "The Core did not move the connection here.", `code` `protocolError`, `retryable` false, then the close. A move itself needs two connections, which a fixture does not script; `tst_session_transport_switch` holds the station and the desktop to it |
| `connect-deadline` | No `auth.request` within 30000 ms: `session.end` "This app did not finish connecting to the Core in time.", `retryable` true |
| `property-write` | A write and its `property.result` and side-effect `delta`; a refused outbound property and an unknown one; a write without a `writeId` answered by `delta`; a write to a slice's signal strength refused as outbound; on the receive-only Core, a `transmit` write of `power` taken off the air and a write of `mox` and `voxEnabled` refused with the receive-only reason; at `transmitSettingsVersion` 2, a write of `cpdrLevelDb` taken, and `micGainDb` and `monitorVolume` out of range refused with their ranges beside a write of the outbound `tunePowerForTxBand`; at `transmitSettingsVersion` 3, a write of `micBoost` and `lineInBoost` taken, and `lineInBoost` out of range refused with its range beside a write of the outbound `activeTxProfile`; at `transmitSettingsVersion` 4, a write of `txEqBandsJson`, `txEqUseLegacy` and `txLevelerDecay` taken, and a nine-value `txEqBandsJson`, a `cfcCompressionJson` with a value out of range and `txAlcDecay` out of range each refused whole with its range |
| `tx-eq-curve` | `txEqCurveVersion` 1 (the client declares `txEqCurve` 1): the capability after `accessoryTxVersion`, `txEqCurve` last in the `TransmitModel` schema and, in the `transmit` snapshot, the flat default curve (`state` `default`) for the static station's empty `txEqParaEqData`; a write of the worked example's `txEqParaEqData` (section 7.1, "The TX EQ curve") taken, then the side-effect `delta` carrying its `txEqCurve` (`state` `saved`); a write to `txEqCurve` refused as outbound, the curve unchanged |
| `tx-eq-set-curve` | `txEqCurveVersion` 2 (the client declares `txEqCurve` 2): the capability at 2; `txEq.setCurve` with the link document's worked example sent back as it was shown, but out of order and unrounded, taken: the side-effect `delta` carrying the new `txEqParaEqData` and the worked example's `txEqCurve`, then the accepted `command.result` whose `curve` is that `txEqCurve`; a four-point curve refused whole "Choose a curve of 5, 10 or 18 points." with nothing sent after; `txEq.resetCurve` taken, the `delta` and a `curve` of five flat points spread from 50 to 3000 Hz, preamp 0 |
| `cfc-set-profile` | `transmitSettingsVersion` 15 (the client declares `cfcProfile` 1): `cfcProfile` last in the `TransmitModel` schema at ordinal 88 and in the `transmit` snapshot; `cfc.setProfile` with a 5-band profile and the snapshot's revision taken: the side-effect `delta` carrying the new `cfcPostEqGainDb`, `cfcPrecompDb`, `cfcParaEqData` and `cfcProfile`, then the accepted `command.result` whose `profile` is the stored profile; the same profile sent again with the old revision refused "The CFC settings changed on the Core. Check the new values and try again." with nothing sent; a misnamed argument refused "The CFC settings were not understood." |
| `settings-write` | A station-scoped write echoed with its origin; an operator-local write rejected; a removal sent as `settings.value` with no entry; on the receive-only Core, a DSP > Options TX key and a PA forward-power table key (`paCalibration/calPoint1`, version 6) taken off the air, an OC transmit pin (`oc/tx/20m/pin3`), an OC pin action (`oc/actions/pin1/action`) and TX Display Cal (`cal/txDisplayOffset`) taken off the air (version 8), and a transmit hardware key refused |
| `unknown-verb` | `command.result` refused, "The Core does not know this request. Updating the Core may help."; the connection stays up |
| `unknown-kind` | `session.end` "The Core could not read a message from this app.", `retryable` false, `code` `protocolError` |
| `verbs-*` | Each verb in `commands`, grouped by the capability that gates it, invoked with its own arguments (for `setPgxlHardware`, one of its three optional ones) and, where it takes any, with one argument renamed (and nothing else changed: the same values and kinds); the two get different answers, so each shows the station read the arguments (a PureSignal action with arguments it does not take is refused "The Core could not read this PureSignal request." before the transmit gate is asked); `verbs-ps3` (which requires `psAlgorithmVersion` 3) invokes the PureSignal verbs whose answers do not depend on arming (`ps3.off`, `ps3.twoTone` off and `ps3.saveCorrection`), and `verbs-ps3-arming` (which also requires `transmitSettingsVersion` 7) invokes `ps3.twoTone` with `enabled` true, a key (section 18.6) refused on that receive-only Core "This Core is set to receive only." (`refusalCode` `stationReceiveOnly`), and the arming verbs (`ps3.single`, `ps3.automatic`, `ps3.applyCurrent`, `ps3.restoreCorrection`), taken and then failing on the static station, which has no PureSignal running ("PureSignal is unavailable until the radio is ready."); `nnr.applyModelSelection` names the revision `dspAssets` gave in the snapshot; `verbs-tgxl-control` (which requires `remoteTgxlControlVersion` 2) invokes the antenna, operate and bypass switches, and `verbs-tgxl-relays` (which requires `remoteTgxlControlVersion` 4) matches `scanTgxlLan`'s `devicesJson` as any text, since a real Tuner Genius on the test computer's network may answer, and its accepted `setTgxlAddress` is followed by the `tuner` delta carrying the saved address, then a blank host, saved and shown as blank, and the address again; `verbs-pgxl-control` (which requires `remotePgxlControlVersion` 4) does the same for `scanPgxlLan` and `setPgxlAddress` (the `amplifier` delta, and the blank host), and its `setPgxlOperate` is refused on the static station, which has no amp connected ("The Core is not connected to the Power Genius."); `verbs-rfkit` (which requires `remoteRfKitControlVersion` 2) connects, disconnects and switches the RF-Kit, and `verbs-rfkit-control` (which requires `remoteRfKitControlVersion` 4) invokes `setRfKitOperate`, `setRfKitAntenna` and `setRfKitTciMode`, refused on the static station with no amp admitted ("The Core is not connected to the RF-Kit amplifier."), and its accepted `setRfKitAddress` is followed by the `rfkit` delta carrying the saved address, then the blank host and the address again; `verbs-tx-antenna` (which requires `radioHardwareVersion` 6) invokes `setAlexTxAntenna` with a band's current antenna (taken, no delta) and with `band` renamed; `verbs-io-board` (which requires `radioHardwareVersion` 7) invokes `requestIoBoardI2c` and `setIoBoardOutput`, each refused on the static station, which has no radio connection to reach the I2C bus through ("The radio is not connected, so its I2C bus cannot be reached."), and each with one argument renamed; `verbs-radio-sample-rate` (which requires `radioHardwareVersion` 9, run by the station only) invokes `setRadioSampleRate` as a paired device, refused on the static station, which has no radio connection ("The radio is not connected, so its sample rate cannot change."), and with its argument renamed; `verbs-level-calibration` (which requires `radioHardwareVersion` 12, run by the station only) invokes `resetLevelCalibration` (taken) and with an argument it does not take (refused "The Core could not read this request."); `verbs-dsp-info` (which requires `dspInfoVersion` 1) invokes `dsp.filterResponse` with `highResolution` false (taken, `stepHz` 0 and an empty `magnitudesDbJson`), with `highResolution` true (refused on the static station, which runs no receiver channel: "The Core's receiver for this slice is not running.") and with one argument renamed; `verbs-records` (which requires `recordStreamVersion` 1) subscribes to `spots` with a backlog (taken, then the `record.batch` reset, empty on the static station), to a stream the Core does not keep (refused "The Core does not keep that list."), and with one argument renamed, then unsubscribes right and wrong; `verbs-station-radios` (which requires `stationRadiosVersion` 1, on a station set up with `stationRadios`: its static radio and a second "Bench G2" in sight, signed in as a paired device, since the Core refuses these verbs to a pairing-token sign-in) subscribes to `stationRadios`, sets the second radio's model (taken, then the record's upsert), refuses forgetting the Core's radio, rescans, refuses a radio it cannot see and takes the second radio, each with one argument renamed where it takes any; `station-radio-confirm` chooses the second radio with another device listening, is asked (`confirm.request`, `change` "Radio"), proceeds, and the other device is told (`notice`); `verbs-spots` (which requires `recordStreamVersion` 1) invokes `spots.connect` for the DX cluster with no callsign saved (refused "Enter your callsign in Spot Hub first."), `spots.disconnect` for POTA (taken: it is not running), `spots.sendCommand` to a cluster that is not connected (refused "The DX cluster is not connected."), each with one argument renamed, and `spots.clearAll` (taken; no station source is ever dialled); `verbs-support` (which requires `supportBundleVersion` 1) invokes `support.collect` (taken, its `bundle` matched as any text) and with an argument it does not take (refused "The Core could not read this request."), `support.setLogCategories` with one category and then with none (each taken, then the `radio` delta carrying `logCategories`) and with its argument renamed, and subscribes to `coreLog` with a backlog of 0 (taken, then an empty reset) and unsubscribes. A version's new verbs go in a fixture of their own, so an app at the older version still runs the older file |

`tst_link_conformance_session` also checks that every verb in the
`commands` table is invoked both ways by some fixture, and that the two
legs of each get different answers.

The several-devices design's `sharing-budget` fixture (a holder and another
device over the Core's total: the holder's share is its whole request, the
other's reason `sharedConnection`, or `sharedProcessing` under the
governor's cut) needs the transmit holder, so it lands with Task 34; until
then `tst_display_budget_split` and `tst_station_multi_session` hold each
device's share, generation and reason (section 6.4).

### 16.4 Media vectors

`tst_link_conformance_regen` writes every vector from the station's own
encoders and fixed inputs, into `NEREUS_LINK_REGEN_OUT` only. Each `.bin`
is one packet as it travels.

**Decoding a vector.** A runner decodes a vector on a fresh decoder. When
`expect` holds `"after": ["<fixture id>", ...]`, it first decodes each
named vector's bytes, in order, on that same decoder, without checking
their own expectations, then decodes the vector itself and compares only
its own expectation. `after` is not followed further: a named vector's
own `after` is not decoded first (it is not recursive), so a vector's
`after` lists every packet before it, in order, that the decoder must
have seen (`nsdc1-keyframe-after-loss` names `nsdc1-full` itself, not a
vector that names it). A vector without `after` decodes on a fresh decoder
alone. Loss is a packet left out of `after`: `nsdc1-keyframe-after-loss`
is decoded after `nsdc1-full` only, with the delta that came between
omitted. An `after` that names the vector itself, a missing vector, a
vector of another codec, or that forms a cycle through the vectors it
names, is a malformed vector, and the runner reports it.

The station's media runner decodes the bytes and compares the result with
`expect`. Where the encoder is exact (`nrsc1`, `dnssd-txt`, `ps3d`, `nsdc1`, `nsdx1`) it also
holds the encoder to the bytes: it encodes `expect` (or, for `nsdc1` and
`nsdx1`, the regen target's fixed inputs, and for the three malformed `nsdc1`
vectors and the refused `nsdx1` vectors those packets with the damage the
table names) again and
compares. Opus is not held
to its bytes, because its floating-point encoder may differ between
processors; its vectors hold decoders to the reference PCM instead.

| Codec | Vector | After | Decoded values |
| --- | --- | --- | --- |
| `nrsc1` | `lan-announcement`: one schema-1 LAN announcement datagram, as a Core from before schema 2 sends it (section 14.1) | none | `schema` 1, `controlPort`, `fingerprint`, `coreName`, `radioName`, `radioMac`, `radioConnected`, exact |
| `nrsc1` | `lan-announcement-2`: one schema-2 LAN announcement datagram, from a claimed Core whose pairing window was reopened (section 14.1) | none | `schema` 2, the fields above, `claimed` true, `identity` (base64url of the 32 bytes, no padding), `label` `KG4VCF/shack`, `pairing` `code`, exact |
| `nrsc1` | `lan-announcement-2-devices`: the `lan-announcement-2` datagram with the device count byte, 2, appended after Pairing (section 14.1) | none | The fields of `lan-announcement-2` and `devicesConnected` 2, exact |
| `nrsc1` | `lan-announcement-2-radio`: the `lan-announcement-2-devices` datagram with the radio state byte, 1 (`connected`), appended after the count (section 14.1) | none | The fields of `lan-announcement-2-devices` and `radio` `connected`, exact |
| `nrsc1` | `lan-announcement-2-waiting`: the same Core waiting for a radio to be chosen: Radio connected 0, no radio name, MAC `00:00:00:00:00:00`, radio state 2 (section 14.1) | none | The fields of `lan-announcement-2-devices` with `radioConnected` false, `radioName` empty, that MAC, and `radio` `waiting`, exact |
| `nrsc1` | `lan-announcement-2-trailing`: the `lan-announcement-2-radio` datagram with five bytes appended after its known fields, which a reader ignores (section 14.1) | none | The same fields as `lan-announcement-2-radio`, and `ignoredTrailingBytes` 5: the vector's last five bytes are not decoded, and the encoder writes the bytes before them, exact |
| `dnssd-txt` | `dnssd-txt`: the Bonjour TXT record of the Core of `lan-announcement-2-radio` (section 14.2) | none | `serviceType` `_nereus-station._tcp` and `txt`, the entries as strings (`v`, `id`, `claimed`, `pair`, `name`, `devices`, `radio`); the bytes are those entries in that order, exact |
| `ps3d` | `ps3d-frame`: one PureSignal display chunk, eight points and four correction points | none | Every header field and the eight value lists; `tolerance` `{"absolute": 0}`, because the values travel as IEEE-754 binary64 |
| `nsdc1` | `nsdc1-full`: frame 1, a keyframe | none | `disposition` `accepted`, `reason` `none`, `keyframe` (the header's keyframe flag), the context (`endpointId`, `contextGeneration`, `minDbm`, `maxDbm`), `encoderSequence`, `producerTimestamp`, `waterfallAdvance` and the reconstructed `traceDbm`, `waterfallDbm` and `wideDbm` rows; `tolerance` `{"dbm": 0.01}` |
| `nsdc1` | `nsdc1-delta`: frame 2, a delta | `nsdc1-full` | As above, `keyframe` false |
| `nsdc1` | `nsdc1-delta-after-loss`: frame 3, a delta, when frame 2 was lost | `nsdc1-full` | `disposition` `needKeyframe`, `reason` `sequenceGap`, no frame |
| `nsdc1` | `nsdc1-keyframe-after-loss`: frame 4, the keyframe the sender was asked for | `nsdc1-full` | `accepted`, `keyframe` true, the frame |
| `nsdc1` | `nsdc1-malformed-delta`: frame 2's delta with its trace plane's block size code set to 4 (no such size), to a fresh decoder | none | `disposition` `rejected`, `reason` `malformed`, `keyframe` false, no frame (not `needKeyframe` `noHistory`) |
| `nsdc1` | `nsdc1-malformed-stale-delta`: frame 2's delta with sequence 0 (older than frame 1's) and its last byte cut off | `nsdc1-full` | `rejected`, `malformed`, `keyframe` false, no frame (not `staleSequence`) |
| `nsdc1` | `nsdc1-malformed-keyframe`: frame 4's keyframe with its trace plane claiming one block more than its length needs, to a decoder that needs a keyframe | `nsdc1-full`, `nsdc1-delta-after-loss` | `rejected`, `malformed`, `keyframe` true, no frame |
| `nsdc1` | `nsdc1-transmit`: the first frame of a transmit display context (parity Task 28, `txDisplayVersion` 1): the transmit window, -80 to 20 dBm, and a tune tone's trace and waterfall rows; a transmit frame is an ordinary NSDC frame | none | As `nsdc1-full`: `accepted`, `keyframe` true, `waterfallAdvance` true, the frame |
| `nsdx1` | `nsdx1-full`: the display extras datagram beside `nsdc1-full`'s endpoint, every section (three blobs, a 32-sample peak hold row, the noise floor, the waterfall's levels) | none | `accepted` true, `reason` `none`, `endpointId`, `contextGeneration`, `encoderSequence`, `peakBlobs` (`pixel`, `dbm`), `peakHoldDbm` (the decoder's dequantised row), `noiseFloorDbm`, `waterfallLevelsDbm` (`lowDbm`, `highDbm`); `context` names the endpoint context it decodes against; `tolerance` `{"dbm": 0.01}` |
| `nsdx1` | `nsdx1-noise-floor`: the noise floor section alone | none | As above, with `noiseFloorDbm` the only section |
| `nsdx1` | `nsdx1-other-generation`: `nsdx1-full`'s bytes, against generation 2 | none | `accepted` false, `reason` `contextMismatch` |
| `nsdx1` | `nsdx1-unknown-section`: `nsdx1-full` with section bit `0x10` set | none | `accepted` false, `reason` `unknownSections` |
| `nsdx1` | `nsdx1-truncated`: `nsdx1-full` with its last byte cut off | none | `accepted` false, `reason` `truncated` |
| `opus` | `opus-1` to `opus-4`: four consecutive RTP packets of the speakers' mix at the station's settings (48 kHz stereo, 1920 samples per packet, 48000 bit/s, fullband, in-band FEC off, the Core's default `audio_bitrate` since R-R3-21; a Core set to `audio_bitrate = 24000` sends wideband instead, which the same decoder reads). A receiver stream runs Opus at 48000 bit/s fullband whatever `audio_bitrate` says (media control document, receiver audio) and is read by the same decoder; its `encoder` object reports the rate | the packets before it | `status` `accepted`, `sequence`, `timestamp`, `channels` 2, `bandwidth` 1105 (Opus fullband), `samplesPerChannel` 1920, and `pcm16`, the station decoder's output as 16-bit values (`round(sample * 32767)`); `ssrc` is the packets' RTP source, which the decoder is given; `tolerance` `{"minSnrDb": 60}` |

The three `nsdc1-malformed-*` vectors hold a decoder to the display codec
document's order ("State and recovery"): the whole datagram's structure
first (the codec bound, the header, and every plane), then the endpoint,
context, generation, sequence and history rules, then the datagram is
applied. A datagram that is both malformed and refused is `rejected` with
its structure reason, whatever state the decoder is in, and leaves history
untouched.

An NSDC `dbm` tolerance applies to every number of the decoded frame; an
NSDX one to every number of the decoded datagram, whose expectation lists
exactly the sections it carries (display extras v1, section 4). An
Opus tolerance is either `{"minSnrDb": N}` (the decoded PCM is at least N
dB above its difference from `pcm16`) or `{"lsb16": N}` (no sample is more
than N 16-bit steps from `pcm16`); the vector states which.

### 16.5 Running the station's runners

```
cmake --build build --target tst_link_conformance_control tst_link_conformance_session tst_link_conformance_media
QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_link_conformance_(control|session|session_connectable|session_connectable_datachannel|media)$' --output-on-failure
```

The session runner is registered three times: `tst_link_conformance_session`
runs every fixture but those whose `radio` is `"connectable"`, and
`tst_link_conformance_session_connectable` runs those alone
(`NEREUS_LINK_CONNECTABLE` set to `skip` and `only`; unset, the binary
runs every fixture). It runs each fixture twice, in two modes: over an
in-process pipe (`sessionFixtures`) and over the control data channel of
section 20 (`sessionFixturesOverADataChannel`, iPhone app plan Task 28),
where every client the fixture plays reaches the station over a data
channel of its own on this computer, DTLS and SCTP included, the station
presenting its own certificate. The connectable fixtures run one mode per
process: `tst_link_conformance_session_connectable` over the pipe and
`tst_link_conformance_session_connectable_datachannel` over the data
channel (`NEREUS_LINK_MODE` set to `loopback` or `datachannel`; unset, both
modes). In the data-channel mode the station's end reports no address of
the client's own, as the pipe does, since the fixtures were written over
it.

`tst_data_channel_transport` runs the framing fixtures (the sender's cut
and the receiver's joining, section 16.1), and over a real connection on
this computer: a message each way, a 300 KiB settings snapshot in chunks,
each end's cap, a whole session signed in by device key, a Core whose
DTLS certificate is not the bound one refused before the device sends
anything, and the heartbeat declaring the link dead at each end.

Each runner also alters one of its fixtures in memory and checks that the
failure names the step or field that differs; the media runner also checks
that a malformed `after` is reported. With
`NEREUS_LINK_TRACE_DIR` set, `tst_link_conformance_session` writes every
message the station sent in each fixture to `<id>.jsonl` there, which is
how a fixture is written to what the code does.

## 17. Changing the link

- **Anything on the wire changes with its documents, in the same commit.**
  A change that adds or alters a message, key, capability, class,
  property, verb, settings rule, telemetry field, media operation or limit
  updates this document, `tests/data/link/v1/` and `surface.json` together
  (regenerate with `tst_link_surface_manifest_regen`, then
  `python3 scripts/render-link-tables.py`), and the conformance runners pass
  on both ends.
- **New features get capability versions, not minors.** Each new feature
  gets its own `<feature>Version` in `StationCapabilities`, following
  `notchControlVersion` and `audioProfileVersion`. `kSessionProtocolMinor`
  stays 11.
- **Nothing unadvertised.** A client never sends a message kind or verb
  the station has not advertised, because an older station ends the
  connection on anything it cannot decode (section 13); an older client
  logs and ignores an unknown kind. Older peers see exactly today's wire.
- **The transmit boundary stays at the station.** No change lets a remote
  client key the radio outside the station's gates: transmit disabled
  until `snapshot.complete`, the watchdog, the starvation deadline, the
  time-out, the unkey-confirmed gate, `TxInterlockPolicy`, PA protection
  and SWR gating. A reconnect, a snapshot replay, a path change or a
  property write never keys.
- **Operator wording.** A reason a client may show an operator is plain
  English with no protocol terms. Every reason the station sends (the
  `reason` of `auth.result`, `session.end`, `command.result`,
  `property.result`, `settings.reject`, `pair.fail`, `confirm.request`
  and `notice`, a take choice's `why`, the three strings of `change`, and
  the display's
  `rejected` and `allocation-result`) passes `OperatorWording::isPlain` and names no
  function, class, requirement or phase; `tst_station_reason_wording`
  checks the sources that word them (every reason literal, a reason of one
  word, and what `.arg()` inserts into one; a new function that words a
  reason, in any file, fails until it is scanned) and every reason the
  session fixtures record. An app shows these as sent. Text the station
  sends as a property value that an app shows as it arrives is held to the
  same rule: the `connectionError` of the `tuner`, `amplifier` and `rfkit`
  objects, a slice's `nnrStatus` and `nnrLastError`, the radio's
  `rxFilter0Reason`, `rxFilter1Reason`, `rxFilter0LowPassReason`,
  `settingsSaveError`,
  `receiveLayoutRestoreMessage` and `fourO3AListenerError`, the
  `stationTci` object's `error`, the `txState` object's `stopText`
  (section 18.8), the `amplifier` object's
  `efficiencyText` (the Power Genius's own reading, passed on as it
  reports it), the `accessorySettings` object's `pgxlAnswer` and
  `tgxlAnswer`, and `pureSignalSettings`' `lastLoadError` (the same test
  reads the sources that write them). A library's or the system's own
  error text (a socket's, a file's) is never sent as one of these: the
  station words it and logs the library's text. A reason passed on
  without words of its own (a variable, a call) is named in the test with
  the source its words come from. Codes are not reasons and keep their
  spelling: an audio context's `reason` (`client-disabled`,
  `receiver-limit`, ...), `displayBudgetReason`, and the display retire
  reasons "slice removed" and "slice stream binding changed", which
  windows in use compare as they are. Device names and short names (the
  `devices` and `connectedDevices` objects, section 7.1) are the
  operator's own words, checked as names (section 3.5) and never held to
  `isPlain`, whose terms would refuse an ordinary name such as "Grant's
  iPhone"; a sentence of the station's that carries one is checked with
  the name set aside.

## 18. Transmit

iPhone app plan Task 34 (R-IOS-02, R-IOS-03, R-IOS-13; the several-devices
design, rulings 7.4, 8.1 to 8.5, 8.8, 8.13 and 8.15). This section is the
Core's side of remote transmit that exists today. Keying from a device
(`tx.key`, `tx.unkey`, `tx.tune`, `tx.twoTone`, section 18.6) came with
Task 35, and the microphone line with keying on a filled buffer (section
18.6) with Task 36; the watchdog, its keepalive and the microphone
starvation action (section 18.7) with Task 37; the transmit state
(`txState`, section 18.8) with Task 39; taking transmit (`tx.take`) with
Task 77.

### 18.1 Who may transmit

A session may transmit (`txPermitted`, section 6.3) when all of these hold,
in this order; the first that fails is its refusal (18.3):

1. the Core's `remote_transmit` is allow (`nereusd.conf`, default allow; a
   desktop that hosts a Core is deny) (`stationReceiveOnly`);
2. its hello declared `remoteTx` 1 at minor 11 (`notReady`, "Update this
   app");
3. it signed in with a paired device's key, not the older token
   (`notReady`, "Pair this device");
4. `snapshot.complete` has been sent to it (`notReady`);
5. transmit is unheld, or its device holds it (`otherDeviceHolds`,
   `changingHands`, `stopNotConfirmed`).

With `remote_transmit` deny the Core is receive-only, as every Core was
before: it refuses every key, its own radio's PTT included.

### 18.2 The holder of transmit

One device at a time holds transmit, keyed or not; it starts unheld. The
radio's own PTT (its microphone or a footswitch) makes the Core's own
position the holder, named "Radio", on unheld transmit and, taking it
without a question, from another holder (section 18.9). A person's key on
unheld transmit makes its device the holder; a program's key never does.
While another device holds transmit, every other device's key is refused;
a device takes transmit with `tx.take` (section 18.9). A holder keeps
transmit after its key ends, the Core's own position included, until
another takes it or it is released.

Every change of holder, and every release, is a transfer: every key from
every source is refused ("Transmit is changing hands."); a keyed holder is
unkeyed through the unkey-confirmed gate (receive reached, or 2000 ms and
the transmitter stopped at once); if the radio still reads keyed the Core
stops it again and waits up to 2000 ms more, and if it still reads keyed
transmit ends unheld and every key is refused ("The radio did not confirm
it stopped transmitting.") until it reads off. A new holder always starts
unkeyed, and VOX is turned off at every change of holder.

A holder whose link drops is unkeyed at once, keys refused until the radio
reads off, and keeps transmit, away and unkeyed, for its 3 minutes; the
same device signing in again within them still holds it, unkeyed, and keys
with its next press. At the end of its 3 minutes, on `session.leave` and
on its revocation, transmit becomes unheld through a transfer.

`connectedDevices` (section 7.1) shows it: `holdsTransmit` is true for the
holder, keyed or not; `state` is `transmitting` while it is on the air, and
`transmittingForSeconds` how long.

### 18.3 Refusals

A refused key, `tx.setTxSlice` or on-air change carries one of these. A
`command.result` refused this way has the sentence as its `reason` and, in
its `values`, `refusalCode` (the code) and `refusalFix` (the fix, or empty)
as `utf8` entries; a `property.result` and `settings.reject` carry the
sentence. A client shows the sentence as sent and may offer the fix.
"<holder>" is the holder's name as `connectedDevices` numbers it, or
"Radio"; "<short name>" is its short name.

| Code | Sentence | Fix |
| --- | --- | --- |
| `notReady` | This device is still connecting to the Core. Try again in a moment. | |
| `notReady` | Update this app to transmit through this Core. | |
| `notReady` | Pair this device with the Core to transmit through it. | |
| `stationReceiveOnly` | This Core is set to receive only. | |
| `bandPlan` | The band plan's own sentence (for example "14.360000 MHz is outside the transmit bands for your region (United States)." or "AM is not allowed on 60 m in the United States."), or "The band plan does not allow transmitting here." | |
| `interlock` | The radio's transmit inhibit input is holding transmit off. | |
| `interlock` | The transmit interlock is holding transmit off. Check it in Setup. | |
| `ampStandby` | The amplifier is in standby. Operate it, or change the interlock in Setup. | `operateAmp` |
| `paProtection` | The amplifier has tripped. Reset it before transmitting. | |
| `swr` | The SWR is over the interlock's limit. Check the antenna, or change the interlock in Setup. | |
| `otherDeviceHolds` | <holder> has the transmitter. | `takeTransmit` |
| `programNeedsTransmit` | A program can transmit only while this device has transmit. Take transmit here first. | `takeTransmit` |
| `micNotReady` | Microphone is not ready. Check Audio settings and retry. (the Core's own microphone); This device's microphone is not connected to the Core. Wait a moment and try again. (a remote voice key with no microphone line, section 18.6); No sound has reached the Core from this device's microphone. Wait a moment and try again. (a remote key whose line sent nothing within 1 s, or did not fill within 250 ms of its first packet, section 18.6) | |
| `changingHands` | Transmit is changing hands. Try again in a moment. | |
| `stopNotConfirmed` | The radio did not confirm it stopped transmitting. | |
| `holderOnAir` | <short name> is on the air. Try again when they stop. ("The radio is on the air. Try again when it stops." while the radio's own PTT, or the Core's own keys, hold transmit; the same sentence for a key whose transmit binding would land on another device's slice, from a device with a slice of its own to move the flag to, while the radio is not back in receive or the flag is frozen, and for the hosting desktop's own PTT in that case) | `takeTransmit` |
| `notHolder` | Take transmit on this device first. | `takeTransmit` |
| `otherDeviceHolds` | <holder> has the transmitter. Take it to stop the transmission. (`tx.unkey`, or TUNE or two-tone off, from a device that does not hold transmit) | `takeTransmit` |
| `keyEnded` | The Core already stopped this transmission. Key again to transmit. | |
| `noTransmitSlice` | There is no slice to transmit on. Add a slice first. (every key while the Core has no slice; and a device's key, the hosting desktop's included, whose transmit binding would land on another device's slice (for a device on sliceAccess or the hosting desktop; for any other device, only the slice it lost when another device took control of it) while the device has no slice of its own it may transmit on; with one, its unkeyed flag moves there and the key goes ahead; and the radio's own PTT on a hosting desktop, with the flag on another device's slice, while the desktop has no slice) | |
| `chooseTransmitSlice` | You took this slice from another device. Choose it for transmit first with its TX button. (a key whose transmit binding would land on a slice this device took from another device and has not chosen with `tx.setTxSlice`, or the hosting desktop's TX button, while it has no other slice it may transmit on) | |

`changingHands` and `stopNotConfirmed` are the several-devices design's two
sentences without codes; `keyEnded` answers a copy of a key the Core has
already stopped (section 18.6); `holderOnAir` is ruling 7.4's; `notHolder` answers
`tx.setTxSlice` while nobody holds transmit, a case the design does not
settle. `chooseTransmitSlice` is the slice control plan's ruling Q8(c):
a lone window or phone keying on its own slice is never refused for not
having sent `tx.setTxSlice`.

### 18.4 While the holder is on the air

While the holder is keyed, these changes from any other device are refused
with `holderOnAir`, never asked; the holder's own change is not refused by
this rule, and with the holder unkeyed none is. Every holder counts, the
station device's own keys included (the radio's PTT, and the Core's own MOX,
TUNE or a Tuner Genius hardware TUNE): the rule names changes, not holders.
The holder changes its transmit antennas, and the Alex tab's three transmit
high-pass switches (section 8.1), on the air as in Thetis; another device's
change waits. The saved amplifier and tuner addresses
(`setTgxlAddress`, `setPgxlAddress`, `setRfKitAddress`) and the LAN scans go
ahead on the air; the addresses are still asked of a device that holds
transmit (the several-devices design, table 7.1). The station device has no
session, so a change that would disturb only it applies without a question.
The words name the holder by its short name; "The radio is on the air. Try
again when it stops." after the radio's own PTT took transmit, and for the
Core's own keys on a Core no desktop hosts; a hosting desktop's own key is
named after the desktop.

- the transmit path: the amplifier (`configurePgxl`, `disconnectPgxl`,
  `setPgxlConnectionSettings`, `setPgxlName`, `setPgxlHardware`,
  `setPgxlNetwork`, `savePgxlSettings`), the tuner (`configureTgxl`,
  `disconnectTgxl`, `setTgxlName`, `setTgxlNetwork`, `saveTgxlSettings`,
  `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass`, and the `tuner`
  object's operate, bypass and antenna and the `amplifier` object's
  operate), an antenna (`setAlexRxAntenna`, a slice's `rxAntenna` or
  `txAntenna`, the `alexAntennas` object), PureSignal (the
  `pureSignalSettings` object, `transmit`'s `pureSig`, and every `ps3.*`
  verb but `ps3.twoTone` and `ps3.subscribeDisplay`), the interlock
  (`setTxInterlockPolicy`) and the power cap (`setPgxlPowerCap`);
- a sample-rate change on a Protocol 1 radio (`requestSliceSampleRate`),
  which stops the radio's data flow;
- a C-Tune centre change on the receiver of the holder's transmit slice
  (`requestStreamCentre`).

While the station device is keyed (the radio's own PTT, its mic or
footswitch, or the Core's own keys), the slice it transmits on is frozen for
every device, its owner's included (the several-devices design, ruling 8.11;
D64): a write of its `frequency`, `dspMode`, `filterLow`, `filterHigh`,
`txAntenna`, `band`, `xitEnabled` or `xitHz`, `removeSlice` or
`slice.selectBand` for it, a pan move that would carry, move or close it (the
owner's `requestStreamCentre` to a free receiver, or the anchor's move of its
receiver), and a sample-rate change that reaches it, are refused
`holderOnAir` with the holder's on-air words. A change asked before the key
and confirmed during it is refused at `confirm.proceed` the same way, and
nothing of it applies; so is a take of the transmit slice or its receiver.
The freeze ends with the key, and the transmit flag never moves while it
holds (`tx.setTxSlice`, a change of holder).

### 18.5 `tx.setTxSlice`

`tx.setTxSlice {sliceId}` moves the transmit flag to the slice with that
id (never a list position). It is the holder's: from another device while
transmit is held it is refused `otherDeviceHolds`, and while nobody holds
transmit `notHolder`; for a slice that is not the holder's own it is
refused with the words every slice verb uses for another device's slice
(iPhone app plan Task 77, ruling 8.10). The holder's choice is remembered:
when it takes transmit again, the flag goes to that slice if it is still
its own, otherwise to its active slice. When the holder's transmit slice
closes, the flag moves to another of its slices; with none left, transmit
is released through a transfer to nobody (ruling 8.12). While the holder is keyed the transmitter is unkeyed
through the unkey-confirmed gate first and the flag moves once receive is
reached; unkeyed it moves at once. The result is `accepted` when the move
is asked; the slices' `txSlice` deltas carry the move.

### 18.6 Keying

iPhone app plan Task 35 (R-IOS-13; spec section 4.6 item 7; D51, D58,
D63; the several-devices design, section 2.2 and rulings 8.3, 8.5 and
8.14). Four verbs, under `remoteTxVersion` 1 at minor 11, for a peer that
declared `remoteTx` 1 (any other peer is refused "Update this app to
transmit through this Core."):

- `tx.key {trigger}`: key the transmitter on the transmit slice.
  `trigger` is `"screen"`, `"headset"`, `"bluetooth"`, `"actionButton"`
  (a person's key) or `"tci"` (a program's key, sent by a remote window's
  TCI server for an app); any other value is refused "The Core could not
  read this request."
- `tx.unkey {epoch}`: release the key with that epoch. The transmitter
  stops now (the normal unkey; TUNE and two-tone end their own way).
- `tx.tune {on}`: TUNE on or off; on keys at the tune power.
- `tx.twoTone {on}`: the two-tone test on or off.
- `tx.tunerTune {on}` (`remoteTxVersion` 2, iPhone app plan Task 77): the
  Core's Tuner Genius autotune, as the Core's own Tuner page runs it (the
  amplifier to standby, the tune carrier, the tuner's sweep, the amplifier
  back to operate). It is a key for every rule below, answered with the
  epoch its carrier takes; the carrier starts once the amplifier is in
  standby (up to 1.5 s later), keyed as this device, so the watchdog
  watches it (section 18.7), and ends when the tuner finishes, or after
  3 s when the tuner never starts its sweep. `{on:false}`, or `tx.tune
  {on:false}`, ends this device's cycle, keyed or still waiting. The
  session's and the holder's refusals come first; then, while the radio is
  on the air (MOX on or walking back to receive, or TUNE; the device's own
  key included), it is refused `holderOnAir` "The radio is on the air. Try
  again when it stops." with no fix, since the amplifier is never switched
  between standby and operate while RF flows; then in plain words "No
  Tuner Genius is connected to the Core." when none is, and "The tuner is
  already tuning." while any cycle runs or any start is pending. The same
  `holderOnAir` refusal comes while this device's own start is on its way
  to keying (its `tx.key` waiting for its microphone, its two-tone
  settling). After those, in plain words: "The amplifier is still
  switching. Try again in a moment." while the Power Genius has not
  reported the state it was last sent to, and "A transmission is about to
  start. Try again when it ends." while a key is about to start at the
  Core (a PTT held, a take running) and the amplifier would have to be
  switched. While this device's cycle runs, waiting for the amplifier or
  with its carrier up, its own `tx.key`, `tx.twoTone {on:true}` and
  `ps3.twoTone {enabled:true}` are refused "The tuner is already tuning."
  (and `tx.tune {on:true}` while it waits; with the carrier up it answers
  with the carrier's epoch). If the radio goes on the air meanwhile (a VOX
  key), the cycle ends without keying; so does a cycle whose amplifier
  does not report standby within 1.5 s. The amplifier returns to operate
  only once the radio reads receive, no key is on or about to start and
  the amplifier has reported the state it was last sent to; until then it
  stays in standby (a taker transmits barefoot). A take (section 18.9)
  ends the old holder's cycle before the transfer.

**The amplifier changing over.** From any `operate=0` or `operate=1` the
Core's Power Genius is sent (by the autotune or by an operator) until it
reports that state, every key holds its RF (MOX and `keyed` are on, the
transmitter sends nothing): a device's key of any kind, VOX and the radio's
own PTT alike. If the amplifier has not reported within 1.5 s of the
command, the key is stopped (`stopReason` `station`, "The amplifier did not
finish switching. Try again."). With no Power Genius connected nothing
waits.

Every key passes the Core's gates in the order of section 18.1, then TX
inhibit, the PA trip, receive only, the band plan and the interlock, and
only then the holder's rule (section 18.2), as every key at the Core does;
a refused one is refused with its code (section 18.3), and a key refused
before the holder's rule takes nothing. TUNE and two-tone meet TX inhibit,
the PA trip, receive only and the interlock before the holder's rule, and
the band plan in the mode they transmit in (TUNE swaps CW for sideband
first); a TUNE or two-tone refused after the holder's rule never keys, and
the take it made is released, so a refused TUNE or two-tone, a device's or
the station's (its TUNE button, a Tuner Genius hardware TUNE), takes
nothing either.

**Who may key.** A person's key (`tx.key` with any trigger but `"tci"`,
and `tx.tune`, `tx.twoTone`, `tx.tunerTune` and `ps3.twoTone` on) while
transmit is unheld makes that device the holder, unkeyed, and then keys;
nobody is asked, and the transmit flag goes to that device's own slice
(ruling 8.10). The holder
keys. Another device's key while transmit is held, keyed or not, present
or away, is refused `otherDeviceHolds`. A program's key
(`tx.key {trigger:"tci"}`) never takes transmit: it keys only while its
device already holds transmit, and is otherwise refused
`programNeedsTransmit` (unheld) or `otherDeviceHolds` (held by another
device), and nobody becomes the holder. While a device holds transmit, a
VOX key at the Core is that device's.

**Who may release.** `tx.unkey`, `tx.tune` and `tx.twoTone` off, and
`ps3.twoTone {enabled:false}`, are the holder's: from a device that does not hold transmit they are refused
`otherDeviceHolds` ("<holder> has the transmitter. Take it to stop the
transmission.") and the transmission continues. A release ends only that
device's own key (a VOX key while it holds transmit is its own). The Core's
own safety stops end whoever is keyed.

**The keying epoch.** An accepted `tx.key`, `tx.tune {on:true}` or
`tx.twoTone {on:true}` answers with the key's epoch in its `values`:
`epoch` (`i64`, 1 to 4294967295, advancing with every key; a refused key
spends none). `tx.unkey` names it. A device sends each key and each unkey
three times, as the same command (the same verb and `id`); the Core acts
on the first and answers every copy:

- a copy of an accepted key is answered with the same epoch while that
  key is on, and refused `keyEnded` once it has ended (released, stopped
  by the Core, or taken), so a delayed copy never keys again;
- a copy of a refused key is answered with the same refusal;
- a `tx.key` with a new `id` while the device's own key is on changes
  nothing and is answered with that key's epoch;
- a `tx.unkey` whose epoch is older than the device's key now on is
  ignored (answered accepted; nothing changes), and one with nothing of
  the device's on is answered accepted.

A session's commands are forgotten when it ends. A device whose link
drops is unkeyed at once (section 18.2) and, when it signs in again, nothing
keys until it sends a new `tx.key`: no replay, no resume.

An accepted `tx.unkey`, and an accepted TUNE or two-tone off, carry no
values.

**Keying on a filled buffer (Task 36).** A device whose media connection
carries the microphone line (its media `start` carried `remoteTxVersion`;
remote media control document, Microphone line) sends its microphone while
it transmits. Its `tx.key`, in a mode that transmits the microphone (every
mode but CWL and CWU), is answered once the line's buffer holds its target
(30 ms on a steady link, remote media control document), and then keys. A device starts its line with the key, so the 250 ms
for the buffer to fill runs from the line's first packet after the key
arrives; when the buffer has not filled within 250 ms of that packet, or no
packet has come within 1 s of the key, the key is refused `micNotReady`, "No sound has reached the Core from this device's
microphone. Wait a moment and try again." The holder's own refusals come first, at once.
Copies of a waiting key, and a new `tx.key` from the same device, get the
waiting key's answer; a `tx.unkey` from the device while its key waits
answers that key `keyEnded`, and it never keys. `tx.tune` and
`tx.twoTone` use no microphone and key at once. A voice key (`tx.key` with
any trigger, a program's included, in a mode that transmits the microphone)
from a device whose media carries no microphone line (no media yet, as
right after a reconnect, or a media start without `remoteTxVersion`) is
refused at once `micNotReady`, "This device's microphone is not connected
to the Core. Wait a moment and try again.", after the session's own
refusals and the holder's, and nothing keys: the Core never puts its own
microphone on the air for a remote key. While a device is keyed on its
line, or has VOX armed, the transmitter takes that line instead of the
Core's configured microphone; at unkey the configured source returns with
the line's buffer empty. Several devices may carry a line at once (each
media connection its own); one line feeds the transmitter at a time: the
keyed device's, else the line of the device whose key is waiting for its
buffer, else the VOX device's (the holder when it has VOX armed). Every
other line is received and dropped, never mixed in, and a change of line
starts from silence. One device's media ending never closes another's
line, and a keyed device whose media restarts stays on silence until its
new line carries audio. A VOX key while the device has VOX armed is that
device's, as a VOX key while it holds transmit already is.

**Programs through a remote window.** A remote window's TCI server
forwards an app's `trx:N,true` as `tx.key {trigger:"tci"}` and its
`trx:N,false` as `tx.unkey` for the window's own key. It takes the TCI
transmit audio for the app only after the Core accepts the key; a refused
key takes nothing, and the app hears `trx:N,false` while the window shows
the Core's sentence. The Core's own TCI server stays receive-only: a
program through it never keys, held or unheld.

**The desktop's remote window.** It keys as a phone does: its MOX (the TX
applet's button and the container's) sends `tx.key {trigger:"screen"}`, its
TUNE `tx.tune`, its two-tone `tx.twoTone`, each command three times under
one `id`; its own MoxController keys nothing. A release names the key's
epoch, or 4294967295 when released before the answer came (never older
than the device's live key, so a key accepted just before the release
still stops). After the Core ends a key on its own (its `transmitting`
falls), the next press is a new command. Its TUNE off goes whenever it
asked TUNE on, even before the Core's `tune` reached it. A program's
release while the operator's own MOX holds the same key on sends nothing;
the operator's MOX off ends a program's key too. The window's VOX button
writes `transmit.voxEnabled` (a permitted session's write, section 18.1),
and while it is on the window streams its microphone on the microphone
line unkeyed.

### 18.7 The watchdog and microphone starvation

iPhone app plan Task 37 (R-IOS-13; remote design sections 8.3, 12.1 and
12.3; pairing design section 9.7; spec section 4.6 items 1 and 2).

**The numbers, stated together.** A device keyed, or with VOX armed, sends
a keepalive every 100 ms. The Core stops transmitting once more than
400 ms pass without one from it (the link-loss deadline). A keyed device's
microphone line counts as starved after 250 ms without audio (the
starvation deadline). The Core's transmit buffer for the line targets
30 ms on a steady link and never holds more than 120 ms. A client's first reconnect comes
1000 ms after a loss. So 120 < 250 < 400 < 1000: starvation is handled
before the link counts as lost, and the Core has stopped before any
reconnect (`RemoteTxWatchdog`, `RemoteMicConfig`, checked at compile time).

**`tx.keepalive {sequence, epoch}`.** Under `remoteTxVersion` 1 at minor
11, for a peer that declared `remoteTx` 1 (any other is refused "Update
this app to transmit through this Core."), both arguments `i64`:

- `sequence`: 1 for the first keepalive of a session, then one more with
  each keepalive the device sends, whichever path it takes; it never starts
  again while the session lasts. 0, or more than 9007199254740991, is
  refused "The Core could not read this request."
- `epoch`: the epoch of the device's key now on, from its answer (section
  18.6); 4294967295 while a key's answer has not come and for a key the
  device was never answered for (a VOX key, TUNE and two-tone when it did
  not keep theirs), which is never older. 0 to 4294967295.

It is sent once, never three times: a lost one is overtaken by the next.
It is answered `accepted` with no values, whether or not it counted, and
refused only when unreadable.

**What the Core watches.** A device while `keyedBy` names it (its own key,
its program's, TUNE, two-tone, or a VOX key that is its), and a device
whose accepted write turned `transmit.voxEnabled` on, while VOX stays on.
Watching starts with a fresh 400 ms. A keepalive counts when its
`sequence` is newer than the last that counted from that device (a copy
by another path, or one overtaken, does not) and its `epoch` is not older
than the device's key's. The device's own release (`tx.unkey`, TUNE or
two-tone off) ends the watch on its key at once, so a transmission that
ends by itself after a release (a RADE end-of-over tail) is never
taken for a lost link.

**When it stops.** More than 400 ms without a keepalive that counts, or
the device's session ending (a drop, leaving, a replacement or a
revocation), and the Core turns off the VOX that device armed and, when
the key on the air is that device's, stops transmitting at once (the
emergency stop, then StopAllTx) with "The link to <device> went quiet, so
the Core stopped transmitting.", "<device>" being its name as
`connectedDevices` gives it. Nothing keys again by itself: the device's
next key is a new `tx.key` (section 18.6).

**The paths.** On the session's WebSocket the keepalive is this verb. On a
media connection with the microphone line it travels on that connection's
`tx` data channel instead (unordered, never retransmitted; 13 bytes, remote
media control, "The "tx" data channel"), so a lost keepalive never waits
behind a retransmission: at 5 % loss the channel's keepalives never trip
the watchdog in ten minutes, while the same loss on a reliable in-order
channel does (`tst_remote_tx_watchdog`). The rendezvous, relay and separate
control connection (iPhone app plan Tasks 26 to 29) hand their keepalives
to the same rules.

**When a keepalive counts as heard.** A `tx`-channel keepalive is judged
at its network receipt (when the media transport received it), not when
the Core's event loop got round to it: a keepalive that waited behind a
stall of the Core is heard at the time it arrived, and that time only
moves the device's last keepalive forward, never back. The 400 ms is
measured from that receipt both when a keepalive comes and when the
check runs. A check that fires more than 5 ms after its time ran behind a
stall, so it first lets the keepalives waiting behind it in, one turn of
the event loop, and then judges. That turn is given once for each
device's overdue period: the check after it judges the device, whatever
else (another device's keepalives included) moved the check in between.
A check no more than 5 ms late is the timer's ordinary slack and judges
at once.

**VOX a device armed.** A device whose media carries no microphone line
cannot arm it: its `transmit.voxEnabled` write is refused "This device's
microphone is not connected to the Core. Wait a moment and try again."
That refusal is the backstop: a client shows its VOX control disabled with
the same words while its media carries no microphone line (the desktop's
TX applet VOX button and Setup's Enable VOX do). It goes off when that
device's session ends, when its link goes quiet and
when its microphone line closes. While it is on
and that device's line does not carry the audio VOX listens to, a VOX key
at the Core is refused (`micNotReady`): the Core never keys from its own
microphone because a device armed VOX.

**Microphone starvation on a live link.** When the device's line starves
while it is keyed on it, the transmit mode decides: LSB, USB, DSB, CWL,
CWU, DIGL, DIGU and SPEC stay keyed (silence there puts no carrier on the
air; WDSP's DSB adds none), until the time-out or the operator ends it;
AM, SAM, FM, DRM, RADE_U and RADE_L stop at once with "No microphone audio
arrived from <device>, so the Core stopped transmitting." TUNE and
two-tone use no microphone and are never stopped by it.

#### Independent transmit watch (unpublished extension)

A client declaring `remoteTx:1` and `txWatchPath:1` at minor 11 or newer can
receive `txWatchPathVersion:1` after its authenticated snapshot is complete.
This requires a paired device-key session, current transmit permission and a
live supported primary route. A direct WSS primary uses the separate WSS
route below. A relayed DTLS primary additionally requires `txWatchRelay:1`
and its own negotiated watch relay grant. Token-only sessions and older peers
receive no extension. An arbitrary DTLS primary is not a supported watch route.

On that primary, `tx.watchTicket` takes no arguments. An accepted command result
contains `ticket` (utf8, canonical base64url of 32 random bytes), `expiresInMs`
(i64, 10000) and `path` (utf8, `/tx-watch/v1`). The one-use ticket binds the exact
primary, admitted paired device, logical session and connection generation.
There is one pending ticket or active watch per primary and at most four per
Core. Issuance is limited to once per second per primary. Matching consumes the
ticket even if attachment fails; an unrelated guess cannot consume it.

The client opens a NEW WSS connection on the verified primary's authority and
fixed path. Before disclosing its ticket it checks the actual new TLS peer
certificate SHA-256 against the current verified Core pin. Neither ticket nor
pin goes in the URL, logs or rendezvous signaling. The first binary application
message is byte 1 followed by 32 raw ticket bytes. Core acknowledges with `[1,0]`.
Only the existing 13-byte channel keepalive format is then accepted. No hello,
session command, text, settings or media passes through this socket.

The Core branches this exact path before ordinary peer adoption, refusing query
parameters and user information. Before returning to the event loop it limits
messages and frames to 33 bytes. At most eight pending attachments (also eight
per address group) may wait for five seconds. Accepted watches have a 20-frame
burst/50-frame-per-second bound and 4 KiB outbound backlog bound. These sockets
never take a device place or hold transmit independently.

Primary end, revocation, replacement, or the start of a path move retires the
watch and invalidates its ticket before callbacks can reuse it. A failed path
move does not restore the old generation. Core rechecks the primary binding
and permission for each heartbeat. Auxiliary loss alone never ends or replaces
the primary; primary loss still stops its key immediately. The existing sequence,
epoch and 400 ms rules apply, so duplicate or stale copies never revive a key.
Receipt checks elapsed time before renewal as well as on the timer: a packet
arriving more than 400 ms after the last accepted one cannot rescue an expired
watch merely because the event loop has delayed the timer.

The client uses one 100 ms timer and the SAME sequence/epoch for its independent
copy and ordinary media-or-primary copy. It must pause ALL heartbeat paths while
any release is being dispatched or awaits its own accepted command result. A
nonzero command number alone is not delivery proof. A failed/refused release
remains fenced until session reset; an unrelated accepted off cannot clear it.
Every continuation checks owner lifetime, connection generation and this fence.

For a supported relayed primary, `tx.watchRelay` takes exactly one argument:
`offer` (ordinal 0, utf8), a nonempty valid UTF-8 SDP without NUL and at most
64 KiB. Core reserves the same bounded ticket before constructing a separate
relay-only DTLS answerer using its persistent certificate. The accepted result
has exactly four fields: `ticket` (ordinal 0, utf8), `expiresInMs` (1, i64,
10000), `path` (2, utf8, `relay-dtls-v1`) and `answer` (3, utf8). The ten-second
construction and attachment deadline runs from ticket issue. The watch has its
own relay leg, no STUN/TURN or public candidate path, and carries the same
33-byte attachment, two-byte acknowledgement and 13-byte heartbeat frames.
The client must verify the actual DTLS certificate against its current Core
pin before sending the ticket. Relay grants use rendezvous section 12.9 and
are separate from this Core-authenticated ticket.

New relay-watch admission requires the matching primary's unexpired grant.
An already issued pending ticket or attached watch can survive admission-grant
expiry while its exact primary route, generation and authority remain valid.
Capabilities preserve that distinction. Primary end, replacement, revocation
and path move retire pending construction before late callbacks can attach it.

Direct client/Core attachment and both relay owners are built with real
paired transport regressions. Production RV opt-in and actual loaded loss
acceptance remain in progress. The Core relay fixture uses a local relay
protocol player; the client owner fixture uses a bounded command responder.
Neither proves combined acceptance by the production Python service or
implies any radio transmission.

### 18.8 The transmit state (`txState`)

iPhone app plan Task 39 (D14, R-IOS-13, R-IOS-21; spec section 5.5 items 5
and 8). The `txState` object (`TransmitState`, `txStateVersion` 1; 2 adds
the holder of transmit and `keyedForSeconds`, appended after `stopSerial`,
and `stopEpoch` after them, then `highSwr` and `swrWindBackLatched`,
added with `txDisplayVersion` 1 and sent by every Core whatever its
`txDisplayVersion`, as `stopEpoch` is; then `forwardAdcRaw`,
`reflectedAdcRaw` and `compressionDb`, with `txReadingsVersion` 1) goes to
a peer at minor 11 whose hello declared `remoteTx` 1, in its snapshot after
`connectedDevices`, and as deltas. Every property is `outbound`; a write
is refused as any outbound property's is.

| Property | Meaning |
| --- | --- |
| `keyed` | The radio is on the air (MOX, TUNE or two-tone): the radio object's `transmitting` |
| `tuning`, `twoTone` | TUNE is on; the two-tone test is on |
| `txSliceId` | The slice transmit is bound to, -1 for none |
| `keyedByName`, `keyedByKind`, `keyedTrigger` | Who keyed (the device's name as `connectedDevices` numbers it, its kind) and how (the `tx.key` trigger, `tune`, `twoTone`, `vox`, or the Core's own `radioPtt`, `cat` or `station`); `""` while unkeyed |
| `keyedSinceMs` | When the key began, in milliseconds on the Core's own monotonic clock (the one `connectedDevices`' durations use); 0 while unkeyed. Compare it only with another `keyedSinceMs` |
| `timeOutRemainingSeconds` | Whole seconds before the transmit time-out stops this key, -1 when no time-out applies (unkeyed, or the limit for this device's kind is off). Phones and tablets have their own limit, 180 s by default |
| `forwardPowerWatts`, `reflectedPowerWatts`, `swr` | The radio's power readings, as the Core's own power and SWR meters show them; `swr` is 1.0 with no forward power |
| `alcDb`, `micLevelDb` | The ALC and MIC readings as the Core's meters show them (Thetis's readings: ALC floored at -30 dB, MIC at -195 dB); -400, no reading, while the Core has no transmit channel |
| `txEnding` | True only during a RADE end-of-over tail: after an operator's release in RADE, while the radio sends FreeDV's end-of-over frame (at most 1 s). The Core's stops never wait for it |
| `stopReason` | Why the Core last stopped a transmission on its own: `""` (none since the Core started), `linkLost`, `micStarved`, `timeOut`, `takenOver`, `revoked` or `station` |
| `stopText` | That stop in plain words, for an app to show as sent |
| `stopSerial` | Advances by one with each such stop (serial-number arithmetic, as `devices`' `revision`); 0 before the first |
| `stopEpoch` | The keying epoch (the `epoch` `tx.key`, `tx.tune` or `tx.twoTone` answered with) of the key that stop ended; 0 before the first stop. An app ends only a key of its own whose epoch is this one or older: a key it pressed after the stop, answered before the stop's update arrived, goes on |
| `holderDeviceId` | Who holds transmit (the several-devices design, ruling 8.1; `txStateVersion` 2): the device's id as `connectedDevices` sends it, `station` for the Core's own position (the radio's PTT, the Core's own keys); `""` while unheld |
| `holderName`, `holderShortName`, `holderKind` | The holder's name and short name as `connectedDevices` numbers them, and its kind; for the station device, kind `station`, named after the desktop that hosts the Core for that desktop's own MOX or TUNE (ruling 8.1), and "Radio", "Radio" after the radio's own PTT took transmit or on a Core no desktop hosts; `""` while unheld |
| `holderSource` | How it got transmit: `device`, or `radioPtt` after a take by the radio's own PTT (a mic or a footswitch); read this, never the name, to tell the radio from a device; `""` while unheld |
| `holderForSeconds` | How long it has held transmit, in whole seconds on the Core's clock when this is sent (ruling 10.3; the app counts on from its receipt); 0 while unheld |
| `holderEpoch` | Advances with every change of holder (a take, a release); the same while the holder keys, unkeys or goes away |
| `holderAway` | The holder's link dropped and it keeps transmit, unkeyed, for its 3 minutes |
| `holderTransferring` | Every key is refused while it is true: transmit is changing hands, or a dropped holder's key is being stopped ("Transmit is changing hands."), or, with no holder, the radio did not confirm it stopped transmitting after a transfer ("The radio did not confirm it stopped transmitting.") until MOX reads off |
| `keyedForSeconds` | How long the key now on has been on, in whole seconds on the Core's clock when this is sent (ruling 10.3); 0 while unkeyed. It supersedes `keyedSinceMs`, which a Core still sends |
| `highSwr` | The Core's high-SWR protection has tripped (parity Task 28, appended after `stopEpoch`; sent by every Core that sends `txState`, whatever its `txDisplayVersion`): what the Core's own window hands its transmitting pan's high-SWR border |
| `swrWindBackLatched` | The protection's drive fold-back has latched; the border shows fold-back while this and `highSwr` are both true |
| `forwardAdcRaw`, `reflectedAdcRaw` | The radio's raw forward and reflected power readings (i64, the ADC counts of its last PA sample, transmitting or not; parity Task 33, `txReadingsVersion` 1; 0 before the first sample). Existing remote desktop windows scale these with the Core's `hpsdrModel` as their native PA Values page does |
| `compressionDb` | The COMP reading (f64, dB; parity Task 33 follow-up, `txReadingsVersion` 1), as the Core's own Compression meters show it: Thetis's reading, the transmit channel's `TXA_COMP_AV` floored at -30 dB (console.cs:46979, dsp.cs:1013-1014 [v2.10.3.15]), so -30 with the speech processor off; -400, no reading, while the Core has no transmit channel. Read with the other meters |
| `forwardRawPowerWatts`, `forwardAdcVolts`, `reflectedAdcVolts` | Core-scaled raw forward power (W) and forward/reverse ADC voltage (V), f64, `txReadingsVersion` 2. The Core calls the same `PaTelemetryScaling` functions as native PA Values with its current radio model, on the existing PA sample/meter cadence and radio change. All three are outbound only and reset with the session |
| `eqDb`, `levelerDb`, `levelerGainDb`, `cfcDb`, `cfcGainDb`, `alcGainDb`, `alcGroupDb` | The seven stage readings a local window's container meters show (f64, dB; A9, `txReadingsVersion` 3), each Thetis's reading as its MOX branch works it from the transmit channel (console.cs:46971-46986 [v2.10.3.15]): EQ and Leveler `TXA_EQ_AV` and `TXA_LVLR_AV` floored at -30 dB, Leveler gain `TXA_LVLR_GAIN` negated and floored at 0, CFC `TXA_CFC_AV` floored at -30, CFC gain `TXA_CFC_GAIN` floored at 0, ALC gain `TXA_ALC_GAIN` plus 3 floored at -195, and ALC group `TXA_ALC_PK` floored at -30 plus `TXA_ALC_GAIN` plus 3 floored at 0. -400, no reading, while the Core has no transmit channel. Read and sent with the other meters, and reset with the session |

The AM Mod Monitor's readings (peaks, holds, carrier, lamps and envelope
trace) are not `txState` properties: they travel as the `txAmModulation`
and `txAmModulationFeedback` record streams, to a peer that subscribes
(`txModMonitorVersion` 1, section 7.7), whether or not it declared
`remoteTx`.

**When it is sent.** While keyed the Core reads the meters ten times a
second, from the transmit lane's last readings (never a DSP call on its
event loop), and sends a reading that changed, with the time left when it
changed. While unkeyed only changes are sent: the power readings as the
radio reports them (they fall to 0 at the unkey), and `forwardAdcRaw` and
`reflectedAdcRaw` as the radio's samples change them, nothing else.

**A window's transmit meters** (parity Task 33) read this object while
keyed, with the local window's scaling, smoothing and peak handling: the TX
applet's RF Pwr and SWR bars and the container Power, Reflected Power and
SWR meters read `forwardPowerWatts`, `reflectedPowerWatts` and `swr`
(through the window's radio status, as one sample); the ALC and MIC
meters read `alcDb` and `micLevelDb`. The S-meter's TX modes: Power reads
`forwardPowerWatts`, SWR `swr`, Level `micLevelDb` (the MIC reading, the
one a local window's Level mode reads) and Compression `compressionDb` (the
COMP reading, the one a local window's Compression mode, its Compression
meters and the Phone/CW compression gauge read). On a Core below
`txReadingsVersion` 1 Compression shows no reading with "This Core does not
send this reading. Updating the Core may help." Each container meter bound
to a reading `txState` does not carry (EQ, Leveler, Leveler Gain, CFC, CFC
Gain, ALC Gain, ALC Group) shows no reading with its reason, never 0.

**PA Values in a remote window** reads `forwardPowerWatts`,
`reflectedPowerWatts` and `swr` for its power rows and `forwardAdcRaw`
and `reflectedAdcRaw` for its raw rows, with the local page's peak and
minimum tracking. One difference is accepted: a local window's radio
status takes forward and then reflected power as two steps, so its page
also sees an SWR worked from the new forward reading and the previous
reflected one and may track it as a peak. The Core sends each sample's
three readings together, so a remote page never sees that in-between pair;
its SWR value matches, and its SWR peak can be lower. Changes
travel in the 50 ms delta flush like any other.

**Stops.** Each key (each rise of `keyed`) is stopped at most once: the
first reason the Core records for it advances `stopSerial` and sets
`stopReason` and `stopText` together, and a later one for the same key
changes nothing. A device's own `tx.unkey`, and TUNE or two-tone off, are
no stop. The reasons:

- `timeOut`: the transmit time-out (iPhone app plan Task 38): "Transmit
  stopped after 3:00, the Core's time-out for phones and tablets." for a
  phone or tablet, "Transmit stopped after 3:00, the Core's transmit
  time-out." for any other, and "Transmit stopped: the Core's network
  check went unanswered for 3:00." for the ping time-out, each with its
  limit;
- `linkLost`: the device on the air went quiet (the watchdog, section
  18.7), or its session ended, or the holder connected again on another
  link, while it was on the air: "The link to <device> went quiet, so the
  Core stopped transmitting.", the one sentence the Core's log and toast
  say too;
- `revoked`: the holder was removed from the Core while on the air:
  "<device> was removed from the Core, so the Core stopped transmitting.";
- `micStarved`: no microphone audio arrived from the device on the air
  in a mode that stops on it (the starvation action, section 18.7): "No
  microphone audio arrived from <device>, so the Core stopped
  transmitting.";
- `takenOver`: another device or the radio took transmit while the holder
  was on the air (iPhone app plan Task 77): "<taker> took transmit, so the
  Core stopped transmitting." ("Radio took transmit, ..." for the radio's
  own PTT);
- `station`: any other stop the Core made itself: "The Core stopped
  transmitting.", or, for a key whose RF waited for the Power Genius and
  was stopped when it did not report within 1.5 s (iPhone app plan Task
  77 fix round 2; words from fix round 3), "The amplifier did not answer.
  Put it in standby or disconnect it in Setup to transmit without it."
  (A FAULT report while `operate=1` is unconfirmed, or an error reply to
  the operate command, ends the wait at once: the key goes out barefoot.
  From fix round 4 an error reply ends it only when no earlier command is
  still unconfirmed, and only for a code that reads and is not zero.) A
  key whose Power Genius goes to operate by itself while the radio
  transmits (commanded by nobody, as a fault clearing on its own) is
  stopped with "The amplifier switched to operate by itself while the
  radio was transmitting, so the Core stopped transmitting."

The Core sends the object to the holder and to every other declaring peer,
so a device that lost its link learns why from the snapshot when it signs
in again. A device that was removed is not connected to learn it.

### 18.9 Taking transmit

iPhone app plan Task 77 (R-IOS-02, R-IOS-03; the several-devices design,
sections 8.4 to 8.7, rulings 8.1, 8.6 to 8.12 and 7.7; `remoteTxVersion`
2).

**`tx.take {holderEpoch, shownKeyed}`** (both optional, sent together or
not at all; for a peer with `sessionHolderVersion` 1, any other is refused
"Update this app to transmit through this Core."). It never keys: the
device keys with its next `tx.key`, which it may send straight after an
accepted take. The session's own gate (section 18.1, items 1 to 4) comes
first. Then:

- transmit unheld: taken at once, answered accepted with `values`
  `holderEpoch` (the epoch after the take);
- the device already holds it: accepted, `holderEpoch`, nothing changes;
- another holder, and `holderEpoch` equals `txState.holderEpoch` and the
  holder is not on the air unless `shownKeyed` is true (the device asked
  its operator first, ruling 8.7): taken at once;
- another holder otherwise: refused "Waiting for you to confirm." with
  `phase` `needsConfirmation`, then a `confirm.request` of kind
  `takeTransmit` (section 7.5), `affected` empty, with `holder`, the
  holder's entry: `{deviceId, name, shortName, kind, source, state, keyed,
  connectedForSeconds, lastActivitySeconds, awayForSeconds,
  transmittingForSeconds}`, `source` `device` or `radioPtt` (then "Radio",
  kind `station`, `deviceId` `station`), `state` `transmitting` while the
  holder is on the air (the red "Unkey and take over"), `away` in its
  3 minutes (asked without red; nothing to unkey), else `listening`;
- a transfer running, or the radio not confirming its stop: refused
  `changingHands` or `stopNotConfirmed`.

A copy of a `tx.take` (the same command id, which a device repeats as it
does every transmit command) is never asked again and never takes twice:
while the first one's take runs, that take's one result answers the id;
after it, a copy gets the same answer again.

`confirm.proceed {id, choice: -1}` takes transmit; when the holder was not
on the air when asked and is now, or the holder changed, it is answered
"Waiting for you to confirm." and a new `confirm.request` follows (the
operator always sees the red question before a carrier is cut); when
transmit became unheld meanwhile it takes at once.

**The take** is the transfer of section 18.2: a keyed holder is unkeyed
through the unkey-confirmed gate before anyone is assigned, every key is
refused meanwhile ("Transmit is changing hands."), and the taker holds
transmit unkeyed. The answer (of `tx.take` or of the proceed) arrives when
the transfer ends: accepted with `holderEpoch`, or refused
`stopNotConfirmed`. A holder taken from on the air has its stop recorded
(`stopReason` `takenOver`). It is told with a `notice` of kind
`transmitTaken`, "<taker> took transmit.", naming the taker (`bySource`
`radioPtt` and "Radio" for the radio's own PTT), with Take it back. A
holder taken from while away does not get transmit back when it returns:
its notice waits and follows its `snapshot.complete`. A taker whose own
session ends while its take runs holds transmit away when the take ends
(its 3 minutes running) if it dropped, and transmit is unheld if it left
or was a token window.

**Take it back.** `notice.takeBack {id}` of a `transmitTaken` notice is
`tx.take {}` with its usual question (red when the taker is on the air,
the radio's microphone included) and answers as `tx.take` does. A notice
is taken back at most once.

**The radio's own PTT** (its mic or a footswitch, the radio's PTT bit),
at a press edge: with the Core's own position already holding transmit it
keys, the names and source unchanged; otherwise it takes transmit without
a question, unkeying a holder on the air first, whatever its key
(`tx.key`, TUNE, two-tone, `tx.tunerTune` or VOX), and keys only once the
transfer ends and only while still pressed. A press that takes nothing (TX
inhibit, the PA trip, receive only, a transfer already running) is held
off until it is released, so it never keys later without a fresh press. A press released during the
transfer keys nothing. A press still held after a device takes transmit
back does not take it again; the next press does, and releasing it never
unkeys a device's key. `txState` then names "Radio" with `holderSource`
`radioPtt`. A desktop that hosts the Core does not take on its MOX or
TUNE while another device holds transmit: that key is refused
`otherDeviceHolds`, and the desktop takes by these rules first.

**The transmit slice.** A take moves the transmit flag to the new
holder's own slice (its last choice, else its active slice); the radio's
own PTT transmits where the flag is. `txSlice` marks a slice only while its
owner holds transmit (section 7.1).

**The transmitter's own settings** (ruling 7.7). While transmit is held,
a write to the `transmit` object from another device is refused
"<holder> has the transmitter.", and so is `txProfile.select` (with
`refusalCode` `otherDeviceHolds`); with transmit unheld any device may
change them. Turning `transmit.voxEnabled` on needs holding transmit: from
another device it is refused "<holder> has the transmitter.", and with
transmit unheld "Take transmit on this device first.". A carrier action
(`tx.tune`, `tx.twoTone`, `tx.tunerTune`, `ps3.twoTone` on) is a key
(section 18.6).

## 19. The rendezvous

A client reaches a Core it cannot address directly through the rendezvous
at `rv.nereussdr.com` (or a self-hosted one), specified in its own document,
[2026-09-23-rendezvous-v1.md](2026-09-23-rendezvous-v1.md), with its
conformance suite in `rendezvous/conformance/v1/`. It uses this document's
identity values unchanged (section 3.4): the Core registers under an id
derived from its station identity key, a device introduces itself by its
device id (section 3.5) with a signature by its device key, and pairing by
code carries section 3.6's `pair.*` messages, as text, inside the
rendezvous's mailbox messages. The rendezvous introduces the two ends and
mints relay credentials; the session that follows is this link, on its own
connection, direct or through the relay, never through the rendezvous.

**Pairing through a mailbox** (iPhone app plan Task 27). A mailbox carries
the `pair.*` messages of section 3.6 and nothing else: no `hello` goes
either way and no `session.end`. The device sends `pair.start` in code
mode as the mailbox's first message (one tap never pairs through it: a
mailbox has no address of its own), and the exchange then runs exactly as
on a direct connection, the station's `pair.fail` or `pair.confirm` ending
it. A message of any other kind from the device ends the pairing as a
protocol error. A mailbox has no certificate, so the device keeps the
Core's identity key from the station's box and checks the certificate
binding against the certificate of its first sign-in (section 3.4). The
Core gives its nameplate back only after that mailbox has closed, since
releasing a nameplate ends its mailbox.

The rendezvous never learns a device's name or the Core's label (the
rendezvous document, section 1), and it reads every mailbox message, so
nothing that names either travels in the clear there. The device's
`pair.start` through a mailbox gives a neutral `name` for its kind (the
desktop "Computer", the phone "iPhone"), since a device's own name often
carries its operator's callsign; its real name goes only in the sealed
confirmation box (section 3.6, step 5), and that is the name the Core
records, as it is for every pairing by code. The Core's label goes only in
its sealed box too: `pair.accept`, which carries it in the clear, is sent
only after one tap, which a mailbox never carries.

## 20. Control over a data channel

iPhone app plan Task 28 (R-IOS-16; the remote design, section 10.4). A
device the rendezvous introduced (section 19; the rendezvous document,
section 6.3) runs this link over a WebRTC data channel instead of a
WebSocket. Everything from section 3 on applies unchanged: the same
messages, the same connect sequence (the station sends `hello` first, as
soon as its end opens), the same identity checks, caps, heartbeat,
deadlines and endings. This section says what differs: how the connection
is made and how messages travel on it (`DataChannelTransport`,
`StationRendezvous`, `RendezvousDialer`).

**The connection.**

- It is a peer connection of its own, carrying one data channel and
  nothing else: labelled `control`, reliable and ordered (neither
  `maxRetransmits` nor `maxPacketLifeTime`), announced in band, made by the
  device, which makes the offer. The offer is `introduce`'s `offer` and the
  Core's answer is its `answer`. Each description holds one `m=application`
  line and no candidates; a description with any other line, or with a
  candidate in it, is refused, so the Core answers no introduction whose
  offer is not a control connection. Candidates travel one at a time
  through the rendezvous (`candidate`), the end of candidates as an empty
  one.
- ICE is as the rendezvous document, sections 6.3 and 8, says: one STUN
  server, from the rendezvous's `hello`; the relay credentials the answer
  brings; each end chooses the STUN server and its one relay host by its
  own address families, the names resolved on that end first
  (`IceConfiguration`); gathering starts once the credentials are known;
  the path MTU is 996 bytes. A Core that answered with `turn` true and has
  no `credentials` within 5 s (`StationRendezvous::kCredentialsTimeoutMs`)
  gathers without the relay. With `relay = deny` no relay candidate is used
  in either direction.
- The Core forgets an introduction, freeing its place among the 16 it
  holds (`RendezvousClient::kMaxLiveIntroductions`), as soon as its
  connection opens or fails, or when it has not opened within
  `StationRendezvous::kAnswerDeadlineMs` (the credentials wait, two name
  lookups of 3 s and the 63 s connect deadline); nothing goes on the wire
  for it. An introduction handed back after that is dropped. The desktop
  gives one attempt `RendezvousDialer::kDialDeadlineMs` (the service's 10 s
  hello time, two lookups and the connect deadline), then retries with the
  same backoff as a WebSocket connection (section 12.4), through the
  rendezvous again. Once the channel opens the device leaves the
  rendezvous; the session never passes through it.
- The media connection (section 11) of such a session is a second peer
  connection, negotiated in `media.control` exactly as section 11 says, with
  the control connection's ICE settings: the same STUN server, candidates
  of every type, and the relay only where the control connection needed
  it. An end whose control connection's selected pair goes through a relay
  (either end's relay candidate, or the far end's relay learned as
  peer-reflexive) allocates on its chosen relay host for media too; an end
  whose control path is direct gathers no relay candidate for media, so a
  direct session holds one relay allocation at each end (its control
  connection's), not two. Each end still accepts the far end's relay
  candidates when the relay is allowed. The desktop allows its connection stage
  23.5 s more for the gathering (`RemoteMediaController`). A session over a
  WebSocket keeps host candidates only.
- When a connection ends, each end gives each of its relay allocations back
  at once, with a TURN `Refresh` whose `LIFETIME` is 0 (RFC 8656 section
  7.2), rather than holding it, and a place in the relay's quota for the
  Core's id, for the allocation's lifetime.

**The certificate.** The Core presents its own persistent TLS certificate
(section 3.1, the one its identity key binds, section 3.4) in the control
connection's DTLS handshake; the media connection keeps one-off
certificates. The device takes the SHA-256 of the certificate the DTLS
handshake carried (the DTLS layer also holds it equal to the SDP's
`a=fingerprint`, but the SDP arrives through the rendezvous, so its
fingerprint alone is never trusted) and uses it wherever a WebSocket
session uses the TLS certificate's: the `hello`'s `certBinding` must verify
for it before the device sends anything (section 3.4), and it is the
certificate hash in the device's sign-in transcript (section 3.5). A Core
whose DTLS certificate is not the bound one is refused, as
`identityChanged`, before the device's `hello`. The Core's end sees no
certificate of the device's.

**What the Core allows on it.** The Core marks every control connection
the rendezvous introduced (`StationServer::acceptIntroducedTransport`) and
holds it to three rules a direct connection does not have, because an
introduction is not device authentication (the rendezvous document,
section 4) and the service could replay one:

- It signs in by a paired device's own key only. An `auth.request` with a
  `token`, with or without a `device` block, is refused (`auth.result`,
  code `protocolError`, not retryable) before either limiter sees it.
- It never pairs. Every `pair.*` message is answered with `pair.fail` and
  the connection ends; no code is taken or burned. Pairing through the
  service is the mailbox's, by code (section 3.6 and the rendezvous
  document, section 6.5).
- While still connecting, every such connection counts as one source
  against the per-address handshake cap (`kMaxHandshakesPerAddress`, 2),
  whatever address it reports; one that would pass it gets the same
  retryable `session.end` as an address over the cap. A device's sign-in
  failures are limited per introduction as well as per device id and
  address (`DeviceAuthRequest::introduction`).

**Messages.**

- A session message (section 4: one JSON object, UTF-8) travels as one or
  more binary data-channel messages, its chunks, each at most 61440 bytes
  (`controlChannelChunkBytes` in section 15): a first byte of `0x01` (more of
  this message follows) or `0x02` (this chunk ends it), then the next piece
  of the message, at least one byte. A sender fills every chunk but the last
  (61439 bytes of the message each), so a message of n bytes takes
  ceil(n / 61439) chunks, and sends a message's chunks one after another.
  Nothing travels as a text data-channel message.
- A receiver joins the pieces in order. It ends the connection, as the
  WebSocket's cap does, the moment the bytes it has joined for one message
  pass its inbound cap (section 12.3: the station 1 MiB, the client 8 MiB),
  without waiting for the last chunk. It also ends it on an empty message,
  a chunk with no piece, a first byte it does not know, a message longer
  than 61440 bytes, a text message, and a ping or pong that is not 5 bytes.
- The heartbeat (section 12.1) uses 5-byte messages: a ping is `0x10`
  followed by a 4-byte id (big-endian, the first 1, then one more each
  time), a pong `0x11` followed by the id of the ping it answers. The
  transport answers a ping at once, as a WebSocket stack does; a pong for a
  ping this end sent is the only evidence the link is alive, and the rules
  of section 12.1 (a ping every 20 s, the link dead at a tick with 2
  unanswered) are unchanged. A ping or pong may arrive between two chunks of
  a message and leaves the message being joined as it is.
- An end closes by closing the channel and its connection. As on a
  WebSocket, the reason a client acts on arrives first in `session.end` or
  `auth.result`; the close itself carries none.
- On a relayed path the station's end has no address of the device's own
  (section 12.3's connection with no address): it is not counted by
  address, and one tap never pairs over it.

The framing fixtures (`framing/`, section 16.1) hold both ends to the
chunking and the heartbeat bytes; section 16.5 names the station's runners.

## 21. Paths

iPhone app plan Task 29 (R-IOS-16, R-IOS-08; the pairing design, section
5.4; the remote design, section 12.1). A device paired with a Core has
several ways to reach it: straight to an address it has for the Core (the
Core's WebSocket, section 2), and through the rendezvous (section 19) over
the connection it introduces (section 20), whose ICE finds a path on its
own: between the two ends' addresses, through the addresses the STUN server
reflects, or through the relay (TURN over UDP). A device tries these
together, uses whichever reaches the Core first, and moves the session to a
better path when one opens later, without ending it. This section says how
(`PathRacer`, `SwitchableTransport`, the `path.*` messages and the media
`replace` operation). Nothing here applies to a Core trusted by its pin and
its token (section 3.2): a device connects to such a Core at its addresses
one after another, as before.

### 21.1 The race

A device connecting to a paired Core starts every path it has at once, each
a **rung** (`PathRacer::Rung`):

| Rank | Rung | Starts |
| --- | --- | --- |
| 0 | the Core's WebSocket at an address on one of this device's own networks (a loopback address, one inside the subnet of one of its running interfaces, or a name ending `.local`) | at once for an IPv6 address; 250 ms later for an IPv4 address when the device has an IPv6 address to try (`PathRacer::kIpv4DelayMs`) |
| 1 | the Core's WebSocket at any other address | the same |
| 2 | the rendezvous, its connection's selected pair not relayed | at once |
| 3 | the rendezvous, its selected pair relayed | (the same rung as 2; its rank is known when the connection opens) |
| 4 | the floor: the relay over TCP 443 (reserved, section 21.6) | not yet built |

The addresses are the ones the device keeps for the Core (where it last
reached it, most recent first, section 14's announcements, the addresses
the Core itself sent in `devices`' `coreAddresses` at its last sign-in or
since, section 7.1, and the address the operator gave), each host name
resolved first, its IPv6 addresses tried
at once and its IPv4 addresses 250 ms later. The rendezvous rung needs the
Core's rendezvous id, which a device derives from the Core's identity key
(the rendezvous document, section 4.2), so a device learns it from the
Core's `hello` at its first sign-in or from pairing.

A rung is **ready** when the Core's `hello` on it shows the identity key
this device paired with and its certificate binding verifies for the
certificate that connection presented (section 3.4; over the rendezvous the
DTLS certificate, section 20). A rung whose `hello` shows another key ends
as "another computer answered" and the others go on. The first rung to be
ready wins: the device signs in on it (section 3.5) and the session runs
there. A rung that becomes ready while the winner is signing in, and whose
rank is better, is kept for an upgrade (section 21.3) once the session
reaches `snapshot.complete`; every other rung stops then. A device never
signs in twice in one race, so the same-device rule (section 12.4,
`sameDevice`) never ends the winner.

A direct rung has the Core's connect deadline (section 12.2, 30 s) to be
ready; the rendezvous rung has `RendezvousDialer::kDialDeadlineMs`. When
every rung has ended without a winner the attempt fails, and the device
retries with the backoff of section 12.4 and races again.

**An older Core.** A Core that registers with the rendezvous but answers no
introduction (one without `controlChannelVersion`, section 6.3) leaves the
rendezvous rung waiting. A device stops waiting for the Core's `answer`
`RendezvousDialer::kAnswerDeadlineMs` (10 s) after its `introduce` and ends
that rung with "This Core can't be reached through the internet service.
Updating the Core may help." A device that recorded `controlChannelVersion`
1 for the Core knows it is not too old: it ends the rung with "The Core did
not answer through the internet service. Check that it is on and online."
A device with a fresh authenticated `controlChannelVersion` 0 observation
does not start the rung, and says the first words. Once that observation
expires or a changed network generation invalidates it, the next race
tries the service again under the same answer deadline and bounded retry
backoff. A failed service probe cannot renew the negative observation; a
new authenticated ordinary-session snapshot can.

**The relay.** A Core with `relay = deny` in `nereusd.conf` answers every
introduction with `turn` false (the rendezvous document, section 6.3), so
no relay credentials are minted and neither end uses a relay candidate; the
floor (section 21.6) needs the same permission. A Core says which it has in
its capabilities (`relayAllowed`, section 6.3); a device records it with
the paired Core and, while it says false, leaves the relay out of every
race and says why. A device that has recorded nothing yet learns it from the
Core's `answer`: one without relay credentials, though the device asked
for the relay, and with no relay grant after it (the rendezvous document,
section 12.1: a service that holds a relay secret sends one whenever the
Core allows the relay, even with no TURN secret), is the Core's `relay =
deny`, and the record says so. A service with neither secret offers no
relay at all, and a device that recorded nothing yet reads that the same
way until its first sign-in records `relayAllowed`.

At most `PathRacer::kMaxDirectOpening` (2) direct rungs are open and not
yet signed in at once, the rest waiting their turn: a Core takes
`StationServer::kMaxHandshakesPerAddress` (2) connections still signing in
from one address, counting each until its `snapshot.complete`, and all of
a device's direct rungs come from one. A rung holds its turn from its start
until it ends, is let go, or the race finishes with the winner signed in
(a winner still signing in and a standby hold both). An address is dialled once however the device
names it (a literal and a host name that resolves to it), and an IPv6
address keeps its scope (a link-local one reaches nothing without it).

**The attempt record.** The device keeps one line per rung
(`StationConnectionAttempt`): the path (this network, direct, through the
internet service, relay), the address or the service's host, and how it
ended: connected, no answer, no answer in time, another computer answered,
did not connect, another path connected first, the Core has the relay
turned off, or the Core can't be reached through the internet service. It
is for the connection messages; nothing of it goes on the wire.

### 21.2 Moving the session to another connection

`controlSwitchVersion` 1 (section 6.3). A session moves from its
connection (the **old** one) to another (the **new** one) without ending:
no `session.end`, no sign-in, no `snapshot.complete` and nothing replayed.
Messages keep their order in each direction across the move. Only the
device starts a move, and only to a connection of a better rank than the
one the session runs on (section 21.1).

1. The device opens the new connection and waits for the Core's `hello`
   there, which must show the paired identity key with a binding that
   verifies for the new connection's certificate (section 3.4). It sends
   nothing on the new connection yet.
2. On the old connection it invokes `session.pathTicket` (no arguments;
   with any, the Core refuses it with "The request to move this connection
   was not understood.", as `session.leave`).
   The Core answers with `values` `ticket` (`utf8`: 32 random bytes,
   base64url without padding, 43 characters) and `expiresInMs` (`i64`,
   10000, `StationServer::kPathTicketLifetimeMs`). A ticket belongs to the
   session that asked, is good once, and is gone after 10 s or when the
   session asks again. The Core refuses (`accepted` false) while the radio
   is transmitting or switching between receive and transmit (`MoxController`
   not idle: keyed, or MOX's delay timers running), with "Not while the
   radio is transmitting.", and a device does not ask while it is keyed or
   has VOX armed.
3. On the new connection the device sends its `hello` (section 5.1) and
   then, in place of `auth.request`, `path.join` with the ticket. The
   ticket is a secret like the token: it travels only inside a connection
   the Core's identity was verified on, and is never logged.
4. The Core checks the join: the ticket is live; the new connection agreed
   the same major and minor as the session; it is not a pairing or a
   mailbox; the radio is not transmitting (as in step 2); and when the new
   connection came through the rendezvous (section 20), the session signed
   in with a paired device's own key. Any failure ends the new connection
   alone with `session.end` (`retryable` false, code `protocolError`,
   "The Core did not move the connection here.") and consumes the
   ticket; the session goes on over the old connection. On success nothing
   is sent on the new connection yet.
5. The Core sends `path.switch` on the old connection, the last message it
   sends there, and from then sends the session's messages on the new one.
6. The device, when `path.switch` arrives on the old connection, sends its
   own `path.switch` there, the last message it sends there, and from then
   sends on the new connection.
7. Each end reads the old connection up to the other's `path.switch`, then
   the new one. What arrives on the new connection before that is held, in
   order (at most 4 times the end's inbound cap, section 12.3; past it the
   session ends). When the device's `path.switch` reaches it, the Core
   closes the old connection, with no `session.end`; the device closes its
   end once the Core has, or after 5 s.

The heartbeat (section 12.1) runs on the connection the end sends on; a
pong on either connection counts. The new connection's connect deadline
(section 12.2) stops when it joins.

**When something fails.** A device that has sent `path.join` and gets no
`path.switch` on the old connection within 10 s
(`SwitchableTransport::kSwitchDeadlineMs`) closes the new connection and
stays on the old one. An old connection that closes before the other end's
`path.switch` counts as that `path.switch`: the end reads the new
connection from then on. A new connection that closes after the move
ends the session as a lost link does (section 12.4, retryable). A Core that
has sent `path.switch` and hears no `path.switch` from the device on the
old connection within 10 s ends the session as a lost link does (section
12.4, retryable): what the device sent on the old connection may still be
in flight, and the device reconnects and resyncs rather than lose it
silently. A device that has sent its `path.switch` closes its end of the
old connection once the Core has, or after 5 s. A `path.switch` outside a move, or a `path.join` from a
device whose Core did not advertise `controlSwitchVersion`, is a message
out of turn: `path.switch` is ignored, and `path.join` ends that connection
as step 4 says.

**Transmit.** A move never keys. It does not start while the radio is
transmitting or MOX's delay timers run (steps 2 and 4). A key pressed while
a move is under way goes through: the device's `tx.keepalive` messages
(section 18.7) keep their order across the move like every other message,
so the gap the watchdog sees is at most the difference between the two
connections' delays, and its 400 ms deadline (`RemoteTxWatchdog::
kLinkLossDeadlineMs`) is unchanged. A path that dies while keyed is caught
by that deadline on every path, as before (section 18.7).

### 21.3 Upgrading

While a session runs on a rung worse than rank 0, a device looks for a
better one: at once with a rung the race kept (section 21.1), then 5 s,
30 s and 2 minutes after the session reached `snapshot.complete`, and every
5 minutes after that (`PathRacer::kUpgradeRetryMs`), racing only the rungs
of a better rank than the current one: an address whose rank cannot beat
the current one is not dialled at all. Before each look, and at
`snapshot.complete`, a session through the rendezvous takes its rank from
the pair its connection settled on then (an ICE agent may nominate a
relayed pair first and a direct one later). A ticket refused for now (the
radio on the air) keeps the schedule on its current step. Through the
rendezvous an upgrade
looks for a path that is not relayed: the device gathers no relay candidate
and accepts none of the Core's for it (the Core may still allocate one, as
for any introduction; section 21.4). The first rung ready moves the
session (section 21.2). Nothing is tried while the device is keyed or has
VOX armed; the schedule waits for the unkey.

After a move the media connection (section 11) moves too, when the Core
advertises `mediaReplaceVersion` 1: the device asks for a new media
connection with the media `replace` operation (the remote media control
document, "Replacing the media connection"). A move that comes before the
media connection is ready (the race's standby taken at `snapshot.complete`
is the usual case) leaves the replacement pending: the device starts it
once media is ready and the radio is back on receive, so media never stays
on a relay the session has left. A session whose control came
through the rendezvous keeps the rendezvous's STUN server for its media
after a move, and takes a relay for media only while its control
connection's path is relayed (section 20), so media over a direct control
connection holds no relay allocation.

### 21.4 Relay allocations

Through the relay each connection allocates on one relay host at each end
that needs the relay (`IceConfiguration::setRelay`, one host chosen by
address family), so a session relayed at both ends holds 2 allocations at
each end: its control connection's and its media connection's, 4 of the
Core's relay quota of 8 (the rendezvous document, section 8). A second
device relayed at both ends fits (8); a third device's connection is
refused an allocation (TURN `486`) and gathers without the relay at that
end, so it connects only where a path without the relay exists. An
upgrade's connection through the rendezvous asks for no relay at the
device, but the Core cannot tell it from a first connection (the
introduction carries no such flag) and, when its `relay` allows, allocates
one for its end as for any answer. That allocation lives as long as the
Core's end of that connection: a few seconds when the look finds nothing
better (the device leaves the introduction and the Core frees the unopened
answer), or the rest of the session when the session moves onto it (its
relay is never selected, but an ICE agent keeps its allocations until the
connection closes). So a session stuck on the relay briefly takes one more
of the quota every look (at most every 5 minutes, section 21.3), and one
that moved to the service's path without the relay keeps one at the Core.
While the quota is full a look's allocation is refused (TURN `486`) and the
look runs without the Core's relay, which it would not use anyway.
Carrying control and media on one connection would halve these; it changes
section 20's offer and is not part of this version.

### 21.5 The rungs in code

`PathRacer` takes its rungs as `PathRacer::Rung` objects: `start()`, a
`rank()`, and signals for the transport once it opened and for its end. The
direct rung (`DirectPathRung`) wraps the Core's WebSocket at one address and
the rendezvous rung (`RendezvousPathRung`) the `RendezvousDialer`. A rung
added later joins the race with its rank and changes nothing else. ICE's
own candidates are the other seam: `IceConfiguration` takes extra candidate
sources (`IceConfiguration::CandidateSource`), each a low-priority remote
candidate the ICE agent races with the others inside one connection.

### 21.6 The floor (reserved)

A network that passes only TCP 443 blocks every rung above. The floor that
reaches the Core from such a network is chosen separately (the relay floor
measurement, `2026-09-23-relay-floor-measurement.md`) and joins either as
rank 4, a rung of its own, or as a candidate source inside the rendezvous
connection's ICE. Until it is built, a device on such a network does not
reach the Core, and its attempt record says what each rung met.


### Session microphone selection (`radioMic` 2)

At the existing minor-11 boundary, an authenticated peer declaring `radioMic` 2
and `remoteTx` 1 may receive `radioMicVersion: 2`. Version 1 only reports the
radio microphone catalogue and add-on note. It never offers this command.

`tx.setMicSource` has exactly one argument: ordinal 0, UTF-8 `source`, with the
literal value `ClientAudio` or `RadioMic`. The command changes only that
authenticated device/session's input selection. It neither changes the Core's
independent PC/VAX/radio preference nor keys or releases transmit. MOX and a
key waiting for microphone priming refuse changes. Session permission, holder
authority and the Core's `board.radioMic` capability remain admission gates.
An accepted result contains exactly one UTF-8 value `source` at ordinal 0,
matching the requested literal. A refusal reports the retained source and may
include the existing transmit refusal code/fix. Exact duplicates replay their
original answer without applying an old choice again; reuse of a stored source
command id for another payload or verb is refused. End/replacement of the
authenticated session retires its choice and reply cache.

A desktop keeps its PC/VAX preference separate from the accepted session input.
Only a matching current-session accepted reply displays Radio as active and
releases the desktop's microphone uplink demand. Pending, malformed, refused
or stale replies cannot start a microphone-dependent key using another input.
A Radio voice key latches genuine radio samples through normal or RADE processing
until hardware dekeys. It does not require desktop microphone RTP, priming or
RTP starvation recovery, and the Core's independent PC-capture readiness/loss
does not gate that exact remote key. Session/holder, RF, inhibit and control
keepalive/watchdog rules still apply. ClientAudio retains its silent feed-loss
and starvation-stop behavior; it never falls back to the radio microphone.

Remote radio-microphone VOX is unavailable under the existing Core-local VOX
authority. Selecting Radio disarms that device's remote VOX; attempts to arm it
are refused with “VOX from the radio microphone is not available from this
window.” Remote desktop program audio shares microphone RTP and also requires
ClientAudio: a Radio voice-program key is refused with “Choose PC/VAX input to
transmit program audio.” Core-local program overrides and non-microphone CW,
TUNE and two-tone paths retain their existing behavior. This feature adds no
remote VAX transport or claim about a physical codec sample format.

The radio speaker already receives all receiving slices automatically. Slice
AF level, mute and pan affect it. The Core's master volume/mute also affects
Core-local output; a desktop's speaker device and master affect that desktop.
This microphone feature adds no separate remote Core-master control.
