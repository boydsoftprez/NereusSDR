# Remote media control version 1

This is the R3 implementation contract for R-R3-02 through R-R3-09.
Session protocol minor 1 and capability `remoteMediaVersion=1` negotiate it.
An authenticated, snapshot-complete active control session is required in
both directions. Older clients retain the R2 control protocol.

Control messages use the existing WSS envelope:

```json
{"type":"media.control","payload":{"op":"start","connectionId":"canonical-uuid"}}
```

The complete encoded envelope is limited to 128 KiB. Each operation has an
exact set of keys. Integers are JSON numbers checked for range and integrality
before narrowing; strings and numeric values are never coerced. Display
arrays and audio packets use the separate encrypted media connection.

## Media peer

`connectionId` is a non-null lowercase UUID with hyphens and no braces.
The GUI creates it after the control snapshot and sends `start`; Core is
the offerer and the GUI the answerer. Every subsequent operation carries
the same ID. Control-session replacement retires the old peer, subscriptions,
codec histories and callbacks, including deliberate silent redials.

A `start` on a new `connectionId` while the Core still holds a peer for the
same session replaces that peer; it is never refused. One media controller
serves one device's session, so the peer it holds is that device's own:
usually a half-open one whose media died on the app's side before the Core's
ICE consent check noticed. The old peer is torn down, its endpoints retired
and its display demand removed (every other device's budget share follows,
`DaemonMediaController::retirePeerKeepingSession`), and nothing is sent for
the old `connectionId`. A different device has its own session and its own
controller, so its media is untouched. A device that signs in again gets a
new session; its older connection ends with `sameDevice` and its media with
it (the several-devices design, ruling 4.8).

When the Core drops a media peer on its own, it tells the app at once with
the whole-peer `rejected` (`endpointId` 0, `revision` 0) for that peer's
`connectionId`, before it clears the peer, so the app starts media again
without waiting for its own peer to time out. It is sent when the Core's
transport reports the connection failed (ICE consent lost, a failed DTLS
handshake), reason exactly "The Core lost the audio and display
connection.", and when the connection closes without a reported failure,
reason exactly "The audio and display connection to the Core closed."
(`kMediaPeerLostReason` and `kMediaPeerClosedReason`, `MediaPeer.h`). It is
not sent when the session itself ends (the control link is gone) or when a
new `start` replaces the peer. An app treats these two reasons as "media is
gone, start again"; every other whole-peer `rejected` (the Core could not
start a peer) ends media for that connection. The desktop's remote window
starts its media over through its usual recovery on the two drop reasons.

| Operation | Exact payload fields beyond `op` and `connectionId` |
| --- | --- |
| `start` | None; a GUI whose Core advertised `audioProfileVersion` adds `audioProfileVersion`, a whole number of at least 1 (anything else is refused and no peer starts); a GUI whose Core advertised `receiverAudioVersion` may add `receiverAudioVersion` the same way (see Receiver audio), one whose Core advertised `headphonesMixVersion` may add `headphonesMixVersion` the same way (see Headphones mix), one whose Core advertised `remoteTxVersion` may add `remoteTxVersion` the same way (see Microphone line), and one whose Core advertised `txDisplayVersion` may add `txDisplayVersion` the same way (see Transmit display) |
| `description` | `sdp`, `type` (`offer` or `answer`, appropriate to peer role) |
| `candidate` | `candidate`, `mid` |

SDP is limited to 64 KiB. Candidate and MID strings are limited to 4 KiB and
256 bytes respectively; NUL is forbidden. At most 64 remote candidate
controls are admitted, including candidates buffered before the description.
The R3 backend accepts host ICE candidates. SDP-embedded candidates are
rejected: this version uses bounded trickle candidates exclusively. This
provides direct LAN media,
and does not close the separate R5 TURN/TCP, TURN/TLS or NAT traversal gates.

Display is an unordered SCTP data channel with zero retransmissions, secured
by DTLS. Audio is separate RTP/SRTP, payload type 111. The session audio SSRC
is derived by SHA-256 over the ASCII prefix `NereusSDR/media-audio-ssrc/v1:`
followed by the canonical connection UUID. The first four digest bytes form
an unsigned big-endian integer; zero maps to one. Both peers use this ID,
and reject mismatching RTP. SSRC is a routing identity; authentication comes
from the pinned WSS session and its negotiated DTLS peer.

A peer is ready when its connection is up and its display channel and audio
line are open. Each peer reports ready before any display message, raw I/Q
message or audio packet that arrived with that opening, so the first display
message a peer handles always finds it ready, and a reply sent from that
handler is taken. An earlier build could report such a message first; the
reply was then refused and, on the unreliable display channel, never sent
(addendum G-127). Transmit keepalives on the `tx` channel are not ordered
against ready: they are reported as they arrive, and sending one needs only
the `tx` channel open, not a ready peer.

The speakers' mix and the headphones mix are Opus at the Core's
`audio_bitrate`: 48000 bit/s with fullband sound (audio up to 20 kHz) by
default for every mode (R-R3-21, operator decision of 2026-09-26), or 24000
bit/s with wideband sound (up to 8 kHz) when the Core sets `audio_bitrate =
24000`, which stays accepted. Either way the packets are 48 kHz stereo, 40 ms
(1920 samples), constrained VBR, with Opus in-band FEC off: FEC lives only in
Opus's speech layer, turning it on moves every packet to that layer, which at
48000 bit/s gives up most of the sound above 8 kHz, and it rebuilds at most
the one packet before, never a burst. The media offer's `maxaveragebitrate`
is that same rate. The `encoder` object of each audio context reports the
rate in use, so a GUI assumes none.

Receiver audio streams (R-R3-43) share the one audio m-line and its SRTP
context with the main stream; only their SSRCs differ. Receiver stream `n`,
for `n` from 0 to 3, has the SSRC formed the same way from the ASCII prefix
`NereusSDR/media-receiver-ssrc/v1:`, the decimal `n`, a colon, then the
canonical connection UUID. A value that is zero, equal to the main SSRC or
equal to an earlier receiver's SSRC is replaced by the next integer (modulo
2^32) until it is none of these, so the five IDs are distinct and both peers
derive the same set. The receiver streams are declared only when both peers
start the media connection with receiver audio (the GUI added
`receiverAudioVersion` to its `start`; see Receiver audio).
Then Core's offer carries, after the main stream's
`a=ssrc:<main> cname:nereus-mixed-stereo` line, one
`a=ssrc:<receiver n> cname:nereus-receiver-<n>` line per receiver in order,
and each peer sends and accepts exactly the main SSRC and the four receiver
SSRCs. Without it the offer, the answer and every audio line are exactly as
before, and only the main SSRC is sent or accepted. RTP with any other SSRC
is refused and reported. The transport library itself does not refuse it:
libdatachannel v0.24.5 hands every packet on a connection with one media
line to that line's track whatever its SSRC (measured, not assumed), so the
refusal is the media peer's receive filter. Each peer's queue of received
RTP between drains holds 64 packets per declared stream, 256 ms of lossless
audio (250 packets/s) for every stream: 64 packets without receiver
streams, 320 with them, and 64 more with the headphones mix; when full, the
oldest packet is dropped.

The headphones mix (R-R3-45) is one more stream on the same m-line and SRTP
context. Its SSRC is formed the same way from the ASCII prefix
`NereusSDR/media-headphones-ssrc/v1:` followed by the canonical connection
UUID; a value that is zero, the main SSRC or any of the four receiver SSRCs
(declared or not) is replaced by the next integer (modulo 2^32) until it is
none of these. It is declared only when the GUI added
`headphonesMixVersion` to its `start`: Core's offer then carries, after any
receiver lines, one `a=ssrc:<headphones> cname:nereus-headphones-mix` line,
and each peer also sends and accepts that SSRC. Without it nothing above
changes.

The microphone line (iPhone app plan Task 36) is a second audio m-line,
`a=mid:mic`, after the main one, which the client sends on and the Core
receives. It is offered only when the client added `remoteTxVersion` to
its `start` (see Microphone line); without it the offer, the answer and
every audio line are exactly as before. Core's offer declares it
receive-only (`a=recvonly`), with no SSRC of the Core's, carrying Opus as
`a=rtpmap:111 opus/48000/2` with
`a=fmtp:111 minptime=10;useinbandfec=1;stereo=0;maxaveragebitrate=48000`
(RFC 7587 section 6.1: the most the Core will receive on average, so the
ceiling of every microphone encoder on the line),
and, when the offer carries the lossless format on the main line, the same
`a=rtpmap:96 L16/48000/2` after Opus. The answer takes it send-only and
declares the microphone's SSRC on it:
`a=ssrc:<microphone> cname:nereus-microphone`. That SSRC is formed from the
ASCII prefix `NereusSDR/media-mic-ssrc/v1:` followed by the canonical
connection UUID; a value that is zero, the main SSRC, any of the four
receiver SSRCs or the headphones SSRC (declared or not) is replaced by the
next integer (modulo 2^32) until it is none of these. The client sends only
that SSRC on the line and the Core accepts only it; the line joins the
bundle and the lip-sync group (`a=group:BUNDLE audio mic 0`,
`a=group:LS audio mic`). The Core's queue of received RTP holds 64 more
packets for it.


