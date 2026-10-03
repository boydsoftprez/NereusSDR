cmake_minimum_required(VERSION 3.20)
project(nereus_wdsp210_native_asan C)

get_filename_component(NEREUS_SOURCE_DEFAULT
  "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(NEREUS_SOURCE "${NEREUS_SOURCE_DEFAULT}" CACHE PATH
  "NereusSDR source tree")
set(NEREUS_BUILD "${NEREUS_SOURCE}/build" CACHE PATH
  "Existing NereusSDR build containing rnnoise and libspecbleach")

add_library(rnnoise_static STATIC IMPORTED GLOBAL)
set_target_properties(rnnoise_static PROPERTIES
  IMPORTED_LOCATION "${NEREUS_BUILD}/third_party/rnnoise/librnnoise_static.a"
  INTERFACE_INCLUDE_DIRECTORIES "${NEREUS_BUILD}/_deps/rnnoise_upstream-src/include;${NEREUS_BUILD}/_deps/rnnoise_upstream-src/src")

add_library(specbleach_static STATIC IMPORTED GLOBAL)
set_target_properties(specbleach_static PROPERTIES
  IMPORTED_LOCATION "${NEREUS_BUILD}/third_party/libspecbleach/libspecbleach_static.a"
  INTERFACE_INCLUDE_DIRECTORIES "${NEREUS_BUILD}/_deps/libspecbleach_upstream-src/include")

find_path(FFTW3_INCLUDE_DIRS fftw3.h REQUIRED
  PATHS /opt/homebrew/include /usr/local/include)
find_library(FFTW3_LIBRARY NAMES fftw3 REQUIRED
  PATHS /opt/homebrew/lib /usr/local/lib)
find_library(FFTW3F_LIBRARY NAMES fftw3f REQUIRED
  PATHS /opt/homebrew/lib /usr/local/lib)
set(FFTW3_LIBRARIES "${FFTW3_LIBRARY};${FFTW3F_LIBRARY}")

add_subdirectory("${NEREUS_SOURCE}/third_party/wdsp" wdsp)
get_target_property(WDSP_ASAN_SOURCES wdsp_static SOURCES)
list(LENGTH WDSP_ASAN_SOURCES WDSP_ASAN_SOURCE_COUNT)
message(STATUS "ASan-instrumented WDSP sources: ${WDSP_ASAN_SOURCE_COUNT}")
target_compile_options(wdsp_static PRIVATE
  -fsanitize=address -fno-sanitize-recover=address
  -fno-omit-frame-pointer -O1 -g)

add_executable(native_asan_probe src/native_asan_probe.c)
target_compile_options(native_asan_probe PRIVATE
  -fsanitize=address -fno-sanitize-recover=address
  -fno-omit-frame-pointer -O1 -g)
target_link_options(native_asan_probe PRIVATE
  -fsanitize=address -fno-sanitize-recover=address)
target_link_libraries(native_asan_probe PRIVATE wdsp_static)
