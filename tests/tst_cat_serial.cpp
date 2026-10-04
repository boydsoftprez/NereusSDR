// no-port-check: NereusSDR-original native serial CAT integration tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
#include "CatSerialTestDevice.h"
#include "models/SliceModel.h"
#include "core/SliceOwnership.h"
#include <QTcpServer>
#include <QTcpSocket>
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#endif
using namespace NereusSDR;
class TstCatSerial : public QObject {
    Q_OBJECT
private slots:
    void init() {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); }
        }
    }
    void acceptingDeviceQueueIsBounded() {
        const auto device = std::make_shared<CatSerialTestDevice>(); device->holdOutput = true; device->maximumWrite = 64 * 1024;
        CatSerialTransport transport(device); CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = "test-owned";
        QVERIFY(transport.start(config)); QSignalSpy failed(&transport, &CatSerialTransport::failed);
        for (int i = 0; i < 5; ++i) { transport.writeBytes(QByteArray(64 * 1024, 'x')); }
        QCOMPARE(failed.size(), 1); QVERIFY(!transport.isOpen());
        QCOMPARE(device->output.size(), 256 * 1024);
    }
    void partialWritesErrorsAndCallbacks() {
        const auto device = std::make_shared<CatSerialTestDevice>(); device->maximumWrite = 2;
        auto transport = std::make_unique<CatSerialTransport>(device);
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = "test-owned";
        QVERIFY(transport->start(config)); QCOMPARE(device->pinReads, 0);
        transport->writeBytes("abc"); transport->writeBytes("def");
        QCOMPARE(device->output, QByteArray("abcd")); QCOMPARE(transport->queuedBytes(), qint64(2));
        device->completeWrite(); QCOMPARE(device->output, QByteArray("abcdef")); QCOMPARE(transport->queuedBytes(), qint64(0));
        device->maximumWrite = 0; transport->writeBytes("tail"); QCOMPARE(transport->queuedBytes(), qint64(4));
        transport->stop(); device->completeWrite(); QCOMPARE(device->output, QByteArray("abcdef"));
        QSignalSpy failed(transport.get(), &CatSerialTransport::failed);
        device->disappear(); QCOMPARE(failed.size(), 0);
        QVERIFY(transport->start(config)); device->maximumWrite = -1;
        transport->writeBytes("failed"); QCOMPARE(failed.size(), 1); QVERIFY(!transport->isOpen());
        device->maximumWrite = 2; QVERIFY(transport->start(config));
        device->onWrite = [&] { transport.reset(); };
        transport->writeBytes("delete"); QVERIFY(!transport); QVERIFY(!device->opened);
        device->onWrite = {};
        transport = std::make_unique<CatSerialTransport>(device); QVERIFY(transport->start(config));
        device->onClose = [&] { transport.reset(); };
        transport->stop(); QVERIFY(!transport);
        device->onClose = {};
    }
    void configurationFailuresAndUnavailableDependency() {
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = "test-owned";
        const auto device = std::make_shared<CatSerialTestDevice>(); CatSerialTransport injected(device);
        QSignalSpy errors(&injected, &CatSerialTransport::failed);
        config.serialParity = "invented"; QVERIFY(!injected.start(config)); QCOMPARE(device->opens, 0);
        config.serialParity = "None"; device->refuseOpen = true; QVERIFY(!injected.start(config));
        QVERIFY(injected.errorString().contains("open failure")); QCOMPARE(errors.size(), 2);
        device->refuseOpen = false; QVERIFY(injected.start(config)); device->pinsAvailable = false;
        QVERIFY(!injected.setPinSampling(true)); QVERIFY(injected.errorString().contains("sampling unavailable")); QVERIFY(!device->opened);
#ifndef HAVE_SERIALPORT
        CatSerialTransport native; QVERIFY(!native.start(config)); QVERIFY(native.errorString().contains("dependency unavailable"));
        RadioModel model; CatService& service = *model.catService();
        QVERIFY(service.applyChannelConfig(1, config));
        CatGlobalConfig global = service.globalConfig(); global.pttEnabled = true; global.pttDeviceSource = "Physical";
        global.pttSerialDevice = "test-owned-separate"; global.pttUseCts = true;
        QVERIFY(service.applyGlobalConfig(global)); service.startConfigured();
        QVERIFY(!service.isListening(1)); QVERIFY(service.channelState(1).contains("dependency unavailable"));
        QVERIFY(service.pttState().contains("dependency unavailable")); QVERIFY(service.sessionIds(1).isEmpty());
        QVERIFY(service.globalConfig().pttEnabled); QVERIFY(service.channelConfig(1).serialEnabled);
#endif
    }
    void mixedTcpAndSerialCloseAndAi() {
        const auto device = std::make_shared<CatSerialTestDevice>(); RadioModel model; model.addSlice(); model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice()); model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        CatService& service = *model.catService();
        QVERIFY(service.setSerialTransportFactoryForTest([device] { return std::make_shared<CatSerialTransport>(device); }));
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = "test-owned";
        config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1;
        QTcpServer probe; QVERIFY(probe.listen(QHostAddress::LocalHost, 0)); config.tcpEnabled = true; config.tcpPort = probe.serverPort(); probe.close();
        QVERIFY(service.applyChannelConfig(1, config));
        CatGlobalConfig global = service.globalConfig(); global.aiEnabled = true; global.allowKenwoodAi = true;
        QVERIFY(service.applyGlobalConfig(global)); service.startConfigured();
        QTcpSocket tcp; tcp.connectToHost(QHostAddress::LocalHost, service.boundPort(1));
        QTRY_COMPARE(service.clientCount(1), 1); QCOMPARE(service.sessionIds(1).size(), 2);
        model.sliceById(0)->setFrequency(14074000);
        QTRY_VERIFY(device->output.contains("FA00014074000;"));
        QTRY_VERIFY(tcp.bytesAvailable() > 0); tcp.readAll(); device->output.clear();
        device->receive("ZZZZ;ZZEM1;ZZEM;");
        QVERIFY(!service.serialTransport(1)); QVERIFY(service.isListening(1)); QCOMPARE(service.clientCount(1), 1);
        QCOMPARE(device->output, QByteArray()); QVERIFY(service.channelConfig(1).serialEnabled);
        tcp.write("ZZZZ;ZZEM;");
        QTRY_VERIFY(tcp.bytesAvailable() > 0); QCOMPARE(tcp.readAll(), QByteArray("?;ZZEM0;"));
        QVERIFY(service.isListening(1));
    }
    void nativeQueueAndUnsupportedPins() {
#if defined(HAVE_SERIALPORT) && (defined(Q_OS_MAC) || defined(Q_OS_LINUX))
        const int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK); QVERIFY(master >= 0);
        const auto cleanup = qScopeGuard([master] { ::close(master); });
        QCOMPARE(grantpt(master), 0); QCOMPARE(unlockpt(master), 0);
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = QString::fromLocal8Bit(ptsname(master));
        CatSerialTransport transport; QVERIFY(transport.start(config)); QSignalSpy failed(&transport, &CatSerialTransport::failed);
        for (int i = 0; i < 4; ++i) { transport.writeBytes(QByteArray(64 * 1024, 'x')); }
        QCOMPARE(transport.queuedBytes(), qint64(256 * 1024)); QCOMPARE(failed.size(), 0);
        transport.writeBytes("limit"); QCOMPARE(failed.size(), 1); QVERIFY(!transport.isOpen());
        QVERIFY(transport.start(config));
        // A kernel PTY has no modem inputs. An unsupported ioctl must be visible, never sampled as released.
        QSignalSpy samples(&transport, &CatSerialTransport::pttSampled);
        const bool supported = transport.setPinSampling(true);
        if (!supported) { QVERIFY(!transport.errorString().isEmpty()); QCOMPARE(samples.size(), 0); }
        else { QVERIFY(transport.isOpen()); QCOMPARE(samples.size(), 1); }
        transport.stop();
        config.serialStopBits = "1.5";
        const bool accepted = transport.start(config);
        if (!accepted) { QVERIFY(!transport.errorString().isEmpty()); }
        transport.stop();
