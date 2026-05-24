// no-port-check: NereusSDR-original unit-test file.  The Thetis reference
// below is a cite comment documenting which upstream line each assertion
// verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_pa_settings_bypass.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::paSettingsBypass (D4).
//
// chkBypassANANPASettings is visible on G2E-group SKUs and represents a
// per-radio "bypass the board-specific PA calibration table" flag.
//
// Thetis source (UI-only, no CheckedChanged handler):
//   setup.cs:19921 [v2.10.3.15] — chkBypassANANPASettings.Visible = true
//   setup.designer.cs:49237-49245 [v2.10.3.15] — tooltip "BP PA"
//
// NereusSDR persistence: AppSettings key "PaSettingsBypass" under the
// per-MAC hardware/<mac>/tx/ prefix, matching the TransmitModel pattern
// established by antiVoxRun (3M-3a-iv).
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstTransmitModelPaSettingsBypass : public QObject {
    Q_OBJECT
private slots:

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT VALUES
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void paSettingsBypass_defaultFalse() {
        // G2E bypass defaults OFF: user must explicitly enable it.
        TransmitModel t;
        QCOMPARE(t.paSettingsBypass(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // ROUND-TRIP SETTER
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setPaSettingsBypass_true_roundTrip() {
        TransmitModel t;
        t.setPaSettingsBypass(true);
        QCOMPARE(t.paSettingsBypass(), true);
    }

    void setPaSettingsBypass_false_roundTrip() {
        TransmitModel t;
        t.setPaSettingsBypass(true);   // set to true first
        t.setPaSettingsBypass(false);  // then back to false
        QCOMPARE(t.paSettingsBypass(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SIGNAL EMISSION
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setPaSettingsBypass_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::paSettingsBypassChanged);
        t.setPaSettingsBypass(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    void setPaSettingsBypass_false_emitsSignal() {
        TransmitModel t;
        t.setPaSettingsBypass(true);
        QSignalSpy spy(&t, &TransmitModel::paSettingsBypassChanged);
        t.setPaSettingsBypass(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD (no signal on same-value set)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotent_paSettingsBypass_false_noSignal() {
        // setPaSettingsBypass(false) on fresh model (default = false) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::paSettingsBypassChanged);
        t.setPaSettingsBypass(false);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_paSettingsBypass_true_noSignal() {
        TransmitModel t;
        t.setPaSettingsBypass(true);
        QSignalSpy spy(&t, &TransmitModel::paSettingsBypassChanged);
        t.setPaSettingsBypass(true);  // same value — no second emission
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PERSISTENCE (AppSettings round-trip via loadFromSettings + auto-persist)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void paSettingsBypass_persistsTrue_acrossReload() {
        AppSettings::instance().clear();

        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:11");

        {
            TransmitModel m1;
            m1.loadFromSettings(mac);    // activates per-MAC auto-persist
            m1.setPaSettingsBypass(true); // → persistOne("PaSettingsBypass", "True")
        }

        TransmitModel m2;
        m2.loadFromSettings(mac);
        QCOMPARE(m2.paSettingsBypass(), true);

        AppSettings::instance().clear();
    }

    void paSettingsBypass_persistsFalse_acrossReload() {
        AppSettings::instance().clear();

        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:22");

        {
            TransmitModel m1;
            m1.loadFromSettings(mac);
            m1.setPaSettingsBypass(true);   // set true first
            m1.setPaSettingsBypass(false);  // then explicit false
        }

        TransmitModel m2;
        m2.loadFromSettings(mac);
        QCOMPARE(m2.paSettingsBypass(), false);

        AppSettings::instance().clear();
    }

    void paSettingsBypass_defaultFalse_whenNoSettingsKey() {
        // If no key exists in AppSettings, default must be false.
        AppSettings::instance().clear();

        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:33");
        TransmitModel m;
        m.loadFromSettings(mac);  // key absent → uses default

        QCOMPARE(m.paSettingsBypass(), false);

        AppSettings::instance().clear();
    }
};

QTEST_GUILESS_MAIN(TstTransmitModelPaSettingsBypass)
#include "tst_transmit_model_pa_settings_bypass.moc"
