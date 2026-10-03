// no-port-check: NereusSDR-original. R-R3-46 / R-R3-49 (remote-window
// parity Task 13): Hardware Config's OC transmit pins, OC pin actions,
// User Dig Out, TX Display Cal and Volts/Amps Calibration from a remote
// window, as from a local one; the Alex-1 TX filter options and the HL2 TX
// timings hidden in both windows with their saved values kept.
//
// The on-air rule follows Thetis for each control (v2.10.3.15): the TX OC
// pin boxes are greyed while MOX is on unless OC hot switching is allowed
// (setup.cs UpdateForHotSwitch; NereusSDR does not build hot switching), so
// the Core refuses them on the air and both windows close them; the pin
// actions, TX Display Cal and Volts/Amps Calibration change while
// transmitting in Thetis, so the Core takes them on the air too.
//
// Loopback link, no RF and no hardware: nothing here keys a radio. "On the
// air" keys the Core's own MoxController against a test TxChannel with the
// receive-only MOX pre-check lifted, as tst_remote_pa_pages does. The P1
// bytes are composed by the real connection class without a socket. No
// audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-49 (parity Task 13).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/TxChannel.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/CalibrationTab.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/setup/hardware/OcOutputsTab.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kTransmitReason = QStringLiteral("Remote transmit is not here yet");
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:13");

QString hw(const QString& rest)
{
    return QStringLiteral("hardware/%1/%2").arg(kMac, rest);
}

std::unique_ptr<RadioModel> makeStationRadioModel(bool hl2)
{
    auto model = std::make_unique<RadioModel>();
    const HPSDRHW board = hl2 ? HPSDRHW::HermesLite : HPSDRHW::Saturn;
    model->setBoardForTest(board);
    model->setHpsdrModelForTest(hl2 ? HPSDRModel::HERMESLITE : HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = kMac;
    info.name = hl2 ? QStringLiteral("Bench HL2") : QStringLiteral("Bench G2");
    info.boardType = board;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A receive-only Core with its transmit chain wired to a test TxChannel and
// one window, handshake complete. `coreUsesProcessSettings`: the Core's
// store is the process-wide AppSettings (so the Core's live OC matrix and
// calibration read what the server stored), or a file of its own for the
// window-page cases (whose window reads through its SettingsProxy).
struct Session {
    Session(const QString& securityDir, QObject* parent, bool coreUsesProcessSettings,
            bool hl2 = false)
        : ownSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , settings(coreUsesProcessSettings ? AppSettings::instance() : ownSettings)
        , txChannel(/*channelId=*/1)
    {
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        core = makeStationRadioModel(hl2);
        core->wireTransmitChainForTest(&txChannel);
        core->ocMatrixMutable().setMacAddress(kMac);
        core->calibrationControllerMutable().setMacAddress(kMac);
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
    void unkeyCore()
    {
        core->moxController()->setMox(false);
    }

    QTemporaryDir settingsDir;
    AppSettings ownSettings;
    AppSettings& settings;
    TxChannel txChannel;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

// MOX, TUNE, the two-tone test and VOX all stay off on the Core.
bool nothingKeyed(const Session& s, QString* what)
{
    const TransmitModel& tx = s.core->transmitModel();
    const auto fail = [what](const char* name) {
        if (what) { *what = QString::fromLatin1(name); }
        return false;
    };
    if (s.core->mox() || tx.isMox()) { return fail("MOX"); }
    if (s.core->tune() || s.core->isTune() || tx.isTune()) { return fail("TUNE"); }
    if (tx.isTwoToneActive()) { return fail("two-tone"); }
    if (tx.voxEnabled()) { return fail("VOX"); }
    if (s.core->moxController()->state() != MoxState::Rx) { return fail("MoxController"); }
    return true;
}

QString settingsRejectReason(LoopbackTransport* windowEnd, const QString& key)
{
    QString reason;
    for (const QByteArray& wire : windowEnd->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::SettingsReject
            && QString::fromUtf8(message.objectKey) == key) {
            reason = message.reason;
        }
    }
    return reason;
}

// The P1 bank-0 C&C bytes an ANAN-100D sends on 20 m with this OC matrix,
// keyed or not (C2 carries the OC byte shifted up one bit).
QByteArray p1Bank0(const OcMatrix& matrix, bool mox)
{
    P1RadioConnection p1;
    p1.setBoardForTest(HPSDRHW::Angelia);
    p1.setOcMatrix(&matrix);
    p1.setReceiverFrequency(0, 14200000);
    p1.setMox(mox);
    return p1.captureBank0ForTest();
}

QGroupBox* groupTitled(QWidget* root, const QString& part)
{
    for (QGroupBox* box : root->findChildren<QGroupBox*>()) {
        if (box->title().contains(part)) { return box; }
    }
    return nullptr;
}

QPushButton* buttonText(QWidget* root, const QString& text)
{
    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button->text() == text) { return button; }
    }
    return nullptr;
}

QCheckBox* checkWithTip(QWidget* root, const QString& tip)
{
    for (QCheckBox* box : root->findChildren<QCheckBox*>()) {
        if (box->toolTip() == tip) { return box; }
    }
    return nullptr;
}

}  // namespace

