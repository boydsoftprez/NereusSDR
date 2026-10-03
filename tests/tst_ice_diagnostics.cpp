// =================================================================
// tests/tst_ice_diagnostics.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The opt-in ICE check log
// (IceDiagnostics): its switch, the address redactor every line passes
// through, the per-packet lines it leaves out, and a real pair of peers on
// this computer whose whole log carries none of its addresses.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-30 - Review fix: the username fragment in every form, the
//                 STUN username, realm and nonce, and the "yes" switch.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDeadlineTimer>
#include <QMutex>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QRegularExpression>

#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>

#include "core/session/CoreAddresses.h"
#include "core/session/IceDiagnostics.h"

using namespace NereusSDR;

namespace {

QNetworkAddressEntry entry(const char* ip)
{
    QNetworkAddressEntry e;
    e.setIp(QHostAddress(QString::fromLatin1(ip)));
    return e;
}

// Every literal the unit cases use; none may survive a redaction.
const char* const kLiterals[] = {
    "203.0.113.7", "198.51.100.23", "192.168.1.20", "10.0.0.5",
    "2001:db8:1:2:3:4:5:6", "2001:0db8:0000:0000:0000:0000:0000:0042", "2001:db8::42",
    "2001:db8::1", "fe80::1c2b:3a4d:5e6f:7081", "2600:1700:abcd:12::99",
    "2001:db8:aa::211:22ff:fe33:4455", "2001:db8:aa::9c3e:71d2:a0b4:5f11",
};

void expectNoLiteral(const QString& text)
{
    for (const char* literal : kLiterals) {
        const QString l = QString::fromLatin1(literal);
        QVERIFY2(!text.contains(l, Qt::CaseInsensitive), qPrintable(text));
    }
}

} // namespace

class TestIceDiagnostics : public QObject {
    Q_OBJECT

private slots:
    void ipv4IsRedacted()
    {
        IceAddressRedactor r;
        const QString out = r.redact(QStringLiteral("Sending request to 203.0.113.7:3478"));
        QCOMPARE(out, QStringLiteral("Sending request to <v4#1>:3478"));
    }

    void fullAndCompressedIpv6AreOneAddress()
    {
        IceAddressRedactor r;
        const QString a = r.redact(QStringLiteral("a 2001:0db8:0000:0000:0000:0000:0000:0042 b"));
        const QString b = r.redact(QStringLiteral("c 2001:db8::42 d"));
        QCOMPARE(a, QStringLiteral("a <v6#1> b"));
        QCOMPARE(b, QStringLiteral("c <v6#1> d"));
        QCOMPARE(r.redact(QStringLiteral("2001:db8:1:2:3:4:5:6")), QStringLiteral("<v6#2>"));
    }

    void unbracketedIpv6WithPortKeepsThePort()
    {
        // libjuice prints "host:port" for both families, unbracketed.
        IceAddressRedactor r;
        QCOMPARE(r.redact(QStringLiteral("Received STUN datagram from 2001:db8::1:50123")),
                 QStringLiteral("Received STUN datagram from <v6#1>:50123"));
        // A four-digit port reads as part of the address unless the address
        // is already known; either way nothing of the address is left.
        QCOMPARE(r.redact(QStringLiteral("to 2001:db8::1:3478")),
                 QStringLiteral("to <v6#1>:3478"));
    }

    void v4MappedIsTheIpv4Address()
    {
        IceAddressRedactor r;
        QCOMPARE(r.redact(QStringLiteral("x 198.51.100.23 y ::ffff:198.51.100.23 z")),
                 QStringLiteral("x <v4#1> y <v4#1> z"));
        QCOMPARE(r.redact(QStringLiteral("[::ffff:198.51.100.23]:9000")),
                 QStringLiteral("<v4#1>:9000"));
    }

    void scopedLinkLocalKeepsItsScope()
    {
        IceAddressRedactor r;
        QCOMPARE(r.redact(QStringLiteral("from fe80::1c2b:3a4d:5e6f:7081%en0:50000")),
                 QStringLiteral("from <v6#1>%en0:50000"));
        QCOMPARE(r.redact(QStringLiteral("fe80::1c2b:3a4d:5e6f:7081%2")),
                 QStringLiteral("<v6#1>%2"));
    }

