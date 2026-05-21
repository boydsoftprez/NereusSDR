// =================================================================
// tests/tst_smart_sdr_api_listener_ptt_chain.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream port. Covers C1 to C6 from the
// approved design doc docs/architecture/4o3a-lan-ptt-pcap-divergence.md
// (commit 559890a2).
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Smoke test (Task 0 foundation).
// =================================================================

#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QHostAddress>

#include "core/SmartSdrApiListener.h"

using NereusSDR::SmartSdrApiListener;

namespace {

// Helper: wait until `pred()` returns true or `timeoutMs` elapses, pumping
// the Qt event loop. Used because the listener does its work over queued
// signals + async TCP, so blocking sleeps would deadlock.
template<typename Pred>
bool waitFor(Pred pred, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (!pred() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return pred();
}

// Drain everything readable on `sock` into a QByteArray, with up to
// `timeoutMs` to let bytes arrive. Test-side mirror of what Wireshark
// would record for one TCP-4992 client.
QByteArray drain(QTcpSocket* sock, int timeoutMs = 200)
{
    QByteArray out;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (sock->bytesAvailable() > 0) {
            out.append(sock->readAll());
        }
    }
    return out;
}

}  // namespace

class SmartSdrApiListenerPttChainTest : public QObject
{
    Q_OBJECT

private slots:
    void smoke_listenerAcceptsClientAndSendsBanner();
};

// Task 0 smoke test: prove the harness machinery works end-to-end.
// Start the listener on loopback + ephemeral port, connect a QTcpSocket,
// and confirm the V/H banner pair arrives. If this passes, every later
// test in this file has a working foundation.
void SmartSdrApiListenerPttChainTest::smoke_listenerAcceptsClientAndSendsBanner()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();
    QVERIFY(port > 0);

    QTcpSocket fakeAmp;
    fakeAmp.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(fakeAmp.waitForConnected(1000));

    const QByteArray bytes = drain(&fakeAmp);
    const QString text = QString::fromUtf8(bytes);
    QVERIFY2(text.startsWith(QStringLiteral("V1.4.0.0\n")),
             qPrintable(QStringLiteral("expected V<ver>\\n prefix, got: ") + text));
    QVERIFY2(text.contains(QStringLiteral("\nH")),
             qPrintable(QStringLiteral("expected H<handle>\\n line, got: ") + text));
}

QTEST_MAIN(SmartSdrApiListenerPttChainTest)
#include "tst_smart_sdr_api_listener_ptt_chain.moc"
