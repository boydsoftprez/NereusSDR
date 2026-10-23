// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_hosting_slice_letters.cpp  (NereusSDR)
// =================================================================
//
// JJ 2026-10-23: "the core headless and core inside window should be the
// same experience." A paired device that comes back after a Core restart
// gets the same slice letter, with that slice's saved settings, whether the
// Core restarted headless or inside a desktop window, in either direction.
// Inside a window, the Core keeps the device's letter and settings for it
// and makes its slice only when it signs in; the window's own slice takes
// a letter nobody's device has, binds a receiver and carries transmit.
//
// The logs this follows (Zephyrus G2E Core, 2026-10-22/23): at 22:25 the
// headless Core had JJ's window on slice A and saved its step and AGC-T
// AUTO there; the Core stopped without a leave; at 23:37 it came back
// inside the desktop window, the window took A, and the device was given
// a new slice B, so its A settings never came back to it.
//
// Every start here is the production one: a StationHost (with the window
// as its hosting device inside the window, as GuiDesktopStationRuntime
// starts it; without one headless, as DaemonApp does, after the receive
// layout is prepared) and then RadioModel::connectToRadio against the
// P1FakeRadio (ConnectableRadioModel), which sizes the stream pool, binds
// the slices and loads the window slice's state. Every stop is the
// production one, with the devices still on the Core (no session.leave):
// inside the window, GuiDesktopStationRuntime::prepareForRetirement's
// steps (stopAllTx, the host's stop, saveForStationHandover); headless,
// DaemonApp's release (saveForStationHandover, then the host's stop).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-23: PR 627 fix wave: driven through StationHost and the real
//               connect and shutdown; the window keeps letters instead of
//               holding slices. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-23: PR 627 round three: kept letters across a radio switch,
//               a Run Core off/on cycle and admission step 2. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "UnusedLoopbackPort.h"
#include "core/ReceiveLayoutStore.h"
#include "core/StationBinaryLocator.h"
#include "core/station/StationHost.h"
#include "fakes/ConnectableRadioModel.h"
#include "gui/DesktopStationController.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "gui/StationServiceManager.h"

#include <QSslSocket>

#include <algorithm>
#include <functional>
#include <vector>

using NereusSDR::Test::ConnectableRadioModel;

namespace {

// ConnectableRadioModel's radio.
const QString kMac = QStringLiteral("aa:bb:cc:11:22:33");
constexpr int kDeviceStepHz = 1000;
constexpr double kDeviceFrequencyHz = 7074000.0;
// Another radio the window connects to, with its own layout and settings.
const QString kOtherMac = QStringLiteral("aa:bb:cc:44:55:66");
constexpr int kOtherLetter = 3;
constexpr int kOtherStepHz = 250;
constexpr double kOtherFrequencyHz = 14074000.0;

enum class Hosting { Headless, Window, WindowRunCoreOff };

// One Core run: its host is stopped before its radio model goes.
struct Station {
    std::unique_ptr<ConnectableRadioModel> radio;
    // Headless: DaemonApp's StationHost. Inside the window: the window's
    // GuiDesktopStationRuntime, whose controller runs the StationHost.
    std::unique_ptr<StationHost> host;
    std::unique_ptr<GuiDesktopStationRuntime> runtime;
    std::vector<std::unique_ptr<LoopbackTransport>> apps;
    Hosting hosting = Hosting::Headless;
    // Set before the connect, while `radio` is still being made.
    RadioModel* radioModel = nullptr;

    RadioModel& model() { return *radioModel; }
    StationHost* activeHost() const
    {
        if (runtime) {
            return runtime->controller() ? runtime->controller()->host() : nullptr;
        }
        return host.get();
    }
    StationServer* server() const
    {
        StationHost* const current = activeHost();
        return current ? current->server() : nullptr;
    }

    ~Station()
    {
        runtime.reset();
        host.reset();
        radio.reset();
    }
};

class Sandbox {
public:
    Sandbox()
        : m_profileDirectory(QFileInfo(AppSettings::instance().filePath()).absolutePath())
    {
        // The window's Core keeps its identity and paired devices in the
        // profile directory; the headless Core here uses the same one, as
        // the two share a profile on JJ's computer. Each case starts with
        // no paired devices.
        QFile::remove(QDir(m_profileDirectory).filePath(QLatin1String(DeviceStore::kFileName)));
        m_security = NereusSDR::Test::withSharedTlsIdentity(
            NereusSDR::Test::seedCoreIdentity(m_profileDirectory));
    }

