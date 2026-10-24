// no-port-check: NereusSDR-original test helper.
//
// UnusedLoopbackPort: a port for a test to hand to something that listens
// on it later (a station server, a CAT or TCI listener, a Core started as a
// process), or to dial expecting nothing to answer.
//
// The pattern it replaces, listen on port 0, read the port and close, takes
// the port from the kernel's ephemeral range. Once closed, the kernel hands
// that same range out to every other process's port-0 listener and
// outgoing connection, so on a machine running tests side by side another
// test can hold the port by the time this one listens: "The bound address
// is already in use" on 127.0.0.1:37915 in tst_desktop_station_window on
// the Linux self-hosted CI (PR 611).
//
// This helper picks from 20000 to 31999, below the ephemeral range, which
// the kernel never hands out for port 0 or an outgoing connection, and
// which no product default uses (CAT 13013, rigctld 4532, the station 47910
// and 47911, TCI 50001).
//   - Linux: the range is read from /proc/sys/net/ipv4/ip_local_port_range
//     (default 32768 to 60999); when its low end is below 32000 the pick
//     ends there instead, starting at most 12000 below it and never below
//     1024.
//   - macOS (and the BSDs) and Windows: the range is not read. The
//     defaults assumed are 49152 to 65535 on both (macOS
//     net.inet.ip.portrange.first/last; Windows' dynamic port range, netsh
//     int ipv4 show dynamicport tcp), far above the pick.
// It checks that nothing listens there now on TCP or UDP, choosing at
// random so that tests picking at the same moment seldom choose alike.
//
// One limit: on macOS, the BSDs and Windows the check on 127.0.0.1 can
// succeed while another process listens on the wildcard address (0.0.0.0)
// at that port, as those systems let a socket bind a specific address
// beside a wildcard listener (Qt sets SO_REUSEADDR on Unix listeners;
// Windows lets the more specific bind win unless the first socket took
// SO_EXCLUSIVEADDRUSE). Linux refuses that bind, so the check sees it. The
// random pick below the ephemeral range makes such a collision unlikely,
// not impossible.
//
// Use it only for a port the test hands to something that listens later
// (or dials expecting nothing to answer). A test's own listener that keeps
// its port for the whole test listens on port 0 and reads serverPort().
//
// 2026-10-22, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-22: the Linux pick ends below ip_local_port_range's low end; the
// assumed ranges elsewhere and the wildcard-listener limit are named. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once

#include <QtGlobal>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QUdpSocket>

#if defined(Q_OS_LINUX)
#include <QFile>
#include <QList>
#endif

#include <algorithm>
#include <utility>

namespace NereusSDR {

inline constexpr int kUnusedLoopbackPortFirst = 20000;
inline constexpr int kUnusedLoopbackPortEnd = 32000;  // exclusive
inline constexpr int kUnusedLoopbackPortLowest = 1024;
inline constexpr int kUnusedLoopbackPortAttempts = 200;

// The range the pick is made from: [first, end).
inline std::pair<int, int> unusedLoopbackPortRange()
{
    int first = kUnusedLoopbackPortFirst;
    int end = kUnusedLoopbackPortEnd;
#if defined(Q_OS_LINUX)
    QFile range(QStringLiteral("/proc/sys/net/ipv4/ip_local_port_range"));
    if (range.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> fields = range.readAll().simplified().split(' ');
        bool ok = false;
        const int low = fields.isEmpty() ? 0 : fields.first().toInt(&ok);
        if (ok && low > kUnusedLoopbackPortLowest + 1 && low < end) {
            end = low;
            first = std::max(kUnusedLoopbackPortLowest,
                             end - (kUnusedLoopbackPortEnd - kUnusedLoopbackPortFirst));
        }
    }
#endif
    return {first, end};
}

// A port nothing listens on at `address` now, other than `except`; 0 when
// none was found.
inline quint16 unusedLoopbackPort(const QHostAddress& address = QHostAddress::LocalHost,
                                  quint16 except = 0)
{
    const auto [first, end] = unusedLoopbackPortRange();
    for (int attempt = 0; attempt < kUnusedLoopbackPortAttempts; ++attempt) {
        const auto port = static_cast<quint16>(QRandomGenerator::global()->bounded(first, end));
        if (port == except) {
            continue;
        }
        QTcpServer tcp;
        if (!tcp.listen(address, port)) {
            continue;
        }
        QUdpSocket udp;
        if (!udp.bind(address, port, QAbstractSocket::DontShareAddress)) {
            continue;
        }
        return port;
    }
    return 0;
}

} // namespace NereusSDR
