// no-port-check: NereusSDR-original. CW transmit (R-CW-01, R-CW-02, R-CW-03):
// the CW settings on TransmitModel, the station CW pitch accessor and
// RadioModel::cwPitchHz, and the settings on the `transmit` object.
// Loopback link, no RF and no hardware: nothing here keys a radio, and no
// audio device or radio socket is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: F5 restore correction: switching radios keeps the new
//               radio's unplugged selection, including fresh restore and
//               reentrant request validation. J.J. Boyd (KG4VCF),
//               AI-assisted via OpenAI Codex.
//   2026-10-27: Final fix wave: authoritative port refusal and a different-computer mirror.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26: CW transmit, created: defaults, clamps, idempotent setters
//               and persistence of the CW settings; clampCwPitchHz,
//               stationCwPitchHz and RadioModel::setCwPitchHz; the
//               settings on the link (transmitSettingsVersion 16), their
//               round trip through a test Core and their range refusals.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-27: CW transmit final fix wave F2: the Core's reason QSK is not
//               offered reaches the window, and a QSK write where it is not
//               offered is refused with it. J.J. Boyd (KG4VCF), AI-assisted
//               via Claude Code.
//   2026-10-27: CW transmit final fix wave F5: a peer's serial port write
//               takes only None or one of the Core's ports. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/CwSerialKeyInput.h"
#include "core/CwTypes.h"
#include "core/HardwareProfile.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// F2: a radio that offers QSK (a Saturn on the newer protocol), so a QSK
// write is taken.
void offerQsk(RadioModel& core)
{
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    info.boardType = HPSDRHW::Saturn;
    info.firmwareVersion = 10;
    info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
    core.setLastRadioInfoForTest(info);
    core.cwKeyerController()->reevaluate();
}

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:42");

// The 30 CW settings in their declared (wire) order, the wire kind, the
// settings key and the default R-CW-01 gives.
struct CwSetting {
    const char* name;
    MirrorWireKind kind;
    const char* key;
    QVariant defaultValue;
    QVariant otherValue;   // a value in range that is not the default
};

QList<CwSetting> cwSettings()
{
    return {
        {"cwKeyerSpeed", MirrorWireKind::Int64, "CW_KeyerSpeed", 25, 30},
        {"cwKeyerMode", MirrorWireKind::Int64, "CW_KeyerMode", 2, 0},
        {"cwKeyerWeight", MirrorWireKind::Int64, "CW_KeyerWeight", 50, 40},
        {"cwReversePaddles", MirrorWireKind::Bool, "CW_ReversePaddles", false, true},
        {"cwStrictSpacing", MirrorWireKind::Bool, "CW_StrictSpacing", false, true},
        {"cwBreakInMode", MirrorWireKind::Int64, "CW_BreakInMode", 1, 2},
        {"cwBreakInDelayMs", MirrorWireKind::Int64, "CW_BreakInDelay", 300, 500},
        {"cwKeyDownDelayMs", MirrorWireKind::Int64, "CW_KeyDownDelay", 7, 20},
        {"cwKeyUpDelayMs", MirrorWireKind::Int64, "CW_KeyUpDelay", 10, 30},
        {"cwSidetone", MirrorWireKind::Bool, "CW_Sidetone", true, false},
        {"cwRadioSidetone", MirrorWireKind::Bool, "CW_RadioSidetone", true, false},
        {"cwComputerSidetone", MirrorWireKind::Bool, "CW_ComputerSidetone", true, false},
        {"cwAutoModeSwitch", MirrorWireKind::Bool, "CW_AutoModeSwitch", false, true},
        {"cwAutoModeReturn", MirrorWireKind::Bool, "CW_AutoModeReturn", false, true},
        {"cwAutoModeReturnMs", MirrorWireKind::Int64, "CW_AutoModeReturnMs", 2000, 3000},
        {"cwSerialPort", MirrorWireKind::Utf8, "CW_SerialPort", QString(),
         QStringLiteral("/dev/tty.usbserial-1")},
        {"cwSerialPaddles", MirrorWireKind::Bool, "CW_SerialPaddles", false, true},
        {"cwSerialKeyLine", MirrorWireKind::Int64, "CW_SerialKeyLine", 0, 1},
        {"cwSerialPttLine", MirrorWireKind::Int64, "CW_SerialPttLine", 0, 2},
        {"cwxSpeed", MirrorWireKind::Int64, "CWX_Speed", 22, 30},
        {"cwxRepeat", MirrorWireKind::Bool, "CWX_Repeat", false, true},
        {"cwxRepeatDelayS", MirrorWireKind::Int64, "CWX_RepeatDelay", 3, 10},
        {"cwxMemory1", MirrorWireKind::Utf8, "CWX_Memory1", QString(), QStringLiteral("CQ TEST 1")},
        {"cwxMemory2", MirrorWireKind::Utf8, "CWX_Memory2", QString(), QStringLiteral("CQ TEST 2")},
        {"cwxMemory3", MirrorWireKind::Utf8, "CWX_Memory3", QString(), QStringLiteral("CQ TEST 3")},
        {"cwxMemory4", MirrorWireKind::Utf8, "CWX_Memory4", QString(), QStringLiteral("CQ TEST 4")},
        {"cwxMemory5", MirrorWireKind::Utf8, "CWX_Memory5", QString(), QStringLiteral("CQ TEST 5")},
        {"cwxMemory6", MirrorWireKind::Utf8, "CWX_Memory6", QString(), QStringLiteral("CQ TEST 6")},
        {"cwxMemory7", MirrorWireKind::Utf8, "CWX_Memory7", QString(), QStringLiteral("CQ TEST 7")},
        {"cwxMemory8", MirrorWireKind::Utf8, "CWX_Memory8", QString(), QStringLiteral("CQ TEST 8")},
    };
}

