// no-port-check: NereusSDR-original production CAT DSP regressions.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#define private public
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/MoxController.h"
#undef private
#include "core/cat/CatService.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/AudioEngine.h"
#include "core/DspControlThread.h"
#include "core/ReceiverManager.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/codec/P2CodecOrionMkII.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class DspCatMockConnection : public RadioConnection {
    Q_OBJECT
public:
    // Ordered log of setTxDrive() argument values.
    QList<int> txDriveLog;
    // Ordered log of setTxFrequency() argument values (TUNE VFO offset).
    QList<quint64> txFreqLog;

    explicit DspCatMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    // ── Pure-virtual overrides ────────────────────────────────────────────────
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64 hz) override { txFreqLog.append(hz); }
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int level) override { txDriveLog.append(level); }
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};



class TstCatDspCommands : public QObject {
 Q_OBJECT
 quint64 start(RadioModel& model) {
  model.addSlice(); model.addSlice(); model.addSlice(); model.removeSlice(1);
  for (SliceModel* slice : model.slices()) { model.sliceOwnership()->setOwner(slice->sliceIndex(),SliceOwnership::stationDevice()); }
  CatEndpointConfig config; config.binding.primarySliceId=0; config.binding.secondarySliceId=2;
  if (!model.catService()->applyChannelConfig(1,config)) { return 0; }
  model.catService()->startConfigured(); return model.catService()->openSession(1,CatTransportKind::Tester);
 }
 void prepare(RadioModel& model,StepAttenuatorController& controller,P2CodecOrionMkII& codec) {
  model.setBoardForTest(HPSDRHW::OrionMKII); model.setConnectionStateForTest(ConnectionState::Connected);
  model.configureStreamPool(5,5,192000); model.receiverManager()->setMaxReceivers(5); model.receiverManager()->setP2Codec(&codec);
  model.bindUnboundSlices(); controller.setTickTimerEnabled(false); controller.setMinAttenuation(0); controller.setMaxAttenuation(31);
  model.setStepAttController(&controller); controller.setAdcRouting(0,1,Band::Band20m,false,1u<<2);
  controller.setPreampMode(PreampMode::On); controller.setRx2PreampMode(PreampMode::On); controller.setAttenuation(10); controller.setRx2Attenuation(10);
  model.audioEngine()->setVolume(0.5f);
  for (SliceModel* slice : model.slices()) {
   slice->setDspMode(DSPMode::USB); slice->setAgcMode(AGCMode::Slow); slice->setNbMode(NbMode::NB); slice->setActiveNr(NrSlot::Off);
   slice->setAnfEnabled(true); slice->setSnbEnabled(true); slice->setApfEnabled(true); slice->setBinauralEnabled(true); slice->setMuted(true);
   slice->setAutoAgcEnabled(true); slice->setAgcThreshold(20); slice->setApfTuneHz(25); slice->setAfGain(50); slice->setAudioPan(0);
   slice->setNr4Reduction(10); slice->setNb1Threshold(30); slice->setDiglOffsetHz(1500); slice->setDiguOffsetHz(1500);
   slice->setSsqlEnabled(false); slice->setFmsqEnabled(false); slice->setAmsqEnabled(true);
   slice->setAmsqThresh(-80-model.rxMeterOffsetDbForSlice(slice->sliceIndex())); slice->setSsqlThresh(50); slice->setFmsqThresh(-20);
   slice->setDiversityPhaseDeg(9); slice->setLocked(false);
  }
 }
 QJsonObject snapshot(RadioModel& model) {
  QJsonObject state; state["masterVolume"]=model.audioEngine()->volume();
  const QList<QByteArray> properties{"agcMode","nbMode","activeNr","anfEnabled","snbEnabled","apfEnabled","binauralEnabled","muted","autoAgcEnabled","agcThreshold","apfTuneHz","afGain","audioPan","nr4Reduction","nb1Threshold","diglOffsetHz","diguOffsetHz","ssqlEnabled","fmsqEnabled","amsqEnabled","amsqThresh","ssqlThresh","fmsqThresh","diversityPhaseDeg","locked"};
  for (int id : {0,2}) {
   SliceModel* slice=model.sliceById(id); const QString prefix=id==0 ? "primary." : "secondary.";
   for (const QByteArray& property : properties) { state[prefix+QString::fromLatin1(property)]=QJsonValue::fromVariant(slice->property(property)); }
   state[prefix+"agcMode"]=int(slice->agcMode()); state[prefix+"nbMode"]=int(slice->nbMode()); state[prefix+"activeNr"]=int(slice->activeNr());
   state[prefix+"slider"]=slice->amsqThresh()+model.rxMeterOffsetDbForSlice(id);
   state[prefix+"attenuation"]=model.stepAttFacade()->attenuationDbForSlice(id); state[prefix+"preamp"]=model.stepAttFacade()->preampModeForSlice(id);
  }
  for (const QString& group : {"PhoneRx","PhoneTx","CwRx","CwTx","DigRx","DigTx"}) { const QString key="DspOptionsBufferSize"+group; state["settings."+key]=AppSettings::instance().value(key,64).toInt(); }
  state["diversityOwner"]=QJsonDocument::fromJson(model.diversityState().toUtf8()).object().value("live").toObject().value("sliceId").toInt(-1); return state;
 }
private slots:
 void fixtureExecution_data() {
  QTest::addColumn<QJsonObject>("fixture"); QFile file(QFINDTESTDATA("data/cat/requests.json")); QVERIFY(file.open(QIODevice::ReadOnly)); int count=0;
  for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).array()) { const QJsonObject fixture=value.toObject(); if (fixture.value("executionStatus").toString() != "executed-task-6") { continue; } QTest::newRow(qPrintable(fixture.value("id").toString())) << fixture; ++count; }
  QCOMPARE(count,174);
 }
 void fixtureExecution() {
  QFETCH(QJsonObject,fixture); P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model;
  const quint64 id=start(model); QVERIFY(id); prepare(model,controller,codec); const QJsonObject before=snapshot(model);
  QCOMPARE(model.catService()->processFrame(id,fixture.value("request").toString().toLatin1()),fixture.value("expectedReply").toString().toLatin1());
  const QJsonObject after=snapshot(model); const QJsonObject expected=fixture.value("expectedState").toObject();
  for (auto it=expected.begin(); it!=expected.end(); ++it) { QCOMPARE(after.value(it.key()),it.value()); }
  if (fixture.value("expectedMutation").toString()=="none" || expected.isEmpty()) { QCOMPARE(after,before); }
  model.setStepAttController(nullptr);
 }

 void init() { AppSettings::instance().clear(); }
 void cleanup() { AppSettings::instance().clear(); }
 void nativeReadbackAndScaling() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService();
  QCOMPARE(service.processFrame(id,"ZZAG075;"),QByteArray()); QCOMPARE(model.audioEngine()->volume(),0.75f); QCOMPARE(model.sliceById(0)->afGain(),50);
  model.audioEngine()->setVolume(0.2f); QCOMPARE(service.processFrame(id,"ZZAG;"),QByteArray("ZZAG020;"));
  QCOMPARE(service.processFrame(id,"AG1255;"),QByteArray()); QCOMPARE(model.audioEngine()->volume(),1.0f); QCOMPARE(service.processFrame(id,"AG1;"),QByteArray("AG0255;"));
  QCOMPARE(service.processFrame(id,"ZZLE065;"),QByteArray()); QCOMPARE(model.sliceById(2)->afGain(),65); QCOMPARE(model.sliceById(0)->afGain(),50);
  model.sliceById(2)->setAfGain(42); QCOMPARE(service.processFrame(id,"ZZLE;"),QByteArray("ZZLE042;"));
  QCOMPARE(service.processFrame(id,"ZZAR+999;"),QByteArray()); QCOMPARE(model.sliceById(0)->agcThreshold(),120);
  model.sliceById(0)->setAutoAgcEnabled(true); QCOMPARE(service.processFrame(id,"ZZAR+120;"),QByteArray()); QVERIFY(model.sliceById(0)->autoAgcEnabled());
  QCOMPARE(service.processFrame(id,"ZZAR-999;"),QByteArray()); QCOMPARE(model.sliceById(0)->agcThreshold(),-20); QVERIFY(!model.sliceById(0)->autoAgcEnabled());
  QCOMPARE(service.processFrame(id,"ZZAT-999;"),QByteArray()); QCOMPARE(model.sliceById(0)->apfTuneHz(),-250); QCOMPARE(service.processFrame(id,"ZZAT;"),QByteArray("ZZAT-250;"));
  QCOMPARE(service.processFrame(id,"ZZNG075;"),QByteArray()); QCOMPARE(model.sliceById(0)->nr4Reduction(),15.0);
  QCOMPARE(service.processFrame(id,"ZZOL9999;"),QByteArray()); QCOMPARE(model.sliceById(0)->diglOffsetHz(),5000);
 }
 void squelchModesExtremesAndNativeChanges() {
  P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model; const quint64 id=start(model); prepare(model,controller,codec); CatService& service=*model.catService(); SliceModel* slice=model.sliceById(0);
  const double calibration=model.rxMeterOffsetDbForSlice(0);
  QCOMPARE(service.processFrame(id,"SQ0255;"),QByteArray()); QCOMPARE(slice->amsqThresh(),-calibration); QVERIFY(slice->amsqEnabled());
  QCOMPARE(service.processFrame(id,"SQ0000;"),QByteArray()); QCOMPARE(slice->amsqThresh(),-160-calibration); QCOMPARE(service.processFrame(id,"SQ1;"),QByteArray("SQ1000;"));
  QCOMPARE(service.processFrame(id,"ZZSO2;"),QByteArray()); QVERIFY(slice->ssqlEnabled()); QVERIFY(!slice->amsqEnabled());
  QCOMPARE(service.processFrame(id,"SQ0255;"),QByteArray()); QCOMPARE(slice->ssqlThresh(),100.0); QCOMPARE(service.processFrame(id,"ZZSQ;"),QByteArray("ZZSQ000;"));
  QCOMPARE(service.processFrame(id,"SQ0000;"),QByteArray()); QCOMPARE(slice->ssqlThresh(),0.0);
  slice->setSsqlThresh(25); QCOMPARE(service.processFrame(id,"ZZSQ;"),QByteArray("ZZSQ120;"));
  slice->setDspMode(DSPMode::FM); QCOMPARE(service.processFrame(id,"ZZSO1;"),QByteArray()); QVERIFY(slice->fmsqEnabled()); QVERIFY(!slice->ssqlEnabled());
  QCOMPARE(service.processFrame(id,"ZZSQ100;"),QByteArray()); QCOMPARE(slice->fmsqThresh(),-14.8); QCOMPARE(service.processFrame(id,"ZZSQ;"),QByteArray("ZZSQ101;"));
  QCOMPARE(service.processFrame(id,"SQ0255;"),QByteArray()); QCOMPARE(slice->fmsqThresh(),-40.0);
  QCOMPARE(service.processFrame(id,"SQ0000;"),QByteArray()); QCOMPARE(slice->fmsqThresh(),0.0);
  const QJsonObject before=snapshot(model); QCOMPARE(service.processFrame(id,"ZZSO3;"),QByteArray("?;")); QCOMPARE(service.processFrame(id,"SQ0x00;"),QByteArray("?;")); QCOMPARE(snapshot(model),before);
  model.setStepAttController(nullptr);
 }
 void frontendAdcRoutingAndSourceInertModes() {
  P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model; const quint64 id=start(model); prepare(model,controller,codec); CatService& service=*model.catService();
  QVERIFY(model.stepAttFacade()->isBound()); QVERIFY(model.stepAttFacade()->sliceUsesRx2(2));
  QCOMPARE(service.processFrame(id,"ZZRX20;"),QByteArray()); QCOMPARE(controller.attenuatorDb(),20); QCOMPARE(controller.rx2AttenuatorDb(),10);
  QCOMPARE(service.processFrame(id,"ZZRY07;"),QByteArray()); QCOMPARE(controller.rx2AttenuatorDb(),7); QCOMPARE(controller.attenuatorDb(),20);
  QCOMPARE(service.processFrame(id,"ZZPA0;"),QByteArray("?;")); QCOMPARE(controller.preampMode(),PreampMode::On);
  QCOMPARE(service.processFrame(id,"ZZPB0;"),QByteArray("?;")); QCOMPARE(controller.rx2PreampMode(),PreampMode::On);
  QCOMPARE(service.processFrame(id,"ZZPA2;"),QByteArray("?;")); QCOMPARE(controller.preampMode(),PreampMode::On);
  controller.setPreampMode(PreampMode::SaMinus20); QCOMPARE(service.processFrame(id,"ZZPA;"),QByteArray("ZZPA8;"));
  QCOMPARE(service.processFrame(id,"ZZPA1;"),QByteArray()); QCOMPARE(controller.preampMode(),PreampMode::On);
  controller.setRx2PreampMode(PreampMode::SaMinus30); QCOMPARE(service.processFrame(id,"ZZPB;"),QByteArray("ZZPB9;"));
  QCOMPARE(service.processFrame(id,"ZZPB1;"),QByteArray()); QCOMPARE(controller.rx2PreampMode(),PreampMode::On);
  QCOMPARE(service.processFrame(id,"ZZPA9;"),QByteArray()); QCOMPARE(controller.preampMode(),PreampMode::On);
  QCOMPARE(service.processFrame(id,"ZZPB6;"),QByteArray()); QCOMPARE(controller.rx2PreampMode(),PreampMode::On);
  controller.setAdcRouting(0,-1,Band::Band20m,false); QVERIFY(!model.stepAttFacade()->sliceUsesRx2(2));
  QCOMPARE(service.processFrame(id,"ZZRY05;"),QByteArray()); QCOMPARE(service.processFrame(id,"ZZRX;"),QByteArray("ZZRX05;")); QCOMPARE(service.processFrame(id,"ZZRY;"),QByteArray("ZZRY05;"));
  model.setStepAttController(nullptr); const int before=controller.attenuatorDb(); QCOMPARE(service.processFrame(id,"ZZRX19;"),QByteArray("?;")); QCOMPARE(controller.attenuatorDb(),before);
 }
 void bufferLiteralSourceFallback() {
  P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model; const quint64 id=start(model); prepare(model,controller,codec); CatService& service=*model.catService();
  const QList<QByteArray> commands{"ZZHR","ZZHT","ZZHU","ZZHW","ZZHX"};
  const QStringList groups{"PhoneRx","PhoneTx","CwRx","DigRx","DigTx"};
  for (qsizetype i=0; i<commands.size(); ++i) {
   const QString key="DspOptionsBufferSize"+groups[i];
   for (const QByteArray& literal : {QByteArray("V"),QByteArray("v"),QByteArray("+"),QByteArray("-")}) {
    AppSettings::instance().setValue(key,8192); QCOMPARE(service.processFrame(id,commands[i]+literal+";"),QByteArray()); QCOMPARE(AppSettings::instance().value(key).toInt(),256);
   }
  }
  const QJsonObject before=snapshot(model); QCOMPARE(service.processFrame(id,"ZZHVv;"),QByteArray()); QCOMPARE(snapshot(model),before);
  model.setStepAttController(nullptr);
 }
 void bufferAuthorityAndScheduler() {
  P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model; const quint64 id=start(model); prepare(model,controller,codec); CatService& service=*model.catService();
  QList<int> applied; model.setDspOptionsApplyObserverForTest([&](int slice,DSPMode) { applied.append(slice); });
  QCOMPARE(service.processFrame(id,"ZZHR4;"),QByteArray()); QCOMPARE(AppSettings::instance().value("DspOptionsBufferSizePhoneRx").toInt(),4096);
  QTRY_COMPARE(applied.size(),2); QVERIFY(applied.contains(0)); QVERIFY(applied.contains(2));
  QCOMPARE(service.processFrame(id,"ZZHR;"),QByteArray("ZZHR4;"));
  QCOMPARE(service.processFrame(id,"ZZHV6;"),QByteArray()); QVERIFY(!AppSettings::instance().contains("DspOptionsBufferSizeCwTx")); QCOMPARE(service.processFrame(id,"ZZHV;"),QByteArray("ZZHV0;"));
  AppSettings::instance().setValue("DspOptionsBufferSizeCwRx",8192); QCOMPARE(service.processFrame(id,"ZZHV;"),QByteArray("ZZHV5;"));
  QCOMPARE(service.processFrame(id,"ZZHT6;"),QByteArray()); QCOMPARE(AppSettings::instance().value("DspOptionsBufferSizePhoneTx").toInt(),16384);
  QCOMPARE(service.processFrame(id,"ZZHX9;"),QByteArray()); QCOMPARE(AppSettings::instance().value("DspOptionsBufferSizeDigTx").toInt(),256);
  model.moxController()->m_mox=true; QCOMPARE(service.processFrame(id,"ZZHR1;"),QByteArray("?;")); QCOMPARE(AppSettings::instance().value("DspOptionsBufferSizePhoneRx").toInt(),4096); model.moxController()->m_mox=false;
  model.setConnectionStateForTest(ConnectionState::Disconnected); QCOMPARE(service.processFrame(id,"ZZHU2;"),QByteArray("?;"));
  model.setStepAttController(nullptr);
 }
 void diversityAdmittedTransactionAndForeignOwner() {
  P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model; const quint64 id=start(model); prepare(model,controller,codec); CatService& service=*model.catService();
  QCOMPARE(service.processFrame(id,"ZZDE1;"),QByteArray()); QCOMPARE(model.diversityTargetSlice(),model.sliceById(0)); QCOMPARE(service.processFrame(id,"ZZDE;"),QByteArray("ZZDE1;"));
  QCOMPARE(service.processFrame(id,"ZZDD-18000;"),QByteArray()); QCOMPARE(model.sliceById(0)->diversityPhaseDeg(),180.0); QCOMPARE(service.processFrame(id,"ZZDD;"),QByteArray("ZZDD+18000;"));
  model.sliceOwnership()->setOwner(0,"foreign"); const QString before=model.diversityState(); QCOMPARE(service.processFrame(id,"ZZDE0;"),QByteArray("?;")); QCOMPARE(model.diversityState(),before);
  model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice()); QCOMPARE(service.processFrame(id,"ZZDE0;"),QByteArray()); QVERIFY(!model.sliceById(0)->diversityEnabled()); QCOMPARE(service.processFrame(id,"ZZDE;"),QByteArray("ZZDE0;"));
  model.setBoardForTest(HPSDRHW::Hermes); QCOMPARE(service.processFrame(id,"ZZDE1;"),QByteArray("?;")); QVERIFY(!model.sliceById(0)->diversityEnabled()); QCOMPARE(service.processFrame(id,"ZZDE;"),QByteArray("ZZDE0;"));
  model.setStepAttController(nullptr);
 }
 void ownershipIncarnationAndLockReentry() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService();
  model.sliceOwnership()->setOwner(0,"foreign"); QCOMPARE(service.processFrame(id,"ZZNT1;"),QByteArray("?;")); QVERIFY(!model.sliceById(0)->anfEnabled()); model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice());
  model.sliceById(0)->setLocked(true); QSignalSpy second(model.sliceById(2),&SliceModel::lockedChanged);
  connect(model.sliceById(0),&SliceModel::lockedChanged,&service,[&] { QVERIFY(!model.sliceById(2)->locked()); model.removeSlice(2); });
  model.sliceById(2)->setLocked(true); second.clear(); QCOMPARE(service.processFrame(id,"ZZVL0;"),QByteArray("?;")); QCOMPARE(second.count(),0); QVERIFY(!model.sliceById(0)->locked());
  QCOMPARE(model.addSlice(),1); QCOMPARE(model.addSlice(),2); QCOMPARE(service.processFrame(id,"ZZGU4;"),QByteArray("?;"));
  model.sliceById(0)->setLocked(true); QCOMPARE(service.processFrame(id,"ZZVL1;"),QByteArray("?;")); QVERIFY(model.sliceById(0)->locked());
 }
 void nativeRxParameterWiring() {
#ifdef HAVE_WDSP
  DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection);
  const quint64 id=start(model); CatService& service=*model.catService(); WdspEngine* engine=model.wdspEngine();
  engine->m_initialized = true;
  RxChannel* channel=engine->createRxChannel(0,64,256,48000,48000,48000); QVERIFY(channel); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(channel->isWdspReady());
  QCOMPARE(service.processFrame(id,"ZZGT4;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->agcMode(),AGCMode::Fast);
  QCOMPARE(service.processFrame(id,"ZZNA1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->nbMode(),NbMode::NB);
  QCOMPARE(service.processFrame(id,"ZZNB1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->nbMode(),NbMode::NB2);
  QCOMPARE(service.processFrame(id,"ZZNA0;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->nbMode(),NbMode::NB2);
  QCOMPARE(service.processFrame(id,"ZZNE1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->activeNr(),NrSlot::NR1);
  QCOMPARE(service.processFrame(id,"ZZNS1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->activeNr(),NrSlot::NR2);
  QCOMPARE(service.processFrame(id,"ZZNR0;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QCOMPARE(channel->activeNr(),NrSlot::NR2);
  QCOMPARE(service.processFrame(id,"ZZNT1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(channel->m_anfEnabled.load());
  QCOMPARE(service.processFrame(id,"ZZAP1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(channel->apfEnabled());
  QCOMPARE(service.processFrame(id,"ZZNN1;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(channel->snbEnabled());
  QCOMPARE(service.processFrame(id,"ZZSO2;"),QByteArray()); QVERIFY(engine->receiveLane()->waitIdleForTest(30000)); QVERIFY(channel->ssqlEnabled()); QVERIFY(!channel->amsqEnabled());
  channel->setWdspEngine(engine);
  QCOMPARE(channel->dspBlockSize(),4096);
  AppSettings::instance().setValue("DspOptionsBufferSizePhoneRx",64);
  QCOMPARE(service.processFrame(id,"ZZHR;"),QByteArray("ZZHR4;"));
  QCOMPARE(service.processFrame(id,"ZZHR1;"),QByteArray());
  QTRY_COMPARE(channel->dspBlockSize(),512); QVERIFY(engine->receiveLane()->waitIdleForTest(30000));
  QCOMPARE(service.processFrame(id,"ZZHR;"),QByteArray("ZZHR1;"));
  engine->destroyRxChannel(0); model.injectConnectionForTest(nullptr);
#endif
 }
 void lockNotificationRevocation() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService();
  model.sliceById(0)->setLocked(true); model.sliceById(2)->setLocked(true);
  QSignalSpy primary(model.sliceById(0),&SliceModel::lockedChanged); QSignalSpy secondary(model.sliceById(2),&SliceModel::lockedChanged);
  connect(model.sliceById(0),&SliceModel::lockedChanged,&service,[&] {
   QVERIFY(!model.sliceById(0)->locked()); QVERIFY(!model.sliceById(2)->locked());
   model.sliceOwnership()->setOwner(2,"new-owner");
  });
  QCOMPARE(service.processFrame(id,"ZZVL1;"),QByteArray("?;"));
  QCOMPARE(primary.count(),1); QCOMPARE(secondary.count(),0);
  QVERIFY(!model.sliceById(2)->locked()); QCOMPARE(model.sliceOwnership()->mark(2).owner,QByteArray("new-owner"));
 }
 void lockAtomicCommit() {
  RadioModel model; const quint64 id=start(model); CatService& service=*model.catService();
  model.sliceById(0)->setLocked(true);
  QSignalSpy primary(model.sliceById(0),&SliceModel::lockedChanged); QSignalSpy secondary(model.sliceById(2),&SliceModel::lockedChanged);
  bool called=false;
  connect(model.sliceById(2),&SliceModel::lockedChanged,&service,[&](bool value) { called=true; QCOMPARE(model.sliceById(0)->locked(),value); QCOMPARE(model.sliceById(2)->locked(),value); });
  QCOMPARE(service.processFrame(id,"ZZVL0;"),QByteArray()); QVERIFY(called); QCOMPARE(primary.count(),0); QCOMPARE(secondary.count(),1);
  connect(model.sliceById(0),&SliceModel::lockedChanged,&service,[&] { QVERIFY(!model.sliceById(2)->locked()); });
  QCOMPARE(service.processFrame(id,"ZZVL1;"),QByteArray()); QCOMPARE(primary.count(),1); QCOMPARE(secondary.count(),2);
 }
 void productionDspEffects() {
  RadioModel model; const quint64 id=start(model); QVERIFY(id); CatService& service=*model.catService();
  QSignalSpy anf(model.sliceById(0),&SliceModel::anfEnabledChanged);
  QCOMPARE(service.processFrame(id,"ZZNT1;"),QByteArray()); QVERIFY(model.sliceById(0)->anfEnabled()); QCOMPARE(anf.count(),1);
  QCOMPARE(service.processFrame(id,"ZZGU4;"),QByteArray()); QCOMPARE(model.sliceById(2)->agcMode(),AGCMode::Fast);
  QCOMPARE(service.processFrame(id,"ZZOL2100;"),QByteArray()); QCOMPARE(model.sliceById(0)->diglOffsetHz(),2100);
 }
};
QTEST_GUILESS_MAIN(TstCatDspCommands)
#include "tst_cat_dsp_commands.moc"
