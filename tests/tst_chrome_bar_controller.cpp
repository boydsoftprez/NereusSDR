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
};
QTEST_MAIN(TstChromeBarController)
#include "tst_chrome_bar_controller.moc"
