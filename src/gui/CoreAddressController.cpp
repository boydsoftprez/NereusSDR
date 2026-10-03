// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CoreAddressController.h"

namespace NereusSDR {
CoreAddressController::CoreAddressController(CoreTargetStore& store, QObject* parent,
    CoreAddressProbe::RungFactory factory, int deadlineMs)
    : QObject(parent), m_store(store), m_probe(nullptr, std::move(factory), deadlineMs)
{
    connect(&m_probe, &CoreAddressProbe::finished, this, &CoreAddressController::checked);
}

CoreAddressController::~CoreAddressController()
{
    m_operation.reset();
    m_probe.cancel();
}

void CoreAddressController::inspectTarget(const QString& id)
{
    const quint64 incarnation = m_store.targetIncarnation(id);
    if (m_inspectedId != id || m_inspectedIncarnation != incarnation) { cancel(); }
    m_inspectedId = id;
    m_inspectedIncarnation = incarnation;
}

quint64 CoreAddressController::addAddress(const QString& host, int port)
{
    return begin(host, port, std::nullopt);
}

quint64 CoreAddressController::editAddress(const QString& previous, const QString& host, int port)
{
    return begin(host, port, previous);
}

quint64 CoreAddressController::begin(const QString& host, int port, std::optional<QString> previous)
{
    cancel();
    const quint64 id = ++m_nextOperation;
    const auto target = m_store.target(m_inspectedId);
    m_operation = Operation{id, m_inspectedId, m_inspectedIncarnation,
        target ? target->connection.identityFingerprint : QByteArray(), QString(), std::move(previous)};
    if (!target || !current(*m_operation)) {
        refuseLater(Outcome::Refused, QStringLiteral("Select a saved paired Core before checking an address."));
        return id;
    }
    QString error;
    m_operation->address = CoreTargetStore::normalizeManualAddress(host, port, &error);
    if (m_operation->address.isEmpty()) {
        refuseLater(Outcome::Refused, error);
        return id;
    }
    const QString& address = m_operation->address;
    const auto& old = m_operation->previousAddress;
    if (old && !target->manualAddresses.contains(*old)) {
        refuseLater(Outcome::Refused, QStringLiteral("The retained Core address changed or was removed."));
    } else if (target->manualAddresses.contains(address) && (!old || *old != address)) {
        refuseLater(Outcome::Refused, QStringLiteral("This Core address is already retained."));
    } else if (!old && target->manualAddresses.size() >= CoreTargetStore::kMaxManualAddresses) {
        refuseLater(Outcome::Refused, QStringLiteral("A Core can retain at most four manual addresses."));
    } else {
        m_probe.start(QUrl(address), m_operation->identity);
    }
    return id;
}

bool CoreAddressController::current(const Operation& operation) const
{
    const auto target = m_store.target(operation.targetId);
    return operation.incarnation != 0 && m_inspectedId == operation.targetId
        && m_inspectedIncarnation == operation.incarnation
        && m_store.targetIncarnation(operation.targetId) == operation.incarnation
        && target && target->connection.identityFingerprint.size() == 32
        && target->connection.identityFingerprint == operation.identity
        && !target->connection.allowUnpinned;
}

void CoreAddressController::checked(CoreAddressProbe::Outcome outcome, const QString& reason)
{
    if (!m_operation) { return; }
    if (!current(*m_operation)) {
        finish(Outcome::StaleTarget, QStringLiteral("The saved Core changed. Check the address again."));
        return;
    }
    if (outcome != CoreAddressProbe::Outcome::Verified) {
        finish(outcome == CoreAddressProbe::Outcome::TimedOut ? Outcome::TimedOut
            : outcome == CoreAddressProbe::Outcome::Cancelled ? Outcome::Cancelled : Outcome::Refused, reason);
        return;
    }
    // Never save the snapshot from before the probe. Store operations copy the
    // fresh current record and edit compares the exact old canonical entry.
    const Operation operation = *m_operation;
    const auto target = m_store.target(operation.targetId);
    if (operation.previousAddress && !target->manualAddresses.contains(*operation.previousAddress)) {
        finish(Outcome::StaleTarget, QStringLiteral("The retained Core address changed or was removed."));
        return;
    }
    QString error;
    const bool saved = operation.previousAddress
        ? m_store.updateManualAddress(operation.targetId, *operation.previousAddress, operation.address, &error)
        : m_store.addManualAddress(operation.targetId, operation.address, &error);
    finish(saved ? Outcome::Saved : Outcome::SaveFailed, error);
}

bool CoreAddressController::removeAddress(const QString& address, QString* error)
{
    cancel();
    if (m_inspectedIncarnation == 0
        || m_store.targetIncarnation(m_inspectedId) != m_inspectedIncarnation) {
        if (error) { *error = QStringLiteral("The saved Core changed. Reopen its addresses."); }
        return false;
    }
    return m_store.removeManualAddress(m_inspectedId, address, error);
}

void CoreAddressController::cancel()
{
    if (m_operation) { finish(Outcome::Cancelled, QStringLiteral("Address check cancelled.")); }
    m_probe.cancel();
}

void CoreAddressController::refuseLater(Outcome outcome, const QString& reason)
{
    // Keep pending until this callback so caller can associate the returned id.
    const quint64 id = m_operation->id;
    QTimer::singleShot(0, this, [this, id, outcome, reason] {
        if (m_operation && m_operation->id == id) { finish(outcome, reason); }
    });
}

void CoreAddressController::finish(Outcome outcome, const QString& reason)
{
    if (!m_operation) { return; }
    const quint64 id = m_operation->id;
    m_operation.reset();
    m_probe.cancel();
    QTimer::singleShot(0, this, [this, id, outcome, reason] { emit finished(id, outcome, reason); });
}
} // namespace NereusSDR
