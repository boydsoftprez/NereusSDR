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
    void sliceOwner_isLocalClientHandleNotPerClientBanner();
    void c1_localClientHandleIsStableAndDistinctFromBanners();
    void c2_pttRequestedIsOneFrameWithCanonicalFields();
    void c2b_localInitTunePicksFirstTgxlAsInitiator();
    void handshakeFlap_lsbInitialStatusUsesNegativeFilterPassband();
    void handshakeFlap_usbInitialStatusUsesPositiveFilterPassband();
    void c3_transmittingIsOneFrameWithCommaSeparatedAmpList();
    void c4_transmittingIsDelayed30msAfterLastAck();
    void c5_unkeyEmitsUnkeyRequestedThenTwoReadyFrames();
    void c5b_wireDrivenTuneOffPreservesAmpTgReason();
    void c6_pttAPushesAreReplacedWithAmplifierStateBroadcasts();
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

// 2026-05-22 bench-fix regression test: the slice 0 S-frame's
// client_handle= body field carries the SLICE OWNER (the synthetic local
// client handle), not the per-recipient banner handle. Canonical FLEX
// uses the same owner handle on every recipient's copy of the frame.
// Bench evidence pre-fix (16:02:39): amps refused interlock ready ACK
// because slice.client_handle (per-client banner) != PTT_REQUESTED
// .tx_client_handle (synthetic). After this fix the two match and amps
// recognize the TX request as coming from the slice owner.
void SmartSdrApiListenerPttChainTest::sliceOwner_isLocalClientHandleNotPerClientBanner()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const QString localHandle = listener.localClientHandle();
    QVERIFY(!localHandle.isEmpty());

    QTcpSocket a, b;
    a.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(a.waitForConnected(1000));
    b.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(b.waitForConnected(1000));
    const QString textA = QString::fromUtf8(drain(&a));
    const QString textB = QString::fromUtf8(drain(&b));

    // Both clients must see slice 0 with client_handle=0x<localHandle>.
    // The slice owner is global, not per-recipient.
    const QString expected =
        QStringLiteral("client_handle=0x") + localHandle;
    QVERIFY2(textA.contains(expected),
             qPrintable(QStringLiteral("client A slice missing %1, body: %2")
                            .arg(expected, textA.left(400))));
    QVERIFY2(textB.contains(expected),
             qPrintable(QStringLiteral("client B slice missing %1, body: %2")
                            .arg(expected, textB.left(400))));
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

// C2 follow-up: when the operator presses TUNE in the local NereusSDR UI
// (NOT a remote TGXL hardware button press), no `transmit tune on` command
// arrives on the wire to populate m_lastTuneInitiator. The PTT_REQUESTED
// frame must still carry reason=AMP:<TGXL-name> via the
// initiatingAmpName(TUNE) first-TunerGeniusXL fallback, matching canonical
// pcap T+167.678. Bench-confirmed 2026-05-21 19:38:51: without the
// fallback the wire emits reason= empty.
void SmartSdrApiListenerPttChainTest::c2b_localInitTunePicksFirstTgxlAsInitiator()
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

    // NO `transmit tune on` over the wire. RadioModel invokes the listener
    // directly (the local-UI TUNE path).
    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));

    const QByteArray bytes = drain(&tgxl);
    const QStringList frames =
        findS0InterlockFrames(bytes, QStringLiteral("PTT_REQUESTED"));
    QCOMPARE(frames.size(), 1);
    const QString frame = frames.first();
    QVERIFY2(frame.contains(QStringLiteral("reason=AMP:TG ")),
             qPrintable(QStringLiteral("expected reason=AMP:TG fallback, got: ") + frame));
}

// 2026-05-21 handshake-flap follow-up: canonical FLEX uses NEGATIVE
// filter passband for LSB (verified by tshark against
// captures/flex-tgxl-direct-CONTROL.pcapng @ T+206.741:
//   mode=LSB wide=0 filter_lo=-2800 filter_hi=-100
// ). Our prior hardcoded `filter_lo=100 filter_hi=2800` failed TGXL's
// mode-vs-filter validation and caused the SmartSDR API socket to flap
// within ~2 sec of connect. Regression test for the sign correction.
void SmartSdrApiListenerPttChainTest::handshakeFlap_lsbInitialStatusUsesNegativeFilterPassband()
{
    SmartSdrApiListener listener;
    listener.setSliceMode(0, QStringLiteral("LSB"));
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket fakeAmp;
    fakeAmp.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(fakeAmp.waitForConnected(1000));
    const QByteArray bytes = drain(&fakeAmp);
    const QString text = QString::fromUtf8(bytes);
    QVERIFY2(text.contains(QStringLiteral("mode=LSB ")),
             qPrintable(QStringLiteral("expected mode=LSB in initial status, got: ")
                            + text.left(400)));
    QVERIFY2(text.contains(QStringLiteral("filter_lo=-2800 filter_hi=-100 ")),
             qPrintable(QStringLiteral("expected negative LSB filter passband, got: ")
                            + text.left(400)));
}

