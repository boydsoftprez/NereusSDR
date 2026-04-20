# All-Board Radio-Control Parity — Spec (Phase 3P)

**Status:** Draft, pending maintainer approval
**Date:** 2026-04-20
**Author:** JJ Boyd — KG4VCF
**Upstream stamps:** `[ramdor 501e3f5]` (archived canonical) · `[mi0bot c26a8a4]` (v2.10.3.13-beta2, HL2-active)
**Originating bug reports:**
- Hermes Lite 2: bandpass filter does not switch on band / VFO change
- Hermes Lite 2: S-ATT (step attenuator) has no audible / hardware effect

---

## 1. Goal & non-goals

### Goal

Wire-level parity with Thetis for every HPSDR Protocol 1 and Protocol 2 board type NereusSDR enumerates in `HPSDRModel` — so NereusSDR sends the same bytes Thetis sends, for the same user action, on every board. Parity verified by hand-derived byte-table unit tests, not by "looks right on my radio."

This spec covers eight phases (A–H) that together close the gap. Phase A is the minimum to ship both reported bug fixes; B–F raise the rest of the radio-control surface to byte-identical Thetis parity; G–H complete the userland: calibration, status feedback, settings hygiene.

**At the end of phase H, NereusSDR's hardware/radio-control surface is userland-complete** vs Thetis — every Setup page a Thetis user reaches for has a NereusSDR equivalent, every status readout a Thetis user expects is exposed.

### Non-goals

- TX-side DSP, keyer, and audio path (Phase 3M line of work).
- PureSignal (Phase 3M-4).
- New DSP features or UI redesign.
- SmartSDR / FlexRadio compatibility.
- Firmware-side changes.

We are fixing only the **wire-protocol compose layer**, the **directly-adjacent radio-control UI plumbing**, and the **diagnostic surface** users need to operate the radio confidently.

---

## 2. Phase roadmap

| Phase | Branch | Scope | Userland deliverable | Fixes reported bug? |
|---|---|---|---|---|
| **A** | `phase3p-a-p1-wire-parity` | `IP1Codec` interface + 4 subclasses (`Standard`, `Hl2`, `AnvelinaPro3`, `RedPitaya`); shared `AlexFilterMap`; `setReceiverFrequency` / `setTxFrequency` recompute filter bits; byte-table unit tests | RxApplet S-ATT slider range widens to `BoardCapabilities::attenuator.maxDb` (HL2 → 0–63 dB); BPF auto-switches on tune | **Yes — both** |
| **B** | `phase3p-b-p2-wire-parity` | `IP2Codec` interface + `OrionMkII` / `Saturn` subclasses; Saturn BPF1 band edges; per-ADC RX1 preamp; TX ATT setter scaffolding; ADC OVL splits per-ADC | Hardware → Antenna/ALEX sub-tabs Alex-1/Alex-2 Filters (HPF bypass + per-band edge editors + Saturn BPF1 panel); RxApplet ADC OVL splits to OVL₀/OVL₁ on dual-ADC boards | No |
| **C** | `phase3p-c-preamp-combo` | Full `SetComboPreampForHPSDR` port; 7-mode preamp encoding; folds in PR #34 tail | RxApplet preamp combo populates per-board (HL2 → 4 items, Alex-equipped → 7 items) | No |
| **D** | `phase3p-d-oc-matrix` | `OcMatrix` model (14 × 7 × RX/TX); HF + SWL sub-sub-tabs; TX pin action mapping; USB BCD; external PA control; live OC pin state | Hardware → OC Outputs page populated 1:1 with Thetis (RX matrix + TX matrix + pin actions + USB BCD + ext PA + reset) | No |
| **E** | `phase3p-e-hl2-ioboard` | `IoBoardHl2` I2C TLV queue; 12-step state machine; HL2 bandwidth monitor; N2ADR filter selector; register state diagnostic | Hardware → HL2 I/O page (was placeholder): connection status + register table + state machine viz + I2C log + bandwidth monitor mini | Fixes *different* HL2 bug (external N2ADR shield) |
| **F** | `phase3p-f-accessories` | Apollo PA + ATU + LPF; Alex II RX2 routing; PennyLane legacy PA; per-band antenna assignment | Hardware → Antenna/ALEX → Antenna Control sub-tab (per-band TX/RX1/RX2 assignment + Block-TX safety); RxApplet antenna buttons populate per-band per Alex matrix; Setup pages for Apollo / Alex II / Penny | No |
| **G** | `phase3p-g-calibration` | Frequency calibration (HPSDR correction factor + 10 MHz ext ref); Level calibration (per-band reference + 6m LNA offsets); TX Display Cal; consolidate with existing PA Current calc | Hardware → Calibration page (renamed from PA Calibration) modeled 1:1 on Thetis General → Calibration with 5 group boxes | No |
| **H** | `phase3p-h-status-hygiene` | Status packet readouts (PA temp/current/SWR/PTT-source); per-MAC settings hygiene (clamp warnings, mismatch alerts); Connection Quality history; Export/Import config; live LED indicators on existing pages (Alex-2 active filters, OC pin state) | Diagnostics → Radio Status page (PA, power, PTT cards + connection quality + hygiene actions); plus Connection Quality, Settings Validation, Export/Import Config sub-tabs | No |

**Branch strategy:** one branch per phase, one PR per phase. PR #34 (3G-13 step attenuator) lands first as-is with its 5-bit encoding (correct for Hermes/Orion); Phase A adds the HL2 6-bit branch on top.

---

## 3. Architecture — hybrid codec + capabilities

Thetis itself has two kinds of per-board divergence:

- **Behavioral splits** — literally different functions in C source. `WriteMainLoop` vs `WriteMainLoop_HL2`. `AnvelinaPro3` cycles 17 banks vs 16. RedPitaya carves out ADC1 under MOX. We mirror these with **per-board codec subclasses**.
- **Parameter differences** — bit masks, enable bits, bank counts, combo contents. We mirror these with **BoardCapabilities fields** already in place (`BoardCapabilities.h`).

### Interface

```cpp
// src/core/codec/IP1Codec.h — NereusSDR-original (no attribution row)
class IP1Codec {
public:
    virtual ~IP1Codec() = default;
    virtual void composeCcForBank(int bank,
                                  const CodecContext& ctx,
                                  quint8 out[5]) const = 0;
    virtual int  maxBank() const = 0;                          // 16, 17, or 18
    virtual bool usesI2cIntercept() const { return false; }    // HL2 only
};
```

`CodecContext` is a POD of everything the codec needs (MOX bit, `m_stepAttn[0..2]`, `m_rxPreamp[0..2]`, Alex HPF/LPF bits, TX drive, freq words, OC byte, etc.) so codecs are pure functions of state and capabilities — trivially unit-testable.

### Subclass tree (Phase A)

```
IP1Codec
├── P1CodecStandard        — ramdor WriteMainLoop, banks 0–16
│   ├── P1CodecAnvelinaPro3 — adds bank 17 extra OC
│   └── P1CodecRedPitaya    — overrides bank 12 C1 (ADC1 MOX carve-out)
└── P1CodecHl2              — mi0bot WriteMainLoop_HL2, banks 0–18
                              (6-bit ATT + MOX-branch, bank 17 TX latency,
                               bank 18 reset-on-disconnect, I2C intercept)
```

P2 mirrors this in Phase B with `IP2Codec`, `P2CodecOrionMkII`, `P2CodecSaturn`.

### `BoardCapabilities` additions

```cpp
struct Attenuator {
    bool   present;
    int    minDb, maxDb;
    quint8 mask;          // 0x1F (5-bit) or 0x3F (6-bit HL2)
    quint8 enableBit;     // 0x20 (std) or 0x40 (HL2)
    bool   moxBranchesAtt; // HL2 sends tx_step_attn when MOX asserted
};
```

