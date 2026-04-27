# Phase 3M-1a — Two-Phase Thetis Pre-Code Review

**Status:** §1 Pre-code review (committed before any TDD task).
**Date:** 2026-04-25.
**Author:** J.J. Boyd ~ KG4VCF (with AI tooling).
**Upstream stamp:** Thetis `v2.10.3.13-7-g501e3f51` — every inline cite uses `[v2.10.3.13]`. deskhpsdr cites use the repo HEAD as cloned 2026-04-25.

---

## 0. What this document is

3M-1a is the first MOX byte and first RF on the air. Master design Section
5.1.5 calls it HIGH risk. Per master design 5.1.3, a **two-phase Thetis
review** (pre-code + post-code) is the primary defense against
wrong-bytes-on-the-wire — more important here than on any prior phase.

This file holds **both phases**:

- **§1 (this commit):** READ → SHOW for every Thetis surface that 3M-1a
  ports. Every TDD task in `phase3m-1a-tune-only-first-rf-plan.md` cites a
  subsection here.
- **§2 (appended after Phase H lands, before the PR opens):** post-code
  review — for each surface, did the impl match the pre-code transcription?
  Note any deltas + reasoning.

Source-first protocol (`CLAUDE.md` "READ → SHOW → TRANSLATE") governs every
DSP / radio / protocol piece below. UI plumbing (TxApplet, SetupPages) is
NereusSDR-native per `feedback_source_first_ui_vs_dsp`.

---

## 0.1. MOX-without-TUN behaviour (open-question resolution)

**Question:** in 3M-1a the MOX button is visible on TxApplet, but mic input
+ voice TX land in 3M-1b. What does Thetis do when the user hits MOX with
no mic source plumbed?

**Resolution from `console.cs:29311-29678 [v2.10.3.13]` reading:**
`chkMOX_CheckedChanged2` does **not** check whether a mic source is wired.
It engages MOX, calls `HdwMOXChanged(tx=true)`, calls
`AudioMOXChanged(true)`, and turns on the WDSP TX channel
(`WDSP.SetChannelState(WDSP.id(1, 0), 1, 0)`) regardless of whether mic
samples are flowing. With no source, the TX channel emits zero-I/Q (or
whatever the input buffer last held) into the wire.

**Implication for 3M-1a:** match Thetis. Allow MOX-without-TUN. With no
mic source, NereusSDR will write zero-I/Q frames to UDP port 1029 (P2) /
EP2 zones (P1) and the radio will transmit a suppressed-carrier silent
signal — harmless on the wire, no audible click, SwrProtectionController
sees `alex_fwd ≈ 0` and stays out of foldback (the 5W floor TODO from §7
matches exactly this case). No `BandPlanGuard`-style rejection.

This decision is **source-first**, not a NereusSDR design choice. We match
Thetis behaviour even where it's surprising.

---

## 1. `chkMOX_CheckedChanged2` — TX-direction MOX toggle handler

**Cite:** `console.cs:29311-29678 [v2.10.3.13]`.

### 1.1 Function signature

```csharp
private void chkMOX_CheckedChanged2(object sender, System.EventArgs e)
```

### 1.2 Verbatim source (excerpts; full body 367 lines)

Pre-flight gates and pre-change delegate (lines ~29311-29350):

```csharp
if(chkMOX.Checked && _ganymede_pa_issue)
{
    // abort the change if there is a ganymede pa issue
    chkMOX.CheckedChanged -= chkMOX_CheckedChanged2;
    chkMOX.Checked = false;
    chkMOX.CheckedChanged += chkMOX_CheckedChanged2;
    return;
}

bool bOldMox = _mox; //MW0LGE_21b used for state change delgates at end of fn

MoxPreChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, _mox, chkMOX.Checked); // MW0LGE_21k8

NetworkIO.SendHighPriority(1);
if (_rx_only && chkMOX.Checked)
{
    chkMOX.Checked = false;
    return;
}
```

VAC bypass logic and TX freq computation (CW pitch offset etc.):

```csharp
if (!(ARP.IsBusy && BypassVACWhenPlayingWAV)) // dont change vac bypass if it being used by ARP
{
    if (allow_mox_bypass && _current_ptt_mode != PTTMode.MIC &&
                            _current_ptt_mode != PTTMode.SPACE &&
                            _current_ptt_mode != PTTMode.CAT)
    { ... }
}
```

RX channel shutdown (non-full-duplex), ATT-on-TX dispatch, hardware flip:

```csharp
if (m_bATTonTX)
{
    if (HardwareSpecific.Model == HPSDRModel.HPSDR)
    {
        temp_mode = RX1PreampMode;
        SetupForm.RX1EnableAtt = false;
        RX1PreampMode = PreampMode.HPSDR_OFF;            // set to -20dB
        if (_rx2_preamp_present)
        {
            temp_mode2 = RX2PreampMode;
            RX2PreampMode = PreampMode.HPSDR_OFF;
        }
    }
    else
    {
        //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
        int txAtt = getTXstepAttenuatorForBand(_tx_band);
        if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
                        (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
                         radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)) txAtt = 31; // reset when PS is OFF or in CW mode

        SetupForm.ATTOnRX1 = getRX1stepAttenuatorForBand(rx1_band); //[2.10.3.6]MW0LGE att_fixes
        SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value

        updateAttNudsCombos();
    }
}
else
{
    NetworkIO.SetTxAttenData(0);
    Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes
}

UpdateAAudioMixerStates();
UpdateDDCs(rx2_enabled);

HdwMOXChanged(tx, freq);   // flip the hardware
Display.MOX = tx;
psform.Mox = tx;
cmaster.Mox = tx;          // loads router bit, among other things

Audio.RX1BlankDisplayTX = blank_rx1_on_vfob_tx;

if (radio.GetDSPTX(0).CurrentDSPMode != DSPMode.CWL &&
    radio.GetDSPTX(0).CurrentDSPMode != DSPMode.CWU) // turn on the transmitter unless in CW mode
{
    if (rf_delay > 0)
        Thread.Sleep(rf_delay);
    AudioMOXChanged(tx);    // set MOX in audio.cs - wait 'til here to allow last audio to clear AAMix before changing to MON volume
    WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);
}
else
    AudioMOXChanged(tx);    // set MOX in audio.cs
```

TX→RX path (lines ~29614-29661):

```csharp
else                        // change to RX mode
{
    if (space_mox_delay > 0)
        Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE

    _mox = tx;
    psform.Mox = tx;
    WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);  // turn off the transmitter (no action if it's already off)

    if (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
        radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)
    {
        if (!cw_fw_keyer && key_up_delay > 0)
            Thread.Sleep(key_up_delay);
    }
    else
    {
        if (mox_delay > 0)
            Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
    }
    UpdateDDCs(rx2_enabled);
    UpdateAAudioMixerStates();

    AudioMOXChanged(tx);    // set audio.cs to RX
    HdwMOXChanged(tx, freq);// flip the hardware
    Display.MOX = tx;
    cmaster.Mox = tx;       // loads router bit, among other things
    if (ptt_out_delay > 0)
        Thread.Sleep(ptt_out_delay);                 //wcp:  added 2018-12-24, time for HW to switch
    WDSP.SetChannelState(WDSP.id(0, 0), 1, 0);  // turn on appropriate receivers
    if (RX2Enabled)
        WDSP.SetChannelState(WDSP.id(2, 0), 1, 0);
    if (radio.GetDSPRX(0, 1).Active)
        WDSP.SetChannelState(WDSP.id(0, 1), 1, 0);

    Audio.RX1BlankDisplayTX = blank_rx1_on_vfob_tx;
    ...
}
```

Post-change delegate at the very end (~line 29677):

```csharp
if (bOldMox != tx) MoxChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, bOldMox, tx); // MW0LGE_21a
```

### 1.3 Constants and timer values

| Constant | Default | Source |
|---|---|---|
| `rf_delay` | 30 ms | RX→TX wait between hardware flip and TX channel on |
| `mox_delay` | 10 ms | TX→RX in-flight sample clear (SSB/FM only) |
| `space_mox_delay` | 0 ms | TX→RX delay before WDSP transmitter off |
| `key_up_delay` | 10 ms | TX→RX delay in CW mode (only when `!cw_fw_keyer`) |
| `ptt_out_delay` | 20 ms | TX→RX delay before WDSP receivers on (HW settle) |
| `txAtt = 31` | dB | Forced ATT when PS off or CW mode (non-HPSDR boards) |

### 1.4 State-machine transitions

**RX→TX (ordered):**
1. Pre-flight gate (Ganymede PA issue, RX-only flag).
2. `MoxPreChangeHandlers?.Invoke(...)` (pre-change delegate).
3. `NetworkIO.SendHighPriority(1)`.
4. VAC bypass adjust (if applicable per PTT mode).
5. FM offset apply (FM with non-Simplex repeater offset).
6. TX freq compute from VFO + XIT + CW pitch offset.
7. Out-of-band gate (`CheckValidTXFreq` with US 60m exception).
8. `_pause_DisplayThread = true`.
9. RX channel shutdown (non-full-duplex; conditional per
   `chkVFOATX/chkVFOBTX/RX2Enabled/mute_*`).
10. ATT-on-TX path (HPSDR vs non-HPSDR; CW or PS-off → 31 dB).
11. `UpdateAAudioMixerStates()` + `UpdateDDCs(rx2_enabled)`.
12. `HdwMOXChanged(tx=true, freq)` ← **first MOX wire bit**.
13. `Display.MOX = true`, `psform.Mox = true`, `cmaster.Mox = true`.
14. If non-CW mode: `Thread.Sleep(rf_delay)` then
    `AudioMOXChanged(true)` then
    `WDSP.SetChannelState(WDSP.id(1, 0), 1, 0)`.
    If CW mode: `AudioMOXChanged(true)` immediately (no sleep, no
    WDSP TX-on — CW uses firmware keyer).
15. `_pause_DisplayThread = false`.
16. `UIMOXChangedTrue()` (UI lock-down).
17. `MoxChangeHandlers?.Invoke(...)` (post-change delegate).

**TX→RX (ordered):**
1. `Thread.Sleep(space_mox_delay)` (default no-op).
2. `_mox = false`, `psform.Mox = false`.
3. `WDSP.SetChannelState(WDSP.id(1, 0), 0, 1)` (TX off + flush).
4. CW: `Thread.Sleep(key_up_delay)` if no FW keyer.
   Non-CW: `Thread.Sleep(mox_delay)`.
5. `UpdateDDCs` + `UpdateAAudioMixerStates`.
6. `AudioMOXChanged(false)`.
7. `HdwMOXChanged(tx=false, freq)` ← MOX wire bit clear.
8. `Display.MOX = false`, `cmaster.Mox = false`.
9. `Thread.Sleep(ptt_out_delay)` (HW settle).
10. `WDSP.SetChannelState(WDSP.id(0, 0), 1, 0)` (RX1 on); RX2 if
    enabled; sub-RX if active.
