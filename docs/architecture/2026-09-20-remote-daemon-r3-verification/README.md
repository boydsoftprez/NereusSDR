# R3 verification ledger

This ledger separates the recovered integration baseline, component evidence
and real remote receive acceptance. A component pass does not close R3.

## Installed packet-burst playback correction, 200d2a0e

The actual receiver now uses an already-admitted expected packet when its next
speaker refill would otherwise underflow. It preserves ordering, normal hold,
loss policy, queue bounds and WDSP feedback. The measured-burst regression went
from a spontaneous restart to decoding all 99 valid packets with exactly the
one deliberate concealment. Seven affected targets, the native-block readiness
check, the complete build and all 692 test executables pass; eleven existing
inner Qt skips remain. One independent review is resolved, with strengthened
tests rerun successfully. The same checkpoint corrects clipped graph labels.
Matching Core/GUI `200d2a0e` are installed; all 115 source-overlay hashes, native
production staging/dependencies and GUI identity/signature checks passed. Core
PID 17097 was active with zero restarts; GUI PID 39092 opened saved profile
`radxa_5c_r3` and authenticated at 22:22:39. The initial speaker-open backlog
recovered automatically into context 3. Live Connection, Round trip and Audio
graphs are readable, including the formerly clipped packet/frame units.

The Core restart recreated only receiver A, leaving the second GUI pan empty.
Receiver B was recreated through +RX; the operator then selected 80m LSB and
continued tuning. RADE listening acceptance is pending the operator comparison;
this interval must not be reported as a RADE soak. Multi-slice restart restore
is recorded as R-R3-34. Rollback is
`/var/lib/nereus-build/rollback-0b408427-before-200d2a0e/`.

## Installed checkpoint 0b408427, September 21, 21:42

Matching signed Core and GUI `0b408427` are installed and receiving through the
Rock 5C at `.106`. This includes telemetry `d6ce05a5` and the RADE mixer capacity
correction. All 109 packaged source hashes, the native production build/stage,
installed hashes, GUI executable tag/private library UUIDs and strict/deep
signature checks passed. The full desktop gate passed 692/692 executables in
154.93 seconds, with eleven pre-existing inner Qt skips.

The live Core banner and Connection/Audio history graphs are verified. They
showed about 18.7 Mbps radio ingress, measured Core RTT and about 48,000 source
frames/25 accepted Opus packets per second. Audio graph labels were clipped;
a font-measured gutter correction passes the two affected graph/dialog tests,
but is not installed yet. Reconnect gaps and final readability remain pending.

With A in USB and B in RADE-L, Core's earlier 39–44k frame/s deficit is resolved
in the observed intervals. The GUI still reports intermittent buffer underflows
and restarts amid 80–150 ms packet-arrival gaps. Smooth listening and sustained
RADE operation therefore remain open. The receiver investigation preserves the
existing producer-clock contract; no speculative jitter deadline change is
installed. See [RADE multislice](rade-multislice.md) and
[telemetry verification](core-telemetry.md) for evidence and limits.

## Previous checkpoint dd2a9ebf, September 21, 21:11

Signed `dd2a9ebf` is installed in matching Core and GUI builds. It includes
R-R3-31's owning-slice RADE routing, decoder/worker lifetime guards, unsynchronised
quiet cadence and warming-target mixer admission. All 685 desktop test
executables passed with eleven existing inner Qt skips, followed by the native
production build and matching GUI identity/signature checks. The corrected
native test-helper guard is documented with the original failure.
See [RADE multislice](rade-multislice.md) for the reproducer, review corrections,
installed hashes, rollback and short live session evidence. Hardware two-pan
listening failed with stuttering on A while B used RADE; a measured Core source
rate deficit is under investigation. Sustained operation remains open.

The approved banner plus connection/audio telemetry is now integrated in the
primary worktree after focused checks and one consolidated review. The complete
build and all 692 test executables pass, with eleven existing inner Qt skips.
It is not installed yet; the live RADE cadence failure takes priority. See
[telemetry verification](core-telemetry.md) and the
[agreed design](../2026-09-21-core-telemetry-design.md). CPU and memory history
remain follow-on work.

## Previous checkpoint d9c7bce1, September 21, 19:47

Signed `d9c7bce1` is installed in the macOS GUI and on the Rock. It includes
the VFO callback lifetime correction and the preceding signed TGXL connection
lifecycle fix (`3a589945`). The fresh full suite passed 684/684 executables in
144.54 seconds, with eleven pre-existing inner Qt skips; the new regression has
none. The independent VFO review was clean. See [VFO lifetime](vfo-lifetime.md)
and [TGXL lifecycle](tgxl-recovery.md) for separate regression evidence.

All 63 packaged source hashes matched. Native build/staging and dependency
checks passed. The installed Core library hash is
`367e55fb46d6212e25923db571aa9e2ed963e00f6a5926be92cf1ea8b943c881`;
nereusd and RADE hashes remain the same as the preceding checkpoint below.
Rollback is available at
`/var/lib/nereus-build/rollback-706b9a5f-before-d9c7bce1/`.

The GUI's executable tag and both private library UUIDs matched the freshly
built artifacts, its bundle passed strict/deep signature verification, and its
new VFO binding symbol was present. GUI PID 76366 opened with saved profile
`radxa_5c_r3` and endpoint `.106:50055`. At 19:45 it visibly showed live 80m
reception at 3.927 MHz LSB, with matching flag/applet meters (-82 dBm at that
observation). The initial speaker-open delay caused a packet backlog and one
500 ms receive timeout, followed by automatic audio-context recovery.

The GUI remained open across Core installation. It observed shutdown at
19:46:40, retried and authenticated at 19:46:58, then resumed Opus reception.
Core PID 7205 is active with zero service restarts and a listener on `.106`.
The first two-second diagnostic contained 96,128 source frames, 50 encoded
and accepted packets, zero encoder errors and zero source drops. The user's
Settings dialog was open at the final visual inspection and was left intact;
the post-installation media recovery evidence is the handshake/audio log and
Core diagnostics, not a new unobscured waterfall screenshot.

Live two-pan to one-pan crash acceptance, the separate RADE slice-routing
repair and sustained listening remain open. TGXL positive identity, live Core
configuration and the intentionally disabled antenna/RF actions are not closed
by its lifecycle fix. No RF action or OS/VPN/switch/network change was performed.

## Latest live interruption, September 21, 19:09–19:20

The earlier smooth checkpoint did not establish continuous acceptance. GUI
PID 54322 crashed at 19:09:12 while applying a mirrored AGC threshold to a
VFO slider, after a pan layout change rehomed a slice. The dangling flag
connection is reproduced by a failing production-binding regression; see
[VFO lifetime verification](vfo-lifetime.md). Core PID 3656 stayed active, but
later reported zero audio source frames and repeated media-context renewal.
A passive capture still contained Saturn DDC2 I/Q on UDP 1037 and status on
1026, so this occurrence must not be labelled total radio ingress loss.

The original 706b9a5f executable and private library UUIDs were verified;
CMake had regenerated Info.plist, invalidating its bundle signature. Re-signing
and reopening the saved radxa_5c_r3 profile restored the intended build, not
reception. An authorized Core restart at 19:19 restored approximately 48,000
audio source frames and 25 accepted Opus packets per second; Core PID 5242
has zero service restarts. The GUI retained a blank previous pan, so complete
visual recovery required reopening the GUI again. At 19:22 the live 40m
spectrum/waterfall and both meters (-94 dBm) were verified in GUI PID 66655,
still 706b9a5f with profile radxa_5c_r3 and Core at .106. The station restart
restored slice A and removed the earlier slice B; this is recovery evidence,
not proof that the failing multi-slice state is repaired. No new TGXL binaries
are installed. Its source is signed as 3a589945, with 683 test executables
passing; see [TGXL lifecycle verification](tgxl-recovery.md).

The VFO correction now passes its regression, all four affected GUI targets,
independent review and a fresh full 684-executable suite. Installation and live
layout acceptance are the next checkpoint; this evidence does not yet change
the installed 706b9a5f state described above.

A separate source/log audit identified a RADE channel belonging to B being
routed through A's worker path. This suppresses A's normal audio contribution
and correlates with all three zero-frame intervals. That repair is pending;
the details and limits of the evidence are in the VFO incident record above.
At 19:30 the reopened GUI also logged a 1.036-second owner-thread packet delay,
an underflow and two subsequent stream-gap recoveries. Later packet reception
resumed. The restored single-slice state is still not a soak pass.

## Previous receive status, September 21, checkpoint 706b9a5f

Signed `706b9a5f` was installed on the Rock and in the matching macOS GUI.
It adds typed media-loss recovery through the existing authenticated reconnect
path. All 55 packaged source hashes matched; the native build/stage, updated
Core-library hash and exported typed-failure symbols, install with rollback,
and GUI deep/strict signature verification passed. The prior install is kept
at `/var/lib/nereus-build/rollback-8c011066-before-706b9a5f/`.

Installed SHA-256:

| File | SHA-256 |
| --- | --- |
| nereusd | `4a685f62e6c9ed6a7e83fbe5daadda8eacf5aeb4079e7e30d481310e001b7202` |
| libNereusCore.so | `23303eceb27fe1744533aea39a442e897ee2afa633e86ec9206d779d0e105dbc` |
| librade.so.0.1 | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

