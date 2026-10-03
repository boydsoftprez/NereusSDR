// =================================================================
// tests/tst_remote_tx_display_settings.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// Remote-window parity Task 30 (A12, R-R3-49, R-R3-21, R-R3-10): Setup >
// Display > TX Display from a remote window. Thetis's TX Display handlers
// set the transmit analyzer at once, keyed or not (setup.cs:18146-18210
// [v2.10.3.15]); so does the Core, for a window's write of one of the nine
// analyzer keys, at txDisplayVersion 2.
//
//   1. The Core applies each of the nine written or removed by a window to
//      its own TxAnalyzer at once (read from the analyzer's getters and
//      analyzerConfigCount(), not the settings file), on a receive-only
//      Core too, and a second window sees the new value.
//   2. On the air the change is applied and nothing is refused; the radio
//      stays as it was.
//   3. In a remote window the nine controls show the Core's keys and write
//      them; below version 2 they are disabled with the reason, while the
//      window's own waterfall controls stay live.
//   4. A local window drives its own analyzer as before.
//
// Loopback and a test TxChannel only: no RF, no audio device.
//
// Modification history (NereusSDR):
//   2026-09-27 : Created for parity Task 30 by J.J. Boyd (KG4VCF).
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>

#include <functional>
#include <memory>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/setup/DisplaySetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOlderCore = QStringLiteral(
    "This Core does not apply transmit display settings from this app. "
    "Updating the Core may help.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:30");
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// One remote window: its model, settings proxy and client, over a loopback
// pair to the Core.
struct Window {
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&model, &proxy};
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

// A Core whose settings store is the process's (the store its TxAnalyzer
// reads, as nereusd's is), with a TX analyzer, media on and a test
// TxChannel for keying; two windows.
struct Session {
    Session(const QString& securityDir, QObject* parent, bool receiveOnly)
        : txChannel(/*channelId=*/1)
    {
        core = makeStationRadioModel();
        core->setReceiveOnlyStationPolicy(receiveOnly);
        core->wireTransmitChainForTest(&txChannel);
        analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId);
        core->setTxAnalyzer(analyzer.get());
        // A Core that has keyed once: the analyzer's deferred first
        // configuration is behind it, so every change reaches WDSP at once
        // (before that, the FFT size and window wait for the first key-up,
        // on either path).
        analyzer->start();
        server = std::make_unique<StationServer>(
            core.get(), AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        server->setMediaEnabled(true);
        for (Window* w : {&first, &second}) {
            w->coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
            w->windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
            w->coreEnd->linkTo(w->windowEnd);
        }
    }
    ~Session()
    {
        first.client.disconnectFromStation(QStringLiteral("test complete"));
        second.client.disconnectFromStation(QStringLiteral("test complete"));
        server.reset();
        analyzer->stop();
        core->setTxAnalyzer(nullptr);
        core->injectTxChannelForTest(nullptr);
    }
    bool connect(Window& w)
    {
        QSignalSpy completed(&w.client, &StationClient::handshakeComplete);
        w.client.startSession(w.windowEnd, server->token());
        server->acceptTransport(w.coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    void write(const char* key, const QString& value)
    {
        first.windowEnd->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            QLatin1String(key), value, QStringLiteral("window"))));
    }
    void remove(const char* key)
    {
        first.windowEnd->sendText(SessionMessages::encode(
            SessionMessages::settingsRemove(QLatin1String(key))));
    }
    void key(bool on)
    {
        MoxController* const mox = core->moxController();
        // Lifting the receive-only pre-check stands in for a Core that can
        // transmit; the keying goes through the same path.
        mox->setMoxCheck({});
        mox->setMox(on);
    }

    TxChannel txChannel;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<TxAnalyzer> analyzer;
    std::unique_ptr<StationServer> server;
    Window first;
    Window second;
};

