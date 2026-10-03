// no-port-check: NereusSDR-original. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanStatusText.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
// =================================================================

#include "gui/PanStatusText.h"

#include "gui/OperatorReasonText.h"

#include <QStringList>

namespace NereusSDR {
namespace {

using Phase = PanDisplayState::Phase;

// Each short line comes with shorter forms. The pan paints the longest form
// that fits its row, measured in the font it paints with, and never an
// elided one; the last form is a word or two so it fits any usable pan
// (tst_pan_status_overlay and tst_spectrum_status_overlay check both). A
// reduction or pause never comes from the network. When the Core says its
// computer is busy (DisplayBudgetReason::CoreBusy) the line says "Core busy";
// otherwise it is the Core's display limit and the line says "Core limit"
// (R-R3-08, R-R3-37).

PanStatusText withForms(const QStringList& forms, const QString& explanation)
{
    return {forms.value(0), explanation, forms.mid(1)};
}

// The pan's own display: what happened, why, and what happens next.
PanStatusText displayText(const PanDisplayState& state)
{
    switch (state.phase) {
    case Phase::None:
        return {};
    case Phase::Showing: {
        const bool coreBusy = state.budgetReason == DisplayBudgetReason::CoreBusy;
        QString explanation;
        if (state.reduced() && coreBusy) {
            explanation = QStringLiteral(
                "The Core computer is busy, so the Core is sending %1 points across, %2 "
                "updates a second, instead of the %3 points and %4 updates a second this "
                "pan asked for. That keeps its audio and receivers running smoothly. Full "
                "quality comes back by itself when the Core has room.")
                .arg(state.pixels).arg(state.fps)
                .arg(state.requestedPixels).arg(state.requestedFps);
        } else if (state.reduced()) {
            explanation = QStringLiteral(
                "The Core is sending %1 points across, %2 updates a second, instead of "
                "the %3 points and %4 updates a second this pan asked for. The Core has "
                "a limit on how much display it sends, and your open pans share it. "
                "Full quality comes back by itself when there is room.")
                .arg(state.pixels).arg(state.fps)
                .arg(state.requestedPixels).arg(state.requestedFps);
        } else {
            explanation = QStringLiteral(
                "Full quality from the Core: %1 points across, %2 updates a second.")
                .arg(state.pixels).arg(state.fps);
        }
        if (state.receivedFps >= 0.0) {
            explanation += QStringLiteral(" Arriving at about %1 updates a second.")
                .arg(state.receivedFps, 0, 'f', 1);
        }
        if (state.extendedView) {
            explanation += QStringLiteral(" Includes the extended view.");
        }
        if (!state.reduced()) {
            return {QString(), explanation, {}};
        }
        if (state.pixels != state.requestedPixels) {
            return withForms({coreBusy ? QStringLiteral("Less detail: Core busy")
                                       : QStringLiteral("Less detail: Core limit"),
                              QStringLiteral("Less detail")},
                             explanation);
        }
        return withForms({coreBusy ? QStringLiteral("Slower: Core busy")
                                   : QStringLiteral("Slower: Core limit"),
                          QStringLiteral("Slower")},
                         explanation);
    }
    case Phase::Waiting:
        return withForms({QStringLiteral("Waiting for the Core"), QStringLiteral("Waiting")},
                         QStringLiteral("This pan has asked the Core for its display. "
                                        "It appears as soon as the Core answers."));
    case Phase::ChangingWindow:
        return withForms({QStringLiteral("Waiting for the Core"), QStringLiteral("Waiting")},
                         QStringLiteral("This receiver's spectrum settings changed, so this "
                                        "pan is asking the Core for a display that matches. "
                                        "The last picture stays until the Core answers."));
    case Phase::Stalled:
        return withForms({QStringLiteral("Core not answering"), QStringLiteral("No answer")},
                         QStringLiteral("This pan asked the Core for its display and the "
                                        "answer is overdue. It keeps waiting, and the display "
                                        "comes back as soon as the Core answers."));
    case Phase::Paused: {
        const bool coreBusy = state.budgetReason == DisplayBudgetReason::CoreBusy;
        const QStringList forms{coreBusy ? QStringLiteral("Paused: Core busy")
                                         : QStringLiteral("Paused: Core limit"),
                                QStringLiteral("Paused")};
        if (state.pureSignalOverLimit && coreBusy) {
            return withForms(forms,
                             QStringLiteral("The Core computer is busy, so it lowered how much "
                                            "display it sends, and the PureSignal display "
                                            "already running uses more than that. This pan's "
                                            "display is paused and the last picture is held. "
                                            "It resumes when the PureSignal display closes or "
                                            "the Core has room again."));
        }
        if (state.pureSignalOverLimit) {
            return withForms(forms,
                             QStringLiteral("The PureSignal display already running uses more "
                                            "than the Core's current display limit, so this "
                                            "pan's display is paused and the last picture is "
                                            "held. It resumes when the PureSignal display "
                                            "closes or the limit rises."));
        }
        if (coreBusy) {
            return withForms(forms,
                             QStringLiteral("The Core computer is busy, so this pan's display "
                                            "is paused to keep its audio and receivers running "
                                            "smoothly, and the last picture is held. It resumes "
                                            "by itself when the Core has room."));
        }
        return withForms(forms,
                         QStringLiteral("The Core's display limit has no room for this pan "
                                        "right now, so the last picture is held. It resumes "
                                        "by itself when there is room, for example when "
                                        "another pan closes or gets smaller."));
    }
    case Phase::Refused:
        return withForms(OperatorReasonText::shortFormsForDisplay(state.refusalReason),
                         QStringLiteral("This pan's display request did not go through. %1 %2")
                             .arg(OperatorReasonText::forDisplay(state.refusalReason),
                                  OperatorReasonText::panNextStep(state.refusalReason)));
    case Phase::TooManyPans:
        return withForms({QStringLiteral("Too many pans")},
                         QStringLiteral("This computer is already showing the Core's display "
                                        "on as many pans as it can. This pan's display starts "
                                        "when another pan closes."));
    }
    return {};
}

PanStatusText pureSignalText(const PanDisplayState& state)
{
    switch (state.pureSignal) {
    case PanDisplayState::PureSignal::Fine:
        return {};
    case PanDisplayState::PureSignal::Refused:
        return withForms({QStringLiteral("PureSignal: refused"), QStringLiteral("PS: refused"),
                          QStringLiteral("Refused")},
                         QStringLiteral("The PureSignal display did not start. %1 Your pan "
                                        "displays carry on as before.")
                             .arg(OperatorReasonText::forDisplay(
                                 state.pureSignalRefusalReason)));
    case PanDisplayState::PureSignal::Stalled:
        return withForms({QStringLiteral("PureSignal: no answer"),
                          QStringLiteral("PS: no answer"), QStringLiteral("No answer")},
                         QStringLiteral("A change to the PureSignal display is waiting for "
                                        "the Core, and its answer is overdue. It takes effect "
                                        "as soon as the Core answers."));
    }
    return {};
}

PanStatusText zoomText(const PanDisplayState& state)
{
    switch (state.zoomLimit) {
    case PanDisplayState::ZoomLimit::None:
        return {};
    case PanDisplayState::ZoomLimit::LargestSize:
        return withForms({QStringLiteral("Finest detail reached"),
                          QStringLiteral("Finest detail")},
                         QStringLiteral("You have zoomed in as far as the Core can add "
                                        "detail. Zooming in further enlarges the same "
                                        "points."));
    case PanDisplayState::ZoomLimit::SharedEngine:
        return withForms({QStringLiteral("Less detail: shared"),
                          QStringLiteral("Less detail")},
                         QStringLiteral("This receiver's spectrum is shared with another "
                                        "pan, so this pan gets the detail that pan's setting "
                                        "gives. It gets its own detail when the spectrum is "
                                        "no longer shared."));
    case PanDisplayState::ZoomLimit::SourceBins:
        return withForms({QStringLiteral("Showing %1 points").arg(state.zoomPoints),
                          QStringLiteral("%1 points").arg(state.zoomPoints)},
                         QStringLiteral("The receiver has no finer detail at this zoom, so "
                                        "this pan shows %1 points stretched to fit. Zooming "
                                        "out brings back full detail.")
                             .arg(state.zoomPoints));
    }
    return {};
}

// Parity Task 29 (A11): a transmitting pan whose Core sends no transmit
// display holds its last picture; the line says why and what helps, in
// forms that fit the pan's row (the whole sentence on hover).
PanStatusText transmitDisplayText(const PanDisplayState& state)
{
    if (!state.transmitDisplayMissing) {
        return {};
    }
    return withForms({QStringLiteral("No transmit display: update the Core"),
                      QStringLiteral("No transmit display"),
                      QStringLiteral("Update Core")},
                     QStringLiteral("This Core does not send its transmit display. "
                                    "Updating the Core may help."));
}

} // namespace

PanStatusText buildPanStatusText(const PanDisplayState& state)
{
    // The pan's own display speaks first, then the PureSignal display, then
    // the zoom detail. The short line (with its shorter forms) is the first
    // that has one; the explanation carries all of them.
    PanStatusText text;
    QStringList paragraphs;
    for (const PanStatusText& part :
         {transmitDisplayText(state), displayText(state), pureSignalText(state),
          zoomText(state)}) {
        if (text.shortLine.isEmpty()) {
            text.shortLine = part.shortLine;
            text.shorterForms = part.shorterForms;
        }
        if (!part.explanation.isEmpty()) {
            paragraphs.append(part.explanation);
        }
    }
    text.explanation = paragraphs.join(QLatin1Char('\n'));
    return text;
}

} // namespace NereusSDR