class TstRemoteOcCal : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void keysAreOnTheListAtVersion8();
    void txPinReachesTheCoresCodecOffTheAir();
    void txPinIsRefusedOnTheAirAndPinActionWaitsForReceive();
    void transmitCalibrationAppliesAtOnceOnAndOffTheAir();
    void n2adrAppliesItsWholePresetOffTheAir();
    void remoteHardwarePageFollowsTheGates();
    void olderCoreKeepsTheControlsClosedWithItsReason();
    void localHardwarePageClosesTxPinsOnTheAirOnly();
    void hiddenAlexAndHl2ControlsKeepTheirSavedValues();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteOcCal::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
    AppSettings::setProfileOverride(QStringLiteral("remote-oc-cal-%1")
                                        .arg(QCoreApplication::applicationPid()));
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemoteOcCal::cleanupTestCase()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteOcCal::cleanup()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
    UnbuiltFeatures::resetForTest();
}

void TstRemoteOcCal::keysAreOnTheListAtVersion8()
{
    // Taken off the air, refused on it: the OC transmit pins.
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(hw(QStringLiteral("oc/tx/20m/pin3"))));
    QVERIFY(!StationServer::isTransmitSettingKeyTakenOnAir(hw(QStringLiteral("oc/tx/20m/pin3"))));
    // Taken on and off the air: the pin actions and the transmit calibration,
    // with the Calibration tab's own copies.
    for (const QString& rest : {QStringLiteral("oc/actions/pin1/action"),
                                QStringLiteral("cal/txDisplayOffset"),
                                QStringLiteral("cal/paSens"),
                                QStringLiteral("cal/paOffset"),
                                QStringLiteral("paCalibration/cal/txDisplayOffset"),
                                QStringLiteral("paCalibration/cal/paSens"),
                                QStringLiteral("paCalibration/cal/paOffset"),
                                QStringLiteral("paCalibration/cal/paDefaultRestored"),
                                QStringLiteral("paCalibration/cal/logVoltsAmps")}) {
        QVERIFY2(StationServer::isTransmitSettingKeyAcceptedOffAir(hw(rest)), qPrintable(rest));
        QVERIFY2(StationServer::isTransmitSettingKeyTakenOnAir(hw(rest)), qPrintable(rest));
    }
    // The PA forward-power table keeps version 6's on-air rule.
    QVERIFY(!StationServer::isTransmitSettingKeyTakenOnAir(hw(QStringLiteral("paCalibration/calPoint1"))));
    // Still refused: the hidden and still-transmit-gated hardware keys.
    for (const QString& rest : {QStringLiteral("hl2/pttHangMs"), QStringLiteral("hl2/txLatencyMs"),
                                QStringLiteral("tx/UserDigOut"), QStringLiteral("alex/master/hpfBypassOnTx"),
                                QStringLiteral("alex/lpf/20m/start"),
                                QStringLiteral("ocOutputs/hardware/oc/extPa/model")}) {
        QVERIFY2(!StationServer::isTransmitSettingKeyAcceptedOffAir(hw(rest)), qPrintable(rest));
    }
    QVERIFY(!StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("hardware/oc/allowHotSwitching")));

    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    QVERIFY(s.connect());
    // 14 since the Core owns Prevent transmitting on a different band, 15
    // since a remote window edits the CFC bands.
    QCOMPARE(s.client->capabilities().transmitSettingsVersion, 15);
    QVERIFY(s.client->transmitSettingsAvailable(8));
    const IStationLink* link = s.window.stationLink();
    QVERIFY(link != nullptr && link->transmitSettingsAvailable(8));
}

