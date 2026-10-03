// no-port-check: NereusSDR-original. Task 48 shared station hosting around a
// borrowed model.
#include "core/station/StationHost.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/SliceOwnership.h"
#include "core/station/StationSliceOwnershipPolicy.h"
#include "core/daemon/DaemonTelemetryController.h"
#include "core/daemon/HostTelemetrySampler.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RendezvousClient.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationRendezvous.h"
#include "core/session/StationServer.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/media/DaemonMediaController.h"
#include "models/RadioModel.h"
#include "models/ReceiverDspLoadSampler.h"
#include "models/SliceModel.h"

#include <QHostInfo>
#include <QTimer>

#include <algorithm>

namespace NereusSDR {

StationHost::StationHost(RadioModel* model, const StationHostOptions& options)
    : m_radioModel(model), m_options(options)
{
    m_stationListenRetryTimer = std::make_unique<QTimer>();
    m_stationListenRetryTimer->setSingleShot(true);
    connect(m_stationListenRetryTimer.get(), &QTimer::timeout,
            this, &StationHost::attemptStationServerListen);
    if (model) {
        connect(model, &QObject::destroyed, this, [this] {
            m_radioModel = nullptr;
            stop();
        });
    }
}

StationHost::~StationHost()
{
    stop();
}

QHostAddress StationHost::listenerAddressFor(const QString& bind)
{
    // An empty bind or :: serves both IPv4 and IPv6. A concrete address
    // retains its family, matching the daemon configuration contract.
    return bind.isEmpty() || bind == QStringLiteral("::")
        ? QHostAddress(QHostAddress::Any) : QHostAddress(bind);
}

void StationHost::quiesce()
{
    if (m_quiescing) { return; }
    if (m_listenInProgress) {
        // A direct listeningChanged callback may request shutdown while
        // StationServer::listen() is still on the stack. Suppress all work
        // now, and close the listener as soon as listen() returns.
        m_quiescePending = true;
        m_quiescing = true;
        ++m_generation;
        return;
    }
    m_quiescing = true;
    m_quiescingInProgress = true;
    ++m_generation;
    m_statusPage.reset();
    m_rendezvous.reset();
    cancelStationServerListenRetry();
    m_stationAnnouncer.reset();
    m_dnsSdAdvertiser.reset();
    m_stationAnnouncement = {};
    m_dnsSdRecord = {};
    m_dnsSdAttempt.reset();
    if (m_stationServer) { m_stationServer->close(); }
    m_quiescingInProgress = false;
    // A stop requested from a close callback is completed by the owning
    // caller's subsequent stop(), after its nested connection stack unwinds.
}

void StationHost::stop()
{
    if (m_stopping) { return; }
    if (m_quiescingInProgress || m_listenInProgress) {
        // Finish the StationServer close/listen call stack before destroying
        // the server from a callback it emitted.
        m_stopPending = true;
        return;
    }
    m_stopping = true;
    quiesce();
    m_displayGovernorTimer.reset();
    m_displayGovernor.reset();
    m_telemetryControllers.clear();
    m_hostSampler.reset();
    m_mediaHub.reset();
    m_stationServer.reset();
    m_started = false;
    m_stopPending = false;
    m_stopping = false;
}

bool StationHost::start()
{
    if (!m_radioModel || !m_options.settings || m_started
        || (m_options.hostingDevice && m_radioModel->role() != RadioModel::Role::Local)) {
        return false;
    }
    m_started = true;
    m_quiescing = false;
    m_quiescePending = false;
    m_stopPending = false;
    const StationHostOptions& cfg = m_options;
    const quint64 generation = m_generation;
    cancelStationServerListenRetry();
    m_stationListenAttemptCount = 0;
    m_stationListenNextDelayMs = m_stationListenRetryInitialMs;
    if (cfg.remotePort == 0) {
        qCInfo(lcApp) << "DaemonApp: remote control disabled (remote_port = 0)";
        return true;
    }
    if (cfg.remotePort < 0 || cfg.remotePort > 65535) {
        qCWarning(lcApp) << "DaemonApp: remote_port is out of range:"
                          << cfg.remotePort << "- remote control not started";
        return true;
    }

    // iPhone app Task 12: an empty remote_bind (the default) is every
    // interface, IPv4 and IPv6 (QHostAddress::Any is dual stack); so is "::"
    // (R-R3-26, DaemonConfig::listenAddressFor).
    const QHostAddress bind = listenerAddressFor(cfg.remoteBind);
    if (bind.isNull()) {
        qCWarning(lcApp) << "DaemonApp: remote_bind is not a valid address:"
                          << cfg.remoteBind << "- remote control not started";
        return true;
    }

    // Constructing it is what provisions the TLS certificate and the Core's
    // identity key (iPhone app Task 12: never a pairing token any more; an
    // upgraded Core's existing token is loaded), and what prints the
    // first-run banner to stdout on the run that creates the key
    // (StationServer's constructor), deliberately not into the log. The
    // caller resolved and acquired the settings profile before start().
    m_stationServer = std::make_unique<StationServer>(m_radioModel.data(),
                                                      *cfg.settings, cfg.securityDirectory,
                                                      nullptr, cfg.linkMajors);
    if (cfg.hostingDevice) {
        const QPointer<StationServer> server(m_stationServer.get());
        const QPointer<RadioModel> model(m_radioModel);
        const QByteArray& stationId = SliceOwnership::stationDevice();
        server->deviceSessions()->registerHostingDevice(
            stationId, cfg.hostingDevice->name, cfg.hostingDevice->shortName);
        if (!server || !model) { return false; }
        server->setStationDeviceWords(cfg.hostingDevice->name,
                                      cfg.hostingDevice->shortName);
        if (!server || !model) { return false; }
        const QPointer<StationHost> self(this);
        StationSliceOwnershipPolicy::activate(model, server, [self, server, generation] {
            return self && server && !self->m_quiescing && self->m_generation == generation;
        });
        if (!self || !server || !model) { return false; }
    }
#ifdef NEREUS_BUILD_TESTS
    if (m_serverCreatedForTest) { m_serverCreatedForTest(m_stationServer.get()); }
#endif
    if (m_quiescing) { return false; }
    // Set before listen() so the first authenticated client sees the media
    // capability, never a control-only session that cannot be upgraded.
    m_stationServer->setMediaEnabled(true);
    // iPhone app Task 12: nereusd.conf's pairing_lan_click, read by the
    // pairing window (Task 14) for the one-click pairing on this network.
    m_stationServer->setPairingLanClickAllowed(cfg.pairingLanClickAllowed);
    // iPhone app plan Task 34: nereusd.conf's remote_transmit (allow by
    // default): the station transmit gate, and the model's receive-only
    // policy when it is deny.
    m_stationServer->setRemoteTransmitAllowed(cfg.remoteTransmitAllowed);
    // Parity Task 21 (R-IOS-18): the radios the Core finds and its choice of
    // one (stationRadiosVersion 1).
    m_stationServer->setStationRadios(cfg.stationRadios);
    // Parity Task 22 (R-R3-49): nereusd.conf, secrets removed, goes in the
    // Core's support bundle.
    m_stationServer->setSupportConfigPath(cfg.supportConfigPath);
    // iPhone app Task 17 (R-IOS-08): the status page, bound exactly where the
    // listener binds: `bind` above, listenerAddressFor(remote_bind), which is
    // DaemonConfig::listenAddressFor's rule (R-R3-26). A Core bound to one
    // address shows its page, and its code while unclaimed, only there; "::"
    // and an empty remote_bind take IPv4 and IPv6. It answers only this
    // computer's own networks whatever the bind. Its failure to listen is
    // logged, never fatal: the Core still runs and the console still shows
    // the code.
    if (cfg.statusPage) {
        StationStatusPage::Sources sources;
        sources.server = [this]() { return m_stationServer.get(); };
        sources.radio = [this]() { return radioStatus(); };
        sources.label = [this]() { return coreLabel(); };
        m_statusPage = std::make_unique<StationStatusPage>(std::move(sources));
        if (m_statusPage->listen(bind, static_cast<quint16>(cfg.statusPort))) {
            qCInfo(lcApp) << "DaemonApp: status page on port" << m_statusPage->serverPort();
        } else {
            qCWarning(lcApp) << "DaemonApp: the status page could not listen on port"
                              << cfg.statusPort << ":" << m_statusPage->lastError();
            m_statusPage.reset();
        }
    }
    // The first start's notice, beside the identity key banner the server
    // printed (which names station-identity.pem's full path and asks for a
    // backup): the Core's label and where its status page is, and how to
    // get the pairing code (nereusd pairing show). Standard output, never
    // the log, and never the code itself: on a packaged Core standard
    // output is the journal (Part C fix wave).
    if (m_stationServer->stationIdentity().wasCreatedThisRun()) {
        StationServer::printToConsole(
            StationStatusPage::formatFirstStartNotice(coreLabel(), statusPageAddress()));
    }
    // R-R3-08/37/40: with display_adaptive on, the Core always advertises a
    // display budget, so apps plan in budget mode from the start and follow
    // it down when the Core is busy: the configured pair when there is one,
    // otherwise a ceiling no real layout reaches. Off: exactly as before.
    std::optional<DisplayBudgetLimits> displayCeiling = cfg.displayBudgetLimits;
    if (cfg.displayAdaptive && !displayCeiling) {
        displayCeiling = DisplayLoadGovernor::computedCeiling();
        // An app older than the budget reason keeps legacy mode exactly.
        m_stationServer->setDisplayBudgetForReasonPeersOnly(true);
    }
    if (displayCeiling) {
        m_stationServer->setDisplayBudgetLimits(*displayCeiling);
    }
    // iPhone app Task 76 (ruling 9.1): a media controller per admitted
    // session, each made as its session's media starts.
    m_mediaHub = std::make_unique<DaemonMediaHub>(
        m_stationServer.get(), m_radioModel.data(), this);
    // R-R3-23: before listen(), so the first peer and sender use it.
    m_mediaHub->setAudioTargetBitrate(cfg.audioBitrate);
    // R-R3-21: once at start, the speakers' Opus rate this Core uses.
    qCInfo(lcApp).noquote() << QStringLiteral("DaemonApp: speakers' audio is Opus at %1 bit/s, %2")
                                   .arg(cfg.audioBitrate)
                                   .arg(cfg.audioBitrate == 48000
                                            ? QStringLiteral("fullband (sound up to 20 kHz)")
                                            : QStringLiteral("wideband (sound up to 8 kHz)"));
    m_mediaHub->setAudioLosslessAllowed(cfg.audioLosslessAllowed);
    // Install every source before advertising the capability. A client can
    // authenticate immediately after listen(), so there must be no window in
    // which telemetry is negotiated without a collector to publish it: each
    // session's collector is made as its telemetry starts (Task 76).
    m_hostSampler = std::make_shared<SharedHostSampler>();
    connect(m_stationServer.get(), &StationServer::telemetrySessionStarted,
            this, [this, generation](quint64 epoch) {
                if (generation == m_generation && !m_quiescing) { startSessionTelemetry(epoch); }
            });
    connect(m_stationServer.get(), &StationServer::telemetrySessionEnded,
            this, [this, generation](quint64 epoch) {
                if (generation == m_generation && !m_quiescing) { endSessionTelemetry(epoch); }
            });
    m_stationServer->setTelemetryEnabled(true);
    if (cfg.displayAdaptive && displayCeiling) {
        m_displayGovernor = std::make_unique<DisplayLoadGovernor>(*displayCeiling);
        m_displayGovernorClock.start();
        m_displayGovernorTimer = std::make_unique<QTimer>();
        m_displayGovernorTimer->setInterval(ReceiverDspLoadSampler::kSampleIntervalMs);
        connect(m_displayGovernorTimer.get(), &QTimer::timeout,
                this, [this, generation] {
                    if (generation == m_generation && !m_quiescing) { evaluateDisplayLoad(); }
                });
        // Only a media session has display to lower. When it ends, the next
        // one starts from the ceiling rather than from this one's load.
        connect(m_stationServer.get(), &StationServer::mediaSessionStarted, this, [this, generation] {
            if (generation == m_generation && !m_quiescing && m_displayGovernorTimer) {
                m_displayGovernorTimer->start();
            }
        });
        connect(m_stationServer.get(), &StationServer::mediaSessionEnded, this, [this, generation] {
            // Task 76: the load goes on while another device is on.
            if (generation != m_generation || m_quiescing
                || !m_displayGovernor || !m_displayGovernorTimer
                || !m_stationServer->mediaSessionEpochs().isEmpty()) {
                return;
            }
            m_displayGovernorTimer->stop();
            publishDisplayBudget(m_displayGovernor->reset());
        });
    }
    m_stationAnnouncer = std::make_unique<StationLanAnnouncer>();
    if (!m_dnsSdAdvertiser) {
        m_dnsSdAdvertiser = std::make_unique<DnsSdAdvertiser>();
    }
    connect(m_dnsSdAdvertiser.get(), &DnsSdAdvertiser::failed, this, [this, generation]() {
        // Let the next change try again rather than retrying at once.
        if (generation == m_generation && !m_quiescing) { m_dnsSdAttempt.reset(); }
    });
    const auto update = [this, generation] {
        if (generation == m_generation && !m_quiescing) { updateStationAnnouncement(); }
    };
    connect(m_stationServer.get(), &StationServer::listeningChanged,
            this, update);
    // iPhone app Task 16: the announcement and Bonjour carry the label, the
    // claimed state and how the Core pairs, so a rename (Task 13), a claim
    // or a change to the pairing window (Task 14) announces again.
    if (StationDevicesFacade* devices = m_stationServer->devicesFacade()) {
        connect(devices, &StationDevicesFacade::stationLabelChanged,
                this, update);
        connect(devices, &StationDevicesFacade::devicesStateChanged,
                this, update);
    }
    if (PairingWindow* window = m_stationServer->pairingWindow()) {
        connect(window, &PairingWindow::stateChanged,
                this, update);
    }
    connect(m_radioModel.data(), &RadioModel::connectionStateChanged,
            this, update);
    connect(m_radioModel.data(), &RadioModel::infoChanged,
            this, update);
    // iPhone app plan Task 25: waiting for a radio choice, or no longer.
    connect(m_radioModel.data(), &RadioModel::stationRadioWaitingChanged,
            this, update);
    // iPhone app Task 71: the device count follows the places taken.
    connect(m_stationServer->deviceSessions(), &DeviceSessionRegistry::placesTakenChanged,
            this, update);
    m_stationListenBind = cfg.remoteBind;
    m_stationListenArmed = true;
    m_stationListenPort = static_cast<quint16>(cfg.remotePort);
    attemptStationServerListen();
    if (!m_started || m_quiescing || generation != m_generation) {
        return false;
    }
    startRendezvous();
    return m_started && !m_quiescing;
}

void StationHost::startRendezvous()
{
    if (m_quiescing || !m_started) { return; }
    const StationHostOptions& cfg = m_options;
    m_rendezvous.reset();
    if (!m_stationServer) {
        return;
    }
    // iPhone app plan Task 29 (R-IOS-16): devices are told whether this
    // Core allows the relay (capability relayAllowed, link section 21.1).
    m_stationServer->setRelayAllowed(cfg.relayAllowed);
    const QList<QUrl> servers = RendezvousClient::serverUrls(cfg.rendezvousServers);
    if (servers.isEmpty()) {
        qCInfo(lcApp) << "DaemonApp: no remote access service configured "
                         "(rendezvous_servers is empty)";
        return;
    }
    m_rendezvous = std::make_unique<StationRendezvous>(m_stationServer.get(), servers,
                                                       cfg.relayAllowed);
    if (!m_rendezvous->start()) {
        qCWarning(lcApp) << "DaemonApp: the Core has no usable identity key, so it does not "
                            "register with the remote access service";
        m_rendezvous.reset();
    }
}

namespace {
QString announcementName(const QString& input, const QString& fallback)
{
    QString result;
    int bytes = 0;
    for (char32_t codepoint : input.toUcs4()) {
        if (QChar::category(codepoint) == QChar::Other_Control) { continue; }
        const QString character = QString::fromUcs4(&codepoint, 1);
        const int size = character.toUtf8().size();
        if (bytes + size > kStationLanMaxCoreNameBytes) { break; }
        result += character;
        bytes += size;
    }
    return result.trimmed().isEmpty() ? fallback : result.trimmed();
}
}


QString StationHost::coreLabel() const
{
    if (m_stationServer) {
        if (const StationDevicesFacade* devices = m_stationServer->devicesFacade()) {
            if (!devices->stationLabel().isEmpty()) {
                return devices->stationLabel();
            }
        }
    }
    return announcementName(m_options.coreName.isEmpty() ? QHostInfo::localHostName()
                                                             : m_options.coreName,
                            QStringLiteral("Nereus Core"));
}

StationRadioStatus StationHost::radioStatus() const
{
    StationRadioStatus status;
    if (m_radioModel && m_radioModel->isConnected()) {
        status.connected = true;
        status.model = m_radioModel->model();
        status.name = m_radioModel->name();
    }
    return status;
}

QString StationHost::statusPageAddress() const
{
    if (!m_statusPage || !m_statusPage->isListening()) {
        return {};
    }
    return StationStatusPage::addressForOperator(m_statusPage->serverAddress(),
                                                 m_statusPage->serverPort());
}


static_assert(DisplayLoadGovernor::kLoadIntervalMs == ReceiverDspLoadSampler::kSampleIntervalMs,
              "the display load governor settles in load intervals");

DisplayLoadInputs StationHost::gatherDisplayLoadInputs()
{
    if (m_displayLoadInputsForTest) {
        return m_displayLoadInputsForTest();
    }
    DisplayLoadInputs inputs;
    // The plan changes only when a channel or signal processing thread
    // starts or stops; its mutex is taken only then.
    ThreadPlacement& placement = ThreadPlacement::instance();
    const quint64 revision = placement.planRevision();
    if (m_placementPlanRevision != revision) {
        // Where threads actually run: a refused move leaves its role
        // sharing the housekeeping cores.
        m_placementPlan = placement.appliedPlan();
        m_placementPlanRevision = revision;
    }
    inputs.placement = m_placementPlan;
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice == nullptr) {
            continue;
        }
        const int sliceId = slice->sliceIndex();
        if (const std::optional<ReceiverDspLoad> load = m_radioModel->receiverDspLoad(sliceId)) {
            inputs.receivers.append({sliceId, *load});
        }
    }
    if (m_hostSampler) {
        m_hostSampler->setGovernorCpus(inputs.placement.active ? inputs.placement.housekeeping
                                                               : QList<int>{});
        inputs.systemCpuPercent = m_hostSampler->reading().systemCpuPercent;
        inputs.housekeepingCpuPercent = m_hostSampler->governorCpuPercent();
        inputs.cpuSampleBeganMsAgo = m_hostSampler->cpuSampleBeganMsAgo();
    }
    return inputs;
}