`BoardCapabilities` grows additional fields in later phases (`ocPinCount`, `hasApollo`, `hasAlex`, `hasPennyLane`, `p2SaturnBpf1Edges`, `p2PreampPerAdc`, `paTempLimit`, `paCurrentLimit`) — all populated per board in `BoardCapabilities.cpp`.

---

## 4. Attribution & provenance — non-negotiable, per-phase

Every new `.h` / `.cpp` in phases A–H is a **new attribution event** under the NereusSDR source-first porting protocol (`CLAUDE.md` §Source-First, `docs/attribution/HOW-TO-PORT.md`).

### Per-phase workflow — run for every new file

1. **Declare the port.** State: *"this file ports from `<ramdor path>:<range>` `[ramdor 501e3f5]` and/or `<mi0bot path>:<range>` `[mi0bot c26a8a4]`"*.
2. **Select the template variant** (`docs/attribution/HEADER-TEMPLATES.md`):
   - `ChannelMaster/*` sources (LGPL, Samphire-clean) → `thetis-no-samphire` or `mi0bot`.
   - `Console/*` sources (`console.cs`, `setup.cs`, `display.cs`) → `thetis-samphire`.
   - Multiple upstream files → `multi-source` with **every** header, separated by `// --- From <filename> ---`.
3. **Copy upstream headers verbatim.** No paraphrasing, no merging. Add the trailing *Modification history (NereusSDR)* block with port date, human author (`JJ Boyd / KG4VCF`), and AI tooling disclosure.
4. **Add a PROVENANCE row** in `docs/attribution/THETIS-PROVENANCE.md` in the **same commit** that introduces the file.
5. **Stamp every ported logic line.** Both upstream stamps when logic is present in both forks:

   ```cpp
   // From Thetis networkproto1.c:770 [ramdor 501e3f5 / mi0bot c26a8a4] — canonical ATT encoding
   out[4] = (m_stepAttn[0] & 0x1F) | 0x20;
   ```

   Single stamp when fork-specific:

   ```cpp
   // From mi0bot-Thetis networkproto1.c:1099-1102 [mi0bot c26a8a4] — HL2-only ATT encoding
   const quint8 val = ctx.mox ? m_txStepAttn[0] : m_stepAttn[0];
   out[4] = (val & 0x3F) | 0x40;
   ```

6. **CI gates.** `scripts/verify-thetis-headers.py` and `scripts/check-new-ports.py` must pass. Pre-push hook (`scripts/install-hooks.sh`) runs both. PRs that add ported code without a matching PROVENANCE row fail CI.

### Inline comment preservation

All `//` comments from the upstream source on or above a ported line are preserved verbatim in the C++ translation — including `// MW0LGE`, `// MI0BOT`, `// DH1KLM`, `// G8NJJ`, `// -W2PA`, `// TODO`, `// FIXME`, behavioral notes. Where the translation restructures code so the comment no longer sits inline, annotate:

```cpp
// unsure why this was forced, but left as-is for all radios other than Red Pitaya
// [original inline comment from networkproto1.c:607-608]
```

### Definition of Done — attribution

A phase does **not** merge until:

- Every new file has a verbatim header block.
- Every new file has a row in `THETIS-PROVENANCE.md`.
- Every ported logic block has an inline `[ramdor ...]` and/or `[mi0bot ...]` stamp.
- `verify-thetis-headers.py` and `check-new-ports.py` pass.

Reviewer checklist template (added to `.github/pull_request_template.md` in Phase A):

```markdown
- [ ] New files have upstream headers (verbatim, multi-source where applicable)
- [ ] THETIS-PROVENANCE.md updated in this PR
- [ ] Every ported line has [ramdor <sha>] / [mi0bot <sha>] stamp
- [ ] verify-thetis-headers.py and check-new-ports.py pass
```

---

## 5. Source-priority methodology

When ramdor and mi0bot diverge on the same byte, the spec resolves the conflict case-by-case using this block format. Every divergence in phases A–H gets one such block:

```
### <Bank or register>

- ramdor <file>:<line>   → <encoding>                           [ramdor 501e3f5]
- mi0bot <file>:<line>   → <encoding>                           [mi0bot c26a8a4]
- mi0bot <file>:<line>   → <fork-specific encoding if any>      [mi0bot c26a8a4]
- Decision: <which codec uses which encoding>
- Rationale: <one sentence>
```

**Default rules** (override requires an explicit rationale):

1. Where ramdor and mi0bot agree, ramdor is the canonical citation; mi0bot cited as "matches ramdor."
2. Where they differ *on HL2*, mi0bot wins.
3. Where they differ *outside HL2* (rare — mi0bot mostly adds, rarely changes), call it out explicitly in the decision line.
4. Where mi0bot supplies HL2-only functionality absent from ramdor (`WriteMainLoop_HL2`, I2C intercept, bandwidth monitor, N2ADR filter), mi0bot is the sole source.

### Userland parity rule

For Setup-page IA, page density, and group-box layout, default to matching Thetis 1:1 (see `feedback_thetis_userland_parity` memory). NereusSDR-native spin allowed where it serves the user (e.g., live LED indicators on existing pages, color-coded RX/TX matrix grids, register state diagnostic on HL2 I/O page) — flagged explicitly in each phase's "Spin from Thetis" notes.

---

## 6. Phase A — P1 wire-bytes parity foundation

**Branch:** `phase3p-a-p1-wire-parity`

### 6.1 Files created

| Path | Ports from | Variant |
|---|---|---|
| `src/core/codec/IP1Codec.h` | *(interface — no port)* | — |
| `src/core/codec/CodecContext.h` | *(POD — no port)* | — |
| `src/core/codec/P1CodecStandard.{h,cpp}` | `ChannelMaster/networkproto1.c:419-698` `[ramdor 501e3f5]` | `thetis-no-samphire` |
| `src/core/codec/P1CodecAnvelinaPro3.{h,cpp}` | `ChannelMaster/networkproto1.c:668-674, 682` `[ramdor 501e3f5]` | `thetis-no-samphire` |
| `src/core/codec/P1CodecRedPitaya.{h,cpp}` | `ChannelMaster/networkproto1.c:606-616` `[ramdor 501e3f5]` (DH1KLM) | `thetis-no-samphire` |
| `src/core/codec/P1CodecHl2.{h,cpp}` | `mi0bot-Thetis/ChannelMaster/networkproto1.c:869-1201` `[mi0bot c26a8a4]` | `mi0bot` |
| `src/core/codec/AlexFilterMap.{h,cpp}` | `Console/console.cs:6830-6942, 7168-7234` `[ramdor 501e3f5]` | `thetis-samphire` |
| `tests/core/codec/P1CodecStandardTest.cpp` | *(test fixture — data sourced from upstream, cited per row)* | — |
| `tests/core/codec/P1CodecAnvelinaPro3Test.cpp` | ditto | — |
| `tests/core/codec/P1CodecRedPitayaTest.cpp` | ditto | — |
| `tests/core/codec/P1CodecHl2Test.cpp` | ditto | — |
| `tests/core/codec/AlexFilterMapTest.cpp` | ditto | — |

### 6.2 Files modified

