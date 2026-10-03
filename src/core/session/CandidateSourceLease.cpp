// no-port-check: NereusSDR-original.

#include "core/session/CandidateSourceLease.h"

#include <QThread>

#include <utility>

namespace NereusSDR {

CandidateSourceLease::CandidateSourceLease(const IceConfiguration& ice)
    : m_ice(ice)
{
}

CandidateSourceLease::~CandidateSourceLease()
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_source) {
        m_source->stop();
    }
}

std::shared_ptr<CandidateSourceLease> CandidateSourceLease::create(
    const IceConfiguration& ice)
{
    return {new CandidateSourceLease(ice),
            [](CandidateSourceLease* lease) { lease->deleteLater(); }};
}

void CandidateSourceLease::start(int lane, const QString& connectionId,
                                 std::function<void(const QString& candidate)> add)
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_startRequested) {
        return;
    }
    m_startRequested = true;
    m_source = m_ice.makeCandidateSource(lane, connectionId);
    if (m_source) {
        m_source->start(std::move(add));
    }
}

std::optional<NetworkPathSnapshot> CandidateSourceLease::networkPathSnapshot() const
{
    if (thread() != QThread::currentThread() || !m_source) {
        return std::nullopt;
    }
    return m_source->networkPathSnapshot();
}

} // namespace NereusSDR