void StationHost::startSessionTelemetry(quint64 epoch)
{
    if (!m_stationServer || !m_radioModel || m_telemetryControllers.count(epoch) != 0) {
        return;
    }
    // Task 76: this session's own collector, with its own sequence and its
    // own audio diagnostics (its media controller's).
    const QPointer<StationHost> self(this);
    auto controller = std::make_unique<DaemonTelemetryController>(
        m_stationServer.get(), m_radioModel.data(), nullptr, this,
        DaemonTelemetryController::MonotonicClock{},
        [self, epoch] {
            return self && self->m_mediaHub ? self->m_mediaHub->audioDiagnostics(epoch)
                                            : DaemonAudioDiagnostics{};
        },
        std::unique_ptr<HostTelemetrySampler>{},
        DaemonTelemetryController::ReceiverLoadProvider{}, m_hostSampler);
    controller->bindToSession(epoch);
    m_telemetryControllers.emplace(epoch, std::move(controller));
}

void StationHost::endSessionTelemetry(quint64 epoch)
{
    auto it = m_telemetryControllers.find(epoch);
    if (it == m_telemetryControllers.end()) {
        return;
    }
    // It hears this same signal and stops itself; it goes once the signal
    // has unwound.
    DaemonTelemetryController* controller = it->second.release();
    m_telemetryControllers.erase(it);
    controller->deleteLater();
}

