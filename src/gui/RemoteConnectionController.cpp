// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#include "RemoteConnectionController.h"
#include "core/AppSettings.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/RendezvousClient.h"
#include "core/session/StationClient.h"
#include "core/session/PathRacer.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QSignalBlocker>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

namespace NereusSDR {

namespace {

// iPhone app plan Task 29 (R-IOS-16): the remote access service's servers
// this computer uses (RendezvousClient::serverUrls entries): AppSettings
// RemoteAccessServers, comma separated, or when unset the default a Core's
// rendezvous_servers has (RendezvousClient::kDefaultServer).
QStringList remoteAccessServerEntries()
{
    const QString saved = AppSettings::instance()
                              .value(QStringLiteral("RemoteAccessServers"), QString())
                              .toString()
                              .trimmed();
    if (saved.isEmpty()) {
        return {QString::fromLatin1(RendezvousClient::kDefaultServer)};
    }
    QStringList entries;
    for (const QString& entry : saved.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (!entry.trimmed().isEmpty()) {
            entries.append(entry.trimmed());
        }
    }
    return entries;
}

} // namespace

QList<QUrl> configuredRemoteAccessServers()
{
    return RendezvousClient::serverUrls(remoteAccessServerEntries());
}

RemoteConnectionController::RemoteConnectionController(
    StationClient* client, RadioModel* model, RemoteStationOptions options, QObject* parent)
    : QObject(parent), m_client(client), m_model(model), m_options(std::move(options))
{
    connect(client, &StationClient::connectionActivityChanged,
            this, &RemoteConnectionController::changed);
    connect(client, &StationClient::handshakeComplete, this, [this] {
        m_operatorDisconnected = false;
        m_pendingMediaRecoveryEpoch = 0;
        m_retryAttempt = 0;
        // iPhone app plan Task 29: what this sign-in told of reaching the
        // Core through the internet service, for this window's next
        // connect (the saved Core records it too).
        if (m_client) {
            if (!m_client->stationRendezvousId().isEmpty()) {
                m_options.rendezvousId = m_client->stationRendezvousId();
            }
            if (m_client->capabilities().relayAllowedEntry) {
                m_options.relayAllowed = m_client->capabilities().relayAllowed ? 1 : 0;
            }
        }
        emit changed();
    });
    connect(client, &StationClient::stateSnapshotApplied, this, [this] {
        if (!m_client || !m_client->isHandshakeComplete()) { return; }
        m_options.controlChannelVersion = m_client->capabilities().controlChannelVersion;
        m_options.negativeControlObservedMs = m_options.controlChannelVersion == 0
            ? QDateTime::currentMSecsSinceEpoch() : -1;
    });
    connect(client, &StationClient::sessionEnded, this, [this](const QString&) {
        m_pendingMediaRecoveryEpoch = 0;
        emit changed();
    });
    // iPhone app Task 18: once this computer's key is enrolled with the
    // Core, a later Connect signs in by key as well.
    // iPhone app plan Task 78 item 7 (G-53): the Core is full and asks
    // which device this window replaces; the status line says so.
    connect(client->remoteDevices(), &RemoteDevicesState::heldChanged, this,
            &RemoteConnectionController::changed);
    connect(client, &StationClient::stationIdentityLearned, this,
            [this](const QByteArray& identity) { m_options.identityFingerprint = identity; });
    connect(client, &StationClient::reconnectScheduled, this,
            [this](int attempt, int delayMs) {
        m_retryAttempt = attempt;
        m_retryDelayMs = delayMs;
        emit changed();
    });
    connect(model, &RadioModel::connectionStateChanged,
            this, &RemoteConnectionController::changed);
    connect(model, &RadioModel::infoChanged,
            this, &RemoteConnectionController::changed);
    connect(model, &RadioModel::receiveLayoutRestoreStatusChanged,
            this, &RemoteConnectionController::changed);
    connect(client, &StationClient::stateSnapshotApplied,
            this, &RemoteConnectionController::changed);
}

QString RemoteConnectionController::endpointText() const
{
    if ((!m_client || !m_client->isHandshakeComplete())
        && m_options.url.isEmpty() && !m_options.rendezvousId.isEmpty()) {
        return m_options.hasAuthenticatedDirectAddresses() ? tr("No configured address") : tr("Remote access");
    }
    const QUrl url(m_client && m_client->isHandshakeComplete()
                       ? m_client->connectedUrl() : QUrl(m_options.url));
    if (m_client && m_client->isHandshakeComplete() && url.isEmpty()) {
        if (m_client->pathRank() == PathRacer::ServiceRelayed || m_client->pathRank() == PathRacer::Floor) {
            return tr("Remote access relay (no direct Core address)");
        }
        return tr("Remote access service (no direct Core address)");
    }
    // Never display URL user-info/query/fragment or the pairing token.
    QString host = url.host();
    if (host.contains(QLatin1Char(':'))) { host = QLatin1Char('[') + host + QLatin1Char(']'); }
    return url.port() >= 0 ? host + QLatin1Char(':') + QString::number(url.port()) : host;
}

std::optional<RemoteStationOptions> RemoteConnectionController::currentOptions() const
{
    if (!m_currentOptionsSource) { return m_options; }
    const auto current = m_currentOptionsSource();
    if (!current || QUrl(current->url) != QUrl(m_options.url) || current->token != m_options.token
        || current->fingerprint != m_options.fingerprint || current->allowUnpinned != m_options.allowUnpinned
        || current->identityFingerprint != m_options.identityFingerprint
        || current->reachFromAnywhere != m_options.reachFromAnywhere) { return std::nullopt; }
    return current;
}

bool RemoteConnectionController::canConnect() const
{
    const QPointer<const RemoteConnectionController> self(this);
    const auto reasonSource = m_admissionUnavailableReasonSource;
    const QString admission = reasonSource ? reasonSource() : QString();
    if (!self || !admission.isEmpty()) { return false; }
    const auto options = currentOptions();
    if (!m_client || !options || !options->isValidRemoteTarget() || m_client->isConnectionActive()) {
        return false;
    }
    return !options->url.isEmpty() || options->hasAuthenticatedDirectAddresses()
        || (options->reachFromAnywhere && options->serviceConnectRefusal().isEmpty()
            && !configuredRemoteAccessServers().isEmpty());
}

bool RemoteConnectionController::canDisconnect() const
{
    return m_client && m_client->isConnectionActive();
}

ConnectionState RemoteConnectionController::state() const
{
    if (!m_client) { return ConnectionState::Disconnected; }
    if (m_client->isHandshakeComplete()) { return ConnectionState::Connected; }
    if (m_client->isReconnectPending()) { return ConnectionState::LinkLost; }
    if (m_client->isConnectionActive()) { return ConnectionState::Connecting; }
    return ConnectionState::Disconnected;
}

QString RemoteConnectionController::statusText() const
{
    switch (state()) {
    case ConnectionState::Connected: return tr("Core connected");
    case ConnectionState::Connecting:
    case ConnectionState::Probing:
        if (m_client && m_client->remoteDevices()->held()) {
            return tr("Core full, choose a device to replace");
        }
        return tr("Connecting to Core");
    case ConnectionState::LinkLost:
        // The Core restarted to change its radio: a reconnect, not a fault.
        if (m_client && !m_client->radioChangeReason().isEmpty()) {
            return tr("Core changing radio, reconnecting");
        }
        return tr("Retrying Core (attempt %1)").arg(m_retryAttempt);
    case ConnectionState::Disconnected:
        if (stopNotice() != CoreStopNotice::None) { return stopTitle(); }
        return m_operatorDisconnected ? tr("Core disconnected")
             : m_client && !m_client->lastError().isEmpty() ? tr("Core connection failed")
             : tr("Core disconnected");
    }
    return {};
}

QString RemoteConnectionController::radioText() const
{
    if (state() != ConnectionState::Connected) { return tr("Radio state unavailable"); }
    if (!m_model || !m_model->isConnected()) { return tr("Radio offline"); }
    const QString name = m_model->name().isEmpty() ? m_model->model() : m_model->name();
    return name.isEmpty() ? tr("Radio connected") : tr("Radio: %1").arg(name);
}

QString RemoteConnectionController::detailText() const
{
    QString text = tr("Core: %1\n%2\n%3")
        .arg(endpointText(), statusText(), radioText());
    const QPointer<const RemoteConnectionController> self(this);
    const auto reasonSource = m_admissionUnavailableReasonSource;
    const QString admissionReason = reasonSource ? reasonSource() : QString();
    if (!self) { return text; }
    if (!admissionReason.isEmpty()) { text += QLatin1Char('\n') + admissionReason; }
    if (m_options.url.isEmpty()) {
        if (!m_options.reachFromAnywhere) {
            text += tr("\nTurn on remote access for this Core in Connections to connect.");
        } else if (!m_options.serviceConnectRefusal().isEmpty()) {
            text += QLatin1Char('\n') + m_options.serviceConnectRefusal();
        } else if (configuredRemoteAccessServers().isEmpty()) {
            text += tr("\nRemote access servers are unavailable. Check this computer's remote access setting.");
        }
    }
    if (state() == ConnectionState::Connected && m_model
        && !m_model->receiveLayoutRestoreMessage().isEmpty()) {
        text += tr("\nReceivers: %1").arg(m_model->receiveLayoutRestoreMessage());
    }
    if (m_client && m_client->isReconnectPending()) {
        text += tr("\nRetry delay: %1 s. Disconnect cancels automatic retries.")
            .arg((m_retryDelayMs + 999) / 1000);
    }
    if (stopNotice() != CoreStopNotice::None) {
        // R-R3-38: the stop message's own words, not the raw reason.
        text += QLatin1Char('\n') + stopText();
    } else if (m_client && !m_client->isHandshakeComplete()
               && !m_client->radioChangeReason().isEmpty()) {
        // The operator's ruling of 2026-09-26: the Core's own words (which
        // radio it switches to), then this window's.
        text += tr("\n%1\nThis window reconnects by itself when the Core is back.")
                    .arg(m_client->radioChangeReason());
    } else if (m_client && !m_client->isHandshakeComplete() && !m_operatorDisconnected
        && !m_client->lastError().isEmpty()) {
        // The raw reason is in the log; shown here in user words (R-R3-17).
        text += tr("\nLast failure: %1")
                    .arg(OperatorReasonText::forDisplay(m_client->lastError()));
    }
    // iPhone app plan Task 27 (R-IOS-16): what this computer tried, path by
    // path, while it is not connected.
    if (m_client && !m_client->isHandshakeComplete() && !m_operatorDisconnected
        && !m_client->connectionAttempt().summary().isEmpty()) {
        text += QLatin1Char('\n') + m_client->connectionAttempt().summary();
    }
    return text;
}

CoreStopNotice RemoteConnectionController::stopNotice() const
{
    // Only a window that stopped by itself: an operator's Disconnect, a
    // live or retrying link and a window never connected show nothing.
    if (!m_client || m_operatorDisconnected || state() != ConnectionState::Disconnected) {
        return CoreStopNotice::None;
    }
    const StationEndReport report = m_client->lastEndReport();
    switch (report.kind) {
    case StationEndReport::Kind::None: return CoreStopNotice::None;
    case StationEndReport::Kind::TakenOver: return CoreStopNotice::TakenOver;
    case StationEndReport::Kind::VersionRefused:
        if (report.appMajor >= 0 && report.coreMajor >= 0) {
            if (report.appMajor < report.coreMajor) { return CoreStopNotice::UpdateThisApp; }
            if (report.appMajor > report.coreMajor) { return CoreStopNotice::UpdateCore; }
        }
        return CoreStopNotice::UpdateOlderSide;
    case StationEndReport::Kind::Refused: return CoreStopNotice::Refused;
    case StationEndReport::Kind::DeviceRemoved: return CoreStopNotice::DeviceRemoved;
    case StationEndReport::Kind::PairingRequired: return CoreStopNotice::PairingRequired;
    case StationEndReport::Kind::IdentityChanged: return CoreStopNotice::IdentityChanged;
    }
    return CoreStopNotice::None;
}

QString RemoteConnectionController::stopTitle() const
{
    switch (stopNotice()) {
    case CoreStopNotice::None: return {};
    case CoreStopNotice::TakenOver: return tr("Core taken over");
    case CoreStopNotice::UpdateThisApp: return tr("Update this app");
    case CoreStopNotice::UpdateCore: return tr("Update the Core");
    case CoreStopNotice::UpdateOlderSide: return tr("Core version does not match");
    case CoreStopNotice::Refused: return tr("Core refused this window");
    case CoreStopNotice::DeviceRemoved: return tr("Removed from the Core");
    case CoreStopNotice::PairingRequired: return tr("Pair with the Core");
    case CoreStopNotice::IdentityChanged: return tr("Core not recognized");
    }
    return {};
}

QString RemoteConnectionController::stopText() const
{
    const QString noRetry = tr("This window does not reconnect by itself.");
    const QString reason = m_client ? m_client->lastEndReport().reason : QString();
    switch (stopNotice()) {
    case CoreStopNotice::None: return {};
    case CoreStopNotice::TakenOver: {
        // iPhone app plan Task 78 item 3 (G-53): a Core that let a fifth
        // device take this window's place names it and says when. An
        // older Core names the other app by its network address only.
        const StationEndReport report = m_client->lastEndReport();
        const QString at = report.endedAt.isValid()
            ? QLocale().toString(report.endedAt.time(), QLocale::ShortFormat)
            : QString();
        QString what;
        if (!report.takenOverByName.isEmpty()) {
            what = at.isEmpty()
                ? tr("%1 took this window's place on the Core.").arg(report.takenOverByName)
                : tr("%1 took this window's place on the Core at %2.")
                      .arg(report.takenOverByName, at);
        } else if (!report.takenOverBy.isEmpty()) {
            what = tr("Another app at %1 connected to the Core and took over.")
                       .arg(report.takenOverBy);
        } else {
            what = tr("Another app connected to the Core and took over.");
        }
        return what + QLatin1Char(' ') + noRetry + QLatin1Char(' ')
             + tr("Take it back to use the Core here again.");
    }
    case CoreStopNotice::UpdateThisApp:
        return tr("This app is too old to work with this Core. Update NereusSDR on "
                  "this computer to use it.") + QLatin1Char(' ') + noRetry;
    case CoreStopNotice::UpdateCore:
        return tr("This Core is too old to work with this app. Update NereusSDR on "
                  "the Core's computer to use it.") + QLatin1Char(' ') + noRetry;
    case CoreStopNotice::UpdateOlderSide:
    case CoreStopNotice::Refused:
        // The Core's own reason in user words (R-R3-17, R-R3-21).
        return OperatorReasonText::forDisplay(reason) + QLatin1Char(' ') + noRetry;
    // iPhone app Task 18: chosen by the end's code, so the words are this
    // app's own and the same whichever Core version sent the end.
    case CoreStopNotice::DeviceRemoved:
        return tr("The Core no longer has this computer among its paired devices. "
                  "Pair this computer with the Core again to use it here.")
             + QLatin1Char(' ') + noRetry;
    case CoreStopNotice::PairingRequired:
        return tr("The Core now signs in paired devices only. Pair this computer "
                  "with the Core to use it here.")
             + QLatin1Char(' ') + noRetry;
    case CoreStopNotice::IdentityChanged:
        // This app's own words: it refused the Core before sending anything.
        return OperatorReasonText::forDisplay(reason) + QLatin1Char(' ') + noRetry;
    }
    return {};
}

bool RemoteConnectionController::offersTakeBack() const
{
    return stopNotice() == CoreStopNotice::TakenOver;
}

bool RemoteConnectionController::updateThisAppHelps() const
{
    const CoreStopNotice notice = stopNotice();
    return notice == CoreStopNotice::UpdateThisApp || notice == CoreStopNotice::UpdateOlderSide;
}

void RemoteConnectionController::takeBack()
{
    // R-R3-38: connects again. A full Core then asks which device this
    // window replaces, starting on the one that took its place (iPhone
    // app plan Task 78 item 3, G-53); nothing is taken without that
    // choice.
    connectToStation();
}

void RemoteConnectionController::connectToStation()
{
    // Materialize retained listeners before readiness, including manual-only Cores.
    const auto options = currentOptions();
    if (!options) { return; }
    m_options = *options;
    if (!canConnect()) { return; }
    m_operatorDisconnected = false;
    m_retryAttempt = 0;
    const QPointer<RemoteConnectionController> self(this);
    m_client->setCandidateSource([self]() -> std::optional<StationClient::ConnectionCandidates> {
        if (!self) { return std::nullopt; }
        const auto current = self->currentOptions();
        if (!current || !current->isValidRemoteTarget()) { return std::nullopt; }
        StationClient::ConnectionCandidates candidates;
        QStringList addresses = current->cachedAddresses;
        if (current->identityFingerprint.size() == 32 && !current->allowUnpinned) {
            addresses = current->directCandidates + addresses + current->coreAddresses;
        }
        for (const QString& address : addresses) {
            const QUrl url(address);
            if (RemoteStationOptions::isValidStationUrl(address) && !candidates.addresses.contains(url)) {
                candidates.addresses.append(url);
            }
        }
        if (current->identityFingerprint.size() == 32 && !current->allowUnpinned
            && current->reachFromAnywhere && !current->rendezvousId.isEmpty()) {
            candidates.service.servers = configuredRemoteAccessServers();
            candidates.service.rendezvousId = current->rendezvousId;
            candidates.service.relayAllowed = current->relayAllowed != 0;
            candidates.service.controlChannelVersion = current->effectiveControlChannelVersion(
                QDateTime::currentMSecsSinceEpoch());
            candidates.service.currentControlChannelVersion = [self] {
                const auto fresh = self ? self->currentOptions() : std::nullopt;
                return fresh ? fresh->effectiveControlChannelVersion(QDateTime::currentMSecsSinceEpoch()) : 0;
            };
        }
        return candidates;
    });
    // Legacy pinned/token clients retain their existing ordered address behavior.
    QList<QUrl> cached;
    for (const QString& address : m_options.cachedAddresses) { cached.append(QUrl(address)); }
    m_client->setCachedAddresses(cached);
    m_client->connectToStation(QUrl(m_options.url), m_options.token, m_options.fingerprint,
                               m_options.allowUnpinned, m_options.identityFingerprint);
    emit changed();
}

void RemoteConnectionController::setCurrentOptionsSource(CurrentOptionsSource source)
{
    m_currentOptionsSource = std::move(source);
}

void RemoteConnectionController::disconnectFromStation()
{
    if (!m_client) { return; }
    // Latch before synchronous teardown emits state and retained-radio signals.
    m_operatorDisconnected = true;
    m_pendingMediaRecoveryEpoch = 0;
    // iPhone app plan Task 78: the operator's Disconnect is leaving on
    // purpose (the several-devices design, section 4.6): the Core frees
    // this window's place, and transmit, now rather than after 3 minutes.
    m_client->leaveSession();
    m_client->disconnectFromStation(QStringLiteral("operator disconnect"));
    emit changed();
    emit operatorDisconnected();
}

void RemoteConnectionController::recoverMediaSession(quint32 expectedEpoch,
                                                      const QString& reason)
{
    if (!m_client || m_operatorDisconnected || expectedEpoch == 0
        || !m_client->isHandshakeComplete()
        || m_client->sessionEpoch() != expectedEpoch
        || m_pendingMediaRecoveryEpoch == expectedEpoch) {
        return;
    }
    m_pendingMediaRecoveryEpoch = expectedEpoch;
    QMetaObject::invokeMethod(this, [this, expectedEpoch, reason] {
        if (m_pendingMediaRecoveryEpoch != expectedEpoch) { return; }
        m_pendingMediaRecoveryEpoch = 0;
        if (!m_client || m_operatorDisconnected
            || !m_client->isHandshakeComplete()
            || m_client->sessionEpoch() != expectedEpoch) {
            return;
        }
        m_client->disconnectFromStation(
            reason.isEmpty() ? QStringLiteral("station media connection failed") : reason,
            true);
    }, Qt::QueuedConnection);
}

RemoteConnectionPanel::RemoteConnectionPanel(RemoteConnectionController* controller,
                                           QWidget* parent, RemoteMediaController* media)
    : QDialog(parent)
{
    setWindowTitle(tr("Core connection"));
    auto* layout = new QVBoxLayout(this);
    auto* details = new QLabel(this);
    details->setObjectName(QStringLiteral("coreConnectionDetails"));
    details->setTextFormat(Qt::PlainText);
    details->setWordWrap(true);
    details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(details);

    // The "Remote audio" section: only with a media controller, so a panel
    // built without one (there is no such call site today, but tests build
    // one directly) stays exactly as it was before R-R3-23.
    QLabel* audioDetails = nullptr;
    QPushButton* retryButton = nullptr;
    QComboBox* qualityChoice = nullptr;
    if (media) {
        audioDetails = new QLabel(this);
        audioDetails->setObjectName(QStringLiteral("remoteAudioDetails"));
        audioDetails->setTextFormat(Qt::PlainText);
        audioDetails->setWordWrap(true);
        audioDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(audioDetails);
        // R-R3-23: the operator's audio quality choice, stored on this
        // computer. What the Core actually runs, and why when it is not the
        // choice, is in the section text above.
        auto* qualityRow = new QHBoxLayout;
        auto* qualityLabel = new QLabel(tr("Audio quality:"), this);
        qualityChoice = new QComboBox(this);
        qualityChoice->setObjectName(QStringLiteral("remoteAudioQuality"));
        qualityChoice->addItem(tr("Opus"), QVariant::fromValue(int(RemoteAudioProfile::Opus)));
        qualityChoice->addItem(tr("Lossless"), QVariant::fromValue(int(RemoteAudioProfile::Lossless)));
        qualityChoice->setToolTip(tr("Opus is compressed and needs 24 to 48 kbit/s. Lossless "
                                     "plays the Core's audio unchanged, for digital modes, "
                                     "and needs about 1.6 Mbit/s. If the network cannot carry "
                                     "it, audio stays on Opus. Saved on this computer."));
        qualityLabel->setBuddy(qualityChoice);
        qualityRow->addWidget(qualityLabel);
        qualityRow->addWidget(qualityChoice, 1);
        layout->addLayout(qualityRow);
        qualityChoice->setCurrentIndex(
            qualityChoice->findData(int(media->audioProfileChoice())));
        QPointer<RemoteMediaController> choiceMedia(media);
        connect(qualityChoice, &QComboBox::currentIndexChanged, this,
                [qualityChoice, choiceMedia](int index) {
            if (!choiceMedia || index < 0) { return; }
            choiceMedia->setAudioProfileChoice(
                static_cast<RemoteAudioProfile>(qualityChoice->itemData(index).toInt()));
        });
        retryButton = new QPushButton(tr("Retry audio"), this);
        retryButton->setObjectName(QStringLiteral("retryRemoteAudio"));
        retryButton->setAutoDefault(false);
        layout->addWidget(retryButton);
        connect(retryButton, &QPushButton::clicked,
                media, &RemoteMediaController::retryAudio);
    }

    auto* buttons = new QDialogButtonBox(this);
    auto* dial = buttons->addButton(tr("Connect"), QDialogButtonBox::ActionRole);
    dial->setObjectName(QStringLiteral("connectCore"));
    auto* stop = buttons->addButton(tr("Disconnect"), QDialogButtonBox::ActionRole);
    stop->setObjectName(QStringLiteral("disconnectCore"));
    auto* close = buttons->addButton(QDialogButtonBox::Close);
    for (auto* button : {dial, stop, close}) { button->setAutoDefault(false); }
    layout->addWidget(buttons);
    connect(dial, &QPushButton::clicked, controller, &RemoteConnectionController::connectToStation);
    connect(stop, &QPushButton::clicked, controller, &RemoteConnectionController::disconnectFromStation);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    const auto refresh = [this, controller, details, dial, stop] {
        const QString text = controller->detailText();
        const bool textChanged = details->text() != text;
        details->setText(text);
        dial->setEnabled(controller->canConnect());
        stop->setEnabled(controller->canDisconnect());
        if (textChanged) { fitHeightToContent(); }
    };
    connect(controller, &RemoteConnectionController::changed, this, refresh);
    refresh();

    if (media) {
        QPointer<RemoteMediaController> guardedMedia(media);
        m_refreshAudio = [this, guardedMedia, audioDetails, retryButton, qualityChoice] {
            if (!guardedMedia) { return; }
            // R-R3-43: and each receiver stream apps on this computer use.
            const QString text = formatRemoteAudioDetails(guardedMedia->audioStatus(),
                                                          guardedMedia->audioTelemetry(),
                                                          guardedMedia->audioDelay(),
                                                          guardedMedia->receiverAudioTelemetry());
            const bool textChanged = audioDetails->text() != text;
            audioDetails->setText(text);
            retryButton->setEnabled(guardedMedia->audioStatus().retryAvailable);
            {
                const QSignalBlocker blocker(qualityChoice);
                qualityChoice->setCurrentIndex(
                    qualityChoice->findData(int(guardedMedia->audioProfileChoice())));
            }
            if (textChanged) { fitHeightToContent(); }
        };
        connect(media, &RemoteMediaController::audioStatusChanged, this,
                [this] { m_refreshAudio(); });
        // audioStatusChanged() fires only when the derived status changes;
        // the numeric health measurements move continuously while playing,
        // so this section also polls once a second, but only while the
        // panel is shown (showEvent / hideEvent start and stop it).
        m_audioTimer = new QTimer(this);
        m_audioTimer->setObjectName(QStringLiteral("remoteAudioPanelTimer"));
        m_audioTimer->setInterval(1000);
        connect(m_audioTimer, &QTimer::timeout, this, [this] { m_refreshAudio(); });
        m_refreshAudio();
    }

    // Width is a starting size the operator may change; the height always
    // follows the wrapped text, so nothing is clipped at larger fonts and no
    // fixed gap is left below short content.
    resize(440, height());
    fitHeightToContent();
}

void RemoteConnectionPanel::fitHeightToContent()
{
    QLayout* const top = layout();
    if (!top) { return; }
    top->activate();
    const int contentHeight = top->totalHeightForWidth(width());
    if (contentHeight > 0) {
        resize(width(), contentHeight);
    } else {
        adjustSize();
    }
}

void RemoteConnectionPanel::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_audioTimer) {
        m_refreshAudio();
        m_audioTimer->start();
    }
    fitHeightToContent();
}

