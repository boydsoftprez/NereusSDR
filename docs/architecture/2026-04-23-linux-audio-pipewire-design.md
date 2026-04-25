# Linux Audio — PipeWire-native Bridge — Design Spec

**Status:** Design drafted 2026-04-23
**Author:** JJ Boyd (KG4VCF), drafted with Claude Code (Opus 4.7, 1M context)
**Branch:** `ubuntu-dev`
**Scope:** Replace the `pactl`-primary Linux audio bridge with a PipeWire-native
primary path (libpipewire-0.3) and a `pactl`/`QAudioSink` fallback, add
per-stream live telemetry, split CW sidetone and TX MON into their own sinks
with independent latency budgets, introduce per-slice output routing, and
add a first-run remediation dialog so end users on modern distros get working
audio with zero manual package installation.

---

## 1. Context and motivation

### 1.1 Observed failure

On Ubuntu 25.10 with PipeWire 1.4.7 and pipewire-pulse running as the
PulseAudio replacement, launching NereusSDR produces:

```
WRN: LinuxPipeBus: pactl timed out: QList("load-module", ...)
WRN: LinuxPipeBus: "pactl load-module failed for /tmp/nereussdr-vax-3.pipe"
WRN: LinuxPipeBus open failed for VAX 3 : "pactl load-module failed ..."
INF: AudioEngine started ( speakers bus NOT open )
```

Every VAX source and the TX sink fail to open. WSJT-X cannot see any
NereusSDR virtual device. The user gets no in-app feedback explaining
why.

### 1.2 Root cause

`LinuxPipeBus` (`src/core/audio/LinuxPipeBus.{h,cpp}`) creates virtual
audio devices by shelling out to `pactl load-module module-pipe-source`
and `module-pipe-sink`. It uses the fact that `pactl` is the common
control surface shared by legacy PulseAudio and modern
pipewire-pulse — making it a "lowest common denominator" that works on
both stacks without runtime detection.

Diagnostic run on the affected machine:

| Probe | Result |
|---|---|
| `which pactl` | *(empty — binary not installed)* |
| `pactl info` | `No such file or directory (os error 2)` |
| `systemctl --user status pipewire` | **active (running)** 1.4.7 |
| `systemctl --user status pipewire-pulse` | **active (running)** |
| `ls /run/user/1000/pipewire-0` | socket present |
| `which pw-cli pw-loopback pw-link` | all present in `/usr/bin` |

Ubuntu 25.10 ships `pipewire-pulse` (the shim daemon) *without* making
`pulseaudio-utils` (the package that provides the `pactl` binary) a
hard dependency. Our `QProcess::start("pactl", ...)` errors immediately
with "No such file or directory", which the 5-second `waitForFinished`
reports as a timeout, producing misleading log output.

The LCD-command abstraction we chose at port time is no longer the LCD
on modern distros. A growing fraction of target systems will exhibit
this exact failure mode.

### 1.3 What changes

- Talking to the PipeWire daemon directly — over its Unix socket at
  `$XDG_RUNTIME_DIR/pipewire-0` via `libpipewire-0.3` — becomes the
  primary Linux audio path.
- The existing `pactl` path remains as an explicit fallback for pure
  PulseAudio systems (Ubuntu 20.04 LTS, older RHEL, server installs
  that opted out of PipeWire), used only when `pactl` is actually
  present.
- When neither path is available, the user sees a first-run dialog
  that names what is missing and how to install it, rather than a
  silent failure.
- The speakers output path gains a PipeWire-native variant, so CW
  sidetone and TX MON can get their own sinks with their own
  latency budgets. This makes latency on Linux competitive with
  Windows/macOS native audio and unlocks ham-specific workflows
  (different sinks per RX slice, tight CW sidetone on USB headphones,
  clean voice MON).

### 1.4 Alignment with long-term architecture

Per the slice-based routing vision, audio routing is a property of a
**slice**, not of "RX1" or "RX2". This design uses `SliceModel` as the
unit for output routing and avoids any RX1/RX2 bifurcation. Future
multi-pan / DDC / ADC rework can build on the per-slice routing
primitive introduced here.

---

## 2. Goals, non-goals, success criteria

### 2.1 Goals (in priority order)

1. **Zero-friction Linux audio out-of-the-box** on any PipeWire-based
   distro. User installs the AppImage, launches, opens WSJT-X, picks
   "NereusSDR VAX 1" in its audio picker — done.
2. **Preserve compatibility** with pure-PulseAudio distros (Ubuntu
   20.04 LTS / RHEL 9 / server installs) through the existing
   `pactl` path *if* `pactl` is present.
3. **Never fail silently.** When no backend is available, the user
   gets a modal dialog that names what is missing and how to install
   it, plus Setup page indicators they can check any time.
4. **Expose real telemetry** to power users: measured latency,
   xrun counters, CPU time in the process callback, stream state.
   No cutesy labels hiding technical facts.
5. **Competitive latency** with Windows/macOS native paths: ≤15 ms
   end-to-end at default settings, ≤3 ms reachable for CW/expert ops.
6. **Split output streams** for RX monitor / CW sidetone / TX MON
   so each can get its own sink and latency budget.
