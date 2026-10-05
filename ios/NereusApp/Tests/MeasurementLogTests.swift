// NereusSDR for iOS: tests of the measurement log: a row a minute, its conditions, the file and the support bundle
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// Plan Task 68 step 1 (R-IOS-23, R-IOS-22, R-IOS-10; spec section 8): the
/// log keeps, per minute of a session, the bytes in and out by kind, the
/// battery level, the heat and the band's request, on this phone, and the
/// support bundle carries it. Every time comes from a clock the test moves.
@Suite("Measurement log", .serialized)
@MainActor
struct MeasurementLogTests {
    /// A clock the test moves.
    final class Clock {
        var now: Date

        init(_ now: Date) {
            self.now = now
        }
    }

    /// Traffic by kind the test adds.
    final class Traffic {
        var reading = TrafficCounter.ByKind()

        func add(_ kind: TrafficCounter.Kind, in bytesIn: UInt64, out bytesOut: UInt64 = 0) {
            reading[kind].bytesIn += bytesIn
            reading[kind].bytesOut += bytesOut
        }

        var totals: TrafficCounter.Totals { reading.sum }
    }

    /// 2026-09-27 19:42 UTC.
    static let start = Date(timeIntervalSince1970: 1_790_538_120)

    static let wifiFull = MeasurementLog.Conditions(network: "Wi-Fi", inFront: true, mode: .full,
                                                    request: SessionPolicy.Mode.full.request)
    static let cool = MeasurementLog.Power(batteryPercent: 81, charging: false, heat: .nominal)

