#include "RadioConnection.h"
#include "P1RadioConnection.h"
#include "P2RadioConnection.h"
#include "LogCategories.h"

namespace NereusSDR {

RadioConnection::RadioConnection(QObject* parent)
    : QObject(parent)
{
}

RadioConnection::~RadioConnection() = default;

std::unique_ptr<RadioConnection> RadioConnection::create(const RadioInfo& info)
{
    switch (info.protocol) {
    case ProtocolVersion::Protocol2: {
        auto conn = std::make_unique<P2RadioConnection>();
        return conn;
    }
    case ProtocolVersion::Protocol1:
        return std::make_unique<P1RadioConnection>();
    }
    qCWarning(lcConnection) << "Unknown protocol version:" << static_cast<int>(info.protocol);
    return nullptr;
}

void RadioConnection::setState(ConnectionState newState)
{
    ConnectionState expected = m_state.load();
    if (expected != newState) {
        m_state.store(newState);
        emit connectionStateChanged(newState);
    }
}

// ---------------------------------------------------------------------------
// Rolling-window byte-rate counters
// ---------------------------------------------------------------------------

void RadioConnection::pruneSamples(QList<ByteSample>& samples, qint64 nowMs, int windowMs)
{
    while (!samples.isEmpty() && (nowMs - samples.first().ms) > windowMs) {
        samples.removeFirst();
    }
}

void RadioConnection::recordBytesSent(qint64 n)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    pruneSamples(m_txSamples, nowMs, 5000);
    m_txSamples.append({nowMs, n});
}

void RadioConnection::recordBytesReceived(qint64 n)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    pruneSamples(m_rxSamples, nowMs, 5000);
    m_rxSamples.append({nowMs, n});
}

double RadioConnection::txByteRate(int windowMs) const
{
    return rateFromSamples(m_txSamples, windowMs);
}

double RadioConnection::rxByteRate(int windowMs) const
{
    return rateFromSamples(m_rxSamples, windowMs);
}

double RadioConnection::rateFromSamples(const QList<ByteSample>& samples, int windowMs)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 totalBytes = 0;
    for (const auto& s : samples) {
        if (nowMs - s.ms <= windowMs) {
            totalBytes += s.bytes;
        }
    }
    // Mbps = bytes * 8 / windowMs / 1e3
    return (totalBytes * 8.0) / (windowMs * 1000.0);
}

} // namespace NereusSDR
