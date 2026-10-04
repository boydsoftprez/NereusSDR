# Native CAT implementation verification

Local verification dated 2026-10-04, against original implementation base
`620883fde90b629ec15005fdac8d6ac7fcd43998` and qualified Core
`27716f5d6700e1e7d1808acfe478756249828e8c`. Final Task 14 base is accepted
signed `d8c4fc1f258966f8d78e24a807fe48b72457e63a`. Thetis remains
v2.10.3.15 / `3759d096`; AetherSDR is `1e0718ad`. No later Core integration,
publication is implied. The initial full-feature review found five required
corrections. The [consolidated correction wave](CORRECTIONS.md), based on signed
Task 14 head `025e7f365552594e441bc1f3c82a2420cb41892a`, records fresh focused
regressions. Scoped rereview and refreshed full/platform acceptance are pending;
the lead owns their acceptance and final document closure.

The full-suite and platform results below are retained pre-wave Task 14
evidence on that accepted signed head. They remain historical evidence while
the correction wave refreshes its combined production and platform gates.

The catalogue has 419 descriptors, 349 active registrations and 70 inactive
entries. Fresh `tst_cat_coverage` executes all 783 current request fixtures
through the production service and real models; there are zero pending replies.
Outcomes remain Faithful 15, Adapted 163, SourceInert 42, Unavailable 129,
Inactive 70. An active registration may implement a precise unavailable
contract. These counts establish complete accounting, not full Thetis
functional parity. [Fixture contracts](../../../tests/data/cat/README.md),
[command mapping](../cat/2026-10-04-command-mapping.csv),
[accepted ledger](progress.md) and [local PR description](PR-DESCRIPTION.md)
record the delivered scope and corrections.

The fixture `executionStatus` is preserved owning-family history (158 Task 5,
174 Task 6, 305 Task 7, 146 not executed by those earlier owners). The separate
`productionCoverageStatus` records all 783 executed by the combined production
target, including 140 inactive and six router records. Fresh execution output,
not metadata alone, establishes the final production coverage claim.

## Production contracts and acceptance mapping

| Approved requirement | Delivered implementation and actual evidence |
| --- | --- |
| Exact catalogue/parser/contracts | Raw pinned XML, source widths/scaling/literal switches and all 419 CSV/metadata codes reconciled; catalogue/parser/coverage targets execute. Semicolon framing, text case, malformed/inactive/no-mutation and 41-byte request cap have actual byte tests. |
| Stable endpoint A/B | Four channels hold explicit IDs and frozen runtime incarnations. Bindings/RX/integration/rigctld tests prove focus independence, missing B without slice creation, removal/reused-ID refusal and explicit rebind. |
| Actual model and authority | Adapter uses public SliceAccessPolicy/SliceOwnership/StationSliceFreeze and native typed setters; ready WDSP parameter/lane/meter tests and connection intent integration prove effects. No successful cached stubs or Take/authority replacement. |
| TX ownership and ordering | Coordinator shares only same-target PTT; tags/accepted generations, pending-admission cancellation and native Tune/TwoTone cycles preserve newer operators. Ownership/MOX/arbiter/TCI/tone gates prove final release, conflict/refusal, lifecycle cancellation and reentrant supersession. |
| Native transport and reporting | Real owned loopback sockets/QSerialPort PTY slaves/POSIX PTYs exercise fragmentation, queues, errors, first command/no echo, GUID/AI and observed HUP/reopen. Global AI uses actual settled model state, bound mappings and source 200 ms interval. |
| Settings and live UI | Service-owned reconfigureChannel/reconfigureGlobal validate/save desired tuple once, retain visible bind/open failures and protect newer callback edits. Setup/applet/tester tests and actual normal/scaled Cocoa captures prove reachable local controls/status, remote guidance and no-key tester. |
| Lifecycle/headless/R1 | Inert RadioModel construction; start after policy-ready desktop/daemon setup; stop before retirement; permanent denial after retirement. Headless/desktop/daemon/R1 tests execute without a radio. Remote-role service/pages cannot host local listeners. |
| Separate Hamlib | Newline framer, independent four TCP handles/diagnostics and optional PTY dialect share stable targets/claims. Real client and wire tests exercise supported short/long/ERP, NET handshake and precise unsupported/missing-target errors. |
| Platform/full acceptance | Fresh macOS application/tests and split full-suite results below; current-source Linux Core and independent no-SerialPort evidence refreshed after the final correction. Windows and bench limitations remain explicit. Lead integrated review pending. |

