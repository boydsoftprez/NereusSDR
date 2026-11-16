// tests/tst_connection_panel_saved_radios.cpp
//
// Round-trip tests for AppSettings saved-radio persistence (Phase 3I Task 15)
// + stale-sweep exemption for saved MACs (Phase 3Q Task 11).
//
// Uses AppSettings(filePath) direct constructor so each test gets an isolated
// in-memory store — no pollution of the user's real settings file.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestConnectionPanelSavedRadios : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Helper: build a settings instance backed by a temp file.
    // Each call returns a fresh in-memory store (no load()).
    AppSettings makeSettings() const {
        return AppSettings(m_tempDir.path() + QStringLiteral("/NereusSDR.settings"));
    }

    RadioInfo makeHl2Info() const {
        RadioInfo info;
        info.name            = QStringLiteral("Bench HL2");
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.address         = QHostAddress(QStringLiteral("192.168.1.42"));
        info.port            = 1024;
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.firmwareVersion = 72;
        return info;
    }

    RadioInfo makeSaturnInfo() const {
        RadioInfo info;
        info.name            = QStringLiteral("Shack G2");
        info.macAddress      = QStringLiteral("aa:bb:cc:44:55:66");
        info.address         = QHostAddress(QStringLiteral("192.168.1.20"));
        info.port            = 1024;
        info.boardType       = HPSDRHW::Saturn;
        info.protocol        = ProtocolVersion::Protocol2;
        info.firmwareVersion = 21;
        return info;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_tempDir.isValid());
    }

    void saveAndListSingleRadio() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();

        RadioInfo info = makeHl2Info();
        settings.saveRadio(info, /*pinToMac=*/true, /*autoConnect=*/false);

        QList<SavedRadio> list = settings.savedRadios();
        QCOMPARE(list.size(), 1);
        const SavedRadio& saved = list.first();
        QCOMPARE(saved.info.name,       info.name);
        QCOMPARE(saved.info.macAddress, info.macAddress);
        QCOMPARE(saved.info.boardType,  HPSDRHW::HermesLite);
        QCOMPARE(saved.info.protocol,   ProtocolVersion::Protocol1);
        QCOMPARE(saved.pinToMac,        true);
        QCOMPARE(saved.autoConnect,     false);
    }

    void saveMultipleAndRoundTrip() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        settings.saveRadio(makeHl2Info(),    true,  false);
        settings.saveRadio(makeSaturnInfo(), true,  true);

        QList<SavedRadio> list = settings.savedRadios();
        QCOMPARE(list.size(), 2);

        // Look up by MAC
        std::optional<SavedRadio> hl2 = settings.savedRadio(QStringLiteral("aa:bb:cc:11:22:33"));
        QVERIFY(hl2.has_value());
        QCOMPARE(hl2->info.name, QStringLiteral("Bench HL2"));

        std::optional<SavedRadio> g2 = settings.savedRadio(QStringLiteral("aa:bb:cc:44:55:66"));
        QVERIFY(g2.has_value());
        QCOMPARE(g2->info.name,     QStringLiteral("Shack G2"));
        QCOMPARE(g2->autoConnect,   true);
    }

    void forgetRadio() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        settings.saveRadio(makeHl2Info(),    true, false);
        settings.saveRadio(makeSaturnInfo(), true, true);
        QCOMPARE(settings.savedRadios().size(), 2);

        settings.forgetRadio(QStringLiteral("aa:bb:cc:11:22:33"));
        QList<SavedRadio> list = settings.savedRadios();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().info.macAddress, QStringLiteral("aa:bb:cc:44:55:66"));
    }

    void lastConnectedTracking() {
        AppSettings settings = makeSettings();
        settings.setLastConnected(QStringLiteral("aa:bb:cc:11:22:33"));
        QCOMPARE(settings.lastConnected(), QStringLiteral("aa:bb:cc:11:22:33"));

        settings.setLastConnected(QString());
        QVERIFY(settings.lastConnected().isEmpty());
    }

    void discoveryProfileRoundTrip() {
        AppSettings settings = makeSettings();
        settings.setDiscoveryProfile(DiscoveryProfile::Fast);
        QCOMPARE(settings.discoveryProfile(), DiscoveryProfile::Fast);

        settings.setDiscoveryProfile(DiscoveryProfile::SafeDefault);
        QCOMPARE(settings.discoveryProfile(), DiscoveryProfile::SafeDefault);
    }

    void updateExistingRadioPreservesFields() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        RadioInfo info = makeHl2Info();
        settings.saveRadio(info, true, false);

        // Simulate a reconnect that bumps firmware version
        info.firmwareVersion = 73;
        settings.saveRadio(info, true, true);  // now autoConnect=true

        QList<SavedRadio> list = settings.savedRadios();
        QCOMPARE(list.size(), 1);  // still one entry, not duplicated
        QCOMPARE(list.first().info.firmwareVersion, 73);
        QCOMPARE(list.first().autoConnect,          true);
    }

    // The second firmware number is saved with the first, so a saved row
    // reads the same after a restart as the row discovery filled in.
    void firmwareBetaVersionRoundTrips() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        RadioInfo info = makeSaturnInfo();
        info.boardType           = HPSDRHW::OrionMKII;
        info.firmwareVersion     = 22;
        info.firmwareBetaVersion = 14;
        settings.saveRadio(info, true, false);

        std::optional<SavedRadio> saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->info.firmwareVersion,     22);
        QCOMPARE(saved->info.firmwareBetaVersion, 14);
        QCOMPARE(saved->info.firmwareText(),      QStringLiteral("22.14"));

        // A later connect with a release that reports none replaces it.
        info.firmwareBetaVersion = 0;
        settings.saveRadio(info, true, false);
        saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->info.firmwareBetaVersion, 0);
        QCOMPARE(saved->info.firmwareText(),      QStringLiteral("22"));
    }

    // The radio's receiver count survives saving, so a connect from the saved
    // entry sizes the stream pool as a connect from discovery does. An edit,
    // which has no reply to read, leaves it alone.
    void reportedReceiversRoundTrips() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        RadioInfo info = makeSaturnInfo();
        info.boardType = HPSDRHW::OrionMKII;
        info.reportedReceivers = 5;
        settings.saveRadio(info, true, false);

        std::optional<SavedRadio> saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->info.reportedReceivers, 5);
        QCOMPARE(saved->info.maxReceivers, 5);

        info.reportedReceivers = 0;
        settings.saveRadio(info, true, false);
        saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->info.reportedReceivers, 5);

        // A new reply replaces it, as a firmware change would.
        info.reportedReceivers = 8;
        settings.saveRadio(info, true, false);
        QCOMPARE(settings.savedRadio(info.macAddress)->info.reportedReceivers, 8);
    }

    // A radio saved before the count was kept has no such key and reads as
    // reporting none, and seeing it on the network fills the key in.
    void discoverySightingFillsAnOlderSavedEntry() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        const RadioInfo info = makeSaturnInfo();
        settings.saveRadio(info, true, false);
        QCOMPARE(settings.savedRadio(info.macAddress)->info.reportedReceivers, 0);

        QVERIFY(settings.refreshRadioReportedReceivers(info.macAddress, 5));
        QCOMPARE(settings.savedRadio(info.macAddress)->info.reportedReceivers, 5);

        // Unchanged, no report, or a radio nobody saved: nothing to write.
        QVERIFY(!settings.refreshRadioReportedReceivers(info.macAddress, 5));
        QVERIFY(!settings.refreshRadioReportedReceivers(info.macAddress, 0));
        QVERIFY(!settings.refreshRadioReportedReceivers(QStringLiteral("00:00:00:00:00:01"), 5));
        QVERIFY(!settings.savedRadio(QStringLiteral("00:00:00:00:00:01")).has_value());
    }

    // A radio saved before the number was kept has no such key and reads
    // as reporting none.
    void savedRadioWithoutABetaVersionKeyReadsNone() {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        const RadioInfo info = makeHl2Info();
        settings.saveRadio(info, true, false);
        settings.remove(QStringLiteral("radios/%1/firmwareBetaVersion").arg(info.macAddress));

        const std::optional<SavedRadio> saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->info.firmwareVersion,     72);
        QCOMPARE(saved->info.firmwareBetaVersion, 0);
    }

    // -------------------------------------------------------------------------
    // Phase 3Q Task 11 — design §7.4
    // Saved radios must survive the stale sweep even when their lastSeen
    // timestamp is well past the discovered-only timeout (60 s).
    // Discovered-only radios with the same expired timestamp MUST be removed.
    // -------------------------------------------------------------------------
    void savedRadioSurvivesStaleSweep() {
        // Arrange: one saved MAC and one discovered-only MAC, both with a
        // lastSeen timestamp 90 s in the past (past the 60 s timeout).
        const QString savedMac     = QStringLiteral("aa:bb:cc:11:22:33");
        const QString discoveredMac = QStringLiteral("dd:ee:ff:77:88:99");

        RadioDiscovery disc;
        disc.setSavedMacs(QStringList{savedMac});

        const qint64 staleTs = QDateTime::currentMSecsSinceEpoch() - 90 * 1000;

        RadioInfo savedInfo = makeHl2Info();
        savedInfo.macAddress = savedMac;
        disc.injectLastSeenForTest(savedMac, savedInfo, staleTs);

        RadioInfo discoveredInfo;
        discoveredInfo.name       = QStringLiteral("Ghost Radio");
        discoveredInfo.macAddress = discoveredMac;
        disc.injectLastSeenForTest(discoveredMac, discoveredInfo, staleTs);

        // Monitor radioLost emissions
        QSignalSpy lostSpy(&disc, &RadioDiscovery::radioLost);

        // Act: run the stale sweep directly
        disc.forceStaleCheckForTest();

        // Assert: exactly one radioLost emitted — the discovered-only radio
        QCOMPARE(lostSpy.count(), 1);
        QCOMPARE(lostSpy.first().first().toString(), discoveredMac);

        // Assert: savedMac is still accessible via discoveredRadios() (i.e. not removed)
        const QList<RadioInfo> remaining = disc.discoveredRadios();
        bool savedStillPresent = false;
        for (const RadioInfo& ri : remaining) {
            if (ri.macAddress == savedMac) {
                savedStillPresent = true;
                break;
            }
        }
        QVERIFY2(savedStillPresent,
                 "Saved radio must survive the stale sweep regardless of lastSeen age");
    }

    // Persistence audit: a radio's first connect turns Auto-connect on.
    void firstConnectTurnsAutoConnectOn()
    {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        const RadioInfo info = makeHl2Info();
        settings.saveRadioOnConnect(info);
        const auto saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->autoConnect, true);
        QCOMPARE(saved->pinToMac, false);
    }

    // After that a connect keeps the saved choices, whichever they are.
    void laterConnectKeepsTheSavedChoices()
    {
        AppSettings settings = makeSettings();
        settings.clearSavedRadios();
        const RadioInfo info = makeHl2Info();
        settings.saveRadio(info, /*pinToMac=*/true, /*autoConnect=*/false);
        settings.saveRadioOnConnect(info);
        auto saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->autoConnect, false);
        QCOMPARE(saved->pinToMac, true);

        settings.saveRadio(info, /*pinToMac=*/false, /*autoConnect=*/true);
        settings.saveRadioOnConnect(info);
        saved = settings.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->autoConnect, true);
        QCOMPARE(saved->pinToMac, false);
    }
};

QTEST_MAIN(TestConnectionPanelSavedRadios)
#include "tst_connection_panel_saved_radios.moc"
