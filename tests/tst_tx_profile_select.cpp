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
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
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
#include "fakes/LoopbackTransport.h"
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
                                     const QString& name) {
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

void TstTxProfileSelect::wrongArgumentsAreNotUnderstood()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const QString notUnderstood =
        QStringLiteral("The request for the transmit profile was not understood.");
    for (const QByteArray verb : {QByteArrayLiteral("txProfile.select"),
                                  QByteArrayLiteral("txProfile.save"),
                                  QByteArrayLiteral("txProfile.delete")}) {
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
