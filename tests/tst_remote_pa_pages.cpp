// no-port-check: NereusSDR-original. R-R3-46 / R-R3-49 / R-R3-32
// (remote-window parity Task 6): Setup > PA (PA Gain, Watt Meter, PA Values),
// Radio Status's PA card and the HW Volts, Amps and Temperature meters in a
// remote window; the PA keys taken by the Core off the air and applied at
// once; the Core's TX inhibit mirrored. Loopback link, no RF and no
// hardware: nothing here keys a radio. "On the air" keys the Core's own
// MoxController with the receive-only MOX pre-check lifted, as
// tst_remote_transmit_setup_pages does. No audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Task 24: remote Settings Validation
//                                    gate reasons. AI-assisted via Codex.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-49 / R-R3-32 (parity
//                                    Task 6). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Parity Task 33: PA Values shows the
//                                    Core's transmit readings, and the
//                                    reason below txReadingsVersion 1.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the PA
//                                    keys are taken on the air and reload
//                                    at receive. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Radio Status names a TCI key, a key
//                                    from a device that does not hold
//                                    transmit and a RADE end-of-over tail
//                                    alike in both windows, and its history
//                                    records a release with its source.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/PaCalProfile.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/PttMode.h"
#include "core/TxChannel.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/RemoteTelemetryController.h"
#include "gui/diagnostics/RadioStatusPage.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/setup/PaSetupPages.h"
#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "gui/widgets/SystemTile.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kTransmitReason = QStringLiteral("Remote transmit is not here yet");
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:06");

QString paKey(const QString& rest)
{
    return QStringLiteral("hardware/%1/pa/profile/%2").arg(kMac, rest);
}

QString calKey(const QString& rest)
{
    return QStringLiteral("hardware/%1/paCalibration/%2").arg(kMac, rest);
}

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
// one window, handshake complete. `store` is the Core's settings store: the
// process-wide AppSettings for the Core-side cases (so the Core's live PA
// objects read what the server stored), a separate file for the window-page
// cases (whose window reads through its own SettingsProxy).
struct Session {
    Session(const QString& securityDir, QObject* parent, bool coreUsesProcessSettings)
        : ownSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , settings(coreUsesProcessSettings ? AppSettings::instance() : ownSettings)
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
        if (!completed.wait(30000) && completed.count() != 1) {
            return false;
        }
        // The handshake completes before the capability descriptor and the
        // settings snapshot land. Wait for both, bounded, rather than leaving
        // it to a later QTRY's default five seconds, which a loaded machine
        // can overrun.
        return QTest::qWaitFor([this] { return client->settingsHygieneAvailable(); }, 30000);
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

// A bank of one user profile "Bench" (active) and the G2's factory profile,
// written straight into a Core store, 20 m gain `gain20m`.
void seedBank(AppSettings& store, float gain20m)
{
    PaProfile bench(QStringLiteral("Bench"), HPSDRModel::ANAN_G2, false);
    bench.setGainForBand(Band::Band20m, gain20m);
    PaProfile factory(QStringLiteral("Default - ANAN_G2"), HPSDRModel::ANAN_G2, true);
    store.setValue(paKey(QStringLiteral("_names")), QStringLiteral("Bench,Default - ANAN_G2"));
    store.setValue(paKey(QStringLiteral("Bench")), bench.dataToString());
    store.setValue(paKey(QStringLiteral("Default - ANAN_G2")), factory.dataToString());
    store.setValue(paKey(QStringLiteral("active")), QStringLiteral("Bench"));
}

float storedGain20m(AppSettings& store, const QString& name)
{
    PaProfile p;
    if (!p.dataFromString(store.value(paKey(name)).toString())) { return -1.0f; }
    return p.getGainForBand(Band::Band20m);
}

StationTelemetrySnapshot paSample(quint32 sequence)
{
    StationTelemetrySnapshot sample;
    sample.sequence = sequence;
    sample.sampledElapsedMs = 100 * sequence;
    sample.radio.connected = true;
    sample.radio.paVolts = 13.8;
    sample.radio.supplyVolts = 12.1;
    sample.radio.paCurrentAmps = 1.5;
    sample.radio.paTemperatureCelsius = 38.0;
    return sample;
}

}  // namespace

class TstRemotePaPages : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void paKeysAreOnTheOffAirListAtVersion6();
    void paProfileWriteReachesTheCoresBankAtOnce();
    void paCalibrationWriteReachesTheCoresTableAtOnce();
    void paKeysRefusedOnTheAirForAWindowThatDoesNotTransmit();
    void remotePaGainPageShowsAndChangesTheCoresBank();
    void remoteWindowHoldsTheCoresOnAirRowOnKeyedRetune();
    void remoteWindowOnAnOlderCoreKeepsItsOwnRow();
    void remoteWattMeterPageChangesTheCoresTable();
    void remotePaReadingsShowOnRadioStatusPaValuesAndMeters();
    void localRadioStatusSetsPaVoltage();
    void remoteRadioStatusShowsTheCoresTransmitAndUptime();
    void radioStatusNamesEveryKeyAlikeInBothWindows();
    void remoteSettingsResetAndTokenMutationGivePlainDisabledReasons();
    void coreTxInhibitReachesTheWindow();
    void systemTileSaysTheReadingsAreTheCores();
    void newReasonsArePlain();

