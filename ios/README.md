# NereusSDR for iPhone and iPad

A native Swift app that runs your NereusSDR Core from an iPhone or iPad: the
band, the VFO flags, receive audio and transmit. The phone does no radio signal
processing of its own. The Core (`nereusd`) runs the radio and the DSP; the app
is a thin client that shows what the Core sends and asks it to change things.

The design is `docs/architecture/2026-09-23-iphone-app-design.md` and the work
is planned in `docs/architecture/2026-09-23-iphone-app-plan.md`.

## Licence

Everything here is NereusSDR-original and licensed under the GNU GPL version 3
or later, with an additional permission that allows distribution through the
App Store and TestFlight. The full text, with that permission at the end, is in
[`LICENSE`](LICENSE).

No code, data tables or text from Thetis, WDSP or AetherSDR may enter this
directory: their authors have not granted the App Store permission. Anything
the desktop computes with ported code is computed by the Core and reaches the
app as data. Bundled third-party code must be BSD, MIT, ISC, Apache-2.0 or
MPL-2.0 and gets a row in [`THIRD-PARTY.md`](THIRD-PARTY.md).

Every source file (`.swift`, `.c`, `.h`, `.m`, `.metal`, `.cpp`) starts with
these two lines:

```swift
// NereusSDR for iOS: <what this file is for>
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
```

`Package.swift` keeps its `// swift-tools-version` line first, with the two
header lines right after it.

`python3 scripts/verify-ios-provenance.py`, run from the repository root and in
CI, checks the headers, the upstream names, the `THIRD-PARTY.md` rows, and that
nothing outside the tests reaches for the conformance fixtures.

## Module map

The Swift package lives in `NereusKit/`. Modules are added as the plan reaches
them.

| Module | What it holds |
| --- | --- |
| `NereusModels` | Plain value types shared by every module, such as `Frequency` |
| `NereusLink` | The TLS session to the Core: handshake, versions, reconnect |
| `NereusMirror` | The Core's snapshot and its live updates |
| `NereusMedia` | Opus in and out, the jitter buffer, the display-frame decoder, the media peer to the Core (`MediaPeer`), media control (`MediaControlClient`), the display-quality allocator (`DisplayQualityAllocator`) and audio playback (`AudioPlaybackCore`, over the lock-free ring of the small C target `CAudioRing`) |
| `NereusKitTesting` | `FakeStation`, a Core in the same process played from the link's conformance suite, for the app's tests; never linked into the app |
| `NereusBand` | The band in Metal (`BandRenderer`): the spectrum trace, the waterfall, the band-plan strip, the dBm and frequency scales and the Core's display extras, with each pan's own display settings (`BandDisplaySettings`); its shaders ship as source and compile at run time. The flags, markers and gestures join it later |
| `NereusApp` (app target) | The SwiftUI screens: tabs, Setup, sheets |

The platform pieces (audio session, Live Activity, Push to Talk, the Action
button, Bluetooth PTT and MIDI, Local Network) sit in the app target.

## The app

`project.yml` describes the Xcode project for XcodeGen (`brew install
xcodegen`); the project itself is generated, never committed:

```sh
ios/scripts/generate-project.sh
xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR \
    -destination 'platform=iOS Simulator,name=iPhone 17' \
    -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test \
    -skip-testing:NereusSDRTests/ScrollViewIdentifierTests
ios/scripts/sim-test.sh "iPhone 17" "<derived data folder>" \
    -only-testing:NereusSDRTests/ScrollViewIdentifierTests
```

`ScrollViewIdentifierTests` runs alone, as CI's first test step does: it only
catches a regression when its walk is the first to turn application
accessibility on, and that switch belongs to the simulator, so in a shared run
(or on a simulator a run left it on) it cancels instead. It must report one
test passed; a cancel is not a pass.

The scheme's tests are the app's own (`NereusApp/Tests`, `NereusApp/UITests`)
and NereusKit's, run on the simulator. On a busy Mac, build with
`build-for-testing` into a derived data folder, then run the tests with
`ios/scripts/sim-test.sh "<simulator name>" "<derived data folder>"
-only-testing:<target>/<suite>`. It runs `ios/scripts/sim-ready.sh` with the
app just built, which waits until the simulator has really finished booting
(`simctl bootstatus -b` can return early while it is still migrating data),
installs the app, launches it until one launch succeeds and waits for that
process to exit. Then it runs `test-without-building`, and when that fails
(a test, or "The test runner hung before establishing connection") it keeps
the result bundle and the simulator's logs for testmanagerd, CoreSimulator
and the app before Xcode prunes them. Run `sim-ready.sh` on its own only with
the built app's path: without one, the first launch of the new install
happens inside xcodebuild, with no retry, which is how the runner hung. A UI
test that asks for the band and finds Welcome fails at once and says so; the
app's log then names the arguments each of its processes started with. Where the simulator has never
booted, as on CI's runner, `ios/scripts/sim-settle.sh "<simulator name>"`
goes between the two instead: it boots the simulator and waits for the Mac to
go quiet, because a first boot keeps a small Mac busy for a quarter of an
hour and tests run through it are slow enough for their waits to run out. In
a debug build the app, when it
hosts the unit tests, starts with an empty scene: no Keychain, no looking for
Cores, no sound. Building needs Xcode 27. The app's Metal
shaders ship as source (`NereusApp/Resources/Shaders`) and compile at run
time, so no Metal toolchain is needed. `NereusApp/Resources/Licenses.json`
holds every `THIRD-PARTY.md` library's notice for the licences screen (Setup,
Diagnostics, Licences); the provenance check keeps the two in step.

