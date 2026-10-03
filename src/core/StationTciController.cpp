// no-port-check: NereusSDR-original. R-R3-48 / R-R3-25 the Core's station TCI server.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 follow-up: a listener that cannot start is retried
// with a bounded backoff and a plain reason. J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework: the station address and this computer bound
// separately; only the blocked one retried, named in the reason. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework follow-up: the first listen failure logged
// once. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: Lane B takes integration (R-IOS-01, R-R3-21): the blocked-port error
// says the Core's address and the Core's TCI server. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-25: iPhone app Task 73 (R-IOS-02, ruling 5.13): the Core's server
// changes only the station device's own slices. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-29: JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the
// rest of the TCI Server page's settings. J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code.
// 2026-09-27: Parity Task 23 (R-R3-48, R-R3-42, R-R3-49): the apps, their
// disconnect and the four options. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
// 2026-09-28: slice control plan Task 2: the write gate is SliceAccessPolicy's;
// a slice nobody controls but devices listen to is refused. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "core/StationTciController.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/StationNetwork.h"
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/TciClientSession.h"
#include "core/TciServer.h"
#include "models/RadioModel.h"

#include <QWebSocket>

#include <algorithm>
#include <iterator>

namespace NereusSDR {

StationTciController::StationTciController(RadioModel* radio, StationTciModel* model,
                                           QObject* parent)
    : QObject(parent), m_radio(radio), m_model(model)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &StationTciController::apply);
#ifdef HAVE_WEBSOCKETS
    // The Core's own server on the Core's radio model (a Local model, so
    // vfo:, split_enable:, audio and I/Q come from the Core's receivers).
    //
    // TCI compatibility keys (the carry note of the remote window Setup
    // plan): the Core seeds none of TciEmulateExpertSDR3Protocol,
    // TciEmulateSunSDR2Pro, TciCwluBecomesCw, TciCwBecomesCwuAbove10mhz or
    // the other Tci* keys, and relies on their readers' defaults on
    // purpose (the two emulation keys read True, as in a fresh window).
    // A Core whose settings file still holds Tci values from before
    // R-R3-42 honours those, as the operator's own earlier choices. Since
    // parity Task 23 a remote window changes four of them
    // (setStationTciOptions, setOptions below).
    m_server = std::make_unique<TciServer>(radio);
    m_server->setStationReceiveOnly(true);
    // iPhone app Task 73 (ruling 5.13): it reads every slice (trx:N is
    // slice N) and changes only the station device's own: the slices it
    // runs held for an absent device, and, before any device is on the
    // Core, the slices nobody owns yet. A device's slice changes only on
    // that device. Slice control plan Task 2 (SliceAccessPolicy): a slice
    // with no controller that devices still listen to is nobody's to
    // change until one of them takes control.
    m_server->setSliceWriteGate([radio = QPointer<RadioModel>(radio)](int sliceId) {
        if (!radio) {
            return false;
        }
        const SliceOwnership& ownership = *radio->sliceOwnership();
        return SliceAccessPolicy::mayChange(ownership, SliceOwnership::stationDevice(), sliceId)
            || SliceAccessPolicy::stationMayChangeUnclaimed(ownership, sliceId);
    });
    // Rework follow-up 2: this controller logs the station listener's
    // state changes itself (once each); the server's per-attempt listen,
    // start and stop lines go to debug, so nothing is logged twice.
    m_server->setQuietListenAttempts(true);
    connect(m_server.get(), &TciServer::errorOccurred, this, [this](const QString& error) {
        // The raw socket error is for the log; the object carries plain
        // words (apply()).
        m_error = error;
    });
    connect(m_server.get(), &TciServer::operatorNotice, this,
            [](const QString& peer, const QString& reason, bool) {
        qCInfo(lcTci) << "Station TCI server:" << reason << "(app" << peer << ")";
    });
    // Parity Task 23 (stationTciVersion 2): the apps, as the `tciClients`
    // stream. An app arriving or leaving, and each line an app sends (its
    // last command, and what it subscribed to), change the list; changes in
    // one turn of the event loop are published once.
    const auto clientsChanged = [this] { publishClients(); };
    connect(m_server.get(), &TciServer::clientConnected, this, clientsChanged);
    connect(m_server.get(), &TciServer::clientDisconnected, this, clientsChanged);
    connect(m_server.get(), &TciServer::serverStopped, this, clientsChanged);
    connect(m_server.get(), &TciServer::txAudioActiveClientChanged, this, clientsChanged);
    connect(m_server.get(), &TciServer::messageLogged, this,
            [this](const QString& direction, const QString&, const QString&, qint64) {
        if (direction == QLatin1String("in")) {
            publishClients();
        }
    });
