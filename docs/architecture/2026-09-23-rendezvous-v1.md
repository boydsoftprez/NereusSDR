# Rendezvous version 1

This is the written specification of the rendezvous: the small signalling
service at `rv.nereussdr.com` (and at any self-hosted address) that lets a
client reach a Core it cannot address directly, and carries the pairing
exchange when the two are not on one network. It is part of R-IOS-08 (pair
by code through the relay) and R-IOS-16 (direct and relay) of the
[iPhone app plan](2026-09-23-iphone-app-plan.md), Task 26, and D38 of the
[iPhone app design](2026-09-23-iphone-app-design.md). The design behind it is
the [station identity and pairing design](2026-08-02-remote-station-identity-and-pairing-design.md),
sections 5.1 to 5.5.

Three parties use it:

- **The service**, `rendezvous/server/` in this repository.
- **A station**: the Core (`nereusd`) in its station role, which registers
  under its id and answers introductions (iPhone app plan Task 27).
- **A client**: the desktop's remote window (Task 27) or the iPhone app
  (Task 27a), which introduces itself to a station, or opens a pairing
  mailbox.

The conformance suite in `rendezvous/conformance/v1/` holds all three to
this document (section 10). Where the service's code and this document
disagree, this document is the authority and the code is the bug.

Section numbers are stable. Later revisions add sections; they never
renumber.

## 1. What the rendezvous does and does not do

The rendezvous introduces; it never carries a session. It does four things:

1. **Registration.** A station proves it holds its identity key and is then
   reachable under its id (section 6.2).
2. **Introductions.** A client passes an offer to a station by id, the
   station answers, and both trickle ICE candidates through the service
   (section 6.3). The session itself then runs on its own ICE connection,
   direct or through the relay, never through the rendezvous.
3. **Relay credentials.** When a station accepts an introduction and allows
   the relay, the service mints short-lived TURN credentials for coturn and
   gives the same ones to both ends (section 8), and at the same moment a
   relay grant for the WebSocket relay, one token for each end (section
   12).
4. **Pairing mailboxes.** A station claims a nameplate (the number in its
   pairing code) and a client opens the mailbox on it; the two exchange the
   link's `pair.*` messages as opaque text (section 6.5).

It never learns a label, a callsign, a pairing code's words or a device's
name, and it cannot tell whether a device is paired: it matches opaque ids,
forwards what it is given untouched, and keeps everything in memory
(section 9). What it can see is in the pairing design, section 5.2: the
addresses of both ends, that an id is online, traffic timing, and the
fingerprints of the DTLS certificates in the offers and answers it carries
(the Core's is its TLS certificate's).

## 2. Transport

- One WebSocket per connection, over TLS on TCP 443: `wss://rv.nereussdr.com/`
  (a self-hosted server has its own host name). The path is `/`; the service
  ignores it. On the NereusSDR server, `/v1/relay` and every path under it
  reach the WebSocket relay instead (section 12), never the service. On the NereusSDR server the server's own Caddy terminates TLS and
  forwards the WebSocket to the service on loopback by host name; the service
  itself never listens on a public address.
- **What a client sends to open it.** The opening handshake is RFC 6455's,
  over HTTP/1.1: `GET /` with `Upgrade: websocket` (the value in any case;
  Apple's Network.framework writes `WebSocket`), `Connection: Upgrade`,
  `Sec-WebSocket-Key` and `Sec-WebSocket-Version: 13`, and a `Host` of the
  service's name, with or without `:443`. Open it over HTTP/1.1, in TLS
  with ALPN `http/1.1` or none: the NereusSDR server does not offer
  WebSockets over HTTP/2 (RFC 8441; its Caddy advertises no
  `SETTINGS_ENABLE_CONNECT_PROTOCOL`), so a client whose stack would use
  HTTP/2 must be kept to HTTP/1.1 for this connection. Anything that is
  not a WebSocket upgrade gets `426` with a short plain text (and, over
  HTTP/1.1, `Upgrade: websocket`). `rendezvous/tests/caddy-check.sh`
  shows both the usual request and Apple's exact one upgrading through
  Caddy, and what an HTTP/2 client gets.
- Messages travel as WebSocket text frames, each one JSON object, encoded
  compactly, with a string `type` naming its kind. A binary frame is a
  protocol error (section 7).
- The service caps each inbound message at 131072 bytes (128 KiB) at the
  WebSocket layer, before anything is decoded; a longer one closes the
  connection with close code 1009 and no error message. The field caps of
  section 5.2 are smaller and are checked when a message is decoded.
- **The sender's rule.** A field's cap (section 5.2) counts the UTF-8 bytes
  of the decoded string, not of its JSON text, and the whole message as
  sent, encoded, must also be at most 131072 bytes. A station or client
  checks both before it sends, and treats a message that would break either
  as it treats a field that is too long: it does not send it. The two
  differ only for text that must be escaped: a 65536-byte body of control
  characters is 393216 bytes encoded, while a body of up to 21000 bytes
  always fits however its sender escapes it (an escape takes at most 6
  bytes for each UTF-8 byte it stands for), and so does any real SDP or
  `pair.*` message.
- **What a peer accepts.** The service never sends a message longer than
  132096 bytes, and a station or client accepts any message from the
  service of up to 262144 bytes (256 KiB). The longest message the service
  builds is the `answer` it sends a client: the station's `answer` (at most
  131072 bytes) less its `to`, plus a `turn` object. The only part of that
  object set by configuration is its URL list, so the service refuses to
  start with TURN URLs that are not printable ASCII (a quote and a
  backslash excluded, so none is ever escaped) or whose list, written as a
  compact JSON array, is over 768 bytes; four URLs of the default form take
  about 180. With that cap the bound holds (`test_queues_and_sizes.py`
  checks it at the cap).
- The service sends a WebSocket ping every 20 s and closes a connection whose
  pong has not come back 20 s later (section 9.2). Pings are control frames,
  not messages. Clients answer pings as any WebSocket stack does and need
  not send their own.
- The service closes a connection with close code 1000 after an error that
  closes (section 7), and with 1001 when it is shutting down. The reason a
  peer acts on is always in the `error` message before the close, never in
  the close frame, whose reason text is empty. Two closes come without an
  `error` message: 1009 for a message over the cap (above), and 1008 for a
  peer that has stopped reading: its unsent messages have passed its
  queue's cap, or it holds the largest share of the service's budget for
  every queue together when that budget runs out, or one message has taken
  longer than 30 s to send (section 9.1). It would not read an error
  message either.
- The service sends in the order it reads. When a peer closed with 1008
  was itself sending (its own message took the service past its budget),
  the message it sent still reaches the other end first, and only then
  what its leaving causes (`mailbox.closed` `peerLeft`,
  `introduction.end` `clientLeft` or `stationLeft`).

## 3. Roles and the life of a connection

Every connection starts the same way: the service sends `hello` (section
6.1) at once. The connection's first message decides its role, for the rest
of its life:

| First message | Role |
| --- | --- |
| `register` | station |
| `introduce` or `mailbox.open` | client |
| anything else | a protocol error: `error` `protocolError`, then the close |

A station connection serves one id. A client connection holds at most one
live introduction and at most one open mailbox at a time; when one ends it
may start another on the same connection. A message of the other role's
kinds is a protocol error.

Timers (defaults in section 9.1, all configurable):

- **Handshake.** A connection that has not sent its first message within
  10 s, or a station that has not finished registering within 10 s of its
  connection opening, gets `error` `timeout` and the close.
- **Idle.** A client connection with no live introduction and no open
  mailbox for 30 s gets `error` `idle` and the close. The timer starts when
  the client's first message has been answered without leaving anything
  pending (an `offline` answer, say) and whenever its introduction or mailbox
  ends. It starts once: a refused request while it runs (`offline`,
  `rateLimited`, `nameplateUnknown`, `nameplateBusy` or any other error)
  does not start it again, so refused requests cannot keep a connection
  open. Only a live introduction or an open mailbox stops it. A registered
  station is never idle; the pings keep its connection and its NAT mapping
  alive.

## 4. Identity values

### 4.1 Keys and signatures

The values are the station link's, section 3.4
([2026-09-23-station-link-v1.md](2026-09-23-station-link-v1.md)), with no
change:

- A **public key** is strict base64url (no padding, only `A-Z a-z 0-9 - _`,
  the unused low bits of the last character zero, a length that is not 1 mod
  4) of a canonical 91-byte P-256 SubjectPublicKeyInfo DER with its point
  uncompressed: 122 characters. Canonical means it loads as a P-256 key and
  encodes back to the same 91 bytes. Nothing else is accepted.
- A **signature** is ECDSA P-256 over SHA-256, raw `r || s`, 64 bytes,
  strict base64url: 86 characters. Not DER. `r` and `s` are each 32 bytes
  big-endian and each from 1 to n - 1, n the order of the P-256 group; 0 and
  n or above never verify. A high-s signature (`s` above n / 2) is valid:
  OpenSSL and CryptoKit verify it, and nothing on this wire depends on a
  signature being the only one for its message.
- A **device id** is the link's device id (section 3.5): the SHA-256 of the
  device's SPKI DER, 32 bytes, strict base64url: 43 characters. It is the
  device's `id` in the Core's paired devices, not its key.

### 4.2 The station's rendezvous id

A station registers under, and a client introduces itself to, the station's
**rendezvous id**:

```
id = base32(SHA-256("NereusSDR rendezvous id v1\n" || SPKI DER))[0..26]
```

- `SPKI DER` is the 91 bytes of the Core's **station identity key**, the
  P-256 key of link section 3.4: the key the Core sends as `identity`
  `publicKey` in its `hello` and in `pair.accept`. It is never derived from
  the TLS certificate's key, which is a different key and may change.
- The prefix is the 27 ASCII bytes `NereusSDR rendezvous id v1` followed by
  one line feed (0x0A).
- `base32` is RFC 4648 base32 with the standard alphabet, lowercased, with
  no padding. The id is its first 26 characters (130 of the digest's 256
  bits), so it matches `[a-z2-7]{26}`.

The id is fixed for the life of the key (the pairing design, section 5.2:
rotation is deferred). A client that paired with a Core holds its key from
`pair.accept` and derives the id itself. This section is the only
definition of the id; the pairing design points here.

`rendezvous/conformance/v1/crypto/rendezvous-id.json` gives keys, their
digests and their ids.

### 4.3 The registration proof

After `register`, the service sends a `challenge` whose `nonce` is 32 bytes
from the operating system's random generator, new for every registration.
The station signs, with its station identity key:

```
"NereusSDR rendezvous register v1\n" || nonce
```

where `nonce` is the **32 raw bytes** the challenge's base64url decodes to,
not its text. The prefix is 33 ASCII bytes ending in one line feed. The
transcript is 65 bytes. `crypto/register-proof.json` gives a fixed public
key, a nonce, the transcript bytes and valid and invalid signatures.

### 4.4 The introduction signature

Every connection's `hello` carries its own `nonce`, 32 random bytes, new per
connection. A client introducing itself on that connection signs, with its
device key:

```
"NereusSDR introduce v1\n" || station id || nonce
```

where `station id` is the 26 ASCII bytes of the station's rendezvous id
(section 4.2) and `nonce` is the **32 raw bytes** the connection's `hello`
`nonce` decodes to, not its text. The prefix is 23 ASCII bytes ending in one
line feed; the transcript is 81 bytes.

The service does not verify this signature: it cannot know which devices a
station has paired. It forwards `device` and `deviceSignature` untouched and
adds the `nonce` of the introducing connection from its own record, never
from anything the client sent. The station verifies the signature with the
key of the paired device whose id is `device`, and answers nothing when the
device is not paired, revoked, or the signature does not verify (iPhone app
plan Task 27). Binding the nonce stops an observer who saw one introduction
from replaying it on a connection of its own; binding the station id stops a
signature for one Core being shown to another.