`RadioModel::catService()` exposes the owned service. Low-level
`applyChannelConfig` requires a stopped channel; `applyGlobalConfig` allows live
preferences only with unchanged PTT ingress. Public `reconfigureChannel` and
`reconfigureGlobal` accept desired configuration, which is distinct from a
successful open; unchanged proposals do not persist/reconnect/recapture.
`processFrame`/`processBytes`, `testCommand`, transport/client/path signals and
separate rigctld bound-address/port/count diagnostics expose actual state.
`RigctlProtocol(CatModelAdapter&, CatTxCoordinator&, int channel, quint64 sessionId)`
has its own per-session newline/protocol state. The adapter resolves stable
incarnations and revalidates writes after synchronous callbacks. Transmit claims
use existing native admission and never connect/discover/create a radio/slice.

Each of the four channels can enable native Thetis TCP, optional serial and
macOS/Linux PTY. Four separate rigctld TCP slots require a chosen port; the PTY
selects Thetis or Rigctld once per session. Ordinary serial stays Thetis.
Listeners and welcome default off, first Thetis TCP defaults 127.0.0.1:13013.
The applet's existing TCP/PTY buttons deliberately operate CAT1 with explicit
tooltips; four status/path rows remain and all channels have individual Setup
controls. Remote pages show a host-local reason instead of a new wire operation.

## RX contact and native source boundaries

Frequency, CTUN, mode/filter, AGC/NR/NB/ANF/APF/squelch, frontend attenuation,
admitted diversity and off-air buffer controls reuse existing model parameter
wiring. RX meter reads reuse the existing ready lane/channel cache and per-slice
calibration. `tst_cat_integration` sends real TCP bytes through production
transport/framing/parser/router and captures actual RX receiver-number/Hz and
TX frequency intent through an identified no-hardware connection. Receiver
numbers derive from native ReceiverManager mapping, rather than slice-ID guesses.

Across the feature, RadioModel/SliceModel changed for inert service ownership,
native Tune lifecycle and frequency notification survival; MoxController and
TwoTone helpers changed for accepted-intent observation and owned cancellation.
AudioEngine, RxChannel, WdspEngine and RadioConnection processing sources remain
byte-identical to the original implementation base. No I/Q/WDSP algorithm,
signal routing, DSP thread, callback mutex or authority-gate redesign occurred.
This is real software state/wiring evidence; deterministic test-only admission
and connection intent do not prove a radio MOX bit, RF or received speaker audio.

## Current platform and external-client evidence

| Environment | Actual evidence | Remaining evidence |
| --- | --- | --- |
| macOS 27.0 arm64, Qt 6.11.0, SDK 27.0 | Task 14 fresh configure and application/named build; 43/43 focused CTests. Task 13 had 26/26 current named gates and actual normal/scaled Cocoa Setup captures. Native owned PTY/QSerialPort byte and lifecycle tests execute. | Real cables/CTS/DSR and radio/audio/RF bench; other external applications. |
| Linux 6.12.76-linuxkit aarch64, Debian 13, GCC 14.2, Qt 6.8.2 | Task 14 current read-only source compiled into the retained distinct writable build in an owned network-disabled disposable container: 17/17 Core CAT gates, 34.85 s tests / 96.45 s wrapper, 1,333 Qt passes and zero failures/skips. Actual QtSerialPort compiler/link proof and official Hamlib/native MOX assertions execute; rigctld has 14/14 Qt cases and zero skips. Optional DFNR disabled in this lane. | Linux GUI/application, x86/hosted CI, physical serial and bench. |
| Independent macOS no-SerialPort | Task 14 fresh configure 14.46 s, named 13-target build 28.74 s and 13/13 anchored CTests 49.12 s; 177 Qt passes, zero failures, two explicit capture/client skips. Actual compiler definitions/headers and Core/GUI links omit QtSerialPort. Absence/refusal and native PTY/TCP assertions execute. | QSerialPort-only slots are compiled out; this is optional-absence evidence, not physical serial. Official executable slot skips here and runs on Linux. |
| Windows | Production guards/explicit PTY unavailable branch and CI's optional qtserialport module configuration inspected. | Actual Windows Qt SDK/compiler and runtime unavailable locally. No hosted CI dispatched; configuration is not execution. |

