# Display codec v1

`DisplayCodec` carries reduced dBm display rows on the R3 unreliable media
channel. It has no socket, GUI, radio, or Qt Multimedia dependency. The codec
is NereusSDR-original; its input is the output of the independent trace and
waterfall reducers.

R-R3-11: production rows are antenna-referenced dBm. Core applies the
station calibration before reduction/quantization, including the optional
wide row. A remote renderer must not add its own local radio calibration.
This corrects the missing level calibration at the first display checkpoint;
it does not change the v1 packet layout.

## Bounds and context

One frame has an endpoint ID, a context generation, a 32-bit encoder sequence,
a sender-monotonic nanosecond timestamp, a finite `minDbm..maxDbm` quantisation interval,
and two required rows (trace and waterfall) plus one optional DSS-wide row.
Every row has 1 through 4096 samples. The interval and all row lengths are
part of the wire context. A context change requires a new generation and a
keyframe.

The timestamp is only ordered against frames from the same sender; it is not
comparable to the receiver's local clock. Source sample position and source
timing are accepted-context/endpoint-layer fields, not v1 codec fields.

One encoder instance belongs to one endpoint. The caller advances its context
generation for every range or row-shape change, and advances the sequence
strictly within that generation. A newer generation may restart the sequence.
The encoder rejects a same-generation context-shape change and stale/equal
sequence rather than emitting a frame the decoder must reject.

Inputs outside the interval are clipped. A finite input maps to the nearest
integer in `[0,255]`, using `round((dbm - minDbm) * 255 / (maxDbm - minDbm))`.
The normal reconstructed error for in-range inputs is at most half a quantum,
where `quantum = (maxDbm - minDbm) / 255`.

The encoder can use a non-negative integer dead zone in quantised units. When
the newly quantised value differs from the previous reconstructed value by at
most that dead zone, it retains the reconstructed value. Its error bound is
then `(deadZone + 0.5) * quantum`; each comparison is against the original
newly quantised input and the decoder reconstruction, so error does not
accumulate along a delta chain. Version 1 uses dead zone zero by default.

Frames are bounded to 16 KiB by the parser and the encoder. This is a codec
allocation limit, not the media transport datagram limit: transport owns
fragmentation and its 1000-byte total-packet cap.

## Byte layout

All multibyte integers are unsigned network byte order. Floating-point values
are IEEE-754 binary32 bit patterns written as unsigned 32-bit network-order
integers. No native C++ object is serialized.

The fixed header is exactly 42 bytes:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII magic `NSDC` |
| 4 | 1 | version, `1` |
| 5 | 1 | flags: `0x01` keyframe, `0x02` waterfall advance, `0x04` wide row; all other bits reject |
| 6 | 2 | header size, `42` |
| 8 | 4 | endpoint ID |
| 12 | 4 | context generation |
| 16 | 4 | encoder sequence |
| 20 | 8 | sender-monotonic timestamp in nanoseconds |
| 28 | 4 | minimum dBm |
| 32 | 4 | maximum dBm |
| 36 | 2 | trace length |
| 38 | 2 | waterfall length |
| 40 | 2 | wide-row length, zero when absent |

The header is followed by one encoded plane in trace, waterfall, then wide
order. Each plane begins with `blockSizeCode:u8` (`0=16`, `1=32`, `2=64`,
`3=128`) and `blockCount:u16`. The chosen size applies to the entire plane.
The block count must exactly cover the declared plane length, with all nonfinal
blocks at the chosen size.

Each block has `mode:u8`, `bitWidth:u8`, `sampleCount:u8`, `payloadBytes:u16`,
then its payload. `mode=0` is a signed temporal residual, and `mode=1` is an
absolute quantised value. Absolute blocks always use bit width 8 and exactly
one byte per sample. Residuals have widths 1 through 8; values are packed MSB
first, each signed residual biased by `1 << (bitWidth - 1)`. A residual block
whose values do not fit that representation falls back to absolute mode.
Keyframe blocks must all be absolute. Delta residuals are relative to the
decoder's previous reconstructed plane, never to a sender-only float input.

The encoder evaluates the four block sizes and deterministically selects the
one with the smallest block payload plus headers; equal costs select the
smaller size. This adapts stable rows and noisy/peaked rows without changing
the decoder contract.

## State and recovery

The first frame, every context change, every 120th frame, and an explicit
keyframe request are keyframes. The decoder accepts a delta only when the
endpoint/context match its accepted history and the sequence is exactly the
next unsigned 32-bit sequence. A loss or forward gap returns `NeedKeyframe`
and leaves history untouched until a complete keyframe arrives. Older/equal
sequences use the unsigned half-range rule and reject as stale. An older
context generation rejects; a newer generation needs a keyframe.

Any malformed packet (including unknown flags, invalid floats/context, bad
plane count/length, block overrun, invalid packing, truncation, or packet over
the codec bound) returns `Rejected` without changing the prior accepted frame
or reconstruction history.

The decoder checks a datagram in this order:

1. The whole datagram's structure: the codec bound, the header, and every
   plane (plane count and lengths, block sizes, packing, overrun,
   truncation, floats).
2. Then the endpoint, context, generation, sequence and history rules.
3. Then the datagram is applied.