QString storedAs(const QVariant& value)
{
    if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    }
    return value.toString();
}

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    model->setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
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
    {
        core = makeStationRadioModel();
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    // A raw `transmit` write the way any app can send it, and the Core's
    // answer for it.
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

    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
    quint32 nextId = 92000;
};

}  // namespace

class TstCwSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void freshProfileReadsTheDefaults();
    void settersClampToTheirRanges();
    void settersAreIdempotent();
    void settingsPersistPerRadio();
    void clampCwPitchHzClampsAndRounds();
    void stationCwPitchHzReadsClamped();
    void setCwPitchHzStoresAndEmitsOnce();
    void remoteWindowFollowsTheCoresPitch();
    void settingsAreOnTheLinkAfterCfcProfile();
    void coreOffersTransmitSettingsVersion16();
    void eachSettingRoundTripsThroughTheCore();
    void outOfRangeWritesAreRefusedWithTheRange();
    void qskWriteWhereItIsNotOfferedIsRefusedWithTheReason();
    void serialPortWriteTakesOnlyTheCoresPorts();
    void directSerialPortWriteRefusesUnknownWithoutPersisting();
    void remoteMirrorKeepsACorePortAbsentFromThisComputer();
    void savedSerialPortForNextRadioReplacesThePreviousPortEvenWhenUnavailable();
    void freshModelKeepsItsSavedUnavailableSerialPort();

private:
    QTemporaryDir m_securityDir;
};

void TstCwSettings::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile =
        QStringLiteral("cw-settings-%1").arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstCwSettings::init()
{
    AppSettings& s = AppSettings::instance();
    s.remove(QStringLiteral("CWPitch"));
    for (const CwSetting& c : cwSettings()) {
        s.remove(QStringLiteral("hardware/%1/tx/%2").arg(kMac, QLatin1String(c.key)));
    }
    // F5: the round trip's port is one of the test Core's serial ports.
    setCwSerialPortNamesForTest(QStringList{QStringLiteral("/dev/tty.usbserial-1")});
}

void TstCwSettings::cleanup()
{
    setCwSerialPortNamesForTest(std::nullopt);
}

