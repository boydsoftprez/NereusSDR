# Remote Daemon Architecture (`nereusd`): Design

**Status:** Approved design, not yet planned or implemented
**Date:** 2026-07-28
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code
**Upstream references captured this session:** Thetis `v2.10.3.15` @ `3759d096`

---

## 1. Goal

Run a headless NereusSDR daemon on a small Linux host (Raspberry Pi class)
that owns the radio and performs all baseband DSP, and let the existing
NereusSDR GUI connect to it from anywhere as a remote console.

The daemon does everything NereusSDR does today except draw. It connects to
the radio over Ethernet using Protocol 1 or Protocol 2, runs the full WDSP
receive and transmit chains, computes the spectrum, and streams audio,
spectrum, meters, and state to a remote client. The client renders, accepts
operator input, and sends microphone audio back.

**Non-goal:** this is not a screen-sharing or VNC approach, and it is not a
raw-IQ streaming approach. Both are rejected in §3.

---

## 2. Findings that shaped this design

These are recorded because they are not obvious and because several of them
were discovered by reading upstream rather than by reasoning.

### 2.1 Our core is already GUI-free

`src/core/` and `src/models/` contain 133 source entries in `CORE_SOURCES`
plus `MODEL_SOURCES` and exactly **one** `#include "gui/..."` across all of
them: `gui/SpectrumWidget.h` at `src/models/RadioModel.cpp:293`, which is the
non-owning view hook `MainWindow` installs. The headless split is therefore
a build-system change plus one interface extraction, not a rewrite.

CMake is already the right shape: `NereusSDRObjs` is an OBJECT library and
`NereusSDR` is a thin executable that adds only `src/main.cpp`.

### 2.2 TCI has no spectrum stream, and this is the reason ON7OFF exists

`TCIServer.cs` in Thetis `v2.10.3.15` is 9,342 lines and contains **zero**
occurrences of spectrum, waterfall, fft, or panadapter. `TCIStreamType`
(`TCIServer.cs:343-350 [v2.10.3.15]`) is still exactly five values: IQ,
RX_AUDIO, TX_AUDIO, TX_CHRONO, LINEOUT.

**How every current TCI client draws a panadapter without one.** TCI does
carry raw IQ. `TCIStreamType.IQ_STREAM` is type 0, with `iq_start`,
`iq_stop`, and `iq_samplerate` commands, and `PublishIQSamples`
(`TCIServer.cs:5832-5840 [v2.10.3.15]`) emits float32 IQ at rates up to
384 kHz. Clients subscribe and **compute their own FFT**. That is the
existing mechanism, and it costs roughly 2 Mbit/s.

This is worth stating explicitly because "TCI has no spectrum stream" and
"TCI clients display panadapters" both being true is otherwise confusing. A
panadapter can be drawn from IQ; it simply costs 2 Mbit/s to do it that way.

**The commercial ON7OFF "TCI Remote Compactor" is the proof of what that
costs.** Their Android client runs in two configurations. Direct on a LAN, it
subscribes to the IQ stream, pulls about 2 Mbit/s, and runs a Blackman-Harris
FFT on the phone. Their own product page states this is unusable on cellular.
Via the Compactor, a Windows middlebox on the shack LAN consumes that same IQ
stream, performs the FFT, reduction, and Opus encoding itself, and ships under
100 kbit/s onward.

**The Compactor is precisely the server-side reduction stage Thetis lacks,
sold as a separate product.** This is independent commercial confirmation of
the bandwidth argument that drove §3: a vendor shipping to real users
concluded that raw IQ does not survive the internet and that reduction must
happen at the source. Our daemon performs that reduction inside the process
that already owns the radio, so there is no second machine, no Windows
requirement, and no double handling of the IQ.

MW0LGE's README line "will not require 3rd party solutions" (§2.4) is most
plausibly a direct reference to this product.

Note: the Compactor product page lists NereusSDR by name as a supported TCI
server, alongside Thetis, Zeus, and AetherSDR. Our TCI implementation is
already a third-party integration target.

### 2.3 Thetis has the spectrum reducer, switched off, with nowhere to send it

Three layers are easy to conflate here, so they are separated explicitly:

| Layer | State in released `v2.10.3.15` |
| --- | --- |
| Spectrum **producer** (`clsSpectrumProcessor.cs`) | In source, compiled, **never instantiated** |
| TCI **wire protocol** | No spectrum stream type (see §2.2) |
| MW0LGE's **unreleased** build | Both present and wired; known only from screenshots (§2.5) |


`clsSpectrumProcessor.cs` (1,353 lines, present at `v2.10.3.15`) performs
exactly the server-side FFT-to-pixels reduction this design needs. Each
endpoint allocates a **dedicated** WDSP analyzer via `cmaster.AllocAnalyzer`,
a worker thread calls `SpecHPSDRDLL.GetPixels()` at a configured frame rate,
and the result is a `float[pixels]` array of dBm with a `dataIndex` frame
counter for change detection. It is per-source (receiver or transmitter) with
independent pixels, frame rate, FFT size, sample rate, zoom, and pan.

Defaults, used here as design anchors
(`clsSpectrumProcessor.cs:59-69 [v2.10.3.15]`):

| Constant | Value |
| --- | --- |
| `DefaultPixels` | 1024 |
| `DefaultFrameRate` | 15 |
| `DefaultFftSize` | 4096 |
| `DefaultMaxFftSize` | 262144 |
| `DefaultSampleRate` | 192000 |
| `DefaultWindowType` | 4 |
| `DefaultDetectorTypePan` | 0 |
| `DefaultAverageMode` | 0 |
| `DefaultAverageTau` | 0.12 |
| `EmptyPixelValue` | -200.0f |

**The class has never executed in any shipping Thetis.** Every call site in
`console.cs` is commented out (`console.cs:1125-1131 [v2.10.3.15]`), and the
field declared at `console.cs:1200` has no live assignment. It is in
`Thetis.csproj` so it compiles, but it is work-in-progress committed in a
disabled state alongside a debug form used to eyeball the pixel output.
Diffing our clone against `origin/master` also confirms `TCIServer.cs` is
untouched upstream.

**Consequence for risk assessment:** we may port the endpoint model from
released GPL source, but it must not be described as proven upstream code.
The parts we actually reuse for the mathematics, `SpectrumDetector` and
`SpectrumAvenger`, are verbatim WDSP `analyzer.c` ports that run in our
shipping application today, so the risk sits in the endpoint plumbing rather
than in the reduction itself.

The commented-out call sites are more informative about intent than the class
defaults, because they are MW0LGE's own test parameters:

```csharp
//_spectrumProcessor.AddReceiver(0, 1024, 15, 8192);   // RX1: 1024 px
//_spectrumProcessor.AddReceiver(1, 128, 15, 8192);    // RX2: 128 px
//_rx1SpectrumTestForm = _spectrumProcessor.ShowReceiverTestForm(0, -130, -40);
```

RX2 at **128 pixels** against RX1's 1024 indicates a thumbnail or strip-sized
secondary panadapter rather than a full one. Per-viewport pixel counts are
therefore expected to vary widely between panadapters and between clients,
which is precisely what the crop-based approach in §9.1 handles well. The
-130 to -40 dBm window is also close to the -135 to -30 seen in the later
test client, so that quantization range has been stable across iterations.

### 2.4 Thetis was un-archived on 2026-07-02 to build remote operation

From the current upstream `ReadMe.md`, quoting Richie MW0LGE:

