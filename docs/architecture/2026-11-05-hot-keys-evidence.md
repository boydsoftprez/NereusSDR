# Hot keys: evidence

Status: built. Every row below is checked against the tests and the implementer reports. Nothing here has run on a real
device, a real plugin host or a hardware keyboard on JJ's radio.

Design: [2026-11-05-hot-keys-design.md](2026-11-05-hot-keys-design.md), section 11. Plan:
[2026-11-05-hot-keys-plan.md](2026-11-05-hot-keys-plan.md).

## How to read this

Layers:

* **Unit**: logic with fakes and an injected clock.
* **UI**: offscreen windows, real widgets and Setup pages, driven by test events.
* **Integration**: a real local socket, with the plugin clients as real child processes. A stub stands in for the plugin
  host; no real Stream Deck, Ulanzi or StreamController app took part.
* **Hardware**: needs a real device, a real plugin host or JJ's radio. None has run.

States:

* **passed**: the named task ran the test on this Mac, offscreen with a scratch home, and it passed. Only the tests for
  code a task changed were run, so a task number is the last run recorded in a report, not a rerun for this document.
* **pending**: nothing has run it, because it needs a device, a plugin host, or a first Windows or Linux compile.

Every test below exists in `tests/` and is registered in `tests/CMakeLists.txt` (checked with grep for this document;
nothing was built or run to write it). Rows with a hardware part read "passed" for the simulated part and "pending" for
the rest.

## Table