The GUI (PID 54322) was left open across the authorized Core installation.
It observed station shutdown at 18:31:07, retried automatically, authenticated
again at 18:31:25, and resumed Opus reception. Core PID 3656 had zero restarts
and listened at `.106:50055`. Its first two-second audio diagnostic reported
96,064 source frames, 50 encoded/accepted packets, zero encoder errors and zero
source drops. At 18:32 the GUI visibly showed live 3D/2D and matching applet/flag
meters (-65 dBm at that observation).

The user confirmed both audio and waterfall were smooth after this restart.
This is a successful short receive checkpoint; it does not close the soak.

This establishes normal full-Core restart recovery, not yet the new media-only
failure path on hardware. That path has real pinned-TLS integration coverage;
see [media recovery](media-recovery.md). Sustained listening, media-only live
recovery, initial negotiation deadline/backoff and the two-hour soak remain
open. After the restart Core itself opened `.106 -> .234:9010` and received
`info serial=241288-1 version=1.2.17 nickname=Tuner_Genius_XL 3way=1`.
The user's endpoint change persisted through settings; applying it live without
a restart, validating identity before admission, and remote tuner actions are
still incomplete. No RF actions or OS/VPN/switch/network configuration were
performed.

## Previous receive status, September 21, checkpoint 8c011066

Signed `8c011066` was installed on the Rock and in the matching macOS GUI.
It includes the listener retry, initial DDC-rate correction and audio diagnostics.
All 45 packaged source hashes matched; native build, staging, dependency checks,
installation with rollback, and GUI strict/deep signature verification passed.
The previous installation is retained at
`/var/lib/nereus-build/rollback-95b19467-before-8c011066/`.

Installed SHA-256:

| File | SHA-256 |
| --- | --- |
| nereusd | `4a685f62e6c9ed6a7e83fbe5daadda8eacf5aeb4079e7e30d481310e001b7202` |
| libNereusCore.so | `325f7a99f81d02726b65bade69a0e8372604c635c484ca3e16e593cf4d2cdf05` |
| librade.so.0.1 | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

The live startup defect is verified before/after: the old code commanded
DDC2 at 48 kHz against a 192 kHz host; the new code converges to 192 kHz within
2 ms of its initial bootstrap command. The GUI then displayed live 3D/2D,
noise-floor telemetry and matching applet/flag meters (one observation -83 dBm).
Core produced approximately 48,000 mixed frames and 25 Opus packets per second.
See [startup/audio evidence](startup-audio.md).

**Sustained receive acceptance remains open.** The user reported smooth output
when working, then a disconnection without recovery while on Starlink through
ZeroTier. The log separates two failures: audio underflows amid receive callback
gaps up to 481 ms (later 1.855/6.032 s), and media closure at 17:39:01 while the
control session remained authenticated. That state left the GUI saying Core
connected with no live media. R-R3-28 now tracks automatic recovery of that
specific state; see [media recovery evidence](media-recovery.md). Manual panel
Disconnect/Connect restored a handshake and audio;
later complete control-link losses entered the existing automatic retry path.

Core was temporarily rebound to Wi-Fi `.105` using its unchanged certificate
and pairing; Ethernet retained carrier but lost its IPv4 lease. The GUI's private
launcher overrides the endpoint to `.105`. Subsequent Mac routing changed to
local `en0`; the board again stopped answering SSH at `.105` and `.106`.
At 18:04 the user reported `.106` back; the board had a fresh boot, both
addresses, and a wired route to Saturn. Core and the private GUI launcher were
restored to `.106` with unchanged pairing. The service (PID 1569 after that
restart) listened successfully, and audio/display resumed. This is a successful
ordinary restart, not a controlled late-address boot test. No OS, VPN, switch
or network configuration was changed by this work. Network stability,
sustained playback and a two-hour soak remain unaccepted.

## Previous receive checkpoint, 95b19467

Signed checkpoint `95b19467` was installed on the Rock 5C and its matching
macOS GUI was launched. It adds Core connection controls, snapshot-safe
startup, TX capability gating and receive-only tuner telemetry. The broader
accessory connection/identity/configuration work remains task 4d. See the
[control acceptance matrix](remote-controls.md) for the bounded scope.

The corrected native Release build and staged installation passed. All 32
source-overlay hashes matched the signed checkpoint. An initial package used
zero file timestamps and caused Ninja to reuse old Core objects; that candidate
was caught by library-hash comparison and rolled back before GUI testing.
The packager now preserves current timestamps and touches verified source before
building. The subsequent build compiled the changed Core sources, exported the
new policy/telemetry symbols, and produced matching staged/installed hashes:

| File | SHA-256 |
| --- | --- |
| `/usr/local/bin/nereusd` | `8e0b6483c27ac79c757b41e02bdcf78eee31e8b16a07f01f066c2fc6c0faa599` |
| `/usr/local/lib/libNereusCore.so` | `422bd1989c4af83c013ed151a32cda9613efe7d72119cc2603abd0e950630d82` |
| `/usr/local/lib/librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

The valid rollback is
`/var/lib/nereus-build/rollback-501b2701-before-95b19467/`. Private station
identity, pairing and profile were preserved. The service was last observed
active with zero restarts. Reachability has since returned over Wi-Fi at
`.105`; Core was temporarily rebound to that address, retaining its certificate
and pairing. Ethernet has carrier but no IPv4 lease (lease lost at 16:34:10).
This later loss does not establish the cause of the earlier media failure. The macOS
bundle build and deep/strict code-signature verification passed.

**Live receive acceptance failed and remains open.** The GUI authenticated,
received encrypted waterfall frames and displayed one slice. Audio repeatedly
stalled before the control connection timed out; spectrum/meter health was not
accepted. The user described a lethargic waterfall that stopped after moving
the VFO away and back. That sequence does not yet establish a tuning cause.
Manual Disconnect cancelled the retry and left the GUI stopped. The other
connection surfaces and successful reconnect still need live verification.

After the earlier power cycle, Ethernet initially had carrier but no IPv4
lease, and the route to Saturn used Wi-Fi. Ethernet later regained its lease
and the Saturn route returned to `end1`. The user confirmed disabling and
reenabling the MikroTik port, explaining the recorded 16:03/16:04 carrier drops,
not the original lockup. During the failed smoke, both the Rock and switch
management endpoint became unreachable from the Mac; the router and Saturn
still responded through the existing VPN. No OS or network configuration was
changed. Hardware/network diagnosis remains open.

A separate startup defect is reproducible: binding the configured wired
address before that address exists leaves the service running without a Core
listener, even after the address returns. R-R3-26 adds cancellable, capped
backoff with the same station objects and identity; implementation and hardware
verification are tracked separately from this installed checkpoint.

At the preceding `501b2701` checkpoint the user accepted smooth C-Tune wheel
and in-band scale-drag zoom, and reported working mixed stereo Opus with
occasional stutters. Those results do not override the failed latest smoke.
Full ADC-wide zoom, audio diagnostics/profile comparison, capacity, the
two-hour hardware soak and R5 traversal remain open. The sections below retain
historical checkpoint evidence and do not override this current status.

Candidate startup-rate regression and audio-counter evidence is recorded in
[startup receive cadence and diagnostics](startup-audio.md).

## Late-network listener recovery, R-R3-26

The regression was reproduced with a real occupied loopback port: the running
daemon had no listener and no retry. The three recovery/cancellation/restart
cases failed before implementation; disabled/invalid configuration remained
inert.

DaemonApp now retains the existing StationServer, media controller, model,
certificate/token and slices while retrying the exact configured address and
port. Delay starts at one second and doubles to a 30-second cap. Stop cancels
the timer and clears its target before teardown; each new start resets the
target and delay. Invalid addresses/ports and disabled remote control do not
retry. Failure, retry delay and success are logged, with readiness and pending
state available from the daemon.

A fresh `tst_daemon_app` build/run passed in 1.15 seconds. Real-loopback cases
cover multiple capped retries followed by successful binding with unchanged
station objects/token/slices, no delayed bind after stop, replacement-target
recovery, and disabled/invalid configuration. Logs are private
`r3-listener-{red,green}-{build,test}.log`. A fresh `all_tests` build and
unfiltered `ctest --test-dir build-integration -j8 --no-tests=error
--output-on-failure` then passed **682/682 test executables** in **94.99 seconds**. No executable
was skipped. Eleven existing Qt cases inside those executables skipped for
host-API availability or explicitly deferred harness/UI coverage; the new
listener cases all executed. Full logs are private
`r3-listener-full-{build,test,cases}.log`.
Signed code checkpoint `e518b63d` is included in the installed `8c011066`;
a real late-address boot test remains pending. It does not claim to repair the network outage or media stalls.

## Combined baseline, 14124e7c

The signed integration checkpoint includes the published open-PR work selected
on September 20, including the 3D waterfall. The macOS RelWithDebInfo build
has GPU spectrum enabled and DFNR disabled.

`cmake --build build-integration --target all_tests -j10` succeeded, followed
by unfiltered `ctest --test-dir build-integration -j10 --no-tests=error
--output-on-failure`: **662 passed, zero failed, zero skipped**, 41.71 seconds.

The first full run had five failures. Two source corrections are in the signed
checkpoint: nine TX-analyzer settings now have Station scope, and a programmatic
slider-signal test no longer waits unnecessarily for window exposure. Three
fixture consumers needed two local build-only symlinks (`cty.dat` and
`tests/fixtures/adif/sample.adi`). Those links are an environment workaround,
not a repository fix. Detailed transcripts remain in the maintainer's private
work directory, `~/.config/nereus/work/combined-tests-ed246176/`.

## Rock 5C installation, 14124e7c

Native aarch64 Release build and staged daemon-component installation passed
on the 2 GB Rock 5C, Armbian Trixie, Qt 6.8.2 and GCC 14. GPU rendering, tests
and DFNR were disabled for the headless build. The staged binary resolves
NereusCore and RADE beside its installation; no missing dependency was reported.
The binary RUNPATH is `$ORIGIN/../lib`, and the Core library uses `$ORIGIN`.

The checkpoint is now installed. The previous `daf05135` binary, RADE library,
systemd unit and configuration were preserved under the root-only directory
`/var/lib/nereus-build/rollback-daf05135-before-14124e7c/`. Station identity,
private pairing data and the daemon profile were preserved. The existing
service stopped cleanly, and the installed service is enabled and active with
`Result=success`, `ExecMainStatus=0`, and `NRestarts=0`.

A fresh board-local authenticated TLS probe confirmed:

- Pinned Saturn identity: MAC `2C:CF:67:AB:FC:F4`, board 10, firmware 27.
- Connected radio, one slice, 2,502 settings, `txPermitted=false`.
- Receive tune from 14.225 to 14.226 MHz, then verified restoration.
- 161 meter samples, 141 distinct values, over roughly 15 seconds.

Installed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `/usr/local/bin/nereusd` | `705f011ebfd50ec46c6c69f690c7625ce91b33484e60e17198a8de399671c36d` |
| `/usr/local/lib/libNereusCore.so` | `22a9e65e93bf46db186c4fbc39bbf83d00741d0a704d684ce23754bf6467129c` |
| `/usr/local/lib/librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

