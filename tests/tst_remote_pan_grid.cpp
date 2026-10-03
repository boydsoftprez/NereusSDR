// =================================================================
// tests/tst_remote_pan_grid.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// Parity ruling C12 (remote-window parity plan, 2026-09-24): Setup >
// Display > Grid & Scales' dB Max and dB Min per band, from a remote
// window. The Core's per-band values win:
//
//   1. DisplayGridMax_<band> / DisplayGridMin_<band> are the Core's
//      settings (Station scope), so a window reads and writes the Core's.
//   2. A window's write reaches the Core's pan at once: on that band the
//      Core's range moves, and a removal returns the band's default.
//   3. A remote window's pan never pushes its own values on a band
//      crossing; the Core's pan applies its own and the range reaches the
//      window through the pan's mirrored floor and ceiling.
//   4. A window's pan re-reads a band when the Core's value arrives, so
//      the Setup page shows the Core's numbers.
//   5. A local window's pan applies its per-band grid on a crossing, as
//      before.
//   6. Grid & Scales' spinboxes show the Core's value as it arrives.
//
// Loopback only: no radio, no audio device.
//
// Modification history (NereusSDR):
//   2026-09-28 : Created for parity ruling C12 by J.J. Boyd (KG4VCF).
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QFile>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/setup/DisplaySetupPages.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMax20 = QStringLiteral("DisplayGridMax_20m");
const QString kMin20 = QStringLiteral("DisplayGridMin_20m");
const QString kMax40 = QStringLiteral("DisplayGridMax_40m");
const QString kMin40 = QStringLiteral("DisplayGridMin_40m");

void clearGridKeys()
{
    for (const QString& key : {kMax20, kMin20, kMax40, kMin40}) {
        AppSettings::instance().remove(key);
    }
}

std::unique_ptr<RadioModel> makeCore()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:12");
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    return model;
}

struct Window {
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&model, &proxy};
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

struct Session {
    Session(const QString& securityDir, QObject* parent)
    {
        core = makeCore();
        server = std::make_unique<StationServer>(
            core.get(), AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        window.coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        window.windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        window.coreEnd->linkTo(window.windowEnd);
    }
    ~Session()
    {
        window.client.disconnectFromStation(QStringLiteral("test complete"));
        server.reset();
    }
    bool connect()
    {
        QSignalSpy completed(&window.client, &StationClient::handshakeComplete);
        window.client.startSession(window.windowEnd, server->token());
        server->acceptTransport(window.coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    void write(const QString& key, const QString& value)
    {
        window.windowEnd->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, value, QStringLiteral("window"))));
    }
    void remove(const QString& key)
    {
        window.windowEnd->sendText(SessionMessages::encode(SessionMessages::settingsRemove(key)));
    }

    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    Window window;
};

bool anyRefusal(LoopbackTransport* windowEnd)
{
    for (const QByteArray& wire : windowEnd->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::SettingsReject) {
            return true;
        }
    }
    return false;
}

} // namespace

