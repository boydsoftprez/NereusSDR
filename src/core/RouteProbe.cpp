// =================================================================
// src/core/RouteProbe.cpp  (NereusSDR)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See RouteProbe.h for design rationale.
// =================================================================
#include "RouteProbe.h"

#include <QUdpSocket>

namespace NereusSDR {

QHostAddress probeLocalAddressFor(const QHostAddress& target)
{
    if (target.isNull() || target == QHostAddress::AnyIPv4
        || target == QHostAddress::AnyIPv6) {
        return QHostAddress{};
    }

    QUdpSocket probe;
    // Port 9 == discard.  UDP connect is local-only; no packets sent.
    // Timeout is small (50 ms) because this is a kernel route lookup,
    // not a network round-trip.
    probe.connectToHost(target, /*port=*/9);
    if (!probe.waitForConnected(/*ms=*/50)) {
        return QHostAddress{};
    }

    const QHostAddress local = probe.localAddress();
    if (local.protocol() != QAbstractSocket::IPv4Protocol) {
        return QHostAddress{};
    }
    if (local.isNull() || local == QHostAddress::AnyIPv4
        || local == QHostAddress::LocalHost) {
        // Localhost-loopback is a degenerate result we treat as "no
        // route" -- the kernel returns 127.0.0.1 when probing a peer
        // that maps to the loopback interface, which is never what we
        // want for talking to a real piece of network hardware.
        // (Exception: tests that intentionally probe LocalHost handle
        // the result via probe directly, not through this helper.)
        return QHostAddress{};
    }
    return local;
}

} // namespace NereusSDR