## Replacing the media connection (replace)

iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4;
capability `mediaReplaceVersion` 1, which a Core sends whenever media is
on). A GUI moves its media to a new peer connection without a gap: a new
peer is made beside the current one, audio arrives on both for a moment,
and the old one retires. It is how media follows a control session that
moved to a better path (the link document, section 21.3).

| Direction | Exact payload fields beyond `op` and `connectionId` |
| --- | --- |
| GUI to Core | `replaces`: the current peer's `connectionId`; optional `mediaDirectVersion` 1 (below) |
| Core to GUI | `replaces`: the peer that retired |

`connectionId` is the new peer's, a new canonical UUID. The new peer takes
everything the current one negotiated in its `start` (the audio profile,
receiver streams, headphones mix, microphone line and transmit display);
`replace` carries none of `start`'s version keys.

1. The GUI sends `replace` once its current peer is ready. The Core refuses
   with the whole-peer `rejected` (`endpointId` 0, `revision` 0) for the
   new `connectionId`, and the current peer carries on, when `replaces` is
   not its current peer's id, the new id is the current one, the current
   peer is not ready, a replacement is already under way, or the radio is
   transmitting or switching between receive and transmit (`MoxController`
   not idle): "The Core did not move audio and display: {reason}.", where
   the reason is "the radio is transmitting" for the last and "that
   connection is not the current one" for the others.
2. The Core makes the new peer as offerer, with the ICE settings of the
   session's control connection now (the link document, section 20), and
   trickles its `description` and `candidate` operations under the new
   `connectionId`; the GUI answers under it. Everything keeps running on
   the current peer meanwhile.
3. Once the new peer is ready, the Core sends each audio stream on both
   peers: every packet goes on each with the same RTP sequence number and
   timestamp, and each peer's own SSRCs (derived from its own
   `connectionId`, as above). Display and the "tx" data channel stay on the
   current peer; microphone packets and "tx" messages are taken from
   either.
