# TCI Port Design (Phase 3J-1)

**Status:** Draft (pending user spec review)
**Author:** J.J. Boyd (KG4VCF)
**Date:** 2026-05-09
**Master plan phase:** 3J (TCI portion only; CAT/MIDI in Phase 3K-1/2/3)
**Related:** [docs/MASTER-PLAN.md](../MASTER-PLAN.md), [CONTRIBUTING.md](../../CONTRIBUTING.md)

## Source versions reviewed

| Repo | Commit | Notes |
|---|---|---|
| Thetis | `v2.10.3.13 @501e3f51` | Authoritative for TCI behavior; `Project Files/Source/Console/TCIServer.cs` (8,821 lines) |
| AetherSDR | `@0cd4559` (boydsoftprez fork; upstream main HEAD reviewed for transport patterns) | Structural template for Qt6 transport layer; `src/core/TciServer.{h,cpp}` (180+1,621 LOC) and `src/core/TciProtocol.{h,cpp}` (123+1,487 LOC) |
| WDSP | NereusSDR `third_party/wdsp/` (TAPR v1.29 + linux_port.h) | Resampler `create_resampleFV` / `xresampleFV` for audio rate conversion in TCI streams |

## Executive summary

Phase 3J-1 ports Thetis TCI (Transceiver Control Interface) to NereusSDR with **full Thetis parity** as a ship-blocker requirement. The implementation uses a **hybrid approach**: AetherSDR's Qt6 transport layer (`QWebSocketServer` + multi-client lifecycle) is ported verbatim, while the protocol layer (`TciProtocol`) is written from scratch reading Thetis source 1:1.

Five user-locked decisions and one architectural divergence shape this PR:

1. **Protocol identifier** defaults to `Thetis,2.0` for client-string-match compatibility, with full feature parity on six compat flags (`EmulateExpertSDR3Protocol`, `EmulateSunSDR2Pro`, `CWLUbecomesCW`, `CWbecomesCWUabove10mhz`, `IQSwap`, `AlwaysStreamIQ`).
2. **Init burst typo** at Thetis `TCIServer.cs:2374-2375` (verified copy-paste bug) is **fixed** in our port. Conscious divergence, documented inline and in the divergence ledger.
3. **Two-switch dispatch** mirrors Thetis exactly (`parts.Length==1` queries + `parts.Length==2` sets).
4. **TX audio silent rejection** replicates Thetis verbatim. Surfaced to operator in TciApplet/ClientChainApplet UI, never as a wire-visible error.
5. **Outbound text encoding** is UTF-8 on all platforms. Diverges from Thetis-on-Windows `Encoding.Default` (Windows-1252) for non-ASCII bytes; ASCII content (>99% of wire traffic) is byte-identical.

NereusSDR's slice architecture (Slice A/B/C/D, replacing RX1/RX2) maps to Thetis's `trx:N` wire format at the protocol layer boundary. UI shows Slice A/B/etc; the wire keeps Thetis-strict `trx_count:2;` by default.

Phase 3J-1 scope: TCI server, protocol layer, two new applets (TciApplet + ClientChainApplet), Setup page rewrite, banner status badge, menu integration. Spots are deferred to Phase 3J-2; CW TX commands are stubbed for 3M-2; CAT TCP/serial/MIDI follow as Phase 3K-1/2/3.

---

## 1. Architecture overview

TCI lives on its own thread, parallel to the existing Connection / Audio / Spectrum threads. `TciServer` (transport) and `TciProtocol` (Thetis-faithful logic) both run on the TCI thread. Cross-thread access uses `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` for state reads. State writes go through model setters which already emit signals. No model state lives on the TCI thread.

```
┌────────────────────────────────────────────────────────────┐
│  Setup → Network → TCI Server (CatTciServerPage)           │   GUI
│  TciApplet  (Slice meters/gain, port, enable)              │
│  ClientChainApplet  (per-client status rows)               │
│  Bottom status-bar m_tciIndicator (live-wired existing)    │
└──────────────────────────┬─────────────────────────────────┘
                           │ Qt signals (auto-queued)
┌──────────────────────────▼─────────────────────────────────┐
│  TciServer  (Qt6 QWebSocketServer + multi-client)          │   TCI thread
│   per-client session state, three priority send queues,    │
│   binary audio + IQ pipelines, 3-layer VFO coalescing,     │
│   sensor push timers, broadcast fan-out                    │
│       │ TciProtocol::handleCommand(QString) → QString      │
│       │ TciProtocol::pendingNotification() → QString       │
│       ▼                                                    │
│  TciProtocol  (Thetis-faithful, transport-blind)           │
│   2 dispatch tables (set + query), 62 commands,            │
│   12 compat flags, 49 per-client state fields,             │
│   slice→trx mapping, mode map, dB volume scaling           │
└──────────────────────────┬─────────────────────────────────┘
                           │ QMetaObject::invokeMethod
┌──────────────────────────▼─────────────────────────────────┐
│  RadioModel  SliceModel  TransmitModel  RxChannel          │   Main / Audio
│  AudioEngine  FFTEngine   (existing, only new tap points)  │
└────────────────────────────────────────────────────────────┘
```

### 1.1 Thread layout

