// no-port-check: NereusSDR-original. R-R3-49 (parity Task 3): a remote
// window's TX profile combos (TX applet, Phone/CW applet, RADE applet),
// Setup > Audio > TX Profile, Setup > Audio > TX Input's radio microphone
// and RADE's Reset vocoder act on an in-process Core. Loopback link, no RF
// and no hardware: nothing here keys a radio. "On the air" keys the Core's
// own MoxController with the receive-only MOX pre-check lifted, as
// tst_transmit_model_properties does. No audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): created.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec lane: the HL2 window's
//                                    Radio Mic and Line In Gain steps.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the
//                                    microphone settings are taken keyed;
//                                    Reset vocoder is the holder's. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QGroupBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/RadeChannel.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SetupDialog.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/RadeApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/TxProfileSetupPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:01");
const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    model->setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    model->scopeTxProfiles(kMac);
    return model;
}

struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , txChannel(/*channelId=*/1)
    {
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
        core->injectTxChannelForTest(nullptr);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        const bool done = completed.wait(5000) || completed.count() == 1;
        return done && QTest::qWaitFor([this] {
            return window.micProfileManager()->profileNames()
                == core->micProfileManager()->profileNames();
        }, 3000);
    }
    SessionPropertyResult writeTransmit(const QByteArray& name, MirrorWireKind kind,
                                        const QVariant& value)
    {
        const quint32 writeId = ++nextId;
        MirrorUpdate update;
        update.ordinal = 1;
        update.name = name;
        update.kind = kind;
        update.value = value;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite(QByteArrayLiteral("transmit"), {update}, writeId)));
        SessionPropertyResult found;
        const bool arrived = QTest::qWaitFor([&] {
            for (const QByteArray& wire : windowEnd->received()) {
                SessionMessage message;
                if (!SessionMessages::decode(wire, &message)
                    || message.kind != SessionMessageKind::PropertyResult
                    || message.writeId != writeId) {
                    continue;
                }
                for (const SessionPropertyResult& result : message.propertyResults) {
                    if (result.property == name) {
                        found = result;
                        return true;
                    }
                }
            }
            return false;
        }, 3000);
        if (!arrived) {
            found.reason = QStringLiteral("no property.result arrived");
        }
        return found;
    }
    void keyCore()
    {
        MoxController* const mox = core->moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
    }
    void unkeyCore() { core->moxController()->setMox(false); }
    QString coreActive() const { return core->micProfileManager()->activeProfileName(); }

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
    quint32 nextId = 94000;
};

QComboBox* phoneProfileCombo(PhoneCwApplet& applet)
{
    for (QComboBox* combo : applet.findChildren<QComboBox*>()) {
        if (combo->accessibleName() == QStringLiteral("Microphone profile")) {
            return combo;
        }
    }
    return nullptr;
}

QStringList items(const QComboBox* combo)
{
    QStringList out;
    for (int i = 0; i < combo->count(); ++i) {
        out.append(combo->itemText(i));
    }
    return out;
}

}  // namespace

class TstRemoteTxProfiles : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void txAndPhoneCombosPickTheCoresProfiles();
    void aRefusedPickLeavesEveryComboOnTheCoresProfile();
    void profileCombosFollowTheirGate();
    void radeProfileAndResetVocoderActOnTheCore();
    void radeResetVocoderRefusals();
    void txProfilePageWorksOnTheCoresProfiles();
    void txProfilePageGatesEachControlOnItsVersion();
    void setupOpensTxProfileWithoutRemoteTransmit();
    void microphoneSettingsReachTheCore();
    void microphoneSettingsAreTakenOnTheAir();
    void txInputPageGatesTheMicrophoneOnVersion3();
    void hl2RadioMicAndLineInStepsFromTheWindow();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteTxProfiles::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("remote-tx-profiles-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
}