private:
    QTemporaryDir m_securityDir;
};

void TstRemotePaPages::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("remote-pa-pages-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemotePaPages::cleanupTestCase()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemotePaPages::cleanup()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemotePaPages::paKeysAreOnTheOffAirListAtVersion6()
{
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(paKey(QStringLiteral("Bench"))));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(paKey(QStringLiteral("active"))));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(calKey(QStringLiteral("calPoint3"))));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(calKey(QStringLiteral("boardClass"))));
    // The rest of the transmit hardware stays refused.
    QVERIFY(!StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("hardware/%1/tx/micGainDb").arg(kMac)));
    // The Calibration tab's transmit fields joined at version 8 (parity
    // Task 13, tst_remote_oc_cal), taken on the air too; the PA table keeps
    // version 6's on-air rule.
    QVERIFY(StationServer::isTransmitSettingKeyTakenOnAir(
        QStringLiteral("hardware/%1/paCalibration/cal/paSens").arg(kMac)));
    QVERIFY(!StationServer::isTransmitSettingKeyTakenOnAir(calKey(QStringLiteral("calPoint3"))));

    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    QVERIFY(s.connect());
    // 7 since parity Task 7 (PureSignal arming), 8 since parity Task 13
    // (Hardware Config's OC transmit pins and transmit calibration); 6 is
    // within it.
    // 14 since the Core owns Prevent transmitting on a different band, 15
    // since a remote window edits the CFC bands.
    QCOMPARE(s.client->capabilities().transmitSettingsVersion, 15);
    QVERIFY(s.client->transmitSettingsAvailable(6));
}

// B5.15: a PA Gain change from a window reaches the Core's live PA profile
// bank at once (the in-memory profile its drive is computed from).
void TstRemotePaPages::paProfileWriteReachesTheCoresBankAtOnce()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    PaProfileManager* const bank = s.core->paProfileManager();
    bank->setMacAddress(kMac);
    bank->load(HPSDRModel::ANAN_G2);
    QVERIFY(bank->activeProfile());
    const QString active = bank->activeProfileName();
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());

    PaProfile edited = *bank->activeProfile();
    edited.setGainForBand(Band::Band20m, 44.5f);
    s.proxy.setValue(paKey(active), edited.dataToString());
    QTRY_COMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 44.5f);

    // A new profile, made active, as the page's New does.
    PaProfile fresh(QStringLiteral("Contest"), HPSDRModel::ANAN_G2, false);
    fresh.setGainForBand(Band::Band40m, 51.0f);
    QSignalSpy listChanged(bank, &PaProfileManager::profileListChanged);
    s.proxy.setValue(paKey(QStringLiteral("Contest")), fresh.dataToString());
    s.proxy.setValue(paKey(QStringLiteral("_names")),
                     bank->profileNames().join(QLatin1Char(',')) + QStringLiteral(",Contest"));
    s.proxy.setValue(paKey(QStringLiteral("active")), QStringLiteral("Contest"));
    QTRY_COMPARE(bank->activeProfileName(), QStringLiteral("Contest"));
    QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band40m), 51.0f);
    QVERIFY(listChanged.count() >= 1);

    // A removed profile leaves the bank.
    s.proxy.setValue(paKey(QStringLiteral("active")), active);
    s.proxy.setValue(paKey(QStringLiteral("_names")),
                     QStringList(bank->profileNames()).filter(QRegularExpression(QStringLiteral("^(?!Contest$)")))
                         .join(QLatin1Char(',')));
    s.proxy.remove(paKey(QStringLiteral("Contest")));
    QTRY_VERIFY(!bank->profileByName(QStringLiteral("Contest")));
    QTRY_COMPARE(bank->activeProfileName(), active);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// B5.15: the Watt Meter's PA forward-power table reaches the Core's
// calibration controller at once.
void TstRemotePaPages::paCalibrationWriteReachesTheCoresTableAtOnce()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    CalibrationController& cal = s.core->calibrationControllerMutable();
    s.proxy.setValue(calKey(QStringLiteral("boardClass")),
                     QString::number(static_cast<int>(PaCalBoardClass::Anan100)));
    s.proxy.setValue(calKey(QStringLiteral("calPoint3")), QStringLiteral("33.5"));
    QTRY_COMPARE(cal.paCalProfile().boardClass, PaCalBoardClass::Anan100);
    QTRY_COMPARE(cal.paCalProfile().watts[3], 33.5f);
}