4. `DaemonMediaController::kReplaceOverlapMs` (1000 ms) after the new peer
   was ready, the Core stops sending on the old peer, moves its displays to
   the new one (each display's next frame there a keyframe), and sends
   `replace` with the new `connectionId` and `replaces` the old. It keeps
   taking microphone packets and "tx" messages on the old peer for
   `kReplaceDrainMs` (2000 ms), then closes it; nothing more is sent for
   the old `connectionId` (no `rejected`).
5. From that `replace` on, every operation carries the new `connectionId`,
   and the GUI sends its microphone and "tx" messages on the new peer.
   Audio contexts already running keep their SSRCs: the GUI takes the new
   peer's packets for each stream as that stream's (receiver stream `n`'s
   for receiver `n`, and so on). A context the Core sends later names the
   new peer's SSRC, as for any peer.

**Dual receive.** From step 3 the GUI receives each stream on both peers.
It keeps one queue per stream, drops a packet whose RTP timestamp it has
already taken for that stream (`RtpDuplicateFilter`, the last 64
timestamps per stream), and plays the rest in timestamp order, so a packet
lost on one path and carried on the other is heard once and none twice.
Display frames are taken from both peers until the `replace` from the Core;
a display's frames on the new peer start with a keyframe.

**When it fails.** A new peer that fails or closes before step 4, or is not
ready within `IceConfiguration::kConnectDeadlineMs`, is dropped with the
whole-peer `rejected` for its `connectionId` ("The Core lost the audio and
display connection." or "The audio and display connection to the Core
closed."), and the current peer carries on. A current peer that fails
during a replacement ends the replacement too: the Core drops both, as it
drops a peer today, and the GUI starts media again.

**Transmit.** A replacement never keys and does not start while the radio
is transmitting or MOX's delay timers run. A key pressed during one goes
through: the Core keeps taking "tx" keepalives and microphone packets on
both peers until the old one closes, so the watchdog's 400 ms deadline
sees no gap the move made.

**A direct-only replace** (the direct media ladder; capability
`mediaDirectVersion` 1, which a Core sends only to a device with media that
declared `mediaDirect` 1, the link document sections 6.1 and 6.3). The GUI
may add `"mediaDirectVersion": 1` to its `replace`. The Core then makes the
new peer with its STUN server and host candidates only: no media tunnel
candidate and no relay (`MediaTunnel::directIceFor`). Everything else is
the replace above: the same refusals, the same overlap, and the Core's own
`replace` keeps its three fields. A `replace` that names the field when the
Core did not advertise it, or with any value but 1, is malformed, as any
other extra key. A direct-only replace that does not become ready is
dropped as in "When it fails", and the tunnel keeps carrying media. The
older relay leg's refusal ("This older relay media path cannot move while
it is still in use.") is judged by how the connection in use was made, so a
connection that started on the tunnel may always move.

Every media connection, the first one included, gathers with the Core's
STUN server (the device takes it from `mediaStunUrls`, or the last one its
rendezvous gave it), and the tunnel candidate stays the lowest priority, so
a direct path is chosen whenever one works on the first connection. ICE
never changes its choice afterward, so only a replace moves media off the
tunnel.

**Trying a direct path while on the tunnel.** While its media runs over the
tunnel on a Core with `mediaDirectVersion` 1, the desktop window sends a
direct-only replace at the steps of the control upgrade schedule
(`PathRacer::kUpgradeRetryMs`: 5, 30, 120 and 300 seconds, the last
repeating), counted from when media settled on the tunnel. It skips a step
while it is keyed, has VOX armed or the Core is on the air; a step that is
skipped, refused or not ready by its deadline leaves media on the tunnel
and waits for the next. A new media start begins the schedule again.

**Falling back when a direct path goes quiet.** On a direct path (not the
tunnel, not a relay) with receive audio wanted, when no audio or display
packet arrives for `RemoteMediaController::kDirectMediaSilenceFallbackMs`
(5000 ms) while the control session still runs, the window sends the
normal three-field `replace` and starts the direct schedule again at its
first step. That replace's new connection offers the tunnel alone
(`MediaTunnel::tunnelIceFor`): the tunnel's candidate only, no STUN server
and no host candidates, and it takes none the Core signals, so ICE can only
nominate the tunnel. Nothing changes on the wire or at the Core. The
fallback runs once per silence: only a media packet arms it again. If no
media has arrived one window (5000 ms) after the fallback finished,
whether it moved media or failed, the window asks for recovery (a new
media start). Receive audio is wanted when this window's radio is
connected and it is not muted; the silence while the Core transmits is
expected, so the return to receive starts the window again. The tunnel
and the relays keep the stall rule (`kMediaStallMs`, a new media start).
While the Core is on the air the window does nothing here; the Core's own
rule for a keyed device whose microphone packets stop ends transmit as a
lost link. The Core sees
only the packets it receives: it cannot tell that its own packets stop
arriving at the device, so the fallback is the device's to start.

## Display subscriptions

GUI-to-Core `subscribe` has these exact additional fields:

```text
endpointId, revision, sliceId, tier, fftSize, windowType,
centreHz, spanHz, pixels, fps, framesPerLine,
trace, waterfall, minDbm, maxDbm, wideSpanFactor
```

`endpointId` and `revision` are nonzero uint32 values. Revisions use unsigned
half-range ordering across wrap. `sliceId` must identify a live station
slice; Core resolves its stream, so the client cannot request an arbitrary
DDC. `tier` is `wide` or `fine`. Up to eight endpoints are admitted, each
requesting 1..4096 pixels and 1..60 frames/second. An FFT size is a power of
two from 1024; a size FFTEngine supports is honoured as asked, and a larger
one is granted FFTEngine's largest size. A request sizes its (stream, tier)
engine only while it is that engine's only subscriber; beside another
endpoint it is granted the engine's current size, so no pan's spectrum
changes to satisfy another's request. When the last neighbour leaves, an
endpoint held to a neighbour's size is granted its own. The engine runs at
the highest frame rate its endpoints ask for, and each endpoint keeps its own
cadence, so a rate change alone renews no endpoint. Pixels are granted as
min(requested, visible source bins, 4096). Incompatible window choices are
refused. A global
window change unsubscribes all old-window endpoints before requesting any
replacement, so shared sources can adopt the new window.

Capability `spectrumGrantVersion=2` (parity Task 17, R-R3-01) adds one
optional field, `decimation`: a whole number 1 to 16, the desktop's Setup >
Display > Rendering > Decimation. The endpoint's engine passes only every
Nth I/Q pair to its FFT (`FFTEngine::setDecimation`), as a local window's
engine does. It follows the FFT size's sharing rule: a request sets its
engine's decimation only while it is the engine's only subscriber (any
device's); beside another endpoint it runs at the engine's decimation, and
when it is left alone it gets its own. An endpoint running at another
decimation than it asked for is told so: its context's `limit` is `shared`
(below), as for a shared engine's size, until it is left alone and its
renewed context says `none`. A change renews the context. Absent,
the engine runs undecimated (1). Only a peer at the grant minor may send it
(`StationServer::spectrumGrantAvailable`); a value outside 1 to 32, or one
that is not a whole number, is refused as a request the Core cannot read.
The desktop sends it to a Core that advertised version 2 on every
subscribe of every pan, from the value its Setup page keeps; a window told
less does not send it. In a local window the same setting applies to every
pan's engine (the FFT engine pool's `setDecimation`, which also reaches an
engine made later), not only the first stream's.

The quantisation window (`minDbm`, `maxDbm`) is what the pan shows (parity
Task 17, R-R3-01, R-R3-04). The codec carries 256 levels across it, so the
desktop asks for the pan's own dBm range (its reference level down by its
dynamic range, less the normalise shift the pan adds when it draws, in the
frame's own un-normalised values) widened by the waterfall levels in force,
which colour the frame's values as they come; each edge is rounded outward
to a tenth of a dB and held inside -400 to 100. The levels in force are the
stored low and high levels, unless waterfall AGC, noise-floor AGC or
Clarity sets them at run time (parity Task 17 follow-up); then the window
holds the run-time levels with 10 dB of headroom each side, rounded outward
to a whole dB, so no colour clips at the window's edge and a remote pan
colours as a local one does. Those levels move a little every line, so the
window keeps them while they fit and it is no more than 20 dB wider than
they need on either side, and asks again only when they leave it or it is
wider than that. The AGCs work on the values the pan receives, which the
Core clamps to the window, so with nothing below the window (a receiver
passing no noise) their low level sits a margin under whatever edge the
window has; an AGC's low level therefore takes the window no lower than
60 dB under the pan's floor or the stored low level, whichever is lower,
so the window does not walk down to -400 dBm a request at a time.
Clarity's levels come from the Core's noise floor of the whole source and
are not held to that. A signal above 0
dBm on a pan whose reference level is above 0 is drawn at its level, and a
20 dB pan with its waterfall levels inside it steps by 20/255 dB. A change
of range asks the Core again once the new range has held for one frame
period of the pan's rate; the desktop's planner looks every 100 ms, so it
asks at most once a planner tick, and a drag of the dBm strip asks once it
stops, not at every step. The Core accepts any finite window inside those
limits, as before.

The averaging constants in `trace` and `waterfall` are for the rate the
request asks (`fps`): the desktop computes each from its spectrum and
waterfall averaging times, exp(-1 / (fps x time)), as the Core computes an
app's from `averageTimeMs` (display extras v1), and under the display
budget at the rate the budget gives the pan.

Both `trace` and `waterfall` contain exactly `detector`, `averageMode` and
`averageAlpha`. Detector codes are the existing SpectrumDetectorMode values:
0 peak, 1 Rosenfell, 2 average, 3 sample, 4 RMS. Averaging codes are the
existing SpectrumAvenger values: -1 peak hold, 0 none, 1 recursive linear,
2 time window linear, 3 recursive logarithmic. Alpha is finite in [0,1].
Each plane owns an independent reducer history.

Frequencies and spans are in Hz. Spans must be positive; source and request
edges and the requested wide product must remain finite. The quantization
window has finite `minDbm < maxDbm`. `wideSpanFactor=0` disables wide coverage;
otherwise it exceeds one and represents the GUI's maximum 3D shape. Core
clamps coverage to the actual source and the wide row to 768 samples. A
wholly nonoverlapping request is rejected. A retune that removes its coverage
also rejects and retires the endpoint; Core never acknowledges a zero-span
display context.

Core-to-GUI `context` has exactly 19 fields in total:

```text
op, connectionId, endpointId, revision, contextGeneration, sourceStream,
sourceCentreHz, sampleRateHz, centreHz, spanHz, wideCentreHz, wideSpanHz,
traceSamples, waterfallSamples, wideSamples, minDbm, maxDbm, fps, framesPerLine
```

A subscription that negotiated the extended view (minor 6) adds a 20th
field, `wideband`.

Session protocol minor 9 and capability `spectrumGrantVersion=1` add five
fields reporting what Core granted the endpoint (R-R3-01, R-R3-08), so the
context has 24 fields, or 25 with `wideband`:

```text
grantedFftSize, grantedTier, requestedPixels, grantedPixels, limit
```

`grantedFftSize` is the FFT size the endpoint's engine actually runs
(1..262144). The desktop draws with it: a remote pan's bin width (the
receiver's sample rate over the granted size), its Hz/bin readout and its
normalise shift follow the grant, not the window's own idle engine (parity
Task 17, R-R3-01); a context without a grant (an older Core) uses the size
the window asked for. The desktop also runs the pan's active peak hold,
peak blob and noise floor decay per frame at the context's `fps`, the rate
the Core sends after the display budget, so their times per second stay
right when the budget lowers it. `grantedTier` is `wide` or `fine`. `requestedPixels` and
`grantedPixels` are 1..4096 with granted not above requested. `limit` names
what reduced the grant: `none`, `largest-size` (the request was above the
largest supported FFT size), `shared` (another endpoint uses the same stream
and tier engine, so its size stands, or its decimation when that is not the
one this endpoint asked for) or `source-bins` (the crop has fewer
source bins than the requested pixels). A minor 8 or older peer receives
the 19- or 20-field context unchanged, and each side accepts only the shape
it negotiated. Both sides use one codec, `RemoteSpectrumContext`. The GUI
shows a plain status line on the pan while the grant is limited.

Core configures this from the first actual frame of the current source
generation, then sends it before encoded display. Exact sample counts clamp
to available cropped bins. Center/span describe the accepted bin-aligned
coverage; they are acknowledgments, not a new GUI gesture. A retune or
reconfiguration creates a new context generation and clears input overlap,
reduction history and pending output. A frame arriving before its context
is discarded; the GUI requests a keyframe after accepting context.

| Operation | Exact additional fields |
| --- | --- |
| `unsubscribe` | `endpointId` (and `revision` on the display budget wire, below) |
| `keyframe` | `endpointId`, `contextGeneration` |
| `rejected` (Core to GUI) | `endpointId`, `revision`, `reason` |
| `noise-floor` (Core to GUI) | `endpointId`, `revision`, `contextGeneration`, `floorDbm` |

R-R3-12 adds `noise-floor` as optional display metadata; older clients ignore
the unknown operation. It has exactly six payload fields including `op` and
`connectionId`. Core sends it after the accepted context, once initially and
then at most twice per second per endpoint, using a fresh source frame.
`floorDbm` is finite and bounded to [-400,100]. It is the existing
NoiseFloorEstimator's 30th percentile of the full-source FFT dBm bins, with
station calibration applied once, before viewport crop, detector, averaging
or codec quantization. No full FFT array travels over WSS.

The GUI requires the current authenticated session, connection ID, endpoint
revision and context generation. Only the visible active pan with unchanged
subscription inputs may feed its existing Clarity controller. Local cadence,
EWMA, deadband, manual override, re-tune, TX pause and palette behavior remain
in effect. The binary display codec and R3 media version are unchanged. This
restores the existing global active-pan Clarity behavior; independent
per-pan Clarity controllers and Auto AGC-T telemetry remain separate work.

Keyframe requests are limited to five per endpoint per second and must
match its current context. A whole-peer rejection uses endpoint/revision
zero. Unsubscribe releases an unused source. Rebinding, slice removal and
disconnect invalidate the corresponding endpoint. Hidden GUI panes
unsubscribe, and a newly shown pane receives a new endpoint ID.

### Display budget (unsubscribe revision and allocation-result)

When the Core enforces a session display budget it advertises capability
`remoteDisplayBudgetVersion=1`, and a peer at session protocol minor 7 or
later (`kRemoteDisplayBudgetSessionProtocolMinor`) gets the budget wire
below. The Core applies it while media is available, budget enforcement is
on and a budget is in force for the session (`StationServer::
displayBudgetAvailable`). A budget the Core computed for itself is in
force only for a peer at minor 11 (`kDisplayBudgetReasonSessionProtocolMinor`);
an older peer keeps the legacy wire exactly: no budget, no pacing, no
allocation results. `DaemonMediaController::handleUnsubscribe` and
`sendAllocationResult` are the code.

On the budget wire, GUI-to-Core `unsubscribe` has exactly `op`,
`connectionId`, `endpointId` and `revision`: the endpoint's release is a
revisioned operation like a subscribe, and `revision` is a nonzero uint32.
Without the budget wire it has exactly `op`, `connectionId` and
`endpointId`, as in the table above.

Core-to-GUI `allocation-result` answers every subscribe and unsubscribe
outcome on the budget wire, and replaces `rejected` for an endpoint with a
nonzero endpoint and revision there (an involuntary retirement, such as a
source retune or slice removal, is reported the same way, so the GUI learns
the charge the Core released). It has exactly 11 fields:

```text
op, connectionId, endpointId, revision, accepted, reason, budgetGeneration,
acceptedRevision, applicationBytesPerSecond, spectrumSampleUnitsPerSecond,
messagesPerSecond
```

| Field | Meaning |
| --- | --- |
| `op` | `allocation-result` |
| `connectionId` | the media peer's connection ID |
| `endpointId` | the endpoint the operation named; the desktop client accepts 1 to 4294967295 |
| `revision` | the revision of the subscribe or unsubscribe this answers; 1 to 4294967295 |
| `accepted` | boolean: whether that operation was granted |
| `reason` | empty when accepted; otherwise why not, in plain words (the link document's section 17); the desktop client reads at most 512 characters |
| `budgetGeneration` | the generation of the display budget the Core judged it against (`DisplayBudgetLimits::generation`); 1 to 4294967295 |
| `acceptedRevision` | the revision the Core now holds for the endpoint, or 0 when it holds none; 0 to 4294967295 |
| `applicationBytesPerSecond`, `spectrumSampleUnitsPerSecond` | the charge the Core retains for the endpoint after this outcome; whole numbers up to 9007199254740991 |
| `messagesPerSecond` | the retained message rate; 0 to 200 (`kDisplaySenderMessagesPerSecond`, one per 5 ms sender interval) |

The desktop client refuses a result unless the three charge fields are
all 0 when `acceptedRevision` is 0 and all above 0 when it is not, and
unless it has exactly these 11 fields. A result whose `revision` is not the GUI's
pending one only restates what the Core retains: the desktop client
applies it only when it releases a reservation (acceptedRevision 0, zero
charge) for the endpoint's current revision.

A subscription refused because it does not fit the device's display budget
share has the reason "The Core's display limit has no room left."
(`kDisplayBudgetRefusalReason`, compared exactly by clients). With several
devices on one Core (the several-devices design, ruling 9.3) its request
still counts in that device's share of the budget after the refusal, and
ends when the first of these happens: the GUI subscribes that endpoint
again (the new request replaces it), the GUI unsubscribes it, or
`DaemonMediaController::kRefusedDisplayDemandHoldMs` (10 s, the app's own
allocation acknowledgement timeout, `kDisplayAllocationAckTimeoutMs`) passes
after the refusal was sent without either. The refusal follows the
`capabilities` that carry the share the request produced, so a GUI that
still wants the display plans inside that share and subscribes again well
inside the hold (the desktop re-plans on every capabilities change and every
100 ms). A GUI that drops a display the Core refused (a pane closed, hidden,
or paused because the share has no room for it) unsubscribes it, so it
stops counting against the other devices at once.

The [display codec specification](2026-09-20-display-codec-v1.md) defines
the binary packets, reconstruction and loss recovery. Pending source/output
slots and transport queues are bounded. A failed nonblocking send is not
retried with the same bytes: it may already have entered the library buffer;
the following attempt uses a keyframe. Dropped I/Q invalidates the FFT input
history before post-gap samples are processed. Endpoint cadence follows an
advancing schedule with bounded early-jitter tolerance; it does not restart
its entire interval after each arrival or catch up with a burst after a stall.

### Clarity re-tune (clarity-retune)

Capability `displayExtrasVersion=2` (R-IOS-27, R-IOS-06; display extras
v1 is its version 1) at agreed minor 11 adds GUI-to-Core `clarity-retune`,
with exactly `op`, `connectionId` and `endpointId` (a nonzero uint32).
It is Clarity's Re-tune for that endpoint: the Core calls
`ClarityController::retuneNow` on the endpoint's own Clarity controller,
the one a subscription whose `waterfallLevels` mode is `"clarity"` owns
([display extras v1](2026-09-23-display-extras-v1.md)), which is what the
desktop's Re-tune button does for its pan. The next noise floor re-anchors
the smoothing and the waterfall levels at once, inside the poll window,
and the new levels reach the app in the endpoint's next NSDX datagram.
Other endpoints are untouched. `DaemonMediaController::handleClarityRetune`
is the code.

A re-tune that runs is not answered. The Core refuses one it cannot run
with Core-to-GUI `rejected` (its five fields, above) naming the endpoint,
with `revision` 0 and one of these reasons:

| Reason | When |
| --- | --- |
| "That display is not one this app opened." | `connectionId` is not the active media peer's |
| "That display is no longer open on the Core." | no live endpoint has that `endpointId` |
| "Clarity is not setting this display's waterfall levels." | the endpoint's `waterfallLevels` mode is not `"clarity"`, or it asked for no display extras |

No other `rejected` names an endpoint with `revision` 0 (a subscription's
revision is never 0, and the whole-peer refusal has `endpointId` 0 too), so
this refusal retires nothing: the endpoint stays open and its frames keep
coming. The budget wire does not change this: the refusal is still
`rejected`, never `allocation-result`, because it answers no subscribe or
unsubscribe. A request of any other shape (another key, an `endpointId` of
0 or not a whole number, a `connectionId` that is not a canonical UUID) is
ignored and not answered.

A peer the Core did not tell `displayExtrasVersion` 2 (an older Core, or a
session below minor 11, which is never told the capability) gets exactly
today's behaviour: the operation goes where an unknown operation always
has, and nothing is answered. `tst_display_extras` holds all of this.

## Receiver mini display endpoints

At agreed session minor 11, a GUI hello that declares `miniDisplay: 1`
receives capability `miniDisplayVersion: 1` when media is available. The
capability is omitted for a peer that did not declare the feature. A media
`start` may then declare `miniDisplayVersion: 1`; another value, or a
declaration without that capability, is refused. A `subscribe` on that media
connection may add exactly `displayRole: "mini"`. An absent role remains a
pan and keeps the existing request and response shapes. Any other role,
non-string role, or mini role without the media-start declaration is refused.

A mini is an ordinary revisioned display endpoint with its own endpoint ID,
context generation, grant, budget charge and source lifecycle. Its `sliceId`
identifies the receiver; two endpoints for the same slice may request
different RF crops. The Core validates source coverage and grants the actual
pixel count through the existing spectrum path. During TX, a pan keeps its
existing shared-pan takeover rule. A mini follows TX only when its `sliceId`
is the slice recorded at TX rise. Other minis keep receive frames while their
receiver source remains available; without coverage they have no valid
context to display. A changed role requires a newer subscription revision,
which replaces the endpoint context and generation. Existing TX duplex,
retirement, and endpoint limits apply.

## Transmit display


Capability `txDisplayVersion=1` (remote-window parity Task 28, A11,
R-R3-49) at agreed minor 11: while the Core's radio is keyed, the pan
hosting the transmitting slice shows the transmit analyzer's display
instead of the receiver's, as Thetis shows its transmit display on the
transmitting receiver's display while keyed with display duplex off
(`console.cs:24281-24338 [v2.10.3.15]`, DisplayThread). The Core sends 1
while media is on and it has a TX analyzer (a Core that runs its own DSP),
0 otherwise; a peer below minor 11 is never told it. Only a window that
adds `txDisplayVersion` (a whole number of at least 1) to its `start`
gets any of what follows; anything else in that field refuses the start
and no peer starts. A window that does not add it gets exactly today's
wire: no `transmit` field, and receive frames while keyed.

**Subscribe.** A declaring window's `subscribe` may carry two more fields,
both or neither: `txMinDbm` and `txMaxDbm`, the window the Core quantises
the transmit display to (finite, `txMinDbm < txMaxDbm`, each inside -400
to 100 dBm). Absent, the transmit display uses the endpoint's `minDbm` and
`maxDbm`. Values outside those rules are refused as "The Core could not
read this display request.", as the receive window's are; one edge alone,
or either field from a window that did not declare it, is a subscribe of
another shape. The desktop sends the pan's transmit grid (its transmit
reference level down by its transmit dynamic range) widened by the
transmit waterfall levels (Thetis `TXWFAmpMin` and `TXWFAmpMax`,
`display.cs:6420-6427 [v2.10.3.15]`), each edge rounded outward to a tenth
of a dB.

**Context.** Every `context` a declaring window is sent carries one more
field, `transmit` (boolean): 25 fields, or 26 with `wideband`. `false` is
the receiver's display, exactly as before. Both sides use
`RemoteSpectrumContext` (`decodeRemoteSpectrumContext`'s
`transmitNegotiated`).

**The rise.** On the Core's MOX rise (its `MoxController`, whatever keyed
it: a MOX click, the radio's PTT, TUNE, VOX, another device), each endpoint
of a declaring window whose slice is the transmit slice or shares its pan
on the Core (`panKey`) becomes a viewer of the transmit display
(`RadioModel::txDisplayFeed`, `TxDisplayFeed`), centred on the carrier at
the endpoint's own span, as a local window re-centres its transmitting pan
at the rise. It is sent a new context with `transmit` true:

| Field | Value |
| --- | --- |
| `sourceStream` | the endpoint's receive stream, unchanged |
| `sourceCentreHz` | the carrier: the transmit slice's frequency plus XIT when XIT is on (`RadioModel::txFrequencyForSlice`) |
| `sampleRateHz` | 96000, the TX DSP rate |
| `centreHz`, `spanHz` | the transmit display's view: the carrier plus the middle of the analyzer's window, and the window's width |
| `traceSamples`, `waterfallSamples` | the view's pixels, never more than the endpoint's granted pixels |
| `wideCentreHz`, `wideSpanHz`, `wideSamples` | 0 |
| `minDbm`, `maxDbm` | the transmit window |
| `fps` | the analyzer's output rate (15), never faster than the subscribe asked |
| `framesPerLine` | 1: each analyzer frame brings a waterfall row |
| `grantedFftSize` | the transmit analyzer's FFT size |
| `grantedTier`, `requestedPixels` | as the endpoint's receive grant |
| `grantedPixels` | `traceSamples` |
| `limit` | `none` when this endpoint governs the view, `shared` when another does |
| `transmit` | true |

Then the endpoint gets the analyzer's trace (its pixout 0) and waterfall
row (pixout 1), encoded by the display codec as receive frames are (a
transmit frame is an ordinary NSDC frame, first a keyframe; the
`nsdc1-transmit` vector), each plane the analyzer's pixels brought to
`traceSamples` by keeping the highest of the pixels each sample covers.
No receive frame and no `noise-floor` reaches that endpoint until the
fall. `keyframe` for the transmit context's generation is honoured as for
a receive one. Endpoints on other pans keep their receive frames and
contexts through the whole key.

**The view.** The analyzer runs one view for every viewer. The governing
viewer sets it: a local viewer when there is one (a desktop window running
its own DSP, or hosting the Core), otherwise the lowest viewer id; the
view is held inside the analyzer's +/-48 kHz baseband around the carrier,
its edges relative to the carrier in 100 Hz steps, and a span under
1000 Hz keeps the last good view (`TxAnalyzer::clampViewToBaseband`). A
later `subscribe` from a viewing endpoint while keyed moves its view (a
pan or zoom on the transmitting pan moves the analyzer when it governs)
and is answered with a new context for its revision. The view follows the
carrier: a change of XIT, its offset or the transmit slice's frequency
while keyed renews every viewer's context at the new carrier, as Thetis's
display follows XIT while transmitting (`console.cs:22138-22150
[v2.10.3.15]`, "xit, only when txing"). When the governing viewer leaves,
the next governs and each viewer's context is renewed. The analyzer runs
on every key whether or not anything watches.

**The fall.** On the MOX fall each viewing endpoint stops viewing; the next
receive source frame configures it again for the request it holds (the
receive view it had before the rise, unless it subscribed again while
keyed) and it is sent a context with `transmit` false, then receive
frames. The window asks a keyframe as after any context.

**Budget.** A transmit frame is charged to the display budget exactly as a
receive frame of the same size: the same pacer, the endpoint's own admitted
frame bytes and sample units, and the endpoint's own cadence, so a lowered
share (a lower `fps` asked) lowers the transmit frame rate as it does the
receive one.

**txState.** `highSwr` and `swrWindBackLatched` came with this version on
the `txState` object (station link section 18.8), for the transmitting
pan's high-SWR border. They do not depend on it: every Core that sends
`txState` sends them, whatever its `txDisplayVersion`.

**Display duplex (version 3).** Capability `txDisplayVersion=3`
(remote-window parity Task 31, A11, R-R3-49), sent under the same condition
as 1 and 2, adds display duplex (DUP), Thetis's DUP button
(`console.cs:15390-15395 [v2.10.3.15]`, `_display_duplex`, false by default;
`console.cs:37555-37575 [v2.10.3.15]`, "chkRX2SR is the DUPlex button"). A
window whose Core sends 3 adds `txDisplayVersion` 3 to its `start`; a window
whose Core sends 1 or 2 still declares 1. Only a peer that declared 3 may add
one more field to a `subscribe`, `duplex` (boolean, absent is false); from any
other peer, or as anything but a boolean, it is a subscribe of another shape.
While the Core is keyed, an endpoint whose latest accepted subscribe carries
`duplex` true is not a viewer of the transmit display even on the transmitting
pan: it keeps its receive frames and every context it is sent says `transmit`
false, as Thetis's DisplayThread keeps the receiver on the display while keyed
with DUP on (`console.cs:24281-24338 [v2.10.3.15]`,
`if (bLocalMox && !_display_duplex)`). A `subscribe` while keyed that adds or
drops `duplex` swaps the endpoint at once: dropping it makes the endpoint a
viewer (a context with `transmit` true, then transmit frames), adding it
gives the endpoint back to the receiver (the next source frame sends a
context with `transmit` false, then receive frames). The desktop sends
`duplex` true on every subscription while its View > Display duplex (DUP)
item or container DUP button is on, and omits it otherwise, so a window with
DUP off sends exactly version 2's wire.

The Core also takes each device's DUP from its subscriptions (any accepted
subscription with `duplex` true) for noise blanking: at a key while that
device holds transmit, NB and NB2 go off on the transmit slice, and at the
unkey they come back if that device's DUP is still on, as Thetis's
`UIMOXChangedTrue` and `UIMOXChangedFalse` do (`console.cs:29176-29213
[v2.10.3.15]`). With no device holding transmit the Core's own window's DUP
counts. A `duplex` change touches no radio setting when it arrives, so it is
taken on and off the air.

