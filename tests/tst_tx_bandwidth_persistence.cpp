// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_tx_bandwidth_persistence.cpp  (NereusSDR)
// =================================================================
//
// TDD for Plan 4 Task 2 (D1) — FilterLow/FilterHigh schema additions.
//
// Verifies:
//   1. filterValuesPersistAcrossProfileSave — values survive a profile
//      save/switch/activate cycle.
//   2. activatingProfilePushesFilterToTransmitModel — setActiveProfile
//      emits filterChanged and updates filterLow/filterHigh on the model.
//   3. modifiedFlagSetsAndClearsOnSave — isActiveProfileModified detects
//      drift and clears after saveActiveProfile.
//
// All tests scope their AppSettings writes to a unique test MAC so they
// don't collide with factory profiles or other test suites.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Plan 4 Cluster A).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// Unique MAC to avoid colliding with factory profile tests.
static const QString kTestMac = QStringLiteral("aa:bb:cc:dd:ee:ff");

class TstTxBandwidthPersistence : public QObject {
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
    // Test 1: filter values persist across profile save/switch/activate
    // =========================================================================
    void filterValuesPersistAcrossProfileSave()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kTestMac);
        mgr.load();

        TransmitModel tx;
        tx.loadFromSettings(kTestMac);

        // Set up a second "other" profile so we can switch away from P1.
        tx.setFilterLow(500);
        tx.setFilterHigh(2500);
        mgr.saveProfile(QStringLiteral("other"), &tx);

        // Set P1 values and save.
        tx.setFilterLow(1710);
        tx.setFilterHigh(2710);
        mgr.saveProfile(QStringLiteral("P1"), &tx);

        // Switch to other profile — this overwrites the live model.
        mgr.setActiveProfile(QStringLiteral("other"), &tx);
        QCOMPARE(tx.filterLow(),  500);
        QCOMPARE(tx.filterHigh(), 2500);

        // Re-activate P1 — should restore 1710/2710.
        mgr.setActiveProfile(QStringLiteral("P1"), &tx);
        QCOMPARE(tx.filterLow(),  1710);
        QCOMPARE(tx.filterHigh(), 2710);
    }

    // =========================================================================
    // Test 2: activating a profile pushes filter values into TransmitModel
    // =========================================================================
    void activatingProfilePushesFilterToTransmitModel()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kTestMac);
        mgr.load();

        TransmitModel tx;
        tx.loadFromSettings(kTestMac);

        // Save a "wide" profile with non-default filter values.
        tx.setFilterLow(50);
        tx.setFilterHigh(3650);
        mgr.saveProfile(QStringLiteral("P-wide"), &tx);

        // Reset the model to defaults so activating "P-wide" has something
        // to change.
        tx.setFilterLow(100);
        tx.setFilterHigh(2900);

        QSignalSpy spy(&tx, &TransmitModel::filterChanged);

        mgr.setActiveProfile(QStringLiteral("P-wide"), &tx);

        // At least one filterChanged emission expected.
        QVERIFY(spy.count() >= 1);
        QCOMPARE(tx.filterLow(),  50);
        QCOMPARE(tx.filterHigh(), 3650);
    }

    // =========================================================================
    // Test 3: isActiveProfileModified detects drift; clears after saveActive
    // =========================================================================
    void modifiedFlagSetsAndClearsOnSave()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kTestMac);
        mgr.load();

        TransmitModel tx;
        tx.loadFromSettings(kTestMac);

        // Save profile P1 with known filter values.
        tx.setFilterLow(1710);
        tx.setFilterHigh(2710);
        mgr.saveProfile(QStringLiteral("P1"), &tx);

        // Activate P1 — model now matches saved values.
        mgr.setActiveProfile(QStringLiteral("P1"), &tx);

        // Should not be modified: live matches saved.
        QVERIFY(!mgr.isActiveProfileModified(&tx));

        // Drift: change filterLow without saving.
        tx.setFilterLow(1500);

        // Now the live model differs from the saved profile.
        QVERIFY(mgr.isActiveProfileModified(&tx));

        // Save the active profile — modified flag should clear.
        mgr.saveActiveProfile(&tx);

        // After save, live matches saved again.
        QVERIFY(!mgr.isActiveProfileModified(&tx));
    }
};

QTEST_MAIN(TstTxBandwidthPersistence)
#include "tst_tx_bandwidth_persistence.moc"
