# Recording, voice keyer and DSR: what has been checked

Revision checked: `e14ff77f2` on branch `claude/recording-voice-keyer-dsr` (draft PR 797), 2026-11-16, macOS
(clang) only. Spec: [2026-11-15-recording-voice-keyer-dsr-design.md](2026-11-15-recording-voice-keyer-dsr-design.md),
section 8.

Layers: **S** software test on this machine, **I** integration (station loopback test or UI capture),
**H** on JJ's radios or a bench Core. Every H row below says "pending, JJ". Nobody has run any of this on a
radio, a Core on the bench, a phone or an iPad. Hardware tasks 20, 27 and 29 wait for a branch Core on the bench
Pi 5 at .66.

## Run for this document

One combined build and one ctest pass through the lane's load gate: 38 ctest entries (27 test names, some split
into several entries), 0 failed. The names:

* Required by the plan: `tst_core_has_no_gui_includes`, `tst_unbuilt_features`, `tst_remote_gui_gating`,
  `tst_link_surface_manifest`, `tst_link_surface_manifest_regen`, `tst_hot_key_bindings`. The brief names
  `tst_link_surface`; no such test exists. The surface test is `tst_link_surface_manifest`.
* Added by this plan: `tst_recording_store`, `tst_recording_quiet_rule`, `tst_recording_contact`,
  `tst_recording_player`, `tst_recording_on_air`, `tst_recording_audio_files`,
  `tst_recording_audio_files_no_lame`, `tst_recordings_link`, `tst_recordings_access`,
  `tst_recordings_panel_window`, `tst_recordings_setup_page`, `tst_flag_record_play`, `tst_voice_keyer`,
  `tst_voice_keyer_desktop`, `tst_dsr_ring`, `tst_dsr_recorder`, `tst_dsr_rewind`, `tst_dsr_past_rows`,
  `tst_dsr_desktop_rewind`, `tst_dsr_save`, `tst_dsr_setup_page`.

The full `core` and `gui` label runs were not done; the full suite runs on GitHub when the PR is marked ready.

Repo gates, each run once, all pass: `verify-thetis-headers.py` (513/513), `verify-inline-tag-preservation.py`,
`verify-inline-cites.py`, `check-new-ports.py` (166 files), `verify-provenance-sync.py` (446/446),
`check-third-party-licenses.py` (33 libraries), `compliance-inventory.py --fail-on-unclassified`,
`verify-test-registration.py`, `generate-contributor-indexes.py --check`, `render-link-tables.py --check`,
`verify-no-gui-dsp-access.py`, `verify-ios-provenance.py`, `verify-ios-workspace-colours.py`.

Phone and iPad tests were run in their own tasks (package, app and shot tests on the iPhone 17 and iPad Air 13-inch
M4 simulators) and not repeated here; only the two iOS repo gates above were run again.

## Acceptance table

State: passed means the named tests passed in the run above (or, for the phone, in the task that ran them);
pending means nothing has run it.