Task 13 built the official Hamlib 4.7.0 release tarball (SHA256
`24542b09cb2432458ba239b2ba8f5b7fb67cde64df6553f150e6eb8475a87a23`).
The actual client identifies `Hamlib 4.7.0`, timestamp 2026-02-15T21:14:25Z,
SHA `554e02b39`, 64-bit. Six `rigctl -m2` invocations against owned ephemeral
loopback endpoints prove NET open, F/f/M/m with fresh-client readback,
S/I/i/X/x split effects, AF/RIT and T/t. Captured stdout/stderr/wire requests and
native assertions include an observed MOX-on and disconnect retirement/MOX
release. Refreshed Task 14 Linux rigctld evidence is 14 passed, zero failed/skipped; the macOS
optional-client slot is skipped without the task-owned Linux executable.

Hamlib 4.7's get_lock_mode waits for an extra record even after an error. A
missing frozen target still returns truthful ENTARGET (-12); that client's
missing-target executable timeout was not measured in the available-binding
lane. No unlocked value is fabricated. get_powerstat and q remain unavailable
(-11). Native dump_state describes the actual software subset with empty
hardware lists, not a Flex/hardware identity. Other Hamlib versions/apps,
WSJT-X/JTDX/loggers and physical control devices remain unverified.

## Full-suite findings and corrections

The first fresh 1,131-test nonrealtime run found 11 failed executables, including
one TGXL timeout; its raw log and inner Qt results are preserved. The new Tune
wrapper converted native untyped OFF calls into a station-identity-only release,
which refused to release a remote device's carrier. Restoring the established
untyped stop contract for native callers fixes remote/TGXL/TX audio and the
unheld-key session regressions. Explicit keyer/CAT cancellation remains guarded.
The valid Tune/state fixture assertions were retained.

CAT's newly reachable applet exposed generic unavailable-control tooltips.
VAX, IQ and the three-rate selector remain disabled and retained; their tooltips
now state the applet's specific unavailable capabilities. Initial PTY wording is
dialect-neutral and synchronization names the selected Thetis/Rigctld dialect.
The wording policy permits the genuine Thetis PTY product phrase while still
rejecting source names. CAT validator sentences and the adapter's actual
station-on-air reason forwarding are covered by the existing wording scan;
“slice selection” replaces an internal “identity” term. Only three catalogue
expectations change from CAT unavailable to available, in the two recorded
catalogue fixtures; no Tune/state expectations were refreshed.

The native popup hover failure passes in isolation. Its sources are unchanged
across this feature, and the original parallel run allowed Cocoa windows to
compete for focus. Final nonrealtime coverage separates all 11 native-window
executables into a serial lane; no popup behavior or timeout was changed.

## Linux failures and verified fixes

The first current Linux run built successfully but failed two existing native
regressions. Isolated reruns reproduced both before their fixes:

- Closing a PTY master produced native HUP/ERR and EOF on the QSerialPort slave,
  without Qt 6.8 emitting a device error. A Linux-only 20 ms timer in the native
  device checks its actual QSerialPort handle for HUP/ERR/invalid-descriptor
  events. It never reads input or interprets an ordinary empty read as failure.
  Close stops the timer; existing failure/generation handling clears the exact
  session/PTT claim and protects callback restart/deletion.
- Linux master-side `tcflush` left 4095 bytes in the linked slave's input queue
  after an observed close. At peer loss the transport now temporarily opens its
  own slave, flushes and closes it before external notification. It retains no
  anchor, preserves the endpoint, and leaves the existing bounded first-request
  drain/raw-mode installation intact. Reopened peers receive no old output.

The valid native regressions were preserved. Linux kernel probes, failing runs,
then passing current builds establish the corrections. Original upstream serial
notice bytes and all owning inline comments remain exact; the provenance row
labels the new native Linux mechanics as NereusSDR-original.

A PTY remains one shared kernel stream. Several slave handles are one peer;
a close/reopen entirely between observations can hide HUP. Fresh-session and
cleanup guarantees apply to observed HUP, explicit close, error, stop or
retirement. No claim of process identity or unseen-gap detection is made.


## Task 14 final verification

Final measured gates:

