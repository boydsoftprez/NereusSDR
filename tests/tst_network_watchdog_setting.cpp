// no-port-check: NereusSDR-original test.
//
// tests/tst_network_watchdog_setting.cpp
//
// R-R3-49 / R-R3-21 / R-R3-11: the Network Watchdog checkbox (Setup >
// General > Options) is a radio setting, applied where the radio is.
//
// Covered: a local window's checkbox reaches its own radio connection and is
// saved; the connect path hands the saved value to the connection before it
// starts (a real RadioModel connecting to the P1 loopback fake); a remote
// window's checkbox reaches the Core over the in-process loopback, is saved
// in the Core's settings and applied to the Core's radio; the checkbox is
// disabled with the Core's reason while the Core's settings are unavailable;
// and an older Core's refusal puts the box back and says, in plain words,
// that the Core needs updating. No hardware; no audio device is opened by
// these objects.
//
// Modification history (NereusSDR):
//   2026-09-24: created (R-R3-49, R-R3-21, R-R3-11), by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"

#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackTransport.h"
#include "OperatorWording.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kKey = QStringLiteral("NetworkWatchdogEnabled");

// The radio end: records every watchdog value it was given.
class WatchdogRecordingConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit WatchdogRecordingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    QList<bool> watchdog;

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool on) override
    {
        m_watchdogEnabled = on;
        watchdog.append(on);
    }
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

class ScopedRemoteBackend final {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend() { AppSettings::instance().setRemoteBackend(nullptr); }
};

QCheckBox* watchdogBox(GeneralOptionsPage& page)
{
    return page.findChild<QCheckBox*>(QStringLiteral("chkNetworkWDT"));
}

void resetLocalSetting()
{
    AppSettings::instance().remove(kKey);
}

} // namespace

class TestNetworkWatchdogSetting final : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }
    void init() { resetLocalSetting(); }
    void cleanup() { resetLocalSetting(); }

    // ---- A local radio ----------------------------------------------------

    void localCheckboxReachesTheRadioAndIsSaved()
    {
        RadioModel model;
        WatchdogRecordingConnection radio;
        model.injectConnectionForTest(&radio);
        // Detach before either goes, even when a check fails early.
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        {
            GeneralOptionsPage page(&model);
            QCheckBox* box = watchdogBox(page);
            QVERIFY(box != nullptr);
            QVERIFY(box->isChecked());

            box->setChecked(false);
            QTRY_VERIFY(!radio.watchdog.isEmpty());
            QCOMPARE(radio.watchdog.last(), false);
            QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("False"));

            box->setChecked(true);
            QTRY_COMPARE(radio.watchdog.last(), true);
            QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));
        }
    }

    void connectHandsTheSavedValueToTheConnection()
    {
        AppSettings::instance().setValue(kKey, QStringLiteral("False"));
        auto harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioConnection* conn = harness->model().connection();
        QVERIFY(conn != nullptr);
        // Queued ahead of connectToRadio on the connection's own thread, so
        // it is in place once the link is up.
        QVERIFY(!conn->isWatchdogEnabled());
    }

    // ---- A remote window --------------------------------------------------

    void remoteCheckboxReachesTheCoreAndItsRadio()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel core;
        WatchdogRecordingConnection coreRadio;
        core.injectConnectionForTest(&coreRadio);
        const auto detach = qScopeGuard([&core] { core.injectConnectionForTest(nullptr); });
        {
            StationServer server(&core, coreSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
            RadioModel window(RadioModel::Role::Remote);
            SettingsProxy proxy;
            StationClient client(&window, &proxy);
            auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
            auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
            stationEnd->linkTo(clientEnd);
            QSignalSpy completed(&client, &StationClient::handshakeComplete);
            client.startSession(clientEnd, server.token());
            server.acceptTransport(stationEnd);
            QVERIFY(completed.wait(5000) || !completed.isEmpty());

            ScopedRemoteBackend backend(&proxy);
            GeneralOptionsPage page(&window);
            QCheckBox* box = watchdogBox(page);
            QVERIFY(box != nullptr);
            QVERIFY(box->isChecked());

            box->setChecked(false);
            QTRY_COMPARE(coreSettings.value(kKey).toString(), QStringLiteral("False"));
            QTRY_VERIFY(!coreRadio.watchdog.isEmpty());
            QCOMPARE(coreRadio.watchdog.last(), false);

            box->setChecked(true);
            QTRY_COMPARE(coreSettings.value(kKey).toString(), QStringLiteral("True"));
            QTRY_COMPARE(coreRadio.watchdog.last(), true);
        }
    }

    void removingTheKeyOnTheCoreAppliesTheDefault()
    {
        // A settings reset on the Core removes the key. The settings then
        // read the default (on), so the Core's radio must take it too
        // rather than keep the last value it was given.
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel core;
        WatchdogRecordingConnection coreRadio;
        core.injectConnectionForTest(&coreRadio);
        const auto detach = qScopeGuard([&core] { core.injectConnectionForTest(nullptr); });
        {
            StationServer server(&core, coreSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

            coreSettings.setValue(kKey, QStringLiteral("False"));
            QTRY_VERIFY(!coreRadio.watchdog.isEmpty());
            QCOMPARE(coreRadio.watchdog.last(), false);

            coreSettings.remove(kKey);
            QTRY_COMPARE(coreRadio.watchdog.last(), true);
        }
    }

    void remoteCheckboxIsDisabledWithoutTheCoresSettings()
    {
        RadioModel window(RadioModel::Role::Remote);
        GeneralOptionsPage page(&window);
        QCheckBox* box = watchdogBox(page);
        QVERIFY(box != nullptr);
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        page.setStationSettingsAvailable(false, reason);
        QVERIFY(!box->isEnabled());
        page.setStationSettingsAvailable(true, QString());
        QVERIFY(box->isEnabled());
    }

    void olderCoreRefusalPutsTheBoxBackAndSaysSo()
    {
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        ScopedRemoteBackend backend(&proxy);
        GeneralOptionsPage page(&window);
        QCheckBox* box = watchdogBox(page);
        auto* note = page.findChild<QLabel*>(QStringLiteral("lblNetworkWDTCore"));
        QVERIFY(box != nullptr && note != nullptr);
        QVERIFY(note->isHidden());

        box->setChecked(false);
        // An older Core keeps the key to itself: it refuses the write and
        // has no value for it (StationClient hands this to the proxy).
        proxy.applyRejection(kKey, QVariant());
        QVERIFY(box->isChecked());
        QVERIFY(!note->isHidden());
        QVERIFY(OperatorWording::isPlain(note->text()));
        QVERIFY(note->text().contains(QStringLiteral("Core needs updating")));

        // Another key's refusal leaves it alone.
        note->setVisible(false);
        proxy.applyRejection(QStringLiteral("Region"), QVariant());
        QVERIFY(note->isHidden());
    }
};

QTEST_MAIN(TestNetworkWatchdogSetting)
#include "tst_network_watchdog_setting.moc"
