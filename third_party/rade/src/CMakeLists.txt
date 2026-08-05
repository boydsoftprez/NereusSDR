include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# NereusSDR vendored patch: gate libm linking on non-Windows. MSVC has
# no separate `m.lib` (math functions live in the default CRT); CMake
# resolves `target_link_libraries(... m)` to `m.lib` on Windows and
# fails with LNK1181: cannot open input file 'm.lib'. Use a generator
# expression so Linux/macOS keep the explicit libm link unchanged and
# Windows drops it.
set(RADE_LIBM $<$<NOT:$<PLATFORM_ID:Windows>>:m>)

# LPCNet feature extraction tool
add_executable(lpcnet_demo lpcnet_demo.c)
target_link_libraries(lpcnet_demo opus ${RADE_LIBM})

# DSP source files
set(RADE_DSP_SOURCES
    rade_dsp.c
    rade_ofdm.c
    rade_bpf.c
    rade_acq.c
    rade_tx.c
    rade_rx.c
)

# RADE library (Python-free)
add_library(rade SHARED
    rade_api_nopy.c
    rade_enc.c
    rade_dec.c
    rade_enc_data.c
    rade_dec_data.c
    ${RADE_DSP_SOURCES}
)
target_link_libraries(rade opus ${RADE_LIBM})
target_compile_definitions(rade PRIVATE -DIS_BUILDING_RADE_API=1 -DRADE_PYTHON_FREE=1)

set_target_properties(rade PROPERTIES
    VERSION 0.1
    SOVERSION 0.1
    PUBLIC_HEADER "rade_api.h"
)

if(NOT WIN32)

# Standalone transmitter
add_executable(radae_tx radae_tx_nopy.c)
target_link_libraries(radae_tx rade opus m)

# Standalone receiver
add_executable(radae_rx radae_rx_nopy.c)
target_link_libraries(radae_rx rade opus m)

# Utility: convert real baseband to complex IQ
add_executable(real2iq real2iq.c)
target_link_libraries(real2iq m)

# WAV demodulator: WAV in → demod → WAV out
add_executable(rade_demod_wav rade_demod_wav.c)
target_link_libraries(rade_demod_wav rade opus m)

# WAV modulator: WAV in → features → RADE encode → OFDM mod → WAV out
add_executable(rade_modulate_wav rade_modulate_wav.c)
target_link_libraries(rade_modulate_wav rade opus m)

# BPF test tool
add_executable(rade_bpf_test rade_bpf_test.c kiss_fft.c kiss_fftr.c)
target_link_libraries(rade_bpf_test rade opus m)

# BER test tool: QPSK symbol BER test for modem equalisation code
add_executable(rade_ber_test rade_ber_test.c)
target_link_libraries(rade_ber_test rade m)

# Callsign round-trip test: set callsign → EOO TX → OFDM demod → decode
add_executable(rade_callsign_test rade_callsign_test.c)
target_link_libraries(rade_callsign_test rade m)

endif(NOT WIN32)
