// NereusSDR for iOS: the Core's Setup > Display description in its catalogue: the controls, the bin width and the FFT plan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

extension StationCatalog {
    /// The catalogue's `display` (link document section 7.4, R-IOS-06,
    /// R-IOS-18, R-IOS-27): the desktop's Setup > Display controls the Core
    /// describes, in the desktop pages' order, the bin width readout and the
    /// rule for the FFT size to ask for. A Core that sends none keeps the
    /// phone's earlier behaviour. Labels, options and ranges are shown as
    /// sent; the app carries no copy of them.
    public struct Display: Equatable, Sendable, Decodable {
        /// One of a control's choices: the value it writes and its label.
        public struct Option: Equatable, Sendable, Decodable {
            public let value: Double
            public let label: String

            public init(value: Double, label: String) {
                self.value = value
                self.label = label
            }
        }

        /// How a control is drawn.
        public enum Kind: Equatable, Sendable {
            case choice
            /// A slider over `min...max`, or stepping through its options.
            case slider
            case toggle
            /// A kind this app does not know: shown greyed.
            case other(String)

            /// The kind as the Core names it.
            public var name: String {
                switch self {
                case .choice:
                    return "choice"
                case .slider:
                    return "slider"
                case .toggle:
                    return "switch"
                case .other(let text):
                    return text
                }
            }

            init(_ text: String) {
                switch text {
                case "choice":
                    self = .choice
                case "slider":
                    self = .slider
                case "switch":
                    self = .toggle
                default:
                    self = .other(text)
                }
            }
        }

        /// One control.
        public struct Control: Equatable, Sendable, Decodable {
            /// The Core's setting this control writes with `settings.write`
            /// when ``isStation``; the desktop's own key otherwise, or nil.
            public let settingsKey: String?
            /// `station` (the Core's value, shared by every device) or
            /// `device` (kept per pan on this phone and sent in the display
            /// subscription).
            public let scope: String
            /// What the control sets in the display subscription (`fftSize`,
            /// `trace.detector`, `decimation`, ...).
            public let subscribe: String
            public let page: String
            public let group: String
            public let label: String
            public let kind: Kind
            public let defaultValue: Double?
            public let options: [Option]
            public let min: Double?
            public let max: Double?
            public let step: Double?
            /// Nil when the Core sends none (a choice, a slider over options).
            public let unit: String?
            public let decimals: Int?
            /// The value that means Off, with its label (the Hz/bin target).
            public let offValue: Double?
            public let offLabel: String?

            /// The Core keeps this value for every device.
            public var isStation: Bool { scope == "station" }
            /// This phone keeps this value for the pan.
            public var isDevice: Bool { scope == "device" }

            /// A slider's range, `{min, max, step}`; nil for a control
            /// without one (a choice, or a slider over options).
            public var range: StationCatalog.Range? {
                guard let min, let max, let step, max >= min, step > 0 else {
                    return nil
                }
                return StationCatalog.Range(min: min, max: max, step: step)
            }

            /// `value` as the control reads it: Off, an option's label, or
            /// the number to its decimals with its unit.
            public func text(_ value: Double) -> String {
                if let offValue, let offLabel, value == offValue {
                    return offLabel
                }
                if let option = options.first(where: { $0.value == value }) {
                    return option.label
                }
                let number = String(format: "%.\(Swift.max(0, decimals ?? 0))f", value)
                guard let unit, !unit.isEmpty else {
                    return number
                }
                return "\(number) \(unit)"
            }

            /// `value` kept inside the control: on its nearest option, or
            /// within its range on its nearest step.
            public func clamped(_ value: Double) -> Double {
                guard value.isFinite else {
                    return defaultValue ?? min ?? options.first?.value ?? 0
                }
                if !options.isEmpty {
                    return options.min { abs($0.value - value) < abs($1.value - value) }?.value ?? value
                }
                guard let range else {
                    return value
                }
                let steps = ((value - range.min) / range.step).rounded()
                return Swift.min(Swift.max(range.min + steps * range.step, range.min), range.max)
            }

            enum CodingKeys: String, CodingKey {
                case settingsKey, scope, subscribe, page, group, label, kind, options, min, max, step, unit
                case decimals, offValue, offLabel
                case defaultValue = "default"
            }