    private static func temporaryFile() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("MeasurementLogTests-\(UUID().uuidString)", isDirectory: true)
            .appendingPathComponent("measurements.csv")
    }

    @Test("a row a minute: bytes in and out by kind, battery, charging, the heat and the band's request")
    func rowAMinute() {
        let clock = Clock(Self.start)
        let traffic = Traffic()
        traffic.add(.control, in: 999, out: 999)
        let log = MeasurementLog(file: nil, now: { clock.now }, traffic: { traffic.reading })
        log.begin(Self.wifiFull, power: Self.cool)
        traffic.add(.control, in: 12_000, out: 3_000)
        traffic.add(.display, in: 400_000, out: 0)
        traffic.add(.audio, in: 180_000, out: 0)
        traffic.add(.transmit, in: 0, out: 50)
        traffic.add(.other, in: 7, out: 0)
        clock.now.addTimeInterval(30)
        log.sample(Self.wifiFull, power: Self.cool)
        #expect(log.rows.isEmpty)
        clock.now.addTimeInterval(30)
        log.sample(Self.wifiFull, power: MeasurementLog.Power(batteryPercent: 80, charging: false, heat: .nominal))
        #expect(log.rows == [
            "2026-09-27T19:42:00Z,60.0,Wi-Fi,In front,Full,30,full,80,no,nominal,"
                + "12000,3000,400000,0,180000,0,0,50,7,0",
        ])
        // The next row starts where this one ended.
        traffic.add(.display, in: 1_000, out: 0)
        clock.now.addTimeInterval(65)
        log.sample(Self.wifiFull, power: MeasurementLog.Power(batteryPercent: nil, charging: true, heat: .fair))
        #expect(log.rows.last == "2026-09-27T19:43:00Z,65.0,Wi-Fi,In front,Full,30,full,unknown,yes,fair,"
            + "0,0,1000,0,0,0,0,0,0,0")
        #expect(MeasurementLog.columns.joined(separator: ",") ==
            "start,seconds,network,app,mode,band_frames_a_second,band_detail,battery_percent,charging,heat,"
            + "control_in,control_out,display_in,display_out,audio_in,audio_out,transmit_in,transmit_out,"
            + "other_in,other_out")
    }

    @Test("a row ends early when the network, the request or the app's place changes, and keeps the hottest heat")
    func rowEndsOnChange() {
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let log = MeasurementLog(file: nil, now: { clock.now }, traffic: { traffic.reading })
        log.begin(Self.wifiFull, power: Self.cool)
        clock.now.addTimeInterval(10)
        log.sample(Self.wifiFull, power: MeasurementLog.Power(batteryPercent: 81, charging: false, heat: .serious))
        clock.now.addTimeInterval(10)
        let cellular = MeasurementLog.Conditions(network: "Cellular", inFront: true, mode: .balanced,
                                                 request: SessionPolicy.Mode.balanced.request)
        traffic.add(.display, in: 2_000)
        log.sample(cellular, power: Self.cool)
        #expect(log.rows == ["2026-09-27T19:42:00Z,20.0,Wi-Fi,In front,Full,30,full,81,no,serious,"
            + "0,0,2000,0,0,0,0,0,0,0"])

        // Locked with sound only: no band asked for.
        clock.now.addTimeInterval(5)
        let away = MeasurementLog.Conditions(network: "Cellular", inFront: false, mode: .balanced, request: .none)
        log.sample(away, power: Self.cool)
        #expect(log.rows.last == "2026-09-27T19:42:20Z,5.0,Cellular,In front,Balanced,15,full,81,no,nominal,"
            + "0,0,0,0,0,0,0,0,0,0")
        clock.now.addTimeInterval(40)
        let saver = MeasurementLog.Conditions(network: "Cellular", inFront: false, mode: .saver,
                                              request: SessionPolicy.Mode.saver.request)
        traffic.add(.audio, in: 120_000)
        traffic.add(.control, in: 3_000)
        log.sample(saver, power: Self.cool)
        #expect(log.rows.last == "2026-09-27T19:42:25Z,40.0,Cellular,Away,Balanced,0,none,81,no,nominal,"
            + "3000,0,0,0,120000,0,0,0,0,0")

        // A change at the very moment a row opened takes the new conditions as its own.
        log.sample(Self.wifiFull, power: Self.cool)
        #expect(log.rows.count == 3)
        clock.now.addTimeInterval(60)
        log.sample(Self.wifiFull, power: Self.cool)
        #expect(log.rows.last?.hasPrefix("2026-09-27T19:43:05Z,60.0,Wi-Fi,In front,Full,30,full,") == true)
        #expect(log.rows.count == 4)
    }

    @Test("the session's end closes its last row; nothing is kept outside a session")
    func sessionEnds() {
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let log = MeasurementLog(file: nil, now: { clock.now }, traffic: { traffic.reading })
        clock.now.addTimeInterval(120)
        traffic.add(.display, in: 5_000)
        log.sample(Self.wifiFull, power: Self.cool)
        log.end(power: Self.cool)
        #expect(log.rows.isEmpty && !log.isRecording)

        log.begin(Self.wifiFull, power: Self.cool)
        #expect(log.isRecording)
        clock.now.addTimeInterval(12.5)
        traffic.add(.audio, in: 30_000)
        log.end(power: MeasurementLog.Power(batteryPercent: 79, charging: false, heat: .critical))
        #expect(log.rows == ["2026-09-27T19:44:00Z,12.5,Wi-Fi,In front,Full,30,full,79,no,critical,"
            + "0,0,0,0,30000,0,0,0,0,0"])
        #expect(!log.isRecording)
        clock.now.addTimeInterval(300)
        traffic.add(.audio, in: 30_000)
        log.sample(Self.wifiFull, power: Self.cool)
        #expect(log.rows.count == 1)
        #expect(log.bundleLines() == [MeasurementLog.heading, MeasurementLog.columns.joined(separator: ","),
                                      log.rows[0]])
        #expect(MeasurementLog(file: nil, now: { clock.now }, traffic: { traffic.reading }).bundleLines()
            == [MeasurementLog.heading, MeasurementLog.noRows])
    }

    @Test("the rows are kept in a file the next start reads, and past the most rows the oldest go first")
    func keptOnThePhone() throws {
        let file = Self.temporaryFile()
        defer { try? FileManager.default.removeItem(at: file.deletingLastPathComponent()) }
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let log = MeasurementLog(file: file, now: { clock.now }, traffic: { traffic.reading })
        log.begin(Self.wifiFull, power: Self.cool)
        clock.now.addTimeInterval(60)
        log.sample(Self.wifiFull, power: Self.cool)
        clock.now.addTimeInterval(30)
        log.end(power: Self.cool)
        #expect(log.rows.count == 2)
        #expect(try String(contentsOf: file, encoding: .utf8) == log.rows.joined(separator: "\n") + "\n")
        let again = MeasurementLog(file: file, now: { clock.now }, traffic: { traffic.reading })
        #expect(again.rows == log.rows)

        // A full log: one more row drops a day's oldest rows at once.
        let full = (0..<MeasurementLog.mostRows).map { "row \($0)" }
        try Data((full.joined(separator: "\n") + "\n").utf8).write(to: file)
        let busy = MeasurementLog(file: file, now: { clock.now }, traffic: { traffic.reading })
        #expect(busy.rows.count == MeasurementLog.mostRows)
        busy.begin(Self.wifiFull, power: Self.cool)
        clock.now.addTimeInterval(60)
        busy.end(power: Self.cool)
        #expect(busy.rows.count == MeasurementLog.mostRows - MeasurementLog.dropAtOnce)
        #expect(busy.rows.first == "row \(MeasurementLog.dropAtOnce + 1)")
        #expect(busy.rows.last?.hasPrefix("2026-09-27T19:43:30Z,60.0,") == true)
        #expect(try String(contentsOf: file, encoding: .utf8) == busy.rows.joined(separator: "\n") + "\n")

        // An oversized file is cut to the most rows when read.
        let over = (0..<(MeasurementLog.mostRows + 5)).map { "row \($0)" }
        try Data((over.joined(separator: "\n") + "\n").utf8).write(to: file)
        let read = MeasurementLog(file: file, now: { clock.now }, traffic: { traffic.reading })
        #expect(read.rows.count == MeasurementLog.mostRows && read.rows.first == "row 5")
    }

    // MARK: In the session

    final class PowerReading {
        var reading = ThermalAndPowerWatcher.Reading.steady
        var batteryPercent: Int? = 90
    }

    @Test("the session keeps the log: its rows follow the network, the band's request and sound only while away")
    func sessionKeepsTheLog() async throws {
        let suite = "MeasurementLogTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let network = ConnectionFlowTests.FakeNetwork()
        let reading = PowerReading()
        let power = ThermalAndPowerWatcher(read: { reading.reading }, center: NotificationCenter())
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let app = AppModel(phoneSettings: settings,
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           sessionSources: SessionController.Sources(
                               power: power, network: network, traffic: { traffic.totals },
                               now: { clock.now }, tick: .seconds(3600), kinds: { traffic.reading },
                               battery: { reading.batteryPercent }, measurementFile: nil))
        let session = app.longSession
        let log = session.measurements
        network.set(NetworkPath(online: true, interfaces: ["en0"], wifi: true))
        await session.tick()
        #expect(!log.isRecording && log.rows.isEmpty)

        session.sessionStarted(owner: 1)
        session.connectionChanged(.connected)
        #expect(log.isRecording)
        traffic.add(.control, in: 10_000, out: 2_000)
        traffic.add(.display, in: 500_000)
        traffic.add(.audio, in: 150_000)
        clock.now.addTimeInterval(60)
        reading.batteryPercent = 89
        await session.tick()
        #expect(log.rows == ["2026-09-27T19:42:00Z,60.0,Wi-Fi,In front,Full,30,full,89,no,nominal,"
            + "10000,2000,500000,0,150000,0,0,0,0,0"])

        // Locked with Sound only on: the band stops; only sound and the
        // Core's readings come (R-IOS-10's measurement).
        clock.now.addTimeInterval(5)
        session.sceneChanged(inForeground: false)
        #expect(log.rows.count == 2)
        traffic.add(.control, in: 4_000, out: 500)
        traffic.add(.audio, in: 150_000)
        clock.now.addTimeInterval(60)
        await session.tick()
        #expect(log.rows.last == "2026-09-27T19:43:05Z,60.0,Wi-Fi,Away,Full,0,none,89,no,nominal,"
            + "4000,500,0,0,150000,0,0,0,0,0")

        // On cellular the row ends and the next asks for Balanced.
        clock.now.addTimeInterval(1)
        session.sceneChanged(inForeground: true)
        clock.now.addTimeInterval(2)
        network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        clock.now.addTimeInterval(7)
        traffic.add(.display, in: 30_000)
        session.sessionEnded(owner: 1)
        #expect(!log.isRecording)
        #expect(Array(log.rows.suffix(3)) == [
            "2026-09-27T19:44:05Z,1.0,Wi-Fi,Away,Full,0,none,89,no,nominal,0,0,0,0,0,0,0,0,0,0",
            "2026-09-27T19:44:06Z,2.0,Wi-Fi,In front,Full,30,full,89,no,nominal,0,0,0,0,0,0,0,0,0,0",
            "2026-09-27T19:44:08Z,7.0,Cellular,In front,Balanced,15,full,89,no,nominal,0,0,30000,0,0,0,0,0,0,0",
        ])
    }

    // MARK: The support bundle

    @Test("the support bundle's phone log carries the measurement log between its header and the log lines")
    func inTheSupportBundle() async throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("MeasurementLogTests-\(UUID())")
        defer { try? FileManager.default.removeItem(at: directory) }
        let clock = Clock(Self.start)
        let traffic = Traffic()
        let log = MeasurementLog(file: nil, now: { clock.now }, traffic: { traffic.reading })
        let bundle = SupportBundleModel(mirror: MirrorStore(send: { _ in }), commands: nil,
                                        phoneLog: PhoneLog { ["first line", "second line"] },
                                        measurements: log, directory: directory, now: { clock.now })
        await bundle.collect()
        let empty = try String(contentsOf: try #require(bundle.files.first), encoding: .utf8)
        #expect(empty.contains("\n\n\(MeasurementLog.heading)\n\(MeasurementLog.noRows)\n\nfirst line\nsecond line\n"))

        log.begin(Self.wifiFull, power: Self.cool)
        traffic.add(.audio, in: 150_000)
        clock.now.addTimeInterval(60)
        log.end(power: Self.cool)
        await bundle.collect()
        #expect(bundle.files.count == 1)
        let phone = try String(contentsOf: try #require(bundle.files.first), encoding: .utf8)
        #expect(phone.hasPrefix("NereusSDR for iPhone and iPad\n"))
        let section = [MeasurementLog.heading, MeasurementLog.columns.joined(separator: ","),
                       "2026-09-27T19:42:00Z,60.0,Wi-Fi,In front,Full,30,full,81,no,nominal,"
                           + "0,0,0,0,150000,0,0,0,0,0"].joined(separator: "\n")
        #expect(phone.contains("Z\n\n\(section)\n\nfirst line\nsecond line\n"))
        #expect(phone.hasSuffix("first line\nsecond line\n"))
    }
}
