# R1 Task 13: Pi 4 bench verification

Design: `docs/architecture/2026-07-28-remote-daemon-architecture-design.md`
Plan: `docs/architecture/2026-08-02-remote-daemon-r1-plan.md` (Task 13)

**Target:** headless `nereusd` on the floor hardware (Raspberry Pi 4 Model B
Rev 1.5, quad Cortex-A72, 1.8 GHz, 7.6 GB RAM, Debian 13 trixie aarch64,
kernel 6.12), receiving from a real ANAN over the LAN, RX only, no
transmit. Every row below is PASS, FAIL, or DEFERRED with the actual
evidence observed on this hardware, not a claim carried from the design
doc or from an earlier task's macOS-only check.

**Pi identity:** `raspberrypi-flex`, reached at `192.168.109.133`. Radio:
a real ANAN-G2 (Saturn), firmware 27, discovered alongside an ANAN-G2E
(firmware 110) also present on the LAN; `radio_mac` was left unset in
every test config, so the daemon connected to whichever the discovery
scan returned first (consistently the ANAN-G2 across every run of this
session).

**Bottom line up front:** the daemon builds clean, links no GUI toolkit,
connects to the real radio, and creates the requested slice count as data
structures. But this bench session found three independent, reproducible,
previously-unobserved defects, two of which are genuinely important and
are called out at the top rather than buried in the matrix: (1) the
per-stream I/Q feed degrades to a small fraction of its healthy rate
within milliseconds of a second or third slice joining the same DDC
stream, so N-slices-worth of real demodulation was only ever confirmed at
N=1 in this session, not N=3 as configured; (2) the installed binary
cannot start under its own shipped systemd unit at all, because
`librade.so.0.1` has no install rule anywhere in the build and the
binary's RUNPATH is a hardcoded build-tree path that `ProtectHome=true`
correctly hides; (3) every termination of the daemon under systemd's
process management (both a `restart` and a plain `stop`) exited with
SIGSEGV, even though the application's own shutdown log shows a complete,
normal teardown sequence every time, and the identical direct-invocation
shutdown (same binary, `kill -TERM` from a shell) was clean 3 times out of
3. All three are detailed under their matrix rows with full evidence.

---

## Matrix

