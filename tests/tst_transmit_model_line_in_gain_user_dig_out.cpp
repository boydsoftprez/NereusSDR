// no-port-check: NereusSDR-original unit-test file.  The Thetis source
// citations below are cite comments documenting which upstream lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_line_in_gain_user_dig_out.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::lineInGain + userDigOut Q_PROPERTYs (Task 2.4
// of the P1 full-parity epic).  Covers:
//   - Defaults (lineInGain == 0 / userDigOut == 0)
//   - setLineInGain clamps to [0, 31]
//   - setUserDigOut masks to [0, 15] (low 4 bits)
//   - Signals emit on actual change, not on idempotent set
//   - persistToSettings + loadFromSettings round-trip preserves both values
//   - MicProfileManager round-trip preserves both values via the
//     Mic_LineInGain / Mic_UserDigOut bundle keys.
//
// Source references (for traceability; logic ported in TransmitModel.cpp +
// MicProfileManager.cpp):
//   Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//     case 11:
//       C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//   Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
//     case 11:
//       C3 = prn->user_dig_out & 0b00001111;
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

class TstTransmitModelLineInGainUserDigOut : public QObject {
    Q_OBJECT
private slots:

    void initTestCase()
    {
        // Clean state for the whole suite — share-the-singleton AppSettings.
        AppSettings::instance().clear();
    }

