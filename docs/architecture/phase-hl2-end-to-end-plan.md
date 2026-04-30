# Phase HL2 End-to-End — Implementation Handoff Plan

**Status:** Plan; not yet started. Drafted from hardware-bring-up findings 2026-04-29.
**Date:** 2026-04-29.
**Branch base:** `test/hl2-hardware-2026-04-29` (currently main + PR #151 + PR #152)
**Hardware:** Hermes Lite 2 + N2ADR filter board + HL2 I/O board (first hardware contact for KG4VCF)
**Source-first refs:**
- mi0bot-Thetis (HL2-specific patches) — `/Users/j.j.boyd/mi0bot-Thetis` `[v2.10.3.13-beta2]` / `[@c26a8a4]`
- Upstream Thetis (general logic) — `/Users/j.j.boyd/Thetis` `[v2.10.3.13]` / `[@501e3f51]`
- Existing analysis: [`phase3m-1c-hl2-tx-path-review.md`](phase3m-1c-hl2-tx-path-review.md) — mi0bot↔Thetis HL2 TX desk-review (lean on this heavily)

---

## 1. Why this exists

KG4VCF received HL2 hardware on 2026-04-29 and during bring-up observed:

| Observed symptom | True root cause (verified in code) |
|---|---|
| I/O Setup → Init/Probe button does nothing on hardware | `Hl2IoBoardTab::onProbeClicked()` is a deliberate no-op (timestamp only, `Hl2IoBoardTab.cpp:832`); the actual driver `P1RadioConnection::hl2SendIoBoardInit()` is a `TODO(3I-T12)` stub at `P1RadioConnection.cpp:2434` |
| N2ADR filter board: no relay clicks on band change → audible noise | The N2ADR enable checkbox writes `hl2/n2adrFilter` to AppSettings (`Hl2IoBoardTab.cpp:828`) but **no consumer reads it**. No I2C write fires on band change. |
| Step attenuator: not working correctly | RX-side wire fix landed in PR #85 (`P1CodecHl2.cpp:53`). TX-side P1 attenuator wire path **does not exist**. PR #85 explicitly noted "awaiting HL2 hardware smoke." |
| TX: nothing at all | TX I/Q pump landed in PR #152 (`TxWorkerThread`/MOX/SPSC ring) but `P1RadioConnection::setTxDrive()` is `/* stub — Task 7 */` at `P1RadioConnection.cpp:814` and the C1 drive byte is hardcoded `0` in the C&C composer (`P1RadioConnection.cpp:519-520`). Radio is told "TX with 0% drive" → no RF. |

Conclusion: these are not bugs in user-land config or HL2 hardware. They are **unimplemented features** that this plan tracks to completion.

---

## 2. Inventory — what's missing (file:line evidence)

### 2.1 I/O board / Phase 3L (I2C-over-ep2 wire encoding)

| Location | Evidence | Gap |
|---|---|---|
| `src/core/P1RadioConnection.cpp:2434` | `void P1RadioConnection::hl2SendIoBoardInit()` body is one `qCInfo(...)` log + `return` | Probe driver is a stub |
| `src/core/P1RadioConnection.cpp:2428` | `// TODO(3I-T12): When Phase 3L lands, locate the I2C-over-ep2 wire encoding` | Wire format not ported |
| `src/core/P1RadioConnection.cpp:2444` | `// TODO(3I-T12): Port ChannelMaster I2C-over-ep2 framing for full init.` | Same |
| `src/core/P1RadioConnection.cpp:2465` | `// TODO(3I-T12): Port the full byte-rate monitor` | Bandwidth monitor uses ep6 sequence-gap proxy, not the upstream `compute_bps()` algorithm |
| `src/core/P1RadioConnection.h:221` | `// Full I2C init sequence is deferred (TODO(3I-T12))` | Same |
| `src/core/IoBoardHl2.cpp:23` | `// Closes Phase 3I-T12 deferred work` (PR #95 closed UI scaffold; **wire encoding still deferred**) | Memory-line "PR #95 closes 3I-T12" is misleading — only the UI half closed |

Upstream reference: mi0bot `Console/HPSDR/IoBoardHl2.cs:129-145` (`IOBoard.readRequest()` initiates I2C reads), `NetworkIO.I2CReadInitiate(bus=1, addr, reg)`, ChannelMaster.dll `network.c` (the actual ep2 framing). NereusSDR has IoBoardHl2 register mirror + queue + state machine + `applyI2cReadResponse()` parser already; only the **outbound wire framing** is missing.

### 2.2 N2ADR filter board (the user-audible "no clicks" symptom)

| Location | Evidence | Gap |
|---|---|---|
| `src/gui/setup/hardware/Hl2IoBoardTab.cpp:300` | `m_n2adrFilter = new QCheckBox(tr("Enable N2ADR Filter board"), ...)` | UI exists |
| `src/gui/setup/hardware/Hl2IoBoardTab.cpp:828` | `AppSettings::instance().setValue("hl2/n2adrFilter", ...)` | Setting persisted |
| `src/core/SettingsHygiene.cpp:163-180` | "N2ADR filter is an HL2-only hardware option" — hygiene check rejects N2ADR=true on non-HL2 | Validation present |
| **(none)** | grep found **no** consumer of `"hl2/n2adrFilter"` outside the writer + hygiene check | **Consumer entirely absent** — no I2C write, no OC byte mutation, no band-change reaction |
| `src/core/BoardCapabilities.cpp:591` | `// HL2 uses IoBoardHl2 for I2C accessory control instead.` (HL2 has no OC pins) | N2ADR on HL2 is **I2C-driven**, not OC-driven; depends on §2.1 wire framing landing |

Note: this is intertwined with Phase 3L. N2ADR cannot click without I2C writes; I2C writes need the wire encoding. **Phase 3L unblocks both I/O board detection AND N2ADR.**

### 2.3 Step attenuator (TX side)

| Location | Evidence | Gap |
|---|---|---|
| `src/core/codec/P1CodecHl2.cpp:6-18` | PR #85 header — "Fixes reported HL2 S-ATT bug at the wire layer" | RX wire fix shipped |
| `src/core/codec/P1CodecHl2.cpp:53` | `// This is the load-bearing bug fix for the reported S-ATT issue.` | RX uses 6-bit mask + `0x40` enable + TX/RX mux branch |
| `src/core/P1RadioConnection.cpp:2274` | `out[4] = static_cast<quint8>((m_stepAttn[0] & 0x1F) \| 0x20);` | RX path complete (ADC0 step ATT + enable) |
| `src/core/P2RadioConnection.cpp:1151` | `// Mirrors Thetis ChannelMaster/netInterface.c:1006 SetTxAttenData` | P2 TX atten exists |
| **(none)** | No P1 TX atten setter found | **P1 TX atten path absent** — likely Thetis-style "SetTxAttenData" never wired to P1CodecHl2 |
| `docs/MASTER-PLAN.md:873` | `**Note:** HL2 ATT logic may need cross-checking against mi0bot/Thetis-HL2 fork` | Cross-check still TBD |

### 2.4 TX path

| Location | Evidence | Gap |
|---|---|---|
| `src/core/P1RadioConnection.cpp:814` | `void P1RadioConnection::setTxDrive(int /*level*/)  { /* stub — Task 7 */ }` | **TX drive setter is a stub — single biggest TX blocker** |
| `src/core/P1RadioConnection.cpp:519-520` | `out[1] = 0;  // drive level — TODO(3I-T7): wire from state` | C1 byte hardcoded zero in C&C compose |
| `src/core/P1RadioConnection.cpp:1496` | `ctx.txDrive = m_txDrive;` | Cached in CodecContext but never read by composer |
| `src/core/P1RadioConnection.cpp:487-520` | `// TODO(3I-T7): The "ALEX RX antenna routing" + drive level + mic/apollo flags` | 3-part deferral: ALEX RX routing + TX drive + mic/apollo flags |
| `src/core/codec/P1CodecHl2.cpp` | No TX drive byte in `compose()` | HL2 codec composer never emits drive |
| `src/core/TxChannel.cpp:545` | `// NereusSDR activates cfir unconditionally in 3M-1a; P1/P2 gating is deferred` | CFIR always-on; P1/P2 board-specific gating TBD |
| `src/core/TxMicRouter.h:38-39` | `// PC/Radio mic source implementations deferred to 3M-1b` (still using NullMicSource) | Mic source = silent zero-pad |
| `src/core/MoxController.h:92,110` | VOX/CW/TCI PTT modes rejected (deferred to 3M-3a) | Manual PTT only |
| `src/core/TxChannel.cpp:940-951` | `// In 3M-1b: AM/SAM TX is gated off by BandPlanGuard.` | AM/SAM TX rejected; verify HL2 mode coverage |
| `src/core/TwoToneController.cpp:172` | `// TODO(3M-1c-polish): expose TxChannel TUN-active state` | Two-tone TUN auto-stop is a stub |

For the **detailed mi0bot↔Thetis HL2 TX comparison** (HL2 EP2 zone timing, mic-jack handling, drive scaling, antenna mux, AM/SAM coverage, PA enable byte, CFIR gate, exciter on/off), see [`phase3m-1c-hl2-tx-path-review.md`](phase3m-1c-hl2-tx-path-review.md). That doc is the source-first authority for HL2 TX design decisions.

### 2.5 Other gaps (broad sweep)

| Location | Evidence | Gap |
|---|---|---|
| `src/core/AudioEngine.h:361` | `// on the TX pull side lives in Phase 3M` | VAX TX audio routing deferred to 3O |
| `src/core/TxChannel.h:551` | `/// **Deferred to 3M-3b.** Throws std::logic_error if called in 3M-1b` | AM/SAM dispatch deferred |

---

## 3. Phasing

The total scope spans Phase 3L + remaining 3M-1c + bits of 3M-2/3M-3 — multiple sessions. Recommend tackling in this order:

### Phase A — Minimum-viable HL2 TX + N2ADR audible (highest ROI per session)

| Step | File:line | Action | Verifies |
|---|---|---|---|
| A1 | `P1RadioConnection.cpp:814` | Implement `setTxDrive(int level)` — store to `m_txDrive`, update CodecContext | (no observable effect alone) |
| A2 | `P1RadioConnection.cpp:519-520` | Wire `m_txDrive` → C1 byte in C&C compose; respect mi0bot HL2 drive scaling per `phase3m-1c-hl2-tx-path-review.md` §2.5 | TX power > 0 W into dummy load |
| A3 | `P1CodecHl2.cpp::compose()` | Confirm HL2 codec emits drive byte (or refactor compose to read from CodecContext) | RF on TX |
| A4 | New consumer slot | Read `hl2/n2adrFilter` AppSettings; on `bandChanged` from `PanadapterModel`, emit I2C write to N2ADR relay select register | N2ADR clicks per band |

**Phase 3L wire-encoding is a prerequisite for A4.** Two sub-options:

- **Option A4-fast:** if mi0bot exposes a minimal "I2C write only" path that's smaller than the full ChannelMaster.dll port, port that subset for the N2ADR write and keep `hl2SendIoBoardInit()` deferred. Risk: partial port creates a maintenance pothole.
- **Option A4-full:** do Phase B before A4 so I2C is fully wired. Cleaner, takes 1 extra session.

**Recommendation:** Option A4-full. The desk-review for Phase 3L isn't huge; doing it once cleanly avoids carrying two I2C code paths.

### Phase B — I/O board detection + Phase 3L wire encoding

| Step | File:line | Action |
|---|---|---|
| B1 | mi0bot `ChannelMaster/network.c` | Source-first read: locate I2C-over-ep2 wire framing |
| B2 | New file `src/core/HL2I2CWire.{h,cpp}` (or extend `P1RadioConnection`) | Port the wire framing as a self-contained encoder |
| B3 | `P1RadioConnection.cpp:2434` | Replace `hl2SendIoBoardInit()` stub with real probe sequence (3 reads: 0x41/0, 0x1d/9, 0x1d/10) |
| B4 | Hl2IoBoardTab integration | Verify `applyI2cReadResponse()` is being called by the codec on response frames; `IoBoardHl2::isDetected()` should flip `true` |
| B5 | (optional) `hl2CheckBandwidthMonitor()` line 2470 | Replace ep6-gap proxy with mi0bot's `compute_bps()` |
| B6 | UX | Re-evaluate `Hl2IoBoardTab::onProbeClicked()` — when probing actually does something, the button should trigger a re-probe (not just log a marker) |

**Verification:** Setup → Hardware → HL2 I/O Board tab — "Last probe" updates with hardware version + firmware major/minor, board flips to detected.

### Phase C — Step ATT TX + 3M-1c polish

| Step | File:line | Action |
|---|---|---|
| C1 | new — parallel to `P1RadioConnection.cpp:2274` | P1 TX step-att path (mirror Thetis `SetTxAttenData`) |
| C2 | mi0bot cross-check | Verify HL2 RX inversion `(31 − N)` matches PR #85's port (mi0bot `console.cs:11075/11251/19380`) |
| C3 | `TwoToneController.cpp:172` | Wire TUN auto-stop to TxChannel TUN-active state |
| C4 | `TxMicRouter` | Replace NullMicSource with real PC mic / radio mic-jack readers (Phase 3M-1b leftover) |

### Phase D — Future, out of scope for this handoff

3M-2 (CW), 3M-3 (TX processing 18-stage), 3M-4 (PureSignal), 3M-3b (AM/SAM TX), 3O VAX audio TX side. Track separately.

---

## 4. Source-first protocol (non-negotiable per CLAUDE.md)

For each unit of work in Phases A/B/C:

1. **READ** the mi0bot-Thetis source first (HL2-specific) — fall back to upstream Thetis for non-HL2 logic. Stamp the version: `[v2.10.3.13-beta2]` for tagged values, `[@c26a8a4]` for post-tag. Run `git -C /Users/j.j.boyd/mi0bot-Thetis describe --tags` and `git -C /Users/j.j.boyd/Thetis describe --tags` once at session start.
2. **SHOW** the original C# / C before writing C++ — `"Porting from [file]:[lines] — original logic:"` then quote.
3. **TRANSLATE** to C++20/Qt6 preserving constants, magic numbers, and inline attribution tags verbatim. `// From Thetis [file]:[line] [stamp]` on every ported line.
4. **VERIFY** with the inline-cite hooks (`scripts/verify-inline-cites.py`) before committing.

## 5. Hardware verification rubric

After each phase, run on hardware:

| Phase A passes if | Phase B passes if | Phase C passes if |
|---|---|---|
| TX into dummy load shows non-zero forward power on radio meter | Setup → HL2 I/O Board tab shows "Detected: Yes" + hardware version + firmware revision | RX step-ATT clicks at every dB step audible; TX into dummy load with TX-ATT engaged shows reduced fwd power matching set value |
| Band change → N2ADR relay click audible from filter board | "Last probe" timestamp updates on connect | PR #85 RX inversion confirmed against mi0bot |
| N2ADR enabled vs disabled: noise floor noticeably different on RX | Bandwidth monitor doesn't false-throttle on long rag-chew | Two-tone test auto-stops when TX is dropped |

Capture wire data with `tcpdump -i <iface> -w hl2-bringup.pcap udp port 1024 or udp port 1025` during each verification — diff against captures from Thetis on the same hardware to catch wire-byte divergence.

## 6. Constraints

- **GPG signing.** All commits via key `20C284473F97D2B3`. Never use `--no-verify`.
- **Inline-cite stamps** on every Thetis-ported line.
- **One PR per phase** (A/B/C). Each PR: source-first pre-review doc, code, hardware verification log, post-code review.
- **No design speculation in code.** If the answer isn't in mi0bot or upstream Thetis, ASK before guessing.
- **Plans go in `docs/architecture/`.** This file lives there. Per-phase pre/post review docs likewise (mirror `phase3m-1c-*` naming).

## 7. Tracking

- Open a GitHub Issue per phase (A/B/C) — title: "Phase HL2-A: TX drive + N2ADR" etc. Link this plan.
- Verification matrix: extend the existing `docs/architecture/phase3m-1c-verification/` matrix with HL2 hardware rows.
- This plan supersedes the loose memory note "PR #95 closes Phase 3I-T12" — that memory should be updated to "PR #95 closed the I/O-board UI scaffold; wire encoding deferred to Phase 3L."

---

## 8. Out-of-scope

- Windows firewall DDC UDP issue (separate, environmental — captured in memory `project_nereussdr_cross_platform_status.md`).
- Phase 3M-2/3/4 (CW, TX processing, PureSignal) — handoff later.
- Skin / TCI / CAT — orthogonal to HL2 hardware.

---

**Author:** J.J. Boyd ~ KG4VCF, with AI tooling.
