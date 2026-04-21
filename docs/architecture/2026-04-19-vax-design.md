# VAX — Virtual Audio eXchange Design Spec

**Status:** Design approved (brainstorm 2026-04-19), ready for implementation plan
**Author:** J.J. Boyd (KG4VCF), AI-assisted (Claude Opus 4.7)
**Target phase:** Phase 3O — Audio Routing & Cross-Platform Audio Engine
**Supersedes:** n/a (new subsystem)
**Reserves integration points for:** Phase 3J (TCI + Spots), future native Windows driver work

---

## 1. Purpose

Ship NereusSDR's complete RX audio routing story in a single, reviewable phase. Today NereusSDR can play RX audio through the system default output and nothing else. After this phase, each receiver can be routed independently to speakers and to any of four virtual audio channels ("VAX 1–4"), using native drivers on macOS and Linux and user-installed virtual cables on Windows — all wired through a single cross-platform audio engine with Thetis-grade power-user controls.

The design follows **AetherSDR's mental model** (assign VAX at the source, view levels at the destination) rather than a patchbay or matrix, because it fits ham workflows, matches NereusSDR's existing applet architecture, and minimizes new UI paradigms.

## 2. Scope

**In scope:**

- **Cross-platform audio engine** based on PortAudio (MME / DirectSound / WDM-KS / WASAPI-shared / WASAPI-exclusive / ASIO on Windows; CoreAudio on macOS; ALSA / PulseAudio / PipeWire / JACK on Linux).
- **VAX architecture** — four virtual audio channels + one TX input:
  - macOS: native CoreAudio HAL plugin (`libASPL` based), ported from AetherSDR.
  - Linux: native PulseAudio `module-pipe-source` + `module-pipe-sink` bridge, ported from AetherSDR.
  - Windows: user-installed virtual cables (VB-CABLE, VAC, Voicemeeter) with **first-run auto-detection** and graceful install prompts.
- **Routing model** — per-receiver VAX assignment on the VFO flag; speakers are always on unless muted; a single VAX slot holds TX at a time.
- **UI additions** —
  - `VaxApplet` (ported from AetherSDR `DaxApplet`, renamed) docked in the right container panel.
  - `VfoWidget` gains a VAX selector row (OFF / 1 / 2 / 3 / 4).
  - Menu-bar master output widget (global volume + mute + device shortcut).
  - First-run auto-detect dialog (three platform variants).
  - Setup → Audio page with **Devices**, **VAX**, **Advanced** sub-tabs.
- **Optional Direct ASIO engine** (Windows only) for Thetis `cmASIO` parity, gated behind an "Advanced" radio button inside the Devices tab.
- **AppSettings persistence** for every device / slot / gain / device-bind.
- **Full end-to-end wiring** — zero unwired controls, zero dangling signals, zero TODO-for-future setters.
- **Attribution compliance** per `docs/attribution/HOW-TO-PORT.md` for every ported file.

**Explicitly out of scope (reserved integration points):**

- **TCI server and TCI audio streams** — deferred to Phase 3J. The `IAudioBus` tap contract in this spec is designed to let TCI subscribe to the same taps without refactor.
- **NereusSDR-owned Windows virtual audio driver** — deferred as potential future work. Design hooks are preserved so a signed driver can replace the BYO path without breaking user-visible bindings.
- **TX mic DSP chain** (compressor, TX EQ) — lives in Phase 3M. This spec only wires the TX **routing** (mic input device → TXA), not DSP between them.
- **IQ streaming to VAX** — Thetis has an "IQ to VAC" toggle. Wired but feature-flagged off; implementation deferred.

---

## 3. Architecture Overview

### 3.1 Signal flow

```
       RX1 DSP ─┐
       RX2 DSP ─┤
       RX3 DSP ─┼──┬─► Master mix ──► QAudioSink (Speakers)
       RX4 DSP ─┤  │                   └── volume, mute, device
       ...     ─┘  │
                   ├─► VAX 1 tap ──► IAudioBus::push("vax-1")
                   ├─► VAX 2 tap ──► IAudioBus::push("vax-2")
                   ├─► VAX 3 tap ──► IAudioBus::push("vax-3")
                   └─► VAX 4 tap ──► IAudioBus::push("vax-4")
                                           │
                       ┌───────────────────┼───────────────────┐
                       ▼                   ▼                   ▼
                 macOS HAL plugin    Linux Pulse bridge    Windows BYO
                 (POSIX shm ring)    (named FIFO)          (PortAudio sink
                                                           to OS device)

       TX path:
       QAudioSource ──► IAudioBus::pull("tx-in") ──► TXA DSP ──► Radio
       (user-picked OS capture device; on Mac/Linux, includes "NereusSDR TX")
```

### 3.2 `IAudioBus` abstraction

The same internal interface, three platform backends. Declared in `src/core/IAudioBus.h`.

```cpp
namespace NereusSDR {

struct AudioFormat {
    int sampleRate = 48000;      // Hz
    int channels = 2;            // 1 or 2
    enum class Sample { Float32, Int16, Int24, Int32 } sample = Sample::Float32;
};

class IAudioBus {
public:
    virtual ~IAudioBus() = default;

    // Lifecycle
    virtual bool open(const AudioFormat& format) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Producer side (RX taps): write a block of interleaved PCM.
    virtual qint64 push(const char* data, qint64 bytes) = 0;

    // Consumer side (TX): read a block of interleaved PCM.
    virtual qint64 pull(char* data, qint64 maxBytes) = 0;

    // Metering (RMS of last block; published atomically for UI).
    virtual float rxLevel() const = 0;  // 0.0–1.0
    virtual float txLevel() const = 0;

    // Diagnostics
    virtual QString backendName() const = 0;
    virtual AudioFormat negotiatedFormat() const = 0;
};

} // namespace NereusSDR
```

Five concrete implementations in `src/core/audio/`:

| Class | Platform | Backend | Source |
|---|---|---|---|
| `CoreAudioHalBus` | macOS | POSIX shm ring to HAL plugin | Port of AetherSDR `VirtualAudioBridge` |
| `LinuxPipeBus` | Linux | Named FIFO via `module-pipe-source`/`sink` | Port of AetherSDR `PipeWireAudioBridge` |
| `PortAudioBus` | Windows (default) | PortAudio stream to user-picked device | New |
| `DirectAsioBus` | Windows (opt-in) | ASIO SDK direct, cmASIO parity | New (Thetis `cmasio.c` port) |
| `CoreAudioBus` | macOS / fallback | PortAudio CoreAudio for Speakers/Headphones/TX | New |

`AudioEngine` owns one instance per routable endpoint (Speakers, Headphones, TX, VAX 1–4) and dispatches by slot.

### 3.3 Threading

