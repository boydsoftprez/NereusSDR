# NereusSDR Architecture Overview

## Layer Diagram

```
+---------------------------------------------+
|                  GUI Layer                    |
|  MainWindow, SpectrumWidget, VfoWidget, etc. |
+---------------------------------------------+
|                Model Layer                    |
|  RadioModel, SliceModel, PanadapterModel     |
+---------------------------------------------+
|                 Core Layer                    |
|  RadioDiscovery, RadioConnection, AudioEngine |
|  WdspEngine, AppSettings                     |
+---------------------------------------------+
|              Protocol Layer                   |
|  OpenHPSDR P1 (UDP 1024), P2 (UDP multi-port)|
+---------------------------------------------+
|                DSP Layer                      |
|  WDSP (demod, AGC, NR, NB, ANF, PureSignal) |
|  FFTW3 (spectrum FFT)                        |
+---------------------------------------------+
```

## Key Architectural Difference from AetherSDR

In AetherSDR, the FlexRadio does most DSP and sends:
- Decoded audio (PCM) for speakers
- FFT bin data for spectrum display
- Waterfall tile images for waterfall display

In NereusSDR, the OpenHPSDR radio sends only:
- Raw I/Q samples (24-bit, from the ADC)
- Feedback data (forward power, SWR, etc.)

**The client does ALL signal processing:**
1. I/Q samples -> WDSP -> Demodulated audio
2. I/Q samples -> FFTW3 -> FFT bins -> GPU rendering -> Spectrum/Waterfall
3. I/Q samples -> WDSP -> PureSignal feedback processing

This means:
- Client CPU/GPU usage is significantly higher than AetherSDR
- Multiple receivers require multiple WDSP channels (each is an independent DSP pipeline)
- Spectrum/waterfall quality is limited by client FFT size and GPU rendering performance
- PureSignal feedback loop runs entirely on the client

## Thread Architecture

| Thread | Components |
|--------|-----------|
| **Main** | GUI rendering, RadioModel, all sub-models, user input |
| **Connection** | RadioConnection (UDP/TCP I/O, protocol framing) |
| **Audio** | AudioEngine + WdspEngine (I/Q processing, DSP, audio output) |
| **Spectrum** | FFT computation, waterfall data generation |

Cross-thread communication uses auto-queued signals exclusively.

## Data Flow: RX Path

```
Radio (ADC) --UDP--> RadioConnection --signal--> RadioModel
                                                      |
                                                      v
                                                 WdspEngine (per-channel)
                                                 |          |
                                                 v          v
                                          AudioEngine   FFT/Spectrum
                                              |              |
                                              v              v
                                          Speakers      SpectrumWidget
```

## Data Flow: TX Path

```
Microphone --> AudioEngine --> WdspEngine (TX channel)
                                    |
                                    v
                              Modulated I/Q
                                    |
                                    v
                              RadioConnection --UDP--> Radio (DAC)
```
