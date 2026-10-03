# TGXL connection lifecycle, R-R3-22

This is the bounded retry/cancellation portion of task 4d. It does not complete
remote accessory commands, identity admission, live configuration, or authorize
RF actions. Execution follows `yonder-cost-aware-execution`.

## Observed defect

The GUI retried `.234:9008` after connect timeouts and reported source binding
outside UnconnectedState followed by an invalid socket descriptor. The user
corrected the port to 9010 and status responses arrived. Core applied the
persisted setting at restart and connected to the actual tuner. The original
static reconnect callbacks still retained old endpoints and could not be
cancelled when the operator disconnected or replaced the endpoint.

The intended correction retains the existing 1/2/5/10/30/60-second schedule,
using one owned timer and a current-request generation. Socket preparation,
failed source-bind fallback, explicit stop, replacement and destruction must
not permit an old callback to dial or retire a newer connection. A successful
TCP/version session cancels pending retry. The existing wire commands and
receive-only tuner policy remain unchanged.

Qt documents that a socket can still be unwinding its error when errorOccurred
is delivered; a replacement dial must wait for the event loop when necessary.
See [QAbstractSocket error delivery](https://doc.qt.io/qt-6/qabstractsocket.html#errorOccurred).
AetherSDR's owned timer provides the existing structural precedent; Nereus's
exponential backoff and source-route fallback remain native orchestration.

## Regression evidence before the correction

A fresh build of `tst_tgxl_connection_reconnect` succeeded, then real-loopback
cases reproduced the faults:

- Replacing the endpoint still produced one connection to the old endpoint
  (expected zero).
- Explicit Disconnect still produced one later connection (expected zero).
- A real bind failure on a nonlocal test address armed an extra retry while
  the OS-default fallback established a connection (expected no extra retry).
- A synchronous endpoint replacement from the retry notification connected to
  the new endpoint twice (expected once).

The test seam compresses the unchanged schedule for socket observation; it
also causes an actual QTcpSocket bind failure. It does not invent device replies
beyond the existing version-handshake fixture. Private logs:
`r3-tgxl-retry-red-{build,test}.log`.

The initial executable ended with SIGTRAP after these failures (3.25 seconds).
The crash report establishes destructor reentry: QAbstractSocket destruction
emitted disconnected, which entered the partially destroyed TgxlConnection,
then a retry consumer invoked another connection. Explicit quiescent destruction
is part of the lifecycle correction. The remaining cases were not all reached
in that failed run.

PGXL has a similar old retry pattern; its lifecycle remains a separate open
part of task 4d. This bounded patch does not claim to repair PGXL.

The first candidate passed seven focused suites (6.13 seconds, no Qt case
skips). The consolidated review then found a remaining active-socket race:
replacing endpoint A with B invalidated the request but left A's socket alive
until the deferred dial. A late A connected/version callback could cancel B's
timer, and A's error/disconnect callback could be mistaken for B's. Graceful
operator cancellation had the same late-handshake admission gap. Regression
coverage and correction must associate callbacks with the socket attempt,
including cancellation while waiting for the version banner.

A second fresh regression build reproduced all three gaps (9 passed, 3 failed,
zero skipped; 9.91 seconds for the executable). With real loopback TCP peers
holding the version banner, a controlled callback seam delivered the old
attempt's callback at the precise ordering boundary: replacement B received
zero connections instead of one; the cancelled session emitted one connected
signal instead of zero; a late old failure emitted one disconnect for B
instead of zero. Logs: `r3-tgxl-active-red-{build,test}.log`. The correction
must route that seam through the same attempt admission used by actual socket
callbacks, rather than fix only the tests.

Hardware acceptance is pending. The running 706b9a5f station is not using
this new source; the user initially confirmed its audio and waterfall smooth.

## Review correction and model notification

Each actual dial now receives a unique attempt token associated with its
endpoint request. All four real socket signal callbacks capture that token;
replacement, cancellation and a queued next attempt retire the old callbacks
immediately. The three ordering regressions enter the same handlers as those
callbacks. Parser-only tests retain their explicit offline injection path.
The corrected seven focused suites passed in 6.12 seconds. These tests control
the late-callback ordering; they do not claim the OS naturally reproduced it.

Lead integration inspection also found that retiring an established socket's
callbacks suppressed the model's disconnected notification during replacement.
A real-loopback test with a bound TunerModel failed: observers received only
`true` rather than `true, false` while B's version banner was held (12 passed,
1 failed, zero skipped). Replacement now publishes the retired connection's
disconnected state after queuing the new request, with no member access after
that public signal. The test requires the complete `true, false, true`
sequence once B is admitted. Logs: `r3-tgxl-state-red-{build,test}.log`.

## Final software verification