// USB stays positive (regression guard that the helper does not
// invert the sign for upper-sideband modes).
void SmartSdrApiListenerPttChainTest::handshakeFlap_usbInitialStatusUsesPositiveFilterPassband()
{
    SmartSdrApiListener listener;
    listener.setSliceMode(0, QStringLiteral("USB"));
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket fakeAmp;
    fakeAmp.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(fakeAmp.waitForConnected(1000));
    const QByteArray bytes = drain(&fakeAmp);
    const QString text = QString::fromUtf8(bytes);
    QVERIFY2(text.contains(QStringLiteral("mode=USB ")),
             qPrintable(QStringLiteral("expected mode=USB in initial status, got: ")
                            + text.left(400)));
    QVERIFY2(text.contains(QStringLiteral("filter_lo=100 filter_hi=2800 ")),
             qPrintable(QStringLiteral("expected positive USB filter passband, got: ")
                            + text.left(400)));
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

// Task 4 (C4): TRANSMITTING is emitted no sooner than ~30 ms after the
// last interlock ready ACK arrives. Matches pcap T+167.704 to T+167.734.
void SmartSdrApiListenerPttChainTest::c4_transmittingIsDelayed30msAfterLastAck()
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
    drain(&tgxl); drain(&pgxl);

    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);

    // Both amps ACK back to back.
    QElapsedTimer timer;
    timer.start();
    tgxl.write("C10|interlock ready 1\n");
    tgxl.flush();
    pgxl.write("C10|interlock ready 2\n");
    pgxl.flush();

    // Wait for interlockGranted; record elapsed time from the second ACK
    // write to the signal fire. Should be >= 25 ms (allow 5 ms jitter
    // below the nominal 30 ms target).
    QVERIFY(grantSpy.wait(500));
    const qint64 elapsedMs = timer.elapsed();
    QVERIFY2(elapsedMs >= 25,
             qPrintable(QStringLiteral("expected >= 25 ms settle, got %1 ms")
                            .arg(elapsedMs)));
    // Sanity upper bound (the 500 ms ACK timeout would be a regression).
    QVERIFY2(elapsedMs < 200,
             qPrintable(QStringLiteral("settle too long: %1 ms").arg(elapsedMs)));
}

// Task 5 (C5): unkey emits UNKEY_REQUESTED, then READY (empty reason),
// then READY (reason=AMP:<initiator>). Matches pcap T+168.874 to T+168.877.
void SmartSdrApiListenerPttChainTest::c5_unkeyEmitsUnkeyRequestedThenTwoReadyFrames()
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
    drain(&tgxl); drain(&pgxl);
    tgxl.write("C10|interlock ready 1\n"); tgxl.flush();
    pgxl.write("C10|interlock ready 2\n"); pgxl.flush();
    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);
    QVERIFY(grantSpy.wait(500));
    drain(&tgxl); drain(&pgxl);  // TRANSMITTING + pttA=1 chatter

    // Trigger un-key.
    listener.setInterlockTransmitting(false, QStringLiteral("TUNE"));

    // Capture frames across ~50 ms so the 2 ms + 0.5 ms gaps land.
    const QByteArray bytes = drain(&tgxl, 100);
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));

    // Filter to the S0|interlock lines in arrival order.
    QStringList interlockLines;
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("S0|interlock"))) {
            interlockLines << line;
        }
    }
    QVERIFY2(interlockLines.size() >= 3,
             qPrintable(QStringLiteral("expected at least 3 interlock lines, got: ")
                            + interlockLines.join(QStringLiteral(" || "))));
    QVERIFY2(interlockLines[0].contains(QStringLiteral("state=UNKEY_REQUESTED")),
             qPrintable(interlockLines[0]));
    QVERIFY2(interlockLines[0].contains(QStringLiteral("reason=AMP:TG")),
             qPrintable(interlockLines[0]));
    QVERIFY2(interlockLines[1].contains(QStringLiteral("state=READY")),
             qPrintable(interlockLines[1]));
    QVERIFY2(interlockLines[1].contains(QStringLiteral(" reason= ")),
             qPrintable(QStringLiteral("expected empty reason in first READY: ") + interlockLines[1]));
    QVERIFY2(interlockLines[2].contains(QStringLiteral("state=READY")),
             qPrintable(interlockLines[2]));
    QVERIFY2(interlockLines[2].contains(QStringLiteral("reason=AMP:TG")),
             qPrintable(interlockLines[2]));
}

