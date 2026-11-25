// no-port-check: NereusSDR-original. R-R3-49 (parity Task 3; iPhone plan
// Task 40): the Core's TX profiles on the `transmit` object and the
// txProfile.select / save / delete verbs. Loopback link, no RF and no
// hardware: nothing here keys a radio. "On the air" keys the Core's own
// MoxController with the receive-only MOX pre-check lifted, as
// tst_transmit_settings_gate and tst_transmit_model_properties do. No audio
// device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the Core
//                                    publishes its profiles, a window's
//                                    manager mirrors them and never saves,
//                                    and select, save and delete act on the
//                                    Core with their refusals.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Save and Delete are the holder's while
//                                    transmit is held (ruling 7.7); the
//                                    Core's own key holds it here.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-11-17  Brian Sutton (W4KZO)  txProfile.import stores and selects
//                                    a profile, with its refusals.
//                                    AI-assisted via xAI Grok.
//   2026-11-22  J.J. Boyd / KG4VCF  CI fix: the mirror's requester takes
//                                    the imported values. Review fixes: the
//                                    Core's refusals of a large, non-text,
//                                    badly named or out-of-range import and
//                                    of one with no settings; the wiring
//                                    keys stay the Core's; a remote
//                                    window's TX Profile page exports the
//                                    Core's stored profile, waits for the
//                                    Core's save and import, and leaves
//                                    Import disabled on a Core at version
//                                    16. Follow-up: a refused import or
//                                    export save shows on the page alone
//                                    and keeps the edits unsaved; a
//                                    dropped link forgets a waiting
//                                    export; the Core refuses a name with
//                                    two underscores in a row. AI tooling:
//                                    Claude Code.
//   2026-11-25  J.J. Boyd / KG4VCF  The page's claims by command id: an
//                                    ordinary save's answer never completes
//                                    an export, a second export waits, and
//                                    a closed page leaves the refusal to
//                                    the general notice; edits made during
//                                    the save, or behind a refused import,
//                                    stay unsaved; an active profile that
//                                    changed during the file dialog stops
//                                    the export. AI tooling: Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QComboBox>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "core/TxProfileFile.h"
#include "fakes/LoopbackTransport.h"
#include "gui/setup/TxProfileSetupPage.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
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
    // As a connect does: the profile bank for this radio.
    model->scopeTxProfiles(kMac);
    return model;
}

QString storedField(const QString& profile, const QString& field)
{
    return AppSettings::instance()
        .value(QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(kMac, profile, field))
        .toString();
}

// A receive-only Core and one window, handshake complete.
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
        return completed.wait(5000) || completed.count() == 1;
    }
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        found.reason = QStringLiteral("no command.result arrived");
        (void)QTest::qWaitFor([&] {
            for (const QByteArray& wire : windowEnd->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::CommandResult
                    && message.commandId == id) {
                    found = message;
                    return true;
                }
            }
            return false;
        }, 3000);
        return found;
    }
    void keyCore()
    {
        MoxController* const mox = core->moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
    }
    void unkeyCore() { core->moxController()->setMox(false); }
    MicProfileManager& coreProfiles() { return *core->micProfileManager(); }
    MicProfileManager& windowProfiles() { return *window.micProfileManager(); }

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
    quint32 nextId = 93000;
};

MirrorUpdate nameArg(const QString& value, const QByteArray& argName = "name")
{
    return MirrorUpdate{0, argName, MirrorWireKind::Utf8, QVariant(value)};
}

}  // namespace

class TstTxProfileSelect : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void profilesAreOnTheLinkAfterTheEarlierSettings();
    void coreOffersTransmitSettingsVersion3();
    void theWindowShowsTheCoresProfiles();
    void aMirrorNeverKeepsProfilesHere();
    void selectAppliesTheProfileOnTheCore();
    void selectRefusals();
    void saveStoresTheCoresSettingsUnderThatName();
    void saveRefusals();
    void deleteRemovesTheProfile();
    void deleteRefusals();
    void importStoresTheProfileAndSelectsIt();
    void importRefusals();
    void importRefusalsTheCoreChecks();
    void importKeepsTheCoresWiring();
    void saveRefusesABadName();
    void remoteExportReadsTheCoresStoredProfile();
    void remoteExportYesWaitsForTheCoresSave();
    void remoteImportConfirmsOnceTheCoreAnswers();
    void remoteImportRefusalShowsOnThePage();
    void remoteExportSaveRefusalShowsOnThePageAlone();
    void aDroppedLinkForgetsTheWaitingExport();
    void remoteExportStopsWhenTheActiveProfileChanges();
    void aSecondExportWaitsForTheFirst();
    void anOrdinarySavesAnswerDoesNotCompleteAnExport();
    void editsDuringTheExportsSaveStayUnsaved();
    void aRefusedImportKeepsTheEditsUnsaved();
    void aClosedPageLeavesTheRefusalToTheWindow();
    void doubleUnderscoreProfilesSelectAndDelete();
    void importIsDisabledOnACoreAtVersion16();
    void wrongArgumentsAreNotUnderstood();
    void aRefusedSelectShowsTheCoresProfileAgain();
    void anUnconnectedWindowAsksNothing();
    void refusalsArePlainWords();

private:
    QTemporaryDir m_securityDir;
};

void TstTxProfileSelect::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("tx-profile-select-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
}

void TstTxProfileSelect::init()
{
    // Each case starts from the factory profile bank.
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstTxProfileSelect::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstTxProfileSelect::profilesAreOnTheLinkAfterTheEarlierSettings()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const struct {
        const char* name;
        MirrorWireKind kind;
        bool twoWay;
    } expected[] = {
        {"micBoost", MirrorWireKind::Bool, true},
        {"micXlr", MirrorWireKind::Bool, true},
        {"micTipRing", MirrorWireKind::Bool, true},
        {"micBias", MirrorWireKind::Bool, true},
        {"micPttDisabled", MirrorWireKind::Bool, true},
        {"lineIn", MirrorWireKind::Bool, true},
        {"lineInBoost", MirrorWireKind::Float64, true},
        {"activeTxProfile", MirrorWireKind::Utf8, false},
        {"txProfilesJson", MirrorWireKind::Utf8, false},
    };
    // After tuneDrivePowerSource (parity Task 2's last), in this order.
    const MirrorProperty* last = schema.byName("tuneDrivePowerSource");
    QVERIFY(last);
    quint16 ordinal = last->ordinal;
    for (const auto& e : expected) {
        const MirrorProperty* prop = schema.byName(e.name);
        QVERIFY2(prop, e.name);
        QCOMPARE(prop->kind, e.kind);
        QCOMPARE(prop->ordinal, ++ordinal);
        QCOMPARE(MirrorPolicy::inboundAllowed("TransmitModel", e.name), e.twoWay);
        QCOMPARE(prop->isWritable, e.twoWay);
    }
}

