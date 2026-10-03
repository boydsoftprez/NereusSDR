// no-port-check: NereusSDR-original. R-R3-49 / R-R3-32 / R-IOS-13
// (remote-window parity Task 33): the CFC bar chart and PA Values' transmit
// readings in a remote window (txReadingsVersion 1: txState's forwardAdcRaw
// and reflectedAdcRaw, the txCfcCompression record stream); a remote
// window's RF Pwr and SWR bars, container transmit meters and S-meter read
// the Core; Max Bin is measured by each window from the slice's own pan.
// Loopback link, a test TxChannel, no RF and no hardware: "on the air" keys
// the Core's own MoxController with the receive-only MOX pre-check lifted,
// as tst_remote_pa_pages does. No audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF. Slice-aware own-pan resolver callback.
//                                    AI-assisted via OpenAI Codex.
//   2026-09-27  J.J. Boyd / KG4VCF  Remote-window parity Task 33 (R-R3-49,
//                                    R-R3-32, R-IOS-13). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  A9 (iPhone app plan Task 39): the seven
//                                    container stage meters
//                                    (txReadingsVersion 3). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <atomic>
#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "core/StepAttenuatorFacade.h"
#include "core/TxChannel.h"
#include "core/WdspTypes.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/meters/TxMeterPump.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/HGauge.h"
#include "gui/SMeterWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/setup/PaSetupPages.h"
#include "gui/widgets/MetricLabel.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:33");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A receive-only Core with its transmit chain wired to a test TxChannel and
// one window, handshake complete (tst_remote_pa_pages's harness).
struct Session {
    Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , txChannel(/*channelId=*/1)
    {
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        core = makeStationRadioModel();
        core->wireTransmitChainForTest(&txChannel);
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    ~Session()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        client.reset();
        server.reset();
        core->injectTxChannelForTest(nullptr);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    void keyCore()
    {
        MoxController* const mox = core->moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
    }
    void unkeyCore() { core->moxController()->setMox(false); }

    QTemporaryDir settingsDir;
    AppSettings settings;
    TxChannel txChannel;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

// A connection the local PA Values page subscribes to, whose PA samples a
// test fires (tst_pa_values_page's TestNullConnection).
class LocalConnection : public RadioConnection {
    Q_OBJECT
public:
    LocalConnection() : RadioConnection() {}
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void sendTxIq(const float*, int) override {}
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
    void setWatchdogEnabled(bool) override {}
    using RadioConnection::paTelemetryUpdated;
};

// The CFC display a test Core reads: each bin a whole tenth of a dB inside
// the chart's 0..16 dB range, so the wire's rounding changes nothing.
double cfcBin(int i)
{
    return static_cast<double>(i % 160) / 10.0;
}

// What the local dialog draws for one reading: Thetis's timerTick slice
// over the chart's range (frmCFCConfig.cs timerTick, TxCfcDialog).
QVector<double> expectedCfcBars(const ParametricEqWidget& chart)
{
    const double binsPerHz = static_cast<double>(TxChannel::kCfcDisplayBinCount)
        / TxChannel::kCfcDisplaySampleRateHz;
    const int start = static_cast<int>(chart.frequencyMinHz() * binsPerHz);
    const int end = std::min(static_cast<int>(chart.frequencyMaxHz() * binsPerHz),
                             TxChannel::kCfcDisplayBinCount - 1);
    QVector<double> bars;
    for (int i = start; i <= end; ++i) {
        bars.append(cfcBin(i));
    }
    return bars;
}

// Everything a window's meters need to follow its Core's transmit state,
// as MainWindow wires them (wireRemoteTransmitMeters, populateDefaultMeter).
struct RemoteMeters {
    explicit RemoteMeters(RadioModel& model, TransmitState& state)
    {
        poller.setSMeter(&smeter);
        poller.setRadioStatus(&model.radioStatus());
        poller.setRemoteRadioModel(&model, []() { return true; });
        poller.setRemoteTxReadingsAvailable([]() { return true; });
        poller.setRemoteTransmitState(&state, [this]() { return unavailable; });
        QObject::connect(&state, &TransmitState::stateChanged, &smeter,
                         [this, &state]() { smeter.setTransmitting(state.keyed()); });
        QObject::connect(&model.radioStatus(), &RadioStatus::powerChanged, &smeter,
                         [this](double fwd, double, double swr) {
                             smeter.setTxMeters(static_cast<float>(fwd),
                                                static_cast<float>(swr));
                         });
    }
    SMeterWidget smeter;
    MeterPoller poller;
    QString unavailable;
};

void tick(MeterPoller& poller)
{
    QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
}

}  // namespace

class TstRemoteTxReadings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void capabilityComesRightAfterTxStateVersion();
    void coreSendsItsRawPaReadingsKeyedOrNot();
    void corePublishesScaledPaReadings();
    void paValuesMatchesTheLocalPageOnTheSameInputs();
    void paValuesBelowVersion1ShowsEachUnavailable();
    void cfcChartDrawsTheCoresBinsAndStopsWhenClosed();
    void cfcStreamWrongShapesAreRefused();
    void cfcChartBelowVersion1SaysWhy();
    void remoteBarsAndMetersMatchALocalWindow();
    void sMeterTxModesMatchALocalWindow();
    void sMeterMenuOffersEveryItemInARemoteWindow();
    void maxBinReadsTheSlicesOwnPanAsALocalWindowDoes();
    void cfcBinsRoundTripToATenth();
    void pumpWorksTheCompressionReadingAsThetis();
    void pumpWorksTheStageReadingsAsALocalWindow();
    void remoteStageMetersMatchALocalWindow();
    void coreSendsItsStageReadingsKeyed();
    void newWordingIsPlain();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteTxReadings::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    AppSettings::setProfileOverride(
        QStringLiteral("remote-tx-readings-%1").arg(QCoreApplication::applicationPid()));
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemoteTxReadings::cleanupTestCase()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteTxReadings::cleanup()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

// txReadingsVersion goes right after txStateVersion, only with it.
void TstRemoteTxReadings::capabilityComesRightAfterTxStateVersion()
{
    StationCapabilities caps;
    caps.radioIdentityEntries = true;
    caps.remoteTxEntry = true;
    caps.txStateVersion = 2;
    caps.txReadingsVersion = 1;
    QList<QByteArray> names;
    for (const MirrorUpdate& u : caps.toUpdates()) {
        names.append(u.name);
    }
    const qsizetype state = names.indexOf("txStateVersion");
    QVERIFY(state >= 0);
    QCOMPARE(names.value(state + 1), QByteArray("txReadingsVersion"));
    QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).txReadingsVersion, 1);

    caps.remoteTxEntry = false;
    names.clear();
    for (const MirrorUpdate& u : caps.toUpdates()) {
        names.append(u.name);
    }
    QVERIFY(!names.contains("txReadingsVersion"));

    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QCOMPARE(s.server->txReadingsVersion(), 3);
    QCOMPARE(s.client->capabilities().txReadingsVersion, 3);
    QVERIFY(s.client->txReadingsAvailable());
    QVERIFY(s.client->txStageReadingsAvailable());
    QCOMPARE(s.window.stationTxReadingsVersion(), 3);
    QCOMPARE(s.window.stationTransmitState(), s.client->transmitState());

    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    AppSettings remoteSettings(scratch.filePath(QStringLiteral("NereusSDR.settings")));
    RadioModel remote(RadioModel::Role::Remote);
    StationServer relay(&remote, remoteSettings,
                        NereusSDR::Test::seedUpgradedCoreToken(scratch.path()));
    QCOMPARE(relay.txReadingsVersion(), 0);
}