// B4.2: a window's TX pin, off the air, reaches the Core's OC matrix, and
// the Core's next P1 frame carries it as a local window's edit would.
void TstRemoteOcCal::txPinReachesTheCoresCodecOffTheAir()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    const OcMatrix& oc = s.core->ocMatrix();
    QVERIFY(!oc.pinEnabled(Band::Band20m, 2, /*tx=*/true));
    const QByteArray before = p1Bank0(oc, /*mox=*/true);

    s.proxy.setValue(hw(QStringLiteral("oc/tx/20m/pin3")), QStringLiteral("True"));
    QTRY_VERIFY(oc.pinEnabled(Band::Band20m, 2, /*tx=*/true));
    QCOMPARE(oc.maskFor(Band::Band20m, /*tx=*/true), quint8(0x04));

    // The same edit in a local window's matrix.
    OcMatrix local;
    local.setPin(Band::Band20m, 2, /*tx=*/true, true);
    const QByteArray keyed = p1Bank0(oc, /*mox=*/true);
    QCOMPARE(keyed, p1Bank0(local, /*mox=*/true));
    QCOMPARE(quint8(keyed.at(2)) & 0xFE, 0x04 << 1);
    QVERIFY(keyed != before);
    // On receive the RX pins go out, which the edit left alone.
    QCOMPARE(quint8(p1Bank0(oc, /*mox=*/false).at(2)) & 0xFE, 0);

    // An SWL TX pin and a whole reset (the tabs' Reset OC defaults).
    s.proxy.setValue(hw(QStringLiteral("oc/tx/%1/pin7").arg(bandKeyName(Band::Band49m))),
                     QStringLiteral("True"));
    QTRY_VERIFY(oc.pinEnabled(Band::Band49m, 6, /*tx=*/true));
    s.proxy.setValue(hw(QStringLiteral("oc/tx/20m/pin3")), QStringLiteral("False"));
    QTRY_VERIFY(!oc.pinEnabled(Band::Band20m, 2, /*tx=*/true));
    QString keyedNow;
    QVERIFY2(nothingKeyed(s, &keyedNow), qPrintable(keyedNow));
}

void TstRemoteOcCal::txPinIsRefusedOnTheAirAndPinActionWaitsForReceive()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    QVERIFY(s.connect());
    const OcMatrix& oc = s.core->ocMatrix();
    const OcMatrix::TXPinAction before = oc.pinAction(0);
    QVERIFY(before != OcMatrix::TXPinAction::Tune);

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    const QString pin = hw(QStringLiteral("oc/tx/40m/pin2"));
    s.proxy.setValue(pin, QStringLiteral("True"));
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, pin), kOnAir);
    QVERIFY(!s.settings.contains(pin));

    // The pin action is taken on the air (Thetis stores it keyed) but
    // reaches the matrix the codec reads once the radio is on receive.
    const QString action = hw(QStringLiteral("oc/actions/pin1/action"));
    s.proxy.setValue(action, QStringLiteral("tune"));
    QTRY_COMPARE(s.settings.value(action).toString(), QStringLiteral("tune"));
    QTest::qWait(150);
    QCOMPARE(oc.pinAction(0), before);
    QVERIFY(!oc.pinEnabled(Band::Band40m, 1, /*tx=*/true));

    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(oc.pinAction(0), OcMatrix::TXPinAction::Tune);
    QVERIFY(!oc.pinEnabled(Band::Band40m, 1, /*tx=*/true));
    QVERIFY(settingsRejectReason(s.windowEnd, action).isEmpty());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
}

// B4.3: TX Display Cal and Volts/Amps Calibration reach the Core's
// calibration at once, on the air too (Thetis applies them keyed).
void TstRemoteOcCal::transmitCalibrationAppliesAtOnceOnAndOffTheAir()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    QVERIFY(s.connect());
    const CalibrationController& cal = s.core->calibrationController();

    s.proxy.setValue(hw(QStringLiteral("cal/txDisplayOffset")), QStringLiteral("2.5"));
    s.proxy.setValue(hw(QStringLiteral("cal/paSens")), QStringLiteral("1.25"));
    s.proxy.setValue(hw(QStringLiteral("cal/paOffset")), QStringLiteral("0.5"));
    s.proxy.setValue(hw(QStringLiteral("paCalibration/cal/paSens")), QStringLiteral("1.25"));
    QTRY_COMPARE(cal.txDisplayOffsetDb(), 2.5);
    QTRY_COMPARE(cal.paCurrentSensitivity(), 1.25);
    QTRY_COMPARE(cal.paCurrentOffset(), 0.5);
    QTRY_COMPARE(s.settings.value(hw(QStringLiteral("paCalibration/cal/paSens"))).toString(),
                 QStringLiteral("1.25"));

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    s.proxy.setValue(hw(QStringLiteral("cal/txDisplayOffset")), QStringLiteral("-1.5"));
    s.proxy.setValue(hw(QStringLiteral("cal/paOffset")), QStringLiteral("0.75"));
    QTRY_COMPARE(cal.txDisplayOffsetDb(), -1.5);
    QTRY_COMPARE(cal.paCurrentOffset(), 0.75);
    QVERIFY(s.core->isCoreOnAir());
    QVERIFY(settingsRejectReason(s.windowEnd, hw(QStringLiteral("cal/txDisplayOffset"))).isEmpty());
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
}

