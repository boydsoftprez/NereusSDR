# Display codec v2: every row stands alone

Status: approved by JJ on 2026-10-22 (after two spec reviews); his decisions
are in section 13. No code yet.

JJ's decision of 2026-10-22: "yes do C then build A". Step 1 (C) is a Core
setting that sends every v1 display frame as a keyframe, with no wire change,
being built on `claude/display-keyframe-every-frame`; it is the bench baseline
for this design. Step 2 (A) is this document: a new display codec version in
which every waterfall row and every trace is coded on its own, losslessly, so
a lost message costs exactly its own row and nothing waits for a keyframe.

Inputs: the loss study of 2026-10-22 (the crew folder's
`study-waterfall-loss.md`), the v1 design
([display codec v1](2026-09-20-display-codec-v1.md)), `DisplayCodec.{h,cpp}`,
the Core sender (`DaemonMediaController`), the window side
(`RemoteMediaController`, `RemoteDisplayPresenter`, `SpectrumWidget`), the
iPhone decoder (`ios/NereusKit/Sources/NereusMedia/DisplayFrameDecoder.swift`)
and the other decoders listed in section 12.

## 1. The problem in one paragraph

A v1 frame is a 42-byte header, a trace plane and a waterfall plane. Between
keyframes each plane is a temporal delta against the previous frame
(DisplayCodec.cpp:236-263), so a lost message breaks the chain: the decoder
returns `NeedKeyframe` and drops every later delta (DisplayCodec.cpp:576-592),
the window asks for a keyframe at most every 200 ms
(RemoteMediaController.cpp:8413-8414), the Core grants at most 5 a second
(DaemonMediaController.cpp:279, 4223), and meanwhile the window repeats the
last row and later blends the gap (RemoteDisplayPresenter.cpp:163-213,
286-306). On JJ's STRIX link (media RTT 71 ms, every frame two datagrams on an
unordered channel with no retransmission) one lost datagram costs about seven
made-up rows. That is the smear.

## 2. What the references do (study, then choose)

Every reference the study read sends rows that stand alone:

- OpenWebRX (develop `640c5b0b`, csdr `1f15b8c5`): one message per row, 4-bit
  IMA ADPCM reset for every row with 10 pad samples (csdr adpcm.cpp:165-170,
  253-260). TCP, no gap fill.
- KiwiSDR (master `efb38e2b`): a 16-byte header with a sequence
  (rx_waterfall.h:73-89), 8-bit dB, IMA ADPCM fresh for every row
  (rx_waterfall.cpp:1577-1584, 1660-1667).
- piHPSDR (`4aa95c5`): one byte per bin, zlib per row, over UDP
  (server_thread.c:221-232); a loss costs one row (client_thread.c:642-653).
- AetherSDR (`1e0718ad`) and Flex: waterfall tiles of absolute 16-bit bins,
  self-contained; a partial tile is dropped (PanadapterStream.cpp:1071-1075,
  1094-1095).
- WebRTC video keeps temporal prediction and pays for it with NACK, FEC,
  picture-loss requests and intra refresh, because a video intra frame is 5 to
  10 times a delta frame. Ours is not: section 6 measures an intra row at or
  near today's temporal delta.

What we take: self-contained rows (all of them) and one row per message where
it fits (OpenWebRX, KiwiSDR, piHPSDR). What we do not take: ADPCM. The study
measured IMA ADPCM at our 0.47 dB quantum with 3 to 10 dB error at carrier
edges (p99), and JJ asked for lossless.

Deflate (zlib, piHPSDR's choice) was measured on the same bin-to-bin residuals
(raw deflate, level 9, no zlib header): 460 bytes a plane on the peak scene,
371 average, 512 at 8 bins per pixel, 639 raw, against 421 to 610 for the Rice
coder below; it wins only on long runs (a flat row, 10 bytes; a ramp, 27). On
the rows a receiver actually shows it is 4 to 9 percent larger, and it would
add a zlib dependency to the phone's decode path. The entropy code chosen is a
Rice code with a parameter per block and an escape for incompressible blocks.
That is the structure of FLAC's coded residual (RFC 9639, section 9.2.7:
partitions, a Rice parameter per partition, an escape code); only the
structure is borrowed, none of FLAC's field sizes or code.

## 3. Choosing the predictor and entropy code

