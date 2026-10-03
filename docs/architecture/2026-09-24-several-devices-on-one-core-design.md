# Several devices on one Core: design

**Status:** Final, 2026-09-24. The iPhone session reviewed the draft against its screens
(`multi-client-design-review.md`, findings 1 to 14, every ruling taken) and the operator
answered its eight questions the same day (the iPhone design's D58 to D64). Both are folded in
here, with the iPhone session's re-review of this revision and the operator's confirmation of
its last narrowing (an older window turned away from a full Core, section 15.2).
**Date:** 2026-09-24
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code
**Code cited at:** `codex/lane-b` `0c9e10ab`, except where a cite is marked `@aa6c5505`: this
revision re-checked every cite it added or changed against lane B's `aa6c5505` and stamps
those. Task 13 has since landed on lane B as `c35048dd`, which moves lines in
`StationServer.{h,cpp}`, the link and the plan; an unstamped cite stays at `0c9e10ab`. The
link cited with a line number is at `0c9e10ab` unless stamped. The gaps plan is cited at
`e14b42eb` on `codex/receiver-tx-gaps`. The iPhone design's decisions D44 to D64 and its
sections 3.9, 4.5 and 5.8 are cited at `94ec2f7e` on `claude/nereussdr-iphone-app-5fb988`,
where the iPhone session wrote the operator's answers as D58 to D64, amended D52 and bounded
D56 by D62. The iPhone plan on that branch is cited at `06209b82` or `7da93d19`, as each cite
says.

Documents are cited by short name, all in `docs/architecture/`:

| Short name | File |
| --- | --- |
| the link | `2026-09-23-station-link-v1.md` |
| the plan | `2026-09-23-iphone-app-plan.md` |
| the iPhone design | `2026-09-23-iphone-app-design.md` |
| the pairing design | `2026-08-02-remote-station-identity-and-pairing-design.md` |
| the remote design | `2026-07-28-remote-daemon-architecture-design.md` |
| the budget design | `2026-09-22-session-display-budget-design.md` |
| the gaps plan | `2026-09-24-receiver-and-transmit-gaps-plan.md` (`e14b42eb`) |

**How to read it.** "**Ruling n.m**" is a rule this design sets. "**Design ruling**" marks a
place where a review ruling or an answer could not be built exactly as worded and this design
writes the closest form that can be, with the evidence. The operator's decisions are cited by
their numbers in the iPhone design (D44 to D64, section 2.1). Section 15 lists every place the
design narrows or widens a decision, and section 16 records what the operator decided on
2026-09-24 and where it landed. Everything else is a statement about today's code with its
cite. Words in quotes that a user would read are provisional: the phone's screens set the
final wording.

**Attribution.** NereusSDR-original. FlexRadio's Multi-Flex and AetherSDR's handling of it
are design precedent only; nothing is ported, so no upstream header or provenance row
applies.

---

## 1. Goal

Up to four devices (desktop windows, iPhones, iPads) use one Core at the same time. Each
runs its own slices and pans on receivers the Core hands out from the radio's pool, sees
where the others are, and is asked before it disturbs them. One device at a time holds the
transmitter; every device shows which one, and its operator is the control operator.

This document covers the Core's side: sessions and admission, ownership, receivers, settings
every device shares, transmit, media and capacity, the link, and the desktop's own screens.
The phone's screens belong to the iPhone session and point here.

**Not in scope.** Guest devices with fewer permissions (every paired device stays the
owner's, as today), and more than four devices.

---

## 2. Decisions

### 2.1 The operator's decisions (binding)

The iPhone design's section 3.9, at `94ec2f7e`. The wording here is a summary; the iPhone
design's table is the text.

| # | Decision |
| --- | --- |
| D44 | Up to four devices on one Core at the same time, each with its own session. |
| D45 | Each device owns its slices and pans. The Core hands receivers out from the radio's pool; only the owning device tunes, changes or closes its slices. |
| D46 | Other devices' slices show on the band as labelled, read-only markers carrying that device's name. |
| D47 | Slice letters are shared across the Core, from one pool (a drawn detail). |
| D48 | Two devices may share one receiver when their slices fit its window. |
| D49 | When no receiver is free, a device takes one after confirming. It sees which device holds each receiver and what it is doing, and picks one. That device's slice on it closes, and it is told who took it and when, with Take it back, which asks the same question the other way. |
| D50 | Moving a pan whose receiver another device shares asks first, naming that device. Once confirmed, that device's slice moves to a free receiver, or closes if none is free, and that device is told. |
| D51 | One device holds transmit, and its operator is the control operator. Every device shows who has it: the PTT button names the device and TX marks its slice. Another device takes transmit after confirming; the holder is told, with Take it back. If the holder is on the air, the button reads "Unkey and take over" in red and the Core unkeys it first. |
| D52 | The radio's own PTT takes transmit: a mic or a footswitch (amended by D58: a program, even one on the Core's computer, transmits only while its own device holds transmit). The press is the confirmation; the holder is told, with Take it back, and a holder on the air is unkeyed first. |
| D53 | Any device may change a setting every device hears (sample rate, preamp and attenuator, antennas, PureSignal, the amp and the tuner), but a change that would disturb another device's slices asks first and names that device, which is told afterwards. |
| D54 | When the devices together ask for more display and audio than the Core can send, the transmit holder keeps its full band and sound; the others share what is left, their frame rates dropping first, with a "Sharing" chip on the band. |
| D55 | A fifth device takes one's place after confirming. It lists the four connected (name, how long, listening on what or transmitting, when last active), starting on the one idle longest. The device whose place is taken is told who took it and when, with Take it back; a device on the air is unkeyed first. |
| D56 | A device coming back to its own dropped session is not asked. It gets its slices back, and transmit only if nobody took it meanwhile, within the 3 minutes D62 gives it. |
| D57 | Setup, Devices lists the devices connected now apart from the devices only paired, each with its slice letters and bands, and TX on the one with transmit. |
| D58 | A program transmits only while its own device holds transmit. A program keying through a device's window (TCI, VAX or CAT) is refused otherwise, and the window says why; a person takes transmit first. The same holds on the computer whose window runs the Core. On a Core with no NereusSDR window, programs cannot transmit. |
| D59 | An older NereusSDR window is let in. It works with its own slices; anything that would affect another device is refused with a plain "update NereusSDR"; if a take closes its only slice, it is disconnected with that reason. |
| D60 | While the holder is on the air, changes to the transmit path wait: the amp, the tuner, an antenna, PureSignal, the interlock, the power limit, a change that would move or close the transmitting slice, and a Protocol 1 sample-rate change are refused with "<device> is on the air. Try again when they stop." Another device can still Unkey and take over, then make the change. |
| D61 | The receive antenna stays put on a band crossing while another device listens through that antenna input, and the person tuning is told. The new band's transmit antenna still applies at key-down. |
| D62 | A device that drops keeps its place, its slices and transmit for 3 minutes, unkeyed at once. After that its slices are saved for its return, and its place and transmit are freed; coming back to a full Core then brings the fifth-device question. |
| D63 | When nobody has transmit, a tap on PTT takes it and keys, and every device shows who has it. Programs still never take it (D58). |
| D64 | Five narrower rules, kept together: nobody can take the receiver of a slice someone is transmitting on; while the radio's own mic or footswitch is keyed, the slice it transmits on cannot be retuned until the press ends; a fifth device cannot take the place of the computer running the Core; sound is never cut when the Core runs short; the desktop running the Core asks before taking transmit, like any device, while a mic plugged into the radio still takes it at once. |

The iPhone design's D35 (a desktop that hosts the Core counts its own window as one of the
four, and that window takes transmit by the same rules as any device) is carried forward, as
D64 confirms. D21's one-device takeover is replaced by D44 to D57.

### 2.2 Consequences agreed with the iPhone session

- The active slice is per device. The TX slice is chosen by the transmit holder, among its
  own slices.
- Each device hears its own slices.
- The mirror gains owner marks and a view per device.
- Media and the display budget are per device, within one shared Core budget (D54).
- The desktop's own window counts as one of the four when the desktop hosts the Core (D35).
- Transmit follows its holder, not a session. R4's gate (Task 34) and keying (Task 35)
  become per holder: `keyedBy` names the holder, and "Unkey and take over" reuses the
  `UnkeyGate`. Task 41 splits in two: the transmit takeover and the fifth-device takeover.
- TCI and VAX apps act as the device whose window serves them (R-R3-42), and key only while
  that device holds transmit (D58). The Core's own TCI server stays receive-only.
- The "disturbs" test in D53 needs the Core to know which devices' slices sit on each
  ADC and receiver: on Protocol 1 the sample rate is radio-wide, so a change touches
  everyone; on 2-ADC boards the antenna and preamp touch that ADC's slices; PureSignal on
  1-ADC boards suspends every user receiver while the holder transmits.
- Settings split along the existing scopes: Station-scoped settings are the shared ones;
  window-scoped settings stay per device.
- Withdrawn by D48: "each receiver belongs to one device".

### 2.3 The controller's rulings (given to this draft)

- The pan of the device that claimed a receiver anchors that receiver's window.
- Taking a receiver closes every slice on it, each owning device told. D49's text and
  this ruling settle it; ruling 6.7 applies it.
- When the transmit holder disconnects, the Core unkeys at once. (It also ruled that transmit
  then stays unheld; D56 and D62 keep transmit held for a device through its 3 minutes
  instead, section 2.4.)
- A change that disturbs another device answers "needs confirmation", naming the devices,
  and a confirming command applies it.

### 2.4 Settled with the phone screens (2026-09-24)

The operator approved the multi-client phone screens and settled four details, which the
iPhone design now carries inside the decisions named. They are binding here.

| # | Settled detail |
| --- | --- |
| S1 (D47) | Slice letters are shared across the Core: a slice has one letter on every device, from one pool the Core hands out, as it hands out receivers (ruling 5.5). |
| S2 (D51) | Every device knows who has transmit: the Core tells every session the holder's device name and whether it is on the air. After the radio's own PTT, the holder shows as "Radio" (ruling 8.1, section 10.3). |
| S3 (D55, D57) | The fifth device's list starts on the device idle longest that can be replaced (ruling 4.6; D64 keeps the hosting desktop's place). For each connected device the Core reports how long since it was last active, how long it has been connected, and what it is doing: listening, and on what, or transmitting. The device list the Devices page reads carries the same (section 10.3). |
| S4 (D56, D62) | A device that comes back after a dropped link within 3 minutes gets its slices back, and gets transmit back only if nobody took it meanwhile. While its holder is away within those 3 minutes, the radio is unkeyed and transmit is held for that device. A take during that time is D51's confirmation without the red button, since nobody is on the air (rulings 8.7, 8.15). |

The screens also show who took what and when (a receiver, transmit, a place), name the device
whose slices a shared change would reach, and list each connected device's slices and bands on
the Devices page. Section 10.3 says where the Core sends each.

---

## 3. What the Core does today

The design changes these facts; each is cited once here and referred to later.

**One session.** `StationServer` keeps one `m_session` beside all its peers
(`src/core/session/StationServer.h:665-666`); `hasAuthenticatedSession()` is "At most one, by
construction" (`StationServer.h:429-430`). Its topology note chose one shared mirror "because
there is never more than one authenticated session" and says a second viewer "would then want
a mirror per viewer" (`StationServer.h:40-60`).

**Preemption.** A second authenticated connection ends the first with the `takenOver` code,
not retryable (`src/core/session/StationServer.cpp:1661-1685`), takes `m_session`, bumps the
media epoch and renames the dispatcher's owner to `station:<epoch>`
(`StationServer.cpp:1696-1701`). The link states the rule (the link, lines 1606 and
1630-1633); its `preempted` fixture pins it (the link, line 2061).

**Connections.** Every socket, connecting or not, counts against `kMaxConcurrentPeers` = 8
(`StationServer.h:254-260`); the ninth gets a retryable end before any hello
(`StationServer.cpp:1017-1032`; fixture `connection-limit`, the link, line 2054).

**Liveness.** The Core pings every peer every 20 s and, at a tick where two pings are still
unanswered, ends the session, `retryable` true (`StationServer.h:251-252` and
`StationServer.cpp:1490-1511` `@aa6c5505`; the link, section 12.1, lines 1800-1808
`@aa6c5505`). A link that falls silent just after a pong is ended at the third tick, 60 s
later; one that falls silent with a ping outstanding, at the second, 40 s later. The desktop
client runs the same heartbeat (`StationClient.h:304-305` `@aa6c5505`) and redials after 1,
2, 5, 10, 30 and 60 s, then every 60 s (`StationClient.cpp:1266` and `1280-1300`
`@aa6c5505`; the link, lines 1895-1898 `@aa6c5505`). The phone runs the same heartbeat and
the same schedule (the plan at `7da93d19`, lines 1001-1002 and 1017-1019).

**Discovery.** The LAN announcement's schema 2 extends only by appending a field after the
last, each new field stating what a reader that never sees it assumes (the link, lines
1988-1994 `@aa6c5505`); a schema-2 datagram is at most 479 bytes against a listener's 512
(lines 1960-1963 `@aa6c5505`). The Bonjour TXT record has five entries in a fixed order, and a
client ignores a key it does not know (lines 2031-2042 `@aa6c5505`).

**Devices are already known.** Since Task 12 each peer carries the paired device it signed in
as, empty for a token sign-in that enrolled nothing (`StationServer.h:564-569`; set at
`StationServer.cpp:1643-1652`).

**Everything goes to the one session.** Mirror messages, command results, settings values and
object creates all reach `sendToSession` (`StationServer.cpp:709-710`, `718-719`, `720-750`,
`783-791`), which already filters per peer by minor and capability
(`StationServer.cpp:2151-2218`). Media control is accepted from the current session only
(`StationServer.cpp:1409-1413`; the link, line 1506).

**Echo.** The mirror suppresses every notify raised while it applies a peer's write,
including side effects on other objects (`src/core/session/StateMirror.h:54-84`); the write's
readback returns side effects on the written object to the writer only
(`StationServer.cpp:2009-2032`).

**Receivers.** `SliceStreamAllocator` joins any active window that covers a frequency, else
claims a free receiver, else refuses (`src/core/SliceStreamAllocator.cpp:119-169`); it does
not know who asks. A caller may ask for its own window, which skips sharing
(`SliceStreamAllocator.cpp:119-124`). A slice that is not its receiver's only occupant moves
only its shift (`SliceStreamAllocator.cpp:185-214`). A receiver's slices share its antenna
(the first slice's, `src/models/RadioModel.cpp:19061-19077`) and its noise blanker, which a
joining slice adopts (`RadioModel.cpp:6737-6760`). A C-Tune pin holds the whole receiver's
window (`RadioModel.cpp:6139-6145`, read at `6604-6605`). A C-Tune centre change is refused
when any slice on the receiver would fall outside (`RadioModel.cpp:6183-6194`). A sample-rate
change is planned for every slice and refused whole if any would be refused
(`RadioModel.cpp:6264-6324`); on Protocol 1 the rate is radio-wide
(`RadioModel.cpp:6069-6077`, `6274-6287`, `6476-6483`), and a live change pauses audio input
and stops the radio's data flow (`RadioModel.cpp:17886-17898`). The Core knows each
receiver's ADC (`RadioModel::adcForStream`, `RadioModel.cpp:14666-14679`).

**Antennas follow one slice.** A band crossing by the active slice re-applies that band's
Alex antenna (`RadioModel.cpp:12817-12826`), and every MOX change re-routes the Alex antennas
(`RadioModel.cpp:16293-16298`).

**One active slice.** `setActiveSlice` clears the previous slice's flag across every slice
(`RadioModel.cpp:8510-8527`); `active` and `txSlice` are sent to clients only
(`src/core/session/MirrorPolicy.cpp:118-119`).

**The restart manifest.** The Core saves its live slices per radio and restores them at start
(`src/core/ReceiveLayoutStore.h:26-70`; `src/core/daemon/DaemonApp.cpp:266-273`); a manifest
holds one to five slices with unique ids (`src/core/ReceiveLayoutStore.cpp:101-119`).

**Transmit.** `TxSliceArbiter` binds one slice (`src/core/TxSliceArbiter.h:25-27`) and does
nothing in a remote window (`TxSliceArbiter.cpp:68`, `162`). `MoxController` is "last setter
wins" (`src/core/MoxController.h:567-570`) and gates only keying
(`MoxController.cpp:494-517`); unkeying is never gated (`MoxController.h:256-257`). The radio
reports its PTT on every status frame as a level (`RadioModel.cpp:1307-1311`, wired at
`11755-11758`). The mic handler sets `PttMode::Mic` before it keys
(`MoxController.cpp:1196-1197`) and releases whenever the mode reads Mic
(`MoxController.cpp:1207-1208`), and `setMox(false)` does not clear the mode
(`MoxController.h:562-565`). The gaps plan's Task 7 makes every keying source set, clear and
guard its PTT mode as Thetis does (the gaps plan, lines 233-264). `txPermitted` is false
(`StationServer.cpp:2511-2512`); remote transmit is the plan's Part F.

**TCI.** The Core's own TCI server is built receive-only
(`src/core/StationTciController.cpp:44-45`; `src/core/TciServer.cpp:2660-2675`). On a
desktop, the TCI audio lock picks whose audio feeds the transmitter, but a second app's `trx`
still keys MOX (`TciServer.cpp:2676-2705`, `2735`); the gaps plan's Task 4 makes that do what
Thetis does (the gaps plan, lines 172-176).

**Media.** `DaemonMediaController` "Owns one authenticated daemon media session"
(`src/core/session/media/DaemonMediaController.h:83-86`): one peer, at most 8 display
endpoints (`DaemonMediaController.cpp:35`), at most 4 receiver streams
(`src/core/session/media/IMediaTransport.h:91`). Its main audio is the master mix or the
speakers' mix of every slice (`DaemonMediaController.cpp:2971-2976`), from AudioEngine's one
master tap (`src/core/AudioEngine.cpp:2161-2191`).

**A remote window keeps its last slice.** When the Core destroys a slice, a remote window
removes it through `removeSliceImpl` (`RadioModel.cpp:4511-4520`), which never removes the
last slice (`RadioModel.cpp:7562-7567`).

---

## 4. Devices and sessions

### 4.1 What a device is

**Ruling 4.1.** A device is one of:

- **A paired device**, identified by its device key's id (the link, section 3.5;
  `Peer::deviceId`).
- **A window signed in with the older token without a key** (the token branch of
  `StationServer.cpp:1564-1641` leaves `deviceId` empty). It is a device for the life of its
  session, with an id the Core makes (`token:<n>`) and a name the Core gives it ("Computer at
  192.168.1.20", from the peer address). It cannot be recognised when it comes back, so it has
  no grace period (4.5).
- **The station device**: the operating position at the radio itself. On a desktop that
  hosts the Core (D35) it is that desktop's own window, which has no network session, counts
  as one of the four (section 2.2) and carries the hosting desktop's device name and id. On a
  Core with no desktop it has no window and does not count; it runs slices held for absent
  devices (5.2) and holds transmit when the radio's own PTT (a mic or a footswitch, D52)
  takes it (8.5). Its kind is `station`, the value Task 48 already reports for keying at the
  desktop (the plan, lines 4190-4192). As a transmit holder it shows as "Radio" after the
  radio's own PTT takes transmit, on any Core (settled detail S2, ruling 8.1).

**Ruling 4.2.** One session per device. A second connection of the same device replaces the
first (4.4).

**Ruling 4.3.** Every device has a name and a short name, and one device carries the same
of each on every page.

- **The name** is the one it paired with (`DeviceStore`, the link, section 3.5), or the one
  the Core gives a token window.
- **The short name** is one the device chooses, for the places a screen has room for one word
  ("PTT" over "MacBook", "B, MacBook" on a marker's label; the iPhone design, section 5.8
  items 1, 2 and 5). The app sends it every time it signs in, as a new optional string
  `shortName` in `auth.request`'s `device` block (`SessionDeviceBlock`,
  `SessionMessages.h:317-323` `@aa6c5505`; the link, line 193 `@aa6c5505`). Pairing is
  unchanged: `pair.start`'s `device` block and the code-mode confirmation box keep
  `{"publicKey","name","kind"}`, and the app signs in straight after pairing, so the Core has
  the short name from the first session. `shortName` is outside the device-auth transcript
  the block's `signature` covers: it is a label inside the session's TLS, and a Core reads the
  signed fields unchanged. Design ruling (2026-09-24, settled with the phone session so the
  phone builds its pairing and sign-in wire once): this key lands with the identity and
  pairing work (Part C) before it reaches integration, not with this design's tasks.
  `shortName` sits outside the signed transcript exactly as `name` does (the link, lines
  195-201 `@aa6c5505`: the transcript binds the challenge, the certificate and the two keys,
  no name).
- **Validation** (re-review finding 3). A name and a short name are the operator's own words,
  not the Core's sentences, so they are validated the way `DeviceStore::isValidName` validates
  a device name (`src/core/security/DeviceStore.cpp:210-224` `@aa6c5505`): not empty after
  trimming, no control, format, line-separator or paragraph-separator characters, and valid
  UTF-8. The name keeps `DeviceStore`'s cap of 64 bytes of UTF-8 (`kMaxNameBytes`,
  `DeviceStore.h:74` `@aa6c5505`); the short name's cap is 32 bytes of UTF-8
  (`kMaxShortNameBytes`), counted the same way, room for 16 characters of any script. Neither
  is held to the plain-wording test (`OperatorWording::isPlain`), whose term list would
  refuse ordinary names ("Grant's iPhone"); only the Core's own sentences are, and the tests
  hold them to it with names such as "Grant's iPhone" embedded.
  The Core stores it with the device and replaces it at each sign-in that carries one, so a
  renamed phone's next connection updates it. Missing, empty or unusable, it is the device's
  kind in plain words ("Phone", "Tablet", "Computer"); a token window's is "Computer"; the
  station device's is the hosting desktop's, and "Radio" after the radio's own PTT.
- **Numbering.** When two devices carry the same name, the Core adds a number to the one
  paired later ("iPhone 2"). Short names are numbered on their own collisions by the same
  rule, whether or not the full names collide, including two devices that both fall back to
  "Phone" ("Phone", "Phone 2") (re-review finding 8). Both apply in everything the Core
  sends: `devices` (Task 13's paired list, `StationDevicesFacade.cpp:83-97` `@aa6c5505`, which
  today sends the stored name as it is), `connectedDevices`, `session.held`, markers,
  `txState`, `confirm.request` and `notice`. Token windows are numbered after the paired
  devices, in the order they connected. One device therefore has one name, and one short
  name, on the Devices page's two lists and on every screen (review finding 13).

### 4.2 Admission and the limit of four

**Ruling 4.4.** `kMaxDeviceSessions` = 4 (D44) counts admitted sessions, devices in
their grace period (4.5), and a hosting desktop's own window. It does not count connections
still connecting, a fifth device while it is being asked (4.3), pairing connections (Task 14,
which end after pairing, the plan, lines 1635 and 1676), or the station device of a Core with
no desktop.

**Ruling 4.5.** `kMaxConcurrentPeers` stops meaning "one session and a few stale sockets"
(`StationServer.h:254-260`) and becomes a pure socket cap of 24. While a device reconnects it
can hold its old socket (not yet noticed dead) and four racing attempts, since Task 29 races
direct connections over IPv6 and IPv4, the rendezvous path and the relay floor (the plan,
lines 2914-2918): five sockets for each of four devices is 20, and a fifth device's four
attempts make 24. At the 1 MiB per-socket message cap (`StationServer.h:262-298`) that is 24
MiB of exposure before sign-in, still small on a Pi 4 by the header's own argument. The next
socket gets today's retryable end, unchanged.

After `auth.result` accepted, the Core decides:

1. The device already has a session (live or away): section 4.4.
2. A place is free: today's connect sequence (capabilities, settings, schemas, objects,
   `snapshot.complete`), in the device's own view (5.5).