void TstRemotePaPages::paKeysRefusedOnTheAirForAWindowThatDoesNotTransmit()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/true);
    PaProfileManager* const bank = s.core->paProfileManager();
    bank->setMacAddress(kMac);
    bank->load(HPSDRModel::ANAN_G2);
    const QString active = bank->activeProfileName();
    const float before = bank->activeProfile()->getGainForBand(Band::Band20m);
    s.core->sliceById(0)->setFrequency(14200000.0); // transmits on 20 m
    QVERIFY(s.connect());

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    // R-R3-49 / R-IOS-27 (JJ's ruling, follow Thetis): on the air only the
    // device that holds transmit changes the transmitting band's PA
    // values. The Core keyed itself, so the window's change is refused with
    // the Core's value handed back, not held until receive; another band
    // is refused as Thetis greys it. The Watt Meter's points stay taken.
    const QString kept = s.settings.value(paKey(active)).toString();
    PaProfile edited = *bank->activeProfile();
    edited.setGainForBand(Band::Band20m, before + 3.0f);
    s.proxy.setValue(paKey(active), edited.dataToString());
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, paKey(active)),
                 RadioModel::paHolderOnlyReason());
    PaProfile otherBand = *bank->activeProfile();
    otherBand.setGainForBand(Band::Band40m, 51.0f);
    s.proxy.setValue(paKey(active), otherBand.dataToString());
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, paKey(active)),
                 RadioModel::paOnAirLockedReason());
    s.proxy.setValue(calKey(QStringLiteral("calPoint2")), QStringLiteral("19"));
    QTRY_COMPARE(s.settings.value(calKey(QStringLiteral("calPoint2"))).toString(),
                 QStringLiteral("19"));
    QCOMPARE(s.settings.value(paKey(active)).toString(), kept);
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
    QTest::qWait(150);
    QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), before);
    QCOMPARE(s.settings.value(paKey(active)).toString(), kept);
}