void TstTxProfileSelect::coreOffersTransmitSettingsVersion3()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    // At least 3 (parity Task 4 raised it to 4).
    QVERIFY(s.server->buildCapabilities().transmitSettingsVersion >= 3);
    QVERIFY(s.client->capabilities().transmitSettingsVersion >= 3);
    QVERIFY(s.client->transmitSettingsAvailable(3));
}

void TstTxProfileSelect::theWindowShowsTheCoresProfiles()
{
    Session s(m_securityDir.path(), this);
    const QStringList coreNames = s.coreProfiles().profileNames();
    QVERIFY(coreNames.size() > 1);
    QCOMPARE(s.core->transmitModel().activeTxProfile(), QStringLiteral("Default"));
    QCOMPARE(TransmitModel::txProfileNamesFromJson(s.core->transmitModel().txProfilesJson()),
             coreNames);

    QVERIFY(s.connect());
    QVERIFY(s.windowProfiles().isStationMirror());
    QTRY_COMPARE(s.windowProfiles().profileNames(), coreNames);
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Default"));
    QCOMPARE(s.window.txProfile(), QStringLiteral("Default"));
    QCOMPARE(s.window.txProfilesList(), coreNames);

    // A change made at the Core reaches the window.
    QSignalSpy listChanged(&s.windowProfiles(), &MicProfileManager::profileListChanged);
    QVERIFY(s.coreProfiles().saveProfile(QStringLiteral("Made At The Core"),
                                         &s.core->transmitModel()));
    QTRY_VERIFY(s.windowProfiles().profileNames().contains(QStringLiteral("Made At The Core")));
    QVERIFY(listChanged.count() >= 1);
    QSignalSpy activeChanged(&s.windowProfiles(), &MicProfileManager::activeProfileChanged);
    QVERIFY(s.coreProfiles().setActiveProfile(QStringLiteral("AM"), &s.core->transmitModel()));
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("AM"));
    QVERIFY(!activeChanged.isEmpty());
    QCOMPARE(activeChanged.last().at(0).toString(), QStringLiteral("AM"));
}

void TstTxProfileSelect::aMirrorNeverKeepsProfilesHere()
{
    QStringList asked;
    MicProfileManager mirror;
    mirror.setStationMirror([&asked](MicProfileManager::StationRequest request,
                                     const QString& name,
                                     const QHash<QString, QVariant>&) {
        asked.append(QStringLiteral("%1:%2").arg(int(request)).arg(name));
        return true;
    });
    const QString mac = QStringLiteral("11:22:33:44:55:66");
    mirror.setMacAddress(mac);
    mirror.load();
    QVERIFY(!AppSettings::instance().contains(
        QStringLiteral("hardware/%1/tx/profile/_names").arg(mac)));
    QVERIFY(mirror.profileNames().isEmpty());

    mirror.applyStationProfiles({QStringLiteral("Default"), QStringLiteral("AM")});
    mirror.applyStationActiveProfile(QStringLiteral("Default"));
    TransmitModel tx;
    QVERIFY(mirror.setActiveProfile(QStringLiteral("AM"), &tx));
    // Nothing changes here until the Core reports it.
    QCOMPARE(mirror.activeProfileName(), QStringLiteral("Default"));
    QVERIFY(mirror.saveProfile(QStringLiteral("Mine"), &tx));
    QVERIFY(mirror.deleteProfile(QStringLiteral("AM")));
    QCOMPARE(asked, (QStringList{QStringLiteral("0:AM"), QStringLiteral("1:Mine"),
                                 QStringLiteral("2:AM")}));
    // The last-profile rule answers here as it does locally.
    mirror.applyStationProfiles({QStringLiteral("Default")});
    QVERIFY(!mirror.deleteProfile(QStringLiteral("Default")));
    QCOMPARE(asked.size(), 3);
    QVERIFY(!AppSettings::instance().contains(
        QStringLiteral("hardware/%1/tx/profile/Mine/MicGain").arg(mac)));
}

void TstTxProfileSelect::selectAppliesTheProfileOnTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    QCOMPARE(coreTx.filterLow(), 100);  // the Default profile's

    // As the window's TX applet combo does.
    QVERIFY(s.windowProfiles().setActiveProfile(QStringLiteral("Default DX"),
                                                &s.window.transmitModel()));
    QTRY_COMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Default DX"));
    // The profile's values are the Core's live transmit settings now, as a
    // local pick makes them (Default DX's TX filter is 200 to 3100 Hz).
    QCOMPARE(coreTx.filterLow(), storedField(QStringLiteral("Default DX"),
                                             QStringLiteral("FilterLow")).toInt());
    QCOMPARE(coreTx.filterLow(), 200);
    QCOMPARE(coreTx.filterHigh(), 3100);
    QTRY_COMPARE(s.window.transmitModel().activeTxProfile(), QStringLiteral("Default DX"));
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Default DX"));
    QTRY_COMPARE(s.window.transmitModel().filterLow(), 200);
    // Nothing keyed.
    QVERIFY(!s.core->moxController()->isMox());
    QVERIFY(!coreTx.isMox());
    QVERIFY(!coreTx.isTune());

    // The raw verb, as any app sends it.
    const SessionMessage result = s.invoke("txProfile.select", {nameArg(QStringLiteral("AM"))});
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QTRY_COMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("AM"));
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("AM"));
}

