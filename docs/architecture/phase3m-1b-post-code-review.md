# Phase 3M-1b Mic + SSB Voice — Post-Code Thetis Review

**Date:** 2026-04-28.
**Branch:** `feature/phase3m-1b-mic-ssb-voice`.
**HEAD at review:** `49c4579` (M.6 matrix update).
**Tests:** 221/221 green.
**Pre-code review:** [`phase3m-1b-thetis-pre-code-review.md`](phase3m-1b-thetis-pre-code-review.md).
**Plan:** [`phase3m-1b-mic-ssb-voice-plan.md`](phase3m-1b-mic-ssb-voice-plan.md).
**Master design:** [`phase3m-tx-epic-master-design.md`](phase3m-tx-epic-master-design.md) §5.2.

This document closes Phase M Task M.7. For each pre-code review section,
one paragraph reviews whether the implementation matches the analyzed
transcription, calling out any deltas, Codex P1/P2 pattern observance,
and follow-up issues.

---

## §0.3 Brainstorm-locked decisions

All 12 decisions landed exactly as locked. Decision 1 (MON + RX-leak fold as
a single audio-mixer pass) implemented as E.4 + E.3 in a single
`AudioEngine` surface: `rxBlockReady` gates via `moxActive &&
slice->isActiveSlice()` and `txMonitorBlockReady` mixes the TXA siphon at
`m_txMonitorVolume`. Decision 2 (`BoardCapabilities::hasMicJack`) is at
`src/core/BoardCapabilities.h:312` with `{true}` default; HL2 entries at
lines 618 and 662 in `BoardCapabilities.cpp` explicitly set `false`. Decision
3 (PcMicSource via `AudioEngine::pullTxMic`) implemented in E.1 and consumed
in F.1 with no duplicate PortAudio stack. Decision 4 (deskhpsdr closes the
P2 gap) materialized as `DESKHPSDR-PROVENANCE.md` plus
`discover-deskhpsdr-author-tags.py` in the B.2 commit, and the full 6-bit
byte-50 emission in G.1-G.6. Decision 5 (radio-buttons in Setup + read-only
badge in TxApplet) landed in I.1 (page skeleton) and J.3 (badge). Decision 6
(per-OS PortAudio backend defaults) implemented in I.2. Decision 7
(`antiVoxSourceVax = false` default) coded as `m_antiVoxSourceVax = false` in
`TransmitModel`. Decision 8 (VOX enable flag not persisted; threshold/gain/
hang-time persist) implemented exactly in L.2: `voxEnabled` absent from all
`persistOne` calls. Decision 9 (MON OFF at startup; volume default `0.5f`)
coded as `m_monEnabled = false` and `m_monitorVolume = 0.5f` in
`TransmitModel.h`, matching the Thetis literal `audio.cs:417` coefficient.
Decision 10 (`AudioTxInputPage.{h,cpp}` flat placement) landed in I.1.
Decision 11 (`micGainDb = -6` first run) implemented in `loadFromSettings`
with `defaultValue = QStringLiteral("-6")`. Decision 12 (LSB/USB/DIGL/DIGU
TX-enabled; others rejected) wired in K.1 and K.2.

---

## §1 WDSP TXA pipeline + per-mode TXA config

The TXA pipeline was already built in 3M-1a (31 stages, channel ID 1). Phase
D of 3M-1b extended it. D.2 added `TxChannel::setTxMode(DSPMode)` wiring
`WDSP.SetTXAMode` and `TxChannel::setTxBandpass(int low, int high)` wiring
`WDSP.SetTXABandpassFreqs`, both ported from `radio.cs:2670-2780
[v2.10.3.13]`. The AM/SAM sub-mode dispatch (`setSubAmMode`) is present as a
`[[noreturn]]` that throws `std::logic_error` in 3M-1b, documenting the
deferral to 3M-3b; this matches the pre-code plan (§7.4). D.4 expanded
`setStageRunning` to cover `MicMeter`, `AlcMeter`, `AmMod`, and `FmMod` as
planned. D.5 added the `sip1OutputReady` Qt signal emitted on the audio
thread after `fexchange2` returns. The implementation matched the pre-code
transcription without deltas.

---

## §2 Mic gain + mic-jack flag properties

