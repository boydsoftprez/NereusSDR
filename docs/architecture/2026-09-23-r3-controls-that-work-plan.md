# R3 controls that work implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the lane the controller names at dispatch. The placeholders the operator is
> deciding on (the "Unfinished Controls" page) are not touched here; a separate plan
> follows his decisions.

**Goal:** every control whose feature already exists does what it says, the bugs the
placeholder audit found are fixed, and no user-visible text outside the placeholders
still carries developer wording.

**Architecture:** no new features. Menu items, applet controls, overlay buttons,
status badges and Setup controls that duplicate or front an existing feature are
connected to it; settings saved under one name and read under another are
reconciled; settings that only take effect when their Setup page opens are applied
at startup; developer wording is replaced with plain words.

**Tech stack:** C++20, Qt 6 widgets, Qt Test (off-screen).

**Spec:** the placeholder and dead-control audit of 2026-09-23 (file:line at
integration `da929d5f`; re-locate by text):
`/Users/j.j.boyd/.config/nereus/work/placeholder-dead-control-audit-2026-09-23.md`;
operator directives of 2026-09-23 (nothing stays disabled or inert without a plan;
all user-visible text in plain user words). R3 plan requirements R-R3-17, R-R3-21.

## Global Constraints

- Work in the worktree, branch and build directory the controller names at dispatch.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- Do not change, hide or remove any item on the operator's decision list (the
  "Unfinished Controls" page; the audit's placeholders for unbuilt features). Their
  tooltips are changed only by the plan that follows his decisions.
- A setting whose saved and read names differ keeps what users already saved: read the
  old name once and write the new one (a one-time migration), then use one name.
- Every changed user-visible string passes `OperatorWording::isPlain`; run the tests of
  every file whose strings change (grep the tests for the old text).
