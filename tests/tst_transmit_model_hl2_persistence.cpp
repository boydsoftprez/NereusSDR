// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// references below are cite comments documenting which Thetis lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_hl2_persistence.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel polymorphic per-band tune-power clamp.
//
// Spec §6; Plan Task 6; Issue #175.
//
// Covers:
//   - HL2: setTunePowerForBand clamps to [0, 99] (mi0bot Tune scale,
//     33 sub-steps).
//   - Non-HL2: setTunePowerForBand clamps to [0, 100] (canonical Thetis
//     0-100 watts target).
//   - HL2 negative values clamp to 0.
//   - dB-to-int / int-to-dB formula round-trip at boundary values (pins
//     the math used by Task 9 UI conversion).
//
// Source references (cite comments only — no Thetis logic in tests):
//   mi0bot console.cs:47616-47666 [v2.10.3.13-beta2] — HL2 Tune slider
//     0..99 encoding (33 sub-steps formula).
// =================================================================

#include <QtTest/QtTest>
#include "models/TransmitModel.h"
#include "core/HpsdrModel.h"
#include "core/AppSettings.h"

using NereusSDR::HPSDRModel;

class TestTransmitModelHl2Persistence : public QObject {
    Q_OBJECT
private slots:
    void hl2_clamp_to_99() {
        // On HL2, per-band tune power clamps [0, 99] (mi0bot encoding).
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::HERMESLITE);
        m.setTunePowerForBand(NereusSDR::Band::Band40m, 150);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::Band40m), 99);
    }

    void nonHl2_clamp_to_100() {
        // On non-HL2, per-band tune power clamps [0, 100] (existing).
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::ANAN100);
        m.setTunePowerForBand(NereusSDR::Band::Band40m, 150);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::Band40m), 100);
    }

    void hl2_negative_clamps_to_zero() {
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::HERMESLITE);
        m.setTunePowerForBand(NereusSDR::Band::Band40m, -10);
        QCOMPARE(m.tunePowerForBand(NereusSDR::Band::Band40m), 0);
    }

    void hl2_dB_to_int_round_trip_at_boundaries() {
        // Per-band stored value is int 0..99 on HL2.
        // UI conversion: dB-to-int = round((33 + dB*2) * 3)
        // Inverse:        int-to-dB = (value/3 - 33) / 2
        // (used by Task 9; this test pins the math)

        // dB = 0     -> int = 99
        // dB = -16.5 -> int = 0
        // dB = -7.5  -> int = round(((33 + -15) * 3)) = round(54) = 54
        //   (exactly: (33 + -15) * 3 = 18 * 3 = 54)
        const double dB_99 = (99.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_99 + 1.0, 0.0 + 1.0));   // ~0
        const double dB_0 = (0.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_0, -16.5));
        const double dB_54 = (54.0/3.0 - 33.0) / 2.0;
        QVERIFY(qFuzzyCompare(dB_54, -7.5));
    }

    void hl2_setTunePower_global_clamps_to_99() {
        // Issue #175 review fix: setTunePower (Fixed-mode global) must
        // polymorph the same way as setTunePowerForBand.  Without the
        // fix, the global Fixed value would be allowed to reach 100 on
        // an HL2, violating spec §6 [0, 99] HL2 ceiling.
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::HERMESLITE);
        m.setTunePower(150);
        QCOMPARE(m.tunePower(), 99);
    }

    // R-R3-46 fix wave + follow-up item 2. PowerPage::applyHpsdrModel no
    // longer lets its spinbox's range clamp write tune power back, so the
    // stored value must stay valid on its own, and saved under the right
    // radio. A connect sets the new model (setHpsdrModel) before it loads
    // the new radio's settings (loadFromSettings): the clamp happens at the
    // load, for that radio. 100 W on another radio becomes 99 (0 dB) on an
    // HL2, and the other radio's saved value is left as it was.
    void modelChangeClampsTheStoredTunePower() {
        const QString radioA = QStringLiteral("02:00:00:00:75:01");
        const QString hl2 = QStringLiteral("02:00:00:00:75:02");
        auto& s = NereusSDR::AppSettings::instance();
        s.clearHardwareValues(radioA);
        s.clearHardwareValues(hl2);
        const auto fixedKey = [](const QString& mac) {
            return QStringLiteral("hardware/%1/tx/FixedTunePower").arg(mac);
        };

        // Session on radio A: fixed tune power 100 W, saved for A.
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::ANAN100);
        m.loadFromSettings(radioA);
        m.setTunePower(100);
        QCOMPARE(s.value(fixedKey(radioA)).toString(), QStringLiteral("100"));

        // Same session, connect to an HL2 whose saved value is also 100.
        s.setValue(fixedKey(hl2), QStringLiteral("100"));
        m.setHpsdrModel(HPSDRModel::HERMESLITE);   // before its settings load
        QCOMPARE(s.value(fixedKey(radioA)).toString(), QStringLiteral("100"));
        m.loadFromSettings(hl2);
        QCOMPARE(m.tunePower(), 99);
        QCOMPARE(s.value(fixedKey(hl2)).toString(), QStringLiteral("99"));
        QCOMPARE(s.value(fixedKey(radioA)).toString(), QStringLiteral("100"));

        // The in-memory value already equal to the clamp still fixes the
        // HL2's saved value.
        s.setValue(fixedKey(hl2), QStringLiteral("100"));
        m.loadFromSettings(hl2);
        QCOMPARE(m.tunePower(), 99);
        QCOMPARE(s.value(fixedKey(hl2)).toString(), QStringLiteral("99"));

        // A model that holds no radio's settings (a remote window's copy of
        // the Core's) is left alone.
        NereusSDR::TransmitModel window;
        window.setHpsdrModel(HPSDRModel::ANAN100);
        window.setTunePower(100);
        QSignalSpy untouched(&window, &NereusSDR::TransmitModel::tunePowerChanged);
        window.setHpsdrModel(HPSDRModel::HERMESLITE);
        QCOMPARE(window.tunePower(), 100);
        QCOMPARE(untouched.count(), 0);
        s.clearHardwareValues(radioA);
        s.clearHardwareValues(hl2);
    }

    void nonHl2_setTunePower_global_clamps_to_100() {
        // Mirror test for non-HL2: ceiling stays at 100.
        NereusSDR::TransmitModel m;
        m.setHpsdrModel(HPSDRModel::ANAN100);
        m.setTunePower(150);
        QCOMPARE(m.tunePower(), 100);
    }
};

QTEST_MAIN(TestTransmitModelHl2Persistence)
#include "tst_transmit_model_hl2_persistence.moc"
