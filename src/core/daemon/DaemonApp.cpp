// =================================================================
// src/core/daemon/DaemonApp.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonApp.h for the design
// rationale (R1 Task 10).
//
// Modification history (NereusSDR):
//   2026-09-20: relay RadioModel connection state without dereferencing a
//               RadioModel being destroyed, by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via OpenAI Codex.
//   2026-09-22: R-R3-36 Task 5: the daemon's RadioModel never demands PC
//               microphone capture, so nereusd never starts the capture
//               helper, by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-23: display load governor inputs follow thread placement, a
//               step is accepted only once published, and a computed
//               ceiling reaches only apps that know the budget reason
//               (R-R3-08, R-R3-37, R-R3-40), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: the Core's attenuator range is the one the local RX applet
//               uses (BoardCapsTable::stepAttMaxDb, so a board with Alex
//               reaches 61 dB), and its controller saves a change shortly
//               after it is made and sends a band's restored attenuation
//               to the radio (R-R3-46, R-R3-11), by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: the step attenuator uses RadioModel's feed (slice A's
//               receive band, as Thetis rx1_band) instead of a copy of it
//               (R-R3-46, R-R3-11), by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-44: nereusd publishes no VAX devices on the Core
//               host (VAX belongs to the remote window's computer), by
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-24: R-R3-48: the Core runs its own station TCI server
//               (station_tci_bind), by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-24: R-R3-22 / R-R3-47: every station listener on the station
//               network (station_bind, RadioModel::setStationBind), by
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-24: iPhone app Task 4 (R-IOS-01): the station advertises the
//               link majors setLinkMajors() names (a debug build's
//               --test-link-majors), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: R-R3-39 / R-IOS-03: the Core creates and runs the desktop's
//               TX analyzer (display 5, which the TX siphon feeds), so a
//               keyed Core no longer dereferences a missing display, after
//               Thetis ChannelMaster cmaster.c:192-198 [v2.10.3.15], by
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-24: iPhone app Task 12 (R-IOS-08): an empty remote_bind
//               listens on every interface, IPv4 and IPv6; the pairing
//               token is no longer created; pairing_lan_click reaches the
//               station server. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-24: iPhone app Task 14 (R-IOS-08): the pairing code is printed
//               on the Core's console. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17 (R-IOS-08): the status page, the first
//               start's label and page address, and the console socket.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: the status page binds with the listener's dual-stack rule
//               (iPhone app Task 17, R-IOS-08, R-R3-26). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): remote_transmit sets the
//               station transmit gate and the receive-only policy. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the announcement and the
//               Bonjour record carry how many devices hold a place. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-25: iPhone app Task 76 (R-IOS-31): a media controller and a
//               telemetry controller per admitted session (DaemonMediaHub),
//               the governor fed every controller's charge and running
//               while any media session is live. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08, R-IOS-16): the Core
//               registers with the remote access service, holds a nameplate
//               while its pairing window is open, and pairs through the
//               service's mailbox (startRendezvous()). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 19 (R-IOS-25): the Core starts the station's
//               spot sources whose Auto-Connect or Auto-Start is on, with
//               no window. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 21 (R-IOS-18): the Core's choice of radio
//               (StationRadios::choose: a radio chosen from an app, then
//               radio_mac, then the one radio in sight, else wait; never
//               the first found); station.selectRadio restarts the run on
//               the chosen radio; station.rescanRadios scans while
//               connected. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 28 (R-R3-49, A11): the TX analyzer's MOX start
//               and stop move into RadioModel's TxDisplayFeed. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-R3-49: the config file's sample_rate_hz is a starting
//               value only; a rate already saved for the radio wins at start
//               (applyConfigToSettings). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the server told
//               nereusd.conf's `relay` (relayAllowed). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: R-R3-49 (remote-window parity Task 22): nereusd.conf's path
//               to the station server, for the Core's support bundle. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Fix wave LINK minor 3 (TX path): stop() stops all
//               transmit before it tears down the listener and the
//               radio. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: TX safety: the radio retire-and-retry uses RadioModel's
//               recovery retire, so a lost-link key lock holds until the
//               rebuilt link is Connected. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: a late failure of the radio a change left behind no longer
//               ends the change chosen after it (m_radioChangeRestartPending):
//               the change's deadline and the connect watchdog are both
//               2000 ms. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: the check moves into endRadioSwitch, so a Connected, a
//               discovery with no radio to choose or a connect that did
//               not start cannot end a change before its restart runs
//               either. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: the restart for a radio change stops a deadline the run
//               it restarts started, so a discovery completion queued
//               ahead of the restart cannot end the new change early
//               (restartForRadioChange). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: a radio change ends when the run serving it stops
//               (stopRadioRecovery: stop(), a station release, the
//               operator's disconnect), when its restart starts a run with
//               no discovery, and a restart that fails drops the pending
//               choice. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: a run stopped between a radio change's choice and its
//               restart cancels that restart (stopRadioRecovery), so a
//               finished stop or station release stays finished; the
//               chooser is answered refused and the choice is dropped.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: radioChangeStoppedReason's words match the Core's other
//               radio change message. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/daemon/DaemonApp.h"
#include "core/station/StationRadios.h"
#include "core/station/StationHost.h"
#include "core/daemon/DaemonAgcSource.h"
#include "core/daemon/DaemonTelemetryController.h"
#include "core/daemon/HostTelemetrySampler.h"
#include "models/ReceiverDspLoadSampler.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CoreInit.h"
#include "core/FFTRouter.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorController.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/TxSliceArbiter.h"
#include "core/WdspEngine.h"
#include "core/daemon/StationControlCommands.h"
#include "core/daemon/StationControlSocket.h"
#include "core/session/StationServer.h"
#include "core/session/StationLanAnnouncer.h"
#include "core/session/RendezvousClient.h"
#include "core/session/StationRendezvous.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/media/DaemonMediaController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

DaemonApp::DaemonApp(QObject* parent)
    : QObject(parent)
{
    m_radioRetryTimer = new QTimer(this);
    m_radioRetryTimer->setSingleShot(true);
    connect(m_radioRetryTimer, &QTimer::timeout,
            this, &DaemonApp::attemptRadioDiscovery);
    m_radioSwitchDeadline = new QTimer(this);
    m_radioSwitchDeadline->setSingleShot(true);
    connect(m_radioSwitchDeadline, &QTimer::timeout, this, &DaemonApp::endRadioSwitch);
}

DaemonApp::~DaemonApp()
{
    stop();
}

