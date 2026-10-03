// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_slice_select_band.cpp  (NereusSDR)
// =================================================================
//
// R-IOS-27, R-IOS-06: slice.selectBand, a device's band button for a slice.
// The Core runs its own band restore on the slice, the path the desktop's
// band buttons run (RadioModel::onBandButtonClicked(SliceModel*, Band)), so
// the band's saved frequency, mode and filter come back as they do on the
// desktop. Gated by bandSelectVersion 1 in the minor-11 block.
//
// Loopback link, no RF and no hardware: nothing here keys a radio. "On the
// air" keys the Core's own MoxController with the receive-only MOX
// pre-check lifted, as tst_tx_profile_select does, to show the desktop's
// rule (a band change is allowed on the air) holds. No audio device is
// opened.
//
//   cmake --build build --target tst_slice_select_band
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_slice_select_band$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: created.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-02: transmit group fix wave 2,
//                                    the Core's own key freezes the
//                                    transmit slice for another device.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/BandGrid.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:01");
const QString kNotUnderstood = QStringLiteral("The request to change band was not understood.");
const QString kNoReceiver = QStringLiteral("That receiver is no longer on the Core.");
const QString kNoSuchBand = QStringLiteral("The Core has no band button for that band.");
const QString kOlderApp = QStringLiteral("Update this app to change bands on this Core.");

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
    return model;
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
    SessionMessage invoke(const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke("slice.selectBand", id, arguments)));
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
    SessionMessage selectBand(int sliceId, int band)
    {
        return invoke({MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(qlonglong(sliceId))},
                       MirrorUpdate{0, "band", MirrorWireKind::Int64, QVariant(qlonglong(band))}});
    }
    SliceModel* slice() const { return core->slices().first(); }
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
    quint32 nextId = 95000;
};

struct Tuned {
    double frequencyHz = 0.0;
    DSPMode mode = DSPMode::LSB;
    int filterLow = 0;
    int filterHigh = 0;
    bool operator==(const Tuned& other) const
    {
        return frequencyHz == other.frequencyHz && mode == other.mode
            && filterLow == other.filterLow && filterHigh == other.filterHigh;
    }
};

Tuned tunedOf(const SliceModel* slice)
{
    return Tuned{slice->frequency(), slice->dspMode(), slice->filterLow(), slice->filterHigh()};
}

QByteArray describe(const Tuned& t)
{
    return QByteArray::number(t.frequencyHz, 'f', 0) + " Hz, mode "
        + QByteArray::number(static_cast<int>(t.mode)) + ", filter "
        + QByteArray::number(t.filterLow) + " to " + QByteArray::number(t.filterHigh);
}

}  // namespace

class TstSliceSelectBand : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void theVerbIsDeclaredWithItsCapability();
    void coreOffersBandSelectVersion1();
    void theCapabilityKeepsItsPlaceInTheMinor11Block();
    void restoresTheBandAsTheDesktopButtonDoes();
    void aFirstVisitTakesTheBandsSeedAsTheDesktopDoes();
    void everyGridBandIsTaken();
    void theSameBandChangesNothing();
    void aLockedSliceIsRefusedAsTheDesktopRefusesIt();
    void anUnknownReceiverIsRefused();
    void aBandOffTheGridIsRefused();
    void wrongArgumentsAreNotUnderstood();
    void aBandChangeWaitsWhileTheCoresOwnKeyIsOnTheAir();
    void anOlderAppIsToldToUpdate();
    void refusalsArePlainWords();

private:
    QTemporaryDir m_securityDir;
};

void TstSliceSelectBand::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("slice-select-band-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
}

void TstSliceSelectBand::init()
{
    // Each case starts with no band memory.
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstSliceSelectBand::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstSliceSelectBand::theVerbIsDeclaredWithItsCapability()
{
    bool found = false;
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        if (spec.verb != QByteArrayLiteral("slice.selectBand")) {
            continue;
        }
        found = true;
        QCOMPARE(spec.arguments.size(), 2);
        QCOMPARE(spec.arguments.at(0).name, QByteArrayLiteral("sliceId"));
        QCOMPARE(spec.arguments.at(0).kind, MirrorWireKind::Int64);
        QCOMPARE(spec.arguments.at(1).name, QByteArrayLiteral("band"));
        QCOMPARE(spec.arguments.at(1).kind, MirrorWireKind::Int64);
        QCOMPARE(spec.capability, QByteArrayLiteral("bandSelectVersion"));
        QCOMPARE(spec.capabilityVersion, 1);
        QCOMPARE(int(spec.minMinor), int(kRadioIdentitySessionProtocolMinor));
    }
    QVERIFY2(found, "slice.selectBand is not a verb");
}

