# Phase 3F: Multi-Panadapter + Multi-Slice + Wideband Extension + Full Diversity Port

**Date:** 2026-05-26
**Author:** J.J. Boyd ~KG4VCF, co-authored with Claude Sonnet 4.6
**Status:** Design (brainstorm complete, awaiting spec review)
**Scope:** Phase 3F (multi-panadapter), 3F-DIV (full Thetis Diversity port), wideband extended-pan
**Supersedes:** `docs/architecture/phase3f-multi-panadapter-plan.md` (2026-04-09), `docs/architecture/multi-panadapter.md` (Phase 2B design)

---

## 0. Source Attribution

This design ports behaviour from multiple upstreams. Per `docs/attribution/HOW-TO-PORT.md`, every implementation that derives from these sources must carry the appropriate header and inline cites.

- **Thetis** v2.10.3.15 (ramdor/Thetis) - DDC state machine, Diversity dialog, WDSP integration, filter policy
- **mi0bot-Thetis** v2.10.3.13-beta2 - HL2 384 kHz P1 extension, HL2 PS rate carveout
- **AetherSDR** v0.8.19-10-g0cd4559a - slice lifecycle, pan layout templates, panadapter applet, floating window pattern, audio model, RIT/XIT scope
- **WDSP** (TAPR/OpenHPSDR-wdsp) - DSP primitives (`SetEXTDIV*`, `SetXcmInrate`, `Spectrum`)

---

## 1. Overview

Phase 3F transitions NereusSDR from a fixed single-slice client to a hardware-capability-driven multi-slice client. Operators on 2-ADC boards (G2-class) get up to 5 user slices; HL2 stays at 1; intermediate SKUs gate at their hardware maximum. The pan layer is rebuilt around the AetherSDR overlay model. Wideband bandscope is folded into the pan-zoom gesture rather than a separate pan type. The Thetis Diversity dialog is ported in full, including the radar visualisation.

### Goals

1. **Multi-slice operation** capped per SKU by `BoardCapabilities.maxSlices`. Slice creation is on-demand via `+RX` button or `RadioModel::addSliceOnPan(panId)` (AetherSDR-faithful workflow).
2. **Multi-panadapter** with 5 layout templates (1, 2v, 2h, 12h, 2x2) plus floating windows.
3. **AetherSDR overlay model**: a pan is a viewport into one DDC's FFT. Any slice whose frequency falls within the pan's visible range appears as an overlay flag, regardless of which DDC actually demodulates it.
4. **Antenna-driven ADC chain assignment** on 2-ADC boards: the slice's antenna preference determines which Alex chain (ADC) it lands on. Codec automates the routing.
5. **Hybrid Alex filter policy**: auto-distribute across chains, auto-bypass on multi-band conflict, operator override available.
6. **Wideband as extended pan**: zoom a DDC pan out beyond its native 1.5 MHz and the pan extends into wideband data for the wings, keeping a listenable I/Q island in the centre.
7. **Full Thetis Diversity port**, including the radar widget and direction-finding math.
8. **TX-slice arbiter** (`TxSliceArbiter`) enforces single-TX invariant with MOX-drop guard.

### Non-goals

- Operator-facing per-DDC manual selector on every slice flag (codec abstracts DDC numbers; Setup → Hardware → DDC Routing handles the rare power-user case).
- Per-slice output device routing (AetherSDR-faithful: single global output device, per-slice pan/gain/mute via existing `MasterMixer`).
- Per-slice VFO B / split mode (AetherSDR-faithful: single VFO per slice; XIT handles ±10 kHz offsets).
- HL2 P1 wideband (different protocol mechanism; deferred to 3F-W follow-on after research).
- 8-pane or larger layouts (use floating windows for overflow).

---

## 2. Per-SKU Capability Table

### BoardCapabilities extensions for Phase 3F

Three fields added to the existing `BoardCapabilities` row per SKU:

| Field | Type | Purpose |
|---|---|---|
| `maxSlices` | `int` | Hardware cap on user-facing slices |
| `hasDiversity` | `bool` | `adcCount >= 2` shorthand |
| `widebandAdcs` | `int` | Number of ADCs that support the wideband stream (0 = none) |

Existing fields used as inputs: `adcCount`, `supportedSampleRates`, `defaultSampleRate`, `hasPureSignal`, `antennaInputCount`, `hasAlex`.

### Resolved values per SKU

| SKU | ADCs | DDCs | User DDCs | maxSlices | Sample-rate ladder (kHz) | hasDiversity | widebandAdcs |
|---|---|---|---|---|---|---|---|
| HermesLite2 (HL2) | 1 | 4 | DDC0 only | **1** | 48, 96, 192, 384 | false | 0 (defer, P1 mechanism) |
| HermesLite2 RX-only | 1 | 4 | DDC0 only | **1** | 48, 96, 192, 384 | false | 0 |
| Metis | 1 | 3 | DDC0-2 | **3** | 48, 96, 192 | false | 0 |
| Hermes (ANAN-10/100) | 1 | 4 | DDC0-3 | **4** | 48, 96, 192 | false | 0 |
| HermesII (ANAN-10E/100B) | 1 | 2 | DDC0-1 | **2** | 48, 96, 192 | false | 0 |
| Angelia (ANAN-100D) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192 | true | 2 |
| Orion (ANAN-200D) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192 | true | 2 |
| OrionMkII / 7000DLE / 8000DLE | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| Saturn / ANAN-G2 / G2_1K | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| HermesC10 / ANAN-G2E | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| AnvelinaPro3 | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| Andromeda | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| RedPitaya (P1 mode) | 1 | 4 | DDC0-3 | **4** | 48, 96, 192, 384 | false | 0 |
| RedPitaya (P2 mode) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |

Source cites:
- Sample rate ladders: Thetis `setup.cs:849-850 [v2.10.3.15]` (P1 base + P2 array), mi0bot `setup.cs:850-851 [v2.10.3.13]` (HL2 384k extension via `include_extra_p1_rate`)
- DDC reservations: Thetis `console.cs:8186-8538 [v2.10.3.15]` (UpdateDDCs state machine)
- HL2-specific PS rate carveout: mi0bot `console.cs:8409-8488 [v2.10.3.13]`

### Why DDC0/DDC1 are reserved on 2-ADC boards

Thetis reserves DDC0+DDC1 as a synced pair for PureSignal feedback (during TX) and Diversity (when enabled), regardless of operator preference. Following Thetis exactly preserves the byte-for-byte wire compatibility we already have for the RX1/RX2 case, and avoids the "your slice vanishes on TX" UX trap. User slices land on DDC2-6 (5 slots max on 2-ADC boards).

---

## 3. Slice Model

### Lifecycle: on-demand (AetherSDR pattern)

Slices are created and destroyed by operator action, not pre-allocated. Mechanics:

- **Create**: `+RX` button on `SpectrumOverlayPanel` (top-left of any pan), or View > Add slice on active pan (Ctrl+R), or `RadioModel::addSliceOnPan(QString panId)`.
- **Destroy**: right-click VFO flag > Remove slice, or operator dismisses the pan that hosts it (with confirmation if last slice on that pan).
- **Cap check**: `RadioModel::maxSlices()` (from `BoardCapabilities`). Status-bar reject if exceeded: `"<SKU> supports a maximum of <N> slices"` (port of AetherSDR `MainWindow.cpp:6849-6859`).
- **Slice letter assignment**: A, B, C, D, E in order of creation. Letter freed on destroy. Re-creation gets the lowest available letter.

### Per-slice state (SliceModel additions for Phase 3F)

Existing `SliceModel` already carries frequency, mode, filter, AGC, NR/NB family, RIT/XIT (±10 kHz range, sufficient for SSB DX split), audio pan, gain, mute, VAX channel, antenna preference, txSlice flag.

New Q_PROPERTYs added in 3F:

| Property | Type | Purpose |
|---|---|---|
| `sliceLetter` | `QChar` | A-E for UI display |
| `chainIndex` | `int` | 0 or 1 (which ADC chain hosts this slice) |
| `ddcIndex` | `int` | Read-only: codec-assigned DDC (informational, exposed in Setup) |
| `sampleRateHz` | `int` | Per-slice DDC rate; default `SampleRateCatalog::kDefaultSampleRate` (192 kHz) clamped to SKU max |
| `diversityEnabled` | `bool` | Slice-A-only, gated on `BoardCapabilities.hasDiversity` |
| `widebandExtensionRequested` | `bool` | Derived from pan zoom state, propagates BPF bypass |
| `psPaused` | `bool` | True when this slice's DDC is reclaimed by PS during MOX |

### Per-band per-slice persistence

Existing per-band schema (e.g. `SliceA_Band_20m_Frequency`) extends to cover the new properties:

```
hardware/<mac>/SliceA_Band_20m_Frequency = 14225000
hardware/<mac>/SliceA_Band_20m_Mode = USB
hardware/<mac>/SliceA_Band_20m_Antenna = ANT1
hardware/<mac>/SliceA_Band_20m_SampleRate = 192000
hardware/<mac>/SliceA_Band_20m_DiversityEnabled = False
hardware/<mac>/SliceA_Band_20m_DiversityPhase = 47.5
hardware/<mac>/SliceA_Band_20m_DiversityGain = 0.85
hardware/<mac>/SliceA_Band_20m_DiversityCrossFire = False
hardware/<mac>/SliceA_Band_20m_DiversityLockAngle = False
hardware/<mac>/SliceA_Band_20m_DiversityFineNull = 0.0
hardware/<mac>/SliceA_Band_20m_DiversityM1_Phase = 12.0
hardware/<mac>/SliceA_Band_20m_DiversityM1_Gain = 1.0
... (8 memory slots per band)
```

Slices B-E carry the same schema (minus diversity which is Slice-A-only) per their letter.

### Audio routing: single global output (AetherSDR-faithful)