void TstRemoteTxProfiles::init()
{
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemoteTxProfiles::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteTxProfiles::txAndPhoneCombosPickTheCoresProfiles()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const QStringList coreNames = s.core->micProfileManager()->profileNames();

    TxApplet tx(&s.window);
    tx.setMicProfileManager(s.window.micProfileManager());
    tx.setTxProfilePermitted(true);
    PhoneCwApplet phone(&s.window);
    phone.setTxProfilePermitted(true);
    QComboBox* const txCombo = tx.profileCombo();
    QComboBox* const phoneCombo = phoneProfileCombo(phone);
    QVERIFY(txCombo && phoneCombo);
    QVERIFY(txCombo->isEnabled());
    QVERIFY(phoneCombo->isEnabled());
    QCOMPARE(items(txCombo), coreNames);
    QCOMPARE(items(phoneCombo), coreNames);
    QCOMPARE(txCombo->currentText(), QStringLiteral("Default"));
    QCOMPARE(phoneCombo->currentText(), QStringLiteral("Default"));

    // The TX applet's combo selects through txProfile.select.
    txCombo->setCurrentText(QStringLiteral("AM"));
    QTRY_COMPARE(s.coreActive(), QStringLiteral("AM"));
    QTRY_COMPARE(phoneCombo->currentText(), QStringLiteral("AM"));
    QCOMPARE(txCombo->currentText(), QStringLiteral("AM"));

    // The Phone/CW applet's does too; both show the Core's active profile.
    phoneCombo->setCurrentText(QStringLiteral("Default DX"));
    QTRY_COMPARE(s.coreActive(), QStringLiteral("Default DX"));
    QTRY_COMPARE(txCombo->currentText(), QStringLiteral("Default DX"));

    // A profile saved at the Core appears in both lists.
    QVERIFY(s.core->micProfileManager()->saveProfile(QStringLiteral("Late"),
                                                     &s.core->transmitModel()));
    QTRY_VERIFY(items(txCombo).contains(QStringLiteral("Late")));
    QTRY_VERIFY(items(phoneCombo).contains(QStringLiteral("Late")));
    QVERIFY(!s.core->moxController()->isMox());
}

void TstRemoteTxProfiles::aRefusedPickLeavesEveryComboOnTheCoresProfile()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TxApplet tx(&s.window);
    tx.setMicProfileManager(s.window.micProfileManager());
    tx.setTxProfilePermitted(true);
    PhoneCwApplet phone(&s.window);
    phone.setTxProfilePermitted(true);
    RadeApplet rade(&s.window);
    rade.setTxProfilePermitted(true);
    QComboBox* const txCombo = tx.profileCombo();
    QComboBox* const phoneCombo = phoneProfileCombo(phone);
    QComboBox* const radeCombo = rade.profileComboForTest();

    // Put the RADE combo on the Core's profile (it opens on the RADE preset).
    emit s.window.micProfileManager()->activeProfileChanged(QStringLiteral("Default"));
    QCOMPARE(radeCombo->currentText(), QStringLiteral("Default"));

    // The window's gate would be closed on the air; a pick that races the
    // key reaches the Core and is refused.
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    QSignalSpy toast(&s.window, &RadioModel::sliceAddRejected);
    txCombo->setCurrentText(QStringLiteral("AM"));
    QTRY_VERIFY(!toast.isEmpty());
    QCOMPARE(toast.last().at(0).toString(), kOnAir);
    QTRY_COMPARE(txCombo->currentText(), QStringLiteral("Default"));
    QCOMPARE(phoneCombo->currentText(), QStringLiteral("Default"));
    QCOMPARE(radeCombo->currentText(), QStringLiteral("Default"));
    QCOMPARE(s.coreActive(), QStringLiteral("Default"));

    emit radeCombo->textActivated(QStringLiteral("AM"));
    QTRY_VERIFY(toast.count() >= 2);
    QTRY_COMPARE(radeCombo->currentText(), QStringLiteral("Default"));
    QCOMPARE(s.coreActive(), QStringLiteral("Default"));
    s.unkeyCore();
}

