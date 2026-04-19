cmake_minimum_required(VERSION 3.20)
project(NereusSDR VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / IDE integration

# macOS: ensure Homebrew lib/include paths are in the search path
if(APPLE)
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOMEBREW_PREFIX)
        link_directories("${HOMEBREW_PREFIX}/lib")
        include_directories("${HOMEBREW_PREFIX}/include")
    endif()
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# --------------------------------------------------------------------------
# Qt6 components
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Multimedia
)

# Optional Qt components
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(Qt6SerialPort_FOUND)
    message(STATUS "Qt6::SerialPort found — serial PTT/CW support enabled")
else()
    message(STATUS "Qt6::SerialPort not found — serial PTT/CW support disabled")
endif()

find_package(Qt6 QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    message(STATUS "Qt6::WebSockets found — TCI server enabled")
else()
    message(STATUS "Qt6::WebSockets not found — TCI server disabled")
endif()

# --------------------------------------------------------------------------
# PortAudio v19 (Phase 3O VAX — PortAudioBus fallback for IAudioBus)
# --------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG        v19.7.0  # verified cmake-clean release
)

# PortAudio build options — turn off ASIO. Direct ASIO support was
# dropped during the 2026-04-19 GPL compliance review (see plan §"GPL
# Compliance Review"); PortAudio's built-in ASIO host API is gated off
# for the same reason (the Steinberg ASIO SDK license is
# GPL-incompatible). All other PortAudio host APIs remain enabled:
# MME / DS / WDM-KS / WASAPI on Windows, CoreAudio on macOS,
# ALSA / Pulse / PipeWire / JACK on Linux.
set(PA_BUILD_SHARED OFF CACHE BOOL "PortAudio static" FORCE)
set(PA_BUILD_STATIC ON  CACHE BOOL "PortAudio static" FORCE)
set(PA_USE_ASIO    OFF CACHE BOOL "PortAudio ASIO host API — forced OFF for GPL compliance (Steinberg SDK incompatible)" FORCE)

# PortAudio v19.7.0 uses `cmake_minimum_required(VERSION 2.8)`, which CMake
# 4.x rejects because compatibility with CMake < 3.5 has been removed. Pin
# the minimum policy version for the nested project so FetchContent can
# configure it under modern CMake without touching the vendored source.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for vendored subprojects" FORCE)

FetchContent_MakeAvailable(portaudio)

