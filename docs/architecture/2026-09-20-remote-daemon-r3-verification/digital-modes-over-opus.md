# Digital modes over the Core's Opus audio (R-R3-43, R-R3-23)

How FT8 decodes when a receiver's audio reaches WSJT-X through the Core's
receiver audio stream, at each audio quality setting, compared with the
untouched audio. Measured 2026-09-23 on codex/lane-b (base f1115117), and
confirmed 2026-09-24 with a fresh seed (see "Confirming run" below). The
first run was a measurement only; the confirming run led to the operator's
decision recorded under "Decision": receiver streams use Opus at 48 kbit/s
whenever they are compressed.

## Scope of this run

- **FT8 only.** FST4 and Q65 were not measured in this run. The tool
  supports them (`--modes fst4,q65`); a full run was started and cancelled
  on the operator's word to keep this Mac free for live audio testing.
- **5 signals per SNR step**, SNR -10 to -24 dB in 1 dB steps, three cases
  (one signal at two input levels, and a crowded band): 225 files, each
  decoded four ways (900 decodes). With 5 files per step, one file is 20
  percentage points, so a difference of one or two decodes at a step is
  within chance. The tool's default is 20 per step.
- Files only. No audio device was opened. Four files at a time at lowered
  priority (`nice 10`), 473 s wall time.

## What was measured

Four arms per file, all decoded by WSJT-X 3.1.0 `jt9 -8 -d 3` (FT8, deep):

| Arm | Path |
|---|---|
| untouched | the 12 kHz 16-bit file, as WSJT-X's own simulator writes it |
| lossless | up to 48 kHz, the Core's lossless (L16) profile, back to 12 kHz |
| opus48 | up to 48 kHz, the Core's Opus at 48 kbit/s (fullband), back to 12 kHz |
| opus24 | up to 48 kHz, the Core's Opus at 24 kbit/s (wideband, the default), back to 12 kHz |

The lossless arm shares the 48 kHz round trip with the Opus arms, so the
difference between lossless and an Opus arm is the codec alone.

### The Core's codec settings, as used

The codec stage (`nereus-opus-bench-codec`) links NereusCore and calls the
same objects the Core and the app use; it does not restate the settings.

- Encoder: `src/core/session/media/OpusAudioCodec.cpp:257-268`:
  `opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO)`, `OPUS_SIGNAL_MUSIC`,
  bandwidth forced by `bandwidthForBitrate()` (`OpusAudioCodec.cpp:109`:
  24000 wideband, 48000 fullband), constrained VBR, complexity 10, in-band FEC
  off, DTX off. Frame 1920 samples, 40 ms (`OpusAudioCodec.h:23`).
- Bitrate choice, as measured on 2026-09-23: `audio_bitrate` in nereusd.conf,
  24000 default or 48000 (`src/core/daemon/DaemonConfig.h:116-117`), passed to
  every receiver stream (`src/core/session/media/DaemonMediaController.cpp:1501`)
  and the main stream (`DaemonMediaController.cpp:2622`); sender built at
  `src/core/session/media/DaemonAudioSender.cpp:31`. Since the decision below,
  receiver streams no longer follow `audio_bitrate`: they use
  `DaemonMediaController::kReceiverAudioOpusBitrate` (48000) whenever they are
  compressed, and only the speakers' mix and the headphones mix follow
  `audio_bitrate`.
- Decoder (app): `src/core/session/media/RemoteAudioReceiver.cpp:362`
  (`OpusAudioDecoder`, `opus_decoder_create(48000, 2)` at
  `OpusAudioCodec.cpp:366`, `opus_decode_float` at `OpusAudioCodec.cpp:403`).
- Lossless: `PcmAudioPacketiser` and `decodeL16Rtp`
  (`src/core/session/media/PcmAudioCodec.h:22-35`, `quantiseL16Sample` at
  `src/core/session/media/PcmAudioCodec.cpp:50`), 192-frame packets
  (`RemoteAudioReceiver.cpp:370` on the app side).