> Development is continuing for remote op access, with a full permission and
> state system, display codecs, opus support, remote web client (MW0LGE),
> remote windows client (OE3IDE). TCI will support WS and WSS connections.
> This is being implemented natively in Thetis and will not require 3rd party
> solutions.

ETA is stated as "when it is done, perhaps a few months from now."

**None of it is public, and this was verified rather than inferred.** The
whole `Project Files/Source` tree was searched for the vocabulary MW0LGE's
own test client exposes:

| Term | Files containing it |
| --- | --- |
| keyframe / Keyframe / KeyFrame | 0 |
| adaptive block / AdaptiveBlock | 0 |
| quantiz / Quantiz | 0 |
| frames per line / FramesPerLine | 0 |
| blockDelta / BlockDelta | 0 |

Zero, in every case. The display codec does not exist in the repository in
any form. Neither does a TCI display stream: the 12 occurrences of "display"
in `TCIServer.cs` are all display *calibration offset* fields passed through
`sendCalibration`. There is no Opus anywhere (`packages.config` at
`origin/master` has no Opus package and there is no Opus binary; the only
`opus_types.h` is a header vendored inside rnnoise). There is no compression
of any kind; `encodeSamples` is pure int16/int24/int32/float32 format
conversion. There is no TLS, because the WebSocket layer is hand-rolled over
a raw `TcpListener` (`TCIServer.cs:1876 [v2.10.3.15]`). There is no
permission or state system.

Expressed as a pipeline, what exists stops abruptly:

```
WDSP analyzer → GetPixels() → float[1024] dBm → [codec] → [display stream] → [WSS] → client
   PRESENT        PRESENT         PRESENT         ABSENT       ABSENT          ABSENT
```

**Everything up to and including the array of dBm values exists. Everything
that turns that array into a network stream does not.** The demonstration in
§2.5 runs against a tree that has never been pushed.

**Consequence:** the wire format we would normally port from does not exist
yet, and this is not a matter of waiting for a branch to be readable. There
is no branch. `origin/master` is five commits ahead of our clone: four README
edits and an N1MM fix. The public repository has shown no remote-op code
movement since 2026-03-15. This design therefore builds natively and treats
future TCI WS/WSS alignment as a transport implementation against internals
that are already done. See §11.

### 2.5 Measured targets from two upstream implementations

Two screenshots of MW0LGE's in-development clients give real numbers:

*Thetis Remote Web Client v0.1.132:* RX 94.4 kbit/s total, RTT 27 ms,
30.0 fps spectrum, "Audio worklet 181 ms resample 1.000213x".

*Test TCI Audio Spectrum Client:* Display in 104.00 kbit/s, Audio in
39.55 kbit/s, total in 144.18 kbit/s, **out 0.24 kbit/s**. Settings visible:
1024 pixels, 30 fps, FFT 65536, Hann window, Detector Peak, Averaging Log
Recursive, Avg 120 ms, 384 kHz display span.

ON7OFF independently claims "under 100 kbps". Three implementations landing
in the 95 to 145 kbit/s band establishes the target well.

The 181 ms figure and the 1.000213x resample ratio are the audio jitter
buffer depth and continuous clock-drift correction, and both are load-bearing
(§8.3).

**Header overhead is significant.** The Opus target in the test client is
24,000 bit/s but measured audio is 39.55 kbit/s. At 40 ms frames that is 25
frames per second, and the 64-byte TCI binary header costs
25 × 64 × 8 = **12.8 kbit/s**, roughly a third of the audio budget. At 20 ms
frames it would exceed the payload. This drives the coalescing requirement
in §7.3.

### 2.6 Codec parameters observed in MW0LGE's test client

Recorded as a starting point. **These come from a screenshot of a UI, not
from source.** We cannot and do not port from them; they inform parameter
choice only, and the display codec is NereusSDR-original code (§9.2).

*Display codec:* Bits 8, Min dBm -135, Max dBm -30, Delta on, Block delta on,
Adaptive blocks on, Keyframe 120, Accuracy "1 Normal", Frames per line 2.

*Opus:* application `audio` (not `voip`), signal `music` (not `voice`),
bandwidth `mediumband`, bitrate 24000, VBR and constrained VBR both on,
complexity 10, FEC off, DTX off, frame 40 ms (1920 samples at 48 kHz).

The `audio`/`music` choice over the speech-optimized modes is deliberate and
correct for a receiver: the speech paths assume a human voice and would
damage CW tones, digital-mode warble, and band noise.

### 2.7 We already have most of the DSP half

| Piece | Location |
| --- | --- |
| WDSP `detector()` verbatim port, bin to pixel, 5 modes | `src/gui/spectrum/SpectrumDetector.{h,cpp}` |
| WDSP `avenger()` verbatim port, frame averaging | `src/gui/spectrum/SpectrumAvenger.{h,cpp}` |
| FFT engine, worker thread, FFTW3, 7 windows, `kMaxFftSize` 262144 | `src/core/FFTEngine.{h,cpp}` |
| Overlap-based frame advance (`advance = sampleRate / fps`) | `FFTEngine.cpp:584` |
| Viewport to bin-range mapping | `SpectrumWidget::visibleBinRange()` |
| Receiver-to-pan FFT fan-out, one DDC feeding N pans at different zooms | `src/core/FFTRouter.h` (Phase 3F) |
| Single-TX invariant, RF-safe MOX-drop handoff, per-MAC persistence | `src/core/TxSliceArbiter.h` (Phase 3F) |
| Multi-pan layout, 5 templates, float-to-second-monitor | `src/gui/PanadapterStack.h` (Phase 3F) |
| Real Qt WebSocket server, operator-configurable bind address | `src/core/TciServer.cpp:1126` |
| 3-priority send queue, bounded oldest-drop | `src/core/TciSendQueue.h` |
| 64-byte LE binary framing | `src/core/TciBinaryFrame.{h,cpp}` |
| Opus, static, built with `OPUS_DRED` + `OPUS_OSCE` | `third_party/rade/cmake/BuildOpus.cmake` |
| r8brain 24-bit polyphase resampler | `third_party/r8brain/` |

`SpectrumDetector` and `SpectrumAvenger` are verbatim ports of WDSP
`analyzer.c` and have no GUI dependencies despite their location. Opus is
already a proper CMake IMPORTED target from the RADE work.

### 2.8 This design assumes Phase 3F has landed

**Baseline: PR #293** (`feature/phase3f-sub-epic-a-foundation`), multi-pan
plus multi-slice, 8 sub-epics, roughly 110 commits, +50,091 / -1,520 across
258 files. Draft at the time of writing, bench verification pending. This
design is written against that branch, **not** against `main`, because
planning a remote console against a single-pan single-slice world would be
obsolete before R1 shipped.

What 3F changes for this design:

- **`FFTRouter` already implements the §9.1 model.** One receiver's FFT fans
  out to N pans at different zoom levels. The remote spectrum path attaches
  to it rather than inventing routing.
- **`TxSliceArbiter` already owns TX arbitration** across up to `maxSlices`
  slices, with RF-safe MOX-drop handoff. §7.1 routes through it.
- **Multi-pan is a first-class case, not a deferral.** Up to 5 pans for one
  operator on most SKUs, which makes the shared-FFT smearing question in
  §9.1 live rather than hypothetical, and makes the per-session bandwidth
  budget in §9.5 mandatory.
- **AppSettings migrates v5 to v6** with per-band per-slice persistence,
  which the §6.2 `SettingsProxy` and the §6.3 boundary must reflect.
- **Wideband extended pan** adds a second spectrum source with its own
  refresh rate (§9.6).