**Keyed calibration.** The Core calibrates what it sends while keyed as
Thetis calibrates the transmitting receiver's display (RX1Offset,
`display.cs:4820-4850 [v2.10.3.15]`), so a window adds no calibration of its
own to a remote pan: transmit frames get the TX Display Cal Offset
(`tx_display_cal_offset`, Setup > Calibration, the Core's
`hardware/<mac>/cal/txDisplayOffset`, `setup.cs:14364 [v2.10.3.15]`) added to
the analyzer's values before they are quantised; a `duplex` endpoint's
receive frames on the transmitting pan get that offset plus the receive
calibration without its preamp half (Thetis `RXCalibrationOffset(1)`) plus
the transmit attenuator applied (Thetis `Display.TXAttenuatorOffset`, set
beside every `SetTxAttenData`, `console.cs:10613-10622 [v2.10.3.15]`) in
place of the receive calibration with its preamp. Every other frame keeps the
receive calibration (`RadioModel::rxMeterOffsetDb`), as before.

Everything else a window does with DUP is its own drawing: keyed with DUP on
its transmitting pan keeps the receive span and bins under the red border,
the transmit grid and the transmit waterfall levels (`display.cs:1782-1790`
and `6420-6427 [v2.10.3.15]` read `localMox` only); the TX filter overlay sits
at the VFO against the receive span with no XIT (`display.cs:4564-4594`,
`console.cs:22144 [v2.10.3.15]`); a window running its own DSP applies the
same keyed calibration itself; a DUP change while keyed resets the blob
maxima and the active peak hold (`display.cs:514-521 [v2.10.3.15]`). On a Core that sends 0, 1, 2 or no entry the window shows
its DUP controls disabled with "This Core does not show the receiver while
transmitting for this app. Updating the Core may help." and its pan behaves
as DUP off.