- **Main thread:** `RadioModel`, `SliceModel`, UI widgets, applet parameter updates. Never blocks, never touches PCM buffers directly.
- **Audio worker thread** (existing, owned by `AudioEngine`): RX tap plumbing, per-VAX `IAudioBus::push`, TX `pull`, level metering. All parameter reads are `std::atomic`; never holds a mutex in the audio callback (per CLAUDE.md).
- **No new threads introduced.** `CoreAudioHalBus` and `LinuxPipeBus` perform their I/O on the existing audio worker thread. The macOS HAL plugin runs in `coreaudiod`'s own thread (out-of-process); the Linux pipe FIFOs are read by PulseAudio/PipeWire in their own processes. No IPC blocks the NereusSDR audio thread.

### 3.4 Routing resolution

Each `SliceModel` carries a `vaxChannel` int: `0=Off, 1..4=VAX 1–4`. `AudioEngine::rxBlockReady(sliceId, float* samples, int frames)` executes:

```cpp
// Always push to the master mix (speakers path), respecting per-slice mute/volume.
if (!slice.audioMuted()) {
    m_masterMix.accumulate(slice, samples, frames);
}

// Tap to the slice's selected VAX (if any).
const int vaxCh = slice.vaxChannel();
if (vaxCh >= 1 && vaxCh <= 4) {
    m_vaxBus[vaxCh - 1]->push(samples, frames * sizeof(float) * 2);
}
```

No per-cell enable matrix. No wires to cross. Two reads per block: mute and VAX channel. Both atomic.

### 3.5 TX arbitration

- `TransmitModel::txOwnerSlot()` returns `VaxSlot { None, 1, 2, 3, 4, MicDirect }`.
- Only one slot feeds TX at a time. Switch is instantaneous on user change (no cross-fade).
- PTT sources: physical PTT, VOX, UI button. PTT assertion is orthogonal to owner selection — you can hold PTT while any owner is selected.
- When the owner is `VAX N`, TX pulls from `m_vaxBus[N-1]` for `pull()`. When `MicDirect`, TX pulls from the OS capture device configured as the "TX input device".

---

## 4. File Inventory and Attribution

### 4.1 Ported files (from AetherSDR)

Per `docs/attribution/HOW-TO-PORT.md` rule 6: AetherSDR has no per-file GPL headers. Each ported file gets a NereusSDR port-citation block referencing the AetherSDR project URL and primary author, followed by the standard modification-history block.

