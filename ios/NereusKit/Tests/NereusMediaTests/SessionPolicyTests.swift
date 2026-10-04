// NereusSDR for iOS: tests of what the band asks for in each circumstance of a long session
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia
import Testing

/// R-IOS-22, R-IOS-23, D27, D28: each circumstance yields the stated request.
@Suite("Session policy")
struct SessionPolicyTests {
    typealias Circumstances = SessionPolicy.Circumstances
    typealias Request = SessionPolicy.Request

    struct Row: Sendable, CustomTestStringConvertible {
        let name: String
        let circumstances: Circumstances
        let request: Request

        var testDescription: String { name }
    }

    static let full = Request(fps: 30, detail: .full)
    static let balanced = Request(fps: 15, detail: .full)
    static let saver = Request(fps: 5, detail: .half)

    static let rows: [Row] = [
        Row(name: "Wi-Fi starts at Full", circumstances: Circumstances(), request: full),
        Row(name: "cellular starts at Balanced", circumstances: Circumstances(network: .cellular),
            request: balanced),
        Row(name: "Wi-Fi at Balanced", circumstances: Circumstances(wifiMode: .balanced), request: balanced),
        Row(name: "cellular at Full", circumstances: Circumstances(network: .cellular, cellularMode: .full),
            request: full),
        Row(name: "cellular at Saver", circumstances: Circumstances(network: .cellular, cellularMode: .saver),
            request: saver),
        Row(name: "cellular at Audio only asks for no endpoints",
            circumstances: Circumstances(network: .cellular, cellularMode: .audioOnly), request: .none),
        Row(name: "Wi-Fi never reads a cellular-only mode",
            circumstances: Circumstances(wifiMode: .saver), request: full),
        Row(name: "locked or in another app with Sound only: no endpoints",
            circumstances: Circumstances(inForeground: false), request: .none),
        Row(name: "locked on cellular with Sound only: no endpoints",
            circumstances: Circumstances(network: .cellular, inForeground: false), request: .none),
        Row(name: "away with Sound only off keeps the band",
            circumstances: Circumstances(inForeground: false, soundOnlyWhenAway: false), request: full),
        Row(name: "Low Power Mode drops Full to Saver",
            circumstances: Circumstances(lowPowerMode: true), request: saver),
        Row(name: "Low Power Mode drops Balanced to Saver",
            circumstances: Circumstances(network: .cellular, lowPowerMode: true), request: saver),
        Row(name: "Low Power Mode keeps Audio only",
            circumstances: Circumstances(network: .cellular, cellularMode: .audioOnly, lowPowerMode: true),
            request: .none),
        Row(name: "Low Power Mode with its switch off changes nothing",
            circumstances: Circumstances(lowPowerMode: true, lowPowerDropsToSaver: false), request: full),
        Row(name: "a fair heat changes nothing", circumstances: Circumstances(heat: .fair), request: full),
        Row(name: "a serious heat halves Full", circumstances: Circumstances(heat: .serious),
            request: Request(fps: 15, detail: .full)),
        Row(name: "a serious heat halves Balanced", circumstances: Circumstances(network: .cellular, heat: .serious),
            request: Request(fps: 7, detail: .full)),
        Row(name: "a serious heat halves Saver",
            circumstances: Circumstances(network: .cellular, cellularMode: .saver, heat: .serious),
            request: Request(fps: 2, detail: .half)),
        Row(name: "a critical heat is Saver", circumstances: Circumstances(heat: .critical), request: saver),
        Row(name: "a critical heat keeps Audio only",
            circumstances: Circumstances(network: .cellular, cellularMode: .audioOnly, heat: .critical),
            request: .none),
        Row(name: "heat with its switch off changes nothing",
            circumstances: Circumstances(heat: .critical, hotPhoneSlows: false), request: full),
        Row(name: "Low Power Mode and a serious heat together",
            circumstances: Circumstances(lowPowerMode: true, heat: .serious), request: Request(fps: 2, detail: .half)),
    ]

    @Test("each circumstance yields the stated request", arguments: rows)
    func table(_ row: Row) {
        #expect(SessionPolicy.request(for: row.circumstances) == row.request)
    }

    @Test("the modes ask for the spec's rates and detail, and the defaults are D28's")
    func modes() {
        #expect(SessionPolicy.Mode.full.request == Self.full)
        #expect(SessionPolicy.Mode.balanced.request == Self.balanced)
        #expect(SessionPolicy.Mode.saver.request == Self.saver)
        #expect(!SessionPolicy.Mode.audioOnly.request.subscribes)
        #expect(SessionPolicy.Mode.wifiDefault == .full)
        #expect(SessionPolicy.Mode.cellularDefault == .balanced)
        #expect(SessionPolicy.Mode.wifiModes == [.full, .balanced])
        #expect(SessionPolicy.Mode.cellularModes == [.full, .balanced, .saver, .audioOnly])
    }

    @Test("a request caps the band's frames and halves its width, never raising either")
    func capping() {
        #expect(Self.saver.fps(30) == 5)
        #expect(Self.full.fps(10) == 10)
        #expect(Self.saver.pixels(402) == 201)
        #expect(Self.saver.pixels(1) == 1)
        #expect(Self.full.pixels(402) == 402)
        #expect(Request.none.fps(30) == 30)
    }
}