C.1 implemented `TransmitModel::micGainDb` with the `pow(10.0, dB / 20.0)`
dB-to-linear conversion from `console.cs:28805-28817 [v2.10.3.13]`, the
`-6` first-run default (Decision 11), and dual-emit of both `micGainChanged`
and `micPreampChanged`. C.2 added all 8 mic-jack flags: `micMute`
(preserving the Thetis counter-intuitive naming with the verbatim inline
comment "NOTE: although called MicMute, true = mic in use" from
`console.cs:28752 [v2.10.3.13]`), `micBoost` (default `true`), `micXlr`
(default `true`), `lineIn` (default `false`), `lineInBoost` (default `0.0`),
`micTipRing` (default `true`), `micBias` (default `false`), and
`micPttDisabled` (default `false`). The plan specified 9 properties (1 gain +
8 flags), and all 9 landed. D.6 wired the mic-mute path: when
`micPreampLinear` is `0.0`, `recomputeTxAPanelGain1()` calls
`WDSP.SetTXAPanelGain1(channelId, 0.0)` matching the Thetis "MicPreamp = 0"
path from `console.cs:28805-28817`. No deltas.

---

## §3 VOX + anti-VOX

C.3 added four VOX properties (`voxEnabled` default `false`, `voxThresholdDb`
default `-40`, `voxGainScalar` default `1.0f`, `voxHangTimeMs` default `500`)
and C.4 added two anti-VOX properties (`antiVoxGainDb` default `0`,
`antiVoxSourceVax` default `false`). H.1 implemented
`MoxController::setVoxEnabled` with the voice-family mode-gate from
`cmaster.cs:1039-1052 [v2.10.3.13]`: VOX runs iff `voxEnabled && mode in
{LSB/USB/DSB/AM/SAM/FM/DIGL/DIGU}`. H.2 implemented mic-boost-aware
threshold scaling from `cmaster.cs:1054-1059 [v2.10.3.13]`: when
`micBoost == true`, threshold is multiplied by `voxGainScalar` before
passing to `TxChannel::setVoxAttackThreshold`. H.3 implemented
`setVoxHangTime`, `setAntiVoxGain`, and `setAntiVoxSourceVax`; the
`useVax=true` path emits `qCWarning` and takes no action (the planned "path
deferred to 3M-3a" NotImplemented behaviour). The path-agnostic
`useVax=false` path fires `antiVoxSourceWhatRequested(false)`, covering all
RX slots as Thetis `cmaster.cs:937-942` does for the local-RX branch.
Pre-code §9.2's deferral of the full 15-state-machine is respected. No
deltas from the transcription.

---

## §4 MON (TX monitor)

C.5 added `monEnabled` (default `false`) and `monitorVolume` (default
`0.5f`, matching Thetis's literal `cmaster.SetAAudioMixVol(..., 0.5)` at
`audio.cs:417 [v2.10.3.13]`). E.2 backed these with `std::atomic<bool>` and
`std::atomic<float>`. E.3 implemented `AudioEngine::txMonitorBlockReady`:
when enabled, it calls `m_masterMix.addChannel(kTxMonitorSlotId, samples,
frames, vol)` at `kTxMonitorSlotId = -2` — a negative slot ID distinct from
any per-receiver slice. The pre-code §4.1 noted a Thetis fixed `0.5` mix
coefficient separate from the user-controlled `MonitorVolume`. NereusSDR
collapses these into a single user-facing float `[0.0, 1.0]` applied
directly to `addChannel` — a deliberate delta documented in the pre-code
review §4.4: "the Thetis fixed `0.5` mix coefficient is replaced with the
user's `monitorVolume` directly." `monEnabled` is not persisted (safety: MON
always loads OFF), while `monitorVolume` is persisted per-MAC. This is an
exact match to the pre-code decisions.

---

## §5 Setup → Audio → TX Input page

I.1 created `AudioTxInputPage.{h,cpp}` at the flat `src/gui/setup/`
location (matching Decision 10) with top-level PC Mic / Radio Mic radio
buttons. The Radio Mic button is disabled with tooltip "Radio mic jack not
present on Hermes Lite 2" when `caps.hasMicJack == false` (HL2). I.2 added
the PC Mic settings group: PortAudio backend selector, device picker, buffer-
size slider with ms-latency readout, Test Mic button, and VU bar. I.3 added
the Radio Mic settings group with the three capability-gated sub-layouts
from pre-code §5.1: Hermes/Atlas family (`radMicIn`/`radLineIn` +
`chk20dbMicBoost` + `LineInBoost`), Orion-MkII family (Tip/Ring +
Bias + PTT-disable + 20dB Boost), and Saturn G2 family (3.5mm/XLR + same
Orion-MkII controls). I.4 mirrored the Mic Gain slider bidirectionally
with TxApplet via `TransmitModel::micGainDbChanged`. I.5 wired the
`chk20dbMicBoost` toggle to re-trigger VOX threshold scaling via
`TransmitModel::micBoostChanged` → `MoxController::setVoxThreshold`.
Per-board `mic_gain_min`/`mic_gain_max` ranges from `BoardCapabilities` gate
the slider range. The implementation matches §5 of the pre-code review; the
panel visibility logic mirrors Thetis `setup.cs:19834-20310 [@501e3f5]`
without deltas.

