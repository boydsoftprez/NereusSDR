// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_pa_profile_manager.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for PaProfileManager (Phase 2 Agent 2B of issue #167).
//
// Covers:
//   1. First-launch on fresh MAC seeds 16 factory + Bypass + sets active
//   2. Reconnect preserves stored profiles (user edits not auto-overwritten)
//   3. setMacAddress switches scope (per-MAC isolation)
//   4. saveProfile overwrites + emits signals only on new membership
//   5. deleteProfile (manifest update + last-profile guard + signal)
//   6. setActiveProfile for non-existent name returns false
//   7. Active-profile-on-connect logic (3 sub-cases)
//   8. regenerateFactoryDefaults restores factory values, leaves customs
//   9. activeProfile() accessor returns pointer to active profile
//
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/PaGainProfile.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "models/Band.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
static const QString kMacB = QStringLiteral("ff:ee:dd:cc:bb:aa");

static QString profileBlobKey(const QString& mac, const QString& name)
{
    return QStringLiteral("hardware/%1/pa/profile/%2").arg(mac, name);
}

static QString activeKeyPath(const QString& mac)
{
    return QStringLiteral("hardware/%1/pa/profile/active").arg(mac);
}

static QString manifestKeyPath(const QString& mac)
{
    return QStringLiteral("hardware/%1/pa/profile/_names").arg(mac);
}

class TstPaProfileManager : public QObject {
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
    // Test 1: First-launch on fresh MAC seeds 16 factory + Bypass + sets active
    // =========================================================================
    //
    // From Thetis setup.cs:23295-23316 [v2.10.3.13] initPAProfiles — seeds
    // one "Default - <model>" entry per HPSDRModel.LAST iteration plus one
    // "Bypass" profile. NereusSDR seeds on first connect via load(connectedModel),
    // and the active profile is "Default - <connectedModel>" (NereusSDR-spin
    // enhancement: Thetis just defaulted to combo index 0).
    void firstLaunch_seedsAllFactoryProfiles()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        const QStringList names = mgr.profileNames();
        // 16 factory (HPSDRModel::FIRST is sentinel -1, so 16 enum entries
        // from HPSDR=0 through REDPITAYA=15) plus 1 Bypass = 17 total.
        QCOMPARE(names.size(), 17);

        // Active = "Default - ANAN8000D" because connectedModel was ANAN8000D.
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default - ANAN8000D"));