// B5.15: PA Gain in a remote window shows the Core's bank and changes it;
// the Core's own change shows on the page; on the air a change is refused
// and the page goes back to the Core's value; auto-calibrate keeps the
// remote transmit reason.
void TstRemotePaPages::remotePaGainPageShowsAndChangesTheCoresBank()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    seedBank(s.settings, 47.5f);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    s.core->sliceById(0)->setFrequency(14200000.0); // transmits on 20 m
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    QTRY_COMPARE(s.window.paProfileManager()->activeProfileName(), QStringLiteral("Bench"));

    PaGainByBandPage page(&s.window);
    QComboBox* combo = page.profileComboForTest();
    QDoubleSpinBox* gain20 = page.gainSpinForTest(Band::Band20m);
    QDoubleSpinBox* max20 = page.maxPowerSpinForTest(Band::Band20m);
    QCheckBox* autoCal = page.autoCalibrateCheckForTest();
    QVERIFY(combo && gain20 && max20 && autoCal);

    // Closed until the dialog pushes the version 6 gate, with its reason.
    QVERIFY(!gain20->isEnabled());
    QCOMPARE(gain20->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    page.setTransmitSettingsPermittedAt(6, true, QString());
    page.setTransmitPermitted(false, kTransmitReason);
    QVERIFY(gain20->isEnabled() && combo->isEnabled() && max20->isEnabled());
    QVERIFY(page.newButtonForTest()->isEnabled());
    // The sweep keys the radio: it keeps the remote transmit reason.
    QVERIFY(!autoCal->isEnabled());
    QCOMPARE(autoCal->toolTip(), kTransmitReason);
    // A capability pass does not reopen it.
    page.applyCapabilityVisibility(s.window.boardCapabilities());
    QVERIFY(!autoCal->isEnabled());
    QVERIFY(gain20->isEnabled());

    // The Core's values.
    QCOMPARE(combo->currentText(), QStringLiteral("Bench"));
    QCOMPARE(gain20->value(), 47.5);

    // A change reaches the Core's store at once.
    gain20->setValue(45.0);
    QTRY_COMPARE(storedGain20m(s.settings, QStringLiteral("Bench")), 45.0f);

    // The Core's own change shows on the page.
    PaProfile coreEdit;
    QVERIFY(coreEdit.dataFromString(s.settings.value(paKey(QStringLiteral("Bench"))).toString()));
    coreEdit.setGainForBand(Band::Band20m, 49.0f);
    s.settings.setValue(paKey(QStringLiteral("Bench")), coreEdit.dataToString());
    QTRY_COMPARE(gain20->value(), 49.0);

    // On the air the Core keyed itself, so the window holds no transmit:
    // its change is refused and the Core keeps its value (JJ's ruling).
    // The page shows it: the transmitting band's row with the holder
    // reason, profiles and the other bands with Thetis's lock.
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QVERIFY(!gain20->isEnabled());
    QCOMPARE(gain20->toolTip(), RadioModel::paHolderOnlyReason());
    QVERIFY(!combo->isEnabled());
    QCOMPARE(combo->toolTip(), RadioModel::paOnAirLockedReason());
    QVERIFY(!page.gainSpinForTest(Band::Band40m)->isEnabled());
    gain20->setValue(40.0);
    QTRY_VERIFY(!settingsRejectReason(s.windowEnd, paKey(QStringLiteral("Bench"))).isEmpty());
    QCOMPARE(storedGain20m(s.settings, QStringLiteral("Bench")), 49.0f);
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
    QVERIFY(gain20->isEnabled() && combo->isEnabled());

    // The gate closes the editor with its reason.
    page.setTransmitSettingsPermittedAt(6, false, kOnAir);
    QVERIFY(!gain20->isEnabled() && !combo->isEnabled());
    QCOMPARE(gain20->toolTip(), kOnAir);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// The Core holds its transmit band while keyed (console.cs TXBand setter,
// //[2.10.3.6]MW0LGE no band change on TX fix). A remote window opens and
// locks the row the Core holds, not the row its own slice now shows.
void TstRemotePaPages::remoteWindowHoldsTheCoresOnAirRowOnKeyedRetune()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    seedBank(s.settings, 47.5f);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    s.core->sliceById(0)->setFrequency(14200000.0); // transmits on 20 m
    QVERIFY(s.connect());
    QTRY_COMPARE(s.window.paProfileManager()->activeProfileName(), QStringLiteral("Bench"));

    PaGainByBandPage page(&s.window);
    page.setTransmitSettingsPermittedAt(6, true, QString());
    page.setTransmitPermitted(false, kTransmitReason);
    QDoubleSpinBox* gain20 = page.gainSpinForTest(Band::Band20m);
    QDoubleSpinBox* gain17 = page.gainSpinForTest(Band::Band17m);
    QVERIFY(gain20 && gain17);

    const int band20 = static_cast<int>(Band::Band20m);
    const int band17 = static_cast<int>(Band::Band17m);
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QTRY_COMPARE(s.window.paOnAirBandIndex(), band20);

    // Retune the transmit slice to 17 m while keyed; the window's slice
    // follows the Core's.
    s.core->sliceById(0)->setFrequency(18100000.0);
    QTRY_VERIFY(s.window.sliceById(0) != nullptr
                && qFuzzyCompare(s.window.sliceById(0)->frequency(), 18100000.0));
    QCoreApplication::processEvents();
    // The Core still holds 20 m, and so does the window.
    QCOMPARE(s.core->paOnAirBandIndex(), band20);
    QCOMPARE(s.window.paOnAirBandIndex(), band20);
    // The open row is 20 m: the holder's row, with the holder reason; the
    // 17 m row carries Thetis's on-air lock.
    QCOMPARE(gain20->toolTip(), RadioModel::paHolderOnlyReason());
    QCOMPARE(gain17->toolTip(), RadioModel::paOnAirLockedReason());
    // The Core accepts a 20 m edit from the holder and refuses 17 m.
    QVERIFY(s.core->paOnAirEditRefusal(false, band20, true).isEmpty());
    QCOMPARE(s.core->paOnAirEditRefusal(false, band17, true),
             RadioModel::paOnAirLockedReason());

    // At receive the next band change moves the Core's row, and the
    // window's with it.
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
    s.core->sliceById(0)->setFrequency(18110000.0);
    QTRY_COMPARE(s.core->paOnAirBandIndex(), band17);
    QTRY_COMPARE(s.window.paOnAirBandIndex(), band17);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// An older Core sends no paTransmitBand: the window keeps the row its own
// state gives, as before the Core published its held band.
void TstRemotePaPages::remoteWindowOnAnOlderCoreKeepsItsOwnRow()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    seedBank(s.settings, 47.5f);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    s.client->withholdFeatureForTest(QByteArrayLiteral("paTransmitBand"));
    s.core->sliceById(0)->setFrequency(18100000.0); // the Core transmits on 17 m
    const int windowOwnRow = s.window.paOnAirBandIndex();
    QSignalSpy windowBand(&s.window, &RadioModel::paTransmitBandChanged);
    QVERIFY(s.connect());
    QTRY_COMPARE(s.window.paProfileManager()->activeProfileName(), QStringLiteral("Bench"));

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QCOMPARE(s.core->paOnAirBandIndex(), static_cast<int>(Band::Band17m));
    QCoreApplication::processEvents();
    QCOMPARE(windowBand.count(), 0);
    QCOMPARE(s.window.paOnAirBandIndex(), windowOwnRow);
    s.unkeyCore();
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// B5.15: the Watt Meter in a remote window changes the Core's PA table.
void TstRemotePaPages::remoteWattMeterPageChangesTheCoresTable()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    // The window shows its G2's factory table until one is saved.
    QTRY_COMPARE(s.window.calibrationController().paCalProfile().boardClass,
                 PaCalBoardClass::Anan100);

    PaWattMeterPage page(&s.window);
    auto* group = page.findChild<PaCalibrationGroup*>();
    QVERIFY(group);
    QCOMPARE(group->spinBoxCountForTest(), 10);
    QVERIFY(!group->isEnabled());
    page.setTransmitSettingsPermittedAt(6, true, QString());
    QVERIFY(group->isEnabled());

    group->setSpinValueForTest(4, 41.0);
    QTRY_COMPARE(s.settings.value(calKey(QStringLiteral("calPoint4"))).toString(),
                 QStringLiteral("41"));
    QCOMPARE(s.settings.value(calKey(QStringLiteral("boardClass"))).toInt(),
             static_cast<int>(PaCalBoardClass::Anan100));

    // The Core's own change shows on the page.
    s.settings.setValue(calKey(QStringLiteral("calPoint4")), QStringLiteral("43"));
    QTRY_COMPARE(group->spinValueForTest(4), 43.0);
}

