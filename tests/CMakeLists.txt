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
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Pre-existing tests from main (smoke + 3G-9 meter suite) ────────────────
nereus_add_test(tst_setup_helpers)
nereus_add_test(tst_smoke)

# ── Issue #75: receiver leak across disconnect/reconnect ──────────────────
nereus_add_test(tst_receiver_manager_reconnect)
nereus_add_test(tst_container_persistence)
nereus_add_test(tst_meter_item_bar)
nereus_add_test(tst_reading_name)
nereus_add_test(tst_meter_item_scale)
nereus_add_test(tst_meter_presets)

# ── Phase 3G-9b clarity defaults invariants ───────────────────────────────
nereus_add_test(tst_clarity_defaults)

# ── Phase 3G-9c Clarity adaptive display tuning ──────────────────────────
nereus_add_test(tst_noise_floor_estimator)
nereus_add_test(tst_noise_floor_tracker)
nereus_add_test(tst_clarity_controller)

# ── Phase 3I radio-connector tests ────────────────────────────────────────
nereus_add_test(tst_hpsdr_enums)
nereus_add_test(tst_board_capabilities)

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

# ── About dialog ─────────────────────────────────────────────────────────
nereus_add_test(tst_about_dialog)

# ── Phase 3O Sub-Phase 11 Task 11a: VaxFirstRunDialog ────────────────────
nereus_add_test(tst_vax_first_run_dialog)

# ── Phase 3O VAX routing ──────────────────────────────────────────────────
nereus_add_test(tst_slice_model_vax)
nereus_add_test(tst_transmit_model_tx_owner)

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

# ── Phase 3P-G commit 1: CalibrationController model ─────────────────────────
# Verifies defaults (freqFactor=1.0, all offsets=0), setter signal emission
# (idempotent no-fire), effectiveFreqCorrectionFactor() toggle between normal
# and 10 MHz ref factor, and per-MAC AppSettings persistence round-trip.
# Ports setup.cs:5137-5144; 14036-14050; 22690-22706; 14325; 17243; 18315 +
# console.cs:9766-9844; 21022-21086 [@501e3f5].
nereus_add_test(tst_calibration_controller)

# ── Phase 3P-G commit 2: CalibrationTab UI smoke tests ───────────────────────
# Verifies construction doesn't crash, 5 group boxes present
# (Freq Cal / Level Cal / HPSDR Freq Cal Diagnostic / TX Display Cal /
# PA Current), controller->UI sync, and UI->controller write-back.
nereus_add_test(tst_calibration_tab)
target_compile_definitions(tst_calibration_tab PRIVATE NEREUS_BUILD_TESTS)

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


