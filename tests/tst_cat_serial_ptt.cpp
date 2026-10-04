// no-port-check: NereusSDR-original release-armed physical PTT ownership tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "CatFixtureHarness.h"
#include "CatSerialTestDevice.h"
static void pumpCat() { for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); } }
class TstCatSerialPtt : public QObject {
    Q_OBJECT
    void setup(RadioModel& model, RxCatMockConnection& connection) {
        model.injectConnectionForTest(&connection);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        // Same identified software-model admission harness as TCP/ownership tests; no physical microphone.
        model.moxController()->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true, {}}; });
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.configureStreamPool(2, 3, 192000); model.addSlice(); model.addSlice();
        for (SliceModel* slice : model.slices()) {
            model.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice());
            slice->setFrequency(14074000);
        }
        model.txSliceArbiter()->requestHandoff(0, SliceOwnership::stationDevice());
    }
    void sample(CatService& service, const std::shared_ptr<CatSerialTestDevice>& device, int channel, bool cts, bool dsr) {
        const CatGlobalConfig config = service.globalConfig();
        const int source = config.pttDeviceSource == "Physical" ? config.pttChannel : config.pttDeviceSource.right(1).toInt();
        if (channel == source) { device->cts = cts; device->dsr = dsr; }
        // Timer polls and explicitly injected samples see the same device level.
        service.applyPttSample(channel, cts, dsr);
    }
    bool configure(CatService& service, const std::shared_ptr<CatSerialTestDevice>& device, bool separate = false, int channel = 1) {
        if (!service.setSerialTransportFactoryForTest([device] { return std::make_shared<CatSerialTransport>(device); })) { return false; }
        CatEndpointConfig config; config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1;
        config.serialEnabled = !separate; config.serialDevice = "test-owned-device";
        if (!service.applyChannelConfig(channel, config)) { return false; }
        CatGlobalConfig global = service.globalConfig(); global.pttEnabled = true;
        global.pttDeviceSource = separate ? "Physical" : "CAT" + QString::number(channel); global.pttChannel = channel;
        global.pttSerialDevice = "test-owned-ptt"; global.pttUseCts = true; global.pttUseDsr = true;
        return service.applyGlobalConfig(global);
    }