| Path | Change |
|---|---|
| `src/core/P1RadioConnection.h` | Remove `composeCcForBank` / `composeCcBank*` declarations; add `std::unique_ptr<IP1Codec> m_codec` |
| `src/core/P1RadioConnection.cpp` | `composeCcForBank` becomes a one-liner delegating to `m_codec`; `applyBoardQuirks()` instantiates the codec from `m_hardwareProfile.model`; `setReceiverFrequency` / `setTxFrequency` call `AlexFilterMap::computeHpf/Lpf` and cache bits |
| `src/core/P2RadioConnection.cpp` | Delete local `computeAlexHpf` / `computeAlexLpf` (lines 916-968); call `AlexFilterMap`. Byte output byte-identical before/after |
| `src/core/BoardCapabilities.{h,cpp}` | Add `Attenuator::{mask, enableBit, moxBranchesAtt}`; populate per-board |
| `src/gui/applets/RxApplet.{h,cpp}` | S-ATT spinbox `setMaximum()` reads from `BoardCapabilities::attenuator.maxDb` (HL2 → 63, others → 31). No layout change |
| `docs/attribution/THETIS-PROVENANCE.md` | 5 new rows (one per new ported file) |
| `CHANGELOG.md` | "Fixed: HL2 bandpass filter now switches on band change. Fixed: HL2 S-ATT now attenuates." |
| `.github/pull_request_template.md` | Add attribution checklist (one-time setup that subsequent phases reuse) |

### 6.3 Byte divergence catalog (Phase A)

Banks that are byte-identical across ramdor, mi0bot, and NereusSDR today are summarized in one line; divergent banks get a full block.

| Bank | C0 | Status | Notes |
|---|---|---|---|
| 0 | `0x00 \| XmitBit` | identical | Sample rate / OC / preamp / antenna / duplex / NDDC |
| 1 | `0x02 \| XmitBit` | identical | TX VFO |
| 2 | `0x04 \| XmitBit` | identical | RX1 VFO |
| 3 | `0x06 \| XmitBit` | identical | RX2 VFO |
| 4 | `0x1C \| XmitBit` | identical | ADC map + TX drive ATT |
| 5–9 | `0x08 \| XmitBit` … `0x10` | identical | RX3–RX7 VFOs |
| 10 | `0x12 \| XmitBit` | **divergent (BPF bug)** | Alex HPF/LPF bits never computed in NereusSDR P1 — see §6.3.1 |
| 11 | `0x14 \| XmitBit` | **divergent (S-ATT bug)** | HL2 needs 6-bit mask + 0x40 enable + MOX TX/RX branch — see §6.3.2 |
| 12 | `0x16 \| XmitBit` | partial divergence | RedPitaya MOX carve-out matches ramdor; mi0bot drops the gate in HL2 path — see §6.3.3 |
| 13 | `0x1E \| XmitBit` | identical | CW enable / sidetone level |
| 14 | `0x20 \| XmitBit` | identical | CW hang / sidetone freq |
| 15 | `0x22 \| XmitBit` | identical | EER PWM |
| 16 | `0x24 \| XmitBit` | identical | BPF2 |
| 17 | `0x26` / `0x2E` | **divergent (collision)** | ramdor + mi0bot standard: AnvelinaPro3 extra OC. mi0bot HL2: TX latency + PTT hang — see §6.3.4 |
| 18 | `0x74` | **HL2-only** | mi0bot reset-on-disconnect — see §6.3.5 |

#### 6.3.1 Bank 10 C3/C4 — Alex HPF / LPF bits

- ramdor `networkproto1.c:583-590` → sends `C3 = alex_hpf`, `C4 = alex_lpf` as provided by upper layer `[ramdor 501e3f5]`
- mi0bot `networkproto1.c:752-759` → identical to ramdor `[mi0bot c26a8a4]`
- **NereusSDR current:** `P1RadioConnection.cpp:1253-1254` emits `m_alexHpfBits` / `m_alexLpfBits` — but both are initialized to `0` in `P1RadioConnection.h:225-226` and **never recomputed**. `setReceiverFrequency` at `P1RadioConnection.cpp:749-754` only stores the Hz. NereusSDR P2 ports the compute at `P2RadioConnection.cpp:916-968` and calls it from `setReceiverFrequency` — P1 never got that treatment.
- **Decision:** Lift P2's compute into shared `src/core/codec/AlexFilterMap.{h,cpp}` (ports `Console/console.cs:6830-6942` [ramdor 501e3f5] for HPF, `7168-7234` for LPF). `P1RadioConnection::setReceiverFrequency` and `setTxFrequency` call the shared helper and update `CodecContext.alexHpfBits` / `alexLpfBits`. `P2RadioConnection` drops its copy and calls the shared helper. Both connections gate the C&C send on `BoardCapabilities::hasAlexFilters`.
- **Rationale:** The logic is frequency→bits math, identical on P1 and P2, authored by Samphire/W2PA in `console.cs`. Shared helper is honest to the source and avoids drift.

#### 6.3.2 Bank 11 C4 — RX step attenuator + enable bit

- ramdor `networkproto1.c:601` → `(adc[0].rx_step_attn & 0x1F) | 0x20` `[ramdor 501e3f5]`
- mi0bot `networkproto1.c:770` → identical to ramdor `[mi0bot c26a8a4]` (standard path)
- mi0bot `networkproto1.c:1099-1102` → `(XmitBit ? tx_step_attn : rx_step_attn) & 0x3F | 0x40` `[mi0bot c26a8a4]` (HL2-only, with MOX branch and 6-bit range)
- **NereusSDR current:** `P1RadioConnection.cpp:1266` uses `(m_stepAttn[0] & 0x1F) | 0x20` for every board. HL2 firmware interprets the enable bit at the wrong position and the value is silently truncated past 31 dB. No MOX branch.
- **Decision:** `P1CodecStandard` emits ramdor form. `P1CodecHl2` emits HL2 form with MOX branch and 6-bit mask.
- **Rationale:** mi0bot's HL2 path is the sole source for HL2 encoding; mi0bot's standard path matches ramdor so ramdor wins as canonical citation on the non-HL2 codec.

#### 6.3.3 Bank 12 C1 — RX step attenuator ADC1

- ramdor `networkproto1.c:606-616` → `(XmitBit && model != REDPITAYA) ? 0x1F : adc[1].rx_step_attn`, masked to 5 bits for REDPITAYA, enable `| 0x20` `[ramdor 501e3f5]`
- mi0bot `networkproto1.c:775-786` → identical to ramdor `[mi0bot c26a8a4]` (standard path)
- mi0bot `networkproto1.c:1107-1111` → drops the REDPITAYA gate since HL2 is known not to be RedPitaya `[mi0bot c26a8a4]` (HL2 path)
- **NereusSDR current:** `P1RadioConnection.cpp:1273-1280` correctly mirrors ramdor's REDPITAYA gate. ✓ For non-HL2 boards this is already right.
- **Decision:** `P1CodecStandard` keeps the REDPITAYA gate. `P1CodecHl2` drops it (mi0bot style) because it has proved the model is HL2.
- **Rationale:** Preserve existing correct behavior on Hermes/Orion/RedPitaya; HL2 doesn't need the gate.

#### 6.3.4 Bank 17 — AnvelinaPro3 extra OC vs HL2 TX latency

- ramdor `networkproto1.c:668-674` + `:682` → `C0 = 0x26`, `C1 = oc_output_extras & 0x0F`, gated on `HPSDRModel_ANVELINAPRO3` via `end_frame = 17` `[ramdor 501e3f5]`
- mi0bot standard `networkproto1.c:837-843` → identical to ramdor `[mi0bot c26a8a4]`
- mi0bot HL2 `networkproto1.c:1162-1168` → `C0 = 0x2E`, `C3 = ptt_hang & 0x1F`, `C4 = tx_latency & 0x7F` `[mi0bot c26a8a4]` (HL2-only)
- **Decision:** `P1CodecStandard::maxBank()` returns 16. `P1CodecAnvelinaPro3` overrides `maxBank()` → 17 and emits the ramdor bank 17. `P1CodecHl2::maxBank()` returns 18 and emits the mi0bot HL2 bank 17 (TX latency) and bank 18 (reset).
- **Rationale:** The two bank-17 meanings are genuinely different bytes; codec subclassing keeps them honest without "if (HL2) else if (AP3)" soup in one function.

#### 6.3.5 Bank 18 — HL2 reset-on-disconnect

