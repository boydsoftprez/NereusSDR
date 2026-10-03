#pragma once
// no-port-check: NereusSDR-original.

#include <QString>
#include <QtGlobal>

#include <optional>

QT_BEGIN_NAMESPACE
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR {

// A present-tense route observation, never an identity or an ICE proposal.
// Empty addresses and zero ports mean that endpoint detail is unavailable.
struct NetworkPathSnapshot {
    enum class Kind { Unknown, Direct, Relayed };
    enum class Carrier { WebSocket, Ice, WebRelay };
    enum class Endpoints { Socket, IceCandidates };

    Kind kind = Kind::Unknown;
    Carrier carrier = Carrier::Ice;
    Endpoints endpoints = Endpoints::IceCandidates;
    bool mediaRidesControl = false;
    QString localAddress;
    quint16 localPort = 0;
    QString remoteAddress;
    quint16 remotePort = 0;
    QString localCandidateType;
    QString remoteCandidateType;
};

// Reads only numeric socket endpoints on the socket's owning Qt thread.
std::optional<NetworkPathSnapshot> socketNetworkPathSnapshot(
    const QWebSocket* socket, NetworkPathSnapshot::Carrier carrier);

} // namespace NereusSDR
