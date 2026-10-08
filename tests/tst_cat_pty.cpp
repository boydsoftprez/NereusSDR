// no-port-check: NereusSDR-original native PTY CAT integration tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
#include "CatFixtureHarness.h"
#include "CatSerialTestDevice.h"
#include <QTcpServer>
#include <QTcpSocket>
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <termios.h>
class PtyPeer {
public:
    ~PtyPeer() { close(); }
    bool open(const QString& path) { close(); m_fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC); return m_fd >= 0; }
    void close() { if (m_fd >= 0) { const int descriptor = std::exchange(m_fd, -1); const int result = ::close(descriptor); Q_ASSERT(result == 0); Q_UNUSED(result); } }
    bool send(const QByteArray& bytes) { return ::write(m_fd, bytes.constData(), bytes.size()) == bytes.size(); }
    QByteArray take() { QByteArray bytes; char buffer[4096]; for (;;) { const ssize_t count = ::read(m_fd, buffer, sizeof(buffer)); if (count <= 0) { break; } bytes.append(buffer, count); } return bytes; }
    int descriptor() const { return m_fd; }
private: int m_fd{-1};
};
#endif
using namespace NereusSDR;
class TstCatPty : public QObject {
    Q_OBJECT
    void prepare(RadioModel& model) {
        model.addSlice(); model.addSlice();
        for (SliceModel* slice : model.slices()) { model.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice()); }
        model.sliceById(0)->setFrequency(14074000); model.sliceById(1)->setFrequency(7074000);
    }
    CatEndpointConfig endpoint(int slice = 0) { CatEndpointConfig config; config.ptyEnabled = true; config.binding.primarySliceId = slice; return config; }
