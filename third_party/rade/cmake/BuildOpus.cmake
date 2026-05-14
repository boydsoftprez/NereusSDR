# NereusSDR vendored patches in this file (vs. upstream radae_nopy
# at b289102, see third_party/rade/VERSION.txt):
#   1. BUILD_COMMAND $(MAKE) -> make
#      Make-only $-escape; Ninja can't parse it.
#   2. BUILD_BYPRODUCTS added to ExternalProject_Add(build_opus)
#      Required so Ninja knows libopus.a is produced by build_opus
#      and the consumer link step can wait on it.
#   3. PATCH_COMMAND ${CMAKE_SOURCE_DIR} -> ${CMAKE_CURRENT_LIST_DIR}/..
#      Upstream assumes standalone build (CMAKE_SOURCE_DIR == radae_nopy
#      root). When nested via add_subdirectory, CMAKE_SOURCE_DIR is the
#      parent project root; CMAKE_CURRENT_LIST_DIR works in both modes.
#   4. Windows branch: build Opus via its native CMakeLists.txt instead
#      of autotools. cmd.exe can't run ./autogen.sh and the default
#      Windows CI runner image has no autoconf/automake/libtool. Opus
#      ships both autogen.bat + dnn/download_model.bat (Windows-native
#      model download via PowerShell) AND a full CMakeLists.txt with
#      OPUS_DRED and OPUS_OSCE options, so the Windows path uses CMake
#      to bypass autotools entirely. Linux + non-universal macOS keep
#      the autotools chain.
# All four are noted at their site as well. See Task A2 of
# docs/architecture/phase3j2-3r-spots-and-rade-design.md.

message(STATUS "Will build opus with FARGAN")

set(CONFIGURE_COMMAND ./autogen.sh && ./configure --with-pic --enable-osce --enable-dred --disable-shared --disable-doc --disable-extra-programs)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

if (NOT DEFINED OPUS_URL)
set(OPUS_URL https://github.com/xiph/opus/archive/940d4e5af64351ca8ba8390df3f555484c567fbb.zip)
endif (NOT DEFINED OPUS_URL)
message(STATUS "Using Opus from ${OPUS_URL}")

include(ExternalProject)
if(APPLE AND BUILD_OSX_UNIVERSAL)
# Opus ./configure doesn't behave properly when built as a universal binary;
# build it twice and use lipo to create a universal libopus.a instead.
ExternalProject_Add(build_opus_x86
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    PATCH_COMMAND sh -c "patch dnn/nnet.h < ${CMAKE_CURRENT_LIST_DIR}/../src/opus-nnet.h.diff"
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=x86_64-apple-darwin --target=x86_64-apple-darwin CFLAGS=-arch\ x86_64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND make
    # NereusSDR vendored patch: upstream uses $(MAKE), which Ninja
    # parses as a literal-dollar escape error ("bad $-escape"). $(MAKE)
    # is a Makefile-specific variable; replacing with plain `make`
    # works for both Ninja and Make top-level generators. See Task A2
    # of phase3j2-3r-spots-and-rade-design.md.
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)
ExternalProject_Add(build_opus_arm
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    PATCH_COMMAND sh -c "patch dnn/nnet.h < ${CMAKE_CURRENT_LIST_DIR}/../src/opus-nnet.h.diff"
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=aarch64-apple-darwin --target=aarch64-apple-darwin CFLAGS=-arch\ arm64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND make
    # NereusSDR vendored patch: upstream uses $(MAKE), which Ninja
    # parses as a literal-dollar escape error ("bad $-escape"). $(MAKE)
    # is a Makefile-specific variable; replacing with plain `make`
    # works for both Ninja and Make top-level generators. See Task A2
    # of phase3j2-3r-spots-and-rade-design.md.
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)

ExternalProject_Get_Property(build_opus_arm BINARY_DIR)
ExternalProject_Get_Property(build_opus_arm SOURCE_DIR)
set(OPUS_ARM_BINARY_DIR ${BINARY_DIR})
ExternalProject_Get_Property(build_opus_x86 BINARY_DIR)
set(OPUS_X86_BINARY_DIR ${BINARY_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND lipo ${OPUS_ARM_BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} ${OPUS_X86_BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} -output ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} -create
    DEPENDS build_opus_arm build_opus_x86)

add_custom_target(
    libopus.a
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX})

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include ${SOURCE_DIR})

