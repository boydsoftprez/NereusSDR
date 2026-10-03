#pragma once
// no-port-check: NereusSDR-original.

#include "core/session/IceConfiguration.h"

#include <QObject>

#include <functional>
#include <memory>

namespace NereusSDR {

// A loopback candidate's socket must outlive the actual ICE transport, not
// merely PeerConnection::close(): libdatachannel tears ICE down on another
// thread after close returns. rtc::Configuration::iceTransportLifetime keeps
// this lease until the last ICE holder is destroyed. The custom shared_ptr
// deleter then posts QObject deletion to this object's Qt thread, where the
// candidate source owns its UDP socket and may safely release it.
class CandidateSourceLease final : public QObject {
public:
    static std::shared_ptr<CandidateSourceLease> create(const IceConfiguration& ice);

    void start(int lane, const QString& connectionId,
               std::function<void(const QString& candidate)> add);
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const;

    ~CandidateSourceLease() override;

private:
    explicit CandidateSourceLease(const IceConfiguration& ice);

    // The factory may hold a RelayLeg or MediaTunnel. Declare it before the
    // source so it is destroyed after the source and its socket are stopped.
    IceConfiguration m_ice;
    std::shared_ptr<IceConfiguration::CandidateSource> m_source;
    bool m_startRequested = false;
};

} // namespace NereusSDR