bool anyRefusal(LoopbackTransport* windowEnd)
{
    for (const QByteArray& wire : windowEnd->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::SettingsReject) {
            return true;
        }
    }
    return false;
}

// The nine, each with a value off its default and the analyzer's reading.
struct Case {
    const char* key;
    QString value;
    std::function<QString(const TxAnalyzer&)> read;
};

QList<Case> nineChanges()
{
    const auto num = [](int v) { return QString::number(v); };
    return {
        {TxAnalyzer::kFftSizeKey, QStringLiteral("16384"),
         [num](const TxAnalyzer& a) { return num(a.fftSize()); }},
        {TxAnalyzer::kWindowTypeKey, QStringLiteral("6"),
         [num](const TxAnalyzer& a) { return num(a.windowType()); }},
        {TxAnalyzer::kPanDetectorKey, QStringLiteral("2"),
         [num](const TxAnalyzer& a) { return num(a.panDetector()); }},
        {TxAnalyzer::kPanAveragingKey, QStringLiteral("3"),
         [num](const TxAnalyzer& a) { return num(a.panAveraging()); }},
        {TxAnalyzer::kPanAvTimeMsKey, QStringLiteral("250"),
         [num](const TxAnalyzer& a) { return num(a.panAvTimeMs()); }},
        {TxAnalyzer::kPanNormalizeKey, QStringLiteral("True"),
         [](const TxAnalyzer& a) {
             return a.panNormalize() ? QStringLiteral("True") : QStringLiteral("False");
         }},
        {TxAnalyzer::kWfDetectorKey, QStringLiteral("3"),
         [num](const TxAnalyzer& a) { return num(a.wfDetector()); }},
        {TxAnalyzer::kWfAveragingKey, QStringLiteral("1"),
         [num](const TxAnalyzer& a) { return num(a.wfAveraging()); }},
        {TxAnalyzer::kWfAvTimeMsKey, QStringLiteral("700"),
         [num](const TxAnalyzer& a) { return num(a.wfAvTimeMs()); }},
    };
}

void clearNine()
{
    for (const Case& c : nineChanges()) {
        AppSettings::instance().remove(QLatin1String(c.key));
    }
}

} // namespace

class TstRemoteTxDisplaySettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void coreAppliesEachWindowWriteAtOnce_data();
    void coreAppliesEachWindowWriteAtOnce();
    void keyedChangeIsAppliedAndNothingIsRefused();
    void removalReturnsTheDefault();
    void valueTheAnalyzerDoesNotTakeComesBackAsItsOwn();
    void otherKeysLeaveTheAnalyzerAlone();
    void remotePageShowsAndWritesTheCoresKeys();
    void olderCoreDisablesTheNineOnly();
    void localPageDrivesItsOwnAnalyzer();
    void hostingWindowsPageFollowsARemoteChange();
    void reasonIsPlain();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteTxDisplaySettings::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    AppSettings::setProfileOverride(
        QStringLiteral("remote-tx-display-settings-%1").arg(QCoreApplication::applicationPid()));
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                     QStringLiteral("7"));
    // No radio is attached: the model's own not-connected notes, and the
    // bare test window's notes about its missing catalogue, are the
    // harness's.
    QLoggingCategory::setFilterRules(QStringLiteral(
        "nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false\n"
        "nereus.connection.warning=false\nnereus.stationclient.warning=false\n"
        // The harness's TxChannel is a test channel with no WDSP channel.
        "nereus.dsp.warning=false\nqt.qpa.fonts.warning=false"));
}

void TstRemoteTxDisplaySettings::cleanupTestCase()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteTxDisplaySettings::init()
{
    clearNine();
}

void TstRemoteTxDisplaySettings::cleanup()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    clearNine();
}

void TstRemoteTxDisplaySettings::coreAppliesEachWindowWriteAtOnce_data()
{
    QTest::addColumn<bool>("receiveOnly");
    QTest::newRow("transmitting Core") << false;
    QTest::newRow("receive-only Core") << true;
}