void TstSliceSelectBand::coreOffersBandSelectVersion1()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QCOMPARE(s.server->buildCapabilities().bandSelectVersion, 1);
    QCOMPARE(s.client->capabilities().bandSelectVersion, 1);
}

void TstSliceSelectBand::theCapabilityKeepsItsPlaceInTheMinor11Block()
{
    StationCapabilities caps;
    caps.radioIdentityEntries = true;
    caps.transmitSettingsVersion = 6;
    caps.bandSelectVersion = 1;
    const QList<MirrorUpdate> updates = caps.toUpdates();
    // The original entries remain a contiguous ordered block even as
    // independent capabilities are added after it.
    QList<QByteArray> names;
    for (const MirrorUpdate& update : updates) { names.append(update.name); }
    const QList<QByteArray> originalBlock{
        "transmitSettingsVersion", "bandSelectVersion", "meterReadingsVersion",
        "dspInfoVersion", "recordStreamVersion", "stationRadiosVersion",
        "txDisplayVersion", "displayClockVersion", "controlChannelVersion",
        "txMonitorAudioVersion", "stationFreedvVersion", "mediaReplaceVersion",
        "controlSwitchVersion", "relayAllowed", "supportBundleVersion",
        "mediaTunnelVersion", "mediaRelayRoutingVersion"};
    const qsizetype first = names.indexOf(QByteArrayLiteral("transmitSettingsVersion"));
    QVERIFY(first >= 0);
    QCOMPARE(names.mid(first, originalBlock.size()), originalBlock);
    QCOMPARE(names.indexOf(QByteArrayLiteral("remoteIqVersion")),
             first + originalBlock.size());
    QCOMPARE(names.indexOf(QByteArrayLiteral("txModMonitorVersion")),
             first + originalBlock.size() + 1);
    QCOMPARE(names.indexOf(QByteArrayLiteral("accessoryTxVersion")),
             first + originalBlock.size() + 2);
    QVERIFY(!names.contains(QByteArrayLiteral("setupDescriptionVersion")));
    QVERIFY(!names.contains(QByteArrayLiteral("miniDisplayVersion")));
    QCOMPARE(updates.at(first + 1).kind, MirrorWireKind::Int64);
    QCOMPARE(updates.at(first + 1).value.toInt(), 1);
    QCOMPARE(StationCapabilities::fromUpdates(updates).bandSelectVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(updates).transmitSettingsVersion, 6);

    // Below minor 11 the entry is never sent.
    StationCapabilities older;
    older.bandSelectVersion = 1;
    for (const MirrorUpdate& update : older.toUpdates()) {
        QVERIFY(update.name != QByteArrayLiteral("bandSelectVersion"));
    }
}

// The verb and the desktop's band button, driven on the same Core and slice,
// bring back the same saved frequency, mode and filter.
void TstSliceSelectBand::restoresTheBandAsTheDesktopButtonDoes()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    const int id = slice->sliceIndex();

    // Visit 40 m on the desktop and leave it tuned somewhere of its own.
    s.core->onBandButtonClicked(slice, Band::Band20m);
    s.core->onBandButtonClicked(slice, Band::Band40m);
    slice->setFrequency(7074000.0);
    slice->setDspMode(DSPMode::DIGU);
    slice->setFilter(200, 2800);
    const Tuned saved40 = tunedOf(slice);
    s.core->onBandButtonClicked(slice, Band::Band20m);
    QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band20m);

    // The device's band button.
    const SessionMessage result = s.selectBand(id, static_cast<int>(Band::Band40m));
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QVERIFY(result.reason.isEmpty());
    QCOMPARE(result.affectedKeys, (QList<QByteArray>{QByteArrayLiteral("slice:0")}));
    const Tuned byVerb = tunedOf(slice);

    // The desktop's band button, from the same starting band.
    s.core->onBandButtonClicked(slice, Band::Band20m);
    s.core->onBandButtonClicked(slice, Band::Band40m);
    const Tuned byDesktop = tunedOf(slice);

    QVERIFY2(byVerb == byDesktop, describe(byVerb) + " by the verb, " + describe(byDesktop)
                                      + " by the desktop");
    QVERIFY2(byVerb == saved40, describe(byVerb) + " by the verb, " + describe(saved40) + " saved");
}

