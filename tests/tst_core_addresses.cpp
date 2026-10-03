// =================================================================
// tests/tst_core_addresses.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The phone's direct addresses:
// which of the Core's addresses a signed-in device is told it can dial
// (CoreAddresses::dialable), their wire form, and that a change of the
// interfaces is said once (CoreAddressWatcher).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-29 - The Rock's temporary and deprecated mix, through Qt's
//                 flags and through the kernel's. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDeadlineTimer>
#include <QNetworkAddressEntry>
#include <QSignalSpy>

#include "core/session/CoreAddresses.h"

using namespace NereusSDR;

namespace {

QNetworkAddressEntry entry(const char* ip, int prefix = -1)
{
    QNetworkAddressEntry e;
    e.setIp(QHostAddress(QString::fromLatin1(ip)));
    if (prefix >= 0) {
        e.setPrefixLength(prefix);
    }
    return e;
}

QNetworkAddressEntry temporary(const char* ip)
{
    QNetworkAddressEntry e = entry(ip, 64);
    e.setDnsEligibility(QNetworkAddressEntry::DnsIneligible);
    return e;
}

QNetworkAddressEntry deprecated(const char* ip)
{
    QNetworkAddressEntry e = entry(ip, 64);
    // Preferred lifetime over, valid for another hour: a renumbered
    // prefix's old address.
    e.setAddressLifetime(QDeadlineTimer(0), QDeadlineTimer(3600 * 1000));
    return e;
}

QNetworkAddressEntry preferred(const char* ip)
{
    QNetworkAddressEntry e = entry(ip, 64);
    e.setDnsEligibility(QNetworkAddressEntry::DnsEligible);
    e.setAddressLifetime(QDeadlineTimer(3600 * 1000), QDeadlineTimer(7200 * 1000));
    return e;
}

} // namespace

class TestCoreAddresses : public QObject {
    Q_OBJECT

private slots:
    void onlyStableGlobalIpv6AndPublicIpv4()
    {
        const QList<QNetworkAddressEntry> entries{
            entry("fe80::1", 64),                   // link-local
            entry("fd12:3456::1", 64),              // unique local
            temporary("2001:db8:1::abcd"),          // privacy address
            deprecated("2001:db8:2::1"),            // renumbered away
            preferred("2001:db8:1::211:22ff:fe33:4455"), // SLAAC EUI-64
            entry("2001:db8:1::5", 64),             // lifetime not known
            entry("::1", 128),                      // loopback
            entry("::ffff:198.51.100.9"),           // mapped
            entry("192.168.1.20", 24),              // private
            entry("10.0.0.2", 8),                   // private
            entry("172.16.4.1", 12),                // private
            entry("100.64.1.1", 10),                // carrier-grade NAT
            entry("169.254.3.3", 16),               // link-local
            entry("127.0.0.1", 8),                  // loopback
            entry("1.2.3.4", 24),                  // public
            entry("192.0.2.7", 24),                 // documentation
        };
        const QStringList got =
            CoreAddresses::dialable(entries, QHostAddress(QHostAddress::Any), 47910);
        QCOMPARE(got, (QStringList{QStringLiteral("[2001:db8:1::5]:47910"),
                                   QStringLiteral("[2001:db8:1:0:211:22ff:fe33:4455]:47910"),
                                   QStringLiteral("1.2.3.4:47910")}));
    }

