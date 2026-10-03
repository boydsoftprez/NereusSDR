# Display extras v1

The display computations the desktop runs on its own spectrum and waterfall
(the peak blobs, the active peak hold trace, the noise-floor line, the
waterfall's automatic levels, normalise, the calibration offset and the
averaging constant) are ports of Thetis. An app does not carry them (iPhone
app design, section 4.11, D4). A spectrum subscription asks the Core to run
them, and the Core sends their results on the display channel in an NSDX
datagram beside each NSDC frame (display codec v1). The Core runs the same
code the desktop's `SpectrumWidget` runs (`src/core/spectrum`:
`PeakBlobDetector`, `ActivePeakHoldTrace`, `DisplayFollowers`, and
`ClarityController`), so for the same rows and settings the numbers agree
(within 0.01 dB, `tst_display_extras`).

Requirements: R-IOS-27, R-IOS-11. Capability: `displayExtrasVersion` 1.

Version 2 (R-IOS-27, R-IOS-06) adds the media control operation
`clarity-retune`, Clarity's Re-tune for one endpoint whose waterfall
levels are in `"clarity"` mode ([remote media control
v1](2026-09-20-remote-media-control-v1.md), "Clarity re-tune"). The Core
sends 2; everything below needs 1, so a client that compares the version as
a minimum reads 2 as it read 1.

Version 3 adds the optional `onTx` member of `activePeakHold` and makes the
Core's peak hold behave as the desktop's now does (Thetis display.cs:5011,
5333-5364 [v2.10.3.15]): a bin raised in a frame holds for `holdMs` before it
falls, and while the endpoint's slice is transmitting (MOX on and it is the
transmit slice) a trace without `onTx` true neither updates, falls nor is
sent. `onTx` absent is true, so an app that never sends it sees its trace run
through transmit as before. The Core sends 3; a client sends `onTx` only to
a Core that sent 3.

Version 4 adds the optional `fastAttack` member of `noiseFloor` and the
noise floor state section (bit `0x10`): one byte after the waterfall
levels, bit 0 set while the noise floor is in fast attack (after a band
change, a tune of more than 0.5 MHz or a MOX edge, until the floor has
settled, as the desktop's `NoiseFloorFollower`). The desktop draws the line
in its fast-attack colour then, and its grid tracking ignores a floor in
fast attack. The Core sends the section only to an endpoint whose
`noiseFloor` is enabled with `fastAttack` true; `fastAttack` absent is
false, so an app that never sends it gets exactly the version 3 datagrams.
The Core sends 4; a client sends `fastAttack` only to a Core that sent 4.

## 1. Who may ask

The Core advertises `displayExtrasVersion` in the minor-11 block of
the capabilities message (station link section 6.3), 3 while media is
enabled and 0 otherwise. A client may put the fields of section 2 in a
`subscribe` operation only when the Core it is connected to sent 1 and the
session agreed minor 11. A Core that did not advertise it, or a subscription
at an older minor, refuses a `subscribe` carrying any of them as one it
cannot read (as it refuses any unknown key today).

A subscription without any of the fields gets exactly today's frames, bytes
and charge: no NSDX datagram, no shift, the averaging constant the plane
objects name. `tst_display_extras` holds this to a golden: a fixed run
(`tests/OlderPeerDisplayRun.h`) recorded from the Core before display
extras existed (`535dd412`), in `tests/data/display_extras_older_peer_frames.json`,
compared charge, display control messages and every datagram byte for
byte. The extras belong to the subscription: each endpoint asks for
its own, and a new `subscribe` for the endpoint replaces them and starts
their computations again. Nothing in them is per device.

## 2. The subscription fields

All optional; each present field asks for what it names. The ranges are the
desktop's (its Setup pages and `SpectrumWidget`'s setters). A field with
another shape, a member not listed, or a value out of range makes the
request one the Core cannot read.