// B6.5: the Core's PA readings on Radio Status, PA Values and the HW meters
// in a remote window, each said to be the Core's; stale is unavailable.
void TstRemotePaPages::remotePaReadingsShowOnRadioStatusPaValuesAndMeters()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    s.server->setTelemetryEnabled(true);
    qint64 now = 10000;
    RemoteTelemetryController telemetry(s.client.get(), nullptr, nullptr, [&] { return now; });
    telemetry.setPaReadingsTarget(&s.window);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.client->telemetryAvailable());

    RadioStatusPage status(&s.window);
    auto* statusTitle = status.findChild<QLabel*>();
    Q_UNUSED(statusTitle);
    PaValuesPage values(&s.window);
    MeterWidget meters;
    auto* volts = new TextItem(&meters);
    auto* amps = new TextItem(&meters);
    auto* temperature = new TextItem(&meters);
    volts->setBindingId(MeterBinding::HwVolts);
    amps->setBindingId(MeterBinding::HwAmps);
    temperature->setBindingId(MeterBinding::HwTemperature);
    meters.addItem(volts);
    meters.addItem(amps);
    meters.addItem(temperature);
    MeterPoller poller;
    poller.addTarget(&meters);
    poller.setPaReadingsModel(&s.window);
    const auto tick = [&]() {
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
    };

    const auto labelWithText = [&status](const QString& text) {
        for (QLabel* label : status.findChildren<QLabel*>()) {
            if (label->text() == text) { return label; }
        }
        return static_cast<QLabel*>(nullptr);
    };

    // Before any reading: unavailable, never 0.
    QVERIFY(labelWithText(QStringLiteral("PA Status, from the Core")));
    QVERIFY(labelWithText(QStringLiteral("Unavailable")));
    QCOMPARE(values.paCurrentTextForTest(), QStringLiteral("Unavailable"));
    QCOMPARE(values.supplyVoltsTextForTest(), QStringLiteral("Unavailable"));
    tick();
    QVERIFY(isNoMeterReading(volts->value()));
    QVERIFY(isNoMeterReading(temperature->value()));

    QVERIFY(s.server->sendTelemetry(paSample(1), s.server->sessionEpoch()));
    QTRY_COMPARE(s.window.paReadings().paVolts, std::optional<double>(13.8));
    // Radio Status: PA Voltage (the G2's PA volts), PA Current, PA Temp.
    QLabel* voltage = labelWithText(QStringLiteral("13.8 V"));
    QVERIFY(voltage);
    QCOMPARE(voltage->toolTip(), QStringLiteral("From the Core"));
    QVERIFY(labelWithText(QStringLiteral("1.5 A")));
    QCOMPARE(values.paCurrentTextForTest(), QStringLiteral("1.50 A"));
    QCOMPARE(values.supplyVoltsTextForTest(), QStringLiteral("12.1 V"));
    QVERIFY(values.paTempTextForTest().contains(QStringLiteral("38.0")));
    // Parity Task 33: the Core's transmit readings (txReadingsVersion 1),
    // its raw forward reading before any sample.
    QCOMPARE(values.fwdAdcTextForTest(), QStringLiteral("0"));
    tick();
    QCOMPARE(volts->value(), 13.8);
    QCOMPARE(amps->value(), 1.5);
    QCOMPARE(temperature->value(), 38.0);

    // Stale: unavailable again.
    now += 4000;
    telemetry.sampleNow();
    QTRY_VERIFY(!s.window.paReadings().paVolts);
    QVERIFY(!labelWithText(QStringLiteral("13.8 V")));
    QCOMPARE(values.paCurrentTextForTest(), QStringLiteral("Unavailable"));
    tick();
    QVERIFY(isNoMeterReading(volts->value()));
    QVERIFY(volts->isNoReading(volts->value()));
}