`nereus-opus-bench-codec --describe` from this build:

```
opus24 rate=48000 channels=2 frame=1920 target_bitrate=24000 audio_bandwidth_hz=8000 lookahead_frames=312
opus48 rate=48000 channels=2 frame=1920 target_bitrate=48000 audio_bandwidth_hz=20000 lookahead_frames=312
lossless rate=48000 channels=2 frame=192 bits=16 payload_type=96
```

The mono signal goes in on both channels (a receiver's audio has left equal to
right) and the decoded left channel comes out. Packets go through RTP framing
exactly as on the wire; no packet is lost or reordered, and the app's jitter
buffer and rate matcher are not in the path. The 312-frame Opus lookahead is
trimmed so all arms line up.

### Signals

- Clean FT8 signals from WSJT-X's `ft8sim` at SNR 99 (no noise), a different
  message for each signal, spread over 300 to 2580 Hz.
- White Gaussian noise from a fixed seed (20260923, per file), scaled by the
  simulator's own SNR rule: signal power over noise power in 2500 Hz.
  (`ft8sim` itself picks a new random seed each run, so its noisy files are
  not reproducible; the noise is added here instead.)
- Levels: "sim" puts the noise at -50.3 dBFS, as `ft8sim` writes it (100
  counts of 32768); "loud" puts it at -30 dBFS.
- Crowded: the target plus ten other FT8 signals in the same file at +10 to
  -14 dB, at least 120 Hz from the target, at the "sim" level. All ten
  decoded in every arm of every crowded file (750 per arm, 75 files x 10).
- Up and down sampling: a 385-tap Kaiser-windowed sinc low-pass, cutoff
  5.4 kHz, the same filter for every arm.

## Results

Decodes of the sent message per SNR step (out of 5). "50 % point" is the SNR
at which half the files decode, by linear interpolation between steps.

### FT8, one signal, noise at -50.3 dBFS

| SNR (dB) | untouched | lossless | opus48 | opus24 |
|---:|---:|---:|---:|---:|
| -10 to -20 | 55/55 | 55/55 | 55/55 | 55/55 |
| -21 | 5/5 | 5/5 | 4/5 | 3/5 |
| -22 | 0/5 | 0/5 | 0/5 | 0/5 |
| -23 | 0/5 | 0/5 | 1/5 | 0/5 |
| -24 | 0/5 | 0/5 | 0/5 | 0/5 |
| total | 60 | 60 | 60 | 58 |
| weakest decoded (dB) | -21 | -21 | -23 | -21 |
| 50 % point (dB) | -21.5 | -21.5 | -21.4 | -21.2 |

### FT8, one signal, noise at -30 dBFS

| SNR (dB) | untouched | lossless | opus48 | opus24 |
|---:|---:|---:|---:|---:|
| -10 to -20 | 55/55 | 55/55 | 55/55 | 55/55 |
| -21 | 5/5 | 5/5 | 3/5 | 3/5 |
| -22 | 1/5 | 1/5 | 1/5 | 0/5 |
| -23 | 0/5 | 0/5 | 0/5 | 0/5 |
| -24 | 0/5 | 0/5 | 0/5 | 0/5 |
| total | 61 | 61 | 59 | 58 |
| weakest decoded (dB) | -22 | -22 | -22 | -21 |
| 50 % point (dB) | -21.6 | -21.6 | -21.2 | -21.2 |

### FT8, crowded band (ten other signals), noise at -50.3 dBFS

