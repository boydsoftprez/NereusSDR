# Optional microphone capture helper implementation plan

> **Execution:** run with `crew` under `yonder-cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort
> follow the risk-based policy. No review between tasks; one whole-branch
> review at the end.

**Goal:** receiving never waits for the PC microphone. Native microphone
capture runs in a small helper program (`nereus-audio-capture`) that
NereusSDR starts, stops and, when it hangs, kills; the app shows
Preparing / Ready / a concrete failure with Retry beside the microphone
controls, and PC-mic transmission is admitted only when that input is ready.

**Architecture:** a bounded binary/JSON record protocol over the child's
private stdin/stdout pipes. `CaptureSupervisor` (core, owned by
`AudioEngine`) spawns and supervises the helper on an owned I/O thread,
tracks demand leases, generations and deadlines, and writes PCM into
`CaptureAudioBus`, a stable reader that replaces the directly opened
PortAudio input bus. `RadioModel` derives session demand and composes
microphone readiness into the existing MOX precheck.

**Tech stack:** C++20, Qt6 Core (`QProcess`, `QThread`, `QJsonDocument`),
PortAudio through the existing `PortAudioBus` and r8brain `Resampler` (only
inside the helper), AVFoundation on macOS (existing `MacMicPermission`),
CMake/Ninja, Qt Test with the self-re-exec test pattern from
`tests/tst_daemon_signals.cpp`.

**Spec:** [Optional microphone capture lifecycle](2026-09-22-optional-microphone-capture-design.md),
approved by the operator on 2026-09-22 (helper program option and the
operator behavior list). Requirement R-R3-36; preserves R-R3-06/07/17 and
the local desktop Core/DSP path.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
  Never edit or build in `/Users/j.j.boyd/NereusSDR` or any other checkout.
- Every change traces to R-R3-36; R-R3-06/07/17/21 behavior is preserved.
- Commits: GPG-signed (`git commit -S`), hooks run with
  `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`; never `--no-gpg-sign`, never
  `--no-verify`. No `Co-Authored-By` trailer. No em-dash characters in
  commit messages or docs. Stage explicit paths only.
- New source files carry the house header plus
  `// no-port-check: NereusSDR-original. <reason>` (see
  `src/core/session/media/DaemonAudioSender.h`). This plan ports no Thetis
  logic; if a task finds it needs Thetis behavior, stop and report.
- `src/core` and `src/models` stay GUI-free (`tst_core_has_no_gui_includes`
  scans them). Use `AppSettings`, never `QSettings`.
- C++ style: braces on every control-flow body; no raw `new`/`delete`
  except Qt parent ownership; `m_camelCase`, `kPascalCase`; `qCWarning` /
  `qCInfo` logging, no exceptions; platform guards `Q_OS_MAC` / `Q_OS_WIN` /
  `Q_OS_LINUX`.
- The parent process never calls a PortAudio input API, never waits on the
  helper synchronously from the GUI or a DSP/audio thread, and never kills
  anything but its own `QProcess` child. No `Pa_Terminate` or detached
  thread workaround for a stuck open. Speaker output (in-process
  `PortAudioBus` output) is unchanged.
- Fixed contract values (used verbatim by every task):
  protocol version 1; record magic bytes `N C A P`; header 12 bytes;
  JSON payload at most 4096 bytes; PCM record = 24-byte PCM header plus
  1..4800 float32 frames (mono, 48000 Hz); the helper sends 480-frame
  (10 ms) PCM records; helper hello deadline 3000 ms from process start;
  open deadline 10000 ms from Open to the first valid PCM record of that
  generation, paused while the helper reports it is waiting for OS
  permission; stop deadline 2000 ms, then `QProcess::kill()`; parent reader
  ring 4800 frames (100 ms, the existing PortAudioBus capture ring
  duration). The native checkpoint (Task 8) measures real open times and
  delivery delay; only that evidence may change these values.
- A named microphone that cannot be found is a failure, never a silent
  switch to another device. An empty device name means the system default.
- Operator strings are plain English, with no protocol, generation, pipe or
  process wording. Raw diagnostics go to the log.
- Tests: build the exact targets first, then run
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  Record `uptime` load averages with any timing. Do not run the unfiltered
  suite. No test opens a real microphone except the existing native tests;
  new lifecycle tests use the fake child or the real helper with a
  nonexistent device.
- Hardware (Rock, Saturn, the running GUI, real microphones, RF) is off
  limits to implementers.

## What already exists

- `AudioEngine::start()` opens speakers then synchronously calls
  `ensureTxInputOpen()` (`src/core/AudioEngine.cpp:399-400`, body
  `:837-865`), which loads `AudioDeviceConfig::loadFromSettings("audio/TxInput")`
  and `makeBus(cfg, true)` (`:542-564`, always `PortAudioBus`, blocking
  `open()`). `setTxInputConfig()` (`:980-995`) resets `m_txInputBus` and
  reopens synchronously with no lock, racing `pullTxMic()` on the TX worker.
  `m_txInputBus` is `std::unique_ptr<IAudioBus>` (`AudioEngine.h:792`).
  `isPcMicSelected()` (intent, `.cpp:1466`), `isPcMicOverrideActive()`
  (intent and open bus, `.cpp:1471`), `pullTxMic()` (`.cpp:1506-1566`, handles
  Int16 or Float32, takes channel 0), `pcMicInputLevel()` (`.cpp:1914-1920`,
  bus `txLevel()`, polled by PhoneCwApplet every 50 ms and Test Mic every
  10 ms), signal `txInputConfigChanged(AudioDeviceConfig)` (`h:720`), test
  seam `setTxInputBusForTest()` (`h:352`). PortAudio is initialized once per
  `AudioEngine` (`.cpp:186-194`, `248-251`).