# --------------------------------------------------------------------------
# FFTW3 (for client-side FFT — spectrum and waterfall)
# --------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(WIN32)
    set(FFTW3_ROOT "${CMAKE_SOURCE_DIR}/third_party/fftw3")

    # The double-precision FFTW DLL is GPLv2 and gitignored
    # (third_party/fftw3/.gitignore), so a fresh Windows clone doesn't have
    # it. Auto-fetch from fftw.org on first configure (same URL release.yml
    # and ci.yml use). Without this, a clean Windows build fails with a
    # cryptic "No rule to make target libfftw3-3.dll" link error.
    if(NOT EXISTS "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        set(_fftw_url "https://fftw.org/pub/fftw/fftw-3.3.5-dll64.zip")
        set(_fftw_zip "${CMAKE_BINARY_DIR}/fftw-3.3.5-dll64.zip")
        set(_fftw_tmp "${CMAKE_BINARY_DIR}/fftw-tmp")
        message(STATUS "FFTW3: libfftw3-3.dll missing, fetching ${_fftw_url}")
        file(DOWNLOAD "${_fftw_url}" "${_fftw_zip}"
             STATUS _fftw_dl_status
             TLS_VERIFY ON
             SHOW_PROGRESS)
        list(GET _fftw_dl_status 0 _fftw_dl_code)
        if(NOT _fftw_dl_code EQUAL 0)
            list(GET _fftw_dl_status 1 _fftw_dl_msg)
            message(FATAL_ERROR
                "FFTW3 download failed (${_fftw_dl_msg}).\n"
                "Manually download ${_fftw_url}, then extract:\n"
                "  fftw3.h            -> ${FFTW3_ROOT}/include/\n"
                "  libfftw3-3.dll     -> ${FFTW3_ROOT}/bin/\n"
                "  libfftw3-3.def     -> ${FFTW3_ROOT}/lib/\n"
                "and rerun CMake.")
        endif()
        file(MAKE_DIRECTORY "${_fftw_tmp}")
        file(ARCHIVE_EXTRACT INPUT "${_fftw_zip}" DESTINATION "${_fftw_tmp}")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/include")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/bin")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/lib")
        file(COPY "${_fftw_tmp}/fftw3.h"        DESTINATION "${FFTW3_ROOT}/include")
        file(COPY "${_fftw_tmp}/libfftw3-3.dll" DESTINATION "${FFTW3_ROOT}/bin")
        file(COPY "${_fftw_tmp}/libfftw3-3.def" DESTINATION "${FFTW3_ROOT}/lib")
        message(STATUS "FFTW3: fetched libfftw3-3.dll into ${FFTW3_ROOT}/bin/")
    endif()

    if(EXISTS "${FFTW3_ROOT}/include/fftw3.h")
        set(FFTW3_FOUND TRUE)
        set(FFTW3_INCLUDE_DIRS "${FFTW3_ROOT}/include")
        set(FFTW3_DLL "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        # Prefer a proper .lib import library when available. MSVC link.exe
        # refuses to link against a DLL directly (LNK1107 "invalid or
        # corrupt file"), while MinGW's ld is happy either way. release.yml
        # generates third_party/fftw3/lib/fftw3.lib from the .def file;
        # ci.yml (MinGW) skips that and links the DLL directly.
        if(EXISTS "${FFTW3_ROOT}/lib/fftw3.lib")
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/fftw3.lib")
            message(STATUS "FFTW3: using import library ${FFTW3_ROOT}/lib/fftw3.lib")
        else()
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3-3.dll")
            message(STATUS "FFTW3: linking directly against DLL (no fftw3.lib)")
        endif()
        # Float precision FFTW3 for FFTEngine display FFT
        if(EXISTS "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            set(FFTW3F_DLL "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            message(STATUS "FFTW3F (float) found — FFTEngine enabled")
        elseif(EXISTS "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            set(FFTW3F_DLL "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            message(STATUS "FFTW3F (float) found — FFTEngine enabled (DLL direct)")
        endif()
    else()
        message(STATUS "FFTW3 not found at ${FFTW3_ROOT}. Client-side FFT disabled.")
    endif()
else()
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3 fftw3)
    endif()
    if(NOT FFTW3_FOUND)
        find_library(FFTW3_LIB fftw3)
        find_path(FFTW3_INC fftw3.h)
        if(FFTW3_LIB AND FFTW3_INC)
            set(FFTW3_FOUND TRUE)
            set(FFTW3_LIBRARIES ${FFTW3_LIB})
            set(FFTW3_INCLUDE_DIRS ${FFTW3_INC})
        endif()
    endif()
endif()
if(FFTW3_FOUND)
    message(STATUS "FFTW3 found — client-side FFT enabled")
    # Also find fftw3f (single-precision) for FFTEngine display FFT
    if(NOT WIN32)
        find_library(FFTW3F_LIB fftw3f HINTS ${FFTW3_LIBRARY_DIRS})
        if(FFTW3F_LIB)
            list(APPEND FFTW3_LIBRARIES ${FFTW3F_LIB})
            message(STATUS "FFTW3F (float) found — FFTEngine enabled")
        endif()
    endif()
else()
    message(STATUS "FFTW3 not found — client-side FFT disabled")
endif()

# --------------------------------------------------------------------------
# WDSP DSP library (required for signal processing)
# --------------------------------------------------------------------------
option(ENABLE_WDSP "Build with WDSP DSP engine" ON)
if(ENABLE_WDSP)
    set(WDSP_DIR ${CMAKE_SOURCE_DIR}/third_party/wdsp)
    if(EXISTS "${WDSP_DIR}/src/comm.h")
        set(WDSP_FOUND TRUE)
        message(STATUS "WDSP found — DSP engine enabled")
    else()
        set(WDSP_FOUND FALSE)
        message(STATUS "WDSP not found at ${WDSP_DIR} — DSP engine disabled (stub mode)")
    endif()
endif()

# On Windows, generate FFTW3 import library from .def if .lib is missing
if(WIN32 AND FFTW3_FOUND AND NOT EXISTS "${FFTW3_ROOT}/lib/fftw3.lib")
    find_program(MSVC_LIB lib.exe)
    if(MSVC_LIB)
        execute_process(
            COMMAND ${MSVC_LIB} /def:${FFTW3_ROOT}/lib/libfftw3-3.def
                    /machine:x64 /out:${FFTW3_ROOT}/lib/fftw3.lib
            WORKING_DIRECTORY ${FFTW3_ROOT}/lib
            RESULT_VARIABLE LIB_RESULT
        )
        if(LIB_RESULT EQUAL 0)
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/fftw3.lib")
            message(STATUS "Generated FFTW3 import library")
        endif()
    else()
        # Fallback: link directly against DLL (MinGW compatible)
        set(FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        # Re-append float import library if available
        if(EXISTS "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/libfftw3f-3.a")
        elseif(EXISTS "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
        endif()
        message(STATUS "FFTW3: linking directly against DLL (lib.exe not found)")
    endif()
endif()

# --------------------------------------------------------------------------
# GPU-accelerated spectrum/waterfall (QRhi — requires Qt 6.7+)
#
# History: v0.1.1 shipped a Windows installer that silently disabled
# GPU spectrum because this block used `message(STATUS)` fallbacks when
# the private-module detection failed, and the shipping pipeline never
# explicitly requested -DNEREUS_GPU_SPECTRUM=ON. Users saw a blank
# spectrum area and no log line from SpectrumWidget::initialize().
#
# The check now does three things differently:
#   1. Probes for both a _FOUND variable AND an imported target, since
#      Qt's private-module CMake exports have varied across 6.x minor
#      releases and install-qt-action layouts.
#   2. Captures a specific reason string when either component is
#      missing so the error message can point the user at the fix.
#   3. Promotes the fallback to FATAL_ERROR when the caller opted in
#      to GPU spectrum (the default) — silently shipping a CPU-only
#      binary is the exact footgun that caused v0.1.1. Users who
#      genuinely want the CPU path must pass -DNEREUS_GPU_SPECTRUM=OFF
#      explicitly to acknowledge it.
# --------------------------------------------------------------------------
option(NEREUS_GPU_SPECTRUM "Enable GPU-accelerated spectrum/waterfall via QRhi (requires Qt 6.7+ with ShaderTools and Gui private headers)" ON)

# Remember whether the caller requested it before any auto-disable, so
# we can produce a hard failure if we can't honor the request.
set(_nereus_gpu_spectrum_requested ${NEREUS_GPU_SPECTRUM})
set(_nereus_gpu_fail_reason "")

if(NEREUS_GPU_SPECTRUM)
    if(Qt6_VERSION VERSION_LESS "6.7")
        set(_nereus_gpu_fail_reason
            "Qt ${Qt6_VERSION} is older than 6.7 (QRhiWidget requires 6.7+)")
        set(NEREUS_GPU_SPECTRUM OFF)
    else()
        # Ask CMake to locate the private modules. QUIET so we can emit
        # a single, context-rich failure message below instead of
        # multiple cryptic find_package warnings.
        find_package(Qt6 QUIET COMPONENTS ShaderTools GuiPrivate)

        # Belt-and-braces detection: accept either the _FOUND variable
        # or the imported target. install-qt-action on Windows has
        # shipped Qt 6.7/6.8 layouts where Qt6GuiPrivate_FOUND is not
        # set even though the Qt6::GuiPrivate target exists, which is
        # what silently broke v0.1.1.
        set(_nereus_have_shadertools FALSE)
        set(_nereus_have_guiprivate  FALSE)
        if(Qt6ShaderTools_FOUND OR TARGET Qt6::ShaderTools)
            set(_nereus_have_shadertools TRUE)
        endif()
        if(Qt6GuiPrivate_FOUND OR TARGET Qt6::GuiPrivate)
            set(_nereus_have_guiprivate TRUE)
        endif()

        if(_nereus_have_shadertools AND _nereus_have_guiprivate)
            message(STATUS "QRhi support enabled — GPU spectrum/waterfall active")
        else()
            set(_nereus_gpu_fail_reason "Qt6 private modules missing:")
            if(NOT _nereus_have_shadertools)
                string(APPEND _nereus_gpu_fail_reason " ShaderTools")
            endif()
            if(NOT _nereus_have_guiprivate)
                string(APPEND _nereus_gpu_fail_reason " GuiPrivate")
            endif()
            set(NEREUS_GPU_SPECTRUM OFF)
        endif()
    endif()
endif()

# Fail loudly if the caller asked for GPU spectrum and we couldn't
# deliver it. This prevents a repeat of the v0.1.1 silent-CPU-fallback
# shipping incident.
if(_nereus_gpu_spectrum_requested AND NOT NEREUS_GPU_SPECTRUM)
    message(FATAL_ERROR
        "NEREUS_GPU_SPECTRUM was requested but cannot be enabled.\n"
        "Reason: ${_nereus_gpu_fail_reason}\n"
        "Fix options:\n"
        "  - install-qt-action: ensure 'qtshadertools' is in the modules list; "
        "private headers come with qtbase by default on Linux/macOS but some "
        "install-qt-action Windows layouts need the private headers pulled in "
        "via the Qt installer.\n"
        "  - Local dev: install Qt with ShaderTools and private headers "
        "(official online installer: check 'Qt Shader Tools' and 'Qt 5 Compat' "
        "isn't needed, but private headers ship with the base Qt module).\n"
        "  - To build without GPU spectrum (CPU-only fallback), re-run cmake "
        "with -DNEREUS_GPU_SPECTRUM=OFF.")
endif()

# --------------------------------------------------------------------------
# Source files
# --------------------------------------------------------------------------
set(CORE_SOURCES
    src/core/AppSettings.cpp
    src/core/BoardCapabilities.cpp
    src/core/SampleRateCatalog.cpp
    src/core/HardwareProfile.cpp
    src/core/LogCategories.cpp
    src/core/SupportBundle.cpp
    src/core/RadioDiscovery.cpp
    src/core/RadioConnection.cpp
    src/core/P1RadioConnection.cpp
    src/core/P2RadioConnection.cpp
    src/core/ReceiverManager.cpp
    src/core/AudioEngine.cpp
    src/core/WdspEngine.cpp
    src/core/RxChannel.cpp
    src/core/FFTEngine.cpp
    src/core/NoiseFloorEstimator.cpp
    src/core/NoiseFloorTracker.cpp
    src/core/ClarityController.cpp
    src/core/StepAttenuatorController.cpp
    src/core/audio/MasterMixer.cpp
    src/core/audio/PortAudioBus.cpp
)

set(MODEL_SOURCES
    src/models/RadioModel.cpp
    src/models/RxDspWorker.cpp
    src/models/Band.cpp
    src/models/SliceModel.cpp
    src/models/PanadapterModel.cpp
    src/models/MeterModel.cpp
    src/models/TransmitModel.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/ConnectionPanel.cpp
    src/gui/AddCustomRadioDialog.cpp
    src/gui/SupportDialog.cpp
    src/gui/AboutDialog.cpp
    src/gui/SpectrumWidget.cpp
    src/gui/SpectrumOverlayMenu.cpp
    src/gui/widgets/CenterMarkSlider.h
    src/gui/widgets/FilterPassbandWidget.cpp
    src/gui/widgets/GuardedComboBox.h
    src/gui/widgets/GuardedSlider.h
    src/gui/widgets/ResetSlider.h
    src/gui/widgets/ScrollableLabel.cpp
    src/gui/widgets/TriBtn.h
    src/gui/widgets/TriBtn.cpp
    src/gui/widgets/VfoLevelBar.cpp
    src/gui/widgets/VfoModeContainers.cpp
    src/gui/widgets/VfoModeContainers.h
    src/gui/widgets/VfoStyles.h
    src/gui/widgets/VfoWidget.cpp
    src/gui/containers/ContainerWidget.cpp
    src/gui/containers/FloatingContainer.cpp
    src/gui/containers/ContainerManager.cpp
    src/gui/containers/ContainerSettingsDialog.cpp
    src/gui/containers/MmioEndpointsDialog.cpp
    src/gui/containers/MmioVariablePickerPopup.cpp
    src/core/mmio/ITransportWorker.cpp
    src/core/mmio/MmioEndpoint.cpp
    src/core/mmio/ExternalVariableEngine.cpp
    src/core/mmio/FormatParser.cpp
    src/core/mmio/UdpEndpointWorker.cpp
    src/core/mmio/TcpListenerEndpointWorker.cpp
    src/core/mmio/TcpClientEndpointWorker.cpp
    src/core/mmio/SerialEndpointWorker.cpp
    src/gui/containers/meter_property_editors/BaseItemEditor.cpp
    src/gui/containers/meter_property_editors/BarItemEditor.cpp
    src/gui/containers/meter_property_editors/SolidColourItemEditor.cpp
    src/gui/containers/meter_property_editors/SpacerItemEditor.cpp
    src/gui/containers/meter_property_editors/FadeCoverItemEditor.cpp
    src/gui/containers/meter_property_editors/ImageItemEditor.cpp
    src/gui/containers/meter_property_editors/ScaleItemEditor.cpp
    src/gui/containers/meter_property_editors/NeedleItemEditor.cpp
    src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp
    src/gui/containers/meter_property_editors/TextItemEditor.cpp
    src/gui/containers/meter_property_editors/TextOverlayItemEditor.cpp
    src/gui/containers/meter_property_editors/SignalTextItemEditor.cpp
    src/gui/containers/meter_property_editors/LedItemEditor.cpp
    src/gui/containers/meter_property_editors/HistoryGraphItemEditor.cpp
    src/gui/containers/meter_property_editors/MagicEyeItemEditor.cpp
    src/gui/containers/meter_property_editors/DialItemEditor.cpp
    src/gui/containers/meter_property_editors/WebImageItemEditor.cpp
    src/gui/containers/meter_property_editors/FilterDisplayItemEditor.cpp
    src/gui/containers/meter_property_editors/RotatorItemEditor.cpp
    src/gui/containers/meter_property_editors/ClockItemEditor.cpp
    src/gui/containers/meter_property_editors/VfoDisplayItemEditor.cpp
    src/gui/containers/meter_property_editors/ClickBoxItemEditor.cpp
    src/gui/containers/meter_property_editors/DataOutItemEditor.cpp
    src/gui/containers/meter_property_editors/ButtonBoxItemEditor.cpp
    src/gui/containers/meter_property_editors/BandButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/ModeButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/FilterButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/AntennaButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/TuneStepButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/OtherButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/VoiceRecordPlayItemEditor.cpp
    src/gui/containers/meter_property_editors/DiscordButtonItemEditor.cpp
    src/gui/meters/MeterWidget.cpp
    src/gui/meters/MeterItem.cpp
    src/gui/meters/ItemGroup.cpp
    src/gui/meters/MeterPoller.cpp
    src/gui/meters/SpacerItem.cpp
    src/gui/meters/FadeCoverItem.cpp
    src/gui/meters/LEDItem.cpp
    src/gui/meters/HistoryGraphItem.cpp
    src/gui/meters/MagicEyeItem.cpp
    src/gui/meters/NeedleScalePwrItem.cpp
    src/gui/meters/SignalTextItem.cpp
    src/gui/meters/DialItem.cpp
    src/gui/meters/TextOverlayItem.cpp
    src/gui/meters/WebImageItem.cpp
    src/gui/meters/FilterDisplayItem.cpp
    src/gui/meters/RotatorItem.cpp
    src/gui/meters/ButtonBoxItem.cpp
    src/gui/meters/BandButtonItem.cpp
    src/gui/meters/ModeButtonItem.cpp
    src/gui/meters/FilterButtonItem.cpp
    src/gui/meters/AntennaButtonItem.cpp
    src/gui/meters/TuneStepButtonItem.cpp
    src/gui/meters/OtherButtonItem.cpp
    src/gui/meters/VoiceRecordPlayItem.cpp
    src/gui/meters/DiscordButtonItem.cpp
    src/gui/meters/VfoDisplayItem.cpp
    src/gui/meters/ClockItem.cpp
    src/gui/meters/ClickBoxItem.cpp
    src/gui/meters/DataOutItem.cpp
    src/gui/HGauge.cpp
    src/gui/ColorSwatchButton.cpp
    src/gui/SpectrumOverlayPanel.cpp
    src/gui/SetupDialog.cpp
    src/gui/SetupPage.cpp
    src/gui/setup/GeneralSetupPages.cpp
    src/gui/setup/GeneralOptionsPage.cpp
    src/gui/setup/HardwarePage.cpp
    src/gui/setup/hardware/RadioInfoTab.cpp
    src/gui/setup/hardware/AntennaAlexTab.cpp
    src/gui/setup/hardware/OcOutputsTab.cpp
    src/gui/setup/hardware/XvtrTab.cpp
    src/gui/setup/hardware/PureSignalTab.cpp
    src/gui/setup/hardware/DiversityTab.cpp
    src/gui/setup/hardware/PaCalibrationTab.cpp
    src/gui/setup/hardware/Hl2IoBoardTab.cpp
    src/gui/setup/hardware/BandwidthMonitorTab.cpp
    src/gui/setup/AudioSetupPages.cpp
    src/gui/setup/DspSetupPages.cpp
    src/gui/setup/DisplaySetupPages.cpp
    src/gui/setup/SetupHelpers.cpp
    src/gui/setup/TransmitSetupPages.cpp
    src/gui/setup/AppearanceSetupPages.cpp
    src/gui/setup/CatNetworkSetupPages.cpp
    src/gui/setup/KeyboardSetupPages.cpp
    src/gui/setup/DiagnosticsSetupPages.cpp
    src/gui/applets/AppletWidget.cpp
    src/gui/applets/NyiOverlay.cpp
    src/gui/applets/AppletPanelWidget.cpp
    src/gui/applets/RxApplet.cpp
    src/gui/applets/TxApplet.cpp
    src/gui/applets/PhoneCwApplet.cpp
    src/gui/applets/EqApplet.cpp
    src/gui/applets/FmApplet.cpp
    src/gui/applets/DigitalApplet.cpp
    src/gui/applets/PureSignalApplet.cpp
    src/gui/applets/DiversityApplet.cpp
    src/gui/applets/CwxApplet.cpp
    src/gui/applets/DvkApplet.cpp
    src/gui/applets/CatApplet.cpp
    src/gui/applets/TunerApplet.cpp
)

# --------------------------------------------------------------------------
# Resources
# --------------------------------------------------------------------------
qt_add_resources(RESOURCES resources.qrc)

# --------------------------------------------------------------------------
# App icon (per-platform) — from main (PR #11)
# --------------------------------------------------------------------------
set(APP_ICON_SOURCES "")
if(APPLE)
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/NereusSDR.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND APP_ICON_SOURCES ${APP_ICON_MACOS})
elseif(WIN32)
    list(APPEND APP_ICON_SOURCES "${CMAKE_SOURCE_DIR}/resources/icons/NereusSDR.rc")
endif()

# --------------------------------------------------------------------------
# Object library — shared between the app executable and test targets so
# the same translation units are compiled once and consumed by both
# (Phase 3I test infrastructure). Properties on NereusSDRObjs propagate
# to consumers (PUBLIC) via the CMake INTERFACE; the thin executable just
# links src/main.cpp on top. Bundles the per-platform APP_ICON_SOURCES so
# the icon lands in both the .app and the test binaries.
# --------------------------------------------------------------------------
add_library(NereusSDRObjs OBJECT
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${GUI_SOURCES}
    ${RESOURCES}
    ${APP_ICON_SOURCES}
)

add_executable(NereusSDR src/main.cpp)
target_link_libraries(NereusSDR PRIVATE NereusSDRObjs)

set_target_properties(NereusSDR PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "NereusSDR.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "NereusSDR"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.boydsoftprez.NereusSDR"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "© 2026 J.J. Boyd (KG4VCF); GPL port of Thetis (ramdor/Thetis) and mi0bot/OpenHPSDR-Thetis; descends from FlexRadio PowerSDR lineage. GPLv3."
)

target_include_directories(NereusSDRObjs PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# --------------------------------------------------------------------------
# Linking
# --------------------------------------------------------------------------
target_link_libraries(NereusSDRObjs PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

if(WIN32)
    # Needed by RadioDiscovery.cpp's setsockopt() call for SO_BROADCAST.
    target_link_libraries(NereusSDR PRIVATE ws2_32)
endif()

if(Qt6SerialPort_FOUND)
    target_link_libraries(NereusSDRObjs PUBLIC Qt6::SerialPort)
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_SERIALPORT)
endif()

if(Qt6WebSockets_FOUND)
    target_link_libraries(NereusSDRObjs PUBLIC Qt6::WebSockets)
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_WEBSOCKETS)
endif()

if(FFTW3_FOUND)
    target_include_directories(NereusSDRObjs PUBLIC ${FFTW3_INCLUDE_DIRS})
    target_link_libraries(NereusSDRObjs PUBLIC ${FFTW3_LIBRARIES})
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_FFTW3)
endif()

if(WDSP_FOUND)
    add_subdirectory(third_party/wdsp)
    target_link_libraries(NereusSDRObjs PUBLIC wdsp_static)
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_WDSP)
endif()

# PortAudio (Phase 3O VAX) — always linked; PortAudioBus is the cross-platform
# IAudioBus fallback when no native low-latency backend is available.
target_link_libraries(NereusSDRObjs PRIVATE portaudio_static)
target_include_directories(NereusSDRObjs PUBLIC ${portaudio_SOURCE_DIR}/include)

if(NEREUS_GPU_SPECTRUM)
    target_link_libraries(NereusSDRObjs PUBLIC Qt6::GuiPrivate)
    target_compile_definitions(NereusSDRObjs PUBLIC NEREUS_GPU_SPECTRUM)

    if(Qt6ShaderTools_FOUND)
        qt_add_shaders(NereusSDRObjs "nereus_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/waterfall.vert
                resources/shaders/waterfall.frag
                resources/shaders/spectrum.vert
                resources/shaders/spectrum.frag
                resources/shaders/overlay.vert
                resources/shaders/overlay.frag
        )

        qt_add_shaders(NereusSDRObjs "nereus_meter_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/meter_textured.vert
                resources/shaders/meter_textured.frag
                resources/shaders/meter_geometry.vert
                resources/shaders/meter_geometry.frag
        )
    endif()
endif()

# --------------------------------------------------------------------------
# Version define
# --------------------------------------------------------------------------
target_compile_definitions(NereusSDRObjs PUBLIC
    NEREUSSDR_VERSION="${PROJECT_VERSION}"
)

# --------------------------------------------------------------------------
# Compiler warnings
# --------------------------------------------------------------------------
if(MSVC)
    target_compile_options(NereusSDRObjs PRIVATE /W3 /Zc:__cplusplus)
else()
    target_compile_options(NereusSDRObjs PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(NereusSDRObjs PUBLIC -fsanitize=address)
        target_link_options(NereusSDR PRIVATE -fsanitize=address)
    endif()
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS NereusSDR
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)

# Bundle third-party license notices + NereusSDR's LICENSE into release artifacts.
# LICENSE-DUAL-LICENSING is shipped alongside LICENSE because Thetis maintains
# both at project root and GPL §1 "keep intact all notices that refer to this
# License" extends to every license-related file upstream, not only the main
# LICENSE text. A binary recipient obtaining the source is owed both files.
install(DIRECTORY packaging/third-party-licenses/
        DESTINATION share/doc/nereussdr/licenses
        COMPONENT licenses)
install(FILES LICENSE LICENSE-DUAL-LICENSING
        DESTINATION share/doc/nereussdr
        COMPONENT licenses)

# --------------------------------------------------------------------------
# Tests (Qt6::Test). Opt-in via -DNEREUS_BUILD_TESTS=ON.
# --------------------------------------------------------------------------
option(NEREUS_BUILD_TESTS "Build NereusSDR test suite" OFF)
if(NEREUS_BUILD_TESTS)
    # Propagate the define to the object library so test-guard blocks
    # (e.g. isTabVisibleForTest) are compiled into NereusSDRObjs and
    # therefore available for linking into test executables.
    target_compile_definitions(NereusSDRObjs PUBLIC NEREUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
