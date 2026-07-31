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
| HermesLite2 (HL2) | 1 | 4 | DDC0-1 | **5** | 48, 96, 192, 384 | false | 0 (defer, P1 mechanism) |
| HermesLite2 RX-only | 1 | 4 | DDC0-1 | **5** | 48, 96, 192, 384 | false | 0 |
| Metis | 1 | 3 | DDC0-2 | **3** | 48, 96, 192 | false | 0 |
| Hermes (ANAN-10/100) | 1 | 4 | DDC0-3 | **4** | 48, 96, 192 | false | 0 |
| HermesII (ANAN-10E/100B) | 1 | 2 | DDC0-1 | **2** | 48, 96, 192 | false | 0 |
| Angelia (ANAN-100D) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192 | true | 2 |
| Orion (ANAN-200D) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192 | true | 2 |
| OrionMkII / 7000DLE / 8000DLE | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| Saturn / ANAN-G2 / G2_1K | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| HermesC10 / ANAN-G2E (see note) | 1 | 4 | DDC0-3 | **5** | 48, 96, 192, 384, 768, 1536 | false | 1 |
| AnvelinaPro3 | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| Andromeda | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |
| RedPitaya (P1 mode) | 1 | 4 | DDC0-3 | **4** | 48, 96, 192, 384 | false | 0 |
| RedPitaya (P2 mode) | 2 | 7 | DDC2-6 | **5** | 48, 96, 192, 384, 768, 1536 | true | 2 |

Source cites:
- Sample rate ladders: Thetis `setup.cs:849-850 [v2.10.3.15]` (P1 base + P2 array), mi0bot `setup.cs:850-851 [v2.10.3.13]` (HL2 384k extension via `include_extra_p1_rate`)
- DDC reservations: Thetis `console.cs:8186-8538 [v2.10.3.15]` (UpdateDDCs state machine)
- HL2-specific PS rate carveout: mi0bot `console.cs:8409-8488 [v2.10.3.13]`

#### Note: the ANAN-G2E is the one 1-ADC Protocol 2 SKU

It sits between Saturn and AnvelinaPro3 in this table because it shares their sample-rate ladder and their MKII BPF filter bank, **not** because it shares their DDC map. Do not group it with its table neighbours.

Until 2026-07-25 this row read `2 | 7 | DDC2-6 | 5 | true | 2`, copied wholesale from the rows around it on the reasoning that the G2E speaks Protocol 2 and therefore has the 2-ADC DDC map. That inference is wrong, and it propagated: it produced `userDdcCount = 5` and `widebandAdcs = 2` in `BoardCapabilities.cpp`, and it is the root of the Task 7c ship-blocker (commits `133c30bd`, `60ecfccc`), where `P2CodecOrionMkII::kStreamToDdc = {2,3,4,5,6}` was applied to a board whose `primaryRxDdcForBoard` returns DDC0.

What the sources actually say:

- **1 ADC.** Thetis `clsHardwareSpecific.cs:129-135 [v2.10.3.15]` model init calls `NetworkIO.SetRxADC(1)`. The 2-ADC boards in the adjacent cases (ANAN100D, ANAN200D, ORIONMKII) all call `SetRxADC(2)`. This makes `hasDiversity` false and caps `widebandAdcs` at 1, since an ADC that is not on the board cannot carry a wideband stream.
- **4 DDCs, rx1 on DDC0.** Thetis `console.cs:8387-8392 [v2.10.3.15]` groups `ANAN_G2E` with HERMES / ANAN10 / ANAN100 on `P1_rxcount = 4; nddc = 4;`, and `console.cs:8610-8642` (P2) plus `:8704-8730` (P1) group `HermesC10` with Hermes and HermesII on `rx1 = 0; rx2 = 1;`. Across every MOX, diversity and PureSignal branch, that case never enables anything above DDC1.
- The SKU's own authority, [2026-05-21-anan-g2e-port-design.md](2026-05-21-anan-g2e-port-design.md) §"Resolved values", recorded ADC count 1, Max RX 4 and Diversity **No** from the start. This table contradicted it for two months.

**On `maxSlices = 5` over 4 user DDCs.** Until 2026-07-31 this was the only row where the two differed; the HL2 rows now follow the same pattern (see the note below). The gap is deliberate: slices whose frequencies fall inside an existing DDC's window share that DDC (`SliceStreamAllocator::placeSlice`), so a slice cap above the DDC count is meaningful rather than an error. Per maintainer decision 2026-07-25 the ceiling holds at 5 across all SKUs until Phase 3F multi-slice is proven on a bench. A fifth G2E slice with no covering window is refused with an explanation, not silently dropped.

**Standing caveat.** Every DDC count in this table is Thetis's client policy, not verified silicon. Receiver count is a compile-time Verilog parameter that has shipped as 2, 4, 7 and 8 on the same board. See the `maxSlices` comment in `src/core/BoardCapabilities.h` and `docs/attribution/GATEWARE-PROVENANCE.md`. The G2E has no public gateware at all, so its DDC count in particular is the best available evidence rather than hardware truth. The ADC count is different in kind: that is a physical part, and `SetRxADC(1)` is reliable.

#### Note: the HL2 rows were derived from a source that does not cover the HL2

Corrected 2026-07-31. Both HL2 rows read `DDC0 only | 1` until then, sourced
from the "DDC reservations" cite above, ramdor Thetis `console.cs:8186-8538
[v2.10.3.15]`. That switch has no `HERMESLITE` case: five case groups, no
`default:` arm, and `HERMESLITE` appears in ramdor on seven lines across
three files: `enums.cs:128,397`, `clsHardwareSpecific.cs:353,354,393`, and
`ChannelMaster/network.h:422,444`. An HL2 leaves it with `nddc = 0`.

mi0bot is authoritative for this SKU and enables DDC1 for RX2 in two arms of
its `HERMESLITE` case (`console.cs:8425-8429` and `:8453-8457
[v2.10.3.13-beta2]`), so the row is `DDC0-1`. `maxSlices` moves to the project
ceiling of 5 because slices sharing a DDC window cost nothing.

Same failure mode as the ANAN-G2E row noted above: a value copied from a cite
that does not describe the SKU. Full analysis in
[2026-07-31-hl2-slice-cap-design.md](2026-07-31-hl2-slice-cap-design.md).

### Why DDC0/DDC1 are reserved on 2-ADC boards

Thetis reserves DDC0+DDC1 as a synced pair for PureSignal feedback (during TX) and Diversity (when enabled), regardless of operator preference. Following Thetis exactly preserves the byte-for-byte wire compatibility we already have for the RX1/RX2 case, and avoids the "your slice vanishes on TX" UX trap. User slices land on DDC2-6 (5 slots max on 2-ADC boards).

---

## 3. Slice Model

### Lifecycle: on-demand slices over a pre-allocated stream pool

**Amended 2026-07-25 (Sub-Epic I).** The original text read "Slices are created
and destroyed by operator action, not pre-allocated," which conflated the
operator-visible lifecycle with the implementation mechanics, and it assumed one
slice per DDC. Both are corrected here.

**Operator-visible lifecycle (unchanged, AetherSDR pattern):** slices are created
and destroyed by operator action, letters are assigned in creation order and
freed on destroy, and the cap check rejects past `maxSlices`.

**Implementation (corrected, Thetis / deskhpsdr pattern).** Two pools open at
connect and are never resized at runtime:

- `userDdcCount` **DDC streams**, each one hardware DDC plus its ReceiverManager
  receiver, FFTEngine, panadapter window, and noise blanker.
- `maxSlices` **WDSP RX channels**, one per possible slice.

Slices bind to streams **many-to-one**. A slice whose frequency falls inside an
active stream's window joins it and is tuned by a shift offset
(`RxChannel::setShiftFrequency`, the Thetis `RXOsc` port at
`radio.cs:1409-1420 [v2.10.3.15]`). A slice that fits no active window claims a
free DDC. When no DDC is free and none fits, the add is rejected with a message
naming the limit.

What slices sharing a stream share, and what they do not, is fixed by
ChannelMaster's `struct _rcvr` (`cmaster.h:75-82 [v2.10.3.15]`): one I/Q input,
one noise blanker (`panb` / `pnob`), one panadapter (`run_pan`), but
`audio[cmMAXSubRcvr]`, an independent audio output per slice. So co-hosted
slices share the spectrum window and the noise blanker, and keep their own mode,
filter, AGC, shift and audio.