7. **Per-slice routing** — each `SliceModel` targets its own sink.

### 2.2 Non-goals (explicit scope guards)

- **No Flatpak / Snap sandboxing** work. Portal integration is a
  separate future design.
- **No PackageKit / pkexec** auto-installation of missing packages.
  Too intrusive for an SDR app.
- **No user-facing backend override** (e.g. "force pactl"). Detection
  is automatic. A hidden `Audio/LinuxBackendPreferred` AppSettings key
  exists for support debugging only.
- **No changes to the `IAudioBus` interface.** Existing contract is
  preserved so macOS/Windows paths are untouched.
- **No speakers-output changes on macOS/Windows.** Those platforms
  keep using `QAudioSink` → WASAPI / CoreAudio respectively.
- **No multi-pan / DDC / ADC rework.** Scope is audio routing only.
- **No JACK-specific code.** PipeWire's JACK compatibility layer
  means our streams appear as JACK ports automatically.

### 2.3 Success criteria

- **On Ubuntu 25.10 (the affected machine)** after the change: all 4
  VAX sources and the TX sink come up clean at startup, WSJT-X sees
  `NereusSDR VAX 1/2/3/4` in its audio picker with no user setup, no
  warnings in the log.
- **On a pactl-only Ubuntu 20.04 VM**: behavior identical to today
  — pactl path still used, same functionality.
- **On a machine with neither backend available**: first-run dialog
  fires once with accurate per-distro remediation; Setup page
  backend strip shows "No audio backend"; "Rescan" button works
  after user installs the missing package.
- **Latency on the affected machine**: Primary output ≤15 ms
  measured with quantum=512 at 48 kHz. CW-sidetone stream reaches
  ≤4 ms measured with quantum=128.
- **All existing unit tests pass.** New tests cover the detection
  matrix, ring buffer correctness, `SliceModel.sinkNodeName`
  persistence, and route dispatch.

---

## 3. Architecture overview

### 3.1 Class structure

```
IAudioBus  (existing interface — src/core/IAudioBus.h, unchanged)
    │
    ├── LinuxPipeBus           (EXISTING — pactl + FIFO. Untouched.
    │                           Now selected only when detector
    │                           returns Pactl.)
    │
    ├── PipeWireBus            (NEW — thin IAudioBus wrapper owning
    │                           one PipeWireStream configured per
    │                           role. One wrapper class, many roles.)
    │         │
    │         └─ uses PipeWireStream   (NEW — single unified class,
    │                                    mode-driven via StreamConfig.
    │                                    All stream shapes live here.)
    │
    └── QAudioSinkAdapter      (NEW — thin IAudioBus wrapper around
                                existing QAudioSink use. Used for the
                                Primary speakers output when backend
                                is Pactl or None. Sidetone/MON are
                                PipeWire-only, disabled on fallback.)

AudioEngine  (existing)
   ├── detectLinuxBackend()        (NEW)
   ├── m_linuxBackend              (NEW — enum, cached)
   ├── m_pipewireThreadLoop        (NEW — owned instance, PipeWire-only)
   └── makeVaxBus(int channel)      (dispatches on m_linuxBackend)

SliceModel  (existing)
   └── Q_PROPERTY(QString sinkNodeName …)   (NEW — per-slice routing)
```

### 3.2 The three PipeWire stream shapes (one class handles all)

| Role | `PW_DIRECTION` | `media.class` | We do |
|---|---|---|---|
| VAX 1–4 (virtual capture device) | `OUTPUT` | `Audio/Source` | Produce; other apps read us as a source |
| TX input (consumer) | `INPUT` | `Stream/Input/Audio` | Consume from a user-picked source |
| Primary / Sidetone / MON (sink producer) | `OUTPUT` | `Stream/Output/Audio` | Produce into a user-picked sink |

The three shapes differ in `StreamConfig` fields and a small amount of
setup code. A single `PipeWireStream` class with a `StreamConfig`
struct covers all three with one state machine, one ring buffer, one
telemetry surface. Each `IAudioBus` concrete wrapper is a ~40-line
file that constructs the right `StreamConfig` and forwards
push/pull through the stream.

### 3.3 Thread loop ownership

Exactly one `pw_thread_loop` lives per process, owned by
`AudioEngine`:

```
AudioEngine
  └── PipeWireThreadLoop m_pwLoop       (started in ctor if backend == PipeWire)
        ├── pw_context   (created on m_pwLoop)
        ├── pw_core      (connected to pipewire-0)
        └── PipeWireStream × N          (each registers streams on m_pwLoop)
```

- Started in `AudioEngine::AudioEngine()` when
  `m_linuxBackend == LinuxAudioBackend::PipeWire`.
- Stopped in `AudioEngine::~AudioEngine()` after all streams are
  closed.
- Every `pw_*` call in `PipeWireStream` is bracketed by
  `pw_thread_loop_lock()` / `_unlock()`.
- DSP thread and GUI thread never touch libpipewire directly — they
  go through the lock-free ring in each `PipeWireStream`.
- All PipeWire callbacks (`on_process`, `on_state_changed`,
  `on_param_changed`, `on_global_added/removed`) run on the
  `pw_thread_loop` internal thread. Cross-thread signalling to Qt
  main thread uses `QMetaObject::invokeMethod` with
  `Qt::QueuedConnection`.