// A12 / row 23: each of the nine from a window reaches the Core's analyzer
// at once, on a receive-only Core too, and the second window sees it.
void TstRemoteTxDisplaySettings::coreAppliesEachWindowWriteAtOnce()
{
    QFETCH(bool, receiveOnly);
    Session s(m_securityDir.path(), this, receiveOnly);
    QVERIFY(s.connect(s.first));
    QVERIFY(s.connect(s.second));
    // Parity Task 31 raised it to 3; the analyzer keys came with 2.
    QCOMPARE(s.server->txDisplayVersion(), 3);
    QTRY_COMPARE(s.first.client.capabilities().txDisplayVersion, 3);
    QTRY_COMPARE(s.first.model.stationTxDisplayVersion(), 3);
    QSignalSpy secondSaw(&s.second.model, &RadioModel::stationSettingChanged);

    for (const Case& c : nineChanges()) {
        const int before = s.analyzer->analyzerConfigCount();
        QVERIFY2(c.read(*s.analyzer) != c.value, c.key);
        s.write(c.key, c.value);
        QTRY_COMPARE(c.read(*s.analyzer), c.value);
        QVERIFY2(s.analyzer->analyzerConfigCount() > before, c.key);
        QTRY_COMPARE(s.second.proxy.value(QLatin1String(c.key), QString()).toString(), c.value);
        bool reported = false;
        for (const QList<QVariant>& call : secondSaw) {
            reported = reported || call.at(0).toString() == QLatin1String(c.key);
        }
        QVERIFY2(reported, c.key);
    }
    QVERIFY(!anyRefusal(s.first.windowEnd));
    // One analyzer: the Core's own transmit display runs what the window set.
    QCOMPARE(s.core->txAnalyzer(), s.analyzer.get());
    QCOMPARE(s.analyzer->binWidthHz(), 96000.0 / 16384.0);
}

// Row 6 remotely: a change while the Core is keyed is applied (Thetis's
// handlers have no MOX check), nothing is refused, and the Core stays keyed
// as it was.
void TstRemoteTxDisplaySettings::keyedChangeIsAppliedAndNothingIsRefused()
{
    Session s(m_securityDir.path(), this, /*receiveOnly=*/true);
    QVERIFY(s.connect(s.first));
    s.key(true);
    QTRY_VERIFY(s.core->moxController()->state() != MoxState::Rx);
    QString onAir;
    QVERIFY(s.core->stationOnAirRefusal(&onAir));
    for (const Case& c : nineChanges()) {
        const int before = s.analyzer->analyzerConfigCount();
        s.write(c.key, c.value);
        QTRY_COMPARE(c.read(*s.analyzer), c.value);
        QVERIFY2(s.analyzer->analyzerConfigCount() > before, c.key);
    }
    QVERIFY(!anyRefusal(s.first.windowEnd));
    QVERIFY(s.core->moxController()->state() != MoxState::Rx);
    s.key(false);
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
}

void TstRemoteTxDisplaySettings::removalReturnsTheDefault()
{
    Session s(m_securityDir.path(), this, /*receiveOnly=*/false);
    QVERIFY(s.connect(s.first));
    s.write(TxAnalyzer::kPanAvTimeMsKey, QStringLiteral("400"));
    s.write(TxAnalyzer::kFftSizeKey, QStringLiteral("8192"));
    QTRY_COMPARE(s.analyzer->panAvTimeMs(), 400);
    QTRY_COMPARE(s.analyzer->fftSize(), 8192);
    s.remove(TxAnalyzer::kPanAvTimeMsKey);
    s.remove(TxAnalyzer::kFftSizeKey);
    QTRY_COMPARE(s.analyzer->panAvTimeMs(), TxAnalyzer::kDefaultPanAvTimeMs);
    QTRY_COMPARE(s.analyzer->fftSize(), TxAnalyzer::kDefaultFftSize);
    QVERIFY(!AppSettings::instance().contains(QLatin1String(TxAnalyzer::kPanAvTimeMsKey)));
}

