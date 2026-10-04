// no-port-check: NereusSDR-original production CAT TX/global regressions.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "CatFixtureHarness.h"
#include <QSemaphore>
#include "core/TwoToneController.h"
#include "core/PureSignal.h"
#include "core/cat/CatService.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
using namespace NereusSDR;
class TstCatTxCommands : public QObject {
 Q_OBJECT
 std::unique_ptr<RadioModel> m_txHost;
 WdspEngine* m_txEngine{nullptr};
 TxChannel* m_liveTx{nullptr};
 quint64 start(RadioModel& model) {
  model.addSlice(); model.addSlice();
  for (SliceModel* slice:model.slices()) { model.sliceOwnership()->setOwner(slice->sliceIndex(),SliceOwnership::stationDevice()); }
  CatEndpointConfig config; config.binding.primarySliceId=0; config.binding.secondarySliceId=1;
  if (!model.catService()->applyChannelConfig(1,config)) { return 0; }
  model.catService()->startConfigured(); return model.catService()->openSession(1,CatTransportKind::Tester);
 }
private slots:
 void initTestCase() {
  // One actual OpenChannel lifetime covers all selectable functions. Each
  // RadioModel borrows the real channel only while this owner remains live.
  m_txHost=std::make_unique<RadioModel>(); m_txEngine=m_txHost->wdspEngine(); m_txEngine->m_initialized = true;
  m_liveTx=m_txEngine->createTxChannel(1,64,256,48000,48000,48000); QVERIFY(m_liveTx); QVERIFY(m_txEngine->transmitLane()->waitIdleForTest(30000));
 }
 void cleanupTestCase() {
  m_txEngine->destroyTxChannel(1); QVERIFY(m_txEngine->transmitLane()->waitIdleForTest(30000)); m_liveTx=nullptr; m_txHost.reset(); m_txEngine=nullptr;
 }
 void init() { AppSettings::instance().clear(); }
 void cleanup() { AppSettings::instance().clear(); }
 void identitiesAndSerial() {
  RadioModel model; const quint64 id=start(model); QVERIFY(id); CatService& service=*model.catService();
  const QList<QString> identities{"PowerSDR","TS-50S","TS-2000","TS-480"}; const QList<QByteArray> replies{"ID900;","ID013;","ID019;","ID020;"};
  for (int i=0;i<identities.size();++i) { CatGlobalConfig config=service.globalConfig(); config.rigIdentity=identities[i]; config.serialNumber="1234-5678"; QVERIFY(service.applyGlobalConfig(config)); QCOMPARE(service.processFrame(id,"ID;"),replies[i]); QCOMPARE(service.processFrame(id,"ZZSN;"),QByteArray("ZZSN1234-5678;")); }
  CatGlobalConfig config=service.globalConfig(); config.serialNumber="bad"; QVERIFY(service.applyGlobalConfig(config)); QCOMPARE(service.processFrame(id,"ZZSN;"),QByteArray("O;")); QCOMPARE(service.globalConfig().serialNumber,QString("bad"));
 }
 void nativeTxEffectsAndEqValidation() {
  RadioModel model; const quint64 id=start(model); QVERIFY(id); CatService& service=*model.catService(); TransmitModel& tx=model.transmitModel();
  QCOMPARE(service.processFrame(id,"ZZMG+12;"),QByteArray()); QCOMPARE(tx.micGainDb(),12);
  QCOMPARE(service.processFrame(id,"ZZGE1;"),QByteArray()); QVERIFY(tx.dexpEnabled());
  QCOMPARE(service.processFrame(id,"ZZCT99;"),QByteArray()); QCOMPARE(tx.cpdrLevelDb(),20);
  QCOMPARE(service.processFrame(id,"ZZMO1;"),QByteArray()); QVERIFY(tx.monEnabled());
  QCOMPARE(service.processFrame(id,"ZZPC077;"),QByteArray()); QCOMPARE(tx.power(),77);
  QList<int> before{tx.txEqPreamp()}; for(int i=0;i<10;++i){ before.append(tx.txEqBand(i)); }
  QCOMPARE(service.processFrame(id,"ZZEB01000100100100100100100100100100100V;"),QByteArray("?;"));
  QList<int> after{tx.txEqPreamp()}; for(int i=0;i<10;++i){after.append(tx.txEqBand(i));} QCOMPARE(after,before);
  QCOMPARE(service.processFrame(id,"ZZEB010001001001001001001001001001001001;"),QByteArray()); QCOMPARE(tx.txEqPreamp(),1); for(int i=0;i<10;++i){QCOMPARE(tx.txEqBand(i),1);}
 }
 void testerCannotKeyAndUnavailableNoEffects() {
  RadioModel model; const quint64 id=start(model); QVERIFY(id); CatService& service=*model.catService();
  service.session(id)->context().transmitAllowed=false;
  for(const QByteArray& command:QList<QByteArray>{"TX;","ZZTX1;","ZZTU1;","ZZUT1;","ZZUS;"}) { QCOMPARE(service.processFrame(id,command),QByteArray("?;")); QVERIFY(!model.transmitModel().isMox()); QVERIFY(!model.isTune()); }
  const DSPMode mode=model.sliceById(0)->dspMode();
  for(const QByteArray& command:QList<QByteArray>{"KYtest;","ZZMX;","ZZVA1;","ZZJP1;"}) { QCOMPARE(service.processFrame(id,command),QByteArray("?;")); QCOMPARE(model.sliceById(0)->dspMode(),mode); QVERIFY(!model.transmitModel().isMox()); }
 }
 void liveRxMeterCacheReadinessFormattingAndCalibration() {
#ifdef HAVE_WDSP
  P2CodecOrionMkII codec; StepAttenuatorController controller; DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection);
  CatTask7FixtureHarness harness; const quint64 id=harness.start(model,controller,codec); CatService& service=*model.catService(); WdspEngine* engine=model.wdspEngine();
  model.sliceById(2)->setFrequency(7150000.0);
  QVERIFY(model.sliceById(0)->streamIndex()!=model.sliceById(2)->streamIndex());
  engine->m_initialized = true;
  RxChannel* first=engine->createRxChannel(0,64,256,48000,48000,48000); RxChannel* second=engine->createRxChannel(2,64,256,48000,48000,48000); QVERIFY(first); QVERIFY(second);
  QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); first->setActive(true); second->setActive(true); QVERIFY(engine->receiveLane()->waitIdleForTest(30000));
  model.setRxMeterCalOverrideDb(4.5); controller.setRx2Attenuation(23);
  DdcAssignment routing{}; routing.streamDdc[model.sliceById(0)->streamIndex()]=0; routing.streamDdc[model.sliceById(2)->streamIndex()]=1; routing.adcCtrl1=1 << 2; model.publishDdcAssignmentForTest(routing);
  QCOMPARE(model.adcForStream(model.sliceById(2)->streamIndex()),1);
  const CatBinding binding=service.session(id)->binding(); double value=0;
  // Hold the existing lane at a barrier to observe a genuinely cold cache.
  QSemaphore entered,release; const auto coldRelease=qScopeGuard([&] { release.release(); }); engine->receiveLane()->postBarrier([&] { entered.release(); release.acquire(); }); QVERIFY(entered.tryAcquire(1,30000));
  QCOMPARE(service.processFrame(id,"SM0;"),QByteArray("?;")); QVERIFY(!first->meterReadingReady());
  release.release(); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(first->meterReadingReady());
  // WDSP computes a finite floor for real zero input; readiness is the
  // capability test. The floor must remain available after actual refresh.
  QVERIFY(service.adapter().readRxMeter(binding,CatVfo::Primary,RxMeterType::SignalPeak,value));
  QCOMPARE(service.processFrame(id,"SM0;"),QByteArray("SM00000;"));
  // Real running WDSP receives deterministic I/Q, then the existing getter
  // requests the cache refresh. This integration is distinct from injection.
  std::array<float,64> inI{},inQ{},outI{},outQ{};
  for(int block=0;block<160;++block) { for(int n=0;n<64;++n) { const double phase=2.0*3.141592653589793*(block*64+n)*1000/48000.0; inI[size_t(n)]=float(0.01*std::cos(phase)); inQ[size_t(n)]=float(0.01*std::sin(phase)); } first->processIq(inI.data(),inQ.data(),outI.data(),outQ.data(),64); }
  first->getMeter(RxMeterType::SignalPeak); QVERIFY(engine->receiveLane()->waitIdleForTest(30000));
  QTRY_VERIFY(service.adapter().readRxMeter(binding,CatVfo::Primary,RxMeterType::SignalPeak,value));
  const double actual=first->getMeter(RxMeterType::SignalPeak); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(actual>-400); QCOMPARE(value,actual+model.rxMeterOffsetDbForSlice(0));
  // Deterministic cache injection proves selector distinctions and source
  // formatting on these same actual active/ready channel wrappers.
  QSemaphore formatsEntered,formatsRelease; const auto formatRelease=qScopeGuard([&] { formatsRelease.release(); }); engine->receiveLane()->postBarrier([&] { formatsEntered.release(); formatsRelease.acquire(); }); QVERIFY(formatsEntered.tryAcquire(1,30000));
  for(RxChannel* channel:{first,second}) { channel->m_meterCache[0].store(-100.25); channel->m_meterCache[1].store(-110.75); channel->m_meterCache[2].store(-9.25); channel->m_meterCache[3].store(-30.75); channel->m_meterCacheReady.store(true); }
  const double offset=model.rxMeterOffsetDbForSlice(0); QVERIFY(offset != model.rxMeterOffsetDbForSlice(2));
  first->m_meterCache[0].store(-400.0);
  QCOMPARE(service.processFrame(id,"SM0;"),QByteArray("SM00000;"));
  QCOMPARE(service.processFrame(id,"ZZRM0;"),QByteArray("ZZRM0")+QByteArray::number(float(-400+offset),'f',1)+" dBm;");
  first->m_meterCache[0].store(-100.25);
  QCOMPARE(service.processFrame(id,"ZZRM0;"),QByteArray("ZZRM0")+QByteArray::number(float(-100.25+offset),'f',1)+" dBm;");
  QCOMPARE(service.processFrame(id,"ZZRM1;"),QByteArray("ZZRM1")+QByteArray::number(float(-110.75+offset),'f',1)+" dBm;");
  QCOMPARE(service.processFrame(id,"ZZRM2;"),QByteArray("ZZRM2-9.3 dBFS;")); QCOMPARE(service.processFrame(id,"ZZRM3;"),QByteArray("ZZRM3-30.8 dBFS;"));
  QVERIFY(service.adapter().readRxMeter(binding,CatVfo::Secondary,RxMeterType::SignalPeak,value)); QCOMPARE(value,-100.25+model.rxMeterOffsetDbForSlice(2));
  model.sliceById(0)->setDspMode(DSPMode::RADE_U); QCOMPARE(service.processFrame(id,"ZZRM0;"),QByteArray("ZZRM0")+QByteArray::number(float(-100.25+offset),'f',1)+" dBm;");
  formatsRelease.release(); QVERIFY(engine->receiveLane()->waitIdleForTest(30000));
  first->setActive(false); first->setActive(true); QVERIFY(engine->receiveLane()->waitIdleForTest(30000));
  QVERIFY(!first->meterReadingReady()); service.adapter().readRxMeter(binding,CatVfo::Primary,RxMeterType::SignalPeak,value); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(first->meterReadingReady());
  first->setActive(false); QVERIFY(!service.adapter().readRxMeter(binding,CatVfo::Primary,RxMeterType::SignalPeak,value));
  engine->destroyRxChannel(0); engine->destroyRxChannel(2); model.setStepAttController(nullptr); model.injectConnectionForTest(nullptr);