    void bracketedIpv6WithPort()
    {
        IceAddressRedactor r;
        QCOMPARE(r.redact(QStringLiteral("dial [2600:1700:abcd:12::99]:50055 now")),
                 QStringLiteral("dial <v6#1>:50055 now"));
    }

    void candidateLinesCarryTheType()
    {
        IceAddressRedactor r;
        const QString srflx = r.redact(QStringLiteral(
            "a=candidate:2 1 UDP 1686052607 203.0.113.7 61000 typ srflx raddr 192.168.1.20 "
            "rport 61000"));
        QCOMPARE(srflx, QStringLiteral(
            "a=candidate:2 1 UDP 1686052607 <v4#1 srflx> 61000 typ srflx raddr <v4#2 host> "
            "rport 61000"));
        // The type sticks to the address for the rest of the run.
        QCOMPARE(r.redact(QStringLiteral("Sending request to 203.0.113.7:61000")),
                 QStringLiteral("Sending request to <v4#1 srflx>:61000"));
        const QString relay = r.redact(QStringLiteral(
            "candidate:3 1 UDP 16777215 10.0.0.5 50000 typ relay raddr 198.51.100.23 rport 1"));
        QCOMPARE(relay, QStringLiteral(
            "candidate:3 1 UDP 16777215 <v4#3 relay> 50000 typ relay raddr <v4#4 srflx> rport 1"));
        const QString host6 = r.redact(QStringLiteral(
            "a=candidate:1 1 UDP 2122317823 2001:db8::42 50001 typ host"));
        QCOMPARE(host6, QStringLiteral(
            "a=candidate:1 1 UDP 2122317823 <v6#1 host> 50001 typ host"));
    }

    void addressesInsideSdpAreRedacted()
    {
        IceAddressRedactor r;
        const QString sdp = QStringLiteral(
            "v=0\r\no=rtc 123 0 IN IP4 127.0.0.1\r\nc=IN IP6 2001:db8::42\r\n"
            "a=fingerprint:sha-256 AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:"
            "23:45:67:89:AB:CD:EF:01:23:45:67:89\r\n"
            "a=candidate:1 1 UDP 2122317823 192.168.1.20 50001 typ host\r\n");
        const QString out = r.redact(sdp);
        expectNoLiteral(out);
        QVERIFY(out.contains(QStringLiteral("IN IP4 loopback")));
        QVERIFY(out.contains(QStringLiteral("IN IP6 <v6#1>")));
        // A fingerprint is not an address.
        QVERIFY(out.contains(QStringLiteral("AB:CD:EF:01:23:45:67:89:AB:CD")));
        QVERIFY(out.contains(QStringLiteral("<v4#1 host> 50001 typ host")));
    }

    void loopbackIsNamed()
    {
        IceAddressRedactor r;
        QCOMPARE(r.redact(QStringLiteral("wsrelay1 127.0.0.1 40000 typ host / ::1:5000")),
                 QStringLiteral("wsrelay1 loopback 40000 typ host / loopback:5000"));
    }

    void repeatedAddressKeepsItsToken()
    {
        IceAddressRedactor r;
        r.redact(QStringLiteral("198.51.100.23"));
        r.redact(QStringLiteral("2001:db8::1"));
        QCOMPARE(r.redact(QStringLiteral("203.0.113.7 198.51.100.23 203.0.113.7 2001:db8::1")),
                 QStringLiteral("<v4#2> <v4#1> <v4#2> <v6#1>"));
    }