- `PortAudioBus::open()` (`src/core/audio/PortAudioBus.cpp:268-460`):
  `resolveDevice()` (`:50-216`) silently falls back when a named device is
  missing (`:159-165`), opens at the device's native rate, resamples with
  `Resampler` to the requested rate, and the callback writes a 100 ms SPSC
  ring; `txLevel()` is the block peak `max(|x|)` (`:850-866`).
- TX worker: `TxWorkerThread::dispatchOneBlock()` pulls 64 mono float
  frames per call (`TxWorkerThread.cpp:592` RADE, `:834` WDSP) at radio mic
  block cadence and zero-fills when PC is selected but short or unavailable
  (commit `5a1c7ef3`). DEXP/VOX reads the same block (`:874`). TCI audio
  pre-empts the mic (`:497-515`); Tune and two-tone are WDSP PostGen and never
  read the mic.
- Teardown: `RadioModel::teardownConnection()` calls
  `m_audioEngine->stop()` (`RadioModel.cpp:13618`, which resets the TX
  input bus at `AudioEngine.cpp:473`) before `m_txWorker->stopPump()`
  (`:13636`).
- Daemon: `DaemonApp` owns one `RadioModel` (`DaemonApp.cpp:95`); its
  connect path runs `AudioEngine::start()` (`RadioModel.cpp:7837`), so
  `nereusd` opens a PC microphone today.
- Configuration: Devices page TX Input `DeviceCard` (prefix `audio/TxInput`,
  `AudioDevicesPage.cpp:47-53`) persists with `cfg.saveToSettings(m_prefix)`
  (`DeviceCard.cpp:635`) and calls `AudioEngine::setTxInputConfig`
  (`AudioDevicesPage.cpp:94-106`). `AudioDeviceConfig` fields: `deviceName`
  (empty = platform default), `sampleRate` 48000, `channels` 2,
  `bufferSamples` 128, `exclusiveMode`, `hostApiIndex` -1, `driverApi`,
  `bitDepth` 32, `eventDriven`, `bypassMixer`, `manualLatencyMs`. The "TX
  Input" Setup page `AudioTxInputPage` (requires transmit) edits
  session-only `TransmitModel::pcMicHostApiIndex` (-1), `pcMicDeviceName`
  (empty), `pcMicBufferSamples` (512) that never reach capture
  (`TransmitModel.cpp:2477-2500`). Test Mic (`AudioTxInputPage.cpp:695-738`)
  only starts a 10 ms VU timer.
- Mic source: `MicSource {Pc=0, Radio=1, Vax=2}`
  (`src/core/audio/CompositeTxMicRouter.h:66-70`); `TransmitModel::micSource`
  persisted per radio, default `Pc` (`TransmitModel.cpp:1416-1447`), wired to
  `AudioEngine::onMicSourceChanged` (`RadioModel.cpp:9282-9292`).
- MOX: `MoxController::setMox(true)` runs `m_moxCheck` first
  (`MoxController.cpp:494-500`) and returns before interlocks and all RF
  effects on refusal, emitting `moxRejected(reason)` (toast 3000 ms in
  `MainWindow.cpp:5394-5397`; TxApplet resync `:1069-1075`). The check is
  installed by `RadioModel::installBandPlanMoxCheck()`
  (`RadioModel.cpp:10609-10662`: remote refusal, then band plan).
  `setMox(false)` never runs checks. `MoxController::isManualMox()` is true
  for TUN (`MoxController.h:236-247`); `TwoToneController::isActive()`
  (`TwoToneController.h:224`). PTT sources: TxApplet button, TCI `trx`
  (`RadioModel::setMox`, `RadioModel.cpp:14892-14904`), VOX
  (`MoxController::onVoxActive`, `:1246-1254`).
- macOS permission: `src/core/MacMicPermission.{h,mm}` requests
  AVCaptureDevice access at app launch (`src/main.cpp:159`); entitlements
  `packaging/macos/app-entitlements.plist` (`device.audio-input`,
  `cs.disable-library-validation`).
