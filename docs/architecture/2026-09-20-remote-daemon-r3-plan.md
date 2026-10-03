# Remote daemon R3: working remote receive

> Execution: yonder-cost-aware-execution. Requirements and acceptance criteria
> are binding; test order and review effort follow the Yonder risk-based
> policy. Reuse approved decisions and valid evidence for unchanged code.

**Goal:** Hear the Saturn through Core on the Rock 5C and render its live
panadapter, 2D waterfall and integrated 3D waterfall in the desktop GUI.

**Architecture:** Core owns the radio, WDSP, shared FFT engines, spectrum
reducers and the mixed stereo encoder. The existing authenticated WSS session
controls a direct encrypted media peer. Opus travels as RTP/SRTP; native
display frames travel on a separate unreliable channel. GUI models and
rendering remain client-side.

**Stack:** C++20, Qt6, existing WDSP/FFTW/Opus, OpenSSL, pinned libdatachannel,
Qt Test, CMake/Ninja. Variable clock correction requires the source validation
in task 1; the fixed-ratio Resampler is not its implementation.

**Specifications:** [umbrella](2026-07-28-remote-daemon-architecture-design.md),
[R2/R3 addendum](2026-08-03-remote-daemon-r2-r3-design-addendum.md),
[identity and network design](2026-08-02-remote-station-identity-and-pairing-design.md),
[current review and decisions](2026-09-20-remote-daemon-r3-review.md),
[September 21 controls/accessory audit](2026-09-21-remote-gui-control-gap-audit.md).
This plan supersedes procedural boilerplate in older plans, not their
substantive safety or acceptance requirements.

This is the next phase of Claude's original plan, expanded against recovered
code and current hardware. The source-document and conversation map is in
the review. Preserve the network addendum's raced connection attempts,
UDP/TLS relay paths, dual-stack identity/pairing and carrier measurement gates.
This R3 plan validates direct media; it makes no carrier-path or relay claim.

## What completion gives the operator

The R3 deliverable is a usable split receiver: the Rock 5C boots Core, owns
the Saturn connection and receive DSP, and serves the existing NereusSDR GUI
on the Mac. The GUI tunes and controls the station, shows live spectrum and
2D/3D waterfalls, and plays one mixed stereo Opus feed with the existing
slice volume, mute and left/right placement controls. It includes the
published open-PR changes integrated into this branch, including 3D waterfall.
Their presence in the build does not make remote transmit an R3 feature.

| Checkpoint | Observable result | Completion evidence |
| --- | --- | --- |
| First display | A live Saturn spectrum and advancing 2D/3D waterfall in the remote GUI | Real board-to-Mac session; tuning and zoom remain aligned |
| First sound | The same session plays mixed stereo Opus with working slice controls | Actual listening plus two-slice mix and loss/clock tests |
| R3 accepted | Multiple pans and slices remain usable through reconnect and sustained reception; connection, audio and receive-safe accessory controls show accepted station state | Four-pan measurements, two-hour audio run, install/boot/reconnect checks, visible-control and accessory acceptance |
| R4 | Remote microphone/PTT and transmit operation | TX chain and watchdog/starvation/handoff safety acceptance together |
| R5 | Remote access across difficult internet connections without a required VPN | Direct/relay racing, dual-stack and CGNAT-to-CGNAT evidence |
| R6 | Routine installation and use without development-session help | Remaining pairing/administration UX, reachability diagnostics, packaging and documentation; basic station selection, connection controls and visible state are now R3 requirements |

R1/R2 establish the split and control foundation. They are not a completed
remote receiver. Codec tests, dependency probes and successful builds are
supporting evidence; no R3 checkpoint is complete until its operator-visible
result is demonstrated. There is no separate executable named NereusUI yet;
this plan uses the existing NereusSDR executable in remote station mode.

**Current remaining order, September 22 (operator clarification):** the goal
includes receive, transmit, traversal, station selection and standalone desktop
operation. The operator's question about TX timing was not a request to jump
phases. Preserve the saved R5-before-R4 sequence; any future ordering change
must be explicit. Bringing basic connection selection forward is the operator's
new priority, not a substitution for the full plan.

1. **Finish the current display checkpoint.** Signed source `fc129a39` contains
   R-R3-37; matching GUI/Core and all-tests build passed, with 730/730 CTest
   executables passing. Runtime limits and deployment acceptance remain pending.
2. **Implement the Core/radio and local-radio selector (new task 4g).** Promote
   the basic selection flow from R6, preserving the identity design's three
   groups and the desktop's self-contained local capability. Complete its
   design and lifecycle tests before replacing the current single-Core panel.
3. **Close the remaining R3 behavior and operator checks:** visible menus and
   connection actions, actual TGXL identity/admission, wideband/3D parity,
   mixed SSB/RADE and Opus health/controls, and optional-capture startup. The
   receive-layout and pan-lifetime software corrections are already implemented;
   their real Core-restart acceptance remains open. New gaps attach to these
   named work items instead of silently displacing them.
4. **Complete R3 capacity and installation acceptance (task 6).** Measure the
   Rock and separate Pi 4 floor, install a matching recoverable checkpoint,
   and verify boot/reconnect plus the two-hour listening run. Pending hardware
   observations do not prohibit independent source work on the roadmap.
5. **R5 traversal, then R4 safeguarded remote TX**, as previously ordered. R4
   includes microphone uplink, Core TX integration, PTT/MOX, unkey-confirmed
   handoff, watchdog, starvation handling and TX timeout together. PR #291's
   old prerequisite is satisfied: merged August 2 at `7e681cc1`, which is an
   ancestor of this integration branch. The green R3 suite does not close
   skipped TX harness/RADE cases; those remain explicit R4 coverage work.
6. **R6 remaining operations:** complete pairing/administration, packaging,
   reachability guidance and documentation. Basic usable selection is step 2,
   rather than being held for this final phase.

**Built after R4, by operator decision (2026-09-23).** NereusSDR keeps its own design: a
Thetis feature is ported when it fits and serves operators, and features built around
Thetis's VFO A/B or RX1/RX2 structure are not forced in. R3 finishes the remote Core
working the way local NereusSDR does, plus cleanup. These features the operator chose to
build come after R5 and R4, each ported as it fits; until then their controls are hidden
(the unfinished controls plan). Their build choices and scout notes are in the
controller's notes (`questions-for-jj-2026-09-23.md`, "BUILD CHOICES RESULTS";
`unfinished-controls-scout-*.md`):
- Receive equalizer in one window with transmit (global across receivers, its own
  setting, with Thetis's bandwidth control for receive and transmit).
- Synchronous AM options and the DC block (per slice; DC block on after the AGC by
  default).
- Container macro buttons (open and close containers, send MMIO messages) and spectrum
  averaging on and off.
- Local connection history, the logging controls, local network diagnostics.
- Meter peak hold, text hold in the number readouts, digital delay and history band,
  starting at Thetis's values.
- Mute VAX during transmit on another slice (on by default), local and remote.
- WSJT-X spot filters (starting ticked) and the RBN rate limit (RBN spots only).
- RADE callsigns decoded and sent in FreeDV's format with the "TX ending" tail (never
  keyed more than 1 s after release), then FreeDV decodes to PSK Reporter (mode FREEDV,
  the Core uploads with a remote radio).
- TCI settings: rate limit per slice (milliseconds, 100 by default), CW to CWU, TX
  channel, sensor interval default, the three receiver 2 VFO options starting at
  today's messages, stream channels.
- The CW peak filter's bandwidth and gain, the AM maximum squelch tail, FM deviation
  and de-emphasis.
- DXCC spot colouring end to end (a switch and a log import; the country table already
  loads).

The matching selector Core/GUI checkpoint `c28e1565` is installed. Its full
desktop suite passed 738/738, the fresh native build passed, and the GUI
authenticated and received audio/spectrum. Actual IPv4 LAN announcements
reached the Mac. Native selector interaction is pending because the Mac was
locked; smooth-audio/soak acceptance also remains open. After an earlier reboot,
empty installation files required restoring the verified `3402d171` Core;
that verified version is the durable rollback for `c28e1565`. Source completion,
deployment and operator acceptance are separate evidence. R4 remains part of the objective;
R3's current TX-disabled runtime is an intermediate boundary, not the product's
final feature scope.

## Network decisions carried forward

The [identity/network design](2026-08-02-remote-station-identity-and-pairing-design.md)
remains authoritative, particularly sections 5.4, 9.5, 9.8 and 13. R3's
direct LAN checkpoint is not a reduction of the internet vision.

| Work | When it enters implementation | Required result |
| --- | --- | --- |
| Encrypted, ICE-capable transport; RTP/display sequencing; bounded queues | R3 now | A media boundary reusable by direct and relayed paths |
| IPv4/IPv6 MTU, mapping behavior/lifetime and uplink measurements | Prepare alongside R3; run on the actual carrier/VPS bench before final R5 transport choices | Evidence for datagram limits, traversal and keepalive policy |
| STUN candidates, connectivity checks and UDP hole punching | R5 receive work | Establish a direct path where NAT/firewall behavior permits it |
| Private rendezvous, authenticated bootstrap and relay for control as well as media | R5 receive work | Neither endpoint needs an inbound public listener or a required third-party VPN |
| Race direct and both relay paths; prefer IPv6/direct and upgrade in the background | R5 receive work | First usable connection starts reception; path changes preserve audio timing |
| DTLS/UDP 443 and TLS/TCP 443 fallback, dual-stack self-hosting | R5 receive work | Proved relay routes including the two-CGNAT case; short-lived credentials and rate/staleness limits |
| Path switches while transmitting | Only with R4's complete TX safeguards | Watchdog, starvation and unkey/handoff acceptance remain mandatory |

Recommended sequencing from this review: implement receive-only R5 traversal
immediately after R3's working LAN receiver, without waiting for all remote
TX work. R4/R5 retain their original scope labels. The carrier bench is still
a prerequisite to settling its unmeasured policy; a LAN pass cannot replace it.

The pinned libdatachannel v0.24.5 `libjuice` backend explicitly rejects
TURN/TCP and TURN/TLS in `src/impl/icetransport.cpp:159-161`. Its optional
`libnice` backend has TURN/TLS support, but has not been selected, packaged or
validated here. The current backend therefore proves only a subset of the
future ladder. Before R5 implementation is considered ready, resolve and test
the missing relay transports; do not silently omit TLS/443. Distinguish
encrypted media carried through TURN/UDP from DTLS on the client-to-relay leg.
The present direct WSS control listener also does not supply a CGNAT bootstrap:
R5 must make authenticated control/signalling reachable along with media.

Existing open items remain open: carrier measurements, exact keepalive policy,
TURN versus a custom relay where needed, seamless-switch mechanics, PAKE
dependency and rendezvous operational ownership. They were not all settled
by the earlier brainstorming, and this review does not present them as such.

## Global constraints and requirement map

