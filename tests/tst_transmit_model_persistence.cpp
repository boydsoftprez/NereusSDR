// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_transmit_model_persistence.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::loadFromSettings(mac) /
// persistToSettings(mac) + per-setter auto-persist (L.2, Phase 3M-1b).
//
// Covers:
//   - Round-trip per persisted key (15 cases)
//   - Non-persisted keys (voxEnabled, monEnabled, micMute — 3 cases)
//   - First-run defaults (all 15 at C.1-C.5 defaults; micGainDb -6)
//   - Multi-MAC isolation (MAC A's values don't bleed into MAC B)
//
// =================================================================

#include <QtTest/QtTest>
#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
static const QString kMacB = QStringLiteral("ff:ee:dd:cc:bb:aa");

class TstTransmitModelPersistence : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // Clear all AppSettings to guarantee a clean state for the whole suite.
        AppSettings::instance().clear();
    }

    void init()
    {
        // Clear before each test for isolation.
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §1  First-run defaults — fresh AppSettings, loadFromSettings returns
    //     the C.1-C.5 defaults for all 15 persisted properties.
    // =========================================================================

    void firstRunDefaults_micGainDb_minus6()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default -6 per plan §0 row 11.
        QCOMPARE(t.micGainDb(), -6);
    }

    void firstRunDefaults_micBoost_true()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default true — console.cs:13237 [v2.10.3.13]
        QVERIFY(t.micBoost());
    }

    void firstRunDefaults_micXlr_false()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default false: matches Thetis user-visible Saturn 3.5 mm default.
        // Cite setup.designer.cs:8635 [v2.10.3.13+501e3f51]
        // (radSaturn3p5mm.Checked = true) plus the no-guard handler at
        // setup.cs:16450-16454 [v2.10.3.13+501e3f51] that clears the XLR
        // wire bit during designer init. See TransmitModel.h m_micXlr
        // comment for the constructor-vs-designer dual-source analysis.
        QVERIFY(!t.micXlr());
    }

    void firstRunDefaults_lineIn_false()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default false — console.cs:13213 [v2.10.3.13]
        QVERIFY(!t.lineIn());
    }

    void firstRunDefaults_lineInBoost_zero()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default 0.0 — console.cs:13225 [v2.10.3.13]
        QCOMPARE(t.lineInBoost(), 0.0);
    }

    void firstRunDefaults_micTipRing_true()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default true — setup.designer.cs:8683 [v2.10.3.13]
        QVERIFY(t.micTipRing());
    }

    void firstRunDefaults_micBias_false()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default false — setup.designer.cs:8779 [v2.10.3.13]
        QVERIFY(!t.micBias());
    }

    void firstRunDefaults_micPttDisabled_false()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default false — console.cs:19757 [v2.10.3.13]
        QVERIFY(!t.micPttDisabled());
    }

    void firstRunDefaults_voxThresholdDb_minus40()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default -40 — NereusSDR-original conservative starting point
        QCOMPARE(t.voxThresholdDb(), -40);
    }

    void firstRunDefaults_voxGainScalar_1()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default 1.0f — audio.cs:194 [v2.10.3.13]
        QCOMPARE(t.voxGainScalar(), 1.0f);
    }

    void firstRunDefaults_voxHangTimeMs_500()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default 500 — setup.designer.cs:45020-45024 [v2.10.3.13]
        QCOMPARE(t.voxHangTimeMs(), 500);
    }

    void firstRunDefaults_antiVoxGainDb_zero()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default 0 — NereusSDR-original safe starting point
        QCOMPARE(t.antiVoxGainDb(), 0);
    }

    // 3M-3a-iv post-bench refactor (Option A): firstRunDefaults_antiVoxSourceVax_false
    // removed alongside the antiVoxSourceVax property.

    void firstRunDefaults_monitorVolume_half()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default 0.5f — audio.cs:417 [v2.10.3.13] literal mix coefficient
        QCOMPARE(t.monitorVolume(), 0.5f);
    }

    void firstRunDefaults_micSource_Pc()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Default Pc — NereusSDR-native; always safe and available
        QCOMPARE(t.micSource(), MicSource::Pc);
    }

    // ── Two-tone test properties (3M-1c B.2) — first-run defaults ────────────

    void firstRunDefaults_twoToneFreq1_700()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.twoToneFreq1(), 700);
    }

    void firstRunDefaults_twoToneFreq2_1900()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.twoToneFreq2(), 1900);
    }

    void firstRunDefaults_twoToneLevel_zero()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // From Thetis setup.Designer.cs:61994-62003 [v2.10.3.13]
        // udTwoToneLevel.Value = 0 dB.  Phase 3M-4 Task 17 restored
        // Thetis-faithful default (was NereusSDR-original -6 dB which
        // under-drove calcc LCOLLECT).
        QCOMPARE(t.twoToneLevel(), 0.0);
    }

    void firstRunDefaults_twoTonePower_50()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // NereusSDR-original (Thetis Designer = 10 %).
        QCOMPARE(t.twoTonePower(), 50);
    }

    void firstRunDefaults_twoToneFreq2Delay_0()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.twoToneFreq2Delay(), 0);
    }

    void firstRunDefaults_twoToneInvert_true()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        // setup.Designer.cs:61963 [v2.10.3.13]: Checked = true.
        QVERIFY(t.twoToneInvert());
    }

    void firstRunDefaults_twoTonePulsed_false()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QVERIFY(!t.twoTonePulsed());
    }

    void firstRunDefaults_twoToneDrivePowerSource_DriveSlider()
    {
        // Matches Thetis console.cs:46553 [v2.10.3.13]:
        //   private DrivePowerSource _2ToneDrivePowerSource = DRIVE_SLIDER;
        TransmitModel t;
        t.loadFromSettings(kMacA);
        QCOMPARE(t.twoToneDrivePowerSource(), DrivePowerSource::DriveSlider);
    }

    // =========================================================================
    // §2  Round-trip per persisted key (15 cases)
    //     Set a non-default value, verify AppSettings has the key, then
    //     construct a fresh TransmitModel + loadFromSettings → same value.
    // =========================================================================

    void roundTrip_micGainDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicGainDb(10);
        }
        // Verify key was written
        const QString key = QStringLiteral("hardware/%1/tx/MicGain").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("10"));
        // Fresh load
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.micGainDb(), 10);
    }

    void roundTrip_micBoost()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicBoost(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(!t2.micBoost());
    }

    void roundTrip_micXlr()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicXlr(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(!t2.micXlr());
    }

    void roundTrip_lineIn()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setLineIn(true);  // flip from default false
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(t2.lineIn());
    }

    void roundTrip_lineInBoost()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setLineInBoost(6.5);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.lineInBoost(), 6.5);
    }

    void roundTrip_micTipRing()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicTipRing(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(!t2.micTipRing());
    }

    void roundTrip_micBias()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicBias(true);  // flip from default false
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(t2.micBias());
    }

    void roundTrip_micPttDisabled()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicPttDisabled(true);  // flip from default false
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(t2.micPttDisabled());
    }

    void roundTrip_voxThresholdDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setVoxThresholdDb(-20);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.voxThresholdDb(), -20);
    }

    void roundTrip_voxGainScalar()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setVoxGainScalar(2.5f);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.voxGainScalar(), 2.5f);
    }

    void roundTrip_voxHangTimeMs()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setVoxHangTimeMs(1000);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.voxHangTimeMs(), 1000);
    }

    void roundTrip_antiVoxGainDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setAntiVoxGainDb(12);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.antiVoxGainDb(), 12);
    }

    // 3M-3a-iv post-bench refactor (Option A): roundTrip_antiVoxSourceVax
    // removed alongside the antiVoxSourceVax property.

    void roundTrip_monitorVolume()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMonitorVolume(0.75f);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.monitorVolume(), 0.75f);
    }

    void roundTrip_micSource_Radio()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicSource(MicSource::Radio);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.micSource(), MicSource::Radio);
    }

    // ── Two-tone test properties (3M-1c B.2) — round-trip ────────────────────

    void roundTrip_twoToneFreq1()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneFreq1(800);  // flip from default 700
        }
        const QString key = QStringLiteral("hardware/%1/tx/TwoToneFreq1").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("800"));
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneFreq1(), 800);
    }

    void roundTrip_twoToneFreq2()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneFreq2(2100);  // flip from default 1900
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneFreq2(), 2100);
    }

    void roundTrip_twoToneLevel()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneLevel(-12.5);  // flip from default -6
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneLevel(), -12.5);
    }

    void roundTrip_twoTonePower()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoTonePower(75);  // flip from default 50
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoTonePower(), 75);
    }

    void roundTrip_twoToneFreq2Delay()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneFreq2Delay(250);  // flip from default 0
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneFreq2Delay(), 250);
    }

    void roundTrip_twoToneInvert()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneInvert(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(!t2.twoToneInvert());
    }

    void roundTrip_twoTonePulsed()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoTonePulsed(true);  // flip from default false
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QVERIFY(t2.twoTonePulsed());
    }

    void roundTrip_twoToneDrivePowerSource_Fixed()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        }
        const QString key = QStringLiteral("hardware/%1/tx/TwoToneDrivePowerOrigin").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("Fixed"));
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneDrivePowerSource(), DrivePowerSource::Fixed);
    }

    void roundTrip_twoToneDrivePowerSource_TuneSlider()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.twoToneDrivePowerSource(), DrivePowerSource::TuneSlider);
    }

    // =========================================================================
    // §3  Non-persisted keys — voxEnabled, monEnabled, micMute must NEVER
    //     write to AppSettings and must ALWAYS load at their safe defaults.
    // =========================================================================

    void nonPersisted_voxEnabled_keyAbsent()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        t.setVoxEnabled(true);  // attempt to persist — must not write key

        const QString key = QStringLiteral("hardware/%1/tx/voxEnabled").arg(kMacA);
        QVERIFY(!AppSettings::instance().value(key).isValid());
    }

    void nonPersisted_voxEnabled_alwaysLoadsDefault()
    {
        // Pre-seed a "True" value as if someone manually wrote it.
        const QString key = QStringLiteral("hardware/%1/tx/voxEnabled").arg(kMacA);
        AppSettings::instance().setValue(key, QStringLiteral("True"));

        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Must still load as false regardless of stored value.
        QVERIFY(!t.voxEnabled());
    }

    void nonPersisted_monEnabled_keyAbsent()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        t.setMonEnabled(true);  // attempt to persist — must not write key

        const QString key = QStringLiteral("hardware/%1/tx/monEnabled").arg(kMacA);
        QVERIFY(!AppSettings::instance().value(key).isValid());
    }

    void nonPersisted_monEnabled_alwaysLoadsDefault()
    {
        // Pre-seed a "True" value as if someone manually wrote it.
        const QString key = QStringLiteral("hardware/%1/tx/monEnabled").arg(kMacA);
        AppSettings::instance().setValue(key, QStringLiteral("True"));

        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Must still load as false regardless of stored value.
        QVERIFY(!t.monEnabled());
    }

    void nonPersisted_micMute_keyAbsent()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        t.setMicMute(false);  // attempt to persist — must not write key

        const QString key = QStringLiteral("hardware/%1/tx/micMute").arg(kMacA);
        QVERIFY(!AppSettings::instance().value(key).isValid());
    }

    void nonPersisted_micMute_alwaysLoadsDefault()
    {
        // Pre-seed a "False" value as if someone manually wrote it.
        const QString key = QStringLiteral("hardware/%1/tx/micMute").arg(kMacA);
        AppSettings::instance().setValue(key, QStringLiteral("False"));

        TransmitModel t;
        t.loadFromSettings(kMacA);
        // Must still load as true (mic in use) regardless of stored value.
        QVERIFY(t.micMute());
    }

    // =========================================================================
    // §4  Multi-MAC isolation — MAC A's values must not bleed into MAC B.
    // =========================================================================

    void multiMacIsolation_macBSeesDefaults()
    {
        // Set non-default values for MAC A.
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicGainDb(20);
            t.setMicBoost(false);
            t.setVoxThresholdDb(-10);
            t.setMonitorVolume(0.9f);
            t.setMicSource(MicSource::Radio);
        }

        // Fresh model loaded with MAC B — must see first-run defaults.
        TransmitModel t2;
        t2.loadFromSettings(kMacB);
        QCOMPARE(t2.micGainDb(), -6);
        QVERIFY(t2.micBoost());
        QCOMPARE(t2.voxThresholdDb(), -40);
        QCOMPARE(t2.monitorVolume(), 0.5f);
        QCOMPARE(t2.micSource(), MicSource::Pc);
    }

    void multiMacIsolation_macAPreservedAfterMacBLoad()
    {
        // Write MAC A values.
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setMicGainDb(10);
            t.setVoxHangTimeMs(750);
        }

        // Load MAC B (different values).
        {
            TransmitModel t;
            t.loadFromSettings(kMacB);
            t.setMicGainDb(-20);
            t.setVoxHangTimeMs(1500);
        }

        // Re-load MAC A — must still have original values.
        TransmitModel t3;
        t3.loadFromSettings(kMacA);
        QCOMPARE(t3.micGainDb(), 10);
        QCOMPARE(t3.voxHangTimeMs(), 750);

        // Re-load MAC B — must still have its own values.
        TransmitModel t4;
        t4.loadFromSettings(kMacB);
        QCOMPARE(t4.micGainDb(), -20);
        QCOMPARE(t4.voxHangTimeMs(), 1500);
    }

    // =========================================================================
    // §5  persistToSettings — bulk-write produces the same result as
    //     auto-persist, readable by a fresh loadFromSettings.
    // =========================================================================

    void persistToSettings_roundTrip()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        t.setMicGainDb(5);
        t.setMicBoost(false);
        t.setAntiVoxGainDb(-15);
        t.setMonitorVolume(0.3f);
        t.setMicSource(MicSource::Radio);

        // Bulk-flush again (should be idempotent / no-op since auto-persist ran)
        t.persistToSettings(kMacA);

        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.micGainDb(), 5);
        QVERIFY(!t2.micBoost());
        QCOMPARE(t2.antiVoxGainDb(), -15);
        QCOMPARE(t2.monitorVolume(), 0.3f);
        QCOMPARE(t2.micSource(), MicSource::Radio);
    }

    // =========================================================================
    // §6  Auto-persist before loadFromSettings — setters called before
    //     loadFromSettings must NOT write to AppSettings (m_persistMac empty).
    // =========================================================================

    void autoPerist_noopBeforeLoadFromSettings()
    {
        TransmitModel t;
        // No loadFromSettings call — m_persistMac is empty.
        t.setMicGainDb(15);
        t.setVoxThresholdDb(-5);

        const QString keyGain = QStringLiteral("hardware/%1/tx/MicGain").arg(kMacA);
        const QString keyVox  = QStringLiteral("hardware/%1/tx/Dexp_Threshold").arg(kMacA);
        QVERIFY(!AppSettings::instance().value(keyGain).isValid());
        QVERIFY(!AppSettings::instance().value(keyVox).isValid());
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelPersistence)
#include "tst_transmit_model_persistence.moc"