void TstTxProfileSelect::selectRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());

    SessionMessage result = s.invoke("txProfile.select", {nameArg(QStringLiteral("Nope"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("There is no transmit profile called Nope."));
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Default"));

    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    result = s.invoke("txProfile.select", {nameArg(QStringLiteral("AM"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, kOnAir);
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Default"));
    s.unkeyCore();
    QTRY_VERIFY(!s.core->moxController()->isMox());
    // Taken once the radio is off the air.
    QTRY_VERIFY(s.invoke("txProfile.select", {nameArg(QStringLiteral("AM"))}).accepted);
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("AM"));
}

void TstTxProfileSelect::saveStoresTheCoresSettingsUnderThatName()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();

    // A setting changed from the window, then saved under a new name.
    const int mic = coreTx.micGainDb() == 9 ? 10 : 9;
    s.window.transmitModel().setMicGainDb(mic);
    QTRY_COMPARE(coreTx.micGainDb(), mic);
    const QString amBefore = storedField(QStringLiteral("AM"), QStringLiteral("MicGain"));
    QVERIFY(s.windowProfiles().saveProfile(QStringLiteral("Contest"),
                                           &s.window.transmitModel()));
    QTRY_VERIFY(s.coreProfiles().profileNames().contains(QStringLiteral("Contest")));
    QCOMPARE(storedField(QStringLiteral("Contest"), QStringLiteral("MicGain")),
             QString::number(mic));
    QTRY_VERIFY(s.windowProfiles().profileNames().contains(QStringLiteral("Contest")));

    // The same name again overwrites that profile only.
    const int mic2 = mic + 1;
    s.window.transmitModel().setMicGainDb(mic2);
    QTRY_COMPARE(coreTx.micGainDb(), mic2);
    const int countBefore = s.coreProfiles().profileNames().size();
    const SessionMessage result = s.invoke("txProfile.save", {nameArg(QStringLiteral("Contest"))});
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QCOMPARE(storedField(QStringLiteral("Contest"), QStringLiteral("MicGain")),
             QString::number(mic2));
    QCOMPARE(s.coreProfiles().profileNames().size(), countBefore);
    QCOMPARE(storedField(QStringLiteral("AM"), QStringLiteral("MicGain")), amBefore);

    // A comma becomes an underscore, as the local Save does.
    QVERIFY(s.invoke("txProfile.save", {nameArg(QStringLiteral("A,B"))}).accepted);
    QTRY_VERIFY(s.windowProfiles().profileNames().contains(QStringLiteral("A_B")));
    QVERIFY(!s.core->moxController()->isMox());
}

void TstTxProfileSelect::saveRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const int count = s.coreProfiles().profileNames().size();

    SessionMessage result = s.invoke("txProfile.save", {nameArg(QStringLiteral("   "))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("Give the transmit profile a name."));

    // The Core's own key holds transmit: Save is the holder's (ruling 7.7).
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    result = s.invoke("txProfile.save", {nameArg(QStringLiteral("On Air"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, kOnAir);
    s.unkeyCore();
    QTRY_VERIFY(!s.core->moxController()->isMox());
    QCOMPARE(s.coreProfiles().profileNames().size(), count);
    QVERIFY(storedField(QStringLiteral("On Air"), QStringLiteral("MicGain")).isEmpty());
}

void TstTxProfileSelect::deleteRemovesTheProfile()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.invoke("txProfile.save", {nameArg(QStringLiteral("Temporary"))}).accepted);
    QTRY_VERIFY(s.windowProfiles().profileNames().contains(QStringLiteral("Temporary")));

    // As Setup > Audio > TX Profile's Delete does in the window.
    QVERIFY(s.windowProfiles().deleteProfile(QStringLiteral("Temporary")));
    QTRY_VERIFY(!s.coreProfiles().profileNames().contains(QStringLiteral("Temporary")));
    QTRY_VERIFY(!s.windowProfiles().profileNames().contains(QStringLiteral("Temporary")));
    QVERIFY(storedField(QStringLiteral("Temporary"), QStringLiteral("MicGain")).isEmpty());

    // A factory profile goes as it does locally.
    QVERIFY(s.invoke("txProfile.delete", {nameArg(QStringLiteral("AM"))}).accepted);
    QTRY_VERIFY(!s.windowProfiles().profileNames().contains(QStringLiteral("AM")));

    // Deleting the active profile moves the Core to another; the window follows.
    QVERIFY(s.invoke("txProfile.select", {nameArg(QStringLiteral("Default DX"))}).accepted);
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Default DX"));
    QVERIFY(s.invoke("txProfile.delete", {nameArg(QStringLiteral("Default DX"))}).accepted);
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), s.coreProfiles().activeProfileName());
    QVERIFY(s.coreProfiles().activeProfileName() != QStringLiteral("Default DX"));
}

void TstTxProfileSelect::deleteRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());

    SessionMessage result = s.invoke("txProfile.delete", {nameArg(QStringLiteral("Nope"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("There is no transmit profile called Nope."));

    // The Core's own key holds transmit: Delete is the holder's (ruling 7.7).
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    result = s.invoke("txProfile.delete", {nameArg(QStringLiteral("AM"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, kOnAir);
    QVERIFY(s.coreProfiles().profileNames().contains(QStringLiteral("AM")));
    s.unkeyCore();
    QTRY_VERIFY(!s.core->moxController()->isMox());

    // The last profile stays, with the local page's words.
    for (const QString& name : s.coreProfiles().profileNames()) {
        if (name != QStringLiteral("Default")) {
            QVERIFY(s.coreProfiles().deleteProfile(name));
        }
    }
    QTRY_COMPARE(s.windowProfiles().profileNames(), QStringList{QStringLiteral("Default")});
    result = s.invoke("txProfile.delete", {nameArg(QStringLiteral("Default"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason,
             QStringLiteral("It is not possible to delete the last remaining TX profile."));
    QCOMPARE(s.coreProfiles().profileNames(), QStringList{QStringLiteral("Default")});
    // The window's own delete says so without asking.
    QVERIFY(!s.windowProfiles().deleteProfile(QStringLiteral("Default")));
}

void TstTxProfileSelect::importStoresTheProfileAndSelectsIt()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.client->transmitSettingsAvailable(kTransmitSettingsTxProfileFileVersion));
    QHash<QString, QVariant> values;
    values.insert(QStringLiteral("MicGain"), QStringLiteral("-14"));
    values.insert(QStringLiteral("NotAProfileKey"), QStringLiteral("nope"));
    QVERIFY(s.windowProfiles().importProfile(QStringLiteral("Desk,Mic"), values,
                                             &s.window.transmitModel()));
    QTRY_COMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Desk_Mic"));
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Desk_Mic"));
    QCOMPARE(storedField(QStringLiteral("Desk_Mic"), QStringLiteral("MicGain")),
             QStringLiteral("-14"));
    QCOMPARE(s.core->transmitModel().micGainDb(), -14);
    QCOMPARE(storedField(QStringLiteral("Desk_Mic"), QStringLiteral("FilterLow")),
             MicProfileManager::defaultProfileValues().value(QStringLiteral("FilterLow")).toString());
    QVERIFY(storedField(QStringLiteral("Desk_Mic"), QStringLiteral("NotAProfileKey")).isEmpty());
}

void TstTxProfileSelect::importRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const int count = s.coreProfiles().profileNames().size();
    SessionMessage result = s.invoke("txProfile.import",
        {nameArg(QStringLiteral("   ")),
         nameArg(QStringLiteral("{}"), "keysJson")});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("Give the transmit profile a name."));

    result = s.invoke("txProfile.import",
        {nameArg(QStringLiteral("Broken")),
         nameArg(QStringLiteral("[]"), "keysJson")});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("The transmit profile was not understood."));

    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    result = s.invoke("txProfile.import",
        {nameArg(QStringLiteral("On Air")),
         nameArg(QStringLiteral("{\"MicGain\":\"-9\"}"), "keysJson")});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, kOnAir);
    s.unkeyCore();
    QCOMPARE(s.coreProfiles().profileNames().size(), count);
    QVERIFY(storedField(QStringLiteral("On Air"), QStringLiteral("MicGain")).isEmpty());
}

// PR 809 review: what the Core checks itself, whatever the window sent.
void TstTxProfileSelect::importRefusalsTheCoreChecks()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const int count = s.coreProfiles().profileNames().size();
    SessionMessage result;
    const struct {
        QString name;
        QString keysJson;
        QString reason;
    } refused[] = {
        {QStringLiteral("Huge"),
         QStringLiteral("{\"MicGain\":\"%1\"}").arg(QString(256 * 1024, QLatin1Char('1'))),
         QStringLiteral("The transmit profile is too large.")},
        {QStringLiteral("Numbers"), QStringLiteral("{\"MicGain\":-9}"),
         QStringLiteral("A setting in the transmit profile is not text.")},
        {QStringLiteral("Tab\tName"), QStringLiteral("{\"MicGain\":\"-9\"}"),
         QStringLiteral("Use a transmit profile name without tabs, line breaks or other "
                        "control characters.")},
        {QString(65, QLatin1Char('x')), QStringLiteral("{\"MicGain\":\"-9\"}"),
         QStringLiteral("Use a transmit profile name of 64 characters or fewer.")},
        {QStringLiteral("Loud"), QStringLiteral("{\"MicGain\":\"loud\"}"),
         QStringLiteral("Choose a mic level from -50 to 70 dB.")},
        {QStringLiteral("Leveler"), QStringLiteral("{\"Lev_On\":\"yes\"}"),
         QStringLiteral("The transmit profile's leveler setting is not on or off.")},
        {QStringLiteral("Curve"), QStringLiteral("{\"TXParaEQData\":\"zzz\"}"),
         QStringLiteral("The Core could not read that equalizer curve. Save the curve again "
                        "and retry.")},
        {QStringLiteral("Empty"), QStringLiteral("{}"),
         QStringLiteral("The transmit profile has no settings.")},
        {QStringLiteral("Newer"), QStringLiteral("{\"FutureKnob\":\"3\"}"),
         QStringLiteral("The transmit profile has no settings.")},
    };
    for (const auto& r : refused) {
        result = s.invoke("txProfile.import", {nameArg(r.name), nameArg(r.keysJson, "keysJson")});
        QVERIFY2(!result.accepted, qPrintable(r.reason));
        QCOMPARE(result.reason, r.reason);
        QVERIFY(OperatorWording::isPlain(result.reason));
    }
    QCOMPARE(s.coreProfiles().profileNames().size(), count);
}

void TstTxProfileSelect::importKeepsTheCoresWiring()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setMicXlr(true);
    coreTx.setMicPttDisabled(false);
    const SessionMessage result = s.invoke("txProfile.import",
        {nameArg(QStringLiteral("Wired")),
         nameArg(QStringLiteral("{\"MicGain\":\"-8\",\"Mic_XLR\":\"False\","
                                "\"Mic_PTT_Disabled\":\"True\"}"), "keysJson")});
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QCOMPARE(coreTx.micGainDb(), -8);
    QVERIFY(coreTx.micXlr());
    QVERIFY(!coreTx.micPttDisabled());
    QCOMPARE(storedField(QStringLiteral("Wired"), QStringLiteral("Mic_XLR")), QStringLiteral("True"));
}

void TstTxProfileSelect::saveRefusesABadName()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const int count = s.coreProfiles().profileNames().size();
    SessionMessage result = s.invoke("txProfile.save", {nameArg(QStringLiteral("Line\nBreak"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral(
        "Use a transmit profile name without tabs, line breaks or other control characters."));
    result = s.invoke("txProfile.save", {nameArg(QString(65, QLatin1Char('y')))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral("Use a transmit profile name of 64 characters or fewer."));
    result = s.invoke("txProfile.save", {nameArg(QStringLiteral("plan c"))});
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason,
             QStringLiteral("The settings file cannot keep this transmit profile name. "
                            "Use a different name."));
    QCOMPARE(s.coreProfiles().profileNames().size(), count);
    // The window does not send one.
    QVERIFY(!s.windowProfiles().saveProfile(QStringLiteral("Tab\tName"), &s.window.transmitModel()));
}

namespace {

QString writeProfileFile(const QTemporaryDir& dir, const QString& name, const QJsonObject& keys)
{
    const QJsonObject root{{QStringLiteral("nereusTxProfileVersion"), TxProfileFile::kVersion},
                           {QStringLiteral("name"), name},
                           {QStringLiteral("keys"), keys}};
    const QString path = dir.filePath(QStringLiteral("in.nereus-tx-profile"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QJsonDocument(root).toJson());
    return path;
}

TxProfileFile::Document readProfileFile(const QString& path)
{
    TxProfileFile::Document document;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        TxProfileFile::decode(file.readAll(), &document, nullptr);
    }
    return document;
}

}  // namespace

void TstTxProfileSelect::remoteExportReadsTheCoresStoredProfile()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setMicGainDb(-13);
    QVERIFY(s.coreProfiles().saveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.coreProfiles().setActiveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.connect());
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Desk"));
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    QCOMPARE(page.profileCombo()->currentText(), QStringLiteral("Desk"));
    // The Core's live level moves on; its stored profile does not.
    coreTx.setMicGainDb(-2);
    QTRY_COMPARE(s.window.transmitModel().micGainDb(), -2);
    page.setUnsavedPromptHook([](const QString&) {
        return TxProfileSetupPage::UnsavedPromptResult::No;
    });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Desk.nereus-tx-profile"));
    page.setExportPathHook([path] { return path; });
    bool ok = false;
    QString shown;
    page.setFileResultHook([&](bool success, const QString& text) {
        ok = success;
        shown = text;
    });
    page.exportButton()->click();
    QVERIFY2(ok, qPrintable(shown));
    const TxProfileFile::Document document = readProfileFile(path);
    QCOMPARE(document.name, QStringLiteral("Desk"));
    QCOMPARE(document.keys.value(QStringLiteral("MicGain")), QStringLiteral("-13"));
    QVERIFY(!document.keys.contains(QStringLiteral("Mic_XLR")));
}

void TstTxProfileSelect::remoteExportYesWaitsForTheCoresSave()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setMicGainDb(-13);
    QVERIFY(s.coreProfiles().saveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.coreProfiles().setActiveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.connect());
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Desk"));
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    coreTx.setMicGainDb(-2);
    QTRY_COMPARE(s.window.transmitModel().micGainDb(), -2);
    int asked = 0;
    page.setUnsavedPromptHook([&asked](const QString&) {
        ++asked;
        return TxProfileSetupPage::UnsavedPromptResult::Yes;
    });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Desk.nereus-tx-profile"));
    page.setExportPathHook([path] { return path; });
    int results = 0;
    bool ok = false;
    page.setFileResultHook([&](bool success, const QString&) {
        ++results;
        ok = success;
    });
    page.exportButton()->click();
    QCOMPARE(asked, 1);
    // Nothing is written before the Core has saved.
    QVERIFY(!QFile::exists(path));
    QCOMPARE(results, 0);
    QTRY_COMPARE(results, 1);
    QVERIFY(ok);
    QCOMPARE(storedField(QStringLiteral("Desk"), QStringLiteral("MicGain")), QStringLiteral("-2"));
    QCOMPARE(readProfileFile(path).keys.value(QStringLiteral("MicGain")), QStringLiteral("-2"));
}

void TstTxProfileSelect::remoteImportConfirmsOnceTheCoreAnswers()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(!s.windowProfiles().profileNames().isEmpty());
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    // As MainWindow's gate does for a Core at version 17.
    page.setTransmitSettingsPermittedAt(kTransmitSettingsTxProfileFileVersion, true, {});
    QVERIFY(page.importButton()->isEnabled());
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeProfileFile(dir, QStringLiteral("From File"),
        QJsonObject{{QStringLiteral("MicGain"), QStringLiteral("-19")},
                    {QStringLiteral("FutureKnob"), QStringLiteral("1")}});
    page.setImportPathHook([path] { return path; });
    int results = 0;
    bool ok = false;
    QString shown;
    page.setFileResultHook([&](bool success, const QString& text) {
        ++results;
        ok = success;
        shown = text;
    });
    page.importButton()->click();
    QCOMPARE(results, 0);
    QTRY_COMPARE(results, 1);
    QVERIFY2(ok, qPrintable(shown));
    QCOMPARE(shown, QStringLiteral("Imported \"From File\" and selected it.\n\n"
                                   "One setting this version of NereusSDR does not know was left out."));
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("From File"));
    QCOMPARE(s.core->transmitModel().micGainDb(), -19);
}