// A value the page's control could not have written applies what the
// setter makes of it, and every window is told that value.
void TstRemoteTxDisplaySettings::valueTheAnalyzerDoesNotTakeComesBackAsItsOwn()
{
    Session s(m_securityDir.path(), this, /*receiveOnly=*/false);
    QVERIFY(s.connect(s.first));
    QVERIFY(s.connect(s.second));
    s.write(TxAnalyzer::kFftSizeKey, QStringLiteral("5000"));
    QTRY_COMPARE(s.analyzer->fftSize(), 8192);
    QTRY_COMPARE(s.second.proxy.value(QLatin1String(TxAnalyzer::kFftSizeKey), QString())
                     .toString(),
                 QStringLiteral("8192"));
    s.write(TxAnalyzer::kPanAvTimeMsKey, QStringLiteral("0"));
    QTRY_COMPARE(s.analyzer->panAvTimeMs(), 1);
    QTRY_COMPARE(AppSettings::instance().value(QLatin1String(TxAnalyzer::kPanAvTimeMsKey))
                     .toString(),
                 QStringLiteral("1"));
    // The other eight are not written back with it.
    QVERIFY(!AppSettings::instance().contains(QLatin1String(TxAnalyzer::kWfAvTimeMsKey)));
}

void TstRemoteTxDisplaySettings::otherKeysLeaveTheAnalyzerAlone()
{
    RadioModel model;
    TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
    model.setTxAnalyzer(&analyzer);
    const int before = analyzer.analyzerConfigCount();
    AppSettings::instance().setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("65536"));
    model.applyRemoteTxDisplaySetting(QStringLiteral("DisplayFftSize"));
    model.applyRemoteTxDisplaySetting(QStringLiteral("DisplayTxWfGradient"));
    QCOMPARE(analyzer.analyzerConfigCount(), before);
    QCOMPARE(analyzer.fftSize(), TxAnalyzer::kDefaultFftSize);
    AppSettings::instance().remove(QStringLiteral("DisplayFftSize"));
    model.setTxAnalyzer(nullptr);
}