// forwardAdcRaw and reflectedAdcRaw follow the radio's samples unkeyed, and
// the meters' pace keyed.
void TstRemoteTxReadings::coreSendsItsRawPaReadingsKeyedOrNot()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitState* windowTx = s.client->transmitState();
    s.core->handlePaTelemetryForTest(1234, 567, 0, 0, 0, 0);
    QCOMPARE(s.server->transmitState()->forwardAdcRaw(), 1234);
    QTRY_COMPARE(windowTx->forwardAdcRaw(), 1234);
    QTRY_COMPARE(windowTx->reflectedAdcRaw(), 567);

    // The window's RF Pwr bar and container meters, wired as MainWindow
    // wires them, over the real link (the G2 bench finding of 2026-09-26:
    // the Core read 4 W on TUNE while the window's bars stayed at 0).
    TxApplet applet(&s.window);
    RemoteMeters meters(s.window, *windowTx);

    s.keyCore();
    QTRY_VERIFY(s.core->isTransmitting());
    s.core->handlePaTelemetryForTest(2100, 90, 0, 0, 0, 0);
    QTRY_COMPARE(windowTx->forwardAdcRaw(), 2100);
    QTRY_COMPARE(windowTx->reflectedAdcRaw(), 90);
    const double coreWatts = s.core->radioStatus().forwardPowerWatts();
    QVERIFY(coreWatts > 1.0);
    QTRY_COMPARE(windowTx->forwardPowerWatts(), coreWatts);
    QTRY_COMPARE(s.window.radioStatus().forwardPowerWatts(), coreWatts);
    QTRY_VERIFY(applet.fwdPowerGauge()->value() > 0.25 * coreWatts);
    QCOMPARE(s.window.radioStatus().swrRatio(), s.core->radioStatus().swrRatio());

    // The COMP reading travels with the other meters, keyed: the Core's
    // pump works it as Thetis does (console.cs:46979, max(-30, COMP)).
    QVERIFY(s.server->transmitState()->property("compressionDb").isValid());
    QTRY_COMPARE(windowTx->property("compressionDb").toDouble(),
                 s.server->transmitState()->property("compressionDb").toDouble());
    s.unkeyCore();
    QTRY_VERIFY(!s.core->isTransmitting());
}

void TstRemoteTxReadings::corePublishesScaledPaReadings()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QCOMPARE(s.client->capabilities().txReadingsVersion, 3);
    const double watts = scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 2600);
    const double forwardVolts = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 2600);
    const double reflectedVolts = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 300);
    TransmitState* const coreState = s.server->transmitState();
    QSignalSpy changed(coreState, &TransmitState::adcRawChanged);
    QObject::connect(coreState, &TransmitState::adcRawChanged, &s.window, [coreState, radio = s.core.get()]() {
        const HPSDRModel model = radio->hardwareProfile().model;
        QCOMPARE(coreState->forwardRawPowerWatts(), scaleFwdPowerWatts(
            model, static_cast<quint16>(coreState->forwardAdcRaw())));
        QCOMPARE(coreState->forwardAdcVolts(), scaleFwdRevVoltage(
            model, static_cast<quint16>(coreState->forwardAdcRaw())));
        QCOMPARE(coreState->reflectedAdcVolts(), scaleFwdRevVoltage(
            model, static_cast<quint16>(coreState->reflectedAdcRaw())));
    });
    s.core->handlePaTelemetryForTest(2600, 300, 0, 0, 0, 0);
    QCOMPARE(changed.size(), 1);
    QTRY_COMPARE(s.client->transmitState()->property("forwardRawPowerWatts").toDouble(), watts);
    QTRY_COMPARE(s.client->transmitState()->property("forwardAdcVolts").toDouble(), forwardVolts);
    QTRY_COMPARE(s.client->transmitState()->property("reflectedAdcVolts").toDouble(), reflectedVolts);
    s.core->handlePaTelemetryForTest(2600, 300, 0, 0, 0, 0);
    QCOMPARE(changed.size(), 1);
    s.core->setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    s.core->emitCurrentRadioChangedForTest();
    QCOMPARE(changed.size(), 2);
    QCOMPARE(coreState->forwardAdcRaw(), 2600);
    QCOMPARE(coreState->forwardRawPowerWatts(),
             scaleFwdPowerWatts(HPSDRModel::HERMESLITE, 2600));
    QTRY_COMPARE(s.client->transmitState()->forwardRawPowerWatts(),
                 coreState->forwardRawPowerWatts());
    QVERIFY(coreState->forwardRawPowerWatts() != watts);
    s.core->setHpsdrModelForTest(HPSDRModel::HERMES);
    s.core->emitCurrentRadioChangedForTest();
    QCOMPARE(changed.size(), 3);
    QCOMPARE(coreState->forwardRawPowerWatts(), scaleFwdPowerWatts(HPSDRModel::HERMES, 2600));
    QVERIFY(coreState->forwardRawPowerWatts() != watts);
    s.core->handlePaTelemetryForTest(0, 0, 0, 0, 0, 0);
    QCOMPARE(coreState->forwardRawPowerWatts(), 0.0);
    QCOMPARE(coreState->forwardAdcVolts(), 0.0);
    QCOMPARE(coreState->reflectedAdcVolts(), 0.0);
    s.core->handlePaTelemetryForTest(1000, 200, 0, 0, 0, 0);
    QVERIFY(coreState->forwardRawPowerWatts() > 0.0);
    coreState->unbind();
    QCOMPARE(coreState->forwardRawPowerWatts(), 0.0);
    QCOMPARE(coreState->forwardAdcVolts(), 0.0);
    QCOMPARE(coreState->reflectedAdcVolts(), 0.0);
    coreState->bind(s.core.get());
    QCOMPARE(coreState->forwardRawPowerWatts(), scaleFwdPowerWatts(HPSDRModel::HERMES, 1000));
    s.client->transmitState()->clearStationValues();
    QCOMPARE(s.client->transmitState()->forwardRawPowerWatts(), 0.0);
    QCOMPARE(s.client->transmitState()->forwardAdcVolts(), 0.0);
    QCOMPARE(s.client->transmitState()->reflectedAdcVolts(), 0.0);
}

