// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_mic_profile_manager.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for MicProfileManager (Phase 3M-1c chunk F).
//
// Covers F.1-F.6:
//   F.1  Class skeleton + load/save/delete/setActive operations
//   F.2  Save flow with overwrite + comma-strip
//   F.3  Delete flow with last-profile guard
//   F.4  setActiveProfile flow (writes values + updates active key)
//   F.5  First-launch "Default" profile seed
//   F.6  Per-MAC isolation (two MACs maintain independent lists)
//
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
static const QString kMacB = QStringLiteral("ff:ee:dd:cc:bb:aa");

static QString profileKey(const QString& mac, const QString& name, const QString& field)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(mac, name, field);
}

static QString activeKey(const QString& mac)
{
    return QStringLiteral("hardware/%1/tx/profile/active").arg(mac);
}

static QString manifestKey(const QString& mac)
{
    return QStringLiteral("hardware/%1/tx/profile/_names").arg(mac);
}

class TstMicProfileManager : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §F.5  First-launch "Default" profile seed
    // §3M-3a-i Batch 4 (A.2)  Plus 21 Thetis factory profiles
    // =========================================================================

    void firstLaunch_seedsDefaultPlusFactoryProfiles()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        const QStringList names = mgr.profileNames();
        // Default + 20 Thetis factory profiles ported from
        // database.cs:AddTXProfileTable [v2.10.3.13] (Default DX +
        // 19 entries from the bIndcludeExtraProfiles block).
        QCOMPARE(names.size(), 21);
        // Default remains the active profile so existing-user first-run
        // behaviour is unchanged.
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default"));

        // Verify every expected factory profile is present.
        const QStringList expected = {
            QStringLiteral("Default"),
            QStringLiteral("Default DX"),
            QStringLiteral("Digi 1K@1500"),
            QStringLiteral("Digi 1K@2210"),
            QStringLiteral("AM"),
            QStringLiteral("Conventional"),
            QStringLiteral("D-104"),
            QStringLiteral("D-104+CPDR"),
            QStringLiteral("D-104+EQ"),
            QStringLiteral("DX / Contest"),
            QStringLiteral("ESSB"),
            QStringLiteral("HC4-5"),
            QStringLiteral("HC4-5+CPDR"),
            QStringLiteral("PR40+W2IHY"),
            QStringLiteral("PR40+W2IHY+CPDR"),
            QStringLiteral("PR781+EQ"),
            QStringLiteral("PR781+EQ+CPDR"),
            QStringLiteral("SSB 2.8k CFC"),
            QStringLiteral("SSB 3.0k CFC"),
            QStringLiteral("SSB 3.3k CFC"),
            QStringLiteral("AM 10k CFC"),
        };
        for (const QString& name : expected) {
            QVERIFY2(names.contains(name),
                     qPrintable(QStringLiteral("missing factory profile: ") + name));
        }
    }

    // Bit-for-bit regression trap: pick D-104 (a profile with several
    // non-default values across the EQ + MicGain keys) and verify the
    // seed values are byte-identical to the Thetis source row.
    // From Thetis database.cs:5926-6148 [v2.10.3.13] (D-104 profile block).
    void firstLaunch_d104FactoryProfileMatchesThetisVerbatim()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        // EQ enable false (D-104 ships EQ off — the user enables via D-104+EQ).
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TXEQEnabled")).toString(),
                 QStringLiteral("False"));
        // From Thetis database.cs:5935 [v2.10.3.13] (TXEQPreamp = -6).
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TXEQPreamp")).toString(),
                 QStringLiteral("-6"));
        // From Thetis database.cs:5936-5938 [v2.10.3.13] (TXEQ1=7, TXEQ2=3, TXEQ3=4).
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TXEQ1")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TXEQ2")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TXEQ3")).toString(),
                 QStringLiteral("4"));
        // From Thetis database.cs:5939-5945 [v2.10.3.13] (TXEQ4..10 all 0).
        for (int i = 4; i <= 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "D-104",
                             QStringLiteral("TXEQ%1").arg(i))).toString(),
                     QStringLiteral("0"));
        }
        // Default freq grid inherits from defaultProfileValues — verify
        // a couple to confirm the merge worked.
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TxEqFreq1")).toString(),
                 QStringLiteral("32"));
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "TxEqFreq6")).toString(),
                 QStringLiteral("1000"));
        // From Thetis database.cs:5960 [v2.10.3.13] (MicGain = 25).
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "MicGain")).toString(),
                 QStringLiteral("25"));
        // From Thetis database.cs:5963-5973 [v2.10.3.13] (Lev/ALC unchanged from defaults).
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "Lev_On")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "D-104", "ALC_MaximumGain")).toString(),
                 QStringLiteral("3"));
    }

    // SSB 2.8k CFC has a custom 10-band EQ shape AND a custom freq grid
    // (30/70/300/500/1000/2000/3000/4000/5000/6000) — verify it matches
    // the Thetis source row exactly.
    // From Thetis database.cs:8445-8667 [v2.10.3.13].
    void firstLaunch_ssb28CfcFactoryProfileMatchesThetisVerbatim()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "TXEQEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "TXEQPreamp")).toString(),
                 QStringLiteral("4"));
        // EQ shape: -9, -6, -4, -3, -2, -1, -1, -1, -2, -2.
        const int kExpectedG[10] = {-9, -6, -4, -3, -2, -1, -1, -1, -2, -2};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC",
                             QStringLiteral("TXEQ%1").arg(i + 1))).toString(),
                     QString::number(kExpectedG[i]));
        }
        // Freq grid: 30/70/300/500/1000/2000/3000/4000/5000/6000.
        const int kExpectedF[10] = {30, 70, 300, 500, 1000, 2000, 3000, 4000, 5000, 6000};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC",
                             QStringLiteral("TxEqFreq%1").arg(i + 1))).toString(),
                     QString::number(kExpectedF[i]));
        }
    }

    // Digi profiles ship with Lev_On=False (data modes don't want the
    // analog leveler firing on dial tones).  Verify the override took
    // and the override merge actually replaced the Default's "True".
    // From Thetis database.cs:5046 [v2.10.3.13] (Digi 1K@1500 Lev_On = false).
    void firstLaunch_digiProfileOverridesLevOn()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Digi 1K@1500", "Lev_On")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Digi 1K@2210", "Lev_On")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Digi 1K@1500", "MicGain")).toString(),
                 QStringLiteral("5"));
    }

    void firstLaunch_defaultProfileMatchesDocumentedDefaults()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        // F.5: every documented default must be written to AppSettings under
        // hardware/<mac>/tx/profile/Default/<key>.  Spot-check each row of
        // the documented table.
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Default", "MicGain")).toString(),
                 QStringLiteral("-6"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_Input_Boost")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_XLR")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Line_Input_On")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Line_Input_Level")).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_TipRing")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_Bias")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_PTT_Disabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Dexp_Threshold")).toString(),
                 QStringLiteral("-40"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "VOX_GainScalar")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "VOX_HangTime")).toString(),
                 QStringLiteral("500"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "AntiVox_Gain")).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "AntiVox_Source_VAX")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "MonitorVolume")).toString(),
                 QStringLiteral("0.5"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Mic_Source")).toString(),
                 QStringLiteral("Pc"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneFreq1")).toString(),
                 QStringLiteral("700"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneFreq2")).toString(),
                 QStringLiteral("1900"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneLevel")).toString(),
                 QStringLiteral("-6"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoTonePower")).toString(),
                 QStringLiteral("50"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneFreq2Delay")).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneInvert")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoTonePulsed")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TwoToneDrivePowerOrigin")).toString(),
                 QStringLiteral("DriveSlider"));
    }

    void subsequentLaunch_doesNotOverwriteDefault()
    {
        // First launch.
        {
            MicProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load();
        }
        // Manually mutate one of the Default profile values.
        AppSettings::instance().setValue(profileKey(kMacA, "Default", "MicGain"),
                                          QStringLiteral("42"));

        // Second launch: load should not overwrite the existing Default values
        // and should not re-seed the factory profiles.
        MicProfileManager mgr2;
        mgr2.setMacAddress(kMacA);
        mgr2.load();

        QCOMPARE(AppSettings::instance().value(profileKey(kMacA, "Default", "MicGain")).toString(),
                 QStringLiteral("42"));
        // Default + 20 factory profiles seeded on the first launch persist.
        QCOMPARE(mgr2.profileNames().size(), 21);
    }

    void firstLaunch_emitsProfileListChanged()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);
        mgr.load();
        QCOMPARE(listSpy.count(), 1);
    }

    void defaultProfileValues_staticHelperMatchesSeed()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // Spot-check a few representative keys.
        QCOMPARE(defs.value("MicGain").toString(), QStringLiteral("-6"));
        QCOMPARE(defs.value("Mic_Input_Boost").toString(), QStringLiteral("True"));
        QCOMPARE(defs.value("VOX_HangTime").toString(), QStringLiteral("500"));
        QCOMPARE(defs.value("TwoToneFreq1").toString(), QStringLiteral("700"));
        QCOMPARE(defs.value("TwoToneDrivePowerOrigin").toString(),
                 QStringLiteral("DriveSlider"));
        // 96 keys total: 23 mic/VOX/MON/two-tone (3M-1c) + 27 EQ/Lev/ALC
        // (3M-3a-i G) + 1 TXParaEQData (3M-3a-ii follow-up Batch 6) +
        // 41 CFC/CPDR/CESSB/PhRot (3M-3a-ii G) +
        // 2 FilterLow/FilterHigh (Plan 4 Cluster A D1) +
        // 2 line_in_gain/user_dig_out (P1 full-parity Task 2.4).
        QCOMPARE(defs.size(), 96);
    }

    // ─────────────────────────────────────────────────────────────────────
    // §G  3M-3a-i — TX EQ + Leveler + ALC keys (27 new keys bundled)
    // ─────────────────────────────────────────────────────────────────────

    void firstLaunch_seedsEqLevAlcDefaults()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        // Spot-check the new EQ/Lev/ALC defaults sourced from
        // Thetis database.cs:4552-4594 [v2.10.3.13] + WDSP TXA.c:111-128.
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQPreamp")).toString(),
                 QStringLiteral("0"));
        // WDSP TXA.c:113 default_G[1..10] = {-12, -12, -12, -1, +1, +4, +9, +12, -10, -10}
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQ1")).toString(),
                 QStringLiteral("-12"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQ4")).toString(),
                 QStringLiteral("-1"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQ8")).toString(),
                 QStringLiteral("12"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TXEQ10")).toString(),
                 QStringLiteral("-10"));
        // WDSP TXA.c:112 default_F[1..10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000}
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TxEqFreq1")).toString(),
                 QStringLiteral("32"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TxEqFreq6")).toString(),
                 QStringLiteral("1000"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "TxEqFreq10")).toString(),
                 QStringLiteral("16000"));
        // Leveler — database.cs:4584-4588 [v2.10.3.13]
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Lev_On")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Lev_MaxGain")).toString(),
                 QStringLiteral("15"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "Lev_Decay")).toString(),
                 QStringLiteral("100"));
        // ALC — database.cs:4592-4594 [v2.10.3.13]
        QCOMPARE(s.value(profileKey(kMacA, "Default", "ALC_MaximumGain")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(s.value(profileKey(kMacA, "Default", "ALC_Decay")).toString(),
                 QStringLiteral("10"));
    }

    void saveProfile_capturesEqLevAlcLiveKeys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // Mutate EQ/Lev/ALC properties.
        tx.setTxEqEnabled(true);
        tx.setTxEqPreamp(5);
        tx.setTxEqBand(0, -8);
        tx.setTxEqBand(9, 7);
        tx.setTxEqFreq(0, 50);
        tx.setTxEqFreq(9, 14000);
        tx.setTxLevelerOn(false);
        tx.setTxLevelerMaxGain(8);
        tx.setTxLevelerDecay(250);
        tx.setTxAlcMaxGain(20);
        tx.setTxAlcDecay(25);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("EqProfile", &tx);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TXEQEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TXEQPreamp")).toString(),
                 QStringLiteral("5"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TXEQ1")).toString(),
                 QStringLiteral("-8"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TXEQ10")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TxEqFreq1")).toString(),
                 QStringLiteral("50"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "TxEqFreq10")).toString(),
                 QStringLiteral("14000"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "Lev_On")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "Lev_MaxGain")).toString(),
                 QStringLiteral("8"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "Lev_Decay")).toString(),
                 QStringLiteral("250"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "ALC_MaximumGain")).toString(),
                 QStringLiteral("20"));
        QCOMPARE(s.value(profileKey(kMacA, "EqProfile", "ALC_Decay")).toString(),
                 QStringLiteral("25"));
    }

    void setActiveProfile_appliesEqLevAlcValuesToTransmitModel()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setTxEqEnabled(true);
        tx.setTxEqPreamp(8);
        tx.setTxEqBand(2, -3);
        tx.setTxEqFreq(2, 200);
        tx.setTxLevelerOn(false);
        tx.setTxLevelerMaxGain(10);
        tx.setTxLevelerDecay(300);
        tx.setTxAlcMaxGain(12);
        tx.setTxAlcDecay(20);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("MyEq", &tx);

        // Mutate to other values.
        tx.setTxEqEnabled(false);
        tx.setTxEqPreamp(0);
        tx.setTxEqBand(2, 5);
        tx.setTxEqFreq(2, 100);
        tx.setTxLevelerOn(true);
        tx.setTxLevelerMaxGain(15);
        tx.setTxLevelerDecay(100);
        tx.setTxAlcMaxGain(3);
        tx.setTxAlcDecay(10);

        QVERIFY(mgr.setActiveProfile("MyEq", &tx));
        QCOMPARE(tx.txEqEnabled(), true);
        QCOMPARE(tx.txEqPreamp(), 8);
        QCOMPARE(tx.txEqBand(2), -3);
        QCOMPARE(tx.txEqFreq(2), 200);
        QCOMPARE(tx.txLevelerOn(), false);
        QCOMPARE(tx.txLevelerMaxGain(), 10);
        QCOMPARE(tx.txLevelerDecay(), 300);
        QCOMPARE(tx.txAlcMaxGain(), 12);
        QCOMPARE(tx.txAlcDecay(), 20);
    }

    void deleteProfile_removesEqLevAlcKeys()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ToDelete", &tx);

        // Spot-check a couple of EQ keys are present.
        QVERIFY(AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "TXEQEnabled")));
        QVERIFY(AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "Lev_MaxGain")));

        QVERIFY(mgr.deleteProfile("ToDelete"));

        // All EQ/Lev/ALC keys for the deleted profile must be removed.
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "TXEQEnabled")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "TXEQ1")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "TxEqFreq1")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "Lev_On")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "Lev_MaxGain")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "Lev_Decay")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "ALC_MaximumGain")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "ALC_Decay")));
    }

    // =========================================================================
    // §F.1  Class skeleton + load/save round-trip
    // =========================================================================

    void saveProfile_capturesAllLiveKeys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);

        // Set non-default values on every live key.
        tx.setMicGainDb(10);
        tx.setMicBoost(false);
        tx.setMicXlr(false);
        tx.setLineIn(true);
        tx.setLineInBoost(6.5);
        tx.setMicTipRing(false);
        tx.setMicBias(true);
        tx.setMicPttDisabled(true);
        tx.setVoxThresholdDb(-20);
        tx.setVoxGainScalar(2.5f);
        tx.setVoxHangTimeMs(1000);
        tx.setAntiVoxGainDb(12);
        tx.setAntiVoxSourceVax(true);
        tx.setMonitorVolume(0.75f);
        tx.setMicSource(MicSource::Radio);
        tx.setTwoToneFreq1(800);
        tx.setTwoToneFreq2(2100);
        tx.setTwoToneLevel(-12.5);
        tx.setTwoTonePower(75);
        tx.setTwoToneFreq2Delay(250);
        tx.setTwoToneInvert(false);
        tx.setTwoTonePulsed(true);
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("MyProfile", &tx);

        // Verify the profile keys are written.
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "MicGain")).toString(),
                 QStringLiteral("10"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_Input_Boost")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_XLR")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Line_Input_On")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Line_Input_Level")).toString(),
                 QStringLiteral("6.5"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_TipRing")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_Bias")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_PTT_Disabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Dexp_Threshold")).toString(),
                 QStringLiteral("-20"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "VOX_GainScalar")).toString(),
                 QStringLiteral("2.5"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "VOX_HangTime")).toString(),
                 QStringLiteral("1000"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "AntiVox_Gain")).toString(),
                 QStringLiteral("12"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "AntiVox_Source_VAX")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "MonitorVolume")).toString(),
                 QStringLiteral("0.75"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "Mic_Source")).toString(),
                 QStringLiteral("Radio"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneFreq1")).toString(),
                 QStringLiteral("800"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneFreq2")).toString(),
                 QStringLiteral("2100"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneLevel")).toString(),
                 QStringLiteral("-12.5"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoTonePower")).toString(),
                 QStringLiteral("75"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneFreq2Delay")).toString(),
                 QStringLiteral("250"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneInvert")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoTonePulsed")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "MyProfile", "TwoToneDrivePowerOrigin")).toString(),
                 QStringLiteral("Fixed"));

        // Profile list now contains Default + 20 factory profiles + MyProfile = 22.
        QCOMPARE(mgr.profileNames().size(), 22);
        QVERIFY(mgr.profileNames().contains("MyProfile"));
        QVERIFY(mgr.profileNames().contains("Default"));
        QVERIFY(mgr.profileNames().contains("D-104+EQ"));  // factory profile
    }

    void saveProfile_emitsProfileListChanged()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);
        mgr.saveProfile("Profile1", &tx);
        QCOMPARE(listSpy.count(), 1);
    }

    // =========================================================================
    // §F.2  Save flow with overwrite + comma-strip
    // =========================================================================

    void saveProfile_overwriteExistingDoesNotEmitDuplicate()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setMicGainDb(10);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("MyProfile", &tx);

        // Update model + save again to overwrite.
        tx.setMicGainDb(20);
        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);
        mgr.saveProfile("MyProfile", &tx);

        // Overwrite path may emit profileListChanged or not — Thetis has no
        // membership change in the overwrite path; check value updated.
        // Per F.2 contract: "Overwrites if the name already exists.  Return true."
        QCOMPARE(AppSettings::instance().value(profileKey(kMacA, "MyProfile", "MicGain")).toString(),
                 QStringLiteral("20"));
        // Profile count remains the same (Default + 20 factory + MyProfile = 22).
        QCOMPARE(mgr.profileNames().size(), 22);
    }

    void saveProfile_stripsCommas()
    {
        // Thetis precedent: name = name.replace(",", "_") for TCI safety.
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        const bool ok = mgr.saveProfile("Bad,Name", &tx);
        QVERIFY(ok);
        QVERIFY(mgr.profileNames().contains("Bad_Name"));
        QVERIFY(!mgr.profileNames().contains("Bad,Name"));
    }

    // =========================================================================
    // §F.3  Delete flow with last-profile guard
    // =========================================================================

    // After §F.5 + 3M-3a-i Batch 4 (A.2): first-launch seeds Default + 20
    // factory profiles, so the "last remaining" state must be REACHED by
    // deleting 20 profiles first.  This still verifies the F.3 contract
    // (last-remaining guard fires + verbatim Thetis warning).
    void deleteProfile_lastRemainingIsRefused()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        // Default + 20 factory profiles seeded.  Delete all but one to
        // reach the last-remaining state.
        const QStringList all = mgr.profileNames();
        QCOMPARE(all.size(), 21);
        for (const QString& name : all) {
            if (name == QStringLiteral("Default")) {
                continue;
            }
            QVERIFY(mgr.deleteProfile(name));
        }
        QCOMPARE(mgr.profileNames().size(), 1);

        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);

        // Capture qCWarning output so we can verify the verbatim Thetis string.
        // (QtMessageHandler is a global setter; restore it after.)
        QString lastWarning;
        QtMessageHandler oldHandler = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
                Q_UNUSED(ctx);
                if (type == QtWarningMsg) {
                    s_capturedWarning = msg;
                }
            });

        s_capturedWarning.clear();
        const bool ok = mgr.deleteProfile("Default");
        qInstallMessageHandler(oldHandler);

        QCOMPARE(ok, false);
        QCOMPARE(listSpy.count(), 0);
        QCOMPARE(mgr.profileNames().size(), 1);
        // Verbatim Thetis string per F.3 spec.
        QVERIFY2(s_capturedWarning.contains(
                     QStringLiteral("It is not possible to delete the last remaining TX profile")),
                 qPrintable(QStringLiteral("captured warning was: ") + s_capturedWarning));
    }

    void deleteProfile_removesProfileKeys()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ToDelete", &tx);

        // Verify it exists.
        QVERIFY(AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "MicGain")));

        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);
        const bool ok = mgr.deleteProfile("ToDelete");
        QCOMPARE(ok, true);
        QCOMPARE(listSpy.count(), 1);

        // All keys for the profile should be removed.
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "MicGain")));
        QVERIFY(!AppSettings::instance().contains(profileKey(kMacA, "ToDelete", "VOX_HangTime")));
        QVERIFY(!mgr.profileNames().contains("ToDelete"));
    }

    void deleteProfile_activeFallsBackToFirstRemaining()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ProfileA", &tx);
        mgr.saveProfile("ProfileB", &tx);

        // Activate ProfileA.
        QVERIFY(mgr.setActiveProfile("ProfileA", &tx));
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("ProfileA"));

        // Delete the active profile.
        QSignalSpy activeSpy(&mgr, &MicProfileManager::activeProfileChanged);
        const bool ok = mgr.deleteProfile("ProfileA");
        QCOMPARE(ok, true);
        // Active falls back to the first remaining (lexicographically: Default).
        QCOMPARE(activeSpy.count(), 1);
        QVERIFY(mgr.activeProfileName() != QStringLiteral("ProfileA"));
        QVERIFY(mgr.profileNames().contains(mgr.activeProfileName()));
    }

    // =========================================================================
    // §F.4  setActiveProfile flow
    // =========================================================================

    void setActiveProfile_writesActiveKey()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("OtherProfile", &tx);

        QSignalSpy spy(&mgr, &MicProfileManager::activeProfileChanged);
        const bool ok = mgr.setActiveProfile("OtherProfile", &tx);
        QCOMPARE(ok, true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("OtherProfile"));
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("OtherProfile"));
        QCOMPARE(AppSettings::instance().value(activeKey(kMacA)).toString(),
                 QStringLiteral("OtherProfile"));
    }

    void setActiveProfile_appliesValuesToTransmitModel()
    {
        // Save profile A with a specific MicGain
        TransmitModel txA;
        txA.loadFromSettings(kMacA);
        txA.setMicGainDb(15);
        txA.setVoxHangTimeMs(800);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("Profile15", &txA);

        // Mutate model to different values.
        txA.setMicGainDb(-30);
        txA.setVoxHangTimeMs(100);

        // Activate profile — should restore the saved values.
        QVERIFY(mgr.setActiveProfile("Profile15", &txA));
        QCOMPARE(txA.micGainDb(), 15);
        QCOMPARE(txA.voxHangTimeMs(), 800);
    }

    void setActiveProfile_unknownProfileReturnsFalse()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QSignalSpy spy(&mgr, &MicProfileManager::activeProfileChanged);
        const bool ok = mgr.setActiveProfile("Nonexistent", &tx);
        QCOMPARE(ok, false);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default"));
    }

    void load_restoresActiveProfileFromSettings()
    {
        // Seed: write the active key directly + create both profiles.
        {
            TransmitModel tx;
            MicProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load();
            mgr.saveProfile("OtherProfile", &tx);
            mgr.setActiveProfile("OtherProfile", &tx);
        }
        // Fresh manager — load should pick up "OtherProfile" as active.
        MicProfileManager mgr2;
        mgr2.setMacAddress(kMacA);
        mgr2.load();
        QCOMPARE(mgr2.activeProfileName(), QStringLiteral("OtherProfile"));
    }

    // =========================================================================
    // §F.6  Per-MAC isolation
    // =========================================================================

    void perMacIsolation_independentProfileLists()
    {
        // MAC A: Default + ProfileA1 + ProfileA2
        {
            TransmitModel tx;
            MicProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load();
            mgr.saveProfile("ProfileA1", &tx);
            mgr.saveProfile("ProfileA2", &tx);
        }
        // MAC B: Default + ProfileB1
        {
            TransmitModel tx;
            MicProfileManager mgr;
            mgr.setMacAddress(kMacB);
            mgr.load();
            mgr.saveProfile("ProfileB1", &tx);
        }

        // Reload MAC A — should still have its own list.
        // Default + 20 factory + 2 user profiles = 23.
        MicProfileManager mgrA;
        mgrA.setMacAddress(kMacA);
        mgrA.load();
        QStringList namesA = mgrA.profileNames();
        QCOMPARE(namesA.size(), 23);
        QVERIFY(namesA.contains("Default"));
        QVERIFY(namesA.contains("ProfileA1"));
        QVERIFY(namesA.contains("ProfileA2"));
        QVERIFY(!namesA.contains("ProfileB1"));

        // Reload MAC B — should have its own list.
        // Default + 20 factory + 1 user profile = 22.
        MicProfileManager mgrB;
        mgrB.setMacAddress(kMacB);
        mgrB.load();
        QStringList namesB = mgrB.profileNames();
        QCOMPARE(namesB.size(), 22);
        QVERIFY(namesB.contains("Default"));
        QVERIFY(namesB.contains("ProfileB1"));
        QVERIFY(!namesB.contains("ProfileA1"));
    }

private:
    static QString s_capturedWarning;
};

// File-static used by deleteProfile_lastRemainingIsRefused message handler.
QString TstMicProfileManager::s_capturedWarning;

QTEST_APPLESS_MAIN(TstMicProfileManager)
#include "tst_mic_profile_manager.moc"