// Passing (PA Voltage never set): locally Radio Status's PA Voltage comes
// from this window's own radio.
void TstRemotePaPages::localRadioStatusSetsPaVoltage()
{
    RadioModel local;
    local.setBoardForTest(HPSDRHW::Saturn);
    local.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioStatusPage status(&local);
    const auto hasText = [&status](const QString& text) {
        for (QLabel* label : status.findChildren<QLabel*>()) {
            if (label->text() == text) { return true; }
        }
        return false;
    };
    QVERIFY(hasText(QStringLiteral("PA Status")));
    QVERIFY(hasText(QStringLiteral("Unavailable")));
    QVERIFY(!local.paReadingsFromCore());
}

// Radio Status in a remote window: the PTT card, Forward / Reflected / SWR
// and Uptime follow the Core, from what the link already carries (the
// mirrored txState and station telemetry's connectionAgeMs), and read as the
// Core's own window reads them.
void TstRemotePaPages::remoteRadioStatusShowsTheCoresTransmitAndUptime()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    s.server->setTelemetryEnabled(true);
    qint64 now = 10000;
    RemoteTelemetryController telemetry(s.client.get(), nullptr, nullptr, [&] { return now; });
    telemetry.setPaReadingsTarget(&s.window);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.client->telemetryAvailable());
    QVERIFY(s.window.stationTransmitState() != nullptr);

    // The remote window's meter poller copies the Core's power readings
    // into its RadioStatus, as MainWindow wires it.
    MeterPoller poller;
    poller.setRemoteRadioModel(&s.window, [] { return true; }, {});
    poller.setRemoteTransmitState(s.window.stationTransmitState(), {});

    RadioStatusPage remote(&s.window);
    RadioStatusPage local(s.core.get());
    const auto readout = [](const RadioStatusPage& page, const char* id) {
        const QString wanted = QStringLiteral("diagnostics.radioStatus.") + QLatin1String(id);
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->property("nereusSetupId").toString() == wanted) {
                return label->text();
            }
        }
        return QStringLiteral("<missing>");
    };
    const auto pttText = [](const RadioStatusPage& page) {
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("Active: "))) {
                return label->text();
            }
        }
        return QStringLiteral("<missing>");
    };

    QCOMPARE(pttText(remote), QStringLiteral("Active: none"));
    QCOMPARE(readout(remote, "forward"), QStringLiteral("– W"));

    // Keyed from the Core's MOX: both windows say MOX and TX.
    s.keyCore();
    QTRY_COMPARE(pttText(local), QStringLiteral("Active: MOX"));
    QTRY_COMPARE(pttText(remote), QStringLiteral("Active: MOX"));
    QCOMPARE(readout(local, "mode"), QStringLiteral("TX"));
    QCOMPARE(readout(remote, "mode"), QStringLiteral("TX"));

    // The Core's power readings while keyed.
    s.core->radioStatus().setPowerReadings(50.0, 2.0, 1.5);
    QTRY_COMPARE(readout(local, "forward"), QStringLiteral("50.0 W"));
    QTRY_COMPARE(readout(remote, "forward"), QStringLiteral("50.0 W"));
    QCOMPARE(readout(remote, "reflected"), readout(local, "reflected"));
    QCOMPARE(readout(remote, "swr"), readout(local, "swr"));
    QCOMPARE(readout(remote, "reflected"), QStringLiteral("2.0 W"));

    // Released: none again, and the power readouts rest.
    s.unkeyCore();
    QTRY_COMPARE(pttText(local), QStringLiteral("Active: none"));
    QTRY_COMPARE(pttText(remote), QStringLiteral("Active: none"));
    QTRY_COMPARE(readout(remote, "forward"), QStringLiteral("– W"));
    QCOMPARE(readout(remote, "mode"), QStringLiteral("RX (idle)"));

    // Uptime: the Core's connection age, which keeps counting between
    // samples, and is unavailable once the samples go stale.
    QCOMPARE(readout(remote, "uptime"), QStringLiteral("–"));
    StationTelemetrySnapshot sample = paSample(1);
    sample.radio.connectionAgeMs = 95000;
    QVERIFY(s.server->sendTelemetry(sample, s.server->sessionEpoch()));
    QTRY_VERIFY(s.window.connectionAgeMs().has_value());
    QVERIFY(*s.window.connectionAgeMs() >= 95000);
    QTRY_VERIFY(readout(remote, "uptime").startsWith(QStringLiteral("1m 3")));
    now += 4000;
    telemetry.sampleNow();
    QTRY_VERIFY(!s.window.connectionAgeMs().has_value());
    QTRY_COMPARE(readout(remote, "uptime"), QStringLiteral("–"));
}