// One set of radio samples feeds a local window's PA Values and, through the
// Core, a remote window's: every value and the peak and minimum match.
void TstRemoteTxReadings::paValuesMatchesTheLocalPageOnTheSameInputs()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());

    RadioModel local;
    local.setBoardForTest(HPSDRHW::Saturn);
    local.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    auto* conn = new LocalConnection();
    local.injectConnectionForTest(conn);
    PaValuesPage localPage(&local);
    PaValuesPage remotePage(&s.window);

    const auto feed = [&](quint16 fwd, quint16 rev) {
        s.core->handlePaTelemetryForTest(fwd, rev, 0, 0, 0, 0);
        local.handlePaTelemetryForTest(fwd, rev, 0, 0, 0, 0);
        emit conn->paTelemetryUpdated(fwd, rev, 0, 0, 0, 0);
    };
    const auto same = [&]() {
        return remotePage.fwdCalibratedTextForTest() == localPage.fwdCalibratedTextForTest()
            && remotePage.revPowerTextForTest() == localPage.revPowerTextForTest()
            // SWR's current value; its peak differs by design: a local
            // window's RadioStatus sets forward then reflected power, so
            // its page also sees an SWR worked from the new forward and the
            // old reflected reading, which the Core's single sample does
            // not carry (reported with Task 33).
            && remotePage.swrTextForTest().section(QLatin1String("  ("), 0, 0)
                   == localPage.swrTextForTest().section(QLatin1String("  ("), 0, 0)
            && remotePage.fwdRawTextForTest() == localPage.fwdRawTextForTest()
            && localPage.fwdRawTextForTest() == QString::number(
                   s.server->transmitState()->forwardRawPowerWatts(), 'f', 2) + QStringLiteral(" W")
            && remotePage.fwdVoltageTextForTest() == localPage.fwdVoltageTextForTest()
            && localPage.fwdVoltageTextForTest() == QString::number(
                   s.server->transmitState()->forwardAdcVolts(), 'f', 2) + QStringLiteral(" V")
            && remotePage.revVoltageTextForTest() == localPage.revVoltageTextForTest()
            && localPage.revVoltageTextForTest() == QString::number(
                   s.server->transmitState()->reflectedAdcVolts(), 'f', 2) + QStringLiteral(" V")
            && remotePage.fwdAdcTextForTest() == localPage.fwdAdcTextForTest()
            && remotePage.revAdcTextForTest() == localPage.revAdcTextForTest();
    };

    feed(2600, 300);
    QTRY_VERIFY2(same(), qPrintable(remotePage.fwdCalibratedTextForTest() + QLatin1String(" / ")
                                    + localPage.fwdCalibratedTextForTest()));
    QCOMPARE(remotePage.fwdAdcTextForTest(), QStringLiteral("2600"));
    QCOMPARE(remotePage.fwdRawTextForTest(),
             QString::number(scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 2600), 'f', 2)
                 + QStringLiteral(" W"));
    const auto diff = [&]() {
        return QStringList{remotePage.fwdCalibratedTextForTest(), localPage.fwdCalibratedTextForTest(),
                           remotePage.revPowerTextForTest(), localPage.revPowerTextForTest(),
                           remotePage.swrTextForTest(), localPage.swrTextForTest(),
                           remotePage.fwdRawTextForTest(), localPage.fwdRawTextForTest(),
                           remotePage.fwdAdcTextForTest(), localPage.fwdAdcTextForTest()}
            .join(QLatin1String(" | "));
    };
    feed(1800, 120);
    QTRY_VERIFY2(same(), qPrintable(diff()));
    QCOMPARE(remotePage.fwdCalibratedPeakForTest(), localPage.fwdCalibratedPeakForTest());
    QCOMPARE(remotePage.fwdCalibratedMinForTest(), localPage.fwdCalibratedMinForTest());
    QVERIFY(remotePage.fwdCalibratedTextForTest().contains(QStringLiteral("(P ")));

    // Each says it is the Core's (R-R3-32).
    for (MetricLabel* label : remotePage.findChildren<MetricLabel*>()) {
        if (label->toolTip().isEmpty()) {
            continue;
        }
        QCOMPARE(label->toolTip(), QStringLiteral("From the Core"));
    }

    // ADC overload: the Core's step attenuator's report.
    QCOMPARE(remotePage.adcOverloadTextForTest(), QStringLiteral("No"));
    QVERIFY(s.window.stepAttFacade()->applyRemoteProperty("overloadAdc1", 2));
    QCOMPARE(remotePage.adcOverloadTextForTest(), QStringLiteral("Yes (ADC 1)"));
    QVERIFY(s.window.stepAttFacade()->applyRemoteProperty("overloadAdc1", 0));
    QCOMPARE(remotePage.adcOverloadTextForTest(), QStringLiteral("No"));

    local.injectConnectionForTest(nullptr);
    delete conn;
}