void TstSliceSelectBand::aFirstVisitTakesTheBandsSeedAsTheDesktopDoes()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    s.core->onBandButtonClicked(slice, Band::Band20m);

    const SessionMessage result = s.selectBand(slice->sliceIndex(), static_cast<int>(Band::Band17m));
    QVERIFY2(result.accepted, qPrintable(result.reason));
    const BandSeed seed = BandDefaults::seedFor(Band::Band17m);
    QVERIFY(seed.valid);
    QCOMPARE(slice->frequency(), seed.frequencyHz);
    QCOMPARE(slice->dspMode(), seed.mode);
    QVERIFY(slice->hasSettingsFor(Band::Band17m));
}

void TstSliceSelectBand::everyGridBandIsTaken()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    for (const BandGridEntry& entry : kBandGrid) {
        const SessionMessage result =
            s.selectBand(slice->sliceIndex(), static_cast<int>(entry.band));
        QVERIFY2(result.accepted, qPrintable(QString::fromLatin1(entry.label) + QStringLiteral(": ")
                                             + result.reason));
        QCOMPARE(bandFromFrequency(slice->frequency()), entry.band);
    }
}

// The desktop's button does nothing on the band the slice is on, and says
// nothing; the verb takes it and changes nothing.
void TstSliceSelectBand::theSameBandChangesNothing()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    s.core->onBandButtonClicked(slice, Band::Band20m);
    slice->setFrequency(14200000.0);
    const Tuned before = tunedOf(slice);
    const SessionMessage result = s.selectBand(slice->sliceIndex(), static_cast<int>(Band::Band20m));
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QVERIFY(tunedOf(slice) == before);
}

// The desktop refuses a band change on a locked slice with its own reason;
// the verb gives the same one and changes nothing.
void TstSliceSelectBand::aLockedSliceIsRefusedAsTheDesktopRefusesIt()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    s.core->onBandButtonClicked(slice, Band::Band20m);
    slice->setLocked(true);
    const Tuned before = tunedOf(slice);

    QSignalSpy ignored(s.core.get(), &RadioModel::bandClickIgnored);
    s.core->onBandButtonClicked(slice, Band::Band40m);
    QCOMPARE(ignored.count(), 1);
    const QString desktopReason = ignored.at(0).at(1).toString();

    const SessionMessage result = s.selectBand(slice->sliceIndex(), static_cast<int>(Band::Band40m));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, desktopReason);
    QCOMPARE(result.reason,
             QStringLiteral("Band 40m ignored: the slice is locked. Unlock it to change bands."));
    QVERIFY(tunedOf(slice) == before);
}

void TstSliceSelectBand::anUnknownReceiverIsRefused()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const Tuned before = tunedOf(s.slice());
    const SessionMessage result = s.selectBand(99, static_cast<int>(Band::Band40m));
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, kNoReceiver);
    QVERIFY(tunedOf(s.slice()) == before);
}

// A band the grid has no button for (GEN, XVTR, the SWL bands) or no band
// at all is refused; the desktop's grid has no way to ask for one.
void TstSliceSelectBand::aBandOffTheGridIsRefused()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const Tuned before = tunedOf(s.slice());
    for (const int band : {static_cast<int>(Band::GEN), static_cast<int>(Band::XVTR),
                           static_cast<int>(Band::Band49m), -1, 99}) {
        const SessionMessage result = s.selectBand(s.slice()->sliceIndex(), band);
        QVERIFY2(!result.accepted, qPrintable(QString::number(band)));
        QCOMPARE(result.reason, kNoSuchBand);
    }
    QVERIFY(tunedOf(s.slice()) == before);
}