| ID | Test | Layer | State |
|---|---|---|---|
| R-HK-01 | `tst_hot_key_router::everyDefaultChordRunsItsAction`, `::everyDefaultChordComesBackFromItsEvent`; `tst_hot_key_bindings::freshSettingsUseTheDefaults`, `::listMatchesThePlanTable`; `tst_hot_key_setup_knobs::theRowsMatchTheDefaults` | UI, unit, hardware | passed (Task 7, Task 1, Task 18); hardware keyboard run on JJ's radio pending |
| R-HK-02 | `tst_hot_key_dispatcher::offSwitch_callsNoTarget`, `::offSwitch_duringHoldDropsItOnce`; `tst_hot_key_router::offEatsNoKeyAndActsOnNothing`, `::goingOffWhileHeldForgetsTheKey`; `tst_hot_key_setup_keyboard::theSwitchAndTheViewItemMoveTogetherAndStopKeys`; `tst_midi_profile::routing_offSwitchStopsItAndForgetsWhatIsDown`; `tst_deck_link::theGlobalSwitchMeansMessagesReachNothing`; `tst_device_input::theOffSwitchStopsEverything` | unit, UI | passed (Tasks 2, 7, 8, 10, 12, 19) |
| R-HK-03 | `tst_hot_key_router::mainAndFloatingWindowsAct`, `::otherWindowsKeepTheirKeys` (Setup, dialog, Roundtable, amplifier chart, any other window; its body drives a tree, table, tab bar and slider), `::textWidgetsKeepEveryKey`, `::noKeyActsUnderAModalDialogOrWithNoActiveWindow`; `tst_hot_key_window_actions::disabledKeysShowTheirReasonAndChangeNothing` | UI | passed (Task 7, Task 3) |
| R-HK-04 | `tst_hot_key_router::aClickedControlKeepsNothing`, `::aControlReachedWithTabKeepsArrowsAndSpace`, `::aTabbedControlStaysSoAcrossAWindowSwitch`, `::aFocusedHandleKeepsTheArrowsDuringAHold` | UI, hardware | passed (Task 7); real focus on a real slider pending |
| R-HK-05 | `tst_hot_key_router::aSpectrumClickLeavesTheBoxesTheirFocusAndKeys` (CWX and Roundtable boxes) | UI, hardware | passed (Task 7); a real mouse click on the spectrum pending |
| R-HK-06 | `tst_hot_key_dispatcher::slice_noPinUsesTheWindowSlice`, `::slice_bindingPinActsOnThatSlice`, `::slice_closedPinRunsNothingAndSaysSo`, `::listened_refuses`, `::lock_refuses`; `tst_hot_key_window_actions::aPinnedKeyActsOnItsSliceAndAClosedLetterSaysSo`, `::hostingWindowListensToAnotherDevicesSlice`; `tst_hot_key_remote_actions::aListenedSliceRefusesTuneAndVolumeAsksForThisDevicesLevel` | unit, UI | passed (Task 2, Task 3) |
| R-HK-07 | `tst_hot_key_window_actions::theSelectedFlagChoosesBothSliceAndPan`; `tst_hot_key_remote_actions::theSelectedFlagChoosesBothSliceAndPan` | UI | passed (Task 3) |
| R-HK-08 | `tst_hot_key_dispatcher::ledger_pressKeysAndReleaseUnkeys`, `::ledger_twoSourcesLastReleaseUnkeys`, `::ledger_otherSourceReleaseDoesNotCount`, `::sleep_aGapDropsEveryHoldFromEverySource`; `tst_hot_key_router::eachFailsafeDropsAHoldOnceAndLeavesMoxOn`, `::aReleaseWithTheModifierLiftedFirstStillReleases`, `::sleepDropsAHoldAndTheKeyBehavesAfterWake`; `tst_hot_key_transmit::localHoldDropsOnFailsafe`, `::anotherDevicesKeyIsNotThisWindowsTransmit`; `tst_midi_profile::unplug_dropsTheHoldAndComesBack`; `tst_device_parser_rc28::aHoldSeesItsReleaseWithAnotherButtonDown` | unit, UI, hardware | passed (Tasks 2, 4, 7, 10, 13 and the first-look and sleep fixes); a real sleep, a real unplug and OS key events pending |
| R-HK-09 | `tst_hot_key_router::noTransmitChordKeysFromATextFieldADialogOrAnInactiveWindow`, `::noKeyActsUnderAModalDialogOrWithNoActiveWindow` | UI, hardware | passed (Task 7); a real inactive-window run pending |
| R-HK-10 | `tst_hot_key_transmit::connectingNeverKeys`, `::aHoldNeedsARadio` | unit | passed (Task 4; transmit suite rerun in Tasks 7, 12 and the first-look fixes) |
| R-HK-11 | `tst_hot_key_transmit::aTimeOutClearsTheClaimSoALaterMoxSurvivesTheRelease`, `::txInhibitRefusesThroughTheMoxToast`, `::aRefusedGateRefusesThroughTheMoxToast`, `::aRefusedRemoteHoldSaysItWentOff`, `::aHostedHoldKeysThroughTheController`; `tst_hot_key_dispatcher::ledger_refusedKeyHoldReleaseDoesNothing` | unit, UI | passed (Task 4, Task 2). MIDI, knob and deck presses reach the same dispatcher and controller; no test fires the gate from each of those sources alone |
| R-HK-12 | `tst_hot_key_bindings::conflictChangesNothingUntilReplaced`, `::replaceTakesOnlyTheOneKey`, `::transmitActionsRefusePlainKeys`, `::latchedActionsRefuseSpace`, `::reservedKeysNameTheMenuItem`, `::invalidBindsAreRefused`, `::importRefusesEachKindOfBadLine`; `tst_hot_key_setup_keyboard::eachRefusalShowsItsReasonAndChangesNothing`, `::aConflictOffersReplaceOrCancel` | unit, UI | passed (Task 1, Task 8) |
| R-HK-13 | `tst_hot_key_cw::commaAndPeriodDriveTheSoftBits`, `::slashIsTheHandKey`, `::inAVoiceModeTheKeysDoNothingAndSayNothing`, `::remoteDotAndDashReachTheCoresKeyLine`, `::remoteStraightKeyAndMessagesReachTheCore`, `::remoteOnAVoiceModeTheKeysSendNothing`; `tst_remote_cw_paddle::dotAndDashSetOnlyTheSoftBitsAndWatchBeforeTheFirstBit`; `tst_cw_soft_paddle` | unit, UI, hardware | passed (Task 5, Task 6, first-look fixes); keying a real radio in CW pending |
| R-HK-14 | `tst_hot_key_dispatcher::burst_tenPressesLimitedAndNoStepLost`, `::burst_twoSlicesKeepSeparateLimits`, `::coalescer_sumsOppositeSteps`; `tst_hot_key_router::separateFastPressesGoThroughTheSameLimitWithNoStepLost`, `::repeatAddsToTunesAndLevelsAndNothingElse` | unit, UI | passed (Task 2, Task 7); proved with a held fake clock, so no timing or load is involved |
| R-HK-15 | MIDI: `tst_midi_decoder::relative_center64LocksOnItsDetents`, `::relative_twosComplementLocksOnItsDetents`, `::message_controlChangeIsAbsolute`, `::message_pitchBend`; `tst_midi_profile::routing_relativeKnobTurns`, `::routing_levelFaderMovesItsRangeOver20Steps`. Parsers: `tst_device_parser_rc28::rotationSignAndSpeed`, `tst_device_parser_powermate::rotationIsSigned`, `tst_device_parser_shuttle::theRingIsSignedAndLimited`, `tst_device_parser_tmate2::aKeyPressAndItsRelease`, `tst_device_parser_streamdeck::aKeyDownAndUp`, `tst_device_parser_flexcontrol::theWheelAndItsSpeeds`, `tst_device_parser_ulanzi::everyControlHasItsKey` | unit (simulated input) | passed (Tasks 10, 13, 14, 15, 16). Input is built from documented layouts; every device stays untested on real hardware |
| R-HK-16 | `tst_deck_link::helloGetsEveryActionTheStateAndTheDisplay`, `::aPressReachesTheDispatcherWithItsSourceAndPin`, `::aStateChangeAndADisplayChangeArrive`, `::aHoldDropsHoweverTheConnectionEnds`, `::theClassUsesOnlyALocalSocket`, `::onlyThisUserCanReachIt`; `tst_deck_plugin_clients::aRealClientDrivesTheLinkAndKillingItReleases` (node and python3 clients, SIGKILL releases the hold); `tst_deck_plugin_actions::theActionListFileMatchesTheActions` | unit, integration | passed (Task 19, Task 20). The clients ran against a stub host, not a real plugin host. The JavaScript and Python clients have not run on Windows or Linux |
| R-HK-17 | `tst_hot_key_setup_keyboard::everyDefaultAndReservedKeyIsOnTheMap`, `::theMapFitsEveryWidth`; `tst_hot_key_setup_midi::choosingADeviceSavesAndOpensIt`; `tst_hot_key_setup_knobs::theEightDevicesAreListedAndUntested`; `tst_hot_key_setup_deck::threePluginsEachWithInstallStateAndUntested`; `tst_setup_pages_fit_window`; `tst_workspace_colour_sweep` | UI, hardware | passed for the page tests (Tasks 8, 11, 18, 21). Page captures were looked at against the mockup by the implementers; JJ's own look in the Setup look is pending. The colour sweep scene groups `_scenes1` to `_scenes6` last ran in the CI-fix round; later pages are left to GitHub's run |
| R-HK-18 | `tst_core_has_no_gui_includes::coreNeverIncludesGui`, `::coreNeverIncludesDeviceLibraries`, `::coreSourcesListNoDeviceLibrary`, `::nereusdLinksNoDeviceLibrary` | unit | passed (Task 1, Task 10, Task 18). The link check reads `nereusd` where the build has one |
| R-HK-19 | `tst_hot_key_device_bindings::noDeviceIsMarkedTested`; `tst_hot_key_setup_knobs::noStateCanChangeUntested`; `staysMarkedUntested` in each of `tst_device_parser_rc28`, `_powermate`, `_shuttle`, `_tmate2`, `_streamdeck`, `_flexcontrol`; `tst_hot_key_setup_midi::aControllerStartsEmptyAndIsUntested`; `tst_deck_plugin_actions::everyPluginSaysUntestedAndCarriesItsLicenseNote` | unit, UI | passed (Tasks 13 to 16, 18, 20). Ulanzi has no `staysMarkedUntested` case; `tst_hot_key_device_bindings::noDeviceIsMarkedTested` and `tst_hot_key_setup_knobs::noStateCanChangeUntested` cover it |