void TstRemoteTxReadings::paValuesBelowVersion1ShowsEachUnavailable()
{
    RadioModel window(RadioModel::Role::Remote);
    window.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    TransmitState state;
    window.setStationTransmitState(&state);
    QVERIFY(state.applyStationValue("forwardPowerWatts", 40.0));
    QVERIFY(state.applyStationValue("forwardAdcRaw", 2000));
    PaValuesPage page(&window);
    const QString reason = TransmitState::txReadingNotSentText();
    for (const QString& text : {page.fwdCalibratedTextForTest(), page.fwdRawTextForTest(),
                                page.revPowerTextForTest(), page.swrTextForTest(),
                                page.fwdVoltageTextForTest(), page.revVoltageTextForTest(),
                                page.adcOverloadTextForTest(), page.fwdAdcTextForTest(),
                                page.revAdcTextForTest()}) {
        QCOMPARE(text, QStringLiteral("Unavailable"));
    }
    for (MetricLabel* label : page.findChildren<MetricLabel*>()) {
        if (label->toolTip() == reason) {
            return;  // at least one carries the reason; the loop above checked all nine
        }
    }
    QFAIL("No reading carries the reason.");
}

// The remote dialog's chart draws the Core's own display over the same
// range; closing it unsubscribes and the Core stops reading.
void TstRemoteTxReadings::cfcChartDrawsTheCoresBinsAndStopsWhenClosed()
{
    Session s(m_securityDir.path(), this);
    std::atomic<int> reads{0};
    s.server->setCfcDisplayReaderForTest([&reads](double* bins, int count) {
        ++reads;
        for (int i = 0; i < count; ++i) {
            bins[i] = cfcBin(i);
        }
        return true;
    });
    QVERIFY(s.connect());

    TxApplet applet(&s.window);
    applet.setStationCfcBarChart([&s](bool wanted) { s.client->setCfcCompressionWanted(wanted); });
    connect(s.client.get(), &StationClient::cfcCompressionReceived, &applet,
            [&applet](const QList<double>& bins, qint64) {
                applet.applyStationCfcCompression(bins);
            });

    // Not keyed: the dialog asks, the Core does not read.
    applet.requestOpenCfcDialog();
    TxCfcDialog* dialog = applet.cfcDialog();
    QVERIFY(dialog && dialog->isVisible());
    QVERIFY(!dialog->barChartTimer()->isActive());   // no local TxChannel read
    QTRY_VERIFY(s.client->cfcCompressionWanted());
    QTest::qWait(200);
    QVERIFY(!s.server->cfcCompressionPollingForTest());
    QCOMPARE(reads.load(), 0);

    // Keyed with CFC on: the chart draws the Core's bins.
    s.core->transmitModel().setCfcEnabled(true);
    s.keyCore();
    QTRY_VERIFY(s.core->isTransmitting());
    QTRY_VERIFY(s.server->cfcCompressionPollingForTest());
    QTRY_VERIFY(!dialog->compWidget()->barChartData().isEmpty());
    QCOMPARE(dialog->compWidget()->barChartData(), expectedCfcBars(*dialog->compWidget()));

    // Closed: unsubscribed, and the Core stops reading.
    dialog->hide();
    QVERIFY(!s.client->cfcCompressionWanted());
    QTRY_VERIFY(!s.server->cfcCompressionPollingForTest());
    const int after = reads.load();
    QTest::qWait(200);
    QCOMPARE(reads.load(), after);

    // CFC off while keyed: no reads either.
    applet.requestOpenCfcDialog();
    QTRY_VERIFY(s.server->cfcCompressionPollingForTest());
    s.core->transmitModel().setCfcEnabled(false);
    QVERIFY(!s.server->cfcCompressionPollingForTest());
    s.unkeyCore();
    QTRY_VERIFY(!s.core->isTransmitting());
    dialog->hide();
}

// records.subscribe to the stream, right and wrong (the wrong shapes are
// the stream command's own checks).
void TstRemoteTxReadings::cfcStreamWrongShapesAreRefused()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.server->recordStreamForTest(QString::fromLatin1(TransmitState::kCfcStream)));
    QCOMPARE(s.server->recordStreamForTest(QString::fromLatin1(TransmitState::kCfcStream))
                 ->capacity(),
             1);
    const auto refused = [&s](quint32 id) {
        for (const QByteArray& wire : s.windowEnd->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandId == id) {
                return !message.accepted;
            }
        }
        return false;
    };
    // Right: accepted.
    s.client->setCfcCompressionWanted(true);
    QTRY_COMPARE(s.server->recordStreamForTest(QString::fromLatin1(TransmitState::kCfcStream))
                     ->subscriberCount(),
                 1);
    // Wrong: a backlog that is not a number.
    const quint32 wrong = s.client->invokeCommand(
        "records.subscribe",
        {MirrorUpdate{0, "stream", MirrorWireKind::Utf8,
                      QVariant(QString::fromLatin1(TransmitState::kCfcStream))},
         MirrorUpdate{0, "backlog", MirrorWireKind::Utf8, QVariant(QStringLiteral("one"))}});
    QVERIFY(wrong != 0);
    QTRY_VERIFY(refused(wrong));
    // Wrong: an unsubscribe with a backlog.
    const quint32 extra = s.client->invokeCommand(
        "records.unsubscribe",
        {MirrorUpdate{0, "stream", MirrorWireKind::Utf8,
                      QVariant(QString::fromLatin1(TransmitState::kCfcStream))},
         MirrorUpdate{0, "backlog", MirrorWireKind::Int64, QVariant(qlonglong(1))}});
    QTRY_VERIFY(refused(extra));
    s.client->setCfcCompressionWanted(false);
    QTRY_COMPARE(s.server->recordStreamForTest(QString::fromLatin1(TransmitState::kCfcStream))
                     ->subscriberCount(),
                 0);
}

void TstRemoteTxReadings::cfcChartBelowVersion1SaysWhy()
{
    RadioModel window(RadioModel::Role::Remote);
    TxApplet applet(&window);
    bool asked = false;
    applet.setStationCfcBarChart([&asked](bool wanted) { asked = wanted; });
    applet.setStationCfcBarChartUnavailable(TransmitState::txReadingNotSentText());
    applet.requestOpenCfcDialog();
    TxCfcDialog* dialog = applet.cfcDialog();
    QVERIFY(dialog);
    QVERIFY(dialog->barChartReasonLabel()->isVisibleTo(dialog));
    QCOMPARE(dialog->barChartReasonLabel()->text(), TransmitState::txReadingNotSentText());
    QVERIFY(dialog->compWidget()->barChartData().isEmpty());
    applet.setStationCfcBarChartUnavailable(QString());
    QVERIFY(!dialog->barChartReasonLabel()->isVisibleTo(dialog));
    QVERIFY(asked);
    dialog->hide();
    QVERIFY(!asked);
}

