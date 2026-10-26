# PureSignal ATT-on-TX gates: PS-A versus PSEnabled

Date: 2026-10-25. Status: the proposed fix below is applied on branch
`claude/puresignal-psa-att-gate`. "What NereusSDR does" describes the tree
before that fix; its line cites point at the fixed tree on that branch.

Thetis pin: v2.10.3.15 (`3759d09`). mi0bot-Thetis v2.10.3.13-beta2 has the
same two gates (`console.cs:30310`, `:47752`), so HL2 behaves the same.
NereusSDR line cites: branch `claude/puresignal-psa-att-gate` (base
`08a525895`), after the fix.

## Short answer

Both gates diverge from Thetis, but not quite in the way first suspected.

* **Key-down "force 31 dB when PS is off" rule.** Thetis asks "is the PS-A
  button on?". NereusSDR asks "is PSEnabled on?". These differ while a Single
  Cal is waiting or running, while a saved correction is being restored, and
  for a moment while PS turns off. In those states Thetis forces 31 dB at
  key-down and NereusSDR uses the band's stored TX ATT instead.
* The suspected case, "after a Single Cal (Stay-on) NereusSDR forces 31 where
  Thetis keeps the band value", **does not diverge**. Thetis's Single Cal
  button also turns PS-A off, so Thetis forces 31 in Stay-on too.
* **Power-change rule.** Thetis asks "is PS-A on?". NereusSDR asks "is calcc
  applying a correction right now?". These differ in two places: PS-A on with
  no correction yet (Thetis forces 31 on a power increase, NereusSDR does
  not), and Stay-on after Single Cal or Restore (NereusSDR forces 31, Thetis
  does not).

JJ runs Auto-Cal, where both gates agree with Thetis once calibration has
converged. Single Cal and Restore users see the difference.

## What Thetis does

### The two flags are different things

* `chkFWCATUBypass` is the PS-A button. Its handler sets
  `psform.AutoCalEnabled` from it (`console.cs:43782-43805`), and
  `ForcePureSignalAutoCalDisable()` unchecks it (`console.cs:43776-43779`).
* `PSForm.PSEnabled` (`PSForm.cs:235-269`) is the feedback fan-out:
  `UpdateDDCs`, `NetworkIO.SetPureSignal`, the router bit and `PSRunCal`. It
  touches no attenuator. The command state machine flips it
  (`PSForm.cs:632-726`):

| Command state | PSEnabled |
| --- | --- |
| OFF (`:632`) | false |
| TurnOnAutoCalibrate (`:644`) | true |
| TurnOnSingleCalibrate (`:658`) | true |
| SingleCalibrate (`:666`) | unchanged (true), waits here until a correction is applied, which needs a key-down |
| StayON (`:677`) | false |
| TurnOFF (`:701`) | true, until the engine reaches LRESET |
| IntiateRestoredCorrection (`:717`) | true, until the engine reaches LSTAYON |

* Single Cal (`btnPSCalibrate_Click`, `PSForm.cs:466-479`), Reset
  (`btnPSReset_Click`, `:486-491`) and Restore (`btnPSRestore_Click`,
  `:534-545`) all call `console.ForcePureSignalAutoCalDisable()` first. So
  PS-A is off for every path except Auto-Cal.
* The state machine runs whenever the radio is powered, keyed or not
  (`PSForm.cs:199`, `run = !_ps_closing && _power && ...`).

### Key-down rule

`console.cs:29612-29616` [v2.10.3.15]:

```csharp
//MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
int txAtt = getTXstepAttenuatorForBand(_tx_band);
if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
                (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
                 radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)) txAtt = 31; // reset when PS is OFF or in CW mode
```

`_forceATTwhenPSAoff` defaults to true (`console.cs:29336`).

