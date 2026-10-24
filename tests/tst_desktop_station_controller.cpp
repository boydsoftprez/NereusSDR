// no-port-check: NereusSDR-original. Task 48 desktop hosting seam; no radio opened.
#include "UnusedLoopbackPort.h"
#include "gui/DesktopStationController.h"

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/StepAttenuatorController.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"

#include <QPointer>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace NereusSDR;

namespace {
StationHostOptions optionsFor(AppSettings& settings, const QString& directory)
{
    StationHostOptions options;
    options.settings = &settings;
    options.securityDirectory = directory;
    options.hostingDevice = StationHostOptions::HostingDevice{
        QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
    options.remoteBind = QStringLiteral("127.0.0.1");
    options.statusPage = false;
    options.remotePort = unusedLoopbackPort();
    return options;
}

const QString kAttenuationKey = QStringLiteral("options/stepAtt/rx1Value");

// The step attenuator saves into the process's own settings (the test
// sandbox, kept between runs). Start each radio from nothing, so a value an
// earlier run saved is never mistaken for this run's.
void forgetRadio(const QString& mac)
{
    AppSettings::instance().load();
    AppSettings::instance().clearHardwareValues(mac);
    AppSettings::instance().save();
}

int savedAttenuation(const QString& mac)
{
    return AppSettings::instance().hardwareValue(mac, kAttenuationKey, -1).toInt();
}
}

class TstDesktopStationController : public QObject {
    Q_OBJECT
private slots:
    void requiresLocalModelAndProfileOwnership()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        StationHostOptions options = optionsFor(settings, directory.path());
        RadioModel local;
        DesktopStationController controller(&local, options);
        QVERIFY(controller.server() == nullptr);
        QVERIFY(!controller.start(false));
        QVERIFY(controller.host() == nullptr);
        QVERIFY(!controller.enabled());

        RadioModel remote(RadioModel::Role::Remote);
        DesktopStationController remoteController(&remote, options);
        QVERIFY(!remoteController.start(true));
        QVERIFY(remoteController.host() == nullptr);

        options.remotePort = 0;
        DesktopStationController disabled(&local, options);
        QVERIFY(!disabled.start(true));
        QVERIFY(!disabled.enabled());
        QVERIFY(disabled.server() == nullptr);

        options = optionsFor(settings, directory.filePath(QStringLiteral("other-profile")));
        DesktopStationController wrongProfile(&local, options);
        QVERIFY(!wrongProfile.start(true));
        QVERIFY(wrongProfile.server() == nullptr);
    }