bool DaemonApp::start(const DaemonConfig& cfg)
{
    if ((cfg.displayApplicationBytesPerSecond || cfg.spectrumSampleUnitsPerSecond)
        && !cfg.displayBudgetLimits()) {
        QString configurationError;
        cfg.validate(&configurationError);
        qCWarning(lcApp) << "DaemonApp: invalid configuration:" << configurationError;
        return false;
    }
    // Defensive: no test calls start() twice without an intervening
    // stop(), but leaking the previous RadioModel (socket, WdspEngine,
    // discovery timers, ...) would be a silent resource leak the moment
    // one does. Tearing down first makes a second start() behave exactly
    // like stop() + start().
    if (m_radioModel) {
        stop();
    }

    if (m_radioConnectInProgress) {
        return false; // stop is deferred until the nested connection setup unwinds.
    }
    m_stopDeferred = false;
    m_radioConfig = cfg;
    // Parity Task 21 (R-IOS-18): the choice order's first two steps, a radio
    // chosen from an app, then radio_mac; empty leaves the third (the one
    // radio in sight) to the first scan (StationRadios::choose).
    ensureStationRadios();
    {
        // Fix wave, I6: this run's pending choice (a radio change in this
        // process), then the last choice that connected, then radio_mac. A
        // new nereusd has no pending choice, so one that never connected is
        // never reloaded.
        const QString pending = m_stationRadios->pendingChoice();
        const QString saved = m_stationRadios->savedChoice();
        m_selectedRadioMac = !pending.isEmpty() ? pending
            : !saved.isEmpty()                 ? saved
                                               : cfg.radioMac;
        m_stationRadios->setTarget(m_selectedRadioMac);
    }
    m_radioAttempted = false;
    m_radioConnectedBefore = false;
    m_radioRetryNextMs = m_radioRetryInitialMs;

    // Idempotent process-wide (CoreInit.cpp's s_initialized guard), so
    // this is a genuine no-op when server_main.cpp already called it
    // before constructing this DaemonApp. Calling it here too means a
    // caller that constructs a DaemonApp directly -- every test in
    // tst_daemon_app.cpp -- does not have to replay that bootstrap step
    // itself.
    NereusSDR::CoreInit::initialize();

    m_radioModel = std::make_unique<RadioModel>();
    // R-R3-36: nereusd never opens a microphone. Set before anything below
    // can connect the model, so no local-session capture demand is ever
    // taken and the capture helper is never started.
    m_radioModel->setPcCaptureAllowed(false);
    // Fix wave, M3: a key that arrives while the Core changes its radio is
    // refused.
    m_radioModel->setStationRadioChangeUnderway(m_stationRadios->switching());
    m_radioModel->setStationRadioWaiting(m_stationRadios->waitingReason());
    // R-R3-44: nor does it publish VAX devices. A remote window's VAX
    // channels are on the operator's computer; outputs here would be
    // devices nothing on the Core host feeds.
    m_radioModel->audioEngine()->setVaxOutputsAllowed(false);
#ifdef NEREUS_BUILD_TESTS
    m_radioModel->wdspEngine()->setSynchronousInitForTest(m_synchronousWdspForTest);
    if (m_radioInitializerForTest) {
        m_radioInitializerForTest(m_radioModel.get());
    }
#endif
    const quint64 runGeneration = m_radioRecoveryGeneration;
    connect(m_radioModel.get(), &RadioModel::connectionStateChanged, this,
            [this, runGeneration](ConnectionState state) {
        if (runGeneration == m_radioRunGeneration) {
            onRadioStateForRecovery(state);
        }
    }, Qt::QueuedConnection);
    m_radioRunGeneration = runGeneration;
    connect(m_radioModel.get(), &RadioModel::radioDisconnectRequested, this, [this]() {
        if (!m_retiringRadio) {
            stopRadioRecovery();
        }
    });
    // iPhone app plan Task 34: the station-side receive-only policy is the
    // config's remote_transmit = deny (allow by default: the station
    // transmit gate then decides per session). Installed before
    // controllers, peripherals, slices, or the radio can produce a
    // callback: the daemon owns real hardware even though its model has the
    // normal local role.
    m_radioModel->setReceiveOnlyStationPolicy(!cfg.remoteTransmitAllowed);
    createTxAnalyzer();
    // R-R3-22 / R-R3-47: every station listener (4992, the Power Genius and
    // Tuner Genius discovery, the station TCI server) accepts connections on
    // the station network only: nereusd.conf's station_bind, else the
    // radio's subnet. Set before any of them exists or starts.
    m_radioModel->setStationBind(cfg.stationBind);
    m_radioModel->enableStationAccessoryIdentity();
    // R-R3-48: the Core's own TCI server on the station network, switched
    // by the app's one TCI switch (setStationTci) and kept in the Core's
    // settings. Receive-only until remote transmit (StationTciController).
    m_radioModel->enableStationTci(cfg.stationBind);
    m_stepAttController = std::make_unique<StepAttenuatorController>();
    // R-R3-46 / R-R3-11: a change from a remote window is on disk shortly
    // after the Core applies it, not only when the Core stops.
    m_stepAttController->setDebouncedSaveEnabled(true);
    // R-R3-46: a band change on the Core sends that band's attenuation and
    // preamp to the radio, so the radio runs what every window shows.
    m_stepAttController->setBandRestoreToRadio(true);
    m_radioModel->setStepAttController(m_stepAttController.get());
    m_stepAttController->setReceiverManager(m_radioModel->receiverManager());
    if (MoxController* const mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::hardwareFlipped,
                m_stepAttController.get(),
                &StepAttenuatorController::onMoxHardwareFlipped,
                Qt::QueuedConnection);
    }

    m_agcSource = std::make_unique<DaemonAgcSource>(m_radioModel.get());

    // R1 Task 11: dedicated thread for the wideband FFT dispatch hop --
    // RadioModel currently hops that work onto ITS OWN thread to stay off
    // the P2 connection thread's hot path (wireConnectionSignals,
    // RadioModel.cpp), which is fine for the GUI (that own thread IS the
    // main thread, and getting off the network thread onto it is the
    // whole point) but nereusd has no such spare thread of its own -- its
    // RadioModel lives on the daemon's single Qt event-loop thread, which
    // will carry other daemon responsibilities from R1 Task 12 onward.
    //
    // Lazily created once and reused across a restart (see
    // widebandThread()'s doc comment in the header for why stop() joins
    // rather than destroys it): a QThread that has been quit()+wait()'d
    // can be start()ed again.
    if (!m_widebandThread) {
        m_widebandThread = std::make_unique<QThread>();
        m_widebandThread->setObjectName(QStringLiteral("WidebandFftThread"));
    }
    m_widebandThread->start();

    // Injected BEFORE either branch below that might call connectToRadio()
    // (and therefore wireConnectionSignals()), so the very first P2
    // connection's wideband-frame hop already targets the dedicated
    // thread instead of RadioModel's own.
    m_radioModel->setWidebandDispatchThread(m_widebandThread.get());

    // Relay connection-state transitions to this class's own signal.
    // Fires only on a REAL state change (RadioModel emits
    // connectionStateChanged from its connection-thread-marshalled state
    // machine), never synthetically from start() itself.
    connect(m_radioModel.get(), &RadioModel::connectionStateChanged, this,
            [this](NereusSDR::ConnectionState state) {
        if (m_stepAttControllerConfigured && m_stepAttController) {
            if (state == ConnectionState::Connected && m_radioModel
                && m_radioModel->connection()) {
                applyStepAttenuatorConnection(
                    m_radioModel->connection()->radioInfo().macAddress);
            } else {
                m_stepAttController->setRadioConnection(nullptr);
            }
        }
        emit radioConnected(state == ConnectionState::Connected);
    });

    QString radioMac;
#ifdef NEREUS_BUILD_TESTS
    if (m_testBoard.has_value()) {
        // Test-only path -- see primeBoardForTest()'s doc comment in
        // DaemonApp.h for why this exists instead of a real
        // connectToRadio() round trip. Mirrors the exact two calls
        // tst_p1_hl2_rx2_wiring.cpp already uses to prime a RadioModel
        // without a live connection: setBoardForTest() populates
        // boardCapabilities() from the real BoardCapabilities table (the
        // same table applyHpsdrModel() reads from inside connectToRadio()),
        // and configureStreamPool() sizes the stream allocator the same
        // way connectToRadio() does right before creating Slice A.
        m_radioModel->setBoardForTest(*m_testBoard);
        m_radioModel->prepareReceiveLayout(m_testRadioMac);
        const auto& primedCaps = m_radioModel->boardCapabilities();
        const int poolSlices = primedCaps.maxSlices > 0 ? primedCaps.maxSlices : 1;
        m_radioModel->configureStreamPool(m_radioModel->userStreamCount(), poolSlices,
                                           cfg.sampleRateHz);
        radioMac = m_testRadioMac;
    } else