- **Per-slice DSP routing is deferred to 3F-1**, so audio remains Slice A's
  regardless of how many slices exist. The remote audio path is built for N
  and ships with one (§16).

Also in flight and assumed landed: **PR #291** (NB live-rate fix, TX I/Q
telemetry, mic native-rate path, already merged into #293), which affects the
§8.2 microphone uplink, and **PR #306** (ANAN-G2E disconnect wedge fix).

**Consequence for phasing:** R1 (§15) is planned against the 3F branch and
should not begin until #293 merges, or must be rebased onto it. The build
split touches CMake target composition, which #293's 258 changed files would
conflict with badly.

### 2.9 Unrelated gap noted in passing

Thetis `v2.10.3.15` has 17 `_ex` TCI commands. Our dispatch implements 6.
Missing: `rx_step_att_ex`, `rx_step_att_enabled_ex`, `rx_preamp_att_ex`,
`rx_nb_enable_ex`, `agc_auto_ex`, `vfo_sync_ex`, `vfo_swap_ex`,
`fm_deviation_ex`, `tx_filter_band_ex`, `tx_frequency_ex`, `run_cat_ex`.
This is a pre-existing TCI parity gap, out of scope here, and should get its
own issue.

---

## 3. The split point: server demodulates

**Decision: the daemon performs all DSP and streams decoded audio. The client
is a control surface and a renderer.**

Alternatives considered and rejected:

| Cut | Server owns | Rejected because |
| --- | --- | --- |
| Raw IQ | Transport only | Bandwidth. This is `rtl_tcp` |
| Narrow IQ per slice | Radio, channelization | 400 to 800 kbit/s per slice against roughly 100 for decoded audio, needs a server-side down-converter that does not exist, and TX becomes jitter-fatal because network jitter turns into on-air IQ underruns |
| **Decoded audio** | **Radio, DSP, FFT, audio** | **Chosen** |
| View-model only | Radio, DSP, and the models | Would require rewriting every existing GUI binding |
| Pixels | Everything | This is VNC |

The narrow-IQ option was seriously considered because it would let the client
keep running its own WDSP chain, requiring no protocol coverage for DSP
controls at all. It was rejected once the measured numbers came in: roughly
100 kbit/s for decoded audio plus reduced spectrum is not close to 400 to 800
kbit/s for narrow IQ, and the control-surface tax that motivated it is
avoidable by other means (§6).

### 3.1 Accepted costs

**Codec-over-codec on digital modes.** The daemon demodulates and Opus
encodes. Weak-signal decoding from Opus audio is degraded, and RADE is a
neural vocoder whose output would pass through a second speech codec.
Mitigations: a selectable lossless or high-rate PCM audio mode, and running
WSJT-X class decoders on the daemon host where the audio is clean.

**Microphone audio is compressed before the TX chain.** The client's mic is
Opus-encoded, then the daemon runs EQ, Leveler, CFC, CESSB. Mitigation: a
high-rate or lossless mic uplink when the link allows.

**DSP controls incur one round trip.** Acceptable for toggles, slightly
rubbery on a filter-width drag. AF gain, mute, and balance stay client-side
and apply to decoded audio so the continuously-touched controls remain
instant.

**PureSignal stays daemon-side.** It requires sample-correlated feedback IQ
and is not exposed for remote real-time operation beyond enable/disable and
status.

---

## 4. Process and build architecture

### 4.1 Target split

`NereusSDRObjs` becomes three targets:

| Target | Contents | Qt modules |
| --- | --- | --- |
| `NereusCore` (OBJECT) | `CORE_SOURCES` + `MODEL_SOURCES` | Core, Network, WebSockets, Multimedia, ZLIB |
| `NereusGui` (OBJECT) | `GUI_SOURCES` + resources, links `NereusCore` | adds Widgets, Svg, GuiPrivate |
| `NereusSDR` (exe) | `src/main.cpp` + `NereusGui` | full |
| `nereusd` (exe) | `src/server_main.cpp` + `NereusCore` + remote sources | no Widgets, no Svg, no QRhi |

The daemon build drops Qt Widgets, Qt Svg, the entire QRhi shader pipeline,
and all 214 GUI source entries.

### 4.2 Prerequisites for the split

1. `src/models/RadioModel.cpp:299` includes `gui/SpectrumWidget.h`. Extract a
   minimal abstract sink interface into `src/core/` that `SpectrumWidget`
   implements, and have `RadioModel` hold that instead. **Verified against
   the Phase 3F branch:** despite 3F migrating 125 call sites from
   `m_spectrumWidget` to `m_panStack`, this remains the only
   `#include "gui/"` anywhere in `src/core/` or `src/models/`. The build
   split survives 3F intact.
2. Move `SpectrumDetector` and `SpectrumAvenger` from `src/gui/spectrum/` to
   `src/core/spectrum/`. Their WDSP license headers travel verbatim and
   unchanged; this is a relocation, not a re-port, so no new PROVENANCE row
   is required, but the existing rows must have their paths updated.
3. Verify no other `MODEL_SOURCES` entry transitively requires Qt Widgets.
   This is a build-and-fix step, not a design question.

### 4.3 Optional audio on the daemon

Qt Multimedia stays linked so a daemon on a shack machine can monitor
locally, but a headless host with no sound device must start cleanly. Audio
output is optional at runtime and its absence is not an error.

### 4.4 Packaging

**One installer, two binaries, daemon off by default.** The desktop package
ships `NereusSDR` and `nereusd`. Out of the box the GUI runs in its existing
in-process direct mode and the user never learns the daemon exists. Enabling
a home server is a Setup toggle. The Pi package ships `nereusd` and a systemd
unit only.

This preserves the current single-install "the app is the whole radio"
experience for everyone who does not want to change it.

---

## 5. Phasing and the always-client-server end state

**v1 adds the daemon and remote client mode. The existing in-process local
path is left untouched.**

The end state is that the GUI always talks to a daemon, including locally
over loopback. That is architecturally correct and removes dual-mode entirely.
It is not the first step, because `RadioModel` currently owns `WdspEngine`
in-process, so switching wholesale would require every DSP control to be
mirrored before the application works at all. That is a big-bang migration of
a working product.

Running the GUI against `localhost` works from day one with no extra work, so
the "sit at the Pi and open the GUI" case is covered by v1 regardless.

**Standing rule that keeps the end state reachable, at no cost:**

> No feature may be implemented in a way that only works in-process. Every
> new DSP or radio control goes through the model `Q_PROPERTY` layer that
> `StateMirror` reflects.

Collapsing the local path onto the daemon later then becomes a deletion
rather than a rewrite.

---

## 6. State synchronization

The naive form of this design needs a wire command, a daemon handler, a
client-side branch, and a connect-time sync path for every settable
parameter. For NereusSDR that is realistically 300 to 500 parameters
(`MicProfileManager` alone bundles 91 keys), each adding four things forever.
That is rejected.

Instead, two generic mechanisms cover essentially everything.

### 6.1 `StateMirror`

Reflects `Q_PROPERTY` declarations over `QMetaObject`: enumerate properties,
subscribe to their `NOTIFY` signals, transmit `(object, property, variant)`
on change, apply on the far side. Written once, generic.

Current coverage, 133 properties:

| Model | Q_PROPERTY count |
| --- | --- |
| `SliceModel` | 87 |
| `TransmitModel` | 16 |
| `TunerModel` | 13 |
| `RadioModel` | 9 |
| `PanadapterModel` | 4 |
| `MeterModel` | 4 |

