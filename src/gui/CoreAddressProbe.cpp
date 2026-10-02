// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CoreAddressProbe.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationClient.h"

namespace NereusSDR {
CoreAddressProbe::CoreAddressProbe(QObject* parent, RungFactory factory, int deadlineMs)
    : QObject(parent), m_factory(std::move(factory)), m_deadlineMs(qMax(1, deadlineMs))
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        finish(Outcome::TimedOut, QStringLiteral("The Core did not answer in time."));
    });
}

CoreAddressProbe::~CoreAddressProbe()
{
    releaseRace();
}

void CoreAddressProbe::start(const QUrl& endpoint, const QByteArray& pairedIdentity)
{
    const quint64 generation = ++m_startGeneration;
    const QPointer<CoreAddressProbe> self(this);
    cancel();
    // A Cancelled observer may start a newer check or destroy this owner.
    // Let that newer request keep sole ownership instead of overwriting it.
    if (!self || generation != m_startGeneration) { return; }
    m_pending = true;
    if (pairedIdentity.size() != 32 || !endpoint.isValid()
        || endpoint.scheme() != QLatin1String("wss") || endpoint.host().isEmpty()
        || endpoint.port() < 1 || endpoint.port() > 65535 || !endpoint.userInfo().isEmpty()
        || !endpoint.path().isEmpty() || endpoint.hasQuery() || endpoint.hasFragment()) {
        finish(Outcome::Refused, QStringLiteral("A paired Core and a secure listener address are required."));
        return;
    }
    auto* racer = new PathRacer(this);
    m_racer = racer;
    racer->setVetter([pairedIdentity](const SessionMessage& hello, SessionTransport* transport) {
        return transport != nullptr && StationClient::helloProvesPairedCore(
            hello, transport->peerCertificateSha256(), pairedIdentity);
    });
    connect(racer, &PathRacer::won, this, [this, racer](const PathRacer::Ready& ready) {
        // A won transport is unparented and transferred by PathRacer. Never
        // construct a StationClient or send any application frame on this link.
        if (ready.transport) {
            ready.transport->closeLink(QStringLiteral("Core address identity check complete"));
            ready.transport->deleteLater();
        }
        if (m_racer == racer && m_pending) { finish(Outcome::Verified, QString()); }
    });
    connect(racer, &PathRacer::failed, this, [this, racer](const QString& reason) {
        if (m_racer == racer && m_pending) {
            finish(Outcome::Refused, reason.isEmpty()
                ? QStringLiteral("This address did not prove the saved Core's identity.") : reason);
        }
    });
    PathRung* rung = m_factory ? m_factory(endpoint)
        : new DirectPathRung(endpoint, StationClient::kMaxIncomingMessageBytes, racer);
    if (rung == nullptr) {
        finish(Outcome::Refused, QStringLiteral("The Core address could not be checked."));
        return;
    }
    racer->addRung(rung);
    // Includes DNS/TLS/socket opening, unlike the racer's post-open hello bound.
    m_deadline.start(m_deadlineMs);
    racer->start();
}

void CoreAddressProbe::cancel()
{
    if (m_pending) { finish(Outcome::Cancelled, QStringLiteral("Address check cancelled.")); }
}

void CoreAddressProbe::releaseRace()
{
    m_deadline.stop();
    if (m_racer) {
        PathRacer* racer = m_racer;
        m_racer = nullptr;
        disconnect(racer, nullptr, this, nullptr);
        racer->cancel();
        // May be inside won/failed; defer destruction of the signal sender.
        racer->deleteLater();
    }
}

void CoreAddressProbe::finish(Outcome outcome, const QString& reason)
{
    if (!m_pending) { return; }
    m_pending = false;
    releaseRace();
    emit finished(outcome, reason);
}
} // namespace NereusSDR