`DaemonMediaController` (`reconcileTransmitDisplay`,
`trySendTransmitFrame`) is the Core's code; `tst_remote_tx_display` and
`tst_tx_display_feed` hold it. The desktop declares it at `start`, sends
the transmit window and hands a transmit context and its frames on
(`RemoteMediaController::transmitContextReceived`,
`transmitFrameReceived`); drawing them is the MOX display controller's
(parity Task 29). Display duplex is held by `tst_display_duplex` and the
two `duplex` cases of `tst_remote_tx_display` (parity Task 31).

## Per-device audio quality (opusBitrate, audioQualityVersion)

iPhone app plan Task 23 (R-IOS-09). A device whose hello declared the
feature `audioQuality` 1 is told `audioQualityVersion` 1 (last in the
minor-11 capabilities block, while the Core offers media). Only then may
its `audio` control carry `opusBitrate`, a whole number, beside `profile`:

| Field | Meaning |
| --- | --- |
| `opusBitrate` | The Opus bitrate this device wants for its main (speakers') stream: one of the catalogue's `audio.opusProfiles` bitrates |

The catalogue's `audio.opusProfiles` lists the station's measured table
(`OpusAudioCodec.h` `kOpusMeasuredProfiles`, from
`2026-09-20-remote-daemon-r3-verification/opus-profile-probe.txt`), in
order: `{bitrate: 24000, bandwidthHz: 8000}` (wideband) and
`{bitrate: 48000, bandwidthHz: 20000}` (fullband). A profile taken out of
the table is gone from the catalogue.

A bitrate in the table becomes this device's: the Core rebuilds the
stream's encoder at the next capture block (sequence and timestamp carry
on) and answers with an `audio-context` whose `encoder` reports it
(`targetBitrate`, `audioBandwidthHz`). Any other bitrate is refused: the
answering `audio-context` adds `opusBitrateRefusal`, plain words ("This
Core does not offer that audio quality. The audio stays as it was."), and
the running encoder stays. A later control without `opusBitrate` keeps the
device's choice; a new media peer starts at the Core's `audio_bitrate`.
The choice is per device (each media session has its own); receiver
streams, the headphones mix and the SDP's `maxaveragebitrate` (the Core's
`audio_bitrate`) do not follow it.

`opusBitrate` from a device that was not told `audioQualityVersion`, or
without `profile`, makes the control one the Core cannot read (it is not
taken). A device that never sends it gets the Core's `audio_bitrate`
exactly as before, and its contexts never carry `opusBitrateRefusal`.

## Receiver audio (receiver-audio and receiver-audio-context)

Capability `receiverAudioVersion=1` (R-R3-43) negotiates it; the session
protocol minor is unchanged. The Core advertises version 1 whenever media is
on. A GUI that sees it may add `receiverAudioVersion` (a whole number of at
least 1) to its `start`; only a GUI at the audio status detail minor may, and
a malformed value starts no peer. Only then does the offer declare the four
receiver stream IDs (see Media peer), and only then does the Core honour a
`receiver-audio` request. A GUI that did not declare it at `start` is never
sent a receiver stream or a receiver context, whatever it asks: the transport
library would deliver such packets to it and it would refuse and report each
one. Its offer, contexts and packets are exactly as before.