| Gate | Exact result |
| --- | --- |
| Initial fresh configure/application/named43 build | Configure EXIT0,19.33s; app/named targets EXIT0,10.02s; anchored43/43 EXIT0,130.70s, two explicit capture/client Qt skips. |
| Fresh all_tests and corrective final app/all_tests | Initial all_tests EXIT0,1113.68s; after final corrections app/all_tests EXIT0,76.92s. No stale-binary suite claim. |
| Final ordinary nonrealtime | 1120/1120 EXIT0,539.89s;1118Qt totals19173pass/0fail/354skip. |
| Final native-window, serial | 11/11 EXIT0,70.59s;220Qtpass/0fail/1optional capture skip. Native popup passes; no source/test/timeout change. |
| First realtime attempt, serial |21/22,EXIT8,467.08s;monitor-speaker return-level assertion fails while unrelated compiler activity raises failure-time load to25.77/19.98/14.61. Unchanged isolated target passes1/1,13Qtpassed/0failed/0skipped,39.68s; final watched22-case timing qualification passes below. |
| Second realtime attempt, watched | EXIT8/395.19s;firstPureSignal executable failed0.71s, but CTest detail/output incomplete and LastTest byte-identical to prior isolated run. No pass count or Qt totals claimed. Preserved separately. |
| Unchanged isolated PureSignal |1/1 EXIT0/3.80s;6Qtpassed/0failed/0skipped, verbose actual BEGIN/SWAP/epoch assertions, zero competing samples. |
| Final realtime, serial/verbose | 22/22 EXIT0,395.92s wrapper/395.88s CTest;22fresh Qt blocks301pass/0fail/19skip;actual70samples have no compiler/foreign-test activity or heavy container work. |
| Current Linux17/official client | 17/17 EXIT0,34.85s tests/96.45s configure/build wrapper;1333Qtpass/0fail/0skip, rigctld14/14 including actual official-client execution. |
| Independent noSerial13 | Configure/build/tests/artifact0;13/13,49.12s;177Qtpass/0fail/2explicit capture/client skips, actual compiler/link absence. |
| Exact compliance and raw preservation | All16 final commands EXIT0,77.99s, including exact qualifiedCore/originalfeature/Task14BASE and FULL scans.52raw notice regions/1264relocated source comments, exactXML/419/349/70/783; changed notices/all old comments plus Aether23+3 retained.19Python compliance cases pass. |

The first broad1131nonrealtime run failed11executables (including a TGXL
timeout), EXIT8/628.37s. It is retained separately; it is not rewritten as a
successful attempt. Corrective14checks passed13with one uncovered reason
placement; the final reason scan passes after precise validator/forwarding
coverage and sentence correction. Final partitioned gates above establish the
actual corrected source.

The ordinary+native lanes contain355Qt skipped slots across22executables:
324normal/datachannel/connectable harness-selection skips, seven real-audio
hardware checks, six optional captures, six legacy unopened-WDSP harness slots,
two deferred RADE routing/bench slots, three optional measurement/hash dumps,
and seven other optional/platform/capability cases including the Linux-only
thread syscall, unbuilt DFNR, CPU-only paint, one-item/device API and modal-menu
cases. The official Hamlib skip on macOS executes in the current Linux lane;
CAT's native/scaled capture slot has accepted actual Task13 capture evidence.
Complete per-slot reasons and Qt totals are retained in LastTest snapshots and
inner-result JSON. Executable passes do not mean every slot executed.

The optional extra Linux remote-keying target pulled GUI-library autogen and
was stopped deliberately (EXIT137/258.68s) before execution. The resumed17Core
lane is final proof; Linux GUI and Linux native remote-keying are not claimed.
Native remote-keying/TGXL/station/TX-ring/conformance regressions pass on macOS.

Pre-realtime load was3.97/7.75/9.27, declining to3.81/7.65/9.23 after the host
check. No compilers/builds or owned containers were running; preexisting
containers were0.00–0.16% CPU. Existing desktop SDR/WindowServer activity remained
visible and untouched. The first realtime lane began in that valid state, but
unrelated compiler activity appeared later: the failing monitor slot recorded
load25.77/19.98/14.61; the post-host snapshot showed more than ten clang workers
at70–87% CPU and load21.36/20.11/15.38. No ownership, timeout or test threshold
was changed to conceal it. The loaded attempt is preserved; isolated and all22
qualification uses actual competing-process inspection and lightweight
host samples during the rerun. The unchanged isolated monitor target passes
1/1 in39.68s (13Qtpasses/0failures/0skips), including the failing speaker-return
assertion. Its eight samples have load3.46–3.92 and no foreign tests; a transient
compiler appears only in the final sample and ends before the fresh22lane.
That isolated result is not described as an uninterrupted quiet epoch.
No1131nonrealtime repeat is warranted.

