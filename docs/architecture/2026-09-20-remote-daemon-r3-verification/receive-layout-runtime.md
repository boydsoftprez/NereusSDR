# R-R3-34: Core receive-layout startup and recovery

This follows the [store](receive-layout-store.md) and
[passive hydration](receive-layout-hydration.md) checkpoints. Core now owns
saved receiver membership throughout startup, resource admission, persistence,
same-radio recovery and authenticated GUI reconciliation. The authoritative
contract is the [receive-layout design](../2026-09-22-core-receive-layout-design.md).

## Implemented behavior

- Load once for the selected radio MAC, before configured receivers can replace
  its saved layout. An unspecified identity stays pending until discovery.
  Saved membership overrides both configured-count top-ups; removing B remains
  effective after restart even with a larger configured count.
- Preserve stable IDs, QObject identity for retained members, descriptor order,
  frequency/mode, pan bindings and compatible legacy preferences. Active focus
  remains a separate identity. Offline hydration starts no DSP or RADE decoder.
- Admit stable IDs against board channel capacity and allocate independent pan
  streams. Later members join only their own pan's window; they cannot silently
  borrow another pan's DDC. Refused members are removed with an explicit reason.
  All-refused layouts use ordinary configured fallback receivers.
- Keep corrupt, unsupported and partially refused records intact. Pending and
  protected restoration cannot overwrite them. Accepted changes use the existing
  atomic settings save, visible failure/retry path and synchronous stop flush.
- Start the explicitly saved RADE receive owner after DSP and worker creation,
  including B while A owns TX. Reconnect refreshes worker generations while
  retaining live tuning/objects. Recovery does not repeat destructive startup
  admission: independent pans reclaim separate streams, while live members
  that diverged within a pan may reclaim additional available streams. A
  missing owner cannot silently promote another
  RADE-mode receiver during capture. A channel-0 pool object remains dormant when
  no stable receiver A exists.
- Reseed a connected GUI through the full station snapshot. Retained locked
  slices accept authoritative frequencies without unlocking operator controls.
  Media/session handshake runs once per epoch; a separate notification marks
  subsequent state snapshots. Genuine operator edits wait until the snapshot
  completion marker, while inbound updates do not echo.
- Mirror read-only restoration state/detail. Reset optional status at new
  attachment for compatibility with older Core builds. Core connection details
  retain refusal reasons while connected, with a warning when receivers cannot
  be restored.

## Automated evidence

`tst_receive_layout_runtime` uses real AppSettings file saves/reloads and two
DaemonApp lifetimes with the real board capability table and allocator. It
covers count precedence, deletion, sparse IDs, MAC isolation, offline pending,
invalid/degraded protection, immediate shutdown capture, same/different pan
allocation, all-refused fallback and failed atomic-save retry.

`tst_receive_layout_native` uses actual WDSP channels, workers, RADE and a P1
loopback radio, with simulated host audio devices. It starts and reconnects
layouts `{2,4}` without A and A-USB/B-RADE with A owning TX. It verifies active
channels, exact tuning/modes, no phantom active channel 0, stable objects/live
edits on recovery, RADE ownership/generation, and owner removal without implicit
promotion. The sparse-ID row also uses two independent pans with overlapping
frequency windows and checks that their DDCs remain separate after reconnect.
This is runtime integration evidence, not decoded on-air speech or
physical speaker acceptance.

`tst_receive_layout_session` authenticates StationServer and StationClient over
the existing loopback transport. It verifies complete silent-preference
hydration, stable/shared identity, rejected read-only writes, one handshake and
media epoch across reseeding, a deterministically held snapshot marker with an
interposed operator edit, and compatible reconnect without newer status fields.
The locked-frequency regression first failed with the GUI at 14.225 MHz while
Core restored 14.2932 MHz, then passed with the authoritative apply path; normal
operator tuning remains locked until explicitly unlocked.

The consolidated review identified four accepted corrections: locked snapshot
frequency, edits during a reseed, later same-pan placement and optional status
on older peers. All have focused regression evidence. Its proposed selection
of the first descriptor as active was declined: the design explicitly keeps
descriptor order separate from a retained active identity.

The initial integrated eight-target run passed in 62.78 seconds. The additional
native owner-removal test passed in 44.38 seconds. After review corrections,
runtime and allocator checks passed, and the final session checks passed in
1.40 seconds. The final matching application/daemon/all-tests build passed.
The first unfiltered 724-test run passed 721 tests in 329.17 seconds, with
build-start load averages 3.14/3.74/4.35 and test-start load 16.14/8.23/6.02.
All five new hydration/runtime/session/native targets passed. The three failures
were `tst_daemon_radio_recovery` (a receiver-identity comparison reached a deleted
QObject), `tst_port_audio_bus` (default input open timed out), and
`tst_audio_engine_speakers_live_reconfig` (speaker re-open timed out). The recovery
failure was traced to startup admission being applied again during in-process
recovery: a live receiver outside its original pan window was removed instead
of retained. The native device failures remain open and are not counted as
passing. Logs: `r3-receive-layout-final-build.log` and
`r3-receive-layout-final-tests.log`.

The recovery correction passed all four focused targets in 50.08 seconds:
`daemon_radio_recovery`, `receive_layout_native`, `receive_layout_runtime` and
`slice_stream_allocator` (each with the `tst_` prefix). Identity assertions now
use guarded pointers and verify membership count, so an actual deletion reports
a test failure instead of crashing QtTest's QObject formatter. The new native
case retains independent streams even when the two pans overlap in frequency.
The final matching application/daemon/all-tests build passed. The unfiltered
rerun passed **722/724** in **307.05 seconds**, with build-start load averages
1.57/3.62/5.06 and test-start load 8.14/4.95/5.49. The recovery regression and
all receive-layout/session/GUI checks passed. The unchanged
`tst_port_audio_bus` and `tst_audio_engine_speakers_live_reconfig` each reached
native input opening and timed out at 120 seconds; this is not a full-suite
green result. No tests were excluded and no timeout or assertion was weakened.
Logs: `r3-receive-layout-recovery-final-build.log`,
`r3-receive-layout-recovery-final-tests.log`, and the corresponding load log.
The local source checkpoint is reviewable; native device and hardware gates
remain open.

## Native host-audio investigation

The earlier passive full-suite attempt timed out in four native startup tests.
An independent installed application had already spent 541.93 seconds opening
its default microphone before reporting an internal PortAudio error. The
reproduced tests stopped at the same capture-open boundary; neither hydration
nor the changed radio lifecycle ran before that boundary. Debugger attachment
also stalled, so the exact blocking interval inside `Pa_OpenStream` is inferred
from the before/after logging, not a captured stack.

Lifecycle fixtures now install opened fake speaker/microphone devices before
every AudioEngine start, including reconnect. A test-only initializer persists
across stop while ordinary bus teardown remains intact. Real DSP, radio,
worker and reconnect assertions are unchanged. The explicit native PortAudio
device tests remain unchanged. No OS/audio service changes were made.

Production microphone startup behavior is unchanged. Its separate receive
responsiveness gap is R-R3-36; a non-cancellable native open requires a safe
lifecycle design, not merely a timer around an operation that is still running.

## Hardware acceptance pending

No new Core installation, GUI relaunch, radio/tuner operation or public branch
mutation is claimed. The Rock two-receiver restart, authenticated GUI resume,
mixed ordinary/RADE listening and sustained receive checkpoint still require
operator hardware acceptance. R-R3-34 and the overall Core/GUI goal remain open
until those boundaries are satisfied.