| Field | Shape | Meaning |
| --- | --- | --- |
| `peakBlobs` | `{count, holdMs, fallDbPerSec, insideOnly}` | The top-N peak markers. `count` 1 to 20. `holdMs` 0 (no hold: markers follow each frame) or 100 to 60000. `fallDbPerSec` 0 (a marker disappears at the end of its hold) or 1 to 60 (it falls at this rate after the hold). `insideOnly` true keeps markers inside the slice's receive filter. Asks for the blob section. |
| `activePeakHold` | `{enabled, holdMs, fallDbPerSec[, onTx]}` | The active peak hold trace. `holdMs` 100 to 60000: a bin raised in a frame holds this long before it falls. `fallDbPerSec` 0.1 to 120. `onTx` (version 3, optional, true when absent): keep the trace running while the endpoint's slice transmits; when false the section is not sent while it transmits. Asks for the peak hold section while `enabled`. |
| `noiseFloor` | `{enabled, shiftDb[, fastAttack]}` | The noise-floor line, moved by `shiftDb` (-12 to 12). Asks for the noise floor section while `enabled`. `fastAttack` (version 4, optional, false when absent): also ask for the noise floor state section while `enabled`. |
| `waterfallLevels` | `{mode, lowDbm, highDbm, offsetDb}` | The waterfall's low and high levels. `mode` `"manual"` (the operator's `lowDbm` and `highDbm`, -400 to 100 each), `"agc"` (the desktop's follower on each line's minimum and maximum, 12 dB outside them), `"noiseFloorAgc"` (the line's 10th-percentile floor plus `offsetDb`, -60 to 60, and 60 dB above that) or `"clarity"` (Clarity, fed the Core's full-source noise floor, the same the `noise-floor` operation carries; the manual levels until Clarity first speaks). Asks for the levels section. |
| `normalize` | boolean | Normalise to a 1 Hz bandwidth: every dBm moves by -10 log10(bin width), the bin width being the source's sample rate over its FFT size. It applies only while the subscription's `trace.detector` is 2 (Average), 3 (Sample) or 4 (RMS), as Thetis (specHPSDR.cs updateNormalizePan) and the desktop apply it; with Peak or Rosenfell the request is kept and moves nothing. |
| `calibrationOffsetDb` | number, -30 to 30 | The display calibration offset: every dBm moves by it. |
| `averageTimeMs` | whole number, 10 to 9999 | The spectrum's averaging time (the desktop's spectrum Averaging Time). The Core computes the constant exp(-1 / (fps × τ)) from it at the endpoint's `fps` and uses it for the trace plane in place of its `averageAlpha`, and for the waterfall plane too when `waterfallAverageTimeMs` is absent; `averageMode` still comes from the plane objects. |
| `waterfallAverageTimeMs` | whole number, 10 to 9999 | The waterfall's averaging time (the desktop's waterfall Averaging Time). The Core computes the waterfall plane's constant from it the same way. Absent, the waterfall takes `averageTimeMs`'s; with neither, the plane object's `averageAlpha`. |

**Calibration and normalise are applied at the Core.** The Core adds
`calibrationOffsetDb` plus the normalise shift to every sample of the
trace, waterfall and wide rows before it encodes the NSDC frame, and to
every dBm in the NSDX datagram. An app draws what it receives: it adds
nothing, and colours the waterfall row against the levels the datagram
carries (which are moved by the same amount). The frame's quantisation
interval (`minDbm`, `maxDbm`) does not move: a moved sample outside it is
clipped as any sample is.

**What the Core also follows**, as the desktop does for its own pan: a band
change or a tune of more than 0.5 MHz of the endpoint's slice, and either
edge of transmit, put the noise floor into fast attack; either edge of
transmit re-primes the `"agc"` follower; while transmitting the waterfall's
levels hold and Clarity pauses. A new endpoint context (a retune, a zoom, a
new FFT size) starts the peak hold and the blobs again.

**The charge.** An endpoint that asks for any section is charged for its
datagrams on top of its frames (station link section 12.3, display budget):
one message a frame more, at most the datagram's largest size a frame
(section 3.3), and the peak hold row's samples a frame. An endpoint that
asks only for `normalize`, `calibrationOffsetDb`, `averageTimeMs` or
`waterfallAverageTimeMs` is charged as today.

## 3. The NSDX datagram

### 3.1 When it is sent

After the Core sends an endpoint's NSDC frame, and while that endpoint asks
for at least one section, it sends one NSDX datagram for that frame, on the
next send and before any other frame, on the same display channel. It
carries the frame's endpoint id, context generation and encoder sequence,
so a receiver pairs it with its frame. It is never resent: a datagram the
channel refuses is dropped (and counted as a refusal), and the next frame
brings its own. A newer frame's datagram replaces one not yet sent. A
receiver that gets a frame and no datagram keeps drawing the extras it has.

The datagram is stateless: it decodes on its own against the endpoint's
accepted context, with no history and no `after`.

### 3.2 Layout