bool StationHost::anyDisplayBudgetInForce() const
{
    const QList<quint64> epochs = m_stationServer->mediaSessionEpochs();
    if (epochs.isEmpty()) {
        // No device yet: what a first one would be given.
        return m_stationServer->displayBudgetLimits().has_value();
    }
    for (quint64 epoch : epochs) {
        if (m_stationServer->displayBudgetLimits(epoch)) {
            return true;
        }
    }
    return false;
}

void StationHost::evaluateDisplayLoad()
{
    if (!m_displayGovernor || !m_radioModel || !m_mediaHub || !m_stationServer) {
        return;
    }
    // An app older than the budget reason plans without a budget (legacy
    // mode): there is nothing for it to follow. Task 76: with several
    // devices, the governor runs while any of them has a budget.
    if (!anyDisplayBudgetInForce()) {
        return;
    }
    // Cached readings only: RadioModel's 500 ms load snapshot, the shared
    // host sampler and the placement plan. None takes a DSP lock or
    // restarts another reader's interval.
    const qint64 nowMs = m_displayGovernorNowForTest ? m_displayGovernorNowForTest()
                                                     : m_displayGovernorClock.elapsed();
    const DisplayBudgetCharge accepted = m_acceptedDisplayChargeForTest
        ? m_acceptedDisplayChargeForTest() : m_mediaHub->acceptedDisplayCharge();
    DisplayLoadReading reading
        = displayLoadReadingFrom(gatherDisplayLoadInputs(), nowMs, accepted);
    // Fix wave 3 (ruling 9.3): one floor pan for each device sharing the
    // budget, so a cut never pauses every device's display.
    reading.floorPans = m_stationServer->displayBudgetSharingCount();
    const std::optional<DisplayLoadDecision> decision = m_displayGovernor->update(reading);
    if (!decision) {
        return;
    }
    if (publishDisplayBudget(decision)) {
        m_displayGovernor->accept(*decision);
    } else if (const auto published = m_stationServer->configuredDisplayBudgetLimits()) {
        // Refused: the next proposal follows what is published.
        m_displayGovernor->syncGeneration(published->generation);
    }
}