`SliceModel`'s 87 are the per-receiver operating state: mode, filter, AGC,
the NR and NB families, gains, squelch, RIT and XIT. This is what an operator
touches while operating, and every property added in future is covered with
no protocol work.

Echo suppression uses the existing `m_updatingFromModel` / `QSignalBlocker`
pattern already used throughout the codebase for GUI-model sync.

### 6.2 `SettingsProxy`

`AppSettings` is a flat string key/value store with 576 call sites across 81
files. In remote mode the client's reads and writes are proxied to the
daemon's store. The 47 Setup pages that write settings directly keep working
unchanged because they already go through a single-instance API.

### 6.3 The settings boundary

Three-way, not two.

**Station settings live on the daemon.** Radio model and MAC, sample rate,
DDC configuration, PA profiles, calibration, antenna routing and Alex flags,
step attenuator, accessory configuration (PGXL, TGXL, RF2K-S), TX processing
profiles. These describe the station and follow the radio.

**Operator settings live on the client.** Window geometry, container layout,
applet visibility, skins, color schemes, waterfall palette, grid colors,
fonts. These describe this person at this screen. A laptop must not inherit
the Pi's window positions, and connecting from a different machine must not
disturb the station.

**Operating state lives on the daemon and is mirrored live.** VFO frequency,
mode, filter, AGC, NR, NB, gains, PTT.

**This line runs through the existing Display setup page and requires
splitting it.** FFT size, window function, averaging mode, and detector mode
change what the daemon computes and are station-side. Palette, grid colors,
dBm range, and trace styling change what the client renders and are
operator-side. They currently sit adjacent in one page.

### 6.4 Spot client placement

Split by nature. DX cluster, RBN, POTA, and PSK Reporter are station-level:
they should keep collecting with the GUI closed, and PSK Reporter should
report the station's location. WSJT-X UDP is local to wherever WSJT-X runs,
which is the operator's machine. Both placements are configurable; these are
the defaults.

---

## 7. Session and wire

### 7.1 Session model

**Single operator, one session at a time.** Authentication is a generated
pre-shared token, never user-chosen, rate-limited on failure.

Every connection nonetheless carries a session identity and a role from day
one, even though the role is always owner and there is only ever one session.
Client authentication is the thing that leaks into every other subsystem if
retrofitted, and stubbing it in now costs nothing.

**TX ownership is already solved and must not be reinvented.** Phase 3F ships
`TxSliceArbiter` (`src/core/TxSliceArbiter.h`), which owns the single-TX
invariant across up to `maxSlices` slices, performs RF-safe handoff by
dropping MOX and waiting for `moxChanged` confirmation before flipping the
`isTxSlice` flag, persists the bound slice per-MAC, and re-establishes the
invariant after every slice-list mutation via `syncToSliceList()`.

Remote TX requests route through `requestHandoff(int)` like any other caller,
and the client observes `txBoundSliceChanged` and `handoffBlocked`. The
session layer's concern is *which client* may ask; *which slice* transmits is
the arbiter's, and it is authoritative regardless of whether the request
originated locally or remotely. The watchdog in §12.1 drops MOX through
`MoxController` on the same path the arbiter uses.

### 7.2 Framing

**Split by payload: use a standard where one genuinely exists for that
payload, and do not pretend one exists where it does not.**

| Stream | Framing | Why |
| --- | --- | --- |
| Audio (both directions) | **RTP, Opus per RFC 7587** | A real standard for this exact payload. RTCP supplies loss, jitter and RTT for free. Native if we adopt WebRTC media tracks |
| Spectrum | **Compact native** | No standard payload for reduced bins exists anywhere. VITA-49 has none, RTP has none, FlexRadio invented their own |
| Meters, control | Control channel | Small, infrequent, reliable-ordered |
| Context and metadata | Control channel | Where FlexRadio puts it too, rather than in VITA context packets |

**Audio on RTP** is the part worth being deliberate about. Opus-in-RTP is
specified, so the audio stream is decodable by anything speaking RTP rather
than only by our own client. RTP's timestamp is already sample-count at a
defined clock rate, which is exactly the semantic drift correction needs
(§8.3). And RTCP receiver reports carry packet loss, jitter, and round-trip
time as standard, which is the link-quality panel we would otherwise build
ourselves and build worse. This also interacts with §10.4: if the transport
lands on WebRTC media tracks rather than data channels, RTP is not a choice,
it is what you get.

**Spectrum gets native framing** because no envelope helps. Whatever standard
we wrapped it in, the payload would still be ours, so the envelope buys only
stream identity, sequence, and timing, which fit in roughly twelve bytes.

Alternatives considered for the spectrum stream and rejected: VITA-49
(evaluated seriously, see below), SigMF (a recording format, not a wire
protocol, though relevant if IQ recording is added later), FlatBuffers or
CBOR (serialization layers, not timing or media formats, and still needing
framing underneath), and SRT or RIST (broadcast video transports, overkill
and video-shaped).

**Why not VITA-49 for spectrum,** given it is what FlexRadio uses for the
same job. Three of its ideas are adopted:

- **A stream identifier** per source, so several streams share one connection.
- **Context carried separately from data**, so a retune does not glitch
  spectrum frames already in flight. Without this, the alternative is stapling
  centre frequency and calibration onto every single frame. We take the idea
  and carry context on the control channel rather than as VITA context
  packets, which is also what FlexRadio does.
- **A sample-count timestamp** rather than wall-clock, because daemon and
  client clocks are unsynchronized and what drives drift correction is
  sample-count divergence.

The standard itself is not adopted. Its two real payoffs are wire dissection
by standard tools and ecosystem interoperability, and **both are neutralized
here**: the stream runs inside TLS or DTLS to a single client, so a capture
shows ciphertext, and there is no standard VITA-49 payload for reduced
spectrum bins anyway (FlexRadio defines a custom class ID for exactly this).
Adopting the envelope would mean implementing a subset of a specification
written for hardware on a LAN, then inventing our payload inside it. Note
that VITA-49 has no Opus payload either, which is one of the reasons audio
goes on RTP instead.

Header size is not the deciding factor in either direction. At 30 fps a
28-byte VITA-49 header costs roughly 6.7 kbit/s against about 100, so under
seven percent.

**The existing 64-byte `TciBinaryFrame` header is explicitly not reused for
the remote path.** Thirty-two of its sixty-four bytes are reserved zeros, it
carries no timestamp and no context concept, and §7.3 requires compact
sub-frame headers regardless. It remains unchanged for the existing TCI
server, which is a separate surface.

The `TciSendQueue` three-priority drain (Urgent, Binary, Control) **is**
reused; it is orthogonal to header layout.

**The framing must be transport-agnostic.** The transport underneath changes
across the ladder in §10 and must not be visible above the framing layer.

Sub-frames must be self-delimiting (carry their own length), so a coalesced
message is a plain concatenation and §7.3 needs no separate multiplex header.

**Deliberately left open, and reversible:** a header carrying these semantics
maps mechanically onto VITA-49 later should SmartSDR-family interoperability
ever become a goal. The PGXL work already left a VITA-49-style discovery
beacon (`FlexRadioDiscoveryBroadcaster.cpp:272`) and a minimal SmartSDR API
server, so that road is shorter than it looks. It is a product decision, not
a technical blocker, and nothing here forecloses it.

### 7.3 Coalescing (required, not an optimization)

Sent naively, a session emits roughly 30 spectrum frames per second plus 25
to 50 Opus frames plus 5 meter updates, each paying a 64-byte header plus
transport framing. §2.5 shows this costs 12.8 kbit/s in headers alone for
audio at 40 ms frames, and more at shorter frames.