add_library(opus STATIC IMPORTED)
add_dependencies(opus libopus.a)
set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

elseif(APPLE AND CMAKE_OSX_ARCHITECTURES)

# NereusSDR vendored patch (macOS single-arch cross-compile):
# release.yml does NOT set BUILD_OSX_UNIVERSAL — each macOS row
# builds for a single arch (matrix.arch ∈ {x86_64, arm64}) and
# passes -DCMAKE_OSX_ARCHITECTURES=<arch>. The plain autotools
# `else` branch below ignores CMAKE_OSX_ARCHITECTURES, so on the
# macos-15 (arm64) runner the Intel row produced arm64 Opus
# objects and the NereusSDR link failed with
#   "ignoring file libopus.a: found arch 'arm64', required 'x86_64'"
# Pass --host/--target plus CFLAGS=-arch X to autoconf so Opus
# builds for the requested arch regardless of runner host arch.
# Mirrors the per-arch CONFIGURE_COMMAND extension that the
# `APPLE AND BUILD_OSX_UNIVERSAL` branch uses for its two
# sub-builds. See Task A2 of phase3j2-3r-spots-and-rade-design.md.
#
# Gate uses bare `CMAKE_OSX_ARCHITECTURES` (truthy-test) rather than
# `DEFINED CMAKE_OSX_ARCHITECTURES` so an empty-string value (ci.yml
# does not pass -DCMAKE_OSX_ARCHITECTURES, leaving it defined-but-empty)
# falls through to the plain autotools `else` branch instead of
# hitting the FATAL_ERROR below.

list(LENGTH CMAKE_OSX_ARCHITECTURES _osx_arch_count)
if(_osx_arch_count GREATER 1)
    message(FATAL_ERROR
        "BuildOpus.cmake: multi-arch CMAKE_OSX_ARCHITECTURES (${CMAKE_OSX_ARCHITECTURES}) "
        "requires BUILD_OSX_UNIVERSAL=ON path; got single-arch path. Set BUILD_OSX_UNIVERSAL "
        "or pick a single arch.")
endif()

if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(_opus_host_triple "x86_64-apple-darwin")
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(_opus_host_triple "aarch64-apple-darwin")
else()
    message(FATAL_ERROR "BuildOpus.cmake: unsupported CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
endif()

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

set(_opus_binary_dir ${CMAKE_CURRENT_BINARY_DIR}/build_opus-prefix/src/build_opus)
set(_opus_static_lib ${_opus_binary_dir}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(build_opus
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    PATCH_COMMAND sh -c "patch dnn/nnet.h < ${CMAKE_CURRENT_LIST_DIR}/../src/opus-nnet.h.diff"
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
        --host=${_opus_host_triple}
        --target=${_opus_host_triple}
        CFLAGS=-arch\ ${CMAKE_OSX_ARCHITECTURES}\ -O2\ -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}
    BUILD_COMMAND make
    BUILD_BYPRODUCTS ${_opus_static_lib}
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)

ExternalProject_Get_Property(build_opus BINARY_DIR)
ExternalProject_Get_Property(build_opus SOURCE_DIR)
add_library(opus STATIC IMPORTED)
add_dependencies(opus build_opus)

set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${_opus_static_lib}"
    IMPORTED_IMPLIB   "${_opus_static_lib}"
)

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include ${SOURCE_DIR})

elseif(WIN32)

# NereusSDR vendored patch (Windows): bypass autotools, use Opus's
# native CMake build. cmd.exe can't run ./autogen.sh and the default
# Windows CI runner image has no autoconf/automake/libtool. Opus's
# CMakeLists.txt at the pinned commit (940d4e5af6) supports
# OPUS_DRED + OPUS_OSCE (the FARGAN/DRED neural enhancers RADE needs),
# and dnn/download_model.bat fetches the model via PowerShell instead
# of wget. See patch (4) at the top of this file.

# Model hash must stay in sync with autogen.sh in the pinned Opus
# commit (verify: gh api repos/xiph/opus/contents/autogen.sh?ref=940d4e5).
set(_opus_model_sha "4ed9445b96698bad25d852e912b41495ddfa30c8dbc8a55f9cde5826ed793453")

