# Thetis CAT compatibility and setup design

- Date: 2026-10-04
- Status: proposed written design for maintainer review; implementation has not started.
- Source baseline: Thetis v2.10.3.15, commit `3759d096`.
- Author: J.J. Boyd (KG4VCF), with OpenAI Codex assistance.

## Intended result

Logging software, digital-mode programs and control hardware should be able to
control NereusSDR using the standard and extended CAT commands provided by the
configured Thetis upstream. The maintainer approved proceeding with the proposed
full command-layer and serial/TCP setup design on 2026-10-04. This document makes
the architectural choices concrete for review before implementation planning.

Preserve Thetis wire behavior where the underlying capability exists. Adapt its
global Console and two-VFO assumptions to NereusSDR's independently modeled
slices and single transmitter. Account for every upstream descriptor, including
inactive commands, upstream defects and commands for missing Nereus capabilities.
Do not describe catalogue coverage as full functional parity.

## Source findings and alternatives

The existing Phase 3K roadmap calls for four slice-bound rigctld channels, TCP
CAT and CAT configuration. Current `CatApplet`, `CatSerialPortsPage` and the
Tools CAT action are placeholders. The CAT PTT entry point already exists in
`MoxController::onCatPtt`.

Thetis sources under `Project Files/Source/Console/` establish:

| Source | Finding |
| --- | --- |
| `CAT/CATStructs.xml` | 419 distinct descriptors; 348 active (39 standard, 309 extended `ZZ`), 71 inactive. |
| `CAT/CATParser.cs:410-593` | Semicolon framing, case-insensitive prefixes, activity and field-width validation, payload exceptions and extended dispatch. |
| `CAT/CATCommands.cs` | 10,591 lines of handlers; many directly access Console controls or Setup forms. Standard commands often delegate to extended counterparts. |
| `CAT/SIOListenerII.cs:31,324,556,784` | Four ordinary serial listeners, all controlling the same global Console. |
| `CAT/SIOListenerII.cs:1013,1187,1361` | Additional dedicated Andromeda, Aries and Ganymede listeners. |
| `CAT/SDRSerialPortII.cs:186-234`, `CAT/SerialPortPTT.cs:34` | Serial pin configuration and bit-bang PTT/keying. |
| `CAT/TCPIPcatServer.cs:275-298,539` | Stream framing, GUID registration/removal and directed device messages. |
| `console.cs:15657` | Thread-safe parser entry marshals work onto the Console UI thread. |
| `console.cs:2335`, `setup.cs:22396`, `setup.Designer.cs:58812` | Effective TCP default is `127.0.0.1:13013`; the transport class's fallback port is not the application default. |

Three approaches were considered:

1. **Full catalogue with model adaptation, recommended.** Build one faithful
   Thetis command layer and native transports, with a reviewed outcome for every
   command. This establishes a maintainable compatibility boundary.
2. **Logging-client subset.** Faster initial delivery, but leaves extended CAT
   behavior unspecified and requires revisiting parser/model boundaries later.
3. **Setup controls alone.** Enables configuration UI work but does not deliver
   external radio control while the parser and transports are absent.

Use Thetis for CAT semantics. Study AetherSDR's current `CatPort`,
`RigctlProtocol`, `SmartCatProtocol` and `SmartCatSession` for native Qt transport
and endpoint patterns. The roadmap's older `RigctlServer`/`RigctlPty` class names
have drifted. AetherSDR's SmartSDR command semantics are not authoritative for
OpenHPSDR or Thetis behavior.

## Scope and delivery boundaries

The requested result includes the complete Thetis command inventory, native
serial and TCP CAT, associated compatibility/PTT settings, and working CAT UI.
Implement in dependent stages within that scope:

1. Catalogue, parser, model adapter and ordinary logging/control commands.
2. Serial/TCP sessions, reporting, settings and live UI integration.
3. Remaining extended families supported by existing Nereus models, with
   production-model integration tests and the final compatibility report.

Hamlib rigctld remains a distinct additional dialect in Phase 3K. Give it the
same model adapter and explicit slice bindings, with its own framing, errors
and tests. The implementation plan must name its delivery separately so a
working rigctld server cannot be mistaken for a completed Thetis CAT port.

Account for dedicated Andromeda/Aries/Ganymede messages in the catalogue. Their
device setup, unsolicited traffic and hardware workflows require separate
integration work and hardware verification. MIDI mapping and CAT scripting
engines are also separate integrations. This design does not silently fold
those subsystems, audio routing, or recording into the first CAT implementation.
Missing capabilities remain documented as unavailable; adding their underlying
features requires a scoped design rather than fabricated readbacks.

