# Optional microphone capture lifecycle

**Status:** approved by the operator on 2026-09-22. Asked in plain terms, the
operator chose "a small helper program inside NereusSDR runs the mic" over a
background thread inside the app or opening the mic only on demand, together
with the operator behavior below (receiving starts right away, a mic problem
shows in Audio settings with Retry, PC-mic transmit waits for a ready mic and a
fresh PTT press, and the selected mic is never silently replaced). The
implementation plan fixes the exact transport bounds and deadlines and runs the
native permission/latency experiment before production capture is replaced.
The source-intent prerequisite is complete at `5a1c7ef3`. **Requirements:**
R-R3-36, preserving R-R3-06/07/17 and the parent architecture's local desktop
Core/DSP and future R4 microphone transport.

Design workflow: `yonder-brainstorming`; implementation planning and execution
use `yonder-writing-plans` and `yonder-cost-aware-execution` after approval.

## Outcome and established evidence

A microphone that cannot open must not delay receiving, reconnecting,
disconnecting or exiting. The selected microphone must be the one actually
used. Local users retain microphone metering, Test Mic, VOX and ordinary PC-mic
transmission; the remote GUI later reuses the same capture owner for R4.

An installed Mac app spent 541.93 seconds at optional input opening before a
PortAudio error. Related lifecycle tests stopped at the same boundary. The
exact native call was not stack-proven, so this design isolates initialization,
device resolution, open, start and close together, rather than assuming only
`Pa_OpenStream` can stall.

Current source:

- `AudioEngine::start()` opens speakers and then calls `ensureTxInputOpen()`
  synchronously. `setTxInputConfig()` also opens synchronously after destroying
  its prior bus. The hardware-owning daemon follows the local AudioEngine path.
- `PcMicSource` only delegates to `AudioEngine::pullTxMic`; it is not a second
  capture backend or cancellable opener.
- Devices configures persisted `audio/TxInput`. The PC Mic page edits separate
  TransmitModel session properties, and Test Mic currently starts only a meter
  timer. These surfaces do not reliably configure/test the same actual input.
- `PortAudioBus` silently resolves an unavailable named device to a default.
  It already contains the required native-rate resampling; preserve that DSP.
- The source-intent fix makes unavailable PC input silence in ordinary and RADE
  paths. It does not implement readiness admission or safe bus replacement.
- `MoxController::setMox(true)` invokes RadioModel's precheck before interlocks,
  state advance, relay/MOX effects and TX. Release does not consult that check.
- The existing RadioModel teardown stops AudioEngine before the TX worker.
  Capture storage cannot be retired in that order while a worker can read it.

## Alternatives and recommendation

| Approach | Result | Remaining problem |
| --- | --- | --- |
| Open synchronously only on demand | Removes optional input from RX startup | Selecting/testing a microphone can still freeze its caller and shutdown |
| Open on an owned thread in the Core process | Keeps the caller responsive during open | Native open is not cancellable; joining can hang and detaching can race ownership/PortAudio termination |
| Isolate native capture in a child process | Keeps native capture lifetime separate and allows failed capture to be terminated | Adds process supervision, bounded PCM transfer, packaging and native permission acceptance |

**Recommendation:** a supervised capture child, with an event-driven parent
transport on an owned I/O thread. The parent I/O thread never calls native audio
APIs and never waits for a native opener. Native audio callbacks never block on
IPC. Keep the current input backend/resampling in the child rather than changing
audio behavior and lifecycle simultaneously.