#endif
    {
        // Bring up the station before asynchronous discovery. An authenticated
        // client can observe the disconnected state while a radio powers up.
        m_radioRecoveryEnabled = true;
        radioMac = m_selectedRadioMac;
        m_radioModel->prepareReceiveLayout(radioMac);
    }

    createConfiguredSlices(cfg.sliceCount);
#ifdef NEREUS_BUILD_TESTS
    if (m_testBoard.has_value()) {
        m_radioModel->bindUnboundSlices();
        createConfiguredSlices(cfg.sliceCount); // all-refused manifests use normal fallback count
        m_radioModel->completeReceiveLayoutStartup();
    }
#endif
    configureStepAttenuatorController(radioMac);
    mintFftEndpoints();

    // Remote Daemon R2 Task 18: the wss control plane. AFTER the slices
    // exist, so a client connecting immediately gets them in its
    // connect-time burst without waiting for a delta -- StationServer's
    // own ObjectRegistry::backfillExistingSlices() covers the case either
    // way, but there is no reason to make it the only cover.
    StationHostOptions hosting;
    hosting.settings = &AppSettings::instance();
    hosting.stationRadios = m_stationRadios.get();
    hosting.selectedRadioMac = [this] { return m_selectedRadioMac; };
    hosting.coreName = cfg.coreName;
    hosting.remoteBind = cfg.remoteBind;
    hosting.remotePort = cfg.remotePort;
    hosting.statusPage = cfg.statusPage;
    hosting.statusPort = cfg.statusPort;
    hosting.pairingLanClickAllowed = cfg.pairingLanClickAllowed;
    hosting.remoteTransmitAllowed = cfg.remoteTransmitAllowed;
    hosting.supportConfigPath = cfg.sourcePath;
    hosting.linkMajors = m_linkMajors;
    hosting.audioBitrate = cfg.audioBitrate;
    hosting.audioLosslessAllowed = cfg.audioLosslessAllowed;
    hosting.displayAdaptive = cfg.displayAdaptive;
    hosting.displayBudgetLimits = cfg.displayBudgetLimits();
    hosting.rendezvousServers = cfg.rendezvousServers;
    hosting.relayAllowed = cfg.relayAllowed;
    m_stationHost = std::make_unique<StationHost>(m_radioModel.get(), hosting);
#ifdef NEREUS_BUILD_TESTS
    m_stationHost->setListenRetryIntervalsForTest(m_stationListenRetryInitialMs,
                                                   m_stationListenRetryMaximumMs);
    if (m_testDnsSdAdvertiser) {
        m_stationHost->setDnsSdAdvertiserForTest(std::move(m_testDnsSdAdvertiser));
    }
    m_stationHost->setDisplayLoadSourcesForTest(m_displayLoadInputsForTest,
                                                 m_displayGovernorNowForTest,
                                                 m_acceptedDisplayChargeForTest);
#endif
    // The initial receiver seed is complete. From here a station client's
    // edits must be tracked even if layout writeback awaits radio admission.
    m_radioModel->beginStationHandoverEditTracking();
    m_stationHost->start();

    // Parity Task 19 (R-IOS-25; remote design section 6.4): the station's
    // spot sources (DX cluster, RBN, POTA, PSK Reporter) whose Auto-Connect
    // or Auto-Start is on in the Core's settings run here, with no window.
    // After the station server, so its streams hold the first console
    // lines. Each window runs its own WSJT-X and SpotCollector listeners.
    m_radioModel->restoreStationSpotSources();

    // Fix round 1, Finding 1: mintFftEndpoints() only fills m_topology's
    // own private bookkeeping. Without this call the subscriptions never
    // reached RadioModel's live FFTRouter (RadioModel::fftRouter()) and
    // were completely inert. See the file header above.
    publishFftTopology();
    if (m_radioRecoveryEnabled) {
        m_radioRetryTimer->start(0);
    }

    return true;
}

void DaemonApp::stop()
{
    // LINK minor 3 (TX path): the radio stops transmitting before anything
    // else goes, as beginStationRelease() does, so a SIGTERM never tears
    // the model down under a key.
    if (m_radioModel) {
#ifdef NEREUS_BUILD_TESTS
        if (m_stopAllTxForTest) {
            m_stopAllTxForTest();
        } else
#endif
        {
            m_radioModel->stopAllTx(QStringLiteral("The Core is stopping."));
        }
    }
    // iPhone app Task 17: nothing answers the console or a browser once
    // the Core is going away.
    // Fix wave (I1): a radio change's restart keeps the console commands.
    if (!m_keepConsoleOnStop) {
        m_controlSocket.reset();
        m_controlCommands.reset();
    }
    if (m_stationHost) {
        m_stationHost->quiesce();
    }
    stopRadioRecovery();
    // Parity Task 21: a rescan still listening is dropped.
    ++m_radioScanGeneration;
    if (m_radioScanThread) {
        m_radioScanThread->requestInterruption();
        m_radioScanThread->wait();
        m_radioScanThread.reset();
    }
    if (m_stationRadios) {
        m_stationRadios->clearCurrent();
    }
    if (m_radioConnectInProgress) {
        // A cold WDSP initialization pumps a nested event loop. Never delete
        // the RadioModel from inside its still-running connect stack. Its
        // non-interruptible wisdom job may finish, but the cancelled connect
        // path cannot create a radio socket afterward.
        m_stopDeferred = true;
        m_radioModel->disconnectFromRadio();
        return;
    }
    m_stopDeferred = false;
    // The host retires all media, telemetry and server callbacks while its
    // borrowed RadioModel is still alive. The daemon destroys that model below.
    if (m_stationHost) {
        m_stationHost->stop();
        m_stationHost.reset();
    }
    m_agcSource.reset();

    if (!m_radioModel) {
        m_stepAttController.reset();
        m_stepAttControllerConfigured = false;
        return;
    }

    // Reverse of start(): drop this run's FFT-topology subscriptions and
    // push the removal to the router BEFORE the RadioModel that owns it
    // is destroyed, so the router explicitly loses every mapping rather
    // than the mapping only going away because the whole object does.
    // See clearFftTopology()'s own doc comment for why this is a
    // per-consumer unsubscribe() loop, not `m_topology = FftTopology{};`
    // (fix round 2, Finding 1 reopened -- the wholesale-replacement
    // version silently made the removal a no-op).
    clearFftTopology();

    // R1 Task 11: quiesce the wideband thread before destroying the
    // RadioModel below. RadioModel::setWidebandDispatchThread pointed a
    // member OF this (about to be destroyed) RadioModel --
    // m_widebandDispatchContext -- at this thread; if the thread's event
    // loop were still processing when ~RadioModel() runs, a queued
    // wideband-frame delivery could land mid-dispatch against an object
    // being destroyed concurrently on this thread. quit()+wait() blocks
    // until the thread's event loop has fully stopped, so nothing can
    // touch RadioModel from another thread by the time m_radioModel.reset()
    // runs below. Deliberately NOT reset()/destroyed here -- see
    // widebandThread()'s own doc comment in the header for why the same
    // pointer must stay valid (joined, not running) after stop() rather
    // than going null or dangling.
    if (m_widebandThread) {
        m_widebandThread->quit();
        m_widebandThread->wait();
    }

    // Stop controller callbacks from the connection before RadioModel begins
    // destroying that connection. Keep the controller itself alive through
    // RadioModel teardown: teardownConnection() saves the controller's
    // per-MAC state while the model still holds its non-owning pointer.
    if (m_stepAttController) {
        m_stepAttController->setRadioConnection(nullptr);
    }

    // ~RadioModel() calls teardownConnection() and deletes every slice
    // (RadioModel.cpp), so resetting the pointer alone satisfies
    // "stop() leaves sliceCount() == 0." Reset before emitting so any
    // radioConnected(false) listener already sees a consistent
    // (sliceCount() == 0) state if it queries back into this object.
    m_radioModel->flushPendingSettingsSave();
    m_radioModel.reset();
    // After the model: its TX channel, the one thing feeding display 5, has
    // closed, and with the lane gone DestroyAnalyzer runs here (the order
    // the desktop keeps, MainWindow deleting its RadioModel first).
    m_txAnalyzer.reset();
    // Teardown also saves controller state. Commit the final accepted Core
    // configuration while AppSettings and the daemon profile still exist.
    AppSettings::instance().save();
    m_stepAttController.reset();
    m_stepAttControllerConfigured = false;

    emit radioConnected(false);
}