// R-R3-46: the N2ADR switch on the Core's HL2 now applies its whole preset
// off the air (transmit pins included), and waits for receive on the air.
void TstRemoteOcCal::n2adrAppliesItsWholePresetOffTheAir()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true, /*hl2=*/true);
    QVERIFY(s.connect());
    const OcMatrix& oc = s.core->ocMatrix();
    s.core->ocMatrixMutable().setPin(Band::Band20m, 0, /*tx=*/true, true);
    const QString n2adr = hw(QStringLiteral("hl2IoBoard/n2adrFilter"));

    s.proxy.setValue(n2adr, QStringLiteral("True"));
    // mi0bot setup.cs chkHERCULES (HERMESLITE branch): 40 m RX and TX pin 3.
    QTRY_VERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/false));
    QVERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));
    QVERIFY(!oc.pinEnabled(Band::Band20m, 0, /*tx=*/true));  // cleared by the preset
    QCOMPARE(s.settings.value(hw(QStringLiteral("oc/tx/40m/pin3"))).toString(),
             QStringLiteral("True"));

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    s.proxy.setValue(n2adr, QStringLiteral("False"));
    QTRY_COMPARE(s.settings.value(n2adr).toString(), QStringLiteral("False"));
    QTest::qWait(150);
    QVERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));   // held while keyed
    s.unkeyCore();
    QTRY_VERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));
    QVERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/false));
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
}