bool StationHost::publishDisplayBudget(const std::optional<DisplayLoadDecision>& decision)
{
    if (!decision || !m_stationServer) {
        return false;
    }
    if (!m_stationServer->setDisplayBudgetLimits(decision->limits, decision->reason)) {
        qCWarning(lcApp) << "DaemonApp: display budget generation"
                          << decision->limits.generation << "was not accepted";
        return false;
    }
    qCInfo(lcApp).nospace() << "DaemonApp: display budget "
                            << (decision->reason == DisplayBudgetReason::CoreBusy
                                    ? "lowered, Core busy" : "restored")
                            << ": " << decision->limits.applicationBytesPerSecond
                            << " bytes/s, " << decision->limits.spectrumSampleUnitsPerSecond
                            << " samples/s (generation " << decision->limits.generation << ")";
    return true;
}

#ifdef NEREUS_BUILD_TESTS
bool StationHost::stationAnnouncedForTest() const
{
    return m_stationAnnouncer && m_stationAnnouncer->isActive();
}

bool StationHost::dnsSdAdvertisedForTest() const
{
    return m_dnsSdAdvertiser && m_dnsSdAdvertiser->isActive();
}

void StationHost::setDnsSdAdvertiserForTest(std::unique_ptr<DnsSdAdvertiser> advertiser)
{
    m_dnsSdAdvertiser = std::move(advertiser);
}
#endif

