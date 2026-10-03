# Remote display limits implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** close the open items of R3 plan sections 2 and 3: the Core decides
and reports what spectrum resolution each pan actually gets, never disturbs
another pan's spectrum to satisfy a request, and never lets stale display data
pile up in the transport.

**Architecture:** Core keeps the existing request contract (the GUI derives an
FFT size from its resolution target and sends it). Core enforces tier and
sharing rules in `DaemonMediaController`, records a per-endpoint grant, and,
for peers that negotiate protocol minor 9, reports the grant in the spectrum
context through a shared codec. The display data channel gets explicit SCTP
buffer limits applied before the first peer, so a slow link drops display
frames (latest value wins) instead of delaying them, and the Core logs real
frame sizes and fragment counts for the hardware check. Capacity refusal and
downgrade (R-R3-08's CPU side) is a separate plan: it needs a Rock
measurement and an operator decision first.

**Tech stack:** C++20, Qt 6, `FftEnginePool`, `DaemonSpectrumSource`,
`SpectrumEndpoint`, `DaemonMediaController`, `RemoteMediaController`,
libdatachannel v0.24.5 over usrsctp (`cmake/NereusRemoteMedia.cmake:29-58`),
Qt Test.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) section 2 (lines
270-310, requirements R-R3-01, 04, 08, 09) and section 3 (lines 312-345,
R-R3-03, 04, 05, 09); [architecture design](2026-07-28-remote-daemon-architecture-design.md)
section 9.4 ("Latest-value-wins at the producer, never a queue"); the
[display codec v1 spec](2026-09-20-display-codec-v1.md).

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- NereusSDR-original code; no Thetis port involved. Keep existing file
  headers; new files carry the house header plus
  `// no-port-check: NereusSDR-original. <reason>`.
- `src/core` stays GUI-free (`tst_core_has_no_gui_includes`).
- Protocol additions are gated by negotiated minor and a capability version,
  following the remote audio status pattern (`kRemoteAudioStatusSessionProtocolMinor`,
  `remoteAudioStatusVersion`, `RemoteAudioContext`). A minor 8 or older peer
  receives today's spectrum context byte for byte (19 keys, 20 with wideband).
- No change to the display codec v1 byte layout, the FFT window, detector or
  averaging formulas, the audio path, Opus settings, or thread priorities.
- Operator strings are plain English, in the style of the existing pan status
  lines (`RemoteMediaController.cpp:1283-1365`), with no protocol words.
- Tests: build exact targets, run
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  Record load averages with timing. No unfiltered suite. Never build the
  `NereusSDR` target (the controller builds the app at gates). Hardware (Rock,
  radio, running GUI) is off limits to implementers.

## What already exists

Read-only investigation, 2026-09-22 (saved scout notes; paths in the checkout):
- `FftEnginePool` keys engines by `FftSourceKey{streamIndex, FftTier}`
  (`src/core/spectrum/FftEnginePool.h:43-66`); supported FFT sizes are powers
  of two from 1024 to 262144 (`src/core/FFTEngine.cpp:105-112`). All daemon
  display engines share one thread (`FftEnginePool.cpp:141-151`).
- Endpoints on the same (stream, tier) share one engine sized to the largest
  requested FFT size and frame rate, and Core trusts the client's `tier` label
  (`src/core/session/media/DaemonMediaController.cpp:1521-1574`); a change to
  the shared configuration renews every endpoint on that key.
- Subscription validation (`DaemonMediaController.cpp:725-879`): framesPerLine
  1..65535 and any finite dBm window, while the GUI rejects contexts with
  framesPerLine above 10000 or dBm outside [-400, 100]
  (`src/gui/RemoteMediaController.cpp:1972-1975`). `SpectrumEndpoint::configure`
  accepts a crop that does not overlap the source and gives it zero span
  (`SpectrumEndpoint.cpp:192-201`).
- The pixel clamp `min(requested, visible bins, 4096)` exists but is applied at
  the first frame and reported only through `traceSamples`
  (`SpectrumEndpoint.cpp:166-178`; `DaemonMediaController.cpp:1148-1163,
  1228-1229`). The context is built inline in `sendContext` (1209-1238) and
  the GUI parses it inline, requiring exactly 19 or 20 keys
  (`RemoteMediaController.cpp:1930-1933`).
