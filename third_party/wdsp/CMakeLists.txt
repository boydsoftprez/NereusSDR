# WDSP DSP Library — static build for NereusSDR
#
# Based on TAPR/OpenHPSDR-wdsp v1.29 by Warren Pratt, NR0V
# Cross-platform support via linux_port.h/c from g0orx/wdsp (John Melton, G0ORX)
#
# WDSP is licensed under GPL-2.0-or-later. NereusSDR distributes the
# combined work under GPLv3 via GPLv3 §5(b) upgrade. See
# docs/attribution/WDSP-PROVENANCE.md.

cmake_minimum_required(VERSION 3.20)
project(wdsp C)

# Collect all C source files
file(GLOB WDSP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# linux_port.c is only compiled on non-Windows
if(WIN32)
    list(FILTER WDSP_SOURCES EXCLUDE REGEX "linux_port\\.c$")
endif()

add_library(wdsp_static STATIC ${WDSP_SOURCES})

target_include_directories(wdsp_static
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# FFTW3 — header needed for compilation, library for final link
if(FFTW3_INCLUDE_DIRS)
    target_include_directories(wdsp_static PRIVATE ${FFTW3_INCLUDE_DIRS})
endif()
if(FFTW3_LIBRARIES)
    target_link_libraries(wdsp_static PUBLIC ${FFTW3_LIBRARIES})
endif()

# Static build: PORT macro is empty (no dllexport)
target_compile_definitions(wdsp_static PRIVATE
    WDSP_STATIC_BUILD
)

# Platform-specific configuration
if(WIN32)
    target_link_libraries(wdsp_static PRIVATE avrt)
    target_compile_definitions(wdsp_static PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
    )
else()
    target_link_libraries(wdsp_static PRIVATE pthread m)
    target_compile_definitions(wdsp_static PRIVATE _GNU_SOURCE)
    if(APPLE)
        # macOS: dispatch framework not needed — linux_port uses named sem_open
    endif()
endif()

# Suppress warnings in third-party C code
target_compile_options(wdsp_static PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    $<$<C_COMPILER_ID:MSVC>:/W0>
)
