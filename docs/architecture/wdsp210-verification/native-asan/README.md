# WDSP 2.10 native AddressSanitizer probe

This standalone project compiles every source in the reviewed
`third_party/wdsp/CMakeLists.txt` inventory with AddressSanitizer. It does not
modify or reuse the main build's `wdsp_static` archive.

From the repository root:

```sh
cmake -S docs/architecture/wdsp210-verification/native-asan \
  -B /tmp/nereus-wdsp210-asan/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/nereus-wdsp210-asan/build --parallel 2
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:check_initialization_order=1 \
  /tmp/nereus-wdsp210-asan/build/native_asan_probe \
  tests/fixtures/dsp/ps3-v2-source-writer.txt
```

Configuration prints the WDSP source count; the expected reviewed inventory is
83 C translation units. The probe exercises both RX and TX filter resize and
sample processing, a valid PS3 restore, active and quiescent correction stop,
retained-correction apply, cancellation after a restore reaches IQC BEGIN,
bounded display extraction, and channel teardown.

The main build's rnnoise and libspecbleach static archives and the system FFTW
libraries are linked dependencies, so AddressSanitizer does not instrument
their object code. AppleClang's macOS AddressSanitizer runtime also reports
that `detect_leaks` is unsupported; this probe therefore verifies address
safety but is not a leak-sanitizer run. The probe intentionally skips the
optional FFTW wisdom pre-generation pass, which can take several minutes and
does not change the exercised WDSP lifecycle contracts.
