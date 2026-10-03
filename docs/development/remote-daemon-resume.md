# Core and remote GUI: restart point

Recovered on 2026-09-20 by J.J. Boyd (KG4VCF), with OpenAI Codex assistance.

## Where the work lives

| Work | Location | Verified state |
| --- | --- | --- |
| Shipping baseline | `main` at `efd88e69` | Does not contain the daemon split |
| R1 foundation | [PR #315](https://github.com/boydsoftprez/NereusSDR/pull/315), branch `claude/nereus-thin-client-arch-8f24ec`, tip `a7324cdd` | Open; conflicts with main. Its last Linux, Windows, macOS, compliance and CodeQL checks passed. Parts of its PR description still predate those results. |
| R2 control session | `claude/remote-daemon-r2`, worktree `/Users/j.j.boyd/NereusSDR/.worktrees/remote-daemon-r2` | Recovered clean at `efddd7ea`, 75 commits after R1. No remote branch or R2 PR was found. |
| R2 integrated with main and selected open PRs | `codex/integrate-r2-main`, worktree `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR` | Signed checkpoint `14124e7c` includes the published 3D waterfall and other selected PR work. Both executables build; unfiltered desktop suite passes 662/662 with a documented local fixture-path workaround. |
| R3 media | [September 20 implementation plan](../architecture/2026-09-20-remote-daemon-r3-plan.md), extending Claude's architecture and addendum | Codec, headless FFT source and encrypted media adapter in progress; end-to-end remote media not yet implemented |

The build targets are `NereusCore`, `NereusGui`, `nereusd`, and `NereusSDR`.
"NereusUI" refers to the existing GUI running in remote mode; there is no
separate executable with that name yet.

## What works and what remains

R1 extracts spectrum production from the GUI and builds a headless daemon.
It was built and run on a Raspberry Pi 4B, 8 GB, Debian 13 aarch64. Its
bench recorded multi-slice I/Q degradation and a systemd shutdown crash.
The missing installed RADE library was subsequently fixed.

R2 implements TLS and token authentication, the state mirror, station settings
proxy, slice commands, meters, heartbeat/reconnect, and `--station` GUI mode.
Its 20 planned tasks and subsequent code review fixes are committed. Its
last August handoff still had incomplete hardware and UI acceptance rows.
The August 9 result of 618 passing tests excluded ten known failing tests;
it was not a clean pass of every registered test.

R2 intentionally has **no remote spectrum, waterfall, audio, or transmit**.
R2 plus R3 is the agreed remote receive release unit. R4 adds transmit and
its safety checks. R5 adds NAT traversal and relaying.

## Work performed on September 20

Two unresolved PR #315 review findings also existed on R2:

1. `DaemonApp`'s connection-state relay dereferenced its owning `unique_ptr`
   after `reset()` had cleared it. A regression reproducing that lifetime
   window failed with SIGSEGV. The relay now uses the emitted state value.
2. The termination signal handler posted a queued Qt call, allocating inside
   signal context. The new regression caught allocation for both SIGTERM
   and SIGINT. The handler now sets a `sig_atomic_t` flag; a main-thread timer
   requests shutdown. Subprocess tests also exercise the actual entry point
   and require both signals to produce normal exit code 0.

Both executables were rebuilt. These six test executables passed after the
fixes on macOS arm64 with Qt 6.11.0:

- `tst_daemon_app`
- `tst_daemon_signals`
- `tst_remote_slice_commands`
- `tst_station_session`
- `tst_session_link_loss`
- `tst_remote_gui_gating`

This is focused verification, not a new full-suite or cross-platform claim.
The fixes are on the R2 worktree; PR #315 has not been updated with them.

The integration branch resolves conflicts in `CLAUDE.md`,
`tests/CMakeLists.txt`, and `src/gui/MainWindow.cpp`. The connection-state
handler keeps R2's null-safe local-connection fallback and uses main's new
station hardware row and layout update. The isolated macOS RelWithDebInfo
build passed `tst_daemon_app`, `tst_daemon_signals`, `tst_station_session`,
`tst_remote_gui_gating`, `tst_layout_thumbnail`,
`tst_pan_layout_dialog_gating`, `tst_pan_menu_routing`,
`tst_mainwindow_status_bar_safety`, and `tst_station_block` (9/9).
All commit hooks passed. A Debug configure exposed an existing ASAN link
flag mismatch for `nereusd`; that separate build issue remains open.

## Hardware rediscovery

The current target is the maintainer's **Radxa ROCK 5C running Armbian
26.8.3 Trixie**, verified over SSH on September 20. It has 2 GB RAM, a
29 GB root filesystem, and the `6.1.115-vendor-rk35xx` aarch64 kernel.
The existing administrator is `yonder`. Wired Ethernet is
`192.168.109.106`; Wi-Fi is `192.168.109.105`. SSH key access from the
maintainer's Mac is configured, and both addresses present the same host key.

The OEM fan is already supported by the running device tree and kernel:
`cooling_device5` is `pwm-fan`, `hwmon7` exposes PWM control, and the
`soc-thermal` zone uses `step_wise` with a 60 C initial fan trip. Idle was
32-35 C. A manual maximum-state command read back state 4 / PWM 255, and
the maintainer confirmed the fan physically spun. The kernel then returned
it to idle automatically. No custom fan service or governor replacement
was installed. There is no fan RPM sensor exposed on this board.
A 90-second, eight-worker CPU load check subsequently exercised automatic
control: the fan advanced to state 1 / PWM 64 and the hottest sensor peaked
at 62.8 C. All load workers were stopped after the check.
See [Radxa's fan interface documentation](https://docs.radxa.com/en/rock5/rock5c/getting-started/interface-usage/fan).

The previous SBC was `raspberrypi-flex`, reached as `jj@192.168.109.133`.
SSH to that address timed out on September 20; `raspberrypi-flex.local` did
not resolve. That previous Pi is not the selected installation target.

Read-only P1 and P2 discovery on the current LAN found:

| Radio | IP | MAC | Board byte | State at discovery |
| --- | --- | --- | --- | --- |
| ANAN-G2E / HermesC10 | `192.168.109.198` | `40:84:32:B0:B0:8D` | `0x14` | Idle |
| ANAN-G2 / Saturn | `192.168.109.45` | `2C:CF:67:AB:FC:F4` | `0x0A` | Idle |

The earlier bench instructions authorized the G2E and excluded the G2.
On September 20, the maintainer explicitly changed the target to the
**ANAN-G2 / Saturn**. A fresh P1/P2 discovery from the Rock 5C found that
radio idle at `192.168.109.45`, MAC `2C:CF:67:AB:FC:F4`, board `0x0A`.
The SBC configuration now pins that MAC. Repeat both discovery probes and
identify by board byte before a future run; never fall back to the first
radio discovered.

September 20 bench used separate `r2resume_20260920` daemon and
`r2resume_20260920_client` GUI profiles, loopback TLS port 50056, one slice,
192 kHz, and the G2E's pinned MAC. No MOX or TUNE command was issued.

- First daemon run generated its cold WDSP cache and logged
  `Connected to "ANAN-G2E"` at 20:06:10 local time. Both processes exited 0
  on SIGTERM. The first client attempt correctly refused a fingerprint
  supplied in the wrong format by the temporary launch helper; the helper
  was corrected to the documented colon-separated format.
- On restart, discovery found only the excluded G2. The daemon remained
  disconnected instead of selecting a different radio. The G2E no longer
  answered either broadcast or direct discovery. This follows shutdown but
  does not yet establish its cause; the radio's power/network state and
  reconnect behavior need investigation.
- The second GUI established the authenticated TLS session. Settings opened
  through the remote proxy. Radio > Connect, Disconnect, Manage Radios and
  Protocol Info were visibly disabled. Spectrum and waterfall stayed blank,
  as expected for R2. The session survived opening Settings.
- Both second-run processes also stopped with exit code 0. No bench process
  was left running. A direct P2 discovery retry and ping still received no
  answer from the G2E; the G2 remained discoverable.
- The live-connected GUI, populated-settings round trip, moving meters,
  multi-slice throughput, and systemd lifecycle are **not verified** by
  this run. Do not promote these partial observations to full R2 acceptance.

A later discovery from the Rock 5C's wired interface found the G2E again
at `192.168.109.198`, with the same MAC and board byte, idle. This restores
the opportunity for an SBC bench; it does not explain the earlier outage.

The original temporary logs and supervised launch helper were under
`/tmp/nereus-resume-20260920` and were lost during the Mac upgrade/reboot.
Committed sanitized evidence remains available. New private build/probe work
is stored under `~/.config/nereus/work/`; pairing data remains outside the repo.

## Rock 5C installation and live R2 verification

The first native Release build of signed checkpoint `daf05135` completed on the
Rock 5C with Qt 6.8.2, GCC 14, OpenSSL 3.5.7, WDSP and FFTW. GPU rendering,
tests and DFNR were disabled for this headless build. The installed executable
is `/usr/local/bin/nereusd`, with its required RADE shared library installed.
The stock systemd unit is enabled and uses its reserved `daemon` profile.

`/etc/nereusd.conf` pins the Saturn MAC, one slice at 192 kHz, and the TLS
control listener at `wss://192.168.109.106:50055`. Pairing credentials are kept
outside the repository. No MOX, TUNE or other transmit command was issued.

A board-local client and a Mac client both verified:

- Certificate SHA-256 pin and token authentication.
- Connected ANAN-G2 / Saturn capabilities, firmware 27, five supported slices,
  and `txPermitted=false`.
- A populated snapshot of 2,502 station settings and one receive slice.
- Receive tuning from 14.225 to 14.226 MHz and restoration to 14.225 MHz,
  confirmed by the station's saved-frequency updates.
- 160 receive-meter samples over about 15 seconds, with changing values.

The temporary probe initially expected an ordinary property delta after its
write. `StateMirror::applyInbound` deliberately suppresses that echo. Those
timeouts were a probe error, not evidence of a tuning failure. The corrected
probe observes `settings.value` for the saved frequency and restoration.

The Mac GUI connected in isolated profile `radxa_5c_r2`, displayed the Saturn
and live VFO receive level, and opened Settings. Local-radio connection and
management actions were visibly disabled. Spectrum and waterfall are blank
because R3 media transport is not implemented. The separate analog S-meter
remained at its local idle reading while the VFO meter was live; do not claim
all meter widgets have remote coverage. Client logs also report missing
apply hooks for several `TunerModel` properties; accessory mirroring still
needs an acceptance review.

Repeated service stops completed with `Result=success`, exit code 0, and no
forced kill. Subsequent starts reconnected to the pinned Saturn. A later
steady-state sample used about 97 MiB and reported a 41.6 C SoC temperature.
A subsequent board reboot also passed: the enabled service started on its
own, and the already-open Mac GUI reconnected automatically about 33 seconds
after shutdown. The OEM kernel-managed fan was present after boot.
Multi-slice throughput and a longer soak remain unverified.

The installed Core was subsequently upgraded to signed combined checkpoint
`14124e7c`. Native ARM build, staged shared-library installation and a fresh
authenticated receive-control probe all passed. The service remains enabled
and active with zero restarts; the probe verified Saturn identity, 2,502
settings, tuning/restoration, and 161 changing receive-meter samples.
The previous binary/library/unit set is retained in a root-only rollback
directory. See the [current verification ledger](../architecture/2026-09-20-remote-daemon-r3-verification/README.md).
The new baseline still has control-only R2 behavior. No new reboot or live
remote media result is implied by this installation.

### Network observation, not a proven fix

An early streaming session caused severe LAN latency and packet loss. With
Core stopped, Mac-to-board tests measured about 7-8 Mbit/s. Those tests included
the Mac's Wi-Fi hop; they do not establish a wired-link speed limit. Traffic
captures confirmed that P2 radio I/Q used the board's `end1` Ethernet interface.
Its simultaneous `wlan0` connection carried a normal 1 Hz discovery beacon.

The connection recovered during interface diagnostics and service restarts.
A controlled comparison then stayed healthy with both the original and lower
transmit-interrupt batching settings, so no specific cause or workaround has
been established. Ethernet checksum offload, EEE and interrupt batching were
restored to the original settings; Wi-Fi is also restored. No persistent
network tuning was installed.

Read-only inspection of the maintainer's MikroTik hEX at `192.168.109.85`
confirmed the Rock 5C on `ether2` and the Saturn reached through `ether1`.
Both links were gigabit full duplex with hardware switching and no configured
bandwidth limit or simple/tree queue. A five-second live-stream sample showed
no new FCS errors, collisions or dropped packets on either port. No switch
configuration was changed. The recovered connection had roughly 0.3 ms
board-to-router and 3-6 ms Mac-to-board latency while receiving. A later
8 MiB SSH download during live reception measured 45 Mbit/s, including SSH
connection setup and the Mac Wi-Fi hop. The limited earlier result was not
a persistent link limit.

## Next milestones

1. Follow the [R3 implementation plan](../architecture/2026-09-20-remote-daemon-r3-plan.md).
   One mixed stereo Opus feed is confirmed; original network decisions are
   preserved. Claude's original documents and execution history are mapped in
   the [objective review](../architecture/2026-09-20-remote-daemon-r3-review.md).
2. Deliver one live Saturn pan and advancing 2D/3D waterfall through encrypted
   media, then actual mixed stereo playback with the slice controls working.
3. Complete multi-pan/tier capacity, remaining meter/accessory acceptance,
   bandwidth, audio-clock and two-hour hardware-soak gates. Install and verify
   reconnect with media on both ends. R2 plus R3 remains one receive release.
4. Prepare the carrier-network bench alongside R3. The review recommends
   receive-only R5 NAT traversal after working LAN reception, without waiting
   for the full R4 TX implementation. Preserve both relay paths, including
   TLS/443, which the current libjuice backend does not provide.
5. Keep the integrated signed branch ready for review. No public push or PR
   update has been performed; the maintainer's posting preference still applies.

## Sources to resume from

- [Current R3 plan and network continuity](../architecture/2026-09-20-remote-daemon-r3-plan.md)
- [Recovered Claude planning chain and review](../architecture/2026-09-20-remote-daemon-r3-review.md)
- [Current integration and installation evidence](../architecture/2026-09-20-remote-daemon-r3-verification/README.md)
- [Rock 5C live control and reboot evidence](../architecture/2026-08-03-remote-daemon-r2-verification/rock-5c-2026-09-20/README.md)

- [R2 implementation plan](../architecture/2026-08-03-remote-daemon-r2-plan.md)
- [R2 acceptance procedure and ledger](../architecture/2026-08-03-remote-daemon-r2-verification/README.md)
- [R2/R3 design addendum](../architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md)
- [Umbrella architecture](../architecture/2026-07-28-remote-daemon-architecture-design.md)
- [Pi R1 bench evidence](../architecture/2026-08-02-remote-daemon-r1-verification/README.md)
- [PR #315 review](https://github.com/boydsoftprez/NereusSDR/pull/315/files)

Historical conversation: Claude session `cce211f4-844a-4816-8f4e-893075723aca`,
August 3-9, 2026. The final handoff and bench discussion are in archived
lines 2480-2551 under the `peaceful-ramanujan-64ff46` conversation archive.
