// =================================================================
// tests/tst_juice_ipv6_source_address.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. libjuice offers one host
// candidate per IPv6 /64 and took the first address getifaddrs listed. On
// 2026-10-24 a Mac listed its secured address first and sent from its
// temporary one (net.inet6.ip6.prefer_tempaddr), so the address it offered
// was not the address its checks came from, stateful firewalls at both ends
// dropped the checks, and media to a Core settled on IPv4
// (cmake/patches/libjuice-0005). These cases gather the host addresses of a
// wildcard-bound libjuice socket and check that each IPv6 /64 is offered
// once, and on macOS with the address the system sends from. Only this
// computer's own addresses are used and nothing is sent; no radio, Core or
// audio device is opened.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <optional>
#include <vector>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include "addr.h"
#include "udp.h"
}
#endif

namespace {

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)

constexpr int kPrefixBytes = 8;
constexpr int kMaxRecords = 64;

using Prefix = std::array<quint8, kPrefixBytes>;

Prefix prefixOf(const in6_addr& addr)
{
    Prefix prefix {};
    std::memcpy(prefix.data(), &addr, kPrefixBytes);
    return prefix;
}

QString textOf(const in6_addr& addr)
{
    char buffer[INET6_ADDRSTRLEN] {};
    ::inet_ntop(AF_INET6, &addr, buffer, sizeof(buffer));
    return QString::fromLatin1(buffer);
}

// The usable IPv6 addresses of this computer, as libjuice filters them:
// up, not loopback, not docker0, not loopback or link-local.
std::vector<in6_addr> listedIpv6()
{
    std::vector<in6_addr> listed;
    ifaddrs* ifas = nullptr;
    if (::getifaddrs(&ifas) != 0) {
        return listed;
    }
    for (const ifaddrs* ifa = ifas; ifa; ifa = ifa->ifa_next) {
        const sockaddr* sa = ifa->ifa_addr;
        if (!sa || sa->sa_family != AF_INET6 || !(ifa->ifa_flags & IFF_UP)
            || (ifa->ifa_flags & IFF_LOOPBACK) || std::strcmp(ifa->ifa_name, "docker0") == 0
            || addr_is_local(sa)) {
            continue;
        }
        listed.push_back(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr);
    }
    ::freeifaddrs(ifas);
    return listed;
}

bool isListed(const std::vector<in6_addr>& listed, const in6_addr& addr)
{
    for (const in6_addr& a : listed) {
        if (std::memcmp(&a, &addr, sizeof(addr)) == 0) {
            return true;
        }
    }
    return false;
}

// The source the system picks for a host in `prefix` (its ::1), or nothing
// when it has no route there or ::1 is this computer's own address.
std::optional<in6_addr> systemSourceFor(const Prefix& prefix)
{
    sockaddr_in6 dest {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = htons(9);
    std::memcpy(&dest.sin6_addr, prefix.data(), kPrefixBytes);
    dest.sin6_addr.s6_addr[15] = 1;

    const int fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return std::nullopt;
    }
    sockaddr_in6 source {};
    socklen_t len = sizeof(source);
    const bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) == 0
        && ::getsockname(fd, reinterpret_cast<sockaddr*>(&source), &len) == 0
        && source.sin6_family == AF_INET6
        && std::memcmp(&source.sin6_addr, &dest.sin6_addr, sizeof(in6_addr)) != 0;
    ::close(fd);
    if (!ok) {
        return std::nullopt;
    }
    return source.sin6_addr;
}

// The IPv6 host addresses libjuice gathers for a wildcard-bound socket, or
// nothing when the socket is IPv4 only.
std::optional<std::vector<in6_addr>> gatheredIpv6()
{
    udp_socket_config_t config {};
    const socket_t sock = udp_create_socket(&config);
    if (sock == INVALID_SOCKET) {
        return std::nullopt;
    }
    addr_record_t bound {};
    addr_record_t records[kMaxRecords] {};
    const bool dualStack = udp_get_bound_addr(sock, &bound) == 0 && bound.addr.ss_family == AF_INET6;
    const int count = udp_get_addrs(sock, records, kMaxRecords);
    closesocket(sock);
    if (!dualStack || count < 0) {
        return std::nullopt;
    }
    std::vector<in6_addr> gathered;
    for (int i = 0; i < std::min(count, kMaxRecords); ++i) {
        if (records[i].addr.ss_family == AF_INET6) {
            gathered.push_back(reinterpret_cast<const sockaddr_in6*>(&records[i].addr)->sin6_addr);
        }
    }
    return gathered;
}

#endif

} // namespace

class TstJuiceIpv6SourceAddress : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
        QSKIP("POSIX sockets only; the libjuice change is compiled on macOS.");
#endif
    }

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Every /64 this computer has an address in is offered exactly once.
    void eachPrefixIsOfferedOnce()
    {
        const auto gathered = gatheredIpv6();
        if (!gathered) {
            QSKIP("libjuice's socket is IPv4 only on this computer.");
        }
        const std::vector<in6_addr> listed = listedIpv6();
        if (listed.empty()) {
            QSKIP("This computer has no usable IPv6 address.");
        }

        std::map<Prefix, int> offered;
        for (const in6_addr& addr : *gathered) {
            QVERIFY2(isListed(listed, addr), qPrintable(textOf(addr) + " is not this computer's"));
            ++offered[prefixOf(addr)];
        }
        for (const in6_addr& addr : listed) {
            const auto it = offered.find(prefixOf(addr));
            QVERIFY2(it != offered.end(), qPrintable("no candidate in the /64 of " + textOf(addr)));
            QCOMPARE(it->second, 1);
        }
    }

    // On macOS each /64 is offered with the address the system sends from
    // when that address is one of this computer's.
    void eachPrefixOffersTheSystemSource()
    {
#if !defined(Q_OS_MAC)
        QSKIP("The libjuice change is compiled on macOS only.");
#else
        const auto gathered = gatheredIpv6();
        if (!gathered) {
            QSKIP("libjuice's socket is IPv4 only on this computer.");
        }
        const std::vector<in6_addr> listed = listedIpv6();

        int checked = 0;
        for (const in6_addr& addr : *gathered) {
            const std::optional<in6_addr> source = systemSourceFor(prefixOf(addr));
            if (!source || prefixOf(*source) != prefixOf(addr) || !isListed(listed, *source)) {
                continue;
            }
            QCOMPARE(textOf(addr), textOf(*source));
            ++checked;
        }
        if (checked == 0) {
            QSKIP("No IPv6 /64 here has a route the system picks a source for.");
        }
        qInfo("checked %d IPv6 /64(s)", checked);
#endif
    }
#endif
};

QTEST_GUILESS_MAIN(TstJuiceIpv6SourceAddress)
#include "tst_juice_ipv6_source_address.moc"
