// no-port-check: NereusSDR-original test.
//
// R-R3-37, R-R3-21: a pan's remote display status is a short line that fits
// a narrow pan plus a full explanation for hover, both in user words, and
// the Core's reasons are translated when shown, never echoed raw.

#include <QtTest/QtTest>

#include "OperatorWording.h"
#include "PanStatusSamples.h"

#include "core/session/media/SpectrumEndpoint.h"
#include "gui/OperatorReasonText.h"
#include "gui/PanStatusText.h"

using namespace NereusSDR;

namespace {

PanDisplayState showing(int pixels, int fps, int requestedPixels, int requestedFps)
{
    PanDisplayState state;
    state.phase = PanDisplayState::Phase::Showing;
    state.pixels = pixels;
    state.fps = fps;
    state.requestedPixels = requestedPixels;
    state.requestedFps = requestedFps;
    return state;
}

PanDisplayState phase(PanDisplayState::Phase p)
{
    PanDisplayState state;
    state.phase = p;
    return state;
}

} // namespace

class TstPanStatusText : public QObject {
    Q_OBJECT
private slots:
    void nothingToReportSaysNothing()
    {
        QCOMPARE(buildPanStatusText(PanDisplayState{}), PanStatusText{});
    }

    void fullQualityHasNoShortLineButExplainsOnHover()
    {
        PanDisplayState state = showing(1024, 30, 1024, 30);
        state.receivedFps = 29.46;
        state.extendedView = true;
        const PanStatusText text = buildPanStatusText(state);
        QVERIFY(text.shortLine.isEmpty());
        QCOMPARE(text.explanation,
                 QStringLiteral("Full quality from the Core: 1024 points across, 30 updates "
                                "a second. Arriving at about 29.5 updates a second. Includes "
                                "the extended view."));
    }

    // A cut for the Core's display limit (no reason from the Core) keeps
    // the display-limit wording.
    void reducedQualitySaysWhatWhyAndNext()
    {
        const PanStatusText lessDetail = buildPanStatusText(showing(512, 15, 1024, 30));
        QCOMPARE(lessDetail.shortLine, QStringLiteral("Less detail: Core limit"));
        QVERIFY(lessDetail.explanation.contains(QStringLiteral("512 points across, 15 updates")));
        QVERIFY(lessDetail.explanation.contains(QStringLiteral("1024 points and 30 updates")));
        QVERIFY(lessDetail.explanation.contains(QStringLiteral("comes back by itself")));
        QVERIFY(lessDetail.explanation.contains(QStringLiteral("limit on how much display")));
        QVERIFY(!lessDetail.explanation.contains(QStringLiteral("busy")));

        const PanStatusText slower = buildPanStatusText(showing(1024, 15, 1024, 30));
        QCOMPARE(slower.shortLine, QStringLiteral("Slower: Core limit"));
        QVERIFY(!slower.explanation.contains(QStringLiteral("busy")));
    }

    // Lane B carry (R-R3-08, R-R3-37): a cut the Core made because its
    // computer is busy says "Core busy" in the short line and explanation.
    void coreBusyCutSaysCoreBusy()
    {
        PanDisplayState lessDetail = showing(512, 15, 1024, 30);
        lessDetail.budgetReason = DisplayBudgetReason::CoreBusy;
        PanStatusText text = buildPanStatusText(lessDetail);
        QCOMPARE(text.shortForms(), (QStringList{QStringLiteral("Less detail: Core busy"),
                                                 QStringLiteral("Less detail")}));
        QVERIFY(text.explanation.startsWith(QStringLiteral("The Core computer is busy")));
        QVERIFY(text.explanation.contains(QStringLiteral("512 points across, 15 updates")));
        QVERIFY(text.explanation.contains(QStringLiteral("1024 points and 30 updates")));
        QVERIFY(text.explanation.contains(QStringLiteral("comes back by itself")));

        PanDisplayState slower = showing(1024, 15, 1024, 30);
        slower.budgetReason = DisplayBudgetReason::CoreBusy;
        text = buildPanStatusText(slower);
        QCOMPARE(text.shortLine, QStringLiteral("Slower: Core busy"));
        QVERIFY(text.explanation.startsWith(QStringLiteral("The Core computer is busy")));

        PanDisplayState paused = phase(PanDisplayState::Phase::Paused);
        paused.budgetReason = DisplayBudgetReason::CoreBusy;
        text = buildPanStatusText(paused);
        QCOMPARE(text.shortForms(), (QStringList{QStringLiteral("Paused: Core busy"),
                                                 QStringLiteral("Paused")}));
        QVERIFY(text.explanation.startsWith(QStringLiteral("The Core computer is busy")));
        paused.pureSignalOverLimit = true;
        text = buildPanStatusText(paused);
        QCOMPARE(text.shortLine, QStringLiteral("Paused: Core busy"));
        QVERIFY(text.explanation.startsWith(QStringLiteral("The Core computer is busy")));
        QVERIFY(text.explanation.contains(QStringLiteral("PureSignal")));

        // At the requested quality the reason says nothing: no line.
        PanDisplayState full = showing(1024, 30, 1024, 30);
        full.budgetReason = DisplayBudgetReason::CoreBusy;
        QVERIFY(buildPanStatusText(full).shortLine.isEmpty());
        QVERIFY(!buildPanStatusText(full).explanation.contains(QStringLiteral("busy")));
    }

