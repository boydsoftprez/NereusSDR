# PA Calibration Hotfix (#167) — Bench Verification Guide

Status: pre-tag verification for **v0.3.2**. Branch `hotfix/pa-calibration-167`, 18 commits, ctest 325/325, all verifiers green, end-of-epic reviewer verdict: **READY WITH CAVEATS** pending bench.

This guide is the on-the-bench checklist. Every step is sized for one operator with a dummy load, a watt meter, and the radio under test. Run sections in order. **Never bench without a dummy load.**

## Pre-flight: safety + setup

### Hardware checklist

- [ ] **50 ohm dummy load** rated at >= 200 W average, 500 W peak. No antenna for any of these tests.
- [ ] **External watt meter** in line between the radio and the dummy load. Bird, Diamond SX-200, or similar. Calibrated to within 5% on the bands you're testing.
- [ ] **DC supply** sized for the radio (8000DLE: ~25 A peak at 13.8 V; G2: ~22 A; HL2: ~5 A).
- [ ] **Coax** rated for the test power. RG-58 OK to 100 W; use RG-8X / LMR-240 above that.
- [ ] **Mic disconnected** during all PA gain tests. Two-tone is the only test that exercises audio path.

### Software checklist

- [ ] On `hotfix/pa-calibration-167`. Verify:
  ```
  cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/keen-feynman-c0d801
  git branch --show-current     # → hotfix/pa-calibration-167
  git log -1 --oneline          # → 378b6e3 chore: Phase 9 final integration ...
  git log --oneline origin/main..HEAD | wc -l   # → 18
  ```
- [ ] Clean build:
  ```
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
  cmake --build build -j$(sysctl -n hw.ncpu)
  ```
- [ ] Full ctest pass before bench:
  ```
  ctest --test-dir build --output-on-failure -j$(sysctl -n hw.ncpu)
  ```
  Expect **325/325**. Any new failure: STOP, do not bench.
- [ ] Pre-bench AppSettings backup:
  ```
  cp ~/.config/NereusSDR/NereusSDR.settings ~/.config/NereusSDR/NereusSDR.settings.pre-v0.3.2-bench
  ```
  Lets you roll back if anything mis-persists during the tests.

### Rollback plan

If anything looks wrong on the bench (wire byte too high, unexpected power, app crash):

1. **Disengage TUNE / MOX immediately** (spacebar or click the button off).
2. **Pull DC** if power readings are runaway (>20% above radio spec).
3. **Restore prior AppSettings**:
   ```
   cp ~/.config/NereusSDR/NereusSDR.settings.pre-v0.3.2-bench ~/.config/NereusSDR/NereusSDR.settings
   ```
4. **Switch back to `main`**:
   ```
   git switch main
   cmake --build build -j$(sysctl -n hw.ncpu)
   ./build/NereusSDR
   ```
5. **Log the failure** in this session with: radio model, band, slider position, observed wattage, expected wattage, screenshot if relevant.

---

## Test 1: K2GX regression on ANAN-G2 (or 8000DLE)

This is THE ship-blocking test. Pre-hotfix this exact configuration produced >300 W on a 200 W radio. Post-hotfix it must be at or below the radio's spec'd max.

### Setup

- Radio: ANAN-G2 (or ANAN-8000DLE if accessible).
- Dummy load connected.
- Watt meter in line.
- Mic disconnected.
- Antenna routing: ANT1 (or whatever is wired to dummy load).

### Steps

1. **Launch the build:**
   ```
   ./build/NereusSDR
   ```
2. **Connect the radio** via the connection panel. Wait for a green "Connected" indicator.
3. **Verify factory profile loaded.** Open Setup → PA → PA Gain. The active profile combo should show:
   - ANAN-G2: `Default - ANAN_G2`
   - 8000DLE: `Default - ANAN8000D`