- Build: `NereusSDR` (`CMakeLists.txt:1573`), `nereusd` links `NereusCore`
  only (`:1650-1651`); NereusCore-only executable template
  `nereus-media-probe` (`:2063-2066`). macOS POST_BUILD copies private libs
  into `Contents/Frameworks`, injects `NSMicrophoneUsageDescription`, then
  `codesign --force --deep --sign -` (`:1618-1629`); app rpath
  `@executable_path/../Frameworks` (`:1589-1596`). `release.yml` signs the
  app with Developer ID, `--options runtime`, entitlements (`:643-650`);
  Linux AppImage installs the default component into AppDir
  (`:190-192`); Windows hand-copies non-Qt binaries into `deploy\`
  (`:899-912`). `nereusd` install component (`CMakeLists.txt:2375-2465`).
  Path lookup precedent: `src/core/ModelPaths.cpp:33-58`.
- Test patterns: self-re-exec (`tests/tst_daemon_signals.cpp:24-26,108-113,135-139`);
  `tests/fakes/FakeAudioBus.h`; existing targets `tst_audio_engine_pull_tx_mic`,
  `tst_tx_worker_thread`, `tst_rade_tx_pump`, `tst_audio_tx_input_pc_mic_group`,
  `tst_audio_tx_input_page_skeleton`, `tst_device_card`,
  `tst_audio_device_config_roundtrip`, `tst_band_plan_guard_mox_rejection`,
  `tst_pc_mic_source`, `tst_remote_gui_gating`.

## File Structure

| Create | Responsibility |
| --- | --- |
| `src/core/audio/CaptureProtocol.{h,cpp}` | Record framing, bounded incremental reader, JSON and PCM message codecs |
| `src/core/audio/CaptureHelper.{h,cpp}` | The helper's logic: control loop, native open through PortAudioBus, PCM pump, stdin watchdog |
| `src/capture_main.cpp` | `nereus-audio-capture` entry point calling `runCaptureHelper()` |
| `src/core/audio/CaptureSupervisor.{h,cpp}` | Parent-side process supervision, demand leases, generations, deadlines, status |
| `src/core/audio/CaptureAudioBus.{h,cpp}` | Stable input `IAudioBus` reader fed by the supervisor |
| `src/core/audio/CaptureHelperLocator.{h,cpp}` | Finds the installed helper per platform |
| `src/gui/setup/CaptureStatusText.{h,cpp}` | Operator wording for capture status |
| `tests/fakes/FakeCaptureChild.{h,cpp}` | Scripted protocol peer run by self-re-exec |
| `tests/tst_capture_protocol.cpp`, `tst_capture_helper_process.cpp`, `tst_capture_supervisor.cpp`, `tst_capture_helper_locator.cpp`, `tst_capture_admission.cpp`, `tst_capture_status_text.cpp` | Tests per task |

| Modify | Change |
| --- | --- |
| `src/core/audio/PortAudioBus.{h,cpp}` | Strict named-input resolution option used only by the helper |
| `src/core/MacMicPermission.{h,mm}` | Status query and blocking request for the helper |
| `src/core/AudioEngine.{h,cpp}` | Supervisor and reader replace the eager input bus; config getter; demand/retry API |
| `src/models/RadioModel.{h,cpp}`, `src/core/daemon/DaemonApp.cpp` | Session demand, daemon exclusion, MOX admission, input-loss release |
| `src/models/TransmitModel.{h,cpp}` | PC mic device fields become projections of `audio/TxInput` |
| `src/gui/setup/AudioDevicesPage.cpp`, `AudioTxInputPage.{h,cpp}` | Shared config, status, Retry, real Test Mic |
| `CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/release.yml` | Helper target, bundle placement, signing, install rules, tests |

---

## Task 1: Capture record protocol

**Requirements:** R-R3-36 (bounded parsers, no stale speech across
generations).

**Files:**
- Create: `src/core/audio/CaptureProtocol.h`, `src/core/audio/CaptureProtocol.cpp`
- Modify: `CMakeLists.txt` (core source list beside `PortAudioBus.cpp`), `tests/CMakeLists.txt` (`nereus_add_test(tst_capture_protocol)`)
- Test: `tests/tst_capture_protocol.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces (namespace `NereusSDR::CaptureProtocol`):
  ```cpp
  inline constexpr quint8 kVersion = 1;
  inline constexpr int kHeaderBytes = 12;          // "NCAP", u8 version, u8 type, u16 reserved(0), u32 payloadBytes, little-endian
  inline constexpr int kMaxJsonBytes = 4096;
  inline constexpr int kPcmHeaderBytes = 24;       // u32 generation, u32 frameCount, u64 framePosition, u64 sentMonotonicNs
  inline constexpr int kMaxPcmFrames = 4800;
  inline constexpr int kHelperPcmFrames = 480;
  inline constexpr int kSampleRate = 48'000;
  enum class RecordType : quint8 { Hello = 1, Status = 2, Pcm = 3, Configure = 16, Open = 17, Stop = 18, Shutdown = 19 };
  struct Record { RecordType type; QByteArray payload; };
  QByteArray encodeRecord(RecordType type, const QByteArray& payload); // empty QByteArray if payload exceeds the type's bound
  class RecordReader {
  public:
      enum class Error { None, BadMagic, BadVersion, BadReserved, UnknownType, Oversize };
      void append(const char* data, qsizetype size);
      std::optional<Record> next();        // one complete record, or nullopt
      Error error() const;                 // sticky; after an error next() returns nullopt forever
      qsizetype bufferedBytes() const;     // never above kHeaderBytes + the largest payload bound
  };
  struct PcmBlock { quint32 generation = 0; quint64 framePosition = 0; quint64 sentMonotonicNs = 0; QVector<float> samples; };
  // sentMonotonicNs: std::chrono::steady_clock nanoseconds when the helper wrote the record
  // (a host-wide monotonic clock on macOS, Linux and Windows), used only for delay statistics.
  QByteArray encodePcm(quint32 generation, quint64 framePosition, quint64 sentMonotonicNs, const float* samples, int frameCount);
  std::optional<PcmBlock> decodePcm(const QByteArray& payload); // frameCount 1..4800, exact size, all finite

  enum class HelperState { Permission, Opening, Ready, Failed, Stopped };
  enum class FailReason { None, PermissionDenied, DeviceNotFound, OpenFailed, StartFailed, InputLost, Internal };
  struct Hello { int protocol = 0; qint64 pid = 0; QString build; };
  struct Configure { quint32 generation = 0; AudioDeviceConfig device; };
  struct Command { quint32 generation = 0; };            // Open and Stop
  struct Status { quint32 generation = 0; HelperState state = HelperState::Stopped;
                  QString actualDevice; int nativeRate = 0; int nativeChannels = 0;
                  FailReason reason = FailReason::None; QString detail; };
  // encodeX returns a complete record; decodeX validates exact key sets.
  QByteArray encodeHello(const Hello&);       std::optional<Hello> decodeHello(const QByteArray& json);
  QByteArray encodeConfigure(const Configure&); std::optional<Configure> decodeConfigure(const QByteArray& json);
  QByteArray encodeOpen(const Command&);      QByteArray encodeStop(const Command&);
  std::optional<Command> decodeCommand(const QByteArray& json);
  QByteArray encodeStatus(const Status&);     std::optional<Status> decodeStatus(const QByteArray& json);
  QByteArray encodeShutdown();
  ```
  JSON keys: Hello `protocol`, `pid`, `build`; Configure `generation` plus
  one key per `AudioDeviceConfig` field (`deviceName`, `sampleRate`,
  `channels`, `bufferSamples`, `exclusiveMode`, `hostApiIndex`,
  `driverApi`, `bitDepth`, `eventDriven`, `bypassMixer`,
  `manualLatencyMs`); Open/Stop `generation`; Status `generation`, `state`
  (`permission|opening|ready|failed|stopped`), `actualDevice`,
  `nativeRate`, `nativeChannels`, `reason`
  (`none|permission-denied|device-not-found|open-failed|start-failed|input-lost|internal`),
  `detail`.