PortAudio documents abort/close for an existing stream, and returns a valid
stream handle only after successful opening. Its threading guidance also
requires serialized open/close. Neither establishes safe cancellation of an
in-progress open. Sources: [PortAudio API](https://files.portaudio.com/docs/v19-doxydocs/portaudio_8h.html),
[PortAudio threading guidance](https://github.com/PortAudio/portaudio/wiki/Tips_Threading).

## Operator behavior

1. Receive startup completes without waiting for PC capture. A stored PC-source
   preference alone does not open a microphone in an idle client or daemon.
2. An active local radio session that permits TX and has PC mic selected
   automatically prepares that input after receive startup. This preserves
   ordinary ready-to-use local operation and live mic metering without an extra
   arm button. The conjunction of active session, authority and selected source
   is the capture demand; restoring a preference alone is not.
3. Test Mic obtains a separate capture demand while its page is testing, even
   before radio connection. It releases that demand on Stop Test, page closure
   or destruction. One consumer ending does not stop another active consumer.
4. PC VOX requires the same prepared input before detector operation. A muted
   mic does not release capture: mute continues to mean silence while readiness
   and metering remain observable according to existing meter behavior.
5. Show **Preparing microphone**, **Microphone ready**, or a concrete failure
   beside the microphone controls. Failed state offers **Retry microphone**.
   Do not put capture diagnostics over a receive spectrum/waterfall.
6. A PC-dependent transmit request made before Ready is refused before all RF
   effects, with **Microphone is not ready. Check Audio settings and retry.**
   Becoming Ready never replays a queued PTT/MOX request; a fresh press is needed.
7. If the active PC input is lost or reconfigured during transmission, publish
   unavailable input immediately and use the existing orderly unkey path. Hold
   zeros throughout release. Never switch implicitly to the radio microphone.
8. Failure remains visible until an explicit retry, device change or genuinely
   new eligible session. Do not retry indefinitely or keep stale speech queued.
9. Named-device failure reports that device as unavailable. An explicit Default
   selection still permits normal default-device resolution. Speaker fallback
   remains independent.
10. Remote receive capability opens no TX capture on either endpoint. Later R4
    enables capture on the client that owns its microphone; it must not turn a
    client device preference into a request to open that device on Core.

## Ownership and interfaces

Proposed source units (names are design interfaces, not existing classes):

- `src/core/audio/CaptureSupervisor.{h,cpp}` owns demand leases, configuration,
  generation, status and an owned event-driven I/O worker. It publishes
  `CaptureStatus { state, configuredDevice, actualDevice, reason, generation }`.
  States are Closed, PreparingPermission, Opening, Ready, Failed and Stopping.
  Readiness includes the first valid PCM from the current generation.
- `src/core/audio/CaptureAudioBus.{h,cpp}` exposes a stable parent-side PCM
  reader/meter to AudioEngine. Its queue exists for the worker's full lifetime;
  native streams/pointers never cross into this process. Unavailable state and
  generation changes make reads return no frames and retire buffered speech.
- `src/capture_main.cpp` runs the native input owner with PortAudioBus and its
  existing resampler. It constructs no RadioModel, DSP channels or widgets.
  Native init/open/close and input enumeration stay in the child.
- `AudioEngine` owns the supervisor and stable reader. Its start path does not
  eagerly open input. Configuration updates go through the supervisor instead
  of resetting a native bus while an audio worker may read it.
- `RadioModel` derives active-session capture demand and composes the readiness
  check into the existing MOX precheck. It derives the actual resolved source:
  active TCI, VAX, radio microphone and generated Tune/two-tone do not depend on
  PC capture. Existing band-plan, interlock and remote-capability checks remain.
- Audio Devices and PC Mic/Test Mic bind to one `AudioDeviceConfig` owner under
  `audio/TxInput`. Legacy TransmitModel properties project that config instead
  of remaining a second device selection. Preserve persisted values/defaults.

On disconnect: cancel TX intent and perform ordinary release, stop/quiesce the
TX worker, withdraw capture demand, retire samples/status, stop the child and
then release reader storage. Asynchronous completions carry generation and
session identity; a retired result cannot publish Ready into a new connection.

Reconfiguration deliberately retires the old input before preparing the new
one. Keeping old capture active while displaying a new selected device would
misrepresent the source. On failure, show Failed for the requested device.

## Process and PCM contract

Use inherited private process pipes, without a network listener. Parent control
messages configure/open/stop a generation; child status identifies protocol
version, generation, actual selected device and negotiated format. Framed PCM
records include generation, monotonically increasing frame position, frame count
and canonical 48 kHz mono float samples. Reuse existing mono extraction and
native-rate resampling behavior; do not add a new gain/downmix algorithm.

Every parser has explicit maximum record/storage bounds and rejects malformed
lengths, unsupported formats, non-finite samples and stale generations before
publication. Control/error records remain distinct from PCM and child diagnostic
logging goes to stderr. There are no unbounded queued Qt PCM signals or buffers.
The transport thread feeds the stable reader directly, independent of GUI event
processing. The native callback only writes its bounded local ring.

The implementation plan must specify and test record sizes, queue bounds,
write/backpressure handling and deadlines as one contract. Reuse existing
capture buffer settings; measure added PCM delivery delay before selecting the
transport drain interval. Do not turn existing ring capacity into a new latency
target. Overflow/discontinuity must retire stale buffered speech and expose
unavailability under the same release policy; it cannot replay a long backlog.

Permission is a distinct PreparingPermission phase. Do not count a legitimate
OS consent prompt as a hung native open. After permission resolves, supervise
open/first-PCM and stop with explicit bounded deadlines. On expiry, invalidate
the generation and terminate the child. Its control watchdog must also exit
after parent pipe closure even when the native owner is stuck, avoiding an
orphan microphone process after parent failure. Never kill an unrelated process
by name, detach a parent native-opener thread, or call Pa_Terminate across one.

## Packaging and boundaries of the guarantee

Build `nereus-audio-capture` from the same revision as its parent. On macOS,
stage it in the app's Helpers directory with correct private-library resolution;
sign nested content before the containing bundle. Package it with the desktop
on Linux/Windows. The nereusd install component carries no helper, since the
daemon never opens a microphone.

Permission identity must be observed for the signed application and its helper,
including deny/retry behavior. Apple documents the input entitlement and usage
description requirements, but that is not evidence that a new helper inherits
the existing grant. Do not reset TCC or alter system permission databases to
make a test pass. Sources: [Audio Input entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.device.audio-input),
[media authorization](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos).

This boundary isolates optional capture. Parent-side output initialization,
speaker opening and VAX are separate native operations; this design does not
claim that every host-audio API is now asynchronous. If measurements show another
blocking boundary, track and fix that boundary explicitly.

## Acceptance and implementation handoff

- A fake child that never completes open cannot delay radio receive startup,
  reconnect, disconnect or exit. A stale success after cancellation is ignored.
- Child crash, malformed records, partial reads/writes, backpressure, no first
  PCM, input loss, reconfiguration and parent death have deterministic tests.
  No stale speech or Ready state crosses generation/session boundaries.
- Rejected PC-dependent keying emits no hardwareFlipped(true), antenna/MOX
  change, TX worker start or RX withdrawal. Unkey remains available in every
  failure state. TCI/VAX/radio/generated paths preserve their existing checks.
- Devices and PC Mic show the same actual configuration. Test Mic really opens
  that input, displays readiness/failure and releases only its own demand.
- Native acceptance covers packaged macOS permission and named-device identity,
  Windows/Linux child lifecycle, input continuity/added latency, and the Rock
  session reconnect. A successful fake test does not satisfy these checks.
- Safeguarded hardware TX remains a separate operator checkpoint; this work
  must not enable the unimplemented remote TX protocol ahead of R4.

This architecture and operator behavior were approved on 2026-09-22. The
implementation plan fixes exact IPC/lifecycle parameters
and file/test ownership, including a native permission/latency experiment before
replacing production capture. Unproven native behavior is an explicit dependency,
not a guessed API contract. R3 completion and the full Core/GUI goal remain open.
