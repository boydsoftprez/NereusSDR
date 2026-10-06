// NereusSDR for iOS: asking a FreeDV Reporter station to QSY: your slice's frequency, theirs or a typed one, then Send QSY
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Ask to QSY (spec section 5.7 item 9, picture 21's third phone): pick
/// the frequency (the active slice's now, the station's, or one you type),
/// then Send QSY. The Core passes the request to FreeDV Reporter, and your
/// radio tunes there too, as on the desktop.
struct AskToQsySheet: View {
    enum Pick: Hashable {
        case slice
        case theirs
        case typed
    }

    @ObservedObject var freedv: FreeDVReporterModel
    let station: FreeDVStation
    @State private var pick: Pick
    @State private var typed = ""

    init(freedv: FreeDVReporterModel, station: FreeDVStation, pick: Pick? = nil, typed: String = "") {
        self.freedv = freedv
        self.station = station
        _pick = State(initialValue: pick ?? (freedv.sliceFrequencyHz != nil ? .slice : .theirs))
        _typed = State(initialValue: typed)
    }

    /// The frequency a pick asks for, whole hertz; nil when there is none.
    static func frequency(_ pick: Pick, slice: Double?, theirs: Double, typed: String) -> Double? {
        switch pick {
        case .slice:
            return slice
        case .theirs:
            return theirs > 0 ? theirs : nil
        case .typed:
            let text = typed.trimmingCharacters(in: .whitespaces).replacingOccurrences(of: ",", with: ".")
            guard let mhz = Double(text), mhz > 0, mhz < 10_000 else {
                return nil
            }
            return (mhz * 1_000_000).rounded()
        }
    }

    /// "7.236400 MHz".
    static func megahertz(_ hz: Double) -> String {
        String(format: "%.6f MHz", hz / 1_000_000)
    }

    var body: some View {
        let hz = Self.frequency(pick, slice: freedv.sliceFrequencyHz, theirs: station.frequencyHz, typed: typed)
        let sent = freedv.qsySentTo == station.callsign
        let reason = freedv.qsyReason
        let letter = freedv.sliceLetter
        FreeDVChrome.Sheet(identifier: "freedv.qsySheet") {
            HStack(alignment: .firstTextBaseline) {
                Text("Ask \(station.callsign) to QSY")
                    .font(.system(size: 18, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                Spacer()
            }
            Text("FreeDV Reporter passes the request to \(station.callsign)'s software, and your radio tunes there too.")
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Frequency")
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                    Text(caption(letter))
                        .font(.system(size: 10.5))
                        .foregroundStyle(ChromeColours.textDim)
                }
                Spacer()
                if pick == .typed {
                    TextField("MHz", text: $typed)
                        .keyboardType(.decimalPad)
                        .multilineTextAlignment(.trailing)
                        .font(.system(size: 14, design: .monospaced))
                        .foregroundStyle(ChromeColours.textBright)
                        .frame(width: 130, height: 32)
                        .padding(.horizontal, 8)
                        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                        .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .accessibilityLabel("Frequency in MHz")
                        .accessibilityIdentifier("freedv.qsy.typed")
                } else {
                    Text(hz.map(Self.megahertz) ?? "-")
                        .font(.system(size: 14, design: .monospaced))
                        .foregroundStyle(ChromeColours.textBright)
                        .frame(minWidth: 130, minHeight: 32, alignment: .trailing)
                        .padding(.horizontal, 8)
                        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                        .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .accessibilityIdentifier("freedv.qsy.frequency")
                }
            }
            .padding(10)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
            HStack(spacing: 6) {
                FreeDVChrome.Choice(title: letter.map { "Slice \($0)" } ?? "Your slice", chosen: pick == .slice,
                                    enabled: freedv.sliceFrequencyHz != nil, identifier: "freedv.qsy.slice") {
                    pick = .slice
                }
                FreeDVChrome.Choice(title: "Their frequency", chosen: pick == .theirs,
                                    enabled: station.frequencyHz > 0, identifier: "freedv.qsy.theirs") {
                    pick = .theirs
                }
                FreeDVChrome.Choice(title: "Type one", chosen: pick == .typed, identifier: "freedv.qsy.type") {
                    pick = .typed
                }
            }
            FreeDVChrome.Wide(title: sent ? "QSY sent to \(station.callsign)" : "Send QSY", primary: true,
                              enabled: !sent && reason == nil && hz != nil, identifier: "freedv.qsy.send") {
                if let hz {
                    freedv.sendQsy(station, hz: hz)
                }
            }
            FreeDVChrome.Wide(title: sent ? "Close" : "Cancel", identifier: "freedv.qsy.cancel") {
                freedv.openQsy = nil
            }
            if let note = freedv.note {
                AccessoryChrome.Refusal(text: note) { freedv.note = nil }
            } else if let reason {
                AccessoryChrome.Note(text: reason)
            } else if hz == nil {
                AccessoryChrome.Note(text: FreeDVReporterModel.noQsyFrequencyReason)
            }
        }
    }

    private func caption(_ letter: String?) -> String {
        switch pick {
        case .slice:
            return letter.map { "Your slice \($0), now" } ?? "Your slice, now"
        case .theirs:
            return "Where \(station.callsign) is now"
        case .typed:
            return "Typed, in MHz"
        }
    }
}