private slots:
    void init() {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); }
        }
    }
    void endpointExistsWithoutPeer() {
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig config; config.ptyEnabled = true;
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QVERIFY(service.isListening(1));
        QVERIFY(service.sessionIds(1).isEmpty());
        QTimer observation; observation.setSingleShot(true); QSignalSpy elapsed(&observation, &QTimer::timeout);
        observation.start(60); QTRY_COMPARE(elapsed.size(), 1);
        QVERIFY(service.sessionIds(1).isEmpty()); QVERIFY(!service.ptyTransport(1)->hasPeer());
        QVERIFY(!service.ptySlavePath(1).isEmpty());
        QCOMPARE(service.clientCount(1), 0);
        QSignalSpy paths(&service, &CatService::ptyPathChanged);
        service.stopAll(); QVERIFY(service.ptySlavePath(1).isEmpty()); QCOMPARE(paths.last().at(1).toString(), QString());
#else
        QVERIFY(!service.isListening(1));
#endif
    }
    void deliveredDialectKeepsSibling() {
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig config = endpoint(); config.ptyDialect = "Rigctld";
        QTcpServer probe; QVERIFY(probe.listen(QHostAddress::LocalHost, 0)); config.tcpEnabled = true; config.tcpPort = probe.serverPort(); probe.close();
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QVERIFY(!service.ptySlavePath(1).isEmpty()); QCOMPARE(service.channelState(1),QString("Listening"));
#else
        QVERIFY(service.ptySlavePath(1).isEmpty());
#endif
        QVERIFY(service.isListening(1)); QCOMPARE(service.transportState(1, CatTransportKind::Tcp), QString("Listening"));
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QCOMPARE(service.transportState(1, CatTransportKind::Pty),QString("Listening"));
#else
        QVERIFY(service.transportState(1, CatTransportKind::Pty).contains("unavailable"));
#endif
    }
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    void realBytesBindingsAndRawTerminal() {
        RadioModel model; prepare(model); CatService& service = *model.catService();
        QVERIFY(service.applyChannelConfig(1, endpoint())); QVERIFY(service.applyChannelConfig(2, endpoint(1))); service.startConfigured();
        PtyPeer a; PtyPeer b; QVERIFY(a.open(service.ptySlavePath(1))); QVERIFY(b.open(service.ptySlavePath(2)));
        QTRY_COMPARE(service.sessionIds(1).size(), 1); QTRY_COMPARE(service.sessionIds(2).size(), 1);
        termios format{}; QCOMPARE(::tcgetattr(a.descriptor(), &format), 0); QVERIFY(!(format.c_lflag & (ICANON | ECHO | ISIG))); QVERIFY(!(format.c_oflag & OPOST));
        QByteArray first; QByteArray second;
        QVERIFY(a.send("F")); QVERIFY(b.send("FA;")); QTRY_VERIFY((second += b.take()) == "FA00007074000;");
        QVERIFY(first.isEmpty()); QVERIFY(a.send("A00014200000;FA;MD;"));
        QTRY_VERIFY((first += a.take()) == "FA00014200000;MD2;");
        QCOMPARE(model.sliceById(0)->frequency(), 14200000.0); QCOMPARE(model.sliceById(1)->frequency(), 7074000.0);
        QCOMPARE(service.clientCount(1), 0); QCOMPARE(service.clientCount(2), 0);
        service.stopAll(); QVERIFY(service.ptySlavePath(1).isEmpty()); QVERIFY(service.ptySlavePath(2).isEmpty());
        QVERIFY(!service.isListening(1));
    }
    void immediateFirstCommandBeforePeerObservation() {
        RadioModel model; prepare(model); CatService& service = *model.catService();
        QVERIFY(service.applyChannelConfig(1, endpoint())); service.startConfigured();
        PtyPeer peer; const QString path = service.ptySlavePath(1);
        QVERIFY(service.sessionIds(1).isEmpty()); QVERIFY(peer.open(path)); QVERIFY(peer.send("FA;"));
        // A real client may send in the same turn as open, before the 20ms observation/raw setup.
        QVERIFY(service.sessionIds(1).isEmpty()); QCOMPARE(peer.take(), QByteArray()); QByteArray reply;
        QTRY_VERIFY((reply += peer.take()) == "FA00014074000;");
        QTRY_COMPARE(service.sessionIds(1).size(), 1); const quint64 first = service.sessionIds(1).first();
        peer.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty());
        QVERIFY(peer.open(path)); QVERIFY(peer.send("FA;ID;")); QVERIFY(service.sessionIds(1).isEmpty());
        QCOMPARE(peer.take(), QByteArray()); reply.clear(); QTRY_VERIFY((reply += peer.take()) == "FA00014074000;ID019;");
        QVERIFY(service.sessionIds(1).first() > first);
    }
    void noPeerAiAndReopenHaveNoStaleBytes() {
        RadioModel model; prepare(model); CatService& service = *model.catService();
        QVERIFY(service.applyChannelConfig(1, endpoint()));
        CatGlobalConfig global = service.globalConfig(); global.allowKenwoodAi = true; global.aiEnabled = true; QVERIFY(service.applyGlobalConfig(global));
        service.startConfigured(); const QString path = service.ptySlavePath(1);
        model.sliceById(0)->setFrequency(14075000); QCoreApplication::processEvents(); service.reporter().flushPending();
        QCOMPARE(service.ptySlavePath(1), path); QVERIFY(service.sessionIds(1).isEmpty()); QVERIFY(service.isListening(1));
        PtyPeer peer; QVERIFY(peer.open(path)); QTRY_COMPARE(service.sessionIds(1).size(), 1);
        const quint64 old = service.sessionIds(1).first(); QCOMPARE(peer.take(), QByteArray());
        QByteArray result; QVERIFY(peer.send("FA;")); QTRY_VERIFY((result += peer.take()) == "FA00014075000;");
        result.clear(); model.sliceById(0)->setFrequency(14076000); QTRY_VERIFY((result += peer.take()) == "FA00014076000;");
        QVERIFY(peer.send("FA000")); QSignalSpy logged(&service, &CatService::messageLogged);
        QTRY_VERIFY(service.session(old)->framer().bufferedBytes() > 0);
        peer.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty()); QVERIFY(service.isListening(1)); QCOMPARE(service.ptySlavePath(1), path);
        model.sliceById(0)->setFrequency(14077000); QCoreApplication::processEvents(); service.reporter().flushPending();
        QVERIFY(peer.open(path)); QTRY_COMPARE(service.sessionIds(1).size(), 1); QVERIFY(service.sessionIds(1).first() > old);
        QVERIFY(peer.take().isEmpty()); QVERIFY(peer.send("FA;")); result.clear(); QTRY_VERIFY((result += peer.take()) == "FA00014077000;");
        service.stopAll(); QVERIFY(!service.ptyTransport(1));
    }
    void mixedTransportsAndIndependentFailures() {
        RadioModel model; prepare(model); CatService& service = *model.catService();
        const auto device = std::make_shared<CatSerialTestDevice>();
        QVERIFY(service.setSerialTransportFactoryForTest([device] { return std::make_shared<CatSerialTransport>(device); }));
        CatEndpointConfig config = endpoint(); config.serialEnabled = true; config.serialDevice = "test-owned";
        QTcpServer probe; QVERIFY(probe.listen(QHostAddress::LocalHost, 0)); config.tcpEnabled = true; config.tcpPort = probe.serverPort(); probe.close();
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
        QVERIFY(device->opened); QVERIFY(!service.ptySlavePath(1).isEmpty()); QVERIFY(service.boundPort(1));
        PtyPeer peer; QVERIFY(peer.open(service.ptySlavePath(1))); QTcpSocket tcp; tcp.connectToHost(QHostAddress::LocalHost, service.boundPort(1));
        QTRY_COMPARE(service.sessionIds(1).size(), 3); QTRY_COMPARE(service.clientCount(1), 1);
        device->disappear(); QVERIFY(service.channelState(1).contains("Serial error")); QVERIFY(service.isListening(1));
        QVERIFY(!service.ptySlavePath(1).isEmpty()); QCOMPARE(service.clientCount(1), 1);
        service.stopAll(); device->refuseOpen = true; service.startConfigured();
        QVERIFY(service.channelState(1).contains("Serial error")); QVERIFY(!service.ptySlavePath(1).isEmpty()); QVERIFY(service.boundPort(1));
        service.stopAll(); device->refuseOpen = false;
        QVERIFY(probe.listen(QHostAddress::LocalHost, config.tcpPort)); service.startConfigured();
        QVERIFY(service.channelState(1).contains("TCP error")); QVERIFY(device->opened); QVERIFY(!service.ptySlavePath(1).isEmpty());
    }
    void partialOutputBoundAndCloseDropsKernelTail() {
        CatPtyTransport transport; CatEndpointConfig config = endpoint(); QVERIFY(transport.start(1, config));
        PtyPeer peer; QVERIFY(peer.open(transport.slavePath())); QSignalSpy opened(&transport, &CatPtyTransport::peerOpened);
        QTRY_COMPARE(opened.size(), 1); QVERIFY(transport.attachSession(1));
        const QByteArray block(64 * 1024, 'x'); QVERIFY(transport.writeBytes(1, block)); QVERIFY(transport.queuedBytes() > 0);
        QByteArray result; QTRY_VERIFY((result += peer.take()).size() == block.size()); QCOMPARE(result, block); QCOMPARE(transport.queuedBytes(), qint64(0));
        QVERIFY(transport.writeBytes(1, block)); peer.close(); QSignalSpy closed(&transport, &CatPtyTransport::peerClosed); QTRY_COMPARE(closed.size(), 1);
        QCOMPARE(transport.queuedBytes(), qint64(0)); QVERIFY(transport.isOpen()); QVERIFY(peer.open(transport.slavePath())); QTRY_COMPARE(opened.size(), 2);
        QVERIFY(transport.attachSession(2)); QVERIFY(peer.take().isEmpty());
        QSignalSpy failed(&transport, &CatPtyTransport::failed);
        for (int count = 0; count < 6 && transport.isOpen(); ++count) { transport.writeBytes(2, block); }
        QCOMPARE(failed.size(), 1); QVERIFY(!transport.isOpen()); QVERIFY(transport.slavePath().isEmpty());
    }
    void lastPeerCancelsPttBeforeNotificationAndPreservesNewOwner() {
        RxCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); prepare(model);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.moxController()->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true, {}}; });
        CatService& service = *model.catService(); QVERIFY(service.applyChannelConfig(1, endpoint())); service.startConfigured();
        PtyPeer peer; QVERIFY(peer.open(service.ptySlavePath(1))); QTRY_COMPARE(service.sessionIds(1).size(), 1);
        const quint64 old = service.sessionIds(1).first(); QVERIFY(peer.send("TX;")); QTRY_VERIFY(model.moxController()->isMox());
        connect(&service, &CatService::sessionClosed, &model, [&](quint64 id) { if (id == old) { QVERIFY(!model.moxController()->isMox()); QVERIFY(!service.session(id)); } });
        peer.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty()); QVERIFY(!model.moxController()->isMox());
        QVERIFY(peer.open(service.ptySlavePath(1))); QTRY_COMPARE(service.sessionIds(1).size(), 1); QVERIFY(!model.moxController()->isMox());
        QVERIFY(peer.send("TX;")); QTRY_VERIFY(model.moxController()->isMox()); model.moxController()->setMox(true);
        peer.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty()); QVERIFY(model.moxController()->isMox());
        model.moxController()->setMox(false); model.injectConnectionForTest(nullptr);
    }
    void departurePreservesAnotherChannelClaim() {
        RxCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); prepare(model);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.moxController()->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true, {}}; });
        CatService& service = *model.catService(); QVERIFY(service.applyChannelConfig(1, endpoint())); QVERIFY(service.applyChannelConfig(2, endpoint())); service.startConfigured();
        PtyPeer first; PtyPeer second; QVERIFY(first.open(service.ptySlavePath(1))); QVERIFY(second.open(service.ptySlavePath(2)));
        QTRY_COMPARE(service.sessionIds(1).size(), 1); QTRY_COMPARE(service.sessionIds(2).size(), 1);
        QVERIFY(first.send("TX;")); QTRY_VERIFY(model.moxController()->isMox());
        const quint64 other = service.sessionIds(2).first(); QSignalSpy logged(&service, &CatService::messageLogged); QVERIFY(second.send("TX;"));
        QTRY_VERIFY(logged.size() > 0);
        first.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty());
        QVERIFY(model.moxController()->isMox()); QVERIFY(service.session(other));
        second.close(); QTRY_VERIFY(service.sessionIds(2).isEmpty()); QVERIFY(!model.moxController()->isMox()); model.injectConnectionForTest(nullptr);
    }
    void multipleSlaveHandlesAreOnePeerStream() {
        RadioModel model; prepare(model); CatService& service = *model.catService(); QVERIFY(service.applyChannelConfig(1, endpoint())); service.startConfigured();
        PtyPeer first; PtyPeer second; const QString path = service.ptySlavePath(1); QVERIFY(first.open(path)); QVERIFY(second.open(path));
        QTRY_COMPARE(service.sessionIds(1).size(), 1); const quint64 id = service.sessionIds(1).first(); first.close();
        QByteArray reply; QVERIFY(second.send("FA;")); QTRY_VERIFY((reply += second.take()) == "FA00014074000;");
        QCOMPARE(service.sessionIds(1).first(), id); second.close(); QTRY_VERIFY(service.sessionIds(1).isEmpty());
    }
    void directSessionClosePublishesStoppedAndKeepsOtherChannel() {
        RadioModel model; prepare(model); CatService& service = *model.catService();
        QVERIFY(service.applyChannelConfig(1, endpoint())); QVERIFY(service.applyChannelConfig(2, endpoint(1))); service.startConfigured();
        PtyPeer peer; QVERIFY(peer.open(service.ptySlavePath(1))); QTRY_COMPARE(service.sessionIds(1).size(), 1);
        const QString other = service.ptySlavePath(2); service.closeSession(service.sessionIds(1).first());
        QVERIFY(!service.isListening(1)); QVERIFY(service.ptySlavePath(1).isEmpty());
        QCOMPARE(service.transportState(1, CatTransportKind::Pty), QString("Stopped"));
        QCOMPARE(service.channelState(1), QString("Stopped")); QCOMPARE(service.ptySlavePath(2), other); QVERIFY(service.isListening(2));
    }
    void callbackRestartDeleteAndRetirement() {
        auto model = std::make_unique<RadioModel>(); prepare(*model); CatService* service = model->catService(); QVERIFY(service->applyChannelConfig(1, endpoint())); service->startConfigured();
        PtyPeer peer; QVERIFY(peer.open(service->ptySlavePath(1))); QTRY_COMPARE(service->sessionIds(1).size(), 1);
        const quint64 old = service->sessionIds(1).first(); bool restarted = false;
        connect(service, &CatService::sessionClosed, service, [&](quint64 id) { if (id == old && !restarted) { restarted = true; service->stopAll(); service->startConfigured(); } });
        peer.close(); QTRY_VERIFY(restarted); QVERIFY(service->isListening(1)); QVERIFY(!service->ptySlavePath(1).isEmpty());
        QVERIFY(peer.open(service->ptySlavePath(1))); QTRY_COMPARE(service->sessionIds(1).size(), 1);
        connect(service, &CatService::messageLogged, service, [&](int, bool inbound, const QByteArray&) { if (inbound) { model.reset(); } });
        QVERIFY(peer.send("FA;")); QTRY_VERIFY(!model); peer.close();
        RadioModel retired; CatService& final = *retired.catService(); QVERIFY(final.applyChannelConfig(1, endpoint())); final.startConfigured();
        connect(&final, &CatService::ptyPathChanged, &retired, [&](int, const QString& path) { if (path.isEmpty()) { final.startConfigured(); } });
        final.beginRetirement(); QVERIFY(!final.isStarted()); QVERIFY(final.ptySlavePath(1).isEmpty());
    }
#endif
};
QTEST_GUILESS_MAIN(TstCatPty)
#include "tst_cat_pty.moc"