## Component and thread boundaries

Place the command catalogue, parser, session state, transports and model adapter
under `src/core/`. None includes GUI headers. Use concrete typed APIs for
production model operations, so successful parser tests cannot hide missing
runtime method bindings.

Proposed responsibilities:

| Component | Responsibility |
| --- | --- |
| `CatCommandCatalog` | Immutable descriptor widths, activity, validation kind and handler identity. |
| `CatParser` | Validate a complete command and format the appropriate result; no GUI or socket operations. |
| `CatModelAdapter` | Resolve endpoint targets and perform model operations, conversions and real state readback. |
| `CatSession` | Own per-client stream buffer and protocol-local state, including registrations and reporting subscriptions. |
| `CatService` | Own endpoint configuration, transport lifecycle, reporting routes and CAT PTT claims. |
| Serial/TCP/PTY transports | Deliver bytes and expose actual connection/error state; delegate parsing and model work. |

Use existing Qt event-loop patterns for asynchronous network and serial I/O.
Model access occurs on the main/model thread. If a platform transport needs a
worker, it emits queued signals containing complete requests; it cannot touch
models or invoke GUI-thread work synchronously. CAT creates no DSP thread and
does not place a mutex in an audio callback.

The service has no dependence on `MainWindow` construction. Its lifecycle must
be usable by a local console and a headless Core host. A remote client must
route commands through the authoritative Core's existing control interface;
it must not keep a second independent radio state. If the current remote
interface cannot express an operation, document that capability gap before
planning its extension. Do not add a new remote protocol inside the CAT parser.

## Endpoint, VFO and slice semantics

Each ordinary endpoint has an explicit primary slice ID and optional secondary
slice ID. Store stable IDs, resolved through `RadioModel::sliceById`, rather than
list positions. Endpoint assignments are independent of GUI focus and labels.
Thetis VFO A/RX1 operations address the configured primary; VFO B/RX2 operations
address the secondary where the particular upstream handler has that meaning.
Record exceptions in the per-command matrix rather than applying a guessed
generic mapping to all extended commands.

Do not create a slice as a side effect of opening a connection or querying VFO
B. Missing targets produce the command's documented unavailable/error outcome.
Removing a bound slice invalidates its binding rather than retargeting the
next slice that occupies the same list position. Changing endpoint assignments
requires stopping that endpoint first and clearing its session state.

The first endpoint proposes primary slice A and optional secondary slice B
when those stable identities exist; all endpoints and transports start disabled.
Additional endpoint bindings require explicit selection. This is a proposed
new CAT default, not a change to existing VFO or DSP defaults.

For actual two-frequency split operation, keep the primary receive slice tuned
and hand transmit selection to the secondary through `TxSliceArbiter`. Simplex
selects the primary. Read back the actual arbiter and model state, not a
session-local success flag. Handoff obeys existing unkey-before-handoff
behavior. A later TX request waits for confirmed handoff and is rejected if
the binding is invalid. A queued handoff cannot cause a cancelled or disconnected
session to transmit later.

This deliberately introduces CAT split compatibility using two real slices.
Existing TCI VFO-B and split shims currently omit that behavior; changing TCI is
a separate scope decision. XIT/RIT continue to use the existing model setters
and upstream scaling. Do not emulate arbitrary split tuning with XIT limits.

## Transmit control

All endpoints share one physical transmit chain. TX/RX commands use CAT's
existing `MoxController::onCatPtt` path after resolving the selected TX slice
and existing interlocks. They never set a radio keying bit directly.

The existing PTT slots are last-setter-wins, not shared source arbitration
(`MoxController.h:567`, `MoxController.cpp:1223`). Adopt a defined CAT ownership
contract rather than introducing global PTT refcounting in this port:

- Accept the first CAT TX claim only when the transmitter is idle and any
  required handoff is confirmed. Reserve that target during pending handoff.
  Reject CAT takeover or handoff while another PTT source controls transmit.
- Track accepted claims by session or serial pin source. Additional clients may
  claim the same TX slice; reject claims for a different target. Reject CAT
  split/simplex/handoff changes while a CAT claim or another source's TX is live.
- A release, disconnect, endpoint stop, radio disconnect or transport error
  removes that source's claim. The final claim releases MOX only while CAT still
  owns the same activation. Add an ownership guard to `onCatPtt(false)` analogous
  to the existing guarded microphone release, instead of its current
  unconditional `setMox(false)`.
