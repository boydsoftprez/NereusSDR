// no-port-check: NereusSDR-original desktop Core runtime tests; temporary profile only.
#include "gui/GuiDesktopStationRuntime.h"
#include "core/session/RemoteDevicesState.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "gui/SetupDialog.h"
#include "gui/setup/RemoteStationPage.h"
#include "core/session/StationServer.h"
#include "core/station/StationRadios.h"
#include "core/station/StationHost.h"
#include "core/security/StationIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/ConnectionState.h"
#include "core/SliceOwnership.h"
#include "core/ReceiverManager.h"
#include "core/ReceiveLayoutStore.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/session/SessionMessages.h"
#include "models/SliceModel.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsScope.h"
#include <QCheckBox>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

using namespace NereusSDR;

namespace {
int freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) return 0;
    return probe.serverPort();
}

QString serviceConfigPath(AppSettings& settings)
{
    return QFileInfo(settings.filePath()).absolutePath() + QStringLiteral("/station.conf");
}

bool writeConfig(const QString& path, int port)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(QStringLiteral("remote_port = %1\nremote_bind = 127.0.0.1\n"
                                     "status_page = off\nrendezvous_servers =\n")
                          .arg(port).toUtf8()) > 0;
}

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

SessionMessage diversityRequest(const RadioModel& model, int source, int target)
{
    const auto* own = model.sliceOwnership();
    const auto number = [](const char* name, qint64 value) {
        return MirrorUpdate{0, name, MirrorWireKind::Int64, value};
    };
    return SessionMessages::commandInvoke("diversity.setTarget", 902,
        {{0, "enabled", MirrorWireKind::Bool, target >= 0},
         number("stateRevision", model.diversityStateRevision()),
         number("sourceSliceId", source), number("sourceIncarnation", source < 0 ? 0 : own->incarnation(source)),
         number("sourceControlRevision", source < 0 ? 0 : own->controlRevision(source)),
         number("targetSliceId", target), number("targetIncarnation", target < 0 ? 0 : own->incarnation(target)),
         number("targetControlRevision", target < 0 ? 0 : own->controlRevision(target))});
}

StationServiceOptions isolatedService(AppSettings& settings, const QString& home)
{
    StationServiceOptions service;
    service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
    service.homeDirectory = home;
    service.inheritActiveProfile = false;
    service.runner = [](const QString&, const QStringList&) { return StationServiceCommandResult{0, {}}; };
    return service;
}

QString preservedSettingsPath(const QString& path)
{
    return path + QStringLiteral(".runtime-test-original");
}

bool blockSettingsSave(const QString& path)
{
    const QString preserved = preservedSettingsPath(path);
    if (!QFileInfo(path).isFile() || QFileInfo::exists(preserved)
        || !QFile::rename(path, preserved)) {
        return false;
    }
    if (QDir().mkdir(path)) {
        QFile sentinel(QDir(path).filePath(QStringLiteral("save-blocker")));
        if (sentinel.open(QIODevice::WriteOnly)
            && sentinel.write("keep directory nonempty") > 0) {
            return true;
        }
        sentinel.close();
        QFile::remove(sentinel.fileName());
        QDir().rmdir(path);
    }
    QFile::rename(preserved, path);
    return false;
}

bool restoreSettingsFile(const QString& path)
{
    return QFile::remove(QDir(path).filePath(QStringLiteral("save-blocker")))
        && QDir().rmdir(path) && QFile::rename(preservedSettingsPath(path), path);
}
}

class NeverBackend final : public ISettingsBackend {
public:
    bool handlesKey(const QString&) const override { return false; }
    QVariant value(const QString&, const QVariant& fallback) const override { return fallback; }
    void setValue(const QString&, const QVariant&) override {}
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override {}
    QStringList handledKeys() const override { return {}; }
};