private slots:
    void init() {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); }
        }
    }
    void singleInputAndRefusal_data() {
        QTest::addColumn<bool>("useCts"); QTest::newRow("CTS-legacy-RTS") << true; QTest::newRow("DSR-legacy-DTR") << false;
    }
    void singleInputAndRefusal() {
        QFETCH(bool, useCts);
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService& service = *model.catService();
        QVERIFY(configure(service, device)); CatGlobalConfig global = service.globalConfig();
        global.pttUseCts = useCts; global.pttUseDsr = !useCts; QVERIFY(service.applyGlobalConfig(global));
        service.startConfigured();
        sample(service, device, 2, false, false); sample(service, device, 1, true, true); QVERIFY(!model.moxController()->isMox());
        sample(service, device, 1, !useCts, useCts); // Selected input released, ignored input asserted.
        sample(service, device, 1, true, true); QVERIFY(model.moxController()->isMox());
        sample(service, device, 1, !useCts, useCts); pumpCat(); QVERIFY(!model.moxController()->isMox());
        model.moxController()->setKeyingGate([](PttMode, const KeyerIdentity&) { return KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::changingHands()}; });
        sample(service, device, 1, true, true); QVERIFY(!model.moxController()->isMox()); QCOMPARE(service.pttState(), QString("PTT request refused"));
        model.moxController()->setKeyingGate({});
        sample(service, device, 1, true, true); QVERIFY(!model.moxController()->isMox()); // Held refusal never retries.
        sample(service, device, 1, false, false); sample(service, device, 1, true, true); QVERIFY(model.moxController()->isMox());
        service.stopAll(); pumpCat(); model.injectConnectionForTest(nullptr);
    }
    void serialSessionCloseCancelsPinBeforeHandle() {
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService& service = *model.catService();
        QVERIFY(configure(service, device)); service.startConfigured();
        const quint64 command = service.sessionIds(1).first();
        sample(service, device, 1, false, false); sample(service, device, 1, true, false); QVERIFY(model.moxController()->isMox());
        QCOMPARE(service.processFrame(command, "TX;"), QByteArray());
        connect(&service, &CatService::pttStateChanged, &model, [&](const QString& state) {
            if (state == "Stopped") { QVERIFY(!model.moxController()->isMox()); }
        });
        device->onClose = [&] { QVERIFY(!model.moxController()->isMox()); };
        service.closeSession(command); pumpCat(); QVERIFY(!model.moxController()->isMox());
        QVERIFY(!service.serialTransport(1)); model.injectConnectionForTest(nullptr);
    }
    void sharesClaimAndPreservesNewerOwner() {
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService& service = *model.catService();
        QVERIFY(configure(service, device)); service.startConfigured();
        const quint64 command = service.sessionIds(1).first();
        sample(service, device, 1, false, false); sample(service, device, 1, true, false); QVERIFY(model.moxController()->isMox());
        QCOMPARE(service.processFrame(command, "TX;"), QByteArray());
        sample(service, device, 1, false, false); QVERIFY(model.moxController()->isMox()); // Command still owns its own claim.
        service.closeSession(command); pumpCat(); QVERIFY(!model.moxController()->isMox());
        service.stopAll(); service.startConfigured(); sample(service, device, 1, false, false); sample(service, device, 1, true, false);
        QVERIFY(model.moxController()->isMox()); model.moxController()->setMox(true);
        device->disappear(); pumpCat(); QVERIFY(model.moxController()->isMox());
        QVERIFY(service.pttState().startsWith("PTT error:"));
        model.moxController()->setMox(false); model.injectConnectionForTest(nullptr);
    }
    void outsideBindingAndReusedSliceRefused() {
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService& service = *model.catService();
        QVERIFY(configure(service, device)); service.startConfigured();
        const int outside = model.addSlice(); model.sliceOwnership()->setOwner(outside, SliceOwnership::stationDevice());
        model.sliceById(outside)->setFrequency(14074000);
        QVERIFY(model.txSliceArbiter()->requestHandoff(outside, SliceOwnership::stationDevice()));
        sample(service, device, 1, false, false); sample(service, device, 1, true, false);
        QVERIFY(!model.moxController()->isMox()); QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), outside);
        model.removeSlice(0); QCOMPARE(model.addSlice(), 0); model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14074000); QVERIFY(model.txSliceArbiter()->requestHandoff(0, SliceOwnership::stationDevice()));
        sample(service, device, 1, false, false); sample(service, device, 1, true, true); QVERIFY(!model.moxController()->isMox());
        model.injectConnectionForTest(nullptr);
    }
    void unsupportedSharedPinsAndMissingSourceVisible() {
        RadioModel model; CatService& service = *model.catService();
        const auto device = std::make_shared<CatSerialTestDevice>(); device->pinsAvailable = false;
        QVERIFY(configure(service, device)); service.startConfigured();
        QVERIFY(service.pttState().contains("sampling unavailable")); QVERIFY(service.channelState(1).contains("sampling unavailable"));
        QVERIFY(!device->opened); QVERIFY(service.globalConfig().pttEnabled);
        service.stopAll(); CatEndpointConfig config = service.channelConfig(1); config.serialEnabled = false;
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
        QVERIFY(service.pttState().contains("require an open"));
    }
    void allSharedChannelsUseTheirOpenHandle_data() {
        QTest::addColumn<int>("channel");
        for (int channel = 1; channel <= 4; ++channel) { QTest::newRow(qPrintable(QString::number(channel))) << channel; }
    }
    void allSharedChannelsUseTheirOpenHandle() {
        QFETCH(int, channel);
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService& service = *model.catService();
        QVERIFY(service.setSerialTransportFactoryForTest([device] { return std::make_shared<CatSerialTransport>(device); }));
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = "test-owned";
        config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1; QVERIFY(service.applyChannelConfig(channel, config));
        CatGlobalConfig global = service.globalConfig(); global.pttEnabled = true; global.pttUseCts = true;
        global.pttDeviceSource = "CAT" + QString::number(channel); QVERIFY(service.applyGlobalConfig(global)); service.startConfigured();
        QCOMPARE(device->opens, 1); QVERIFY(service.serialTransport(channel));
        sample(service, device, channel, false, true); sample(service, device, channel, true, true); QVERIFY(model.moxController()->isMox());
        service.stopAll(); pumpCat(); QVERIFY(!model.moxController()->isMox()); model.injectConnectionForTest(nullptr);
    }
    void duplicateDeviceAssignmentsRejected() {
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig first; first.serialEnabled = true; first.serialDevice = "test-owned-shared";
        QVERIFY(service.applyChannelConfig(1, first)); QVERIFY(!service.applyChannelConfig(2, first));
        CatGlobalConfig global = service.globalConfig(); global.pttEnabled = true; global.pttUseCts = true;
        global.pttDeviceSource = "Physical"; global.pttSerialDevice = first.serialDevice;
        QVERIFY(!service.applyGlobalConfig(global)); global.pttSerialDevice = "test-owned-exclusive";
        QVERIFY(service.applyGlobalConfig(global)); first.serialDevice = global.pttSerialDevice;
        QVERIFY(!service.applyChannelConfig(2, first));
    }
    void restoredDuplicateAssignmentsCannotOpenTwice() {
        for (int channel = 1; channel <= 2; ++channel) {
            const QString prefix = "Cat/Channels/" + QString::number(channel) + '/';
            AppSettings::instance().setValue(prefix + "SerialEnabled", "True");
            AppSettings::instance().setValue(prefix + "SerialDevice", "test-owned-duplicate");
        }
        RadioModel model; CatService& service = *model.catService();
        const auto device = std::make_shared<CatSerialTestDevice>();
        QVERIFY(service.setSerialTransportFactoryForTest([device] { return std::make_shared<CatSerialTransport>(device); }));
        service.startConfigured(); QCOMPARE(device->opens, 0);
        QVERIFY(service.channelState(1).contains("duplicate", Qt::CaseInsensitive));
        QVERIFY(service.channelState(2).contains("duplicate", Qt::CaseInsensitive));
    }
    void closeCancelsBeforeNoticeAndRestartSurvives() {
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        QList<std::shared_ptr<CatSerialTestDevice>> devices;
        CatService& service = *model.catService();
        const auto first = std::make_shared<CatSerialTestDevice>(); QVERIFY(configure(service, first));
        QVERIFY(service.setSerialTransportFactoryForTest([&] {
            const auto device = std::make_shared<CatSerialTestDevice>(); devices.append(device);
            return std::make_shared<CatSerialTransport>(device);
        }));
        service.startConfigured(); const auto device = devices.last(); const quint64 old = service.sessionIds(1).first();
        sample(service, device, 1, false, false); sample(service, device, 1, true, false); QVERIFY(model.moxController()->isMox());
        bool noticed = false;
        connect(&service, &CatService::sessionClosed, &model, [&](quint64 id) {
            if (id == old && !noticed) {
                noticed = true; QVERIFY(!model.moxController()->isMox());
                service.stopAll(); service.startConfigured();
            }
        });
        devices.first()->receive("ZZZZ;TX;ZZEM1;");
        QVERIFY(noticed); QVERIFY(service.isStarted()); QVERIFY(service.isListening(1));
        QVERIFY(service.sessionIds(1).first() > old); QVERIFY(devices.last()->opened); QCOMPARE(devices.first()->output, QByteArray());
        QVERIFY(!service.session(service.sessionIds(1).first())->context().verboseErrors);
        service.beginRetirement(); service.startConfigured(); QVERIFY(!service.isStarted());
        model.injectConnectionForTest(nullptr);
    }
    void deletionDuringPinAdmission() {
        RxCatMockConnection connection; auto model = std::make_unique<RadioModel>(); setup(*model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>(); CatService* service = model->catService();
        QVERIFY(configure(*service, device)); service->startConfigured(); sample(*service, device, 1, false, false);
        connect(model->moxController(), &MoxController::requestAccepted, service, [&](const KeyerIdentity&, quint64, bool on) { if (on) { model.reset(); } });
        sample(*service, device, 1, true, false); QVERIFY(!model); QVERIFY(!device->opened);
    }
    void compositeReleaseAndSelectedTarget_data() {
        QTest::addColumn<bool>("separate"); QTest::addColumn<int>("channel");
        QTest::newRow("shared-CAT1") << false << 1; QTest::newRow("exclusive-physical-channel1") << true << 1;
        QTest::newRow("exclusive-physical-channel4") << true << 4;
    }
    void compositeReleaseAndSelectedTarget() {
        QFETCH(bool, separate); QFETCH(int, channel);
        RxCatMockConnection connection; RadioModel model; setup(model, connection);
        const auto device = std::make_shared<CatSerialTestDevice>();
        CatService& service = *model.catService(); QVERIFY(configure(service, device, separate, channel));
        service.startConfigured(); QCOMPARE(device->opens, 1); QVERIFY(!model.moxController()->isMox());
        sample(service, device, channel, false, true); sample(service, device, channel, true, false);
        QVERIFY(!model.moxController()->isMox());
        sample(service, device, channel, false, false);
        QVERIFY(model.txSliceArbiter()->requestHandoff(1, SliceOwnership::stationDevice()));
        sample(service, device, channel, true, false); QVERIFY(model.moxController()->isMox());
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), 1);
        sample(service, device, channel, false, true); QVERIFY(model.moxController()->isMox());
        sample(service, device, channel, false, false); pumpCat(); QVERIFY(!model.moxController()->isMox());
        sample(service, device, channel, false, true); QVERIFY(model.moxController()->isMox());
        device->disappear(); pumpCat(); QVERIFY(!model.moxController()->isMox()); QVERIFY(!device->opened);
        QVERIFY(service.pttState().startsWith("PTT error:"));
        sample(service, device, channel, false, false); sample(service, device, channel, true, true);
        QVERIFY(!model.moxController()->isMox());
        service.stopAll(); device->cts = true; device->dsr = false;
        service.startConfigured(); QVERIFY(!model.moxController()->isMox());
        sample(service, device, channel, false, false); sample(service, device, channel, true, false);
        QVERIFY(model.moxController()->isMox()); service.stopAll(); pumpCat(); QVERIFY(!model.moxController()->isMox());
        model.injectConnectionForTest(nullptr);
    }
};
QTEST_MAIN(TstCatSerialPtt)
#include "tst_cat_serial_ptt.moc"