class TstRemotePanGrid : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("remote-pan-grid-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false\n"
            "nereus.connection.warning=false\nnereus.stationclient.warning=false\n"
            "qt.qpa.fonts.warning=false"));
    }
    void cleanupTestCase()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }
    void init() { clearGridKeys(); }
    void cleanup()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        clearGridKeys();
    }

    // 1. The per-band grid pair is the Core's; the rest of Display* is not.
    void gridKeysAreTheCores()
    {
        for (const QString& key : {kMax20, kMin20, kMax40, kMin40,
                                   QStringLiteral("DisplayGridMax_160m"),
                                   QStringLiteral("DisplayGridMin_XVTR")}) {
            QCOMPARE(int(classifySettingsKey(key)), int(SettingsScope::Station));
        }
        QCOMPARE(int(classifySettingsKey(QStringLiteral("DisplayGridStep"))),
                 int(SettingsScope::OperatorLocal));
        QCOMPARE(int(classifySettingsKey(QStringLiteral("ClarityFloor_20m"))),
                 int(SettingsScope::OperatorLocal));
    }

    // 2. A window's write moves the Core's pan on that band at once; a
    //    write for another band waits for that band; a removal returns the
    //    default.
    void windowWriteReachesTheCoresPan()
    {
        Session s(m_securityDir.path(), this);
        s.core->addPanadapter();
        PanadapterModel* corePan = s.core->panadapters().first();
        corePan->setCenterFrequency(14200000.0);
        QCOMPARE(corePan->band(), Band::Band20m);
        QCOMPARE(corePan->dBmCeiling(), -40);
        QCOMPARE(corePan->dBmFloor(), -140);
        QVERIFY(s.connect());

        s.write(kMax20, QStringLiteral("-30"));
        s.write(kMin20, QStringLiteral("-120"));
        QTRY_COMPARE(corePan->dBmCeiling(), -30);
        QTRY_COMPARE(corePan->dBmFloor(), -120);
        QCOMPARE(corePan->perBandGrid(Band::Band20m).dbMax, -30);

        s.write(kMax40, QStringLiteral("-50"));
        QTRY_COMPARE(corePan->perBandGrid(Band::Band40m).dbMax, -50);
        QCOMPARE(corePan->dBmCeiling(), -30);  // still on 20 m
        corePan->setCenterFrequency(7100000.0);
        QCOMPARE(corePan->dBmCeiling(), -50);
        QCOMPARE(corePan->dBmFloor(), -140);

        corePan->setCenterFrequency(14200000.0);
        s.remove(kMax20);
        QTRY_COMPARE(corePan->dBmCeiling(), -40);
        QCOMPARE(corePan->dBmFloor(), -120);
        QVERIFY(!anyRefusal(s.window.windowEnd));
    }

    // 3 and 4. A remote window's pan never applies its own grid on a
    //    crossing, and re-reads a band when the Core's value arrives.
    void remotePanTakesTheCoresValues()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.addPanadapter();
        PanadapterModel* pan = remote.panadapters().first();
        QVERIFY(!pan->followsBandGrid());
        pan->setCenterFrequency(14200000.0);
        pan->setdBmCeiling(-25);  // the Core's range, mirrored
        pan->setdBmFloor(-115);

        AppSettings::instance().setValue(kMax40, -60);
        remote.reportStationSettingChanged(kMax40);
        QCOMPARE(pan->perBandGrid(Band::Band40m).dbMax, -60);
        pan->setCenterFrequency(7100000.0);
        QCOMPARE(pan->band(), Band::Band40m);
        // The crossing left the mirrored range for the Core to move.
        QCOMPARE(pan->dBmCeiling(), -25);
        QCOMPARE(pan->dBmFloor(), -115);

        // A whole snapshot re-reads every band.
        AppSettings::instance().setValue(kMax20, -35);
        AppSettings::instance().setValue(kMin20, -125);
        remote.reportStationSettingChanged(QString());
        QCOMPARE(pan->perBandGrid(Band::Band20m).dbMax, -35);
        QCOMPARE(pan->perBandGrid(Band::Band20m).dbMin, -125);
        QCOMPARE(pan->dBmCeiling(), -25);

        // The window's own edit on its band still shows at once.
        pan->setPerBandDbMax(Band::Band40m, -45);
        QCOMPARE(pan->dBmCeiling(), -45);
        QCOMPARE(AppSettings::instance().value(kMax40).toInt(), -45);
    }

    // 5. A local window: unchanged.
    void localPanAppliesItsGridOnACrossing()
    {
        AppSettings::instance().setValue(kMax40, -55);
        AppSettings::instance().setValue(kMin40, -130);
        RadioModel local;
        local.addPanadapter();
        PanadapterModel* pan = local.panadapters().first();
        QVERIFY(pan->followsBandGrid());
        pan->setCenterFrequency(14200000.0);
        pan->setCenterFrequency(7100000.0);
        QCOMPARE(pan->dBmCeiling(), -55);
        QCOMPARE(pan->dBmFloor(), -130);
        // A setting that is not a grid key changes nothing.
        pan->setdBmCeiling(-20);
        pan->applyStationGridSetting(QStringLiteral("DisplayGridStep"));
        QCOMPARE(pan->dBmCeiling(), -20);
        pan->applyStationGridSetting(kMax40);
        QCOMPARE(pan->dBmCeiling(), -55);
    }

    // 6. The page shows the Core's value as it arrives.
    void gridPageShowsTheCoresValue()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.addPanadapter();
        remote.panadapters().first()->setCenterFrequency(14200000.0);
        GridScalesPage page(&remote);
        QSpinBox* dbMax = nullptr;
        for (QSpinBox* spin : page.findChildren<QSpinBox*>()) {
            if (spin->toolTip().startsWith(QStringLiteral("Signal level at the top"))) {
                dbMax = spin;
            }
        }
        QVERIFY(dbMax != nullptr);
        QCOMPARE(dbMax->value(), -40);
        AppSettings::instance().setValue(kMax20, -33);
        remote.reportStationSettingChanged(kMax20);
        QCOMPARE(dbMax->value(), -33);
        AppSettings::instance().setValue(kMax20, -31);
        remote.reportStationSettingChanged(QString());
        QCOMPARE(dbMax->value(), -31);
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstRemotePanGrid)
#include "tst_remote_pan_grid.moc"