- ramdor → no case 18; the frame index never reaches 18 `[ramdor 501e3f5]`
- mi0bot `networkproto1.c:1170-1176` → `C0 = 0x74`, `C4 = reset_on_disconnect & 0x01` `[mi0bot c26a8a4]`
- **Decision:** `P1CodecHl2` only.
- **Rationale:** HL2-only firmware feature with no upstream in ramdor.

### 6.4 Frequency → filter bits recomputation

`AlexFilterMap.h` exports pure functions:

```cpp
namespace nereus::codec::alex {
    // From Thetis console.cs:6830-6942 [ramdor 501e3f5]
    quint8 computeHpf(double freqMhz);

    // From Thetis console.cs:7168-7234 [ramdor 501e3f5]
    quint8 computeLpf(double freqMhz);
}
```

Callers in Phase A:

- `P1RadioConnection::setReceiverFrequency(idx, hz)` — stores Hz; if `idx == 0`, also calls `computeHpf(hz / 1e6)` and stores in `m_alexHpfBits`.
- `P1RadioConnection::setTxFrequency(hz)` — stores Hz; calls `computeLpf(hz / 1e6)` and stores in `m_alexLpfBits`.
- `P2RadioConnection::setReceiverFrequency` — already does the right thing; switches from local helper to `AlexFilterMap`.

Gate on `m_caps->hasAlexFilters` before sending. HL2 without the N2ADR shield has `hasAlexFilters = false`; the bytes remain zero and no harm done. Phase E adds the N2ADR path.

### 6.5 Testing strategy

**Fixture structure:** one test file per codec subclass (`tests/core/codec/P1Codec<Board>Test.cpp`), plus `AlexFilterMapTest.cpp` for the freq-math helpers.

**Row format:** `{bank, mox, scenario → expected 5 bytes}` with upstream citation per row:

```cpp
// From networkproto1.c:1099-1102 [mi0bot c26a8a4]
TEST_F(P1CodecHl2Test, Bank11_Tx_AttAt20dB_Hl2Encoding) {
    CodecContext ctx = makeCtx();
    ctx.mox = true;
    ctx.txStepAttn[0] = 20;
    quint8 out[5];
    codec_.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[0], 0x15);                            // C0 = 0x14 | XmitBit
    EXPECT_EQ(out[4], (20 & 0x3F) | 0x40);              // 0x54 — 6-bit mask, bit-6 enable
}
```

**Depth:** representative scenarios per bank — min / mid / max input, MOX on and off, per-board quirks exercised. ~500–700 assertions total. Runs <1 s. Hand-derived from the divergence catalog with explicit upstream citation on each row.

**Regression freeze:** before the refactor, capture each current `P1RadioConnection::composeCcForBank` output for every (bank, model, MOX) tuple into a baseline JSON. The byte-table tests assert the new codecs produce the same bytes for every non-HL2 board. Hermes/Orion/RedPitaya/AnvelinaPro3 ship byte-identical to main.

### 6.6 Definition of Done — Phase A

1. `phase3p-a-p1-wire-parity` branch merged into `main`.
2. `setAttenuator(20)` on HL2 → byte trace `C4 = 0x54` in bank 11 during RX, `C4 = 0x54` with MOX=1 in TX.
3. `setReceiverFrequency(14_100_000)` on HL2 → byte trace bank 10 `C3 = 0x01` (13 MHz HPF), `C4 = 0x01` (30/20 LPF).
4. RxApplet S-ATT slider on HL2 ranges 0–63; on Hermes/Orion ranges 0–31.
5. Byte-identical output vs pre-refactor main for every non-HL2 board (regression-freeze test).
6. `verify-thetis-headers.py` + `check-new-ports.py` pass.
7. 5 new rows in `THETIS-PROVENANCE.md`.
8. Every ported logic block in the 6 new files has a `[ramdor …]` and/or `[mi0bot …]` stamp.
9. No regression in existing P1 test suite.
10. CHANGELOG entry.

---

## 7. Phase B — P2 wire-bytes audit + Alex filter UI

**Branch:** `phase3p-b-p2-wire-parity`

### 7.1 Wire-byte work

Codec subclasses for P2 + the Saturn band-edge override:

| Path | Ports from | Variant |
|---|---|---|
| `src/core/codec/IP2Codec.h` | *(interface — no port)* | — |
| `src/core/codec/P2CodecOrionMkII.{h,cpp}` | `ChannelMaster/network.c:821-1248` `[ramdor 501e3f5]` | `thetis-no-samphire` |
| `src/core/codec/P2CodecSaturn.{h,cpp}` | `Console/console.cs:6944-7040` `[ramdor 501e3f5]` (G8NJJ setBPF1ForOrionIISaturn) + P2 packet deltas | `multi-source` |

### 7.2 Setup UI work — Antenna / ALEX → Alex-1 Filters and Alex-2 Filters sub-sub-tabs

The existing Hardware → Antenna / ALEX tab gets a third sub-level. Two new sub-sub-tabs (Alex-1 Filters, Alex-2 Filters; the Antenna Control sub-sub-tab is built in Phase F):

**Alex-1 Filters page** (ports `Console/setup.designer.cs:tpAlexFilterControl` `[ramdor 501e3f5]`) — three columns:
- **Alex HPF Bands** — 5 master toggles (HPF Bypass, HPF Bypass on TX, HPF Bypass on PureSignal, Disable 6m LNA on TX, Disable 6m LNA on RX) + 6 HPF rows (1.5 / 6.5 / 9.5 / 13 / 20 MHz + 6m bypass) each with bypass checkbox and Start/End frequency editors
- **Alex LPF Bands** — 7 LPF rows (160 / 80 / 60-40 / 30-20 / 17-15 / 12-10 / 6m) with Start/End editors; no master toggles
- **Saturn BPF1 Bands** — same shape as HPF, dimmed/hidden when board ≠ ANAN-G2 / G2-1K (NereusSDR spin: Thetis shows always; we gate on `BoardCapabilities::p2SaturnBpf1Edges`)

**Alex-2 Filters page** (ports `Console/setup.designer.cs:tpAlex2FilterControl` `[ramdor 501e3f5]`) — two columns:
- **Alex-2 HPF Bands** — LED row (live filter selection from ep6 status — Phase H), master "ByPass / 55 MHz BPF" toggle, 6 HPF rows with bypass + Start/End editors
- **Alex-2 LPF Bands** — LED row (live), 7 LPF rows with Start/End editors, no master toggles (RX-only path)
- "Active" detection bar at top showing board status + currently-selected HPF/LPF
- Whole tab auto-hides when board has no Alex-2 (HL2, Hermes)

### 7.3 RxApplet ADC OVL split

Today's single `OVL` badge splits into `OVL₀` + `OVL₁` for dual-ADC boards (Orion-MKII / 7000D / 8000D / AnvelinaPro3 / G2). Wired off the per-ADC bits in the P2 status packet. Single-ADC boards (HL2, Hermes) keep one badge.

### 7.4 Files created (UI)

| Path | Ports from | Variant |
|---|---|---|
| `src/gui/setup/hardware/AntennaAlexAlex1Tab.{h,cpp}` | `Console/setup.designer.cs:tpAlexFilterControl` (~lines 23385-25538) `[ramdor 501e3f5]` | `thetis-samphire` |
| `src/gui/setup/hardware/AntennaAlexAlex2Tab.{h,cpp}` | `Console/setup.designer.cs:tpAlex2FilterControl` (~lines 25539-26857) `[ramdor 501e3f5]` | `thetis-samphire` |
| `tests/core/codec/P2CodecOrionMkIITest.cpp` | per-row citations | — |
| `tests/core/codec/P2CodecSaturnTest.cpp` | per-row citations | — |

### 7.5 Files modified