StationLanPairing StationHost::stationLanPairingFor(const StationServer& server)
{
    // The pairing window (link document section 3.6): an unclaimed Core
    // pairs with one tap on its own network unless pairing_lan_click is
    // deny; a reopened window pairs by code only; a closed one not at all.
    const PairingWindow* window = server.pairingWindow();
    if (server.pairingVersion() < 1 || window == nullptr) {
        return StationLanPairing::Closed;
    }
    switch (window->state()) {
    case PairingWindow::State::OpenUnclaimed:
        return server.pairingLanClickAllowed() ? StationLanPairing::Click
                                               : StationLanPairing::Code;
    case PairingWindow::State::OpenReopened:
        return StationLanPairing::Code;
    case PairingWindow::State::ClosedClaimed:
    case PairingWindow::State::ClosedUnclaimed:
        break;
    }
    return StationLanPairing::Closed;
}

void StationHost::updateStationAnnouncement()
{
    if (!m_stationAnnouncer) { return; }
    if (!m_stationServer || !m_stationServer->isListening() || !m_radioModel) {
        m_stationAnnouncer->stop();
        if (m_dnsSdAdvertiser) { m_dnsSdAdvertiser->stop(); }
        m_stationAnnouncement = {};
        m_dnsSdRecord = {};
        m_dnsSdAttempt.reset();
        return;
    }
    StationLanAnnouncement announcement;
    announcement.controlPort = m_stationServer->serverPort();
    announcement.fingerprint = m_stationServer->certificateFingerprint();
    announcement.coreName = announcementName(m_options.coreName.isEmpty()
        ? QHostInfo::localHostName() : m_options.coreName, QStringLiteral("Nereus Core"));
    announcement.radioConnected = m_radioModel->isConnected();
    announcement.radioName = announcementName(m_radioModel->name().isEmpty()
        ? m_radioModel->model() : m_radioModel->name(),
        announcement.radioConnected ? QStringLiteral("Radio") : QString());
    announcement.radioMac = m_radioModel->currentRadioMac().toUpper();
    if (announcement.radioMac.isEmpty()) { announcement.radioMac = (m_options.selectedRadioMac ? m_options.selectedRadioMac() : QString()).toUpper(); }
    if (announcement.radioMac.isEmpty()) { announcement.radioMac = QStringLiteral("00:00:00:00:00:00"); }
    // iPhone app Task 16: schema 2, the only schema a station sends.
    announcement.schema = kStationLanAnnouncementSchema;
    announcement.identity = m_stationServer->stationIdentity().fingerprint();
    if (const StationDevicesFacade* devices = m_stationServer->devicesFacade()) {
        announcement.claimed = devices->claimed();
        announcement.label = devices->stationLabel();
    } else {
        announcement.claimed = m_stationServer->deviceStore()->isClaimed();
    }
    announcement.pairing = stationLanPairingFor(*m_stationServer);
    // iPhone app Task 71 (ruling 10.4): how many devices hold a place, 0 on
    // a Core no device has claimed. A number only, never who.
    announcement.devicesConnected = m_stationServer->devicesConnectedForDiscovery();
    // iPhone app plan Task 25 (R-IOS-16): the radio connected, offline, or
    // waiting for a choice (StationRadios' reason, RadioModel's
    // stationRadioWaiting), so a list says "Waiting for a radio" before a
    // device connects.
    announcement.radio = announcement.radioConnected ? StationLanRadio::Connected
        : m_radioModel->stationRadioWaiting().isEmpty() ? StationLanRadio::Offline
                                                         : StationLanRadio::Waiting;
    m_stationAnnouncement = announcement;
    m_stationAnnouncer->update(m_stationServer->serverAddress(), announcement);

    // Bonjour follows the announcer's rule: only where the listener serves.
    DnsSdRecord record;
    record.instanceName = dnsSdInstanceName(announcement.displayName());
    record.label = announcement.label;
    record.identity = announcement.identity;
    record.claimed = announcement.claimed;
    record.pairing = announcement.pairing;
    record.devicesConnected = announcement.devicesConnected.value_or(0);
    record.radio = announcement.radio.value_or(StationLanRadio::Offline);
    const std::optional<quint32> where = dnsSdInterfaceForListener(m_stationServer->serverAddress());
    if (!where || !m_dnsSdAdvertiser) {
        if (m_dnsSdAdvertiser) { m_dnsSdAdvertiser->stop(); }
        m_dnsSdRecord = {};
        m_dnsSdAttempt.reset();
        return;
    }
    record.interfaceIndex = *where;
    m_dnsSdRecord = record;
    const quint16 port = m_stationServer->serverPort();
    // Asked once per change: an advertiser that cannot (no Bonjour here, or
    // the platform refused) is not asked again, and logged again, until what
    // it would advertise changes.
    if (m_dnsSdAdvertiser->isActive() || !m_dnsSdAttempt
        || m_dnsSdAttempt->first != port || m_dnsSdAttempt->second != record) {
        m_dnsSdAttempt = std::make_pair(port, record);
        m_dnsSdAdvertiser->start(port, record);
    }
}