    void waitingStalledPausedAndFullHaveTheirOwnLines()
    {
        using Phase = PanDisplayState::Phase;
        QCOMPARE(buildPanStatusText(phase(Phase::Waiting)).shortLine,
                 QStringLiteral("Waiting for the Core"));
        QCOMPARE(buildPanStatusText(phase(Phase::ChangingWindow)).shortLine,
                 QStringLiteral("Waiting for the Core"));
        QCOMPARE(buildPanStatusText(phase(Phase::Stalled)).shortLine,
                 QStringLiteral("Core not answering"));
        QCOMPARE(buildPanStatusText(phase(Phase::Paused)).shortLine,
                 QStringLiteral("Paused: Core limit"));
        QCOMPARE(buildPanStatusText(phase(Phase::TooManyPans)).shortLine,
                 QStringLiteral("Too many pans"));
        PanDisplayState overLimit = phase(Phase::Paused);
        overLimit.pureSignalOverLimit = true;
        const PanStatusText text = buildPanStatusText(overLimit);
        QCOMPARE(text.shortLine, QStringLiteral("Paused: Core limit"));
        QVERIFY(text.explanation.contains(QStringLiteral("PureSignal")));
        QVERIFY(text.explanation != buildPanStatusText(phase(Phase::Paused)).explanation);
    }

    void refusalIsTranslatedNeverEchoed()
    {
        PanDisplayState refused = phase(PanDisplayState::Phase::Refused);
        refused.refusalReason = QStringLiteral("requested crop is outside source coverage");
        const PanStatusText text = buildPanStatusText(refused);
        QCOMPARE(text.shortLine, QStringLiteral("Refused: out of range"));
        QVERIFY(text.explanation.contains(
            QStringLiteral("This view reaches past the frequencies the receiver covers.")));
        QVERIFY(!text.explanation.contains(refused.refusalReason));

        refused.refusalReason = QStringLiteral("endpoint ledger overflow 42");
        const PanStatusText unknown = buildPanStatusText(refused);
        QCOMPARE(unknown.shortLine, QStringLiteral("Refused by the Core"));
        QVERIFY(!unknown.explanation.contains(QStringLiteral("ledger")));
        QVERIFY(!unknown.explanation.contains(QStringLiteral("42")));
    }