        // Spot-check the active profile's gain values match the factory row
        // for ANAN8000D 80m.
        const PaProfile* p = mgr.activeProfile();
        QVERIFY(p != nullptr);
        const float expected80m = defaultPaGainsForBand(HPSDRModel::ANAN8000D,
                                                        Band::Band80m);
        QCOMPARE(p->getGainForBand(Band::Band80m), expected80m);
    }

    // Verify every factory profile name is present and the Bypass is too.
    void firstLaunch_allModelDefaultsPresent()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN_G2);

        const QStringList names = mgr.profileNames();
        const QStringList expected = {
            QStringLiteral("Default - HPSDR"),
            QStringLiteral("Default - HERMES"),
            QStringLiteral("Default - ANAN10"),
            QStringLiteral("Default - ANAN10E"),
            QStringLiteral("Default - ANAN100"),
            QStringLiteral("Default - ANAN100B"),
            QStringLiteral("Default - ANAN100D"),
            QStringLiteral("Default - ANAN200D"),
            QStringLiteral("Default - ORIONMKII"),
            QStringLiteral("Default - ANAN7000D"),
            QStringLiteral("Default - ANAN8000D"),
            QStringLiteral("Default - ANAN_G2"),
            QStringLiteral("Default - ANAN_G2_1K"),
            QStringLiteral("Default - ANVELINAPRO3"),
            QStringLiteral("Default - HERMESLITE"),
            QStringLiteral("Default - REDPITAYA"),
            QStringLiteral("Bypass"),
        };
        for (const QString& name : expected) {
            QVERIFY2(names.contains(name),
                     qPrintable(QStringLiteral("missing factory profile: ") + name));
        }
    }

    void firstLaunch_emitsProfileListChanged()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        QSignalSpy listSpy(&mgr, &PaProfileManager::profileListChanged);
        mgr.load(HPSDRModel::ANAN_G2);
        QCOMPARE(listSpy.count(), 1);
    }

    // =========================================================================
    // Test 2: Reconnect preserves stored profiles
    // =========================================================================
    //
    // From Thetis setup.cs:1920-1959 [v2.10.3.13] RecoverPAProfiles —
    // stored profiles WIN over factory defaults at deserialization time,
    // so future Thetis-table updates don't overwrite user edits. NereusSDR
    // mirrors this: subsequent load() calls don't re-seed.
    void reconnect_preservesStoredProfiles()
    {
        // First launch.
        {
            PaProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load(HPSDRModel::ANAN8000D);

            // Save a custom profile.
            PaProfile custom(QStringLiteral("MyProfile"),
                             HPSDRModel::ANAN8000D, false);
            custom.setGainForBand(Band::Band80m, 49.0f);
            mgr.saveProfile(QStringLiteral("MyProfile"), custom);
        }

        // Mutate the stored Default - ANAN8000D 80m gain to 49.0
        // by re-saving over it (simulating user edit through UI).
        {
            PaProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load(HPSDRModel::ANAN8000D);

            // Edit the existing factory profile.
            PaProfile edited(QStringLiteral("Default - ANAN8000D"),
                             HPSDRModel::ANAN8000D, true);
            edited.setGainForBand(Band::Band80m, 49.0f);
            mgr.saveProfile(QStringLiteral("Default - ANAN8000D"), edited);
        }

        // Reconnect: edits persist, no re-seed.
        PaProfileManager mgr2;
        mgr2.setMacAddress(kMacA);
        mgr2.load(HPSDRModel::ANAN8000D);

        // Manifest has all 17 factory + 1 custom = 18.
        QCOMPARE(mgr2.profileNames().size(), 18);

        // The user edit to Default - ANAN8000D is preserved (not factory-reset).
        const PaProfile* p = mgr2.profileByName(QStringLiteral("Default - ANAN8000D"));
        QVERIFY(p != nullptr);
        QCOMPARE(p->getGainForBand(Band::Band80m), 49.0f);

        // Custom profile still there.
        QVERIFY(mgr2.profileNames().contains(QStringLiteral("MyProfile")));
    }

    // =========================================================================
    // Test 3: setMacAddress switches scope (per-MAC isolation)
    // =========================================================================
    void perMacIsolation_independentBanks()
    {
        // MAC A: load + save custom.
        {
            PaProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load(HPSDRModel::ANAN8000D);

            PaProfile macAOnly(QStringLiteral("MacAOnly"),
                               HPSDRModel::ANAN8000D, false);
            macAOnly.setGainForBand(Band::Band20m, 33.3f);
            mgr.saveProfile(QStringLiteral("MacAOnly"), macAOnly);
        }

        // MAC B: load fresh -> only own factory profiles, no MacAOnly.
        PaProfileManager mgr;
        mgr.setMacAddress(kMacB);
        mgr.load(HPSDRModel::ANAN_G2);

        const QStringList names = mgr.profileNames();
        QVERIFY(!names.contains(QStringLiteral("MacAOnly")));
        // 17 (16 factory + Bypass) — same as a fresh first launch.
        QCOMPARE(names.size(), 17);
        // Active is per-MAC and reflects MAC B's connected model.
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default - ANAN_G2"));
    }

    // =========================================================================
    // Test 4: saveProfile overwrites + emits signals only on new membership
    // =========================================================================
    void saveProfile_overwritesAndEmitsCorrectly()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        QSignalSpy listSpy(&mgr, &PaProfileManager::profileListChanged);

        // Save a new profile -> emits profileListChanged.
        PaProfile p1(QStringLiteral("New1"), HPSDRModel::ANAN8000D, false);
        p1.setGainForBand(Band::Band40m, 42.0f);
        QVERIFY(mgr.saveProfile(QStringLiteral("New1"), p1));
        QCOMPARE(listSpy.count(), 1);

        // Save again with same name (overwrite) -> NO new emit.
        PaProfile p1_v2(QStringLiteral("New1"), HPSDRModel::ANAN8000D, false);
        p1_v2.setGainForBand(Band::Band40m, 99.0f);
        QVERIFY(mgr.saveProfile(QStringLiteral("New1"), p1_v2));
        QCOMPARE(listSpy.count(), 1);  // unchanged

        // Round-trip: read back the saved profile blob and verify content.
        const PaProfile* read = mgr.profileByName(QStringLiteral("New1"));
        QVERIFY(read != nullptr);
        QCOMPARE(read->getGainForBand(Band::Band40m), 99.0f);
    }

    // =========================================================================
    // Test 5: deleteProfile + last-profile guard
    // =========================================================================
    //
    // From Thetis setup.cs:9617-9624 [v2.10.3.13] btnTXProfileDelete_Click
    // (analogous guard for TX profiles): refuse to delete the LAST remaining
    // profile. PaProfileManager mirrors this guard.
    void deleteProfile_emitsAndUpdatesManifest()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        // Save a custom profile to delete.
        PaProfile p1(QStringLiteral("TempProfile"),
                     HPSDRModel::ANAN8000D, false);
        mgr.saveProfile(QStringLiteral("TempProfile"), p1);

        QSignalSpy listSpy(&mgr, &PaProfileManager::profileListChanged);
        QVERIFY(mgr.deleteProfile(QStringLiteral("TempProfile")));
        QCOMPARE(listSpy.count(), 1);

        // Manifest no longer has it.
        QVERIFY(!mgr.profileNames().contains(QStringLiteral("TempProfile")));
        QVERIFY(mgr.profileByName(QStringLiteral("TempProfile")) == nullptr);
    }

    void deleteProfile_lastProfileGuard()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        // Manually delete down to the last profile, then verify guard fires.
        QStringList names = mgr.profileNames();
        // 17 entries; delete 16 of them.
        int target = names.size() - 1;
        for (int i = 0; i < target; ++i) {
            const QString name = names.at(i);
            mgr.deleteProfile(name);
        }
        // One profile left.
        QCOMPARE(mgr.profileNames().size(), 1);
        const QString lastName = mgr.profileNames().first();

        // Guard: cannot delete the last one.
        QVERIFY(!mgr.deleteProfile(lastName));
        QCOMPARE(mgr.profileNames().size(), 1);
    }

    void deleteProfile_nonexistentReturnsFalse()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        QVERIFY(!mgr.deleteProfile(QStringLiteral("DoesNotExist")));
    }

    // =========================================================================
    // Test 6: setActiveProfile for non-existent name returns false
    // =========================================================================
    void setActiveProfile_nonexistentReturnsFalse()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        QSignalSpy activeSpy(&mgr, &PaProfileManager::activeProfileChanged);
        QVERIFY(!mgr.setActiveProfile(QStringLiteral("DoesNotExist")));
        QCOMPARE(activeSpy.count(), 0);

        // Active unchanged.
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default - ANAN8000D"));
    }

    void setActiveProfile_validNameEmitsAndUpdates()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        QSignalSpy activeSpy(&mgr, &PaProfileManager::activeProfileChanged);
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default - ANAN_G2")));
        QCOMPARE(activeSpy.count(), 1);
        QCOMPARE(activeSpy.takeFirst().at(0).toString(),
                 QStringLiteral("Default - ANAN_G2"));
        QCOMPARE(mgr.activeProfileName(),
                 QStringLiteral("Default - ANAN_G2"));

        // activeProfile() now returns the new one.
        const PaProfile* p = mgr.activeProfile();
        QVERIFY(p != nullptr);
        QCOMPARE(p->name(), QStringLiteral("Default - ANAN_G2"));
    }

    // =========================================================================
    // Test 7: Active-profile-on-connect logic (3 sub-cases)
    // =========================================================================
    //
    // NereusSDR-spin enhancement over Thetis:
    //   a) Stored active exists and is in manifest -> restore it
    //   b) Stored active is a deleted name -> fall back to Default-<connectedModel>
    //   c) No stored active -> fall back to Default-<connectedModel>

    // 7a: stored active matches manifest -> restored on reconnect.
    void activeOnConnect_storedActiveExists()
    {
        // First launch with ANAN8000D, then switch to G2 profile, then reconnect.
        {
            PaProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load(HPSDRModel::ANAN8000D);
            mgr.setActiveProfile(QStringLiteral("Default - ANAN_G2"));
        }

        // Reconnect with a different connectedModel than the stored active —
        // stored active wins.
        PaProfileManager mgr2;
        mgr2.setMacAddress(kMacA);
        mgr2.load(HPSDRModel::ANAN7000D);  // different model
        QCOMPARE(mgr2.activeProfileName(),
                 QStringLiteral("Default - ANAN_G2"));
    }

    // 7b: stored active is a deleted name -> fall back to Default-<connectedModel>.
    void activeOnConnect_storedActiveMissingFallsBack()
    {
        // First launch and create a custom profile.
        {
            PaProfileManager mgr;
            mgr.setMacAddress(kMacA);
            mgr.load(HPSDRModel::ANAN8000D);

            PaProfile custom(QStringLiteral("CustomProfile"),
                             HPSDRModel::ANAN8000D, false);
            mgr.saveProfile(QStringLiteral("CustomProfile"), custom);
            mgr.setActiveProfile(QStringLiteral("CustomProfile"));
            mgr.deleteProfile(QStringLiteral("CustomProfile"));
            // Note: deleteProfile of the active one resets it to a fallback;
            // mimic Thetis here by manually pinning the stored active key
            // back to the now-deleted name to test the load-time fallback.
            AppSettings::instance().setValue(activeKeyPath(kMacA),
                                              QStringLiteral("CustomProfile"));
        }

        PaProfileManager mgr2;
        mgr2.setMacAddress(kMacA);
        mgr2.load(HPSDRModel::ANAN8000D);
        QCOMPARE(mgr2.activeProfileName(),
                 QStringLiteral("Default - ANAN8000D"));
    }

    // 7c: no stored active -> Default-<connectedModel>.
    void activeOnConnect_noStoredActive()
    {
        // First launch — load() seeds and writes Default-<model>.
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN_G2_1K);
        QCOMPARE(mgr.activeProfileName(),
                 QStringLiteral("Default - ANAN_G2_1K"));
    }

    // =========================================================================
    // Test 8: regenerateFactoryDefaults restores factory values
    // =========================================================================
    void regenerateFactoryDefaults_restoresOnlyFactoryProfiles()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);

        // User edits a factory profile + adds a custom profile.
        {
            PaProfile edited(QStringLiteral("Default - ANAN8000D"),
                             HPSDRModel::ANAN8000D, true);
            edited.setGainForBand(Band::Band80m, 88.0f);  // off-spec
            mgr.saveProfile(QStringLiteral("Default - ANAN8000D"), edited);
        }
        {
            PaProfile custom(QStringLiteral("UserCustom"),
                             HPSDRModel::ANAN8000D, false);
            custom.setGainForBand(Band::Band80m, 12.0f);
            mgr.saveProfile(QStringLiteral("UserCustom"), custom);
        }

        // Capture active before regen.
        const QString activeBefore = mgr.activeProfileName();

        // Regenerate.
        mgr.regenerateFactoryDefaults(HPSDRModel::ANAN8000D);

        // Factory profile restored to canonical value.
        const PaProfile* p = mgr.profileByName(QStringLiteral("Default - ANAN8000D"));
        QVERIFY(p != nullptr);
        const float expected80m = defaultPaGainsForBand(HPSDRModel::ANAN8000D,
                                                        Band::Band80m);
        QCOMPARE(p->getGainForBand(Band::Band80m), expected80m);

        // Custom profile preserved untouched.
        const PaProfile* custom = mgr.profileByName(QStringLiteral("UserCustom"));
        QVERIFY(custom != nullptr);
        QCOMPARE(custom->getGainForBand(Band::Band80m), 12.0f);

        // Active profile not changed.
        QCOMPARE(mgr.activeProfileName(), activeBefore);
    }

    // =========================================================================
    // Test 9: activeProfile() accessor
    // =========================================================================
    void activeProfile_returnsCurrentProfile()
    {
        PaProfileManager mgr;
        // Before load, no active.
        QCOMPARE(mgr.activeProfile(), nullptr);

        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN_G2);

        const PaProfile* p = mgr.activeProfile();
        QVERIFY(p != nullptr);
        QCOMPARE(p->name(), QStringLiteral("Default - ANAN_G2"));
        QCOMPARE(p->model(), HPSDRModel::ANAN_G2);

        // After switching active, accessor reflects the change.
        mgr.setActiveProfile(QStringLiteral("Bypass"));
        const PaProfile* bypass = mgr.activeProfile();
        QVERIFY(bypass != nullptr);
        QCOMPARE(bypass->name(), QStringLiteral("Bypass"));
        // Bypass row is all-100.0 sentinel.
        QCOMPARE(bypass->getGainForBand(Band::Band20m), 100.0f);
    }

    // Sanity: profileByName(unknown) returns nullptr.
    void profileByName_unknownReturnsNullptr()
    {
        PaProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load(HPSDRModel::ANAN8000D);
        QCOMPARE(mgr.profileByName(QStringLiteral("DoesNotExist")), nullptr);
    }
};

QTEST_MAIN(TstPaProfileManager)
#include "tst_pa_profile_manager.moc"