The second watched22attempt ended EXIT8/395.19s and recorded an unexplained
PureSignal executable failure at0.71s. CTest's LastTest remained byte-identical
to the earlier isolated monitor run; stdout omitted subsequent results despite
observed later child executables. Its stale log is preserved and supplies no
new Qt totals or exact failed assertion. The unchanged PureSignal target then
passes verbose1/1 in3.80s (6Qtpasses/0failures/0skips), including actual
BEGIN/SWAP and old-epoch cancellation assertions. Repository/temp1MiB write,
fsync and readback probes pass with2.21GBfree; disk pressure is recorded without
claiming it caused the lost evidence. No build artifacts were removed.
Seventy host samples for that second attempt have load3.02–5.46, two samples
with compilers and16with foreign tests, including a46.4% CPU PureSignal session;
normal OS/app activity is retained separately. Neither an all-passed result nor
an uninterrupted quiet epoch is inferred. The final verbose watched22lane passes EXIT0/395.92s (395.88s CTest),
with all22fresh LastTest blocks and301Qtpasses/0failures/19skips. Initial/final
LastTest timestamps and sizes prove currency (final1,048,661bytes). All70host
samples have zero compiler/build processes and zero foreign test processes;
preexisting container CPU peaks5.83%, with no heavy container work. Load ranges
3.55–9.59, starts4.21/4.00/6.56 and ends3.55/4.54/5.99. Short recorded OS/app
bursts include ANE, indexing, coreaudio, crash reporting and desktop applications;
this is a measured no-competing-build/test epoch, not a zero-activity machine.
Monitor13/0/0 and PureSignal6/0/0 both execute inside this final lane.
No source/test/timeout change was made for these observations.

The final three lanes cover all1,153registered executables:1120+11+22, all
CTestEXIT0. Inner Qt totals across1,151Qt blocks are19,694passed/0failed/374skipped;
two ordinary registrations are non-Qt. Realtime's19skips consist of17nonrealtime
fixture-selection slots and two existing TURN/relay keyable-session harness
requirements that remain unperformed. These are distinct from the ordinary
355skips and from the official Linux client's zero skips.

Every raw wrapper records argv, UTC, exit, elapsed seconds and load before/after
under `.crew/2026-10-04-thetis-cat-plan/task-14-*.log`. The actual final CTest
JSON inventory, focused/full LastTest snapshots, compliance and preserved failed
attempts are retained there. No timeout was enlarged and no cite baseline was
recaptured. Native-window tests retain their preregistered platform exception;
ordinary tests use the existing offscreen/per-test settings sandbox.

Run named targets before anchored CTest. For final integration on a shared host:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build --target NereusSDR nereusd all_tests -j6
CTEST_PARALLEL_LEVEL=6 ctest --test-dir build -LE 'realtime|native-window' --output-on-failure
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build -L native-window --output-on-failure
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build -L realtime --output-on-failure
```

Task 14 builds the application and 43 named CAT/native regressions first, runs
that exact anchored selection, then builds all_tests. Final nonrealtime coverage partitions 1,120 ordinary executables at level 6
and all 11 native-window executables at level 1 to prevent Cocoa focus
competition. The realtime run uses level 1 alone, with no owned heavy
build/container. Current registration has 1,153 tests, 1,131 without
realtime, 22 realtime and 11 native-window. Qt inner skipped slots are reported
separately from CTest executable pass counts.

## Remaining acceptance

A required consolidated-review follow-up remains: on Windows the applet's PTY
button is disabled correctly, but the later local-host tooltip assignment
overwrites its unavailable-platform explanation. The lead retains a narrow
native-platform guard and Windows disabled/reason assertion proposal; it is not
applied or waived in this Task14 validated source. Windows runtime/SDK evidence
remains absent, and the branch is not described as ready to ship.


The lead's fresh integrated requirements/code review remains pending. The lead
checked later Core candidates; no fully qualified advance was available for
this handoff, so the selected qualified baseline remains `27716f5d`. A local PR
description is prepared; no PR/push/hosted
CI/merge/release/publication was performed. Windows SDK/runtime, Linux GUI/x86,
physical serial cable/modem pins, other logging/digital-mode clients, radio
MOX/RF and discovery → connect → received I/Q/WDSP → speaker bench remain
unverified. Exact unavailable catalogue contracts stay visible for missing
recording/CWX/VAC/memories, controller/MIDI/scripting, GUI-only display/recenter
and preset identity operations. Existing unrelated bench limitations remain.