- The Core frame path already keeps one latest slot per source and at most one
  queued `frameAvailable(key)` per source (`DaemonSpectrumSource.cpp:422-456`);
  no test pins the coalescing.
- Display channel: `display`, unordered, `maxRetransmits` 0, mtu 1000,
  maxMessageSize 64 KiB (`LibDataChannelMediaTransport.cpp:32, 268-272,
  370-375`). usrsctp holds up to 1 MiB of send data before
  `bufferedAmount()` rises, so the Core's "refuse while bufferedAmount != 0"
  (`LibDataChannelMediaTransport.cpp:540-546`) misses up to about 20 s of
  stale display at measured rates. `rtc::SetSctpSettings`,
  `setBufferedAmountLowThreshold` and `onBufferedAmountLow` are unused. The
  Core never connects the transport's `errorOccurred`
  (`DaemonMediaController.cpp:681-699`). The GUI transport keeps at most 8
  messages or 256 KiB and drops the oldest without counting
  (`LibDataChannelMediaTransport.cpp:36-37, 123-132`).
- Frame sizes: plane cost A(n) = 3 + 5*ceil(n/128) + n; header 42 bytes;
  largest valid frame 4096/4096/768 = 9,361 bytes (pinned in
  `tests/tst_display_budget.cpp:80-93`). SCTP carries 876 payload bytes per
  DATA chunk (path MTU 892 from `sctptransport.cpp:243-249`, plus 12 in
  `usrsctp sctp_pcb.c:4465-4481`, minus the 12-byte common header and 16-byte
  DATA header), so that frame is 11 fragments; a 64 KiB PureSignal chunk is 75.

## Task 1: Core tier and sharing rules, aligned validation, pinned coalescing

**Requirements:** R-R3-01, R-R3-08, R-R3-09.

**Files:**
- Modify: `src/core/session/media/DaemonMediaController.{h,cpp}`,
  `src/core/session/media/DaemonSpectrumSource.{h,cpp}` (only if the grant
  needs source support), `src/core/session/media/SpectrumEndpoint.{h,cpp}`
  (shared limit constants, crop rejection), `src/gui/RemoteMediaController.cpp`
  (use the shared constants), `src/core/daemon/DaemonApp.h` (the stale comment
  at lines 41-54 that says no pool is constructed)