When sharing this Mac with another build or crew lane, reserve
`/tmp/nereus-ios-full.lock` with `mkdir` before building or booting a simulator.
Record the owner's token, release only that owner's lock in an exit trap, and
never remove another owner's lock. Use one simulator at a time and jobs 2.
Shut down the owned simulator before non-app builds or releasing the slot;
delete temporary test devices on exit. A healthy run keeps its slot until it
finishes. Reuse its derived data for focused checks rather than rebuilding
from an empty cache.

`ios/scripts/archive.sh` makes the Release archive for TestFlight and the App
Store, from phone main only: its build number is the commit count of HEAD
plus 1000 (`build-number.sh`), its version is `project.yml`'s, and its name comes from
`build-tag.sh`. It refuses uncommitted changes to tracked files and a build
name that does not name HEAD, so every archive is a commit. It writes `ios/.build/archive/NereusSDR <version> (<build>).xcarchive`
and stops if the archived app has no privacy manifest. It signs with the
team’s certificate, so only the controller or JJ runs it.

Upload the archive with Organizer's **App Store Connect** choice, or with
`xcodebuild -exportArchive -exportOptionsPlist ios/Config/ExportOptions.plist`.
Never choose **TestFlight Internal Only**: App Store Connect keeps such a build
out of external testing groups, the public TestFlight link and App Store review
for good, and the only way back is a new archive from a newer commit.

## Testing

```sh
ios/scripts/swift-test.sh --jobs 2             # every NereusKit test
ios/scripts/swift-test.sh --jobs 2 --filter FrequencyTests
```

The script runs `swift test` in `NereusKit/` and passes any arguments through.
With Xcode selected it runs as is. With only the command line tools selected
(`xcode-select -p` prints `/Library/Developer/CommandLineTools`) it adds the
framework search path and rpaths that swift-testing needs there. Tests use
swift-testing (`import Testing`) and the Swift 6 language mode.

```sh
ios/scripts/interop-test.sh                    # the media peer against the Core's transport
ios/scripts/retirement-probe.sh                 # isolated TURN/DNS retirement and global cleanup
```

The interop tests connect the app's `MediaPeer` to the Core's own media
transport over 127.0.0.1. The script builds the station helper
`nereus_media_offerer` (`tests/tools/`) in the station build directory
(`build/`, or `$NEREUS_STATION_BUILD`), which must already be configured with
`-DNEREUS_BUILD_TESTS=ON`, then runs `MediaPeerInteropTests` with
`NEREUS_MEDIA_OFFERER` set. Without that variable `swift-test.sh` skips them.
One of them runs the helper with `--media-control`, where it plays the Core's
side of media control with the Core's own `MediaPeer`, for the sound-only case
driven through `MediaControlClient`.

The retirement probe builds the pinned macOS `CDataChannel` target, then runs
its `IceTransport` and process-global cleanup in a separate process. Run it
alongside package and interop tests when changing libjuice close ownership or
libdatachannel teardown. It checks the retained agent's sole initialization
token, a live thread-pool continuation during delayed DNS, and cleanup after
the resolver exits. It is separate because global cleanup cannot safely run
inside the parallel Swift test suites.

The vendored libraries (`COpus`, and `CDataChannel`, `CJuice`, `CSrtp`,
`CUsrsctp`, `CPlog` and `CMbedTLS` for the media peer) are written by
`ios/scripts/vendor-sources.sh` and checked against their pinned archives by
`swift-test.sh` before every run.

### Accessory meter display

The app and Live Activity share `NereusModels.TransmitMeterRule` and
`MeterReadout`. A present amplifier in OPERATE supplies the main forward-power
and SWR meters, with Power Genius taking precedence over RF-Kit. The display
runs from 0 to 2000 W with a 1500 W legal-limit marker. Neither value sets an
amplifier rating or changes a control, and a reported 2300 W stays visible as
2300 W. The transmit panel retains the radio's drive beside that meter while
keyed. Accessory pages show forward, reflected, SWR and temperature readings;
idle power readings use dashes while temperature stays live.