void TstTxProfileSelect::remoteImportRefusalShowsOnThePage()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(!s.windowProfiles().profileNames().isEmpty());
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    // As MainWindow's gate does for a Core at version 17.
    page.setTransmitSettingsPermittedAt(kTransmitSettingsTxProfileFileVersion, true, {});
    QVERIFY(page.importButton()->isEnabled());
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeProfileFile(dir, QStringLiteral("On Air"),
        QJsonObject{{QStringLiteral("MicGain"), QStringLiteral("-19")}});
    page.setImportPathHook([path] { return path; });
    int results = 0;
    bool ok = true;
    QString shown;
    page.setFileResultHook([&](bool success, const QString& text) {
        ++results;
        ok = success;
        shown = text;
    });
    QSignalSpy chooser(&s.window, &RadioModel::sliceAddRejected);
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    page.importButton()->click();
    QTRY_COMPARE(results, 1);
    s.unkeyCore();
    QVERIFY(!ok);
    QCOMPARE(shown, kOnAir);
    QVERIFY(!s.coreProfiles().profileNames().contains(QStringLiteral("On Air")));
    // The page's message is the only one: no toast, no word to the
    // receive chooser.
    QCOMPARE(chooser.count(), 0);
}

// The save an export waits on: its refusal shows on the page alone, no file
// is written, and the edits stay unsaved.
void TstTxProfileSelect::remoteExportSaveRefusalShowsOnThePageAlone()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setMicGainDb(-13);
    QVERIFY(s.coreProfiles().saveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.coreProfiles().setActiveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.connect());
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Desk"));
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    coreTx.setMicGainDb(-2);
    QTRY_COMPARE(s.window.transmitModel().micGainDb(), -2);
    int asked = 0;
    page.setUnsavedPromptHook([&asked](const QString&) {
        ++asked;
        return TxProfileSetupPage::UnsavedPromptResult::Yes;
    });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Desk.nereus-tx-profile"));
    page.setExportPathHook([path] { return path; });
    int results = 0;
    bool ok = true;
    QString shown;
    page.setFileResultHook([&](bool success, const QString& text) {
        ++results;
        ok = success;
        shown = text;
    });
    QSignalSpy chooser(&s.window, &RadioModel::sliceAddRejected);
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    page.exportButton()->click();
    QCOMPARE(asked, 1);
    QTRY_COMPARE(results, 1);
    s.unkeyCore();
    QVERIFY(!ok);
    QCOMPARE(shown, kOnAir);
    QVERIFY(!QFile::exists(path));
    QCOMPARE(chooser.count(), 0);
    QVERIFY(!s.windowProfiles().stationRequestClaimed(
        s.windowProfiles().lastStationRequestId()));
    // Still unsaved: the next export asks again.
    page.setExportPathHook([] { return QString(); });
    page.exportButton()->click();
    QCOMPARE(asked, 2);
}