    StationHostOptions headlessOptions() const
    {
        StationHostOptions options;
        options.settings = &AppSettings::instance();
        options.securityDirectory = m_security;
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.remotePort = unusedLoopbackPort();
        options.statusPage = false;
        options.rendezvousServers.clear();
        return options;
    }

    // The window's runtime as GuiSessionCoordinator::installDesktopStation
    // makes it, with the Core's configuration on a free loopback port.
    std::unique_ptr<GuiDesktopStationRuntime> makeRuntime(RadioModel& model, bool runCore)
    {
        AppSettings& settings = AppSettings::instance();
        QFile config(QDir(m_profileDirectory).filePath(QStringLiteral("station.conf")));
        if (!config.open(QIODevice::WriteOnly)
            || config.write(QStringLiteral("remote_port = %1\nremote_bind = 127.0.0.1\n"
                                           "status_page = off\nrendezvous_servers =\n")
                                .arg(unusedLoopbackPort())
                                .toUtf8())
                <= 0) {
            return nullptr;
        }
        config.close();
        settings.setValue(QStringLiteral("DesktopCore/Run"),
                          runCore ? QStringLiteral("True") : QStringLiteral("False"));
        StationServiceOptions service;
        service.platform = StationPlatform::MacOS;
        service.profileDirectory = m_profileDirectory;
        service.homeDirectory = m_home.path();
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        return std::make_unique<GuiDesktopStationRuntime>(
            &model, &settings, AppSettings::profileOverride(), true, service);
    }

    // `beforeConnect` runs once the host is up and before the radio connects.
    std::unique_ptr<Station> start(Hosting hosting, HPSDRHW board = HPSDRHW::HermesLite,
                                   const std::function<void(Station&)>& beforeConnect = {})
    {
        auto station = std::make_unique<Station>();
        Station* const raw = station.get();
        raw->hosting = hosting;
        bool hostStarted = true;
        raw->radio = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local,
            [this, raw, hosting, &beforeConnect, &hostStarted](RadioModel& model) {
                raw->radioModel = &model;
                if (hosting == Hosting::Headless) {
                    // DaemonApp::start: the layout first, then the
                    // configured slice when the layout made none.
                    model.prepareReceiveLayout(kMac);
                    if (model.slices().isEmpty()) {
                        model.addSlice(QStringLiteral("pan-0"));
                    }
                    raw->host = std::make_unique<StationHost>(&model, headlessOptions());
                    hostStarted = raw->host->start();
                } else {
                    // GuiSessionCoordinator: the runtime's restore() before
                    // the window's first connect.
                    raw->runtime = makeRuntime(model, hosting == Hosting::Window);
                    hostStarted = raw->runtime && raw->runtime->restore()
                        && (hosting == Hosting::WindowRunCoreOff || raw->server() != nullptr);
                }
                if (beforeConnect) {
                    beforeConnect(*raw);
                }
            },
            board);
        if (!raw->radio || !hostStarted) {
            return nullptr;
        }
        if (hosting == Hosting::Headless) {
            // DaemonApp::onRadioStateForRecovery on Connected.
            raw->model().completeReceiveLayoutStartup();
        }
        QCoreApplication::processEvents();
        return station;
    }

    // Run Core turned on in a window that already has its slices.
    static bool turnOnRunCore(Station& station)
    {
        station.hosting = Hosting::Window;
        return station.runtime->setRunCore(true) && station.server() != nullptr;
    }

    // The Core stops with its devices still on it: the window's retirement
    // (GuiDesktopStationRuntime::prepareForRetirement), or DaemonApp's
    // release (saveForStationHandover, then the host's stop).
    static void stop(std::unique_ptr<Station>& station)
    {
        QString error;
        if (station->hosting == Hosting::Headless) {
            QVERIFY2(station->model().saveForStationHandover(&error), qPrintable(error));
            station->host->stop();
        } else {
            QVERIFY2(station->runtime->prepareForRetirement(false, &error), qPrintable(error));
        }
        station.reset();
        RadioDiscovery::clearHoldOffForTest();
    }

    void pair(Station& station, const Device& device)
    {
        QVERIFY(station.server());
        // The store is the profile directory's: kept across restarts.
        if (!station.server()->deviceStore()->find(device.key.fingerprint())) {
            QVERIFY(station.server()->deviceStore()->add(device.record()));
        }
    }