During a Single Cal in Thetis, the key-down forces 31, then auto-attenuate
(`timer2code`, `PSForm.cs:728-790`, not gated on PS-A) walks the value down
mid-transmission, and the StayON retry loop (`PSForm.cs:688-699`, "fix for
when we were performing a single cal, but needed to change attenuation")
re-runs the cal. Starting from 31 is the intended behavior.

### Power-change rule

`console.cs:46819-46826` [v2.10.3.15]:

```csharp
//[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
if (new_pwr != _lastPower && chkFWCATUBypass.Checked && _forceATTwhenPowerChangesWhenPSAon)
{
    if(new_pwr > _lastPower || _forceATTwhenPowerChangesWhenPSAon_anddecreased)
        SetupForm.ATTOnTX = 31;

    _lastPower = new_pwr;
}
```

The gate is PS-A again, not PSEnabled and not "corrections applied".

## What NereusSDR does

* Key-down: `StepAttenuatorController::onMoxHardwareFlipped`
  (`src/core/StepAttenuatorController.cpp:1137-1151`) forces 31 when
  `!m_psActive`. `m_psActive` is fed from `PureSignal::psEnabledChanged`
  (`src/models/RadioModel.cpp:20679-20727`). The comment at
  `StepAttenuatorController.cpp:1041` already maps `!chkFWCATUBypass.Checked`
  to "PS-A not active", but the wire carries PSEnabled.
* That wire came from `e5114f22f` (2026-05-07, "Codex Fix C"), which moved
  three connections from `autoCalEnabledChanged` to `psEnabledChanged`. Two
  of them, `ReceiverManager::setPureSignalEnabled` and
  `RadioConnection::setPuresignalRun`, are correct: they mirror the
  `PSEnabled` setter body. The third, `setPsActive`, is not part of that
  setter in Thetis; it belongs to the PS-A button.
* Power change: `TransmitModel::setPowerUsingTargetDbm`
  (`src/models/TransmitModel.cpp:2050-2080`) gates on `pureSignalActive()`,
  which returns `PureSignal::correctionsBeingApplied()`
  (`TransmitModel.cpp:1587-1625`). Its comment calls this "the runtime
  equivalent" of PS-A. It is not.

## Where they disagree

Key-down rule, non-CW, "force ATT when PS-A off" on (the default):

| PS situation | PS-A | PSEnabled | Thetis at key-down | NereusSDR at key-down |
| --- | --- | --- | --- | --- |
| PS off | off | false | 31 | 31 |
| Auto-Cal running | on | true | band value | band value |
| Single Cal clicked, waiting or running | off | true | **31** | **band value** |
| Stay-on after Single Cal or Restore | off | false | 31 | 31 |
| Restoring a saved correction | off | true | **31** | **band value** |
| Turning off (until engine resets) | off | true | **31** | **band value** |
| PS-A just pressed, before the next 100 ms poll tick | on | false | **band value** | **31** |

Power-change rule, "force ATT when power changes and PS-A on" on (the
default):

| PS situation | Thetis | NereusSDR |
| --- | --- | --- |
| PS-A on, correcting | forces 31 | forces 31 |
| PS-A on, no correction yet (first key on a band, feedback out of range, after a reset) | **forces 31** | **no change** |
| PS-A off, Stay-on after Single Cal or Restore | **no change** | **forces 31** |
| PS-A off, no correction | no change | no change |

Because `_lastPower` only advances when the gate passes, the two programs also
end up comparing against different "last power" baselines afterward.

## What the user sees

* **Single Cal.** The usual flow is: click Single Cal, then key a two-tone.
  The state machine sits in SingleCalibrate until that key. Thetis keys at
  31 dB and lets auto-attenuate bring it down. NereusSDR keys at whatever TX
  ATT that band last stored. If that stored value is low (for example from a
  lower power setting), the feedback receiver gets a hot first sample. It
  also means the "force 31 when PS-A is off" safety option does nothing for
  the length of a Single Cal.
* **Restore.** Same as Single Cal for the key that completes the restore.
* **Power changes with Auto-Cal before it converges.** Thetis jumps TX ATT to
  31 when the drive goes up, so auto-attenuate starts again from the safe end.
  NereusSDR leaves TX ATT where it is. Auto-attenuate usually catches it on
  the next calibration attempt anyway (feedback out of range triggers its
  31.1 dB fallback), so this is mostly a one-cycle difference.
* **Power changes after a Single Cal.** NereusSDR writes 31 into the band's
  stored TX ATT; Thetis leaves it alone. With "force ATT when PS-A off" on,
  the next key forces 31 in both anyway, so this is mostly hidden. With that
  option off, NereusSDR's next key on that band uses 31 where Thetis uses the
  converged value.

None of this affects Auto-Cal once it has converged, which is JJ's normal
setup.

## Proposed fix (not applied)

1. **Key-down gate follows PS-A.** In `RadioModel.cpp:20679-20727`, connect
   `PureSignal::autoCalEnabledChanged` (not `psEnabledChanged`) to
   `StepAttenuatorController::setPsActive`, and make the initial push
   `setPsActive(m_pureSignal->isAutoCalEnabled())`. Leave the other two
   `psEnabledChanged` connections as they are. `autoCalEnabled` already
   tracks PS-A exactly: the applet and TX-applet PS-A buttons call
   `setAutoCalEnabled`, and `singleCalibrate`, `reset`, `restoreCorrections`
   and `setEnabled(false)` all call `forceAutoCalDisable()`, the port of
   `ForcePureSignalAutoCalDisable` (`src/core/PureSignal.cpp:546`, `:599`,
   `:680`, `:848`, port at `:688-696`). Cite `console.cs:29612-29616` and
   `console.cs:43782-43787` [v2.10.3.15].
2. **Power-change gate follows PS-A.** In `TransmitModel::pureSignalActive()`,
   return `m_pureSignal->isAutoCalEnabled()` instead of
   `correctionsBeingApplied()`, and replace the "runtime equivalent" comment
   with the `console.cs:46819-46826` [v2.10.3.15] cite. Both objects live on
   the main thread (PureSignal is never moved), so the plain-bool read is
   safe. The test seam stays as is.
3. **Tests.** In `tests/tst_puresignal_coordinator.cpp` or a RadioModel-level
   test: Single Cal pending leaves the controller's `psActive()` false, Auto-Cal
   on sets it true, Stay-on keeps it false. In
   `tests/tst_transmit_model_set_power_using_dbm.cpp`: with PS-A on and no
   correction, a power increase forces 31; in Stay-on with PS-A off, it does
   not.
4. **Comments.** Fix the Codex Fix C rationale at `RadioModel.cpp:20679-20716`.
   It says the PSEnabled wire was needed to "lift the 31 dB safety pin" for
   Single Cal. Thetis keeps the pin for Single Cal and relies on
   auto-attenuate to walk it down.

Risk: this changes TX attenuation during Single Cal and Restore, which is on
the PureSignal feedback path. It needs a bench Single Cal (JJ keys) to confirm
auto-attenuate walks the value down from 31 and the cal converges.
`autoAttentionTick` is not gated on PS-A, matching Thetis, so it should.

## Noticed in passing (not investigated)

When Thetis forces 31 at key-down it goes through `SetupForm.ATTOnTX` →
`TxAttenData`, which also stores 31 as that band's TX ATT
(`console.cs:10613-10625`, `setTXstepAttenuatorForBand`). NereusSDR sends 31
to the radio but leaves `m_txAttByBand` unchanged
(`StepAttenuatorController.cpp:1137-1201`). So after a forced key, Thetis shows
and keeps 31 for that band while NereusSDR keeps the earlier value. That is a
separate question from the gates and should be checked on its own.