// B4.2 / B4.3 in a remote window: the page shows the Core's values, its
// edits reach the Core, and on the air the TX pins and the resets close
// with the reason while the pin actions and calibration stay live.
void TstRemoteOcCal::remoteHardwarePageFollowsTheGates()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    s.settings.setValue(hw(QStringLiteral("oc/tx/80m/pin1")), QStringLiteral("True"));
    AppSettings::instance().setRemoteBackend(&s.proxy);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    s.window.alexAntennaFacade()->setWindowAvailability(true, {});

    HardwarePage page(&s.window);
    page.setTransmitPermitted(false, kTransmitReason);
    page.setTransmitSettingsPermitted(true, QString());
    page.setTransmitSettingsPermittedAt(8, true, QString());

    QGroupBox* txPins = groupTitled(&page, QStringLiteral("TX OC Pins per Band"));
    QGroupBox* actions = groupTitled(&page, QStringLiteral("TX Pin Action mapping"));
    QGroupBox* swlTx = groupTitled(&page, QStringLiteral("TX OC matrix"));
    QGroupBox* userDigOut = groupTitled(&page, QStringLiteral("User Dig Out"));
    QGroupBox* txDisplay = groupTitled(&page, QStringLiteral("TX Display Cal"));
    QGroupBox* voltsAmps = groupTitled(&page, QStringLiteral("Volts/Amps Calibration"));
    QPushButton* reset = buttonText(&page, QStringLiteral("Reset OC defaults"));
    QPushButton* swlReset = buttonText(&page, QStringLiteral("Reset SWL OC pins"));
    QVERIFY(txPins && actions && swlTx && userDigOut && txDisplay && voltsAmps && reset && swlReset);
    const QList<QWidget*> onAirClosed{txPins, swlTx, reset, swlReset};
    const QList<QWidget*> alwaysLive{actions, txDisplay, voltsAmps};

    // Off the air, with the Core at version 8 and remote transmit refused,
    // every one is live.
    for (QWidget* w : onAirClosed + alwaysLive) {
        QVERIFY2(w->isEnabled(), qPrintable(w->objectName()));
    }
    QVERIFY(userDigOut->isEnabled());
    // The Core applies the N2ADR switch's whole preset at version 8, so the
    // switch does not say it moves the receive filters only.
    auto* hl2Io = qobject_cast<Hl2IoBoardTab*>(
        page.tabWidgetForTest(HardwarePage::Tab::Hl2IoBoard));
    QVERIFY(hl2Io != nullptr);
    QVERIFY(!hl2Io->n2adrToolTipForTest().contains(Hl2IoBoardTab::receiveOnlyN2adrNote()));

    // The Core's pin shows; a click reaches the Core's store.
    QCheckBox* tx80p1 = checkWithTip(&page, QStringLiteral("TX OC pin 1, band 80m"));
    QCheckBox* tx20p3 = checkWithTip(&page, QStringLiteral("TX OC pin 3, band 20m"));
    QVERIFY(tx80p1 && tx20p3);
    QTRY_VERIFY(tx80p1->isChecked());
    tx20p3->setChecked(true);
    QTRY_COMPARE(s.settings.value(hw(QStringLiteral("oc/tx/20m/pin3"))).toString(),
                 QStringLiteral("True"));

    // On the air: the TX pins and resets close with the reason; the pin
    // actions and calibration stay live, as in Thetis.
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QTRY_VERIFY(!txPins->isEnabled());
    for (QWidget* w : onAirClosed) {
        QVERIFY2(!w->isEnabled(), qPrintable(w->objectName()));
        QCOMPARE(w->toolTip(), kOnAir);
    }
    for (QWidget* w : alwaysLive) {
        QVERIFY2(w->isEnabled(), qPrintable(w->objectName()));
    }
    // A TX Display Cal change goes through on the air.
    auto* txDisplaySpin = txDisplay->findChild<QDoubleSpinBox*>();
    QVERIFY(txDisplaySpin != nullptr);
    txDisplaySpin->setValue(4.0);
    QTRY_COMPARE(s.settings.value(hw(QStringLiteral("cal/txDisplayOffset"))).toString(),
                 QStringLiteral("4"));
    // User Dig Out follows the transmit settings gate MainWindow pushes,
    // which closes on the air (parity Task 1).
    page.setTransmitSettingsPermitted(false, kOnAir);
    QVERIFY(!userDigOut->isEnabled());
    QCOMPARE(userDigOut->toolTip(), kOnAir);

    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_VERIFY(txPins->isEnabled());
    QVERIFY(reset->isEnabled() && swlReset->isEnabled() && swlTx->isEnabled());
    page.setTransmitSettingsPermitted(true, QString());
    QVERIFY(userDigOut->isEnabled());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
}

void TstRemoteOcCal::olderCoreKeepsTheControlsClosedWithItsReason()
{
    // A remote window with no link that offers version 8.
    RadioModel remote(RadioModel::Role::Remote);
    remote.alexAntennaFacade()->setWindowAvailability(true, {});
    HardwarePage page(&remote);
    page.setTransmitSettingsPermittedAt(8, false, IStationLink::transmitSettingsUnavailableReason());
    const QString reason = IStationLink::transmitSettingsUnavailableReason();
    for (const QString& title : {QStringLiteral("TX OC Pins per Band"),
                                 QStringLiteral("TX Pin Action mapping"),
                                 QStringLiteral("TX OC matrix"),
                                 QStringLiteral("TX Display Cal"),
                                 QStringLiteral("Volts/Amps Calibration")}) {
        QGroupBox* box = groupTitled(&page, title);
        QVERIFY2(box != nullptr, qPrintable(title));
        QVERIFY2(!box->isEnabled(), qPrintable(title));
        QCOMPARE(box->toolTip(), reason);
    }
    QPushButton* reset = buttonText(&page, QStringLiteral("Reset OC defaults"));
    QVERIFY(reset && !reset->isEnabled());
    QCOMPARE(reset->toolTip(), reason);
    // Its N2ADR switch moves the receive filters only, and says so.
    page.setTransmitPermitted(false, kTransmitReason);
    auto* hl2Io = qobject_cast<Hl2IoBoardTab*>(
        page.tabWidgetForTest(HardwarePage::Tab::Hl2IoBoard));
    QVERIFY(hl2Io != nullptr);
    QVERIFY(hl2Io->n2adrToolTipForTest().contains(Hl2IoBoardTab::receiveOnlyN2adrNote()));
}