- Test: `tests/tst_remote_fft_production.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_spectrum_endpoint.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `struct SpectrumGrant { int requestedFftSize; int grantedFftSize;
  FftTier grantedTier; int requestedPixels; int grantedPixels;
  SpectrumLimitReason reason; }` and
  `enum class SpectrumLimitReason { None, LargestSize, SharedEngine, SourceBins }`
  in `SpectrumEndpoint.h`; `DaemonMediaController` keeps the current grant per
  endpoint (read by Task 2). Shared limits in `SpectrumEndpoint.h`:
  `kMaxFramesPerLine = 10000`, `kMinDbmLimit = -400.0`, `kMaxDbmLimit = 100.0`
  (the GUI's current accepted-context rules), used by both sides.

**Acceptance:**
- A request may change the size of its (stream, tier) engine only when that
  endpoint is the engine's only subscriber. While the engine serves another
  endpoint, the request is granted the engine's current size; when that is
  smaller than requested, the grant's reason is `SharedEngine`. So a request
  labelled "wide" with a larger size never lengthens a Wide engine another pan
  uses, and a Fine request never touches the Wide engine.
- Two endpoints on one stream (E1 wide, E2 fine): E1's bin count and context
  generation do not change while E2 subscribes, resizes or unsubscribes.
- A requested size above the largest supported size is granted the largest
  size with reason `LargestSize` (today's validation rejects it; the GUI never
  sends it, so this is Core robustness).
- The pixel grant `min(requested, visible bins, 4096)` is computed when the
  source geometry is known and stored in the grant with reason `SourceBins`
  when it reduces the request; extended view keeps its current rule. The
  display budget charges the granted pixels, not the requested ones.
- Validation: framesPerLine above 10000, dBm limits outside [-400, 100], and a
  crop that does not overlap the source are rejected with the existing typed
  rejection path, leaving any existing endpoint and its source untouched.
  Existing rejections (fps 0 or 61, pixels 0 or 4097, span <= 0, fftSize 3000,
  numbers sent as strings, minDbm == maxDbm) keep passing.
- Ownership stays as today and is pinned: a subscription from a stale epoch or
  a connectionId that does not match the session's peer is dropped without
  touching existing endpoints (add the test if none pins it).
- Coalescing test: ten frames produced while the owner thread is blocked
  produce exactly one `frameAvailable`; `takeLatest` returns the newest and a
  second call returns nothing.
- `tst_fft_engine_pool`, `tst_auto_zoom_fft` and the existing controller tests
  stay green.

**Verification:** consequential state transitions: the sharing regression
(E1 unchanged while E2 churns) fails first on the current tree, then passes.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_fft_production tst_daemon_media_controller tst_spectrum_endpoint tst_fft_engine_pool tst_auto_zoom_fft tst_remote_media_controller tst_display_budget_contract -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_fft_production|tst_daemon_media_controller|tst_spectrum_endpoint|tst_fft_engine_pool|tst_auto_zoom_fft|tst_remote_media_controller|tst_display_budget_contract)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Add the sharing regression and the coalescing test; confirm
  the sharing test fails on the current tree.
- [ ] **Step 2:** Implement the grant, the sharing rule, the shared limits and
  the crop rejection; run the commands; commit.

## Task 2: Report the grant to the GUI (protocol minor 9)

**Requirements:** R-R3-01, R-R3-04, R-R3-08, R-R3-09.

**Files:**
- Create: `src/core/session/media/RemoteSpectrumContext.{h,cpp}` (shared
  encode/decode of the spectrum context, both shapes)
- Modify: `src/core/session/SessionMessages.h` (`kSessionProtocolMinor = 9`,
  `kRemoteSpectrumGrantSessionProtocolMinor = 9`),
  `src/core/session/StationCapabilities.{h,cpp}` (`spectrumGrantVersion = 1`),
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.cpp`
  (availability like `remoteAudioStatusAvailable()`),
  `src/core/session/media/DaemonMediaController.cpp` (`sendContext` uses the
  codec), `src/gui/RemoteMediaController.{h,cpp}` (decode through the codec,
  pan status), `docs/architecture/2026-09-20-remote-media-control-v1.md` (the
  context fields), `CMakeLists.txt` (register the new source)
- Test: new `tests/tst_remote_spectrum_context.cpp`; update
  `tests/tst_daemon_media_controller.cpp`, `tests/tst_remote_media_controller.cpp`,
  `tests/tst_display_budget_contract.cpp` (minor 9), `tests/tst_station_session.cpp`
  (negotiation, like `remoteAudioStatusVersion`)

**Interfaces:**
- Consumes: Task 1 `SpectrumGrant`, `SpectrumLimitReason`.
- Produces: `QJsonObject encodeRemoteSpectrumContext(const SpectrumContextMessage&, bool grantNegotiated)`
  and `std::optional<SpectrumContextMessage> decodeRemoteSpectrumContext(const QJsonObject&, bool grantNegotiated)`;
  `SpectrumContextMessage` carries today's 19/20 fields plus, when negotiated,
  `grantedFftSize`, `grantedTier` ("wide" or "fine"), `requestedPixels`,
  `grantedPixels`, `limit` ("none", "largest-size", "shared", "source-bins").

**Acceptance:**
- A minor 9 peer with `spectrumGrantVersion >= 1` receives the extended
  context; a minor 8 or older peer receives exactly today's keys and values
  (golden comparison against the current inline builder). The GUI decoder
  accepts exactly the shape it negotiated and rejects the other.
- Round trip, each limit value, unknown limit strings, missing or extra keys,
  and non-integral numbers are covered in the codec test.
- The GUI shows, beside the existing pan status, one plain line only while a
  grant is limited: `largest-size` "Zoom detail is at the station's maximum";
  `shared` "Zoom detail limited: this receiver's spectrum is shared with
  another pan"; `source-bins` "Showing %1 points: the receiver has no finer
  detail here". Nothing is shown for `none`. The status clears when the grant
  is no longer limited or the endpoint goes away.
- New GUI with old Core and old GUI with new Core both keep working (tests
  with a minor 8 peer on each side).