// A remote window's page shows the Core's nine and writes them; another
// window's change and a refusal bring it back to the Core's value; the
// bin width is the Core's FFT size at 96000 Hz.
void TstRemoteTxDisplaySettings::remotePageShowsAndWritesTheCoresKeys()
{
    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    QMap<QString, QString> snapshot;
    snapshot.insert(QLatin1String(TxAnalyzer::kFftSizeKey), QStringLiteral("65536"));
    snapshot.insert(QLatin1String(TxAnalyzer::kWindowTypeKey), QStringLiteral("2"));
    snapshot.insert(QLatin1String(TxAnalyzer::kPanDetectorKey), QStringLiteral("1"));
    snapshot.insert(QLatin1String(TxAnalyzer::kPanAveragingKey), QStringLiteral("2"));
    snapshot.insert(QLatin1String(TxAnalyzer::kPanAvTimeMsKey), QStringLiteral("45"));
    snapshot.insert(QLatin1String(TxAnalyzer::kPanNormalizeKey), QStringLiteral("False"));
    snapshot.insert(QLatin1String(TxAnalyzer::kWfDetectorKey), QStringLiteral("2"));
    snapshot.insert(QLatin1String(TxAnalyzer::kWfAveragingKey), QStringLiteral("3"));
    snapshot.insert(QLatin1String(TxAnalyzer::kWfAvTimeMsKey), QStringLiteral("333"));
    proxy.applySnapshot(snapshot);
    proxy.setReady(true);
    AppSettings::instance().setRemoteBackend(&proxy);
    window.setStationTxDisplayVersion(2);

    TxDisplayPage page(&window);
    page.setStationSettingsAvailable(true, QString());
    const QList<QWidget*> nine = page.txAnalyzerControlsForTest();
    QCOMPARE(nine.size(), 9);
    auto* fft = qobject_cast<QSlider*>(nine[0]);
    auto* win = qobject_cast<QComboBox*>(nine[1]);
    auto* panDet = qobject_cast<QComboBox*>(nine[2]);
    auto* panAvg = qobject_cast<QComboBox*>(nine[3]);
    auto* panTime = qobject_cast<QSpinBox*>(nine[4]);
    auto* norm = qobject_cast<QCheckBox*>(nine[5]);
    auto* wfDet = qobject_cast<QComboBox*>(nine[6]);
    auto* wfAvg = qobject_cast<QComboBox*>(nine[7]);
    auto* wfTime = qobject_cast<QSpinBox*>(nine[8]);
    QVERIFY(fft && win && panDet && panAvg && panTime && norm && wfDet && wfAvg && wfTime);

    // The Core's values.
    QCOMPARE(fft->value(), 4);
    QCOMPARE(page.txFftSizeReadoutForTest()->text(), QStringLiteral("65536"));
    QCOMPARE(page.txBinWidthReadoutForTest()->text(),
             QString::number(96000.0 / 65536.0, 'f', 3));
    QCOMPARE(win->currentIndex(), 2);
    QCOMPARE(panDet->currentIndex(), 1);
    QCOMPARE(panAvg->currentIndex(), 2);
    QCOMPARE(panTime->value(), 45);
    QVERIFY(!norm->isChecked());
    QCOMPARE(wfDet->currentIndex(), 2);
    QCOMPARE(wfAvg->currentIndex(), 3);
    QCOMPARE(wfTime->value(), 333);
    for (QWidget* control : nine) {
        if (control != norm) {
            QVERIFY(control->isEnabled());
        }
    }
    // Normalize follows the pan detector (Peak and Rosenfell: off).
    QVERIFY(!norm->isEnabled());

    // Each change writes the Core's key.
    QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
    const auto lastWrite = [&writes](const char* key) {
        QString value;
        for (const QList<QVariant>& call : writes) {
            if (call.at(0).toString() == QLatin1String(key)) {
                value = call.at(1).toString();
            }
        }
        return value;
    };
    fft->setValue(1);
    QCOMPARE(lastWrite(TxAnalyzer::kFftSizeKey), QStringLiteral("8192"));
    QCOMPARE(page.txFftSizeReadoutForTest()->text(), QStringLiteral("8192"));
    QCOMPARE(page.txBinWidthReadoutForTest()->text(), QString::number(96000.0 / 8192.0, 'f', 3));
    win->setCurrentIndex(5);
    QCOMPARE(lastWrite(TxAnalyzer::kWindowTypeKey), QStringLiteral("5"));
    panDet->setCurrentIndex(4);
    QCOMPARE(lastWrite(TxAnalyzer::kPanDetectorKey), QStringLiteral("4"));
    QVERIFY(norm->isEnabled());
    panAvg->setCurrentIndex(1);
    QCOMPARE(lastWrite(TxAnalyzer::kPanAveragingKey), QStringLiteral("1"));
    panTime->setValue(99);
    QCOMPARE(lastWrite(TxAnalyzer::kPanAvTimeMsKey), QStringLiteral("99"));
    norm->setChecked(true);
    QCOMPARE(lastWrite(TxAnalyzer::kPanNormalizeKey), QStringLiteral("True"));
    wfDet->setCurrentIndex(0);
    QCOMPARE(lastWrite(TxAnalyzer::kWfDetectorKey), QStringLiteral("0"));
    wfAvg->setCurrentIndex(0);
    QCOMPARE(lastWrite(TxAnalyzer::kWfAveragingKey), QStringLiteral("0"));
    wfTime->setValue(1234);
    QCOMPARE(lastWrite(TxAnalyzer::kWfAvTimeMsKey), QStringLiteral("1234"));
    QCOMPARE(writes.count(), 9);

    // Another window's change shows here, without writing it back.
    writes.clear();
    proxy.applyRemoteValue(QLatin1String(TxAnalyzer::kWfAvTimeMsKey), QStringLiteral("500"),
                           QString());
    window.reportStationSettingChanged(QLatin1String(TxAnalyzer::kWfAvTimeMsKey));
    QCOMPARE(wfTime->value(), 500);
    proxy.applyRemoteValue(QLatin1String(TxAnalyzer::kFftSizeKey), QStringLiteral("262144"),
                           QString());
    window.reportStationSettingChanged(QLatin1String(TxAnalyzer::kFftSizeKey));
    QCOMPARE(fft->value(), 6);
    QCOMPARE(page.txFftSizeReadoutForTest()->text(), QStringLiteral("262144"));
    QCOMPARE(writes.count(), 0);

    // A refusal puts the Core's value back.
    proxy.applyRejection(QLatin1String(TxAnalyzer::kWindowTypeKey), QStringLiteral("3"));
    QCOMPARE(win->currentIndex(), 3);
    QCOMPARE(writes.count(), 0);

    // Without the Core's settings the nine wait with the dialog's reason.
    page.setStationSettingsAvailable(false, QStringLiteral("Connect to the Core to change these."));
    for (QWidget* control : nine) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), QStringLiteral("Connect to the Core to change these."));
    }
    page.setStationSettingsAvailable(true, QString());
    QVERIFY(fft->isEnabled() && norm->isEnabled());
    AppSettings::instance().setRemoteBackend(nullptr);
}