void TstRemoteTxProfiles::profileCombosFollowTheirGate()
{
    RadioModel window{RadioModel::Role::Remote};
    TxApplet tx(&window);
    tx.setMicProfileManager(window.micProfileManager());
    PhoneCwApplet phone(&window);
    RadeApplet rade(&window);
    QComboBox* const txCombo = tx.profileCombo();
    QComboBox* const phoneCombo = phoneProfileCombo(phone);
    QComboBox* const radeCombo = rade.profileComboForTest();
    QPushButton* const reset = rade.resetVocoderButtonForTest();

    // A remote window starts with them closed, with the plain reason.
    for (QWidget* control : {static_cast<QWidget*>(txCombo), static_cast<QWidget*>(phoneCombo),
                             static_cast<QWidget*>(radeCombo), static_cast<QWidget*>(reset)}) {
        QVERIFY(!control->isEnabled());
        QVERIFY2(OperatorWording::isPlain(control->toolTip()), qPrintable(control->toolTip()));
    }

    // The profile gate opens them whatever remote transmit says.
    tx.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    phone.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    rade.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    tx.setTxProfilePermitted(true);
    phone.setTxProfilePermitted(true);
    rade.setTxProfilePermitted(true);
    QVERIFY(txCombo->isEnabled());
    QVERIFY(phoneCombo->isEnabled());
    QVERIFY(radeCombo->isEnabled());
    QVERIFY(reset->isEnabled());

    // On the air they grey with its reason.
    tx.setTxProfilePermitted(false, kOnAir);
    phone.setTxProfilePermitted(false, kOnAir);
    rade.setTxProfilePermitted(false, kOnAir);
    for (QWidget* control : {static_cast<QWidget*>(txCombo), static_cast<QWidget*>(phoneCombo),
                             static_cast<QWidget*>(radeCombo), static_cast<QWidget*>(reset)}) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), kOnAir);
    }
}

void TstRemoteTxProfiles::radeProfileAndResetVocoderActOnTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const coreSlice = s.core->activeSlice();
    QVERIFY(coreSlice);
    RadeChannel* const channel =
        s.core->wdspEngine()->createRadeChannel(coreSlice->sliceIndex());
    QVERIFY(channel);
    QCOMPARE(channel->resetTxCountForTest(), 0);

    RadeApplet rade(&s.window);
    rade.setTxProfilePermitted(true);
    QComboBox* const combo = rade.profileComboForTest();
    QPushButton* const reset = rade.resetVocoderButtonForTest();
    QVERIFY(items(combo).contains(QStringLiteral("RADE")));
    QVERIFY(reset->isEnabled());

    // The profile combo picks the Core's profile.
    emit combo->textActivated(QStringLiteral("RADE"));
    QTRY_COMPARE(s.coreActive(), QStringLiteral("RADE"));

    // Reset vocoder resets the Core's RADE transmit vocoder and keys nothing.
    QSignalSpy moxChanges(s.core->moxController(), &MoxController::moxStateChanged);
    reset->click();
    QTRY_COMPARE(channel->resetTxCountForTest(), 1);
    QVERIFY(!s.core->moxController()->isMox());
    QVERIFY(!s.core->transmitModel().isMox());
    QVERIFY(!s.core->transmitModel().isTune());
    QVERIFY(moxChanges.isEmpty());

    s.core->wdspEngine()->destroyRadeChannel(coreSlice->sliceIndex());
}

