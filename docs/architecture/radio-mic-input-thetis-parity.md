# Radio Mic Input: Thetis Parity Port

**Status:** design (2026-05-05)
**Author:** J.J. Boyd ~ KG4VCF, AI-assisted via Anthropic Claude Code
**Thetis source stamp:** `[v2.10.3.13+501e3f51]`
**Scope owner:** single PR; no sub-project decomposition

---

## 1. Goal

Bring the NereusSDR radio-mic-input feature to byte-for-byte parity with Thetis
across every Setup control, every wire bit, every per-SKU gating decision, and
the radio-mic data path itself. After this lands, a user opening Setup on any
supported board sees the same controls Thetis would show, every checkbox
actually reaches the radio, and the user's PC-Mic / Radio-Mic toggle actually
switches the audio source on both Protocol 1 and Protocol 2.

This is the follow-on to issue #182. That issue closed the Mic PTT Disable
wire-bit gap; this design closes every other gap on the same surface.

---

## 2. Thetis source map

The "radio mic input" feature surface in Thetis lives in three places on the
Setup form, plus one data path through ChannelMaster.

**Setup → Transmit tab → grpBoxMic** ("Mic" group). Per TX profile, all SKUs.

| Control | Property | Default | Wire-side path |
| --- | --- | --- | --- |
| `radMicIn` / `radLineIn` | `console.LineIn` | `false` (Mic) | `SetMicGain` → `NetworkIO.SetLineIn` |
| `chk20dbMicBoost` | `console.MicBoost` | `true` | `SetMicGain` → `NetworkIO.SetMicBoost` |
| `udLineInBoost` | `console.LineInBoost` | `0.0` dB | `SetMicGain` → 32-entry index lookup → `NetworkIO.SetLineBoost` |
| `udMicGainMin` | `console.MicGainMin` | `-40` | UI slider lower bound |
| `udMicGainMax` | `console.MicGainMax` | `+10` | UI slider upper bound |

(Cite: `setup.cs:7684-7687`, `setup.cs:9685-9694`, `setup.cs:14257-14279`,
`setup.designer.cs:46740-46924`, `console.cs:13213-13238`,
`console.cs:40827-40859`. Persistence: TX-profile columns
`Mic_Input_On`, `Mic_Input_Boost`, `Line_Input_Level` per `database.cs:4457-4459`.)

**Setup → General tab → H/W Select sub-tab → Hardware Options group → ORION
panel** (per radio, gated by SKU).

| Control | Property | Default | Wire-side path |
| --- | --- | --- | --- |
| `radOrionPTTOn` / `radOrionPTTOff` | `console.MicPTTDisabled` | `false` (PTT enabled) | `NetworkIO.SetMicPTT(disabled?1:0)` |
| `radOrionMicTip` / `radOrionMicRing` | (control state IS the truth, no console property) | Tip checked | `NetworkIO.SetMicTipRing(tip?0:1)` |
| `radOrionBiasOn` / `radOrionBiasOff` | (control state IS the truth) | Off checked | `NetworkIO.SetMicBias(on?1:0)` |
| `radSaturnXLR` / `radSaturn3p5mm` (G2 only) | (control state IS the truth) | 3.5 mm checked | `NetworkIO.SetMicXlr(xlr?1:0)` |

(Cite: `setup.cs:16450-16477`, `setup.designer.cs:8599-8800`,
`console.cs:19757-19766`. Persistence: control-name reflection in
`SaveOptions` (`setup.cs:1519-1632`); these are radio-specific controls
saved by control name string, not via the TX-profile DB.)

**Setup → General tab → Options sub-tab → grpGeneralOptions.**

| Control | Property | Default | Wire-side path |
| --- | --- | --- | --- |
| `chkGenAllModeMicPTT` | `console.AllModeMicPTT` | `false` | client-only PTT mode-gate |
| `udGenPTTOutDelay` | `console.PTTOutDelay` | `20` ms (designer wins on load) | `MoxController.PTTOutDelay` timer |
| `chkGeneralDisablePTT` | `console.DisablePTT` | `false` | gates entire `PollPTT` loop |

