// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_mic_profile_manager_dexp_round_trip.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-iii Task 11 — round-trip tests for the 11 new DEXP
// bundle keys added to MicProfileManager (envelope, gate ratios,
// look-ahead, side-channel filter).
//
// Coverage:
//   1. Bundle keys complete  — kKeys contains all 11 new keys
//                              (asserted via defaultProfileValues()).
//   2. Defaults full         — defaultProfileValues() returns every key
//                              with the exact value from
//                              setup.Designer.cs:44755-45250 [v2.10.3.13].
//   3. Capture round-trip    — set 11 TM properties to non-default
//                              values, save profile, assert all 11
//                              bundle keys read back from AppSettings
//                              with the expected stringified values.
//   4. Apply round-trip      — save a profile under non-default values,
//                              mutate TM to defaults, setActiveProfile,
//                              assert all 11 TM getters return the
//                              saved values.
//   5. Boolean serialization — "True" / "False" round-trip sanity for
//                              the 3 new boolean keys (DEXP_Enabled,
//                              DEXP_LookAheadEnabled,
//                              DEXP_SideChannelFilterEnabled).
//
// Source-first cites: every default value asserted matches a specific
// row in /Users/j.j.boyd/Thetis/Project Files/Source/Console/setup.Designer.cs
// [v2.10.3.13].
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

static QString profileKey(const QString& mac, const QString& name, const QString& field)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(mac, name, field);
}

class TstMicProfileManagerDexpRoundTrip : public QObject {
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
    // §1  Bundle keys complete — kKeys contains all 11 new keys
    //
    // We can't introspect the private kKeys list directly, but
    // defaultProfileValues() iterates over the same shape — every key
    // bundled in kKeys has a corresponding default in defaultProfileValues().
    // So asserting "all 11 keys appear in defaultProfileValues()" indirectly
    // asserts they're in the bundle.
    // =========================================================================

    void bundleKeys_complete()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();

