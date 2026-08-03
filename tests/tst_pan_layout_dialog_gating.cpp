// no-port-check: AetherSDR-derived. See PanLayoutDialog.h.
#include <QtTest/QtTest>
#include "gui/PanLayoutDialog.h"

using namespace NereusSDR;

// PanLayoutDialog's ctor takes a single pre-computed ceiling: the caller
// (MainWindow::showPanLayoutDialog) is responsible for reducing
// BoardCapabilities down to qMin(maxSlices, userDdcCount) before
// constructing the dialog (final-fix-wave finding 2). Each fixture below
// does that same reduction inline, with a comment citing the real
// BoardCapabilities.cpp row, so a reader can see this is the DDC-derived
// answer and not a bare slice count.
class TstPanLayoutDialogGating : public QObject {
    Q_OBJECT
private slots:
    void fiveSliceBoardSeesEverything() {
        // Saturn/ANAN-G2/G2-1K/Andromeda: maxSlices=5, userDdcCount=5.
        PanLayoutDialog d(qMin(5, 5), QStringLiteral("1"), QStringLiteral("ANAN-G2"));
        // Asserting the count alone (originally QCOMPARE(..., 9)) would still
        // pass if the wrong nine ids were shown, or if they came out of
        // order. Assert the exact id list, in grid order, instead. See
        // task-B3-brief.md "Test rigour".
        QCOMPARE(d.visibleLayoutIds(), (QStringList{
                     QStringLiteral("1"),   QStringLiteral("2v"),
                     QStringLiteral("2h"),  QStringLiteral("12h"),
                     QStringLiteral("2h1"), QStringLiteral("3v"),
                     QStringLiteral("2x2"), QStringLiteral("4v"),
                     QStringLiteral("3h2")}));
        QVERIFY(d.footerText().isEmpty());
    }

    void fourSliceBoardHidesTheFivePanLayout() {
        // Hermes: maxSlices=4, userDdcCount=4.
        PanLayoutDialog d(qMin(4, 4), QStringLiteral("1"), QStringLiteral("Hermes"));
        QCOMPARE(d.visibleLayoutIds().size(), 8);
        QVERIFY(!d.visibleLayoutIds().contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes")));
        QVERIFY(d.footerText().contains(QStringLiteral("1 layout")));
    }

    void threeSliceBoardHidesThePanCountsAboveThree() {
        // Atlas/Metis: maxSlices=3, userDdcCount=3.
        PanLayoutDialog d(qMin(3, 3), QStringLiteral("1"), QStringLiteral("Metis"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids.size(), 6);
        QVERIFY(!ids.contains(QStringLiteral("2x2")));
        QVERIFY(!ids.contains(QStringLiteral("4v")));
        QVERIFY(!ids.contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("3 layouts")));
    }

    void twoSliceBoardKeepsOnlyTheOneAndTwoPanLayouts() {
        // HermesII: maxSlices=2, userDdcCount=2.
        PanLayoutDialog d(qMin(2, 2), QStringLiteral("1"), QStringLiteral("Hermes II"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids, (QStringList{QStringLiteral("1"),
                                   QStringLiteral("2v"),
                                   QStringLiteral("2h")}));
    }

    void hl2GatesOnDdcCountNotMaxSlices() {
        // Hermes Lite 2 (BoardCapabilities.cpp:842-843): maxSlices=5,
        // userDdcCount=2. This is the regression case for finding 2: gating
        // on maxSlices alone showed all nine layouts, including 3h2's five
        // pans, on a board that can only ever open two independent DDCs.
        // Picking 3h2 painted five tiles, three of which could never
        // receive a slice. Gating on qMin(5, 2) must show only the 1 and
        // 2-pan layouts.
        PanLayoutDialog d(qMin(5, 2), QStringLiteral("1"),
                          QStringLiteral("Hermes Lite 2"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids, (QStringList{QStringLiteral("1"),
                                   QStringLiteral("2v"),
                                   QStringLiteral("2h")}));
        QVERIFY(!ids.contains(QStringLiteral("12h")));
        QVERIFY(!ids.contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes Lite 2")));
        QVERIFY(d.footerText().contains(QStringLiteral("6 layouts")));
        // The footer must describe pans, not slices: this board hosts up to
        // 5 slices total, just never more than 2 independent pans.
        QVERIFY(d.footerText().contains(QStringLiteral("pans")));
    }

    void singleAlwaysSurvives() {
        // Synthetic single-pan fixture; no real board caps at exactly 1.
        PanLayoutDialog d(1, QStringLiteral("1"), QStringLiteral("Tiny"));
        QVERIFY(d.visibleLayoutIds().contains(QStringLiteral("1")));
    }

    void footerNamesTheBoardNotTheApp() {
        // HermesII: maxSlices=2, userDdcCount=2.
        PanLayoutDialog d(qMin(2, 2), QStringLiteral("1"), QStringLiteral("Hermes II"));
        // "this radio does not have that", never "this app does not have that".
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes II")));
        QVERIFY(d.footerText().contains(QStringLiteral("radio")));
    }

    void selectedLayoutIsEmptyBeforeAnyChoice() {
        // Saturn/ANAN-G2/G2-1K/Andromeda: maxSlices=5, userDdcCount=5.
        PanLayoutDialog d(qMin(5, 5), QStringLiteral("1"), QStringLiteral("ANAN-G2"));
        QVERIFY(d.selectedLayout().isEmpty());
    }
};
QTEST_MAIN(TstPanLayoutDialogGating)
#include "tst_pan_layout_dialog_gating.moc"
