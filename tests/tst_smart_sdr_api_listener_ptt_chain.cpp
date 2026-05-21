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

// Send a canned `interlock create` from `sock`. Returns when the listener
// has assigned an interlock id (i.e. it->interlockId != 0 inside the
// listener). The fake amp ALSO needs an `amplifier create` first so the
// listener tracks ampHandle / ampModel / interlockName correctly.
void registerFakeAmp(QTcpSocket* sock,
                     const QString& model,
                     const QString& interlockName,
                     const QString& serial = QStringLiteral("TEST-1"))
{
    QByteArray cmd;
    cmd.append(QStringLiteral("C1|amplifier create ip=127.0.0.1 port=9999"
                              " model=%1 serial_num=%2 ant=ANT1\n")
                   .arg(model).arg(serial)
                   .toUtf8());
    cmd.append(QStringLiteral("C2|interlock create type=AMP name=%1"
                              " serial=%2 valid_antennas=ANT1\n")
                   .arg(interlockName).arg(serial)
                   .toUtf8());
    sock->write(cmd);
    sock->flush();
    // Pump the event loop so the listener processes the lines.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 200) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
}

// Find all `S0|interlock state=<state>` frames in a captured byte stream.
// Returns the body portion after `|` so test assertions can substring-match.
QStringList findS0InterlockFrames(const QByteArray& bytes,
                                  const QString& state)
{
    QStringList out;
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("S0|interlock "))
            && line.contains(QStringLiteral("state=") + state)) {
            out << line;
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
    void c1_localClientHandleIsStableAndDistinctFromBanners();
    void c2_pttRequestedIsOneFrameWithCanonicalFields();
    void c3_transmittingIsOneFrameWithCommaSeparatedAmpList();
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

// Task 1 (C1): After start(), the listener owns a synthetic
// local-client handle that (a) is 8-hex, (b) is the same value for the
// lifetime of the listener, and (c) is distinct from any client's banner
// handle assigned at accept time.
void SmartSdrApiListenerPttChainTest::c1_localClientHandleIsStableAndDistinctFromBanners()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));

    const QString localHandle = listener.localClientHandle();
    QCOMPARE(localHandle.size(), 8);
    // Hex digits only.
    for (QChar c : localHandle) {
        QVERIFY2(c.isDigit() || (c.toLatin1() >= 'A' && c.toLatin1() <= 'F'),
                 qPrintable(QStringLiteral("non-hex character in handle: ") + localHandle));
    }
    // Stable across reads.
    QCOMPARE(listener.localClientHandle(), localHandle);

    // Connect two clients and confirm the banner-assigned handles differ
    // from the local-client handle.
    QTcpSocket a, b;
    a.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(a.waitForConnected(1000));
    b.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(b.waitForConnected(1000));

    auto bannerOf = [](const QByteArray& bytes) -> QString {
        const QString text = QString::fromUtf8(bytes);
        const int hIdx = text.indexOf(QStringLiteral("\nH"));
        if (hIdx < 0) { return QString(); }
        const int nlIdx = text.indexOf(QLatin1Char('\n'), hIdx + 2);
        return text.mid(hIdx + 2, (nlIdx - hIdx - 2));
    };

    const QString bannerA = bannerOf(drain(&a));
    const QString bannerB = bannerOf(drain(&b));
    QVERIFY(!bannerA.isEmpty());
    QVERIFY(!bannerB.isEmpty());
    QVERIFY2(bannerA != localHandle,
             qPrintable(QStringLiteral("client A banner ") + bannerA
                        + QStringLiteral(" collides with local handle ")
                        + localHandle));
    QVERIFY2(bannerB != localHandle,
             qPrintable(QStringLiteral("client B banner ") + bannerB
                        + QStringLiteral(" collides with local handle ")
                        + localHandle));
}

// Task 2 (C2): PTT_REQUESTED is exactly one frame broadcast to all
// subscribers, with canonical fields. Verified against pcap T+167.678
// from flex-tgxl-direct-CONTROL.pcapng.
void SmartSdrApiListenerPttChainTest::c2_pttRequestedIsOneFrameWithCanonicalFields()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();
    const QString localHandle = listener.localClientHandle();

    // Two amps: TGXL initiates TUNE, PGXL participates.
    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);  // banner + initial status

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);  // R-frames for the create commands

    // Simulate TGXL pressing its hardware TUNE button: it would send
    // `C<n>|transmit tune on`. The listener records TG as the initiator.
    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    // Pump the event loop so the listener processes the tune-on line. We
    // do not strictly need to see the R-frame here; what matters is that
    // dispatchLine has run before setInterlockTransmitting is called.
    drain(&tgxl); drain(&pgxl);

    // RadioModel side: setInterlockTransmitting(true, "TUNE").
    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));

    // Both subscribers should see the same one PTT_REQUESTED frame.
    const QByteArray tgxlBytes = drain(&tgxl);
    const QByteArray pgxlBytes = drain(&pgxl);
    const QStringList tgxlFrames =
        findS0InterlockFrames(tgxlBytes, QStringLiteral("PTT_REQUESTED"));
    const QStringList pgxlFrames =
        findS0InterlockFrames(pgxlBytes, QStringLiteral("PTT_REQUESTED"));
    QCOMPARE(tgxlFrames.size(), 1);
    QCOMPARE(pgxlFrames.size(), 1);

    const QString frame = tgxlFrames.first();
    QVERIFY2(frame.contains(QStringLiteral("tx_client_handle=0x") + localHandle),
             qPrintable(QStringLiteral("expected synthetic tx_client_handle in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("reason=AMP:TG ")),
             qPrintable(QStringLiteral("expected reason=AMP:TG in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("source=TUNE")),
             qPrintable(QStringLiteral("expected source=TUNE in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("tx_allowed=1")),
             qPrintable(QStringLiteral("expected tx_allowed=1 in: ") + frame));
    QVERIFY2(frame.endsWith(QStringLiteral("amplifier=")),
             qPrintable(QStringLiteral("expected empty amplifier= in: ") + frame));
}

// Task 3 (C3): TRANSMITTING is exactly one frame with
// amplifier=0x<h1>,0x<h2>,... (comma-separated list of every keyed amp).
// Matches pcap T+167.734.
void SmartSdrApiListenerPttChainTest::c3_transmittingIsOneFrameWithCommaSeparatedAmpList()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl);

    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);  // PTT_REQUESTED frames

    // Both amps ACK. The interlock ids are 1 and 2 by registration order.
    tgxl.write("C10|interlock ready 1\n");
    tgxl.flush();
    pgxl.write("C10|interlock ready 2\n");
    pgxl.flush();

    const QByteArray tgxlBytes = drain(&tgxl, 300);
    const QStringList frames =
        findS0InterlockFrames(tgxlBytes, QStringLiteral("TRANSMITTING"));
    QCOMPARE(frames.size(), 1);

    const QString frame = frames.first();
    QVERIFY2(frame.contains(QStringLiteral("source=TUNE")),
             qPrintable(frame));
    QVERIFY2(frame.contains(QStringLiteral("amplifier=0x")),
             qPrintable(frame));
    QVERIFY2(frame.count(QLatin1Char(',')) == 1,
             qPrintable(QStringLiteral("expected exactly one comma in amplifier=, got: ") + frame));
}

QTEST_MAIN(SmartSdrApiListenerPttChainTest)
#include "tst_smart_sdr_api_listener_ptt_chain.moc"
