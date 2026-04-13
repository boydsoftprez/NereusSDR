// tests/tst_connection_panel_saved_radios.cpp
//
// Round-trip tests for AppSettings saved-radio persistence (Phase 3I Task 15).
//
// Uses AppSettings(filePath) direct constructor so each test gets an isolated
// in-memory store — no pollution of the user's real settings file.

#include <QtTest/QtTest>
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
};

QTEST_MAIN(TestConnectionPanelSavedRadios)
#include "tst_connection_panel_saved_radios.moc"