---

## 4. Backend detection

### 4.1 Algorithm

```cpp
enum class LinuxAudioBackend { PipeWire, Pactl, None };

LinuxAudioBackend AudioEngine::detectLinuxBackend()
{
    // Step 1: Honour debug override if set.
    const auto forced = AppSettings::instance().value(
        "Audio/LinuxBackendPreferred", "").toString();
    if (forced == "pipewire") { return LinuxAudioBackend::PipeWire; }
    if (forced == "pactl")    { return LinuxAudioBackend::Pactl; }
    if (forced == "none")     { return LinuxAudioBackend::None; }

    // Step 2: Check $XDG_RUNTIME_DIR reachable.
    const QByteArray xdg = qgetenv("XDG_RUNTIME_DIR");
    if (xdg.isEmpty()) {
        qCWarning(lcAudio) << "XDG_RUNTIME_DIR unset — "
                              "not in a logged-in desktop session";
        // Fall through to pactl probe.
    } else if (QFile::exists(xdg + "/pipewire-0")) {
        // Step 3: Attempt a real PipeWire connect with 500 ms timeout.
        if (probePipeWireConnect(500ms)) {
            return LinuxAudioBackend::PipeWire;
        }
    }

    // Step 4: pactl probe.
    const auto pactlPath = QStandardPaths::findExecutable("pactl");
    if (!pactlPath.isEmpty()) {
        QProcess p;
        p.start(pactlPath, {"--version"});
        if (p.waitForFinished(500) && p.exitCode() == 0) {
            return LinuxAudioBackend::Pactl;
        }
    }

    return LinuxAudioBackend::None;
}
```

`probePipeWireConnect(timeout)` does a throwaway
`pw_context_connect()` against the socket with a hard deadline,
measuring elapsed time and disconnecting cleanly. No side effects.

### 4.2 Cached result and rescan

- Result is computed once in `AudioEngine::AudioEngine()` and stored
  in `m_linuxBackend`.
- `AudioEngine::rescanLinuxBackend()` re-runs detection and re-emits
  `linuxBackendChanged(LinuxAudioBackend old, LinuxAudioBackend new)`.
- The "Rescan" button on the Setup → Audio backend strip calls this
  method. The first-run dialog's "Rescan after install" button also
  calls it.
- If the new result is *different* from the old one, all open
  `IAudioBus` instances are closed and re-created using the new
  backend. Currently-open consumers (WSJT-X etc.) will see a brief
  audio discontinuity — acceptable for a user-triggered rescan.

### 4.3 Detection budget

Total detection time is bounded by **≤1 s** even in the worst case:

- 500 ms PipeWire connect timeout.
- 500 ms pactl version timeout (only reached if PipeWire probe
  failed).

If either daemon is pathologically slow (socket present but unresponsive),
detection returns `None` rather than blocking the UI at startup.

---

## 5. `PipeWireStream` — the single unified class

### 5.1 Public interface

```cpp
class PipeWireStream {
public:
    struct StreamConfig {
        // Required
        QString nodeName;          // e.g. "nereussdr.rx-primary"
        QString nodeDescription;   // e.g. "NereusSDR Primary Output"
        pw_direction direction;    // OUTPUT or INPUT
        QString mediaClass;        // "Audio/Source", "Stream/Output/Audio",
                                   //   or "Stream/Input/Audio"

        // Format (always 48000/stereo/F32LE for now)
        uint32_t rate = 48000;
        uint32_t channels = 2;

        // Latency
        uint32_t quantum = 512;    // 128 / 256 / 512 / 1024 / 2048 / custom

        // Target
        QString targetNodeName;    // empty = follow default; else specific
        QString mediaRole;         // "Music", "Communication", "Phone", ...
    };

    explicit PipeWireStream(pw_thread_loop* loop, pw_core* core,
                            StreamConfig cfg, QObject* parent = nullptr);
    ~PipeWireStream();

    bool open();
    void close();

    // Produce path (OUTPUT direction): caller writes audio here.
    qint64 push(const char* data, qint64 bytes);

    // Consume path (INPUT direction): caller reads audio from here.
    qint64 pull(char* data, qint64 maxBytes);

    // Telemetry (main-thread safe — all backed by atomics).
    struct Telemetry {
        double measuredLatencyMs;
        double ringDepthMs;
        double pwQuantumMs;
        double deviceLatencyMs;
        uint64_t xrunCount;
        double processCbCpuPct;
        QString streamStateName;   // "streaming", "paused", "error", ...
        int consumerCount;         // only meaningful for Audio/Source role
    };
    Telemetry telemetry() const;

signals:
    void streamStateChanged(QString state);
    void telemetryUpdated();      // coalesced at 1 Hz
    void peerConsumerCountChanged(int n);
    void errorOccurred(QString reason);
};
```

### 5.2 State machine