    static LoopbackTransport* signIn(Station& station, const Device& device,
                                     const QHash<QByteArray, int>& features = kHolder)
    {
        StationServer* server = station.server();
        auto app = std::make_unique<LoopbackTransport>(QStringLiteral("app"));
        auto* peer = new LoopbackTransport(QStringLiteral("station"));
        peer->setPeerAddress(QStringLiteral("192.0.2.7"));
        peer->linkTo(app.get());
        LoopbackTransport* const client = app.get();
        station.apps.push_back(std::move(app));
        server->acceptTransport(peer);
        if (!QTest::qWaitFor([client]() { return !client->received().isEmpty(); }, 5000)) {
            qWarning("signIn: the Core sent no hello");
            return client;
        }
        const QJsonObject hello = firstOfType(client->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        const SessionMessage auth = SessionMessages::authRequest(
            QString(), device.block(challenge, QByteArray::fromHex(pin.toLatin1()),
                                    server->stationIdentity().publicKeySpki()));
        Core::send(client, auth, kSessionProtocolMinor, features);
        return client;
    }

private:
    QString m_profileDirectory;
    QString m_security;
    QTemporaryDir m_home;
};

QList<int> ownedBy(Station& station, const Device& device)
{
    return station.model().sliceOwnership()->ownedBy(device.key.fingerprint());
}

// The device's settings on its slice, as JJ set them.
void setDeviceSettings(Station& station, int sliceId, int stepHz = kDeviceStepHz)
{
    SliceModel* slice = station.model().sliceById(sliceId);
    QVERIFY(slice);
    slice->setFrequency(kDeviceFrequencyHz);
    slice->setStepHz(stepHz);
    slice->setAutoAgcEnabled(true);
}

void verifyDeviceSettings(Station& station, int sliceId, int stepHz = kDeviceStepHz)
{
    const SliceModel* slice = station.model().sliceById(sliceId);
    QVERIFY(slice);
    QCOMPARE(slice->frequency(), kDeviceFrequencyHz);
    QCOMPARE(slice->stepHz(), stepHz);
    QVERIFY(slice->autoAgcEnabled());
}

// The window's own slice: the station device owns it for nobody, it has a
// receiver, it carries transmit, and it is the window's active slice.
void verifyWindowSlice(Station& station, int sliceId)
{
    RadioModel& model = station.model();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice);
    const SliceOwnership::Mark mark = model.sliceOwnership()->mark(sliceId);
    QCOMPARE(mark.owner, SliceOwnership::stationDevice());
    QVERIFY(mark.heldFor.isEmpty());
    QVERIFY(slice->streamIndex() >= 0);
    QCOMPARE(model.txBoundSlice(), slice);
    QCOMPARE(model.activeSlice(), slice);
}

// The other radio's layout: the device on Slice D there, with that letter's
// settings saved for that radio.
void seedOtherRadio(const QByteArray& device)
{
    SliceModel slice(kOtherLetter);
    slice.setSettingsRadioIdentity(kOtherMac);
    slice.setFrequency(kOtherFrequencyHz);
    slice.setStepHz(kOtherStepHz);
    slice.setAutoAgcEnabled(true);
    slice.saveToSettings(slice.band());
    ReceiveSliceState kept;
    kept.id = kOtherLetter;
    kept.panKey = QStringLiteral("pan-0");
    kept.frequencyHz = kOtherFrequencyHz;
    kept.dspMode = DSPMode::USB;
    kept.owner = device;
    QString reason;
    QVERIFY2(ReceiveLayoutStore::stage(AppSettings::instance(), kOtherMac, {kept}, &reason),
             qPrintable(reason));
}

bool layoutKeeps(const QString& radioMac, const Device& device, int letter)
{
    const ReceiveLayoutStore::LoadResult layout =
        ReceiveLayoutStore::load(AppSettings::instance(), radioMac);
    return std::any_of(layout.slices.cbegin(), layout.slices.cend(),
                       [&device, letter](const ReceiveSliceState& entry) {
                           return entry.owner == device.key.fingerprint() && entry.id == letter;
                       });
}

