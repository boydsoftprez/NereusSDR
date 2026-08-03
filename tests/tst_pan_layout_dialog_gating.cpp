// no-port-check: AetherSDR-derived. See PanLayoutDialog.h.
#include <QtTest/QtTest>
#include "gui/PanLayoutDialog.h"

using namespace NereusSDR;

class TstPanLayoutDialogGating : public QObject {
    Q_OBJECT
private slots:
    void fiveSliceBoardSeesEverything() {
        PanLayoutDialog d(5, QStringLiteral("1"), QStringLiteral("ANAN-G2"));
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
        PanLayoutDialog d(4, QStringLiteral("1"), QStringLiteral("Hermes"));
        QCOMPARE(d.visibleLayoutIds().size(), 8);
        QVERIFY(!d.visibleLayoutIds().contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes")));
        QVERIFY(d.footerText().contains(QStringLiteral("1 layout")));
    }

    void threeSliceBoardHidesThePanCountsAboveThree() {
        PanLayoutDialog d(3, QStringLiteral("1"), QStringLiteral("Metis"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids.size(), 6);
        QVERIFY(!ids.contains(QStringLiteral("2x2")));
        QVERIFY(!ids.contains(QStringLiteral("4v")));
        QVERIFY(!ids.contains(QStringLiteral("3h2")));
        QVERIFY(d.footerText().contains(QStringLiteral("3 layouts")));
    }

    void twoSliceBoardKeepsOnlyTheOneAndTwoPanLayouts() {
        PanLayoutDialog d(2, QStringLiteral("1"), QStringLiteral("Hermes II"));
        const QStringList ids = d.visibleLayoutIds();
        QCOMPARE(ids, (QStringList{QStringLiteral("1"),
                                   QStringLiteral("2v"),
                                   QStringLiteral("2h")}));
    }

    void singleAlwaysSurvives() {
        PanLayoutDialog d(1, QStringLiteral("1"), QStringLiteral("Tiny"));
        QVERIFY(d.visibleLayoutIds().contains(QStringLiteral("1")));
    }

    void footerNamesTheBoardNotTheApp() {
        PanLayoutDialog d(2, QStringLiteral("1"), QStringLiteral("Hermes II"));
        // "this radio does not have that", never "this app does not have that".
        QVERIFY(d.footerText().contains(QStringLiteral("Hermes II")));
        QVERIFY(d.footerText().contains(QStringLiteral("radio")));
    }

    void selectedLayoutIsEmptyBeforeAnyChoice() {
        PanLayoutDialog d(5, QStringLiteral("1"), QStringLiteral("ANAN-G2"));
        QVERIFY(d.selectedLayout().isEmpty());
    }
};
QTEST_MAIN(TstPanLayoutDialogGating)
#include "tst_pan_layout_dialog_gating.moc"