void TstSliceSelectBand::wrongArgumentsAreNotUnderstood()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const qlonglong id = s.slice()->sliceIndex();
    const qlonglong band = static_cast<int>(Band::Band40m);
    const QList<QList<MirrorUpdate>> wrong{
        // A renamed argument.
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(id)},
         MirrorUpdate{0, "conformanceWrongName", MirrorWireKind::Int64, QVariant(band)}},
        // One missing.
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(id)}},
        // One extra.
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(id)},
         MirrorUpdate{0, "band", MirrorWireKind::Int64, QVariant(band)},
         MirrorUpdate{0, "mode", MirrorWireKind::Int64, QVariant(qlonglong(1))}},
        // The band as text.
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(id)},
         MirrorUpdate{0, "band", MirrorWireKind::Utf8, QVariant(QStringLiteral("40m"))}},
        // The receiver as a real number.
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Float64, QVariant(double(id))},
         MirrorUpdate{0, "band", MirrorWireKind::Int64, QVariant(band)}},
    };
    const Tuned before = tunedOf(s.slice());
    for (const QList<MirrorUpdate>& arguments : wrong) {
        const SessionMessage result = s.invoke(arguments);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, kNotUnderstood);
    }
    QVERIFY(tunedOf(s.slice()) == before);
}

// The desktop's band buttons are not held while the radio transmits
// (onBandButtonClicked has no on-air check), but a remote device is another
// operator: while the Core's own key transmits on its slice, that slice is
// frozen (the several-devices design, rulings 7.4 and 8.11), and the band
// change goes ahead once the key ends.
void TstSliceSelectBand::aBandChangeWaitsWhileTheCoresOwnKeyIsOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    SliceModel* const slice = s.slice();
    s.core->onBandButtonClicked(slice, Band::Band20m);
    s.keyCore();
    QVERIFY(s.core->stationOnAirRefusal(nullptr));
    const SessionMessage refused =
        s.selectBand(slice->sliceIndex(), static_cast<int>(Band::Band40m));
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, QStringLiteral("The radio is on the air. Try again when it stops."));
    QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band20m);
    s.unkeyCore();
    QTRY_VERIFY(!s.core->stationOnAirRefusal(nullptr));
    const SessionMessage result = s.selectBand(slice->sliceIndex(), static_cast<int>(Band::Band40m));
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band40m);
}

// A peer below minor 11 is never offered the capability, and its invoke is
// refused before the Core reads it.
void TstSliceSelectBand::anOlderAppIsToldToUpdate()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    std::unique_ptr<RadioModel> core = makeStationRadioModel();
    StationServer server(core.get(), settings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* app = new LoopbackTransport(QStringLiteral("app"), this);
    auto* station = new LoopbackTransport(QStringLiteral("station"), &server);
    station->linkTo(app);
    server.acceptTransport(station);
    QVERIFY(QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000));
    app->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, quint16(kRadioIdentitySessionProtocolMinor - 1), 0,
        QStringLiteral("NereusSDR iPhone"))));
    app->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    QVERIFY(QTest::qWaitFor(
        [app]() { return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")); },
        5000));
    for (const QByteArray& wire : app->received()) {
        QVERIFY(!wire.contains("bandSelectVersion"));
    }
    const double before = core->slices().first()->frequency();
    app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "slice.selectBand", 7001,
        {MirrorUpdate{0, "sliceId", MirrorWireKind::Int64, QVariant(qlonglong(0))},
         MirrorUpdate{0, "band", MirrorWireKind::Int64,
                      QVariant(qlonglong(static_cast<int>(Band::Band40m)))}})));
    SessionMessage found;
    QVERIFY(QTest::qWaitFor([&] {
        for (const QByteArray& wire : app->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult && message.commandId == 7001) {
                found = message;
                return true;
            }
        }
        return false;
    }, 3000));
    QVERIFY(!found.accepted);
    QCOMPARE(found.reason, kOlderApp);
    QCOMPARE(core->slices().first()->frequency(), before);
}

void TstSliceSelectBand::refusalsArePlainWords()
{
    for (const QString& reason :
         {kNotUnderstood, kNoReceiver, kNoSuchBand, kOlderApp,
          QStringLiteral("This Core cannot change bands for an app."),
          QStringLiteral("Band 40m ignored: the slice is locked. Unlock it to change bands.")}) {
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY2(OperatorWording::coreCalledStationIn(reason).isEmpty(), qPrintable(reason));
    }
}

QTEST_MAIN(TstSliceSelectBand)
#include "tst_slice_select_band.moc"