QList<int> sliceIds(Station& station)
{
    QList<int> ids;
    for (const SliceModel* slice : station.model().slices()) {
        ids.append(slice->sliceIndex());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

class TestStationHostingSliceLetters : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // 22:25 headless, 23:37 inside the window: the device keeps A with its
    // step and AUTO, and the window takes B. Nothing of A runs until the
    // device signs in.
    void headlessThenInsideTheWindowKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{1});
        verifyWindowSlice(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0);
        QCOMPARE(sliceIds(*core), (QList<int>{0, 1}));
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        verifyWindowSlice(*core, 1);
    }

    // Inside the window the device is given B; after a headless restart B
    // is still its letter, with its settings.
    void insideTheWindowThenHeadlessKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 0);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(core->model().sliceById(1));
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        verifyDeviceSettings(*core, 1);
    }

    // The whole run from the logs: headless (22:25), inside the window
    // (23:37), the window again after a quit without a leave (07:33),
    // headless again (07:57). The device is on A throughout.
    void theSequenceFromTheLogsKeepsADeviceOnItsLetterThroughEveryRestart()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0);
        setDeviceSettings(*core, 0, 500);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0, 500);
        setDeviceSettings(*core, 0, 250);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0, 250);
    }

    // Headless to headless, as before: the letter and settings come back.
    void headlessThenHeadlessKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        verifyDeviceSettings(*core, 1);
    }

    // A device that signs in to the window's Core before its radio has
    // connected waits for the radio, then gets its own letter; the window
    // still gets a letter of its own and the other device's stays kept.
    void aDeviceAdmittedBeforeTheRadioConnectsGetsItsLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        bool admittedEarly = false;
        core = sandbox.start(Hosting::Window, HPSDRHW::HermesLite, [&](Station& station) {
            QVERIFY(!station.model().isConnected());
            admittedEarly = admitted(Sandbox::signIn(station, iphone));
            QVERIFY(ownedBy(station, iphone).isEmpty());
        });
        QVERIFY(core);
        QVERIFY(admittedEarly);
        verifyWindowSlice(*core, 2);
        QTRY_COMPARE(ownedBy(*core, iphone), QList<int>{1});
        verifyDeviceSettings(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyWindowSlice(*core, 2);
    }

    // An older window (no sessionHolder) needs its slice at admission; one
    // that signs in before the radio connects is placed at once, as before,
    // not refused (CI, tst_core_parity, 2026-10-23).
    void anOlderWindowAdmittedBeforeTheRadioConnectsIsPlacedAtOnce()
    {
        Sandbox sandbox;
        Device window(QStringLiteral("Older window"), QStringLiteral("computer"));
        const QHash<QByteArray, int> olderWindow{{"deviceAuth", 1}};

        bool admittedEarly = false;
        bool ownedEarly = false;
        auto core = sandbox.start(Hosting::Window, HPSDRHW::HermesLite, [&](Station& station) {
            sandbox.pair(station, window);
            admittedEarly = admitted(Sandbox::signIn(station, window, olderWindow));
            ownedEarly = !ownedBy(station, window).isEmpty();
        });
        QVERIFY(core);
        QVERIFY(admittedEarly);
        QVERIFY(ownedEarly);
    }

    // The saved layout keeps every letter the radio has (HermesII, two):
    // the window still takes one, the last, and the device whose letter it
    // took gets no slice while none is free; its slice and settings go to
    // its saved slices.
    void aFullLayoutLeavesTheWindowTheLastLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless, HPSDRHW::HermesII);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window, HPSDRHW::HermesII);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{1});
        verifyWindowSlice(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QVERIFY(ownedBy(*core, iphone).isEmpty());
        const QList<SavedSlice> kept =
            DeviceLayoutStore::load(AppSettings::instance(), kMac, iphone.key.fingerprint());
        QCOMPARE(kept.size(), 1);
        QCOMPARE(kept.first().id, 1);
        QCOMPARE(kept.first().frequencyHz, kDeviceFrequencyHz);
        QVERIFY(!kept.first().settings.isEmpty());
    }

    // Run Core turned on after the radio connected, as in real use: the
    // window has had A all along and saved its own settings under A. The
    // device's A is not given to it (the window has it); it gets B, at its
    // layout's frequency, with its own settings from A as the connect found
    // them, never the window's (JJ's ruling on the open behavior).
    void runCoreTurnedOnLaterGivesTheDeviceAFreeLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::WindowRunCoreOff);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{0});
        SliceModel* windowSlice = core->model().sliceById(0);
        windowSlice->setStepHz(50);
        core->model().flushPendingSettingsSave();

        QVERIFY(Sandbox::turnOnRunCore(*core));
        QVERIFY(core->model().hostsStationDevice());
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        QCOMPARE(sliceIds(*core), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        verifyDeviceSettings(*core, 1);
        QCOMPARE(windowSlice->stepHz(), 50);
        QCOMPARE(core->model().txBoundSlice(), windowSlice);
    }

    // Run Core turned on while the radio is disconnected and the window's
    // slices are still there: the device letters are kept from the radio
    // the window last had, not dropped at the next save.
    void runCoreTurnedOnWhileDisconnectedKeepsTheDevicesLetters()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::WindowRunCoreOff);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{0});
        core->model().disconnectFromRadio();
        QTRY_VERIFY(!core->model().isConnected());
        QCOMPARE(sliceIds(*core), QList<int>{0});

        QVERIFY(Sandbox::turnOnRunCore(*core));
        QVERIFY(core->model().hostsStationDevice());
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{1});
    }

    // Run Core turned on while disconnected keeps the last radio's letters;
    // the window then connects to another radio. The device gets that
    // radio's letter and settings, and the last radio's kept letter never
    // reaches that radio's layout (controller ruling 3).
    void anotherRadioAfterRunCoreTurnedOnWhileDisconnectedUsesItsOwnLetters()
    {
        anotherRadioUsesItsOwnLetters(false);
    }

    // The same when the window, already hosting, switches radios.
    void switchingRadiosWhileHostingUsesTheNewRadiosLetters()
    {
        anotherRadioUsesItsOwnLetters(true);
    }

    // A device placed on its kept letter changes its settings there. Run
    // Core turned off and on again, with its slice closed in between, gives
    // it its current settings, never the connect's older snapshot
    // (controller ruling 3).
    void runCoreOffAndOnKeepsTheDevicesCurrentSettings()
    {
        Sandbox sandbox;
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{0});
        verifyDeviceSettings(*core, 0);
        setDeviceSettings(*core, 0, kOtherStepHz);
        core->model().requestSettingsSave();
        core->model().flushPendingSettingsSave();

        // Run Core off keeps the iPhone's slice in the window, held for it,
        // and its letter in the layout. The window then closes that slice,
        // so Run Core on keeps the letter again from the layout.
        QVERIFY(core->runtime->setRunCore(false));
        core->model().removeSlice(0);
        QCOMPARE(sliceIds(*core), QList<int>{1});
        QVERIFY(Sandbox::turnOnRunCore(*core));
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{0});
        verifyDeviceSettings(*core, 0, kOtherStepHz);
    }

    // Admission step 2: a device's saved slice on a letter the window's
    // Core keeps for another device goes to a free letter, and the other
    // device still gets its letter (controller ruling 3).
    void aSavedSliceNeverTakesALetterKeptForAnotherDevice()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        // The Mac's own saved slice, also on A, from an earlier session. It
        // is inside the window's receiver (B at 14.225 MHz), so the
        // iPhone's 7.074 MHz still has a receiver of its own.
        constexpr double kSavedFrequencyHz = 14200000.0;
        QVERIFY(DeviceLayoutStore::append(
            AppSettings::instance(), kMac, mac.key.fingerprint(),
            SavedSlice{0, QStringLiteral("pan-0"), kSavedFrequencyHz, DSPMode::USB, {}},
            WdspEngine::kMaxSliceChannels));

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{2});
        QCOMPARE(core->model().sliceById(2)->frequency(), kSavedFrequencyHz);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{0});
        verifyDeviceSettings(*core, 0);
    }

    // HermesII (two letters), the Mac on A and the iPhone on B: the window
    // takes B. The iPhone does not sign in while the window writes B; its
    // B settings stay its own and come back to it on the next headless
    // start, never the window's.
    void theWindowsLetterNeverSavesItsSettingsAsADevices()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless, HPSDRHW::HermesII);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window, HPSDRHW::HermesII);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        core->model().sliceById(1)->setStepHz(50);
        core->model().requestSettingsSave();
        core->model().flushPendingSettingsSave();
        Sandbox::stop(core);

        const ReceiveLayoutStore::LoadResult layout =
            ReceiveLayoutStore::load(AppSettings::instance(), kMac);
        for (const ReceiveSliceState& entry : layout.slices) {
            QVERIFY2(entry.owner != iphone.key.fingerprint() || entry.id != 1,
                     "the layout names the window's letter as the iPhone's");
        }

        core = sandbox.start(Hosting::Headless, HPSDRHW::HermesII);
        QVERIFY(core);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        verifyDeviceSettings(*core, 1);
    }

    // Run Core turned off: the model no longer hosts, keeps no letters, and
    // the layout keeps the device's letter for the next start.
    void stoppingTheHostEndsHostingAndKeepsTheLayout()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        QVERIFY(core->model().hostsStationDevice());
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});
        QVERIFY(core->runtime->setRunCore(false));
        QVERIFY(!core->model().hostsStationDevice());
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        core->model().requestSettingsSave();
        core->model().flushPendingSettingsSave();
        const ReceiveLayoutStore::LoadResult layout =
            ReceiveLayoutStore::load(AppSettings::instance(), kMac);
        QCOMPARE(layout.state, ReceiveLayoutStore::LoadState::Loaded);
        QCOMPARE(layout.slices.size(), 1);
        QCOMPARE(layout.slices.first().id, 0);
        QCOMPARE(layout.slices.first().owner, mac.key.fingerprint());
    }

    // A headless layout's entries no paired device has (an unowned slice)
    // come through a run inside the window unchanged.
    void insideTheWindowKeepsTheHeadlessUnownedEntries()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        const int unowned = core->model().addSlice(QStringLiteral("pan-0"));
        QCOMPARE(unowned, 1);
        core->model().sliceOwnership()->setOwner(unowned, QByteArray());
        core->model().sliceById(unowned)->setFrequency(3573000.0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        Sandbox::stop(core);

        const ReceiveLayoutStore::LoadResult layout =
            ReceiveLayoutStore::load(AppSettings::instance(), kMac);
        QCOMPARE(layout.state, ReceiveLayoutStore::LoadState::Loaded);
        QCOMPARE(layout.slices.size(), 2);
        QCOMPARE(layout.slices.at(0).id, 0);
        QCOMPARE(layout.slices.at(0).owner, mac.key.fingerprint());
        QCOMPARE(layout.slices.at(1).id, 1);
        QVERIFY(layout.slices.at(1).owner.isEmpty());
        QCOMPARE(layout.slices.at(1).frequencyHz, 3573000.0);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), (QList<int>{0, 1}));
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(ownedBy(*core, mac).contains(0));
        verifyDeviceSettings(*core, 0);
    }