// Keyed with the same forward and reflected readings, a remote window's RF
// Pwr and SWR bars and its container Power and SWR meters show what a local
// window shows; unkeyed, they fall as a local window's do.
void TstRemoteTxReadings::remoteBarsAndMetersMatchALocalWindow()
{
    // Local: this window's own radio.
    RadioModel local;
    local.setBoardForTest(HPSDRHW::Saturn);
    local.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    TxApplet localApplet(&local);
    MeterWidget localMeters;
    auto* localPower = new TextItem(&localMeters);
    auto* localSwr = new TextItem(&localMeters);
    localPower->setBindingId(MeterBinding::TxPower);
    localSwr->setBindingId(MeterBinding::TxSwr);
    localMeters.addItem(localPower);
    localMeters.addItem(localSwr);
    MeterPoller localPoller;
    localPoller.addTarget(&localMeters);
    localPoller.setRadioStatus(&local.radioStatus());

    // Remote: the Core's txState, as the window's StationClient applies it.
    RadioModel window(RadioModel::Role::Remote);
    window.setBoardForTest(HPSDRHW::Saturn);
    window.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    window.setStationConnectionState(ConnectionState::Connected);
    TxApplet remoteApplet(&window);
    TransmitState state;
    RemoteMeters remote(window, state);
    MeterWidget remoteMeters;
    auto* remotePower = new TextItem(&remoteMeters);
    auto* remoteSwr = new TextItem(&remoteMeters);
    remotePower->setBindingId(MeterBinding::TxPower);
    remoteSwr->setBindingId(MeterBinding::TxSwr);
    remoteMeters.addItem(remotePower);
    remoteMeters.addItem(remoteSwr);
    remote.poller.addTarget(&remoteMeters);

    // The Core: the readings its radio's samples give, keyed.
    RadioModel core;
    core.setBoardForTest(HPSDRHW::Saturn);
    core.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    QVERIFY(state.applyStationValue("keyed", true));
    QVERIFY(window.applyMirroredValue("transmitting", QVariant(true)).isEmpty());
    for (int i = 0; i < 60; ++i) {
        const quint16 fwd = static_cast<quint16>(2400 + (i % 2) * 8);
        const quint16 rev = static_cast<quint16>(260 + (i % 2) * 4);
        core.handlePaTelemetryForTest(fwd, rev, 0, 0, 0, 0);
        local.handlePaTelemetryForTest(fwd, rev, 0, 0, 0, 0);
        QVERIFY(state.applyStationValue("forwardPowerWatts",
                                        core.radioStatus().forwardPowerWatts()));
        QVERIFY(state.applyStationValue("reflectedPowerWatts",
                                        core.radioStatus().reflectedPowerWatts()));
        QVERIFY(state.applyStationValue("swr", core.radioStatus().swrRatio()));
    }
    // One more, steady: the unsmoothed readings match exactly.
    core.handlePaTelemetryForTest(2404, 262, 0, 0, 0, 0);
    local.handlePaTelemetryForTest(2404, 262, 0, 0, 0, 0);
    QVERIFY(state.applyStationValue("forwardPowerWatts", core.radioStatus().forwardPowerWatts()));
    QVERIFY(state.applyStationValue("reflectedPowerWatts",
                                    core.radioStatus().reflectedPowerWatts()));
    QVERIFY(state.applyStationValue("swr", core.radioStatus().swrRatio()));
    QVERIFY(local.radioStatus().forwardPowerWatts() > 1.0);
    QCOMPARE(remotePower->value(), localPower->value());
    QCOMPARE(remoteSwr->value(), localSwr->value());
    QCOMPARE(window.radioStatus().swrRatio(), local.radioStatus().swrRatio());
    // The bars, after their own smoothing and 20 Hz refresh: within 1 % of
    // each other (each window's smoothing runs once per changed reading).
    const double watts = local.radioStatus().forwardPowerWatts();
    QTRY_VERIFY2(localApplet.fwdPowerGauge()->value() > 0.9 * watts
                     && remoteApplet.fwdPowerGauge()->value() > 0.9 * watts,
                 qPrintable(QStringLiteral("%1 / %2 / %3")
                                .arg(remoteApplet.fwdPowerGauge()->value())
                                .arg(localApplet.fwdPowerGauge()->value())
                                .arg(watts)));
    QVERIFY(qAbs(remoteApplet.fwdPowerGauge()->value() - localApplet.fwdPowerGauge()->value())
            < 0.01 * watts);
    QCOMPARE(remoteApplet.swrGauge()->value(), localApplet.swrGauge()->value());
    // The S-meter's Power needle, as a local window's reads the same sample.
    QCOMPARE(remote.smeter.testTxValue(),
             static_cast<float>(local.radioStatus().forwardPowerWatts()));

    // Unkeyed: the Core's readings fall, and the bars snap as a local
    // window's do at its own unkey.
    QVERIFY(state.applyStationValue("forwardPowerWatts", 0.0));
    QVERIFY(state.applyStationValue("reflectedPowerWatts", 0.0));
    QVERIFY(state.applyStationValue("swr", 1.0));
    QVERIFY(state.applyStationValue("keyed", false));
    QVERIFY(window.applyMirroredValue("transmitting", QVariant(false)).isEmpty());
    tick(remote.poller);
    QCOMPARE(remotePower->value(), 0.0);
    QCOMPARE(remoteSwr->value(), 1.0);
    QCOMPARE(remoteApplet.fwdPowerGauge()->value(), 0.0);
    QCOMPARE(remoteApplet.swrGauge()->value(), 1.0);
    QCOMPARE(remote.smeter.testTxValue(), 0.0f);
}