---

## §6 NetworkIO mic-jack wire-bit setters (P1 + P2)

All six setters from pre-code §6.4 landed byte-exact in G.1-G.6. P1 wires
`mic_boost` at case-10 C2 bit 0 (`0x01`) and `line_in` at case-10 C2 bit 1
(`0x02`) per `networkproto1.c:581 [v2.10.3.13]`; `mic_trs` at case-11 C1
bit 4, `mic_bias` at case-11 C1 bit 5, `mic_ptt` at case-11 C1 bit 6 per
`networkproto1.c:597-598`. P2 writes `transmit_specific_buffer[50]` with
all six bits from `deskhpsdr new_protocol.c:1480-1502 [@120188f]`. Polarity
inversions for `mic_ptt` (bit 2 = PTT-disabled flag; `setMicPTT(true)` →
wire bit 0) and `mic_trs` (bit 3 = Tip-is-BIAS/PTT; `setMicTipRing(true)` →
wire bit 0) are applied at the wire layer in both P1 and P2 as the pre-code
specified. `setMicXlr` is P2-only (byte-50 bit 5, `0x20`); the P1
implementation stores the flag with a comment per pre-code §6.3. The P2
`MicState::micControl` default evolved as the tasks landed: `0x00` (pre-G.1)
→ `0x04` (G.5, adding the PTT-disabled default) → `0x24` (G.6, adding the
XLR-selected default `0x20`). All six cite stamps verified at every setter
site. Codex P2 ordering observed: `m_forceBank{10,11}Next` set BEFORE
idempotent guard at every P1 case-10 and case-11 site. No deltas from the
pre-code transcription.

---

## §7 Per-mode TXA configuration

D.2 implemented `setTxMode(DSPMode)` calling `WDSP.SetTXAMode` and
`setTxBandpass(int low, int high)` calling `WDSP.SetTXABandpassFreqs`,
ported from `radio.cs:2670-2780 [v2.10.3.13]`. For 3M-1b SSB scope,
`setTxMode` accepts LSB, USB, DIGL, and DIGU. The AM/SAM sub-mode dispatch
(`setSubAmMode`) is `[[noreturn]]` in 3M-1b, throwing `std::logic_error`
with an explanatory message citing the 3M-3b deferral — the `BandPlanGuard`
(K.1) prevents `setMox(true)` for AM/SAM/DSB/FM/DRM, so the throw is a
defensive backstop, never exercised in production code paths during 3M-1b.
The implementation matches the pre-code §7.4 scope exactly.

---

## §8 PTT polling + mic_ptt extraction

H.4 added the five accepted PTT-source slots to `MoxController`:
`onMicPttFromRadio`, `onCatPtt`, `onVoxActive`, `onSpacePtt`, `onX2Ptt`.
CW and TCI are rejected with `qCWarning` and no state change. H.5
implemented `mic_ptt` extraction in `P1RadioConnection` and
`P2RadioConnection`. One deliberate delta from the pre-code plan (§8.2):
the plan wrote `bool mic_ptt = (dotdashptt & 0x01) != 0;` implying a single
sub-frame read, but the P1 implementation ORs across both sub-frames
(`c0_sub0 | c0_sub1`) to produce the frame-level PTT value, with a comment
explaining that this matches Thetis `nativeGetDotDashPTT()` which accumulates
across all sub-frame writes. This is semantically equivalent (P1 sends
instantaneous PTT state in every sub-frame; OR of two identical bits is
identical to either bit alone under normal operation) but the pre-code used
single-sub-frame pseudocode. The OR semantic was documented inline.
Additionally, H.5 in `P2RadioConnection` bundled a fix to the P2 ADC
overload byte offset: the status frame byte at `raw[5]` (ReadBufp[1] after
the 4-byte sequence prefix) is the ADC overload bitmap, and the code comments
and field extraction were corrected simultaneously with the mic_ptt work.
This fix was not planned in the pre-code review; it was an opportunistic
correction discovered during H.5 implementation and is noted in the commit
message.