4. **Verify the 80m gain entry.** In the gain spinbox row for `80m`:
   - ANAN-G2: should read **50.5** (factory)
   - 8000DLE: should read **50.5** (factory)
5. **Set band to 80m.** VFO to 3.750 MHz.
6. **Set drive slider to 50%.** Read the value field; it should show 50 W or similar.
7. **Engage TUNE** (TUN button on TxApplet, or via menu).
8. **Read the watt meter immediately.** Expected:

   | Radio | Slider | Pre-hotfix W | Post-hotfix W | Pre-hotfix wire | Post-hotfix wire |
   |---|---|---|---|---|---|
   | ANAN-G2 | 50% | ~300+ | ~70-90 | 127 | ~49 |
   | ANAN-8000DLE | 50% | ~300+ | ~70-90 | 127 | ~49 |

   The exact wattage depends on the in-line watt meter calibration and the specific PA gain (factory is 50.5 dB; your radio may be 50.0 to 51.0 dB depending on temperature, supply voltage, ATU bypass state).

9. **Disengage TUNE.**
10. **Record observed wattage** in this session log:
    - Radio: ____
    - 80m TUN slider 50% observed: _____ W
    - In acceptable range (< 200 W average, ideally 60-100 W)? Y / N

### Pass criteria

- Observed power on 80m TUN at slider 50% is **at or below 200 W** (the radio's spec'd max).
- Ideally observed is in the 60-100 W range (consistent with the math: target_dbm = 50 - 50.5 = -0.5; volts = 0.211; audio_volume = 0.264; wire = 49 → ~0.18 of full PA capability → ~36% of 200 W = ~72 W).
- No app crash. No watchdog kick. No protocol disconnect.

### Fail criteria (any one => STOP)

- Observed power exceeds 200 W on slider 50%. The math fix did not apply.
- Observed power is essentially zero (< 1 W). The math is over-attenuating.
- App crashes during TUNE engagement.

If pass: continue to Test 2. If fail: invoke rollback plan, capture logs, file a bug.

---

## Test 2: HL2 regression (no auto-cal yet)

The HL2 sentinel is the most important deviation in this hotfix. HL2 factory PA gain on HF is 100.0 (sentinel for "no compensation"). The math kernel short-circuits to linear scaling so HL2 transmit on HF behaves as it did pre-v0.3.2.

### Setup

- Radio: Hermes Lite 2.
- Dummy load connected.
- Watt meter in line.
- Mic disconnected.

### Steps

1. **Connect HL2** via the connection panel.
2. **Verify factory profile loaded.** Setup → PA → PA Gain. Active should be `Default - HERMESLITE`.
3. **Verify the gain table.** All HF rows (160m through 10m) should show **100.0**. The 6m row should show **38.8**. The XVTR / GEN / WWV rows should show 100.0 (sentinel).
4. **Set band to 80m.** VFO to 3.750 MHz.
5. **Set drive slider to 50%.**
6. **Engage TUNE.**
7. **Read watt meter:**

   | Test | Slider | Expected (HL2) | Why |
   |---|---|---|---|
   | 80m, slider 50% | 50% | ~2.5 W | Linear fallback: audio_volume = 0.5, wire byte ~127, HL2 PA outputs ~5 W max so ~2.5 W at 50% |
   | 80m, slider 100% | 100% | ~5 W (HL2 max) | Linear: audio_volume = 1.0, wire byte 255 |

   The exact wattage depends on HL2's PA gain and load. HL2 typically maxes at 5 W on HF.

8. **Switch to 6m.** VFO to 50.150 MHz.
9. **Set drive slider to 50%.**
10. **Engage TUNE.**
11. **Read watt meter:**

    | Test | Slider | Expected (HL2 6m) | Why |
    |---|---|---|---|
    | 6m, slider 50% | 50% | ~5 W (clamp) | dBm math kicks in: gbb = 38.8, target = 50 W, audio_volume clamps to 1.0, output ~5 W |

    On HL2 6m the math drives audio_volume to 1.0 even at slider 50% — HL2 PA on 6m is much smaller than the audio rail can drive. This is correct behavior (preserved from v0.3.1).

