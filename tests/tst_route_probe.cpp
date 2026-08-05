// =================================================================
// tests/tst_route_probe.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test.  No upstream source file ported.
//
// Verifies the contract of src/core/RouteProbe.cpp's
// probeLocalAddressFor() helper, which PgxlConnection, TgxlConnection,
// and FlexRadioDiscoveryBroadcaster all use to ask the kernel which
// local source IP it would use to reach a given peer.
//
// Tests are host-portable: they assert the API contract (null input
// -> null result; well-known unreachable targets -> null result; a
// real-LAN result is a sensible IPv4 that is not loopback or wildcard)
// rather than any specific local IP, so the suite passes on any CI
// host with or without an internet connection.
//
// Modification history (NereusSDR):
//   2026-05-26 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "core/RouteProbe.h"

using namespace NereusSDR;

class TstRouteProbe : public QObject {
    Q_OBJECT
private slots:

    // A null target trivially returns a null result -- nothing to look up.
    void nullTargetReturnsNull()
    {
        const QHostAddress result = probeLocalAddressFor(QHostAddress{});
        QVERIFY(result.isNull());
    }

    // 0.0.0.0 is the "any" address and should never be a valid probe
    // target.  Some kernels would return a usable local IP for it
    // (default route) but our helper deliberately rejects it so callers
    // never get a misleading "any" hint.
    void anyIPv4TargetReturnsNull()
    {
        const QHostAddress result =
            probeLocalAddressFor(QHostAddress::AnyIPv4);
        QVERIFY(result.isNull());
    }

    // Probing the IPv6 ANY address should also be rejected -- the helper
    // is IPv4-only by design (PGXL / TGXL / RF-Kit / OpenHPSDR all run
    // on IPv4).
    void anyIPv6TargetReturnsNull()
    {
        const QHostAddress result =
            probeLocalAddressFor(QHostAddress::AnyIPv6);
        QVERIFY(result.isNull());
    }

    // Probing the loopback address yields the loopback as the "source"
    // because the kernel would bind to 127.0.0.1.  The helper rejects
    // loopback as a degenerate result (we only want real-LAN sources
    // for peripheral binding), so this must return null.
    void loopbackTargetReturnsNull()
    {
        const QHostAddress result =
            probeLocalAddressFor(QHostAddress::LocalHost);
        QVERIFY(result.isNull());
    }

    // Sanity: probing a well-known public address.  On a CI host with no
    // network this returns null (waitForConnected times out); on a host
    // with a default route it returns an IPv4 LAN-style address.  Either
    // outcome is acceptable -- we just verify the result, if any, is a
    // sane IPv4 that is neither loopback nor wildcard.
    void publicTargetReturnsRealOrNull()
    {
        const QHostAddress result =
            probeLocalAddressFor(QHostAddress(QStringLiteral("8.8.8.8")));
        if (result.isNull()) {
            // No network on this CI host -- acceptable.
            QSKIP("no IPv4 default route on this host");
        }
        QCOMPARE(result.protocol(), QAbstractSocket::IPv4Protocol);
        QVERIFY(result != QHostAddress::LocalHost);
        QVERIFY(result != QHostAddress::AnyIPv4);
        QVERIFY(!result.isNull());
    }
};

QTEST_MAIN(TstRouteProbe)
#include "tst_route_probe.moc"