// The link drops before the Core answers the save an export waits on: a
// later answer never writes that file.
void TstTxProfileSelect::aDroppedLinkForgetsTheWaitingExport()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    QVERIFY(s.coreProfiles().saveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.coreProfiles().setActiveProfile(QStringLiteral("Desk"), &coreTx));
    QVERIFY(s.connect());
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Desk"));
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    coreTx.setMicGainDb(-2);
    QTRY_COMPARE(s.window.transmitModel().micGainDb(), -2);
    page.setUnsavedPromptHook([](const QString&) {
        return TxProfileSetupPage::UnsavedPromptResult::Yes;
    });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Desk.nereus-tx-profile"));
    page.setExportPathHook([path] { return path; });
    int results = 0;
    page.setFileResultHook([&results](bool, const QString&) { ++results; });
    page.exportButton()->click();
    const quint32 id = s.windowProfiles().lastStationRequestId();
    QVERIFY(id != 0);
    QVERIFY(s.windowProfiles().stationRequestClaimed(id));
    // Before the answer can arrive.
    s.client->disconnectFromStation(QStringLiteral("test"));
    QVERIFY(!s.windowProfiles().stationRequestClaimed(id));
    QVERIFY(s.windowProfiles().isStationMirror());
    // A later save's answer.
    s.windowProfiles().reportStationRequestFinished(MicProfileManager::StationRequest::Save,
                                                    id, true, {});
    QTest::qWait(100);
    QCOMPARE(results, 0);
    QVERIFY(!QFile::exists(path));
}