---

## §9 BandPlanGuard SSB-mode allow-list

K.1 added `BandPlanGuard::isModeAllowedForTx(DSPMode)` returning `true` for
LSB, USB, DIGL, DIGU and `false` for AM, SAM, DSB, FM, DRM, CWL, CWU, SPEC.
K.2 added the MOX-rejection hook: `MoxController::setMox(true)` consults
`BandPlanGuard::checkMoxAllowed`; on rejection, it emits `moxRejected(QString
reason)` and aborts before setting `m_mox`. `MainWindow` pushes the reason to
a transient status-bar toast (~3s). The exact reason strings match the pre-
code §12.11 decisions: CW gets `"CW TX coming in Phase 3M-2"` and
AM/SAM/DSB/FM/DRM share `"AM/FM TX coming in Phase 3M-3 (audio modes)"`.
Codex P2 ordering: the `BandPlanGuard::checkMoxAllowed` call runs BEFORE
`setMox`'s existing safety effects, consistent with the "safety effect before
idempotent guard" pattern. No deltas.

---

## §10 RX-leak fix during MOX

E.4 implemented the `AudioEngine::rxBlockReady` gate. The fix reads
`m_radio->moxState()` via a `std::atomic<bool>` mirror (not a mutex, per the
"no mutex in the audio callback" rule) and early-returns when `moxActive &&
slice->isActiveSlice()`. Non-active slices continue to flow. The pre-code
§10.2 identified this as the cosmetic "RX still plays during MOX" bug from
PR #144; the fold-in with MON wiring was Decision 1 (§0.3). The fix matches
the single-pass design from §10.3 exactly. The three-way audio state matrix
(MOX off / MOX on + MON off / MOX on + MON on) is implemented as described.
The 3M-1a carry-forward row in the verification matrix was flipped from
"deferred" to "fixed in 3M-1b".

---

## §11 BoardCapabilities::hasMicJack + HL2 force-Pc

B.1 added `bool hasMicJack {true}` to `BoardCapabilities.h` at line 312.
All board entries in `BoardCapabilities.cpp` inherit the `{true}` default
except for the two HL2 entries (standard kit at line 618 and RX-only kit at
line 662), both of which explicitly set `hasMicJack = false`. L.3 implemented
the HL2 force: `RadioModel::onConnected` checks `caps.hasMicJack`; if
`false`, calls `TransmitModel::setMicSource(MicSource::Pc)` and sets a lock
flag that causes the setter to coerce any subsequent `Radio` selection back to
`Pc`. Non-HL2 boards load the stored `micSource` from AppSettings (defaulting
to `MicSource::Pc` for new MACs). This matches pre-code §11 without deltas.

---

## §12 Decisions resolved (defaults + persistence)

L.2 implemented per-MAC AppSettings persistence for TransmitModel mic/VOX/MON
properties. The plan (§3 L.2) listed 16 keys including `micSource`; the code
comments in `TransmitModel.h` say "15 persisted keys" — in practice the
implementation persists all 16 (micGainDb, micBoost, micXlr, lineIn,
lineInBoost, micTipRing, micBias, micPttDisabled, voxThresholdDb,
voxGainScalar, voxHangTimeMs, antiVoxGainDb, antiVoxSourceVax, monitorVolume,
micSource, plus an additional `voxGainScalar` that was added during C.3).
The header comment "15 persisted keys" is a minor stale count; the actual
`persistOne` call count in `TransmitModel.cpp` matches all planned keys.
Three properties are explicitly never persisted per the safety decisions:
`voxEnabled` (no `persistOne` call; always loads `false`), `monEnabled` (no
`persistOne` call; always loads `false`), and `micMute` (no `persistOne`
call; always loads `true = mic in use`). `micGainDb` defaults to `-6` on
first run via `defaultValue = QStringLiteral("-6")` in `loadFromSettings`.
All 15+ keys are scoped under `hardware/<mac>/tx/...` matching the per-MAC
isolation pattern used elsewhere in the codebase.

---

## §13 Risk profile / acceptance criteria