12. **Compare to pre-hotfix.** Switch back to `main` briefly:
    ```
    git stash    # save any pending edits
    git switch main
    cmake --build build -j$(sysctl -n hw.ncpu)
    ./build/NereusSDR
    ```
    Repeat steps 4-11 on `main`. The wattage should match (linear scaling on HF, dBm math falls through).

13. **Switch back to hotfix:**
    ```
    git switch hotfix/pa-calibration-167
    cmake --build build -j$(sysctl -n hw.ncpu)
    ```

### Pass criteria

- HL2 80m slider 50% wattage matches v0.3.1 baseline (within 10%).
- HL2 6m slider 50% wattage matches v0.3.1 baseline (within 10%).
- No regression on any HL2 HF band.

### Fail criteria

- HL2 80m output is essentially zero (< 0.5 W) on slider 50%. The sentinel short-circuit is not firing — the math is feeding 100.0 dB into the kernel and outputting near-zero audio.
- HL2 wattage on HF differs from v0.3.1 by more than 20%.

If pass: continue to Test 3. If fail: invoke rollback plan; this is the most likely deviation to bite us.

---

## Test 3: Setup → PA visual smoke per SKU

Verify the PA Setup pages render correctly for each connected radio.

### Steps for each radio you have

1. Connect the radio.
2. Open Setup → PA.
3. **PA Gain page checklist:**
   - [ ] Profile combo populated (at least `Default - <model>` and `Bypass`).
   - [ ] 14 band rows visible with band labels.
   - [ ] Gain spinbox column populated with non-zero values for HF bands (or 100.0 for HL2).
   - [ ] 9 drive-step adjust spinboxes per band, all 0.0 default.
   - [ ] Per-band max-power spinbox + Use Max checkbox visible.
   - [ ] 4 buttons present: New / Copy / Delete / Reset Defaults.
   - [ ] Warning icon + label hidden when active profile matches factory.
   - [ ] chkPANewCal "New Cal" checkbox present.
   - [ ] chkAutoPACalibrate "Auto Calibrate" checkbox present.
4. **Watt Meter page checklist:**
   - [ ] PaCalibrationGroup present (per-board PA-cal spinbox set).
   - [ ] "Show PA Values page" checkbox present, default checked.
   - [ ] "Reset PA Values" button present.
5. **PA Values page checklist (under "Show PA Values page" toggle):**
   - [ ] FWD calibrated label, REV power label, SWR label.
   - [ ] PA current, PA temperature, supply volts labels.
   - [ ] FWD ADC raw, REV ADC raw, ADC overload labels.
   - [ ] Raw FWD power label (NEW in 5B).
   - [ ] Drive power label (NEW in 5B).
   - [ ] FWD voltage label (NEW in 5B).
   - [ ] REV voltage label (NEW in 5B).
   - [ ] Reset button (NEW in 5B).
6. **Per-SKU visibility check:**
   - HL2: PA Gain page should NOT show a Ganymede warning.
   - Andromeda (if available): PA Gain page SHOULD show "Ganymede 500W follow-up" informational warning.
   - HL2 RX-only kit (if available): PA category should be HIDDEN entirely from Setup navigation.
   - Any radio with `hasPureSignal=true` (PureSignal-capable): PA-Profile / TX-Profile recovery linkage warning visible.

### Steps for cross-page reset wiring

1. On Setup → PA → Watt Meter, click "Reset PA Values".
2. Switch to PA Values page.
3. Confirm peak/min annotations on all 6 tracked metrics (FWD calibrated, REV, SWR, PA current, PA temperature, supply volts) are reset to current value (peak == min on display).

### Pass criteria