    // The Rock's end1 as the lead read it (2026-09-29): six global
    // temporary addresses, one current and five deprecated, beside one
    // stable EUI-64 (mngtmpaddr) address; wlan0 one stable. Only the two
    // stable ones are dialable. The stable one's lifetime is finite, as a
    // SLAAC address's is, so Qt calls it isTemporary(); that is why the
    // filter never reads isTemporary().
    void aTemporaryAndDeprecatedMixYieldsOnlyTheStable()
    {
        QList<QNetworkAddressEntry> entries;
        QNetworkAddressEntry current = temporary("2001:db8:1:0:a1b2:c3d4:e5f6:1");
        current.setAddressLifetime(QDeadlineTimer(3600 * 1000), QDeadlineTimer(7200 * 1000));
        entries.append(current);
        for (int i = 2; i <= 6; ++i) {
            QNetworkAddressEntry old =
                temporary(qPrintable(QStringLiteral("2001:db8:1:0:a1b2:c3d4:e5f6:%1").arg(i)));
            old.setAddressLifetime(QDeadlineTimer(0), QDeadlineTimer(3600 * 1000));
            entries.append(old);
        }
        const QNetworkAddressEntry stable = preferred("2001:db8:1:0:211:22ff:fe33:4455");
        QVERIFY(stable.isTemporary());
        entries.append(stable);
        entries.append(preferred("2001:db8:1:0:211:22ff:fe33:9999"));
        QCOMPARE(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::Any), 50055),
                 (QStringList{QStringLiteral("[2001:db8:1:0:211:22ff:fe33:4455]:50055"),
                              QStringLiteral("[2001:db8:1:0:211:22ff:fe33:9999]:50055")}));
    }

    // The same mix as Linux's /proc/net/if_inet6 shows it, when Qt reports
    // no flags at all: the kernel's temporary (0x01) and deprecated (0x20)
    // bits alone drop the six.
    void theKernelsFlagsAloneDropThem()
    {
        const QByteArray procText =
            "20010db800010000a1b2c3d4e5f60001 02 40 00 01     end1\n"
            "20010db800010000a1b2c3d4e5f60002 02 40 00 21     end1\n"
            "20010db800010000a1b2c3d4e5f60003 02 40 00 21     end1\n"
            "20010db800010000a1b2c3d4e5f60004 02 40 00 21     end1\n"
            "20010db800010000a1b2c3d4e5f60005 02 40 00 21     end1\n"
            "20010db800010000a1b2c3d4e5f60006 02 40 00 21     end1\n"
            "20010db800010000021122fffe334455 02 40 00 00     end1\n"
            "20010db800010000021122fffe339999 03 40 00 00    wlan0\n"
            "fe80000000000000021122fffe334455 02 40 20 80     end1\n"
            "00000000000000000000000000000001 01 80 10 80       lo\n"
            "not a line\n";
        const QHash<QString, CoreAddresses::KernelIpv6Flags> flags =
            CoreAddresses::parseIfInet6(procText);
        QCOMPARE(flags.size(), 10);
        QVERIFY(flags.value(QStringLiteral("2001:db8:1:0:a1b2:c3d4:e5f6:1")).temporary);
        QVERIFY(!flags.value(QStringLiteral("2001:db8:1:0:a1b2:c3d4:e5f6:1")).deprecated);
        QVERIFY(flags.value(QStringLiteral("2001:db8:1:0:a1b2:c3d4:e5f6:2")).deprecated);
        QVERIFY(!flags.value(QStringLiteral("2001:db8:1:0:211:22ff:fe33:4455")).temporary);

        QList<QNetworkAddressEntry> plain;
        for (int i = 1; i <= 6; ++i) {
            plain.append(entry(qPrintable(QStringLiteral("2001:db8:1:0:a1b2:c3d4:e5f6:%1").arg(i)), 64));
        }
        plain.append(entry("2001:db8:1:0:211:22ff:fe33:4455", 64));
        plain.append(entry("2001:db8:1:0:211:22ff:fe33:9999", 64));
        // Without the kernel's flags all eight would pass.
        QCOMPARE(CoreAddresses::dialable(plain, QHostAddress(QHostAddress::Any), 50055).size(), 8);
        QCOMPARE(CoreAddresses::dialable(CoreAddresses::withKernelFlags(plain, flags),
                                         QHostAddress(QHostAddress::Any), 50055),
                 (QStringList{QStringLiteral("[2001:db8:1:0:211:22ff:fe33:4455]:50055"),
                              QStringLiteral("[2001:db8:1:0:211:22ff:fe33:9999]:50055")}));
    }

    void thePortIsTheListenersOwn()
    {
        const QList<QNetworkAddressEntry> entries{entry("2001:db8::7", 64)};
        QCOMPARE(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::Any), 50055),
                 QStringList{QStringLiteral("[2001:db8::7]:50055")});
        QVERIFY(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::Any), 0).isEmpty());
    }

    void onlyWhatTheListenerServes()
    {
        const QList<QNetworkAddressEntry> entries{entry("2001:db8::7", 64),
                                                  entry("2001:db8::8", 64),
                                                  entry("1.2.3.4", 24)};
        // A loopback listener serves nobody else.
        QVERIFY(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::LocalHost), 47910)
                    .isEmpty());
        QVERIFY(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::LocalHostIPv6), 47910)
                    .isEmpty());
        // One family, or one address.
        QCOMPARE(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::AnyIPv4), 47910),
                 QStringList{QStringLiteral("1.2.3.4:47910")});
        QCOMPARE(CoreAddresses::dialable(entries, QHostAddress(QHostAddress::AnyIPv6), 47910),
                 (QStringList{QStringLiteral("[2001:db8::7]:47910"),
                              QStringLiteral("[2001:db8::8]:47910")}));
        QCOMPARE(CoreAddresses::dialable(entries, QHostAddress(QStringLiteral("2001:db8::8")),
                                         47910),
                 QStringList{QStringLiteral("[2001:db8::8]:47910")});
        // A listener on a private address: nothing a phone away from home
        // could dial.
        QVERIFY(CoreAddresses::dialable({entry("192.168.1.20", 24)},
                                        QHostAddress(QStringLiteral("192.168.1.20")), 47910)
                    .isEmpty());
    }

    void sortedWithoutRepeatsAndCapped()
    {
        QList<QNetworkAddressEntry> entries;
        // Two interfaces on one prefix list the same address twice.
        entries.append(entry("2001:db8::9", 64));
        entries.append(entry("2001:db8::9", 64));
        for (int i = 20; i >= 10; --i) {
            entries.append(entry(qPrintable(QStringLiteral("2001:db8::%1").arg(i))));
        }
        entries.append(entry("1.2.3.4"));
        const QStringList got =
            CoreAddresses::dialable(entries, QHostAddress(QHostAddress::Any), 47910);
        QCOMPARE(got.size(), CoreAddresses::kMaxAddresses);
        QCOMPARE(got.first(), QStringLiteral("[2001:db8::9]:47910"));
        QCOMPARE(got.at(1), QStringLiteral("[2001:db8::10]:47910"));
        QStringList unique = got;
        QCOMPARE(unique.removeDuplicates(), 0);
        // Every IPv6 address comes before the IPv4 one, so the cap drops it.
        QVERIFY(!got.contains(QStringLiteral("1.2.3.4:47910")));
    }

    void scopeIsDropped()
    {
        QNetworkAddressEntry scoped = entry("2001:db8::7", 64);
        QHostAddress ip = scoped.ip();
        ip.setScopeId(QStringLiteral("en0"));
        scoped.setIp(ip);
        QCOMPARE(CoreAddresses::dialable({scoped}, QHostAddress(QHostAddress::Any), 47910),
                 QStringList{QStringLiteral("[2001:db8::7]:47910")});
    }

    void wireForm()
    {
        QCOMPARE(CoreAddresses::toJson({}), QStringLiteral("{\"addresses\":[]}"));
        QCOMPARE(CoreAddresses::toJson({QStringLiteral("[2001:db8::7]:47910"),
                                        QStringLiteral("1.2.3.4:47910")}),
                 QStringLiteral("{\"addresses\":[\"[2001:db8::7]:47910\",\"1.2.3.4:47910\"]}"));
    }

    void watcherSaysEachChangeOnce()
    {
        QList<QNetworkAddressEntry> interfaces{entry("2001:db8::7", 64)};
        CoreAddressWatcher watcher;
        watcher.setEntrySource([&interfaces]() { return interfaces; });
        QSignalSpy changed(&watcher, &CoreAddressWatcher::addressesChanged);

        // Nothing while no listener runs.
        watcher.refresh();
        QCOMPARE(changed.count(), 0);
        QVERIFY(watcher.addresses().isEmpty());

        watcher.start(QHostAddress(QHostAddress::Any), 47910);
        QVERIFY(watcher.isActive());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(watcher.addresses(), QStringList{QStringLiteral("[2001:db8::7]:47910")});

        // The same interfaces again: nothing new.
        watcher.refresh();
        QCOMPARE(changed.count(), 1);

        // A new temporary address arrives: still nothing new.
        interfaces.append(temporary("2001:db8::abcd"));
        watcher.refresh();
        QCOMPARE(changed.count(), 1);

        // SLAAC renumbering: the old prefix is deprecated, a new one comes.
        interfaces = {deprecated("2001:db8::7"), entry("2001:db8:9::7", 64)};
        watcher.refresh();
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.last().first().toStringList(),
                 QStringList{QStringLiteral("[2001:db8:9::7]:47910")});

        // The listener closes: no addresses.
        watcher.stop();
        QVERIFY(!watcher.isActive());
        QCOMPARE(changed.count(), 3);
        QVERIFY(watcher.addresses().isEmpty());
    }

    void watcherReadsAgainWhileListening()
    {
        CoreAddressWatcher watcher;
        watcher.setEntrySource([]() { return QList<QNetworkAddressEntry>{}; });
        QVERIFY(!watcher.isRefreshing());
        watcher.start(QHostAddress(QHostAddress::Any), 47910);
        QVERIFY(watcher.isRefreshing());
        QCOMPARE(watcher.refreshIntervalMs(), CoreAddresses::kRefreshIntervalMs);
        watcher.stop();
        QVERIFY(!watcher.isRefreshing());
    }
};

QTEST_MAIN(TestCoreAddresses)
#include "tst_core_addresses.moc"