```
       ┌────────────┐
       │  Unopened  │
       └─────┬──────┘
             │ open()
             ▼
       ┌────────────┐   pw_stream_connect() → state PAUSED
       │  Connecting│
       └─────┬──────┘
             │ on_state_changed(STREAMING)
             ▼
       ┌────────────┐   produce: push() → ring, on_process drains.
       │  Streaming │   consume: on_process → ring, pull() drains.
       └─┬────┬─────┘
         │    │
         │    └── error (format reject, peer missing, daemon gone) → Errored
         │
         │ close()
         ▼
       ┌────────────┐
       │   Closed   │
       └────────────┘
```

State changes emit `streamStateChanged(state)` on the main thread.

### 5.3 Ring buffer

One lock-free SPSC ring per stream, sized for ~100 ms of audio
(48 kHz · stereo · F32 → ~38.4 kB of payload; rounded up to the
nearest power-of-two capacity to simplify index masking — 64 kB
in practice, giving ~170 ms headroom). Implementation: a fixed-size
byte ring with `std::atomic<size_t>` read/write indices and
release/acquire ordering on the single producer / single consumer
pairs. Writers never block — on overflow (DSP outrunning PipeWire,
unusual under normal load) the oldest half-quantum is dropped and
the xrun counter increments.

### 5.4 `on_process` body

Produce direction (OUTPUT):

```cpp
void PipeWireStream::on_process_output() {
    pw_buffer* b = pw_stream_dequeue_buffer(m_stream);
    if (!b) { m_xruns.fetch_add(1); return; }

    spa_buffer* sb = b->buffer;
    uint8_t* dst = static_cast<uint8_t*>(sb->datas[0].data);
    const uint32_t dstCapacity = sb->datas[0].maxsize;

    const qint64 popped = m_ring.popInto(dst, dstCapacity);
    if (popped < int64_t(dstCapacity)) {
        // Underfed from DSP — emit silence for the rest of the quantum.
        std::memset(dst + popped, 0, dstCapacity - popped);
    }

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = sizeof(float) * m_cfg.channels;
    sb->datas[0].chunk->size   = dstCapacity;
    pw_stream_queue_buffer(m_stream, b);

    maybeEmitTelemetry();    // 1 Hz coalesced
}
```

Consume direction (INPUT) is the mirror image: dequeue a buffer, push
its contents into the ring, queue it back empty.

### 5.5 Latency measurement

At 1 Hz coalesced from `on_process`:

```cpp
pw_time t;
pw_stream_get_time_n(m_stream, &t, sizeof(t));

// PW_TIME gives us these fields (ns):
//   queued   — bytes queued in our stream
//   buffered — bytes buffered downstream (in pw graph)
//   delay    — device hardware latency
//
// Convert to ms; publish via telemetry().
double ringDepthMs   = m_ring.usedBytes() / bytesPerMs;
double pwQuantumMs   = (t.queued + t.buffered) / 1e6;
double deviceLatMs   = t.delay / 1e6;
double totalMs       = ringDepthMs + pwQuantumMs + deviceLatMs;
```

### 5.6 CPU time in callback

```cpp
timespec t0, t1;
clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
// ... on_process body ...
clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);

const double cbNs = (t1.tv_sec - t0.tv_sec) * 1e9
                  + (t1.tv_nsec - t0.tv_nsec);
const double quantumNs = quantumFrames / rate * 1e9;
m_cpuPct.store(cbNs / quantumNs * 100.0);
```

---

## 6. Wrapper classes (`IAudioBus` implementations)

### 6.1 `PipeWireBus` — one wrapper per role

```cpp
class PipeWireBus : public IAudioBus {
public:
    enum class Role {
        Vax1, Vax2, Vax3, Vax4,
        TxInput,
        Primary, Sidetone, Monitor
    };

    PipeWireBus(Role role, AudioEngine* engine);

    bool open(const AudioFormat& fmt) override;
    qint64 push(const char*, qint64) override;
    qint64 pull(char*, qint64) override;
    float rxLevel() const override;

    PipeWireStream* stream() const { return m_stream.get(); }

private:
    PipeWireStream::StreamConfig configFor(Role r) const;
    std::unique_ptr<PipeWireStream> m_stream;
};
```

`configFor` centralises role → StreamConfig mapping:

| Role | direction | media.class | default node.name | media.role |
|---|---|---|---|---|
| Vax1..4 | OUTPUT | `Audio/Source` | `nereussdr.vax-N` | `Music` |
| TxInput | INPUT | `Stream/Input/Audio` | `nereussdr.tx-input` | `Phone` |
| Primary | OUTPUT | `Stream/Output/Audio` | `nereussdr.rx-primary` | `Music` |
| Sidetone | OUTPUT | `Stream/Output/Audio` | `nereussdr.cw-sidetone` | `Communication` |
| Monitor | OUTPUT | `Stream/Output/Audio` | `nereussdr.tx-monitor` | `Communication` |

Per-slice routing creates additional `PipeWireBus(Role::Primary)`
instances on demand if a slice's `sinkNodeName` targets a node other
than the Primary's target. Cache keyed by `(Role, targetNodeName)`
in `AudioEngine`.

### 6.2 `QAudioSinkAdapter`

