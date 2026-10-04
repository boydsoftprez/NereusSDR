// NereusSDR for iOS: the TX EQ curve as the Core sends it on transmit.txEqCurve, read and turned into its drawn line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMirror

/// R-IOS-34, D92 (link document section 7.1, "The TX EQ curve
/// (`txEqCurve`)"): the curve is read as sent, never reordered; its line at
/// a frequency is the response plus `preampDb`, a sum of bells when
/// parametric, straight lines between the points when not; `unavailable`
/// (or any state the phone does not know) has no line.
@Suite struct TxEqCurveTests {
    /// The link document's worked example, as the Core sends it.
    static let worked = """
    {"maxHz":3000,"minHz":50,"parametric":true,"points":[{"frequencyHz":50,"gainDb":-6,"q":1.5},\
    {"frequencyHz":300,"gainDb":3,"q":2},{"frequencyHz":1200,"gainDb":-1.5,"q":4},\
    {"frequencyHz":2400,"gainDb":4,"q":3},{"frequencyHz":3000,"gainDb":0,"q":1}],"preampDb":-2.5,"state":"saved"}
    """

    @Test("the worked example reads as sent and draws to 0.01 dB at its five points")
    func workedExample() throws {
        let curve = try #require(TxEqCurve(json: Self.worked))
        #expect(curve.state == .saved)
        #expect(curve.parametric)
        #expect(curve.preampDb == -2.5 && curve.minHz == 50 && curve.maxHz == 3000)
        #expect(curve.points.map(\.frequencyHz) == [50, 300, 1200, 2400, 3000])
        #expect(curve.points.map(\.gainDb) == [-6, 3, -1.5, 4, 0])
        #expect(curve.points.map(\.q) == [1.5, 2, 4, 3, 1])
        let expected: [(Double, Double)] = [(50, -7.04), (300, -3.51), (1200, -4.00), (2400, 1.50), (3000, -2.50)]
        for (hz, db) in expected {
            let drawn = try #require(curve.levelDb(atHz: hz))
            #expect(abs(drawn - db) < 0.005, "\(hz) Hz: \(drawn)")
        }
    }

    @Test("the default curve: ten points from 0 to 4000 Hz at 0 dB, a flat line at the preamp")
    func defaultCurve() throws {
        let points = (0..<10).map { index in
            "{\"frequencyHz\":\(Double(index) * 4000 / 9),\"gainDb\":0,\"q\":4}"
        }.joined(separator: ",")
        let json = "{\"state\":\"default\",\"parametric\":true,\"preampDb\":0,\"minHz\":0,\"maxHz\":4000,"
            + "\"points\":[\(points)]}"
        let curve = try #require(TxEqCurve(json: json))
        #expect(curve.state == .default)
        #expect(curve.points.count == 10)
        let line = curve.line(samples: 41)
        #expect(line.count == 41)
        #expect(line.first?.hz == 0 && line.last?.hz == 4000)
        #expect(line.allSatisfy { $0.db == 0 })
    }