// Task 6 (C6): on key-down, no S<h>|amplifier 0x<h> pttA=1; instead
// S0|amplifier 0x<h> state=TRANSMIT_A for each keyed amp. On un-key,
// no pttA=0; instead state=IDLE. Matches pcap T+167.740 (state=TRANSMIT_A
// ~5 ms after TRANSMITTING) and T+168.881 (state=IDLE ~5 ms after the
// second READY).
void SmartSdrApiListenerPttChainTest::c6_pttAPushesAreReplacedWithAmplifierStateBroadcasts()
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

    // Key-down.
    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);
    tgxl.write("C10|interlock ready 1\n"); tgxl.flush();
    pgxl.write("C10|interlock ready 2\n"); pgxl.flush();
    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);
    QVERIFY(grantSpy.wait(500));

    // Capture key-down chatter for ~50 ms past the grant.
    const QByteArray keyDownBytes = drain(&tgxl, 50);
    const QString keyDownText = QString::fromUtf8(keyDownBytes);
    QVERIFY2(!keyDownText.contains(QStringLiteral("pttA=1")),
             qPrintable(QStringLiteral("expected no pttA=1 push, got: ") + keyDownText));
    QVERIFY2(keyDownText.contains(QStringLiteral("state=TRANSMIT_A")),
             qPrintable(QStringLiteral("expected state=TRANSMIT_A broadcast, got: ") + keyDownText));

    // Un-key.
    drain(&pgxl);  // clear pgxl buffer
    listener.setInterlockTransmitting(false, QStringLiteral("TUNE"));
    const QByteArray unKeyBytes = drain(&tgxl, 100);
    const QString unKeyText = QString::fromUtf8(unKeyBytes);
    QVERIFY2(!unKeyText.contains(QStringLiteral("pttA=0")),
             qPrintable(QStringLiteral("expected no pttA=0 push, got: ") + unKeyText));
    QVERIFY2(unKeyText.contains(QStringLiteral("state=IDLE")),
             qPrintable(QStringLiteral("expected state=IDLE broadcast, got: ") + unKeyText));
}

// Task 5 (C5) follow-up: when the un-key path is triggered by a
// `transmit tune off` line coming over the wire (the production path),
// the UNKEY_REQUESTED frame must still carry reason=AMP:<initiator-name>
// pulled from m_lastTuneInitiator. Regression test for the
// clear-before-emit ordering bug fixed in this commit: previously the
// initiator was cleared BEFORE tuneRequested(false) emitted, so the
// synchronous chain into setInterlockTransmitting(false, "TUNE") read
// an empty initiator and fell back to AMP:PG-XL.
void SmartSdrApiListenerPttChainTest::c5b_wireDrivenTuneOffPreservesAmpTgReason()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    // Wire the listener's tuneRequested signal back to its own
    // setInterlockTransmitting, mirroring the RadioModel hookup so the
    // test runs the full production cycle.
    QObject::connect(&listener, &SmartSdrApiListener::tuneRequested,
                     &listener,
                     [&listener](bool on) {
                         listener.setInterlockTransmitting(
                             on, QStringLiteral("TUNE"));
                     });

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    // TGXL hardware TUNE press: tune on -> ACKs -> tune off, all over wire.
    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl); drain(&pgxl);  // PTT_REQUESTED chatter

    tgxl.write("C10|interlock ready 1\n"); tgxl.flush();
    pgxl.write("C10|interlock ready 2\n"); pgxl.flush();
    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);
    QVERIFY(grantSpy.wait(500));
    drain(&tgxl); drain(&pgxl);  // TRANSMITTING + state=TRANSMIT_A

    // Wire-driven un-key.
    tgxl.write("C11|transmit tune off\n");
    tgxl.flush();

    // Capture frames for ~50 ms so the UNKEY_REQUESTED + 2 READYs land.
    const QByteArray bytes = drain(&tgxl, 100);
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));

    QString unkeyLine;
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("S0|interlock"))
            && line.contains(QStringLiteral("state=UNKEY_REQUESTED"))) {
            unkeyLine = line;
            break;
        }
    }
    QVERIFY2(!unkeyLine.isEmpty(),
             qPrintable(QStringLiteral("no UNKEY_REQUESTED frame in: ")
                            + lines.join(QStringLiteral(" || "))));
    QVERIFY2(unkeyLine.contains(QStringLiteral("reason=AMP:TG")),
             qPrintable(QStringLiteral("expected reason=AMP:TG (TGXL initiated TUNE), got: ")
                            + unkeyLine));
}

QTEST_MAIN(SmartSdrApiListenerPttChainTest)
#include "tst_smart_sdr_api_listener_ptt_chain.moc"