- Local and remote windows: a control connected here behaves as its target does in
  each mode (a control whose target is unavailable remotely shows that target's
  reason).
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets, then `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No
  unfiltered suite. Never build the `NereusSDR` target. No hardware.

## Task 1: The bugs the audit found

**Requirements:** R-R3-21, R-R3-17.

**Files:** the sites in the audit's "Dead settings" and "Other bugs found" sections:
- Penny external control (`src/gui/setup/hardware/OcOutputsHfTab.cpp` writes
  `hardware/oc/pennyExtCtrl`; the reader wants `hardware/<mac>/penny/extCtrlEnabled`).
- WSJT-X spot colours (`src/gui/SpotHubDialog.cpp` writes `WsjtxColor*`; `RadioModel`
  reads `WsjtxSpotColor`) and spot life (`WsjtxSpotLifetime` written,
  `WsjtxSpotLifetimeSec` read).
- Speech Processor's "Open VOX/DEXP Setup" (`src/gui/setup/TransmitSetupPages.cpp`
  looks for "VOX/DEXP"; the page is "DEXP/VOX").
- Ctrl+Shift+K bound to Radio > Disconnect and to clearing spots (`src/gui/MainWindow.cpp`):
  give clearing spots a free shortcut; Disconnect keeps Ctrl+Shift+K.
- Multimeter average, decimal, unit and history duration, and DSP Options' high
  resolution filter setting, applied at startup (today only when their Setup page opens).
- Connection Quality's "EP6 sequence gaps" readout shows the sequence-gap count it names.
- Band > HF goes through the band button path so the per-band memory applies.
- Tools menu test entries (antenna switch toast, TX-bound re-route) only in developer
  builds.
- DXCC spot colouring has no country table: `DxccColorProvider::loadCtyDat()` defaults
  to `:/cty.dat`, but no resource file includes `cty.dat` and nothing calls the function
  (found by the Linux suite run of 2026-09-23). Find how the shipped feature meant to get
  its table (a bundled resource or a downloaded file) and make DXCC colouring use it.
- Tests: the tests covering each site, plus new cases where none exists (a settings
  migration case per renamed key; a startup-apply case; a shortcut uniqueness check
  over all menu actions and application shortcuts).

**Acceptance:**
- Each saved value is read back where it is used, including values saved under the old
  name before this change.
- The Speech Processor button opens DEXP/VOX; Ctrl+Shift+K disconnects; no two actions
  share a shortcut (the uniqueness check fails on a duplicate).
- Multimeter and high-resolution filter settings apply after a restart without opening
  Setup.
- Band > HF restores the band's last frequency and mode like the band button.
- Release builds show no test entries in the Tools menu.
- DXCC colouring works from a fresh install: a spot from a known country gets its country's
  colour tier (a test loads the table the app loads).

**Verification:** the covering tests and the new cases, built and run by exact name.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Fix each bug with a test that fails first; commit.

## Task 2: Controls pointed at the features that already exist

**Requirements:** R-R3-21.

**Files:** the sites in the audit, each connected to the named target:
- `src/gui/MainWindow.cpp` menus: File > Profiles > TX Profiles and Mic Profiles to
  Setup > Audio > TX Profile; File > Profiles > Import and Export to Diagnostics >
  Export / Import; Radio > Antenna Setup to Hardware > Antenna/ALEX; DSP > Diversity to
  Tools > Diversity; Band > GEN to the GEN band button's path; Tools > VAX Audio to Setup >
  Audio > VAX; Help > What's New to the release notes link the About dialog uses.
- `src/gui/widgets/VfoWidget.cpp`: the right-click Diversity entry to Tools > Diversity.
- `src/gui/applets/PhoneCwApplet.cpp`: compression gauge to the transmit compression
  meter; microphone source and mic profile to the existing transmit settings; AM carrier
  to `TransmitModel::setAmCarrierLevel` (all behind the transmit permission in a remote
  window, as their targets are).
- `src/gui/containers/` and `src/gui/MainWindow.cpp`: container controls Mode, Filter,
  Antenna, Tune Step and VFO display to their actions.
- `src/gui/SpectrumOverlayPanel.cpp`: ATT to the step attenuator; the zoom buttons to the
  zoom path; the VAX button's tooltip in plain words.
- `src/gui/widgets/StatusBadge.cpp` and its users: a click opens the matching VFO flag
  popup; badges with nothing to open lose the hand cursor.
- Setup: General > Startup & Preferences auto-connect, callsign and grid to their
  existing settings; DSP > NR/ANF Enable ANF, DSP > CW peak filter, DSP > AM and FM
  squelch to their slice controls; Appearance > Meter Styles S-meter group to the
  S-meter settings; Appearance small filter display to the VFO flag's small mode;
  Hardware > OC Outputs live pin lights to the live pin state; Diagnostics > Logs to the
  log viewer; Audio > VAX level readout to the VAX level source.
- Tests: new or extended cases per surface (a menu action triggers its target; a Setup
  control changes the slice or setting it fronts; a badge click opens its popup).

**Acceptance:**
- Every item above does what its label says in a local window; in a remote window it
  does the same where its target works remotely, and otherwise shows the target's
  reason.
- None of them is greyed out or inert any more.

**Verification:** the new and extended cases, built and run by exact name.

**Execution note (advisory):** opus. Split into two commits (menus, applets, overlay
and badges; then Setup pages) if the diff is large.

- [ ] **Step 1:** Menus, applets, overlay, badges, containers with tests.
- [ ] **Step 2:** Setup pages with tests; commit.

## Task 3: Developer wording outside the placeholders

**Requirements:** R-R3-17, R-R3-21.

**Files:** the "Developer wording users see" sites in the audit that are not on the
operator's decision list: the TX refusal text "CW TX coming in Phase 3M-2" and "FM TX
coming in Phase 3M-3b" (also shown for DRM) in `src/gui/applets/TxApplet.cpp` and
`src/core/safety/BandPlanGuard.cpp` (plain words, and the right mode named for DRM);
internal class names in `src/gui/MainWindow.cpp` and in the antenna conflict-policy
choices; "(3M-3a-iii)" in `src/gui/setup/TransmitSetupPages.cpp`; "Phase 3J-1" in
`src/gui/setup/AudioTciPage.cpp`; "Task 24+" in `src/gui/setup/AudioVaxPage.cpp`;
"design spec §11.3" in `src/gui/SpectrumOverlayPanel.cpp`; "QT_LOGGING_TO_CONSOLE=1"
where a user reads it; the RF-Kit "Feature request filed" tooltip.
- Test: the wording sweep test extended to these files; the tests of every file whose
  strings change.

**Acceptance:**
- No phase names, task numbers, class names, design-document references, environment
  variables or em dashes remain in these user-visible strings; the sweep test covers
  them.
- The DRM refusal names DRM, not FM.

**Verification:** `tst_operator_wording_sweep` plus the covering tests, by exact name.

**Execution note (advisory):** opus (small).

- [ ] **Step 1:** Rewrite each string, extend the sweep; commit.
