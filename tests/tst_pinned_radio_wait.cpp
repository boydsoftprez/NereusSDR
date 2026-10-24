// =================================================================
// tests/tst_pinned_radio_wait.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original: no Thetis or AetherSDR upstream.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Startup auto-connect to a pinned radio (PinnedRadioWait,
//                 review of PR 600, Important 1): a listed radio connects
//                 at once, a radio on a late adapter still connects, a
//                 joined startup scan is not stopped, and the give-up comes
//                 only after the scan ends. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-22 - A scan started by a held-back request is not stopped by
//                 the backstop. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QSignalSpy>
#include <QTimer>
#include <QUdpSocket>
#include <QtTest>

#include "core/PinnedRadioWait.h"
#include "core/RadioDiscovery.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("00:1C:C0:A2:14:8B");

// A fake radio on the loopback. It counts the P1 probes it receives and
// answers only the one numbered `answerProbe` (0: never answers), so a
// walk over several "adapters" (NIC entries pointing at it) finds it on
// the adapter of that number.
class LateFakeRadio : public QObject {
    Q_OBJECT
public:
    explicit LateFakeRadio(int answerProbe, QObject* parent = nullptr)
        : QObject(parent), m_answerProbe(answerProbe) {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, &LateFakeRadio::onReadyRead);
    }
    int p1Probes() const { return m_p1Probes; }

    QList<RadioDiscovery::ScanNic> nics(int count) const {
        QList<RadioDiscovery::ScanNic> list;
        for (int i = 0; i < count; ++i) {
            RadioDiscovery::ScanNic nic;
            nic.name = QStringLiteral("loopback-test-%1").arg(i);
            nic.local = QHostAddress::LocalHost;
            nic.directed = QHostAddress::LocalHost;
            nic.generalBroadcast = false;
            nic.port = m_socket->localPort();
            list.append(nic);
        }
        return list;
    }

private slots:
    void onReadyRead() {
        while (m_socket->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(m_socket->pendingDatagramSize()));
            QHostAddress src;
            quint16 srcPort = 0;
            m_socket->readDatagram(buf.data(), buf.size(), &src, &srcPort);
            if (buf.size() < 3 || static_cast<quint8>(buf[0]) != 0xEF
                || static_cast<quint8>(buf[1]) != 0xFE
                || static_cast<quint8>(buf[2]) != 0x02) {
                continue;   // the P2 probe, not answered
            }
            ++m_p1Probes;
            if (m_p1Probes != m_answerProbe) {
                continue;
            }
            QByteArray reply(63, 0);
            reply[0] = char(0xEF); reply[1] = char(0xFE); reply[2] = 0x02;
            reply[3] = 0x00; reply[4] = 0x1C; reply[5] = char(0xC0);
            reply[6] = char(0xA2); reply[7] = 0x14; reply[8] = char(0x8B);
            reply[9] = 75;          // firmware
            reply[10] = 6;          // boardId = HL2
            m_socket->writeDatagram(reply, src, srcPort);
        }
    }

private:
    QUdpSocket* m_socket{nullptr};
    int m_answerProbe{0};
    int m_p1Probes{0};
};

RadioInfo pinnedRadio()
{
    RadioInfo info;
    info.macAddress = kMac;
    info.address = QHostAddress::LocalHost;
    info.protocol = ProtocolVersion::Protocol1;
    return info;
}

} // namespace

class TstPinnedRadioWait : public QObject {
    Q_OBJECT
private slots:
    void init() { RadioDiscovery::clearHoldOffForTest(); }

    // The startup scan replied before the wait began: the radio is in the
    // list, so it connects at once, with no scan of its own and no wait.
    void aListedRadioConnectsWithoutWaiting() {
        LateFakeRadio radio(0);
        RadioDiscovery disc;
        // Loopback only, so a scan started by mistake never leaves the machine.
        disc.setScanNicsForTest(radio.nics(1));
        disc.setTimingForTest({1, 2, 50});
        disc.injectLastSeenForTest(kMac, pinnedRadio(), QDateTime::currentMSecsSinceEpoch());
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);

        PinnedRadioWait wait(&disc, kMac);
        QSignalSpy found(&wait, &PinnedRadioWait::found);
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        wait.start();