11. ATT/preamp restore.
12. Clear PA telemetry caches (`pa_fwd_power`, `pa_rev_power`,
    `HighSWR`).
13. `UIMOXChangedFalse()`.
14. `MoxChangeHandlers?.Invoke(...)`.

### 1.5 Side-effects to capture in MoxController

- Owns `_mox` flag (single source of truth).
- Owns `_current_ptt_mode` (PTTMode enum, 9 values — only `MANUAL`
  wired in 3M-1a; `_current_ptt_mode = PTTMode.NONE` on TX→RX).
- Owns `_manual_mox` flag (set true on TUN, false on TUN-off).
- Drives `Display.MOX`, `psform.Mox`, `cmaster.Mox` (mirrored elsewhere).
- Drives `AudioMOXChanged` boundary (audio thread sees MOX state).
- Drives `WDSP.SetChannelState` for TX channel on/off.
- Drives `HdwMOXChanged` for hardware register flip.

### 1.6 NereusSDR mapping

| Thetis | NereusSDR (3M-1a) |
|---|---|
| `chkMOX_CheckedChanged2` (event handler) | `MoxController::setMox(bool)` slot |
| `_mox` field | `MoxController::m_mox` |
| `_current_ptt_mode` field | `MoxController::m_pttMode` (new `PttMode` enum at `src/core/PttMode.h`; distinct from existing NereusSDR-native `PttSource` enum used by Diagnostics) |
| `_manual_mox` field | `MoxController::m_manualMox` |
| `MoxPreChangeHandlers`/`MoxChangeHandlers` | Phase signals (`txAboutToBegin` / `rxReady`) |
| `Thread.Sleep(...)` blocking calls | 6 `QTimer` chains driven by `MoxState` enum |
| `HdwMOXChanged(tx, freq)` call | `hardwareFlipped` phase signal — handlers in `RadioConnection` for wire bits, `AlexController`/`StepAttenuatorController` for routing |
| `Display.MOX = tx` | `SpectrumWidget::setMoxOverlay(bool)` |
| `WDSP.SetChannelState(WDSP.id(1, 0), 1/0, 0/1)` | `TxChannel::setRunning(bool)` |
| `UIMOXChangedTrue/False()` | `MoxController::txStateChanged(bool)` signal subscribers |

---

## 2. `HdwMOXChanged` — hardware-flip side of MOX

**Cite:** `console.cs:29033-29122 [v2.10.3.13]`.

### 2.1 Verbatim source

```csharp
private void HdwMOXChanged(bool tx, double freq)
{
    if (tx)
    {
        if (m_bQSOResetTimerOnMox) QSOTimerReset();
        if (m_bQSOTimerDuringMoxOnly && !m_bQSOTimerRunning) QSOTimerRunning = true;

        if (bpf2_gnd) NetworkIO.SetBPF2Gnd(1);

        updateVFOFreqs(tx);

        // make sure TX freq has been set

        UpdateRX1DDSFreq();
        UpdateRX2DDSFreq();
        UpdateTXDDSFreq();

        Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
        Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);

        if (penny_ext_ctrl_enabled) //MW0LGE_21k
        {
            int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
            if (!IsSetupFormNull) SetupForm.UpdateOCLedStrip(_mox, bits);
        }

        UpdateTRXAnt();
        if (rx1_xvtr_index >= 0)
        {
            Alex.getAlex().UpdateAlexAntSelection(lo_band, _mox, alex_ant_ctrl_enabled, true);
        }
        else
        {
            Alex.getAlex().UpdateAlexAntSelection(_tx_band, _mox, alex_ant_ctrl_enabled, false);
        }
        UpdateTRXAnt(); //[2.3.10.6]MW0LGE added

        NetworkIO.SetTRXrelay(1);
        if (cw_fw_keyer &&
            (RX1DSPMode == DSPMode.CWL || RX1DSPMode == DSPMode.CWU) &&
             !chkTUN.Checked &&
             _current_ptt_mode != PTTMode.SPACE &&
            _current_ptt_mode != PTTMode.CAT &&
            _current_ptt_mode != PTTMode.CW)
            NetworkIO.SetPttOut(0);
        else NetworkIO.SetPttOut(1);
        if (serialPTT != null) serialPTT.setDTR(true);
    }
    else // rx
    {
        if (m_bQSOTimerDuringMoxOnly && m_bQSOTimerRunning) QSOTimerRunning = false;

        NetworkIO.SetPttOut(0);
        NetworkIO.SetTRXrelay(0);

        if (serialPTT != null) serialPTT.setDTR(false);

        if (!_rx1_step_att_enabled)
            RX1PreampMode = rx1_preamp_mode;

        updateVFOFreqs(tx);

        UpdateRX1DDSFreq();
        UpdateRX2DDSFreq();
        UpdateTXDDSFreq();

        Band lo_band = Band.FIRST;
        Band lo_bandb = Band.FIRST;
        lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
        lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);

        if (penny_ext_ctrl_enabled) //MW0LGE_21k
        {
            int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
            if (!IsSetupFormNull) SetupForm.UpdateOCLedStrip(_mox, bits);
        }

        UpdateTRXAnt(); //[2.3.10.6]MW0LGE added
        if (rx1_xvtr_index >= 0)
        {
            Alex.getAlex().UpdateAlexAntSelection(lo_band, _mox, alex_ant_ctrl_enabled, true);
        }
        else
        {
            Alex.getAlex().UpdateAlexAntSelection(rx1_band, _mox, alex_ant_ctrl_enabled, false);
        }
        NetworkIO.SetBPF2Gnd(0);
        UpdateTRXAnt();
    }
}
```

### 2.2 Wire-bit value table

| Flag | RX value | TX value | NetworkIO call |
|---|---|---|---|
| MOX | 0 | 1 | (set on next outbound C&C frame; see §7) |
| BPF2Gnd | 0 | 1 | `NetworkIO.SetBPF2Gnd(int)` (only if `bpf2_gnd` is set) |
| TRXrelay | 0 | 1 | `NetworkIO.SetTRXrelay(int)` |
| PttOut | 0 | 0 or 1 | `NetworkIO.SetPttOut(int)` (gate per CW FW keyer logic below) |
| Serial DTR | false | true | `serialPTT.setDTR(bool)` |

**PttOut CW gate (TX path only):** if FW keyer is on AND mode is CWL/CWU
AND TUN is **not** active AND PTT mode is not SPACE/CAT/CW → `SetPttOut(0)`.
Otherwise `SetPttOut(1)`. In 3M-1a, TUN is the only path firing MOX, so
PttOut on TX is always **1** for TUN.

### 2.3 Side-effects ordered (RX→TX)

1. QSO timer reset (gated on `m_bQSOResetTimerOnMox`).
2. QSO timer start (gated on `m_bQSOTimerDuringMoxOnly`).
3. `NetworkIO.SetBPF2Gnd(1)` (gated on `bpf2_gnd`).
4. `updateVFOFreqs(tx=true)`.
5. RX1/RX2/TX DDS frequency updates.
6. Penny external PA control (`Penny.UpdateExtCtrl(...)` → OC LED strip).
7. `UpdateTRXAnt()` (Alex antenna routing precondition).
8. `Alex.UpdateAlexAntSelection(band, mox=true, alex_enabled, xvtr)`.
9. `UpdateTRXAnt()` again (intentional double-call per `[2.3.10.6]MW0LGE`).
10. `NetworkIO.SetTRXrelay(1)`.
11. `NetworkIO.SetPttOut(0|1)` (CW FW-keyer gate; always 1 for TUN).
12. `serialPTT.setDTR(true)`.

### 2.4 Side-effects ordered (TX→RX)

1. QSO timer stop (gated on `m_bQSOTimerDuringMoxOnly`).
2. `NetworkIO.SetPttOut(0)`.
3. `NetworkIO.SetTRXrelay(0)`.
4. `serialPTT.setDTR(false)`.
5. RX1 preamp restore (gated on `!_rx1_step_att_enabled`).
6. `updateVFOFreqs(tx=false)`.
7. RX1/RX2/TX DDS frequency updates.
8. LO band recompute.
9. Penny external PA control (RX context).
10. `UpdateTRXAnt()`.
11. `Alex.UpdateAlexAntSelection(band, mox=false, alex_enabled, xvtr)`.
12. `NetworkIO.SetBPF2Gnd(0)`.
13. `UpdateTRXAnt()` again.

### 2.5 NereusSDR mapping

`HdwMOXChanged` becomes the body of `MoxController::hardwareFlipped`'s
slot in `RadioModel`. The slot fans out to:

- `RadioConnection::setMox(true/false)` (P1 frame builder writes byte 3
  bit 0; P2 frame builder writes high-priority byte 4 bit 1 — see §7).
- `RadioConnection::setBpf2GndBit(true/false)` — **deferred for HL2**
  (firmware doesn't have BPF2; ANAN-only feature, P1 byte/bit position
  not yet found in open-source — see §7.4).
- `RadioConnection::setTrxRelay(true/false)` — P1 byte 6 bit 7
  (semantic: 1=disable, so RX state writes `1`); P2 routed via Saturn
  register C0=0x24 (deferred).