| # | Layer | Tests | State | Not proven |
| --- | --- | --- | --- | --- |
| A1 | I | `tst_recordings_link` (anyDeviceStartsAndAnotherStops, aDroppedLinkLeavesTheRecordingRunning), `tst_flag_record_play`, `tst_recording_contact` (stopFromAnotherDeviceSaysWhich); phone: RecordingFlagTests, RecordingsClient suites | I passed | the one-second phone update on a real network |
| A1 | H | desktop flag and phone together on a Core | pending, JJ | |
| A2 | S | `tst_recording_contact` (togetherIsMonoWithBoth, apartIsReceiveLeftAndYouRight, keepMyVoiceOffLeavesTheMonitorOut, monitorCountsOnlyWhileMoxIsOn) | S passed | |
| A2 | H | listen to a recording of your own transmit | pending, JJ | |
| A3 | S | `tst_recording_contact` (startByItselfArmsAndMoxStartsTheTransmitSlice, quietStopKeepsAndRearms, quietWithoutStartByItselfGoesIdle), `tst_recording_quiet_rule` (6 cases) | S passed | stop-by-itself with squelch off and with FM squelch on, on a radio |
| A4 | S, I | `tst_recording_store` (baseNames, sameMinuteGetsANumber, renameChangesNameAndFile, removeDeletesFilesAndRows), `tst_recordings_link` (renameAndDeleteReachEveryDevice), `tst_recordings_panel_window` | S, I passed | the call taken from a logbook: no log has been tried, so only the call from the room and the empty case are covered (tst_recording_contact call* cases) |
| A5 | I | `tst_recordings_link` (eachDeviceGetsItsOwnPlayback), `tst_recording_player` | I passed | |
| A5 | H | play on the phone, nothing on the desktop speaker | pending, JJ | |
| A6 | S | `tst_recording_on_air` (keyingSendsThePlaybackFromWhereItPlays, anotherDeviceKeyingSendsItsMic, theEndWhileKeyedHandsOverWithinOneBlock, aSeekOnTheAirSendsNoMicrophone), `tst_recordings_link` (theKeyingDeviceIsToldItsPlaybackIsOnTheAir) | S passed | |
| A6 | H | on-air check of the fixed level (app `293ae156e`) | pending, JJ | |
| A7 | S | `tst_recording_on_air` (theSwitchesAreOffOnTheAirAndComeBackAfter, aSwitchSetToUseKeepsTheUsersValue, aChangeOnTheAirIsKeptAndAppliedAtTheEnd, aProfileChangeOnTheAirKeepsTheSwitchesOff, stoppingOnTheAirEndsItAndRestores) | S passed | |
| A8 | S | `tst_voice_keyer` (sendKeysAsTheRequesterAndUnkeysAtTheEnd, stopUnkeys, repeatWaitsAndSendsAgainUntilStopped, theSendersPttHandsTheAirToItsMic, inCwASendIsRefusedAndNothingKeys), `tst_voice_keyer_desktop`, `tst_hot_key_bindings`; phone: RecordingsClient and voice message tests | S passed | recording M1 from the desktop mic and the phone mic on a real mic |
| A8 | H | record M1 on both mics, send, Repeat, Stop, Escape, PTT, MOX, F1 to F8 | pending, JJ | |
| A9 | S | `tst_voice_keyer` (theFirstEmptyMessage, aStereo44kImportIsConvertedTo48kMono, anImportOverTwoMinutesIsRefused, anExportLoadsBackTheSameByteForByte), `tst_recording_audio_files` (wavMonoRoundTrip, wavStereoKeepsTheSides) | S passed | |
| A10 | S | `tst_dsr_ring` (block61WritesOverBlock1, aHundredMegabyteLimitErasesTheOldest, oneSecondFromTheMiddleReadsBack, aRestartFindsItsPlace), `tst_dsr_recorder` (sixtyFakeSecondsAreSixtyBlocks) | S passed | a real radio's rate and frequency on the bench Core |
| A11 | I | `tst_dsr_rewind` (rewindFeedsTheSliceFromTheRingAndLeavesTheRestLive, liveReturnsToTheRadioAndTheFrequencyBefore, pttWhileRewoundGoesLiveOnThePlayedFrequency), `tst_dsr_desktop_rewind` (aClickOnAPastSignalRewinds, liveAndPushToTalk); phone: rewind suites | I passed | |
| A11 | H | rewind on the desktop and phone | pending, JJ | |
| A12 | I | `tst_dsr_past_rows` (rowsComeNewestFirstAndOnlyWhatIsKept, rowsComeInChunks, threeKeptHoursShadedByActivity, rowsComeReducedToThePansPixels), `tst_dsr_desktop_rewind` (aDragPastTheWindowsMinutesAsksTheCore, theHoursMapAndATypedTimeJump) | I passed | remote past rows lack the Core's display calibration offset, use the Peak detector only, and a fast drag fills one request behind (known, from the final review) |
| A12 | H | hours beyond 20 minutes redraw on a Core | pending, JJ | |
| A13 | S | `tst_dsr_save` (aWholePanSaveIsTheStretch, soundOnlyIsSliceASound, saveSoundOnlyFromAWholePan, anOpenedStretchReplaysWithTheRecorderOff, anOpenedStretchEndsWithTheRadioOrSliceA, theWindowOpensAndSavesSoundFromAWholePan) | S passed | sound-only saves start with about 100 ms of silence (the WDSP delay is not trimmed) |
| A13 | H | save both kinds and replay the whole-pan stretch | pending, JJ | |
| A14 | I | `tst_dsr_recorder` (anSdCardIsNeverUsed, onByDefaultAtTenGigabytesOnTheCoresOwnDisk), `tst_recording_store` (sdCardDevices), `tst_dsr_setup_page` (anSdCardOnlyCoreDisablesTheSwitch) | I passed | |
| A14 | H (Pi) | SD-card-only Pi 5 Core at .66 | pending, JJ (waits for the branch Core) | |
| A15 | I | `tst_flag_record_play` (not_connected_and_an_older_core_disable_both_with_the_reason), `tst_recordings_access` (aRemoteWindowOnAnOlderCoreIsToldAndSendsNothing), `tst_dsr_setup_page` (anOlderCoreOrNoConnectionDimsTheGroups), `tst_dsr_desktop_rewind` (anOlderCoreOrTheRecorderOffKeepsTheStripAsItWas), `tst_voice_keyer_desktop`, `tst_remote_gui_gating`, `tst_unbuilt_features`; phone: older-Core reason tests | I passed | |
| A16 | I | UI captures, listed below | I captured and compared in each task | not re-taken here; the phone and iPad flag states are shots, not run on a device |
| A17 | S | `tst_dsr_recorder` (theTapNeverWaitsForTheWriter), `tst_recording_contact` (tapsNeitherAllocateNorBlock), `tst_recording_on_air` (pullNeitherAllocatesNorWaits), `tst_dsr_ring` | S passed | a timing run on a Core under a full DSR load has not been done; it needs the bench Core |

