# Sub-epic C-1 (NR Cross-Platform) — Manual Verification Matrix

> **Status:** Matrix drafted. Work through every item below before
> opening the merge PR. Tick each checkbox or fill in the table row.
> Reference items by their prefix (A1, B2, etc.) in PR comments.

## How to use

1. Build and launch the worktree binary:
   ```
   cmake --build build && open build/NereusSDR.app   # macOS
   cmake --build build && ./build/NereusSDR          # Linux
   cmake --build build && .\build\NereusSDR.exe      # Windows
   ```
2. Connect to a radio if the item requires audio (§C items); a noise
   floor on an unused band is sufficient for most audibility checks.
3. For each item below, follow the **How to verify** column, observe
   the **Expected** result, and tick the checkbox.
4. Items marked **DEFERRED** or **NA** do not require a tick — read
   the note and move on.
5. Items marked **conditional** require a specific platform or second
   slice; skip gracefully if the prerequisite is unavailable and note
   it in the PR.

Total items: **47** (40+ mandatory, 7 deferred/conditional/NA).

---

## §A. VFO Flag Layout (per platform)

Verify that the NR button row inside the VFO Flag shows the correct
set of buttons on each platform and fits within the pre-existing cell
span.

| # | Item | Expected | How to verify |
|---|---|---|---|
| A1 | Linux — button count | 5 visible buttons: NR1, NR2, NR3, NR4, DFNR. BNR and MNR are hidden (not just greyed — absent from layout). | Launch on Linux, open VFO Flag, count NR buttons. |
| A2 | Windows (no NVIDIA GPU) — button count | 5 visible: NR1, NR2, NR3, NR4, DFNR. BNR hidden, MNR hidden. | Launch on a Windows host without an RTX 4000+ card, open VFO Flag, count. |
| A3 | Windows (NVIDIA RTX 4000+) — button count | 6 visible: NR1, NR2, NR3, NR4, DFNR, BNR. MNR hidden. | Launch on Windows with RTX 4000+ (BNR capability flag true), open VFO Flag. |
| A4 | macOS — button count | 6 visible: NR1, NR2, NR3, NR4, DFNR, MNR. BNR hidden. | Launch on macOS, open VFO Flag, count NR buttons. |
| A5 | Cell span / flag width unchanged | All visible buttons fit within the original NR\|NR2 two-button cell span. Flag total width is identical to pre-C1. | Resize the VFO Flag to its minimum width; buttons should not overflow or wrap. Compare against a pre-C1 screenshot if available. |

---

## §B. Mutual Exclusion

Verify that enabling one NR variant automatically disables all others,
and that clicking an active button turns NR off.

- [ ] **B1.** With all NR buttons off, click **NR1**. Verify NR2, NR3, NR4, DFNR (and BNR/MNR where present) all remain off. NR1 lights up.
- [ ] **B2.** While **NR2** is active, click **NR4**. Verify NR2 turns off and NR4 turns on in the same action (single-click, no intermediate state).
- [ ] **B3.** While **NR4** is active, click **NR4** again. Verify NR4 turns off and all NR is disabled (Off state).
- [ ] **B4.** *(DEFERRED — separate follow-up PR)* Spectrum overlay NR-bank mirror. The overlay panel's NR section should reflect the VFO Flag NR state. **Skip this item; it is tracked as a follow-up PR. Do not block merge on B4.**

---

## §C. WDSP Pipeline (audible — requires connected radio)

All items require a live radio connection with a noisy signal
(e.g. 40 m SSB band or a weak CW signal for NR testing).

| # | Item | Expected | How to verify |
|---|---|---|---|
| C1 | NR1 on — basic spectral subtraction | Broadband noise floor drops on voice peaks; voice remains intelligible. | Enable NR1, tune to a noisy SSB signal. Compare noise floor with NR1 on vs off. |
| C2 | NR2 with AE Filter on | Fewer musical artifacts ("warbling") compared to NR2 without AE Filter. | Enable NR2, right-click → popup → enable AE Filter. Listen for reduced artifacts on a CW or SSB signal. |
| C3 | NR2 with post2 Factor 25 | More aggressive noise reduction audible; some voice detail may be sacrificed. | Enable NR2, right-click → popup → set post2 Factor to 25 (max). Compare with Factor 0. |
| C4 | NR3 with Default model | Voice-centric denoising; speech intelligibility improved relative to NR1. | Enable NR3, leave model at "Default". Listen on a weak SSB voice signal. |
| C5 | NR3 with user-loaded .bin model | Model-specific denoising characteristic audible; differs from Default model. | Enable NR3, right-click → "More Settings…" → NR3 sub-tab → load a custom .bin file. Observe audible change vs Default. |
| C6 | NR4 Algo 1 vs Algo 2 vs Algo 3 | Each algorithm produces a perceptibly different noise signature or tonality. | Enable NR4, switch among Algo 1 / 2 / 3 in the popup or Setup page. Listen on a noisy signal. |
| C7 | NR position Pre-AGC vs Post-AGC | AGC behaves differently: Pre-AGC means AGC acts on the NR-reduced signal; Post-AGC means NR runs on the AGC output. Gain hunting pattern changes. | Enable NR1 or NR2. In Setup → DSP → NR → NR1 sub-tab, toggle Position between Pre-AGC and Post-AGC. Observe AGC gain hunting on a fading signal. |

