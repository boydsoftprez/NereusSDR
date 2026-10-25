// no-port-check: NereusSDR-original regression test.
// =================================================================
// tests/tst_slice_filter_constrain.cpp  (NereusSDR)
// =================================================================
//
// Filter edges are constrained as Thetis's ConstrainFilter does
// (console.cs:35044-35131 [v2.10.3.15]). Thetis runs it in
// UpdateRX1Filters / UpdateRX2Filters (console.cs:7580, 7700
// [v2.10.3.15]), the one path every receive filter change takes, and
// then refuses a zero-width result (console.cs:7582 [v2.10.3.15]). The whole-passband
// moves (filter shift and width sliders, whole-filter drag, the meter
// filter drag) call it first with filterShift = true so the width is
// kept (console.cs:35202, 35420, 50635, 50711; MeterManager.cs:19399 [v2.10.3.15]).
//
// Two rules:
//   sideband - an edge may not cross 0 Hz the wrong way for the mode;
//              only when "Limit filter edges to sidebands" is on, which
//              is off by default (console.cs:7516 [v2.10.3.15]).
//   cap      - |edge| <= MaxFilterShift and MaxFilterWidth, both 10000
//              by default (console.cs:13208, 13221 [v2.10.3.15]); not in FM.
//
// Before this test SliceModel::setFilter took any value: a 250 kHz
// filter, a zero-width filter, and an LSB edge across zero with the
// limit on.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Claude Code. Inputs of
//                 several cases from Longpath tests/tst_filter_constrain.cpp
//                 (see the banner below).
//   2026-10-22 - The 10 Hz minimum width (console.cs:7544-7567
//                 [v2.10.3.15]), the resync of a constrained request, a
//                 mode change that constrains to zero width, and
//                 filterForRequest. J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Claude Code.
//   2026-10-24 - The RADE modes follow their sideband with the limit on
//                 and are unchanged with it off (NereusSDR's own
//                 extension, JJ's ruling). J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Claude Code.
//   2026-10-24 - shiftFilter re-sends the stored pair when a move held at
//                 the cap leaves it in place. J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Claude Code.
// =================================================================