void TstCwSettings::cleanupTestCase()
{
    setCwSerialPortNamesForTest(std::nullopt);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstCwSettings::freshProfileReadsTheDefaults()
{
    TransmitModel unloaded;
    TransmitModel loaded;
    loaded.loadFromSettings(kMac);
    for (const CwSetting& c : cwSettings()) {
        QVERIFY2(unloaded.property(c.name) == c.defaultValue, c.name);
        QVERIFY2(loaded.property(c.name) == c.defaultValue, c.name);
    }
    QCOMPARE(loaded.cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicB));
    QCOMPARE(loaded.cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
    QCOMPARE(loaded.cwSerialKeyLine(), static_cast<int>(CwSerialLine::None));
}

void TstCwSettings::settersClampToTheirRanges()
{
    TransmitModel tx;
    tx.setCwKeyerSpeed(0);
    QCOMPARE(tx.cwKeyerSpeed(), 1);
    tx.setCwKeyerSpeed(61);
    QCOMPARE(tx.cwKeyerSpeed(), 60);
    tx.setCwKeyerWeight(20);
    QCOMPARE(tx.cwKeyerWeight(), 33);
    tx.setCwKeyerWeight(70);
    QCOMPARE(tx.cwKeyerWeight(), 66);
    tx.setCwKeyDownDelayMs(300);
    QCOMPARE(tx.cwKeyDownDelayMs(), 255);
    tx.setCwKeyUpDelayMs(600);
    QCOMPARE(tx.cwKeyUpDelayMs(), 500);
    tx.setCwBreakInDelayMs(1500);
    QCOMPARE(tx.cwBreakInDelayMs(), 1000);
    tx.setCwBreakInDelayMs(-5);
    QCOMPARE(tx.cwBreakInDelayMs(), 0);
    tx.setCwAutoModeReturnMs(0);
    QCOMPARE(tx.cwAutoModeReturnMs(), 1);
    tx.setCwAutoModeReturnMs(10005);
    QCOMPARE(tx.cwAutoModeReturnMs(), 10000);
    tx.setCwAutoModeReturnMs(2004);
    QCOMPARE(tx.cwAutoModeReturnMs(), 2000);
    tx.setCwAutoModeReturnMs(2005);
    QCOMPARE(tx.cwAutoModeReturnMs(), 2010);
    tx.setCwKeyerMode(5);
    QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicB));
    tx.setCwKeyerMode(-1);
    QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::Straight));
    tx.setCwBreakInMode(3);
    QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
    tx.setCwSerialKeyLine(9);
    QCOMPARE(tx.cwSerialKeyLine(), static_cast<int>(CwSerialLine::Cts));
    tx.setCwSerialPttLine(-2);
    QCOMPARE(tx.cwSerialPttLine(), static_cast<int>(CwSerialLine::None));
    tx.setCwxSpeed(4);
    QCOMPARE(tx.cwxSpeed(), 5);
    tx.setCwxSpeed(61);
    QCOMPARE(tx.cwxSpeed(), 60);
    tx.setCwxRepeatDelayS(0);
    QCOMPARE(tx.cwxRepeatDelayS(), 1);
    tx.setCwxRepeatDelayS(61);
    QCOMPARE(tx.cwxRepeatDelayS(), 60);
}

void TstCwSettings::settersAreIdempotent()
{
    TransmitModel tx;
    for (const CwSetting& c : cwSettings()) {
        const QMetaObject* meta = tx.metaObject();
        const QMetaProperty prop = meta->property(meta->indexOfProperty(c.name));
        QVERIFY2(prop.isValid() && prop.hasNotifySignal(), c.name);
        QSignalSpy spy(&tx, prop.notifySignal());
        QVERIFY2(tx.setProperty(c.name, c.defaultValue), c.name);
        QVERIFY2(spy.isEmpty(), c.name);
        QVERIFY2(tx.setProperty(c.name, c.otherValue), c.name);
        QCOMPARE(spy.count(), 1);
        QVERIFY2(tx.setProperty(c.name, c.otherValue), c.name);
        QCOMPARE(spy.count(), 1);
    }
}

void TstCwSettings::settingsPersistPerRadio()
{
    AppSettings& s = AppSettings::instance();
    {
        TransmitModel tx;
        tx.loadFromSettings(kMac);
        for (const CwSetting& c : cwSettings()) {
            QVERIFY2(tx.setProperty(c.name, c.otherValue), c.name);
            const QString key = QStringLiteral("hardware/%1/tx/%2").arg(kMac, QLatin1String(c.key));
            QVERIFY2(s.contains(key), qPrintable(key));
            QCOMPARE(s.value(key).toString(), storedAs(c.otherValue));
        }
        QCOMPARE(s.value(QStringLiteral("hardware/%1/tx/CW_Sidetone").arg(kMac)).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("hardware/%1/tx/CW_ReversePaddles").arg(kMac)).toString(),
                 QStringLiteral("True"));
    }
    TransmitModel second;
    second.loadFromSettings(kMac);
    for (const CwSetting& c : cwSettings()) {
        QVERIFY2(second.property(c.name) == c.otherValue, c.name);
    }
    // Another radio keeps its own defaults.
    TransmitModel other;
    other.loadFromSettings(QStringLiteral("AA:BB:CC:DD:EE:43"));
    QCOMPARE(other.cwKeyerSpeed(), 25);
    QVERIFY(other.cwSidetone());
}