            public init(from decoder: any Decoder) throws {
                let container = try decoder.container(keyedBy: CodingKeys.self)
                settingsKey = try container.decodeIfPresent(String.self, forKey: .settingsKey)
                scope = try container.decode(String.self, forKey: .scope)
                subscribe = try container.decode(String.self, forKey: .subscribe)
                page = try container.decodeIfPresent(String.self, forKey: .page) ?? ""
                group = try container.decodeIfPresent(String.self, forKey: .group) ?? ""
                label = try container.decode(String.self, forKey: .label)
                kind = Kind(try container.decode(String.self, forKey: .kind))
                defaultValue = try? container.decodeIfPresent(Double.self, forKey: .defaultValue)
                options = try container.decodeIfPresent([Option].self, forKey: .options) ?? []
                min = try container.decodeIfPresent(Double.self, forKey: .min)
                max = try container.decodeIfPresent(Double.self, forKey: .max)
                step = try container.decodeIfPresent(Double.self, forKey: .step)
                unit = try container.decodeIfPresent(String.self, forKey: .unit)
                decimals = try container.decodeIfPresent(Int.self, forKey: .decimals)
                offValue = try container.decodeIfPresent(Double.self, forKey: .offValue)
                offLabel = try container.decodeIfPresent(String.self, forKey: .offLabel)
            }
        }

        /// The bin width readout: its label and decimals.
        public struct BinWidth: Equatable, Sendable, Decodable {
            public let label: String
            public let decimals: Int

            public init(label: String, decimals: Int) {
                self.label = label
                self.decimals = decimals
            }

            /// The width of one bin of `fftSize` at `sampleRateHz`, in hertz,
            /// to the readout's decimals (the pan's rate over the size).
            public func text(sampleRateHz: Double, fftSize: Int) -> String? {
                guard sampleRateHz > 0, fftSize > 0 else {
                    return nil
                }
                return String(format: "%.\(Swift.max(0, decimals))f", sampleRateHz / Double(fftSize))
            }
        }

        /// The FFT sizes the Core computes, smallest to largest.
        public struct FftPlan: Equatable, Sendable, Decodable {
            public let minFftSize: Int
            public let maxFftSize: Int

            public init(minFftSize: Int, maxFftSize: Int) {
                self.minFftSize = minFftSize
                self.maxFftSize = maxFftSize
            }

            /// The smallest power of two from ``minFftSize`` that reaches
            /// `target`, at most ``maxFftSize``.
            public func rounded(_ target: Double) -> Int {
                var size = Swift.max(1, minFftSize)
                while size < maxFftSize && Double(size) < target {
                    size *= 2
                }
                return size
            }

            /// The FFT size to ask for (link 7.4, `fftPlan`): enough bins for
            /// `pixels` across `spanHz` of `sampleRateHz`, held at or below a
            /// Hz/bin target above 0, and never below the Size setting; fine
            /// when it is above that floor.
            public func plan(sampleRateHz: Double, pixels: Int, spanHz: Double, sizeSetting: Double,
                             hzPerBinTarget: Double?) -> (fftSize: Int, fine: Bool) {
                var wanted = spanHz > 0 ? sampleRateHz * Double(pixels) / spanHz : 0
                if let target = hzPerBinTarget, target.isFinite, target > 0 {
                    wanted = Swift.max(wanted, sampleRateHz / target)
                }
                let floor = rounded(sizeSetting)
                let size = Swift.max(floor, rounded(wanted))
                return (size, size > floor)
            }
        }

        public let controls: [Control]
        public let binWidth: BinWidth
        public let fftPlan: FftPlan

        /// The control that writes the Core's `key`.
        public func control(settingsKey key: String) -> Control? {
            controls.first { $0.settingsKey == key }
        }

        /// The first control that sets `field` in the subscription.
        public func control(subscribe field: String) -> Control? {
            controls.first { $0.subscribe == field }
        }

        enum CodingKeys: String, CodingKey {
            case controls, binWidth, fftPlan
        }

        public init(from decoder: any Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            // A control that does not read is left out; the rest stand.
            controls = try container.decode([LenientControl].self, forKey: .controls).compactMap(\.control)
            binWidth = try container.decode(BinWidth.self, forKey: .binWidth)
            fftPlan = try container.decode(FftPlan.self, forKey: .fftPlan)
        }

        private struct LenientControl: Decodable {
            let control: Control?

            init(from decoder: any Decoder) throws {
                control = try? Control(from: decoder)
            }
        }
    }
}