    void borrowsLocalModelAndReportsActualListener()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        QPointer<RadioModel> borrowed(&model);
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        QVERIFY(controller.host() != nullptr);
        QVERIFY(controller.enabled());
        QVERIFY(controller.server() != nullptr);
        QCOMPARE(controller.server()->deviceSessions()->placesTaken(), 1);
        controller.stop();
        QVERIFY(!controller.enabled());
        QVERIFY(controller.server() == nullptr);
        QVERIFY(borrowed == &model);
        controller.stop();
    }

    void occupiedPortDoesNotClaimToBeEnabled()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        StationHostOptions options = optionsFor(settings, directory.path());
        options.remotePort = occupied.serverPort();
        DesktopStationController controller(&model, options);
        QVERIFY(!controller.start(true));
        QVERIFY(!controller.enabled());
        QCOMPARE(controller.requestMox(true).state,
                 DesktopStationController::RequestState::Refused);
        controller.stop();
    }

    void modelDeathStopsBorrowingHost()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        auto model = std::make_unique<RadioModel>();
        DesktopStationController controller(model.get(), optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        model.reset();
        QVERIFY(controller.host() == nullptr);
        QVERIFY(!controller.enabled());
        QCOMPARE(controller.requestTune(true).state,
                 DesktopStationController::RequestState::Refused);
    }

    void holderQuestionNeverKeysAndStaleConfirmationCannotTake()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        StationServer* const server = controller.server();
        QVERIFY(server != nullptr);
        QObject peerSession;
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = QByteArray("token:desktop-test");
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(peer, &peerSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder* const holder = server->transmitHolder();
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        QVERIFY(holder->isHeldBy(peer.deviceId));

        const auto asked = controller.requestMox(true);
        QCOMPARE(asked.state, DesktopStationController::RequestState::Ask);
        QVERIFY(asked.question.has_value());
        QCOMPARE(asked.question->holderName, QStringLiteral("Phone"));
        QCOMPARE(asked.question->holderEpoch, holder->epoch());
        QVERIFY(!model.mox());
        QVERIFY(holder->isHeldBy(peer.deviceId));

        controller.requestMox(false); // invalidates the displayed question
        QCOMPARE(controller.confirmTake(*asked.question).state,
                 DesktopStationController::RequestState::Refused);
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QVERIFY(!model.mox());

        const auto tuneAsk = controller.requestTune(true);
        QCOMPARE(tuneAsk.state, DesktopStationController::RequestState::Ask);
        QVERIFY(tuneAsk.question.has_value());
        QVERIFY(!model.isTune());
        const auto confirming = controller.confirmTake(*tuneAsk.question);
        QCOMPARE(confirming.state, DesktopStationController::RequestState::Pending);
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));

        // A stale off from the desktop must never end a different holder's key.
        holder->release(SliceOwnership::stationDevice(), QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        controller.requestMox(false);
        controller.requestTune(false);
        QVERIFY(holder->isHeldBy(peer.deviceId));
        controller.stop();
    }

    void shutdownInvalidatesQuestionWithoutKeying()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        StationServer* const server = controller.server();
        TransmitHolder* const holder = server->transmitHolder();
        QObject peerSession;
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = QByteArray("token:shutdown-test");
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        QCOMPARE(server->deviceSessions()->admit(peer, &peerSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        const auto asked = controller.requestMox(true);
        QCOMPARE(asked.state, DesktopStationController::RequestState::Ask);
        QVERIFY(asked.question.has_value());
        controller.stop();
        QCOMPARE(controller.confirmTake(*asked.question).state,
                 DesktopStationController::RequestState::Refused);
        QVERIFY(!model.mox());
        QVERIFY(controller.server() == nullptr);
    }

    void stopCanBeReenteredFromListenerClose()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        StationServer* const server = controller.server();
        int closes = 0;
        connect(server, &StationServer::listeningChanged, &controller, [&](bool listening) {
            if (!listening) {
                ++closes;
                controller.stop();
            }
        });
        controller.stop();
        QCoreApplication::processEvents();
        QCOMPARE(closes, 1);
        QVERIFY(controller.host() == nullptr);
        QVERIFY(!controller.enabled());
    }

    void listenerCloseCanDeleteController()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        auto* controller = new DesktopStationController(
            &model, optionsFor(settings, directory.path()));
        QPointer<DesktopStationController> alive(controller);
        QVERIFY(controller->start(true));
        StationServer* const server = controller->server();
        connect(server, &StationServer::listeningChanged, &model, [&](bool listening) {
            if (!listening) { delete controller; }
        });
        controller->stop();
        QVERIFY(alive.isNull());
    }

    void sliceAdoptionCanDeleteControllerDuringStart()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        QVERIFY(model.addSlice(QStringLiteral("pan-0")) >= 0);
        auto* controller = new DesktopStationController(
            &model, optionsFor(settings, directory.path()));
        QPointer<DesktopStationController> alive(controller);
        connect(model.sliceOwnership(), &SliceOwnership::markChanged, &model,
                [&](int, const QByteArray&, const QByteArray&) { delete controller; });
        QVERIFY(!controller->start(true));
        QVERIFY(alive.isNull());
    }

    void stopDuringStartEndsTxBeforeListenerCloses()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        // This in-process model has no radio connection. Set the existing
        // controller's TUNE flag so stopAllTx emits its completion signal.
        model.moxController()->setTune(true);
        QVERIFY(model.moxController()->isManualMox());
        const StationHostOptions options = optionsFor(settings, directory.path());
        DesktopStationController controller(&model, options);
        QStringList order;
        connect(&model, &RadioModel::transmitStopped, &controller,
                [&](const QString&) { order.append(QStringLiteral("txStopped")); });
        controller.setServerCreatedForTest([&](StationServer* server) {
            connect(server, &StationServer::listeningChanged, &model, [&](bool listening) {
                if (!listening) { order.append(QStringLiteral("listenerClosed")); }
            });
            // Open only loopback through the Host's existing test seam; no
            // peer is admitted before the stop-during-start callback.
            QVERIFY(server->listen(QHostAddress::LocalHost, options.remotePort));
            controller.stop();
        });
        QVERIFY(!controller.start(true));
        QCOMPARE(order, QStringList({QStringLiteral("txStopped"),
                                     QStringLiteral("listenerClosed")}));
        QVERIFY(!controller.enabled());
    }

    void runningHostEndsTxBeforeListenerCloses()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        QStringList order;
        connect(&model, &RadioModel::transmitStopped, &controller,
                [&](const QString&) { order.append(QStringLiteral("txStopped")); });
        connect(controller.server(), &StationServer::listeningChanged, &model,
                [&](bool listening) {
                    if (!listening) { order.append(QStringLiteral("listenerClosed")); }
                });
        model.moxController()->setTune(true);
        QVERIFY(model.moxController()->isManualMox());
        controller.stop();
        QCOMPARE(order, QStringList({QStringLiteral("txStopped"),
                                     QStringLiteral("listenerClosed")}));
    }

    void rejectedTuneCallbackCanDeleteController()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        auto* controller = new DesktopStationController(
            &model, optionsFor(settings, directory.path()));
        QPointer<DesktopStationController> alive(controller);
        QVERIFY(controller->start(true));
        QCOMPARE(controller->server()->takeTransmitForStation({}, {}),
                 TransmitHolder::TakeVerdict::AtOnce);
        connect(&model, &RadioModel::tuneRefused, &model,
                [&](const QString&) { delete controller; });
        controller->requestTune(true);
        QVERIFY(alive.isNull());
    }

    void rejectedTuneCallbackCanCancelItsOwnIntent()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        QCOMPARE(controller.server()->takeTransmitForStation({}, {}),
                 TransmitHolder::TakeVerdict::AtOnce);
        int refusals = 0;
        connect(&model, &RadioModel::tuneRefused, &controller, [&](const QString&) {
            ++refusals;
            controller.requestTune(false);
        });
        QCOMPARE(controller.requestTune(true).state,
                 DesktopStationController::RequestState::Pending);
        QCOMPARE(refusals, 1);
        QVERIFY(!model.isTune());
        QVERIFY(!model.mox());
        controller.stop();
    }

    // The tests below play the window's part: the window owns the step
    // attenuator controller and hands it to its model before the Core starts.

    void runningCoreSavesAttenuatorChangesShortlyAfter()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        RadioModel model;
        model.setStepAttController(&stepAtt);
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(!stepAtt.debouncedSaveEnabled());
        QVERIFY(controller.start(true));
        QVERIFY(stepAtt.debouncedSaveEnabled());
        controller.stop();
        QVERIFY(!stepAtt.debouncedSaveEnabled());
    }

    void changeWhileRunningReachesTheSettingsWithoutStopping()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        const QString mac = QStringLiteral("02:00:00:00:47:01");
        forgetRadio(mac);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        stepAtt.loadSettings(mac);
        RadioModel model;
        model.setStepAttController(&stepAtt);
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        QCOMPARE(savedAttenuation(mac), -1);

        stepAtt.setAttenuation(12);
        QCOMPARE(stepAtt.attenuatorDb(), 12);
        QTRY_COMPARE_WITH_TIMEOUT(savedAttenuation(mac), 12, 20000);
        QVERIFY(controller.enabled());
        QVERIFY(!stepAtt.savePending());
        controller.stop();
    }

    void stoppingWritesAPendingChangeAtOnce()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        const QString mac = QStringLiteral("02:00:00:00:47:02");
        forgetRadio(mac);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        stepAtt.loadSettings(mac);
        RadioModel model;
        model.setStepAttController(&stepAtt);
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));

        stepAtt.setAttenuation(18);
        QVERIFY(stepAtt.savePending());
        QCOMPARE(savedAttenuation(mac), -1);
        controller.stop();
        QCOMPARE(savedAttenuation(mac), 18);
        QVERIFY(!stepAtt.savePending());
        QVERIFY(!stepAtt.debouncedSaveEnabled());
    }

    void leavesAloneASaveItDidNotTurnOn()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        stepAtt.setDebouncedSaveEnabled(true);
        RadioModel model;
        model.setStepAttController(&stepAtt);
        DesktopStationController controller(&model, optionsFor(settings, directory.path()));
        QVERIFY(controller.start(true));
        QVERIFY(stepAtt.debouncedSaveEnabled());
        controller.stop();
        QVERIFY(stepAtt.debouncedSaveEnabled());
    }

    void attenuatorControllerGoneBeforeStopIsSafe()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        {
            auto stepAtt = std::make_unique<StepAttenuatorController>();
            stepAtt->setTickTimerEnabled(false);
            RadioModel model;
            model.setStepAttController(stepAtt.get());
            DesktopStationController controller(&model, optionsFor(settings, directory.path()));
            QVERIFY(controller.start(true));
            model.setStepAttController(nullptr);
            stepAtt.reset();
            controller.stop();
            QVERIFY(!controller.enabled());
            QVERIFY(controller.host() == nullptr);
        }
        {
            // A model with no step attenuator controller starts and stops as before.
            RadioModel model;
            QVERIFY(model.stepAttController() == nullptr);
            DesktopStationController controller(&model, optionsFor(settings, directory.path()));
            QVERIFY(controller.start(true));
            QVERIFY(controller.enabled());
            controller.stop();
            QVERIFY(!controller.enabled());
        }
    }
};

QTEST_MAIN(TstDesktopStationController)
#include "tst_desktop_station_controller.moc"
