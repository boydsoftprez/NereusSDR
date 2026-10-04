# CAT verification through Task 12

Verified 2026-10-04 against the accepted Core baseline `27716f5d` and Task 12
base `789c3523`, using Thetis v2.10.3.15 / `3759d096`. This records automated
software and platform evidence. Final application/full-suite acceptance belongs
to Task 14; radio, RF and external-client acceptance remain pending.

The catalogue has 419 descriptors, 349 active registrations and 70 inactive
entries. All 783 current request fixtures execute through the production service
and models; none has an invented pending reply. The classified outcomes remain
Faithful 15, Adapted 163, SourceInert 42, Unavailable 129 and Inactive 70.
An active registration can implement an explicit unavailable contract. These
counts establish complete accounting, rather than functional parity for missing
recording, VAC, CWX, memory or dedicated controller integrations. See the
[fixture contracts](../../../tests/data/cat/README.md) and
[command mapping](../cat/2026-10-04-command-mapping.csv).

## Production integration and architecture

`tst_cat_integration` sends real loopback TCP bytes through `CatTcpTransport`,
framing, the production parser/router and `RadioModel`. It checks actual
frequency/mode/filter state and notifications, TX selection/readback, and exact
RX receiver-number/Hz and TX-frequency intent delivered to an identified
no-hardware `RadioConnection` subclass. Hardware receiver numbers come from the
native `ReceiverManager` mapping; they are not guessed from slice IDs. Existing
`injectConnectionForTest` and `wireReceiverManagerHardwarePushesForTest` seams
install that connection and its production signal wiring. No new runtime test
API or replacement permission gate was added.

The integration covers GUI-focus independence, A/B routing without slice
creation, foreign-owner and receive-only refusal, selected-slice on-air freeze,
removed/reused incarnations and explicit rebind, AI routing on reversed bindings,
shared CAT claims, disconnect/live reconfiguration cleanup, preservation of a
newer accepted operator claim, retirement during parser logging, remote-role
refusal and inert startup. Native serial tests also exercise deletion and restart
inside a real device-disappearance callback. Local QObject observers disconnect
stack-capturing test callbacks before their captured state leaves scope.

CAT RX controls use the existing SliceModel/RxChannel, AudioEngine,
StepAttenuatorFacade, admitted diversity and off-air DSP-buffer APIs. Task 12
changes no model setter/wiring, WDSP algorithm, I/Q routing, audio callback or
thread architecture. Its production changes are limited to two native transport
fixes described below. `tst_core_has_no_gui_includes` passes against all current
CAT sources. Software-model keying uses the existing coordinator/controller and
a deterministic band-plan admission seam; it does not prove radio MOX-bit
transmission, discovery/connect, received I/Q, WDSP-to-speaker audio or RF.

## Platform and optional-dependency evidence

| Environment | Observed evidence | Remaining evidence |
| --- | --- | --- |
| macOS 27.0 arm64, Qt 6.11.0 | Rebuilt all 18 CAT targets plus 12 named R1/MOX/arbiter/TCI/TX regressions: 30/30 pass. Fresh affected serial/integration rerun after test callback-lifetime refinements: 2/2 pass. Actual QSerialPort on owned PTY slaves and native CAT PTY byte/lifecycle tests pass. | Real serial cable/modem inputs, external clients and radio/audio/RF bench. |
| Linux 6.12.76-linuxkit aarch64, Debian 13, GCC 14.2, Qt 6.8.2 | Current read-only source compiled in a disposable network-disabled container into a separate writable build; all 16 Core-only CAT targets pass. Fresh affected serial/integration rerun: 2/2 pass. Actual native PTY first-command/no-echo, observed HUP/reopen, queue, PTT and callback cases pass. Compiler defines `HAVE_SERIALPORT`; actual Core linkage includes Qt SerialPort. | Linux GUI/application and x86 CI execution, real cables/clients and bench. This local Linux build explicitly disables optional DFNR. |
| Independent macOS build without SerialPort | Fresh `build-no-serial-task12` configure/build; serial, serial_ptt, settings, headless, PTY, integration, setup and applet pass 8/8. Actual serial/setup compile commands lack `HAVE_SERIALPORT` and SerialPort include paths; Core and GUI linkage omit Qt SerialPort. | Native QSerialPort-only slots are compiled out and provide no physical serial evidence. Unavailable-state assertions and native PTY tests still execute. |
| Windows | CI Qt module lists now explicitly request `qtserialport`, while the production dependency stays optional. Existing test registration/sharding validation passes. | Actual Windows Qt SDK/compiler and execution are unavailable locally. Guard inspection and CI configuration are not Windows compilation or runtime evidence. Hosted CI has not been dispatched. |

Existing local dependency sources and hash-checked Opus archives supplied the
fresh Linux and no-SerialPort builds; no old application/dependency binaries were
used as platform proof, no host package was installed and no network privilege,
device mount or existing unrelated container was used. The CI edit only adds
`qtserialport` to the three existing install-Qt module lists. The new test uses
`nereus_add_test(... CORE_ONLY)` and the existing automatic labels/sharding.

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

## Reproducing focused checks

Read [fast-test-loop](../../development/fast-test-loop.md), configure with
`-DNEREUS_BUILD_TESTS=ON`, then build named targets before anchored CTest runs.
The Task 12 macOS selection is:

```sh
cat_targets=(
  tst_cat_catalog tst_cat_parser tst_cat_bindings tst_cat_settings
  tst_cat_headless tst_cat_tx_ownership tst_cat_rx_commands tst_cat_dsp_commands
  tst_cat_tx_commands tst_cat_coverage tst_cat_tcp tst_cat_reporting
  tst_cat_serial tst_cat_serial_ptt tst_cat_pty tst_cat_setup tst_cat_applet
  tst_cat_integration tst_core_has_no_gui_includes tst_tx_slice_arbiter
  tst_tx_slice_binding_invariant tst_band_plan_guard_mox_rejection
  tst_radio_model_set_tune tst_two_tone_controller
  tst_mox_controller_ptt_source_dispatch tst_mox_controller_ptt_sources
  tst_mox_controller_phase_signals tst_mox_controller_tune tst_tci_tx_mutex
  tst_tx_frequency_follows_tx_slice
)
cmake --build build --target "${cat_targets[@]}" -j6
cat_regex="^($(IFS='|'; printf '%s' "${cat_targets[*]}"))$"
ctest --test-dir build -R "$cat_regex" --output-on-failure
```

For no-SerialPort, configure a separate directory with
`-DCMAKE_DISABLE_FIND_PACKAGE_Qt6SerialPort=ON`, build the eight named targets in
the platform table and run their exact anchored regex. The verification builds
also used `FETCHCONTENT_FULLY_DISCONNECTED`, per-dependency source overrides and
`NEREUS_DEPENDENCY_ARCHIVE_DIR` for their existing local sources/archives. The
Linux selection is the 16 CAT targets above excluding GUI setup/applet; it was
built before running the anchored tests on the actual Linux kernel.

Raw command/exit/platform evidence is retained locally in
`.crew/2026-10-04-thetis-cat-plan/task-12-*.log`; native per-case output is also
preserved in the corresponding CTest `LastTest.log` snapshots. Actual durations:
macOS 30-target CTest 64.87 s, final Linux 16-target CTest 21.99 s, independent
no-SerialPort CTest 18.23 s. Task 12 does not run the whole suite. Hamlib rigctld
is Task 13 and is not delivered or tested by these results.