**One message per tick carrying a multiplex header and N sub-frames.** At a
30 Hz tick that is 30 messages per second carrying everything. `TciSendQueue`
already drains on a 5 ms timer, so this is a modification to code we own.

---

## 8. Data flow

### 8.1 Receive

```
Radio → P1/P2RadioConnection → ReceiverManager → RxChannel (WDSP demod)
   ├→ audio PCM ──→ OpusAudioCodec ────────────────┐
   ├→ raw IQ ────→ FFTEngine ─→ SpectrumEndpoint ──┤
   │                            (detector+avenger)  ├→ RemoteFrameMux
   │                            → DisplayCodec      │
   ├→ meters (MeterPoller) ────────────────────────┤
   └→ model property changes → StateMirror ────────┘
                                                    ↓
                                    RemoteSession → TciSendQueue → transport

client: transport → demux
   ├→ Opus → decode → jitter buffer → drift resampler → AudioEngine → speakers
   ├→ display frames → decode → SpectrumWidget (trace + waterfall)
   ├→ meter frames → MeterModel
   └→ property updates → StateMirror → models → existing GUI bindings
```

### 8.2 Transmit

```
client: mic → PcMicSource → OpusAudioCodec → transport
daemon: transport → Opus decode → jitter buffer → TxMicRouter
        → TxWorkerThread → TxChannel (full TXA chain) → RadioConnection
```

The complete TX processing chain (EQ, Leveler, ALC, CFC, CPDR, CESSB, Phase
Rotator, DEXP/VOX) runs on the daemon at the radio's clock, unchanged.

### 8.3 Audio clock discipline

The daemon's audio clock and the client's playback clock are independent and
will drift. Two mechanisms, both matching what upstream demonstrably does:

**Jitter buffer**, target depth around 180 ms, adaptive. This absorbs network
variance and, on a reliable transport, head-of-line stalls.

**Continuous drift correction** by resampling at a ratio near unity, driven
by buffer depth. `third_party/r8brain` is already vendored and is the right
tool. MW0LGE's client shows a ratio of 1.000213x in steady state, which is
the expected order of magnitude.

---

## 9. Spectrum path

### 9.1 One FFT, N viewport reducers

**Decision: one `FFTEngine` per receiver at full size, with a per-session
`SpectrumEndpoint` that crops to that viewport's bin range and reduces to its
pixel count.**

Thetis allocates a dedicated WDSP analyzer per endpoint (§2.3). We do not,
for three reasons: it is always at least as many FFTs as our approach and
never fewer; our `SpectrumDetector` and `SpectrumAvenger` are already verbatim
ports of the same WDSP `analyzer.c` that Thetis's analyzer is built from, so
output semantics match anyway; and it would mean maintaining a second spectrum
pipeline beside `FFTEngine`.

`SpectrumWidget::visibleBinRange()` already performs the viewport-to-bin
mapping and is the basis for the crop.

**This model already exists in the codebase as of Phase 3F.** `FFTRouter`
(`src/core/FFTRouter.h`, NereusSDR-original, Sub-Epic D Task 7) does exactly
this fan-out: its own header states that "one DDC can feed N pans at
different zoom levels of the same I/Q data". Its API is
`mapPanToReceiver(panId, receiverId)` and a slot `onFftFrame(receiverId,
binsDbm)` that emits `fftFrameForPan(panId, receiverId, binsDbm)`, fed by the
per-receiver `FFTEngine`.

**`FFTRouter::fftFrameForPan` is therefore the insertion point for the remote
spectrum path**, and this design must attach to it rather than invent a
parallel routing layer. It lives in `src/core/` and is GUI-free, so it works
in the daemon unchanged. `SpectrumEndpoint` becomes a per-pan consumer of
that signal.

**The known cost, which Phase 3F makes live rather than hypothetical:** pans
sharing a receiver share its FFT size. If one pan is zoomed deep, forcing a
large FFT, the long time window smears the waterfall on another pan watching
wide off the same DDC.

An earlier draft of this document claimed the problem was nil at
single-operator scope, on the assumption that one session meant one viewport.
**That assumption is wrong.** Phase 3F gives one operator up to `maxSlices`
pans (five on Saturn, G2, G2E, OrionMkII, Angelia, Orion, AnvelinaPro3,
Andromeda and RedPitaya-P2), and `FFTRouter` explicitly supports several of
them at different zoom levels on the same DDC. A single operator can produce
the collision without a guest session anywhere in sight.

The mitigation is unchanged but its priority is not: two shared FFTs per
receiver, one wide-and-fast and one narrow-and-fine spun up when a pan zooms
past a threshold, rather than N analyzers. It is no longer a
guest-sessions-only concern and should be evaluated during R3 rather than
deferred indefinitely.

**Physics worth recording**, at 192 kHz and 1024 pixels:

| Zoomed to | FFT size needed | Hz/bin | Window spans |
| --- | --- | --- | --- |
| 192 kHz (full) | 1,024 | 188 | 5 ms |
| 48 kHz | 4,096 | 47 | 21 ms |
| 12 kHz | 16,384 | 12 | 85 ms |
| 3 kHz (SSB) | 65,536 | 2.9 | 341 ms |
| 500 Hz (CW) | 524,288 | 0.37 | 2.7 s |

The last row exceeds `kMaxFftSize` (262144). This is not a practical problem:
0.73 Hz/bin is already far finer than useful and the 1.37 s smear at that
size would make it unusable regardless. Frame rate is decoupled from FFT size
by overlap (`FFTEngine.cpp:584`), so 30 fps is available at any size.

### 9.2 Display codec

**NereusSDR-original code.** The parameter names in §2.6 come from a
screenshot, and standard techniques (quantization, temporal delta, block
coding, keyframes) are not ported material. There is no upstream source to
port and therefore no attribution obligation, and equally no source to check
against.

Design:

1. Quantize `float` dBm to 8 bits across a configurable `[minDbm, maxDbm]`
   window. At -135 to -30 that is 105 dB over 256 levels, 0.41 dB per step.
2. Temporal delta against the previous frame.
3. Block-wise coding with adaptive block size.
4. Periodic keyframe (every 120 frames at 30 fps is 4 seconds), plus a
   keyframe on demand after reconnect or decoder desync.
5. Decouple waterfall advance from trace rate ("frames per line"): the trace
   updates at full rate while the waterfall scrolls at half, halving the
   waterfall payload with no perceptual loss.

Target: 1024 pixels at 30 fps in roughly 100 kbit/s, which is 3.4 bits per
pixel, 9.4x against raw float32 and 2.4x against plain 8-bit quantization.
Achieved by upstream, so achievable.

### 9.3 Audio codec

Opus, starting from the parameters in §2.6: application `audio`, signal
`music`, bandwidth `mediumband`, 24 kbit/s, VBR with constrained VBR,
complexity 10, FEC off, 40 ms frames. Bandwidth should track the receive
filter width and mode rather than being fixed, since AM and FM need more than
mediumband.

A lossless or high-rate PCM mode is required for digital-mode work and for
microphone uplink on capable links (§3.1).

**Carried as RTP with Opus per RFC 7587** (§7.2), which also yields RTCP
receiver reports for the loss, jitter, and round-trip figures the client
surfaces as link quality.

### 9.4 Endpoint model and staleness

Three requirements taken from the shape of `clsSpectrumProcessor` and from
what §2.5's screenshots demonstrate. They are easy to build the wrong way and
each has a specific failure mode.

