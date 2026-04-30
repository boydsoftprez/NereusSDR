# Phase 3L — HL2 Visibility & Options Surface (Design)

**Status:** Approved 2026-04-30 (visual brainstorm). Implementation plan to follow.
**Branch base:** `test/hl2-hardware-2026-04-29` (currently 2 commits behind `origin/main`).
**Companion plan:** [`phase-hl2-end-to-end-plan.md`](phase-hl2-end-to-end-plan.md) — wire encoding / I2C / N2ADR consumer (Phases A–C). This doc is the **UI-visibility layer** that sits on top.
**Source-first refs:**
- mi0bot-Thetis — `/Users/j.j.boyd/mi0bot-Thetis` `[v2.10.3.13-beta2]` / `[@c26a8a4]`
- Verified files this session: `setup.designer.cs`, `setup.cs`, `console.cs`, `Hermes-Lite2/gateware/rtl/i2c_bus2.v`

---

## 1. Why this exists (and why it's separate from the end-to-end plan)

During hardware bring-up KG4VCF observed three symptoms that the existing
end-to-end plan does **not** address — they are visibility/UX gaps, not
wire-encoding gaps:

| Symptom | Cause | Where it belongs |
|---|---|---|
| "I hear band relay clicks but see no indication of which pin or bank fired" | Today's UI shows the OC byte only on the I/O Board page; no live indicator | New `HL2 Options` tab + promoted live LED strip widget |
| "There's no place to map LPF/HPF or see bank state like in Thetis" | mi0bot has a dedicated `tpHL2Options` page with three group boxes; we don't | Three new groups under a single new Setup tab |
| "ATT goes negative on HL2 in mi0bot — does ours?" | NereusSDR caps the HL2 step attenuator at `[0, 63]` raw; mi0bot exposes signed `−28..+32` dB user-facing with wire conversion `wire = 31 − userDb` (mi0bot `console.cs:11074-11075`, `setup.cs:16085-16086`) | Codec caps + UI mapping fix |

These are independent of the wire-encoding work but want to ship together
because they're all "make HL2 feel like mi0bot does."

---

## 2. Source-first survey results (2026-04-30 session)

Three findings drove the final design and are recorded here so a future
reader doesn't have to re-derive them:

### 2.1 mi0bot tab layout when HL2 is connected

`setup.cs:20220-20253` (`HardwareSpecific` switch, HPSDRModel.HERMESLITE
case):