Measured with scratch drivers outside the repository (in the crew scratchpad,
`codecv2/v2sim.cpp`, `codecv2/fast.cpp` and `codecv2/v3.cpp`; they link the
repository's real `DisplayCodec.cpp` for the v1 figures). The re-measurement
for this revision ran at `nice -n 15` while the Mac was under load. Scenes are
the study's: 783 pixels, a 120 dB window (quantum 0.47 dB), 12 carriers, 4
keyed CW signals and 2 SSB-like bands, 1,500 frames each; plus raw single-bin
rows, uniform noise (the worst case), flat, a drifting ramp, a 1,024-pixel
average pan and a 4,096-pixel peak pan; plus, for section 6, the trace
log-recursive averaged at alpha 0.12 (the `SpectrumPlaneRequest` default,
SpectrumEndpoint.h:80-84) with the waterfall unaveraged. Every candidate was
decoded back and compared with the encoder's quantized input: zero mismatches
on every scene.

Mean payload bytes per plane (the bit stream only, without the 4-byte plane
prefix of section 4.2):

| Candidate | Peak 167 | Avg 167 | Peak+IIR | Avg 8 | Raw 1 bin | Noise | 4096 px |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Previous bin, Rice per 16 | 421 | **353** | **347** | 488 | 620 | 801 | 2,372 |
| Previous bin, Rice per 32 | 423 | 380 | 362 | 487 | 614 | 793 | 2,365 |
| Previous bin, Rice per 64 | 426 | 396 | 372 | **486** | 610 | **788** | 2,369 |
| Two-tap (2a - b), Rice per 32 | 471 | 404 | 397 | 534 | 672 | 793 | 2,648 |
| Median of last three, Rice per 32 | 447 | 427 | 401 | 510 | 616 | 793 | 2,437 |
| Mean of last two, Rice per 32 | 431 | 404 | 379 | 495 | 610 | 793 | 2,376 |
| Previous or two-tap per block, Rice per 32 | 426 | 375 | 362 | 490 | 617 | 796 | 2,377 |
| Previous bin, adaptive Rice (no side data) | 422 | 399 | 375 | 486 | 608 | 841 | 2,360 |
| Previous bin, raw deflate level 9 | 460 | 371 | 381 | 512 | 639 | 788 | 2,451 |
| Order-0 entropy of previous-bin residuals (floor) | 393 | 304 | 311 | 460 | 586 | 758 | 2,280 |
| v1 temporal delta, today (for scale) | 512 | 414 | 370 | 581 | 745 | 821 | 2,908 |

Choices, and why:

- **Predictor: the previous bin.** The two-tap predictor and the medians lose
  everywhere: display rows are mostly noise around a smooth floor, and a
  longer predictor adds that noise rather than canceling it. Choosing between
  predictors per block gains nothing worth a bit per block. In one dimension a
  median of `a`, `b` and `2a - b` is always `a`, so the "median" tried is the
  median of the last three bins.
- **Entropy code: Rice with one parameter `k` per block, and an escape.** It is
  within 4 to 16 percent of the order-0 entropy floor, decodes with shifts and
  a leading-zero count, and ports to Swift in about a hundred lines. Adaptive
  Rice (a running parameter, as JPEG-LS does) needs no side data but loses on
  averaged rows and, on noise, exceeds the v1 bound (841 bytes against 821),
  so it would need an escape anyway. An arithmetic code could take back the
  last 30 to 50 bytes a plane, at a cost in complexity and phone porting that
  this does not justify.
- **Block size chosen per plane: 16, 32 or 64.** No one size wins: 16 is best
  on averaged and smoothed rows, 64 on raw and noisy rows. The encoder tries
  all three and keeps the smallest, as v1 already does with its block sizes.
  Mean whole-plane sizes with that choice, the 4-byte prefix included: peak
  425, average 357, peak with IIR 351, average of 8 bins 490, raw 614, noise
  792, flat 108, ramp 172, 1,024 px 452, 4,096 px 2,369; a trace
  log-averaged at 0.12 is 317 (peak scene), 311 (average) and 375 (8 bins).

## 4. Wire format

All multibyte integers are unsigned network byte order, as v1. Within the bit
stream of a plane, bits are written and read most significant bit first in
each byte.

### 4.1 Header

The 42-byte header is v1's, field for field and offset for offset, with
`version = 2`. Endpoint ID stays at offset 8 and context generation at 12, so
the window's routing (RemoteMediaController.cpp:8451-8464) reads both
versions alike. The header still carries the full context (interval and all
three row lengths) whatever planes follow, so a message is self-describing.

Flags at offset 5 change meaning in version 2:

| Bit | Name | Meaning |
| --- | --- | --- |
| `0x02` | waterfall advance | This frame advances the waterfall (as v1). |
| `0x04` | wide plane | A wide (3D) plane follows the waterfall plane. |
| `0x08` | trace plane | A trace plane is present. |
| `0x10` | waterfall plane | A waterfall plane is present. |
| `0x20` | split | The frame was sent as two messages (section 5). |
| `0x01`, `0x40`, `0x80` | | Reject (`UnknownFlags`). |

The valid combinations, and nothing else (`Malformed` otherwise):

| Flags | Message |
| --- | --- |
| `0x08` | A whole frame that does not advance the waterfall: the trace only. |
| `0x1A` | A whole frame: trace and waterfall. |
| `0x1E` | A whole frame: trace, waterfall and wide. |
| `0x2A` | The trace part of a split frame. |
| `0x32` | The row part of a split frame: waterfall. |
| `0x36` | The row part of a split frame: waterfall and wide. |

`0x04` is set exactly when `0x10` is set and the header's wide length is not
zero; a message breaking that rule is `Malformed`. There is no keyframe flag:
every version 2 message is decodable alone. The message ends exactly at the
end of its last plane; a byte after it is `Malformed`, as in v1.

A frame that does not advance the waterfall carries no waterfall plane in
v2. Today the waterfall plane rides every frame and the window uses it only
when the frame advances (RemoteMediaController.cpp:8797,
SpectrumWidget.cpp:2891, FilterDisplayItem.cpp:211). With the default 30 ms
waterfall period at 30 fps every frame advances (framesPerLine 1), so this
saves bytes only for a slower waterfall.

### 4.2 Plane

Planes follow the header in trace, waterfall, wide order, those present only.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | coding: `2` (previous-bin Rice); any other value rejects |
| 1 | 1 | block size code: `0` = 16, `1` = 32, `2` = 64; 3 and above reject |
| 2 | 2 | payload bytes |
| 4 | payload | the bit stream |

The bit stream:

1. The first sample, 8 bits, as its quantized value.
2. Then samples 1 to n-1 in blocks of the coded size (the last may be short).
   Each block starts with `k`, 3 bits.
   - `k` = 7: the block's samples follow as raw 8-bit quantized values.
   - `k` = 0 to 6: each sample is coded as a residual against the sample
     before it. All of this is 8-bit unsigned arithmetic that wraps: the
     residual `r` is `x[i] - x[i-1]` computed as an 8-bit unsigned value and
     read as a signed 8-bit (two's complement) number, -128 to 127. It is
     zigzag-mapped to `z = 2r` when `r >= 0` and `z = -2r - 1` otherwise (so
     `z` is 0 to 255), and written as `z >> k` zero bits, a one bit, then the
     low `k` bits of `z`. The decoder rebuilds `x[i] = x[i-1] + r`, again in
     8-bit unsigned arithmetic that wraps.
3. Zero bits pad the stream to a whole byte.

Quantization is v1's exactly (`round((dbm - minDbm) * 255 / (maxDbm -
minDbm))`, clipped), so v2 reconstructs what a v1 keyframe reconstructs, bit
for bit. The dead zone does not exist in v2: it only ever applied against a
previous frame.

The decoder rejects as `Malformed`: a payload that runs out, a `z` above 255,
a payload with bytes left after the last sample, any nonzero pad bit, and a
plane prefix out of range. It does not require the encoder's choices to be
the smallest; any valid stream decodes.

The encoder, for each of the three block sizes, tries every block with each
`k` from 0 to 6 and with the raw escape (up to 8 codings a block), and keeps
the one with the fewest bits; ties go to the smaller `k`, and the escape is
taken only when it is strictly fewer bits than every `k`. It then keeps the
block size whose whole plane has the fewest bytes, ties to the smaller size.
This is deterministic, so golden vectors are stable.

### 4.3 Exact sizes and the bound

A v2 plane of `n` samples is at most

    P(n) = 4 + n + ceil(3 * ceil((n - 1) / 64) / 8)

bytes: the 4-byte prefix, the first sample, every other sample raw, and a
3-bit `k` per 64-sample block (the encoder never does worse than its
all-escape choice at 64-sample blocks). The scratch check ran every `n` from 1
to 4,096 with incompressible rows: every plane decoded exactly and stayed
within `P(n)`, and `P(n) <= A(n)` (v1's `3 + 5*ceil(n/128) + n`) for every
`n`. For example `P(783) = 792` against `A(783) = 821`, and `P(4096) = 4,124`
against 4,259.

So a whole v2 frame is never larger than a v1 keyframe of the same shape,
`42 + 2*A(n) + A(wide)`, which is what `DisplayBudget` already charges
(DisplayBudget.cpp:128-142, DisplayBudget.h:64-73). **The display budget,
its charge and its wire do not change**, on the Core, the window
(`RemoteDisplayAllocator`) or the phone (`DisplayQualityAllocator`).

## 5. One message or two

The study suggested two, so that each fits one datagram. The rule:

- A frame is sent whole when the whole message fits one SCTP DATA chunk
  (876 bytes, `IMediaTransport::kSctpDataPayloadBytes`), and always when it
  carries no row (`0x08`).
- Otherwise it is split into a row part (waterfall, and the wide plane when
  present) and a trace part, both with the frame's sequence, timestamp and
  context, provided the two parts together (one more 42-byte header) stay
  within the endpoint's admitted frame cost, `displayCost.maximumFrameBytes`
  (DaemonMediaController.cpp:6551). That is the bound the budget charges:
  `42 + 2*A(pixels)`, plus `A(768)` (`SpectrumEndpoint::kMaxWideSamples`)
  whenever the wide plane is included, whatever the actual wide length
  (DisplayBudget.cpp:137-141). When the parts would not fit (possible only for
  nearly incompressible rows of 640 pixels or fewer), the frame is sent whole.

Why: a frame bigger than one datagram is lost when any of its datagrams is
lost. Whole, a two-datagram frame loses its row with about twice the
datagram loss rate; split, each part is one datagram, so the row is lost at
the datagram loss rate and a lost trace part no longer costs a row. The
datagram count is the same. The extra header costs about 5 percent of the
bytes of a split frame.

For JJ's 783-pixel pan (peak scene) a whole v2 frame averages 892 bytes, just
over one datagram, so 91 percent of frames split into two parts of about 467
bytes, each one datagram. Of the study's receiver scenes, only the
167-bins-per-pixel average and smoothed scenes fit one datagram whole: the
average scene, the peak and average scenes with the trace log-averaged, and
peak with IIR on both (section 6, 1.00 datagrams each; the synthetic flat and
ramp rows fit too). Every 8-bins-per-pixel frame splits: the average scene
(1,064 bytes) and its averaged-trace variant (949 bytes) both split on 100
percent of frames, as do the raw, noise, 1,024-pixel and 4,096-pixel scenes.

Send order: the row part first, then the trace part. The display channel
holds at most one message when SCTP cannot take it at once, and refuses the
next (v1 document, "Transport limits"). If that happens to the first part,
the second is the one refused, and losing a trace costs less than losing a
row. If the first part is refused, neither is sent (the frame is dropped, as
today). A refused second part is counted.

Pacing and the admitted cost (DaemonMediaController.cpp:6551-6559) are
checked against the frame's total bytes, before the first part is offered. A
v2 frame over its admitted cost cannot happen by section 4.3; if it ever did
it is dropped and logged, with no keyframe forced (there is none to force).

Rows bigger than one datagram stay one message. A row part passes 876 bytes
at about 1,550 pixels on the peak scene and about 1,850 on the average scene,
so a pan wider than about 1,700 pixels still loses its whole row when any one
of the row's datagrams is lost (a 4,096-pixel row part is about 2,411 bytes,
three datagrams). Cutting a row into self-contained pieces would make a lost
datagram cost part of a row, which the window cannot draw today without a
change to its gap fill (a visual change), so it is not in this design.

The display extras datagram (NSDX) still follows each frame once, unchanged.

## 6. Measured estimate: v2 against v1

Mean bytes per frame (trace and waterfall, header included), kbps per pan at
20 and 30 fps (the study assumed 20 rows a second for JJ's pan; the default
is 30), and datagrams per frame. "Keyframe every frame" is Step 1. Plane
prefixes are the real 4 bytes.

| Scene | Pixels | v1 today, B | Keyframe every frame (Step 1), B | v2, B | v1 kbps 20/30 | Step 1 kbps 20/30 | v2 kbps 20/30 | Datagrams v1 / Step 1 / v2 | v2 against v1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Peak, 167 bins/px | 783 | 1,072 | 1,684 | 930 | 172 / 257 | 269 / 404 | 149 / 223 | 2.00 / 2 / 1.91 | -13% |
| Peak, 167 bins/px, trace log-averaged 0.12 | 783 | 843 | 1,684 | 784 | 135 / 202 | 269 / 404 | 125 / 188 | 1.12 / 2 / 1.00 | -7% |
| Average, 167 bins/px | 783 | 877 | 1,684 | 757 | 140 / 210 | 269 / 404 | 121 / 182 | 1.46 / 2 / 1.00 | -14% |
| Average, 167 bins/px, trace log-averaged 0.12 | 783 | 753 | 1,684 | 710 | 120 / 181 | 269 / 404 | 114 / 170 | 1.01 / 2 / 1.00 | -6% |
| Peak + IIR 0.3 on both | 783 | 781 | 1,684 | 743 | 125 / 187 | 269 / 404 | 119 / 178 | 1.01 / 2 / 1.00 | -5% |
| Average, 8 bins/px | 783 | 1,207 | 1,684 | 1,064 | 193 / 290 | 269 / 404 | 170 / 255 | 2.00 / 2 / 2.00 | -12% |
| Average, 8 bins/px, trace log-averaged 0.12 | 783 | 931 | 1,684 | 949 | 149 / 223 | 269 / 404 | 152 / 228 | 1.91 / 2 / 2.00 | +2% |
| Raw, 1 bin/px | 783 | 1,532 | 1,684 | 1,312 | 245 / 368 | 269 / 404 | 210 / 315 | 2.00 / 2 / 2.00 | -14% |
| Uniform noise (worst) | 783 | 1,684 | 1,684 | 1,668 | 269 / 404 | 269 / 404 | 267 / 400 | 2.00 / 2 / 2.00 | -1% |
| Flat | 783 | 326 | 1,684 | 258 | 52 / 78 | 269 / 404 | 41 / 62 | 1.01 / 2 / 1.00 | -21% |
| Drifting ramp | 783 | 674 | 1,684 | 386 | 108 / 162 | 269 / 404 | 62 / 93 | 1.01 / 2 / 1.00 | -43% |
| Average | 1,024 | 1,106 | 2,176 | 988 | 177 / 265 | 348 / 522 | 158 / 237 | 2.01 / 3 / 2.00 | -11% |
| Peak | 4,096 | 5,881 | 8,560 | 4,821 | 941 / 1,411 | 1,370 / 2,054 | 771 / 1,157 | 7.03 / 10 / 6.00 | -18% |

What the numbers say:

- The **waterfall plane** is smaller in v2 than v1's temporal delta on every
  scene (for example 425 against about 515 bytes on the peak scene). Rows from
  a fresh FFT each frame change too much from one frame to the next for a
  temporal delta to win.
- A **trace that is averaged over time** changes little from frame to frame,
  and there v1's temporal delta is the better coder: about 286 bytes against
  v2's 317 on the peak scene, about 306 against 375 at 8 bins per pixel
  (v1's trace plane inferred from its frame size less the header and the
  same-scene waterfall plane).
- Net, a v2 frame is 1 to 43 percent smaller than today's v1 frame on 12 of
  the 13 scenes, and 2 percent larger on one: an averaged trace on a narrow
  zoom (8 bins per pixel). Uniform noise, the worst case, is 1 percent
  smaller. Against Step 1, which is the only v1 that survives loss, v2 is
  smaller on every scene, by 1 to 85 percent.
- The largest v2 frame on the 783-pixel scenes (1,668 bytes, uniform noise)
  is below the 1,684-byte v1 keyframe.

What a lost datagram costs:

- v1 today: the frame, then every frame until a granted keyframe arrives
  (about seven made-up rows on the STRIX link).
- Step 1: one frame (a row and a trace), but every frame is the keyframe size.
- v2: one row or one trace, never both, never more, on pans up to about 1,700
  pixels; at most one made-up (blended) row per lost row.

## 7. Version negotiation

Pattern: `spectrumPicture`, step for step
([remote media control v1](2026-09-20-remote-media-control-v1.md), "The
picture a context continues"): the client's hello declares a feature, the
Core advertises a capability only to a peer that declared it, and that peer
adds the version to its media `start`. Old peers keep their exact capability
descriptor.

- **Client declares** `displayCodec: 2` in its hello, beside
  `spectrumPicture: 1` (StationClient.cpp:1069), only in a build whose
  decoder reads v2.
- **Core advertises** `displayCodecVersion` 2 only to a peer at agreed
  session minor 11 whose hello declared `displayCodec` 2, while media is
  available and the `RemoteDisplayCodecV2` setting is on (below); any other
  peer is sent no entry, so its capabilities are today's. The gate is a
  `StationServer` function modeled on `spectrumPictureAvailable`
  (StationServer.cpp:13181-13189, `peerDeclares(session, "displayCodec", 2)`),
  the capabilities field is set the way `spectrumPictureVersion` is
  (StationServer.cpp:13826-13831), and the entry is appended after
  `rxMeterStampsVersion` and before `coreBuildInfo`, which stays last
  (StationCapabilities.cpp:589-618).
- **Client asks** by adding `displayCodecVersion` 2 to its media `start`
  (as RemoteMediaController.cpp:5461-5463 adds `spectrumPictureVersion`),
  only when the Core advertised it. The desktop window does this in the same
  change as the Core encoder. The iPhone app declares and asks only when its
  own v2 decoder lands, in its own change worked in parallel (section 8).
- **Core accepts** the `start` field in `DaemonMediaController::handleStart`
  exactly as it accepts `spectrumPictureVersion`
  (DaemonMediaController.cpp:2367-2380): refused unless the same gate holds
  for this session and the value is exactly 2. A Core that did not advertise
  it refuses a `start` carrying it, as it refuses any field it cannot read
  today, so a client never sends it to an older Core. This `start` field
  remains the per-connection switch.
- **Core encodes** every display endpoint of a media connection whose `start`
  carried it in v2, the transmit display included; every other connection
  gets exactly today's v1 bytes. `tst_display_extras`' older-peer golden
  (`tests/OlderPeerDisplayRun.h`) and the link conformance fixtures hold that
  byte for byte.
- **Per media connection, kept across a replace.** Every new media
  connection, whether direct, relayed or through the media tunnel, begins with
  its own `start`, so the codec is agreed for each one. A `replace` after a
  handover has no `start` of its own: it carries exactly `op`, `connectionId`
  and `replaces` (plus `mediaDirectVersion` on the direct ladder,
  DaemonMediaController.cpp:2904-2913), and the replacement inherits what the
  `start` agreed, which the Core keeps in `m_startOfferedLossless` and
  `m_startMicLine` (:2462-2465) and hands to the new peer (:3024-3026). The
  agreed codec version is stored beside `m_startMicLine` and kept across a
  `replace` the same way, so a replacement stays on the codec its `start`
  agreed. `finishReplacement` forces a keyframe on every endpoint
  (:3121-3124); for a v2 connection that flag is a no-op (section 9.3).
  Relays and the tunnel forward the display channel's datagrams without
  reading them, so nothing changes there.
- **Decoders read both.** The window's decoder chooses by the version byte,
  so a frame of either version decodes whatever the `start` said.
- **The `start` field also stops keyframe requests.** A window whose media
  `start` carried `displayCodecVersion` 2 sends no `keyframe` operation for
  a context accept, a re-grant or a transmit context (section 9.3); a window
  on a v1 connection keeps asking as today. The window keeps this "`start`
  carried v2" flag for the media connection, not for its current transport,
  so it too survives a `replace`: a replaced connection stays v2 and the
  window sends no `keyframe` operation for it. The flag clears only when the
  media connection stops.
- **Fallback switch.** A Core setting `RemoteDisplayCodecV2` (AppSettings,
  `"True"` by default, no UI). It is read once, when the Core starts, and
  that one value feeds both the capability gate and the `start` check, so
  what a session was told and what its `start` is held to never disagree.
  Set to `"False"` (and the Core restarted), the Core advertises the
  capability to no peer, so every new media connection is v1. It exists for
  side-by-side bench runs and as a field fallback.
- The Step 1 keyframe-every-frame setting applies to v1 connections only. It
  stays until the iPhone app has v2, and is then removed (section 13).

No client breaks: an old window or phone never declares `displayCodec`, so
it is sent no new capability entry and no new bytes, and its `start` is
today's; a new window with an old Core sees no capability and stays on v1; a
new window with a new Core asks and gets v2, or gets v1 when the fallback
switch is off.

## 8. The iPhone app

The phone decodes NSDC v1 in `DisplayFrameDecoder.swift` and asks for
keyframes through `MediaControlClient` (5 a second per endpoint). Plan:

- **In its own change, worked in parallel with the desktop and Core
  change:** a v2 path in `DisplayFrameDecoder` (header and flag rules, the
  Rice plane, the part pairing of section 9.2), `displayCodec: 2` in the
  phone's hello and `displayCodecVersion` 2 in its media `start` when the
  Core advertises it, and its keyframe requests kept for v1 connections
  only. The golden vectors (encoder input rows, v2 messages, split and
  whole, plus malformed cases) live under `tests/data/display_codec_v2/`,
  and both the C++ and the NereusKit tests read the same files, so the two
  decoders are held to one encoder. The phone's decoder must pass the same
  golden vectors before its change lands.
- **Until its change lands:** it never declares `displayCodec` or adds the
  `start` field, so the new Core sends it no new capability entry and v1
  frames, and Step 1's keyframe-every-frame setting removes its smear when
  JJ turns it on.
- Why a change of its own: the phone ships on its own `ios-v` tags through
  App Store Connect, so its change can land separately from the desktop and
  Core change, before or after it. Either order is safe because the Core
  serves both versions and a phone without v2 stays on v1.

## 9. The decoder and the window

### 9.1 Decoder state

A v2 decoder keeps no history a message depends on. It keeps:

- the accepted context (endpoint, generation, interval, row lengths) and the
  last released sequence `L`, for the stale and old-context rules;
- at most one held part of a split frame, `H` (its sequence `h`, which part
  it is, its header and planes), waiting for its partner;
- the last decoded trace, waterfall row and wide row of the context, used
  only to fill a released frame's absent planes (section 9.2), never to
  decode. `reset()` and a change of generation both clear these fill planes,
  so a fill never carries a row or trace across contexts; until the context
  decodes a plane of its own, a fill is minimum dBm.

Order of checks is v1's: the whole message's structure first (header, flag
combination, every plane), then endpoint, context, generation and sequence,
then the message is applied. A message of an older generation is `Rejected`
`OldContext`. A message of a newer generation first releases any held part
as a partial frame under the old context (its fills from the old context's
fill planes; counters as for any release, below: `partsLost` 1, and
`framesLost` from `h` against the old `L`). The decoder then takes the new
context with no `L` and empty fill planes, and the message is handled as
below. Frames lost across the generation change are not counted, since the
two sequences are not comparable. There is no `NeedKeyframe` and no
`NoHistory` in v2.

**Dispositions.** v1's `DisplayCodecDisposition` has `Accepted`,
`NeedKeyframe` and `Rejected` (DisplayCodec.h:40-44), and the window branches
on them (RemoteMediaController.cpp:8487-8509). v2 adds one value and uses
three:

- `Accepted`: one or two frames released, oldest first. The result's `frame`
  holds the first; it gains a frame count (0 to 2) and a second frame, which
  the v1 path never fills. The frame count is 0 on `Held` and `Rejected`.
- `Held` (new): the message was valid and is held as `H`; nothing is
  released, the reason is `None`, and the window does nothing more with it
  (no loss, no keyframe request, no presentation).
- `Rejected`, with the reason in the table below.

A message that releases a frame and is itself held (a part newer than `h`,
or a part of a newer generation while a part is held) is `Accepted` with one
frame. A held part followed by a whole frame newer than `h` releases both,
the held part as a partial frame and then the whole frame: `Accepted` with
two frames.

### 9.2 Pairing the parts of a split frame

The channel is unordered, so either part may come first. "Newer" and "not
newer" use v1's unsigned half-range rule: `s` is not newer than `t` when
`staleOrEqual(s, t)` (DisplayCodec.cpp:136-140, used at 573). "Stale" means
not newer than `L`; with no `L` (a fresh context) nothing is stale.

**Releasing a frame** of sequence `q` (whole, merged or partial) always does
the same two things: when there is an `L`, `framesLost += (q - L - 1) mod
2^32`, in 32-bit unsigned arithmetic; then `L = q`. A released frame is
always newer than `L`, so the difference is 0 to 2^31 - 2 and never wraps
negative. With no `L` (the first release of a context) `framesLost` does not
change. A partial frame also adds 1 to `partsLost`. The "Counter" column
below lists what moves besides that rule.

Within one generation, for a message of sequence `s`:

| Held | Message | Action | Disposition | Counter |
| --- | --- | --- | --- | --- |
| Nothing | Stale (whole or part) | Nothing changes | `Rejected` `StaleSequence` | `staleMessages` |
| Nothing | Whole, newer than `L` | Release it | `Accepted`, 1 frame | release rule |
| Nothing | Part, newer than `L` | Hold it | `Held` | none |
| `H` | Stale (whole or part) | `H` stays | `Rejected` `StaleSequence` | `staleMessages` |
| `H` | Whole or part, newer than `L` and older than `h` | Release it now (a part as a partial frame); `H` stays | `Accepted`, 1 frame | release rule |
| `H` | Part, `s = h`, the other part, headers agree | Release the merged frame; nothing held | `Accepted`, 1 frame | release rule |
| `H` | Part, `s = h`, the same part again | A duplicate; `H` stays | `Rejected` `StaleSequence` | `duplicateParts` |
| `H` | Part, `s = h`, the other part, headers disagree | `H` stays | `Rejected` `Malformed` | `malformed` |
| `H` | Whole, `s = h` | A sequence is whole or split, never both; `H` stays | `Rejected` `Malformed` | `malformed` |
| `H` | Whole, newer than `h` | Release `H` as a partial frame, then release the whole frame; nothing held | `Accepted`, 2 frames | release rule, twice |
| `H` | Part, newer than `h` | Release `H` as a partial frame, then hold the new part | `Accepted`, 1 frame | release rule |

"Headers agree" means every header field except the flags is equal: magic,
version, header size, endpoint, generation, sequence, timestamp, interval and
the three row lengths. The merged frame takes that shared header, waterfall
advance set, the trace from the trace part and the row planes from the row
part. A partial frame takes its own part's header.

`decode()` therefore returns zero, one or two frames, oldest first (the v1
path never returns more than one). At most one part is held at any time, so
the decoder's memory is bounded by one part. The held part is discarded, not
released, by `reset()`, which the window calls on every context accept and
rebind (RemoteMediaController.cpp:8250). Both parts are sent in the same
sender turn, so a hold normally lasts well under a millisecond; a part whose
partner was lost waits for the next message of the endpoint, about one frame
period. For the receive display that wait is inside the presenter's playout
delay. The transmit display draws on arrival (section 9.3), so a transmit
frame whose partner was lost is drawn one frame late.

A released frame always has full-length planes, so nothing downstream sees a
short frame (`SpectrumWidget` checks all three lengths, SpectrumWidget.cpp:
2855-2858; `FilterDisplayItem` checks the waterfall against the trace,
FilterDisplayItem.cpp:195-198):

- Trace part only (its row was lost): the trace, waterfall advance clear, and
  the waterfall and wide planes filled with the last decoded ones (with
  minimum dBm when there are none yet). The window never draws them as a
  row; the next row fills the slot (section 9.3).
- Row part only (its trace was lost): the row planes, waterfall advance set,
  and the last decoded trace (minimum dBm when there is none), marked
  `traceCarried`. The window passes such a frame to the presenter as not
  current for measurement (section 9.3), so the IMD measurement never counts
  a repeated trace.
- A whole frame without a row (`0x08`): the trace, waterfall advance clear,
  the waterfall and wide planes filled the same way.

The decoder's counters (`framesLost`, `partsLost`, `staleMessages`,
`duplicateParts`, `malformed`) feed the window's diagnostics line. A
structural reject (`BadMagic`, `UnsupportedVersion`, `UnknownFlags`,
`Truncated`, `Oversized` or `Malformed`, found before any state is read)
counts `malformed`; a context reject (`ContextMismatch` or `OldContext`)
counts nothing.

### 9.3 Keyframes, the interval and the gap fill

- **Keyframe requests.** The window asks for a keyframe in four places: after
  a decoder `NeedKeyframe` (RemoteMediaController.cpp:8488-8504), and with no
  loss at all, when it accepts a context (:8257), takes a transmit context
  (:8282) and is re-granted (:8400). For a binding on a media connection whose
  `start` carried `displayCodecVersion` 2, or that has decoded a v2 frame, the
  last three are skipped. A v2 decoder never returns `NeedKeyframe`; if a v1
  frame ever reached such a binding, its `NeedKeyframe` still asks, so no
  path is left without recovery. The Core still answers the operation (a v1
  connection needs it); for a v2 endpoint it is a no-op.
- **`kKeyframeInterval` (120):** unchanged for v1; meaningless for v2, whose
  encoder has no history. The Core's "force a keyframe after a refused or
  oversized frame" (DaemonMediaController.cpp:6558, 6596, 7307, 7336) is a
  no-op for v2.
- **Gap fill: unchanged.** The presenter's blend (RemoteDisplayPresenter.cpp:
  161-215) already fills the slots between two real rows from the producer
  timestamps, so a lost row is one blended row between its neighbors and the
  time axis stays true. The repeat path (:286-306) runs only while waiting
  for a keyframe, which a v2 binding never does, so it falls silent. JJ has
  already turned down drawing a lost row as a visible gap, so the blend stays.
  No drawing behavior changes.
- **The presenter's input changes, not its drawing.** `noteLoss()` cannot mark
  one frame: it marks every queued frame and starts a keyframe wait
  (RemoteDisplayPresenter.cpp:241-252). The presenter gains a third argument,
  `push(const DisplayCodecFrame& frame, const RemoteSpectrumCapture& capture,
  bool measurementCurrent = true)`, which sets that item's
  `measurementCurrent`; the window passes `false` for a `traceCarried` frame.
- **The window's other changes, none of them visual:** route version 2 to the
  decoder, loop over the frames an `Accepted` result carries, return early on
  `Held` (section 9.1), skip the keyframe
  requests above, and add `displayMessagesLost` and `displayPartsLost` to its
  display diagnostics beside today's `keyframeWaits` (which stays, and should
  read 0 on v2).
- **Transmit display:** the same encoder and decoder; frames go straight to
  the transmit display as today (RemoteMediaController.cpp:8489-8495,
  8512-8522), with no keyframe request.
- **Mini displays** take frames from the same presenter queue and check the
  trace and waterfall lengths (RemoteMediaController.cpp:8718-8724); the
  filled planes keep them working unchanged.

## 10. Performance

Measured in the scratch program on this Mac (Apple Silicon, one thread, at
low priority while JJ was using it), with a straightforward implementation
(64-bit bit buffer to write, one bit at a time to read):

| Work | 783 px | 4,096 px |
| --- | ---: | ---: |
| v2 encode, one plane | 8 us | 50 us |
| v2 decode, one plane | 13 us | 77 us |
| v1 encode, one frame (two planes, with float quantizing) | 44 us | 395 us |
| v1 decode, one frame | 38 us | 149 us |

The v2 figures leave out float quantizing, which both versions do the same
way. Even so, v2 is no more work than v1: it codes each block up to eight
ways for three block sizes over 8-bit residuals, where v1 searches bit
widths for four block sizes.

Estimates for the Cores and the phone (not measured; the implementation
measures them, below): a Pi 4 (Cortex-A72) runs scalar integer code like this
about 3 to 4 times slower than this Mac, so about 30 us per 783-pixel plane;
four pans at 30 fps is under 1 percent of one core. The worst Rock 5C load
named in v1 (4,096 points, 60 fps, with 3D) is about 200 us of encode per
frame on its A76 cores, about 1 percent of one core. An iPhone's decode
cost is in the range of this Mac's. The implementation adds a timed
benchmark to the codec test (a `QBENCHMARK` over the study scenes) and
records it on the Pi 4 and Rock 5C Cores as part of the bench run in
section 11; the Core session installs those builds.

## 11. Tests

1. **Codec test** (`tst_display_codec_v2`, beside `tst_display_budget`): the
   study scenes, the averaged-trace scenes, and noisy, flat, drifting, raw
   and 4,096-pixel rows. Random drops and Gilbert-Elliott bursts (2 percent
   loss, mean burst 3), applied per datagram, with split and whole frames and
   reordering. Pass when every surviving message decodes alone, bit-exact to
   the encoder's quantized input; the mean frame stays within the section 6
   figures for its scene; no frame exceeds the v1 keyframe for its shape; and
   every `n` from 1 to 4,096 with each wide length keeps `P(n) <= A(n)` and a
   split frame within the admitted cost. Plus the malformed set: every invalid
   flag combination (including `0x04` against the wide length), coding and
   block codes out of range, payload overrun, trailing bytes after the last
   plane or in a payload, nonzero pad, a `z` above 255, truncation at every
   byte, and structure checked before state (as v1's tests do). Plus every row
   of the pairing table in section 9.2, including either part first, a lost
   part, a duplicate, a part older than the held one, disagreeing headers and
   a generation change while a part is held, each checked for its disposition
   (`Accepted` with its frame count, `Held` or `Rejected` with its reason) and
   its counters; `framesLost` with no `L` and across the 2^32 sequence wrap;
   and fills of minimum dBm after `reset()` and after a generation change.
2. **Presenter test:** v2 frames through the decoder and
   `RemoteDisplayPresenter` under the same loss: real rows drawn equals rows
   sent minus rows lost; one blended row per lost row (bursts up to
   `kMaxGapRows`); no repeated rows; no keyframe waits; a `traceCarried` frame
   pushed with `measurementCurrent` false never advances the IMD measurement.
3. **Interop:**
   - a new Core and a client whose hello does not declare `displayCodec`: no
     `displayCodecVersion` entry (its capabilities byte-identical to today's,
     `coreBuildInfo` still last), a `start` carrying the field refused, and
     byte-identical v1 frames (the older-peer golden and link conformance
     fixtures);
   - a new Core and a v2 window: `displayCodec` 2 declared,
     `displayCodecVersion` 2 advertised before `coreBuildInfo`, the
     `start` field sent, v2 frames, and no `keyframe` operation at all,
     through context accepts, re-grants and transmit contexts;
   - an old Core (no capability) and a v2 window: no `start` field, v1 frames,
     keyframe requests and recovery as today;
   - the fallback switch off at Core start: no capability to any peer, v1 to
     a v2 window, which keeps asking for keyframes, and a `start` carrying
     the field refused;
   - a relayed and a tunneled media connection: each `start` agrees the
     codec;
   - a handover: a `replace` keeps v2 (the replacement's frames are v2) and
     the window sends no `keyframe` operation;
   - the golden vectors read by the C++ test now and by NereusKit's test in
     the phone change.
4. **Bench**, 30 to 45 minutes on the STRIX link, Step 1 first, then v2:
   Core `keyframeRequests` flat (zero from the v2 window); window
   `keyframeWaits` 0; at most 1 made-up row (blended plus repeated) per lost
   datagram (today about 7). The Core's `daemon display diagnostics` line
   gains mean bytes per frame and the count of split frames and refused
   second parts (today it logs only the largest frames).

## 12. What changes where

- `DisplayCodec.{h,cpp}`: a v2 encoder and the decoder's v2 path, `decode()`
  returning 0 to 2 frames, the new counters.
- `DaemonMediaController`: the media `start` parser takes the field
  (`handleStart`, about DaemonMediaController.cpp:2215-2384); the agreed
  version is kept beside `m_startMicLine` across a `replace`; the encoder
  chosen per media connection from it, split and send order, totals for
  pacing, the diagnostics fields.
- `StationServer`: the gate function (the hello declaration, media, minor 11
  and the `RemoteDisplayCodecV2` setting read once at Core start), used both
  for the capability and by the `start` parser.
- `StationCapabilities`: the `displayCodecVersion` entry, before
  `coreBuildInfo`, for a peer that declared `displayCodec` 2.
- `StationClient`: the `displayCodec: 2` hello declaration.
- `RemoteMediaController`: the `start` field, routing, the frame loop and
  `Held`, the "`start` carried v2" flag kept across a `replace`,
  `traceCarried`, the skipped keyframe requests, the new counters.
- `RemoteDisplayPresenter`: `push()` gains `measurementCurrent` (its drawing
  is unchanged).
- Other decoders, which stay on v1 by never sending the `start` field, and
  are listed so nobody is surprised: `tools/nereus-media-probe.cpp:606` and
  `tests/tools/nereus_rendezvous_peer.cpp:966`. Either can later move to v2
  by declaring `displayCodec` 2 in its hello, adding the `start` field when
  the Core advertises it, and reading v2 with the same decoder.
- Fixtures: `tests/data/link/v1/surface.json` and the link conformance
  fixtures gain the hello declaration, the capability (only for a peer that
  declared it) and the `start` field; every existing fixture for a peer that
  does not declare it stays byte for byte.
- Docs: this file, a pointer from the v1 document, the media control
  document's `start` row and a "display codec" section beside "The picture a
  context continues", and station link v1's hello features and capability
  list.
- Unchanged: `DisplayBudget`, the allocators, the presenter's drawing, the
  mini displays, the NSDX extras, the PS3D codec, relays and the tunnel. The
  phone changes in its own change, worked in parallel (section 8).

## 13. Decisions (JJ, 2026-10-22)

JJ answered all six questions on 2026-10-22. Five follow the recommendation;
the fourth changes it.

1. **Split two-datagram frames into a row message and a trace message, row
   first.** On the 783-pixel pan a lost datagram then costs one row or one
   trace, not both, for about 5 percent more bytes on those frames
   (section 5).
2. **Keep the hidden Core fallback setting** `RemoteDisplayCodecV2`, read
   once at Core start, with no UI (section 7). It serves bench comparisons
   and is a quick way back in the field.
3. **On by default, merged only after the STRIX bench shows at most one
   made-up row per lost datagram** (section 11, item 4).
4. **Changed from the recommendation: the iPhone gets v2 in its own change,
   worked in parallel with the desktop and Core implementation, not after
   it.** The phone change can land separately, and its decoder must pass the
   same golden vectors (section 8).
5. **Accept whole-row loss on pans wider than about 1,700 pixels for now,**
   and look again after the STRIX bench (section 5).
6. **Keep the keyframe-every-frame setting until the iPhone has v2, then
   remove it** (section 7).