| VOX (`vox.toggle`, JJ's request of 2026-11-07; no R-HK number) | `tst_hot_key_bindings::listMatchesThePlanTable`, `::exportFormat`; `tst_hot_key_dispatcher::scope_transmitActionsIgnoreSliceRefusals`, `::turn_neverKeys`; `tst_hot_key_transmit::localVoxIsTheTxAppletsButton`, `::aHostedVoxWhileAnotherDeviceHoldsSaysWhoAndStaysOff`, `::aRemoteWindowWithoutPermissionRefusesVoxInTheCoresWords`; `tst_hot_key_state_feed::everyActionFromTheBrief`, `::transmitStatesComeFromTheSource`, `::windowVoxFollowsTheTxAppletsButton`; `tst_hot_key_setup_keyboard::theListSearchesNamesAndKeys` (row count); `tst_deck_plugin_actions` | unit, UI, hardware | see the VOX run below. A remote window arming VOX through a live microphone line has no test: the remote harness opens no line, so only the refusal is shown. A real deck key lighting for it is pending |

No R-HK row lacks a test.

## Still unproven

* Nothing has run on a real device or a real plugin host. MIDI controllers, the RC-28, PowerMate, Shuttle, TMate 2,
  Stream Deck+, FlexControl, the Ulanzi dial, and the Elgato, Ulanzi and StreamController plugins are checked only
  against simulated input built from documented layouts, or against stub hosts.
* Windows and Linux lines first compile when the PR is marked ready. The last head GitHub checked was `b986ede56`.
  Everything added after it, including the HID, serial and Ulanzi backends for those systems, has been compiled only on
  macOS.
* A hardware keyboard run on JJ's radio is pending. That covers real OS key events, the OS inactive state, a hold on a
  Cmd chord, a real sleep while a key is held, a real unplug, and holding Space while the menu bar is clicked.
* No Input Monitoring prompt at launch with no device attached: checked only with a test binary, not the app itself.
* Real USB, serial and MIDI open paths (`RealHidLayer`, `RealSerialLayer`, the Linux and Windows backends) have no test,
  because opening them needs a device.
* JJ's look at the Setup pages, the lit key colors, the strip and the key labels is pending.

## Notes on what the tests show

* A pass on any device parser shows the code agrees with the documented layout. It does not show the device sends it.
* The off-Mac line of `tst_hot_key_router::menuShortcutsStillWork` (Ctrl+M reaches its menu item) cannot be driven on
  macOS offscreen. It passed on Linux in GitHub's run at head `2806394fd`.
* Mutation checks were run in Tasks 6, 15, 16, 17 and 19 and in the sleep failsafe, before JJ cut them back on
  2026-11-06. They are not part of the evidence above.