| Path | Change |
|---|---|
| `src/gui/setup/hardware/AntennaAlexTab.{h,cpp}` | Splits into a parent with sub-sub-tabs (Antenna Control / Alex-1 Filters / Alex-2 Filters) |
| `src/gui/applets/RxApplet.{h,cpp}` | ADC OVL splits per-ADC for boards with `BoardCapabilities::adcCount > 1` |

### 7.6 Definition of Done — Phase B

1. P2 codec subclasses chosen from `m_hardwareProfile.model` at connect.
2. ANAN-G2 / G2-1K use Saturn BPF1 band edges when configured (auto-hides on non-Saturn).
3. OrionMKII per-ADC preamp wires through to byte 1403.
4. `AlexFilterMap` calls from P2 byte-identical pre/post.
5. Byte-table tests cover OrionMKII + Saturn scenarios.
6. Antenna / ALEX sub-sub-tabs render and persist edits per-MAC.
7. RxApplet ADC OVL splits on dual-ADC boards.
8. Attribution Definition of Done from §4.

---

## 8. Phase C — Preamp combo full port + RxApplet population

**Branch:** `phase3p-c-preamp-combo`

### Scope

- Complete port of `SetComboPreampForHPSDR` from `Console/console.cs:40755-40825` `[ramdor 501e3f5]`. Today `BoardCapabilities::preampComboItems()` has most of the table; this phase audits the last per-board case and verifies the 7-mode `PreampMode` enum mapping at `console.cs:28350-28404`.
- Wire `RxApplet` preamp combo to populate per `BoardCapabilities::preampComboItems(model, alexPresent)`. Today the combo shows a static set; after Phase C it shows board-specific items (HL2 → 4 items including "-30 dB"; Alex-equipped Hermes/Orion → 7 items including "On" and "-50 dB").
- `StepAttenuatorController` encodes the 7-mode selection into the preamp C&C bits (`P1RadioConnection` bank 11 C1 bits `[6:0]`).
- Folds in PR #34's tail: final polish, test coverage.
- Per-band persistence (already in `StepAttenuatorController`).

### Files created

- `src/core/PreampMode.h` — if not already extracted; otherwise just extend existing enum with stamps.
- `tests/core/PreampComboTest.cpp` — one test per board verifying combo contents match Thetis.

### Definition of Done

1. Changing the preamp combo on every board model produces the Thetis-matching combo set.
2. Selected mode encodes into correct bits.
3. RxApplet preamp combo on HL2 shows 4 items (Off / -10 / -20 / -30 dB); on Alex-equipped Orion shows 7 items.
4. Per-band persistence round-trips.
5. Attribution DoD from §4.

---

## 9. Phase D — OC matrix UI

**Branch:** `phase3p-d-oc-matrix`

### Scope