| Thread | TCI role |
|---|---|
| Main (GUI) | Setup page edits, applet UI updates, title-bar badge |
| Connection | Untouched. Already emits `rawIqData` we tap |
| Audio | New tap into post-DSP RX audio (lock-free SPSC ring write); new TX SPSC ring drain |
| Spectrum | Untouched. TCI does not consume FFT output, only raw I/Q |
| **TCI (new)** | TciServer + TciProtocol + sensor timers + audio drain timer + WS server + 3-layer VFO coalescing |

### 1.2 Slice ↔ trx mapping (NereusSDR architectural divergence)

NereusSDR uses Slice A/B/C/D for multi-pan multi-slice support. Thetis hardcodes `trx:N` numeric (RX1/RX2). Mapping happens at the protocol-layer boundary:

| NereusSDR UI | TCI wire format | Notes |
|---|---|---|
| Slice A | `trx:0` | Thetis RX1 equivalent |
| Slice B | `trx:1` | Thetis RX2 equivalent |
| Slice C | `trx:2` | NereusSDR extension (internal-only) |
| Slice D | `trx:3` | NereusSDR extension (internal-only) |

The init burst sends `trx_count:2;` hardcoded for strict Thetis client compat (locked decision). Slice C/D exist internally but are NOT exposed via TCI in Phase 3J-1. Future advanced-user toggle to opt into `trx_count:4;` is out of scope.

---

## 2. Component breakdown

Eight components. Five new, three edits to existing scaffolding. All paths relative to `/Users/j.j.boyd/NereusSDR/`.

### 2.1 New: `src/core/TciServer.{h,cpp}` — transport layer

Owns `QWebSocketServer`, multi-client session table, three priority send queues per client, binary audio+IQ pipelines, 3-layer VFO coalescing thread, RX+TX sensor timers. Lives on TCI thread.

Verbatim port shape from AetherSDR `src/core/TciServer.{h,cpp}` (180+1,621 LOC), with these additions surfaced by Sweep C and E:

- **Three priority send queues per client** (Urgent / Binary / Control+Coalesced) per `TCIServer.cs:769-774 [v2.10.3.13]` with drain at `:1645-1679`. Drain order: Urgent first, then Binary, then Control. Bounded depth; oldest-drop on overflow.
- **3-layer VFO coalescing thread** per `TCIServer.cs:751, 747, 1314-1381 [v2.10.3.13]`: per-event one-shot timers + bounded LinkedList of 10 with oldest-drop + outbound-coalesced map. `m_VFODataThread`.
- **RX sensor timer** at `TCIServer.cs:2566 [v2.10.3.13]` (default 200 ms, range 30..1000 ms).
- **TX sensor timer** at `TCIServer.cs:2581 [v2.10.3.13]` (default 200 ms, MOX-gated by upstream meter pump).
- **20-second server-driven ping** with payload `"Thetis"` per `TCIServer.cs:2650-2654 [v2.10.3.13]`. Configured via `QWebSocket::setHandshakeOptions` rather than hand-rolled (Qt6 idiom).
- **Audio binary pipeline**: `publishRxAudio(rx, l, r, n, srcRate)` with 64-byte LE header (16×uint32) per `TCIServer.cs:5240 [v2.10.3.13]` (`buildStreamPayload`). Per-(client, rx) WDSP `create_resampleFV` instance for sample rate conversion.
- **TX audio inbound** per `TCIServer.cs:5602 [v2.10.3.13]` (`handleBinaryFrame`): single-client mutual exclusion via `m_txAudioActiveClient` field; second client gets silent drop (Decision 4 lock-in).

Per-client `ClientSession` struct mirrors Thetis `TCPIPtciSocketListener` per Sweep D (49 fields total). Critical fields: `m_iqStreamEnabled` (HashSet<int>, `TCIServer.cs:766`), `m_audioStreamEnabled` (HashSet<int>, `TCIServer.cs:767`), `m_audioStreamSamples` (int default 2048, range 100..2048), `m_seenModernTxAudioNegotiation` (bool), `m_disconnected` / `m_stopClient` lifecycle flags.

Estimated size: 3,500 to 4,000 LOC (revised upward from initial 2,400-2,800 estimate after Sweep C surfaced full threading model).

### 2.2 New: `src/core/TciProtocol.{h,cpp}` — Thetis-faithful command logic

Transport-blind. Holds `RadioModel*` + `QStringList m_pendingNotifications` queue. Returns command response strings.

Public API mirrors AetherSDR's seam at `TciProtocol.cpp:1-17 [@0cd4559]`:

- `handleCommand(QString) -> QString`
- `pendingNotification() -> QString` (drained by TciServer after each handleCommand)
- `buildInitBurst() -> QStringList` (sent on connect; ports `sendInitialisationData` `TCIServer.cs:2512` + `sendInitialRadioState` `TCIServer.cs:2363`)
- Slots fed by RadioModel signals (auto-queued onto TCI thread): `onVfoChanged`, `onModeChanged`, `onMoxChanged`, `onSplitChanged`, `onMeterUpdate`, etc. Each enqueues a notification string.

**Two dispatch tables** (locked Decision 3) mirror Thetis exactly:

- `handleSetCommand(name, args)` for `parts.Length==2` cases — 60 commands per `TCIServer.cs:4924-5128 [v2.10.3.13]`
- `handleQueryCommand(name)` for `parts.Length==1` cases — 21 commands per `TCIServer.cs:5134-5197 [v2.10.3.13]`
- ~13 command names appear in both tables as set/query pairs (Sweep A finding). AetherSDR's heuristic `isSet = !args.empty()` approach is a known bug source we explicitly avoid.

