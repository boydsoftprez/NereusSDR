# Two-tone as one setting, and remote transmit edits saved: plan

> **Execution:** run with `crew` under the `cost-aware-execution` policy. One
> implementer at a time, no per-task review, one whole-branch review at the end.

**Goal.** The two-tone test level stops coming back at -6 dB. Two-tone becomes
one station setting, as in Thetis, instead of a value every TX profile carries
and restores. A one-time settings upgrade drops the two-tone values saved
inside profiles and resets a saved -6 dB level to Thetis's 0 dB. A transmit
setting changed from a remote app is written to the Core's settings file, not
only held in memory until the Core exits.

**Why.** On 2026-10-09 PureSignal calibration on the Rock 5C Core failed with
the two-tone at -6 dB: the halved envelope (TX monitor peak about 0.30 against
hwPeak 0.6121) never fills calcc's upper amplitude buckets, so the collect
state times out and restarts (third_party/wdsp/src/calcc.c:2223-2252; the same
requirement in Thetis wdsp/calcc.c:701-760 [v2.10.3.15]). At 0 dB it
calibrated at once. The model default has been 0 since 1972888fb and the
profile seed since 74812ec8b, but profiles saved before then carry -6 and put
it back every time the profile loads. Thetis never had this problem because
two-tone is not part of a Thetis TX profile: `AddTXProfileTable`
(database.cs:4299 [v2.10.3.15]) has no two-tone columns; the level is the
Setup spinbox udTwoToneLevel, default 0, range -96..0
(setup.Designer.cs:62144-62173 [v2.10.3.15]), read at setup.cs:11093-11095
[v2.10.3.15].

## Global Constraints

Copied from CLAUDE.md and CONTRIBUTING.md; every task keeps them.

- C++20 / Qt6. No raw `new`/`delete`, no `#define` constants, braces on all
  control flow, `auto` only when the type is obvious. `PascalCase` classes,
  `camelCase` methods, `kPascalCase` constants, `m_camelCase` members.
  Errors via `qCWarning(lcCategory)`, no exceptions.
- **`AppSettings`, never `QSettings`.** PascalCase keys; booleans are the
  strings `"True"` / `"False"`. Transmit keys live at `hardware/<mac>/tx/<Key>`;
  profile keys at `hardware/<mac>/tx/profile/<name>/<Key>`.
- Thetis cites carry the version tag: `// From Thetis file:line [v2.10.3.15]`
  (pin `3759d09`). Constants keep their exact values as named `constexpr`.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Don't remove code you didn't add, beyond what a task names.
- Each changed source file that carries a "Modification history (NereusSDR)"
  block gets a dated line (2026-10-09, J.J. Boyd, AI-assisted with Claude Code).
- Commits are GPG-signed. Never `--no-gpg-sign`, never `--no-verify`. No
  `Co-Authored-By: Claude` trailer. No em-dashes in commit messages.
- Tests: read `docs/development/fast-test-loop.md` first. Build and run single
  test targets, never the whole suite. Run test binaries directly with
  `QT_QPA_PLATFORM=offscreen` as a prefix on the command; never export it.
- Build directory: `build/` is already configured (2026-10-09) with no
  network fetches. Build targets in it; do not reconfigure from scratch. If a
  reconfigure is ever needed, use exactly:

  ```
  D=/Users/j.j.boyd/NereusSDR/build/_deps
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DNEREUS_DEPENDENCY_ARCHIVE_DIR=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/archives \
    -DFETCHCONTENT_SOURCE_DIR_LIBSPECBLEACH_UPSTREAM=$D/libspecbleach_upstream-src \
    -DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$D/portaudio-src \
    -DFETCHCONTENT_SOURCE_DIR_RNNOISE_UPSTREAM=$D/rnnoise_upstream-src \
    -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
    -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
    -DENABLE_DFNR=OFF > .crew/configure.log 2>&1
  ```

  The archive directory's eight files match this branch's SHA-256 pins in
  `cmake/NereusDependencyArchives.cmake`. Any network fetch is reported, not
  worked around.
- No device work. Nothing is deployed to the Rock or any bench Core; Core
  installs belong to the Core session or JJ. Hardware verification (JJ keys a
  two-tone with PS Auto on and watches calCount and feedbackLevel rise) is
  reported as pending.
- The two-tone defaults themselves do not change: Freq1 700, Freq2 1900,
  Level 0, the rest as `TransmitModel` has them today.

## Task 1: Two-tone leaves TX profiles

**Behaviour.** Saving, loading, or switching a TX profile neither stores nor
restores any two-tone value. The live two-tone settings stay where they are
(`hardware/<mac>/tx/TwoTone*`, persisted by `TransmitModel`), are edited from
Setup as today, and are no longer part of a profile's "unsaved changes" watch.

**Changes.**

