# R3 unfinished controls implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the lane the controller names at dispatch, after the controls-that-work
> plan (Task 2 here removes controls that plan's Task 2 leaves behind).

**Goal:** every control a user can see does what it says. The unfinished controls the
operator reviewed on 2026-09-23 are hidden until their feature is built, or removed,
and the container function buttons NereusSDR already shows work where their feature
exists.

**Architecture:** one list in the app names every feature that is not built yet; each
menu item, Setup page or group, applet control, overlay control, status bar item and
container control that fronts such a feature asks that list before it is shown, so
building a feature later is one change to the list plus the feature itself. Removed
controls and the code that only served them are deleted; what users saved for them
stays in the settings file untouched. The container function buttons whose feature
exists are connected to it, acting on the container's own slice.

**Tech stack:** C++20, Qt 6 widgets, Qt Test (off-screen).

**Spec:** the operator's decisions of 2026-09-23 on the "Unfinished Controls" and
"Control Build Choices" pages and in chat (recorded in
`/Users/j.j.boyd/.config/nereus/work/questions-for-jj-2026-09-23.md`: "DECISION PAGE
RESULTS", "BUILD CHOICES RESULTS", "NEREUSSDR IDENTITY + ORDER"), reproduced below: the
features he chose to build are built after R5 and R4 and stay hidden until then;
NereusSDR keeps its own design, so Thetis controls built around VFO A/B or RX1/RX2 are
not forced in. The placeholder and dead-control audit (file:line at integration
`da929d5f`, re-locate by text):
`/Users/j.j.boyd/.config/nereus/work/placeholder-dead-control-audit-2026-09-23.md`; the
container button scouts:
`/Users/j.j.boyd/.config/nereus/work/unfinished-controls-scout-containers-diag-2026-09-23.md`
and `/Users/j.j.boyd/.config/nereus/work/unfinished-controls-scout-thetis-buttons-2026-09-23.md`.
R3 plan requirements R-R3-21 and, added with this plan, R-R3-49.

## The operator's decisions

Hide = hidden in local and remote windows until the feature is built. After R4 = the
operator chose to build it, after R5 and R4; hidden until then. Remove = the control
goes.

| Item | Where | Outcome |
|---|---|---|
| display-mode | View > Display Mode | Hide |
| ui-scale | View > UI Scale; Setup > General > UI Scale & Theme | Hide |
| dark-theme | View > Dark Theme | Remove |
| minimal-mode | View > Minimal Mode; Setup > Appearance > Collapsible Display | Hide |
| keyboard | View > Keyboard Shortcuts; Setup > Keyboard > Shortcuts | Hide |
| equalizer | DSP > Equalizer | Hide; after R4 |
| transverters | Radio > Transverters; Band > VHF; Hardware > XVTR; OC Outputs VHF tab | Hide |
| band-stack | Band > Band Stacking; the band stack dots on the status bar | Hide |
| cwx | Tools > CWX; Phone/CW applet CW page; the CW keyer settings; CWX on the status bar | Hide |
| memories | Tools > Memory Manager; the Memories spot option | Hide |
| cat | Tools > CAT Control; Setup > Serial Ports and TCP/IP CAT; CAT on the status bar | Hide |
| midi | Tools > MIDI Mapping; Setup > MIDI Control | Hide |
| macros | Tools > Macro Buttons | Remove |
| help | Help > Getting Started, Help, Data Modes | Hide |
| acc | Phone/CW applet +ACC | Hide |
| phone-mon | Phone/CW applet MON and its level | Hide |
| fm-page | Phone/CW applet FM page | Hide |
| rfkit-tune | RF-Kit applet TUNE and BYPASS | Hide |
| macro-buttons | Container function buttons | Task 3: the buttons whose feature exists work; the rest are hidden; macro buttons after R4 |
| discord | Container controls: Discord button | Remove |
| voice | Voice record and play container control; VFO flag record and play; DVK on the status bar | Hide |
| rf-gain | Pan overlay RF Gain slider | Remove |
| wnb | Pan overlay WNB | Remove |
| iq-ch | Pan overlay IQ channel | Remove |
| fdx | Status bar FDX | Hide |
| startup-rest | Setup > General > Startup & Preferences: what is left after auto-connect, callsign and grid are connected | Remove |
| navigation | Setup > General > Navigation | Hide |
| sam | Setup > DSP > AM/SAM synchronous AM options | Hide; after R4 |
| rx2-display | Setup > Display > RX2 Display | Remove |
| gradients | Setup > Appearance > Gradients | Remove |
| skins | Setup > Appearance > Skins | Hide |
| tx-profiles-leaf | Setup > Transmit > TX Profiles (a page that only says it moved) | Hide |
| rx2-rate | Setup > Hardware > Radio Info RX2 sample rate | Remove |
| bw-monitor | Setup > Hardware > Bandwidth Monitor (Hermes Lite 2) | Hide |
| hl2-i2c | Setup > Hardware > HL2 Options second I2C bus | Hide |
| conn-history | Setup > Diagnostics > Connection Quality 60-second history | Hide; after R4 |
| logging | Setup > Diagnostics > Logging: log level, open and clear, categories | Hide; after R4 |
| siggen | Setup > Diagnostics > Signal Generator and Hardware Tests | Hide |
| netdiag-local | Network Diagnostics with a local radio: Jitter, Loss, Gap | Built (remote-window parity Task 6): measured per connection (RadioLinkStats), shown in both windows |
| dsp-rate | Setup > Audio > Advanced DSP rate and block size | Hide |
| vax-feedback | Setup > Audio > Advanced VAX feedback tuning | Remove |
| iq-to-vax | Setup > Audio > Advanced Send IQ to VAX, TX Monitor to VAX | Hide |
| mute-vax-tx | Setup > Audio > Advanced Mute VAX during transmit on another slice | Hide; after R4 |
| ant-conflict | Setup > Hardware > Antenna conflict policy | Hide |
| oc-extras | Setup > Hardware > OC Outputs hot switching, USB BCD, external PA | Hide |
| hw-diversity | Setup > Hardware > Diversity tab | Remove |
| perf-checks | Setup > Diagnostics > Performance checkboxes | Remove |
| multimeter | Setup > Multimeter peak hold, text hold, digital delay, history | Hide those four; after R4 |
| wsjtx-filters | Spot Hub WSJT-X filters (three) | Hide; after R4 |
| rbn-rate | Spot Hub RBN rate limit | Hide; after R4 |
| freedv-psk | Spot Hub report FreeDV decodes to PSK Reporter | Hide; after R4 |
| spot-auto-bg | Spot display automatic background colour | Remove |
| tci-extras | TCI rate limit, CW to CWU, TX channel, sensor intervals, the three RX2 VFO options, stream channels | Built (2026-09-29, codex/tci-rx2-quirks); left the unbuilt list |
| tci-sliceb | TCI Slice B rate | Remove |
| small-filter | Setup > Appearance small filter display on the VFO flag (the flag stores the setting but draws nothing with it) | Hide |
| apf-params | Setup > DSP > CW peak filter bandwidth and gain (no setting behind them; the peak filter's on and off works) | Hide; after R4 |
| am-tail | Setup > DSP > AM/SAM maximum squelch tail | Hide; after R4 |
| fm-dev | Setup > DSP > FM deviation and de-emphasis | Hide; after R4 |
| mic-acc | Phone/CW applet microphone source: the ACC item | Hide (with acc) |
| dxcc-colour | DXCC spot colouring: the country table loads, but nothing switches the colouring on and no log import exists | After R4 (nothing visible claims it today) |
| export-radio | Setup > Diagnostics > Export / Import: Export Connected Radio (the button only said it was not available) | Hide (added 2026-09-24 under the operator's decision-page rule, until built = hide). Built 2026-09-28: it saves the connected radio's settings (the Core's in a remote window) and is disabled with the reason when no radio is connected |
| fm-tx | Setup > DSP > FM: the greyed FM transmit group | Hide until FM transmit is built (added 2026-09-24, same rule; asked 2026-09-24, the operator may override) |
| fm-repeater | The VFO flag's FM repeater buttons (minus, simplex, plus): they store a transmit direction and FM transmit is not built | Hide until FM transmit is built (added 2026-09-24, same rule) |
| ddc-routing | Setup > Hardware > DDC Routing: its choices do not steer the radio's receivers yet | Hide until multi-panadapter receiver routing is built (added 2026-09-24, same rule) |
| remote-filter-policy | The filter policy dialog in a remote window says editing is not available; a local window edits it | Build: moved to the remote radio hardware plan's Task 6 (it needs a new Core property and a version; added 2026-09-24) |
| hpf-bcast | Filter policy dialog: "HPF (broadcast band reject) enabled" (nothing reads it) | Hide (added 2026-09-24 under the operator's decision-page rule, until built = hide) |
| freq-cal | Calibration: the frequency calibration Start button (nothing handles it) | Hide (added 2026-09-24, same rule) |
| fm-flag | The VFO flag's FM page: the CTCSS tone mode and tone choices (no tone encoder or detector), the Offset box (a transmit shift) and Rev (it only changes the display) | Hide until built: controls that only serve FM transmit go with the FM transmit entry; a receive-side one that is not built gets its own entry (added 2026-09-24, same rule) |

## Global Constraints

- Work in the worktree, branch and build directory the controller names at dispatch.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- The table above is binding. An item's outcome is never changed, and nothing outside
  the table (and Task 3's button list) is hidden or removed. Nothing marked "after R4"
  is built here.
- Hidden is not removed: a hidden control keeps its code and its saved settings, and
  appears again when its feature is marked built. Removed controls keep users' saved
  values in the settings file untouched (no migration, no deletion), as the anti-VOX
  source setting did.
- Local and remote windows: every hide, removal and connection applies to both. A
  connected button works in a remote window where its target works remotely; otherwise
  it shows its target's reason in plain words.
- Operator wording: plain user words; every new or changed string passes
  `OperatorWording::isPlain`; run the tests of every file whose strings change.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets, then `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No
  unfiltered suite. Never build the `NereusSDR` target. No hardware.

## Task 1: One list of unbuilt features, and hide them

**Requirements:** R-R3-49 (added to the R3 plan's requirement table in this task:
"Every control a user can see does what its label says. A control whose feature is not
built yet is hidden in local and remote windows through one list that names each
unbuilt feature, and appears when its feature is built. Controls the operator chose to
remove are gone; values users saved for them stay in the settings file."), R-R3-21.

**Files:**
- Create: `src/gui/UnbuiltFeatures.{h,cpp}` (the list: one entry per unbuilt feature,
  each marked not built; a single query every surface calls), added to the GUI
  library's CMake source list
- Modify: every surface of the table's Hide rows, re-located from the audit:
  `src/gui/MainWindow.cpp` (the View, Radio, Band, DSP, Tools and Help menu items; the
  status bar band stack dots, CWX, DVK, FDX and CAT items), `src/gui/SetupDialog.cpp`
  (hidden pages are not registered; a category left with no pages is not shown; the
  Setup search finds no hidden page), `src/gui/setup/GeneralSetupPages.cpp`,
  `src/gui/setup/AppearanceSetupPages.cpp`, `src/gui/setup/KeyboardSetupPages.cpp`,
  `src/gui/setup/CatNetworkSetupPages.cpp` (Serial Ports, TCP/IP CAT, MIDI Control, and
  the TCI settings rows of the table), `src/gui/setup/AudioTciPage.cpp` (the TCI
  settings rows of the table), `src/gui/setup/TransmitSetupPages.cpp`,
  `src/gui/setup/DspSetupPages.cpp` (the CW keyer settings; the synchronous AM options;
  the CW peak filter and squelch controls the controls-that-work plan connected stay),
  `src/gui/setup/DiagnosticsSetupPages.cpp` (Signal Generator, Hardware Tests, Logging),
  `src/gui/diagnostics/DiagnosticsPhaseHPages.cpp` (Connection Quality history),
  `src/gui/NetworkDiagnosticsDialog.cpp` (Jitter, Loss and Gap rows for a local radio),
  `src/gui/setup/MultimeterPage.cpp` (peak hold, text hold, digital delay, history),
  `src/gui/setup/AudioAdvancedPage.cpp` (DSP rate and block size; Send IQ to VAX and TX
  Monitor to VAX; Mute VAX during transmit on another slice), `src/gui/setup/hardware/`
  (XVTR tab, OC Outputs VHF tab and the hot switching, USB BCD and external PA groups,
  Bandwidth Monitor tab, HL2 Options second I2C bus group, antenna conflict policy
  group), `src/gui/applets/PhoneCwApplet.cpp` (+ACC, MON and its level, the CW and FM
  pages; with one page left the applet shows no page tabs),
  `src/gui/applets/Rf2ksApplet.cpp` (TUNE, BYPASS), `src/gui/widgets/VfoWidget.cpp`
  (record, play), `src/gui/SpotHubDialog.cpp` (the Memories spot option, the WSJT-X
  filters, the RBN rate limit, the FreeDV to PSK Reporter option),
  `src/gui/containers/ContainerSettingsDialog.cpp` and the container item loading (the
  Voice Rec/Play control is not offered; one already in a saved container loads, is not
  shown, and is kept when the container is saved)
- Test: create `tests/tst_unbuilt_features.cpp` (registered in `tests/CMakeLists.txt`
  with the labels the neighbouring GUI tests use); extend the tests of every surface
  above that asserted a hidden control's presence (grep `tests/` for each control's
  object name and text)

**Acceptance:**
- Every Hide row of the table is absent in a local window and in a remote window: no
  menu item, no Setup page or group, no applet control, no status bar item, no
  container control offered; Setup categories with nothing left are absent and the
  Setup search does not find hidden pages.
- Marking one feature built in the list (in a test) makes every one of its surfaces
  appear, and nothing else changes.
- Hiding never deletes a saved value: a settings file holding values for hidden
  controls round-trips unchanged through a start and a save.
- A saved container holding a Voice Rec/Play control loads without error, does not
  show it, and saves it back.
- The tests enumerate the list, so an unbuilt feature added without its surfaces
  hidden fails a test.

**Verification:** `tst_unbuilt_features` plus the changed surface tests, built and run
by exact name; the controller records the test targets in the ledger.

**Execution note (advisory):** opus. Depends on the controls-that-work plan.

- [ ] **Step 1:** The list and its query, with `tst_unbuilt_features` failing first
  against one surface; then every surface; commit.

## Task 2: Remove the controls the operator removed

**Requirements:** R-R3-49, R-R3-21.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (View > Dark Theme, Tools > Macro Buttons),
  `src/gui/SpectrumOverlayPanel.{h,cpp}` (the RF Gain slider, WNB and IQ channel
  controls and their signals and handlers),
  `src/gui/setup/GeneralSetupPages.cpp` (every Startup & Preferences control except the
  three the controls-that-work plan connected: auto-connect, callsign, grid),
  `src/gui/setup/DisplaySetupPages.cpp` and `src/gui/SetupDialog.cpp` (RX2 Display),
  `src/gui/setup/AppearanceSetupPages.cpp` (Gradients),
  `src/gui/setup/hardware/RadioInfoTab.cpp` (RX2 sample rate),
  `src/gui/setup/AudioAdvancedPage.cpp` (VAX feedback tuning),
  `src/gui/setup/hardware/DiversityTab.*` and its registration (Hardware > Diversity),
  `src/gui/setup/DiagnosticsSetupPages.cpp` (Performance checkboxes),
  `src/gui/SpotHubDialog.cpp` (automatic background colour option),
  `src/gui/setup/AudioTciPage.cpp` or `src/gui/setup/CatNetworkSetupPages.cpp` (the TCI
  Slice B rate setting), `src/gui/containers/ContainerSettingsDialog.cpp`, the container
  item factory and the Discord button item and its editor (a Discord control in a saved
  container is dropped on load with one log line and no error); PROVENANCE rows and
  CMake source lists for any ported file deleted outright
- Test: the tests covering each surface (grep `tests/` for the removed controls' object
  names and text; tests that only exercised a removed control go with it), plus a
  saved-container case with a Discord control

**Acceptance:**
- None of the Remove rows appears in a local or remote window, and no code path is left
  that only served one of them (no orphaned signals, handlers, settings readers or
  editors).
- Saved values for removed controls stay in the settings file untouched after a start
  and a save.
- A saved container holding a Discord control loads without error and without it.
- Attribution checks pass (`scripts/verify-thetis-headers.py`,
  `scripts/check-new-ports.py` run by the pre-commit hook).

**Verification:** the covering tests, built and run by exact name.

**Execution note (advisory):** opus. After Task 1 and the controls-that-work plan's
Task 2.

- [ ] **Step 1:** Remove each control, its code and its tests; commit.

## Task 3: The container function buttons that fit, working on their own slice

**Requirements:** R-R3-49, R-R3-21.

**Files:**
- Modify: `src/gui/meters/OtherButtonItem.{h,cpp}` and `src/gui/meters/ButtonBoxItem.{h,cpp}`
  (an unavailable state with a reason; lit state from the target), a small dispatcher
  that maps each connected button to its existing target (new file in `src/gui/containers/`
  or `src/gui/meters/`, NereusSDR-original), `src/gui/containers/ContainerWidget.{h,cpp}`
  and `src/gui/containers/ContainerSettingsDialog.cpp` (the container's receiver choice
  lists the slices A to D instead of RX1/RX2, reading today's saved RX1 as slice A and
  RX2 as slice B; the item editor tag fix), `src/gui/MainWindow.cpp` (connect each
  container's buttons and band buttons to its own slice; remote gating in
  `applyRemoteRoleGating`), `src/models/RadioModel.{h,cpp}` (a band button overload that
  takes the slice)
- Test: create `tests/tst_other_button_item.cpp`; extend `tests/tst_container_persistence.cpp`
  and `tests/tst_remote_gui_gating.cpp`

**The buttons.** Of the 34 function buttons NereusSDR offers today, these are connected
to features that already exist (scout note, "IDs and targets"): Power (a local window
connects or disconnects its radio; a remote window connects or disconnects its Core),
MON (`TransmitModel::setMonEnabled`), TUN (`RadioModel::setTune`), MOX
(`MoxController::setMox`), 2TON (`TwoToneController::setActive`), PS-A (the PureSignal
facade's action), ANF (`SliceModel::setAnfEnabled`), SNB (`SliceModel::setSnbEnabled`),
MNF (`NotchModel::setGlobalEnabled`), Peak (`SpectrumWidget::setPeakHoldEnabled` on the
container slice's spectrum), CTUN (`SpectrumWidget::setCtunEnabled`, routed to the Core
in a remote window as today), Mute (`SliceModel::setMuted`), BIN
(`SliceModel::setBinauralEnabled`), VAX 1 and VAX 2 (captions changed from VAC1 and VAC2;
`AudioEngine::setVaxEnabled` for channels 1 and 2). These are hidden (through Task 1's
list, never written into the saved visibility): RX2, SUB RX and Pan Swap (they belong to
Thetis's RX1/RX2 structure), DUP (full duplex), Play and Rec (voice keyer), xPA (external
PA), AVG (built after R4 with the display work), Var1, Var2, Rx/Tx and XVTR (no setting behind
them; XVTR goes with the transverters), and the eleven display modes. The macro
buttons stay hidden (built after R4).

**Acceptance:**
- On a container set to slice A while slice B is active, ANF turns slice A's ANF on and
  off and the button lights to match; Mute, BIN and SNB act on slice A the same way; a
  band button on that container changes slice A's band (today it changes the active
  slice).
- A container set to a slice that does not exist shows its buttons unavailable with a
  plain reason, and a click does nothing.
- A saved layout with today's RX1/RX2 receiver setting loads as slice A/B; the choice
  now offers slices A to D and round-trips.
- Local and disconnected: MOX, TUN and 2TON are unavailable and do nothing. In a remote
  window, the transmit buttons show the transmit reason on click and change nothing; the
  others act on the Core's slice.
- A layout saved today with every button visible loads with the hidden buttons not
  drawn, and saving it keeps their saved visibility unchanged.
- The item's property editor opens for the function buttons and for the band, mode,
  filter, antenna, tune step and VFO display items (their tags match what they save).

**Verification:** `tst_other_button_item`, `tst_container_persistence`,
`tst_remote_gui_gating` and the container editor tests, built and run by exact name.

**Execution note (advisory):** opus. After Task 1.

- [ ] **Step 1:** The dispatcher, slice targeting, receiver choice, gating and editor
  fix with tests; commit.

## Task 4: No placeholder marks left

**Requirements:** R-R3-49, R-R3-21.

**Files:**
- Modify: every remaining `SetupPage::markNyi` and `NyiOverlay::markNyi` call on a
  surface the app creates (after Tasks 1-3 each such control is hidden, removed or
  working); the applet classes the app never creates are left alone
- Test: extend `tests/tst_operator_wording_sweep.cpp` (or a new sweep test) to build
  the main window, every Setup page and every applet the app creates, local and
  remote, and assert no widget carries a not-yet-implemented mark or tooltip

**Acceptance:**
- No control a user can reach carries a not-yet-implemented mark, overlay or tooltip;
  the sweep fails if one is added.

**Verification:** the sweep test and the tests of changed files, by exact name.

**Execution note (advisory):** opus (small). After Tasks 1-3.

- [ ] **Step 1:** Remove the remaining marks, extend the sweep; commit.
