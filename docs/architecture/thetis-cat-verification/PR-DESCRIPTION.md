# Add native slice-bound Thetis CAT and separate Hamlib rigctld

External clients can now control the local authoritative Nereus Core through
four configured Thetis CAT channels and four separate Hamlib rigctld listeners.
The channels target explicit stable A/B slices, read actual model state and
route supported changes through existing admitted model APIs. Native Setup,
CAT applet/status, bounded log and isolated tester expose live configuration
and actual errors. All listeners default off; first Thetis TCP proposes
127.0.0.1:13013, welcome defaults off, and rigctld requires a chosen nonzero port.

Scope is the approved 2026-10-04 design/plan against qualified Core
`27716f5d6700e1e7d1808acfe478756249828e8c`; whole-feature review starts at
`620883fde90b629ec15005fdac8d6ac7fcd43998`. Task 14 starts at accepted signed
`d8c4fc1f258966f8d78e24a807fe48b72457e63a`. This local packet does not claim
integration with a later Core candidate, completed lead review, hosted CI or
publication.

## Delivered behavior and source decisions

- The pinned Thetis v2.10.3.15 / `3759d096` XML, parser, owning CAT handlers,
  Console/Setup defaults, serial and TCP reporting sources establish widths,
  scaling, errors, literal switches and exact inert behavior. All 419
  descriptors have concrete contracts: 349 active registrations, 70 inactive;
  outcomes Faithful 15, Adapted 163, SourceInert 42, Unavailable 129. All 783
  current fixtures execute through the production service/models. Accounting
  does not promise missing Thetis functionality.
- Four logical channels share native TCP, optional Qt SerialPort and macOS/Linux
  PTYs. Each accepted client has independent framing/verbose/GUID state and
  frozen slice incarnations. Removing/reusing an ID invalidates its old binding;
  connection/getters never create slices or follow GUI focus. Explicit two-slice
  split selects the real TX arbiter target and preserves the primary RX tune.
- A Qt main/model-thread service owns configuration and transport lifecycle.
  `reconfigureChannel`/`reconfigureGlobal` mean accepted desired configuration;
  bind/open failure stays saved and visibly failed. Identical proposals do not
  restart or recapture. A channel tuple change retires that channel’s ingress,
  sessions, queued reports and exact claims, and rearms its relevant shared PTT
  input; unrelated channels keep running. Global PTT edits affect their relevant
  ingress. Newer callback configuration wins; retirement is permanent.
  `AppSettings` stores PascalCase keys and True/False strings, without transient
  PTT, registrations, runtime incarnations, meters or antenna state.
- Shared same-target PTT claims use the existing MoxController/station gate;
  Tune/TwoTone are exclusive. Actual accepted-request tags/generations and cycle
  cancellation guard native delayed work and cleanup. Disconnect, transport
  failure, stop and reconfiguration release only the owned activation. Newer
  operator/TCI/mic intent survives; stale CAT claims cannot rekey. CTS/DSR input
  polling preserves the legacy RTS/DTR label mapping and adds release-before-
  assertion startup/reopen arming. No output pin assertion is inferred.
- The existing CAT applet TCP/PTY buttons deliberately control CAT1, with explicit
  tooltips. Four indicators/path rows remain, while each channel has individual
  Setup controls. Tools CAT Control, status, log and tester are reachable through
  builtCat. Native normal/scaled Setup captures were inspected. Remote-role
  windows show host-local guidance; no remote configuration protocol is added.
  Tester uses the real parser/router and refuses PTT/Tune/TwoTone, VOX enable and
  PureSignal single/calibration actions.
- Hamlib is a distinct newline dialect, with short/long/command-local ERP
  replies and separate listeners/diagnostics; optional PTY selects exactly one
  dialect. It never receives Thetis welcome/AI/GUID behavior. AetherSDR
  `1e0718ad` supplied Qt/protocol patterns and supported contract ports; primary
  Hamlib 4.7 rig.h/rigctl_parse.c/netrigctl.c resolved errors, ERP reset/separators,
  independent RIT/XIT and mandatory NET open handshake. Native dump_state and
  chk_vfo advertise actual software subset and empty hardware lists, without
  Flex bands/power identity, create-on-demand B, cached/stub success or a generic
  protocol framework. Exact source notices, selected comments and same-commit
  consumed provenance accompany genuine derivatives.

Upstream corrections/adaptations are explicit in source/provenance/contracts:
retain full signed offset magnitude and return O; when width 5 cannot represent
it; prevalidate the full ten-band EQ payload; reject invalid configured serial
text width; canonical single GUID framing and strict suffix width; catalogue-
derived bounded framing instead of the TCP residual reset; session-local verbose
state; per-bound-VFO AI coalescing with the original 200 ms interval; omit the
first-500 IF/MD main-thread sleeps. ZZMX dispatch is present upstream—the earlier
missing-dispatch finding was an extractor error, and its native memory workflow
still returns unavailable. Native callback lifetime fixes preserve frequency
notification survival, valid MOX phase ordering and owned tone restoration.
Linux-specific kernel HUP and PTY slave-tail failures were reproduced before
native corrections; valid regressions were preserved.