This installation still has R2 control behavior. It does not carry R3 audio
or spectrum. Reboot/reconnect passed earlier at `daf05135`; a new reboot at
`14124e7c` has not been performed. All live tests were receive-only.

## R3 component evidence

- [Remote audio status and native-check fixes](remote-audio-status.md):
  R-R3-23 task 5a item 1 (persistent codec/status/health display, Retry)
  plus the R-R3-34, R-R3-13, R-R3-21/25, R-R3-24 and R-R3-17/38 native-check
  fixes. Full gate at 9982cbed: build passed, unfiltered ctest 743/743
  passed. Hardware and operator acceptance rows are pending.
- [Pinned transport build and two-peer probe](transport-probe.md): encrypted
  display and RTP exchange and stop/recreate passed in the standalone harness.
  Production Qt transport and MediaPeer tests now pass, including callback
  retirement during stop/restart/deletion. The loopback probe passed two
  rounds at MTU 1000 (transport-probe.md:86-88); RTP packets are capped at
  940 bytes before encryption (OpusAudioCodec.h:26, PcmAudioCodec.h:41,
  refused oversize at LibDataChannelMediaTransport.cpp:770, from ea55d24a).
  A packet capture of the encrypted audio (Opus 24k, Opus 48k and Lossless)
  showing every packet at 1000 bytes or less remains pending; only the
  display capture exists so far (969 bytes). The selected libjuice backend
  lacks TURN/TCP and TURN/TLS; see the
  [plan's network continuity section](../2026-09-20-remote-daemon-r3-plan.md#network-decisions-carried-forward).
- [Opus source/packet probe](opus-profile-probe.txt) and
  [fixture source](opus-profile-probe.c): how high the sound reaches follows
  the bitrate (sound up to 8 kHz at 24 kbit/s; sound up to 20 kHz at
  48 kbit/s), and the stereo channel count and the audio data rate are
  checked. This is not listening or Rock 5C CPU evidence.
- Display codec: integrated target passes, including finite-extreme arithmetic,
  reconstructed-history error bounds, lost-delta recovery, malformed input and
  stale/wrapped sequence regressions.
- Headless FFT source and pool: focused integrated tests pass. Retune discards
  pending input and overlap, and the daemon uses bounded ingress/latest output.
- Session media gates: `tst_station_session` and `tst_session_link_loss` pass
  after fixing replacement-session retirement while preserving silent redial.
- Desktop and daemon display integration: production targets build, and the
  focused transport, peer, source, endpoint, session, local/remote rendering
  and GUI-controller checks pass. The two-pane regression uses actual
  authenticated control, the daemon controller and tagged I/Q production
  with a deterministic test carrier. Both panes receive decoded frames after
  a shared window change and a radio disconnect/reconnect. The daemon tests
  also cover subscription revision, epoch, no-overlap and slice retirement.
- Real Saturn media, GPU display and on-wire packet-size evidence remain
  pending installation of this checkpoint.
- Mixed-audio tap: `tst_daemon_audio_source` passes 8 cases using the real
  master-mixer path, independent left/right values, per-slice mute, local
  master-mute isolation, bounded overflow with honest sample positions,
  restart and synchronous callback retirement. Network sender, jitter/playback
  and adaptive clock correction remain pending.
  The existing WDSP RMATCH probe preserved stereo and bounded occupancy for
  120 simulated seconds at fixed compensation ratios for +/-500 ppm; this is
  not an adaptive-controller or long-session acceptance result.

## Operator acceptance still pending

Refreshed against the 2026-09-24 read-only audit of the R3 plan's unchecked
boxes.

The two-hour hardware audio soak is superseded: the operator decided on
2026-09-23 at 09:50 that soak runs are removed as gates. The periodic
counters remain (38f79249, the 60 s log line), but no gate depends on them.

Still pending an operator or device observation:

- Loopback or Rock packet-size capture of the encrypted audio codecs (Opus
  24k, Opus 48k, Lossless) at 1000 bytes or less; only the display capture
  exists so far (969 bytes).
- Wideband wings and the optional 3D row: largest frame size and fragment
  count on the Rock at 4096 points, 60 fps, 3D on; real zoom and wing
  gestures including a slice shared with another window; the wideband
  frame freshness limit, pending the Saturn's real burst cadence.
- Connections screen entry points: each click starts exactly one Core
  attempt; a manual Disconnect does not redial; first connection, Core
  unreachable, retry backoff, cancel, Core recovered, Core up with radio
  down, and a manual Disconnect after a good session.
- Persistent Core identity and retry/error wording; queue item C (what a
  permanently refused or taken-over window shows) is still undecided.
- Station selection: LAN announcements (IPv4 and IPv6), switching between
  the local radio and the Rock with real display and audio, add/edit/forget,
  and Core online with radio offline.
- Slice-flag lifetime while a pan shrinks and the Core keeps sending AGC
  updates.
- TX-entry-point disabled-state wording review at an operator checkpoint.
- Tuner Genius (TGXL) real connection from the Rock: the connection to
  .234:9010 still hangs half-open; a network check of the Rock's route and
  interface comes first.
- Measured Core-to-client audio latency readout on the Rock with a real
  speaker; the Network Diagnostics tab labels, connected and disconnected.
- A real two-slice Core stop/start and window reconnect with the same IDs.
- Opus profile listening (24 vs 48 kbit/s) with Rock CPU and wire bytes; the
  acknowledged-configuration Lossless path carried to the Rock.
- The RADE two-pan smoke test (A on SSB, B on RADE-U).
- Session display budget under one, four, floating and shared-stream pans on
  the Rock.
- Full-span and deep-zoom capacity measurement on the Rock and the Pi 4 (the
  Pi 4 cannot yet be upgraded to current code; its installer refuses an
  existing install).
- The final combined gate: the full desktop suite once more at the merged
  head, plus ARM-sensitive tests and a staged Linux install.
- Late-network boot recovery (R-R3-26), media loss with control still
  healthy (R-R3-28), and the radio stopping then resuming (R-R3-29), each on
  the actual boards. R-R3-27 (a Core started before its radio is
  discoverable) was observed twice on 2026-09-23, 08:24-08:35 and again at
  20:15; its acceptance still needs the slice-retention check from the
  window's log; see [radio-recovery.md](radio-recovery.md).
- A signed install and rollback repeated at the final checkpoint.
- One-pan and four-pan on-wire budgets and delivered quality, including
  informal internet (Pi 4 over public IPv6) evidence.

Internet carrier and CGNAT-to-CGNAT evidence remain separate R5 gates.

## Consolidated display-checkpoint review

One independent review identified five corrections: shared FFT-window changes
needed batch retirement; endpoint pacing needed an advancing schedule;
no-overlap subscriptions needed explicit rejection; dropped I/Q needed input
history reset; and SDP-embedded candidates bypassed the host-only admission
path. These are corrected in the current source and covered by focused
regressions. Integrated and hardware results will be recorded against the
signed checkpoint after the combined checks finish.

The two-pane integration also exposed an R2 state-application gap: the
read-only mirrored `RadioModel.connected` delta had no client writer. The
client now applies it through the existing remote-only connection-state
setter, allowing media subscriptions to retire and resume when the radio
changes state while the station control connection stays up.

## Combined R3 display-checkpoint software gate

The final combined macOS build succeeded, followed by unfiltered
`ctest --test-dir build-integration -j10 --no-tests=error --output-on-failure`:
**672 passed, zero failed, zero skipped**, 58.96 seconds. The focused source
and daemon-controller gate passed before the full run. Logs are retained in
`~/.config/nereus/work/r3-transport-integration/final-suite-302df0e7/`.

Native aarch64 Core builds with the same reviewed source also passed on the
Rock 5C. The maintainer verified SHA-256 agreement for all 79 files in the
source overlay. Signed installation and live media evidence follow separately;
this software gate does not establish remote audio playback or R5 traversal.

## Installed display checkpoint, ea55d24a

Signed commit `ea55d24a088b6b611e1acc5d4cbc5341cdd0f459` is installed on the
Rock 5C. Native Release build, daemon/license staged installation, clean stop
and fresh authenticated receive-control verification passed. The service is
active with zero restarts. Previous `14124e7c` binaries, libraries, unit and
private configuration are recoverable from
`/var/lib/nereus-build/rollback-14124e7c-before-ea55d24a/`.

A real 20-second Saturn media run received **298 distinct decoded frames**,
298 waterfall advances and 298 wide rows, with zero decoder rejections or
keyframe-gap requests. The requested 15-fps view had a 48,046.875-Hz accepted
span, 1,024 trace samples, 1,024 waterfall samples and 768 wide samples.
[Machine-readable results](live-display-ea55d24a.json) contain no pairing data.

The packet capture shows the media using `end1` (Ethernet), with maximum
IPv4 packet size **969 bytes**. The measured DTLS flow was approximately
324.86 kbit/s Core to client and 23.31 kbit/s in return, including handshake
and SCTP acknowledgments. This is one 15-fps display profile, excluding WSS
control, radio I/Q and audio; it does not establish the total session budget
or SRTP packet-size acceptance. During the probe the service used about
102 MiB and the CPU sensor read 48.1 C.

The signed desktop GUI is running with private profile `radxa_5c_r3`.
Changing spectrum and 2D waterfall signal history were observed, including
operator band changes from 14.225 MHz USB to 3.650 and 3.830 MHz LSB. The
3D payload is arriving, and renderer tests pass; operator confirmation of
the live 3D view is pending. No RF transmission was requested by this work.

Installed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `nereusd` | `c9f6fea40007f2e7c4538a8eaa00903224c04330697e77220bdbb2b0b52ff312` |
| `libNereusCore.so` | `fc03f826e82e102f9224d7a2b053e36e4d111bc37da9fcac4e46689660836aa7` |
| `librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

Remaining R3 work includes the remote Opus sender/playback and clock-control
path, gesture-margin refinement, session budgeting, multi-pan capacity and
long hardware audio verification. R5 traversal requirements remain intact.

## Receive-path investigation, September 21

At installed checkpoint `ea55d24a`, the operator reported little/no signal.
A passive capture during operator-driven 80m/20m band changes confirmed the
Saturn filter commands follow the band. The active DDC3 frequency followed
the VFO, both receive-preselector words selected the expected band, both
step-attenuator bytes were zero, and every captured command had PTT off.
Stable ANT1 selections were Alex0 `0x01400020` on 80m and `0x01100002` on
20m. The last captured antenna change selected EXT2 (`0x01100c02` at 20m).
The physical antenna/socket path remains unconfirmed; a correct command
does not prove the physical relay or antenna feed.

A separate three-second active-DDC capture contained 2,324 packets and
553,112 complex samples, with no sequence gaps or all-zero stream. The
captured signal was predominantly noise: complex RMS -88.11 dBFS; a
4096-point Hann analysis gave a median -122.06 dBFS/bin and peak -120.17
dBFS/bin. These are raw diagnostic units, not calibrated antenna dBm.

Source tracing identified two Core/UI omissions: DaemonApp did not create
and restore the RF-gain controller, and the remote display boundary did not
apply the station's `rxMeterOffsetDb()` calibration. Corrections and their
verification are tracked under R-R3-11. This finding does not establish
that the external RF path is working, nor does it close remote audio.

The operator then reported a power issue and rebooted the Rock. Core started
automatically and reconnected to the Saturn with no service restart failures.
A subsequent raw-DDC2 capture contained 498 packets with no sequence gaps;
complex RMS was -48.77 dBFS, median -84.64 dBFS/bin and strongest peak -66.73
dBFS/bin. Clear RF peaks and populated waterfall history were then observed
in the remote GUI around 3.869 MHz, still running `ea55d24a`, before either
software correction was installed. The captures differ in operating state;
this establishes recovery, not an isolated causal test of the power supply.

The operator relaunched the default local profile, which was disconnected.
At their request, the development GUI was reopened with `radxa_5c_r3`; its
profile title, established TCP connection to Core's port 50055 and fresh
authenticated station handshake were verified. The large analog meter still
showed -127 dBm while the mirrored slice flag showed about -65 dBm. That is
a separate remote meter-binding gap; it is not evidence of missing RF.

Calibration qualification: the daemon's absent-key defaults enable step ATT
at 0 dB, giving Saturn's existing factory offset of -4.476 dB. The +15.524 dB
offset applies only with step ATT disabled and preamp mode Off. Neither
number should be presented as the proven cause of the original flat RF.

R-R3-11 software gate: the daemon now owns/restores its RF-gain controller,
tracks stable slice identities and bound band/mode, reconnects its hardware
binding, and preserves controller lifetime through teardown. Core calibrates
trace/waterfall/wide rows before reduction and quantization; remote rendering
does not add the client's local calibration. Local direct rendering is
unchanged. Eight focused test executables passed (4.50 seconds), followed
by all 672 registered tests passing with zero failures/skips (65.52 seconds).
The final renderer-fixture edit was confirmed present in that build. Logs
are in `~/.config/nereus/work/r3-rx-calibration/`. Native installation and
live corrected-checkpoint evidence follow separately.

## Installed RF-gain and calibration checkpoint, f8531cd9

Signed commit `f8531cd92676209f0acaf96b475e0d6c7a51e1b3` is installed on
the Rock 5C. The ten-file source overlay was hash-verified, and the native
aarch64 Release build passed. The first installation attempt detected a
missing staged license component and rolled back successfully. After staging
both the daemon and licenses and adding the corresponding preflight check,
the second attempt succeeded. Prior `ea55d24a` binaries, libraries, service
unit and private configuration are retained in
`/var/lib/nereus-build/rollback-ea55d24a-before-f8531cd9-attempt2/`.
The service is active with `Result=success`, `ExecMainStatus=0` and zero
restarts. This verifies a service restart; the operator's earlier board
power cycle ran the previous checkpoint.

A fresh authenticated 20-second media probe received **230 distinct decoded
frames**, 230 waterfall advances and 230 wide rows, with zero decoder
rejections or keyframe-gap requests. Trace and waterfall each contained
1,024 samples; wide rows contained 768 samples. The accepted span was
48,046.875 Hz. This bounded run includes startup and is not sustained
frame-rate or long-duration acceptance.
[Machine-readable results](live-display-f8531cd9.json) contain no pairing data.

The rebuilt desktop bundle passed strict code-signature verification. It
was reopened with profile `radxa_5c_r3`; the title showed `f8531cd9`, a fresh
authenticated handshake completed, and its established TCP connection was
verified to the Rock's wired address `192.168.109.106:50055`. Live spectrum
and populated, advancing 2D waterfall were observed at the operator's
3.650-MHz LSB selection. The large analog meter and connection/status
indicators still need remote bindings; the mirrored slice meter is the
currently useful RF indication. No RF transmission was performed.

Installed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `nereusd` | `8e0b6483c27ac79c757b41e02bdcf78eee31e8b16a07f01f066c2fc6c0faa599` |
| `libNereusCore.so` | `c0ca4f294174afb83efa14af11fe1501e9e5dd67719382c8fbf3d7a39f592e4b` |
| `librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

Remote receive audio remains unfinished: capture and Opus codec components
exist, but network audio sending, jitter/clock control and GUI playback are
not connected. Silence in this remote profile is therefore expected at this
checkpoint. The agreed next receive milestone is one mixed stereo Opus feed
preserving slice volume, mute and stereo placement. R5 traversal decisions
remain intact; neither this installation nor its display probe completes R3.

## Clarity parity investigation, September 21

The operator reported a grainier waterfall when using both automatic Clarity
and the Clarity Blue palette. Tracing confirmed that remote subscriptions
already preserve the local per-plane detector, averaging mode and alpha.
The missing boundary was Clarity's input: MainWindow connected it only to
the GUI's local `FFTEngine::fftReady`, which does not produce radio frames
in the remote role. Clarity could therefore remain enabled without updating
its thresholds, while its active flag suppressed the legacy waterfall AGC.

R-R3-12 restores the full-source percentile as bounded `noise-floor` control
metadata from Core. It is measured before display crop/reduction/quantization
and receives the station calibration once. The client routes only current,
accepted, visible active-pan measurements to the same Clarity cadence, EWMA,
deadband and operator gates used locally. The palette, detector settings and
binary display codec are unchanged. The codec's existing 180/255 dB quantum
is approximately 0.706 dB; this investigation does not attribute the reported
texture to that quantization.

The native aarch64 candidate build passed, with all four changed Core source
files verified against the immutable overlay manifest. The five relevant
test executables pass. The nonzero source regression compares Core with an
independent local FFTEngine and NoiseFloorEstimator using broadband input
plus an out-of-crop carrier, at 1,024/2,048 FFT sizes and two calibration
settings; the floor agrees within 0.01 dB. Other checks cover the zero-power
floor, context ordering, cadence, retirement, client rejection of stale or
malformed metadata, local/remote Clarity smoothing and operator gates, and
preservation of saved thresholds.

The rebuilt unfiltered desktop suite passed **672/672**, zero failed/skipped,
in 63.07 seconds. Logs are retained in
`~/.config/nereus/work/r3-clarity/`. Signed installation and live visual
evidence follow separately; this software gate does not establish remote
audio playback or exact subjective waterfall parity.

## Installed Clarity checkpoint, 3c4b15e6

Signed commit `3c4b15e67cbef1b1991bcf3d36a2b273d818272c` is installed on
the Rock 5C and in the reopened desktop bundle. The source overlay was
verified against signed Git blobs and SHA-256 hashes before the native
aarch64 Release build. Both daemon and license components were staged.
The service is active with `Result=success`, `ExecMainStatus=0` and zero
restarts. The prior installation is retained in
`/var/lib/nereus-build/rollback-f8531cd9-before-3c4b15e6/`. This verifies
a service restart, not a board reboot with this checkpoint.

The Mac bundle passed strict code-signature verification. Its title shows
`3c4b15e6` with profile `radxa_5c_r3`, and its actual TCP connection goes to
the Rock's wired address `192.168.109.106:50055`. The fresh session logged
an authenticated handshake, a Core noise-floor measurement of
**-88.9303 dBm**, and its first encrypted remote spectrum frame. Live RF
traces and slice meters were observed at the operator's 3.897600-MHz LSB
selection. No RF transmission was performed.

Installed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `nereusd` | `8e0b6483c27ac79c757b41e02bdcf78eee31e8b16a07f01f066c2fc6c0faa599` |
| `libNereusCore.so` | `8ab49b1a0786423f8471bf2aea27e15e6771133ab9eb4cb5939fcd3b7deddd0c` |
| `librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

The first live waterfall was almost black, so visual acceptance remains
open. The saved profile has Clarity enabled, palette `Default` (index 0),
black level 32 and color gain 16. With the existing renderer formula,
black level 32 adds `(125 - 32) * 0.4 = 37.2 dB` to Clarity's low threshold.
For the first observed floor, this places the effective black cutoff at
about -56.7 dBm and hides weaker signals. The next check is to remove that
profile offset and select the operator's requested Clarity Blue palette,
then compare the live result. No profile setting has been changed during
this check: the Mac locked before the controls could be inspected. This
is not evidence that subjective grain or local/remote visual parity has
been resolved. The remote audio and R5 traversal milestones above remain
unfinished.

## Applet S-meter correction, September 21

September 22 follow-up: the operator again reported a nonworking meter on
`c28e1565`. Native inspection at 18:00:56 UTC showed selected slice A and the
applet both at -107 dBm; at 18:04:13, after operator changes, selected slice B
and the applet both showed -88 dBm while A showed -93 and C -111. Source tracing
confirmed the remote reading and active-slice selection paths. This establishes
that it was updating at those observations, not that an intermittent fault was
fixed. No meter code was changed; clarification of the reported failure is
pending. The rebuilt 740-target suite, including meter regressions, passed.

The operator confirmed that the large applet meter remained unresponsive.
Its `MeterPoller` still waited for and polled the GUI's local RxChannel,
which is inactive in the remote role. The existing mirrored per-slice value
was insufficient for all applet modes: the headless pump defaults to signal
average, while S-Meter and S-Meter Peak select signal peak.

R-R3-13 adds independent, read-only `signalPeakDbm` and `signalAverageDbm`
properties to SliceModel. Core's existing meter pump produces both with the
station calibration once; the legacy selected reading is preserved. The
remote poller uses those source values for the applet, custom signal meter
items and each slice flag. Max Bin scans the decoded, calibrated display
within each slice's passband, preserving the existing measurement-pixel
path before visual notch rendering. The remote timer starts independently
of local WDSP. It clears the visible RX reading on disconnect and waits for
the current session's completed snapshot before showing retained models.
Local direct polling and widget ballistics are unchanged.

Six focused executables passed in 8.99 seconds. They cover actual Core meter
reads, source selection independent of the legacy selector, calibrated
read-only mirror round trips without outbound telemetry writes, active
slice IDs, applet/flag agreement, disconnect/TX/model destruction, and
decoded-passband Max Bin with context invalidation. Consolidated review
identified the pre-snapshot reconnect gap; a readiness callback and a
regression covering that interval were added before the final full suite.
The rebuilt unfiltered full suite passed **673/673**, zero failed/skipped,
in 53.34 seconds, including the readiness regression. The native aarch64
candidate built successfully after its six changed Core/model source files
were hash-verified. Logs are retained in
`~/.config/nereus/work/r3-applet-meter/`. Signed installation and live applet
observations follow separately; the locked Mac currently blocks the latter.

The adjacent binding audit identified two separate open requirements now in
the plan: R-R3-14 for Core's effective CH/BPF/WIDE state, and R-R3-15 for
station Auto AGC-T measurements. These still read local controller/tracker
state in the remote GUI. The existing RadioModel connection-state mirror
already has a client apply path; no blanket claim that all connection status
is broken is made here. The meter correction does not close these other
requirements or the mixed-stereo Opus milestone.

## Installed meter checkpoint, 74145a4b

Signed commit `74145a4bcfc2504547e563d230aa54b5ad7f452e` is installed on
the Rock 5C. All 18 files exported from the prior deployed checkpoint were
verified against signed Git blobs and SHA-256 hashes, then verified again
on the board. The native Release build passed, daemon and license components
were staged, and installation succeeded with the prior configuration and
binaries retained in
`/var/lib/nereus-build/rollback-3c4b15e6-before-74145a4b/`.
The service is active, with `Result=success`, `ExecMainStatus=0` and zero
restarts. Only a service restart was tested, not a board reboot.

The Mac GUI build identifies itself as `codex/integrate-r2-main@74145a4b`
and passes strict code-signature verification. It has **not** been reopened:
the Mac remained locked at the final UI check. The running old GUI is not
evidence of the new meter behavior. Visible applet/flag movement and mode
switching on the real Saturn feed remain pending, along with the earlier
Clarity profile adjustment. No RF transmission was performed.

Installed SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `nereusd` | `8e0b6483c27ac79c757b41e02bdcf78eee31e8b16a07f01f066c2fc6c0faa599` |
| `libNereusCore.so` | `f78ae36d2ae7cc3ce7cf19c6f179ee326bae814186d24bd23dedf172a4b3e8c8` |
| `librade.so.0.1` | `18e56fe8ee4b8a8450cc786cbcfed9bab147ec11df64e33486ce50e0ff91c32f` |

## Live meter observation and overnight reconnect

After the Mac was unlocked, the old `3c4b15e6` GUI was still running but
disconnected. Its log showed an overnight closed connection and six failed
redial attempts; this does not establish the underlying network cause.
Core remained active with zero service restarts. Closing the GUI and
relaunching profile `radxa_5c_r3` opened the matching `74145a4b` build and
completed a fresh authenticated handshake. The actual TCP connection was
verified to `192.168.109.106:50055`, and the client logged fresh Core
noise-floor and encrypted spectrum input.

The applet initially showed about -67 dBm with a responsive needle. A later
observation, after the operator selected the stronger signal at 3.915100 MHz,
showed -39 dBm on both the applet and active slice flag, with advancing
spectrum and waterfall. The agent did not retune or transmit. This closes
the initial real-signal needle check; all-mode and longer reconnect testing
remain distinct acceptance work.

The current manual recovery is to quit and relaunch the saved Core profile.
The normal Radio/Connect action still targets the local radio-discovery
path, so it is not yet a remote-station reconnect control. A private desktop
launcher named `Nereus Core - Rock 5C.command` now launches the correct
build/profile and rejects a duplicate instance. R-R3-16 records the missing
direct reconnect control rather than presenting relaunch as the finished UX.


## Manual Core session controls, September 21

R-R3-16 now routes Radio/Connect and Radio/Disconnect, their existing
keyboard shortcuts, and the connection context menus to the configured
Core session. The window creates one StationClient and media controller
and reuses them across manual connections. Enablement follows session
activity, including an incomplete handshake or pending retry, independently
of whether Core's radio is connected. Disconnect cancels pending retries;
Connect starts a fresh sequence without local radio discovery.

The retry schedule saturates at 60 seconds and continues retrying. The six
failed redials observed above were a log excerpt, not a six-attempt limit.
No retry-policy change is included here.

Focused session/link-loss, remote GUI gating and media-controller tests
passed (3/3, 13.46 seconds). New session regressions cover incomplete
handshake activity, authenticated Core with an offline radio, repeated
manual sessions on the same client, cancellation during backoff and a fresh
retry sequence afterward. The matching `all_tests` and GUI targets were
built, followed by an unfiltered **673/673 pass, zero failed or skipped,
60.94 seconds**. Strict app code-signature verification passed.

The first full run and isolated retry hit the same native popup-exposure
wait in the existing 3D span-control test, before its state assertions ran.
That state-only test no longer opens a native popup; all enablement assertions
remain, and the separate real SpectrumWidget interaction test remains intact.
The final full run above includes this test correction.

Live GUI acceptance is pending at this source checkpoint. Core remains at
`74145a4b`; this client session-control change requires no wire-schema or
server behavior change. Remote audio is still incomplete. The source audit
identified WDSP rmatch/varsamp as a continuous-rate candidate, but the bus
currently lacks public device-consumption/queue-depth feedback. Add that
feedback before validating the clock loop; a timer alone cannot measure the
Mac sound device's clock. Task 5 records the sender, receiver/playback
and output-control implementation sequence. The existing order, remaining
task 4a receive bindings followed by task 5 audio, is retained.


## Live manual reconnect, 73fcccfe

The signed GUI was built and strictly code-signature-verified, then opened
with the existing `radxa_5c_r3` profile (PID 19939). Core remained running at
`74145a4b`, active with `ExecMainStatus=0` and `NRestarts=0`.

In the actual Radio menu, Connect was disabled and Disconnect enabled while
the session was active. Selecting Disconnect removed the TCP connection
without closing the GUI, enabled Connect, and disabled Disconnect. Selecting
Connect on the same process authenticated again at 09:10:47 local time and
received fresh Core noise-floor data. The established TCP endpoint was
`192.168.109.104:62313 -> 192.168.109.106:50055`.

The visible GUI then showed live spectrum and populated waterfall, with the
applet and both slice flags at -57 dBm on the operator's 3.952200 MHz tuning.
No frequency, gain, filter, mute or transmit control was changed for this
check. This closes the initial live R-R3-16 menu round trip; multi-hour link
recovery and audio reconnect acceptance remain separate work. The GUI is
left running and connected. Audio playback remains task 5, following the
remaining task 4a receive bindings.


## Core filter and Auto AGC-T bindings, September 21

R-R3-14 publishes each Core filter chain's mode, effective state, band and
reason as outbound-only RadioModel telemetry. Remote CH and WIDE indicators
use those values and the already-mirrored slice chain assignment. Presentation
waits for the complete station snapshot, including slice reconciliation, and
is unavailable after session retirement. The Filter Policy dialog shows the
reported Core state read-only; remote policy editing remains unavailable.
An offline/awaiting dialog explicitly withholds the retained prior reading.

R-R3-15 now has a daemon-lifetime, bounded FFT source and the existing
NoiseFloorTracker for each active stream. Previously, only MainWindow created
these trackers and Core's existing Auto AGC-T timer skipped all enabled slices.
Core now feeds the same FFT-to-tracker path used locally, independently of
client display subscriptions. It does not substitute the Clarity percentile
floor or introduce new AGC threshold math. Retune, mode, routing, attenuation,
MOX, stream suspension and disconnection invalidate the measurement until
fresh tracker convergence. Unbound slices become invalid immediately, and
source teardown unregisters every borrowed tracker before destroying it.

Three read-only per-slice properties carry the floor, validity and generation.
The GUI flags remain per-slice; the RX applet follows the active slice and
shows an awaiting state when no valid station reading exists. Its AUTO toggle
resolves the active slice when clicked. Local direct mode keeps its own
per-stream source.

Focused tests passed 7/7 (22.14 seconds), followed by a rebuilt full suite
675/675 (63.45 seconds). Consolidated review found the unavailable-dialog case
and two coverage gaps. The correction adds snapshot-completion gating, an
unavailable dialog state, and a test of the production two-slice GUI binding
with active switching, telemetry deltas, local-floor contamination and
connection loss. All six affected tests then passed (21.83 seconds).
The post-review full run exposed a pre-existing test timing race: polling for
exactly one scheduled retry could miss the compressed 50 ms retry under load.
The test now starts the listener synchronously on the first scheduling signal
and still verifies automatic, unaided reconnect. After rebuilding that test,
the complete suite passed 675/675 with no skips in 61.27 seconds.

No new binary is installed at this source checkpoint. Core remains
74145a4b and the GUI remains 73fcccfe. Live filter/Auto AGC-T observation on
the Saturn/Rock/Mac path remains pending. These changes do not close R3's
audio, capacity or long-run acceptance gates. Task 5 audio integration is
continuing after the receive-binding implementation.


## Receive deployment and audio integration progress, September 21

Signed receive checkpoint `04da2aab` was built natively and installed on
ROCK 5C at 10:20 EDT. Both `nereusd` and NereusCore were staged with licences.
The rollback copy is `rollback-74145a4b-before-04da2aab`. Service checks
reported active/running, zero restarts and exit status zero; the existing
GUI re-established its session automatically. The previous GUI binary does
not yet display the new filter and Auto AGC properties, so visual acceptance
awaits the combined GUI update.

The first combined audio build completed. Six of eight focused tests passed:
Opus codec, actual mixer-to-sender, audio session controls, jitter ordering,
PortAudio device pacing/flush, and existing GUI display lifecycle. Two failures
were investigated before deployment:

- The 1-hour drift gate found 20,202 underflows for the first tested clock
  direction. WDSP's feedback windows count calls; direct 1,920-frame input and
  480-frame output exposed only 125 calls/second. A separate probe using the
  source's native 64-frame blocks and unchanged WDSP defaults had zero
  under/overflows in both drift directions for 300 simulated seconds. The
  bounded 64-frame adapter subsequently passed the full-hour gate in both
  directions with zero rate-matcher underflows or overflows (104.83 seconds
  wall time for both simulations).
- A less-than-0.1 channel-correlation assertion was incompatible with the
  approved lossy 24 kbit/s Opus profile. A direct pinned-codec probe measured
  correlation 0.1042 and 23.5/27.5 dB tone isolation for 997/1703 Hz L/R inputs,
  reproducing the playback result. The receiver test now requires at least
  18 dB intended-channel isolation and nonzero channel energy. That check
  passed. The same direct probe at 48 kbit/s measured about 64-71 dB isolation;
  actual listening comparison is still pending, and the default is unchanged.

Consolidated audio review identified two integration defects: a fixed 960-frame
speaker target could not cover supported 1,024/2,048-frame callbacks, and a
three-packet arrival burst could overflow the rate matcher. Corrections now
report the actual/configured callback quantum and cover it when replenishing;
jitter release waits for room in the rate matcher. Explicit diagnostics reject
silent under/overflow repair. Large-callback and 3/8-packet burst tests pass (5.31 seconds). Larger
speaker queues retain the same initial rate-matcher reserve; the 480, 1,024
and 2,048-frame callback cases pass without rate-matcher under/overflow.
The real DTLS/SRTP two-controller test also passes (2.48 seconds), including
stereo tones from the actual master mixer, local mute without changing slice
mix settings, fresh resume context and session disconnect/reconnect. Its
isolation checks compare the same tone across the two channels, preserving
the distinct per-slice gains. These are automated fixtures, not physical
speaker listening. Audio has not yet been installed or accepted on hardware.


The first rebuilt 681-test full run found three failures. The clock simulation
exceeded the general 120-second timeout under parallel load; its dedicated
limit is now 180 seconds without reducing either hour of simulated samples.
Two existing real-channel tests crashed after `WDSP:CreateSemaphore: File
exists`. Both crash reports traced to the macOS compatibility layer's
process-local semaphore names colliding between concurrently running test
processes. The narrow correction uses process-unique names and immediately
unlinks successful named semaphores; it changes no DSP calculation. Focused
concurrent regression and the final complete rerun are recorded below when
finished. No audio installation is accepted on the basis of this failed run.


## Combined audio software gate, September 21

The semaphore collision reproduced before the correction; the same concurrent
pair passed afterward. A rebuilt unfiltered full suite then passed **681/681,
zero failed and zero skipped**, in **136.57 seconds**. The continuous clock
test completed in 136.55 seconds under the parallel workload, retaining both
simulated hours and zero-underflow/overflow assertions. The GUI target built
successfully. Detailed build/test logs are in the maintainer's private work
directory as `r3-audio-final-{build,test}.log`.

The earlier repeated real-device stress run passed 18 test invocations before
a later PortAudio reopen hung; the exact concurrent regression and the final
whole suite passed. This does not establish unlimited physical-device
reopen/close stress acceptance. Hardware audio listening, native CPU/bandwidth
measurement and the two-hour live soak remain pending at this source gate.


## Audio checkpoint awaiting installation

Signed audio checkpoint `0123d8e1` passed the repository attribution hooks.
The immutable source overlay and SHA-256 manifest were prepared from Git.
The first transfer ended with a closed connection before the native build
started; subsequent SSH attempts to both board addresses and a ping to the
home router timed out. The existing GUI also lost its control/media session.
The operator had confirmed that the Mac was now remote/on VPN. No network
settings were changed, and no audio binary was installed: Core remains
`04da2aab`, with the old `73fcccfe` GUI still running. Installation resumes
when the LAN route is available, followed by decoded live audio diagnostics
and physical speaker listening. This interruption is not R5 traversal work.

The route subsequently recovered, and the verified `0123d8e1` overlay reached
the board. Native compilation is underway. The optional `nereus-media-probe --audio` diagnostic now builds and advertises its flag. It validates the current
audio context before decoding and reports finite PCM energy, actual stereo
bandwidth/channels, byte counts and discontinuities. A successful diagnostic
is transport/decoder evidence and does not replace physical listening.


## Installed first-audio checkpoint, 0123d8e1

Native aarch64 Release build and daemon/licence staging passed. The 36-file
overlay matched the signed Git SHA-256 manifest. Core `0123d8e1` was installed
at 11:34 EDT with rollback to `04da2aab`; service checks reported active,
`Result=success`, `ExecMainStatus=0`, and `NRestarts=0`. The Mac GUI was rebuilt
at diagnostics checkpoint `8a23f8f7`, passed strict code-signature verification,
and opened with the existing private `radxa_5c_r3` profile. Its production audio
code matches Core `0123d8e1`.

The bounded live diagnostic received 218 distinct display frames and 117
stereo wideband Opus packets (224,640 decoded PCM frames), with finite channel
RMS approximately 0.09138 and no audio sequence/timestamp gaps or rejections.
The first diagnostic attempt ended early on a temporary disabled context;
the probe was corrected to wait for Core's own media-ready transition. The
production GUI already handles this transition. The capture is evidence of
nonzero live decoding, not sustained delivery at the nominal packet rate.

The operator confirmed first sound: audio works for a while, then becomes
bursty/choppy. This does **not** pass continuous playback acceptance. A separate
25-second Core-side capture establishes normal source cadence: 602 SRTP audio
packets over 24.249 seconds, median 39.986 ms between packets and maximum
89.980 ms. Ethernet carries both media paths. Display traffic measured about
460.56 kbit/s in this selected GUI view (maximum IP packet 969 bytes); audio
plus its small UDP control overhead measured about 35.1 kbit/s (maximum IP
packet 175 bytes). This excludes WSS control and radio I/Q and is not the
complete session-budget gate.

The capture exposed the chop mechanism: three RTP timestamp steps of 1,984
frames and one of 2,944 frames, instead of the normal 1,920. A short capture
lock miss abandoned a partial packet and resumed off the negotiated packet
grid. The jitter buffer then rejected following packets until the 500 ms
watchdog reset the context. Three captured timestamp shifts correlate with
GUI restarts approximately 500 ms later. The correction keeps the honest
source clock but resumes packet assembly at the next whole packet boundary,
so the existing loss-concealment path can recover. Verification follows at
the correction's checkpoint; the first-audio build remains installed for now.

Initial live receive binding observations in the new GUI show the active
slice and large applet agreeing at -52 dBm, the Core-provided Auto AGC floor
around -105 dB with AUTO active, and CH 0 selecting 20m. Spectrum and the
Clarity Blue 2D waterfall advance. These observations do not close all band,
meter-mode, reconnect or subjective Clarity acceptance cases.


## Partial-ingress loss correction

The deterministic regression reserves and rejects a 64-frame ingress callback
inside a partial packet, then verifies that the recovered blocks begin at
source frames 1,920 and 3,840 with unchanged stereo samples. Consumer allocation
now occurs outside the capture mutex, reducing the opportunity for ingress
lock misses. The source position still includes dropped frames; the correction
does not conceal loss by renumbering time.

The rebuilt source, sender, daemon session, receiver and real encrypted session
tests all passed (5/5, 4.86 seconds). Full-suite and native installation results
will be recorded after the signed correction is built. The live diagnostic's
temporary-disabled-context handling is included in this correction.


## Installed packet-grid correction, 5c24eda0

The rebuilt unfiltered suite passed **681/681, zero failed/skipped**, in
101.49 seconds; both simulated one-hour drift cases remain intact. Native
ARM build and staged daemon/licence installation passed. Core `5c24eda0`
replaced `0123d8e1` at approximately 12:01:50 EDT, preserving a rollback at
`/var/lib/nereus-build/rollback-0123d8e1-before-5c24eda0`. Service result and
exit status are successful with zero restarts. The existing `8a23f8f7` GUI
automatically reconnected.

A three-minute wired capture contains 4,242 SRTP packets over 179.213 seconds:
4,220 timestamp steps of 1,920 frames and 21 of 3,840, with **no off-grid
steps**, continuous sequence numbers and zero kernel capture drops. Median
spacing is 40.076 ms; the maximum 215.140 ms includes the reconnect phase.
The capture confirms the source correction. Playback is still **not accepted**:
initial watchdog resets continued until about 12:02:15, followed by a
62-second context before a rate-matcher underflow and later arrival-queue
bursts. Client restart diagnostics are being added to distinguish input
arrival stalls from consumer scheduling and clock behavior.

A 10-second one-pan live sample measured Core at 61.9% of one CPU core,
138,916 KiB resident memory and 47.153 degrees C. This is an observation
under the current receiver configuration, not four-pan capacity evidence.

The operator reported smoother audio after this correction, with occasional
stutters accompanied by a waterfall slowdown. Receiver restart diagnostics
were built into signed GUI `a7a6da39`; receiver and encrypted-session tests
passed 2/2. The first cold-start observation shows a three-second speaker
open followed by an arrival backlog, so startup backlog remains an explicit
follow-up rather than evidence of radio silence.

At approximately 12:07:57 the media connection failed, followed by a WSS
timeout. Both Rock addresses became unreachable while the router, Saturn
and MikroTik still answered pings. A read-only MikroTik query at 12:11
reported **no physical link on ether2**, the port identified by the operator.
No switch or VPN settings were changed. Further live acceptance waits for
board/link recovery; this outage must not be counted as an audio pass or
attributed to the receiver without additional evidence.

Transport diagnostics now record library RTP callback spacing and queue age
at actual owner-thread delivery, after display processing. Warnings above
80 ms are bounded to one per second and contain timing/counts only. The
existing 64-packet queue, retirement checks and transport API are unchanged.
The rebuilt transport, media peer, receiver and encrypted audio session tests
passed 4/4 in 5.04 seconds. Full-suite verification follows this source gate.


## Diagnostic GUI checkpoint, 4d923aeb

The complete desktop/test build and strict application code-signature check
passed. An unfiltered 681-test run passed **674 and timed out seven** in
160.95 seconds. The failures are `tst_port_audio_bus`,
`tst_audio_engine_speakers_live_reconfig`, `tst_connectable_radio_model`,
`tst_connected_state_equivalence`, `tst_remote_role_inert`,
`tst_session_verbs`, and `tst_slice_meter_pump`. A live sample of the last
test shows `Pa_OpenStream` blocked in the macOS Core Audio hardware-property
RPC while opening the microphone through `ensureTxInputOpen`. A subsequent
serial failed-test retry remained in the first PortAudio test for 45 seconds
and was stopped as a bounded diagnostic. No tests were excluded, no device
settings or audio services were changed, and this run is **not** reported as
a full pass. The production source correction's earlier 681/681 pass and
the diagnostic changes' focused 4/4 pass remain separate evidence.

Logs are retained privately as `r3-diagnostics-final-{build,test}.log`,
`r3-diagnostics-serial-audio-test.log` and `r3-diag-test-hang-sample.txt`.
The `4d923aeb` GUI was reopened with the existing station profile. It waits
for the Rock's link to return. A second read-only switch check still showed
ether2 not running, with switch-recorded last link-down `2026-09-21 12:06:40`
(the switch clock was not compared against the Mac). Remaining playback,
long-soak and capacity gates stay open.

## Tuning waterfall history regression

The operator reported that both ordinary tuning and C-Tune wiped the
waterfall. The remote client cleared all 2D/3D history when issuing a changed
subscription and again when accepting Core's context. Local tuning already
has its own history policy; renewing a remote codec must not add a full wipe.

The correction separates invalidation of live/pending planes from full
binding retirement. Subscription/context renewal preserves painted history,
while disconnect, replacement and rejection retain their full-clear paths.
Core's bin-aligned accepted geometry uses the existing local waterfall
reprojection; 3D rows retain their recorded RF centre/span. Codec, revision,
generation and view guards still reject obsolete incoming media. No Core
source invalidation, DDC policy or DSP formula is changed.

Before the correction, regressions failed for a fixed-view source retune,
a small tuning move, the request/ACK interval and RF-aligned history. After
the correction, the rebuilt renderer, authenticated media controller, 3D
row tee and 3D ring tests passed **4/4 in 5.24 seconds**. The renderer checks
actual 2D images/timestamps, 3D row RF identity, continued painting and old
generation rejection. Full-suite and installed-GUI evidence follow below.
Live tuning verification remains pending: both known Rock addresses still
failed SSH reachability at this checkpoint.

The rebuilt unfiltered suite passed **674/681**, with the same seven
audio-device tests listed in the diagnostic checkpoint timing out, in
157.89 seconds. This is not a full pass. After tightening the controller
fixture to retain its post-tune Clarity gesture guard, that rebuilt test
also passed. The production code was unchanged by that test-only follow-up.
Private logs: `r3-tune-{red,green,full}-{build,test}.log` and
`r3-tune-final-focused-{build,test}.log`.

The source audit also identified a separate remote C-Tune control gap.
MainWindow's C-Tune pan drag currently calls the client-local
ReceiverManager; a Remote-role client has no wired radio connection, and
its resulting `shiftOffsetHz` change is daemon-authoritative and is not
forwarded. C-Tune pinning is also GUI-local. Ordinary `frequency` writes
do reach Core. This is not evidence of a mislabelled Core FFT source, and
the visual correction does not close C-Tune hardware-control parity.
Follow-up needs an authenticated Core-owned pin/centre operation with
shared-stream, bounds, reconnect and remote-inertness coverage, reusing
the existing local allocator/receiver behavior.

## Connection controls and feedback audit, September 21

The operator reported that reconnect and Connect/Disconnect appeared not to
work, and asked whether their GUI was incomplete or covered by the plan.
On the running `ef3e69d7` GUI, the Radio menu initially disabled Connect and
enabled Disconnect during an active connection attempt. Selecting Disconnect
enabled Connect and disabled Disconnect. Selecting Connect started a fresh
sequence; the following socket timeout scheduled retry attempt 1. The bottom
station block continued to say `Click to connect` throughout. These are
observations of cancellation and redial, not successful reconnection.

At this checkpoint Rock `.106` TCP ports 22 and 50055 timed out, `.105` SSH
was unavailable, and MikroTik `.85` SSH was reachable. The earlier physical
ether2 link-down remains prior evidence; no new switch-link read or network
configuration change was performed in this audit. The operator was asked
to check the board's power and Ethernet link.

Source tracing confirms a second, independent problem: title/status/pan
click-to-connect affordances call `MainWindow::showConnectionPanel`, whose
remote branch only logs a suppression message and returns. The user hit
that path at 13:45. Remote context menus share the working menu actions,
but StationBlock itself suppresses its context menu when painted disconnected.
Existing transient toasts do not provide persistent Core/retry status, and
the chrome still follows mirrored radio state rather than the Core session.

Do not turn the central suppressed-panel path into an unconditional dial:
`onConnectionStateChanged` also invokes it automatically after disconnect
when a radio name is retained. Such a change could redial during teardown
and defeat explicit cancellation. The correction must distinguish operator
affordances from automatic local-mode panel opening and cover both paths.

No product code or deployed binary was changed in this discussion/audit.
The plan now explicitly retains the verified menu work while reopening full
R-R3-16 interface acceptance and adding R-R3-17 for minimum persistent
identity/status/error feedback. The broader station-selection/pairing screen
remains grounded in the existing identity design and the R5/R6 roadmap.


## September 21: power recovery and C-Tune spectrum restart

The operator identified the outage as USB power shutting off. After power
returned, the running `ef3e69d7` GUI automatically reconnected at 13:54:37
local time. The operator confirmed that small tune gestures now preserve
painted waterfall history. This is live acceptance of the history fix, not
of the outstanding 3D, audio-soak or complete connection-interface gates.

The next report was a spectrum trace dropping from the top on each C-Tune
wheel step. Source inspection found two independent defects:

- `SpectrumAvenger` reset its recursive log accumulator to zero (0 dB).
  The wrapper's comment promised a mode-dependent first-apply seed, but
  that seed was absent. The existing port now follows Thetis
  `wdsp/analyzer.c` `SetDisplayAverageMode` at v2.10.3.15 / `3759d096`:
  log mode starts at -160 dB and linear mode at `1e-12`.
- The GUI's C-Tune pin and explicit pan-centre calls only reached its
  inert local receiver. Core saw an ordinary frequency write and retuned
  a sole stream, recreating the display reducer on every wheel step.

The averaging regressions failed before the correction and pass after it:
recursive linear/log startup, clear, resize and endpoint source-retune
reset, including rejection of frames from the old source. Reducer,
endpoint, widget-parity and remote-render targets: **4/4 passed, 2.02 s**.

R-R3-18's implementation uses two typed authenticated commands:
`requestStreamCtunPinned(sliceId, pinned)` and
`requestStreamCentre(sliceId, centreHz)`. Core resolves the current stream,
maintains its pin independently of other streams, projects the effective
pin to every slice sharing it, and owns all DDC/shift/notch-origin updates.
The centre command refuses a move that would displace a cohost outside
its receive window. Ordinary VFO tuning still uses the existing mirror.
Session minor 2 and `remoteCtunVersion=1` gate this new command path.
The GUI restores a saved preference once per acquired stream, displays
Core's effective pin, and retains its preference when support is absent
or the connection closes. The original local C-Tune path is preserved.

Verification gates for the combined change: authenticated wheel tuning
leaves source centre/context unchanged; explicit pan motion updates centre
and all cohost shifts; unpin restores normal tuning; pins on independent
streams do not interact; disconnect clears Core pins and reconnect restores
the GUI preference. Matching Core and GUI installation and live operator
acceptance are still pending at this checkpoint.


The consolidated review identified two lifecycle gaps before installation:
a refused pan command needed GUI rollback, and logical stream indices could
be reused before a GUI poll saw the empty state. The correction adds a
Core-owned, outbound `streamEpoch` for each stream lifetime, epoch-scoped
command outcomes, and restoration completion only after Core accepts the
pin. Refused centre moves restore affected views to their last accepted
Core source centre without echoing another hardware command. Context/frame
admission also checks the current stream lifetime.

The initial combined full suite passed **681/681 in 120.44 s**; the previous
macOS microphone-open timeouts were absent in this run. After the review
correction, typed-command tests passed in **2.14 s** and the GUI/controller
regressions passed in **5.76 s**, including a cohost-blocked pan move and
retirement/reuse of the same stream index within one event-loop turn.
The final combined full run and matching native installation are in progress.

A subsequent operator report concerns the frequency-scale bandwidth drag
snapping back on release. Investigation is separate from acceptance of the
C-Tune correction; a sample-rate clamp has not yet been established as its
cause. The currently running GUI is still `ef3e69d7`.


The zoom investigation confirmed a capability mismatch (R-R3-19). The local
Extended-view preference allowed a scale drag up to the ADC-wide ceiling,
while remote media supplies only the current DDC source. Core correctly
clamped that oversized crop and its ACK replaced the GUI view. Zoom is not
the local sample-rate selection path, so a drag now stops at the remote
DDC limit rather than changing a shared stream's sample rate. The saved
local Extended-view preference remains intact, and local wings still work.
Remote ADC-wide transport remains unimplemented and is not implied by this fix.

A real press/move/release regression reproduced **450 kHz requested from a
192 kHz source** before the fix. It now stops at 192 kHz during the drag and
remains there after the source ACK. Remote render, authenticated controller,
and local extended-wing tests passed **3/3 in 6.99 s**. The preceding final
C-Tune full suite passed **681/681 in 99.23 s**. A combined full run including
the small zoom correction passed **681/681 in 94.38 s**, with all test targets
rebuilt first and no exclusions. Both candidate native Core builds passed;
the signed checkpoint still needs its final build tag, staged install and
matching GUI before live acceptance.

The operator then clarified that zoom must retain the existing wideband
extended-pan capability. The May 26 Wideband Extended Pan plan and current
local ADC capture/FFT/wings implementation confirm that this is existing
feature parity. R-R3-20 and task 4b now explicitly track its missing remote
transport, Core-owned shared capture/filter state and local/remote comparison.
The DDC-only clamp is recorded as an interim fallback, not a finished zoom
implementation.

The repository's per-pan wiring check blocked the first commit attempt. Its
sender convention exposed a real timing gap: immediately after selecting a
new slice, a gesture could use the previous subscription's slice until the
100 ms refresh. Both remote gesture handlers now resolve the owning pan's
current slice at invocation. The new regression failed with slice 0 instead
of slice 1 before the change; the authenticated controller test passes after
it (**5.97 s**), and the unmodified repository wiring check passes. A fresh
combined run follows this last correction.

The following combined run passed 680/681 and exposed a related rollback
race in the new slice-selection scenario. A replacement subscription lost
the last accepted source centre while awaiting its first frame. Rebinding
the same widget to a cohost in the same stream lifetime now preserves that
centre; a different stream or lifetime cannot inherit it. The focused
controller regression then passed three consecutive runs (**15.87 s total**).
The final combined gate is rerun after this correction.

Final gate after the gesture-time routing and shared-stream rollback fixes:
**681/681 passed in 93.87 s**, all targets rebuilt first, no exclusions.
Matching signed Core/GUI deployment and operator gesture acceptance follow.


## September 21: signed tuning checkpoint installed

Signed source checkpoint **501b2701** passed the final **681/681** desktop
suite and unchanged repository hooks. The matching Mac app built and passed
`codesign --verify --deep --strict`. All 34 changed-file hashes since the
previous Core checkpoint matched on the Rock 5C; native Core source was
unchanged from the passing reviewed native build. Final native configure,
build, daemon/license staged install and dependency/help checks passed.

Core was installed at about 14:55 EDT, retaining
`rollback-5c24eda0-before-501b2701`. Clean stop and start passed, with
`ActiveState=active`, `Result=success`, `ExecMainStatus=0`, `NRestarts=0`.
The installed `libNereusCore.so` SHA-256 is
`d50411c42ea89860aa048843abdac799c2a8d780fe682a3231170e79476d75a2`.

The matching GUI reopened with the dedicated profile and visible build tag
`501b2701`. Station handshake completed at 14:55:42; the first encrypted
spectrum frame arrived at 14:55:45. A live screenshot at 14:56 showed the 3D
spectrum, advancing 2D waterfall and applet/slice signal readings on 40m LSB.
MOX, TUNE and VOX remained off. The operator's wheel/scale-drag acceptance
question is pending; a working display is not proof of those gestures.

One audio startup restart coincided with a 2676 ms GUI-owner queue delay;
reception restarted as context 3. Subsequent short observation still showed
80–88 ms packet-arrival gaps. Sustained audio and the two-hour run remain
open; this tuning checkpoint does not claim to fix them. The old GUI's
pre-update media outage and misleading connected/disconnected chrome also
reinforce the existing R-R3-16/17 acceptance gates.

The operator subsequently confirmed **“Both now behave smoothly”** for
C-Tune wheel tuning and frequency-scale drag zoom within the current
bandwidth on the installed 501b2701 build. Those reported gesture regressions
are now live-accepted. This does not close R-R3-20 wideband transport, sustained
audio, connection feedback or multi-pan capacity acceptance.