// Each TX mode in a remote window reads the Core's reading as a local window
// reads its own; a mode the Core does not send shows "--" with the reason.
void TstRemoteTxReadings::sMeterTxModesMatchALocalWindow()
{
    SMeterWidget localMeter;
    MeterPoller localPoller;
    localPoller.setSMeter(&localMeter);
    RadioStatus localStatus;
    QObject::connect(&localStatus, &RadioStatus::powerChanged, &localMeter,
                     [&localMeter](double fwd, double, double swr) {
                         localMeter.setTxMeters(static_cast<float>(fwd),
                                                static_cast<float>(swr));
                     });
    localMeter.setTransmitting(true);

    RadioModel window(RadioModel::Role::Remote);
    window.setStationConnectionState(ConnectionState::Connected);
    TransmitState state;
    RemoteMeters remote(window, state);
    QVERIFY(state.applyStationValue("keyed", true));

    // The same raw WDSP readings: the local poller works them; the Core's
    // pump works them the same way (thetisTxReading) before sending.
    const double rawMic = -14.0;
    localPoller.handOutTxReadingForTest(MeterBinding::TxMic, rawMic);
    localPoller.handOutTxReadingForTest(MeterBinding::TxComp, -6.0);
    const double micReading = thetisTxReading(ThetisTxReading::Mic,
                                              [rawMic](TxMeterType) { return rawMic; });
    localStatus.setForwardPower(42.0);
    localStatus.setReflectedPower(3.0);
    QVERIFY(state.applyStationValue("forwardPowerWatts", 42.0));
    QVERIFY(state.applyStationValue("reflectedPowerWatts", 3.0));
    QVERIFY(state.applyStationValue("swr", localStatus.swrRatio()));
    QVERIFY(state.applyStationValue("micLevelDb", micReading));
    // The follow-up to Task 33: the COMP reading, as the Core works it
    // (thetisTxReading Comp, the reading the local poller hands out).
    const double compReading = thetisTxReading(ThetisTxReading::Comp,
                                               [](TxMeterType) { return -6.0; });
    QVERIFY(state.applyStationValue("compressionDb", compReading));
    tick(remote.poller);

    for (const QString& mode : {QStringLiteral("Power"), QStringLiteral("SWR"),
                                QStringLiteral("Level")}) {
        localMeter.setTxMode(mode);
        remote.smeter.setTxMode(mode);
        QCOMPARE(remote.smeter.testTxValue(), localMeter.testTxValue());
        QCOMPARE(remote.smeter.testTxReadout(), localMeter.testTxReadout());
        QVERIFY(remote.smeter.toolTip().isEmpty());
    }
    // Compression: txState's compressionDb, as the local window reads its
    // own COMP reading.
    localMeter.setTxMode(QStringLiteral("Compression"));
    remote.smeter.setTxMode(QStringLiteral("Compression"));
    QVERIFY(localMeter.testTxReadout() != QStringLiteral("--"));
    QCOMPARE(remote.smeter.testTxValue(), localMeter.testTxValue());
    QCOMPARE(remote.smeter.testTxReadout(), localMeter.testTxReadout());
    QVERIFY(remote.smeter.toolTip().isEmpty());
    QVERIFY(remote.smeter.txModeUnavailableReason(SMeterWidget::TxMode::Compression).isEmpty());

    // A Core that sends no transmit state: every TX mode says so.
    remote.unavailable = QStringLiteral("This Core does not send transmit meters.");
    tick(remote.poller);
    for (SMeterWidget::TxMode mode : {SMeterWidget::TxMode::Power, SMeterWidget::TxMode::SWR,
                                      SMeterWidget::TxMode::Level,
                                      SMeterWidget::TxMode::Compression}) {
        QCOMPARE(remote.smeter.txModeUnavailableReason(mode), remote.unavailable);
    }
    remote.smeter.setTxMode(QStringLiteral("Power"));
    QCOMPARE(remote.smeter.testTxReadout(), QStringLiteral("--"));
    // The local widget never has an unavailable mode.
    QVERIFY(localMeter.txModeUnavailableReason(SMeterWidget::TxMode::Compression).isEmpty());
}

// The right-click menu offers every item in a remote window, a TX mode the
// Core does not send included, and peak hold works on the Core's readings.
void TstRemoteTxReadings::sMeterMenuOffersEveryItemInARemoteWindow()
{
    RadioModel window(RadioModel::Role::Remote);
    window.setStationConnectionState(ConnectionState::Connected);
    QVERIFY(window.addSliceWithStationId(0) >= 0);
    window.setActiveSlice(0);
    TransmitState state;
    RemoteMeters remote(window, state);
    tick(remote.poller);
    // Every TX mode has a reading from this Core (Compression included).
    QVERIFY(remote.smeter.txModeUnavailableReason(SMeterWidget::TxMode::Compression).isEmpty());
    remote.smeter.setTxModeUnavailable(SMeterWidget::TxMode::Compression,
                                       MeterPoller::remoteTxMeterNotSentText());

    std::unique_ptr<QMenu> menu(remote.smeter.buildContextMenuForTesting());
    int actions = 0;
    std::function<void(QMenu*)> walk = [&](QMenu* m) {
        for (QAction* a : m->actions()) {
            if (a->isSeparator()) {
                continue;
            }
            QVERIFY2(a->isEnabled() && a->isVisible(), qPrintable(a->text()));
            ++actions;
            if (a->menu()) {
                walk(a->menu());
            }
        }
    };
    walk(menu.get());
    // TX 4 + RX 4 + peak hold (Enabled, Decay with 3, Reset) + face 7, and
    // the four submenu titles.
    QVERIFY2(actions >= 4 + 4 + 1 + 1 + 3 + 1 + 7 + 3, qPrintable(QString::number(actions)));

    // Peak hold on the Core's reading.
    remote.smeter.setRxMode(QStringLiteral("Signal"));
    remote.smeter.setPeakHoldEnabled(true);
    window.sliceById(0)->setSignalPeakDbm(-60);
    tick(remote.poller);
    QCOMPARE(remote.smeter.levelDbm(), -60.0f);
    QCOMPARE(remote.smeter.testPeakLevel(), -60.0f);
    remote.smeter.resetPeak();
    remote.smeter.setPeakHoldEnabled(false);
}

