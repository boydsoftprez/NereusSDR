# third_party/rnnoise/CMakeLists.txt
#
# Vendored Xiph.org rnnoise (BSD 3-clause) via FetchContent at a pinned
# upstream commit. Used by third_party/wdsp/src/rnnr.c (NR3 backend).
# See README.md for strategy rationale.
#
# Runtime-only model loading: we define USE_WEIGHTS_FILE so the baked-in
# weight arrays in rnnoise_data*.c are compiled out. NereusSDR loads the
# Default_large.bin model at runtime via RNNRloadModel() on radio connect.
# rnnoise_create(NULL) is never called — RadioModel always passes the loaded
# model pointer.
#
# Analysis of the NULL-model code path (from denoise.c:285-308):
#   rnnoise_init(st, NULL) → falls into the #ifndef USE_WEIGHTS_FILE else branch
#   which calls init_rnnoise(&st->model, rnnoise_arrays) where rnnoise_arrays is
#   defined in rnnoise_data_little.c (little-endian) / rnnoise_data.c (BE).
#   Defining USE_WEIGHTS_FILE suppresses that else branch entirely, so excluding
#   rnnoise_data*.c from the source glob produces a clean link.
#   Reference: Thetis rnnoise/src/denoise.c:285-308, rnnoise_data_little.c:274781.

cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# Pinned to match the revision vendored in Thetis v2.10.3.13 @ 501e3f51.
# Verified 2026-04-23 against https://gitlab.xiph.org/xiph/rnnoise HEAD.
FetchContent_Declare(
    rnnoise_upstream
    GIT_REPOSITORY https://gitlab.xiph.org/xiph/rnnoise.git
    GIT_TAG        70f1d256acd4b34a572f999a05c87bf00b67730d
    GIT_SHALLOW    FALSE   # full clone required for specific SHA pin
)

FetchContent_MakeAvailable(rnnoise_upstream)

# Build the library from upstream source files.
# Exclude:
#   - dump_features, dump_rnnoise_tables, write_weights: standalone tools, not lib
#   - rnnoise_data, rnnoise_data_little: baked model weights; excluded because
#     USE_WEIGHTS_FILE is defined below (runtime-only model loading path).
file(GLOB RNNOISE_SOURCES "${rnnoise_upstream_SOURCE_DIR}/src/*.c")
list(FILTER RNNOISE_SOURCES EXCLUDE REGEX
    "/(dump_features|dump_rnnoise_tables|write_weights|rnnoise_data|rnnoise_data_little)\\.c$")

if(NOT RNNOISE_SOURCES)
    message(FATAL_ERROR
        "rnnoise source glob returned no files — FetchContent did not populate "
        "rnnoise_upstream_SOURCE_DIR (${rnnoise_upstream_SOURCE_DIR})?")
endif()