| ID | Required behavior |
| --- | --- |
| R-R3-01 | Core produces FFT frames headlessly from actual stream I/Q. Pans stay client-local; endpoints are explicit session-owned subscriptions. |
| R-R3-02 | Authentication, snapshot readiness and the active session generation gate media negotiation and delivery. Preemption, disconnect and reconnect discard the previous peer and all media state. |
| R-R3-03 | Retain WSS control, separate RTP/SRTP Opus audio and unreliable native display media. Actual media UDP datagrams meet the parent 1000-byte cap, including transport overhead. |
| R-R3-04 | Trace and waterfall retain independent detector/averaging settings. Optional bounded 3D coverage supports the integrated renderer without shipping full FFT arrays. |
| R-R3-05 | Codec error is bounded, packet loss cannot silently corrupt a delta chain, and all parser allocations and queues have explicit limits. |
| R-R3-06 | One 48 kHz stereo master is captured after per-slice gain/mute/pan and the mixer barrier, before local speaker gain/mute/device. No network or codec work runs on the DSP callback. |
| R-R3-07 | Audio jitter, loss and independent clocks are handled continuously. Client playback mute flushes queued audio and can suspend encoding without mutating station slice gain. |
| R-R3-08 | Two FFT tiers can coexist per stream, with explicit capacity refusal/downgrade. Four pans share a session-wide display budget; hidden endpoints are explicitly disabled. |
| R-R3-09 | Frequency/context changes, slice ID reuse, layout changes, float/dock and reconnect do not apply stale media or leak endpoints. |
| R-R3-10 | The desktop package retains self-contained local radio/DSP operation when no external Core is available, alongside remote-Core mode. Preserve source attribution and settings ownership. A future bundled local-Core process may change internals but cannot require a separate Core host or manual service setup. Remote receive requires real hardware and long-session evidence. |
| R-R3-11 | Core restores station RF gain controls and produces antenna-calibrated display levels. The remote GUI applies no second local calibration; local direct rendering retains its existing calibration. |
| R-R3-12 | Remote Clarity receives Core's unsmoothed full-source noise-floor estimate before display reduction/quantization, with station calibration applied once. The GUI retains its existing smoothing, deadband, TX/manual-override gates and palette; stale or inactive endpoint measurements cannot drive the active pan. |
| R-R3-13 | Remote applet and slice S-meters select Core's independent calibrated peak/average readings, or the decoded display's passband Max Bin. They follow active slice identity and never poll local DSP or add client calibration. Disconnect clears the live reading, and reconnect waits for the current snapshot; RX telemetry remains read-only. |
| R-R3-14 | Remote CH/BPF/WIDE indicators show Core's effective per-chain filter state and reason, including initial snapshot and reconnect. Client-local Alex bookkeeping cannot stand in for station state. |
| R-R3-15 | Remote Auto AGC-T visuals reflect the station's per-slice AGC noise-floor state. GUI Clarity smoothing is not an AGC measurement source. |
| R-R3-16 | Remote GUI Connect/Disconnect actions operate the configured Core station session. Disconnect cancels pending retries; explicit Connect starts a fresh session without relaunching or invoking direct-radio discovery. |
| R-R3-17 | The GUI persistently identifies its configured Core and shows disconnected, connecting, retrying, or connected state independently of Core's radio state. Failed attempts leave a useful reason and available recovery/cancel action. A Core session with an offline radio must not appear to be a disconnected GUI session. |
| R-R3-18 | Remote C-Tune preserves the Core receive-window centre during in-window VFO tuning. Explicit pan motion moves Core's window and shifts every cohost without moving their VFOs. Pin state belongs to the stream, is negotiated, clears with the station session, and is restored from GUI preference after reconnect; direct local behavior remains unchanged. |
| R-R3-19 | Remote frequency-scale and wheel zoom stop at the currently supplied DDC bandwidth during the gesture. An unsupported ADC-wide view must not be offered then collapsed by Core's crop ACK. Preserve the local extended-view preference; wider remote coverage requires a separately advertised wideband source. |
| R-R3-20 | Restore the existing extended-pan behavior across Core/UI: supported wideband ADC data fills the view outside the listenable DDC island, with the existing zoom gestures, RF alignment, wing tuning and filter-state feedback. Core owns capture, calibration and shared ADC/BPF demand. The R-R3-19 DDC-only limit is an interim compatibility fallback, not completion of receive parity. |
| R-R3-21 | Visible remote receive controls act on their declared local or station owner and show accepted state. Controls unavailable in the negotiated role/capabilities are visibly disabled with a reason. TX affordances respect `txPermitted=false` in R3; Core refusal remains mandatory. A visible-control inventory and interaction tests distinguish implemented behavior from unavailable features. |
| R-R3-22 | Core owns station accessory sockets, discovery, live configuration application and basic connect/disconnect/cancel. It validates device identity before declaring a TGXL connected, cancels obsolete retries, and publishes current status. Tuner telemetry applies without commands; headless frequency/mode reporting follows the stable station TX-bound slice. Direct local behavior is preserved. |
| R-R3-23 | Remote audio exposes persistent playback/media/output status, the actual accepted codec profile and useful measured health. Local device/trim/mute stay separate from station slice mix controls. Any quality selector requires measured profiles and acknowledged Core configuration; unimplemented codec settings are not presented as functioning controls. |
| R-R3-24 | Attaching/reconnecting and restoring saved GUI layout hydrates the authoritative snapshot without outbound slice creation. One saved pan attaching to one existing station slice retains that slice. Explicit post-hydration operator add/layout actions still work within station capacity. |
| R-R3-25 | Receive-only remote frequency changes, snapshot replay and inbound accessory telemetry cannot initiate tune-carrier orchestration or TX-coupled accessory commands. Apply the guard before enabling repaired TGXL connection/telemetry. Local-direct operation retains its established behavior; authorized remote TX/tuner workflows remain R4. |
| R-R3-26 | A configured Core listener recovers when its bind address becomes available after startup. A running radio service must not silently remain without remote control after a transient bind failure. Retry is bounded/backed off, preserves station identity, and is cancelled by stop/reconfiguration; readiness and errors are observable. |
| R-R3-27 | A daemon that starts before its configured radio is discoverable can later recover that same radio without a daemon restart. Discovery retries must preserve the pinned radio choice, station identity and slice ownership, remain cancellable, and keep the Core control plane responsive. Listener recovery alone does not establish radio recovery. |
| R-R3-28 | Unexpected media transport failure/closure must recover through a fresh authenticated session even when Core control remains connected. Recovery reuses the configured endpoint, certificate pin, pairing, bounded backoff and authoritative snapshot; manual Disconnect/teardown cancels it. Stale peer callbacks cannot reconnect a newer or intentionally stopped session. Ordinary audio jitter/device errors and permanent protocol refusals do not trigger whole-session retry. |
| R-R3-29 | An established radio stream that stops delivering data must not remain indefinitely presented as healthy. Detect and report receive silence using the protocol's authoritative behavior, safely retire stale DSP/media state, and provide cancellable recovery of the selected radio. Verify separately from initial discovery, first-packet connection timeout and GUI/Core media loss; preserve receive-only/TX safety policy. |
| R-R3-30 | Mirrored model updates cannot target a destroyed or replaced GUI widget. Pan layout changes, slice rehoming/removal and reconnect must retire widget-bound callbacks while preserving updates to the replacement view. Reproduce the observed VFO AGC update crash and cover the actual widget lifetime boundary. |
| R-R3-31 | RADE receives the audio of its owning slice, including slice B on a second pan, without redirecting or silencing another slice. Switching an already-streaming slice to RADE, decoder acquisition/loss of sync, channel replacement and worker restart preserve the one mixed stereo output and reject stale input/decoded speech. Existing mixer cadence and per-slice controls remain authoritative. |
| R-R3-32 | The Core-connected banner restores useful live telemetry in remote mode. Distinguish radio-to-Core measurements, Core-to-GUI transport health and GUI-local playback; values identify their source and units. Authentication alone cannot imply healthy radio/media, stale or unavailable data is explicit, and reconnect cannot display old measurements as current. |
| R-R3-33 | Inspect freshly fetched AetherSDR upstream and port its applicable telemetry history/graphing UI with source attribution. Adapt data ownership to Nereus Core/GUI while preserving meaningful graph behavior, bounded history, sampling and gaps. Do not copy Flex-specific transport assumptions or label inferred loss/latency as measured telemetry. Verify direct and remote modes, reconnect, stale data and rendering overhead. |
| R-R3-34 | Core persists and restores the operator's receive slices and their identities, frequencies/modes and pan bindings across restart. Unsupported or failed restoration is explicit, with actionable recovery; an attached GUI cannot retain an apparently configured but empty second pan without explanation. Reconcile daemon configured slice count, station persistence and snapshot hydration; cover at least the observed two-receiver restart. |
| R-R3-35 | Telemetry graphs expose measured Core↔GUI traffic in kbit/s or Mbit/s, including direction and total, with Opus audio separately visible. State the accounting boundary (payload versus transport/wire overhead) and include both control and media without double counting. Show client audio buffering delay separately from control RTT; never present RTT or half-RTT as measured one-way Opus latency. Unknown measurements and reconnects produce explicit gaps. |
| R-R3-36 | Receive startup and reconnect remain responsive when an optional host microphone cannot open. Unneeded capture must not block receive initialization. Preserve microphone metering/PC transmit availability and safe teardown; native device acceptance is separate from deterministic DSP/loopback lifecycle tests. |
| R-R3-37 | Core advertises and independently enforces measured aggregate display byte and spectrum sample limits. Favor the active pan, reduce background FPS then pixels, and restore requested quality when headroom returns. Account PureSignal display traffic; keep Opus independent. Revisioned outcomes preserve reservations across refusal, stale replies and reconnects. Production limits require hardware measurements. |
| R-R3-38 | One connection-selection flow exposes local radios, LAN Core/radio pairs and saved stations. Show Core identity, its configured/connected radio, and separate Core/radio availability. Select, connect, cancel, disconnect and switch without editing launch arguments or restarting manually. Retire prior session/media/settings ownership before activating another target; preserve local direct operation and authenticated station authority. |
| R-R3-39 | A Core stays reachable and controllable while its signal processing is overloaded. A call that needs a receiver's DSP lock waits at most about one DSP block on every platform, so heartbeats, session traffic, audio and commands keep flowing, and channel shutdown or rebuild never frees memory a running DSP block still uses. A DSP-control thread that keeps WDSP calls off the event loop entirely is required before remote transmit (R4). |
| R-R3-40 | The Core measures each receiver's processing load and input delay, reports them in its telemetry, and bounds the delay so memory and latency cannot grow without limit. A receiver whose noise-reduction model cannot keep up steps down (Premium, Standard, off) within seconds, keeps the operator's saved choice, says why in plain words locally and remotely, and lets the operator try again. The model's cost on the Core's hardware is measured, not assumed. |
| R-R3-41 | On Linux the Core runs each busy DSP worker on its own fastest core, chosen from the kernel's CPU capacity data without board tables, and keeps spectrum, network and housekeeping threads off those cores. It raises DSP priority only where the system permits and logs once, in plain words, what it got. Computers with identical cores still give the DSP worker a core of its own when there are enough cores. |
| R-R3-42 | A remote window serves TCI to apps on its computer. TCI commands act on the Core's slices; the receive audio for TCI receiver N is the Core's slice N from the network; transmit and raw I/Q are refused in plain words until remote transmit. TCI settings belong to this computer. |
| R-R3-43 | The Core sends a receiver's own audio on a separate stream only while an app on the client asks for it, at the session's single audio quality choice with the same link trial and fallback. Older apps and Cores see today's wire unchanged. |
| R-R3-44 | VAX works in a remote window: a VAX channel assigned to a receiver carries that receiver's audio from the Core at the level local VAX gives, paced by the VAX output's own clock. The Core host publishes no VAX devices of its own. |
| R-R3-45 | Each receiver plays on the speakers or the headphones device, chosen on its VFO flag as the audio design specified, with a local radio and in a remote window; a remote window receives a second mix from the Core while any receiver is routed to the headphones. |
| R-R3-46 | A remote window controls the Core's radio hardware the way a local window controls its own. The Core tells the window which radio it has; the attenuator, preamp, auto-attenuate, overload indication, receive antennas, OC receive pins, the HL2 filter board, calibration and the receive sample rate are applied by the Core live and saved there. Transmit-side hardware follows the transmit permission until remote transmit. |
| R-R3-47 | Station accessories (Power Genius XL, Tuner Genius XL, RF-Kit RF2K-S) connect to the Core only. The Core checks each device's identity before admitting it, keeps it connected, follows the band, records its faults, and relays status, meters and faults to every client; clients change connection settings and the interlock policy through documented, versioned session commands, so the desktop remote window and the iPhone app are thin clients of one API. Transmit-coupled accessory actions wait for remote transmit. |
| R-R3-48 | The Core serves TCI on the station network for station devices such as the RF-Kit, switched by the same TCI switch and port as the app and remembered on the Core; transmit requests over it are refused until remote transmit. |
| R-R3-49 | Every control a user can see does what its label says. A control whose feature is not built yet is hidden in local and remote windows through one list that names each unbuilt feature, and appears when its feature is built. Controls the operator chose to remove are gone; values users saved for them stay in the settings file. |
| R-R3-50 | Every library compiled into or shipped with a NereusSDR desktop package or a Core package has its licence text and a row in `packaging/third-party-licenses/README.md`, and CI fails when a vendored or fetched library has none. |

The September 21 user request explicitly adds banner telemetry restoration and
the current Aether telemetry graph port. Upstream was fetched from
`ten9876/AetherSDR`; the audit uses immutable commit `0dea0dd7` (September 21),
in a separate checkout so the existing Aether feature branch is preserved.
The source audit must identify the actual graph classes, license/provenance,
metric owners and integration tests before production porting begins. The
same user report reconfirms RADE-U on the second pan failing and muting pan 1.

The authorized radio is the ANAN-G2/Saturn, MAC `2C:CF:67:AB:FC:F4`, board
`0x0A`; the September 20 instruction supersedes the old G2E-only bench rows.
All live tests are receive-only. Remote TX stays disabled. No public post,
push or PR update is part of this implementation checkpoint.

Keep core GUI-free, use AppSettings, preserve upstream notices/comments,
sign commits, and use dedicated profiles for every test process. Never place
tokens, private keys or passwords in this plan, source, test fixtures or logs.

## Shared interfaces and sequencing

New media code lives under `src/core/session/media/`; rendering integration
lives in `src/gui/RemoteMediaController.{h,cpp}` and SpectrumWidget. Existing
session and model classes retain their thread ownership and synchronous echo
guards. Library callbacks must queue owned data onto the correct Qt owner;
they must not call RadioModel or AppSettings on a library worker thread.

- `MediaSourceKey { int streamIndex; FftTier tier; }`, where `FftTier` is
  `Wide` or `Fine`. This identifies production, not a pan or hardware DDC.
- `SpectrumSubscription` contains a session-local `quint32 endpointId`,
  source/resolution request, enabled state, crop center/span, pixels, target
  fps, frames-per-line, independent plane reduction settings, quantization
  range and optional 3D coverage request. The daemon returns an accepted
  `SpectrumContext` with actual values and a new context generation.
- `ReducedSpectrumFrame` carries endpoint/context identity, encoder sequence,
  producer/sample timing, exact frequency coverage, independent trace and
  waterfall arrays, waterfall-advance flag, and optional wide-row coverage.
  Context changes reset reduction and codec history. An accepted context is
  required before any matching media frame can render.
- `IMediaTransport` exposes start/stop, local signalling output, validated
  remote signalling input, nonblocking display send, RTP send, owned receive
  buffers, readiness and failure. It carries no model mutation logic.
- `RemoteAudioTap` writes borrowed interleaved float samples into a bounded
  SPSC ring synchronously; ownership ends when the callback returns.
  `RemoteAudioSender` drains it off the DSP thread. `RemoteAudioReceiver`
  owns packet ordering, Opus state, clock correction and the playback ring.