void TstRemoteTxProfiles::radeResetVocoderRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const coreSlice = s.core->activeSlice();
    QVERIFY(coreSlice);
    RadeApplet rade(&s.window);
    rade.setTxProfilePermitted(true);
    QSignalSpy toast(&s.window, &RadioModel::sliceAddRejected);

    // No RADE channel on the Core's active slice.
    rade.resetVocoderButtonForTest()->click();
    QTRY_VERIFY(!toast.isEmpty());
    QCOMPARE(toast.last().at(0).toString(),
             QStringLiteral("RADE is not running on the Core's active slice."));

    RadeChannel* const channel =
        s.core->wdspEngine()->createRadeChannel(coreSlice->sliceIndex());
    QVERIFY(channel);
    // The Core's own key holds transmit: the reset is the holder's (ruling
    // 7.7), so this window's waits.
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    rade.resetVocoderButtonForTest()->click();
    QTRY_VERIFY(toast.count() >= 2);
    QCOMPARE(toast.last().at(0).toString(), kOnAir);
    QCOMPARE(channel->resetTxCountForTest(), 0);
    s.unkeyCore();

    // An argument the verb does not take.
    const quint32 id = 94999;
    s.windowEnd->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "rade.resetVocoder", id,
        {MirrorUpdate{0, "conformanceWrongName", MirrorWireKind::Bool, QVariant(true)}})));
    QTRY_VERIFY([&] {
        for (const QByteArray& wire : s.windowEnd->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandId == id) {
                return !message.accepted
                    && message.reason
                        == QStringLiteral("The request to reset the RADE vocoder was not understood.");
            }
        }
        return false;
    }());
    QCOMPARE(channel->resetTxCountForTest(), 0);
    s.core->wdspEngine()->destroyRadeChannel(coreSlice->sliceIndex());
}

void TstRemoteTxProfiles::txProfilePageWorksOnTheCoresProfiles()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    MicProfileManager* const coreProfiles = s.core->micProfileManager();
    TxProfileSetupPage page(&s.window, s.window.micProfileManager(),
                            &s.window.transmitModel());
    // As SetupDialog does for a Core at transmitSettingsVersion 3, off the air.
    page.setTransmitSettingsPermitted(true, QString());
    page.setTransmitSettingsPermittedAt(2, true, QString());
    page.setTransmitSettingsPermittedAt(3, true, QString());
    QCOMPARE(items(page.profileCombo()), coreProfiles->profileNames());
    QCOMPARE(page.profileCombo()->currentText(), QStringLiteral("Default"));

    // Pick a profile (no unsaved changes to keep).
    page.setUnsavedPromptHook([](const QString&) {
        return TxProfileSetupPage::UnsavedPromptResult::No;
    });
    page.simulateUserComboChangeForTest(QStringLiteral("AM"));
    QTRY_COMPARE(coreProfiles->activeProfileName(), QStringLiteral("AM"));

    // Save... under a new name.
    page.setSavePromptHook([](const QString&) {
        return TxProfileSetupPage::SavePromptResult{true, QStringLiteral("From The Window")};
    });
    page.saveButton()->click();
    QTRY_VERIFY(coreProfiles->profileNames().contains(QStringLiteral("From The Window")));
    QTRY_VERIFY(items(page.profileCombo()).contains(QStringLiteral("From The Window")));

    // Save... over an existing name asks first, then overwrites that one.
    bool askedToOverwrite = false;
    page.setOverwriteConfirmHook([&askedToOverwrite] {
        askedToOverwrite = true;
        return true;
    });
    const int count = coreProfiles->profileNames().size();
    page.saveButton()->click();
    QVERIFY(askedToOverwrite);
    QTest::qWait(100);
    QCOMPARE(coreProfiles->profileNames().size(), count);

    // Delete removes the selected profile (the Core's active one, AM).
    page.setDeleteConfirmHook([] { return true; });
    QCOMPARE(page.profileCombo()->currentText(), QStringLiteral("AM"));
    page.deleteButton()->click();
    QTRY_VERIFY(!coreProfiles->profileNames().contains(QStringLiteral("AM")));
    QTRY_VERIFY(!items(page.profileCombo()).contains(QStringLiteral("AM")));

    // TX filter and AM carrier are the Core's settings.
    page.filterLowSpin()->setValue(150);
    QTRY_COMPARE(s.core->transmitModel().filterLow(), 150);
    page.filterHighSpin()->setValue(3200);
    QTRY_COMPARE(s.core->transmitModel().filterHigh(), 3200);
    const int carrier = s.core->transmitModel().amCarrierLevel() == 60 ? 61 : 60;
    page.amCarrierSpin()->setValue(carrier);
    QTRY_COMPARE(s.core->transmitModel().amCarrierLevel(), carrier);

    // The last profile stays, with the page's own words.
    for (const QString& name : coreProfiles->profileNames()) {
        if (name != coreProfiles->activeProfileName()) {
            QVERIFY(coreProfiles->deleteProfile(name));
        }
    }
    QTRY_COMPARE(page.profileCombo()->count(), 1);
    QString rejection;
    page.setRejectionMessageHook([&rejection](const QString& message) { rejection = message; });
    page.deleteButton()->click();
    QCOMPARE(rejection,
             QStringLiteral("It is not possible to delete the last remaining TX profile"));
    QCOMPARE(coreProfiles->profileNames().size(), 1);
    QVERIFY(!s.core->moxController()->isMox());
}