void TstCwSettings::clampCwPitchHzClampsAndRounds()
{
    QCOMPARE(clampCwPitchHz(150), 200);
    QCOMPARE(clampCwPitchHz(2400), 2250);
    QCOMPARE(clampCwPitchHz(604), 600);
    QCOMPARE(clampCwPitchHz(605), 610);
    QCOMPARE(clampCwPitchHz(600), 600);
    QCOMPARE(clampCwPitchHz(200), 200);
    QCOMPARE(clampCwPitchHz(2250), 2250);
}

void TstCwSettings::stationCwPitchHzReadsClamped()
{
    QCOMPARE(stationCwPitchHz(), kCwPitchDefaultHz);
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("100"));
    QCOMPARE(stationCwPitchHz(), 200);
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("2400"));
    QCOMPARE(stationCwPitchHz(), 2250);
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("703"));
    QCOMPARE(stationCwPitchHz(), 700);
}

void TstCwSettings::setCwPitchHzStoresAndEmitsOnce()
{
    RadioModel model;
    QCOMPARE(model.cwPitchHz(), 600);
    QSignalSpy spy(&model, &RadioModel::cwPitchChanged);
    model.setCwPitchHz(705);
    QCOMPARE(model.cwPitchHz(), 710);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("CWPitch")).toString(),
             QStringLiteral("710"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 710);
    model.setCwPitchHz(710);
    QCOMPARE(spy.count(), 1);
    // Out of range reads clamped.
    model.setCwPitchHz(5000);
    QCOMPARE(model.cwPitchHz(), 2250);
    QCOMPARE(spy.count(), 2);
}

// The catalogue hook: on a remote window the Core's CWPitch arrives as a
// station setting (a value, or a whole snapshot), and cwPitchHz follows.
void TstCwSettings::remoteWindowFollowsTheCoresPitch()
{
    RadioModel window{RadioModel::Role::Remote};
    QCOMPARE(window.cwPitchHz(), 600);
    QSignalSpy spy(&window, &RadioModel::cwPitchChanged);
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("800"));
    window.reportStationSettingChanged(QStringLiteral("CWPitch"));
    QCOMPARE(window.cwPitchHz(), 800);
    QCOMPARE(spy.count(), 1);
    // The same value again, or another key, emits nothing.
    window.reportStationSettingChanged(QStringLiteral("CWPitch"));
    window.reportStationSettingChanged(QStringLiteral("filters/USB/0/Low"));
    QCOMPARE(spy.count(), 1);
    // A snapshot (empty key) re-reads it too.
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("455"));
    window.reportStationSettingChanged(QString());
    QCOMPARE(window.cwPitchHz(), 460);
    QCOMPARE(spy.count(), 2);
    // notifyStationCwPitchMaybeChanged directly (a Core, or a refused write).
    AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("900"));
    window.notifyStationCwPitchMaybeChanged();
    QCOMPARE(window.cwPitchHz(), 900);
    QCOMPARE(spy.count(), 3);
}

// R-CW-03: each setting under its setter's name and wire kind, declared
// after cfcProfile so every earlier ordinal stays, and Bidirectional.
void TstCwSettings::settingsAreOnTheLinkAfterCfcProfile()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const MirrorProperty* last = schema.byName("cfcProfile");
    QVERIFY(last);
    quint16 ordinal = last->ordinal;
    for (const CwSetting& c : cwSettings()) {
        const MirrorProperty* prop = schema.byName(c.name);
        QVERIFY2(prop, c.name);
        QCOMPARE(prop->kind, c.kind);
        QCOMPARE(prop->ordinal, ++ordinal);
        QVERIFY2(prop->isWritable, c.name);
        QCOMPARE(MirrorPolicy::directionFor("TransmitModel", c.name),
                 MirrorDirection::Bidirectional);
        QVERIFY2(MirrorPolicy::inboundAllowed("TransmitModel", c.name), c.name);
    }
    // F2: why QSK is not offered, read-only, right after the settings.
    const MirrorProperty* reason = schema.byName("cwQskUnavailableReason");
    QVERIFY(reason);
    QCOMPARE(reason->ordinal, ++ordinal);
    QCOMPARE(reason->kind, MirrorWireKind::Utf8);
    QCOMPARE(MirrorPolicy::directionFor("TransmitModel", "cwQskUnavailableReason"),
             MirrorDirection::Outbound);
    QVERIFY(!MirrorPolicy::inboundAllowed("TransmitModel", "cwQskUnavailableReason"));
}