| NereusSDR path | Ported from | LOC (reference) | Rename notes |
|---|---|---|---|
| `src/core/audio/CoreAudioHalBus.{h,cpp}` | `AetherSDR src/core/VirtualAudioBridge.{h,cpp}` | 95 + 336 | Rebrand all `AetherSDR` → `NereusSDR`, shm paths `/aethersdr-dax-*` → `/nereussdr-vax-*`, class rename. |
| `src/core/audio/LinuxPipeBus.{h,cpp}` | `AetherSDR src/core/PipeWireAudioBridge.{h,cpp}` | ~80 + 386 | Rebrand class, FIFO paths `/tmp/aethersdr-dax-*` → `/tmp/nereussdr-vax-*`, PulseAudio module `source_name=nereussdr-vax-1`, description "NereusSDR VAX 1". |
| `hal-plugin/NereusSDRVAX.cpp` | `AetherSDR hal-plugin/AetherSDRDAX.cpp` | ~330 | Rebrand device UIDs (`com.aethersdr.dax.*` → `com.nereussdr.vax.*`), device names, shm paths. **Fix macOS 14.4+ regression:** use `killall coreaudiod` fallback in postinstall script. |
| `hal-plugin/CMakeLists.txt`, `Info.plist` | AetherSDR equivalents | small | Bundle ID `com.nereussdr.vax`, factory UUID regenerated. |
| `packaging/macos/hal-installer.sh` + postinstall | AetherSDR `packaging/macos/build-installer.sh` + postinstall | small | Rebrand + killall fallback. |
| `src/gui/applets/VaxApplet.{h,cpp}` | `AetherSDR src/gui/DaxApplet.{h,cpp}` | 48 + 216 | **Rename class `DaxApplet` → `VaxApplet`, all signal names `dax*` → `vax*`. Integrate with NereusSDR `ContainerWidget`/applet framework (not AetherSDR's applet parent).** Ported styling merged with NereusSDR `STYLEGUIDE.md`. |
| `src/gui/widgets/MeterSlider.{h,cpp}` | `AetherSDR src/gui/MeterSlider.{h,cpp}` | 129 + 2 | Shared meter+slider composite widget used by VaxApplet. AetherSDR's `.cpp` is nearly empty (logic is header-inline); port faithfully keeping that shape. |

**Port-citation block template for each ported file:**

```cpp
// =================================================================
// <repo-relative path>  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   <aethersdr path>
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Renamed DAX → VAX and
//                 rebranded identifiers for the NereusSDR-native VAX
//                 device family.
// =================================================================
```

### 4.2 New files (NereusSDR-original)

| Path | Role |
|---|---|
| `src/core/IAudioBus.h` | Abstract bus interface (cross-backend contract). |
| `src/core/audio/PortAudioBus.{h,cpp}` | PortAudio stream to user-picked OS audio device. Windows default + macOS/Linux fallback. |
| `src/core/audio/DirectAsioBus.{h,cpp}` | Native ASIO SDK engine (Windows only, opt-in). Ported logic from Thetis `ChannelMaster/cmasio.{h,c}` — **adds Thetis attribution headers per HOW-TO-PORT.md**. |
| `src/core/audio/AsioSdkHost.{h,cpp}` | Thin wrapper over the ASIO SDK. Driver enumeration, load, callback routing. |
| `src/core/audio/VirtualCableDetector.{h,cpp}` | Enumerates OS audio devices and regex-matches known virtual-cable product patterns (VB-Audio family, VAC, Voicemeeter, Dante, FlexRadio DAX). |
| `src/core/audio/MasterMixer.{h,cpp}` | Per-slice mute/volume/pan → stereo mix → single output sink. |
| `src/gui/MasterOutputWidget.{h,cpp}` | Menu-bar corner widget: speaker icon, master slider, mute button, right-click device picker. |
| `src/gui/SetupAudioPage.{h,cpp}` | Setup → Audio page with Devices/VAX/Advanced sub-tabs. Extends existing `SetupPage` base. |
| `src/gui/VaxFirstRunDialog.{h,cpp}` | First-run auto-detect modal (five scenarios A–E from the brainstorm mockups). |
| `src/gui/VaxChannelSelector.{h,cpp}` | Small 5-button group widget embedded into `VfoWidget` (OFF / 1 / 2 / 3 / 4). |

### 4.3 Modified existing files

| Path | Change |
|---|---|
| `src/models/SliceModel.{h,cpp}` | Add `int vaxChannel()` / `setVaxChannel(int)` with `std::atomic<int>` storage, `vaxChannelChanged(int)` signal, AppSettings key `Slice<id>/VaxChannel`. |
| `src/models/TransmitModel.{h,cpp}` | Add `VaxSlot txOwnerSlot()` / `setTxOwnerSlot(VaxSlot)`, signal, AppSettings key `tx/OwnerSlot`. |
| `src/gui/VfoWidget.{h,cpp}` | Embed `VaxChannelSelector` in a new row below the MODE/FILT/AGC tabs. Bidirectional wire to `SliceModel::vaxChannel`. |
| `src/core/AudioEngine.{h,cpp}` | Replace `QAudioSink`-only output with the `IAudioBus`-based model: master-mix goes to Speakers/Headphones bus; per-slice VAX taps feed the four VAX buses; TX consumes either a VAX bus or the mic bus based on `TransmitModel::txOwnerSlot`. |
| `src/core/AppSettings.{h,cpp}` | Schema additions (below). Add a `v2` settings-migration path to import old single-device audio config. |
| `src/gui/MainWindow.{h,cpp}` | Instantiate `MasterOutputWidget` in the menu bar, wire it to `AudioEngine`. Trigger `VaxFirstRunDialog` on first launch / on new-cable detection. |
| `src/gui/SetupDialog.{h,cpp}` | Register `SetupAudioPage` under the Audio category. |
| `docs/attribution/THETIS-PROVENANCE.md` | Add row for `DirectAsioBus` + `AsioSdkHost` (ported from `ChannelMaster/cmasio.{h,c}` + `Console/clsCMASIOConfig.cs`). |
| `docs/attribution/aethersdr-contributor-index.md` (or equivalent) | Add rows for `VirtualAudioBridge`, `PipeWireAudioBridge`, `DaxApplet`, `MeterSlider`. |
| `docs/MASTER-PLAN.md` | Add Phase 3O section, update status lines, add Plan Review History entry. |
| `CMakeLists.txt` | Add PortAudio dependency (vendored or `find_package`), conditionally add ASIO SDK + HAL plugin subdirs. |
| `README.md` | Add brief "VAX routing" mention in feature list; link to user docs. |
| `resources/help/install-virtual-cables.md` | New user-facing help doc. |

---

## 5. Data Model Changes

### 5.1 `SliceModel`

```cpp
// New:
std::atomic<int> m_vaxChannel{0};  // 0=Off, 1..4=VAX N
public:
    int vaxChannel() const { return m_vaxChannel.load(); }
    void setVaxChannel(int ch);   // validates 0..4, persists, emits
signals:
    void vaxChannelChanged(int ch);
```

Persistence: `AppSettings` XML key `Slice<sliceId>/VaxChannel`, stored as string `"0".."4"`. On load, invalid values clamp to 0.

### 5.2 `TransmitModel`

```cpp
enum class VaxSlot { None = 0, MicDirect, Vax1, Vax2, Vax3, Vax4 };
std::atomic<VaxSlot> m_txOwnerSlot{VaxSlot::MicDirect};
public:
    VaxSlot txOwnerSlot() const { return m_txOwnerSlot.load(); }
    void setTxOwnerSlot(VaxSlot s);
signals:
    void txOwnerSlotChanged(VaxSlot s);
```

Persistence: `tx/OwnerSlot` stored as string `"MicDirect"` etc.

### 5.3 `AudioEngine`

```cpp
// New members:
std::unique_ptr<IAudioBus> m_speakersBus;
std::unique_ptr<IAudioBus> m_headphonesBus;       // optional, may be null
std::unique_ptr<IAudioBus> m_txInputBus;          // OS capture device
std::array<std::unique_ptr<IAudioBus>, 4> m_vaxBus;
MasterMixer m_masterMix;

// Public API additions:
void setSpeakersConfig(const AudioDeviceConfig&);
void setHeadphonesConfig(const AudioDeviceConfig&);
void setTxInputConfig(const AudioDeviceConfig&);
void setVaxConfig(int ch, const AudioDeviceConfig&);

// AudioDeviceConfig carries: driver API, device name, sample rate, bit depth,
// channels, buffer size, exclusive mode, ASIO engine choice, etc.
```

### 5.4 `AppSettings` schema additions

All keys PascalCase, boolean as `"True"`/`"False"`, per CLAUDE.md.

```
audio/Speakers/DriverApi            (string: MME|DirectSound|WDM-KS|WASAPI|WASAPI-Exclusive|ASIO|CoreAudio|ALSA|Pulse|PipeWire|JACK)
audio/Speakers/DeviceName           (string: device display name)
audio/Speakers/SampleRate           (int: Hz)
audio/Speakers/BitDepth             (int: 16|24|32)
audio/Speakers/Channels             (int: 1|2)
audio/Speakers/BufferSamples        (int)
audio/Speakers/ExclusiveMode        (bool)
audio/Speakers/EventDrivenCallback  (bool)
audio/Speakers/BypassMixer          (bool)
audio/Speakers/ManualLatencyMs      (int, 0=auto)
audio/Speakers/AsioEngine           (string: PortAudio|Direct)
audio/Speakers/DirectAsio/DriverName       (string — only if AsioEngine=Direct)
audio/Speakers/DirectAsio/BlockSize        (int)
audio/Speakers/DirectAsio/LockMode         (bool)
audio/Speakers/DirectAsio/InputMode        (string: Left|Right|Both)
audio/Speakers/DirectAsio/BaseInChannel    (int, 0-indexed)
audio/Speakers/DirectAsio/BaseOutChannel   (int, 0-indexed)

audio/Headphones/*                  (same keys; optional, may be unset)
audio/TxInput/*                     (same keys)

audio/Vax1/Enabled                  (bool)
audio/Vax1/DeviceName               (string — "NereusSDR VAX 1" on Mac/Linux; "CABLE-A Input" etc. on Windows)
audio/Vax1/DriverApi                (string, only on Windows BYO path)
audio/Vax1/SampleRate               (int)
audio/Vax1/BitDepth                 (int)
audio/Vax1/Channels                 (int)
audio/Vax1/BufferSamples            (int)
audio/Vax1/RxGain                   (float: 0.0–1.0)
audio/Vax1/Muted                    (bool)
audio/Vax1/ExclusiveMode            (bool, Windows only)
audio/Vax2/*                        (same)
audio/Vax3/*                        (same)
audio/Vax4/*                        (same)

audio/TxGain                        (float: 0.0–1.0)

audio/Master/Volume                 (float: 0.0–1.0)
audio/Master/Muted                  (bool)
audio/Master/ActiveOutput           (string: Speakers|Headphones)

audio/DspRate                       (int: WDSP internal rate, default 48000)
audio/DspBlockSize                  (int: fexchange2 granularity, default 1024)

audio/VacFeedback/<ch>/Gain         (float, IVAC parity)
audio/VacFeedback/<ch>/SlewTimeMs   (float)
audio/VacFeedback/<ch>/PropRingMinUs   (int)
audio/VacFeedback/<ch>/PropRingMaxUs   (int)
audio/VacFeedback/<ch>/FfRingMinUs     (int)
audio/VacFeedback/<ch>/FfRingMaxUs     (int)
audio/VacFeedback/<ch>/FfRingAlpha     (float)

audio/SendIqToVax                   (bool, default False)
audio/TxMonitorToVax                (bool, default False)
audio/MuteVaxDuringTxOnOtherSlice   (bool, default True)

Slice<sliceId>/VaxChannel          (int: 0..4)
tx/OwnerSlot                        (string: None|MicDirect|Vax1..4)

audio/FirstRunComplete              (bool)
audio/LastDetectedCables            (string: CSV of device-name hashes for diff-on-next-run)
```

### 5.5 Settings migration

Pre-VAX builds stored a single `audio/OutputDevice` key (per existing `AudioEngine`). Migration on first launch post-upgrade:

```cpp
if (settings.contains("audio/OutputDevice") && !settings.contains("audio/Speakers/DeviceName")) {
    settings.setValue("audio/Speakers/DeviceName", settings.value("audio/OutputDevice").toString());
    settings.setValue("audio/Speakers/DriverApi", platformDefaultApi());
    settings.setValue("audio/Speakers/SampleRate", 48000);
    // … sensible defaults for the rest
    settings.setValue("audio/FirstRunComplete", "False");  // trigger VaxFirstRunDialog
    settings.remove("audio/OutputDevice");
    settings.save();
}
```

---

## 6. Control Wiring Table

**Every** widget listed below has its full round-trip wiring specified. No "TBD", no "future PR", no unwired stubs. A row lacking a column means that column is N/A for that control.

Columns: **Widget** · **Widget → Model** · **Model → Widget** (echo prevention) · **Model → Action** · **AppSettings key** · **Guard**

### 6.1 VFO flag — VAX channel selector

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| `VaxChannelSelector` button group (OFF/1/2/3/4) | `buttonClicked(int)` → `SliceModel::setVaxChannel(int)` | `vaxChannelChanged(int)` → `VaxChannelSelector::setValue(int)` | `SliceModel::setVaxChannel` persists and (if different) emits `vaxChannelChanged`; `AudioEngine::sliceVaxChanged(sliceId, ch)` slot updates the tap routing | `Slice<sliceId>/VaxChannel` | `QSignalBlocker` on `VaxChannelSelector` during `setValue` |

### 6.2 VFO flag — per-RX audio block (already existed, extended)

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| Volume slider | `valueChanged(int)` → `SliceModel::setAudioVolume(float)` | `audioVolumeChanged` → `slider.setValue` | `AudioEngine::sliceVolumeChanged` updates `MasterMixer` weight for that slice | `Slice<sliceId>/AudioVolume` | `m_updatingFromModel` |
| Pan slider | `valueChanged(int)` → `SliceModel::setAudioPan(float)` | `audioPanChanged` → `slider.setValue` | `MasterMixer` per-slice pan coefficient | `Slice<sliceId>/AudioPan` | same |
| MUTE button | `toggled(bool)` → `SliceModel::setAudioMuted(bool)` | `audioMutedChanged` → `button.setChecked` | Master mix skips slice block when muted; VAX tap still runs (optional per-VAX mute owns that) | `Slice<sliceId>/AudioMuted` | same |
| BINAURAL toggle | `toggled(bool)` → `SliceModel::setBinauralEnabled(bool)` | `binauralChanged` → button | `RxChannel::setBinaural` calls WDSP `SetRXAPanelBinaural` | `Slice<sliceId>/Binaural` | same |
| SPEAKERS/HDPHN buttons | exclusive group → `SliceModel::setOutputRoute(Output)` | `outputRouteChanged` → button group | `MasterMixer` routes the slice to speakers OR headphones bus | `Slice<sliceId>/OutputRoute` | same |
| RX level meter (readonly) | n/a | `AudioEngine::sliceMeter(sliceId, float)` → `meter.setLevel(float)` | n/a | none (ephemeral) | n/a |

### 6.3 Menu-bar `MasterOutputWidget`

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| Master volume slider | `valueChanged(int)` → `AudioEngine::setMasterVolume(float)` | `masterVolumeChanged(float)` → `slider.setValue` | Applies gain on the speakers bus | `audio/Master/Volume` | `m_updatingFromModel` |
| Master MUTE button | `toggled(bool)` → `AudioEngine::setMasterMuted(bool)` | `masterMutedChanged(bool)` → button | Mutes speakers bus | `audio/Master/Muted` | same |
| Speaker icon (click) | `clicked()` → toggles mute | same | same | same | n/a |
| Speaker icon (right-click) | context menu with `QActionGroup` of output devices → `AudioEngine::setSpeakersDevice(QString)` | device change re-enumerates the menu next open | Rebuilds `m_speakersBus` | `audio/Speakers/DeviceName` | n/a |

### 6.4 `VaxApplet` (ported from AetherSDR `DaxApplet`, renamed)

For each of the four channels:

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| `VaxChannelStrip[ch].enableButton` | `toggled(bool)` → `AudioEngine::setVaxEnabled(ch, bool)` | `vaxEnabledChanged(ch, bool)` → button | Opens/closes `m_vaxBus[ch-1]`; triggers platform driver init (Mac HAL, Linux pipe, Windows PortAudio) | `audio/Vax<ch>/Enabled` | `m_updatingFromModel` |
| `VaxChannelStrip[ch].rxGainSlider` | `valueChanged(float)` → `AudioEngine::setVaxRxGain(ch, float)` | `vaxRxGainChanged(ch, float)` → slider | Applied to tap output before `push` | `audio/Vax<ch>/RxGain` | same |
| `VaxChannelStrip[ch].muteButton` | `toggled(bool)` → `AudioEngine::setVaxMuted(ch, bool)` | `vaxMutedChanged` → button | Skips tap when muted | `audio/Vax<ch>/Muted` | same |
| `VaxChannelStrip[ch].rxMeter` (readonly) | n/a | `vaxRxLevel(ch, float)` → `meter.setLevel(float)` | Level computed in audio thread, published atomic | none | n/a |
| `VaxChannelStrip[ch].deviceLabel` (click) | opens device picker → `AudioEngine::setVaxDevice(ch, QString)` | `vaxDeviceChanged` → label text | Rebuilds `m_vaxBus[ch-1]` with new device | `audio/Vax<ch>/DeviceName` | n/a |
| `VaxChannelStrip[ch].tagsLabel` (readonly) | n/a | Listens to every `SliceModel::vaxChannelChanged`; shows list of slices currently assigned to channel `ch` | n/a | none | n/a |
| TX row: tx gain slider | `valueChanged(float)` → `AudioEngine::setVaxTxGain(float)` | `vaxTxGainChanged(float)` → slider | Applied in TX pull path before TXA | `audio/TxGain` | `m_updatingFromModel` |
| TX row: tx meter (readonly) | n/a | `vaxTxLevel(float)` → meter | Level of active TX source | none | n/a |

### 6.5 Setup → Audio → Devices tab (for each of Speakers / Headphones / TX Input)

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| `driverApiCombo` | `currentTextChanged(QString)` → `AudioEngine::setXxxDriverApi(QString)` | on load, combo text set from settings | Re-creates backing `IAudioBus` with new API | `audio/Xxx/DriverApi` | `m_updatingFromModel` + `QSignalBlocker` |
| `engineRadioGroup` (PortAudio/Direct ASIO — visible only when driverApi=ASIO) | `buttonToggled` → `AudioEngine::setXxxAsioEngine(AsioEngine)` | on load | Swaps `PortAudioBus` ↔ `DirectAsioBus` | `audio/Xxx/AsioEngine` | same |
| `asioDriverCombo` (Direct ASIO only) | `currentTextChanged` → `AudioEngine::setXxxDirectAsioDriver(QString)` | `directAsioDriversEnumerated` → combo | `AsioSdkHost::loadDriver` | `audio/Xxx/DirectAsio/DriverName` | same |
| `asioControlPanelButton` | `clicked()` → `AsioSdkHost::openControlPanel(driverName)` | n/a | Opens ASIO driver's native UI (Windows modal) | none | n/a |
| `baseInChannelCombo`, `baseOutChannelCombo` | `currentIndexChanged` → setters | on load | Passed to `DirectAsioBus` for channel base | `audio/Xxx/DirectAsio/BaseInChannel`, `BaseOutChannel` | same |
| `inputModeRadioGroup` (Left/Right/Both, Direct ASIO only) | `buttonToggled` → setters | on load | Maps to cmASIO `IM_LEFT/IM_RIGHT/IM_BOTH` | `audio/Xxx/DirectAsio/InputMode` | same |
| `lockModeCheckbox` (Direct ASIO only) | `toggled` → setter | on load | cmASIO lock-mode flag | `audio/Xxx/DirectAsio/LockMode` | same |
| `deviceCombo` (PortAudio only) | `currentTextChanged` → `AudioEngine::setXxxDeviceName(QString)` | on load / device-change-detected | Reopens bus | `audio/Xxx/DeviceName` | same |
| `sampleRateCombo` + `autoMatchCheckbox` | `currentIndexChanged` / `toggled` → setters | on load | Bus reopened if needed | `audio/Xxx/SampleRate` | same |
| `bitDepthCombo` | same | same | same | `audio/Xxx/BitDepth` | same |
| `channelsCombo` (Mono/Stereo) | same | same | same | `audio/Xxx/Channels` | same |
| `bufferSamplesSpinner` + derived-ms label | `valueChanged` → setter | on load | Applied on bus reopen | `audio/Xxx/BufferSamples` | same |
| `manualLatencyCheckbox` + `latencyMsSpinner` | same | same | same | `audio/Xxx/ManualLatencyMs` | same |
| `exclusiveModeCheckbox`, `eventDrivenCallback`, `bypassMixer` (WASAPI only) | same | same | same | respective keys | same |
| `formatPreviewLabel` (readonly) | n/a | `AudioEngine::xxxFormatNegotiated(AudioFormat)` → label | Shows actually-negotiated format + status pill | none | n/a |

### 6.6 Setup → Audio → VAX tab

Per-channel card (1–4):

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| `vaxEnabledCheckbox` | `toggled(bool)` → `AudioEngine::setVaxEnabled(ch, bool)` | `vaxEnabledChanged(ch, bool)` → checkbox | Same as 6.4 | `audio/Vax<ch>/Enabled` | `m_updatingFromModel` |
| `vaxDriverApiCombo` (hidden on Mac/Linux native; shown Windows BYO) | same pattern as 6.5 | same | same | `audio/Vax<ch>/DriverApi` | same |
| `vaxDeviceCombo` | `currentTextChanged` → `AudioEngine::setVaxDevice(ch, QString)` | `vaxDeviceChanged` | Rebuilds `m_vaxBus[ch-1]` | `audio/Vax<ch>/DeviceName` | same |
| Format controls (rate/bit-depth/channels/buffer) | same as 6.5 pattern | same | same | respective keys | same |
| `vaxExclusiveModeCheckbox` (WASAPI only) | same | same | same | `audio/Vax<ch>/ExclusiveMode` | same |
| "Auto-detect…" button (unassigned channels) | `clicked()` → `VirtualCableDetector::scan()` → opens first-run-style sub-dialog with detected cables | n/a | If user picks one, calls `setVaxDevice(ch, name)` | n/a | n/a |

### 6.7 Setup → Audio → Advanced tab

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| `dspSampleRateCombo` | `currentIndexChanged` → `AudioEngine::setDspSampleRate(int)` | on load | WDSP channel reconfig | `audio/DspRate` | `m_updatingFromModel` |
| `dspBlockSizeCombo` | same | same | same | `audio/DspBlockSize` | same |
| `vacFeedbackTargetCombo` + per-target gain/slew/rings editors | writes to `audio/VacFeedback/<ch>/*` keys → `AudioEngine::setVacFeedbackParams(ch, params)` | on load (reloads per-target when target combo changes) | Applied to IVAC-parity resamplers inside the VAX bus implementation | as schemed | same |
| `sendIqToVaxCheckbox` | `toggled` → setter | on load | **Feature-flagged: emits a log-warning "IQ to VAX is reserved for future — see Phase 3O+"** | `audio/SendIqToVax` (stored but not active) | same |
| `txMonitorToVaxCheckbox` | `toggled` → setter | on load | same feature-flag note as above | `audio/TxMonitorToVax` | same |
| `muteVaxDuringTxOnOtherSliceCheckbox` | `toggled` → setter | on load | Active: when slice A holds TX, slices B/C/D's VAX taps emit silence (mirrors Thetis behavior) | `audio/MuteVaxDuringTxOnOtherSlice` | same |
| "Detected cables" label row (readonly) | n/a | Live result from `VirtualCableDetector::lastScan()` | n/a | none | n/a |
| "Rescan" button | `clicked()` → `VirtualCableDetector::scan()` | updates Detected-cables row | May open `VaxFirstRunDialog::rescanMode()` if new cables appear | `audio/LastDetectedCables` | n/a |
| "Reset all audio to defaults" button | `clicked()` → confirm modal → `AudioEngine::resetAudioSettings()` | triggers full re-sync from defaults | Clears all `audio/*` keys + rebuilds buses | all `audio/*` removed | n/a |

### 6.8 First-run dialog (`VaxFirstRunDialog`)

| Widget | Action |
|---|---|
| "Apply suggested" button (scenario A, E) | For each suggestion row, call `AudioEngine::setVaxDevice(ch, name)` + `setVaxEnabled(ch, true)`; close dialog; set `audio/FirstRunComplete=True`. |
| "Customize…" button | Close; open Setup → Audio → VAX tab with detected cables pre-filled in each channel card. |
| "Skip" button | Close; set `audio/FirstRunComplete=True`; VAX stays disabled. |
| "Open download page" buttons (scenario B) | `QDesktopServices::openUrl` to vendor's official URL (hardcoded mapping per product in `VirtualCableDetector`). |
| "Continue without VAX" | Same as Skip. |
| "Rescan now" | `VirtualCableDetector::scan()`, reload dialog content. |
| "Got it" (Mac/Linux native scenarios) | Close; set `audio/FirstRunComplete=True`; VAX enabled by default (since drivers are ready). |

### 6.9 TX source / PTT arbitration

Currently shown in the mockup on the Audio Setup page's TX Source bar. Wiring:

| Widget | Widget → Model | Model → Widget | Model → Action | AppSettings | Guard |
|---|---|---|---|---|---|
| TX source combo (MicDirect / VAX 1 / VAX 2 / VAX 3 / VAX 4) | `currentIndexChanged` → `TransmitModel::setTxOwnerSlot(VaxSlot)` | `txOwnerSlotChanged` → combo | TX path switches `pull()` source on next block | `tx/OwnerSlot` | `m_updatingFromModel` |
| PTT owner button row (duplicated in mockup) | same wiring (convenience surface) | same | same | same | same |
| TX input device combo (for MicDirect) | `currentTextChanged` → `AudioEngine::setTxInputDevice(QString)` | on load | Rebuilds `m_txInputBus` | `audio/TxInput/DeviceName` | same |

---

## 7. UI Components in Detail

### 7.1 `VaxChannelSelector` (VFO flag, new)

- 5 small buttons: OFF, 1, 2, 3, 4.
- Exclusive (single-select), backed by `QButtonGroup`.
- Styled per STYLEGUIDE.md: base `#1a2a3a`/`#205070`, active `#0070c0`/`#ffffff`.
- Fixed height 22 px, each button 26 px wide min.
- Embedded in a new row on `VfoWidget` under the existing MODE/FILT/AGC/NR tabs, grouped with an "AUDIO" row already planned.

### 7.2 `VaxApplet` (docked, ported + renamed)

- Title bar "VAX" + subtitle "4 channels" (per STYLEGUIDE.md applet header).
- Four `VaxChannelStrip` rows + divider + TX row.
- Each `VaxChannelStrip` layout (top-to-bottom, compact):
  ```
  [VAX N name]  [assigned-slices chips]              [MUTE]
  [level meter ═══════════════════════]  [RxGain slider]  [dB readout]
  [device-type tag] · [device name]
  ```
- Fixed width ~310 px docked; resizable if floated.
- All controls keyboard-navigable (Qt focus chain).

### 7.3 `MasterOutputWidget` (menu bar corner, new)

- 3 elements horizontal: speaker icon (14 px), slider (100 px wide), dB readout (inset), MUTE button.
- Speaker icon click toggles mute; scroll wheel on slider adjusts in 1 dB steps; right-click icon opens device picker submenu.
- Styled as an inset `#0a0a18` / `#1e2e3e` container per STYLEGUIDE.md "Inset Value Display" pattern.

### 7.4 `VaxFirstRunDialog` (modal, new)

- Five scenario constructors (see mockup-firstrun.html for exact layout).
- Fixed 560 px wide; modal; parented to MainWindow.
- Dismissable via `Escape`; but only counts as first-run-complete if user clicked **Apply suggested**, **Skip**, or **Got it** (Mac/Linux).
- Rescan mode (scenario E) pops unobtrusively only when a new cable is detected on app startup.

### 7.5 Setup → Audio pages (flat left-nav children)

**Superseded by addendum [2026-04-20-phase3o-subphase12-addendum.md](2026-04-20-phase3o-subphase12-addendum.md) §3 (2026-04-20).**

The original spec described a single `SetupAudioPage` with four inner `QTabWidget` sub-tabs. Preflight for Sub-Phase 12 discovered this would conflict with six existing stub pages under the Audio category (from Phase 3-UI) and deviate from the flat-nav convention used by DSP / Display / Transmit / Appearance / CAT. The revised structure replaces the six stubs with four flat pages:

- `AudioDevicesPage` — wires per §6.5 (Devices tab content, now a page).
- `AudioVaxPage` — wires per §6.6 (VAX tab content, now a page).
- `AudioTciPage` — placeholder "Coming in Phase 3J (see §11.1)".
- `AudioAdvancedPage` — wires per §6.7 (Advanced tab content, now a page).

Each extends `SetupPage`, is registered directly under the "Audio" category in `SetupDialog::buildTree()`, and is styled per STYLEGUIDE.md with groupbox conventions from `src/gui/SetupPage.cpp`. No `QTabWidget` is used. `VaxFirstRunDialog::openSetupAudioTab(QString)` is renamed to `openSetupAudioPage(QString pageLabel)` and wires to `SetupDialog::selectPage(pageLabel)` directly.

---

## 8. Platform-Specific Implementation

### 8.1 macOS

- **HAL plugin** at `/Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver` (system-wide, admin install).
- Built separately from main app (`hal-plugin/CMakeLists.txt`) because libASPL FetchContent conflicts with the main Ninja build — pattern already proven by AetherSDR.
- `.pkg` installer with two components (app + driver). Postinstall script:
  ```bash
  #!/bin/bash
  # macOS 14.4+ locked down launchctl kickstart for coreaudiod; use killall.
  launchctl kickstart -kp system/com.apple.audio.coreaudiod 2>/dev/null \
    || sudo killall coreaudiod \
    || true
  exit 0
  ```
- Uninstall shell script ships alongside; removes `.driver` and `/dev/shm/nereussdr-vax-*` segments.
- Signing: Apple Developer ID Application + notarization. NereusSDR already pays the $99/yr per CLAUDE.md; no new cost.
- Devices exposed: "NereusSDR VAX 1", "NereusSDR VAX 2", "NereusSDR VAX 3", "NereusSDR VAX 4", "NereusSDR TX".
- Format: float32 stereo @ 48 kHz (we may move from AetherSDR's 24 kHz to 48 kHz to align with Thetis DSP rate; decision confirmed with user as part of this spec).
- IPC: POSIX shm ring, ~2-sec ring at 48 kHz stereo float = ~768 KB per segment.

### 8.2 Linux

- **Bridge** loaded at NereusSDR app startup via `pactl load-module module-pipe-source ...` (RX × 4) + `pactl load-module module-pipe-sink ...` (TX × 1).
- FIFOs in `/tmp/nereussdr-vax-{1..4}.pipe` and `/tmp/nereussdr-tx.pipe`.
- Node names: `nereussdr-vax-1` etc.; descriptions: "NereusSDR VAX 1" etc.
- Works on pure PulseAudio **and** PipeWire-via-pipewire-pulse.
- Stale-module cleanup on startup (port AetherSDR's scan: `pactl list modules short | grep nereussdr-` and `pactl unload-module <idx>`).
- Module lifetimes bound to app lifetime; clean teardown on quit.
- No separate installer; modules load at runtime from the app.
- Packaged as `.deb` / `.rpm` / AUR / AppImage per existing NereusSDR release.yml.
- **Deferred:** Flatpak/Snap (sandboxing blocks virtual audio node publication per `xdg-desktop-portal` issue #1142). Document this in README.

### 8.3 Windows

- **Default engine:** `PortAudioBus`. Enumerate audio devices via PortAudio host APIs (MME, DirectSound, WDM-KS, WASAPI-shared, WASAPI-exclusive, ASIO).
- **Opt-in engine:** `DirectAsioBus`. When user selects "Direct ASIO" in the Engine radio group, we load the ASIO SDK driver directly (no PortAudio). Ported from Thetis `ChannelMaster/cmasio.c` with full Thetis attribution header per HOW-TO-PORT.md.
- **VAX channels on Windows:** user-picked OS audio devices. `VirtualCableDetector` pre-populates suggestions.
- **No kernel driver shipped in this phase.** A future phase may fork `VirtualDrivers/Virtual-Audio-Driver` (MIT path, SysVAD-based) and add a signed Windows driver for "NereusSDR VAX 1–4" first-class devices. Integration points reserved — device-name patterns in `VirtualCableDetector` already include a placeholder for "NereusSDR VAX N" so the future driver slots in transparently.
- `help/install-virtual-cables.md` documents VB-CABLE (recommended), VAC, Voicemeeter.

### 8.4 PortAudio integration (all platforms)

- Vendor PortAudio at `third_party/portaudio/` (MIT-compatible license, Apache-esque; compatible with GPLv3).
- Link statically into NereusSDR executable.
- Initialize in `AudioEngine::init`, terminate in destructor.
- Host API enumeration: per-platform filter (only show APIs that exist on this OS).
- PortAudio ASIO host API requires Steinberg ASIO SDK headers (not redistributable, but headers-only). Fetch via build-time env var or skip ASIO on CI. **Document clearly** in `CONTRIBUTING.md` build section.

### 8.5 ASIO SDK integration (Windows Direct ASIO)

> **Compliance note (2026-04-19):** After a GPL-3 compliance review during plan authoring, **Direct ASIO is deferred from Phase 3O.** The Steinberg ASIO SDK license is not GPL-3 compatible; linking it into distributed NereusSDR binaries would violate both the ASIO SDK terms and GPL-3. Audacity and other GPL audio projects hit the same wall and ship ASIO-disabled. Decision recorded in the implementation plan's GPL Compliance Review section. This spec section is retained as design intent; if community demand justifies revisiting, it lands as a separate compliance proposal (developer-build-only gate or Steinberg commercial licensing), not part of Phase 3O.

For reference, the original design was:

- Third-party: Steinberg ASIO SDK headers — **cannot be bundled** in the repo due to Steinberg license. Developer must download from Steinberg and drop into `third_party/asio-sdk/` locally.
- Build-time check: if headers present, compile `DirectAsioBus`; otherwise compile a stub that reports "Direct ASIO unavailable — rebuild with ASIO SDK" when user selects it.
- Runtime fallback: if Direct ASIO can't load a driver, fall back to PortAudio ASIO and log a warning.

**Phase 3O actually ships:** PortAudio's built-in ASIO host API (when enabled via `PA_USE_ASIO`) covers 95%+ of ASIO use cases via the Driver API dropdown. No Engine radio button. No native SDK direct-access engine.

---

## 9. Implementation Difficulty Ranking

Per user preference: no calendar estimates. Ordering from easiest to hardest.

🟢 **Easy**
- Windows `VirtualCableDetector` + first-run helper (enumerate devices, regex-match product patterns, show dialog).
- Linux `LinuxPipeBus` (port AetherSDR bridge, rebrand strings).
- AppSettings schema additions + migration path.
- `VaxChannelSelector` widget (button group).
- `MasterOutputWidget` (menu-bar strip).

🟡 **Medium**
- `VaxApplet` port + rename from AetherSDR `DaxApplet` (integrate with NereusSDR's `ContainerWidget`/applet framework).
- `CoreAudioHalBus` + HAL plugin (`hal-plugin/NereusSDRVAX.cpp`) — port from AetherSDR including signing/notarization in release.yml, plus macOS 14.4+ killall fix.
- `SetupAudioPage` (Devices/VAX/Advanced tabs with all wiring).
- TX arbitration state machine.
- `MasterMixer` — per-slice mute/volume/pan accumulation (simple on the surface, RT-safety rigor required).

🟠 **Medium-hard**
- `IAudioBus` abstraction + audio-engine refactor to replace the direct `QAudioSink` model.
- `PortAudioBus` — full driver-API coverage, format negotiation, buffer handling, watchdogs for device-change/unplug (port the robust patterns from AetherSDR `AudioEngine`).

🔴 **Hard**
- `DirectAsioBus` + `AsioSdkHost` — port Thetis `cmasio.c` logic. ASIO callbacks are notoriously unforgiving RT-safety contexts; testing requires physical ASIO hardware. Byte-for-byte Thetis header port + inline-comment preservation per HOW-TO-PORT.md.
- End-to-end smoke test matrix (WSJT-X QSO round trip on Mac / Linux / Windows, including cable-install first-run).

---

## 10. Risks & Edge Cases

| Risk | Mitigation |
|---|---|
| **macOS 14.4+ kickstart lockdown** on `coreaudiod` | Postinstall script already falls back to `killall coreaudiod`. Verified against AetherSDR community reports. |
| **PipeWire/Pulse stale modules** after NereusSDR crash | Startup scans `pactl list modules short` for `nereussdr-` and unloads before loading fresh ones. Port of AetherSDR's proven cleanup. |
| **Windows 24H2 virtual-audio regressions** (Elgato, VB-Audio both hit these in 2025) | Treat first-run detect as authoritative; show an "Unsupported device" state if PortAudio can't open a detected cable; log which cable and surface it in SupportDialog. |
| **WASAPI exclusive-mode contention** (another app holds the device) | Detect and fall back to shared mode with a warning pill; user can retry from Setup. |
| **ASIO SDK headers missing at build time** | Stub `DirectAsioBus`; runtime warn-and-fallback to PortAudio ASIO. `CONTRIBUTING.md` documents. |
| **Audio-thread RT-safety violations** | Review every new `push`/`pull` path for: no `qDebug` in the hot path, no allocations, no locks; use Clang `[[clang::no_destroy]]` / `-fsanitize=thread` in CI where feasible. |
| **Shared-memory name collision** when NereusSDR and AetherSDR both installed on the same Mac/Linux host | Different prefixes (`/nereussdr-vax-*` vs `/aethersdr-dax-*`), different HAL plugin bundle IDs. Both coexist cleanly. |
| **SliceModel `vaxChannel` persistence across band/slice creation** | Stored per-slice-id; on new slice creation, default to OFF. Tested in the existing SliceModel persistence tests. |
| **TX output-device contention** when user picks the same device as Speakers | Block the selection in UI (combo shows incompatible devices greyed with tooltip). |
| **VB-CABLE uninstalled while app is running** | Detector re-scans on Setup→Audio open; if the cable disappears mid-session, VAX bus reports the error upstream, UI shows a warning badge on that slot. |
| **First-run dialog blocking the first launch** | Dialog is modal over MainWindow but MainWindow is fully functional; dialog dismissible with ESC/Skip. No deadlock path. |
| **PortAudio ASIO SDK licensing** | Vendor PortAudio without ASIO host API by default; developer must opt-in with Steinberg SDK (headers-only, non-distributable) for ASIO support. |

---

## 11. Reserved Integration Points

### 11.1 TCI (Phase 3J)

`IAudioBus` taps are the contract. When Phase 3J lands:

- TCI server subscribes to the same `AudioEngine::vaxTap(ch)` signal path that drives `CoreAudioHalBus::push` / `LinuxPipeBus::push` / `PortAudioBus::push`.
- TCI's per-client resampler consumes the tap float32 samples and framing-over-WebSocket-encodes them.
- No refactor required. The `AudioEngine::setVaxTapSubscriber(ch, ITapConsumer*)` registration path is stubbed in this phase (unused) and activated in 3J.

### 11.2 NereusSDR-owned Windows virtual audio driver

`VirtualCableDetector` regex list reserves `"NereusSDR VAX \d"` as a recognized device-name pattern. When we ship a signed Windows driver:

- Driver exposes devices named "NereusSDR VAX 1..4" + "NereusSDR TX".
- Detector auto-binds them on first run, replacing user-installed BYO cables if present.
- User's existing VAX slot bindings migrate automatically (device name lookup).
- No new UI surface; the Windows experience quietly upgrades to native.

### 11.3 IQ-to-VAX (Thetis parity)

`audio/SendIqToVax` setting is persisted but currently logs-and-ignores. When activated in a future phase, RX1's pre-DSP I/Q feeds `m_vaxBus[ch-1]` via a separate `pushIq()` API on `IAudioBus`. Consumer apps see a 2-channel I/Q stream instead of demodulated audio.

---

## 12. Attribution Compliance Checklist

For every ported file in §4.1 above, the PR introducing the port must:

- [ ] Add the port-citation + modification-history header block (template in §4.1).
- [ ] Register the file in `docs/attribution/aethersdr-contributor-index.md` (or equivalent provenance file).
- [ ] For files with Thetis ancestry (`DirectAsioBus`, `AsioSdkHost`): copy Thetis byte-for-byte header from `ChannelMaster/cmasio.c` AND `Console/clsCMASIOConfig.cs` per HOW-TO-PORT.md rule 4 (multi-source file stacking).
- [ ] Preserve all inline attribution markers (`//MW0LGE`, `//W4WMT`, etc.) verbatim at the corresponding port positions.
- [ ] Stamp all `// From Thetis …` cites with `[vX.Y.Z.W]` (captured once at port-session start via `git -C ../Thetis describe --tags`).
- [ ] Run `scripts/verify-thetis-headers.py` — must pass.
- [ ] Run `scripts/check-new-ports.py` — must detect new PROVENANCE rows.
- [ ] Human author (J.J. Boyd KG4VCF) reviews the full diff before merge, including every attribution block, per CLAUDE.md ring-1 checklist.

---

## 13. Plan Review History

| Date | Reviewer | Outcome |
|---|---|---|
| 2026-04-19 | J.J. Boyd (KG4VCF) | Design brainstormed against Thetis VAC/cmASIO + AetherSDR DaxApplet + SmartSDR DAX model + Loopback/Dante/RME reference UIs. User chose AetherSDR-style routing (VFO-flag selector + VaxApplet) over patchbay/matrix after side-by-side mockups. BYO-cable chosen for Windows v1 with native driver deferred. TCI scoped out. |
| 2026-04-19 | J.J. Boyd (KG4VCF) | Pre-execution GPL-3 compliance audit performed while authoring the implementation plan. **Direct ASIO engine (Sub-Phase 13) dropped** from Phase 3O scope: the Steinberg ASIO SDK license prohibits redistribution of modified SDK code and is not GPL-3 compatible; linking it into distributed NereusSDR binaries would violate both licenses. PortAudio's built-in ASIO host API retained as the ASIO path. All other bundled / linked components (AetherSDR GPL-3 ports, Thetis GPL-2+ ports where applicable, libASPL MIT, PortAudio MIT, Qt6 LGPL-3) verified compatible. PortAudio license verification step added to plan Task 3.1. Full `COMPLIANCE-INVENTORY.md` registration added as plan Task 14.4 including the macOS HAL plugin's separate-binary / mere-aggregation (GPL-3 §5) reasoning. |

---

## 14. Success Criteria

- [ ] All controls listed in §6 are fully round-trip wired; zero unwired buttons, zero TODO-for-later setters, zero fire-once widgets.
- [ ] Thetis user migrating to NereusSDR can configure equivalent VAC1/VAC2 workflow in Setup → Audio in under 5 minutes on Windows.
- [ ] macOS user sees "NereusSDR VAX 1–4" + "NereusSDR TX" in WSJT-X audio picker immediately after install.
- [ ] Linux user on any major distro (Fedora/Ubuntu/Arch) sees "NereusSDR VAX 1–4" in WSJT-X without extra setup.
- [ ] Windows user with VB-CABLE installed sees auto-detected suggestions on first launch.
- [ ] Windows user without any virtual cable sees install guidance with links; can use speakers-only mode to skip.
- [ ] Master output widget persists device and volume across restarts; responds to keyboard scroll-wheel and mute toggle.
- [ ] All ported files carry correct attribution headers; `scripts/verify-thetis-headers.py` and `scripts/check-new-ports.py` pass.
- [ ] Audio-thread RT-safety: no `qDebug`, no allocations, no locks in any callback on the hot path.
- [ ] TCI integration path is stub-reserved so Phase 3J lands with no refactor to this code.
- [ ] Help doc `help/install-virtual-cables.md` published and linked from first-run dialog.