#endif
}

StationTciController::~StationTciController()
{
#ifdef HAVE_WEBSOCKETS
    if (m_server) {
        m_server->stop();
    }
#endif
}

// static
QString StationTciController::blockedReason(quint16 port, const QList<QHostAddress>& blocked)
{
    bool thisComputer = false;
    QStringList stationAddresses;
    for (const QHostAddress& address : blocked) {
        if (address.isLoopback() || address == QHostAddress(QHostAddress::AnyIPv4)
            || address == QHostAddress(QHostAddress::AnyIPv6)
            || address == QHostAddress(QHostAddress::Any)) {
            thisComputer = true;
        } else {
            stationAddresses.append(address.toString());
        }
    }
    if (thisComputer && stationAddresses.isEmpty()) {
        return QStringLiteral("Another program on the Core's computer is using port %1, so "
                              "apps there cannot reach the Core's TCI server. The Core keeps "
                              "trying.").arg(port);
    }
    if (!thisComputer) {
        return QStringLiteral("Another program is using port %1 at the Core's address %2, so "
                              "devices on the radio's network cannot reach the Core's TCI "
                              "server. "
                              "The Core keeps trying.").arg(port).arg(stationAddresses.join(QStringLiteral(", ")));
    }
    return QStringLiteral("Another program is using port %1 on the Core's computer and at the "
                          "Core's address %2, so the Core's TCI server cannot start. The "
                          "Core keeps trying.").arg(port).arg(stationAddresses.join(QStringLiteral(", ")));
}

void StationTciController::resetRetry()
{
    m_retryTimer.stop();
    m_retryStep = 0;
}

TciServer* StationTciController::server() const
{
#ifdef HAVE_WEBSOCKETS
    return m_server.get();
#else
    return nullptr;
#endif
}

void StationTciController::setBindOverride(const QString& address)
{
    const QString trimmed = address.trimmed();
    if (trimmed == m_bind.bindOverride) {
        return;
    }
    m_bind.bindOverride = trimmed;
    apply();
}

void StationTciController::setRadioAddress(const QHostAddress& radio)
{
    if (radio == m_bind.radio) {
        return;
    }
    m_bind.radio = radio;
    apply();
}

void StationTciController::setInterfaceEntriesForTest(const QList<QNetworkAddressEntry>& entries)
{
    m_bind.entriesForTest = entries;
    apply();
}

void StationTciController::applySaved()
{
    auto& settings = AppSettings::instance();
    m_enabled = settings.value(enabledKey(), QStringLiteral("False")).toString()
        == QStringLiteral("True");
    bool ok = false;
    const int port = settings.value(portKey(), QString::number(kDefaultPort)).toString().toInt(&ok);
    m_port = ok && port >= 1024 && port <= 65535 ? static_cast<quint16>(port) : kDefaultPort;
    apply();
}

bool StationTciController::setEnabled(bool enabled, int port, QString* reason)
{
    if (port < 1024 || port > 65535) {
        if (reason) {
            *reason = QStringLiteral("Choose a TCI port from 1024 to 65535.");
        }
        return false;
    }
    m_enabled = enabled;
    m_port = static_cast<quint16>(port);
    auto& settings = AppSettings::instance();
    settings.setValue(enabledKey(), enabled ? QStringLiteral("True") : QStringLiteral("False"));
    settings.setValue(portKey(), QString::number(port));
    settings.save();
    resetRetry();   // a request tries at once, from the first delay again
    apply();
    if (reason) {
        reason->clear();
    }
    return true; // Saved; whether it listens is the `stationTci` object.
}