    void codeAndWordsAreLeftAlone()
    {
        IceAddressRedactor r;
        const QString line = QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: STUN entry 3: Failed; "
            "std::string face::add, version 0.24.5, size=1200 12:30:45");
        QCOMPARE(r.redact(line), line);
    }

    void theCoresOwnIpv6IsMarked()
    {
        IceAddressRedactor r;
        QNetworkAddressEntry stable = entry("2001:db8:aa::211:22ff:fe33:4455");
        QNetworkAddressEntry temporary = entry("2001:db8:aa::9c3e:71d2:a0b4:5f11");
        temporary.setDnsEligibility(QNetworkAddressEntry::DnsIneligible);
        QNetworkAddressEntry deprecated = entry("2001:db8::1");
        deprecated.setAddressLifetime(QDeadlineTimer(0), QDeadlineTimer::Forever);
        r.setLocalAddresses({stable, temporary, deprecated, entry("192.168.1.20")});

        QCOMPARE(r.redact(QStringLiteral(
                     "a=candidate:1 1 UDP 2122317823 2001:db8:aa::9c3e:71d2:a0b4:5f11 5 typ host")),
                 QStringLiteral("a=candidate:1 1 UDP 2122317823 <v6#1 host temporary> 5 typ host"));
        QCOMPARE(r.redact(QStringLiteral("[2001:db8:aa::211:22ff:fe33:4455]:50055")),
                 QStringLiteral("<v6#2 stable eui64>:50055"));
        QCOMPARE(r.redact(QStringLiteral("2001:db8::1")),
                 QStringLiteral("<v6#3 stable deprecated>"));
        // A peer's address is never marked.
        QCOMPARE(r.redact(QStringLiteral("2600:1700:abcd:12::99")), QStringLiteral("<v6#4>"));
    }

    void localSummaryListsTheCoresIpv6()
    {
        IceAddressRedactor r;
        QNetworkAddressEntry temporary = entry("2001:db8:aa::9c3e:71d2:a0b4:5f11");
        temporary.setDnsEligibility(QNetworkAddressEntry::DnsIneligible);
        r.setLocalAddresses({entry("2001:db8:aa::211:22ff:fe33:4455"), temporary,
                             entry("fe80::1c2b:3a4d:5e6f:7081"), entry("192.168.1.20")});
        const QString summary = r.localSummary();
        expectNoLiteral(summary);
        QVERIFY2(summary.contains(QStringLiteral("<v6#1 stable eui64>")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("<v6#2 temporary>")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("<v4#1>")), qPrintable(summary));
    }

    void nothingLeaks()
    {
        IceAddressRedactor r;
        QString all;
        for (const char* literal : kLiterals) {
            const QString l = QString::fromLatin1(literal);
            all += r.redact(QStringLiteral("a %1 b [%1]:1 c %1:40000 d x%1y").arg(l));
        }
        expectNoLiteral(all);
    }

    void switchValues()
    {
        QVERIFY(IceDiagnostics::switchRequested(QByteArrayLiteral("1")));
        QVERIFY(IceDiagnostics::switchRequested(QByteArrayLiteral("true")));
        QVERIFY(IceDiagnostics::switchRequested(QByteArrayLiteral("on")));
        QVERIFY(IceDiagnostics::switchRequested(QByteArrayLiteral("yes")));
        QVERIFY(IceDiagnostics::switchRequested(QByteArrayLiteral(" YES ")));
        QVERIFY(!IceDiagnostics::switchRequested(QByteArray()));
        QVERIFY(!IceDiagnostics::switchRequested(QByteArrayLiteral("0")));
        QVERIFY(!IceDiagnostics::switchRequested(QByteArrayLiteral("no")));
    }

    void perPacketLinesAreLeftOut()
    {
        QVERIFY(IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::DtlsTransport::outgoing@1: Send size=120")));
        QVERIFY(IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: Received datagram, size=120")));
        QVERIFY(!IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: STUN entry 2: Failed")));
        QVERIFY(!IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: Cancelling check for "
            "lower-priority pair")));
        // As libjuice really writes them: its source file and line first.
        QVERIFY(IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: conn_poll.c:412: "
            "Receiving datagram")));
        QVERIFY(IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: agent.c:1733: "
            "STUN entry 0 pair matching incoming address")));
        QVERIFY(!IceDiagnostics::isLibraryNoise(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: agent.c:1502: "
            "Candidate pair check succeeded")));
    }

    void passwordsAreHidden()
    {
        QStringList lines;
        IceDiagnostics::installForTest([&](const QString& line) { lines.append(line); });
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: agent.c:1626: STUN integrity check failed, password=\"s3cretPw\""));
        IceDiagnostics::logPath("control", QStringLiteral("a=ice-pwd:AzkrYDCUsPbwbZd"));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: ice.c:173: Created local description: ufrag=\"LrY/\", pwd=\"L3mmGGjqLCcSITly1m+ChZ\""));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: ice.c:142: Parsed remote description: ufrag=\"9NCk\", pwd=\"zIrWy2ZPIijuUYVSb2rtre\", candidates=0"));
        IceDiagnostics::installForTest({});
        const QString all = lines.join(QLatin1Char('\n'));
        QVERIFY2(!all.contains(QStringLiteral("s3cretPw")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("AzkrYDCUsPbwbZd")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("password=\"<hidden>\"")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("L3mmGGjqLCcSITly1m+ChZ")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("zIrWy2ZPIijuUYVSb2rtre")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("pwd=\"<hidden>\", candidates=0")), qPrintable(all));
    }

    // Review fix (I-1, M-3): the ICE username fragment in every form
    // libjuice prints it, and the STUN username, realm and nonce.
    void usernameFragmentsAreHidden()
    {
        QStringList lines;
        IceDiagnostics::installForTest([&](const QString& line) { lines.append(line); });
        // ice.c:172 and ice.c:141.
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: ice.c:173: Created local description: ufrag=\"LrY/\", pwd=\"L3mmGG\""));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: ice.c:142: Parsed remote description: ufrag=\"9NCk\", pwd=\"zIrWy2\", candidates=0"));
        // agent.c:505 over ice.c:302: the description's first line.
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: agent.c:505: Generated local SDP description: a=ice-ufrag:Qw7e\r\n"
            "a=ice-pwd:Pz0kLmnopq\r\n"));
        IceDiagnostics::logPath("control", QStringLiteral("a=ice-ufrag:Hq2x"));
        // agent.c:1286 and agent.c:1296.
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: agent.c:1286: STUN local ufrag check failed, expected=\"Aa1b\", actual=\"Cc2d\""));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: agent.c:1296: STUN remote ufrag check failed, expected=\"Ee3f\", actual=\"Gg4h\""));
        // stun.c:713, :763 and :774, on a path other than the library's.
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: stun.c:713: Got username: Jj5k:Ll6m"));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: stun.c:763: Got realm: shack realm Nn7p"));
        IceDiagnostics::logPath("control", QStringLiteral(
            "juice: stun.c:774: Got nonce: Qq8rNonce"));
        IceDiagnostics::installForTest({});
        const QString all = lines.join(QLatin1Char('\n'));
        for (const char* secret : {"LrY/", "9NCk", "Qw7e", "Pz0kLmnopq", "Hq2x", "Aa1b", "Cc2d",
                                   "Ee3f", "Gg4h", "Jj5k", "Ll6m", "Nn7p", "shack realm",
                                   "Qq8rNonce"}) {
            QVERIFY2(!all.contains(QLatin1String(secret)), qPrintable(all));
        }
        QVERIFY2(all.contains(QStringLiteral("ufrag=\"<hidden>\"")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("a=ice-ufrag:<hidden>")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("expected=\"<hidden>\", actual=\"<hidden>\"")),
                 qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("Got username: <hidden>")), qPrintable(all));
        // The per-message STUN lines are left out of the library's log whole.
        for (const char* line : {"Got username: Jj5k:Ll6m", "Got realm: shack", "Got nonce: Qq8r"}) {
            QVERIFY(IceDiagnostics::isSecretLine(
                QStringLiteral("rtc::impl::IceTransport::LogCallback@391: juice: stun.c:713: ")
                + QLatin1String(line)));
            QVERIFY(!IceDiagnostics::isLibraryNoise(
                QStringLiteral("rtc::impl::IceTransport::LogCallback@391: juice: stun.c:713: ")
                + QLatin1String(line)));
        }
        QVERIFY(!IceDiagnostics::isSecretLine(QStringLiteral(
            "rtc::impl::IceTransport::LogCallback@391: juice: agent.c:1502: "
            "Candidate pair check succeeded")));
    }

    void offWritesNothing()
    {
        QVERIFY(!IceDiagnostics::enabled());
        // No sink, nothing to write to; the call is a no-op.
        IceDiagnostics::logPath("control", QStringLiteral("203.0.113.7"));
    }

    // A pair of real peers on this computer: every line the log writes goes
    // through the redactor, and none carries any of this computer's own
    // addresses. Its lines are the sample excerpt in the lane report.
    void realPeersLeakNothing()
    {
        QMutex mutex;
        QStringList lines;
        QStringList ufrags;
        // The Core's own address source, as the switch installs it.
        IceDiagnostics::installForTest([&](const QString& line) {
            QMutexLocker lock(&mutex);
            lines.append(line);
        }, &CoreAddresses::localEntries);
        QVERIFY(IceDiagnostics::enabled());

        {
            rtc::Configuration config;
            auto offerer = std::make_shared<rtc::PeerConnection>(config);
            auto answerer = std::make_shared<rtc::PeerConnection>(config);
            const std::weak_ptr<rtc::PeerConnection> weakOfferer = offerer;
            const std::weak_ptr<rtc::PeerConnection> weakAnswerer = answerer;
            offerer->onLocalDescription([weakAnswerer, &mutex, &ufrags](rtc::Description d) {
                if (const auto ufrag = d.iceUfrag()) {
                    QMutexLocker lock(&mutex);
                    ufrags.append(QString::fromStdString(*ufrag));
                }
                if (auto p = weakAnswerer.lock()) { p->setRemoteDescription(d); }
            });
            offerer->onLocalCandidate([weakAnswerer](rtc::Candidate c) {
                IceDiagnostics::logPath("test", QStringLiteral("local candidate %1")
                                                    .arg(QString::fromStdString(c.candidate())));
                if (auto p = weakAnswerer.lock()) { p->addRemoteCandidate(c); }
            });
            answerer->onLocalDescription([weakOfferer, &mutex, &ufrags](rtc::Description d) {
                if (const auto ufrag = d.iceUfrag()) {
                    QMutexLocker lock(&mutex);
                    ufrags.append(QString::fromStdString(*ufrag));
                }
                if (auto p = weakOfferer.lock()) { p->setRemoteDescription(d); }
            });
            answerer->onLocalCandidate([weakOfferer](rtc::Candidate c) {
                if (auto p = weakOfferer.lock()) { p->addRemoteCandidate(c); }
            });
            std::atomic<bool> open{false};
            answerer->onDataChannel([&open](std::shared_ptr<rtc::DataChannel>) { open = true; });
            auto channel = offerer->createDataChannel("diag");
            QTRY_VERIFY_WITH_TIMEOUT(open.load(), 10000);
            IceDiagnostics::logSelectedPair("test", *offerer);
            channel->close();
            offerer->close();
            answerer->close();
        }
        IceDiagnostics::installForTest({});
        QVERIFY(!IceDiagnostics::enabled());

        QMutexLocker lock(&mutex);
        const QString all = lines.join(QLatin1Char('\n'));
        QVERIFY2(!lines.isEmpty(), "no lines");
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            for (const QNetworkAddressEntry& e : iface.addressEntries()) {
                QHostAddress ip = e.ip();
                ip.setScopeId(QString());
                if (ip.isLoopback()) {
                    continue;
                }
                QVERIFY2(!all.contains(ip.toString(), Qt::CaseInsensitive),
                         "an address of this computer reached the log");
            }
        }
        QVERIFY(all.contains(QStringLiteral("[test] selected pair")));
        QVERIFY2(!all.contains(QStringLiteral("a=ice-pwd:")), "a session description reached the log");
        QVERIFY2(!all.contains(QRegularExpression(QStringLiteral("pwd=\"[^<]"))),
                 "an ICE password reached the log");
        QVERIFY2(!all.contains(QStringLiteral("Receiving datagram")), "per-packet lines reached the log");
        // Review fix (I-1): no ICE username fragment, in any form.
        QCOMPARE(ufrags.size(), 2);
        for (const QString& ufrag : std::as_const(ufrags)) {
            QVERIFY(!ufrag.isEmpty());
            QVERIFY2(!all.contains(ufrag), "an ICE username fragment reached the log");
        }
        QVERIFY2(!all.contains(QRegularExpression(QStringLiteral("ufrag[=:]\"?[^<\"]"))),
                 "an ICE username fragment reached the log");
        const QByteArray excerpt = qgetenv("NEREUS_ICE_DIAG_EXCERPT");
        if (!excerpt.isEmpty()) {
            QFile file(QString::fromLocal8Bit(excerpt));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write(all.toUtf8());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestIceDiagnostics)
#include "tst_ice_diagnostics.moc"