| SNR (dB) | untouched | lossless | opus48 | opus24 |
|---:|---:|---:|---:|---:|
| -10 to -18 | 45/45 | 45/45 | 45/45 | 45/45 |
| -19 | 5/5 | 5/5 | 5/5 | 3/5 |
| -20 | 5/5 | 5/5 | 5/5 | 3/5 |
| -21 | 4/5 | 4/5 | 2/5 | 1/5 |
| -22 | 0/5 | 0/5 | 0/5 | 0/5 |
| -23 | 0/5 | 0/5 | 0/5 | 0/5 |
| -24 | 0/5 | 0/5 | 0/5 | 0/5 |
| total | 59 | 59 | 57 | 52 |
| weakest decoded (dB) | -21 | -21 | -21 | -21 |
| 50 % point (dB) | -21.4 | -21.4 | -20.8 | -20.2 |

### Across all three cases (180 files per arm)

| | lossless | opus48 | opus24 |
|---|---:|---:|---:|
| decodes (untouched: 180) | 180 | 176 | 168 |
| files lost that untouched decoded | 0 | 5 | 12 |
| files gained that untouched missed | 0 | 1 | 0 |
| mean change in jt9's reported SNR (decoded in both) | 0.0 dB | -0.1 dB | -0.9 dB |

Other results:

- Lossless decoded exactly the files the untouched audio did, in every case.
- Every loss is at -19 dB or weaker, within 2 dB of FT8's decode limit
  (about -21 dB here). From -10 to -18 dB every arm decoded every file.
- The one "gained" opus48 file (-23 dB, one signal) is below the limit
  where the other arms decoded nothing; with 5 files per step it reads as chance.
- One opus24 file at -24 dB produced one decode that was not the sent
  message (a false decode). No other arm produced one in the single-signal
  cases.

## Plain answer per setting

- **Lossless:** costs no decodes. Same decodes as the untouched audio, same
  weakest SNR, same reported SNR.
- **Opus 48 kbit/s:** costs 4 decodes out of 180 net (5 lost, 1 gained), all
  at -21 dB, about 0.1 to 0.6 dB on the 50 % point, and the same weakest SNR
  decoded or better. Reported SNR is unchanged within 0.1 dB.
- **Opus 24 kbit/s (the default):** costs 12 decodes out of 180, all at -19
  to -22 dB; the 50 % point moves 0.3 to 0.4 dB with one signal and 1.2 dB in
  the crowded band, and the weakest decode is -21 dB where untouched reached
  -22 dB at the louder level. Reported SNR reads about 0.9 dB lower.

## What the numbers suggest for the operator

Written after the first run, before any decision; kept as it was read then.

- When the link carries lossless, digital modes lose nothing, which fits the
  choice that VAX and TCI streams follow the one Audio quality setting.
- When they fall back to Opus, the cost is confined to signals within about
  2 dB of the decode limit. At 48 kbit/s it is close to measurement noise at
  this sample size; at 24 kbit/s it is larger, and largest on a crowded band
  (7 of the 14 files untouched decoded from -19 to -21 dB were lost).
- Two possible decisions followed, neither made in the first run: whether
  receiver streams that feed apps should use 48 kbit/s when falling back to
  Opus even while the speaker stream stays at 24 kbit/s; and whether the VAX
  page should say that Opus can cost the weakest decodes. Both were settled
  after the confirming run; see "Decision (2026-09-24)".
- Before either, a fuller run (20 signals per step, FST4 and Q65 included)
  would firm up the 24 versus 48 kbit/s difference; the command below does it.
- Still pending at the operator checkpoint (hardware): a live side-by-side
  decode through a real Core and remote window.

## Confirming run (2026-09-24)

A second FT8 run with a fresh noise seed, to check the first run's order
before deciding. Same tool, same arms, same three cases and the same codec
profiles (`nereus-opus-bench-codec --describe` printed exactly the three
lines above).

- **Seed 20260924** (the first run used 20260923). WSJT-X 3.1.0 `jt9`.
- **5 signals per SNR step**, SNR -10 to -24 dB, three cases: 225 files, 900
  decodes. With 5 files per step, one file is 20 percentage points.