Why pre-allocation: the original model has no upstream to port from. Thetis
opens all 10 RX channels plus TX in `CreateRadio()` (`cmaster.cs:502-516`, with
`cmRCVR=5` and `cmSubRCVR=2`), and deskhpsdr opens every receiver in one loop at
startup (`radio.c:1256-1259 [@f3d857c]`, comment: "To be on the safe side, we
create ALL receiver panels here"). Neither touches a WDSP channel at runtime.
AetherSDR's on-demand model is right for AetherSDR because its slices live on
the FlexRadio server; NereusSDR owns the DSP locally, so on-demand would mean
opening a WDSP channel on a UI click. That is also the pattern that crashed in
PR #219, where destroying live channels invalidated seven raw-pointer holders.

**Documented divergences from Thetis:**

| Aspect | Thetis | NereusSDR | Why |
|---|---|---|---|
| Slices per stream | 2 (`cmSubRCVR`) | up to `maxSlices` | Product requirement: A through D on one DDC. WDSP imposes no such cap (`MAX_CHANNELS 32`); `cmSubRCVR` is a Thetis structure choice. |
| User DDC streams | 2 in practice (RX1 / RX2) | every user DDC the SKU exposes (5 on G2) | Thetis leaves DDC4-6 idle on Saturn-class; our codecs already map them. |
| Slice leaves its window | disables Multi-RX (`console.cs:31924`) | promote to a free DDC; reject only when none free | Better operator outcome, and we have DDCs Thetis never uses. |

Idle streams cost memory only. Their DDCs stay out of the `ddcEnable` bitmask,
so the radio never streams them, exactly as Thetis's `UpdateDDCs` gates its
pre-opened channels.

Mechanics:

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

#### Producer and consumer (added 2026-06-01)

The decision tree above shipped with neither. `AlexController::notifySlicesOnAdc` had no production caller, and nothing composed a wire byte from `BpfEffective`, so the HPF on the wire came from whichever receiver `P2RadioConnection::setReceiverFrequency` saw last. Slice A on 20 m went deaf the moment slice B tuned 40 m. Reported by CT1IQI on PR #293 (2026-05-31).

Both halves now exist:

- **Producer**: `RadioModel::republishAlexAdcSlices()` groups the bound slices by the filter chain their stream sits on (`RadioModel::chainForStream`) and hands each group to `notifySlicesOnAdc`. It runs from `requestDdcAssignment()`, which is already the coalescing point for slice bind / retune / removal / antenna change, and once more on `ConnectionState::Connected`.
- **Consumer**: `RadioConnection::setAlexRxBpf(AlexRxBpf)` carries `{hpfBitsAdc0, hpfBitsAdc1}` in the Thetis HPF bit encoding, `-1` meaning "no slice on this ADC, keep the existing bits". P2 routes ADC0 into Alex0 and ADC1 into Alex1; P1 has one filter word on the wire and takes ADC0's decision only.

`Filtered` selects from the **lowest** slice frequency on the chain. With a single band on the chain that is byte-for-byte what the old frequency-derived path produced, which is what keeps the single-slice wire locks green.

##### Closing the grouping key (defect D1, 2026-07-29)

The producer originally grouped by `ReceiverManager::receiverConfig(stream).adcIndex`. Nothing
distributed streams across both ADCs at the time, and `setAdcForReceiver` was called exactly once
(receiver 0 to ADC 0), so ADC1 reported "no decision" in practice and the wire agreed with the
analysis by accident.

The antenna-driven codec routing then landed and ended that accident: a slice on an RX-only antenna
really is routed to ADC1, first on the OrionMkII family and then on the ANAN-G2. The analysis kept
counting it on chain 0, found two ranges on one chain, and bypassed **both** chains when one of them
should have been filtered.

The key is now `RadioModel::chainForStream(stream)`, the single resolver behind all three readers
(`republishAlexAdcSlices`, `sliceChainIndex`, `bypassReasonForAdc`). It reads `RadioModel::m_streamAdc`,
which `publishDdcAssignment` fills by decoding the same two ADC-control bytes the codec just composed
(`NereusSDR::adcForDdc`, the inverse of the codec's encode). Reading the assignment back rather than
re-deriving the ADC from the antenna keeps one copy of that policy, in the codec.

Held on `RadioModel` rather than read out of `ReceiverManager` because a `ReceiverConfig` exists only
for a receiver `connectToRadio` has created; without a connection there is nothing to answer from and
the model would silently report ADC0 for everything, which is the defect again. `publishDdcAssignment`
still mirrors the value into `ReceiverManager::setAdcForReceiver` so its long-standing `adcIndex` field
stops reporting 0 for a DDC the radio moved.

Two rules ride along with the key:

- **Chain, not ADC.** `chainForStream` folds any ADC index at or above `rxFilterChainCount` onto chain 0
  (§16.1.2). On a two-ADC / one-chain SKU both ADCs sit behind one preselector, so a stream on ADC1 is
  genuinely behind chain 0 and its range has to be counted there. `hpfBitsFor` refuses to compose a word
  for a chain at or above the count as well, so no board is handed a filter word for hardware it lacks.
- **Diversity.** While the DDC0/DDC1 sync pair is engaged, every slice counts on **both** chains, so the
  two band sets are identical by construction and the two decisions are identical by construction,
  including when that decision is bypass. See "Diversity: identical chains" below.

##### Diversity: identical chains

Raised by CT1IQI on PR #293: under diversity Alex0 and Alex1 must be set identical, and that may mean
identically bypassed. Diversity runs DDC0 on ADC0 and DDC1 on ADC1 as a synchronous pair sampling one
signal through two front ends. Different preselectors on the two legs means different amplitude and
group-delay responses and nothing coherent for the combiner to weight; a filtered leg against a
bypassed leg is the worst case of that.

`republishAlexAdcSlices` implements this by adding every slice to both chains' band sets while
`RadioModel::diversityActive()` is true, rather than by computing one chain and copying it to the
other. There is no mirroring step to keep in sync, and the "identically bypassed" case falls out for
free: two ranges in the shared set take both chains wide together.

Thetis cannot be ported here. It has no bypass-on-multi-band concept anywhere, and its diversity is
two receivers on two chains, so the chain count and the receiver count are the same number and the
question never arises. NereusSDR puts up to five slices on two chains. NereusSDR-original policy,
written to the reporter's stated requirement.

`P2CodecOrionMkII::buildAlex1` still mirrors Alex0's HPF onto Alex1 when ADC1 has no decision, and
that fallback must stay: it is the G2E pcap-verified behaviour for "no diversity and nothing on ADC1".
Before D1 it was also what made diversity come out right, by accident. It could never express the
bypass case, because it masks `0x20` off.

#### Divergence from Thetis: bypass, not widen

Thetis has **no** bypass-on-multi-band concept anywhere in the codebase. It handles two receivers two ways, and NereusSDR ports one of them and diverges on the other.

**Ported.** Two independent Alex wire words, one per chain. `prbpfilter` (Alex0, bytes 1432-1435) is ADC0 and is fed from `setAlex1HPF(_rx1_dds_freq)`; `prbpfilter2` (Alex1, bytes 1428-1431) is ADC1 and is fed from `setAlex2HPF(rx2_dds_freq_mhz)`.

- `console.cs:15401 + 15435-15443 [v2.10.3.15]`
- `ChannelMaster/network.c:1040-1050 [v2.10.3.15]`
- `ChannelMaster/netInterface.c:604-651 [v2.10.3.15]`

Upstream inline attribution preserved verbatim (`console.cs:15441`): `HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM`. The model gate on `setAlex2HPF` is `//N1GP`-adjacent in `setAlex1HPF` too (`console.cs:6830`, already carried in `AlexFilterMap.h`).

**Diverged.** On boards with only one filter board, Thetis widens the single HPF to the lower of the two receiver frequencies rather than bypassing it:

```csharp
// console.cs:15500-15510 UpdateAlexRXFilter [v2.10.3.15]
private void UpdateAlexRXFilter()
{
    if (!_mox)
    {
        if (!_rx2_preamp_present && chkRX2.Checked)
        {
            if (rx1_dds_freq_mhz < rx2_dds_freq_mhz) setAlex1HPF(rx1_dds_freq_mhz);
            else setAlex1HPF(rx2_dds_freq_mhz);
        }
    }
}
```

We bypass instead. Two reasons:

1. **Widening is only sound on a high-pass ladder.** A lower corner still passes the higher slice, so `min(f)` works. Thetis runs that branch exclusively on the legacy single-filter-board radios, and never on the Mk II BPF boards this report is about: ANAN-7000D / 8000D / G2 / G2-1K set `_rx2_preamp_present = true` (`console.cs:14783-14857 [v2.10.3.15]`), which makes `UpdateAlexRXFilter` a no-op on exactly that hardware. If those selections really are band-pass rather than high-pass, widening would leave the higher slice attenuated, which is the reported bug again by another route. Bypass is correct under either reading of the hardware; widening is not.
2. **Thetis tops out at two receivers on two chains.** NereusSDR puts up to five slices on two chains, so "two receivers, two filters" does not describe three slices sharing one ADC across three bands.

Operators who would rather keep a filtered chain than a wide one have `BpfMode::ForceBand`; the `WIDE` badge and its `reasonText` exist so the trade is never silent.

Note that Thetis's filter selection never consults ADC assignment at all: `GetADCInUse()` / `nRX1ADCinUse` / `nRX2ADCinUse` feed only the step attenuator, preamp linking, and ADC-overload paths. An RX2 placed on ADC0 in Thetis receives through the RX1-selected BPF1 with no compensation and no warning.

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


---

## 16. ADC / DDC Auto-Routing, Filter-Bypass as Last Resort, and the WIDE Indicator

Amendment date: 2026-07-25. Worktree `feature/phase3f-sub-epic-a-foundation` @ `b8705cb8`.
Upstream pins: Thetis `v2.10.3.15` (`3759d096`), deskhpsdr `@f3d857c`, n1gp-Anvelina_PROIII `@8e86a61`.

This section supersedes the ADC/filter parts of §4 and rule 3 of §5. It exists because a
falsification pass against Thetis, deskhpsdr and the Anvelina gateware showed that several
premises the earlier sections were built on are wrong, and because the current code would route
a Saturn-class radio using the wrong filter ladder.

---

### 16.0 The headline constraint, stated plainly

The requested outcome is "auto ADC/DDC routing with wideband (filter bypass) as the last resort".
That is achievable, but not completely, and the shortfall is a hardware property, not an
implementation gap:

> On the ANAN-G2, ANAN-G2E and Anvelina Pro 3 the RX preselector is a **band-pass** bank, not a
> high-pass bank. A band-pass chain can serve exactly one filter range at a time. When two slices
> on one chain need different ranges, engaging either range makes the other slice **deaf**, not
> merely less protected. Bypass is the only setting under which both slices still receive.

So on those SKUs "keep the filter engaged" and "keep every slice hearing" are in direct conflict
once a chain carries two filter ranges, and the router must pick one. This section proposes
"every slice keeps hearing" (bypass) as the default, and makes the alternative available as an
explicit operator pin. That is a product decision and is listed in §16.7 for confirmation.

The picture is different, and better, on the legacy **high-pass** boards (ANAN-100/100B/100D/200D,
Hermes, ANAN-10/10E, Atlas). There, selecting the corner for the **lowest** slice frequency on the
chain keeps every slice passing while retaining real protection. That is Thetis's own rule
(`console.cs:15504-15507 [v2.10.3.15]`) and on those boards multi-range sharing never needs a
bypass at all.

Two further limits worth stating before the design:

- The ANAN-G2E has **one** filter chain (`adcCount = 1`, `BoardCapabilities.cpp:646`). It has no
  second ADC to escape to, so the bypass decision is reached with **two** slices in different
  ranges, not three or more. Auto-routing cannot buy a G2E anything except the choice of which
  compromise to take.
- On the ANAN-G2 and Anvelina Pro 3 the ceiling is **two** protected filter ranges simultaneously,
  because there are two chains. A third range forces a merge and therefore a bypass on one chain.

---

### 16.1 The corrected hardware model

#### 16.1.1 What a chain is

A **chain** is one independently addressable RX preselector filter bank plus the ADC behind it.
It is the unit the Alex filter words address. The two chains are carried in two separate 32-bit
words on the P2 high-priority packet:

- **Alex0** (`prbpfilter`, packet bytes 1432-1435, `ChannelMaster/network.c:1046-1051 [v2.10.3.15]`)
  carries chain 0 (ADC0) filters **plus** the whole RX input-routing set: `_XVTR_Rx_In` bit 8,
  `_Rx_2_In` bit 9, `_Rx_1_In` bit 10, `_Rx_1_Out` bit 11, `_10_dB_Atten` bit 14
  (RX MASTER IN SEL RL22), and `_ANT_1/2/3` bits 24-26
  (`ChannelMaster/network.h:277-298 [v2.10.3.15]`).
- **Alex1** (`prbpfilter2`, packet bytes 1428-1431, `ChannelMaster/network.c:1040-1044 [v2.10.3.15]`)
  carries chain 1 (ADC1) filters, `_rx2_gnd` bit 8, a bypass, and `_TXANT_1/2/3` bits 24-26.
  The bit positions that hold RX input routing in Alex0 are **reserved and unnamed** in Alex1
  (`ChannelMaster/network.h:328-346 [v2.10.3.15]`).

Consequence, and this is load-bearing for §16.1.6: **there is no second RX antenna selector.**
Chain 1 has filters of its own and no input selector of its own.

Independently confirmed in the Anvelina Pro III gateware, which labels the ADC1 field
"Rx1 filters" and puts antenna selection only in the TX word
(`n1gp-Anvelina_PROIII High_Priority_CC.v:257,259`, duplicated in `Angelia_Protocol_2_v12
High_Priority_CC.v:185`).

#### 16.1.2 How many chains a SKU has (not `adcCount`, not `hasAlex2`)

Thetis drives a second chain for exactly one list of models
(`console.cs:15435-15443 [v2.10.3.15]`): ORIONMKII, ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K,
ANVELINAPRO3, REDPITAYA. Resolving those through `clsHardwareSpecific.cs:143-190 [v2.10.3.15]`
gives exactly `HPSDRHW.OrionMKII` and `HPSDRHW.Saturn`, and every model under those two HW values
is in the list. So chain count **is** expressible as a per-`HPSDRHW` row:

```
rxFilterChainCount = 2  when board == HPSDRHW::OrionMKII || board == HPSDRHW::Saturn
                        || board == HPSDRHW::SaturnMKII || board == HPSDRHW::Andromeda
rxFilterChainCount = 1  otherwise
```

The last two are NereusSDR rows upstream cannot answer for, added to this rule when the field was
implemented (defect D4, 2026-07-29):

- **`HPSDRHW::SaturnMKII`** appears once in all of Thetis (`enums.cs:399 [v2.10.3.15]`,
  `SaturnMKII = 11, // ANAN-G2: MKII board?`) and no `HPSDRModel` resolves to it, so
  `console.cs:15435-15443` has nothing to say about it. `kSaturnMKII` is a Saturn-derived row for the
  ANAN-G2 MkII board revision and dispatches to `P2CodecSaturn`, which antenna-routes to ADC1. Two is
  the value that keeps the wire and the filter analysis agreeing there.
- **`HPSDRHW::Andromeda`** has no Thetis `HPSDRHW` entry at all (the enum ends at `HermesC10 = 20`),
  is a NereusSDR-original SKU slot at integer 21, and its row is derived from `kSaturn` throughout.
  Same reasoning, same value. Revisit with the rest of that row when Andromeda hardware specs land.

Declaring one chain on either would recreate defect D1 for that SKU: the codec would route an
RX-only-antenna slice to ADC1 while the analysis folded it back onto chain 0.

`adcCount` does **not** predict this. ANAN-100D and ANAN-200D are `SetRxADC(2)`
(`clsHardwareSpecific.cs [v2.10.3.15]`) yet are absent from the `setAlex2HPF` list, and
`SetAlexLPFBits` copies Alex0 into Alex1 under mask `0xf8ff0000`
(`ChannelMaster/netInterface.c:692-693 [v2.10.3.15]`), which covers bits 16-23 and 27-31 and
therefore never touches the HPF nibble at bits 1-6. On a 100D the Alex1 HPF nibble stays at its
`create_rnet` zero (`ChannelMaster/netInterface.c:1554-1555 [v2.10.3.15]`) forever: second word
present, second chain not driven.

`hasAlex2` does not predict it either. See §16.1.7.

#### 16.1.3 There are two ladders on the same relay bits

Same bit positions (1, 2, 3, 4, 5, 6, 12), same Thetis byte encoding
(`0x10 / 0x08 / 0x04 / 0x01 / 0x02 / 0x40`, with `0x20` = bypass), **different physical filters**
depending on the board. deskhpsdr states it outright: `alex.h:78`,
"NOTE: Anan-7000/8000 use band-pass filters here".

Thetis dispatches at `console.cs:6827-6838 [v2.10.3.15]`:

```csharp
private void setAlex1HPF(double freq)
{
    if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (HardwareSpecific.Hardware == HPSDRHW.Saturn)
       || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    {
        setBPF1ForOrionIISaturn(freq);
    }
    else
    {
        setAlexHPF(freq);
    }
}
```

So:

```
chain 0 ladder = Bpf1      when board in {OrionMKII, Saturn, HermesC10}
chain 0 ladder = LegacyHpf otherwise
chain 1 ladder = Bpf1      ALWAYS
```

Chain 1 is unconditional because `setAlex2HPF` has no hardware branch at all
(`console.cs:7069-7071 [v2.10.3.15]`), matching deskhpsdr's alex1 word
(`new_protocol.c:1347-1361 [@f3d857c]`). **Do not copy the chain 0 dispatch onto chain 1.**

#### 16.1.4 The real boundaries

**Band-pass ladder (Bpf1).** Thetis designer defaults, decoded from
`setup.Designer.cs:24982-25522 [v2.10.3.15]`, reproduced to the digit by deskhpsdr's constants at
`alex.h:116-122 [@f3d857c]` and its board branch at `new_protocol.c:1314-1327 [@f3d857c]`:

| Range | MHz (start .. end) | Bits (Thetis `SetAlexHPFBits`) | deskhpsdr name |
| --- | --- | --- | --- |
| 0 | 1.5 .. 2.099999 | `0x10` | `ALEX_ANAN7000_RX_160_BPF` |
| 1 | 2.1 .. 5.499999 | `0x08` | `ALEX_ANAN7000_RX_80_60_BPF` |
| 2 | 5.5 .. 10.999999 | `0x04` | `ALEX_ANAN7000_RX_40_30_BPF` |
| 3 | 11.0 .. 21.999999 | `0x01` | `ALEX_ANAN7000_RX_20_15_BPF` |
| 4 | 22.0 .. 34.999999 | `0x02` | `ALEX_ANAN7000_RX_12_10_BPF` |
| 5 | 35.0 .. 61.44 | `0x40` | `ALEX_ANAN7000_RX_6_PRE_BPF` |
| bypass | anything else | `0x20` | `ALEX_ANAN7000_RX_BYPASS_BPF` |

Range 3 covers 20 m, 17 m **and** 15 m on one relay. That is the fact that motivated the
"filter range, not ham band" rule and it is confirmed.

**High-pass ladder (LegacyHpf).** Defaults from `setup.Designer.cs:23832-24384 [v2.10.3.15]`:
1.8 / 6.5 / 9.5 / 13 / 20 / 50 / 61.44, matching deskhpsdr `new_protocol.c:1391-1404 [@f3d857c]`.

Three properties of the upstream ladder that the router must reproduce and that a naive
`if` ladder does not:

1. **The edges are user-editable.** Every boundary is a Setup spinner
   (`SetupForm.BPF1_1_5Start` reads `ud1_5BPF1Start.Value`, `setup.cs:5193-5251 [v2.10.3.15]`).
   The numbers above are defaults, not constants.
2. **Comparisons are inclusive on both ends with an explicit `else` bypass tail**
   (`console.cs:6972-7064 [v2.10.3.15]`). A gap between a range's end and the next range's start
   silently drops to bypass rather than clamping. The router must model the ladder as an ordered,
   gap-tolerant table with a bypass fallthrough.
3. **Each range has its own operator bypass checkbox** (`bpf1_1_5bp_bypass`, `bpf1_6_5bp_bypass`,
   and so on, `console.cs:6975, 6990, 7005, 7020, 7035, 7050 [v2.10.3.15]`), plus a whole-chain
   `alex_hpf_bypass` (`console.cs:6965-6971 [v2.10.3.15]`). A per-range bypass is a **filtered
   outcome the operator asked to be wide**, and it is not a routing failure. The router treats a
   per-range bypass as "this range's engaged bits are `0x20`" and still counts it as its own
   group for routing purposes.

#### 16.1.5 Per-SKU behaviour

| | ANAN-G2 | ANAN-G2E | Anvelina Pro 3 |
| --- | --- | --- | --- |
| Thetis `HPSDRHW` | `Saturn` (10) | `HermesC10` (20) | `OrionMKII` (5) |
| Source | `clsHardwareSpecific.cs:164-170 [v2.10.3.15]` | `clsHardwareSpecific.cs:129-135 [v2.10.3.15]` | `clsHardwareSpecific.cs:178-184 [v2.10.3.15]` |
| ADCs (`SetRxADC`) | 2 | 1 | 2 |
| `rxFilterChainCount` | 2 | 1 | 2 |
| Chain 0 ladder | Bpf1 | Bpf1 | Bpf1 |
| Chain 1 ladder | Bpf1 | n/a | Bpf1 |
| RX-only input labels | BYPS / EXT1 / XVTR (`setup.cs:20304 [v2.10.3.15]`) | BYPS / EXT1 / XVTR | BYPS / EXT1 / XVTR (`setup.cs:20406 [v2.10.3.15]`) |
| Second front end has its own step att + 6 m LNA offset | yes (`setup.cs:20205-20206, 6291 [v2.10.3.15]`) | n/a | yes |
| Diversity | yes | no (1 ADC) | yes |

Anvelina Pro 3 is **not** "identical to the G2" even though the three headline numbers match. It
rides a different `HPSDRHW`, so it takes different Thetis calibration and PA constants
(`psDefaultPeak` 0.2899 vs 0.6121, `clsHardwareSpecific.cs:307/310 [v2.10.3.15]`; RX meter cal
offset 4.841644 vs -4.476, `clsHardwareSpecific.cs:414-420 [v2.10.3.15]`), and it alone emits the
bank-17 extra OC byte at `packetbuf[1397]` (`ChannelMaster/network.c:1016-1020 [v2.10.3.15]`).
Router code must key on `HPSDRHW`, never on display name and never on an Anvelina-specific enum,
because Thetis has no Anvelina-branded HW value.

#### 16.1.6 Antenna: one selector, per band, global across chains

This refutes design rule 3 as written. There is exactly one RX antenna selection on every
OpenHPSDR radio, it is scoped **per band**, and it is shared by both chains:

- Thetis: `SetAntBits(int rx_only_ant, int trx_ant, int tx_ant, int rx_out, char tx)` takes exactly
  one `rx_only_ant` (`ChannelMaster/netInterface.c:459 [v2.10.3.15]`), and
  `private byte[] RxOnlyAnt = new byte[12]; // 1 = rx1, 2 = rx2, 3 = xv` is dimensioned by band,
  not by ADC (`HPSDR/Alex.cs:59 [v2.10.3.15]`).
- deskhpsdr: one `band->alexRxAntenna` (`band.h:68 [@f3d857c]`) feeding
  `rxant = receiver[0]->alex_antenna;` with the explicit comment
  `// ASSUMPTION: receiver[0] is associated with the first ADC`
  (`new_protocol.c:1532-1534 [@f3d857c]`).
- Saturn / G2 register API is the cleanest single artefact: the filter setter is parameterised
  per receiver, `SetAlexRXFilters(bool IsRX1, unsigned int Bits)`, while the antenna setter is not,
  `SetAlexRXAnt(unsigned int Bits)` (`deskhpsdr saturnregisters.h:356-357, 383-385 [@f3d857c]`).

**Therefore:** "a chain serves one antenna at a time" is not a rule the router can act on, because
a chain cannot serve a *different* antenna from the other chain. Rule 3 is restated in §16.2.3
step 0.

On the 7000D / 8000D / G2 / G2-1K / Anvelina Pro 3 the single selected antenna necessarily reaches
both filter banks: there is no RX 2 jack (the RX-only inputs are BYPS / EXT1 / XVTR), no second
selector, yet ADC1 has a complete BPF2 bank with its own step attenuator and its own 6 m LNA
offset, and diversity works. **This is strong convergent inference, not a cited hardware fact.**
No schematic or upstream comment states the split in words. It is therefore a bench row
(§16.7 Q9), and the router's ADC-distribution step is gated on it.

On ANAN-100D / 200D the opposite holds: ADC1 is drawn hardwired to connector C2, labelled "RX 2"
(`Path_Illustrator.cs:4380, 5009-5014, 5695 [v2.10.3.15]`), and deskhpsdr's own preset text says
so (`ddc_menu.c:231-236 [@f3d857c]`). Distributing a stream to ADC1 there would change which
physical antenna it hears. Fortunately those SKUs have `rxFilterChainCount = 1` by §16.1.2, so
the router never proposes it. The two facts line up; do not break one without rechecking the other.

Trap to avoid while implementing: "RX1" and "RX2" in the antenna grid are **rear-panel jack names**,
not receivers. `labelRXAntControl.Text = "  RX1   RX2    XVTR"` for Hermes / ANAN-10 / ANAN-10E
(`setup.cs:19886-19889 [v2.10.3.15]`), and the on-TX checkboxes confirm it:
`chkEXT1OutOnTx.Text = "RX 2 IN on Tx"` (`setup.cs:19893-19894 [v2.10.3.15]`). All three route to
Alex0, that is, to ADC0.

#### 16.1.7 Capability-flag corrections, by field name

**`BoardCapabilities::hasAlex2`** (`BoardCapabilities.h:340`, doc block at `:326-329`) conflates
three separate upstream concepts and must be split. Its own doc block says it gates the
Setup > Antenna > Alex-2 Filters sub-tab citing `setup.cs:6228`, but the real tab gate is
`setup.cs:6458-6464 [v2.10.3.15]`, whose model list **excludes** `ANAN_G2E` and **excludes**
`ORIONMKII`. Meanwhile `kHermesC10` sets it `true` (`BoardCapabilities.cpp:701`) citing
`SetMKIIBPF(1)`, which is a different thing entirely: `mkiibpf` gates only the suppression of the
Alex attenuator relay writes (`ChannelMaster/netInterface.c:421-423 [v2.10.3.15]`,
`void SetAlexAtten(int bits) { if (mkiibpf) return;`) and the RL17 / RL22 remap in `SetAntBits`
(`ChannelMaster/netInterface.c:461-477 [v2.10.3.15]`).

User-visible consequence today: a G2E operator is shown an Alex-2 Filters tab
(`AntennaAlexTab.cpp:160-164`) that Thetis never shows, for a second bank the radio does not have.

Proposed replacement fields (all derivable per `HPSDRHW`, all cited). `rxFilterChainCount` is no longer proposed: it shipped with defect D4 (2026-07-29) and lives on `BoardCapabilities`. The rest are still design-only.

| New field | Value | Source |
| --- | --- | --- |
| `rxFilterChainCount` | 2 for `{OrionMKII, Saturn}` plus the NereusSDR-only Saturn-derived rows `{SaturnMKII, Andromeda}` (§16.1.2), else 1 | `console.cs:15435-15443 [v2.10.3.15]` |
| `rxFilterLadderChain0` | `Bpf1` for `{OrionMKII, Saturn, HermesC10}`, else `LegacyHpf` | `console.cs:6829-6831 [v2.10.3.15]` |
| `mkiiBpfWireStyle` | true when `SetMKIIBPF(1)` | `clsHardwareSpecific.cs [v2.10.3.15]` per model |
| `showsAlex2SetupTab` | membership in the tab list | `setup.cs:6458-6464 [v2.10.3.15]` |

`showsAlex2SetupTab` cannot be made correct as a single bool on a per-`HPSDRHW` row, because
`kOrionMKII` serves five models that disagree upstream (ORIONMKII is in the driver list but not
the tab list; REDPITAYA is in both lists yet has `SetMKIIBPF(0)`). It needs per-`HPSDRModel`
resolution or an explicit model set. `rxFilterChainCount` and `rxFilterLadderChain0` do **not**
have this problem and are safe on the existing rows.

Note also that Thetis is internally inconsistent here: ORIONMKII gets Alex2 HPF bits written for
a bank whose settings the operator can never see or edit. That is upstream behaviour. Do not
"fix" it in NereusSDR without a decision (§16.7 Q10).

**`BoardCapabilities.cpp:620-621`** asserts "OrionMKII uses standard Alex HPF/LPF; Saturn BPF1
override is G2/G2-1K only". Refuted by `console.cs:6829 [v2.10.3.15]`, where OrionMKII is the
**first** member of the BPF1 set. This wrongly excludes Anvelina Pro 3, ANAN-7000DLE and
ANAN-8000DLE.

**`SettingsHygiene.cpp:78-81`** encodes `usesBpf1` as `{Saturn, SaturnMKII, HermesC10}` while
citing `console.cs:6829-6834`, whose actual set is `{OrionMKII, Saturn, HermesC10}`. Two errors in
one expression: OrionMKII dropped, `SaturnMKII` added. `SaturnMKII` appears exactly once in all of
Thetis (`enums.cs:399 [v2.10.3.15]`) and is referenced by no code. The consequence is data loss,
not cosmetics: the `!usesBpf1` arm deletes `hardware/<mac>/alex/bpf1/*/start|end`, so an Anvelina
Pro 3 or ANAN-7000DLE owner has their band-edge table erased at startup.

#### 16.1.8 Prerequisites that block this section

None of the following are introduced by this design. All of them are on the path the router runs
through, and the router must not be built on top of them.

1. **`computeHpf` is the wrong ladder for the target SKUs.**
   `src/core/codec/AlexFilterMap.cpp:82-91` implements the legacy HPF ladder with **no board
   argument**, and `RadioModel::republishAlexAdcSlices` calls it for both chains
   (`src/models/RadioModel.cpp:8952`). On a G2, G2E or Anvelina Pro 3 that is wrong on four ham
   bands. Worked table, translating our bits through our own scatter map
   (`src/core/codec/P2CodecSaturn.cpp:179-185`) into deskhpsdr's named passbands:

   | Freq | Thetis Bpf1 | Ours | Verdict |
   | --- | --- | --- | --- |
   | 1.85 MHz (160 m) | `0x10` 160 BPF | `0x10` | agrees by coincidence |
   | 3.70 MHz (80 m) | `0x08` 80/60 BPF | `0x10` 160 BPF | **wrong** |
   | 5.35 MHz (60 m) | `0x08` 80/60 BPF | `0x10` 160 BPF | **wrong** |
   | 7.15 MHz (40 m) | `0x04` 40/30 BPF | `0x08` 80/60 BPF | **wrong** |
   | 10.1 MHz (30 m) | `0x04` | `0x04` | agrees by coincidence |
   | 14.2 MHz (20 m) | `0x01` | `0x01` | agrees by coincidence |
   | 18.1 MHz (17 m) | `0x01` | `0x01` | agrees by coincidence |
   | 21.2 MHz (15 m) | `0x01` 20/15 BPF | `0x02` 12/10 BPF | **wrong** |
   | 24.9 MHz (12 m) | `0x02` | `0x02` | agrees by coincidence |
   | 28.5 MHz (10 m) | `0x02` | `0x02` | agrees by coincidence |
   | 50.1 MHz (6 m) | `0x40` | `0x40` | agrees by coincidence |

   Seven of eleven agree by accident. A bench test on 20 m or 10 m looks perfect. The tells are
   80 m, 60 m, 40 m and 15 m, and 80 m is the first-boot default frequency
   (`src/core/P2RadioConnection.cpp:595` sets 3865000).

   Separately, our bypass edge is 1.5 MHz where Thetis uses 1.8
   (`udAlex1_5HPFStart = 18` scaled, `setup.Designer.cs:23832-23836 [v2.10.3.15]`) and deskhpsdr
   uses `HPFfreq < 1800000LL`. So 1.5 to 1.8 MHz engages a filter where upstream bypasses, on the
   P1 boards where our ladder is otherwise correct (`src/core/P1RadioConnection.cpp:852` uses the
   same function).

2. **The BPF1 escape hatch is dead and points at the wrong word.**
   `src/core/P2RadioConnection.cpp:2373-2376` hardcodes `ctx.p2SaturnBpfHpfBits = 0`, and
   `P2CodecSaturn::buildAlex1` returns the parent word unchanged when it is zero
   (`src/core/codec/P2CodecSaturn.cpp:162`). Worse, when it is non-zero it substitutes the BPF1
   bits into **Alex1** (`src/core/codec/P2CodecSaturn.cpp:153-187`), while Thetis's
   `setBPF1ForOrionIISaturn` calls `NetworkIO.SetAlexHPFBits` throughout
   (`console.cs:6959-7063 [v2.10.3.15]`, fifteen call sites), which writes `prbpfilter`, that is,
   **Alex0**. Enabling the current code path would put chain 0's decision into chain 1's word and
   clobber `ctx.alexHpfBitsAdc1`.

3. **The P2 TX low-pass filter follows the last RX retune, not the TX frequency.**
   `P2RadioConnection::setReceiverFrequency` rewrites the single global `m_alex.lpfBits` from the
   **receive** frequency for any receiver index (`src/core/P2RadioConnection.cpp:740-741`), and
   `setTxFrequency` never touches it (`:748-753`). Upstream splits exactly this:
   `SetAlexLPFBits(int bits, bool isTX, bool isMox)` writes Alex1 only when `isMox || isTX` and
   Alex0 only when `isMox || !isTX` (`ChannelMaster/netInterface.c:682-707 [v2.10.3.15]`), with
   the comment "TX settings are encoded in the Alex1 word" at `:679-681`; `UpdateTXDDSFreq` calls
   `setAlexLPF(tx_dds_freq_mhz, true)` (`console.cs:15467 [v2.10.3.15]`). Our P1 path already gets
   it right (`src/core/P1RadioConnection.cpp:856-868`).

   Today this is masked because one slice means RX freq equals TX freq. Multi-slice removes the
   mask. This section's router **never touches the LPF**, but it is the feature that makes the
   defect routine, so the LPF fix is a hard prerequisite for shipping auto-routing.

4. **ADC distribution does not exist.** `ReceiverManager::setAdcForReceiver` is called exactly once,
   receiver 0 to ADC0 (`src/models/RadioModel.cpp:4640`). Every stream reports ADC0, which
   `republishAlexAdcSlices` documents at `src/models/RadioModel.cpp:8907-8913`.
   `ReceiverManager::m_rxAdcCtrl1` defaults to 0 (`src/core/ReceiverManager.h:324`) where Thetis
   defaults to 4 (`console.cs:15099 [v2.10.3.15]`, meaning DDC0 to ADC0 and DDC1 to ADC1), and
   `setRxAdcCtrl1` has zero callers.

5. **`HardwareDdcRoutingPage` writes only AppSettings.** The per-DDC ADC table persists to
   `AppSettings::instance().setHardwareValue(...)` (`src/gui/setup/HardwareDdcRoutingPage.cpp:145,
   153`) and never reaches `ReceiverManager` or the wire.

6. **`SpectrumStatusOverlay::setWideBpf` has zero callers.** The pill and its click signal exist
   (`src/gui/widgets/SpectrumStatusOverlay.h:57`, paint at `.cpp:171-176`, hit-test at `.cpp:211`),
   nothing drives them.

7. **Effective-BPF changes do not push on their own trigger.** `republishAlexAdcSlices()` has two
   call sites only: `requestDdcAssignment()` (`src/models/RadioModel.cpp:3145`) and the
   `Connected` arm (`:9509`). Neither `setWidebandActive` (`:3574-3581`) nor
   `FilterPolicyDialog` Apply (`src/gui/widgets/FilterPolicyDialog.cpp:112-117`) republishes, so
   those two state changes repaint the label and reach the radio only on the next VFO tick.

---

### 16.2 The routing algorithm

#### 16.2.1 Shape and placement

A pure function, no Qt, no hardware, unit-testable in isolation, in the same spirit as
`SliceStreamAllocator`:

```
FilterChainPlan FilterChainRouter::plan(const FilterChainInput& in) const;
```

It runs inside `RadioModel::requestDdcAssignment()` **after** the allocator has settled slice
bindings and **before** `invokeCodecDdcAssignment()`. It produces:

- `chainForStream[stream]` (0 or 1, or -1 for idle streams)
- `engagedBits[chain]` (the Alex HPF byte, `0x20` for bypass, -1 for "hold, nothing receiving")
- `effective[chain]` (`Filtered` / `Bypass` / `WidebandLocked`) plus a `reasonText`
- `attenuatedSlices` (slice ids whose frequency lies outside the engaged passband, only possible
  under `ForceBand` or a band-pass merge that the operator pinned)

`FilterChainInput` carries: `rxFilterChainCount`, both ladders (as editable range tables), the
active-stream descriptors (index, centre Hz, sample rate Hz, slice frequencies), per-chain
operator mode, per-stream operator ADC pin, wideband-extended flags, MOX, PureSignal running,
diversity active, and the **previous plan** (for stickiness, §16.2.5).

#### 16.2.2 Definitions

- **Filter range**: an entry in the active ladder table, identified by its index. Index -1 means
  "no range covers this frequency", which is the ladder's bypass fallthrough
  (`console.cs:7060-7063 [v2.10.3.15]`).
- **Slice range**: `ladder.rangeIndexFor(sliceFrequencyMhz)`.
- **Stream group key**: the **sorted set** of the slice ranges of every live slice on that stream.
  A stream is single-group when the set has one element.
- **Chain union**: the union of the group keys of the streams routed to that chain.

Two notes that matter:

1. The group key is computed from **slice frequencies**, not from the DDC window edges. The
   operator hears at slice frequencies; the filter's job is to let those through. Window edges are
   handled separately in §16.4 as a display concern, because a 1536 kHz window on 160 m is wider
   than the entire 500 kHz `ALEX_ANAN7000_RX_160_BPF` passband
   (`deskhpsdr alex.h:122 [@f3d857c]`) and no routing decision can fix that.
2. A stream can be multi-group only when it has two or more slices whose ranges differ. That is
   possible because `SliceStreamAllocator` groups by window fit and prefers sharing
   (`src/core/SliceStreamAllocator.cpp:64-73`), and a 1536 kHz window can straddle 11.0 MHz. A
   multi-group stream **cannot be split by the router**, because its slices are on one DDC. It is
   resolved by step 4 like any other multi-range chain.

#### 16.2.3 The algorithm

**Step 0. Antenna.** Do nothing. There is one RX antenna selector, per band, shared by both chains
(§16.1.6). Antenna is **not** an input to chain assignment and cannot produce a chain conflict.
Design rule 3 is replaced by: *"a change of RX antenna affects both chains simultaneously and is
never a reason to route a stream to a different chain."* The only antenna-adjacent rule the router
keeps is the per-band scoping already implemented in `AlexController::setRxAnt(Band, int)`
(`src/core/accessories/AlexController.h:126, 206`).

**Step 1. Canonicalise.** Sort active streams by `streamIndex` ascending. Compute each stream's
group key. Discard idle streams. This makes the plan independent of `m_slices` insertion order,
which is required because `removeSlice` does not renumber survivors.

**Step 2. Honour pins.** Any stream with an operator ADC pin takes that chain unconditionally.
Any chain with `BpfMode::ForceBypass` or `ForceBand` keeps that mode; it still receives streams,
it just does not get an auto-computed engagement in step 4.

**Step 3. Assign unpinned streams to chains.**

- If `rxFilterChainCount == 1`: every stream goes to chain 0. Skip to step 4. This is the ANAN-G2E
  path and also every P1 board and every legacy HPF board.
- Otherwise, let `K` be the set of distinct group keys among unpinned streams, and `F` the set of
  chains not already saturated by pins.
  - **3a.** Every stream whose key already appears on some chain joins that chain. Free, no cost.
  - **3b.** If `|K| <= |F|`, give each remaining distinct key its own chain. Choose in ascending
    key order (compare by the smallest range index in the key, then the second smallest, and so
    on), and for each, pick the lowest-index free chain. Deterministic.
  - **3c.** If `|K| > |F|`, merge. Repeatedly merge the two keys whose **union spans the fewest
    ranges**, which in practice merges adjacent ranges first. Ties broken by lowest smallest-range
    index, then by lowest second-smallest. Repeat until `|K| == |F|`, then apply 3b.

  Rationale for 3c's cost function: merging range 3 with range 4 (20/15 m with 12/10 m) costs one
  bypass on a two-range chain. Merging range 0 with range 5 (160 m with 6 m) costs the same bypass
  but strands a far larger swathe. On a legacy HPF chain the cost function is genuinely different,
  because merging is nearly free there (step 4 keeps everything passing), so on `LegacyHpf`
  chains 3c is a no-op: merge in ascending key order and stop.

**Step 4. Resolve each chain's engagement.** Priority order, highest first, matching the existing
`AlexController::recomputeBpf` order (`src/core/accessories/AlexController.cpp:117-152`):

1. Chain has **wideband extended view** active on any of its streams: `WidebandLocked`, bits
   `0x20`, reason `"BYPASS (wideband active)"`.
2. Chain is in **MOX with PureSignal** and the operator's HPF-bypass-on-PS option is set:
   bits `0x20`, reason `"BYPASS (PureSignal TX)"`. This is a port, not a divergence:
   `console.cs:6957-6964 [v2.10.3.15]`,
   `if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled))) NetworkIO.SetAlexHPFBits(0x20);`,
   and our wire-confirmed implementation at `src/core/P2RadioConnection.cpp:2348-2350`.
3. Chain mode is `ForceBypass`: bits `0x20`, reason `"BYPASS (operator override)"`.
4. Chain mode is `ForceBand(r)`: engage range `r`. Every slice on the chain whose range is not `r`
   goes into `attenuatedSlices`. Reason `"<range label> (forced)"`.
5. Chain union is empty: emit -1 (hold last). Mirrors Thetis, which only calls `setAlex2HPF` when
   RX2 exists (`console.cs:15435-15442 [v2.10.3.15]`).
6. Chain union has exactly one range `r`: engage `r`. `Filtered`. Reason is the range label. If the
   operator has ticked that range's per-range bypass checkbox, the emitted bits are `0x20` but the
   effective state is still reported as `Filtered (bypassed by request)`, because it is not a
   routing failure and must not raise WIDE as a fault.
7. Chain union has two or more ranges. **Ladder-dependent, and this is the whole design:**
   - **`LegacyHpf`**: engage the range of the **lowest slice frequency on the chain**. Every slice
     still passes, because a high-pass corner set for the lowest occupant passes everything above
     it. Effective `Filtered`, reason `"<range label> (shared, set from lowest)"`. **This is a
     Thetis port**, `console.cs:15504-15507 [v2.10.3.15]`:
     ```csharp
     if (!_rx2_preamp_present && chkRX2.Checked)
     {
         if (rx1_dds_freq_mhz < rx2_dds_freq_mhz) setAlex1HPF(rx1_dds_freq_mhz);
         else setAlex1HPF(rx2_dds_freq_mhz);
     }
     ```
     Bypass is **never** reached for the multi-range reason on a legacy HPF board. That satisfies
     "bypass is the last resort" completely on those SKUs.
   - **`Bpf1`**: engage **bypass**, bits `0x20`, effective `Bypass`, reason
     `"BYPASS (multi-range: 40/30 + 20/15)"`.

     **This is a deliberate divergence from Thetis and must not be presented as a port.** Thetis
     applies its `UpdateAlexRXFilter` lowest-frequency rule whenever `!_rx2_preamp_present`
     (`console.cs:15503 [v2.10.3.15]`), and the ANAN-G2E is exactly such a board
     (`_rx2_preamp_present = false`, `console.cs:14835-14838 [v2.10.3.15]`) while also being a
     `Bpf1` board (`console.cs:6831 [v2.10.3.15]`, `HermesC10`). On a band-pass ladder,
     "set from lowest" does not widen the chain, it **selects a passband that excludes the higher
     receiver**. Upstream would make the higher slice deaf. We bypass instead, so both slices keep
     receiving with reduced front-end protection. Justification: the operator can always recover
     protection with an explicit `ForceBand` pin, but cannot recover a receiver that is not
     hearing anything, and a silent deaf receiver is the worse failure. §16.7 Q1 asks for
     confirmation of this default.

**Step 5. Emit.** Fill `AlexRxBpf{hpfBitsAdc0, hpfBitsAdc1}` exactly as
`republishAlexAdcSlices` does today (`src/models/RadioModel.cpp:8955-8970`), and marshal to the
connection thread with `QMetaObject::invokeMethod`.

#### 16.2.4 Determinism requirements (testable)

| ID | Requirement |
| --- | --- |
| D1 | `plan()` is a pure function of `FilterChainInput`. Same input, same output, always. |
| D2 | The output does not depend on the order of streams or slices in the input containers. Assert by shuffling the input and comparing plans. |
| D3 | Every tie-break in step 3 is total. There is no "pick either". |
| D4 | `plan()` never mutates the allocator, never re-binds a slice, and never emits a Qt signal. |
| D5 | On `rxFilterChainCount == 1` the plan is byte-identical to the single-chain plan regardless of how many streams exist. |
| D6 | With exactly one live slice, the emitted bits are byte-identical to the pre-Phase-3F frequency-derived path for that board's ladder. This is the no-regression anchor. |

#### 16.2.5 Stability and hysteresis

The falsification pass established that thrash exists but is narrower than first claimed, and that
Thetis itself has **no hysteresis anywhere** in its filter ladders (`console.cs:6839-6950` and
`console.cs:6953-7064 [v2.10.3.15]` are bare threshold ladders re-evaluated on every retune). It
also established that `P2RadioConnection::setAlexRxBpf` dedups only on an identical pair of words
(`src/core/P2RadioConnection.cpp:969-972`), so a stationary slice sends nothing and only an actual
boundary crossing reaches the wire.

The design therefore splits the two decisions, because they are not the same risk:

- **Filter engagement (which range a chain uses): no hysteresis.** Thetis parity. A crossing
  changes a relay, and that is what upstream does. Adding hysteresis here would be an
  approval-gated divergence under the source-first protocol for no established benefit.
- **Chain assignment (which ADC a stream sits on): hysteresis required.** This is a decision Thetis
  does not have, because Thetis never re-routes a DDC between ADCs. Moving a stream changes its
  entire front end mid-audio: ADC1 has its own step attenuator
  (`setup.cs:20205-20206 [v2.10.3.15]`) and its own 6 m LNA offset (`setup.cs:6291 [v2.10.3.15]`).
  A stream that ping-pongs across a range boundary would swap front ends on every knob detent.

  Rules:
  - **S1 Sticky.** `plan()` takes the previous plan. Whenever two candidate assignments are
    equally good under step 3, prefer the previous one. This is what stops an unrelated slice
    being added from reshuffling streams that were already fine.
  - **S2 Edge deadband.** A stream is considered to have left its current range only once its
    governing slice frequency is more than `kRangeEdgeHysteresisHz` past the boundary. Proposed
    10 kHz. Applies to the **chain assignment** input only, never to the engaged bits.
  - **S3 Dwell.** A stream does not change chains until its new group key has been stable for
    `kChainMoveDwellMs`. Proposed 750 ms. Implemented by carrying a pending-move timestamp in the
    plan and re-running on expiry; the router stays pure by taking "now" as an input.
  - **S4 No move under MOX.** Chain assignment is frozen while `mox` is true. See §16.3.3.

  Both constants are NereusSDR-original and are product calls: §16.7 Q4.

#### 16.2.6 Interaction with `SliceStreamAllocator`

Clean separation of ownership, no negotiation:

| Layer | Owns | Never does |
| --- | --- | --- |
| `SliceStreamAllocator` | slice to stream (DDC) | knows nothing about ADCs or filters |
| `FilterChainRouter` | stream to chain (ADC), and each chain's engaged filter | never re-binds a slice, never moves a stream centre |

The allocator's existing preferences already work in the router's favour. Rule 1 prefers sharing an
active stream whose window covers the frequency (`src/core/SliceStreamAllocator.cpp:64-73`), and
co-hosted slices are by construction within one window, so they are usually in one filter range.
`RadioModel::addSlice` seeds a new slice from the active slice's frequency before binding
(`src/models/RadioModel.cpp:3482-3486`), so the common case is a single-group stream.

Two collision cases and their rules:

1. **Window straddles a range boundary with two slices either side.** Allocator will co-host. The
   router cannot split them, because they share a DDC. Resolution is step 4 case 7. Do **not** ask
   the allocator to split: that would fight CTUN pinning
   (`SliceStreamAllocator::retuneSlice`'s `mayRetuneStream` contract,
   `src/core/SliceStreamAllocator.h:69-79`).
2. **Two streams in the same range, one chain free.** The router leaves them together (step 3a).
   Spreading them would consume the second chain for no filter benefit and would cost a front-end
   change.

One future coupling is explicitly **not** implemented here: publishing per-chain "group pressure"
back to the allocator so it prefers to co-host a new slice on a stream already in the same range.
Deferred, §16.8.

#### 16.2.7 Test matrix for the router (unit, no hardware)

| # | Board / ladder | Chains | Slices | Expected |
| --- | --- | --- | --- | --- |
| R1 | Saturn / Bpf1 | 2 | one at 14.200 | chain0 range 3, `Filtered`, bits `0x01`; chain1 -1 |
| R2 | Saturn / Bpf1 | 2 | 14.200 + 21.200 | one chain, range 3 (same range), `Filtered`. No second chain used. |
| R3 | Saturn / Bpf1 | 2 | 14.200 + 7.150 | two chains, ranges 3 and 2, both `Filtered`. No bypass. |
| R4 | Saturn / Bpf1 | 2 | 14.200 + 7.150 + 3.700 | ranges 2 and 1 merged (adjacent, cheapest), that chain `Bypass`; other chain range 3 `Filtered`. |
| R5 | HermesC10 / Bpf1 | 1 | 14.200 + 7.150 | one chain, `Bypass`, reason lists both ranges. |
| R6 | Angelia / LegacyHpf | 1 | 14.200 + 7.150 | engaged from lowest (7.150), `Filtered`, no bypass. |
| R7 | any | any | 0 live | bits -1 on every chain, hold. |
| R8 | Saturn | 2 | R4 input, shuffled order | plan identical to R4 (D2). |
| R9 | Saturn | 2 | R3 input, then add a 4th slice at 14.250 | slices 1 and 4 co-host, chain assignment unchanged (S1). |
| R10 | Saturn | 2 | slice tuned back and forth across 11.000 within 10 kHz | chain assignment does not change (S2). |
| R11 | Saturn | 2 | crossing 11.000 by 50 kHz, held 200 ms then reverted | chain assignment does not change (S3). |
| R12 | Saturn | 2 | MOX asserted mid-crossing | chain assignment frozen (S4). |
| R13 | any | any | chain mode `ForceBypass` | bits `0x20` regardless of slice set. |
| R14 | Saturn | 2 | chain mode `ForceBand(3)`, a slice at 7.150 on that chain | bits for range 3; slice reported in `attenuatedSlices`. |
| R15 | Saturn | 2 | extended view on chain 1 | chain 1 `WidebandLocked` even if it is single-range. |

Wire-level assertions (byte-for-byte, in the codec tests) belong with the existing
`tst_alex_per_adc_bpf_wire.cpp` harness.

---

### 16.3 Interaction rules: diversity, PureSignal, TX

#### 16.3.1 Diversity

Verified corrections to §8's premises:

- The pair is **pinned to DDC0 + DDC1**, not "any even n". Thetis writes only one sync byte, DDC0's
  (`ChannelMaster/network.c:1171-1172 [v2.10.3.15]`, `packetbuf[1363] = prn->rx[0].sync;`), and our
  own de-interleaver hard-codes `if (ddcIndex == 0 && m_rx[0].sync != 0)`
  (`src/core/P2RadioConnection.cpp:2819`). A pair anywhere else would produce a stream nothing can
  de-interleave.
- The **odd DDC is not in the enable mask**. Thetis sets `DDCEnable = DDC0; SyncEnable = DDC1;`
  (`console.cs:8234-8240 [v2.10.3.15]`); deskhpsdr writes `receive_specific_buffer[7] = 1;` with
  the comment "enable DDC0 only; DDC1 is synchronized to DDC0"
  (`new_protocol.c:1908-1909 [@f3d857c]`). If we enable both we diverge from every reference.
- There is exactly **one** diversity pair on every SKU. ChannelMaster creates a single WDSP DIV
  engine with `nr = 2` (`ChannelMaster/sync.c:35 [v2.10.3.15]`,
  `create_divEXT(0, 0, 2, 1024);`), and Thetis only ever calls `SetEXTDIVRun(0, ...)`.
- The layout is **not** "diversity first, then everything else". On G2-class, diversity takes
  DDC0 + DDC1, DDC2 goes idle, and RX2 stays on DDC3
  (`console.cs:8299-8302 [v2.10.3.15]`).
- On Protocol 1 there is no DDC sync concept at all. Diversity is one C&C bit,
  `C4 |= (P1_en_diversity) << 7; // if diversity, locks VFOs`
  (`ChannelMaster/networkproto1.c:471 [v2.10.3.15]`). A P1 router must not reason about sync pairs.

**Router rules under diversity:**

- **DIV1.** DDC0 and DDC1 are removed from the user stream pool. On Saturn-class this is already
  true (`kStreamToDdc[5] = {2, 3, 4, 5, 6}`, `src/core/codec/P2CodecSaturn.cpp:223`).
- **DIV2.** The diversity pair occupies both chains by construction: DDC0 on ADC0, DDC1 on ADC1.
  The router treats it as a **pinned single-group occupant of both chains**, keyed on slice A's
  frequency.
- **DIV3.** Both chains engage the **same range**, taken from the diversity slice's frequency.
  **This is a divergence from Thetis and an adoption of deskhpsdr policy.** Thetis feeds
  `setAlex2HPF(rx2_dds_freq_mhz)` with no diversity override
  (`console.cs:15443 [v2.10.3.15]`), so upstream's ADC1 band-pass tracks VFO B while DDC1 is tuned
  to VFO A. deskhpsdr forces both selectors from `DDCfrequency[0]`
  (`new_protocol.c:1308-1309, 1341-1342 [@f3d857c]`). We follow deskhpsdr because the DIV combiner
  requires the two branches to be spectrally coherent; a mismatched band-pass on one branch
  destroys the combination. §16.7 Q7 asks for confirmation.
- **DIV4.** With diversity active on a 2-chain SKU, remaining user streams **cannot be spread**
  across chains, because both chains are committed. All remaining streams take chain 0's
  engagement decision and inherit it. If they are not in the diversity range, chain 0 is already
  pinned by DIV3, so those streams are reported in `attenuatedSlices` on a `Bpf1` board. Show
  this, do not silently accept it (§16.5).
- **DIV5 (blocking bug).** `ReceiverManager::m_rxAdcCtrl1` defaults to 0
  (`src/core/ReceiverManager.h:324`, `.cpp:152`) where Thetis defaults to 4
  (`console.cs:15099 [v2.10.3.15]`), and `setRxAdcCtrl1` has zero callers. Thetis's diversity
  branch passes `cntrl1` through unmodified and inherits its default; we would inherit a broken
  one and both branches would read ADC0. Fix before diversity is enabled.
- **DIV6.** A synced pair halves the per-packet payload: the master DDC's stream carries 119
  sample pairs instead of 238 samples (`deskhpsdr newhpsdrsim.c:1029-1032 [@f3d857c]`) and the
  slave's port goes silent. Any per-DDC bandwidth accounting in the router must model this.

#### 16.3.2 PureSignal, and what happens to the DDC budget on TX

The DDC budget **does** change on TX, it changes differently per family, and it is a function of
**protocol**, not of ADC count. Thetis's `GetDDC` branches on protocol first
(`console.cs:8562` P2, `console.cs:8647` P1 `[v2.10.3.15]`) and only then on hardware. Indexing is
`tot = MOX + (Diversity << 1) + (PSEnabled << 2)` (`console.cs:8560 [v2.10.3.15]`).

| Family / protocol | Plain RX | On MOX + PS | Collapse? |
| --- | --- | --- | --- |
| P2 Saturn / OrionMkII / Orion / Angelia | rx1 = DDC2, rx2 = DDC3 (`console.cs:8570-8579 [v2.10.3.15]`) | rx1 = DDC2 kept (`tot=5`) | no |
| P2 Hermes / HermesII / HermesC10 (G2E) | rx1 = DDC0, rx2 = DDC1 (`console.cs:8610-8618 [v2.10.3.15]`) | `tot=5` and `tot=7` bodies are **empty**, rx1 and rx2 stay at the -1 initialiser (`console.cs:8554, 8631-8642 [v2.10.3.15]`) | **total** |
| P1 Angelia / Orion / OrionMkII | rx1 = DDC0, rx2 = DDC2 (`console.cs:8651-8660 [v2.10.3.15]`) | PS on psrx=3 / pstx=4 | no |
| P1 Hermes / HermesC10 (G2E) | rx1 = DDC0, rx2 = DDC1 | PS on psrx=2 / pstx=3 (`console.cs:8704-8733 [v2.10.3.15]`) | no |
| P1 HermesII (ANAN-10E / 100B) | rx1 = DDC0, rx2 = DDC1 | psrx=0 / pstx=1, rx1/rx2 unset (`console.cs:8769-8780 [v2.10.3.15]`) | **total** |

Independently corroborated by deskhpsdr, which has no shared ancestry with Thetis:
`// note that for HERMES, receiver[i] is associated with DDC(i) but beyond (that is, ANGELIA,
ORION, ORION2, G2) receiver[i] is associated with DDC(i+2)` (`new_protocol.c:1830-1834 [@f3d857c]`),
and `// the DDC for PS_RX_FEEDBACK is always DDC0 ... PS_TX_FEEDBACK is always DDC1`
(`new_protocol.c:1861-1862 [@f3d857c]`).

Two corrections to §9 of this document, which is wrong in two ways: it says "On 1-ADC SKUs
(Hermes/HermesII): all user slices pause", which is P2-correct but P1-wrong for Hermes (does not
pause) while P1-correct for HermesII (does pause); and the ANAN-G2E, the SKU that motivates this
work, is not named in §9 at all.

**Router rules under PureSignal:**

- **PS1.** The router does **not** re-plan on a PS collapse. It **freezes** the last plan and marks
  every affected stream `suspended`. Re-planning during a collapse would compute a plan for an
  empty stream set and push it to the wire.
- **PS2.** On collapse, chain engagement is **held**, not recomputed. Emitting -1 (hold) is
  correct: the chain still has its last bits and nothing is receiving on it.
- **PS3.** The MOX+PS HPF bypass (step 4 case 2) is a **port**, and it applies to chain 0 only,
  because `setBPF1ForOrionIISaturn` calls `SetAlexHPFBits` (Alex0). Our codec already gets the
  Alex0/Alex1 split right and documents the wire-byte diff that proved it
  (`src/core/codec/P2CodecOrionMkII.cpp:556-580`).
- **PS4 (adjacent defect the router will trip over).** `P2CodecHermes::applyDdcAssignment` sets
  `if (slices[0].live) { a.streamDdc[0] = 0; }` **before** the PureSignal branch
  (`src/core/codec/P2CodecHermes.cpp:247`) and never clears it inside
  (`:249-276`), while the suspension detector requires `streamDdc[st] < 0`
  (`src/models/RadioModel.cpp:12079`). So on a G2E, stream 0 is never reported suspended during
  PS TX, and the "PS HOLD" pill never fires for the one slice that actually lost its DDC. One-line
  fix inside the PS branch. `P1CodecStandard::applyDdcAssignment` has the same shape
  (`src/core/codec/P1CodecStandard.cpp:900-921`).

#### 16.3.3 TX

- **TX1. The router does not touch the LPF, ever.** The RX band-pass decision and the TX low-pass
  are different words and different frequencies. Our own code already states the rule
  (`src/core/P2RadioConnection.cpp:2329-2330`). The LPF defect in §16.1.8 item 3 is a prerequisite,
  not part of this work.
- **TX2. Chain assignment is frozen while MOX is asserted.** No stream changes chain on TX. This
  is S4 in §16.2.5 and it is NereusSDR-original: Thetis has no chain assignment to freeze. The
  reason is that a chain move during transmit changes which front end is grounded by
  `_rx2_gnd` / `ALEX1_ANAN7000_RX_GNDonTX`, which deskhpsdr comments is asserted on TX always
  ("The main purpose of RX2 is DIVERSITY. Therefore, ground RX2 upon TX *always*.",
  `new_protocol.c:1364-1368 [@f3d857c]`).
- **TX3. Filter engagement may still change on TX**, because case 2 (PS bypass) and case 1
  (wideband) are both TX-reachable, and because Thetis recomputes engagement freely. Only the
  assignment is frozen.

---

### 16.4 The WIDE indicator

#### 16.4.1 What it means, exactly

> **WIDE means: the RX preselector chain feeding this panadapter is bypassed on the wire right now.**

Concretely: the Alex word for this pan's chain has the bypass bit set (`0x20` at the byte level,
bit 12 in the 32-bit word, `ChannelMaster/netInterface.c:604-651 [v2.10.3.15]`,
`prbpfilter->_Bypass = (bits & 0x20) != 0;`).