private:
    // The Mac on A and the iPhone on B of the first radio; the iPhone on D
    // of the other. The window starts on the first radio, Run Core on
    // (hostingAtConnect) or turned on after a disconnect, then connects to
    // the other radio.
    void anotherRadioUsesItsOwnLetters(bool hostingAtConnect)
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        seedOtherRadio(iphone.key.fingerprint());
        if (QTest::currentTestFailed()) {
            return;
        }

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(hostingAtConnect ? Hosting::Window : Hosting::WindowRunCoreOff);
        QVERIFY(core);
        core->model().disconnectFromRadio();
        QTRY_VERIFY(!core->model().isConnected());
        if (!hostingAtConnect) {
            QVERIFY(Sandbox::turnOnRunCore(*core));
        }
        QCOMPARE(core->model().reservedSliceLetters(),
                 hostingAtConnect ? (QList<int>{0, 1}) : QList<int>{1});

        RadioDiscovery::clearHoldOffForTest();
        RadioInfo other = core->radio->radioInfo();
        other.macAddress = kOtherMac;
        core->model().connectToRadio(other);
        QTRY_COMPARE_WITH_TIMEOUT(core->model().connectionState(), ConnectionState::Connected,
                                  10000);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{kOtherLetter});

        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{kOtherLetter});
        const SliceModel* slice = core->model().sliceById(kOtherLetter);
        QVERIFY(slice);
        QCOMPARE(slice->frequency(), kOtherFrequencyHz);
        QCOMPARE(slice->stepHz(), kOtherStepHz);
        Sandbox::stop(core);

        QVERIFY2(!layoutKeeps(kOtherMac, iphone, 1),
                 "the other radio's layout has the first radio's kept letter");
        QVERIFY(layoutKeeps(kOtherMac, iphone, kOtherLetter));
        QVERIFY(layoutKeeps(kMac, iphone, 1));
    }
};

QTEST_MAIN(TestStationHostingSliceLetters)
#include "tst_station_hosting_slice_letters.moc"
