# C Port of RADE V1

This repo contains a pure C implementaion of RADE V1, with all Python code and dependencies removed. It was derived from the [reference Python implementation](https://github.com/drowe67/radae) with the asistance of Claude Code. It has been reviewed, and carefully tested by the FreeDV team.  It passes the same [suite of automated tests](https://github.com/drowe67/radae/pull/66) as the Python version.

This has been tested on Linux and macOS.

## Build

```
cd radae_nopy
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc) # or -j$(sysctl -n hw.logicalcpu) on macOS
```

## Demo tools

### RADE Demod: WAV RADE → WAV Speech Audio

Take a wav file off air and produce a demodulated wav file
```
Usage:
rade_demod_wav [-v 0|1|2] <input.wav> <output.wav>
```

### RADE Modulate: WAV Speech Audio → WAV RADE

Take a wav file with speech in it and produce a RADE OFDM encoded output wav file ready for transmission.

```
Usage:
rade_modulate_wav [-v 0|1|2] <intput.wav> <output.wav>
```

### Encode: WAV → IQ

```
sox ../voice.wav -r 16000 -t .s16 -c 1 - | \
  ./src/lpcnet_demo -features /dev/stdin - | \
  ./src/radae_tx > tx.iq
```

### Decode: IQ → WAV  

```
cat tx.iq | \
  ./src/radae_rx | \
  ./src/lpcnet_demo -fargan-synthesis /dev/stdin - | \
  sox -t .s16 -r 16000 -c 1 - decoded.wav
```

### Decode: WAV RADE → WAV (multiple steps)

```
sox ../FDV_offair.wav -r 8000 -e float -b 32 -c 1 -t raw - | \
./src/real2iq | \
./src/radae_rx > features.f32
./src/lpcnet_demo -fargan-synthesis features.f32 - | \
sox -t .s16 -r 16000 -c 1 - decoded.wav
play decoded.wav
```

## Files
| File                                          | Purpose                                             |
| --------------------------------------------- | --------------------------------------------------- |
| `src/rade_dsp.h/c`                            | Complex math utilities, constants, pilot generation |
| `src/rade_ofdm.h/c`                           | OFDM modulation/demodulation with DFT matrices      |
| `src/rade_bpf.h/c`                            | Complex bandpass filter                             |
| `src/rade_tx.h/c`                             | Transmitter (encoder + OFDM mod)                    |
| `src/rade_acq.h/c`                            | Acquisition and pilot detection                     |
| `src/rade_rx.h/c`                             | Receiver with sync state machine                    |
| `src/rade_api_nopy.c`                         | Python-free API implementation                      |
| `src/radae_tx_nopy.c` / `src/radae_rx_nopy.c` | Standalone executables                              |

```
radae_nopy/
├── CMakeLists.txt
├── cmake/
│   └── BuildOpus.cmake
└── src/
    ├── CMakeLists.txt
    ├── lpcnet_demo.c          # Feature extraction
    ├── radae_tx_nopy.c        # Transmitter
    ├── radae_rx_nopy.c        # Receiver
    ├── real2iq.c              # Real → IQ converter
    ├── rade_api.h             # Public API
    ├── rade_api_nopy.c        # API implementation
    ├── rade_dsp.h/c           # DSP primitives
    ├── rade_ofdm.h/c          # OFDM mod/demod
    ├── rade_bpf.h/c           # Bandpass filter
    ├── rade_tx.h/c            # Transmitter internals
    ├── rade_rx.h/c            # Receiver internals
    ├── rade_acq.h/c           # Acquisition
    ├── rade_enc.h/c           # Neural encoder
    ├── rade_dec.h/c           # Neural decoder
    ├── rade_enc_data.c        # Encoder weights
    ├── rade_dec_data.c        # Decoder weights
    └── opus-nnet.h.diff       # Opus patch
```

The implementation uses built-in neural network weights (compiled from `rade_enc_data.c` and `rade_dec_data.c`), eliminating any need for Python or external model files at runtime.

# Automated testing

A suite of tests will be run on every GitHub push.  They can also be run [locally](https://github.com/drowe67/radae/pull/66).