- New `OcMatrix` model: 14 bands × 7 pins × {RX, TX} = 196 bits. Ports `Console/HPSDR/Penny.cs:33-150` `[ramdor 501e3f5]` (`RXABitMasks` / `TXABitMasks`).
- New Setup → Hardware → OC Outputs page (replaces today's empty placeholder). Ports `Console/setup.designer.cs:tpOCHFControl` (~13658-13666) and `tpOCSWLControl` `[ramdor 501e3f5]`.
- HF and SWL **sub-sub-tabs** matching Thetis's split.
- Per-MAC persistence under `AppSettings` `hardware/<mac>/oc/{rx,tx}/<band>/pin<n>` and `.../actions/pin<n>/{mox,tune,2tone,vox,pain}`.
- `P1CodecStandard` and `P1CodecHl2` source `CodecContext.ocByte` from `OcMatrix::maskFor(currentBand, mox)` instead of today's pass-through.

### OC Outputs page contents (Thetis 1:1, color-coded RX/TX)

- **Top toggle row:** Penny Ext Control enable, N2ADR Filter (HERCULES), Allow hot switching, Reset OC defaults button
- **RX OC matrix** (blue, left): 14 bands × 7 pins, click cell to toggle. NereusSDR spin: GEN/WWV rows greyed (no OC sense for those), XVTR active. Thetis is 12 bands.
- **TX OC matrix** (red, right): same shape; distinct color so the two grids are scannable
- **TX Pin Action mapping** (bottom-left): per-pin checkboxes for what triggers it during TX (MOX / Tune / 2-Tone / VOX / PA-In)
- **USB BCD output** (bottom-middle): enable + format + invert
- **External PA control** (bottom-middle): model + bias delay
- **Live OC pin state** (bottom-right): 7 LED dots polled at 100 ms (Phase H lights this up)

### Files created

- `src/core/OcMatrix.{h,cpp}` (port from `HPSDR/Penny.cs:33-150` `[ramdor 501e3f5]`, variant `thetis-no-samphire`)
- `src/gui/setup/hardware/OcOutputsHfTab.{h,cpp}` (port from `setup.designer.cs:tpOCHFControl` `[ramdor 501e3f5]`, variant `thetis-samphire`)
- `src/gui/setup/hardware/OcOutputsSwlTab.{h,cpp}` (port from `tpOCSWLControl` `[ramdor 501e3f5]`)
- Tests.

### Files modified

- `src/gui/setup/hardware/OcOutputsTab.{h,cpp}` — turn into a parent with HF/SWL sub-sub-tabs.

### Definition of Done

1. OC matrix UI matches Thetis 1:1 within the 14-band scheme.
2. User edits → OC byte updates on next C&C frame.
3. TX pin action mapping persists per-MAC.
4. Per-MAC persistence for entire matrix.
5. Codec byte-table tests extended with OC matrix scenarios.
6. Attribution DoD from §4.

---

## 10. Phase E — HL2 IoBoard I2C + N2ADR filter + diagnostic UI

**Branch:** `phase3p-e-hl2-ioboard`

### Scope

- `IoBoardHl2` class ports `mi0bot-Thetis Console/console.cs:25781-25945` `[mi0bot c26a8a4]` — 12-step state machine (`UpdateIOBoard`) + I2C TLV queue from `mi0bot-Thetis/ChannelMaster/network.h:113-148` `[mi0bot c26a8a4]`.
- `HermesLiteBandwidthMonitor` ports `mi0bot-Thetis/ChannelMaster/bandwidth_monitor.{c,h}` `[mi0bot c26a8a4]`. Closes the long-open `TODO(3I-T12)` at `P1RadioConnection.cpp:892, 939, 1416-1462`.
- `P1CodecHl2` gains I2C intercept mode — when the I2C queue is non-empty, frame compose is overridden to carry I2C TLV bytes per `mi0bot-Thetis/ChannelMaster/networkproto1.c:898-943` `[mi0bot c26a8a4]`.
- Read path — `MetisReadThreadMainLoop_HL2` equivalent in NereusSDR parses I2C responses at the ep6 input; ports `mi0bot-Thetis networkproto1.c:422-585` `[mi0bot c26a8a4]`.
- HL2 CW keyer bit 3 (cwx_ptt) — one-line conditional in TX sample encoding per `mi0bot-Thetis networkproto1.c:1248-1256` `[mi0bot c26a8a4]`. Deferred to Phase 3M-2 if TX isn't there yet; otherwise include.

### HL2 I/O page contents (fills today's empty `Hl2IoBoardTab` placeholder)

- **Connection status bar** (top, green): board detected, hardware version (e.g. v3 N2ADR shield), I2C address, last-probe age
- **Configuration panel** (left): N2ADR Filter master enable + auto-switch description, Op Mode / Antenna / RX2 input / Antenna tuner status, Probe and Reset buttons
- **Register state table** (right): all 8 HL2 I/O registers (HardwareVersion, REG_CONTROL, REG_OP_MODE, REG_ANTENNA, REG_RF_INPUTS, REG_INPUT_PINS, REG_ANTENNA_TUNER, REG_FAULT) with addr / value / decoded meaning, polled @ 40 ms
- **12-step UpdateIOBoard state machine viz** (middle): visual progress through the ~480 ms cycle with current step highlighted
- **I2C transaction log** (bottom-left): timestamped read/write log with ack status; queue depth indicator
- **Bandwidth monitor mini** (bottom-right): EP6 ingress / EP2 egress meters, LAN PHY throttle status, dropped frame count, link out to full Bandwidth Monitor tab

NereusSDR spin: register state + I2C log are not exposed in mi0bot Thetis — pure NereusSDR diagnostic to help users troubleshoot HL2 I/O issues without Wireshark. Page auto-hides on non-HL2 boards.

### Files created

- `src/core/IoBoardHl2.{h,cpp}` (`mi0bot` variant, multi-source: console.cs + network.h)
- `src/core/HermesLiteBandwidthMonitor.{h,cpp}` (`mi0bot` variant)
- Files modified: `src/gui/setup/hardware/Hl2IoBoardTab.{h,cpp}` (replace placeholder with full content) and `src/gui/setup/hardware/BandwidthMonitorTab.{h,cpp}` (wire to real monitor)
- Tests.

### Definition of Done

1. HL2 I/O board version probed and logged at connect.
2. N2ADR filter selection propagates through I2C to the HL2 I/O board.
3. LAN PHY throttle detected and handled per the existing stubs.
4. HL2 I/O page renders register table, state machine viz, I2C log, bandwidth mini.
5. No regression on HL2 without the I/O board (probe fails gracefully).
6. Attribution DoD from §4.

---

## 11. Phase F — Accessory boards + Antenna Control

**Branch:** `phase3p-f-accessories`

### Scope

- `ApolloController` — ports `Console/setup.cs:15566-15590` `[ramdor 501e3f5]` + Apollo tuner loop; PA + ATU + LPF bytes.
- `AlexController` — ports `Console/HPSDR/Alex.cs:30-106` `[ramdor 501e3f5]`; per-band antenna arrays, RX-only antenna, Alex II RX2 routing. Replaces existing Alex stubs.
- `PennyLaneController` — ports `Console/HPSDR/Penny.cs` `[ramdor 501e3f5]` ext-ctrl enable + per-pin action mapping.
- `BoardCapabilities::{hasApollo, hasAlex, hasPennyLane}` per `Console/setup.cs:19834-20205` `[ramdor 501e3f5]` board rules (Apollo enabled on ANAN100/100B/100D/200D/7000D/8000D/OrionMKII/RedPitaya; disabled on HPSDR/Hermes/HermesII/Angelia/OrionMKI/G2 family).
- New Antenna / ALEX → Antenna Control sub-sub-tab.
- Setup dialog page gating — disable checkboxes / show "not available on this board" per capabilities.
- RxApplet antenna buttons populate from per-band Alex matrix; on band change, the active button auto-switches.

### Antenna Control page (third Antenna/ALEX sub-sub-tab)

Ports `Console/setup.designer.cs:grpAlexAntCtrl` (~lines 5981-7000) `[ramdor 501e3f5]`:
- **Safety row** at top: "Block TX on Ant 2" / "Block TX on Ant 3" — prevents transmit into a receive-loop antenna
- **TX Antenna grid** (left): 14 bands × 3 ports, radio buttons
- **RX1 / RX2 Antenna grid** (right): 14 bands × 3 ports, split into RX1 cluster + RX2/RX-only cluster

NereusSDR spin: 14 bands vs Thetis 12 (extra GEN + WWV rows defaulted to Ant 1).

### Files created

- `src/core/accessories/ApolloController.{h,cpp}`
- `src/core/accessories/AlexController.{h,cpp}` (replaces stubs)
- `src/core/accessories/PennyLaneController.{h,cpp}`
- `src/gui/setup/hardware/AntennaAlexAntennaControlTab.{h,cpp}` (port `grpAlexAntCtrl` `[ramdor 501e3f5]`)
- Setup pages for Apollo / Alex II / Penny gating.
- Tests.

### Files modified

- `src/gui/applets/RxApplet.{h,cpp}` — antenna buttons populate per-band from `AlexController::antennaFor(band, slot)`; on band change, active button updates.

### Definition of Done

1. Each accessory controller byte-identical to Thetis for its subsystem.
2. Antenna Control page renders, persists per-MAC, and drives `AlexController`.
3. RxApplet antenna buttons reflect per-band Alex assignment and update on band change.
4. Capability-gated UI for Apollo / Alex II / Penny.
5. Attribution DoD from §4.

---

## 12. Phase G — Calibration

**Branch:** `phase3p-g-calibration`

### Scope

Today NereusSDR has a "PA Calibration" sub-tab under Hardware that holds only the PA Current (A) calculation group. Phase G renames it to **Calibration** and adds the four other group boxes Thetis groups under General → Calibration, modeled 1:1.

Ports `Console/setup.designer.cs:tpGeneralCalibration` (~lines 11667-12263) and the underlying console.cs handlers `[ramdor 501e3f5]`.

### Calibration page contents (5 group boxes, 2×3 grid)

- **Freq Cal** (`grpGeneralCalibration`) — Frequency input + Start button + "Larger FFT sizes / lower sample rates give increased accuracy" hint
- **Level Cal** (`grpGenCalLevel`) — Frequency, Level (dBm), Rx1 6m LNA gain offset, Rx2 6m LNA gain offset, Start, Reset
- **HPSDR Freq Cal Diagnostic** (`grpHPSDRFreqCalDbg`) — Correction factor + Reset, separate 10 MHz factor + Reset, "Using external 10 MHz ref" toggle
- **TX Display Cal** (`grpBoxTXDisplayCal`) — single Offset (dB)
- **PA Current (A) calculation** (`groupBoxTS27`, **already exists today** as the sole PA Calibration tab content) — Sensitivity, Offset, Default, Log Volts/Amps to VALog.txt

### Wire-side wiring

- Frequency calibration writes the HPSDR correction factor — the long divisor used in Hz↔phase-word conversion. Update both connections (`P1RadioConnection`, `P2RadioConnection`) to use the configured factor.
- Level calibration is purely a display offset for now; per-band gain trim (a Thetis feature in setup.cs) is **out of scope** for Phase G — defer to a future "Per-Band Gain Trim" addendum.
- 10 MHz external reference toggle just sets a flag the user reads back — no firmware-side wire bits in OpenHPSDR P1/P2 (mi0bot HL2 firmware doesn't expose this).

### Files created

- `src/core/CalibrationController.{h,cpp}` — frequency correction factor, level cal state, 10 MHz ref flag, per-MAC persistence (port from `console.cs` calibration handlers `[ramdor 501e3f5]`, variant `thetis-samphire`)
- `tests/core/CalibrationControllerTest.cpp`

### Files modified

- `src/gui/setup/hardware/PaCalibrationTab.{h,cpp}` → rename to `CalibrationTab`, expand to host 5 group boxes
- `src/core/P1RadioConnection.cpp` / `src/core/P2RadioConnection.cpp` — apply frequency correction factor in Hz→phase-word
- `src/core/BoardCapabilities.{h,cpp}` — add `paCurrentSensitivity`, `paCurrentOffset` defaults per board

### Definition of Done

1. Calibration page renders all 5 group boxes; layout matches Thetis 2×3 grid.
2. Frequency cal writes the correction factor and the radio's reported frequency moves accordingly during calibration.
3. Level cal applies the dBm offset to S-meter and spectrum readings.
4. 10 MHz ext ref toggle persists per-MAC; UI shows the alternate correction factor when enabled.
5. PA Current calc behaviour preserved byte-for-byte vs pre-rename.
6. Per-MAC persistence under `hardware/<mac>/cal/{freqFactor,freqFactor10M,using10M,levelOffset,rx1_6mLna,rx2_6mLna,txDisplayOffset,paSens,paOffset}`.
7. Attribution DoD from §4.

---

## 13. Phase H — Status feedback & settings hygiene

**Branch:** `phase3p-h-status-hygiene`

### Scope

NereusSDR-native consolidation of status feedback into a Diagnostics top-level tab (Diagnostics already exists). Thetis surfaces these readouts piecemeal across Front Console, PA Settings, and main meter — Phase H pulls them into one dashboard. Also lights up the live LED indicators built into earlier phases (Alex-2 active filters, OC pin state) and adds settings-mismatch warnings when the persisted state disagrees with the connected board's capabilities.

### Diagnostics → Radio Status page

- **Top status bar:** connected radio + MAC, uptime, firmware version, current mode
- **PA Status card:** Temperature, Current, Voltage with bar meters and per-board limits (`BoardCapabilities::paTempLimit`, `paCurrentLimit`)
- **Forward / Reflected Power + SWR card:** bar meters; idle until TX, last-TX peak in scale labels
- **PTT Source card:** pill row of all sources (MOX/VOX/CAT/Mic/CW/Tune/2-Tone) with active highlighted; 8-event PTT history log
- **Connection quality card:** RTT latency, EP6 sequence gaps, watchdog resets, LAN PHY throttle, ingress/egress rates (links to full Connection Quality sub-tab)
- **Settings hygiene card:** live mismatch warnings (e.g., "S-ATT setting clamped — persisted 45 dB exceeds this radio's 0–31 dB range"); Reset settings to defaults for this board; Forget this radio

### Diagnostics sub-tabs (placeholder pages added in Phase H, full content per sub-phase)

| Sub-tab | Content | Phase |
|---|---|---|
| Radio Status | Above dashboard | H (this phase) |
| Connection Quality | 60 s history graph of latency / sequence gaps / throttle events | H |
| Settings Validation | Full audit list of mismatches + per-row "fix it" actions | H |
| Export / Import Config | Per-MAC + global AppSettings export/import (XML) | H |
| Logs | Recent qCWarning / qCDebug log viewer | H |

### Live LED indicators wired up across earlier phase pages

- **Alex-2 Filters page** (Phase B) — HPF and LPF LED rows reflect live filter selection from ep6 status
- **OC Outputs page** (Phase D) — Live pin state LED row reflects last C&C OC byte sent
- **HL2 I/O page** (Phase E) — register state table polls @ 40 ms; bandwidth meter live

### Wire-side parsing

- Status packet bytes parsed in `P1RadioConnection::parseEp6Frame` / `P2RadioConnection::parseStatusPacket`. PA temp / current / fwd-rev power live in well-defined offsets; per-MAC `paTempLimit` / `paCurrentLimit` from BoardCapabilities populates the meter scales.
- PTT-source tracking is internal: NereusSDR sets a `PttSource` enum on each MOX assert based on the trigger (UI button → MOX, CAT command → CAT, mic ring → MicPtt, firmware keyer → CW, etc.).
- Settings hygiene runs a `validate()` pass on every connect: walks `AppSettings` for the connected MAC, compares to `BoardCapabilities`, emits mismatch records.

### Files created

- `src/core/RadioStatus.{h,cpp}` — PA temp/current/SWR/PTT-source aggregator (variant `multi-source`, parses status packet bytes referenced from `console.cs` status handlers `[ramdor 501e3f5]`)
- `src/core/SettingsHygiene.{h,cpp}` — NereusSDR-native, no port; validates AppSettings against BoardCapabilities
- `src/core/PttSource.h` — enum + tracking
- `src/gui/diagnostics/RadioStatusPage.{h,cpp}`
- `src/gui/diagnostics/ConnectionQualityPage.{h,cpp}`
- `src/gui/diagnostics/SettingsValidationPage.{h,cpp}`
- `src/gui/diagnostics/ExportImportConfigPage.{h,cpp}`
- `src/gui/diagnostics/LogsPage.{h,cpp}`
- Tests for each.

### Files modified

- `src/core/P1RadioConnection.cpp` / `src/core/P2RadioConnection.cpp` — parse PA temp/current/SWR/forward/reflected from status bytes; emit signals to `RadioStatus`
- `src/gui/applets/RxApplet.cpp` — ADC OVL split (already in Phase B; H wires per-ADC color coding)
- `src/gui/setup/hardware/Hl2IoBoardTab.cpp` — bandwidth monitor live data
- `src/gui/setup/hardware/AntennaAlexAlex2Tab.cpp` — LED row live data
- `src/gui/setup/hardware/OcOutputsHfTab.cpp` — live pin state LED row live data

### Definition of Done

1. Diagnostics → Radio Status page renders all 5 cards.
2. PA temp / current / fwd / refl / SWR populate during TX; idle shows last-TX peaks.
3. PTT source pills light correctly per trigger.
4. Settings hygiene catches per-MAC mismatches on connect (S-ATT over-range, missing per-band Alex assignment, etc.) and offers fix actions.
5. Live LED indicators wired across Alex-2 Filters, OC Outputs, and HL2 I/O pages.
6. Connection Quality 60 s history graph renders.
7. Export / Import config round-trips per-MAC settings.
8. Attribution DoD from §4.

---

## 14. Cross-cutting concerns

### BoardCapabilities expansion

`BoardCapabilities` grows from ~15 fields today to roughly 35 across all eight phases. Every addition is populated per board in `BoardCapabilities.cpp`; codec subclasses and UI code read from the struct, not hardcoded literals. **New fields default to current behavior** so non-default values don't regress existing boards.

### Settings persistence

All new per-board / per-band state goes through `AppSettings` under `hardware/<mac>/...` keys, consistent with the radio-authoritative rule in `CLAUDE.md`:

- Phase A: (nothing new — all state already persisted).
- Phase B: Saturn BPF1 edges, per-ADC preamp, dither/random toggles, Alex HPF bypass per band.
- Phase C: per-band preamp mode (already in `StepAttenuatorController`).
- Phase D: OC matrix (14 bands × 7 pins × 2 modes), TX pin actions, USB BCD config.
- Phase E: N2ADR filter present + version, op mode, antenna, RX2 input, tuner status.
- Phase F: per-band Alex antenna (TX/RX1/RX2), Block-TX safety, Apollo / Alex II / Penny settings.
- Phase G: frequency correction factor (default + 10 MHz variant), level offset, 6m LNA offsets, TX display offset, PA current sensitivity / offset.
- Phase H: PTT source default override, settings-hygiene acknowledgement state, export/import history.

### Test infrastructure

Each phase lands its tests under `tests/core/codec/`, `tests/core/accessories/`, `tests/core/diagnostics/`, etc. CI runs all of them per PR. The regression-freeze baseline from Phase A is a JSON file in `tests/core/codec/data/` that every subsequent phase keeps updated when (intentionally) changing byte output.

### PR review template

Phase A also lands a `.github/pull_request_template.md` update adding the attribution checklist from §4. Every subsequent radio-control PR uses it.

### Userland-parity rule

For Setup-page IA, page density, and group-box layout, default to matching Thetis 1:1 (codified as `feedback_thetis_userland_parity` in maintainer memory). NereusSDR-native spin allowed where it serves the user — flagged explicitly in each phase's "Spin from Thetis" notes (color-coded RX/TX OC matrix grids, live LED rows on Alex-2 / OC pages, register state diagnostic on HL2 I/O page, consolidated Diagnostics dashboard vs Thetis's piecemeal status surfaces).

### Public API invariants (don't-break-this list)

- `P1RadioConnection::setAttenuator(int)`, `setReceiverFrequency(int, quint64)`, `setTxFrequency(quint64)` signatures unchanged.
- `StepAttenuatorController` public surface unchanged.
- `RxApplet` slot-signal wiring to `RadioModel` unchanged.
- `BoardCapabilities` field defaults reproduce today's bytes for every board that already works.

---

## 15. Risks & migration

| Risk | Mitigation |
|---|---|
| Phase A accidentally regresses Hermes/Orion (vast majority of installed base) | Regression-freeze byte tables capture current output before the refactor; refactor must produce byte-identical output for non-HL2 boards |
| PR #34 in-flight collides with Phase A step-att byte changes | Land PR #34 first; Phase A rebases onto `main` and extends `StepAttenuatorController` with HL2 branching. Merge order explicit in PR bodies |
| Other in-flight PRs (#53 Auto AGC-T, 3G-10 slice features) | Phase A doesn't touch their code paths. Rebase order: PR #34 → PR #53 → Phase A. Document in each PR body |
| mi0bot has silent non-HL2 drift from ramdor | Every divergence block cites both file:line pairs; reviewer cross-checks "matches ramdor" claims |
| Windows-only regressions (endianness, bit order) not caught on macOS dev box | Byte tables are pure-data assertions and run cross-platform; CI matrix already builds Linux + macOS + Windows |
| HL2 hardware regression possible since only JJ has HL2 | Phase A ships to alpha first; beta feedback required before stable release. Document affected boards explicitly in release notes |
| Attribution regression (PR merges code without PROVENANCE row) | `check-new-ports.py` in pre-push hook and CI. Reviewer checklist in PR template |
| Codec subclass proliferation hurts readability | Each subclass file is small (one board's overrides); tests for each live next to the codec. Subclass hierarchy stays flat — no grandchild overrides |
| P2 audit (Phase B) turns up more gaps than expected | Spec's audit already surfaced Saturn edges, per-ADC preamp, dither/random, TX ATT. If more turn up, scope grows within B — don't silently drift. Document in B's plan doc |
| Phase F antenna auto-switching surprises users mid-QSO | Per-band Alex matrix defaults reproduce today's static behavior (Ant 1 for unset bands). Auto-switch only kicks in once user has explicitly configured per-band assignments |
| Phase G scope creep into TX power calibration | TX power calibration belongs to Phase 3M; Phase G covers freq + level + display + PA current only |
| Phase H consumes status packet bytes 3M will also consume | `RadioStatus` aggregator owns the parse; 3M consumers attach as signal listeners, no duplicated parsing |
| BoardCapabilities field defaults break a working board | Every new field has an explicit "default reproduces today's bytes" assertion in `BoardCapabilities.cpp`; covered by regression-freeze tests |

### Rollback strategy

Phase A ships behind an env-var feature flag (`NEREUS_USE_LEGACY_P1_CODEC=1`) for one release cycle. If a regression is reported on hardware we don't have, users can revert to the pre-refactor compose path without rolling back the binary. Flag removed in the release after Phase B merges.

---

## 16. Acceptance criteria — full parity (spec-wide)

- Every entry in `HPSDRModel` enum has a codec subclass (P1) or codec mapping (P2).
- Every ported line carries stamp(s) in the form `[ramdor <sha>]` and/or `[mi0bot <sha>]`.
- `THETIS-PROVENANCE.md` lists every ported file with upstream source file(s), line range(s), and variant.
- `verify-thetis-headers.py` and `check-new-ports.py` pass for every PR in the phase range.
- Hand-derived byte tables cover every codec × every bank × MOX on/off for P1; equivalent command-packet coverage for P2.
- Both reported bugs (HL2 BPF, HL2 S-ATT) regression-tested.
- Non-HL2 boards byte-identical to pre-refactor `main`.
- `CHANGELOG.md` entries per phase.
- Attribution CI gates green on every PR.
- Setup pages (Hardware → Antenna/ALEX → 3 sub-sub-tabs, OC Outputs → HF/SWL, HL2 I/O, Calibration) match Thetis 1:1 within the userland-parity rule (§5).
- Diagnostics → Radio Status renders all 5 live cards; Settings Validation catches per-MAC mismatches on board switch.
- After Phase H merges, NereusSDR's hardware/radio-control surface is **userland-complete** vs Thetis: every Setup page a Thetis user reaches for has a NereusSDR equivalent, every status readout a Thetis user expects is exposed.

---

## 17. Reference index

### Upstream source maps (at spec-pin versions)

**ramdor / Thetis `[ramdor 501e3f5]`** — `/Users/j.j.boyd/Thetis/`
- `Project Files/Source/ChannelMaster/networkproto1.c` — P1 WriteMainLoop, banks 0–17
- `Project Files/Source/ChannelMaster/network.c` — P2 command packets
- `Project Files/Source/ChannelMaster/network.h` — HPSDRModel enum, I2C TLV struct
- `Project Files/Source/Console/console.cs` — `setAlexHPF/LPF`, `SetComboPreampForHPSDR`, OC callers, `UpdateIOBoard`, calibration handlers
- `Project Files/Source/Console/setup.cs` — Setup form behavior
- `Project Files/Source/Console/setup.designer.cs` — tab tree, group box layout
- `Project Files/Source/Console/HPSDR/Penny.cs` — Penny bitmasks
- `Project Files/Source/Console/HPSDR/Alex.cs` — Alex antenna arrays
- `Project Files/Source/Console/HPSDR/NetworkIO.cs` — P/Invoke to ChannelMaster

**mi0bot / Thetis-HL2 `[mi0bot c26a8a4]`** — `/Users/j.j.boyd/mi0bot-Thetis/` (v2.10.3.13-beta2)
- `Project Files/Source/ChannelMaster/networkproto1.c` — adds `WriteMainLoop_HL2` (869–1201), `MetisReadThreadMainLoop_HL2` (422–585), I2C intercept (898–943), CW keyer bit 3 (1248–1256)
- `Project Files/Source/ChannelMaster/bandwidth_monitor.{c,h}` — MW0LGE HL2 bandwidth monitor
- `Project Files/Source/Console/console.cs:25781-25945` — `UpdateIOBoard` state machine
- `Project Files/Source/Console/setup.cs:20234-20238` — `chkHERCULES` N2ADR filter toggle

### NereusSDR anchors

- `src/core/HpsdrModel.h:94-113` — `HPSDRModel` enum (16 board types)
- `src/core/HpsdrModel.h:117-129` — `HPSDRHW` hardware enum
- `src/core/BoardCapabilities.{h,cpp}` — per-board capability table
- `src/core/P1RadioConnection.{h,cpp}` — today's single-function compose
- `src/core/P2RadioConnection.{h,cpp}` — P2 with already-ported Alex helpers
- `src/core/StepAttenuatorController.{h,cpp}` — 3G-13 controller (PR #34 in flight)
- `src/gui/setup/HardwarePage.cpp:109-117` — existing 9-tab Hardware shell that all UI work plugs into
- `src/gui/applets/RxApplet.{h,cpp}` — per-receiver UI strip touched by Phases A/B/C/F
- `docs/attribution/HOW-TO-PORT.md` — template / stamp grammar
- `docs/attribution/THETIS-PROVENANCE.md` — grep-able inventory
- `scripts/verify-thetis-headers.py` / `scripts/check-new-ports.py` — CI gates

### Audit artifacts (this session)

The byte-level divergence catalogs and UI mockup series produced during design — P1 (banks 0–18 × 11 board types × MOX on/off), P2 (Saturn vs OrionMKII vs AnvelinaPro3), preamp combo + OC matrix + HL2 IoBoard + accessories audit, and 9 page mockups (sitemap v2 corrected, Calibration, Antenna Control, Alex-1 Filters, Alex-2 Filters, OC Outputs, HL2 I/O, RxApplet evolution, Radio Status) — are the research backbone for this spec. The Phase A catalog is reproduced in §6.3; the rest are referenced inline in each phase section and will be reproduced in each phase's implementation plan when that phase's PR opens. Mockups live ephemerally under `.superpowers/brainstorm/` (gitignored).

---

*End of spec.*