**Acceptance:**
- Round trips for every record and message type, including a 4800-frame
  PCM record and the full `AudioDeviceConfig` field set.
- `RecordReader` handles a stream split at every byte boundary (feed one
  byte at a time) and several records in one append.
- Rejections (sticky error or nullopt): wrong magic, version 2, nonzero
  reserved, unknown type, JSON payload 4097 bytes, PCM frameCount 0 or 4801,
  PCM size mismatch, NaN or infinite sample, JSON with a missing or extra
  key, `generation` 0 or above 4294967295, `state` or `reason` outside the
  lists, strings longer than 512 characters, `nativeRate` outside
  8000..384000, `nativeChannels` outside 1..32.
- `bufferedBytes()` never exceeds `kHeaderBytes + kPcmHeaderBytes + 4800*4`
  even when fed an endless stream without a valid header.

**Verification:** ordinary feature, pure code: deterministic unit tests.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_capture_protocol -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_capture_protocol$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** sonnet. No prerequisites.

- [ ] **Step 1:** Write `tst_capture_protocol` with the cases above.
- [ ] **Step 2:** Implement `CaptureProtocol` until it passes; commit.

## Task 2: The helper program and a scripted fake

**Requirements:** R-R3-36 (native capture isolated in a child; watchdog on
parent loss; named-device failure is visible; permission is its own phase).

**Files:**
- Create: `src/core/audio/CaptureHelper.h`, `src/core/audio/CaptureHelper.cpp`, `src/capture_main.cpp`, `tests/fakes/FakeCaptureChild.h`, `tests/fakes/FakeCaptureChild.cpp`
- Modify: `src/core/audio/PortAudioBus.h`, `src/core/audio/PortAudioBus.cpp` (strict input resolution), `src/core/MacMicPermission.h`, `src/core/MacMicPermission.mm`, `CMakeLists.txt` (core sources; `add_executable(nereus-audio-capture src/capture_main.cpp)` linking `NereusCore` PRIVATE with `nereus_apply_warnings`, not `EXCLUDE_FROM_ALL`), `tests/CMakeLists.txt`
- Test: `tests/tst_capture_helper_process.cpp`

**Interfaces:**
- Consumes: Task 1 `CaptureProtocol`.
- Produces:
  ```cpp
  // CaptureHelper.h
  int runCaptureHelper(int argc, char** argv); // reads records on stdin, writes records on stdout, logs on stderr
  // PortAudioBus: when set before open(), a missing named input device fails open()
  // with errorString() starting "device-not-found:" instead of falling back.
  void setStrictInputDevice(bool strict);
  // MacMicPermission.h (no-ops returning Granted on other platforms)
  enum class MicPermission { Granted, Denied, Undetermined };
  MicPermission microphonePermissionStatus();
  MicPermission requestMicrophonePermissionAndWait(); // blocks the calling thread until the user answers
  // FakeCaptureChild.h (tests only)
  int runFakeCaptureChild(const QString& scenario);
  ```

**Acceptance:**
- Helper behavior: sends Hello (protocol 1, pid, build tag) first. A stdin
  reader thread parses records; stdin EOF, a read error or a protocol error
  calls `std::_Exit(0)` at once, even while another thread is inside a native
  call. The main thread executes Configure/Open/Stop/Shutdown in order.
  Open for generation N: on macOS query permission; Undetermined sends
  Status `permission` and waits for the answer; Denied sends Status
  `failed`/`permission-denied`; then Status `opening`, then opens a
  `PortAudioBus` input with `setStrictInputDevice(true)`, the configured
  `AudioDeviceConfig`, requested format 48000 Hz mono Float32 (the bus keeps
  its existing native-rate resampling), sends Status `ready` with the actual
  device name, native rate and channels, and starts a 10 ms pump that pulls
  from the bus and sends 480-frame PCM records with `framePosition` counting
  from 0 per generation. A failed open sends Status `failed` with
  `device-not-found`, `open-failed` or `start-failed` and the bus error in
  `detail`. A capture that stops producing for 500 ms after Ready sends
  `failed`/`input-lost`. Stop closes the bus and sends `stopped`. Records
  for a generation other than the current Configure's are ignored.
  stdout writes are serialized by one mutex and flushed per record.
- `PortAudioBus::setStrictInputDevice` affects only input opens that set it;
  existing callers keep today's fallback. Empty `deviceName` still resolves
  the platform default.
- Fake scenarios (argument after `--fake-capture-child`): `ready`
  (hello; Opening then Ready then PCM of a 1 kHz tone every 10 ms),
  `hang-open` (hello; Opening and then nothing), `no-hello`,
  `permission-then-ready` (Permission for 300 ms, then as `ready`),
  `crash-after-ready` (exit code 3 after 100 ms of PCM), `malformed`
  (bad magic after hello), `oversize` (PCM frameCount 4801),
  `input-lost` (Ready, PCM for 100 ms, then Status failed/input-lost),
  `ignore-stop` (as `ready` but never answers Stop), `stale` (Ready and PCM
  tagged with generation N-1). All scenarios exit on stdin EOF.
- `tst_capture_helper_process` re-executes itself with `--capture-helper`
  (define `main` as `captureHelperEntryPointForTest` and include
  `src/capture_main.cpp`, as `tst_daemon_signals` does) and proves with the
  real helper: Hello arrives within 3000 ms; Configure with device name
  `"NereusSDR test device that does not exist"` plus Open yields Status
  `failed`/`device-not-found` (or `permission-denied` if macOS denies the
  test process; both are accepted and logged, neither opens a device);
  closing the helper's stdin makes it exit within 1000 ms; Shutdown makes it
  exit with code 0.