NereusSDR's `MasterMixer` already supports per-slice gain/pan/mute keyed by slice ID for N slices (`MasterMixer.h:23` setSliceGain). One global output device. Per-slice VAX channel (0=Off, 1..4) already wired. **No changes required for 3F audio routing.** AetherSDR has the same model (`AudioEngine.h:304-422`: single `QAudioSink* m_audioSink`, single `QAudioDevice m_outputDevice`).

### VFO A/B / split: not implemented

AetherSDR has no formal split. Per-slice single frequency + RIT/XIT covers Hz to 10 kHz offsets. Bigger TX offsets handled by creating a second slice and binding TX to it via `TxSliceArbiter`. The stubbed `RadioModel::setSplit(int rx, bool on)` is deleted in 3F cleanup.

---

## 4. DDC / Chain / Alex Architecture

### Mental model: receiver chains

On 2-ADC boards, think of the hardware as **two parallel receiver chains**:

```
Chain 0: some antenna -> Alex0 (HPF/BPF/LPF) -> ADC0 -> DDCs {2, 4, 6}
Chain 1: some antenna -> Alex1 (HPF/BPF/LPF) -> ADC1 -> DDCs {3, 5}
```

Each chain is on one antenna at a time. Slices land on whichever chain is connected to the antenna the slice wants. The codec does the routing; operator picks antennas per slice.

### Codec routing (antenna-driven)

When a new slice's antenna preference is set (or changed), the codec:

1. Looks for a chain already on that antenna - if found, slice joins it (chain may go WIDE if multi-band).
2. Otherwise looks for an idle chain (no other slices) - that chain's antenna becomes the new request.
3. Otherwise the request conflicts with both chains' current antennas: escalate to operator (see §5 conflict resolution).

Codec output extends `PsDdcConfig` (already exists for PureSignal) to carry up to 5 user slices' assignments:

```cpp
struct DdcAssignment {
    int rate[8];          // per-DDC sample rate, 0 = not enabled
    int ddcEnable;        // bitmask: DDC0=bit0..DDC6=bit6
    int syncEnable;       // bitmask: which DDCs sync to DDC0 (PS or diversity)
    int adcCtrl1;         // ADC routing for DDC0-3 (2 bits each)
    int adcCtrl2;         // ADC routing for DDC4-7
    int p1DdcConfig;      // P1 preset (0-6), translated from per-DDC choices
    int p1Diversity;
    int p1RxCount;
    int nDdc;
    int psFwdDdc;         // for PureSignal
    int psRevDdc;         // for PureSignal
};
```

This is a strict extension of the existing `PsDdcConfig` (which already handles DDC0-3 for 2-RX cases). DDC4-6 fields populated only when user slices C/D/E exist.

### Hybrid filter policy (Alex BPF per chain)

The per-chain BPF state machine:

```cpp
struct AlexAdcState {
    enum BpfMode { Auto, ForceBand, ForceBypass };
    enum BpfEffective { Filtered, Bypass, WidebandLocked };
    Band currentBpfBand;
    BpfMode mode;
    BpfEffective effective;
    QString reasonText;  // for WIDE badge tooltip
};
```

Decision tree (computed by `AlexController::recomputeBpf(int adc)`):

1. **WidebandLocked** if any pan on this ADC has wideband extension requested (priority over operator force-bypass; explicit operator intent supersedes).
2. **Bypass** if `mode == ForceBypass`.
3. **Filtered** to TX-bound band if `mode == ForceBand`.
4. Auto mode:
   - 0 slices: `Filtered` to last band, reason "idle".
   - 1 unique band among slices: `Filtered` to that band, reason "<band>".
   - 2+ unique bands: `Bypass`, reason "BYPASS (multi-band: <bands>)".

Recompute triggers (16-row event matrix in §10).

### LPF: TX-only, never bypassed

LPF set on MOX-on to the TX-bound slice's band, released on MOX-off. Non-negotiable for FCC harmonic suppression. Per-chain (each Alex board has its own LPF, but only the TX-bound chain's LPF matters since TX is single-RF).

### HPF: per-ADC operator preference + PS override

HPF (1.8 MHz high-pass) defaults ON. Setup option per-ADC "Disable HPF on ADC N" for MW listening. PS engaging during MOX automatically bypasses HPF on the FB ADC (existing `setHpfBypassOnPs()`, Thetis-faithful).

### File touches

- `src/core/codec/IP1Codec.h`, `IP2Codec.h`: extend `applyPureSignalDdcConfig()` signature to take `std::array<SliceConfig, 5>` instead of `(rx1, rx2)`. Keep backward-compat helper that wraps 2-slice case.
- `src/core/codec/*.{h,cpp}` (all 6 per-board codecs): extend per-board `UpdateDDCs()`-equivalent logic to populate DDC4-6 when slices C/D/E exist.
- `src/core/ReceiverManager.{h,cpp}`: grow `m_hwToLogical` map from 2 entries to `maxSlices`.
- `src/core/WdspEngine.{h,cpp}`: grow `m_channels[2]` to `m_channels[maxSlices]`.
- `src/core/accessories/AlexController.{h,cpp}`: add `AlexAdcState m_perAdcState[2]`, `recomputeBpf(int adc)`, `setBpfMode(int adc, BpfMode)`, `setWidebandActive(int adc, bool)`, `bpfStateChanged(int adc, const AlexAdcState&)` signal.
- `src/core/P2RadioConnection.{h,cpp}`: extend `applyPsDdcConfig` to accept the new `DdcAssignment` struct directly; write `packetbuf[23]` wideband enable byte (currently never written, see §7).

---

## 5. Antenna Conflict Resolution

### Scenarios

| Scenario | Resolution |
|---|---|
| Same-band slices, same antenna | No conflict. Codec keeps them on one chain, both filtered. |
| Different-band slices, different antennas (2-ADC) | Codec splits across chains. Both filtered. |
| Different-band slices, same antenna | One chain hosts both. BPF auto-bypasses. WIDE badge on both pans. |
| 3+ distinct antennas on 2-ADC | Capacity conflict, operator resolution required (see below). |
| Single antenna across all slices | All on one chain, other chain idle. |

### Capacity conflict policy (Setup → Antenna Control)

Three modes operator picks once:

- **Auto-switch (RX only safe)** [default]: if conflict chain hosts only RX-only slices, silently re-route, show toast with Undo. If TX-bound slice on conflict chain, show confirmation dialog.
- **Always ask**: confirmation dialog for any re-route.
- **Auto-switch (any)**: silent re-route even when TX-bound affected (use with care).

### Toast notification (RX-only auto-switch)

Bottom-right, 8-second auto-dismiss, non-blocking. Border-left in `kGreenBg` accent. Format:
> ✓ **Antenna auto-switched**
> Slice B moved from ANT2 to ANT3 for Slice C.    [UNDO]

### TX-bound confirmation dialog

Modal. Lists at-risk slices in a group box (highlighting TX-bound). Three action buttons:
1. **Cancel** - don't add the new slice
2. **Use ANT1 instead** (or whichever existing chain antenna) - compromise the new slice's antenna preference
3. **Re-route ANT3** (the new request) - confirm the TX-affecting change

Body text warns: "Verify ANT3 is rated for the current band and TX power."

---

## 6. TxSliceArbiter

Dedicated class `src/core/TxSliceArbiter.{h,cpp}`. RadioModel owns one instance. Enforces single-TX invariant: exactly one slice can be TX-bound at any moment.

### Public API

```cpp
class TxSliceArbiter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int txBoundSliceIndex READ txBoundSliceIndex NOTIFY txBoundSliceChanged)

public:
    int txBoundSliceIndex() const;
    SliceModel* txBoundSlice() const;

public slots:
    /// Request handoff to a new slice. If MOX is keyed, drops MOX first,
    /// performs the handoff, then operator can re-key. RF-safe.
    /// Returns true if handoff succeeded (or was a no-op).
    bool requestHandoff(int newSliceIndex);

signals:
    void txBoundSliceChanged(int oldIndex, int newIndex);
    void handoffBlocked(int requestedIndex, QString reason);
};
```

### Handoff sequence

1. Check requested slice exists and is not already TX-bound (no-op early-exit).
2. If MOX is keyed: call `MoxController::setMox(false)` and wait for `moxChanged(false)` confirmation.
3. Update `SliceModel::txSlice` to false on old, true on new.
4. Notify `AlexController` to update TX-side antenna routing per new slice's `txAntenna`.
5. Notify `MoxController` of new TX-bound slice (for mode tracking, anti-VOX retap, PA profile reload).
6. Notify `PureSignal` (if active) - PS may need to recalibrate to new band.
7. Emit `txBoundSliceChanged(oldIndex, newIndex)`.
8. Persist to AppSettings: `TxBoundSliceIndex = newIndex` (per-MAC).

### Subscribers

- `MoxController`: tracks which slice's mode/audio drives TX
- `AlexController`: updates TX antenna + LPF on handoff
- `VfoWidget`: updates `setTxSlice(bool)` on every slice's flag (red TX badge appears/disappears)
- `SpectrumStatusOverlay`: updates TX badge on the affected pans
- Persistence layer: writes `TxBoundSliceIndex`

### Click handler on `m_txBadge`

`VfoWidget::m_txBadge` (existing, currently has no `clicked()` handler) gains a click connection:
- Click on a non-TX-bound slice's TX badge → `TxSliceArbiter::requestHandoff(thisSliceIndex)`
- Click on TX-bound slice's TX badge → no-op (you're already TX-bound)

### Restore on launch

`TxBoundSliceIndex` restored from AppSettings on connect. If that slice doesn't exist post-restore (e.g. operator deleted it last session), default to Slice A.

---

## 7. Wideband Extended Pan

### Reframing

Wideband is not a separate pan type. It is the natural extension of zooming a DDC pan past its native bandwidth. Operator zooms wheel out, the visible spectrum widens, and the pan transitions seamlessly:

- Centre region (±768 kHz around DDC centre): **listenable I/Q** from the DDC, full resolution (~375 Hz/bin at 1536 kHz DDC rate)
- Wing regions: **wideband data** from the ADC's wideband stream, coarse (7.5 kHz/bin)
- Dashed boundary lines mark the transition
- Click in centre → slice retunes within DDC (instant)
- Click in wing → DDC retunes to that frequency, listenable island jumps (~50-100 ms wire command)