    @Test("unavailable, and a state the phone does not know, have no line")
    func unavailable() throws {
        let curve = try #require(TxEqCurve(json: "{\"state\":\"unavailable\"}"))
        #expect(curve.state == .unavailable)
        #expect(curve.points.isEmpty)
        #expect(curve.levelDb(atHz: 1000) == nil)
        #expect(curve.line(samples: 10).isEmpty)
        let unknown = try #require(TxEqCurve(json: Self.worked.replacingOccurrences(of: "\"saved\"",
                                                                                    with: "\"later\"")))
        #expect(unknown.state == .unavailable)
        #expect(unknown.line(samples: 10).isEmpty)
    }

    @Test("a value that does not read as the curve's shape reads as unavailable; no value reads as none")
    func malformed() throws {
        #expect(TxEqCurve(json: "") == nil)
        #expect(TxEqCurve(json: "not json")?.state == .unavailable)
        #expect(TxEqCurve(json: "[1,2]")?.state == .unavailable)
        // min not below max, and a single point: no curve to draw.
        let backwards = Self.worked.replacingOccurrences(of: "\"minHz\":50", with: "\"minHz\":3000")
        #expect(TxEqCurve(json: backwards)?.state == .unavailable)
        let onePoint = "{\"state\":\"saved\",\"parametric\":false,\"preampDb\":0,\"minHz\":0,\"maxHz\":10,"
            + "\"points\":[{\"frequencyHz\":0,\"gainDb\":1,\"q\":1}]}"
        #expect(TxEqCurve(json: onePoint)?.state == .unavailable)
    }

    @Test("unknown keys are ignored and key order does not matter")
    func unknownKeys() throws {
        let json = "{\"state\":\"saved\",\"extra\":[1],\"points\":[{\"q\":1,\"gainDb\":2,\"frequencyHz\":100,"
            + "\"note\":\"x\"},{\"gainDb\":4,\"frequencyHz\":200,\"q\":1}],\"maxHz\":200,\"minHz\":100,"
            + "\"preampDb\":1,\"parametric\":false}"
        let curve = try #require(TxEqCurve(json: json))
        #expect(curve.state == .saved)
        #expect(curve.points == [.init(frequencyHz: 100, gainDb: 2, q: 1), .init(frequencyHz: 200, gainDb: 4, q: 1)])
    }

    @Test("straight lines: the first gain at and below it, the last at and above it, lines between, plus the preamp")
    func straightLines() throws {
        let json = "{\"state\":\"saved\",\"parametric\":false,\"preampDb\":-1,\"minHz\":0,\"maxHz\":3000,"
            + "\"points\":[{\"frequencyHz\":0,\"gainDb\":-6,\"q\":2},{\"frequencyHz\":1000,\"gainDb\":6,\"q\":2},"
            + "{\"frequencyHz\":2000,\"gainDb\":0,\"q\":2},{\"frequencyHz\":3000,\"gainDb\":3,\"q\":2}]}"
        let curve = try #require(TxEqCurve(json: json))
        #expect(curve.levelDb(atHz: -50) == -7)
        #expect(curve.levelDb(atHz: 0) == -7)
        #expect(curve.levelDb(atHz: 500) == -1)
        #expect(curve.levelDb(atHz: 1000) == 5)
        #expect(curve.levelDb(atHz: 1500) == 2)
        #expect(curve.levelDb(atHz: 2500) == 0.5)
        #expect(curve.levelDb(atHz: 3000) == 2)
        #expect(curve.levelDb(atHz: 3500) == 2)
        // The drawn points: evenly spaced from minHz to maxHz, each at the curve's level.
        let line = curve.line(samples: 7)
        #expect(line.map(\.hz) == [0, 500, 1000, 1500, 2000, 2500, 3000])
        #expect(line.map(\.db) == [-7, -1, 5, 2, -1, 0.5, 2])
    }

    @Test("points are drawn in the order sent, never reordered; two points at one frequency keep their order")
    func neverReordered() throws {
        let json = "{\"state\":\"saved\",\"parametric\":false,\"preampDb\":0,\"minHz\":0,\"maxHz\":100,"
            + "\"points\":[{\"frequencyHz\":0,\"gainDb\":0,\"q\":1},{\"frequencyHz\":50,\"gainDb\":-3,\"q\":1},"
            + "{\"frequencyHz\":50,\"gainDb\":3,\"q\":1},{\"frequencyHz\":100,\"gainDb\":0,\"q\":1}]}"
        let curve = try #require(TxEqCurve(json: json))
        #expect(curve.points.map(\.gainDb) == [0, -3, 3, 0])
        #expect(curve.levelDb(atHz: 25) == -1.5)
        #expect(curve.levelDb(atHz: 75) == 1.5)
    }

    @Test("a narrow bell is never narrower than the range over 6000")
    func narrowestBell() throws {
        let json = "{\"state\":\"saved\",\"parametric\":true,\"preampDb\":0,\"minHz\":0,\"maxHz\":6000,"
            + "\"points\":[{\"frequencyHz\":0,\"gainDb\":0,\"q\":20},{\"frequencyHz\":3000,\"gainDb\":10,\"q\":20000},"
            + "{\"frequencyHz\":6000,\"gainDb\":0,\"q\":20}]}"
        let curve = try #require(TxEqCurve(json: json))
        // Width 1 Hz: sigma = 1 / 2.35482, so half a hertz off the centre the bell is at half height.
        let half = try #require(curve.levelDb(atHz: 3000.5))
        #expect(abs(half - 5) < 0.001)
    }

    @Test("the line's scale: -24 to 24 dB, a level beyond it drawn at the edge")
    func scale() {
        #expect(TxEqCurve.scaleDb == -24...24)
        #expect(TxEqCurve.drawnDb(30) == 24 && TxEqCurve.drawnDb(-30) == -24 && TxEqCurve.drawnDb(3) == 3)
    }
}
