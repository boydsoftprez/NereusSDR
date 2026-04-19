#include <QtTest/QtTest>
#include "models/SliceModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstSliceModelVax : public QObject {
    Q_OBJECT
private slots:
    void defaultsToOff() {
        SliceModel s(0);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void setAndGet() {
        SliceModel s(0);
        s.setVaxChannel(2);
        QCOMPARE(s.vaxChannel(), 2);
    }

    void clampsOutOfRangeLow() {
        SliceModel s(0);
        s.setVaxChannel(-5);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void clampsOutOfRangeHigh() {
        SliceModel s(0);
        s.setVaxChannel(99);
        QCOMPARE(s.vaxChannel(), 0);
    }

    void emitsSignalOnChange() {
        SliceModel s(0);
        QSignalSpy spy(&s, &SliceModel::vaxChannelChanged);
        s.setVaxChannel(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    void noSignalOnSameValue() {
        SliceModel s(0);
        s.setVaxChannel(2);
        QSignalSpy spy(&s, &SliceModel::vaxChannelChanged);
        s.setVaxChannel(2);
        QCOMPARE(spy.count(), 0);
    }

    void persistsToSettings() {
        AppSettings::instance().clear();
        SliceModel s(7);
        s.setVaxChannel(4);
        QCOMPARE(AppSettings::instance().value("Slice7/VaxChannel").toString(), "4");
    }

    void restoresFromSettings() {
        AppSettings::instance().clear();
        AppSettings::instance().setValue("Slice7/VaxChannel", "3");
        SliceModel s(7);
        s.loadFromSettings();
        QCOMPARE(s.vaxChannel(), 3);
    }

    void clampsCorruptValueOnLoad() {
        AppSettings::instance().clear();
        AppSettings::instance().setValue("Slice7/VaxChannel", "99");
        SliceModel s(7);
        s.loadFromSettings();
        QCOMPARE(s.vaxChannel(), 0);
    }
};

QTEST_APPLESS_MAIN(TstSliceModelVax)
#include "tst_slice_model_vax.moc"