### Wire format (Thetis network.c:550-603 [v2.10.3.15])

- 8 UDP ports (1027-1034), one per ADC slot
- Per-packet: 1028 bytes = 4 bytes seq + 512 samples × 16-bit BE real (not I/Q)
- Frame assembly: 32 packets per frame, seq 0..31 from radio
- Frame interval: 70 ms (14.3 Hz refresh)
- Per-frame: 16,384 real samples = 133 µs snapshot of real time
- Effective bandwidth per ADC: ~3.76 Mbps wire
- 5 user slices @ 1536 kHz + 1 wideband ADC = ~380 + 4 = ~384 Mbps wire (38% of gigabit)

### The enable bit gap

`P2RadioConnection.cpp:2141-2145` currently writes `packetbuf[24..28]` (wideband config bytes) but **never writes `packetbuf[23]`** (the per-ADC enable mask, Thetis `network.c:880-882 [v2.10.3.15]`). One-line fix in `composeCmdRx`: `buf[23] = m_wbEnableMask;` where mask bit N corresponds to ADC N.

### Implementation tasks

1. **Enable plumbing**: `P2RadioConnection::setWidebandEnabled(int adc, bool on)` slot, internal `m_wbEnableMask` byte, write `buf[23]` in `composeCmdRx`. Recompute mask whenever a pan transitions in/out of extended mode.
2. **Receive path**: replace `case 2..9: break;` stub in `P2RadioConnection.cpp:1608-1617` with real packet decode. Sequence-error handling matches Thetis `network.c:572-600` (zero-pad partial frames).
3. **Frame accumulator**: per-ADC buffer 32 packets × 512 samples → 16384 floats. Emit `widebandFrameReady(int adcIndex, const QVector<float>& samples)`.
4. **FFT pipeline**: new `WidebandFftEngine` (real-input variant of FFTEngine), 16384-pt FFTW3 r2c plan, output 8192 dBm bins covering 0..61.44 MHz.
5. **SpectrumWidget rendering**: `SpectrumWidget` gains `paintExtendedPan()` that composites listenable I/Q (centre) + wideband data (wings) with dashed boundary indicators. Bin density discontinuity at boundary is visually intentional (different stroke weight/opacity).
6. **Zoom gesture coordination**: when `SpectrumWidget`'s visible-range zoom exceeds DDC bandwidth, set `SliceModel::widebandExtensionRequested = true`. Propagates to `AlexController` (auto-bypass BPF for that ADC) and `P2RadioConnection` (set wideband enable bit). When zoom returns within DDC range, clear the request.
7. **Per-pan toggle**: right-click pan → "Extended view" (default on). Operator can disable for a specific pan if they want BPF protection for weak-signal work.
8. **HL2 / standard P1 wideband**: separate research item (different protocol mechanism), defer to 3F-W follow-on. Capability-gated via `BoardCapabilities.widebandAdcs > 0`.

### Refresh rate mismatch

Listenable region: DDC frame rate (~50 fps typical at 192 kHz). Wideband: 14 fps. Visually the wings update slower than the centre. Operator's eye attends to the brighter listenable region; the slower wing update is documented but acceptable.

---

## 8. Diversity: Full Thetis Port

### Scope: every Thetis control + radar widget + direction finding

Per-SKU gated on `BoardCapabilities.hasDiversity` (2-ADC boards only). Slice A only (consumes DDC0+DDC1 sync pair).

### Tools menu entry

`Tools > Diversity Dialog…` (keyboard `Ctrl+Shift+D`), modeless, capability-gated. Greyed on 1-ADC SKUs.

### Dialog structure

Left column:
- **Sensitivity pattern** group box: `DiversityRadarWidget` (custom paint widget, ported from Thetis `DiversityForm.cs:picRadar_Paint`). Polar plot showing antenna sensitivity vs azimuth. Compass labels (N/E/S/W). Range rings. Sensitivity lobe in cyan. Null direction marker. Live update as phase/gain dial. Click-and-drag on radar to interactively retune phase/gain.
- **Direction finding** group box: `udAntSpacing` (meters), `udCalib` (calibration), derived `d/lambda` display, derived direction label.

Right column:
- **Enable diversity** checkbox + DIV pill
- **Phase & Gain** group box (per-band):
  - `udAngle` (radians slider + spinbox, -π to +π)
  - `udAngle0` (degrees, 0-359)
  - `udFineNull` (precision adjustment)
  - `udR` (gain ratio, 0.0-10.0)
  - 7 quick-nudge buttons: -10°, -45°, -90°, +45°, +90°, +10°, 180° (green for the flip)
- **Mode** group: Auto adjust, Cross-fire (+180°), Lock angle, Always on top
- **Memories** (per-band): 8 slots M1-M8 in 4×2 grid. Each shows stored phase+gain or "empty". Click to recall. Right-click to store current. Bold green dot on active slot.
- **Reference ADC**: radio buttons "ADC 0 (primary)" / "ADC 1 (secondary)" [translated from Thetis Mercury1/Mercury2]
- **Slice integration**: "Sync Slice A → Slice B frequency" (greyed when no Slice B), "Link ATT between Slice A and Slice B" [translated from chkVFOSync, chkNoAttLink with positive semantics]

Footer:
- WDSP state readout: `EXTDIVOutput: 0 (combined) · EXTDIVRun: 1 · WDSP ch 0`
- Auto-find null button (one-shot)
- Close button

### WDSP wrappers

Port from Thetis `dsp.cs:609-619 [v2.10.3.15]`:

```cpp
class TxChannel {  // actually RxChannel since diversity is RX-side
    void setExtDivRun(bool run);                                          // SetEXTDIVRun
    void setExtDivNr(int nr);                                             // SetEXTDIVNr
    void setExtDivOutput(int output);                                     // SetEXTDIVOutput
    void setExtDivRotate(int nr, const double* iRotate, const double* qRotate);  // SetEXTDIVRotate
};
```

Phase+gain translate to rotation coefficients: `I = gain × cos(phase)`, `Q = gain × sin(phase)`.

### DDC topology shift (existing UpdateDDCs behaviour preserved)

When diversity enables on Slice A:
- Slice A migrates DDC2 → DDC0+DDC1 sync pair (DDC0 on ADC0, DDC1 on ADC1, both synced)
- DDC2 frees up
- Other user slices (B-E) unaffected (live on DDC3-6)

When PS engages during MOX with diversity active: PS wins (Thetis behaviour). Diversity pauses with status "DIV paused (PS active)" on Slice A's flag. Resumes on MOX-off.

### Persistence (per-band per-slice-A)

Per band (14 bands × Slice A):
- `DiversityEnabled`, `DiversityPhase`, `DiversityGain`, `DiversityFineNull`, `DiversityCrossFire`, `DiversityLockAngle` (6 values × 14 = 84)
- `DiversityM1_Phase` through `DiversityM8_Phase` + matching `_Gain` (16 × 14 = 224)

Global (not per-band):
- `Diversity_ReferenceAdc`, `Diversity_AntSpacingMeters`, `Diversity_Calibration`, `Diversity_SyncSliceB`, `Diversity_LinkAtt`, `Diversity_AlwaysOnTop`, `Diversity_AutoAdjust`, `Diversity_DialogGeometry` (8 values)

Total: ~316 persisted values. Mechanical.

### Translations (Thetis-RX1/RX2 concepts → N-slice)

| Thetis | NereusSDR | Reason |
|---|---|---|
| `radioButtonMerc1` / `radioButtonMerc2` | "ADC 0 (primary)" / "ADC 1 (secondary)" radio | Mercury board → ADC routing terminology in our codec model |
| `chkVFOSync` | "Sync Slice A → Slice B frequency" (greyed when no Slice B) | RX1/RX2 pair → A↔B in N-slice |
| `chkNoAttLink` | "Link ATT between Slice A and Slice B" (positive semantics) | Removed double-negative |
| `Andromeda diversity form landscape` | skipped | Thetis touch-console UI, not applicable to desktop-first NereusSDR |

### Scope estimate

~10 working days within 3F. Day-by-day:
1. WDSP wrappers (SetEXTDIVRun/Nr/Output/Rotate) + RxChannel integration
2. SliceModel diversity properties + per-band persistence schema
3. DiversityDialog QWidget basics (controls, mode checkboxes, quick-nudge buttons)
4. WDSP wrappers wire-up complete + basic enable test on G2
5. DiversityRadarWidget custom paint (port `picRadar_Paint` + `SensitivityAtAngle` math)
6. Memory slots (M1-M8 per band, store/recall logic)
7. Direction finding (antenna spacing math, derived d/lambda, direction label)
8. Slice integration (sync, ATT link), DDC topology hook (Slice A migrates DDC2 → DDC0+1), PS conflict handling
9. Per-SKU bench verification matrix (G2, G2E, MkII, Saturn, Andromeda, AnvelinaPro3, Angelia, Orion, OrionMkII)
10. Polish, tooltips, settings-restore edge cases

---

## 9. PS-on Transition UX

When PureSignal engages during MOX, the PS pair (DDC0+DDC1) is reclaimed regardless of slice ownership:

- On 1-ADC SKUs (Hermes/HermesII): all user slices pause (DDC0/1 are user DDCs on those boards).
- On 2-ADC SKUs without diversity: no user slice impact (DDC0/1 already reserved per §2).
- On 2-ADC SKUs with diversity on Slice A: Slice A's diversity pauses, falls back to single-ADC DDC2 RX.

### Visual treatment

Paused slices get:
- Pan rendered at 55% opacity (greyed via `QPainter` global alpha or stylesheet)
- `PS HOLD` pill (amber, italic) on the SpectrumStatusOverlay
- Same `PS HOLD` pill on the VfoWidget flag header

Auto-resume on MOX-off restores opacity and removes the pill. No operator action required.

### Bottom bar