## Captures (A16 and the other new scenes)

Taken and compared against the mockups in the tasks that built them, not taken again for this document.

* Desktop: `flag-recplay--*` (idle, recording, recording-myvoice, armed, playing, kept, older-core, menu), `voice-button`,
  `voice-keyer-panel`, `recordings-panel`, `recordings-window`, `recordings-setup`,
  `look-dialog-setup-recordings`, `dialog-setup-recordings`, `dsr-setup`, `look-dialog-setup-digital-spectrum-recorder`,
  `dialog-setup-digital-spectrum-recorder`, `dsr-rewind--past`, `--hours`, `--drawing`, `--typed-time`, `--older-core`,
  `dsr-save--bracket`, `dsr-save--box`.
* iPhone (iPhone 17 simulator shots): flag states, Recordings list and settings pages, voice keyer, rewind and save pages.
* iPad (iPad Air 13-inch M4 simulator; the iPad Pro 13-inch M5 simulator would not launch the app): `recordingsShots`, nine shots.

Mockup differences found and settled in those tasks are in the task reports and in the PR's veto notes.

## Build checks

* `nereusd` builds through the load gate (6 ninja steps, linked). It links `libNereusCore.dylib`, and `otool -L` shows
  that library linked to `libmp3lame` (the Homebrew LAME found at configure time). macOS only.
* A window running its own radio records: the recording service takes the host `Local` and `nereusd` takes `Core`
  (`tst_recording_store` dsrDefaultFollowsTheHost, `tst_dsr_recorder` theDefaultFollowsWhereItRuns). Task 10b.
* Only macOS clang has compiled this branch. Windows, Linux and the Pi builds, and the Intel macOS LAME 3.100
  source build, wait for GitHub.

## Still unproven

* Everything marked H above.
* The call taken from a logbook (A4).
* Stop-by-itself on a radio with squelch off, and with FM squelch on.
* A timing run on a Core under a full DSR load (A17).
* Windows, Linux and Pi compilation and the LAME paths on those systems.