- `tpPennyCtrl.Text = "Hermes Lite Control"` — relabel of existing OC matrix tab
- `tpAlexControl.Text = "Ant/Filters"` — relabel of existing antenna routing tab
- `tpHL2Options` becomes visible — **new tab** with three group boxes
- `chkHERCULES.Visible = true; chkHERCULES.Text = "N2ADR Filter"` — master toggle
- `ucIOPinsLedStripHF.DisplayBits = 6` — 6-bit input strip (HL2 has 6 input pins, vs Hermes's 8)

We mirror these three relabels/additions directly. Existing NereusSDR tabs
are kept where they are; only their **labels** change when an HL2 is
connected.

### 2.2 What's actually on `tpHL2Options` (`setup.designer.cs:11075-11135`)

Three group boxes, in this order:

**`groupBoxHL2RXOptions`** (text: "Hermes Lite Options") — 8 HL2-specific
radio behavior knobs:
- `chkSwapAudioChannels`
- `chkCl2Enable` + `udCl2Freq` + `labelCl2Freq` (CL2 clock-out enable + frequency MHz)
- `chkExt10MHz` (external 10 MHz reference)
- `chkDisconnectReset` (force disconnect/reset on next start)
- `udPTTHang` + `labelPttHang` (ms)
- `udTxBufferLat` + `labelTxLatency` (ms)
- `chkHL2PsSync` (PureSignal sync)
- `chkHL2BandVolts` (band voltage output 0–3.3 V on RJ45 — useful for amp keying)

**`groupBoxI2CControl`** — manual I2C R/W tool: `chkI2CEnable`, bus combo,
addr combo, register combo, data combo, Read/Write buttons, result label.

**`grpIOPinState`** — three LED strips (`ucOCLedStrip` user-control class):
- `ucOutPinsLedStripHF` — HL2 output pins (8 LEDs, 1 byte at I2C 0x20)
- `ucIOPinsLedStripHF` — HL2 input pins (`DisplayBits = 6`)
- (we also surface today's OC byte LED indicator as a third strip per page that benefits from it)

**Critically: `chkAutoATTRx1` / `chkAutoATTRx2` are NOT on this tab.**
They live in `groupBoxTS47` ("Auto Attenuate RX") on `tpOptions1` (the
general Options page), at `setup.designer.cs:8895-8949`. Our PR #53 Auto
AGC-T + PR #34 step-attenuator pages already cover the equivalent
NereusSDR surface — no duplication needed.

### 2.3 HL2 attenuator range bug (sign-aware)

mi0bot exposes a **signed** −28..+32 dB user-facing range and converts to
the wire format with `wire = 31 − userDb`:

| Reference | What it shows |
|---|---|
| `console.cs:11074-11075` | `NetworkIO.SetADC1StepAttenData(31 − _rx1_attenuator_data)` |
| `setup.cs:1087-1088, 16085-16086` | `udHermesStepAttenuatorData.Maximum = 32; .Minimum = −28` |
| `setup.cs:11041-11045` | `chkRX1StepAtt` enable gating per HPSDR model |

NereusSDR's current state:
- `BoardCapabilities.cpp:591`-area HL2 caps: `{0, 63, 1, true, 0x3F, 0x40, true}` — wrong range, wrong inversion mask interpretation
- `tests/tst_p1_codec_hl2.cpp` clamp range `[0, 31]` — wrong on the negative side
- RX-side wire path inversion `out[4] = (31 − rxUserDb) & 0x3F | 0x40` already lives in `P1CodecHl2.cpp:53` (PR #85), but caller passes raw 0–31 not signed −28..+32

Fix is two-line caps + UI-side sign conversion + test range update.

---

## 3. Design

### 3.1 Tab structure

```
Setup → Hardware → ...
    ├── Radio Info                     (existing, no change)
    ├── Hermes Lite Control            (relabel of "OC Outputs" tab when HL2 connected)
    ├── Ant/Filters                    (relabel of "Antenna Control" tab when HL2 connected)
    ├── HL2 Options                    (NEW)
    ├── HL2 I/O Board                  (existing — keep, this is our diagnostic surface)
    └── Bandwidth Monitor              (existing, no change)
```

Relabels are gated on `caps.boardKind == BoardKind::Hermes Lite` (set by
`BoardCapabilities`); when a non-HL2 radio is connected, labels revert.

### 3.2 New `HL2 Options` tab — three group boxes

| Group | Source-first parity | NereusSDR backing |
|---|---|---|
| **Hermes Lite Options** (8 knobs) | `groupBoxHL2RXOptions` (mi0bot setup.designer.cs:11086-11105) | New `Hl2OptionsModel` + AppSettings keys `hl2/<setting>` per-MAC. Most knobs need C&C bank-2 wire bits we don't currently emit — defer wire-format work to follow-up PR, ship UI + persistence first. |
| **I2C Control** | `groupBoxI2CControl` (`chkI2CEnable` + R/W tool) | Reuses existing `IoBoardHl2::enqueueRead/Write` queue — we already have the wire path from today's WIP. |
| **I/O Pin State** | `grpIOPinState` (output + input LED strips, click-to-toggle gated by `chkI2CEnable`) | Promote today's inline OC LED widget on `Hl2IoBoardTab` to a reusable `OcLedStripWidget`. Output strip click writes via `IoBoardHl2::enqueueWrite(0x1d, 169, mask)`; input strip is read-only (polled every 40 ms when board detected). |

### 3.3 Hermes Lite Control sub-tab completion (SWL real + VHF placeholder)

`OcOutputsTab` is a `QTabWidget` already mirroring mi0bot's
`grpTransmitPinActionSWL` nested-tab structure
(`setup.designer.cs:14362-14364` — three sub-tabs HF/VHF/SWL). Today
ships HF + a SWL `QLabel` placeholder; VHF is missing entirely. The
N2ADR Filter master toggle on the HF sub-tab writes per-band entries
to **both** the HF amateur grid (10 bands) and the SWL grid (13
bands) per mi0bot `setup.cs:14324-14368`. Without the SWL tab being
real, the user can see only the HF half of what N2ADR is doing.

| Sub-tab | mi0bot source | NereusSDR action this PR |
|---|---|---|
| **HF** (`tpOCHFControl`) | full RX/TX matrix + USB BCD + Ext PA HF + chkHERCULES + Penny Ext-Ctrl + reset | Existing `OcOutputsHfTab` — no change |
| **VHF** (`tbOCVHFControl`) | VHF transmit pin matrix + Ext PA VHF + Penny Ext-Ctrl-VHF + reset | New: `QLabel` placeholder ("VHF — not used by HL2; coming for non-HL2 boards"). Sub-tab count parity with mi0bot. ~5 lines. |
| **SWL** (`tpOCSWLControl`) | 13 SWL bands × 7 pins RX matrix + Ext PA SWL + Ext-Ctrl SWL + reset | New: `OcOutputsSwlTab` mirroring HF tab structure. 13 bands: 120m, 90m, 61m, 49m, 41m, 31m, 25m, 22m, 19m, 16m, 14m, 13m, 11m. ~250 lines. |

The chkHERCULES (N2ADR Filter) handler in our `Hl2IoBoardTab` /
`RadioModel` reconcile path must extend to also write the 13 SWL pin-7
entries when the toggle flips on, and clear them when off — mirroring
mi0bot's `chkHERCULES_CheckedChanged` complete pattern at
`setup.cs:14324-14368`. The OcMatrix backing already supports SWL
band storage (it's keyed on the `Band` enum which has SWL entries);
only the writer + the renderer are missing.

### 3.4 Signed ATT range fix

Two-file change:

- `BoardCapabilities.cpp` HL2 entry: caps `{−28, +32, 1, true, 0x3F, 0x40, true}` (range now signed)
- `RxApplet` step-ATT control: range from caps (already does), so the spinbox auto-picks up new range
- `tests/tst_p1_codec_hl2.cpp`: extend test fixture to cover the signed corners: `userDb = −28` → `(31 − (−28)) & 0x3F = 59 = 0x3B` → byte `0x7B`; `userDb = +32` → `(31 − 32) & 0x3F = 0x3F` → byte `0x7F`; `userDb = 0` → `0x5F`. mi0bot encodes the full signed range through the same `(31 − N)` formula; the 6-bit mask is what makes negatives valid wire bytes.

Wire path itself is unchanged — `out[4] = ((31 − userDb) & 0x3F) | 0x40` already lives in `P1CodecHl2.cpp:53` (PR #85). Caller change only: `RxApplet`/StepAttenuatorController stops clamping to `[0, 31]` and passes the signed value straight through.

### 3.5 Carry-forward of today's WIP

Today's 12 modified files include changes that belong with this PR (they
already implement the diagnostic surface this design depends on):

| File | Change | Already done | Belongs here because |
|---|---|---|---|
| `P1RadioConnection.{h,cpp}` | Sequenced I2C probe state machine + REG_CONTROL=1 init + ocByte change logging | ✓ | Phase B' (partial) — gates §3.2 I2C Control & I/O Pin State usability |
| `IoBoardHl2.{h,cpp}` | New signals `i2cTxComposed`, `currentOcByteChanged`; raw IN-byte logging | ✓ | Powers §3.2 I/O Pin State live indicators |
| `P1CodecHl2.cpp` | RX 31−N inversion (already in PR #85 path) + tx-composed signal emission | ✓ | Wire layer this design assumes |
| `Hl2IoBoardTab.{h,cpp}` | Status label clarification ("mi0bot custom I/O board (0x41): Not detected"), persistence-key fix `hl2/n2adrFilter` → `hl2IoBoard/n2adrFilter`, live OC pin LED indicator, `restoreSettings` reconcile via `onN2adrToggled` | ✓ | Bug fixes that should not wait for the new tab |
| `RadioModel.cpp` | App-launch OcMatrix reconcile from persisted N2ADR setting | ✓ | Closes the "no clicks without checkbox" symptom regardless of UI work |
| `tst_*.cpp` | Test updates following the above | ✓ | |

These should be staged into one PR with the new `HL2 Options` tab + signed
ATT — they're all "make HL2 work like mi0bot."

---

## 4. Out of scope (explicitly deferred)

- **Hermes Lite Options wire-format implementation.** UI + persistence
  ship; the C&C bank-2 byte composition for CL2 freq, ext-10MHz, PTT
  hang, TX latency, PS sync, band volts is a follow-up PR. mi0bot
  composes these at `console.cs:HermesLite_*` helpers — a separate
  source-first port session.
- **I2C bus 0 surface.** mi0bot exposes bus 0/1 in the I2C tool combo;
  we currently only wire bus 1. Follow-up.
- **Per-band BPF mapping editor.** The existing OC matrix on the
  Hermes Lite Control tab already covers per-band-per-pin assignment;
  no new editor needed for N2ADR.

## 5. Rebase plan

Current branch is 2 commits behind `origin/main` (PR #151 spectrum repaint,
PR #152 TX polish v3). Today's 12-file WIP is independent of both.

1. `git stash push -u -m "hl2-bringup-2026-04-30-wip"` (12 files)
2. `git fetch origin && git rebase origin/main`
3. `git stash pop`
4. Resolve conflicts (none expected — different subsystems)
5. Rebuild + verify nothing regressed (smoke test: connect to HL2, check
   bandwidth monitor still reads, OC LED still updates per band)
6. Stage carry-forward commits per §3.4 grouping; open PR

## 6. Constraints (per CLAUDE.md)

- GPG-sign every commit (`20C284473F97D2B3`)
- Inline-cite stamps on every ported line: `// From mi0bot setup.designer.cs:11086-11105 [v2.10.3.13-beta2]`
- Verbatim Thetis/mi0bot file headers on any new file that ports logic
- PROVENANCE table updated for new files
- Hardware verification before merge (per `phase-hl2-end-to-end-plan.md` §5 rubric, plus: HL2 Options tab loads on HL2-connect, three group boxes visible, signed ATT spinbox accepts −28..+32)

---

**Author:** J.J. Boyd ~ KG4VCF, with AI tooling.