3. The Core is full: section 4.3 for a device that declared the new feature (10.1); section
   10.7 for an older window.

### 4.3 The fifth device (D55)

D55 is built on the messages Task 41 planned (the plan, lines 3742-3761), which were never
built and change shape here.

- Instead of `capabilities`, the Core sends `session.held` listing the four (settled detail
  S3): `{devices: [{deviceId, name, shortName, kind, state, holdsTransmit,
  lastActivitySeconds, connectedForSeconds, awayForSeconds, transmittingForSeconds,
  listeningOn, transmittingOn, from, replaceable}], revision}`, plus `placeTaken` when this
  device's own place was taken while it was away, or `placeFreed` when its own 3 minutes ran
  out (below).
  - The entry has the same keys, with the same meanings, as an entry of
    `connectedDevices.listJson` (10.3), plus `from` and `replaceable`, so a device is named
    and described the same way on both screens (ruling 4.3).
  - `state` is `transmitting` while the device is on the air, `away` in its grace period, and
    `listening` otherwise; `holdsTransmit` is true for the holder, keyed or not.
  - `listeningOn` lists the device's slices, each `{letter, frequencyHz, mode, band}`;
    `transmittingOn` is its transmit slice, in the same shape, while it transmits.
  - `lastActivitySeconds` keeps Task 41's meaning (the plan, lines 3745-3747): it counts from
    the device's last command, property write or settings write, and heartbeats do not count
    (the link has no other keepalive; its heartbeat is WebSocket ping and pong, line 81). A
    device counts as active for as long as it is on the air. `from` keeps Task 41's meaning.
    The four durations follow the one clock convention (ruling 10.3).
  - `replaceable` is false only for a hosting desktop's own window, which runs the Core and
    cannot be sent away (D64).
- **Ruling 4.6.** The list is ordered by idle time, longest first, and the app's choice
  starts on the first entry whose `replaceable` is true (settled detail S3; review finding 4).
  An away device counts as idle longer than any device that is present, so away devices come
  first, the one away longest first, as the approved in-between state draws it (board v50,
  10.9); present devices follow by `lastActivitySeconds`, and a device on the air is never
  idle, so it comes last. A hosting desktop keeps its
  place in the order, shown but not selectable, so the list still reads idle longest first.
  With four places taken there are always at least three replaceable devices, since only one
  window can host the Core.
- The client answers `session.takeover {deviceId, revision}`; an empty `deviceId` cancels. A
  `deviceId` whose entry is not `replaceable` is refused like an old `revision`: the Core
  sends the current list again.
- Task 41's rules stay: the 30 s connect deadline pauses while the question is open and the
  answer deadline is 60 s (the plan, lines 3748-3749); the waiting device learns nothing but
  the list, no snapshot, settings or media (lines 3768-3769); only a paired, authenticated
  device receives `session.held` (line 3770).
- **Ruling 4.7.** An answer carrying an old `revision` is not acted on; the Core sends the
  current list again. When a place frees while questions are open, the device asked first is
  admitted at once (the arrival of `capabilities` ends its question), and every other waiting
  device gets the new list.
- On a choice: if the chosen device holds transmit, the Core first releases it through a
  transfer to nobody (8.2): every key refused, the unkey confirmed if it was keyed, and MOX
  read off. Only then does it end that device's session with `session.end`, reason "iPad took
  this device's place on the Core.", `retryable` false, code `takenOver`, with `takenOverBy`
  (the taker's name, the key Task 41 planned, line 3752), `takenOverById` (the taker's device
  id, so the replaced device's Take it back can preselect it in the list it meets; review
  finding 7) and `secondsAgo` (how long ago, ruling 10.3; it replaces Task 41's planned
  `at`). The reason carries no time: the app shows the time from `secondsAgo`, by its own
  clock and in its own time zone. The replaced device gets no grace period: its slices close
  at once and are saved for its return (5.3).
- Cancel, or no answer in 60 s: `session.end` "The Core already has four devices
  connected.", `retryable` false, a new code `coreFull`.