**Verification:** process and native boundary: deterministic process tests
with the real helper on a nonexistent device; real devices are Task 8.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target nereus-audio-capture tst_capture_helper_process tst_capture_protocol tst_port_audio_bus -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_capture_helper_process|tst_capture_protocol|tst_port_audio_bus)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (native lifecycle, threads, process
exit). Requires Task 1.

- [ ] **Step 1:** Add strict input resolution and the permission status /
  blocking request, keeping every existing caller's behavior.
- [ ] **Step 2:** Implement `runCaptureHelper`, `capture_main.cpp` and the
  executable target.
- [ ] **Step 3:** Implement `FakeCaptureChild` and
  `tst_capture_helper_process`; run the commands; commit.

## Task 3: Supervisor and stable reader

**Requirements:** R-R3-36 (receive, reconnect, disconnect and exit never
wait for capture; bounded deadlines; stale results ignored; failure visible
until retry, device change or a new session).

**Files:**
- Create: `src/core/audio/CaptureSupervisor.h`, `src/core/audio/CaptureSupervisor.cpp`, `src/core/audio/CaptureAudioBus.h`, `src/core/audio/CaptureAudioBus.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt` (`tst_capture_supervisor` with `fakes/FakeCaptureChild.cpp`)
- Test: `tests/tst_capture_supervisor.cpp`

**Interfaces:**
- Consumes: Task 1 protocol; Task 2 `runFakeCaptureChild` (tests only).
- Produces:
  ```cpp
  class CaptureAudioBus final : public IAudioBus {
      // Input-only reader. open() returns true and changes nothing; isOpen()
      // is true only while the supervisor's current generation is Ready.
      // pull() returns Float32 mono 48 kHz bytes, 0 when unavailable.
      // negotiatedFormat() {48000 Hz, 1 channel, Float32}; txLevel() is the
      // peak |x| of the most recent PCM record; push() returns 0.
  };
  class CaptureSupervisor final : public QObject {
      Q_OBJECT
  public:
      enum class Demand { LocalSession, TestMic };
      struct Status {
          enum class State { Closed, PreparingPermission, Opening, Ready, Failed, Stopping };
          enum class Reason { None, PermissionDenied, DeviceNotFound, OpenFailed, StartFailed,
                              InputLost, Timeout, HelperMissing, HelperDidNotStart,
                              HelperExited, ProtocolError };
          State state = State::Closed;
          QString configuredDevice;   // empty = system default
          QString actualDevice;
          Reason reason = Reason::None;
          quint32 generation = 0;
          friend bool operator==(const Status&, const Status&) = default;
      };
      class Lease { public: Lease(); Lease(Lease&&) noexcept; Lease& operator=(Lease&&) noexcept;
                    ~Lease(); void release(); bool isActive() const; /* ... */ };
      struct Options {
          QString program;            // empty: locateCaptureHelper() (Task 4)
          QStringList arguments;
          int helloTimeoutMs = 3000;
          int openTimeoutMs = 10000;
          int stopTimeoutMs = 2000;
      };
      explicit CaptureSupervisor(Options options = {}, QObject* parent = nullptr);
      ~CaptureSupervisor() override;          // calls shutdown()
      Lease acquire(Demand demand);           // GUI/owner thread only
      void configure(const AudioDeviceConfig& config);
      void retry();
      Status status() const;
      CaptureAudioBus* reader() const;        // same pointer for the supervisor's lifetime
      void shutdown();                        // idempotent; returns within stopTimeoutMs + 500 ms
  signals:
      void statusChanged(const NereusSDR::CaptureSupervisor::Status& status);
  };
  ```

**Acceptance:**
- No demand means no helper process. The first active lease starts the
  helper, sends Configure then Open for a new generation; the last release
  sends Stop, waits at most 2000 ms, kills if needed, and returns to Closed.
  A lease released twice or outliving the supervisor is harmless.
- Ready requires Status `ready` for the current generation and at least one
  valid PCM record of that generation. PCM or Status for any other
  generation is dropped. `framePosition` going backwards is a protocol
  error; a forward gap flushes the reader.
- While Ready, the supervisor logs at debug level every 10 s the delivery
  delay (parent arrival steady_clock minus `sentMonotonicNs`) as p50, p95
  and max over the window, for the Task 8 measurement. No operator display.
- Deadlines: no Hello in 3000 ms -> Failed/HelperDidNotStart; Open to first
  PCM beyond 10000 ms, not counting time in Permission ->
  Failed/Timeout with the child killed; no Stopped within 2000 ms of Stop ->
  kill. Unexpected child exit while demanded -> Failed/HelperExited.
  Malformed or oversize record -> kill, Failed/ProtocolError. Missing
  program -> Failed/HelperMissing without spawning.
- Failed persists: the supervisor never retries by itself. `retry()`,
  `configure()` with a different config, or a new demand after demand had
  dropped to zero starts a new generation.
- `configure()` retires the current generation first (reader flushed and
  unavailable before the new generation opens); a result from the retired
  generation cannot publish Ready.
- Threading: the `QProcess` lives on a `QThread` owned by the supervisor;
  PCM goes straight into the reader from that thread; `statusChanged` is
  emitted on the supervisor's thread. `acquire`, `configure`, `retry`,
  `status` and `shutdown` never block on the helper beyond `shutdown`'s
  bound. Reader SPSC ring holds 4800 frames; when full, new frames are
  dropped and counted; `pull()` from the TX worker never takes a lock.