void TstRemoteTxProfiles::txProfilePageGatesEachControlOnItsVersion()
{
    RadioModel window{RadioModel::Role::Remote};
    TxProfileSetupPage page(&window, window.micProfileManager(), &window.transmitModel());
    const QList<QWidget*> profileControls{page.profileCombo(), page.saveButton(),
                                          page.deleteButton()};
    // A remote window's page starts closed, with the plain reason, until
    // the Core says it takes them.
    for (QWidget* control : profileControls + QList<QWidget*>{page.filterLowSpin(),
                                                              page.amCarrierSpin()}) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    }
    page.setTransmitSettingsPermitted(true, QString());
    page.setTransmitSettingsPermittedAt(2, true, QString());
    page.setTransmitSettingsPermittedAt(3, false, kOnAir);
    for (QWidget* control : profileControls) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), kOnAir);
    }
    QVERIFY(page.filterLowSpin()->isEnabled());
    QVERIFY(page.amCarrierSpin()->isEnabled());

    page.setTransmitSettingsPermitted(false, kOnAir);
    QVERIFY(!page.filterLowSpin()->isEnabled());
    QVERIFY(!page.filterHighSpin()->isEnabled());
    QVERIFY(page.amCarrierSpin()->isEnabled());
    page.setTransmitSettingsPermittedAt(2, false, kOnAir);
    QVERIFY(!page.amCarrierSpin()->isEnabled());

    page.setTransmitSettingsPermittedAt(3, true, QString());
    page.setTransmitSettingsPermittedAt(2, true, QString());
    page.setTransmitSettingsPermitted(true, QString());
    for (QWidget* control : profileControls) {
        QVERIFY(control->isEnabled());
    }
    QVERIFY(page.filterLowSpin()->isEnabled());
    QVERIFY(page.amCarrierSpin()->isEnabled());

    // A Core below version 3 says why, in plain words.
    page.setTransmitSettingsPermittedAt(3, false, QString());
    QCOMPARE(page.saveButton()->toolTip(), IStationLink::transmitSettingsUnavailableReason());
}

void TstRemoteTxProfiles::setupOpensTxProfileWithoutRemoteTransmit()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SetupDialog dialog(&s.window);
    dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    dialog.setStationSettingsAvailable(true, QString());
    dialog.setTransmitSettingsPermitted(true, QString());
    dialog.setTransmitSettingsPermitted(true, QString(), 2);
    dialog.setTransmitSettingsPermitted(true, QString(), 3);
    dialog.selectPage(QStringLiteral("TX Profile"));
    auto* const page = dialog.findChild<TxProfileSetupPage*>();
    QVERIFY(page);
    QVERIFY(page->isEnabledTo(&dialog));
    QVERIFY(page->profileCombo()->isEnabledTo(&dialog));
    QVERIFY(page->saveButton()->isEnabledTo(&dialog));
    QTRY_COMPARE(items(page->profileCombo()),
                 s.core->micProfileManager()->profileNames());

    // On the air the dialog passes the reason to the page's controls.
    dialog.setTransmitSettingsPermitted(false, kOnAir, 3);
    QVERIFY(!page->saveButton()->isEnabledTo(&dialog));
    QCOMPARE(page->saveButton()->toolTip(), kOnAir);
    // The TX filter keeps the version 1 gate.
    QVERIFY(page->filterLowSpin()->isEnabledTo(&dialog));
}

