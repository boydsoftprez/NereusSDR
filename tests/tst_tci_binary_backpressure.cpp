// tests/tst_tci_binary_backpressure.cpp  (NereusSDR)
// no-port-check: NereusSDR-original test. The fix it covers is ported from
// Longpath cf756d8b (Martin Fischer, OE5SOS), which shipped no test; see
// docs/attribution/LONGPATH-PROVENANCE.md for TciServer.cpp.
//
// A TCI app that stops reading (a phone the OS froze in a pocket, a hung
// program) keeps its connection open. Qt buffers every binary frame the
// server sends it, without limit. Receive audio and I/Q to such an app are
// now dropped once 256 KiB or more wait for it.
//
// Both ways a block reaches the app are covered: sent at once (a server with
// no model, so no receive lane), and sent once the receive lane has
// resampled it (a local RadioModel's lane, which an app asking for a rate
// other than 48 kHz uses).
//
// The stalled app is a raw TCP socket that does the WebSocket handshake by
// hand and then stops reading (a small read buffer, never read), so the
// kernel buffers fill and the server's socket holds the rest.
//
// Modification history (NereusSDR):
//   2026-10-22 - Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QTcpSocket>
#include <QVector>

#include <vector>

#include "core/AppSettings.h"
#include "core/TciServer.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// A masked client text frame (RFC 6455 section 5.2); payloads under 126 bytes.
QByteArray maskedTextFrame(const QByteArray& payload)
{
    const char mask[4] = {0x12, 0x34, 0x56, 0x78};
    QByteArray frame;
    frame.append(char(0x81));
    frame.append(char(0x80 | payload.size()));
    frame.append(mask, 4);
    for (int i = 0; i < payload.size(); ++i) {
        frame.append(char(payload.at(i) ^ mask[i % 4]));
    }
    return frame;
}

// Connects, completes the handshake, sends `setup` and subscribes to receive
// audio and I/Q for receiver 0. Afterwards the socket reads nothing more.
bool openStalledClient(QTcpSocket& socket, TciServer& server,
                       const QList<QByteArray>& setup = {})
{
    socket.connectToHost(QStringLiteral("127.0.0.1"), server.port());
    if (!QTest::qWaitFor([&]() {
            return socket.state() == QAbstractSocket::ConnectedState; }, 3000)) {
        return false;
    }
    socket.write(QStringLiteral(
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:%1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n").arg(server.port()).toLatin1());
    QByteArray response;
    if (!QTest::qWaitFor([&]() {
            response += socket.readAll();
            return response.contains("\r\n\r\n"); }, 3000)
        || !response.startsWith("HTTP/1.1 101")) {
        return false;
    }
    // From here on the app reads nothing: Qt holds at most this much, and
    // the kernel's buffers fill behind it.
    socket.setReadBufferSize(4096);
    for (const QByteArray& line : setup) {
        socket.write(maskedTextFrame(line));
    }
    socket.write(maskedTextFrame("audio_start:0;"));
    socket.write(maskedTextFrame("iq_start:0;"));
    return QTest::qWaitFor([&]() {
        return server.activeIqSubscriberCount(0) == 1; }, 3000);
}

// Fills the stalled app's buffers with I/Q, then checks that I/Q and four
// receive audio blocks to it are dropped. `model` is the server's model, or
// nullptr; with a model, waits for its receive lane to go idle at the end.
void checkBoundedIqAndAudio(TciServer& server, QTcpSocket& app, RadioModel* model)
{
    QCOMPARE(server.localBinaryBackpressureDrops(), quint64(0));

    // I/Q: 128 kB frames, letting the event loop run after each, until 50
    // frames in a row are dropped (the kernel's buffers are full and the
    // server holds its 256 KiB), or 96 MB, far past anything the kernel
    // buffers on loopback. Without the bound every frame is queued in Qt
    // and none is dropped.
    QVector<float> iq(32768, 0.25f);
    constexpr qint64 kFrameBytes = 32768 * 4;
    constexpr qint64 kGiveUpBytes = 96LL * 1024 * 1024;
    constexpr int kDroppedInARow = 50;
    qint64 offered = 0;
    int inARow = 0;
    while (inARow < kDroppedInARow && offered < kGiveUpBytes) {
        const quint64 before = server.localBinaryBackpressureDrops();
        server.injectRawIqForTest(0, 192000, iq);
        offered += kFrameBytes;
        inARow = server.localBinaryBackpressureDrops() > before ? inARow + 1 : 0;
        QCoreApplication::processEvents();
    }
    QVERIFY2(server.localBinaryBackpressureDrops() > 0,
             qPrintable(QStringLiteral("%1 MB of I/Q went to an app that reads "
                                       "nothing and none was dropped")
                            .arg(offered / (1024 * 1024))));
    QVERIFY2(inARow >= kDroppedInARow,
             qPrintable(QStringLiteral("I/Q frames still went out after %1 MB")
                            .arg(offered / (1024 * 1024))));

    // Receive audio to the same app is dropped too. Each 2048-frame block
    // the drain timer completes is one frame to the app. No I/Q is offered
    // now, so the count moves only for audio.
    const quint64 audioBaseline = server.localBinaryBackpressureDrops();
    std::vector<float> left(2048, 0.1f);
    std::vector<float> right(2048, -0.1f);
    for (int block = 0; block < 4; ++block) {
        server.injectAudioFrameForTest(0, left.data(), right.data(), 2048, 48000);
    }
    QTRY_COMPARE_WITH_TIMEOUT(server.localBinaryBackpressureDrops(), audioBaseline + 4, 3000);

    app.abort();
    server.stop();
    if (model != nullptr) {
        QVERIFY(model->waitForReceiveLaneForTest(5000));
    }
}

} // namespace

class TestTciBinaryBackpressure : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void stalled_app_gets_bounded_iq_and_audio();
    void stalled_app_gets_bounded_audio_from_the_receive_lane();
};

void TestTciBinaryBackpressure::initTestCase()
{
    AppSettings::instance().setValue(QStringLiteral("TciIqSwap"), QStringLiteral("True"));
    AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"),
                                     QStringLiteral("False"));
}

void TestTciBinaryBackpressure::stalled_app_gets_bounded_iq_and_audio()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QTcpSocket app;
    QVERIFY(openStalledClient(app, server));
    checkBoundedIqAndAudio(server, app, nullptr);
}

// A 12 kHz stream is resampled on a local model's receive lane and sent
// when the lane hands the block back; that send is gated too.
void TestTciBinaryBackpressure::stalled_app_gets_bounded_audio_from_the_receive_lane()
{
    RadioModel model;
    QVERIFY2(model.receiveLane() != nullptr, "a local model has a receive lane");
    TciServer server(&model);
    QVERIFY(server.start(0));
    QTcpSocket app;
    QVERIFY(openStalledClient(app, server, {QByteArrayLiteral("audio_samplerate:12000;")}));
    checkBoundedIqAndAudio(server, app, &model);
}

QTEST_MAIN(TestTciBinaryBackpressure)
#include "tst_tci_binary_backpressure.moc"

#endif // HAVE_WEBSOCKETS
