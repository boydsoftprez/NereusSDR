# Display Codec v2 Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** Every remote display row decodes on its own (NSDC v2), so a lost datagram
costs at most one made-up waterfall row instead of a run of rows until a keyframe.

**Architecture:** A stateless v2 plane coder (previous-bin residuals, zigzag, Rice with
a raw escape) beside the v1 codec in `DisplayCodec.{h,cpp}`, a decoder that pairs the
row and trace parts of a split frame, and a per-media-connection agreement (hello
declaration, Core capability, media `start` field) on the `spectrumPicture` pattern.
The Core picks v1 or v2 per media connection; the desktop window and, in a parallel
change, the iPhone app read both. The display budget, the presenter's drawing and the
gap fill do not change.

**Tech Stack:** C++20, Qt6 (QtTest, QJson), the Core's media path
(`DaemonMediaController`, `IMediaTransport`, SCTP data channel), the window's
`RemoteMediaController` and `RemoteDisplayPresenter`; Swift (NereusKit, Swift
Testing, SwiftPM) for lane B.

**Spec:** [Display codec v2: every row stands alone](2026-10-22-display-codec-v2-intra-rows.md).
Every decision is settled there (JJ's decisions are its section 13); this is how it
gets built.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/development/fast-test-loop.md` first.
  They bind every task.
- No upstream code is ported. FLAC, OpenWebRX and KiwiSDR were read for structure only
  (spec section 2). New files carry `no-port-check: NereusSDR-original.` in their
  header; no attribution header, no provenance row.
- Every touched source or test file gets a "Modification history (NereusSDR)" line in
  the file's existing style, for example
  `2026-10-22: NSDC v2 ... J.J. Boyd (KG4VCF), AI-assisted via Claude Code.`
  New files start the block.
- C++ style: no `goto`, no raw `new`/`delete` (Qt parent or `unique_ptr`), `constexpr`
  constants named `kPascalCase`, braces on all control flow, `m_camelCase` members,
  `auto` only when the type is obvious, `qCWarning(lcCategory)` for errors, no
  exceptions. Platform guards `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX` only.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Cross-thread values are `std::atomic`; never hold a mutex in the audio callback. The
  Core encodes displays on its display send thread, so anything the encoder path reads
  that the main thread writes is atomic or read under the existing `displayGuard()`.
- Settings go through `AppSettings`, never `QSettings`. Booleans are the strings
  `"True"` / `"False"`.
- Don't remove code you didn't add (Task 10 removes only code PR 622 added, by JJ's
  decision in spec section 13, item 6).
- Old peers keep their exact bytes: a peer whose hello does not declare `displayCodec`
  gets no new capability entry, its media `start` shape is today's, and it gets
  byte-identical v1 frames. `tests/OlderPeerDisplayRun.h` with
  `tests/data/display_extras_older_peer_frames.json`, and the link conformance fixtures
  under `tests/data/link/v1/`, stay byte for byte unless a step below names the change.
- No visual change: the presenter's drawing, gap fill, mini displays, the NSDX extras,
  the PS3D codec, `DisplayBudget` and both allocators are unchanged.
- American spelling and no em-dashes in all new text (comments, test names, strings,
  docs). No source cites in user-visible strings.
- Commits are GPG-signed (`git commit -S`); never `--no-gpg-sign`. No
  `Co-Authored-By` trailer. If signing fails, stop and report BLOCKED.
- **One build lane on this Mac across every session and crew.** Only one agent
  compiles or runs tests at a time, Xcode and simulator runs included. Tasks marked
  parallel may be worked in parallel worktrees, but each waits for the controller's
  go before it builds or tests.
- Build dir: reuse one per lane. A new build dir is configured and compiled with the
  network denied, never downloading:

  ```bash
  DEPS=~/.config/nereus/work/pr-merge-review/review-deps
  ARGS=()
  for d in "$DEPS"/*-src; do
    n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
    ARGS+=("-DFETCHCONTENT_SOURCE_DIR_${n}=${d}")
  done
  sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
      -DENABLE_DFNR=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
      -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
      "${ARGS[@]}"
  sandbox-exec -p '(version 1)(allow default)(deny network*)' cmake --build build --target <targets>
  ```

  Then grep the configure output for "Performing download step"; it must not appear.
- Build single test targets, never the whole suite by default. Run tests with
  `ctest --test-dir build -R '^<name>$' --output-on-failure` (ctest sets offscreen and a
  per-test home). A direct binary run sets `HOME`, `CFFIXED_USER_HOME` and
  `NEREUS_TEST_HOME` to one scratch folder, `QT_QPA_PLATFORM=offscreen`, and passes
  `-maxwarnings 0`.
- Swift tests: `ios/scripts/swift-test.sh --jobs 2 --filter <SuiteOrTest>`. App tests:
  `ios/scripts/generate-project.sh`, then
  `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test`.
- A failed test is never ignored or retried away. A failure under load is a finding:
  record the load average and the assertion.
- The STRIX (JJ's Windows Core) is read-only to agents unless JJ says yes in chat.
  JJ's own NereusSDR settings are off limits except read-only logs.
- Branches: lane A is `claude/display-codec-v2`, cut from `origin/main` once PR 622
  (keyframe every frame, commit `2bc7ae1a1`) is on main; check with
  `git merge-base --is-ancestor 2bc7ae1a1 origin/main`, and if it is not, the
  controller asks JJ before cutting. Lane B is `claude/display-codec-v2-ios`, cut from
  lane A at Task 1's commit and rebased onto lane A after Task 3 lands (for
  `surface.json` and any golden vector fix).

## What already exists

- `src/core/session/media/DisplayCodec.h` / `.cpp`: NSDC v1.
  - `DisplayCodecContext` (endpointId, contextGeneration, minDbm, maxDbm,
    traceSamples, waterfallSamples, wideSamples).
  - `DisplayCodecFrame` (context, encoderSequence, producerTimestamp,
    waterfallAdvance, traceDbm, waterfallDbm, wideDbm).
  - `enum class DisplayCodecDisposition { Accepted, NeedKeyframe, Rejected }`
    (DisplayCodec.h:40-44) and `DisplayCodecReason` (None ... Malformed).
  - `DisplayCodecDecodeResult { disposition, reason, frame }`.
  - `DisplayCodecEncoder` (`kHeaderBytes = 42`, `kMaxPlanes = 3`,
    `kMaxSamplesPerPlane = 4096`, `kMaxEncodedBytes = 16 * 1024`,
    `kKeyframeInterval = 120`; `encode(frame, requestKeyframe)`,
    `lastEncodedKeyframe()`, `reset()`).
  - `DisplayCodecDecoder` (`decode(const QByteArray&)`, `reset()`).
  - In the `.cpp`: `kMagic 0x4e534443`, `kVersion 1`, `staleOrEqual` (:136),
    `quantize` (:142), `dequantize` (:150), the header Writer and Reader, and the
    decoder's order of checks (:505-625).
- `src/core/session/media/DisplayBudget.h` / `.cpp`:
  `displayCodecWorstCasePlaneBytes(quint32)` (v1's `A(n) = 3 + 5*ceil(n/128) + n`),
  `spectrumDisplayCost(pixels, fps, includeWidePlane)`,
  `SpectrumDisplayCost::maximumFrameBytes`.
- `src/core/session/media/IMediaTransport.h`: `kSctpDataPayloadBytes = 876` (:360),
  `DisplaySendResult` (:386: `Sent`, `Queued`, `Busy`, `Refused`, ...),
  `sctpFragmentCount`.
- `src/core/session/media/DaemonMediaController.h` / `.cpp`:
  - `EndpointEntry` (:559) with `DisplayCodecEncoder encoder` and `forceKeyframe`.
  - `handleStart` (the `spectrumPictureVersion` block at :2367-2380; the negotiated
    flags and `m_startMicLine` at :2458-2465).
  - The `replace` key check (:2904-2913); the replacement's `peer->start` (:3024-3026);
    `finishReplacement` forcing keyframes (:3121-3124).
  - `handleKeyframe` (:4193).
  - The receive display send (encode :6543, cost and pacing :6551-6559,
    `peerSubmitDisplay`, `recordDisplaySent` :1350, `displaySendRefusals`).
  - The transmit display send (`frame.encoderSequence = entry.txSequence++` :7287,
    through :7336).
  - `DaemonDisplayDiagnostics` (h:295) and `daemonDisplayDiagnosticsLine` (:1326),
    logged as `daemon display diagnostics periodic|final ...`.
- PR 622 (on main before lane A is cut):
  - `kDisplayKeyframeEveryFrameSettingsKey[] = "DisplayKeyframeEveryFrame"`.
  - `DaemonMediaController::setDisplayKeyframeEveryFrame(bool)` /
    `displayKeyframeEveryFrame()` / `m_displayKeyframeEveryFrame`.
  - `DaemonMediaHub::setDisplayKeyframeEveryFrame` / `displayKeyframeEveryFrame()`.
  - Diagnostics fields `displayFramesSent`, `displayKeyframesSent`,
    `displayBytesSent`, `displayKeyframeEveryFrame`; the line's
    `framesSent= keyframesSent= meanFrameBytes= keyframeEveryFrame=` before
    `senderGapMaxMs=`, which stays last.
  - The read in `StationHost::start`.
  - Tests:
    - `tst_station_host::displayKeyframeEveryFrameIsReadAtStart(_data)`
    - `tst_daemon_media_controller::keyframeEveryFrameSendsRowsThatDecodeAlone(_data)`
    - `tst_display_budget::keyframeEveryFrameFitsTheAdmittedCostAtEveryPanWidth`
    - the window's per-pan counters (`RemoteDisplayPanTelemetry`)
- `src/core/session/StationServer.h` / `.cpp`:
  - `peerDeclares(SessionTransport*, const QByteArray&, int)` (h:860).
  - `spectrumPictureAvailable(quint64 epoch)` (cpp:13181-13189, h:1305).
  - The capabilities block that sets `caps.spectrumPictureVersion` (cpp:13826-13831)
    and `caps.rxMeterStampsVersion` (cpp:13846).
  - `kRadioIdentitySessionProtocolMinor`.
- `src/core/session/StationCapabilities.h` / `.cpp`: `toUpdates` appends
  `rxMeterStampsVersion` then `coreBuildInfo` (last); `fromUpdates` parses by name;
  fields at h:646 and nearby.
- `src/core/session/StationClient.cpp`: `m_declaredFeatures.insert(...)` for
  `miniDisplay`, `spectrumPicture`, `firmwareBeta` (:1066-1073). Test seam
  `withholdFeatureForTest(feature)`.
- `src/core/station/StationHost.cpp`: `StationHost::start`, with
  `m_stationServer` (created :151) and `cfg.settings->value(...)` reads; listens
  at :717.
- `src/gui/RemoteMediaController.h` / `.cpp`:
  - `RemoteDisplayTelemetry` (h:177).
  - `pictureNegotiated` (cpp:1283, set at :5460-5463); `txMonitorNegotiated` cleared
    in stop (:4217).
  - The binding decoder (:1181); context accept (`binding.decoder.reset()` :8250,
    keyframe request :8257); transmit context request (:8282); re-grant request
    (:8400); `requestKeyframe()` (:8407).
  - `receiveDisplay` (routing :8451-8464, decode :8487, `NeedKeyframe` branch,
    `Accepted` branch, presenter push near :8540); mini display check (:8718-8724).
- `src/gui/RemoteDisplayPresenter.h` / `.cpp`: `Item::measurementCurrent`,
  `push(frame, centreHz, spanHz)`, `push(frame, const RemoteSpectrumCapture&)`,
  `noteLoss()`, `Counters`, `kMaxGapRows = 64`.
- `src/gui/RemoteTelemetryController.cpp`: the `Remote diagnostics:` line with
  `displayKeyframeWaits=` and `displayKeyframeRequests=` (:463-464).
- Tests and harnesses:
  - `tests/tst_display_codec.cpp`, `tests/tst_display_budget.cpp`,
    `tests/tst_display_extras.cpp`.
  - `tests/tst_daemon_display_retune.cpp`: a `Harness` with StationServer,
    DaemonMediaController, StationClient and FakeTransport; `connectSession()` and
    `start(bool declarePicture)`; the `withholdFeatureForTest` pattern in
    `aStartMayDeclareThePictureOnlyAsTheCoreOffersIt`.
  - `tests/tst_daemon_media_controller.cpp`: `Harness`, `establishSession()`,
    `startReadyPeer()`, `useManualDisplayTicks()`, `sendDisplayTick()`, FakeTransport
    `displays`, `nextSubmitResult`, `onDisplaySend`.
  - `tests/tst_remote_media_controller.cpp`: `countControl(spy, op)`.
  - `tests/tst_media_replace.cpp` (REALTIME; `theCoreSendsOnBothThenTheNewPeerTakesOver`).
  - `tests/tst_remote_display_presenter.cpp`, `tests/tst_remote_telemetry.cpp`,
    `tests/tst_station_host.cpp`.
  - `tests/tst_firmware_beta_link.cpp` (capability-order pattern).
  - `tests/tst_remote_audio_session.cpp` (`modernStartWithoutConnection` pins the
    start shape).
  - `tests/LinkSurface.cpp` (hello features near :769-777, capabilities near
    :875-882, start), `tst_link_surface_manifest` and `_regen`
    (`NEREUS_LINK_REGEN_OUT=tests/data/link/v1`), `tests/data/link/v1/surface.json`.
  - `tst_link_conformance_media`.
  - Registration: `nereus_add_test(name ...)` in `tests/CMakeLists.txt`
    (`tst_display_codec` near :10406, `tst_daemon_display_retune` near :6321),
    `target_compile_definitions(... NEREUS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")`.
- Other v1 decoders: `tools/nereus-media-probe.cpp` (`switch (decoded.disposition)`
  :608) and `tests/tools/nereus_rendezvous_peer.cpp` (:860). They stay on v1.
- iOS (NereusKit, `ios/NereusKit/`):
  - `Sources/NereusMedia/DisplayFrameDecoder.swift` (`decode(_:) -> DisplayDecodeResult`,
    `static func dbm(_ q: UInt8, minimum: Double, maximum: Double) -> Float`).
  - `DisplayDecodeResult.swift`, `DisplayDecodeDisposition.swift` (`accepted`,
    `needKeyframe`, `rejected`; String raw values), `DisplayDecodeReason.swift`
    (lowerCamel raw values), `DisplayFrame.swift`.
  - `MediaControlClient.swift`:
    - `Media` per connection with `spectrumPictureDeclared` (:340)
    - start (:1010-1014) and the replacement copy (:1285)
    - replacement decoder reset (:1451) and keyframe request (:1461)
    - `requestKeyframe(endpointId:)` (:2218), context accept request (:2310)
    - `display(_:connection:)` switch (:2359)
  - `MediaFeatureGates.swift` (`spectrumPictureMinor = 11`, `spectrumPicture`).
  - `Sources/NereusLink/LinkFeatures.swift` (`LinkFeatures.app`, near :236).
  - Tests:
    - `Tests/NereusMediaTests/DisplayFrameDecoderTests.swift`
    - `MediaControlClientTests.swift`
    - `Tests/LinkTestSupport/LinkFixtureLoader.swift` (`root`)
    - `Tests/NereusLinkTests/LinkSurfaceOrdinalTests.swift` (`capabilities.count == 116`
      at :20 and :77)
    - `LinkConformanceSessionTests.swift`, `StationSessionLoopbackTests.swift`

## File Structure

Create:

| Path | Responsibility |
| --- | --- |
| `tests/DisplayCodecV2Vectors.h` | Shared, header-only: the vector JSON schema names, a reader, and the deterministic scene generator (splitmix64) used by the regen and codec tests. |
| `tests/tst_display_codec_v2.cpp` | v2 plane and frame coding, bounds, sizes, decoder pairing, loss simulation, malformed set, golden vector check, `QBENCHMARK`. |
| `tests/tst_display_codec_v2_regen.cpp` | Writes `vectors.json` from the encoder into `NEREUS_DISPLAY_CODEC_V2_REGEN_OUT` only. |
| `tests/data/display_codec_v2/vectors.json` | Golden vectors both decoders read. |
| `tests/tst_display_codec_v2_link.cpp` | Core negotiation: capability gate, `start` acceptance, fallback setting, v1/v2 per connection (Tasks 3, 4 and 6). |
| `ios/NereusKit/Tests/NereusMediaTests/DisplayCodecV2VectorTests.swift` | The phone decoder against `vectors.json`. |
| `ios/NereusKit/Sources/NereusMedia/DisplayDecodeCounters.swift` | The phone decoder's counters. |

Modify:

| Path | Change |
| --- | --- |
| `src/core/session/media/DisplayCodec.h` / `.cpp` | v2 plane coder, v2 frame encoder, v2 decoder path, `Held`, frame count, counters, `traceCarried`, public quantize helpers. |
| `src/core/session/media/DisplayBudget.h` | `displayCodecV2WorstCasePlaneBytes`. |
| `src/core/session/StationServer.h` / `.cpp` | v2 gate, setter, capability field. |
| `src/core/session/StationCapabilities.h` / `.cpp` | `displayCodecVersion` entry. |
| `src/core/session/StationClient.cpp` | `displayCodec: 2` hello declaration. |
| `src/core/station/StationHost.cpp` | Read `RemoteDisplayCodecV2` once at start (Task 3); remove the PR 622 read (Task 10). |
| `src/core/session/media/DaemonMediaController.h` / `.cpp` | `start` field, agreed version, v2 send path, split, diagnostics; Task 10 removes the PR 622 setting. |
| `src/gui/RemoteMediaController.h` / `.cpp` | `start` field, v2 flag, frame loop, `Held`, `traceCarried`, skipped keyframe requests, telemetry. |
| `src/gui/RemoteDisplayPresenter.h` / `.cpp` | `push(..., bool measurementCurrent = true)`. |
| `src/gui/RemoteTelemetryController.cpp` | `displayMessagesLost=`, `displayPartsLost=`. |
| `tools/nereus-media-probe.cpp` | A `Held` case. |
| `tests/LinkSurface.cpp`, `tests/data/link/v1/surface.json` | Hello declaration, capability, start field. |
| `tests/CMakeLists.txt` | Register the new tests. |
| Existing tests named per task | New cases. |
| `ios/NereusKit/Sources/NereusMedia/DisplayFrameDecoder.swift`, `DisplayDecodeResult.swift`, `DisplayDecodeDisposition.swift`, `DisplayFrame.swift`, `MediaControlClient.swift`, `MediaFeatureGates.swift` | Phone v2 (Tasks 8, 9). |
| `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift` | `"displayCodec": 2` (Task 9). |
| `docs/architecture/2026-09-20-display-codec-v1.md` | Pointer to v2 (Task 1). |
| `docs/architecture/2026-09-20-remote-media-control-v1.md` | `start` row and a "display codec" section (Task 3). |
| `docs/architecture/2026-09-23-station-link-v1.md` | Hello feature and capability (Task 3). |

## Task 1: NSDC v2 encoder, bounds and golden vectors

**Requirements:** spec sections 4.1, 4.2, 4.3, 5 (the split rule as an encoding
choice), 6 (sizes), 8 (golden vectors), 11 item 1 (encoder, size and bound cases),
12 (the v1 document pointer).

**Files:**
- Modify: `src/core/session/media/DisplayCodec.h`, `src/core/session/media/DisplayCodec.cpp`
- Modify: `src/core/session/media/DisplayBudget.h` (add the v2 bound beside
  `displayCodecWorstCasePlaneBytes`)
- Create: `tests/DisplayCodecV2Vectors.h`, `tests/tst_display_codec_v2.cpp`,
  `tests/tst_display_codec_v2_regen.cpp`, `tests/data/display_codec_v2/vectors.json`
- Modify: `tests/CMakeLists.txt` (register both tests next to `tst_display_codec`, with
  `NEREUS_TEST_DATA_DIR`)
- Modify: `docs/architecture/2026-09-20-display-codec-v1.md` (one paragraph near the
  top: version 2 is specified in `2026-10-22-display-codec-v2-intra-rows.md`, and v1
  stays for every connection that did not agree v2)

**Interfaces:**
- Consumes: nothing.
- Produces (in `namespace NereusSDR`, `DisplayCodec.h` unless noted):
  - `quint8 displayCodecQuantize(float dbm, float minDbm, float maxDbm);` and
    `float displayCodecDequantize(quint8 q, float minDbm, float maxDbm);`: the existing
    internal `quantize` / `dequantize`, made public. The v1 paths call them; v1
    behavior and bytes are unchanged.
  - `QByteArray encodeDisplayCodecV2Plane(const QVector<quint8>& samples);`: one v2
    plane (prefix and bit stream) for 1 to `kMaxSamplesPerPlane` samples; empty
    otherwise.
  - `bool decodeDisplayCodecV2Plane(const QByteArray& bytes, int& offset, int length, QVector<quint8>& samples);`:
    reads exactly `length` samples at `offset` and advances `offset`; false, with
    `offset` and `samples` untouched, for anything malformed or truncated.
  - In `DisplayBudget.h`:
    `constexpr quint32 displayCodecV2WorstCasePlaneBytes(quint32 samples);`, which is
    `P(n) = 4 + n + ceil(3 * ceil((n - 1) / 64) / 8)`, and 0 for 0 samples.
  - ```cpp
    struct DisplayCodecV2Encoding {
        /// One message (whole) or two (row part, then trace part).
        std::vector<QByteArray> messages;
        bool split() const { return messages.size() == 2; }
        qsizetype totalBytes() const;
    };
    /// Empty `messages` for invalid input (the same rules as v1's encode()).
    DisplayCodecV2Encoding encodeDisplayCodecV2(const DisplayCodecFrame& frame,
                                                quint32 admittedFrameBytes);
    ```
  - `DisplayCodecFrame` gains `bool traceCarried {false};` (set only by the v2 decoder,
    Task 2).
  - `tests/DisplayCodecV2Vectors.h`: `namespace NereusSDR::Test::DisplayCodecV2Vectors`
    with `QString vectorsPath()` (`NEREUS_TEST_DATA_DIR "/display_codec_v2/vectors.json"`),
    the scene generator `QVector<float> sceneRow(Scene, int pixels, quint64 seed, int frame)`
    and `enum class Scene` (named below), and JSON helpers to read and write the
    schema below.
  - `tests/data/display_codec_v2/vectors.json`, schema version 1 (exact):

    ```json
    {
      "format": "nsdc2-vectors",
      "schema": 1,
      "planes": [
        {"name": "...", "samples": [0, 255], "blockSize": 16, "plane": "<hex>"}
      ],
      "frames": [
        {"name": "...", "admittedFrameBytes": 1684, "frame": FRAME, "messages": ["<hex>"]}
      ],
      "sequences": [
        {"name": "...", "messages": ["<hex>"],
         "results": [{"disposition": "accepted", "reason": "none", "frames": [FRAME]}],
         "counters": {"framesLost": 0, "partsLost": 0, "staleMessages": 0,
                      "duplicateParts": 0, "malformed": 0}}
      ]
    }
    ```

    `FRAME` is `{"endpointId", "contextGeneration", "encoderSequence",
    "producerTimestamp" (a decimal string), "minDbm", "maxDbm", "traceSamples",
    "waterfallSamples", "wideSamples", "waterfallAdvance", "traceCarried",
    "trace", "waterfall", "wide"}`, where the three planes are arrays of quantized
    values (0 to 255). `minDbm` and `maxDbm` are exact binary values (-150 and -30).
    Dispositions and reasons use the lowerCamel names of the Swift enums
    (`accepted`, `held`, `rejected`; `none`, `staleSequence`, `malformed`,
    `oldContext`, `unknownFlags`, `unsupportedVersion`, `truncated`, `badMagic`,
    `oversized`, `contextMismatch`), so both languages map them by name. A decoder
    passes a frame when every plane value equals its own dequantization of the stated
    q and every header field matches.

**Acceptance:**
- Plane coding follows section 4.2 exactly:
  - Prefix: coding `2`, block code `0`/`1`/`2` for 16/32/64, u16 payload bytes, big
    endian.
  - First sample: 8 bits. Each block starts with a 3-bit `k`; `k = 7` means raw 8-bit
    samples.
  - Residuals are 8-bit wrapping, read as signed, then zigzag-mapped
    (`z = 2r` or `-2r - 1`), and written as `z >> k` zeros, a one, and the low `k`
    bits. Zero pad to a byte, MSB first.
  - Hand-checked vectors, written into the test as literal bytes:
    - `{100}`: prefix `02 00 00 01`, payload `64`.
    - `{100, 101, 99}`: residuals +1 and -2 give z = 2 and 3. `k = 1` (9 bits)
      ties `k = 2` and wins as the smaller. Bits `01100100 001 010 011` plus pad:
      prefix `02 00 00 03`, payload `64 29 80`.
    - `{0, 255}`: residual -1, so z = 1. `k = 0` (`01`) ties `k = 1` (`11`) and wins.
      Bits `00000000 000 01` plus pad: prefix `02 00 00 02`, payload `00 08`.
- Encoder determinism:
  - Each block keeps the fewest bits; ties go to the smaller `k`.
  - The escape is taken only when strictly fewer bits than every `k`.
  - The block size with the fewest plane bytes wins; ties go to the smaller size.
  - Encoding the same input twice gives the same bytes. Each `planes` vector
    re-encodes to its stated hex.
- Round trip and bound:
  - For every `n` from 1 to 4,096 with uniform-noise samples (seeded), the plane
    decodes to the input exactly.
  - Its size is `<= displayCodecV2WorstCasePlaneBytes(n)`, and that is
    `<= displayCodecWorstCasePlaneBytes(n)` (arithmetic check for every `n`, 1 to 4,096).
  - `displayCodecV2WorstCasePlaneBytes(783) == 792` and
    `displayCodecV2WorstCasePlaneBytes(4096) == 4124`.
  - All-equal, ramp (`i & 255`), alternating 0/255 and all-255 rows round-trip.
- Frame coding:
  - Header: v1's 42 bytes with `version = 2`. Endpoint at offset 8, generation at 12.
    The header carries the full context.
  - Flags are exactly one of `0x08`, `0x1A`, `0x1E` (whole) or `0x32`/`0x36` plus `0x2A`
    (split). `0x04` is set exactly when a waterfall plane is present and the wide
    length is not zero.
  - A frame with `waterfallAdvance` false encodes as `0x08`, the trace only.
  - Planes appear in trace, waterfall, wide order. The message ends at its last plane.
- Split rule (section 5), with the 876 threshold being
  `IMediaTransport::kSctpDataPayloadBytes`:
  - A whole message of `<= 876` bytes, or any `0x08` frame, is one message.
  - Otherwise, when the row part plus the trace part `<= admittedFrameBytes`, it is two
    messages: `[0]` the row part (`0x32`/`0x36`), `[1]` the trace part (`0x2A`). Both
    carry the frame's header fields.
  - Otherwise it is one whole message.
  - Cases:
    - A 783-pixel peak-scene frame with
      `admittedFrameBytes = spectrumDisplayCost(783, 30, false)->maximumFrameBytes`
      splits.
    - A flat 783-pixel frame stays whole.
    - A 600-pixel uniform-noise frame with `admittedFrameBytes` equal to its whole
      size stays whole.
- No whole or split total exceeds `42 + 2*A(n) + A(wide)` for its shape.
  - Every `n` from 1 to 4,096, with wide 0 and 768, splits within
    `spectrumDisplayCost(n, 30, wide > 0)->maximumFrameBytes`. Use noise rows, and
    `n` 1 to 768 for the wide plane at trace 783.
- Scene sizes:
  - The test generator `sceneRow` is portable: splitmix64 seeded with 1234 and inverse
    CDF for the exponential noise, so C++ and Swift can reproduce it.
  - Scenes:
    - floor `-125 + 3 sin(i*0.004*783/N) + sin(i*0.05*783/N)`
    - 12 carriers (SNR `10 + 40u`), 4 keyed CW (toggle p 0.15, SNR `20 + 20u`), and
      2 SSB 15 px wide (SNR 25, level -60 with p 0.3, else `-10u`)
    - Gaussian width 1.2
    - per-bin exponential noise; peak = max, average = mean over the bins of a pixel
    - IIR alpha on dBm, and alpha 0.12 on the trace only for averaged-trace scenes
  - Range -150 to -30. Over 300 frames, the mean v2 frame bytes are within 1.03 times
    these figures:

    | Scene | Bytes |
    | --- | ---: |
    | Peak, 167 bins/px | 930 |
    | Peak, trace averaged | 784 |
    | Average, 167 bins/px | 757 |
    | Average, trace averaged | 710 |
    | Peak IIR 0.3 | 743 |
    | Average, 8 bins/px | 1,064 |
    | Average, 8 bins/px, trace averaged | 949 |
    | Raw | 1,312 |
    | Uniform noise | 1,668 |
    | Flat | 258 |
    | Ramp | 386 |
    | 1,024 px average | 988 |
    | 4,096 px peak | 4,821 |

    The 4,096 px scene uses 32 bins per pixel peak, and the 1,024 px scene 128 bins per
    pixel average. If the portable generator cannot reach a figure within 1.03, the
    implementer reports the measured values instead of loosening the bound.
- Quantization helpers: for every q in 0 to 255 at range -150 to -30,
  `displayCodecQuantize(displayCodecDequantize(q, -150, -30), -150, -30) == q`.
  `tst_display_codec` still passes unchanged (v1 bytes identical).
- `vectors.json` is written only by `tst_display_codec_v2_regen` into
  `NEREUS_DISPLAY_CODEC_V2_REGEN_OUT` (a temporary directory when unset, as
  `tst_link_surface_manifest_regen` does).
- `tst_display_codec_v2` checks that every `planes` and `frames` entry re-encodes to
  its stated hex, so a codec change cannot drift from the file.
- The file holds:
  - at least the three hand-checked planes, plus noise, flat, ramp and alternating
    planes at 783 and 4,096
  - one plane for each block size winning
  - whole `0x08`, `0x1A` and `0x1E` frames, and split `0x32`+`0x2A` and `0x36`+`0x2A`
    frames
  - the `sequences` list (decoder expectations), stated by hand from the spec tables:
    - every row of the section 9.2 pairing table
    - either part first; a lost row part; a lost trace part
    - a duplicate part; a part older than the held one
    - disagreeing headers; a whole frame with the held sequence
    - a generation change while a part is held
    - `framesLost` across the 2^32 wrap (sequence `0xFFFFFFFE` then `1`:
      `framesLost` 2)
    - fills of minimum dBm (q 0) after a reset and after a generation change
    - one message for each malformed case listed in Task 2
  - Task 2 verifies `sequences`; Task 1 only writes them.

**Verification:** risk tier: wire format (new bytes on the network), so test-first for
the plane coder and the split rule. Unit layer. Commands:
`cmake --build build --target tst_display_codec_v2 tst_display_codec_v2_regen tst_display_codec`;
`ctest --test-dir build -R '^tst_display_codec(_v2|_v2_regen)?$' --output-on-failure`.
Regenerate the file with
`NEREUS_DISPLAY_CODEC_V2_REGEN_OUT=tests/data/display_codec_v2 HOME=$T CFFIXED_USER_HOME=$T NEREUS_TEST_HOME=$T QT_QPA_PLATFORM=offscreen ./build/tests/tst_display_codec_v2_regen -maxwarnings 0`
(with `T=$(mktemp -d)`), and read the diff before committing.

**Execution note (advisory):** opus. Touches the wire format (no device reachability).
No prerequisites. Not parallel: lane A's Tasks 2 to 6 and lane B both start from its
commit.

- [ ] **Step 1:** Write the hand-checked plane cases, the bound arithmetic and the
  split cases in `tst_display_codec_v2.cpp`; register both tests; see them fail.
- [ ] **Step 2:** Make the quantize helpers public, then add the plane coder, the bound
  and `encodeDisplayCodecV2`, until the cases pass and `tst_display_codec` is
  unchanged.
- [ ] **Step 3:** Add `DisplayCodecV2Vectors.h` (generator, schema), the scene-size
  cases, the noise round trip for every `n`, and the regen test; generate
  `vectors.json` with the hand-stated `sequences`.
- [ ] **Step 4:** Add the v1 document's pointer paragraph; commit.

## Task 2: NSDC v2 decoder and part pairing

**Requirements:** spec sections 4.1, 4.2 (decoder rejects), 9.1, 9.2, 10 (benchmark),
11 item 1 (decoder, pairing, loss and malformed cases), 12 (`tools/nereus-media-probe.cpp`).

**Files:**
- Modify: `src/core/session/media/DisplayCodec.h`, `src/core/session/media/DisplayCodec.cpp`
- Modify: `tools/nereus-media-probe.cpp` (a `case DisplayCodecDisposition::Held:` that
  counts nothing and requests nothing; it stays on v1 and never sees one)
- Test: `tests/tst_display_codec_v2.cpp`

**Interfaces:**
- Consumes: Task 1's plane coder, `encodeDisplayCodecV2`, `DisplayCodecFrame::traceCarried`,
  `vectors.json` and `DisplayCodecV2Vectors.h`.
- Produces:
  - `enum class DisplayCodecDisposition { Accepted, NeedKeyframe, Rejected, Held };`
    (`Held` appended last).
  - `DisplayCodecDecodeResult` gains `int frameCount {0};` and
    `DisplayCodecFrame secondFrame;`. `frame` holds the first released frame. The v1
    path sets `frameCount = 1` on `Accepted` and 0 otherwise, and never fills
    `secondFrame`.
  - ```cpp
    struct DisplayCodecDecoderCounters {
        quint64 framesLost {0};
        quint64 partsLost {0};
        quint64 staleMessages {0};
        quint64 duplicateParts {0};
        quint64 malformed {0};
    };
    ```
    and `const DisplayCodecDecoderCounters& DisplayCodecDecoder::counters() const;`.
    Counters survive `reset()` (the window resets on every context accept and reads
    the totals). Only the v2 path moves them.
  - `DisplayCodecDecoder::decode()` chooses by the version byte (offset 4): 1 is
    today's path, unchanged; 2 is the v2 path; anything else is `Rejected`
    `UnsupportedVersion`. The two paths keep separate state. `reset()` clears both
    (v2: context, `L`, `H`, fill planes).

**Acceptance:**
- Order of checks (section 9.1): the whole structure first, then endpoint, context,
  generation and sequence, then apply.
  - Structure covers the header, flag combination and every plane.
  - Structural rejects (`BadMagic`, `UnsupportedVersion`, `UnknownFlags`, `Truncated`,
    `Oversized`, `Malformed`) count `malformed`.
  - Context rejects (`ContextMismatch`, `OldContext`) count nothing.
  - A structurally bad message of an old generation is a structural reject, as in v1's
    tests.
- Malformed set, each a literal message:
  - Flags `0x01`, `0x40`, `0x80` give `UnknownFlags`.
  - Every other flag byte outside `{0x08, 0x1A, 0x1E, 0x2A, 0x32, 0x36}` (built
    from bits `0x02`, `0x04`, `0x08`, `0x10`, `0x20`) gives `Malformed`.
  - `0x1A` with a nonzero wide length, and `0x1E` with a zero wide length, give
    `Malformed`.
  - Plane coding byte not 2, and block code 3 and 255, give `Malformed`.
  - Payload overrun, a byte after the last plane, and payload bytes left after the
    last sample give `Malformed`.
  - Nonzero pad bits, and a `z` above 255 (`k = 0` with 256 leading zeros), give
    `Malformed`.
  - Truncation at every byte of a valid split part and a valid whole frame gives
    `Truncated` or `Malformed`, never a crash or an accept.
- Pairing (section 9.2), each with its disposition, frame count and counters. Use
  sequences on one generation, with `L` starting empty:
  - Whole frame 1 gives `Accepted` (1 frame), `framesLost` 0. Then whole frame 4
    gives `Accepted` (1 frame), `framesLost` 2.
  - Row part 5 gives `Held`, reason `None`, frame count 0. Then trace part 5 gives
    `Accepted` (1 frame): the merged frame has the waterfall advance set, the trace
    from the trace part, the row planes from the row part, and `traceCarried` false.
  - The trace part first, then the row part, gives the same merged frame.
  - Row part 6 held, then row part 6 again: `Rejected` `StaleSequence`,
    `duplicateParts` 1, and `H` stays.
  - Row part 6 held, then trace part 6 whose timestamp differs: `Rejected`
    `Malformed`, `malformed` 1, and `H` stays.
  - Row part 6 held, then whole 6: `Rejected` `Malformed`, `malformed` 1.
  - `L` = 4 and row part 6 held, then whole 5: `Accepted` (1 frame, sequence 5), and
    `H` stays.
  - With `L` = 5, row part 6 held, then whole 8: `Accepted` with 2 frames. `frame` is the partial
    6 (row only: `traceCarried` true, trace = last decoded trace, or q 0 when none).
    `secondFrame` is 8. `partsLost` 1, and `framesLost` moves by 1 (for 7).
  - Row part 6 held, then trace part 7: `Accepted` with 1 frame (partial 6), and part
    7 is held.
  - Trace part only (row lost), released by a later message: the trace, waterfall
    advance clear, waterfall and wide = the last decoded ones (q 0 when none).
  - A stale whole or part: `Rejected` `StaleSequence`, `staleMessages` 1.
  - A `0x08` whole frame: the trace, waterfall advance clear, fills as above.
  - A newer generation while a part is held: `H` is released as a partial frame
    under the old context (fills from the old context, `partsLost` 1, `framesLost`
    from `h` against the old `L`). Then the new context starts with no `L` and empty
    fills. The new message is handled as a first message; no frames lost across
    generations.
  - An older generation: `Rejected` `OldContext`, no counter.
  - Sequence `0xFFFFFFFE`, then `1`: `framesLost` 2.
  - After `reset()`, the first released row-only partial frame carries a trace of
    q 0. Counters keep their values across `reset()`.
  - v2 never returns `NeedKeyframe` or `NoHistory`.
- Golden vectors: every `sequences` entry of `vectors.json` gives exactly its
  `results` and `counters` from a fresh decoder. A wrong hand-stated expectation is
  fixed in the regen source and the file, with the spec rule that decides it named
  in the commit message.
- Loss simulation:
  - Model: Gilbert-Elliott, per datagram (`ceil(bytes / 876)` datagrams per message),
    with r = 1/3 and p = r*0.02/0.98 (2 percent loss, mean burst 3). Separately,
    random 2 percent drops. Each with adjacent-message reordering at probability 0.1.
  - Run 3,000 frames of the peak scene at 783 px, and 1,000 at 4,096 px, fixed seeds.
  - Every released frame equals the encoder's quantized input for its sequence
    (merged or whole), or follows the partial and fill rules, plane for plane.
  - Released sequences are strictly increasing, and
    `framesLost + released frames == last released - first released + 1`.
  - `partsLost` equals the number of released partial frames.
  - Every frame none of whose messages was dropped or moved by a swap is released
    whole or merged.
- `QBENCHMARK` cases: v2 encode and decode of one 783-pixel and one 4,096-pixel peak
  frame. They are recorded, not asserted.
- `tst_display_codec` (v1) passes unchanged.

**Verification:** risk tier: wire format and parser of network input, so test-first.
Unit layer. Commands:
`cmake --build build --target tst_display_codec_v2 tst_display_codec nereus-media-probe`;
`ctest --test-dir build -R '^tst_display_codec(_v2)?$' --output-on-failure`. Check
`-maxwarnings 0` output is free of warnings with one direct run.

**Execution note (advisory):** opus. Parser of untrusted network bytes (flag for an
earlier look if JJ wants one). Prerequisite: Task 1. Can run in parallel with Task 3
in its own worktree, building only when the controller gives it the lane.

- [ ] **Step 1:** Add the malformed set and the pairing cases; see them fail.
- [ ] **Step 2:** Add `Held`, the result fields, the counters, the version dispatch and
  the v2 path until the cases and the golden `sequences` pass.
- [ ] **Step 3:** Add the loss simulation and the benchmark; add the probe's `Held`
  case; commit.

## Task 3: Negotiation and the fallback setting

**Requirements:** spec section 7 (declare, advertise, accept, fallback switch), 11
item 3 (the capability, `start` and fallback cases at the Core), 12 (`StationServer`,
`StationCapabilities`, `StationClient`, fixtures, docs).

**Files:**
- Modify: `src/core/session/StationServer.h`, `src/core/session/StationServer.cpp`
- Modify: `src/core/session/StationCapabilities.h`, `src/core/session/StationCapabilities.cpp`
- Modify: `src/core/session/StationClient.cpp`
- Modify: `src/core/station/StationHost.cpp`
- Modify: `src/core/session/media/DaemonMediaController.h`, `.cpp` (`handleStart`
  only)
- Modify: `tests/LinkSurface.cpp`, `tests/data/link/v1/surface.json` (regenerated)
- Modify: `ios/NereusKit/Tests/NereusLinkTests/LinkSurfaceOrdinalTests.swift` (the
  capability count 116 becomes 117, at :20 and :77, because `surface.json` gains one
  capability)
- Create: `tests/tst_display_codec_v2_link.cpp` (register with
  `fakes/LoopbackTransport.cpp` and `EXTRA_LABELS session`, as
  `tst_daemon_display_retune`)
- Modify: `tests/tst_station_host.cpp`, `tests/tst_firmware_beta_link.cpp` (only if its
  order assertions name the entry before `coreBuildInfo`; it must still pass)
- Modify: `docs/architecture/2026-09-20-remote-media-control-v1.md` (the `start` row
  gains `displayCodecVersion`; a short "The display codec a connection agrees"
  section after "#### The picture a context continues" (:430), pointing at the v2
  spec), `docs/architecture/2026-09-23-station-link-v1.md` (the hello feature
  `displayCodec` 2 beside `spectrumPicture` near :1006, the `displayCodecVersion` row
  in the capability table near :1258)

**Interfaces:**
- Consumes: nothing from Tasks 1 and 2.
- Produces:
  - `StationServer.h`:
    `inline constexpr char kRemoteDisplayCodecV2SettingsKey[] = "RemoteDisplayCodecV2";`
  - `void StationServer::setDisplayCodecV2Enabled(bool enabled);` and
    `bool StationServer::displayCodecV2Enabled() const;` (default true).
  - `bool StationServer::displayCodecV2Available(quint64 epoch) const;`: true exactly
    when all of these hold:
    - `mediaAvailable(epoch)`
    - the media session's peer has `agreedMinor >= kRadioIdentitySessionProtocolMinor`
    - `peerDeclares(session, "displayCodec", 2)`
    - `displayCodecV2Enabled()`

    It is modeled line for line on `spectrumPictureAvailable`.
  - `StationCapabilities::displayCodecVersion` (`int`, default 0). `toUpdates` appends
    `intEntry("displayCodecVersion", displayCodecVersion)` when it is above 0, after
    `rxMeterStampsVersion` and before `coreBuildInfo`, which stays last. `fromUpdates`
    reads it, and a bad entry reads as absent, as the neighbors do.
  - The server's capabilities block sets
    `caps.displayCodecVersion = caps.radioIdentityEntries && media && peerDeclares(transport, "displayCodec", 2) && displayCodecV2Enabled() ? 2 : 0;`
    next to `caps.rxMeterStampsVersion`.
  - `StationClient` declares `m_declaredFeatures.insert("displayCodec", 2)` beside
    `spectrumPicture`. `withholdFeatureForTest("displayCodec")` withholds it.
  - `StationHost::start` reads the setting once, before the server listens:
    `cfg.settings->value(kRemoteDisplayCodecV2SettingsKey, "True").toString() == "True"`,
    passed to `m_stationServer->setDisplayCodecV2Enabled(...)`. When off, it logs one
    `qCInfo(lcApp)` line naming the key, as PR 622's read does.
  - `DaemonMediaController`: `handleStart` accepts `displayCodecVersion` exactly as it
    accepts `spectrumPictureVersion`. `legacyShape.remove`; refused unless
    `m_server->displayCodecV2Available(m_epoch)` and `exactUnsigned(..., nonzero)`
    and the value `== 2`. On success it stores the agreed version in
    `std::atomic<quint32> m_displayCodecVersion{1}` (2 when the field was accepted,
    else 1), assigned beside `m_startMicLine`. It is never reset by
    `finishReplacement` or the `replace` path (a `replace` carries no `start`), only
    by the next `start`. Accessor:
    `quint32 DaemonMediaController::displayCodecVersion() const noexcept;`.

**Acceptance (in `tst_display_codec_v2_link`, on the `tst_daemon_display_retune`
harness pattern):**
- Default Core, client declares `displayCodec` 2, minor 11, media on:
  - `client.capabilities().displayCodecVersion == 2`.
  - The capability names in order end with `..., "rxMeterStampsVersion",
    "displayCodecVersion", "coreBuildInfo"` (when those neighbors are sent).
  - A `start` with `displayCodecVersion` 2 starts media, and
    `controller.displayCodecVersion() == 2`.
- The same Core, `start` with `displayCodecVersion` set to each of `1`, `3`, `0`,
  `true`, `"2"` and `2.5`: no media transport is created (refused). Then a `start`
  without the field starts media with `displayCodecVersion() == 1`.
- Client with `withholdFeatureForTest("displayCodec")`:
  - `displayCodecVersion == 0`, and there is no `displayCodecVersion` entry. Its
    capability update list is byte-identical to a Core built before this task (compare
    against the list with the entry removed from a declaring peer's, as
    `theCoreStatesThePictureOnlyToAClientThatDeclaredIt` does).
  - A `start` carrying the field is refused; one without it starts.
- `server.setDisplayCodecV2Enabled(false)` before the session: a declaring client sees
  `displayCodecVersion == 0`, and its `start` carrying the field is refused.
- `tst_station_host`: `remoteDisplayCodecV2IsReadAtStart_data` rows: absent,
  `"True"`, `"False"`, `"true"`, `"0"`. Only absent and `"True"` enable it. Check
  `classifySettingsKey(QStringLiteral("RemoteDisplayCodecV2")) == SettingsScope::OperatorLocal`,
  as PR 622's test does for its key.
- `LinkSurface.cpp` adds `{"displayCodec", 2}` to the hello features,
  `caps.displayCodecVersion = 2` to the captured capabilities, and
  `displayCodecVersion` to the start shape it records.
  - Regenerated `surface.json` differs from today's only by those three additions.
    Read the diff.
  - `tst_link_surface_manifest` passes.
  - The phone's `LinkSurfaceOrdinalTests` passes with 117.
- Unchanged and passing: `tst_daemon_display_retune`, `tst_firmware_beta_link`,
  `tst_link_conformance_control`, `tst_link_conformance_session`,
  `tst_link_conformance_media`, `tst_display_extras`, `tst_remote_audio_session`.
  The window does not yet send the field, so its start shape is today's.

**Verification:** risk tier: networking and older-peer compatibility (an old peer's
exact capability list), so test-first for the gate and the order. Integration layer
(loopback session). Commands:
- Build:
  `cmake --build build --target tst_display_codec_v2_link tst_station_host tst_link_surface_manifest tst_link_surface_manifest_regen tst_daemon_display_retune tst_firmware_beta_link tst_link_conformance_control tst_link_conformance_session tst_link_conformance_media tst_display_extras tst_remote_audio_session`
- Test:
  `ctest --test-dir build -R '^(tst_display_codec_v2_link|tst_station_host|tst_link_surface_manifest|tst_daemon_display_retune|tst_firmware_beta_link|tst_link_conformance_(control|session|media)|tst_display_extras|tst_remote_audio_session.*)$' --output-on-failure`
- Regenerate:
  `NEREUS_LINK_REGEN_OUT=tests/data/link/v1 QT_QPA_PLATFORM=offscreen HOME=$T CFFIXED_USER_HOME=$T NEREUS_TEST_HOME=$T ./build/tests/tst_link_surface_manifest_regen -maxwarnings 0`
- Swift: `ios/scripts/swift-test.sh --jobs 2 --filter LinkSurfaceOrdinalTests`
  (its own lane turn).

**Execution note (advisory):** opus. Networking and session negotiation: flag for an
earlier review. Prerequisite: Task 1 (branch point only). Can run in parallel with
Task 2 in its own worktree; the two touch disjoint files.

- [ ] **Step 1:** Write the capability, `start` and fallback cases in
  `tst_display_codec_v2_link.cpp` and the `tst_station_host` rows; see them fail.
- [ ] **Step 2:** Implement the server gate and setter, the capability field and entry,
  the hello declaration, the StationHost read and the `handleStart` acceptance.
- [ ] **Step 3:** Update `LinkSurface.cpp`, regenerate `surface.json`, read the diff,
  and bump the phone's ordinal count.
- [ ] **Step 4:** Update the two docs; run the verification set; commit.

## Task 4: Core sender in v2

**Requirements:** spec sections 5 (send order, refusals, pacing on the total), 7
("Core encodes", kept across `replace`, Step 1 applies to v1 only), 9.3 (keyframe
no-op, the forced keyframe a no-op), 11 item 4 (diagnostics), 12
(`DaemonMediaController`).

**Files:**
- Modify: `src/core/session/media/DaemonMediaController.h`, `.cpp`
- Test: `tests/tst_daemon_media_controller.cpp`, `tests/tst_display_codec_v2_link.cpp`

**Interfaces:**
- Consumes:
  - `encodeDisplayCodecV2(const DisplayCodecFrame&, quint32)`,
    `DisplayCodecV2Encoding` (Task 1); `DisplayCodecDecoder` with v2 (Task 2).
  - `DaemonMediaController::displayCodecVersion()` and `m_displayCodecVersion`
    (Task 3).
  - PR 622's `m_displayKeyframeEveryFrame` and the `displayFramesSent`,
    `displayKeyframesSent`, `displayBytesSent` fields.
- Produces:
  - `DaemonDisplayDiagnostics` gains `quint64 displaySplitFrames = 0;` and
    `quint64 displaySecondPartRefusals = 0;`.
  - `daemonDisplayDiagnosticsLine` gains `splitFrames=%N secondPartRefusals=%M`,
    placed after `keyframeEveryFrame=` and before `senderGapMaxMs=`, which stays last.

**Acceptance:**
- On a connection whose `start` agreed v2, every receive and transmit display frame is
  encoded with
  `encodeDisplayCodecV2(frame, current.displayCost.maximumFrameBytes)` (the transmit
  display with its own endpoint's admitted cost). The v1 encoder and
  `forceKeyframe` are not consulted.
- On a v1 connection the bytes are exactly today's: `tst_display_extras`' older-peer
  golden passes, and `keyframeEveryFrameSendsRowsThatDecodeAlone` passes.
- Step 1 (`DisplayKeyframeEveryFrame` on) does not change v2 bytes.
- Pacing and the admitted cost are checked once, against `totalBytes()` and the
  frame's sample count, before the first message is offered.
  - Over the cost (which cannot happen by section 4.3), the frame is dropped with
    `qCWarning(lcDaemonMedia)`, and no keyframe is forced.
  - A test drives it with an admitted cost made smaller than the frame (a harness
    `DisplayBudgetLimits` or direct entry cost).
- A split frame's two messages go in the same sender turn, row part first. This is an
  exception to "a turn sends one display message at most", for v2 split frames only.
  With FakeTransport:
  - Both `Sent`: `displays` holds `[row part, trace part]` in that order;
    `displaySplitFrames` 1.
  - First part `Busy` or `Refused`: nothing more is offered, and `displays` gains
    nothing from that frame. `displaySendRefusals` 1, `displaySplitFrames` 0. The next
    frame is sent normally as v2 (no keyframe state).
  - First `Sent` or `Queued`, second `Busy` or `Refused`: `displaySecondPartRefusals` 1.
    The frame counts as sent (its row was taken).
- Diagnostics on v2:
  - `displayFramesSent` counts frames, not messages.
  - `displayBytesSent` counts the bytes the transport took.
  - `displayKeyframesSent` does not move.
  - `displayMaxDeltaBytes` and `displayMaxFragments` take the largest single message.
  - The line reads `... keyframeEveryFrame=off splitFrames=1 secondPartRefusals=0 senderGapMaxMs=...`
    in a test that parses it.
- `handleKeyframe` on a v2 connection is counted in `displayKeyframeRequests` as today
  and sends nothing extra. The limit is unchanged.
- `finishReplacement` still sets `forceKeyframe`, and a v2 connection ignores it.
- The NSDX extras datagram still follows each frame once, after the frame's last part.
- Decode check: every message the FakeTransport saw on a v2 connection, fed in order
  to a `DisplayCodecDecoder`, gives `Accepted` or `Held` and nothing else; the
  released frames match the frames the Core reduced.
- `tst_display_codec_v2_link`: a v2 `start`, then frames: every display message has
  version byte 2. A v1 `start` on the same Core, in another harness: version byte 1.

**Verification:** risk tier: networking send path on the Core (not the audio path),
so tests with the change, integration layer. Commands:
- Build:
  `cmake --build build --target tst_daemon_media_controller tst_display_codec_v2_link tst_display_extras tst_daemon_display_retune`
- Test:
  `ctest --test-dir build -R '^(tst_daemon_media_controller|tst_display_codec_v2_link|tst_display_extras|tst_daemon_display_retune)$' --output-on-failure`
- Also build and run `tst_daemon_display_send_thread` once (REALTIME, wall-clock
  bounds; record the load average if it fails).

**Execution note (advisory):** opus. Networking (the Core's display send path, the
send thread): flag for an earlier review. Prerequisites: Tasks 2 and 3. Can run in
parallel with Task 5 in its own worktree (disjoint files), building only on the
controller's go.

- [ ] **Step 1:** Write the send-order, refusal, pacing and diagnostics cases; see them
  fail.
- [ ] **Step 2:** Choose the encoder by `m_displayCodecVersion` in the receive and
  transmit send paths; send both parts in one turn; record diagnostics; extend the
  line.
- [ ] **Step 3:** Run the verification set; commit.

## Task 5: Window decoding, presenter input and telemetry

**Requirements:** spec sections 7 (client asks; the "`start` carried v2" flag kept
across `replace`; decoders read both), 9.2 (released frames, `traceCarried`), 9.3
(skipped keyframe requests, presenter input, transmit display, mini displays), 11
item 2 (presenter test), 12 (`RemoteMediaController`, `RemoteDisplayPresenter`).

**Files:**
- Modify: `src/gui/RemoteMediaController.h`, `src/gui/RemoteMediaController.cpp`
- Modify: `src/gui/RemoteDisplayPresenter.h`, `src/gui/RemoteDisplayPresenter.cpp`
- Modify: `src/gui/RemoteTelemetryController.cpp`
- Test: `tests/tst_remote_display_presenter.cpp`, `tests/tst_remote_media_controller.cpp`,
  `tests/tst_remote_telemetry.cpp`, `tests/tst_remote_audio_session.cpp` (its exact
  start shape gains the field only where its Core advertises the capability)

**Interfaces:**
- Consumes:
  - `DisplayCodecDisposition::Held`, `DisplayCodecDecodeResult::frameCount` and
    `secondFrame`, `DisplayCodecFrame::traceCarried` and
    `DisplayCodecDecoder::counters()` (Task 2).
  - `StationCapabilities::displayCodecVersion` (Task 3).
- Produces:
  - `void RemoteDisplayPresenter::push(const DisplayCodecFrame& frame, const RemoteSpectrumCapture& capture, bool measurementCurrent = true);`
    It sets that item's `Item::measurementCurrent`; the drawing is unchanged. The
    `(frame, centreHz, spanHz)` overload is unchanged.
  - `RemoteDisplayTelemetry` gains `quint64 messagesLost = 0;` (the sum of the
    decoders' `framesLost`) and `quint64 partsLost = 0;` (the sum of `partsLost`).
    They are accumulated over every binding, retired ones included, the way
    `keyframeWaits` is.
  - The `Remote diagnostics:` line gains `displayMessagesLost=%1` and
    `displayPartsLost=%1`, right after `displayKeyframeRequests=`.

**Acceptance:**
- Start:
  - When `d->client->capabilities().displayCodecVersion >= 2` (with the spectrum grant
    negotiated, as `pictureNegotiated`), the media `start` carries
    `displayCodecVersion` 2.
  - A private flag `displayCodecV2Negotiated` is set beside `pictureNegotiated` and
    cleared only where `txMonitorNegotiated` is cleared in stop (:4217). It is not
    cleared by a peer replacement.
  - Against a Core without the capability, the start carries no field (byte-identical
    to today).
- With the flag set, the window sends no `keyframe` operation for a context accept
  (:8257), a transmit context (:8282) or a re-grant (:8400). Assert with
  `countControl(controls, "keyframe") == 0` through each of those three. With the
  flag clear, each still sends one, as today.
- Also skipped for a binding that has decoded a v2 frame. A binding's
  `NeedKeyframe` (from a v1 frame) still asks.
- `receiveDisplay`:
  - Version 2 is routed like version 1 (endpoint at 8, generation at 12).
  - `Held` returns at once: no loss, no request, no presentation.
  - `Accepted` presents `frameCount` frames in order (`frame`, then `secondFrame`).
    Each goes to the presenter with `measurementCurrent = !traceCarried`. A transmit
    binding emits `transmitFrameReceived` once per frame.
- Presenter test (`tst_remote_display_presenter`):
  - v2 frames from `encodeDisplayCodecV2` through a `DisplayCodecDecoder` into the
    presenter, under the Task 2 loss model at row period 33 ms.
  - Real rows drawn equal rows sent minus rows lost.
  - Exactly one blended row per lost row for bursts up to `kMaxGapRows`.
  - `counters().rowsRepeated == 0` and `counters().keyframeWaits == 0`.
  - A frame pushed with `measurementCurrent = false` comes out of `takeDue` with
    `measurementCurrent == false`, and a following current frame with `true`.
- The mini display check (:8718-8724) passes with released partial frames (full-length
  planes). One test feeds a row-only partial frame and sees the mini display take it.
- `tst_remote_telemetry`: the line contains
  `displayKeyframeRequests=<n> displayMessagesLost=<a> displayPartsLost=<b>` in that
  order.
- Unchanged and passing: `displayRidesAStallAndPlaysOnTheAudioClock`,
  `transmitDisplayBypassesTheAudioClockPresenter`, the presenter's existing cases, and
  `queuedFrameAfterLossKeepsPictureButNotCurrentImd`.

**Verification:** risk tier: the window's display path (no visual change, no audio
path), so tests with the change, unit and integration layers. Commands:
- Build:
  `cmake --build build --target tst_remote_display_presenter tst_remote_media_controller tst_remote_telemetry tst_remote_audio_session`
- Test:
  `ctest --test-dir build -R '^(tst_remote_display_presenter|tst_remote_media_controller|tst_remote_telemetry|tst_remote_audio_session.*)$' --output-on-failure`

`ui-verification`: no pixels change, so no capture is required.

**Execution note (advisory):** opus. Networking (the window's media `start` and
keyframe operations): flag for an earlier review. Prerequisites: Tasks 2 and 3. Can
run in parallel with Task 4 in its own worktree.

- [ ] **Step 1:** Add the presenter overload argument and its cases.
- [ ] **Step 2:** Add the start field and flag, the frame loop, `Held`, `traceCarried`
  and the skipped requests, with their `tst_remote_media_controller` cases.
- [ ] **Step 3:** Add the telemetry fields and line; run the verification set; commit.

## Task 6: End-to-end interop matrix

**Requirements:** spec section 11 item 3 (every interop bullet), 7 (relay, tunnel,
`replace`).

**Files:**
- Modify: `tests/tst_display_codec_v2_link.cpp`, `tests/tst_media_replace.cpp`,
  `tests/tst_media_tunnel.cpp`, `tests/tst_relay_session.cpp`
- Fix code in `src/` only if a case exposes a defect in Tasks 3 to 5 (then name it in
  the report)

**Interfaces:**
- Consumes: everything Tasks 1 to 5 produce.
- Produces: nothing new.

**Acceptance:**
- New Core, client that does not declare `displayCodec`:
  - No `displayCodecVersion` entry; `coreBuildInfo` is last.
  - A `start` carrying the field is refused.
  - Frames have version byte 1; `tst_display_extras` older-peer golden and
    `tst_link_conformance_*` pass byte for byte.
- New Core, v2 window (a `RemoteMediaController` on the Core):
  - `displayCodec` 2 is declared; `displayCodecVersion` 2 comes before `coreBuildInfo`;
    the `start` carries the field.
  - Frames have version byte 2.
  - Zero `keyframe` operations through a context accept, a re-grant and a transmit
    context.
- Old Core (a server with `setDisplayCodecV2Enabled(false)` stands in for a Core with
  no capability, and is checked to send no entry) and a v2 window:
  - No `start` field; v1 frames.
  - After a dropped frame, the window's `NeedKeyframe` path sends one `keyframe`
    operation and recovers, as today.
- Fallback off (`setDisplayCodecV2Enabled(false)`, the value `StationHost` passes for
  `"False"`, whose read Task 3 covers): no capability to a declaring peer, v1 to a v2
  window, which keeps asking for keyframes; a `start` carrying the field is refused.
- Relayed media connection (`tst_relay_session`) and tunneled media connection
  (`tst_media_tunnel`): each `start` agrees v2, and the frames delivered are version 2.
- Handover (`tst_media_replace`, after the pattern of
  `theCoreSendsOnBothThenTheNewPeerTakesOver`):
  - After the `replace`, the replacement's display messages are version 2.
  - `controller.displayCodecVersion()` stays 2.
  - The window sends no `keyframe` operation.
- Golden vectors: `tst_display_codec_v2` reads `tests/data/display_codec_v2/vectors.json`
  in place (already true after Task 2; asserted again here by path).

**Verification:** risk tier: networking interop across versions, integration layer.
Commands:
- Build:
  `cmake --build build --target tst_display_codec_v2_link tst_media_replace tst_media_tunnel tst_relay_session tst_display_extras tst_link_conformance_control tst_link_conformance_session tst_link_conformance_media tst_display_codec_v2`
- Test:
  `ctest --test-dir build -R '^(tst_display_codec_v2_link|tst_media_replace.*|tst_media_tunnel.*|tst_relay_session|tst_display_extras|tst_link_conformance_(control|session|media)|tst_display_codec_v2)$' --output-on-failure`

`tst_media_replace` is REALTIME: run it with the Mac otherwise idle, and record the
load average with any failure.

After this task, the controller runs the repository's full suite once on the finished
lane A revision (`cmake --build build --target all_tests && ctest --test-dir build -j10`),
in its own lane turn.

**Execution note (advisory):** opus. Networking interop: flag for an earlier review.
Prerequisites: Tasks 4 and 5. Not parallel.

- [ ] **Step 1:** Write each interop case; run them; fix any defect found at its
  source, with a test.
- [ ] **Step 2:** Run the verification set; commit.

## Task 7: STRIX bench (hardware evidence, merge gate)

**Requirements:** spec sections 11 item 4 and 13 items 3 and 5 (on by default, merged
only after the STRIX shows at most one made-up row per lost datagram); section 10
(benchmark on the Pi 4 and Rock 5C).

**Files:** none in the repository. The evidence goes in the lane A PR body and the
crew ledger.

**Interfaces:**
- Consumes:
  - A lane A build on the STRIX Core and on JJ's window.
  - The Core's `daemon display diagnostics` line (Task 4 fields, PR 622 fields).
  - The window's `Remote diagnostics:` line (Task 5 fields).
- Produces: a recorded pass or fail, which gates merging lane A.

**Acceptance (pending until observed):**
- Who:
  - The controller with JJ. Every change on the STRIX (install, setting, restart)
    needs JJ's yes in chat; agents only read its logs.
  - The Pi 4 and Rock 5C installs are the Core session's job.
- Runs:
  - Step 1 baseline: lane A Core with `RemoteDisplayCodecV2` `"False"` and
    `DisplayKeyframeEveryFrame` `"True"`, 30 to 45 minutes.
  - Then v2: `RemoteDisplayCodecV2` `"True"` (or absent) and
    `DisplayKeyframeEveryFrame` `"False"`, 30 to 45 minutes.
  - The Core restarts between runs, because both settings are read once at start.
  - Same pan (783 px), same band and window placement as JJ's usual link.
- Measured, from the logs, as the change over the run:
  - Core `keyframeRequests`.
  - Window: `displayKeyframeWaits`, `displayRowsBlended`, `displayRowsRepeated`,
    `displayMessagesLost` and `displayPartsLost`.
  - Core `splitFrames`, `secondPartRefusals` and `meanFrameBytes`.
- Pass, for the v2 run (all must hold):
  1. The Core's `keyframeRequests` delta is 0.
  2. The window's `displayKeyframeWaits` delta is 0.
  3. `(displayRowsBlended + displayRowsRepeated) <= (displayMessagesLost + displayPartsLost)`.
  4. `displayMessagesLost + displayPartsLost >= 20`, so the run saw real loss. If not,
     extend the run once by 30 minutes. If it is still under 20, record the bench as
     inconclusive and ask JJ.
- Baseline record: the Step 1 run has no loss counters, so record its made-up rows per
  minute (`(displayRowsBlended + displayRowsRepeated)` per minute) and its
  `meanFrameBytes`. Compare it with v2's made-up rows per minute and `meanFrameBytes`.
- Human smoke, JJ: during the v2 run the waterfall shows no smear runs after loss
  (single blended rows only). This is pending until JJ says what he saw.
- Pi 4 and Rock 5C: the `QBENCHMARK` figures of `tst_display_codec_v2` from a lane A
  build on each (the Core session runs them). Recorded beside section 10's estimates;
  not a gate.

**Verification:** device layer, hardware evidence; stays pending until observed.

**Execution note (advisory):** no implementer. The controller and JJ; touches a live
Core (reachability), so JJ's yes for each step. Prerequisite: Task 6. Not parallel;
the window build JJ runs is made in the controller's one build lane.

- [ ] **Step 1:** Ask JJ for the STRIX install and the Step 1 settings; run and record
  the baseline.
- [ ] **Step 2:** With JJ's yes, switch the settings, restart the Core, run and record v2.
- [ ] **Step 3:** Write the four checks and the comparison into the lane A PR body;
  mark the gate passed, failed or inconclusive.

## Task 8: Phone v2 decoder (lane B)

**Requirements:** spec sections 8 (a v2 path in `DisplayFrameDecoder`, same golden
vectors), 4.1, 4.2, 9.1, 9.2.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusMedia/DisplayFrameDecoder.swift`,
  `DisplayDecodeResult.swift`, `DisplayDecodeDisposition.swift`, `DisplayFrame.swift`
- Create: `ios/NereusKit/Sources/NereusMedia/DisplayDecodeCounters.swift`
- Create: `ios/NereusKit/Tests/NereusMediaTests/DisplayCodecV2VectorTests.swift`
- Modify: `ios/NereusKit/Tests/NereusMediaTests/DisplayFrameDecoderTests.swift` (only
  where an existing exhaustive check needs the new case)

**Interfaces:**
- Consumes: `tests/data/display_codec_v2/vectors.json` (Task 1, read through
  `LinkFixtureLoader.root` the way `DisplayFftSizingTests` reads `tests/data`).
- Produces:
  - `DisplayDecodeDisposition` gains `case held` (raw value `"held"`).
  - `DisplayDecodeResult` gains `public let frames: [DisplayFrame]`. `frame` stays and
    equals `frames.first`. The existing
    `init(disposition:reason:frame:)` stays, setting `frames` to `frame.map { [$0] } ?? []`.
    A new `init(disposition:reason:frames:)` is added.
  - `DisplayFrame` gains `public let traceCarried: Bool`, with a defaulted `false`
    init parameter so existing call sites compile. `isKeyframe` is `false` for every
    v2 frame.
  - `public struct DisplayDecodeCounters: Equatable, Sendable { framesLost, partsLost, staleMessages, duplicateParts, malformed: UInt64 }`
    and `DisplayFrameDecoder.counters`. They survive `reset()`, as in C++.
  - `DisplayFrameDecoder.decode(_:)` chooses by the version byte, as the C++ decoder.

**Acceptance:**
- Every `planes` entry: the v2 plane decodes to `samples`.
- Every `frames` entry: its messages decode, alone or paired, to `frame`.
- Every `sequences` entry: from a fresh decoder, exactly the stated dispositions,
  reasons, frames (planes compared against `DisplayFrameDecoder.dbm(q, ...)` of the
  stated q) and counters.
- The existing v1 tests in `DisplayFrameDecoderTests` pass unchanged.
- No decode of any `vectors.json` message, truncated at every byte, traps.

**Verification:** risk tier: parser of network input on the phone, so test-first
against the vectors. Unit layer (SwiftPM). Commands:
`ios/scripts/swift-test.sh --jobs 2 --filter DisplayCodecV2VectorTests` and
`ios/scripts/swift-test.sh --jobs 2 --filter DisplayFrameDecoderTests`.

**Execution note (advisory):** opus. Parser of untrusted bytes. Prerequisite: Task 1
(lane B is cut from its commit). Can run in parallel with lane A Tasks 2 to 6 in its
own worktree; its Swift runs take the one build lane like any build.

- [ ] **Step 1:** Write `DisplayCodecV2VectorTests` over the three vector lists; see
  them fail.
- [ ] **Step 2:** Add the types and the v2 path until they pass and the v1 tests are
  unchanged; commit.

## Task 9: Phone negotiation and keyframes on v1 only (lane B)

**Requirements:** spec sections 7 (client declares and asks; the flag kept across a
`replace`), 8 (declare and ask when the Core advertises; keyframe requests for v1
connections only), 9.3.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusLink/LinkFeatures.swift`
- Modify: `ios/NereusKit/Sources/NereusMedia/MediaFeatureGates.swift`
- Modify: `ios/NereusKit/Sources/NereusMedia/MediaControlClient.swift`
- Test: `ios/NereusKit/Tests/NereusMediaTests/MediaControlClientTests.swift`,
  `ios/NereusKit/Tests/NereusLinkTests/LinkConformanceSessionTests.swift` and
  `StationSessionLoopbackTests.swift` (their exact `LinkFeatures.app` expectations gain
  `"displayCodec": 2`)

**Interfaces:**
- Consumes:
  - Task 8's `held`, `frames`, `traceCarried` and counters.
  - Task 3's capability name `displayCodecVersion` (in `surface.json` after lane B
    rebases onto lane A).
- Produces:
  - `LinkFeatures.app` gains `"displayCodec": 2`.
  - `MediaFeatureGates`: `public static let displayCodecMinor: UInt16 = 11` and
    `public var displayCodecV2: Bool`, which is
    `spectrumGrant && agreedMinor >= Self.displayCodecMinor && capabilityVersion("displayCodecVersion") >= 2`.
  - `Media` gains `var displayCodecDeclared = false`. It is set in start when
    `gates.displayCodecV2`, together with `payload["displayCodecVersion"] = .number(2)`,
    and copied to the replacement candidate beside `spectrumPictureDeclared` (:1285).

**Acceptance:**
- Start:
  - A Core advertising `displayCodecVersion` 2 at minor 11 with the spectrum grant gets
    `displayCodecVersion` 2 in the phone's `start`.
  - A Core without it gets today's exact `start`, asserted by the existing start-shape
    tests.
- With `displayCodecDeclared`, no keyframe is requested at context accept (:2310) or
  after a replacement (:1461). Without it, both still request as today.
- `display(_:connection:)`:
  - `.accepted` yields `.displayFrame` once per element of `frames`, in order.
  - `.held` does nothing.
  - `.needKeyframe` still requests (a v1 frame).
- A replacement keeps `displayCodecDeclared`, and the replaced connection requests no
  keyframe.
- `traceCarried` frames reach `BandModel` unchanged. The phone has no IMD measurement
  consumer, so no consumer change is made; `SMeterModel` reads the trace as today.
- App tests (xcodebuild) pass, since `DisplayDecodeDisposition` gained a case.

**Verification:** risk tier: networking on the phone, unit and integration layers.
Commands:
- `ios/scripts/swift-test.sh --jobs 2 --filter MediaControlClientTests`
- `ios/scripts/swift-test.sh --jobs 2 --filter LinkConformanceSessionTests`
- `ios/scripts/swift-test.sh --jobs 2 --filter StationSessionLoopbackTests`
- `ios/scripts/swift-test.sh --jobs 2 --filter LinkSurfaceOrdinalTests`
- Then the app tests: `ios/scripts/generate-project.sh` and the `xcodebuild ... test`
  command in Global Constraints.

Each is its own lane turn.

**Execution note (advisory):** opus. Networking: flag for an earlier review.
Prerequisites: Task 8, and lane B rebased onto lane A after Task 3. Lane B lands as
its own PR (the phone ships on `ios-v` tags), before or after lane A.

- [ ] **Step 1:** Rebase lane B onto lane A's Task 3 commit (or later); confirm the
  117-capability `surface.json`.
- [ ] **Step 2:** Write the start, keyframe and frame-loop cases; implement the gate,
  the declaration, the copy and the loop; update the exact-feature tests.
- [ ] **Step 3:** Run the Swift and app tests; commit.

## Task 10: Remove keyframe every frame

**Requirements:** spec sections 7 (last bullet) and 13 item 6: keep the setting until
the iPhone has v2, then remove it.

**Files:**
- Modify: `src/core/session/media/DaemonMediaController.h`, `.cpp` (remove
  `kDisplayKeyframeEveryFrameSettingsKey`, both `setDisplayKeyframeEveryFrame` /
  `displayKeyframeEveryFrame` pairs (controller and hub), `m_displayKeyframeEveryFrame`
  in both, `DaemonDisplayDiagnostics::displayKeyframeEveryFrame`, and
  `keyframeEveryFrame=` in the line; the two encode call sites go back to
  `encode(frame, forceKeyframe)`)
- Modify: `src/core/station/StationHost.cpp` (remove the read and its log line)
- Modify: `tests/tst_station_host.cpp` (remove `displayKeyframeEveryFrameIsReadAtStart`
  and `_data`), `tests/tst_daemon_media_controller.cpp` (remove
  `keyframeEveryFrameSendsRowsThatDecodeAlone` and `_data`), and any line-format test
  that names `keyframeEveryFrame=`

**Interfaces:**
- Consumes: Task 4's line layout.
- Produces: the line reads
  `... framesSent=%13 keyframesSent=%14 meanFrameBytes=%15 splitFrames=%16 secondPartRefusals=%17 senderGapMaxMs=%18`.

**Acceptance:**
- Kept: `tst_display_budget::keyframeEveryFrameFitsTheAdmittedCostAtEveryPanWidth`
  (it checks the v1 keyframe bound, which still holds), and the `framesSent`,
  `keyframesSent` and `meanFrameBytes` fields.
- `rg -n 'DisplayKeyframeEveryFrame|keyframeEveryFrame' src tests` finds only the kept
  budget test.
- A Core with `DisplayKeyframeEveryFrame` `"True"` left in its settings file starts
  normally and ignores it (a stale key is harmless).
- v1 connections keep the 120-frame keyframe interval and keyframe requests;
  `tst_display_codec` and `tst_display_extras` pass.

**Verification:** risk tier: removal of a bench setting, unit and integration layers.
Commands:
- Build:
  `cmake --build build --target tst_daemon_media_controller tst_station_host tst_display_budget tst_display_extras tst_display_codec`
- Test:
  `ctest --test-dir build -R '^(tst_daemon_media_controller|tst_station_host|tst_display_budget|tst_display_extras|tst_display_codec)$' --output-on-failure`

**Execution note (advisory):** opus. No networking change. Prerequisites: lane A
merged after Task 7 passed, and lane B (Tasks 8 and 9) landed. A separate branch from
main at that point. Not parallel.

- [ ] **Step 1:** Remove the setting, its reads, fields and tests; run the
  verification set; commit.

## Notes from the final review (2026-10-23)

- Malformed counts for the shared rejects: the rejects decided before the version
  byte chooses a path (oversized, bad magic, an unsupported version, a datagram cut
  inside the magic or the version) count `malformed` in the decoder's counters for a
  v1 datagram as well as a v2 one (`DisplayCodecDecoder::decode`). This is what spec
  section 9.2 says and what the phone's decoder does, so the code stays as it is. The
  v1 path's own rejects still count nothing.
- Float context fields: both decoders compare `minDbm` and `maxDbm` by value, so
  `-0.0` equals `+0.0`, and a NaN interval end is malformed before any comparison.
  The golden vectors carry `context-floats-compare-by-value`, `malformed-nan-min-dbm`
  and `malformed-nan-max-dbm`.
