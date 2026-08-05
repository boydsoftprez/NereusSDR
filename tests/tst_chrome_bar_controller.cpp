// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include <QLabel>
#include <QSignalSpy>
#include <QWidget>
#include "gui/chrome/ChromeBarController.h"

using namespace NereusSDR;

class TstChromeBarController : public QObject {
    Q_OBJECT
private:
    QWidget* host{nullptr};

    QLabel* makeItem(int widthPx) {
        auto* l = new QLabel(host);
        l->setFixedWidth(widthPx);
        return l;
    }

    /// Anchor + two foldable rungs + the overflow chip. See the arithmetic
    /// table above overflowChipIsNotChargedWhileNothingIsFolded.
    void addChipLadder(ChromeBarController& c, QLabel*& sys, QLabel*& extra) {
        QLabel* anchor = makeItem(100);
        sys            = makeItem(60);
        extra          = makeItem(40);
        QLabel* chip   = makeItem(26);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.addItem(extra, nullptr, 2, QStringLiteral("Extra"));
        c.addOverflowChip(chip);
    }

private slots:
    void init() {
        host = new QWidget;
        host->resize(2000, 46);
    }

    void cleanup() {
        delete host;
        host = nullptr;
    }

    void wideBarShowsEverything() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(2000);
        QVERIFY(!anchor->isHidden());
        QVERIFY(!sys->isHidden());
        QVERIFY(c.foldedLabels().isEmpty());
    }

    void narrowBarFoldsRungOne() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(120);
        QVERIFY(!anchor->isHidden());
        QVERIFY(sys->isHidden());
        QCOMPARE(c.foldedLabels(), QStringList{QStringLiteral("System")});
    }

    void separatorFoldsWithItsItem() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        QLabel* sep = makeItem(14);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, sep, 1, QStringLiteral("System"));
        c.relayout(120);
        QVERIFY(sys->isHidden());
        QVERIFY(sep->isHidden());
    }

    void relayoutIsIdempotent() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(120);
        const bool first = sys->isHidden();
        c.relayout(120);
        QCOMPARE(sys->isHidden(), first);
    }

    void noOscillationAcrossNeighbouringWidths() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        for (int w = 100; w <= 400; ++w) {
            c.relayout(w);
            const bool cold = sys->isHidden();
            c.relayout(w + 1);
            c.relayout(w);
            QCOMPARE(sys->isHidden(), cold);
        }
    }

    void foldStateChangedOnlyFiresOnChange() {
        ChromeBarController c;
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(makeItem(60), nullptr, 1, QStringLiteral("System"));
        QSignalSpy spy(&c, &ChromeBarController::foldStateChanged);
        c.relayout(120);
        QCOMPARE(spy.count(), 1);
        c.relayout(120);
        QCOMPARE(spy.count(), 1);
        c.relayout(2000);
        QCOMPARE(spy.count(), 2);
    }

    // A width refresh must not discard the separator overhead folded in at
    // registration. It used to: setNaturalWidth overwrote the whole cached
    // figure with the primary widget's width alone, so the system tile shed
    // its separator on the first CPU tick and the TCI indicator on its first
    // state change, leaving the fold plan undercounting the bar. Found by
    // Codex on PR #316.
    void refreshingWidthKeepsTheSeparatorOverhead() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* item   = makeItem(60);
        QLabel* sep    = makeItem(14);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(item, sep, 1, QStringLiteral("Item"));

        // Registered width is 60 + 14 + one 6 px gap = 80. With the anchor,
        // one gap and the padding, the bar needs 198 to show everything.
        c.relayout(198);
        QVERIFY(!item->isHidden());

        // Re-report the SAME primary width. The overhead must survive, so
        // the decision must not move.
        c.setNaturalWidth(item, 60);
        c.relayout(198);
        QVERIFY2(!item->isHidden(),
                 "a no-op width refresh changed the fold decision");

        // One pixel under and it folds, proving the overhead is still
        // counted rather than quietly dropped.
        c.relayout(197);
        QVERIFY2(item->isHidden(),
                 "separator overhead was lost, so the bar was undercounted");
    }

    void setNaturalWidthChangesTheDecision() {
        ChromeBarController c;
        QLabel* sys = makeItem(60);
        c.addItem(makeItem(100), nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));
        c.relayout(180);
        QVERIFY(!sys->isHidden());
        c.setNaturalWidth(sys, 200);
        c.relayout(180);
        QVERIFY(sys->isHidden());
    }

    void widthIsCachedAtRegistrationNotReReadOnRelayout() {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        // Unlike makeItem's fixed-width labels, this one's sizeHint() is
        // driven by real text, so growing the text is a genuine content
        // change a live re-measurement would see.
        auto* sys = new QLabel(QStringLiteral("Sys"), host);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));

        c.relayout(300);
        const bool before = sys->isHidden();
        QVERIFY(!before);

        // Grow the content drastically without calling setNaturalWidth.
        // The cached-at-registration contract says the fold decision must
        // not move; only an explicit setNaturalWidth call may move it.
        sys->setText(QStringLiteral(
            "SystemSystemSystemSystemSystemSystemSystemSystemSystemSystem"));
        c.relayout(300);
        QCOMPARE(sys->isHidden(), before);
    }

    // ── Task A8 fix round 1, finding-driven: the availability axis ────────
    // Two independent gates decide visibility: fold (width-driven, internal)
    // and availability (an external fact reported via setItemAvailable).
    // An item shows only when both hold.

    void unavailableItemStaysHiddenAtAWidthWhereItsRungIsNotFolded()
    {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));

        // Wide enough that rung 1 would not fold on width alone.
        c.relayout(2000);
        QVERIFY(!sys->isHidden());

        c.setItemAvailable(sys, false);
        c.relayout(2000);
        QVERIFY(sys->isHidden());
        // The never-fold anchor is unaffected by another item's
        // availability.
        QVERIFY(!anchor->isHidden());
    }

    void availableItemAtAFoldedRungStaysHidden()
    {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));

        // Explicitly available (the default), but the width forces rung 1
        // to fold regardless -- availability cannot un-fold something.
        c.setItemAvailable(sys, true);
        c.relayout(120);
        QVERIFY(sys->isHidden());
        QCOMPARE(c.foldedThroughRung(), 1);
    }

    void togglingAvailabilityAtFixedWidthFlipsVisibilityWithoutChangingFoldRung()
    {
        ChromeBarController c;
        QLabel* anchor = makeItem(100);
        QLabel* sys    = makeItem(60);
        c.addItem(anchor, nullptr, 0, QString());
        c.addItem(sys, nullptr, 1, QStringLiteral("System"));

        // Wide enough that width pressure alone never folds rung 1, at
        // this width, regardless of what buildTable() includes.
        c.relayout(2000);
        const int rungBefore = c.foldedThroughRung();
        QVERIFY(!sys->isHidden());

        c.setItemAvailable(sys, false);
        c.relayout(2000);
        QVERIFY(sys->isHidden());
        QCOMPARE(c.foldedThroughRung(), rungBefore);

        c.setItemAvailable(sys, true);
        c.relayout(2000);
        QVERIFY(!sys->isHidden());
        QCOMPARE(c.foldedThroughRung(), rungBefore);
    }

    // The math for this lives in tst_chrome_fold_plan; what is checked
    // here is that addOverflowChip actually carries onlyWhenFolded through
    // buildTable, so registering the chip via the plain addItem path (as
    // ChromeBarItems did before) is caught.
    //
    // Both tests below use anchor 100 (rung 0) + sys 60 (rung 1) +
    // extra 40 (rung 2) + chip 26 (rung 0, charged only once folded):
    //
    //   rung 0, chip free    : 100 + 60 + 40      + 2 gaps + 12 pad = 224
    //   rung 0, chip charged : 100 + 60 + 40 + 26 + 3 gaps + 12 pad = 256
    //   rung 1, chip charged : 100 +      40 + 26 + 2 gaps + 12 pad = 190
    //   rung 1, chip free    : 100 +      40      + 1 gap  + 12 pad = 158
    //   rung 2, chip charged : 100 +           26 + 1 gap  + 12 pad = 144
    //
    // A third rung matters: planFold falls back to the highest rung present
    // when nothing fits, so a two-rung ladder cannot tell "rung 1 was
    // chosen" from "rung 1 was all there was". Built by addChipLadder,
    // declared with makeItem above -- a helper taking arguments must not
    // sit in the private slots block, where QTest would try to run it.
    void overflowChipIsNotChargedWhileNothingIsFolded() {
        ChromeBarController c;
        QLabel* sys = nullptr;
        QLabel* extra = nullptr;
        addChipLadder(c, sys, extra);

        // 240 sits between 224 and 256: everything fits, so nothing may
        // fold, and the chip must not be charged for announcing a fold
        // that has not happened.
        c.relayout(240);
        QCOMPARE(c.foldedThroughRung(), 0);
        QVERIFY2(!sys->isHidden(),
                 "the system tile folded at a width where it fits, because "
                 "the overflow chip was charged for reporting the fold");
        QVERIFY(c.foldedLabels().isEmpty());
    }

    // The other direction: once the chip IS on screen its width counts,
    // which is why it is registered at all (final-fix-wave finding 4).
    // Dropping the chip from the budget outright, rather than
    // conditionally, passes the test above and fails this one.
    void overflowChipIsChargedOnceSomethingFolds() {
        ChromeBarController c;
        QLabel* sys = nullptr;
        QLabel* extra = nullptr;
        addChipLadder(c, sys, extra);

        // 189 clears rung 2 (144) but not rung 1 (190) -- by one pixel, and
        // only because the chip's 26 is in that 190. Uncharged, rung 1
        // costs 158 and would be accepted here.
        c.relayout(189);
        QCOMPARE(c.foldedThroughRung(), 2);
        QVERIFY(sys->isHidden());
        QVERIFY(extra->isHidden());
        QCOMPARE(c.foldedLabels(),
                 (QStringList{QStringLiteral("System"), QStringLiteral("Extra")}));
    }
};
QTEST_MAIN(TstChromeBarController)
#include "tst_chrome_bar_controller.moc"