- All controls render correctly on all SKUs you tested.
- No empty / blank pages.
- No crashes when toggling the chkPAValues checkbox or clicking Reset PA Values.
- Cross-page wiring fires (Watt Meter Reset → PA Values peak/min cleared).

### Fail criteria

- Missing controls (e.g., one of the 4 new labels on PA Values page).
- Cross-page reset doesn't propagate.
- App crashes when navigating between pages.

---

## Test 4: Two-tone smoke (optional)

The two-tone test path now routes through the new math kernel. Smoke-verify it doesn't break.

### Setup

- Same as Test 1 (ANAN-G2 / 8000DLE on dummy load).

### Steps

1. Connect radio.
2. Open Setup → Two-Tone (or wherever the controller is exposed).
3. Set drive-source = DriveSlider. Set 2-tone power = 30 W.
4. Set band 80m. Drive slider to 30%.
5. Engage 2-tone test (start).
6. Read watt meter. Expected:
   - With 30 W slider on 80m / 8000DLE / 50.5 dB gain: target_dbm = 10*log10(30000) - 50.5 = 44.77 - 50.5 = -5.73; volts = sqrt(10^-0.573 * 0.05) = sqrt(0.0134) = 0.116; volume = 0.145; wire = 38; output ~12 W average per tone (24 W combined PEP).
7. Disengage 2-tone.
8. Repeat with drive-source = Fixed, 2-tone power = 17 W. Verify output drops accordingly.

### Pass criteria

- 2-tone produces a clean two-tone output on the spectrum analyzer (or audio-monitored watt meter).
- Drive-source switching produces the expected wattage differences.
- No app crash on 2-tone start/stop.

### Fail criteria

- 2-tone produces excessive power (>= 200 W on 8000DLE).
- 2-tone produces zero output.
- Crash on drive-source enum change mid-test.

---

## Test 5: ADC overload check (optional, but recommended for HL2)

The hotfix did not change ADC overload semantics, but the PA Values page now shows the ADC overload state. Smoke-verify it still fires.

### Steps

1. Connect HL2.
2. With dummy load + low-power signal source, push enough RF into the antenna port to trigger ADC OVF.
3. PA Values page should show "ADC Overload: Yes (ADC X)" within 100 ms.
4. After 2 seconds without overload, label should auto-clear to "No".

### Pass criteria

- Overload fires + auto-clears.

### Fail criteria

- Overload state never fires (UI bug).
- Overload state never clears (timer bug).

---

## Final pre-tag checklist

After all tests pass:

- [ ] All 5 tests above passed on at least one radio.
- [ ] K2GX regression test passed (Test 1) on the highest-gain radio you have access to.
- [ ] HL2 regression test passed (Test 2) if HL2 is available.
- [ ] No new crashes observed during any test.
- [ ] Watt meter readings recorded in the session log.
- [ ] Pre-bench AppSettings backup is intact (in case post-tag rollback is needed).

If all checked: **ready to push branch and open PR**. After PR review and merge, run `/release patch` to tag v0.3.2.

If any unchecked or any test failed: file a bug with the exact configuration, observed values, and which test failed. Do NOT tag v0.3.2.

---

## Bench session log template

Copy this into your bench session notes:

```
## v0.3.2 Bench Verification — <DATE> — <TESTER>

### Hardware
- Radio: <model> SN <serial>
- Dummy load: <make/model>, rating
- Watt meter: <make/model>, last calibrated <date>
- DC supply: <volts> V at <max amps> A
- Build: NereusSDR <git short hash>, hotfix/pa-calibration-167

### Test 1 — K2GX regression (80m TUN @ slider 50%)
- Observed wattage: ____ W
- Pass / Fail: ___
- Notes:

### Test 2 — HL2 regression (if available)
- 80m slider 50% observed: ____ W (expected ~2.5 W)
- 6m slider 50% observed: ____ W (expected ~5 W clamp)
- Compared against main baseline: matched / drifted / not tested
- Pass / Fail: ___
- Notes:

### Test 3 — Setup → PA visual smoke
- PA Gain page: PASS / FAIL on each SKU tested
- Watt Meter page: PASS / FAIL
- PA Values page (4 new labels + reset): PASS / FAIL
- Per-SKU visibility (HL2 / Andromeda / RX-only / PS): PASS / FAIL
- Cross-page reset wiring: PASS / FAIL
- Notes:

### Test 4 — Two-tone smoke
- DriveSlider 30% on 8000DLE 80m: ____ W (expected ~24 W PEP)
- Fixed mode 17 W: ____ W
- Pass / Fail: ___
- Notes:

### Test 5 — ADC overload smoke
- Overload fires: Y / N
- Auto-clears after 2s: Y / N
- Pass / Fail: ___

### Verdict
- Ready to tag v0.3.2: Y / N
- Blocking issues:
- Follow-up issues to file:
```

---

## Common observed wattage hand-computations

For your reference during the bench, here are the math-kernel-predicted values at key (model, band, slider) combinations:

| Model | Band | gbb (dB) | Slider | audio_volume | wire byte | Output (% of max) |
|---|---|---|---|---|---|---|
| ANAN-100 | 80m | 50.5 | 50 W | 0.187 | 49 | ~37% |
| ANAN-100 | 80m | 50.5 | 100 W | 0.264 | 68 | ~52% |
| ANAN-200D | 80m | 50.5 | 100 W | 0.264 | 68 | ~52% |
| ANAN-200D | 80m | 50.5 | 200 W | 0.373 | 97 | ~74% |
| ANAN-8000DLE | 80m | 50.5 | 100 W | 0.264 | 68 | ~52% |
| ANAN-8000DLE | 80m | 50.5 | 200 W | 0.373 | 97 | ~74% |
| ANAN-8000DLE | 10m | 42.0 | 200 W | 0.992 | 257→255 | ~99% (tightest cell) |
| ANAN-G2 | 20m | 50.9 | 100 W | 0.252 | 65 | ~50% |
| HL2 | 80m | 100 (sentinel) | 50 W | 0.500 (linear) | 127 | ~50% |
| HL2 | 80m | 100 (sentinel) | 100 W | 1.000 (linear) | 255 | 100% |
| HL2 | 6m | 38.8 | 100 W | 1.000 (clamp) | 255 | 100% |

ANAN-8000D B10M @ 200W is the tightest unclamped cell — `audio_volume = 0.992` is only 0.8% from the rail. Worth noting if you happen to bench-test that combination.

---

## What this verification does NOT cover

The hotfix scope was the math kernel + Setup UI. The bench verifies the safety fix lands correctly. Post-bench follow-ups (NOT in this verification):

- Auto-calibration sweep wall-clock — Phase 7 has the state machine but does not auto-engage TUNE/MOX. Manual TUNE engagement before Auto Calibrate is required. This is by-design until the HL2 ATT/filter safety audit closes.
- ChannelMaster `SetTXFixedGain` actually applying DSP attenuation. Today the IQ scalar value is stored in the stub but not applied. Wire byte is the K2GX safety lever.
- PureSignal feedback DDC live wiring. ATT-on-TX gate is structurally complete but dormant until 3M-4.
- Andromeda Ganymede 500W PA tab. Informational warning only in v0.3.2.
- Thetis profile string import. NereusSDR uses 27-entry layout, deliberately incompatible with Thetis 423/507.

---

## Bench-verification reviewer sign-off

After running through all tests and filling in the session log:

```
Tested by: ________________________ Callsign: ________
Date: ________________________
Verdict: READY / NEEDS-WORK / BLOCKED
Signature:
```

If READY: push branch + open PR + merge + tag v0.3.2 via `/release patch`.

If NEEDS-WORK or BLOCKED: file the failures, do not tag.