Each stream is one receiver's own audio: 48 kHz stereo taken where local VAX
takes it, after the transmit gate and before the slice's mute, gain and pan,
the mix and the speakers' volume. The slice's AF gain is applied in the
Core's mixer, not in the receive channel, so neither local VAX nor this
stream carries it, and both stay audible at AF 0. While the transmit gate withholds the slice's audio the stream
sends nothing and its RTP timestamps advance over the gap. A receiver stream
runs beside the main one; starting, stopping or changing it never restarts or
re-announces the main stream, and the main `audio` control never touches a
receiver stream.

GUI-to-Core `receiver-audio` has exactly these fields:

| Field | Meaning |
| --- | --- |
| `op`, `connectionId` | As every operation; the current peer's ID |
| `sliceId` | Non-negative integer, the Core's slice ID |
| `revision` | Nonzero uint32; per slice ID for the whole media connection, it only goes up (serial-number order, as `audio`) |
| `enabled` | Boolean |
| `profile` | `opus` or `lossless`, the session's one audio quality choice |

The Core ignores a request with any other key, a wrong or retired
`connectionId`, a malformed field, or a revision at or below the slice's last
accepted one. Each accepted request, and each change of radio, media
readiness or slice that affects a wanted stream, is answered with one
`receiver-audio-context`: the audio-profile shape of `audio-context` with op
`receiver-audio-context` and `sliceId` added:

| Field | Meaning |
| --- | --- |
| `op` | `receiver-audio-context` |
| `connectionId`, `revision`, `enabled` | As `audio-context`; `revision` is the slice's request it answers |
| `sliceId` | The slice the stream carries |
| `generation` | uint32 counting receiver contexts; its own count, not the main context's generation |
| `ssrc` | The receiver stream ID the packets carry, or 0 when a disabled context holds none (`receiver-limit`, or a slice that was never there) |
| `firstSequence`, `firstTimestamp` | Where the stream ID's RTP timeline continues; each receiver stream ID keeps one timeline for the whole media connection, as the main stream does; 0 with `ssrc` 0 |
| `profile`, `encoder`, `profileRefusal` | As the audio-profile shape of `audio-context` |
| `reason` | While disabled: `client-disabled`, `media-not-ready`, `radio-offline`, `encoder-unavailable`, `slice-removed` or `receiver-limit` |

The profile follows the rules of the main stream: Opus, or lossless when
asked, allowed by the Core's `audio_lossless` setting and carried by this
media connection; otherwise Opus with `profileRefusal`. Opus on a receiver
stream always runs at 48000 bit/s with fullband sound (audio up to 20 kHz),
whatever the Core's `audio_bitrate` (which sets the speakers' mix and the
headphones mix only): when Opus is asked for, when lossless is refused and
when the GUI asks for Opus after its link trial (operator decision of
2026-09-24, from the FT8 measurement in
`2026-09-20-remote-daemon-r3-verification/digital-modes-over-opus.md`). The
`encoder` object reports it, so a GUI reads the rate from the context and
assumes none; the media offer's `maxaveragebitrate` stays the main stream's
target, as before. The GUI keeps every stream on the one choice, and one link
trial covers every lossless stream.

At most four receiver streams run at once, one per receiver stream ID; the
lowest free ID is taken when a stream is wanted and kept until the stream is
turned off, its slice goes or the session ends. A fifth request is refused
with `receiver-limit` and `ssrc` 0 and is not queued: the GUI asks again once
it has let a stream go. A wanted stream keeps its ID and intent while the
radio is offline or the media connection is not ready, and resumes when both
return. When the slice is removed its stream stops with `slice-removed`, its
ID goes free, and its revision stays so a stale request stays refused; a
request for a slice ID the Core does not have is answered `slice-removed`
and remembered nowhere. When the session or the media connection ends every
receiver stream stops with it, with no context (there is no GUI to tell).

The two new reason strings, `slice-removed` and `receiver-limit`, occur only
in `receiver-audio-context`; an `audio-context` carrying one is malformed. A
GUI shows every reason through `OperatorReasonText` in plain words, never as
the wire string.

## Headphones mix (headphones-audio and headphones-audio-context)

Capability `headphonesMixVersion=1` (R-R3-45) negotiates it; the session
protocol minor is unchanged. The Core advertises version 1 whenever media is
on. A GUI that sees it may add `headphonesMixVersion` (a whole number of at
least 1) to its `start`; only a GUI at the audio status detail minor may,
and a malformed value starts no peer. Only then does the offer declare the
headphones stream ID (see Media peer) and does the Core honour a
`headphones-audio` request. A GUI that did not declare it gets exactly the
wire it gets today: no headphones ID, no headphones context, and the main
stream carrying the station's whole program.

Each slice has a speakers-or-headphones output route (`outputRoute`, a
mirrored slice property both sides may write, saved and restored by the
Core). The Core's mixer makes two mixes from the one set of receivers, each
at its slice's gain, pan and mute: the speakers' mix (the receivers routed
to the speakers) and the headphones mix (those routed to the headphones).
For a GUI that declared the headphones mix, the main stream carries the
speakers' mix alone, and the headphones mix travels on its own stream
while it runs; a receiver routed to the headphones is heard only there.
For any other GUI the main stream carries both mixes added together, as
before. Neither mix carries master volume or mute; those are the GUI's
own, on its speakers only.

Shared listening (slice control plan Task 6, `sliceAccessVersion` 1 in
the station link): a device's main stream carries the slices it controls
at their AF gain, pan and mute, and each slice it listens to without
controlling at the device's own listening level (`slice.setListenLevel`),
centered, and not muted by the controller's mute. Two listeners of one
slice hear it at their own levels; the controller's AF gain changes only
the controller's audio. When control of a slice passes to or from the
device, its audio moves between the two without a gap. The Core's own
speakers follow the same rule for the Core's own device. This departs
from Thetis, which sets the AF gain in the receive channel
(`SetRXAPanelGain1`); the Core holds that gain at 1.0 and applies AF in
its mixer.

GUI-to-Core `headphones-audio` has exactly these fields:

| Field | Meaning |
| --- | --- |
| `op`, `connectionId` | As every operation; the current peer's ID |
| `revision` | Nonzero uint32; for the whole media connection it only goes up (serial-number order, as `audio`) |
| `enabled` | Boolean: whether this computer can play the headphones mix now (it has headphones open and they have not failed) |
| `profile` | `opus` or `lossless`, the session's one audio quality choice |

The Core ignores a request with any other key, a wrong or retired
`connectionId`, a malformed field, or a revision at or below its last
accepted one. The headphones mix runs while the GUI asked for it, some slice
is routed to the headphones, the radio is connected and media is ready.
Each accepted request, and each change of those that starts or stops the
mix, is answered with one `headphones-audio-context` (a radio drop only when
it changes what the app was last told: a mix that was sending stops, and
`media-not-ready` becomes `radio-offline`, while `no-headphones-receiver`
and `client-disabled` stand): the audio-profile
shape of `audio-context` with op `headphones-audio-context`:

| Field | Meaning |
| --- | --- |
| `op` | `headphones-audio-context` |
| `connectionId`, `revision`, `enabled` | As `audio-context`; `revision` is the newest `headphones-audio` request |
| `generation` | uint32 counting headphones contexts; its own count |
| `ssrc` | The headphones stream ID, always |
| `firstSequence`, `firstTimestamp` | Where the headphones stream's RTP timeline continues; it keeps one timeline for the whole media connection |
| `profile`, `encoder`, `profileRefusal` | As the audio-profile shape of `audio-context` |
| `reason` | While disabled: `client-disabled`, `no-headphones-receiver`, `radio-offline`, `media-not-ready` or `encoder-unavailable` |

Routing a second or third receiver to the headphones, or one of several
back, changes only what the mix contains, not the stream: no new context
is sent. The profile follows the rules of the main stream, and the GUI's
one link trial counts the headphones mix beside every other lossless
stream, so one fallback moves it to Opus with the rest. Starting, stopping
or changing the headphones mix never restarts the main stream or a
receiver stream. When the session or the media connection ends the
headphones mix stops with it, with no context.

The GUI plays the headphones mix on this computer's headphones output with
its own receiver and rate matcher, paced by that device's clock. A
headphones device failure stops only that receiver: the GUI asks the Core
to stop the mix, says what happened in plain words, and the speakers play
on. It asks again when the headphones device is opened, closed or
changed, or the operator chooses the audio quality again (a decoder that
could not start depends on it); a media reconnect keeps the failure.
With a Core that did not advertise the capability, a slice flag routed to
the headphones says in plain words that this Core cannot send audio for the
headphones, so the receiver plays on the speakers (such a Core sums every
receiver into the main stream).

The new reason string `no-headphones-receiver` occurs only in
`headphones-audio-context`; an `audio-context` or `receiver-audio-context`
carrying it is malformed. A GUI shows reasons through `OperatorReasonText`
in plain words, never as the wire string.

## Transmit monitor (monitor-audio)

Capability `txMonitorAudioVersion=1` (R-IOS-13, R-R3-49; remote-window
parity Task 32) negotiates it; the session protocol minor is unchanged. The
Core advertises version 1 in the minor-11 capabilities block whenever media
is on and it runs its own radio model (0 without media). A GUI that sees it
may add `txMonitorAudioVersion` (a whole number of at least 1) to its
`start`; a malformed value, or the key from a peer the Core did not tell,
starts no peer. Only then does the Core honour a `monitor-audio` request. A
GUI that did not declare it gets exactly the wire it gets today, and no
transmit monitor.