// Max Bin is measured by the window from the slice's own pan: the passband
// peak of that pan's displayed trace, the reading a local window takes
// (SpectrumWidget::peakDbmInSlicePassband).
void TstRemoteTxReadings::maxBinReadsTheSlicesOwnPanAsALocalWindowDoes()
{
    const auto feed = [](SpectrumWidget& widget, qint64 centreHz, int loudBin, float dbm) {
        SpectrumEndpointContext context;
        context.codec = {29, 1, -180, 0, 11, 11, 0};
        context.exactCentreHz = centreHz;
        context.exactSpanHz = 10000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(11, -120);
        frame.traceDbm[loudBin] = dbm;
        frame.waterfallDbm = QVector<float>(11, -110);
        return widget.updateRemoteSpectrum(frame);
    };
    SpectrumWidget panA;
    SpectrumWidget panB;
    QVERIFY(feed(panA, 10000000, 8, -43));
    QVERIFY(feed(panB, 14000000, 8, -71));

    RadioModel window(RadioModel::Role::Remote);
    window.setStationConnectionState(ConnectionState::Connected);
    QVERIFY(window.addSliceWithStationId(4) >= 0);
    SliceModel* slice = window.sliceById(4);
    slice->setPanKey(QStringLiteral("pan-b"));
    slice->setFrequency(14002000.0);
    slice->setFilter(500, 2500);
    window.setActiveSlice(0);

    const auto source = MeterPoller::panMaxBinSource([&](const SliceModel* owner) -> SpectrumWidget* {
        const QString key = owner->panKey();
        if (key == QLatin1String("pan-a")) { return &panA; }
        if (key == QLatin1String("pan-b")) { return &panB; }
        return nullptr;
    });
    // The slice's own pan (B), not the first one.
    QCOMPARE(source(slice), -71.0);
    // The local reading of the same widget, tuned and filtered as the slice.
    panB.setVfoFrequency(slice->frequency());
    panB.setFilterOffset(slice->filterLow(), slice->filterHigh());
    QCOMPARE(source(slice), panB.peakDbmInSlicePassband());

    // A remote window's S-meter in Max Bin reads it.
    MeterPoller poller;
    SMeterWidget meter;
    poller.setSMeter(&meter);
    poller.setRemoteRadioModel(&window, []() { return true; }, source);
    meter.setRxMode(QStringLiteral("Max Bin"));
    tick(poller);
    QCOMPARE(meter.levelDbm(), -71.0f);
    // A pan that is gone: no reading, never 0.
    slice->setPanKey(QStringLiteral("pan-gone"));
    tick(poller);
    QCOMPARE(meter.sUnitsText(), QStringLiteral("--"));
}

void TstRemoteTxReadings::cfcBinsRoundTripToATenth()
{
    std::array<double, TxChannel::kCfcDisplayBinCount> bins{};
    for (int i = 0; i < TxChannel::kCfcDisplayBinCount; ++i) {
        bins[static_cast<std::size_t>(i)] = (i - 500) * 0.0137;
    }
    const QString text = TransmitState::encodeCfcBins(bins.data(), TxChannel::kCfcDisplayBinCount);
    const QList<double> back = TransmitState::decodeCfcBins(text);
    QCOMPARE(back.size(), TxChannel::kCfcDisplayBinCount);
    for (int i = 0; i < TxChannel::kCfcDisplayBinCount; ++i) {
        QVERIFY(qAbs(back.at(i) - bins[static_cast<std::size_t>(i)]) <= 0.05 + 1e-9);
    }
    QVERIFY(TransmitState::decodeCfcBins(QStringLiteral("not base64!")).isEmpty());
    QVERIFY(TransmitState::decodeCfcBins(QStringLiteral("AA==")).isEmpty());  // one byte: no int16
}

// The pump's COMP reading is Thetis's: max(-30, TXA_COMP_AV); with no
// transmit channel it is the no-reading value.
void TstRemoteTxReadings::pumpWorksTheCompressionReadingAsThetis()
{
    RadioStatus status;
    const TxMeterReadings none = TxMeterPump::read(status, nullptr);
    QCOMPARE(none.compressionDb, TxMeterReadings::kNoReadingDb);
    // The pump works it with the local poller's function: max(-30, raw).
    QCOMPARE(thetisTxReading(ThetisTxReading::Comp, [](TxMeterType) { return -400.0; }), -30.0);
    QCOMPARE(MeterPoller::compressionReading(-6.0),
             thetisTxReading(ThetisTxReading::Comp, [](TxMeterType) { return -6.0; }));
    TransmitState state;
    QVERIFY(state.applyStationValue("compressionDb", -12.5));
    QCOMPARE(state.property("compressionDb").toDouble(), -12.5);
    state.clearStationValues();
    QCOMPARE(state.property("compressionDb").toDouble(), TxMeterReadings::kNoReadingDb);
    // A window's transmit meters: only the eight readings txState does not
    // carry stay disabled, and Compression is no longer one of them.
    QVERIFY(!MeterPoller::remoteTxBindingsNotSent().contains(MeterBinding::TxComp));
}

namespace {

// The seven stage readings, each binding with its `txState` name.
struct StageReading {
    int bindingId;
    const char* name;
    double TxMeterReadings::*field;
};
const StageReading kStageReadings[] = {
    { MeterBinding::TxEq,          "eqDb",          &TxMeterReadings::eqDb },
    { MeterBinding::TxLeveler,     "levelerDb",     &TxMeterReadings::levelerDb },
    { MeterBinding::TxLevelerGain, "levelerGainDb", &TxMeterReadings::levelerGainDb },
    { MeterBinding::TxCfc,         "cfcDb",         &TxMeterReadings::cfcDb },
    { MeterBinding::TxCfcGain,     "cfcGainDb",     &TxMeterReadings::cfcGainDb },
    { MeterBinding::TxAlcGain,     "alcGainDb",     &TxMeterReadings::alcGainDb },
    { MeterBinding::TxAlcGroup,    "alcGroupDb",    &TxMeterReadings::alcGroupDb },
};

// A distinct GetTXAMeter reading for each WDSP meter, inside every floor.
double rawStageMeter(TxMeterType meter)
{
    return -1.25 - 0.75 * static_cast<int>(meter);
}

} // namespace

