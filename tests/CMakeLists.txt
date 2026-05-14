# NereusSDR test suite. Opt-in via -DNEREUS_BUILD_TESTS=ON.
#
# Test targets compile against NereusSDRObjs (the OBJECT library that backs
# the main executable) so behavior under test is byte-identical to shipped
# code. Each test gets its own thin executable + add_test() registration.

find_package(Qt6 REQUIRED COMPONENTS Test)

function(nereus_add_test name)
    # Additional source files may be passed as extra arguments (ARGN) —
    # needed by Phase 3I tests that link test-only fakes like P1FakeRadio.
    #
    # TestSandboxInit.cpp is always compiled in. Its global static
    # constructor calls QStandardPaths::setTestModeEnabled(true) before
    # main() runs, so AppSettings::instance() resolves to a sandbox
    # path instead of the real user config file. Without this, ctest
    # runs can (and have — see v0.1.1 alpha) silently wipe the
    # developer's NereusSDR.settings via P1FakeRadio-keyed test writes.
    add_executable(${name} ${name}.cpp TestSandboxInit.cpp ${ARGN})
    target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)

    # Reuse the NereusSDRObjs precompiled header (CMakeLists.txt §PCH).
    # 451 test executables × per-target PCH compile would burn the wins;
    # REUSE_FROM means the PCH is built once on NereusSDRObjs and shared
    # across every test target. Save 5 to 10 minutes of test compile
    # time on Linux CI. Gated on NEREUS_USE_PCH so users who turn the
    # PCH off (e.g. for header-hygiene auditing) don't get a broken
    # link.
    if(NEREUS_USE_PCH)
        target_precompile_headers(${name} REUSE_FROM NereusSDRObjs)
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Pre-existing tests from main (smoke + 3G-9 meter suite) ────────────────
nereus_add_test(tst_setup_helpers)
nereus_add_test(tst_smoke)

# ── Issue #75: receiver leak across disconnect/reconnect ──────────────────
nereus_add_test(tst_receiver_manager_reconnect)

# ── HL2 connect-init parity: spectrum DDC center push (Bug 1) ─────────────
# Pins the contract that RadioModel::loadSliceState() emits
# sliceStateRestored(idx) at completion, mirroring Thetis
# txtVFOAFreq_LostFocus's unconditional state push at power-on
# (console.cs:27204 [v2.10.3.13]).  Without this hook the spectrum widget's
# m_ddcCenterHz stays at the slice's pre-restore default in CTUN mode.
nereus_add_test(tst_radio_model_slice_state_restored)

# ── Issue #153 sub-bug 2: SSB MOX TXA seeding ────────────────────────────
# Pins the contract that RadioModel::pushTxModeAndBandpass() emits
# txModeAndBandpassPushed(mode, audioLow, audioHigh) with the active
# slice's current DSPMode + filter cutoffs.  Wired into createTxChannel,
# SliceModel::dspModeChanged, and MoxController::txAboutToBegin so SSB
# MOX no longer requires a prior TUN press to seed TXA mode + bp0.
nereus_add_test(tst_radio_model_push_tx_mode_and_bandpass)
nereus_add_test(tst_container_persistence)
nereus_add_test(tst_meter_item_bar)
nereus_add_test(tst_reading_name)
nereus_add_test(tst_meter_item_scale)
nereus_add_test(tst_meter_presets)

# ── Phase 3M-1c G.1 / G.2: VfoDisplayItem TX-state badge + Phase L routing ──
# Verifies the existing 3G-8 widget machinery (setTransmitting / isTransmitting
# round-trip + paint() colour-swap rendered to QImage) AND demonstrates the
# Phase L wiring lambda that will dispatch MoxController::moxChanged(int rx,
# bool, bool) to per-VFO setTransmitting based on the rx index semantic from
# pre-code review §6.5 ((rx2_enabled && vfobTx) ? 2 : 1).  Routing tests connect
# directly to MoxController and drive setMox to exercise the Pre/Post emit
# walk completion.
nereus_add_test(tst_vfo_display_item_tx_badge)

# ── Phase 3G-9b clarity defaults invariants ───────────────────────────────
nereus_add_test(tst_clarity_defaults)

# ── Phase 3G-9c Clarity adaptive display tuning ──────────────────────────
nereus_add_test(tst_noise_floor_estimator)
nereus_add_test(tst_noise_floor_tracker)
nereus_add_test(tst_clarity_controller)

# ── Task 2.10: per-band NF-estimate priming (NereusSDR-original) ─────────
nereus_add_test(tst_per_band_nf_priming)

# ── Task 2.11: Clarity + NF-aware grid coexistence (NereusSDR-original) ────
nereus_add_test(tst_clarity_nf_grid_coexistence)

# ── Phase 3I radio-connector tests ────────────────────────────────────────
nereus_add_test(tst_hpsdr_enums)
nereus_add_test(tst_board_capabilities)

# ── Phase 3M-4 Task 1: PureSignal cap fields (psDefaultPeak + psSampleRate) ──
# 3 tests covering ANAN-G2 (P2 default 0.2899, psSampleRate=192000),
# HermesLite (mi0bot HL2 override 0.233, psSampleRate=0 sentinel for
# rx1_rate per mi0bot console.cs:8472-8488), and Atlas (no PureSignal).
# Source: ramdor clsHardwareSpecific.cs:285-310 [v2.10.3.13] +
#         mi0bot clsHardwareSpecific.cs:300-330 [v2.10.3.13-beta2].
nereus_add_test(tst_puresignal_caps)

# ── Phase 3M-4 Task 2: PureSignal calcc.c + iqc.c vendored verbatim ──
# Smoke test: confirms calcc.c (1,164 LOC) + iqc.c (315 LOC) are picked up
# by the third_party/wdsp/CMakeLists.txt GLOB and that the SetPS* public
# API exported by calcc.c is reachable through wdsp_static (which is
# PUBLIC-linked into NereusSDRObjs). Does NOT exercise DSP behavior;
# that requires an opened channel and is covered by Tasks 3+.
# Source: Thetis wdsp/calcc.c:891-1132 [v2.10.3.13] @501e3f51.
nereus_add_test(tst_wdsp_ps_smoke)

# ── Phase 3M-4 Task 3: TxChannel PureSignal API wrappers (22 functions) ──
# Smoke tests for the 22 PS API wrappers added in Task 3:
#   19 calcc setters/readers (setPSRunCal, setPSMox, setPSReset, setPSMancal,
#     setPSAutomode, setPSTurnon, setPSControl, setPSLoopDelay, setPSMoxDelay,
#     setPSTXDelay → returns double, setPSHWPeak, getPSHWPeak, getPSMaxTX,
#     setPSPtol, getPSDisp → 7 buffers, setPSFeedbackRate, setPSPinMode,
#     setPSMapMode, setPSStabilize, setPSIntsAndSpi, getPSInfo → int[16])
#   plus 2 static channel-routing helpers (setPSRxIdx, setPSTxIdx) for the
#     Stream0/Stream1 RX/TX feedback wiring per cmaster.cs:533-534.
# Same does-not-crash strategy as B-1/B-2/B-3 PS smoke tests.  Wrappers
# null-guard on `txa[m_channelId].rsmpin.p == nullptr` so calls on a bare
# `TxChannel ch(1)` are safe in HAVE_WDSP-linked builds.
# Source: Thetis wdsp/calcc.c:891-1132 [v2.10.3.13] +
#         Thetis cmaster.cs:143-147 [v2.10.3.13].
nereus_add_test(tst_tx_channel_ps_setters)

# ── Phase 3M-4 Task 4: PsFeedbackChannel wrapper (WDSP feedback RX channel) ──
# 4 tests covering channel-id uniqueness, sample-rate round-trip, activity
# counter, and distinctness from the existing TX channel id (1).  The
# PsFeedbackChannel is the host-side handle to the WDSP feedback RX channel
# that calcc reads autonomously via pscc() (calcc.c:617 [v2.10.3.13]).
# WdspEngine owns one instance, opened during initialize() (or via
# openPsFeedbackChannelForTesting() in the test path) at channel id 5 per
# the wdsp-integration.md §11.1 documented design.
# Source: NereusSDR-original wrapper.  No Thetis source; Thetis manages
#         WDSP channels via ChannelMaster.dll (see ps_sync_stub.c).
nereus_add_test(tst_ps_feedback_channel)