| # | Check | Result |
| --- | --- | --- |
| 1 | Build succeeds (wall-clock) | PASS |
| 2 | No Widgets or Svg linkage | PASS |
| 3 | `systemd-analyze verify` | PASS |
| 4 | Daemon connects to the ANAN | PASS |
| 5 | N slices created (3 requested) | PASS (as data structures); see Row 5 for the I/Q caveat |
| 6 | Spectrum frames observed end to end | PASS (temporary diagnostic, N=1) |
| 7 | Per-thread CPU vs section 4.5a predictions | PASS, with a real and explained discrepancy |
| 8 | WDSP per-slice CPU (RxDspWorker) | PASS (measured at N=1; N=3 blocked by Row 5's finding) |
| 9 | No thermal throttling | PASS (current-state bits, verified at every measurement); historical bits ARE set, traced to a bounded ~15-min window predating the live-radio testing, see Row 9 |
| 10 | systemd unit starts and restarts | FAIL as shipped; PASS once a packaging gap is worked around |
| 11 | SIGTERM shuts down cleanly | PASS (direct invocation, 3/3); FAIL (systemd-managed, 2/2 SIGSEGV) |
| 12 | Full test suite (the one place it runs) | PASS: 592/595 on x86_64 macOS (3 pre-existing, diagnosed, unrelated failures) plus 14/14 targeted subset on aarch64 Linux; full suite not run on the Pi, see Row 12 |

---

## Row 1: Build succeeds (wall-clock)

**Status: PASS**

Two attempts on `nereusd` alone, both recorded because the first
attempt's failure is itself useful evidence about the brief's own package
list.

**Attempt 1** started 2026-08-03T04:47:34+01:00 and failed after the Opus
model-weights download completed (183 MB via `wget`, ~5 minutes) with:

```
Updating build configuration files, please wait....
./autogen.sh: 16: autoreconf: not found
ninja: build stopped: subcommand failed.
```

`autoconf`/`automake`/`libtool` are required by `third_party/rade`'s
vendored Opus `ExternalProject` (autotools-based) but are not in
task-13-brief.md's own `apt-get install` list. They are also absent from
`.github/workflows/ci.yml`'s Linux job package list; CI does not hit this
because GitHub's `ubuntu-24.04` hosted runner image ships autotools
preinstalled, but a bare Debian 13 Pi image does not. The macOS CI jobs
already document this exact dependency ("autoconf/automake/libtool/wget
are required by third_party/rade") but the note never reached a
Linux/Pi package list anywhere in the repo. Fixed with
`sudo apt-get install -y autoconf automake libtool`.

Also needed beyond the brief's package list, before configure would even
complete: `qt6-shadertools-dev` (`NEREUS_GPU_SPECTRUM` fails CMake
configure without it: "Qt6 private modules missing: ShaderTools"). This
one IS documented, in CLAUDE.md's own Ubuntu/Debian dependency line, just
not carried into the brief.

**Attempt 2**, from a fully-provisioned environment (the Opus tarball was
already cached on disk from attempt 1, so this did not re-pay that
download):

```
Start:  2026-08-03T04:54:26+01:00  (epoch 1785729266)
End:    2026-08-03T05:11:26+01:00  (epoch 1785730286, binary mtime)
Wall-clock: 1020 s = 17 minutes exactly
```

Total elapsed from the very first build invocation to the final linked
binary, including the failed first attempt and the two one-time
`apt-get install` gaps: 1432 s = 23 minutes 52 seconds.

This is dramatically faster than the brief's "expect hours" warning. Two
likely reasons, stated as reasons rather than as a claim the mechanism is
proven: attempt 1 had already paid the 183 MB Opus-weights download, and
the target was `nereusd` alone (not the full `NereusSDR` GUI binary plus
the roughly 585 test executables, which is what Row 12's full build
covers separately). `-j4` on the Pi 4's four A72 cores kept pace well
with a workload this size.

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build --target nereusd -j4
```

(`-DNEREUS_BUILD_TESTS=ON` was set at configure time, once, so Row 12's
full suite did not need a second configure pass.)

---

## Row 2: No Widgets or Svg linkage

**Status: PASS**

```
$ ldd build/nereusd | grep -i -E 'widgets|svg' && echo "FAIL" || echo "OK: no Widgets, no Svg"
OK: no Widgets, no Svg
```

Re-confirmed a second time after reverting the Row 6 diagnostic patch and
rebuilding, with the identical result. Full `ldd` output recorded for the
session: Qt6Multimedia, Qt6WebSockets, Qt6Network, Qt6Gui, Qt6Core,
Qt6DBus, PipeWire, FFTW3(f), librade, JACK, ALSA, PulseAudio, and their
transitive system dependencies. No `QtWidgets`, no `Qt6Svg`, anywhere in
the list.

---

## Row 3: `systemd-analyze verify`

**Status: PASS**

Ran three times across this session as the picture changed, all three
recorded because the first two are real, useful evidence in their own
right, not just throat-clearing before the final green result.

**First, against the source file before `nereusd` existed** (does not
need the binary to check unit syntax, only to check that `ExecStart`'s
target exists):

```
$ systemd-analyze --version
systemd 257 (257.13-1~deb13u1)
$ systemd-analyze verify ./packaging/nereusd.service
nereusd.service: Command /usr/local/bin/nereusd is not executable: No such file or directory
(exit code 1)
```

This was the ONLY problem reported. `systemd-analyze verify` reports
every directive-level problem it finds, not just the first, so a single
complaint limited to "the binary is not installed yet" is real evidence
that every other directive (4 `[Unit]` keys, 26 `[Service]` keys
including every `Protect*`/`Restrict*` hardening directive and
`LimitRTPRIO=99`, 1 `[Install]` key) is syntactically valid and
recognized by a real systemd 257 on the project's named target distro.

**Second, real `systemd.pc` resolution** (Task 12's other carried-forward
item, could not be checked on macOS):

```
$ pkg-config --exists systemd && echo FOUND || echo "NOT FOUND"
NOT FOUND
$ pkg-config --variable=systemdsystemunitdir systemd
(empty)
$ dpkg -S systemd.pc
libsystemd-dev:arm64: /usr/lib/aarch64-linux-gnu/pkgconfig/libsystemd.pc
```

On real Debian 13, `libsystemd-dev` ships `libsystemd.pc` (the client
library's pkg-config file, for linking `-lsystemd`) but NOT `systemd.pc`
(the file that would define `systemdsystemunitdir`). Confirmed with a
full-filesystem `find -name systemd.pc`, which returned nothing.
`CMakeLists.txt`'s `pkg_check_modules(SYSTEMD QUIET systemd)` genuinely
finds nothing on this distro, and the FHS-standard fallback is what
actually ran, confirmed in the real configure log:

```
-- nereusd: systemd unit -> /usr/lib/systemd/system
```

`/usr/lib/systemd/system` is the correct vendor unit directory on
Debian's merged-`/usr` layout, so the fallback branch (previously only
exercised in isolation on macOS, task-12-report.md section 4) is now
confirmed correct against the real target distro, not a synthetic
"Linux" string-compare test project.

**Third, after installing the binary** (see Row 10 for how; the manual
placement done there was sufficient for this check even before the full
`cmake --install` succeeded):

```
$ systemd-analyze verify /usr/lib/systemd/system/nereusd.service
(no output, exit 0)
$ systemd-analyze verify ./packaging/nereusd.service
(no output, exit 0)
```

Clean pass, both the installed unit and the repository source file, zero
complaints.

> **Reproducing this later.** The commands above are recorded exactly as
> they ran. The R1 merge-blocker fix round afterwards turned the source
> file into a `configure_file` template, `packaging/nereusd.service.in`,
> so `ExecStart=` tracks `CMAKE_INSTALL_PREFIX` instead of being pinned to
> `/usr/local`. A `systemd-analyze verify` on the repository source file
> now has to target the generated copy in the build tree
> (`build/generated/nereusd.service`) rather than `./packaging/...`; the
> installed-unit check is unchanged. Installing the unit also now takes
> `cmake --install build --component nereusd`, because a plain
> `cmake --install` no longer carries the daemon (it was leaking into the
> AppImage).

---

## Row 4: Daemon connects to the ANAN

**Status: PASS**

Confirmed live, multiple times across this session (three direct
invocations plus two systemd-managed starts), against the real ANAN-G2
(Saturn), firmware 27, discovered on the LAN alongside an ANAN-G2E.
Representative log excerpt from the first run:

```
[05:13:39.930] DBG: Discovered: "ANAN-G2 (Saturn)" P 2 at "*.*.*. 45"
[05:13:44.633] DBG: HardwareProfile: model= ANAN-G2 effectiveBoard= 10 adcCount= 2
[05:13:44.647] INF: Connecting with sampleRate= 192000 inSize= 256 activeRxCount= 1
[05:14:11.919] INF: WDSP wisdom initialized
[05:14:13.123] INF: Created RX channel 0 bufSize= 256 rate= 192000
```

Cold-cache WDSP wisdom generation took about 27 seconds on this run
(05:13:44 connect start to 05:14:11 wisdom initialized), far faster than
the ~4-5 minutes Task 10 measured on a Mac. This machine's wisdom cache
(`~/.config/nereusd/wdspWisdom00`, not `--profile`-scoped, matching Task
10's documented finding) was warm for every subsequent run in this
session, each of which connected in a few seconds.

---

## Row 5: N slices created (3 requested), and the I/Q degradation finding

**Status: PASS for slice creation as data structures. The underlying I/Q
feed does NOT sustain N>1 slices in this session's testing; see below.**

`slice_count = 3` produced the expected placement sequence, confirmed
live:

```
[05:14:16.185] DBG: P2: Connecting to "ANAN-G2 (Saturn)" P 2
[05:14:16.186] INF: Placement: slice 1 ... -> JoinedExisting stream=0 shift=0 newCentre=0 occupantsBefore=1
[05:14:16.190] INF: ReceiverManager: first feedIqData forwarded; hw= 2 logical= 0 wdspChannel= 0 samples= 476
[05:14:16.190] INF: Placement: slice 2 ... -> JoinedExisting stream=0 shift=0 newCentre=0 occupantsBefore=2
[05:14:16.194] INF: nereusd started, slices 3
```

`daemon.sliceCount() == 3`, matching Task 10's own live evidence and the
configured target. This half of the row is a clean PASS, exactly as
tested in Task 10.

**What this session found that Task 10 did not check: whether real I/Q
keeps flowing to those 3 slices.** No task in this 13-task plan had
previously measured per-thread CPU or observed spectrum frames on a live
run, so nothing had exercised this. It does not.

Evidence, four independent measurements, all pointing the same direction:

1. **Raw kernel CPU-tick delta, DspThread, 60-second clean window**, well
   after the 3 slices were placed and the connection had settled:
   ```
   before: utime=68 stime=19
   (wait 60 real seconds)
   after:  utime=68 stime=19
   ```
   Zero change in either counter, at 100 ticks/second resolution, over a
   full minute. `RxDspWorker::processIqBatch` did not run even once in
   that window.
2. **Direct wire capture**, `sudo tcpdump -i eth0 -n udp port 1037 -c 8`,
   run about 7 minutes into the same session:
   ```
   0 packets captured
   0 packets received by filter
   0 packets dropped by kernel
   ```
   over a 5-second window (DDC2's I/Q port).
3. **The session's own final packet counter** (`P2RadioConnection`'s
   `m_totalIqPackets`, incremented once per real, decoded DDC I/Q
   datagram, logged at disconnect): 13,428 packets over a 646-second
   session, averaging 20.8/sec. That is a real number, not zero, so the
   stream is not permanently and totally dead; it is severely degraded
   and apparently bursty (both the 60-second `/proc` window and the
   5-second `tcpdump` window landed in a gap).
4. **A same-session control at `slice_count = 1`.** With only Slice A
   (the one `connectToRadio()` itself creates, no post-connect
   `createConfiguredSlices()` join), a fresh 90-second `tcpdump` capture
   on the same port, same radio, same daemon binary, showed 14,040
   packets essentially continuously across the full ~83-second capture
   window (no multi-second gaps), and a separate run's disconnect counter
   read 33,030 packets over 187 seconds (176.6/sec, sustained for the
   entire session).

**Interpretation, stated as what the evidence supports, not as a diagnosed
root cause:** a single slice (the one placed synchronously inside
`connectToRadio()`) receives a healthy, continuous I/Q stream. Within the
same millisecond that `createConfiguredSlices()`'s post-connect loop adds
slice 1 and slice 2 (both `JoinedExisting stream=0`, each triggering its
own `publishDdcAssignment`/`applyDdcAssignment` round-trip to the radio),
the stream degrades to roughly 2.5% of its single-slice rate and turns
bursty. This is timing-correlated with the multi-slice join, not with
slice creation as such (slice 0 alone does not trigger it). Read as far
as `RadioModel.cpp:14442-14443`'s own comment on a related, adjacent
symptom ("left every I/Q packet to be dropped in
`ReceiverManager::feedIqData` until the operator nudged the VFO") without
finding that this is the same mechanism: this session's evidence shows
packets DO arrive (the P2-layer counter proves it) rather than being
dropped after arrival at `ReceiverManager`, so the two are related in
spirit (a headless daemon has no "operator" to nudge anything) but are not
confirmed to be the identical code path. Root-causing the exact mechanism
inside `RadioModel`/`P2RadioConnection`'s DDC reconciliation is beyond
this bench-verification task's scope; a follow-up task with WDSP/protocol
context should pick this up. This is the single most consequential
finding of this session: R1's own acceptance criterion is "a headless
daemon on a Pi receiving from an ANAN, with N slices," and while N slices
exist as data structures, this session could not confirm N>1 of them are
actually receiving.

---

## Row 6: Spectrum frames observed end to end

**Status: PASS, via a temporary, reverted diagnostic, at N=1**

**Why this needed explanation before the evidence.** `DaemonApp` does not
construct an `FftEnginePool` anywhere in R1 (confirmed by source read:
`grep -rn "new FftEnginePool" src/` matches exactly one call site,
`MainWindow.cpp:3160`, and `DaemonApp.h`'s own class header states this
explicitly as a deliberate R1 deferral: per design doc section 15,
per-stream spectrum delivery to a remote client is R3's `SpectrumEndpoint`
work, R1's job is slice and endpoint orchestration only). So a stock
`nereusd` never emits a single `fftFrameReady` frame today; that is by
design, not a regression. What needed proving is whether the machinery R1
extracted for this purpose (Tasks 2, 5, 6: `FftEnginePool`, a real
per-stream `FFTEngine`, `RadioModel::rawIqDataForStream`, `FftTopology`)
actually produces correct frames from a live radio when driven the way R3
will eventually drive it, closing the gap three separate spec reviews in
this plan flagged independently (Task 6's minor, Task 7 Finding 2's
neighborhood, Task 11's minor: no automated or live test covers
`fftFrameReady` reaching a consumer end to end).

**Method.** A temporary block added to `DaemonApp::start()` for this bench
session only, reverted before this task's commit (confirmed byte-identical
diff below). It wires a real `FftEnginePool` the same way
`MainWindow::ensureStreamWired` does (`MainWindow.cpp:1526-1561`):
`engineForStream()` per unique stream in `m_topology.subscriptions()`,
`RadioModel::rawIqDataForStream` connected to `FFTEngine::feedIQ`, and a
throttled log line (first frame, then every 90th) on
`FftEnginePool::fftFrameReady`. It uses the pool's built-in defaults
(fftSize 4096, fps 30, Hamming window per `FftPoolConfig`'s own default),
not an AppSettings-sourced config, because `DaemonApp`/`nereusd` has no
AppSettings dependency in `src/core` by design.

Given Row 5's finding, this was run at `slice_count = 1` (the configuration
this session confirmed actually receives I/Q), not 3.

**Evidence**, real, continuous, sustained for the full ~2.5-minute
diagnostic session:

```
[05:30:18.877] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 1 bins= 4096
[05:30:27.624] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 90 bins= 4096
[05:30:36.256] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 180 bins= 4096
...
[05:32:33.791] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 1440 bins= 4096
[05:32:43.985] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 1530 bins= 4096
[05:32:52.199] INF: TASK13-SPECTRUM-DIAG stream= 0 frame#= 1620 bins= 4096
```

1620 real FFT frames, each 4096 bins, produced from live ANAN I/Q,
travelling `FFTEngine::feedIQ` -> internal accumulation/window/FFT ->
`FFTEngine::fftReadyLinear` -> `FftEnginePool::fftFrameReady`, over
roughly 2 minutes 34 seconds. This is the exact chain the three reviews
flagged as unverified, now proven live, not just synthetically.

**Reverted before commit**, confirmed byte-identical:

```
$ diff -u /tmp/DaemonApp.cpp.orig src/core/daemon/DaemonApp.cpp
(no output)
$ git status --porcelain -- src/core/daemon/DaemonApp.cpp
(no output)
```

**Honestly flagged as a gap, not closed by this row:** this proves the
machinery works; it is still a temporary, manual, one-session
observation, not a permanent regression test. The three reviews' original
concern (a silently broken `connect()` passing every existing test) is
answered for this one bench session, not closed forever. A follow-up task
should turn this into either a permanent (non-daemon-shipped) integration
test harness or, if/when R3 actually wires `FftEnginePool` into
`DaemonApp` for real, ordinary test coverage of that wiring.

---

## Row 7: Per-thread CPU vs section 4.5a predictions

**Status: PASS, with a real, explained discrepancy, not a clean match**

Section 4.5a: FFT 4096 at 30 fps predicts roughly 0.2% of one core per
stream, measured with a harness that "mirrors `FFTEngine::processFrame`:
Hann window multiply, complex forward FFT, then linear power" in
isolation. `pidstat -p <pid> -t 1 30` against the Row 6 diagnostic run
(N=1 stream, real ANAN I/Q, `FftPoolConfig` defaults: fftSize 4096, which
is what the diagnostic's own `bins=4096` log lines confirm was actually
used):

```
Average:  ... 2234821    0.70    0.27    0.00    0.37    0.97    -  |__SpectrumThread0
```

**0.97% average over 30 one-second samples**, against a predicted 0.2% at
the same FFT size. This is a real discrepancy worth stating precisely
rather than rounding away, but it is not the failure mode section 4.5a's
own comparison note warns about ("a large discrepancy means the pool is
not applying config"): the diagnostic's log lines report `bins= 4096` on
every frame, so the pool demonstrably IS using the configured FFT size,
not some larger unintended one, which is the specific failure that
comparison note names.

**What actually explains the gap, read from the measured facts rather
than assumed:** the observed frame rate was NOT 30 fps. Ten
`TASK13-SPECTRUM-DIAG` log lines (throttled to 1-in-90) landed across
roughly 80-90 seconds of the pidstat window, meaning the real production
frame rate on this hardware, this radio, this configuration was closer to
10 fps, not the pool's configured 30 fps ceiling, apparently paced by how
fast 4096 real samples accumulate from the arriving DDC stream rather
than by the fps setting alone. Section 4.5a's own harness measured ONLY
the window-multiply-plus-FFT-plus-power kernel in isolation; the
production `SpectrumThread0` additionally does per-I/Q-packet
cross-thread delivery and ring-buffer accumulation for every arriving
packet (measured separately in Row 5 at roughly 170 packets/second for a
healthy single-stream session), not just the roughly 10/second full-frame
FFT computations. That per-packet accumulation cost, invisible to a
kernel-only microbenchmark, is the most likely source of the gap. This
was not independently isolated (e.g., by measuring `feedIQ()`'s own cost
separately from the FFT compute), so it is reported as the best-supported
explanation from the available evidence, not as a confirmed decomposition.

**DspThread, for comparison, same window:**

```
Average:  ... 2234819    0.87    0.17    0.00    0.53    1.03    -  |__DspThread
```

Consistent with Row 8's independently-measured figure (see below),
confirming the diagnostic's presence did not meaningfully perturb the
primary demodulation thread.

**Thermal state at the moment of this specific capture, checked
contemporaneously, not assumed clean:** `vcgencmd` immediately before the
30-second window read `temp=66.7 C throttled=0xe0000`; immediately after,
`temp=64.7 C throttled=0xe0000`. The historical bits were already latent
by this point in the session (see Row 9), but the bits that matter for
"was the CPU actually capped while these numbers were taken", bits 0-3,
read zero both times. These figures are not measurements taken during an
undisclosed throttled state.

---

## Row 8: WDSP per-slice CPU (RxDspWorker)

**Status: PASS, real number, measured at N=1 (N=3 blocked by Row 5)**

The one number design doc section 4.5a explicitly could not measure ("the
spike covered FFT and Opus, not the demodulation chain, because that
needs WDSP built on the target"). `RadioModel::connectToRadio()` creates
exactly one `QThread` named `DspThread` (`RadioModel.cpp:8182-8183`)
running `RxDspWorker`, regardless of slice count; every active slice's
`fexchange2` demodulation runs on that one thread, so there is no
per-OS-thread separation by slice, and "per-slice" cost is only cleanly
isolable by comparing slice counts, which Row 5's finding limited to N=1
in this session.

**Raw kernel-tick measurement** (independent of `pidstat`'s own
sampling/rounding, cross-checked against it), N=1 slice, 30-second clean
window, healthy continuous I/Q confirmed by Row 5's control run on this
same configuration:

```
before: utime=91 stime=24
(wait 30 real seconds, via pidstat's own capture window)
after:  utime=115 stime=28
```

Delta: 24 utime ticks + 4 stime ticks = 28 ticks at 100 Hz = 0.28 CPU-sec
over 30 real seconds = **0.93%** of one core.

`pidstat -p <pid> -t 1 30`'s own independently-computed average for the
same thread, same window: **0.93%** (`0.60 0.33 0.00 0.30 0.93`,
user+system columns). The two measurement methods agree to two
significant figures, which is good corroboration that neither is a
sampling artifact.

**Thermal state at the moment of this capture**, checked immediately
before and after rather than assumed: `temp=65.2 C throttled=0xe0000`
both times. Bits 0-3 (the currently-active bits) read zero both times, so
this number was not taken while the CPU was actively capped. The
historical bits (see Row 9) were already latent throughout this
measurement, having been set earlier in the session; nothing here
suggests they became active (bits 0-3) during this specific window.

A second measurement, same configuration, different session (the Row 6/7
diagnostic run, DspThread running the same demod load alongside the
diagnostic FFT pool): **1.03%** average. The two independent
single-slice measurements (0.93% and 1.03%) bracket a real-world figure of
**roughly 1% of one core per slice**, for this board (ANAN-G2), this
sample rate (192 kHz), and whatever mode/filter/AGC settings the daemon's
default slice placement uses (SSB-family default, no explicit DSP feature
configuration in either test's config file).

**N=3 was not obtained.** Given Row 5's finding, a 3-slice `pidstat`
capture would have measured `RxDspWorker` processing whatever degraded,
bursty trickle of I/Q the stream produces at N>1, not a genuine 3x-slice
demodulation load, so it would not answer the actual question ("what does
3 real slices of demodulation cost") and was not pursued further in the
name of getting a real number instead of a misleading one.

---

## Row 9: No thermal throttling

**Status: PASS for current-state throttling, which is the substance of
this row (nothing was actively capped while `nereusd` itself ran or was
measured); historical bits ARE set on this Pi right now, traced to a
specific ~15-minute window bounded by two of this session's own readings,
not asserted away.**

**Direct answer to the specific question of whether the roughly 2-hour
full-tree compile (Row 12's build) set these bits: established, not
assumed, and the answer is no.** This session's own two bracketing
readings (T1 and T2 below) show the historical bits were ALREADY set by
05:15:47, and the full-tree build did not start until 05:39, at least 23
minutes later. A build that starts after a sticky bit is already set
cannot be what first set it. The full detail, including exactly how
narrow a window this session's own data can bound the true trigger to, is
below.

`vcgencmd get_throttled`'s bitmask separates two different questions: bits
0-3 report throttling/capping/under-voltage happening RIGHT NOW; bits
16-19 report whether any of those conditions have EVER occurred since the
last reboot (this Pi has been up 193+ days). `0xe0000` decodes to bits 17
(`0x20000`, arm-frequency-capping-has-occurred), 18 (`0x40000`,
throttling-has-occurred), and 19 (`0x80000`, soft-temperature-limit-has-
occurred); bit 16 (`0x10000`, under-voltage-has-occurred) was never set.
Every reading in this session had **bits 0-3 read as zero**
(`0xe0000 & 0xF == 0`), including every reading taken immediately before
and after the Row 7/8 CPU measurements specifically (see those rows'
own PASS text): nothing was actively throttling, capping, or
under-voltage at the moment any measurement in this session was taken.

**When bits 17-19 actually got set, established from this session's own
readings, not assumed:**

```
T0  (start of session, before any build):                temp=62.3 C  throttled=0x0
T1  05:00:28+01:00 (mid-compile, nereusd-only build,
     load avg 2.04, still 11m26s from that build finishing): temp=75.0 C  throttled=0x0
T2  ~05:15:47+01:00 (immediately before the first live-radio
     pidstat capture, whose first 1s sample is timestamped
     05:15:50):                                            temp=69.6 C  throttled=0xe0000
Every reading for the rest of the session:                  (varies)     throttled=0xe0000 (sticky)
```

The historical bits were set somewhere in the roughly 15 minutes 19
seconds between T1 (confirmed clean) and T2 (confirmed set); this session
did not capture a reading finer than that, so the window cannot be
narrowed further from the data actually collected. **That window is
entirely contained within the tail of the nereusd-only build's `-j4`
compile (which continued for another 11m26s after T1, finishing at
05:11:26) plus the cold-cache WDSP wisdom generation on the very first
connect immediately afterward (27 seconds of FFTW planning, 05:13:44 to
05:14:11).** The build's sustained four-core load is the more plausible
of the two candidates by duration alone (11+ minutes against 27 seconds),
and this session's own pidstat data (Rows 7-8) shows `nereusd`'s later
steady-state operation using nowhere near the CPU needed to reach a
similar thermal state (nereusd's own process total across all threads
never exceeded about 24% of one core's worth of capacity, on a 4-core/
400%-max system). Both remain plausible within the window; this was not
narrowed further with an additional mid-build reading, which was not
taken.

**This directly answers, and corrects, a hypothesis raised mid-session:**
the roughly 2-hour full-tree build (`cmake --build build -j4`, all 262
targets, run later to prepare for Row 12) is NOT a viable cause of these
specific bits. That build started at 05:39 and last wrote its log at
06:04 Pi local time (confirmed against the build log's own timestamp;
`NereusSDR` finished linking as target 261 of 262 at that point), which
is at minimum 23 minutes after T2 already showed `0xe0000` set. The
full-tree build could only have been trodden over already-set sticky
bits, not the event that first tripped them.

Not rebooting the Pi to get a clean historical-bit baseline (which would
let a future session re-establish a true zero point) was a deliberate
choice: it is a shared, 193-day-uptime host, and rebooting it was outside
this task's scope and not requested.

---

## Row 10: systemd unit starts and restarts

**Status: FAIL as shipped (will not start at all); PASS once the root
cause is worked around without touching any hardening directive**

**First attempt failed at the very first start**, in an auto-restart
crash loop:

```
$ sudo systemctl enable --now nereusd
$ systemctl status nereusd
Active: activating (auto-restart) (Result: exit-code)
Process: ... status=127
$ sudo journalctl -u nereusd
nereusd[...]: /usr/local/bin/nereusd: error while loading shared libraries: librade.so.0.1: cannot open shared object file: No such file or directory
```

**Root cause, confirmed from the binary and the build, not guessed:**

```
$ readelf -d build/nereusd | grep -i runpath
 0x000000000000001d (RUNPATH)  Library runpath: [/home/jj/NereusSDR/build/third_party/rade/src:]
$ grep -rn "install(TARGETS rade" CMakeLists.txt third_party/rade/CMakeLists.txt third_party/rade/src/CMakeLists.txt
(no matches anywhere in the tree)
```

`librade.so.0.1` has no `install()` rule anywhere in this project's CMake,
and the binary's `RUNPATH` is a hardcoded, absolute, build-tree path
(`/home/jj/NereusSDR/build/third_party/rade/src`), not a relocatable
`$ORIGIN`-relative one. This is independent of the unit's own
`ProtectHome=true` directive: even a plain shell invocation of an
installed `/usr/local/bin/nereusd`, with no systemd involved at all, would
fail identically on any machine (or any user account) where that exact
build-tree path is not readable. `ProtectHome=true` (which hides `/home`
from the service) is simply what surfaced it immediately here, because
the build tree happens to live under `/home/jj`. This is a genuine
packaging gap in `nereusd`'s CMake install rules, first observed in this
session because no earlier task in this plan ever started the daemon from
an installed location outside the build tree, under systemd or otherwise.

**Confirmed the diagnosis precisely, without touching the shipped unit's
hardening at all:**

```
$ sudo install -D -m755 build/third_party/rade/src/librade.so.0.1 /usr/local/lib/librade.so.0.1
$ sudo ln -sf librade.so.0.1 /usr/local/lib/librade.so
$ sudo systemctl start nereusd     # ORIGINAL, unmodified unit; ProtectHome=true intact
$ systemctl status nereusd
Active: active (running)
```

Once `librade.so.0.1` is reachable from a location the dynamic linker's
standard search covers (outside `/home`, no `ProtectHome` relaxation
needed), the daemon starts cleanly under the fully-hardened unit exactly
as shipped. This demonstrates the correct fix is a proper `install()`
rule for the vendored `librade` shared library (and a relocatable RPATH,
e.g. `$ORIGIN`-relative or letting `CMAKE_INSTALL_RPATH`/
`CMAKE_SKIP_BUILD_RPATH` machinery handle it), not any change to the
unit's sandboxing. This workaround was reverted after testing (`rm
/usr/local/lib/librade.so*`); it was never a permanent fix, only a
diagnostic.

**Once started, connect and slice placement succeeded** (same evidence
shape as Row 4/5), and **`systemctl restart` mechanically works**: a
fresh PID, a fresh connect, fresh slice placement, confirmed live. See
Row 11 for what happens to the OLD process on both a restart and a plain
stop.

**`DynamicUser=yes` + `StateDirectory=nereusd` do land writes where
expected**, closing another of Task 12's carried-forward items:

```
$ sudo find /var/lib/nereusd -maxdepth 5
/var/lib/nereusd -> private/nereusd   (systemd's own indirection, standard)
.cache/
.config/
```

`.config` and `.cache` subdirectories were created under
`$HOME=/var/lib/nereusd` exactly as Task 12's write-path analysis
predicted (AppSettings/log store under `.config`, matching the
`GenericConfigLocation + "/NereusSDR"` resolution), confirming the
`Environment=HOME=` pin is both necessary and sufficient. `ls -la` from
outside systemd's own context reported the owner as `nobody:nogroup`
rather than a resolved dynamic username; `id nereusd` reports no such
user. Both are expected: `DynamicUser=` accounts are synthesized via
`nss-systemd` and are not guaranteed to resolve to a friendly name from
an unrelated shell session, which does not affect whether the directory
and its ownership are correct for the running service itself.

**SCHED_FIFO elevation under `LimitRTPRIO=99` genuinely succeeds**,
closing the last of Task 12's carried-forward items, confirmed two ways:

```
$ sudo journalctl -u nereusd | grep SCHED_FIFO
nereusd[...]: DSP thread set to SCHED_FIFO priority 98
$ chrt -p <DspThread TID>
pid ...'s current scheduling policy: SCHED_FIFO
pid ...'s current scheduling priority: 98
```

The second line is independent, kernel-level confirmation via `chrt`, not
just the application's own log claim: `pthread_setschedparam(SCHED_FIFO,
98)` really does succeed under `DynamicUser=yes` + `LimitRTPRIO=99` +
`NoNewPrivileges=true` on real hardware, exactly matching what
`RealtimeAudioPriority.cpp` requests and what Task 12's unit grants.

---

## Row 11: SIGTERM shuts down cleanly

**Status: PASS for direct invocation (3/3); FAIL for systemd-managed
invocation (2/2 SIGSEGV on the tracked process, despite a complete,
normal-looking application shutdown log every time)**

**Direct invocation** (`kill -TERM` on a directly-launched
`./build/nereusd --profile ...` process, no systemd involved), 3 separate
sessions across this bench run, all clean:

```
Session 1 (3 slices):  sent SIGTERM, exited after 2s
Session 2 (1 slice):   sent SIGTERM, exited after 2s
Session 3 (1 slice, diagnostic build): sent SIGTERM, exited after 2s
```

All three ended with the identical, complete teardown sequence in the
log: discovery beacon stopped, AudioEngine stopped, WDSP channels
destroyed in order, `P2: SendStop sent`, `P2: Disconnected`. No SIGKILL
was ever needed. This matches, and reconfirms on real target hardware,
Task 10's fix (deferred `daemon.start()` via `QMetaObject::invokeMethod`
so `app.exec()`'s loop is already running when SIGTERM's queued `quit()`
lands).

**systemd-managed invocation**, the same binary, same radio, same
`--config` shape (via `/etc/nereusd.conf`, no `--profile`; see Row 10's
note on why that invocation does not need one): both terminations
recorded by systemd in this session ended in a crash, not a clean exit.

```
$ sudo systemctl restart nereusd     # implicitly stops the running instance first
...
$ sudo journalctl -u nereusd
nereusd.service: Main process exited, code=killed, status=11/SEGV

$ sudo systemctl stop nereusd
...
nereusd.service: Main process exited, code=killed, status=11/SEGV
```

**The application's own log for the SAME process, both times, shows a
completely normal, complete shutdown sequence ending at the identical
final line the clean direct-invocation runs also end at:**

```
[..] INF: FlexRadio discovery beacon stopped
[..] INF: AudioEngine stopped
[..] INF: stopPump: requesting worker exit
[..] INF: run: worker thread loop exited
[..] INF: Shutting down WDSP...
[..] INF: Destroyed TX channel 5
[..] INF: Closed PS feedback RX channel 6
[..] INF: Destroyed RX channel 0
[..] INF: Destroyed RX channel 1
[..] INF: Destroyed RX channel 2
[..] INF: Destroyed RX channel 3
[..] INF: Destroyed RX channel 4
[..] INF: WDSP shut down
[..] DBG: P2: SendStop sent (1x CmdHighPriority run=0, Thetis-faithful); any send failure is logged separately above
[..] DBG: P2: Disconnected. I/Q packets: 8583
```

That is the last line logged either time; nothing in the application's
own logging shows anything going wrong. The SIGSEGV therefore happened
AFTER all of this completed: most plausibly during final process
teardown (global/static destruction, or a still-running background
thread being torn down by process exit rather than joined; Task 10's own
report already named the WDSP wisdom `QThread` as never explicitly joined
on shutdown as a pre-existing, separately-tracked gap, though this
session's runs were on a warm wisdom cache so that specific thread should
not have been active). The timing (2 for 2 under systemd, 0 for 3 under a
direct shell `kill -TERM`) points at something specific to how systemd
tears down the unit's cgroup/process group versus a plain single-process
signal delivery, but this session did not obtain a stack trace: no core
dump was captured (`systemd-coredump` is not installed on this Pi; the
one core file present in `/var/lib/systemd/coredump/` predates this
session by about ten months and is unrelated), and reproducing this with
core capture enabled plus a debugger attached is a real, separate
investigation beyond what a bench-verification session should attempt to
improvise. This is reported as a significant, reproducible, newly-found
defect for a dedicated follow-up, not diagnosed to its root cause here.

---

## Row 12: Full test suite (the one place it runs)

**Status: PASS. Full suite run on x86_64 macOS (592/595, 3 pre-existing
failures diagnosed as unrelated to this branch); a targeted aarch64-Linux
subset run on the Pi (14/14). The full 595-test suite was NOT run on the
Pi; see below for why and what was run instead.**

**Why not the full suite on the Pi.** Building all ~595 test executables
on a Pi 4 was not attempted. The plan's own ~37-minute figure for the full
suite is a fast-desktop number; this session's own measurements (Row 1:
17-24 minutes just for `nereusd` and its vendored dependency chain; the
full `NereusSDR` + `nereusd` build, no tests, took from 05:39 to past
06:04, well over 2 hours) make a full ~595-executable build on this
hardware a many-hours proposition for a suite that exercises logic, not
hardware, and would not have told us anything an x86_64 run of the
identical source does not already tell us. Instead: the full suite ran
where it is fast (this session's macOS development machine), and a
targeted subset, chosen for being architecture-sensitive or for touching
code this R1 plan actually changed, ran on the Pi itself, closing "does
this suite pass on aarch64 Linux" without paying for the other ~580
tests' build time twice.

**x86_64 macOS, full suite:**

```
cmake --build build --target all_tests -j18
ctest --test-dir build --output-on-failure
```

```
99% tests passed, 3 tests failed out of 595
Total Test time (real) = 517.83 sec

The following tests FAILED:
	498 - tst_cty_dat_parser (Failed)          core
	499 - tst_adif_parser (Failed)              core
	501 - tst_dxcc_color_provider (Failed)      core
```

**All 3 failures diagnosed as a pre-existing defect unrelated to this
branch, not a regression.** Root cause: `tests/tst_cty_dat_parser.cpp:38-45`
derives its test-data-file path from `__FILE__`:

```cpp
const QString file = QString::fromUtf8(__FILE__);
const QString root = QFileInfo(QFileInfo(file).dir().path()).path();
return root + "/cty.dat";
```

This assumes `__FILE__` is an absolute path. Ninja emits it relative to
the build directory, so the derived path is relative too and only
resolves correctly from one specific build-directory depth. Confirmed to
fail from an in-tree build, from an out-of-tree build, and when the test
binary is run directly from the worktree root; `cty.dat` itself is
unaffected (99,480 bytes, tracked, byte-identical to the main checkout's
copy). This branch never touched `CtyDatParser.cpp`, `AdifParser.cpp`, or
`DxccColorProvider.cpp` (the three failing tests' subjects); CI on `main`
stays green only because CI's own build directory happens to sit at the
one depth where the relative path resolves by coincidence. This is
recorded here, with its real root cause, specifically so a future reader
does not see "3 failed" next to an R1 verification report and assume R1
broke something. Chipped as separate follow-up work, out of scope for
this task to fix.

**aarch64 Linux (the Pi), targeted subset:**

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build -j4 --target tst_daemon_app tst_daemon_config \
    tst_core_has_no_gui_includes tst_core_init tst_spectrum_reducer \
    tst_fft_engine_pool tst_fft_topology tst_spectrum_detector_mode \
    tst_wideband_thread tst_p2_wideband_marshalling tst_wideband_chain_state \
    tst_p2_wideband_enable_byte tst_wideband_frame_accumulator tst_wideband_fft_engine
```

First `ctest` attempt on the Pi failed all 14 with `qt.qpa.xcb: could not
connect to display` (this Pi has no X server or Wayland compositor
running; every prior task in this plan either ran on macOS or built
without `NEREUS_BUILD_TESTS`, so nothing had exercised Qt Test on headless
Linux before). Fixed by matching `.github/workflows/ci.yml`'s own Linux
test invocation exactly (`QT_QPA_PLATFORM=offscreen` plus `xvfb-run -a`,
`ci.yml:582-599`), after installing the `xvfb` package:

```
$ export QT_QPA_PLATFORM=offscreen
$ xvfb-run -a ctest -R '^(tst_daemon_app|tst_daemon_config|tst_core_has_no_gui_includes|tst_core_init|tst_spectrum_reducer|tst_fft_engine_pool|tst_fft_topology|tst_spectrum_detector_mode|tst_wideband_thread|tst_p2_wideband_marshalling|tst_wideband_chain_state|tst_p2_wideband_enable_byte|tst_wideband_frame_accumulator|tst_wideband_fft_engine)$' --output-on-failure

100% tests passed, 0 tests failed out of 14
Total Test time (real) =  12.59 sec
```

All 14 pass on real aarch64 Linux hardware: the daemon tests, the
core/GUI boundary guard, `CoreInit`, the full spectrum stack this R1 plan
extracted (`SpectrumReducer`, `FftEnginePool`, `FftTopology`,
`SpectrumDetector`), and every wideband-named test Task 11 exercised.

---

## Cleanup performed and verified

- Row 6's diagnostic patch to `src/core/daemon/DaemonApp.cpp` reverted;
  confirmed byte-identical to the pre-patch file via `diff`, and `git
  status --porcelain` shows no modification to any tracked file.
- Row 10's diagnostic `librade.so*` placement under `/usr/local/lib`
  removed.
- All manually-placed install artifacts (`/usr/local/bin/nereusd`,
  `/usr/lib/systemd/system/nereusd.service`,
  `/usr/local/share/nereusd/nereusd.conf.sample`) removed; the unit was
  disabled (`systemctl disable`) before removal.
- `/etc/nereusd.conf` removed.
- Every throwaway `--profile` directory (`task13bench`, `task13bench2`,
  `task13bench3`) removed from `~/.config/NereusSDR/profiles/` on the Pi.
- Confirmed no bare `~/.config/NereusSDR/NereusSDR.settings` (the
  shared/default store) was ever created on the Pi: every run in this
  session used either `--profile <throwaway>` or, for the two
  systemd-managed runs, `DynamicUser`'s own isolated `$HOME=
  /var/lib/nereusd`, distinct from the `jj` user's real config directory
  either way.
- No stray `nereusd` processes confirmed via `pgrep` after every stop in
  this session.
- No transmit occurred at any point in this session (RX only; every
  config used defaults with no TUNE/MOX request, confirmed by reviewing
  every command run against the radio).