// Below txDisplayVersion 2 the nine are disabled with the reason; the
// window's own waterfall controls work; version 2 arriving opens them.
void TstRemoteTxDisplaySettings::olderCoreDisablesTheNineOnly()
{
    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    proxy.applySnapshot({{QLatin1String(TxAnalyzer::kPanDetectorKey), QStringLiteral("3")}});
    proxy.setReady(true);
    AppSettings::instance().setRemoteBackend(&proxy);
    window.setStationTxDisplayVersion(1);

    TxDisplayPage page(&window);
    page.setStationSettingsAvailable(true, QString());
    const QList<QWidget*> nine = page.txAnalyzerControlsForTest();
    for (QWidget* control : nine) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), kOlderCore);
        QCOMPARE(control->accessibleDescription(), kOlderCore);
    }
    QVERIFY(page.txWfLowLevelForTest()->isEnabled());
    QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
    page.txWfLowLevelForTest()->setValue(-40);
    for (const QList<QVariant>& call : writes) {
        QVERIFY(!TxAnalyzer::isSettingsKey(call.at(0).toString()));
    }

    // The Core updated: the nine open, Normalize by its detector (Sample).
    window.setStationTxDisplayVersion(2);
    for (QWidget* control : nine) {
        QVERIFY(control->isEnabled());
        QVERIFY(control->toolTip() != kOlderCore);
    }
    window.setStationTxDisplayVersion(0);
    QVERIFY(!nine[0]->isEnabled());
    QCOMPARE(nine[0]->toolTip(), kOlderCore);
    AppSettings::instance().setRemoteBackend(nullptr);
}