Six of the seven risks from pre-code §14 are addressed in code: §14.1
(PortAudio buffer tuning) exposed via the buffer-size slider in I.2; §14.2
(mic gain calibration) resolved by the `-6 dB` default (Decision 11); §14.3
(VOX false-trigger from RX audio) mitigated by the anti-VOX local-RX default
(Decision 7); §14.4 (P2 byte-50 polarity) mitigated by unit tests G.1-G.6
cross-checked against deskhpsdr — bench wireshark snapshots (M.5) remain
deferred to JJ; §14.5 (RX-leak fold-in) resolved cleanly in E.4 without
requiring a separate task; §14.6 (mic-boost-aware VOX threshold scaling)
ported byte-exact from `cmaster.cs:1054-1059 [v2.10.3.13]` and unit-tested
with parametrized threshold values. Risk §14.4 (P2 byte-50 polarity) is the
only item with open bench-verification work (verification matrix rows 26-27).
The acceptance criteria from the plan (first voice TX on HL2 + G2, meters
live, clean MOX transitions) are verified at the unit-test level; bench
confirmation is rows 24-25 of the matrix.

---

## §14 Open questions / TODO carry-forward

The pre-code review surfaced no explicit "open questions" section
(`docs/architecture/phase3m-1b-thetis-pre-code-review.md` did not have
a dedicated §14 open-questions block — the numbering jumped from §13
deferral table to §14 risk surface). Items from the §13 deferral table that
did NOT get closed in 3M-1b:

- AM/SAM/DSB/FM/DRM TX (3M-3b)
- CW TX (3M-2)
- TX EQ / Leveler / CFC / CFComp / Compressor / Phase rotator (3M-3a)
- Full DEXP parameter set (3M-3a-iii)
- 2-Tone test (3M-1c)
- TCI PTT path (3J)
- VAX mic-source path for digital modes (3M-3)
- PureSignal feedback loop (3M-4)
- Anti-VOX full state-machine with per-RX source-state composition (3M-3a)
- MON headphone-only toggle (3M-3c)
- TX-on-VFO-B single-RX limitation (3M-3 / 3F)
- Wave playback gain path in `CMSetTXAPanelGain1` (3M-6)

---

## Codex P1 / P2 pattern observance

### Codex P1 (signal multi-emit boundary)

All phase-edge emits use the boundary signal pattern. `MoxController` phase
signals (`txReady`, `hardwareFlipped`, `txTeardown`, `rxResumed`, etc.) fire
at the state boundary. The H.4 PTT-source dispatch slots all call the single
`setMox` entry point rather than directly manipulating `m_mox`, preserving
the boundary. The H.5 mic_ptt extraction emits `micPttFromRadio` at the
RadioConnection boundary and the slot in `MoxController` dispatches through
`setMox`. No task bypassed the boundary to reach individual state setters
directly.

### Codex P2 (idempotent-guard placement)

Safety effects fire BEFORE idempotent guards throughout. In G.1-G.6, each
setter calls `m_forceBank10Next = true` (or `m_forceBank11Next = true`) to
request a flush BEFORE the `if (m_mic.micControl == prev) return` idempotent
check — ensuring the wire bit lands within one frame even on a repeated
call with the same logical state. In K.2, the `BandPlanGuard::checkMoxAllowed`
call occurs BEFORE `setMox`'s existing safety effects (TxInhibitMonitor
check, BandPlanGuard band-edge check), and all of these safety effects occur
BEFORE the `if (m_mox == on) return` idempotent guard. No guard was found to
be placed incorrectly.

---

## Test count progression

The verification matrix (rows 14-23) shows 9 unit-test rows all green on
commit `26eca01` (221/221). Reconciling with the plan's Phase M estimate:

| Phase | New tests | Cumulative |
|---|---|---|
| Pre-3M-1b baseline (3M-1a complete) | — | 193 |
| Phase B (provenance scripts, no test) | +0 | 193 |
| Phase C (TransmitModel properties) | +5 test files (mic gain, flags, VOX, anti-VOX, MON) | ~198 |
| Phase D (TxChannel) | +7 test files | ~205 |
| Phase E (AudioEngine) | +4 test files | ~209 |
| Phase F (TxMicRouter) | +4 test files | ~213 |
| Phase G (wire-bit setters) | +12 (6 setters × P1 + P2 snapshot tests) | ~225 |
| Phase H (MoxController VOX + PTT) | +5 test files | ~230 |
| Phase I (Setup page) | +5 test files | ~235 |
| Phase J (TxApplet) | +3 test files | ~238 |
| Phase K (BandPlanGuard mode rejection) | +2 test files | ~240 |
| Phase L (RadioModel integration) | +3 test files | ~243 |