The transmit monitor (MON) is the transmitter's own audio as it goes on the
air, at MON's level (`monitorVolume`, 0.5 by default: Thetis mixes the
transmitter's stream into its output at 0.5 while MON is on, `audio.cs:407-424`
and `console.cs:29040-29066` [v2.10.3.15]). On a Core it is carried in the
media audio of the one device that holds transmit (`txState`'s holder),
while MON is on and the radio is on the air; no other device's stream
carries it. While a remote device holds transmit the Core's own speakers and
headphones leave MON out, so MON plays only on that device; while the
station device holds it (a window hosting the Core, or the radio's own PTT)
the Core's own outputs play it as before (the operator's ruling of
2026-09-26).

GUI-to-Core `monitor-audio` has exactly these fields:

| Field | Meaning |
| --- | --- |
| `op`, `connectionId` | As every operation; the current peer's ID |
| `revision` | Nonzero uint32; for the whole media connection it only goes up (serial-number order, as `audio`) |
| `route` | `speakers`: the main stream; `headphones`: the headphones stream; `none`: nowhere |

The Core ignores a request with any other key, a wrong or retired
`connectionId`, a malformed field, a route it does not know, or a revision
at or below its last accepted one. It answers each accepted request with one
`monitor-audio-context`:

| Field | Meaning |
| --- | --- |
| `op` | `monitor-audio-context` |
| `connectionId`, `revision` | The current peer's ID; the request's revision |
| `route` | The route as applied: `headphones` becomes `speakers` for a GUI whose start did not declare `headphonesMixVersion`, whose main stream then carries MON |

`monitor-audio` is taken on and off the air: it only chooses which of the
device's own streams carries MON, and reaches neither the radio nor any
device. The route stands for the media connection; the holder, MON and the
air decide whether MON is in it. The headphones stream runs while MON is
routed there for the holder with MON on, as it does for a receiver routed
to the headphones (its context's `no-headphones-receiver` then does not
apply). No context is sent when the holder, MON or the air change. When the
session or the media connection ends the route is forgotten, and the next
connection asks again.

The desktop sends its MON output choice beside the MON button (SPEAKERS or
PHONES, `audio/TxMonitor/Output`) as `route`, at each media connection and
on every change. With a Core below version 1 its MON output pair is shown
disabled with "This Core does not send the transmit monitor. Updating the
Core may help."; MON itself still turns the Core's monitor on. The phone's
rule is its own: `headphones` while its output is headphones and `none`
otherwise.

## AM Mod Monitor readings (not media)

The AM Mod Monitor (R-IOS-13, R-R3-49; capability `txModMonitorVersion=1`)
is not carried on the media connection. Its readings travel on the control
session as the `txAmModulation` and `txAmModulationFeedback` record streams
(station link sections 6.3 and 7.7), so a device needs no media start, no
display subscription and no share of the display budget to watch it, and a
device without media sees it too. The Core measures the transmit I/Q it
sends its radio (or the PureSignal feedback), not what any media stream
carries, while the radio is keyed in AM, SAM or DSB. Its envelope trace is
not a display frame: it is at most 512 points per record, sent with the
50 ms record flush, never on the audio clock. The transmit display (above)
and the Mod Monitor are independent: either may run without the other.

## Microphone line (iPhone app plan Task 36)

Capability `remoteTxVersion=1` (R-IOS-13), which the Core sends only to a
client whose hello declared `remoteTx` 1 at minor 11 (the link document,
section 18), negotiates it; the session protocol minor is unchanged. Such a
client may add `remoteTxVersion` (a whole number of at least 1) to its
`start`; from any other client, or malformed, the start is refused and no
peer starts. Only then does the offer carry the microphone line (see Media
peer). There is no control operation for the line: the client sends on it
when it transmits, and the Core decides what the transmitter takes.

**What the client sends.** The microphone, mono 48 kHz, as Opus 20 ms
frames with in-band FEC (payload type 111, one channel) averaging no more
than the line's `maxaveragebitrate` of 48000 bit/s, or, when its line
agreed the L16 format, L16 packets of 192 frames with the microphone in
both channels (payload type 96). The Core accepts either. A desktop remote
window sends Opus at 24000 bit/s, or L16 when its operator chose lossless
audio. The phone's rates (Opus at 48000 bit/s full band, 24000 bit/s under
its Save data choice, and L16 when lossless is agreed) are pending the
phone audio-quality change (claude/iphone-audioquality). A client sends
while its device holds transmit, while its own key is down, or while it
has VOX armed (the Core's VOX on and its session permitted to transmit),
and never otherwise. A program keying
through a desktop window's TCI server is sent on the line in place of the
microphone while its audio comes (the app's left channel, resampled to 48
kHz).

**What the Core does with it.** Packets are put in sequence order; one
behind the stream (reordered too late, or repeated) is dropped and counted.
A gap is rebuilt when the next packet arrives: every lost packet but the
last by Opus loss concealment, the last from the arriving packet's in-band
FEC (Opus codes FEC only for frames its voice detector calls active; for
any other frame the decoder conceals); a lost L16 packet is 4 ms of
silence. A gap longer than 60 ms inserts nothing. The decoded audio goes to
the transmit jitter buffer on the transmit pump's thread, with WDSP rmatch
at a ratio the buffer sets matching the sender's clock to the radio's in
both directions. Its target is one packet plus a margin that starts at
10 ms (30 ms for the phone's 20 ms packets), grows only with the jitter
measured in the packets' arrivals, eases back while the link is steady, and
never exceeds 120 ms (beyond that the oldest audio is dropped and counted);
a standing excess is shed only in silence. After every change of use the
buffer starts empty and the pump hears silence until it holds its target,
then the audio.

**When the transmitter takes it.** The transmitter's microphone is the line
while the device whose media carries it is keyed (its own key, its
program's key, or a VOX key that is its), while its key waits for the
buffer, and while it has VOX armed; otherwise the operator's configured
source applies. Every change empties the buffer, so at unkey nothing of the
device's audio is left. In the RADE modes the line feeds the RADE encoder
as any microphone does.

**Keying on a filled buffer.** A `tx.key` from a device whose media carries
the line, in a mode that transmits the microphone (every mode but CWL and
CWU), keys once the line's buffer holds its target (30 ms on a steady link); if it has not
within 250 ms of the line's first packet after the key, or no packet has
come within 1 s of the key, it is refused `micNotReady` (the link document,
sections 18.3 and 18.6). TUNE and two-tone use no microphone and key at once, and a
device without the line keys with the Core's own source, as before.

**Starvation.** While the device is keyed on its line, 250 ms without audio
is starvation, and audio arriving again ends it. What the Core then does
depends on the transmit mode (iPhone app plan Task 37; the link document,
section 18.7): in LSB, USB, DSB, CWL, CWU, DIGL, DIGU and SPEC the key goes
on, silent; in AM, SAM, FM, DRM, RADE_U and RADE_L the Core stops
transmitting with "No microphone audio arrived from <device>, so the Core
stopped transmitting." TUNE and two-tone are never stopped by it.

**The "tx" data channel (iPhone app plan Task 37).** A media connection
whose `start` carried `remoteTxVersion` also has a second SCTP data
channel, labelled `tx`, which the Core (the offerer) opens beside
`display`, like it unordered and with zero retransmissions, so a lost
message is overtaken by the next instead of holding anything behind it.
The answerer takes it only when its own start asked for the line; any
other connection has only `display`, exactly as before. The device sends
its transmit keepalive on it, one binary message of 13 bytes every 100 ms
while it is keyed or has VOX armed: byte 0 is 1 (a keepalive), bytes 1
to 8 the `sequence` and bytes 9 to 12 the `epoch`, both big-endian, with
the meanings of `tx.keepalive` (the link document, section 18.7). The Core
reads anything else on it as nothing. While the channel is open the device
sends its keepalives there and not on the session; the Core counts either,
and the same sequence twice once.

**The monitor.** With MON on, the audio the Core sends a device while it
is keyed carries the transmit monitor in the Core's mix, exactly as the
Core's own speakers would play it (the monitor level, before the speakers'
volume). The phone plays it on headphones only.

## Measured audio delay (clock-probe and clock-echo)

Capability `audioClockVersion=1` (R-R3-35) negotiates it; the session
protocol minor is unchanged. The Core advertises version 1 whenever media is
on. A GUI sends probes only while audio plays to a Core that advertised it,
one every 1000 ms (`RemoteMediaController::kClockProbeIntervalMs`); a Core
without it is never probed and shows no delay.

GUI-to-Core `clock-probe` has exactly these fields:

| Field | Meaning |
| --- | --- |
| `op`, `connectionId` | As every operation; the current peer's ID |
| `id` | uint32, the probe's number |
| `t0` | Non-negative integer nanoseconds on the GUI's clock when the probe was sent |

Core-to-GUI `clock-echo` has exactly nine fields:

| Field | Meaning |
| --- | --- |
| `op`, `connectionId` | As every operation |
| `id`, `t0` | Copied from the probe |
| `t1` | Core clock (non-negative integer nanoseconds) when the probe arrived, read first |
| `t2` | Core clock when the echo left, read last |
| `generation` | The running audio context's generation, or 0 while no context is capturing |
| `rtpTimestamp` | The RTP time at the end of the newest captured block (its timestamp plus 1920), or 0 |
| `capturedNs` | The Core clock when that block's last frame reached the audio tap, or 0 |

The Core ignores a probe with any other key, a wrong or retired
`connectionId`, an `id` that is not a uint32, or a negative or non-integer
`t0`. The GUI accepts an echo only as the answer to one of its last eight
probes, matching both `id` and `t0`; `t3` is its own clock on arrival.

From the echo the GUI computes the Core-minus-GUI clock offset
`((t1 - t0) + (t2 - t3)) / 2` and the round trip `(t3 - t0) - (t2 - t1)`. Of
the probes in the last 16 s it uses the one with the lowest round trip; half
that round trip, plus a 100 ppm allowance for the two clocks' drift, bounds
the offset's error. Half the round trip is shown only as accuracy, never as
a delay. No figure is shown once the newest echo is 3 s old, while the
echo's `generation` is 0 or is not the context this computer plays, or across
a new audio context until its first echo arrives.

The delay is the time this computer plays a sample minus the time the Core
captured it (mapped through the offset). The play time counts, after the
rate matcher fill and the speaker queue, the fixed delays inside the
pipeline: the Opus codec's algorithmic delay (`OPUS_GET_LOOKAHEAD`, 312
frames at 48 kHz, for Opus only) and the rate matcher's filter delay (69
frames). The speaker queue drains a device callback at a time, so half a
callback is counted and the other half is added to the accuracy, together
with half the time the queue read took. The device's own latency is added
when the audio backend reports it; otherwise the figure says it does not
count the speaker device.