    void pureSignalAndZoomLinesGiveWayToThePansOwnLine()
    {
        PanDisplayState state = showing(1024, 30, 1024, 30);
        state.pureSignal = PanDisplayState::PureSignal::Refused;
        state.pureSignalRefusalReason =
            QStringLiteral("PureSignal display does not fit the session display budget");
        state.zoomLimit = PanDisplayState::ZoomLimit::SourceBins;
        state.zoomPoints = 128;
        PanStatusText text = buildPanStatusText(state);
        QCOMPARE(text.shortLine, QStringLiteral("PureSignal: refused"));
        QVERIFY(text.explanation.contains(QStringLiteral("PureSignal display did not start")));
        QVERIFY(text.explanation.contains(QStringLiteral("shows 128 points")));

        state.pureSignal = PanDisplayState::PureSignal::Fine;
        QCOMPARE(buildPanStatusText(state).shortLine, QStringLiteral("Showing 128 points"));

        state.pixels = 512;
        text = buildPanStatusText(state);
        QCOMPARE(text.shortLine, QStringLiteral("Less detail: Core limit"));
        QVERIFY(text.explanation.contains(QStringLiteral("shows 128 points")));

        PanDisplayState zoomOnly;
        zoomOnly.zoomLimit = PanDisplayState::ZoomLimit::LargestSize;
        QCOMPARE(buildPanStatusText(zoomOnly).shortLine, QStringLiteral("Finest detail reached"));
        zoomOnly.zoomLimit = PanDisplayState::ZoomLimit::SharedEngine;
        QCOMPARE(buildPanStatusText(zoomOnly).shortLine, QStringLiteral("Less detail: shared"));
    }

    void everyBuilderOutputIsPlain()
    {
        for (const PanDisplayState& state : PanStatusSamples::all()) {
            const PanStatusText text = buildPanStatusText(state);
            if (state.phase == PanDisplayState::Phase::None
                && state.zoomLimit == PanDisplayState::ZoomLimit::None
                && !state.transmitDisplayMissing) {
                QCOMPARE(text, PanStatusText{});
                continue;
            }
            QVERIFY2(text.explanation.isEmpty() || OperatorWording::isPlain(text.explanation),
                     qPrintable(text.explanation + QStringLiteral(" [")
                                + OperatorWording::internalTermIn(text.explanation)
                                + QStringLiteral("]")));
            QVERIFY2(text.shortLine.isEmpty() || OperatorWording::isPlain(text.shortLine),
                     qPrintable(text.shortLine));
            QVERIFY2(!text.explanation.isEmpty(), qPrintable(text.shortLine));
            QVERIFY(!text.explanation.contains(QStringLiteral("%1")));
        }
    }

    // I1: every short line comes with shorter forms, longest first, each
    // plain; the last is at most a word or two so it fits a narrow pan.
    void everyShortLineHasOrderedShorterForms()
    {
        int checked = 0;
        for (const PanDisplayState& state : PanStatusSamples::all()) {
            const PanStatusText text = buildPanStatusText(state);
            const QStringList forms = text.shortForms();
            if (text.shortLine.isEmpty()) {
                QVERIFY(forms.isEmpty());
                QVERIFY(text.shorterForms.isEmpty());
                continue;
            }
            ++checked;
            QCOMPARE(forms.constFirst(), text.shortLine);
            for (qsizetype i = 0; i < forms.size(); ++i) {
                QVERIFY2(OperatorWording::isPlain(forms.at(i)), qPrintable(forms.at(i)));
                if (i > 0) {
                    QVERIFY2(forms.at(i).size() < forms.at(i - 1).size(),
                             qPrintable(forms.join(QStringLiteral(" | "))));
                }
            }
            QVERIFY2(forms.constLast().size() <= 13,
                     qPrintable(forms.join(QStringLiteral(" | "))));
        }
        QVERIFY2(checked >= 200, qPrintable(QString::number(checked)));
    }

    void namedShorterForms()
    {
        using Phase = PanDisplayState::Phase;
        QCOMPARE(buildPanStatusText(showing(512, 15, 1024, 30)).shortForms(),
                 (QStringList{QStringLiteral("Less detail: Core limit"),
                              QStringLiteral("Less detail")}));
        QCOMPARE(buildPanStatusText(showing(1024, 15, 1024, 30)).shortForms(),
                 (QStringList{QStringLiteral("Slower: Core limit"), QStringLiteral("Slower")}));
        QCOMPARE(buildPanStatusText(phase(Phase::Waiting)).shortForms(),
                 (QStringList{QStringLiteral("Waiting for the Core"), QStringLiteral("Waiting")}));
        PanDisplayState refused = phase(Phase::Refused);
        refused.refusalReason = QStringLiteral("requested crop is outside source coverage");
        QCOMPARE(buildPanStatusText(refused).shortForms(),
                 (QStringList{QStringLiteral("Refused: out of range"),
                              QStringLiteral("Refused")}));
        refused.refusalReason = QStringLiteral("a reason this app has never seen");
        QCOMPARE(buildPanStatusText(refused).shortForms(),
                 (QStringList{QStringLiteral("Refused by the Core"), QStringLiteral("Refused")}));
        PanDisplayState zoom;
        zoom.zoomLimit = PanDisplayState::ZoomLimit::SourceBins;
        zoom.zoomPoints = 16384;
        QCOMPARE(buildPanStatusText(zoom).shortForms(),
                 (QStringList{QStringLiteral("Showing 16384 points"),
                              QStringLiteral("16384 points")}));
    }