It reports an **effect**, not a cause. Four things cause it, and the badge is identical for all
four; the cause appears in the tooltip and in the Filter Policy dialog.

#### 16.4.2 Which pans show it

A pan shows WIDE if and only if:

```
pan -> its bound stream -> chainForStream[stream] -> effective is Bypass or WidebandLocked
```

Corollaries, all of which are testable:

- A pan whose stream is idle or unbound shows nothing. Not WIDE, not filtered.
- On a 2-chain SKU with the bypass on chain 1 only, pans on chain 0 do **not** show WIDE. This is
  the whole point of routing: the badge is per chain, and it tells the operator which of their
  receivers is exposed.
- Two pans on the same chain always agree. If they disagree, that is a bug.
- On a 1-chain SKU (ANAN-G2E, every P1 board) all pans agree by construction.
- A per-range operator bypass checkbox (§16.1.4 item 3) does **not** raise WIDE. The operator asked
  for it, and it is reported as `Filtered (bypassed by request)` in the chain indicator, not as a
  fault badge. §16.7 Q5 asks whether the maintainer agrees.

#### 16.4.3 The collision with Sub-Epic F "wideband extended view" is not a semantic collision

§7 item 6 of this document already defines the causal chain: when the visible range zoom exceeds
DDC bandwidth, `SliceModel::widebandExtensionRequested` is set, which propagates to
`AlexController` (auto-bypass BPF for that ADC). That is implemented today at
`src/models/RadioModel.cpp:3574-3581` and given top priority in
`src/core/accessories/AlexController.cpp:117-119`.