// static
QString StationTciController::unknownClientReason()
{
    return QStringLiteral("That app is no longer connected to the Core's TCI server.");
}

void StationTciController::setOptions(bool emulateExpertSdr3, bool emulateSunSdr2Pro,
                                      bool cwluBecomesCw, bool sendInitialState)
{
    // The keys and values TciProtocol::buildInitBurst and
    // buildInitialRadioStateLines read for every app that connects.
    const auto flag = [](bool on) {
        return on ? QStringLiteral("True") : QStringLiteral("False");
    };
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), flag(emulateExpertSdr3));
    settings.setValue(QStringLiteral("TciEmulateSunSDR2Pro"), flag(emulateSunSdr2Pro));
    settings.setValue(QStringLiteral("TciCwluBecomesCw"), flag(cwluBecomesCw));
    settings.setValue(QStringLiteral("TciSendInitialFrequencyStateOnConnect"),
                      flag(sendInitialState));
    settings.save();
    qCInfo(lcTci) << "Station TCI server options: ExpertSDR3" << emulateExpertSdr3
                  << "SunSDR2 PRO" << emulateSunSdr2Pro << "CWL/CWU as CW" << cwluBecomesCw
                  << "initial state" << sendInitialState;
    publish();
}

bool StationTciController::setSettings(const QVariantMap& changes, QString* reason)
{
    // Check every change first, so a request is taken whole or not at all.
    QList<std::pair<const StationTciModel::Setting*, QString>> writes;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const StationTciModel::Setting* setting = StationTciModel::setting(it.key().toUtf8());
        if (setting == nullptr) {
            if (reason) {
                *reason = QStringLiteral("The Core's TCI server has no such setting.");
            }
            return false;
        }
        QString text;
        switch (setting->kind) {
        case StationTciModel::Setting::Kind::Bool:
            if (it.value().typeId() != QMetaType::Bool) {
                if (reason) {
                    *reason = QStringLiteral("That TCI server setting is on or off.");
                }
                return false;
            }
            text = it.value().toBool() ? QStringLiteral("True") : QStringLiteral("False");
            break;
        case StationTciModel::Setting::Kind::Int:
        case StationTciModel::Setting::Kind::TxChannel: {
            bool ok = false;
            const int value = it.value().toInt(&ok);
            if (!ok || value < setting->min || value > setting->max) {
                if (reason) {
                    *reason = QStringLiteral("That value is outside the range the Core's TCI "
                                             "server allows (%1 to %2).")
                                  .arg(setting->min).arg(setting->max);
                }
                return false;
            }
            text = setting->kind == StationTciModel::Setting::Kind::TxChannel
                ? StationTciModel::txChannelText(value) : QString::number(value);
            break;
        }
        }
        writes.append({setting, text});
    }
    auto& settings = AppSettings::instance();
    for (const auto& [setting, text] : writes) {
        settings.setValue(QString::fromLatin1(setting->key), text);
        qCInfo(lcTci) << "Station TCI server setting" << setting->name << "=" << text;
    }
    settings.save();
#ifdef HAVE_WEBSOCKETS
    if (m_server) {
        // As the page does for a window's own server: these two reach the
        // running server at once; the rest are read when next needed.
        if (changes.contains(QStringLiteral("rateLimitMs"))) {
            m_server->setUpdateGapMs(changes.value(QStringLiteral("rateLimitMs")).toInt());
        }
        if (changes.contains(QStringLiteral("alwaysStreamIq"))) {
            m_server->refreshRemoteIqDemand();
        }
        // Thetis setup.cs:37386-37394 [v2.10.3.15]: the TX channel reaches
        // the running server at once too.
        if (changes.contains(QStringLiteral("txChannel"))) {
            m_server->setTxStereoInputMode(static_cast<TciServer::TxStereoInputMode>(
                changes.value(QStringLiteral("txChannel")).toInt()));
        }
    }