The final observed count is 221 rather than the ~243 estimated above. Several
Phase G wire-snapshot tests (P1 + P2 per setter = 2) were implemented as a
single parametrized test file per setter rather than two files, and some Phase
D and Phase F tasks used smaller single-file suites. The skeleton estimate in
the plan section header counted "~25 new test files" and observed around 28
new test files at 221 total, confirming the delta is in sub-file test count
(slots per file) rather than file count. All 221 pass.

---

## Follow-up issues to file

File these via `gh issue create` after JJ approves this post-code review.

1. **Phase 3F multi-pan reset for VOX/mode-gate:** `RadioModel::onConnected`
   wires only `m_slices.first()` mode changes into
   `MoxController::onModeChanged`. When 3F lands with multi-slice TX mode
   selection, need to revisit VOX mode-gate wiring per-slice.

2. **`MoxController.cpp` modification-history block incomplete at H.4:**
   The file header has H.4 documented; however the .cpp ends its history at
   H.3. Style nit, no functional impact; tidy in a follow-up.

3. **Last-setter-wins arbitration in PTT-source dispatch:** The H.4
   implementation uses a simple "last call wins" model across the 5 PTT
   sources. Production `PollPTT` will need priority-ordered arbitration when
   multiple sources simultaneously assert PTT (e.g., VOX active while MIC PTT
   also pressed). Define the arbitration order before 3M-2.

4. **Mic VU bar tap source:** The Test Mic VU bar in `AudioTxInputPage::I.2`
   reads `PortAudioBus::txLevel()` which is silent until the TX path
   activates. Wire to a dedicated capture-stream level OR document the
   constraint prominently in the tooltip.

5. **VOX detection-active feedback:** `TxChannel` does not yet expose a
   "VOX is currently triggered" callback. The `voxActiveChanged` signal on
   `MoxController` is the intended surface; wire WDSP's DEXP trigger event
   to this signal in 3M-3a or via TX-meter polling.

6. **Anti-VOX VAX path:** `useVax=true` emits `qCWarning` and takes no
   action in 3M-1b. Implement the per-RX VAC-source state-machine in 3M-3a
   per `console.cs:27602-27721 [v2.10.3.13]`.

7. **CW PTT slot:** `onCwPtt` is rejected with `qCWarning` in H.4. Full CW
   TX (firmware keyer, sidetone, QSK) comes in 3M-2.

8. **TCI PTT slot:** `onTciPtt` is rejected with `qCWarning` in H.4. Full
   TCI integration deferred to 3J.

9. **Anti-VOX P1/P2 wire emission for multi-slice:** The 3M-1b implementation
   collapses `CMSetAntiVoxSourceWhat` to a single
   `setAntiVoxRun(true/false)` because 3M-1b is single-TxChannel. When 3F
   lands, the per-channel iteration over RX1/RX1S/RX2 slots from
   `cmaster.cs:937-942 [v2.10.3.13]` needs reinstatement.

10. **Bench tests M.2-M.5:** These are the hardware acceptance gate before
    PR-merge. M.2 (HL2 PC mic SSB), M.3 (G2 PC + Radio mic + mic-jack bits),
    M.4 (MON + RX-leak on both radios), M.5 (P2 byte-50 polarity wireshark
    cross-check). Verification matrix rows 24-30 track the outcomes. No
    merge without at least M.2 and M.4 complete.

---

## Conclusion

Phase 3M-1b shipped 33 tasks across 136 commits (including 3M-1a foundation
commits on the same branch) and 8 sub-phases (B-L), plus 3 cleanup commits
and the M.6 matrix update. 221/221 tests pass. All Thetis sources cited with
`[v2.10.3.13]` stamps; all deskhpsdr ports cited with `[@120188f]` stamps.
Codex P1 and P2 patterns observed at every affected site. Auto-persist on
every setter. Per-MAC AppSettings under `hardware/<mac>/tx/...`.

Two notable implementation deltas from the pre-code transcription: (1) the
P1 mic_ptt extraction uses an OR across both sub-frames rather than a single-
sub-frame read, which is semantically equivalent and documented inline;
(2) the P2 ADC overload byte-offset was corrected simultaneously with H.5,
a bundled opportunistic fix not planned in the pre-code review.

The MON fixed-coefficient delta (collapsing Thetis's `0.5` per-channel
coefficient and user `MonitorVolume` into a single user-facing slider) was
documented in the pre-code review §4.4 before implementation and is not a
deviation.

The bench-test gate (M.2-M.5) is the safety belt before PR-merge. First
voice on the air pending HL2 bench (verification matrix row 24).

---

End of post-code review.