- `tst_capture_supervisor` (fake child via self-re-exec) covers: ready path
  with samples readable from the reader and level > 0; `hang-open` ->
  Timeout at 10 s measured with a shortened `openTimeoutMs` of 500 ms, and
  `shutdown()` returning within the bound while the child hangs;
  `no-hello`; `permission-then-ready` pausing the deadline (open timeout
  300 ms still reaches Ready); `crash-after-ready` -> HelperExited and reader
  unavailable; `malformed` and `oversize` -> ProtocolError; `input-lost`;
  `ignore-stop` killed within the stop bound; `stale` never Ready; two leases
  (session and Test Mic) where releasing one keeps capture open; retry and
  configure after Failed; destroying the supervisor with a live hanging
  child returns within the bound and leaves no child process (check the
  pid is gone).

**Verification:** consequential lifecycle: invariant tests with the fake
child, including bounded shutdown against a hanging child.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_capture_supervisor tst_capture_protocol -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_capture_supervisor|tst_capture_protocol)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (process supervision, threads,
deadlines). Requires Tasks 1 and 2.

- [ ] **Step 1:** Write the supervisor tests against the fake scenarios.
- [ ] **Step 2:** Implement `CaptureAudioBus` and `CaptureSupervisor`; run
  the commands; commit.

## Task 4: Build, locate and package the helper

**Requirements:** R-R3-36 (packaged beside its parent, same revision),
R-R3-10 (self-contained desktop).

**Files:**
- Create: `src/core/audio/CaptureHelperLocator.h`, `src/core/audio/CaptureHelperLocator.cpp`
- Modify: `CMakeLists.txt` (macOS bundle placement and signing, install rules), `.github/workflows/release.yml` (sign and ship the helper), `src/core/audio/CaptureSupervisor.cpp` (default program from the locator), `tests/CMakeLists.txt`
- Test: `tests/tst_capture_helper_locator.cpp`

**Interfaces:**
- Consumes: Task 2 target `nereus-audio-capture`; Task 3 `Options::program`.
- Produces:
  ```cpp
  // Returns the absolute helper path or an empty string when none exists.
  QString locateCaptureHelper(const QString& applicationDir = QCoreApplication::applicationDirPath());
  ```

**Acceptance:**
- Lookup order, first existing executable wins: macOS
  `<appDir>/../Helpers/nereus-audio-capture`, then
  `<appDir>/nereus-audio-capture`; Linux `<appDir>/nereus-audio-capture`,
  then `<appDir>/../lib/nereus/nereus-audio-capture`; Windows
  `<appDir>/nereus-audio-capture.exe`. Tested with temporary directory
  layouts for the current platform, including "none present".
- macOS build: `NereusSDR` depends on `nereus-audio-capture`; POST_BUILD
  copies the helper to `NereusSDR.app/Contents/Helpers/`, the helper's
  rpath resolves `@executable_path/../Frameworks`, the helper is signed
  (ad hoc locally, with `packaging/macos/app-entitlements.plist`) before the
  existing bundle signing, and `codesign --verify --strict --deep` of the
  bundle passes. `otool -L` of the copied helper shows only system,
  Qt and `@rpath` NereusCore dependencies that resolve inside the bundle.
- Linux: a default-component `install(TARGETS nereus-audio-capture RUNTIME
  DESTINATION ${CMAKE_INSTALL_BINDIR})` so the AppImage includes it. The
  `nereusd` install component carries no helper: the daemon never opens a
  microphone and a daemon-only build does not produce the helper.
- Windows: POST_BUILD copy next to `NereusSDR.exe`; `release.yml` copies it
  into `deploy\` with the same `Test-Path` failure guard as `rade.dll`.
- `release.yml` macOS signs `Contents/Helpers/nereus-audio-capture` with the
  Developer ID, `--options runtime --timestamp` and the app entitlements
  before the app signing step, and verifies it.
- The supervisor's default `Options::program` is `locateCaptureHelper()`;
  an empty result gives Failed/HelperMissing.

**Verification:** packaging: build-tree evidence on macOS (bundle layout,
`codesign --verify`, `otool -L`), locator unit test; Linux/Windows packaging
steps are checked by reading `release.yml` and remain pending until the next
tagged build.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target NereusSDR nereus-audio-capture tst_capture_helper_locator -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_capture_helper_locator$' --no-tests=error --output-on-failure
codesign --verify --strict --deep /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration/NereusSDR.app
otool -L /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration/NereusSDR.app/Contents/Helpers/nereus-audio-capture
```

**Execution note (advisory):** sonnet. Requires Tasks 2 and 3.

- [ ] **Step 1:** Implement the locator and its test.
- [ ] **Step 2:** Add bundle placement, signing, install rules and the
  `release.yml` steps; run the commands; commit.

## Task 5: AudioEngine uses the supervisor; session demand; daemon exclusion

**Requirements:** R-R3-36 (receive startup and reconnect do not open
capture; unneeded capture never blocks; safe teardown), R-R3-06/07.

**Files:**
- Modify: `src/core/AudioEngine.h`, `src/core/AudioEngine.cpp`, `src/models/RadioModel.h`, `src/models/RadioModel.cpp`, `src/core/daemon/DaemonApp.cpp`
- Test: `tests/tst_audio_engine_pull_tx_mic.cpp`, `tests/tst_tx_worker_thread.cpp`, new `tests/tst_capture_session_demand.cpp` (registered with `fakes/FakeCaptureChild.cpp`)