// The same key reads the same in the Core's window and a remote one: a TCI
// key as TCI; a key from a device that does not hold transmit as the Core's
// own window names it (Remote); the RADE end-of-over tail as the key it
// ends; and the history records each release with the source it ended.
void TstRemotePaPages::radioStatusNamesEveryKeyAlikeInBothWindows()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    s.server->setTelemetryEnabled(true);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.client->telemetryAvailable());
    const TransmitState* const txState = s.window.stationTransmitState();
    QVERIFY(txState != nullptr);

    RadioStatusPage remote(&s.window);
    RadioStatusPage local(s.core.get());
    const auto pttText = [](const RadioStatusPage& page) {
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->text().startsWith(QStringLiteral("Active: "))) {
                return label->text();
            }
        }
        return QStringLiteral("<missing>");
    };
    const auto lastEvent = [](const RadioStatusPage& page) {
        const QListWidget* const list = page.findChild<QListWidget*>();
        if (list == nullptr || list->count() == 0) {
            return QStringLiteral("<empty>");
        }
        return list->item(0)->text();
    };
    const auto bothRead = [&](const QString& text) {
        return pttText(local) == text && pttText(remote) == text;
    };

    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    // The keys below come straight from the Core's own keyers, so no
    // device's hold on transmit is asked for.
    mox->setKeyingGate({});

    // A TCI key.
    mox->setMox(true, KeyerIdentity::station(PttMode::Tci));
    QVERIFY(mox->isMox());
    QCOMPARE(s.core->keyedBy().trigger, QByteArrayLiteral("tci"));
    QTRY_VERIFY2(bothRead(QStringLiteral("Active: TCI")),
                 qPrintable(pttText(local) + QStringLiteral(" / ") + pttText(remote)));
    mox->setMox(false);
    QTRY_VERIFY(bothRead(QStringLiteral("Active: none")));
    QTRY_VERIFY2(lastEvent(local).endsWith(QStringLiteral("TX end (TCI)")),
                 qPrintable(lastEvent(local)));
    QTRY_VERIFY2(lastEvent(remote).endsWith(QStringLiteral("TX end (TCI)")),
                 qPrintable(lastEvent(remote)));

    // A device's key with nobody holding transmit: the Core names the device
    // but has no kind for it.
    KeyerIdentity device;
    device.deviceId = QByteArrayLiteral("phone-without-transmit");
    mox->setMox(true, device);
    QVERIFY(mox->isMox());
    QCOMPARE(s.core->keyedBy().deviceId, device.deviceId);
    QVERIFY(s.core->keyedBy().deviceKind.isEmpty());
    // No link carries this device's keepalives here, so the transmit
    // watchdog stops watching it rather than ending the key.
    s.server->txWatchdog()->released(device.deviceId);
    QTRY_VERIFY2(bothRead(QStringLiteral("Active: Remote")),
                 qPrintable(pttText(local) + QStringLiteral(" / ") + pttText(remote)));
    QVERIFY(mox->isMox());
    mox->setMox(false, device);
    QTRY_VERIFY(bothRead(QStringLiteral("Active: none")));

    // A RADE end-of-over tail: the radio stays on the air after the
    // release, and both windows keep the key's source until it ends.
    mox->setEndOfOverTail([]() { return true; });
    s.keyCore();
    QTRY_VERIFY(bothRead(QStringLiteral("Active: MOX")));
    s.unkeyCore();
    QVERIFY(s.core->endOfOverTailActive());
    QCOMPARE(pttText(local), QStringLiteral("Active: MOX"));
    QTRY_VERIFY(txState->txEnding());
    QCOMPARE(pttText(remote), QStringLiteral("Active: MOX"));
    mox->onEndOfOverTailDone();
    QTRY_VERIFY(bothRead(QStringLiteral("Active: none")));
    QVERIFY2(lastEvent(local).endsWith(QStringLiteral("TX end (MOX)")),
             qPrintable(lastEvent(local)));
    QTRY_VERIFY2(lastEvent(remote).endsWith(QStringLiteral("TX end (MOX)")),
                 qPrintable(lastEvent(remote)));

    // A radio with no station running names nobody as keyed, so its own
    // page reads the key's source from the key itself, through a tail too.
    RadioModel alone;
    MoxController* const aloneMox = alone.moxController();
    aloneMox->setMoxCheck({});
    aloneMox->setEndOfOverTail([]() { return true; });
    aloneMox->setMox(true, KeyerIdentity::station(PttMode::Tci));
    QVERIFY(aloneMox->isMox());
    QVERIFY(alone.keyedBy().isEmpty());
    // The page follows the key once the TX walk has ended.
    QTRY_COMPARE(alone.radioStatus().activePttSource(), PttSource::Tci);
    aloneMox->setMox(false);
    QVERIFY(alone.endOfOverTailActive());
    QCOMPARE(alone.radioStatus().activePttSource(), PttSource::Tci);
    aloneMox->onEndOfOverTailDone();
    QTRY_COMPARE(alone.radioStatus().activePttSource(), PttSource::None);
    QCOMPARE(alone.radioStatus().recentPttEvents().first().source, PttSource::Tci);
    QVERIFY(!alone.radioStatus().recentPttEvents().first().isStart);
}