void TstCwSettings::coreOffersTransmitSettingsVersion16()
{
    QCOMPARE(kTransmitSettingsCwVersion, 16);
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QCOMPARE(s.server->buildCapabilities().transmitSettingsVersion, kTransmitSettingsCwVersion);
    QCOMPARE(s.client->capabilities().transmitSettingsVersion, kTransmitSettingsCwVersion);
    QVERIFY(s.client->transmitSettingsAvailable(kTransmitSettingsCwVersion));
    QVERIFY(!s.client->transmitSettingsAvailable(kTransmitSettingsCwVersion + 1));
    // A Core that offers 15 has no CW settings: the window's CW controls
    // read it as unavailable.
    s.client->setTransmitSettingsVersionForTest(kTransmitSettingsCfcProfileVersion);
    QVERIFY(!s.client->transmitSettingsAvailable(kTransmitSettingsCwVersion));
    QVERIFY(s.client->transmitSettingsAvailable(kTransmitSettingsCfcProfileVersion));
}

void TstCwSettings::eachSettingRoundTripsThroughTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    // F2: QSK is one of the values written, so the Core's radio offers it.
    offerQsk(*s.core);
    QVERIFY2(coreTx.cwQskUnavailableReason().isEmpty(), qPrintable(coreTx.cwQskUnavailableReason()));
    for (const CwSetting& c : cwSettings()) {
        QVERIFY2(coreTx.property(c.name) == c.defaultValue, c.name);
        // As the window's control does: its own model, which the link sends
        // on. The Core applies it and its value comes back.
        QVERIFY2(windowTx.setProperty(c.name, c.otherValue), c.name);
        QTRY_VERIFY2(coreTx.property(c.name) == c.otherValue, c.name);
        QTRY_VERIFY2(windowTx.property(c.name) == c.otherValue, c.name);
    }
    // And back from the Core to the window.
    coreTx.setCwKeyerSpeed(41);
    QTRY_COMPARE(windowTx.cwKeyerSpeed(), 41);
    coreTx.setCwxMemory3(QStringLiteral("TEST DE CORE"));
    QTRY_COMPARE(windowTx.cwxMemory3(), QStringLiteral("TEST DE CORE"));
}