void TstRemoteTxProfiles::microphoneSettingsReachTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();

    struct Flag {
        const char* name;
        std::function<void(TransmitModel&, bool)> set;
        std::function<bool(const TransmitModel&)> get;
    };
    const QList<Flag> flags{
        {"micBoost", [](TransmitModel& t, bool v) { t.setMicBoost(v); },
         [](const TransmitModel& t) { return t.micBoost(); }},
        {"micXlr", [](TransmitModel& t, bool v) { t.setMicXlr(v); },
         [](const TransmitModel& t) { return t.micXlr(); }},
        {"micTipRing", [](TransmitModel& t, bool v) { t.setMicTipRing(v); },
         [](const TransmitModel& t) { return t.micTipRing(); }},
        {"micBias", [](TransmitModel& t, bool v) { t.setMicBias(v); },
         [](const TransmitModel& t) { return t.micBias(); }},
        {"micPttDisabled", [](TransmitModel& t, bool v) { t.setMicPttDisabled(v); },
         [](const TransmitModel& t) { return t.micPttDisabled(); }},
        {"lineIn", [](TransmitModel& t, bool v) { t.setLineIn(v); },
         [](const TransmitModel& t) { return t.lineIn(); }},
    };
    for (const Flag& flag : flags) {
        // The window shows the Core's value first.
        QTRY_VERIFY2(flag.get(windowTx) == flag.get(coreTx), flag.name);
        const bool want = !flag.get(coreTx);
        flag.set(windowTx, want);
        QTRY_VERIFY2(flag.get(coreTx) == want, flag.name);
        QTRY_VERIFY2(flag.get(windowTx) == want, flag.name);
    }

    const double boost = coreTx.lineInBoost() == -12.0 ? -11.5 : -12.0;
    windowTx.setLineInBoost(boost);
    QTRY_COMPARE(coreTx.lineInBoost(), boost);
    QTRY_COMPARE(windowTx.lineInBoost(), boost);

    // A Core-side change shows in the window.
    coreTx.setMicBias(!coreTx.micBias());
    QTRY_COMPARE(windowTx.micBias(), coreTx.micBias());

    // Out of range is refused whole, with the range, and the Core keeps its value.
    for (const double bad : {-35.0, 12.5}) {
        const SessionPropertyResult result =
            s.writeTransmit("lineInBoost", MirrorWireKind::Float64, QVariant(bad));
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose a Line In gain from -34.5 to 12.0 dB."));
        QCOMPARE(coreTx.lineInBoost(), boost);
    }
    // The ends of the range are taken.
    QVERIFY(s.writeTransmit("lineInBoost", MirrorWireKind::Float64, QVariant(-34.5)).accepted);
    QTRY_COMPARE(coreTx.lineInBoost(), -34.5);
    QVERIFY(s.writeTransmit("lineInBoost", MirrorWireKind::Float64, QVariant(12.0)).accepted);
    QTRY_COMPARE(coreTx.lineInBoost(), 12.0);

    // The profile outputs are the Core's reports.
    QCOMPARE(s.writeTransmit("activeTxProfile", MirrorWireKind::Utf8,
                             QVariant(QStringLiteral("AM"))).reason,
             QStringLiteral("The Core sets this itself; it cannot be changed from here."));
    QVERIFY(!s.core->moxController()->isMox());
}

void TstRemoteTxProfiles::microphoneSettingsAreTakenOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const bool before = coreTx.micBoost();
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    // Remote parity on the air (transmitSettingsVersion 13): Setup > Audio
    // > TX Input changes them while a local window transmits.
    for (const QByteArray name : {QByteArrayLiteral("micXlr"),
                                  QByteArrayLiteral("micTipRing"), QByteArrayLiteral("micBias"),
                                  QByteArrayLiteral("micPttDisabled"),
                                  QByteArrayLiteral("lineIn")}) {
        const SessionPropertyResult result =
            s.writeTransmit(name, MirrorWireKind::Bool, QVariant(true));
        QVERIFY2(result.accepted, qPrintable(QString::fromUtf8(name) + QStringLiteral(": ")
                                             + result.reason));
    }
    QVERIFY(s.writeTransmit("lineInBoost", MirrorWireKind::Float64, QVariant(3.0)).accepted);
    // The window's own change reaches the Core too.
    s.window.transmitModel().setMicBoost(!before);
    QTRY_COMPARE(coreTx.micBoost(), !before);
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
}