void TstRemotePaPages::remoteSettingsResetAndTokenMutationGivePlainDisabledReasons()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    QVERIFY(s.connect());
    QVERIFY(s.client->settingsHygieneAvailable());
    QVERIFY(!s.client->signedInWithDeviceKey());

    SettingsValidationPage validation(&s.window);
    validation.setStationSettingsAvailable(true, {});
    RadioStatusPage status(&s.window);
    status.setStationSettingsAvailable(true, {});
    const auto button = [](QWidget& page, const QString& label) {
        for (QPushButton* candidate : page.findChildren<QPushButton*>()) {
            if (candidate->text() == label) { return candidate; }
        }
        return static_cast<QPushButton*>(nullptr);
    };
    auto* validate = button(validation, QStringLiteral("Re-validate"));
    auto* validationRepair = button(validation, QStringLiteral("Repair Invalid Settings"));
    auto* validationForget = button(validation, QStringLiteral("Forget This Radio"));
    auto* statusRepair = button(status, QStringLiteral("Repair invalid settings"));
    auto* statusForget = button(status, QStringLiteral("Forget this radio"));
    QVERIFY(validate && validate->isEnabled());
    // G-38: this Core offers Repair (settingsHygieneVersion 2); a window
    // signed in with the pairing token still cannot run it.
    QTRY_VERIFY(s.client->settingsRepairAvailable());
    for (QPushButton* repair : {validationRepair, statusRepair}) {
        QVERIFY(repair && !repair->isEnabled());
        QCOMPARE(repair->toolTip(),
                 QStringLiteral("Pair this computer with the Core to repair its radio settings."));
    }
    for (QPushButton* forget : {validationForget, statusForget}) {
        QVERIFY(forget && !forget->isEnabled());
        QVERIFY(forget->toolTip().contains(QStringLiteral("Pair this computer")));
    }

    RadioModel olderWindow(RadioModel::Role::Remote);
    SettingsValidationPage older(&olderWindow);
    older.setStationSettingsAvailable(true, {});
    auto* olderValidate = button(older, QStringLiteral("Re-validate"));
    QVERIFY(olderValidate && !olderValidate->isEnabled());
    QVERIFY(olderValidate->toolTip().contains(QStringLiteral("does not offer Settings Validation")));
}

// Carried from gaps Task 13: the Core's TX inhibit reaches the window as
// `txInhibited`, and clears when the session ends.
void TstRemotePaPages::coreTxInhibitReachesTheWindow()
{
    Session s(m_securityDir.path(), this, /*coreUsesProcessSettings=*/false);
    QVERIFY(s.connect());
    QVERIFY(!s.window.isTxInhibited());
    QSignalSpy changed(&s.window, &RadioModel::txInhibitedChanged);
    // The radio's inhibit input, as gaps Task 13's connection feeds it.
    bool input = true;
    safety::TxInhibitMonitor& monitor = s.core->txInhibit();
    monitor.setEnabled(true);
    monitor.setUserIoReader([&input] { return input; });
    QVERIFY(s.core->isTxInhibited());
    QTRY_VERIFY(s.window.isTxInhibited());
    QVERIFY(changed.count() >= 1);
    input = false;
    monitor.setUserIoReader([&input] { return input; });
    QTRY_VERIFY(!s.window.isTxInhibited());
    input = true;
    monitor.setUserIoReader([&input] { return input; });
    QTRY_VERIFY(s.window.isTxInhibited());
    s.window.clearRemoteTransmittingState();
    QVERIFY(!s.window.isTxInhibited());
    monitor.setUserIoReader({});
    monitor.setEnabled(false);
}

// B6.5 / R-R3-32: the System tile's PA row names its source in a remote
// window; a local window's row carries only its own hint.
void TstRemotePaPages::systemTileSaysTheReadingsAreTheCores()
{
    SystemTile tile;
    tile.setPaVolts(13.8);
    QVERIFY(tile.toolTip().isEmpty());
    tile.setPaSourceNote(QStringLiteral("From the Core"));
    QCOMPARE(tile.toolTip(), QStringLiteral("From the Core"));
    tile.setPaTempCelsius(40.0);
    QVERIFY(tile.toolTip().startsWith(QStringLiteral("From the Core\n")));
    tile.setPaSourceNote(QString());
    QVERIFY(!tile.toolTip().contains(QStringLiteral("Core")));
}

void TstRemotePaPages::newReasonsArePlain()
{
    for (const QString& text : {
             QStringLiteral("Unavailable"), QStringLiteral("From the Core"),
             QStringLiteral("PA Status, from the Core"),
             QStringLiteral("This Core does not send this reading. Updating the Core may help."),
             QStringLiteral("Not measured"),
             QStringLiteral("Expected a boolean TX inhibit observation.")}) {
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
    }
}

QTEST_MAIN(TstRemotePaPages)
#include "tst_remote_pa_pages.moc"