#endif
    if (reason) {
        reason->clear();
    }
    publish();
    return true;
}

bool StationTciController::disconnectClient(const QString& id, QString* reason)
{
#ifdef HAVE_WEBSOCKETS
    if (m_server) {
        const auto clients = m_server->clients();
        for (auto it = clients.cbegin(); it != clients.cend(); ++it) {
            if (m_clientIds.value(it.key()) == id && it.key() != nullptr) {
                qCInfo(lcTci) << "Station TCI server: disconnecting app" << it.value()->peer
                              << "at a remote device's request";
                // As the TCI Clients applet's Disconnect: the server's own
                // disconnect handling tidies the app's state.
                it.key()->close();
                if (reason) {
                    reason->clear();
                }
                return true;
            }
        }
    }
#endif
    if (reason) {
        *reason = unknownClientReason();
    }
    return false;
}

void StationTciController::publishClients()
{
    if (m_clientsQueued) {
        return;
    }
    m_clientsQueued = true;
    QMetaObject::invokeMethod(this, [this] {
        m_clientsQueued = false;
        if (!m_model) {
            return;
        }
        QList<StationTciClient> list;
#ifdef HAVE_WEBSOCKETS
        if (m_server && m_server->isRunning()) {
            const auto clients = m_server->clients();
            const QWebSocket* transmitter = m_server->activeTxAudioClient();
            QHash<const void*, QString> ids;
            for (auto it = clients.cbegin(); it != clients.cend(); ++it) {
                const std::shared_ptr<TciClientSession>& session = it.value();
                if (!session || session->disconnected) {
                    continue;
                }
                QString id = m_clientIds.value(it.key());
                if (id.isEmpty()) {
                    id = QString::number(m_nextClientId++);
                }
                ids.insert(it.key(), id);
                StationTciClient client;
                client.id = id;
                client.name = session->userAgent.trimmed();
                if (client.name.isEmpty()) {
                    client.name = QStringLiteral("(unknown)");
                }
                client.address = session->peer;
                QList<int> audio(session->audioStreamEnabled.cbegin(),
                                 session->audioStreamEnabled.cend());
                QList<int> iq(session->iqStreamEnabled.cbegin(), session->iqStreamEnabled.cend());
                std::sort(audio.begin(), audio.end());
                std::sort(iq.begin(), iq.end());
                for (int rx : audio) {
                    client.subscriptions.append(QStringLiteral("audio:%1").arg(rx));
                }
                for (int rx : iq) {
                    client.subscriptions.append(QStringLiteral("iq:%1").arg(rx));
                }
                if (session->rxSensorsEnabled) {
                    client.subscriptions.append(QStringLiteral("rxSensors"));
                }
                if (session->txSensorsEnabled) {
                    client.subscriptions.append(QStringLiteral("txSensors"));
                }
                client.transmitting = transmitter != nullptr && transmitter == it.key();
                client.lastCommand = session->lastCommand;
                list.append(client);
            }
            m_clientIds = ids;
        } else {
            m_clientIds.clear();
        }
#endif
        // In the order they connected (their ids rise).
        std::sort(list.begin(), list.end(), [](const StationTciClient& a,
                                               const StationTciClient& b) {
            return a.id.toULongLong() < b.id.toULongLong();
        });
        m_model->setClients(list);
    }, Qt::QueuedConnection);
}

QList<QHostAddress> StationTciController::wantedAddresses() const
{
    // The one station listener rule (StationNetwork::StationBind).
    return m_bind.listenAddresses();
}