    void init()
    {
        // Clean state per test (persistence + profile-manager tests use it).
        AppSettings::instance().clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT VALUES
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void default_lineInGain_isZero() {
        // Default 0 = no line-in attenuation.
        // Source: Thetis networkproto1.c:600 [v2.10.3.13] — line_in_gain
        // ships zero when the host has not set it.
        TransmitModel t;
        QCOMPARE(t.lineInGain(), 0);
    }

    void default_userDigOut_isZero() {
        // Default 0 = all 4 user-dig-out pins low.
        // Source: Thetis networkproto1.c:601 [v2.10.3.13] — user_dig_out
        // ships zero when the host has not set it.
        TransmitModel t;
        QCOMPARE(t.userDigOut(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // ROUND-TRIP SETTERS
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setLineInGain_midRange_roundTrip() {
        TransmitModel t;
        t.setLineInGain(15);
        QCOMPARE(t.lineInGain(), 15);
    }

    void setLineInGain_max_roundTrip() {
        TransmitModel t;
        t.setLineInGain(31);
        QCOMPARE(t.lineInGain(), 31);
    }

    void setUserDigOut_midRange_roundTrip() {
        TransmitModel t;
        t.setUserDigOut(7);
        QCOMPARE(t.userDigOut(), 7);
    }

    void setUserDigOut_max_roundTrip() {
        TransmitModel t;
        t.setUserDigOut(15);
        QCOMPARE(t.userDigOut(), 15);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // CLAMP / MASK
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setLineInGain_clampNegative_to0() {
        TransmitModel t;
        t.setLineInGain(20);   // first establish a non-zero baseline
        QCOMPARE(t.lineInGain(), 20);
        t.setLineInGain(-5);
        QCOMPARE(t.lineInGain(), 0);
    }

    void setLineInGain_clampOverMax_to31() {
        TransmitModel t;
        t.setLineInGain(50);
        QCOMPARE(t.lineInGain(), 31);
    }

    void setLineInGain_clampWayOverMax_to31() {
        TransmitModel t;
        t.setLineInGain(0xFFFF);
        QCOMPARE(t.lineInGain(), 31);
    }

    void setUserDigOut_maskHighBits_low4Only() {
        // 0xF3 = 0b1111_0011 — high nibble must drop, only 0x03 survives.
        TransmitModel t;
        t.setUserDigOut(0xF3);
        QCOMPARE(t.userDigOut(), 0x03);
    }

    void setUserDigOut_maskOverMax_low4Only() {
        TransmitModel t;
        t.setUserDigOut(20);   // 0x14 → 0x04
        QCOMPARE(t.userDigOut(), 0x04);
    }

    void setUserDigOut_negativeBecomesMaskedTwosComplement() {
        // -1 (0xFFFF FFFF) & 0x0F = 0x0F.  Documents the bit-mask behaviour
        // — caller is expected to pass non-negative values; mask is the
        // safety net.
        TransmitModel t;
        t.setUserDigOut(-1);
        QCOMPARE(t.userDigOut(), 0x0F);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SIGNAL EMISSION
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setLineInGain_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(15);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 15);
    }

    void setUserDigOut_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::userDigOutChanged);
        t.setUserDigOut(7);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 7);
    }

    void setLineInGain_clampSignalCarriesClampedValue() {
        // Signal must carry the post-clamp value, not the raw input.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(99);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 31);
    }

    void setUserDigOut_maskSignalCarriesMaskedValue() {
        // Signal must carry the post-mask value, not the raw input.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::userDigOutChanged);
        t.setUserDigOut(0xFE);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0x0E);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD (no signal on no-op set)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotent_lineInGain_default_noSignal() {
        // setLineInGain(0) on fresh model (default 0) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(0);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_userDigOut_default_noSignal() {
        // setUserDigOut(0) on fresh model (default 0) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::userDigOutChanged);
        t.setUserDigOut(0);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_lineInGain_explicitSameValue_noSignal() {
        TransmitModel t;
        t.setLineInGain(20);
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(20);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_userDigOut_explicitSameValue_noSignal() {
        TransmitModel t;
        t.setUserDigOut(7);
        QSignalSpy spy(&t, &TransmitModel::userDigOutChanged);
        t.setUserDigOut(7);
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_lineInGain_clampedSameValue_noSignal() {
        // Set max, then attempt over-max; clamped value matches stored, no emit.
        TransmitModel t;
        t.setLineInGain(31);
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(99);  // clamps to 31, which equals m_lineInGain
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_userDigOut_maskedSameValue_noSignal() {
        // Set value 7, then attempt 0x17 (= 7 after mask); no emit.
        TransmitModel t;
        t.setUserDigOut(7);
        QSignalSpy spy(&t, &TransmitModel::userDigOutChanged);
        t.setUserDigOut(0x17);  // masks to 7, which equals m_userDigOut
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PERSISTENCE ROUND-TRIP (loadFromSettings ⇄ persistToSettings)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void firstRunDefaults_lineInGain_zero() {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.lineInGain(), 0);
    }

    void firstRunDefaults_userDigOut_zero() {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.userDigOut(), 0);
    }

    void roundTrip_lineInGain_autoPersist() {
        // After loadFromSettings, each setter auto-persists to AppSettings.
        // Verify a fresh model picks up the saved value on next load.
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setLineInGain(17);
        }
        // Verify the key was written under the per-MAC tx prefix.
        const QString key = QStringLiteral("hardware/%1/tx/LineInGain").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("17"));
        // Fresh load must restore the persisted value.
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.lineInGain(), 17);
    }

    void roundTrip_userDigOut_autoPersist() {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setUserDigOut(11);
        }
        const QString key = QStringLiteral("hardware/%1/tx/UserDigOut").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("11"));
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.userDigOut(), 11);
    }

    void roundTrip_lineInGain_persistToSettings_bulk() {
        // Bulk-write path (persistToSettings(mac)) preserves the same value.
        TransmitModel t;
        t.setLineInGain(25);
        t.persistToSettings(kMacA);
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.lineInGain(), 25);
    }

    void roundTrip_userDigOut_persistToSettings_bulk() {
        TransmitModel t;
        t.setUserDigOut(9);
        t.persistToSettings(kMacA);
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.userDigOut(), 9);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // MICPROFILEMANAGER ROUND-TRIP (Mic_LineInGain / Mic_UserDigOut)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void micProfileManager_save_writesLineInGainAndUserDigOut() {
        // Save a profile from a model with non-default values; verify the
        // bundle keys appear under the expected per-MAC paths.
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();   // seeds factory profiles
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setLineInGain(13);
        tx.setUserDigOut(5);
        QVERIFY(mgr.saveProfile(QStringLiteral("Custom"), &tx));

        const QString gainKey = QStringLiteral("hardware/%1/tx/profile/Custom/Mic_LineInGain").arg(kMacA);
        const QString digKey  = QStringLiteral("hardware/%1/tx/profile/Custom/Mic_UserDigOut").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(gainKey).toString(), QStringLiteral("13"));
        QCOMPARE(AppSettings::instance().value(digKey).toString(),  QStringLiteral("5"));
    }

    void micProfileManager_setActive_appliesLineInGainAndUserDigOut() {
        // Save profile at one set of values, then mutate the model and
        // re-activate the profile.  The model values must be restored.
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setLineInGain(22);
        tx.setUserDigOut(12);
        QVERIFY(mgr.saveProfile(QStringLiteral("Custom"), &tx));

        // Mutate model away from the saved values.
        tx.setLineInGain(0);
        tx.setUserDigOut(0);
        QCOMPARE(tx.lineInGain(), 0);
        QCOMPARE(tx.userDigOut(), 0);

        // Re-activate the profile — values must restore.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Custom"), &tx));
        QCOMPARE(tx.lineInGain(), 22);
        QCOMPARE(tx.userDigOut(), 12);
    }

    void micProfileManager_defaultProfile_lineInGainAndUserDigOutAreZero() {
        // The seeded "Default" profile carries the design defaults (both 0).
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // Force model to non-default to verify the active-profile load
        // overwrites correctly.
        tx.setLineInGain(31);
        tx.setUserDigOut(15);
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        QCOMPARE(tx.lineInGain(), 0);
        QCOMPARE(tx.userDigOut(), 0);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelLineInGainUserDigOut)
#include "tst_transmit_model_line_in_gain_user_dig_out.moc"