```cpp
class QAudioSinkAdapter : public IAudioBus {
public:
    QAudioSinkAdapter(const QAudioDevice& device);
    bool open(const AudioFormat& fmt) override;
    qint64 push(const char*, qint64) override;
    qint64 pull(char*, qint64) override;  // returns 0 — not a consumer
    float rxLevel() const override;

private:
    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<QIODevice>  m_io;
};
```

Minimal wrapper over the existing Qt audio output so `AudioEngine`
can treat the Pactl/None primary speakers path uniformly.

---

## 7. Data flow

### 7.1 Produce path

```
DSP/Audio thread                     pw_thread_loop (libpipewire)
────────────────                     ────────────────────────────
AudioEngine::rxBlockReady(sliceId, samples, N):
  routeKey = slice.sinkNodeName().isEmpty()
             ? "primary" : slice.sinkNodeName();
  bus = m_bus[routeKey];            // cached per-target stream
  bus->push(samples, N * frameBytes)
        │
        ▼
  PipeWireStream::push(data, bytes):
     m_ring.pushCopy(data, bytes)   // lock-free SPSC
                                       on_process_output():
                                           buf = pw_stream_dequeue_buffer()
                                           m_ring.popInto(buf.data)
                                           pw_stream_queue_buffer(buf)
```

### 7.2 Consume path (TX input)

```
pw_thread_loop                                 DSP/Audio thread
──────────────                                 ────────────────
on_process_input():
    buf = pw_stream_dequeue_buffer()
    m_ring.pushCopy(buf.data)
    pw_stream_queue_buffer(buf)
                                                AudioEngine::pullTxAudio(dst, maxBytes):
                                                    bus = m_txBus;
                                                    return bus->pull(dst, maxBytes)
                                                          │
                                                          ▼
                                                    PipeWireStream::pull:
                                                        return m_ring.popInto(dst, maxBytes)
```

### 7.3 Routing decision

The route decision in `rxBlockReady` is hot-path — it runs for every
DSP block. Lookup is a `QHash<QString, PipeWireBus*>` already
materialised; no string allocation per block. When `sinkNodeName`
changes on a slice (user picks a different sink in Setup), the new
bus is created/fetched once and cached.

---

## 8. Error handling and UI wiring

### 8.1 Error taxonomy

| Layer | Failure | User message |
|---|---|---|
| Detection | `pipewire-0` socket missing | "PipeWire socket not found." |
| Detection | Both probes fail | First-run dialog, per-distro remediation. |
| Detection | `$XDG_RUNTIME_DIR` unset | "Not running inside a logged-in desktop session. Launch NereusSDR from your desktop, not from `sudo` or `ssh`." |
| Stream open | Format reject | "PipeWire refused requested format (48000/stereo/F32LE)." |
| Stream open | Target sink missing | Orange badge "target sink `<name>` not found — falling back to default sink". |
| Stream runtime | Peer disappears | Amber badge + `audioStreamLost` toast. Auto-reconnect attempts when target reappears. |
| Stream runtime | xrun spike | Counter ticks in Setup page. One-time log at 10 xruns/min. No popup. |
| rtkit | RT priority refused | Gear-icon warning in telemetry strip. "Add user to `audio` group for quantum < 256." |

### 8.2 First-run dialog

Triggered exactly once — when detection returns `None` and
`AppSettings.value("Audio/LinuxFirstRunSeen", false) == false`:

```
┌─ Linux audio bridge not available ─────────────────────────────┐
│                                                                 │
│  NereusSDR could not connect to a Linux audio daemon.           │
│                                                                 │
│  Detected state:                                                │
│    PipeWire socket:      not found at /run/user/1000/pipewire-0 │
│    pactl binary:         not in $PATH                           │
│                                                                 │
│  To enable VAX and audio output on Linux, install one of:       │
│                                                                 │
│    Ubuntu / Debian:      sudo apt install pipewire pipewire-pulse│
│                          (or) sudo apt install pulseaudio-utils │
│                                                                 │
│    Fedora / RHEL:        sudo dnf install pipewire-pulseaudio   │
│                          (or) sudo dnf install pulseaudio-utils │
│                                                                 │
│    Arch:                 sudo pacman -S pipewire pipewire-pulse │
│                                                                 │
│  NereusSDR will run without audio routing in the meantime.      │
│                                                                 │
│  [ Copy commands ]    [ Rescan after install ]    [ Dismiss ]   │
└─────────────────────────────────────────────────────────────────┘
```

Dialog is also reachable from `Help → Diagnose audio backend` so the
user can bring it up intentionally after install. The copy shown
reflects the current detection state each time it opens, not the
first-run state.

### 8.3 Setup → Audio page structure

```
Setup → Audio
  ├── Backend         ← NEW diagnostic strip (all sub-pages share it)
  ├── Output          ← NEW — Primary / Sidetone / MON / Per-slice routing
  ├── VAX             ← existing AudioVaxPage, cards rebuilt for PipeWire
  ├── Devices         ← existing AudioDevicesPage (unchanged)
  └── Advanced        ← existing AudioAdvancedPage (unchanged)
```

### 8.4 Signal propagation

