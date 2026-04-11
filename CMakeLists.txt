cmake_minimum_required(VERSION 3.20)
project(NereusSDR VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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
# FFTW3 (for client-side FFT — spectrum and waterfall)
# --------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(WIN32)
    set(FFTW3_ROOT "${CMAKE_SOURCE_DIR}/third_party/fftw3")
    if(EXISTS "${FFTW3_ROOT}/include/fftw3.h")
        set(FFTW3_FOUND TRUE)
        set(FFTW3_INCLUDE_DIRS "${FFTW3_ROOT}/include")
        set(FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        set(FFTW3_DLL "${FFTW3_ROOT}/bin/libfftw3-3.dll")
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
# --------------------------------------------------------------------------
option(NEREUS_GPU_SPECTRUM "Enable GPU-accelerated spectrum/waterfall via QRhi" ON)
if(NEREUS_GPU_SPECTRUM)
    if(Qt6_VERSION VERSION_LESS "6.7")
        message(STATUS "Qt ${Qt6_VERSION} < 6.7 — GPU spectrum disabled (requires QRhiWidget)")
        set(NEREUS_GPU_SPECTRUM OFF)
    else()
        find_package(Qt6 QUIET COMPONENTS ShaderTools)
        find_package(Qt6 QUIET COMPONENTS GuiPrivate)
        if(Qt6ShaderTools_FOUND AND Qt6GuiPrivate_FOUND)
            message(STATUS "QRhi support enabled — GPU spectrum/waterfall active")
        else()
            message(STATUS "Qt6 ShaderTools or GuiPrivate not found — GPU spectrum disabled")
            set(NEREUS_GPU_SPECTRUM OFF)
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# Source files
# --------------------------------------------------------------------------
set(CORE_SOURCES
    src/core/AppSettings.cpp
    src/core/LogCategories.cpp
    src/core/SupportBundle.cpp
    src/core/RadioDiscovery.cpp
    src/core/RadioConnection.cpp
    src/core/P2RadioConnection.cpp
    src/core/ReceiverManager.cpp
    src/core/AudioEngine.cpp
    src/core/WdspEngine.cpp
    src/core/RxChannel.cpp
    src/core/FFTEngine.cpp
)

set(MODEL_SOURCES
    src/models/RadioModel.cpp
    src/models/SliceModel.cpp
    src/models/PanadapterModel.cpp
    src/models/MeterModel.cpp
    src/models/TransmitModel.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/ConnectionPanel.cpp
    src/gui/SupportDialog.cpp
    src/gui/SpectrumWidget.cpp
    src/gui/SpectrumOverlayMenu.cpp
    src/gui/widgets/FilterPassbandWidget.cpp
    src/gui/widgets/VfoWidget.cpp
    src/gui/containers/ContainerWidget.cpp
    src/gui/containers/FloatingContainer.cpp
    src/gui/containers/ContainerManager.cpp
    src/gui/containers/ContainerSettingsDialog.cpp
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
    src/gui/SpectrumOverlayPanel.cpp
    src/gui/SetupDialog.cpp
    src/gui/SetupPage.cpp
    src/gui/setup/GeneralSetupPages.cpp
    src/gui/setup/HardwareSetupPages.cpp
    src/gui/setup/AudioSetupPages.cpp
    src/gui/setup/DspSetupPages.cpp
    src/gui/setup/DisplaySetupPages.cpp
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
# Executable
# --------------------------------------------------------------------------
add_executable(NereusSDR
    src/main.cpp
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${GUI_SOURCES}
    ${RESOURCES}
)

set_target_properties(NereusSDR PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_include_directories(NereusSDR PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# --------------------------------------------------------------------------
# Linking
# --------------------------------------------------------------------------
target_link_libraries(NereusSDR PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

if(Qt6SerialPort_FOUND)
    target_link_libraries(NereusSDR PRIVATE Qt6::SerialPort)
    target_compile_definitions(NereusSDR PRIVATE HAVE_SERIALPORT)
endif()

if(Qt6WebSockets_FOUND)
    target_link_libraries(NereusSDR PRIVATE Qt6::WebSockets)
    target_compile_definitions(NereusSDR PRIVATE HAVE_WEBSOCKETS)
endif()

if(FFTW3_FOUND)
    target_include_directories(NereusSDR PRIVATE ${FFTW3_INCLUDE_DIRS})
    target_link_libraries(NereusSDR PRIVATE ${FFTW3_LIBRARIES})
    target_compile_definitions(NereusSDR PRIVATE HAVE_FFTW3)
endif()

if(WDSP_FOUND)
    add_subdirectory(third_party/wdsp)
    target_link_libraries(NereusSDR PRIVATE wdsp_static)
    target_compile_definitions(NereusSDR PRIVATE HAVE_WDSP)
endif()

if(NEREUS_GPU_SPECTRUM)
    target_link_libraries(NereusSDR PRIVATE Qt6::GuiPrivate)
    target_compile_definitions(NereusSDR PRIVATE NEREUS_GPU_SPECTRUM)

    if(Qt6ShaderTools_FOUND)
        qt_add_shaders(NereusSDR "nereus_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/waterfall.vert
                resources/shaders/waterfall.frag
                resources/shaders/spectrum.vert
                resources/shaders/spectrum.frag
                resources/shaders/overlay.vert
                resources/shaders/overlay.frag
        )

        qt_add_shaders(NereusSDR "nereus_meter_shaders"
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
target_compile_definitions(NereusSDR PRIVATE
    NEREUSSDR_VERSION="${PROJECT_VERSION}"
)

# --------------------------------------------------------------------------
# Compiler warnings
# --------------------------------------------------------------------------
if(MSVC)
    target_compile_options(NereusSDR PRIVATE /W3 /Zc:__cplusplus)
else()
    target_compile_options(NereusSDR PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(NereusSDR PRIVATE -fsanitize=address)
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