void TstRemoteTxProfiles::txInputPageGatesTheMicrophoneOnVersion3()
{
    RadioModel window{RadioModel::Role::Remote};
    AudioTxInputPage page(&window);
    QSlider* const gain = page.micGainSlider();
    QVERIFY(gain);
    const QString noTransmit = QStringLiteral("Remote transmit is unavailable");

    // Remote transmit closed, transmit settings open: Mic Gain and the
    // radio microphone groups are live, the mic source is not (C2).
    page.setTransmitPermitted(false, noTransmit);
    page.setTransmitSettingsPermittedAt(3, true, QString());
    QVERIFY(gain->isEnabled());
    QVERIFY(!page.micSourceGroup()->isEnabled());
    QCOMPARE(page.micSourceGroup()->toolTip(), noTransmit);
    for (QGroupBox* group : {page.hermesRadioMicGroup(), page.orionRadioMicGroup(),
                             page.saturnRadioMicGroup()}) {
        if (group) {
            QVERIFY(group->isEnabled() || !group->property("SetupPageSavedTransmitEnabled")
                                                .isValid());
        }
    }

    // On the air they grey with its reason.
    page.setTransmitSettingsPermittedAt(3, false, kOnAir);
    QVERIFY(!gain->isEnabled());
    QCOMPARE(gain->toolTip(), kOnAir);
    for (QGroupBox* group : {page.hermesRadioMicGroup(), page.orionRadioMicGroup(),
                             page.saturnRadioMicGroup()}) {
        if (group) {
            QVERIFY(!group->isEnabled());
            QCOMPARE(group->toolTip(), kOnAir);
        }
    }
    // Without the Core's settings, that reason comes first.
    page.setStationSettingsAvailable(false, QStringLiteral("Connect to the Core to change these."));
    QCOMPARE(gain->toolTip(), QStringLiteral("Connect to the Core to change these."));
    page.setStationSettingsAvailable(true, QString());
    page.setTransmitSettingsPermittedAt(3, true, QString());
    QVERIFY(gain->isEnabled());
    // Other versions do not touch them.
    page.setTransmitSettingsPermittedAt(2, false, kOnAir);
    QVERIFY(gain->isEnabled());
}

// Radio codec lane: the window's TX Input on the HL2 Core has Radio Mic
// open with the audio add-on note and the Hermes group, and its Line In
// Gain slider's 1.5 dB step reaches the Core.
void TstRemoteTxProfiles::hl2RadioMicAndLineInStepsFromTheWindow()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_COMPARE(s.window.boardCapabilities().board, HPSDRHW::HermesLite);
    QVERIFY(s.window.boardCapabilities().radioMicNeedsAddOn);
    AudioTxInputPage page(&s.window);
    QVERIFY(page.radioMicButton()->isEnabled());
    QCOMPARE(page.radioMicButton()->toolTip(), RadioModel::radioMicAddOnNote());
    QCOMPARE(page.hermesRadioMicGroup()->title(), QStringLiteral("Radio Mic (Hermes Lite 2)"));

    QSlider* const gain = page.hermesLineInGainSlider();
    QVERIFY(gain);
    const int next = gain->value() == -9 ? -12 : -9;
    gain->setValue(next);
    QTRY_COMPARE(s.core->transmitModel().lineInBoost(), next / 2.0);
    QTRY_COMPARE(s.window.transmitModel().lineInBoost(), next / 2.0);
}

QTEST_MAIN(TstRemoteTxProfiles)
#include "tst_remote_tx_profiles.moc"
