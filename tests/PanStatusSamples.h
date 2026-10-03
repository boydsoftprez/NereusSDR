// no-port-check: NereusSDR-original test helper.
//
// One of every pan display state the status builder words (R-R3-37), for the
// tests that check the wording is plain and that each short line fits a
// narrow pan.
#pragma once

#include "gui/OperatorReasonText.h"
#include "gui/PanStatusText.h"

#include <QList>
#include <QString>

namespace NereusSDR::PanStatusSamples {

inline QList<PanDisplayState> all()
{
    using Phase = PanDisplayState::Phase;
    QList<PanDisplayState> states;
    const auto phase = [](Phase p) {
        PanDisplayState state;
        state.phase = p;
        return state;
    };
    const auto showing = [&phase](int pixels, int fps, int requestedPixels, int requestedFps) {
        PanDisplayState state = phase(Phase::Showing);
        state.pixels = pixels;
        state.fps = fps;
        state.requestedPixels = requestedPixels;
        state.requestedFps = requestedFps;
        state.receivedFps = 29.5;
        state.extendedView = true;
        return state;
    };

    states << phase(Phase::None);
    states << showing(1024, 30, 1024, 30);   // full quality
    states << showing(512, 15, 1024, 30);    // less detail
    states << showing(1024, 15, 1024, 30);   // slower
    states << phase(Phase::Waiting) << phase(Phase::ChangingWindow)
           << phase(Phase::Stalled) << phase(Phase::Paused) << phase(Phase::TooManyPans);
    PanDisplayState overLimit = phase(Phase::Paused);
    overLimit.pureSignalOverLimit = true;
    states << overLimit;
    // Lane B carry (R-R3-08, R-R3-37): the same cuts when the Core says its
    // computer is busy.
    for (PanDisplayState busy : {showing(512, 15, 1024, 30), showing(1024, 15, 1024, 30),
                                 phase(Phase::Paused), overLimit}) {
        busy.budgetReason = DisplayBudgetReason::CoreBusy;
        states << busy;
    }

    QStringList reasons = OperatorReasonText::knownReasons();
    reasons << QStringLiteral("a reason this app has never seen") << QString();
    for (const QString& reason : reasons) {
        PanDisplayState refused = phase(Phase::Refused);
        refused.refusalReason = reason;
        states << refused;
        PanDisplayState pureSignal = showing(1024, 30, 1024, 30);
        pureSignal.pureSignal = PanDisplayState::PureSignal::Refused;
        pureSignal.pureSignalRefusalReason = reason;
        states << pureSignal;
    }
    PanDisplayState stalled = showing(1024, 30, 1024, 30);
    stalled.pureSignal = PanDisplayState::PureSignal::Stalled;
    states << stalled;

    for (PanDisplayState::ZoomLimit limit :
         {PanDisplayState::ZoomLimit::LargestSize, PanDisplayState::ZoomLimit::SharedEngine,
          PanDisplayState::ZoomLimit::SourceBins}) {
        PanDisplayState zoom;
        zoom.zoomLimit = limit;
        zoom.zoomPoints = 16384;
        states << zoom;
        PanDisplayState zoomShowing = showing(1024, 30, 1024, 30);
        zoomShowing.zoomLimit = limit;
        zoomShowing.zoomPoints = 16384;
        states << zoomShowing;
    }
    // Parity Task 29 (A11): a transmitting pan on a Core that sends no
    // transmit display, alone and over a showing display.
    PanDisplayState noTransmitDisplay;
    noTransmitDisplay.transmitDisplayMissing = true;
    states << noTransmitDisplay;
    PanDisplayState noTransmitDisplayShowing = showing(512, 15, 1024, 30);
    noTransmitDisplayShowing.transmitDisplayMissing = true;
    states << noTransmitDisplayShowing;
    return states;
}

} // namespace NereusSDR::PanStatusSamples