So extended view is **one of the causes of bypass**, not a different meaning of the same word.
WIDE remains a single, unambiguous statement about the RF path.

What is genuinely missing is a way to see that **extended view** is on, which is a data-source
statement (the wings of the pan come from the ADC wideband stream, `network.c:550-603 [v2.10.3.15]`)
rather than an RF statement. Recommendation: a separate `EXT` pill on the same overlay, distinct
colour, never overloaded onto WIDE. That is a UI call, §16.7 Q6.

One consent problem must be fixed alongside, because it is what makes WIDE necessary rather than
merely nice. The only unclamped route into extended view is the zoom slider under the spectrum:
`zoomBar->setRange(1, 768)` then `bwHz = val * 1000.0` fed to `setFrequencyRange` with no clamp
against the live sample rate (`src/gui/MainWindow.cpp:1550-1564`), while both in-widget zoom
gestures **do** clamp (`std::clamp(newBw, 1000.0, m_sampleRateHz)`,
`src/gui/SpectrumWidget.cpp:6288, 6635`). The slider's tooltip is "Zoom: drag to adjust spectrum
bandwidth" and says nothing about the RF path, and its handle is initialised to 768 while the
default DDC rate is 192000 (`src/core/SampleRateCatalog.h:104`), so it already rests four times
above the true span. On a 192 kHz radio, any drag landing above roughly 25 percent of the track
bypasses the preselector. §16.7 Q3.

