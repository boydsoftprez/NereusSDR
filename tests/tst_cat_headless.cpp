// no-port-check: NereusSDR-original CAT policy/lifecycle regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#define private public
#include "core/daemon/DaemonApp.h"
#undef private
#include "core/daemon/DaemonConfig.h"
#include "core/MoxController.h"
#include "models/RadioModel.h"
using namespace NereusSDR;
class TstCatHeadless : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings& settings = AppSettings::instance();
        for (const QString& key : settings.allKeys()) {
            if (key.startsWith("Cat/")) { settings.remove(key); }
        }
    }
    void localAndRemoteLifetime()
    {
        RadioModel local;
        CatService* service = local.catService();
        QVERIFY(service); QVERIFY(!service->isStarted());
        QSignalSpy connection(&local, &RadioModel::connectionStateChanged);
        QSignalSpy mox(local.moxController(), &MoxController::moxChanged);
        service->startConfigured(); QVERIFY(service->isStarted());
        QVERIFY(!service->isListening(1)); QCOMPARE(connection.size(), 0); QCOMPARE(mox.size(), 0);
        CatEndpointConfig config = service->channelConfig(1);
        QVERIFY(!service->applyChannelConfig(1, config));
        const quint64 session = service->openSession(1, CatTransportKind::Tester);
        QVERIFY(session); QVERIFY(!service->session(session)->context().transmitAllowed); QCOMPARE(service->processFrame(session, "ZZEM1;"), QByteArray());
        QCOMPARE(service->processFrame(session, "ZZEM;"), QByteArray("ZZEM1;"));
        service->stopAll(); service->stopAll(); QVERIFY(!service->isStarted());
        QCOMPARE(service->processFrame(session, "ZZEM;"), QByteArray("?;"));
        config.tcpEnabled = true; config.tcpPort = 13013;
        QVERIFY(service->applyChannelConfig(1, config));
        service->startConfigured(); QVERIFY(!service->isListening(1));
        QVERIFY(service->channelState(1).contains("unavailable", Qt::CaseInsensitive));
        service->stopAll();
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.catService()); remote.catService()->startConfigured();
        QVERIFY(!remote.catService()->isStarted()); QVERIFY(!remote.catService()->openSession(1, CatTransportKind::Tester));
    }
    void synchronousLifecycleCallbacks()
    {
        RadioModel model;
        CatService* service = model.catService();
        bool restarted = false;
        connect(service, &CatService::channelStateChanged, &model, [&](int, const QString& state) {
            if (!restarted && state == "Disabled") {
                restarted = true;
                service->stopAll();
                service->startConfigured();
            }
        });
        service->startConfigured(); QVERIFY(restarted); QVERIFY(service->isStarted());
        for (int channel = 1; channel <= 4; ++channel) { QCOMPARE(service->channelState(channel), QString("Disabled")); }
        const quint64 id = service->openSession(1, CatTransportKind::Tester);
        const quint64 secondOld = service->openSession(2, CatTransportKind::Tester);
        quint64 replacement = 0;
        bool restartOnClose = false;
        connect(service, &CatService::sessionClosed, &model, [&](quint64) {
            if (!restartOnClose) {
                restartOnClose = true; service->startConfigured();
                replacement = service->openSession(1, CatTransportKind::Tester);
            }
        });
        service->stopAll(); QVERIFY(service->isStarted());
        QVERIFY(!service->session(id)); QVERIFY(!service->session(secondOld));
        QVERIFY(replacement && service->session(replacement));
        QCOMPARE(service->channelState(4), QString("Disabled"));
        const quint64 next = service->openSession(1, CatTransportKind::Tester);
        connect(service, &CatService::globalConfigurationChanged, &model, [&] { service->stopAll(); });
        QCOMPARE(service->processFrame(next, "ZZID;"), QByteArray("?;"));
        QVERIFY(!service->isStarted());
    }
    void callbackDestroysModel()
    {
        std::unique_ptr<RadioModel> model = std::make_unique<RadioModel>();
        CatService* service = model->catService();
        connect(service, &CatService::channelStateChanged, service, [&](int, const QString& state) {
            if (state == "Disabled") { model.reset(); }
        });
        service->startConfigured(); QVERIFY(!model);
        model = std::make_unique<RadioModel>(); service = model->catService(); service->startConfigured();
        const quint64 session = service->openSession(1, CatTransportKind::Tester);
        connect(service, &CatService::globalConfigurationChanged, service, [&] { model.reset(); });
        QCOMPARE(service->processFrame(session, "ZZID;"), QByteArray("?;")); QVERIFY(!model);
    }
    void retirementCannotRestart()
    {
        std::unique_ptr<RadioModel> model = std::make_unique<RadioModel>();
        CatService* service = model->catService(); service->startConfigured();
        const quint64 id = service->openSession(1, CatTransportKind::Tester);
        QVERIFY(id);
        bool attempted = false; bool restarted = false;
        const auto attemptRestart = [&] {
            attempted = true;
            service->startConfigured();
            restarted = restarted || service->isStarted();
            QVERIFY(!service->applyChannelConfig(1, service->channelConfig(1)));
            QVERIFY(!service->applyGlobalConfig(service->globalConfig()));
            QCOMPARE(service->openSession(1, CatTransportKind::Tester), quint64(0));
        };
        connect(service, &CatService::sessionClosed, service, [&](quint64) { attemptRestart(); });
        connect(service, &CatService::channelStateChanged, service, [&](int, const QString& state) {
            if (state == "Stopped") { attemptRestart(); }
        });
        model.reset(); QVERIFY(attempted); QVERIFY(!restarted);
    }
    void invalidRestorationAndStableSessions()
    {
        AppSettings& settings = AppSettings::instance();
        settings.setValue("Cat/Channels/1/TcpEnabled", "True");
        settings.setValue("Cat/Channels/1/TcpPort", 0);
        RadioModel model;
        model.addSlice(); model.addSlice();
        CatService* service = model.catService();
        service->startConfigured();
        QVERIFY(service->channelState(1).startsWith("Invalid")); QVERIFY(!service->isListening(1));
        QCOMPARE(settings.value("Cat/Channels/1/TcpEnabled").toString(), QString("True"));
        QCOMPARE(settings.value("Cat/Channels/1/TcpPort").toInt(), 0);
        const quint64 id = service->openSession(1, CatTransportKind::Tester);
        QVERIFY(id);
        const CatBinding old = service->session(id)->binding();
        model.removeSlice(0); QCOMPARE(model.addSlice(), 0);
        emit model.connectionStateChanged(ConnectionState::Disconnected);
        QVERIFY(!service->adapter().resolveSlice(service->session(id)->binding(), CatVfo::Primary));
        QCOMPARE(service->session(id)->binding().primaryIncarnation, old.primaryIncarnation);
        service->stopAll(); service->startConfigured();
        const quint64 rebound = service->openSession(1, CatTransportKind::Tester);
        QVERIFY(service->adapter().resolveSlice(service->session(rebound)->binding(), CatVfo::Primary));
    }
    void daemonStoppedDuringCatStartup()
    {
        DaemonApp daemon; daemon.primeBoardForTest(HPSDRHW::HermesLite);
        bool stopped = false;
        daemon.m_radioInitializerForTest = [&](RadioModel* model) {
            connect(model->catService(), &CatService::channelStateChanged, &daemon, [&](int, const QString& state) {
                if (!stopped && state == "Disabled") { stopped = true; daemon.stop(); }
            });
        };
        DaemonConfig config = DaemonConfig::defaults(); config.remotePort = 0; config.statusPage = false;
        QVERIFY(!daemon.start(config)); QVERIFY(stopped); QVERIFY(!daemon.radioModelForTest());
    }
    void daemonRestart()
    {
        DaemonApp daemon;
        daemon.primeBoardForTest(HPSDRHW::HermesLite);
        DaemonConfig config = DaemonConfig::defaults(); config.remotePort = 0; config.statusPage = false;
        QVERIFY(daemon.start(config));
        QVERIFY(daemon.radioModelForTest()->catService()->isStarted());
        QPointer<CatService> service(daemon.radioModelForTest()->catService());
        daemon.beginStationRelease(); QVERIFY(!service->isStarted());
        QCOMPARE(daemon.recoverFailedStationRelease(), DaemonApp::StationReleaseRecoveryResult::Restored);
        QVERIFY(service->isStarted());
        daemon.stop(); QVERIFY(!service || !service->isStarted());
        QVERIFY(daemon.start(config)); QVERIFY(daemon.radioModelForTest()->catService()->isStarted());
        daemon.stop();
    }
};
QTEST_GUILESS_MAIN(TstCatHeadless)
#include "tst_cat_headless.moc"