A malformed datagram returns `Rejected` with its structure reason, whatever
state the decoder is in, and history is untouched. A delta with a malformed
plane sent to a fresh decoder is `Rejected` `Malformed`, not `NeedKeyframe`
`NoHistory`; a malformed delta with a stale sequence is `Rejected`
`Malformed`, not `StaleSequence`. The only check left for step 3 is one that
needs history: a residual that would take a reconstructed value outside
`[0,255]` returns `Rejected` `Malformed`, also without changing history. A
delta that arrives after a gap is checked for structure and then returns
`NeedKeyframe` without being applied, so that check never runs against a
stale plane.

## Measured sizes and fragments (R-R3-03, R-R3-05)

A plane of `n` samples costs at most `A(n) = 3 + 5*ceil(n/128) + n` bytes:
the 3-byte plane header, one 5-byte block header per 128 samples, and one
byte per sample. A keyframe is exactly that worst case, because every block is
absolute and the encoder then picks 128-sample blocks:

    keyframe = 42 + 2*A(pixels) + A(wide)     (A(0) = 0 without a 3D row)

A delta is never larger: a residual block costs no more than the absolute
block it falls back to, and the encoder takes the cheapest block size.
`tst_display_budget` checks both rules over every combination of 15 pixel
counts and 6 wide-row lengths with noisy, drifting and flat rows.

The transport sends a display message of `n` bytes as `ceil(n / 876)` SCTP
DATA chunks, one UDP datagram each. 876 is the DATA payload left in the
1000-byte media packet budget: libdatachannel sets the SCTP path MTU to
1000 - 12 - 48 - 8 - 40 = 892, usrsctp adds its 12-byte common header
back (904) and fragments at 904 - 12 - 16 = 876
(`IMediaTransport::kSctpDataPayloadBytes`).

| Shape (trace/waterfall/3D) | Keyframe bytes | Fragments |
| --- | ---: | ---: |
| 1024/1024/none | 2,176 | 3 |
| 1024/1024/768 | 2,977 | 4 |
| 4096/4096/none | 8,560 | 10 |
| 4096/4096/768 | 9,361 | 11 |
| PureSignal chunk, 64 KiB | 65,536 | 75 |

The channel is unordered with no retransmission, so losing any fragment loses
the whole message; larger frames are proportionally more exposed to loss.
Core logs the largest keyframe and delta it actually sent, with their
fragment counts, as `daemon display diagnostics` (every 10 seconds while they
change, and once when the media peer ends). The Rock check at 4096 points,
60 fps with 3D, is that this line never exceeds 9,361 bytes / 11 fragments and
that a capture shows no media datagram above 1000 bytes (969 on IPv4).

## Transport limits (R-R3-04, R-R3-09)

Display frames follow latest-value-wins at the producer, never a queue.
Each process applies these SCTP settings once, before its first media peer
(`applyMediaSctpSettingsOnce()`):

- Send buffer 65,536 bytes (`kSctpSendBufferBytes`), down from the library's
  1 MiB. It cannot be smaller: it is the 64 KiB maximum message that
  PureSignal chunks need.
- Receive buffer 131,072 bytes (`kSctpReceiveBufferBytes`), twice the maximum
  message, so every message is delivered whole.

usrsctp takes a message only while it fits in the send buffer beside the data
not yet acknowledged. When it does not, libdatachannel takes the message and
holds it until there is room: that message is still sent, exactly once, is
measured like any other, and is counted as `queuedLate`. While the library
holds a message the display channel takes nothing more, and the Core offers
nothing more: each spectrum endpoint keeps only its latest input, and a
PureSignal snapshot keeps its place. When the held message goes out, the
display channel reports that it is writable and the Core offers the next
message at once, so a PureSignal snapshot advances at the link's
acknowledgement pace instead of needing an acknowledgement within one send
tick. A newer PureSignal snapshot still replaces the one waiting to start.

A display message offered and not taken is a refusal (`sendRefusals`). A
refused spectrum frame is dropped, never resent, and the endpoint's next
frame is a keyframe. A PureSignal chunk refused only because the channel was
busy is offered again later (it was never sent, so this is not a resend); a
chunk the transport refused outright abandons the rest of its snapshot. An
error on the display channel itself counts (`transportErrors`), is logged
once per distinct text per media peer, and is handled like a failed send;
signalling and other media errors are logged once but not counted. What is
bounded by construction: the library holds at most one display message (a
further one is Busy), and SCTP's send buffer is set to 64 KiB and its
receive buffer to 128 KiB. Those figures are not a whole-path limit, and
this document does not claim one. What was measured: with a stalled
receiver, `tst_media_transport` offers the largest spectrum frame (9,361
bytes) until a full second passes in which nothing is taken, and the path
took exactly 26 frames (243,386 bytes; 13 sent at once, 13 held by the
library) on every run. The test holds it to that measurement plus one frame
(252,747 bytes); with libdatachannel's defaults 1,376,067 bytes piled up.

The GUI keeps at most 32 received display messages or 256 KiB between
drains, dropping the oldest first (8 before R-R3-21, which dropped the middle
of the delta chain in the burst a 480 ms stall releases). Each dropped message is counted
(`displayMessagesDropped`) apart from the received bytes, which count every
arrival, and the GUI log reports drops at most every 10 seconds.