#### 16.4.4 Exact strings

Badge text: `WIDE`. Existing colours are already correct
(`src/gui/widgets/SpectrumStatusOverlay.cpp:171-176`: amber on dark amber, `#ffb800` on `#604000`).

Tooltip, one of these five, selected by cause. No source citations in user-visible strings, per
project convention:

| Cause | Tooltip |
| --- | --- |
| multi-range auto (`Bpf1`) | `Preselector bypassed. This receiver chain is serving 40/30 m and 20/15 m at once, and the band-pass filter can only pass one of them. Bypassing keeps both slices hearing. Click to change the filter policy for this chain.` |
| extended view | `Preselector bypassed because this panadapter is showing more spectrum than the receiver's own bandwidth. Zoom back in, or turn off extended view for this pan, to restore filtering.` |
| operator override | `Preselector bypassed by your Filter Policy setting for this chain. Click to change it.` |
| PureSignal TX | `Preselector bypassed while PureSignal is transmitting, so the feedback path sees an unfiltered coupler signal. Filtering returns when transmit ends.` |
| diversity range mismatch | `Preselector bypassed because diversity has pinned both receiver chains to one filter range and this slice is outside it. Click to change the filter policy for this chain.` |

Clicking the badge opens the Filter Policy dialog for that chain
(`wideBadgeClicked` already exists, `src/gui/widgets/SpectrumStatusOverlay.cpp:211-216`).