#endif
 }
 void stationHolderAuthorityAndReentrantEq() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService(); TransmitModel& tx=model.transmitModel();
  tx.setPower(50); model.setOtherDeviceHoldsRefusal([] { return QStringLiteral("foreign device holds transmit"); });
  QCOMPARE(service.processFrame(id,"ZZPC077;"),QByteArray("?;")); QCOMPARE(tx.power(),50);
  model.setOtherDeviceHoldsRefusal({}); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};});
  KeyerIdentity foreign; foreign.deviceId="foreign"; model.moxController()->setMox(true,foreign);
  QVERIFY(model.moxController()->isMox()); QCOMPARE(service.processFrame(id,"ZZPC077;"),QByteArray("?;")); QCOMPARE(tx.power(),50); model.moxController()->setMox(false);
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMox(true); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"ZZPC077;"),QByteArray()); QCOMPARE(tx.power(),77); model.moxController()->setMox(false);
  // Revocation at the first native signal must stop the remaining writes.
  const int last=tx.txEqBand(9); connect(&tx,&TransmitModel::txEqPreampChanged,&model,[&] {model.sliceOwnership()->setOwner(0,"foreign");});
  QCOMPARE(service.processFrame(id,"ZZEB010001002003004005006007008009010011;"),QByteArray("?;")); QCOMPARE(tx.txEqPreamp(),1); QCOMPARE(tx.txEqBand(9),last);
 }
 void receiveOnlyRefusesAllNativeSources() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService(); service.session(id)->context().transmitAllowed=true;
  model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};});
  QCOMPARE(service.processFrame(id,"ZZTI1;"),QByteArray()); QVERIFY(model.isRxOnly());
  MoxController* mox=model.moxController();
  mox->setMox(true); QVERIFY(!mox->isMox()); mox->onCatPtt(true); QVERIFY(!mox->isMox()); mox->onCatPtt(false);
  mox->onMicPttFromRadio(true); QVERIFY(!mox->isMox()); mox->onMicPttFromRadio(false);
  mox->onVoxActive(true); QVERIFY(!mox->isMox()); mox->onVoxActive(false);
  mox->onSpacePtt(true); QVERIFY(!mox->isMox()); mox->onSpacePtt(false);
  mox->onX2Ptt(true); QVERIFY(!mox->isMox()); mox->onX2Ptt(false);
  model.setMox(true); QVERIFY(!mox->isMox());
  for(const QByteArray& frame:QList<QByteArray>{"TX;","ZZTX1;","ZZTU1;","ZZUT1;"}) { QCOMPARE(service.processFrame(id,frame),QByteArray("?;")); QVERIFY(!mox->isMox()); }
  QCOMPARE(service.processFrame(id,"ZZTI0;"),QByteArray()); QVERIFY(!model.isRxOnly());
 }
 void literalToggleNoOpsAndTesterVoxRefusal() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService(); TransmitModel& tx=model.transmitModel();
  for(const QByteArray& code:QList<QByteArray>{"MO","ZZMO","ZZCP","PS","ZZPS"}) { for(char literal:QByteArray("Vv+-")) { QCOMPARE(service.processFrame(id,code+literal+';'),QByteArray()); } }
  QCOMPARE(service.processFrame(id,"ZZVE1;"),QByteArray("?;")); QVERIFY(!tx.voxEnabled()); QVERIFY(!model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"ZZVE;"),QByteArray("ZZVE0;")); QCOMPARE(service.processFrame(id,"ZZVE0;"),QByteArray()); QVERIFY(!tx.voxEnabled());
 }
 void productionToneLifecycle_data() {
  QTest::addColumn<bool>("twoTone"); QTest::newRow("Tune") << false; QTest::newRow("TwoTone") << true;
 }
 void productionToneLifecycle() {
  QFETCH(bool,twoTone); DspCatMockConnection connection; TxChannel& channel=*m_liveTx; RadioModel model; model.injectConnectionForTest(&connection);
  const quint64 id=start(model); CatService& service=*model.catService(); service.session(id)->context().transmitAllowed=true;
  model.injectTxChannelForTest(&channel); model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true); model.twoToneController()->setSettleDelaysMs(0,0);
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};}); model.setTuneOffSettleMsForTest(0);
  model.transmitModel().setPower(61); const QByteArray code=twoTone ? "ZZUT" : "ZZTU";
  QCOMPARE(service.processFrame(id,code+"1;"),QByteArray()); QTRY_VERIFY(model.moxController()->isMox());
  if(twoTone) {QTRY_VERIFY(model.twoToneController()->isActive());} else {QVERIFY(model.isTune());}
  QCOMPARE(service.processFrame(id,"RX;"),QByteArray()); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,(twoTone ? "ZZTU0;":"ZZUT0;")),QByteArray()); QVERIFY(model.moxController()->isMox());
  const quint64 foreign=service.openSession(1,CatTransportKind::Tester); QVERIFY(foreign); service.session(foreign)->context().transmitAllowed=true;
  QCOMPARE(service.processFrame(foreign,code+"0;"),QByteArray()); QVERIFY(model.moxController()->isMox());
  service.closeSession(id); QTRY_VERIFY(!model.moxController()->isMox()); QTRY_VERIFY(!model.isTune()); QTRY_VERIFY(!model.twoToneController()->isActive()); QTRY_COMPARE(model.transmitModel().power(),61);
  model.injectTxChannelForTest(nullptr); model.twoToneController()->setTxChannel(nullptr); model.injectConnectionForTest(nullptr);
 }
 void productionToneReentry_data() {productionToneLifecycle_data();}
 void productionToneReentry() {
  QFETCH(bool,twoTone); DspCatMockConnection connection; TxChannel& channel=*m_liveTx; RadioModel model; model.injectConnectionForTest(&connection);
  const quint64 reentrant=start(model); CatService& service=*model.catService(); service.session(reentrant)->context().transmitAllowed=true; const QByteArray code=twoTone ? "ZZUT":"ZZTU";
  model.injectTxChannelForTest(&channel); model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true); model.twoToneController()->setSettleDelaysMs(0,0);
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};}); model.setTuneOffSettleMsForTest(0);
  MoxController* mox=model.moxController(); QByteArray callbackReply;
  mox->setKeyingGate([&](PttMode,const KeyerIdentity& keyer) { if(keyer.requestTag) { callbackReply=service.processFrame(reentrant,twoTone ? "ZZTU0;":"ZZUT0;"); mox->setMox(true); } return KeyingAnswer{}; });
  QCOMPARE(service.processFrame(reentrant,code+"1;"),QByteArray("?;")); QCOMPARE(callbackReply,QByteArray()); QVERIFY(mox->isMox()); const quint64 stamp=mox->acceptedRequestGeneration();
  service.closeSession(reentrant); QCOMPARE(mox->acceptedRequestGeneration(),stamp); QVERIFY(mox->isMox()); QVERIFY(!model.isTune()); QVERIFY(!model.twoToneController()->isActive());
  mox->setKeyingGate({}); mox->setMox(false); model.injectTxChannelForTest(nullptr); model.twoToneController()->setTxChannel(nullptr); model.injectConnectionForTest(nullptr);
 }
 void disconnectStopsAtRetiredSessionCallback() {
  DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); const quint64 id=start(model); CatService& service=*model.catService(); service.session(id)->context().transmitAllowed=true;
  model.setConnectionStateForTest(ConnectionState::Connected); model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};});
  QCOMPARE(service.processFrame(id,"TX;"),QByteArray()); QVERIFY(model.moxController()->isMox()); bool retired=false;
  connect(model.moxController(),&MoxController::requestAccepted,&model,[&](const KeyerIdentity& keyer,quint64,bool on) {if(!on && keyer.requestTag) {retired=true; service.closeSession(id);}});
  QCOMPARE(service.processFrame(id,"PS0;"),QByteArray("?;")); QVERIFY(retired); QVERIFY(model.isConnected()); QVERIFY(!service.session(id));
  model.injectConnectionForTest(nullptr);
 }
 void disconnectPreservesNewIntent_data() {
  QTest::addColumn<int>("actor"); QTest::newRow("station-operator") << 0; QTest::newRow("foreign-holder") << 1; QTest::newRow("model-deletion") << 2; QTest::newRow("same-tag-on") << 3;
 }
 void disconnectPreservesNewIntent() {
  QFETCH(int,actor); DspCatMockConnection connection; std::unique_ptr<RadioModel> model=std::make_unique<RadioModel>(); model->injectConnectionForTest(&connection); const quint64 id=start(*model); CatService* service=model->catService(); service->session(id)->context().transmitAllowed=true;
  model->setConnectionStateForTest(ConnectionState::Connected); MoxController* mox=model->moxController(); mox->setTimerIntervals(0,0,0,0,0,0); mox->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};});
  QCOMPARE(service->processFrame(id,"TX;"),QByteArray()); QVERIFY(mox->isMox()); bool intervened=false; quint64 newer=0;
  connect(mox,&MoxController::requestAccepted,model.get(),[&](const KeyerIdentity& keyer,quint64,bool on) {
   if(!on && keyer.requestTag && !intervened) {intervened=true; if(actor==0 || actor==3) {if(actor==3) {mox->onCatPtt(true,keyer);} else {mox->setMox(true);} newer=mox->acceptedRequestGeneration();} else if(actor==1) {model->setOtherDeviceHoldsRefusal([] {return QStringLiteral("foreign holder");});} else {model.reset();}}
  });
  QCOMPARE(service->processFrame(id,"PS0;"),QByteArray("?;")); QVERIFY(intervened);
  if(actor==2) {QVERIFY(!model); return;}
  QVERIFY(model->isConnected()); QVERIFY(service->session(id));
  if(actor==0 || actor==3) {QVERIFY(mox->isMox()); QCOMPARE(mox->acceptedRequestGeneration(),newer); mox->setMox(false);}
  model->setOtherDeviceHoldsRefusal({}); model->injectConnectionForTest(nullptr);
 }
 void explicitDisconnectFromInitialOperatorIntent() {
  DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); const quint64 id=start(model); CatService& service=*model.catService(); model.setConnectionStateForTest(ConnectionState::Connected);
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};}); model.moxController()->setMox(true); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"PS0;"),QByteArray()); QVERIFY(!model.isConnected()); QVERIFY(!model.moxController()->isMox()); model.injectConnectionForTest(nullptr);
 }
 void toneOffDoesNotReleaseOtherKind() {
  DspCatMockConnection connection; TxChannel& channel=*m_liveTx; RadioModel model; model.injectConnectionForTest(&connection);
  const quint64 id=start(model); CatService& service=*model.catService(); service.session(id)->context().transmitAllowed=true;
  model.injectTxChannelForTest(&channel); model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true); model.twoToneController()->setSettleDelaysMs(0,0);
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};}); model.setTuneOffSettleMsForTest(0);
  QVERIFY(service.txCoordinator().requestPtt(id,0)); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"ZZTU0;"),QByteArray()); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"ZZUT0;"),QByteArray()); QVERIFY(model.moxController()->isMox());
  QCOMPARE(service.processFrame(id,"RX;"),QByteArray()); QTRY_VERIFY(!model.moxController()->isMox());
  model.injectTxChannelForTest(nullptr); model.twoToneController()->setTxChannel(nullptr); model.injectConnectionForTest(nullptr);
 }
 void realTxMeterSelectorsAndAudioWiring() {
#ifdef HAVE_WDSP
  DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); const quint64 id=start(model); CatService& service=*model.catService();
  WdspEngine* engine=m_txEngine; TxChannel* channel=m_liveTx; QVERIFY(channel); QVERIFY(engine->transmitLane()->waitIdleForTest(30000));
  model.setConnectionStateForTest(ConnectionState::Connected);
  model.wireTransmitChainForTest(channel); QVERIFY(engine->transmitLane()->waitIdleForTest(30000));
  QCOMPARE(service.processFrame(id,"ZZMG+12;"),QByteArray()); QCOMPARE(service.processFrame(id,"ZZGE1;"),QByteArray());
  QCOMPARE(service.processFrame(id,"ZZCP1;"),QByteArray()); QCOMPARE(service.processFrame(id,"ZZCT11;"),QByteArray());
  QCOMPARE(service.processFrame(id,"ZZTL00300;"),QByteArray("?;"));
  QCOMPARE(service.processFrame(id,"ZZTL0300;"),QByteArray()); QCOMPARE(service.processFrame(id,"ZZTH03100;"),QByteArray());
  QCOMPARE(service.processFrame(id,"ZZMO1;"),QByteArray()); QCOMPARE(service.processFrame(id,"ZZTM080;"),QByteArray());
  QVERIFY(engine->transmitLane()->waitIdleForTest(30000));
  QCOMPARE(channel->lastMicPreampForTest(),model.transmitModel().micPreampLinear()); QVERIFY(channel->lastDexpRunForTest()); QVERIFY(channel->lastTxCpdrOnForTest()); QCOMPARE(channel->lastTxCpdrGainDbForTest(),11.0);
  QCOMPARE(channel->m_filterLowHz,300); QCOMPARE(channel->m_filterHighHz,3100); QVERIFY(model.audioEngine()->txMonitorEnabled()); QCOMPARE(model.audioEngine()->txMonitorVolume(),0.8f);
  model.setBoardForTest(HPSDRHW::OrionMKII); PureSignal* ps=model.installPureSignalForTest(channel); QVERIFY(ps); QSignalSpy calibrations(ps,&PureSignal::calibrationStarted);
  ps->setAutoCalEnabled(false);
  QCOMPARE(service.testCommand(1,"ZZLI1;"),QByteArray("?;")); QVERIFY(!ps->isAutoCalEnabled());
  QCOMPARE(service.processFrame(id,"ZZLI1;"),QByteArray()); QVERIFY(ps->isAutoCalEnabled()); QCOMPARE(service.processFrame(id,"ZZLI;"),QByteArray("ZZLI1;"));
  service.session(id)->context().transmitAllowed=true;
  ps->setRunCalibrationProcessing(false); QCOMPARE(service.processFrame(id,"ZZUS;"),QByteArray("?;")); QCOMPARE(calibrations.size(),0); QVERIFY(!model.moxController()->isMox());
  ps->setRunCalibrationProcessing(true); QCOMPARE(service.processFrame(id,"ZZUS;"),QByteArray()); QCOMPARE(calibrations.size(),1); QVERIFY(!ps->isAutoCalEnabled()); QVERIFY(!model.moxController()->isMox());
  ps->setOperationalReadinessPredicate([] {return false;}); QCOMPARE(service.processFrame(id,"ZZLI1;"),QByteArray("?;")); QVERIFY(!ps->isAutoCalEnabled()); ps->setOperationalReadinessPredicate({});
  service.session(id)->context().transmitAllowed=false;
  const bool voxBefore=channel->lastVoxRunForTest(); QCOMPARE(service.processFrame(id,"ZZVE1;"),QByteArray("?;")); QVERIFY(engine->transmitLane()->waitIdleForTest(30000)); QCOMPARE(channel->lastVoxRunForTest(),voxBefore); QVERIFY(!model.transmitModel().voxEnabled());
  model.moxController()->setTimerIntervals(0,0,0,0,0,0); model.moxController()->setMoxCheck([] {return safety::BandPlanGuard::MoxCheckResult{true,{}};}); model.moxController()->setMox(true); QVERIFY(model.moxController()->isMox());
  QSemaphore entered,release; const auto guard=qScopeGuard([&] {release.release();}); engine->transmitLane()->postBarrier([&] {entered.release();release.acquire();}); QVERIFY(entered.tryAcquire(1,30000));
  channel->m_txMeterCache[size_t(wdspTxaMeterIndex(TxMeterType::AlcPeak))].store(-4.25);
  channel->m_txMeterCache[size_t(wdspTxaMeterIndex(TxMeterType::AlcAvg))].store(-12.75);
  QCOMPARE(service.processFrame(id,"ZZRM4;"),QByteArray("ZZRM4-12.8 dB;"));
  channel->m_txMeterCache[size_t(wdspTxaMeterIndex(TxMeterType::AlcAvg))].store(-400.0);
  QCOMPARE(service.processFrame(id,"ZZRM4;"),QByteArray("ZZRM4-20.0 dB;"));
  model.radioStatus().setPowerReadings(81.4,2.4,1.234);
  QCOMPARE(service.processFrame(id,"ZZRM5;"),QByteArray("ZZRM581 W;")); QCOMPARE(service.processFrame(id,"ZZRM7;"),QByteArray("ZZRM72 W;")); QCOMPARE(service.processFrame(id,"ZZRM8;"),QByteArray("ZZRM81.2 : 1;")); QCOMPARE(service.processFrame(id,"ZZRM6;"),QByteArray("?;"));
  release.release(); QVERIFY(engine->transmitLane()->waitIdleForTest(30000)); model.moxController()->setMox(false);
  model.injectTxChannelForTest(nullptr); model.injectConnectionForTest(nullptr);
#endif
 }
};
QTEST_GUILESS_MAIN(TstCatTxCommands)
#include "tst_cat_tx_commands.moc"