void StationHost::attemptStationServerListen()
{
    if (m_quiescing || !m_stationServer || m_stationListenPort == 0 || !m_stationListenArmed) {
        return;
    }
    if (m_stationServer->isListening()) {
        m_stationListenRetryTimer->stop();
        return;
    }

    const QHostAddress bind = listenerAddressFor(m_stationListenBind);
    if (bind.isNull()) {
        // startStationServer validates before latching, so this is only a
        // defensive guard against future mutation. Invalid config never
        // becomes an indefinitely retried bind target.
        qCWarning(lcApp) << "DaemonApp: refusing listener retry for invalid bind"
                          << m_stationListenBind;
        cancelStationServerListenRetry();
        return;
    }

    ++m_stationListenAttemptCount;
    m_listenInProgress = true;
    const bool listening = m_stationServer->listen(bind, m_stationListenPort);
    m_listenInProgress = false;
    if (m_quiescePending) {
        m_quiescePending = false;
        m_quiescing = false;
        quiesce();
    }
    if (m_stopPending && !m_stopping) {
        stop();
        return;
    }
    if (m_quiescing) { return; }
    if (listening) {
        m_stationListenRetryTimer->stop();
        qCInfo(lcApp) << "DaemonApp: remote control listening on wss://"
                       << m_stationListenBind << ":" << m_stationServer->serverPort()
                       << "after" << m_stationListenAttemptCount << "attempt(s)";
        return;
    }

    qCWarning(lcApp) << "DaemonApp: remote control listener attempt"
                      << m_stationListenAttemptCount << "failed on"
                      << m_stationListenBind << m_stationListenPort << ":"
                      << m_stationServer->lastError();
    scheduleStationServerListenRetry();
}

