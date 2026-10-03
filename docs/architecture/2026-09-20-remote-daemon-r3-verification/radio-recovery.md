# Selected-radio recovery, R-R3-27 and R-R3-29

The [recovery design](../2026-09-22-radio-recovery-design.md) separates radio
transport loss from GUI/Core media loss and initial connection timeout. Core
can start before its selected radio appears and can retire an established P2
connection after all accepted inbound UDP stops. DaemonApp alone owns fresh
discovery and reconnect, pinned to the configured MAC or the first MAC chosen
for this daemon run. Retry delays are 1, 2, 4, 8, then at most 15 seconds;
the existing post-stop discovery quiet period still applies.

## Behavior and boundaries

P2 uses the three-second all-inbound deadline from Thetis v2.10.3.15
`ChannelMaster/network.c:655–666`. Valid status traffic alone keeps it alive;
this does not detect missing I/Q while status continues. Accepted traffic must
come from the selected address and negotiated role and pass the existing
packet-size check. Disabled wideband ADC streams do not count. IPv4-mapped
IPv6 addresses identify the same IPv4 peer; unrelated IPv6 addresses do not.

Loss stops producers, clears transmit intent, sends run=0/MOX=0, closes
ingress, then reports one NoDataTimeout and LinkLost. There is no low-level
P2 retry. Core retires DSP/audio/FFT routing and rebuilds around the same
slice objects, preserving frequency, mode, pan membership and active selection.
Queued codec and wideband callbacks cannot affect the replacement connection.

Discovery runs on a cancellable worker with fresh results. The control plane
stays available, and an already authenticated GUI receives updated radio
capabilities and MAC-scoped settings without a second handshake or replay of
slice/pan creation. Explicit disconnect and stop cancel recovery. A stop
during non-interruptible WDSP wisdom generation cancels the eventual dial and
defers destruction until setup unwinds; a second start cannot overlap it.

## Regression and review evidence

The preserving-reconnect regression first reproduced active selection being
reset from B to A. Real P1 loopback and WDSP initialization now verify object
and value preservation, including a wrong-MAC rejection. Daemon tests cover
late arrival, a wrong radio responding first, busy responses, quiet-period
deferral, worker interruption, stale completion and stop during setup.

Real P2 UDP tests cover first I/Q versus established loss, continued command
egress before loss, one unkeyed terminal stop, status-only and enabled-wideband
keepalive, malformed/wrong-source/disabled-wideband rejection, explicit stop,
endpoint replacement and reentrant error observers. A diagnostic run exposed
macOS's mapped sender address `::ffff:127.0.0.1`; the corrected acceptance path
passes the real socket test while rejecting a distinct `::1` peer.

The integrated daemon test connects through real P2 UDP, stops ingress, waits
for connection and WDSP retirement, then rediscovers the same radio and checks
both receivers. It also rejects old wideband work queued before FFT and old
publication already queued after FFT, while accepting a fresh frame. Session
tests authenticate real client/server peers and verify late capabilities,
scoped settings and replacement-session isolation. Audio/display tests verify
LinkLost retirement and fresh-generation resubscription.

One consolidated independent review covered the integrated change. Its codec
lifetime, wideband retirement and disabled-stream acceptance findings are
corrected. Final focused P2 and daemon executables passed **2/2 in 11.45 s**;
the other six affected executables also passed. Private evidence logs use
`r3-recovery-final-focused-{build,test}.log` and
`r3-recovery-corrected-test.log`.

A fresh `all_tests` and `nereusd` build followed by unfiltered ctest passed
**698/698 executables in 146.59 s**. The eleven existing inner Qt skips concern
host audio APIs or deferred harness coverage; none of the new recovery cases
was skipped. Logs: `r3-recovery-all-{build,test,load,skips}.log`. The recorded
pre-suite load averages were 15.86 / 10.16 / 7.57.

Matching native Core and GUI installation is recorded below. Real-radio
loss/resume acceptance remains pending. This checkpoint does not close the two-hour listening soak, IQ-only health policy,
Core-restart receiver persistence or the separate P2 Setup Network WDT parity
audit.


## Installed checkpoint and interrupted live acceptance

Signed software **55e7d49f** was installed on the Rock 5C and in the saved
`radxa_5c_r3` GUI profile on September 22. All 154 packaged source hashes were
verified before the native build and staged daemon/license installation.
The GUI build tag, private-library UUIDs and strict/deep code signature passed.
Rollback retains the previous 3402d171 binaries and private configuration.

The service stopped cleanly at 01:54:20 EDT and restarted with zero automatic
restarts. Core control listened at 01:54:21.928; the existing GUI automatically
completed its handshake at 01:54:23.076 while the radio was still offline.
The selected Saturn delivered first I/Q at 01:54:28.512, and the GUI reported
48 kHz stereo Opus at 01:54:28.729. The late-radio control/capability path was
therefore exercised during normal startup. This was not a physical absent-radio
or power-cycle recovery test.

At approximately 01:54:53, all observed traffic from the Rock stopped. Audio
reported no playable packets at 01:54:54; the media peer declared failure at
01:55:21 and the GUI entered automatic retry. Subsequent TCP attempts timed
out. SSH and ping also failed at both known Rock addresses (.106 and .105),
while the router (.1) and Saturn (.45) continued to respond normally. The Mac
route was its local en0 interface, with address .30. The MikroTik management
address .85 did not respond in this check either. This evidence establishes
loss of host reachability; it does not establish whether the cause was board
power, Ethernet, OS failure or application load. Board inspection and recovered
system logs are required before assigning a cause.

**Hardware acceptance remains open.** The installation and initial automatic
GUI reconnect succeeded, but sustained reception and physical radio-loss
recovery are not accepted. Private logs: `r3-recovery-native-build.log`,
`r3-recovery-install.log`, `r3-recovery-live-core.log`,
`r3-recovery-live-followup-core.log` and the saved-profile GUI log.

## R-R3-27 seen live, 2026-09-23

R-R3-27 (a Core started while its radio is not yet discoverable, then finds
it) was observed live twice on 2026-09-23. Source: the 2026-09-24 audit of
the R3 plan's open items (the crew ledger's `r3-audit.md`, row for plan line
1318).

The first observation ran from about 08:24 to 08:35. Build `ac4c63aa` was
installed while the G2's `p2app` was down. The Core ignored the other radio
on the network (`.107`) and kept waiting for the configured MAC. The window
authenticated at 08:24:43, well before the radio was reachable. The G2 was
found at 08:35:42, and audio played at 08:35:53, roughly eleven minutes after
authentication. No Core or window restart occurred at any point in the
interval.

The audit records a second observation that evening, at about 20:15, on
build `44584133`. It records nothing more about that session.

Both builds, `ac4c63aa` and `44584133`, contain the recovery code from
`55e7d49f` (the checkpoint installed above). R-R3-27 has been observed twice;
its acceptance still needs the slice-retention check against the window's own
log for these sessions, so the plan box stays open. Separately open:
physical radio-loss/resume acceptance for R-R3-29 (a radio that stops
sending after an established connection, rather than one that is absent at
start).