Existing PSA indicator (`m_psaIndicator`, PsaIndicatorWidget) continues to show FB + PS state at the bottom-bar level. No changes needed in 3F.

---

## 10. Alex Filter Recompute Trigger Matrix

`AlexController::recomputeBpf(int adc)` fires on these events:

| Event | Triggers | Scope |
|---|---|---|
| Slice created | ✓ | new slice's ADC |
| Slice destroyed | ✓ | removed slice's ADC |
| Slice freq change → different band | ✓ | that slice's ADC |
| Slice freq change → same band | (no) | — |
| Slice moved to different DDC/ADC (Setup override) | ✓✓ | both old and new ADC |
| Wideband extension enabled/disabled on ADC N | ✓ | ADC N |
| Operator changes BpfMode (Auto/ForceBand/ForceBypass) | ✓ | that ADC |
| TX-bound slice changes (TxSliceArbiter handoff) | conditional | only if ForceBand mode (BPF tracks TX-bound) |
| MOX on (start TX) | (LPF only) | LPF set to TX-bound band, BPF unchanged |
| MOX off (end TX) | (LPF only) | LPF released, BPF unchanged |
| PS engages | (HPF only) | HPF bypass for FB ADC via existing `setHpfBypassOnPs` |
| Diversity enabled | ✓ | ADC0 (Slice A moved there) |
| Slice mode change | (no) | mode doesn't affect filter selection |
| Slice sample rate change | (no) | rate doesn't affect filter selection |
| Slice enable/disable (live → dormant) | ✓ | that slice's ADC |
| Antenna change (per-band Alex routing) | conditional | BPF unchanged, only routing bits |
| Antenna change that switches chains | ✓✓ | both old and new chain |

---

## 11. UI Surface Atlas Summary

See visual mockups in `docs/architecture/2026-05-26-phase3f-ui-mockups/` (separate directory of HTML pages preserved from brainstorm session for design review reference).

### New widgets (11)

1. `SpectrumStatusOverlay` (top-right per pan, 22px height, mirror of `SpectrumOverlayPanel` left collapse pattern)
2. `+PAN` dropdown menu (activated placeholder in bottom status bar)
3. `+RX` button (green) prepended to `SpectrumOverlayPanel`'s button stack
4. Right-click VFO flag context menu (Make TX, Antenna, Sample rate, Move pan, Move DDC, Diversity, Filter policy, Remove)
5. Antenna picker submenu (with inline chain-consequence hints)
6. Antenna auto-switch toast (bottom-right, 8s auto-dismiss, Undo button)
7. TX-bound antenna re-route confirmation dialog (modal, lists at-risk slices)
8. Filter Policy popup (per-chain BPF mode, HPF toggle)
9. Pan Layout picker dialog (5 visual tiles, "Wide+2" tagged WIDEBAND)
10. Setup → Hardware → DDC Routing page (full per-DDC table with assignments + ADC + status)
11. Diversity Dialog (full Thetis port: radar widget + direction finding + memories + all controls)

### Modified widgets (7)

1. Bottom status bar: `+PAN` cluster activated, CH 0 / CH 1 stacked indicators added
2. `VfoWidget`: DIV badge + chain tag in header row (chain tag hidden on 1-ADC SKUs)
3. `SpectrumWidget`: extended-pan rendering (listenable island + wideband wings + dashed boundaries)
4. `SpectrumOverlayPanel`: +RX button added (green, capability-gated)
5. Setup → Antenna Control: gains "Conflict policy" group (3 radio modes)
6. View menu: Pan Layout (Ctrl+L), Add slice on active pan (Ctrl+R), Float active pan
7. Tools menu: Diversity Dialog (Ctrl+Shift+D)

All using exact `StyleConstants.h` palette + real slice colors from `VfoWidget::sliceColor()` (cyan / magenta / green / yellow for A / B / C / D).

---

## 12. Persistence Schema (Complete)

### Per-MAC (hardware/<mac>/)

Per-slice per-band (×5 slices × 14 bands = 70 base rows):
- `Slice<X>_Band_<band>_Frequency` (Hz)
- `Slice<X>_Band_<band>_Mode`
- `Slice<X>_Band_<band>_FilterLow`, `FilterHigh`
- `Slice<X>_Band_<band>_AgcMode`, `AgcThreshold`, `AgcHang`, `AgcSlope`
- `Slice<X>_Band_<band>_RitEnabled`, `RitHz`, `XitEnabled`, `XitHz`
- `Slice<X>_Band_<band>_AfGain`, `AudioPan`, `Muted`
- `Slice<X>_Band_<band>_Antenna`
- `Slice<X>_Band_<band>_SampleRate` (new)
- `Slice<X>_Band_<band>_NrSlot`, `NbMode`, `Squelch*`, etc. (existing per-band schema)

Slice A only (per band):
- `SliceA_Band_<band>_DiversityEnabled`, `DiversityPhase`, `DiversityGain`, `DiversityFineNull`, `DiversityCrossFire`, `DiversityLockAngle`
- `SliceA_Band_<band>_DiversityM<N>_Phase`, `_Gain` (N=1..8)

Per-chain state:
- `Alex0_BpfMode`, `Alex1_BpfMode` (Auto/ForceBand/ForceBypass)
- `Alex0_HpfEnabled`, `Alex1_HpfEnabled`
- `Wideband_Adc0_Enabled`, `Wideband_Adc1_Enabled` (derived from pan state)

TX arbiter:
- `TxBoundSliceIndex` (0..4)