**Per-viewport endpoint configuration, owned by the client.** Each session
independently sets pixel count, frame rate, FFT size, sample rate, zoom, and
pan for each source it is watching. This mirrors `clsSpectrumProcessor`'s
per-endpoint setters, and MW0LGE's own commented-out call sites show the
range expected in practice: `AddReceiver(0, 1024, 15, 8192)` for RX1 against
`AddReceiver(1, 128, 15, 8192)` for RX2, an eightfold difference in pixel
count between two panadapters on one machine.

**Latest-value-wins, not a queue.** The producer keeps the most recent frame
double-buffered with a monotonic frame index. A consumer takes the latest and
notes its index; an unchanged index means nothing new, and an index that
jumped by three means three frames were skipped and **that is correct
behaviour, not loss**. A display never wants a backlog. This is
`clsSpectrumProcessor`'s `dataIndex` model, and it is the single most valuable
idea in that class.

*Failure mode if built as a push queue:* a network or CPU hiccup leaves the
client rendering progressively staler spectrum and falling further behind
with no mechanism to recover, because every frame is still "owed".

**Pixel count is a bandwidth control, decoupled from widget width.** The
client requests a pixel count and stretches to whatever its renderer is. The
§2.5 test client requests 1024 and reports a widget width of 1184, so it
interpolates upward. On a thin link the request drops to 512 and stretches
harder.

*Failure mode if bound to widget width:* resizing a window silently changes
bandwidth, and a large monitor cannot be served on a slow link at all.

**Zoom and pan are applied server-side, before reduction.** The transmitted
pixels cover exactly the visible span and nothing outside it. The §2.5 web
client's debug row (`LHz -40039`, `SHz 140325`, `DDS 14200000`) resolves to a
visible window of 14.15996 to 14.30029 MHz against a 384 kHz endpoint rate,
confirming the server crops to the viewport rather than sending the full span
for the client to discard. This is the same crop model as §9.1, arrived at
independently.

**Frame rate is a target, not a guarantee.** The client measures actual
delivered rate and displays it, as the §2.5 clients do (29.0 against a
requested 30).

### 9.5 Multi-pan budget (Phase 3F)