namespace {

// A remote window's TX Profile page on "Desk" (stored at -13 dB) with the
// Core's live level since moved to -2, so the page holds unsaved edits.
struct ExportingPage {
    explicit ExportingPage(Session& s)
        : session(s)
    {
        TransmitModel& coreTx = s.core->transmitModel();
        coreTx.setMicGainDb(-21);
        s.coreProfiles().saveProfile(QStringLiteral("Mobile"), &coreTx);
        coreTx.setMicGainDb(-13);
        s.coreProfiles().saveProfile(QStringLiteral("Desk"), &coreTx);
        s.coreProfiles().setActiveProfile(QStringLiteral("Desk"), &coreTx);
    }
    bool open()
    {
        if (!session.connect()
            || !QTest::qWaitFor([this] {
                   return session.windowProfiles().activeProfileName() == QStringLiteral("Desk");
               })) {
            return false;
        }
        page = std::make_unique<TxProfileSetupPage>(&session.window, &session.windowProfiles(),
                                                    &session.window.transmitModel());
        page->setTransmitSettingsPermittedAt(kTransmitSettingsTxProfileFileVersion, true, {});
        page->setUnsavedPromptHook([this](const QString&) {
            ++asked;
            return answer;
        });
        page->setExportPathHook([this] { return path(); });
        page->setFileResultHook([this](bool success, const QString& text) {
            ++results;
            ok = success;
            shown = text;
        });
        session.core->transmitModel().setMicGainDb(-2);
        return dir.isValid() && QTest::qWaitFor([this] {
            return session.window.transmitModel().micGainDb() == -2;
        });
    }
    QString path() const { return dir.filePath(QStringLiteral("Desk.nereus-tx-profile")); }

    Session& session;
    QTemporaryDir dir;
    std::unique_ptr<TxProfileSetupPage> page;
    TxProfileSetupPage::UnsavedPromptResult answer{TxProfileSetupPage::UnsavedPromptResult::Yes};
    int asked{0};
    int results{0};
    bool ok{false};
    QString shown;
};

}  // namespace

// Another device selects a profile while this window chooses the file.
void TstTxProfileSelect::remoteExportStopsWhenTheActiveProfileChanges()
{
    Session s(m_securityDir.path(), this);
    ExportingPage e(s);
    QVERIFY(e.open());
    e.page->setExportPathHook([&] {
        s.coreProfiles().setActiveProfile(QStringLiteral("Mobile"), &s.core->transmitModel());
        [&s] {
            QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Mobile"));
        }();
        return e.path();
    });
    e.page->exportButton()->click();
    QCOMPARE(e.results, 1);
    QVERIFY(!e.ok);
    QCOMPARE(e.shown, QStringLiteral("The active transmit profile changed while you were "
                                     "choosing the file. Nothing was saved or exported."));
    // Round trip the link: nothing was sent.
    QVERIFY(!s.invoke("txProfile.select", {nameArg(QStringLiteral("Nobody"))}).accepted);
    QVERIFY(!QFile::exists(e.path()));
    QCOMPARE(storedField(QStringLiteral("Desk"), QStringLiteral("MicGain")), QStringLiteral("-13"));
    QCOMPARE(storedField(QStringLiteral("Mobile"), QStringLiteral("MicGain")), QStringLiteral("-21"));
}

