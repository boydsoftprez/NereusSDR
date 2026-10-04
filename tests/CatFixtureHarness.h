// no-port-check: NereusSDR-original shared production CAT fixture harnesses.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include <QtTest>
#define private public
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
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
#include "core/MicProfileManager.h"
#include "core/TxSliceArbiter.h"
#include "models/TransmitModel.h"
#include <QJsonArray>
#include <QJsonObject>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class RxCatMockConnection : public RadioConnection {
public:
    // Ordered log of setTxDrive() argument values.
    QList<int> txDriveLog;
    // Ordered log of setTxFrequency() argument values (TUNE VFO offset).
    QList<quint64> txFreqLog;

    explicit RxCatMockConnection(QObject* parent = nullptr)
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




struct CatRxFixtureHarness {
    quint64 start(RadioModel& model, bool secondary=true) {
        CatEndpointConfig config; config.binding.primarySliceId=0; if (secondary) { config.binding.secondarySliceId=2; }
        if (!model.catService()->applyChannelConfig(1,config)) { return 0; }
        model.catService()->startConfigured(); const quint64 id=model.catService()->openSession(1,CatTransportKind::Tester);
        // Exercise transport-authorized selection with the in-process tester seam.
        if (id) { model.catService()->session(id)->context().transmitAllowed=true; }
        return id;
    }
    void slices(RadioModel& model) {
        model.addSlice(); model.addSlice(); model.addSlice(); model.removeSlice(1);
        model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice()); model.sliceOwnership()->setOwner(2,SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14074000); model.sliceById(2)->setFrequency(7074000);
        for (SliceModel* slice : model.slices()) { slice->setDspMode(DSPMode::USB); slice->setStepHz(100); slice->setFilter(100,3000); slice->setRitHz(-123); slice->setXitHz(456); }
        model.txSliceArbiter()->requestHandoff(0,SliceOwnership::stationDevice());
    }
 void run(const QJsonObject& fixture) {

    RxCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection);
    model.configureStreamPool(2,3,192000); slices(model);
    const quint64 id=start(model); QVERIFY(id);
    CatService& service=*model.catService();
    const QByteArray frame=fixture.value("request").toString().toLatin1();
    QCOMPARE(service.processFrame(id,frame),fixture.value("expectedReply").toString().toLatin1());
    const QJsonObject state=fixture.value("expectedState").toObject();
    for (auto it=state.begin(); it!=state.end(); ++it) {
        const QString key=it.key(); const QJsonValue value=it.value(); SliceModel* slice=key.startsWith("secondary") ? model.sliceById(2) : model.sliceById(0);
        if (key.endsWith("Hz") && (key.startsWith("primary") || key.startsWith("secondary"))) { QCOMPARE(slice->frequency(),value.toDouble()); }
        else if (key.endsWith("Mode")) { QCOMPARE(int(slice->dspMode()),value.toInt()); }
        else if (key.endsWith("Low")) { QCOMPARE(slice->filterLow(),value.toInt()); }
        else if (key.endsWith("High")) { QCOMPARE(slice->filterHigh(),value.toInt()); }
        else if (key.endsWith("Ctun")) { QCOMPARE(slice->streamCtunPinned(),value.toBool()); }
        else if (key=="ritHz") { QCOMPARE(slice->ritHz(),value.toInt()); }
        else if (key=="xitHz") { QCOMPARE(slice->xitHz(),value.toInt()); }
        else if (key=="ritEnabled") { QCOMPARE(slice->ritEnabled(),value.toBool()); }
        else if (key=="xitEnabled") { QCOMPARE(slice->xitEnabled(),value.toBool()); }
        else if (key=="txSliceId") { QCOMPARE(model.txBoundSlice()->sliceIndex(),value.toInt()); }
        else if (key=="stepHz") { QCOMPARE(slice->stepHz(),value.toInt()); }
        else if (key=="ZZRA") { QCOMPARE(service.globalConfig().rttyOffsetAEnabled,value.toBool()); }
        else if (key=="ZZRB") { QCOMPARE(service.globalConfig().rttyOffsetBEnabled,value.toBool()); }
        else if (key=="ZZRH") { QCOMPARE(service.globalConfig().rttyDiguHz,value.toInt()); }
        else if (key=="ZZRL") { QCOMPARE(service.globalConfig().rttyDiglHz,value.toInt()); }
        else { QFAIL(qPrintable(key)); }
    }
    if (fixture.value("expectedMutation").toString()=="none") {
        QCOMPARE(model.sliceById(0)->frequency(),14074000.0); QCOMPARE(model.sliceById(2)->frequency(),7074000.0);
        QCOMPARE(model.sliceById(0)->dspMode(),DSPMode::USB); QCOMPARE(model.sliceById(2)->dspMode(),DSPMode::USB);
        QCOMPARE(model.sliceById(0)->filterLow(),100); QCOMPARE(model.sliceById(0)->filterHigh(),3000);
        QCOMPARE(model.sliceById(0)->ritHz(),-123); QCOMPARE(model.sliceById(0)->xitHz(),456);
        QCOMPARE(model.txBoundSlice(),model.sliceById(0));
    }
    model.injectConnectionForTest(nullptr);
 }
};
class DspCatMockConnection : public RadioConnection {
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




struct CatDspFixtureHarness {
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
 void run(const QJsonObject& fixture) {
   P2CodecOrionMkII codec; StepAttenuatorController controller; RadioModel model;
  const quint64 id=start(model); QVERIFY(id); prepare(model,controller,codec); const QJsonObject before=snapshot(model);
  QCOMPARE(model.catService()->processFrame(id,fixture.value("request").toString().toLatin1()),fixture.value("expectedReply").toString().toLatin1());
  const QJsonObject after=snapshot(model); const QJsonObject expected=fixture.value("expectedState").toObject();
  for (auto it=expected.begin(); it!=expected.end(); ++it) { QCOMPARE(after.value(it.key()),it.value()); }
  if (fixture.value("expectedMutation").toString()=="none" || expected.isEmpty()) { QCOMPARE(after,before); }
  model.setStepAttController(nullptr); model.injectConnectionForTest(nullptr);
 }
};
struct CatTask7FixtureHarness {
 quint64 start(RadioModel& model,StepAttenuatorController& controller,P2CodecOrionMkII& codec) {
  CatDspFixtureHarness dsp; const quint64 id=dsp.start(model); dsp.prepare(model,controller,codec);
  model.sliceById(0)->setLocked(false); model.sliceById(2)->setLocked(false);
  model.txSliceArbiter()->requestHandoff(0,SliceOwnership::stationDevice());
  TransmitModel& tx=model.transmitModel();
  tx.setPower(50); tx.setMicGainDb(-6); tx.setMonEnabled(false); tx.setMonitorVolume(0.5f); tx.setVoxEnabled(false); tx.setVoxThresholdDb(-40); tx.setVoxHangTimeMs(500);
  tx.setCpdrOn(false); tx.setCpdrLevelDb(2); tx.setDexpEnabled(false); tx.setTxEqEnabled(false); tx.setFilterLow(100); tx.setFilterHigh(2900); tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
  tx.setTxEqPreamp(0); for(int i=0;i<10;++i){tx.setTxEqBand(i,0);}
  if (MicProfileManager* profiles=model.micProfileManager()) {
   profiles->setMacAddress("02:00:00:00:00:77"); profiles->load();
   tx.setMicGainDb(12); profiles->saveProfile("A CAT fixture",&tx);
   tx.setMicGainDb(-6); profiles->saveProfile("Z CAT fixture",&tx);
   const QStringList names=profiles->profileNames();
   for(const QString& name:names) { if(name != "A CAT fixture" && name != "Z CAT fixture") { profiles->deleteProfile(name); } }
   model.selectTxProfileForStation("Z CAT fixture",nullptr,false);
  }
  CatGlobalConfig config=model.catService()->globalConfig(); config.allowKenwoodAi=true; config.aiEnabled=false; config.rigIdentity="TS-2000"; config.serialNumber="1234-5678"; config.limitReportedPower=true;
  model.catService()->applyGlobalConfig(config);
  model.setRxOnly(false);
  QCoreApplication::setApplicationVersion("2026.10.0");
  return id;
 }
 QJsonObject snapshot(RadioModel& model) {
  CatDspFixtureHarness dsp; QJsonObject state=dsp.snapshot(model); TransmitModel& tx=model.transmitModel();
  for (const QByteArray& property:QList<QByteArray>{"power","micGainDb","monEnabled","monitorVolume","voxEnabled","voxThresholdDb","voxHangTimeMs","cpdrOn","cpdrLevelDb","dexpEnabled","txEqEnabled","filterLow","filterHigh","mox"}) { state["tx."+QString::fromLatin1(property)]=QJsonValue::fromVariant(tx.property(property)); }
  state["tx.eqPreamp"]=tx.txEqPreamp(); for(int i=0;i<10;++i){state[QStringLiteral("tx.eq%1").arg(i)]=tx.txEqBand(i);state[QStringLiteral("tx.freq%1").arg(i)]=tx.txEqFreq(i);}
  state["tx.profile"]=model.micProfileManager() ? model.micProfileManager()->activeProfileName() : QString();
  state["connected"]=model.isConnected(); state["rxOnly"]=model.isRxOnly(); state["tune"]=model.isTune(); state["identity"]=model.catService()->globalConfig().rigIdentity; state["ai"]=model.catService()->globalConfig().aiEnabled; state["serial"]=model.catService()->globalConfig().serialNumber;
  state["primary.rxAntenna"]=model.sliceById(0)->rxAntenna(); state["primary.txAntenna"]=model.sliceById(0)->txAntenna(); return state;
 }
 void run(const QJsonObject& fixture) {
  P2CodecOrionMkII codec; StepAttenuatorController controller; DspCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); const quint64 id=start(model,controller,codec); QVERIFY(id);
  if (fixture.value("familyModelSetup").toObject().value("serial").isString()) { CatGlobalConfig config=model.catService()->globalConfig(); config.serialNumber=fixture.value("familyModelSetup").toObject().value("serial").toString(); QVERIFY(model.catService()->applyGlobalConfig(config)); }
  const QString hardware=fixture.value("familyModelSetup").toObject().value("hardware").toString();
  if(hardware == "HPSDR") { model.setBoardForTest(HPSDRHW::Atlas); }
  if(hardware == "HERMES") { model.setBoardForTest(HPSDRHW::Hermes); }
  const QJsonObject before=snapshot(model);
  const bool verboseBefore=model.catService()->session(id)->context().verboseErrors;
  QCOMPARE(model.catService()->processFrame(id,fixture.value("request").toString().toLatin1()),fixture.value("expectedReply").toString().toLatin1());
  const QJsonObject after=snapshot(model); const QJsonObject expected=fixture.value("expectedState").toObject();
  for(auto it=expected.begin();it!=expected.end();++it){QCOMPARE(after.value(it.key()),it.value());}
  if(fixture.value("command").toString()=="ZZEM" && fixture.value("form").toString()=="Set") { QVERIFY(model.catService()->session(id)->context().verboseErrors); }
  else { QCOMPARE(model.catService()->session(id)->context().verboseErrors,verboseBefore); }
  if(fixture.value("expectedMutation").toString()=="none"){QCOMPARE(after,before);}
  model.setStepAttController(nullptr); model.injectConnectionForTest(nullptr);
 }
};
