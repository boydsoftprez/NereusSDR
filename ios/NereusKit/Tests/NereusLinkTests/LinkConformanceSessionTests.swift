// NereusSDR for iOS: the link's session fixtures, played against the app's session in the client's role
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import Testing
@testable import NereusLink

/// Link document section 16.3, an app's runner (`SessionFixturePlayer`),
/// with the session's link layer as the client. It runs every fixture whose
/// `runs` names "app".
@Suite struct LinkConformanceSessionTests {
    typealias SessionFixture = LinkSessionTestSupport.SessionFixture

    static func appFixtures() throws -> [SessionFixture] {
        try SessionFixtures.forApp()
    }

    static func mirrorClasses() throws -> [String: [[String: Any]]] {
        try SessionFixtures.mirrorClasses()
    }

    static func linkMajors() throws -> [UInt16] {
        try SessionFixtures.linkMajors()
    }

    @Test func theAppRunsTheFixturesMarkedForIt() throws {
        let fixtures = try Self.appFixtures()
        #expect(fixtures.count == 85)
        #expect(Set(fixtures.map(\.id)).count == fixtures.count)
        for id in Self.diversityProducerSessions {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(!fixtures.contains { $0.id == "session-radio-mic-source" })
        for id in Self.diversityProducerSessions {
            let fixture = try #require(fixtures.first { $0.id == id })
            var expected = ["deviceAuth": 1, "diversityControl": 1, "sessionHolder": 1, "sliceAccess": 1]
            if id == "session-diversity-control-pattern" { expected["diversityPattern"] = 1 }
            #expect(fixture.declaredFeatures == expected, "\(id)")
            #expect(fixture.signsInWithDevice, "\(id)")
        }
        // MON's route on the media connection (Core trunk 4582efc90): the
        // client makes its media connection id (`"$uuid:<name>"`), and a
        // start without headphonesMixVersion, as the phone's, gets
        // headphones answered as speakers. The undeclared case is the
        // station's alone: its monitor-audio is scripted.
        #expect(fixtures.contains { $0.id == "session-monitor-audio" })
        #expect(!fixtures.contains { $0.id == "session-monitor-audio-undeclared" })
        // A second ADC's attenuation per slice, now the app declares adcAttenuators 1.
        #expect(fixtures.contains { $0.id == "session-adc-attenuators" })
        // The CFC band editor (cfcProfile 1): the app reads transmit.cfcProfile
        // and sends cfc.setProfile.
        #expect(fixtures.contains { $0.id == "session-cfc-set-profile" })
        // The TX EQ curve (txEqCurveVersion 1, Core checkpoint 338a895ac, read; 2, changed through
        // txEq.setCurve and txEq.resetCurve): the app declares txEqCurve 2.
        #expect(fixtures.contains { $0.id == "session-tx-eq-curve" })
        #expect(fixtures.contains { $0.id == "session-tx-eq-set-curve" })
        // The band plan the Core follows (the catalogue's active plan and
        // spots), record streams with the Core's spots, and the Core's radio
        // from a device, from the trunk at 4f8ec22c.
        for id in ["session-settings-band-plan", "session-verbs-records", "session-verbs-spots"]
            + Self.stationRadios {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        // The HL2 I/O board (radioHardwareVersion 7) and the DSP facts
        // (dspInfoVersion 1).
        for id in ["session-verbs-io-board", "session-verbs-dsp-info"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        // These accepted Core cases declare only features the app already has,
        // and each is played by both session transports below.
        // The Core's own TCI server settings (stationTciSettingsVersion 1,
        // setStationTciSettings), now the app declares stationTciSettings 1.
        #expect(fixtures.contains { $0.id == "session-verbs-station-tci-settings" })
        for id in ["session-verbs-station-tci-server", "session-verbs-accessory-tx",
                   "session-verbs-support", "session-verbs-freedv", "session-verbs-tx-mod-monitor"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        // Band select and the notch the Core places at a slice (bandSelectVersion 1,
        // notchControlVersion 2), and the accessory, antenna and PureSignal verbs
        // that came with them.
        for id in ["session-verbs-band-select", "session-verbs-notch-at-slice", "session-verbs-pgxl-control",
                   "session-verbs-rfkit-control", "session-verbs-tgxl-relays", "session-verbs-tx-antenna",
                   "session-verbs-ps3-arming"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(fixtures.contains { $0.id == "session-connect-connectable" })
        // The Settings Validation panel (settingsHygiene 2) and a Setup
        // description at version 1, run once the app declares them.
        for id in ["session-verbs-settings-hygiene", "session-settings-hygiene-token", "session-verbs-two-tone-preset"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(fixtures.contains { $0.id == "session-verbs-transmit-settings" })
        #expect(fixtures.contains { $0.id == "session-verbs-tx-profiles" })
        #expect(fixtures.contains { $0.id == "session-verbs-pa-profiles" })
        #expect(fixtures.contains { $0.id == "session-settings-write" })
        #expect(fixtures.contains { $0.id == "session-catalog-anan-g2" })
        #expect(fixtures.contains { $0.id == "session-catalog-hermes-lite-2" })
        #expect(fixtures.contains { $0.id == "session-wrong-token" })
        #expect(fixtures.contains { $0.id == "session-connection-limit" })
        #expect(fixtures.contains { $0.id == "session-unknown-kind" })
        // The device sign-ins the app is the subject of run on both ends;
        // the failing proofs are the station's alone.
        for id in ["session-device-sign-in", "session-device-not-paired", "session-pairing-required"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        for id in ["session-device-other-challenge", "session-device-other-certificate", "session-devices"] {
            #expect(!fixtures.contains { $0.id == id }, "\(id)")
        }
        // Besides the two device sign-ins, every several-devices, remote
        // transmit and station radio fixture, and the band plan's, signs in
        // as a paired device (ruling 10.1; a token session may not transmit,
        // section 18.1).
        #expect(fixtures.filter(\.signsInWithDevice).map(\.id).sorted()
                == (["session-device-not-paired", "session-device-sign-in", "session-settings-band-plan",
                     "session-verbs-settings-hygiene"]
                    + Self.severalDevices + Self.remoteTransmit + Self.stationRadios + Self.diversityProducerSessions).sorted())
        for major in try Self.linkMajors() {
            #expect(LinkVersionPolicy.supportedMajors.contains(major), "the suite covers major \(major)")
        }
    }

    /// Only these two retained producer transcripts reference the actual device key.
    static let diversityProducerSessions = ["session-diversity-control-pattern", "session-diversity-control-no-pattern"]

    /// The several-devices fixtures an app's runner plays.
    static let severalDevices = [
        "session-anchor-band-change", "session-connected-devices", "session-foreign-write-refused",
        "session-grace-expired", "session-grace-return", "session-held-for-device",
        "session-non-anchor-pan-move", "session-share-receiver", "session-shared-setting-confirm",
        "session-short-name", "session-take-receiver", "session-two-devices",
    ]

    /// The Core's radio chosen from a device: the Core refuses these verbs
    /// to a pairing-token sign-in (link 9.1).
    static let stationRadios = ["session-station-radio-confirm", "session-verbs-station-radios"]

    /// The remote transmit fixtures the app's side runs, whose client
    /// declares `remoteTx`, which the app declares from Task 54.
    static let remoteTransmit = [
        "session-grace-transmit-held", "session-key-without-microphone", "session-tx-keepalive",
        "session-unheld-key",
        // Taking transmit between devices (remoteTxVersion 2, the trunk at 6c3f543d).
        "session-take-during-grace", "session-take-transmit", "session-take-transmit-keyed",
        "session-tx-mark", "session-unheld-key-carriers",
    ]

    /// The fixtures marked for the app whose client declares a feature the
    /// app does not: each needs phone behaviour or a control the app does
    /// not have.
    // radioMic 1 reads the board; version 2 requires the phone's source-selection work.
    static let waitingOnNewPhoneWork = ["session-radio-mic-source"]

    /// Every fixture marked for the app runs: the several-devices ones
    /// (`sessionHolder`, Task 56c) and the remote transmit ones (`remoteTx`,
    /// Task 54). Only the fixtures named in ``waitingOnNewPhoneWork`` wait
    /// for a feature the app lacks, so a new one waiting fails here.
    @Test func everyFixtureMarkedForTheAppRuns() throws {
        #expect(try SessionFixtures.waitingForAFeature().map(\.id).sorted() == Self.waitingOnNewPhoneWork)
        let ids = Set(try Self.appFixtures().map(\.id))
        for id in Self.severalDevices + Self.remoteTransmit {
            #expect(ids.contains(id), "\(id)")
        }
        #expect(LinkFeatures.app == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                     "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1,
                                  "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
    }

    /// JSON inside a string (section 16.1): an app's runner sends the
    /// expectation, filled, as its compact text; `"$json"` beside another
    /// key is a malformed fixture.
    @Test func jsonInsideAStringIsSentAsItsText() throws {
        var placeholders = FixturePlaceholders(mirrorClasses: [:], certificateSHA256: Data(count: 32))
        let expectation = LinkJSON.array([.object(["name": .string("$string"), "seconds": .string("$int"),
                                                   "state": .string("listening")])])
        let message = LinkJSON.object(["type": .string("delta"), "key": .string("connectedDevices"),
                                       "properties": .array([.object(["value": .object(["$json": expectation])])])])
        let filled = try placeholders.fillStation(message, at: "test")
        guard case .object(let object) = filled, case .array(let entries)? = object["properties"],
              case .object(let entry)? = entries.first, case .string(let text)? = entry["value"] else {
            Issue.record("the string was not filled")
            return
        }
        #expect(!text.contains(" "))
        #expect(try LinkJSON.parse(text) == .array([.object(["name": .string(""), "seconds": .number(0),
                                                             "state": .string("listening")])]))
        let beside = LinkJSON.object(["type": .string("delta"),
                                      "value": .object(["$json": expectation, "other": .number(1)])])
        #expect(throws: LinkFixtureLoader.Malformed.self) { try placeholders.fillStation(beside, at: "test") }
    }

    /// A media connection id the client makes (section 16.1, `"$uuid:<name>"`):
    /// each fill is a new canonical lower-case UUID, and matching records the
    /// one the client sent under its name for the `"$ref:<name>"` after it.
    @Test func aMediaConnectionIdIsFilledLowerCaseAndRecordedAtMatch() throws {
        var placeholders = FixturePlaceholders(mirrorClasses: [:], certificateSHA256: Data(count: 32))
        let template = LinkJSON.object(["type": .string("media.control"),
                                        "payload": .object(["op": .string("start"),
                                                            "connectionId": .string("$uuid:media1")])])
        var counter = 1
        let first = try placeholders.fillClient(template, counter: &counter, origin: "test", at: "test")
        let second = try placeholders.fillClient(template, counter: &counter, origin: "test", at: "test")
        func id(_ message: LinkJSON) -> String? {
            guard case .object(let object) = message, case .object(let payload)? = object["payload"],
                  case .string(let id)? = payload["connectionId"] else {
                return nil
            }
            return id
        }
        let filled = try #require(id(first))
        #expect(FixturePlaceholders.isCanonicalUUID(filled))
        #expect(filled == filled.lowercased() && UUID(uuidString: filled) != nil)
        #expect(id(second) != filled)
        #expect(counter == 1)
        #expect(placeholders.record["media1"] == nil)

        #expect(try placeholders.matchClient(template, first, at: "test") == nil)
        #expect(placeholders.record["media1"] == .string(filled))
        let after = LinkJSON.object(["type": .string("media.control"),
                                     "payload": .object(["op": .string("stop"),
                                                         "connectionId": .string("$ref:media1")])])
        #expect(try placeholders.matchClient(after, try placeholders.fillClient(after, counter: &counter,
                                                                                origin: "test", at: "test"),
                                             at: "test") == nil)

        // An upper-case or braced id is not the canonical form.
        for wrong in [filled.uppercased(), "{\(filled)}", "not-a-uuid"] {
            var fresh = FixturePlaceholders(mirrorClasses: [:], certificateSHA256: Data(count: 32))
            let sent = LinkJSON.object(["type": .string("media.control"),
                                        "payload": .object(["op": .string("start"),
                                                            "connectionId": .string(wrong)])])
            #expect(try fresh.matchClient(template, sent, at: "test") != nil, "\(wrong)")
            #expect(fresh.record["media1"] == nil)
        }
    }

    /// The original PA transcript has nine behavioural no-bank refusals
    /// and nine scripted malformed-request refusals. Play it unchanged
    /// through the same session runner used by the exhaustive app suites.
    @Test func paProfileFixturePassesOverBothSessionTransports() async throws {
        let fixture = try #require(Self.appFixtures().first { $0.id == "session-verbs-pa-profiles" })
        #expect(fixture.declaredFeatures == ["paProfiles": 1])
        let classes = try Self.mirrorClasses()
        for _ in try Self.linkMajors() {
            for mode in [SessionFixturePlayer.TransportMode.webSocket, .dataChannel] {
                let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                                              transportMode: mode).play()
                for failure in failures {
                    Issue.record("\(fixture.id) (\(mode)): \(failure)")
                }
            }
        }
    }

    @Test func everyAppFixturePasses() async throws {
        let classes = try Self.mirrorClasses()
        for _ in try Self.linkMajors() {
            for fixture in try Self.appFixtures() {
                let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                    producerDeviceIdentityReferences: Self.diversityProducerSessions.contains(fixture.id)).play()
                for failure in failures {
                    Issue.record("\(fixture.id): \(failure)")
                }
            }
        }
    }

    /// The same fixtures with the session over the control data channel
    /// (link document section 20, iPhone app plan Task 28a): every message
    /// the Core sends crosses as chunks, what the app sends is joined under
    /// the Core's 1 MiB cap, and the heartbeat is 5-byte pings and pongs.
    /// Nothing the session does may differ.
    @Test func everyAppFixturePassesOverTheControlDataChannel() async throws {
        let classes = try Self.mirrorClasses()
        for _ in try Self.linkMajors() {
            for fixture in try Self.appFixtures() {
                let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                                              transportMode: .dataChannel,
                    producerDeviceIdentityReferences: Self.diversityProducerSessions.contains(fixture.id)).play()
                for failure in failures {
                    Issue.record("\(fixture.id) (data channel): \(failure)")
                }
            }
        }
    }

    /// Every app fixture's client hello requires `majors` and `features`,
    /// and the app's hello meets it.
    @Test func theClientHelloDeclaresMajorsAndFeatures() throws {
        for fixture in try Self.appFixtures() {
            let hellos = fixture.steps.filter { step in
                (step["from"] as? String) == "client" && ((step["message"] as? [String: Any])?["type"] as? String) == "hello"
            }
            for step in hellos {
                let message = step["message"] as? [String: Any]
                #expect(message?["majors"] as? String == "$majors", "\(fixture.id)")
                // A fixture where the client shares the Core names its
                // features (section 16.1), each one the app declares, and the
                // runner's session declares exactly those; the rest take any
                // object. A fixture may name an earlier Setup description or
                // TX EQ curve version than the app's: the app reads every
                // version from 1 (a Core at txEqCurve 1 keeps the curve read-only).
                if let features = message?["features"] as? [String: Int] {
                    #expect(features == fixture.declaredFeatures, "\(fixture.id)")
                    for (name, version) in features {
                        if name == "sliceAccess" && Self.diversityProducerSessions.contains(fixture.id) {
                            // These producers pin the version-1 form; the phone also handles 2 and 3.
                            #expect(version == 1, "\(fixture.id): \(name)")
                            #expect(LinkFeatures.app[name] == 3, "\(fixture.id): \(name)")
                        } else if name == "setupDescription" || name == "txEqCurve" {
                            #expect((LinkFeatures.app[name] ?? 0) >= version, "\(fixture.id): \(name)")
                        } else {
                            #expect(LinkFeatures.app[name] == version, "\(fixture.id): \(name)")
                        }
                    }
                } else {
                    #expect(message?["features"] as? String == "$object", "\(fixture.id)")
                }
            }
        }
    }

    /// The runner notices a difference and names the step: a fixture
    /// altered in memory to expect another minor fails at the client hello.
    @Test func anAlteredFixtureFailsAtItsStep() async throws {
        guard var fixture = try Self.appFixtures().first(where: { $0.id == "session-unknown-kind" }) else {
            Issue.record("no unknown-kind fixture")
            return
        }
        var steps = fixture.steps
        var hello = steps[1]["message"] as? [String: Any] ?? [:]
        hello["minor"] = 10
        steps[1]["message"] = hello
        fixture = SessionFixture(id: fixture.id, runs: fixture.runs, steps: steps)
        let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: try Self.mirrorClasses()).play()
        #expect(failures.first?.hasPrefix("step 1:") == true, "\(failures)")
    }
}