**Verification:** wire compatibility, consequential: golden legacy shape plus
both mixed-version directions.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_spectrum_context tst_daemon_media_controller tst_remote_media_controller tst_display_budget_contract tst_station_session nereusd -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_spectrum_context|tst_daemon_media_controller|tst_remote_media_controller|tst_display_budget_contract|tst_station_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator): a deep zoom on the Rock shows the grant in the
GUI log and, when limited, the status line.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Codec with golden legacy test.
- [ ] **Step 2:** Negotiation, Core send, GUI decode and status; run the
  commands; commit.

## Task 3: Bound the display transport and measure real frames

**Requirements:** R-R3-03, R-R3-04, R-R3-05, R-R3-09.

**Files:**
- Modify: `src/core/session/media/LibDataChannelMediaTransport.{h,cpp}`,
  `src/core/session/media/IMediaTransport.h`, `src/core/session/media/MediaPeer.cpp`
  (only if the settings seam lives there), `src/core/session/media/DaemonMediaController.{h,cpp}`,
  `src/gui/RemoteMediaController.cpp` (display drop counter in diagnostics),
  `docs/architecture/2026-09-20-display-codec-v1.md` (measured sizes and the
  fragment rule), `docs/architecture/2026-09-20-remote-daemon-r3-verification/display-capacity.md`
- Test: `tests/tst_media_transport.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_display_budget.cpp`

**Interfaces:**
- Consumes: nothing from Tasks 1-2 (independent files; runs after them only
  to keep one implementer in the tree).
- Produces: named constants `kSctpSendBufferBytes = 65536` (the floor set by
  the 64 KiB maximum message size that PureSignal chunks need),
  `kSctpReceiveBufferBytes = 131072` (twice the maximum message, so usrsctp
  delivers every message whole), `kSctpDataPayloadBytes = 876` (with its
  derivation in a comment); a process-wide `applyMediaSctpSettingsOnce()`
  called before the first peer on both sides; telemetry fields
  `displayMaxKeyframeBytes`, `displayMaxDeltaBytes`, `displayMaxFragments`,
  `displaySendRefusals`, `displayTransportErrors` (Core) and
  `displayMessagesDropped` (GUI).

**Acceptance:**
- The SCTP settings are applied exactly once, before the first peer, in both
  processes (seam test). A 64 KiB message still crosses the real encrypted
  loopback; a 9,361-byte frame arrives whole.
- With the send side stalled (test seam that stops the receiver draining),
  the Core refuses new display frames instead of queueing them once the
  bounded buffer fills, counts each refusal, forces a keyframe on the next
  successful send, and never holds more than the configured buffer plus one
  message.
- The Core connects the transport's `errorOccurred`: it logs once per error
  kind, counts it, and treats it as a failed send.
- The GUI drop rule is unchanged (8 messages or 256 KiB, oldest first) and
  now counts each dropped message separately from received bytes.
- The periodic Core diagnostics line reports the largest keyframe and delta
  actually sent and their fragment count `ceil(bytes / 876)`: a
  1024/1024/768 keyframe logs 2,977 bytes and 4 fragments; 4096/4096/768 logs
  9,361 bytes and 11 fragments. A property test checks every keyframe equals
  `42 + 2*A(pixels) + A(wide)` and every delta is no larger. Counters reset
  with a new session.
- The codec spec and the display-capacity evidence file record the measured
  rule and the new limits.

**Verification:** real-time transport path, consequential: loopback tests
with the real library; the stalled-receiver test fails first on the current
tree (unbounded backlog), then passes.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_media_transport tst_media_peer tst_daemon_media_controller tst_display_budget tst_display_codec tst_remote_media_controller nereusd -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_media_transport|tst_media_peer|tst_daemon_media_controller|tst_display_budget|tst_display_codec|tst_remote_media_controller)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller and operator): on the Rock at 4096 pixels,
60 fps with 3D, the journal shows a largest frame of at most 9,361 bytes, and
a packet capture shows every media datagram at 1000 bytes or less (969 on
IPv4).

**Execution note (advisory):** opus.

- [ ] **Step 1:** Add the stalled-receiver regression and the size property
  test; confirm the regression fails on the current tree.
- [ ] **Step 2:** Apply the SCTP settings, the refusal accounting, the error
  handling and the counters; update the two documents; run the commands;
  commit.