**The §2.5 measured targets are per panadapter, not per session.** Every
upstream figure we have (94.4 kbit/s, 104 kbit/s display, ON7OFF's "under
100 kbps") describes one panadapter. Phase 3F allows up to `maxSlices` pans
for a single operator:

| SKU class | maxSlices |
| --- | --- |
| Saturn / G2 / G2E / OrionMkII / 7000DLE / 8000DLE / AnvelinaPro3 / Andromeda / Angelia / Orion / RedPitaya P2 | 5 |
| Hermes (ANAN-10/100), RedPitaya P1 | 4 |
| Metis | 3 |
| HermesII (ANAN-10E/100B) | 2 |
| HermesLite2 | 1 |

Five pans at 100 kbit/s each is 500 kbit/s of display traffic before audio,
which is a different link entirely from the single-pan case and is not
something a marginal cellular connection will carry.

**A session-wide budget is therefore required, not per-pan defaults.** The
client is given a total display budget and allocates it across its open pans,
rather than each pan independently requesting 1024 pixels at 30 fps. The
levers already exist per §9.4: pixel count, frame rate, and frames-per-line
are all per-endpoint, so a five-pan layout might run the focused pan at 1024
by 30 and the rest at 256 by 10.

MW0LGE's own commented-out call sites show the same instinct: 1024 pixels for
RX1 against 128 for RX2 (§2.3).

**Pans not visible must not be transmitted at all.** A floated pan on a
hidden monitor, a pan scrolled out of view, or a minimised window should have
its endpoint disabled rather than reduced. `clsSpectrumProcessor` carries
`SetReceiverEnabled` for precisely this and it is the cheapest saving
available.

### 9.6 Wideband extended pan

Phase 3F Sub-Epic F adds a second and structurally different spectrum source:
Protocol 2 wideband packets accumulated and run through a 16384-point FFTW
real-to-complex transform, feeding `SpectrumWidget` bins alongside the normal
DDC path. Zooming out past the DDC span auto-engages it, and the 3F design
notes a deliberate refresh-rate mismatch between the two sources.

**Decision: out of scope for v1, and explicitly so.** Three reasons. It is a
separate data path with a different rate, so it needs its own endpoint and
codec tuning rather than reusing the DDC path. Its visual rendering (bins as
background fill behind the DDC island) is itself deferred to Phase 3F-1
upstream, so there is no settled local behaviour to mirror. And it is a
zoomed-way-out overview, which is the least latency-sensitive and least
frequently used view, making it the cheapest thing to leave out of a first
release.

**What v1 must not do is preclude it.** Wideband becomes an additional stream
identity with its own endpoint configuration, which the §9.4 model already
accommodates. A remote client zoomed past the DDC span in v1 shows the DDC
span and stops, rather than silently misrendering.

---

## 10. Transport

### 10.1 Governing principle

> **No component of the normal operating path may depend on infrastructure
> the operator does not control. Infrastructure we run may make things
> easier; it may never be what makes them possible.**

This is a resilience requirement, not a cost one. If the standard path runs
through hosted infrastructure, an outage takes every station offline at once.
It also fixes incentives: if relay were the default, direct connectivity would
be an optimization nobody exercises and its edge cases would rot.

This principle also rules out any dependency on a third-party VPN client
(Tailscale, ZeroTier) for the app's network layer to function, a required
license or activation check, and any mandatory cloud settings sync.

### 10.2 Direct first, relay last

The connection ladder, tried in this order:

| Tier | Path | Works when |
| --- | --- | --- |
| 1 | IPv6 host to host | Both ends have IPv6. No traversal machinery needed beyond the pinhole the outbound probe creates |
| 2 | IPv4 host to host | Same LAN |
| 3 | IPv4 server-reflexive, hole-punched | Both NATs use endpoint-independent mapping |
| 4 | Relay | Everything else |

This is ICE candidate ordering, and **IPv6 preference is not special-cased**.
It falls out of IPv6 being a host candidate with high local preference, and
host candidates outranking anything requiring NAT. The two requirements,
first-class IPv6 and CGNAT traversal, are one mechanism.

**Structural luck worth noting:** the carriers that impose CGNAT (T-Mobile,
Starlink) do so because they exhausted IPv4, which is the same reason they
hand out IPv6. The population most affected by CGNAT is disproportionately
likely to succeed at tier 1 and never punch at all.

Tier 3 fails only against address-and-port-dependent (symmetric) mapping,
where the reflexive address learned from STUN does not apply to the peer.
Most carrier NAT is endpoint-independent because symmetric behavior breaks
games and VoIP.

### 10.3 The rendezvous, and precisely what it does

Two peers that cannot be dialed cannot find each other without an
introduction. A rendezvous is therefore mandatory. It performs three distinct
jobs, and conflating them caused confusion during design:

| Job | Traffic | Public option exists? |
| --- | --- | --- |
| STUN reflection | ~2 packets per session | Yes, but we self-host |
| Signaling / introduction | ~2 KB per session | **No, and never will** |
| TURN relay | 144 kbit/s per relayed session | No |

Signaling is application-specific by nature: it must know which daemon belongs
to which operator and authenticate both ends. Every peer-to-peer application
runs its own.

STUN cannot be skipped as a *function*, because the address the signaling
server observes is the TCP mapping and media flows over a different UDP
socket with a different NAT mapping. But it need not be a separate or
third-party server: `nereus-rendezvous` answers STUN on a UDP port. **One
binary, one host, three roles.** No dependency on public STUN.

**Critically: an outage stops new sessions from starting. It does not drop
established ones.** Once ICE selects a pair, media flows peer to peer and the
rendezvous holds only idle control connections. This must be preserved
deliberately.

Three further measures reduce the dependency:

- **Cached peer state.** Remember a station's last known candidates and try
  them directly on reconnect before contacting the rendezvous. Addresses
  frequently survive across sessions, giving a path needing no infrastructure.
- **Direct address entry as a first-class option.** A stable IPv6 address,
  same-LAN use, or an existing port forward must be usable by typing it in,
  bypassing the rendezvous entirely.
- **Multiple rendezvous endpoints.** The client accepts an ordered list. We
  run one, others may run their own. Configuration, not architecture.

### 10.4 Transport implementation

The preferred implementation is an ICE stack with DTLS and data channels.
`libdatachannel` is the leading candidate: it provides ICE (via libjuice),
DTLS, and SCTP data channels, is the same stack browsers use, and works with
any standard STUN and TURN server.

**Unreliable data channels are a material benefit beyond traversal.**
WebSocket over TCP head-of-line blocks: one lost packet stalls everything
behind it, which is why upstream carries a 181 ms audio buffer. With data
channels, control goes on a reliable ordered channel and audio and spectrum
on an unreliable one, so a lost audio frame is a concealed 40 ms gap rather
than a stall. It also keeps a future browser client viable.

**Open item, gated before the dependency lands:** `libdatachannel` is MPL-2.0
and libjuice is LGPL. Both are very probably compatible with GPLv2-or-later,
but this project's compliance bar requires verification, not probability. If
it does not clear, the fallback is a native implementation: STUN binding
requests and hole punching are modest, and Qt supplies `QDtls` and
`QDtlsClientVerifier`, so no new dependency is required. The cost of the
fallback is a hand-rolled candidate and check implementation with a likely
lower traversal success rate.

### 10.5 Encryption and exposure

TLS on the rendezvous control channel and DTLS on the media path, both
mandatory. `QWebSocketServer` already supports secure mode and the bind
address is already operator-configurable
(`src/core/TciServer.cpp:1126`), so `wss://` is close to free.

Media is encrypted end to end, so a relay carries ciphertext it cannot read.

### 10.6 IPv6 correctness work

Concrete and non-optional:

- Bind dual-stack. `QHostAddress::Any` is dual-stack in Qt.
  `QHostAddress::LocalHost` is IPv4 `127.0.0.1` only, which is the current
  default.
- Parse and render bracketed literals (`wss://[2001:db8::1]:50001`)
  everywhere: connection dialog, settings, logs, reconnect. This is where
  IPv6 support most commonly breaks silently.
- Happy Eyeballs (RFC 8305). When a hostname yields both A and AAAA, attempt
  both in parallel and take the first to connect. Qt does not do this, and
  without it a client on a network with broken IPv6 stalls for a timeout that
  users read as "the app is slow".
- Client-finds-daemon discovery must be IPv6-capable from the start.
  (`RadioDiscovery`'s IPv4 UDP broadcast on port 1024 is radio-side and on
  the LAN, so it is unaffected.)

### 10.7 Reachability diagnostics

The daemon reports its own situation honestly: whether it has a global IPv6
address or only link-local, whether its IPv4 is in `100.64.0.0/10` (RFC 6598
CGNAT space, a two-line check), or in RFC 1918 space behind a router where
forwarding would work.

A Setup page stating "You have a routable IPv6 address, connect to
`[2001:db8::1]:50001`. Your IPv4 is behind carrier-grade NAT, so port
forwarding is not available" is worth more than documentation. Pure
client-and-daemon code, no infrastructure.

---

## 11. Relationship to upstream Thetis

We build natively now and treat alignment as a later transport
implementation. §2.4 establishes, by verified absence rather than inference,
that the wire format we would otherwise port from does not exist publicly and
has no committed date.

**This is not a schedule risk, because there is nothing to be gated on.**
Waiting would mean waiting for code that has shown no movement in the public
repository since 2026-03-15, against a stated ETA of "perhaps a few months"
from 2026-07-02, and it will most likely arrive as a single large drop rather
than incrementally.

What this does and does not mean:

- Ported material (`clsSpectrumProcessor`'s endpoint model, our existing
  detector and avenger) follows the normal source-first protocol with headers
  and PROVENANCE rows.
- **The display codec is unambiguously original work.** There is no upstream
  source for it in any form, so there is no attribution question and no risk
  of reimplementing GPL code we have not read. The parameter names in §2.6
  come from a screenshot and inform parameter choice only.
- The session layer, state mirror, and transport are likewise
  NereusSDR-original.
- **The portable part is the part we need least.** `clsSpectrumProcessor`
  produces pixels by driving WDSP's analyzer; we already produce them via
  `FFTEngine` plus `SpectrumDetector` plus `SpectrumAvenger`, verbatim ports
  of the same `analyzer.c`, which unlike the Thetis class execute in a
  shipping application. What we take from it is design shape (§9.1, §9.4),
  not code.
- If and when Thetis's TCI WS/WSS extensions land, they become an additional
  transport and command mapping against internals that are already built and
  tested, not a redesign.
- The compatibility surface for any future interoperation is the wire format
  and the pixel semantics, not the internals.

Our existing TCI server is unaffected and continues to serve third-party
applications as it does today.

---

## 12. Safety

### 12.1 TX watchdog (hard requirement)

**If the link drops while transmitting, the daemon keys down immediately.**

This is not solely an engineering concern. Remote operation requires the
control operator to be able to shut down the transmitter, and a stuck carrier
because a hotel wifi dropped is precisely the scenario the rules exist for.

Implementation is driven by transport liveness rather than a bespoke
mechanism, and the liveness source changes as the transport ladder is built:

- On the plain transport (phases R2 through R4), an application-level
  heartbeat on the control channel, with a deadline measured in low hundreds
  of milliseconds while MOX is asserted.
- Once ICE lands (R5), consent freshness (RFC 7675) is already probing
  continuously and declares a dead path within roughly 30 seconds, far faster
  in practice. It supplements rather than replaces the heartbeat, because the
  heartbeat's deadline while transmitting must stay much tighter than consent
  freshness allows.

The daemon drops MOX on whichever signal fires first.

This requires a test, and the test is not optional.

Notably, ON7OFF's product implements the same protection independently, which
is reasonable evidence the requirement is correct.

### 12.2 Further TX protections

- **PTT timeout.** A configurable maximum continuous transmission, defaulting
  to a few minutes.
- **TX disabled until the session is fully established** and state has synced,
  so a half-connected client cannot key an unknown frequency.
- **Existing protections stay daemon-side and authoritative**:
  `TxInterlockPolicy`, PA over-drive safety, SWR gating, out-of-band checks.
  A remote client cannot override them.

---

## 13. Error handling and reconnect

**Link loss.** The client retains last-known state, visually indicates
staleness rather than showing stale values as live, and reconnects with
exponential backoff. The pattern already used by `PgxlConnection` is the
reference.

**Reconnect.** Attempt cached peer candidates first (§10.3), then the
rendezvous. Request a display-codec keyframe on resume. Re-sync state via
`StateMirror` snapshot rather than assuming continuity.

**ICE restart.** A network change (wifi to LTE) triggers gathering and
checking again over the existing rendezvous connections without operator
action.

**Audio underrun.** The jitter buffer refills and the drift resampler
corrects. Underruns are counted and surfaced, matching the "Audio err" and
"Display err" counters upstream exposes.

**Daemon-side client loss.** Drop MOX immediately (§12.1), stop encoding,
retain radio state and connection. The radio does not disconnect because the
operator's laptop closed.

**No audio device on the daemon.** Not an error (§4.3).

---

## 14. Testing

**Unit.** Display codec round-trip with a bounded dB error assertion. Opus
round-trip. `StateMirror` property reflection across the full `Q_PROPERTY`
set. Frame mux and demux. Coalescing correctness under queue pressure. CGNAT
address-space detection.

**Integration.** Daemon and client in one process over an in-memory
transport, exercising the full path without a network. This is the primary
regression harness and should be fast enough for CI.

**Loopback.** Daemon and GUI as separate processes on one machine over
`localhost` with a real radio attached. **This exercises the entire remote
path without any Pi hardware and should be the main development loop.**

**Network.** Daemon on a Pi, client on a laptop, across LAN, then across the
internet, then the target case: **daemon behind one carrier CGNAT and client
behind a different carrier CGNAT.**

**Bench matrix.** A verification matrix under
`docs/architecture/2026-07-28-remote-daemon-verification/` covering each
transport tier, the TX watchdog, PTT timeout, reconnect, ICE restart on
network change, bandwidth against the §2.5 targets, and audio drift over a
long session.

---

## 15. Phasing

Each phase is independently useful and independently verifiable, and each
gets its own implementation plan document. This design is the umbrella; it is
deliberately larger than a single plan.

**R1: build split and daemon skeleton.** Three-way CMake split, the three
prerequisites in §4.2, `nereusd` with config file and systemd unit. The daemon
connects to a radio and runs the RX chain to a local audio device. No network
client.
*Verified by:* headless daemon on a Pi receiving from an ANAN.

**R2: state mirror and plain transport.** `StateMirror`, `SettingsProxy`,
session and auth, and a plain WebSocket transport with no ICE and no codecs
(raw PCM audio, raw float bins). This transport works over loopback, over a
LAN, and over the internet wherever the daemon is directly reachable
(a routable IPv6 address, or an existing port forward), which is what R3 and
R4 are verified against. NAT traversal arrives in R5.
*Verified by:* full operator control of a daemon from a GUI on the same host.

**R3: codecs and spectrum endpoint.** `SpectrumEndpoint` per-viewport
reducer, `DisplayCodec`, `OpusAudioCodec`, coalescing, jitter buffer, drift
resampler. RX complete.
*Verified by:* §2.5 bandwidth targets met over a LAN, audio stable over hours.

**R4: transmit and its safety harness.** Mic uplink, daemon-side TX chain
wiring, TX watchdog, PTT timeout, TX-disabled-until-synced. These ship
together; TX does not land before its safety.
*Verified by:* on-air TX from a remote client, and a watchdog test that
physically severs the link mid-transmission.

**R5: transport ladder.** `nereus-rendezvous` (STUN, signaling, TURN),
candidate gathering and checks, then hole punching, then relay. Cached peer
state, direct address entry, multiple rendezvous endpoints. IPv6 correctness
work (§10.6).
*Verified by:* the CGNAT-to-CGNAT case, and an established session surviving
a rendezvous outage.

**R6: operations.** Reachability diagnostics, connection UX, packaging for
Pi and desktop, documentation.

---

## 16. Deferred, with reasons

**Saturn on-board daemon.** The ANAN-G2 and G2E Saturn board pairs the FPGA
with a Raspberry Pi CM4 over PCIe, so the daemon could run inside the radio
with IQ arriving by DMA and never touching Ethernet. `RadioConnection` is
already an abstract base with pure virtuals and `P1RadioConnection` /
`P2RadioConnection` are simply two implementations, so this is a third
subclass and nothing above it changes. Deferred because: the CM4 is a
Cortex-A72 at 1.5 GHz against a Pi 5's Cortex-A76 at 2.4 GHz and the CPU
budget needs measuring rather than assuming; G8NJJ's `saturn` repository is a
new upstream requiring its own provenance table; and it means installing our
software onto a radio that shipped with working firmware.

**Guest sessions.** Roles and identity exist from day one (§7.1) but only
owner is implemented.

**Per-slice audio over the link.** Phase 3F defers per-slice DSP routing to
3F-1: a new slice's flag updates the model and the codec's DDC assignment,
but the audio actually decoded remains Slice A's. The remote audio path is
therefore built for N streams and ships carrying one, and gains the rest for
free when 3F-1 lands.

**Multiple simultaneous panadapters over the link is NOT deferred.** Phase 3F
makes it a first-class case from the start. See §9.1 and §9.5.

**Collapsing local mode onto the daemon.** §5.

**Web client.** The transport choice in §10.4 keeps it viable; nothing else
here assumes it.

---

## 17. Open items

1. **`libdatachannel` licensing and current state.** Must be verified before
   the dependency is accepted. Fallback documented in §10.4.
2. **CPU headroom measurement on target hardware.** Full WDSP RX plus FFT
   plus Opus plus display codec on a Pi 5. RADE and DeepFilterNet3 are the
   specific risks. This is a measurement spike, and it should happen during
   R1 so the answer is known before R3 depends on it.
3. **Display setup page split** (§6.3). Mechanical but touches shipped UI, so
   it needs its own small design pass.
4. **Two shared FFTs per receiver, or one** (§9.1). Phase 3F makes the
   smearing collision reachable by a single operator with two pans on one DDC
   at different zooms. Evaluate during R3 with a real multi-pan layout rather
   than deciding on paper.
5. **How the session-wide display budget is allocated across pans** (§9.5).
   Whether the client allocates it, the daemon enforces a cap, or both.
   Needs a real five-pan layout on a real link to answer sensibly.
6. **Whether audio rides WebRTC media tracks or data channels** (§7.2, §10.4).
   Media tracks make RTP native and, with a full WebRTC stack, would supply
   the jitter buffer and packet-loss concealment we would otherwise write
   ourselves (§8.3). `libdatachannel` carries media as RTP passthrough
   without that audio pipeline, so it does not remove that work; a full
   `libwebrtc` would, at a much heavier dependency cost. Decide alongside
   open item 1, not separately.

---

## 18. Decisions, for the record

| Decision | Choice |
| --- | --- |
| Baseline | Phase 3F (PR #293) assumed landed, not `main` |
| Spectrum routing | Attach to the existing `FFTRouter`, do not reinvent it |
| TX arbitration | Route through the existing `TxSliceArbiter` |
| Multi-pan | First-class from v1, with a session-wide bandwidth budget |
| Wideband pan | Out of scope for v1, not precluded |
| DSP split point | Daemon demodulates; client is control surface and renderer |
| Upstream alignment | Build natively now; TCI WS/WSS becomes a later transport |
| Spectrum architecture | One shared FFT per receiver, per-viewport crop and reduce |
| Session model | Single operator, one session; identity and role present from day one |
| Architectural cut | Boundary C, models mirrored on both sides |
| Daemon requirement | Optional; local direct path untouched in v1 |
| Packaging | One installer, two binaries, daemon off by default |
| Settings boundary | Three-way: station, operator, operating state |
| Audio framing | RTP with Opus per RFC 7587, plus RTCP for link statistics |
| Spectrum framing | Compact native, VITA-49 semantics, VITA-49 standard not adopted |
| Context and metadata | Control channel, not in-band with data frames |
| Spectrum delivery model | Latest-value-wins with a frame index, never a queue |
| Transport default | Direct first, relay as fallback |
| Hosted infrastructure | May make things easier, never possible |
| Third-party VPN dependency | Rejected |
| TX watchdog | Hard requirement, ships with TX |