#### 16.4.5 What WIDE does not cover

A chain can be **filtered** and still show the operator a panadapter that is mostly outside the
engaged passband, because the DDC window can be wider than the passband. On 160 m the
`ALEX_ANAN7000_RX_160_BPF` passband is 1.5 to 2.0 MHz, 500 kHz wide
(`deskhpsdr alex.h:122 [@f3d857c]`), while the DDC window at 768 kHz or 1536 kHz is wider than the
entire filter. The audio is correct, the spectrum display is honest about power, but the operator
sees a filtered-out shoulder with no explanation.

WIDE is the wrong badge for that: nothing is bypassed. Proposal is a shaded region on the pan
outside the engaged passband, with the same tooltip vocabulary. That is a new visual and therefore
a maintainer call, §16.7 Q5.

---

### 16.5 UI surfaces

| Surface | State today | Change |
| --- | --- | --- |
| `SpectrumStatusOverlay` WIDE pill | painted, `setWideBpf` has **zero callers** (`SpectrumStatusOverlay.h:57`) | Drive it from the router plan through the pan's stream. Tooltips per §16.4.4. |
| `SpectrumStatusOverlay` CH pill | always painted (`SpectrumStatusOverlay.cpp:146-153`), `m_chainIndex` has no production writer (`setChainIndex` callers are tests only) | Write the real chain index from the router. **Hide the pill entirely when `!caps.hasAlexFilters`**, which restores the 3P-I-a contract ("HL2/Atlas hide all antenna UI on `!caps.hasAlex \|\| antennaInputCount < 3`"). Today `makeChainIndicator(0)` is added unconditionally (`src/gui/MainWindow.cpp:5053`). |
| Bottom-bar chain indicators | already consume `bpfStateChanged` (`src/gui/MainWindow.cpp:1627-1643`) | Gate CH 0 on `hasAlexFilters`. **CH 1 moved to `rxFilterChainCount >= 2` with defect D4 (2026-07-29)**; it had been on `adcCount >= 2`, which offered a second chain, and a Filter Policy override for it, on the two-ADC / one-chain SKUs. |
| `FilterPolicyDialog` | Auto / Force band / Force bypass radios exist (`FilterPolicyDialog.cpp:72-76`); HPF checkbox is scaffolded per its own header comment | Add a read-only "Currently: `<reasonText>`" line and a **Why?** expander listing each stream on the chain with its frequency and its filter range. This is the discoverability answer. Apply must call back into the router; today it does not republish at all (`:112-117`). |
| `HardwareDdcRoutingPage` | per-DDC ADC table writes AppSettings only (`:145, 153`) | Feed it into `FilterChainInput` as the per-stream **pin**. Add an explicit `Auto` row value so the operator can un-pin. Pins are per MAC, per DDC, and override everything except MOX freeze. |
| `AntennaPickerMenu` | labels non-current entries "(switches chain)" and the current one `Chain %1 - current` (`AntennaPickerMenu.cpp:51, 58`) | **Both strings are wrong** and teach a model the hardware does not have (§16.1.6). Remove the chain wording. Also: `EXT1` / `EXT2` / `BYPS` are added with bare `addAction()` and no connect (`:66-68`), so they are inert on every slice. |
| `AlexController` | `recomputeBpf` counts **bands**, not filter ranges, and has hardcoded `adc >= 2` bounds (`AlexController.cpp:104, 111`) | Replace band counting with range counting from the ladder table. Take chain count from `rxFilterChainCount` rather than the literal 2. Read `currentBpfBand` under `ForceBand` (today it is written only in the Auto single-band branch at `:140` and never read, so `ForceBand` silently selects from the lowest slice frequency instead). |
| `RadioModel::republishAlexAdcSlices` | `constexpr int kAdcCount = 2` (`RadioModel.cpp:8891`); calls `computeHpf` for both chains (`:8952`) | Take chain count from capabilities; call the ladder table. Also connect `AlexController::bpfStateChanged` to a republish so every present and future trigger reaches the wire by construction, which fixes the "flips later on an unrelated VFO tick" gap in §16.1.8 item 7. |

