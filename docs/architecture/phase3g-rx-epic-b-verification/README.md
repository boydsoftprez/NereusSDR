# Sub-epic B — Noise Blanker Family Manual Verification

**Scope:** verify NB / NB2 / SNB cycling, tuning persistence, and Thetis parity after Tasks 1-11 land.

**Upstream reference:** Thetis `v2.10.3.13` @ `501e3f5` — `cmaster.c:43-68`, `console.cs:43513-43560`, `setup.cs:8572,16222-16237`.

**Setup:**
1. Clean slate: `rm -f ~/.config/NereusSDR/NereusSDR.settings`
2. Build: `cmake --build build -j$(nproc) --target NereusSDR`
3. Launch: `./build/NereusSDR.app/Contents/MacOS/NereusSDR` (macOS) / `./build/NereusSDR` (Linux)
4. Connect to any P1 or P2 radio. Tune RX1 to 40m USB with a known noise source (powerline, ignition, neighbour arc-welder, etc.) if you want a perceptual check on rows 10+.

**Sign-off:** tick each row in the PR body. Row 10 is subjective and can be marked SKIP if no noisy source is available at verification time.

| # | Behaviour under test | Expected result |
|---|---|---|
| 1 | Click VFO NB button once (Off → NB) | Label reads `"NB"` with active/lit background. Audio passing through `xanbEXTF` (Whitney blanker — click-reduction on sharp impulses). |
| 2 | Click VFO NB button again (NB → NB2) | Label reads `"NB2"` with active/lit background. NB path disengaged; audio now passing through `xnobEXTF`. Mutual exclusion holds — confirm no double-blanking artifacts. |
| 3 | Click VFO NB button third time (NB2 → Off) | Label reads `"NB"` with dim/un-lit background. Both NB and NB2 disengaged; raw I/Q passes through processIq untouched. |
| 4 | Click SNB button (independent of NB state) | SNB active regardless of NB state — can coexist with step 1 or 2. Spectral blanking visible on sustained tonal statics that the time-domain blankers can't touch. |
| 5 | RxApplet "Thr" slider to value 50 | `~/.config/NereusSDR/NereusSDR.settings` gains key `Slice0/Band40m/NbThreshold` with value `8.25` (0.165 × 50 per `setup.cs:8572 [v2.10.3.13]`). |
| 6 | RxApplet "Lag" slider to 15 ms | Same file gains `Slice0/Band40m/NbLagMs = 15`. Lag slider range is 0-20 ms per `setup.designer.cs udDSPNBLag.Maximum [v2.10.3.13]`. |
| 7 | Change band (40m → 20m), set NB2 on 20m | Key `Slice0/Band20m/NbMode = 2` written. Previous `Slice0/Band40m/NbMode` preserved unchanged (verify by diffing the settings file before/after). |
| 8 | Quit + restart the app | All NB / NB2 / SNB state restored per-slice-per-band: re-tuning to 40m restores 40m's NB state; tuning to 20m restores 20m's NB2. Setup → DSP → NB/SNB values match last session. |
| 9 | Setup → DSP → NB/SNB — change NB threshold default from 30 to 45 | Key `NbDefaultThresholdSlider = 45` persisted. Applied at next channel-create (currently means reconnect; live-apply is deferred to follow-up). |
| 10 | First launch with a clean settings file — verify default defaults | In-process `NbTuning{}` defaults exactly match Thetis `cmaster.c:43-68 [v2.10.3.13]` byte-for-byte: `nbThreshold=30.0`, `nbTauMs=0.1`, `nbHangMs=0.1`, `nbAdvMs=0.1`, `nbBacktau=0.05`, `nb2Mode=0`, `nb2MaxImpMs=25.0`, etc. Verified by `tst_nb_family` unit test (ctest target `tst_nb_family`). |
| 11 | Subjective — 40m noisy band, 20 kHz span, neighbour noise audible | NB perceptibly reduces sharp click-type impulses. NB2 perceptibly reduces denser impulse storms that NB stutters on. SNB perceptibly removes tonal/wideband statics that are invisible to both time-domain blankers. Mark SKIP if no noise source available. |
| 12 | Menu bar → DSP → NB (alternate toggle path) | Menu bar NB action toggles the same slice property as the VFO button. Both paths end at `slice->setNbMode(...)`; state remains consistent. |
| 13 | Mutual-exclusion stress: click VFO NB 6× rapidly | Label sequence should be exactly `NB-lit → NB2-lit → dim → NB-lit → NB2-lit → dim`. No stuck-dual states, no skipped states. (Automated equivalent: `tst_nb_family::cycle_three_returns_to_origin`.) |

## Attribution hook evidence

Before declaring this matrix complete, confirm all three pre-commit / CI hooks green against the sub-epic branch:

```bash
python3 scripts/verify-inline-tag-preservation.py
python3 scripts/verify-thetis-headers.py
python3 scripts/check-new-ports.py
```

All three must pass with zero findings. Log the output in the PR body under "Attribution".

## Automated-test evidence

```bash
ctest --test-dir build --output-on-failure -R 'tst_nb_family|tst_slice_nb_persistence|tst_rxchannel_nb2_polish'
```

Expected:

```
1/3 Test #NN: tst_rxchannel_nb2_polish ........  Passed
2/3 Test #NN: tst_nb_family ...................  Passed
3/3 Test #NN: tst_slice_nb_persistence ........  Passed
100% tests passed, 0 tests failed out of 3
```

Paste the full ctest output into the PR description.

## Screenshots required in PR body

1. VFO flag showing the NB button in the **Off** state (dim)
2. VFO flag showing the NB button in the **NB** state (lit, label `"NB"`)
3. VFO flag showing the NB button in the **NB2** state (lit, label `"NB2"`)
4. RxApplet with Thr + Lag sliders visible (capture a non-default value so persistence round-trip is observable via the screenshot alone)
5. Setup → DSP → NB/SNB page with all controls enabled (the pre-sub-epic state had everything greyed via `disableGroup`)