void TstTxProfileSelect::aSecondExportWaitsForTheFirst()
{
    Session s(m_securityDir.path(), this);
    ExportingPage e(s);
    QVERIFY(e.open());
    e.page->exportButton()->click();
    QCOMPARE(e.results, 0);
    const QString other = e.dir.filePath(QStringLiteral("Other.nereus-tx-profile"));
    e.page->setExportPathHook([other] { return other; });
    e.page->exportButton()->click();
    QCOMPARE(e.results, 1);
    QVERIFY(!e.ok);
    QCOMPARE(e.shown, QStringLiteral("Another export is still waiting for the Core. "
                                     "Try again when it has finished."));
    QCOMPARE(e.asked, 1);
    QTRY_COMPARE(e.results, 2);
    QVERIFY2(e.ok, qPrintable(e.shown));
    QVERIFY(QFile::exists(e.path()));
    QVERIFY(!QFile::exists(other));
    // The first is done: another may start.
    e.page->exportButton()->click();
    QCOMPARE(e.results, 3);
    QVERIFY2(e.ok, qPrintable(e.shown));
    QVERIFY(QFile::exists(other));
}

void TstTxProfileSelect::anOrdinarySavesAnswerDoesNotCompleteAnExport()
{
    Session s(m_securityDir.path(), this);
    ExportingPage e(s);
    QVERIFY(e.open());
    e.page->exportButton()->click();
    const quint32 exportId = s.windowProfiles().lastStationRequestId();
    QVERIFY(exportId != 0);
    // Another save's answer comes first.
    s.windowProfiles().reportStationRequestFinished(MicProfileManager::StationRequest::Save,
                                                    exportId + 1000, true, {});
    QVERIFY(s.windowProfiles().stationRequestClaimed(exportId));
    QCoreApplication::sendPostedEvents(e.page.get());
    QCOMPARE(e.results, 0);
    QVERIFY(!QFile::exists(e.path()));
    QTRY_COMPARE(e.results, 1);
    QVERIFY2(e.ok, qPrintable(e.shown));
    QCOMPARE(readProfileFile(e.path()).keys.value(QStringLiteral("MicGain")), QStringLiteral("-2"));
}

void TstTxProfileSelect::editsDuringTheExportsSaveStayUnsaved()
{
    Session s(m_securityDir.path(), this);
    ExportingPage e(s);
    QVERIFY(e.open());
    e.page->exportButton()->click();
    QCOMPARE(e.asked, 1);
    // After the save left, before its answer.
    s.window.transmitModel().setMicGainDb(-5);
    QCOMPARE(e.results, 0);
    QTRY_COMPARE(e.results, 1);
    QVERIFY2(e.ok, qPrintable(e.shown));
    QCOMPARE(readProfileFile(e.path()).keys.value(QStringLiteral("MicGain")), QStringLiteral("-2"));
    // The later edit is still unsaved.
    e.answer = TxProfileSetupPage::UnsavedPromptResult::Cancel;
    e.page->exportButton()->click();
    QCOMPARE(e.asked, 2);
}

void TstTxProfileSelect::aRefusedImportKeepsTheEditsUnsaved()
{
    Session s(m_securityDir.path(), this);
    ExportingPage e(s);
    QVERIFY(e.open());
    const QString file = writeProfileFile(e.dir, QStringLiteral("On Air"),
        QJsonObject{{QStringLiteral("MicGain"), QStringLiteral("-19")}});
    e.page->setImportPathHook([file] { return file; });
    e.answer = TxProfileSetupPage::UnsavedPromptResult::No;
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    e.page->importButton()->click();
    QCOMPARE(e.asked, 1);
    QTRY_COMPARE(e.results, 1);
    s.unkeyCore();
    QVERIFY(!e.ok);
    QCOMPARE(e.shown, kOnAir);
    // The general refusal path, as for a request the page did not send.
    s.windowProfiles().reportStationRequestRefused();
    QCOMPARE(e.page->profileCombo()->currentText(), QStringLiteral("Desk"));
    e.answer = TxProfileSetupPage::UnsavedPromptResult::Cancel;
    e.page->exportButton()->click();
    QCOMPARE(e.asked, 2);
}

// Setup closes while its page waits: the refusal goes to the window's
// general notice, once, for an import and for an export's save.
void TstTxProfileSelect::aClosedPageLeavesTheRefusalToTheWindow()
{
    Session s(m_securityDir.path(), this);
    QSignalSpy notice(&s.window, &RadioModel::sliceAddRejected);
    ExportingPage e(s);
    QVERIFY(e.open());
    const QString file = writeProfileFile(e.dir, QStringLiteral("On Air"),
        QJsonObject{{QStringLiteral("MicGain"), QStringLiteral("-19")}});
    e.page->setImportPathHook([file] { return file; });
    e.answer = TxProfileSetupPage::UnsavedPromptResult::No;
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    notice.clear();
    e.page->importButton()->click();
    const quint32 importId = s.windowProfiles().lastStationRequestId();
    QVERIFY(s.windowProfiles().stationRequestClaimed(importId));
    e.answer = TxProfileSetupPage::UnsavedPromptResult::Yes;
    e.page->exportButton()->click();
    const quint32 exportId = s.windowProfiles().lastStationRequestId();
    QVERIFY(exportId != importId);
    QVERIFY(s.windowProfiles().stationRequestClaimed(exportId));
    e.page.reset();
    QVERIFY(!s.windowProfiles().stationRequestClaimed(importId));
    QVERIFY(!s.windowProfiles().stationRequestClaimed(exportId));
    QTRY_COMPARE(notice.count(), 2);
    s.unkeyCore();
    QCOMPARE(e.results, 0);
    QVERIFY(!QFile::exists(e.path()));
}

// Profiles an earlier build named with double underscores: a remote window
// selects and deletes them, whether the name could be saved now or not.
void TstTxProfileSelect::doubleUnderscoreProfilesSelectAndDelete()
{
    Session s(m_securityDir.path(), this);
    auto& settings = AppSettings::instance();
    const QStringList names{QStringLiteral("My__Profile"), QStringLiteral("Rig__c")};
    const QString manifest = QStringLiteral("hardware/%1/tx/profile/_names").arg(kMac);
    for (const QString& name : names) {
        settings.setValue(manifest, settings.value(manifest).toString() + QLatin1Char(',') + name);
    }
    // Published again, the added names with them.
    s.core->scopeTxProfiles(kMac);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.windowProfiles().profileNames().contains(names.last()));
    for (const QString& name : names) {
        SessionMessage result = s.invoke("txProfile.select", {nameArg(name)});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(s.coreProfiles().activeProfileName(), name);
        result = s.invoke("txProfile.delete", {nameArg(name)});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(!s.coreProfiles().profileNames().contains(name));
    }
}