# rnnoise_model_init.c provides init_rnnoise(), which is normally bundled
# inside rnnoise_data_little.c / rnnoise_data.c. Those files are excluded
# because they contain 274 000+ lines of baked model weights. We supply
# a thin shim that contains only the function body (no weight data).
# Ported from Thetis rnnoise_data_little.c:274936-274952 [v2.10.3.13 @501e3f51].
set(RNNOISE_MODEL_INIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/rnnoise_model_init.c")

add_library(rnnoise_static STATIC ${RNNOISE_SOURCES} ${RNNOISE_MODEL_INIT_SRC})

target_include_directories(rnnoise_static PUBLIC
    "${rnnoise_upstream_SOURCE_DIR}/include"
    "${rnnoise_upstream_SOURCE_DIR}/src")

target_compile_definitions(rnnoise_static PRIVATE
    # Suppress the baked-weight fallback branch in denoise.c:298 so we don't
    # need rnnoise_data*.c at link time. Every NereusSDR launch calls
    # RNNRloadModel() before NR3 can run, so the NULL-model path is dead code.
    USE_WEIGHTS_FILE
    # Standard Opus build macros expected by rnnoise's internal SSE/NEON paths.
    RNNOISE_BUILD
    COMPILE_OPUS
)

# rnnoise's autotools build generates two headers that rnn.h / rnn.c include
# unconditionally:
#
#   config.h      — autoconf feature-detection macros
#   rnnoise_data.h — model architecture constants (CONV1_STATE_SIZE, GRU*_STATE_SIZE,
#                    the RNNoise typedef, and init_rnnoise() declaration)
#
# Neither is shipped in the git tree; both are produced by the autotools /
# Python model-dump pipeline. We provide hand-written shims that match the
# constants from the Thetis-bundled copy at:
#   ../Thetis/Project Files/lib/NR_Algorithms_x64/src/rnnoise/src/rnnoise_data.h
# (Thetis v2.10.3.13 @ 501e3f51 — verified 2026-04-23).
#
# The rnnoise_data.h constants match the Default_large.bin model that
# NereusSDR loads at runtime via RNNRloadModel(). They must stay in sync if
# the model is ever upgraded.

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rnnoise_config")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise_config/config.h"
"/* NereusSDR-generated config.h shim for rnnoise FetchContent build.
 * Minimal: only the macros rnnoise needs to compile without autotools.
 * Generated at configure time — do not edit by hand.
 * Upstream: https://gitlab.xiph.org/xiph/rnnoise @ 70f1d256 (2026-04-23)
 */
#pragma once
#define HAVE_CONFIG_H 1
#define PACKAGE_VERSION \"nereus-pinned-70f1d256\"
/* USE_WEIGHTS_FILE is already set via target_compile_definitions above;
 * repeating here keeps the config.h self-consistent for any translation
 * unit that includes it before seeing the command-line -D flag. */
#define USE_WEIGHTS_FILE 1
")

# rnnoise_data.h — model architecture constants.
# Source: Thetis/Project Files/lib/NR_Algorithms_x64/src/rnnoise/src/rnnoise_data.h
# From Thetis rnnoise/src/rnnoise_data.h [v2.10.3.13 @501e3f51]
# Must be in the same directory as config.h so the PRIVATE include below
# reaches both files. rnn.h includes it with "rnnoise_data.h" (not a path
# prefix), so the rnnoise_config dir must appear before src/ in the search
# path — the target_include_directories(PRIVATE rnnoise_config) below
# guarantees that ordering.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rnnoise_config/rnnoise_data.h"
"/* NereusSDR-generated rnnoise_data.h shim for rnnoise FetchContent build.
 * Constants ported from:
 *   Thetis/Project Files/lib/NR_Algorithms_x64/src/rnnoise/src/rnnoise_data.h
 *   Thetis v2.10.3.13 @ 501e3f51 — verified 2026-04-23.
 * Generated at configure time — do not edit by hand.
 * If the Default_large.bin model is upgraded, update these constants to match.
 */
#ifndef RNNOISE_DATA_H
#define RNNOISE_DATA_H

#include \"nnet.h\"

#define CONV1_OUT_SIZE 128
#define CONV1_IN_SIZE 65
#define CONV1_STATE_SIZE (65 * (2))
#define CONV1_DELAY 1

#define CONV2_OUT_SIZE 384
#define CONV2_IN_SIZE 128
#define CONV2_STATE_SIZE (128 * (2))
#define CONV2_DELAY 1

#define GRU1_OUT_SIZE 384
#define GRU1_STATE_SIZE 384

#define GRU2_OUT_SIZE 384
#define GRU2_STATE_SIZE 384

#define GRU3_OUT_SIZE 384
#define GRU3_STATE_SIZE 384

#define DENSE_OUT_OUT_SIZE 32
#define VAD_DENSE_OUT_SIZE 1

typedef struct {
    LinearLayer conv1;
    LinearLayer conv2;
    LinearLayer gru1_input;
    LinearLayer gru1_recurrent;
    LinearLayer gru2_input;
    LinearLayer gru2_recurrent;
    LinearLayer gru3_input;
    LinearLayer gru3_recurrent;
    LinearLayer dense_out;
    LinearLayer vad_dense;
} RNNoise;

int init_rnnoise(RNNoise *model, const WeightArray *arrays);

#endif /* RNNOISE_DATA_H */
")

target_include_directories(rnnoise_static PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/rnnoise_config")

set_target_properties(rnnoise_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Silence known upstream warnings so NereusSDR's -Wall / -Wextra don't fire
# on vendored third-party code.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rnnoise_static PRIVATE
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-deprecated-declarations
        -Wno-implicit-function-declaration)
endif()
