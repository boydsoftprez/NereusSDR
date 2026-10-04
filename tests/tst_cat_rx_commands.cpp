// no-port-check: NereusSDR-original CAT model integration regressions.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#define private public
#include "core/MoxController.h"
#undef private
#include "core/cat/CatService.h"
#include "core/SliceOwnership.h"
#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/TxSliceArbiter.h"
#define private public
#include "core/MoxController.h"
#undef private
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class RxCatMockConnection : public RadioConnection {
    Q_OBJECT
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



class TstCatRxCommands : public QObject {
 Q_OBJECT
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
private slots:
 void init() { AppSettings::instance().clear(); }
 void cleanup() { AppSettings::instance().clear(); }
 void fixtureExecution_data() {
    QTest::addColumn<QJsonObject>("fixture");
    QFile file(QFINDTESTDATA("data/cat/requests.json")); QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonArray fixtures=QJsonDocument::fromJson(file.readAll()).array(); int count=0;
    for (const QJsonValue& value : fixtures) {
        const QJsonObject fixture=value.toObject(); if (fixture.value("executionStatus").toString() != "executed-task-5") { continue; }
        QTest::newRow(qPrintable(fixture.value("id").toString())) << fixture; ++count;
    }
    QCOMPARE(count,158);
 }
 void fixtureExecution() {
    QFETCH(QJsonObject,fixture);
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
 void rttyInverseAndNativeReadback() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    for (int offset : {2125,-2125}) {
        CatGlobalConfig global=service.globalConfig(); global.rttyOffsetAEnabled=true; global.rttyOffsetBEnabled=true; global.rttyDiguHz=offset; global.rttyDiglHz=offset; QVERIFY(service.applyGlobalConfig(global));
        model.sliceById(0)->setDspMode(DSPMode::DIGU); model.sliceById(2)->setDspMode(DSPMode::DIGL);
        QCOMPARE(service.processFrame(id,"FA00014075000;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),double(14075000-offset)); QCOMPARE(service.processFrame(id,"FA;"),QByteArray("FA00014075000;"));
        QCOMPARE(service.processFrame(id,"FB00007075000;"),QByteArray()); QCOMPARE(model.sliceById(2)->frequency(),double(7075000+offset)); QCOMPARE(service.processFrame(id,"FB;"),QByteArray("FB00007075000;"));
    }
    QCOMPARE(service.processFrame(id,"ZZRA0;"),QByteArray()); model.sliceById(0)->setFrequency(14076000); QCOMPARE(service.processFrame(id,"FA;"),QByteArray("FA00014076000;"));
 }
 void permissionsIncarnationAndCallbackReentry() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    model.sliceById(0)->setLocked(true); QCOMPARE(service.processFrame(id,"FA00014075000;"),QByteArray("?;")); QCOMPARE(model.sliceById(0)->frequency(),14074000.0); model.sliceById(0)->setLocked(false);
    QCOMPARE(service.processFrame(id,"FA99999999999;"),QByteArray("?;")); QCOMPARE(model.sliceById(0)->frequency(),14074000.0);
    model.sliceOwnership()->setOwner(0,"foreign"); QCOMPARE(service.processFrame(id,"ZZMD07;"),QByteArray("?;")); QCOMPARE(model.sliceById(0)->dspMode(),DSPMode::USB); model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice());
    model.moxController()->m_mox=true; model.moxController()->m_currentKeyer=KeyerIdentity::station(PttMode::Cat);
    QCOMPARE(service.processFrame(id,"ZZFH+3200;"),QByteArray("?;")); QCOMPARE(model.sliceById(0)->filterHigh(),3000); QCOMPARE(service.processFrame(id,"ZZFR+3200;"),QByteArray()); model.moxController()->m_mox=false;
    connect(model.sliceById(0),&SliceModel::filterChanged,&service,[&] { service.closeSession(id); });
    QCOMPARE(service.processFrame(id,"ZZSF15002000;"),QByteArray("?;")); QVERIFY(!service.session(id)); QCOMPARE(model.sliceById(0)->filterLow(),500); QCOMPARE(model.sliceById(0)->filterHigh(),2500);
    const quint64 next=service.openSession(1,CatTransportKind::Tester); model.removeSlice(2); QCOMPARE(model.addSlice(),1); QCOMPARE(model.addSlice(),2);
    QCOMPARE(service.processFrame(next,"FB;"),QByteArray("?;")); QCOMPARE(service.processFrame(next,"ZZME01;"),QByteArray("?;")); QCOMPARE(model.slices().size(),3);
 }
 void actualTxSelectionAndStatus() {
    RxCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection); slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    QCOMPARE(service.processFrame(id,"FT1;"),QByteArray()); QCOMPARE(model.txBoundSlice(),model.sliceById(2)); QVERIFY(!model.moxController()->isMox());
    QCOMPARE(service.processFrame(id,"ZZSP;"),QByteArray("ZZSP1;")); QCOMPARE(service.processFrame(id,"ZZSW1;"),QByteArray()); QCOMPARE(model.txBoundSlice(),model.sliceById(0));
    model.sliceById(0)->setRitEnabled(true); model.sliceById(0)->setXitEnabled(true);
    QCOMPARE(service.processFrame(id,"IF;"),QByteArray("IF000140740000010-0012310000020000000;"));
    model.sliceById(0)->setRitEnabled(false); QCOMPARE(service.processFrame(id,"ZZIF;"),QByteArray("ZZIF000140740000010+00456010000010000000;"));
    service.session(id)->context().transmitAllowed=false; QCOMPARE(service.processFrame(id,"FT1;"),QByteArray("?;")); QCOMPARE(model.txBoundSlice(),model.sliceById(0));
    model.injectConnectionForTest(nullptr);
 }
 void boundBandAndSharedCtun() {
    RadioModel model; slices(model);
    model.sliceById(2)->setFrequency(14075000); model.configureStreamPool(2,3,192000);
    model.sliceById(0)->setFrequency(14074001); model.sliceById(2)->setFrequency(14075001); model.setActiveSlice(0); const quint64 id=start(model); CatService& service=*model.catService();
    QCOMPARE(model.sliceById(0)->streamIndex(),model.sliceById(2)->streamIndex()); QVERIFY(model.sliceById(2)->streamIndex()!=2);
    QCOMPARE(service.processFrame(id,"ZZCO1;"),QByteArray()); QVERIFY(model.sliceById(0)->streamCtunPinned()); QVERIFY(model.sliceById(2)->streamCtunPinned());
    QCOMPARE(service.processFrame(id,"ZZCN;"),QByteArray("ZZCN1;"));
    QVERIFY(model.requestStreamCtunPinned(0,false)); QCOMPARE(service.processFrame(id,"ZZCO;"),QByteArray("ZZCO0;"));
    const double original=model.sliceById(0)->frequency(); QCOMPARE(service.processFrame(id,"ZZBT017;"),QByteArray()); QCOMPARE(bandFromFrequency(model.sliceById(2)->frequency()),Band::Band17m); QCOMPARE(model.sliceById(0)->frequency(),original); QCOMPARE(model.activeSlice(),model.sliceById(0));
 }
 void clampsLookupExtremesAndSteps() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    QCOMPARE(service.processFrame(id,"ZZRH+9999;"),QByteArray()); QCOMPARE(service.globalConfig().rttyDiguHz,3000);
    QCOMPARE(service.processFrame(id,"ZZFH99999;"),QByteArray()); QCOMPARE(model.sliceById(0)->filterHigh(),10000);
    QCOMPARE(service.processFrame(id,"SH;"),QByteArray("SH11;")); QCOMPARE(service.processFrame(id,"SH00;"),QByteArray()); QCOMPARE(model.sliceById(0)->filterHigh(),1400); QCOMPARE(service.processFrame(id,"SH;"),QByteArray("SH00;"));
    QCOMPARE(service.processFrame(id,"ZZAC25;"),QByteArray()); QCOMPARE(model.sliceById(0)->stepHz(),10000000); QCOMPARE(service.processFrame(id,"ZZAC26;"),QByteArray("?;"));
    model.sliceById(0)->setStepHz(100); QCOMPARE(service.processFrame(id,"UP;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14074100.0); QCOMPARE(service.processFrame(id,"DN;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14074000.0);
    QCOMPARE(service.processFrame(id,"ZZSZ0;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14074100.0);
    QCOMPARE(service.processFrame(id,"ZZAD99;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14074100.0);
    model.sliceById(0)->setDspMode(DSPMode::RADE_U); QCOMPARE(service.processFrame(id,"MD;"),QByteArray("?;")); QCOMPARE(service.processFrame(id,"ZZMD;"),QByteArray("?;"));
 }

 void sourceLiteralNoopsAndNegativeCountRefusal() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    for (const QByteArray& frame : {QByteArray("ZZAE-1;"),QByteArray("ZZAF-1;"),QByteArray("ZZBE-1;"),QByteArray("ZZBF-1;")}) { QCOMPARE(service.processFrame(id,frame),QByteArray("?;")); }
    QCOMPARE(model.sliceById(0)->frequency(),14074000.0); QCOMPARE(model.sliceById(2)->frequency(),7074000.0);
    QCOMPARE(service.processFrame(id,"ZZMEV0;"),QByteArray()); QCOMPARE(model.sliceById(2)->dspMode(),DSPMode::USB);
    CatGlobalConfig global=service.globalConfig(); global.rttyOffsetAEnabled=true; QVERIFY(service.applyGlobalConfig(global));
    QSignalSpy notification(&service,&CatService::globalConfigurationChanged);
    QCOMPARE(service.processFrame(id,"ZZRAV;"),QByteArray()); QVERIFY(service.globalConfig().rttyOffsetAEnabled); QCOMPARE(notification.count(),0);
    QCOMPARE(service.processFrame(id,"SHV0;"),QByteArray()); QCOMPARE(model.sliceById(0)->filterHigh(),0);
 }
 void modelDeletionDuringCallbackAndMissingSecondary() {
    auto model=std::make_unique<RadioModel>(); slices(*model); const quint64 id=start(*model,false); CatService* service=model->catService();
    QCOMPARE(service->processFrame(id,"FB;"),QByteArray("?;")); QCOMPARE(model->slices().size(),2);
    connect(model->sliceById(0),&SliceModel::frequencyChanged,service,[&] { model.reset(); });
    QCOMPARE(service->processFrame(id,"FA00014075000;"),QByteArray("?;")); QVERIFY(!model);
 }
 void diversityDeletionAndSupersedingFrequencyCallbacks() {
    {
        auto model=std::make_unique<RadioModel>(); slices(*model); const quint64 id=start(*model); CatService* service=model->catService();
        model->sliceById(0)->diversityPattern();
        connect(model->sliceById(0),&SliceModel::diversityPatternChanged,service,[&] { model.reset(); });
        QCOMPARE(service->processFrame(id,"FA00014075000;"),QByteArray("?;")); QVERIFY(!model);
    }
    {
        RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
        SliceModel* slice=model.sliceById(0); bool superseded=false;
        connect(slice,&SliceModel::frequencyChanged,&service,[&](double) { if (!superseded) { superseded=true; slice->setFrequency(7100000); } });
        QCOMPARE(service.processFrame(id,"FA00014075000;"),QByteArray("?;"));
        QCOMPARE(slice->frequency(),7100000.0); QCOMPARE(slice->band(),Band::Band40m);
    }
    {
        RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
        SliceModel* slice=model.sliceById(0); slice->diversityPattern(); bool superseded=false;
        connect(slice,&SliceModel::diversityPatternChanged,&service,[&](const QString&) { if (!superseded) { superseded=true; slice->setFrequency(7100000); } });
        QCOMPARE(service.processFrame(id,"FA00014075000;"),QByteArray("?;"));
        QCOMPARE(slice->frequency(),7100000.0); QCOMPARE(slice->band(),Band::Band40m);
    }
 }
 void unavailableVerboseClassification() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    QCOMPARE(service.processFrame(id,"ZZEM1;"),QByteArray());
    QCOMPARE(service.processFrame(id,"ZZFI;"),QByteArray("ZZEM:ZZFI:Feature Not Available;"));
    QCOMPARE(service.processFrame(id,"ZZDP;"),QByteArray("ZZEM:ZZDP:Feature Not Available;"));
 }
 void signedReadbackRepresentability() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    for (int value : {-99999,-10000,-9999,9999,10000,99999}) {
        model.sliceById(0)->setRitHz(value); model.sliceById(0)->setXitHz(value); model.sliceById(0)->setFilter(value,value); model.sliceById(2)->setFilter(value,value);
        for (const QByteArray& code : {QByteArray("ZZRF"),QByteArray("ZZXF"),QByteArray("ZZFL"),QByteArray("ZZFH"),QByteArray("ZZFR"),QByteArray("ZZFS")}) {
            const QByteArray expected=std::abs(value)>9999 ? QByteArray("O;") : code+(value<0 ? "-" : "+")+QByteArray::number(std::abs(value))+";";
            QCOMPARE(service.processFrame(id,code+';'),expected);
        }
        QCOMPARE(model.sliceById(0)->ritHz(),value); QCOMPARE(model.sliceById(0)->xitHz(),value); QCOMPARE(model.sliceById(0)->filterLow(),value); QCOMPARE(model.sliceById(2)->filterHigh(),value);
    }
 }
 void literalSourceModeAndNumericConversion() {
    RadioModel model; slices(model); const quint64 id=start(model); CatService& service=*model.catService();
    QCOMPARE(service.processFrame(id,"ZZME08;"),QByteArray("?;")); QCOMPARE(model.sliceById(2)->dspMode(),DSPMode::USB);
    for (const QByteArray& command : {QByteArray("ZZME+1;"),QByteArray("ZZME-0;"),QByteArray("ZZME99;"),QByteArray("ZZMD+1;")}) { QCOMPARE(service.processFrame(id,command),QByteArray()); }
    QCOMPARE(model.sliceById(0)->dspMode(),DSPMode::USB); QCOMPARE(model.sliceById(2)->dspMode(),DSPMode::USB);
    QCOMPARE(service.processFrame(id,"SH+1;"),QByteArray()); QCOMPARE(model.sliceById(0)->filterHigh(),0);
    for (const QByteArray& command : {QByteArray("ZZACV0;"),QByteArray("ZZADV0;"),QByteArray("ZZAU0v;"),QByteArray("ZZAEV0;")}) { QCOMPARE(service.processFrame(id,command),QByteArray("?;")); }
    QCOMPARE(model.sliceById(0)->frequency(),14074000.0); QCOMPARE(model.sliceById(0)->stepHz(),100);
 }
 void frequencyAndRuntimePreferences() {
  RadioModel model;
  QCOMPARE(model.addSlice(), 0); QCOMPARE(model.addSlice(), 1); QCOMPARE(model.addSlice(), 2); model.removeSlice(1);
  model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice()); model.sliceOwnership()->setOwner(2, SliceOwnership::stationDevice());
  model.sliceById(0)->setFrequency(14074000); model.sliceById(2)->setFrequency(7074000);
  CatService& service = *model.catService(); CatEndpointConfig config; config.binding.primarySliceId=0; config.binding.secondarySliceId=2;
  QVERIFY(service.applyChannelConfig(1,config)); service.startConfigured(); const quint64 id=service.openSession(1,CatTransportKind::Tester); QVERIFY(id);
  QCOMPARE(service.processFrame(id,"FA;"),QByteArray("FA00014074000;")); QCOMPARE(service.processFrame(id,"FB;"),QByteArray("FB00007074000;"));
  QSignalSpy frequency(model.sliceById(0),&SliceModel::frequencyChanged);
  QCOMPARE(service.processFrame(id,"FA00014075000;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14075000.0); QCOMPARE(frequency.count(),1);
  QSignalSpy global(&service,&CatService::globalConfigurationChanged);
  QCOMPARE(service.processFrame(id,"ZZRA1;"),QByteArray()); QCOMPARE(global.count(),1); QVERIFY(service.globalConfig().rttyOffsetAEnabled);
  CatGlobalConfig changed=service.globalConfig(); changed.pttUseCts=!changed.pttUseCts; QVERIFY(!service.applyGlobalConfig(changed)); QCOMPARE(global.count(),1);
 }
 void modesOffsetsAndEdges() {
  RadioModel model; model.addSlice(); model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice());
  CatService& service=*model.catService(); CatEndpointConfig config; config.binding.primarySliceId=0; QVERIFY(service.applyChannelConfig(1,config)); service.startConfigured(); const quint64 id=service.openSession(1,CatTransportKind::Tester);
  QCOMPARE(service.processFrame(id,"ZZMD07;"),QByteArray()); QCOMPARE(model.sliceById(0)->dspMode(),DSPMode::DIGU);
  QCOMPARE(service.processFrame(id,"ZZMD12;"),QByteArray("?;")); QCOMPARE(model.sliceById(0)->dspMode(),DSPMode::DIGU);
  QCOMPARE(service.processFrame(id,"ZZRF-0123;"),QByteArray()); QCOMPARE(model.sliceById(0)->ritHz(),-123);
  QCOMPARE(service.processFrame(id,"ZZFH+3000;"),QByteArray()); QCOMPARE(model.sliceById(0)->filterHigh(),3000);
  QCOMPARE(service.processFrame(id,"ZZML;"),QByteArray("ZZML LSB00: USB01: DSB02: CWL03: CWU04:  FM05:  AM06:DIGU07:SPEC08:DIGL09: SAM10: DRM11;"));
 }
};
QTEST_GUILESS_MAIN(TstCatRxCommands)
#include "tst_cat_rx_commands.moc"