// --- From tst_filter_constrain.cpp (Longpath) ---
// Several cases take their inputs from Longpath
// tests/tst_filter_constrain.cpp:53-214 [@383f058] (commit 6a5df5f1).
// Longpath's banner for that file, byte for byte:
// =================================================================
// tests/tst_filter_constrain.cpp  (Longpath)
// =================================================================
//
// Die Filterkanten duerfen nicht ueberall hin.
//
// GEFUNDEN AM 2026-08-20 beim Lesen von Thetis: SliceModel::setFilter
// nahm JEDEN Wert an. Wer in LSB die obere Kante nach rechts ueber die
// Null zog, bekam einen Filter, der in das andere Seitenband
// hineinreicht — und nichts hielt ihn auf. Auch kein Deckel: 200 kHz
// Bandbreite waren moeglich.
//
// Thetis hat dafuer ConstrainFilter (console.cs:34974-35062,
// v2.10.3.15-5-g852bf0e) und laesst JEDE Filteraenderung hindurch, in
// UpdateRX1Filters (console.cs:7510) — dem einen Trichter, durch den
// alles muss.
//
// Zwei Regeln, und nur die erste ist abschaltbar (in Thetis sogar
// VORGABE AUS, console.cs:7446):
//
//   Seitenband — je nach Betriebsart darf eine Kante die Null nicht
//                ueberschreiten.
//   Deckel     — |Kante| <= 10 000 Hz, ausser bei FM. Gilt immer.
//
// Der Unterschied zwischen KANTE ZIEHEN und DURCHLASS VERSCHIEBEN
// steckt in filterShift: beim Verschieben soll die Breite erhalten
// bleiben, also wandert die andere Kante mit, wenn eine anstoesst.
// Ohne diesen Fall waere ein verschobener Durchlass am Rand ploetzlich
// schmaler — und das faellt beim Hoeren auf, nicht beim Lesen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceFilterConstrain : public QObject
{
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // -- The 10 kHz cap: always on, every mode but FM --------------------

    void capClampsBothEdges()
    {
        int low = -100000;
        int high = 100000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB));
        QCOMPARE(low, -10000);
        QCOMPARE(high, 10000);
    }

    void capLeavesANormalFilterAlone()
    {
        int low = -2850;
        int high = -150;
        QVERIFY(!SliceModel::constrainFilter(low, high, DSPMode::LSB));
        QCOMPARE(low, -2850);
        QCOMPARE(high, -150);
    }

    void fmIsNotConstrained()
    {
        int low = -50000;
        int high = 50000;
        QVERIFY(!SliceModel::constrainFilter(low, high, DSPMode::FM, false, true));
        QCOMPARE(low, -50000);
        QCOMPARE(high, 50000);
    }

    void setFilterAppliesTheCap()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 250000);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 10000);
    }

    void singleEdgeSettersApplyTheCap()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        s.setFilterHigh(40000);
        QCOMPARE(s.filterHigh(), 10000);
        s.setFilterLow(-40000);
        QCOMPARE(s.filterLow(), -10000);
    }

    void capKeepsTheWidthWhenShifting()
    {
        // A 3000 Hz passband moved past +10 kHz: with filterShift the other
        // edge moves by the same amount.
        int low = 8000;
        int high = 11000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB, true));
        QCOMPARE(high, 10000);
        QCOMPARE(low, 7000);
    }

    void shiftFilterKeepsTheWidth()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 3100);
        s.shiftFilter(8000, 11000);
        QCOMPARE(s.filterLow(), 7000);
        QCOMPARE(s.filterHigh(), 10000);
    }

    // -- Sideband crossing: only with the limit on ------------------------

    void limitIsOffByDefault()
    {
        SliceModel s;
        QVERIFY(!s.limitFiltersToSidebands());
        s.setDspMode(DSPMode::LSB);
        s.setFilter(-2850, 500);
        QCOMPARE(s.filterLow(), -2850);
        QCOMPARE(s.filterHigh(), 500);
    }

    void lsbUpperEdgeStopsAtZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-2850, 500);
        QCOMPARE(s.filterLow(), -2850);
        QCOMPARE(s.filterHigh(), 0);
    }

    void usbLowerEdgeStopsAtZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-400, 2700);
        QCOMPARE(s.filterLow(), 0);
        QCOMPARE(s.filterHigh(), 2700);
    }

    void singleEdgeSetterCannotCrossZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-2850, -150);
        s.setFilterHigh(900);
        QCOMPARE(s.filterHigh(), 0);
    }

    void cwAndDigitalFollowTheirSideband()
    {
        int low = -600;
        int high = 200;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::CWL, false, true));
        QCOMPARE(high, 0);

        low = -200;
        high = 600;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::DIGU, false, true));
        QCOMPARE(low, 0);
    }

    // NereusSDR extension (JJ's ruling 2026-10-24): Thetis has no RADE. With
    // the limit on, an upper RADE mode stops its lower edge at the carrier
    // as USB does, and a lower one its upper edge as LSB does; with it off
    // nothing changes for them.
    void radeFollowsItsSidebandOnlyWithTheLimitOn()
    {
        {
            SliceModel s;
            s.setDspMode(DSPMode::RADE_U);
            QCOMPARE(s.dspMode(), DSPMode::RADE_U);
            s.setLimitFiltersToSidebands(true);
            s.setFilter(-400, 2350);
            QCOMPARE(s.filterLow(), 0);
            QCOMPARE(s.filterHigh(), 2350);
        }
        {
            SliceModel s;
            s.setDspMode(DSPMode::RADE_L);
            QCOMPARE(s.dspMode(), DSPMode::RADE_L);
            s.setLimitFiltersToSidebands(true);
            s.setFilter(-2350, 400);
            QCOMPARE(s.filterLow(), -2350);
            QCOMPARE(s.filterHigh(), 0);
        }
        {
            SliceModel s;
            s.setDspMode(DSPMode::RADE_U);
            QCOMPARE(s.dspMode(), DSPMode::RADE_U);
            s.setFilter(-400, 2350);
            QCOMPARE(s.filterLow(), -400);
            QCOMPARE(s.filterHigh(), 2350);
        }
        {
            SliceModel s;
            s.setDspMode(DSPMode::RADE_L);
            QCOMPARE(s.dspMode(), DSPMode::RADE_L);
            s.setFilter(-2350, 400);
            QCOMPARE(s.filterLow(), -2350);
            QCOMPARE(s.filterHigh(), 400);
        }
        // The other two (RADE v2, which a slice takes only when the
        // experimental opt-in is on), through the static function.
        int low = -300;
        int high = 2000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::RADE2_U, false, true));
        QCOMPARE(low, 0);
        low = -2000;
        high = 300;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::RADE2_L, false, true));
        QCOMPARE(high, 0);
        low = -2000;
        high = 300;
        QVERIFY(!SliceModel::constrainFilter(low, high, DSPMode::RADE2_L, false, false));
        QCOMPARE(high, 300);
    }

    void amKeepsZeroInsideThePassband()
    {
        int low = 200;
        int high = 4000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::AM, false, true));
        QCOMPARE(low, 0);
        QCOMPARE(high, 4000);

        low = -4000;
        high = -200;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::SAM, false, true));
        QCOMPARE(low, -4000);
        QCOMPARE(high, 0);
    }

    void sidebandShiftKeepsTheWidth()
    {
        // A 2400 Hz LSB passband shifted 500 Hz past zero.
        int low = -1900;
        int high = 500;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::LSB, true, true));
        QCOMPARE(high, 0);
        QCOMPARE(low, -2400);
    }

    // -- Zero width -------------------------------------------------------

    void setFilterRefusesZeroWidth()
    {
        // Both edges past the cap: (12000, 15000) becomes (10000, 10000)
        // and is refused. The stored pair is re-sent so a view that drew
        // the request follows the slice.
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(12000, 15000);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 2900);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 100);
        QCOMPARE(spy.first().at(1).toInt(), 2900);
    }

    // -- A constrained request re-sends the stored pair ---------------------

    void aRequestHeldAtTheCapResendsTheStoredPair()
    {
        // A drag past the cap twice: the first moves the slice to the cap,
        // the second leaves it there. Both times filterChanged carries what
        // the slice holds, never the request.
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(100, 11000);
        s.setFilter(100, 12000);
        QCOMPARE(s.filterHigh(), 10000);
        QCOMPARE(spy.count(), 2);
        for (const QList<QVariant>& emitted : spy) {
            QCOMPARE(emitted.at(0).toInt(), 100);
            QCOMPARE(emitted.at(1).toInt(), 10000);
        }
    }

    void aShiftHeldAtTheCapResendsTheStoredPair()
    {
        // A centre drag that goes on past the cap with the slice already
        // there: shiftFilter keeps 8100..10000 and re-sends it, so a view
        // that drew the request drops it. A shift that asks for the stored
        // pair itself sends nothing.
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(8100, 10000);
        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.shiftFilter(9200, 11100);
        QCOMPARE(s.filterLow(), 8100);
        QCOMPARE(s.filterHigh(), 10000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 8100);
        QCOMPARE(spy.at(0).at(1).toInt(), 10000);
        s.shiftFilter(8100, 10000);
        QCOMPARE(spy.count(), 1);
    }

    void anUnchangedRequestEmitsNothing()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(100, 2900);
        QCOMPARE(spy.count(), 0);
    }

    // -- The 10 Hz minimum width (UpdateRX1Filters, before the constraint) --

    void zeroWidthRequestWidensToTenHz()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        s.setFilter(1500, 1500);
        QCOMPARE(s.filterLow(), 1500);
        QCOMPARE(s.filterHigh(), 1510);

        s.setDspMode(DSPMode::LSB);
        s.setFilter(-1500, -1500);
        QCOMPARE(s.filterLow(), -1510);
        QCOMPARE(s.filterHigh(), -1500);
    }

    void minimumWidthMovesTheEdgeThetisMoves()
    {
        // Sideband modes move the edge away from the carrier side Thetis
        // names; an inverted pair is widened the same way.
        int low = 3000;
        int high = 100;
        SliceModel::applyMinimumFilterWidth(low, high, DSPMode::USB);
        QCOMPARE(low, 3000);
        QCOMPARE(high, 3010);

        low = -100;
        high = -3000;
        SliceModel::applyMinimumFilterWidth(low, high, DSPMode::CWL);
        QCOMPARE(low, -3010);
        QCOMPARE(high, -3000);

        // AM-type: the edge nearer zero is the one kept.
        low = 500;
        high = -2000;
        SliceModel::applyMinimumFilterWidth(low, high, DSPMode::AM);
        QCOMPARE(low, -2010);
        QCOMPARE(high, -2000);

        low = 2000;
        high = -500;
        SliceModel::applyMinimumFilterWidth(low, high, DSPMode::DSB);
        QCOMPARE(low, 2000);
        QCOMPARE(high, 2010);

        // Wide enough already: untouched.
        low = 100;
        high = 110;
        SliceModel::applyMinimumFilterWidth(low, high, DSPMode::DIGU);
        QCOMPARE(low, 100);
        QCOMPARE(high, 110);

        // FM's case is not ported, and DRM is not in Thetis's switch.
        for (const DSPMode mode : {DSPMode::FM, DSPMode::DRM}) {
            low = 3000;
            high = 100;
            SliceModel::applyMinimumFilterWidth(low, high, mode);
            QCOMPARE(low, 3000);
            QCOMPARE(high, 100);
        }
    }

    void setFilterHandlesAnInvertedPairAsThetis()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(3000, 100);
        QCOMPARE(s.filterLow(), 3000);
        QCOMPARE(s.filterHigh(), 3010);

        s.setDspMode(DSPMode::AM);
        s.setFilter(500, -2000);
        QCOMPARE(s.filterLow(), -2010);
        QCOMPARE(s.filterHigh(), -2000);
    }

    void filterForRequestIsWhatSetFilterKeeps()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        QCOMPARE(s.filterForRequest(100, 12000), std::make_pair(100, 10000));
        QCOMPARE(s.filterForRequest(1500, 1500), std::make_pair(1500, 1510));
        // Refused: the current pair.
        QCOMPARE(s.filterForRequest(12000, 15000), std::make_pair(100, 2900));
        QCOMPARE(s.filterForRequest(200, 2400), std::make_pair(200, 2400));
    }

    void setFilterRefusesAFilterConstrainedToZeroWidth()
    {
        // USB with the limit on: (-500, 0) becomes (0, 0).
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(100, 2900);
        s.setFilter(-500, 0);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 2900);
    }

    // -- A mode change takes the same path (SetRX1Mode -> UpdateRX1Filters) --

    void modeChangeConstrainsTheSavedFilter()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        const QString prefix = QStringLiteral("Slice%1/Band%2/Mode%3/")
                                   .arg(s.sliceIndex())
                                   .arg(bandKeyName(Band::Band20m))
                                   .arg(SliceModel::modeName(DSPMode::LSB));
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterLow"), -60000);
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterHigh"), -150);
        s.setDspMode(DSPMode::LSB);
        QCOMPARE(s.filterLow(), -10000);
        QCOMPARE(s.filterHigh(), -150);
    }

    void modeChangeKeepsThePreviousFilterWhenTheSavedOneHasNoWidthLeft()
    {
        // A saved LSB filter wholly above the cap constrains to
        // (10000, 10000). Thetis returns before the DSP (console.cs:7582
        // [v2.10.3.15]), so the receiver keeps the filter it had.
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        const QString prefix = QStringLiteral("Slice%1/Band%2/Mode%3/")
                                   .arg(s.sliceIndex())
                                   .arg(bandKeyName(Band::Band20m))
                                   .arg(SliceModel::modeName(DSPMode::LSB));
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterLow"), 12000);
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterHigh"), 15000);
        s.setDspMode(DSPMode::LSB);
        QCOMPARE(s.dspMode(), DSPMode::LSB);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 2900);
    }
};

QTEST_MAIN(TestSliceFilterConstrain)
#include "tst_slice_filter_constrain.moc"