void RemoteConnectionPanel::hideEvent(QHideEvent* event)
{
    if (m_audioTimer) {
        m_audioTimer->stop();
    }
    QDialog::hideEvent(event);
}

CoreStopBanner::CoreStopBanner(RemoteConnectionController* controller, QWidget* parent)
    : QFrame(parent), m_controller(controller)
{
    setObjectName(QStringLiteral("coreStopBanner"));
    // The same panel and amber accent as the window's warning notices.
    setStyleSheet(QStringLiteral(
        "QFrame#coreStopBanner { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 3px; }"
        "QLabel { border: none; background: transparent; }")
        .arg(QString::fromLatin1(Style::kPanelBg), QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kAmberText)));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(6);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("coreStopTitle"));
    m_title->setTextFormat(Qt::PlainText);
    m_title->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: bold;")
                               .arg(QString::fromLatin1(Style::kTextPrimary)));
    layout->addWidget(m_title);
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("coreStopText"));
    m_text->setTextFormat(Qt::PlainText);
    m_text->setWordWrap(true);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_text->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                              .arg(QString::fromLatin1(Style::kTextPrimary)));
    layout->addWidget(m_text);

    auto* row = new QHBoxLayout;
    row->addStretch(1);
    m_takeBack = new QPushButton(tr("Take it back"), this);
    m_takeBack->setObjectName(QStringLiteral("coreStopTakeBack"));
    m_chooseCore = new QPushButton(tr("Choose another Core"), this);
    m_chooseCore->setObjectName(QStringLiteral("coreStopChooseCore"));
    m_checkUpdates = new QPushButton(tr("Check for updates"), this);
    m_checkUpdates->setObjectName(QStringLiteral("coreStopCheckUpdates"));
    for (QPushButton* button : {m_takeBack, m_chooseCore, m_checkUpdates}) {
        button->setStyleSheet(Style::buttonBaseStyle());
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    layout->addLayout(row);

    connect(m_takeBack, &QPushButton::clicked, this, [this] {
        if (m_controller) { m_controller->takeBack(); }
    });
    connect(m_chooseCore, &QPushButton::clicked, this,
            &CoreStopBanner::chooseAnotherCoreRequested);
    connect(m_checkUpdates, &QPushButton::clicked, this,
            &CoreStopBanner::checkForUpdatesRequested);
    if (controller) {
        connect(controller, &RemoteConnectionController::changed, this, &CoreStopBanner::refresh);
    }
    refresh();
}

void CoreStopBanner::setChooseAnotherCoreAvailable(bool available)
{
    m_chooseAvailable = available;
    refresh();
}

void CoreStopBanner::setCheckForUpdatesAvailable(bool available)
{
    m_updatesAvailable = available;
    refresh();
}

void CoreStopBanner::refresh()
{
    const bool shown = m_controller && m_controller->stopNotice() != CoreStopNotice::None;
    if (shown) {
        m_title->setText(m_controller->stopTitle());
        m_text->setText(m_controller->stopText());
        m_takeBack->setVisible(m_controller->offersTakeBack());
        m_chooseCore->setVisible(m_chooseAvailable);
        m_checkUpdates->setVisible(m_updatesAvailable && m_controller->updateThisAppHelps());
    }
    setVisible(shown);
    if (shown) { raise(); }
    emit contentChanged();
}
} // namespace NereusSDR
