# NereusSDR test suite. Opt-in via -DNEREUS_BUILD_TESTS=ON.
#
# Test targets compile against NereusSDRObjs (the OBJECT library that backs
# the main executable) so behavior under test is byte-identical to shipped
# code. Each test gets its own thin executable + add_test() registration.

find_package(Qt6 REQUIRED COMPONENTS Test)

function(nereus_add_test name)
    # Additional source files may be passed as extra arguments (ARGN) —
    # needed by Phase 3I tests that link test-only fakes like P1FakeRadio.
    add_executable(${name} ${name}.cpp ${ARGN})
    target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Pre-existing tests from main (smoke + 3G-9 meter suite) ────────────────
nereus_add_test(tst_smoke)
nereus_add_test(tst_container_persistence)
nereus_add_test(tst_meter_item_bar)
nereus_add_test(tst_reading_name)
nereus_add_test(tst_meter_item_scale)
nereus_add_test(tst_meter_presets)

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

# Regression test for Windows shutdown/disconnect crash — cross-thread
# destruction of RadioConnection (and its worker-affined QTimers).
nereus_add_test(tst_cross_thread_teardown fakes/P1FakeRadio.cpp)
target_compile_definitions(tst_cross_thread_teardown PRIVATE NEREUS_BUILD_TESTS)