**Auto versus pinned, operator-facing model:**

- Default per chain is `Auto` (`AlexController.h:93`, persisted default `"0"` at
  `AlexController.cpp:387-390`). Auto means the router decides both assignment and engagement.
- A chain pin (`ForceBand` / `ForceBypass`) fixes engagement but leaves assignment to the router.
- A per-DDC ADC pin fixes assignment for that stream but leaves engagement to the router.
- Wideband extended view beats both, because it is a data-path requirement rather than a preference.
- The operator discovers **why** in three escalating places: the WIDE badge tooltip (one sentence),
  the bottom-bar chain indicator (`reasonText`, already implemented), and the Filter Policy
  dialog's Why? expander (per-stream detail).

---

### 16.6 Per-SKU expectation table

Ladder ranges are named by their deskhpsdr labels. "protected" means a band-pass or high-pass is
engaged that passes every slice on that chain.

| Scenario | ANAN-G2 (Saturn, 2 chains, Bpf1) | ANAN-G2E (HermesC10, 1 chain, Bpf1) | Anvelina Pro 3 (OrionMKII, 2 chains, Bpf1) |
| --- | --- | --- | --- |
| 1 slice, 14.200 | chain0 = 20/15 BPF, protected. No WIDE. | chain0 = 20/15 BPF, protected. No WIDE. | same as G2 |
| 2 slices, 14.200 + 21.200 (same range) | one chain, 20/15 BPF, both protected. No WIDE. Second chain unused. | one chain, 20/15 BPF, both protected. No WIDE. | same as G2 |
| 2 slices, 14.200 + 7.150 (different ranges) | chain0 = 40/30, chain1 = 20/15. Both protected. **No WIDE.** | **one chain only.** Auto bypasses: both hear, no protection. **WIDE on both pans.** | same as G2 |
| 3 slices in 3 ranges (3.700 + 7.150 + 14.200) | 80/60 and 40/30 merge onto one chain (adjacent, cheapest) which bypasses; 20/15 keeps the other chain. **WIDE on the two merged pans only.** | all three on one chain, bypass. **WIDE on all three pans.** | same as G2 |
| Extended view on one pan | that pan's chain bypasses. WIDE on every pan sharing that chain; the other chain unaffected. | bypass. WIDE on every pan. | same as G2 |
| Operator pins chain to `ForceBand(20/15)` with a 40 m slice on it | 20/15 engaged, 40 m slice listed as attenuated, no WIDE (operator chose it). | same. | same |
| Diversity on | pair takes DDC0+DDC1, both chains pinned to the diversity range (DIV3). Remaining streams inherit and may be attenuated. | **not offered** (`hasDiversityReceiver = false`, 1 ADC). | same as G2 |
| PureSignal + MOX | user slices stay live (P2 Saturn `tot=5` keeps rx1 = DDC2). Chain 0 bypasses if HPF-bypass-on-PS is set: transient WIDE. | user slices **collapse** (P2 HermesC10 `tot=5`, `tot=7` empty). Pans show PS HOLD, not WIDE; router freezes. | same as G2 |
| Any P1 connection of the same silicon | different DDC map entirely (`console.cs:8651-8660 [v2.10.3.15]`), single chain in practice; router runs with `rxFilterChainCount` from the row and no sync-pair reasoning. | as G2E. | as G2 |