- Another source taking over, a UI/TCI TX handoff, or an external unkey invalidates
  all prior CAT claims and pending activations. Never automatically rekey from
  those stale claims. Require a fresh explicit TX command; pin PTT requires a
  release/new assertion. Cancellation during CAT's own final release is
  idempotent and cannot emit another activation.

This deliberately permits non-CAT control to interrupt CAT and does not promise
to preserve simultaneous held requests across all sources. It guarantees CAT
cleanup cannot release a newer non-CAT activation. Preserve existing non-CAT
ordering and test concurrent mic/TCI/VOX events and same/different-slice clients.
The implementation plan must define activation identity and event ordering
against the actual `pttModeChanged`, MOX and arbiter signals before coding.

Opening a port, restoring settings or connecting to a radio never keys it.
Thetis's legacy PTT labels describe RTS/DTR wiring but sample input lines:
`PTTOnRTS` reads `CtsHolding` (CTS), and `PTTOnDTR` reads `DsrHolding` (DSR),
at `SDRSerialPortII.cs:234` and following pin-processing code. Preserve that
mapping, make the actual sampled input clear in setup, and keep output RTS/DTR
assertion separate. An initially asserted CTS/DSR input is not treated as a
fresh transmit request: require a release followed by a new assertion after
the endpoint is ready. This is an explicit startup behavior adaptation. Pin
polarity and supported facilities come from the serial sources and platform APIs,
with distinct configuration for CAT bytes and PTT.

## Parsing, transport and reporting behavior

Use one session buffer per TCP client/serial endpoint. Handle fragmented
commands and multiple commands in one read without crossing client boundaries.
Normalize command prefixes only; preserve payload case/content wherever
Thetis permits text, EQ, GUID or other special suffixes.

Port upstream lengths, signs, zero padding, answer prefixes and setter silence.
Preserve inactive-command rejection and command-specific error behavior.
Validation precedes mutation. A rejected request leaves the underlying model
unchanged, and every query reads actual model state.

Derive maximum command sizes from the catalogue and variable-payload handlers.
Do not blindly port TCP's 255-character residual-buffer reset if it truncates a
valid supported command. Use a bounded documented recovery policy that tests
oversized and subsequent valid commands. Record any resulting upstream
divergence in source, provenance and the compatibility report.

Implement `ZZGA`/`ZZGR` registration and directed outbound messages at the
session/service boundary. Preserve Thetis automatic-information enablement and
destination choices; emit from model changes and avoid request echoes or
duplicate notifications. Record source behavior and any multi-endpoint
adaptation explicitly. Logs show actual requests/replies and transport state.

Serial support uses `Qt6::SerialPort` when available, with clear unavailable
state when the optional dependency is absent. Physical serial ports work on
macOS, Linux and Windows. PTYs are an explicit optional transport on platforms
supporting them; a Windows virtual COM pair is a user-provided device, not an
automatically installed driver. Implementation planning must verify platform
facilities and resource cleanup before selecting PTY APIs.

## Settings and UI

Use `AppSettings`, PascalCase keys, and `"True"`/`"False"` booleans. Persist
endpoint configuration and CAT preferences, not transient client registrations,
PTT, live meters or antenna state. Define a single settings schema in the plan
and reuse it for all UI and service consumers.

Wire the existing Serial Ports page, CAT applet, Tools action and CAT status
indicator. Add native TCP and compatibility configuration using established
Setup patterns. Show enabled/listening, connected, stopped and error states
from service signals. Use model update guards or `QSignalBlocker`; do not have
UI writes and settings restoration reopen the same endpoint twice.

Serial options are source-grounded at `setup.Designer.cs:57973,58053,58069,58083`:
baud 300/1200/2400/4800/9600/19200/38400/57600/115200; parity
none/odd/even/mark/space; bits 8/7/6; stops 1/1.5/2. Unsupported platform
combinations are unavailable with a clear reason. PTT options come from
`setup.cs:1140` and `setup.Designer.cs:57437`. Rig identification options
PowerSDR/TS-2000/TS-50S/TS-480 come from `setup.Designer.cs:59524` and
`CATCommands.cs:291`; they change reported identity, not underlying command
availability. Compatibility options are inventoried at
`setup.Designer.cs:59330-59565`.