- An away device chosen this way has no connection to end: its place, its slices (closed and
  saved, 5.3) and any hold on transmit go at once, and the Core keeps who took its place and
  when until that device next signs in. It then learns it from `session.held`'s `placeTaken
  {byName, byId, secondsAgo}` when the Core is full again, or from a `notice` of kind
  `placeTaken` when it is admitted (7.4).
- A device whose 3 minutes ran out (ruling 4.11) and which comes back to a full Core finds
  `placeFreed {secondsAgo}` in its `session.held`: its place was not taken by anyone, it was
  freed when its time ran out, so its app says so above the list rather than naming a taker.
- Take it back: the replaced device connects again, meets the full Core, and finds the taker
  in the list; its app preselects the entry whose `deviceId` is the `takenOverById` it was
  given (or `placeTaken.byId`), when that entry is still there and replaceable.

### 4.4 The same device connecting again

**Ruling 4.8.** When a device signs in while it already has a session, live or away, the Core
admits the new connection as that device's session at once, with no question, and ends the
older connection with `session.end` "This device connected again.", `retryable` false, a new
code `sameDevice`. The device keeps its slices, pans and active slice. Its media starts over
on the new connection. If it held transmit, it keeps holding it, unkeyed: a key on the
older connection is stopped through the transfer's fence (ruling 8.2), and the hold carries
over to the new connection (ruling 8.15). This keeps the reclaim rule, the iPhone design's
D56 ("A device coming back to its own dropped session is not asked", bounded by D62's 3
minutes, at `94ec2f7e`), without preemption. The same rule covers a device whose old
connection the Core has not yet noticed is dead: its new sign-in ends the old one at once.

**Ruling 4.9.** A device authenticates on one connection at a time. Task 29's path race lets
"the first session to reach `snapshot.complete`" win (the plan, lines 2916-2918); two racing
connections that both sign in would replace each other under ruling 4.8 (and under today's
preemption too). The client therefore sends `auth.request` only on the first path whose
`hello` arrives and closes the others before signing in on any of them.

### 4.5 The grace period (D62)

**Ruling 4.10.** A device whose session ends without leaving on purpose (a lost link, the
heartbeat, the app stopped by its system) is **away** for 180 s (D62: "3min"). It keeps its
place among the four; its slices keep running; its pans stay; markers show it as away;
notices for it wait (7.4). If it held transmit, the radio is unkeyed at once and transmit
stays held for it, unkeyed, until it comes back, its 180 s end, or another device takes it
(ruling 8.15).

**Why 180 s holds a device's redials.** The Core's 180 s start when it ends the session (3,
"Liveness"): at once when the socket closes, or 40 to 60 s after a silent link falls quiet,
at the heartbeat's second or third tick. The device notices the loss by a socket error, by its
own heartbeat after the same 40 to 60 s, or by being told by its system; it then redials after
1, 2, 5, 10, 30 and 60 s, then every 60 s (the desktop and the phone alike), so its attempts
come 1, 3, 8, 18, 48, 108, 168 and 228 s after it noticed.

- When both ends notice at the same heartbeat tick, the first seven attempts, up to 168 s,
  fall inside the 180 s, the last with 12 s to spare.
- The worst alignment is the device noticing 20 s after the Core (the Core's last ping was
  lost with the link, so it ends the session at 40 s; the device's was answered just before
  the loss, so it notices at 60 s): its attempts come 21, 23, 28, 38, 68, 128 and 188 s
  into the Core's 180 s, so six of them fall inside, the last 52 s before the end.
- A device that notices before the Core (its heartbeat ticks first, or its system tells it)
  may reach the Core while the old session still looks alive; ruling 4.8 admits it at once
  and ends the old connection, so it never waits on the Core's heartbeat.
- From the real loss of a silent link a device therefore has 220 to 240 s in all; from a clean
  close, 180 s.

The times are the earliest each attempt can come: an attempt that hangs before it fails (a
dial the network neither answers nor refuses) pushes the later ones back. An address with no
route fails at once, and the link's 30 s connect deadline bounds an attempt whose WebSocket
opened (the link, section 12.2). And an absent device's receivers return to the others within
3 minutes.

**What each side shows.**

- **Within the 180 s, the device** comes back without asking (the iPhone design, section 5.8
  item 15): admitted at once (ruling 4.8), its slices arrive as its own `slice:` objects,
  `txState` names it as the holder, unkeyed, when nobody took transmit, and any waiting
  notices (a `transmitTaken` among them) arrive after `snapshot.complete`.
- **Within the 180 s, every other device** sees it `away` in `connectedDevices` with its
  `awayForSeconds`, its slices' markers with `ownerAway` true, and, if it held transmit,
  `txState.holderAway` true with the holder still named.
- **After the 180 s, every other device** sees it leave `connectedDevices`; its slices'
  markers go (closed and saved) or stay with `ownerAway` true (held for it when no other
  device was connected, ruling 4.11); `txState` shows transmit unheld, or its new holder.
- **After the 180 s, the device**, coming back to a Core with a place free, is admitted and
  receives a `notice` of kind `graceEnded` after `snapshot.complete`: "You were away for more
  than 3 minutes. Your slices are back, and transmit was freed." (the last clause only when it
  held transmit), with `slices` listing any saved slice that could not be restored (5.2). When
  one did not fit, the notice does not say the slices are back; it names what was not restored
  in the words of the `slicesNotRestored` notice: "You were away for more than 3 minutes. 2 of
  your slices could not be restored: all the radio's receivers are in use." (fix wave,
  2026-09-25). So
  the iPhone design's screen 15, "transmit was still yours", is shown only within the 180 s;
  after them the phone shows this notice and `txState` as it stands (review finding 6).
  Coming back to a full Core, it meets the fifth-device question with `placeFreed` (4.3).

**Ruling 4.11.** When the 180 s end, the device's place is freed, and its slices close and are
saved for its return (5.3), unless no other device is connected. Then they pass to the station
device, still held for their owner (5.2), and keep running, as a Core with one client keeps
its slices today. If it still held transmit, transmit is released through a transfer to
nobody and becomes unheld (ruling 8.15). The Core keeps that the device's time ran out, for
`graceEnded` or `placeFreed`, until the device next signs in, is revoked, or the Core
restarts.

Token windows and replaced devices get no grace period. Revoking a device (Task 13) ends its
grace at once, closes its slices (held ones included) and forgets its saved layout.

### 4.6 Leaving on purpose

**Ruling 4.12.** A new verb `session.leave {}` ends a device's session without a grace
period: after the accepted result the client closes the connection, and its slices close
(saved) or pass to the station device, held for it, if it was the last. A deliberate
Disconnect then frees receivers for the others at once.

### 4.7 Older windows (D59)

A window without the new feature is let in (D59). Section 10.7 says what it sees.

---

## 5. Ownership

### 5.1 Owner marks

**Ruling 5.1.** A new Core class, `SliceOwnership` (`src/core/`), holds every slice's owner (a
device id, `token:<n>`, the station device, or none), each owner's active slice (5.7) and each
receiver's anchor (6.2). A slice the station device runs for an absent device carries a
second mark, **held for** that device. Owner marks live at the Core. On the wire they show as
which `slice:` objects a device receives (its own) and as the owner fields of markers (5.4);
no `slice:` property changes.

### 5.2 Where a device's slices come from

**Ruling 5.2.** At admission, in this order:

1. **Slices held for it.** Slices the station device has been running for this device (it
   left last, or the Core restarted with them in the manifest, 5.3) become its own again.
2. **Its saved slices.** Slices saved when they closed (5.3) are restored where they fit: each
   joins a window that covers it or claims a free receiver, and takes its old slice id when
   that id is free. Its per-slice settings (the `Slice` keys, the link, section 8) come back
   with it either way, from the copy its store kept (5.3). What does not fit is reported to the
   device after its `snapshot.complete`, in a `notice` of kind `slicesNotRestored` whose
   `slices` lists them ("2 of your slices could not be restored: all the radio's receivers
   are in use."; review finding 14), or inside the `graceEnded` notice when its 3 minutes had
   run out (4.5).
3. **Slices with no owner.** The first device admitted while no other device is connected
   adopts every slice that has no owner: on a Core with no desktop, the ones it created at
   first start (`DaemonApp.cpp:967-1013`) or restored from a manifest written before owners
   existed. Slices held for another device are never adopted, so a returning device finds no
   duplicate and no stranger in its place. On a hosting desktop the window is the first
   device, at start, so it adopts them; it never gives its slices away, since it stays
   connected.
4. **A first slice.** A device that still owns no slice gets one, placed at the
   station-level active slice's frequency (5.7), which joins that slice's receiver (D48)
   and costs no receiver. When the slice cap is full it starts with none, and its app
   offers to take one (6.4).

**Ruling 5.2a** (settled with iPhone app Task 73). A slice the Core makes itself while one
device alone holds a place on it (its radio arriving after the device, the Core's own slice
top-up, a layout restored late) is that device's, as the slices it found at admission were
(step 3); with several devices on the Core such a slice has no owner and shows to each as a
marker. After a restart a manifest entry naming a device comes back held for it (step 1).

Slices held for an absent device keep running and keep their receivers. Another device may
take such a receiver (D49); nobody is there to ask or tell, so the closed slice is saved
in its owner's layout store (5.3), and the owner learns at its next admission, in the restore
report of step 2.

### 5.3 Two stores

**Ruling 5.3.**

- **The restart manifest** (`ReceiveLayoutStore`, per radio) keeps holding only the live
  slices, so its limit of five slices with unique ids (`ReceiveLayoutStore.cpp:101-119`) still
  fits. Each entry gains its owner, or the device it is held for, so a Core restart restores
  every live slice with its mark (`DaemonApp.cpp:266-273`). An entry without an owner (a
  manifest from before this design) restores as a slice with no owner.
- **A new `DeviceLayoutStore`**, per device and per radio, keeps each device's closed slices
  for its return: id, pan, frequency, mode, and a copy of the slice's own settings (its
  `Slice` keys, every band), taken when the slice closes. A slice restored under another
  letter gets its copy written to that letter's keys, so a slice a take closed comes back
  with its settings (the iPhone design, section 5.8 item 9), and a taker's new slice never
  keeps them. At most the board's `maxSlices` entries per paired device, removed when the
  device is revoked. It is separate from the manifest, so absent devices never use up the
  manifest's five places.

### 5.4 What another device sees: markers (D46)

**Ruling 5.4.** A new mirrored class, `SliceMarker`, key `marker:<sliceId>`: one object per
slice, sent to every view except its owner's. All properties go from the Core to the client:

| Property | Wire kind | Meaning |
| --- | --- | --- |
| `sliceId` | `i64` (sent in `object.create` only) | the slice's id; its letter is 'A' + id |
| `ownerDeviceId` | `utf8` | the owner's id, or the id of the device it is held for |
| `ownerName` | `utf8` | the owner's name, as the Core sends it (ruling 4.3) |
| `ownerShortName` | `utf8` | the owner's short name, for the label (ruling 4.3) |
| `ownerKind` | `utf8` | `phone`, `tablet`, `computer` or `station` |
| `ownerAway` | `bool` | the owner is in its grace period, or the slice is held for it |
| `frequency` | `f64` | as the slice's |
| `dspMode` | `enum` | the slice's domain |
| `filterLow`, `filterHigh` | `i64` | the passband edges, as the slice's |
| `txSlice` | `bool` | TX on the label: this is the transmit slice **and** its owner holds transmit (ruling 5.4a) |
| `band` | `enum` | as the slice's `band` (the link, line 891) |
| `streamIndex` | `i64` | the receiver it sits on, as the slice's own `streamIndex` (`MirrorPolicy.cpp:139`), so a device sees which other slices share its own receivers; shown as described below |
| `psPaused` | `bool` | its receiver is paused by PureSignal, as the slice's own `psPaused` (`MirrorPolicy.cpp:150`) |

A marker's colour is its letter's: the palette the desktop draws slices with is indexed by
the slice id (`VfoWidget::sliceColor`, `src/gui/widgets/VfoWidget.cpp:3400-3409`), and letters
are the Core's (ruling 5.5), so every device draws a slice in the same colour and no colour
goes on the wire. Older windows never receive a marker (10.7); if one did, the link's
unknown-class rule drops it (the link, lines 1145-1147).

**Receiver numbers.** A screen that names a receiver ("Receiver 2") shows `streamIndex` + 1.
`streamIndex` is the Core's logical receiver index, from 0, and -1 while a slice is bound to
none (`SliceModel.h:278-287`, `1435` `@aa6c5505`), so a slice on `streamIndex` 1 is on
"Receiver 2", and a slice on -1 names no receiver. The same holds for `streamIndex` in a
`takeReceiver` choice (6.4) and in `affected` (7.3) (review finding 7).

**Ruling 5.4a. TX marks only the holder's slice** (review finding 3). The TX mark on a slice
means that slice's owner holds transmit and transmits on it (D51; the iPhone design, section
5.8 items 1, 2 and 6). The Core therefore sends `txSlice` true, on a `slice:` object and on a
`marker:`, only while the slice is the transmit slice and its owner is
`txState.holderDeviceId`; otherwise it sends false, whatever the arbiter binds. The Core still
binds the transmit slice internally as ruling 8.12 and ruling 8.11 say; only what goes on the
wire follows this rule, and a change of holder sends the changed `txSlice` values to every
view. A `slice:` object goes only to its owner's view and a `marker:` is the Core's own object
(5.5), so each value has one reader and nothing about the rule differs between views. A
hosting desktop's own window draws TX on its slices by the same rule (section 12).

What the owner of a slice frozen by the radio's PTT sees (ruling 8.11): no TX on its slice,
since "Radio" holds transmit, not the owner. It reads the freeze from `txState`: `keyed` true,
`holderSource` `radioPtt` (8.1) and `txSliceId` its own slice's id. While those hold, its app
shows the slice as in use by the radio ("The radio is transmitting on this frequency.") and a
retune, mode or filter change, or closing it, is refused with the on-air reason (ruling 7.4).
Every other device sees the same slice's marker without TX, and "Radio" on its PTT. `txSliceId`
is Task 39's own `txState` property (the plan, line 3648), not a new one.

**Ruling 5.5.** Slice letters come from one pool the Core hands out, as it hands out receivers
(settled detail S1): the slice id, lowest free first (`RadioModel.cpp:7061-7091`), shown as 'A'
+ id on every device (`RadioModel.cpp:7073-7078`). A slice keeps its letter on every device for
its whole life, a restored slice takes its old letter when it is free (ruling 5.2), and "B,
iPhone" on a marker never collides with a device's own "A".

### 5.5 A view per device

**Ruling 5.6.** `StateMirror` keeps its one set of watches. Each admitted session gets a
`MirrorView`: its own outbound coalescer (a shared one would lose another session's pending
deltas on attach, `StationServer.h:44-47`), its own attach burst, and a filter: its own
`slice:` objects, `marker:` objects for every other slice, and the Core's objects its minor
and capabilities allow (today's `sendToSession` filter, `StationServer.cpp:2151-2218`).

**Ruling 5.7.** Echo suppression becomes per writer. A change made while applying device A's
write is withheld from A's view only, as today, and reaches every other view: a shared
receiver's blanker, a shared setting. It replaces the single `m_applying` guard
(`StateMirror.h:54-84`). A change applied on `confirm.proceed` is the requester's write too,
so its readback reaches the requester in the proceed's own answer instead (ruling 7.4a).

**Ruling 5.8.** Routing:

- `command.result`, `property.result`, `settings.reject`, `confirm.request` and `notice` go
  to one session: the requester, or the device a notice is for.
- `delta`, `object.create`, `object.destroy` and `settings.value` go to every view that holds
  the object or key. `settings.value` already carries the writer's origin (the link, lines
  1292-1297), so the writer still recognises its own echo.
- A change of owner (adoption, a slice closed by a take) goes to each view as
  `object.destroy` of the old form and `object.create` of the new one (`slice:<id>` to
  `marker:<id>`, or back).
- Capabilities are already per peer; `txPermitted` (Task 34) and the display allowance (9.3)
  are per device.
- The dispatcher's owner string (`SessionCommandDispatcher.cpp:480-491`) becomes
  `station:<sessionId>`, so one device leaving cancels only its own DSP-asset jobs.

### 5.6 Writes to another device's objects

**Ruling 5.9.** A device can address only its own slices. Refused, with "That slice belongs to
iPhone. It can be changed only there.":

- a `property.write` to a `slice:` key outside the writer's view, or to any `marker:` key;
- `removeSlice`, `setActiveSliceById`, `nnr.*` and `notch.add` naming another device's slice
  (the link, lines 1370, 1373, 1402-1404 and 1422);
- `requestSliceSampleRate`, `requestStreamCentre` and `requestStreamCtunPinned` naming a
  slice that is not the requester's (fix wave C1, 2026-09-25): another device's, one nobody
  owns, or one held for a device, whatever receivers are in use;
- a `settings.write` or `settings.remove` of a slice's own keys, `Slice<N>/...` (the `Slice`
  keys, Core scope; the link, section 8), from a device that does not own slice N, or for an
  id no live slice holds, whose keys would seed the next slice under it (fix wave I3,
  2026-09-25; the settings reject carries the Core's value);
- `tx.setTxSlice` (Task 34) naming another device's slice.

Four requests on a shared receiver are routes, not refusals, when they name the requester's
own slice: a sample-rate change is D53 (section 7); a C-Tune centre change or pin follows the
anchor rules (6.2, 6.3); and the anchor's own band change is a pan move (ruling 6.5). Naming
any other slice, they are refused as above: a device reaches a shared receiver only through
its own slice on it.

### 5.7 The active slice

**Ruling 5.10.** `setActiveSliceById` sets the requester's active slice among its own. A
slice's `active` property now means "its owner's active slice", so each device sees exactly
one active slice among its own objects, with no change on the wire.

**Ruling 5.11.** Some duties exist once per radio and follow `RadioModel::activeSlice()`
today. They follow a **station-level active slice**: the transmit holder's active slice while
transmit is held, otherwise the most recent active-slice change by any device. That keeps
today's behaviour for one device and gives the station's frequency to its control operator.
Corrected against the code by iPhone app Task 73 (`@46b40373`); the first version of this ruling
also named the simplex push, band tracking and the settings save, which do not follow the
active slice:

- **Follow the station-level active slice:** the FreeDV Reporter frequency (moved by Task 73
  from the per-slice handler in `RadioModel::wireSliceSignals` to one wired in
  `RadioModel::addSliceImpl`, gated on the station-level slice and re-published when it
  moves), and TCI's two broadcasts that exist once per radio, `digl_offset` and
  `digu_offset` (`TciServer.cpp:1030-1043` `@46b40373`, which read `RadioModel::activeSlice()`, now the
  station-level slice). `RadioModel::activeSlice()` itself is the station-level slice on the
  Core. The FreeDV Reporter narrows this (settled by the fix wave after the group review of
  Tasks 71 to 76): it lists the frequency of a slice in RADE mode when one exists (the first in
  creation order, whoever owns it), and the station-level slice's otherwise, because the
  reporter lists FreeDV activity. A slice entering or leaving RADE mode, or closing, re-checks
  which slice is listed (`RadioModel::freedvReportedSlice`,
  `RadioModel::refreshFreedvReportedFrequency`).
- **Per slice, and they stay so:** the simplex transmit-follows-receive push follows the
  transmit-bound slice, not the active one (`RadioModel.cpp:7787` `@46b40373` in `addSliceImpl`, gated on
  `txBoundSlice()`, `RadioModel.cpp:12636-12642` `@46b40373`, the arbiter's binding with no active-slice
  fallback); band tracking and the settings save run for every slice that changes band or
  value (`RadioModel::wireSliceSignals`, the per-slice `frequencyChanged` handler). The
  radio-wide antenna switch that band tracking makes is ruling 5.11a's (Task 75).

A hosting desktop's window reads its own active slice, not this one.

**Ruling 5.11a. The receive antenna stays put (D61).** The per-band antenna switch that band
tracking makes (`RadioModel.cpp:12817-12826`) re-applies the new band's receive antenna only
when no other device listens through that antenna input: no other device has a slice on a
receiver fed by the ADC that relay feeds (every receiver on a 1-ADC board; on a 2-ADC board,
ADC0's receivers for ANT1 to ANT3, 6.5). Otherwise the receive antenna stays where it is,
tuning goes ahead at once with no question, and the person tuning is told in a `notice` of
kind `antennaKept`: "The antenna stays on ANT1 while the iPad listens on it." (D61's words,
the other device's short name in place of "the iPad"). It stays, too, when a transmission
ends. The new band's transmit antenna still applies at key-down, since every MOX change
re-routes the Alex antennas (`RadioModel.cpp:16293-16298`). Once the other device's slices
leave that ADC, the next band crossing switches as today; the Core does not switch on its
own when they leave.

### 5.8 Pans on the Core

**Ruling 5.12.** A pan is a pair (device, pan key) at the Core; a window's pan keys are its
own strings today (`RadioModel.cpp:7120-7132`). The Core keeps, for each receiver, the pan
that anchors it (6.2). It does not mirror pans: markers do not need them, and the `pan:<i>`
key stays unused (the link, lines 1130-1134).

Built by fix wave I4 (2026-09-25), with no wire change: whether a new slice opens a new pan
(`addSliceImpl`'s `openingANewPan`, and the take chooser's `AddPan` need) counts only the
requesting device's own slices on that pan key (`RadioModel::panHasSlicesFor`), so a second
device's first slice on "pan-0" claims a receiver of its own even though the first device's
"pan-0" holds slices. `pan:<i>` is unused in fact: only `RadioModel::addPanadapter` makes a
`PanadapterModel`, and neither the Core nor a window calls it (a window's panadapters are its
`PanadapterStack` applets, whose centre, span and dBm range are the window's own), so no Core
sends `pan:<i>` and a write to it changes nothing. A window moves its receiver's centre with
`requestStreamCentre` (6.3), never through a pan object.

### 5.9 TCI and VAX

**Ruling 5.13.** A TCI server acts as the device whose window serves it (section 2.2). A
hosting desktop's TCI server maps its receivers to the station device's own slices, in id
order. A remote window's does already, because its model holds only its own slices. The
Core's own TCI server reads every slice as today (`trx:N` is slice N,
`src/core/TciProtocol.h:191-195`) and changes only the station device's own; it stays
receive-only (`StationTciController.cpp:44-45`), so its programs never key (D58: "On a Core
with no NereusSDR window, programs can't transmit"). Its transmit broadcasts (`tx_frequency`,
`TciServer.cpp:591-613`) describe the transmitter, whoever holds it.

A program keys only while the device whose window serves it holds transmit (D58; ruling
8.14), on a remote window and on a hosting desktop alike; it never takes transmit, held or
unheld (D63).

**Ruling 5.14.** VAX on the Core's computer carries only the station device's slices; a remote
window's VAX carries its own (R-R3-44).

---

## 6. Receivers

### 6.1 Sharing (D48)

The allocator needs no change: `placeSlice` already joins any window that covers a frequency,
whoever claimed it (`SliceStreamAllocator.cpp:119-133`).

Sharing brings two effects from today's code:

- A receiver's noise blanker and antenna belong to the receiver, not the slice
  (`RadioModel.cpp:6737-6760`, `19061-19077`). **Ruling 6.1.** Once two devices share a
  receiver, a blanker or antenna change on one's slice is a D53 change for the other
  (section 7).
- A shared window stops following its anchor's tuning: a slice that is not its receiver's
  only occupant moves only its shift (`SliceStreamAllocator.cpp:185-214`), as multi-slice does
  today.

### 6.2 The anchor

The controller's ruling: the pan of the device whose slice claimed the receiver (the
allocator's `NewStream` outcome, `SliceStreamAllocator.cpp:135-143`) anchors its window.

**Ruling 6.2.** When the anchor's last slice leaves the receiver, the anchor passes to the
device whose slice has been on it longest. Nobody is asked or told: nothing on anyone's band
moves. When the last slice leaves, the receiver is free.

**Ruling 6.3.** The C-Tune pin of a shared receiver is its anchor's. A pin holds the whole
receiver's window (`RadioModel.cpp:6139-6145`, read by every placement on it at `6604-6605`),
so `requestStreamCtunPinned` from a device that does not anchor the receiver is refused ("This
panadapter shows iPhone's receiver. Its C-Tune setting is iPhone's.").
A pin lasts as its anchor's pans do (ruling 4.8): through the anchor's link dropping, its
media ending and its coming back as the same device. It ends when the anchor leaves for good
(`session.leave`, a token window's end, the end of its 180 s, revocation), and the receiver
keeps its window, unpinned (fix wave after the group review of Tasks 71 to 76,
`RadioModel::clearStreamCtunPinsAnchoredBy`; before it, every pin ended with the Core's last
media session).

### 6.3 Moving a pan (D50)

**Ruling 6.4.** A C-Tune centre change from the anchor (`requestStreamCentre`,
`RadioModel.cpp:6148-6206`) that would leave only other devices' slices outside the new window
goes through the confirm step (7.3), naming those devices. If the operator goes ahead, the
Core moves the window; each other device's slice outside it goes to a free receiver
(`placeSlice`) or, with none free, closes; each device is told (`notice` `sliceMoved` or
`sliceClosed`, no Take it back). A change that would leave the anchor's own slice outside
stays refused, as today. A move that would move or close a keyed holder's transmit slice is
refused (ruling 7.4, D60).

**Ruling 6.5. The anchor changes band** (review finding 1). A write to one of the anchor's
slices that leaves its receiver's window (a band change, or any retune outside the window)
depends on who else is on the receiver:

- **Another device's slice shares the receiver:** it is a pan move, D50, and goes through the
  confirm step as `panMove` (7.3). The write is not applied; its answer is "Waiting for you to
  confirm.", and the `confirm.request` carries `change` `{label, from, to}` in bands ("40 m",
  "20 m"), so the app words "Go to 20 m" and "Stay on 40 m" (the iPhone design, section 5.8
  item 10), and `affected` names each other device with each of its slices' `effect`: `moves`
  when a free receiver would take it, `closes` otherwise. On proceed the receiver follows the
  anchor's slice, re-centred on its new frequency as a lone slice's receiver is today
  (`retuneSlice`'s `RetunedStream` outcome, `SliceStreamAllocator.cpp:200-206` `@aa6c5505`);
  each other device's slice outside the new window goes to a free receiver (`placeSlice`) or,
  with none free, closes; each is told (`sliceMoved` or `sliceClosed`, no Take it back). A
  slice of another device that the new window still covers stays. Cancel changes nothing. The
  on-air refusal applies (ruling 7.4, D60), and the receive antenna follows ruling 5.11a (D61).
- **Nobody else shares it:** as today. A lone slice takes its receiver with it
  (`SliceStreamAllocator.cpp:200-206` `@aa6c5505`); a slice that shares the receiver only with
  the anchor's own other slices leaves for a free receiver (`SliceStreamAllocator.cpp:218`
  `@aa6c5505`), disturbing nobody. With none free, it opens the chooser of section 6.4, with
  the anchor's own receiver among the others.

**Design ruling 6.5a.** When the anchor has another slice of its own on the shared receiver
that the new window would not cover, the band change is not a pan move: it follows the second
bullet (the retuned slice leaves for a free receiver, or the chooser, where picking the shared
receiver is D50's move and picking another is D49's take). Finding 1's rule moves the whole
receiver with the anchor, which would strand the anchor's own other slice; today's C-Tune rule
already refuses a move that leaves the mover's own slice outside (ruling 6.4;
`RadioModel.cpp:6183-6194`), and a device's own slices are never counted as disturbed (ruling
7.3), so there is nobody to ask about them. Screen 10 (one slice of the anchor's, one of
another device's) is the first bullet exactly.

In that case's chooser (no receiver free), the shared receiver is listed with `takeable`
false and a `why` naming the anchor's own slice that would be stranded ("Your slice E would
close."), so the operator never closes their own slice by surprise (re-review finding 6). The
other receivers are D49's take as usual. To use the shared receiver, the operator first
closes or moves that slice, and the band change is then the first bullet.

**Ruling 6.6.** A device that does not anchor its receiver moves its pan by taking the pan,
with its slices on it, to a free receiver centred where it asked: the allocator's own-window
path (`preferOwnStream`, `SliceStreamAllocator.cpp:119-124`). Its slices must fit the new
window, as today's C-Tune rule requires. The anchor and the receiver it leaves are not
disturbed, so nobody is asked and nothing is refused. With no free receiver it gets the
chooser of section 6.4. D50 is not narrowed: it asks whenever a move would disturb
another device, and this move disturbs none.

**Ruling 6.6a. Slices with no owner in a pan move** (fix wave after the group review of Tasks
71 to 76, accepting the code as it is). A pan move (rulings 6.4 and 6.5) names only slices a
device owns or is holding for a device that is away; a slice with no owner (5.2, step 3) that
the new window leaves outside goes to a free receiver or, with none free, closes, with nobody
asked and nobody told (`StationServer::checkPanMove`, the `Apply` kind). Nobody's work is lost:
no device holds such a slice, and a held slice is its owner's and is named and saved as
ruling 5.2 says.

A sample-rate change on a shared receiver is D53 (section 7); slices that no longer fit
the narrower window follow the same move-or-close rule, and the on-air refusal (ruling 7.4)
applies.

### 6.4 Taking a receiver (D49)

The trigger is a request that needs a receiver (add a slice, add a pan, retune out of a
window, move a pan without anchoring it) refused because every receiver is in use
(`SliceStreamAllocator.cpp:145-169`).

- An older window gets today's refusal, its words naming the devices that hold the receivers.
- A device with the new feature gets the refusal and a `confirm.request` of kind
  `takeReceiver`, with one choice per receiver: `{choice, streamIndex, adc, centreHz,
  rateHz, anchorName, slices: [{sliceId, letter, deviceId, deviceName, frequencyHz, mode,
  band, txSlice}], devices: [{deviceId, name, shortName, state, lastActivitySeconds}],
  takeable, why}`. `devices` says what each device on that receiver is doing and when it was
  last active, as in `session.held` (4.3); a free receiver, listed only when Take it back asks
  (below), has no slices. `txSlice` follows ruling 5.4a, and a screen names the receiver
  `streamIndex` + 1 (5.4).
- The operator picks one; the client sends `confirm.proceed {id, choice}`.
- The Core checks again (7.3), closes the slices on that receiver as ruling 6.7 says, tells
  each owning device (`notice` `receiverTaken`, with Take it back), and then applies the stored
  request on the freed receiver, so no one else can claim it in between.

**Ruling 6.7.** Taking a receiver closes every other device's slice on it, even one that would
still fit the new window: D49 says the holder's slice closes, and the controller ruled
every slice on it. The owner was told its slice closes, which is simpler to show in the
chooser and to predict than a partial survival. The taker's own slices on that receiver stay
if the new window still covers them, and are otherwise placed like any retune; they are its
own, so nobody else is affected.

**Ruling 6.8.** A receiver carrying the transmit slice of a holder on the air is listed with
`takeable` false and why ("on the air"). The operator confirmed this narrowing of D49 on
2026-09-24 (D64: "nobody can take the receiver of a slice someone is transmitting on"). When its holder is not keyed, taking it closes the
transmit slice, and the holder's transmit moves to another of its slices, or is released
(through a transfer to nobody, 8.2) if it has none.

**Ruling 6.9.** When the slice cap, not the receivers, is full, the chooser lists slices (kind
`takeSlice`) and taking one closes only that slice, with the same confirmation and notice. The
cap is the board's `maxSlices` (`RadioModel.cpp:7707-7720`) within WDSP's five channels
(`src/core/WdspEngine.h:291`); a board whose slices all share one receiver has free receivers
and no free slice, and taking a receiver would close all of them.

**Ruling 6.10.** A remote window cannot drop its last slice (`RadioModel.cpp:4511-4520`,
`7562-7567`). When a take would close an older window's last slice, the Core ends that
window's session ("iPhone took the receiver this app was using. Update NereusSDR to share
the Core.", not retryable, `takenOver`) instead of leaving a slice on its
screen that no longer exists. D59 settles it: "if a take closes its only slice, it is
disconnected with that reason".

- Take it back (`notice.takeBack`) asks the same question the other way, as the iPhone design's
  D49 has it: a `confirm.request` of kind `takeReceiver` whose first choice is the receiver the
  taker now holds, followed by any receiver free by then, which disturbs nobody. On proceed the
  Core recreates the device's closed slices at their frequencies, modes and pans, with their
  settings (5.3).
- An away device's slices can be taken like any other; its notice waits for its return (4.5).
  Slices held for a device that has left are covered by ruling 5.2.

### 6.5 Capacity per board

From `src/core/BoardCapabilities.cpp`. Slices across every device are capped by the board's
`maxSlices` within WDSP's five channels; a Core may advertise fewer (`effectiveMaxSlices`,
`StationServer.cpp:2502-2509`).

| Board (rows in `BoardCapabilities.cpp`) | ADCs | User receivers | Slice cap | While the holder transmits with PureSignal |
| --- | --- | --- | --- | --- |
| Atlas (296-301) | 1 | 3 | 3 | no PureSignal |
| Hermes (352-357) | 1 | 4 | 4 | user receivers stop (`P1CodecStandard.cpp:907-926`; `P2CodecHermes.cpp:256-283`) |
| HermesII (414-419) | 1 | 2 | 2 | user receivers stop (same codecs) |
| Angelia, Orion (472-481, 534-542) | 2 | 5 | 5 | receivers keep running on DDC2-6 |
| OrionMKII family (596-617) | 2 | 5 | 5 | keep running (`P2CodecOrionMkII.cpp:1222-1246`) |
| G2E / HermesC10 (687-717) | 1 | 4 | 5 | user receivers stop |
| Hermes Lite 2 (815-843) | 1 | 2 | 5 | user receivers stop (`P1CodecHl2.cpp:912-933`) |
| Hermes Lite 2, receive only (945-952) | 1 | 2 | 5 | no transmit |
| Saturn / G2, SaturnMKII, Andromeda (1009-1026, 1076-1093, 1145-1161) | 2 | 5 | 5 | keep running |

- The sample rate is radio-wide on Protocol 1 and per receiver on Protocol 2
  (`RadioModel.cpp:6069-6077`).
- On 2-ADC boards a receiver's antenna picks its ADC: ANT1-3 on ADC0, EXT1/EXT2 on ADC1
  (`P2CodecOrionMkII.cpp:1181-1207`).
- Demodulation load is bounded by the slice cap, not by the number of devices: four devices on
  a Hermes Lite 2 still share two receivers and five slices.

---

## 7. Settings that affect every device (D53)

### 7.1 The list and what each touches

**Ruling 7.1.** D53's rule applies to every change below, not only the six it names:
any device may make it; a change that disturbs another device asks first, naming it; that
device is told afterwards. The list grows with Part H's live-apply Core settings, each Setup
description naming the scope its control touches. Section 15 lists the additions for the
operator.

**Ruling 7.1a (the operator, 2026-09-28): two tiers.** Ruling 7.1's "asks first" now holds only
for a change that can take another device's reception away or reaches the transmitter, and only
when a **connected** device is disturbed:

- **Asks first:** the sample rate (Protocol 1 and Protocol 2), the radio (`station.selectRadio`),
  the receive antenna, the transmit antenna, PureSignal, diversity, 4O3A on or off, the
  amplifier, the tuner and the RF-Kit amplifier's antenna, the transmit interlock and the power
  cap. The transmitter's other Core settings (Receive Only, the transmit region, External TX
  Inhibit), which the ruling does not name, keep asking.
- **Applies at once and tells:** the attenuator, the preamp, the ADC1 preamp and the automatic
  attenuator; a shared receiver's noise blanker; the notches; the receive DSP options; the
  receive filter policy. Nobody is asked, an older window makes them too, and each disturbed
  device is sent the `settingChanged` notice of 7.4 once the change has applied.
- **Away devices are never asked about.** The question names only connected devices; a change
  on an asking row that would disturb only away devices applies at once, and each away device
  is told when it returns (7.4). An away device's slice a sample rate cannot keep closes and is
  saved for its return, as a confirmed rate change closes it.

A change touching rows of both tiers asks. The "Tier" column below is the table
`StationServer::sharedTierOf` reads (`kSharedTiers`, `StationSharedSettings.cpp`), one table for
both tiers; `classifyShared` marks which rows a change touches.

The "Saved as" column gives each setting's keys and their scope in the link's settings table
(the link, section 8): "Core scope" is a key the settings proxy shares with every device;
"Core-owned" is a family that changes only through its object or verb (the link, lines
1312-1333). Both are already shared: this design adds no scope.

| Setting | Changed today through | Saved as | Touches | Tier (7.1a) |
| --- | --- | --- | --- | --- |
| Sample rate, Protocol 1 | `requestSliceSampleRate` (`SessionCommandDispatcher.cpp:317`) | `hardware/<mac>/radioInfo/sampleRate`, Core scope (the link, line 1223; `RadioModel.cpp:18007`) | every receiver (`RadioModel.cpp:6277-6283`, `6476-6483`); stops the radio's data flow (`17886-17898`) | asks |
| Sample rate, Protocol 2 | the same verb | the same | that receiver (`RadioModel.cpp:6284-6287`) | asks |
| Attenuator, preamp | `stepAtt` (`attenuationDb`, `preampMode`, `enabled`, auto-attenuate) | `hardware/<mac>/options/stepAtt/...`, `.../autoAtt/...`, `.../preamp/...`, Core-owned (line 1324) | ADC0 (`P2RadioConnection.cpp:1127-1148`; `P1RadioConnection.cpp:1217-1245`) | tells |
| ADC1 preamp | `stepAtt.rx1Preamp` | the same family | ADC1 (`P2RadioConnection.cpp:1150-1160`) | tells |
| Receive antenna | a slice's `rxAntenna`; `alexAntennas` (`rxOutOverride`, Disable RX Bypass relay, joined at the checkpoint carry of 2026-09-25); `setAlexRxAntenna` | `hardware/<mac>/alex/antenna/...`, Core-owned (line 1325) | the ADC the relay feeds (every receiver on a 1-ADC board) and the receiver's other slices (`RadioModel.cpp:19061-19077`) | asks |
| Receive filter policy | `setAlexBpfMode` | saved for its radio (line 1437) | its chain's ADC | tells |
| PureSignal | `pureSignalSettings`, `transmit.pureSig`, `ps3.*` except two-tone | `hardware/<mac>/puresignal/...`, Core-owned (line 1327) | on a 1-ADC board every user receiver while the holder transmits (6.5); the transmitter | asks |
| Diversity | a slice's `diversityEnabled`, `diversityPhaseDeg`, `diversityGainDb` and `diversityFineNullEnabled` (the last three added by the fix wave after the group review of Tasks 71 to 76) | the `Slice` keys, Core scope (line 1224) | receiver 0 on a 2-ADC board (`P2CodecOrionMkII.cpp:1260-1289`); every receiver on a 1-ADC board (`P2CodecHermes.cpp:284-302`; `P1CodecStandard.cpp:927-941`) | asks |
| Noise blanker, shared receiver | a slice's `nbMode` and NB1/NB2 knobs | the `Slice` and `Nb` keys, Core scope (lines 1224, 1240) | that receiver's slices (`RadioModel.cpp:6737-6760`) | tells |
| Notches | `notch.*`, `notches.globalEnabled`, `notches.autoIncrease` | `NotchCount`, `Notch<N>...`, Core-owned (line 1323) | every slice whose passband holds the notch (one list for every slice: `RadioModel.cpp:5120-5135`; applied per channel, `RxChannel.cpp:1613`) | tells |
| Receive DSP options | `settings.write` (`RadioModel.cpp:18278-18290`) | `DspOptions...Rx`, Core scope (line 1239) | every receiver | tells |
| Transmit antenna | a slice's `txAntenna`; `alexAntennas` (`txAntennas`, `blockTxAnt2`, `blockTxAnt3`, `rxOutOnTx`, `ext1OutOnTx`, `ext2OutOnTx`); `setAlexTxAntenna` (these joined at the checkpoint carry of 2026-09-25); the Alex tab's transmit high-pass switches, `hardware/<mac>/alex/master/{hpfBypassOnTx,hpfBypassOnPs,disable6mLnaOnTx}` (joined at the trunk merge of remote transmit, 2026-09-26) | `hardware/<mac>/alex/antenna/...`, Core-owned (line 1325) | the transmitter (7.5) | asks |
| The amplifier | `amp.operate`, `amp.standby` (Task 42); `configurePgxl` and its settings verbs; `setPgxlOperate`, `setPgxlAddress`, `setRfKitOperate`, `setRfKitTciMode`, `setRfKitAddress` (joined at the checkpoint carry of 2026-09-25; the scans only listen and join no list) | `PGXL_...`, Core scope (line 1226) | the transmitter | asks |
| The tuner, the RF-Kit amplifier's antenna | `tuner.operate`, `tuner.bypass`, `tuner.antenna`, `rfkit.antenna` (Task 42); `configureTgxl`; `moveTgxlRelay`, `setTgxlAddress`, `setRfKitAntenna` (joined at the checkpoint carry of 2026-09-25) | `TGXL_...`, `RfKit_...`, Core scope (lines 1227-1228) | ADC0's receivers on a 2-ADC board, every receiver on a 1-ADC board, and the transmitter | asks |
| 4O3A on or off | `setFourO3AEnabled` (added by the fix wave after the group review of Tasks 71 to 76) | `hardware/<mac>/peripherals/FourO3A_Enabled`, saved for its radio (`RadioModel::peripheralValue`) | what the tuner touches: ADC0's receivers on a 2-ADC board, every receiver on a 1-ADC board, and the transmitter, since it connects or drops the amplifier and the tuner together | asks |
| Transmit interlock, power cap | `setTxInterlockPolicy`, `setPgxlPowerCap` | `PGXL_TxInterlockMode` and its three siblings (`src/core/TxInterlockPolicy.cpp:136-160`), `PGXL_PowerCapEnabled`, `PGXL_PowerCapW` (`src/core/StationAccessoryData.cpp:164-166`), Core scope | the transmitter | asks |
| The radio | `station.selectRadio` (Task 25, the plan, lines 2421-2425) | the Core's saved choice (Task 25) | every slice | asks |

**Ruling 7.2.** The tuner and the RF-Kit amplifier's antenna switch count as touching ADC0 on a
2-ADC board because they sit in the antenna path the transmitter shares, which feeds ADC0; the
receive-only inputs feeding ADC1 bypass them. The Core cannot know more about the operator's
wiring.

A change that touches nothing beyond the requester's own slices applies at once: a slice's own
settings on a receiver it does not share, display defaults, spot and reporter settings.

A `settings.remove` returns its key to the default, live, so it is a write of the default: it
is checked, asked and told exactly as that write would be (fix wave I3, 2026-09-25), with
`change.to` "Default".

### 7.2 The disturbed set

**Ruling 7.3.** A new pure Core class, `DisturbanceCheck`, takes a change's scope (the radio,
ADC n, receiver r, a frequency range, the transmitter) and the Core's topology (every slice's
owner, receiver, ADC (`adcForStream`) and passband; the transmit holder, whether it is keyed,
and its transmit slice), and returns, for each device other than the requester, its affected
slices and what happens to each: `changes` (keeps receiving, differently), `moves` (to another
receiver), `closes`, or `pausesWhileTransmitting`, plus the holder when the transmitter is
touched. The requester's own slices never count.

A rate change is simulated with today's plan (`planStreamSampleRateChange`,
`RadioModel.cpp:6264-6324`). Another device's slice the plan would refuse becomes `closes`; the
requester's own refused slice still refuses the whole change, as today. After Confirm, those
slices close only once the rate change is certain (the plan still holds and, on Protocol 1, the
radio took the new rate), just before it commits; a rate change refused on its later turn closes
nothing and tells nobody (fix wave after the group review of Tasks 71 to 76,
`RadioModel::setStreamSampleRateClosing`). The proceed records each closing slice with its id,
the slice itself and its owner (`SliceOwnership::Mark::subject()`); if, on that later turn, any id
no longer names that same slice or has another owner (closed, and its id reused by any new slice,
the same owner's or an unowned one included), the whole change is refused with "That setting
changed since you asked. Make the change again." and nothing closes (fix wave 2, Important 4; fix
wave 3, Important 1, identity rather than owner alone). Any `requestSliceSampleRate`, confirmed or
not, also re-checks on that turn that its slice is still the requester's, and is refused with the
foreign-slice reason when it is not, and with "That receiver is no longer on the Core." when its
id now names a different slice of the requester's own (or nobody's)
(`SessionCommandDispatcher::handleRequestSliceSampleRate`).

**Ruling 7.4. Changes wait while the holder is on the air (D60).** While a holder is on the
air, these changes from any other device are refused, not asked:

- a change that would move or close the holder's transmit slice (a pan move, a band change
  that moves a shared receiver, a sample-rate change, a take);
- a Protocol 1 sample-rate change, which stops the radio's data flow
  (`RadioModel.cpp:17886-17898`);
- a change to the transmit path (ruling 7.8): the amplifier, the tuner, an antenna (receive or
  transmit, since on these radios the relays that pick them sit in the path the transmitter
  shares), PureSignal, the interlock or the power cap.

The reason names the holder: "The iPhone is on the air. Try again when they stop." (D60's
words, the holder's short name in place of "The iPhone"; "The radio is on the air." when the
radio's own PTT holds it). Only the holder's own action, the Core's safety stops, or another
device's "Unkey and take over" (D51) end a transmission; the other device can use Unkey and
take over, then make the change. A change the holder makes itself is not refused by this rule.
These refusals are built with `TransmitHolder` (Task 34), so they exist from the moment
transmit does (13.3).

The transmit group's second fix round (2026-09-26) settles who counts and what is exempt:
every holder on the air counts, the station device's own keys included (the radio's PTT, a
hosting desktop's MOX or TUNE, a Tuner Genius hardware TUNE); the exemptions are by change,
not by holder. The saved accessory addresses (`setTgxlAddress`, `setPgxlAddress`,
`setRfKitAddress`) and the LAN scans go ahead on the air (the operator's parity ruling), the
addresses still asked of a device that holds transmit (table 7.1, ruling 7.8); the
amplifier and tuner switches wait. The transmit antennas (a slice's `txAntenna`,
`alexAntennas`, `setAlexTxAntenna`) change on the air only for the holder, as in Thetis for
the operator who is transmitting; any other device's change waits (the controller's
ruling). The station device has no session, so it is never asked or told: a change that
would disturb only it applies at once, while ruling 7.4 still holds the changes it names.
The operator confirmed this narrowing of D50 and D53 on 2026-09-24 (D60, D64).

### 7.3 The confirm step

The controller's ruling, made concrete.

1. The change arrives as today: `command.invoke`, `property.write` with a `writeId`, or
   `settings.write`.
2. An empty disturbed set applies it as today.
3. Otherwise, for a device with the new feature, nothing is applied. The change's own answer
   refuses it with the reason "Waiting for you to confirm.": a `command.result` also carries
   `phase` `needsConfirmation` in `values`, as the PureSignal actions carry their phase (the
   link, lines 1347-1356); a `property.result` or `settings.reject` carries the reason. The
   Core then sends `confirm.request` (section 10.2):
   - `kind`: `sharedSetting` (D53), `panMove` (D50), `takeReceiver` or
     `takeSlice` (D49, ruling 6.9), `takeTransmit` (D51).
   - `change`: `{label, from, to}` in plain words, such as "Attenuator, ADC 1", "0 dB",
     "20 dB", or for a pan move the bands, "40 m" and "20 m", so the app can show the change
     itself and word its buttons ("Go to 20 m", "Stay on 40 m"; the iPhone design, section
     5.8 items 10 and 11). Absent for a take.
   - `affected`: one entry per disturbed device, `{deviceId, deviceName, deviceShortName,
     state, holdsTransmit, slices: [{sliceId, letter, frequencyHz, band, mode, adc,
     streamIndex, effect}]}` (review finding 7). `state` is the device's `listening`,
     `transmitting` or `away`, as in `session.held` (4.3); `mode` is the slice's `dspMode`
     value, `adc` the ADC its receiver is fed from (`RadioModel::adcForStream`), from 0, and
     `streamIndex` its receiver, shown as `streamIndex` + 1 (5.4). A screen shows `adc` + 1
     ("ADC 1" for ADC0), and the Core writes `change.label` with the same numbering, so
     "Attenuator, ADC 1" is ADC0's attenuator (re-review finding 7). `effect` is per slice:
     `changes` (keeps receiving, differently), `moves` (to a free receiver), `closes`, or
     `pausesWhileTransmitting`. A holder counted only because the transmitter is touched
     (ruling 7.8) is listed with `holdsTransmit` true and no slices.
   - `choices` (a take), `holder` (transmit, 8.4), `expiresInMs`, and `forCommandId`,
     `forWriteId` or `forSettingsKey` naming the change it holds.
4. The client asks its operator and answers `confirm.proceed {id, choice}` (`choice` -1 when
   the kind has none) or `confirm.cancel {id}`. Nothing the client sent before this answer
   changes anything, so a change that asks is always "confirmed, then applied", with the
   Core's list of what it reaches shown before the operator decides (the iPhone design,
   section 4.5 items 4 and 5). Only taking transmit may be asked on the device first (ruling
   8.7), because the device already has everything that question shows; every other
   question starts from the Core's answer, since only the Core knows what a change reaches.
5. On proceed the Core computes the set again. If it names a device or an effect the operator
   was not shown, the Core sends a new `confirm.request` instead of acting. Otherwise it
   applies the stored change exactly as the original request would have, answers
   `confirm.proceed` with the outcome, the `affected` keys and the readback, and tells the
   disturbed devices (7.4).

**Ruling 7.4a. The proceed's answer carries the readback** (review finding 10). The change is
applied as the requester's own write, so per-writer echo suppression (ruling 5.7) withholds
its notifications from the requester, and without this the asking device would never see its
own change land. `confirm.proceed`'s `command.result` therefore carries, in `values`, what the
original request's answer would have carried had it applied at once:

- for a property write, `objectKey` and, as property entries, the settled value of every
  property the write named, the values today's `property.result` returns for a write
  (`StationServer.cpp:2774-2776` `@aa6c5505`), with its side effects on the written object
  sent to the requester as today's correction `delta` (`StationServer.cpp:2778-2802`
  `@aa6c5505`);
- for a settings write, `settingsKey` and `value`, the stored value, as `settings.value`
  would carry it;
- for a command, the original command's own result values.

The applied change still reaches every other view as a `delta` or `settings.value` (ruling
5.8). A proceed that is refused (expired, the target changed, the set grew) carries no
readback, and the requester's client restores the value it shows from its mirror.

**Ruling 7.5.** A request expires 60 s after it is sent, the answer deadline Task 41 already
uses; a later proceed is refused with "That question has expired. Make the change again." A
device has at most one open request; a new disturbing change from it replaces the old one.
Open requests are dropped when the requester's session ends.

**Ruling 7.6.** A stored change remembers its target's value when it was asked (the property,
the settings key, or what the verb acts on). On proceed, if that value has changed since,
whoever changed it, the requester included, the proceed is refused with "That setting changed
since you asked. Make the change again." A new write from the requester to the same target
cancels its open request.

**Ruling 7.6a** (fix wave I2, 2026-09-25). Every slice a request names (the written slice, a
`sliceId` argument, the slices a move carries) must still be the requester's at proceed; a
slice id is handed out lowest free first (ruling 5.5), so a slice closed within the 60 s may
be another device's under the same id. A request whose slice closes or passes to another
owner is dropped at once, and its proceed is refused as changed ("That setting changed since
you asked. Make the change again." for `sharedSetting`, "What this change reaches has
changed. Make the change again." for the other kinds); nothing is applied.

An older window gets the refusal only, with the reason "This change would affect iPhone.
Update NereusSDR to confirm changes that affect other devices." (D59). The older-window
reasons do not say "computer": a phone that has not yet declared the feature meets them too.

### 7.4 Notices

`notice {id, kind, reason, byDeviceId, byName, byShortName, byKind, bySource, secondsAgo,
takeBack, slices, change}` goes to one device. `byName`, `byShortName` and `byKind` name who
did it; `bySource` is `device` when a device did it and `radioPtt` when the radio's own PTT
took transmit, whose names are then "Radio" and whose kind is `station` (ruling 8.1, review
finding 2), so a device that happens to be called "Radio" is never mistaken for the radio.
`secondsAgo` is how long ago, by the one clock convention (ruling 10.3); the app shows the
time of day from it, by its own clock and in its own time zone. `slices` lists the device's
slices it touched, `[{sliceId, letter, frequencyHz, mode, band}]`, including closed ones,
whose frequency and settings the Core keeps for Take it back (5.3); `change` is the `{label,
from, to}` of a `settingChanged` (7.3). A notice about the device's own state (the last three
kinds below) has no `by` keys.

| Kind | From | Take it back |
| --- | --- | --- |
| `settingChanged` | D53 | no (D53 says only "told afterwards") |
| `sliceMoved`, `sliceClosed` | D50, a narrower rate | no (D50) |
| `receiverTaken`, `sliceTaken` | D49 | yes |
| `transmitTaken` | D51 and D52 | yes |
| `placeTaken` | D55, a device replaced while away (4.3) | no (it has a place again) |
| `antennaKept` | D61, the receive antenna stayed put (ruling 5.11a) | no |
| `graceEnded` | D62, the device came back after its 3 minutes (4.5) | no |
| `slicesNotRestored` | saved slices that did not fit at admission (5.2) | no |

Take it back is a verb, `notice.takeBack {id}`: for a receiver or a slice it asks the take
question the other way and, on proceed, recreates the closed slices with their settings
(6.4); for transmit it is `tx.take` (8.6). An away device's notices wait for its return and
are sent right after its `snapshot.complete`. When it comes back after its 3 minutes have
ended they still arrive, after its `graceEnded` notice and without Take it back, so it
learns who took what and when; the Core keeps them until the device returns, it is revoked,
or the Core restarts. A slice taken from an away device lives only in its notice while Take it
back is possible; when the 3 minutes end first, Take it back ends and the slice is saved in the
device's `DeviceLayoutStore` with its settings, as ruling 5.2 saves a held slice another device
closes, so the device's next admission restores it (fix wave 2,
`StationServer::saveTakenSlicesFor`). A Take it back already delivered (the device was there
when its slice was taken) ends the same way: when the device's next away period ends, the record
goes and the slice is saved then (fix wave 3; `ConfirmStep::endTakeBacks` ends every record the
device has, delivered or waiting). The save needs the radio the store is kept under: when no
radio is connected as the 3 minutes end, nothing can be saved, so Take it back is kept and
arrives with the notice at the device's return; it ends, the slice saved, at the end of the
device's next away period with a radio connected (fix wave 3, the re-review's Minor 2).

### 7.5 The transmitter's own settings

**Ruling 7.7.** While transmit is held, the transmitter's settings (the `transmit` object's
controls of Task 40, the plan, lines 3693-3698; `txProfile.select`) and the actions that put a
carrier on the air (`tx.twoTone`, `ps3.twoTone`, `tuner.tune`) are the holder's. Another
device's change is refused with "iPhone has the transmitter." With transmit unheld, any device
may change the settings; an action that puts a carrier on the air takes transmit first, as a
key does (ruling 8.3), so a carrier never goes on with nobody holding transmit (re-review
finding 2). Arming VOX needs transmit (ruling 8.4).

**Ruling 7.8.** The other changes in 7.1 that touch the transmitter (transmit antenna,
amplifier, tuner, RF-Kit antenna, PureSignal, interlock, power cap) follow D53, with the
holder counted as disturbed, so asked for, named and told, when the requester is not the
holder. While the holder is on the air they wait: they are refused with the on-air reason
until the holder stops (D60, ruling 7.4).

---

## 8. Transmit (D51 and D52)

### 8.1 The holder

**Ruling 8.1.** A new Core record, `TransmitHolder` (`src/core/safety/`, beside Task 34's
`StationTxGate` and `UnkeyGate`): the holder's device id, name and kind, since when, whether it
is keyed, and an epoch that advances with every change of holder. It has three states:

- **unheld**: nobody holds transmit;
- **held**: one device holds it, keyed or not, or away in its grace period (never keyed);
- **transferring**: the Core is taking transmit from a device, to give to another device or to
  nobody.

It starts unheld. Every session learns the holder's name and whether it is on the air (settled
detail S2). `txState`, Task 39's object (the plan, lines 3647-3655), gains `holderDeviceId`,
`holderName`, `holderShortName`, `holderKind`, `holderSource`, `holderForSeconds`,
`holderEpoch`, `holderAway` and `holderTransferring`, beside its existing `keyed`, which says
whether the holder is on the air. Task 39's planned `keyedSinceMs` (station clock, the plan,
line 3649) becomes `keyedForSeconds`, by the one clock convention (ruling 10.3), so the TX
clock and "holds transmit for 5 minutes" read the same way on every screen. `txState` goes to
every session, as Task 39 sends it; only a window built before Task 39, which drops a class it
does not know (the link, section 7.1), shows no transmit state. `connectedDevices` carries the
same per device (10.3).

**`holderSource`** (review finding 2) says how the holder got transmit: `device` for a device
that took it, and `radioPtt` for the radio's own PTT (a mic or a footswitch, D52 as amended by
D58). It is the key an app reads to tell "Radio" from a device, never the name: a device the
operator happened to call "Radio" still has `holderSource` `device`. `holderKind` stays the
device kind, `station` for both the station device's takes.

The holder's names are set by the take and stay until the next change of holder:

- After a take by the radio's own PTT input (a mic or a footswitch), on any Core,
  `holderName` and `holderShortName` are "Radio", `holderKind` is `station` and
  `holderSource` is `radioPtt`. Every D52 take shows this way. On a Core with no desktop this
  is the station device's only way to take transmit.
- On a hosting desktop, a take by the desktop's own MOX or TUNE (which asks like any device's,
  D64, ruling 8.9a) shows the desktop's name and short name, kind `station`, source `device`
  (Task 48's kind, ruling 4.1).
- A program never takes transmit (D58, D63), so no take is ever named after one.
- Otherwise it is the device's name and short name, numbered as ruling 4.3 numbers them,
  source `device`.

A press of the radio's PTT while the station device already holds transmit is a key, not a
take: the names and the source stay as they are.

`keyedByName` and `keyedByKind` always name the holder (`keyedBy` names the holder, section
2.2). `stopReason` `takenOver` covers transmit taken by another device or by the radio's PTT.
Task 35's keying epoch stays per key.

### 8.2 The transfer

**Ruling 8.2.** Every change of holder, and every release, is a transfer:

1. Enter **transferring**. From here until step 3 ends, the Core refuses every key from every
   source (the old holder's `tx.key`, `tx.tune` and `tx.twoTone`, the radio's PTT, VOX, TCI,
   any other device) with "Transmit is changing hands. Try again in a moment." Unkeying is
   never refused.
2. If the old holder is keyed, the Core runs `UnkeyGate::unkey` (Task 34) and waits for
   `Confirmed` or `TimedOut` (2000 ms, after which the stop has still been applied, the plan,
   lines 3329-3332). Keyed or not, it then reads MOX. The Core assigns no holder while MOX
   reads on: it stops again (`stopAllTx`, Task 33) and waits up to another 2000 ms. If MOX
   still reads on, the transfer ends with transmit unheld and keys still refused until MOX
   reads off, and every device is told "The radio did not confirm it stopped transmitting."
3. With MOX off, the Core assigns the new holder, **unkeyed**, or leaves transmit unheld;
   disarms VOX (ruling 8.4); binds the transmit slice (ruling 8.10); advances the holder epoch;
   tells the old holder; and publishes `txState`.

A new holder always starts unkeyed. A transfer happens on: a take by a device (8.4), a take by
the radio's PTT (8.5), a fifth device replacing the holder
(4.3), the holder's last slice closing (ruling 8.12), and the holder leaving on purpose, being
revoked or its 180 s ending (ruling 8.15).

A holder going away (its link dropped, section 4.5) is not a change of holder. The Core stops
its transmission at once and runs step 2's fence, keys refused until MOX reads off, then keeps
transmit held for it, away and unkeyed (ruling 8.15).

### 8.3 Keying and taking

**Ruling 8.3.**

- The holder keys as Task 35 defines (`tx.key`, `tx.tune`, `tx.twoTone`). Every action that
  puts a carrier on the air is a key for these rules: `tx.key`, `tx.tune`, `tx.twoTone`,
  `ps3.twoTone` and `tuner.tune` (the tuner's tune keys the radio at its tune power).
- **A person's key on unheld transmit takes it and keys (D63).** A device's `tx.key`,
  `tx.tune`, `tx.twoTone`, `ps3.twoTone` or `tuner.tune` from a person (any `trigger` but a program's, below) while
  transmit is unheld makes it the holder, unkeyed, and then keys; nobody is asked, and there
  is nothing to wait for, since nobody is keyed; every device's `txState` then names it. It
  differs from D51's rejected "Whoever keys first" because nobody is taken from. If MOX still
  reads on after a failed transfer (ruling 8.2), the key is refused until it reads off.
- **A program never takes transmit (D58, D63).** A key a program sends is `tx.key {trigger:
  "tci"}` from a remote window's TCI server (Task 35, the plan, lines 3399-3402), and CAT's
  own trigger once CAT is built; on a hosting desktop it is the local TCI or CAT server's key
  through the same gate (ruling 8.13). It keys only while its device already holds transmit.
  Otherwise it is refused, held or unheld: with transmit held by another device, with
  `otherDeviceHolds` as below; with transmit unheld, with a new `TxRefusal`,
  `programNeedsTransmit`, text "A program can transmit only while this device has
  transmit. Take transmit here first.", fix `takeTransmit`. The window the program runs
  through shows the refusal (D58: "the window says why"). A program therefore never makes
  anyone the control operator without them choosing it.
- A device's key while another holds transmit is refused with Task 34's refusal, renamed from
  `otherDeviceKeyed` (the plan, line 3321) to `otherDeviceHolds`: text "iPhone has the
  transmitter.", fix `takeTransmit`. It names an away holder the same way.
- `tx.take {holderEpoch, shownKeyed}` (both optional) takes transmit without keying: at once
  when unheld, through the confirmation when held (8.4, ruling 8.7).

**Ruling 8.4.** VOX keys on audio, not on a person, so it follows the holder strictly. Arming
VOX (`transmit.voxEnabled`) from a device that does not hold transmit is refused; its client
takes transmit first. The Core disarms VOX at every change of holder, when the holder goes
away, and whenever transmit becomes unheld; re-arming needs holding transmit again. Task 36's
mic source "follows the remote device while it holds transmit or has VOX armed" (the plan,
lines 3447-3448) therefore always means the holder.

**Ruling 8.5.** Who may unkey:

- `tx.unkey` is the holder's. A device that does not hold transmit stops a transmission only
  by taking transmit ("Unkey and take over", D51); its `tx.unkey` is refused with
  "iPhone has the transmitter. Take it to stop the transmission."
- The station device's own releases (the MOX button, the radio's PTT released, a program's
  `trx:N,false` through a hosting desktop's TCI server) release only the station device's own
  key (ruling 8.8).
- The Core's safety stops (the watchdog, starvation, the time-out, the interlock,
  `stopAllTx`) unkey whoever is keyed.

**Ruling 8.6.** Taking transmit never keys by itself; the device keys with its next `tx.key`.
An app may send that `tx.key` straight after a confirmed take if its screens want one press to
do both. The radio's PTT is the one exception, and even it keys only after the transfer ends
(ruling 8.9).

### 8.4 Taking transmit from another device (D51)

- `tx.take {}` while another device holds transmit: the refusal and a `confirm.request` of
  kind `takeTransmit` whose `holder` is the holder's entry in the shape of `session.held`'s
  (4.3): name, kind, how long connected, when last active, what it is doing, and its TX clock
  (`transmittingForSeconds`) while on the air. The client shows "Take transmit from iPhone?",
  or the red "Unkey and take over" when the holder is on the air.
- **Ruling 8.7.** The confirmation may happen on the device first. A client that has shown
  its operator the question from what it already has (`txState` and `connectedDevices`, 10.3)
  sends `tx.take {holderEpoch, shownKeyed}` after the operator confirms. The Core takes at
  once when `holderEpoch` still names the holder that was shown and the holder is not on the
  air now unless `shownKeyed` is true; otherwise it answers with a `confirm.request`, as for
  a `tx.take` without them. On a proceed, likewise, if the holder was not keyed when asked
  and is keyed now, the Core sends a new `confirm.request`. Either way the operator always
  sees the red question before a carrier is cut.
- Then the Core runs the transfer (ruling 8.2). The result, of the `tx.take` or of the
  proceed, arrives when the transfer ends, with the new holder unkeyed, the old holder told
  (`transmitTaken`, with Take it back), and `txState` showing the new holder on every
  device.
- **A take during grace** (settled detail S4). While the holder is away in its grace period
  nobody is on the air, so the take asks D51's question without the red button, naming
  the away device ("Take transmit from iPhone? It has been away for 40 s."; the holder's
  `state` is `away` in the request and in `connectedDevices`). The transfer has nothing to unkey.
  The away device does not get transmit back when it returns; its `transmitTaken` notice, with
  Take it back, waits for it (7.4).

### 8.5 The radio's own PTT (D52), programs, and the hosting desktop's buttons

The sources that take transmit at once, with the press as the confirmation, are the radio's
mic PTT and a footswitch on its PTT input, both arriving as the radio's PTT bit
(`RadioModel.cpp:11755-11758`): D52 as D58 amended it, "a mic or a footswitch". They key as
the station device, on any Core, with or without a desktop.

Programs on the Core's computer no longer count (D58): on a hosting desktop a program keying
through the window's TCI server, or its CAT server once built (`RadioModel.cpp:1317`), keys
only while that window holds transmit, and is otherwise refused and the window says why
(ruling 8.3); on a Core with no NereusSDR window there is no window to hold transmit, so its
programs cannot key and its TCI server stays receive-only (`StationTciController.cpp:44-45`).
The hosting desktop's own MOX and TUNE buttons (and its space bar, once wired,
`RadioModel.cpp:1322`) ask like any device's (ruling 8.9a).

**Ruling 8.8.** The keying gate runs on a press edge, before any keying state changes. The
radio reports its PTT on every status frame as a level (`RadioModel.cpp:1307-1311`), so the
Core acts only on a change from released to pressed and from pressed to released. Today
`onMicPttFromRadio` sets `PttMode::Mic` and then keys (`MoxController.cpp:1196-1197`), so a
gate inside `setMox` would refuse a key only after the mode had been overwritten; and a
release unkeys whenever the mode reads Mic (`MoxController.cpp:1207-1208`), which
`setMox(false)` never clears (`MoxController.h:562-565`). With the gate first, a refused press
leaves the mode alone, and a release unkeys only the station device's own key: after a remote
device has taken transmit back while the station's microphone is still held, releasing that
microphone does not unkey the remote holder. The gaps plan's Task 7 sets, clears and guards
PTT modes by source as Thetis does (the gaps plan, lines 233-264); it lands before Tasks 34 and
77, which build on it. Every other keying entry point (a remote `tx.key`, VOX, TCI) asks the
same gate before it touches any keying state.

**Ruling 8.9.** When another device holds transmit, a press takes it without a question (the
press is the confirmation): the Core runs the transfer to the station device (ruling 8.2),
which unkeys a holder on the air first. The press keys nothing while the transfer runs. When
the transfer ends with MOX off, the station device holds transmit, unkeyed; if the PTT is
still down, the press that asked for the take then keys, as a new key by the station device
through the gate. A PTT released during the transfer keys nothing. A PTT still held after
another device takes transmit back does not take it again; the next press does. Without this,
a held hand microphone would take transmit back on every status frame.

**Ruling 8.9a. The hosting desktop's own MOX and TUNE ask (D64, D35).** When another device
holds transmit and the operator at a desktop that hosts the Core presses its MOX or TUNE
button, the desktop shows the same question as any device ("Take transmit from the iPhone?",
red "Unkey and take over" when that device is on the air), from its own `TransmitHolder`
state, and takes through `tx.take`'s rules (ruling 8.7) before it keys. The desk microphone,
which is the radio's own PTT, still takes transmit at once (ruling 8.9). With transmit unheld,
the button takes and keys at once, as any person's key does (D63).

**Ruling 8.9b. The Tuner Genius's own front-panel TUNE takes as the radio's PTT does
(maintainer, 2026-10-01).** The operator pressing TUNE on the Tuner Genius is at the station,
so the press is the confirmation, as for the radio's own PTT (ruling 8.9). While another device
holds transmit, the press takes it through the same transfer (the holder is told
`transmitTaken`, by "Radio", source `radioPtt`), keys nothing while the transfer runs, and keys
the tune carrier when it ends only if the tuner's cycle is still running. Every other gate
stays: the cycle never starts while the radio is on the air (so it never unkeys a holder), the
amplifier rules, TX inhibit, the PA trip and receive only, and the 3 s start watchdog. The press
is only a `transmit tune on` that the connected Tuner Genius sends from its own address on the
SmartSDR API port and answers nothing the Core sent the tuner. The tuner answers each `autotune`
the Core sends (the Tuner page, a device's tune, the band-change recall) and echoes each
`transmit tune=1/0` the Core broadcasts with its own tune on or off. The Core counts each of
these until its answer arrives, the tuner refuses that `autotune`, the sweep it started ends, or
3 s pass (the capture shows the answer at 503 ms; 3 s is the Core's existing tuner start
window). A tune on is the tuner's own TUNE only when nothing is waiting for one.
The tuner's `tuning=1` alone, another SmartSDR API client's line, and a device's `tx.tunerTune`
never take; they are station or device keys under ruling 8.9a and the holder rules.

*Implementation note (not a ruling; awaiting JJ's confirmation, 2026-10-01).* How the TGXL tune
lane implements the counting above:

- The Core counts every tune=1 frame it writes to the SmartSDR API port: the change, the 1 s
  re-send, a new client, and a subscription push. It counts them whether or not the tuner's
  own :9010 link is up, because how often the tuner echoes has never been measured. A tune=0
  is counted only when it changes, since an idle tune=0 push gives the tuner nothing to echo.
- Each `autotune` is counted by link and sequence. It owns the first `tuning=1` after it, as in
  the captures.
- A tune on that answers an echo, or a running cycle's `autotune`, is dropped: it never keys
  and never takes.
- **Exception:** a tune on that answers the band-change recall's `autotune` keys the carrier
  that sweep needs, as a station key under ruling 8.9a, and never takes. This matches what
  the Core did before the lane, because the recall's sweep has no other carrier.
- Takes never outnumber the tuner's real presses. A press inside the 3 s window after a tune
  can use up a waiting entry and be dropped, which fails closed.
- The Core logs each answer's kind and latency and the echoes per tune, so a bench tune can
  size the window.

### 8.6 Take it back

For transmit, `notice.takeBack` is `tx.take` with its usual confirmation: red when the taker is
on the air, including a person on the radio's microphone.

### 8.7 The transmit slice

**Ruling 8.10.** When a transfer assigns a new holder, the Core binds the transmit slice to the
new holder's chosen transmit slice if it still exists, otherwise to its active slice.
`tx.setTxSlice` (Task 34, the plan, lines 3333-3335) is the holder's, for its own slices; from
any other device it is refused.

**Ruling 8.11.** On a Core with no desktop, the station device owns no slice of its own while
devices are connected (5.2). When the radio's PTT takes transmit there, the transmit slice
stays where it is, on another device's slice: the radio transmits on its current transmit
frequency, as a radio on its own would. This is where D51's "among its own slices"
cannot hold (section 15). While the station device is keyed, that transmit slice is frozen: its
owner's retune, mode or filter change and its closing are refused with the on-air reason, and
no take, move or rate change may touch it (ruling 7.4). The freeze ends with the press. The
operator confirmed this narrowing of D45 on 2026-09-24 (D64: "while the radio's own mic or
footswitch is keyed, the slice it transmits on can't be retuned until the press ends"). What
the slice's owner sees is in ruling 5.4a: no TX on its slice, since the radio holds transmit,
and the freeze read from `txState`.

**Ruling 8.12.** When the holder's transmit slice closes (its owner closed it, or a take while
unkeyed), the transmit slice moves to another of the holder's slices. With none left, transmit
is released through a transfer to nobody (ruling 8.2), which unkeys first. Every release works
this way. With transmit unheld the binding stays where it was, keeping the arbiter's one-slice
rule (`TxSliceArbiter.h:25-27`); `txState` shows no holder.

### 8.8 MoxController and TxSliceArbiter

**Ruling 8.13.**

- **MoxController.** A new keying gate, `setKeyingGate(...)`, beside the band-plan check
  (`MoxController.h:272-273`) and the interlock (`MoxController.cpp:502-517`), is asked on
  every press edge and every remote key, with the source (`PttMode`) and the keyer, before the
  PTT mode or MOX changes (ruling 8.8). `TransmitHolder` answers: admit; refuse with a reason
  (including "changing hands" while transferring); or take, then key if still pressed (ruling
  8.9). `setMox(bool)` stays for the Core's local callers, which key as the station device; a
  new `setMox(bool on, const KeyerIdentity& keyer)` serves remote keying. A release names its
  keyer and unkeys only that keyer's key (ruling 8.5); unkeying stays ungated
  (`MoxController.h:256-257`). `keyedBy` (Task 35, the plan, lines 3403-3405) comes from the
  keyer.
- **TxSliceArbiter.** It gains an owner lookup beside `setSliceList`
  (`TxSliceArbiter.h:50-52`): `requestHandoff(sliceId)` refuses a slice its requester does not
  own; a new `bindForHolder(holder, preferredSliceId)` moves the flag when a transfer ends;
  `syncToSliceList`'s first bind picks among the holder's slices when there is one; the flag
  never moves while the station device is keyed (ruling 8.11). A remote window's arbiter still
  does nothing (`setRemote`, `TxSliceArbiter.h:54-65`).

### 8.9 TCI transmit, and the gaps plan's Task 4

**Ruling 8.14.** The gaps plan's Task 4 makes a second TCI app's `trx` do what Thetis does,
keying or refused (the gaps plan, lines 172-176). On a Core with several devices two rules
apply, in order:

1. **The holder rule.** A program keys only while its TCI server's device already holds
   transmit (D58). It never takes transmit, held or unheld (D63), on a remote window and on a
   hosting desktop alike (ruling 8.3); the Core's own TCI server on a Core with no window
   stays receive-only.
2. **The Thetis rule.** Among the programs of one TCI server, which all act as the same
   device, Task 4's Thetis behaviour decides whether a second program's `trx` keys.

Where the two differ, the holder rule wins: nothing lets a program key for a device that does
not hold transmit. The TCI audio lock (`m_txAudioActiveClient`, `TciServer.cpp:2676-2719`) is
taken only after the holder rule admits the key, and a program's `trx:N,false` releases only
its own device's key. Task 35's plan to key through the Core's own TCI server (the plan, lines
3399-3402) is withdrawn (D58); its remote-window path, `tx.key {trigger:"tci"}`, stays, under
the holder rule.

### 8.10 When the holder leaves

The controller's ruling: unkey at once. Settled detail S4 (D56, D62) then keeps transmit held
for a device that drops, for its 3 minutes.

**Ruling 8.15.**

- **A dropped holder** (a lost link, the heartbeat, the app stopped by its system) is unkeyed at
  once, as Task 37 stops a dropped holder (the plan, lines 3520-3521), through the transfer's
  fence (ruling 8.2): keys refused until MOX reads off. Transmit then stays held for it, away
  and unkeyed, with VOX disarmed. Other devices' keys are refused naming it; another device's
  take asks D51's question without the red button (ruling 8.7); the radio's own PTT
  still takes it on the press (ruling 8.9).
- **It comes back within 180 s** (the same device signing in again, ruling 4.8): if nobody took
  transmit meanwhile, it still holds it, unkeyed, and keys with its next press. If another
  device took it, it does not get it back; its `transmitTaken` notice, with Take it back, is
  waiting.
- **Its 180 s end, it leaves on purpose, it is replaced by a fifth device, or it is revoked:**
  transmit is released through a transfer to nobody (ruling 8.2) and becomes unheld. A device
  whose 180 s ended learns it from `graceEnded` when it comes back (4.5), never from screen
  15's "transmit was still yours".

D21's reason is kept: a device that lost its link is never locked out of its own transmitter by
the drop itself, and it never comes back keyed.

---

## 9. Media and capacity (D54)

### 9.1 A media controller per device

**Ruling 9.1.** One `DaemonMediaController` per admitted session, each with its own media peer,
display endpoints (at most 8, `DaemonMediaController.cpp:35`), receiver streams (at most 4,
`IMediaTransport.h:91`) for its own slices only, headphones mix and audio clock. Each session's
`media.control` reaches its own controller.

- A receiver's FFT stays shared between everyone watching it, as the budget design keeps it
  (the budget design, section "Capacity descriptor and accounting"). A device subscribes a
  display only for its own slices; its pans ride its slices' receivers, shared or not. When a
  slice passes to another owner, the old owner's displays on it retire exactly as a removed
  slice's do (reason `slice removed`), since that device's view destroys the slice (ruling
  5.8); its receiver streams already stopped as `slice-removed` (fix wave after the group review
  of Tasks 71 to 76, `DaemonMediaController::retireSliceDisplays`).
- A hosting desktop's own window draws locally and sends nothing over the network, so it has no
  media controller.
- Telemetry (`station.metrics.v1`) goes to every session that negotiated it.

### 9.2 Each device hears its own slices

**Ruling 9.2.** AudioEngine's mixer builds one mix per owner (today one master tap,
`AudioEngine.cpp:2161-2191`), each summing that owner's slices with their gain, pan and mute. A
device's main audio (the speakers' mix) and headphones mix come from its own. The Core's local
output plays the station device's mix: a hosting desktop's window's slices, or the slices a
headless Core holds for absent devices. Receiver streams are offered only for the device's own
slices. Slice audio settings (`afGain`, `muted`, `audioPan`, `outputRoute`,
`MirrorPolicy.cpp:114`, `152-153`, `158`) stay slice properties, now each belonging to one
device. The holder's mix carries the transmit monitor (Task 36).

### 9.3 Splitting the display budget

Today one budget is "in force for the current session" (`StationServer.h:447-451`), set by
configuration or by the load governor
(`src/core/session/media/DisplayLoadGovernor.h:65-148`).

**Ruling 9.3.** A new pure class, `DisplayBudgetSplit`, takes the Core's total limits, the
admitted network devices, each one's requested charge and the holder, and returns one
`DisplayBudgetLimits` per device, each with its own generation, advertised in that device's
capabilities (the existing budget entries, the link, section 6.4):

1. A network holder gets its whole request, up to the Core's total: the iPhone design keeps
   the transmit holder whole (D54; its error table, "Keeps the transmit holder whole").
2. What is left is shared among the other network devices, equally, and a device asking for
   less than its share leaves the difference to the rest (max-min fair).
3. With transmit unheld, held by the station device (which sends no display over the network),
   or held by a device that is away, every network device gets an equal, max-min fair share of
   the total.
4. PureSignal's display is charged once, to the device that subscribes to it. The governor's
   floor counts it (`DisplayLoadGovernor.h:141-143`); charging it per device would count it
   four times.

**The requested charge is demand, not grant** (fix wave I5, 2026-09-25; fix wave 2,
2026-09-25; fix wave 3, 2026-09-25). A device's request is the sum of its display subscriptions'
charges as subscribed, at the frame rate it asked for and the pixels it asked for clamped to what
its window can carry (the source bins in the window at the engine's FFT size,
`SpectrumEndpoint::grantedPixels`; a device asking for more pixels than its window has bins
would otherwise keep a demand it cannot use, taking share from the others and hearing
`sharedConnection` while fully served), before the budget clamps them, a subscription refused
for the budget included (until the bound below), and never less than one useful pan: 256 pixels
at 10 frames a second with its wide plane, the governor's own floor pan
(`DisplayLoadGovernor::floorPanCharge`, the same floor as `DisplayLoadGovernor.h:135-136` and
`RemoteDisplayAllocator.cpp:20-21`). The floor is what keeps a device that joins second from
being starved: before it has subscribed anything, or when it is an older client that plans
inside its share and so never asks for more, it is still counted as wanting one pan, so another
device that is not a present holder, asking for the whole total, cannot leave it less than one
useful pan while the total holds one for each device (below). The link carries nothing that
tells a sound-only device from one that has not subscribed yet, so every admitted network device
is floored, the sound-only one included; the floor it keeps is a budget it never spends. What no
device asks for is shared equally among the devices as room to grow into, so a device alone
keeps the whole total, as before shares. A subscription is admitted against the share the device
has with its new request (for a present holder, what rule 1 gives it), not the share it had.

**What the floor guarantees, exactly** (fix wave 3, the re-review's Minor 1 and its first
out-of-scope item):

- With no present network holder (rule 3, or rule 2's others when the holder is the station
  device or away), each admitted network device's share is at least the smaller of one useful
  pan and an equal part of the total, in each field.
- The load governor never cuts the total below PureSignal's display plus one useful pan for
  each network device sharing the budget (`DisplayLoadGovernor::floorCharge(pans)`, fed
  `StationServer::displayBudgetSharingCount` on every reading), capped at its ceiling; when a
  device joins while a cut is in force, the cut rises to that floor at once. So a cut never
  pauses every device's display: without a present holder each device keeps one useful pan
  under any cut.
- A total smaller than that can come only from configuration: a display allowance configured
  for the Core (`DaemonConfig::displayBudgetLimits`) below one useful pan per device. Then each
  device gets an equal part of it, which can be less than one pan, and a device whose part is
  too small pauses its display, sound kept.
- Beside a present network holder (rule 1, with Task 34), the other devices share only what the
  holder's request leaves. A holder asking for the whole total leaves them a share of 1 (the
  least a share can be), so their displays pause, sound kept, for as long as it asks for that;
  the floor does not reserve anything against a holder.

**A refused request ends** (fix wave 2, Important 2). A display the client drops, closes or
pauses leaves its device's request. The client unsubscribes a display the Core refused when it
drops it (the desktop does, `RemoteMediaController::sendRefusedRelease`), and the Core does not
rely on that alone: a subscription refused for the budget counts in the request until the client
asks for that endpoint again (a new subscribe replaces it), unsubscribes it, or does not ask
again within `DaemonMediaController::kRefusedDisplayDemandHoldMs` of the refusal. The refusal is
sent after the `capabilities` carrying the share the request produced (the next budget
generation it was sent), and a planner that still wants the display re-plans at once: the
desktop's runs on every capabilities change and every 100 ms. The hold is the app's own
allocation acknowledgement timeout (`kDisplayAllocationAckTimeoutMs`, 10 s), the longest the
app waits for the Core's answer on a slow link, so a re-ask within the same round trip is never
missed, while a display dropped without a word stops cutting the other devices then. When the
hold ends the endpoint asks for what it asked before the refusal (its live display's request,
or nothing).

**Every client asks for what the operator wants** (fix wave 2, Critical 1). A planner that only
ever asks for what its share allows never shows its demand, so its share never grows past the
equal part it started with, and rules 2 and 3 fail for a device that joins second. So every
client, holder or not, subscribes each pane at the pixels and frame rate the operator wants. A
subscription refused for the budget (reason "The Core's display limit has no room left.") is
answered by the `capabilities` share the Core publishes with the new request, sent before the
refusal; the planner then plans inside that share as before (lower background frame rates first,
then detail, then the active pan, down to one pan at 256 pixels and 10 frames a second) and
subscribes again, and what it subscribes then is its request. The demand the Core records is
therefore what each device asked for, and the split is fair. A planner asks again when what the
operator wants grows (a pan added, a pan made wider or faster), and when the transmit holder
changes (fix wave 3, the re-review's Important 2). A pan made wider by a resize (the same pans at
the same frame rates, only their widths changed) asks once the widths have stayed put for
`RemoteMediaController::kResizeSettleMs`, 200 ms, two of the desktop planner's 100 ms ticks
(`kPlannerIntervalMs`), so a window dragged wider asks once when the drag stops, not at every
step (fix wave 3, the re-review's Minor 3); any other growth asks at once. The holder changes that
ask:

- this device becomes the present holder (it takes transmit, or comes back from away holding it):
  rule 1 gives a holder its whole request, and its request is the plan it made inside its old
  share until it asks again;
- the holder changes to another device, or to the station device or the radio's PTT;
- a holder lets go or goes away (transmit released, or its device away): rule 3's equal shares
  come back only for devices that ask again, since a device held down beside the holder asks for
  what it planned there.

The client reads these from the holder notification (`txState`'s `holderEpoch`, which moves with
every change of holder, a release included, and `holderAway`; ruling 8.1): a change of either is
an ask. Asking again on every new generation would let two constrained devices trade their
planning leftovers back and forth without end; a change of holder is an operator event, not a
generation, and rule 1 is not symmetric (only the holder is kept whole), so asking on it cannot
loop. A share that grows because another device asks for less is grown into by planning inside
it.

- The desktop's planner (`RemoteMediaController::refreshBudgetSubscriptions`, with
  `RemoteDisplayAllocator`) does this: it plans without the share while asking, sends every pan's
  wanted request whatever the share, ends the ask at the first budget refusal or once every pan
  has been answered, and then plans inside the share. Its holder trigger is
  `RemoteMediaController::setTransmitHolder(holderEpoch, holderAway)`. This branch receives no
  holder notification (the Core's holder is always unheld until Task 34), so nothing calls it
  yet: **the merge with Task 34 connects the client's `txState` (`holderEpoch`, `holderAway`) to
  it**, and replaces the Core's test seam `StationServer::setDisplayBudgetHolderForTest` with
  `TransmitHolder`'s holder in `splitDisplayBudget`.
- The phone's planner, `DisplayQualityAllocator` (phone Task 52), must do the same: at the start
  of its media session, whenever the displays the operator wants grow (a pane widened by a
  resize or a rotation once its width has held for 200 ms, not at every step), and whenever
  `txState`'s
  `holderEpoch` or `holderAway` changes (it takes transmit, the holder changes, a holder lets go or
  goes away), subscribe every visible pane at its wanted pixels and frame rate; on an
  `allocation-result` refused with the reason
  above, plan inside the share in the `capabilities` it already holds and subscribe the planned
  qualities; otherwise plan inside the share as the budget design says; never ask again only
  because a new generation arrived; and unsubscribe any endpoint the Core refused that it then
  drops (a pane paused, closed or hidden).

**The phone computes its own frame rate** (review finding 5). The Core hands each device a
share, never a frame rate. Inside its share each device's own client plans its displays: it
lowers background frame rates first, then detail, then the active pan, down to one useful pan
at 256 pixels and 10 frames a second (the budget design, lines 93-102; the desktop's planner,
`RemoteDisplayAllocator.cpp:20-21`, `87` and `109` `@aa6c5505`; the Core's matching floor,
`DisplayLoadGovernor.h:135-136` `@aa6c5505`), and the Core admits against the share, so
"frame rates dropping first" holds per device. The rate the "Sharing" chip shows ("Sharing ·
12 fps", the iPhone design, section 5.8 item 7) is the active pan's rate in the phone's own
plan; the `fps` of a display endpoint's `context` only echoes what the phone subscribed at, so
it is not a separate answer from the Core. On the phone the planner is
`DisplayQualityAllocator`, phone Task 52 (the plan at `590d2e36`, lines 4479-4489), whose
settled frame rate is what the chip shows (re-review finding 11). A share too small even for that one pan (beside a
present holder whose request leaves too little, or under a configured display allowance smaller
than one useful pan per device; see "What the floor guarantees" above) suspends that device's
display as the budget design already does, pane and slice kept and the band marked paused
(the budget design, lines 107-112); its sound is never cut (ruling 9.4; D64).

What a slowed device is told:

- `capabilities` carries its own share in the existing budget entries
  (`displayApplicationBytesPerSecond`, `spectrumSampleUnitsPerSecond`,
  `displayBudgetGeneration`; the link, section 6.4), and a `displayBudgetReason` that says
  which of the Core's limits is short, while its share is below its request and another
  device is admitted. Two new values of `DisplayBudgetReason`, which today has only `none` and
  `coreBusy` (`DisplayBudget.h:82-85` `@aa6c5505`):
  - `sharedConnection`: the devices share what the Core sends, and the governor has cut
    nothing. The total in force is the display allowance configured for the Core
    (`DaemonConfig::displayBudgetLimits`, `DaemonConfig.cpp:338-345` `@aa6c5505`) or the
    Core's computed ceiling (`DisplayLoadGovernor::computedCeiling`,
    `DisplayLoadGovernor.h:148` `@aa6c5505`). The drawn screen's note reads "The Core's
    connection is full. The MacBook has transmit, so it keeps its full band and sound." (the
    iPhone design, section 5.8 item 7, picture 22). Its second sentence holds only while a
    network device holds transmit; the phone session writes the note for each reason and
    each holder case (a device, nobody, "Radio") (re-review finding 4).
  - `sharedProcessing`: the governor has cut the total because the Core computer is short of
    processing time (the reason `coreBusy` would be in force alone). It has no drawn words
    yet; the phone session writes them.
  They take the place of `coreBusy` while another device is admitted and the device's share
  is below its request; alone on the Core a device still sees `coreBusy` or `none`, and so
  does a device beside others whose share covers all it asks for. Only devices with the new
  feature receive the new values. Under demand-based requests (fix wave 2, Important 3) the
  request is the device's demand, at least one useful pan; since every client asks for what the
  operator wants (above), a device that wants more than its share beside another device hears
  `sharedConnection` (or `sharedProcessing` under the governor's cut), as this ruling meant from
  the start. The first fix wave's clients planned inside their share, so their demand never
  exceeded it and they heard `none` or `coreBusy` beside another device; that is what this
  round undoes.
- A change of holder or of devices publishes new generations.

**Design ruling 9.3a.** Finding 5 asks the reason to say whether "the uplink or the
processing is short". The Core measures processing only: the governor steps on receiver load
and host CPU (`DisplayLoadGovernor.h:67-79` `@aa6c5505`), and nothing in the Core measures its
uplink. So `sharedConnection` means the display allowance the Core sends within is shared, set
by configuration for its connection or by its own ceiling, not a measured full link; and
`sharedProcessing` means the governor's cut is in force. Those are the two statements the Core
can make truly, and each matches one of the phone's two sentences. A measured uplink would be
a third value, raised with the link's version, if the bench (9.5) shows it is needed.

### 9.4 Audio

**Ruling 9.4.** Audio is not split. Opus costs 427 us per 40 ms frame, 1.07% of one Pi 4 core
per stream, measured mono at 24 kbit/s (the remote design, lines 570-572). The most a Core can
carry is four devices, each with its main mix, its headphones mix and four receiver streams
(`IMediaTransport.h:91`): 24 streams, about 26% of one core by that measurement. Stereo and the
48 kbit/s receiver streams cost more, and the bench measures it (9.5). That is still small next
to display and demodulation, and cutting audio would cut what an operator hears, so "the holder
keeps its full audio" holds for everyone. Sound is never cut when the Core runs short: the
other devices' bands slow first, then pause with sound kept, until there is room (D64, which
confirms this reading of D54).

### 9.5 CPU on a Rock or Pi: measured and guessed

**Measured:**

- FFT per stream on a Pi 4: 59 us per frame at 4,096 points to 11.8 ms at 262,144, and four
  threads give only 1.35 times one thread's throughput (the remote design, lines 553-560,
  589-596).
- Opus, as in 9.4.
- On the Rock 5C the premium NNR model held a Cortex-A76 at 100%
  (`2026-09-23-r3-dsp-overload-plan.md`, lines 38-40).
- Live: the Core measures each receiver's load and its host's CPU (telemetry versions 2 and 3,
  the link, section 10), which drive the governor.

**Bounded by the code, not measured:** demodulation. Slices across every device stay within the
slice cap (6.5), so WDSP's load does not grow with the number of devices.

**Guessed (bench pending):**

- Reducing and encoding display frames for up to four devices' endpoints on shared FFTs: the
  budget design gives format bounds, "not measured sustainable operating rates" (the budget
  design, line 77).
- Up to 24 audio streams in stereo, some at 48 kbit/s (9.4).
- Four media peers' encryption and packet handling.
- One mix per owner: a few multiply-adds per sample per slice.
- Uplink: about four times one device's traffic, 0.6 to 2.1 Mbit/s from the pairing design's
  145 to 520 kbit/s per session (the pairing design, lines 254-255).

---

## 10. The link

### 10.1 Feature and capability

**Ruling 10.1.**

- A hello feature, `sessionHolder` 1. The name stays the one Task 41 gave it (the plan, line
  3741) and the phone's plan already declares (the plan at `06209b82`, lines 982-985, and its
  Task 56). Nothing was built under its first meaning, one device holding the Core, so it takes
  this design's meaning with no clash: a client that shares a Core with other devices. The
  Core declares it, so a client knows before signing in that the Core admits up to four and
  may ask the fifth-device question. A client that implements this design declares it, so the
  Core knows before admission that it can answer `session.held`, draw markers and handle
  confirmations and notices.
- A client declares it only together with `deviceAuth` 1 or later, and the Core treats
  `sessionHolder` without `deviceAuth` as not declared. Only paired devices therefore see who
  is on the Core, as the iPhone design requires (section 4.5 item 1), and every view with the
  feature also receives Task 13's `devices` object for the Devices page's paired list.
- It cannot ride on `deviceAuth` alone, as Task 13's `devices` object does (the operator's
  ruling of 2026-09-24, in the plan as amended by `c35048dd`): a window that signs in by key
  but predates this design must still be treated as an older window (10.7).
- A capability, `sessionHolderVersion` 1, sent at agreed minor 11 to a peer that declared the
  feature, 0 otherwise. `kSessionProtocolMinor` stays 11
  (`src/core/session/SessionMessages.h:197`).
- Version 1 brings all of it: sessions, admission and the fifth device; ownership, views and
  markers; confirmations and notices; taking a receiver or a slice; the holder on `txState`
  and `tx.take` (which also needs `remoteTxVersion` 1). A later change to any part raises it.
- **Two gates** (review finding 11). `session.held` arrives in place of `capabilities`
  (4.2), so no capability can gate it or its answer: `session.held` and `session.takeover`
  are gated by the hello feature alone, `sessionHolder` 1 in both ends' `hello` (the Core's
  and the client's), with `deviceAuth` 1, at agreed minor 11. Everything else in this design
  follows the link's two-key gate (the link, section 6.2): a client uses it only at agreed
  minor 11 with `sessionHolderVersion` at least 1, which a client that met `session.held`
  learns only once it is admitted and receives `capabilities`.

### 10.2 Messages

New kinds, sent only to peers that declared `sessionHolder`; the Core refuses a new kind from
a peer that did not, as it refuses any undecodable message today.

| Kind | Direction | Keys |
| --- | --- | --- |
| `session.held` | Core to client, instead of `capabilities` | `devices` (array, 4.3), `revision` (number); optional `placeTaken` `{byName, byId, secondsAgo}` or `placeFreed` `{secondsAgo}` (4.3) |
| `session.takeover` | client to Core | `deviceId` (string, empty to cancel), `revision` (number) |
| `confirm.request` | Core to client | `id`, `kind`, `reason`, `affected`, `expiresInMs`; optional `change`, `choices`, `holder`, `forCommandId`, `forWriteId`, `forSettingsKey` (7.3) |
| `notice` | Core to client | `id`, `kind`, `reason`, `secondsAgo`, `takeBack`; optional `byDeviceId`, `byName`, `byShortName`, `byKind`, `bySource`, `slices`, `change` (7.4) |

`session.held` and `session.takeover` are gated as ruling 10.1 says; `confirm.request` and
`notice` by `sessionHolderVersion` 1.

`session.end` gains three optional keys: `takenOverBy` (the taker's name, as Task 41 planned),
`takenOverById` (review finding 7) and `secondsAgo`, which replaces Task 41's planned `at`
(ruling 10.3). A reason never carries a time: an app shows the time from `secondsAgo`.

`auth.request`'s `device` block gains an optional string, `shortName` (ruling 4.3). It lands
with the identity and pairing work (Part C), so every Core that reads a `device` block reads
it, and a client sends it at every sign-in with no gate. Pairing's `device` block and the
code-mode confirmation box are unchanged.

**Ruling 10.3. One clock convention** (review finding 7). Every time the Core sends about a
session, a device, transmit, a notice or an end is a duration the Core measures on its own
monotonic clock at the moment it encodes the message, in whole seconds, named `...ForSeconds`,
`...Seconds` or `secondsAgo`: `connectedForSeconds`, `lastActivitySeconds`, `awayForSeconds`,
`transmittingForSeconds`, `txState`'s `holderForSeconds` and `keyedForSeconds`, and the
`secondsAgo` of `notice`, `session.end`, `placeTaken` and `placeFreed`. The app counts on from
its own receipt of the message, and turns a `secondsAgo` into a time of day by its own clock
and time zone. No wall-clock time from the Core reaches these screens, so a Core with no
real-time clock (a Pi 4 before its clock syncs) still shows "connected 2 hours" and a TX clock
correctly. A duration inside an object is re-measured whenever the Core sends that object or
property (its `object.create` at attach, a `delta` on any change), and not otherwise: the app
keeps counting between sends. Task 13's `devices` keeps its ISO 8601 `pairedAt` and `lastSeen`
(the link, lines 1332-1337 `@aa6c5505`): those are dates that outlive a session and a Core
restart, not durations a screen counts.

### 10.3 Objects

- `marker:<id>`, class `SliceMarker` (5.4): for every slice another device owns, its owner,
  letter, frequency, receiver and TX.
- `txState` (Task 39) gains the holder properties (8.1): who has transmit and whether they are
  on the air, for every session.
- **`connectedDevices`**, class `ConnectedDevicesFacade`, new: who is on the Core, the list
  the Devices page reads for "Connected now" (settled detail S3; the iPhone design, D57). Sent
  only at agreed minor 11 to a view with `sessionHolderVersion` 1, so older peers see exactly
  today's wire (the link, section 17). Every property is `outbound`:
  - `listJson` (`utf8`): a JSON array with one entry per device that has a session, live or
    away, in the order they were admitted: `{deviceId, name, shortName, kind, paired,
    hostsCore, revocable, state, holdsTransmit, lastActivitySeconds, connectedForSeconds,
    awayForSeconds, transmittingForSeconds, listeningOn, transmittingOn}`. It is the same
    entry as `session.held`'s (4.3), less `from` and `replaceable`, plus `paired`,
    `hostsCore` and `revocable`, so a device is described one way on every screen.
    - `deviceId` is Task 13's `id` for a paired device, so an app joins the two lists;
      `token:<n>` for a window signed in with the older token (`paired` false).
    - `name` and `shortName` are numbered as ruling 4.3 says, the same as in `devices`, so
      the Devices page's two lists name one device the same way (review finding 13).
    - `hostsCore` is true for a hosting desktop's own window. `revocable` is false for it and
      for a token window, whose ids Task 13's revoke refuses; an app also hides Revoke on its
      own entry.
    - `state` and `holdsTransmit` as in `session.held` (4.3).
    - The four durations follow ruling 10.3, measured when the list is sent; `0` when they
      do not apply (`awayForSeconds` for a device that is not away, `transmittingForSeconds`
      off the air). The list is re-sent only when something in it changes, and the app
      counts on between sends. `lastActivitySeconds` is re-measured at most once a minute
      per device, so a device that keeps tuning does not resend the list to every other;
      `session.held` carries it exact.
    - `listeningOn`: `[{sliceId, letter, band, mode}]`, every slice the device owns, an away
      device's included. A slice's live frequency is on its `marker:<id>`, so tuning does not
      change the list; `session.held`, sent once, carries `frequencyHz` too.
      `transmittingOn` is its transmit slice in the same shape while it transmits, else
      absent. Slices held for a device that has left show only on their markers
      (`ownerAway`, 5.4).
  - `revision` (`i64`), as Task 13's `devices.revision`: moves by one with every change,
    compared by serial-number arithmetic.
  - `deviceLimit` (`i64`): 4.
- `devices` (Task 13) keeps its keys: the paired list, each with `connected`. Its `name` is
  numbered by ruling 4.3 from now on, and each entry gains `shortName`.

In the JSON of this section and section 4.3, `band` is the `Band` value the slice's `band`
property carries (the link, line 891) and `mode` the `dspMode` value, so an app names both as
it does for its own slices; `frequencyHz` is in hertz.

A view with the feature receives its own `slice:` objects, a `marker:` for every other slice,
`connectedDevices`, and every Core object its minor and capabilities allow. An older view
receives its own `slice:` objects and the Core objects.

### 10.4 Verbs

All under `sessionHolderVersion` 1, minimum minor 11:

| Verb | Arguments | Notes |
| --- | --- | --- |
| `confirm.proceed` | `id` i64, `choice` i64 | `choice` -1 when the kind has none |
| `confirm.cancel` | `id` i64 | |
| `notice.takeBack` | `id` i64 | 7.4 |
| `tx.take` | optional `holderEpoch` i64, `shownKeyed` bool | ruling 8.7; also needs `remoteTxVersion` 1 |
| `session.leave` | none | 4.6 |

Changed: the slice verbs refuse another device's slice (5.6); `requestStreamCtunPinned` and
`requestStreamCentre` follow the anchor rules (6.2, 6.3); `tx.setTxSlice` and `tx.unkey` are
the holder's (8.3, 8.7); Task 42's accessory verbs follow 7.5.

### 10.5 Changes to the link's sections

- **Section 5.1.** A full Core sends `session.held` after `auth.result` in place of
  `capabilities`, to a peer that declared the feature.
- **Section 3.5.** `auth.request`'s `device` block gains the optional `shortName` (ruling
  4.3), sent at every sign-in; it lands with Part C. Section 3.6 (pairing) is unchanged.
- **Section 6.2.** `session.held` and `session.takeover` are gated by the hello feature alone
  (ruling 10.1).
- **Section 6.4.** `capabilities` gains `sessionHolderVersion`; `displayBudgetReason` gains
  `sharedConnection` and `sharedProcessing` (9.3).
- **Section 7.1.** The `SliceMarker` and `ConnectedDevicesFacade` classes, the `marker:<id>`
  and `connectedDevices` keys, the holder properties of `txState` (and `keyedForSeconds` in
  place of Task 39's planned `keyedSinceMs`), `txSlice`'s meaning on a slice (ruling 5.4a),
  and `devices`' numbered names and `shortName` (ruling 4.3). The one clock convention
  (ruling 10.3) is stated once, here.
- **Section 7.3.** The refusal for another device's object, and the "Waiting for you to
  confirm." answer.
- **Section 8.1.** A disturbing `settings.write` gets `settings.reject` and a
  `confirm.request`.
- **Section 11.** "The station accepts media control only from the current session" (the link,
  line 1506) becomes: from each admitted session, for its own media.
- **Section 12.3.** `kMaxConcurrentPeers` 24, a cap on sockets of every kind; a new
  `kMaxDeviceSessions` 4.
- **Section 12.4.** The preemption row and the "Only one session is authenticated at a time"
  paragraph go (the link, lines 1606 and 1630-1633). New rows:

  | Cause | Message | `retryable` | `code` |
  | --- | --- | --- | --- |
  | Replaced by a fifth device | `session.end` naming the taker, with `takenOverBy`, `takenOverById` and `secondsAgo` | false | `takenOver` |
  | The fifth device cancelled or did not answer | `session.end` "The Core already has four devices connected." | false | `coreFull` (new) |
  | An older window meets a full Core | `session.end` "The Core is full. Update NereusSDR to take a device's place, or try again later." (15.2, the operator's wording) | true | none |
  | The same device connected again | `session.end` "This device connected again." | false | `sameDevice` (new) |
  | An older window's last slice was taken | `session.end` naming the taker (ruling 6.10) | false | `takenOver` |

  `SessionEndCode` (`SessionMessages.h:272-297` `@aa6c5505`) gains `coreFull` and
  `sameDevice`. The Heartbeat timeout row stays `retryable` true: that end is what starts a
  device's 3 minutes (ruling 4.10).
- **Sections 14.1 and 14.2.** The connected-device count (ruling 10.4).
- **Section 15.** `maxPeers` 24; new limits `maxDeviceSessions` 4, `graceMs` 180000,
  `takeoverAnswerMs` 60000, `confirmExpiryMs` 60000, `unkeyConfirmMs` 2000 (from Task 34),
  `shortNameMaxBytes` 32; `kStationLanMaxSchema2DatagramBytes` 480.
- **Section 17.** The wording rule's list gains the `reason` of `confirm.request` and
  `notice` and the three strings of `change`, held to `OperatorWording::isPlain` and tested
  with device names such as "Grant's iPhone" embedded. Device names and short names are the
  operator's words, validated as ruling 4.3 says, and are not held to it.

**Ruling 10.4. The count before connecting** (review finding 7). The iPhone design's screen
14 lists a Core with "4 devices on it" before the phone connects, so the count travels in
discovery, appended under the link's own rules:

- **The LAN announcement** gains one byte after Pairing, "Devices connected": 0 to 4, the
  places taken as ruling 4.4 counts them (admitted sessions, away devices and a hosting
  desktop's window). A schema-2 datagram grows to at most 480 bytes
  (`kStationLanMaxSchema2DatagramBytes`, 479 today, `StationLanAnnouncement.h:41`
  `@aa6c5505`), still under a listener's 512, and stays schema 2, as the append rule allows (the link, lines 1988-1994 `@aa6c5505`). A
  reader that never sees the byte (a datagram from an older Core) assumes the count is not
  known and shows none. A new vector, `media/lan-announcement-2-devices.bin`, is the
  `lan-announcement-2` datagram with the byte; `lan-announcement-2-trailing` keeps proving
  that bytes beyond it are ignored.
- **The Bonjour TXT record** gains a sixth entry after `name`: `devices`, `0` to `4`. `v`
  stays `1`, since a client ignores a key it does not know (the link, lines 2031-2042
  `@aa6c5505`); the station updates the record in place when the count changes, as it does
  for any fact. `media/dnssd-txt.bin` is written again with the sixth entry.
- The count is a number only. Who is on the Core still reaches paired, signed-in devices
  alone (ruling 10.1; the iPhone design, section 4.5 item 1): the LAN learns how full the Core
  is, never who is on it. A Core that is not claimed has no devices and sends 0. A Core reached
  through the rendezvous or the relay has no announcement, so its count shows only after
  sign-in (from `connectedDevices`).

### 10.6 Conformance

- The `preempted` fixture (the link, line 2061) is withdrawn with the `preemptingClient` setup
  key (`tests/LinkFixtures.cpp:982-991`; `tests/LinkFixtures.h:150-152`).
- `connection-limit` (the link, line 2054) opens twenty-four other connections.
- **A runner that plays several clients.** `stationSetup.otherClients`:
  `[{"name": "b", "device": 1, "features": {...}}]`, each signing in as paired device n (Task
  13's `otherPairedDevices` and its `$ref:device:<n>`). Client steps may carry `"client"`,
  `"from": "station"` steps `"to"`; absent means the fixture's own client. New steps
  `{"connect": "<name>"}` (that client's whole connect sequence, its messages up to
  `snapshot.complete` taken without matching) and `{"close": "<name>"}`; `expectClosed` may
  name a client. The Core's messages are matched per client, in that client's own arrival
  order. `otherConnections` (`tests/tst_link_conformance_session.cpp:280-290`) stays for sockets
  that never sign in. As the plan requires, the format change goes to the iPhone session first.
- **An app's runner** plays only its own client and skips other clients' steps and messages
  sent to them. A fixture whose own-client behaviour an app can adopt runs on both ends.
- New fixtures: see section 14.2.

### 10.7 What an older window sees (D59)

The operator let older windows in (D59: "Let it in").

- While a place is free it is admitted with its own slices and the Core's objects; no markers,
  `connectedDevices`, confirmations or notices.
- A refusal that involves another device names it and says to update NereusSDR.
- A full Core refuses it with a retryable end ("The Core is full. Update NereusSDR to take a device's place, or try again later.", the wording
  the operator confirmed in 15.2), since it cannot answer the question; it tries again on its
  own backoff. A device with the feature keeps the fifth-device flow (4.3).
- When a take would close its last slice, its session ends (ruling 6.10).
- **Ruling 10.2.** It gets a slice at admission (5.2) when the slice cap allows. When it does
  not, it is refused, retryable, in words that name the limit that is full: "All the radio's
  slices are in use. Try again when another device closes one." when the slice cap is full,
  and "All the radio's receivers are in use. Try again when another device frees one." when
  no receiver is free for its slice (re-review finding 9).
- Being refused while the Core is full or has no slice for it, with no question to answer, is
  a narrowing of D55 and D59 for older windows (15.2).

### 10.8 What the phone reads, item by item

The iPhone design lists what the phone needs from this design (section 4.5, at `94ec2f7e`).
Each item's wire answer:

| The phone needs | The Core sends |
| --- | --- |
| **Who is on the Core** (item 1; D55, D57) | `connectedDevices.listJson` (10.3): per device `name`, `shortName`, `kind`, `connectedForSeconds`, `lastActivitySeconds`, `state`, `listeningOn` (letter, band, mode), `holdsTransmit` and `transmittingForSeconds` (its TX clock); only to paired devices (10.1). Before connecting, on the LAN, only the count: the announcement's and the TXT record's `devices` (ruling 10.4). |
| **Whose each slice is** (item 2; D45 to D47) | Its own: its `slice:<id>` objects, letter 'A' + `sliceIndex`. Every other: `marker:<id>` with `ownerDeviceId`, `ownerName`, `ownerShortName`, `ownerKind`, `ownerAway`, letter 'A' + `sliceId`, colour from the letter (5.4). One letter pool (ruling 5.5); a write to another's slice is refused (ruling 5.9). |
| **Who has transmit, and whether on the air** (item 3; D51, D52, D63) | `txState`, to every session: `holderName` and `holderShortName` ("Radio" after the radio's own PTT), `holderSource` (`device` or `radioPtt`), `holderKind`, `holderDeviceId`, `holderAway`, `holderTransferring`, `holderForSeconds`, `holderEpoch`, with Task 39's `keyed` and `keyedForSeconds` (ruling 8.1). TX on a slice: `txSlice` on its `slice:` or `marker:`, true only while its owner holds transmit (ruling 5.4a); a slice frozen by the radio's PTT is read from `txState` (`keyed`, `holderSource` `radioPtt`, `txSliceId`). Taking it: a key on unheld transmit takes and keys (D63); `tx.take {holderEpoch, shownKeyed}` after the phone's own sheet, or `tx.take {}` answered by `confirm.request` `takeTransmit` (ruling 8.7). |
| **Which slices each receiver carries** (item 4; D48, D49) | `streamIndex` on every own `slice:` and every `marker:` (5.4), shown as "Receiver `streamIndex` + 1". When a receiver must be taken, `confirm.request` `takeReceiver`: per receiver `streamIndex`, `adc`, its slices (letter, owner, frequency, mode, band, TX) and its devices (`shortName`, `state`, `lastActivitySeconds`), `takeable` and `why` (6.4). |
| **What a shared change or a pan move would reach, before confirming** (items 4 and 5; D50, D53) | `confirm.request` `sharedSetting` or `panMove` (7.3), a band change of a shared receiver included (ruling 6.5): `change {label, from, to}`, and `affected` with each device's `deviceShortName` and `state`, and each of its slices' `mode`, `adc`, `streamIndex` and `effect` (`changes`, `moves`, `closes`, `pausesWhileTransmitting`). After Confirm, `confirm.proceed`'s result carries the readback (ruling 7.4a). While the holder is on the air, such a change that reaches its transmit path or transmit slice is refused with the on-air reason instead (D60, ruling 7.4). |
| **Notices with who, what and when** (item 6) | `notice` (7.4): `kind`, `byName`, `byShortName`, `byKind`, `bySource`, `secondsAgo`, `slices`, `change`, `takeBack`. A place: `session.end` with `takenOverBy`, `takenOverById` and `secondsAgo`, or, for a device replaced while away, `placeTaken` (4.3). |
| **The frame rate a slowed device gets** (item 7; D54) | The phone computes it: its own planner fits its displays into the share `capabilities` carries, and the chip shows the active pan's rate from that plan. `displayBudgetReason` says why its share is cut: `sharedConnection` or `sharedProcessing` (9.3, design ruling 9.3a). |
| **The fifth-device list** (item 8; D55, D64) | `session.held {devices, revision, placeTaken, placeFreed}`, idle longest first, the choice starting on the first `replaceable` entry, each device's state, slices and TX clock (4.3, ruling 4.6); the answer `session.takeover {deviceId, revision}`; ends `takenOver` and `coreFull` (10.5). |
| **The reclaim rules** (item 8; D56, D62) | Within 3 minutes the same device signs in again: admitted with no question, its older connection ended with `sameDevice` (ruling 4.8); its slices arrive as its own `slice:` objects; `txState.holderDeviceId` is its own id, unkeyed, when nobody took transmit, and otherwise its waiting `transmitTaken` notice arrives after `snapshot.complete` (7.4, ruling 8.15). After 3 minutes: a `graceEnded` notice on admission, or `placeFreed` in `session.held` when the Core is full (4.5). |

---

### 10.9 The in-between states the operator approved (board v50)

The operator approved five in-between states the phone session drew (2026-09-24, "Keep all
as drawn"). Each is carried by fields this design already sends; none needs a new one.

| State | Carried by |
| --- | --- |
| A holder that is away (the take asks without red) | `txState.holderAway` true, holder still named (8.1); in the `takeTransmit` request, the holder entry's `state` `away` and `awayForSeconds` (ruling 8.7, 8.4 "A take during grace"); `keyed` false, since an away holder is never keyed (ruling 8.15), which is what keeps the button from turning red |
| Transmit on its way (PTT waits with a turning ring) | `txState.holderTransferring` true on every device during a transfer (ruling 8.2); for the device taking, its own `tx.take` or `confirm.proceed` result, which arrives when the transfer ends (8.4), and, for a key on unheld transmit, its `tx.key` result |
| A slice held by the radio's mic (ON AIR, tuning waits) | the frozen slice (ruling 8.11, 5.4a): `txState.keyed` true, `holderSource` `radioPtt` and `txSliceId` naming the slice; its `txSlice` false on the wire; a tuning write refused with the on-air reason until the press ends |
| Away on Devices (amber dot, "away for 1 minute") | `connectedDevices` entry `state` `away` with `awayForSeconds` (10.3, ruling 10.3); on its slices' markers, `ownerAway` true (5.4) |
| Away first in a fifth device's list | `session.held` entry `state` `away` and `awayForSeconds`; ruling 4.6 orders away devices first |

## 11. Other documents this amends

- **The remote design, section 7.1** (lines 905-951) and its summary row (line 2049): "Single
  operator, one session at a time" becomes up to four devices; the control operator is the
  transmit holder; preemption gives way to admission and the same-device rule; every paired
  device keeps the role of owner.
- **The pairing design:** its decisions row "Concurrent sessions: One ... A new connection
  preempts" (line 65) becomes "Up to four; a fifth asks to take one's place"; section 7's
  "Multiple devices may be paired; one may be connected" (lines 359-361) becomes "up to four may
  be connected"; its non-goal (lines 41-44) keeps guest sessions out and drops "multi-operator
  access".
- **`StationServer.h`:** the topology note (lines 40-60), the `Peer` comment (548-552),
  `hasAuthenticatedSession` (429-430), `sessionPreempted` (537-541) and the
  `kMaxConcurrentPeers` comment (254-260).
- **The iPhone design:** the iPhone session amended it at `06209b82` (section 3.9 with D44 to
  D57, D21 replaced, D22 and D35 carried forward, section 4.5 rewritten as what the phone
  needs from this design, section 5.8, R-IOS-02, R-IOS-03, R-IOS-17, R-IOS-30 and R-IOS-31)
  and wrote the operator's answers at `94ec2f7e` (D58 to D64, D52 amended, D56 bounded by
  D62, section 4.5 item 8 "within 3 minutes"); section 10.8 answers its section 4.5 item by
  item. Nothing is left for it here.
- **The link:** the changes in section 10.5.
- **The Phase 3F design** says nothing about several clients; it gains a pointer here.

---

## 12. The desktop's own screens (Core/GUI work)

Each is named here; the wording follows the phone's screens.

1. **Other devices' markers on the panadapter** (D46). `SpectrumWidget::drawVfoMarker`
   (`src/gui/SpectrumWidget.cpp:6620-6625`) draws each own slice through `drawSliceMarker`
   (`SpectrumWidget.cpp:6633`). It gains a second kind of marker, drawn as the phone draws
   it: a dashed centre line in the slice's colour with a hollow triangle, dashed grey passband
   edges with no fill, and a label at the foot of the spectrum with the slice letter and the
   device's short name, plus TX by ruling 5.4a (the slice is the transmit slice and that
   device holds transmit); no flag, not draggable. A click on
   the label says whose slice it is and that only that device can tune it or close it. In a
   remote window it comes from `SliceMarker` objects; on a hosting desktop, from
   `SliceOwnership`. Today's palette gives the fifth letter, E, the same cyan as A
   (`VfoWidget.cpp:3403-3409`); the label's letter tells them apart, and a fifth colour is
   the operator's call.
2. **The transmit holder.** The bottom banner's TX badge (`ChromeBarController`,
   `src/gui/chrome/ChromeBarController.h:45`) names the holder by its short name when it is
   another device, or "Radio" when `holderSource` is `radioPtt`, in red while it is on the
   air, marked away while its holder is away, and shows "changing hands" during a transfer.
   The pan's TX pill (`SpectrumStatusOverlay::txBadgeClicked`,
   `src/gui/widgets/SpectrumStatusOverlay.h:139`) offers Take transmit. The window's own VFO
   flags show TX only while the window's device holds transmit (ruling 5.4a), and a slice the
   radio's PTT has frozen shows as in use by the radio (ruling 8.11). A refused program key
   (ruling 8.3) shows on the window in plain words ("WSJT-X can transmit only while this
   window has transmit.").
3. **Notices with Take it back.** A card on the band for each notice. Ends of the whole session
   stay in the remote window's stop panel (`src/gui/RemoteConnectionController.cpp:165-210`),
   whose Take it back now reconnects into the fifth-device list rather than preempting
   (`RemoteConnectionController.cpp:205-210`) and preselects `takenOverById`. The panel shows
   the end's time from `secondsAgo`. The `graceEnded`, `slicesNotRestored` and `antennaKept`
   notices use the same card, with nothing to answer.
4. **The take-a-receiver chooser.** A dialog listing the receivers (or slices, section 6.4),
   each with its devices, slices and frequencies; one pick.
5. **Confirmations for shared settings.** One dialog shape for every `confirm.request`, naming
   each device and what happens to its slices.
6. **Taking transmit.** "Take transmit from iPhone?", or the red "Unkey and take over". On a
   hosting desktop its own MOX and TUNE ask this way too (ruling 8.9a).
7. **The fifth-device choice.** A dialog at connect in a remote window listing the four, idle
   longest first, the choice starting on the first that can be replaced: name, how long
   connected, when last active, and what each is doing (listening on which slices,
   transmitting with its TX clock, or away); the hosting desktop shown but not selectable;
   picking the one on the air turns the button red. With `placeFreed`, it says the window's
   own place was freed after 3 minutes away.
8. **Who is connected.** On a hosting desktop's Remote Access page (Task 49) and on a remote
   window's This Core page (Task 25): the devices connected now, each with its slice letters
   and bands and TX on the holder, then the paired ones; from the session registry on a
   hosting desktop, from `connectedDevices` and `devices` in a remote window.

Two changes underneath: `RxApplet`'s slice buttons (`src/gui/applets/RxApplet.h:203`) and the
VFO flags show only the window's own slices; and a remote window with the new feature accepts
the Core destroying its last slice (today refused, `RadioModel.cpp:7562-7567`), showing an empty
band that offers to take a receiver.

---

## 13. Plan impact

One plan, per the operator's rule. New station tasks take the next numbers, since lettered
tasks in the plan are phone tasks (the plan, lines 47-51). The iPhone session sets the plan's
overall order; the operator wants the phone's first build to listen on Wi-Fi, before transmit
and remote access. This section says only how these tasks depend on each other and on
earlier ones.

### 13.1 Station tasks that change

| Task | Change |
| --- | --- |
| 13 | Landed as `c35048dd`. Its revoke gains the multi-device consequences in Tasks 73 (slices, held slices, saved layout) and 34 (transmit released through the transfer); its `devices` list gains numbered names and `shortName` (ruling 4.3), built in Task 71. |
| 14 | `auth.request`'s `device` block carries the optional `shortName` (ruling 4.3), built with Part C before it reaches integration; pairing is unchanged. |
| 16 | The LAN announcement's appended count byte and the TXT record's `devices` entry (ruling 10.4), built in Task 71. |
| 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 47, 50 | None. A pairing connection counts only as a socket (4.2); record streams are already per peer (the plan, line 2239). |
| 23 | Its per-session bitrate is per device; after Task 76. |
| 24 | Its acceptance per device: one device on sound only while another has displays; after Task 76. |
| 25 | `station.selectRadio` goes through the confirm step, since it touches every slice; the `vax` object is the station device's; after Task 75. |
| 26 | Check only: coturn's quotas "per session and in total" (the plan, line 2568) sized for four sessions per Core. |
| 29 | The path race signs in on one path only (ruling 4.9). Its phone half, 29a, is in 13.4. |
| 34 | Rewritten: `TransmitHolder` with its three states and the transfer (rulings 8.1, 8.2); D60's on-air refusals built with it (ruling 7.4: the transmit-path changes and the Protocol 1 rate change, which need only `keyed` and the list, and the transmit-slice refusal for every move and take that exists when it lands), so transmit never exists without them (re-review finding 5); a dropped holder unkeyed through the fence and holding transmit, unkeyed, for its 180 s; release through the transfer when they end, on leaving, on replacement and on revocation (ruling 8.15); VOX disarmed on every change (ruling 8.4); `StationTxGate` also requires holding or unheld; `otherDeviceKeyed` becomes `otherDeviceHolds`, and `programNeedsTransmit` is new (ruling 8.3); `tx.setTxSlice` and `tx.unkey` are the holder's; the keying gate on edges (rulings 8.8, 8.13). It gives `DisplayBudgetSplit` (Task 76) its holder. After the gaps plan's Task 7. |
| 35 | Rewritten: keying per holder; a person's key on unheld transmit takes it and keys (D63); a program's key (`trigger` `tci`, and CAT's once built) keys only while its device holds transmit and never takes it (D58, ruling 8.3); `keyedBy` names the holder; releases only the keyer's own key; the Core's own TCI keying (the plan, lines 3399-3402) is withdrawn (D58); a remote window's TCI follows ruling 8.14; "A second device cannot key while another device holds the session" (lines 3413-3414) becomes the `otherDeviceHolds` refusal. |
| 36 | The mic source and VOX follow the holder (ruling 8.4). |
| 37 | A dropped holder is unkeyed at once through the transfer's fence and keeps transmit, unkeyed, for its 180 s (ruling 8.15). |
| 38 | Unchanged in substance: the time-out follows the holder's device kind. |
| 39 | `txState` gains `holderDeviceId`, `holderName`, `holderShortName`, `holderKind`, `holderSource`, `holderForSeconds`, `holderEpoch`, `holderAway` and `holderTransferring`, sent to every session, with "Radio" and `radioPtt` after the radio's own PTT (ruling 8.1); `keyedSinceMs` becomes `keyedForSeconds` (ruling 10.3); `takenOver` covers transmit taken. |
| 40 | The `transmit` object's controls are the holder's while held (ruling 7.7). |
| 41 | Rewritten as the fifth device: `session.held` with the list, idle longest first, the choice starting on the first replaceable entry, each device's `shortName`, activity, slices and TX clock (4.3, ruling 4.6); `session.takeover` naming a device, a non-replaceable one refused; a holder replaced through the transfer; the replaced device told with `takenOverBy`, `takenOverById` and `secondsAgo`, or, when it was away, `placeTaken` at its next sign-in; `placeFreed` for a device whose 180 s ran out; older windows refused when full; gated by the hello feature (ruling 10.1). After Tasks 71, 73 and 34. |
| 42 | Amplifier, tuner and RF-Kit verbs follow ruling 7.8 (any device, the holder named) and wait while the holder is on the air (D60, ruling 7.4, from Task 34); `tuner.tune` is the holder's, and on unheld transmit takes it first (rulings 7.7, 8.3). |
| 43-46 | Any Setup write may answer "Waiting for you to confirm."; each live-apply Core control's description names its scope. |
| 48 | Rewritten: `StationHost` runs the session registry; the desktop window is the station device and one of the four; its flags show only its own slices, with TX by ruling 5.4a; its local audio plays its own mix; its MOX and TUNE ask like any device's (D64, ruling 8.9a); its TCI and CAT programs key only while the window holds transmit (D58; rulings 5.13, 8.14). The markers and other screens come in Task 78. |
| 49 | Adds who is connected now (short name and name, how long, when last active, slice letters and bands, TX on the holder, away), with no new action; Revoke already drops a device. |
| 70 | Gains the multi-device bench rows (14.3). |

### 13.2 New station tasks

| Task | What it builds | Depends on |
| --- | --- | --- |
| 71. Device sessions and admission | Only what needs no ownership: `DeviceSessionRegistry` (token ids and names, names and short names numbered by ruling 4.3, in `devices` too); the stored `shortName` in every name the Core sends (the key itself lands with Part C); the four and the socket cap; the same-device rule; away presence and its 180 s timer, and the record that a device's time ran out (ruling 4.11); `session.leave` ending a session without the away state; the `sessionHolder` feature, its hello-feature gate for `session.held` and `session.takeover`, and `sessionHolderVersion`; the `connectedDevices` object with its presence keys and the one clock convention (ruling 10.3; its `listeningOn` comes with Task 73, `holdsTransmit` with Task 34); the count in the announcement and the TXT record (ruling 10.4); a fifth device refused, retryable, until Task 41; link sections 3.5, 5.1, 12.3, 12.4, 14 and 15; the several-client runner; `preempted` withdrawn; the section 11 amendments to the remote design, the pairing design and `StationServer.h` | 12, 13, 14, 16 |
| 72. A mirror view per device | `MirrorView`; echo per writer; results, notices and settings routed per session; the dispatcher's owner per session; capabilities per device | 71 |
| 73. Slice ownership, markers and the active slice | `SliceOwnership` with held-for marks; `SliceMarker` (with `ownerShortName`); refusals for others' objects; the active slice per device and the station-level one; held slices, restore, adoption of unowned slices and the first slice (5.2); `DeviceLayoutStore`, with each closed slice's settings, and owners in the manifest (5.3); what the end of the 180 s, `session.leave` and revocation do to slices; TCI and VAX mapping; link sections 7.1 and 7.3 (objects, refusals) | 72 |
| 74. Receivers: anchors, moving a pan, taking a receiver | anchors and the pin; the anchor's move, its band change on a shared receiver as `panMove` (ruling 6.5, design ruling 6.5a), and the non-anchor's move to its own receiver; the take chooser (receiver or slice) and Take it back asking the other way; the older window's slice at admission and its last slice (rulings 10.2, 6.10); the transmit-slice on-air refusal (ruling 7.4) on its pan moves, band changes and takes, reading `TransmitHolder` (whichever of 34 and 74 lands second wires it); the confirm step with the proceed's readback (ruling 7.4a) and notices (`confirm.request`, `confirm.proceed`, `confirm.cancel`, `notice`, `notice.takeBack`, and the `graceEnded` and `slicesNotRestored` notices); link sections 7.3, 8.1 and 9 for the confirm answers and verbs | 73 |
| 75. Settings that affect every device | `DisturbanceCheck`; the list in 7.1 through the confirm step for commands, property writes and settings writes, with `affected`'s `mode`, `adc`, `streamIndex` and `state`; the receive antenna kept on a band crossing (ruling 5.11a, `antennaKept`); the transmit-slice on-air refusal on a Protocol 2 rate change that would move or close the transmit slice (whichever of 34 and 75 lands second wires it); the target rules (rulings 7.5, 7.6); notices | 74 |
| 76. Media and capacity per device | a media controller per session; one mix per owner and the Core's local output; `DisplayBudgetSplit`, with `sharedConnection` and `sharedProcessing` (9.3); telemetry per session; link sections 6.4 and 11. The split takes the holder as an input: with no `TransmitHolder` yet it treats transmit as unheld (rule 3, equal shares), and Task 34 gives it the holder | 72, 73 |
| 77. Taking transmit | `tx.take` and its confirmation, on the Core or first on the device (`holderEpoch`, `shownKeyed`); the take during the 180 s; "Unkey and take over" through the transfer; D52 on edges with the take-then-key rule (rulings 8.8, 8.9); the hosting desktop's MOX and TUNE asking (ruling 8.9a); Take it back; the transmit slice on a transfer and its freeze (rulings 8.10, 8.11); TX only on the holder's slice (ruling 5.4a); the TCI holder rule (ruling 8.14); `ps3.twoTone` and `tuner.tune` taking transmit first (rulings 7.7, 8.3); the transmitter-path rulings 7.7 and 7.8 (the on-air refusals are Task 34's) | 34, 35, 39, 75, gaps Tasks 4 and 7 |
| 78. The desktop's screens for several devices | section 12, in the remote window and on a hosting desktop | 48, 49 (and so 41, 75, 77) |

### 13.3 How they depend on each other

- **Listening with several devices** needs 71 to 76 and nothing from Part E or Part F: 71 on
  Part C (12, 13, 14, 16), then 72, 73, 74 and 75 in a line, and 76 on 72 and 73. Nothing in
  71 to 76 needs a transmit task: until Task 34 exists there is no holder, the split gives
  equal shares, and `txSlice` goes on the wire as today until Task 77. So a phone build that
  listens on Wi-Fi with other devices on the Core needs these six and its own phone tasks
  (13.4), not remote access or transmit.
- **Remote access** (Part E, 26 to 29 and 29a) needs only ruling 4.9 from this design, and
  Task 71's same-device rule for a path race that signs in twice.
- **Transmit:** the gaps plan's Tasks 4 and 7 (on `codex/receiver-tx-gaps`) come before Task
  34; its Task 7 is flagged there for an earlier review, which the operator decides (the gaps
  plan, lines 238-239). Then 34 and 35 as rewritten, 36 to 40, and 77 after 34, 35, 39 and
  75. Task 34 needs 71 for the away state and the 180 s.
- **The fifth device** (41) needs 71, 73 and 34. 42 needs 34's on-air refusal.
- **D60's refusals exist the moment transmit does:** Task 34 builds them with
  `TransmitHolder`, so Task 35, which keys, never runs without them. The transmit-slice
  refusal on moves and takes is wired by whichever of 34 and 74 (moves, takes) or 75 (rate
  changes) lands second.
- **Part H** (43 to 46) needs 75 for "Waiting for you to confirm."
- **The desktop:** 48 and 49 as rewritten need 71 to 77 for what they show; 78 comes after
  48 and 49, and so after 41, 75 and 77; 50 after 48.
- Part D's 23 and 24 need 76, and 25 needs 75.

### 13.4 Phone tasks the iPhone session re-reads

Not this session's to change; the iPhone session folds them into the phone plan (review
findings 5 and 13).

- **8** (the control session): `sessionHolder` keeps its name, and its capability is
  `sessionHolderVersion`; `session.held` and `session.takeover` are gated by the hello feature
  (ruling 10.1); `auth.request`'s `device` block carries `shortName` (ruling 4.3).
- **11** (media per device).
- **52** (`DisplayQualityAllocator`): plans inside the device's share, the budget design's
  order and its 256-pixel, 10-frames-a-second floor (9.3).
- **15** (sign-in): `auth.request`'s `device` block carries `shortName` at every sign-in, from Part C; pairing is unchanged.
- **16a** (finding stations): the TXT record's `devices` count for "4 devices on it" (ruling
  10.4).
- **29a** (the path race on the phone): sign in on one path only (ruling 4.9).
- **53** (markers): `marker:<id>` (5.4), `ownerShortName` on the label, TX only by ruling 5.4a,
  "Receiver `streamIndex` + 1".
- **54** (PTT): `txState`'s holder with `holderShortName` and `holderSource`, and
  `keyedForSeconds` in place of Task 39's planned `keyedSinceMs` for the TX clock, "changing
  hands",
  `tx.take {holderEpoch, shownKeyed}`, a tap on unheld transmit taking and keying (D63), the
  frozen slice read from `txState` (ruling 5.4a), the Sharing chip from the phone's own plan
  and `displayBudgetReason`'s two values (9.3); the TX panel and VOX are the holder's (rulings
  7.7, 8.4).
- **56** (the fifth-device, transmit, receiver and confirmation screens): `session.held` with
  the choice on the first replaceable entry, `placeTaken` and `placeFreed`; `confirm.request`
  with `affected`'s `mode`, `adc`, `streamIndex` and `state`; the proceed's readback (ruling
  7.4a); a drag that opened a question sends only its final value (ruling 7.6); `notice` with
  `bySource` and `secondsAgo`, and the `graceEnded`, `slicesNotRestored` and `antennaKept`
  kinds; Take it back preselecting `takenOverById`; the sheet closing when `capabilities`
  arrives (ruling 4.7); the on-air refusal (D60). Its "send their request only after
  confirmation, and Cancel sends nothing" becomes: the change goes to the Core, which applies
  nothing and answers with the `confirm.request` the sheet shows; Confirm sends
  `confirm.proceed`, Cancel `confirm.cancel` (7.3).
- **58** (Devices): Connected now from `connectedDevices` (durations by ruling 10.3), Paired
  from `devices`, one name per device on both (ruling 4.3).
- **70** (the bench).

Section 10.8 lists each name against the iPhone design's section 4.5.

---

## 14. Verification

### 14.1 Unit and integration

- **Unit** (injected clocks, no sleeps):
  - `DeviceSessionRegistry`: the four, the 180 s and their end, the same device, token
    windows, the fifth device's list and revision, the first-asked rule; the list ordered idle
    longest first with the choice on the first replaceable entry, a hosting desktop never
    chosen, activity counted from commands and writes but not heartbeats, a keyed device never
    idle; `placeTaken` kept for a device replaced while away and `placeFreed` for one whose
    time ran out; names and short names numbered by pairing order, the same in `devices` and
    `connectedDevices`; a missing short name falling back to the kind's word.
  - The redial arithmetic, as a table test over an injected clock: the Core's heartbeat (20 s,
    two missed pongs) against the 1, 2, 5, 10, 30, 60 s schedule, both alignments of 4.5,
    every attempt up to the sixth inside 180 s.
  - `SliceOwnership` and the stores: held slices return to their owner; adoption takes only
    unowned slices; a Core restart keeps owners; `DeviceLayoutStore` restore and its
    `slicesNotRestored` report; anchors passing.
  - The anchor's band change: shared, a `panMove` with each other slice's `moves` or
    `closes`; alone, today's retune; with the anchor's own other slice left outside, today's
    retune (design ruling 6.5a).
  - `DisturbanceCheck`, table-driven: Protocol 1 rate, Protocol 2 rate on a shared receiver,
    ADC0 preamp on a 2-ADC board with slices on both ADCs, PureSignal on a 1-ADC board,
    diversity, a shared blanker, a notch inside another passband, the tuner's antenna, the
    radio choice; the on-air refusals (D60: a pan move, a band change on a shared receiver, a
    rate change and a take against a keyed holder's transmit slice, a Protocol 1 rate change
    while keyed, and an amplifier, tuner, antenna, PureSignal, interlock or power-cap change
    from another device while the holder is on the air); `affected` carrying `mode`, `adc`,
    `streamIndex` and `state`.
  - The receive antenna on a band crossing: kept, with `antennaKept`, while another device
    listens through that antenna input; switched when nobody does; the transmit antenna
    applied at key-down either way.
  - The confirm step: expiry, a grown set re-asked, a changed target refused, a request dropped
    at session end, the proceed's readback for a property write, a settings write and a
    command.
  - `TransmitHolder`: take unheld, take held, keyed through the `UnkeyGate`; during a transfer
    every source's key refused (the old holder, the radio's PTT, VOX, TCI); `TimedOut` with MOX
    still on assigns no holder; a new holder starts unkeyed; VOX disarmed on every change and
    when the holder goes away; a dropped holder keeps transmit, unkeyed, keys refused until MOX
    reads off; a take during the 180 s asked without the red button; the returning device
    holding again only if nobody took it; release when the 180 s end, on leaving, replacement
    and revocation; `tx.take {holderEpoch, shownKeyed}` taking at once only while both still
    hold; the holder's names "Radio" and `holderSource` `radioPtt` after a take by the radio's
    PTT, and `device` for a device named "Radio"; a person's key on unheld transmit taking and
    keying; a program's key (`trigger` `tci`) refused `programNeedsTransmit` on unheld
    transmit and `otherDeviceHolds` on held, and keying while its device holds.
  - `txSlice` on the wire: true only while the owner holds transmit; false on a slice the
    radio's PTT transmits on; a change of holder sending the changed values.
  - The keying gate on edges: a refused press leaves the PTT mode unchanged; releasing a
    station microphone held through a remote take-back does not unkey the remote holder; the
    station's pending press keys only after the transfer and only if still down; a hosting
    desktop's MOX asking while another device holds.
  - `DisplayBudgetSplit`: the holder whole up to the total, PureSignal charged once, equal
    shares, max-min; `sharedConnection` without a governor cut and `sharedProcessing` with
    one, only while another device is admitted; equal shares with no holder.
  - `MirrorView`: A changes a shared blanker; B gets the delta, A does not.
  - The clock convention: every duration re-measured at send, none from the wall clock; a
    `notice` held for an away device carrying `secondsAgo` from when it happened.
  - Discovery: the announcement's count byte and its absence, the 480-byte bound, the TXT
    record's sixth entry.
- **Integration: a multi-session harness.** One `StationServer` over a `ConnectableRadioModel`
  (the fake Protocol 1 radio for a 1-ADC board; a 2-ADC board primed the way
  `DaemonApp::primeBoardForTest` does), and several `LoopbackTransport` clients signed in as
  different paired devices, walking each of D44 to D64; media through fake transports,
  one controller per device; and a hosting-desktop harness, `StationHost` on a local model, with
  the local window as a device.
- **Red checks:** remove the owner filter and the foreign-write test fails; drop the per-writer
  echo and B misses A's shared blanker change; take the press rule out and a held PTT takes
  transmit straight back; drop the transfer's key refusal and the old holder's re-press keys
  during the unkey.

### 14.2 Conformance fixtures for several clients

| Fixture | Holds the Core to | `runs` |
| --- | --- | --- |
| `two-devices` | each device receives its own slices and the other's markers | `station`, `app` |
| `foreign-write-refused` | a write and a verb on another device's slice, refused with the plain reason | `station`, `app` |
| `share-receiver` | B's slice joins A's receiver; A's C-Tune move asks, naming B; on proceed B's slice moves or closes and B is told | `station`, `app` |
| `anchor-band-change` | B's slice shares A's receiver; A retunes its slice to another band: `panMove` with the bands in `change`, naming B; on proceed the receiver follows A and B's slice moves or closes; on cancel nothing changes | `station`, `app` |
| `non-anchor-pan-move` | B moves its pan on A's receiver: B goes to a free receiver, nobody asked; with none free, the chooser | `station`, `app` |
| `take-receiver` | every receiver held; the chooser with each receiver's slices and devices; proceed closes every other device's slice on the chosen receiver, the owners told with Take it back; Take it back asks the other way | `station`, `app` |
| `take-slice` | the slice cap full with receivers free: the slice chooser | `station` |
| `shared-setting-confirm` | a receive antenna change (ruling 7.1a: it asks) with another device on ADC0: `confirm.request` with `change`, each device's `state` and each slice's `mode`, `adc`, `streamIndex` and `effect`; proceed, its result carrying the readback; `notice` with who, what and `secondsAgo` | `station`, `app` |
| `confirm-grew`, `confirm-target-changed` | the disturbed set grows, or the target changes, before proceed: a new request or a refusal, nothing applied | `station` |
| `shared-setting-notice` | ruling 7.1a: a preamp change with another device on ADC0 applies at once, nobody asked; the other device gets `notice` `settingChanged` and the `delta` | `station` |
| `on-air-refusals` | a pan move, a band change on a shared receiver, a rate change and a take against a keyed holder's transmit slice, and a tuner, amplifier or antenna change from another device while the holder is keyed, are refused with the on-air reason naming the holder (D60) | `station` |
| `antenna-kept` | A crosses a band edge while B listens on the same ADC: the receive antenna stays, A gets `antennaKept`; with B gone, the next crossing switches | `station` |
| `take-transmit`, `take-transmit-keyed` | the holder named; red when keyed; the unkey before the change; the new holder unkeyed; `tx.take {holderEpoch, shownKeyed}` taking at once, and asked again when the holder keyed since | `station`, `app` |
| `transfer-refuses-keys` | the old holder's key during the transfer is refused | `station` |
| `radio-ptt-takes-transmit` | an injected PTT press takes transmit, and every session's `txState` names "Radio" with `holderSource` `radioPtt`; the frozen slice's owner sees `txSlice` false; a held PTT does not take it back; a release does not unkey the remote holder | `station` |
| `unheld-key` | a device's key on unheld transmit takes and keys, and so do `tuner.tune` and `ps3.twoTone` (never a carrier with nobody holding); a program's key (`trigger` `tci`) on unheld transmit is refused `programNeedsTransmit` and keys once its device holds | `station`, `app` |
| `tx-mark` | `txSlice` true only on the holder's own transmit slice, on `slice:` and `marker:` alike | `station`, `app` |
| `fifth-device`, `fifth-device-cancel`, `fifth-device-no-answer` | the list idle longest first, with each device's short name, state, slices and TX clock; a non-replaceable entry refused; the replace, with `takenOverBy`, `takenOverById` and `secondsAgo`; `coreFull`; gated by the hello feature | `station`, `app` |
| `fifth-device-away` | an away device replaced; at its next sign-in, `placeTaken` with who and when | `station` |
| `same-device-again` | the older connection ends `sameDevice`; slices kept | `station` |
| `grace-return`, `grace-expired`, `held-for-device` | away kept for 180 s; closed, or held for the device when it was the last; another device does not adopt held slices; a device back after 180 s gets `graceEnded`, or `placeFreed` on a full Core | `station`, `app` |
| `grace-transmit-held`, `take-during-grace` | the holder drops: unkeyed at once, transmit held for it; it comes back holding transmit, unkeyed. With a take during grace: asked without the red button; the returning device gets transmit only by taking it, and its `transmitTaken` arrives after `snapshot.complete` | `station`, `app` |
| `connected-devices` | `connectedDevices` lists each session with short name, state, durations, slices and TX; names numbered as in `devices`; a token window and a hosting desktop marked; an older view never receives it | `station`, `app` |
| `sharing-budget` | a holder and another device over the Core's total: the holder's share is its whole request; the other's reason is `sharedConnection`, or `sharedProcessing` with the governor's cut in force | `station` |
| `older-window` | own objects only; a disturbing change refused; a full Core refused retryable; a take of its last slice ends it `takenOver` | `station` |
| `short-name` | a device signs in with a `shortName`; it appears on its markers, in `connectedDevices` and `devices`; a new sign-in replaces it | `station`, `app` |
| `connection-limit` | twenty-four other sockets | `station` |

The discovery vectors of ruling 10.4 (`lan-announcement-2-devices`, the rewritten
`dnssd-txt`) join the media vectors (the link, section 16.4).

### 14.3 Bench (pending until observed)

Two or three devices: the desktop remote window on the Mac, the iPhone, the iPad. The
fifth-device row adds desktop windows started with their own profiles, each paired as its own
device (if Task 18 keeps a device key per profile; otherwise a test client).

**ANAN-G2 (Protocol 2, 2 ADCs) at the Rock Core:**

1. Two devices, each on its own receiver: markers on both; a write to the other's slice refused.
2. B joins A's receiver; A drags its pan: asked, naming B; B's slice moves to a free receiver;
   with all five held, B's slice closes and B is told. B drags its own pan: B moves to a free
   receiver and A is not asked.
3. All five receivers held; the phone adds a slice: the chooser; the taken device is told and
   takes it back.
4. A keyed into a dummy load; B takes with "Unkey and take over": the carrier stops before B
   holds; A re-pressing PTT during the unkey keys nothing; A is told; B starts unkeyed.
5. The radio's mic PTT pressed while the phone holds and is keyed: the phone is unkeyed first
   and told; the station keys only after the change; every device shows "Radio" as the
   holder. The phone takes transmit back while the
   mic is still held: releasing the mic does not unkey the phone.
6. B on EXT1 (ADC1) only; A changes the ADC0 preamp: B is not asked. With B on ANT1, B is asked
   and told.
7. PureSignal on the G2 (2 ADCs): while A transmits, B's receivers keep running.
8. The fifth device: four connected, one of them keyed; the fifth's list starts on the device
   idle longest that can be replaced; the fifth replaces the keyed one; it is unkeyed first and
   told, and its Take it back preselects the taker.
9. B shares A's receiver on 40 m; A taps 20 m: asked, naming B, "Go to 20 m" and "Stay on 40
   m"; on Go, A's receiver moves and B's slice moves or closes. With B's slice on ANT1 and A
   crossing onto a band whose antenna is ANT2, A is told the antenna stays on ANT1.

**Hermes Lite 2 (Protocol 1, 1 ADC) at the Pi 4 Core:**

10. A changes the sample rate while B has a slice: asked, naming B; B told; B's slice re-placed
   or closed. While A is keyed, B's rate change is refused with the on-air reason.
11. PureSignal on the HL2 while A transmits: B's receivers pause, shown on B's markers, and come
    back.
12. The phone, holding transmit and keyed, in airplane mode for 30 s: the radio unkeyed at
    once, VOX disarmed, the phone shown away with transmit held for it; it comes back holding
    transmit, unkeyed, and keys only on the next press. Again, with the iPad taking transmit
    during the 30 s (asked without the red button): the phone comes back without transmit
    and finds the notice. After 3 minutes away its slices close, or are held for it when it
    was the only device, and transmit is released; when it reconnects its slices come back
    and it shows the `graceEnded` notice, not "transmit was still yours". With the phone
    locked in a pocket, playing audio, for 5 minutes, it never goes away at all.
13. Capacity: the holder with two pans at 30 frames a second, two other devices: the others'
    frame rates drop first and their bands show Sharing with the rate they get, while the
    holder's band stays whole and sound is never cut; the chip's rate is the phone's own
    plan's; host CPU, receiver load and uplink recorded with the load average (the Core's
    telemetry and `top`).

**Either:**

14. An older desktop window (built before this) admitted with its own slices; its disturbing
    change refused in plain words; refused retryable when the Core is full.
15. A desktop hosting the Core with the phone connected: the desktop window counts as one of the
    four; its MOX while the phone holds transmit asks "Take transmit from the iPhone?" (red
    when the phone is keyed), while the desk microphone takes transmit at once; WSJT-X through
    the desktop's TCI keys only while the desktop holds transmit, and is refused with the
    window saying why otherwise. A fifth device cannot pick the desktop.
16. A Core with no NereusSDR window: a program on it through the Core's own TCI server never
    keys.

---

## 15. Conflicts with the decisions

Where the design cannot follow a decision as written, or narrows or widens one. Each has its
evidence and how the design handles it. Every open question the draft raised is settled
(section 16).

### 15.1 Conflicts

| Conflict | Evidence | Handled by |
| --- | --- | --- |
| D51 against D52, for a hosting desktop's own MOX: the desktop is at the Core's computer and also one of the four devices | section 2.2 (D35); D52's text | D64 (the operator's answer H): the desktop's MOX and TUNE ask like any device's; the desk microphone, the radio's own PTT, takes at once (ruling 8.9a) |
| D52 ("an app on the Core's computer") against the agreed receive-only Core TCI server | `StationTciController.cpp:44-45`; `TciServer.cpp:2660-2675`; Task 35 planned station TCI keying (the plan, lines 3399-3402) | D58 (answers A and B) amends D52 to "a mic or a footswitch": a program keys only while its window's device holds transmit, and on a Core with no window never; the Core's own TCI server stays receive-only (rulings 8.3, 8.14) |
| D51's "among its own slices" against the radio's PTT on a Core with no desktop, where the station device owns no slice | ruling 5.2 | ruling 8.11 (transmits on the current transmit slice, frozen while keyed; D64 confirms the freeze); TX shows on no slice then (ruling 5.4a) |
| D49 against an older window's last slice, which a remote window cannot drop | `RadioModel.cpp:4511-4520`, `7562-7567` | ruling 6.10, which D59 (answer C) settles |
| D50 against the anchor ruling (a device not anchoring a shared receiver moving its pan) | section 2.3; `SliceStreamAllocator.cpp:119-124` | ruling 6.6: it moves to its own receiver, disturbing nobody; D50 is not narrowed |
| D50 against the anchor's band change on a shared receiver, which today's allocator treats as a retune that moves only the anchor's slice | `SliceStreamAllocator.cpp:200-218` `@aa6c5505`; review finding 1 | ruling 6.5: a `panMove`, D50 as written; design ruling 6.5a when the anchor's own other slice would be stranded |
| D53 against refusing transmitter-path changes while the holder is on the air | ruling 7.8 | D60 (answer D): they wait (ruling 7.4) |
| D49 against ruling 6.8: a keyed holder's transmit receiver cannot be taken while on the air | D51 makes "Unkey and take over" the only way another device stops a transmission | ruling 6.8, confirmed by D64 (answer H) |
| D53 against a band crossing that switches the per-band antenna under other devices without asking | `RadioModel.cpp:12817-12826` | D61 (answer E): the receive antenna stays put (ruling 5.11a) |
| D45 against D48: a shared receiver's blanker and antenna are the receiver's, so one device's change alters another's slice | `RadioModel.cpp:6737-6760`, `19061-19077` | ruling 6.1 (treated as D53) |
| D50 and D53 against the on-air rule: a change that would move or close a keyed holder's transmit slice, or a Protocol 1 rate change while on the air, is refused instead of asked | `RadioModel.cpp:17886-17898` | ruling 7.4, which D60 (answer D) settles |
| D54's "full display" when the governor has cut the Core's total below the holder's request | the governor lowers the total (`DisplayLoadGovernor.h:65-116`) | ruling 9.3: the holder gets the whole total and the others' displays pause, sound kept, until the load falls (D54; D64 confirms sound is never cut) |
| D54's "Sharing" words against what the Core can measure: it measures processing, not its uplink | `DisplayLoadGovernor.h:67-79` `@aa6c5505`; `DisplayBudget.h:82-85` `@aa6c5505` | design ruling 9.3a: `sharedConnection` (the allowance the Core sends within is shared) and `sharedProcessing` (the governor's cut) |
| The controller's ruling (transmit unheld when its holder disconnects) against settled detail S4 | sections 2.3 and 2.4 | S4 wins: transmit is held for the device through its 3 minutes (D56, D62; ruling 8.15) |
| The controller's confirm ruling (a disturbing change answers "needs confirmation") against the phone's take-transmit sheet, shown before any request (the plan at `06209b82`, Task 54) | section 2.3 | ruling 8.7: `tx.take {holderEpoch, shownKeyed}` is itself the confirming command, taken at once only while the holder and its on-air state are what the operator saw; otherwise the Core asks |
| D52's "the press" against a PTT reported as a level | `RadioModel.cpp:1307-1311` | ruling 8.8 (edges), not a conflict but required to build it |

### 15.2 Widenings and narrowings, listed plainly for the operator

**Narrowings the operator confirmed on 2026-09-24:**

- **D56 narrowed to 3 minutes.** A device coming back to its own dropped session gets its
  slices and transmit back only within 180 s; after that its slices are saved and its place
  and transmit freed (D62, the operator's answer F, "3min"; rulings 4.10, 4.11, 8.15). The
  iPhone design bounds D56 by D62 at `94ec2f7e`.
- **Ruling 6.8.** A keyed holder's transmit receiver cannot be taken (narrows D49; D64, answer
  H item 1).
- **Ruling 7.4, as D60 settles it.** While the holder is on the air, a change from another
  device to the amp, the tuner, an antenna, PureSignal, the interlock or the power cap, a change
  that would move or close the holder's transmit slice, and a Protocol 1 sample-rate change,
  wait: refused, not asked (narrows D50 and D53; D60, answer D).
- **Ruling 8.11.** While the radio's own PTT is keyed, the slice it transmits on cannot be
  retuned until the press ends (narrows D45; D64, answer H item 2).
- **A fifth device cannot replace the hosting desktop** (narrows D55; ruling 4.6; D64, answer H
  item 3).
- **Sound is never cut.** When the Core runs short, the others' bands slow first, then pause
  with sound kept, until there is room (a reading of D54; ruling 9.3, 9.4; D64, answer H item
  4).
- **The hosting desktop's own MOX and TUNE ask like any device's** ("Take transmit from the
  iPhone?", red when that device is on the air), while the desk microphone, the radio's own
  PTT, still takes transmit at once (D35 as amended; ruling 8.9a; D64, answer H item 5).
- **D52 narrowed to the radio's own PTT** (a mic or a footswitch). A program, even on the
  Core's computer, keys only while its window's device holds transmit, and on a Core with no
  window never (D58, answers A and B; rulings 8.3, 8.5, 8.14).

**Narrowing confirmed by the operator on 2026-09-24 (re-review finding 9):** "Turned away,
saying why".

- **An older window is turned away when there is no room.** It cannot answer the
  fifth-device question, so a full Core (four devices on) refuses it, retryable, instead of
  letting it take a place (D55), and a Core with no receiver free for its slice refuses it the
  same way (D59 lets it in, but only with a slice). The refusal says: "The Core is full. Update
  NereusSDR to take a device's place, or try again later." Nobody already connected is
  disturbed. It gets in on its own backoff when a place or a receiver frees, or once it is
  updated (10.7, ruling 10.2).

**The operator's own decision, not a narrowing:** a person's key on unheld transmit takes it
and keys (D63, answer G; ruling 8.3). It differs from D51's rejected "Whoever keys first"
because nobody is taken from; programs still never take transmit.

**Widenings and narrowings the design makes within the decisions:**

- **Widened:** D53's rule covers more than its six items: diversity, a shared receiver's
  noise blanker, notches, receive DSP options, the receive filter policy, the transmit antenna,
  the transmit interlock and power cap, and the choice of radio (ruling 7.1).
- **Widened:** a change that touches the transmitter counts the holder as disturbed even when
  none of its slices are affected, so it is asked for and named (ruling 7.8).
- **Widened:** D49's take extends to a single slice when the slice cap, not the receivers,
  is full (ruling 6.9).
- **Narrowed:** the transmitter's own settings (power, mic, processing, VOX, profiles, the TX
  filter) and the actions that key (two-tone, the tuner's tune) are the holder's while transmit
  is held (ruling 7.7).
- **Narrowed:** arming VOX needs transmit, and VOX is disarmed at every change of holder (ruling
  8.4).
- **Narrowed:** only the holder unkeys; another device stops a transmission by taking transmit
  (ruling 8.5).
- **Narrowed:** TX marks a slice only while its owner holds transmit, so the slice the radio's
  PTT transmits on shows no TX (ruling 5.4a; review finding 3, drawn by D51's "TX marks its
  slice").

---

## 16. Decisions made

The draft's six open questions, and the two the review added (the reclaim limit and a key on
unheld transmit), went to the operator on 2026-09-24 as questions A to H. Each answer is quoted
in his words where he gave words, with its decision number in the iPhone design (`94ec2f7e`)
and where it landed here. No question is left open.

| # | The question | The operator's answer | Where it landed |
| --- | --- | --- | --- |
| A (D58) | An app on a connected device's computer (WSJT-X through a window's TCI, VAX or CAT) keys | "Only if its device has transmit". It transmits only while that window already holds transmit; otherwise the key is refused and the window says why. A person takes transmit first; a program never takes transmit from someone and never makes anyone the control operator without them choosing it. | 2.2; 5.9 (ruling 5.13); ruling 8.3 (`programNeedsTransmit`); ruling 8.14; 12 item 2; 13.1 Task 35 |
| B (D58, D52 amended) | Apps on the Core's own computer, put by the operator as "a tci program on the same computer as core/gui client should be able to tx?" | "It waits for its window". A TCI program on the computer whose NereusSDR window hosts the Core transmits only while that window holds transmit; if another device holds it, the key is refused and the window says why. D52 narrows to the radio's own PTT (mic and footswitch). On a Core with no NereusSDR window, programs cannot key, and the Core's own TCI server stays receive-only. Ruling 8.1 becomes: an app's key on the hosting desktop's TCI server is taken only while that window holds transmit, otherwise refused. The source field stays, so a radio-PTT take shows as "Radio". | 2.1 (D52); 4.1; ruling 8.1 (`holderSource`, no take named after a program); 8.5; ruling 8.14; 13.1 Tasks 35, 48; 15.2 |
| C (D59) | An older window meets several devices | "Let it in". It is admitted and works with its own slices; anything that would affect another device is refused with a plain "update NereusSDR" message; if a take closes the only slice it has, its session ends with that reason. | 4.7; ruling 6.10; 7.3 (the older window's refusal); 10.7 |
| D (D60) | Transmitter-path changes while the holder is on the air | "Wait until they stop". A change to the amp, the tuner, an antenna, PureSignal, the interlock or the power cap from another device is refused while the holder is transmitting, with "The iPhone is on the air. Try again when they stop." (the holder's name in place of "The iPhone"). It answers ruling 7.4 too: a change that would move or close the keyed holder's transmit slice, or a Protocol 1 sample-rate change during a transmission, waits the same way. The other device can still Unkey and take over, then make the change. | ruling 7.4; ruling 7.8; rulings 6.4, 6.5; 13.1 Task 42; 13.2 Task 77; 15.2 |
| E (D61) | The band-crossing antenna | "The antenna stays put". While another device listens through that antenna input, the receive antenna stays where it is, and the person tuning is told ("The antenna stays on ANT1 while the iPad listens on it."). Tuning is never held up by a question. The new band's transmit antenna still applies at key-down. | ruling 5.11a (`antennaKept`); 7.4 notices; 13.2 Task 75 |
| F (D62, D56 bounded) | The reclaim limit | "3min" (typed, not either option offered). The grace period is 180 s. Within it a dropped device keeps its place, its slices keep running, and transmit stays held for it, unkeyed. After it, the slices are saved for its return, and its place and transmit are freed; a device returning to a full Core then gets the fifth-device question. | 4.5 (rulings 4.10, 4.11, the redial arithmetic, what each side shows); 4.3 (`placeFreed`); ruling 8.15; 10.5 (`graceMs` 180000); 15.2 |
| G (D63) | A key while transmit is unheld | "The tap takes it and keys". When nobody holds transmit, a person's PTT takes transmit and keys at once, and every device shows who has it. It differs from D51's rejected "Whoever keys first" because nobody is taken from. Programs still follow answer A: they never take transmit, even when it is unheld. | ruling 8.3; ruling 8.9a (with transmit unheld); 15.2 |
| H (D64) | The narrowings, confirmed together | "Keep all five": a keyed holder's transmit receiver cannot be taken (ruling 6.8); while the radio's own PTT is keyed, the slice it transmits on cannot be retuned until the press ends (ruling 8.11); a fifth device cannot replace the hosting desktop; sound is never cut when the Core runs short; the hosting desktop's own MOX and TUNE ask like any device's, while the desk mic still takes transmit at once. | rulings 6.8, 8.11, 4.6, 9.3 and 9.4, 8.9a; 15.2 |

---

## Appendix A. Requirement IDs (provisional)

For the plan's task headers and commit subjects. The controller confirms the prefix.

| ID | Requirement | Sections |
| --- | --- | --- |
| R-MC-01 | Up to four device sessions; one per device; a socket cap separate from the device limit | 4.1, 4.2 |
| R-MC-02 | The fifth device's list and replacement, with the displaced device told | 4.3 |
| R-MC-03 | The same device again, the 180 s grace period with transmit held for an away holder, leaving on purpose | 4.4, 4.5, 4.6, 8.10 |
| R-MC-04 | A mirror view per device, echo per writer, per-session routing | 5.5 |
| R-MC-05 | Slice ownership, held slices, the two stores, adoption and a device's first slice | 5.1, 5.2, 5.3 |
| R-MC-06 | Markers for other devices' slices; refusals for their objects | 5.4, 5.6 |
| R-MC-07 | The active slice per device and the station-level active slice | 5.7 |
| R-MC-08 | Sharing receivers, the anchor and its pin | 6.1, 6.2 |
| R-MC-09 | Moving a pan, by the anchor and by another device | 6.3 |
| R-MC-10 | Taking a receiver or a slice, with Take it back; an older window's last slice | 6.4 |
| R-MC-11 | The disturbed set, the on-air refusals and the confirm step | 7.2, 7.3 |
| R-MC-12 | Shared settings, the transmitter's own settings, notices | 7.1, 7.4, 7.5 |
| R-MC-13 | The transmit holder and the transfer; taking transmit, Unkey and take over, Take it back | 8.1, 8.2, 8.4, 8.6 |
| R-MC-14 | Keying rules: a person's key on unheld transmit, programs only while their device holds, VOX, who unkeys; the radio's PTT on edges; the hosting desktop's MOX asking | 8.3, 8.5 |
| R-MC-15 | The transmit slice and its freeze, MoxController and TxSliceArbiter identity, TCI, the holder leaving | 8.7-8.10 |
| R-MC-16 | A media controller per device and each device hearing its own slices | 9.1, 9.2 |
| R-MC-17 | The display budget split, holder first | 9.3 |
| R-MC-18 | The link: feature, capability, messages, objects, verbs, short names, one clock convention, the count in discovery, conformance | 4.1, 10 |
| R-MC-19 | Older windows | 10.7 |
| R-MC-20 | The desktop's screens | 12 |
| R-MC-21 | Presence on every device: who holds transmit and whether on the air, and each connected device's activity, slices and bands | 8.1, 10.3, 10.8 |
