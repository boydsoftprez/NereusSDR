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
};
QTEST_MAIN(TstChromeBarController)
#include "tst_chrome_bar_controller.moc"
