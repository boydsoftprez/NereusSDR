// NereusSDR for iOS: PureSignal, one Tools page: on, off and status; calibration stays at the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// PureSignal on the phone (spec section 5.2 item 4, R-IOS-18): the
/// desktop applet's Auto switch as On and Off, and what the Core reports
/// (its state, the calibrating, correcting and feedback lamps, the feedback
/// level and the calibrations). On a radio without PureSignal the page says
/// so and shows no controls.
struct PureSignalPage: View {
    @ObservedObject var model: PureSignalModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "PureSignal", tag: .core)
            if model.radioHasIt {
                SpotHubPage.Card {
                    HStack(spacing: 10) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Automatic calibration")
                                .font(.system(size: 14))
                                .foregroundStyle(ChromeColours.text)
                            Text(model.stateText)
                                .font(.system(size: 11))
                                .foregroundStyle(ChromeColours.textDim)
                                .accessibilityIdentifier("pureSignal.state")
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        ToolPageParts.Choices(options: [(0, "Off"), (1, "On")], selected: model.on ? 1 : 0,
                                              enabled: model.on ? model.offReason == nil : model.onReason == nil,
                                              identifier: "pureSignal.switch") { model.set(on: $0 == 1) }
                            .frame(width: 120)
                    }
                    .padding(.horizontal, 10)
                    .padding(.vertical, 9)
                    if let reason = model.on ? model.offReason : model.onReason {
                        SpotHubPage.Line()
                        ToolPageParts.Reason(text: reason, identifier: "pureSignal.reason")
                    }
                }
                SpotHubPage.Heading(text: "Status", tag: .core).padding(.top, 8)
                SpotHubPage.Card {
                    if let status = model.status {
                        HStack(spacing: 8) {
                            lamp("Calibrating", status.calibrating)
                            lamp("Correcting", status.correctionsApplied)
                            lamp("Feedback", status.hearingFeedback)
                        }
                        .padding(.horizontal, 10)
                        .padding(.vertical, 9)
                        SpotHubPage.Line()
                        feedback(status)
                        SpotHubPage.Line()
                        ToolPageParts.Reading(title: "Correction", value: status.correctionsApplied ? "Applied" : "Off",
                                              identifier: "pureSignal.correction")
                        SpotHubPage.Line()
                        ToolPageParts.Reading(title: "Calibrations",
                                              value: "\(status.successfulCalibrations) of \(status.attemptedCalibrations)",
                                              identifier: "pureSignal.calibrations")
                    } else {
                        ToolPageParts.Reason(text: model.onReason == SpotsModel.notConnectedReason
                                                 ? SpotsModel.notConnectedReason : PureSignalModel.noStatusText,
                                             identifier: "pureSignal.noStatus")
                    }
                }
                ConnectChrome.Note(text: PureSignalModel.atCoreNote).padding(.top, 4)
            } else {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: TransmitModel.noPureSignalText, identifier: "pureSignal.none")
                }
            }
            if let error = model.actionError {
                ToolPageParts.Refusal(text: error, identifier: "pureSignal.actionError").padding(.top, 4)
            }
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "pureSignal.note").padding(.top, 4)
            }
        }
    }

    private func lamp(_ title: String, _ lit: Bool) -> some View {
        HStack(spacing: 5) {
            Circle()
                .fill(lit ? ConnectChrome.pillOn : ConnectChrome.pillUnknown)
                .frame(width: 9, height: 9)
            Text(title)
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(lit ? ChromeColours.textBright : ChromeColours.textDim)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
        .accessibilityValue(lit ? "On" : "Off")
        .accessibilityIdentifier("pureSignal.lamp.\(title)")
    }

    /// The feedback level as the desktop's gauge shows it: amber from 70
    /// percent, red from 90, and the Core's number beside it.
    private func feedback(_ status: PureSignalStatus) -> some View {
        let share = status.feedbackShare
        let colour = share >= 0.9 ? ConnectChrome.pillOff : share >= 0.7 ? ConnectChrome.pillStale : ConnectChrome.pillOn
        return HStack(spacing: 10) {
            Text("Feedback")
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.textDim)
                .frame(width: 72, alignment: .leading)
            GeometryReader { proxy in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 3).fill(ChromeColours.sliderTrack)
                    RoundedRectangle(cornerRadius: 3).fill(colour).frame(width: proxy.size.width * share)
                }
            }
            .frame(height: 8)
            ToolPageParts.ValueBox(text: "\(status.feedbackLevel)", width: 44)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Feedback level")
        .accessibilityValue("\(status.feedbackLevel)")
        .accessibilityIdentifier("pureSignal.feedback")
    }
}