All multibyte integers are unsigned network byte order; floats are
IEEE-754 binary32 bit patterns written as unsigned 32-bit network-order
integers, as in display codec v1. The fixed header is exactly 20 bytes:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII magic `NSDX` |
| 4 | 1 | version, `1` |
| 5 | 1 | sections: `0x01` peak blobs, `0x02` active peak hold, `0x04` noise floor, `0x08` waterfall levels, `0x10` noise floor state (version 4); at least one; any other bit refuses |
| 6 | 2 | header size, `20` |
| 8 | 4 | endpoint ID |
| 12 | 4 | context generation |
| 16 | 4 | encoder sequence of the NSDC frame it goes beside |

The sections follow in bit order, each present only when its bit is set:

| Section | Bytes |
| --- | --- |
| Peak blobs | `count:u8` (0 to 20), then `count` entries of `pixel:u16` (the trace sample the peak is at, below the context's trace length) and `dbm:f32`, strongest first |
| Active peak hold | One NSDC v1 plane of exactly the context's trace length, every block absolute (a keyframe plane: display codec v1, "Byte layout"), quantised on the context's `minDbm..maxDbm`. A sample the hold has not reached yet travels as `minDbm`. |
| Noise floor | `dbm:f32`, where the line is drawn: the smoothed estimate plus `shiftDb` |
| Waterfall levels | `lowDbm:f32`, `highDbm:f32`, the levels in force for the latest waterfall line |
| Noise floor state | `state:u8`: bit 0 set while the noise floor is in fast attack; any other bit refuses |

Nothing may follow the last section. Every float is finite.

### 3.3 Size

The largest datagram, every section at the largest trace (4096 samples), is
`20 + (1 + 20 × 6) + (3 + 5 × 32 + 4096) + 4 + 8 + 1 = 4413` bytes, within the
16 KiB display message bound of display codec v1 and six SCTP DATA
fragments. In general it is at most
`20 + 121·[blobs] + A(trace)·[hold] + 4·[floor] + 8·[levels] + 1·[state]`, where
`A(n) = 3 + 5·ceil(n/128) + n` (display codec v1, "Measured sizes and
fragments"). The Core charges this size (section 2).

### 3.4 Decoding and refusals

A receiver checks, in order: the size (over 16 KiB: `oversized`), the magic
(`badMagic`), the version (`unsupportedVersion`), the rest of the header
(`truncated`), unknown section bits (`unknownSections`), no section or a
header size other than 20 (`malformed`), the endpoint and generation
against its accepted context for that endpoint (`contextMismatch`), then
each section (`truncated` when bytes run out, `malformed` for more than 20
blobs, a blob past the trace, a float that is not finite or a peak hold
plane that is not an absolute plane of the trace's length), and finally
that no byte follows (`malformed`). A refused datagram changes nothing.

## 4. Conformance

The media vectors (station link section 16.4), codec `nsdx1`, are written by
`tst_link_conformance_regen` and run by `tst_link_conformance_media`.
Each expectation names the context it decodes against:

```
{"codec": "nsdx1", "expect": {
   "context": {"endpointId", "contextGeneration", "minDbm", "maxDbm", "traceSamples"},
   "accepted": true|false, "reason": "none"|"<refusal>",
   "endpointId", "contextGeneration", "encoderSequence",
   "peakBlobs": [{"pixel", "dbm"}], "peakHoldDbm": [...],
   "noiseFloorDbm", "waterfallLevelsDbm": {"lowDbm", "highDbm"},
   "tolerance": {"dbm": 0.01}}}
```

The frame fields and the sections appear only for an accepted datagram,
and only the sections it carries; `tolerance` applies to every number of
the decoded values (the peak hold row is the decoder's dequantised
values). A runner decodes the bytes against `context`, compares
`accepted` and `reason` exactly, requires exactly the listed keys, and
compares the numbers within the tolerance.

| Vector | Decoded values |
| --- | --- |
| `nsdx1-full` | Every section: three blobs, a 32-sample hold row, the floor, the levels; accepted |
| `nsdx1-noise-floor` | The floor alone; accepted |
| `nsdx1-other-generation` | `nsdx1-full`'s bytes against generation 2; `contextMismatch` |
| `nsdx1-unknown-section` | `nsdx1-full` with section bit `0x10` set; `unknownSections` |
| `nsdx1-truncated` | `nsdx1-full` with its last byte cut off; `truncated` |

The encoder is exact: the station's runner also encodes the fixed inputs
again and compares the bytes.
