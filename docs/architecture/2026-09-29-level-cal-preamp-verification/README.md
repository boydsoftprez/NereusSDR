# Level Cal and Preamp Bench Verification Matrix

**Status:** `Pending bench`. Nothing here has been run on a radio. The
software side is covered by unit and byte-level wire tests; these rows
need a signal generator and each radio family on the bench.

**What changed:** the preamp combo now carries Thetis's ten preamp modes
with the per-board labels from `console.cs:28405-28450 [v2.10.3.15]`, and
each mode drives the step attenuator, the preamp bit and the Alex
attenuator as `console.cs:19230-19285 [v2.10.3.15]` does. Classic auto-att
steps through the step attenuator settings (`console.cs:21614
[v2.10.3.15]`). Each preamp setting keeps its own receive offset, and the
Level Cal Start button runs Thetis's CalibrateLevel on the Core
(`console.cs:9856-10232 [v2.10.3.15]`).

**Safety:** every row is receive-only. Do not key the radio for any row.
Feed the signal generator through a suitable attenuator; keep the level
at or below -20 dBm at the antenna jack.

| # | Row | Procedure | Pass criterion | Status |
|---|---|---|---|---|
| 1 | Preamp labels, HPSDR | Connect an Atlas/HPSDR, open the preamp combo | Items read as Thetis shows for HPSDR | Pending |
| 2 | Preamp labels, Hermes/Angelia/Orion | Connect each, open the preamp combo | Items read as Thetis shows for that board | Pending |
| 3 | Preamp labels, ANAN-G2 / G2E / 7000D / 8000D | Connect each, open the preamp combo | Items read as Thetis shows for the MKII class | Pending |
| 4 | Preamp labels, HL2 | Connect the HL2, open the preamp combo | Items read as mi0bot-Thetis shows for HL2 | Pending |
| 5 | Preamp drive | Carrier at -73 dBm on 14.100 MHz, step through every preamp item | Each step moves the S-meter by the item's dB within 1 dB; no item leaves the receiver deaf | Pending |
| 6 | Stored preamp choice after upgrade | Pick a preamp item on the old build, update, reconnect | The same item is selected and the S-meter reads as before | Pending |
| 7 | Classic auto-att | Enable Classic auto-att, raise the carrier until ADC overload | Preamp steps to the step attenuator settings, then back as the level falls | Pending |
| 8 | Level Cal, local | Carrier at -73 dBm on 14.100 MHz, Setup, Calibration, Level Cal, Start, answer Yes | Progress runs to 100, "Level Calibration complete." shows, S-meter reads -73 dBm within 1 dB with each preamp item | Pending |
| 9 | Level Cal, remote window | Same as row 8 from a remote window on the Core | Progress follows the Core's run; the result lands on the Core; the local window shows the same reading | Pending |
| 10 | Level Cal, cancel | Start a run, press Cancel part way | Status reads "Level calibration was canceled."; frequency, mode, preamp, step attenuator and buffer are back as before | Pending |
| 11 | Level Cal, no signal | Start with the generator off | The run stops with its reason and nothing changes | Pending |
| 12 | Grid follow | Turn on the grid's noise-floor follow, run row 8 | Follow is off during the run and back on after | Pending |
| 13 | Level Cal, phone | Start and cancel from the phone app | Same as rows 9 and 10 | Pending |
| 14 | Stored Off after upgrade, not Atlas | On a Hermes, Angelia, Orion MKII or G2, save "-20dB" (old Off) on the old build, update, reconnect, carrier at -73 dBm | The same "-20dB" item is selected, now SA -20: the step attenuator reads 20 dB, no preamp bit is sent, and the S-meter reads within 1 dB of before | Pending |
| 15 | RX2 preamp, two-ADC radio | ANAN-100D/200D/7000D/8000D or G2, slice B on the other ADC with its step attenuator off, carrier at -73 dBm; step slice B's preamp through 0, -10, -20 and -30 dB | The other ADC's attenuator reads 0, 10, 20 and 30 dB and slice B's S-meter moves with it; slice A does not move | Pending |
| 16 | RX2 preamp, HPSDR | Atlas with two receivers, step RX2's preamp between 0dB and -20dB | The second receiver's preamp bit follows; RX1 does not change | Pending |
| 17 | Level Cal, G2 with slice B | Row 8 on a G2 calibrating slice A while slice B sits on the other ADC | No preamp bit is sent during the run; the other ADC's attenuator reads 0 dB during the run and returns to its setting after | Pending |
| 18 | HPSDR key, step attenuator | Atlas with ATT on TX on and its step attenuator on: key into a dummy load, unkey | This row keys: only with a dummy load and the maintainer present. The step attenuator turns off at the key and stays off after the unkey, as Thetis does; the preamp mode comes back | Pending |
| 19 | Grid follow, quit mid-run | Turn on grid follow, start row 8, quit NereusSDR part way, relaunch | Grid follow is on after the relaunch | Pending |

## Boards whose wire output changed

Picking a preamp item now sends Thetis's step attenuator, preamp bit and
Alex attenuator values on: Atlas/HPSDR, Hermes, Hermes II, Angelia,
Orion, Orion MKII, Saturn / Saturn MKII (ANAN-G2), HermesC10 (ANAN-G2E),
Anvelina Pro 3, Red Pitaya, and Hermes Lite 2. Boards without the
HPSDR preamp relay no longer receive the preamp bit. Classic auto-att
drives the step attenuator settings of the preamp list.

**The stored-setting migration changes the drive.** A preamp choice stored
as Off (the old "-20dB" item) on any board but Atlas now loads as SA -20:
the same label and the same 20 dB, but sent as 20 dB of step attenuation
where the old build sent the preamp bit off. Thetis drives that item the
same way (`console.cs:19230-19285 [v2.10.3.15]`). Row 14 checks it.

**RX2's own preamp mode (fix wave).** RX2 now keeps Thetis's
`RX2PreampMode` (`console.cs:19413-19520 [v2.10.3.15]`). With RX2's step
attenuator off (the Thetis default) on the ANAN-100D, 200D, Orion MKII,
7000D, 8000D, G2, G2 1K, G2E, Anvelina Pro 3 and Red Pitaya, the other ADC
now carries the RX2 mode's attenuation (0, 10, 20 or 30 dB) where it
carried 0 before; on an HPSDR RX2's mode sends the second receiver's
preamp bit. The level calibration run drives RX2 through the same mode,
so a G2 never gets a preamp bit during a run. On an HPSDR, keying with
ATT on TX on now turns RX1's step attenuator off and leaves it off after
the unkey, as Thetis does (`console.cs:29599-29608 [v2.10.3.15]`). Rows 15
to 18 check these.

## Design notes: deliberate divergences from Thetis

These two differ from Thetis on purpose, and the review accepted them.

- **The calibrated slice counts as RX1 on any ADC.** Thetis's
  CalibrateLevel always calibrates RX1 on VFO A. NereusSDR calibrates the
  slice the operator picks (or the active slice), which stands in for RX1
  and VFO A even when it sits on the other ADC: the run saves, tunes and
  restores that slice, reads its meter, and stores the offsets as RX1's.
  RX2 is still driven through its own preamp mode during the run, as
  Thetis drives it.
- **Grid follow is one setting shared across pans.** Thetis keeps
  GridMinFollowsNFRX1 and GridMinFollowsNFRX2 apart and turns both off for
  the run. NereusSDR stores one `DisplayAdjustGridMinToNoiseFloor` for
  every pan, so the run holds that one setting off and puts it back. While
  it is held, every pan saves the user's value, so a quit or a crash mid-run
  never leaves it off.