(Cite: `setup.cs:12127-12129, 14302-14305, 19694-19754`,
`setup.designer.cs:9029-9226, 8827`.)

**Per-SKU enable map** for the ORION panel (`setup.cs:19830-20405`).
`pnlGeneralHardwareORION.Enabled = false` for: HERMES, ANAN10, ANAN10E, ANAN100,
ANAN100B, ANAN100D, REDPITAYA. `Enabled = true` for: ORION (ANAN200D),
OrionMKII (ANAN7000DLE / ANAN8000DLE), ANVELINAPRO3, ANAN-G2, ANAN-G2-1K.
`panelSaturnMicInput.Enabled = true` only for ANAN-G2 and ANAN-G2-1K
(`setup.cs:6181-6189`). HERMESLITE and ORIONMKII have **no explicit case**
in the SKU switch (fall through to previous SKU's state).

**Radio mic data path.** Mic samples come back from the radio interleaved with
I/Q on P1 and on a dedicated UDP port on P2.

- **P1 EP6 frame** (`networkproto1.c:391-410`): per-frame, mic word is two bytes
  at stride `(2 + nddc * 6)`. Scaled `(byte0 << 24 | byte1 << 16) * (1.0 /
  2147483648.0)` (the 16-bit sample lands in the upper half of a 32-bit word
  before division by 2^31, giving a useful range of ±0.5). Q forced to 0
  (mono-as-complex). Per-radio decimation: factor 1/2/4/8 for 48/96/192/384 kHz
  DDC rate (`netInterface.c:1285-1311`). Then `Inbound(inid(1, 0), spp, buf)`.
- **P2 mic frame** (`network.c:534, 761-773`): UDP port `p2_custom_port_base + 1`
  (= 1026 by default). 132-byte datagram (4-byte seq + 128 mic bytes = 64
  samples). Same scaling formula. Fixed 48 kHz from radio (no client-side
  decimation). Then `Inbound(inid(1, 0), 64, buf)`.
- **Both protocols** call `Inbound()` on every frame regardless of MOX state.
  The mic ring drains continuously; MOX only gates the OUTBOUND I/Q
  (`networkproto1.c:723`).
- **CM worker** (`cmbuffs.c:151-168`) wakes on the ring semaphore, calls
  `xcmaster()`, which runs `xdexp(tx)` (DEXP/VOX) and `fexchange0(...)` for
  the TXA channel (`cmaster.c:377-398`). Mic input rate to WDSP is fixed at
  48 kHz (`cmaster.cs:518` `SetXcmInrate(txinid, 48000)`).
- **Mic source switching**: `asioIN()` (`cmasio.c:120-135`) writes into the
  same `pcm->in[stream]` buffer if VAC/ASIO is the source. There is no
  explicit "use radio mic" bool; the source that fills the buffer last
  before `fexchange0` wins. NereusSDR will model this differently (see §5).

---

## 3. NereusSDR current state and gap matrix

| Surface | Status |
| --- | --- |
| 5 wire bits (mic boost, line in, tip/ring, bias, XLR) | model setters and connection setters exist; **none connected end-to-end via `RadioModel::wireConnectionSignals`** |
| `setLineInBoost` translation layer (dB → 5-bit wire index) | **does not exist on RadioConnection.** Model holds dB; nothing converts to the 5-bit wire field. The `setLineInGain(int)` setter writes the raw 5-bit index but no caller invokes it from the dB property. |
| Mic PTT Disabled | wired (this PR's predecessor commit) |
| `AllModeMicPTT` | **completely missing.** No model property, no Setup control, no MoxController gate. |
| `PTTOutDelay` | hardcoded `constexpr int kPttOutDelayMs = 20` in `MoxController.h:216`. Not user-configurable. |
| `MicGainMin` / `MicGainMax` | **missing.** Mic gain slider uses fixed bounds from `BoardCapabilities`. |
| Per-SKU panel gating | mostly correct; ANAN100D (Angelia) routes through Hermes group when Thetis disables the ORION pane outright; Andromeda has no group at all |
| Radio Mic source on P1 | **two parallel paths.** `TxMicSource` (production, fed directly from the connection) is what the TX worker reads. `RadioMicSource` (dead, subscribed to a `micFrameDecoded` signal that is never emitted) is what the user's PC-Mic / Radio-Mic toggle routes through. **The toggle is decorative on P1.** |
| Radio Mic source on P2 | **does not exist.** No mic frame ingress, no port-1026 socket, no parser. P2 users always get PC mic regardless of toggle. |
| `DisablePTT` | model property exists per-radio; needs a Setup → General → Options checkbox + wiring into the PTT poll loop. |

---

## 4. Design

The port is structured as five layers, ordered bottom-up. Each layer is
testable in isolation; integration is wired in `RadioModel::wireConnectionSignals`.

### 4.1 Connection layer: wire bits

Add one missing virtual to `RadioConnection`:

- `virtual void setLineInBoost(double dB) = 0`. Translates the dB value into
  the 5-bit wire index using the 32-entry table from `setup.cs:9691-9694` and
  `console.cs:40827-40838` (range -34.5 dB to +12.0 dB in 1.5 dB steps; index 0
  = -34.5, index 23 = 0.0, index 31 = +12.0). Implementation calls
  `setLineInGain(index)`. Both P1 and P2 inherit the default (calls
  `setLineInGain` on `this`).

The other six setters (`setMicBoost`, `setLineIn`, `setMicTipRing`, `setMicBias`,
`setMicXlr`, `setMicPTTDisabled`) already exist and are correct. No wire-byte
changes; only the model→connection wiring is missing.

**P2 mic ingress.** Add to `P2RadioConnection`:

- A second `QUdpSocket` bound to port 1026 + `p2CustomPortBase` offset (default
  1025 + 1 = 1026, configurable via the existing P2 base port logic).
- A datagram parser that validates the 4-byte seq + 128 mic bytes structure,
  extracts 64 mono mic samples per frame, applies the `* 1.0 / 2147483648.0`
  scaling, and forwards them to the `TxMicSource` ring (the unified path; see
  §4.4).
- Sequence error tracking on the mic stream, identical to existing per-port
  tracking.
- The socket lives on the connection worker thread (same as the C&C / I/Q
  sockets), so no extra QThread is needed.

### 4.2 Codec context: defaults

`P2RadioConnection::MicState::micControl` default already corrected to `0x20`
(XLR on by default, all other bits clear) by issue #182.

P1 codec `CodecContext::p1MicPTTDisabled` already corrected by issue #182.
Other P1 codec context fields (`p1MicBoost`, `p1LineIn`, `p1MicTipRing`,
`p1MicBias`, `p1LineInGain`) already match Thetis defaults.

`m_micXlr{true}` in `RadioConnection` matches Thetis Saturn ships-as-XLR
default. No change.

`m_micBoost{false}` in `RadioConnection` does **not** match Thetis (`mic_boost
= true` per `console.cs:13237`). **Change to `m_micBoost{true}`** so a fresh
connection writes the same wire bit Thetis would. The TransmitModel default
(`m_micBoost = true` per `TransmitModel.h:1728`) is already correct; this
brings the connection-side default in line.

### 4.3 Model layer: TransmitModel additions

Three new properties:

- `bool allModeMicPTT` (default `false`, per-MAC persistence). Getter, setter,
  signal `allModeMicPTTChanged(bool)`. Cite Thetis `console.cs:12022`.
- `int pttOutDelayMs` (default `20`, per-MAC persistence, range 0..500 from
  Thetis `udGenPTTOutDelay` designer). Getter, setter, signal
  `pttOutDelayMsChanged(int)`. Cite Thetis `console.cs:19694`.
- `int micGainMinDb` (default `-40`, per-MAC, range -96..0). Cite Thetis
  `udMicGainMin` designer.
- `int micGainMaxDb` (default `+10`, per-MAC, range 1..70). Cite Thetis
  `udMicGainMax` designer.

(`disablePTT` already exists on TransmitModel; only the Setup checkbox needs to
be added, see §4.5.)

Persistence: scoped under `hardware/<mac>/transmit/...` for the per-radio keys
(matching the existing pattern for `Mic_PTT_Disabled`, etc., in
`MicProfileManager.cpp`). The TX-profile-bound keys (`MicBoost`, `LineIn`,
`LineInBoost`) stay in the existing per-profile store.

### 4.4 Mic data path unification

Two changes resolve the dead-path / dual-path situation:

- **Delete `RadioMicSource` and the `micFrameDecoded` signal** entirely. They
  are dead code. Audit (already done) confirms `micFrameDecoded` is declared
  in `RadioConnection.h` but never emitted by any subclass; `RadioMicSource`
  is only constructed by `CompositeTxMicRouter` and only ever returns silence.
  Removing them simplifies the audit surface and prevents any future
  accidental wiring through a known-broken path.
- **Make `TxMicSource` the single source of truth for radio mic samples on
  both protocols.** P1 already feeds it via
  `P1RadioConnection::parseEp6Frame` + `decimateMicSamples`. P2 will feed it
  via the new port-1026 parser (§4.1).
- **Wire the user's PC-Mic / Radio-Mic toggle to actually switch sources.**
  `CompositeTxMicRouter` survives but its "Radio" branch now reads from
  `TxMicSource` instead of `RadioMicSource`. When PC Mic is selected,
  `TxMicSource` is left filling its ring and the router pulls from
  `PcMicSource`. When Radio Mic is selected, the router pulls from
  `TxMicSource`. The TX worker thread reads from whichever source the router
  is serving.
- **TX-on gating** stays consistent with Thetis: mic samples flow into the ring
  unconditionally; MOX only gates the outbound I/Q. This matches what
  NereusSDR already does for the TX pump.

This eliminates the dead path, gives the user toggle real meaning on both
protocols, and brings the data flow byte-for-byte in line with Thetis.

### 4.5 UI layer: Setup pages

Three Setup pages get edits.

**Audio TX Input page** (`AudioTxInputPage.cpp`). Already has the mic-jack
group structure. Two adjustments:

- Add `udMicGainMin` and `udMicGainMax` spinboxes to the "PC Mic" mic-gain
  block; they bind to the new TransmitModel properties and update the gain
  slider's range live. (Thetis places these in grpBoxMic alongside the mic
  controls; NereusSDR puts them in the PC Mic group since that's where the
  slider lives.)
- Tighten the per-SKU gating in `updateRadioMicGroupVisibility`: introduce a
  helper `boardShowsOrionMicJackPanel(HPSDRHW)` that returns true for Orion,
  OrionMKII, Saturn, SaturnMKII (matching Thetis's `pnlGeneralHardwareORION`
  enable list). Angelia/ANAN100D drops to no-mic-jack panel (Thetis behavior:
  pane is disabled and no fallback shown). Hermes/HermesII keeps showing the
  Hermes-style group. Andromeda gets the Orion group (Thetis treats it as
  Orion-class; needs verification on bench).

**General Setup page** (new sub-page or extension of an existing page; locate
under Setup → General → Options to match Thetis's IA):

- `chkGeneralDisablePTT` checkbox bound to `TransmitModel::disablePTT`.
- `chkGenAllModeMicPTT` checkbox bound to the new `allModeMicPTT` property.
- `udGenPTTOutDelay` spinbox (range 0-500 ms) bound to the new
  `pttOutDelayMs` property.

**SetupDialog routing**: register the new General Options page in the existing
SetupDialog navigation tree (NereusSDR's IA already has a "General" category;
this slots into it).

### 4.6 RadioModel wiring

`RadioModel::wireConnectionSignals` adds a single helper call,
`connectMicJackSignals()`, that wires every mic-related model signal to its
RadioConnection slot through queued connections (the connection lives on its
own worker thread). Each connect is followed by a prime-on-connect that pushes
the current model value once at boot. The pattern mirrors the existing
`connectMicPttDisabledSignal()` from the issue #182 fix.

Wires to add:

- `micBoostChanged → setMicBoost`
- `lineInChanged → setLineIn`
- `micTipRingChanged → setMicTipRing`
- `micBiasChanged → setMicBias`
- `micXlrChanged → setMicXlr`
- `lineInBoostChanged → setLineInBoost` (the new dB-aware setter)

`micPttDisabledChanged → setMicPTTDisabled` already wired by issue #182.

### 4.7 MoxController: AllModeMicPTT gate

Port the Thetis mode-gate from `console.cs:25480-25495` into
`MoxController::onMicPttFromRadio`. Pseudocode (translated to existing
NereusSDR helpers):

```
if (pressed) {
    const bool voiceMode = isVoiceMode(m_currentMode);
    if (!voiceMode && !m_allModeMicPTT) {
        // Thetis ignores mic_ptt outside voice modes unless AllModeMicPTT is set.
        return;
    }
    setPttMode(PttMode::Mic);
    setMox(true);
}
```

CW handling: Thetis console.cs:25473 already engages MOX for mic_ptt in CW
mode (as PTTMode.CW). NereusSDR's CW dispatch is deferred to phase 3M-2;
documenting the intended path here so the gate doesn't accidentally swallow
mic PTT in CW mode when 3M-2 lands.

`MoxController` gains `setAllModeMicPTT(bool)` slot wired from
`TransmitModel::allModeMicPTTChanged`, and `setPttOutDelayMs(int)` wired from
`pttOutDelayMsChanged` (which adjusts the existing pttOutDelayTimer interval).

The existing `kPttOutDelayMs = 20` constexpr stays as the boot-time default
the timer initializes with; the model setter overrides it on user change and
on connect-time prime.

---

## 5. Per-SKU defaults audit

Before merging, a defaults audit confirms NereusSDR matches Thetis's startup
state on a fresh install. Each cell is "current state → action".

| Property | Thetis default | NereusSDR current | Action |
| --- | --- | --- | --- |
| `MicBoost` (model) | `true` | `true` | none |
| `m_micBoost` (connection) | n/a (model is source) | `false` | change to `true` to match prime path |
| `LineIn` | `false` | `false` | none |
| `LineInBoost` (dB) | `0.0` | `0.0` | none |
| `MicTipRing` (Tip checked) | `Tip` (model bool `true`) | `true` | none |
| `MicBias` | `Off` (model bool `false`) | `false` | none |
| `MicXlr` (Saturn) | 3.5 mm checked (model bool `false`) | `true` | **change model default to `false`** to match Thetis Saturn 3.5 mm default |
| `MicPTTDisabled` | `false` | `false` (post #182) | none |
| `AllModeMicPTT` | `false` | n/a (new property) | default `false` |
| `PTTOutDelay` (ms) | `20` (designer); `0` after first profile load | `20` | model default `20` |
| `MicGainMin` | `-40` | n/a (new property) | default `-40` |
| `MicGainMax` | `+10` | n/a (new property) | default `+10` |
| `DisablePTT` | `false` | `false` | none (UI add only) |

(The `MicXlr` default delta is the only material change. Issue #182 fixed
`MicState::micControl` to `0x20`, which is XLR-bit-set; a Saturn user who has
never opened Setup currently boots with XLR routing. Thetis ships 3.5 mm by
default. We flip the model default; the prime-on-connect then writes the
correct wire bit. Existing user settings survive via persistence.)

---

## 6. Test plan

Three categories.

**Wire-byte unit tests** (one per setter, mirroring existing `tst_p1_*_wire`
and `tst_p2_*_wire` patterns):

- `tst_p1_line_in_boost_wire`: 32-entry index lookup correctness, default
  state, round-trip, cross-bit guard.
- `tst_p2_line_in_boost_wire`: same coverage on byte 51.
- Existing `tst_p1_mic_boost_wire`, `tst_p1_line_in_wire`,
  `tst_p2_mic_boost_wire`, `tst_p2_line_in_wire`,
  `tst_p1_mic_tip_ring_wire`, `tst_p1_mic_bias_wire`,
  `tst_p2_mic_xlr_wire` already cover the existing setters; no new tests
  needed for those.
- `tst_p2_mic_frame_parser`: P2 port-1026 datagram parsing (sequence handling,
  sample count, scaling, malformed packet rejection).

**Model→connection wiring tests** (one per signal, mirroring
`tst_radio_model_mic_ptt_wire`):

- `tst_radio_model_mic_jack_wire`: covers all 6 new connects + their primes
  in one test executable, 3 cases each (toggle reaches connection,
  round-trip, prime on connect).

**Integration tests**:

- `tst_mox_controller_all_mode_mic_ptt`: verify the mode-gate in
  `onMicPttFromRadio` (mic_ptt in USB → fires; mic_ptt in CWU without
  AllModeMicPTT → ignored; mic_ptt in CWU with AllModeMicPTT → fires).
- `tst_radio_model_ptt_out_delay`: verify `pttOutDelayMsChanged` reaches
  `MoxController::setPttOutDelayMs` and updates the timer interval.
- `tst_tx_mic_router_radio_source`: verify that on P1, switching the user
  toggle from PC Mic to Radio Mic actually routes from `TxMicSource` (not
  the deleted `RadioMicSource`); on P2, same with the new port-1026 ingress.
  Uses the existing `MockConnection` pattern.

**Bench tests** (manual, on actual hardware):

- ANAN-G2 (P2): plug a footswitch into the mic jack, press it, confirm TX
  engages. Toggle every Saturn-group control (XLR, Mic Bias, +20 dB Boost,
  Mic PTT Disable). Confirm each reaches the radio. Switch to Radio Mic
  source, confirm voice transmits through the radio's mic jack instead of
  the PC mic.
- HL2 (P1): regression check on the issue #182 prior bench result; confirm
  no change in mic-PTT behavior. The legacy compose path was flipped to
  direct in #182; this PR doesn't re-touch it.
- ANAN-10E or similar Hermes-class P1 board if available: same Setup
  toggles, same wire-bit confirmations. If no hardware available, document
  in the PR test plan.

---

## 7. Out of scope (deferred)

- Phase 3M-2 CW transmit (Thetis `console.cs:25473` mic_ptt → CW PTT branch).
  The mode-gate in §4.7 leaves the CW path open for 3M-2 to fill in.
- VAC bypass on PTT (Thetis `console.cs:25448-25458`). Tied to VAC1/VAC2
  routing which is its own design.
- TCI PTT (Thetis `console.cs:25461-25465`). Phase 3J.
- Mic AGC / mic limiter / mic anti-alias filter (Thetis runs these inside
  WDSP TXA, not on the protocol layer; nothing to port here.
- Andromeda board verification. Flagged as "needs bench" in the PR; no
  pre-merge change unless we have access.

---

## 8. Open questions

None blocking. The Andromeda case (§4.5 last bullet) is a verify-on-bench
item, not a design decision. Any other open questions get raised in the
implementation plan that follows this doc.

---

## 9. References

- This doc supersedes the issue #182 PR's "broader UI design call" deferral.
- Builds on prior 3M-1b mic-jack family infrastructure (commits in PR #149).
- Mechanically follows the issue #182 wire-up pattern (commit `f8d335f`).
- Thetis source root: `/Users/j.j.boyd/Thetis/Project Files/Source/`,
  stamp `[v2.10.3.13+501e3f51]`.