void StationHost::scheduleStationServerListenRetry()
{
    if (m_quiescing || !m_stationServer || m_stationListenPort == 0 || !m_stationListenArmed) {
        return;
    }

    const int delayMs = m_stationListenNextDelayMs;
    qCInfo(lcApp) << "DaemonApp: retrying remote control listener in"
                   << delayMs << "ms on" << m_stationListenBind << m_stationListenPort;
    m_stationListenRetryTimer->start(delayMs);

    if (m_stationListenNextDelayMs < m_stationListenRetryMaximumMs) {
        const qint64 doubled = static_cast<qint64>(m_stationListenNextDelayMs) * 2;
        m_stationListenNextDelayMs = static_cast<int>(
            std::min<qint64>(doubled, m_stationListenRetryMaximumMs));
    }
}

void StationHost::cancelStationServerListenRetry()
{
    if (m_stationListenRetryTimer) {
        m_stationListenRetryTimer->stop();
    }
    m_stationListenBind.clear();
    m_stationListenArmed = false;
    m_stationListenPort = 0;
    m_stationListenNextDelayMs = m_stationListenRetryInitialMs;
}


StationReach StationHost::reach() const
{
    StationReach reach;
    reach.listening = listenerReady();
    reach.listenerRetryPending = listenerRetryPending();
    if (m_dnsSdAdvertiser) {
        reach.bonjourAvailable = m_dnsSdAdvertiser->isAvailable();
        reach.bonjourActive = m_dnsSdAdvertiser->isActive();
    }
    const QList<QUrl> servers = RendezvousClient::serverUrls(m_options.rendezvousServers);
    reach.serviceConfigured = !servers.isEmpty();
    if (m_rendezvous && m_rendezvous->client()) {
        reach.serviceRegistered = m_rendezvous->client()->isRegistered();
        reach.serviceHost = m_rendezvous->client()->currentServer().host();
    }
    if (reach.serviceHost.isEmpty() && !servers.isEmpty()) {
        reach.serviceHost = servers.first().host();
    }
    return reach;
}

bool StationHost::listenerReady() const
{
    return m_stationServer && m_stationServer->isListening();
}

bool StationHost::listenerRetryPending() const
{
    return m_stationListenRetryTimer && m_stationListenRetryTimer->isActive();
}

#ifdef NEREUS_BUILD_TESTS
void StationHost::setListenRetryIntervalsForTest(int initialMs, int maximumMs)
{
    m_stationListenRetryInitialMs = initialMs > 0 ? initialMs : 1;
    m_stationListenRetryMaximumMs = maximumMs >= m_stationListenRetryInitialMs
        ? maximumMs : m_stationListenRetryInitialMs;
}

void StationHost::setDisplayLoadSourcesForTest(std::function<DisplayLoadInputs()> inputs,
                                               std::function<qint64()> clock,
                                               std::function<DisplayBudgetCharge()> accepted)
{
    m_displayLoadInputsForTest = std::move(inputs);
    m_displayGovernorNowForTest = std::move(clock);
    m_acceptedDisplayChargeForTest = std::move(accepted);
}

#endif

} // namespace NereusSDR