An accepted introduction is **not device authentication**. It is only a
reason for the Core to spend an ICE attempt on the client. The service
chooses the nonce and sees every signature, so a service that misbehaves
could show a signature to the Core again; and one connection keeps its
nonce for every introduction it makes, one after another, so a signature
is not bound to a single introduction. The session that runs over the ICE
connection authenticates the device again with its own key (the station
link's device authentication), and the Core admits a device on that alone.

`crypto/introduce-signature.json` gives a fixed device public key, a
station id, a nonce, the transcript bytes and valid and invalid
signatures, including one over the nonce's text.

## 5. Messages

### 5.1 The envelope

Each message is a JSON object with a string `type`. Keys are case
sensitive. The rules for every message the service receives:

- Every key its kind lists must be present with the kind and length
  section 5.2 gives, or the message is a protocol error.
- A key its kind does not list is ignored, and never forwarded: the service
  builds each forwarded message from the listed keys alone.
- A kind the service does not know, a kind of the other role, JSON that
  does not parse, a top level that is not an object, a missing or non-string
  `type`, a key that appears twice in one object, `NaN` or `Infinity`, and a
  string in a listed key holding a lone surrogate (an escape such as
  `\ud800` that is not a Unicode scalar value) are protocol errors. A lone
  surrogate in a key the kind does not list is ignored with that key.
- A whole number is a JSON number written without a fraction or an
  exponent. `true` and `false` are not numbers, and `7.0` and `"7"` are not
  whole numbers.

A string's length is counted in bytes of its UTF-8 encoding. "Forwarded
unchanged" means the forwarded string is the same sequence of Unicode scalar
values, so the same UTF-8 bytes; the escapes a sender chose (`\u00e9` for
`é`) are not preserved, because JSON text is re-encoded, and nothing a
receiver decodes differs.

The service encodes compactly, keys in the order the tables below list
them, and writes non-ASCII characters as UTF-8 rather than escapes.
Receivers must not depend on key order.

**At a station or client.** A station or client decodes what the service
sends by the same tables, with the same tolerance:

- A key it does not know, in a kind it knows, is ignored (and so is a key it
  does not know inside a `turn` object). The message is otherwise decoded as
  usual.
- A kind it does not know is ignored, as is a message it cannot decode: it
  logs it and carries on (section 5.3).

A later version relies on this to add keys to the service's messages
(section 11). The control fixtures hold messages to a station and to a
client that carry keys of no kind listed here, and they decode (section
10.3).

### 5.2 Field kinds

| Kind | Meaning |
| --- | --- |
| `rid` | a rendezvous id: exactly 26 characters of `a-z` and `2-7` (section 4.2) |
| `key` | a public key: strict base64url of exactly 91 bytes, 122 characters (section 4.1); whether it is a canonical P-256 key is checked after decoding |
| `sig` | a signature: strict base64url of exactly 64 bytes, 86 characters |
| `device` | a device id: strict base64url of exactly 32 bytes, 43 characters |
| `nonce` | strict base64url of exactly 32 bytes, 43 characters |
| `intro` | an introduction id: strict base64url of exactly 16 bytes, 22 characters, chosen by the service at random; opaque, never an address |
| `sdp` | a string of 1 to 65536 bytes (UTF-8): an SDP offer or answer, passed on untouched |
| `candidate` | a string of 0 to 4096 bytes: the empty string, which is the end of candidates, or one ICE candidate as the value of RFC 8839's `candidate` attribute, starting `candidate:` (for example `candidate:1 1 UDP 2122317823 2001:db8::7 50123 typ host`); passed on untouched. It never carries the `a=` of an SDP line: a sender whose ICE library gives `a=candidate:...` removes the `a=` before sending, and a candidate that does not start `candidate:` is a protocol error (the service refuses one and closes, and a peer ignores one from the service as a message it cannot decode) |
| `body` | a string of 1 to 65536 bytes: one mailbox message, passed on untouched |
| `nameplate` | a whole number from 1 to 999999 |
| `bool` | `true` or `false` |
| `version` | a whole number from 1 to 65535 |
| `code` | 1 to 64 ASCII letters (`A-Z a-z`); a receiver treats a code it does not know as the generic form of its message |
| `reason` | a string of 1 to 1024 bytes: plain words an app may show as sent |
| `retry` | a whole number from 0 to 2147483647: milliseconds to wait before trying again; 0 means no advice |
| `urls` | an array of 0 to 8 strings, each 1 to 512 bytes: STUN or TURN URLs (RFC 7064, RFC 7065) |
| `turn` | `null`, or an object with these keys: `username` (a string of 1 to 512 bytes), `password` (a string of 1 to 128 bytes), `expires` (a whole number from 0 to 4294967295, Unix seconds) and `urls` (`urls`); section 8. The service sends exactly these; a receiver ignores any other key in it (section 5.1) |
| `relayUrl` | a string of 1 to 512 bytes of printable ASCII (0x21 to 0x7E), starting `wss://`: where the WebSocket relay is (section 12.1) |
| `relayToken` | a string of 1 to 512 bytes of the base64url alphabet (`A-Z a-z 0-9 - _`), no padding: a relay grant's token, opaque to a station and a client (section 12.2; version 1 tokens are 83 characters) |
| `expires` | a whole number from 0 to 4294967295: Unix seconds |

### 5.3 The kinds

**From a station to the service:**

| Kind | Keys | When |
| --- | --- | --- |
| `register` | `id` (`rid`), `publicKey` (`key`) | first message; section 6.2 |
| `prove` | `signature` (`sig`) | after `challenge` |
| `answer` | `to` (`intro`), `answer` (`sdp`), `turn` (`bool`) | once per introduction it accepts |
| `candidate` | `to` (`intro`), `candidate` (`candidate`) | after its `answer` to that introduction |
| `nameplate.claim` | none | registered |
| `nameplate.release` | none | registered |
| `mailbox` | `body` (`body`) | while a mailbox is open on its nameplate |
| `mailbox.close` | none | while a mailbox is open on its nameplate |

**From a client to the service:**

| Kind | Keys | When |
| --- | --- | --- |
| `introduce` | `id` (`rid`), `device` (`device`), `deviceSignature` (`sig`), `offer` (`sdp`) | no live introduction on this connection |
| `candidate` | `candidate` (`candidate`) | while its introduction is live |
| `mailbox.open` | `nameplate` (`nameplate`) | no open mailbox on this connection |
| `mailbox` | `body` (`body`) | while its mailbox is open |
| `mailbox.close` | none | while its mailbox is open |

**From the service to a station:**

| Kind | Keys | Sent |
| --- | --- | --- |
| `hello` | `version` (`version`), `nonce` (`nonce`), `stun` (`urls`) | first, on every connection |
| `challenge` | `nonce` (`nonce`) | after `register` |
| `registered` | `id` (`rid`) | after a valid `prove` |
| `introduction` | `from` (`intro`), `device` (`device`), `deviceSignature` (`sig`), `offer` (`sdp`), `nonce` (`nonce`) | a client introduced itself to this id |
| `credentials` | `from` (`intro`), `turn` (`turn`) | after the station's `answer` with `turn` true |
| `relay.grant` | `from` (`intro`), `url` (`relayUrl`), `token` (`relayToken`), `expires` (`expires`) | just after `credentials`, when the service holds a relay secret (section 12.1) |
| `candidate` | `from` (`intro`), `candidate` (`candidate`) | the client sent one |
| `introduction.end` | `from` (`intro`), `code` (`code`): `clientLeft` or `expired` | the introduction ended |
| `nameplate` | `nameplate` (`nameplate`) | after `nameplate.claim` |
| `nameplate.released` | none | after `nameplate.release` |
| `mailbox.opened` | `nameplate` (`nameplate`) | a client opened the mailbox on its nameplate |
| `mailbox` | `body` (`body`) | the client sent one |
| `mailbox.closed` | `code` (`code`): `closed`, `peerClosed`, `peerLeft` or `expired` | the mailbox closed |
| `error` | `code` (`code`), `reason` (`reason`), `retryAfterMs` (`retry`) | section 7 |

**From the service to a client:**

| Kind | Keys | Sent |
| --- | --- | --- |
| `hello` | `version` (`version`), `nonce` (`nonce`), `stun` (`urls`) | first, on every connection |
| `answer` | `answer` (`sdp`), `turn` (`turn`) | the station answered |
| `relay.grant` | `url` (`relayUrl`), `token` (`relayToken`), `expires` (`expires`) | just after `answer`, when the station's `answer` had `turn` true and the service holds a relay secret (section 12.1) |
| `candidate` | `candidate` (`candidate`) | the station sent one |
| `introduction.end` | `code` (`code`): `stationLeft` or `expired` | the introduction ended |
| `mailbox.opened` | `nameplate` (`nameplate`) | after `mailbox.open` |
| `mailbox` | `body` (`body`) | the station sent one |
| `mailbox.closed` | `code` (`code`): `closed`, `peerClosed`, `peerLeft`, `released` or `expired` | the mailbox closed |
| `error` | `code` (`code`), `reason` (`reason`), `retryAfterMs` (`retry`) | section 7 |

A station or client logs and ignores a message it cannot decode or a kind it
does not know, and ignores a key it does not know in a kind it knows; the
service ends the connection on a message it cannot decode or a kind it does
not know, and ignores unknown keys (section 5.1). That is the station link's
rule (link section 13) applied here.

## 6. Flows

### 6.1 The greeting

```
service -> any:  {"type":"hello","version":1,"nonce":<nonce>,"stun":[<url>, ...]}
```

`version` is the rendezvous version the service speaks, 1 here. `nonce` is
32 random bytes, new for this connection, which an introduction on this
connection signs (section 4.4). `stun` lists the STUN servers from the
service's configuration, plain STUN needing no credentials; on
`rv.nereussdr.com` one IPv4-only and one IPv6-only name, in that order (section 8). A
client uses what it needs of the list (the pinned libjuice uses one STUN
server).

### 6.2 Registration

```
station -> service:  {"type":"register","id":<rid>,"publicKey":<key>}
service -> station:  {"type":"challenge","nonce":<nonce>}
station -> service:  {"type":"prove","signature":<sig>}
service -> station:  {"type":"registered","id":<rid>}
```

- Before it sends a challenge, the service checks that `publicKey` is a
  canonical P-256 key and that `id` is the id derived from it (section 4.2).
  Either failing is `error` `proofFailed` and the close. Without the id
  check, anyone could register anyone's id.
- `prove` must verify over the registration transcript with this
  connection's challenge (section 4.3), or it is `error` `proofFailed` and
  the close. Only a verified `prove` registers the id.
- A verified `prove` whose address group already holds its share of
  registered stations gets `error` `tooManyConnections`, and one when the
  service holds its total, `error` `overloaded`, each instead of
  `registered` and followed by the close (section 9.1). A registration that
  replaces an older one of the same id is never refused for the older
  one's slot.
- A second registration of the same id, on another connection, replaces the
  first: the older connection gets `error` `replaced` and the close, and its
  introductions, nameplate and mailbox end as when a station leaves
  (sections 6.3 and 6.5). A Core that restarts or moves networks therefore
  takes its id back at once.
- A `prove` before `register`, a second `register` on one connection, and
  any other station kind before `registered` are protocol errors.

### 6.3 Introductions

```
client  -> service:  {"type":"introduce","id":<rid>,"device":<device>,"deviceSignature":<sig>,"offer":<sdp>}
service -> station:  {"type":"introduction","from":<intro>,"device":...,"deviceSignature":...,"offer":...,"nonce":<the client connection's hello nonce>}
station -> service:  {"type":"answer","to":<intro>,"answer":<sdp>,"turn":true|false}
service -> client:   {"type":"answer","answer":<sdp>,"turn":<turn object>|null}
service -> client:   {"type":"relay.grant","url":<url>,"token":<device token>,"expires":<n>}   (turn true and a relay secret; section 12.1)
service -> station:  {"type":"credentials","from":<intro>,"turn":<the same turn object>|null}   (only when turn was true)
service -> station:  {"type":"relay.grant","from":<intro>,"url":<url>,"token":<Core token>,"expires":<n>}   (as above)
either  -> service -> other:  candidates, then the end of candidates
```

- **Only its own id.** An introduction goes to the connection registered
  under `id`, and to no other. When no connection is registered under `id`,
  whether the id was never seen, its station left, or its station has not
  finished registering, the client gets `error` `offline` (section 6.4).
- **The introduction id.** `from` (and the station's `to`) is 16 random
  bytes the service picks for this introduction, unique among live ones. It
  names the introduction; it is never an address.
- **Accepting.** The station answers only an introduction it accepts: the
  device is paired and the signature verifies (section 4.4). A station that
  does not accept stays silent; nothing tells the client, which waits for
  its own deadline or the introduction's lifetime. The station answers at
  most once per introduction; a second `answer`, or one naming an
  introduction that has ended, gets `error` `unknownIntroduction` and the
  connection stays.
- **Relay.** `turn` true in the station's `answer` asks for relay
  credentials: the station allows the relay (`relay = allow` in
  `nereusd.conf`, iPhone app plan Task 27). The service mints them then, and
  only then (section 8), and gives the same object to both ends: in the
  client's `answer` and in a `credentials` message to the station. When the
  service has no relay configured it sends `turn` null in both, so a station
  that asked always gets its `credentials` reply. With `turn` false the
  client's `turn` is null, nothing is minted and no `credentials` is sent.
  The same `turn` true, and only it, also has the service mint a relay
  grant for the WebSocket relay when it holds a relay secret: a
  `relay.grant` to the client just after its `answer` and one to the station
  just after its `credentials` (section 12.1).
- **Candidates.** Each end trickles its ICE candidates through the service,
  one `candidate` message per candidate, in section 5.2's form (starting
  `candidate:`, never `a=candidate:`), forwarded untouched. The
  client may send candidates as soon as its introduction is live; the
  station only after its `answer` (a station candidate before it is a
  protocol error). An empty `candidate` is the end of candidates and is
  forwarded like any other. At most 64 non-empty candidates per side per
  introduction are forwarded; the next gets `error` `tooManyCandidates` and
  is dropped, and the connection stays. The end of candidates does not
  count. A client candidate with no live introduction gets `error`
  `noIntroduction` (it can race the introduction's end), and a station
  candidate for an introduction that has ended gets `error`
  `unknownIntroduction`; the connection stays in both cases.
- **Ending.** An introduction ends when either end's connection closes (the
  other gets `introduction.end` `clientLeft` or `stationLeft`), when the
  station's registration is replaced (`stationLeft`), or when its lifetime
  runs out: 120 s after `introduce` by default, covering candidate
  gathering (up to 23.5 s) and ICE's 39.5 s connectivity timer with room to
  spare, when both ends get `introduction.end` `expired`. A client that has
  its session running simply closes its rendezvous connection. A second
  `introduce` while one is live is a protocol error.
- **What an end means to the Core.** `introduction.end` (`clientLeft`,
  `expired` or any other code) ends only the introduction: the Core stops
  sending that introduction's candidates and forgets its id. It never
  touches a session already running over the ICE connection the
  introduction set up, which ends only by its own rules (the station
  link). A client closing its rendezvous connection once its session runs,
  as above, is the ordinary case, and the Core then gets `clientLeft`. The
  same holds at the client for `stationLeft` and `expired`.
- **Order of the client's connection.** A client gathers with the relay
  credentials it receives in `answer`, so an offer carries no candidates of
  its own; both ends' candidates arrive by trickle. The station's
  credentials arrive in `credentials` just after its answer goes out.

### 6.4 Unknown and offline

`introduce` to an id with no registered station answers exactly these bytes,
whatever the reason:

```
{"type":"error","code":"offline","reason":"The Core is not reachable right now. Check that it is running and connected to the internet.","retryAfterMs":0}
```

An id that was never registered, one whose station left, and one whose
station is between `register` and `prove` are indistinguishable by content.
They are distinguishable from an online station that stays silent, by
timing: `offline` comes back at once, a silent station's refusal never
does. That is acceptable because it reveals nothing the rendezvous does not
already show: that an id is online is exactly what it must know to
introduce anyone, and the pairing design (section 5.2) lists it among what
the rendezvous sees. What it does not reveal is whether a given device is
paired, because the station's silence looks the same for an unpaired device,
a revoked one and a bad signature.

The rate limits count introductions to an id the same way whether or not
it is online (section 9.1), so they tell nothing either.

### 6.5 Nameplates and mailboxes

```
station -> service:  {"type":"nameplate.claim"}
service -> station:  {"type":"nameplate","nameplate":<n>}
client  -> service:  {"type":"mailbox.open","nameplate":<n>}
service -> client:   {"type":"mailbox.opened","nameplate":<n>}
service -> station:  {"type":"mailbox.opened","nameplate":<n>}
either  -> service -> other:  {"type":"mailbox","body":<the pair.* message's JSON text>}
one end -> service:  {"type":"mailbox.close"}
service -> it:       {"type":"mailbox.closed","code":"closed"}
service -> other:    {"type":"mailbox.closed","code":"peerClosed"}
```

- **Nameplates** are numbers from 1 to 999999, global to the service: the
  number in a pairing code (link section 3.6). A registered station claims
  one and gets the lowest number no station holds. A station holds at most
  one: a second `nameplate.claim` answers the number it already holds. It
  keeps it until it sends `nameplate.release` (answered with
  `nameplate.released`, also when it held none) or its connection ends.
  When all 999999 are held, a claim gets `error` `nameplatesExhausted` and
  the connection stays.
- **A mailbox** is the one conversation on a nameplate. `mailbox.open` on a
  nameplate no station holds gets `error` `nameplateUnknown`; on one already
  serving a mailbox, `error` `nameplateBusy`; in both cases the connection
  stays. On success both ends get `mailbox.opened`. A nameplate never serves
  two mailboxes at once. A second `mailbox.open` while this connection's
  mailbox is open is a protocol error.
- **Bodies.** `body` is the text of one link `pair.*` message (link section
  3.6), JSON as text, forwarded unchanged (section 5.1). The service does
  not read it. At most 32 bodies per side per mailbox are forwarded; the
  next gets `error` `tooManyMessages` and is dropped. A body with no open
  mailbox gets `error` `noMailbox`; both leave the connection open.
- **Closing.** A mailbox closes when either end sends `mailbox.close` (it
  gets `mailbox.closed` `closed`, the other `peerClosed`), when either end's
  connection ends (the other gets `peerLeft`), when the station releases
  its nameplate (the client gets `released`; the station gets only
  `nameplate.released`), or 300 s after it opened (both get `expired`). A
  `mailbox.close` with no open mailbox gets `error` `noMailbox`.
- **After a mailbox.** A closed mailbox frees the nameplate for the next
  one; the nameplate itself stays with the station until it releases it or
  leaves. So one pairing burns one code (link section 3.6), and the
  station's next code can keep its number.

The rendezvous learns nothing it could test guesses against (link section
3.6): the SPAKE2 exchange runs inside the bodies. Anyone can open a
mailbox on a guessed small number, which uses up the station's current
code; the per-address limit on `mailbox.open` (section 9.1) and the
station's pause of pairing through the service bound that. A NereusSDR
Core never closes its pairing window for codes burned through a mailbox:
five in a row pause pairing through the service for 1 minute, doubling each
time with no pairing in between, at most 60 minutes, while pairing on a
direct connection stays open (link section 3.6, the attempt ceiling).

## 7. Errors

`{"type":"error","code":<code>,"reason":<reason>,"retryAfterMs":<retry>}`
is the only error message. `code` is what a peer acts on; `reason` is the
exact text below, which an app may show as sent; `retryAfterMs` is the
value below, or for `rateLimited` the milliseconds until the attempt would
be allowed (at least 1). An error that closes is followed at once by the
close (code 1000, or 1001 for `shuttingDown`); the others leave the
connection open.

| Code | Closes | `reason` | `retryAfterMs` | Cause |
| --- | --- | --- | --- | --- |
| `protocolError` | yes | "A message could not be read, so the connection was closed. Updating the app or the Core may help." | 0 | section 5.1; a message out of turn or of the other role; a binary frame |
| `proofFailed` | yes | "The Core could not prove its identity, so it was not registered." | 0 | a key that is not a canonical P-256 key, an id not derived from the key, a `prove` that does not verify (section 6.2) |
| `replaced` | yes | "The Core registered again on another connection, so this one was closed." | 0 | the same id registered on another connection |
| `timeout` | yes | "The connection did not finish starting in time." | 0 | the handshake timer (section 3) |
| `idle` | yes | "The connection was closed because it was not being used." | 0 | the idle timer (section 3) |
| `tooManyConnections` | yes | "Too many connections are open from this network. Try again shortly." | 5000 | the per-address-group cap on connections, sent instead of `hello`; or the per-address-group cap on registered stations, sent instead of `registered` (section 9.1) |
| `overloaded` | yes | "The remote access service is busy. Try again shortly." | 5000 | the total cap on connections, sent instead of `hello`; or the total cap on registered stations, sent instead of `registered` (section 9.1) |
| `shuttingDown` | yes | "The remote access service is restarting. Try again shortly." | 5000 | the service is stopping |
| `offline` | no | "The Core is not reachable right now. Check that it is running and connected to the internet." | 0 | section 6.4 |
| `rateLimited` | no | "Too many attempts. Try again in a minute." | until allowed | section 9.1 |
| `noIntroduction` | no | "There is no connection attempt in progress." | 0 | a client candidate with no live introduction |
| `unknownIntroduction` | no | "That connection attempt has already ended." | 0 | a station's `answer` or `candidate` naming no live introduction of its own, or a second `answer` |
| `tooManyCandidates` | no | "Too many network addresses were offered for one connection attempt." | 0 | section 6.3 |
| `nameplateUnknown` | no | "No Core is showing that pairing code right now. Check the code and try again." | 0 | section 6.5 |
| `nameplateBusy` | no | "Another device is pairing with this Core right now. Try again shortly." | 5000 | section 6.5 |
| `nameplatesExhausted` | no | "Pairing codes are not available right now. Try again shortly." | 5000 | section 6.5 |
| `noMailbox` | no | "There is no pairing in progress on this connection." | 0 | section 6.5 |
| `tooManyMessages` | no | "Too many pairing messages were sent." | 0 | section 6.5 |

A peer treats a code it does not know as an error that may or may not
close; it reads the close when it comes. `offline`'s bytes never change
(section 6.4).

## 8. Relay credentials

The relay is coturn with its time-limited credentials (`use-auth-secret`,
`static-auth-secret`). The service and coturn share one secret; the service
mints:

```
expires  = now (Unix seconds) + 86400
username = "<expires>:<station id>"            e.g. "1800086400:m3xq..."
password = base64(HMAC-SHA1(secret, username)) standard base64, with padding
```

- `secret` is the bytes of coturn's `static-auth-secret` as written in its
  configuration (UTF-8); the service reads the same bytes from its secret
  file, without the trailing line end.
- The credentials are valid for 24 hours and are minted only when a station
  accepts an introduction with `turn` true (section 6.3). The WebSocket
  relay's grant is minted at the same moment, under the same condition, with
  its own secret (section 12). The same object
  goes to both ends, so the client and the station allocate with the same
  username. The username carries the station id so coturn's quotas and
  logs group by station, never by device.
- `urls` in the object lists the configured TURN servers. On
  `rv.nereussdr.com` that is an IPv4-only and an IPv6-only relay name, each
  on UDP 3478 and UDP 443 (the pinned libjuice resolves one address per TURN
  host, preferring IPv4, so each family needs a name of its own; the
  pairing design section 9.5 item 3 requires both families). The defaults
  are `rv4.nereussdr.com` and `rv6.nereussdr.com`, in this order:
  `turn:rv4.nereussdr.com:3478?transport=udp`,
  `turn:rv4.nereussdr.com:443?transport=udp`,
  `turn:rv6.nereussdr.com:3478?transport=udp` and
  `turn:rv6.nereussdr.com:443?transport=udp`, and the `stun` list of
  `hello` names the same two hosts on 3478, also IPv4 first:
  `stun:rv4.nereussdr.com:3478`, then `stun:rv6.nereussdr.com:3478`. The
  names are configuration.
- **Why IPv4 first.** The pinned libjuice takes one STUN server name
  (libjuice `agent.c:449-470`), so an end passes it the first it can use.
  STUN supplies the IPv4 server-reflexive candidate for the hole punch: a
  peer on an IPv4-only network behind NAT needs one from an IPv4 STUN
  server, or it has nothing but its private address to offer. A peer with
  IPv6 usually has a global IPv6 host candidate already, and a direct IPv6
  path comes from the ICE checks both ends send: behind a stateful IPv6
  firewall (most home routers) each end's own checks open its firewall
  toward the other, so the other end's checks then get through. So a
  NereusSDR end uses the STUN server on every media connection, the first
  one included, for the IPv4 candidate (the Core passes its STUN servers to
  its devices in `mediaStunUrls`, the station link document section 6.3).
  The IPv4-only name goes first in `stun`, and the TURN list keeps the
  same order so every list the service sends reads the same way. The
  service's built-in defaults, `rendezvous/server/rendezvous.conf.sample`
  and what `rendezvous/deploy/setup-server.sh` writes all list them in
  this order (`test_limits_config_transport.py` and `coturn-check.sh`
  check it).
- The order serves an end that takes the first entry; an end must still not
  depend on it. It chooses its STUN server, and the relay host it allocates
  on, by the address families it has: the first entry whose name resolves,
  on that end, to a family it has a usable address in, or the first entry
  when it has both families or cannot tell. An IPv4-only end behind NAT so
  takes the IPv4-only name whichever the service lists first (NereusSDR:
  `IceConfiguration`).
- **Quotas.** coturn's quotas per user (per username, so per station id)
  do not limit how much the relay is used in total: an id costs nothing
  (anyone can make a key, register it and answer its own introduction with
  `turn` true), so a user of the relay can have as many ids, and as many
  per-id quotas, as it likes. What bounds the relay is coturn's total
  quota, sized in slots (JJ's ruling, 2026-09-26): data use is watched,
  not capped. On `rv.nereussdr.com`:
  - `user-quota` 8 per station id: a session runs two ICE connections
    (control and media), libjuice allocates on each whenever credentials
    are configured, and each connection is up to 2 allocations at each end
    (one per address family), so a session is up to 4 allocations at each
    end and 8 relayed at both. Both ends share the station id's quota
    (coturn counts per username, observed by `coturn-check.sh`: an eighth
    allocation for one id is accepted, a ninth refused with 486). It only
    keeps one Core's sessions from taking the whole relay.
  - `total-quota` 128 slots by default (`RV_RELAY_SLOTS` in
    `setup-server.sh`, the one place it is set): about 16 sessions relayed
    at both ends at once, or 32 at one end.
  - `max-bps` 80000 bytes a second per allocation, each way, above the
    largest session shape (about 520 kbit/s).
  - `bps-capacity` = slots x `max-bps` = 128 x 80000 = 10240000 bytes a
    second. coturn reserves `max-bps` of it for each live allocation and
    refuses one when nothing is left, so it must never bind below the slot
    count; at this value it cannot. The peak it allows is 10.24 MB a
    second out (about 82 Mbit/s) with every slot full at full rate, about
    27 TB in 30 days; real use is far below that, since most sessions go direct
    and a relayed one rarely runs at its cap.
  - The monthly transfer figure (`RV_TRANSFER_GB_PER_MONTH`, default 1000)
    caps nothing. It is the threshold of a daily data-use report
    (`rendezvous/README.md`, "Data use"): a journal line a day with the
    server's outbound bytes so far this calendar month, and a warning
    once they pass it.
- **A relay outlives its credential.** Observed with Ubuntu 24.04's coturn
  4.6.1 (package `4.6.1-1build4`) and the configuration in
  `rendezvous/deploy/turnserver.conf`, by `rendezvous/tests/coturn-check.sh`:
  coturn checks a credential's expiry only when an allocation is made. An
  allocation made while its credential was valid goes on being refreshed
  (`Refresh`, answered with success), and goes on getting permissions
  that relay (`CreatePermission`), after the credential has expired; this
  holds through a stale-nonce challenge too (a `438` with a new nonce,
  after which the same credential is accepted), which coturn issues every
  600 s (`stale-nonce`). Only a new allocation with the expired credential
  is refused (`401`). So a session relayed through coturn is never cut at
  the credential's expiry; a client needs fresh credentials only to make a
  new allocation (after losing the old one, or for a new session), which
  is what decides Task 29's refresh timing. This is an observation of
  coturn, not part of the wire.

`crypto/turn-credentials.json` gives secrets, expiries, ids, usernames and
passwords, the passwords computed with `openssl dgst -sha1 -hmac`, not with
the service's code.

## 9. Limits, storage and logs

### 9.1 Limits

Every value is configurable (`rendezvous.conf`, `[limits]`); these are the
defaults. Every value must be at least 1, except that 0 turns the pings
off and leaves the kernel's socket buffer sizing (`socket_buffer_bytes`);
the service refuses to start otherwise.

**Address groups.** Every count and limit "per address" is kept per
address group: an IPv4 address by itself (an IPv4-mapped IPv6 address
counts as its IPv4 address), or an IPv6 /56. A /56 is what an ISP commonly
delegates to one household, and a host that owns a prefix can dial from
every address in it, so counting by /64 would let one household multiply
every limit by up to 256. (The station link counts its handshakes by /64,
link section 12.3; the rendezvous, open to the whole internet, is stricter.)

**Two pools.** Registered stations and every other connection are counted
apart, so clients can never use up the room stations need, nor the
reverse. A connection that has not registered (a client, or a connection
still starting, whatever it will become) counts in the connection pool; a
station moves from it to the station pool when its `prove` verifies, and
leaves the connection pool then. A registration that replaces an older one
of the same id (section 6.2) does not count the older one.

| Limit | Default | Why |
| --- | --- | --- |
| Introductions per address group | 30 a minute | the brief's figure; a person retrying by hand never meets it |
| Introductions per station id, per address group | 60 a minute | counted per station per network (JJ's ruling, 2026-09-25), so a flood from one network blocks only that network's introductions to the station, never another network's; counted for every id asked for, online or not (section 6.4) |
| Mailbox opens per address group | 10 a minute | each open can use up a pairing code; guessing nameplates is slow |
| Candidates per side per introduction | 64 | more than any real gathering, within the message caps |
| Bodies per side per mailbox | 32 | a pairing exchange is under ten each way |
| Connections per address group (the connection pool) | 16 | a household with several phones behind one address; `tooManyConnections` instead of `hello` beyond it |
| Registered stations per address group (the station pool) | 4 | a household runs one or two Cores; `tooManyConnections` instead of `registered` beyond it |
| Connections in total (the connection pool) | 256 | clients are brief (an introduction lasts at most 120 s, a mailbox 300 s, an idle client 30 s), so even 2000 Cores each reached a few dozen times a day keep well under a hundred at once; kept small because every connection in this pool can be made to hold an unfinished message in the service and in Caddy (below); every Core passes through it while it registers, so after a restart the Cores come back in turns, each `overloaded` one after its retry time; `overloaded` instead of `hello` beyond it |
| Registered stations in total (the station pool) | 2000 | JJ's ruling (2026-09-26): the service supports 2000 registered Cores at once; measured memory fits (below); `overloaded` instead of `registered` beyond it |
| Handshake timeout | 10 s | a registration is one round trip plus a signature |
| Idle timeout for clients | 30 s | a client with nothing pending has no reason to stay |
| Introduction lifetime | 120 s | gathering (23.5 s) plus ICE's 39.5 s timer, with room |
| Mailbox lifetime | 300 s | the pairing exchange, with Argon2id hashing on the Core, and a person typing |
| WebSocket ping interval and timeout | 20 s and 20 s | keeps a station's NAT mapping and Caddy's upstream alive; the station link uses 20 s too (link section 12.1) |
| Outbound queue per connection | 256 messages or 1048576 bytes (1 MiB), whichever comes first | a peer that stops reading is closed (1008, section 2) rather than buffered without bound; 1 MiB holds 16 of the largest messages |
| Outbound queues together | 33554432 bytes (32 MiB) | a budget for every connection's queue at once; a message that would pass it closes (1008) the connection holding the most queued bytes, and the next largest, until it fits, so the peer that stopped reading goes, not whoever happens to be sent to next (the recipient goes only when it is itself the largest) |
| One message's send | 30 s | a connection whose writer has spent longer than this on one message is closed (1008): a peer that stops reading stops the WebSocket pings too (on websockets 10.4 a ping waits behind the same blocked write), so the ping timeout alone would never end it; 30 s is far longer than any real message takes |
| Kernel buffers per connection | 16384 bytes each way (`socket_buffer_bytes`) | the receive and send buffers (SO_RCVBUF, SO_SNDBUF) of every connection, set by the service on its own sockets, so the kernel memory a connection can hold is bounded without changing any host setting; Linux doubles the value, so each connection holds at most 64 KiB in the kernel. Caddy is the only peer, on loopback, where such buffers cost no speed. 0 leaves the kernel's own sizing, which can grow each buffer to megabytes |

**Sizing the totals.** The rendezvous runs on a server of its own: the
service, coturn, the WebSocket relay (section 12) and a Caddy in front of
the service and the WebSocket relay, nothing else. Its
memory is a setup input (`RV_MEMORY_MB`, by default what the kernel
reports), and `rendezvous/deploy/setup-server.sh` works the limits out
from it with one formula, in one place:

```
reserve        = 256 MiB                       the system and coturn
Caddy          GOMEMLIMIT = (memory - 256) x 50%   a soft limit: Go collects
                                               garbage harder near it
the service    MemoryMax  = (memory - 256) x 35%   the kernel ends it past this
headroom       = the other 15%, out of which
the WebSocket relay  MemoryMax = 32 MiB + 2 MiB x its slots   (64 MiB at 16)
```

| Server | memory the kernel reports | Caddy GOMEMLIMIT | the service MemoryMax |
| --- | --- | --- | --- |
| 1 GB | about 961 MiB | 352 MiB | 246 MiB |
| 2 GB | about 1967 MiB | 855 MiB | 598 MiB |

The measurements come from `rendezvous/tests/memory-check.sh`: an
`ubuntu:24.04` container with Ubuntu's `python3-websockets` 10.4 on Python
3.12 and Caddy 2.11.4 from Caddy's repository running
`rendezvous/deploy/Caddyfile` with the GOMEMLIMIT above, the service behind
it, loaded through `wss://` with 2000 registered stations, then 256
clients, then 500 connections each holding all but one byte of a
131072-byte message (the most a peer can make the service hold on the way
in: the cap of section 2, `max_queue` 1, and the service reads each
message as it arrives), then 100 stations that stop reading (each sent
three introductions carrying a 60000-byte offer). Resident memory:

| | the service | Caddy, GOMEMLIMIT 384 MiB | Caddy, GOMEMLIMIT 896 MiB |
| --- | --- | --- | --- |
| at start | 27.9 MiB | 45.4 MiB | 45.2 MiB |
| per idle registered station | 26.3 KiB | 106.5 KiB | 113.2 KiB |
| per idle client | 25 to 26 KiB | 51.3 KiB | 34.0 KiB |
| per connection holding an unfinished message | 170 to 175 KiB | 87.3 KiB | 190.6 KiB |
| per stalled station, with its three clients | 310 to 331 KiB | 366.0 KiB | 483.6 KiB |
| both pools full (2000 and 256), idle | 85.7 MiB | 266.2 MiB | 274.7 MiB |
| then 500 unfinished messages | 168.8 MiB | 308.8 MiB | 367.8 MiB |

With the lower GOMEMLIMIT Caddy's garbage collector returns memory
sooner, which is most of the difference between the two Caddy columns.
The kernel's TCP memory for the whole host stayed under 10 MiB. coturn
holds 19 MiB at start (`coturn-check.sh`) and little per relay.

**At 1 GB.** Both pools full of idle connections take about 256 (reserve)
+ 266 (Caddy) + 86 (the service) = 608 MiB of about 961. A peer that
fills both pools with unfinished messages would need about 28 MiB + 2256 x
(172 + 32 + 64) KiB + 32 MiB, about 650 MiB, in the service alone (its
WebSocket write buffer, 32 KiB, `write_limit`; its kernel buffers, 64
KiB, below; the 32 MiB send budget); the service passes its 246 MiB
ceiling long before that, the kernel ends it, and systemd starts it again
in 5 s (`Restart=on-failure`). Every Core connects and registers again.
Caddy meanwhile was seen to hold 310 to 345 MiB, near its limit. So a 1 GB
server carries 2000 Cores in normal use with room to spare, and under
such an attack loses the service for seconds at a time, but not Caddy,
coturn or the host.

**At 2 GB, recommended for 2000 Cores.** The service's 598 MiB ceiling is
close to its whole worst case (650 MiB), Caddy's 855 MiB limit is far
above what it was seen to hold (under 430 MiB), and the total under that
attack (about 256 + 598 + 430 = 1284 MiB) leaves about 680 MiB of the 1967.
A 2 GB server rides out a peer filling both pools with at most a
restart of the service near the very end.

**The WebSocket relay beside them.** The relay is a process of its own
(section 12) so relay traffic can never starve registrations or take the
service's ceiling. Its ceiling comes from its slots, not the server's
memory, and out of the 15% headroom: 32 MiB for Python and the idle relay
plus 2 MiB a slot, 64 MiB at the default 16 slots. Measured with every one
of those 16 sessions stalled (every lane of both legs of each full, section
12.5), it held 34.4 MiB, plus at most 64 KiB of kernel buffers for each of
its 96 connections at the caps (64 waiting to join and 32 joined; 6 MiB):
about 41 MiB, well under 64. At 2 GB the ceilings together are 256
(reserve) + 855 (Caddy) + 598 (the service) + 64 (the relay) = 1773 MiB of
about 1967, and under the attack above about 256 + 598 + 430 + 64 = 1348,
leaving about 620. At 1 GB they are 256 + 352 + 246 + 64 = 918 of about
961, the relay taking 64 of the 105 MiB of headroom. Each relay leg is also
a connection Caddy holds (section 12.5), at most 96 more beside the
service's 2256, plus refused ones for at most 2 s while they close.

**What protects the rest.** The WebSocket relay's unit
(`rendezvous/deploy/nereus-relay.service`) has `OOMScoreAdjust=700`, the
service's (`rendezvous/deploy/nereus-rendezvous.service`) `500` and Caddy's
drop-in (`rendezvous/deploy/caddy-override.conf`) `-100`, so if the whole
host runs short the kernel ends the relay first and then the service,
which also frees every connection Caddy holds for them (relay legs join
again with their grants, section 12.4); Caddy's drop-in adds
`Restart=on-failure` with `RestartSec=2`, so a Caddy that is ended anyway
comes back at once. The service's unit raises its open-file limit to 8192
(`LimitNOFILE`): one descriptor per connection, and systemd's default of
1024 would stop it accepting before its pools fill (observed in
`memory-check.sh` before the limit was raised). The kernel holds each
connection's socket buffers, which the service fixes at 16384 bytes each
way (`socket_buffer_bytes`; Linux doubles it), at most 64 KiB a
connection; under systemd they are charged to the service's own memory.

**Caddy's memory is Caddy's.** Caddy's own memory, and the kernel's socket
buffers on its client-facing connections (which Linux sizes by itself, up
to megabytes for a peer that stops reading), sit in Caddy's cgroup. They
are bounded neither by the service's settings nor by its `MemoryMax`;
GOMEMLIMIT only makes Caddy collect garbage harder, and does not stop it
growing. That is why the rendezvous has a server of its own: on a server
shared with a website, a peer filling the pools would grow the Caddy that
also serves the website (see `rendezvous/README.md`, "Beside a website").
One vCPU is not the limit: the service's work is a JSON decode per
message and one signature check per registration.

The windows are sliding: a limit of N a minute refuses an attempt when N
were counted in the last 60 s, with `retryAfterMs` until the oldest leaves
the window. A refused attempt is not counted. An introduction counts
against its address group's limit and against its (station id, address
group) limit; it is refused, with the longer wait, when either is full.

The source address is the peer's, or, when the peer is a configured trusted
proxy (by default the loopback addresses, where Caddy connects from), the
last address in `X-Forwarded-For`, which is the one the proxy itself
appended. The service listens on loopback only, IPv4 and IPv6
(`127.0.0.1` and `::1`, port 8710 by default), so every peer is the proxy.

### 9.2 Timers and pings

The handshake and idle timers are section 3. The ping is the WebSocket
layer's: the service pings every 20 s and closes a connection whose pong is
20 s late. A station behind NAT needs traffic to keep its mapping; 20 s is
under every home router's TCP timeout and matches the station link's
heartbeat.

### 9.3 Storage

The service keeps everything in memory, and so does the WebSocket relay
(section 12.6). The service writes nothing to disk: no
station, id, key, address, introduction, nameplate or mailbox, no cache and
no log file. A restart forgets every registration, and stations register
again on their next connection (a station reconnects with its usual
backoff). The only files it reads are its configuration and its TURN secret
at start. `rendezvous/tests/test_nothing_on_disk.py` runs the service as its
own process with an empty working directory, `HOME` and `TMPDIR`, drives
every kind of exchange through it, and checks all three are still empty.

### 9.4 Logs

Logs go to standard error only; under systemd that is the journal. A log
line may name an event (registered, replaced, left, an introduction and how
it ended, a nameplate claimed or released, a mailbox opened or closed, an
error code) and the first six characters of a station id or introduction id.
It never holds a whole id, an address, a label, the TURN secret, a minted
username or password, an SDP, a candidate, a mailbox body, a nonce, a
signature, a public key or a nameplate number (the number is part of a
pairing code). The websockets library's own log records, which can carry
addresses, are switched off. The WebSocket relay's log follows the same
rules (section 12.6).

## 10. Conformance

`rendezvous/conformance/v1/` is the machine-readable half of this document.
Three runners read it: the service's (`rendezvous/tests/`), the Core's
station role (iPhone app plan Task 27) and the clients' (the desktop,
Task 27; the iPhone app, Task 27a). It follows the station link's
conformance format (link section 16) wherever that fits; the differences
come from there being three parties rather than two, and are named below.

### 10.1 The files

- `manifest.json`: `{"rendezvousVersions":[1],"fixtures":[{"id","file","kind"}]}`,
  `kind` one of `crypto`, `control` and `session`. Every file under
  `crypto/`, `control/` and `sessions/` is listed once. There is no
  `requires`: the rendezvous has no capabilities.
- `crypto/*.json`: the derivation and signature vectors (section 10.2).
- `control/*.json`: one message each (section 10.3).
- `sessions/*.json`: one scripted exchange each (section 10.4).
- `sdp/offer.sdp`, `sdp/answer.sdp`: the SDP texts a runner sends where a
  fixture says `"$sdp:offer:<name>"` or `"$sdp:answer:<name>"`.

No file in the suite holds a private key. `crypto/` holds two fixed
public keys (marked `fixed`) with fixed signatures. Those keys were
generated only to produce these vectors and are used by nothing else: no
Core, device or server holds them as an identity. A runner only verifies
with them. Every key that signs during a run (the stations that
register and the devices that introduce themselves in the session
fixtures) is made at run time.

### 10.2 Crypto vectors

| File | Holds | A runner checks |
| --- | --- | --- |
| `base64url.json` | `cases`: `{"text","valid","bytesHex"}` or `{"text","valid":false,"why"}` | its strict base64url decoder accepts exactly the valid texts, to those bytes |
| `p256-spki.json` | `cases`: `{"name","spkiHex","valid"}`: a canonical key, a compressed point, P-384, secp256k1, a point off the curve, a BIT STRING with unused bits, a trailing byte, a truncated key | its key check accepts exactly the valid one |
| `rendezvous-id.json` | `prefixHex` and `cases`: `{"publicKey","digestHex","id"}` | SHA-256 of the prefix and the key's DER is `digestHex`, and the id derived from it is `id` (section 4.2) |
| `register-proof.json` | `key` (`fixed`, `publicKey`, `id`), `nonce`, `transcriptHex`, and `cases`: `{"name","publicKey","signature","valid"}` | `id` is the id of `publicKey`, the transcript of `nonce` is `transcriptHex`, and a case is valid exactly when its key is a canonical P-256 key and its signature (strict base64url, 64 bytes, raw) verifies over it: the signature and its high-s twin (`s` replaced by n - `s`, section 4.1) are; another nonce, a flipped bit, another key, a DER signature, padding, a short signature, three bad keys, `r` of 0, `r` of n and `s` of n are not |
| `introduce-signature.json` | `device` (`fixed`, `publicKey`, `id`), `stationId`, `nonce`, `transcriptHex`, and `cases`: `{"name","signature","valid"}` | the transcript of `stationId` and `nonce` is `transcriptHex`, `id` is the fingerprint of `publicKey`, and a case verifies with the device's key exactly when it is valid: the signature and its high-s twin are; another nonce, another station, the nonce's text, a flipped bit, another device, `r` of 0, `r` of n and `s` of n are not |
| `turn-credentials.json` | `cases`: `{"secret","expires","stationId","username","password"}` | the username and password from section 8 |
| `relay-grant.json` | `prefixHex`, `stationPrefixHex` and `cases`: `{"name","secret","leg","sessionHex","stationId","stationHex","expires","token","valid":true}` or `{"name","secret","token","valid":false,"why"}` | for the service and the relay only (a station and a client treat a token as opaque): each valid token and station value is what section 12.2 gives for its secret, leg, session, station id and expiry, and each invalid token (a flipped bit, another secret, version 2, legs 3 and 0 with MACs that verify, 82 characters, padding, the standard alphabet, empty) is refused. The station values and tokens were computed with `openssl dgst -sha256 -hmac`, not with the service's code |

### 10.3 Control fixtures

`{"from":"station"|"client"|"server","to":"station"|"client","wire":{<the message>},"decodes":true|false}`.
`to` is present only when `from` is `"server"`, since a server message's
shape depends on which role receives it; a message from a station or a
client always goes to the service. This is the one difference from link
section 16.2's `{"from","wire","decodes"}`.

The receiving end decodes `wire`; when `decodes` is true it encodes the
result again and compares it with `wire` as JSON values, key order ignored,
on the keys the kind lists only (and, inside a `turn` object, on its four
keys only): a key the kind does not list is ignored by the receiver
(section 5.1) and takes no part in the comparison.
The service's runner decodes every fixture, in every direction, with the
service's decoder for that direction, and also sends each refusal from a
station or client to the running service, which must answer `error`
`protocolError` and close. A station's runner decodes the fixtures `to`
`"station"` and encodes those `from` `"station"`; a client's runner does the
same for `"client"`.

The fixtures cover every kind in each direction it travels (section 5.3):
eight from a station, five from a client, fourteen to a station and nine to
a client, with both ends of candidates, relay and no relay, and codes a
receiver does not know, and one message to a station
(`server-station-introduction-extra-keys`) and one to a client
(`server-client-answer-extra-keys`, also with an extra key in its `turn`)
carrying keys no kind lists, which decode, and a `relay.grant` to a client
with a key no kind lists (`server-client-relay-grant-extra-keys`). The
refusals: an `id` in capitals and one of 25
characters; a padded key, a key that is a number, a `register` without
`publicKey`; a 63-byte signature; `turn` as a string; an empty `answer`; an
`answer`, an `offer` and a `body` over 65536 bytes (one counted in
two-byte characters); a candidate over 4096 bytes, a null candidate, and a
candidate with the `a=` of an SDP line, from a station, from a client and
(for a client's decoder) from the service; a
`to` of 15 bytes; a station `candidate` without `to`; a `body` that is an
object; a device key where a device id belongs; a padded device signature;
an `introduce` without `offer`; nameplates 0, 1000000, `"7"`, `7.5` and
`true`; unknown kinds from each role and each role's kinds from the other;
a `type` that is a number; and, for a client or station decoder, a `turn`
that is a string or lacks `password`, a `hello` `version` that is a string,
an `introduction` without `nonce`, a negative `retryAfterMs`, a
`credentials` sent to a client and an unknown kind; a `relay.grant` whose
token is a number or padded, whose URL is `https:` or missing, one to a
station without `from` or with `expires` as a string, and a `relay.grant`
sent to the service by a station and by a client.

The 128 KiB transport cap is enforced before decoding, so no fixture holds
it; `test_control.py` checks it against the running service, and
`test_queues_and_sizes.py` checks the sender's rule of section 2 at its
worst case. A lone surrogate cannot be written portably in a JSON fixture
file, so `test_control.py` also holds section 5.1's rule for those.

### 10.4 Session fixtures

`{"runs":[<runners>],"serverSetup":{...},"pairedDevices":[<device names>],"steps":[<steps>]}`.
`pairedDevices` may be absent (no device paired). No other key is allowed.

**Runners.** `runs` names the runners that run the fixture:

| `runs` value | Runner | Plays |
| --- | --- | --- |
| `"service"` | the service's (`rendezvous/tests/runner.py`), which runs every fixture | every connection, against the real service it builds from `serverSetup` |
| `"core"` | the Core's station role (Task 27) | the service, towards its Core on the connection named `station` |
| `"app"` | a client's (the desktop's, Task 27; the phone's, Task 27a) | the service, towards its client on the connection named `client` |
| (relay runners) | the relay's own (`rendezvous/tests/relay_runner.py`), and the Core's and the clients' relay legs (Task 29) | the frame protocol's fixtures in `relay/`, a set of their own: the relay's runner plays every leg against the real relay; a Core's relay-leg runner plays the relay towards its leg on the connection `core`, a client's towards its leg on `device`, each running the fixtures whose `runs` names it (section 12.8) |

**Connections.** A step names a connection. A name starting `station` is a
station connection and one starting `client` is a client connection:
`station`, `station2`, `client`, `client2` and so on. A core runner plays
only `station`, an app runner only `client`; other connections exist only
in the service's run.

**Steps.** One of:

- `{"connect":"<conn>"}` or `{"connect":"<conn>","address":"<ip>"}`: the
  connection opens. The service's runner connects to the service and sends
  `address` (default `192.0.2.1`) as `X-Forwarded-For`, so fixtures can
  place connections on different networks. A core or app runner starts its
  client here, in the role the connection's name gives; the next behaviour
  step says what the client is asked to do.
- `{"from":"server","to":"<conn>","message":{...}}`: the service sends this
  to that connection. The service's runner matches it against the next
  message the connection received. A core or app runner sends it, filled,
  when `to` is its connection, and otherwise only records its placeholders.
- `{"from":"<conn>","role":"behaviour"|"scripted","message":{...}}`: the
  connection sends this. Roles mean what they mean in link section 16.3: a
  behaviour message is one the client under test must produce (a core or app
  runner drives its client to it and matches what it sends); a scripted one
  is a message a conformant client never sends, which a core or app runner
  skips while it gives its client the service's answers that follow. The
  service's runner sends both, filled.
- `{"advanceMs":N}`: time moves N ms. The service's runner waits until the
  service has read everything sent so far, then moves the service's clock,
  firing its timers in order. A core or app runner moves its client's clock
  if it keeps one.
- `{"disconnect":"<conn>"}`: that end closes its connection. A core or app
  runner drives its own client to close; for another connection it does
  nothing.
- `{"expectClosed":"<conn>","code":<close code>}`: the service has closed
  the connection with that WebSocket close code (1000 after an error that
  closes, section 2), with no message between the last one listed and the
  close. The service's runner checks the code in the close frame it
  receives. A core or app runner closes its connection there with that
  code, and its client must send nothing after it.
- `{"expectSilent":"<conn>"}`: nothing more has arrived at that connection.
  The service's runner waits until the service has read everything sent and
  emptied its queues, then checks that nothing is waiting (a closed
  connection receives nothing, so it passes too). A core or app runner
  checks instead that its client sends nothing within 1 s of real time,
  whichever connection is named: that is how a Core's silence towards an
  unpaired device is held (`introduce-unpaired-device`).

A step holds no other key. The service's runner also decodes every message
the service sends with the receiving role's decoder, and fails on a key the
role does not know.

**`serverSetup`** holds the service's configuration for the fixture;
absent keys have these defaults:

| Key | Default |
| --- | --- |
| `stunUrls` | `["stun:rv4.conformance.invalid:3478","stun:rv6.conformance.invalid:3478"]` |
| `turnUrls` | `["turn:rv4.conformance.invalid:3478?transport=udp","turn:rv6.conformance.invalid:3478?transport=udp"]` |
| `turn` | `true`: a TURN secret is configured (the runner makes one at run time); `false`: none |
| `turnTtlSeconds` | 86400 |
| `relay` | `false`: no relay secret, so no `relay.grant`; `true`: a relay secret is configured (the runner makes one at run time, apart from the TURN secret) |
| `relayUrl` | `"wss://rv.conformance.invalid/v1/relay"` |
| `relayTtlSeconds` | 120 |
| `wallClock` | 1800000000: the Unix time when the fixture starts; it moves only with `advanceMs` |
| `introductionsPerAddressPerMinute`, `introductionsPerStationPerMinute`, `mailboxOpensPerAddressPerMinute`, `candidatesPerSide`, `mailboxMessagesPerSide`, `connectionsPerAddress`, `stationsPerAddress`, `maxConnections`, `maxStations`, `handshakeTimeoutMs`, `idleTimeoutMs`, `introductionLifetimeMs`, `mailboxLifetimeMs` | section 9.1 |

A core or app runner reads `stunUrls`, `turnUrls`, `turnTtlSeconds`,
`relayUrl`, `relayTtlSeconds` and `wallClock` to fill the service's
messages, and ignores the rest. Every fixture written before section 12
leaves `relay` at its default, so its wire is exactly what it was.

**`pairedDevices`** names the devices (as in `"$device:<d>:id"`) the Core
has paired before the fixture starts. The core runner makes their keys at
run time and pairs them into its Core by its own means; a device not listed
is not paired. The other runners ignore it.

**Placeholders.** A message may hold placeholders in place of values, each
a JSON string. Those of link section 16.1 that apply keep their meaning:
`"$any"`, `"$string"`, `"$string:<name>"`, `"$int"`, `"$int:<name>"`,
`"$capture:<name>"` and `"$ref:<name>"`, with names recorded once per run
and recorded again by a later placeholder of the same name. The rest are
the rendezvous's own. "Fill" is what a runner puts in a message it sends,
"match" what it checks in a message it receives; a runner also fills the
placeholders of a message on a connection it does not play, to record
them, without sending anything.

| Placeholder | Fill | Match |
| --- | --- | --- |
| `"$b64:<n>:<name>"` | `<n>` random bytes, base64url; recorded | strict base64url of exactly `<n>` bytes; recorded |
| `"$key:<k>:id"`, `"$key:<k>:publicKey"` | the rendezvous id, or the base64url SPKI, of station key `<k>` | equal to that value |
| `"$device:<d>:id"` | the device id (link section 3.5) of device key `<d>` | equal to that value |
| `"$register:<k>:<nonce>:<case>"` | a signature by station key `<k>` over the registration transcript of the nonce recorded as `<nonce>` (section 4.3); `<case>` is `signed`, `otherNonce` (over a random nonce instead) or `flippedBit` (the last bit of `s` flipped) | equal to the value filled for this placeholder |
| `"$introduce:<d>:<k>:<nonce>:<case>"` | a signature by device key `<d>` over the introduction transcript of station key `<k>`'s id and the nonce recorded as `<nonce>` (section 4.4); cases as above | equal to the value filled for this placeholder |
| `"$sdp:offer:<name>"`, `"$sdp:answer:<name>"` | the text of `sdp/offer.sdp` or `sdp/answer.sdp`; recorded | a non-empty string; recorded |
| `"$candidate:<name>"` | a host candidate of the runner's own, in section 5.2's form; recorded | a string that decodes as a `candidate` (section 5.2); recorded |
| `"$turn:<k>:<name>"` | credentials minted as section 8 with the runner's own secret, for station key `<k>`, expiring at `wallClock` plus the seconds advanced plus `turnTtlSeconds`, with `turnUrls`; recorded | exactly that object: the username is `<expires>:<id of k>`, the password the HMAC-SHA1 of it under the secret the runner gave the service (computed by the runner, not the service), `urls` the configured list; recorded |
| `"$relayToken:<leg>:<session>:<k>"` | a token minted as section 12.2 with the runner's own relay secret, for `<leg>` (`core` or `device`), in the session recorded as `<session>` (16 random bytes the first time the name is used), for station key `<k>`, expiring at `wallClock` plus the seconds advanced plus `relayTtlSeconds` | strict base64url of 62 bytes whose MAC verifies under the relay secret the runner gave the service (computed by the runner with HMAC-SHA256 directly), version 1, that leg, station key `<k>`'s station value, that expiry, and the session recorded as `<session>`; a name used for the first time records it and must hold a session no other name holds, so two introductions never share one |

Keys and devices are named by short names (`a`, `b`, `d`, `e`) and made at
run time, never written in a fixture; a key name and a device name live in
separate namespaces. Where a runner does not make a key itself, it takes
it from its client:

- A **core runner**'s station key is its Core's identity key: at the
  behaviour `register`, `"$key:<k>:publicKey"` and `"$key:<k>:id"` are
  checks (a canonical P-256 key; the id derived from it) that record `<k>`
  as that key, and `"$register:<k>:<nonce>:signed"` in its `prove` is a
  check that the signature verifies. Every device key is the runner's own.
- An **app runner** makes each station key and gives its client the one
  its behaviour `introduce` names as the Core it paired with. Its client's
  device key is `<d>` wherever the client's own behaviour messages name
  `"$device:<d>:id"`: the runner takes the public key from its client and
  checks the id is its fingerprint, and checks that
  `"$introduce:<d>:<k>:<nonce>:signed"` verifies over this connection's
  transcript. `"$sdp:offer:<name>"` in its client's `introduce` records
  whatever offer the client made.

A behaviour step's literal values are what the runner tells its client to
send (a nameplate number, a mailbox body), as in link section 16.3.

**The fixtures.**

| Fixture | Runs | What it holds |
| --- | --- | --- |
| `register` | service, core | hello, register, challenge, a valid proof, `registered` |
| `register-other-nonce`, `register-flipped-bit`, `register-other-key` | service | a proof over another nonce, with a flipped bit, or by another key: `proofFailed`, the close |
| `register-id-mismatch`, `register-key-not-p256` | service | an id not derived from the key, and a 91-byte key whose point is not on P-256: `proofFailed` before any challenge, the close |
| `register-prove-first`, `register-twice`, `station-verbs-before-register` | service | out of turn: `protocolError`, the close |
| `register-replaced` | service | a second registration of the same key replaces the first (`replaced`, the close) and receives the next introduction; the first receives nothing |
| `register-timeout`, `handshake-timeout` | service | no proof within 10 s, and no first message within 10 s (nothing at 9999 ms): `timeout`, the close |
| `introduce-answer-relay` | service, core, app | introduce, the introduction with the client connection's nonce and the device fields untouched, an answer with relay, the same credentials to both ends |
| `introduce-answer-direct` | service, app | an answer without relay: `turn` null, no `credentials` |
| `introduce-relay-off` | service | relay asked for on a service without a secret: `turn` null to both |
| `introduce-unpaired-device` | service, core | an introduction from a device the Core has not paired: forwarded, and nothing comes back |
| `introduce-signature-other-nonce`, `introduce-signature-flipped-bit` | service, core | a signature over another nonce, or with a flipped bit: forwarded untouched, and the Core stays silent |
| `introduce-candidates` | service | candidates both ways, the end of candidates, and the cap (2 here) on each side |
| `introduce-station-candidate-before-answer` | service | `protocolError` for the station; the client gets `stationLeft` |
| `introduce-answer-twice`, `introduce-answer-unknown` | service | `unknownIntroduction`, the connection stays |
| `introduce-offline` | service, app | `offline` for an id never registered |
| `introduce-unknown-equals-offline` | service | the same answer for a station that left and an id never seen (the byte comparison is `test_authorisation.py`) |
| `introduce-only-own-id` | service | with two stations registered, the introduction reaches only its own |
| `introduce-twice` | service | a second `introduce` while one is live: `protocolError`; the station gets `clientLeft` |
| `introduce-client-leaves` | service, core | the client closes: `introduction.end` `clientLeft` |
| `introduce-station-leaves` | service, app | the station closes: `introduction.end` `stationLeft` |
| `introduction-expires` | service, core, app | nothing at 119999 ms; `expired` to both at 120000 ms |
| `client-candidate-without-introduction` | service | `noIntroduction` |
| `client-idle` | service | a client with nothing pending: nothing at 29999 ms, `idle` and the close at 30000 ms |
| `client-idle-after-refusals` | service | refusals (`offline` at 0 and 20000 ms, `nameplateUnknown`) do not start the idle timer again: `idle` and the close at 30000 ms after the first |
| `introduce-answer-other-station` | service | with two stations registered, the other station's `answer` and `candidate` naming the first station's live introduction: `unknownIntroduction` twice, and nothing reaches the client or the first station |
| `introduce-rate-limit-address`, `introduce-rate-limit-station` | service | the per-address limit (2 here) with its `retryAfterMs`, allowed again when the window passes; the per-station, per-network limit (1 here), counted for an id that is offline: a second client on the same network is refused, one on another network is still admitted, and the first network is admitted again when the window passes |
| `nameplate-claim` | service, core | claim 1, claim again (1), release, release again, claim 1 |
| `nameplate-lowest-free` | service | 1, 2, 3; 2 released and 1's station leaves; the next claim gets 1, and 2 again after it |
| `mailbox-exchange` | service, core, app | open, `mailbox.opened` to both, bodies both ways (one with line ends, quotes, a backslash and characters outside ASCII) forwarded unchanged, close, `closed` and `peerClosed` |
| `mailbox-station-closes` | service, app | the station closes the mailbox; another client opens the next one on the same nameplate |
| `mailbox-busy`, `mailbox-unknown` | service (`mailbox-unknown` also app) | `nameplateBusy` with the station told nothing; `nameplateUnknown` |
| `mailbox-station-leaves`, `mailbox-client-leaves` | service, and app or core | `peerLeft`; the freed nameplate serves the next station, or the next mailbox |
| `mailbox-release` | service, app | the station releases its nameplate with a mailbox open: `released` to the client |
| `mailbox-expires` | service | `expired` to both at 300000 ms |
| `mailbox-without-open`, `mailbox-message-cap`, `mailbox-rate-limit` | service | `noMailbox` from each side; `tooManyMessages` at the cap (1 here); the per-address open limit (1 here) |
| `unknown-kind`, `client-claims-nameplate`, `station-introduces` | service | an unknown kind, and each role sending the other's: `protocolError`, the close |
| `connections-per-address`, `connections-per-ipv6-prefix` | service | the per-address-group cap (1 here) answered with `tooManyConnections` instead of `hello`; two addresses in one IPv6 /64, and two /64s in one /56, counted as one group, another /56 not; an IPv4-mapped address counted as its IPv4 address |
| `connection-pools` | service | a connection counts in the connection pool until it registers: the per-group cap (1) and the total (2) refuse others while the station is starting, and once it is registered a client from its address, and one more elsewhere, are admitted |
| `stations-per-address`, `stations-in-total` | service | the station pool's per-group cap (1 here, across one IPv6 /56) and its total (1 here): `tooManyConnections` or `overloaded` instead of `registered`, the close; a station on another /56 is registered, and a registration replacing the one that holds the slot is not refused |
| `introduce-relay-grant` | service, core, app | with a relay secret: an answer with `turn` true gives the client its `answer` and then a `relay.grant` with the device token, and the station its `credentials` and then a `relay.grant` naming the introduction, with the Core token of the same session, both with the configured URL and expiring 120 s on |
| `introduce-relay-grant-without-turn` | service | a relay secret and no TURN secret: `turn` null to both, and the grants all the same |
| `introduce-relay-grant-direct` | service, app | with a relay secret, an answer with `turn` false: no grant and no `credentials` |
| `introduce-relay-grant-unaccepted` | service, core | with a relay secret, an introduction the Core does not accept (a device it has not paired): no answer, so no grant to anyone |
| `introduce-relay-grant-per-introduction` | service | two introductions to one station, 5 s apart: two sessions, each grant expiring 120 s after its own answer |

### 10.5 Running the service's runner

```
python3 -m pytest rendezvous/tests -q
```

The authoritative run is inside an `ubuntu:24.04` container with Ubuntu's
`python3-websockets`, `python3-cryptography` and `python3-pytest`, which is
also what CI installs (the `rendezvous` job). The service runs on websockets
10.4 (Ubuntu 24.04, the legacy asyncio implementation) and on websockets 13
and later (the new one); `rendezvous/server/nereus_rendezvous/transport.py`
isolates the difference. The runner also alters one fixture in memory and
checks that the failure names the step and field that differ. The same run
holds the WebSocket relay to section 12 (`test_relay.py`,
`test_relay_queues.py`, `test_relay_grant.py`, and the relay's process in
`test_nothing_on_disk.py`).

## 11. Changing the rendezvous

- **The document, the vectors and the service change together**, in the same
  commit, and the service's runner passes. A change to a kind the Core or a
  client uses reaches their runners (Tasks 27 and 27a) before it ships.
- **Versions add, never change.** A later version adds kinds and keys and
  raises `hello` `version`; it never changes or removes what version 1
  defines, and `offline`'s bytes never change. A peer sends a kind or key
  added after version 1 only to a service whose `hello` `version` includes
  it, because an older service ends the connection on a kind it cannot
  decode (a key it does not know it ignores).
- **The service adds to what peers receive by the peers' tolerance.** A
  station or client ignores a key it does not know in a kind it knows, and a
  kind it does not know (section 5.1). A later service may therefore add
  keys to its messages, and new kinds, without asking which version a peer
  speaks; that is what this rule is for, and the extra-key control
  fixtures (section 10.3) hold every runner to it. A later service never
  changes a key a peer already knows.
- **Nothing on disk, nothing sensitive in the log** (section 9) is part of
  the wire's contract, not an implementation detail: a self-hoster relies
  on it.


## 12. The WebSocket relay

The last way to reach a Core, for a network that passes only web traffic:
TCP 443, perhaps through a proxy, and nothing on UDP. Each end opens one
WebSocket to the relay, a process of its own behind the service's name at
`wss://rv.nereussdr.com/v1/relay`, and the relay carries the datagrams of
the session's ICE connections between them. It is a datagram pipe, not a
byte stream: each message carries exactly one ICE, DTLS, SCTP or SRTP
datagram, so every reliable and every secure layer stays end to end, and a
leg that breaks and joins again costs only the datagrams sent meanwhile
(the relay's end of a session is named by its grant, never by an address).
The relay sees ciphertext, timing, sizes and the two ends' addresses, what
coturn sees (pairing design section 5.2).

Only the service's and the relay's side is here, with what an end must do
on the wire (section 12.3). How an end offers the relay to its ICE agent
(the relay's loopback address as a low-priority candidate in the same
agent) belongs to the Core's and the apps' work (iPhone app plan Task 29).

### 12.1 The grant

A relay grant is minted when, and only when, a station accepts an
introduction with the relay allowed: its `answer` with `turn` true (section
6.3), the moment TURN credentials are minted (section 8), when the service
holds a relay secret (`relay_secret_file`). `turn` true means "the relay is
allowed" (`relay = allow` on the Core) for both relays. So there is no
grant for an introduction the station never answers (a device it has not
paired or has revoked, a signature that does not verify), none for an
answer with `turn` false, and at most one per introduction (a second
`answer` is `unknownIntroduction`).

```
service -> client:   {"type":"answer","answer":<sdp>,"turn":<turn object>|null}
service -> client:   {"type":"relay.grant","url":"wss://rv.nereussdr.com/v1/relay","token":<the device's token>,"expires":<n>}
service -> station:  {"type":"credentials","from":<intro>,"turn":<turn object>|null}
service -> station:  {"type":"relay.grant","from":<intro>,"url":"wss://rv.nereussdr.com/v1/relay","token":<the Core's token>,"expires":<n>}
```

- `url` is where the relay is (`relay_url`; on the NereusSDR server
  `wss://<RV_HOST>/v1/relay`, the service's own name). Both ends get the
  same `url` and the same `expires`, and different tokens: each its own
  end's (section 12.2), both naming one session made for this
  introduction.
- `expires` is the Unix time until which the token can open its session,
  120 s after the answer by default (`relay_ttl_seconds`, the introduction
  lifetime). A session already open takes its legs back whatever the
  expiry (section 12.4).
- **When an end opens its leg.** At once, on receiving its `relay.grant`,
  without waiting for candidate gathering or anything else of the
  introduction: that is what takes back the relay's cold-start gap (the
  design's "opened at the introduction": the introduction accepted, since
  nobody has a grant before the Core accepts). An end that never needs the
  relay closes its leg once its session runs another way, or lets the relay
  end it as idle (section 12.4).
- An end treats `token` as opaque text: it sends it only to the relay at
  `url`, never logs it, and never puts it in the URL (a proxy may log the
  URL). A station and a client check it only as section 5.2's
  `relayToken`.
- **Who can get a grant.** Anyone who registers a station can answer
  introductions to it, including their own, with `turn` true, and so mint
  grants for themselves: an id costs nothing (section 8). A grant proves
  only that some registered station accepted some introduction. That is why
  the relay caps live sessions per station (2 by default) and in total (16
  by default), and rates per lane (section 12.5), as coturn's quotas bound
  TURN.
- **What an older end does.** `relay.grant` is a kind that version 1 ends
  do not know, so they log it and ignore it (sections 5.1 and 5.3), and
  nothing else in the exchange changes: the `answer` and `credentials`
  before it are byte for byte what they were. A service without a relay
  secret sends no `relay.grant` at all, so its wire is exactly version 1's.
  `hello` `version` stays 1: this is the service adding a kind to what it
  sends, which section 11 allows without asking which version a peer
  speaks, and a station or client never sends anything new to the service.
  An end simply uses the relay when a grant comes, and not otherwise. An
  older Core ignoring the grant leaves a newer client's leg alone in its
  session, which ends 30 s later (`peerGone`).
- **Why a kind of its own**, not a key in `answer` and `credentials`: the
  `answer` to a client already carries up to 131072 bytes of the station's
  answer and the `turn` object, 167 bytes under section 2's bound of 132096
  at the caps, with no room for a grant; and the grant's fixtures stand
  apart from the credentials' (section 10).

### 12.2 The token

```
station = HMAC-SHA256(relay secret, "NereusSDR relay station v1\n" || station id)[0..8]
payload = version (1 byte: 1) || leg (1 byte: 1 the Core, 2 the device)
          || session (16 bytes) || station (8 bytes)
          || expires (4 bytes, big-endian Unix seconds)
mac     = HMAC-SHA256(relay secret, "NereusSDR relay grant v1\n" || payload)
token   = base64url(payload || mac), no padding: 62 bytes, 83 characters
```

- The prefixes are the 24 ASCII bytes `NereusSDR relay grant v1` and the
  26 bytes `NereusSDR relay station v1`, each followed by one line feed
  (0x0A): hex `4e65726575735344522072656c6179206772616e742076310a` and
  `4e65726575735344522072656c61792073746174696f6e2076310a`. `station id` is
  the 26 ASCII bytes of the station's rendezvous id (section 4.2).
- `session` is 16 bytes from the operating system's random generator, new
  for every grant. The two tokens of a grant differ only in `leg` and the
  MAC.
- `station` is the same 8 bytes for every grant of one station and tells
  the relay nothing else: it cannot be turned back into the id without the
  secret. The relay counts live sessions per `station` (section 12.5).
- The relay secret is its own secret, never the TURN secret (the service
  refuses to start when they are equal; coturn's configuration holds the
  TURN secret in plain text, and nobody who reads it may mint grants). On
  the NereusSDR server it is 32 random bytes written as 64 hex characters
  in `/etc/nereus-rendezvous/relay-secret` (root only), made by
  `setup-server.sh` and handed to the service and the relay by systemd
  (`LoadCredential=`); the bytes are the file's text without its line end.
- **What the MAC binds.** The session, so the legs of one introduction
  pair only with each other and a token of one introduction is useless for
  another; the leg, so the relay knows the Core's leg from the device's by
  the token alone, whatever the connection claims, and two device legs
  never pair; the station value, which no end can change to escape its
  station's cap; and the expiry, which no end can extend.
- **How the relay checks a grant: the MAC, not a socket to the service.**
  The relay verifies a token with the secret alone, so the two processes
  share no state and no socket: the relay keeps admitting legs while the
  service restarts, and the reverse. Every rule that needs state (who is in
  a session, whether a session has ended, how many sessions a station has,
  section 12.4) is the relay's own, since it sees every join. A local socket
  would add a request and a dependency on the service for every join and
  serve no rule. The cost of a stateless token is that a relay restart
  forgets which sessions ended; the tokens of such a session can open it
  again until they expire (section 12.4), for the same two ends, which is
  harmless.
- The token names no station id, no device and no address.

`crypto/relay-grant.json` has valid and invalid tokens (section 10.2).

### 12.3 Frames

- One WebSocket per leg, over TLS on TCP 443, at the grant's `url`, opened
  as section 2 says (HTTP/1.1, Apple's `Upgrade: WebSocket` works, no
  WebSockets over HTTP/2). No subprotocol is offered and none is required;
  no `Origin` is required. The query is ignored. A plain request there gets
  `426` with a short text, as the service's does.
- Every message is one **binary** WebSocket message: its first byte is the
  **tag**, the rest the **payload**. The WebSocket message's own length is
  the length; there is no length field inside. A message is at most
  **1501 bytes** (the tag and 1500 bytes); a longer one closes the leg with
  1009 before it is read. A text message is a protocol error.
- **Tags:**

| Tag | Direction | Payload | Meaning |
| --- | --- | --- | --- |
| `0x00` | never | | not a tag: a protocol error |
| `0x01` | leg to relay to the other leg | one datagram, 1 to 1500 bytes | the control connection's ICE agent (the station link's control peer): the control lane |
| `0x02` | leg to relay to the other leg | one datagram, 1 to 1500 bytes | the media connection's ICE agent (the link's media peer): the media lane |

| `0x03` to `0x7F` | leg to relay | one datagram, 1 to 1500 bytes | kept for later streams. The relay of frame version 1 carries only 1 and 2: it drops (and counts) a datagram with any other data tag, and does not end the leg. An end sends one only to a relay whose READY version says it carries it, and drops (and counts) one it receives whose tag it does not know |
| `0x80` JOIN | leg to relay | the token, as ASCII | the first message on a leg, within 10 s of opening it |
| `0x81` READY | relay to leg | 2 bytes: the frame version (1), then 1 when the other leg is present or 0 | the join was accepted |
| `0x82` PEER | relay to leg | 1 byte: 1 the other leg joined (or joined again), 0 it left and its place is held (section 12.4) | |
| `0x83` END | relay to leg | 1 to 64 ASCII letters: the code (section 12.4) | the leg is ended; the close follows at once (1000, or 1001 for `shuttingDown`) |
| `0x84` to `0xFF` | relay to leg | | kept for later relay messages: an end ignores one it does not know |

When both media endpoints negotiate `mediaRelayRoutingVersion` 1 in the
station link, the tag-2 payload begins with the media connection's exact
16-byte RFC 4122 UUID, followed by 1 to 1484 bytes of the unchanged agent
datagram. The relay still forwards opaque tag-2 frames and enforces the
same 1501-byte total bound. A peer without that media declaration uses the
original raw tag-2 payload. The UUID routes concurrent media generations
at each endpoint; it does not replace end-to-end DTLS authentication.

- A leg's JOIN when joined, a data message with no payload, a tag of `0x00`,
  and any tag from `0x80` up that a leg sends other than a first JOIN, are
  protocol errors (END `protocolError`).
- **Lanes.** Tags 1 and 2 are two lanes. Each lane has its own rate cap in
  each direction and, towards each leg, its own bounded queue that drops
  its oldest (section 12.5), and the relay writes to a leg taking the lanes
  in turn. So a burst of control datagrams never spends media's budget,
  never evicts queued media and holds media back by at most one datagram
  at a time. Lanes, caps and queues belong to the session, so a leg that
  joins again keeps them.
- **What the relay reads of a datagram: its tag and its length, and
  nothing else.** It forwards the message as it came, tag included, to the
  other leg. It never reads, changes or keeps the payload beyond the lane's
  queue. `test_relay_queues.py` holds it to this with datagrams shaped like
  DTLS records, STUN and RTP that carry a known marker: the relay touches
  only their first byte and their length, queues the very object it
  received, and no log line holds the marker.
- Examples, in hex: a JOIN is `80` followed by the 83 characters of the
  token (84 bytes); READY with the other leg already there `81 01 01`;
  PEER, the other leg left, `82 00`; END for an idle session
  `83 69 64 6c 65` ("idle"); a DTLS 1.2 application-data record for the
  media connection `02 17 fe fd ...`. `conformance/v1/relay/` has every
  exchange byte for byte (section 12.8).

**What an end sends and reads.**

- It never sends a datagram of more than 1500 bytes: it drops one its ICE
  agent gives it rather than send it, since the relay would close the leg
  (1009). It never sends an empty datagram.
- It joins with any grant it holds, without judging `expires` itself (its
  clock and the relay's may differ): the relay answers `expired` when it
  is.
- It may send datagrams right after its JOIN, without waiting for READY:
  the relay reads a leg's messages in order, and forwards them once the
  join is accepted. Datagrams for a leg that is not there (the other end
  not joined yet, or away) are dropped.
- Reading, it takes what it knows of a message and ignores the rest:
  bytes after the ones this section defines for READY or PEER are ignored,
  and so is a READY whose version is above 1, read as version 1's two
  bytes.
- Any close without an END (1006, a close after a failed ping, 1008, 1009,
  a broken network) means: join again with the same token within 30 s
  (section 12.4). An END says what to do instead (the table in 12.4).
- Across a rejoin its shim keeps its loopback sockets and their addresses,
  the ones its ICE agents know as their candidates: that is what lets ICE
  carry on through a reset without noticing (the design's A.3). Only the
  WebSocket is new.
- It sets TCP_NODELAY on its leg and keeps a bounded queue in front of it
  that drops its oldest (section 12.5).

**One leg per end, both connections on it (the stream tag).** A session
runs two ICE connections, control and media, at each end. One leg per end
carries both, a tag telling them apart, rather than two WebSockets per end:
half the TLS connections and half the joins after a reset, and one grant
per session. The tag does not reach the ICE and DTLS demultiplexing at the
ends. Each end keeps one loopback UDP socket per ICE agent: a datagram read
from the control agent's socket goes out with tag 1 and one from the media
agent's with tag 2, and an arriving message's tag picks the socket its
payload is written to, so each agent receives exactly the bytes the other
end's agent sent. RFC 7983's demultiplexing by first byte (0 to 3 STUN, 20
to 63 DTLS, 128 to 191 RTP and RTCP) happens inside each agent on those
untagged datagrams, as it would over UDP. A tag is needed because both
agents send STUN and DTLS, so a datagram's first byte cannot say which
agent it belongs to; the ICE username fragment could, but only in STUN
messages and only by parsing them, which the relay must not do. One byte
is enough: two streams now and 125 more tags kept. Confirming this with
both agents on one leg, through libjuice and on iOS, is the ends' part
(the design marks the sharing as inferred until then); on the relay's side
the tests show both tags forwarded independently and unchanged.

### 12.4 Joining, rejoining and ending

- **Joining.** The relay checks the token (section 12.2): not a token this
  secret minted, `badToken`. Then:
  - its session is open: the leg takes its place, whatever the expiry,
    needing no slot and no place of its station's. An older connection still in that place is ended
    with `replaced` (after a reset the relay may not yet have seen the old
    TCP connection die; the newest connection wins);
  - its session is not open: `ended` when the session has ended (below),
    `expired` when `expires` has passed, `full` when every slot is taken,
    `tooManySessions` when its station already has its sessions (section
    12.5), and otherwise the session opens and the leg is its first.
  The leg gets READY; the other leg, when present, gets PEER 1.
- **Rejoining.** When a leg's connection ends (the peer closed, the network
  broke, or its writer blocked for 10 s on one message, which the relay
  closes with 1008), its place is held for **30 s**: the other leg gets
  PEER 0, and datagrams for the missing leg are dropped, not queued. A new
  connection joining with the same token within 30 s takes the place back
  (READY, and PEER 1 to the other), whatever the grant's expiry. A side
  that has not joined yet has the same 30 s from the session's opening.
  When the 30 s pass, the leg that is there gets END `peerGone` and the
  session ends.
- **Idle.** With both legs present, a session that forwards nothing for
  **30 s** ends (`idle` to both). ICE's consent checks (RFC 7675, about
  every 5 s) keep a session ICE is using alive; a session ICE did not
  choose goes quiet and frees its slot.
- **Once ended.** A session's tokens never open it again while they are
  unexpired (`ended`); after that `expired` answers them. The relay keeps
  this in memory; after a relay restart, a leg whose grant has not expired
  joins again and opens its session afresh, and after the expiry the ends
  need a new introduction.
- WebSocket pings every 20 s, closing a connection whose pong is 20 s late
  (section 9.2).

| END code | Close | When | What the end does | Words an app may show |
| --- | --- | --- | --- | --- |
| `protocolError` | 1000 | a text message, a first message that is not a JOIN, a bad tag or an empty datagram (section 12.3) | nothing more with this leg; the software is at fault | "The connection through the web relay failed. Updating the app or the Core may help." |
| `timeout` | 1000 | no JOIN within 10 s of opening | joins again with the same grant (a live session takes it whatever the expiry) | "The web relay did not answer in time. Trying again." |
| `badToken` | 1000 | the token is not one the relay's secret minted | does not use this grant again | "The web relay did not accept this connection. Try connecting again." |
| `expired` | 1000 | the session is not open and the grant's `expires` has passed | needs a new introduction | "The web relay's permission ran out. Try connecting again." |
| `ended` | 1000 | the grant's session has ended | does not use this grant again | "That relayed connection has ended. Try connecting again." |
| `full` | 1000 | every slot taken, or it waited longest when every waiting place was taken | tries again shortly with the same grant | "The web relay is busy. Trying again shortly." |
| `tooManyConnections` | 1000 | the per-address-group cap on connections | tries again shortly with the same grant | "Too many connections from this network to the web relay. Trying again shortly." |
| `tooManySessions` | 1000 | the station already has as many relayed sessions open as the relay allows | tries again shortly, or when one of the Core's other relayed sessions ends | "This Core already has as many connections through the web relay as it can. Try again shortly." |
| `replaced` | 1000 | a newer connection joined as this leg | does nothing: the newer one is its own | none |
| `peerGone` | 1000 | the other leg did not join, or come back, within 30 s | the session is over | "The other end left the web relay." |
| `idle` | 1000 | nothing forwarded for 30 s with both legs present | the session is over | none |
| `shuttingDown` | 1001 | the relay is stopping | joins again with the same grant (a live session is gone after a restart, so while the grant is unexpired) | "The web relay is restarting. Trying again." |

A leg is also closed with 1009 and no END for a message over 1501 bytes,
and with 1008 and no END when one of its messages has waited 10 s to be
written (it would not read an END either); either way the end joins again
(section 12.3). An end treats an END code it does not know as the session
being over.

### 12.5 Limits

Every value is configurable (`relay.conf`, `[limits]`); these are the
defaults. The message size is the wire's, not a setting.

| Limit | Default | Why |
| --- | --- | --- |
| Sessions at once (`slots`) | 16 (`RV_WS_RELAY_SLOTS`) | about as many as coturn's 128 slots carry relayed at both ends (section 8), and what one vCPU carries in Python: the prototype measured 0.027 to 0.042 CPU seconds a second per session, so 16 take about 0.4 to 0.7 of a vCPU before Caddy's TLS (inferred; to be measured on the server). `full` beyond it; a rejoin never needs a slot |
| Sessions at once per station (`sessions_per_station`) | 9 | Updated for the several-devices design: four current paths, four simultaneous reconnect introductions, and one fifth-device confirmation path. These transport sessions are separate from the Core's four admitted device places and coturn's allocation quota; the grant's `station` value counts them (section 12.2). `tooManySessions` beyond it; a rejoin never counts. The global 16-session cap remains |
| Rate, per session, per lane, each way | 80000 bytes a second (`rate_bytes_per_second`), a bucket holding one second's worth | coturn's `max-bps` (section 8) for each lane: a session may carry up to 2 x 80000 bytes a second each way. What passes a lane's cap is dropped (and counted), never delayed |
| Message | 1501 bytes: a tag and 1500 | one datagram; the ends' ICE, DTLS and SCTP stay under it |
| Send queue per lane towards each leg | 64 messages or 24576 bytes (`queue_frames`, `queue_bytes`), the oldest dropped first | about 0.3 s at a lane's cap: a leg that falls behind loses its oldest datagrams, which SCTP sees as loss and congestion control answers, instead of a delay that grows without bound. The writer takes the lanes in turn; one leg reading slowly never holds up the other |
| Write buffer past the queues | 8192 bytes (the websockets library's `write_limit`) | kept small because what has left the queue can no longer be dropped |
| One message's send | 10 s (`send_stall_ms`) | a leg that stops reading is closed (1008) and its place waits for a rejoin |
| Connections per address group | 36 (`connections_per_address`), joined or not, each counted from its arrival until it has closed, refused ones included | both primary ends and both optional watch ends of the nine transport paths above can share one public address group; groups as section 9.1. A refused or ended connection closes within 2 s (the close handshake's limit), so it cannot hold a place for long |
| Connections not yet joined, in total | 64 (`max_pending`) | a join takes one round trip. When a new connection comes and every waiting place is taken, the one waiting longest is ended (`full`) and the new one waits instead, so a few networks holding connections open cannot shut every join out |
| Join | 10 s (`join_timeout_ms`) | `timeout` |
| Rejoin, and the first join of the other side | 30 s (`rejoin_ms`) | a reset's reconnect is three round trips and a TLS handshake; ICE's consent freshness allows 30 s |
| Idle | 30 s (`idle_timeout_ms`) | frees the slot of a session ICE did not choose |
| WebSocket ping and timeout | 20 s and 20 s | as the service (section 9.2) |
| Kernel buffers per connection | 16384 bytes each way (`socket_buffer_bytes`) | set on every connection the relay accepts on its Unix socket; the send buffer is what the hop to Caddy can hold (above), and at most 64 KiB a connection sits in the kernel |
| Descriptors | 4096 (`LimitNOFILE` in the unit) | the connections above with a wide margin |

The idle and send checks are one lazily armed timer each: a session keeps
the time of its last datagram and its idle timer, when it fires, sets
itself again for what remains; a leg arms its send check when a send starts
and none is armed. A busy session costs one timer an idle period, not one a
datagram.

**The hop from Caddy to the relay is a Unix socket.** The relay listens on
`/run/nereus-relay/relay.sock` (`socket` in `relay.conf`), a Unix stream
socket of mode 0660 and group `caddy`, so Caddy's account may connect and
no other; it has no IP port. There is no peer address on a Unix socket, so
the relay takes the client's from the last `X-Forwarded-For` value, which
Caddy sets (`header_up X-Forwarded-For {remote_host}`), and answers a
request without one, or with one that is not an address, `400`. On Linux
what waits in a Unix stream is charged to its sender's send buffer, so the
relay's own SO_SNDBUF on each connection it accepts (`socket_buffer_bytes`,
16384) is what the hop towards Caddy can hold: about 15 KB, measured in
`ubuntu:24.04` by writing 1001-byte messages into a Unix stream nobody
reads until it would block (15015 bytes; 93093 with the kernel's default
buffers). Over
loopback TCP the same hop was Caddy's receive buffer, which the relay could
not size (about 80 KB in the measurement below).

**No Nagle anywhere.** A Unix socket sends at once. Caddy, between the
relay's socket and each end, is Go, whose TCP connections have TCP_NODELAY
on by default. The ends set it on their own legs, and keep a
bounded, drop-oldest queue of their own as well: the relay's queues cannot
help with what an end buffers.

**What waits after the relay's queues.** Between the relay and a slow
reader sit the library's write buffer, the Unix socket to Caddy, Caddy, and
Caddy's TLS connection to the end, and none of them drops. Measured by
`rendezvous/tests/caddy-check.sh` (step 8): a device leg sending 75
datagrams of 1000 bytes a second for 20 s to a Core leg that reads only 20
a second, through a blocking socket with a 16 KiB receive buffer:

| | how old the datagrams were when read, the largest in each 5 s | where bytes waited |
| --- | --- | --- |
| straight to the relay, no Caddy | 3.3, 3.0, 3.1, 3.1 s | |
| through Caddy, the hop over loopback TCP 8711 | 3.7, 7.3, 11.0, 12.2 s | Caddy's socket to the end 39 KB; Caddy's receive buffer from the relay 77 KB |
| as above, `net.ipv4.tcp_notsent_lowat` 16384 (the relay's commit `31107991`) | 3.6, 7.3, 10.4, 11.9 s | Caddy's socket to the end 13 KB; Caddy's receive buffer from the relay 82 KB |
| through Caddy, the hop over the Unix socket, `tcp_notsent_lowat` 16384 (this document) | 3.2, 4.5, 5.0, 4.5 s | the relay's own send buffer on the Unix socket, about 15 KB |

In every case the relay dropped datagrams for the reader (its queues work)
and the delay levelled off: at about 12 s with the hop over loopback TCP,
and at about 4.5 to 5 s over the Unix socket, 1.5 s above reading the
relay directly, which is what Caddy's own TLS connection to the reader
still holds. `setup-server.sh` sets
`net.ipv4.tcp_notsent_lowat = 16384` for the whole host
(`rendezvous/deploy/sysctl.conf`, with the line that undoes it), which
takes most of what waited in Caddy's socket towards the end, and the Unix
socket takes what waited between the relay and Caddy.

**Memory.** Measured with `rendezvous/tests/relay_memory_probe.py` in an
`ubuntu:24.04` container with Ubuntu's `python3-websockets` 10.4 on Python
3.12, the relay run as its own process as its unit runs it; resident
memory, each step on top of the one before:

| | 16 slots, 64 waiting (the defaults) | 128 slots, 256 waiting |
| --- | --- | --- |
| at start | 28.2 MiB | 28.2 MiB |
| with every waiting connection open | 28.8 MiB | 32.3 MiB |
| with every session open, idle | 29.2 MiB | 34.3 MiB |
| with every leg stalled, its peer sending in both lanes as fast as it can for 5 s | 34.4 MiB | 78.2 MiB |

A stalled leg holds up to about 180 KiB in the relay (the 128-slot run,
where Python was busy enough that every leg's buffers filled), plus at most
64 KiB of kernel buffers per connection. At the defaults that is about
34 MiB and 96 x 64 KiB (6 MiB), under the unit's MemoryMax of 32 MiB
+ 2 MiB x 16 = 64 MiB, which `setup-server.sh` writes in the drop-in
`nereus-relay.service.d/memory.conf`. Section 9.1 has the arithmetic beside
the service and Caddy. At the caps the relay forwards at most 16 sessions x
2 lanes x 80000 bytes a second each way, 2.56 MB a second in each
direction. `test_relay_queues.py` shows the queues never passing their
caps however long a reader stalls (3000 datagrams sent into a leg that
stopped reading: the queue at most 24576 bytes, the library's buffer at
its limit, and the reader, once back, getting what was sent less exactly
what the queue dropped, in order, ending with the newest), a control flood
neither evicting nor holding back media, and each lane's cap holding
through a rejoin.

### 12.6 Storage, logs and data use

- The relay keeps everything in memory and writes nothing to disk, as the
  service (section 9.3). It reads only its configuration and its secret, at
  start. `test_nothing_on_disk.py` runs it as its own process with an empty
  working directory, `HOME` and `TMPDIR` through a session, a rejoin and a
  stop, and checks all three are still empty.
- It logs to standard error only (the journal): a session opening and
  ending (with its counts), a leg joining, leaving or being replaced, and
  the data use below, with at most the first six characters of a session's
  id (its base64url). Never a whole session id, a token, a station value,
  the secret, an address, or anything a leg sends. The websockets library's
  records are switched off, as for the service.
- **Data use.** When a session ends the relay logs how long it ran, the
  frames and bytes it forwarded, and the datagrams it dropped over a lane's
  rate, from full queues, with no one at the other end and with a tag it
  does not carry. Once an hour it checks whether the UTC day has changed
  and, when it has, logs the finished day's total and sessions and the
  total since it started (and the day so far when it stops). The server's
  data-use report (`rendezvous/README.md`, "Data use") counts these bytes
  too, with everything else the server sends.

### 12.7 Deploying it

On the NereusSDR server `setup-server.sh` makes the relay secret, writes
`/etc/nereus-rendezvous/relay.conf` (the Unix socket
`/run/nereus-relay/relay.sock` with mode 0660 and group `caddy`, the slots,
the credential path) and the service's `relay_url` and `relay_secret_file`,
installs `nereus-relay.service`, its memory drop-in and the kernel setting
of `sysctl.conf`, and enables and starts the relay; Caddy's
`handle /v1/relay*` sends the relay's path to that socket
(`reverse_proxy unix//run/nereus-relay/relay.sock`) and everything else to
the service, as before. The relay's unit makes `/run/nereus-relay`
(`RuntimeDirectory=`) and gives the relay the group `caddy`
(`SupplementaryGroups=`), so it may give its socket that group; the unit
opens no IP socket (`RestrictAddressFamilies=AF_UNIX`). No new port
opens: the relay rides TCP 443 under the service's name
(`rendezvous/README.md`). On a server already running the service, the new
code goes first (`rendezvous/deploy.sh`), then `setup-server.sh`, which
refuses, in a dry run too, while the code in `/opt/nereus-rendezvous` is
from before the relay. `rendezvous/tests/caddy-check.sh`, `coturn-check.sh`
and `readme-check.sh` exercise it through Caddy and as the setup script
installs it.

### 12.8 Conformance for the frames

`rendezvous/conformance/v1/relay/` holds the frame protocol's exchanges,
byte for byte. It has a manifest of its own
(`relay/manifest.json`: `{"relayFrameVersions":[1],"fixtures":[{"id","file"}]}`)
and is not listed in `manifest.json`, so the rendezvous runners of section
10 never see it.

A fixture is `{"runs":[<runners>],"relaySetup":{...},"steps":[<steps>]}`
(`relaySetup` may be absent).

**Runners.**

| `runs` value | Runner | Plays |
| --- | --- | --- |
| `"relay"` | the relay's (`rendezvous/tests/relay_runner.py`, run by `test_relay_fixtures.py`) | every connection, against the real relay on a manual clock |
| `"core"` | a Core's relay-leg runner | the relay, towards its leg on the connection named exactly `core` |
| `"app"` | a client's relay-leg runner (the desktop's, the phone's) | the relay, towards its leg on the connection named exactly `device` |

Connections are named for their leg: `core`, `core2`, `device`,
`device2` and so on. A core or app runner plays only its own connection;
every other connection, and every step on one, exists only in the relay's
run, except that such steps' placeholders are still filled to record them
(as in section 10.4).

**How a core or app runner plays the relay.** It gives its leg a grant (the
token of the first `$token` of its own connection, with the relay's URL on
loopback), and has the leg's ICE agents behind its shim: a behaviour data
message from its leg is one the runner makes the leg send by writing the
payload into the loopback socket of the agent the tag names, then matches;
a data message to its leg is one the runner sends, then checks that the
leg wrote exactly the payload to that agent's socket (a datagram the leg
must drop, per the reader rules, must reach no socket). Then, step by step:

- `{"connect":"<conn>"}` (or with `"address":"<ip>"`, which only the
  relay's runner uses, as `X-Forwarded-For`): the leg opens a connection.
  On its own connection, the runner accepts the leg's next connection; a
  `connect` for a connection that has been closed or dropped is the leg
  connecting again (it may already have).
- `{"from":"<conn>","role":"behaviour","binary":[<parts>]}`: the leg must
  send this; the runner matches it. With `"role":"scripted"` (or `"text"`
  instead of `binary`), a message a conformant leg does not send, or need
  not send (such as a datagram pipelined before READY, section 12.3): the
  core or app runner skips it and goes on.
- `{"to":"<conn>","binary":[<parts>]}`: the relay sends this; the core or
  app runner sends it to its leg.
- `{"expectClosed":"<conn>","code":N}`: the relay closes the connection
  with that code; the runner does so.
- `{"disconnect":"<conn>"}`: that connection closes from its own side. On
  the runner's own connection the runner drives its leg to close (only the
  relay's run uses this on a leg under test's other side).
- `{"drop":"<conn>"}`: the connection breaks with no close at all. On its
  own connection the runner cuts the TCP connection without an END or a
  close frame; the leg must connect again and join with the same token
  (section 12.3), which the next steps hold.
- `{"advanceMs":N}`: time moves; the runner moves its leg's clock if it
  keeps one.
- `{"expectSilent":"<conn>"}`: on its own connection, the leg sends
  nothing and opens no new connection within 1 s of real time.
- `{"shutdown":true}`: the relay stops; the next steps give its END.
- After it has sent an END, a runner ignores whatever the leg sends on that
  connection (a leg may have pipelined a JOIN or datagrams before it read
  the END).

**Placeholders.** A message's `parts` are joined in order: two lowercase
hex digits a byte, or a placeholder. `"$token:<leg>:<session>:<station>"`
is a token (as ASCII) the runner mints with its own relay secret for that
leg (`core` or `device`), the session recorded under `<session>` (new the
first time), the station whose id is `<station>` repeated to 26
characters, expiring `grantTtlSeconds` (120) after the clock's time when
it is first filled; a fifth part `expired` makes it expire a second before
that time, and `forged` flips a bit of its MAC. A runner mints each
placeholder once, keyed by all its parts, and fills every later use of the
same key with the same token, so a leg that joins again (after a `drop`,
however far the clock has moved) joins with the very token it was given; a
fixture that wants a new token gives it a key of its own. `"$bytes:<n>:<name>"` is `n` random bytes when
sent, recorded, and when matched any `n` bytes, recorded; `"$ref:<name>"`
is the bytes recorded. `relaySetup` takes `slots`, `sessionsPerStation`,
`connectionsPerAddress`, `maxPending`, `joinTimeoutMs`, `rejoinMs`,
`idleTimeoutMs`, `wallClock` (1800000000) and `grantTtlSeconds`; a core or
app runner reads the last two and applies the rest to nothing.

**Which fixtures each runs.** The relay's runner runs every fixture whose
`runs` has `relay` and checks the shape of the rest, including that every
behaviour JOIN on a leg's own connection uses one token placeholder. A
`-core` fixture has the Core's leg under test on `core` and runs `core`,
and also `relay` except for the `reader-*` fixtures, which only a leg's
runner plays (the relay never sends what they hold); its `-device` twin
does the same with `app` and the device's leg on `device`.

| Fixture | Runs | What it holds |
| --- | --- | --- |
| `join-and-forward` | relay, core, app | READY `81 01 00` and `81 01 01`, PEER `82 01`, datagrams with tags 1 and 2 both ways, 1 to 1500 bytes |
| `idle` | relay, core, app | nothing at 29999 ms after the last datagram, END `idle` to both at 30000 ms |
| `end-expired-core`, `end-expired-device` | relay, and core or app | a grant already expired: END `expired` |
| `end-full-core`, `end-full-device` | relay, and core or app | the one slot taken by another station's session: END `full` |
| `end-too-many-sessions-core`, `-device` | relay, and core or app | `sessionsPerStation` 1 and its station's one session open: the second is refused, END `tooManySessions` |
| `end-too-many-connections-core`, `-device` | relay, and core or app | `connectionsPerAddress` 1 and another connection from the network: END `tooManyConnections` as soon as the leg connects |
| `end-shutting-down-core`, `-device` | relay, and core or app | END `shuttingDown`, close 1001 |
| `end-replaced-core`, `-device` | relay, and core or app | a newer connection joins with the leg's token: END `replaced` on the older, and the leg opens nothing new |
| `end-ended-core`, `-device` | relay, and core or app | the other side leaves; the leg's connection drops 20 s later; the session ends at 30 s; the leg's join after it gets END `ended` |
| `peer-gone-never-joined-core`, `-device` | relay, and core or app | nothing at 29999 ms, END `peerGone` at 30000 ms |
| `peer-gone-after-leaving-core`, `-device` | relay, and core or app | the other leg leaves (PEER `82 00`): nothing at 29999 ms, END `peerGone` at 30000 ms |
| `rejoin-after-close-core`, `-device` | relay, and core or app | the leg's connection drops with no END; it connects again and joins with the same token (READY `81 01 01`, PEER `82 01` to the other), and datagrams cross again |
| `unknown-data-tag-core`, `-device` | relay, and core or app | tags `05` and `7f` sent to the relay are dropped there; a tag 2 datagram after them is delivered |
| `data-before-peer-dropped-core`, `-device` | relay, and core or app | a datagram the leg sends before the other leg is there is dropped |
| `data-behind-join` | relay, app | a datagram pipelined right after JOIN (scripted: allowed, not required) is delivered once the join is accepted; a leg that waits for READY is given READY all the same |
| `reader-ready-trailing-byte-core`, `-device` | core or app | READY `81 01 01 ff`: the leg reads it as READY, other leg present, and delivers the datagram that follows |
| `reader-ready-version-2-core`, `-device` | core or app | READY `81 02 01`: read as version 1's two bytes; the datagram that follows is delivered |
| `reader-peer-trailing-byte-core`, `-device` | core or app | PEER `82 00 ff` and `82 01 ff` read as PEER 0 and 1; the datagram that follows is delivered |
| `reader-unknown-relay-tag-core`, `-device` | core or app | relay messages `84 00 01` and `ff` ignored; the datagram that follows is delivered |
| `reader-unknown-data-tag-core`, `-device` | core or app | data tags `05` and `7f` dropped and reach no agent; the tag 1 datagram that follows is delivered to the control agent |
| `end-timeout`, `end-bad-token` | relay | END `timeout` (no JOIN within 10 s, which a conformant leg never does) and `badToken` (a forged token) |
| `refuse-text`, `refuse-first-not-join`, `refuse-zero-tag`, `refuse-empty-payload`, `refuse-join-twice`, `refuse-oversize` | relay | END `protocolError` and the close, and for 1501 bytes of payload (1502 in all) the close 1009 with no END; the other leg gets PEER `82 00` |

`test_relay_fixtures.py` also checks that each END code a conformant leg
can meet (every code but `protocolError`, `timeout` and `badToken`) is
played towards both the Core's leg and the device's.


### 12.9 Optional independent transmit-watch relay

This extension is additive. It supplies a separate physical WebSocket flow for
an end-to-end encrypted, heartbeat-only connection. A relay grant does not
sign into a Core, obtain a device place, hold transmit, or authorize a key.
The Core requires a separate single-use attachment ticket inside its own
verified TLS/DTLS connection. A main connection and its watch must not share
one outer TCP flow. Core/client attachment and network-loss acceptance are
separate requirements; service/relay tests alone do not establish them.

**Negotiation and rollout.** `relay_watch_version` in `rendezvous.conf` defaults
to `0`. Set it to `1` only after the corresponding relay supports this section.
With both that setting and a relay secret, the service sends `hello.version: 2`
and the optional integer `watchRelayVersion: 1`; otherwise its hello remains
version 1 without the optional key. A supporting station includes
`watchRelayVersion: 1` in `register`, and a supporting device includes it in
`introduce`, only after seeing service version 2 and that declared capability.
Unknown keys remain ignored; a known optional key of the wrong type is refused.
Other declared watch versions do not negotiate version 1 implicitly.

Only after an introduction is accepted with `turn: true`, and both endpoints
explicitly declared watch version 1, does the service append `watchToken` to
each existing `relay.grant`. Its normal `token`, URL, expiry, order and role
remain unchanged. Each end receives only its own watch token. A disabled
service, unsupported/older endpoint, unanswered introduction, or refused relay
produces no watch token. No new message kind or second introduction is needed.
The token stays out of URLs and logs, just like the normal grant.

**Watch token grammar.** The primary token is unchanged from section 12.2.
The watch token is a separate grammar and MAC domain:

```
payload = 0x02 || leg || 0x02 || session[16] || station[8] || expires[4, big-endian]
mac = HMAC-SHA256(secret, "NereusSDR relay grant v2\n" || payload)
token = base64url(payload || mac), without padding
```

The third byte is purpose 2 (watch); no other purpose is valid in this grammar.
It is 63 raw bytes and 84 canonical base64url characters. Session, station and
expiry match the primary grant exactly, and the role byte still identifies the
Core or device. Primary verification accepts only v1; watch verification accepts
only this v2 grammar. MAC comparison is constant-time. New relay admission
explicitly selects the appropriate verifier rather than interpreting a watch
token as permission to open a normal session.

**Admission and retirement.** A watch JOIN requires an existing charged session
with both primary legs present and nonclosing, with identical station and expiry.
It never creates a session or consumes a second logical slot. Missing primary
returns `peerGone`; mismatched grant binding returns `badToken`. As with primary
rejoins, an already live session may accept its matching grant after the grant's
initial-open expiry; an absent or ended session is never revived. One watch leg
per side is stored separately from the primary legs. A same-side watch replacement
replaces only that watch. Either primary leaving, replacement, session end or
shutdown retires both watch legs and their queued data before callbacks can act.
Watch disconnect alone leaves primary admission, rejoin and idle timers unchanged.

**Frames and bounds.** JOIN/READY/PEER/END keep their existing framing. READY and
PEER on a watch describe its watch counterpart. Only tag `0x03` followed by
1 to 1500 bytes of opaque ICE/DTLS data is forwarded between watch legs. Tag 3
remains reserved on primary legs; control/media tags or other data tags on a watch
are protocol errors. The relay never reads an encrypted Core ticket or heartbeat.

Watch sockets count against the existing pending, address and stalled-writer
bounds. The physical address default is now 36: two primary and two watch ends
for each of nine possible transport sessions, even behind one NAT. Logical caps
remain nine per station and sixteen globally; the Core's four admitted device
places remain separate. Watch-specific defaults are 8000 bytes/second, a burst
of one second, and a receiving queue bounded by 16 frames or 8192 bytes with
oldest-first dropping. Those rate buckets survive watch reconnects. Watch bytes
also consume the existing sending-side control bucket, so they cannot bypass its
80000 bytes/second aggregate limit. They count toward traffic statistics but
never refresh primary idle time. Pending watch PEER notices are bounded at 16
on a blocked writer, apart from its fixed READY/END/close overhead.

Verification uses strict codec tests, real loopback service-issued grants and
relay forwarding, adversarial binding/lifecycle cases, and deterministic resource
limits. Existing v1 control and relay fixtures remain unchanged. No production
rollout, Core attachment, TURN-free watch transport, or restrictive-network
transmit-liveness result is implied by those tests.
