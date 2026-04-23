// tst_slice_has_settings_for.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Unit tests for SliceModel::hasSettingsFor(Band) — first-visit probe used
// by RadioModel::onBandButtonClicked to decide between seed and restore.
// Key format matches Phase 3G-10 Stage 2 persistence:
// "Slice<N>/Band<key>/DspMode".

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceHasSettingsFor : public QObject {
    Q_OBJECT

private:
    static void clearKeys() {
        auto& s = AppSettings::instance();
        for (const QString& band : {
                QStringLiteral("20m"), QStringLiteral("40m"),
                QStringLiteral("60m"), QStringLiteral("XVTR") }) {
            s.remove(QStringLiteral("Slice0/Band") + band + QStringLiteral("/DspMode"));
            s.remove(QStringLiteral("Slice1/Band") + band + QStringLiteral("/DspMode"));
        }
    }

private slots:
    void init() { clearKeys(); }
    void cleanup() { clearKeys(); }

    void absent_key_returns_false() {
        SliceModel slice(0);
        QVERIFY(!slice.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice.hasSettingsFor(Band::Band40m));
        QVERIFY(!slice.hasSettingsFor(Band::XVTR));
    }

    void present_key_returns_true() {
        SliceModel slice(0);
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band20m/DspMode"),
                   static_cast<int>(DSPMode::USB));
        QVERIFY(slice.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice.hasSettingsFor(Band::Band40m));
    }

    void per_slice_scoping() {
        SliceModel slice0(0);
        SliceModel slice1(1);
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band20m/DspMode"),
                   static_cast<int>(DSPMode::USB));
        QVERIFY(slice0.hasSettingsFor(Band::Band20m));
        QVERIFY(!slice1.hasSettingsFor(Band::Band20m));
    }
};

QTEST_MAIN(TestSliceHasSettingsFor)
#include "tst_slice_has_settings_for.moc"
