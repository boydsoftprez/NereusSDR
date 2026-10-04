// NereusSDR for iOS: local choices for the approved keyed meter overlay
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

enum KeyedMeterPage: String, CaseIterable, Codable, Identifiable, Sendable {
    case radio = "Radio", amp = "Amp", tuner = "Tuner", custom = "Custom"
    var id: String { rawValue }
    var defaults: [KeyedMeter] {
        switch self {
        case .radio: [.rfPower, .swr, .mic]
        case .amp: [.ampPower, .ampSwr, .ampTemperature]
        case .tuner: [.tunerPower, .tunerSwr, .mic]
        case .custom: [.mic, .eq, .alcGain]
        }
    }
}

enum KeyedMeter: String, CaseIterable, Codable, Identifiable, Sendable {
    case rfPower, swr, mic, ampPower, ampSwr, ampTemperature, tunerPower, tunerSwr
    case eq, leveler, levelerGain, cfc, cfcGain, alcGain, alcGroup
    enum Source: Sendable { case radio, amp, tuner, stage }
    var id: String { rawValue }
    var source: Source {
        switch self {
        case .ampPower, .ampSwr, .ampTemperature: .amp
        case .tunerPower, .tunerSwr: .tuner
        case .rfPower, .swr, .mic: .radio
        default: .stage
        }
    }
    var name: String {
        switch self {
        case .rfPower: "Radio RF power"
        case .swr: "Radio SWR"
        case .mic: "Mic level"
        case .ampPower: "Amp output"
        case .ampSwr: "Amp SWR"
        case .ampTemperature: "Amp temperature"
        case .tunerPower: "Tuner power"
        case .tunerSwr: "Tuner SWR"
        case .eq: "EQ"
        case .leveler: "Leveler"
        case .levelerGain: "Leveler Gain"
        case .cfc: "CFC"
        case .cfcGain: "CFC Gain"
        case .alcGain: "ALC Gain"
        case .alcGroup: "ALC Group"
        }
    }
    var stageIndex: Int? {
        switch self {
        case .eq: 0
        case .leveler: 1
        case .levelerGain: 2
        case .cfc: 3
        case .cfcGain: 4
        case .alcGain: 5
        case .alcGroup: 6
        default: nil
        }
    }
}

enum KeyedMeterStatus: Equatable, Sendable {
    case absent, offline(String), available
    var isAvailable: Bool { self == .available }
    var reason: String? {
        if case .offline(let text) = self { return text }
        return nil
    }
}

struct KeyedMeterAvailability: Equatable, Sendable {
    var amp: KeyedMeterStatus = .absent
    var tuner: KeyedMeterStatus = .absent
    var stagesReason: String? = nil
    func status(for page: KeyedMeterPage) -> KeyedMeterStatus {
        switch page {
        case .amp: amp
        case .tuner: tuner
        case .radio, .custom: .available
        }
    }
    func status(for meter: KeyedMeter) -> KeyedMeterStatus {
        switch meter.source {
        case .amp: amp
        case .tuner: tuner
        case .radio: .available
        case .stage: stagesReason.map(KeyedMeterStatus.offline) ?? .available
        }
    }
    var visiblePages: [KeyedMeterPage] { KeyedMeterPage.allCases.filter { status(for: $0) != .absent } }
    var selectablePages: [KeyedMeterPage] { visiblePages.filter { status(for: $0).isAvailable } }
    var pickerMeters: [KeyedMeter] { KeyedMeter.allCases.filter { status(for: $0) != .absent } }
}

/// Choices are display preferences only. Equipment availability never erases a saved set.
struct KeyedMeterSettings: Equatable, Sendable {
    static let key = "phone.keyedMeters.settings"
    private(set) var page: KeyedMeterPage = .radio
    private var sets = Dictionary(uniqueKeysWithValues: KeyedMeterPage.allCases.map { ($0, $0.defaults) })
    init(data: Data? = nil) {
        guard let data, let stored = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        if let name = stored["page"] as? String, let kept = KeyedMeterPage(rawValue: name) { page = kept }
        guard let keptSets = stored["sets"] as? [String: Any] else { return }
        for tab in KeyedMeterPage.allCases {
            guard let names = keptSets[tab.rawValue] as? [Any] else { continue }
            sets[tab] = tab.defaults.enumerated().map { index, fallback in
                guard names.indices.contains(index), let name = names[index] as? String else { return fallback }
                return KeyedMeter(rawValue: name) ?? fallback
            }
        }
    }
    func meters(on page: KeyedMeterPage) -> [KeyedMeter] { sets[page] ?? page.defaults }
    func data() -> Data? {
        let storedSets = Dictionary(uniqueKeysWithValues: KeyedMeterPage.allCases.map { ($0.rawValue, meters(on: $0).map(\.rawValue)) })
        return try? JSONSerialization.data(withJSONObject: ["page": page.rawValue, "sets": storedSets], options: .sortedKeys)
    }
    func displayPage(availability: KeyedMeterAvailability) -> KeyedMeterPage {
        availability.status(for: page).isAvailable ? page : .radio
    }
    @discardableResult mutating func choose(_ page: KeyedMeterPage, availability: KeyedMeterAvailability) -> Bool {
        guard availability.status(for: page).isAvailable, self.page != page else { return false }
        self.page = page
        return true
    }
    @discardableResult mutating func replace(slot: Int, with meter: KeyedMeter, on page: KeyedMeterPage,
                                            availability: KeyedMeterAvailability) -> Bool {
        guard availability.status(for: page).isAvailable, availability.status(for: meter).isAvailable,
              (0..<3).contains(slot) else { return false }
        var kept = meters(on: page)
        guard kept[slot] != meter else { return false }
        kept[slot] = meter
        sets[page] = kept
        return true
    }
    @discardableResult mutating func swipe(direction: Int, availability: KeyedMeterAvailability) -> Bool {
        guard direction != 0 else { return false }
        let pages = availability.selectablePages
        guard let current = pages.firstIndex(of: displayPage(availability: availability)) else { return false }
        let next = (current + (direction > 0 ? 1 : -1) + pages.count) % pages.count
        return choose(pages[next], availability: availability)
    }
}