void TstCwSettings::outOfRangeWritesAreRefusedWithTheRange()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const struct {
        const char* name;
        qlonglong value;
        const char* reason;
    } cases[] = {
        {"cwKeyerSpeed", 80, "Choose a keyer speed from 1 to 60 WPM."},
        {"cwKeyerSpeed", 0, "Choose a keyer speed from 1 to 60 WPM."},
        {"cwKeyerMode", 3, "Choose a keyer mode: straight key, iambic A or iambic B."},
        {"cwKeyerWeight", 32, "Choose a keyer weight from 33 to 66."},
        {"cwBreakInMode", -1, "Choose a break-in mode: off, semi or QSK."},
        {"cwBreakInDelayMs", 1001, "Choose a break-in delay from 0 to 1000 ms."},
        {"cwKeyDownDelayMs", 256, "Choose a key-down delay from 0 to 255 ms."},
        {"cwKeyUpDelayMs", 501, "Choose a key-up delay from 0 to 500 ms."},
        {"cwAutoModeReturnMs", 0, "Choose a mode return delay from 1 to 10000 ms."},
        {"cwAutoModeReturnMs", 10001, "Choose a mode return delay from 1 to 10000 ms."},
        {"cwSerialKeyLine", 3, "Choose a serial key line: none, DSR or CTS."},
        {"cwSerialPttLine", 3, "Choose a serial PTT line: none, DSR or CTS."},
        {"cwxSpeed", 4, "Choose a CWX speed from 5 to 60 WPM."},
        {"cwxRepeatDelayS", 61, "Choose a CWX repeat delay from 1 to 60 seconds."},
    };
    for (const auto& c : cases) {
        const QVariant before = coreTx.property(c.name);
        const SessionPropertyResult result =
            s.writeTransmit(c.name, MirrorWireKind::Int64, QVariant(c.value));
        QVERIFY2(!result.accepted, c.name);
        QCOMPARE(result.reason, QString::fromLatin1(c.reason));
        QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));
        QCOMPARE(coreTx.property(c.name), before);
    }
    // The ends of each range are taken.
    for (const auto& c : {std::pair<const char*, qlonglong>{"cwKeyerSpeed", 60},
                          {"cwKeyerSpeed", 1}, {"cwKeyerWeight", 66}, {"cwBreakInDelayMs", 0},
                          {"cwKeyDownDelayMs", 255}, {"cwAutoModeReturnMs", 10000},
                          {"cwxSpeed", 5}, {"cwxRepeatDelayS", 60}}) {
        const SessionPropertyResult result =
            s.writeTransmit(c.first, MirrorWireKind::Int64, QVariant(c.second));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(coreTx.property(c.first).toLongLong(), c.second);
    }
    // A value off the 10 ms step is rounded: the Core keeps 1230 and says
    // so, as it does for any write it settles on another value.
    const SessionPropertyResult stepped =
        s.writeTransmit("cwAutoModeReturnMs", MirrorWireKind::Int64, QVariant(qlonglong(1234)));
    QCOMPARE(coreTx.cwAutoModeReturnMs(), 1230);
    QVERIFY(stepped.hasValue);
    QCOMPARE(stepped.value.value.toLongLong(), 1230LL);
    QVERIFY2(OperatorWording::isPlain(stepped.reason), qPrintable(stepped.reason));
}