// Parity both ways: a local window closes the TX pins on the air too (the
// Thetis rule) and keeps the pin actions and calibration live.
void TstRemoteOcCal::localHardwarePageClosesTxPinsOnTheAirOnly()
{
    RadioModel local;
    local.setBoardForTest(HPSDRHW::Angelia);
    TxChannel txChannel(/*channelId=*/1);
    local.wireTransmitChainForTest(&txChannel);
    HardwarePage page(&local);
    QGroupBox* txPins = groupTitled(&page, QStringLiteral("TX OC Pins per Band"));
    QGroupBox* actions = groupTitled(&page, QStringLiteral("TX Pin Action mapping"));
    QGroupBox* txDisplay = groupTitled(&page, QStringLiteral("TX Display Cal"));
    QPushButton* reset = buttonText(&page, QStringLiteral("Reset OC defaults"));
    QVERIFY(txPins && actions && txDisplay && reset);
    QVERIFY(txPins->isEnabled() && reset->isEnabled() && actions->isEnabled());

    MoxController* mox = local.moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(local.isCoreOnAir());
    QTRY_VERIFY(!txPins->isEnabled());
    QCOMPARE(txPins->toolTip(), kOnAir);
    QVERIFY(!reset->isEnabled());
    QVERIFY(actions->isEnabled());
    QVERIFY(txDisplay->isEnabled());
    mox->setMox(false);
    QTRY_VERIFY(!local.isCoreOnAir());
    QTRY_VERIFY(txPins->isEnabled());
    QVERIFY(reset->isEnabled());
    local.injectTxChannelForTest(nullptr);
}

// C5, C6: the Alex-1 LPF band edges are hidden (in both windows, through
// the list: tst_unbuilt_features), and the values users saved stay in the
// settings file when the page loads and saves. The Alex-1 high-pass
// switches C5 also named are applied since plan Task 14, and the HL2 TX
// timings (C6) reach bank 17 since HL2 port part 1, so they are shown,
// their saved values kept too.
void TstRemoteOcCal::hiddenAlexAndHl2ControlsKeepTheirSavedValues()
{
    auto& settings = AppSettings::instance();
    const QMap<QString, QString> seeded{
        {hw(QStringLiteral("alex/master/hpfBypassOnTx")), QStringLiteral("True")},
        {hw(QStringLiteral("alex/master/hpfBypassOnPs")), QStringLiteral("False")},
        {hw(QStringLiteral("alex/master/disable6mLnaOnTx")), QStringLiteral("False")},
        {hw(QStringLiteral("alex/lpf/20m/start")), QStringLiteral("13.5")},
        {hw(QStringLiteral("alex/lpf/20m/end")), QStringLiteral("14.9")},
        {hw(QStringLiteral("hl2/pttHangMs")), QStringLiteral("25")},
        {hw(QStringLiteral("hl2/txLatencyMs")), QStringLiteral("33")},
    };
    for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    QVERIFY(settings.save());
    {
        RadioModel local;
        local.setBoardForTest(HPSDRHW::HermesLite);
        HardwarePage page(&local);
        RadioInfo info;
        info.macAddress = kMac;
        info.boardType = HPSDRHW::HermesLite;
        page.onCurrentRadioChanged(info);
        QCoreApplication::processEvents();
        for (const QString& name : {QStringLiteral("alexHpfBypassOnTx"),
                                    QStringLiteral("alexHpfBypassOnPs"),
                                    QStringLiteral("alexDisable6mLnaOnTx"),
                                    QStringLiteral("hl2TxBufferLatency"),
                                    QStringLiteral("hl2PttHang")}) {
            auto* w = page.findChild<QWidget*>(name);
            QVERIFY2(w != nullptr, qPrintable(name));
            QVERIFY2(!w->isHidden(), qPrintable(name));
        }
        for (const QString& name : {QStringLiteral("alexLpfStart_20m"),
                                    QStringLiteral("alexLpfEnd_20m")}) {
            auto* w = page.findChild<QWidget*>(name);
            QVERIFY2(w != nullptr, qPrintable(name));
            QVERIFY2(!w->isHidden(), qPrintable(name));
        }
        QVERIFY(settings.save());
    }
    QVERIFY(settings.save());
    AppSettings reread(settings.filePath());
    reread.load();
    for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
        QCOMPARE(reread.value(it.key()).toString(), it.value());
    }
}

QTEST_MAIN(TstRemoteOcCal)
#include "tst_remote_oc_cal.moc"