**Interfaces:**
- Consumes: Task 3 `CaptureSupervisor`, `CaptureAudioBus`; Task 4 locator.
- Produces:
  ```cpp
  // AudioEngine
  AudioDeviceConfig txInputConfig() const;
  CaptureSupervisor::Status captureStatus() const;
  CaptureSupervisor::Lease acquireCaptureDemand(CaptureSupervisor::Demand demand);
  void retryCapture();
  // NEREUS_BUILD_TESTS: replaces the supervisor with one built from these options;
  // valid only while no lease is active (asserts otherwise).
  void setCaptureSupervisorOptionsForTest(CaptureSupervisor::Options options);
  signals: void captureStatusChanged(const NereusSDR::CaptureSupervisor::Status& status);
  // RadioModel
  void setPcCaptureAllowed(bool allowed); // default true; DaemonApp sets false
  bool pcCaptureRequired() const;         // PC source selected and local keying would read PC capture
  ```

**Acceptance:**
- `AudioEngine::start()` no longer opens any input; `ensureTxInputOpen()`
  is removed. The engine owns one `CaptureSupervisor` created in its
  constructor with the persisted `audio/TxInput` config applied through
  `configure()` (no open without demand). `setTxInputConfig()` stores the
  config, calls `configure()`, and emits `txInputConfigChanged` as today.
- The TX input read path uses `supervisor->reader()` unless a test bus was
  injected with `setTxInputBusForTest`; `stop()` no longer destroys the
  input reader; `pullTxMic()`, `isPcMicOverrideActive()` and
  `pcMicInputLevel()` keep their signatures and meanings (Ready reader =
  open bus).
- `RadioModel` holds a `LocalSession` lease exactly while: a local radio
  connection is up, `setPcCaptureAllowed(true)`, and the mic source is Pc.
  It acquires after `AudioEngine::start()` returns and releases on source
  change away from Pc and in teardown after the TX worker has stopped.
  `DaemonApp` calls `setPcCaptureAllowed(false)` before connecting, so
  `nereusd` never starts the helper. A remote-role GUI never holds a lease.
- Tests: with a `hang-open` fake installed through
  `setCaptureSupervisorOptionsForTest`, `AudioEngine::start()` and a full
  local connect/disconnect through the existing connectable fixture return
  promptly (under 1000 ms each) and the radio receive path starts; with
  `ready`, the TX worker pulls fake tone samples; with PC selected and
  capture Failed the existing zero-fill tests still pass; switching the
  source to Radio releases the lease and the helper exits; a daemon-flagged
  model connects without starting the helper. Existing
  `tst_audio_engine_pull_tx_mic`, `tst_tx_worker_thread`, `tst_rade_tx_pump`,
  `tst_pc_mic_source` pass unchanged.

**Verification:** consequential audio path change: invariant tests first
(startup returns with a hanging helper, no radio-mic leak), then wiring.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_capture_session_demand tst_audio_engine_pull_tx_mic tst_tx_worker_thread tst_rade_tx_pump tst_pc_mic_source tst_capture_supervisor nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_capture_session_demand|tst_audio_engine_pull_tx_mic|tst_tx_worker_thread|tst_rade_tx_pump|tst_pc_mic_source|tst_capture_supervisor)$' --no-tests=error --output-on-failure
```
Also build `tests_core` and `tests_models` and run `ctest -L core` and
`ctest -L models` once before committing, because `AudioEngine` and
`RadioModel` are shared by many tests.

**Execution note (advisory):** opus (TX audio path, teardown). Requires
Tasks 3 and 4.

- [ ] **Step 1:** Add `tst_capture_session_demand` (hanging helper does not
  delay start/connect; daemon flag; source switch) and confirm the startup
  case fails on the current tree.
- [ ] **Step 2:** Move the engine to the supervisor and reader, add the
  RadioModel lease and the daemon flag; run the commands; commit.

## Task 6: One microphone configuration, real Test Mic, visible status

**Requirements:** R-R3-36 (Devices and PC Mic show and edit the same
input; Test Mic really opens it; Preparing / Ready / failure with Retry
beside the microphone controls).

**Files:**
- Create: `src/gui/setup/CaptureStatusText.h`, `src/gui/setup/CaptureStatusText.cpp`
- Modify: `src/gui/setup/AudioDevicesPage.cpp`, `src/gui/setup/AudioTxInputPage.h`, `src/gui/setup/AudioTxInputPage.cpp`, `src/models/TransmitModel.h`, `src/models/TransmitModel.cpp`, `src/models/RadioModel.cpp` (projection wiring)
- Test: new `tests/tst_capture_status_text.cpp`, `tests/tst_audio_tx_input_pc_mic_group.cpp`, `tests/tst_device_card.cpp` or `tests/tst_audio_tx_input_page_skeleton.cpp`

**Interfaces:**
- Consumes: Task 5 `AudioEngine::txInputConfig()`, `setTxInputConfig()`,
  `captureStatus()`, `captureStatusChanged`, `acquireCaptureDemand()`,
  `retryCapture()`.
- Produces:
  ```cpp
  QString captureStatusText(const CaptureSupervisor::Status& status); // src/gui/setup/CaptureStatusText.h
  ```

**Acceptance:**
- Wording (exact): Closed "Microphone not in use"; PreparingPermission
  "Waiting for microphone permission"; Opening "Preparing microphone";
  Ready "Microphone ready: <actualDevice>"; Stopping "Stopping microphone";
  Failed by reason: PermissionDenied "Microphone access is turned off for
  NereusSDR. Allow it in System Settings, then retry."; DeviceNotFound
  "The selected microphone \"<configuredDevice>\" is not available.";
  OpenFailed and StartFailed "The selected microphone could not be
  opened."; InputLost "The microphone stopped sending audio."; Timeout "The
  microphone did not respond in time."; HelperMissing "Microphone support
  is missing from this installation."; HelperDidNotStart, HelperExited and
  ProtocolError "Microphone support stopped unexpectedly." An empty
  configured device reads as "the system default microphone" where a name
  is shown.
- Both the Devices page (below the TX Input card) and the TX Input page
  (in the PC Mic group, beside Test Mic) show a status label
  (objectName `captureStatus`) and a "Retry microphone" button (objectName
  `retryCapture`) enabled only in Failed; clicking calls
  `AudioEngine::retryCapture()`. Both refresh on `captureStatusChanged`.
- The TX Input page's PC mic device controls read `txInputConfig()` and
  write through `setTxInputConfig()` with the same persistence as the
  Devices card (`saveToSettings("audio/TxInput")`); a change on either page
  appears on the other. `TransmitModel::pcMicHostApiIndex`,
  `pcMicDeviceName` and `pcMicBufferSamples` mirror the engine config via
  `txInputConfigChanged` and their setters forward to
  `AudioEngine::setTxInputConfig` through RadioModel, so there is one
  selection. Existing persisted `audio/TxInput` values are untouched.
- Test Mic: while checked, the page holds a `TestMic` lease (released on
  uncheck, page hide, page destruction); the meter reads the real
  `pcMicInputLevel()`. Test Mic works before a radio is connected.
- Tests: wording table; page tests with a fake supervisor status (drive
  `captureStatusChanged` from an engine whose supervisor uses fake
  scenarios) showing each label and the Retry enable rule; cross-page
  config round trip; Test Mic lease acquired and released (helper process
  starts and exits with the `ready` fake).

**Verification:** console interaction plus functional checks: offscreen
widget tests that exercise Retry and Test Mic and observe supervisor
effects; native look and real device behavior are Task 8.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_capture_status_text tst_audio_tx_input_pc_mic_group tst_audio_tx_input_page_skeleton tst_device_card tst_audio_device_config_roundtrip NereusSDR -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_capture_status_text|tst_audio_tx_input_pc_mic_group|tst_audio_tx_input_page_skeleton|tst_device_card|tst_audio_device_config_roundtrip)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** sonnet. Requires Task 5.

- [ ] **Step 1:** Implement `captureStatusText` with its table test.
- [ ] **Step 2:** Bind both pages to the single config, add status and
  Retry, make Test Mic a real demand, project the TransmitModel fields; run
  the commands; commit.

## Task 7: PC-mic keying waits for a ready microphone

**Requirements:** R-R3-36 (PC-dependent keying refused before RF, never
queued; unkey always works; input loss while keyed uses ordinary release),
R-R3-21.

**Files:**
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp`
- Test: new `tests/tst_capture_admission.cpp`, `tests/tst_band_plan_guard_mox_rejection.cpp`