## RX and native lifecycle contact

Frequency, CTUN, mode/filter, AGC/NR/NB/ANF/APF/squelch, frontend attenuation,
admitted diversity and off-air buffer controls reuse existing model parameter
and receiver wiring. Meter reads use existing ready channel/lane caches and
calibration. RadioModel/SliceModel and MOX/TwoTone helpers changed for inert CAT
ownership, accepted TX intent/cancellation and callback-safe notifications.
AudioEngine, RxChannel, WdspEngine and RadioConnection processing sources remain
unchanged across the feature. No I/Q/WDSP algorithm, signal routing, audio
callback mutex, DSP thread or authority-gate redesign was introduced. Software
model/network intent is distinct from observed received audio or RF delivery.

## Verification and limits

[The verification README](README.md) records fresh Task 14 application/named
builds, anchored gates, actual inventory, split non-realtime/realtime full suite,
normal signed hooks and compliance. [The ledger](progress.md) names each
accepted signed task commit, worker effort, corrections and measured evidence.
Raw attempts, including failed command/fixture/setup attempts, are retained
locally under `.crew/2026-10-04-thetis-cat-plan/task-N-*`.

Current macOS 27 arm64 / Qt 6.11 application and tests, Linux aarch64 / Qt 6.8.2
Core CAT targets, native PTY/QSerialPort behavior on owned test slaves, and the
independent no-SerialPort lane are distinct evidence. Retained pre-wave Task14 coverage on signed `025e7f365552594e441bc1f3c82a2420cb41892a` is
1120/1120 ordinary nonrealtime,11/11 serial native-window and22/22 serial realtime,
all CTestEXIT0;19,694Qt passes/0failures/374skips across1,151Qt blocks. Fresh
current-source Linux17/17 includes actual official Hamlib/native MOX execution
with1,333Qt passes and zero skips; independent noSerial13/13 has177Qt passes and
two explicit capture/client skips. The README preserves earlier failed epochs,
actual host samples, complete fresh LastTest logs and each skip category. Task 13 ran the actual
Hamlib 4.7.0 / `554e02b39` rigctl executable six times against current-source
owned loopback endpoints, observing F/f/M/m fresh readback, split S/I/i/X/x,
AF/RIT and T/t plus native MOX-on/disconnect release. Those tests use the
existing deterministic admission harness; they do not transmit RF.

The initial integrated review required five corrections: owned OFF after
unrelated primary loss in both dialects, native mode-callback lifetime and
supersession, complete combined Hamlib numeric prevalidation, desired global
command snapshots with functional PTT retirement continuation, and both
Windows PTY tooltip branches. The [consolidated wave](CORRECTIONS.md) records
actual causal reds and fresh focused greens, including replacement sampling
and one-attempt open/sampling failure handling. Native RX mode/filter
notification continuations changed; algorithms, values, routing and defaults
are preserved. Windows branch evidence is source-only with platform-correct
runtime assertions; actual Windows execution remains unavailable. Scoped
rereview and refreshed combined/platform acceptance remain pending.

Windows SDK compilation/runtime, Linux GUI/x86, physical cables/pins, other
Hamlib versions/apps, WSJT-X/JTDX/loggers, radio discovery/connect/RX/audio and
RF remain unverified. CI's added Qt SerialPort module lists are configuration,
not branch execution. Dedicated Andromeda/Aries/Ganymede/MIDI/scripting,
recording/CWX/VAC/memories, GUI-only display/recenter/filter identity and other
missing APIs retain explicit reasons/refusal rather than synthetic readbacks.

A PTY is one shared kernel stream: observed HUP/close/error/stop gives fresh
session cleanup, while a close/reopen entirely between observations can be
invisible. Hamlib 4.7 get_lock_mode unconditionally reads an extra record after
an error; missing frozen targets truthfully return ENTARGET (-12), which may
cause that client's timeout. No unlocked filler is returned. get_powerstat and
q remain outside the subset (-11). Final fresh integrated lead review and any
shipping/public action remain pending separately.

Final acceptance also restores the established native untyped Tune OFF release
contract while preserving explicit CAT/requester guards. The first full suite
exposed that a station-typed OFF could strand a remote/TGXL carrier; unchanged
state/ownership regressions now cover the correction. Retained applet VAX/IQ
controls remain disabled with precise capability reasons, and catalogue goldens
change only their three delivered CAT availability entries. The verification
README preserves the failed attempt, root causes and final measured reruns.
