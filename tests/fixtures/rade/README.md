# RADE test fixtures

## rx_test_iq.bin (synthetic)

This fixture is *not* a captured RADE transmission. It is synthetic
24 kHz interleaved float32 I/Q noise generated at test-runtime via
`std::mt19937` with a fixed seed. The exercise is to drive the RX
pipeline (24 kHz I/Q -> 8 kHz RADE_COMP -> rade_rx -> FARGAN -> 16 kHz
mono -> 24 kHz stereo) with input that the codec will *not* sync on,
verify that:

  - `syncChanged(false)` is emitted within a reasonable window
  - no `rxSpeechReady` chunks fire (because the codec never syncs,
    so it never emits decoded features)
  - the pipeline runs without crashing across multiple chunks

The fixture is generated in-test (not on disk) so the test stays
self-contained. A real RADE I/Q capture is needed to verify the
"synced + decoded speech" path; that fixture lands at bench-test
time per the Phase 3R N2 matrix because it requires the TX path
(Task I3) and a known clean-encoded utterance.

## Why synthetic?

The plan asks for `rx_test_iq.bin` containing canned RADE I/Q. A real
file would require:

  1. The Task I3 TX path to encode known speech, OR
  2. A live RADE transmission captured off-the-air.

Both are blocked at I2 time. The synthetic path exercises the
resample-then-rade_rx-then-FARGAN chain and pins the no-sync
contract; the syncing-and-decoding contract is bench-verified.

## Reproducibility

Test code:

```cpp
std::mt19937 rng(0xC0DEC0DEu);
std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
// Fill N * 2 floats with noise (I, Q, I, Q, ...) at 24 kHz.
```

Fixed seed `0xC0DEC0DE` and fixed amplitude `[-0.1, +0.1]` so the
test is deterministic across runs.

## tx_test_speech.bin (synthetic)

16 kHz mono int16 speech-like signal: 1 second of a 300 Hz sine wave
shaped with a Hanning window envelope. The peak amplitude is 24000
(well inside int16 range), the envelope smooths the start and end so
the LPCNet encoder is not driven through a hard step transition, and
the duration is long enough to push the TX pipeline past r8brain's
8 kHz -> 24 kHz upsampler warm-up and produce at least one non-empty
`txModemReady` chunk.

The fixture is also generated at test-runtime by `makeSyntheticSpeech16k`
in `tst_rade_channel.cpp`; the on-disk copy mirrors the in-test buffer
byte-for-byte so an external pipeline can replay the exact input the
I3 tests use. Generation script:

```python
import math, struct
N = 16000; sr = 16000.0; hz = 300.0; peak = 24000.0
twopi = 2 * math.pi
with open('tx_test_speech.bin', 'wb') as f:
    for i in range(N):
        t = i / sr
        env = 0.5 * (1.0 - math.cos(twopi * i / (N - 1)))
        s = peak * env * math.sin(twopi * hz * t)
        s = max(-32768.0, min(32767.0, s))
        f.write(struct.pack('<h', int(s)))
```

(Little-endian int16, 32000 bytes, mono, 16 kHz sample rate.)

A real speech recording is needed to verify the "encode then decode"
round-trip end-to-end; that fixture lands at bench-test time per the
Phase 3R N2 matrix because it requires both ends of the codec
(synthetic input still exercises every code path in `txEncode`).

## Why synthetic for TX too?

Same constraint as RX: a real speech-encoded fixture would need either
a live capture or the I3 TX path to encode known speech. The
synthetic Hanning-windowed sine exercises:

  - the 16 kHz mono int16 -> LPCNet feature extraction path
    (`lpcnet_compute_single_frame_features` over LPCNET_FRAME_SIZE
    chunks),
  - the cross-chunk speech accumulator (m_txAccum),
  - the cross-chunk feature accumulator (m_txFeatAccum) drained at
    `rade_n_features_in_out`-byte boundaries,
  - the `rade_tx()` call itself (verified via the
    `radeTxCallCountForTest()` seam),
  - the 8 kHz RADE_COMP real-leg -> 24 kHz stereo float32 upsample
    via `m_up8to24->processMonoToStereo`,
  - the `txModemReady` Qt signal emission contract.

The semantic correctness of "is what came out a decodable RADE
transmission?" is bench-verified at N2.