# Git for Windows (preinstalled on the Actions windows-latest image)
# bundles patch.exe under usr/bin. Add hints so find_program locates
# it even when Git's usr/bin is not on PATH (it isn't, by default —
# only Git's cmd/ dir is on the runner's PATH).
find_program(PATCH_EXECUTABLE
    NAMES patch
    HINTS
        "C:/Program Files/Git/usr/bin"
        "C:/Program Files (x86)/Git/usr/bin"
    REQUIRED)

# CMake-built Opus (no BUILD_IN_SOURCE) puts the static lib in a
# separate build tree under build_opus-prefix/src/build_opus-build/.
set(_opus_binary_dir ${CMAKE_CURRENT_BINARY_DIR}/build_opus-prefix/src/build_opus-build)
set(_opus_static_lib ${_opus_binary_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}opus${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(build_opus
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    URL ${OPUS_URL}
    # Two-step patch: (1) fetch DRED/OSCE model tarball + extract into
    # dnn/ via the upstream Windows-native script; (2) apply the
    # NereusSDR opus-nnet.h.diff that adds RADE_EXPORT visibility tags
    # to nnet.h. Both are required before configure.
    #
    # The download_model.bat invocation runs from <SOURCE_DIR> (NOT
    # <SOURCE_DIR>/dnn) because the tarball's internal paths all begin
    # with "dnn/" — extracting from <SOURCE_DIR>/dnn would land files
    # at <SOURCE_DIR>/dnn/dnn/... and CMake configure would fail
    # looking for dnn/fargan_data.h. This matches upstream autogen.bat
    # which also runs the bat from the source root.
    PATCH_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> cmd /c dnn\\download_model.bat ${_opus_model_sha}
        COMMAND ${PATCH_EXECUTABLE} <SOURCE_DIR>/dnn/nnet.h -i ${CMAKE_CURRENT_LIST_DIR}/../src/opus-nnet.h.diff
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DOPUS_DRED=ON
        -DOPUS_OSCE=ON
        -DOPUS_BUILD_SHARED_LIBRARY=OFF
        -DOPUS_BUILD_PROGRAMS=OFF
        -DOPUS_BUILD_TESTING=OFF
        -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF
        -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF
    BUILD_BYPRODUCTS ${_opus_static_lib}
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_opus SOURCE_DIR)
add_library(opus STATIC IMPORTED)
add_dependencies(opus build_opus)

set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${_opus_static_lib}"
    IMPORTED_IMPLIB   "${_opus_static_lib}"
)

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include ${SOURCE_DIR})

else(APPLE AND BUILD_OSX_UNIVERSAL)

# Disable Opus CPU feature detection when crosscompiling for ARM due to
# compiler issues building Windows for ARM version.
if (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --disable-rtcd)
endif (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")

# NereusSDR vendored patch: pre-compute BINARY_DIR so we can declare
# BUILD_BYPRODUCTS. Ninja needs to know which file ExternalProject
# produces, otherwise it cannot wire the dependency from the consumer
# (librade.dylib) back to build_opus, and link fails with
# "missing and no known rule to make it". Make-generators don't need
# this. See Task A2 of phase3j2-3r-spots-and-rade-design.md.
set(_opus_binary_dir ${CMAKE_CURRENT_BINARY_DIR}/build_opus-prefix/src/build_opus)
set(_opus_static_lib ${_opus_binary_dir}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(build_opus
    BUILD_IN_SOURCE 1
    PATCH_COMMAND sh -c "patch dnn/nnet.h < ${CMAKE_CURRENT_LIST_DIR}/../src/opus-nnet.h.diff"
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND make
    # NereusSDR vendored patch: upstream uses $(MAKE), which Ninja
    # parses as a literal-dollar escape error ("bad $-escape"). $(MAKE)
    # is a Makefile-specific variable; replacing with plain `make`
    # works for both Ninja and Make top-level generators. See Task A2
    # of phase3j2-3r-spots-and-rade-design.md.
    BUILD_BYPRODUCTS ${_opus_static_lib}
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)

ExternalProject_Get_Property(build_opus BINARY_DIR)
ExternalProject_Get_Property(build_opus SOURCE_DIR)
add_library(opus STATIC IMPORTED)
add_dependencies(opus build_opus)

set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include ${SOURCE_DIR})
endif(APPLE AND BUILD_OSX_UNIVERSAL)