        // Envelope (4) — Task 7.
        QVERIFY(defs.contains(QStringLiteral("DEXP_Enabled")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_DetectorTauMs")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_AttackTimeMs")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_ReleaseTimeMs")));

        // Gate ratios (2) — Task 8.
        QVERIFY(defs.contains(QStringLiteral("DEXP_ExpansionRatioDb")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_HysteresisRatioDb")));

        // Look-ahead (2) — Task 9.
        QVERIFY(defs.contains(QStringLiteral("DEXP_LookAheadEnabled")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_LookAheadMs")));

        // Side-channel filter (3) — Task 10.
        QVERIFY(defs.contains(QStringLiteral("DEXP_LowCutHz")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_HighCutHz")));
        QVERIFY(defs.contains(QStringLiteral("DEXP_SideChannelFilterEnabled")));

        // Total bundle was 96 (CFC round-trip baseline);
        // adding 11 DEXP keys brings it to 107.
        QCOMPARE(defs.size(), 107);
    }

    // =========================================================================
    // §2  defaultProfileValues full — exact value from Thetis Designer.
    // =========================================================================

    void defaults_envelope4Keys()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // setup.Designer.cs:45140 [v2.10.3.13] — chkDEXPEnable (no Checked= setter, so default false).
        QCOMPARE(defs.value("DEXP_Enabled").toString(),       QStringLiteral("False"));
        // setup.Designer.cs:45093 [v2.10.3.13] — udDEXPDetTau.Value = 20.
        QCOMPARE(defs.value("DEXP_DetectorTauMs").toString(), QStringLiteral("20"));
        // setup.Designer.cs:45050 [v2.10.3.13] — udDEXPAttack.Value = 2.
        QCOMPARE(defs.value("DEXP_AttackTimeMs").toString(),  QStringLiteral("2"));
        // setup.Designer.cs:44990 [v2.10.3.13] — udDEXPRelease.Value = 100.
        QCOMPARE(defs.value("DEXP_ReleaseTimeMs").toString(), QStringLiteral("100"));
    }

    void defaults_gateRatios2Keys()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // setup.Designer.cs:44900 [v2.10.3.13] — udDEXPExpansionRatio.Value = 10.
        QCOMPARE(defs.value("DEXP_ExpansionRatioDb").toString(),  QStringLiteral("10"));
        // setup.Designer.cs:44869 [v2.10.3.13] — udDEXPHysteresisRatio.Value scaled to 2.0.
        QCOMPARE(defs.value("DEXP_HysteresisRatioDb").toString(), QStringLiteral("2"));
    }

    void defaults_lookAhead2Keys()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // setup.Designer.cs:44808 [v2.10.3.13] — chkDEXPLookAheadEnable.Checked = true.
        QCOMPARE(defs.value("DEXP_LookAheadEnabled").toString(), QStringLiteral("True"));
        // setup.Designer.cs:44788 [v2.10.3.13] — udDEXPLookAhead.Value = 60.
        QCOMPARE(defs.value("DEXP_LookAheadMs").toString(),      QStringLiteral("60"));
    }

    void defaults_sideChannelFilter3Keys()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // setup.Designer.cs:45240 [v2.10.3.13] — udSCFLowCut.Value = 500.
        QCOMPARE(defs.value("DEXP_LowCutHz").toString(),                 QStringLiteral("500"));
        // setup.Designer.cs:45210 [v2.10.3.13] — udSCFHighCut.Value = 1500.
        QCOMPARE(defs.value("DEXP_HighCutHz").toString(),                QStringLiteral("1500"));
        // setup.Designer.cs:45250 [v2.10.3.13] — chkSCFEnable.Checked = true.
        QCOMPARE(defs.value("DEXP_SideChannelFilterEnabled").toString(), QStringLiteral("True"));
    }

    // =========================================================================
    // §3  Capture round-trip
    //
    // Set every one of the 11 new TM properties to a non-default value,
    // saveProfile via the public API (which calls captureLiveValues
    // internally), then read the AppSettings bundle keys back and
    // compare to the stringified value.
    // =========================================================================

    void capture_envelope_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpEnabled(true);
        tx.setDexpDetectorTauMs(35.0);
        tx.setDexpAttackTimeMs(8.0);
        tx.setDexpReleaseTimeMs(250.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpEnv"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "DexpEnv", "DEXP_Enabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpEnv", "DEXP_DetectorTauMs")).toString(),
                 QStringLiteral("35"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpEnv", "DEXP_AttackTimeMs")).toString(),
                 QStringLiteral("8"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpEnv", "DEXP_ReleaseTimeMs")).toString(),
                 QStringLiteral("250"));
    }

    void capture_gateRatios_2keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpExpansionRatioDb(20.0);
        tx.setDexpHysteresisRatioDb(5.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpRatio"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "DexpRatio", "DEXP_ExpansionRatioDb")).toString(),
                 QStringLiteral("20"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpRatio", "DEXP_HysteresisRatioDb")).toString(),
                 QStringLiteral("5"));
    }

    void capture_lookAhead_2keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpLookAheadEnabled(false);
        tx.setDexpLookAheadMs(120.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpLA"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "DexpLA", "DEXP_LookAheadEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpLA", "DEXP_LookAheadMs")).toString(),
                 QStringLiteral("120"));
    }

    void capture_sideChannelFilter_3keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpLowCutHz(300.0);
        tx.setDexpHighCutHz(2800.0);
        tx.setDexpSideChannelFilterEnabled(false);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpSCF"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "DexpSCF", "DEXP_LowCutHz")).toString(),
                 QStringLiteral("300"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpSCF", "DEXP_HighCutHz")).toString(),
                 QStringLiteral("2800"));
        QCOMPARE(s.value(profileKey(kMacA, "DexpSCF", "DEXP_SideChannelFilterEnabled")).toString(),
                 QStringLiteral("False"));
    }

    // =========================================================================
    // §4  Apply round-trip
    //
    // Save a profile with non-default 11 keys, mutate TM, then
    // setActiveProfile and assert TM getters return the saved values.
    // =========================================================================

    void apply_envelope_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpEnabled(true);
        tx.setDexpDetectorTauMs(45.0);
        tx.setDexpAttackTimeMs(15.0);
        tx.setDexpReleaseTimeMs(400.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpEnvApply"), &tx));

        // Mutate TM back to defaults.
        tx.setDexpEnabled(false);
        tx.setDexpDetectorTauMs(20.0);
        tx.setDexpAttackTimeMs(2.0);
        tx.setDexpReleaseTimeMs(100.0);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("DexpEnvApply"), &tx));
        QCOMPARE(tx.dexpEnabled(),         true);
        QCOMPARE(tx.dexpDetectorTauMs(),   45.0);
        QCOMPARE(tx.dexpAttackTimeMs(),    15.0);
        QCOMPARE(tx.dexpReleaseTimeMs(),   400.0);
    }

    void apply_gateRatios_2keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpExpansionRatioDb(25.0);
        tx.setDexpHysteresisRatioDb(7.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpRatioApply"), &tx));

        tx.setDexpExpansionRatioDb(10.0);
        tx.setDexpHysteresisRatioDb(2.0);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("DexpRatioApply"), &tx));
        QCOMPARE(tx.dexpExpansionRatioDb(),  25.0);
        QCOMPARE(tx.dexpHysteresisRatioDb(),  7.0);
    }

    void apply_lookAhead_2keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpLookAheadEnabled(false);
        tx.setDexpLookAheadMs(150.0);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpLAApply"), &tx));

        tx.setDexpLookAheadEnabled(true);
        tx.setDexpLookAheadMs(60.0);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("DexpLAApply"), &tx));
        QCOMPARE(tx.dexpLookAheadEnabled(), false);
        QCOMPARE(tx.dexpLookAheadMs(),      150.0);
    }

    void apply_sideChannelFilter_3keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setDexpLowCutHz(250.0);
        tx.setDexpHighCutHz(3500.0);
        tx.setDexpSideChannelFilterEnabled(false);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpSCFApply"), &tx));

        tx.setDexpLowCutHz(500.0);
        tx.setDexpHighCutHz(1500.0);
        tx.setDexpSideChannelFilterEnabled(true);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("DexpSCFApply"), &tx));
        QCOMPARE(tx.dexpLowCutHz(),                  250.0);
        QCOMPARE(tx.dexpHighCutHz(),                 3500.0);
        QCOMPARE(tx.dexpSideChannelFilterEnabled(),  false);
    }

    // End-to-end full-bundle round trip — every one of the 11 TM properties
    // mutates, gets saved into a profile, gets reset, gets restored from the
    // profile.  Catches accidental key/setter swaps that per-section tests
    // would miss when a swap stays within one section.
    void apply_all11Keys_endToEnd()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // Pick distinct values for every property so a swap regresses.
        tx.setDexpEnabled(true);
        tx.setDexpDetectorTauMs(33.0);
        tx.setDexpAttackTimeMs(11.0);
        tx.setDexpReleaseTimeMs(333.0);
        tx.setDexpExpansionRatioDb(22.0);
        tx.setDexpHysteresisRatioDb(6.0);
        tx.setDexpLookAheadEnabled(false);
        tx.setDexpLookAheadMs(170.0);
        tx.setDexpLowCutHz(220.0);
        tx.setDexpHighCutHz(4400.0);
        tx.setDexpSideChannelFilterEnabled(false);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("DexpAll"), &tx));

        // Reset to defaults (mirrors TransmitModel m_dexp* initializers).
        tx.setDexpEnabled(false);
        tx.setDexpDetectorTauMs(20.0);
        tx.setDexpAttackTimeMs(2.0);
        tx.setDexpReleaseTimeMs(100.0);
        tx.setDexpExpansionRatioDb(10.0);
        tx.setDexpHysteresisRatioDb(2.0);
        tx.setDexpLookAheadEnabled(true);
        tx.setDexpLookAheadMs(60.0);
        tx.setDexpLowCutHz(500.0);
        tx.setDexpHighCutHz(1500.0);
        tx.setDexpSideChannelFilterEnabled(true);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("DexpAll"), &tx));
        QCOMPARE(tx.dexpEnabled(),                  true);
        QCOMPARE(tx.dexpDetectorTauMs(),            33.0);
        QCOMPARE(tx.dexpAttackTimeMs(),             11.0);
        QCOMPARE(tx.dexpReleaseTimeMs(),            333.0);
        QCOMPARE(tx.dexpExpansionRatioDb(),         22.0);
        QCOMPARE(tx.dexpHysteresisRatioDb(),        6.0);
        QCOMPARE(tx.dexpLookAheadEnabled(),         false);
        QCOMPARE(tx.dexpLookAheadMs(),              170.0);
        QCOMPARE(tx.dexpLowCutHz(),                 220.0);
        QCOMPARE(tx.dexpHighCutHz(),                4400.0);
        QCOMPARE(tx.dexpSideChannelFilterEnabled(), false);
    }

    // =========================================================================
    // §5  Boolean serialization sanity — the 3 new boolean keys.
    //     ("True" / "False" string convention per AppSettings policy.)
    // =========================================================================

    void boolean_serialization_allThreeBoolKeys()
    {
        // Set all 3 to true; round-trip back as "True".
        auto& s = AppSettings::instance();
        s.setValue(profileKey(kMacA, "P", "DEXP_Enabled"),                  QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "DEXP_LookAheadEnabled"),         QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "DEXP_SideChannelFilterEnabled"), QStringLiteral("True"));

        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_Enabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_LookAheadEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_SideChannelFilterEnabled")).toString(),
                 QStringLiteral("True"));

        // Now set all 3 to false; round-trip as "False".
        s.setValue(profileKey(kMacA, "P", "DEXP_Enabled"),                  QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "DEXP_LookAheadEnabled"),         QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "DEXP_SideChannelFilterEnabled"), QStringLiteral("False"));

        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_Enabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_LookAheadEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "DEXP_SideChannelFilterEnabled")).toString(),
                 QStringLiteral("False"));
    }
};

QTEST_APPLESS_MAIN(TstMicProfileManagerDexpRoundTrip)
#include "tst_mic_profile_manager_dexp_round_trip.moc"