- `RadioConnection::setPttOutBit(true/false)` — **deferred** (rear-panel
  PTT-OUT relay; not yet found in deskhpsdr's wire path).
- `AlexController::applyAntennaForBand(band, isTx)` — already wired in
  3P-I-b; first time `isTx=true` is exercised in 3M-1a.
- `RadioModel::setQsoTimer*()` — out of 3M-1a scope (deferred to a later
  phase if required).

---

## 3. `chkTUN_CheckedChanged` — TUN button toggle handler

**Cite:** `console.cs:29978-30157 [v2.10.3.13]`.

### 3.1 Function signature

```csharp
private async void chkTUN_CheckedChanged(object sender, System.EventArgs e)
```

Note the `async void` — Thetis uses `await Task.Delay(...)` and
`await Task.Run(ATUTune)`. NereusSDR will translate the `Task.Delay`
sleeps to `QTimer` chains and the ATU async branch is deferred to 3M-6.

### 3.2 TUN-on path (verbatim)

```csharp
if (chkTUN.Checked)
{
    if (!PowerOn)
    {
        MessageBox.Show("Power must be on to turn on the Tune function.",
            "Power is off",
            MessageBoxButtons.OK,
            MessageBoxIcon.Hand, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST);
        chkTUN.Checked = false;
        return;
    }

    //MW0LGE_21a
    // stop twotone if currently running
    if (SetupForm.TestIMD || chk2TONE.Checked)
    {
        // remove the handler then re-add back after
        // as we need to get this to happen right now
        chk2TONE.CheckedChanged -= new System.EventHandler(chk2TONE_CheckedChanged);
        chk2TONE.Checked = false;
        chk2TONE_CheckedChanged(this, EventArgs.Empty); // do now
        chk2TONE.CheckedChanged += new System.EventHandler(chk2TONE_CheckedChanged);
        await Task.Delay(300);
    }
    //

    _tuning = true;                                                  // used for a few things
    chkTUN.BackColor = button_selected_color;

    old_meter_tx_mode_before_tune = current_meter_tx_mode;
    if (current_meter_tx_mode != tune_meter_tx_mode)                // switch meter mode to power
    {
        CurrentMeterTXMode = tune_meter_tx_mode;
        comboMeterTXMode_SelectedIndexChanged(this, EventArgs.Empty);
    }

    if (_tune_pulse_enabled)
    {
        _tune_pulse_on = true;
        SetupTunePulse();
    }
    else
    {
        _tune_pulse_on = false;
        switch (Audio.TXDSPMode)                                        // put tone in opposite sideband
        {
            case DSPMode.LSB:
            case DSPMode.CWL:
            case DSPMode.DIGL:
                radio.GetDSPTX(0).TXPostGenToneFreq = -cw_pitch;
                break;
            default:
                radio.GetDSPTX(0).TXPostGenToneFreq = +cw_pitch;
                break;
        }

        radio.GetDSPTX(0).TXPostGenMode = 0;
        radio.GetDSPTX(0).TXPostGenToneMag = MAX_TONE_MAG;
        radio.GetDSPTX(0).TXPostGenRun = 1;
    }

    // remember old power //MW0LGE_22b
    if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
        PreviousPWR = ptbPWR.Value;
    // set power
    int new_pwr = SetPowerUsingTargetDBM(out bool bUseConstrain, out double targetdBm, true, true, false);
    //
    if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
    {
        PWRSliderLimitEnabled = false;
        PWR = new_pwr;
    }
    //

    old_dsp_mode = radio.GetDSPTX(0).CurrentDSPMode;                // save current mode
    switch (old_dsp_mode)
    {
        case DSPMode.CWL:
            CWFWKeyer = false;
            Audio.CurrentAudioState1 = Audio.AudioState.DTTSP;      // for CAT, apparently
            Audio.TXDSPMode = DSPMode.LSB;                          // set a non-CW mode of the same sex
            radio.GetDSPTX(0).CurrentDSPMode = DSPMode.LSB;         // do that here too
            break;
        case DSPMode.CWU:
            CWFWKeyer = false;
            Audio.CurrentAudioState1 = Audio.AudioState.DTTSP;
            Audio.TXDSPMode = DSPMode.USB;
            radio.GetDSPTX(0).CurrentDSPMode = DSPMode.USB;
            break;
    }

    if (andromeda_cat_enabled && aries_cat_enabled &&
       (aries_ant1_enabled || aries_ant2_enabled || aries_ant3_enabled))
    {
        ATUTunetokenSource = new CancellationTokenSource();
        ATUTunetoken = ATUTunetokenSource.Token;
        await Task.Run(() => ATUTune(ATUTunetoken), ATUTunetoken);
    }

    chkMOX.Checked = true;

    await Task.Delay(100); // MW0LGE_21k8
    // go for it
    if (!_mox)
    {
        chkTUN.Checked = false;
        return;
    }
    // MW0LGE_21k8 moved below mox
    updateVFOFreqs(chkTUN.Checked, true);

    _current_ptt_mode = PTTMode.MANUAL;
    _manual_mox = true;

    NetworkIO.SetUserOut0(1);       // why this?? CHECK
    NetworkIO.SetUserOut2(1);

    if (apollopresent && apollo_tuner_enabled)
        NetworkIO.EnableApolloAutoTune(1);
}
```

### 3.3 TUN-off path (verbatim)

```csharp
else
{
    _tune_pulse_on = false;

    chkMOX.Checked = false;                                         // we're done
    await Task.Delay(100);

    radio.GetDSPTX(0).TXPostGenRun = 0;

    chkTUN.BackColor = SystemColors.Control;

    switch (old_dsp_mode)                                           // restore old mode if it was changed
    {
        case DSPMode.CWL:
        case DSPMode.CWU:
            radio.GetDSPTX(0).CurrentDSPMode = old_dsp_mode;
            Audio.TXDSPMode = old_dsp_mode;
            CWFWKeyer = true;
            break;
    }
    _tuning = false;

    updateVFOFreqs(chkTUN.Checked, true);

    if (apollopresent)
        NetworkIO.EnableApolloAutoTune(0);

    //MW0LGE_22b
    if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
    {
        PWRSliderLimitEnabled = true;
        PWR = PreviousPWR;
    }

    if (current_meter_tx_mode != old_meter_tx_mode_before_tune) //MW0LGE_21j
        CurrentMeterTXMode = old_meter_tx_mode_before_tune;

    NetworkIO.SetUserOut0(0);      // why this?? CHECK
    NetworkIO.SetUserOut2(0);

    _manual_mox = false;

    if (ATUTunetokenSource != null &&
        ATUTunetokenSource.IsCancellationRequested == false)
    {
        ATUTunetokenSource.Cancel();
    }
}

AndromedaIndicatorCheck(EIndicatorActions.eINTune, false, chkTUN.Checked);
if (AriesCATEnabled)
    SetAriesTuneState(chkTUN.Checked);              // G8NJJ tell ARIES that tune is active

setupTuneDriveSlider(); // MW0LGE_22b

if (oldTune != _tuning) TuneChangedHandlers?.Invoke(RX2Enabled && VFOBTX ? 2 : 1, oldTune, _tuning);
```

### 3.4 Constants and timing

| Constant | Default | Purpose |
|---|---|---|
| 300 ms (await) | hard-coded | 2-TONE settle delay before TUN engages |
| 100 ms (await) | hard-coded | MOX engage / disengage settle delay |
| `cw_pitch` | per-mode, typically 600 Hz | Tone frequency offset (sign-flipped for LSB-family) |
| `MAX_TONE_MAG` | `0.99999f` | Post-gen tone magnitude (`console.cs:29954 [v2.10.3.13]`; inline `// why not 1?  clipping?` per `[v2.10.3.13]` — preserve verbatim on port) |
| `TXPostGenMode = 0` | mode 0 | Sine/tone generator mode (per `wdsp/gen.c` `xgen` switch) |
| `tune_meter_tx_mode` | enum (e.g., POWER) | Forced meter mode during tune |

### 3.5 Critical observations vs master design

- **TUNE does NOT switch DSP mode to AM.** Master design Section 3 said
  "AM mode + carrier-only postgen" — incorrect. Thetis stays in the user's
  current mode (LSB/USB/AM/FM/etc.), with **CW→LSB/USB swap** only.
- **TUNE produces a tone at `±cw_pitch` Hz, not pure DC carrier.** The
  on-air signal is a single-sideband-suppressed-carrier tone in the user's
  current mode (or AM-modulated for AM modes, FM-modulated for FM).
  Operationally the tone sounds like a "CW dit held down".
- **The TUN path does NOT directly read `tunePower_by_band[]`** in
  v2.10.3.13. It calls `SetPowerUsingTargetDBM()` (constraint-based dBm
  target). The array is maintained for backward compatibility / config
  persistence (see §4) but the live tune-power computation is dBm-based.
- **MOX is engaged via `chkMOX.Checked = true`**, which fires
  `chkMOX_CheckedChanged2` (§1) — TUN does not bypass the MOX state
  machine, it drives it.

### 3.6 NereusSDR mapping

| Thetis | NereusSDR (3M-1a) |
|---|---|
| `chkTUN_CheckedChanged` (event handler) | `MoxController::setTune(bool)` slot |
| `_tuning` field | `MoxController::m_tuning` |
| `await Task.Delay(300)` | `QTimer` chained inside MoxController state machine |
| `await Task.Delay(100)` (MOX settle) | `QTimer` for moxDelay (10 ms in master design — to be reconciled with Thetis 100 ms; see §10) |
| 2-TONE pre-stop | Out of scope (3M-3a stage) — leave unimplemented for 3M-1a |
| Apollo ATU integration | Deferred to 3M-6 |
| Aries ATU integration | Deferred to 3M-6 |
| `TXPostGenMode = 0`, `TXPostGenToneMag = MAX_TONE_MAG`, `TXPostGenToneFreq = ±cw_pitch`, `TXPostGenRun = 1/0` | `TxChannel::setTuneTone(bool on, double freqHz, double magnitude)` |
| `SetPowerUsingTargetDBM` | `TransmitModel::setTunePower(int)` — for 3M-1a, simple slider→drive-level path; full dBm-target logic deferred to 3M-3a |
| `NetworkIO.SetUserOut0(1)` / `SetUserOut2(1)` | Out of 3M-1a scope (Penny output bits) — leave unimplemented; cite as `TODO [3M-3]` |

---

## 4. `tunePower_by_band` — per-band tune-power array

**Cites:**
- Declaration: `console.cs:12094 [v2.10.3.13]`
- Initialisation: `console.cs:1819-1820 [v2.10.3.13]`
- Save (persistence): `console.cs:3087-3091 [v2.10.3.13]`
- Restore (load): `console.cs:4904-4910 [v2.10.3.13]`

### 4.1 Verbatim source

```csharp
// Declaration (line 12094):
private int[] tunePower_by_band;

// Init (lines 1819-1820):
tunePower_by_band = new int[(int)Band.LAST];
for (int i = 0; i < (int)Band.LAST; i++) tunePower_by_band[i] = 50;

// Save (lines 3087-3091):
s = "tunePower_by_band/";
for (int i = 0; i < (int)Band.LAST; i++)
    s += tunePower_by_band[i].ToString() + "|";
s = s.Substring(0, s.Length - 1);
a.Add(s);

// Restore (lines 4904-4910):
case var nam when name.StartsWith("tunePower_by_band"):
    list = val.Split('|');
    if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE
    for (int i = 0; i < (int)Band.LAST; i++)
    {
        tunePower_by_band[i] = int.Parse(list[i]);
    }
    break;
```

### 4.2 Array properties

- Type: `int[]`.
- Size: `(int)Band.LAST`. Thetis's `Band` enum runs FIRST=-1, GEN=0,
  B160M=1, B80M=2, ... B6M=11, B2M=12, WWV=13, plus extended bands.
  Per master design 3G-8, NereusSDR's `Band` enum has 14 values
  (160m–6m + GEN + WWV + XVTR), distinct from Thetis's count. The array
  size in NereusSDR will be **14**, indexed by NereusSDR's `Band` enum
  ordinals.
- Default: `50` watts per band on first init.
- Persistence key: `"tunePower_by_band/"` followed by pipe-delimited
  integer list. Stored via `DB.SaveVarsDictionary("Console", ...)`
  (similar pattern to other Thetis options).

### 4.3 The TUN-path observation (important)

The TUN-on path does **not** call `tunePower_by_band[currentBand]`
directly in v2.10.3.13. It calls `SetPowerUsingTargetDBM(out _, out _,
true, true, false)`, which is a constraint-based / fixed power computation
based on `_tuneDrivePowerSource` enum (`FIXED` vs `DRIVE` vs other).

The array exists in v2.10.3.13 as **storage for legacy per-band tune-power
preferences**, but the live TX power during tune comes from the dBm-target
math.

**3M-1a translation choice:** since the master design specifies "per-band
tune_power[14] limit" and the NereusSDR UX is described as a per-band
slider, we will implement the per-band array path **directly** (slider →
`tunePower_by_band[band]` → drive-level via simple linear scale). The
full dBm-target logic + drive-source enum is deferred to 3M-3a alongside
the rest of the TX processing chain. The pre-code review for 3M-3a will
re-port `SetPowerUsingTargetDBM` then.

### 4.4 NereusSDR mapping

- `TransmitModel::m_tunePowerByBand[14]` (`std::array<int, 14>`).
- AppSettings persistence: per-MAC-scoped under
  `hardware/<mac>/tunePowerByBand/<band-index>` (one int per slot;
  no pipe-delimited string — NereusSDR's AppSettings uses scalar keys).
- `TransmitModel::setTunePowerForBand(Band, int)` setter +
  `tunePowerForBand(Band)` getter.
- `MoxController::setTune(true)` reads the current band's slot via
  `RadioModel::transmitModel().tunePowerForBand(currentBand())` and pushes
  the value to `RadioConnection::setDriveLevel(int)`.

---

## 5. `Display.MOX` paths — TX-mode color scheme & `TXAttenuatorOffset`

**Cites:**
- `Display.MOX` setter: `display.cs:1569-1593 [v2.10.3.13]`
- `TXAttenuatorOffset` property: `display.cs:1360-1364 [v2.10.3.13]`
- TX filter colours (3 properties): `display.cs:2288-2410 [v2.10.3.13]`
- `DrawTXFilter` flag: `display.cs:2480-2488 [v2.10.3.13]`

### 5.1 Verbatim source

`Display.MOX` setter:

```csharp
public static bool MOX
{
    get { return _mox; }
    set
    {
        lock (_objDX2Lock)
        {
            if (value != _old_mox)
            {
                bool rx1WillBeTx = value && (!_tx_on_vfob || (_tx_on_vfob && !_rx2_enabled));
                _RX1waterfallPreviousMinValue = getWaterfallCachedPreviousMinOrFloor(1, rx1WillBeTx);

                if (_rx2_enabled)
                {
                    bool rx2WillBeTx = value && (_tx_on_vfob && _rx2_enabled);
                    _RX2waterfallPreviousMinValue = getWaterfallCachedPreviousMinOrFloor(2, rx2WillBeTx);
                }

                PurgeBuffers();
                _old_mox = value;
            }
            _mox = value;
        }
    }
}
```

`TXAttenuatorOffset`:

```csharp
public static float TXAttenuatorOffset
{
    get { return tx_attenuator_offset; }
    set { tx_attenuator_offset = value; }
}
```

TX filter colours (defaults):

```csharp
private static Color tx_filter_color = Color.FromArgb(65, 255, 255, 255);
private static SolidBrush tx_filter_brush = new SolidBrush(tx_filter_color);

private static Color display_filter_tx_color = Color.Yellow;
private static Pen tx_filter_pen = new Pen(display_filter_tx_color, 2); // width 2 MW0LGE

private static Color tx_display_background_color = Color.Black;
```

### 5.2 What the setter actually does

The `Display.MOX` setter does **not** change colors. It:

1. Locks the DX2 (DirectX 2) render mutex.
2. If MOX state actually changed (`value != _old_mox`):
   - Determines which RX channel will be the TX-active channel (RX1 unless
     VFOB-TX with RX2 enabled).
   - Caches the waterfall floor for that channel before the buffer purge,
     so the dBm scale can be restored on TX→RX without a misleading jump.
   - Calls `PurgeBuffers()` to clear pending FFT data.
3. Updates `_mox`.

The TX-mode visual differences come from separate static properties
(`TXFilterColor`, `DisplayFilterTXColor`, `TXDisplayBackgroundColor`) plus
the `TXAttenuatorOffset` float that shifts the dBm reference during TX.
These are read by the spectrum painter at the painted-frame level.

### 5.3 No "W12 / W14 overlay" symbols found

The master design references "W12 TX filter overlay" and "W14 zero-line".
These names don't appear as identifiers in `display.cs`. They are likely
NereusSDR shorthand for the painter's Display widget regions
12 and 14 (some internal numbering in NereusSDR's own `SpectrumWidget`).
Master design wording correction: in 3M-1a the *behaviours* matter:
- TX filter outline drawn (gated on `DrawTXFilter` flag).
- Zero-line marker for TX rendered.
- Background colour switches to `TXDisplayBackgroundColor`.

### 5.4 NereusSDR mapping

- `SpectrumWidget::setMoxOverlay(bool)` slot — call site is the
  `MoxController::txStateChanged` boundary signal (per Codex P1 lesson:
  one signal, one emit).
- `SpectrumWidget::setTxAttenuatorOffsetDb(float)` slot — fed from
  `StepAttenuatorController` when ATT-on-TX kicks in (§6).
- Waterfall floor cache: NereusSDR already caches per-channel waterfall
  floors via `PanadapterModel`. The MOX-overlay slot needs to swap the
  waterfall reference to the TX-active channel's cached floor (matching
  Thetis's behaviour); this is a small extension to the existing cache
  read path.
- `SpectrumWidget::setTxFilterVisible(bool)` slot — gated on Thetis's
  `DrawTXFilter` flag (which is itself toggled from setup pages).
  3G-8 already wired the TX filter overlay stub; 3M-1a activates it.

### 5.5 Constants (verbatim)

| Property | Default | Notes |
|---|---|---|
| `tx_filter_color` | `Color.FromArgb(65, 255, 255, 255)` | Semi-transparent white (alpha 65/255 ≈ 25%) |
| `display_filter_tx_color` | `Color.Yellow` | TX filter line color |
| TX filter pen width | `2` | Per `//MW0LGE` inline comment |
| `tx_display_background_color` | `Color.Black` | TX-mode display background |
| `draw_tx_filter` | `false` | Default off; user-toggled in setup |

---

## 6. ATT-on-TX — `m_bATTonTX` + `_forceATTwhenPSAoff`

**Cites:**
- `m_bATTonTX` decl + property: `console.cs:19041-19067 [v2.10.3.13]`
- `_forceATTwhenPSAoff` decl + property: `console.cs:29285-29290 [v2.10.3.13]`
- TX-path application: `console.cs:29546-29576 [v2.10.3.13]` (inside §1)
- TX→RX path restoration: `console.cs:29637-29659 [v2.10.3.13]` (inside §1)

### 6.1 Verbatim source — fields and properties

```csharp
private bool m_bATTonTX = true;
public bool ATTOnTX
{
    get { return m_bATTonTX; }
    set
    {
        if (!value && _auto_attTX_when_not_in_ps) return; // ignore in this case
        m_bATTonTX = value;
        updateAttNudsCombos();

        if (PowerOn)
        {
            if (m_bATTonTX)
            {
                int txatt = getTXstepAttenuatorForBand(_tx_band);
                NetworkIO.SetTxAttenData(txatt); //[2.10.3.6]MW0LGE att_fixes
                Display.TXAttenuatorOffset = txatt; //[2.10.3.6]MW0LGE att_fixes
            }
            else
            {
                NetworkIO.SetTxAttenData(0);
                Display.TXAttenuatorOffset = 0;
            }
        }
    }
}

private bool _forceATTwhenPSAoff = true; //MW0LGE [2.9.0.7] added
public bool ForceATTwhenPSAoff
{
    get { return _forceATTwhenPSAoff; }
    set { _forceATTwhenPSAoff = value; }
}
```

### 6.2 TX-path application (RX→TX)

(Inside `chkMOX_CheckedChanged2`, lines 29546-29576.)

```csharp
if (m_bATTonTX)
{
    if (HardwareSpecific.Model == HPSDRModel.HPSDR)
    {
        temp_mode = RX1PreampMode;
        SetupForm.RX1EnableAtt = false;
        RX1PreampMode = PreampMode.HPSDR_OFF;            // set to -20dB
        if (_rx2_preamp_present)
        {
            temp_mode2 = RX2PreampMode;
            RX2PreampMode = PreampMode.HPSDR_OFF;
        }
    }
    else
    {
        //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
        int txAtt = getTXstepAttenuatorForBand(_tx_band);
        if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
                        (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
                         radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)) txAtt = 31; // reset when PS is OFF or in CW mode

        SetupForm.ATTOnRX1 = getRX1stepAttenuatorForBand(rx1_band); //[2.10.3.6]MW0LGE att_fixes
        SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value

        updateAttNudsCombos();
    }
}
else
{
    NetworkIO.SetTxAttenData(0);
    Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes
}
```

### 6.3 Force-31-dB trigger logic

```
txAtt = 31  ⟺  (!chkFWCATUBypass.Checked && _forceATTwhenPSAoff)
                                         OR
                  (CurrentDSPMode == CWL || CurrentDSPMode == CWU)
```

In plain English: force the attenuator to 31 dB (max) when **either**:
- PS is OFF (i.e., the firmware CAT/U bypass is *not* checked, AND
  `_forceATTwhenPSAoff` is true), OR
- The current TX DSP mode is CWL or CWU.

### 6.4 NereusSDR mapping

NereusSDR's `StepAttenuatorController` (3G-13) already exists and
implements both Classic and Adaptive auto-att modes. 3M-1a wires its
TX-path activation:

- `StepAttenuatorController::applyTxAttenuationForBand(band)` — already
  implemented (returns the per-band stored ATT value).
- `StepAttenuatorController::shouldForce31Db(dspMode, isPsOff)` — new
  predicate matching §6.3 logic.
- Slot subscriber on `MoxController::hardwareFlipped(bool isTx)`:
  if `isTx` → call applyTxAttenuationForBand + force-31 check; else →
  restore RX-band ATT.
- HPSDR-board variant (Atlas/Hermes-original) uses `PreampMode::HPSDR_OFF`
  via the existing preamp combo. The HPSDR branch is rare (Atlas only) and
  3M-1a will mirror Thetis's `temp_mode` save/restore pattern through
  `StepAttenuatorController::saveRxPreampMode()` /
  `restoreRxPreampMode()` helpers (new in 3M-1a).
- `Display.TXAttenuatorOffset` → `SpectrumWidget::setTxAttenuatorOffsetDb`.

### 6.5 Persistence

Both `m_bATTonTX` and `_forceATTwhenPSAoff` are bool toggles persisted in
Thetis's options dictionary. NereusSDR equivalents already exist as
AppSettings keys from 3G-13 / 3M-0 (`AttOnTxEnabled`,
`ForceAttWhenPsOff`). 3M-1a verifies the toggles take effect on the
TX path and on TX→RX restore.

---

## 7. Wire-format research findings

This section records the research-bounded port (master-design path A) for
the four `// TODO [3M-1a]` markers plus the new MOX-byte and TX-relay
findings. Sources searched:
`/Users/j.j.boyd/Thetis/`,
`/Users/j.j.boyd/mi0bot-Thetis/`,
`/Users/j.j.boyd/deskhpsdr/`,
`/Users/j.j.boyd/Hermes-Lite2/gateware/`.

### 7.1 P1 MOX bit — FOUND

**Wire location:** Outbound P1 Metis frame, byte 3 (C0), bit 0 (`0x01`).
**Semantic:** `1 = MOX on (transmit)`, `0 = MOX off (receive)`.

**Primary cite:** `deskhpsdr/src/old_protocol.c:3595-3599`:

```c
if (radio_is_transmitting()) {
    if (txmode == modeCWU || txmode == modeCWL) {
        if (tune || CAT_cw_is_active || MIDI_cw_is_active || !cw_keyer_internal
            || transmitter->twotone || radio_ptt) {
            output_buffer[C0] |= 0x01;  // MOX = byte 3, bit 0
        }
    } else {
        output_buffer[C0] |= 0x01;    // Always set MOX if non-CW transmitting
    }
}
```

**Cross-confirmation (HL2 firmware, receiving end):**
`Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:297`:

```verilog
CMDCTRL: begin
    ds_cmd_resprqst_next = eth_data[7];
    ds_cmd_addr_next = eth_data[6:1];
    ds_cmd_ptt_next = eth_data[0];      // PTT = bit 0 of CMDCTRL byte
    state_next = CMDDATA3;
end
```

So the firmware decodes MOX/PTT at byte CMDCTRL bit 0, and `old_protocol.c`
encodes it at C0 bit 0 — match.

**3M-1a translation:** in NereusSDR's `P1RadioConnection::sendCmdHighPriority`,
when `m_mox == true`, OR `0x01` into the C0 byte. When `m_mox == false`,
mask off `0x01`.

### 7.2 P1 Alex T/R relay (= Thetis `TRXrelay`) — FOUND

**Wire location:** Outbound P1 frame, byte 6 (C3), bit 7 (`0x80`).
**Semantic:** `1 = T/R relay disabled (PA protect)`, `0 = enabled`.

Note the inverted semantic vs. MOX. Thetis `NetworkIO.SetTRXrelay(1)`
writes "TRXrelay enabled" but the wire bit at C3 bit 7 is "disabled when
1". The mapping happens inside ChannelMaster.dll. We need the **Thetis-API
behavioural** semantic: when MOX goes to TX, Thetis calls
`SetTRXrelay(1)` — meaning "engage TX path through the relay". On the
wire, that writes `0` to bit 7 (let the relay engage).

**Primary cite:** `deskhpsdr/src/old_protocol.c:2909-2910`:

```c
if (txband->disablePA || !pa_enabled) {
    output_buffer[C3] |= 0x80;      // Disable Alex T/R relay (bit 7 = 0x80)
}
```

So `bit 7 = 1` only when PA is intentionally disabled. In normal TX,
the bit stays `0`.

**3M-1a translation:** when `m_mox == true` and PA is enabled, leave
C3 bit 7 = `0`. When PA is disabled (e.g. RX-only SKU, or user
disables-on-band), set C3 bit 7 = `1`. This logic lives in
`P1RadioConnection::composeCmdFrame` and is gated by the existing
`BoardCapabilities::isRxOnlySku` check + per-band PA-disable from 3M-0.

### 7.3 P1 PttOut — DEFERRED

`NetworkIO.SetPttOut(int)` in Thetis dispatches to ChannelMaster.dll.
deskhpsdr's `gpio.c:101-110` shows PttOut handled as a **GPIO line**, not
a wire bit in the Metis frame. This is a hardware-side relay (rear panel
PTT-OUT jack on the radio) rather than a cmd-frame bit.

**Decision for 3M-1a:** defer. The rear-panel PTT-OUT relay is used by
external equipment (linear amps, antenna switches with sequencing) and
is not required for the TUNE-into-dummy-load bench test. Mark
`P1RadioConnection::setPttOutBit` as `// TODO [3M-3]` with a tracking
issue link.

### 7.4 P1 BPF2Gnd — DEFERRED

`NetworkIO.SetBPF2Gnd(int)` is an Apache Labs ANAN-specific feature
(grounds the BPF2 unit during TX to protect a separate RX path from
backfeed). HL2 firmware has no BPF2 entity. deskhpsdr's `old_protocol.c`
does not implement it (deskhpsdr targets HL2 + HermesLite + ANAN; the
ANAN BPF2 path is presumably in ChannelMaster.dll's ANAN branch).

**Decision for 3M-1a:** defer. The bench-test target boards are HL2
(no BPF2) and ANAN-G2 (P2-only — uses Saturn registers, see §7.7).
For 3M-1a, `P1RadioConnection::setBpf2GndBit` is a no-op stub with
`// TODO [3M-1b/3M-3]` comment and a tracking issue link. Future
research can read mi0bot-Thetis more carefully for an explicit handler.

### 7.5 P1 Network watchdog enable bit — FOUND

**Wire location:** RUNSTOP packet, byte 1, bit 7.
**Semantic:** `1 = watchdog disabled`, `0 = watchdog enabled` (default
on).

**Primary cite (HL2 firmware, the receiver):**
`Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-204`:

```verilog
RUNSTOP: begin
    run_next = eth_data[0];
    wide_spectrum_next = eth_data[1];
    runstop_watchdog_valid = 1'b1;
end
```

`Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400`:

```verilog
end else if (runstop_watchdog_valid) begin
    watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
end
```

Thetis call-site reference: `setup.cs:17986 [v2.10.3.13]`:

```csharp
NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
```

So when `chkNetworkWDT.Checked == true` (network watchdog ENABLED in
the Thetis UI), Thetis passes `1` to ChannelMaster.dll. Inverted
semantic: ENABLED = bit 7 → `0` (watchdog NOT disabled).

**3M-1a translation:** in `P1RadioConnection::sendRunStop`, set RUNSTOP
byte 1 bit 7 to `0` when `m_watchdogEnabled == true`, and `1` when
`m_watchdogEnabled == false`. Resolves the
`// TODO [3M-1a]` at `P1RadioConnection.cpp:854`.

### 7.6 P2 MOX bit — FOUND

**Wire location:** P2 high-priority buffer (UDP port **1027**), byte 4,
bit 1 (`0x02`).
**Semantic:** `1 = MOX on`, `0 = off`.

**Primary cite:** `deskhpsdr/src/new_protocol.c:757, 761`:

```c
if (xmit) {
    if (txmode == modeCWU || txmode == modeCWL) {
        if (tune || CAT_cw_is_active || MIDI_cw_is_active || !cw_keyer_internal
            || transmitter->twotone || radio_ptt) {
            high_priority_buffer_to_radio[4] |= 0x02;  // MOX = byte 4, bit 1
        }
    } else {
        high_priority_buffer_to_radio[4] |= 0x02;    // Always set for non-CW TX
    }
}
```

**3M-1a translation:** in `P2RadioConnection::composeCmdHighPriority`,
when `m_mox == true`, OR `0x02` into byte 4 of the high-priority buffer.
The buffer is sent to UDP port 1027 (per deskhpsdr's
`HIGH_PRIORITY_FROM_HOST_PORT` constant).

### 7.7 P2 TX drive level — FOUND

**Wire location:** P2 high-priority buffer, byte 345.
**Range:** 0-255 (linear; 0 = off, 255 = full).

**Primary cite:** `deskhpsdr/src/new_protocol.c:872-876`:

```c
int power = 0;
if ((txfreq >= txband->frequencyMin && txfreq <= txband->frequencyMax)
    || tx_out_of_band_allowed) {
    power = transmitter->drive_level;
}
high_priority_buffer_to_radio[345] = power & 0xFF;
```

**3M-1a translation:** `P2RadioConnection::composeCmdHighPriority`
writes byte 345 = `clamp(driveLevel, 0, 255) & 0xFF` when MOX is true.
The drive level comes from `TransmitModel::tunePowerForBand(currentBand)`
during TUN, or from the RF-Power slider during voice TX (3M-1b).

### 7.8 P2 BPF2Gnd / Alex T/R / Network watchdog — DEFERRED to research

deskhpsdr handles these via Saturn register commands (C0=0x24 indirect
writes, see `saturndrivers.c` / `saturnregisters.c`). The detailed
register layout is outside the 3M-1a budget. For 3M-1a:

- P2 BPF2Gnd: deferred to 3M-3 (ANAN-G2 BPF2 protect path).
- P2 Alex T/R: handled implicitly by the radio firmware when MOX bit
  flips at byte 4 bit 1 — no separate command required (cf. HL2 firmware
  pattern). Verified by deskhpsdr source: `new_protocol.c` does not
  emit a separate T/R command for ordinary MOX.
- P2 Network watchdog: deferred. deskhpsdr does not currently emit a
  P2 watchdog command. Likely a Saturn-specific register; documented
  blocker.

**Decision for 3M-1a:** P2 watchdog wire bit stays a state-tracking stub
(matching the existing `// TODO [3M-1a]` at `P2RadioConnection.cpp:560`).
**Update** the TODO comment to reference this pre-code review §7.8 and
file a tracking issue. Resolves the path-A research outcome.

### 7.9 SwrProtectionController carry-forward TODOs — pure source-first ports

**TODO 1:** `alex_fwd > alex_fwd_limit` floor (5 W default; `2 ×
power-slider for ANAN-8000D`). Cite: `console.cs:26067 [v2.10.3.13]`.
The Thetis source already has this — no research needed. 3M-1a Task
F.3 ports it.

**TODO 2:** `tunePowerSliderValue ≤ 70` override on the tune-bypass
block. Cite: `console.cs:26020-26057 [v2.10.3.13]`. Same — pure port.
3M-1a Task F.3 ports it together with TODO 1 (one commit).

The cite ranges are already in the existing TODO comments in
`SwrProtectionController.cpp:220-225` from 3M-0; no rewriting needed,
just port the missing logic.

### 7.10 Wire-format summary table

| Surface | Protocol | Byte | Bit | Semantic | Status |
|---|---|---|---|---|---|
| MOX | P1 | 3 (C0) | 0 (`0x01`) | 1 = TX | **FOUND** (deskhpsdr + HL2 fw) |
| Alex T/R relay | P1 | 6 (C3) | 7 (`0x80`) | 1 = disabled | **FOUND** (deskhpsdr) |
| BPF2Gnd | P1 | — | — | — | **DEFERRED** to 3M-1b/3M-3 |
| PttOut | P1 | — | — | — | **DEFERRED** to 3M-3 (rear-panel relay) |
| Network watchdog | P1 | RUNSTOP byte 1 | 7 | 1 = disabled | **FOUND** (HL2 fw, resolves TODO) |
| MOX | P2 | high-pri 4 | 1 (`0x02`) | 1 = TX | **FOUND** (deskhpsdr) |
| TX drive level | P2 | high-pri 345 | (full byte 0-255) | linear | **FOUND** (deskhpsdr) |
| BPF2Gnd / T/R | P2 | (Saturn reg) | — | — | **DEFERRED** to 3M-3 (Saturn registers) |
| Network watchdog | P2 | — | — | — | **DEFERRED** with documented blocker (resolves TODO via stub-update path) |

### 7.11 TX I/Q sample format — P1 (16-bit) vs P2 (24-bit)

§7.10 covers the C&C wire bits. The TX I/Q **sample stream** has its own
wire format, which differs between protocols. This section records the
finding (caught during E.2 implementation) so future ports / reviewers
don't repeat the mistake of assuming a uniform sample width.

| Property | Protocol 1 | Protocol 2 |
|---|---|---|
| Bit width | **16-bit signed PCM** | **24-bit signed PCM** |
| Bytes per complex sample | 8 (`mic_L 2B + mic_R 2B + I 2B + Q 2B`) | 6 (`I 3B + Q 3B`; mic on a separate stream) |
| Endianness | Big-endian | Big-endian |
| Float→int gain | `32767.0` (`0x7FFF`) | `8388607.0` (`0x7FFFFF`) |
| UDP port | **1024** (Metis frame, EP2 zones) | **1029** (`TX_IQ_FROM_HOST_PORT`) |
| Frame size | 1032 bytes (Metis) | 1444 bytes (4-byte sequence + 1440-byte payload) |
| Samples per frame | 126 (2 × 63 in EP2 sub-zones) | 240 (1440 ÷ 6) |
| Source cite | `deskhpsdr/src/transmitter.c:1541` (`gain = 32767.0; // 16 bit`) + `deskhpsdr/src/old_protocol.c:2429-2458` (per-sample layout) [@HEAD-2026-04-25] | `deskhpsdr/src/new_protocol.c:1476` (`// should be 24: TX IQ sample width 24 bits`), `:1596` (`// there are 24 bits per sample`), `:2811-2816` (3-byte big-endian I + 3-byte big-endian Q packing), `new_protocol.h:37` (`#define TX_IQ_FROM_HOST_PORT 1029`) [@HEAD-2026-04-25] |

**Cross-confirmation (HL2 firmware):** the receiving end's FPGA decode
path is wired for 16-bit P1; no client (Thetis included) can deviate
without the radio rejecting the frames. Thetis's actual byte composition
lives in `ChannelMaster.dll` (closed source) but must produce the same
16-bit P1 / 24-bit P2 wire bytes per the OpenHPSDR Protocol 1 + 2 specs.

**Why this section exists:** the original dispatch prompt for E.2 told
the implementer "24-bit P1" — wrong, conflated with the older P1 spec
draft that had a 24-bit MIC variant which was never shipped on
HL2/ANAN-class hardware. The implementer caught it via source-first
reading of `deskhpsdr/src/transmitter.c` and pushed back. This is the
kind of correction the source-first protocol exists to surface.

---

## 8. WDSP TXA pipeline — 31 stages

**Cite:** `wdsp/TXA.c:31-479 [v2.10.3.13]`.

Master design Section 5.1.1 said "22 stages". The actual count from
`create_txa()` is **31 stages** (five speech-processing stages — PreEmph,
Leveler, LvlrMeter, CfComp, CfcMeter — live between EqMeter and Bp0 and
were missed in earlier counts). This is a master-design correction that
propagates to Section 2 (architecture) of the 3M-1a design.

### 8.1 Stage list with default Run state

Order matches the WDSP signal-flow path from `create_txa()`:

| # | Stage | Default Run | 3M-1a active? | Notes |
|---|---|---|---|---|
| 0 | `rsmpin` (input resampler) | OFF (turned on later if needed) | YES | Always on when TX channel runs |
| 1 | `gen0` (PreGen, mode=2 noise) | OFF | NO | 2-TONE path (3M-3a) |
| 2 | `panel` (audio panel/level) | ON | NO | Mic-gain control; 3M-1b |
| 3 | `phrot` (phase rotator) | OFF | NO | 3M-3a |
| 4 | `micmeter` (TXA_MIC_PK/AV) | ON | NO | Mic meter; 3M-1b |
| 5 | `amsq` (AM squelch) | OFF | NO | AM-only; 3M-3b |
| 6 | `eqp` (parametric EQ) | OFF | NO | 3M-3a EQ |
| 7 | `eqmeter` (TXA_EQ_PK/AV) | ON (gated on `eqp.run`) | NO | 3M-3a |
| 8 | `bp0` (mandatory BPF) | ON | YES | Always-on BPF before compression |
| 9 | `compressor` | OFF | NO | 3M-3a |
| 10 | `bp1` (post-comp BPF) | OFF | NO | 3M-3a |
| 11 | `osctrl` (output soft clip) | OFF | NO | 3M-3a |
| 12 | `bp2` (post-clip BPF) | OFF | NO | 3M-3a |
| 13 | `compmeter` | ON (gated on `compressor.run`) | NO | 3M-3a |
| 14 | `alc` (always-on AGC) | ON | YES | Always on |
| 15 | `ammod` (AM modulation) | OFF | NO | 3M-3b AM TX |
| 16 | `fmmod` (FM modulation) | OFF | NO | 3M-3b FM TX |
| 17 | `gen1` (PostGen, mode=0 tone) | OFF | YES (during TUNE) | TUNE carrier source |
| 18 | `uslew` (5 ms ramp) | always-on inside `xuslew` | YES | Ramps gen1 output |
| 19 | `alcmeter` (TXA_ALC_PK/AV/GAIN) | ON | YES | ALC telemetry |
| 20 | `sip1` (siphon for spectrum capture) | ON | YES | Powers the TX-side spectrum |
| 21 | `calcc` (calibration / IQ correct) | ON | NO | PureSignal (3M-4) |
| 22 | `iqc` (IQ correction) | OFF | NO | PureSignal (3M-4) |
| 23 | `cfir` (custom CIC FIR) | OFF (turned on if needed) | YES | Output rate match |
| 24 | `rsmpout` (output resampler) | OFF (turned on if needed) | YES | Resample to radio rate |
| 25 | `outmeter` (TXA_OUT_PK/AV) | ON | YES | Output telemetry |

**3M-1a active stages (10 of 31, not 9 as master design said):** rsmpin,
bp0, alc, gen1, uslew, sip1, cfir, rsmpout, alcmeter, outmeter. Master
design's "9 stages" missed alcmeter or sip1; the actual minimum for
audible-clean TUNE + ALC + spectrum capture is the 10 above.

### 8.2 Uslew ramp construction

`wdsp/slew.c:62-75 [v2.10.3.13]` — the ramp is **5 ms** with **0 ms
delay**:

```c
USLEW create_uslew (int channel, volatile long *ch_upslew, int size, double* in, double* out,
                     double rate, double tdelay, double tupslew)
{ ... }
```

Called from `TXA.c:369-377`:

```c
txa[channel].uslew.p = create_uslew (
    channel, &ch[channel].iob.ch_upslew,
    ch[channel].dsp_size,
    txa[channel].midbuff, txa[channel].midbuff,
    ch[channel].dsp_rate,
    0.000,          // tdelay = 0 ms
    0.005);         // tupslew = 5 ms
```

The ramp envelope is a raised cosine (`0.5 * (1 - cos(theta))`,
`wdsp/slew.c:90-106`). State machine: `BEGIN → WAIT (if delay > 0) → UP
(ramp) → ON`.

### 8.3 PostGen (gen1) configuration for TUNE

From §3 (chkTUN):
- `TXPostGenMode = 0` (tone/sine — per `wdsp/gen.c xgen` switch).
- `TXPostGenToneMag = MAX_TONE_MAG = 0.99999f` (`console.cs:29954 [v2.10.3.13]` — inline comment `// why not 1?  clipping?` to preserve verbatim).
- `TXPostGenToneFreq = ±cw_pitch` (sign per current DSP mode).
- `TXPostGenRun = 1` (run on); = 0 (run off, on TUN release).

### 8.4 TX channel construction parameters

`wdsp/cmaster.c:177-190 [v2.10.3.13]`:

```c
OpenChannel(
    chid (in_id, 0),                    // channel number
    pcm->xcm_insize[in_id],              // input buffer size
    4096,                                // dsp buffer size
    pcm->xcm_inrate[in_id],              // input sample rate
    96000,                               // dsp sample rate
    pcm->xmtr[i].ch_outrate,             // output sample rate
    1,                                   // channel type (1 = TX)
    0,                                   // initial state
    0.000,                                // tdelayup
    0.010,                                // tslewup
    0.000,                                // tdelaydown
    0.010,                                // tslewdown
    1);                                   // block until output is available
```

DSP rate = **96 kHz**. Type 1 = TX. Block-on-output = true. Slew
delays at the channel level are 10 ms (the `uslew` 5 ms in §8.2 is the
TXA-stage-level ramp; this is the channel-level state envelope).

### 8.5 NereusSDR mapping

- `WdspEngine::createTxChannel()` calls WDSP `OpenChannel(WDSP.id(1, 0),
  ...)` with the parameters above.
- `TxChannel` constructor calls `create_txa(channel)` and walks the 25-
  stage list above, leaving non-3M-1a stages with `Run=false`.
- `TxChannel::setTuneTone(bool on, double freqHz, double mag)` writes to
  gen1 (PostGen) via `SetTXAPostGenMode/Mag/Freq/Run`.
- The 5 ms uslew is hard-coded in WDSP (`tupslew=0.005`). NereusSDR does
  not need to expose it.

---

## 9. Alex routing — `UpdateAlexAntSelection` (TX branch first activation)

**Cite:** `Console/HPSDR/Alex.cs:310-413 [v2.10.3.13]`.

### 9.1 Signature

```csharp
public void UpdateAlexAntSelection(Band band, bool tx, bool alex_enabled, bool xvtr)
```

Master design called this `applyAlexAntennaForBand`. The actual Thetis
name is `UpdateAlexAntSelection`. NereusSDR's `AlexController` already
exposes a method with the master-design name — that's a NereusSDR-native
naming choice and stays as-is.

### 9.2 TX branch (verbatim, the half that's new in 3M-1a)

```csharp
if (tx)
{
    if (Ext2OutOnTx) rx_only_ant = 1;
    else if (Ext1OutOnTx) rx_only_ant = 2;
    else rx_only_ant = 0;

    rx_out = RxOutOnTx || Ext1OutOnTx || Ext2OutOnTx ? 1 : 0;
    trx_ant = TxAnt[idx];
}
// (RX branch already wired in 3P-I-b; included for completeness in §9.4)
```

Followed by:

```csharp
if (rx_out_override && rx_out == 1)
{
    if (!tx) trx_ant = 4;
    if (tx)
        rx_out = RxOutOnTx || Ext1OutOnTx || Ext2OutOnTx ? 1 : 0;
    else rx_out = 0;
}

if ((trx_ant != 4) && (LimitTXRXAntenna == true))
    trx_ant = 1;

if (m_nOld_rx_only_ant != rx_only_ant ||
    m_nOld_trx_ant != trx_ant ||
    m_nOld_tx_ant != tx_ant ||
    m_nOld_rx_out != rx_out ||
    m_bOld_tx != tx ||
    m_bOld_alex_enabled != alex_enabled)
{
    NetworkIO.SetAntBits(rx_only_ant, trx_ant, tx_ant, rx_out, tx);
    ...
}
```

### 9.3 TX-branch state computations

| Variable | TX value |
|---|---|
| `rx_only_ant` | 1 (Ext2OutOnTx), 2 (Ext1OutOnTx), 0 (otherwise) |
| `trx_ant` | `TxAnt[bandIndex]` |
| `tx_ant` | `TxAnt[bandIndex]` (passed redundantly) |
| `rx_out` | 1 if RxOutOnTx OR Ext1OutOnTx OR Ext2OutOnTx; 0 otherwise |

### 9.4 Mutual-exclusion invariants (already enforced in 3P-I-b)

The Ext1/Ext2/RxOutOnTx trio is already mutually-exclusive in NereusSDR's
`AlexController`. 3M-1a does not change that; it just **fires the TX
branch for the first time** by passing `isTx=true` from
`MoxController::hardwareFlipped`.

### 9.5 NereusSDR mapping

- `MoxController::hardwareFlipped(bool isTx)` →
  `RadioModel::onMoxHardwareFlipped(isTx)` →
  `AlexController::applyAntennaForBand(currentBand(), isTx)`.
- `AlexController::applyAntennaForBand` already implements the TX branch
  (3P-I-b). No new logic needed; just the new call from MOX.
- The 3P-I-b verification matrix entries that flagged "TX path activates
  in 3M-1a" become testable here for the first time.

---

## 10. Master-design corrections (cumulative)

Pre-code review uncovered the following corrections vs master design
Section 5.1:

| # | Master design said | Reality (Thetis source) | Fix in 3M-1a |
|---|---|---|---|
| 1 | TXA pipeline = 22 stages | 31 stages | Use 31-stage list (§8.1) |
| 2 | TUNE switches mode to AM + carrier-only postgen | Stays in current mode (CW→LSB/USB only); gen1 emits tone at ±cw_pitch | TxChannel doesn't change DSP mode; gen1 frequency = ±cw_pitch (§3.5) |
| 3 | "9 active stages" | 10 active stages (alcmeter + sip1 missed) | Activate 10 stages in 3M-1a (§8.1) |
| 4 | `tune_power_by_band[14]` | Sized `(int)Band.LAST` in Thetis (~23); NereusSDR has 14 bands so array stays 14 | NereusSDR-native sizing (§4.2) |
| 5 | TUN reads `tune_power_by_band[band]` | TUN actually calls `SetPowerUsingTargetDBM` (dBm-target); `tunePower_by_band[]` is legacy | NereusSDR ports the simpler per-band-array path; full dBm logic deferred to 3M-3a (§4.3) |
| 6 | `chkMOX_CheckedChanged2` at `console.cs:~28000-29000` | At `console.cs:29311-29678` | Cite range corrected (§1) |
| 7 | `chkTUN_CheckedChanged` at `console.cs:29978-30143` | At `console.cs:29978-30157` (master was slightly short) | Cite range corrected (§3) |
| 8 | "moxDelay 10 ms" timer between hardwareFlipped and txReady | Thetis uses `await Task.Delay(100)` (100 ms) for MOX engage settle in `chkTUN_CheckedChanged`; the 10 ms `mox_delay` is in `chkMOX_CheckedChanged2` for TX→RX SSB/FM in-flight sample clear | TUNE engagement wait = 100 ms; MOX state-machine `moxDelay` = 10 ms remains as the SSB/FM TX→RX in-flight sample clear delay (§1.3). |
| 9 | Alex routing function = `applyAlexAntennaForBand` | Thetis name = `UpdateAlexAntSelection`; NereusSDR already uses native naming `applyAntennaForBand` | Keep NereusSDR-native naming (§9.1) |
| 10 | "W12 / W14 overlay" | No such symbols in display.cs; these are NereusSDR painter region numbers | 3M-1a wires the *behaviours* (TX filter outline gated on `DrawTXFilter`, zero-line, background colour swap); painter region numbering is internal to NereusSDR (§5.3) |

These corrections feed forward into the TDD plan. The plan should NOT
match the master design verbatim where it disagrees with the source.

---

## 11. Implementation implications summary

For each TDD task in `phase3m-1a-tune-only-first-rf-plan.md`, the
relevant pre-code review section is the source-of-truth for behaviour:

| Task category | Cites this section |
|---|---|
| MoxController state machine + 6 timer chains | §1, §1.3 (constants), §1.5 (side-effects) |
| MoxController phase signals + idempotent-guard ordering | §1.4 (transition order — the "safety effect first, idempotent guard second" Codex P2 rule applies to step 12 of RX→TX where `HdwMOXChanged` runs before `_mox` is finalized) |
| PttMode enum (9 values, Thetis-verbatim, separate from existing PttSource) | §1.5 |
| TxChannel 31-stage pipeline + 10 active | §8 |
| TxChannel TUNE tone (gen1) + uslew | §3.5, §8.2-8.3 |
| TxChannel uslew 5 ms ramp (no audible click) | §8.2 |
| RadioConnection sendTxIq virtual base | §7.1, §7.6 |
| P1 MOX wire bit | §7.1 |
| P1 Alex T/R relay wire bit | §7.2 |
| P1 watchdog wire bit | §7.5 |
| P2 MOX wire bit + drive level | §7.6, §7.7 |
| P2 watchdog state-tracking stub | §7.8 |
| Alex applyAntennaForBand(isTx=true) wiring | §9 |
| ATT-on-TX (StepAttenuatorController dispatch) | §6 |
| SwrProtectionController 2 TODOs | §7.9 |
| TUN function port (chkTUN_CheckedChanged) | §3, §3.5 |
| Receive Only checkbox visibility wiring | not Thetis-derived (NereusSDR-native — see master design 5.1.4 carry-forward note) |
| TransmitModel MOX integration | §1.5 |
| RadioModel ownership wiring | (composition only) |
| SpectrumWidget Display.MOX overlay + TXAttenuatorOffset | §5 |
| MeterPoller TX bindings | (3M-0 already routed — just observe data flowing) |
| TxApplet TUN/Tune-Power/RF-Power/MOX | §3 (TUN button) + §1 (MOX button) + §4 (per-band tune power) |
| TransmitSetupPages Power & PA wiring | §4 (per-band tune power) + §6 (m_bATTonTX/_forceATTwhenPSAoff toggles) |
| MOX-without-TUN behaviour | §0.1 (Thetis allows it; NereusSDR matches) |

---

## 12. §2 — Post-code review

Appended after Phase H landed (commits `441dcbc..48fa8d6`), before the PR
opens. Per master design 5.1.3, the post-code review is mandatory.

### 12.1 Section-by-section impl-vs-spec deltas

**§0.1 — MOX-without-TUN behaviour:** Implementation matches. `MoxController::setMox(bool)` is callable directly without `setTune` first; the manual-MOX flag stays false in that path (only `setTune(true)` sets `m_manualMox`). RX-only SKU rejection happens at the SwrProtectionController + RadioModel layer, not inside MoxController. **No delta.**

**§1 — `chkMOX_CheckedChanged2`:** Ported as `MoxController::setMox(bool)` (B.2-B.4) with the 6-state machine + 6 QTimer chains + 6 phase signals. Codex P2 ordering preserved exactly: safety effects fire BEFORE the idempotent guard. **One intentional ordering deviation from Thetis** (documented inline in `MoxController.cpp:150-169`): the 100ms delay between `chkMOX.Checked = true` (Thetis line 30081) and the manual-MOX flag set (line 30093) is collapsed in NereusSDR — the flags are set BEFORE `setMox(true)` so that synchronous phase-signal subscribers see a consistent snapshot. Safe because NereusSDR's setMox is non-async.

**§2 — `HdwMOXChanged`:** Ported as `RadioModel::onMoxHardwareFlipped` slot (F.1) wired to `MoxController::hardwareFlipped` via `Qt::QueuedConnection` in G.1. Order preserved (Alex routing → setMox → setTrxRelay). **One Important issue caught in F.1 review and fixed:** the slot's outgoing calls into RadioConnection (`setMox`, `setTrxRelay`) were direct cross-thread calls until the F.1 fixup wrapped them in `QMetaObject::invokeMethod`. **No remaining delta.**

**§3 — `chkTUN_CheckedChanged`:** Ported as `RadioModel::setTune(bool)` orchestrator (G.4) + `MoxController::setTune(bool)` flag-side handler (B.5). All 3M-1a-relevant side effects covered: power-on guard (refusal signal), CW→LSB/USB swap, sign-select tune-tone freq, tune-power push, MOX engage/release, state save/restore. **Two scope deferrals as planned:** 2-TONE pre-stop (3M-3a), Apollo/Aries ATU (3M-6). **One ordering deviation caught in G.4 review and fixed:** `m_isTuning = true` was set after side effects; moved to BEFORE side effects to match Thetis line 30010. **One Important issue caught and fixed:** cold `setTune(false)` (without prior `setTune(true)`) was restoring stale `m_savedPowerPct` (default 50W) over the user's actual power; G.4 fixup added the `if (!m_isTuning) return;` idempotent guard.

**§4 — `tunePower_by_band`:** Ported as `TransmitModel::m_tunePowerByBand[14]` (G.3) with per-MAC AppSettings persistence. **Format delta from Thetis:** Thetis uses pipe-delimited string (`"50|50|...|50"`); NereusSDR uses scalar per-band keys (`hardware/<mac>/tunePowerByBand/<idx>`) matching the AlexController pattern. Default 50W per band preserved. `//[2.10.3.5]MW0LGE` length-mismatch-guard tag preserved as a comment per the GPL preservation rule (the scalar-key path has no equivalent line). **§4.3's note that Thetis doesn't directly read `tunePower_by_band[]` in v2.10.3.13** stands as documented — NereusSDR uses the array directly per the master-design choice (full `SetPowerUsingTargetDBM` deferred to 3M-3a).

**§5 — `Display.MOX` paths:** `SpectrumWidget::setMoxOverlay(bool)` slot wired in H.1 to `MoxController::moxStateChanged` via `Qt::QueuedConnection`. **Implementation provides the slot + state flag; visual rendering is a minimal indicator** (border tint / TX-mode flag) — the full `TXAttenuatorOffset` + TX filter overlay activations live as additional slots (`setTxAttenuatorOffsetDb`, `setTxFilterVisible`) added in the same commit. Waterfall floor cache swap (Thetis behavior at display.cs:1577-1593) is **deferred**; the slot is wired but the cache-swap side-effect is a 3M-3a polish item. No regression — the existing PanadapterModel cache continues to work.

**§6 — ATT-on-TX:** `StepAttenuatorController::onMoxHardwareFlipped` (F.2) implements the §6.3 force-31-dB predicate exactly: `(m_attOnTxEnabled && ((m_forceAttWhenPsOff && isPsOff) || (CWL || CWU)))`. HPSDR-board variant uses `saveRxPreampMode` / `restoreRxPreampMode` helpers as designed. **One Important issue caught in F.2 review and fixed:** `setIsHpsdrBoard()` had no production call site; MainWindow now wires it at connect time alongside `setMaxAttenuation`. UI side (H.4) wires the ATTOnTX + ForceATTwhenPSAoff checkboxes through the controller's setters.

**§7 — Wire-format research findings:** All 9 §7 entries match the implementation:

  | §  | Surface | Resolved by |
  |---|---|---|
  | 7.1 | P1 MOX (C0 byte 3 bit 0 / 0x01) | E.3 |
  | 7.2 | P1 T/R relay (bank 10 C3 bit 7, INVERTED) | E.4 + E.4 codec polarity propagation fixup |
  | 7.3 | P1 PttOut | DEFERRED to 3M-3 (per spec) |
  | 7.4 | P1 BPF2Gnd | DEFERRED to 3M-3 (per spec) |
  | 7.5 | P1 watchdog (RUNSTOP pkt[3] bit 7) | E.5 |
  | 7.6 | P2 MOX (high-pri byte 4 bit 1) | E.7 + latent `pttOut→m_mox` bug fix |
  | 7.7 | P2 drive level (high-pri byte 345) | E.7 + out-of-band gate inline |
  | 7.8 | P2 BPF2Gnd / Alex T/R / Watchdog (Saturn registers) | DEFERRED with tracking issue (E.8) |
  | 7.9 | SwrProtectionController carry-forward | F.3 |
  | 7.10 | Wire-format summary table | (matches) |
  | 7.11 | TX I/Q sample format (P1 16-bit / P2 24-bit) | E.2 (P1) + E.6 (P2) |

  **§7.11 was added DURING E.2** when the implementer caught the original "P1 = 24-bit" claim in the E.2 dispatch prompt as wrong via source-first reading of `deskhpsdr/src/transmitter.c`. This is the canonical example of source-first push-back the protocol exists to surface.

**§8 — WDSP TXA pipeline:** TxChannel constructed with `WDSP.id(1, 0)` (channel 1) and 31-stage pipeline introspection (C.2). Default Run states match Thetis (12 ON / 19 OFF). 3M-1a active stage list (rsmpin, bp0, alc, gen1, uslew, sip1, cfir, rsmpout, alcmeter, outmeter — 10 stages) confirmed via `tst_tx_channel_pipeline.cpp`. `setTuneTone` PostGen wiring (C.3) writes mode=0, freq=±cw_pitch (sign per current DSP mode), magnitude=`kMaxToneMag = 0.99999f` with `// why not 1?  clipping?` inline comment preserved verbatim per GPL rule. `setRunning` (C.4) calls `WDSP.SetChannelState(WDSP.id(1, 0), 1, 0)`. **No delta.**

**§9 — Alex routing TX branch:** First-time `isTx=true` activation of `AlexController::applyAntennaForBand` happens through F.1's `RadioModel::onMoxHardwareFlipped`. The 3P-I-b infrastructure is reused unchanged. **No delta.**

### 12.2 Codex P1 / P2 pattern audit

| Pattern | Locations | Status |
|---|---|---|
| **P1 — Subscribe at boundary signal, not individual setters** | F.1 / G.1: subscribers attach to `MoxController::hardwareFlipped` (boundary), not to individual `setMox`/`setMoxState` setters. Tested in `tst_mox_controller_phase_signals`. | ✅ Verified |
| **P2 — Safety effect fires BEFORE idempotent guard** | E.3 P1 setMox: `m_forceBank0Next = true` before `if (m_mox == enabled) return`. E.4 P1 setTrxRelay: `m_forceBank10Next = true` before guard. E.7 P2 setMox: pre-existing `if (m_mox == enabled) return;` (cadence covers idempotency, no flush flag needed). | ✅ Verified |
| **Codex inversion sub-pattern** (E.7 specifically) | E.7 setMox originally had a redundant pre-update `sendCmdHighPriority()` that emitted the OLD state before the transition. Caught in code-quality review and removed; the 100ms periodic cadence covers idempotency. | ✅ Fixed in `37aaaef` |
| **Cross-thread queued dispatch** | F.1, F.2, G.1, G.4: every cross-thread setter into `RadioConnection` (which lives on the connection thread) is wrapped in `QMetaObject::invokeMethod`. Established by F.1 fixup (`5fb42ca`); reused in F.2 fixup (`8726aba`) and G.4 (`d07b51c`). | ✅ Verified |

### 12.3 Source-first push-backs (lessons captured)

The source-first protocol caught **6 material issues** during 3M-1a — each prevented a wrong-on-the-wire bug from shipping:

1. **§7.11 — P1 = 16-bit, NOT 24-bit.** E.2 dispatch prompt incorrectly said "P1 24-bit"; implementer caught via `deskhpsdr/src/transmitter.c:1541` (`gain = 32767.0; // 16 bit`). Result: §7.11 added to pre-code review; both P1 (E.2) and P2 (E.6) implementations are wire-correct.

2. **E.4 codec-path polarity bug** — implemented in `composeCcForBankLegacy` only; the codec dispatch path (`P1CodecStandard::bank10`, `P1CodecHl2::bank10`) still wrote `ctx.paEnabled ? 0x80 : 0` — exactly INVERTED. Tests passed because the harness left `m_codec` null, falling through to legacy. Caught in code-quality review; fixed by propagating `trxRelay` through `CodecContext` and updating both codec implementations.

3. **E.4 latent bank-10 polarity from 3M-0** — line 1859 wrote `m_paEnabled ? 0x80 : 0` (INVERTED from deskhpsdr `output_buffer[C3] |= 0x80; // disable Alex T/R relay`). 3M-0 didn't surface it because no prior task fired live RF. E.4 was the first to exercise the bit; the bug was caught before merge.

4. **E.5 default `m_watchdogEnabled = false`** — 3M-0 oversight. Default-`false` would emit bit 7 = 1 (watchdog DISABLED) on first connect, meaning new users would be running with the watchdog OFF. E.5 changed to `true` to match Thetis "default on" behaviour.

5. **E.7 latent `pttOut → m_mox` bug** — `composeCmdHighPriorityLegacy` byte 4 bit 1 was driven by `m_tx[0].pttOut`, which `setMox()` never updates. `setMox(true)` updated `m_mox` but the wire byte stayed at 0. Caught during E.7 implementation by the implementer's pre-port grep; both legacy and codec paths fixed; `m_mox` field added; CodecContext `p2PttOut` populated from `m_mox`.

6. **F.1 / F.2 cross-thread direct calls** — slot fan-out called `m_connection->setMox(...)` and `m_connection->setTrxRelay(...)` directly from the main thread, while connection-state members are written by these methods on the connection thread. Plain-bool data race. Caught in F.1 code-quality review; fixed by wrapping both calls in `QMetaObject::invokeMethod`. Same pattern applied preventatively in F.2 (`setTxStepAttenuation` from StepAttCtrl).

### 12.4 Plan-text corrections folded in

The plan text (`phase3m-1a-tune-only-first-rf-plan.md`) was correct to the planning point but accumulated a few stale references during the pre-code phase:

- "TODO at line 854" → actual is line 886 (E.5 dispatch noted; corrected during E.5 fixup).
- "60 samples / 288-byte payload" for P2 → actual is 240 samples / 1440-byte payload per §7.11 (E.6 dispatch noted; §7.11 supersedes plan text).
- "applyTxAttenuationForBand already implemented" (pre-code §6.4) → actually NOT yet implemented; F.2 added it.

### 12.5 Verification matrix delta summary

3M-1a verification matrix extension landed in I.4 (commit `d48810a`). Highlights:

- **3M-0 row 5 (P1 watchdog wire bit) — RESOLVED in 3M-1a E.5**. Was a known stub in 3M-0; resolved to RUNSTOP pkt[3] bit 7 via HL2 firmware reading.
- **12 new automated rows (14-25)** added: each maps to one ctest target; all green at I.1 (168/168).
- **5 new HL2 bench rows (26-30)** + **5 new G2 bench rows (32-36)** added; rows 31 (refusal) and 35 (RX-only) are SKU-agnostic.
- **3M-0 rows 2, 3, 7-10** (`[3M-1a-bench]`-tagged) are now actionable on hardware — the safety net's PA Status / SWR / TX Inhibit overlays should observe their first live exercise.

### 12.6 Bench-test results

**HL2 BENCH PASS (I.2):** _Pending JJ's bench session._

**G2 BENCH PASS (I.3):** _Pending JJ's bench session._

This document will be updated post-bench with results + any wire-capture
log excerpts before the PR is merged.

### 12.7 Lessons learned + carry-forward to 3M-1b

1. **Source-first protocol works.** Six push-backs (§12.3) were prevented by direct source reads. Maintain the discipline in 3M-1b (PC mic input + voice TX) — the dispatch prompt's claim about codec routes / RTAudio / WASAPI buffer sizes will need the same skepticism.

2. **Codec paths are second-class to legacy paths in tests by default.** E.4 bug shipped (briefly) because tests didn't drive the codec path. Future tests on protocol-byte-level features must explicitly use `setBoardForTest(...)` to exercise the codec dispatch. Pattern established in E.4 fixup; reused in subsequent tasks.

3. **Cross-thread plain-bool members are landmines.** F.1 + F.2 + G.1 + G.4 all needed `invokeMethod` wrappers. For 3M-1b, any new connection-thread state should either be `std::atomic<...>` or routed exclusively through queued slots. Document the threading contract on each new field.

4. **Per-MAC AppSettings should mirror existing controllers' patterns.** AlexController + StepAttenuatorController + (now) TransmitModel all use `hardware/<mac>/<bucket>/<key>`. 3M-1b should reuse this for any new persisted state (mic gain per radio, etc.).

5. **§4.3's observation** — Thetis doesn't read `tunePower_by_band[]` directly in v2.10.3.13; the live computation is dBm-target-based via `SetPowerUsingTargetDBM`. NereusSDR's 3M-1a uses the array directly per master-design choice; 3M-3a will re-port the dBm-target logic and reconcile.

6. **The 100ms `await Task.Delay(100)` in chkMOX is collapsed in NereusSDR.** NereusSDR's setMox is synchronous; the delay isn't needed for state-machine consistency. If a future bench test surfaces a hardware settle issue (e.g., Alex relay needs ≥5ms before the wire-bit fires), the existing `MoxController::moxDelayTimer` (10ms) is the place to extend.

---

**End of pre-code review §2 (post-code).** Combined with §1 (pre-code, sections 0-11), this document is the complete source-first analysis for Phase 3M-1a TUNE-only First RF.