void DaemonApp::beginStationRelease()
{
    m_stationReleaseRadioRecoveryWasEnabled = m_radioRecoveryEnabled;
    if (m_radioModel) {
#ifdef NEREUS_BUILD_TESTS
        if (m_stopAllTxForTest) {
            m_stopAllTxForTest();
        } else
#endif
        {
            m_radioModel->stopAllTx(QStringLiteral("The Core is handing back the radio."));
        }
    }
    if (m_stationHost) { m_stationHost->quiesce(); }
    stopRadioRecovery();
    if (m_radioRetryTimer) { m_radioRetryTimer->stop(); }
}

DaemonApp::StationReleaseRecoveryResult DaemonApp::recoverFailedStationRelease()
{
    // A failure after stop() has destroyed the model must remain fail-closed.
    // While the nested WDSP connect stack is active, neither its model nor
    // the host borrowing it may be retired.
    if (!m_radioModel || !m_stationHost) {
        return StationReleaseRecoveryResult::Unavailable;
    }
    if (m_radioConnectInProgress) { return StationReleaseRecoveryResult::Pending; }

    // quiesce() closes the old listener and retires its sessions. stop()
    // drops the old server and media callbacks without touching the borrowed
    // RadioModel; start() reuses the retained options, identity and model.
    m_stationHost->stop();
    if (!m_stationHost->start()) {
        return StationReleaseRecoveryResult::Unavailable;
    }
    if (m_stationReleaseRadioRecoveryWasEnabled) {
        m_radioRecoveryEnabled = true;
        m_radioRetryTimer->start(0);
    }
    return StationReleaseRecoveryResult::Restored;
}

DaemonApp::StationReleaseResult DaemonApp::tryCompleteStationRelease(QString* reason)
{
    if (reason) { reason->clear(); }
    if (m_radioConnectInProgress) { return StationReleaseResult::Pending; }
    if (m_radioModel && !m_radioModel->saveForStationHandover(reason)) {
        return StationReleaseResult::Failed;
    }
    stop();
#ifdef NEREUS_BUILD_TESTS
    if (m_afterStationStopForTest) { m_afterStationStopForTest(); }
#endif
    QString saveError;
    if (!AppSettings::instance().save(&saveError)) {
        if (reason) {
            *reason = QStringLiteral("The Core could not save its settings. Check its log and "
                                     "try release again.");
        }
        return StationReleaseResult::Failed;
    }
    return StationReleaseResult::Stopped;
}

void DaemonApp::createTxAnalyzer()
{
    // R-R3-39 / R-IOS-03: WdspEngine::createTxChannel points the TX siphon
    // at analyzer display 5 (TXASetSipMode 1, TXASetSipDisplay 5), and
    // Spectrum0 dereferences that display unchecked on every keyed block.
    // Thetis creates the transmitter's display analyzer in ChannelMaster,
    // right after the transmitter's DSP channel, whatever the display does.
    // From Thetis ChannelMaster cmaster.c:192-198 [v2.10.3.15] (create_xmtr):
    //     // display
    //     XCreateAnalyzer (
    //         in_id,
    //         &rc,
    //         262144,
    //         1,
    //         1,
    //         "");
    // NereusSDR's desktop makes the same call through TxAnalyzer, which
    // MainWindow creates; nereusd has no MainWindow, so the Core creates
    // the same TxAnalyzer here, on the model's transmit lane, with the same
    // set-up. Its XCreateAnalyzer is queued on the lane now, ahead of any
    // TX channel's create barrier, so display 5 exists before the first
    // block. A remote window's transmit panadapter is planned separately;
    // this keeps the siphon, which it will need, pointed at a real display.
    if (!m_radioModel->ownsLocalDsp()) {
        return;
    }
    m_txAnalyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId, nullptr,
                                                m_radioModel->transmitLane());
    m_txAnalyzer->applyStationRates();
    // Task 28 (R-R3-49, A11): setTxAnalyzer makes the transmit display's
    // feed (RadioModel::txDisplayFeed), which runs the analyzer on every
    // key as this function used to (SetAnalyzer's bf_sz from the TX
    // channel's DSP block, then start; stop on unkey), whether or not a
    // window watches, and sets its view from a remote window's
    // transmitting pan when one does.
    m_radioModel->setTxAnalyzer(m_txAnalyzer.get());
}

void DaemonApp::configureStepAttenuatorController(const QString& mac)
{
    if (!m_radioModel || !m_stepAttController) {
        return;
    }

    // R-R3-46: the same feed a local window uses (RadioModel::
    // followReceiveSliceWithStepAttenuator): slice A's receive band for the
    // attenuator and preamp memory, the transmit slice's band and mode for
    // ATT-on-TX, on every change of either and when the binding moves.
    m_radioModel->followReceiveSliceWithStepAttenuator();

    m_stepAttControllerConfigured = true;
    applyStepAttenuatorConnection(mac);
}

void DaemonApp::applyStepAttenuatorConnection(const QString& mac)
{
    if (!m_radioModel || !m_stepAttController) {
        return;
    }

    const auto& caps = m_radioModel->boardCapabilities();
    m_stepAttController->setMinAttenuation(caps.attenuator.minDb);
    // R-R3-46: the same ceiling the local RX applet gives this board
    // (RxApplet.cpp, BoardCapsTable::stepAttMaxDb): 61 dB on Atlas, Hermes,
    // Hermes II, Angelia and Orion with Alex filters, the board row's own
    // maximum otherwise (31 dB; the Hermes Lite 2 keeps -28..31 dB).
    m_stepAttController->setMaxAttenuation(
        BoardCapsTable::stepAttMaxDb(caps.board, caps.hasAlexFilters));
    m_stepAttController->setHasStepAttenuatorCal(caps.hasStepAttenuatorCal);
    m_stepAttController->setIsHpsdrBoard(caps.board == HPSDRHW::Atlas);
    m_stepAttController->setRadioConnection(m_radioModel->connection());
    // Before loadSettings: the stored preamp modes move to the ten Thetis
    // modes by the label this board shows.
    m_stepAttController->setBoardIdentity(caps.board,
                                          m_radioModel->hardwareProfile().model,
                                          caps.hasAlexFilters);

    // Select the current band before loading, because loadSettings restores
    // the per-band RX attenuation and preamp slot for m_currentBand.
    m_radioModel->syncStepAttenuatorToReceiveSlice();
    if (!mac.isEmpty()) {
        m_stepAttController->loadSettings(mac);
    }
}