class TstGuiDesktopStationRuntime : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("DesktopRuntimeTest_%1")
                                            .arg(QCoreApplication::applicationPid()));
        QDir().mkpath(QFileInfo(AppSettings::instance().filePath()).absolutePath());
    }
    void cleanup()
    {
        auto& settings = AppSettings::instance();
        settings.remove(QStringLiteral("DesktopCore/Run"));
        settings.remove(QStringLiteral("DesktopCore/KeepRunning"));
        settings.remove(QStringLiteral("StationLabel"));
        settings.remove(QStringLiteral("StationKeyBackupAcknowledged"));
        settings.remove(QStringLiteral("StationRadioChoice"));
        settings.remove(QStringLiteral("DesktopRuntimeRetirementMarker"));
        settings.save();
        QFile::remove(QFileInfo(settings.filePath()).absolutePath()
                      + QStringLiteral("/station.conf"));
    }
    void cleanupTestCase()
    {
        QDir(QFileInfo(AppSettings::instance().filePath()).absolutePath()).removeRecursively();
    }
    void desktopPreferencesStayOnThisComputer()
    {
        QCOMPARE(classifySettingsKey(QStringLiteral("DesktopCore/Run")),
                 SettingsScope::OperatorLocal);
        QCOMPARE(classifySettingsKey(QStringLiteral("DesktopCore/KeepRunning")),
                 SettingsScope::OperatorLocal);
        QCOMPARE(classifySettingsKey(QStringLiteral("StationRadioChoice")),
                 SettingsScope::OperatorLocal);
    }

    // 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
    // Real desktop bootstrap, no caller-side owner mark or CreatorScope.
    void runOffBootstrapOwnsOrdinaryLocalSlicesWithoutStartingHost()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), 8443));
        settings.setValue(QStringLiteral("DesktopCore/Run"), false);
        P2CodecOrionMkII codec;
        RadioModel model;
        const int a = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(a >= 0);
        QVERIFY(model.sliceOwnership()->mark(a).owner.isEmpty());
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.restore());
        QCOMPARE(model.sliceOwnership()->mark(a).owner, SliceOwnership::stationDevice());
        const int b = model.addSlice(QStringLiteral("pan-0"));
        const int c = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(b >= 0 && c >= 0);
        QCOMPARE(model.sliceOwnership()->mark(b).owner, SliceOwnership::stationDevice());
        QCOMPARE(model.sliceOwnership()->mark(c).owner, SliceOwnership::stationDevice());
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(!runtime.controller()->server());
        QVERIFY(!QFileInfo::exists(service.profileDirectory + QStringLiteral("/station-identity.pem")));
        // Only software codec/resource seams; no radio discovery or connection.
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.configureStreamPool(5, 5, 192000);
        model.receiverManager()->setMaxReceivers(5);
        model.bindUnboundSlices();
        model.receiverManager()->setP2Codec(&codec);
        for (const auto pair : {std::pair{-1, a}, std::pair{a, a}, std::pair{a, -1},
                                std::pair{-1, b}, std::pair{b, c}, std::pair{c, b}, std::pair{b, -1}}) {
            const auto result = model.invokeDiversityAsStationDevice(diversityRequest(model, pair.first, pair.second));
            QVERIFY2(result.accepted, qPrintable(result.reason));
            QCOMPARE(QJsonDocument::fromJson(model.diversityState().toUtf8()).object()
                         .value("requested").toBool(), pair.second >= 0);
        }
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        model.setConnectionStateForTest(ConnectionState::Connected);
        const int d = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(d >= 0);
        QCOMPARE(model.sliceOwnership()->mark(d).owner, SliceOwnership::stationDevice());
        QVERIFY(runtime.restore());
        QVERIFY(!runtime.controller()->server());
    }

    void invalidListenerConfigDoesNotBlockLocalOwnershipOrCreateIdentity()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes("remote_port = 70000\n");
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        RadioModel model;
        const int a = model.addSlice("pan-0");
        auto service = isolatedService(settings, temp.filePath("home"));
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!runtime.restore()); // listener error still reported
        QCOMPARE(model.sliceOwnership()->mark(a).owner, SliceOwnership::stationDevice());
        const int b = model.addSlice("pan-0");
        QCOMPARE(model.sliceOwnership()->mark(b).owner, SliceOwnership::stationDevice());
        QCOMPARE(fileBytes(path), bytes);
        QVERIFY(!runtime.controller()->server());
        QVERIFY(!QFileInfo::exists(service.profileDirectory + "/station-identity.pem"));
    }

    void bootstrapPreservesExplicitMarksListenersActiveChoiceAndRevisions()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        RadioModel model;
        auto* own = model.sliceOwnership();
        const int a = model.addSlice("pan-0");
        int foreign;
        { SliceOwnership::CreatorScope scope(own, "other"); foreign = model.addSlice("pan-0"); }
        const int held = model.addSlice("pan-0");
        own->hold(held, "absent");
        const int listened = model.addSlice("pan-0");
        own->join("listener", listened);
        const quint64 foreignRevision = own->controlRevision(foreign);
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true,
                                         isolatedService(settings, temp.filePath("home")));
        QVERIFY(runtime.restore());
        QCOMPARE(own->mark(a).owner, SliceOwnership::stationDevice());
        QCOMPARE(own->mark(foreign).owner, QByteArray("other"));
        QCOMPARE(own->controlRevision(foreign), foreignRevision);
        QCOMPARE(own->mark(held).heldFor, QByteArray("absent"));
        QVERIFY(own->mark(listened).owner.isEmpty());
        QCOMPARE(own->listenersOf(listened), QList<QByteArray>{"listener"});
        const quint64 revision = own->controlRevision(a);
        const int b = model.addSlice("pan-0");
        QVERIFY(model.setActiveSliceByIdFor(SliceOwnership::stationDevice(), b));
        const int c = model.addSlice("pan-0");
        QCOMPARE(own->activeFor(SliceOwnership::stationDevice()), b);
        QVERIFY(runtime.restore());
        QCOMPARE(own->controlRevision(a), revision);
        QCOMPARE(own->activeFor(SliceOwnership::stationDevice()), b);
        own->setOwner(c, {});
        own->leave(SliceOwnership::stationDevice(), c);
        QCoreApplication::processEvents();
        QVERIFY(own->mark(c).owner.isEmpty()); // no adoption on mark/release
        runtime.stop();
        const int afterClose = model.addSlice("pan-0");
        QVERIFY(own->mark(afterClose).owner.isEmpty());
    }

    void bootstrapRefusesWrongAuthority_data()
    {
        QTest::addColumn<int>("kind");
        for (int kind = 0; kind < 6; ++kind) {
            QTest::newRow(qPrintable(QString::number(kind))) << kind;
        }
    }
    void bootstrapRefusesWrongAuthority()
    {
        QFETCH(int, kind);
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        AppSettings other(temp.filePath("other.settings"));
        NeverBackend backend;
        RadioModel model(kind == 3 ? RadioModel::Role::Remote : RadioModel::Role::Local);
        const int a = kind == 3 ? -1 : model.addSlice("pan-0");
        auto service = isolatedService(settings, temp.filePath("home"));
        if (kind == 1) { service.profileDirectory = temp.path(); }
        if (kind == 4) { settings.setRemoteBackend(&backend); }
        const auto restoreBackend = qScopeGuard([&] { settings.setRemoteBackend(nullptr); });
        GuiDesktopStationRuntime runtime(&model, kind == 2 ? &other : &settings,
            AppSettings::profileOverride(), kind != 0, service);
        if (kind == 5) { runtime.stop(); }
        QVERIFY(!runtime.restore());
        if (a >= 0) {
            QVERIFY(model.sliceOwnership()->mark(a).owner.isEmpty());
            const int b = model.addSlice("pan-0");
            QVERIFY(model.sliceOwnership()->mark(b).owner.isEmpty());
        }
        QVERIFY(!runtime.controller()->server());
    }

    void adoptionMayRetireRuntimeWithoutLeavingAnAuthority()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        RadioModel model;
        const int a = model.addSlice("pan-0");
        auto runtime = std::make_unique<GuiDesktopStationRuntime>(&model, &settings,
            AppSettings::profileOverride(), true, isolatedService(settings, temp.filePath("home")));
        QPointer<GuiDesktopStationRuntime> observed(runtime.get());
        connect(model.sliceOwnership(), &SliceOwnership::markChanged, this,
                [&](int id, const QByteArray&, const QByteArray&) { if (id == a) { runtime.reset(); } });
        auto* current = runtime.get();
        QVERIFY(!current->restore());
        QVERIFY(!observed);
        const int b = model.addSlice("pan-0");
        QVERIFY(model.sliceOwnership()->mark(b).owner.isEmpty());
    }

    void laterSliceAdoptionMayRetireModelWithoutResumingCreation()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        auto model = std::make_unique<RadioModel>();
        model->addSlice("pan-0");
        GuiDesktopStationRuntime runtime(model.get(), &settings, AppSettings::profileOverride(), true,
                                         isolatedService(settings, temp.filePath("home")));
        QVERIFY(runtime.restore());
        QPointer<RadioModel> observed(model.get());
        connect(model->sliceOwnership(), &SliceOwnership::markChanged, this,
                [&](int, const QByteArray&, const QByteArray&) { model.reset(); });
        auto* current = model.get();
        QCOMPARE(current->addSlice("pan-0"), -1);
        QVERIFY(!observed);
    }

    void hydrationAdoptsOnlyFinalUnclaimedRoster()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true,
                                         isolatedService(settings, temp.filePath("home")));
        QVERIFY(runtime.restore());
        ReceiveLayoutStore::LoadResult layout;
        layout.state = ReceiveLayoutStore::LoadState::Loaded;
        for (int id = 0; id < 4; ++id) {
            layout.slices.append({id, "pan-0", 14'200'000.0 + id * 1000, DSPMode::USB});
        }
        layout.slices[1].owner = "other";
        layout.slices[2].owner = SliceOwnership::stationDevice();
        layout.slices[2].heldFor = "absent";
        layout.slices[3].owner = SliceOwnership::stationDevice();
        int intermediateStationOwners = 0;
        connect(&model, &RadioModel::sliceAdded, this, [&](int id) {
            if (model.sliceOwnership()->mark(id).owner == SliceOwnership::stationDevice()) {
                ++intermediateStationOwners;
            }
        });
        bool finalUnclaimedObserved = false;
        QList<QByteArray> finalHeldListeners;
        connect(&model, &RadioModel::receiveLayoutHydrated, this, [&] {
            finalUnclaimedObserved = model.sliceOwnership()->mark(0).owner.isEmpty();
            finalHeldListeners = model.sliceOwnership()->listenersOf(1);
            qInfo() << "final held listeners BEFORE queued adoption" << finalHeldListeners;
        });
        RadioModel baseline;
        QString reason;
        QVERIFY2(baseline.hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &reason), qPrintable(reason));
        const auto baselineHeldListeners = baseline.sliceOwnership()->listenersOf(1);
        qInfo() << "raw baseline held listeners" << baselineHeldListeners;
        QVERIFY2(model.hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &reason), qPrintable(reason));
        QCOMPARE(intermediateStationOwners, 0);
        QVERIFY(finalUnclaimedObserved);
        auto* own = model.sliceOwnership();
        const auto heldRevision = own->controlRevision(1);
        QTRY_COMPARE(own->mark(0).owner, SliceOwnership::stationDevice());
        QCOMPARE(own->mark(1).heldFor, QByteArray("other"));
        QCOMPARE(own->controlRevision(1), heldRevision);
        QCOMPARE(own->mark(2).heldFor, QByteArray("absent"));
        QCOMPARE(finalHeldListeners, baselineHeldListeners);
        QCOMPARE(own->listenersOf(1), baselineHeldListeners);
        QCOMPARE(own->listenersOf(2), baseline.sliceOwnership()->listenersOf(2));
        QCOMPARE(own->mark(3).owner, SliceOwnership::stationDevice());
        QVERIFY(!runtime.controller()->server());
    }

    void authorityRetirementDuringAdoptionDoesNotClaimRemainingSlices_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("close") << 0;
        QTest::newRow("destroy-runtime") << 1;
        QTest::newRow("settings-backend") << 2;
    }
    void authorityRetirementDuringAdoptionDoesNotClaimRemainingSlices()
    {
        QFETCH(int, kind);
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        NeverBackend backend;
        const auto restoreBackend = qScopeGuard([&] { settings.setRemoteBackend(nullptr); });
        RadioModel model;
        const int a = model.addSlice("pan-0");
        const int b = model.addSlice("pan-0");
        auto runtime = std::make_unique<GuiDesktopStationRuntime>(&model, &settings,
            AppSettings::profileOverride(), true, isolatedService(settings, temp.filePath("home")));
        connect(model.sliceOwnership(), &SliceOwnership::markChanged, this,
                [&](int id, const QByteArray&, const QByteArray&) {
            if (id != a) { return; }
            if (kind == 0) { runtime->stop(); }
            else if (kind == 1) { runtime.reset(); }
            else { settings.setRemoteBackend(&backend); }
        });
        auto* current = runtime.get();
        QVERIFY(!current->restore());
        QCOMPARE(model.sliceOwnership()->mark(a).owner, SliceOwnership::stationDevice());
        QCOMPARE(model.sliceOwnership()->controlRevision(a), quint64(2));
        QVERIFY(model.sliceOwnership()->mark(b).owner.isEmpty());
        QCOMPARE(model.sliceOwnership()->controlRevision(b), quint64(1));
    }

    void adoptionKeepsAReentrantlyHeldRemainingSlice()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        RadioModel model;
        const int a = model.addSlice("pan-0");
        const int b = model.addSlice("pan-0");
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true,
                                         isolatedService(settings, temp.filePath("home")));
        connect(model.sliceOwnership(), &SliceOwnership::markChanged, this,
                [&](int id, const QByteArray&, const QByteArray&) {
            if (id == a) { model.sliceOwnership()->hold(b, "other"); }
        });
        QVERIFY(runtime.restore());
        QCOMPARE(model.sliceOwnership()->mark(a).owner, SliceOwnership::stationDevice());
        QCOMPARE(model.sliceOwnership()->mark(b).heldFor, QByteArray("other"));
    }

    void adoptionActivePublicationMayRetireModel()
    {
        QTemporaryDir temp;
        auto& settings = AppSettings::instance();
        auto model = std::make_unique<RadioModel>();
        const int a = model->addSlice("pan-0");
        model->sliceById(a)->setActive(false);
        GuiDesktopStationRuntime runtime(model.get(), &settings, AppSettings::profileOverride(), true,
                                         isolatedService(settings, temp.filePath("home")));
        QPointer<RadioModel> observed(model.get());
        connect(model->sliceById(a), &SliceModel::activeChanged, this, [&](bool active) {
            if (active) { model.reset(); }
        });
        QVERIFY(!runtime.restore());
        QVERIFY(!observed);
    }

    void defaultsDoNotStartAListener()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        RadioModel model;
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString& program, const QStringList& args) {
            if (program == QStringLiteral("systemctl")
                && args.contains(QStringLiteral("is-enabled"))) {
                return StationServiceCommandResult{1, {}};
            }
            return StationServiceCommandResult{0, {}};
        };
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!runtime.state().runCore);
        QVERIFY(!runtime.state().keepRunning);
        QVERIFY(!runtime.state().startWithComputer);
        QVERIFY(runtime.controller() != nullptr);
        runtime.restore();
        QVERIFY(!runtime.controller()->enabled());
    }

    void runOpensAndClosesRealTemporaryListener()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const int port = freePort();
        QVERIFY(port > 0);
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), port));
        RadioModel model;
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        const int a = model.addSlice("pan-0");
        QVERIFY(runtime.restore());
        const quint64 revision = model.sliceOwnership()->controlRevision(a);
        QCOMPARE(model.sliceOwnership()->mark(a).owner, SliceOwnership::stationDevice());
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.controller()->enabled());
        QCOMPARE(settings.value(QStringLiteral("DesktopCore/Run")).toBool(), true);
        // iPhone app plan Tasks 49 and 78 item 8: the reach line says what
        // is true now, not a placeholder.
        QTRY_VERIFY2(runtime.state().reachabilityText.startsWith(QStringLiteral("Listening on ")),
                     qPrintable(runtime.state().reachabilityText));
        QVERIFY(!runtime.state().reachabilityText.contains(QStringLiteral("not verified")));
        // Task 78 item 8: this desktop holds its place and is listed as the
        // window that runs the Core.
        QTRY_COMPARE(runtime.connectedDevices()->connectedDevices().size(), 1);
        const RemoteConnectedDevice self = runtime.connectedDevices()->connectedDevices().first();
        QVERIFY(self.hostsCore);
        QCOMPARE(runtime.connectedDevices()->selfDeviceId(), self.deviceId);
        QCOMPARE(runtime.connectedDevices()->deviceLimit(), 4);
        QTcpServer occupied;
        QVERIFY(!occupied.listen(QHostAddress::LocalHost, port));
        QVERIFY(runtime.setRunCore(false));
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(occupied.listen(QHostAddress::LocalHost, port));
        QCOMPARE(settings.value(QStringLiteral("DesktopCore/Run")).toBool(), false);
        QVERIFY(runtime.connectedDevices()->connectedDevices().isEmpty());
        const int b = model.addSlice("pan-0");
        QCOMPARE(model.sliceOwnership()->mark(b).owner, SliceOwnership::stationDevice());
        QCOMPARE(model.sliceOwnership()->controlRevision(a), revision);
        occupied.close(); // release this test's own port probe before restarting
        QVERIFY(runtime.setRunCore(true));
        QCOMPARE(model.sliceOwnership()->controlRevision(a), revision);
        QVERIFY(runtime.setRunCore(false));
        const int c = model.addSlice("pan-0");
        QCOMPARE(model.sliceOwnership()->mark(c).owner, SliceOwnership::stationDevice());
    }

    // iPhone app plan Tasks 49 and 78 item 8: the reach line from the
    // Core's listener, Bonjour and the remote access service.
    void reachLineSaysHowDevicesFindTheCore()
    {
        StationReach reach;
        reach.listening = true;
        reach.bonjourAvailable = true;
        reach.bonjourActive = true;
        reach.serviceConfigured = true;
        reach.serviceRegistered = true;
        reach.serviceHost = QStringLiteral("rv.example.net");
        QCOMPARE(GuiDesktopStationRuntime::reachText(QString(), 50055, reach),
                 QStringLiteral("Listening on every network on this computer, port 50055. "
                                "Devices on this network find it by Bonjour. Registered with the "
                                "remote access service at rv.example.net, so paired devices reach "
                                "it away from this network."));
        reach.serviceRegistered = false;
        reach.bonjourActive = false;
        QCOMPARE(GuiDesktopStationRuntime::reachText(QStringLiteral("10.0.0.5"), 50055, reach),
                 QStringLiteral("Listening on 10.0.0.5, port 50055. Not announced by Bonjour. "
                                "Not registered with the remote access service at "
                                "rv.example.net."));
        reach.bonjourAvailable = false;
        reach.serviceConfigured = false;
        reach.listening = false;
        reach.listenerRetryPending = true;
        const QString text = GuiDesktopStationRuntime::reachText(QString(), 50055, reach);
        QVERIFY(text.startsWith(QStringLiteral("Port 50055 on every network on this computer is "
                                               "not open; trying again.")));
        QVERIFY(text.contains(QStringLiteral("Bonjour is not available on this computer")));
        QVERIFY(text.endsWith(QStringLiteral("paired devices reach it only on this network.")));
    }

    void blockedPortCannotRetryAfterRejectedRunIntent()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        const int port = occupied.serverPort();
        QVERIFY(writeConfig(serviceConfigPath(settings), port));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!runtime.setRunCore(true));
        QVERIFY(runtime.controller()->host() == nullptr);
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(!settings.value(QStringLiteral("DesktopCore/Run"), false).toBool());
        occupied.close();
        QTcpServer next;
        QVERIFY(next.listen(QHostAddress::LocalHost, port));
        QVERIFY(runtime.controller()->host() == nullptr);
    }

    void restoreUsesSavedRunPreferenceAndActualListener()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        settings.setValue(QStringLiteral("DesktopCore/Run"), true);
        QVERIFY(settings.save());
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!runtime.state().runCore);
        QVERIFY(runtime.restore());
        QVERIFY(runtime.state().runCore);
        QVERIFY(runtime.controller()->enabled());
        runtime.stop();
        QVERIFY(!runtime.state().runCore);
    }

    void suppliedRadioBindingsAdvertiseCurrentChoiceAndCapability()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        QVERIFY(writeConfig(path, freePort()));
        QFile config(path);
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Append));
        config.write("station_bind = 127.0.0.1\n");
        config.close();
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        StationRadios radios(settings);
        const QString selected = QStringLiteral("02:00:00:00:00:92");
        RuntimeStationBindings bindings;
        bindings.stationRadios = &radios;
        bindings.selectedRadioMac = [selected] { return selected; };
        bindings.linkMajors = {1};
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(),
                                         true, service, bindings);
        QCOMPARE(runtime.config().stationBind, QStringLiteral("127.0.0.1"));
        QVERIFY(runtime.setRunCore(true));
        QCOMPARE(runtime.controller()->server()->stationRadiosVersion(), 1);
        QCOMPARE(runtime.controller()->host()->stationAnnouncementForTest().radioMac,
                 selected);
        runtime.stop();
    }

    void explicitNetworkAndMediaPolicyFeedsBorrowedHost()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        const int port = freePort();
        QVERIFY(writeConfig(path, port));
        QFile config(path);
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Append));
        config.write("pairing_lan_click = deny\nremote_transmit = deny\n"
                     "relay = deny\naudio_bitrate = 24000\naudio_lossless = deny\n");
        config.close();
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        StationServer* server = runtime.controller()->server();
        QVERIFY(server != nullptr);
        QCOMPARE(server->serverPort(), quint16(port));
        QCOMPARE(server->serverAddress(), QHostAddress::LocalHost);
        QVERIFY(!server->pairingLanClickAllowed());
        QVERIFY(!server->remoteTransmitAllowed());
        QVERIFY(!server->relayAllowed());
        QVERIFY(!runtime.state().reachabilityText.contains(QStringLiteral("anywhere"),
                                                            Qt::CaseInsensitive));
        runtime.stop();
    }

    void invalidExplicitConfigIsNeverOverwritten()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        QFile config(path);
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray bytes("remote_port = 70000\n");
        QCOMPARE(config.write(bytes), bytes.size());
        config.close();
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!runtime.state().available);
        QVERIFY(!runtime.setRunCore(true));
        QVERIFY(!runtime.setKeepRunning(true));
        QFile preserved(path);
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), bytes);
    }

    void invalidHostConfigAllowsCheckedLocalRetirementWithRunOff()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("DesktopCore/Run"), false);
        settings.setValue(QStringLiteral("DesktopRuntimeRetirementMarker"),
                          QStringLiteral("saved before close"));
        const QString path = serviceConfigPath(settings);
        const QByteArray bytes("remote_port = 70000\n");
        QFile config(path);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QCOMPARE(config.write(bytes), bytes.size());
        config.close();
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        for (const bool appQuit : {false, true}) {
            GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(),
                                             true, service);
            QVERIFY(!runtime.state().available);
            QVERIFY(!runtime.setRunCore(true));
            QString reason;
            QVERIFY2(runtime.prepareForRetirement(appQuit, &reason), qPrintable(reason));
            QVERIFY(!runtime.backgroundStartWanted());
            QVERIFY(!runtime.controller()->enabled());
            AppSettings saved(settings.filePath());
            saved.load();
            QCOMPARE(saved.value(QStringLiteral("DesktopRuntimeRetirementMarker")).toString(),
                     QStringLiteral("saved before close"));
            QFile preserved(path);
            QVERIFY(preserved.open(QIODevice::ReadOnly));
            QCOMPARE(preserved.readAll(), bytes);
        }
        StationServiceOptions wrongProfile = service;
        wrongProfile.profileDirectory = temp.path();
        GuiDesktopStationRuntime mismatched(&model, &settings, AppSettings::profileOverride(),
                                            true, wrongProfile);
        QString reason;
        QVERIFY(!mismatched.prepareForRetirement(false, &reason));
        QVERIFY(reason.contains(QStringLiteral("profile"), Qt::CaseInsensitive));
    }

    void wrongSettingsStoreCannotHostOrRetire()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings other(temp.filePath(QStringLiteral("other.settings")));
        RadioModel model;
        StationServiceOptions service;
        service.profileDirectory = temp.path();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiDesktopStationRuntime runtime(&model, &other, {}, true, service);
        QVERIFY(!runtime.state().available);
        QVERIFY(!runtime.setRunCore(true));
        QVERIFY(!runtime.setKeepRunning(true));
        QVERIFY(!runtime.setStartWithComputer(true));
        QString reason;
        QVERIFY(!runtime.prepareForRetirement(true, &reason));
        QVERIFY(!reason.isEmpty());
        QVERIFY(!runtime.backgroundStartWanted());
        QVERIFY(!runtime.controller()->enabled());
    }

    void busyAndRemoteModelsRefuseAllPageActions()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel local;
        GuiDesktopStationRuntime runtime(&local, &settings, AppSettings::profileOverride(), true, service);
        local.setConnectionStateForTest(ConnectionState::Connecting);
        QVERIFY(runtime.state().busy);
        QVERIFY(!runtime.setRunCore(true));
        QVERIFY(!runtime.setKeepRunning(true));
        QVERIFY(!runtime.setStartWithComputer(true));
        QVERIFY(!runtime.renameStation(QStringLiteral("KG4VCF")));
        QVERIFY(!runtime.revokeDevice(QByteArrayLiteral("invalid")));
        QVERIFY(!runtime.addDevice());
        QVERIFY(!runtime.acknowledgeKeyBackup());
        QVERIFY(!runtime.controller()->enabled());
        local.setConnectionStateForTest(ConnectionState::Disconnected);

        RadioModel remote(RadioModel::Role::Remote);
        GuiDesktopStationRuntime remoteRuntime(&remote, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(!remoteRuntime.state().available);
        QVERIFY(!remoteRuntime.setRunCore(true));
        QVERIFY(!remoteRuntime.addDevice());
    }

    void remoteSettingsBackendAndUnownedProfileRefuseActions()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime unowned(&model, &settings, AppSettings::profileOverride(), false, service);
        QVERIFY(!unowned.state().available);
        QVERIFY(!unowned.setRunCore(true));
        NeverBackend backend;
        settings.setRemoteBackend(&backend);
        {
            GuiDesktopStationRuntime proxied(&model, &settings, AppSettings::profileOverride(), true, service);
            QVERIFY(!proxied.state().available);
            QVERIFY(!proxied.setRunCore(true));
            QVERIFY(!proxied.renameStation(QStringLiteral("KG4VCF")));
            QVERIFY(!proxied.addDevice());
        }
        settings.setRemoteBackend(nullptr);
    }

    void staleActionsCannotBypassOnAirGate()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        model.transmitModel().setTune(true); // logical model state; no radio is connected.
        QVERIFY(model.isCoreOnAir());
        QVERIFY(!runtime.setKeepRunning(true));
        QVERIFY(!runtime.setStartWithComputer(true));
        QVERIFY(!runtime.renameStation(QStringLiteral("KG4VCF")));
        QVERIFY(!runtime.revokeDevice(QByteArrayLiteral("invalid")));
        QVERIFY(!runtime.addDevice());
        QVERIFY(!runtime.acknowledgeKeyBackup());
        QVERIFY(!runtime.setRunCore(false)); // stale Setup callback stays locked during TX.
        QVERIFY(runtime.controller()->enabled());
        QVERIFY(settings.value(QStringLiteral("DesktopCore/Run")).toBool());
        model.transmitModel().setTune(false);
        QVERIFY(runtime.setRunCore(false));
        QVERIFY(!runtime.controller()->enabled());
    }

    void startEntryUsesFakeRunnerAndPreservesExplicitConfig()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        QVERIFY(writeConfig(path, freePort()));
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray bytes = original.readAll();
        original.close();
        const QString binary = temp.filePath(QStringLiteral("nereusd"));
        QFile executable(binary);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("binary");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                           | QFileDevice::ExeOwner));
        StationServiceOptions service;
        service.platform = StationPlatform::MacOS;
        service.binaryPath = binary;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        QStringList commands;
        service.runner = [&commands](const QString& program, const QStringList& args) {
            commands << program + args.join(QLatin1Char(' '));
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setKeepRunning(true));
        QVERIFY(runtime.setStartWithComputer(true));
        QVERIFY(runtime.state().keepRunning);
        QVERIFY(runtime.state().startWithComputer);
        QVERIFY(QFileInfo::exists(temp.filePath(QStringLiteral(
            "home/Library/LaunchAgents/com.boydsoftprez.NereusSDR.station.plist"))));
        const StationServiceOptions later = runtime.backgroundServiceOptions();
        QVERIFY(!later.inheritActiveProfile);
        QCOMPARE(later.profileDirectory, QFileInfo(settings.filePath()).absolutePath());
        QCOMPARE(later.homeDirectory, service.homeDirectory);
        QVERIFY(runtime.setRunCore(false));
        QVERIFY(!runtime.state().startWithComputer);
        QVERIFY(!runtime.state().keepRunning);
        QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral(
            "home/Library/LaunchAgents/com.boydsoftprez.NereusSDR.station.plist"))));
        QCOMPARE(settings.value(QStringLiteral("DesktopCore/Run")).toBool(), false);
        QCOMPARE(settings.value(QStringLiteral("DesktopCore/KeepRunning")).toBool(), false);
        QFile preserved(path);
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), bytes);
        QVERIFY(commands.isEmpty());
    }

    void linuxStartupEntryFollowsFakeServiceResult()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        const QString binary = temp.filePath(QStringLiteral("nereusd"));
        QFile executable(binary);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("binary");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                           | QFileDevice::ExeOwner));
        bool enabled = false;
        QStringList calls;
        StationServiceOptions service;
        service.platform = StationPlatform::Linux;
        service.binaryPath = binary;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.userName = QStringLiteral("tester");
        service.inheritActiveProfile = false;
        service.runner = [&enabled, &calls](const QString& program, const QStringList& args) {
            calls << program + QLatin1Char(' ') + args.join(QLatin1Char(' '));
            if (program == QStringLiteral("systemctl")
                && args.contains(QStringLiteral("is-enabled"))) {
                return StationServiceCommandResult{enabled ? 0 : 1, {}};
            }
            if (program == QStringLiteral("systemctl")
                && args.contains(QStringLiteral("enable"))) { enabled = true; }
            if (program == QStringLiteral("systemctl")
                && args.contains(QStringLiteral("disable"))) { enabled = false; }
            if (program == QStringLiteral("loginctl")) {
                return StationServiceCommandResult{0, QStringLiteral("yes\n")};
            }
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setStartWithComputer(true));
        QVERIFY(runtime.state().startWithComputer);
        QVERIFY(enabled);
        QVERIFY(calls.join(QLatin1Char(' ')).contains(QStringLiteral("enable")));
        QVERIFY(runtime.setRunCore(false));
        QVERIFY(!enabled);
        QVERIFY(!runtime.state().startWithComputer);
        QVERIFY(calls.join(QLatin1Char(' ')).contains(QStringLiteral("disable")));
        QVERIFY(!calls.join(QLatin1Char(' ')).contains(QStringLiteral("start nereusd")));
    }

    void deviceActionsUseLiveFacadeAndPersist()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.addDevice());
        QVERIFY(runtime.state().pairingOpen);
        QVERIFY(!runtime.state().pairingCode.isEmpty());
        QVERIFY(runtime.renameStation(QStringLiteral("KG4VCF")));
        QCOMPARE(runtime.state().stationName, QStringLiteral("KG4VCF"));
        QCOMPARE(settings.value(QStringLiteral("StationLabel")).toString(),
                 QStringLiteral("KG4VCF"));
        QVERIFY(!runtime.state().keyBackupPath.isEmpty());
        QVERIFY(runtime.acknowledgeKeyBackup());
        QVERIFY(runtime.state().keyBackupAcknowledged);
        QVERIFY(!runtime.revokeDevice(QByteArrayLiteral("bad-id")));
        runtime.stop();
    }

    void revokeRemovesPairedDeviceThroughServerFacade()
    {
        QTemporaryDir temp;
        QTemporaryDir firstDir;
        QTemporaryDir secondDir;
        QVERIFY(temp.isValid() && firstDir.isValid() && secondDir.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        const StationIdentity firstKey = StationIdentity::loadOrCreate(firstDir.path());
        const StationIdentity secondKey = StationIdentity::loadOrCreate(secondDir.path());
        QVERIFY(firstKey.isValid() && secondKey.isValid());
        PairedDevice first;
        first.id = firstKey.fingerprint();
        first.publicKeySpki = firstKey.publicKeySpki();
        first.name = QStringLiteral("First phone");
        first.kind = QStringLiteral("phone");
        PairedDevice second;
        second.id = secondKey.fingerprint();
        second.publicKeySpki = secondKey.publicKeySpki();
        second.name = QStringLiteral("Second phone");
        second.kind = QStringLiteral("phone");
        DeviceStore* store = runtime.controller()->server()->deviceStore();
        QVERIFY(store->add(first));
        QVERIFY(store->add(second));
        QCOMPARE(runtime.state().devices.size(), 2);
        QVERIFY(runtime.state().devices.at(0).revocable);
        QSignalSpy removed(store, &DeviceStore::deviceRemoved);
        const QByteArray firstId = StationIdentity::toBase64Url(first.id).toLatin1();
        QVERIFY(runtime.revokeDevice(firstId));
        QCOMPARE(removed.size(), 1);
        QVERIFY(!store->find(first.id).has_value());
        QCOMPARE(runtime.state().devices.size(), 1);
        QVERIFY(!runtime.state().devices.at(0).revocable);
        QVERIFY(!runtime.revokeDevice(StationIdentity::toBase64Url(second.id).toLatin1()));
        QVERIFY(store->find(second.id).has_value());
        runtime.stop();
    }

    void explicitQuitAfterRunAndKeepLatchesOnlyBackgroundIntent()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        QList<QPair<QString, QStringList>> commands;
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [&commands](const QString& program, const QStringList& args) {
            commands.append({program, args});
            if (program == QStringLiteral("systemctl")
                && args.contains(QStringLiteral("is-enabled"))) {
                return StationServiceCommandResult{1, {}};
            }
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setKeepRunning(true));
        QString reason;
        QVERIFY2(runtime.prepareForRetirement(true, &reason), qPrintable(reason));
        QVERIFY(runtime.backgroundStartWanted());
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(!runtime.setRunCore(true));
        QVERIFY(!runtime.prepareForRetirement(true, &reason));
        QVERIFY(runtime.backgroundStartWanted());
        // The final service start belongs to the caller after unlock.
        // Linux may make only this read-only probe while constructing the runtime.
        for (const auto& command : commands) {
            QCOMPARE(command.first, QStringLiteral("systemctl"));
            QCOMPARE(command.second, (QStringList{QStringLiteral("--user"),
                                                  QStringLiteral("is-enabled"),
                                                  QStringLiteral("--quiet"),
                                                  QStringLiteral("nereusd.service")}));
        }
        runtime.stop();
        QVERIFY(runtime.backgroundStartWanted());
    }

    void invalidBackgroundConfigPreventsRetirementIntent()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        const QString path = serviceConfigPath(settings);
        QVERIFY(writeConfig(path, freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setKeepRunning(true));
        QFile config(path);
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
        config.write("remote_port = 47910\nstate_directory = /outside-profile\n");
        config.close();
        QString reason;
        QVERIFY(!runtime.prepareForRetirement(true, &reason));
        QVERIFY(reason.contains(QStringLiteral("profile"), Qt::CaseInsensitive));
        QVERIFY(!runtime.backgroundStartWanted());
        QVERIFY(!runtime.controller()->enabled());
    }

    void failedSettingsSaveCannotLatchBackgroundStart()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setKeepRunning(true));
        const QString settingsPath = settings.filePath();
        const QByteArray originalBytes = fileBytes(settingsPath);
        QVERIFY2(!originalBytes.isEmpty(), qPrintable(settingsPath));
        QVERIFY(blockSettingsSave(settingsPath));
        bool blocked = true;
        const auto restoreOnFailure = qScopeGuard([&] {
            if (blocked) { restoreSettingsFile(settingsPath); }
        });
        QString reason;
        QVERIFY(!runtime.prepareForRetirement(true, &reason));
        QVERIFY(!reason.isEmpty());
        QVERIFY(!runtime.backgroundStartWanted());
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(QFileInfo(settingsPath).isDir());
        QVERIFY(QFileInfo::exists(QDir(settingsPath).filePath(QStringLiteral("save-blocker"))));
        QCOMPARE(fileBytes(preservedSettingsPath(settingsPath)), originalBytes);
        QVERIFY(restoreSettingsFile(settingsPath));
        blocked = false;
        QCOMPARE(fileBytes(settingsPath), originalBytes);
        reason.clear();
        QVERIFY2(runtime.prepareForRetirement(true, &reason), qPrintable(reason));
        QVERIFY(runtime.backgroundStartWanted());
    }

    void retirementSavesConnectedChoiceOnlyAndNeverStartsOnReplacement()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        QVERIFY(runtime.setRunCore(true));
        QVERIFY(runtime.setKeepRunning(true));
        RadioInfo info;
        info.macAddress = QStringLiteral("02:00:00:00:00:91");
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        QString reason;
        QVERIFY2(runtime.prepareForRetirement(false, &reason), qPrintable(reason));
        QVERIFY(!runtime.backgroundStartWanted());
        QCOMPARE(settings.value(QStringLiteral("StationRadioChoice")).toString(),
                 info.macAddress);
        QVERIFY(!runtime.controller()->enabled());
        // No newly connected radio may replace the saved choice with empty.
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        GuiDesktopStationRuntime nextRun(&model, &settings, AppSettings::profileOverride(),
                                         true, service);
        QVERIFY2(nextRun.prepareForRetirement(false, &reason), qPrintable(reason));
        QCOMPARE(settings.value(QStringLiteral("StationRadioChoice")).toString(),
                 info.macAddress);
    }

    void runtimeBindsLatePageActionToActualListener()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppSettings& settings = AppSettings::instance();
        QVERIFY(writeConfig(serviceConfigPath(settings), freePort()));
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath(QStringLiteral("home"));
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        RadioModel model;
        GuiDesktopStationRuntime runtime(&model, &settings, AppSettings::profileOverride(), true, service);
        SetupDialog dialog(&model);
        runtime.bindSetupDialog(&dialog);
        dialog.selectPage(QStringLiteral("Remote Access"));
        auto* page = dialog.findChild<RemoteStationPage*>();
        QVERIFY(page != nullptr);
        QVERIFY(page->state().available);
        auto* check = page->findChild<QCheckBox*>(QStringLiteral("remoteAccessRunCore"));
        QVERIFY(check != nullptr);
        check->click();
        QVERIFY(runtime.controller()->enabled());
        QVERIFY(page->state().runCore);
        check->click();
        QVERIFY(!runtime.controller()->enabled());
        QVERIFY(!page->state().runCore);
    }

    void lazyBinderCoversNewAndExistingRemotePages()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        int calls = 0;
        dialog.setRemoteStationPageBinder([&calls](RemoteStationPage*) { ++calls; });
        dialog.selectPage(QStringLiteral("Remote Access"));
        QCOMPARE(calls, 1);
        QVERIFY(dialog.findChild<RemoteStationPage*>() != nullptr);
        dialog.setRemoteStationPageBinder([&calls](RemoteStationPage*) { ++calls; });
        QCOMPARE(calls, 2);
    }
};
QTEST_MAIN(TstGuiDesktopStationRuntime)
#include "tst_gui_desktop_station_runtime.moc"