**Mode map**: `DSPMode` enum (LSB / USB / CWL / CWU / DIGL / DIGU / AM / SAM / DSB / NFM / FM) emitted **uppercase** per `TCIServer.cs:2155 [v2.10.3.13]` (`//MW0LGE_22b mods are uppercase on the sun, replicate`).

**Volume scaling**: dB float in `[-60, 0]`, not 0-100 int. Math from Thetis `linearToDbVolume` / `audioGainToDb` at `TCIServer.cs:4110-4132 [v2.10.3.13]`.

**12 compat flags** with full feature parity (Decision 1 addendum + Sweep D enumeration):

| Flag | AppSettings key | Default | Effect |
|---|---|---|---|
| `EmulateExpertSDR3Protocol` | `TciEmulateExpertSDR3Protocol` | False | `protocol:Thetis,2.0;` becomes `protocol:ExpertSDR3,2.0;` per `TCIServer.cs:2515` |
| `EmulateSunSDR2Pro` | `TciEmulateSunSDR2Pro` | False | `device:HardwareSpecific.Model;` becomes `device:SunSDR2PRO;` per `TCIServer.cs:2523` |
| `CWLUbecomesCW` | `TciCwluBecomesCw` | False | Adds `cw` to modulations_list; remaps mode reports per `TCIServer.cs:2148, 2540` |
| `CWbecomesCWUabove10mhz` | `TciCwBecomesCwuAbove10mhz` | False | Mode reporting flips above 10 MHz per `TCIServer.cs:3873` (W2PA fix for issue #559, `[2.10.3.9]MW0LGE`) |
| `IQSwap` | `TciIqSwap` | True | Swaps I/Q in binary IQ stream per `TCIServer.cs:6111` |
| `AlwaysStreamIQ` | `TciAlwaysStreamIq` | False | Forces IQ stream regardless of subscription per `TCIServer.cs:5401, 7506` |
| `SendInitialFrequencyStateOnConnect` | `TciSendInitialFrequencyStateOnConnect` | True | Gates VFO/IF/DDS sends in init burst per `TCIServer.cs:2365` |
| `RateLimit` | `TciRateLimitMsgsPerSec` | 60 | Per-client message rate cap |
| `ForgetRX2VfoBVFOinfo` | `TciForgetRx2VfoBOnDisconnect` | False | VFO state cleanup quirk |
| `UseRX1vfoaForRX2vfoa` | `TciUseRx1VfoaForRx2Vfoa` | False | VFO mirror quirk |
| `CopyRX2VFObToVFOa` | `TciCopyRx2VfobToVfoa` | False | VFO mirror quirk |
| (per-client) `m_seenModernTxAudioNegotiation` | n/a (runtime) | n/a | Modern vs legacy TX audio header detection |

Estimated size: 2,400 to 2,800 LOC (revised upward from 2,000-2,400 estimate after Sweep D enumerated 12 flags + 49 per-client fields).

### 2.3 New: `src/gui/applets/TciApplet.{h,cpp}` — operator-facing applet

Default-enabled per locked Q5. Sits in Container #0 stack.

Layout (NereusSDR-native, follows existing applet patterns from `RxApplet`/`TxApplet`):

- **Header row**: Status dot, "TCI Server" label, port spinbox (read-only display; Setup is the edit path per locked mockup decision), client count, Setup button
- **Slice A row**: Label + audio level meter + per-slice gain slider [-60..0] dB (post-DSP, TCI-stream-only)
- **TX row**: Label + TX audio level meter + TX gain slider, "silent" when not in MOX
- **Footer**: client count + "Show clients →" link to ClientChainApplet
- **Disabled state**: collapses to single "Enable Server" button + setup hint

When 3F multi-pan ships, the row stack expands to Slice B/C/D as those slices become active. Inline subscription badges per slice (e.g. "IQ:2 Audio:1") are out of scope for Phase 3J-1; full per-client details live in ClientChainApplet.

Wires through `AppSettings` keys: `TciServerEnabled`, `TciServerPort`. Apply on toggle (calls `TciServer::start/stop` queued to TCI thread).

Estimated size: 80 + 350 LOC.

### 2.4 New: `src/gui/applets/ClientChainApplet.{h,cpp}` — per-client status

Default-enabled per locked Q5. Single widget (not split into widget+applet like AetherSDR's `ClientChainApplet` + `ClientChainWidget`; that split was over-engineered).

Per-client row:

- **TX badge** (orange) on the client holding the TX audio mutex
- **Peer** `ip:port` + best-effort name (from WebSocket User-Agent header when present, "(unknown)" otherwise; Thetis has no client-self-id command)
- **Connect duration**
- **Subscription badges**: blue for IQ per-slice, blue for audio per-slice + sample rate, orange for TX direction
- **Last command**: monospace command snippet + age
- **Drop counter** (red): backpressure-driven frame drops per-client
- **Disconnect button**: admin override

Empty state shows bind address + connection hint, no wasted vertical space.

Drives off `TciServer::clientConnected/Disconnected` + a periodic refresh signal (1 Hz default).

Estimated size: 120 + 500 LOC.

### 2.5 Edit: `src/gui/setup/CatNetworkSetupPages.{h,cpp}` — TCI page rewrite

Existing `CatTciServerPage` (currently a placeholder) becomes the full-fat Thetis-parity page. Six group boxes 1:1 with Thetis `grpTCIServer` (per the userland-parity rule):

1. **Server**: Enable, Bind (read-only `127.0.0.1` per Q7), Port, Default Port button, Send initial state on connect, Rate limit, Show Log button, status line
2. **Compatibility**: 4 flags (Emulate ExpertSDR3 protocol, Emulate SunSDR2Pro device, CWL/CWU becomes CW, CW becomes CWU above 10 MHz)
3. **IQ Stream**: Swap I/Q (default on), Always stream IQ (force-on)
4. **Audio Stream**: Block size [100..2048] default 2048, TX channel L/R/Both selector
5. **Sensors**: RX/TX intervals [30..1000] default 200, with `MinimumRequiredRxSensorInterval` aggregation note
6. **VFO Quirks**: 3 Thetis chk* checkboxes (Forget RX2 VFO B, Use RX1 VFO A for RX2, Copy RX2 VFO B to VFO A)

Plus a placeholder noting Spots controls deferred to Phase 3J-2.

Existing file is 99+267 LOC; rewrite lands at ~120+500 LOC.

### 2.6 Edit: `src/gui/applets/CatApplet.{h,cpp}` — strip TCI button row

Existing CatApplet has a TCI button row. Per Q5, that row moves to the new TciApplet. CatApplet stays focused on CAT (Phase 3K-1/C scope).

Net change: ~30 LOC removed, no additions.

### 2.7 Edit: `src/gui/setup/AudioTciPage.{h,cpp}` — flesh out placeholder

Current file is 19+18 LOC, pure placeholder. Becomes the audio-side wiring (sample rate per slice, format selection, master mute behavior). Small page, ~60+150 LOC.

### 2.8 Stub: `SpotData` hook in `TciProtocol`

Phase 3J-2 (AetherSDR Spots port) lands later. Phase 3J-1 pre-creates the empty `handleSpotAdd/Remove/Clear/SimulateClick` handlers and wires the `spot_simulate_click` Thetis-only handler. Phase 3J-2 fills bodies, not wiring.

### 2.9 Edit: Banner & menu integration (live-wire existing scaffolding)

NereusSDR already has cold-wired TCI status scaffolding in MainWindow.cpp. Phase 3J-1 brings it to life rather than building new widgets.

| Existing scaffolding | Phase 3J-1 wires it to |
|---|---|
| `m_tciIndicator` @ MainWindow.cpp:3179 (stacked label "TCI" / "Off") | Live bottom-label updates from TciServer signals; format `Off` / `On` / `On · N` / `On · N ▸TX`; color dim/green/cyan/orange; tooltip shows bind/clients/TX-source; click opens Setup → Network → TCI Server |
| `m_tciSep` @ :3181 (separator) | No change. Existing drop-priority logic at MainWindow.cpp:4318 already pairs CAT/TCI together for narrow-window collapse |
| `tciAction` @ :2848 (Tools → TCI Server… disabled "NYI — Phase 3J") | Remove `setEnabled(false)` + NYI tooltip; wire to Setup → Network → TCI Server jump |
| `addContainerToggle("CAT / TCI", …)` @ :2795 (commented) | Replace with View → Network Applets ▶ submenu (TCI Server + TCI Clients toggles, both default-enabled per Q5; CAT/MIDI greyed for Phase 3K-1/2/3 placeholders) |

The `makeIndicator()` factory at MainWindow.cpp:3152 produces a stacked 2-line label widget (top 11px #607080, bottom 11px #404858, min-width 60px). Same pattern used by CAT/PSU/PA tiles. Live state changes the bottom-label text and color.

Net change in MainWindow.cpp: ~50 LOC (down from ~80 originally estimated; no new widget classes needed).

No new title-bar badge. No new popup widget. The hover tooltip + click-to-Setup is sufficient given the indicator already lives in the operator's glance path.

### 2.10 Build dependency: `qt6-websockets`

New hard requirement on Linux/macOS/Windows. CMake change in root `CMakeLists.txt`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS WebSockets)
target_link_libraries(NereusSDR PRIVATE Qt6::WebSockets)
```

Linux deps line in CLAUDE.md updates: add `qt6-websockets` (Arch) / `qt6-websockets-dev` (Ubuntu/Debian).

### 2.11 Total new code estimate

Roughly **6,000 to 7,000 LOC new + ~700 LOC edits**. Revised upward from initial 5,500 estimate after sweeps surfaced full Thetis state model; reduced ~150 LOC from initial revision after discovering existing `m_tciIndicator` / `tciAction` cold-wired scaffolding eliminates the need for new title-bar badge and popup widgets. Big PR but cohesive (single feature, single review surface).

---

## 3. Data flow

### 3.1 Inbound command path (client → radio)

Client WS frame → TciServer parses framing → `TciProtocol::handleCommand()` returns response + queues notifications → TciServer writes response to caller, drains and broadcasts notifications to all clients. Two-channel pattern (response unicast vs notification broadcast) matches Thetis. Seam preserved from AetherSDR `TciProtocol.cpp:1-17 [@0cd4559]`.

### 3.2 Outbound notification path (radio state → clients)

RadioModel signal (any thread) → `TciProtocol` slot (auto-queued onto TCI thread) → enqueues notification string → TciServer drain loop fans out to all clients. **VFO notifications go through the 3-layer coalescing thread** (per-event one-shot timers + bounded LinkedList of 10 with oldest-drop + outbound-coalesced map) per `TCIServer.cs:751, 1314-1381 [v2.10.3.13]`. A fast knob spin produces ≤ 1 outbound `vfo:` frame per coalesce interval, not 200/sec.

### 3.3 RX audio path (DSP → clients)

Audio thread: `RxChannel::processIq()` → `fexchange2()` → tap to per-slice lock-free SPSC ring buffer (write-side). Existing `AudioEngine::feedAudio()` path untouched.

TCI thread: 5ms drain timer reads ring → for each slice with ≥1 audio-subscribed client → assemble Thetis-shaped block per `m_audioStreamSamples` (default 2048) → for each subscribed client: WDSP `xresampleFV` if rate ≠ 48000 → `encodeSamples()` → 64-byte LE header + payload binary frame → `socket.sendBinaryMessage()`. Header layout per `TCIServer.cs:5240 [v2.10.3.13]` (`buildStreamPayload`).

WDSP resampler lifecycle: one `create_resampleFV` instance per (client, slice) tuple, created on subscribe, destroyed on unsubscribe or disconnect. Stored in `QHash<QPair<QString, int>, void*>` on TciServer.

### 3.4 TX audio path (clients → DSP)

Reverse direction. Inbound binary WS frame → TCI thread `handleBinaryFrame` per `TCIServer.cs:5602 [v2.10.3.13]` → modern-vs-legacy header detection (`TCIServer.cs:5628-5652`) → NaN/Inf zeroed and clamped to `[-4.0, 4.0]` → push to TX-direction SPSC ring → Audio thread drains into `TxChannel`.

**Single-client mutual exclusion**: `m_txAudioActiveClient` field on TciServer. Second client's TX frames silently dropped per locked Decision 4. Operator sees the rejection in TciApplet TX badge (the receiving client doesn't have the badge) and ClientChainApplet (no TX badge on the rejected client).

### 3.5 RX IQ path (raw I/Q → clients)

RadioModel already emits `rawIqData(rx, samples)` for FFTEngine. TCI adds another listener on the TCI thread. **Early-out** if no subscribed clients per `TCIServer.cs:5430 [v2.10.3.13]` (`PublishRxIqSamples`). When subscribed, encode as 64-byte LE header + interleaved I/Q float32 payload, broadcast. IQ is **always FLOAT32, always 2 channels** per `TCIServer.cs:5806-5810 [v2.10.3.13]` (verbatim comment: "not used atm... here for the future").

Apply `IQSwap` flag (default on) before encode per Sweep E.

### 3.6 Sensor push path (timer → clients)

Two timers on the TCI thread.

**RX timer** at 200ms default per `TCIServer.cs:2566 [v2.10.3.13]`. Reads S-meter signal/avg/peak-bin per slice from RadioModel via queued call, formats as `rx_sensors:0,-95.3;` per `TCIServer.cs:2316` and `rx_channel_sensors_ex:0,0,-95.3,-97.1,-93.2;` per `TCIServer.cs:2321`. Also `rx_channel_sensors:` legacy form per Sweep E.

**TX timer** at 200ms default per `TCIServer.cs:2581 [v2.10.3.13]`. Reads mic/fwd/peak-fwd/SWR. Format `tx_sensors:0,-30.0,12.5,12.5,1.10;` per `TCIServer.cs:2326`. Duplicated per TRX index 0,1 from a single reading set per Sweep E.

**Server-wide interval aggregation**: effective interval is the minimum any client requests (`MinimumRequiredRxSensorInterval` per Sweep E). Configurable in setup page; surfaced visibly in setup-page note.

### 3.7 Spot hooks (Phase 3J-2 stub)

`TciProtocol` owns `SpotsModel* m_spots = nullptr` placeholder. Four handlers stubbed (`spot_add`, `spot_remove`, `spot_clear`, `spot_simulate_click`) returning `ok`. Phase 3J-2 fills model and handler bodies.

---

## 4. Error handling

### 4.1 Zero wire-visible error frames (Sweep B mandatory revision)

**Thetis emits no error frames on the wire.** Silence IS the rejection signal. Six silent failure classes per Sweep B: unknown command (A), per-handler validation (B), connection/upgrade/bind (C), TX audio mutex (D), per-frame write (E), send-thread queue (F).

Our port follows this rule exactly. Adding error responses would break ESDR3 / SunSDR / WSJT-X clients that expect silence. All errors go to `qCWarning(lcTci)` local log only.

### 4.2 Parity violations

Treated as ship-blocker per user mandate.

- **Unit tests** assert byte-for-byte format strings against captured Thetis output
- **Verification matrix** runs every command in Thetis dispatch tables (62 unique commands across 2 switches, ~100 matrix rows) end-to-end and diffs response
- **CI gate** fails the PR if any matrix row regresses
- Recovery: there is no recovery in code. Failure means PR doesn't merge

Matrix lives at `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`; row-by-row sign-off.

### 4.3 Connection lifecycle

- **Port already in use**: `TciServer::start()` returns false. CatTciServerPage shows red status text. User edits port, retries.
- **Non-WebSocket client connects**: `QWebSocketServer` rejects during HTTP upgrade. Logged at debug level.
- **Client clean disconnect**: tear down session: drop IQ subs, drop audio subs, free per-(client, slice) WDSP resamplers, clear `m_txAudioActiveClient` if it was this client.
- **Client unclean disconnect** (TCP RST, network drop): Qt6 surfaces via `disconnected()` signal eventually (default 30s); same teardown.

### 4.4 Audio backpressure

Three priority send queues per client (Urgent / Binary / Control+Coalesced) per `TCIServer.cs:769-774 [v2.10.3.13]`. Drain order: Urgent first.

- **RX audio + RX IQ**: bounded queue depth, oldest-drop on overflow. Drop counter exposed in ClientChainApplet ("12 frames dropped").
- **TX audio busy**: silent drop of the second client's frames per locked Decision 4.

### 4.5 CW stub responses (Phase 3J-1 only)

CW TX wires up in 3M-2. Stubs return `ok` and log:

- `cw_macros_speed_up` / `cw_macros_speed_down`: stub returns `ok`, emits notification with unchanged speed value
- `cw_msg`: stub returns `ok`, logs at info level

Each stub gets a `// TODO: 3M-2` comment + tracking issue. CWLUbecomesCW remains wired in Phase 3J-1 because it affects mode reporting strings, not CW keying.

### 4.6 Startup ordering

- TciServer starts after RadioModel construction, before connection
- Persistence: `TciServerEnabled` / `TciServerPort` from AppSettings on startup
- Hot reconfigure: changing port stops + restarts on new port; existing clients drop and reconnect

### 4.7 What we do NOT do

- No exceptions thrown anywhere
- No retries on TCI side for client failures (clients reconnect themselves)
- No silent fallbacks (every error returns to client OR logs visibly)
- No mutexes in the audio callback (lock-free SPSC ring depth handles backpressure)

---

## 5. Testing strategy

### 5.1 Verification matrix

Source of truth for parity. ~100 rows: 60 set commands + 21 query commands + ~13 dual-mode + binary frames + sensors + init burst + compat flag combinations. Per-row: command name, Thetis source line, sample input, expected response (byte-for-byte), expected notifications, manual sign-off.

Lives at `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`. Operator signs off row-by-row with `wscat` or N1MM before Phase 3J-1 ships.

### 5.2 Unit tests (fast, run per-handler-family TDD)

Per project rule "minimize redundant test invocations": TDD is 2x ctest of NEW test only; pre-commit hooks run verifiers; full suite once at epic end.

Suggested split (~12 to 15 ctest binaries):

- `test_tci_protocol_dispatch_set` — 60 set commands
- `test_tci_protocol_dispatch_query` — 21 query commands
- `test_tci_protocol_init_burst` — 98-line init burst byte-for-byte
- `test_tci_protocol_compat_flags` — 12 compat flags toggling wire output
- `test_tci_protocol_volume_db` — `TCIServer.cs:4110-4132` math
- `test_tci_protocol_mode_uppercase` — `TCIServer.cs:2155` uppercase enforcement
- `test_tci_protocol_slice_to_trx` — Slice A/B/C/D → trx:0/1/2/3 mapping
- `test_tci_protocol_cw_stubs` — CW stubs return ok
- `test_tci_protocol_sensors` — 4 wire formats + interval aggregation
- `test_tci_server_priority_queues` — 3-queue drain order
- `test_tci_server_vfo_coalescing` — 3-layer coalesce drops correctly
- `test_tci_server_tx_audio_mutex` — single-client enforcement + silent drop
- `test_tci_audio_ring` — SPSC ring stress (1M samples, no drops)
- `test_tci_resampler_lifecycle` — per-(client, slice) create/destroy
- `test_tci_silent_error_invariant` — no wire-visible error frames

### 5.3 Init burst golden file (CI gate)

98-line init burst captured once from Thetis against ANAN-G2 single-RX baseline. Committed at `tests/data/tci/init_burst_anan_g2_rx1.txt`. CI asserts our output matches modulo radio-identifying fields (MAC, FPGA version, model name).

**Includes the init burst typo fix divergence**: our golden file has `if:1,0,...;` where Thetis sends a duplicate `if:1,1,...;`. Documented in matrix as "divergence (typo fix)" row.

### 5.4 Threading and ring-buffer tests

`test_tci_audio_ring`, `test_tci_audio_drain_timer`, `test_tci_backpressure`. Run in CI but not on every save.

### 5.5 Integration tests with real clients (manual, end-of-epic)

Manual checklist in matrix README:

- **N1MM Logger+** (Windows, primary): connect, retune via spot click, mode change, MOX assert
- **Log4OM** (Windows): retune, mode, sensor display
- **RUMlog-TCI** (macOS): retune, mode, basic sensor
- **WSJT-X** (Linux, if it grows TCI mode by ship time): rig control via TCI

Pass each before Phase 3J-1 merges.

### 5.6 Pre-commit hooks (every commit)

Existing: `verify-thetis-headers.py`, `verify-inline-tag-preservation.py`, `check-new-ports.py`. No new TCI-specific hooks needed.

---

## 6. PR plan + sequencing

### 6.1 Phase 3J-1: TCI from Thetis (this design)

**Scope**: Full Thetis TCI parity. Hybrid transport (AetherSDR) + protocol (Thetis-faithful). New TciApplet + ClientChainApplet default-enabled. CatTciServerPage rewritten. Banner status badge + menu integration. Init burst golden file in CI. Verification matrix signed off.

**Exit criteria** (all hard requirements):

- Verification matrix all rows signed off
- Init burst golden test green in CI
- All 12 to 15 unit ctest binaries green
- Integration: N1MM, Log4OM, RUMlog-TCI all pass manual smoke
- No regressions in existing ctest suite
- Every commit GPG signed
- All ported files carry verbatim Thetis headers + inline attribution
- New PROVENANCE.md rows for every new ported file
- CHANGELOG.md updated with parity claim + 3M-2 follow-up note for CW
- macOS, Linux, Windows CI all green

**Hooks pre-created** for downstream PRs:
- `TciProtocol::m_spots = nullptr` placeholder for Phase 3J-2
- Spot handlers stubbed (`spot_add`, `spot_remove`, `spot_clear`, `spot_simulate_click`) returning `ok`
- `SpotData` POD struct defined in TciProtocol.h
- `// TODO: 3M-2` comments on every CW stub
- ClientChainApplet "Spots: 0" row that lights up when `m_spots != nullptr`

**Estimated size**: ~6,500 LOC new + ~700 LOC edits.

### 6.2 Phase 3J-2: AetherSDR Spots port (deferred)

DX cluster + RBN clients + spot overlay on spectrum. Fills `SpotsModel` placeholder. Wires the 4 spot TCI handlers Phase 3J-1 stubbed. Own brainstorm + design + plan cycle.

### 6.3 Phase 3K-1: CAT TCP / rigctld

4-channel rigctld TCP server. FlexRadio-dialect CAT command set. 127.0.0.1:4532 default.

### 6.4 Phase 3K-2: CAT serial / PTY

Virtual PTY pairs (Linux/macOS) + Windows com0com. Same parser as Phase 3K-1 reused.

### 6.5 Phase 3K-3: MIDI

MIDI control surface bindings. Mirrors Thetis MIDI2Cat user model.

### 6.6 Order summary

| PR | Title | Blocked by |
|---|---|---|
| A | TCI from Thetis | nothing |
| A2 | Spots overlay + cluster/RBN | A |
| B | CAT TCP rigctld | A (CatApplet stripped) |
| C | CAT serial / PTY | B (CAT parser reused) |
| D | MIDI | independent |

---

## 7. Divergence ledger

Every place we deliberately deviate from Thetis bug-for-bug parity. Future maintainers reference this when verifying behavior.

| # | Divergence | Source | Rationale | Risk |
|---|---|---|---|---|
| 1 | **Init burst typo fix** at `TCIServer.cs:2374-2375 [v2.10.3.13]` | We send `if:1,0,...; if:1,1,...;` (the intended cross-product); Thetis sends duplicate `if:1,1,...; if:1,1,...;` | Sweep verification: third `sendIF(1,1)` is a copy-paste bug; sendVFO immediately below enumerates the full `(0,0)(0,1)(1,0)(1,1)` cross-product correctly | Low: clients should be idempotent on `if:` updates; the missing `(1,0)` IF is a useful addition |
| 2 | **WebSocket transport** uses Qt6 `QWebSocketServer` | Thetis hand-rolls RFC 6455 framing at `TCIServer.cs:1499-1596 + 2976-3072 [v2.10.3.13]` because the C# project has no WS library | Qt6 is RFC-correct; Thetis's framing is a constraint workaround, not a behavioral choice | None: clients see WS frames, not how server constructs them |
| 3 | **Slice A/B/C/D ↔ trx:N mapping** | Thetis hardcodes RX1/RX2 throughout | NereusSDR architecture moves to pan + slice for multi-pan multi-slice (see `project_nereussdr_slice_architecture.md`) | None: wire format keeps strict Thetis parity (`trx_count:2;` hardcoded); UI labels diverge |
| 4 | **Outbound text encoding** UTF-8 on all platforms | Thetis uses `Encoding.Default` outbound at `TCIServer.cs:1502 [v2.10.3.13]` (Windows-1252 on Windows, UTF-8 on Linux/macOS); inbound is `Encoding.UTF8` at `TCIServer.cs:3056 [v2.10.3.13]` (asymmetric) | Thetis behavior is non-portable; NereusSDR is cross-platform-first; ASCII content (>99% of wire) is byte-identical | Very low: only non-ASCII bytes (callsigns with diacritics, unusual radio model strings) differ |
| 5 | **Single-class Client status widget** instead of AetherSDR's split | AetherSDR has `ClientChainApplet` + `ClientChainWidget` (two classes, 138+794 LOC) | Single-class collapse is sufficient for our scope; reduces test surface | None |
| 6 | **`MinimumRequiredRxSensorInterval` aggregation surfaced** in Setup page | Thetis aggregates internally without UI surface | Operator transparency; helps debug "why is my client getting slower updates" | None: behavior identical; just adds a UI note |
| 7 | **Broadcast notifications to all clients** | `TciServer::onTextMessageReceived` drain loop and `m_pendingNotifications` design in `TciProtocol.cpp` | Thetis `sendTextFrame` on the per-listener class (`TCIServer.cs:2812-2828 [v2.10.3.13]`) sends notifications only to the originating client; NereusSDR broadcasts to all connected clients so multi-client setups (logger + digital-mode app + UI) see state changes from each other | Low: changes observable wire pattern but improves multi-client UX; clients that set state must tolerate receiving their own change echoed back |

---

## 8. UI design

Mockups at `.superpowers/brainstorm/13064-1778362843/content/`:

- `tci-setup-page.html` — Setup → Network → TCI Server
- `tci-applet-v2.html` — TciApplet (Slice A nomenclature)
- `client-chain-applet.html` — ClientChainApplet
- `banner-menus.html` — Title bar TCI badge, View → Network Applets, Tools → TCI Server

### 8.1 Setup page layout

Six group boxes 1:1 with Thetis `grpTCIServer`: Server / Compatibility / IQ Stream / Audio Stream / Sensors / VFO Quirks. Spots placeholder at bottom signals Phase 3J-2 scope. Bind read-only at `127.0.0.1` per locked Q7.

### 8.2 TciApplet

Header row: status dot + "TCI Server" + port (read-only display) + client count + Setup button. Slice A row + TX row, each with level meter and `[-60, 0]` dB gain slider. Footer: client count + "Show clients →" link to ClientChainApplet. Disabled state: collapsed to single "Enable Server" button. Designed to scale to Slice B/C/D when 3F lands.

### 8.3 ClientChainApplet

Per-client rows with TX badge, peer + name, subscription badges, last command + age, drop counter, disconnect button. Empty state shows bind address + connection hint. Auto-refresh checkbox at the top.

### 8.4 Banner & menus

- **Bottom status-bar TCI indicator** (existing `m_tciIndicator` @ MainWindow.cpp:3179): live-wired from TciServer signals. Bottom label format `Off` / `On` / `On · N` / `On · N ▸TX` with color discipline matching PsaIndicator (dim/green/cyan/orange). Tooltip shows bind/clients/TX-source. Click opens Setup → Network → TCI Server. Drop-priority pairs with CAT in narrow-window collapse (existing logic at :4318).
- **View → Network Applets ▶** submenu: TCI Server + TCI Clients toggles, both default-enabled. CAT/MIDI items greyed for Phase 3K-1/2/3 placeholders. Replaces the commented-out `addContainerToggle("CAT / TCI", …)` at :2795.
- **Tools → TCI Server…** action (existing `tciAction` @ :2848): currently disabled with "NYI — Phase 3J" tooltip; Phase 3J-1 enables it and wires the click to Setup → Network → TCI Server jump.

Mockup reference: `banner-bottom-existing.html` (4 live states + scaffolding-mapping table).

---

## 9. Open follow-ups

Non-blocking. Tracked here for plan-stage discussion or post-Phase 3J-1 enhancement.

- Per-slice subscription badges in TciApplet rows (e.g. "IQ:2 Audio:1") — currently deferred to ClientChainApplet detail
- Advanced-user toggle to opt into `trx_count:4;` in init burst (exposes Slice C/D over TCI; default off for client compat)
- Future client-self-id command / WS subprotocol negotiation (Thetis has no equivalent; would be a NereusSDR extension)
- Performance benchmarks (max clients, max msg rate) — deferred until anyone reports issues
- HTTP upgrade authentication (we ship loopback-only no-auth per Q7; remote-LAN deployment is a future epic)

---

## 10. Appendix: AppSettings keys introduced by Phase 3J-1

All keys persisted to `~/.config/NereusSDR/NereusSDR.settings` via `AppSettings` (NOT `QSettings`). PascalCase per project convention.

| Key | Type | Default | Notes |
|---|---|---|---|
| `TciServerEnabled` | bool | False | Server on/off |
| `TciServerPort` | int | 50001 | Bind port (bind addr always 127.0.0.1) |
| `TciSendInitialFrequencyStateOnConnect` | bool | True | Sends VFO/IF/DDS in init burst |
| `TciRateLimitMsgsPerSec` | int | 60 | Per-client message rate cap (0 = unlimited) |
| `TciAudioStreamSamples` | int | 2048 | Audio block size [100..2048] |
| `TciTxChannel` | string | "Both" | TX audio channel: Left / Right / Both |
| `TciRxSensorIntervalMs` | int | 200 | RX sensor push interval [30..1000] |
| `TciTxSensorIntervalMs` | int | 200 | TX sensor push interval [30..1000] |
| `TciEmulateExpertSDR3Protocol` | bool | False | Compat flag |
| `TciEmulateSunSDR2Pro` | bool | False | Compat flag |
| `TciCwluBecomesCw` | bool | False | Compat flag |
| `TciCwBecomesCwuAbove10mhz` | bool | False | Compat flag (W2PA fix for issue #559) |
| `TciIqSwap` | bool | True | Compat flag |
| `TciAlwaysStreamIq` | bool | False | Compat flag |
| `TciForgetRx2VfoBOnDisconnect` | bool | False | VFO quirk |
| `TciUseRx1VfoaForRx2Vfoa` | bool | False | VFO quirk |
| `TciCopyRx2VfobToVfoa` | bool | False | VFO quirk |

---

## 11. References

- Thetis TCI source: `../Thetis/Project Files/Source/Console/TCIServer.cs` at `v2.10.3.13 @501e3f51`
- AetherSDR transport reference: `../AetherSDR/src/core/TciServer.{h,cpp}` and `TciProtocol.{h,cpp}` at `@0cd4559`
- WDSP resampler: `third_party/wdsp/resample.c`
- Sweep reports (research artifacts, not committed): `/tmp/tci-sweep-{A,B,C,D,E}.md`
- Project rules: `CLAUDE.md` source-first protocol; `CONTRIBUTING.md` coding conventions
- Slice architecture memory: `~/.claude/projects/-Users-j-j-boyd-NereusSDR/memory/project_nereussdr_slice_architecture.md`

---

*End of design doc. Implementation plan to follow via `superpowers:writing-plans` skill.*
