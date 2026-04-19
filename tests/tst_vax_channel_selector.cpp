#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/widgets/VaxChannelSelector.h"

using namespace NereusSDR;

class TstVaxChannelSelector : public QObject {
    Q_OBJECT
private slots:
    void defaultsToOff() {
        VaxChannelSelector w;
        QCOMPARE(w.value(), 0);
    }
    void setValueUpdatesUi() {
        VaxChannelSelector w;
        w.setValue(3);
        QCOMPARE(w.value(), 3);
    }
    void clickingButtonEmitsSignal() {
        VaxChannelSelector w;
        QSignalSpy spy(&w, &VaxChannelSelector::valueChanged);
        w.simulateClick(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }
    void programmaticSetDoesNotEmit() {
        VaxChannelSelector w;
        QSignalSpy spy(&w, &VaxChannelSelector::valueChanged);
        w.setValue(4);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TstVaxChannelSelector)
#include "tst_vax_channel_selector.moc"