    // M5: this app's own pan limit names this app and says what to do; the
    // Core's refusals keep "asks again when you change this pan's view".
    void thisAppsPanLimitSaysCloseAPan()
    {
        PanDisplayState refused = phase(PanDisplayState::Phase::Refused);
        refused.refusalReason =
            QStringLiteral("At most eight remote display pans are supported.");
        const PanStatusText text = buildPanStatusText(refused);
        QCOMPARE(text.shortLine, QStringLiteral("Too many pans"));
        QCOMPARE(text.explanation,
                 QStringLiteral("This pan's display request did not go through. This app "
                                "shows the Core's display on up to eight pans at once. "
                                "Close a pan to show this one."));
        refused.refusalReason = QStringLiteral("session display budget exceeded");
        QVERIFY(buildPanStatusText(refused).explanation.endsWith(
            QStringLiteral("It asks again when you change this pan's view.")));
    }

    void everyTranslationIsPlainAndNeverTheRawReason()
    {
        QStringList reasons = OperatorReasonText::knownReasons();
        reasons << QStringLiteral("session endpoint budget allocation revision 7");
        for (const QString& reason : reasons) {
            const QString sentence = OperatorReasonText::forDisplay(reason);
            const QString shortLine = OperatorReasonText::shortForDisplay(reason);
            QCOMPARE(OperatorReasonText::shortFormsForDisplay(reason).constFirst(), shortLine);
            QVERIFY2(OperatorWording::isPlain(OperatorReasonText::panNextStep(reason)),
                     qPrintable(reason));
            QVERIFY2(OperatorWording::isPlain(sentence),
                     qPrintable(reason + QStringLiteral(" -> ") + sentence));
            QVERIFY2(OperatorWording::isPlain(shortLine),
                     qPrintable(reason + QStringLiteral(" -> ") + shortLine));
            QVERIFY2(sentence != reason, qPrintable(reason));
        }
    }

    void theWireReasonsAreUnchanged()
    {
        // The first two are compared as exact text by apps in use; the
        // table translates them only for display. The source retune is in
        // operator words (R-IOS-01).
        QCOMPARE(QLatin1String(kRetireReasonSliceRemoved), QLatin1String("slice removed"));
        QCOMPARE(QLatin1String(kRetireReasonStreamBindingChanged),
                 QLatin1String("slice stream binding changed"));
        QCOMPARE(QLatin1String(kRetireReasonSourceRetune),
                 QLatin1String("The receiver was retuned away from this view."));
        for (const char* reason : {kRetireReasonSliceRemoved, kRetireReasonStreamBindingChanged,
                                   kRetireReasonSourceRetune}) {
            QVERIFY(OperatorReasonText::knownReasons().contains(QLatin1String(reason)));
        }
    }

    void theSharedCheckCatchesInternalTerms()
    {
        QVERIFY(!OperatorWording::isPlain(QString()));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("   ")));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("Display allocation refused")));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("WIDE plane reserved")));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("Station capabilities changed")));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("SSRC mismatch")));
        QVERIFY(!OperatorWording::isPlain(QStringLiteral("Media peers lost")));
        QVERIFY(OperatorWording::isPlain(QStringLiteral("Paused: Core busy")));
        QVERIFY(OperatorWording::isPlain(QStringLiteral("Paused: Core limit")));
        // Word starts only: an airplane is not a plane term.
        QVERIFY(OperatorWording::isPlain(QStringLiteral("Explanation for the airplane mode")));
    }
};

QTEST_GUILESS_MAIN(TstPanStatusText)
#include "tst_pan_status_text.moc"