---

## §D. Right-Click Popups

Verify that every NR button exposes a `DspParamPopup` with live
parameter controls and a "More Settings…" deep link.

| # | Item | Expected | How to verify |
|---|---|---|---|
| D1 | Each button opens a popup | Right-clicking NR1, NR2, NR3, NR4, DFNR (and BNR/MNR where present) each opens a `DspParamPopup` with controls specific to that algorithm. | Right-click each visible button in turn. Confirm popup appears; confirm it is the correct popup for that algorithm (spot-check one unique control per popup). |
| D2 | Slider changes are live | Moving any slider in the popup produces an audible change immediately (no Apply button needed). | Connect a radio, enable an NR variant, open its popup, drag a slider while listening. |
| D3 | "More Settings…" deep links | Clicking "More Settings…" in any NR popup opens Setup → DSP → NR at the sub-tab matching the clicked NR variant (e.g. NR2 popup → NR2 sub-tab). | Click "More Settings…" from the NR1 popup; verify landing on NR1 sub-tab. Repeat for NR2 and NR4. |
| D4 | Tooltips on every popup control | Hovering over each slider, spinbox, radio button, and checkbox in the popup shows a non-empty tooltip. | Open NR2 popup, hover over every control. Repeat for NR4 popup. |

---

## §E. Setup Page — Thetis Parity

Verify that **Setup → DSP → Noise Reduction** sub-tabs match the
Thetis layout documented in the plan.

| # | Item | Expected | How to verify |
|---|---|---|---|
| E1 | NR1 sub-tab layout | Contains: Taps spinbox, Delay spinbox, Gain spinbox, Leak spinbox, Position combo (Pre-AGC / Post-AGC), RX1 and RX2 enable columns. Matches Thetis `setup.cs` NR1 group layout. | Open Setup → DSP → NR → NR1. Compare against Thetis reference screenshot or plan §E1. |
| E2 | NR2 sub-tab layout | Contains: Gain Method radio group, T1 / T2 spinboxes, NPE Method radio group, AE Filter checkbox, post2 group (Factor spinbox + enable checkbox). | Open Setup → DSP → NR → NR2. Verify all controls present and labeled. |
| E3 | NR3 sub-tab layout | Contains: Model selector combo/path, "Use Model" button, "Default" (reset) button, UseDefaultGain checkbox. | Open Setup → DSP → NR → NR3. Verify all controls present. |
| E4 | NR4 sub-tab layout | Contains: 5 spinboxes (from plan — e.g. Gamma, Xi, Alpha/Beta, smoothing params), Algo 1 / Algo 2 / Algo 3 radio buttons, RX1 and RX2 enable columns. | Open Setup → DSP → NR → NR4. Verify all 5 spinboxes and 3 radios present. |
| E5 | MNR sub-tab (macOS only) | Contains: 6 tuning sliders — Strength, Aggressiveness, Floor, Alpha, Bias, Gsmooth — each with a factory-default Reset button. Tab is only present on macOS. | On macOS: open Setup → DSP → NR, verify MNR tab is present and all 6 sliders + Reset buttons render. On Linux/Windows: verify MNR tab is absent. |
| E6 | DFNR sub-tab | Contains: AttenLimit slider/spinbox and PostFilterBeta slider/spinbox. Defaults match AetherSDR defaults: AttenLimit = 100 dB, PostFilterBeta = 0. | Open Setup → DSP → NR → DFNR. Verify both controls present and at default values on a fresh profile. |

---

## §F. Persistence

Verify that NR state and parameter values survive an application
restart.

- [ ] **F1.** Enable **NR2**, open its popup, set post2 Factor to **18**, close popup. Quit the app, relaunch. Open VFO Flag: NR2 should still be active. Open NR2 popup: post2 Factor should still read 18.
- [ ] **F2.** Inspect AppSettings XML (`~/.config/NereusSDR/NereusSDR.settings`). NR keys must NOT include a band suffix (e.g. `Slice0/NrMode`, not `Slice0/40m/NrMode`). Verify by searching the file for band name strings adjacent to NR keys.
- [ ] **F3.** In Setup → DSP → NR → NR3, load a custom model `.bin` file. Quit and relaunch. Open the same sub-tab: the model path should still be populated and the file still selected.
- [ ] **F4.** *(Conditional — requires two active slices)* Assign slice 0 to NR1 and slice 1 to NR4. Quit and relaunch. Verify slice 0 still shows NR1 active and slice 1 shows NR4 active, independently. **Skip if a second slice cannot be enabled in this build; note in PR.**

---

## §G. Platform-Specific

Verify per-platform presence, absence, and end-to-end operation of
platform-gated NR backends.

