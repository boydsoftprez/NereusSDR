// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "gui/chrome/ChromeFoldPlan.h"

using namespace NereusSDR;

namespace {
QVector<ChromeFoldEntry> sampleTable()
{
    return {
        {0, 100, QStringLiteral("anchor")},
        {1,  60, QStringLiteral("System")},
        {2,  62, QStringLiteral("TGXL")},
        {3,  60, QStringLiteral("CAT")},
        {3,  60, QStringLiteral("TCI")},
        {10, 122, QStringLiteral("Placeholders")},
    };
}
} // namespace

class TstChromeFoldPlan : public QObject {
    Q_OBJECT
private slots:
    void requiredWidthCountsGapsAndPadding() {
        // 6 entries: 464 px of content + 5 gaps * 6 px + 12 px padding.
        QCOMPARE(ChromeFoldPlan::requiredWidth(sampleTable(), 0), 506);
    }

    void foldingRungOneRemovesThatEntry() {
        // Drop the 60 px system tile and one 6 px gap.
        QCOMPARE(ChromeFoldPlan::requiredWidth(sampleTable(), 1), 440);
    }

    void sameRungFoldsTogether() {
        // Rung 3 holds CAT and TCI; folding through 3 removes both.
        const int through2 = ChromeFoldPlan::requiredWidth(sampleTable(), 2);
        const int through3 = ChromeFoldPlan::requiredWidth(sampleTable(), 3);
        QCOMPARE(through2 - through3, 132);  // 60 + 60 + two 6 px gaps
    }

    void rungZeroNeverFolds() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        // Even folding through every rung leaves the anchor.
        QCOMPARE(ChromeFoldPlan::requiredWidth(t, 99), 112);  // 100 + 12 padding
    }

    void planFoldReturnsZeroWhenEverythingFits() {
        QCOMPARE(ChromeFoldPlan::planFold(sampleTable(), 2000), 0);
    }

    void planFoldStopsAtFirstFittingRung() {
        // 440 px fits exactly once rung 1 is folded, so rung 2 must not fire.
        QCOMPARE(ChromeFoldPlan::planFold(sampleTable(), 440), 1);
    }

    void planFoldIsMonotonic() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        int prev = 0;
        for (int w = 2000; w >= 100; --w) {
            const int rung = ChromeFoldPlan::planFold(t, w);
            QVERIFY2(rung >= prev,
                     qPrintable(QStringLiteral("rung went backwards at w=%1").arg(w)));
            prev = rung;
        }
    }

    void planFoldIsDeterministic() {
        const QVector<ChromeFoldEntry> t = sampleTable();
        for (int w = 100; w <= 2000; ++w) {
            QCOMPARE(ChromeFoldPlan::planFold(t, w), ChromeFoldPlan::planFold(t, w));
        }
    }

    void foldedLabelsListsWhatWent() {
        const QStringList got = ChromeFoldPlan::foldedLabels(sampleTable(), 3);
        QCOMPARE(got, (QStringList{QStringLiteral("System"),
                                   QStringLiteral("TGXL"),
                                   QStringLiteral("CAT"),
                                   QStringLiteral("TCI")}));
    }
};
QTEST_MAIN(TstChromeFoldPlan)
#include "tst_chrome_fold_plan.moc"