After the consolidated review correction and model-notification fix, all seven
focused suites passed in 6.06 seconds, with no Qt case skips. A fresh
`all_tests`/`nereusd` build followed by unfiltered ctest passed **683/683 test
executables in 134.32 seconds**. Eleven existing inner Qt cases skipped for
unavailable host APIs or deferred harness coverage; the new lifecycle cases
all ran. Private logs: `r3-tgxl-final-focused-{build,test,cases}.log` and
`r3-tgxl-final-full-{build,test,cases}.log`.

Before installation, the existing 706b9a5f GUI crashed during a mirrored AGC
update, and the still-running Core was subsequently observed producing zero
audio frames. These are separate live acceptance failures, under investigation;
the passing TGXL suite does not establish their resolution.

## Core-owned identity and configuration, September 21

Task 4d now adds a separate daemon accessory identity policy. The ordinary
local GUI keeps its established handshake. Core treats the version banner as
protocol progress, obtains sequence-correlated native `info`, and admits only
after a fresh station-side discovery record names `TunerGenius` or
`TunerGeniusXL` at the actual TCP peer address and received discovery port with
the same serial. Every physical retry needs fresh admission. Version, nickname,
and serial format alone do not establish product identity.

The identity regression first passed its legacy control case and failed five
new cases. The implemented boundary passes no-version and missing-info
timeouts, nonzero/malformed info, mismatched serial, pre-admission command and
telemetry suppression, buffered antenna-switch metadata, endpoint replacement,
cancellation, and fresh retry tokens. The Core coordinator regression then
failed eight cases against inert configuration methods before integration.
It now uses real loopback TCP peers and captured discovery/info grammar to
cover supported aliases, wrong product/serial, wrong discovery port, bounded
scan failure, A-to-B replacement, disconnect, master disable, and radio teardown.

Authenticated `configureTgxl(host, port)` and `disconnectTgxl()` negotiate
protocol minor 4 and `remoteTgxlConfigVersion=1`. Complete endpoint validation,
current radio MAC scope, and the existing 4O3A master gate precede persistence
or dialing. An accepted command means identification started, not connected.
Core publishes eight read-only connection/identity properties in addition to
the existing 13 tuner telemetry fields. Inbound GUI updates never invoke
hardware commands, and disconnected states clear live values.

Remote Peripherals sends one typed endpoint request, preserves unsent drafts,
reports station identification/retry/error state, and cancels through Core.
Its local accessory sockets and LAN scan remain inert. Old or disconnected
station links show an explicit unavailable state. RF-facing tuner controls
remain receive-only gated.

Seven Core-focused executables passed in **13.95 seconds**; eight session,
model, schema, daemon and GUI-focused executables passed in **18.30 seconds**.
Logs: `r3-tgxl-core-green-{build,test}.log` and
`r3-tgxl-integrated-green-{build,test}.log`. Review corrections and the final
unfiltered suite are recorded below. This source is not yet installed;
receive-only real-tuner acceptance remains pending. The separate 4O3A master
command and headless frequency/mode propagation remain open Task 4d work.

The single integrated review found additional boundary gaps. Fresh regressions
reproduced missing disk persistence, receive-only native accessory proxy
writes, serial-only discovery suppression, retry phase loss, and stale admitted
tuner state after session loss. Endpoint persistence now saves both validated
fields before dialing. The real loopback SmartSDR parser regression verifies
that receive-only Core forwards no native TGXL/PGXL writes while the established
local path still works. Session teardown clears admission and live tuner values
but retains the endpoint; the remote Advanced action is explicitly unavailable.
Those three affected executables passed together in **27.18 seconds**.
The discovery/retry/MAC-scope correction gate passed five affected executables
in **5.98 seconds**. Both MAC regressions fail at the intended endpoint
assertions before the fix: cold disabled configuration is missing, and switching
to an empty/disabled radio scope retains the old host. The implementation
publishes every newly selected MAC scope before any optional connection.

The first full run passed **694/695**; the audio-clock simulation passed in
138.97 seconds. The remaining failure exposed an existing test ordering race:
a spy saw the RadioConnection emission before the collector received its queued
slot. An owner-thread barrier and explicit queued-event delivery now establish
the actual observation boundary before advancing the test clock. The unchanged
production collector then passed its focused executable in **1.11 seconds**;
the final unfiltered recheck follows below.

Final rebuilt unfiltered gate: **695/695 executables passed in 133.64 seconds**,
including the simulated audio clock. The eleven existing inner Qt skips remain;
no new regression was skipped. Logs: `r3-tgxl-reviewed-recheck-{build,test}.log`.
The integrated review findings are corrected and checked. This is a source
checkpoint; receive-only real-TGXL identity/connection acceptance is still pending.


## September 22 follow-on: live observation and remaining controls

The matching `c9065756` Core/GUI installation includes the reviewed TGXL
identity/configuration work. Read-only capture after installation confirms
TunerGenius discovery from the expected tuner on UDP 9010. TCP observations
show Core in SYN-SENT with retransmitted requests and no reply, so actual
identity admission is not yet accepted. Service logs separately reproduce
`Invalid socket descriptor` during retries after asynchronous connect timeout.
Private evidence: `r3-tgxl-postinstall-health.log` and
`r3-tgxl-connection-passive.log`. No network configuration or RF action was used.

