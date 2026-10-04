// NereusSDR for iOS: the Diversity sensitivity pattern as the Core sends it on each slice, read as sent
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18 (link document section 7.1, "The diversity pattern",
/// `diversityPatternVersion` 1): each slice's `diversityPattern` is compact
/// JSON; the points are fractions of the peak at `stepDeg` steps from north,
/// clockwise, kept in the order sent; unknown keys are ignored; a value that
/// does not read as the pattern's shape reads as nil.
@Suite struct DiversityPatternTests {
    /// The pattern the diversity-pattern session's Core sends for slice 0.
    static func fixturePattern() throws -> String {
        for message in try FixtureReplay.stationMessages("session-diversity-pattern") {
            if case .delta(let delta) = message, delta.key == "slice:0",
               let entry = delta.properties.first(where: { $0.name == DiversityPattern.propertyName }),
               case .utf8(let json) = entry.value {
                return json
            }
        }
        Issue.record("the session sends no pattern")
        return ""
    }

    @Test("the session's pattern reads as sent: 120 points at 3 degree steps, 5.5 m, no cross-fire")
    func fixture() throws {
        let pattern = try #require(DiversityPattern(json: try Self.fixturePattern()))
        #expect(pattern.points.count == 120)
        #expect(pattern.stepDeg == 3)
        #expect(pattern.spacingMeters == 5.5)
        #expect(pattern.crossFire == false)
        #expect(pattern.points[0] == 0.517 && pattern.points[30] == 0.068 && pattern.points[90] == 1)
        #expect(pattern.points.allSatisfy { (0...1).contains($0) })
    }

    @Test("the worked example: 14.2 MHz, phase 0, gain 0: north 1, 90 degrees 0.518, 180 0.069, 270 0.518")
    func workedExample() throws {
        var points = Array(repeating: 0.5, count: 120)
        points[0] = 1
        points[30] = 0.518
        points[60] = 0.069
        points[90] = 0.518
        let list = points.map { String($0) }.joined(separator: ",")
        let json = "{\"crossFire\":false,\"points\":[\(list)],\"spacingMeters\":5.5,\"stepDeg\":3}"
        let pattern = try #require(DiversityPattern(json: json))
        #expect(pattern.bearing(of: 0) == 0 && pattern.bearing(of: 30) == 90)
        #expect(pattern.bearing(of: 60) == 180 && pattern.bearing(of: 90) == 270)
        #expect(pattern.points[0] == 1 && pattern.points[30] == 0.518)
        #expect(pattern.points[60] == 0.069 && pattern.points[90] == 0.518)
    }

    @Test("key order does not matter and unknown keys are ignored")
    func unknownKeys() throws {
        let json = "{\"stepDeg\":90,\"future\":{\"a\":1},\"points\":[1,0.5,0.25,0.5],\"spacingMeters\":5.5,"
            + "\"crossFire\":false,\"another\":[1,2]}"
        let pattern = try #require(DiversityPattern(json: json))
        #expect(pattern.points == [1, 0.5, 0.25, 0.5])
        #expect(pattern.bearing(of: 3) == 270)
    }

    @Test("nothing sent reads as nil; a value that is not the pattern's shape reads as nil")
    func unreadable() {
        #expect(DiversityPattern(json: "") == nil)
        for json in ["not json", "[1,2]", "{}", "{\"points\":[],\"stepDeg\":3}",
                     "{\"points\":[1,\"x\",0.5],\"stepDeg\":3}", "{\"points\":[1,0.5],\"stepDeg\":0}",
                     "{\"points\":[1,0.5],\"stepDeg\":-3}", "{\"points\":[1,0.5]}",
                     "{\"points\":\"1,0.5\",\"stepDeg\":3}"] {
            #expect(DiversityPattern(json: json) == nil, "\(json)")
        }
    }

    @Test("the spacing and cross-fire are optional facts; missing ones read as nil")
    func optionalFacts() throws {
        let pattern = try #require(DiversityPattern(json: "{\"points\":[1,0.5,0.25],\"stepDeg\":120}"))
        #expect(pattern.spacingMeters == nil && pattern.crossFire == nil)
    }

    @Test("the gate: the hello feature, the capability, the property and the agreed minor")
    func gate() {
        #expect(DiversityPattern.featureName == "diversityPattern")
        #expect(DiversityPattern.capabilityName == "diversityPatternVersion")
        #expect(DiversityPattern.propertyName == "diversityPattern")
        #expect(DiversityPattern.minor == 11)
    }
}