        QCOMPARE(found.count(), 1);   // inside start(), no event loop turn
        QCOMPARE(found.first().at(0).value<RadioInfo>().macAddress, kMac);
        QCOMPARE(gaveUp.count(), 0);
        QCOMPARE(started.count(), 0);
        QVERIFY(!wait.startedScan());
        QVERIFY(!disc.isScanRunning());
        QCOMPARE(radio.p1Probes(), 0);
    }

    // The pinned radio is on the fourth adapter of the startup scan and
    // replies more than 3 s into it. The wait joins that scan and still
    // connects. A fixed 3 s timeout gave up and stopped the scan first.
    void aRadioOnALateAdapterStillConnects() {
        constexpr int kNics = 4;
        constexpr int kQuiet = 5;
        constexpr int kPollMs = 250;   // 1.25 s per adapter
        LateFakeRadio radio(kNics);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics(kNics));
        disc.setTimingForTest({1, kQuiet, kPollMs});

        disc.startDiscovery();   // the startup scan
        PinnedRadioWait wait(&disc, kMac);
        QSignalSpy found(&wait, &PinnedRadioWait::found);
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        QElapsedTimer clock;
        clock.start();
        wait.start();
        QVERIFY(!wait.startedScan());

        QVERIFY(found.wait(20000));
        qInfo().noquote() << QStringLiteral("found after %1 ms, on probe %2")
                                 .arg(clock.elapsed()).arg(radio.p1Probes());
        QCOMPARE(found.count(), 1);
        QCOMPARE(gaveUp.count(), 0);
        QCOMPARE(radio.p1Probes(), kNics);   // the reply came on the last adapter
        // That adapter starts after three whole quiet budgets: the reply
        // cannot have come in under 3 s.
        QVERIFY((kNics - 1) * kQuiet * kPollMs > 3000);
    }

    // The wait outlives its backstop on the startup scan it joined. That
    // scan is not stopped (radios on later adapters still reach the list),
    // and the give-up waits for its end.
    void aJoinedScanIsNotStoppedOnGiveUp() {
        constexpr int kNics = 2;
        LateFakeRadio radio(0);   // never answers
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics(kNics));
        disc.setTimingForTest({1, 8, 100});
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        disc.startDiscovery();   // the startup scan
        PinnedRadioWait wait(&disc, kMac);
        wait.setBackstopForTest(100);
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        wait.start();
        QVERIFY(!wait.startedScan());

        // Past the backstop, before the scan's first adapter is done.
        QTRY_VERIFY_WITH_TIMEOUT(disc.scanStatsForTest().quietPolls >= 3, 5000);
        QVERIFY(disc.isScanRunning());
        QCOMPARE(finished.count(), 0);
        QCOMPARE(gaveUp.count(), 0);

        // The scan runs to its end over both adapters, then the wait gives up.
        QVERIFY(finished.wait(10000));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(started.count(), 1);
        QCOMPARE(radio.p1Probes(), kNics);
        QCOMPARE(gaveUp.count(), 1);
    }

    // A scan the wait started itself is stopped when it outlives the
    // backstop, and the wait gives up then.
    void aScanTheWaitStartedIsStoppedOnGiveUp() {
        LateFakeRadio radio(0);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics(2));
        disc.setTimingForTest({1, 8, 100});

        PinnedRadioWait wait(&disc, kMac);
        wait.setBackstopForTest(100);
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        wait.start();
        QVERIFY(wait.startedScan());
        QVERIFY(disc.isScanRunning());

        QVERIFY(gaveUp.wait(5000));
        QVERIFY(!disc.isScanRunning());
        QCOMPARE(radio.p1Probes(), 1);   // the second adapter was never probed
    }

    // Re-review of PR 600: a start the quiet period holds back does not make
    // the scan that later runs the wait's own (another start may have run
    // first). Its backstop leaves that scan running.
    void aScanStartedAfterAHoldOffIsNotStoppedOnGiveUp() {
        LateFakeRadio radio(0);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics(2));
        disc.setTimingForTest({1, 8, 100});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        disc.holdOffScans(std::chrono::milliseconds(150));

        PinnedRadioWait wait(&disc, kMac);
        wait.setBackstopForTest(100);
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        wait.start();
        QVERIFY(!disc.isScanRunning());

        // The held start runs; the backstop is armed when it starts and
        // fires before the first adapter's eight quiet windows are done.
        QTRY_VERIFY_WITH_TIMEOUT(disc.isScanRunning(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(disc.scanStatsForTest().quietPolls >= 3, 5000);
        QVERIFY(disc.isScanRunning());
        QCOMPARE(gaveUp.count(), 0);

        QVERIFY(finished.wait(10000));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(radio.p1Probes(), 2);
        QCOMPARE(gaveUp.count(), 1);
        QVERIFY(!wait.startedScan());
    }

    // No give-up (the "isn't reachable" toast) before the scan ends: when
    // it comes, every attempt on every adapter has gone quiet (the radio
    // never answers, so each ends after exactly kQuiet quiet windows), and
    // the scan is longer than 3 s.
    void noGiveUpBeforeTheScanEnds() {
        constexpr int kNics = 3;
        constexpr int kQuiet = 5;
        constexpr int kPollMs = 250;
        LateFakeRadio radio(0);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics(kNics));
        disc.setTimingForTest({1, kQuiet, kPollMs});
        QVERIFY(kNics * kQuiet * kPollMs > 3000);

        disc.startDiscovery();   // the startup scan
        PinnedRadioWait wait(&disc, kMac);
        bool scanRanAtGiveUp = true;
        int quietPollsAtGiveUp = -1;
        connect(&wait, &PinnedRadioWait::gaveUp, this, [&]() {
            scanRanAtGiveUp = disc.isScanRunning();
            quietPollsAtGiveUp = disc.scanStatsForTest().quietPolls;
        });
        QSignalSpy gaveUp(&wait, &PinnedRadioWait::gaveUp);
        wait.start();

        QVERIFY(gaveUp.wait(20000));
        QCOMPARE(gaveUp.count(), 1);
        QVERIFY2(!scanRanAtGiveUp, "gave up while the scan was still running");
        QCOMPARE(quietPollsAtGiveUp, kNics * kQuiet);
        QCOMPARE(radio.p1Probes(), kNics);
    }
};

QTEST_MAIN(TstPinnedRadioWait)
#include "tst_pinned_radio_wait.moc"