// R-R3-10: a local window drives its own analyzer exactly as before, with
// no version to wait for.
void TstRemoteTxDisplaySettings::localPageDrivesItsOwnAnalyzer()
{
    RadioModel model;
    TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
    model.setTxAnalyzer(&analyzer);
    {
        TxDisplayPage page(&model);
        page.setStationSettingsAvailable(true, QString());
        const QList<QWidget*> nine = page.txAnalyzerControlsForTest();
        auto* fft = qobject_cast<QSlider*>(nine[0]);
        auto* panDet = qobject_cast<QComboBox*>(nine[2]);
        auto* wfTime = qobject_cast<QSpinBox*>(nine[8]);
        QVERIFY(fft && panDet && wfTime);
        QVERIFY(fft->isEnabled() && panDet->isEnabled() && wfTime->isEnabled());
        QCOMPARE(fft->value(), 3);
        fft->setValue(5);
        QCOMPARE(analyzer.fftSize(), 131072);
        panDet->setCurrentIndex(2);
        QCOMPARE(analyzer.panDetector(), 2);
        QVERIFY(nine[5]->isEnabled());
        wfTime->setValue(640);
        QCOMPARE(analyzer.wfAvTimeMs(), 640);
    }
    model.setTxAnalyzer(nullptr);
}

// Parity both ways: a desktop window hosting the Core shows a remote
// window's change on its own open TX Display page, and showing it writes
// nothing back to the analyzer.
void TstRemoteTxDisplaySettings::hostingWindowsPageFollowsARemoteChange()
{
    RadioModel model;
    TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
    analyzer.start();
    model.setTxAnalyzer(&analyzer);
    {
        TxDisplayPage page(&model);
        page.setStationSettingsAvailable(true, QString());
        const QList<QWidget*> nine = page.txAnalyzerControlsForTest();
        auto* fft = qobject_cast<QSlider*>(nine[0]);
        auto* win = qobject_cast<QComboBox*>(nine[1]);
        auto* panDet = qobject_cast<QComboBox*>(nine[2]);
        auto* norm = qobject_cast<QCheckBox*>(nine[5]);
        auto* wfTime = qobject_cast<QSpinBox*>(nine[8]);
        QVERIFY(fft && win && panDet && norm && wfTime);
        QCOMPARE(fft->value(), 3);
        QVERIFY(!norm->isEnabled());

        // What StationServer does for a window's accepted write.
        const auto remoteWrite = [&model](const char* key, const QString& value) {
            AppSettings::instance().setValue(QLatin1String(key), value);
            model.applyRemoteTxDisplaySetting(QLatin1String(key));
        };
        remoteWrite(TxAnalyzer::kFftSizeKey, QStringLiteral("8192"));
        remoteWrite(TxAnalyzer::kWindowTypeKey, QStringLiteral("6"));
        remoteWrite(TxAnalyzer::kPanDetectorKey, QStringLiteral("3"));
        remoteWrite(TxAnalyzer::kWfAvTimeMsKey, QStringLiteral("777"));
        const int applied = analyzer.analyzerConfigCount();
        QCOMPARE(fft->value(), 1);
        QCOMPARE(page.txFftSizeReadoutForTest()->text(), QStringLiteral("8192"));
        QCOMPARE(page.txBinWidthReadoutForTest()->text(),
                 QString::number(96000.0 / 8192.0, 'f', 3));
        QCOMPARE(win->currentIndex(), 6);
        QCOMPARE(panDet->currentIndex(), 3);
        QVERIFY(norm->isEnabled());
        QCOMPARE(wfTime->value(), 777);
        // Nothing echoed back into the analyzer.
        QCOMPARE(analyzer.analyzerConfigCount(), applied);
        QCOMPARE(analyzer.fftSize(), 8192);

        // The page's own control still drives the analyzer afterwards.
        wfTime->setValue(900);
        QCOMPARE(analyzer.wfAvTimeMs(), 900);
    }
    analyzer.stop();
    model.setTxAnalyzer(nullptr);
}

void TstRemoteTxDisplaySettings::reasonIsPlain()
{
    QCOMPARE(TxDisplayPage::coreDoesNotApplyReason(), kOlderCore);
    QVERIFY2(OperatorWording::isPlain(kOlderCore), qPrintable(kOlderCore));
}

QTEST_MAIN(TstRemoteTxDisplaySettings)
#include "tst_remote_tx_display_settings.moc"