Use the effective upstream TCP default `127.0.0.1:13013`. Listener enablement
defaults to off. The proposed welcome-banner default is off, matching
`console.cs:2393`; Thetis Setup also initializes it true at `setup.cs:385`, so
this is an explicit choice rather than a claim of unambiguous upstream parity.
Do not change existing display, DSP, radio-connect or audio defaults.

## Compatibility matrix and upstream corrections

Before handler implementation, capture all 419 descriptors in a checked-in
matrix. Each row contains command activity; read/set forms and widths; source
handler and versioned lines; target model/API; input/output scaling; representative
wire fixtures; and one outcome: faithful implementation, documented adaptation,
inactive, upstream inert behavior, or unavailable underlying capability.
Unavailable active commands need an exact tested rejection/readback contract;
do not pretend success with a stored value that has no effect.

`ZZMX` is active and implemented at `CATCommands.cs:4394` but missing from parser
dispatch: plan its handler registration as an upstream correction. `ZZDY` is a
commented-out parser case, not another dispatch defect. `AN` is explicitly
inactive in the pinned XML; do not enable it based on an older Nereus design.
`ZZFX` (`CATCommands.cs:3054`) and `ZZTS` (`:6881`) have disabled hardware
operations; preserve/document their actual source behavior instead of inventing
hardware support. A fully classified matrix is required, but it does not prove
missing recording, VAC, controller or other features work.

## Attribution

Follow `docs/attribution/HOW-TO-PORT.md` before every code port. Each destination
gets the exact headers from each upstream file it materially derives from,
the dated Nereus modification history, source block/constant cites stamped
`[v2.10.3.15]`, and same-commit provenance registration. Preserve every inline
comment and author tag; retain exact constants as named `constexpr` values.

Important header distinctions: `CATCommands.cs:1-27`, `CATParser.cs:1-21`,
`SIOListenerII.cs:1-21`, `SDRSerialPortII.cs:1-21`, `SerialPortPTT.cs:1-21` and
`SerialRxEvent.cs:1-8` carry different notices. `TCPIPcatServer.cs:1-6` has a
short author/inspiration header rather than a full per-file GPL block;
`CATStructs.xml` has no per-file header. Preserve actual notices and cite the
project-level license where applicable. Ports from `console.cs:1-56`,
`setup.cs:1-44`, `clsCATMessageQueue.cs:1-39` or `clsCatAtonic.cs:1-39` must also
retain the applicable Samphire dual-license statement. Do not substitute one
file's header for another's.

## Verification and acceptance

Read `docs/development/fast-test-loop.md`; build matching test targets before
running them. Begin with focused CAT targets, then required regression and
integration checks. No tests or application build are needed to review this
documentation-only design; it does not claim runtime verification.

Acceptance for the implementation:

- The matrix accounts for every descriptor and every enabled handler; all
  unavailable capabilities and divergences appear in the delivered report.
- Meaningful parser fixtures cover read/set formatting, extremes, bad values,
  inactive/unknown commands, text payloads, and no mutation on rejection.
- TCP/serial transport tests cover fragmentation, coalescing, separate clients,
  disconnect/restart, occupied ports, buffer recovery and device errors.
- Tests with production `RadioModel`, `SliceModel`, `TransmitModel` and the
  arbiter demonstrate real tuning/mode/filter/DSP changes, stable-ID binding,
  missing/removed slices, split handoff, real readback and settings restoration.
- TX tests cover interlock refusal, handoff completion/cancellation, clients
  sharing a target, conflicting targets, UI/TCI handoff during CAT TX, other PTT
  source takeover, stale release/rekey prevention, asserted CTS/DSR inputs at
  startup and disconnect cleanup.
- Automatic information and directed GUID reporting match the specified wire
  fixtures and use actual model state.
- CAT configuration works on macOS/Linux/Windows; platform limitations are
  reported. Headless ownership is verified when supported by the current Core
  host, and remote-interface gaps are explicitly recorded.
- Relevant core/model regressions, the core-no-GUI-includes check, attribution
  verifiers and required final repository checks pass on rebuilt binaries.
- Real WSJT-X/JTDX or logging-client testing and device/PTT testing are recorded
  separately from automated results; unperformed hardware checks stay unverified.

## Review and next step

The maintainer reviews this written design, particularly the endpoint slice
bindings, two-slice split behavior, CAT PTT claim handling and treatment of
separate controller integrations. Approval permits writing the implementation
plan. That plan must resolve each command's concrete mapping and unavailable
contract from the pinned sources, define the settings schema, enumerate target
files and test commands, and name the delivery boundaries. It must not expand
underlying DSP, recording, audio or remote protocols implicitly.