Task 1 establishes dependency/API contracts. Tasks 2 and 3 may then run in
parallel with separate file ownership. Task 4 integrates them. Task 5 completes
audio. Task 6 expands capacity and runs the final acceptance. Do not parallelize
CMake, session-message schemas, MainWindow edits or hardware operations.
The original numbering is retained; the current remaining order above takes
precedence. Tasks 4c and 4d consume the same session/model boundary and must
agree their capabilities and reply semantics before editing shared files.

## 1. Close dependency and clock-correction contracts

**Requirements:** R-R3-02, 03, 07, 10. **Dependencies:** existing R2.

**Deliverable/files:** pinned dependency manifest/build helper in `cmake/`,
dependency notices in the existing attribution inventory, and a compact
verification record under `docs/architecture/2026-09-20-remote-daemon-r3-verification/`.

- [x] Recover source and existing design decisions; record mixed stereo.
- [x] Verify the saved Opus mode warning against pinned source and packets.
- [x] Validate libdatachannel v0.24.5 and its exact transitive revisions,
  licenses, media-enabled build and standalone direct peer exchange.
- [ ] Prove an unreliable display message and separate RTP packet over real
  encrypted loopback peers without an external STUN service. Stop/recreate
  peers, and capture packet sizes with the configured MTU. A small application
  payload alone does not prove the UDP-size requirement.
  HARDWARE-PENDING: the loopback probe passed two rounds at MTU 1000
  (transport-probe.md:86-88); RTP packets are capped at 940 bytes before
  encryption (OpusAudioCodec.h:26, PcmAudioCodec.h:41, refused oversize at
  LibDataChannelMediaTransport.cpp:770, from ea55d24a). Operator observes: a
  packet capture of the encrypted audio (Opus 24k, Opus 48k and Lossless)
  showing every packet at 1000 bytes or less, on loopback or the Rock LAN;
  only the display capture exists so far (969 bytes).
- [x] Validate a continuously adjustable resampling implementation from an
  existing source, preserving phase/history across ratio changes. Inspect the
  existing WDSP variable-rate path before proposing a new dependency. Record
  exact source and API; do not use periodic ring flushes as drift correction.
- [x] Add the pinned production dependency after its transport source gates;
  expose it through NereusCore without adding GUI linkage or an uncontrolled
  second Opus copy. Include daemon-component install dependencies. The
  independent resampling gate remains a prerequisite for audio playback.

**Verification:** dependency build and two-peer integration, actual packet
inspection, deterministic variable-rate sample accounting. Native ARM and
Windows packaging remain distinct checks. A failed prerequisite blocks its
dependent implementation, not independent codec work.

**Delegation:** Terra for bounded source/API investigation; Sol only if the
integration exposes a difficult platform/linking failure.

## 2. Wire daemon production and independent spectrum planes

**Requirements:** R-R3-01, 04, 08, 09. **Dependencies:** source contract above.

**Files:** `DaemonApp.{h,cpp}`, `FftEnginePool.{h,cpp}`, new
`session/media/SpectrumEndpoint.{h,cpp}` and source/tier types; tests
`tst_remote_fft_production`, `tst_spectrum_endpoint`.

**Consumes/produces:** stream I/Q and source timing into pool engines;
`fftReadyLinear` into one reducer per endpoint plane; produces a latest
`ReducedSpectrumFrame` and accepted `SpectrumContext`.

- [x] Give DaemonApp ownership of a live pool, connect the same per-stream
  I/Q producer used by local mode, and tear it down before RadioModel streams.
- [x] Add `(stream,tier)` lifecycle without changing existing local callers'
  default Wide behavior. Derive FFT sizes from resolution targets, clamp to
  supported sizes, report unmet requests, and never resize an unrelated pan's
  wide engine to satisfy a fine request.
  DONE: b4eefeda (sharing regression failed first), 2140d0ae (the Core
  reports what each pan got), 69315721, follow-up gaps plan Task 1 (every
  remaining pan gets its full grant back). Tests: tst_daemon_media_controller,
  tst_display_budget.
- [x] Validate subscription values and ownership; clamp output pixels to the
  available source bins and report the clamp. Keep a latest frame slot, not
  an unbounded Qt frame queue.
  DONE: b4eefeda (matching validation, coalescing test), 2140d0ae (the clamp
  is reported to the window).
- [x] Use separate SpectrumReducers for trace and waterfall. Apply cadence
  before transport encoding and preserve independent averaging histories.
- [x] Extract the existing 3D wide-crop/peak-preserving behavior into a
  GUI-free helper with its original attribution. Produce bounded coverage
  and at most the renderer's 768 wide samples. Do not substitute another
  detector/averaging formula for the current local wide-row behavior.

**Acceptance:** synthetic stream I/Q produces frames with no MainWindow;
two endpoints on one stream have independent reductions; deep zoom does not
lengthen a neighboring wide FFT; out-of-range crops produce no stale frame;
disable/unsubscribe and slice reuse release the right producer; optional 3D
peaks and frequency coverage match the local helper.

**Verification:** focused Qt core tests and one receive-only daemon capture on
the Rock 5C. Build targets before `ctest -R ... --no-tests=error`.

## 3. Implement the bounded display codec

**Requirements:** R-R3-03, 04, 05, 09. **Dependencies:** frame/context contract;
can proceed independently of the peer library.

**Files:** new `session/media/DisplayCodec.{h,cpp}`, frame codec types,
`tests/tst_display_codec.cpp`; explicit CMake registration.

**Interfaces:** encoder accepts a reduced frame and current context, emits
bounded native bytes; decoder returns either a complete decoded frame,
`NeedKeyframe`, or a typed rejection. Context application and reset are
explicit. Parser failure never changes the last accepted frame/history.

- [x] Define and check in version 1's exact byte layout before connecting
  sockets: network byte order, endpoint/context/sequence fields, bounded
  plane lengths, flags, and no native struct serialization.
- [x] Quantize dBm over the negotiated finite min/max window. Encode temporal
  residuals against the decoder's reconstructed previous frame, using
  adaptive block widths and an absolute/keyframe fallback when deltas expand.
- [x] Specify the accuracy/dead-zone setting and prove its error bound over
  long ramps; errors must not accumulate invisibly through the delta chain.
- [x] Keyframe on first frame/context change, periodically, and on reliable
  request. Reject deltas after a gap until a valid keyframe. Rate-limit keyframe
  requests, accept sequence wrap correctly and reject stale generations.
- [ ] Carry trace and waterfall independently, plus optional bounded 3D row.
  Measure maximum frame size and the peer library's fragmentation behavior;
  cap reassembly memory and transport backlog.
  HARDWARE-PENDING: code done in ec47ce35 (send-buffer limits, stalled-receiver
  test 1,376,067 B before vs 159,137 B after, frame-size logging) and
  0eb8aa74 (PureSignal pacing). Operator observes: on the Rock at 4096 points,
  60 fps with 3D, the Core log shows a largest frame of at most 9,361 B in
  11 fragments, and a capture shows 1000 B or less.

**Acceptance:** quantization error no greater than half one quantization step
for clipped in-range samples at the precise setting; explicitly documented
larger bound for each rate-control setting. Empty, nonfinite, truncated,
oversized, unknown-version and malformed-block inputs reject without state
mutation. Deterministic loss/reordering tests recover at the next complete
keyframe and never display an invalid delta result. Noise and peak fixtures
cover 4096-point full-span input, both planes and 3D overhead.

**Verification:** portable codec tests; fuzz-style bounded malformed inputs;
measured encoded bytes and errors. Synthetic data is codec evidence, not an
RF quality or network-throughput claim.

## 4. Deliver live spectrum through the authenticated session

**Requirements:** R-R3-02 through 05, 09. **Dependencies:** 1 through 3.

**Files:** media transport implementation, `StationServer/StationClient`,
`SessionMessages`, `StationCapabilities`, `RemoteMediaController`, MainWindow,
SpectrumWidget; tests `tst_remote_media_session`, `tst_remote_spectrum_render`.

- [x] Add a minor-version-negotiated media capability and bounded signalling
  messages. Authenticate first; bind peer SDP/certificate fingerprints and
  candidates to the accepted control session. Old peers retain control-only
  behavior and receive no unsupported media messages.
- [x] Establish direct DTLS/SCTP and SRTP using libdatachannel. Retain current
  WSS command/state handling. Configure display unordered with no retries;
  keep RTP separate. Reject oversized signalling before library parsing.
- [x] Add subscribe/context/enable/unsubscribe/keyframe control messages with
  explicit endpoint ownership. Late library callbacks cannot revive an old
  session. A failed peer stops its producers and leaves control responsive.
  DONE: ea55d24a (DaemonMediaController.cpp:913-915) plus revision and session
  checks in b4eefeda. Tests: staleEpochAndForeignConnectionLeaveEndpointsUntouched,
  revisionedUnsubscribeCannotRetireOrResurrectNewerState,
  failedDisplayAttemptDebitsCreditAndRecoversWithKeyframe. Live Saturn display
  seen (README:172, 208).
- [x] Decode into a dedicated external reduced-frame widget entry point.
  Do not feed reduced dBm into the local linear-FFT path and reduce it again.
  Advance 2D and 3D from the decoded waterfall cadence; use explicit supplied
  wide coverage rather than a fictitious full-DDC cache.
- [ ] Preserve immediate crop/zoom within the negotiated margin and update
  endpoint coverage at the existing gesture boundary. Reconfiguration gets
  a new generation and keyframe; old-frequency frames cannot repaint it.
  SUPERSEDED: operator decision D1 (questions file line 27) - the view
  renews at the end of the gesture, final, with no 1.5x margin. The renewal
  half is done (tuning waterfall continuity row, zoom flash hotfix 25df3195).

**Acceptance:** one live Saturn pan and both waterfall modes advance;
retune, zoom and band changes preserve frequency alignment; deliberate loss
recovers; preemption/reconnect has no stale rows, leaks or duplicate feeds.
An unauthenticated or old-session peer cannot subscribe or deliver media.

**Verification:** real loopback peer tests, fake-radio/session boundary tests,
offscreen renderer contracts plus a real GPU visual smoke. Then the Rock 5C
LAN run. This milestone is a display checkpoint, not completed R3.

## 4a. Complete receive telemetry bindings

**Requirements:** R-R3-13 through R-R3-17. **Dependencies:** authenticated
state mirror and decoded remote display.

- [x] Publish separate read-only `SliceModel.signalPeakDbm` and
  `signalAverageDbm` from the existing Core meter pump. Preserve the legacy
  selected `signalStrengthDbm` property and local direct behavior.
- [x] Feed the remote applet and slice meters from these properties,
  selecting the existing peak/average modes locally. Obtain Max Bin from
  each slice's decoded passband. Start the GUI meter timer without waiting
  for a local WDSP channel, and clear readings on disconnect.
- [x] Mirror effective per-chain BPF/WIDE state and reasons; adapt the
  existing CH indicator bindings and replay state after reconnect.
- [x] Mirror station Auto AGC-T measurements and bind the applet/flag
  visuals without feeding back client-derived noise estimates. Core now owns
  a bounded per-stream FFT/tracker independently of display subscriptions;
  both the existing threshold tick and mirrored visuals use that tracker.
  Source and session regressions pass; live deployment acceptance is recorded
  separately and remains pending at this source checkpoint.
- [x] Route remote-role menu/shortcut Connect/Disconnect actions to the saved Core
  session, including cancellation during retry backoff and a fresh explicit
  connection on the same client. Retain direct-radio behavior for local mode.
  Live GUI acceptance is recorded separately in the verification ledger.
- [ ] Complete the disconnected title/status/pan click-to-connect paths;
  each currently reaches the suppressed local ConnectionPanel. Keep explicit
  operator actions separate from automatic panel-open callbacks so a manual
  Disconnect cannot cause an immediate redial during session teardown.
  HARDWARE-PENDING: c28e1565 (Connections selector), 1212d819
  (tst_remote_window_harness covers title, station block and disconnected
  pan), fb82f488 (Connections opens only after the operator's own
  Disconnect). Operator observes: each click in the installed window starts
  exactly one Core attempt; after a manual Disconnect nothing redials and
  Connections opens.
