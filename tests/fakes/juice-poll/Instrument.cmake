# no-port-check: NereusSDR-original, test-only vendor overlay.
# Modification history: 2026-10-03, J.J. Boyd (KG4VCF), OpenAI Codex.
# Compile the actual build-owned patched vendor sources, with exact anchored
# observers/barriers in separate generated files. Shipping sources/archives
# are never changed or linked into this standalone Qt fixture.
function(_nereus_juice_test_replace variable anchor replacement)
    string(FIND "${${variable}}" "${anchor}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "libjuice poll fixture anchor absent: ${anchor}")
    endif()
    string(REPLACE "${anchor}" "" _without "${${variable}}")
    string(LENGTH "${${variable}}" _before)
    string(LENGTH "${_without}" _after)
    string(LENGTH "${anchor}" _length)
    math(EXPR _removed "${_before} - ${_after}")
    if(NOT _removed EQUAL _length)
        message(FATAL_ERROR "libjuice poll fixture anchor is not unique")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _result "${${variable}}")
    set(${variable} "${_result}" PARENT_SCOPE)
endfunction()
set(_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/juice-poll-fixture")
file(MAKE_DIRECTORY "${_fixture_dir}")
file(READ "${libjuice_SOURCE_DIR}/src/conn_poll.c" _poll)
_nereus_juice_test_replace(_poll "\tconn_poll_run(registry);"
    "\tconn_poll_run(registry);\n\tnereus_juice_poll_test_event(registry, NereusJuiceWorkerExited, 0, 0);")
_nereus_juice_test_replace(_poll "\tthread_join(registry_impl->thread, NULL);"
    "\tthread_join(registry_impl->thread, NULL);\n\tnereus_juice_poll_test_event(registry, NereusJuiceWorkerJoined, 0, 0);")
_nereus_juice_test_replace(_poll "void conn_poll_process_udp(juice_agent_t *agent, struct pollfd *pfd) {\n\tconn_impl_t *conn_impl = agent->conn_impl;\n\n\tif (pfd->revents & POLLNVAL)"
    "void conn_poll_process_udp(juice_agent_t *agent, struct pollfd *pfd) {\n\tconn_impl_t *conn_impl = agent->conn_impl;\n\tif (nereus_juice_poll_test_error(agent)) pfd->revents |= POLLERR;\n\n\tif (pfd->revents & POLLNVAL)")
_nereus_juice_test_replace(_poll "\t\tagent_conn_fail(agent);\n\t\tconn_impl->state = CONN_STATE_FINISHED;\n\t}"
    "\t\tagent_conn_fail(agent);\n\t\tconn_impl->state = CONN_STATE_FINISHED;\n\t\tnereus_juice_poll_test_event(agent, NereusJuiceSocketErrorFinished, conn_impl->state, conn_impl->registry->agents_count);\n\t}")
_nereus_juice_test_replace(_poll "\t\tif (i >= pfds->size)\n\t\t\tbreak;"
    "\t\tif (i >= pfds->size) {\n\t\t\tnereus_juice_poll_test_event(agent, NereusJuiceSnapshotMismatch, i, pfds->size);\n\t\t\tbreak;\n\t\t}")
_nereus_juice_test_replace(_poll "\t\tint ret = poll(pfds.pfds, pfds.size, (int)timediff);"
    "\t\tnereus_juice_poll_test_event(registry, NereusJuicePrepared, count, pfds.size);\n\t\tnereus_juice_poll_test_barrier(registry);\n\t\tnereus_juice_poll_test_event(registry, NereusJuicePollEntering, timediff, pfds.size);\n\t\tint ret = poll(pfds.pfds, pfds.size, (int)timediff);\n\t\tnereus_juice_poll_test_event(registry, NereusJuicePollReturned, ret, 0);")
file(READ "${libjuice_SOURCE_DIR}/src/agent.c" _agent)
_nereus_juice_test_replace(_agent "int agent_bookkeeping(juice_agent_t *agent, timestamp_t *next_timestamp) {"
    "int agent_bookkeeping(juice_agent_t *agent, timestamp_t *next_timestamp) {\n\tnereus_juice_poll_test_event(agent, NereusJuiceBookkeeping, 0, 0);")
set(_hooks "${CMAKE_CURRENT_SOURCE_DIR}/fakes/juice-poll/Hooks.h")
file(WRITE "${_fixture_dir}/conn_poll.c" "#include \"${_hooks}\"\n${_poll}")
file(WRITE "${_fixture_dir}/agent.c" "#include \"${_hooks}\"\n${_agent}")
# Record exact vendor leaves and generated overlay provenance for this build.
foreach(_leaf IN ITEMS conn_poll.c agent.c)
    file(SHA256 "${libjuice_SOURCE_DIR}/src/${_leaf}" _original)
    file(SHA256 "${_fixture_dir}/${_leaf}" _overlay)
    file(APPEND "${_fixture_dir}/provenance-next.txt"
        "${libjuice_SOURCE_DIR}/src/${_leaf} ${_original}\n${_fixture_dir}/${_leaf} ${_overlay}\n")
endforeach()
file(RENAME "${_fixture_dir}/provenance-next.txt" "${_fixture_dir}/provenance.txt")
get_target_property(_sources juice-static SOURCES)
list(REMOVE_ITEM _sources "${libjuice_SOURCE_DIR}/src/agent.c" "${libjuice_SOURCE_DIR}/src/conn_poll.c")
add_library(nereus_juice_poll_fixture STATIC EXCLUDE_FROM_ALL ${_sources}
    "${_fixture_dir}/agent.c" "${_fixture_dir}/conn_poll.c" fakes/juice-poll/Access.c)
set_target_properties(nereus_juice_poll_fixture PROPERTIES AUTOMOC OFF C_STANDARD 11)
get_target_property(_definitions juice-static COMPILE_DEFINITIONS)
get_target_property(_libraries juice-static LINK_LIBRARIES)
target_compile_definitions(nereus_juice_poll_fixture PRIVATE ${_definitions})
target_link_libraries(nereus_juice_poll_fixture PRIVATE ${_libraries})
target_compile_definitions(nereus_juice_poll_fixture PUBLIC JUICE_STATIC)
target_include_directories(nereus_juice_poll_fixture PUBLIC "${libjuice_SOURCE_DIR}/include"
    PRIVATE "${libjuice_SOURCE_DIR}/src" "${libjuice_SOURCE_DIR}/include/juice")
if(WIN32)
    target_compile_definitions(nereus_juice_poll_fixture PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