Pan layout:
- `PanLayoutId` ("1" / "2v" / "2h" / "12h" / "2x2")
- `PanCount`, `PanSplitter0Sizes`, `PanSplitter1Sizes` (existing schema from Phase 3F plan)
- `Pan<N>_CenterMhz`, `Pan<N>_BandwidthMhz`, `Pan<N>_FftSize` (existing)
- `Pan<N>_SliceFocus` (which slice's data this pan primarily shows, AetherSDR overlay model)
- `Pan<N>_Floating` (bool), `Pan<N>_FloatGeometry` ("x,y,w,h")
- `Pan<N>_ExtendedView` (bool, default true)

DDC routing override (rare, Setup page only):
- `Slice<X>_DdcOverride` (int, default -1 = auto)
- `Ddc<N>_AdcOverride` (int, default -1 = auto)

### Global (not per-MAC)

- `Diversity_ReferenceAdc`, `Diversity_AntSpacingMeters`, `Diversity_Calibration`, `Diversity_SyncSliceB`, `Diversity_LinkAtt`, `Diversity_AlwaysOnTop`, `Diversity_AutoAdjust`, `Diversity_DialogGeometry`
- `AntennaConflictPolicy` ("AutoSafe" / "AlwaysAsk" / "AutoAny")
- `PanLayoutDialogGeometry`

### Settings schema version bump

`SettingsSchemaVersion` bump from current (v5) to **v6** when 3F lands. Migration: per-MAC settings without per-slice-per-band sample rate get the SKU default (192 kHz clamped to ladder). No data lost; new keys default-populated.

---

## 13. Bench Verification Matrix Structure

Matrix lives at `docs/architecture/2026-05-26-phase3f-verification/README.md` (created in implementation phase). Per-SKU rows × per-feature columns.

### Rows (SKUs to test)

1. HL2 (1-slice) - native bench available
2. HermesII (2-slice) - if available
3. ANAN-G2 / Saturn (5-slice) - primary bench
4. ANAN-G2E / HermesC10 (5-slice) - pending G2E hardware per v0.5.2 status
5. ANAN-7000DLE / 8000DLE (5-slice) - if available

### Columns (features per SKU)

- Slice creation up to maxSlices
- Slice removal restores chain BPF correctly
- Per-slice sample rate change (each ladder value, including 1536 kHz on P2)
- Slice band change (per-band memory load)
- AetherSDR overlay (same-band slices on one pan)
- AetherSDR overlay (cross-band slices showing flag migration)
- Antenna routing single-antenna (no chain switching)
- Antenna routing 2-antenna 2-ADC (auto-distribute)
- Antenna routing 3+ antenna conflict (operator dialog)
- TxSliceArbiter handoff with MOX-drop guard
- Wideband extension via zoom-out gesture
- Wideband extension auto-bypasses Alex BPF
- Click-in-wing retunes DDC (latency check)
- Click-in-listenable-island retunes slice (instant)
- Alex hybrid policy: same-band no badge
- Alex hybrid policy: multi-band auto-bypass + WIDE badge
- Alex hybrid policy: operator ForceBand override
- Alex hybrid policy: operator ForceBypass override
- Alex policy popup accessible from WIDE badge
- Alex policy popup accessible from CH tag
- Diversity enable/disable on Slice A
- Diversity phase + gain controls
- Diversity radar widget renders + updates
- Diversity 8 memory slots (store + recall + per-band)
- Diversity quick-nudge buttons (7 of them)
- Diversity Sync Slice A → Slice B (when B exists)
- Diversity Link ATT
- Diversity direction-finding math (visual check)
- PS during MOX with multiple user slices (pause + auto-resume)
- PS during MOX with diversity active (DIV pauses)
- Layout switch at runtime (1 → 2v → 12h → 2x2 → 1)
- Floating pan window detach + dock-back
- Pan layout persistence across launches
- +PAN dropdown menu opens + functions
- +RX button on overlay panel + status-bar reject on cap exceeded
- Right-click VFO flag context menu
- Antenna auto-switch toast with Undo
- TX-bound re-route confirmation dialog
- Bottom-bar CH state indicators update live
- Settings restore: TxBoundSliceIndex
- Settings restore: per-slice per-band sample rate
- Settings restore: diversity state + memories

Approximately 40 verification points × 5 SKUs = 200 cells. Realistic to verify in phases (G2 first, others as hardware available).

### Documented limitations (carry-overs)

- HL2 wideband: out of scope (P1 mechanism differs). Tracked as 3F-W follow-on.
- HermesII PS-with-diversity: hardware can't do it (only 2 DDCs total). Not a NereusSDR limitation, document in release notes.
- Metis: no PS support (board never had it). Diversity dialog hidden via capability gate.

---

## 14. Build Sequence + Sub-epic Estimates

### Sub-epic ordering

**Sub-epic A: Foundation (~5 days)**
- `WdspEngine` channel array growth from 2 to `maxSlices`
- `ReceiverManager.m_hwToLogical` map growth
- `BoardCapabilities` extensions (`maxSlices`, `hasDiversity`, `widebandAdcs`)
- `SliceModel` new Q_PROPERTYs (sliceLetter, chainIndex, ddcIndex, sampleRateHz, diversityEnabled, widebandExtensionRequested, psPaused)
- `SettingsSchemaVersion` v6 migration
- Unit tests for codec extension (5-slice combinations)

**Sub-epic B: Codec + Chain (~4 days)**
- Extend per-board codec `applyPureSignalDdcConfig` → `applyDdcAssignment(SliceConfig[5])` with backward-compat shim
- Antenna-driven codec routing logic (chain match, idle claim, conflict escalation)
- `AlexController` per-ADC state machine (`recomputeBpf`, mode/effective enums, reasonText)
- 16-row event trigger matrix wiring
- Tests for chain assignment + BPF state machine

**Sub-epic C: TxSliceArbiter + lifecycle (~3 days)**
- `TxSliceArbiter` class + handoff sequence + MOX-drop guard
- `RadioModel::addSliceOnPan(QString panId)`, slice destroy lifecycle
- `VfoWidget::m_txBadge` click handler wiring
- Status-bar reject toast on cap exceeded
- TxBoundSliceIndex persistence + restore

**Sub-epic D: Pan layouts + multi-pan UI (~5 days)**
- `PanadapterStack` 5-template implementation (port AetherSDR `applyLayout`)
- `PanadapterApplet` per-pan container with title and source-aware FFT routing
- `FFTRouter` (receiver → pan fan-out)
- `wirePanadapter()` signal routing (disconnect-before-removal pattern)
- `PanFloatingWindow` port from AetherSDR
- Pan Layout dialog
- +PAN dropdown menu + bottom-bar cluster activation + chain stacked indicators

**Sub-epic E: Per-pan SpectrumStatusOverlay + UI atlas surfaces (~4 days)**
- New `SpectrumStatusOverlay` widget (mirror of SpectrumOverlayPanel pattern)
- Right-click VFO flag context menu
- Antenna picker submenu with chain-consequence hints
- Antenna auto-switch toast + TX-bound confirmation dialog
- Filter Policy popup (per-chain)
- Setup → Hardware → DDC Routing page
- Setup → Antenna Control "Conflict policy" group

**Sub-epic F: Wideband extension (~6 days)**
- `packetbuf[23]` enable byte plumbing in `composeCmdRx`
- P2 wideband receive path decode (replace stub at `P2RadioConnection.cpp:1608`)
- Per-ADC frame accumulator (32 packets × 512 samples)
- `WidebandFftEngine` (real-input r2c, 16384-pt)
- `SpectrumWidget::paintExtendedPan` with listenable island + wing rendering + dashed boundaries
- Zoom-gesture coordination (auto-bypass BPF when extending)
- Per-pan "Extended view" toggle
- Click-in-wing → DDC retune; click-in-island → slice retune

**Sub-epic G: Diversity full port (~10 days)**
- Per §8 day-by-day breakdown above

**Sub-epic H: Bench verification + polish (~5 days)**
- Per-SKU matrix exercise (focus on G2 first)
- Bug triage and fixes
- Tooltip + status-string polish
- Documentation: release notes + user-visible migration notes

### Estimate

Approximately 42 working days for the full 3F + 3F-DIV epic. Realistic calendar with code review + bench iteration: 2-3 months.

### Ordering rationale

A precedes everything (foundations). B + C can parallel after A. D + E in sequence after B+C (D builds pan layout, E adds the per-pan widgets). F can parallel with G (separate concerns: wideband stream vs diversity DSP). H last.

---

## 15. References

### Thetis (v2.10.3.15)

- `Project Files/Source/Console/console.cs:8186-8538` - UpdateDDCs state machine (8 states per board family, byte-faithful behaviour we preserve for RX1/RX2 case)
- `Project Files/Source/Console/console.cs:8540-8640` - GetDDC lookup
- `Project Files/Source/Console/console.cs:15072-15130` - rx_adc_ctrl1/2/P1 properties + GetADCInUse
- `Project Files/Source/Console/clsHardwareSpecific.cs:85-185` - SetRxADC per board type
- `Project Files/Source/Console/setup.cs:849-850` - sample rate ladder arrays (p1_rates, p2_rates)
- `Project Files/Source/Console/DiversityForm.cs` (2937 lines) - full Diversity UI source
- `Project Files/Source/Console/DiversityForm.cs:2390-2440` - SensitivityAtAngle math for radar widget
- `Project Files/Source/Console/dsp.cs:609-619` - SetEXTDIVRun/Nr/Output/Rotate P/Invokes
- `Project Files/Source/ChannelMaster/network.c:550-603` - wideband ADC stream receive
- `Project Files/Source/ChannelMaster/network.c:880-882` - wideband enable byte (packetbuf[23])
- `Project Files/Source/ChannelMaster/netInterface.c:1458-1463` - wideband default config

### mi0bot-Thetis (v2.10.3.13-beta2)

- `Project Files/Source/Console/setup.cs:850-851` - HL2 384 kHz P1 rate extension via `include_extra_p1_rate`
- `Project Files/Source/Console/console.cs:8409-8488` - HL2 PS rate carveout (rx1_rate instead of ps_rate)

### AetherSDR (v0.8.19-10-g0cd4559a)

- `src/gui/PanadapterStack.h/.cpp` - 12-layout template implementation, floating window support
- `src/gui/PanadapterStack.cpp:480-518` - `floatPanadapter` + `PanFloatingWindow` integration
- `src/gui/PanLayoutDialog.cpp` - 5-button layout picker dialog
- `src/gui/PanadapterApplet.h/.cpp` - per-pan container pattern
- `src/gui/PanFloatingWindow.h/.cpp` - detach-to-second-monitor
- `src/gui/SliceColors.h` - cyan/magenta/green/yellow slice colors
- `src/gui/MainWindow.cpp:6849-6859` - `+RX` button with cap check + status-bar reject pattern
- `src/models/SliceModel.h:14-20` - slice Q_PROPERTY list (no outputDevice, no vfoB - AetherSDR-faithful baseline)
- `src/models/SliceModel.h:68-71, 281-284` - per-slice RIT/XIT (one of each per flag)
- `src/core/AudioEngine.h:304-422` - single QAudioSink, single output device model
- `src/models/RadioModel.h:264, 292-293, cpp:537-553, 2377-2415` - addSliceOnPan + sliceAdded/sliceRemoved signals

### NereusSDR existing infrastructure (audited during brainstorm)

- `src/core/codec/IP1Codec.h`, `IP2Codec.h`, per-board codec subclasses - DDC orchestration already shipped for PureSignal (Phase 3M-4)
- `src/core/ReceiverManager.{h,cpp}` - hwToLogical map (currently 2 entries, needs growth)
- `src/core/P2RadioConnection.h:537-550` - RxState struct (already sized for 12 streams)
- `src/core/P2RadioConnection.h:623-626` - wideband config (samplesPerPacket, sampleSize, updateRate, packetsPerFrame already wired)
- `src/core/P2RadioConnection.cpp:1608-1617` - wideband receive stub ("not used yet")
- `src/core/P2RadioConnection.cpp:2141-2145` - wideband config write (packetbuf[24..28] written; [23] enable byte NOT written)
- `src/core/audio/MasterMixer.h:23-29` - per-slice gain/pan/mute keyed by sliceId (already supports N)
- `src/gui/StyleConstants.h` - palette + Style::* style helpers (used throughout for UI fidelity)
- `src/gui/widgets/VfoWidget.h:362, .cpp:615-624, 2177-2180` - m_txBadge (visual surface present, click handler missing)
- `src/gui/widgets/VfoWidget.cpp:2930-2940` - sliceColor() (cyan/magenta/green/yellow A/B/C/D)
- `src/gui/MainWindow.cpp:3930-4180+` - buildStatusBar (+PAN placeholder at line 3990 marked NYI Phase 3F)
- `src/gui/SpectrumOverlayPanel.h` - top-left collapse + 10 menu buttons pattern (template for SpectrumStatusOverlay top-right)
- `src/gui/SpectrumWidget.cpp:1977-2020` - drawTextOverlay corner-text rendering
- `src/core/SampleRateCatalog.h:104, .cpp:117-145` - kDefaultSampleRate = 192000, per-board allowed-rate resolution
- `src/core/accessories/AlexController.h:68-145` - existing per-band antenna API (extends with per-ADC BPF state)
- `src/core/BoardCapabilities.{h,cpp}` - per-SKU capability rows (extends with maxSlices, hasDiversity, widebandAdcs)
- `src/models/SliceModel.h:172-199` - existing Q_PROPERTY list (rich; gains 7 new properties in 3F)
- `src/models/RadioModel.h:1213, .cpp:8948` - setSplit() stub (deleted in 3F cleanup)

### Project memory referenced

- `feedback_port_aether_workflow_enhance_dsp.md` - AetherSDR UX 1:1, mechanics local
- `feedback_hardware_ladders_research_first.md` - read sample rate ladders from source, never extrapolate
- `feedback_audit_codebase_before_claiming.md` - grep before claiming what's missing (extended to upstream comparisons)
- `feedback_mi0bot_authoritative_for_hl2.md` - HL2 features port from mi0bot fork
- `feedback_thetis_attribution_rules.md` - byte-for-byte header preservation
- `feedback_inline_cite_versioning.md` - `[v2.10.3.15]` stamps on every inline cite
- `project_nereussdr_slice_architecture.md` - Slice A/B/C/D divorce from RX1/RX2 (architectural divergence from Thetis)

### Architectural divergences from Thetis (documented)

1. **5-slice extension on 2-ADC boards** vs Thetis's 2-RX cap. RX1/RX2 byte layout preserved exactly (DDC2/3); Slices C/D/E fill Thetis's idle DDC4-6 slots. Operators with 1-2 slices see identical wire behaviour.
2. **No VFO B / split** vs Thetis's `chkVFOSplit`. AetherSDR-faithful: per-slice RIT/XIT for offset, second slice for big retune.
3. **Single global audio output** vs Thetis's RX1/RX2 audio bus routing. AetherSDR-faithful: per-slice pan/gain/mute via `MasterMixer`.
4. **Wideband as extended pan** vs Thetis's separate wideband form. NereusSDR-original innovation; preserves Thetis WDSP `Spectrum()` callback semantics for the FFT side.
5. **Antenna-driven ADC routing** vs Thetis's implicit RX1=ADC0 / RX2=ADC1 coupling. NereusSDR codec abstracts the mapping; operators think in antennas, not ADCs.
6. **AetherSDR overlay model** vs Thetis's fixed per-RX panadapter coupling. Slices appear as overlays on any pan whose visible range includes their frequency.

All divergences are explicit, documented above, and either AetherSDR-faithful (where AetherSDR's model is cleaner for slice-oriented operation) or NereusSDR-original (where the extension is purely additive into Thetis's unused capacity).

---

## Sub-Epic A implementation note (landed 2026-05-26)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md`. The foundation tasks shipped with no operator-visible behaviour change (single-slice operation unchanged). Additions:

- `BoardCapabilities.maxSlices` (per-SKU user-facing slice cap, distinct from existing `maxReceivers` = DDC count)
- `BoardCapabilities.widebandAdcs` (per-ADC wideband support count)
- `SliceModel` 7 new Q_PROPERTYs: `sliceLetter`, `chainIndex`, `ddcIndex`, `sampleRateHz` (with per-band persistence), `diversityEnabled`, `widebandExtensionRequested`, `psPaused`
- `RadioModel::maxSlices()` accessor (BoardCapabilities-driven, returns 1 when disconnected)
- `SettingsSchemaVersion` v5 → v6 (additive, no key renames; main.cpp:279 bumped)

### Commits (12 task boundaries, all GPG-signed)

| SHA | Task | Summary |
|---|---|---|
| 8a98eb55 | 1 | add maxSlices + widebandAdcs to BoardCapabilities struct |
| af39fc70 | 2 | populate maxSlices per SKU per design §2 |
| 85322883 | 3 | populate widebandAdcs per SKU (2 for P2, 0 for P1) |
| 86a116fc | 4 | add SliceModel::sliceLetter Q_PROPERTY (A-E slice ID) |
| 8371e2f4 | 5 | add SliceModel::chainIndex Q_PROPERTY (which Alex chain hosts slice) |
| fc7390a0 | 6 | add SliceModel::ddcIndex Q_PROPERTY (codec-assigned, -1=unassigned) |
| 4498fdd3 | 7 | add SliceModel::sampleRateHz Q_PROPERTY with per-band persistence |
| b8378474 | 8 | add SliceModel::diversityEnabled Q_PROPERTY (Slice-A only) |
| 099fb03a | 9 | add SliceModel::widebandExtensionRequested Q_PROPERTY (zoom-derived) |
| 191f1c75 | 10 | add SliceModel::psPaused Q_PROPERTY (driven by PureSignal during MOX) |
| d548692e | 11 | add RadioModel::maxSlices() accessor (BoardCapabilities-driven) |
| 8f0fa417 | 12 | bump SettingsSchemaVersion v5 to v6 (additive, no key renames) |

### Test coverage

4 new test files / 32 test cases. All green at Sub-Epic A close.

- `tst_board_capabilities_phase3f.cpp` (Tasks 1-3)
- `tst_slice_model_phase3f_properties.cpp` (Tasks 4-10)
- `tst_radio_model_max_slices.cpp` (Task 11)
- `tst_settings_schema_v6_migration.cpp` (Task 12)

### Discovered during implementation

- **`WdspEngine` is already N-channel-capable** via `std::map<int, std::unique_ptr<RxChannel>>` so no "growth" required. Design originally said "grow from 2 to maxSlices"; the audit found N-capable already. Sub-Epic B and beyond can allocate channels at any index without WdspEngine changes.
- **`BoardCapabilities.maxReceivers` is intentionally distinct from `maxSlices`.** maxReceivers = total DDC count (e.g. 7 on G2). maxSlices = user-facing cap after PS+diversity reservation (e.g. 5 on G2). Both fields coexist; downstream code reads whichever fits its purpose.
- **`BoardCapabilities.hasDiversityReceiver` already exists per SKU** (`true` for 2-ADC boards, `false` for 1-ADC). Design doc references to `hasDiversity` should be read as `hasDiversityReceiver`; no new field needed.
- **`ReceiverManager.m_hwToLogical` map growth** (design §4 file touches) is deferred to Sub-Epic B where the codec first emits multi-slice DDC assignments. The map is a `QMap` that grows naturally as entries are added; no Sub-Epic A change required to keep single-slice operation working.
- **AnvelinaPro3 SKU clarification.** Initial Task 2 dispatch prompt mistakenly listed AnvelinaPro3 as P1 4-slice; design §2 (correct) lists it as 5-slice because AnvelinaPro3 maps to `kOrionMkII` at the wire layer (P2 dual-ADC). The implementer subagent caught the discrepancy via source-first audit and used the correct value 5. Good demonstration of source-first discipline overriding a noisy dispatch prompt.
- **`kDefaultSampleRate` lives in `namespace NereusSDR`, not in a `SampleRateCatalog` namespace.** `SampleRateCatalog` is the filename, not a namespace. `SliceModel::m_sampleRateHz` initializer uses the bare unqualified name.
- **Settings schema v6 migration is purely additive.** No key renames, no defaults to populate at migration time. The migration block is empty (placeholder) and the existing `setValue(versionKey, ...)` at the tail of `ensureSettingsAtVersion` handles the version bump itself. New per-slice per-band keys populate lazily on first write via SliceModel's existing per-band save path.

### Clangd stale-index notes

During implementation, the operator's LSP server (clangd) surfaced numerous false-positive `'X' is not a member of 'BoardCapabilities/SliceModel/RadioModel'` errors after each commit because clangd's compile_commands.json index hadn't refreshed. Every diagnostic was verified by grep + ctest before being dismissed. No actual code issues. Reload of the LSP server picks them up on the operator's side.

### Sub-Epic B readiness

Sub-Epic B (Codec + Chain) can now begin per `docs/architecture/2026-05-26-phase3f-sub-epic-b-codec-chain-plan.md`. Foundation pieces it depends on are all in place: BoardCapabilities maxSlices + widebandAdcs, SliceModel chainIndex / ddcIndex / sampleRateHz / diversityEnabled for codec to read, RadioModel::maxSlices() for codec to consume cap, schema v6 ready for additional keys.

---

## Sub-Epic C implementation note (landed 2026-05-27)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-c-tx-arbiter-lifecycle-plan.md`. Single-slice operation unchanged.

Operator-visible changes:

- `RadioModel::addSliceOnPan(QString panId)` creates slices up to `maxSlices`, emits `sliceAdded(int)` on success or `sliceAddRejected(QString reason)` when the cap is hit. The reason string is SKU-aware ("`<SKU>` supports a maximum of N slices").
- `RadioModel::removeSlice(int)` refuses to remove the last surviving slice, hands TX off to the fallback slice if the victim is TX-bound, then removes.
- `VfoWidget` TX badge is now clickable. Click emits `txHandoffRequested(int sliceIndex)`; MainWindow forwards to `RadioModel::txSliceArbiter()->requestHandoff()`.
- MainWindow status bar shows `"TX > Slice X"` toast on a successful handoff and the SKU-aware reject reason on overflow.
- `TxSliceArbiter` drops MOX (RF-safe) before flipping the TX-bound slice and persists the current index under `hardware/<mac>/TxBoundSliceIndex`.

Discovered during implementation:

- **`MoxController::setMox(false)` is synchronous in the Qt event-loop sense**; the arbiter does not need a `QEventLoop`-based wait around the MOX drop.
- **`VfoWidget` already had `setSliceIndex` / `sliceIndex` from 3G-10 Stage 1**; the Sub-Epic C plan's inline-setter spec was redundant. Only `simulateTxBadgeClick`, `txHandoffRequested`, and `onTxBadgeClicked` were genuinely new.
- **`RadioModel::sliceAdded` / `sliceRemoved` already existed with `int` signatures** (not `SliceModel*`); only `sliceAddRejected(QString)` was added.
- **`RadioModel::addSlice()` already existed**; `addSliceOnPan` delegates to it to keep MoxController VOX hookup + active-slice bookkeeping in one place.
- **The deprecated `RadioModel::setSplit(int, bool)` stub had one production caller** (TciProtocol's `handleSplitEnableCommand` dispatcher) and two mock callers (TestMockRadioModel still implements its own `setSplit` for the init-burst test). Task 11 dropped the `QMetaObject::invokeMethod(m_radio, "setSplit", ...)` call from the dispatcher; the broadcast notification still fires so WSJT-X / N1MM / Log4OM see the wire-protocol round-trip ("Split Operation: None/Fake It" remains the supported configuration). `RadioModel::split(int) const` stays at false so init-burst output is stable.

### Sub-Epic D readiness

Sub-Epic D (Pan layouts + multi-pan UI) can now begin: `PanadapterStack`, `PanadapterApplet`, `FFTRouter`, `+RX` button on `SpectrumOverlayPanel`. The slice lifecycle plumbing it depends on (addSliceOnPan, removeSlice, TX handoff, persistence) is all in place.

---

## Sub-Epic D implementation note (landed 2026-05-27)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-d-pan-layouts-plan.md`. 20 tasks across 19 commits (T17 manual smoke deferred to Sub-Epic H bench; T20 PR open deferred per single-PR strategy).

Operator-visible changes:

- `PanadapterStack` widget with 5 layout templates: "1" (Single), "2v" (Stacked), "2h" (Side-by-Side), "12h" (Wide + 2), "2x2" (Grid).
- `PanadapterApplet` per-pan container that hosts one SpectrumWidget + tracks associated slices for overlay rendering.
- `FFTRouter` core class fans out FFT frames from receivers to subscribed pans (1 receiver to N pans).
- `PanFloatingWindow` detaches any pan to a top-level window for multi-monitor setups; dockRequested signal returns it to the stack.
- `PanLayoutDialog` 5-tile visual picker dialog.
- `+PAN` button in the bottom status bar (formerly NYI placeholder) opens a dropdown menu with Add Slice (per-letter cap-gated), Layout (5 templates with current checkmarked), and Float Active Pan.
- View menu gains Pan Layout (Ctrl+L), Add slice (Ctrl+R), and Float active pan entries.
- Per-chain CH 0 / CH 1 stacked indicators in the bottom bar reflect live AlexController BPF state (Filtered = green, WidebandLocked / Bypass = amber). CH 1 hidden on single-ADC SKUs.
- Layout state (PanLayoutId + splitter sizes) persists across app launches via AppSettings.

Architectural shift:

- MainWindow's single `m_spectrumWidget` is replaced by `m_panStack` (PanadapterStack containing N PanadapterApplet instances).
- `activeSpectrumWidget()` accessor provides backward compatibility for the 125 existing call sites; eventually callers should migrate to per-pan addressing via `m_panStack->panadapter(panId)->spectrumWidget()`.
- RadioModel owns a `FFTRouter*` (`m_radioModel->fftRouter()`); MainWindow wires `sliceAdded` and `sliceRemoved` to bind slices to their initial pan + register pan-to-receiver in the router.
- `disconnectPanadapter(panId)` helper on MainWindow follows the AetherSDR issue #242 pattern: disconnect signals before deletion to avoid lambda crashes during teardown. Consumers wire in Sub-Epic E (per-pan close button + right-click Remove).

Discovered during implementation:

- `m_spectrumWidget` had 125 references in MainWindow.cpp; a global `replace_all` to `activeSpectrumWidget()` worked because the field was never written outside its single constructor.
- `PanadapterStack::addPanadapter` auto-attaches the first pan to the root splitter when `m_pans.size() == 1`; subsequent applyLayout calls handle the rest. Idempotent because Qt's `QSplitter::addWidget` on an already-attached child re-parents to end of the splitter.
- The 2x2 test surfaced an orphan-cleanup gap: applyLayout with a different pan-id set leaves previous pans alive. Fixed by adding a QSet-based orphan sweep at the top of applyLayout that removePanadapter()s any existing id not in the new layout's id list.
- `PanFloatingWindow` reparents the applet via QVBoxLayout::addWidget; the floating-window destructor cleans up the applet automatically, so test cleanup uses `delete w;` alone (not `delete applet;` after, which would double-free).
- `setSplit` from Sub-Epic C cleanup had a real TCI dispatcher caller; preserved the wire-protocol broadcast while dropping the no-op method invocation. (Logged in Sub-Epic C retrospective.)

### Sub-Epic E readiness

Sub-Epic E (UI atlas surfaces: SpectrumStatusOverlay, antenna picker per pan, Filter Policy popup, Setup DDC Routing) can now begin.

---

## Sub-Epic E implementation note (landed 2026-05-27)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md`. 16 tasks across 11 commits (T16 PR open deferred per single-PR strategy; T8-T13 bundled into 2 commits per plan's condensed format).

Operator-visible changes:
- `SpectrumStatusOverlay` paint-based per-pan badge widget (slice letter A/B/C/D color-coded, freq.kHz + mode, CH N tag, optional pills TX/WIDE/DIV/PS HOLD). Embedded in every `PanadapterApplet` top-right corner via `resizeEvent` positioning.
- Right-click VFO flag opens a 5-section context menu: Make TX (emits txHandoffRequested), Antenna >, Sample rate > (48k to 1.5M), Diversity > (disabled until Sub-Epic G), Filter policy... (opens FilterPolicyDialog), Remove slice.
- `FilterPolicyDialog` modal accessible via WIDE badge click or CH tag click. Shows current `AlexController::adcState(chainIndex)` effective + reasonText. Operator can override BPF mode (Auto / ForceBand / ForceBypass). HPF checkbox scaffolded for Sub-Epic G.
- `AntennaPickerMenu` right-click submenu with chain-consequence hints ("Chain N - current", "(switches chain)"). Limits options by `BoardCapabilities::antennaInputCount`. RX-only EXT1/EXT2/BYPS rows after separator.
- `AntennaSwitchToast` non-blocking bottom-right widget for auto-switch notification with 8-second auto-dismiss + UNDO button.
- `TxBoundConfirmDialog` modal shown when adding a slice would force re-routing the TX-bound chain to a different antenna. Three outcomes: Cancelled / UseExistingAntenna / ConfirmReroute.
- New Setup -> Hardware -> DDC Routing page (skeleton; per-DDC override table lands in a polish iteration when AppSettings override schema is finalized).
- Existing Setup -> Hardware Config -> Antenna/ALEX -> Antenna Control tab gains a Conflict policy group (3 radio buttons + AppSettings `Antenna_ConflictPolicy` persistence; Auto / Warn / Block).

Discovered during implementation:
- `VfoWidget` doesn't hold `m_currentSlice` / `m_alexController` / `m_caps` members; the AntennaPickerMenu integration into VfoWidget right-click is deferred. Menu is standalone-constructible today; full wire-up is a separate refactor (out of Sub-Epic E scope).
- `RadioModel::antennaAutoSwitched` signal doesn't exist yet; AntennaSwitchToast consumer wire-up deferred. Toast is standalone-constructible.
- `SliceModel` has no public `bandLabel()` accessor (private member only); AntennaPickerMenu derives band label from `Band::bandFromFrequency()` + `Band::bandLabel()` free functions instead.
- `RadioModel::alexControllerMutable()` returns a non-const ref, so dialogs that need to call `setBpfMode` use that accessor (no const_cast needed).
- The "AntennaControlPage" referenced by the plan is actually `AntennaAlexAntennaControlTab` (a nested sub-sub-tab under Hardware Config). New conflict policy group landed via a `buildConflictPolicyGroup(QVBoxLayout*)` private builder matching the existing UI build pattern.
- DDC Routing setup-page registration uses `add(category, label, SetupPage*)` lambda inside SetupDialog ctor.

Sub-Epic F (Wideband extended pan: real-IQ tuning within the wideband DDC, see-beyond ddc-dial rendering) can now begin.

## Sub-Epic F implementation note (landed 2026-05-27)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-f-wideband-plan.md`. 16 tasks across 11 land commits + 1 plan-correction commit (T16 PR open deferred per single-PR strategy; T7-T10 visual rendering polish explicitly deferred to post-bench).

Operator-visible changes:
- Wideband ADC data path is live end-to-end: P2 wideband packet (UDP ports 1027 to 1034 = ADC0 to ADC7) -> WidebandFrameAccumulator (32 packets x 512 samples -> 16384 floats) -> RadioModel forwards per-ADC -> WidebandFftEngine (16384-pt FFTW r2c, 8192 dBm bins) -> RadioModel emits widebandSpectrumReady -> SpectrumWidget stores via setWidebandBins.
- P2 CmdGeneral byte 23 (wb_enable mask) plumbing: setWidebandEnabled(adc, on) toggles the bit; in Connected state triggers sendCmdGeneral so the radio learns promptly. Codec-driven composeCmdGeneral (P2CodecOrionMkII) threads via new CodecContext::p2WbEnableMask.
- SpectrumWidget extendedMode state: zoom-driven auto-derive when visible bandwidth > DDC sample rate, plus operator right-click toggle on the pan (default on, persisted per panId under AppSettings Pan_<panId>_ExtendedView).
- Click-in-wing vs click-in-island disambiguation: when extendedMode is on, clicking inside the listenable island (|freq - ddcCenter| <= sampleRate/2) emits frequencyClicked (existing slice-retune path); clicking in the wing emits new ddcRetuneRequested which retunes the DDC center.
- Wideband activation chain: SliceModel::widebandExtensionRequestedChanged -> AlexController::setWidebandActive (BPF bypass per the per-ADC state machine from Sub-Epic B) + P2RadioConnection::setWidebandEnabled (radio starts streaming wb packets).

Discovered during implementation:
- The plan had a critical wire-format error in T1: it targeted composeCmdRx byte 23 for the wideband mask, but Thetis network.c:879 puts the mask in CmdGeneral byte 23 (CmdRx byte 23 is rx[1].rx_adc per Thetis network.c:1118). Source-first audit during T1 caught it; plan was corrected inline (commit c167bd75) before T1 implementation (commit 0453b889). Following the plan as written would have silently broken RX1 ADC routing the moment any user enabled an alternate ADC.
- Thetis line citations in the plan needed correction: T2 cited network.c:567-571 but the actual decode is at 566-571; T3 stub was at line 1762 not 1608 (plan was stale). Corrected in commit bodies.
- WidebandFftEngine uses FFTW_ESTIMATE not FFTW_MEASURE per codebase convention (FFTEngine.cpp:333) to avoid FFTW measurement-mutex contention with the WDSP audio thread.
- AlexController is stored by value (m_alexController) not by pointer in RadioModel, so the wideband-extension hook uses dot syntax.
- P2 connection access from RadioModel is via qobject_cast on m_connection (no separate m_p2Connection member).
- m_extendedMode auto-derive lands in BOTH setFrequencyRange (canonical bandwidth path) AND setSampleRate (DDC rate change), so any zoom or rate update rebases the decision.
- Visual rendering of wideband bins as a background fill behind the DDC island (with dashed boundary indicators) is explicitly deferred. The data path is live and operator-observable via the right-click toggle, the auto-derive, and the AlexController BPF state badge from Sub-Epic E. Polish lands after bench feedback shapes the visual treatment.

Sub-Epic G (Full Diversity port: DDC0+DDC1 sync pair, SetEXTDIVRun/Nr/Output/Rotate, AlexController gating) can now begin.

## Sub-Epic G implementation note (landed 2026-05-27, bench-minimum)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-g-diversity-plan.md`. **4 of 25 plan tasks landed** (T1 + T2 + T4-simplified + T13). The rest are explicitly deferred to post-bench polish per the user's "keep going until I can bench test" directive.

What shipped (bench-minimum Diversity):
- **T1 RxChannel WDSP External Diversity wrappers** (`setExtDivRun` / `setExtDivNr` / `setExtDivOutput` / `setExtDivRotate`). Ported from Thetis dsp.cs P/Invoke declarations. libwdsp confirmed exporting 5 EXTDIV symbols.
- **T2 SliceModel per-band diversity persistence** for 3 properties: `diversityPhaseDeg`, `diversityGainDb`, `diversityFineNullEnabled`. Per-band per-slice per-MAC under hardware/<mac>/slice<idx>/<band>/Diversity<Field>.
- **T4 (simplified) DiversityDialog skeleton** under Tools > Diversity... (Ctrl+Shift+D). Bench-minimum operator surface: Enable checkbox + Phase slider (0-360 deg, 0.1-deg precision) + Gain slider (-20 to +20 dB, 0.1-dB precision) + Status label.
- **T13 SliceModel-to-RxChannel wire** routes diversityEnabledChanged / diversityPhaseDegChanged / diversityGainDbChanged on Slice A through to WDSP wrappers. Computes I/Q rotation: input 0 identity, input 1 rotated by phase + gain.

What's explicitly DEFERRED to post-bench polish:
- **T3 8-memory diversity slots** (operator quick-recall of saved phase/gain pairs)
- **T5 DiversityRadarWidget** (custom QPainter polar sensitivity pattern, ~500 lines)
- **T6-T10 full DiversityDialog UI** (radar embedded, memory buttons, fine-null toggle, cross-fire mode, lock angle)
- **T11 Direction finding group** (antenna spacing input + calibration + derived direction label)
- **T12 wire DiversityRadarWidget to SliceModel state**
- **T14 auto-find-null** (simple gradient descent over phase/gain to minimize signal strength)
- **T15-T20 polish** (status badges, error handling, restore-defaults button, etc.)
- **T21 PS-active-during-MOX diversity pause UX** (PS HOLD overlay when PS engaged)
- **T22-T25 final integration tests + bench verification matrix**

Architectural constraints discovered:
- **WDSP pdiv[] is a 2-slot array (MAX_EXT_DIVS=2) keyed by an External Diversity id, NOT the RXA channel id.** The bench-minimum wire routes ONLY Slice A through DivId 0. A proper DivId allocator is required before Slice B / per-pan diversity can engage independent pdiv[] slots without colliding. The T13 commit body flags this for follow-up.
- **WDSP `pdiv[id]` derefs unallocated state for id >= 2 (crashes).** Bench-minimum wrappers route through m_channelId; the compile-only test (tst_rx_channel_ext_div_wrappers) uses method-pointer-take to verify signature without invoking, which sidesteps the crash. Real DSP behavior requires a live WDSP session.
- **libwdsp on macOS/Linux confirmed exporting SetEXTDIVBuffsize / SetEXTDIVNr / SetEXTDIVOutput / SetEXTDIVRotate / SetEXTDIVRun.** No platform-specific gating needed.
- **SliceModel persistence is implicit per-MAC via AppSettings singleton.** No `setMacAddress` API on SliceModel; per-MAC scope flows from the connection-state plumbing established in earlier sub-epics.

**Sub-Epic H (bench verification + polish) is next.** Bench session priorities:
1. Verify diversity engage/disengage on G2 (2-ADC SKU).
2. Verify wideband stream activates on operator zoom-out past DDC bandwidth + Alex BPF auto-bypasses.
3. Verify multi-slice add via +PAN dropdown (Slice B on G2; HL2 stays single-slice).
4. Verify TX handoff via VfoWidget badge click drops MOX before flipping.
5. Verify per-pan layout templates (1, 2v, 2h, 12h, 2x2) render correctly.
6. Verify CH 0 / CH 1 BPF state badges reflect AlexController state on band changes.

Post-bench follow-up backlog (sized for ~3-5 working days):
- Sub-Epic G T3 + T5 + T6-T10 + T11 + T12 + T14 + T15-T20 + T21 (full Diversity UI)
- Sub-Epic F T7-T10 visual polish (wideband bins rendered as background fill behind DDC island)
- Sub-Epic E T5 (AntennaPickerMenu integration into VfoWidget)
- Sub-Epic E T6 (RadioModel::antennaAutoSwitched signal + AntennaSwitchToast consumer wire)
- Sub-Epic E T7 (TxBoundConfirmDialog consumer wire-up)
- Sub-Epic E HardwareDdcRoutingPage table (per-DDC override controls)
- Sub-Epic A pre-existing latent unused-include warnings (DdcAssignment.h in RadioModel.h, SpotTableModel.h in MainWindow.cpp)

---

## Phase 3F shipping note (2026-05-27)

Phase 3F multi-pan + multi-slice landed across 8 sub-epics (A-G shipped, H bench verification pending). Single-PR strategy per user directive: ~100+ commits stacked on `feature/phase3f-sub-epic-a-foundation` for one comprehensive review pass + one merge commit.

### What ships in 3F

The headline operator-visible deliverables:

1. **Multi-pan layout** with 5 templates (Single, Stacked, Side-by-Side, Wide+2, Grid 2x2). Pan layout persists across launches. Float any pan to a second monitor via Float Active Pan action.
2. **Multi-slice (up to maxSlices per SKU)** with TxSliceArbiter enforcing the single-TX invariant. RF-safe handoff (MOX drop before TX-slice flip). Slice add/remove via +PAN dropdown or Ctrl+R.
3. **Per-pan badges** showing slice letter, freq, mode, CH N, plus optional TX/WIDE/DIV/PS HOLD pills. Right-click VFO flag for context menu (TX/Antenna/Rate/Diversity/Filter/Remove).
4. **Alex per-ADC BPF state machine** with operator override via FilterPolicyDialog (Auto / Force band / Force bypass). Bottom-bar CH 0 / CH 1 indicators reflect live BPF state.
5. **Wideband data path** end-to-end (P2 wb packets -> per-ADC accumulator -> 16384-pt FFTW r2c -> SpectrumWidget bins). Operator zoom past DDC bandwidth auto-engages extended mode. Click-in-wing retunes DDC; click-in-island retunes slice.
6. **Diversity (bench-minimum)** via Tools > Diversity Dialog (Ctrl+Shift+D). Enable checkbox + phase + gain sliders. Engages WDSP External Diversity on RXA channel 0 with DDC0+DDC1 sync pair.

### What's explicitly deferred to post-bench

- Wideband visual rendering polish (bins as background fill behind DDC island; dashed boundary indicators)
- Full Diversity UI (DiversityRadarWidget polar plot, 8-memory slots, direction finding, auto-find-null, PS-pause UX, ~21 plan tasks remaining)
- Sub-Epic E consumer wire-ups (AntennaPickerMenu in VfoWidget, AntennaSwitchToast/TxBoundConfirmDialog wires pending RadioModel signals, HardwareDdcRoutingPage per-DDC override table)
- Latent unused-include warnings (DdcAssignment.h in RadioModel.h; SpotTableModel.h in MainWindow.cpp)

### Discovered during implementation

- The largest single bug caught by source-first audit: Sub-Epic F Task 1 plan targeted composeCmdRx byte 23 for the wideband enable mask, but Thetis network.c:879 puts the mask in CmdGeneral byte 23 (CmdRx byte 23 is rx[1].rx_adc per Thetis network.c:1118). Plan was corrected inline before implementation landed.
- WDSP External Diversity pdiv[] is a 2-slot array keyed by an External Diversity id (NOT the RXA channel id). Sub-Epic G bench-minimum routes only Slice A through DivId 0; a proper DivId allocator is required for per-pan diversity to engage independent slots.
- WidebandFftEngine uses FFTW_ESTIMATE (not MEASURE) per codebase convention to avoid the FFTW measurement-mutex contention with the WDSP audio thread.
- MainWindow's single m_spectrumWidget had 125 references; refactor to m_panStack used a global replace_all to activeSpectrumWidget() because the field was never written outside its single constructor.
- PanadapterStack::addPanadapter auto-attaches the first pan to the root splitter, and Qt's QSplitter::addWidget on an already-attached child is idempotent (reparents to end), so layout-template apply works without an explicit "is this pan already attached" guard.
- 2x2 layout test surfaced an orphan-cleanup gap: applyLayout with a different pan-id set leaves previous pans alive. Fixed by adding a QSet-based orphan sweep at the top of applyLayout.

### Bench priorities (Sub-Epic H Tasks 2-4)

1. Multi-slice creation on G2 (Slice A through Slice E via +PAN dropdown).
2. Layout switching at runtime (1 -> 2v -> 12h -> 2x2 -> 1) and persistence across launches.
3. CH 0 / CH 1 BPF state indicators on band changes.
4. TX handoff via VFO TX badge click drops MOX before flipping.
5. Wideband activation: operator zoom-out past DDC bandwidth -> Alex BPF auto-bypass -> radio streams wb packets -> bins arrive in SpectrumWidget.
6. Diversity engage on Slice A (G2 only) -> DDC0+DDC1 sync pair codec change -> WDSP External Diversity runs.
7. HL2 single-slice operation unchanged (regression).

### Follow-up PRs after 3F merge

- Diversity full UI (radar widget, 8-memory, direction finding, auto-null, polish + PS-pause UX)
- Wideband visual rendering polish
- Sub-Epic E consumer wire-ups
- Per-SKU bench follow-ups per matrix results

---

## End

Spec ready for review. After approval, transitions to `superpowers:writing-plans` for implementation plan generation, then sub-epic-by-sub-epic implementation in `superpowers:subagent-driven-development` mode.
