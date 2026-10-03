// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_remote_window_on_air_settings.cpp  (NereusSDR)
// =================================================================
//
// R-IOS-13 / R-R3-49 (remote parity, both ways): a real remote MainWindow,
// signed in with its own paired key to a Core that allows remote transmit,
// holds transmit and keys the Core with its TX applet's TUNE. While keyed,
// its transmit settings stay live as a local window's do (LEV, EQ, CFC,
// MON, the TX EQ dialog) and a change reaches the Core, on a Core that
// takes them on the air (transmitSettingsVersion 13). Against an older
// Core (the capability capped at 10 on the wire) the same controls are
// shown disabled with the on-air reason instead, so nothing is sent that
// the Core would refuse. The Core's radio is a static test model; nothing
// reaches a transmitter.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Created (remote parity on the air).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QAction>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/MainWindow.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxEqDialog.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class BackendScope final {
public:
    explicit BackendScope(SettingsProxy* proxy) { AppSettings::instance().setRemoteBackend(proxy); }
    ~BackendScope() { AppSettings::instance().setRemoteBackend(nullptr); }
};

// The Core's end of the link. With a cap, every capabilities message says
// transmitSettingsVersion is at most the cap: a Core from before the
// transmit settings were taken on the air.
class CappingTransport final : public Test::LoopbackTransport {
public:
    CappingTransport(const QString& description, int cap)
        : Test::LoopbackTransport(description), m_cap(cap) {}

    void sendText(const QByteArray& wire) override
    {
        SessionMessage message;
        if (m_cap > 0 && wire.contains("\"capabilities\"") && SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::Capabilities) {
            StationCapabilities caps = StationCapabilities::fromUpdates(message.updates);
            caps.transmitSettingsVersion = std::min(caps.transmitSettingsVersion, m_cap);
            Test::LoopbackTransport::sendText(
                SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
            return;
        }
        Test::LoopbackTransport::sendText(wire);
    }

private:
    int m_cap = 0;
};

QPushButton* buttonNamed(TxApplet* applet, const QString& accessibleName)
{
    for (QPushButton* b : applet->findChildren<QPushButton*>()) {
        if (b->accessibleName() == accessibleName) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TestRemoteWindowOnAirSettings : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("remote-window-on-air-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }
    void init()
    {
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void aKeyedWindowThatHoldsTransmitKeepsItsSettings_data()
    {
        QTest::addColumn<int>("cap");
        QTest::newRow("a Core that takes them on the air") << 0;
        QTest::newRow("an older Core") << 10;
    }
    void aKeyedWindowThatHoldsTransmitKeepsItsSettings()
    {
        QFETCH(int, cap);
        QTemporaryDir directory;
        AppSettings stationSettings(directory.filePath(QStringLiteral("station.settings")));
        stationSettings.setValue(QLatin1String(AppSettings::kDaemonProfileSeededKey),
                                 QStringLiteral("True"));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int slice = station.addSlice(QStringLiteral("pan-0"));
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:41");
        info.boardType = HPSDRHW::Saturn;
        station.setLastRadioInfoForTest(info);
        station.scopeTxProfiles(info.macAddress);
        StationServer server(&station, stationSettings,
                             NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        // The Core allows remote transmit and keys at once.
        server.setRemoteTransmitAllowed(true);
        station.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        station.transmitModel().setMicSourceLocked(false);
        station.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* s = station.sliceById(slice)) {
            s->setDspMode(DSPMode::USB);
            s->setFrequency(14200000.0);
        }

        SettingsProxy proxy;
        BackendScope backend(&proxy);
        MainWindow window({QStringLiteral("ws://127.0.0.1:1"), {}, {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        auto* client = window.findChild<StationClient*>();
        QVERIFY(client);
        // The window signs in with its own key, which the Core has paired.
        const auto key = ClientDeviceIdentity::forThisProfile();
        PairedDevice device;
        device.id = key->fingerprint();
        device.publicKeySpki = key->publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(device));
        auto* stationLink = new CappingTransport(QStringLiteral("station"), cap);
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        QString pin = server.certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        clientLink->setPeerCertificateSha256(QByteArray::fromHex(pin.toLatin1()));
        client->startSession(clientLink, QString(), QString(),
                             server.stationIdentity().fingerprint());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client->isHandshakeComplete());
        QTRY_VERIFY(client->capabilities().txPermitted);
        QCOMPARE(client->capabilities().transmitSettingsVersion >= 13, cap == 0);

        TxApplet* applet = window.findChild<TxApplet*>();
        QVERIFY(applet);
        QPushButton* tune = buttonNamed(applet, QStringLiteral("Tune carrier"));
        auto* lev = applet->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* eq = applet->findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        auto* cfc = applet->findChild<QPushButton*>(QStringLiteral("TxCfcButton"));
        QPushButton* mon = buttonNamed(applet, QStringLiteral("Monitor enable"));
        QVERIFY(tune && lev && eq && cfc && mon);
        const QList<QPushButton*> settings{lev, eq, cfc, mon};
        for (QPushButton* control : settings) {
            QTRY_VERIFY2(control->isEnabled(), qPrintable(control->accessibleName()));
        }
        QTRY_VERIFY(tune->isEnabled());

        // This window's TUNE: it holds transmit and the Core is keyed.
        tune->click();
        QTRY_VERIFY(station.isTune());
        QTRY_VERIFY(window.radioModel()->isCoreOnAir());
        QVERIFY(server.transmitHolder()->isHeldBy(key->fingerprint()));

        TransmitModel& coreTx = station.transmitModel();
        if (cap == 0) {
            // As in a local window: live on the air, and a change lands.
            for (QPushButton* control : settings) {
                QVERIFY2(control->isEnabled(), qPrintable(control->accessibleName()));
            }
            QVERIFY(TxEqDialog::settingsPermitted());
            const bool leveler = coreTx.txLevelerOn();
            lev->click();
            QTRY_COMPARE(coreTx.txLevelerOn(), !leveler);
            const int preamp = coreTx.txEqPreamp() == 3 ? 4 : 3;
            window.radioModel()->transmitModel().setTxEqPreamp(preamp);
            QTRY_COMPARE(coreTx.txEqPreamp(), preamp);
            const int decay = coreTx.txAlcDecay() == 20 ? 21 : 20;
            window.radioModel()->transmitModel().setTxAlcDecay(decay);
            QTRY_COMPARE(coreTx.txAlcDecay(), decay);
            QVERIFY(station.isTune());
        } else {
            // An older Core refuses them on the air: shown disabled with
            // the reason, so nothing is sent to be refused.
            for (QPushButton* control : settings) {
                QTRY_VERIFY2(!control->isEnabled(), qPrintable(control->accessibleName()));
                QCOMPARE(control->toolTip(), RadioModel::onAirReason());
            }
            QVERIFY(!TxEqDialog::settingsPermitted());
        }

        tune->click();
        QTRY_VERIFY(!station.isTune());
        QTRY_VERIFY(!window.radioModel()->isCoreOnAir());
        for (QPushButton* control : settings) {
            QTRY_VERIFY2(control->isEnabled(), qPrintable(control->accessibleName()));
        }
    }
};

QTEST_MAIN(TestRemoteWindowOnAirSettings)
#include "tst_remote_window_on_air_settings.moc"
