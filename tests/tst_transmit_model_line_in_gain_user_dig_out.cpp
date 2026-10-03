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

    void default_lineInGain_isIndexForZeroDb() {
        // Radio codec lane: the index follows lineInBoost, whose default is
        // 0.0 dB, entry 23 of Thetis's table (SetMicGain / MakeLineInList).
        // Source: Thetis console.cs:13247, 40900-40932 [v2.10.3.15].
        TransmitModel t;
        QCOMPARE(t.lineInGain(), 23);
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
        // setLineInGain(23) on fresh model (default 23) must NOT emit.
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInGainChanged);
        t.setLineInGain(23);
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

    void firstRunDefaults_lineInGain_indexForZeroDb() {
        // Radio codec lane: the index follows lineInBoost (default 0.0 dB),
        // which is index 23 in Thetis's lineinboost list.
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.lineInGain(), 23);
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
            // -9.0 dB is index 17 ((-9.0 + 34.5) / 1.5).
            t.setLineInBoost(-9.0);
        }
        // Verify the key was written under the per-MAC tx prefix.
        const QString key = QStringLiteral("hardware/%1/tx/LineInGain").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("17"));
        // Fresh load derives the index from the persisted dB value.
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

    // Radio codec review (2026-09-30): lineInBoost is held on Thetis's
    // 1.5 dB udLineInBoost grid (setup.designer.cs:47006-47034
    // [v2.10.3.15]), at the entry the radio is sent, so the value shown and
    // the wire index agree for an off-grid write.
    void lineInBoost_offGridWrite_snapsToTheSentEntry() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::lineInBoostChanged);
        t.setLineInBoost(5.0);  // between 4.5 (index 26) and 6.0 (index 27)
        QCOMPARE(t.lineInBoost(), 4.5);
        QCOMPARE(t.lineInGain(), 26);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toDouble(), 4.5);
        t.setLineInBoost(5.5);  // nearer 6.0
        QCOMPARE(t.lineInBoost(), 6.0);
        QCOMPARE(t.lineInGain(), 27);
        t.setLineInBoost(-10.0);  // a pre-24 whole-dB step: -10.5, index 16
        QCOMPARE(t.lineInBoost(), -10.5);
        QCOMPARE(t.lineInGain(), 16);
        spy.clear();
        t.setLineInBoost(-10.4);  // the same entry: no change, no signal
        QCOMPARE(spy.count(), 0);
    }

    void lineInBoost_storedWholeDbValue_loadsOnTheGrid() {
        // A value saved in whole dB before the 1.5 dB steps.
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/tx/Line_Input_Level").arg(kMacA),
            QStringLiteral("5"));
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.lineInBoost(), 4.5);
        QCOMPARE(t.lineInGain(), 26);
    }

    void roundTrip_lineInGain_persistToSettings_bulk() {
        // Bulk-write path (persistToSettings(mac)) preserves the same value.
        TransmitModel t;
        t.setLineInBoost(3.0);  // index 25
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
        tx.setLineInBoost(-1.5);  // index 22
        tx.setUserDigOut(12);
        QVERIFY(mgr.saveProfile(QStringLiteral("Custom"), &tx));

        // Mutate model away from the saved values.
        tx.setLineInBoost(-34.5);  // index 0
        tx.setUserDigOut(0);
        QCOMPARE(tx.lineInGain(), 0);
        QCOMPARE(tx.userDigOut(), 0);

        // Re-activate the profile — values must restore.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Custom"), &tx));
        QCOMPARE(tx.lineInGain(), 22);
        QCOMPARE(tx.userDigOut(), 12);
    }

    void micProfileManager_defaultProfile_lineInGainAndUserDigOutAreDefaults() {
        // The seeded "Default" profile carries 0.0 dB line in (index 23)
        // and user dig out 0.
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // Force model to non-default to verify the active-profile load
        // overwrites correctly.
        tx.setLineInBoost(12.0);  // index 31
        tx.setUserDigOut(15);
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        QCOMPARE(tx.lineInGain(), 23);
        QCOMPARE(tx.userDigOut(), 0);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelLineInGainUserDigOut)
#include "tst_transmit_model_line_in_gain_user_dig_out.moc"