// F2 (R-CW-20): the Core says why QSK is not offered, on `transmit`, and a
// QSK write from any peer while it is not is refused with those words; the
// selection stays. Where it is offered, the reason clears and QSK is taken.
void TstCwSettings::qskWriteWhereItIsNotOfferedIsRefusedWithTheReason()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    // The test Core's radio is a Hermes Lite 2, which cannot do QSK.
    const QString reason = coreTx.cwQskUnavailableReason();
    QCOMPARE(reason, s.core->cwKeyerController()->qskUnavailableReason());
    QVERIFY(!reason.isEmpty());
    QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    QTRY_COMPARE(windowTx.cwQskUnavailableReason(), reason);
    // The window's own rule reads the Core's reason.
    QCOMPARE(s.window.cwKeyerController()->qskUnavailableReason(), reason);
    const int before = coreTx.cwBreakInMode();
    QVERIFY(before != static_cast<int>(CwBreakInMode::Qsk));

    const SessionPropertyResult refused =
        s.writeTransmit("cwBreakInMode", MirrorWireKind::Int64,
                        QVariant(qlonglong(static_cast<int>(CwBreakInMode::Qsk))));
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, reason);
    QCOMPARE(coreTx.cwBreakInMode(), before);
    // Off and Semi are still taken.
    const SessionPropertyResult off =
        s.writeTransmit("cwBreakInMode", MirrorWireKind::Int64,
                        QVariant(qlonglong(static_cast<int>(CwBreakInMode::Off))));
    QVERIFY2(off.accepted, qPrintable(off.reason));
    // The reason is the Core's alone: a peer's write of it is refused.
    const SessionPropertyResult written =
        s.writeTransmit("cwQskUnavailableReason", MirrorWireKind::Utf8, QVariant(QString()));
    QVERIFY(!written.accepted);
    QCOMPARE(coreTx.cwQskUnavailableReason(), reason);

    offerQsk(*s.core);
    QVERIFY(coreTx.cwQskUnavailableReason().isEmpty());
    QTRY_VERIFY(windowTx.cwQskUnavailableReason().isEmpty());
    const SessionPropertyResult taken =
        s.writeTransmit("cwBreakInMode", MirrorWireKind::Int64,
                        QVariant(qlonglong(static_cast<int>(CwBreakInMode::Qsk))));
    QVERIFY2(taken.accepted, qPrintable(taken.reason));
    QCOMPARE(coreTx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
    QTRY_VERIFY(s.window.cwKeyerController()->qskAvailable());

    // A later fallback, with the transmit slice in CW: QSK stops being
    // offered while selected. The Core falls back to Semi and says why
    // once; every window and app hears the reason on transmit (a remote
    // window's own notice: tst_cw_qsk_side_effects).
    SliceModel* coreSlice = s.core->txBoundSlice();
    QVERIFY(coreSlice);
    coreSlice->setDspMode(DSPMode::CWU);
    QCoreApplication::processEvents();
    QSignalSpy coreSaid(s.core->moxController(), &MoxController::moxRejected);
    RadioInfo hl2;
    hl2.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    hl2.boardType = HPSDRHW::HermesLite;
    s.core->setLastRadioInfoForTest(hl2);
    s.core->cwKeyerController()->reevaluate();
    QCOMPARE(s.core->cwKeyerController()->effectiveBreakInMode(), CwBreakInMode::Semi);
    QCOMPARE(coreTx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
    QCOMPARE(coreSaid.count(), 1);
    QCOMPARE(coreSaid.at(0).at(0).toString(), reason);
    QTRY_COMPARE(windowTx.cwQskUnavailableReason(), reason);
    QCOMPARE(s.window.cwKeyerController()->qskUnavailableReason(), reason);
    // Once: nothing more while it stays so.
    s.core->cwKeyerController()->reevaluate();
    QCoreApplication::processEvents();
    QCOMPARE(coreSaid.count(), 1);
}

// F5: the reader opens the port and turns on DTR and RTS, so a peer's
// cwSerialPort write takes only None or one of the Core's own serial
// ports. Any other path is refused with plain words and the port stays.
void TstCwSettings::serialPortWriteTakesOnlyTheCoresPorts()
{
    setCwSerialPortNamesForTest(QStringList{QStringLiteral("/dev/cu.usbserial-7"),
                                            QStringLiteral("cu.usbserial-7")});
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    for (const QString& port : {QStringLiteral("/etc/passwd"), QStringLiteral("/dev/disk0"),
                                QStringLiteral("../../etc/passwd"), QStringLiteral("COM99")}) {
        const SessionPropertyResult refused =
            s.writeTransmit("cwSerialPort", MirrorWireKind::Utf8, QVariant(port));
        QVERIFY2(!refused.accepted, qPrintable(port));
        QCOMPARE(refused.reason, QStringLiteral("That serial port is not on the Core's computer."));
        QVERIFY2(OperatorWording::isPlain(refused.reason), qPrintable(refused.reason));
        QVERIFY(coreTx.cwSerialPort().isEmpty());
    }
    for (const QString& port : {QStringLiteral("/dev/cu.usbserial-7"),
                                QStringLiteral("cu.usbserial-7"), QString()}) {
        const SessionPropertyResult taken =
            s.writeTransmit("cwSerialPort", MirrorWireKind::Utf8, QVariant(port));
        QVERIFY2(taken.accepted, qPrintable(taken.reason));
        QCOMPARE(coreTx.cwSerialPort(), port);
    }
}

// F5: direct local writes use the Core's validation too; neither the
// accepted value nor its persisted setting changes on refusal.
void TstCwSettings::directSerialPortWriteRefusesUnknownWithoutPersisting()
{
    RadioModel model;
    TransmitModel& tx = model.transmitModel();
    tx.loadFromSettings(kMac);
    const QString accepted = QStringLiteral("/dev/tty.usbserial-1");
    tx.setCwSerialPort(accepted);
    QCOMPARE(tx.cwSerialPort(), accepted);
    const QString key = QStringLiteral("hardware/%1/tx/CW_SerialPort").arg(kMac);
    QCOMPARE(AppSettings::instance().value(key).toString(), accepted);
    QSignalSpy changed(&tx, &TransmitModel::cwSerialPortChanged);
    QSignalSpy refused(&model, &RadioModel::cwSerialKeyRefusalChanged);
    QTest::ignoreMessage(QtWarningMsg, "CW serial key input: That serial port is not on the Core's computer.");
    tx.setCwSerialPort(QStringLiteral("/etc/passwd"));
    QCOMPARE(tx.cwSerialPort(), accepted);
    QCOMPARE(AppSettings::instance().value(key).toString(), accepted);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(model.cwSerialKeyRefusal(), QString::fromLatin1(kCwSerialPortNotHereText));
    tx.setCwSerialPort(QString());
    QVERIFY(tx.cwSerialPort().isEmpty());
    QCOMPARE(changed.count(), 1);
}

// F5: Core and window can enumerate different computers. Applying an
// accepted mirror value is not a request to open a port on the window.
void TstCwSettings::remoteMirrorKeepsACorePortAbsentFromThisComputer()
{
    setCwSerialPortNamesForTest(QStringList{QStringLiteral("client-only")});
    RadioModel window(RadioModel::Role::Remote);
    TransmitModel& tx = window.transmitModel();
    QSignalSpy changed(&tx, &TransmitModel::cwSerialPortChanged);
    const QString corePort = QStringLiteral("core-only");
    QVERIFY(!cwSerialPortIsHere(corePort));
    QVERIFY(tx.setProperty("cwSerialPort", corePort));
    QCOMPARE(tx.cwSerialPort(), corePort);
    QCOMPARE(changed.count(), 1);
    QVERIFY(window.cwSerialKeyRefusal().isEmpty());
}

// F5 correction: restoring another radio must replace the old selection,
// even while the new radio's saved port is unplugged. It must never fall
// back to opening/persisting the previous radio's enumerated port.
void TstCwSettings::savedSerialPortForNextRadioReplacesThePreviousPortEvenWhenUnavailable()
{
    const QString previousMac = QStringLiteral("AA:BB:CC:DD:EE:44");
    const QString previousPort = QStringLiteral("/dev/tty.usbserial-1");
    const QString missingPort = QStringLiteral("saved-but-unplugged");
    const QString savedKey = QStringLiteral("hardware/%1/tx/CW_SerialPort").arg(kMac);
    AppSettings& settings = AppSettings::instance();
    settings.setValue(savedKey, missingPort);
    TransmitModel tx;
    tx.loadFromSettings(previousMac);
    tx.setCwSerialPort(previousPort);
    QCOMPARE(tx.cwSerialPort(), previousPort);
    QSignalSpy changed(&tx, &TransmitModel::cwSerialPortChanged);
    QSignalSpy refused(&tx, &TransmitModel::cwSerialPortRefused);
    tx.loadFromSettings(kMac);
    QCOMPARE(tx.cwSerialPort(), missingPort);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), missingPort);
    QCOMPARE(refused.count(), 0); // a trusted restore is not an invalid request
    tx.persistToSettings(kMac);
    QCOMPARE(settings.value(savedKey).toString(), missingPort);
    QCOMPARE(settings.value(QStringLiteral("hardware/%1/tx/CW_SerialPort").arg(previousMac)).toString(),
             previousPort);

    // Restoring the label does not bypass the guard at the actual open.
    CwSerialKeyInput input(CwSerialKeyInput::createPlatformLineSource());
    QString error;
    QTest::ignoreMessage(QtWarningMsg, "CW serial key: not a serial port here: saved-but-unplugged");
    QVERIFY(!input.start(tx.cwSerialPort(), false, CwSerialLine::None, CwSerialLine::None, &error));
    QVERIFY(!input.isReading());
    QCOMPARE(error, QString::fromLatin1(kCwSerialPortNotHereText));
}

void TstCwSettings::freshModelKeepsItsSavedUnavailableSerialPort()
{
    const QString missingPort = QStringLiteral("saved-but-unplugged");
    const QString savedKey = QStringLiteral("hardware/%1/tx/CW_SerialPort").arg(kMac);
    AppSettings::instance().setValue(savedKey, missingPort);
    TransmitModel tx;
    QSignalSpy refused(&tx, &TransmitModel::cwSerialPortRefused);
    // A restore must not temporarily disable request validation while it
    // emits: a reentrant direct request still has to be refused.
    connect(&tx, &TransmitModel::cwSerialPortChanged, &tx, [&tx]() {
        tx.setCwSerialPort(QStringLiteral("/etc/passwd"));
    });
    tx.loadFromSettings(kMac);
    QCOMPARE(tx.cwSerialPort(), missingPort);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.first().first().toString(), QString::fromLatin1(kCwSerialPortNotHereText));
    tx.persistToSettings(kMac);
    QCOMPARE(AppSettings::instance().value(savedKey).toString(), missingPort);
}

QTEST_MAIN(TstCwSettings)
#include "tst_cw_settings.moc"