- [ ] Add persistent configured-Core identity and connection/retry/error
  feedback to the existing GUI chrome. Show Core connectivity and radio
  connectivity separately. Offer a clear retry action and cancellation;
  expiring toasts and log-only socket errors are insufficient.
  HARDWARE-PENDING, plus open decision C: 95b19467, c28e1565 ("Retrying Core
  (attempt N)", "Radio offline"), later wording plans. Still undecided: queue
  item C, what a window refused permanently or taken over by another session
  shows; today it reads "Core connection failed"
  (RemoteConnectionController.cpp:89).
- [ ] Verify all visible entry points during first connection, unreachable
  Core, retry backoff, manual cancellation, recovered Core, and connected
  Core with its radio offline. Repeat a manual disconnect after a successful
  session to catch retained-name auto-open callbacks. Treat this as a basic
  R3 usability gate before the two-hour receive acceptance run.
  HARDWARE-PENDING: the automated half is in tst_remote_window_harness
  (14 cases). Operator observes: first connection, Core unreachable (nereusd
  stopped), attempt count rising, cancel, Core recovered, Core up with radio
  down (stop p2app on the G2), manual Disconnect after a good session.

**Verification:** real Core meter reads, read-only mirror round trips,
source selection, stable slice IDs, disconnect/TX/lifetime tests, unchanged
local tests, then visible needle and numeric movement on the Saturn feed.
BPF and Auto AGC-T require their own state/snapshot/reconnect regressions
and live observations; the meter fix does not establish those gates.

The full connection-screen discussion remains a separate follow-up. Preserve
the identity design's three groups: radios on this network, stations on
this network, and paired stations. R5 supplies the remote reachability paths;
R6 completes the selection/pairing experience and packaging. The September 21
operator report brings basic feedback and consistent actions forward into
R3; the earlier menu-only round trip did not prove the whole interface.

## 4b. Restore wideband extended-pan parity

**Requirements:** R-R3-04/08/09/10/11/14/18/19/20. **Dependencies:** accepted
remote spectrum contexts and authenticated receive controls. This explicit
follow-through was added after the September 21 operator report; the older
R3 `Wide` FFT tier and optional 3D wide rows are DDC-bounded and do not supply
ADC-wide wings.

**Existing specification:** [Wideband Extended Pan plan](2026-05-26-phase3f-sub-epic-f-wideband-plan.md)
and the parent multi-pan design, section 7. Reuse the implemented behavior in
`WidebandFrameAccumulator`, `WidebandFftEngine`, RadioModel's per-ADC production
and demand reconciliation, and SpectrumWidget's local extended rendering.

**Owned files:** `src/core/session/{StationCapabilities,SessionMessages}.*`,
`src/core/session/media/` display contracts and endpoint production,
`src/models/{RadioModel,SliceModel}.*`,
`src/gui/{RemoteMediaController,SpectrumWidget,MainWindow}.*`, and their
remote-spectrum, extended-wing and filter-state regression tests.

**Interfaces:** extend the existing `RadioModel::widebandSpectrumReady`
publication with physical ADC identity, capture generation and configured
geometry; raw FFT power in dB must not be described as calibrated antenna
data. The September 22 contract selects Core composition of ADC wings and
the DDC island before averaging, carried in the existing bounded codec v1
trace/waterfall rows. A negotiated source descriptor supplies explicit RF
geometry, level reference and current-session/context identity. Do not
reinterpret `FftTier::Wide` or transmit unrestricted full FFT arrays.

- [x] Trace the current local extended-pan contract end to end, including
  wing click/drag behavior, physical ADC versus filter-chain mapping, BPF
  bypass ownership and restoration, and 2D/3D history. Record the proposed
  capability, source identity and reduced-frame schema before implementation.
  September 22: [source-grounded contract and ordered implementation](2026-09-22-remote-wideband-design.md)
  select Core composition before per-plane averaging, preserving codec v1
  trace/waterfall rows and the separate DDC-only 3D row. The shared Core/local
  normalization helper and Core compositor are implemented. Five focused
  targets pass, including pixel-by-pixel parity against all five local
  detectors with and without a visible DDC island. The endpoint integration
  below now connects that compositor to remote displays. The
  122.88 MHz geometry remains the explicit existing Thetis reference, not a
  negotiated P2 rate or a new per-ADC calibration. Tagged publication now
  retains one latest frame per physical ADC with connection/capture/geometry
  identity and a capture timestamp recorded before FFT dispatch. Seven focused
  targets pass; consolidated review added a regression for retiring availability
  before teardown notifications. That regression and worker lifetime pass
  after correction. The combined GUI/Core/test build succeeds; the unfiltered
  suite passes 723/725 executables, with the same two native microphone-open
  timeouts recorded at the preceding checkpoint (R-R3-36 remains open).
  The model now aggregates physical-ADC capture separately from filter-chain
  bypass and provides endpoint-owner leases with release/remap/retirement
  coverage. Eight matching focused demand checks pass, including actual
  daemon teardown. The matching combined GUI/Core/test build succeeds and
  the unfiltered demand suite passes 724/726 executables, with the same two
  native microphone-open timeouts; the full-suite gate remains unmet.
  The September 22 endpoint integration now binds those leases to negotiated
  display lifetimes, carries Core-composed ADC wings in codec v1 rows, and
  restores GUI zoom availability with strict context admission and retained
  history. Applied capture state establishes identity before the first ADC
  row. Consolidated review corrected synchronous send/retirement lifetime
  hazards and permission/coverage inconsistencies. See the wideband contract
  for current verification: all 12 focused targets pass, the matching
  GUI/Core/all-tests build succeeds, and the unfiltered suite passes 725/727
  executables (313.08 seconds), with the same two native microphone-open
  timeouts and 12 inner Qt skips. The full-suite gate remains unmet; R-R3-36,
  hardware acceptance and Task 6's total session allocator remain open.
- [ ] Carry Core-produced, calibrated wideband display data through the
  authenticated session under the existing datagram and session budgets.
  Aggregate demand across pans; hiding/removing one pan must not disable
  another pan's capture or leave bypass enabled after the last consumer.
  Endpoint transport, shared demand and retirement are implemented and tested;
  the checkbox remains open for total-session allocation and live acceptance.
  HARDWARE-PENDING: endpoint work is in the wideband design; the session
  allocator (R-R3-37) charges the combined rows unchanged. Operator observes
  on the Saturn: wings stay under the budget; hiding one pan leaves the
  other's capture running; filter bypass clears after the last pan.
- [ ] Composite the DDC island and wideband wings using their actual RF
  coverage. Preserve the existing local zoom range on capable hardware;
  keep the DDC fallback only when the remote source is unavailable. Wing
  tuning must use Core commands and preserve shared-slice constraints.
  Core composition and GUI availability/history wiring are implemented; real
  hardware zoom/wing gestures and shared-slice acceptance remain pending.
  HARDWARE-PENDING: Core composition and window wiring done; zoom flash fix
  25df3195. Operator observes: real zoom and wing gestures, including a
  slice shared with another window.
- [ ] Verify zoom across the DDC edge, release without snap-back, inward
  zoom, ADC changes, shared pans, rejected moves and reconnect. Reject stale
  wideband frames and apply calibration once. Verify real Saturn wideband
  capture and BPF transitions without transmitting.
  HARDWARE-PENDING, plus a small follow-on: besides the operator
  observations, no freshness limit has been set for wideband frames (no TTL
  has been guessed, wideband design:411-413); it needs the Saturn's real
  burst cadence measured first.

**Verification:** meaningful integration tests for source identity, bounds,
RF alignment, multi-pan demand and filter restoration; existing local-wing
tests remain green. Compare the same gestures and frequency coverage in local
and remote receive modes. Record live 2D/3D behavior, network cost and Rock 5C
load separately; R3 parity remains open until those observations pass.

## 4c. Complete visible controls and snapshot-safe startup

**Requirements:** R-R3-02/09/10/16/17/21/24.
**Dependencies:** existing StationClient lifecycle and task 4a connection
entry points. This closes the interaction gaps identified by audit C01-C04
and C10; task 4g now brings basic station selection forward from R6.

**Owned files:** `src/gui/{MainWindow,SetupDialog}.*`, affected setup/applets,
`src/core/session/StationClient.*`, `src/models/RadioModel.*`,
`tests/tst_remote_gui_gating.cpp`, session/snapshot tests, and a compact
control acceptance matrix in the existing R3 verification directory.

**Interfaces:** consume station lifecycle, snapshot readiness, generation,
negotiated capabilities and Core radio state. Produce consistent actions and
presentation from those values. Session connection is not snapshot readiness;
restoring presentation must not imply a station create command. Reuse the
existing capability schema; negotiate any genuinely missing capability before
advertising it. Preserve explicit operator add/layout operations after hydrate.

Operator-facing status must describe the effect and available action in ordinary
language. Keep roadmap phase names, protocol capability names and healthy
fallback diagnostics out of operating overlays, tooltips and refusal messages.
Missing optional aggregate display limits are a supported fallback, not a
warning to the operator. Keep actual display pauses, connection failures and
unsupported actions visible with a useful explanation; retain technical details
in diagnostics.

- [x] Inventory currently visible menus, applets and setup controls by owner:
  GUI-local, station-backed, or unavailable in remote receive. Record the
  concrete handler/property and acceptance case. Trace generic mirrored DSP
  setters before labelling them broken; the observed FIR-graph gap is a local
  visualization dependency, not evidence that all DSP settings fail.
  DONE: f14e04e2 plus fix wave feb9dc59 (tst_remote_gui_gating 63/63;
  remote-controls.md). The operator's "no silent disables" rule later turned
  many "unavailable" rows into parity work.
- [x] Wire Tools → Network Diagnostics to the existing
  `MainWindow::openNetworkDiagnostics()` role-aware handler. The Tools entry
  now opens the same local/remote dialog as the status segment. Production
  QAction tests verify both routes without local discovery or connection
  changes, and a disconnected remote session remains disconnected. The test
  uses an isolated settings profile and the existing discovery holdoff;
  production startup behavior is unchanged. Matching GUI/Core/all-tests builds
  pass, followed by an unfiltered **727/727** desktop suite in **276.12 seconds**.
  Operator acceptance remains below.
- [ ] Complete task 4a's title/status/pan connection actions and persistent
  status. Keep automatic panel callbacks separate from explicit connect.
  Disable unavailable local-resource controls with a reason, and gate all
  TX entry points using the negotiated permission while retaining Core guards.
  The remaining named TX-presentation pass covers Phone/CW MIC/PROC/VAX/DEXP,
  XIT, TX-slice handoff, RX-bypass-on-TX, Tools TX Equalizer, and the six TX
  Setup leaves. It consumes the existing `txPermitted` capability; it does not
  implement or remove R4's station TX commands. Preserve local operation,
  receive controls and the separate local-DSP resource gate. VAX Setup is
  receive export and is not classified as a TX page. See the
  [control matrix](2026-09-20-remote-daemon-r3-verification/remote-controls.md).
  HARDWARE-PENDING (operator's wording review): 46b01ebf
  (tst_remote_tx_presentation, tst_remote_tx_widgets), feb9dc59, follow-up
  gaps Task 2. The operator approves the disabled states and their reasons
  at a checkpoint.
- [x] Reproduce the extra-slice startup through the actual connection,
  snapshot and `populateEmptyPans` path. Separate hydration/layout restoration
  from explicit operator creation; do not delete an existing station slice
  to conceal an unintended create. Test reconnect and delayed snapshots.
  DONE: 1212d819 - the held-snapshot rows fail when the remote guard is
  removed; the original fix was 95b19467.
- [x] Add a narrow injected session/presentation harness that exercises the
  real action routing. Slot-existence checks alone do not close this task.
  DONE: 1212d819 (tst_remote_window_harness, 14 cases); multi-pan halves
  added in d9f4e362.
- [ ] R-R3-30: retire SliceModel-to-VFO callbacks with their actual flag
  widget when a pan shrinks or a slice is rehomed. The observed AGC crash
  used a MainWindow-owned lambda capturing a deleted flag. Correct the
  identical pure presentation bindings together; do not suppress valid
  station updates. Exercise the production bindings with a surviving slice,
  a flag removed through SpectrumWidget, and a replacement flag. Prove
  automatic disconnection before applying AGC and sibling properties, then
  prove the replacement receives updates.
  The production lifetime correction and replacement-view regression now pass
  focused/full-suite verification and independent review; live layout acceptance is tracked
  in [VFO lifetime verification](2026-09-20-remote-daemon-r3-verification/vfo-lifetime.md).
  HARDWARE-PENDING: 3075a5b9 (tst_slice_flag_presentation_lifetime: 45
  handlers still live before the fix). Operator observes: shrink two pans to
  one while the Core keeps sending AGC updates; no crash, and the
  replacement flag keeps updating (vfo-lifetime.md:39-40).

**Acceptance:** each connect entry point starts one configured-Core attempt;
cancel during backoff leaves it stopped through delayed callbacks; Core-up /
radio-down presents distinct state. With one saved pan and one Core slice,
connect/reconnect emits zero add-slice commands and preserves station identity.
An explicit add after hydrate emits one valid request and handles refusal.
TX and FIR graph controls show unavailable state when their capabilities or
resources are absent. Local direct connection and layout behavior remain valid.
Shrinking two pans to one while Core continues AGC updates cannot call a
destroyed flag; the migrated slice and its replacement flag stay responsive.

**Verification:** session/hydration and cancellation are consequential state
transitions: establish reproducing integration cases before changing them.
Build `tst_remote_gui_gating`, `tst_session_verbs` and `tst_remote_role_inert`
before running the corresponding `ctest --test-dir build-integration -R
'^tst_(remote_gui_gating|session_verbs|remote_role_inert)$' --no-tests=error`.
Register/build any additional harness target explicitly. Then perform the
listed connect/cancel/reconnect cases in the actual GUI with receive only;
record both automated and visible results in the matrix.

**Execution:** bounded Terra implementation is suitable after the lifecycle
contract is settled; the lead owns shared MainWindow/session integration.
Do not dispatch an overlapping MainWindow editor for task 4d.

Current bounded implementation evidence is recorded in the
[control acceptance matrix](2026-09-20-remote-daemon-r3-verification/remote-controls.md).
Connection presentation, snapshot guards, selected TX/FIR controls and tuner
telemetry now have interaction/session regressions. Full visible-control
inventory and actual GUI acceptance are still required; the checkboxes above
remain open until their complete acceptance is demonstrated.

## 4d. Make basic station accessory use work remotely

**Requirements:** R-R3-02/10/21/22/25. **Dependencies:** source audit C06-C09,
C11-C12, authenticated station commands and state/settings ownership.
Receive-only guards precede repaired live connection and telemetry acceptance.

**Owned files:** `src/core/{TgxlConnection,PgxlConnection,SmartSdrApiListener}.*`,
`src/models/{RadioModel,TunerModel}.*`, session capabilities/messages/client/server,
`src/gui/setup/{CatNetworkSetupPages,FourO3APage,TgxlAdvancedPage}.*`,
`src/gui/{MainWindow,applets/TunerApplet}.*`, accessory/session tests and the
control acceptance matrix. Core remains GUI-free.

**Interfaces:** station-scoped accessory settings already persist through
SettingsProxy. Use an atomic authenticated `configureTgxl(host: Utf8, port:
Int64)` command and an idempotent `disconnectTgxl()` command. A configure
acceptance means validated configuration was saved and identification started;
only the subsequent Core snapshot may report Connected. Validate the complete
endpoint before any persistence or lifecycle change. Refuse without the
current radio MAC scope or with 4O3A disabled. Carry requests through typed
`IStationLink` methods and the existing CommandInvoke/CommandResult path;
do not let a click race individual settings writes and use an old endpoint.
Negotiate support through a capability entry and protocol minor version,
retaining an explicit unavailable state with older Core builds. Inbound
TunerModel state uses a client-only assign/notify adapter, never its hardware
command hook. Discovery is station-side, or its remote button is explicitly
unavailable until implemented. Full administration/pairing remains R6.

**Identity and status contract:** the observed discovery aliases are exactly
`TunerGenius` and `TunerGeniusXL`. Match discovery IP and received discovery
port against the actual TCP peer, then require the native sequence-correlated
`info` serial to match the discovery serial. Native info captures contain
serial/version/nickname, but no product model; a V banner, nickname or serial
format alone cannot establish TGXL identity. Reject `PowerGeniusXL`, a serial
mismatch, or a bounded identification timeout without enabling tuner commands.
A valid manual hostname can be saved and attempted; absence of matching
station-side discovery must remain an actionable identification failure.
Use the existing `LanDiscovery` three-second scan window for discovery;
bound native identification separately and cover its cancellation in tests.
Publish the accepted endpoint, phase, error and observed identity through
read-only TunerModel fields. Endpoint replacement must invalidate all previous
discovery, TCP and info callbacks. Captured wire evidence is in
`captures/flex-tgxl-direct-NOTES.md` and the control verification ledger.

**Implementation sequence:** finish socket retry/cancellation, then implement
Core identity and live configuration together with its snapshot fields and
authenticated command tests. Wire Peripherals to that settled contract next;
remote mode must not watch or dial the Mac-local TGXL socket. Add the typed
`requestFourO3AEnabled(bool)` station-link request and authenticated
`setFourO3AEnabled(enabled: Bool)` verb for the existing master toggle. Return
the station's accepted/refused result and publish actual listener state;
the remote toggle must never start the GUI's local listener. Preserve direct local mode.
Finish headless frequency/mode propagation after that ownership boundary is
settled. Keep the existing receive-only guards throughout these changes.

**September 22 follow-on contract:** `remoteFourO3AControlVersion=1` is a
separate capability within accessory protocol minor 4. The exact typed Bool
command is accepted after validation and durable per-MAC intent storage;
acceptance does not claim a successful bind. Read-only `fourO3AEnabled`,
`fourO3AListening`, and `fourO3AListenerError` report actual Core state. A
repeated enable retries a failed bind; disable always cancels pending accessory
work. The GUI shows pending/result state, clears it across session replacement,
and assigns inbound fields without touching its own listener or settings.
Headless propagation follows `txBoundSlice()`, retaining the existing SmartSDR
slice-0 representation and PGXL 200 ms within-band debounce. Unrelated viewed
slices cannot drive it. Real loopback listener and acknowledged PGXL fixtures
cover initial state, retune, mode, rebind, removal and canceled debounce.


**September 21 software checkpoint:** Core-owned identity/configuration and the
remote Peripherals/applet path are implemented with negotiated minor 4 support.
The integrated review corrections include durable endpoint saves, MAC-scope
reset, identity-sensitive discovery, retry/cancel state, session-loss clearing,
and the legacy native accessory proxy's receive-only guard.

**September 22 software follow-on:** the typed remote master, actual listener
state/error, Core-owned TX-bound frequency/mode propagation and fresh-socket
retry lifecycle are implemented. Authenticated tests caught and corrected the
new state fields' client admission; review caught unanswered requests surviving
session replacement, now covered for both dropped and directly replaced links.
Real TGXL connection/identity acceptance remains open: discovery is visible,
but the latest Core TCP observation is SYN-SENT with no reply. See the
[verification ledger](2026-09-20-remote-daemon-r3-verification/tgxl-recovery.md)
for build, review and installation evidence.

- [x] Establish regressions for remote band-change auto-recall and applet
  reactions to `isTuning`. Guard RX-only operation and telemetry replay from
  autotune/carrier/operate/bypass/relay/antenna commands; preserve the existing
  local-direct workflow. This guard is part of the telemetry change, not a
  later R4 cleanup. Do not enable remote TX as a connection repair.
- [x] Validate TGXL identity before declaring connected/present or enabling
  commands. Derive supported model aliases and serial correlation from actual
  discovery/info evidence; a V banner alone is insufficient. Reject a PGXL
  response and show expected versus observed device. Bound handshake timeout.
- [x] Implement unconditional cancellation across connecting, handshaking,
  connected and backoff states for disable, disconnect, radio teardown and
  endpoint replacement. A cancellable timer or generation check must prevent
  an old captured endpoint from redialling after replacement.
- [x] Route basic remote configuration/apply/connect/disconnect to Core;
  show selected endpoint, identity, pending state and error. Apply all 13 tuner
  snapshot/update fields safely, including false/zero values and disconnect.
  Gate applet orchestration while retaining read-only visual updates.
- [x] Move existing SmartSDR frequency/mode seeding and PGXL band propagation
  from MainWindow to the authoritative Core slice wiring. Follow the stable
  TX-bound slice, including binding change/removal, and remove duplicate GUI
  sends. Retain source attribution and current wire behavior.
- [ ] Complete Core-owned real-device connection/status acceptance without
  tuning or RF. The former `.235:9008` endpoint addressed the amplifier.
  On September 21 the actual tuner `.234:9010` became reachable; the user
  corrected the local GUI port and status responses arrived. That resolves
  the current local TCP question, not remote Core ownership. The connect-time
  retry path also emitted invalid-descriptor/source-bind errors after timeouts;
  reproduce and correct its cancellation/lifecycle behavior.
  HARDWARE-PENDING, cause unknown: the retry lifecycle is fixed, each attempt
  uses a fresh socket (tgxl-recovery.md:219, 270-272). The Rock's connection
  to .234:9010 still hung half-open with no reply while the Mac reached the
  tuner. Operator observes: a network check first, the Rock's route and
  interface, per the station-network rule from 2762d6ec.

**Acceptance:** a PGXL banner/status never yields a connected tuner; supported
tuner identity does, and unknown/timeout responses leave an actionable error.
Disable/disconnect in every lifecycle state causes no delayed redial, including
after endpoint changes. A remote configuration action applies on Core without
restarting it and opens no GUI-side accessory socket. All 13 fields hydrate,
update and recover without any hardware command, including an `isTuning=true`
snapshot. With auto-recall enabled and a stored memory fixture, remote RX band
changes send no autotune. Headless initial, within-band, mode and rebind updates
report the correct station slice; unrelated slices cannot drive the accessory.
Also exercise the real authenticated command boundary with invalid endpoint,
no MAC scope, disabled master, unknown capability, A-to-B replacement while
identity is pending, and GUI reconnect during Core identification. A command
acceptance must never be mistaken for device admission. The 4O3A master toggle
must start/stop only the Core listener and cancel pending accessory work.

**Verification:** establish meaningful authorization/lifecycle regressions
before changes. Reuse observed wire fixtures, never invented TGXL identity
strings. Build/run affected targets including `tst_tgxl_connection_parse`,
`tst_tgxl_connection_ping`, `tst_pgxl_connection_reconnect`,
`tst_tuner_model_apply_status`, `tst_tuner_applet_context_menu`,
`tst_settings_proxy`, `tst_session_verbs` and `tst_remote_role_inert`; register
new lifecycle/telemetry/headless-boundary tests where coverage is absent.
Use the corresponding exact-name `ctest -R` selection with `--no-tests=error`.
Then inspect station-reported frequency/mode and tuner status on the real
bench without RF. Hardware acceptance stays pending while TCP 9010 is
unavailable. Actual tuner operation and pairing/interlock changes remain R4.

**Execution:** Sol is appropriate for this ownership/lifecycle boundary after
the lead settles command and capability contracts. Keep hardware and shared
session/MainWindow edits serial; use one integrated risk-focused review.

## 4e. Restore the Core banner and connection/audio history

**Requirements:** R-R3-32/33/35. **Dependencies:** task 4c's current connection
controller, authenticated session and existing audio diagnostics. User chose
banner and connection/audio graphs first; Core CPU/memory history follows.

**September 21 operator follow-up:** existing audio packets/s and frames/s
do not answer bandwidth usage. Add graphs for Core→GUI, GUI→Core, their total,
and Opus separately, with adaptive kbit/s or Mbit/s units. Audit actual media
and control byte counters before implementation; distinguish encoded audio
payload from RTP/transport overhead and avoid counting a packet twice. The
current Core RTT is a control-channel round trip, not audio end-to-end delay.
Expose measured client audio queue duration in milliseconds where available;
true capture-to-playback one-way latency remains unavailable until its timing
contract is implemented. Cover counter resets, silence/zero traffic, media
context changes, reconnect gaps, and actual graph labels/rendering.

September 22 presentation correction: inspecting the running disconnected GUI
showed that empty graphs hid all series labels and units. The graph now retains
unit-bearing, selectable legends while measurements are unavailable. The
production widget was rendered and visually checked with total/directional
kbps, separate Opus/audio-packet kbps and speaker-buffer ms. This source change
is not yet installed; existing traffic counters and RTT semantics are unchanged.

**Design:** [Core telemetry contract](2026-09-21-core-telemetry-design.md).
This records the freshly fetched Aether source revision, owned measurements,
wire contract, validity, history bounds and concrete acceptance cases.

**Owned files:** new `src/core/session/StationTelemetry.*`,
`src/core/daemon/DaemonTelemetryController.*`, `src/gui/RemoteTelemetryController.*`,
`src/gui/RemoteDiagnosticsDialog.*`, `src/gui/TelemetryHistory.*` and ported
`src/gui/TimeSeriesGraphWidget.h`; existing session messages/capabilities/
client/server/transport, daemon wiring, remote audio receiver, MainWindow and
TitleBar; focused telemetry/session/history/widget tests and attribution.

**Interfaces:** Core publishes a capability-gated `station.metrics.v1`
observation at 1 Hz. GUI combines that with its own control-link and playback
observations. A typed remote banner replaces the text-only paint override.
No station setting, radio action or heartbeat/retry decision is driven by these
measurements. Never equate transport send acceptance with packet delivery.

- [x] Fetch current Aether upstream into an isolated source snapshot and audit
  graph/history licensing, data sources, gaps and lifecycle semantics.
- [x] Settle the first scope with the operator and record the concrete design.
- [x] Implement and test the bounded typed collector/session contract.
- [x] Add observational GUI transport and receiver diagnostics.
- [x] Port/adapt Aether graph/history behavior with attribution and gap tests.
- [x] Wire live banner and remote Network Diagnostics, including older-Core,
  stale and reconnect states; preserve direct-mode behavior.
- [x] Verify the integrated checkpoint and live receive-only Rock 5C session.
  Matching `c9065756` passed 695/695 executable tests, native installation,
  banner/all-three-tab readability and real restart/reconnect history gaps.
  R-R3-35 adds total/directional Core–GUI traffic, separate Opus/track kbps,
  and speaker-buffer ms. Audio soak and true end-to-end delay remain separate.
- [ ] Define and implement measured Core-to-client audio latency. Record the
  exact start/end timing points, cross-host clock relationship and measurement
  uncertainty before labelling any graph as one-way latency. Distinguish
  delivery to the client from actual speaker playback, retain unknown/stale
  gaps, and validate against an independent timing measurement. Existing
  control RTT and sampled PCM buffering do not satisfy this follow-on.
  HARDWARE-PENDING: built per decision D2 (02e7a217, 9e7134fa, 03291c21).
  tst_audio_clock_estimator passed 2000 random trials; tst_remote_audio_session
  measured 237.7 ms vs 238.0 ms heard (Opus). Operator observes: the readout
  on the Rock with a real speaker.
- [ ] Operator checkpoint for the empty-state labels and diagnostics menu:
  verify total/directional kbps or Mbps on Connection, separate Opus kbps on
  Audio, and RTT versus speaker-buffer ms on Round trip / buffering, both
  connected and disconnected. This remains pending installation of the current
  source; previous live graph acceptance does not cover these later fixes.
  HARDWARE-PENDING: labels changed since this box was written (d5eae8b5,
  99106153). Operator observes: the Connection, Audio and Round trip tabs,
  connected and disconnected.

**Verification:** follow the design's codec/lifecycle, real collector,
history, widget and live acceptance cases. Register and build focused tests
before running them. One combined full suite and consolidated review precede
the signed installation; hardware/readability evidence is recorded separately.

## 4f. Preserve the receive layout across Core restart

**Requirement:** R-R3-34. **Observed gap:** installing `200d2a0e` restarted Core
with its configured single receiver. A resumed at 14.2932 MHz USB, while the
GUI retained an empty second pan and B had to be recreated. A GUI reconnect
alone and a Core restart are distinct lifecycle boundaries.

- [x] Audit existing Thetis/local receiver persistence and daemon slice-count
  configuration before selecting the station-owned restore representation.
  September 22 source audit after WDSP integration confirms that NNR leaves
  are per-radio/per-slice, but membership and pan keys are not persisted;
  startup recreates `slice_count` and loads conventional state only for active
  A. A pinned MAC is available before discovery, so restoration can retain the
  responsive offline listener. The proposed bounded per-radio descriptor adds
  stable ID, pan key and current frequency/mode without claiming a migration
  of all legacy `Slice<N>` band settings. Pending hydration must avoid starting
  RADE through a mode setter before connection/resource validation. Exact
  persistence interfaces are defined in the
  [receive-layout design](2026-09-22-core-receive-layout-design.md).
  The data-only store now persists a validated per-radio layout and explicit
  RADE receive-audio owner; its disk round trips and rejection behavior pass
  within the fresh 719-test suite. See the
  [foundation evidence](2026-09-20-remote-daemon-r3-verification/receive-layout-store.md).
  Passive Local-model hydration now reconciles stable objects and restores
  tuning/preferences without decoder startup or bootstrap settings writes;
  [hydration evidence](2026-09-20-remote-daemon-r3-verification/receive-layout-hydration.md)
  records the initial passive checkpoint. Runtime integration now covers daemon
  startup/count precedence, board/pan resource admission, sparse-ID DSP startup,
  explicit RADE receive ownership, settings capture/retry and authenticated GUI
  reseeding. Consolidated review corrections and the in-process recovery
  identity fix pass. The final matching build passes; the unfiltered suite is
  722/724, with two unchanged native microphone-opening timeouts still open.
  See the [runtime evidence](2026-09-20-remote-daemon-r3-verification/receive-layout-runtime.md).
  Hardware restart acceptance remains open.
- [x] Restore saved slice identities, frequencies/modes and view bindings, with
  capability/resource validation and safe fallback for a changed radio.
- [x] Reconcile the attached GUI from the restored snapshot; removed or refused
  receivers must be explicit and must retire stale media/control callbacks.
- [ ] Test a real two-slice Core stop/start and authenticated client reconnect,
  then repeat the two-pan receive-only hardware checkpoint.
  HARDWARE-PENDING: design and runtime evidence is in
  receive-layout-runtime.md. Operator observes: restart nereusd with slices
  A and B on two pans; both come back with the same IDs, frequency, mode and
  pan, and the window creates no slices.

**Verification:** meaningful persistence/snapshot/media lifecycle tests and
real restart acceptance. Do not fix the observation by silently increasing a
hard-coded startup slice count or duplicating receivers during client attach.

## 4g. Select a Core/radio pair or a local radio

**Priority:** next source deliverable after the display-capacity checkpoint,
promoted from R6 by the September 22 operator request. **Requirements:**
R-R3-10/16/17/21/38. **Existing design:** identity/pairing §6 already specifies
“Radios on this network”, “Stations on this network”, and “Your stations”.
This is completion of that product flow, not a new competing connection design.

**Current evidence:** the unified Connections screen and session coordinator
are implemented in the desktop. They expose local radios, saved Core/radio pairs
and untrusted LAN announcements, with explicit connect/disconnect/retry actions.
The desktop retains its embedded Core/DSP. Saved targets migrate the previous
single-station tuple without weakening its pin/token contract. Whole-session
switching, actual two-Core/TLS tests and widget renders pass. Matching GUI/Core
and all-tests build passes with 738/738 CTest executables; installed hardware
acceptance remains open and is tracked in the
[selector evidence](2026-09-20-remote-daemon-r3-verification/station-selection.md).
See the [design](2026-09-22-station-selection-design.md) and
[implementation steps](2026-09-22-station-selection-plan.md).

**Expected presentation:** each remote entry identifies both the Core and its
radio, for example “Rock 5C → Saturn G2”, with Core connectivity distinct from
radio availability. Local radios clearly use this computer's Core/DSP. Choosing
an existing pair does not silently reconfigure which radio its Core owns. Core
radio reassignment, if offered, needs a separate authoritative station command
and identity/ownership acceptance. Persist client credentials locally.

- [x] Produce the concrete selection design and implementation steps from the
  existing panels, startup, settings-proxy and station-lifecycle source. Include
  a reviewable screen layout and explicit switching/error states.
- [x] Implement saved targets and explicit local/remote selection; migrate the
  existing configured station without losing its trust information. Keep the
  existing pinned TLS/token contract until the identity/pairing migration is
  implemented; a basic picker must not weaken authentication.
- [x] Add Core discovery and Core/radio identity presentation from the existing
  dual-stack discovery design. Distinguish advertised/cached labels from the
  authenticated current station snapshot and keep manual address entry useful.
- [x] Connect all entry points to the selector. Switching retires the old
  control/media session, retries, model/render ownership and settings proxy
  before activating the new local or remote target. No overlapping radio owner,
  stale callback, credential crossover or accidental switch on an offline row.
- [ ] Verify local direct operation, Core A→Core B, local→remote→local, wrong
  identity, unavailable Core, connected Core/offline radio, cancel/reconnect,
  settings isolation and visible selection/status. Use isolated fake/loopback
  tests first and then an operator checkpoint with actual radios/Core.
  HARDWARE-PENDING: the automated part passes (station-selection.md:40,
  two-Core encrypted tests). Operator observes (station-selection.md:95-103):
  LAN announcements (IPv4 and IPv6), switching between the local radio and
  the Rock with real display and audio, add/edit/forget, Core online with
  radio offline.

Full key pairing, revocation and administration remain tracked by R6 and the
identity design; their absence must remain visible rather than represented by
nonfunctional controls. Standalone desktop operation remains a product invariant.

## 5. Deliver mixed stereo Opus with continuous playback

First sound is implemented and heard on the bench. Remaining playback
interruptions, the profile comparison and final two-hour stability run are
separate open gates. Follow the current remaining order above; R5 traversal
is not a prerequisite for LAN listening.

**New startup gap (R-R3-36):** an independently installed Mac application
blocked for 541.93 seconds opening its default microphone before PortAudio
returned an internal error. The same boundary stalled four native lifecycle
tests. Test-only audio-device injection now keeps those DSP/loopback tests
deterministic across reconnects; production microphone startup is unchanged.
Plan a receive-startup/capture-demand boundary with explicit failure reporting
and safe teardown. A timeout around a non-cancellable native open is not by
itself a safe recovery mechanism. Native device acceptance remains required.

The bounded source audit identifies `AudioEngine::start()`'s unconditional
`ensureTxInputOpen()` as the receive-startup coupling. Remote GUI models already
skip this local startup path, but receive-only daemon models do not. Explicit
device changes in `setTxInputConfig()` also open capture synchronously. PC-mic
TX consumes that bus through `TxWorkerThread`; microphone meters only poll it,
and the current Test Mic control starts a meter timer without opening capture.
The next implementation must make capture demand explicit and observable,
trace the local MOX admission boundary before changing TX preparation, and
define how meter visibility/Test Mic requests capture. A restored PC-mic source
preference alone must not be confused with an active capture request. Preserve
existing TX safeguards and verify failure/stop/reconnect with injected devices;
keep direct native input opening as a separate acceptance check. This audit
does not yet choose an asynchronous native-open lifecycle or alter production
microphone behavior.

The two existing input-settings surfaces also need one authoritative device
configuration: Devices edits persisted `audio/TxInput` and calls the engine,
while the PC Mic page edits TransmitModel session properties with no engine
configuration connection. Test Mic currently only polls the existing bus.
Integrate configuration, capture demand and actual device readiness together;
do not report a selected named input as ready after silently opening a different
default device. Preserve the independent speaker-device policy.

The source-selection prerequisite is implemented: the worker now uses
`AudioEngine::isPcMicSelected()` independently of capture readiness. Previously,
`isPcMicOverrideActive()` required an open bus, so selected but absent/closed PC
capture could leave radio-mic samples in ordinary, RADE and normal VOX input.
The new regressions reproduced that leakage and now verify zero-filled input,
valid/short PC pulls and deliberate return to radio selection. The existing
combined readiness predicate and non-PC/generated-audio paths are preserved.
See [source-intent evidence](2026-09-20-remote-daemon-r3-verification/pc-mic-source-intent.md)
for checks and remaining limits. The existing
`MoxController::setMox()` precheck precedes RF side effects and is the shared
admission boundary for the later readiness gate; release must remain
unconditional. Worker silence does not implement that gate or an asynchronous
capture lifecycle, and R-R3-36 remains open.

**Requirements:** R-R3-02, 03, 06, 07, 09. **Dependencies:** 1 and 4's session
lifecycle; audio codec unit work can precede GUI spectrum completion.

**Files:** `AudioEngine`, new `RemoteAudioTap/Sender/Receiver`,
`OpusAudioCodec`, `AudioJitterBuffer`, variable resampling adapter and playback
adapter; tests `tst_remote_audio_mix`, `tst_opus_audio_codec`,
`tst_remote_audio_clock`, `tst_remote_audio_session`.

The September 21 source audit narrows the remaining integration:

1. Own an audio sender from `DaemonMediaController`: consume the existing
   `DaemonAudioSource` off the DSP callback, encode with `OpusAudioEncoder`,
   and send via the current session's `MediaPeer::sendRtp`. Source sample
   positions drive timestamps; peer/session retirement stops capture and
   retires the encoder and SSRC.
2. Connect `RemoteMediaController` to `MediaPeer::rtpReceived`, with bounded
   reordering, jitter, loss concealment, continuous rate matching and a
   playback bus independent of local radio DSP. Validate clocks against
   actual device consumption, not the worker timer's cadence.
3. Bind client output device, trim and mute; flush queued audio and retire
   generations on mute/disconnect. Verify actual stereo listening, per-slice
   gain/mute/pan and reconnect on the Saturn/Rock/Mac path.

The vendored WDSP `rmatch`/`varsamp` implementation is a source candidate
for continuous rate matching; its exported `create_rmatchV`, `xrmatchIN`
and `xrmatchOUT` API preserves variable-resampler state. It uses critical
sections, so it must not run in a lock-free device callback. Source review
alone does not close task 1's runtime gate: the wrapper, attribution,
consumer-clock feedback and deterministic drift tests are now implemented.
The bounded adapter feeds the source's 64-frame blocks and preserves its
continuous interpolation. Both signs of 500 ppm pass a simulated hour with
zero underflows/overflows. This closes the offline runtime gate; hardware
listening and long-session acceptance remain separate.

- [x] Tee exactly one successful master drain before speaker volume/mute.
  Preallocate bounded storage with nonblocking producer admission; use an
  explicit overflow/reset policy, counters
  and bounded latency. Do not retain a callback pointer or encode on DSP.
- [ ] Use the existing pinned Opus build, 48 kHz stereo and standard RFC 7587
  packet/timestamp semantics. Begin with the saved 24 kbit/s, constrained VBR,
  40 ms, AUDIO/MUSIC, complexity 10 profile and explicit wideband. Compare
  higher-rate stereo settings during listening; report actual channels,
  packet bandwidth, bytes and CPU. Do not reinterpret the old mono benchmark
  as a measured stereo result.
  HARDWARE-PENDING: code done, the 24k profile, 5f06fce2 (48k passes sound up
  to 20 kHz), afa926a8 (audio_bitrate setting and a matching SDP). FT8 decode
  cost measured (digital-modes-over-opus.md). Operator observes: Rock CPU at
  24k and 48k, bytes on the wire, operator listening.
- [x] Add bounded ordering/jitter, Opus loss concealment and continuous clock
  correction with preserved interpolation state. Start near the design's
  180 ms receive-buffer target; make timing injectable for tests.
- [x] Drive a client playback bus independently of local RadioModel DSP.
  Local output trim/mute must work, mute flushes queued samples, and encoding
  suspension is a session operation. Resume starts a fresh audio generation.
- [x] Sample-rate, mode, band and slice transitions preserve a 48 kHz mixed
  output contract or explicitly reset audio context when required. Include
  cold startup with exactly one slice: the P2 codec assignment must reach the
  radio while connecting and converge on Connected, without depending on a
  later GUI slice creation or retune. Verify wire DDC rate and host DSP geometry
  agree. Close cancels pending work before rings, codec and device storage
  are destroyed.
  DONE: 7ee6d985 (tst_daemon_audio_transitions: 192k to 384k, SSB to RADE,
  band change, slice add/remove), 8c011066 (tst_p2_ddc_assignment_marshalling,
  tst_daemon_audio_session), live wire rate at 192 kHz
  (startup-audio.md:104). Limit: the transition test uses a P1 fixture only.

**Acceptance:** two test slices yield one stereo block per period, retain
left/right placement and mute, and work with no daemon sound device. Offline
clock tests cover both signs of 500 ppm over a simulated hour with bounded
occupancy and no periodic drop/duplicate repair. Loss/bursts conceal and
recover; old SSRC/generation packets cannot play after reconnect. Stereo
listening on the real client is required in addition to packet receipt.

**Additional continuity check from September 21:** leaving RADE on both
receivers resolves the operator's earlier audible issue. The installed mixer
capacity correction restores Core's 48k source rate, but a passive Core capture
still shows outgoing audio gaps up to 110 ms. The actual receiver regression
reproduces underflow with a 100 ms arrival gap followed by a coalesced pair,
while four valid packets remain in the reorder queue; it fails before any
intentional packet loss. Task 5 therefore includes demand-only release of the
exact expected, already received packet when PCM cannot satisfy the next
speaker refill. Preserve normal arrival-plus-80-ms behavior, original loss
deadlines, bounded latency and restart semantics. Test the real receiver,
missing-head/order/reset boundaries and both existing clock-drift directions;
then repeat RADE listening on the Rock. Sender scheduling and the sustained
remote-audio soak remain separate acceptance work.

**Verification:** unit/integration tests, measured Rock 5C encoding and client
playback, then real long-session audio. Lossless/high-rate digital-mode audio
remains a tracked parent requirement; do not advertise Opus as transparent
input for weak-signal decoding or claim that path accepted without its gate.

## 5a. Expose useful remote audio controls and health

**Requirements:** R-R3-06/07/17/21/23. **Dependencies:** current playback path,
task 4c status ownership, and task 5's measured profile comparison.

**Owned files:** `src/gui/{RemoteMediaController,MainWindow}.*`, existing audio
setup surface, `src/core/session/media/{DaemonMediaController,OpusAudioCodec}.*`,
negotiated media contracts if needed, and remote audio/GUI tests.

**Interfaces:** consume actual accepted encoder settings, local output state,
receiver counters and session generation. Produce persistent status and a
small operator-facing profile choice only if the measurements justify it.
The currently fixed 24 kbit/s profile is not a negotiated quality selector.

- [x] Show current codec/rate, local output/mute and persistent output/media
  error with a recovery action. Distinguish no signal from no media, local mute
  and output-device failure; show measured loss/jitter/buffer health with clear
  meanings. Arrival spacing alone must not be labelled packet loss. Source
  complete; see
  [remote audio status verification](2026-09-20-remote-daemon-r3-verification/remote-audio-status.md).
  Hardware acceptance pending.
- [ ] Complete the 24/48 kbit/s mixed-stereo listening/CPU/wire-rate comparison
  in task 5. Decide the offered quality profiles from those results. Preserve
  the selected mixed stereo ownership; do not silently substitute mono.
  HARDWARE-PENDING: Rock CPU at 48k fullband still pending. 48k can only be
  set in nereusd.conf; the app's choice today is Opus or Lossless.
- [x] If profiles are offered, add acknowledged Core configuration with bounded
  reconfiguration and reconnect replay. Display accepted settings on refusal;
  never present raw frame-size/FEC/complexity controls without a working wire
  contract. The addendum's three-tier example is not a mandatory profile list.
  DONE (shape set by decision D3): Lossless choice with Core acceptance,
  plain refusal reasons and replay on reconnect: eda5c0d5, 714e9f88,
  2607265b, d5eae8b5. A real encrypted session carried lossless at
  1537 kbit/s; the link trial fell back to Opus in about 6 s. Lossless to
  the Rock still to be observed.

**Acceptance:** mute and device failures remain understandable after a toast
expires; client trim/mute does not change Core slice gain/pan. Displayed codec
matches actual packets. A refused or stale profile reply cannot claim a new
profile or revive an old session. Reconfiguration flushes incompatible queued
audio and recovers without unbounded latency. Listening comparison and device
failure recovery must be recorded separately from codec unit tests.

**Verification:** build `tst_opus_audio_codec`, `tst_remote_audio_receiver`,
`tst_remote_audio_session` and affected GUI targets before the matching exact
`ctest -R` selection with `--no-tests=error`; add meaningful profile/lifecycle
regressions if negotiation is introduced. Lead/operator compare the same
stereo reception at both rates and record preference plus measured costs.
No selector is required merely to make already-working Opus decoding operate.

## 5b. Preserve both receivers when RADE is selected on slice B

**Requirements:** R-R3-03/06/09/31. **Dependencies:** existing one-active-RADE
scope, SliceModel mode ownership and the master mixer's explicit membership.
This repairs the reported second-pan RADE-U failure; it introduces no new
active-VFO selection policy or transmit behavior.

**Owned files:** `src/models/{RadioModel,RxDspWorker}.*`,
`src/core/RadeChannel.cpp`, `src/core/audio/MasterMixer.*`,
`tests/tst_rade_rx_multislice_routing.cpp`, `tests/tst_master_mixer.cpp`,
affected RADE lifecycle tests and Aether/Thetis attribution.

**Interfaces:** publish the true owning slice identity and a generation to the
DSP worker at all existing RADE publication sites. Keep QObject lifetime on its
owning thread; the DSP worker must not dereference a raw RadeChannel. Queue
current decoded speech back through the DSP thread before AudioEngine so the
master mixer has one producer. Replacing/clearing the binding retires prior
I/Q, decoded speech and resampler history. An obsolete destroy callback cannot
clear a replacement owner, including when a slice ID is reused.

- [x] Reproduce the real two-slice routing/mixer failure before changing it.
  With both ordinary slices already producing, selecting B yielded
  `Aordinary=0 Bordinary=96 radeAfterA=44 radeAfterB=44`; master pushes stayed
  at one. The regression uses actual WDSP channels and the real audio mixer.
- [x] Route only the selected slice to RADE and retain other slice audio.
  Withdraw the selected slice from the mix until current valid output is
  available; mode exit, slice removal, worker replacement and MOX lifecycle
  must not prematurely re-admit an obsolete producer.
- [x] Restore Aether's existing same-sized quiet output for active decoder
  input without enough decoded speech (`RADEEngine.cpp:496–504` at
  `0dea0dd7`). The removed padding's old timer-driven assumption no longer
  matches Nereus's per-slice mixer barrier. Preserve decoder/DSP parameters;
  do not add a mixer timeout to disguise missing production.
- [x] Cover stale queued I/Q/speech, old-owner destruction, removed/reused
  slice IDs and worker teardown/restart. Prove a failed/warming/unsynced B
  decoder cannot stall A's mixed output.
- [x] Preserve every mixed source frame under bounded queued RADE delivery.
  The live check exposed a 39–44k/s mixed-source deficit at nominal 48k/s;
  the prior 256-frame ring discarded ordinary receiver audio while RADE was
  delayed. Adopt Thetis's 4096-frame RX/anti-VOX capacity independently of
  block size, keep immediate readiness drainage, and verify exact frame count
  and sequence contents through repeated/clumped producer delivery. Recheck
  the actual Core rate and listening before accepting the repair.
  DONE: 0b408427 (4096-frame minimum ring, MasterMixer.h:327; frame count and
  sequence tests). Core rate observed at about 48,000 frames/s
  (rade-multislice.md:163-165); zero source drops at 08:39 on 2026-09-23
  after the contention fix. The listening check goes with the next box.
- [ ] Run affected RADE/mixer/audio regressions, consolidated review and full
  suite; install a signed matching checkpoint and perform the receive-only
  two-pan RADE smoke with the user.
  HARDWARE-PENDING: operator observes A on SSB and B on RADE-U on the second
  pan; A stays audible as B enters and leaves RADE, even with nothing to
  decode; B's speech joins the mix.

**Acceptance:** A remains audible when B enters/leaves RADE-U, including
without a decodable signal; only B's post-demodulated input reaches its decoder.
Valid B speech joins the existing mixed stereo output with B's controls. Late
old output cannot re-enrol removed slices or reach replacement decoders.
Selecting ordinary receive restores its current path without a GUI/Core restart.

**Verification:** build `tst_rade_rx_multislice_routing`, `tst_rade_channel`,
affected RadioModel RADE tests and master-mixer/audio targets before focused
`ctest`. Use a deterministic valid decoder fixture where available; do not
claim on-air RADE decoding from an unsynced-noise test. Run full integration
checks once the combined correction is stable. Hardware acceptance includes
ordinary A + RADE B, ordinary B restoration, and pan/slice replacement while
receiving; no RF or tuner actuation.

## 6. Capacity, installation and release evidence

**Requirements:** all. **Dependencies:** integrated receive path.

**Files:** daemon/client capability policy, endpoint budget allocator, install
rules, configuration sample and the R3 verification ledger.

- [x] Use logical `PanadapterStack` membership for remote pane subscriptions.
  A float/dock or layout reparent retains its endpoint and painted history;
  `panRetired` releases it, and stack destruction releases remaining endpoints
  even when the controller survives. A reused pan ID receives a fresh endpoint.
  Retire local bindings before sending unsubscribe so synchronous session
  closure cannot invalidate an in-flight cleanup loop. This establishes stable
  allocation inputs; it does not implement the session-wide budget below.
  The old float/hide behavior failed the new integration regression before
  correction. Pane/layout/floating tests and six focused media/telemetry/recovery
  targets pass; a consolidated independent review found no actionable issues.
  Matching GUI/Core/all-tests builds pass. The unfiltered desktop suite passes
  725/727 in 338.49 seconds, with the same two native microphone-open timeouts
  (`tst_port_audio_bus`, `tst_audio_engine_speakers_live_reconfig`). R-R3-36 and
  the full-suite gate remained open at that checkpoint. The follow-up below
  records the latest suite result. Hardware float/dock acceptance is pending.
- [x] Close the floating pane's window when its logical pan is retired, and
  bind queued float/dock rendering to the original applet/window identity.
  Removing and recreating a pan ID cannot dock or show the replacement through
  stale work. Preserve an outgoing graphics owner until the moved widget has
  rendered in its new window or has been destroyed. Orderly shutdown destroys
  registered and pending-retired panes before their graphics owners, including
  an immediate quit during a dock operation. This completes the additional
  software lifetime gap under R-R3-09; it does not close hardware acceptance.
  Regression tests first exposed a stale floating-window registration and a
  Qt graphics-owner use-after-free. The three focused pane/layout/media targets
  now pass, including hidden destinations, reused IDs and immediate shutdown.
  Independent review found no blocking defect; its rapid multi-window coverage
  recommendation was added and passed for destination render, removal and
  shutdown. Matching GUI/Core/all-tests builds pass. The first unfiltered run
  passed 726/727 in 281.72 seconds and exposed the station-session fixture's
  shared settings file: parallel GUI tests could overwrite a TGXL persistence
  check. A process-specific test profile fixes that isolation gap without
  changing product TGXL behavior. The final unfiltered parallel run passes
  **727/727 in 272.92 seconds**, including both native microphone tests that
  timed out at earlier checkpoints. This satisfies the current software suite
  gate; R-R3-36's optional-capture startup behavior and hardware acceptance
  remain open and are not claimed fixed by one successful device-open run.
- [ ] Exercise one pan, four docked pans, floating pans and multiple slices
  sharing a stream. Allocate the session budget across focused/background
  endpoints; reduce display rate/pixels before impairing audio or control.
  Per parent architecture §9.5, the GUI divides Core's advertised total across
  endpoints and Core validates/enforces that ceiling. The logical pane lifetime
  above replaces the `isVisible()` heuristic. The operator approved the
  active-pan policy in the [September 22 design](2026-09-22-session-display-budget-design.md)
  and [implementation plan](2026-09-22-session-display-budget-plan.md), tracked
  by R-R3-37. The software allocator, acknowledged transitions, visible status
  and Core enforcement are implemented and component-tested; real-hardware
  exercise remains open. No production byte/sample limits have been measured; do not derive them from
  the single-pan 969-byte observation or the eight-endpoint admission cap.
  HARDWARE-PENDING: allocator (R-R3-37), CPU-adaptive plan, 9e8ae810 (older
  apps get their even share again). Operator observes on the Rock: background
  pans slow down first and show why.
- [x] Verify the budget against actual sender time, including rapid endpoint
  replacement and context renewal. `SpectrumEndpoint::configure()` resets
  per-endpoint cadence, so an admission sum alone cannot enforce the session's
  sustained send rate. Define the burst allowance and preserve enforcement
  state through reconfiguration within the same session. Distinguish spectrum
  application bytes from full display-channel traffic, wire overhead and CPU
  work; keep Opus independent. Real-controller tests with injected monotonic
  time now prove bounded byte/sample sends across churn, failed sends and
  production/peer restart; actual codec/chunk sizes are checked independently.
  Production limits and hardware acceptance remain open. See
  [display-capacity evidence](2026-09-20-remote-daemon-r3-verification/display-capacity.md).
- [ ] Measure full-span/noise and deep-zoom cases with both FFT tiers, WDSP,
  stereo Opus and 3D enabled on the Rock 5C. Report actual effective limits;
  preserve the separate Pi 4 hardware-floor obligation until measured there.
  HARDWARE-PENDING, plus a small follow-on: after measuring, set the
  production display limits in nereusd.conf (display-capacity.md:125-129)
  and settle the CPU-adaptive thresholds. The Pi 4 cannot be upgraded to
  current code: install-core-pi4.sh refuses an existing install.
- [ ] Verify the combined full desktop suite once after integration, plus
  ARM-sensitive focused tests and Linux daemon-component staged install.
  Do not hide known failures through exclusions or call a partial run green.
  REMAINING (a gate run): earlier runs - Mac 791/791 at 5c369c79; Linux
  arm64 Debian 13 container 778/778 at 31176d0b and 788/790 at 37072945
  (both flakes fixed in 8f8ae6d6, 5766dd36, 93567686). Must run once more at
  the final merged head.
- [ ] R-R3-26: reproduce configured-listener bind failure followed by address/
  port availability; recover without restarting the radio service or changing
  the configured bind/security scope. Stop/reconfiguration cancels stale retry
  callbacks. The September 21 boot left nereusd active but no control listener
  because Ethernet's address arrived late. Add lifecycle regression coverage
  and test late-network boot on the board; distinguish link flaps from Core
  readiness. Do not claim service-active alone proves remote availability.
  Software recovery/cancellation and capped-backoff regressions now pass,
  including the full 682-test suite. It is installed; controlled late-network
  board acceptance remains pending (see the verification ledger).
  HARDWARE-PENDING: code e518b63d. Operator observes: boot the Rock with its
  wired address arriving after nereusd starts; the control listener comes up
  with no service restart.
- [ ] R-R3-28: reproduce established media loss with a healthy control session,
  then route typed terminal transport failures through cancellable authenticated
  reconnect. Installed checkpoint 706b9a5f now requests the existing
  StationClient retry instead of leaving a closed media peer with a healthy
  control indicator. Focused and full-suite software checks pass. Verify live manual cancel,
  stale epochs, one preserved slice and real resumed display/audio; distinguish
  packet validation/device errors from retryable transport lifecycle failure.
  Track initial backend start refusal, negotiation without a terminal event,
  and repeated media failures across successful control handshakes separately:
  the current control backoff resets at handshake, so it cannot by itself prove
  growing backoff across those media failures. See [recovery evidence](2026-09-20-remote-daemon-r3-verification/media-recovery.md).
  HARDWARE-PENDING: code 706b9a5f, then f62ea04e, 0c9521dc, 010a123f,
  6461a788, 2aba5f05. Tests: tst_remote_connection_controls,
  tst_remote_media_controller. Operator observes: block the media UDP while
  TCP 50055 stays up; the window says it is retrying, waits longer each
  time, Disconnect cancels, the slice survives, display and audio return.
- [ ] R-R3-29: established P2 receive-silence recovery is implemented and
  passes real-loopback checks and the full 698-test suite. Matching Core/GUI
  `55e7d49f` is installed; physical radio-loss/resume acceptance remains pending
  after the Rock became unreachable during the initial live check. The source audit found Thetis ChannelMaster
  `network.c:655-666` uses a three-second all-inbound-UDP deadline followed by
  SendStop/zero notification; it does not itself implement automatic reconnect.
  Keep that source behavior distinct from a separately justified Nereus retry
  policy. A loopback P2 fake must send a real DDC packet, then cease ingress
  while command egress continues. These checks now prove terminal loss,
  same-radio recovery, cancellation and receive-only safety. Valid status-only
  traffic still keeps the sourced all-inbound watchdog alive. See
  [recovery evidence](2026-09-20-remote-daemon-r3-verification/radio-recovery.md).
  HARDWARE-PENDING: code 55e7d49f. Operator observes: stop p2app on the G2's
  Pi for more than 3 s, then start it; the same Core recovers the same radio
  and the window resumes.
- [ ] R-R3-27: cancellable discovery recovery is implemented for startup
  with the configured radio absent, then present. A worker replaces one-shot
  startup discovery; Core control stays responsive and existing GUI sessions
  receive late capabilities/settings while retaining their slice objects.
  Focused wrong-radio, cancellation, stop-during-setup and preservation checks
  pass, including the full 698-test suite. Matching `55e7d49f` is installed;
  the GUI authenticated before radio arrival during normal startup, but actual
  absent-radio startup acceptance remains pending; established-radio loss is tested separately under R-R3-29.
  HARDWARE-PENDING (written up in radio-recovery.md): recovery code
  55e7d49f. Observed twice on 2026-09-23. First at build ac4c63aa, installed
  while the G2's p2app was down: the Core ignored the other radio (.107), the
  window authenticated at 08:24:43, the G2 was found at 08:35:42 and audio
  played at 08:35:53, with no restart. Again at build 44584133 (20:15); the
  audit records nothing more about that session. Recorded in
  [radio-recovery.md](2026-09-20-remote-daemon-r3-verification/radio-recovery.md);
  acceptance still needs the slice-retention check from the window's log.
- [ ] Install a signed checkpoint with a recoverable previous binary/library
  set, preserve private station configuration, and verify boot, clean stop,
  client reconnect and live media. Provide a launcher using private pairing
  configuration outside the repository.
  HARDWARE-PENDING (repeat at the final checkpoint): done many times
  (80f45e28 with rollback to 44584133; the installer now refuses an empty
  service unit). Still unexplained: the earlier empty-files incident
  (station-selection.md:105-115).
- [ ] Run at least two hours of real audio with counters for underrun,
  overflow, loss, jitter, drift ratio, CPU, memory and thermal state. Include
  an explicit client disconnect/reconnect and verify stale audio is flushed.
  SUPERSEDED: operator decision on 2026-09-23 at 09:50, soak runs are
  removed as gates. The counters exist (38f79249, the 60 s periodic log
  line).
- [ ] Record one-pan and four-pan on-wire budgets and delivered quality.
  The parent screenshot figures are comparison targets, not acceptance data
  for this implementation. Complete its internet-capable receive evidence
  separately from LAN success; CGNAT-to-CGNAT remains the R5 acceptance case.
  HARDWARE-PENDING: a measurement; internet evidence (the Pi 4 profile over
  public IPv6) exists only informally.

**Verification:** fresh software results, real transport observations and
hardware/operator observations recorded separately as passed, failed or
pending. An advancing waterfall alone does not complete the release gate.

## Review focus and current ledger

The integrated review focuses on session-generation races, malformed media
and bounded memory, audio callback ownership, independent clock convergence,
frequency/context correctness and 3D/local rendering parity. Each is covered
by an acceptance case above. Independent review is consolidated at that
boundary; routine findings get one focused correction rather than repeated
whole-plan review loops.

| Work | Owner/status | Evidence/next action |
| --- | --- | --- |
| R2 baseline | Preserved rollback checkpoint `14124e7c` | Authenticated control/tuning/meter baseline retained; installed runtime advanced to `ea55d24a` |
| Combined open-PR recovery | Complete source checkpoint `14124e7c` | GUI/Core build and unfiltered 662/662 desktop tests pass; fixture-path workaround recorded separately |
| Native combined Core | Installed signed `200d2a0e`, rollback `0b408427` | Matching Core/GUI include telemetry, the RADE mixer cadence correction and packet-burst playout repair; 692 desktop tests and the native production build pass. Automatic full-Core restart recovery is observed; live media-only loss remains open. The soak is superseded (operator decision, 2026-09-23 09:50): removed as a gate. See [verification ledger](2026-09-20-remote-daemon-r3-verification/README.md). |
| Plan review and mixed-stereo choice | Lead, recorded | Current review; user confirmed mixed stereo and Opus |
| Direct media transport | Production adapter and MediaPeer integrated | Real encrypted peers, bounded callbacks, stop/restart/delete regressions pass; live display capture passes: Ethernet, 969-byte maximum IP packet; separate SRTP-size proof pending |
| Headless FFT and display codec | Implemented and component-tested | Independent Wide/Fine sources, retune input reset, independent planes, crop clamp, bounded codec and recovery tests pass; authenticated daemon-to-GUI regression and live Saturn display pass |
| Audio | User accepted smooth audio and waterfall at `706b9a5f` | Mixed stereo Opus is live. Startup wire/host mismatch was fixed at `8c011066`; source diagnostics show 48 kHz and about 25 Opus packets/s. Earlier Starlink/ZeroTier gaps and underflows still require sustained acceptance and profile comparison. The two-hour soak is superseded (operator decision, 2026-09-23 09:50): removed as a gate; the counters exist (38f79249, the 60 s periodic log line). |
| GUI media wiring | Desktop builds; initial focused checks pass | Dedicated reduced-frame renderer and authenticated subscription controller tested; two-pane, shared-window and reconnect regressions pass; live GPU spectrum/2D observed; operator 3D confirmation and gesture refinement pending |
| Tuning waterfall continuity | Visual regression corrected; focused tests pass | R-R3-04/09/10: subscription/context renewal preserves painted 2D/3D history while rejecting stale incoming planes; accepted RF geometry reprojects existing rows. Full-suite and live tuning gates remain recorded in the verification ledger. |
| Remote C-Tune control parity | R-R3-18 installed at `501b2701`; reported gestures accepted | Authenticated stream pin/centre commands, stream-lifetime guards, gesture-time pan selection, refusal rollback and reducer initialization pass the 681/681 combined suite. Matching Core/GUI are running; operator confirmed both wheel tuning and in-band scale zoom behave smoothly. Broader multi-slice hardware checks remain in task 6. |
| Wideband zoom parity | R-R3-20 source integration implemented; full acceptance open | Core now composes tagged ADC wings with the DDC island in negotiated display rows; GUI admits the descriptor, restores the source zoom ceiling and preserves history. Twelve focused checks pass. Total-session allocation, native ARM installation and real Saturn RF/filter/gesture/network/audio evidence remain pending; the installed build still has the DDC-only limit. See [contract](2026-09-22-remote-wideband-design.md). |
| Core/radio and local-radio selector | R-R3-38 implemented; installation acceptance pending | Unified Connections, saved-target migration, actual two-Core/TLS switching and embedded local Core are implemented. Consolidated source review corrections are complete; native multicast, renderer retirement and operator use remain open. See [evidence](2026-09-20-remote-daemon-r3-verification/station-selection.md). |
| Session display capacity | R-R3-37 software verified; hardware limits pending | Active pan priority, deterministic quality reduction/restoration, Core byte/sample pacing and PS3 accounting pass focused controller tests. Consolidated review findings are resolved; matching GUI/Core build and unfiltered 730/730 suite pass. Production values and Rock/Pi hardware acceptance remain open; see [evidence](2026-09-20-remote-daemon-r3-verification/display-capacity.md). |
| Applet S-meter | Live applet/flag agreement at `706b9a5f` | Matching -65 dBm observed after automatic Core restart recovery; prior no-signal reports remain documented with their source/connection failures. Sustained acceptance remains open. |
| BPF and Auto AGC-T indicators | Source implementation and focused tests pass | R-R3-14/15: station filter snapshot/reconnect, headless per-stream AGC source, active-applet/flag bindings implemented; live 20m filter and AGC floor observed, complete band/reconnect acceptance pending |
| Manual Core reconnect | Configured-endpoint controls installed; title-panel path verified | Title opens details; manual Disconnect/Connect restored receive at `8c011066`. New `706b9a5f` also automatically resumed after Core installation. New media-only recovery has pinned-TLS integration coverage; a real media-only drop and remaining UI entry points need acceptance. |
| Visible controls and snapshot hydration | Bounded task 4c implementation installed at `95b19467` | Startup implicit slice creation is fixed and covered; fresh live attach displayed one slice. TX applet/PureSignal/filter-match, FIR and tuner gates are implemented. Phone/CW, XIT, TX settings and other remaining surfaces are listed in the control matrix. |
| Station accessories / TGXL | Core-owned control/status and headless tracking installed in `3402d171`; real TGXL admission pending | Typed master/configuration, identity checks, cancellation and fresh-socket retries pass automated tests. Core's live listener reports the actual bound frequency/mode. Current tuner TCP attempts time out despite discovery; no identity acceptance is claimed. The new Settings page still needs visual acceptance. ANT1–3 remain receive-only gated. |
| Second-pan RADE | R-R3-31 playback correction installed at signed `200d2a0e`; listening acceptance still open | Owning-slice routing, lifecycle guards, the upstream 4096-frame mixer correction, and demand-only admitted-packet release are installed. The burst/loss regression, consolidated review and 692 desktop tests pass. Ordinary receive has shown no further GUI underflow restart after startup recovery; the one-SSB/one-RADE operator check remains pending. Leaving RADE on both receivers resolved the earlier audible problem. Sender scheduling and sustained operation remain open; see [RADE verification](2026-09-20-remote-daemon-r3-verification/rade-multislice.md). |
| Core banner and telemetry graphs | R-R3-32/33/35 installed; all three tabs and restart gaps observed live | Aether graph/history port uses fetched source `0dea0dd7`. Total/directional Core application traffic, separate Opus bandwidth and speaker buffering are visible in matching `3402d171`; RTT is separately labeled. All 696 desktop tests and native build passed. CPU/memory history and capture-to-playback latency remain follow-on work; see [design](2026-09-21-core-telemetry-design.md). |
| Audio controls and diagnostics | R-R3-23 source complete for status/health/retry; task 5a items 2 and 3 open | Persistent codec/status/health display and Retry are implemented and evidenced; see [remote audio status verification](2026-09-20-remote-daemon-r3-verification/remote-audio-status.md). Fixed 24 kbit/s stereo is still active; the measured 24/48 comparison and any quality selector remain open. A selectable quality profile needs an acknowledged Core contract; no adaptive-rate claim. |
| Boot and radio recovery | R-R3-26 installed; R-R3-27/29 installed in `55e7d49f`; physical acceptance open | All 698 executables pass. Normal restart exercised GUI authentication before Saturn I/Q arrival. The Rock then became unreachable by SSH/ping; host diagnosis, late-network boot and physical radio-loss recovery remain pending. R-R3-27 was observed twice on 2026-09-23 (08:24-08:35 at build ac4c63aa, where a Core started while its radio was down found it later and played audio with no restart, and again at 20:15 at build 44584133); acceptance still needs the slice-retention check. See [evidence](2026-09-20-remote-daemon-r3-verification/radio-recovery.md). |
| R3 completion plan and 2026-09-23 hotfix plans | Stacked on this plan, third lane worktree, branch codex/remote-filter-policy | The 2026-09-24 R3 completion plan (docs/architecture/2026-09-24-r3-completion-plan.md) closed the amplifier applets' Disconnect/Reconnect in a remote window (Task 1, 16ba6687), corrected the Network Watchdog setting to follow Thetis source (Task 2, f92f19db), and added the VAX page's remote-audio-compressed note (Task 3, c236a9a0). The 2026-09-23 plans delivered, ahead of this plan's own boxes: the R3 NR3 model crash hotfix, the R3 controls-that-work plan and its fix wave, the R3 receiver-load hotfix, the R3 unfinished-controls plan and its fix wave, the R3 remote zoom-flash hotfix (25df3195, closing part of the D1 crop/zoom box above), the R3 remote radio hardware plan's Task 6, and the R3 Linux suite fixes plan (the two flakes closed in 8f8ae6d6, 5766dd36, 93567686, credited above against the full-suite gate row). |