- Output directory:
  `/Users/j.j.boyd/.config/nereus/work/ft8-opus48-confirm-2026-09-24`
  (`results.md`, `results.csv`, `summary.json`, `run.log`, `driver.log`).
  Files only, no audio device opened. 230 s wall time; the Mac's load
  average was 8.8 at the start and 10.8 at the end (`driver.log`), which
  slows the run but does not change a decode (each file is decoded offline).

Decodes of the sent message (out of 75 files per case):

| Case | untouched | lossless | opus48 | opus24 |
|---|---:|---:|---:|---:|
| one signal, noise at -50.3 dBFS | 60 | 60 | 59 | 57 |
| one signal, noise at -30 dBFS | 59 | 59 | 59 | 58 |
| crowded band (ten other signals), noise at -50.3 dBFS | 58 | 58 | 57 | 49 |
| total (225 files) | 177 | 177 | 175 | 164 |

Per case:

| Case | 50 % point, untouched / lossless / opus48 / opus24 (dB) | weakest decoded, same order (dB) |
|---|---|---|
| one signal, -50.3 dBFS | -21.5 / -21.5 / -21.2 / -20.8 | -22 / -22 / -22 / -23 |
| one signal, -30 dBFS | -21.5 / -21.5 / -21.2 / -21.2 | -22 / -22 / -22 / -21 |
| crowded band | -21.2 / -21.2 / -20.8 / -19.2 | -21 / -21 / -21 / -21 |

Across all three cases, from `results.csv`:

| | lossless | opus48 | opus24 |
|---|---:|---:|---:|
| decodes (untouched: 177) | 177 | 175 | 164 |
| files lost that untouched decoded | 0 | 4 | 15 |
| files gained that untouched missed | 0 | 2 | 2 |
| mean change in jt9's reported SNR (decoded in both) | 0.0 dB | -0.1 dB | -0.9 dB |

- Lossless again decoded exactly the files the untouched audio did.
- The order is the first run's: untouched = lossless, then opus48 a
  decode or two behind, then opus24 clearly behind. Opus 24 kbit/s is
  worst on the crowded band (49 of 58: it lost files from -18 to -21 dB,
  where opus48 lost one at -21 dB).
- Every other signal in the crowded files decoded in every arm (750 in
  each arm; opus48 751, one extra decode).

## Decision (2026-09-24)

The operator's decision, after this confirming run: **receiver streams (the
per-receiver audio VAX and TCI apps get in a remote window) use Opus at 48
kbit/s, fullband, whenever they are compressed**: when Opus is the audio
quality choice, when the Core refuses lossless, and when lossless falls back
to Opus after the window's link trial. The speakers' mix (and the
headphones mix) keep the Core's own `audio_bitrate`, 24 kbit/s by default.
Lossless, its link trial and its fallback trigger are unchanged.

Built in the R3 completion plan, Task 7
(`docs/architecture/2026-09-24-r3-completion-plan.md`):
`DaemonMediaController::kReceiverAudioOpusBitrate`; each receiver stream's
context reports its encoder, so the remote window's audio status reads
"Receiving, Opus 48 kbit/s" from the Core's report. A window built before
the change decodes 48 kbit/s streams unchanged (the decoder takes either
rate and is never told one; the media offer is as before). The VAX page's
note now says "a few of the weakest" digital-mode signals may not decode,
true at 48 kbit/s and of an older Core at 24 kbit/s.

Still pending at the operator checkpoint (hardware): a live side-by-side
decode through a real Core and remote window.

## Reproduce

From the repository root (builds the codec tool, which is excluded from the
default build, then runs; results in `<build>/digital-mode-opus-bench/`):

```
python3 tools/digital-mode-opus-bench/run-bench.py --build build-lane-b --modes ft8 --count 5 --jobs 4
```

Each finished file's rows are appended to `results.csv` at once;
`--summarise-only` rebuilds `results.md` and `summary.json` from it. The
defaults (`--modes ft8,fst4,q65 --count 20 --jobs 2`) give the full run.
Needs WSJT-X in `/Applications/wsjtx.app` (`--wsjtx` to change) and Python 3
with numpy.