```
pw callback (pw_thread_loop)   AudioEngine (any)         Main thread (UI)
──────────────────────────     ─────────────────         ────────────────
on_state_changed(STREAMING)
    └ emit streamStateChanged ── QueuedConnection ─→   badge update

on_process() { xrun++ }
    └ 1 Hz telemetry tick ──── QueuedConnection ─→   telemetry block
       latencyMeasured(ms)                              update
       xrunCounted(N)
       cpuUsageMeasured(pct)

on_global(node_added/removed)
    └ emit nodeListChanged ─── QueuedConnection ─→   dropdown refresh
```

All cross-thread hops are `Qt::QueuedConnection`. No mutex held
across the pw-loop-lock boundary. Telemetry signals coalesce to
once per second regardless of callback rate.

---

## 9. UI — concrete layouts

### 9.1 Backend strip (shown on every Audio sub-page)

```
┌─ Audio backend ─────────────────────────────────────────────────┐
│  PipeWire 1.4.7 · socket /run/user/1000/pipewire-0 · RT ok      │
│  Session manager: wireplumber · Default sink: alsa_output.pci...│
│  [ Rescan ] [ Open logs ]                                       │
└─────────────────────────────────────────────────────────────────┘
```

States:

- PipeWire: version + socket + RT status.
- Pactl: "PulseAudio (pactl fallback) · /usr/bin/pactl · server <v>".
- None: "No audio backend — click Details" → opens first-run dialog.

### 9.2 VAX card

```
┌─ VAX 1 ────────────────────────────────────────────  [ • On ] ┐
│                                                                │
│  Exposed to system as:   NereusSDR VAX 1                       │
│  Format:                 48000 Hz · Stereo · Float32           │
│  Consumers:              ● WSJT-X (1 client)                   │
│  Level:                  ▓▓▓▓▓▓░░░░  -18 dB                    │
│                                                                │
│  [ Rename… ]  [ Copy node name ]                               │
└────────────────────────────────────────────────────────────────┘
```

### 9.3 Output page

```
┌─ RX → Primary output ────────────────────────────────────  [ ▣ ]┐
│                                                                 │
│  Target sink:            [ ▼ follow default-sink           ]    │
│                          · current: alsa_output.pci-0000_00...  │
│                                                                 │
│  node.name:              nereussdr.rx-primary                   │
│  node.id:                73      media.role: Music              │
│  rate:                   48000 Hz · F32LE · Stereo              │
│  quantum:                [ 128 ] [ 256 ] [●512] [1024] [custom] │
│                                                                 │
│  ─── live telemetry (1 Hz) ─────────────────────────────────    │
│  measured latency:       11.83 ms  (σ 0.21)                     │
│    · our ring:            2.08 ms                               │
│    · pw quantum:         10.67 ms                               │
│    · device reported:     1.15 ms                               │
│  xruns (since open):     0                                      │
│  process-cb CPU:         0.18 ms / 10.67 ms  (1.7 %)            │
│  stream state:           streaming                              │
│                                                                 │
│  volume:                 [────●──────]  -6.0 dB                 │
│  mute on TX:             [x]                                    │
└─────────────────────────────────────────────────────────────────┘

┌─ CW sidetone → separate sink ────────────────────────────  [ ▣ ]┐
│                                                                 │
│  Target sink:            [ ▼ USB headphones (alsa_output...)]   │
│  node.name:              nereussdr.cw-sidetone                  │
│  quantum:                [●128] [ 256 ] [ 512 ] [1024] [custom] │
│                                                                 │
│  measured latency:       2.94 ms  (σ 0.04)                      │
│  xruns (since open):     0                                      │
│  stream state:           streaming                              │
│                                                                 │
│  ⚠ quantum=128 requires RT scheduling. rtkit: granted.          │
└─────────────────────────────────────────────────────────────────┘

┌─ TX MON → separate sink ─────────────────────────────────  [ ▣ ]┐
│  (same layout; quantum 256 default; role = Communication)       │
└─────────────────────────────────────────────────────────────────┘

┌─ Per-slice routing ─────────────────────────────────────────────┐
│  Slice 1 → Primary output                                       │
│  Slice 2 → [ ▼ follow default-sink                          ]   │
│  (additional rows appear as slices are activated)               │
└─────────────────────────────────────────────────────────────────┘
```

Real PipeWire terminology throughout: `quantum`, `node.name`, `node.id`,
`media.role`, `xruns`, `process-cb CPU`, `stream state`. No cutesy
labels. Power users can tune quantum and see latency/xrun react in
real time.

---

## 10. Persistence

All new AppSettings keys (PascalCase per existing convention):

| Key | Type | Scope | Default |
|---|---|---|---|
| `Audio/LinuxFirstRunSeen` | bool | app | `False` |
| `Audio/LinuxBackendPreferred` | string | app | `""` (auto; debug override only) |
| `Audio/Output/Primary/SinkNodeName` | string | app | `""` (follow default) |
| `Audio/Output/Primary/Quantum` | int | app | `512` |
| `Audio/Output/Sidetone/Enabled` | bool | app | `False` |
| `Audio/Output/Sidetone/SinkNodeName` | string | app | `""` |
| `Audio/Output/Sidetone/Quantum` | int | app | `128` |
| `Audio/Output/Monitor/Enabled` | bool | app | `False` |
| `Audio/Output/Monitor/SinkNodeName` | string | app | `""` |
| `Audio/Output/Monitor/Quantum` | int | app | `256` |
| `Audio/Vax1/NodeDescription` | string | app | `NereusSDR VAX 1` |
| `Audio/Vax2/NodeDescription` | string | app | `NereusSDR VAX 2` |
| `Audio/Vax3/NodeDescription` | string | app | `NereusSDR VAX 3` |
| `Audio/Vax4/NodeDescription` | string | app | `NereusSDR VAX 4` |
| `Audio/Tx/SourceNodeName` | string | app | `""` (default mic) |
| `Slice<N>/SinkNodeName` | string | per-slice | `""` (Primary) |