The figure is the delay of the sample heard when the queue was read, not of
the newest sample behind it. While the rate matcher corrects its fill, its
ratio (output frames made per input frame, WDSP rmatch's `var`) is not 1 and
the delay itself changes as the audio plays: the audio between the two
samples was made at that ratio, so it spans `1 / ratio` as much capture time
as play time. The newest sample's delay is reduced by that stretch, the
play time the rate matcher made (everything ahead of the newest sample but
the codec's delay) times `1 - 1 / ratio`, using the ratio at the reading.

## Display on the audio's clock (displayClockVersion)

Capability `displayClockVersion=1` (R-R3-21, R-R3-08) says the Core stamps
every display frame's `producerTimestamp` (the display codec header) and the
`clock-echo`'s `t1`, `t2` and `capturedNs` from one clock: its monotonic
producer clock, steady nanoseconds since that clock's epoch. No message
changes shape. The Core advertises it whenever media is on; it is sent in
the minor-11 capability block after `txDisplayVersion`. Before it, the
echo's times counted from the start of each session's media, so no window
could relate them to a display frame.

A window facing such a Core presents each spectrum trace and waterfall row
at its `producerTimestamp` plus the Core-to-window map, and sends clock
probes while it shows a display, whether or not audio plays. The map is the
measured audio delay minus the clock offset, that is the time a sample
plays here minus the Core time it was captured, so a display frame appears
when the audio captured with it is heard. The offset's own error cancels.
The map follows the audio's delay without taking the playout reading's
noise (up to half a device callback: 11 ms at a 1024-frame quantum) for a
change: a change of the audio's jitter hold of 15 ms or more (deepened after
a stall, or shed as it eases) is taken at once; any other change of at least
the larger of 15 ms and twice the reading's accuracy is taken when the next
reading confirms it; smaller changes are smoothed over 500 ms. With no audio
playing (muted, no stream, a restart before its first echo) the window keeps
the last map, which needs no offset, and replaces it (with the last delay the
audio had, or, before any, half the round trip plus the audio's 80 ms base
hold, against the current offset) only when the two differ by more than the
offset's error bound, which is clock drift. An item the map would hold longer
than 1.5 s is shown at once. Without a map (an older Core, or no echo yet)
each frame is drawn on arrival; the row queue and gap filling below still
apply.

Between the transport and the widget the window keeps the decoded frames of
each pan in order. A lost display message makes the decoder refuse the
chain until a keyframe (requested as before, at most one per 200 ms); while
it waits, each row slot that passes with nothing to show repeats the last
good row. When the next good row arrives, the slots between it and the last
good row that have not repeated are filled with rows blended linearly (in
dBm) from the last good row to it, each at its own slot time, so the time
axis stays true. Gaps in `producerTimestamp` without a loss (rows the Core
never sent) are blended the same way; a gap of more than 64 rows is a pause
and is not filled. The row period is `framesPerLine` frames at the context's
`fps`. The widget queues up to 32 rows and draws one a waterfall tick, two
while more than two wait. Only incoming content waits: tuning, the pan,
zoom and every overlay move at once. A trace or row captured before a tune
is drawn at the frequency it was captured at, and the last trace stays drawn
that way until a frame of the new window presents, so a tune or pan drag
never blanks the trace. A keyframe later than the slots already repeated
takes those rows back from its own row and the next ones (their traces are
drawn, their rows are not), so the waterfall keeps one row per slot.

The transmit display (see Transmit display) is not presented on this clock:
it has no audio playout to follow, so its frames are drawn on arrival and
never enter the pan's queue. When a pan keys, the receive frames still
waiting in its queue are dropped (none is drawn during the over) and nothing
of the over is queued; when it unkeys, the first receive row starts a new
blend chain, so no rows are blended across the over.

The window's diagnostics line carries `displayKeyframeWaits`,
`displayKeyframeRequests`, `displayRowsBlended`, `displayRowsRepeated`,
`displayLargestArrivalGapMs` (the largest wait between two display
messages of one pan in the last 10 s), `displayDelayMs`,
`displayItemsDropped` and `displayRowsDropped` (queue overflows, never
expected). The Core logs each keyframe request it takes (refusals, over its
five a second, at most once every 10 s with their count), and its display
diagnostics line counts them (`keyframeRequests`,
`keyframeRequestsRefused`).

The audio enable/context lifecycle, playback buffering and adaptive session
budget are still being implemented. Their acceptance remains open in the
[R3 plan](2026-09-20-remote-daemon-r3-plan.md); this document does not claim
live hardware or internet traversal acceptance.

## Remote TCI raw I/Q (Task 23, minor 11)

A Core with media enabled advertises `remoteIqVersion=1` in its minor-11
capabilities. A window that saw it adds `remoteIqVersion:1` to media `start`;
other versions and undeclared requests are refused. The peer opens a separate
DTLS-secured, reliable ordered SCTP channel labelled `iq`. It is not the
loss-tolerant display channel, and its byte/receive limits do not share that
channel's application queue. A window with an older Core disables its TCI I/Q
options and tells the operator why when a TCI app requests `iq_start`.

The exact request is `op,connectionId,sliceId,revision,enabled` with
`op="iq-stream"`. Revision is a nonzero increasing uint32 for each slice in
this connection. The Core answers the latest request with exactly
`op,connectionId,sliceId,revision,enabled,generation,sampleRateHz,reason`, with
`op="iq-stream-context"`. Generation is nonzero and advances on a new source,
rate or binding, as well as peer replacement. A disabled context has
`sampleRateHz=0`; its plain `reason` is empty for a requested stop or explains
a refusal. The window accepts a frame only for an enabled context's slice and
generation and the next sequence; gaps stop that stream. Slice N is the Core's
slice N, whose current stream index supplies raw samples. Each context reports
the accepted hardware rate (48,000-384,000 complex pairs/s); rates above
384,000 are refused, with no resampling or falsely labelled output.

Each `iq` message is exactly 24 bytes of little-endian header plus its
samples: ASCII `NSIQ`, schema byte 1, zero flags and reserved bytes, then
uint32 slice ID, generation, sequence and pair count, followed by `count`
interleaved float32 I,Q pairs. Count is 1-1024; nonfinite samples, any extra
or missing byte, other flags/version, and a zero generation are invalid.
Sequence starts at zero and rises by one for each message. The source callback
writes into a fixed 128 KiB, single-producer/single-consumer ring; the Core
main-thread media timer assembles full 1024-pair messages and sends them.
The callback holds shared lifetime state, touches no controller object and
never waits. On retirement it sees a stopped flag; the old ring and its
in-flight callback remain valid until the callback returns. Overflow stops
the context with a reason rather than dropping samples unnoticed.

The per-slice application-byte reservation is `8*r + 24*ceil(r/1024)` bytes/s
at hardware rate `r`; the 24-byte final partial-frame allowance is kept in
the sender's bounded burst. A device's PureSignal reservation remains first;
raw I/Q is taken from its residual display share before its pans. The Core
charges every attempted IQ send to the global application-byte pacer, and the
window replans pans by reducing frame rates before pixels, retaining at least
one frame/s for visible pans. If IQ plus that floor does not fit, the Core
refuses with `The link to the Core is too busy to send raw I/Q for this
receiver.` The Core permits one pending application frame and one library
buffered frame at most; sustained 250 ms backpressure or ingress overflow
stops the generation with a reason. This work does not move the remote TCI
receive-audio resampler; its UI-thread cost remains a separate design item.