Legacy comparison row, for contrast, since it is where the requested outcome is fully achieved:

| Scenario | ANAN-100D / 200D (Angelia / Orion, 1 drivable chain, LegacyHpf) |
| --- | --- |
| 2 slices, 14.200 + 7.150 | engage the 6.5 MHz high-pass (from the lowest occupant). Both slices pass, real protection retained. **No WIDE, ever, for this reason.** Thetis port, `console.cs:15504-15507 [v2.10.3.15]`. |

---

### 16.7 Open questions, maintainer decision required

None of these are decided in this document.

**Q1. Default multi-range behaviour on a band-pass chain.** Bypass so every slice keeps hearing
(proposed), or engage the active slice's range and let the others go deaf? Proposed answer is
bypass, with a visible WIDE badge and an easy pin to the other behaviour. This is the single most
consequential default in the section.

**Q2. Should a second chain be used automatically at all, or only on request?** Splitting two
ranges across ADC0 and ADC1 is what makes "no bypass" achievable on a G2, but it relies on the
inference in §16.1.6 that one antenna feeds both banks, which is not a cited hardware fact.
Options: auto (proposed, gated on the Q9 bench row), opt-in per session, or opt-in per radio.

**Q3. Zoom slider consent.** Should the zoom slider clamp to the live DDC rate, so extended view
(and therefore bypass) is only reachable from an explicit control? Proposed: clamp the slider, and
add an explicit "Extended view" toggle. This changes existing UX behaviour and is out of the
autonomous-agent boundary.

**Q4. Hysteresis constants.** `kRangeEdgeHysteresisHz` proposed 10 kHz, `kChainMoveDwellMs`
proposed 750 ms. Both are NereusSDR-original defaults with no upstream equivalent.

**Q5. Out-of-passband shading, and per-range operator bypass.** Two sub-questions: (a) should the
pan shade the region outside the engaged passband (§16.4.5)? (b) should an operator-ticked
per-range bypass raise WIDE, or be reported as an intentional filtered-wide state?

**Q6. `EXT` pill.** Separate badge for extended view, distinct from WIDE? Proposed yes.

**Q7. Diversity forces identical chain filters (DIV3).** This is a deskhpsdr policy adoption and a
divergence from Thetis, which lets ADC1's band-pass track VFO B independently
(`console.cs:15443 [v2.10.3.15]`). Confirm.

**Q8. BPF1 band-edge table user-editability at ship.** Thetis's boundaries are all Setup spinners
(`setup.cs:5193-5251 [v2.10.3.15]`). Do we ship the table editable, read-only with defaults, or
hidden? Note `BoardCapabilities::p2SaturnBpf1Edges` and the `SettingsHygiene` bug in §16.1.7 both
depend on this answer.

**Q9. Bench confirmation that one antenna feeds both filter banks** on the 7000D / 8000D / G2 /
G2-1K / Anvelina Pro 3. Test: tune two slices to the same band on ANT1, force them onto different
chains, confirm both hear signal. If this fails, Q2 must be answered "opt-in" and the ANAN-G2
column of §16.6 loses its "no WIDE" outcomes.

**Q10. Thetis's ORIONMKII inconsistency.** Upstream writes Alex2 HPF bits for a board whose Alex2
settings the operator can never see (`console.cs:15435` in the driver list,
`setup.cs:6458-6464` absent from the tab list, both `[v2.10.3.15]`). Match upstream, or expose the
tab? Matching upstream is the source-first default; exposing the tab is arguably the better
product.

**Q11. G2E divergence disclosure.** On a G2E, Thetis's own shared-chain rule would deafen the
higher slice (§16.2.3 step 4 case 7). We diverge. Should the operator see a one-time note
explaining that NereusSDR behaves differently from Thetis here?

---

### 16.8 Deliberately deferred

| Item | Reason |
| --- | --- |
| **Bpf1 ladder implementation itself** (`ladderFor(board)`, the editable range table, replacing `computeHpf`'s unconditional use) | It is a prerequisite (§16.1.8 item 1), not part of the router. It needs its own commit with its own Thetis port and its own wire tests, and it touches P1 as well as P2. |
| **P2 TX LPF fix** | Prerequisite (§16.1.8 item 3). Separate defect, separate fix, separate bench row. Blocking for shipping multi-slice, not for landing the router behind a flag. |
| **`hasAlex2` split into four fields** | Touches 13 capability rows and the Setup tab gating. Mechanical but wide; do it as its own change so the diff is reviewable. |
| **`SettingsHygiene` BPF1 set fix** | Data-loss bug with a one-line fix, but it should ship with a migration decision (do we restore erased edges?), so it is not folded in here. |
| **P1 chain routing** | P1 has no DDC sync concept and a different DDC map per family (§16.3.2). `invokeCodecDdcAssignment` is P2-gated today (`src/models/RadioModel.cpp:12192`). The router runs on P1 with `rxFilterChainCount` from the row and produces engagement only, no assignment. Full P1 integration stays in Sub-Epic C. |
| **Allocator group-pressure feedback** (allocator prefers co-hosting a new slice with slices in the same filter range) | Optimisation, not correctness. It couples two layers that are currently cleanly separated, and the win is small when `addSlice` already seeds from the active slice's frequency (`src/models/RadioModel.cpp:3482-3486`). |
| **More than two chains** | No OpenHPSDR SKU has more than two drivable RX filter chains. The router is written with `rxFilterChainCount` as a variable so a third would not be a rewrite, but nothing is tested above two. |
| **Per-slice antenna** | Not a deferral, a removal. There is no such control on any OpenHPSDR radio (§16.1.6). The `AntennaPickerMenu` wording is a bug, not an unimplemented feature. |
| **Out-of-passband shading** | Blocked on Q5. |
| **HL2 and other P1 wideband** | Already deferred by §7 item 8; unchanged. |

---