// Remote Daemon R2 Task 18. Opt-in: cfg.remotePort == 0 means "do not
// listen" and is the default (DaemonConfig.h explains why). A listener
// that fails to come up is logged with StationServer::lastError() and is
// retried at a bounded rate without rebuilding station state. The initial
// failure is NOT a startup failure, matching this class's existing treatment
// of a radio that cannot be found: a daemon that still demodulates locally is
// more useful than one that refuses to boot.
QHostAddress DaemonApp::listenerAddressFor(const QString& bind)
{
    return StationHost::listenerAddressFor(bind);
}

QString DaemonApp::coreLabel() const
{
    return m_stationHost ? m_stationHost->coreLabel() : QString();
}

StationRadioStatus DaemonApp::radioStatus() const
{
    return m_stationHost ? m_stationHost->radioStatus() : StationRadioStatus{};
}

QString DaemonApp::statusPageAddress() const
{
    return m_stationHost ? m_stationHost->statusPageAddress() : QString();
}

StationLanPairing DaemonApp::stationLanPairingFor(const StationServer& server)
{
    return StationHost::stationLanPairingFor(server);
}

StationControlReply DaemonApp::runControlCommand(const QStringList& args)
{
    if (!m_controlCommands) {
        StationControlCommands::Sources sources;
        sources.server = [this]() { return stationServer(); };
        sources.radio = [this]() { return radioStatus(); };
        sources.label = [this]() { return coreLabel(); };
        sources.statusPageAddress = [this]() { return statusPageAddress(); };
        m_controlCommands = std::make_unique<StationControlCommands>(std::move(sources));
    }
    return m_controlCommands->execute(args);
}

StationServer* DaemonApp::stationServer() const
{
    return m_stationHost ? m_stationHost->server() : nullptr;
}

StationStatusPage* DaemonApp::statusPage() const
{
    return m_stationHost ? m_stationHost->statusPage() : nullptr;
}

#ifdef NEREUS_BUILD_TESTS
int DaemonApp::stationListenAttemptCountForTest() const
{
    return m_stationHost ? m_stationHost->listenAttemptCount() : 0;
}

StationLanAnnouncement DaemonApp::stationAnnouncementForTest() const
{
    return m_stationHost ? m_stationHost->stationAnnouncementForTest() : StationLanAnnouncement{};
}

DnsSdRecord DaemonApp::dnsSdRecordForTest() const
{
    return m_stationHost ? m_stationHost->dnsSdRecordForTest() : DnsSdRecord{};
}
#endif

bool DaemonApp::startControlSocket(const QString& path)
{
    m_controlSocket = std::make_unique<StationControlSocket>(
        [this](const QStringList& args) { return runControlCommand(args); });
    if (!m_controlSocket->listen(path)) {
        qCWarning(lcApp) << "DaemonApp: the console commands cannot reach this Core:"
                          << m_controlSocket->lastError();
        m_controlSocket.reset();
        return false;
    }
    qCInfo(lcApp) << "DaemonApp: console commands answered at" << path;
    return true;
}

#ifdef NEREUS_BUILD_TESTS
bool DaemonApp::stationAnnouncedForTest() const
{
    return m_stationHost && m_stationHost->stationAnnouncedForTest();
}

bool DaemonApp::dnsSdAdvertisedForTest() const
{
    return m_stationHost && m_stationHost->dnsSdAdvertisedForTest();
}

void DaemonApp::setDnsSdAdvertiserForTest(std::unique_ptr<DnsSdAdvertiser> advertiser)
{
    m_testDnsSdAdvertiser = std::move(advertiser);
}
#endif

int DaemonApp::sliceCount() const
{
    return m_radioModel ? m_radioModel->slices().size() : 0;
}

bool DaemonApp::stationListenerReady() const
{
    return m_stationHost && m_stationHost->listenerReady();
}

bool DaemonApp::stationListenerRetryPending() const
{
    return m_stationHost && m_stationHost->listenerRetryPending();
}

void DaemonApp::applyConfigToSettings(const DaemonConfig& cfg,
                                      const QString& mac) const
{
    auto& settings = AppSettings::instance();

    // The config file's sample_rate_hz is a starting value only (operator
    // ruling 2026-09-27, R-R3-49). It seeds the radio's rate when nothing
    // is saved for that radio yet; once a rate is saved (chosen from any
    // window, or an earlier seed from this file) the saved rate wins
    // across restarts and installs. Writing it on every start threw away
    // the rate the operator picked from a window each time the Core
    // restarted.
    //
    // sampleRateExplicit, not sampleRateHz > 0: the field always holds a
    // usable rate (validate() rejects <= 0), so it cannot express "the
    // operator did not ask". Seeding on the struct default would stamp a
    // rate onto a radio the config file never mentioned. Same reasoning as
    // the audioDevice branch below.
    if (!mac.isEmpty() && cfg.sampleRateExplicit && cfg.sampleRateHz > 0) {
        const QString rateKey = QStringLiteral("radioInfo/sampleRate");
        // A saved value that is not a positive rate counts as nothing
        // saved: resolveSampleRate() treats it as missing too.
        const int saved = settings.hardwareValue(mac, rateKey).toInt();
        if (saved > 0) {
            qCInfo(lcApp).noquote()
                << QStringLiteral("DaemonApp: using the saved sample rate %1 for this "
                                  "radio (the config file's %2 is only a starting value)")
                       .arg(saved)
                       .arg(cfg.sampleRateHz);
        } else {
            // resolveSampleRate() (SampleRateCatalog.cpp) reads exactly
            // this key at connect time and validates it against the
            // board's allowed-rate list, falling back to the board default
            // with a warning when the value is not supported. The seed
            // goes through that same path as a saved rate does, so the
            // daemon and the GUI never disagree about what is valid.
            settings.setHardwareValue(mac, rateKey, cfg.sampleRateHz);
            qCInfo(lcApp).noquote()
                << QStringLiteral("DaemonApp: seeded the sample rate from the config "
                                  "file: %1")
                       .arg(cfg.sampleRateHz);
        }
    }

    // audio_device is a starting value too (G-16), exactly like the rate
    // above: it seeds audio/Speakers/DeviceName only when no speaker choice
    // is saved. A saved choice (a window's pick, or an earlier seed) wins
    // across restarts. A saved empty name is a choice too: it is how a
    // window saves "platform default" (AudioDeviceConfig::saveToSettings),
    // so presence of the key, not a non-empty value, means "saved".
    //
    // An empty audio_device never writes: it must not stamp an empty
    // DeviceName over anything, and a genuinely unset key resolves to the
    // platform default inside AudioEngine::ensureSpeakersOpen().
    if (!cfg.audioDevice.isEmpty()) {
        const QString speakerKey = QStringLiteral("audio/Speakers/DeviceName");
        if (settings.contains(speakerKey)) {
            qCInfo(lcApp).noquote()
                << QStringLiteral("DaemonApp: using the saved speaker device \"%1\" "
                                  "(the config file's \"%2\" is only a starting value)")
                       .arg(settings.value(speakerKey).toString(), cfg.audioDevice);
        } else {
            settings.setValue(speakerKey, cfg.audioDevice);
            qCInfo(lcApp).noquote()
                << QStringLiteral("DaemonApp: seeded the speaker device from the config "
                                  "file: \"%1\"")
                       .arg(cfg.audioDevice);
        }
    }
}