// A9: the Core works each stage reading exactly as a local window's poll
// works its own transmit channel's (MeterPoller::txReadingForBinding).
void TstRemoteTxReadings::pumpWorksTheStageReadingsAsALocalWindow()
{
    RadioStatus status;
    const TxMeterReadings none = TxMeterPump::read(status, nullptr);
    for (const StageReading& stage : kStageReadings) {
        QCOMPARE(none.*stage.field, TxMeterReadings::kNoReadingDb);
    }
    const std::function<double(TxMeterType)> readRaw = rawStageMeter;
    const TxMeterReadings readings = TxMeterPump::readFrom(status, readRaw);
    for (const StageReading& stage : kStageReadings) {
        QCOMPARE(readings.*stage.field, MeterPoller::txReadingForBinding(stage.bindingId, readRaw));
        QVERIFY2(readings.*stage.field != TxMeterReadings::kNoReadingDb, stage.name);
    }
    // The ones already sent read the same through readFrom.
    QCOMPARE(readings.alcDb, MeterPoller::txReadingForBinding(MeterBinding::TxAlc, readRaw));
    QCOMPARE(readings.micLevelDb, MeterPoller::txReadingForBinding(MeterBinding::TxMic, readRaw));
    QCOMPARE(readings.compressionDb,
             MeterPoller::txReadingForBinding(MeterBinding::TxComp, readRaw));

    // A window's copy takes each by name and drops it with the session.
    TransmitState state;
    QSignalSpy meters(&state, &TransmitState::metersChanged);
    double value = -2.0;
    for (const StageReading& stage : kStageReadings) {
        QVERIFY(state.applyStationValue(stage.name, value));
        QCOMPARE(state.property(stage.name).toDouble(), value);
        QCOMPARE(state.meters().*stage.field, value);
        value -= 1.5;
    }
    QCOMPARE(meters.size(), static_cast<int>(std::size(kStageReadings)));
    state.clearStationValues();
    for (const StageReading& stage : kStageReadings) {
        QCOMPARE(state.property(stage.name).toDouble(), TxMeterReadings::kNoReadingDb);
    }
}

// A9: a remote window's seven container meters show what a local window's
// show for the same transmit channel readings, and a Core below
// txReadingsVersion 3 leaves each disabled with the reason, never hidden.
void TstRemoteTxReadings::remoteStageMetersMatchALocalWindow()
{
    // Local: this window's own transmit channel readings.
    MeterWidget localMeters;
    QHash<int, TextItem*> localItems;
    MeterPoller localPoller;
    // Remote: the Core's txState, as the window's StationClient applies it.
    RadioModel window(RadioModel::Role::Remote);
    window.setStationConnectionState(ConnectionState::Connected);
    QVERIFY(window.addSliceWithStationId(0) >= 0);
    window.setActiveSlice(0);
    TransmitState state;
    RemoteMeters remote(window, state);
    MeterWidget remoteMeters;
    remoteMeters.resize(200, 200);
    QHash<int, TextItem*> remoteItems;
    for (const StageReading& stage : kStageReadings) {
        auto* localItem = new TextItem(&localMeters);
        localItem->setBindingId(stage.bindingId);
        localMeters.addItem(localItem);
        localItems.insert(stage.bindingId, localItem);
        auto* remoteItem = new TextItem(&remoteMeters);
        remoteItem->setBindingId(stage.bindingId);
        remoteMeters.addItem(remoteItem);
        remoteItems.insert(stage.bindingId, remoteItem);
    }
    localPoller.addTarget(&localMeters);
    remote.poller.addTarget(&remoteMeters);

    // A Core at txReadingsVersion 2: each is disabled with the reason.
    bool stages = false;
    remote.poller.setRemoteTxStageReadingsAvailable([&stages]() { return stages; });
    tick(remote.poller);
    for (const StageReading& stage : kStageReadings) {
        QCOMPARE(remoteMeters.bindingUnavailableReason(stage.bindingId),
                 TransmitState::txReadingNotSentText());
    }
    QCOMPARE(remoteMeters.items().size(), static_cast<int>(std::size(kStageReadings)));

    // A Core at 3: available, and keyed they read the Core's values.
    stages = true;
    tick(remote.poller);
    for (const StageReading& stage : kStageReadings) {
        QVERIFY2(remoteMeters.bindingUnavailableReason(stage.bindingId).isEmpty(), stage.name);
    }
    // One GetTXAMeter reading for every WDSP meter, as the local poll's
    // test hand-out takes it (handOutTxReadingForTest).
    constexpr double kRaw = -7.5;
    const TxMeterReadings core = TxMeterPump::readFrom(
        RadioStatus{}, [](TxMeterType) { return kRaw; });
    QVERIFY(state.applyStationValue("keyed", true));
    for (const StageReading& stage : kStageReadings) {
        QVERIFY(state.applyStationValue(stage.name, core.*stage.field));
        localPoller.handOutTxReadingForTest(stage.bindingId, kRaw);
    }
    tick(remote.poller);
    for (const StageReading& stage : kStageReadings) {
        QVERIFY2(localItems.value(stage.bindingId)->value() != TxMeterReadings::kNoReadingDb,
                 stage.name);
        QVERIFY2(remoteItems.value(stage.bindingId)->value()
                     == localItems.value(stage.bindingId)->value(),
                 qPrintable(QStringLiteral("%1: %2 / %3")
                                .arg(QLatin1String(stage.name))
                                .arg(remoteItems.value(stage.bindingId)->value())
                                .arg(localItems.value(stage.bindingId)->value())));
    }
}

// A9: over the link, keyed, the window's copy follows the Core's stage
// readings as its pump reads them.
void TstRemoteTxReadings::coreSendsItsStageReadingsKeyed()
{
    Session s(m_securityDir.path(), this);
    const std::function<double(TxMeterType)> readRaw = rawStageMeter;
    const TxMeterReadings expected = TxMeterPump::readFrom(RadioStatus{}, readRaw);
    s.server->transmitState()->meterPump()->setSource([expected]() { return expected; });
    QVERIFY(s.connect());
    QVERIFY(s.client->txStageReadingsAvailable());
    TransmitState* windowTx = s.client->transmitState();
    s.keyCore();
    QTRY_VERIFY(s.core->isTransmitting());
    for (const StageReading& stage : kStageReadings) {
        QTRY_COMPARE(windowTx->property(stage.name).toDouble(), expected.*stage.field);
        QCOMPARE(s.server->transmitState()->property(stage.name).toDouble(),
                 expected.*stage.field);
    }
    s.unkeyCore();
    QTRY_VERIFY(!s.core->isTransmitting());
}

void TstRemoteTxReadings::newWordingIsPlain()
{
    QVERIFY(OperatorWording::isPlain(TransmitState::txReadingNotSentText()));
    QVERIFY(OperatorWording::isPlain(MeterPoller::remoteTxMeterNotSentText()));
}

QTEST_MAIN(TstRemoteTxReadings)
#include "tst_remote_tx_readings.moc"