# ── Phase 3M-4 Task 7: PureSignal coordinator class ──────────────────────────
# Cal lifecycle (singleCalibrate / setAutoCalEnabled / reset / setEnabled) +
# MOX integration (onMoxChanged → SetPSMox forward) + auto-attention 3-state
# machine (Monitor → SetNewValues → RestoreOperation → Monitor) + status
# polling (pollTimerTick → getPSInfo + Q_PROPERTY signal emission) +
# save/restore (psSaveCorr / psRestoreCorr forwarding) + two-tone wiring +
# UI mirror state (invertRedBlue / hideFeedback) + per-board capability
# application (psDefaultPeak / psSampleRate fan-out).
# Ports the host-side logic from Thetis PSForm.cs timer1code (PSForm.cs:555-
# 728 [v2.10.3.13]), ForcePS (PSForm.cs:920-955), and the eAAState 3-state
# auto-attention machine (PSForm.cs:97-122 + 729-790 [v2.10.3.13]).
# NEREUS_BUILD_TESTS exposes lastPSFeedbackRateForTest seam used by the PR
# #212 codex-fix A test (HL2 sentinel resolution to cmaster.cs:424 ps_rate).
nereus_add_test(tst_puresignal_coordinator)
target_compile_definitions(tst_puresignal_coordinator PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-4 Task 8: PsForm modeless dialog ────────────────────────────────
# Source-first port of Thetis PSForm.cs (1,164 LOC) [v2.10.3.13].  Tests verify
# all 23 designer controls exist by objectName, defaults match Thetis verbatim,
# Advanced toggle collapses/restores body widgets, and every action button
# routes to the PureSignal coordinator (Task 7).  Save button gating mirrors
# Thetis PSForm.cs:574-590 [v2.10.3.13] CorrectionsBeingApplied predicate.
nereus_add_test(tst_psform)

# ── Phase 3M-4 Task 9: AmpViewWindow modeless dialog ─────────────────────────
# Source-first port of Thetis AmpView.cs (528 LOC) [v2.10.3.13].  Tests verify
# title "AmpView 1.0" verbatim, ClientSize 564x401 / MinSize 440x380, 4 toolbar
# checkboxes exist by objectName at exact Thetis x positions
# (chkAVShowGain @ 7,378 / chkAVPhaseZoom @ 242,378 / chkAVLowRes @ 404,378
# / chkStayOnTop @ 490,378), 5 named chart series (Ref / MagAmp / PhsAmp /
# MagCorr / PhsCorr), default chkAVLowRes Checked per AmpView.Designer.cs:
# 182-183 [v2.10.3.13], toggle persistence to AppSettings, stay-on-top window
# flag application, chart axis ranges (Mag 1.0/2.0 + Phase 180/45) matching
# AmpView.cs:435-482 [v2.10.3.13], and lowResStride() == 4 per AmpView.cs:
# 457-463 [v2.10.3.13].
nereus_add_test(tst_ampview_window)

# ── Phase 3M-4 Task 10: PsaIndicatorWidget bottom-banner FB+PS pair ──────────
# Source-first port of Thetis ucInfoBar.cs:820-1098 [v2.10.3.13] (lblFB +
# lblPS sub-controls).  Tests cover the full 6-state machine (PS off /
# RX idle / TX hide-feedback / TX numeric / TX correcting / TX corrections-
# applied not-correcting), the 4 color thresholds with default + swapped
# variants (PSForm.cs:1123-1138 [v2.10.3.13]), left/right click handlers
# routed to invertRedBlueRequested / hideFeedbackToggleRequested
# (ucInfoBar.cs:1042-1054 [v2.10.3.13]), passive PS label (no signals on
# click), compact-fonts threshold for "FB"/"Correct", and tooltip text
# verbatim from ucInfoBar.cs:1081-1096 [v2.10.3.13] including the
# "Showing level, " prefix gating.
nereus_add_test(tst_psa_indicator)

# ── Phase 3M-4 Task 11: Setup deltas for PureSignal ──────────────────────────
# (1) AntennaAlexAlex1Tab "HPF Bypass on PureSignal feedback" toggle now
#     shows the IMD warning dialog when un-checked (multi-paragraph text
#     verbatim from Thetis setup.cs:29274-29292 [v2.10.3.13]).  Cancel
#     reverts via QSignalBlocker.  Emits new hpfBypassOnPsChanged signal.
# (2) GeneralOptionsPage gains 2 PureSignal Info Bar checkboxes that
#     mirror Thetis groupBoxTS23 controls (setup.designer.cs:10567-10632
#     [v2.10.3.13]): chkHideFeebackLevel + chkSwapREDBluePSAColours.
#     User-visible text uses corrected spelling; Thetis typo "Feeback"
#     preserved only in source-cite for traceability.
# Test seam (setImdWarningResultForTest) lets tests drive both the
# Cancel-revert path and the OK-persist path without invoking a real
# QMessageBox::exec() modal.
nereus_add_test(tst_setup_deltas)

# ── Phase 3M-4 Task 13: applet wiring (PureSignalApplet + TxApplet [PS-A]) ───
# Replaces NyiOverlay::markNyi scaffolding in two applets with live wiring to
# the PureSignal coordinator (Task 7).
#
# PureSignalApplet (NereusSDR-native quick-access surface):
#   7 buttons (Calibrate / Auto / Save / Restore / 2-Tone) + 2 gauges (FB /
#   Correction) + 3 LEDs (Cal / Run / Fbk) + 3 info labels (Iterations /
#   Feedback dB / Correction dB).  Right-click on every control emits
#   openPureSignalDialogRequested → MainWindow::openPureSignalDialog.  Save
#   button gated on PureSignal::correctionsBeingAppliedChanged post-Codex Fix
#   D (mirrors PSForm.cs:574-590 [v2.10.3.13] CorrectionsBeingApplied
#   getter; pre-fix the wire was correctingChanged which conflated two
#   distinct Thetis predicates).
#
# TxApplet [PS-A] toggle (source-first port of Thetis chkFWCATUBypass):
#   Visibility gated on caps.hasPureSignal (TxApplet::setBoardCapabilities).
#   Left-click toggle drives PureSignal::setAutoCalEnabled (mirrors
#   chkFWCATUBypass_Click, console.cs:36762 [v2.10.3.13]).  Right-click
#   opens PsForm via openPureSignalDialogRequested signal (mirrors
#   chkFWCATUBypass_MouseDown, console.cs:46149-46152 [v2.10.3.13]).
#
# PureSignal coordinator extended with 3 new signals: calStateChanged
# (engine state for Cal/Run LEDs), correctionPeakChanged (HW peak for
# Correction gauge), and feedbackActiveChanged (correcting && MOX for
# Fbk LED).  All three fired from existing pollTimerTick.
nereus_add_test(tst_applet_ps_wiring)

# ── Phase 3M-1b Task B.1: BoardCapabilities::hasMicJack flag ─────────────────
# Parametrized over all HPSDRHW enum values.
# HL2 and HermesLiteRxOnly → false; all other boards → true.
# Source: pre-code review §11.3 per-board table.
nereus_add_test(tst_board_capabilities_mic_jack)

nereus_add_test(tst_radio_discovery_parse)
# TEST_DATA_DIR lets the fixture loader resolve paths relative to the tests/
# source directory regardless of where the build tree lives.
target_compile_definitions(tst_radio_discovery_parse PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

nereus_add_test(tst_p1_wire_format)
target_compile_definitions(tst_p1_wire_format PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-A AlexFilterMap shared freq→bits helpers (Task 3+4) ──
nereus_add_test(tst_alex_filter_map)

# ── Phase 3P-A P1CodecStandard ramdor WriteMainLoop port (Task 5+6) ──
nereus_add_test(tst_p1_codec_standard)

# ── P1 full-parity Task 1.1: P1CodecStandard mic_ptt direct polarity ──
# Closes the gap PR #161 left for non-HL2 boards (relay flutter root cause
# on ANAN-10E/HermesII; same fix as HL2 commit ca8cd73).
nereus_add_test(tst_p1_codec_standard_bank11_polarity)

# ── P1 full-parity Task 1.4: P1CodecStandard XmitBit on frequency banks ──
# Closes the gap PR #161 left for non-HL2 boards: Thetis networkproto1.c:447
# sets C0 = XmitBit before the case switch, so every bank's C0 carries XmitBit
# on the wire — including frequency banks (1, 2, 3, 5-9).
nereus_add_test(tst_p1_codec_standard_freq_bank_xmitbit)

# ── P1 full-parity Task 2.1: P1RadioConnection::setLineInGain virtual ────────
# 12 tests pin the line_in_gain field (bank 11 C2 low 5 bits, mask 0x1F) in
# P1RadioConnection::setLineInGain():
#   1.  Default state: C2 low 5 bits = 0 (m_lineInGain default 0).
#   2.  setLineInGain(15) → C2 low 5 bits = 0x0F.
#   3.  setLineInGain(31) → C2 low 5 bits = 0x1F (max).
#   4.  setLineInGain(-5) → clamps to 0 (after first storing non-zero).
#   5.  setLineInGain(50) → clamps to 31.
#   6.  setLineInGain does NOT touch C2 bit 6 (puresignal_run cross-bit guard).
#   7.  Bank 11 C0 address remains 0x14.
#   8.  Round-trip: 0 → 23 → 0 produces correct bit values.
#   9.  setLineInGain sets m_forceBank11Next (Codex P2 flush pattern).
#  10.  Repeated same-value call still sets the flush flag.
#  11.  Idempotency: setLineInGain(15) twice doesn't crash.
#  12.  Codec path (HL2): C2 emits 0 unconditionally — documents intentional
#       codec divergence (HL2 has no mic jack); flush flag still fires.
# Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13].
nereus_add_test(tst_p1_line_in_gain_setter)
target_compile_definitions(tst_p1_line_in_gain_setter PRIVATE NEREUS_BUILD_TESTS)

# ── P1 full-parity Task 2.2: P1RadioConnection::setUserDigOut virtual ────────
# 13 tests pin the user_dig_out field (bank 11 C3 low 4 bits, mask 0x0F) in
# P1RadioConnection::setUserDigOut():
#   1.  Default state: C3 low 4 bits = 0 (m_userDigOut default 0).
#   2.  setUserDigOut(0) → C3 low 4 bits = 0 (explicit zero).
#   3.  setUserDigOut(0x0A) → C3 low 4 bits = 0x0A (alternating bits).
#   4.  setUserDigOut(0x0F) → C3 low 4 bits = 0x0F (max).
#   5.  setUserDigOut(0xFF) → C3 = 0x0F (high-nibble masked off).
#   6.  Cross-bit guard: setUserDigOut leaves C1 (mic bits) unchanged.
#   7.  Cross-bit guard: setUserDigOut leaves C2 (line_in/puresignal) unchanged.
#   8.  Bank 11 C0 address remains 0x14.
#   9.  Round-trip: 0 → 0x07 → 0 produces correct bit values.
#  10.  setUserDigOut sets m_forceBank11Next (Codex P2 flush pattern).
#  11.  Repeated same-value call still sets the flush flag.
#  12.  Idempotency: setUserDigOut(0x0A) twice doesn't crash.
#  13.  Codec path (HL2): C3 emits 0 unconditionally — documents intentional
#       codec divergence (HL2 has no Penny header); flush flag still fires.
# Source: Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13].
nereus_add_test(tst_p1_user_dig_out_setter)
target_compile_definitions(tst_p1_user_dig_out_setter PRIVATE NEREUS_BUILD_TESTS)

# ── P1 full-parity Task 2.3: P1RadioConnection::setPuresignalRun virtual ─────
# 12 tests pin the puresignal_run flag (bank 11 C2 bit 6, mask 0x40) in
# P1RadioConnection::setPuresignalRun():
#   1.  Default state: C2 bit 6 = 0 (m_puresignalRun default false).
#   2.  setPuresignalRun(false) → C2 bit 6 = 0 (explicit baseline).
#   3.  setPuresignalRun(true)  → C2 bit 6 = 0x40 (set, no other bits).
#   4.  Cross-bit guard: leaves C2 low 5 bits (line_in_gain) unchanged.
#   5.  Cross-bit guard: leaves C1 (mic bits) unchanged.
#   6.  Cross-bit guard: leaves C3 (user_dig_out) unchanged.
#   7.  Bank 11 C0 address remains 0x14.
#   8.  Round-trip: false → true → false produces correct bit values.
#   9.  setPuresignalRun sets m_forceBank11Next (Codex P2 flush pattern).
#  10.  Repeated same-value call still sets the flush flag.
#  11.  Idempotency: setPuresignalRun(true) twice doesn't crash.
#  12.  Codec path (HL2): C2 emits 0 unconditionally — documents intentional
#       codec divergence (HL2 has no on-radio PS path); flush flag still fires.
# Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13].
nereus_add_test(tst_p1_puresignal_run_setter)
target_compile_definitions(tst_p1_puresignal_run_setter PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-4 Task 17 P1 follow-up: bank 2/3 PS DDC freq override ───────────
# 10 tests cover the verbatim port of the puresignal_run freq override:
#
#   1.  No MOX: bank 2 emits RX freq even with PS armed.
#   2.  HermesII PS-MOX: bank 2 → TX freq override.
#   3.  HermesII PS-MOX: bank 3 → TX freq override.
#   4.  HermesII no-PS MOX: bank 3 emits RX2 freq (Hermes default path).
#   5.  HL2 PS-MOX: bank 2 does NOT override (nDdc=4 disables gate).
#   6.  applyPsDdcConfig packs cntrl1+cntrl2 into m_adcCtrl + bank 4 emit.
#   7.  applyPsDdcConfig arms bank 0 + bank 4 flush flags.
#   8.  setTxStepAttenuation arms m_forceBank4Next.
#   9.  setTxStepAttenuation idempotent: flush flag set even on no-op.
#   10. setPuresignalRun arms bank 0 + bank 11 flush flags.
#
# Source map (per assertion):
#   Bank 2/3 override:           mi0bot networkproto1.c:982-1009 [v2.10.3.13-beta2]
#   Bank 4 ADC routing:          mi0bot console.cs:8531-8532 [v2.10.3.13-beta2]
#   setTxStepAttenuation flush:  Thetis netInterface.c:1006-1016 [v2.10.3.13]
#   setPuresignalRun flush:      Thetis PSForm.cs:246-247 [v2.10.3.13]
nereus_add_test(tst_p1_puresignal_ddc_freq)
target_compile_definitions(tst_p1_puresignal_ddc_freq PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-4 mi0bot audit: per-board PS DDC pair index verification ────────
# 8 tests cover psFbDdc/txMonDdc population by each per-board codec's
# applyPureSignalDdcConfig(), against the Thetis MetisReadThreadMainLoop
# twist() dispatch + console.cs GetDDC() table.
#
# Source map (per assertion):
#   nddc=2 PS pair:        mi0bot networkproto1.c:380-381 [v2.10.3.13-beta2]
#                          + console.cs:8798-8800 GetDDC HermesII case 5
#   nddc=4 PS pair:        mi0bot networkproto1.c:383-387 [v2.10.3.13-beta2]
#                          + console.cs:8757-8762 GetDDC HL2/Hermes case 5
#   nddc=5 P1 PS pair:     mi0bot networkproto1.c:388-392 [v2.10.3.13-beta2]
#                          + console.cs:8710-8714 GetDDC Orion-class case 5
#   P2 PS pair:            network.c:936-945 freq override [v2.10.3.13]
#                          + console.cs:8623 GetDDC P2 case 5
nereus_add_test(tst_ps_ddc_indices_per_board)
target_compile_definitions(tst_ps_ddc_indices_per_board PRIVATE NEREUS_BUILD_TESTS)

# ── P1 full-parity Task 2.5: TransmitModel::pureSig → setPuresignalRun wiring ─
# 8 tests pin the model-to-connection wiring installed in
# RadioModel::wireConnectionSignals + the initial-push pattern in
# RadioModel::connectToRadio.  Wire-byte side (bank 11 C2 bit 6) is
# already covered by tst_p1_puresignal_run_setter; these tests cover the
# higher-level model-layer wiring contract:
#   1.  Initial push: default model state → bank 11 C2 bit 6 clear.
#   2.  Initial push: persisted true → bank 11 C2 bit 6 = 0x40.
#   3.  Toggle on: model setter → wiring → connection setter → bit set.
#   4.  Toggle off: model false → wiring clears bit 6.
#   5.  Round-trip: false → true → false bit-tracks across the wiring.
#   6.  Idempotent setter: same value twice → wiring fires only on change.
#   7.  Cross-bit guard: pureSig wiring does not perturb C2 line_in_gain.
#   8.  loadFromSettings reads the pureSignal/enabled key.
#   8b. loadFromSettings defaults to false when key absent.
# Source: Thetis ChannelMaster/networkproto1.c:599-600 + PSForm.cs:240
#         [v2.10.3.13].
nereus_add_test(tst_radio_model_puresignal_run_wiring)
target_compile_definitions(tst_radio_model_puresignal_run_wiring PRIVATE NEREUS_BUILD_TESTS)

# ── P1 full-parity Task 3.1: PaCalProfile model + interpolation ──────────────
# Tests the per-board PA calibration model: paCalBoardClassFor() mapping for
# every HPSDRModel value, defaults() factory tables for each board class, and
# interpolate() matching Thetis console.cs:6691-6768 CalibratedPAPower +
# PowerKernel [v2.10.3.13]:
#   - Board-class mapping: 17 cases covering every HPSDRModel + sentinels.
#   - defaults(): Anan10/Anan100/Anan8000/HermesLite cal tables match the
#     ud{10,100,200}PA{N}W spinbox label values + None returns all zeros.
#   - interpolate(0): zero-passthrough across all classes.
#   - interpolate identity: default tables round-trip raw → raw at cal points
#     and between cal points.
#   - interpolate at calibrated points: user cal tables recover the spinbox
#     LABEL value (Thetis PowerKernel returns idx*interval, not table[idx]).
#   - interpolate between calibrated points: piecewise-linear.
#   - interpolate above max: linear extrapolation of the last segment.
#   - interpolate None class: identity passthrough.
#   - interpolate below zero: identity on default tables.
#   - interpolate degenerate zero-span: defensive guard, no crash.
nereus_add_test(tst_pa_cal_profile)

# ── Issue #167 Phase 1 Agent 1A: PaGainProfile factory tables ─────────────
# Byte-for-byte parity vs Thetis clsHardwareSpecific.cs:459-751 [v2.10.3.13]
# DefaultPAGainsForBands(HPSDRModel) plus mi0bot HL2 row from
# clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2 @c26a8a4]. K2GX
# regression spotlight: ANAN-8000DLE 80m == 50.5f exactly.
nereus_add_test(tst_pa_gain_profile)

# ── Issue #167 Phase 2 Agent 2A: PaProfile editable per-instance gain table ──
# Faithful port of Thetis PAProfile (setup.cs:23763-24046 [v2.10.3.13]).
# Covers:
#   - Construction + ResetGainDefaultsForModel pulls factory rows from
#     PaGainProfile (ANAN-8000DLE 80m = 50.5f).
#   - GetGainForBand(driveValue == -1) returns base; with driveValue applies
#     calcDriveAdjust subtraction.
#   - calcDriveAdjust 9-step lerp boundaries: drive=0/100 short-circuit to 0,
#     exact step idx 1/5/9, mid-lerp idx 0/5/9.
#   - Setters silently no-op on out-of-range Band / stepIndex (matches
#     Thetis Band.FIRST/LAST guard).
#   - copySettings deep-copies gainValues / gainAdjust / maxPower /
#     useMaxPower; leaves name / model / isFactoryDefault unchanged.
#   - dataToString emits exactly 171 pipe-delimited fields (3 + 14 + 126 + 28).
#   - dataToString → dataFromString round-trip on randomly-filled profile.
#   - Default constructor produces valid empty state (all gains 100.0f
#     sentinel matching Thetis init loop).
#   - dataFromString rejects malformed inputs (wrong field count) → false.
nereus_add_test(tst_pa_profile)

# ── Phase 2 Agent 2B of issue #167: PaProfileManager ─────────────────────────
# Per-MAC bank for PaProfile instances. Mirrors MicProfileManager API verbatim.
# Factory regen on first connect (16 Default-<model> + Bypass) per Thetis
# initPAProfiles (setup.cs:23295-23316 [v2.10.3.13]). Stored profiles win
# over factory defaults per Thetis RecoverPAProfiles (setup.cs:1920-1959
# [v2.10.3.13]) — explicit Reset Defaults button is the only path to refresh
# from canonical tables.
# Covers: first-launch seeding (16 + Bypass + active=Default-<model>),
# reconnect preserves stored profiles, per-MAC isolation, save overwrites
# without re-emitting profileListChanged, delete + last-profile guard,
# setActiveProfile non-existent returns false, active-profile-on-connect
# 3-case logic (NereusSDR-spin), regenerateFactoryDefaults restores factory
# but leaves customs alone, activeProfile() accessor.
nereus_add_test(tst_pa_profile_manager)

# ── Phase 3P-A P1CodecAnvelinaPro3 bank 17 extra OC (Task 7) ──
nereus_add_test(tst_p1_codec_anvelina_pro3)

# ── Phase 3P-A P1CodecRedPitaya bank 12 ADC1 MOX carve-out (Task 8) ──
nereus_add_test(tst_p1_codec_redpitaya)

# ── Phase 3P-A P1CodecHl2 mi0bot WriteMainLoop_HL2 port (Tasks 9+10) ──
nereus_add_test(tst_p1_codec_hl2)

# ── Phase 3P-A regression-freeze gate (Task 16) ──
nereus_add_test(tst_p1_regression_freeze)
target_compile_definitions(tst_p1_regression_freeze PRIVATE
    NEREUS_BUILD_TESTS
    NEREUS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")

# ── Phase 3P-B P2 codec tests ──
nereus_add_test(tst_p2_codec_orionmkii)

# ── Phase 3P-B Task 5: P2CodecSaturn — Saturn BPF1 band-edge override ──
nereus_add_test(tst_p2_codec_saturn)

# ── Phase 3M-4 Task 5: per-board PsDdcConfig codec dispatch ──
# Verifies every codec subclass (P1CodecStandard / P1CodecHl2 /
# P1CodecAnvelinaPro3 / P1CodecRedPitaya / P2CodecOrionMkII /
# P2CodecSaturn) emits the wire-byte map matching its branch in Thetis
# console.cs:8186-8538 UpdateDDCs() [v2.10.3.13] (and mi0bot
# console.cs:8409-8488 [v2.10.3.13-beta2] for HL2).  ~25 cases covering:
#   - G2-class (PS-on/off, MOX/no-MOX, diversity, RX2)
#   - HL2 (rx1_rate vs ps_rate distinction at 96k/192k)
#   - HermesII-class (p1DdcConfig=5 vs Hermes p1DdcConfig=6)
#   - RedPitaya (Rate[1] = rx1_rate in PS-off cases per REDPITAYA PAVEL)
#   - HPSDR (Atlas) returns all zeros sentinel
nereus_add_test(tst_codec_ps_ddc_config)

# ── Phase 3M-4 Task 6: ReceiverManager UpdateDDCs PS branch ──────────────
# Verifies the ReceiverManager state setters (PS / MOX / diversity / RX
# rates / RX2 enable / ADC ctrl shadow) dispatch through the injected
# per-board codec's applyPureSignalDdcConfig() and re-emit the resulting
# PsDdcConfig via ddcConfigChanged for RadioConnection consumption (a
# follow-up task wires that up).  Ports the orchestration of Thetis
# console.cs:8186-8538 UpdateDDCs() [v2.10.3.13].
nereus_add_test(tst_receiver_manager_ps_ddc)

# ── Phase 3P-B Task 7: regression-freeze gate ────────────────────────────
nereus_add_test(tst_p2_regression_freeze)

# ── Phase 3P-B Task 8: Alex-1 Filters sub-sub-tab smoke test ─────────────
nereus_add_test(tst_alex1_filters_tab)

# ── Phase 3P-B Task 9: Alex-2 Filters sub-sub-tab smoke test ─────────────
nereus_add_test(tst_alex2_filters_tab)
target_compile_definitions(tst_p2_regression_freeze PRIVATE
    NEREUS_BUILD_TESTS
    NEREUS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")

# P1 loopback integration test — uses P1FakeRadio from tests/fakes/
nereus_add_test(tst_p1_loopback_connection fakes/P1FakeRadio.cpp)
target_compile_definitions(tst_p1_loopback_connection PRIVATE NEREUS_BUILD_TESTS)

# P1 reconnection state machine test — uses P1FakeRadio auto-streaming (Task 10)
nereus_add_test(tst_reconnect_on_silence fakes/P1FakeRadio.cpp)
target_compile_definitions(tst_reconnect_on_silence PRIVATE NEREUS_BUILD_TESTS)

# Saved-radio persistence round-trip (Phase 3I Task 15)
nereus_add_test(tst_connection_panel_saved_radios)

# HardwarePage capability gating (Phase 3I Task 21)
nereus_add_test(tst_hardware_page_capability_gating)
target_compile_definitions(tst_hardware_page_capability_gating PRIVATE NEREUS_BUILD_TESTS)

# HardwarePage per-MAC persistence round-trip (Phase 3I Task 21)
nereus_add_test(tst_hardware_page_persistence)

# SampleRateCatalog — PR #35
nereus_add_test(tst_sample_rate_catalog)

# Regression test for Windows shutdown/disconnect crash — cross-thread
# destruction of RadioConnection (and its worker-affined QTimers).
nereus_add_test(tst_cross_thread_teardown fakes/P1FakeRadio.cpp)
target_compile_definitions(tst_cross_thread_teardown PRIVATE NEREUS_BUILD_TESTS)

# Regression test for the GUI-thread fexchange2 deadlock — verifies
# that RxDspWorker::processIqBatch runs on its own QThread when driven
# via Qt::QueuedConnection, and that posting a burst of batches does
# not block the sender. See src/models/RxDspWorker.h for the rationale.
nereus_add_test(tst_rx_dsp_worker_thread)

# Regression test for rate-dependent audio jitter: RxDspWorker's drain
# chunk size must follow setBufferSizes() (per-rate Thetis formula
# in_size = 64 * rate / 48000), not a hardcoded 1024 that was correct
# only for 768 kHz and produced fexchange2 size mismatches at every
# other rate after PR #44 changed the default to 192 kHz.
nereus_add_test(tst_rx_dsp_worker_buffer_sizing)

# ── Phase 3G-10 VFO DSP parity + AetherSDR flag port ─────────────────────
nereus_add_test(tst_vfo_level_bar)
nereus_add_test(tst_scrollable_label)
nereus_add_test(tst_vfo_mode_containers)
nereus_add_test(tst_vfo_tooltip_coverage)

# Regression test for issue #73 — VfoWidget frequency-entry parser
nereus_add_test(tst_vfo_frequency_parser)

# ── Phase 3G-10 Stage 2: WDSP wiring — AGC advanced ──────────────────────
nereus_add_test(tst_rxchannel_agc_advanced)
nereus_add_test(tst_slice_agc_advanced)

# ── Phase 3G-10 Stage 2: WDSP wiring — EMNR (NR2) ────────────────────────
nereus_add_test(tst_rxchannel_emnr)
nereus_add_test(tst_slice_emnr)

# ── Phase 3G-10 Stage 2: WDSP wiring — SNB ───────────────────────────────
nereus_add_test(tst_rxchannel_snb)
nereus_add_test(tst_slice_snb)

# ── Phase 3G-10 Stage 2: WDSP wiring — APF (Audio Peak Filter) ───────────
nereus_add_test(tst_rxchannel_apf)
nereus_add_test(tst_slice_apf)

# ── Phase 3G-10 Stage 2: WDSP wiring — Squelch (SSB/AM/FM) ─────────────
nereus_add_test(tst_rxchannel_squelch)
nereus_add_test(tst_slice_squelch)

# ── Phase 3G-10 Stage 2: WDSP wiring — Mute / Audio Pan / Binaural (S2.6) ──
nereus_add_test(tst_rxchannel_audio_panel)
nereus_add_test(tst_slice_audio_panel)

# ── Phase 3G-10 Stage 2: WDSP wiring — NB2 polish (S2.7) ───────────────────
nereus_add_test(tst_rxchannel_nb2_polish)

# ── Phase 3G RX Epic Sub-epic B Task 11: NbFamily cycling + per-band NB persistence ──
# tst_nb_family: pure C++ — cycleNbMode rotation (Off→NB→NB2→Off) and
#   NbTuning defaults pinned to Thetis cmaster.c:43-68 [v2.10.3.13].
#   WDSP-free: NbFamily is never instantiated (no WDSP channel required).
# tst_slice_nb_persistence: SliceModel NbMode + NbTuning round-trip through
#   per-band AppSettings keys (Band40m/Band20m isolation, cross-band case).
nereus_add_test(tst_nb_family)
nereus_add_test(tst_slice_nb_persistence)

# ── Phase 3G-10 Stage 2: RIT/XIT client offset (S2.8) ───────────────────────
nereus_add_test(tst_slice_rit_xit)

# ── Phase 3G-10 Stage 2: Frequency lock (S2.9) ──────────────────────────────
nereus_add_test(tst_slice_frequency_lock)

# ── Phase 3G-10 Stage 2: FM OPT container wire (S2.10a) ─────────────────────
nereus_add_test(tst_fm_opt_container_wire)

# ── Phase 3G-10 Stage 2: DIG offset + RTTY mark/shift (S2.10b) ──────────────
nereus_add_test(tst_dig_rtty_wire)

# ── Phase 3G-10 Stage 2: per-slice-per-band DSP persistence (S2.P) ───────────
nereus_add_test(tst_slice_persistence_per_band)

# ── v0.2.0 regression: band-change lambda reentrancy guard ───────────────────
nereus_add_test(tst_band_change_recursion_guard)

# ── Step attenuator / ADC overload detection ─────────────────────────────
nereus_add_test(tst_step_attenuator_controller)

# ── BoardCapabilities flag-wiring (P1 full-parity §4) ────────────────────
# Verifies populated-but-previously-unconsumed BoardCapabilities flags now
# affect runtime behaviour:
#   §4.1 hasStepAttenuatorCal — gates AutoAttMode::Adaptive selection
#                                (silent-coerce to Classic when false)
#   §4.2 hasSidetoneGenerator  — gates CwSetupPage Sidetone Volume row
#   §4.3 hasPennyLane          — gates OcOutputsTab "User Dig Out" group
#                                (4 bit-toggle checkboxes → TransmitModel)
nereus_add_test(tst_board_capability_flag_wiring)

# ── Issue #118: band-button auto-mode ────────────────────────────────────
nereus_add_test(tst_band_from_name)
nereus_add_test(tst_slice_has_settings_for)
nereus_add_test(tst_band_defaults)
nereus_add_test(tst_radio_model_band_click)

# ── About dialog ─────────────────────────────────────────────────────────
nereus_add_test(tst_about_dialog)

# ── Phase 3O Sub-Phase 11 Task 11a: VaxFirstRunDialog ────────────────────
nereus_add_test(tst_vax_first_run_dialog)

# ── Phase 3O VAX routing ──────────────────────────────────────────────────
nereus_add_test(tst_slice_model_vax)

nereus_add_test(tst_transmit_model_tx_owner)

# ── Phase 3M-1a G.3: TransmitModel tunePowerByBand[14] per-MAC persistence ──
nereus_add_test(tst_transmit_model_tune_power)

# ── Phase 3M-1b C.1: TransmitModel::micGainDb + derived micPreampLinear ─────
# Verifies default (-6 dB), dB→linear round-trip via pow(10, dB/20),
# both micGainDbChanged + micPreampChanged signals emitted together,
# idempotent guard on same-value set, range clamping to kMicGainDbMin/Max.
# Source: console.cs:28805-28817 [v2.10.3.13] (setAudioMicGain dB→scalar).
nereus_add_test(tst_transmit_model_mic_gain)

# ── Phase 3M-1b C.2: TransmitModel mic-jack flag properties (8x) ─────────────
# Verifies defaults, round-trip setters, signal emission, idempotent guard,
# and lineInBoost range clamping for: micMute / micBoost / micXlr / lineIn /
# lineInBoost / micTipRing / micBias / micPttDisabled.
# Source: console.cs:13213-13260 [v2.10.3.13]; console.cs:28752 [v2.10.3.13]
# (MicMute counter-intuitive naming); console.cs:19757-19766 [v2.10.3.13]
# (MicPTTDisabled); setup.designer.cs:46898-46907 [v2.10.3.13]
# (udLineInBoost range -34.5..12.0).
nereus_add_test(tst_transmit_model_mic_jack_flags)

# ── Phase 3M-1b C.3: TransmitModel VOX properties (4x) ───────────────────────
# Verifies defaults (voxEnabled=false / voxThresholdDb=-40 / voxGainScalar=1.0f /
# voxHangTimeMs=500), round-trip setters, signal emission, idempotent guard,
# and range clamping for voxThresholdDb [−80,0], voxGainScalar [0.0f,100.0f],
# and voxHangTimeMs [1,2000].
# Source: audio.cs:167-202 [v2.10.3.13] (VOXEnabled/VOXGain defaults);
#         console.Designer.cs:6018-6019 [v2.10.3.13] (ptbVOX range −80..0);
#         setup.designer.cs:45005-45024 [v2.10.3.13] (udDEXPHold range 1..2000).
nereus_add_test(tst_transmit_model_vox_properties)

# ── Phase 3M-3a-iii Tasks 7-10: TransmitModel DEXP properties (11x) ──────────
# Verifies defaults, round-trip, signals, idempotency, clamps, and persistence
# for the 11 DEXP properties (envelope x4, gate-ratio x2, look-ahead x2,
# side-channel filter x3).  All 11 persist; dexpEnabled has no PTT-safety
# carve-out (only voxEnabled does).
# Source: setup.Designer.cs:44788-45260 [v2.10.3.13] (grpDEXPVOX + grpSCF).
nereus_add_test(tst_transmit_model_dexp)

# ── Phase 3M-1b C.4: TransmitModel anti-VOX properties ─────────────────────
# Verifies defaults (antiVoxGainDb=0 NereusSDR-original), round-trip setters,
# signal emission, idempotent guard, and range clamping for antiVoxGainDb
# [−60,60].  3M-3a-iv post-bench refactor (Option A) dropped the antiVoxSourceVax
# coverage alongside the property itself.
# Source: setup.designer.cs:44699-44728 [v2.10.3.13] (udAntiVoxGain -60..60);
#         setup.cs:18986-18989 [v2.10.3.13] (SetAntiVOXGain dB→linear formula).
nereus_add_test(tst_transmit_model_anti_vox)

# ── Phase 3M-1b C.5: TransmitModel MON properties (2x) ──────────────────────
# Verifies defaults (monEnabled=false matches Thetis audio.cs:406 [v2.10.3.13] /
# monitorVolume=0.5f matches Thetis audio.cs:417 [v2.10.3.13] literal mix
# coefficient), round-trip setters, signal emission, idempotent guard (incl.
# min-boundary zero-case per C.3 fix-up), and range clamping [0.0f, 1.0f].
# Source: audio.cs:406 [v2.10.3.13] (mon default off);
#         audio.cs:417 [v2.10.3.13] (SetAAudioMixVol 0.5 literal);
#         phase3m-1b-thetis-pre-code-review.md §4.2 + §12.5.
nereus_add_test(tst_transmit_model_mon)

# ── Phase 3M-1b L.2: TransmitModel per-MAC mic/VOX/MON persistence ───────────
# Verifies loadFromSettings(mac) / persistToSettings(mac) + auto-persist on
# each setter.  Covers: round-trip per persisted key (15 cases), exclusion of
# non-persisted keys (voxEnabled/monEnabled/micMute — 3 cases), first-run
# defaults (all 15 at C.1-C.5 defaults; micGainDb -6 per §0 row 11),
# multi-MAC isolation, bulk persistToSettings round-trip, and auto-persist
# no-op before loadFromSettings.
nereus_add_test(tst_transmit_model_persistence)

# ── P1 full-parity Task 2.4: TransmitModel lineInGain + userDigOut ───────────
# Verifies the model→connection plumbing for line_in_gain (bank 11 C2 low 5
# bits) and user_dig_out (bank 11 C3 low 4 bits): defaults, clamp/mask in
# setters, signal emission, idempotent guard, AppSettings round-trip via
# loadFromSettings + persistToSettings, and MicProfileManager bundle round-trip
# via the Mic_LineInGain / Mic_UserDigOut bundle keys.
# Source: Thetis ChannelMaster/networkproto1.c:600-601 [v2.10.3.13].
nereus_add_test(tst_transmit_model_line_in_gain_user_dig_out)

# ── Phase 3M-1c B.2: TransmitModel two-tone test properties (7x) ────────────
# Verifies defaults, round-trip setters + signals, idempotent guards, and
# range clamping for: TwoToneFreq1 / TwoToneFreq2 / TwoToneLevel /
# TwoTonePower / TwoToneFreq2Delay / TwoToneInvert / TwoTonePulsed.
# Defaults follow option C: NereusSDR-original safer numerics for Level/Power
# (-6 dB / 50%); Thetis-Designer-aligned for Invert (true) and ranges.
# Source: setup.Designer.cs:61920-62136 [v2.10.3.13] (Designer init values
# for the 7 controls); setup.cs:11019-11200 (handler context).
nereus_add_test(tst_transmit_model_two_tone_properties)

# ── Phase 3M-1c B.3: TransmitModel TwoToneDrivePowerSource enum property ─────
# Verifies default (DriveSlider) + 3-value enum coverage (DriveSlider /
# TuneSlider / Fixed) + signal emission + idempotent guard +
# string-conversion helpers (drivePowerSourceToString / fromString).
# Source: enums.cs:456-461 [v2.10.3.13] (enum definition; 3 values, not 2 —
# pre-code review §2.3 abbreviated as "Fixed / Slider" but Thetis has 3);
# console.cs:46553 [v2.10.3.13] (default DRIVE_SLIDER);
# setup.cs:11111-11119 [v2.10.3.13] (Fixed-mode behaviour reference).
nereus_add_test(tst_transmit_model_two_tone_drive_origin)

# ── Phase 3 Agent 3A of #167: TransmitModel PA-cal hotfix scaffolding ────────
# Verifies the pre-math-kernel state plumbing for the PA calibration safety
# hotfix (issue #167):
#   - per-band normal-mode power array (m_powerByBand[14], default 100 W)
#   - 3 ATT-on-TX-on-power-change safety properties (defaults match Thetis)
#   - m_lastPower sentinel (-1, runtime-only) with reset-on-toggle semantic
#     (CRITICAL: Thetis console.cs:29298 [v2.10.3.13])
#   - pureSignalActive() predicate (returns false unconditionally; 3M-4
#     PureSignal phase wires the live PS-A check)
# Source: console.cs:1817 [v2.10.3.13] (limitPower_by_band default 100 W);
#         console.cs:29285-29310 [v2.10.3.13] (ATT-on-TX safety props);
#         console.cs:29298 [v2.10.3.13] (_lastPower reset on toggle).
nereus_add_test(tst_transmit_model_pa_scaffold)

# ── Phase 3 Agent 3B of #167: TransmitModel::computeAudioVolume math kernel ──
# Verifies the dBm-target math kernel that translates (sliderWatts, band,
# active PA profile) into a normalized audio output level in [0, 1.0]:
#   - K2GX regression (CRITICAL, ship-blocking) — ANAN-8000DLE 80m at 100/200W
#   - sliderWatts <= 0 short-circuit returns 0.0
#   - HL2 sentinel (gbb >= 99.5) — linear fallback at 80m, dBm math at 6m
#   - NereusSDR Bypass profile (all-100.0f) hits the same linear fallback
#   - Safety ceiling matrix: every (HPSDRModel, Band) at radio's spec'd max
#     watts produces audio_volume <= 1.0; tightest cell ANAN-8000D B10M @ 200W
#   - Out-of-range Band hits sentinel via PaProfile::getGainForBand 1000-fallback
#   - No NaN / Inf for pathological inputs (INT_MAX, INT_MIN, negative)
#   - Drive-step lerp adjust applied at exact-step inputs
# Source: console.cs:46720-46751 [v2.10.3.13] (SetPowerUsingTargetDBM math);
#         mi0bot clsHardwareSpecific.cs:484 [v2.10.3.13-beta2] ("100 is no
#                                            output power" sentinel comment).
nereus_add_test(tst_transmit_model_compute_audio_volume)

# ── Phase 3 Agent 3C of #167: TransmitModel::setPowerUsingTargetDbm wrapper ──
# Verifies the deep-parity port of Thetis SetPowerUsingTargetDBM
# (console.cs:46645-46762 [v2.10.3.13]) integrating Phase 3A scaffolding +
# Phase 3B math kernel into a unified API:
#   - All three txMode branches (0=normal, 1=tune, 2=2tone)
#   - Drive-source enum routing for tune/2tone (DriveSlider/TuneSlider/Fixed)
#   - power_by_band write side-effect on txMode 0 (matches console.cs:46676)
#   - bConstrain out-param: false on Fixed drive source per console.cs:46688
#   - ATT-on-TX-on-power-change safety gate firing/dormant/decrease cases
#     (//[2.10.3.5]MW0LGE tag preserved verbatim) — pushes ATT_on_TX=31dB
#     via injected StepAttenuatorController when PS-active + power changes
#   - K2GX integration regression (ANAN-8000DLE 80m TUN slider 50)
#   - audioVolumeChanged signal payload parity with computeAudioVolume
#   - XVTR / power=0 / null-controller edge cases
# Source: console.cs:46645-46762 [v2.10.3.13] (SetPowerUsingTargetDBM full).
nereus_add_test(tst_transmit_model_set_power_using_dbm)

# ── Issue #175 Task 6: TransmitModel polymorphic per-band tune-power clamp ───
# Verifies setTunePowerForBand clamp ceiling polymorphs by SKU:
#   HERMESLITE: [0, 99]   (mi0bot Tune slider scale, 33 sub-steps)
#   others:     [0, 100]  (canonical Thetis 0-100 watts target)
# Also pins the dB<->int conversion formula round-trip used by Task 9.
# Source: mi0bot console.cs:47616-47666 [v2.10.3.13-beta2].
nereus_add_test(tst_transmit_model_hl2_persistence)

# ── Phase 4 Agent 4A of #167: RadioModel drive-path topology + K2GX repro ────
# Wire-byte / IQ-scalar topology (audio.cs:262-271 wire NO SWR / cmaster.cs:
# 1115-1119 IQ HAS SWR) verification for the drive-slider lambda + TUNE
# engagement path.  K2GX regression test pinned: ANAN-8000DLE 80m TUNE
# slider=50 -> wire byte 47..50 (vs. pre-hotfix 127).
# Covers:
#   §1 K2GX regression (ship-blocking)
#   §2 Drive-slider lambda at slider=100
#   §3 HL2 sentinel preserves linear behaviour
#   §4 HL2 6m uses dBm math
#   §5 SWR foldback applies to IQ ONLY, not wire byte
#   §6 TUNE engagement triggers full math chain
#   §7 No active profile -> graceful no-op
#   §8 PaTelemetryScaling::scaleFwdPowerWatts public lift parity
nereus_add_test(tst_radio_model_drive_path)
target_compile_definitions(tst_radio_model_drive_path PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c F: MicProfileManager (load/save/delete/setActive) ────────────
# Verifies the per-MAC mic-profile-bank API: F.1 class skeleton + load/save,
# F.2 save with overwrite + comma-strip, F.3 delete with last-profile guard
# (verbatim Thetis warning string preserved), F.4 setActiveProfile applies
# values back to TransmitModel + persists active key, F.5 first-launch
# "Default" profile seed (23 documented defaults), F.6 per-MAC isolation
# (two MACs maintain independent profile lists).
# Source: setup.cs:9505-9656 [v2.10.3.13] (Thetis comboTXProfileName /
# btnTXProfileSave_Click / btnTXProfileDelete_Click handlers).
nereus_add_test(tst_mic_profile_manager)

# ── Phase 3R Task K1: MicProfileManager "RADE" factory preset ──
# Verifies the NereusSDR-native "RADE" factory profile is seeded on
# first launch and that its EQ-off / Leveler-on / ALC-off /
# CFC-CESSB-PhRot-off bypass philosophy applies cleanly to the
# TransmitModel via setActiveProfile.
nereus_add_test(tst_mic_profile_rade)

# ── Phase 3M-3a-ii Batch 4: MicProfileManager CFC/CPDR/CESSB/PhRot round-trip ──
# Verifies the 41 new bundle keys (Phase Rotator + CFC + CPDR + CESSB)
# round-trip through AppSettings byte-for-byte, including each of the
# 21 Thetis factory profiles' per-row deviations sourced verbatim from
# database.cs:AddTXProfileTable [v2.10.3.13].  Covers: bundle-key presence
# check, defaultProfileValues() for the 41 new keys, per-profile CompanderLevel
# / CESSB_On / CFC scalars + arrays cells, full SSB *.* k CFC + AM 10k CFC
# array overrides, CFCParaEQData blob round-trip, and 5-bool serialization
# sanity.
nereus_add_test(tst_mic_profile_manager_cfc_round_trip)

# ── Phase 3M-3a-ii Batch 4.5: MicProfileManager CFC/CPDR/CESSB/PhRot live path ─
# Verifies the live capture/apply path between TransmitModel and the
# MicProfileManager bundle for the 41 new keys, closing the gap between
# Batch 4 (bundle keys + factory rows) and the TxCfcDialog [Save] /
# profile-combo activation flow.  Covers: capture round-trip (TM property
# → AppSettings bundle key), apply round-trip (bundle key → TM getter),
# end-to-end profile-switch lifecycle (saveProfile → setActiveProfile →
# saveProfile → setActiveProfile back), and bool serialization sanity
# for the 5 new bool keys.
nereus_add_test(tst_mic_profile_manager_cfc_live_path)

# ── Phase 3M-3a-iii Task 11: MicProfileManager DEXP round-trip ────────────
# Verifies the 11 new DEXP bundle keys (envelope, gate ratios, look-ahead,
# side-channel filter) round-trip through MicProfileManager byte-for-byte.
# Defaults sourced from Thetis setup.Designer.cs:44755-45250 [v2.10.3.13]
# (grpDEXPVOX + grpDEXPLookAhead + grpSCF on tpDSPVOXDE).  Covers: bundle-
# key presence (asserting total bundle count of 107), defaultProfileValues()
# for the 11 new keys with verbatim Thetis Designer source cites, capture
# round-trip (TM property -> AppSettings bundle key), apply round-trip
# (bundle key -> TM getter), end-to-end full-bundle 11-key round trip
# (catches accidental key/setter swaps within a section), and bool
# serialization sanity for the 3 new bool keys.
nereus_add_test(tst_mic_profile_manager_dexp_round_trip)

# ── Phase 3M-3a-ii follow-up Batch 6: ParaEqEnvelope helper tests ────
# Verifies the gzip+base64url envelope helper that wraps the
# ParametricEqWidget JSON for storage in TXProfile blobs (TXParaEQData /
# CFCParaEQData).  Mirrors Thetis Common.cs Compress_gzip /
# Decompress_gzip [v2.10.3.13] byte-for-byte (modulo gzip mtime) so
# Thetis-produced blobs decode cleanly into NereusSDR.  Covers ASCII +
# Unicode round-trip, multi-chunk inflate (10+KB JSON), Thetis fixture
# decode (Python-pinned, mtime=0), invalid base64 / non-gzip / truncated
# stream all returning std::nullopt.
nereus_add_test(tst_para_eq_envelope)

# ── Phase 3M-3a-ii follow-up Batch 6: TXParaEQData bundle round-trip ──
# Closes the gap between the new TransmitModel.txEqParaEqData property
# and the MicProfileManager bundle key TXParaEQData.  Covers: bundle-
# key presence + empty default, capture path (TM -> bundle), apply path
# (bundle -> TM), independence from CFCParaEQData (no key collision /
# swapped getters), and empty-blob preservation across save+reload.
nereus_add_test(tst_mic_profile_manager_para_eq_round_trip)

# ── Phase 3M-1c chunk I (I.1-I.5): TwoToneController activation handler ────
# Verifies the chkTestIMD_CheckedChanged port (setup.cs:11040-11191
# [v2.10.3.13]): power-on precondition, MOX cycle-off-then-on flow,
# TXPostGen setter ordering, continuous vs pulsed branch, 0.49999 magnitude
# scaling, mode-aware invert, Freq2Delay deferred Mag2, DrivePowerSource::Fixed
# snapshot/restore of PWR, and BandPlanGuard rejection cleanup (via
# the existing 3M-1b K.1 SSB-mode allow-list rule).
nereus_add_test(tst_two_tone_controller)

# ── Phase 4 Agent 4B of #167: TwoToneController PA-cal integration ────────
# Verifies start()/stop() route through Phase 3C
# TransmitModel::setPowerUsingTargetDbm with bTwoTone=true so the PA gain
# compensation applies during the IMD test (txMode=2 routing per
# console.cs:46693-46708 [v2.10.3.13]).  Covers all three drive-source
# enums (DriveSlider/TuneSlider/Fixed) plus the null-manager guards.
nereus_add_test(tst_two_tone_controller_pa_cal_integration)

# ── Phase 3O VAX schema migration ─────────────────────────────────────────
nereus_add_test(tst_app_settings_vax_migration)

# ── Issue #100: --profile CLI flag for concurrent-instance isolation ─────
nereus_add_test(tst_app_settings_profile)

# ── Phase 3O Sub-Phase 8: VaxChannelSelector widget ───────────────────────
nereus_add_test(tst_vax_channel_selector)

# ── Phase 3O Sub-Phase 2: MasterMixer ─────────────────────────────────────
nereus_add_test(tst_master_mixer)

# ── Phase 3O Sub-Phase 3: PortAudioBus ────────────────────────────────────
nereus_add_test(tst_port_audio_bus)

# ── Phase 3O Sub-Phase 5.3: CoreAudioHalBus (macOS-only) ──────────────────
# Exercises shm-segment lifecycle + push/pull round-trip against the shared
# memory ring the HAL plugin consumes. shm_open / mmap are macOS-specific
# in the bus impl (Q_OS_MAC guard) — the test executable is only built on
# Apple targets to match.
if(APPLE)
    nereus_add_test(tst_core_audio_hal_bus)
endif()

# ── Phase 3O Sub-Phase 6: LinuxPipeBus (Linux-only) ──────────────────────────
# Exercises FIFO lifecycle + pactl module load/unload + push/pull round-trip +
# role policing + format validation + stale-cleanup idempotency.
# The impl is guarded by Q_OS_LINUX; the test binary is only built on Linux
# to match. Tests that call pactl QSKIP when pactl is not in PATH (CI runners
# do not install pulseaudio-utils; run on a developer Linux box for lifecycle
# and metering coverage).
if(UNIX AND NOT APPLE)
    nereus_add_test(tst_linux_pipe_bus)
endif()

# ── Linux audio backend detection (PipeWire-native bridge, Task 3) ───────────
# Pure unit test: injects stub probe callbacks into detectLinuxBackend() to
# cover all 8 (pipewire × pactl × forced-override) combinations plus the
# forced-unknown fall-through and toString mapping. No real daemon required.
# The .cpp providing detectLinuxBackend/toString/defaultProbes is in the
# Linux-only CORE_SOURCES branch, so the test binary is Linux-only to match.
if(UNIX AND NOT APPLE)
    nereus_add_test(tst_linux_backend_detection)
endif()

# ── Linux audio infrastructure — AudioRingSpsc (Task 6) ──────────────────────
# Lock-free single-producer single-consumer byte ring used by the PipeWire
# bridge (Task 8+) to shuttle audio between the DSP thread and the
# pw_thread_loop. Pure header-only template — compiles cleanly on every
# platform, so no Linux-only gate. Covers: push/pop round-trip, empty-pop,
# partial-pop honouring dst capacity, overflow-drops-oldest on a 70 kB write
# into a 64 kiB ring, wrap-around across the buffer boundary, and a
# 1 M-byte concurrent producer/consumer race for memory ordering.
nereus_add_test(tst_audio_ring_spsc)

# ── Phase 3O Sub-Phase 8 Task 8: PipeWireStream config-translation ───────────
# Pure unit test: exercises configToProperties() for Output (virtual source),
# Input (consumer), and targetNodeName routing. No running daemon required.
# Gated by NEREUS_HAVE_PIPEWIRE — on hosts without libpipewire the test
# main() returns 0 and ctest reports PASS.
if(UNIX AND NOT APPLE)
    nereus_add_test(tst_pipewire_stream_config)
endif()

# ── Phase 3O Task 24: env-gated PipeWire stream integration test ─────────────
# Real round-trip: PipeWireThreadLoop.connect() → PipeWireStream(Audio/Source)
# → open() → wait for "streaming" signal → push 500 ms silence → close().
# QSKIPs when XDG_RUNTIME_DIR is unset (no PipeWire session on host).
# Compiles to int main(){return 0;} on platforms without libpipewire (#else).
if(UNIX AND NOT APPLE)
    nereus_add_test(tst_pipewire_stream_integration)
endif()

# ── Phase 3O Sub-Phase 7 Task 7.1: VirtualCableDetector ──────────────────────
nereus_add_test(tst_virtual_cable_detector)

# ── Phase 3O Sub-Phase 8.5: AudioEngine VAX tee logic ────────────────────────
# Exercises the rxBlockReady → m_vaxBus[] tee using FakeAudioBus injected
# via the NEREUS_BUILD_TESTS-only setVaxBusForTest / setSpeakersBusForTest
# seam on AudioEngine. No real CoreAudioHalBus / LinuxPipeBus / PortAudio
# backend required — runs on every platform.
nereus_add_test(tst_audio_engine_vax_tee)

# ── Phase 3O Sub-Phase 10 Task 10a: AudioEngine master-mute API ──────────────
# Covers setMasterMuted / masterMuted / masterMutedChanged. Verifies the
# rxBlockReady mute check gates ONLY the speakers push (VAX taps must keep
# running so 3rd-party consumer apps don't hear the local monitor mute).
nereus_add_test(tst_audio_engine_master_mute)

# ── Phase 3O Sub-Phase 10 Task 10b: MasterOutputWidget ──────────────────────
# Menu-bar composite: speaker-mute button, master-volume slider (0–100),
# inset percent readout, right-click output-device picker. Verifies the
# widget→AudioEngine user path, the AudioEngine→widget echo path with
# re-entry prevention, AppSettings round-trip (audio/Master/Volume,
# audio/Master/Muted), and construction-time restore of saved values.
nereus_add_test(tst_master_output_widget)

# ── Phase 3O Sub-Phase 10 Task 10c: TitleBar host strip ─────────────────────
# 32 px strip that hosts the QMenuBar + MasterOutputWidget. Smoke only:
# construction, setMenuBar() re-parenting, masterOutput() accessor, and
# the fixed 32 px height. Live UI verification is the canonical check for
# the menu-re-parent + in-window render on macOS.
nereus_add_test(tst_title_bar)

# ── Shell-Chrome Redesign sub-PR-4 D.1: ConnectionSegment v2 ─────────────────
# 5 tests: state queries, rates+RTT round-trip, RTT-region left-click emits
# rttClicked(), right-click emits contextMenuRequested(), audio flow-state
# setter round-trip across all four FlowState values.
nereus_add_test(tst_connection_segment_v2)

# ── Phase 3O Sub-Phase 9 Task 9.2a: AudioEngine VAX gain/mute/tx-gain ────────
# Exercises setVaxRxGain / setVaxMuted / setVaxTxGain and their effect on the
# rxBlockReady tee — unity passthrough, half-gain scaling, muted-skip, signal
# emission, clamp-to-range, invalid-channel no-op. Same FakeAudioBus harness
# as tst_audio_engine_vax_tee.
nereus_add_test(tst_audio_engine_vax_gain)

# ── Phase 3O Sub-Phase 9 Task 9.2b: VaxApplet smoke tests ────────────────────
# Widget-level construction, signal plumbing, AppSettings round-trip.
nereus_add_test(tst_vax_applet)

# ── Phase 3O Sub-Phase 9 Task 9.2c: SpectrumOverlayPanel VAX Ch wiring ───────
# setRadioModel() binds the left-edge overlay's VAX Ch combo to slice 0's
# vaxChannel() bidirectionally; IQ Ch stays disabled (feature-flagged per
# design spec §6.7/§11.3).
nereus_add_test(tst_spectrum_overlay_panel)

# ── Phase 3Q Task 2: Unit tests for dBm-strip geometry and adaptive step ──────
# Covers stripRect (right-edge placement), arrowRowRect (top-of-strip),
# arrowHit (left=0, right=1, outside=-1), and adaptiveStepDb across
# the 20/10/5/2 dB boundaries. Uses QTEST_APPLESS_MAIN (no QApplication).
nereus_add_test(tst_spectrum_dbm_strip)

# ── Phase 3P-A Task 15: RxApplet S-ATT slider range from BoardCapabilities ───
# HL2 → 0-63 dB; Hermes → 0-31 dB. Verifies the spinbox max is sourced from
# BoardCapabilities::attenuator.maxDb at construction time, not hardcoded.
nereus_add_test(tst_rxapplet_att_range)
target_compile_definitions(tst_rxapplet_att_range PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3O Sub-Phase 12 Task 12.2 Step 0: AudioDeviceConfig round-trip ──────
# Verifies loadFromSettings / saveToSettings preserve all 10 fields, including
# edge cases: empty driverApi, manualLatencyMs=0, bitDepth=16/24/32, boolean
# combinations, and prefix independence (Speakers vs Headphones).
nereus_add_test(tst_audio_device_config_roundtrip)

# ── Phase 3O Sub-Phase 12 Task 12.2 Step 0: AudioEngine speakers live-reconfig ─
# Verifies speakersConfigChanged / headphonesConfigChanged / txInputConfigChanged
# / vaxConfigChanged signals, mutex safety in rxBlockReady (try_lock), and
# setSpeakersConfig no-crash while audio is simulated. Uses NEREUS_BUILD_TESTS
# seam for debounce bypass and fake bus injection.
nereus_add_test(tst_audio_engine_speakers_live_reconfig)
target_compile_definitions(tst_audio_engine_speakers_live_reconfig PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3O Sub-Phase 12 Task 12.2 Step 0: MasterOutputWidget signal refresh ──
# Verifies that speakersConfigChanged → onSpeakersConfigChanged syncs the widget's
# device-name anchor without emitting outputDeviceChanged (sync-from-engine path,
# not a user action). 5 test cases; runs on all platforms.
nereus_add_test(tst_master_output_widget_signal_refresh)

# ── Phase 3O Sub-Phase 12 Task 12.2: DeviceCard widget ─────────────────────────
# Verifies construction (Output, Input, enableCheckbox), currentConfig(),
# updateNegotiatedPill(), AppSettings round-trip, configChanged emission,
# and enabledChanged emission. 10 test cases.
nereus_add_test(tst_device_card)

# ── Phase 3O Sub-Phase 12 Task 12.3: AudioVaxPage + VaxChannelCard ─────────────
# Verifies VaxChannelCard construction (1–4), loadFromSettings, pill update,
# Auto-detect QMenu cases (no cables / free cable bind / input filtering),
# button visibility, and AudioVaxPage four-card layout. 10 test cases.
# Uses NEREUS_BUILD_TESTS seam setDetectedCablesForTest() to inject cable
# vectors without invoking PortAudio.
nereus_add_test(tst_audio_vax_page_auto_detect)
target_compile_definitions(tst_audio_vax_page_auto_detect PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3O Sub-Phase 12 Task 12.4: AudioEngine::resetAudioSettings ───────────
# Verifies the clear-vs-preserve boundary from addendum §2.5:
#   Cleared: audio/Speakers/*, Headphones/*, TxInput/*, Vax<1..4>/*,
#            DspRate, DspBlockSize, VacFeedback/<ch>/*, feature flags,
#            FirstRunComplete, LastDetectedCables.
#   Preserved: slice/<N>/VaxChannel, tx/OwnerSlot.
# Also verifies signal cascade: speakersConfigChanged, vaxConfigChanged(1..4),
# audioSettingsReset. 12 test cases.
nereus_add_test(tst_audio_engine_reset_audio_settings)
target_compile_definitions(tst_audio_engine_reset_audio_settings PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-B Task 10: per-ADC OVL split + RX1 preamp byte 1403 bit 1 ─────
# Single-ADC boards (HL2/Hermes): 1 OVL badge.
# Dual-ADC boards (OrionMKII, p2PreampPerAdc=true): 2 OVL badges.
# P2RadioConnection::setRx1Preamp(bool) routes to byte 1403 bit 1 via codec.
# Default state (rx1Preamp=false) leaves bit 1 clear → regression-freeze safe.
nereus_add_test(tst_rxapplet_adc_ovl)
target_compile_definitions(tst_rxapplet_adc_ovl PRIVATE NEREUS_BUILD_TESTS)

# ── Issue #174: HL2 A-ATT label flip on auto-att toggle ─────────────────────
# Strict mi0bot port from console.cs:21342-21365 [v2.10.3.13-beta2] AutoAttRX1
# property — on Hermes Lite 2, lblPreamp.Text flips S-ATT ↔ A-ATT when
# auto-attenuate is toggled.  Other boards keep "S-ATT" (ramdor/Thetis
# baseline).  Also covers the new autoAttEnabledChanged signal contract.
nereus_add_test(tst_rxapplet_a_att_label)
target_compile_definitions(tst_rxapplet_a_att_label PRIVATE NEREUS_BUILD_TESTS)

# ── Issue #174: OcOutputsHfTab N2ADR dead-checkbox tombstone ────────────────
# The "N2ADR Filter (HERCULES)" checkbox wrote to a global key with no
# consumer — removed in the same PR.  This tombstone test verifies the
# orphan-key cleanup helper works and that constructing the tab does not
# (re-)write the dead key.  If either fails, the dead checkbox is back.
nereus_add_test(tst_oc_outputs_hf_no_n2adr_writer)
target_compile_definitions(tst_oc_outputs_hf_no_n2adr_writer PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-C Step 2+3: preamp combo per-board contents contract ─────────────
# Locks BoardCapabilities::preampItemsForBoard() output 1:1 with Thetis
# SetComboPreampForHPSDR console.cs:40755-40825 [@501e3f5]:
#   Atlas no-Alex → 2 items (on_off); Atlas with Alex → 7 items (on_off+alex)
#   Hermes no-Alex → 4 items (anan100d); Hermes with Alex → 7 items
#   OrionMKII/Saturn → 4 items always; HL2 → 4 items (anan100d, Phase 3P-C fix)
# Also verifies RxApplet.preampComboItemCountForTest() reflects board caps at
# construction (Phase 3P-C Step 3: no more hardcoded "0dB/-10dB/-20dB/-30dB").
nereus_add_test(tst_preamp_combo)
target_compile_definitions(tst_preamp_combo PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-D Task 1: OcMatrix model + per-MAC persistence ──────────────────
# Verifies per-band × per-pin × {RX,TX} bit assignments, TXPinAction mapping
# (Thetis enums.cs:443 TXPinActions [@501e3f5]), and AppSettings round-trip
# under hardware/<mac>/oc/{rx,tx}/<band>/pin<n> and .../actions/pin<n>/action.
nereus_add_test(tst_oc_matrix)

# ── Phase 3P-D Task 2: OC Outputs HF sub-sub-tab UI + matrix wiring ──────────
# Smoke-tests construction, RX/TX pin sync (matrix→UI via changed()), reset
# flow, GEN band row disabled state, and default TXPinAction (MoxTuneTwoTone).
nereus_add_test(tst_oc_outputs_hf_tab)
target_compile_definitions(tst_oc_outputs_hf_tab PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3L: OC Outputs SWL sub-sub-tab UI + matrix wiring ──────────────────
# Smoke-tests construction, row 0 ↔ Band120m and row 12 ↔ Band11m sync,
# reset clears all 13 SWL bands × 7 pins (RX + TX), and HF amateur entries
# are isolated from SWL writes.  Replaces the QLabel placeholder previously
# at OcOutputsTab.cpp:90-102.
nereus_add_test(tst_oc_outputs_swl_tab)
target_compile_definitions(tst_oc_outputs_swl_tab PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3L: N2ADR Filter preset helper ─────────────────────────────────────
# Locks the per-band write table that backs both the N2ADR UI toggle
# (Hl2IoBoardTab::onN2adrToggled) and the app-launch reconcile
# (RadioModel::handleConnect).  Verifies 10 ham-band ham entries + 13 SWL
# pin-7 RX entries, the disable-wipes-all behaviour, that pre-existing
# manual entries are wiped on enable, and idempotency.
nereus_add_test(tst_n2adr_preset)
target_compile_definitions(tst_n2adr_preset PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3L: HL2 Options tab + model ────────────────────────────────────────
# Hl2OptionsModel: 9-knob defaults match mi0bot designer values verbatim,
# range-clamping per mi0bot setup.designer.cs, idempotent setters, per-MAC
# AppSettings round-trip.
# Hl2OptionsTab: construction smoke, model→UI sync, output LED strip
# reflects IoBoardHl2::currentOcByteChanged, I2C Write button gating.
nereus_add_test(tst_hl2_options_tab)
target_compile_definitions(tst_hl2_options_tab PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-D Task 3: P1RadioConnection ↔ OcMatrix codec wiring ─────────────
# Verifies:
#   - Without OcMatrix wired: bank 0 C2 = 0 (legacy m_ocOutput fallback).
#   - Matrix with pin 0 set for 20m RX: C2 = 0x02 ((mask=0x01) << 1).
#   - MOX selects TX matrix path: pin 1 for 40m TX → C2 = 0x04.
#   - Detaching matrix (setOcMatrix(nullptr)) reverts to legacy 0.
nereus_add_test(tst_p1_oc_matrix_wiring)
target_compile_definitions(tst_p1_oc_matrix_wiring PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-E Task 1: IoBoardHl2 model ──────────────────────────────────────
# Verifies the 32-slot circular I2C FIFO (enqueue/dequeue/depth/clear/overflow),
# 12-step UpdateIOBoard state machine cycle (advance wraps 11→0, all step
# descriptor strings match mi0bot console.cs:25844-25928 [@c26a8a4]), register
# mirror (256-byte array, HardwareVersion stored separately), and
# detection bool (idempotent setter, signal on change).
nereus_add_test(tst_io_board_hl2)

# ── Phase 3P-E Task 2: HL2 I2C intercept + ep6 read parsing ─────────────────
# Verifies that P1CodecHl2::tryComposeI2cFrame() encodes the I2C TLV frame
# (bus, address, control, write_data) into the 5 C&C bytes per
# mi0bot networkproto1.c:895-943 [@c26a8a4], that the queue is dequeued after
# compose, that non-HL2 boards noop, and that the ep6 I2C response test seam
# compiles and runs without crash.
nereus_add_test(tst_p1_hl2_i2c_wiring)
target_compile_definitions(tst_p1_hl2_i2c_wiring PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-E Task 3: HermesLiteBandwidthMonitor ────────────────────────────
# Verifies the port of mi0bot bandwidth_monitor.{c,h} (MW0LGE [@c26a8a4]):
#   - initial state (zero rates, not throttled, zero event count)
#   - first-tick zero return (prev_ms == 0 branch in compute_bps)
#   - bytes <= 0 are silently ignored (upstream if (bytes > 0) guard)
#   - positive ingress rate after two real-clock ticks (delta path)
#   - throttle requires ep2 to be active (not just ep6 silence)
#   - throttle asserted after kThrottleTickThreshold consecutive silent ticks
#   - throttle clears when ep6 resumes
#   - throttle event count increments per throttle assertion
#   - liveStatsUpdated emitted on every tick()
#   - reset() clears all state (upstream bandwidth_monitor_reset() parity)
#   - reset() emits throttledChanged(false) when previously throttled
nereus_add_test(tst_hermes_lite_bandwidth_monitor)

# ── Phase 3P-E Task 4: Hardware → HL2 I/O Setup page UI ──────────────────────
# Smoke-tests construction for HL2 and non-HL2 boards, signal-driven UI
# updates (register change, step advance, detection, throttle, I2C queue)
# without crashes. Auto-hide for non-HL2 boards verified via construction.
nereus_add_test(tst_hl2_io_board_tab)
target_compile_definitions(tst_hl2_io_board_tab PRIVATE NEREUS_BUILD_TESTS)

# ── hermes-filter-debug Bug 2: N2ADR per-MAC persistence + migration ─────────
# Round-trip via setHardwareValue/hardwareValue, multi-MAC isolation, and
# AppSettings::migrateLegacyN2adrFilter (global → per-MAC, HL2-only filter,
# idempotency, preserve-explicit-per-MAC). Pure AppSettings/QTemporaryDir
# unit tests; QTEST_APPLESS_MAIN.
nereus_add_test(tst_hl2_n2adr_persistence)

# ── hermes-filter-debug Bug 2: Hl2IoBoardTab N2ADR-toggle behaviour ──────────
# Smoke-style behavioural tests for the rewired toggle path:
#   * onN2adrToggled emits settingChanged (per-MAC routing via HardwarePage).
#   * applyN2adrMatrix populates / wipes OcMatrix.
#   * restoreSettings does NOT wipe matrix when the n2adrFilter key is
#     absent (the regression that made the filter "always start disabled").
#   * restoreSettings does not echo settingChanged back to HardwarePage.
nereus_add_test(tst_hl2_io_board_tab_n2adr)
target_compile_definitions(tst_hl2_io_board_tab_n2adr PRIVATE NEREUS_BUILD_TESTS)

# ── hermes-filter-debug Bug 1: HL2 step-att signed range ─────────────────────
# Verifies StepAttenuatorController honours a negative minAttenuation
# (no clamp-to-zero) and that RxApplet::setBoardCapabilities propagates the
# board-driven range to its S-ATT spinbox.
nereus_add_test(tst_hl2_step_att_range)
target_compile_definitions(tst_hl2_step_att_range PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-F Task 1: AlexController model — per-band antennas + Block-TX ───
# Verifies per-band TX/RX/RX-only antenna arrays (14 bands, default Ant 1),
# isolation (one band's assignment doesn't affect others), Block-TX safety
# (blockTxAnt2/3 rejects setTxAnt to that port but not setRxAnt),
# clamping (values outside 1..3 clamped to 1 or 3), setAntennasTo1 force-mode,
# antennaChanged signal emission, and per-MAC AppSettings persistence round-trip.
# Ports HPSDR/Alex.cs:30-106 [@501e3f5].
nereus_add_test(tst_alex_controller)

# ── Phase 3P-I-a T14: RadioModel antenna pump integration test ────────────────
# Exercises the full T9/T10/T11 signal path through a mock RadioConnection that
# captures every setAntennaRouting call. Covers: rxAnt change for current band
# reaches wire, band-crossing reapplies new band's antenna, connect applies
# persisted antenna, and !caps.hasAlex writes zero routing (Thetis SetAntBits
# parity). Uses NEREUS_BUILD_TESTS hooks on RadioModel for injection/simulation.
nereus_add_test(tst_antenna_routing_model)
target_compile_definitions(tst_antenna_routing_model PRIVATE NEREUS_BUILD_TESTS)

# Smoke test for Antenna Control sub-sub-tab UI (Phase 3P-F Task 3):
# Widget construction + AlexController wiring + Block-TX safety rejection.
nereus_add_test(tst_antenna_control_tab)
target_compile_definitions(tst_antenna_control_tab PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-F Task 4: RxApplet antenna buttons per-band wiring ───────────────
# Verifies that RxApplet's RX/TX antenna buttons (m_rxAntBtn / m_txAntBtn)
# populate from AlexController at construction time for the current band,
# auto-switch when PanadapterModel::bandChanged fires, and repopulate when
# AlexController::antennaChanged fires for the current band.
nereus_add_test(tst_rxapplet_antenna_buttons)
target_compile_definitions(tst_rxapplet_antenna_buttons PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-F Task 5a: ApolloController model ───────────────────────────────
# Smoke test: defaults all-off (present/filter/tuner = false), individual setter
# signal emission (idempotent no-fire), and per-MAC AppSettings persistence round-trip.
# Ports setup.cs:15566-15590 + console.cs:19060-19105 [@501e3f5].
nereus_add_test(tst_apollo_controller)

# ── Phase 3P-F Task 5b: PennyLaneController model ────────────────────────────
# Smoke test: default extCtrlEnabled = true (matches Thetis penny_ext_ctrl_enabled=true),
# setter signal emission (idempotent no-fire), and per-MAC AppSettings persistence.
# Ports HPSDR/Penny.cs (full) + console.cs:14899 + setup.cs:12679 [@501e3f5].
nereus_add_test(tst_penny_lane_controller)

# ── Phase 3P-H Task 1: RadioStatus + SettingsHygiene + PttSource models ──────
nereus_add_test(tst_ptt_source)
nereus_add_test(tst_radio_status)
nereus_add_test(tst_settings_hygiene)

# ── Phase 3P-H Task 4: status-packet parsing + RadioModel ownership ──────────
# tst_p1_status_telemetry: feeds a hand-crafted 1032-byte ep6 datagram
#   into P1RadioConnection::parseEp6FrameForTest and asserts the new
#   paTelemetryUpdated() signal carries the C0=0x08/0x10/0x18 raw values
#   per Thetis networkproto1.c:332-356 [@501e3f5].
# tst_p2_status_telemetry: same for P2's 60-byte High-Priority status
#   packet at byte offsets 2-3, 10-11, 18-19, 45-46, 51-52, 53-54
#   per Thetis network.c:711-748 [@501e3f5].
# tst_radio_model_status_wiring: verifies RadioModel owns RadioStatus +
#   SettingsHygiene as single instances, that the public radioStatus()
#   accessor surfaces signals to the embedded sub-model, and that
#   settingsHygiene().validate() (called from onConnectionStateChanged
#   on Connected) populates issues for an out-of-range persisted S-ATT.
nereus_add_test(tst_p1_status_telemetry)
nereus_add_test(tst_p2_status_telemetry)
nereus_add_test(tst_radio_model_status_wiring)

# ── Issue #176: P1 EP6 status-frame parser was reading C0 bit 0 (mic_PTT)
# and emitting it as adcOverflow, causing the OVL badge to fire on every
# footswitch press while real ADC saturations went unreported.  This test
# constructs hand-crafted ep6 frames and verifies adcOverflow / micPttFromRadio
# fire from independent bytes per Thetis networkproto1.c:329 + 332-355
# [v2.10.3.13]:
#   ptt_in   = ControlBytesIn[0] & 0x1               (C0 bit 0)
#   case 0x00: adc[0] = ControlBytesIn[1] & 0x01     (C1 bit 0)
#   case 0x20: adc[0..2] = ControlBytesIn[1..3] & 0x01
nereus_add_test(tst_p1_adc_overflow_extraction)

# ── Phase 3P-G commit 1: CalibrationController model ─────────────────────────
# Verifies defaults (freqFactor=1.0, all offsets=0), setter signal emission
# (idempotent no-fire), effectiveFreqCorrectionFactor() toggle between normal
# and 10 MHz ref factor, and per-MAC AppSettings persistence round-trip.
# Ports setup.cs:5137-5144; 14036-14050; 22690-22706; 14325; 17243; 18315 +
# console.cs:9766-9844; 21022-21086 [@501e3f5].
nereus_add_test(tst_calibration_controller)

# ── P1 full-parity §3.2: CalibrationController PA-cal storage + persistence ─
# Verifies setPaCalProfile / setPaCalPoint signals + de-dup, idx-zero +
# out-of-range guards, calibratedFwdPowerWatts forwarding, and per-MAC
# AppSettings round-trip for boardClass + 10 cal-point watts entries.
# Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13+501e3f51].
nereus_add_test(tst_calibration_controller_pa_cal)

# ── P1 full-parity §3.4: alex_fwd routing through CalibratedPAPower ─────────
# Verifies that RadioModel::handlePaTelemetry routes the raw alex_fwd watts
# through CalibrationController::calibratedFwdPowerWatts before publishing
# to RadioStatus.  Identity (no profile) and active (Anan100 with mutated
# watts[5]) paths plus a guard that the reflected-power path is unaffected.
# Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13+501e3f51].
nereus_add_test(tst_radio_model_pa_cal_routing)

# ── P1 full-parity §3.5: SWR-foldback drive scaling ─────────────────────────
# Verifies the TransmitModel::swrProtectFactor property (clamp + signal
# de-dup) and the percent→wire-byte formula
#   wire = clamp(int(255 * f * swrProtect), 0, 255)
# applied at every RadioModel setTxDrive site.  At default factor=1.0 the
# formula is identity to the prior `(power * 255) / 100` — bench behaviour
# for live radios is unchanged until SwrProtectionController is wired in
# a follow-up task.
# Source: mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1].
nereus_add_test(tst_pa_cal_swr_foldback)
target_compile_definitions(tst_pa_cal_swr_foldback PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-G commit 2: CalibrationTab UI smoke tests ───────────────────────
# Verifies construction doesn't crash, 5 group boxes present
# (Freq Cal / Level Cal / HPSDR Freq Cal Diagnostic / TX Display Cal /
# Volts/Amps Calibration), controller->UI sync, and UI->controller write-back.
# Group 6 (PA Forward Power Calibration / PaCalibrationGroup) was migrated
# to PA → Watt Meter on 2026-05-02 (Setup IA reshape Phase 3A) — so the
# tab is back to 5 groups; see tst_pa_watt_meter_page for the migrated
# group's integration coverage.  Group 5 was relabelled from "PA Current (A)
# calculation" to "Volts/Amps Calibration" 2026-05-02 (Phase 3B).
nereus_add_test(tst_calibration_tab)
target_compile_definitions(tst_calibration_tab PRIVATE NEREUS_BUILD_TESTS)

# ── P1 full-parity §3.3: PaCalibrationGroup widget ───────────────────────────
# Verifies the per-board-class PA forward-power cal-point spinbox group:
# default construction has zero spinboxes; populate() builds 10 spinboxes
# with class-specific labels (1W..10W / 10W..100W / 20W..200W / 0.5W..5W);
# None hides the group; spinbox→controller writes via setPaCalPoint;
# controller→spinbox sync via paCalPointChanged; repopulate rebuilds; echo-
# loop guard prevents recursion.
# Source: Thetis setup.cs:5404-5594 ud{10|100|200}PA{N}W spinbox blocks
# [v2.10.3.13+501e3f51].
nereus_add_test(tst_pa_calibration_group)
target_compile_definitions(tst_pa_calibration_group PRIVATE NEREUS_BUILD_TESTS)

# ── Setup IA reshape Phase 3A: PaCalibrationGroup integrated into ────────────
# PA → Watt Meter (PaWattMeterPage). Verifies the page hosts exactly one
# PaCalibrationGroup child, populates from the controller's profile at
# construction, and rebuilds on CalibrationController::paCalProfileChanged
# (radio-swap path). The widget itself is exercised by tst_pa_calibration_group;
# this test pins the host integration only.
nereus_add_test(tst_pa_watt_meter_page)
target_compile_definitions(tst_pa_watt_meter_page PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 5 Agent 5A of #167: PaWattMeterPage gap fill ────────────────────────
# Verifies the two missing Thetis controls now wired into PaWattMeterPage:
#   - chkPAValues toggle  → AppSettings "display/showPaValuesPage" round-trip,
#                            default True on first run.
#   - btnResetPAValues    → emits resetPaValuesRequested() on click;
#                            PaValuesPage (Phase 5B) subscribes for peak/min
#                            tracking reset.
# Source: Thetis chkPAValues + btnResetPAValues setup.designer.cs:51155-51177
#         + setup.cs:16340-16357 [v2.10.3.13].
nereus_add_test(tst_pa_watt_meter_page_gap_fill)
target_compile_definitions(tst_pa_watt_meter_page_gap_fill PRIVATE NEREUS_BUILD_TESTS)

# ── Setup IA reshape Phase 4: live PA Values page ────────────────────────────
# Verifies PaValuesPage builds at least 8 MetricLabel children and updates
# them when the bound RadioStatus + RadioConnection signals fire:
#   - powerChanged → FWD calibrated, REV, SWR labels
#   - paTemperatureChanged / paCurrentChanged → temperature / current labels
#   - supplyVoltsChanged → supply volts label
#   - paTelemetryUpdated → FWD/REV ADC raw labels
#   - adcOverflow → overload label
#   - null-model construction is safe (model-less preview fallback)
# Source: Thetis panelPAValues setup.designer.cs:51155-51177 [v2.10.3.13].
nereus_add_test(tst_pa_values_page)
target_compile_definitions(tst_pa_values_page PRIVATE NEREUS_BUILD_TESTS)

# ── PA hotfix #167 Phase 5B: PaValuesPage 4-label gap fill + peak/min ─────────
# Verifies the four new MetricLabels added in Phase 5B (raw FWD power,
# drive, FWD voltage, REV voltage) populate correctly from the
# PaTelemetryScaling helpers (Phase 1B) and from
# TransmitModel::powerChanged.  Also verifies peak/min running tracking
# on FWD-calibrated / REV / SWR / PA-current / PA-temp / supply-volts,
# the in-page Reset button, and the resetPaValues() public slot used by
# Phase 5A's PaWattMeterPage::resetPaValuesRequested cross-page wiring.
# Source: Thetis panelPAValues setup.designer.cs:51155-51177 +
#         btnResetPAValues_Click setup.cs:16346-16357 [v2.10.3.13].
nereus_add_test(tst_pa_values_page_gap_fill)
target_compile_definitions(tst_pa_values_page_gap_fill PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 6 of #167: PaGainByBandPage live editor ───────────────────────────
# Verifies the live editor that replaced the Phase 2 placeholder body in
# PaSetupPages.cpp:117-145.  Surface mirrors Thetis tpGainByBand
# (setup.designer.cs:47386-47525 [v2.10.3.13]):
#   - profile combo (comboPAProfile) + 4 lifecycle buttons
#     (New / Copy / Delete / Reset Defaults)
#   - 14-band gain spinbox grid (nud160M..nudVHF13)
#   - 14x9 drive-step adjust spinbox matrix (panelAdjustGain — NereusSDR
#     densification: Thetis ships only the row for the selected band)
#   - per-band max-power column (nudMaxPowerForBandPA + chkUsePowerOnDrvTunPA)
#   - warning icon + label (pbPAProfileWarning + lblPAProfileWarning)
#   - chkPANewCal "New Cal" mode toggle
#   - chkAutoPACalibrate auto-cal toggle (Phase 7 wires the sweep flow)
# Lifecycle behavior wired through PaProfileManager (Phase 2B). Tests cover
# combo population, value round-trips, button flows (New / Copy / Delete /
# Reset Defaults), warning visibility on profile divergence.
nereus_add_test(tst_pa_gain_by_band_page_editor)
target_compile_definitions(tst_pa_gain_by_band_page_editor PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 7 of #167: PaGainByBandPage auto-cal sweep state machine ──────────
# Verifies the auto-cal sweep state machine wired to the chkAutoPACalibrate
# checkbox added in Phase 6. The sweep iterates HF bands (160m..6m) x drive
# levels (10/20/.../90), engages TUNE at each step, reads calibrated FWD
# power via PaCalProfile::interpolate(), computes the gain delta from
# observed-vs-target, and writes new gain values into the active PaProfile.
# Mirrors Thetis CalibratePAGain at console.cs:10228-10387 [v2.10.3.13]
# (HF-only band loop) and chkAutoPACalibrate_CheckedChanged at
# setup.cs:15683-15698 [v2.10.3.13] (panel toggle).
# Tests cover: initial Idle state, toggle starts sweep, state transitions
# across bands, profile gain values updated, safety abort on over-power,
# user cancel halts cleanly (no partial writes), completion writes profile
# via PaProfileManager, settle timer dispatches readings, cancel during
# initialization, HF-only sweep skips VHF/GEN/WWV/XVTR.
nereus_add_test(tst_pa_gain_by_band_page_auto_cal)
target_compile_definitions(tst_pa_gain_by_band_page_auto_cal PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-H Task 5a: Alex-2 Filters live LED selection ────────────────────
# Verifies AntennaAlexAlex2Tab::updateActiveLeds() follows Thetis
# console.cs:setAlex2HPF / setAlex2LPF (7060-7299) first-match range logic
# [@501e3f5], tracks PanadapterModel::centerFrequencyChanged, and
# surfaces the BP bypass fallback on no-row-match + master toggle.
nereus_add_test(tst_alex2_live_leds)
target_compile_definitions(tst_alex2_live_leds PRIVATE NEREUS_BUILD_TESTS)

# ── Alex-1 Filters live LED (UX parity follow-up, 2026-04-21) ─────────────────
# Mirrors tst_alex2_live_leds for the Alex-1 Filters sub-sub-tab. Verifies
# AntennaAlexAlex1Tab::updateActiveLeds() lights the correct HPF/LPF row
# for the current SliceModel frequency per Thetis console.cs:setAlexHPF /
# setAlexLPF [@501e3f5]. Does not cover the Saturn BPF1 column — that is a
# separate panel with its own edge table and needs its own LED work if
# Saturn-mode BPF1 is enabled.
nereus_add_test(tst_alex1_live_leds)
target_compile_definitions(tst_alex1_live_leds PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-H Task 5b: OC Outputs live pin-state LED row ─────────────────────
# Verifies OcOutputsHfTab::onLiveStateChanged() computes the displayed OC
# byte = OcMatrix::maskFor(currentBand, isTx) via RadioModel's first
# panadapter + TransmitModel MOX state. Covers empty matrix, RX pin set
# for 20m, band-change recompute, MOX-driven RX↔TX matrix flip, and
# direct setCurrentOcByte path.
nereus_add_test(tst_oc_outputs_live_pins)
target_compile_definitions(tst_oc_outputs_live_pins PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-H Task 5c: HL2 I/O live register polling + bandwidth meter ───────
# Verifies Hl2IoBoardTab spins a 40 ms register-table QTimer (matching
# the group-box title "Register state  (polled @ 40 ms)") that refreshes
# each cell from IoBoardHl2::registerValue, plus the 250 ms bandwidth
# QTimer (Task E3) driving ep6/ep2 Mbps labels + throttle status from
# HermesLiteBandwidthMonitor.
nereus_add_test(tst_hl2_live_polling)
target_compile_definitions(tst_hl2_live_polling PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-I-a T21: widget-level capability gating ──────────────────────────
# Verifies that setBoardCapabilities() on VfoWidget and RxApplet actually
# hides / shows the m_rxAntBtn / m_txAntBtn children based on
# caps.hasAlex + antennaInputCount. Coupled to the setObjectName calls in
# those widgets — the test relies on findChild<QPushButton*>(name) and
# will FAIL LOUDLY if the objectName strings drift.
nereus_add_test(tst_ui_capability_gating)
target_compile_definitions(tst_ui_capability_gating PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3P-I-a T22: kPopupMenu stylesheet invariant ────────────────────────
# Grep-scans the known antenna QMenu call sites and fails if any
# `menu.exec(...)` is not preceded by a `setStyleSheet` using kPopupMenu.
# Gates issue #98 regression (dark-on-dark popup menus on Ubuntu 25.10
# GNOME). NEREUS_SOURCE_ROOT lets the test resolve paths relative to
# the source tree regardless of build directory.
nereus_add_test(tst_popup_style_coverage)
target_compile_definitions(tst_popup_style_coverage PRIVATE
    NEREUS_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# ── Phase 3P-I-b T1: SkuUiProfile per-HPSDRModel UI overlay ──────────────────
# Verifies every HPSDRModel maps to the label + flag set from Thetis
# setup.cs:19832-20375. 8 cases: hermes, anan10, anan100, anan7000d,
# anan8000d, anan_g2, anan_g2_1k, hermeslite.
nereus_add_test(tst_sku_ui_profile)

# ── Phase 3P-I-b T2: AntennaLabels facade — antennaLabels() + rxOnlyLabels() ─
# Exercises the delegator surface: antennaLabels(caps) empty-on-no-alex,
# three-port ANT1/2/3 list, and rxOnlyLabels(sku) for hermes / anan_g2 /
# anan100 families. 5 cases. Values come indirectly from SkuUiProfile which
# cites Thetis setup.cs:19846-20375 [v2.10.3.13 @501e3f5].
nereus_add_test(tst_antenna_labels)

# ── Phase 3G RX Epic sub-epic D: BandPlanManager loader + active-plan API ───
# TDD red bar: tests are registered here but BandPlanManager.h does not exist
# yet. Build of this target will fail until Task 3 ports BandPlanManager from
# AetherSDR [@0cd4559]. Tests cover:
#   - BandSegment / BandSpot value-type defaults (Task 2 types from BandPlan.h)
#   - loadPlans() discovers all 5 bundled JSON files (ARRL/IARU-R1/R2/R3/RAC)
#   - setActivePlan() emits planChanged, ignores unknown names
#   - segments() / spots() reflect the active plan
#   - Default plan after loadPlans() is "ARRL (US)" (no AppSettings key)
nereus_add_test(tst_bandplan_manager)

# ── Phase 3G RX Epic sub-epic E: waterfall scrollback math helpers ──────────
# Pure-arithmetic checks for the capacity-clamp + age→row formulas used by
# SpectrumWidget's scrollback implementation. The test mirrors the formula in
# a parallel shim so it can run without a QApplication or GPU widget; the
# shim references SpectrumWidget::kMaxWaterfallHistoryRows directly so any
# drift in the cap is caught by the build.
nereus_add_test(tst_waterfall_scrollback)

# ── Task 2.8: WaterfallDefaultsPage changes (NF-AGC, Stop-on-TX, W5 removal) ─
# NF-AGC setter round-trips, offset clamp, stop-on-TX setter round-trips,
# and smoke test for setActivePeakHoldTxActive + setWaterfallStopOnTx combo.
nereus_add_test(tst_waterfall_defaults_changes)

# ── Task 2.9: NF-aware grid + Copy button + NoiseFloorEstimator::prime() ─────
# Thetis source: console.cs:46074-46086 [v2.10.3.13] GridMinFollowsNFRX1 /
# tmrAutoAGC_Tick NF grid block. NereusSDR-original: global panadapter default.
# Tests: offset_shifts_grid_min, maintain_delta, disabled_no_change, deadband
# suppression + allow at 2dB, positive offset, prime() store + update + no
# interference with estimate().
nereus_add_test(tst_nf_aware_grid)

# ── Phase 3M-0 Task 2: BandPlanGuard region tables + 60m channels ─────────
# Pure data + pure predicates; no Qt parent, no signals.
# 12 table-driven tests cover US/UK/Japan 60m channelization, extended bypass,
# different-band guard, and 20m in/out-of-band cases.
nereus_add_test(tst_band_plan_guard)

# ── Phase 3M-1b Task K.1: BandPlanGuard SSB-mode allow-list for TX ─────────
# isModeAllowedForTx: 4 allowed (LSB/USB/DIGL/DIGU) + 8 rejected
# (CWL/CWU/AM/SAM/DSB/FM/DRM/SPEC). checkMoxAllowed: reason strings for each
# rejection class + ok path (LSB/USB on valid freq) + freq/band reject on
# an otherwise-allowed mode. ~20 test cases.
nereus_add_test(tst_band_plan_guard_mode_allow_list)

# ── Phase 3M-1b Task K.2: MoxController rejection signal + tooltip override ──
# Tests the full K.2 plumbing:
#   MoxController::setMoxCheck(fn) + moxRejected(QString) signal.
#
# MoxController cases (8):
#   1. CWL → moxRejected("CW TX coming in Phase 3M-2"); state stays Rx.
#   2. CWU → moxRejected with CW reason.
#   3. AM  → moxRejected("AM/FM TX coming in Phase 3M-3 (audio modes)"); state stays Rx.
#   4. FM  → moxRejected with audio-modes reason.
#   5. LSB (allowed) → MOX engages; moxRejected NOT emitted.
#   6. USB (allowed) → same.
#   7. No MoxCheckFn installed → setMox(true) succeeds (backwards-compat).
#   8. setMox(false) never rejected (release path bypasses BandPlanGuard).
#   Extra: SPEC → "Mode not supported for TX"; rejection is repeatable;
#          txAboutToBegin NOT emitted on rejection.
#
# TxApplet::tooltipForMode static helper (12):
#   USB/LSB/DIGL/DIGU → normal "Manual transmit (MOX)" tooltip.
#   CWL/CWU → "CW TX coming in Phase 3M-2".
#   AM/FM/SAM/DSB/DRM → "AM/FM TX coming in Phase 3M-3 (audio modes)".
#   SPEC → "Mode not supported for TX".
#
# Total: ~20 test cases. Uses QTEST_GUILESS_MAIN; no RadioModel required.
nereus_add_test(tst_band_plan_guard_mox_rejection)

# ── Phase 3M-0 Task 3: SwrProtectionController PollPAPWR state machine ─────
# 8 tests cover the two-stage foldback (per-sample + 4-trip windback latch),
# open-antenna heuristic (swr=50 / factor=0.01), low-power floor clamp,
# tune-time bypass, moxOff() latch clear, and latch persistence without moxOff.
# Ports Thetis console.cs:25933-26120 [v2.10.3.13] (PollPAPWR) and
# console.cs:29191-29195 [v2.10.3.13] (UIMOXChangedFalse).
nereus_add_test(tst_swr_protection_controller)

# ── Phase 3M-0 Task 4: TxInhibitMonitor GPIO poll + 4-source aggregation ─────
# 8 tests cover: startup no-inhibit, UserIo01 active-low, reverse-logic inversion,
# immediate Rx2OnlyRadio / OutOfBand / BlockTxAntenna notification paths,
# multi-source priority ordering + all-clear, and poll-cadence duplicate suppression.
# Ports Thetis console.cs:25801-25839 [v2.10.3.13] (PollTXInhibit loop).
nereus_add_test(tst_tx_inhibit_monitor)

# ── Phase 3M-0 Task 5 (updated 3M-1a E.5): setWatchdogEnabled(bool) ──────────
# 8 tests assert boolean state-tracking on RadioConnection (P1 + P2).
# Default changed from false to true in 3M-1a E.5 (see RadioConnection.h).
# Wire-format assertions (RUNSTOP pkt[3] bit 7) are in tst_p1_watchdog_wire.
# API mirrors NetworkIOImports.cs:197-198 [v2.10.3.13] (DllImport SetWatchdogTimer).
# HL2 cite: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400.
nereus_add_test(tst_radio_connection_watchdog)

# ── Phase 3Q Task 3: ConnectFailure enum + connectFailed() signal ─────────────
# 2 tests: emitsTypedFailureOnUnreachable (RFC 5737 black-hole, 2s connect
# watchdog must fire ConnectFailure::Timeout) and noFailureOnIntentionalDisconnect
# (disconnect() before watchdog fires must not emit connectFailed).
# NereusSDR-original infrastructure; no Thetis attribution needed.
nereus_add_test(tst_radio_connection_failure)

# ── Phase 3M-1a Task E.1: TX virtual interface on RadioConnection ──────────
# 16 tests assert:
#   - sendTxIq() is callable on P1 + P2 without crash (stub bodies; wire
#     format filled by Tasks E.2 / E.6 — no byte assertions here).
#   - setTrxRelay() stores state in base-class m_trxRelay, readable via
#     isTrxRelayEngaged(); round-trip and idempotency covered.
#   - setMox / setWatchdogEnabled / setTxDrive unchanged (regression guards).
# Cite: 3M-1a plan Task E.1; Master design §5.1.4.
nereus_add_test(tst_radio_connection_tx_iface)

# ── Phase 3M-1a Task E.2: P1 sendTxIq wire-byte snapshot ─────────────────
# 9 tests verify the EP2 TX I/Q zone byte layout for P1RadioConnection:
#   - Unit pulse (I=1.0 → 0x7FFF), subframe 1 zone offset, partial fill,
#     multi-frame sequence increment, sign/clamp, over-range clamp,
#     underrun silence, mic bytes zero (NullMicSource), zone-0 last sample.
# Porting from deskhpsdr/src/old_protocol.c:2421-2458, transmitter.c:1541
# [@120188f].
nereus_add_test(tst_p1_tx_iq_wire)
target_compile_definitions(tst_p1_tx_iq_wire PRIVATE NEREUS_BUILD_TESTS)

# ── 3M-1a Task E.3: P1 MOX wire-byte emission ────────────────────────────────
# 13 tests covering:
#   1. Default state: C0 bit 0 clear before any setMox call.
#   2. setMox(true) → C0 bit 0 = 1 (bank-0 compose path).
#   3. setMox(false) → C0 bit 0 = 0.
#   4. Bank-0 address bits (7..1) remain 0 after setMox(true).
#   5. setMox() sets m_forceBank0Next so the next frame sends bank-0 ASAP.
#   6. Repeated setMox with same value still sets the flush flag (Codex P2).
#   7. Idempotent setMox(true) twice doesn't crash.
#   8. Round-trip true→false produces correct bytes.
#   9. Sub-frame 0 byte 11 (C0) carries the MOX bit.
#   10. Sub-frame 1 bank-1 C0 is not bit-0-only (address field differs).
#   11. HL2 firmware cross-check: bit 0 of eth_data[0] = PTT (dsopenhpsdr1.v:297).
#   12-13. Static composeCcBank0 helper agrees on bit position.
# Source: deskhpsdr/src/old_protocol.c:3595-3599 [@120188f].
nereus_add_test(tst_p1_mox_wire)
target_compile_definitions(tst_p1_mox_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task E.4: P1 T/R relay wire-byte snapshot ────────────────────
# 11 tests pin the Alex T/R relay bit (C3 bit 7 of bank 10, C0=0x12):
#   1. Default state: relay disengaged → bit 7 = 1.
#   2. setTrxRelay(true) → bit 7 = 0 (relay engaged, normal TX).
#   3. setTrxRelay(false) → bit 7 = 1 (relay disengaged, PA bypass).
#   4. Bank 10 C0 address stays 0x12 regardless of relay state.
#   5. setTrxRelay() sets m_forceBank10Next (Codex P2 flush pattern).
#   6. Repeated same-value call still sets the flush flag.
#   7. setTrxRelay(true) twice doesn't crash.
#   8. Round-trip: true → false → true produces correct bit values.
#   9. Alex HPF bits (C3 bits 6..0) are preserved across relay state changes.
#  10. HL2 firmware cross-check: bank 10 C0 maps to cmd_addr 6'h09 (informational).
#  11. Flush flag is set even when state doesn't change (false→false no-op).
# Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f].
# Latent bug fixed: prior code wrote (m_paEnabled ? 0x80 : 0) — INVERTED.
nereus_add_test(tst_p1_trx_relay_wire)
target_compile_definitions(tst_p1_trx_relay_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c HL2 bench triage (2026-04-29): P1 setTxDrive wire-byte ───────
# 12 tests assert that bank 10 C1 carries the drive level set via setTxDrive().
#
# Bug fix: setTxDrive(int) was a no-op stub from 3M-1a; m_txDrive stayed at 0
# forever, so HL2 (and every P1 family member) silently shipped with zero TX
# drive and produced no RF on TUN or SSB.  PR #149 SSB voice bench tests ran
# only on ANAN-G2 (P2), masking the latent P1 stub.
#
# Standard and HL2 codec paths share the bank-10 C1 encoding per
# Thetis networkproto1.c:579 [v2.10.3.13] (= mi0bot @c26a8a4 line 1061);
# both codec paths exercised below.
nereus_add_test(tst_p1_drive_level_wire)
target_compile_definitions(tst_p1_drive_level_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task E.5: P1 RUNSTOP watchdog wire-byte snapshot ─────────────
# 12 tests assert that pkt[3] bit 7 of the RUNSTOP packet correctly encodes
# m_watchdogEnabled (inverted: 0=enabled, 1=disabled) per HL2 firmware.
#
# Wire cite: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
#   watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
#
# deskhpsdr reference: deskhpsdr/src/old_protocol.c:3811 [@120188f]
#   buffer[3] = command; // no bit-7 OR -- watchdog always enabled implicitly
#
# Default changed: m_watchdogEnabled{false} -> {true} in RadioConnection.h
# so the first sendMetisStart() writes bit 7 = 0 (watchdog active), matching
# deskhpsdr implicit behavior.
nereus_add_test(tst_p1_watchdog_wire)
target_compile_definitions(tst_p1_watchdog_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task E.6: P2 sendTxIq wire-byte snapshot ─────────────────────
# 14 tests verify the 1444-byte UDP TX I/Q frame layout for P2RadioConnection:
#   1. Frame size = 1444 bytes (4-byte seq + 1440-byte payload).
#   2. Payload = 240×6 = 1440 bytes.
#   3. Sequence number starts at 0 and increments per frame.
#   4. Positive full-scale I (+1.0) → bytes 0x7F 0xFF 0xFF.
#   5. Negative full-scale Q (-1.0) → bytes 0x80 0x00 0x01.
#   6. Zero sample → all payload bytes zero.
#   7. Over-range positive (+2.0) clips to 0x7FFFFF.
#   8. Over-range negative (-2.0) clips to 0x800001.
#   9. Underrun (empty ring) → silence (all payload zeros).
#  10. Partial fill (60 of 240 samples) → remaining slots zero-padded.
#  11. Ring saturation: excess beyond capacity dropped, count ≤ capacity.
#  12. Sequence number does not reset across multiple calls.
#  13. Last sample (index 239) at correct offset (byte 1438).
#  14. No HL2 CWX LSB-clear in P2 — LSB=1 preserved.
# Porting from deskhpsdr/src/new_protocol.c:2795-2837 [@120188f].
nereus_add_test(tst_p2_tx_iq_wire)
target_compile_definitions(tst_p2_tx_iq_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task E.7: P2 MOX wire byte (byte 4 bit 1 = 0x02) ─────────────
# 10 tests verify:
#   1-5. Legacy path (no codec): default=0, setMox(true)→1, setMox(false)→0,
#        round-trip, run-bit unaffected, bits-2-7 zero.
#   6-8. Codec path (setBoardForTest): OrionMKII + Saturn both land MOX at bit 1.
#   9.   Idempotency.
#  10.   CW byte (byte 5) unaffected.
# Fixes latent bug: prior code wrote m_tx[0].pttOut to byte 4 bit 1;
# setMox() now writes m_mox (the correct source).
# Source: deskhpsdr/src/new_protocol.c:739-762 [@120188f].
nereus_add_test(tst_p2_mox_wire)
target_compile_definitions(tst_p2_mox_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task E.7: P2 TX drive level wire (byte 345) ──────────────────
# 10 tests verify:
#   1.   Default state: byte 345 = 0.
#   2-5. setTxDrive(50/255/300/-10): value/clamp-high/clamp-low.
#   6-7. Codec path (OrionMKII + Saturn): matches legacy.
#   8-9. Out-of-band gate: in-band → drive passes; GEN-band → byte 345 = 0.
#  10.   Round-trip: two successive setTxDrive calls.
# Out-of-band gate ported from deskhpsdr/src/new_protocol.c:864-876 [@120188f].
# BandPlanGuard does not zero driveLevel upstream; gate is at compose time.
nereus_add_test(tst_p2_drive_level_wire)
target_compile_definitions(tst_p2_drive_level_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-0 Task 6: RadioModel::paTripped() live state + Ganymede handler ─
# 5 tests cover initial state (clear), trip latch (non-zero tripState → true),
# latch reset (resetGanymedePa()), absent-PA clear (setGanymedePresent(false)),
# and the MOX-drop safety side effect on trip latch.
# Ports Thetis Andromeda/Andromeda.cs:914-948 [v2.10.3.13]
# (CATHandleAmplifierTripMessage + GanymedeResetPressed).
nereus_add_test(tst_radio_model_pa_tripped)

# ── Phase 3M-0 Task 7A: per-board PA scaling table ────────────────────────────
# 5 tests pin per-board forward-power watts for ANAN100, ANAN_G2, ORIONMKII,
# REDPITAYA, and the default fallback (HERMES). Values computed from the ported
# formula: volts=(adc-offset)/4095*refVoltage; watts=volts^2/bridgeVolt.
# Ports Thetis console.cs:25008-25072 [v2.10.3.13] (computeAlexFwdPower).
nereus_add_test(tst_pa_scaling)

# ── Phase 1 Agent 1B (issue #167 PA cal hotfix): public PaTelemetryScaling ───
# 8 tests pin scaleFwdPowerWatts byte-for-byte parity with the existing
# private RadioModel.cpp helper (ANAN8000D 3 raw values + HL2 default-branch
# 3 values + FIRST sentinel + zero-raw clamps), and pin scaleFwdRevVoltage
# per-board curve at 2 raw values per major SKU (HERMES / ORIONMKII /
# ANAN_G2 / HL2). Ports Thetis console.cs:25008-25072 [v2.10.3.13]
# (computeAlexFwdPower) — RadioModel.cpp call-site migration deferred to
# Phase 4 Agent 4A.
nereus_add_test(tst_pa_telemetry_scaling)
nereus_add_test(tst_pa_temp_unit)
nereus_add_test(tst_radio_model_hl2_temp_routing)

# ── Phase 3M-0 Task 8: SpectrumWidget "HIGH SWR" overlay state ───────────────
# Verifies setHighSwrOverlay(active, foldback) stores state, foldback flag
# toggles independently, and repeated calls with same args are idempotent.
# No pixel-diff — visual verification deferred to 3M-1a.
# Mirrors Thetis display.cs:4183-4201 [v2.10.3.13].
nereus_add_test(tst_spectrum_widget_highswr_overlay)

# ── Phase 3M-1a H.1: SpectrumWidget MOX overlay state ────────────────────────
# Verifies setMoxOverlay(bool) stores state, setTxAttenuatorOffsetDb stores
# value, setTxFilterVisible stores state.  Idempotency checked for all three.
# No pixel-diff — visual: red border appears on TX.
# Derives from Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
nereus_add_test(tst_spectrum_widget_mox_overlay)

# ── Phase 3M-1a H.2: MeterPoller TX bindings on MOX engage ───────────────────
# Verifies setInTx(bool) switches poll set, idempotency, null guards in
# pollTxMeters() and poll().  No live WDSP reads — headless state tests only.
# Derives from Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch.
nereus_add_test(tst_meter_poller_tx_bindings)

# ── Phase 3M-1a H.3: TxApplet wiring model-layer contracts ───────────────────
# Verifies TransmitModel::setPower/tunePowerForBand round-trips, powerChanged
# and tunePowerByBandChanged signals, MoxController default states,
# moxStateChanged end-of-walk firing, manualMoxChanged on setTune().
# Headless (QCoreApplication only) — TxApplet widget not constructed here
# since QWidget requires a display server.
# Derives from Thetis console.cs:4822 + 12094 + 29978-30157 [v2.10.3.13].
nereus_add_test(tst_tx_applet_tune_wiring)

# ── RF Power per-band persistence wiring (regression) ────────────────────────
# Drives a real TxApplet QWidget so QApplication is required.  Covers:
#   (1) slider valueChanged -> setPowerForBand(currentBand)
#   (2) setCurrentBand(band) -> slider value pulled from powerForBand(band)
#   (3) bootstrap call with default-band still recalls (Gap 3 regression)
#   (4) per-band isolation across slider moves
#   (5) AppSettings round-trip through TransmitModel reconstruction + reload
#   (6) powerByBandChanged signal fires on slider events
# Mirrors Thetis ptbPWR_Scroll @ console.cs:28642 + TXBand setter @
# console.cs:17513 [v2.10.3.13].
nereus_add_test(tst_tx_applet_rf_power_per_band)

# ── Phase 3M-1b (relocated): PhoneCwApplet Mic Gain slider ──────────────────
# Verifies the model-layer contracts that PhoneCwApplet's Mic Gain slider depends on
# (relocated from TxApplet J.1 per JJ feedback 2026-04-28):
#   default value (-6 dB), per-board range (BoardCapabilities::micGainMinDb/Max),
#   bidirectional sync (setMicGainDb / micGainDbChanged), mic-mute state
#   (micMuteChanged fires; default micMute==true), idempotency / no-feedback-loop,
#   and clamping to kMicGainDbMin/Max.
# NereusSDR-native UI (no Thetis cite stamps required for the applet layer).
nereus_add_test(tst_phone_applet_mic_gain)

# ── Phase 3M-1b (new): PhoneCwApplet Mic Level Gauge dB conversion ───────────
# Exercises the linear→dBFS conversion + clamping logic used by the 50 ms
# AudioEngine::pcMicInputLevel() polling timer that drives m_levelGauge.
# Tests: silence/sub-floor/exact-threshold → -40 dB floor; 0.1f → -20 dB;
# 0.5f → -6 dB; 1.0f → 0 dB; NaN/Inf → floor; above FS → +10 ceiling.
# NereusSDR-native UI (no Thetis cite stamps required for the applet layer).
nereus_add_test(tst_phone_applet_mic_level_gauge)

# ── Phase 3M-1b J.2 → 3M-3a-iii Task 16: VOX toggle (model-layer) ───────────
# Verifies the model-layer contracts that BOTH the historical TxApplet VOX
# button AND the current PhoneCwApplet VOX button depend on:
#   default voxEnabled=false (plan §0 row 8 safety — VOX always loads OFF),
#   UI→Model toggle ON/OFF (setVoxEnabled / voxEnabled getter),
#   Model→UI: voxEnabledChanged fires with correct bool,
#   visual state on toggle (signal fires on change),
#   no feedback loop / idempotency (same-value set → no extra emit).
# (The VoxSettingsPopup-specific cases were dropped in 3M-3a-iii Task 16
# alongside the popup widget itself — VOX settings now live in
# Setup → Transmit → DEXP/VOX, exercised by tst_dexp_vox_setup_page.)
# NereusSDR-native UI (no Thetis cite stamps required for the applet layer).
nereus_add_test(tst_tx_applet_vox_toggle)

# ── Phase 3M-3a-i Batch 2 Task F + 3M-3a-ii cleanup: TxApplet LEV/EQ/CFC ────
# Verifies the row of 3 checkable buttons inserted between MON volume and
# TUNE/MOX: LEV/EQ are bidirectional with TransmitModel.txLevelerOn /
# .txEqEnabled (objectNames: TxLevButton, TxEqButton); CFC bidirectional with
# TransmitModel.cfcEnabled (objectName: TxCfcButton, right-click → modeless
# TxCfcDialog).  PROC moved to PhoneCwApplet — see tst_phone_applet_proc.
# (Renamed from tst_tx_applet_lev_eq_proc.cpp during the post-bench cleanup.)
nereus_add_test(tst_tx_applet_lev_eq_cfc)

# ── Phase 3M-VAX-toggle: TxApplet mic-source badge 3-way ────────────────────
# The badge above the gauges previously read "PC mic" or "Radio mic".
# Phase 3M-VAX-toggle extends it to a 3-way switch so MicSource::Vax
# renders "VAX" both on micSourceChanged and on syncFromModel.
nereus_add_test(tst_tx_applet_mic_source_badge)

# ── Phase 3M-3a-ii post-bench cleanup: PhoneCwApplet PROC button + slider ──
# Verifies the PROC controls inherited un-wired from 3I-3 (NyiOverlay-marked):
# PhoneCwProcButton bidirectional with TransmitModel.cpdrOn; PhoneCwProcSlider
# bidirectional with TransmitModel.cpdrLevelDb (range 0..20 dB matching Thetis
# ptbCPDR console.Designer.cs:6042-6043 [v2.10.3.13]); numeric "X dB" value
# label mirrors slider; NyiOverlay markers absent.
nereus_add_test(tst_phone_applet_proc)

# ── Phase 3M-VAX-toggle: PhoneCwApplet VAX button wiring ────────────────────
# Verifies the [VAX] button on the Phone tab now drives MicSource via
# TransmitModel::toggleVaxSource: left-click toggles Vax, second click
# restores the previous non-VAX source, external model changes sync the
# checked state, and right-click emits openSetupRequested("Audio", "TX Input")
# so MainWindow opens Setup -> Audio -> TX Input.
nereus_add_test(tst_phone_applet_vax_toggle)

# ── Phase 3M-3a-i Batch 3 Task A.1: TxEqDialog scaffold ───────────────────
# Verifies the modeless 10-band TX EQ dialog launched from the [EQ]
# right-click and Tools → TX Equalizer menu.  Confirms construction,
# default values from TransmitModel, bidirectional binding (UI → model
# and model → UI), the m_updatingFromModel echo guard, and the
# instance() singleton pattern.
nereus_add_test(tst_tx_eq_dialog)

# ── Phase 3M-3a-ii Batch 6 Task A: TxCfcDialog (10-band CFC editor) ──────
# Verifies the modeless 10-band CFC editor launched from the [CFC]
# right-click on the TxApplet and from CfcSetupPage's [Configure CFC
# bands…] button.  Confirms construction, default values from
# TransmitModel, bidirectional binding (UI → model and model → UI) for
# all 32 controls (profile combo + 5 buttons + 2 globals + 30 band spins),
# echo guard, profile-bank Save/Reset path.  Layout mirrors Thetis
# frmCFCConfig.{cs,Designer.cs} [v2.10.3.13] with NereusSDR-spin 10-row
# grid replacing the selected-band + edit-pane.
nereus_add_test(tst_tx_cfc_dialog)

# ── Phase 3M-1b J.3: TxApplet MON toggle + monitor volume slider + mic-source badge ──
# Verifies the model-layer contracts that TxApplet's MON button, monitor volume
# slider, and mic-source badge depend on:
#   monEnabled default false (plan §0 row 9 safety — MON always loads OFF),
#   setMonEnabled / monEnabledChanged bidirectional round-trip,
#   monitorVolume default 0.5f (Thetis audio.cs:417), float↔int mapping
#   (float = int / 100.0f; int = qRound(float * 100.0f)), boundary 0.0/1.0,
#   micSource default Pc, micSourceChanged fires with Radio/Pc values (badge
#   text contracts "PC mic"/"Radio mic"), idempotency guards.
# Closes Phase J (all 3 J tasks landed).
# NereusSDR-native UI (no Thetis cite stamps required for the applet layer).
nereus_add_test(tst_tx_applet_mon)

# ── Phase 3M-1a H.4: PowerPage wiring — Max Power, per-band Tune Power, ATT-on-TX ──
# (class renamed from PowerPaPage in IA reshape Phase 6, 2026-05-02; test
#  filename retains 'power_pa_page' for git history continuity)
# Verifies Max Power slider ↔ TransmitModel::power(), per-band tune-power spinboxes
# ↔ TransmitModel::tunePowerForBand(), chkATTOnTX and chkForceATTwhenPSAoff widget
# existence + null-controller guards.
# Derives from Thetis console.cs:4822 + 12094 [v2.10.3.13] + setup.designer.cs:5926-5939
# + setup.designer.cs:5660-5671 [v2.10.3.13].
nereus_add_test(tst_transmit_setup_power_pa_page)

# ── Phase 3M-0 Task 9: SWR Protection group box on Setup → Transmit ──────────
# Verifies grpSWRProtectionControl has 5 controls with correct objectNames,
# defaults, and verbatim tooltip text per
# Thetis setup.designer.cs:5793-5924 [v2.10.3.13].
nereus_add_test(tst_transmit_setup_swr_protection)

# ── Issue #175 Task 8: PowerPage grpPATune (Tune group) HL2-aware port ──────
# Verifies the new "Tune" group box on Setup → Transmit → Power has 3 drive-
# source radios + comboTXTUNMeter + udTXTunePower; spinbox bounds polymorph
# by SKU (HL2: -16.5..0 dB / 0.5 step / 1 dp / " dB"; others: 0..100 W /
# 1 step / 0 dp / " W"); fixed-mode toggle persists to TransmitModel; the
# spinbox is disabled when a non-Fixed radio is selected.
# mi0bot-Thetis cites:
#   setup.designer.cs:47874-47930 [v2.10.3.13-beta2] (group layout)
#   setup.cs:20328-20331         [v2.10.3.13-beta2] (HL2 spinbox bounds)
nereus_add_test(tst_transmit_setup_power_page_hl2)

# ── Phase 3M-0 Task 10: External TX Inhibit group box on Setup → Transmit ────
# Verifies grpExtTXInhibit has 2 checkboxes with correct objectNames, defaults,
# verbatim tooltip text per setup.designer.cs:46626-46657 [v2.10.3.13], and
# that toggling chkTXInhibit persists to AppSettings.
nereus_add_test(tst_transmit_setup_tx_inhibit)

# ── Phase 3M-0 Task 11: Disable HF PA ────────────────────────────────────────
# Verifies chkHFTRRelay tooltip "Disables HF PA." verbatim from
# setup.designer.cs:5786 [v2.10.3.13].  Issue #175 Wave 1: chkBlockTxAnt2/3
# cases dropped (Power-page checkboxes wrote dead AppSettings keys; live
# editor lives at AntennaAlexAntennaControlTab via AlexController).
nereus_add_test(tst_transmit_setup_block_tx)

# ── Phase 3M-3a-i Batch 5 Task E: SpeechProcessorPage TX dashboard ───────────
# Verifies the NereusSDR-spin Speech Processor page builds without crashing,
# the TX EQ + Leveler status labels read from + react to TransmitModel signals,
# the active-profile label tracks MicProfileManager::activeProfileChanged, the
# ALC row is statically labelled "always-on", and the cross-link buttons emit
# openSetupRequested(category, page) with the correct setup-page leaf labels.
# Phase 3M-3a-ii Batch 5: extended with Phrot / CFC / CESSB live-binding cases.
nereus_add_test(tst_speech_processor_page)

# ── Phase 3M-3a-ii Batch 5 Task E: CfcSetupPage full implementation ──────────
# Verifies the three group boxes (Phase Rotator / CFC / CESSB) construct,
# initial values reflect TransmitModel defaults, bidirectional binding works
# both directions, spinbox range clamping holds, and the [Configure CFC bands…]
# button emits openCfcDialogRequested (Batch 6 wires the dialog).  Layout
# mirrors Thetis tpDSPCFC tab 1:1 (setup.Designer.cs:46162-46280 [v2.10.3.13]
# for grpPhRot).
nereus_add_test(tst_cfc_setup_page)

# ── Phase 3M-3a-iii Task 14: DexpVoxPage full implementation ─────────────────
# Verifies the three group boxes (VOX / DEXP, Audio LookAhead, Side-Channel
# Trigger Filter) construct, all 14 controls exist with verbatim Thetis object
# names, initial values reflect TransmitModel defaults, bidirectional binding
# works in both directions, and group titles match Thetis verbatim.  Layout
# mirrors Thetis tpDSPVOXDE tab 1:1 (setup.designer.cs:44763-45260 [v2.10.3.13]).
nereus_add_test(tst_dexp_vox_setup_page)

# ── Phase 3M-0 Task 12: Hardware Configuration group box on Setup → General ──
# Verifies grpHardwareConfig has 4 controls + 1 warning label per
# Thetis tpGeneralHardware (setup.designer.cs:8045-8396 [v2.10.3.13]):
# 24-entry Region combo (default United States), Extended checkbox, red warning
# label, chkGeneralRXOnly hidden by default, chkNetworkWDT default ON.
nereus_add_test(tst_general_setup_hardware_config)

# ── Phase 3M-1a G.2: Receive Only checkbox visibility wired from caps.isRxOnlySku ──
# Verifies setReceiveOnlyVisible(bool) works correctly and that the constructor
# reads initial caps (hidden for standard boards, visible for RX-only SKUs like
# HermesLiteRxOnly).  Cite: Thetis setup.designer.cs:8535-8544 [v2.10.3.13]
# (Visible=false default); BoardCapabilities::isRxOnlySku (NereusSDR-original).
# NEREUS_BUILD_TESTS required for RadioModel::setBoardForTest.
nereus_add_test(tst_general_options_page_rx_only)
target_compile_definitions(tst_general_options_page_rx_only PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-0 Task 13: Options group box on Setup → General ─────────────────
# Verifies grpGeneralOptions has chkPreventTXonDifferentBandToRX per
# Thetis setup.designer.cs:9050-9059 [v2.10.3.13]. Tooltip NereusSDR-original
# (Thetis has no tooltip on this control). Default unchecked.
nereus_add_test(tst_general_setup_options)

# ── Setup IA reshape Phase 2: top-level PA category placeholder pages ────────
# Verifies PaGainByBandPage / PaWattMeterPage / PaValuesPage construct cleanly
# with the expected page titles and a placeholder QLabel hinting at the future
# phase tag.  Mirrors Thetis tpPowerAmplifier (setup.designer.cs:47366-47371
# [v2.10.3.13]); panel-level wiring lands in Phase 3+.
nereus_add_test(tst_pa_setup_pages)

# ── PA calibration safety hotfix Phase 8: per-SKU PA visibility matrix ───────
# Verifies PaGainByBandPage::applyCapabilityVisibility honors the five
# BoardCapabilities flags the visibility wiring consumes (hasPaProfile,
# isRxOnlySku, canDriveGanymede, hasPureSignal, hasStepAttenuatorCal) per
# the per-SKU matrix derived from Thetis comboRadioModel_SelectedIndexChanged
# (setup.cs:19812-20310 [v2.10.3.13+501e3f51]). Test matrix covers all 16
# HPSDRModel values + RX-only + Ganymede + PureSignal scenarios (19 cases).
nereus_add_test(tst_pa_setup_per_sku_visibility)
target_compile_definitions(tst_pa_setup_per_sku_visibility PRIVATE NEREUS_BUILD_TESTS)

# ── PA calibration safety hotfix Phase 9: SetupDialog cross-page Reset wiring ─
# Verifies the connect() in SetupDialog::buildTree() that routes
# PaWattMeterPage::resetPaValuesRequested (Phase 5A) into
# PaValuesPage::resetPaValues() (Phase 5B). Drives the WattMeter page's
# Reset button through its Phase 5A test seam, then asserts the Values
# page's running peak/min trackers collapse to current — only possible
# if SetupDialog actually wired the cross-page connect.
# Source: Thetis btnResetPAValues_Click (setup.cs:16346-16357 [v2.10.3.13]).
nereus_add_test(tst_setup_dialog_pa_reset_wiring)
target_compile_definitions(tst_setup_dialog_pa_reset_wiring PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-0 Task 14: MainWindow status-bar TX Inhibit + PA Status badge ───
# Tests the label-state logic for the two safety indicator widgets added to
# the MainWindow status bar:
#   m_txInhibitLabel — hidden by default, shown when inhibit asserts (Task 17)
#   m_paStatusBadge  — "PA OK" (green) / "PA FAULT" (red) per paTripped()
# MainWindow itself is too heavyweight to construct in a unit test (requires
# full RadioModel + WDSP + audio stack). Tests exercise the helper logic via
# standalone QLabel instances; Task 17 integration provides the live wiring.
nereus_add_test(tst_mainwindow_status_bar_safety)

# ── Phase 3M-0 Task 15: AppSettings round-trip audit for all 15 keys ─────────
# Verifies all 15 AppSettings keys introduced by Tasks 9-13 survive a
# save()+load() cycle: SwrProtection* (5), TxInhibitMonitor* (2),
# AlexAnt[23]RxOnly + DisableHfPa (3), Region/Extended/RxOnly/NetworkWDT (4),
# PreventTxOnDifferentBandToRx (1). Uses QTemporaryDir for full isolation.
nereus_add_test(tst_phase3m0_persistence_audit)

# ── Phase 3Q Task 1: ConnectionState enum + RadioModel state exposure ─────────
# 3 tests: initial state Disconnected, signal on transition, no signal for
# redundant transition. NereusSDR-original (no upstream port).
nereus_add_test(tst_connection_state)

# ── Phase 3Q Task 2: RadioDiscovery::probeAddress() unicast probe ─────────────
# 2 tests: success path fills RadioInfo from P1 reply (FakeP1Probe loopback),
# timeout emits probeFailed(addr, port). Backbone for manual-IP + VPN paths.
nereus_add_test(tst_radio_discovery_probe)
target_link_libraries(tst_radio_discovery_probe PRIVATE Qt6::Network)

# ── Phase 3Q Task 4: AddCustomRadioDialog — SKU model picker + action buttons ──
# 4 tests: Auto-detect at index 0, all 16 SKUs present in modelCombo,
# saveOfflineButton + probeButton objectNames accessible via findChild.
nereus_add_test(tst_add_custom_radio_dialog)

# ── Phase 3Q Task 5: ConnectionPanel state pills — threshold logic ─────────────
# 4 tests: online < 60s, stale 60s-5min, offline > 5min, offline when never seen.
# Exercises statePillForLastSeen() static method — pure function, no radio required.
# NereusSDR-original; no Thetis attribution needed.
nereus_add_test(tst_connection_panel_state_pills)

# ── Phase 3Q Task 12: AppSettings::migrateRadioKey — synthetic → real-MAC ────
# 6 tests: core migration (name+autoConnect), pinToMac preserved, same-key no-op,
# absent-key no-op, IP+port preserved, no orphan keys after migration.
# Uses QTemporaryDir for full isolation; QTEST_APPLESS_MAIN (no QApplication).
# NereusSDR-original; no Thetis attribution needed.
nereus_add_test(tst_app_settings_migration)

# ── Issue #241: settings file crash-safety + corruption recovery ─────────────
# 8 tests covering atomic save (QSaveFile), .bak rotation, leading-NUL
# corruption recovery, mid-stream XML parse-error recovery, no-.bak fallback
# to defaults, missing-file = first-run (not corruption), diagnostics-reset
# between load() calls, and no QSaveFile temp residue.
nereus_add_test(tst_app_settings_corruption)

# ── Task 5.1: SettingsSchemaVersion + v0.3.0 migration ────────────────────────
# 5 tests: retired keys removed on v<3 → v3 migration, idempotency, fresh-install
# (no version key), no-op when already at v3, version-zero path.
# Uses QTemporaryDir for full isolation; QTEST_APPLESS_MAIN (no QApplication).
# NereusSDR-original; no Thetis attribution needed.
nereus_add_test(tst_settings_migration_v0_3_0)

# ── Phase 3M-1a Task B.1: PttMode enum (Thetis PTTMode port) ─────────────────
# Verifies all 9 values (None/Manual/Mic/Cw/X2/Cat/Vox/Space/Tci) have
# distinct non-empty labels, integer assignments match Thetis
# enums.cs:346-359 [v2.10.3.13] (First=-1..Last=9), and pttModeFromString
# round-trips both Thetis-source names and user-visible labels case-insensitively.
nereus_add_test(tst_ptt_mode)

# ── Phase 3M-1a Task B.2: MoxController skeleton ──────────────────────────────
# Verifies: default state (isMox=false, state=Rx, pttMode=None);
# setMox(true) emits moxStateChanged(true) exactly once, state→Tx;
# setMox(false) emits moxStateChanged(false) exactly once, state→Rx;
# idempotent guard: repeated setMox(true) emits only on first call;
# Codex P2 ordering lock: runMoxSafetyEffects fires on EVERY setMox call
#   (including idempotent ones) and fires BEFORE m_mox is committed;
# setPttMode idempotent guard + pttModeChanged signal;
# all 7 MoxState enumerators exist and are distinct;
# kXxxMs constants have correct Thetis-cited defaults.
# Updated in B.3: setTimerIntervals(0,...) makes walk synchronous for
# signal-cadence tests; Codex P2 tests unchanged.
nereus_add_test(tst_mox_controller_basic)

# ── Phase 3M-1a Task B.3: MoxController 6 QTimer chains ──────────────────────
# Verifies: default kXxxMs constants match Thetis [v2.10.3.13];
# RX→TX walk: Rx → RxToTxRfDelay (rfDelay) → Tx;
# TX→RX walk: Tx → TxToRxInFlight (keyUpDelay) → TxToRxFlush (pttOutDelay) → Rx;
# moxStateChanged(true) fires at end of RX→TX walk (not at setMox entry);
# moxStateChanged(false) fires at end of TX→RX walk;
# stateChanged signals fire in expected order;
# 0ms timer intervals drive full walk via processEvents() alone;
# breakInDelayTimer never started in 3M-1a;
# spaceDelayTimer never started when kSpaceDelayMs == 0;
# rapid toggle (setMox false during RX→TX walk) cancels in-flight timers.
nereus_add_test(tst_mox_controller_timers)

# ── Phase 3M-1a Task B.4: MoxController 6 phase signals (Codex P1) ───────────
# Verifies: each phase signal fires exactly once per real RX→TX/TX→RX walk;
# Order (RX→TX): txAboutToBegin → hardwareFlipped(true) → txReady → moxStateChanged(true);
# Order (TX→RX): txAboutToEnd → hardwareFlipped(false) → txaFlushed → rxReady → moxStateChanged(false);
# hardwareFlipped fires synchronously in setMox() (before rfDelay / keyUpDelay timers);
# Idempotent guard: repeated setMox(true/false) does NOT re-fire phase signals;
# Rapid toggle: cancelled rfDelay walk emits txAboutToBegin + hardwareFlipped(true)
#   but NOT txReady (slot never ran — cancelled timer);
# Codex P1: txAboutToBegin fires once per transition even when stateChanged fires 2×;
# Phase signals derived from chkMOX_CheckedChanged2 (console.cs:29311-29678 [v2.10.3.13]).
nereus_add_test(tst_mox_controller_phase_signals)

# ── Phase 3M-1a Task B.5: MoxController setTune(bool) slot ───────────────────
# Verifies: setTune(true) engages MOX, sets isManualMox=true, pttMode=Manual,
#   emits manualMoxChanged(true) and pttModeChanged(Manual) each exactly once;
# setTune(false) releases MOX, clears isManualMox (but NOT pttMode — F.1 resets it),
#   emits manualMoxChanged(false) exactly once;
# NereusSDR ordering: flags set BEFORE setMox(true) so phase-signal subscribers
#   see m_manualMox=true at txAboutToBegin time; flag cleared AFTER setMox(false)
#   so m_manualMox stays true during TX→RX phase signals (txAboutToEnd etc.);
# Idempotent guard: setTune(true) twice → manualMoxChanged fires only once;
# TUN→RAW-MOX combinations: direct setMox(true) then setTune(true) flips manualMox;
#   setTune(true) + setMox(false) leaves manualMox true; setTune(false) then clears;
# Codex P2 invariant: runMoxSafetyEffects fires on every setTune call (via setMox);
# Ports chkTUN_CheckedChanged (console.cs:29978-30157 [v2.10.3.13]) flag block.
nereus_add_test(tst_mox_controller_tune)

# ── Phase 3M-1b Task H.1: MoxController::setVoxEnabled voice-family mode-gate ─
# Verifies: setVoxEnabled(bool) + onModeChanged(DSPMode) + voxRunRequested(bool);
# Default state: no spurious emit at startup (m_voxEnabled=false, gated=false);
# LSB + VOX on → voxRunRequested(true) fires once;
# LSB + VOX off (after on) → voxRunRequested(false) fires once;
# CW mode + VOX on → voxRunRequested(true) does NOT fire (gate=false);
# LSB + VOX on, then mode change to CWL → voxRunRequested(false) fires once;
# CWL + VOX on (no emit), then mode change to LSB → voxRunRequested(true) fires once;
# Within-voice mode change (LSB→USB) while VOX on → no double emit;
# Repeat setVoxEnabled(true) in voice mode → no double emit (idempotent);
# All 8 voice-family modes (parametrized): + voxEnabled(true) → emits true;
# All 4 excluded modes (CWL/CWU/SPEC/DRM) + voxEnabled(true) → no true emit.
# Source: Thetis cmaster.cs:1039-1052 [v2.10.3.13] — CMSetTXAVoxRun.
nereus_add_test(tst_mox_controller_vox_enabled)

# ── Phase 3M-1b Task H.2: MoxController::setVoxThreshold mic-boost-aware scaling ─
# Verifies: setVoxThreshold(int dB) + onMicBoostChanged(bool) + setVoxGainScalar(float)
#           + voxThresholdRequested(double);
# NAN sentinel: first call always emits (primes WDSP regardless of default value);
# Boost OFF: threshold = pow(10, dB/20) only (no gain scaling);
# Boost ON, scalar 1.0: same result as boost-off (identity multiplication);
# Boost ON, scalar 0.5: threshold halved vs. boost-off;
# Gain scalar change (0.5→1.0) re-emits with new computed value;
# Mic-boost toggle (on→off→on) re-emits on each transition;
# Idempotent on emitted double: repeat setVoxThreshold(-40) → no second emit;
# Thetis cross-check Combo A: dB=-60, boost=off → 0.001 (exact);
# Thetis cross-check Combo B: dB=-20, boost=on, gain=0.5 → 0.05 (exact);
# Thetis cross-check Combo C: dB=0,   boost=on, gain=2.0 → 2.0 (exact);
# Boundary -80 dB: result > 0, not NaN/Inf, ≈ 1e-4;
# Boundary 0 dB: result ≈ 1.0, not NaN/Inf;
# Boost round-trip: off→on re-emits with the original scaled value.
# Source: Thetis cmaster.cs:1054-1059 + setup.cs:18908-18912 [v2.10.3.13].
nereus_add_test(tst_mox_controller_vox_threshold)

# ── Phase 3M-1b Task H.3: MoxController VOX hang-time + anti-VOX gain ──────
# Verifies:
#   setVoxHangTime: NaN sentinel first-call emit; ms→seconds conversion (/ 1000.0);
#     idempotent on emitted double; boundary 1 ms → 0.001 s, 10000 ms → 10.0 s.
#   setAntiVoxGain: NaN sentinel; dB→linear /20.0 voltage scaling (0 dB → 1.0,
#     -20 dB → 0.1, -60 dB → 0.001); idempotent on emitted double; boundary check.
# 3M-3a-iv post-bench refactor (Option A) dropped the setAntiVoxSourceVax
# coverage alongside the slot+signal in MoxController.
# Source: Thetis setup.cs:18896-18900 + 18986-18990 [v2.10.3.13].
nereus_add_test(tst_mox_controller_anti_vox)

# ── Phase 3M-1b Task H.4: MoxController PTT-source dispatch ──────────────────
# Verifies:
#   Accepted slots (5): onMicPttFromRadio, onCatPtt, onVoxActive, onSpacePtt,
#     onX2Ptt — each press emits pttModeChanged(Xxx) then moxStateChanged(true);
#     each release emits moxStateChanged(false); PttMode NOT cleared on release
#     (F.1 contract: hardwareFlipped(false) subscriber in RadioModel clears it).
#   Rejected slots (2): onCwPtt (deferred to 3M-2), onTciPtt (deferred to 3J) —
#     both press and release emit no signals and leave m_pttMode unchanged.
#   Cross-source switching (§G): last-setter-wins semantic.
#   Idempotency (§H): repeated press does not double-emit.
#   PttMode integer values (§I): spot-check vs enums.cs:346-359 [v2.10.3.13].
# Source: Thetis console.cs:25463-25507 [v2.10.3.13] (PollPTT dispatch);
#   console.cs:26680 [v2.10.3.13] (SPACE); enums.cs:346-359 [v2.10.3.13].
#   Pre-code review §0.3 + master design §5.2.1.
nereus_add_test(tst_mox_controller_ptt_source_dispatch)

# ── Phase 3M-1c Tasks C.2 / C.3 / C.4: MoxController multicast Pre/Post signals ─
# Verifies the new 3-arg Pre/Post MOX state-change signals that future
# observers (PS form, TCI server, MeterPoller, audio recorder, etc.) plug into:
#   moxChanging(int rx, bool oldMox, bool newMox) — Pre, before m_mox commit;
#     subscribers can defensively freeze readings.
#   moxChanged (int rx, bool oldMox, bool newMox) — Post, after the timer
#     walk completes (parallels the existing 1-arg moxStateChanged boundary).
#
# Coverage:
#   §1.1  Pre fires before m_mox commit (isMox() inside slot returns OLD value);
#   §1.2  Post fires after timer walk with correct oldMox/newMox payload;
#   §1.3  Pre→Post ordering across a full RX→TX cycle;
#   §2.1  rx == 1 when rx2_enabled=false, vfobTx=false (default);
#   §2.2  rx == 1 when rx2_enabled=true,  vfobTx=false (RX2 alone is not a TX route);
#   §2.3  rx == 1 when rx2_enabled=false, vfobTx=true;
#   §2.4  rx == 2 when rx2_enabled=true,  vfobTx=true (TX off VFO-B via RX2);
#   §3.1  Idempotent setMox(false) on fresh controller emits neither Pre nor Post;
#   §4.1  TX→RX direction Post carries oldMox=true, newMox=false.
#
# The existing 1-arg moxStateChanged(bool) signal is unchanged — the 3-arg
# moxChanged is an additive overload (Pre/Post overlay), not a rename.
# Source: Thetis console.cs:29324 [v2.10.3.13] (MoxPreChangeHandlers, // MW0LGE_21k8);
#   console.cs:29677 [v2.10.3.13] (MoxChangeHandlers, // MW0LGE_21a).
nereus_add_test(tst_mox_controller_multicast_signals)

# ── Phase 3M-1b Task H.5: mic_ptt extraction from P1/P2 status frames ─────────
# Verifies end-to-end mic_ptt extraction and dispatch through MoxController.
#
# Prong A — RadioConnection-level extraction:
#   §A.1  P1: C0 bit 0 set in sub-frame 0 → micPttFromRadio(true)
#   §A.2  P1: C0 bit 0 set in sub-frame 1 only → micPttFromRadio(true) (OR logic)
#   §A.3  P1: C0 bit 0 clear in both sub-frames → micPttFromRadio(false)
#   §A.4  P1: ADC overload in C1 does NOT set micPtt (PTT is in C0, not C1)
#   §B.1  P2: raw[4] bit 0 set → micPttFromRadio(true)
#   §B.2  P2: raw[4] bit 0 clear → micPttFromRadio(false)
#   §B.3  P2: dot/dash bits (bits 1/2) only → micPttFromRadio(false)
#   §B.4  P2: ADC overload at raw[5] does NOT set micPtt (separate byte)
#
# Prong B — End-to-end (status frame → MoxController):
#   §C.1  P1 press → MoxController engaged, PttMode::Mic
#   §C.2  P1 release → MoxController released, PttMode retained
#   §C.3  P1 repeated press → idempotent (no double-emit)
#   §D.1  P2 press → MoxController engaged, PttMode::Mic
#   §D.2  P2 release → MoxController released, PttMode retained
#   §D.3  P2 repeated press → idempotent (no double-emit)
#   §E.1  P1: consecutive false frames both emit (unconditional emission)
#   §E.2  P2: consecutive false packets both emit (unconditional emission)
#
# Source: Thetis console.cs:25426 [v2.10.3.13] (PollPTT mic_ptt bit);
#   networkproto1.c:329 [v2.10.3.13] (P1 ptt_in bit);
#   network.c:686-689 [v2.10.3.13] (P2 ptt_in byte/bit);
#   deskhpsdr/src/new_protocol.c:2525 [@120188f] (P2 cross-ref).
nereus_add_test(tst_mox_controller_mic_ptt_extraction)
target_compile_definitions(tst_mox_controller_mic_ptt_extraction PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task C.1: WdspEngine TX channel API ──────────────────────────
# Verifies:
#   TX channel constants match Thetis cmaster.c:177-190 [v2.10.3.13]:
#     kTxChannelType=1, kTxBlockOnOutput=1, kTxTSlewUpSecs/DownSecs=0.010,
#     kTxDspSampleRate=96000, kTxDspBufferSize=4096;
#   createTxChannel returns nullptr before WDSP initialized (Approach A);
#   createTxChannel returns nullptr after WDSP open (C++ wrapper null until C.2);
#   destroyTxChannel is idempotent (double-call, never-created channel, post-destroy);
#   txChannel(id) lookup returns nullptr when not created and after destroy;
#   RX channel 0 and TX channel 1 co-exist without aliasing.
#   Channel ID: WDSP.id(1, 0) = CMsubrcvr * CMrcvr = 1 with single-RX layout.
nereus_add_test(tst_wdsp_engine_tx_channel)

# ── Phase 3M-3a-iii Task 20: WdspEngine DEXP DSP-instance lifecycle ──────────
# The bench-failing fix: before Task 20, pdexp[1] was permanently nullptr
# after createTxChannel because create_dexp had never been ported into
# NereusSDR's TX-init path.  Every SetDEXP* setter and the SendCBPushDexpVox
# callback registration silently no-op'd via their pdexp[id] null guards;
# VOX-keying never engaged MOX because xdexp() never ran.
# Verifies:
#   createTxChannel populates pdexp[1] (non-null) - the assertion that
#     proves the fix vs. the bench-failing baseline;
#   destroy + recreate cycle is consistent (create_dexp runs again on
#     the second create);
#   TxChannel::pumpDexp is null-safe before WdspEngine wires the buffer
#     pointer (test builds that construct TxChannel directly without
#     going through WdspEngine::createTxChannel must not segfault).
# Source map (all [v2.10.3.13]):
#   wdsp/cmaster.c:130-157 - Thetis create_dexp call site (default args);
#   wdsp/cmaster.c:267     - Thetis destroy_dexp call site (BEFORE CloseChannel);
#   wdsp/dexp.c:187-227    - create_dexp impl: allocates DEXP, stores pdexp[id];
#   wdsp/dexp.c:230-239    - destroy_dexp impl: frees, but does NOT null pdexp[id].
nereus_add_test(tst_wdsp_engine_dexp_init)

# ── Phase 3R Task J2: WdspEngine RadeChannel lifecycle ──────────────────────
# Pins the createRadeChannel / destroyRadeChannel / radeChannel(id) contract:
#   - first create returns a non-null RadeChannel*;
#   - duplicate create with the same id returns the existing channel
#     (mirrors the createRxChannel pre-existence guard);
#   - destroy makes the lookup return nullptr;
#   - destroy on a never-created id is a safe no-op;
#   - the RadeChannel is parented to the WdspEngine so QObject ownership
#     cleans up correctly on engine destruction.
# RADE is a NereusSDR-native channel type with no Thetis equivalent;
# this test pins the C++-side lifecycle only and does not call into WDSP.
nereus_add_test(tst_wdsp_engine_rade_lifecycle)

# ── Phase 3R Task J3: SliceModel mode-aware channel swap ────────────────────
# Pins the setDspMode RADE <-> WDSP swap contract:
#   - switching to RADE destroys the slice's RxChannel and creates a
#     RadeChannel for the same slice ID;
#   - switching from RADE back to a WDSP mode tears down the RadeChannel
#     and recreates the RxChannel;
#   - the new RadeChannel is started with the "dummy" librade sentinel
#     when no AppSettings "Rade/ModelPath" is configured (librade has
#     weights compiled in per Phase A2b);
#   - setDspMode(currentMode) is a no-op (no signal, no channel churn).
# Uses the WdspEngine friend-access trick (NEREUS_BUILD_TESTS) to set
# m_initialized = true so the createRxChannel seed at the head of the
# test does not bail out on its !m_initialized guard.
nereus_add_test(tst_slice_model_rade_swap)

# ── Phase 3R Task J4: RADE audio routing into AudioEngine ───────────────────
# Pins the contract that RadioModel::wireRadeChannel adds an audio
# connection alongside the I5 signal graph: emitting
# RadeChannel::rxSpeechReady with a float32 stereo PCM block must
# reach AudioEngine's speakers bus through rxBlockReady.  FakeAudioBus
# stand-in counts pushes to detect the route.
nereus_add_test(tst_audio_engine_rade)

# ── Phase 3M-1a Task C.2: TxChannel 31-stage TXA pipeline skeleton ────────────
# Verifies:
#   Stage::kStageCount == 31 (authoritative from wdsp/TXA.c:31-479 [v2.10.3.13]);
#   createTxChannel returns a non-null TxChannel* (C++ wrapper now constructed);
#   txChannel(1)->channelId() == 1;
#   Default Run states match create_txa() first-argument for each of 31 stages:
#     ON : Panel, MicMeter, EqMeter, LvlrMeter, CfcMeter, Bp0, CompMeter, Alc,
#          AlcMeter, Sip1, Calcc, OutMeter;
#     OFF: RsmpIn, Gen0, PhRot, AmSq, Eqp, PreEmph, Leveler, CfComp, Compressor,
#          Bp1, OsCtrl, Bp2, AmMod, FmMod, Gen1, UsLew (no run param), Iqc, Cfir,
#          RsmpOut;
#   destroyTxChannel → txChannel(1) returns nullptr;
#   RX channel 0 + TX channel 1 co-exist without aliasing.
#   Stage count correction: pre-code review §8.1 said 25; actual source has 31
#   (PreEmph, Leveler, LvlrMeter, CfComp, CfcMeter between EqMeter and Bp0).
#   Tests run against stub path (no HAVE_WDSP in unit-test builds); stub returns
#   compile-time defaults matching create_txa() run arguments.
nereus_add_test(tst_tx_channel_pipeline)

# ── Phase 3M-1a Task C.3: TxChannel::setTuneTone() PostGen (gen1) wiring ──────
# Verifies:
#   kMaxToneMag == 0.99999 (from Thetis console.cs:29954 [v2.10.3.13]:
#     MAX_TONE_MAG = 0.99999f; // why not 1?  clipping? — comment preserved verbatim);
#   kMaxToneMag < 1.0 (deliberate: avoids clipping);
#   kMaxToneMag > 0.0 (positive magnitude);
#   setTuneTone(true)  with default args doesn't crash (stub path, no WDSP);
#   setTuneTone(false) with default args doesn't crash;
#   setTuneTone(true, +600.0) — positive freq, USB/CWU/DIGU convention;
#   setTuneTone(true, -600.0) — negative freq, LSB/CWL/DIGL convention;
#   setTuneTone(true, +600.0, 0.5) — custom magnitude override;
#   on→off and off→on toggle sequences don't crash;
#   repeated calls (10 cycles) don't crash;
#   zero freq is accepted;
#   Sign contract: no abs() / clamp / negate inside setTuneTone (sign is caller-authoritative).
#   Ports Thetis console.cs:29954, 30031-30040 [v2.10.3.13] and gen.c:783-813 [v2.10.3.13].
nereus_add_test(tst_tx_channel_tune_tone)

# ── Phase 3M-1a Task C.4: TxChannel::setRunning() + 3M-1a active stages ────
# Verifies:
#   isRunning() initial state is false (channel starts stopped);
#   setRunning(true) doesn't crash (stub path, no WDSP);
#   setRunning(false) doesn't crash;
#   isRunning() returns true after setRunning(true);
#   isRunning() returns false after setRunning(false);
#   toggle sequences (on→off, off→on, on→off→on) don't crash;
#   idempotent calls (setRunning(true) twice, setRunning(false) twice) are safe;
#   isRunning() remains correct after idempotent calls;
#   repeated toggle (10 cycles) doesn't crash;
#   setStageRunning(Stage::Gen0/Gen1/Panel/PhRot/AmSq/Eqp/Compressor/OsCtrl/Cfir/CfComp, true/false)
#     doesn't crash for all supported stages;
#   setStageRunning(RsmpIn/RsmpOut/UsLew, true) is a warning + no-op (not crash);
#   HAVE_WDSP-gated: setRunning(true) channel state ON, cfir ON;
#   HAVE_WDSP-gated: setRunning(false) channel state OFF, cfir OFF.
#   (HAVE_WDSP tests use QSKIP — integration harness pending; no segfault.)
#   Ports Thetis console.cs:29595, 29607 and cmaster.cs:522-527 [v2.10.3.13].
#   Ports Thetis wdsp/channel.c:259-294 and wdsp/cfir.c:233-238 [v2.10.3.13].
nereus_add_test(tst_tx_channel_running)

# ── Phase 3M-1a Task G.1 (production loop bench fix): TxChannel TX I/Q pump ──
# Verifies the fexchange2 → sendTxIq production loop wired in 3M-1a Task G.1.
# Test cases:
#   - setConnection(nullptr) / setMicRouter(nullptr): null-safety (no crash)
#   - setConnection(non-null) then null: detach — no crash
#   - driveOneTxBlock with no connection: null-guard path — no crash
#   - driveOneTxBlock with no mic router: silence-fill path — no crash
#   - setRunning(true) + connection attached: no crash
#   - Repeated toggle cycles (5×) with connection: no crash
#   - Timer fires → sendTxIq reaches MockConnection (≥1 call in 30 ms)
#   - lastN == kTxDspBufferSize (4096) — correct sample count per call
#   - Multiple timer ticks → callCount ≥ 2
#   - After setRunning(false) timer stops — no further calls (±1 tolerance)
#   - Detach connection while running: null-guard in driveOneTxBlock — no crash
#   - CountingMicSource::pullSamples() is called by driveOneTxBlock (≥1)
#   - Null mic router still calls sendTxIq (silence path, not skipped)
#   - setRunning(false) with no prior on: no crash
#   - Teardown order (setConnection(nullptr) before connection destructs): safe
nereus_add_test(tst_tx_channel_production_loop)

# ── Phase 3M-1a Task D.1: TxMicRouter interface + NullMicSource stub ─────────
# Pure unit test: header-only interface + NullMicSource concrete stub.
# Covers typical buffer sizes (64/128/256/1024), defensive edge cases
# (nullptr dst, zero size, negative size, single sample), full-buffer
# zero fill verification, repeated-call consistency, and virtual dispatch
# via TxMicRouter base pointer.
nereus_add_test(tst_tx_mic_router)

# ── Phase 3M-1a Task G.1: RadioModel ownership + wiring integration ──────────
# Verifies:
#   - moxController() is non-null after construction (owns MoxController on main thread)
#   - moxController() parent is RadioModel (Qt parent → auto-cleanup)
#   - txChannel() is null before WDSP initializes (non-owning view guarded)
#   - hardwareFlipped → onMoxHardwareFlipped is connected (F.1 wiring confirmed)
#   - txReady and txaFlushed lambdas guard null m_txChannel (no crash)
#   - moxController() pointer is stable across repeated calls
#   - RadioModel destruction with MoxController in-flight does not crash
#   - F.2 connect signal/slot shape is valid (QSignalSpy confirm)
# Master design §5.1.1; pre-code review §1.6 + §2.5.
nereus_add_test(tst_radio_model_g1_wiring)
target_compile_definitions(tst_radio_model_g1_wiring PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-i Batch 2 Task 1: TransmitModel→TxChannel routing ────────────
# Verifies the 13 connect() pairings for TX EQ + Leveler + ALC signals.
# m_txChannel stays null in unit-test builds (no WDSP); the test confirms
# the model-side fan-out (signal-on-setter, idempotent guard, signal-payload
# correctness) that the RadioModel-installed lambdas consume.
nereus_add_test(tst_radio_model_eq_lev_alc_wiring)

# ── Phase 3M-3a-iii Task 12: TransmitModel→TxChannel DEXP routing ────────────
# Verifies the 11 connect() pairings for the new DEXP signals
# (envelope/gate ratios/look-ahead/SCF) added in Tasks 7-10. Mirrors the
# eq_lev_alc_wiring pattern: m_txChannel stays null in unit-test builds (no
# WDSP); the test confirms the model-side fan-out (signal-on-setter,
# idempotent guard, signal-payload correctness) that the RadioModel-installed
# lambdas consume, plus the on-connect push surface (pushTxProcessingChain
# reads each TM getter and pushes the loaded default to TxChannel; we verify
# the getter surface returns the Thetis Designer defaults).
nereus_add_test(tst_radio_model_dexp_routing)

# ── Phase 3M-1a Task F.1: RadioModel::onMoxHardwareFlipped slot ──────────────
# Verifies the three-step fan-out in Thetis HdwMOXChanged order (§2.3):
#   1. Alex antenna routing (applyAlexAntennaForBand → setAntennaRouting)
#   2. MOX wire bit (m_connection->setMox)
#   3. T/R relay (m_connection->setTrxRelay)
# Test cases: isTx=true order, isTx=false order, each arg forwarded correctly,
# band read from active slice, no-connection graceful no-op, hasAlex=false
# all-steps-fire, slot connectable via QObject::connect.
nereus_add_test(tst_radio_model_mox_hardware_flip)
target_compile_definitions(tst_radio_model_mox_hardware_flip PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1a Task F.2: StepAttenuatorController TX-path activation ─────────
# Verifies onMoxHardwareFlipped(bool isTx) slot and its four helpers:
#   applyTxAttenuationForBand, shouldForce31Db, saveRxPreampMode,
#   restoreRxPreampMode.
# Ported from Thetis console.cs:29546-29576 [v2.10.3.13] pre-code review §6.2-§6.4.
# Test cases (12):
#   1. ATT-on-TX disabled → pushes 0 dB (no attenuation on TX)
#   2. ATT-on-TX enabled, PS on, USB → applies stored TX ATT
#   3. PS off + forceFlag → force 31 dB
#   4. CWL mode → force 31 dB
#   5. CWU mode → force 31 dB
#   6. USB mode + PS on → applies stored TX ATT (no force)
#   7. isTx=false → clears TX ATT to 0 dB (TX→RX restore)
#   8. HPSDR board, isTx=true → saves preamp, forces Minus20 (HPSDR_OFF)
#   9. HPSDR board, isTx=false → restores saved preamp
#  10. shouldForce31Db predicate table-driven (12 cases)
#  11. TX ATT per-band storage round-trip (14 bands)
#  12. No RadioConnection — graceful no-op (no crash)
nereus_add_test(tst_step_att_tx_path)
target_compile_definitions(tst_step_att_tx_path PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 1 Agent 1D of #167: setAttOnTxValue setter ──────────────────────
# Verifies the ATT-on-TX setter used by TransmitModel::setPowerUsingTargetDbm
# (Thetis console.cs:46740-46748 [v2.10.3.13]) — the PS-active power-change
# safety gate writes through this method.
#   1. round-trip set/get
#   2. range clamp at +31 / -28 (HL2 bounds via setMin/MaxAttenuation)
#   3. legacy-board lower bound (default min=0)
#   4. per-MAC persistence round-trip
#   5. writes per-band TX storage slot for current band
nereus_add_test(tst_step_att_on_tx_value)

# ── Phase 3M-1a Task G.4: RadioModel::setTune orchestrator ───────────────────
# Verifies all TUNE side-effects outside MoxController:
#   1. tuneRefused emitted when not connected
#   2. USB mode: no DSP swap, no mode-change signal
#   3. LSB mode: no DSP swap
#   4. CWL → swap to LSB on TUN-on
#   5. CWU → swap to USB on TUN-on
#   6. CWL TUN-on → TUN-off restores CWL
#   7. CWU TUN-on → TUN-off restores CWU
#   8. USB mode unchanged after TUN-on/off cycle
#   9. TUN-on pushes tunePowerForBand to connection (txDriveLog)
#  10. TUN-off restores saved power (txDriveLog.last())
#  11. MoxController::manualMoxChanged(true) fires on TUN-on
#  12. MoxController::manualMoxChanged(false) fires on TUN-off
#  13. Null m_txChannel on TUN-on: no crash (null guard)
#  14. Null m_txChannel on TUN-off: no crash (null guard)
#  15. setTune(false) without prior TUN-on: no crash, manualMoxChanged silent
# Cite: Thetis console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged.
nereus_add_test(tst_radio_model_set_tune)
target_compile_definitions(tst_radio_model_set_tune PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task D.1: TxChannel real mic-router drives fexchange2 with Q=0
# Verifies two invariants when a real (non-null) TxMicRouter is attached:
#   1. CALL CHAIN: pullSamples() is called by driveOneTxBlock() via tickForTest()
#      seam; m_inI holds exactly the values the mock emitted (ramp 1, 2, …, n).
#   2. Q=0 INVARIANT: m_inQ is all-zero after every driveOneTxBlock() cycle.
#      Real mic input is mono (real-valued); driveOneTxBlock() zero-fills m_inQ
#      via std::fill before fexchange2 so no stale data contaminates the Q input.
# Test cases (9):
#   - pullSamples is called after one tick (callCount > 0)
#   - inI spot-check: [0]=1.0, [1]=2.0, [mid]=mid+1, [n-1]=n
#   - inI exhaustive ramp check (all 256 elements)
#   - inQ all-zero after one tick
#   - inQ stays zero across 5 consecutive ticks (stale-data regression check)
#   - inI still matches ramp on each of 3 consecutive ticks
#   - inI size == kBufSize (256)
#   - inQ size == kBufSize (256)
#   - !m_running path: pullSamples not called, no crash
# Pre-code review: §0.3 (PcMicSource arch). Test seam added to TxChannel.h
# under NEREUS_BUILD_TESTS guard.
nereus_add_test(tst_tx_channel_real_mic_router)
target_compile_definitions(tst_tx_channel_real_mic_router PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3R K-bench: TxChannel RADE_U/L -> USB/LSB mapping ──────────────
# Reproducer for user-reported "RADE-U + MOX = no RF" symptom from
# bench-test session.  WDSP's SetTXAMode only accepts modes 0..11
# (LSB..DRM); RADE_U=12 / RADE_L=13 are NereusSDR-native dispatch values
# that must be mapped to USB/LSB before reaching the WDSP modulator.
# Test seam lastWdspTxModeForTest() returns the mapped value; the carry
# m_mode preserves the original RADE_U/L for NereusSDR-side dispatch.
# 6 test cases:
#   radeUpperMapsToUsb
#   radeLowerMapsToLsb
#   usbPassesThroughUnchanged
#   lsbPassesThroughUnchanged
#   allOtherModesPassThroughUnchanged (10 modes)
#   radeSwitchBackToUsbResetsMapping (state-machine sequence)
nereus_add_test(tst_tx_channel_rade_mode_mapping)

# ── Phase 3M-1b Task D.2: TxChannel per-mode TXA config setters ──────────────
# Tests setTxMode(DSPMode), setTxBandpass(int, int), and setSubAmMode(int).
#
# setTxMode wraps WDSP SetTXAMode(channelId, mode).
# setTxBandpass wraps WDSP SetTXABandpassFreqs(channelId, f_low, f_high).
# setSubAmMode is deferred to 3M-3b — throws std::logic_error if called.
#
# Porting cites (in TxChannel.h / TxChannel.cpp):
#   setTxMode:      radio.cs:2670-2696 [v2.10.3.13]
#   setTxBandpass:  radio.cs:2730-2780 [v2.10.3.13]
#   setSubAmMode:   radio.cs:2699-2728 [v2.10.3.13] (DEFERRED)
#
# Test cases (13):
#   - setTxMode does not throw for: LSB, USB, DIGL, DIGU, AM, CWL, CWU
#   - setTxBandpass does not throw for: typical SSB, wide SSB, negative-low
#     (LSB IQ-space), zero-bandwidth, symmetric AM
#   - setSubAmMode throws std::logic_error for: 0, 1, 2, -1 (out-of-range),
#     3 (out-of-range)
#   - setSubAmMode throw carries non-empty error message
# Pre-code review: §7 (per-mode TXA config trace).
nereus_add_test(tst_tx_channel_per_mode_config)

# D.3 Phase 3M-1b — VOX/anti-VOX WDSP wrappers
#
# Exercises (NEREUS_BUILD_TESTS required for test-seam accessors):
#   setVoxRun(bool)              → SetDEXPRunVox    [cmaster.cs:199-200 v2.10.3.13]
#   setVoxAttackThreshold(double)→ SetDEXPAttackThreshold [cmaster.cs:187-188]
#   setVoxHangTime(double)       → SetDEXPHoldTime  [cmaster.cs:178-179]
#                                  (WDSP uses HoldTime for VOX hang; no SetDEXPHangTime)
#   setAntiVoxRun(bool)          → SetAntiVOXRun    [cmaster.cs:208-209]
#   setAntiVoxGain(double)       → SetAntiVOXGain   [cmaster.cs:211-212]
#
# Test cases (17):
#   setVoxRun:             first-call-true, first-call-false, round-trip, idempotent
#   setVoxAttackThreshold: first-call, round-trip, idempotent, zero-value
#   setVoxHangTime:        first-call, round-trip, idempotent, zero-value
#   setAntiVoxRun:         first-call-true, round-trip, idempotent
#   setAntiVoxGain:        first-call, round-trip, idempotent
# Pre-code review: §3.5 (VOX/anti-VOX WDSP DLL imports).
nereus_add_test(tst_tx_channel_vox_anti_vox)
target_compile_definitions(tst_tx_channel_vox_anti_vox PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-iii Tasks 1-2: TxChannel DEXP envelope/timing wrappers ───────
#
# Exercises (NEREUS_BUILD_TESTS required for test-seam accessors):
#   setDexpRun(bool)             → SetDEXPRun         [cmaster.cs:166-167 v2.10.3.13]
#   setDexpDetectorTau(double)   → SetDEXPDetectorTau [cmaster.cs:169-170]
#   setDexpAttackTime(double)    → SetDEXPAttackTime  [cmaster.cs:172-173]
#   setDexpReleaseTime(double)   → SetDEXPReleaseTime [cmaster.cs:175-176]
# Thetis ranges (setup.Designer.cs [v2.10.3.13]):
#   udDEXPDetTau   1..100 ms default 20   (lines 45070-45098)
#   udDEXPAttack   2..100 ms default 2    (lines 45027-45055)
#   udDEXPRelease  2..1000 ms default 100 (lines 44967-44995)
nereus_add_test(tst_tx_channel_dexp_envelope)
target_compile_definitions(tst_tx_channel_dexp_envelope PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-iii Tasks 3-5: TxChannel DEXP gate / SCF / look-ahead ────────
#
# Exercises (NEREUS_BUILD_TESTS required for test-seam accessors):
#   setDexpExpansionRatio(double)        → SetDEXPExpansionRatio       [cmaster.cs:181-182 v2.10.3.13]
#                                          (Math.Pow(10, dB/20) at WDSP boundary)
#   setDexpHysteresisRatio(double)       → SetDEXPHysteresisRatio      [cmaster.cs:184-185]
#                                          (Math.Pow(10, -dB/20) — NEGATIVE sign per setup.cs:18924)
#   setDexpLowCut(double)                → SetDEXPLowCut               [cmaster.cs:190-191]
#   setDexpHighCut(double)               → SetDEXPHighCut              [cmaster.cs:193-194]
#   setDexpRunSideChannelFilter(bool)    → SetDEXPRunSideChannelFilter [cmaster.cs:196-197]
#   setDexpRunAudioDelay(bool)           → SetDEXPRunAudioDelay        [cmaster.cs:202-203]
#   setDexpAudioDelay(double)            → SetDEXPAudioDelay           [cmaster.cs:205-206]
# Thetis ranges (setup.Designer.cs [v2.10.3.13]):
#   udDEXPExpansionRatio  0..30 dB    default 10.0  (lines 44876-44905)
#   udDEXPHysteresisRatio 0..10 dB    default 2.0   (lines 44845-44874)
#   udSCFLowCut           100..10000 Hz default 500  (lines 45217-45245)
#   udSCFHighCut          100..10000 Hz default 1500 (lines 45187-45215)
#   chkSCFEnable          default CHECKED            (line 45250)
#   udDEXPLookAhead       10..999 ms default 60      (lines 44765-44793)
#   chkDEXPLookAheadEnable default CHECKED           (line 44808)
nereus_add_test(tst_tx_channel_dexp_gate)
target_compile_definitions(tst_tx_channel_dexp_gate PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-iii Task 17 (bench fix): TxChannel DEXP pushvox bridge ───────
#
# Verifies the WDSP DEXP threshold-crossing callback bridge that was missed
# in the original 3M-3a-iii plan and surfaced on JJ's bench (clicking [VOX]
# correctly set run_vox=1 in WDSP but mic envelope crossings never reached
# MoxController because the WDSP callback was never registered).
#
# Test seam: TxChannel::invokePushVoxForTest(int id, int active) is exposed
# under NEREUS_BUILD_TESTS as a static method that forwards to the static
# s_pushVoxCallback bridge.  WDSP normally invokes the bridge from inside
# `xdexp` on the audio worker thread; the seam lets tests drive it
# synchronously without a live mic stream / DEXP detector.
#
# Source references [v2.10.3.13]:
#   wdsp/dexp.c:330,339      — pushvox firing points (active=1 attack, =0 release)
#   wdsp/dexp.c:399-403      — SendCBPushDexpVox setter signature
#   cmaster.cs:1125          — Thetis analogue registration callsite
#   cmaster.cs:1903-1906     — Thetis VOX.PushVox callback body
nereus_add_test(tst_tx_channel_pushvox_callback)
target_compile_definitions(tst_tx_channel_pushvox_callback PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-iii Task 18 (bench fix): TxChannel VOX-listening pump gate ──
#
# Verifies the VOX-listening pump gate that closes the SECOND of two VOX-
# keying gaps the original 3M-3a-iii plan missed.  Task 17 wired the WDSP
# pushvox callback (commit 56d6921) but the callback never fired because
# the TXA pipeline pump itself only ran when MOX was engaged
# (driveOneTxBlock + driveOneTxBlockFromInterleaved both early-returned
# on !m_running), creating a chicken-and-egg that prevented VOX from
# ever keying the radio (DEXP couldn't fire pushvox if it never saw mic).
#
# Test seam: TxChannel::voxListeningForTest() exposes the m_voxListening
# atomic for round-trip verification under NEREUS_BUILD_TESTS.
#
# Source references [v2.10.3.13]:
#   wdsp/dexp.c:304          — "DEXP code runs continuously so it can be
#                              used to trigger VOX also." (Thetis pumps
#                              continuously; MOX gates radio-write only.)
#   audio.cs:168-192         — Audio.VOXEnabled setter (only flips DEXP
#                              run_vox via cmaster.CMSetTXAVoxRun; does not
#                              touch channel state).
#   cmaster.cs:1039-1052     — CMSetTXAVoxRun → SetDEXPRunVox.
nereus_add_test(tst_tx_channel_vox_listening)
target_compile_definitions(tst_tx_channel_vox_listening PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task D.4: TxChannel::setStageRunning expansion ───────────────
# Verifies Panel (existing), MicMeter, AlcMeter, AmMod, FmMod dispatch.
# MicMeter/AlcMeter: always-on meter stages — no public WDSP Run setter (meter.c);
#   documented no-op with qCDebug log (wdsp/TXA.c:80-93, 379-392 [v2.10.3.13]).
# AmMod/FmMod: run controlled only via SetTXAMode() — no standalone Run setter
#   (wdsp/ammod.c, fmmod.c, TXA.c:753-789 [v2.10.3.13]);
#   documented no-op with qCWarning log.
# Panel: verified as already dispatched (SetTXAPanelRun, patchpanel.c:201-206).
nereus_add_test(tst_tx_channel_stage_running_expansion)

# ── Phase 3M-1b Task D.5: TxChannel::sip1OutputReady signal ──────────────────
# Verifies the MON-path siphon signal emitted after each fexchange2 cycle.
#
# Signal contract (Plan §4.3, pre-code review §4.3):
#   void sip1OutputReady(const float* samples, int frames)
#   - Emitted AFTER fexchange2 + sendTxIq in driveOneTxBlock().
#   - DirectConnection-only: pointer valid only during synchronous dispatch.
#   - samples: m_outI.data() — post-SSB-modulator I-channel output.
#   - frames:  m_outputBufferSize (256 for P1/HL2; 1024 for P2 Saturn).
#
# Test cases (6):
#   - sip1OutputReady fires exactly once per tickForTest() call.
#   - frames argument matches m_outputBufferSize (kBufSize = 256).
#   - signal fires on every tick across 5 cycles (count == tick).
#   - signal does NOT fire when m_running is false (early-exit guard).
#   - signal does NOT fire when m_connection is null (early-exit guard).
#   - frames matches non-default outputBufferSize (1024 = P2 Saturn).
nereus_add_test(tst_tx_channel_sip1_signal)
target_compile_definitions(tst_tx_channel_sip1_signal PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task D.6: TxChannel::setMicPreamp + recomputeTxAPanelGain1 ───
#
# Exercises (NEREUS_BUILD_TESTS required for lastMicPreampForTest accessor):
#   setMicPreamp(double)          → SetTXAPanelGain1 [dsp.cs:411-412 v2.10.3.13]
#   recomputeTxAPanelGain1()      → SetTXAPanelGain1 force-refresh
#
# Mic-mute semantic (Thetis console.cs:28805-28817 [v2.10.3.13]):
#   chkMicMute.Checked == true  → mic active → MicPreamp = pow(10, gain_db/20)
#   chkMicMute.Checked == false → mic MUTED  → MicPreamp = 0.0
#   setMicPreamp(0.0) maps to the mute=true (Checked==false) path.
#
# Test cases (8):
#   setMicPreamp: initial-NaN, non-zero-value, zero-value (mute case),
#                 round-trip-mute, round-trip-unmute, idempotent
#   recomputeTxAPanelGain1: does-not-crash, after-setMicPreamp-pushes-latest
# Pre-code review: §2.1 (MicMute counter-intuitive naming).
nereus_add_test(tst_tx_channel_mic_mute)
target_compile_definitions(tst_tx_channel_mic_mute PRIVATE NEREUS_BUILD_TESTS)

# Phase 3M-1b D.7 — TxChannel TX meter readouts
#
# Tests:
#   getTxMicMeter()  — wired: reads GetTXAMeter(ch, TXA_MIC_PK=0); returns
#                      kMeterUninitialisedSentinel (-999.0f) when channel not open
#   getAlcMeter()    — wired: reads GetTXAMeter(ch, TXA_ALC_PK=12); same sentinel
#   getEqMeter()     — deferred 3M-3a: returns 0.0f unconditionally
#   getLvlrMeter()   — deferred 3M-3a: returns 0.0f unconditionally
#   getCfcMeter()    — deferred 3M-3a: returns 0.0f unconditionally
#   getCompMeter()   — deferred 3M-3a: returns 0.0f unconditionally
#   kMeterUninitialisedSentinel == -999.0f
#
# Enum value provenance: Thetis wdsp/TXA.h:49-69 [v2.10.3.13] txaMeterType.
# Pre-code review: §1.4 (TX meters live row) + master design §5.2.1.
#
# Test cases (11):
#   sentinel: value-is-minus-999
#   active: getTxMicMeter uninitialised×2, getAlcMeter uninitialised×2
#   deferred: getEqMeter, getLvlrMeter, getCfcMeter, getCompMeter (all zero)
#   distinctness: deferred-not-sentinel, sentinel-not-zero
nereus_add_test(tst_tx_channel_meters)

# ── Phase 3M-3a-iii Task 6: TxChannel DEXP meter readers ─────────────────────
# Pure crash-free + sane-bounds smoke test for two `const noexcept` accessors:
#   getDexpPeakSignal()  — LINEAR amplitude 0..1 (caller does 20*log10 → dB)
#                          From Thetis cmaster.cs:163-164 [v2.10.3.13] +
#                          wdsp/dexp.c:647-654 + console.cs:28952 callsite.
#   getTxMicMeterDb()    — RAW negative dB (caller does sign + 3 dB offset)
#                          From Thetis console.cs:25346-25359 [v2.10.3.13] +
#                          dsp.cs:992-1057 (CalculateTXMeter MIC -> TXA_MIC_AV).
#
# WDSP channel never opened in test build → both wrappers must null-guard
# (pdexp[m_channelId] == nullptr / txa[m_channelId].rsmpin.p == nullptr) and
# return Thetis-faithful idle defaults: 0.0 (DEXP peak) and -200.0 (mic dB).
#
# Test cases (4):
#   - getDexpPeakSignal_returnsZeroWhenIdle              (peak in [0.0, 1.0))
#   - getDexpPeakSignal_callableMultipleTimes_noCrash    (20 successive reads)
#   - getTxMicMeterDb_returnsFloorWhenIdle               (db in [-200.0, 0.0])
#   - getTxMicMeterDb_callableMultipleTimes_noCrash      (20 successive reads)
nereus_add_test(tst_tx_channel_dexp_meters)

# ── Phase 3M-1b Task E.1: AudioEngine::pullTxMic ─────────────────────────────
# Verifies the TX-mic pull accessor that PcMicSource (Phase F.1) will tap:
#   nullBus_returnsZero       — no bus configured → 0
#   zeroN_returnsZero         — n <= 0 guard → 0
#   nullDst_returnsZero       — null dst guard → 0
#   emptyBus_returnsZero      — bus open but no data → 0
#   int16Stereo_returnsSampleCount        — 2 stereo frames → 2
#   int16StereoToFloat32Mono_correctConversion — left/32768, right discarded
#   float32StereoToMono_correctPassthrough     — left taken, right discarded
#   float32Mono_fullTransfer  — mono bus, all 3 samples pass through
#   partialBus_returnsActualCount — 1 frame available, asked for 4 → 1
#
# Uses FakeAudioBus injected via NEREUS_BUILD_TESTS seam setTxInputBusForTest.
# Pre-code review: §0.3 (PcMicSource arch).
nereus_add_test(tst_audio_engine_pull_tx_mic)
target_compile_definitions(tst_audio_engine_pull_tx_mic PRIVATE NEREUS_BUILD_TESTS)

# (Phase 3M-1c D.1/D.2 + L.4 — tst_audio_engine_mic_block_ready and
#  tst_mic_re_blocker — were dropped by the TX pump architecture
#  redesign on 2026-04-29.  The 720-sample mic accumulator and
#  720→256 re-blocker they tested were deleted along with their
#  underlying classes.  See plan §5.3.)

# ── Phase 3M-1c TX pump architecture redesign: TxWorkerThread ────────────
# Verifies the new worker-thread pump that replaces D.1/E.1/L.4 + bench
# fixes.  Test cases (8):
#   - constructDestruct_clean
#   - startPump_withoutDeps_warns_andStaysStopped
#   - startStop_lifecycle_isRunningTracksThread
#   - tickForTest_drivesOneSendTxIq_whenBusHasFullBlock
#   - tickForTest_drivesFexchange2_onEmptyBus_zeroFilled
#   - tickForTest_zeroFillsGap_whenBusPartial
#   - realWorker_pumpCadence_approx5ms
#   - crossThreadSetter_noCrash_finalValueLands
# Plan: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md §5.4.
nereus_add_test(tst_tx_worker_thread)
target_compile_definitions(tst_tx_worker_thread PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3R Task K2: TxWorkerThread::TxPath enum scaffolding ──
# Pins the contract of the lightweight TxPath enum + std::atomic
# m_currentTxPath member + setCurrentTxPath cross-thread slot that
# K-bench will use to route DSPMode::RADE_U / RADE_L slices through
# RadeChannel rather than the WDSP TXA chain.
nereus_add_test(tst_tx_worker_rade_path_enum)
target_compile_definitions(tst_tx_worker_rade_path_enum PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3R Task K3: RADE TX DSP helpers ──
# RadeTxHpf80 (80 Hz Butterworth HPF biquad) + RadeTx48to16
# (48 kHz -> 16 kHz mono downsampler around r8brain).  Both are
# standalone DSP units; K-bench will wire them into TxWorkerThread's
# RADE branch.  Unit-tested here in isolation to pin the DSP
# correctness contract.
nereus_add_test(tst_rade_tx_filters)

# ── Phase 3M-1c TX pump v3 — TxMicSource (Thetis Inbound/cm_main port) ──
# Verifies the ring + semaphore primitives that source the worker's
# cadence from radio mic frames.  Test cases (8):
#   - constructStartStop_clean
#   - waitForBlock_returnsFalse_beforeAnyInbound
#   - inbound_oneBlock_releasesOneSemaphore_drainYieldsSamples
#   - inbound_threeBlocks_releasesThreeSemaphores
#   - inbound_wrapAware_preservesSampleOrder
#   - inbound_partialBlock_doesNotReleaseUntilComplete
#   - stop_unblocksConsumer_inWaitForBlockInfinite
#   - concurrent_producerConsumer_noDataCorruption
nereus_add_test(tst_tx_mic_source)
target_compile_definitions(tst_tx_mic_source PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c TX pump v3 — P1 EP6 mic16 extraction ────────────────────
# Verifies the static parseEp6Frame mic-aware overload extracts the
# mic16 byte zone correctly from synthetic EP6 frames; covers the
# normal mic16 path, the HL2 mic16-zeros quirk, and backward-compat
# with the existing 3-arg overload.
nereus_add_test(tst_p1_mic_extraction)
target_compile_definitions(tst_p1_mic_extraction PRIVATE NEREUS_BUILD_TESTS)

# ── HL2 fix — P1 mic decimation (radio-rate → 48 kHz) ───────────────────
# Verifies the static decimateMicSamples helper and setSampleRate's
# update of the per-rate decimation factor (1/2/4/8 for 48/96/192/384 kHz)
# matching Thetis ChannelMaster/networkproto1.c:391-410 +
# netInterface.c:1287-1310 [v2.10.3.14].
nereus_add_test(tst_p1_mic_decimation)
target_compile_definitions(tst_p1_mic_decimation PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c TX pump v3 — P2 port-1026 mic frame decoder ─────────────
# Verifies the static decodeMicFrame132 helper used by the port-1026
# dispatch path.  Hand-crafted 132-byte frames; covers the normal data
# path, zero data, and wrong-size rejection.
nereus_add_test(tst_p2_mic_frame)
target_compile_definitions(tst_p2_mic_frame PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c E.1: TxChannel push-driven refactor ─────────────────────
# Verifies the QTimer-driven pull model is gone and the (samples, frames)
# push slot is the single fexchange2 entry point.  Test cases (7):
#   - pushFullBlock_drivesOneSendTxIq            — full block → 1 fexchange2
#   - pushNullSamples_stillDrivesSendTxIq        — TUNE silence path
#   - pushMismatchedFrameCount_skipsFexchange2   — contract violation: warn + skip
#   - tickForTest_isSynchronousPushEntryPoint    — 2 ticks → 2 sendTxIq
#   - noTimer_doesNotFireWithoutPush             — no QTimer dispatches
#   - pushWhileNotRunning_isNoOp                 — !running guard
#   - pushWithNullConnection_isNoOp              — null-connection guard
# Pre-code review: Phase 3M-1c E.1.
nereus_add_test(tst_tx_channel_push_driven)
target_compile_definitions(tst_tx_channel_push_driven PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c E.1: TxChannel zero-fill workaround removal ─────────────
# Verifies the 1b353f4 partial-read zero-fill workaround is gone in the
# push-driven model.  Test cases (4):
#   - thirtyPushCycles_yieldThirtySendTxIqCalls   — no missed cycles
#   - inIBufferMatchesPushedSamplesExactly        — no zero-fill contamination
#   - inQBufferStaysZeroAcrossCycles              — Q=0 over 5 cycles
#   - nullPush_zeroFillsInputBuffers              — silence path correctness
# Pre-code review: Phase 3M-1c E.1.
nereus_add_test(tst_tx_channel_no_zero_fill)
target_compile_definitions(tst_tx_channel_no_zero_fill PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1c E.2-E.6: TXA PostGen wrapper setters ────────────────────
#
# Twelve thin C++ wrappers over the WDSP `SetTXAPostGen*` family that drive
# the gen1 (TXA stage 22) two-tone IMD test source.
#
# Setters under test:
#   E.2: setTxPostGenMode(int)                      → SetTXAPostGenMode
#                                                     [setup.cs:11084/11096 v2.10.3.13]
#   E.3: setTxPostGenTTFreq1(double)                → SetTXAPostGenTTFreq
#        setTxPostGenTTFreq2(double)                  [setup.cs:11099-11100 v2.10.3.13]
#        setTxPostGenTTMag1(double)                 → SetTXAPostGenTTMag
#        setTxPostGenTTMag2(double)                   [setup.cs:11102-11103 v2.10.3.13]
#   E.4: setTxPostGenTTPulseToneFreq1(double)       → SetTXAPostGenTTPulseToneFreq
#        setTxPostGenTTPulseToneFreq2(double)         [setup.cs:11087-11088 v2.10.3.13]
#        setTxPostGenTTPulseMag1(double)            → SetTXAPostGenTTPulseMag
#        setTxPostGenTTPulseMag2(double)              [setup.cs:11090-11091 v2.10.3.13]
#   E.5: setTxPostGenTTPulseFreq(int)               → SetTXAPostGenTTPulseFreq
#        setTxPostGenTTPulseDutyCycle(double)       → SetTXAPostGenTTPulseDutyCycle
#        setTxPostGenTTPulseTransition(double)      → SetTXAPostGenTTPulseTransition
#                                                     [setup.cs:34415-34417 v2.10.3.13]
#   E.6: setTxPostGenRun(bool)                      → SetTXAPostGenRun
#                                                     [setup.cs:11107/11166 v2.10.3.13]
#
# Test strategy: pure smoke / does-not-crash, matching the convention
# established by tst_tx_channel_tune_tone.cpp.  The Freq1/Freq2 / Mag1/Mag2
# split-property cache is exercised via the mixedCallSequence regression
# tests (continuous + pulsed) plus a zero-values stress test.
#
# Test cases (15):
#   E.2 mode: off, continuous (mode=1), pulsed (mode=7) — 3 cases
#   E.3 continuous TT: setTxPostGenTTFreq1/Freq2/Mag1/Mag2 typical — 4 cases
#   E.4 pulsed TT:     setTxPostGenTTPulseToneFreq1/Freq2/Mag1/Mag2 — 4 cases
#   E.5 pulse profile: setTxPostGenTTPulseFreq/DutyCycle/Transition — 3 cases
#   E.6 run:          setTxPostGenRun on/off — 2 cases
#   Cache integrity: continuousTT_mixedCallSequence + pulsedTT_mixedCallSequence
#   Edge: setters_zeroValues_doNotCrash
# (15 individual private-slot test methods total.)
nereus_add_test(tst_tx_channel_tx_post_gen_setters)

# ── Phase 3M-3a-i Task B-1: TxChannel TX EQ wrappers ───────────────────────
# Plan: 3M-3a-i Batch 1 Task B-1 — 7 TX EQ wrapper setters (Run / Graph10 /
# Profile / Nc / Mp / Ctfmode / Wintype) on top of WDSP eq.c [v2.10.3.13].
# Test strategy: pure smoke / does-not-crash, matching tst_tx_channel_tx_post_gen_setters
# convention.  Default-constructed TxChannel falls through the rsmpin.p==nullptr
# null-guard for WDSP-linked builds; HAVE_WDSP-undefined builds exercise the stub.
nereus_add_test(tst_tx_channel_eq_setters)

# ── Phase 3M-3a-i Task B-2: TxChannel Leveler/ALC wrappers ─────────────────
# Plan: 3M-3a-i Batch 1 Task B-2 — 5 wrappers (Leveler Run/Top/Decay,
# ALC MaxGain/Decay) over wcpAGC.c [v2.10.3.13] + Stage::Leveler / Stage::Alc
# cases added to setStageRunning.  Same does-not-crash strategy as B-1.
nereus_add_test(tst_tx_channel_leveler_alc_setters)

# ── Phase 3M-3a-ii Task B-3: TxChannel CFC + CPDR + CESSB wrappers ─────────
# Plan: 3M-3a-ii Batch 1 Task B-3 — 9 wrappers over the WDSP TXA dynamics
# section: CFC (cfcomp.c:632-737), CPDR (compress.c:99-117), CESSB
# (osctrl.c:142-150) [all v2.10.3.13].  Includes profile-array validation
# coverage (empty / mismatched-length early-return) plus regression coverage
# for the existing Stage::CfComp / Stage::Compressor / Stage::OsCtrl arms in
# setStageRunning.  Same does-not-crash strategy as B-1 / B-2.
nereus_add_test(tst_tx_channel_cfc_cpdr_cessb_setters)

# ── Phase 3M-3a-ii follow-up Batch 7: TxChannel CFC live-display readback ──
# Plan: 3M-3a-ii follow-up Batch 7 — TxChannel::getCfcDisplayCompression
# wraps WDSP::GetTXACFCOMPDisplayCompression (cfcomp.c:740-757 [v2.10.3.13]).
# Pure pass-through with null + size validation; live-data correctness is
# integration-tested via the parametric-EQ widget bar chart in Tasks 8 + 9.
# This test pins the validation arms (null buffer, too-small buffer) and the
# two public constants (kCfcDisplayBinCount = 1025, kCfcDisplaySampleRateHz
# = 48000.0) against Thetis source ranges.
nereus_add_test(tst_tx_channel_cfc_display)

# ── Issue #167 Phase 1 Agent 1C: TxChannel::setTxFixedGain wrapper ──────────
# Phase 1 foundation work for the PA-calibration safety hotfix.  Mirrors
# Thetis cmaster.cs:1115-1119 CMSetTXOutputLevel [v2.10.3.13]:
#   double level = Audio.RadioVolume * Audio.HighSWRScale;
#   cmaster.SetTXFixedGain(0, level, level);
# The wrapper takes the already-composed `level` and pushes it through to
# the WDSP/ChannelMaster SetTXFixedGain entry point with Igain == Qgain.
# RadioModel call-site composition (audio_volume * swrProtect) is deferred to
# Phase 4A.  Test strategy mirrors tst_tx_channel_mic_mute.cpp: NaN-aware
# idempotent guard verified via lastFixedGainForTest() seam.
nereus_add_test(tst_tx_channel_set_tx_fixed_gain)
target_compile_definitions(tst_tx_channel_set_tx_fixed_gain PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-3a-i Task C: TransmitModel TX EQ + Leveler + ALC properties ──
# Plan: 3M-3a-i Batch 1 Task C — 28 new properties on TransmitModel:
#   - 23 TXProfile keys: TXEQEnabled / TXEQNumBands / TXEQPreamp / TXEQ1..10 /
#     TxEqFreq1..10 / Lev_On / Lev_MaxGain / Lev_Decay / ALC_MaximumGain /
#     ALC_Decay
#   - 4 globals (NOT in TXProfile): eq/nc / eq/mp / eq/ctfmode / eq/wintype
# Tests cover defaults (matching Thetis database.cs:4552-4594 + WDSP TXA.c
# create_eqp G[]/F[]), signal emission idempotency, range clamping
# (Designer ranges from setup.Designer.cs), per-MAC AppSettings round-trip,
# and multi-MAC isolation.
nereus_add_test(tst_transmit_model_eq_lev_alc)

# Plan: 3M-3a-ii Batch 2 — 15 new properties on TransmitModel covering CFC
# (8), CPDR (2), CESSB (1), and Phase Rotator (4):
#   - 14 TXProfile keys: CFCEnabled / CFCPostEqEnabled / CFCPreComp /
#     CFCPostEqGain / CFCEqFreq0..9 / CFCPreComp0..9 / CFCPostEqGain0..9 /
#     CFCParaEQData / CFCPhaseRotatorEnabled / CFCPhaseReverseEnabled /
#     CFCPhaseRotatorFreq / CFCPhaseRotatorStages / CompanderLevel / CESSB_On
#   - 1 global (NOT in TXProfile): cpdr/on (under hardware/<mac>/tx/cpdr/on)
# Tests cover defaults (matching Thetis database.cs:4724-4768), signal
# emission idempotency, range clamping (Designer ranges from
# setup.Designer.cs udPhRotFreq/udPHROTStages, frmCFCConfig.Designer.cs
# nudCFC_*, console.Designer.cs ptbCPDR), per-MAC AppSettings round-trip,
# and multi-MAC isolation.
nereus_add_test(tst_transmit_model_cfc_cpdr_cessb_phrot)

# Plan: 3M-1b E.2. Exercises AudioEngine TX monitor enable + volume state
# (setTxMonitorEnabled / setTxMonitorVolume / signals / clamping / idempotency).
# Pre-code review §4.4.
nereus_add_test(tst_audio_engine_tx_monitor_state)

# Plan: 3M-1b E.3. Exercises AudioEngine::txMonitorBlockReady — the
# audio-thread consumer of TxChannel::sip1OutputReady.
# Cases:
#   disabled_noOutput               — monitor off → mixInto returns silence
#   enabled_hasOutput               — monitor on → mixInto returns audio
#   volumeScalesOutput              — gain 1.0 vs 0.5 produces proportional output
#   nullSamples_noOp                — null ptr guard → no crash, no output
#   zeroFrames_noOp                 — frames=0 guard → no output
#   volumeZero_thenNonZero_hasOutput — gain=0 produces silence; gain=1 produces audio
#   monoExpandedToStereo_LequalsR   — mono sample → L=R in stereo mix
#   disableAfterEnable_silencesNextBlock — re-disable silences subsequent blocks
#
# Uses masterMixForTest() NEREUS_BUILD_TESTS seam to observe MasterMixer output.
# Pre-code review §4.3 + §4.4.
nereus_add_test(tst_audio_engine_tx_monitor_block)
target_compile_definitions(tst_audio_engine_tx_monitor_block PRIVATE NEREUS_BUILD_TESTS)

# Plan: 3M-1b E.4. Exercises AudioEngine::rxBlockReady MOX gate —
# the RX-leak-during-MOX fold that fixes the PR #144 cosmetic regression.
# Cases:
#   moxState_default_isFalse             — m_moxActive defaults false
#   setMoxState_atomic_round_trip        — store/load round-trip
#   rxBlockReady_moxOff_blockFlows       — MOX off → block reaches mixer
#   rxBlockReady_moxOn_activeSlice_blockDropped
#                                        — MOX on + active slice → dropped
#   rxBlockReady_moxOn_nonActiveSlice_blockFlows
#                                        — MOX on + non-active slice → flows
#   rxBlockReady_moxOnToOff_blockResumes — MOX off again → block resumes
#
# Uses setMoxStateForTest() NEREUS_BUILD_TESTS seam and masterMixForTest()
# to observe MasterMixer output.
# Pre-code review §10.3 + §10.4.
nereus_add_test(tst_audio_engine_rx_leak_during_mox)
target_compile_definitions(tst_audio_engine_rx_leak_during_mox PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task F.1: PcMicSource ────────────────────────────────────────
# Verifies that PcMicSource::pullSamples correctly dispatches to
# AudioEngine::pullTxMic via the NEREUS_BUILD_TESTS seam:
#   nullEngine_returnsZero        — null engine → 0, buffer untouched
#   validEngine_noBusData_returnsZero — engine ok, bus empty → 0
#   validEngine_withBusData_returnsSampleCount — 3 frames → 3
#   validEngine_correctSamplesForwarded — dispatch is transparent (no scaling)
#   zeroN_returnsZero             — n=0 propagated from AudioEngine guard
#   nullDst_returnsZero           — nullptr dst propagated from AudioEngine guard
#   viaBasePointer_dispatchWorks  — virtual dispatch through TxMicRouter*
#
# Uses FakeAudioBus injected via setTxInputBusForTest (NEREUS_BUILD_TESTS seam).
# Pre-code review: §0.3 + §12.7 (PcMicSource arch lock).
nereus_add_test(tst_pc_mic_source)
target_compile_definitions(tst_pc_mic_source PRIVATE NEREUS_BUILD_TESTS)

# ── VAX TX → mic source (eager-borg-d64bed, 2026-05-06) ──────────────────────
# Verifies VaxTxMicSource (mirrors PcMicSource pattern) and the
# stereo→mono downmix in AudioEngine::pullVaxTxMic. Uses FakeAudioBus
# injected via the setVaxTxBusForTest seam.
#
# Closes the long-standing TODO at AudioEngine.cpp:306 (Sub-Phase 9
# VAX TX consumer) — the path 3rd-party-app audio → "NereusSDR TX"
# CoreAudio device → /nereussdr-vax-tx shm → CompositeTxMicRouter →
# TxChannel → VOX.
nereus_add_test(tst_vax_tx_mic_source)
target_compile_definitions(tst_vax_tx_mic_source PRIVATE NEREUS_BUILD_TESTS)

# ── Mic_Source pre-connect persistence fallback (eager-borg-d64bed) ──────────
# Verifies the pre-connect global key fallback added so the user's
# Setup -> Audio -> TX Input click survives even when no radio is
# bound at click time.  Cases: pre-connect click writes global,
# load with no per-MAC uses fallback, per-MAC overrides preconnect,
# post-connect writes don't touch preconnect, first-run defaults to Pc.
nereus_add_test(tst_transmit_model_mic_source_preconnect)

# ── Phase 3M-VAX-toggle: TransmitModel previousNonVaxMicSource + toggleVaxSource ──
# Verifies the model-layer plumbing for the PhoneCwApplet VAX button:
# previous-source tracking on every non-VAX setMicSource write, toggleVaxSource
# slot (Vax on -> sets Vax; off -> restores previous), HL2 lock interaction
# (Radio previous coerces to Pc), and Mic_Source_PreVax persistence (per-MAC
# + preconnect fallback, mirroring the existing Mic_Source two-key pattern).
nereus_add_test(tst_transmit_model_vax_toggle)

# ── Phase 3M-1b Task F.4: RadioConnection::micFrameDecoded signal ─────────────
# Verifies the micFrameDecoded Qt signal added to the RadioConnection base class.
# Uses TestRadioConnection minimal concrete subclass (stubs all pure-virtuals).
# Cases (4):
#   micFrameDecoded_signalExists        — QSignalSpy attaches successfully
#   micFrameDecoded_emit_capturedBySpy  — emitMicFrame → spy.count() == 1
#   micFrameDecoded_signalCarriesFrameCount — args.at(1) int frames matches n
#   micFrameDecoded_multipleEmissions   — spy accumulates across 3 emits
#
# DirectConnection-only contract documented in RadioConnection.h.
# Pre-code review §6.4.
nereus_add_test(tst_radio_connection_mic_frame_signal)

# ── Phase 3M-1b Task F.2: RadioMicSource ─────────────────────────────────────
# Verifies RadioMicSource — the TxMicRouter implementation that drains a
# lock-free SPSC float ring fed by RadioConnection::micFrameDecoded.
# Uses TestRadioConnection (inline subclass) to drive micFrameDecoded
# synchronously via DirectConnection. Test seam: NEREUS_BUILD_TESTS exposes
# ringFillForTest() and ringDroppedForTest().
# Cases (14):
#   pullSamples_emptyRing_zeroFills         — empty ring → zero-fill + return n
#   pullSamples_partialRing_returnsZeroFilledRest
#                                           — 4 samples pushed, pull 8 → 4 data + 4 zero
#   pullSamples_fullDrain_returnsAllSamples — push n, pull n → ring empty
#   pullSamples_fifoOrder_preserved         — 16 values in order
#   onMicFrame_pushesToRing                 — ringFill increases after emit
#   onMicFrame_overflow_dropsExtra_recordsDropCount
#                                           — full ring → 5 more dropped, counter=5
#   onMicFrame_nullSamples_noOp             — nullptr samples → no crash, no state change
#   onMicFrame_zeroFrames_noOp              — frames=0 → no crash, no state change
#   roundTrip_pushAndPull                   — two batches pushed, read back in order
#   multipleCycles_fifo_preserved           — 8×1024 cycles, crosses ring wraparound
#   nullConnection_constructorOk            — nullptr conn → constructs ok, zero-fills
#   pullSamples_returnAlwaysN               — underrun → returns n (not toRead)
#   pullSamples_nullDst_returnsZero         — nullptr dst → 0, ring unchanged
#   viaBasePointer_dispatchWorks            — virtual dispatch through TxMicRouter*
#
# Master design §5.2.1. Plan: 3M-1b F.2.
nereus_add_test(tst_radio_mic_source)
target_compile_definitions(tst_radio_mic_source PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task F.3: CompositeTxMicRouter ───────────────────────────────
# Verifies CompositeTxMicRouter — the strategy-pattern selector that holds
# PcMicSource + RadioMicSource and dispatches pullSamples based on MicSource
# selection, with HL2 force-PC and MOX-locked switching.
# Uses real PcMicSource (FakeAudioBus-backed AudioEngine) and real
# RadioMicSource (TestRadioConnection). Test seam: NEREUS_BUILD_TESTS exposes
# hasPendingSwitchForTest().
# Cases (14):
#   defaultSource_isPc                           — construction defaults to Pc
#   setActiveSource_Pc_dispatchesToPc            — Pc selected → PC data flows
#   setActiveSource_Radio_dispatchesToRadio      — Radio selected → radio data
#   setActiveSource_Radio_HL2force_dispatchesToPc — hasMicJack=false → forced Pc
#   setActiveSource_Radio_nullRadioSource_dispatchesToPc — null radio → forced Pc
#   setActiveSource_Pc_duringMox_appliesImmediately — Pc during MOX → deferred
#   setActiveSource_Radio_duringMox_isPending_appliedOnMoxOff
#                                                — Radio during MOX → pending → applied on MOX-off
#   setActiveSource_Radio_duringMox_HL2force_isIgnored — double-gate: HL2+MOX → ignored
#   setMoxActive_offToOn_noEffectOnSource        — MOX-on alone doesn't change source
#   setMoxActive_onToOff_appliesPending          — MOX-off applies pending switch
#   setMoxActive_onToOff_noPending_noEffect      — MOX-off with no pending → no change
#   pullSamples_nullDst_returnsZero              — null dst → 0
#   pullSamples_zeroN_returnsZero                — n=0 → 0
#   pullSamples_noSources_zeroFills              — both sources null → zero-fill
#
# Pre-code review §0.3 + master design §5.2.1. Plan: 3M-1b F.3.
nereus_add_test(tst_tx_mic_router_selector)
target_compile_definitions(tst_tx_mic_router_selector PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.1: P1 mic_boost wire-byte snapshot ────────────────────
# 14 tests pin the mic_boost bit (C2 bit 0 = 0x01) of bank 10 (C0=0x12) in
# P1RadioConnection::setMicBoost():
#   1.  Default state: C2 bit 0 = 0.
#   2.  setMicBoost(true)  → C2 bit 0 = 1.
#   3.  setMicBoost(false) → C2 bit 0 = 0.
#   4.  Bit 6 (0x40) always set regardless of mic_boost.
#   5.  No spillover into C2 bits 7, 5..2, 1.
#   6.  Bank 10 C0 address stays 0x12.
#   7.  setMicBoost sets m_forceBank10Next (Codex P2 flush pattern).
#   8.  Repeated same-value call still sets the flush flag.
#   9.  Idempotency: setMicBoost(true) twice doesn't crash.
#  10.  Round-trip: true → false produces correct bit values.
#  11.  Codec path (Standard): C2 bit 0 set/cleared correctly.
#  12.  Codec path (Standard): C2 bit 0 clear after false.
#  13.  Codec path (HL2): C2 bit 0 set (FW ignores, but bit is correct).
#  14.  T/R relay byte (C3) unaffected by mic_boost changes.
# Source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13].
nereus_add_test(tst_p1_mic_boost_wire)
target_compile_definitions(tst_p1_mic_boost_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.1: P2 mic_boost wire-byte snapshot ────────────────────
# 10 tests pin the mic_boost bit (CmdTx byte 50 bit 1 = 0x02) in
# P2RadioConnection::setMicBoost():
#   1.  Default state: byte 50 bit 1 = 0.
#   2.  setMicBoost(true)  → byte 50 bit 1 = 1.
#   3.  setMicBoost(false) → byte 50 bit 1 = 0.
#   4.  Round-trip true → false → true.
#   5.  Bit 0 (line_in) unaffected by mic_boost.
#   6.  Bits 2-7 of byte 50 unaffected.
#   7.  Idempotency: setMicBoost(true) twice → bit remains 1.
#   8.  Codec path (OrionMKII): mic_boost at byte 50 bit 1.
#   9.  Codec path (Saturn/ANAN-G2): same behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setMicBoost.
# Source: deskhpsdr/src/new_protocol.c:1484-1486 [@120188f].
# First deskhpsdr port in NereusSDR — DESKHPSDR-PROVENANCE.md updated.
nereus_add_test(tst_p2_mic_boost_wire)
target_compile_definitions(tst_p2_mic_boost_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.2: P1 line_in wire-byte snapshot ──────────────────────
# 14 tests pin the line_in bit (C2 bit 1 = 0x02) of bank 10 (C0=0x12) in
# P1RadioConnection::setLineIn():
#   1.  Default state: C2 bit 1 = 0.
#   2.  setLineIn(true)  → C2 bit 1 = 1.
#   3.  setLineIn(false) → C2 bit 1 = 0.
#   4.  Bit 6 (0x40) always set regardless of line_in.
#   5.  No spillover into C2 bits 7, 5..2, 0.
#   6.  Bank 10 C0 address stays 0x12.
#   7.  setLineIn sets m_forceBank10Next (Codex P2 flush pattern).
#   8.  Repeated same-value call still sets the flush flag.
#   9.  Idempotency: setLineIn(true) twice doesn't crash.
#  10.  Round-trip: true → false produces correct bit values.
#  11.  Codec path (Standard): C2 bit 1 set/cleared correctly.
#  12.  Codec path (Standard): C2 bit 1 clear after false.
#  13.  Codec path (HL2): C2 bit 1 set (FW ignores, but bit is correct).
#  14.  setLineIn(true) does NOT touch C2 bit 0 (mic_boost) — cross-bit guard.
# Source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13].
nereus_add_test(tst_p1_line_in_wire)
target_compile_definitions(tst_p1_line_in_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.2: P2 line_in wire-byte snapshot ──────────────────────
# 10 tests pin the line_in bit (CmdTx byte 50 bit 0 = 0x01) in
# P2RadioConnection::setLineIn():
#   1.  Default state: byte 50 bit 0 = 0.
#   2.  setLineIn(true)  → byte 50 bit 0 = 1.
#   3.  setLineIn(false) → byte 50 bit 0 = 0.
#   4.  Round-trip true → false → true.
#   5.  Bit 1 (mic_boost) unaffected by line_in — cross-bit guard.
#   6.  Bits 2-7 of byte 50 unaffected.
#   7.  Idempotency: setLineIn(true) twice → bit remains 1.
#   8.  Codec path (OrionMKII): line_in at byte 50 bit 0.
#   9.  Codec path (Saturn/ANAN-G2): same behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setLineIn.
# Source: deskhpsdr/src/new_protocol.c:1480-1482 [@120188f].
nereus_add_test(tst_p2_line_in_wire)
target_compile_definitions(tst_p2_line_in_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.3: P1 mic_trs wire-byte snapshot ──────────────────────
# 14 tests pin the mic_trs bit (C1 bit 4 = 0x10, INVERTED) of bank 11 (C0=0x14)
# in P1RadioConnection::setMicTipRing().
# POLARITY INVERSION: tipHot=true (Tip-is-mic) → wire bit 0;
#                     tipHot=false (Tip-is-BIAS) → wire bit 1.
#   1.  Default state: C1 bit 4 = 0 (default tipHot=true).
#   2.  setMicTipRing(true)  → C1 bit 4 = 0 (tip-is-mic, wire inverted).
#   3.  setMicTipRing(false) → C1 bit 4 = 1 (tip-is-BIAS, wire inverted).
#   4.  Round-trip: false → true → bit clears.
#   5.  Bank 11 C0 address stays 0x14.
#   6.  setMicTipRing(false) does NOT clobber C1 bits 0-3 (preamp).
#   7.  setMicTipRing(true) leaves C1 all-zero in default state.
#   8.  setMicTipRing sets m_forceBank11Next (Codex P2 flush pattern).
#   9.  Repeated same-value call still sets the flush flag.
#  10.  Idempotency: setMicTipRing(false) twice → bit stays 1.
#  11.  Codec path (Standard): setMicTipRing(false) → C1 bit 4 set.
#  12.  Codec path (Standard): setMicTipRing(true) → C1 bit 4 clear.
#  13.  Codec path (HL2): setMicTipRing(false) → C1 bit 4 set (FW ignores).
#  14.  setMicTipRing(false) does NOT touch C1 bits 0-3 — cross-bit guard.
# Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13].
nereus_add_test(tst_p1_mic_tip_ring_wire)
target_compile_definitions(tst_p1_mic_tip_ring_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.3: P2 mic_tip_ring wire-byte snapshot ─────────────────
# 10 tests pin the mic_ptt_tip_bias_ring bit (CmdTx byte 50 bit 3 = 0x08,
# INVERTED) in P2RadioConnection::setMicTipRing().
# POLARITY INVERSION: tipHot=true → byte-50 bit 3 = 0; tipHot=false → 1.
#   1.  Default state: byte 50 bit 3 = 0 (default tipHot=true).
#   2.  setMicTipRing(false) → byte 50 bit 3 = 1 (tip-is-BIAS, inverted).
#   3.  setMicTipRing(true)  → byte 50 bit 3 = 0 (tip-is-mic, inverted).
#   4.  Round-trip false → true → false.
#   5.  Bits 0 (line_in) and 1 (mic_boost) unaffected — cross-bit guard.
#   6.  Bits 4-7 of byte 50 unaffected.
#   7.  Idempotency: setMicTipRing(false) twice → bit remains 1.
#   8.  Codec path (OrionMKII): mic_trs at byte 50 bit 3 (inverted).
#   9.  Codec path (Saturn/ANAN-G2): same inverted behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setMicTipRing.
# Source: deskhpsdr/src/new_protocol.c:1492-1494 [@120188f].
nereus_add_test(tst_p2_mic_tip_ring_wire)
target_compile_definitions(tst_p2_mic_tip_ring_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.4: P1 mic_bias wire-byte snapshot ──────────────────────
# 14 tests pin the mic_bias bit (C1 bit 5 = 0x20) of bank 11 (C0=0x14)
# in P1RadioConnection::setMicBias().
# Polarity: on=true (bias on) → wire bit 1; on=false (bias off) → wire bit 0.
# Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
#   1.  Default state: C1 bit 5 = 0 (default m_micBias=false).
#   2.  setMicBias(true)  → C1 bit 5 = 1 (bias on → wire bit set).
#   3.  setMicBias(false) → C1 bit 5 = 0 (bias off → wire bit clear).
#   4.  Round-trip: true → false → bit clears after set.
#   5.  Bank 11 C0 address stays 0x14.
#   6.  setMicBias(true) does NOT clobber C1 bits 0-3 (preamp).
#   7.  setMicBias(true) does NOT touch C1 bit 4 (G.3 mic_trs, cross-bit guard).
#   8.  setMicBias(true) produces C1 = 0x20 in default state (only bit 5).
#   9.  setMicBias sets m_forceBank11Next (Codex P2 flush pattern).
#  10.  Repeated same-value call still sets the flush flag (Codex P2).
#  11.  Idempotency: setMicBias(true) twice → bit stays 1.
#  12.  Codec path (Standard): setMicBias(true) → C1 bit 5 set.
#  13.  Codec path (Standard): setMicBias(false) → C1 bit 5 clear.
#  14.  Codec path (HL2): setMicBias(true) → C1 bit 5 set (FW ignores).
# Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13].
nereus_add_test(tst_p1_mic_bias_wire)
target_compile_definitions(tst_p1_mic_bias_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.4: P2 mic_bias wire-byte snapshot ─────────────────────
# 10 tests pin the mic_bias_enabled bit (CmdTx byte 50 bit 4 = 0x10)
# in P2RadioConnection::setMicBias().
# Polarity: on=true → byte-50 bit 4 = 1 (no inversion).
#   1.  Default state: byte 50 bit 4 = 0 (default m_micBias=false).
#   2.  setMicBias(true)  → byte 50 bit 4 = 1 (bias on → wire bit set).
#   3.  setMicBias(false) → byte 50 bit 4 = 0 (bias off → wire bit clear).
#   4.  Round-trip true → false → true.
#   5.  Bits 0 (line_in), 1 (mic_boost), 3 (mic_trs) unaffected — cross-bit guard.
#   6.  Bits 5-7 of byte 50 unaffected.
#   7.  Idempotency: setMicBias(true) twice → bit remains 1.
#   8.  Codec path (OrionMKII): mic_bias at byte 50 bit 4.
#   9.  Codec path (Saturn/ANAN-G2): same behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setMicBias.
# Source: deskhpsdr/src/new_protocol.c:1496-1498 [@120188f].
nereus_add_test(tst_p2_mic_bias_wire)
target_compile_definitions(tst_p2_mic_bias_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.5: P1 mic_ptt wire-byte snapshot ─────────────────────
# 16 tests pin the mic_ptt bit (C1 bit 6 = 0x40, INVERTED) of bank 11 (C0=0x14)
# in P1RadioConnection::setMicPTT().
# POLARITY INVERSION: enabled=true → wire bit CLEAR; enabled=false → wire bit SET.
# Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
#   1.  Default state: C1 bit 6 = 1 (PTT disabled by default — m_micPTT=false → !false=1).
#   2.  setMicPTT(true)  → C1 bit 6 = 0 (PTT enabled → disable-flag cleared).
#   3.  setMicPTT(false) → C1 bit 6 = 1 (PTT disabled → disable-flag set).
#   4.  Round-trip true → false.
#   5.  Bank 11 C0 address must be 0x14.
#   6.  setMicPTT(true) does NOT clobber C1 bits 0-3 (preamp).
#   7.  setMicPTT(true) does NOT touch C1 bits 4 or 5 (G.3/G.4 bits).
#   8.  setMicPTT sets m_forceBank11Next (Codex P2 flush pattern).
#   9.  Flush flag set even when value doesn't change (Codex P2).
#  10.  Idempotency: setMicPTT(true) twice → bit stays 0.
#  11.  Codec path (Standard): setMicPTT(true) → C1 bit 6 clear.
#  12.  Codec path (Standard): setMicPTT(false) → C1 bit 6 set.
#  13.  Codec path (HL2): setMicPTT(true) → C1 bit 6 clear (FW ignores).
#  14.  setMicPTT(true) does NOT touch C1 bits 0-3 — cross-bit guard.
#  15.  G.3+G.4+G.5 composition: mic_trs(inverted)+mic_bias+mic_ptt(inverted) → C1 & 0x70 == 0x70.
#  16.  G.3+G.4 composition sub-case: mic_trs+mic_bias → C1 & 0x30 == 0x30.
# Source: Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13].
nereus_add_test(tst_p1_mic_ptt_wire)
target_compile_definitions(tst_p1_mic_ptt_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.5: P2 mic_ptt wire-byte snapshot ─────────────────────
# 10 tests pin the mic_ptt bit (CmdTx byte 50 bit 2 = 0x04, INVERTED)
# in P2RadioConnection::setMicPTT().
# POLARITY INVERSION: enabled=true → byte-50 bit 2 = 0; enabled=false → = 1.
#   1.  Default state: byte 50 bit 2 = 1 (PTT disabled by default).
#   2.  setMicPTT(true)  → byte 50 bit 2 = 0 (PTT enabled → disable-flag cleared).
#   3.  setMicPTT(false) → byte 50 bit 2 = 1 (PTT disabled → disable-flag set).
#   4.  Round-trip true → false → true.
#   5.  Bits 0 (line_in), 1 (mic_boost) unaffected — cross-bit guard.
#   6.  setMicPTT(true) does NOT touch byte-50 bit 3 (G.3 mic_trs).
#   7.  Idempotency: setMicPTT(true) twice → bit stays 0.
#   8.  Codec path (OrionMKII): mic_ptt at byte 50 bit 2.
#   9.  Codec path (Saturn/ANAN-G2): same behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setMicPTT.
# Source: deskhpsdr/src/new_protocol.c:1488-1490 [@120188f].
nereus_add_test(tst_p2_mic_ptt_wire)
target_compile_definitions(tst_p2_mic_ptt_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Issue #182: model -> connection mic_ptt_disabled wiring ─────────────────
# Coverage:
#   1. Toggling TransmitModel::micPttDisabled = true reaches the connection.
#   2. Toggling back to false also propagates (round-trip).
#   3. Connect-time prime pushes the current model value once.
# Pre-fix: no connect() existed between TransmitModel::micPttDisabledChanged
# and RadioConnection::setMicPTTDisabled, so the Setup checkbox never reached
# the wire on either protocol. Post-fix wires the signal in
# RadioModel::wireConnectionSignals + adds connect-time priming.
# Source cite: Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51].
nereus_add_test(tst_radio_model_mic_ptt_wire)
target_compile_definitions(tst_radio_model_mic_ptt_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.6: P1 mic_xlr storage-only ──────────────────────────
# 8 tests verify that P1RadioConnection::setMicXlr() is STORAGE-ONLY.
# Saturn G2 P2-only feature — P1 hardware has no XLR jack. No wire emission.
# P1 case-10 and case-11 C&C bytes must be UNCHANGED regardless of m_micXlr value.
#   1.  Default state (true): setMicXlr(true) → bank10 unchanged.
#   2.  Default state: setMicXlr(true) → bank11 unchanged.
#   3.  setMicXlr(false) → bank10 all 5 bytes unchanged (critical assertion).
#   4.  setMicXlr(false) → bank11 all 5 bytes unchanged (critical assertion).
#   5.  Round-trip false→true: bank10 unchanged after both calls.
#   6.  Round-trip false→true: bank11 unchanged after both calls.
#   7.  Idempotency: setMicXlr(false) twice → bank11 stable.
#   8.  Codec path (Standard/HermesII): bank11 still unchanged by setMicXlr.
# Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f] (P2 wire — P1 storage-only).
nereus_add_test(tst_p1_mic_xlr_storage)
target_compile_definitions(tst_p1_mic_xlr_storage PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task G.6: P2 mic_xlr wire-byte snapshot ────────────────────
# 10 tests pin the mic_xlr bit (CmdTx byte 50 bit 5 = 0x20, no inversion)
# in P2RadioConnection::setMicXlr().
# POLARITY: xlrJack=true → byte-50 bit 5 = 1; xlrJack=false → = 0. No inversion.
# Default true — Saturn G2 ships with XLR-enabled config.
# MicState::micControl default updated from 0x04 to 0x24 (bit 2 + bit 5 set).
#   1.  Default state: byte 50 bit 5 = 1 (XLR selected by default).
#   2.  setMicXlr(true)  → byte 50 bit 5 = 1 (XLR selected).
#   3.  setMicXlr(false) → byte 50 bit 5 = 0 (XLR deselected).
#   4.  Round-trip false → true.
#   5.  setMicXlr(true) does NOT touch bits 0-4 (G.1-G.5) — cross-bit guard.
#   6.  setMicXlr(false) does NOT touch bits 0-4 — cross-bit guard.
#   7.  Idempotency: setMicXlr(true) twice → bit stays 1.
#   8.  Codec path (OrionMKII): mic_xlr at byte 50 bit 5.
#   9.  Codec path (Saturn/ANAN-G2): same behaviour.
#  10.  Byte 51 (line_in gain) unaffected by setMicXlr.
# Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f].
nereus_add_test(tst_p2_mic_xlr_wire)
target_compile_definitions(tst_p2_mic_xlr_wire PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3M-1b Task I.1: AudioTxInputPage skeleton + TransmitModel::micSource ─
# 12 tests covering:
#   1-2.  HL2 (hasMicJack=false): PC Mic enabled, Radio Mic disabled.
#   2.    HL2: disabled-tooltip exact text "Radio mic jack not present on Hermes Lite 2".
#   3-4.  G2 (hasMicJack=true): both PC Mic and Radio Mic enabled, no tooltip.
#   5.    Default state: PC Mic checked, Radio Mic unchecked.
#   6-7.  UI → Model round-trip: button click drives TransmitModel::micSource.
#   8-9.  Model → UI round-trip: setMicSource() drives radio-button check state.
#  10.    HL2 forced state: Radio Mic button disabled even when model holds Radio.
#  11.    TransmitModel signal emission: setMicSource emits micSourceChanged.
#  12.    TransmitModel idempotency: same-value call emits no signal.
# NEREUS_BUILD_TESTS required for RadioModel::setCapsHasMicJackForTest.
nereus_add_test(tst_audio_tx_input_page_skeleton)
target_compile_definitions(tst_audio_tx_input_page_skeleton PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task I.2 — PC Mic settings group on AudioTxInputPage
# =============================================================================
# Covers:
#   1-3.  Backend combo UI↔model round-trips; device list repopulation.
#   4-6.  Buffer slider: label update, UI↔model round-trips.
#   7-8.  Mic Gain slider UI↔model round-trips.
#   9.    No-feedback-loop: model→UI→model emission count.
#  10.    Test Mic button state machine (checked + timer running / stopped).
#  11-13. TransmitModel idempotency for 3 new PC Mic properties.
#  14-15. PC Mic group box visibility gated by mic-source selection.
# NEREUS_BUILD_TESTS required for RadioModel::setCapsHasMicJackForTest.
nereus_add_test(tst_audio_tx_input_pc_mic_group)
target_compile_definitions(tst_audio_tx_input_pc_mic_group PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task I.3 — Radio Mic settings group with per-family layout
# =============================================================================
# Covers:
#   1.    HL2 (hasMicJack=false): all 3 Radio Mic groups hidden.
#   2-4.  Saturn / OrionMKII / HermesII + Radio Mic: correct group visible.
#   5-6.  PC→Radio Mic and Radio→PC Mic toggle visibility on Saturn.
#   7-8.  Hermes: Line In radio UI↔Model round-trip.
#   9-10. Hermes: +20 dB Mic Boost checkbox UI↔Model round-trip.
#  11-12. Hermes: Line In Gain slider UI↔Model round-trip.
#  13.    Orion: Mic Tip-Ring checkbox UI→Model round-trip.
#  14.    Orion: micPttDisabled model→UI round-trip.
#  15-16. Saturn: 3.5mm/XLR radio UI↔Model round-trip.
#  17.    Saturn: Mic Bias UI→Model round-trip.
#  18.    No feedback loop: micBoostChanged emission count.
# NEREUS_BUILD_TESTS required for RadioModel::setCapsHasMicJackForTest +
#   setCapsHwForTest.
nereus_add_test(tst_audio_tx_input_radio_mic_group)
target_compile_definitions(tst_audio_tx_input_radio_mic_group PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task I.4 — Per-board mic gain slider range on AudioTxInputPage
# =============================================================================
# Covers:
#   1.    Unknown board: slider range = -50/+70 (TransmitModel fallback).
#   2.    Hermes board: slider range = -40/+10 (Thetis runtime defaults).
#   3.    Saturn board: slider range = -40/+10 (Thetis runtime defaults).
#   4.    HermesLite board: slider range = -40/+10 (no mic jack, but caps OK).
#   5.    Slider clamps low: model -50 dB on Hermes → slider at -40.
#   6.    Slider clamps high: model +70 dB on Hermes → slider at +10.
#   7.    UI → model: slider setValue(5) → micGainDb = 5.
#   8.    Model → UI: setMicGainDb(-20) → slider value = -20.
#   9.    TransmitModel clamps above global kMicGainDbMax.
#  10.    No feedback loop: model → UI → no re-emission.
# NEREUS_BUILD_TESTS required for RadioModel::setCapsHwForTest +
#   setCapsHasMicJackForTest.
nereus_add_test(tst_audio_tx_input_mic_gain_mirror)
target_compile_definitions(tst_audio_tx_input_mic_gain_mirror PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task I.5 — Integration: chk20dbMicBoost → VOX threshold scaling
# =============================================================================
# Verifies the full chain:
#   AudioTxInputPage checkbox toggle
#     → TransmitModel::setMicBoost
#     → TransmitModel::micBoostChanged
#     → MoxController::onMicBoostChanged  [H.2 wiring in RadioModel]
#     → MoxController::recomputeVoxThreshold
#     → MoxController::voxThresholdRequested(double)
#
# Test matrix (~7 cases):
#   1.  Default state: NAN sentinel → at least one initial emit.
#   2.  Hermes checkbox ON: voxThresholdRequested fires with scaled value.
#   3.  Hermes checkbox OFF: voxThresholdRequested fires with unscaled value.
#   4.  Model setMicBoost(true) → Hermes UI checkbox reflects change.
#   5.  Idempotency: toggle ON twice → only one additional emit.
#   6.  Orion cross-family: Orion checkbox triggers same recompute chain.
#   7.  Saturn cross-family: Saturn checkbox triggers same recompute chain.
#
# NEREUS_BUILD_TESTS required for RadioModel::setCapsHwForTest +
#   setCapsHasMicJackForTest + moxController() accessor.
nereus_add_test(tst_audio_tx_input_mic_boost_vox_thresh)
target_compile_definitions(tst_audio_tx_input_mic_boost_vox_thresh PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task L.1 — RadioModel owns Pc/Radio mic sources + composite router
# =============================================================================
# Covers:
#   1-3.  Test seams: pcMicSource/radioMicSource/compositeMicRouter null before connect.
#   4.    RadioMicSource (QObject) lives on the main thread after construction.
#   5.    RadioMicSource null-connection: construction + pullSamples = zero-fill.
#   6.    RadioMicSource ring starts empty (ringFillForTest == 0).
#   7.    CompositeTxMicRouter zero-fills when both sources null (edge-case guard).
#   8.    TransmitModel::micPreampChanged signal carries a double linear gain.
#   9.    monEnabledChanged → AudioEngine::setTxMonitorEnabled state round-trip.
#  10.    monitorVolumeChanged → AudioEngine::setTxMonitorVolume state round-trip.
#  11.    MoxCheck lifecycle: install, invoke, clear.
#  12.    MoxCheck rejection: moxRejected emitted + MOX does not advance.
#  13.    MoxCheck approved: no moxRejected, MOX advances.
#  14.    RadioModel lifecycle (no connect): construct + destroy cleanly.
#  15.    RadioModel double lifecycle: two consecutive instances, no leak/crash.
#
# NEREUS_BUILD_TESTS required for RadioModel::pcMicSourceForTest etc. +
#   RadioMicSource::ringFillForTest.
nereus_add_test(tst_radio_model_3m1b_ownership)
target_compile_definitions(tst_radio_model_3m1b_ownership PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1b Task L.3 — HL2 force-Pc on connect (closes Phase L)
# =============================================================================
# Covers:
#   1.  HL2 fresh connect: micSource == Pc.
#   2a. HL2 fresh connect: lock engaged (isMicSourceLocked true).
#   2b. HL2 connect with stored Radio: forced to Pc.
#   2c. HL2 connect with stored Radio: lock engaged.
#   3a. Non-HL2 fresh connect: default Pc.
#   3b. Non-HL2 fresh connect: not locked.
#   4.  Non-HL2 with stored Radio: loads Radio (no override).
#   5.  Lock prevents setMicSource(Radio) on HL2 → stays Pc.
#   6.  Lock released on non-HL2 → setMicSource(Radio) succeeds.
#   7.  isMicSourceLocked() reflects hasMicJack for both boards.
#   8.  Reconnect HL2 → non-HL2: lock released, Radio allowed.
#
# NEREUS_BUILD_TESTS required for RadioModel test seams:
#   setCapsHasMicJackForTest, simulateConnectLoadForTest,
#   simulateDisconnectForTest.
nereus_add_test(tst_radio_model_mic_source_hl2_force)
target_compile_definitions(tst_radio_model_mic_source_hl2_force PRIVATE NEREUS_BUILD_TESTS)

# =============================================================================
# Phase 3M-1c Task H — Setup → Test → Two-Tone IMD page (TestTwoTonePage)
# =============================================================================
# Covers:
#   1.    Constructor doesn't crash with default RadioModel.
#   2.    Initial widget values match TransmitModel two-tone defaults
#         (8 properties: 5 numeric + 2 booleans + DrivePowerSource enum).
#   3-3f. UI → Model round trips (7 numeric / boolean controls).
#   4-4b. Model → UI round trips (QSignalBlocker prevents feedback loops).
#   5.    Defaults preset button → Freq1=700 / Freq2=1900 only (Level/Power
#         unchanged, matching Thetis btnTwoToneF_defaults_Click semantics).
#   6.    Stealth preset button → Freq1=70 / Freq2=190.
#   7.    Invert checkbox initial state checked (twoToneInvert default true).
#   8.    Pulsed checkbox initial state unchecked (twoTonePulsed default false).
#   9.    DrivePowerSource radio button two-way sync
#         (model→UI: Fixed enum selects Fixed radio; UI→model: TuneSlider
#         click sets enum; UI→model: Fixed click sets enum).
nereus_add_test(tst_test_two_tone_page)

# =============================================================================
# Phase 3M-1c Task J — TxApplet TX-Profile combo + 2-TONE button (J.1+J.2)
# =============================================================================
# Covers (TxApplet wiring tests):
#   - Combo accessor present after construction.
#   - Combo populated from MicProfileManager on injection.
#   - User-driven combo change calls setActiveProfile.
#   - profileListChanged refreshes combo entries.
#   - activeProfileChanged updates combo selection programmatically without
#     echo back into setActiveProfile.
#   - Right-click on combo emits TxApplet::txProfileMenuRequested signal
#     (mirrors Thetis comboTXProfile_MouseDown — console.cs:44519-44522
#     [v2.10.3.13]).
#   - 2-TONE button accessor present, checkable, visible.
#   - 2-TONE button tooltip mentions "two-tone" + "Setup".
#   - Click dispatches TwoToneController::setActive(true/false).
#   - twoToneActiveChanged signal mirrors back into the button checked state.
#   - setTwoToneController(nullptr) doesn't crash on click.
nereus_add_test(tst_tx_applet_profile_combo)

# =============================================================================
# Phase 3M-1c Task J — Setup → Audio → TX Profile editor (J.3+J.4)
# =============================================================================
# Covers (TxProfileSetupPage):
#   - Constructor doesn't crash with default RadioModel + injected pointers.
#   - Combo populated from MicProfileManager.
#   - Save button: new name calls saveProfile.
#   - Save button: empty / whitespace name is no-op.
#   - Save button: existing name + overwrite=Yes overwrites.
#   - Save button: existing name + overwrite=No is a no-op.
#   - Delete button: calls deleteProfile and combo shrinks.
#   - Delete button: last-remaining shows verbatim Thetis string and refuses.
#   - J.4: programmatic combo change does NOT trigger unsaved-prompt.
#   - J.4: simulated user combo change DOES trigger prompt when dirty.
#   - J.4: prompt Yes saves before switching.
#   - J.4: prompt No discards before switching.
#   - J.4: prompt Cancel reverts the combo to the original selection.
nereus_add_test(tst_tx_profile_setup_page)

# ── Shell-Chrome Redesign sub-PR-5: RxDashboard ───────────────────────────
# 4 tests: unbound construction, bind, rebind, active-only badge visibility.
nereus_add_test(tst_rx_dashboard)

# ── Shell-Chrome Redesign sub-PR-1: StatusBadge widget ────────────────────
# 8 tests: defaults, setters round-trip, variant style application,
# left/right-click signal emission. Uses QTEST_MAIN (needs QApplication
# for QWidget mouse events).
nereus_add_test(tst_status_badge)

# ── Shell-Chrome Redesign sub-PR-8: MetricLabel ───────────────────────────
# 4 tests: ctor stores label+value, setLabel/setValue round-trip, idempotent.
nereus_add_test(tst_metric_label)

# ── Shell-Chrome Redesign sub-PR-1: StationBlock widget ───────────────────
# 5 tests: default/setRadioName/empty-name appearance switching, click +
# right-click signal emission gated on appearance.
nereus_add_test(tst_station_block)

# ── Shell-Chrome Redesign sub-PR-2: byte-rate counters ────────────────────
# 4 tests: zero-init, in-window accumulation, out-of-window aging, tx/rx
# independence. QTEST_APPLESS_MAIN — no QApplication needed.
nereus_add_test(tst_radio_connection_byte_rates)

# ── Shell-Chrome Redesign sub-PR-2: ping RTT signal ───────────────────────
# 3 tests: emit-on-rtt, duplicate-receive-suppression, receive-without-send.
nereus_add_test(tst_radio_connection_ping_rtt)

# ── Shell-Chrome Redesign sub-PR-2: voltage signals ───────────────────────
# 3 tests: PSU Hermes-DC formula, MKII PA formula, identical-raw suppression.
nereus_add_test(tst_radio_connection_supply_volts)

# ── Shell-Chrome Redesign sub-PR-2: AudioEngine flow state ────────────────
# 5 tests: initial Dead, Healthy on feed, Underrun on single, Stalled on
# persistent, identical-state suppression.
nereus_add_test(tst_audio_engine_flow_state)

# ── Shell-Chrome Redesign sub-PR-3: NetworkDiagnosticsDialog ──────────────
# 4 tests: null-safe construction, refresh on disconnected model, reset
# clears counters, signal-wire safety. QTEST_MAIN — needs QApplication.
nereus_add_test(tst_network_diagnostics_dialog)

# -- Phase 3M-3a-ii follow-up Batch 1: ParametricEqWidget skeleton --
nereus_add_test(tst_parametric_eq_widget_skeleton)

# -- Phase 3M-3a-ii follow-up Batch 2: ParametricEqWidget axis math + ordering --
nereus_add_test(tst_parametric_eq_widget_axis)

# -- Phase 3M-3a-ii follow-up Batch 3: ParametricEqWidget paintEvent + draw helpers --
nereus_add_test(tst_parametric_eq_widget_paint)

# -- Phase 3M-3a-ii follow-up Batch 4: ParametricEqWidget mouse + wheel + signals --
nereus_add_test(tst_parametric_eq_widget_interaction)

# -- Phase 3M-3a-ii follow-up Batch 5: ParametricEqWidget JSON marshal + public API --
nereus_add_test(tst_parametric_eq_widget_json)

# ── Task 1.2: RxChannel captureState/applyState round-trip ───────────────
nereus_add_test(tst_rx_channel_rebuild)

# ── Post-v0.3.2 sample-rate-live crash regression ───────────────────────
# Verifies RxChannel::setSampleRate keeps the same object alive (Thetis
# cmaster.c::SetXcmInrate pattern, NOT destroy-and-recreate).
nereus_add_test(tst_set_sample_rate_live)

# ── Task 1.4: TxChannel captureState/applyState + WdspEngine::rebuildTxChannel
nereus_add_test(tst_tx_channel_rebuild)

# ── Task 1.5: RxChannel::filterResponseMagnitudes() accessor ─────────────
# Verifies: (a) correct nPoints output size, (b) passband > stopband by ≥ 10 dB,
# (c) nPoints ≤ 0 returns empty.  Guarded with HAVE_WDSP + HAVE_FFTW3.
nereus_add_test(tst_filter_high_resolution)

# ── Task 4.4: FilterDisplayItem high-resolution mode ─────────────────────
# Verifies: setHighResolution/highResolution round-trip, idempotency,
# bindRxChannel(nullptr) safety, paint-with-nullptr-channel no-crash,
# bind-and-clear lifecycle.
nereus_add_test(tst_filter_display_high_resolution)

# ── Task 1.6: RadioModel::setSampleRateLive() coordinator ────────────────
# Guard-path contracts (no live WDSP or connection required):
#   changes_rate_without_dropping_channel — unconnected → returns -1,
#     connectionSampleRateHz() stays at 0.
#   idempotent_same_rate_returns_zero — same rate as current → returns 0.
#   rebuild_completes_under_glitch_budget — elapsed < 200 ms (guard path
#     returns immediately; full timing verified on bench in Task 1.8).
#   signal_not_emitted_when_guard_fires — wireSampleRateChanged not emitted.
#   signal_not_emitted_on_same_rate — wireSampleRateChanged not emitted.
nereus_add_test(tst_sample_rate_live_apply)

# ── Task 1.7: RadioModel::setActiveRxCountLive() coordinator ─────────────
# Guard-path contracts (no live WDSP or connection required):
#   not_connected_returns_minus_one — disconnected → returns -1,
#     connectionActiveRxCount() stays at 0.
#   idempotent_same_count_returns_zero — same count as current → returns 0.
#   rebuild_completes_under_glitch_budget — elapsed < 200 ms (guard path
#     returns immediately; full timing verified on bench in Task 1.8).
#   signal_not_emitted_when_guard_fires — activeRxCountChanged not emitted.
#   signal_not_emitted_on_same_count — activeRxCountChanged not emitted.
nereus_add_test(tst_active_rx_count_live_apply)

# Task 1.8: dspChangeMeasured signal existence and emission test.
# Tests that RadioModel::dspChangeMeasured(qint64) signal exists
# and can be spied on. Actual emission is tested on live hardware;
# these tests verify the signal protocol.
nereus_add_test(tst_dsp_change_timer)

# ── Task 4.2: Per-mode buffer/filter/filter-type live-apply ─────────────────
# Tests RxChannel::onModeChanged() and TxChannel::onModeChanged() guard paths
# (no WDSP required — verifies the equality guard, key-part routing, and the
# rebuild call path). Also verifies RadioModel::rebuildDspOptionsForMode()
# no-ops when disconnected.
#
# Test cases:
#   rx_no_engine_returns_zero — no WdspEngine attached → 0
#   rx_engine_attached_channel_not_in_map_returns_minus_one — engine attached,
#     channel not created → rebuild returns -1
#   rx_same_settings_returns_zero_no_rebuild — bufSize/filtSize/filtType all
#     match AppSettings → equality guard fires → 0
#   rx_cw_mode_reads_cw_key_suffix — CWU/CWL use "Cw" key suffix → 0
#   rx_dig_mode_reads_dig_key_suffix — DIGU/DIGL use "Dig" key suffix → 0
#   rx_fm_mode_reads_fm_key_suffix — FM uses "Fm" key suffix → 0
#   rx_changed_filter_size_triggers_rebuild_attempt — 8192!=4096 → -1
#   rx_changed_filter_type_triggers_rebuild_attempt — LinearPhase!=LowLatency → -1
#   tx_no_engine_returns_zero — TxChannel without engine → 0
#   tx_same_settings_returns_zero — both values match → 0
#   tx_changed_filter_size_triggers_rebuild_attempt — 4096!=2048 → -1
#   radiomodel_rebuild_noop_when_disconnected — no emission when disconnected
nereus_add_test(tst_dsp_options_per_mode_apply)

# ── Task 4.3: Filter Impulse Cache toggle persistence ─────────────────────────
# AppSettings round-trip for DspOptionsCacheImpulse and
# DspOptionsCacheImpulseSaveRestore. Both default to "False". The WdspEngine
# reads these flags at finishInitialization() and shutdown(); they take effect
# at engine-init time, not live (see design Section 4C).
#
# Tests:
#   cache_impulse_default_false — remove key → default is "False"
#   cache_impulse_round_trip_true — set "True" → reads back "True"
#   cache_impulse_round_trip_false — set "False" → reads back "False"
#   cache_save_restore_default_false — remove key → default is "False"
#   cache_save_restore_round_trip_true — set "True" → reads back "True"
#   cache_save_restore_round_trip_false — set "False" → reads back "False"
#   cache_impulse_and_save_restore_are_independent — orthogonal flags
nereus_add_test(tst_dsp_options_persistence)

# ── Task 4.5: Warning icon logic — Thetis validity rules ──────────────────────
# Ported from Thetis console.cs:38797-38807 + setup.cs:22391-22400 [v2.10.3.13].
#
# Validity rules: warnings show when settings differ across modes:
#   bufferSizeDifferentRX  = !(phone==fm && fm==cw && cw==dig)  (4-way RX)
#   bufferSizeDifferentTX  = !(phone==fm && fm==dig)            (3-way TX; no CW TX)
#   (same pattern for filter size; filter type uses separate RX/TX type combos)
#   pbWarningBufferSize.Visible = bufferSizeDifferentRX || bufferSizeDifferentTX
#   pbWarningFilterSize.Visible = filterSizeDifferentRX || filterSizeDifferentTX
#   pbWarningBufferType.Visible = filterTypeDifferentRX || filterTypeDifferentTX
#
# Tests the static helpers DspOptionsPage::comboValuesDiffer4 / comboValuesDiffer3
# and the composite OR logic for each warning label.
#
#   differ4_all_equal_returns_false — 4 equal combos → false
#   differ4_one_different_returns_true — 1 of 4 differs → true
#   differ4_two_different_returns_true — 2 of 4 differ → true
#   differ4_all_different_returns_true — all 4 differ → true
#   differ3_all_equal_returns_false — 3 equal combos → false
#   differ3_one_different_returns_true — 1 of 3 differs → true
#   differ3_two_different_returns_true — 2 of 3 differ → true
#   buffer_warn_fires_on_rx_side_only — RX side differs, TX uniform → warning
#   buffer_warn_fires_on_tx_side_only — FM differs → both sides diverge → warning
#   buffer_warn_hidden_when_all_match — all 4 equal → no warning
#   filter_size_same_logic_as_buffer — all equal: no warn; CW changed: warn
#   filter_type_rx_3_of_4_equal_fires — 3 of 4 RX types equal, 1 differs → warn
#   filter_type_all_rx_equal_no_warning — all RX equal AND all TX equal → no warn
nereus_add_test(tst_dsp_options_warning_logic)

# ── Task 2.1: Detector + Averaging split (handwave fix from 3G-8) ─────────
# Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13] DetTypePan / DetTypeWF
# / AverageMode / AverageModeWF integer codes.
#
# tst_detector_modes: pure math over SpectrumWidget::applyDetector().
#   Peak returns max in window; Rosenfell alternates max/min per pixel;
#   Average computes arithmetic mean; Sample takes first bin;
#   RMS computes root-mean-square; empty input / zero outputBins are no-ops.
#   Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free).
#
# Note: tst_averaging_modes was retired with the SpectrumWidget::applyAveraging
# static helper.  Frame averaging is now performed by SpectrumAvenger
# (verbatim WDSP analyzer.c:464-554 [v2.10.3.13] port) on linear-power
# display pixels; coverage of that path is exercised end-to-end via the
# running app.  Direct SpectrumAvenger unit tests are a follow-up item.
nereus_add_test(tst_detector_modes)

# Phase 1B verification -- exhaustive sweep of every detector x averaging
# combination through SpectrumWidget::updateSpectrumLinear():
#   spectrum  5 detectors x 4 averagings = 20
#   waterfall 4 detectors x 4 averagings = 16
#   total                                = 36
# Plus mode-switch / averaging-switch / empty-input edge cases.
# Mirrors the Thetis Pan / WF combo item lists from
# setup.designer.cs:34843-34847 / :34868-34873 / :34436-34440 / :34461-34465
# [v2.10.3.13].  Asserts pipeline output is sized to displayWidth and all
# pixels finite (no NaN / Inf from any detector x averaging combo).
nereus_add_test(tst_spectrum_pipeline_modes)

# Phase 2 -- FFTEngine window function ports (Rectangular / BH-4T / Hann /
# Flat-Top / Hamming / Kaiser / BH-7T) per WDSP analyzer.c:52-173 +
# Thetis combo item ordering (setup.designer.cs:34966-34973 [v2.10.3.13]).
# Verifies enum values match WDSP wire format, ENB falls in the published
# range for each window, Kaiser PiAlpha setter clamps non-positive input.
nereus_add_test(tst_window_functions)

# Phase 2-polish -- FFT size slider semantics.  Verifies slider 0..6 maps
# to {4096..262144} per Thetis setup.cs:16148 [v2.10.3.13]; fftSizeOffsetDb
# round-trips per setup.cs:16154 (slider.Value * 2); spectrumSettingsChanged
# fires only on actual size change per setup.cs:16162-16165; binWidthHz()
# returns plain sampleRate / fftSize (legacy * 2 bug removed).
nereus_add_test(tst_fft_size_slider)

# Phase 2-polish-3 -- auto-zoom FFT math.  NereusSDR-original (Thetis does
# not auto-replan on zoom).  Verifies setFftSizeBaseline round-trip +
# rejection rules, and exercises the bandwidthChangeRequested lambda's
# formula via an inline copy: targetSize = baseline * sampleRate / bwHz,
# floored at baseline, capped at 262144, with [0.66, 1.5] hysteresis
# bracket.
nereus_add_test(tst_auto_zoom_fft)

# ── Task 2.5: ActivePeakHoldTrace ────────────────────────────────────────────
# Unit tests for the per-bin peak-hold trace:
#   peak_value_persists_until_decay: hold + 0.2 dB/frame decay verified
#   on_tx_state_disables_update: onTx=false freezes peaks during TX
#   on_tx_state_allows_update_when_onTx_true: onTx=true keeps updating
#   clear_resets_peaks: all bins back to -inf
#   resize_updates_size_and_clears: size changes + implicit clear
#   disabled_does_not_update_or_decay: no-op when !enabled
#   Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free, pure data logic).
nereus_add_test(tst_active_peak_hold)

# ── Task 2.6: PeakBlobDetector — top-N peak detector with hold/decay ────────
# Verifies: default construction, disabled clears blobs, empty bins, top-N
# selection + sort, count-clamp, inside-filter constraint, hold merge,
# post-hold decay, hold-disabled path, and the Thetis verbatim decay formula
# (entry.max_dBm -= m_dBmPerSecondPeakBlobFall / (float)m_nFps;
#  Display.cs:5483 [v2.10.3.13]).
# Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free, pure data logic).
nereus_add_test(tst_peak_blob_detector)

# ── Task 2.3: Spectrum Defaults overlay group + decimation wire-up ──────────
# Tests for SpectrumWidget overlay setters (ShowMHzOnCursor / ShowBinWidth /
# ShowNoiseFloor / DispNormalize / ShowPeakValueOverlay) and FFTEngine
# decimation (setDecimation range clamp + roundtrip).
#
# tst_spectrum_overlays: accessor roundtrips + default-value asserts.
#   formatCursorFreq Hz vs MHz format verified for on/off states.
#   PeakTextDelay default=500ms from Thetis console.cs:20073 [v2.10.3.13].
#   PeakValueColor default DodgerBlue (#1E90FF) from console.cs:20278 [v2.10.3.13].
#   FFTEngine::setDecimation range [1..32] enforced; out-of-range rejected.
#   Uses QTEST_MAIN (QApplication required for SpectrumWidget construction).
nereus_add_test(tst_spectrum_overlays)

# ── Task 3.1: MultimeterPage + configurable MeterPoller timing ───────────────
# Verifies new setIntervalMs/intervalMs and setAverageWindow/averageWindow
# round-trip accessors on MeterPoller (added for MultimeterPage live wire-up).
#   poll_interval_round_trips: setIntervalMs(50) → intervalMs() == 50
#   poll_interval_default_is_100ms: default matches Thetis 100ms [v2.10.3.13]
#   interval_clamped_below_minimum: 0 → clamped to ≥ 10ms
#   interval_clamped_above_maximum: 99999 → clamped to ≤ 2000ms
#   interval_boundary_10ms/2000ms_accepted: exact boundary values accepted
#   average_window_round_trips: setAverageWindow(8) → averageWindow() == 8
#   average_window_default_is_1: default 1 (no averaging) [v2.10.3.13]
#   average_window clamped [1..32] + boundary tests
#   interval_and_window_are_independent: both setters coexist without clobbering
#   Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free, pure accessor logic).
nereus_add_test(tst_multimeter_timing)

# ── Task 3.2: MeterItem unit-mode fan-out (S / dBm / µV) ────────────────────
# formatValue() static helper for all three units.
# S-meter boundary values: S0 = -127 dBm, S9 = -73 dBm, S9+30 = -43 dBm.
# µV formula: V = sqrt(P * 50 Ω), P = 10^(dBm/10) * 1e-3 W.  -73 dBm ≈ 50 µV.
# setUnitMode / unitMode / setShowDecimal / showDecimal round-trips.
# SignalTextItem::setUnitMode() overrides base to sync per-item Units enum.
# Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free, pure data logic).
nereus_add_test(tst_multimeter_unit_conversion)

# HistoryGraphItem duration setting (Task 3.3).
# setDurationMs / durationMs round-trips, serialization v1/v2, capacity computation,
# AppSettings persistence of MultimeterSignalHistoryDurationMs + Enable flag.
# Uses QTEST_APPLESS_MAIN (no QApplication, WDSP-free, pure MeterItem logic).
nereus_add_test(tst_signal_history)

# -- Phase 3M-3a-iii Task 13: DexpPeakMeter widget (VOX/DEXP live peak strip) --
# 5 tests: default-construct paint safety, signal-level clamp to [0,1], green
# peak fill, red threshold marker, red above-threshold overlay (mirrors Thetis
# picVOX_Paint:28958-28959 [v2.10.3.13] visual recipe).
nereus_add_test(tst_dexp_peak_meter)

# -- Phase 3M-3a-iii Task 15: PhoneCwApplet DEXP (#11) wiring --
# (VOX wiring tests moved to tst_tx_applet_vox_relocated 2026-05-04 with
#  the relocation of the VOX surface to TxApplet.)
# 7 tests: DEXP [ON] bidirectional with TransmitModel, DEXP Th decorative-
# only (Thetis quirk console.cs:28962-28970 [v2.10.3.13]), right-click on
# DEXP [ON] emits openSetupRequested("Transmit","DEXP/VOX"), 100 ms timer
# cadence (matches Thetis UpdateNoiseGate Task.Delay(100)), DexpPeakMeter
# strip constructed, NyiOverlay markers cleared, VOX widgets absent.
nereus_add_test(tst_phone_cw_applet_dexp_wiring)

# -- Phase 3M-3a-iii bench polish 2026-05-04: TxApplet VOX row relocation --
# 7 tests: VOX [ON]/Th/Hold bidirectional with TransmitModel on the new
# TxApplet location (under TUNE/MOX), right-click on VOX emits
# openSetupRequested("Transmit","DEXP/VOX"), DexpPeakMeter constructed,
# 100 ms QTimer drives pollVoxMeter(), and PhoneCw* VOX object names
# absent from TxApplet (one-way relocation pin).
nereus_add_test(tst_tx_applet_vox_relocated)

# -- Issue #175 Task 7: TxApplet HL2 power-slider rescale + dB labels --
# 7 tests: HL2 RF/Tune slider Max+step rescale (mi0bot
# console.cs:2098-2108 [v2.10.3.13-beta2]), SKU swap-back to canonical
# 0..100 step 1, HL2 RF dB label formula (round(drv/6.0)/2)-7.5 at
# slider=60 and slider=90 (mi0bot console.cs:29245-29274), non-HL2
# bare-integer label, Thetis-faithful tooltip wording.
nereus_add_test(tst_tx_applet_hl2_slider)

# -- UI Polish Foundation A1: AppletPanelWidget scrollbar gutter --
nereus_add_test(tst_applet_panel_gutter)

# ── UI Polish Foundation B3: AntennaPopupBuilder capability-gated popup factory ──
# 5 tests cover Tier 1 (full Alex + bypass), Tier 2 (mid Alex, no bypass),
# Tier 3 (no Alex, at most 1 entry), TX mode (no RX-only section), and
# labels() round-trip. Uses actual BoardCapabilities + SkuUiProfile flags.
# Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B3.
nereus_add_test(tst_antenna_popup_builder)

# ── UI Polish Foundation B6: XIT offset application in RadioModel ────────────
# TDD: verifies that wireSliceSignals() applies xitHz to setTxFrequency,
# mirroring the Thetis chkXIT TX NCO shift pattern.
# 5 tests: disabled XIT no shift, enabled+Hz shift, enable/disable toggle,
# negative offset subtracts, zero offset sends base freq.
# Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B6.
nereus_add_test(tst_xit_offset_application)
target_compile_definitions(tst_xit_offset_application PRIVATE NEREUS_BUILD_TESTS)

# ── UI Polish Foundation B8 Task 20: SpectrumOverlayPanel Display-flyout signals ──
# TDD: verifies wfColorGainChanged / wfBlackLevelChanged / colorSchemeChanged
# emit correctly from Display-flyout controls.  Manual-connect tests verify
# the lambda wiring pattern MainWindow uses.
# Per docs/superpowers/plans/2026-05-01-ui-polish-cross-surface.md §B8.
nereus_add_test(tst_b8_overlay_signals)

# ── Plan 4 Task 2 (D1): FilterLow/FilterHigh schema + MicProfile persistence ─
# 3 tests: filter values persist across profile save/switch/activate cycle,
# activating a profile pushes filter values into TransmitModel (filterChanged),
# isActiveProfileModified detects drift and clears after saveActiveProfile.
# Per docs/superpowers/plans/2026-05-01-tx-bandwidth.md §Task 2.
nereus_add_test(tst_tx_bandwidth_persistence)

# ── Plan 4 Task 6 (D8): TX filter debounce + per-mode IQ-space mapping ────────
# 9 tests: debounce coalesces rapid calls, USB/LSB/AM/DIGU/DIGL/FM/DSB/CWL
# per-mode IQ-space sign convention verified via QSignalSpy on txFilterApplied.
# Per docs/superpowers/plans/2026-05-01-tx-bandwidth.md §Task 6.
nereus_add_test(tst_tx_filter_offset_to_wdsp)

# ── Plan 4 Task 7+8 (D9): TX filter overlay palette + paint ──────────────────
# 6 tests: setTxFilterRange stores values, no-op on same values, paint signal
# fires after show, USB/LSB/AM per-mode IQ-space coordinate mapping verified.
# Per docs/superpowers/plans/2026-05-01-tx-bandwidth.md §Task 7/8.
nereus_add_test(tst_spectrum_tx_overlay)

# ── Stage C2: FilterPresetStore user-customisable presets ────────────────────
# 5 tests: defaults match SliceModel::presetsForMode verbatim, setPreset
# round-trips through AppSettings, resetPreset restores Thetis default,
# setPresetsForMode reorder persists, presetsChanged fires on every mutator.
nereus_add_test(tst_filter_preset_store)

# ── Issue #175 Task 1: per-SKU PA UI constants for HL2 mi0bot parity ─────────
# 5 tests asserting the 9 constexpr helpers + 2 attenuator constants added
# in HpsdrModel.h for HL2 (HERMESLITE) mi0bot-Thetis slider/spinbox parity:
#   hl2_rfPowerSlider, hl2_tuneSlider, hl2_fixedTuneSpinbox, hl2_attenuator,
#   anan100_default_100W, ananG2_1k_default_100W_slider.
# Source: mi0bot-Thetis console.cs:2101-2108 + setup.cs:20328-20331 +
#         console.cs:29245-29274 [v2.10.3.13-beta2].
nereus_add_test(tst_hpsdr_model_pa_constants)

# ── Issue #202 deep-fix: defaultModelForBoard auto-pick regression ──────────
# Pins the OrionMKII boardtype → HPSDRModel::ANAN7000D mapping (not the bare
# ORIONMKII enum, which Thetis excludes from its user-facing combo per
# setup.Designer.cs:8515-8528 [v2.10.3.13]).  Plus coverage for the other
# board → model auto-defaults so future enum reordering can't silently
# regress the auto-detect path.
nereus_add_test(tst_default_model_for_board)

# ── Issue #175 Task 2: TxChannel::setPostGenToneMag WDSP wrapper ──────────────
# 2 tests asserting round-trip store-and-retrieve for the thin C++ wrapper
# around SetTXAPostGenToneMag (third_party/wdsp/src/gen.c:800-805 [v2.10.3.13]).
# Used by Task 4 (TransmitModel::setPowerUsingTargetDbm) for HL2 sub-step
# DSP audio-gain modulation.
# Source: mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2].
nereus_add_test(tst_tx_channel_post_gen_tone_mag)

# ── Issue #175 Task 3: TransmitModel::txPostGenToneMag property + signal ──────
# 4 tests: default value (1.0 non-HL2 sentinel), round-trip setter+getter,
# signal emission, and dedupe-on-equal guard (NereusSDR setter convention).
# Wired to TxChannel::setPostGenToneMag in Task 10.
# Source: mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2].
nereus_add_test(tst_transmit_model_tx_post_gen_tone_mag)

# ── Issue #175 Task 4: HL2 sub-step DSP modulation in setPowerUsingTargetDbm ──
# 6 tests asserting the txMode 1 TUNE_SLIDER HL2 branch:
#   slider 0/25/51 -> sub-step (mag 0.40/0.65/0.91, new_pwr 0);
#   slider 52/99   -> attenuator (mag 0.9999, new_pwr -4/90);
#   non-HL2 model  -> pass-through (mag unchanged, new_pwr equals slider).
# Source: mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2].
nereus_add_test(tst_transmit_model_hl2_dbm)

# ── Issue #175 Task 5: HL2 audio-volume formula in computeAudioVolume ─────────
# 5 tests covering the mi0bot HL2 branch ((hl2Power * gbb/100) / 93.75)
# placed BEFORE the gbb >= 99.5 sentinel so HL2 HF bands actually use the
# formula instead of falling into the legacy linear path:
#   §1 HL2 40m (gbb=100) at full   -> clamps to 1.0
#   §2 HL2 40m (gbb=100) at zero   -> 0.0 (sliderWatts<=0 short-circuit)
#   §3 HL2 40m (gbb=100) at mid    -> 50/93.75 exactly
#   §4 HL2 6m  (gbb=38.8) at full  -> (99 * 0.388) / 93.75
#   §5 ANAN-100 model on a Bypass profile -> legacy linear fallback (0.5),
#                                           differs from the HL2 formula value.
# Source: mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2].
nereus_add_test(tst_transmit_model_hl2_audio_volume)

# ── Phase 3M-4 Task 12: two-tone IMD overlay ────────────────────────────────
# 12 tests covering the ImdOverlay peak-detection state machine + IMD3/IMD5
# labeling + EMA smoothing + readout text formatting.  Pure C++ math; no
# Qt-widget instantiation, no PureSignal coordinator dependency.
# Source: Thetis display.cs:5008/5210-5316/5453-5475/5512-5685/5725-5760
#         [v2.10.3.13].
nereus_add_test(tst_imd_overlay)

# ── v0.4.1 hotfix: RadioModel pushes hardware HPSDRModel to ReceiverManager ──
# 5 tests pinning the contract that RadioModel propagates m_hardwareProfile.
# model into BOTH TransmitModel and ReceiverManager. The ReceiverManager
# push was missing in v0.4.0 → P1CodecStandard saw the default HPSDR enum
# value, fell through its model switch's default branch, and emitted an
# empty PsDdcConfig — keeping PsccPump inactive on every Hermes / ANAN-10 /
# ANAN-10E / ANAN-100 / ANAN-100B / AnvelinaPro3-on-P1. HL2 / G2 / Saturn
# were unaffected because their codecs ignore the model parameter.
nereus_add_test(tst_radio_model_hpsdr_model_push)
target_compile_definitions(tst_radio_model_hpsdr_model_push PRIVATE NEREUS_BUILD_TESTS)

# ── v0.4.1 hotfix: setAttenuator + setPreamp bank-11 flush-flag parity ───────
# 7 tests pinning the Codex P2 flush-flag pattern on the two bank-11 setters
# that v0.4.0 missed (peer setters setMicTipRing / setMicBoost / setLineIn /
# setUserDigOut / setMicPTTDisabled / setPuresignalRun all set
# m_forceBank11Next = true before their idempotent guards). Without the
# flush flag, RX step-att and preamp changes wait up to ~22 ms for the next
# natural bank-11 visit in the round-robin instead of landing within ≤2.6 ms.
nereus_add_test(tst_p1_step_att_flush_flag)
target_compile_definitions(tst_p1_step_att_flush_flag PRIVATE NEREUS_BUILD_TESTS)

# ── v0.4.1 hotfix: StepAttenuatorController cross-thread connection pushes ───
# 2 tests pinning the contract that controller.setAttenuation() and
# controller.setPreampMode() marshal to the connection thread via
# QMetaObject::invokeMethod, NOT direct method calls. Direct cross-thread
# slot calls bypass Qt's queued dispatch, write connection-thread-owned
# state from the wrong thread, and (on weaker memory models like ARM64)
# can delay or pair the write with stale reads. The other invokeMethod
# call sites in StepAttenuatorController (setTxStepAttenuation paths at
# lines 364/469/535/557) already follow this pattern; this test pins
# parity for the RX-side setters that v0.4.0 missed.
nereus_add_test(tst_step_att_cross_thread)
target_compile_definitions(tst_step_att_cross_thread PRIVATE NEREUS_BUILD_TESTS)

# ── v0.4.1 hotfix: P1CodecStandard bank 16 ps_run wire (smoking gun) ─────────
# 5 tests pinning that P1CodecStandard's bank 16 emits the puresignal_run bit
# on C2 bit 6 — the same fix that landed in P1CodecHl2 on 2026-05-07 (PR #212
# follow-up bench-fix) but never propagated to P1CodecStandard. Without this
# bit, the radio FPGA fires the cmaster.cs bookkeeping but the PA-loopback
# physical path stays disengaged on Hermes-class boards (Hermes / ANAN-10 /
# ANAN-10E / ANAN-100 / ANAN-100B), causing calcc to reach LCOLLECT but never
# see usable feedback samples. Verified against bench bundle from a friend's
# ANAN-10E running v0.4.1-rc1: TX envelope 0.20 (-14 dBFS) but FB envelope
# 0.0003 (-70 dBFS, ADC noise floor) for 1361 pumped blocks straight.
# Thetis source: networkproto1.c:657-666 [v2.10.3.13] case 16.
nereus_add_test(tst_p1_bank16_pursignal_run)
target_compile_definitions(tst_p1_bank16_pursignal_run PRIVATE NEREUS_BUILD_TESTS)

# ── Phase 3J-2 Task B1: SpotCollectorClient DX-line parser ──────────────────
# Pins the contract that SpotCollectorClient parses the standard DXLab
# "DX de" UDP line format and tags RBN-suffixed spotters (-# suffix) as
# source="RBN". 3 tests: parsesStandardDxLine, parsesRbnSuffixedSpotter,
# rejectsMalformedLine. Source: AetherSDR
# src/core/SpotCollectorClient.cpp [@0cd4559].
nereus_add_test(tst_spot_collector_parser)

# ── Phase 3J-2 Task B2: PotaClient JSON parser + dedup ──────────────────────
# Pins the contract that PotaClient parses api.pota.app/spot/activator
# JSON responses and dedups across consecutive polls via spotId. 3 tests:
# parsesPotaJsonResponse, dedupsAcrossPolls, rejectsMalformedJson.
# Source: AetherSDR src/core/PotaClient.cpp [@0cd4559].
nereus_add_test(tst_pota_client)

# ── Phase 3J-2 Task B3: DxClusterClient telnet parser ───────────────────────
# Pins the contract that DxClusterClient parses the standard DX cluster
# "DX de" telnet line format, detects multi-flavor login prompts
# (login: / call: / callsign: / "Please enter your call" / "your call"),
# strips telnet IAC bytes, and tags RBN-suffixed spotters as source="RBN".
# 7 tests: parsesStandardDxLine, detectsLoginPrompts (data-driven, 8 rows),
# stripsTelnetIAC, tagsRbnSpottersByPrefix, tagsRbnSpottersBySuffix,
# rejectsMalformedLine. Source: AetherSDR src/core/DxClusterClient.cpp
# [@0cd4559].
nereus_add_test(tst_dx_cluster_parser)

# ── Phase 3J-2 Task B4: WsjtxClient binary protocol parser ──────────────────
# Pins the contract that WsjtxClient parses big-endian QDataStream Status
# (type 1) + Decode (type 2) messages per the WSJT-X UDP protocol,
# dispatches the right callsign from each WSJT-X message family
# (CQ / CQ DX / CQ POTA / CQ continent / directed +report / R-report /
# RR73), and computes spot frequency as dial + delta. 6 tests:
# parsesStatusUpdatesDialFreq, parsesDecodeEmitsSpot, skipsNonNewDecodes,
# skipsLowConfidence, extractsCallsign (data-driven, 7 rows). Source:
# AetherSDR src/core/WsjtxClient.cpp [@0cd4559].
nereus_add_test(tst_wsjtx_decoder)

# ── Phase 3J-2 Task B5: FreeDVReporterClient Engine.IO/Socket.IO parser ─────
# Pins the contract that FreeDVReporterClient implements the Engine.IO v4 /
# Socket.IO v4 framing used by qso.freedv.org. 7 tests cover the Engine.IO
# Open handshake + view-only auth payload, Ping/Pong reply, Socket.IO
# Connect ACK transitioning to connected, and the four data events that
# drive the station model and (via NereusSDR dual-feed) the spot stream:
# new_connection -> stationAdded, freq_change -> stationUpdated +
# spotReceived, rx_report -> spotReceived, remove_connection ->
# stationRemoved. Hybrid source: wire protocol from freedv-gui
# src/reporting/FreeDVReporter.cpp [@77e793a]; Qt6 structural pattern from
# AetherSDR src/core/FreeDvClient.cpp [@0cd4559].
nereus_add_test(tst_freedv_reporter_socketio)

# ── Phase 3J-2 Task B6: PskReporterClient IPFIX protocol parser ────────────
# Pins the contract that PskReporterClient implements PSK Reporter's
# IPFIX (RFC 5101) v0.1 framing used by report.pskreporter.info:
# 16-byte IPFIX header (version=10, length, exportTime, sequenceNumber,
# observationDomain), 36-byte RX template, 52-byte TX template, 4-byte
# aligned RX/TX data sets, length-prefixed strings, 5-byte BE frequency,
# signed SNR byte. 7 tests: header layout, RX template+data set,
# sender-record size formula from upstream, sender-record field
# encoding (callsign / freq / SNR / mode / infoSource / flowTime),
# datagram-with-sender layout, parser round-trip emits spotReceived,
# setIdentity affects outgoing packets. Source: freedv-gui
# src/reporting/pskreporter.cpp [@77e793a].
nereus_add_test(tst_psk_reporter_protocol)

# ── Phase 3J-2 Task C1: CtyDatParser (AD1C / K1EA cty.dat lookup) ──────────
# Pins the contract that CtyDatParser loads the AD1C cty.dat country
# file at the repo root, builds a longest-prefix table + exact-match
# table, and resolves callsigns to DXCC entities (primary prefix +
# entity name + continent + CQ zone + ITU zone). 6 tests:
#   loadsRealCtyDat, resolvesUSACallsign (W1AW -> K / United States),
#   resolvesJapanCallsign (JA1ABC -> JA / Japan),
#   resolvesAustraliaCallsign (VK6APH -> VK / Australia),
#   resolvesEnglandCallsign (G3OCA -> G / England),
#   rejectsEmptyOrInvalidCallsign. Source: AetherSDR
# src/core/CtyDatParser.{h,cpp} [@0cd4559].
nereus_add_test(tst_cty_dat_parser)

# ── Phase 3J-2 Task C2: AdifParser (.adi / .adif log parser) ───────────────
# Pins the contract that AdifParser parses ADIF (Amateur Data
# Interchange Format) log files into QVector<QsoRecord> (callsign
# + band + modeGroup). Five tests:
#   parsesSampleAdif (10 QSO bundled fixture, first/last callsigns
#   resolve, SSB -> PHONE / CW -> CW group normalisation),
#   emitsFinishedSignal (QSignalSpy waits parseFileAsync ->
#   finished(QVector<QsoRecord>)),
#   skipsHeaderSection (no <adif_ver> / <programid> leakage),
#   handlesCaseInsensitiveTags (mixed-case <CALL> / <Call> /
#   <call> + <EOR> / <eor> / <Eor> all parse),
#   rejectsMalformedAdif (garbage bytes -> empty vector).
# Source: AetherSDR src/core/AdifParser.{h,cpp} [@0cd4559].
nereus_add_test(tst_adif_parser)

# ── Phase 3J-2 Task C3: DxccWorkedStatus (worked-status tracker) ───────────
# Pins the contract that DxccWorkedStatus tracks per-entity /
# per-band / per-modeGroup worked status from QsoRecord rows
# (typically produced by AdifParser). Six tests:
#   newPrefixIsNewDxcc (unknown primary prefix -> NewDxcc),
#   sameEntitySameBandSameModeIsWorked (already-worked combo ->
#   Worked), sameEntityNewBandIsNewBand (known entity, fresh band
#   -> NewBand), sameEntityNewModeIsNewMode (known entity + band,
#   fresh modeGroup -> NewMode), clearResetsAll (clear()
#   resets totalQsos / entityCount / query path),
#   totalQsoCounting (load() accumulates totalQsos across input
#   rows; entityCount reflects unique-entity tally). Source:
#   AetherSDR src/core/DxccWorkedStatus.{h,cpp} [@0cd4559].
nereus_add_test(tst_dxcc_worked_status)

# ── Phase 3J-2 Task C4: DxccColorProvider (DXCC stack integrator) ──────────
# Pins the contract that DxccColorProvider combines CtyDatParser +
# DxccWorkedStatus + AdifParser to resolve a (callsign, freqMhz,
# mode) spot to one of 4 configurable colors plus a default
# unknown sentinel. Six tests:
#   initialStateReturnsDefaultColor (no cty.dat load -> Unknown
#   -> invalid QColor), loadsCtyDatAndAdif (importFinished
#   emitted with qsoCount=10, entityCount=5 against the bundled
#   sample fixture), resolvesNewDxccColor (CT1ABC / Portugal not
#   in fixture -> colorNewDxcc bright red), resolvesNewBandColor
#   (W1AW on 15m -> colorNewBand orange), resolvesNewModeColor
#   (W1AW on 40m CW -> colorNewMode gold), resolvesWorkedColor
#   (W1AW on 20m PHONE -> colorWorked dim grey). Source:
#   AetherSDR src/core/DxccColorProvider.{h,cpp} [@0cd4559].
nereus_add_test(tst_dxcc_color_provider)

# ── Phase 3J-2 Task D1: SpotModel (TCI-keyed spot sink) ────────────────────
# Pins the contract that SpotModel is a QMap<int, SpotData> sink
# keyed by monotonic spot index, with a TCI-keyed update API:
#   void applySpotStatus(int index, const QMap<QString,QString>& kvs);
# recognising 12 keys (callsign, rx_freq, tx_freq, mode, color,
# background_color, source, spotter_callsign, comment, timestamp,
# lifetime_seconds, priority) and decoding the TCI 0x7F (DEL)
# wire-format quirk to a single ASCII space in callsign and comment.
# Six signals: spotAdded / spotUpdated / spotRemoved / spotsCleared /
# spotsRefreshed / spotTriggered. Seven tests:
#   initialState (empty model reports zero spots), applySpotStatusAddsNew
#   (first applySpotStatus for an index emits spotAdded), applySpotStatusUpdatesExisting
#   (subsequent applySpotStatus emits spotUpdated and preserves prior
#   fields not in the update), appliesAllTwelveKeys (every recognised
#   key round-trips), decodes0x7FAsSpace (TCI wire-format quirk decoded
#   in callsign + comment), removeSpotEmitsSignal (removeSpot emits
#   spotRemoved and erases the row), clearEmitsSignal (clear emits
#   spotsCleared once and empties the map). Source: AetherSDR
#   src/models/SpotModel.{h,cpp} [@0cd4559]. First task of Phase D
#   (Models).
nereus_add_test(tst_spot_model)

# ── Phase 3J-2 Task D2: SpotTableModel + BandFilterProxy ───────────────────
# Pins the contract that SpotTableModel is a QAbstractTableModel with
# 8 columns (Time, Freq, DxCall, Comment, Spotter, Band, Mode, Source)
# bounded at 500 spots; BandFilterProxy is a QSortFilterProxyModel
# that hides spots whose band is in m_hiddenBands. Eight tests:
#   initialState (empty model reports zero rows + 8 columns),
#   addSpotIncrementsRowCount (addSpot grows the row count by one),
#   columnCountIsEight (ColCount sentinel == 8), dataRoundTrip
#   (DxCall column for a freshly-added spot returns the inserted
#   callsign), freqAtRowMatchesSource (freqAtRow(row) returns the
#   inserted freqMhz), setMaxSpotsBounds (with maxSpots=3, adding
#   10 spots leaves rowCount() == 3), bandFilterHidesMatchingBand
#   (proxy sees both spots until setBandVisible("20m", false), then
#   only the 40m spot survives), bandFilterShowsAfterUnhide (toggling
#   visibility back to true restores the row). Source: AetherSDR
#   src/gui/DxClusterDialog.h:33-75 + src/gui/DxClusterDialog.cpp:75-226
#   [@0cd4559]; both classes lived inline in the dialog header upstream
#   and were extracted to standalone src/models/ files so the SpotHubDialog
#   (Phase F) can reuse them. Second task of Phase D (Models).
nereus_add_test(tst_spot_table_model)

# ── Phase 3J-2 Task D3: FreeDVStationModel (live station map) ──────────────
# Pins the contract that FreeDVStationModel is a QHash<QString sid,
# FreeDVStation> sink keyed by Socket.IO session ID, with four public
# slots (onStationAdded / onStationUpdated / onStationRemoved / clear)
# and four signals (stationAdded / stationUpdated / stationRemoved /
# cleared). setOurGridSquare(grid) drives the haversine distance +
# initial-bearing computation so each FreeDVStation row carries
# distanceKm + headingDeg suitable for the FreeDVReporterDialog
# (Phase G) Distance / Heading columns. Seven tests:
#   initialState (empty model reports zero stations),
#   onStationAddedEmits (first onStationAdded for a sid emits
#   stationAdded and grows the map by one), onStationUpdatedReplaces
#   (subsequent onStationUpdated emits stationUpdated and the new
#   FreeDVStation value overwrites the old), onStationRemovedDeletes
#   (onStationRemoved emits stationRemoved and erases the row),
#   clearEmitsCleared (clear emits cleared once and empties the map),
#   distanceComputedFromGrid (with our grid set to FN42 / Boston-ish
#   and a station added at JO31 / Frankfurt-ish, the stored
#   distanceKm lands in the 5500-7000 km great-circle band and the
#   headingDeg lands in [0, 360]), stationBySidLookup (sid -> callsign
#   round-trips, unknown sid returns a default-constructed
#   FreeDVStation). Source: NEW model with no AetherSDR equivalent
#   (gap-fill from the Phase 3J-2 design doc Section 4 Flow 2);
#   distance / heading math ported from freedv-gui's
#   src/gui/dialogs/freedv_reporter.cpp:2312-2410 (calculateDistance_ /
#   calculateLatLonFromGridSquare_ / calculateBearingInDegrees_ /
#   DegreesToRadians_ / RadiansToDegrees_) [@77e793a]. Third task of
#   Phase D (Models).
nereus_add_test(tst_freedv_station_model)

# ── Phase 3J-2 Task D4: RxDecodeModel (bounded ring buffer) ─────────────────
# Pins the contract that RxDecodeModel holds a QVector<RxDecode> bounded
# at maxSize (default 200), emits decodeAdded on each addDecode() call,
# evicts the oldest entry when size exceeds maxSize, and emits cleared
# on clear(). Verifies mixed-source ingestion (rade_text + WSJT-X).
# Asserted via QSignalSpy: initialState (empty on construction),
# addDecodeEmits (signal fires + decodes() grows by 1),
# ringBufferDropsOldest (250 adds with maxSize=200 leaves CALL50..CALL249),
# clearEmits (cleared signal fires + decodes() empty),
# addsFromMultipleSources (rade_text + WSJT-X coexist preserving order).
# Source: NEW NereusSDR-native model. No upstream equivalent.
# Distinguishes "what my radio just heard" from network/cluster spots
# in SpotModel. Per design doc Phase D Task D4 + Section 2 (three-model
# split). Fourth task of Phase D (Models).
nereus_add_test(tst_rx_decode_model)

# ── Phase 3J-2 Task D5: SliceModel snrDb Q_PROPERTY ─────────────────────────
# Pins the contract that SliceModel exposes a snrDb Q_PROPERTY with NaN
# default, emits snrDbChanged only on actual value change, and treats
# NaN->NaN as no-change (no emission) per the design doc. RadeChannel
# (Phase R) will populate this when slice mode is RADE; VfoWidget
# (Phase L1) binds snrDbChanged to paint the SNR row in the flag.
# Cases:
#   initialSnrIsNaN          - default value is NaN (no SNR available)
#   setSnrDbEmitsOnChange    - non-NaN write fires signal + updates getter
#   setSnrDbNoEmitOnSameValue - identical numeric write = no re-emission
#   setSnrDbNoEmitOnNaNToNaN - NaN -> NaN is a no-op (no emission)
#   setSnrDbEmitOnNaNToValue - NaN -> 5.0 emits (signal-acquired event)
#   setSnrDbEmitOnValueToNaN - 5.0 -> NaN emits (signal-lost event)
# Source: NEW NereusSDR-native extension to SliceModel. No upstream
# equivalent. Per design doc Phase D Task D5 + Phase L Task L1.
# Fifth task of Phase D (Models).
nereus_add_test(tst_slice_model_snr)

# ── Phase 3R Task J1: DSPMode::RADE_U / RADE_L enum entries ──────────────────
# Pins the SliceModel::modeName / modeFromName round-trip for the new
# DSPMode::RADE_U = 12 and DSPMode::RADE_L = 13 values, plus the
# legacy "RADE" singular string migration to RADE_U.  RADE is a
# NereusSDR-native extension to the WDSP-derived DSPMode enum (WDSP
# has no knowledge of RADE); the values signal that the slice's
# signal chain runs through the RadeChannel from I1-I3 instead of
# WDSP's RxChannel.  Like USB/LSB, RADE has upper/lower sideband
# variants with mirrored 1700 Hz passbands.
nereus_add_test(tst_dspmode_rade)

# ── Phase 3R Task L1: VfoWidget SNR row ─────────────────────────────────────
# Pins the contract that VfoWidget surfaces the active slice's snrDb
# Q_PROPERTY (set by RadeChannel via I5 routing) as a dim-grey /
# yellow / green text row, visible only when the slice's mode is
# either RADE sideband (DSPMode::RADE_U or DSPMode::RADE_L).
# NereusSDR-native UI extension; no upstream equivalent.
# Cases:
#   nanShowsDashesInGrey      - NaN -> " -   - " in #7a8088
#   lowSnrShowsYellow         - 3.0 dB -> "+3 dB" in #e6c200
#   goodSnrShowsGreen         - 12.0 dB -> "+12 dB" in #4caf50
#   negativeSnrFormats        - -1.0 dB -> "-1 dB" (no double-sign)
#   snrHiddenInNonRadeMode    - USB hides row; RADE_U shows it.
#   snrVisibleInRadeLower     - RADE_L also shows the row.
nereus_add_test(tst_vfo_widget_snr)

# ── Phase 3R Task L2: RadeApplet ────────────────────────────────────────────
# Pins the RadeApplet UI contract:
#   constructsWithRadioModel              - builds against a primed
#                                           RadioModel + RadeChannel.
#   profileComboPopulatesFromMicProfileManager - combo holds the live
#                                           profileNames() list.
#   profileComboDefaultsToRade            - "RADE" preset (K1) is the
#                                           initial selection.
#   onSnrChangedUpdatesSnrLabel           - radeSnrChanged updates text.
#   onSyncChangedUpdatesIndicatorColor    - radeSyncChanged drives the
#                                           LED stylesheet.
#   onTextDecodedShowsCallsign            - rade_text decode appears
#                                           in the last-decoded label.
#   resetButtonClickInvokesResetTx        - button click dispatches to
#                                           RadeChannel::resetTx.
nereus_add_test(tst_rade_applet)

# ── Phase 3R Task L3: Mode menu + VFO chip RADE entry ────────────────────────
# Pins the RADE entries (upper + lower sidebands) in the user-facing
# mode selectors.  Two surfaces are covered (MainWindow Mode menu is
# too heavy for a unit test and is covered by the existing
# tst_dspmode_rade round-trip plus the L3 manual verification
# matrix).
# Cases:
#   vfoModeComboHasRadeUpperEntry        - VFO flag mode combo lists RADE-U.
#   vfoModeComboHasRadeLowerEntry        - VFO flag mode combo lists RADE-L.
#   vfoModeComboChangesToRadeUpper       - selecting RADE-U emits
#                                          modeChanged(DSPMode::RADE_U).
#   vfoModeComboChangesToRadeLower       - selecting RADE-L emits
#                                          modeChanged(DSPMode::RADE_L).
#   vfoModeTabLabelShowsRadeUpperWhenActive - tab #2 label paints "RADE-U".
#   vfoModeTabLabelShowsRadeLowerWhenActive - tab #2 label paints "RADE-L".
#   vfoModeTabChipPurpleInRadeUpper      - tab #2 stylesheet contains
#                                          the RADE purple accent
#                                          (#a78bfa) when RADE-U active.
#   vfoModeTabChipPurpleInRadeLower      - tab #2 stylesheet contains
#                                          the RADE purple accent when
#                                          RADE-L active.
nereus_add_test(tst_mode_menu_rade)

# ── Phase 3J-2 Task E1: SpectrumWidget spot overlay render ─────────────────
# Pins the contract that drawSpotMarkers places spot labels with
# collision-avoiding multi-level stacking, overflows into +N cluster
# badges at maxBottom + 2 (40 px ClusterBinWidth), and exposes
# click-to-tune hit-test rectangles via spotClickRectsForTest() plus
# cluster badge rectangles via spotClustersForTest(). Six tests:
#   emptyOverlayDrawsNothing - empty marker vector emits zero rects
#   singleSpotDrawsOneLabel - one in-range spot -> one click rect
#   overlappingSpotsStackVertically - 3 collisions stack down (y0<y1<y2)
#   overflowSpotsBecomeClusterBadge - 8 spots, max 2 levels -> 1 cluster of 6
#   clickRectAtSpotXTuneable - rect center contains hit point
#   rejectsBeyondVisibleRange - spots outside m_centerHz +/- bw/2 dropped
# Source: AetherSDR src/gui/SpectrumWidget.cpp:4497-4633 [@0cd4559]
# (drawSpotMarkers + showSpotClusterPopup). First task of Phase E
# (panadapter spot overlay).
nereus_add_test(tst_spot_overlay_render)

# ── Phase 3J-2 Task F1+F2+F3: SpotHubDialog tab-strip smoke ────────────────
# Pins the contract that SpotHubDialog (the AetherSDR DxClusterDialog
# port) constructs successfully when handed all 7 spot-ingest clients
# + SpotModel + DxccColorProvider, that its top-level QTabWidget
# exposes exactly 10 tabs (post-3J-2 NereusSDR-native Settings tab at
# index 0 plus the 9 AetherSDR upstream tabs), that the tab labels
# appear in the documented order, that each per-source tab carries the
# uniform template (F2), and that the Spot List tab hosts the merged
# 8-column table with band + source pill filters (F3). 21 tests total:
#   F1 (3): dialogConstructs, hasTenTabs, tabOrderMatchesAetherSdr
#   F2 (12): per-source tab content + everySourceTabHasUniformTemplate
#   F3 (6): spotListTabHasTableView, spotListTabHasTwelveBandPills,
#           spotListTabHasSevenSourcePills,
#           spotListBandPillTogglesProxyFilter,
#           spotListSourcePillTogglesProxyFilter,
#           doubleClickOnSpotRowEmitsTuneRequested
# Source: AetherSDR src/gui/DxClusterDialog.{h,cpp} [@0cd4559]; Display
# tab content lands in F4. Settings tab is NereusSDR-native (no
# upstream); see tst_spothub_settings_tab for its content contract.
nereus_add_test(tst_spothub_dialog_smoke)

# ── Post-3J-2: SpotHub Settings tab (operator identity) ────────────────────
# Pins the contract that SpotHubDialog now exposes a Settings tab in
# the first position with QLineEdit fields for callsign + grid + FreeDV
# status message, a Save button that writes to the canonical User/* +
# FreeDvReporter/Message keys AND propagates to the per-source legacy
# keys (DxClusterCallsign / RbnCallsign / PskReporterCallsign /
# FreeDvReporter/Callsign / FreeDvReporter/GridSquare), validation that
# rejects empty callsigns and invalid Maidenhead grids, the FreeDV tab
# identity-status label (green when identity is set / yellow warning
# when missing), and the per-source tabs falling back to User/Callsign
# when their per-source key is empty.
# Source: NereusSDR-native. AetherSDR upstream scatters identity
# fields across each per-source tab with no central entry point;
# the Settings tab is a NereusSDR UX correction.
nereus_add_test(tst_spothub_settings_tab)

# ── Phase 3J-2 Task G1: FreeDVReporterDialog shell + 14-col table ─────────
# Pins the contract that FreeDVReporterDialog (the freedv-gui
# freedv_reporter.{h,cpp} port) constructs successfully when handed a
# FreeDVStationModel, that its embedded QTableView reports the 14-column
# layout drawn from upstream's createColumn_ switch (column labels,
# alignment, exact column count), and that the per-row TX-red /
# RX-green highlight tones flip on / off in response to
# FreeDVStationModel::stationUpdated. 10 tests total:
#   G1 (10): dialogConstructs, tableHasFourteenColumns,
#            tableColumnHeadersMatchUpstream,
#            tableColumnAlignmentsMatchUpstream,
#            populatesFiveStationsToFiveRows,
#            snrColumnFormatsMinusNinetyNineAsDash,
#            txReportTurnsRowRed, rxReportTurnsRowGreen,
#            highlightClearsAfterSixSeconds, stationRemovedDeletesRow
# Source: freedv-gui src/gui/dialogs/freedv_reporter.{h,cpp} [@77e793a].
# Filter / QSY / message bar (G2) and menu bar with Show / Filter /
# Idle-longer-than menus (G3) follow in later tasks.
nereus_add_test(tst_freedv_reporter_dialog)

# ── Phase 3J-2 Task H2: RadioModel spot-client + view-model wiring ─────────
# Pins the contract that RadioModel owns the seven spot-ingest clients
# (DxCluster + RBN + WSJT-X + SpotCollector + POTA + FreeDV Reporter +
# PSK Reporter), the three view models (SpotModel + FreeDVStationModel +
# RxDecodeModel), and the DxccColorProvider, and that every client's
# spotReceived(DxSpot) signal flows through a per-source adapter slot
# into SpotModel::applySpotStatus. 13 tests total:
#   accessor surface (5): spotModel, freeDvStationModel, rxDecodeModel,
#                         dxccColorProvider, all-six-clients
#   per-source adapters (8): dxCluster, rbn, wsjtx, wsjtx-decode-feed,
#                            pota, spotCollector, freeDvReporter,
#                            pskReporter
#   freeDv station model (1): stationAdded -> stationCount round-trip
# Source: NereusSDR-native. The plan task is Phase 3J-2 H2; AetherSDR's
# upstream RadioModel applies spot status only via TCI parsing, not
# per-client adapter slots, so the wiring shape is NereusSDR-specific.
nereus_add_test(tst_radio_model_spot_wiring)

# ── Phase 3J-2 Task H1: Tools menu Spot Hub + FreeDV Reporter wiring ───────
# Pins the contract that MainWindow::openSpotHub and
# MainWindow::openFreeDVReporter can construct their respective dialogs
# from the RadioModel accessor surface alone (no extra wiring or
# adapter layers). MainWindow itself is too heavy to instantiate in a
# unit test (see tst_mainwindow_status_bar_safety for the established
# isolate-the-logic pattern), so this suite pins the surrounding
# contracts instead: dialog ctor signatures match RadioModel accessor
# return types, SpotModel::clear (Ctrl+Shift+K) empties every row,
# SpotHubDialog::spotsClearedAll routes into SpotModel::clear, and the
# three FreeDVReporterDialog signals (qsyRequested,
# messageSendRequested, tuneRequested) carry the right argument shape
# for MainWindow's open-slot lambda. End-to-end menu wiring is
# verified by the manual bench-test matrix.
# Source: NereusSDR-native test pattern; ports are at H2.
nereus_add_test(tst_mainwindow_tools_spot_hub)

# ── Phase 3J-2 + 3R M1: spot-system AppSettings round-trip ─────────────────
# Pins the key contract for the ~40 AppSettings keys introduced by the
# Phase 3J-2 spot ingest clients (B-phase), the SpotHubDialog (F-phase
# per-source identity / auto-X / colors / Display knobs), the FreeDV
# Reporter dialog (G-phase saved messages / visible columns / idle
# timeout / column filters / band filter), H2's per-source
# lifetime / color resolution on RadioModel, and the Phase 3R RADE
# wrapper. AppSettings is fully dynamic (no key registry), so this
# file is the single enforcement point for the documented key list and
# their documented defaults. The legacyMinutesKeyFallback test pins the
# DxClusterSpotLifetime -> DxClusterSpotLifetimeSec * 60 migration the
# SpotHubDialog F4 Display tab implements at SpotHubDialog.cpp:1727-1730.
# Source: NereusSDR-native test file; AppSettings is the Thetis
# database.cs port, but the keys exercised here are all NereusSDR
# additions for Phase 3J-2 + 3R.
nereus_add_test(tst_spot_settings_round_trip)

# ── Phase 3J-2 + 3R M2: SpotHub Display tab knobs end-to-end ───────────────
# Pins the consumer side of the Display tab loop introduced by
# SpotHubDialog F4 (commit 2026-05-11):
#   F4 already writes every knob change to AppSettings and emits
#   SpotHubDialog::settingsChanged.
#   M2 adds SpectrumWidget::loadSpotDisplaySettings which reads
#   every Display tab key (IsSpotsEnabled / SpotFontSize / SpotsMaxLevel
#   / SpotsStartingHeightPercentage / IsSpotsOverrideColorsEnabled
#   / IsSpotsOverrideBackgroundColorsEnabled / SpotsOverrideColor /
#   SpotsOverrideBgColor / SpotsBackgroundOpacity) and pushes each
#   value into the corresponding SpectrumWidget setter. MainWindow's
#   openSpotHub() wires SpotHubDialog::settingsChanged to that helper
#   so the live spectrum overlay refreshes when the operator drags a
#   slider in the dialog.
# 4 tests:
#   loadSpotDisplaySettingsAppliesPersistedShowSpots
#   loadSpotDisplaySettingsDefaultsShowSpotsTrue
#   loadSpotDisplaySettingsAppliesAllNumericKnobs
#   loadSpotDisplaySettingsAppliesColors
# Source: NereusSDR-native; the consumer hook is M2-local. SpotHubDialog
# F4 producer side is already pinned by tst_spothub_dialog_smoke.
nereus_add_test(tst_spothub_display_knobs)

# ── Phase 3J-2 + 3R M3: spot-client auto-connect / auto-start restore ──────
# Pins the launch-time restore path:
#   RadioModel::restoreSpotClientAutoStartState reads each persisted
#   "<Source>AutoConnect" / "<Source>AutoStart" key from AppSettings
#   and, when True, calls the corresponding start method with the
#   persisted identity / port / interval params. MainWindow invokes
#   this once at startup after the RadioModel is fully wired (sibling
#   to the existing tryAutoReconnect singleShot for the radio
#   connection).
# 10 tests:
#   dxClusterAutoConnectFiresStartCall / FalseDoesNotFire
#   rbnAutoConnectFiresStartCall
#   wsjtxAutoStartFiresListenCall / FalseDoesNotFire
#   spotCollectorAutoStartFiresListenCall
#   potaAutoStartFiresPollingCall / FalseDoesNotFire
#   freeDvReporterAutoConnectFalseDoesNotFire / IsNonDestructive
#   pskReporterAutoStartTrueIsNoOp (PSK Reporter is send-only;
#     auto-start has no meaningful action.)
# Source: NereusSDR-native. The clients themselves are AetherSDR ports,
# but the restore driver is a NereusSDR addition for Phase 3J-2 + 3R M3.
nereus_add_test(tst_spot_auto_start)

# ── Phase 3R Task I1: RadeChannel skeleton (hybrid port) ───────────────────
# Pins the three lifecycle contracts the Phase 3R Task I2/I3/I4 DSP work
# layers onto:
#   initialState                  fresh wrapper is !isActive() && !isSynced()
#   startStop                     start(<real path>) flips isActive() true;
#                                 stop() flips it back
#   modelLoadFailureDisables      start(<nonexistent>) returns false without
#                                 mutating state
# The wrapper itself is a hybrid port: structure (class layout, Q_OBJECT
# signals/slots, member ownership, dtor placement for forward-declared
# unique_ptr types) follows AetherSDR src/core/RADEEngine.{h,cpp}
# [@0cd4559]; the DSP API surface that I2/I3 will plug in (rade_open,
# rade_n_features_in_out, LPCNet feature extractor, FARGAN vocoder)
# follows freedv-gui src/pipeline/RADE{Receive,Transmit}Step.{h,cpp}
# [@77e793a]. I1 ships skeleton bodies only; no DSP yet.
nereus_add_test(tst_rade_channel)

# Phase 3R Task I2a pins the Resampler wrapper around r8brain-free-src
# (vendored MIT, third_party/r8brain/ at SHA 5c44bebe). The Resampler
# port from AetherSDR src/core/Resampler.{h,cpp} [@0cd4559] is byte-for-
# byte aside from the namespace rename. 5 tests: identity 24k->24k,
# downsample 24k->8k, mono-to-stereo / stereo-to-mono / stereo-to-stereo
# convenience helpers. Approximate sample-count checks because r8brain
# CDSPResampler24 has nontrivial internal latency.
nereus_add_test(tst_resampler)

# ── Phase 3R Task I4: RadeText (callsign-over-EOO wrapper) ────────────────
# Pins the five contracts of the thin Qt6 wrapper around the
# third_party/rade callsign-over-EOO channel
# (rade_tx_set_eoo_callsign / rade_rx_get_eoo_callsign, declared in
# third_party/rade/src/rade_api.h:120-145 [@b289102]):
#   constructsAndIsEmpty             fresh wrapper has empty ourCallsign
#   setOurCallsignStoresValue        getter returns the value the setter
#                                    stashed; lower / upper / empty all
#                                    round-trip unchanged
#   pushTxNullRadeIsNoOp             pushTxCallsign(nullptr) is safe
#                                    (RadeChannel's m_rade is null
#                                    between stop() and start())
#   pushTxEmptyCallsignIsNoOp        pushTxCallsign on an empty stash
#                                    must not clobber rade_tx.eoo_bits
#   roundTripDecodesEncodedCallsign  setOurCallsign + pushTxCallsign +
#                                    processRxEooBits emits
#                                    textDecoded("KG4VCF") once
# The headline test does a bit-level round-trip (no OFDM mod/demod)
# because the wrapper only sits on set_eoo_callsign / get_eoo_callsign;
# the full OFDM round-trip is exercised separately by the upstream
# third_party/rade/src/rade_callsign_test.c executable.
nereus_add_test(tst_rade_text)

# ── Phase 3R Task I5: RadeChannel -> RadioModel slot graph wiring ─────────
# Pins the six contracts RadioModel::wireRadeChannel(sliceId, channel, slice)
# must satisfy when Phase J creates a RadeChannel per slice:
#   wireRadeChannelConnectsSnrToSlice    RadeChannel::snrChanged forwards
#                                        through onRadeSnrChanged to
#                                        SliceModel::setSnrDb (cast-up
#                                        float -> double).
#   wireRadeChannelEmitsRadioModelSync   RadeChannel::syncChanged updates
#                                        m_radeSyncedSlices and re-emits
#                                        RadioModel::radeSyncChanged.
#   repeatedSyncEmitsOnlyOnceUntilChange Idempotent: three sync=true
#                                        emissions collapse to one
#                                        RadioModel::radeSyncChanged emit.
#   rxTextDecodedAddsRxDecodeRow         RadeChannel::rxTextDecoded
#                                        constructs an RxDecode row with
#                                        mode=RADE, source=rade_text,
#                                        payload="<call> <grid>" and
#                                        pushes it to RxDecodeModel.
#   rxTextWithEmptyGridStoresCallsignOnly Grid-less decode (the I4 Option B
#                                        callsign-over-EOO channel does
#                                        not carry grid) collapses the
#                                        payload to bare callsign.
#   wiringWithNullChannelIsNoOp          Null channel / null slice
#                                        arguments do not crash.
# Source: NereusSDR-native test pattern; no upstream port. The slot
# contract under test was established by the I5 plan spec.
nereus_add_test(tst_rade_channel_model_wiring)

# ── Phase 3R K-bench: RADE TX end-to-end pump ─────────────────────────────
# Pins the K2-K4 follow-up contracts that turn the deferred RADE TX
# scaffolding into a real pump:
#   setRadeChannelStoresPointer       TxWorkerThread::setRadeChannel
#                                     accessor round-trip + null-clear.
#   radePumpEmitsAfterWarmup          tickForTest with TxPath::Rade
#                                     pushes mic blocks through HPF +
#                                     48->16 r8brain + float->int16 +
#                                     emits radeMicBlockReady with at
#                                     least one non-empty payload.
#   wdspPathStillReachesSendTxIq      With TxPath::Wdsp the worker still
#                                     drives the WDSP TXA chain.  RADE
#                                     branch must be opt-in.
#   txModemReadyRoutesToSendTxIq      RadioModel::wireRadeChannel's
#                                     txModemReady lambda routes the
#                                     RADE baseband through 24 ->
#                                     txSampleRate upsample into
#                                     RadioConnection::sendTxIq.
nereus_add_test(tst_rade_tx_pump)
target_compile_definitions(tst_rade_tx_pump PRIVATE NEREUS_BUILD_TESTS)