void DaemonApp::cancelRadioDiscovery()
{
    ++m_radioRecoveryGeneration;
    m_radioRetryTimer->stop();
    if (m_radioDiscoveryThread) {
        m_radioDiscoveryThread->requestInterruption();
        m_radioDiscoveryThread->wait();
        m_radioDiscoveryThread.reset();
    }
}

void DaemonApp::scheduleRadioDiscovery()
{
    if (!m_radioRecoveryEnabled || !m_radioModel || m_radioDiscoveryThread
        || m_radioRetryTimer->isActive()) {
        return;
    }
    // Nereus daemon policy, not a radio-protocol timeout.
    m_radioRetryTimer->start(m_radioRetryNextMs);
    m_radioRetryNextMs = std::min(m_radioRetryMaximumMs, m_radioRetryNextMs * 2);
}

void DaemonApp::attemptRadioDiscovery()
{
    if (!m_radioRecoveryEnabled || !m_radioModel || m_radioDiscoveryThread
        || m_radioConnectInProgress || m_radioModel->isConnected()) {
        return;
    }
    // Preserve the full process-wide post-stop quiet interval, including in
    // tests which inject discoveries without sending any network probes.
    const qint64 quietMs = m_radioModel->discovery()->holdOffRemainingMs();
    if (quietMs > 0) {
        m_radioRetryTimer->start(int(quietMs));
        return;
    }
    const quint64 generation = m_radioRecoveryGeneration;
    auto result = std::make_shared<QList<RadioInfo>>();
#ifdef NEREUS_BUILD_TESTS
    const auto provider = m_discoveryProviderForTest;
    auto* worker = QThread::create([result, provider]() {
        if (provider) {
            *result = provider();
            return;
        }
#else
    auto* worker = QThread::create([result]() {
#endif
        RadioDiscovery discovery;
        QEventLoop loop;
        QTimer cancellation;
        cancellation.setInterval(25);
        QObject::connect(&cancellation, &QTimer::timeout, &loop, [&loop]() {
            if (QThread::currentThread()->isInterruptionRequested()) {
                loop.quit();
            }
        });
        QObject::connect(&discovery, &RadioDiscovery::discoveryFinished,
                         &loop, &QEventLoop::quit);
        QTimer::singleShot(0, &discovery, [&discovery]() {
            discovery.startDiscovery();
        });
        cancellation.start();
        loop.exec();
        if (!QThread::currentThread()->isInterruptionRequested()) {
            *result = discovery.discoveredRadios();
        }
    });
    worker->setObjectName(QStringLiteral("DaemonRadioDiscovery"));
    m_radioDiscoveryThread.reset(worker);
    connect(worker, &QThread::finished, this, [this, worker, generation, result]() {
        if (generation != m_radioRecoveryGeneration || !m_radioRecoveryEnabled
            || m_radioDiscoveryThread.get() != worker) {
            return;
        }
        worker->wait();
        m_radioDiscoveryThread.reset();
        finishRadioDiscovery(*result);
    });
    worker->start();
}

void DaemonApp::finishRadioDiscovery(const QList<RadioInfo>& found)
{
    if (!m_radioRecoveryEnabled || !m_radioModel) {
        return;
    }
    // Parity Task 21 (R-IOS-18): the Core's choice order. Never the first
    // radio found: a radio chosen from an app, radio_mac, the one radio in
    // sight, or wait for a choice (StationRadios::choose). This run's radio,
    // once identified, is kept across a reconnect.
    m_stationRadios->setVisible(found);
    const StationRadios::Choice choice = StationRadios::choose(
        found, m_selectedRadioMac, m_stationRadios->savedChoice(), m_radioConfig.radioMac);
    if (choice.pick != StationRadios::Pick::Radio) {
        m_stationRadios->setWaiting(choice.reason);
        endRadioSwitch();
        scheduleRadioDiscovery();
        return;
    }
    RadioInfo chosen = choice.radio;
    // The model an app set for this radio (station.setRadioModel), as the
    // local window's Edit radio override applies at connect.
    const HPSDRModel modelOverride = m_stationRadios->overrideFor(chosen.macAddress);
    if (modelOverride != HPSDRModel::FIRST) {
        chosen.modelOverride = modelOverride;
    }
    const RadioInfo* const selected = &chosen;
    if (m_selectedRadioMac.isEmpty()) {
        m_selectedRadioMac = selected->macAddress;
        m_stationRadios->setTarget(m_selectedRadioMac);
    }
    m_stationRadios->setCurrent(chosen);
    const bool preserve = m_radioAttempted;
    m_radioAttempted = true;
    if (!preserve) {
        m_radioModel->prepareReceiveLayout(m_selectedRadioMac);
        applyConfigToSettings(m_radioConfig, m_selectedRadioMac);
    }
    m_radioConnectInProgress = true;
    if (preserve) {
        m_radioModel->connectToRadioPreservingSlices(*selected);
    } else {
        m_radioModel->connectToRadio(*selected);
    }
    m_radioConnectInProgress = false;
    if (m_stopDeferred) {
        stop();
        return;
    }
    if (!m_radioRecoveryEnabled) {
        // Turned off during the connect (stopRadioRecovery), which ended a
        // radio change under way.
        return;
    }
    if (RadioConnection* const connection = m_radioModel->connection()) {
        // Fix wave, C1: a change that neither connects nor fails within the
        // Core's connect bound ends anyway.
        if (m_stationRadios->switching()) {
            m_radioSwitchDeadline->start(m_radioSwitchBoundMs);
        }
        const quint64 generation = m_radioRecoveryGeneration;
        connect(connection, &RadioConnection::connectFailed, this,
                [this, generation](ConnectFailure, const QString&) {
            if (generation == m_radioRecoveryGeneration && m_radioRecoveryEnabled) {
                retireRadioAndRetry();
            }
        }, Qt::QueuedConnection);
    } else {
        // The connect did not start: the change has failed.
        endRadioSwitch();
        scheduleRadioDiscovery();
    }
}

void DaemonApp::onRadioStateForRecovery(ConnectionState state)
{
    if (!m_radioRecoveryEnabled || !m_radioModel || m_retiringRadio
        || state != m_radioModel->connectionState()) {
        return;
    }
    if (state == ConnectionState::Connected) {
        // Parity Task 21: a radio change has finished. Fix wave, I6: a
        // pending choice becomes the saved one now that it connected.
        if (m_stationRadios && m_radioModel->connection()) {
            m_stationRadios->confirmChoice(
                m_radioModel->connection()->radioInfo().macAddress);
        }
        endRadioSwitch();
        m_radioModel->completeReceiveLayoutStartup();
        m_radioRetryNextMs = m_radioRetryInitialMs;
        if (!m_radioConnectedBefore) {
            createConfiguredSlices(m_radioConfig.sliceCount);
            m_radioConnectedBefore = true;
        }
        clearFftTopology();
        mintFftEndpoints();
        publishFftTopology();
    } else if ((state == ConnectionState::LinkLost
                || state == ConnectionState::Disconnected) && m_radioAttempted
               && m_radioModel->connection()) {
        retireRadioAndRetry();
    }
}

void DaemonApp::retireRadioAndRetry()
{
    if (!m_radioRecoveryEnabled || !m_radioModel || m_retiringRadio) {
        return;
    }
    // Fix wave, C1: the new radio's connect failed or its link was lost, so a
    // radio change under way ends (its choice is kept as this run's radio).
    // A change chosen since then restarts the run (restartForRadioChange):
    // this failure is the old radio's, not that change's, so it stays under
    // way (endRadioSwitch keeps it). The change's deadline and the connect
    // watchdog are both 2000 ms, so the deadline can end a change, and a
    // window choose again, while the old radio's failure is still queued.
    endRadioSwitch();
    // Invalidate both discovery completions and terminal reports from the
    // retired connection. RadioModel keeps the slices while retiring all DSP.
    cancelRadioDiscovery();
    clearFftTopology();
    m_retiringRadio = true;
    // TX safety fix round 2 (2026-09-30): a recovery retire, not the
    // operator's disconnect, so a lost-link key lock holds until the
    // rebuilt link is Connected.
    m_radioModel->retireConnectionForRecovery();
    m_retiringRadio = false;
    // Parity Task 21: no radio until it is found again; it stays listed.
    if (m_stationRadios) {
        m_stationRadios->clearCurrent();
    }
    scheduleRadioDiscovery();
}

void DaemonApp::createConfiguredSlices(int sliceCountRequested)
{
    if (m_radioModel->receiveLayoutOverridesConfiguredCount()) {
        return;
    }
    // caps.maxSlices directly, NOT the maxSlices() accessor: that
    // accessor returns 1 until RadioModel::isConnected() is true.
    // RadioModel::connectToRadio() itself reads boardCapabilities()
    // directly for the identical reason (RadioModel.cpp, the comment
    // beside its own "poolSlices" local: "caps.maxSlices rather than the
    // maxSlices() accessor: that accessor returns 1 until isConnected()
    // is true, and m_connection is not assigned until further down this
    // function") -- isConnected() does not become true until AFTER
    // connectToRadio()'s own synchronous WDSP-wisdom wait completes and
    // m_connection is assigned, well after hardware-profile resolution
    // (and therefore this SKU's maxSlices) is already settled. Reading
    // boardCapabilities() directly means this method sees the right
    // number regardless of which path start() took (a real
    // connectToRadio(), the primeBoardForTest() seam, or neither).
    const auto& caps = m_radioModel->boardCapabilities();
    const int capMaxSlices = caps.maxSlices > 0 ? caps.maxSlices : 1;
    const int target = std::min(sliceCountRequested, capMaxSlices);

    // Tops up from however many slices already exist. connectToRadio()
    // (when a radio was found and connected above) already created
    // Slice A via its own no-argument addSlice() call before this method
    // runs, so this loop runs (target - 1) more times in that case. The
    // primeBoardForTest() seam does NOT create Slice A itself (it only
    // primes boardCapabilities() and sizes the stream pool, mirroring
    // tst_p1_hl2_rx2_wiring.cpp's own setBoardForTest() + configureStreamPool()
    // pattern, which likewise leaves the first addSlice() call to its
    // caller), so in that path -- and in the no-radio-found path -- this
    // loop starts from zero and runs the full target times.
    while (m_radioModel->slices().size() < target) {
        const int id = m_radioModel->addSlice();
        if (id < 0) {
            // The allocator refused (e.g. no stream left to share).
            // "Up to" the target, per this class's own contract: stop
            // rather than loop forever re-asking for something that
            // just failed.
            qCWarning(lcApp) << "DaemonApp: addSlice() refused at"
                              << m_radioModel->slices().size() << "of"
                              << target << "requested slices";
            break;
        }
    }
}

void DaemonApp::mintFftEndpoints()
{
    for (SliceModel* slice : m_radioModel->slices()) {
        const int stream = slice->streamIndex();
        if (stream < 0) {
            // Unbound -- e.g. the disconnected-default slice, created
            // before any stream pool exists. Nothing to subscribe to yet.
            continue;
        }
        const QString endpointId =
            QStringLiteral("daemon-ep-%1").arg(m_nextEndpointId++);
        m_topology.subscribe(endpointId, stream);
    }
}

void DaemonApp::publishFftTopology()
{
    if (!m_radioModel) {
        return;
    }
    m_topology.applyTo(*m_radioModel->fftRouter());
}

void DaemonApp::clearFftTopology()
{
    // Per-consumer unsubscribe(), NOT `m_topology = FftTopology{};`. See
    // this method's own doc comment in DaemonApp.h for the full
    // explanation: wholesale replacement also wipes FftTopology's
    // private m_lastAppliedConsumers, which applyTo()'s removal loop
    // needs in order to have anything to remove at all. subscriptions()
    // returns a fresh QList (not a live view into m_streamsByConsumer),
    // so mutating m_topology while iterating it here is safe.
    for (const SpectrumSubscription& sub : m_topology.subscriptions()) {
        m_topology.unsubscribe(sub.consumerId);
    }
    publishFftTopology();
}

#ifdef NEREUS_BUILD_TESTS
QList<int> DaemonApp::fftRouterMappingsForTest(const QString& consumerId) const
{
    if (!m_radioModel) {
        return {};
    }
    return m_radioModel->fftRouter()->receiversForPan(consumerId);
}
#endif


// ── Parity Task 21 (R-IOS-18): the Core's radio ───────────────────────────

void DaemonApp::ensureStationRadios()
{
    if (m_stationRadios) {
        return;
    }
    // The Core's own store (server_main.cpp resolved the profile first).
    m_stationRadios = std::make_unique<StationRadios>(AppSettings::instance());
    m_stationRadios->onSelect = [this](const QString& mac) { switchRadio(mac); };
    m_stationRadios->onRescan = [this]() { rescanRadios(); };
    // Fix wave (M2): the waiting reason reaches every window as the Core's
    // radio property stationRadioWaiting.
    connect(m_stationRadios.get(), &StationRadios::entriesChanged, this, [this]() {
        if (m_radioModel) {
            m_radioModel->setStationRadioWaiting(m_stationRadios->waitingReason());
        }
    });
}

void DaemonApp::switchRadio(const QString& mac)
{
    // The choice is pending (StationRadios::select) until it connects. The
    // operator's ruling of 2026-09-26: the radio is changed by restarting
    // the run, and every app reconnects by itself: the receive layout and per-radio
    // settings of a Core run belong to one radio (RadioModel's receive
    // layout refuses another identity), so the new radio comes up exactly
    // as it would on the Core's next start, with its own layout, settings,
    // capabilities, catalogue and announcement. Every window reconnects.
    // After this turn: the change itself, preceded by its held answer.
    qCInfo(lcApp) << "DaemonApp: changing the Core's radio to" << mac;
    const std::optional<RadioInfo> radio =
        m_stationRadios ? m_stationRadios->radioFor(mac) : std::nullopt;
    m_radioChangeReason = radioChangeReason(radio ? radio->displayName() : mac);
    if (m_radioModel) {
        m_radioModel->setStationRadioChangeUnderway(true);
    }
    // Follow-up N3 (ruling (a)): the chooser's answer and the other
    // devices' notices wait for the restart turn, which says whether the
    // change happens.
    if (StationServer* server = stationServer()) {
        server->holdRadioChangeAnswers();
    }
    m_radioChangeRestartPending = true;
    QTimer::singleShot(0, this, &DaemonApp::restartForRadioChange);
}

QString DaemonApp::radioChangeReason(const QString& radioName)
{
    return QStringLiteral("The Core is switching to %1. This app reconnects by itself.")
        .arg(radioName);
}

QString DaemonApp::radioChangeStoppedReason()
{
    return QStringLiteral("The Core stopped its radio before it could switch.");
}

void DaemonApp::stopRadioRecovery()
{
    m_radioRecoveryEnabled = false;
    cancelRadioDiscovery();
    // No discovery is left to find a changed radio, and no connect can start
    // that would arm the change's deadline, so a change under way would stay
    // switching, refusing every window's choice. A restart's own stop is the
    // exception: the start that follows serves the change.
    if (m_radioChangeRestarting) {
        return;
    }
    if (m_radioChangeRestartPending) {
        // Stopped between the choice and its restart (stop(), a station
        // release, the operator's disconnect): the queued restart would start
        // the run again and undo the stop, so it is cancelled
        // (restartForRadioChange returns once this flag is clear). The change
        // never ran: the chooser's held answer is refused, and the choice is
        // dropped, as on the on-air refusal, since no run serves it.
        m_radioChangeRestartPending = false;
        qCInfo(lcApp) << "DaemonApp: the radio change was cancelled: the run stopped first";
        if (m_stationRadios) {
            m_stationRadios->dropPendingChoice();
        }
        if (StationServer* server = stationServer()) {
            server->finishRadioChange(false, radioChangeStoppedReason());
        }
    }
    endRadioSwitch();
}

void DaemonApp::endRadioSwitch()
{
    // A change chosen but not yet run (switchRadio until restartForRadioChange)
    // is never ended here: whatever reports now (the old radio's failure, its
    // late Connected, a discovery or connect that did not start) belongs to
    // the run the change is about to restart, not to the change. A deadline
    // running now is that run's too (a discovery completion queued ahead of
    // the restart can start one); the restart stops it, and the change's own
    // deadline starts when the new radio's connect does
    // (finishRadioDiscovery).
    if (m_radioChangeRestartPending) {
        return;
    }
    m_radioSwitchDeadline->stop();
    if (m_stationRadios) {
        m_stationRadios->setSwitching(false);
    }
    if (m_radioModel) {
        m_radioModel->setStationRadioChangeUnderway(false);
    }
}

bool DaemonApp::refuseRadioChangeOnAir()
{
    QString reason;
    if (!m_radioModel || !m_radioModel->stationOnAirRefusal(&reason)) {
        return false;
    }
    qCWarning(lcApp) << "DaemonApp: the radio change was refused:" << reason;
    if (m_stationRadios) {
        m_stationRadios->dropPendingChoice();
    }
    // Follow-up N3: the chooser is answered refused with the on-air reason;
    // nobody is told the radio changed.
    if (StationServer* server = stationServer()) {
        server->finishRadioChange(false, reason);
    }
    endRadioSwitch();
    return true;
}

void DaemonApp::restartForRadioChange()
{
    if (!m_radioChangeRestartPending) {
        // The run was stopped first (stopRadioRecovery cancelled the change),
        // or another queued restart already ran it.
        return;
    }
    if (m_radioConnectInProgress) {
        // A connect's nested WDSP start is running; retry once it unwinds.
        QTimer::singleShot(100, this, &DaemonApp::restartForRadioChange);
        return;
    }
    m_radioChangeRestartPending = false;
    // The change's deadline is its own: one the run being restarted started
    // (a discovery completion queued ahead of this turn connected the old
    // choice while the change was switching) would otherwise outlive the
    // restart and end this change before the new radio's first discovery.
    // Stopped here, after the flag clears, not in switchRadio: that queued
    // completion runs between the two and starts the deadline again.
    // Every path from here ends the change (the on-air refusal, a failed
    // start, a run with no discovery) or reaches finishRadioDiscovery,
    // which ends it or starts its deadline. Recovery turned off before
    // that (stopRadioRecovery) ends it too.
    m_radioSwitchDeadline->stop();
    // Fix wave, M3: on the air now (a key that raced the change) refuses
    // the change; nothing keyed is torn down.
    if (refuseRadioChangeOnAir()) {
        return;
    }
    // The operator's ruling of 2026-09-26 (I2): every app is told why and
    // reconnects by itself. The command's answer and the confirm step's
    // notices, held until now (follow-up N3), are written first, and each
    // connection outlives the station server until its end is on the wire.
    if (StationServer* server = stationServer()) {
        // Follow-up N3: the held answer, then the notices, then each end.
        server->finishRadioChange(true, {});
        server->endSessionsForRadioChange(m_radioChangeReason);
    }
    const DaemonConfig cfg = m_radioConfig;
    // Fix wave (I1): the console commands answer across the restart.
    m_keepConsoleOnStop = true;
    m_radioChangeRestarting = true;
    const bool started = start(cfg);
    m_radioChangeRestarting = false;
    m_keepConsoleOnStop = false;
    if (!started) {
        qCWarning(lcApp) << "DaemonApp: the Core could not restart for its new radio";
        // The old run goes on with its own radio, so the choice belongs to
        // no run: kept, a later start() in this process would switch to it
        // unasked, and forget() would refuse that radio as in use.
        if (m_stationRadios) {
            m_stationRadios->dropPendingChoice();
        }
        endRadioSwitch();
    } else if (!m_radioRecoveryEnabled) {
        // A run with no discovery (a test board) never finds the new radio.
        endRadioSwitch();
    }
}

void DaemonApp::rescanRadios()
{
    if (!m_radioModel) {
        return;
    }
    if (!m_radioModel->isConnected()) {
        // Searching already: its next pass updates the list. Bring it
        // forward.
        if (!m_radioDiscoveryThread && m_radioRecoveryEnabled) {
            m_radioRetryTimer->start(0);
        }
        return;
    }
    if (m_radioScanThread) {
        return; // a scan is listening
    }
    // Connected: a scan that only lists what answers (it connects nothing).
    const quint64 generation = ++m_radioScanGeneration;
    auto result = std::make_shared<QList<RadioInfo>>();
#ifdef NEREUS_BUILD_TESTS
    const auto provider = m_discoveryProviderForTest;
    auto* worker = QThread::create([result, provider]() {
        if (provider) {
            *result = provider();
            return;
        }
#else
    auto* worker = QThread::create([result]() {
#endif
        RadioDiscovery discovery;
        QEventLoop loop;
        QTimer cancellation;
        cancellation.setInterval(25);
        QObject::connect(&cancellation, &QTimer::timeout, &loop, [&loop]() {
            if (QThread::currentThread()->isInterruptionRequested()) {
                loop.quit();
            }
        });
        QObject::connect(&discovery, &RadioDiscovery::discoveryFinished,
                         &loop, &QEventLoop::quit);
        QTimer::singleShot(0, &discovery, [&discovery]() {
            discovery.startDiscovery();
        });
        cancellation.start();
        loop.exec();
        if (!QThread::currentThread()->isInterruptionRequested()) {
            *result = discovery.discoveredRadios();
        }
    });
    worker->setObjectName(QStringLiteral("DaemonRadioScan"));
    m_radioScanThread.reset(worker);
    connect(worker, &QThread::finished, this, [this, worker, generation, result]() {
        if (generation != m_radioScanGeneration || m_radioScanThread.get() != worker) {
            return;
        }
        worker->wait();
        m_radioScanThread.reset();
        if (m_stationRadios) {
            m_stationRadios->setVisible(*result);
        }
    });
    worker->start();
}

} // namespace NereusSDR