- `src/core/MicProfileManager.cpp`: remove the eight two-tone keys
  (`TwoToneFreq1`, `TwoToneFreq2`, `TwoToneLevel`, `TwoTonePower`,
  `TwoToneFreq2Delay`, `TwoToneInvert`, `TwoTonePulsed`,
  `TwoToneDrivePowerOrigin`) from `liveKeyList()`, the two-tone block of
  `defaultProfileValues`, `captureLiveValues`, and `applyValuesToModel`.
  Replace them with one comment citing why: Thetis TX profiles carry no
  two-tone fields (database.cs:4299 `AddTXProfileTable` [v2.10.3.15]); the
  level is a Setup value (setup.Designer.cs:62144-62173 [v2.10.3.15]). Update
  comments that count keys (for example "Two-tone (7) + drive-power source
  (1)", and any total of 108) and `MicProfileManager.h` comments that list
  two-tone as profile content. Add a Modification-history line.
- `resources/setup/audio.json`: remove the eight `twoTone*` names from the TX
  profile control's `unsavedChanges.watch` list (lines ~473-480). If the setup
  description carries a revision or checksum that must change with the file,
  change it the way earlier edits to this file did (`git log -p` on it).
- Tests that hold the old shape:
  - `tests/tst_mic_profile_manager.cpp`, `tests/tst_mic_profile_manager_cfc_round_trip.cpp:150`,
    `tests/tst_mic_profile_manager_dexp_round_trip.cpp:100`: default-key count
    108 becomes 100. Two-tone round-trip cases in `tst_mic_profile_manager.cpp`
    (around 247-261, 306-307, 506-513, 552 onward) change to assert the new
    contract: a profile save writes no `TwoTone*` key, and loading a profile
    that still holds old `TwoTone*` keys leaves the live two-tone values
    untouched.
  - `tests/tst_setup_description_live.cpp`: drop the eight two-tone entries
    from `kTxProfileWatchFields` (lines ~88-103), and add the two-tone names to
    the excluded list near line 382 so the watch can never grow them back.
    Regenerate `tests/data/link/v1/tx-profile-watch-60.json` with
    `NEREUS_TX_PROFILE_WATCH_FIXTURE=<path>` as that test supports.
  - iPhone mirror fixture: copy the regenerated fixture to
    `ios/NereusKit/Tests/NereusMirrorTests/Fixtures/TxProfileWatch60/tx-profile-watch-60.json`,
    remove the eight two-tone rows from `typed60-mapping.json` there, and
    change the counts in `ios/NereusKit/Tests/NereusMirrorTests/TxProfileWatch60Tests.swift`
    (60 at lines ~188, 189, 234) to the new field count. Keep the file and
    suite names; they name the fixture, not a count. Run
    `swift test --filter TxProfileWatch60Tests` in `ios/NereusKit`.
  - Any other test the grep finds asserting two-tone in a profile
    (`grep -rn "TwoTone" tests | grep -i profile`).

**Verification.** The tests above, red before and green after where they
assert new behaviour. Report each target's pass line.

## Task 2: Settings schema v10 upgrade

**Behaviour.** On first start after this change, once per settings file:

1. Every `hardware/<mac>/tx/profile/<name>/TwoTone*` key (the eight from
   Task 1) is removed. Nothing reads them any more.
2. Every `hardware/<mac>/tx/TwoToneLevel` whose saved value is exactly -6
   (parsed as a number, so `-6` and `-6.0` both match) becomes `0`. Any other
   value, including other negative levels a user chose, is kept.
3. `SettingsSchemaVersion` becomes 10.

**Changes.**

- `src/core/AppSettings.cpp` `ensureSettingsAtVersion`: a v9 -> v10 block in
  the house shape of the v9 block (`allKeys()` with a `QRegularExpression`,
  a `qDebug()` line per change, the "complete" line). Its comment says why:
  profiles saved before 74812ec8b carried -6, which halves the two-tone
  envelope and keeps PureSignal from calibrating; Thetis's default is 0
  (setup.Designer.cs:62168-62172 [v2.10.3.15]) and Thetis profiles carry no
  two-tone values (database.cs:4299 [v2.10.3.15]). Name -6 as a `constexpr`
  (the retired default) rather than a bare literal.
- `src/core/AppSettings.h:722-735` doc comment, `src/core/CoreInit.cpp:160-171`
  (comment plus the call becomes `ensureSettingsAtVersion(10)`), and the
  comment at `src/core/session/StationServer.cpp:2313-2316` that names the
  literal 9.
- New `tests/tst_settings_schema_v10_migration.cpp`, modelled on
  `tests/tst_settings_schema_v9_migration.cpp`, registered in
  `tests/CMakeLists.txt` beside it. Cases: profile two-tone keys removed for
  two MACs and two profiles while their non-two-tone keys survive; live -6
  and -6.0 become 0; live -12 and 0 are kept; a file already at v10 is not
  touched; the version key ends at 10. Update any existing test that asserts
  the final version is 9.

**Verification.** The new test, red before the block exists, plus
`tst_settings_schema_v9_migration` still green.

## Task 3: A remote transmit edit is saved to disk on the Core

**Behaviour.** When a remote app changes a persisted transmit setting (the
two-tone level from the iPhone, for example) and the Core accepts it, the
Core schedules its coalesced settings save, the same 500 ms save a slice edit
uses. Before this, `TransmitModel::persistOne` (TransmitModel.cpp:2157) only
updated the in-memory `AppSettings`, and nothing wrote the file until the Core
exited cleanly. Keying writes (`isTransmitKeyingProperty`:
StationServer.cpp:1751, mox / tune / voxEnabled / twoToneActive) do not
trigger a save.

**Changes.**

- `src/core/session/StationServer.cpp` `applyPropertyWrite` (7812, results
  built near 8150-8215): after the results are final, if any accepted result
  is on the `transmit` object and is not a keying property, call
  `m_radioModel->requestSettingsSave()`.
- Audit the other objects `applyPropertyWrite` accepts (for example
  `pureSignalSettings`) for the same gap: a model that persists through
  `AppSettings::setValue` with no save scheduled. Fix the ones that share the
  gap the same way and list every object checked, fixed or not, in the report.
- Test, in the harness `tests/tst_session_property_result.cpp` already uses
  (Core `RadioModel` + `StationServer`, a remote `StationClient` over a
  loopback transport): the remote sets `twoToneLevel` to a new value; after
  `station.flushPendingSettingsSave()`, `AppSettings::instance().load()` reads
  the new value back from the file. The same flow with a keying write
  schedules no save. Show the test failing before the fix.

**Verification.** The new test, plus `tst_session_property_result` and any
StationServer test target the change touches, green.