The remaining typed master/listener state and Core-owned frequency propagation
are now under implementation, together with the proven socket-engine reuse
repair. New model tests use actual loopback listeners (including occupied-port
failure) and a loopback PGXL with acknowledged pairing. Session tests cross the
authenticated boundary and GUI tests cover pending/refused/reconnect states.
These changes have not yet passed their build/test gate or been installed.


The first focused run passed **9/10 executables in 58.94 seconds**. The
new authenticated listener test exposed a production admission omission:
StationClient's inbound state-apply allowlist did not include the three new
RadioModel observation fields, so Core bound successfully while the GUI
remained stale. The client now admits only those exact field names through
the inspected Remote-only assignment hook. Rebuilt model, GUI and authenticated
session checks passed **3/3 in 11.27 seconds**. Logs:
`r3-four-o3a-focused-test.log`, `r3-four-o3a-state-apply-{build,test}.log`.
An earlier compile failure was a missing SmartSdrApiListener test include,
corrected before this runtime gate. The full suite and integrated review are
still pending.

The socket-engine explanation is confirmed against the Rock's upstream Qt
6.8.2 source, not only the Mac's Qt version: asynchronous timeout publishes
Unconnected without resetting the engine; abort/close skip the reset in that
state. Each physical retry now owns a fresh socket, and the retired socket is
destroyed through deleteLater after its callbacks are disconnected. Direct
failure-observer replacement/cancellation and retirement are covered by the
passing reconnect executable. See the official
[Qt 6.8.2 socket implementation](https://code.qt.io/cgit/qt/qtbase.git/tree/src/network/socket/qabstractsocket.cpp?h=v6.8.2).
This does not establish why the tuner currently sends no SYN-ACK.

The consolidated review found that an unanswered command survived session
teardown and suppressed completion of the next 4O3A request after reconnect.
Two regressions reproduced the missing completion for both link loss and
direct session replacement. Clearing the retired session's pending commands
fixes both paths. Rebuilt authenticated-session and peripheral-page checks
passed **2/2 in 9.29 seconds**, including the new reconnect rows. Logs:
`r3-four-o3a-reconnect-red-test.log` and
`r3-four-o3a-reconnect-green-{build,test}.log`.

Final source gate: rebuilt `all_tests` and `nereusd`, then the unfiltered suite
passed **696/696 executables in 139.53 seconds**. Eleven existing inner Qt
skips remain; none of the new regressions were skipped. The consolidated
review has no remaining actionable findings after the session-teardown fix.
Logs: `r3-four-o3a-reviewed-{build,test,cases}.log`. This gate establishes
the software checkpoint; matching native installation and real-device
observations are recorded separately below.

### Matching installation and live observations, September 22

Signed software `3402d171` is installed on both the Rock and Mac. All 143
overlay source hashes matched the signed checkpoint before the native build;
staged dependency checks passed. GUI private-library UUIDs match the build,
and strict/deep code-signature verification passed. Core is active with zero
automatic restarts. Rollback is retained at
`/var/lib/nereus-build/rollback-c9065756-before-3402d171/`.

The running Core owns TCP 4992 and the authenticated listener at port 50055.
A read-only loopback `sub slice all` observation returned the actual current
TX-bound receive slice as `RF_frequency=3.865100 mode=LSB`, matching the GUI.
This is real headless initial-state evidence; retune/rebind/removal are covered
by the loopback regression suite, not a new physical-radio tuning test.
The native Settings tree did not expose a usable navigation action through
the current UI automation, so visual acceptance of the new 4O3A page remains
pending; its pending/error/availability behavior passed the GUI tests.

The old Core session ended during installation around 00:36:58. Without a
manual connection action, the GUI reauthenticated at 00:37:16 and resumed
Opus reception. Live spectrum, waterfall and meter were visible; all three
telemetry tabs showed current measurements and the restart gap. Samples were
about 550–580 kbps total Core application traffic, 23–24 kbps Opus payload,
25–26 kbps audio-track messages and 24–29 ms speaker buffering. These are
observations, not fixed-rate or end-to-end latency guarantees.

TGXL discovery remains separate from TCP admission. After installation,
connect-time timeouts and fresh SYN-SENT attempts still occur; no tuner
identity was admitted. The observed retries no longer show the previous
source-bind/invalid-descriptor failure. No RF, tuner actuation, pairing or
network configuration was used. Audio soak remains open; the receive log
still has occasional approximately 80 ms RTP-arrival gaps.

Private evidence: `r3-four-o3a-native-build.log`, `r3-four-o3a-install.log`,
`r3-four-o3a-gui-identity.json`, `r3-four-o3a-live-core.log`,
`r3-four-o3a-listener-observation.log`, and `r3-four-o3a-retry-live.log`.