**Interfaces:**
- Consumes: Task 5 `pcCaptureRequired()`, `AudioEngine::captureStatus()`,
  `captureStatusChanged`.
- Produces: no new public API; the refusal text below.

**Acceptance:**
- The check installed by `installBandPlanMoxCheck()` keeps its current
  order (remote refusal, then band plan) and then, when PC capture is
  required and `captureStatus().state != Ready`, returns
  `{false, "Microphone is not ready. Check Audio settings and retry."}`.
  PC capture is required only when the mic source is Pc, TCI audio is not
  active, the keying is not Tune (`MoxController::isManualMox()`; confirm it
  is set before `setMox(true)` runs the check, otherwise carry tune intent
  explicitly) and two-tone is not active (`TwoToneController::isActive()`).
- A refusal emits `moxRejected` and produces no `hardwareFlipped`, no
  antenna, MOX-bit or T/R change, no TX worker start and no RX withdrawal
  (assert with the same spies the band-plan rejection tests use). Becoming
  Ready later does not key; a new `setMox(true)` is needed.
- While MOX is on with PC capture required, a status change away from Ready
  calls `MoxController::setMox(false)` once (ordinary release); the worker
  keeps feeding zeros until release completes. `setMox(false)` is never
  blocked.
- Radio mic, VAX, TCI audio, Tune and two-tone key normally with capture
  Closed or Failed. The remote-role refusal is unchanged.
- VOX with PC selected and capture not Ready never keys (DEXP reads
  zeros); with Ready it keys through the same check.

**Verification:** RF-admission state transition: invariant tests before
wiring; no RF on hardware in this task.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_capture_admission tst_band_plan_guard_mox_rejection tst_tx_interlock_policy tst_tx_worker_thread -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_capture_admission|tst_band_plan_guard_mox_rejection|tst_tx_interlock_policy|tst_tx_worker_thread)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (TX safety). Requires Task 5; may run
before or after Task 6.

- [ ] **Step 1:** Write `tst_capture_admission` (refusal without RF
  effects for each PTT source, exemptions, no replay, input-loss release,
  unkey always allowed).
- [ ] **Step 2:** Compose the check and the input-loss release; run the
  commands; commit.

## Task 8: Native checkpoint (controller with the operator)

**Requirements:** R-R3-36 native acceptance.

Not an implementer task. After the whole-branch review and the full suite,
on the operator's Mac with the signed build-tree bundle:
1. Launch NereusSDR; confirm no microphone indicator until a mic is needed.
2. Setup, Audio, TX Input: press Test Mic. Record whether macOS asks for
   permission again (helper attribution), time to "Microphone ready", the
   device name shown, and that the meter moves with speech.
3. Select a microphone, unplug it (or turn off the headset), press Test Mic:
   the page names the missing device; Retry after reconnecting works.
4. A Bluetooth headset microphone: record its open time against the 10 s
   deadline.
5. Measure added delivery delay: helper PCM timestamps against parent
   arrival (debug log line added by Task 3), 5 minutes of Test Mic.
6. Quit NereusSDR while Test Mic is preparing; the app exits promptly and
   no `nereus-audio-capture` process remains (`pgrep`).
7. Local radio session with PC mic selected: receive starts immediately;
   PC-mic keying is refused before Ready with the refusal text. No RF
   transmission is performed in this checkpoint unless the operator
   explicitly runs it.
Record each observation separately in
`docs/architecture/2026-09-20-remote-daemon-r3-verification/capture-helper.md`.
Deadlines change only if these measurements require it.
