// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include <QLabel>
#include <QWidget>

#include "gui/chrome/ChromeBarController.h"
#include "gui/chrome/ChromeBarItems.h"

using namespace NereusSDR;

class TstChromeBarItems : public QObject {
    Q_OBJECT

private:
    QWidget* host{nullptr};

    QLabel* w(int px) {
        auto* l = new QLabel(host);
        l->setFixedWidth(px);
        return l;
    }

    // Natural widths from the design doc's section 4.6 ledger.
    ChromeBarWidgets makeWidgets() {
        ChromeBarWidgets g;
        g.panButton        = w(48);
        g.panelToggle      = w(18);
        g.placeholderGroup = w(132);
        g.placeholderSep   = w(14);
        g.chain0           = w(52);
        g.rxDashRow        = w(96);
        g.psaIndicator     = w(66);
        g.stationBlock     = w(168);
        g.catIndicator     = w(60);
        g.catSep           = w(14);
        g.tciIndicator     = w(60);
        g.tciSep           = w(14);
        g.tgxlChip         = w(62);
        g.systemTile       = w(60);
        g.systemTileSep    = w(14);
        g.safetyGroup      = w(200);
        for (int r = 5; r <= 9; ++r) { g.pillByRung[r] = w(22); }
        return g;
    }

private slots:
    void init()    { host = new QWidget; }
    void cleanup() { delete host; host = nullptr; }

    void everythingShowsWhenWide() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        c.relayout(2400);
        QCOMPARE(c.foldedThroughRung(), 0);
        QVERIFY(c.foldedLabels().isEmpty());
    }

    void foldRungNeverGoesBackwardsAsWidthShrinks() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        int prev = 0;
        for (int width = 2400; width >= 300; --width) {
            c.relayout(width);
            const int rung = c.foldedThroughRung();
            QVERIFY2(rung >= prev,
                     qPrintable(QStringLiteral("rung %1 < %2 at width %3")
                                    .arg(rung).arg(prev).arg(width)));
            prev = rung;
        }
        QVERIFY2(prev > 0, "nothing ever folded; fixture widths are wrong");
    }

    void oneWidthAlwaysYieldsOneLayout() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        for (int width = 300; width <= 2400; width += 7) {
            c.relayout(width);
            const int cold = c.foldedThroughRung();
            c.relayout(width + 1);
            c.relayout(width - 1);
            c.relayout(width);
            QCOMPARE(c.foldedThroughRung(), cold);
        }
    }

    void neverFoldingItemsSurviveEveryWidth() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        const QList<QWidget*> mustSurvive = {
            g.panButton, g.panelToggle, g.stationBlock, g.safetyGroup,
            g.psaIndicator
        };
        for (int width = 2400; width >= 300; width -= 3) {
            c.relayout(width);
            for (QWidget* keep : mustSurvive) {
                QVERIFY2(!keep->isHidden(),
                         qPrintable(QStringLiteral("never-fold item folded at %1")
                                        .arg(width)));
            }
        }
    }

    void ladderFoldsInTheDesignedOrder() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        // Design section 6: system tile, then TGXL, then the CAT/TCI pair,
        // then the chain tags, then the RX pills, then the placeholders.
        const QList<QWidget*> order = {
            g.systemTile, g.tgxlChip, g.catIndicator, g.chain0,
            g.pillByRung[5], g.placeholderGroup
        };
        QList<int> foldWidth;
        for (QWidget* item : order) {
            int found = -1;
            for (int width = 2400; width >= 200; --width) {
                c.relayout(width);
                if (item->isHidden()) { found = width; break; }
            }
            QVERIFY2(found > 0, "an item on the ladder never folded");
            foldWidth << found;
        }
        for (int i = 1; i < foldWidth.size(); ++i) {
            QVERIFY2(foldWidth[i] <= foldWidth[i - 1],
                     qPrintable(QStringLiteral("ladder out of order at index %1")
                                    .arg(i)));
        }
    }

    void catAndTciFoldTogether() {
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        registerChromeBarItems(c, g);
        for (int width = 2400; width >= 300; --width) {
            c.relayout(width);
            QCOMPARE(g.catIndicator->isHidden(), g.tciIndicator->isHidden());
        }
    }

    void nullWidgetsAreSkippedNotCrashed() {
        // Defensive case: addItem's null-skip guard, exercised here in
        // case a future caller does not construct every widget. NOT how
        // production represents a single-ADC SKU or a tuner-absent board
        // -- MainWindow::buildStatusBar constructs chain1 and tgxlChip
        // unconditionally and gates their visibility live via
        // setItemAvailable, never by leaving the field null
        // (final-fix-wave finding 8).
        ChromeBarController c;
        ChromeBarWidgets g = makeWidgets();
        g.chain1 = nullptr;
        g.tgxlChip = nullptr;
        registerChromeBarItems(c, g);
        c.relayout(1512);
        QVERIFY(!g.panButton->isHidden());

        // Stronger than "didn't crash": the gap left by two unregistered
        // rungs (2, 4) must not wedge the ladder for anything else. Sweep
        // down and confirm a still-registered item folds normally, and
        // that tgxlChip's label can never surface -- it cannot fold if it
        // was never on the ladder to begin with.
        bool systemTileFolded = false;
        for (int width = 2400; width >= 300; --width) {
            c.relayout(width);
            QVERIFY(!c.foldedLabels().contains(QStringLiteral("TGXL")));
            if (g.systemTile->isHidden()) { systemTileFolded = true; }
        }
        QVERIFY2(systemTileFolded,
                "ladder never folded systemTile with two rungs unregistered");
    }
};
QTEST_MAIN(TstChromeBarItems)
#include "tst_chrome_bar_items.moc"