void StationTciController::apply()
{
#ifdef HAVE_WEBSOCKETS
    if (!m_server) {
        return;
    }
    if (!m_enabled) {
        resetRetry();
        m_failing = false;
        if (m_server->isRunning()) {
            m_server->stop();
        }
        m_listening.clear();
        m_wanted.clear();
        m_blocked.clear();
        m_error.clear();
        publish();
        return;
    }
    const QList<QHostAddress> wanted = wantedAddresses();
    if (m_server->isRunning() && m_server->port() == m_port && wanted == m_wanted) {
        if (m_blocked.isEmpty()) {
            publish();
            return;
        }
        // Retry only what another program held; what bound keeps serving.
    } else {
        if (m_server->isRunning()) {
            m_server->stop();
        }
        m_wanted = wanted;
        m_blocked = wanted;
        m_error.clear();
    }
    const QList<QHostAddress> before = m_listening;
    QList<QHostAddress> stillBlocked;
    for (const QHostAddress& address : std::as_const(m_blocked)) {
        const bool bound = m_server->isRunning() ? m_server->addListener(address)
                                                 : m_server->start(address, m_port);
        if (!bound) {
            stillBlocked.append(address);
        }
    }
    m_blocked = stillBlocked;
    m_listening = m_server->isRunning() ? m_server->listenAddresses() : QList<QHostAddress>{};
    if (m_listening != before && !m_listening.isEmpty()) {
        qCInfo(lcTci) << "Station TCI server listening on" << m_listening << "port" << m_port;
    }
    if (m_blocked.isEmpty()) {
        resetRetry();
        if (m_failing) {
            qCInfo(lcTci) << "Station TCI server listening on every station address again";
        }
        m_failing = false;
    } else {
        const int delay = kRetryDelaysMs[qMin(m_retryStep, int(std::size(kRetryDelaysMs)) - 1)];
        ++m_retryStep;
        if (!m_failing) {
            // One line when it starts failing; the tries stay quiet.
            qCWarning(lcTci) << "Station TCI server could not listen on" << m_blocked
                             << "port" << m_port << m_error << "; retrying";
            m_failing = true;
        }
        m_retryTimer.start(delay);
    }
#endif
    publish();
}

void StationTciController::publish()
{
    if (!m_model) {
        return;
    }
    StationTciModel::State state;
    state.enabled = m_enabled;
    state.port = m_port;
#ifdef HAVE_WEBSOCKETS
    state.listening = m_server && m_server->isRunning();
#endif
    for (const QHostAddress& address : m_listening) {
        if (!address.isLoopback()) {
            state.stationAddress = address.toString();
            break;
        }
    }
    // Which address is blocked, if any, in plain words; the station address
    // above says which one serves the station.
    state.error = m_blocked.isEmpty() ? QString() : blockedReason(m_port, m_blocked);
    // Parity Task 23: the options as the server reads them, with its
    // readers' defaults (TciProtocol::buildInitBurst).
    auto& settings = AppSettings::instance();
    const auto flag = [&settings](const char* key, const char* fallback) {
        return settings.value(QString::fromLatin1(key), QString::fromLatin1(fallback)).toString()
            == QStringLiteral("True");
    };
    state.emulateExpertSdr3 = flag("TciEmulateExpertSDR3Protocol", "True");
    state.emulateSunSdr2Pro = flag("TciEmulateSunSDR2Pro", "True");
    state.cwluBecomesCw = flag("TciCwluBecomesCw", "False");
    state.sendInitialState = flag("TciSendInitialFrequencyStateOnConnect", "True");
    // JJ's ruling of 2026-09-28: the rest of the page's settings, as the
    // server and the page read them.
    for (const StationTciModel::Setting& setting : StationTciModel::settingsTable()) {
        const QString text = settings.value(QString::fromLatin1(setting.key),
                                            QString::fromLatin1(setting.fallback)).toString();
        QVariant value;
        switch (setting.kind) {
        case StationTciModel::Setting::Kind::Bool:
            value = text == QStringLiteral("True");
            break;
        case StationTciModel::Setting::Kind::Int: {
            bool ok = false;
            const int number = text.toInt(&ok);
            value = qBound(setting.min, ok ? number : QString::fromLatin1(setting.fallback).toInt(),
                           setting.max);
            break;
        }
        case StationTciModel::Setting::Kind::TxChannel: {
            const int index = StationTciModel::txChannelIndex(text);
            value = index >= 0 ? index : 2;
            break;
        }
        }
        StationTciModel::setIn(state, setting, value);
    }
    m_model->setState(state);
}

} // namespace NereusSDR