- [ ] **G1.** *(NA — BNR not implemented in Sub-epic C-1)* Windows + NVIDIA: BNR end-to-end denoising. **Skip; tracked as a known deferral. Do not block merge on G1.**
- [ ] **G2.** Windows (no NVIDIA): BNR button is **hidden** (not merely disabled). Open VFO Flag on a Windows host without RTX 4000+. Confirm the BNR button is not in the layout at all.
- [ ] **G3.** macOS: MNR button is present and clicking it enables Accelerate-based noise reduction. Enable MNR on macOS, listen on a noisy signal. Confirm audible noise reduction consistent with an ML-accelerated approach.
- [ ] **G4.** Linux: DFNR works end-to-end. Model loads from the AppImage payload (`squashfs-root/usr/share/NereusSDR/models/dfnet3/` or equivalent path). Enable DFNR on Linux, verify no crash and audible denoising.
- [ ] **G5.** macOS: DFNR works end-to-end. Model loads from `.app/Contents/Resources/models/dfnet3/`. Enable DFNR on macOS, verify no crash and audible denoising.
- [ ] **G6.** Windows: DFNR works end-to-end. `deepfilter.dll` loads successfully and model loads from `<install>/models/dfnet3/`. Enable DFNR on Windows, verify no crash and audible denoising.

---

## §H. Licensing Artifacts

Verify that all third-party license notices are present in the correct
locations in every release artifact.

| # | Item | Expected | How to verify |
|---|---|---|---|
| H1 | AboutDialog license list | The AboutDialog "Third-Party Licenses" section lists: **rnnoise** (BSD-2-Clause), **libspecbleach** (LGPL-2.1), **DeepFilterNet3** (MIT / Apache-2.0). | Open Help → About NereusSDR → Third-Party Licenses. Scroll through all entries. |
| H2 | LGPL-COMPLIANCE.md user-facing relink instructions | `LGPL-COMPLIANCE.md` (shipped in the distribution or accessible from About) renders instructions for users to relink against a modified libspecbleach. Verify the instructions are intelligible and include a URL or archive path. | Open or extract `LGPL-COMPLIANCE.md` from the AppImage / DMG / ZIP. Read the relink section. |
| H3 | `verify-thetis-headers.py` passes | `python3 scripts/verify-thetis-headers.py` exits 0 on all new `wdsp/` files added in this PR. Note: Sub-epic C-1 ports that use `no-port-check: escape-hatch` markers are intentionally excluded from the header check — the escape hatch is documented in the PR body. Verify the script still exits 0 with the markers in place. | Run `python3 scripts/verify-thetis-headers.py` from the repo root. Confirm exit 0. |
| H4 | `verify-inline-tag-preservation.py` passes | The inline-tag preservation script exits 0 for all `// From Thetis` cites added in this PR. | Run `python3 scripts/verify-inline-tag-preservation.py` from the repo root. Confirm exit 0. |
| H5 | Release artifact license directories | Every release artifact (AppImage, DMG, Windows portable ZIP) contains: `licenses/deepfilter/LICENSE`, `licenses/deepfilter/LICENSE-APACHE`, `licenses/deepfilter/LICENSE-MIT`, `licenses/deepfilter/COMMIT`, `licenses/rnnoise/LICENSE`. | AppImage: run `./NereusSDR.AppImage --appimage-extract` and inspect `squashfs-root/usr/share/NereusSDR/licenses/`. DMG: mount and inspect `NereusSDR.app/Contents/Resources/licenses/`. Windows ZIP: unzip and inspect `licenses/`. |

---

## Known Deferrals

The following items are intentionally **not** verified by this matrix.
They are tracked separately and must not block the Sub-epic C-1 merge.

| Deferral | Reason |
|---|---|
| **BNR (NVIDIA Broadcast)** | gRPC/NIM-based integration is out of scope for Sub-epic C-1. G1 is marked NA; BNR appears as a visible button only on RTX 4000+ Windows hosts (A3) and that visibility check is in scope, but the denoising pipeline is not. |
| **Spectrum overlay NR-bank mirror (B4)** | The overlay panel's NR section currently does not reflect VFO Flag NR state. This is a known gap flagged for a follow-up PR. |
| **Unit tests (Task 20)** | `tests/tst_rx_channel_nr_wiring.cpp` WDSP-setter call verification via fake backend. If deferred from this PR, it must be tracked as a follow-up issue before the phase closes. |
| **ANF Enable wiring from Setup page** | `SliceModel` currently lacks an `anfEnabled` Q_PROPERTY wired to the Setup → DSP → NR ANF controls. The VFO Flag's ANF button works via Sub-epic B wiring. The Setup-page wire-up is deferred. |
| **NR3 custom model download / cloud fetch** | Model loading is from a local path only in C-1. Cloud model fetching is a separate feature. |
| **BNR end-to-end denoising (G1)** | Repeated here for clarity: BNR pipeline is deferred; only the button-presence / button-absence layout behavior (A3, G2) is verified. |