#endif
    }
    void nativeDisappearanceAndReadCallbackDeletion() {
#if defined(HAVE_SERIALPORT) && (defined(Q_OS_MAC) || defined(Q_OS_LINUX))
        int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK); QVERIFY(master >= 0);
        const auto cleanup = qScopeGuard([&master] { if (master >= 0) { ::close(master); } });
        QCOMPARE(grantpt(master), 0); QCOMPARE(unlockpt(master), 0);
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = QString::fromLocal8Bit(ptsname(master));
        auto transport = std::make_unique<CatSerialTransport>(); QVERIFY(transport->start(config));
        connect(transport.get(), &CatSerialTransport::bytesReceived, this, [&](const QByteArray&) { transport.reset(); });
        QCOMPARE(::write(master, "close", 5), ssize_t(5)); QTRY_VERIFY_WITH_TIMEOUT(!transport, 3000);
        transport = std::make_unique<CatSerialTransport>(); QVERIFY(transport->start(config));
        QSignalSpy failed(transport.get(), &CatSerialTransport::failed);
        ::close(master); master = -1;
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000); QVERIFY(!transport->isOpen()); QVERIFY(!transport->errorString().isEmpty());
#endif
    }
    void nativeByteLifecycle() {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        const int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        QVERIFY(master >= 0);
        const auto cleanup = qScopeGuard([master] { ::close(master); });
        QCOMPARE(grantpt(master), 0); QCOMPARE(unlockpt(master), 0);
        const QString slave = QString::fromLocal8Bit(ptsname(master)); QVERIFY(!slave.isEmpty());
        RadioModel model; model.addSlice(); model.addSlice();
        CatService* service = model.catService();
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = slave;
        config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1;
        QVERIFY(service->applyChannelConfig(1, config)); service->startConfigured();
#ifdef HAVE_SERIALPORT
        QVERIFY2(service->isListening(1), qPrintable(service->channelState(1)));
        QCOMPARE(service->sessionIds(1).size(), 1);
        QCOMPARE(::write(master, "ZZE", 3), ssize_t(3));
        QCOMPARE(::write(master, "M;ZZEM;", 7), ssize_t(7));
        QByteArray reply;
        QTRY_VERIFY_WITH_TIMEOUT(([&] { char bytes[256]; const ssize_t count = ::read(master, bytes, sizeof(bytes)); if (count > 0) { reply.append(bytes, count); } return reply == "ZZEM0;ZZEM0;"; })(), 3000);
        const quint64 old = service->sessionIds(1).first();
        QCOMPARE(::write(master, "ZZZZ;ZZEM1;ZZEM;", 16), ssize_t(16));
        QTRY_VERIFY_WITH_TIMEOUT(!service->session(old), 3000);
        QVERIFY(!service->isListening(1)); QVERIFY(service->channelConfig(1).serialEnabled);
        service->stopAll(); service->startConfigured(); QVERIFY(service->isListening(1));
        QVERIFY(service->sessionIds(1).first() > old);
#else
        QVERIFY(!service->isListening(1));
        QVERIFY(service->channelState(1).contains("unavailable", Qt::CaseInsensitive));
        QVERIFY(service->sessionIds(1).isEmpty());
#endif
#endif
    }
};
QTEST_GUILESS_MAIN(TstCatSerial)
#include "tst_cat_serial.moc"