void TstTxProfileSelect::importIsDisabledOnACoreAtVersion16()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    s.client->setTransmitSettingsVersionForTest(16);
    QVERIFY(s.client->transmitSettingsAvailable(kTransmitSettingsCwVersion));
    QVERIFY(!s.client->transmitSettingsAvailable(kTransmitSettingsTxProfileFileVersion));
    // The window sends nothing to such a Core.
    const IStationLink::CommandOutcome outcome =
        s.client->requestTxProfileImport(QStringLiteral("Desk"), QStringLiteral("{\"MicGain\":\"-9\"}"));
    QVERIFY(!outcome.sent);
    QCOMPARE(outcome.reason, IStationLink::transmitSettingsUnavailableReason());

    // As SetupDialog passes MainWindow's gates on to the page: each
    // version as the link offers it.
    TxProfileSetupPage page(&s.window, &s.windowProfiles(), &s.window.transmitModel());
    page.setTransmitSettingsPermittedAt(kTransmitSettingsTxProfileFileVersion, true, {});
    QVERIFY(page.importButton()->isEnabled());
    for (const int version : {3, kTransmitSettingsTxProfileFileVersion}) {
        page.setTransmitSettingsPermittedAt(version, s.client->transmitSettingsAvailable(version),
                                            IStationLink::transmitSettingsUnavailableReason());
    }
    QVERIFY(!page.importButton()->isEnabled());
    QCOMPARE(page.importButton()->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    QVERIFY(page.exportButton()->isEnabled());
    QVERIFY(page.saveButton()->isEnabled());
}

void TstTxProfileSelect::wrongArgumentsAreNotUnderstood()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const QString notUnderstood =
        QStringLiteral("The request for the transmit profile was not understood.");
    for (const QByteArray verb : {QByteArrayLiteral("txProfile.select"),
                                  QByteArrayLiteral("txProfile.save"),
                                  QByteArrayLiteral("txProfile.delete"),
                                  QByteArrayLiteral("txProfile.import")}) {
        SessionMessage result = s.invoke(verb, {nameArg(QStringLiteral("AM"), "profile")});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, notUnderstood);
        result = s.invoke(verb, {MirrorUpdate{0, "name", MirrorWireKind::Int64, QVariant(3)}});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, notUnderstood);
    }
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Default"));
}

void TstTxProfileSelect::aRefusedSelectShowsTheCoresProfileAgain()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_COMPARE(s.windowProfiles().activeProfileName(), QStringLiteral("Default"));
    QSignalSpy shown(&s.windowProfiles(), &MicProfileManager::activeProfileChanged);
    QSignalSpy toast(&s.window, &RadioModel::sliceAddRejected);

    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->isMox());
    QVERIFY(s.windowProfiles().setActiveProfile(QStringLiteral("AM"),
                                                &s.window.transmitModel()));
    QTRY_VERIFY(!shown.isEmpty());
    QCOMPARE(shown.last().at(0).toString(), QStringLiteral("Default"));
    QTRY_VERIFY(!toast.isEmpty());
    QCOMPARE(toast.last().at(0).toString(), kOnAir);
    QCOMPARE(s.coreProfiles().activeProfileName(), QStringLiteral("Default"));
    s.unkeyCore();
}

void TstTxProfileSelect::anUnconnectedWindowAsksNothing()
{
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    for (const IStationLink::CommandOutcome& outcome :
         {client.requestTxProfileSelect(QStringLiteral("AM")),
          client.requestTxProfileSave(QStringLiteral("AM")),
          client.requestTxProfileDelete(QStringLiteral("AM")),
          client.requestTxProfileImport(QStringLiteral("AM"), QStringLiteral("{}")),
          client.requestRadeResetVocoder()}) {
        QVERIFY(!outcome.sent);
        QCOMPARE(outcome.reason, IStationLink::transmitSettingsUnavailableReason());
    }
    // The window's combos go back to the Core's (here none) at once.
    QSignalSpy shown(window.micProfileManager(), &MicProfileManager::activeProfileChanged);
    QVERIFY(!window.micProfileManager()->setActiveProfile(QStringLiteral("AM"),
                                                          &window.transmitModel()));
    QCOMPARE(shown.count(), 1);
}

void TstTxProfileSelect::refusalsArePlainWords()
{
    for (const QString& reason :
         {QStringLiteral("There is no transmit profile called AM."),
          QStringLiteral("Give the transmit profile a name."),
          QStringLiteral("It is not possible to delete the last remaining TX profile."),
          QStringLiteral("The request for the transmit profile was not understood."),
          QStringLiteral("The transmit profile was not understood."),
          QStringLiteral("The transmit profile is too large."),
          QStringLiteral("The Core did not import the transmit profile."),
          QStringLiteral("A setting in the transmit profile is not text."),
          QStringLiteral("The transmit profile has no settings."),
          QStringLiteral("Use a transmit profile name of 64 characters or fewer."),
          QStringLiteral("Use a transmit profile name without tabs, line breaks or other "
                         "control characters."),
          QStringLiteral("The settings file cannot keep this transmit profile name. "
                         "Use a different name."),
          QStringLiteral("The transmit profile's leveler setting is not on or off."),
          QStringLiteral("The Core has no radio to keep transmit profiles for."),
          QStringLiteral("Update this app to change transmit profiles on this Core."),
          QStringLiteral("RADE is not running on the Core's active slice."),
          QStringLiteral("The request to reset the RADE vocoder was not understood."),
          QStringLiteral("Choose a Line In gain from -34.5 to 12.0 dB.")}) {
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }
}

QTEST_MAIN(TstTxProfileSelect)
#include "tst_tx_profile_select.moc"
