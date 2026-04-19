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

# ── Phase 3O VAX routing ──────────────────────────────────────────────────
nereus_add_test(tst_slice_model_vax)
nereus_add_test(tst_transmit_model_tx_owner)

# ── Phase 3O VAX schema migration ─────────────────────────────────────────
nereus_add_test(tst_app_settings_vax_migration)

# ── Phase 3O Sub-Phase 2: MasterMixer ─────────────────────────────────────
nereus_add_test(tst_master_mixer)

# ── Phase 3O Sub-Phase 3: PortAudioBus ────────────────────────────────────
nereus_add_test(tst_port_audio_bus)