No schema migration needed — all new keys. Existing pactl-era keys
remain untouched. On upgrade, an existing user on a PipeWire host
transparently moves to the PipeWire path (better latency, more
features) with their VAX assignments intact.

---

## 11. Build and dependencies

### 11.1 New build dependency

- `libpipewire-0.3-dev` (build-time header) — Debian/Ubuntu package name.
- `libpipewire-0.3-0` (runtime shared library) — present on every
  PipeWire distro by definition.
- Minimum version: **0.3.50**.

Fedora/RHEL package name: `pipewire-devel`. Arch:
`pipewire` (headers included with the main package).

### 11.2 CMake wiring

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE libpipewire-0.3>=0.3.50)

if(PIPEWIRE_FOUND)
    target_link_libraries(NereusSDR PRIVATE ${PIPEWIRE_LIBRARIES})
    target_include_directories(NereusSDR PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    target_compile_options(NereusSDR PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
    target_compile_definitions(NereusSDR PRIVATE NEREUS_HAVE_PIPEWIRE)
    message(STATUS "PipeWire ${PIPEWIRE_VERSION} — native Linux audio enabled")
else()
    message(STATUS "libpipewire-0.3 not found — Linux audio limited to pactl path")
endif()
```

- Builds without libpipewire-0.3-dev still compile and run — they
  just behave as if detection always returned Pactl or None.
- All PipeWire C++ code lives behind `#ifdef NEREUS_HAVE_PIPEWIRE`
  so macOS/Windows builds are unaffected.
- The AppImage CI workflow has libpipewire-0.3-dev installed, so
  every release AppImage is PipeWire-capable.

### 11.3 Ubuntu 22.04 LTS footnote

Ubuntu 22.04 ships PipeWire 0.3.48. Two paths:

1. Use the `jammy-updates` repo (has 0.3.58+).
2. Accept that Ubuntu 22.04 runs on the pactl fallback for users
   who haven't updated.

We document option 2 and don't pin a hard minimum that locks out
a major LTS. PipeWire-side code is gated by a runtime version check
if we need features newer than 0.3.50.

---

## 12. Testing strategy

### 12.1 Unit tests (run in existing CI)

| Test file | What it covers | Deps |
|---|---|---|
| `tst_linux_backend_detection` | Mocked `pw_context_connect` + mocked `findExecutable("pactl")` → correct `LinuxAudioBackend` enum | None — injection seam |
| `tst_pipewire_stream_config` | `StreamConfig` → `pw_properties` / `spa_pod` round-trip | spa headers only, no daemon |
| `tst_audio_ring_spsc` | Lock-free SPSC ring: push/pop correctness under thread race, overflow drops oldest, no leak | None |
| `tst_slice_sink_routing` | `SliceModel::sinkNodeName` property set/get/emit + persistence round-trip | Existing SliceModel test fixture |
| `tst_audio_engine_route_dispatch` | `rxBlockReady` picks the right stream based on slice's `sinkNodeName` | Mocked `IAudioBus` |

### 12.2 Integration tests (gated by env)

| Test file | What it covers | Gate |
|---|---|---|
| `tst_pipewire_stream_integration` | Real `pw_stream_connect` against a running daemon, state machine reaches STREAMING, xrun counter works, shutdown clean | Skipped unless `$XDG_RUNTIME_DIR/pipewire-0` reachable |
| `tst_linux_audio_detection_integration` | Detection against real daemon (returns PipeWire), then with socket `chmod 000` (returns None with specific error) | Same gate |

### 12.3 CI additions

- New job: **Linux + pipewire daemon**. Uses a Debian-12 runner image
  with `pipewire` + `wireplumber` installed; boots them as user
  services pre-test; runs the integration tests above; tears down.
- Standard unit jobs continue to run on every commit without the
  extra boot — unit tests need no daemon.

### 12.4 Manual verification matrix

Lives in `docs/architecture/linux-audio-verification/README.md`
matching the 3G-8 pattern. Matrix rows:

| Distro | PipeWire | pactl | Expected detection | Scenarios |
|---|---|---|---|---|
| Ubuntu 25.10 (bare metal) | ✓ 1.4.7 | ✗ | PipeWire | VAX → WSJT-X, sidetone on USB headphones, per-slice routing, reconnect on device replug |
| Ubuntu 22.04 LTS | 0.3.48 (under 0.3.50 floor) | ✓ | Pactl | VAX → WSJT-X, single speakers sink |
| Ubuntu 20.04 LTS (VM) | ✗ | ✓ | Pactl | VAX → WSJT-X, single speakers sink |
| Fedora 41 | ✓ | maybe ✓ | PipeWire | Full scenarios |
| Debian 12 | ✓ 1.0.5 | ✗ | PipeWire | Full scenarios |
| Arch current | ✓ | ✓ | PipeWire | Full scenarios |
| Container, no audio | ✗ | ✗ | None + first-run dialog | Dismiss, Rescan after install, first-run persist flag |

Each manually-verified scenario documents the measured latency, the
xrun count after a 10-minute soak, and a screenshot of the telemetry
strip.

### 12.5 ASAN/UBSAN build

PipeWire C API is unsafe-by-default for the unwary. New CI job
runs an ASAN+UBSAN build of the integration tests. A single clean
run on the shakedown machine is a release gate.

---

## 13. Rollout

- **No feature flag.** Primary path is auto-detected at startup; safe
  fallback exists for every failure mode. A flag would just mean more
  paths to maintain.
- **One PR.** Not split by sub-phase. Shipping a half-migrated state is
  worse than shipping the whole migration.
- **Branch:** `ubuntu-dev` → merged to `main` after manual verification
  passes on at least the three rows exercisable locally (Ubuntu 25.10,
  one LTS VM, one container).
- **Version bump:** feature-level change; candidate for 0.2.0 at next
  release.
- **No breaking config changes.** Existing AppSettings keys untouched.
- **Changelog entry:** "Linux: native PipeWire audio bridge with live
  latency telemetry, separate sidetone/MON sinks, per-slice output
  routing. Falls back to pactl/PulseAudio where PipeWire isn't
  present."
- **Documentation updates:**
  - `CLAUDE.md` Build section: new libpipewire-0.3-dev dependency.
  - `docs/architecture/overview.md` audio layer: two-backend Linux
    model.
  - `docs/architecture/2026-04-23-linux-audio-pipewire-plan.md` —
    the implementation plan (writing-plans skill produces this next).
  - `docs/architecture/linux-audio-verification/README.md` — manual
    verification matrix.
  - `CHANGELOG.md` — the entry above.
  - In-app `Help → Diagnose audio backend` menu item.

---

## 14. Risks and mitigations

| Risk | Mitigation | Residual |
|---|---|---|
| libpipewire ABI break within 0.3.x | Pin ≥0.3.50; 0.3 series ABI is stable by policy | Low |
| Ubuntu 22.04 users on 0.3.48 get pactl fallback | Documented known limitation; functional audio preserved | Accepted |
| `pw_thread_loop` threading bug in our code | Unit tests + integration tests + ASAN build in CI + shakedown on affected machine | Medium until shakedown |
| Per-slice routing confusing for single-slice users | Only shown in Setup Advanced view; RxApplet dropdown appears only when >1 slice is active | Low |
| Latency telemetry misleading on virtualised hosts | Footnote in Setup Output panel: "measured values include guest kernel scheduling overhead" | Accepted |
| User with JACK already running | PipeWire-JACK compatibility layer handles it transparently; our nodes appear as JACK ports too | Low |
| First-run dialog copy drifts from reality | Copy is generated from detection result, not static text | Low |
| rtkit refuses RT priority | Gracefully degrades to SCHED_OTHER; telemetry surfaces the fact | Accepted |

---

## 15. Open questions / future work

- **Flatpak portal** integration (`org.freedesktop.portal.Audio`,
  `.Pipewire`) for a future sandboxed distribution. Out of scope here.
- **AppStream metadata** (`.appdata.xml`) so NereusSDR appears in
  GNOME Software / KDE Discover for hosts that also install a
  `.desktop` file. Separate polish.
- **Single-instance enforcement** on Linux so double-launches don't
  create node-name collisions. Separate.
- **MPRIS** media-key integration. Low value for an SDR app; skip
  unless requested.
- **Multi-pan / DDC / ADC rework** — long-term vision where all
  routing is per-slice. This design's per-slice sink routing is
  a piece of that, but the full rework is a separate, much larger
  design.
- **Speaker-path latency probing** — we could auto-pick the largest
  quantum that sustains zero xruns for 60 seconds as the default on
  first launch. Adaptive behaviour, ship later if the fixed defaults
  prove inadequate.
- **Headset auto-switch semantics** — PipeWire-native paths follow
  the default sink via `media.role`. Confirm behaviour matches what
  hams on Bluetooth headsets expect during the shakedown.

---

## 16. References

- PipeWire API documentation — https://docs.pipewire.org/page_api.html
- `pw_stream` examples — `pipewire/test/examples/` in the PipeWire tree
- AetherSDR CoreAudioHalBus (macOS HAL plugin pattern inspiration)
- Existing NereusSDR `LinuxPipeBus` (`src/core/audio/LinuxPipeBus.{h,cpp}`)
- Existing NereusSDR `IAudioBus` (`src/core/IAudioBus.h`)
- NereusSDR slice-routing vision
  (`memory/project_nereussdr_slice_routing_vision.md`)
- Thetis audio.cs — upstream audio-engine reference
- `/home/skyrunner/.config/NereusSDR/NereusSDR.settings` — AppSettings
  XML, new keys land here on first save
