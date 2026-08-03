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

// Same four rung-1..10 entries as sampleTable(), fed in scrambled order.
// sampleTable() is already ascending by rung, so it cannot distinguish a
// working std::stable_sort in foldedLabels() from one that was removed
// or given a flipped comparator; this table can.
QVector<ChromeFoldEntry> outOfRungOrderTable()
{
    return {
        {10, 122, QStringLiteral("Placeholders")},
        {1,  60,  QStringLiteral("System")},
        {3,  60,  QStringLiteral("CAT")},
        {2,  62,  QStringLiteral("TGXL")},
    };
}

// No entry carries rung 0, so folding through the highest rung present
// can legitimately hide everything without needing an empty table.
QVector<ChromeFoldEntry> noAnchorTable()
{
    return {
        {1, 60, QStringLiteral("System")},
        {2, 62, QStringLiteral("TGXL")},
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

    void planFoldIsOrderIndependent() {
        // A meaningful determinism property for a pure, static function:
        // planFold sums by rung, so the SAME set of entries must fold the
        // same way regardless of the order they appear in the vector.
        // The previous version of this test compared planFold(t, w) to
        // planFold(t, w) with the identical vector in the identical
        // call -- for a static function with no internal state that can
        // never fail, and it proved nothing (final-fix-wave finding 9).
        // Reuses outOfRungOrderTable()'s documented content (System(1),
        // TGXL(2), CAT(3), Placeholders(10), fed in as 10,1,3,2) against
        // the same four entries in ascending order.
        const QVector<ChromeFoldEntry> ascending = {
            {1,  60, QStringLiteral("System")},
            {2,  62, QStringLiteral("TGXL")},
            {3,  60, QStringLiteral("CAT")},
            {10, 122, QStringLiteral("Placeholders")},
        };
        const QVector<ChromeFoldEntry> scrambled = outOfRungOrderTable();
        for (int w = 40; w <= 400; ++w) {
            QCOMPARE(ChromeFoldPlan::planFold(scrambled, w),
                     ChromeFoldPlan::planFold(ascending, w));
        }
    }

    void foldedLabelsListsWhatWent() {
        const QStringList got = ChromeFoldPlan::foldedLabels(sampleTable(), 3);
        QCOMPARE(got, (QStringList{QStringLiteral("System"),
                                   QStringLiteral("TGXL"),
                                   QStringLiteral("CAT"),
                                   QStringLiteral("TCI")}));
    }

    void foldedLabelsSortsByRungRegardlessOfInputOrder() {
        // outOfRungOrderTable() feeds entries in as (10, 1, 3, 2). Folding
        // through the highest rung hides all four; the returned labels
        // must still come back ascending by rung (1, 2, 3, 10). If
        // foldedLabels() ever loses its std::stable_sort, this reports
        // [Placeholders, System, CAT, TGXL], the table's raw encounter
        // order, and the test fails.
        const QStringList got = ChromeFoldPlan::foldedLabels(outOfRungOrderTable(), 10);
        QCOMPARE(got, (QStringList{QStringLiteral("System"),
                                   QStringLiteral("TGXL"),
                                   QStringLiteral("CAT"),
                                   QStringLiteral("Placeholders")}));
    }

    void emptyTableRequiredWidthIsJustPadding() {
        // No entries at all, so visible never leaves 0. sampleTable()
        // always keeps its rung-0 anchor visible, so nothing built on it
        // can reach this branch of requiredWidth().
        const QVector<ChromeFoldEntry> empty;
        QCOMPARE(ChromeFoldPlan::requiredWidth(empty, 0), ChromeFoldPlan::kPadPx);
    }

    void emptyTablePlanFoldAndFoldedLabelsAreSafe() {
        const QVector<ChromeFoldEntry> empty;
        // No rungs exist to fold through, so planFold has nothing to do.
        QCOMPARE(ChromeFoldPlan::planFold(empty, 2000), 0);
        QVERIFY(ChromeFoldPlan::foldedLabels(empty, 10).isEmpty());
    }

    void requiredWidthWithNoRungZeroAnchorFoldsToPadding() {
        // noAnchorTable() has no rung-0 entry, so folding through its
        // highest rung (2) hides both entries and visible reaches 0 with
        // a non-empty input. Without the visible == 0 guard in
        // requiredWidth(), the general formula collapses to
        // kPadPx - kGapPx == 6 instead of kPadPx == 12.
        QCOMPARE(ChromeFoldPlan::requiredWidth(noAnchorTable(), 2), ChromeFoldPlan::kPadPx);
    }
};
QTEST_MAIN(TstChromeFoldPlan)
#include "tst_chrome_fold_plan.moc"
