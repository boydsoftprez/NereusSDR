// =================================================================
// src/core/RouteProbe.h  (NereusSDR)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// NereusSDR-native; no upstream port.
//
// Shared "which local source IP would the kernel pick to reach this
// peer" probe.  Used by FlexRadioDiscoveryBroadcaster to choose the
// beacon's source binding and by PgxlConnection / TgxlConnection to
// source-bind their TCP sockets before connect.
//
// Why this exists: on a multi-homed host (e.g. macOS with a physical
// en0 AND a ZeroTier feth/utun overlay AND vendor-specific virtual
// adapters) more than one local interface can claim a route to the
// same target subnet.  The OS default-routing picker chooses ONE
// based on routing metrics; that choice may not match the operator's
// intent (e.g. ZeroTier installs more specific routes that win even
// when the physical LAN is preferable, or vice versa).  By asking
// the kernel "if I were to talk to X, which local IP would you bind
// to" via a no-packet UDP::connectToHost trick, and then explicitly
// binding our outgoing socket to that source address, we make each
// peripheral's egress interface deterministic per-peer and pick up
// topology changes on every new connect attempt.
// =================================================================
#pragma once

#include <QHostAddress>

namespace NereusSDR {

/// Ask the kernel which local IPv4 source address it would use to
/// reach `target`.  Returns a null QHostAddress if the probe fails
/// (e.g. target is unreachable / no IPv4 default route).
///
/// Implementation: opens a QUdpSocket, calls connectToHost(target,
/// /*port=*/9 /*discard*/).  No bytes are ever transmitted -- UDP
/// connect is a purely local operation that performs a route lookup
/// and binds the local socket address.  localAddress() then returns
/// the kernel's chosen source IP.
///
/// Use this just before binding the source of a new outgoing socket
/// (TCP connect, UDP broadcast) so the bind is consistent with the
/// route the kernel would have picked.  Re-probing on every connect
/// attempt picks up topology changes (DHCP renewal, ZeroTier
/// membership flips, VPN toggles) without needing a higher-level
/// NIC-monitor.
///
/// Thread-safe: each call constructs its own ephemeral socket on the
/// calling thread.
QHostAddress probeLocalAddressFor(const QHostAddress& target);

} // namespace NereusSDR
