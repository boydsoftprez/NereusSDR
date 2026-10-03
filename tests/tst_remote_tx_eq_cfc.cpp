// no-port-check: NereusSDR-original. R-R3-49 (parity Task 4): the TX EQ,
// CFC, phase rotator, CESSB, leveler and ALC settings from a remote window.
// Loopback link, no RF and no hardware: nothing here keys a radio. "On the
// air" keys the Core's own MoxController with the receive-only MOX
// pre-check lifted, as tst_transmit_model_properties does. The Core's
// transmit chain is wired to a test TxChannel with no WDSP channel behind
// it (RadioModel::wireTransmitChainForTest), so each setting is checked on
// the channel's own state. No audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 4): each group
//                                    round-trips from a window to the
//                                    Core's TX channel, a band array of the
//                                    wrong length or out of range is
//                                    refused whole, writes on the air are
//                                    refused, the EQ and CFC dialogs and
//                                    Setup > DSP > CFC and AGC/ALC work in a
//                                    remote window, and a local window's TX
//                                    channel gets the same curves as before.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49: a window that did
//                                    not declare txEqCurve gets no curve on
//                                    the wire and works the same one out of
//                                    txEqParaEqData. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49 (JJ's TX EQ
//                                    ruling): the TX EQ group is taken on
//                                    the air; since transmitSettingsVersion
//                                    13 every group is.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Setup publication (CFC band editor):
//                                    the remote dialog sends the table as
//                                    cfc.setProfile, shows a stale refusal,
//                                    and keeps the property write for an
//                                    older Core.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSemaphore>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/ParaEqCurve.h"
#include "core/ParaEqEnvelope.h"
#include "core/TxChannel.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SetupDialog.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");

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

// A receive-only Core with its transmit chain wired to a test TxChannel,
// and one window, handshake complete.
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
    // A fresh link to the same Core, after the old one was lost.
    bool reconnect(QObject* parent)
    {
        coreEnd = new LoopbackTransport(QStringLiteral("station-end-2"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end-2"), parent);
        coreEnd->linkTo(windowEnd);
        return connect();
    }
    // A raw `transmit` write, and the Core's answer for it.
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
    void unkeyCore()
    {
        core->moxController()->setMox(false);
    }

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

// The legacy EQ's F as Thetis's setTXEQProfile hands it to WDSP
// (eqform.cs:2777-2816 [v2.10.3.15]): F[0] = 0, then the ten centres.
std::vector<double> legacyFreqs(const TransmitModel& tx)
{
    std::vector<double> f{0.0};
    for (int i = 0; i < 10; ++i) { f.push_back(tx.txEqFreq(i)); }
    return f;
}

std::vector<double> legacyGains(const TransmitModel& tx)
{
    std::vector<double> g{static_cast<double>(tx.txEqPreamp())};
    for (int i = 0; i < 10; ++i) { g.push_back(tx.txEqBand(i)); }
    return g;
}

// What Thetis's sendTXDspUpdate hands WDSP for the curve a dialog's widget
// holds (eqform.cs:3041-3072 [v2.10.3.15]): F[0] = 0, G[0] = the preamp,
// Q[0] = 0, then every point, and Q only with Q factors on.
void widgetProfile(const ParametricEqWidget& w, std::vector<double>& f,
                   std::vector<double>& g, std::vector<double>& q)
{
    f.assign(1, 0.0);
    g.assign(1, w.globalGainDb());
    q.assign(1, 0.0);
    for (int i = 0; i < w.bandCount(); ++i) {
        double pf = 0.0, pg = 0.0, pq = 0.0;
        w.getPointData(i, pf, pg, pq);
        f.push_back(pf);
        g.push_back(pg);
        q.push_back(pq);
    }
    if (!w.parametricEq()) { q.clear(); }
}

bool sameCurve(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!qFuzzyCompare(1.0 + a[i], 1.0 + b[i])) { return false; }
    }
    return true;
}

// A five-point parametric curve whose gains are all `gainDb`.
QString flatParametricBlob(double gainDb)
{
    QJsonArray points;
    for (int i = 0; i < 5; ++i) {
        QJsonObject p;
        p.insert(QStringLiteral("frequency_hz"), 100.0 + 700.0 * i);
        p.insert(QStringLiteral("gain_db"), gainDb);
        p.insert(QStringLiteral("q"), 2.0);
        points.append(p);
    }
    QJsonObject root;
    root.insert(QStringLiteral("band_count"), 5);
    root.insert(QStringLiteral("parametric_eq"), true);
    root.insert(QStringLiteral("global_gain_db"), 0.0);
    root.insert(QStringLiteral("frequency_min_hz"), 100.0);
    root.insert(QStringLiteral("frequency_max_hz"), 2900.0);
    root.insert(QStringLiteral("points"), points);
    return ParaEqEnvelope::encode(
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

QString pairedCfcBlob(int count, bool useQ = true)
{
    const auto curve = [count, useQ](double global, double gain, double q) {
        QJsonArray points;
        for (int i = 0; i < count; ++i) {
            points.append(QJsonObject{{QStringLiteral("frequency_hz"), i * 100.0},
                                      {QStringLiteral("gain_db"), gain},
                                      {QStringLiteral("q"), q}});
        }
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("band_count"), count},
            {QStringLiteral("parametric_eq"), useQ},
            {QStringLiteral("global_gain_db"), global},
            {QStringLiteral("frequency_min_hz"), 0.0},
            {QStringLiteral("frequency_max_hz"), (count - 1) * 100.0},
            {QStringLiteral("points"), points}}).toJson(QJsonDocument::Compact));
    };
    return ParaEqEnvelope::encode(curve(6.0, 5.0, 2.0) + QStringLiteral("<SEP>")
                                  + curve(-7.0, -3.0, 6.0));
}

// What a window sent: the cfc.setProfile commands and the cfcParaEqData
// property writes among the messages the Core end received.
struct CfcTraffic {
    int commands = 0;
    int propertyWrites = 0;
};

CfcTraffic cfcTraffic(const LoopbackTransport* coreEnd)
{
    CfcTraffic traffic;
    for (const QByteArray& wire : coreEnd->received()) {
        SessionMessage message;
        if (!SessionMessages::decode(wire, &message)) { continue; }
        if (message.kind == SessionMessageKind::CommandInvoke
            && message.commandVerb == "cfc.setProfile") {
            ++traffic.commands;
        }
        if (message.kind == SessionMessageKind::PropertyWrite) {
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == "cfcParaEqData") { ++traffic.propertyWrites; }
            }
        }
    }
    return traffic;
}

int coreBandCount(const TransmitModel& tx)
{
    CfcProfile::Profile profile;
    return CfcProfile::decode(tx.cfcParaEqData(), profile)
        ? static_cast<int>(profile.f.size()) : 0;
}

QList<QSpinBox*> groupSpins(QWidget* page, const QString& title)
{
    for (QGroupBox* group : page->findChildren<QGroupBox*>()) {
        if (group->title() == title) {
            return group->findChildren<QSpinBox*>();
        }
    }
    return {};
}

}  // namespace

class TstRemoteTxEqCfc : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void bandArraysOfTheWrongLengthOrRangeAreRefusedWhole();
    void scalarsOutOfRangeAreRefusedWithTheRange();
    void eqRoundTripsToTheCoresTxChannel();
    void parametricCurveReachesTheCoresTxChannel();
    void cfcPhaseRotatorAndCessbRoundTrip();
    void lateResultKeepsTheNewerCfcCurve();
    void levelerAndAlcRoundTrip();
    void eachGroupIsRefusedOnTheAir();
    void remoteEqDialogShowsAndChangesTheCoresValues();
    void remoteCfcDialogOpensShowsAndChangesTheCoresValues();
    void remoteSetupPagesChangeTheCoresValues();
    void setupOpensCfcAndSpeechProcessorWithoutRemoteTransmit();
    void localWindowPushesTheSameCurvesAsBefore();
    void legacyBoxSeedsFromThisComputersOldValue();
    void txProfileCarriesTheLegacyBox();
    void newReasonsArePlain();
    void curveIsReadOnTheMainThreadAndHandedByValue();
    void txaFlushedTellsPureSignalOnTheMainThread();
    void parametricEqPushesAreCoalescedToTheTick();
    void unreadableCurveIsRefusedWithAReason();
    void txEqCurveOnlyToAPeerThatDeclaredIt();
    void pairedCfcReachesWdspAndRejectsLegacyWidthMismatch();
    void profileSwitchRestoresOnlyCoherentCfcCurve();
    void settingsReloadAppliesFinalCfcEnableAndCurveTogether();
    void reentrantCfcProjectionKeepsNewestCurve();
    void remoteCfcDialogSendsTheTableAsOneCommand();
    void remoteCfcDialogShowsAStaleRefusal();
    void remoteCfcDialogKeepsThePropertyWriteForAnOlderCore();
    void remoteCfcDialogFollowsTheCoreAfterAResultIsLost();
    void remoteCfcDialogOpenedBeforeTheLinkSendsWhole();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteTxEqCfc::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("remote-tx-eq-cfc-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemoteTxEqCfc::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteTxEqCfc::init()
{
    TxEqDialog::setSettingsPermitted(true, QString());
}

// B5.1 / Acceptance: "a wrong-length array is refused whole".
void TstRemoteTxEqCfc::bandArraysOfTheWrongLengthOrRangeAreRefusedWhole()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const struct {
        QByteArray name;
        QString good;
        QString reason;
        std::function<QString()> coreValue;
    } arrays[] = {
        {"txEqBandsJson", QStringLiteral("[-12,15,0,1,2,3,4,5,6,7]"),
         QStringLiteral("Choose ten TX EQ band levels, each from -12 to 15 dB."),
         [&] { return coreTx.txEqBandsJson(); }},
        {"txEqFreqsJson", QStringLiteral("[10,63,125,250,500,1000,2000,4000,8000,22000]"),
         QStringLiteral("Choose ten TX EQ band centers, each from 10 to 22000 Hz."),
         [&] { return coreTx.txEqFreqsJson(); }},
        {"cfcCompressionJson", QStringLiteral("[0,1,2,3,4,5,6,7,8,16]"),
         QStringLiteral("Choose ten CFC compression levels, each from 0 to 16 dB."),
         [&] { return coreTx.cfcCompressionJson(); }},
        {"cfcEqFreqJson", QStringLiteral("[0,100,200,300,400,500,600,700,800,20000]"),
         QStringLiteral("Choose ten CFC band centers, each from 0 to 20000 Hz."),
         [&] { return coreTx.cfcEqFreqJson(); }},
        {"cfcPostEqBandGainJson", QStringLiteral("[-24,24,0,1,2,3,4,5,6,7]"),
         QStringLiteral("Choose ten CFC post-EQ band levels, each from -24 to 24 dB."),
         [&] { return coreTx.cfcPostEqBandGainJson(); }},
    };
    for (const auto& a : arrays) {
        const QString before = a.coreValue();
        // Nine values, eleven values, one out of range (each end), a value
        // that is not a whole number, a string, and not an array at all.
        const QString bad[] = {
            QStringLiteral("[1,2,3,4,5,6,7,8,9]"),
            QStringLiteral("[1,2,3,4,5,6,7,8,9,10,11]"),
            QStringLiteral("[1,2,3,4,5,6,7,8,9,100000]"),
            QStringLiteral("[-100000,2,3,4,5,6,7,8,9,10]"),
            QStringLiteral("[1,2,3,4,5,6,7,8,9,1.5]"),
            QStringLiteral("[1,2,3,4,5,6,7,8,9,\"10\"]"),
            QStringLiteral("10"),
            QString(),
        };
        for (const QString& value : bad) {
            const SessionPropertyResult result =
                s.writeTransmit(a.name, MirrorWireKind::Utf8, value);
            QVERIFY2(!result.accepted, qPrintable(a.name + ' ' + value));
            QCOMPARE(result.reason, a.reason);
            QCOMPARE(a.coreValue(), before);
        }
        // The ends of the range are taken, whole.
        const SessionPropertyResult ok = s.writeTransmit(a.name, MirrorWireKind::Utf8, a.good);
        QVERIFY2(ok.accepted, qPrintable(a.name + ' ' + ok.reason));
        QCOMPARE(a.coreValue(), a.good);
    }
}

void TstRemoteTxEqCfc::scalarsOutOfRangeAreRefusedWithTheRange()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const struct {
        QByteArray name;
        qlonglong lo;
        qlonglong hi;
        QString reason;
    } scalars[] = {
        {"txEqPreamp", -12, 15, QStringLiteral("Choose a TX EQ preamp from -12 to 15 dB.")},
        {"txEqNc", 32, 8192, QStringLiteral("Choose a TX EQ Nc from 32 to 8192.")},
        {"txEqCtfmode", 0, 1, QStringLiteral("Choose a TX EQ cutoff of 0 (peaking) or 1 (notch).")},
        {"txEqWintype", 0, 1, QStringLiteral("Choose a TX EQ window of 0 (Blackman-Harris) or 1 (Hann).")},
        {"cfcPrecompDb", 0, 16, QStringLiteral("Choose a CFC pre-compression from 0 to 16 dB.")},
        {"cfcPostEqGainDb", -24, 24, QStringLiteral("Choose a CFC post-EQ gain from -24 to 24 dB.")},
        {"phaseRotatorFreqHz", 10, 2000, QStringLiteral("Choose a phase rotator frequency from 10 to 2000 Hz.")},
        {"phaseRotatorStages", 2, 16, QStringLiteral("Choose from 2 to 16 phase rotator stages.")},
        {"txLevelerMaxGain", 0, 20, QStringLiteral("Choose a leveler maximum gain from 0 to 20 dB.")},
        {"txLevelerDecay", 1, 5000, QStringLiteral("Choose a leveler decay from 1 to 5000 ms.")},
        {"txAlcMaxGain", 0, 120, QStringLiteral("Choose an ALC maximum gain from 0 to 120 dB.")},
        {"txAlcDecay", 1, 50, QStringLiteral("Choose an ALC decay from 1 to 50 ms.")},
    };
    TransmitModel& coreTx = s.core->transmitModel();
    for (const auto& e : scalars) {
        const QVariant before = coreTx.property(e.name.constData());
        for (qlonglong v : {e.lo - 1, e.hi + 1}) {
            const SessionPropertyResult r = s.writeTransmit(e.name, MirrorWireKind::Int64, v);
            QVERIFY2(!r.accepted, e.name.constData());
            QCOMPARE(r.reason, e.reason);
            QCOMPARE(coreTx.property(e.name.constData()), before);
        }
        for (qlonglong v : {e.lo, e.hi}) {
            const SessionPropertyResult r = s.writeTransmit(e.name, MirrorWireKind::Int64, v);
            QVERIFY2(r.accepted, qPrintable(e.name + ' ' + r.reason));
            QCOMPARE(coreTx.property(e.name.constData()).toLongLong(), v);
        }
    }
}

// B5.1: preamp, every band, the centres and Nc/Mp/Ctfmode/Wintype, from the
// window's model (as the dialog writes them) to the Core's TX channel.
void TstRemoteTxEqCfc::eqRoundTripsToTheCoresTxChannel()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    QVERIFY(coreTx.txEqUseLegacy());

    windowTx.setTxEqPreamp(7);
    for (int i = 0; i < 10; ++i) {
        windowTx.setTxEqBand(i, i - 5);
        windowTx.setTxEqFreq(i, 100 + 200 * i);
    }
    windowTx.setTxEqNc(4096);
    windowTx.setTxEqMp(true);
    windowTx.setTxEqCtfmode(1);
    windowTx.setTxEqWintype(1);
    QTRY_COMPARE(coreTx.txEqWintype(), 1);
    QTRY_COMPARE(coreTx.txEqFreq(9), 1900);
    QCOMPARE(coreTx.txEqPreamp(), 7);
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(coreTx.txEqBand(i), i - 5);
        QCOMPARE(coreTx.txEqFreq(i), 100 + 200 * i);
    }
    // The Core's TX channel has the ten-band curve and the EQ globals.
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileFForTest(), legacyFreqs(coreTx)));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), legacyGains(coreTx)));
    QCOMPARE(s.txChannel.lastTxEqNcForTest(), 4096);
    QVERIFY(s.txChannel.lastTxEqMpForTest());
    QCOMPARE(s.txChannel.lastTxEqCtfmodeForTest(), 1);
    QCOMPARE(s.txChannel.lastTxEqWintypeForTest(), 1);

    // A change made at the Core shows in the window.
    coreTx.setTxEqBand(4, 11);
    QTRY_COMPARE(windowTx.txEqBand(4), 11);
    coreTx.setTxEqPreamp(-3);
    QTRY_COMPARE(windowTx.txEqPreamp(), -3);
}

// The Legacy box and the parametric curve: the Core applies the curve it
// picks to its own TX channel, whichever window changed it.
void TstRemoteTxEqCfc::parametricCurveReachesTheCoresTxChannel()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();

    QJsonArray points;
    for (int i = 0; i < 5; ++i) {
        QJsonObject p;
        p.insert(QStringLiteral("frequency_hz"), 100.0 + 700.0 * i);
        p.insert(QStringLiteral("gain_db"), (i % 2) ? 6.5 : -4.0);
        p.insert(QStringLiteral("q"), 2.0);
        points.append(p);
    }
    QJsonObject root;
    root.insert(QStringLiteral("band_count"), 5);
    root.insert(QStringLiteral("parametric_eq"), true);
    root.insert(QStringLiteral("global_gain_db"), 1.5);
    root.insert(QStringLiteral("frequency_min_hz"), 100.0);
    root.insert(QStringLiteral("frequency_max_hz"), 2900.0);
    root.insert(QStringLiteral("points"), points);
    const QString blob = ParaEqEnvelope::encode(
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    windowTx.setTxEqParaEqData(blob);
    QTRY_COMPARE(coreTx.txEqParaEqData(), blob);
    // Still legacy: the ten-band curve stays on the channel.
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), legacyGains(coreTx)));

    windowTx.setTxEqUseLegacy(false);
    QTRY_VERIFY(!coreTx.txEqUseLegacy());
    // Every point as Thetis's sendTXDspUpdate hands it to WDSP (worked by
    // hand from eqform.cs:3041-3072 and ucParametricEq.cs PointsFromJson
    // [v2.10.3.15]): F[0] = 0, G[0] = the preamp, Q[0] = 0, Q factors on.
    const std::vector<double> freqs{0, 100, 800, 1500, 2200, 2900};
    const std::vector<double> gains{1.5, -4, 6.5, -4, 6.5, -4};
    const std::vector<double> qs{0, 2, 2, 2, 2, 2};
    QTRY_VERIFY(sameCurve(s.txChannel.lastTxEqProfileFForTest(), freqs));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), gains));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileQForTest(), qs));

    // A legacy band change in parametric mode keeps the parametric curve.
    windowTx.setTxEqBand(0, 3);
    QTRY_COMPARE(coreTx.txEqBand(0), 3);
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), gains));

    // Back to legacy: the ten-band curve returns.
    windowTx.setTxEqUseLegacy(true);
    QTRY_VERIFY(coreTx.txEqUseLegacy());
    QTRY_VERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), legacyGains(coreTx)));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileFForTest(), legacyFreqs(coreTx)));
    QVERIFY(s.txChannel.lastTxEqProfileQForTest().empty());

    // The Core's own change shows in the window.
    coreTx.setTxEqUseLegacy(false);
    QTRY_VERIFY(!windowTx.txEqUseLegacy());
}

// B5.1 / B5.11: the CFC profile (all 30 values), its scalars, the phase
// rotator and CESSB.
void TstRemoteTxEqCfc::cfcPhaseRotatorAndCessbRoundTrip()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();

    for (int i = 0; i < 10; ++i) {
        windowTx.setCfcEqFreq(i, 150 * (i + 1));
        windowTx.setCfcCompression(i, i + 1);
        windowTx.setCfcPostEqBandGain(i, 2 * i - 9);
    }
    windowTx.setCfcPrecompDb(6);
    windowTx.setCfcPostEqGainDb(-8);
    windowTx.setCfcPostEqEnabled(true);
    windowTx.setPhaseRotatorEnabled(true);
    windowTx.setPhaseRotatorFreqHz(420);
    windowTx.setPhaseRotatorStages(12);
    windowTx.setPhaseReverseEnabled(true);
    windowTx.setCessbOn(true);
    QTRY_VERIFY(coreTx.cessbOn());
    QTRY_COMPARE(coreTx.cfcPostEqBandGain(9), 9);

    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> e;
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(coreTx.cfcEqFreq(i), 150 * (i + 1));
        QCOMPARE(coreTx.cfcCompression(i), i + 1);
        QCOMPARE(coreTx.cfcPostEqBandGain(i), 2 * i - 9);
        f.push_back(150.0 * (i + 1));
        g.push_back(i + 1);
        e.push_back(2 * i - 9);
    }
    QVERIFY(sameCurve(s.txChannel.lastTxCfcProfileFForTest(), f));
    QVERIFY(sameCurve(s.txChannel.lastTxCfcProfileGForTest(), g));
    QVERIFY(sameCurve(s.txChannel.lastTxCfcProfileEForTest(), e));
    QCOMPARE(s.txChannel.lastTxCfcPrecompDbForTest(), 6.0);
    QCOMPARE(s.txChannel.lastTxCfcPrePeqDbForTest(), -8.0);
    QVERIFY(s.txChannel.lastTxCfcPostEqRunningForTest());
    QVERIFY(s.txChannel.lastPhaseRotatorRunForTest());
    QCOMPARE(s.txChannel.lastTxPhrotCornerHzForTest(), 420.0);
    QCOMPARE(s.txChannel.lastTxPhrotNstagesForTest(), 12);
    QVERIFY(s.txChannel.lastTxPhrotReverseForTest());
    QVERIFY(s.txChannel.lastTxCessbOnForTest());

    // The parametric CFC blob travels too.
    // (A curve the Core can load: group A fix wave, M4.)
    const QString cfcCurve = pairedCfcBlob(10);
    windowTx.setCfcParaEqData(cfcCurve);
    QTRY_COMPARE(coreTx.cfcParaEqData(), cfcCurve);

    coreTx.setCfcCompression(3, 16);
    QTRY_COMPARE(windowTx.cfcCompression(3), 16);
}

// The cfcPhaseRotatorAndCessbRoundTrip failure under load, made
// deterministic: the Core's answer to an earlier write (the pre-comp)
// arrives after the window has saved a CFC curve and before the window
// sends it. The answer names the value the window already holds, so it
// is no change; it must not re-encode the pending curve, or the window
// sends the Core a curve nobody saved.
void TstRemoteTxEqCfc::lateResultKeepsTheNewerCfcCurve()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();

    // The Core's answers wait, as on a slow link.
    s.coreEnd->setHoldsOutgoing(true);
    windowTx.setCfcPrecompDb(6);
    QTRY_COMPARE(coreTx.cfcPrecompDb(), 6);

    // pairedCfcBlob's pre-comp is 6: the curve leaves it where it was.
    const QString cfcCurve = pairedCfcBlob(10);
    windowTx.setCfcParaEqData(cfcCurve);
    QCOMPARE(windowTx.cfcPrecompDb(), 6);

    // The pre-comp answer lands before the window's next send.
    QSignalSpy completed(s.client.get(), &StationClient::propertyWriteCompleted);
    s.coreEnd->setHoldsOutgoing(false);
    QCoreApplication::sendPostedEvents();
    int precompAnswers = 0;
    for (const QList<QVariant>& args : completed) {
        if (args.at(1).toByteArray() == QByteArrayLiteral("cfcPrecompDb")) { ++precompAnswers; }
    }
    QCOMPARE(precompAnswers, 1);
    QCOMPARE(windowTx.cfcParaEqData(), cfcCurve);

    QTRY_COMPARE(coreTx.cfcParaEqData(), cfcCurve);
    QCOMPARE(windowTx.cfcParaEqData(), cfcCurve);
}

// B5.9: Setup > DSP > AGC/ALC's TX Leveler and TX ALC.
void TstRemoteTxEqCfc::levelerAndAlcRoundTrip()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    windowTx.setTxLevelerMaxGain(9);
    windowTx.setTxLevelerDecay(750);
    windowTx.setTxAlcMaxGain(40);
    windowTx.setTxAlcDecay(25);
    QTRY_COMPARE(coreTx.txAlcDecay(), 25);
    QCOMPARE(coreTx.txLevelerMaxGain(), 9);
    QCOMPARE(coreTx.txLevelerDecay(), 750);
    QCOMPARE(coreTx.txAlcMaxGain(), 40);
    QCOMPARE(s.txChannel.lastTxLevelerTopDbForTest(), 9.0);
    QCOMPARE(s.txChannel.lastTxLevelerDecayMsForTest(), 750);
    QCOMPARE(s.txChannel.lastTxAlcMaxGainDbForTest(), 40.0);
    QCOMPARE(s.txChannel.lastTxAlcDecayMsForTest(), 25);
}

// Remote parity on the air: the TX EQ, CFC, phase rotator, CESSB, leveler
// and ALC settings are taken while the radio is on the air, as a local
// window changes them while transmitting (transmitSettingsVersion 13).
void TstRemoteTxEqCfc::eachGroupIsRefusedOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());

    struct Write {
        QByteArray name;
        MirrorWireKind kind;
        QVariant value;
    };
    const QString curve = ParaEqEnvelope::encode(QStringLiteral(
        "{\"band_count\":5,\"frequency_max_hz\":3000,\"frequency_min_hz\":50,"
        "\"global_gain_db\":-2.5,\"parametric_eq\":true,\"points\":["
        "{\"frequency_hz\":50,\"gain_db\":-6,\"q\":1.5},{\"frequency_hz\":300,\"gain_db\":3,\"q\":2},"
        "{\"frequency_hz\":1200,\"gain_db\":-1.5,\"q\":4},{\"frequency_hz\":2400,\"gain_db\":4,\"q\":3},"
        "{\"frequency_hz\":3000,\"gain_db\":0,\"q\":1}]}"));
    const Write taken[] = {
        {"txEqEnabled", MirrorWireKind::Bool, !coreTx.txEqEnabled()},
        {"txEqUseLegacy", MirrorWireKind::Bool, !coreTx.txEqUseLegacy()},
        {"txEqPreamp", MirrorWireKind::Int64, coreTx.txEqPreamp() == 5 ? 6 : 5},
        {"txEqBandsJson", MirrorWireKind::Utf8, QStringLiteral("[1,1,1,1,1,1,1,1,1,1]")},
        {"txEqFreqsJson", MirrorWireKind::Utf8, QStringLiteral("[20,63,125,250,500,1000,2000,4000,8000,16000]")},
        {"txEqNc", MirrorWireKind::Int64, coreTx.txEqNc() == 1024 ? 2048 : 1024},
        {"txEqMp", MirrorWireKind::Bool, !coreTx.txEqMp()},
        {"txEqCtfmode", MirrorWireKind::Int64, coreTx.txEqCtfmode() == 1 ? 0 : 1},
        {"txEqWintype", MirrorWireKind::Int64, coreTx.txEqWintype() == 1 ? 0 : 1},
        {"txEqParaEqData", MirrorWireKind::Utf8, curve},
    };
    for (const Write& w : taken) {
        const SessionPropertyResult r = s.writeTransmit(w.name, w.kind, w.value);
        QVERIFY2(r.accepted, qPrintable(QString::fromUtf8(w.name) + QStringLiteral(": ") + r.reason));
    }
    QCOMPARE(coreTx.txEqParaEqData(), curve);
    QCOMPARE(coreTx.txEqBand(0), 1);
    // An unreadable curve is still refused for what it is, not for the air.
    {
        const SessionPropertyResult r =
            s.writeTransmit("txEqParaEqData", MirrorWireKind::Utf8, QStringLiteral("x"));
        QVERIFY(!r.accepted);
        QVERIFY(r.reason != kOnAir);
        QCOMPARE(coreTx.txEqParaEqData(), curve);
    }
    // The window's own change reaches the Core on the air.
    const int band = coreTx.txEqBand(2);
    windowTx.setTxEqBand(2, band == 5 ? 6 : 5);
    QTRY_COMPARE(coreTx.txEqBand(2), band == 5 ? 6 : 5);

    const Write alsoTaken[] = {
        {"cfcCompressionJson", MirrorWireKind::Utf8, QStringLiteral("[1,1,1,1,1,1,1,1,1,1]")},
        {"cfcEqFreqJson", MirrorWireKind::Utf8, QStringLiteral("[1,2,3,4,5,6,7,8,9,10]")},
        {"cfcPostEqBandGainJson", MirrorWireKind::Utf8, QStringLiteral("[1,1,1,1,1,1,1,1,1,1]")},
        {"cfcPostEqEnabled", MirrorWireKind::Bool, true},
        {"cfcPostEqGainDb", MirrorWireKind::Int64, 3},
        {"cfcPrecompDb", MirrorWireKind::Int64, 3},
        {"phaseRotatorEnabled", MirrorWireKind::Bool, true},
        {"phaseRotatorFreqHz", MirrorWireKind::Int64, 500},
        {"phaseRotatorStages", MirrorWireKind::Int64, 4},
        {"phaseReverseEnabled", MirrorWireKind::Bool, true},
        {"cessbOn", MirrorWireKind::Bool, true},
        {"txLevelerMaxGain", MirrorWireKind::Int64, 4},
        {"txLevelerDecay", MirrorWireKind::Int64, 400},
        {"txAlcMaxGain", MirrorWireKind::Int64, 60},
        {"txAlcDecay", MirrorWireKind::Int64, 30},
    };
    for (const Write& w : alsoTaken) {
        const SessionPropertyResult r = s.writeTransmit(w.name, w.kind, w.value);
        QVERIFY2(r.accepted, qPrintable(QString::fromUtf8(w.name) + QStringLiteral(": ") + r.reason));
    }
    QCOMPARE(coreTx.txAlcDecay(), 30);
    // The window's own change reaches the Core on the air too.
    const int stages = coreTx.phaseRotatorStages();
    windowTx.setPhaseRotatorStages(stages == 3 ? 4 : 3);
    QTRY_COMPARE(coreTx.phaseRotatorStages(), stages == 3 ? 4 : 3);
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
}

// B5.1: Tools > TX Equalizer in a remote window shows the Core's values and
// changes them, both panels and the Legacy box; the settings gate greys it.
void TstRemoteTxEqCfc::remoteEqDialogShowsAndChangesTheCoresValues()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setTxEqBand(6, 13);
    coreTx.setTxEqFreq(6, 2100);
    coreTx.setTxEqNc(1024);
    QVERIFY(s.connect());
    TransmitModel& windowTx = s.window.transmitModel();
    QTRY_COMPARE(windowTx.txEqBand(6), 13);

    TxEqDialog dlg(&s.window);
    auto* band6 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqBandSpin6"));
    auto* freq6 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin6"));
    auto* preamp = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqPreampSpin"));
    auto* nc = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqNcSpin"));
    auto* wintype = dlg.findChild<QComboBox*>(QStringLiteral("TxEqWintypeCombo"));
    QVERIFY(band6 && freq6 && preamp && nc && wintype);
    QCOMPARE(band6->value(), 13);
    QCOMPARE(freq6->value(), 2100);
    QCOMPARE(nc->value(), 1024);

    band6->setValue(-7);
    freq6->setValue(2300);
    preamp->setValue(9);
    nc->setValue(512);
    wintype->setCurrentIndex(1);
    QTRY_COMPARE(coreTx.txEqWintype(), 1);
    QTRY_COMPARE(coreTx.txEqBand(6), -7);
    QCOMPARE(coreTx.txEqFreq(6), 2300);
    QCOMPARE(coreTx.txEqPreamp(), 9);
    QCOMPARE(coreTx.txEqNc(), 512);
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), legacyGains(coreTx)));

    // A Core change shows in the open dialog.
    coreTx.setTxEqBand(6, 2);
    QTRY_COMPARE(band6->value(), 2);

    // The Legacy box and the parametric panel.
    QAbstractButton* legacy = dlg.modeSelector()->button(0);
    QVERIFY(legacy && legacy->isChecked());
    dlg.modeSelector()->button(1)->click();
    QTRY_VERIFY(!coreTx.txEqUseLegacy());
    QCOMPARE(dlg.panelStack()->currentIndex(), 1);
    auto* gain = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaGainSpin"));
    QVERIFY(gain);
    dlg.parametricWidget()->setSelectedIndex(3);
    gain->setValue(6.5);
    const QString blob = windowTx.txEqParaEqData();
    QVERIFY(!blob.isEmpty());
    QTRY_COMPARE(coreTx.txEqParaEqData(), blob);
    // The Core's TX channel has every point of the curve the window drew,
    // as Thetis hands it to WDSP.
    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> q;
    widgetProfile(*dlg.parametricWidget(), f, g, q);
    QTRY_VERIFY(sameCurve(s.txChannel.lastTxEqProfileGForTest(), g));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileFForTest(), f));
    QVERIFY(sameCurve(s.txChannel.lastTxEqProfileQForTest(), q));
    QCOMPARE(g[4], 6.5);

    // The Core's Legacy box moves the window's.
    coreTx.setTxEqUseLegacy(true);
    QTRY_VERIFY(legacy->isChecked());
    QCOMPARE(dlg.panelStack()->currentIndex(), 0);

    // On the air: greyed with the reason; off it, live again.
    TxEqDialog::setSettingsPermitted(false, kOnAir);
    QVERIFY(!legacy->isEnabled());
    QVERIFY(!dlg.panelStack()->isEnabled());
    QVERIFY(!nc->isEnabled());
    QVERIFY(dlg.settingsReasonLabel()->isVisibleTo(&dlg));
    QCOMPARE(dlg.settingsReasonLabel()->text(), kOnAir);
    // A dialog built while it is closed starts greyed.
    TxEqDialog later(&s.window);
    QVERIFY(!later.modeSelector()->button(0)->isEnabled());
    TxEqDialog::setSettingsPermitted(true, QString());
    QVERIFY(legacy->isEnabled());
    QVERIFY(later.modeSelector()->button(0)->isEnabled());
    QVERIFY(!dlg.settingsReasonLabel()->isVisibleTo(&dlg));
}

// B5.1: the TX applet's CFC right-click opens the CFC dialog in a remote
// window, with the Core's values, and each band's three values change it.
void TstRemoteTxEqCfc::remoteCfcDialogOpensShowsAndChangesTheCoresValues()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setCfcPrecompDb(4);
    QVERIFY(s.connect());
    TransmitModel& windowTx = s.window.transmitModel();
    QTRY_COMPARE(windowTx.cfcPrecompDb(), 4);

    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    auto* cfcBtn = applet.findChild<QPushButton*>(QStringLiteral("TxCfcButton"));
    QVERIFY(cfcBtn);
    QMetaObject::invokeMethod(cfcBtn, "customContextMenuRequested", Q_ARG(QPoint, QPoint()));
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg);
    QVERIFY(dlg->isVisible());
    QCOMPARE(dlg->precompSpin()->value(), 4.0);

    dlg->selectedBandSpin()->setValue(3);
    dlg->compSpin()->setValue(9.0);
    dlg->gainSpin()->setValue(-5.0);
    dlg->freqSpin()->setValue(333);  // between its neighbours' 250 and 500 Hz
    dlg->precompSpin()->setValue(11.0);
    dlg->postEqGainSpin()->setValue(-6.0);
    QTRY_COMPARE(coreTx.cfcPostEqGainDb(), -6);
    QTRY_COMPARE(coreTx.cfcCompression(2), 9);
    QCOMPARE(coreTx.cfcPostEqBandGain(2), -5);
    QCOMPARE(coreTx.cfcPrecompDb(), 11);
    QTRY_COMPARE(coreTx.cfcEqFreq(2), 333);
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileGForTest().at(2), 9.0);
    QCOMPARE(s.txChannel.lastTxCfcProfileEForTest().at(2), -5.0);
    QCOMPARE(s.txChannel.lastTxCfcPrecompDbForTest(), 11.0);

    // A Core change shows in the open dialog.
    coreTx.setCfcPrecompDb(2);
    QTRY_COMPARE(dlg->precompSpin()->value(), 2.0);

    // Greyed with the reason while the Core cannot take it.
    applet.setTxProcessingPermitted(false, kOnAir);
    QVERIFY(!dlg->precompSpin()->isEnabled());
    QVERIFY(!dlg->compWidget()->isEnabled());
    QCOMPARE(dlg->settingsReasonLabel()->text(), kOnAir);
    QVERIFY(dlg->settingsReasonLabel()->isVisibleTo(dlg));
    applet.setTxProcessingPermitted(true);
    QVERIFY(dlg->precompSpin()->isEnabled());

    // The EQ right-click opens the TX EQ dialog.
    auto* eqBtn = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
    QVERIFY(eqBtn);
    QMetaObject::invokeMethod(eqBtn, "customContextMenuRequested", Q_ARG(QPoint, QPoint()));
    TxEqDialog* eq = TxEqDialog::instance(&s.window);
    QVERIFY(eq && eq->isVisible());
    eq->hide();
}

// B5.9 / B5.11: Setup > DSP > CFC and AGC/ALC in a remote window.
void TstRemoteTxEqCfc::remoteSetupPagesChangeTheCoresValues()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();

    CfcSetupPage cfc(&s.window);
    auto* phRot = cfc.findChild<QCheckBox*>(QStringLiteral("chkPHROTEnable"));
    auto* phFreq = cfc.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq"));
    auto* phStages = cfc.findChild<QSpinBox*>(QStringLiteral("udPHROTStages"));
    auto* phRev = cfc.findChild<QCheckBox*>(QStringLiteral("chkPHROTReverse"));
    auto* peq = cfc.findChild<QCheckBox*>(QStringLiteral("chkCFCPeqEnable"));
    auto* precomp = cfc.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp"));
    auto* postGain = cfc.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain"));
    auto* cessb = cfc.findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable"));
    auto* bands = cfc.findChild<QPushButton*>(QStringLiteral("btnCFCBandsConfigure"));
    QVERIFY(phRot && phFreq && phStages && phRev && peq && precomp && postGain && cessb && bands);
    // Closed until the dialog pushes the version 4 gate, with the reason.
    QVERIFY(!phRot->isEnabled());
    QCOMPARE(phRot->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    QVERIFY(bands->isEnabled());
    cfc.setTransmitSettingsPermittedAt(4, true, QString());
    QVERIFY(phRot->isEnabled() && cessb->isEnabled());

    phRot->setChecked(!coreTx.phaseRotatorEnabled());
    const bool phRotWant = phRot->isChecked();
    phFreq->setValue(640);
    phStages->setValue(6);
    phRev->setChecked(!coreTx.phaseReverseEnabled());
    peq->setChecked(!coreTx.cfcPostEqEnabled());
    precomp->setValue(8);
    postGain->setValue(-4);
    cessb->setChecked(!coreTx.cessbOn());
    const bool cessbWant = cessb->isChecked();
    QTRY_COMPARE(coreTx.cessbOn(), cessbWant);
    QTRY_COMPARE(coreTx.cfcPostEqGainDb(), -4);
    QCOMPARE(coreTx.phaseRotatorEnabled(), phRotWant);
    QCOMPARE(coreTx.phaseRotatorFreqHz(), 640);
    QCOMPARE(coreTx.phaseRotatorStages(), 6);
    QCOMPARE(coreTx.cfcPrecompDb(), 8);
    QCOMPARE(s.txChannel.lastTxPhrotCornerHzForTest(), 640.0);
    QCOMPARE(s.txChannel.lastTxCessbOnForTest(), cessbWant);
    cfc.setTransmitSettingsPermittedAt(4, false, kOnAir);
    QVERIFY(!cessb->isEnabled());
    QCOMPARE(cessb->toolTip(), kOnAir);

    AgcAlcSetupPage agc(&s.window);
    const QList<QSpinBox*> lev = groupSpins(&agc, QStringLiteral("TX Leveler"));
    const QList<QSpinBox*> alc = groupSpins(&agc, QStringLiteral("TX ALC"));
    QCOMPARE(lev.size(), 2);
    QCOMPARE(alc.size(), 2);
    QVERIFY(!lev.at(0)->isEnabled());
    agc.setTransmitSettingsPermittedAt(4, true, QString());
    QVERIFY(lev.at(0)->isEnabled());
    lev.at(0)->setValue(12);
    lev.at(1)->setValue(1500);
    alc.at(0)->setValue(80);
    alc.at(1)->setValue(40);
    QTRY_COMPARE(coreTx.txAlcDecay(), 40);
    QCOMPARE(coreTx.txLevelerMaxGain(), 12);
    QCOMPARE(coreTx.txLevelerDecay(), 1500);
    QCOMPARE(coreTx.txAlcMaxGain(), 80);
    QCOMPARE(s.txChannel.lastTxAlcDecayMsForTest(), 40);
}

// B5.11 / B5.13: the pages open with remote transmit denied.
void TstRemoteTxEqCfc::setupOpensCfcAndSpeechProcessorWithoutRemoteTransmit()
{
    RadioModel remote(RadioModel::Role::Remote);
    SetupDialog dialog(&remote);
    dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is not here yet"));
    dialog.setTransmitSettingsPermitted(true, QString(), 4);
    QSignalSpy cfcEditor(&dialog, &SetupDialog::cfcDialogRequested);
    for (const QString& label : {QStringLiteral("CFC"), QStringLiteral("Speech Processor")}) {
        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY2(page, qPrintable(label));
        QVERIFY2(page->isEnabled(), qPrintable(label));
    }
    QWidget* const cfc = dialog.realizedPageForTest(QStringLiteral("CFC"));
    auto* cessb = cfc->findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable"));
    QVERIFY(cessb && cessb->isEnabled());
    cfc->findChild<QPushButton*>(QStringLiteral("btnCFCBandsConfigure"))->click();
    QCOMPARE(cfcEditor.count(), 1);
    // The version 4 gate closes the CFC page's settings with its reason.
    dialog.setTransmitSettingsPermitted(false, kOnAir, 4);
    QVERIFY(!cessb->isEnabled());
    QCOMPARE(cessb->toolTip(), kOnAir);
}

// The ruling's local check: a local window's TX channel gets exactly the
// curve it got before, for both panels, now that RadioModel pushes it.
void TstRemoteTxEqCfc::localWindowPushesTheSameCurvesAsBefore()
{
    auto local = makeStationRadioModel();
    TxChannel channel(1);
    local->wireTransmitChainForTest(&channel);
    TransmitModel& tx = local->transmitModel();
    tx.setTxEqUseLegacy(true);
    tx.setTxEqParaEqData(QString());

    TxEqDialog dlg(local.get());
    // The legacy panel: a band edit gives the ten-band curve.
    dlg.findChild<QSpinBox*>(QStringLiteral("TxEqBandSpin3"))->setValue(5);
    QCOMPARE(tx.txEqBand(3), 5);
    QVERIFY(sameCurve(channel.lastTxEqProfileGForTest(), legacyGains(tx)));
    QVERIFY(sameCurve(channel.lastTxEqProfileFForTest(), legacyFreqs(tx)));

    QVERIFY(channel.lastTxEqProfileQForTest().empty());

    // Into the parametric panel with nothing saved: Thetis's GetDefaults
    // (ParaEQTXData's setter for a blank value), ten flat points from 0 to
    // 4000 Hz with Q 4, until the panel saves a curve.
    dlg.modeSelector()->button(1)->click();
    QVERIFY(!tx.txEqUseLegacy());
    QTest::qWait(150);   // the parametric curve's 100 ms tick (group B fix wave)
    std::vector<double> f;
    std::vector<double> g;
    std::vector<double> q;
    QVERIFY(tx.txEqParaEqData().isEmpty());
    QVERIFY(sameCurve(channel.lastTxEqProfileFForTest(),
                      {0, 0, 4000.0 / 9, 8000.0 / 9, 12000.0 / 9, 16000.0 / 9,
                       20000.0 / 9, 24000.0 / 9, 28000.0 / 9, 32000.0 / 9, 4000}));
    QVERIFY(sameCurve(channel.lastTxEqProfileGForTest(), std::vector<double>(11, 0.0)));
    QVERIFY(sameCurve(channel.lastTxEqProfileQForTest(), {0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}));

    // A parametric edit: every point of the widget's curve, with Q.
    dlg.parametricWidget()->setSelectedIndex(5);
    dlg.findChild<QDoubleSpinBox*>(QStringLiteral("TxEqParaGainSpin"))->setValue(-3.5);
    QTest::qWait(150);   // the parametric curve's 100 ms tick (group B fix wave)
    widgetProfile(*dlg.parametricWidget(), f, g, q);
    QCOMPARE(g[6], -3.5);
    QVERIFY(sameCurve(channel.lastTxEqProfileFForTest(), f));
    QVERIFY(sameCurve(channel.lastTxEqProfileGForTest(), g));
    QVERIFY(sameCurve(channel.lastTxEqProfileQForTest(), q));
    QCOMPARE(q.size(), std::size_t{11});
    // Five bands: five points, no sampling.
    dlg.findChild<QRadioButton*>(QStringLiteral("TxEqParaBands5Radio"))->setChecked(true);
    dlg.findChild<QPushButton*>(QStringLiteral("TxEqCountApplyBtn"))->click();
    QCOMPARE(dlg.parametricWidget()->bandCount(), 5);
    QTest::qWait(150);   // the parametric curve's 100 ms tick (group B fix wave)
    widgetProfile(*dlg.parametricWidget(), f, g, q);
    QCOMPARE(f.size(), std::size_t{6});
    QVERIFY(sameCurve(channel.lastTxEqProfileFForTest(), f));
    QVERIFY(sameCurve(channel.lastTxEqProfileGForTest(), g));
    QVERIFY(sameCurve(channel.lastTxEqProfileQForTest(), q));
    // Q factors off: the same points, no Q.
    auto* useQ = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqParaUseQFactorsChk"));
    QVERIFY(useQ);
    useQ->setChecked(false);
    QVERIFY(!dlg.parametricWidget()->parametricEq());
    QTest::qWait(150);   // the parametric curve's 100 ms tick (group B fix wave)
    QVERIFY(channel.lastTxEqProfileQForTest().empty());
    QCOMPARE(channel.lastTxEqProfileFForTest().size(), std::size_t{6});

    // Back to the legacy panel: the ten-band curve again.
    dlg.modeSelector()->button(0)->click();
    QVERIFY(tx.txEqUseLegacy());
    QVERIFY(sameCurve(channel.lastTxEqProfileGForTest(), legacyGains(tx)));

    // The dialog itself no longer holds the Legacy box as this computer's
    // setting.
    QVERIFY(!AppSettings::instance().contains(QStringLiteral("TxEqDialog/UsingLegacyEQ")));
    local->injectTxChannelForTest(nullptr);
}

void TstRemoteTxEqCfc::legacyBoxSeedsFromThisComputersOldValue()
{
    auto& settings = AppSettings::instance();
    const QString mac = QStringLiteral("11:22:33:44:55:66");
    settings.remove(QStringLiteral("hardware/%1/tx/EQUseLegacy").arg(mac));
    settings.setValue(QStringLiteral("TxEqDialog/UsingLegacyEQ"), QStringLiteral("False"));
    {
        TransmitModel tx;
        tx.loadFromSettings(mac);
        QVERIFY(!tx.txEqUseLegacy());
        QCOMPARE(settings.value(QStringLiteral("hardware/%1/tx/EQUseLegacy").arg(mac)).toString(),
                 QStringLiteral("False"));
    }
    // Once seeded, the radio's own value wins.
    settings.setValue(QStringLiteral("hardware/%1/tx/EQUseLegacy").arg(mac), QStringLiteral("True"));
    {
        TransmitModel tx;
        tx.loadFromSettings(mac);
        QVERIFY(tx.txEqUseLegacy());
    }
    // No old value: the Thetis default, true.
    settings.remove(QStringLiteral("TxEqDialog/UsingLegacyEQ"));
    const QString other = QStringLiteral("11:22:33:44:55:77");
    {
        TransmitModel tx;
        tx.setTxEqUseLegacy(false);
        tx.loadFromSettings(other);
        QVERIFY(tx.txEqUseLegacy());
    }
}

void TstRemoteTxEqCfc::txProfileCarriesTheLegacyBox()
{
    MicProfileManager mgr;
    mgr.setMacAddress(QStringLiteral("11:22:33:44:55:88"));
    mgr.load();
    const auto defaults = MicProfileManager::defaultProfileValues();
    QCOMPARE(defaults.value(QStringLiteral("EQUseLegacy")).toString(), QStringLiteral("True"));
    TransmitModel tx;
    tx.setTxEqUseLegacy(false);
    QVERIFY(mgr.saveProfile(QStringLiteral("Para"), &tx));
    tx.setTxEqUseLegacy(true);
    QVERIFY(mgr.setActiveProfile(QStringLiteral("Para"), &tx));
    QVERIFY(!tx.txEqUseLegacy());
    QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
    QVERIFY(tx.txEqUseLegacy());
}

void TstRemoteTxEqCfc::newReasonsArePlain()
{
    TransmitModel tx;
    const struct { QByteArray name; QVariant value; } bad[] = {
        {"txEqPreamp", 99}, {"txEqNc", 1}, {"txEqCtfmode", 2}, {"txEqWintype", 2},
        {"cfcPrecompDb", 99}, {"cfcPostEqGainDb", 99}, {"phaseRotatorFreqHz", 1},
        {"phaseRotatorStages", 1}, {"txLevelerMaxGain", 99}, {"txLevelerDecay", 0},
        {"txAlcMaxGain", 999}, {"txAlcDecay", 0},
        {"txEqBandsJson", QStringLiteral("[]")}, {"txEqFreqsJson", QStringLiteral("[]")},
        {"cfcCompressionJson", QStringLiteral("[]")}, {"cfcEqFreqJson", QStringLiteral("[]")},
        {"cfcPostEqBandGainJson", QStringLiteral("[]")},
    };
    for (const auto& b : bad) {
        const QString reason = tx.settingRangeRefusal(b.name, b.value);
        QVERIFY2(!reason.isEmpty(), b.name.constData());
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(!reason.contains(QChar(0x2014)));
    }
}

// Group A fix wave, I1: the TX channel runs on its own thread on a Core. The
// EQ curve must be read from the model on the main thread when it changes
// and handed to the channel by value; a push that decodes the model on the
// channel's thread reads the main thread's string while the main thread
// reassigns it. The channel's thread is held while the curve changes twice,
// with a probe queued between the two changes: the first push must carry
// the first curve.
void TstRemoteTxEqCfc::curveIsReadOnTheMainThreadAndHandedByValue()
{
    auto core = makeStationRadioModel();
    TxChannel channel(1);
    core->wireTransmitChainForTest(&channel);
    TransmitModel& tx = core->transmitModel();
    tx.setTxEqParaEqData(flatParametricBlob(0.0));
    tx.setTxEqUseLegacy(false);

    QThread worker;
    worker.start();
    channel.moveToThread(&worker);

    QSemaphore release;
    QSemaphore held;
    QMetaObject::invokeMethod(&channel, [&]() {
        held.release();
        release.acquire();
    });
    held.acquire();

    std::vector<double> firstGains;
    tx.setTxEqParaEqData(flatParametricBlob(6.0));
    // Group B fix wave: parametric pushes are coalesced to Thetis's 100 ms
    // tick, so the +6 dB push is posted when the tick fires, while the
    // channel's thread is still held.
    QTest::qWait(150);
    QMetaObject::invokeMethod(&channel, [&]() {
        firstGains = channel.lastTxEqProfileGForTest();
    });
    tx.setTxEqParaEqData(flatParametricBlob(-6.0));
    release.release();

    // A burst of writes while the channel's thread runs free.
    for (int i = 0; i < 400; ++i) {
        tx.setTxEqParaEqData(flatParametricBlob((i % 2) ? 3.0 : -3.0));
    }
    tx.setTxEqParaEqData(flatParametricBlob(-6.0));
    QTest::qWait(150);   // the tick that posts the last curve

    std::vector<double> lastGains;
    QMetaObject::invokeMethod(&channel, [&]() {
        lastGains = channel.lastTxEqProfileGForTest();
        channel.moveToThread(QCoreApplication::instance()->thread());
    }, Qt::BlockingQueuedConnection);
    worker.quit();
    worker.wait();

    // The first push carries the +6 dB curve it was made for, not the
    // -6 dB one the model held by the time the channel's thread ran.
    QVERIFY(firstGains.size() > 1);
    QVERIFY2(firstGains.at(1) > 0.0, "the push read the model on the channel's thread");
    QVERIFY(lastGains.size() > 1);
    QVERIFY(lastGains.at(1) < 0.0);
    core->injectTxChannelForTest(nullptr);
}

// Group A fix wave, M4: a txEqParaEqData or cfcParaEqData value the Core's
// loader cannot load is refused with a plain reason and changes nothing;
// a loadable curve and the empty "no curve" value are taken.
void TstRemoteTxEqCfc::unreadableCurveIsRefusedWithAReason()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const QString reason =
        QStringLiteral("The Core could not read that equalizer curve. Save the curve again and retry.");
    QVERIFY(OperatorWording::isPlain(reason));

    for (const QByteArray name : {QByteArrayLiteral("txEqParaEqData"),
                                  QByteArrayLiteral("cfcParaEqData")}) {
        const QString good = name == QByteArrayLiteral("cfcParaEqData")
            ? pairedCfcBlob(5) : flatParametricBlob(2.0);
        const SessionPropertyResult ok = s.writeTransmit(name, MirrorWireKind::Utf8, good);
        QVERIFY2(ok.accepted, qPrintable(name + ' ' + ok.reason));
        QCOMPARE(coreTx.property(name.constData()).toString(), good);

        const QString bad[] = {
            QStringLiteral("not a curve"),
            QStringLiteral("{\"points\":[]}"),
            ParaEqEnvelope::encode(QStringLiteral("{\"band_count\":3,\"points\":[{},{}]}")),
        };
        for (const QString& value : bad) {
            const SessionPropertyResult r = s.writeTransmit(name, MirrorWireKind::Utf8, value);
            QVERIFY2(!r.accepted, qPrintable(name + ' ' + value));
            QCOMPARE(r.reason, reason);
            QCOMPARE(coreTx.property(name.constData()).toString(), good);
        }

        const SessionPropertyResult empty =
            s.writeTransmit(name, MirrorWireKind::Utf8, QString());
        QVERIFY2(empty.accepted, qPrintable(name + ' ' + empty.reason));
        QCOMPARE(coreTx.property(name.constData()).toString(), QString());
    }
}

// R-IOS-13 / R-R3-49: today's desktop window declares no txEqCurve, so its
// capabilities, TransmitModel schema, transmit snapshot and deltas carry no
// txEqCurve and no txEqCurveVersion, even after a curve write moves the
// Core's curve; its own TransmitModel works out the same curve from the
// mirrored txEqParaEqData. The declaring side is session-tx-eq-curve.
void TstRemoteTxEqCfc::txEqCurveOnlyToAPeerThatDeclaredIt()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const QString blob = flatParametricBlob(3.0);
    const SessionPropertyResult ok =
        s.writeTransmit(QByteArrayLiteral("txEqParaEqData"), MirrorWireKind::Utf8, blob);
    QVERIFY2(ok.accepted, qPrintable(ok.reason));
    QCOMPARE(s.core->transmitModel().txEqCurve(), ParaEqCurve::txEqCurveJson(blob));
    QVERIFY(s.core->transmitModel().txEqCurve().contains(QStringLiteral("\"state\":\"saved\"")));
    // A change made at the Core reaches the window as a delta (the
    // window's own write above is answered only by its property.result).
    const QString atCore = flatParametricBlob(-4.0);
    s.core->transmitModel().setTxEqParaEqData(atCore);
    QTRY_COMPARE(s.window.transmitModel().txEqParaEqData(), atCore);
    for (const QByteArray& wire : s.windowEnd->received()) {
        QVERIFY2(!wire.contains("txEqCurve"), wire.left(200).constData());
    }
    QCOMPARE(s.client->capabilities().txEqCurveVersion, 0);
    // The window derives the same curve itself.
    QCOMPARE(s.window.transmitModel().txEqCurve(), s.core->transmitModel().txEqCurve());
}

void TstRemoteTxEqCfc::pairedCfcReachesWdspAndRejectsLegacyWidthMismatch()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& tx = s.core->transmitModel();
    const QString eighteen = pairedCfcBlob(18);
    const SessionPropertyResult write = s.writeTransmit(
        QByteArrayLiteral("cfcParaEqData"), MirrorWireKind::Utf8, eighteen);
    QVERIFY2(write.accepted, qPrintable(write.reason));
    QCOMPARE(tx.cfcParaEqData(), eighteen);
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileFForTest().size(), std::size_t(18));
    QCOMPARE(s.txChannel.lastTxCfcProfileGForTest().at(3), 5.0);
    QCOMPARE(s.txChannel.lastTxCfcProfileEForTest().at(3), -3.0);
    QCOMPARE(s.txChannel.lastTxCfcProfileQgForTest().at(3), 2.0);
    QCOMPARE(s.txChannel.lastTxCfcProfileQeForTest().at(3), 6.0);
    QCOMPARE(s.txChannel.lastTxCfcPrecompDbForTest(), 6.0);
    QCOMPARE(s.txChannel.lastTxCfcPrePeqDbForTest(), -7.0);
    const SessionPropertyResult old = s.writeTransmit(
        QByteArrayLiteral("cfcCompressionJson"), MirrorWireKind::Utf8,
        QStringLiteral("[5,5,5,5,5,5,5,5,5,5]"));
    QVERIFY(!old.accepted);
    QCOMPARE(tx.cfcParaEqData(), eighteen);
    const SessionPropertyResult malformed = s.writeTransmit(
        QByteArrayLiteral("cfcParaEqData"), MirrorWireKind::Utf8,
        flatParametricBlob(2.0));
    QVERIFY(!malformed.accepted);
    QCOMPARE(tx.cfcParaEqData(), eighteen);

    const QString fiveGraphic = pairedCfcBlob(5, false);
    QVERIFY(s.writeTransmit(QByteArrayLiteral("cfcParaEqData"), MirrorWireKind::Utf8,
                            fiveGraphic).accepted);
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileFForTest().size(), std::size_t(5));
    QVERIFY(s.txChannel.lastTxCfcProfileQgForTest().empty());
    QVERIFY(s.txChannel.lastTxCfcProfileQeForTest().empty());

    QVERIFY(s.writeTransmit(QByteArrayLiteral("cfcParaEqData"), MirrorWireKind::Utf8,
                            pairedCfcBlob(10)).accepted);
    const SessionPropertyResult legacy = s.writeTransmit(
        QByteArrayLiteral("cfcCompressionJson"), MirrorWireKind::Utf8,
        QStringLiteral("[5,5,9,5,5,5,5,5,5,5]"));
    QVERIFY2(legacy.accepted, qPrintable(legacy.reason));
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileGForTest().at(2), 9.0);
    QCOMPARE(s.txChannel.lastTxCfcProfileQgForTest().at(2), 2.0);
    QVERIFY(tx.cfcParaEqData() != pairedCfcBlob(10));

    // Thetis passes compression frequencies and post-EQ gains to WDSP;
    // resetting one widget can legitimately leave their axes different.
    CfcProfile::Profile divergent;
    QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), divergent));
    divergent.f[2] += 1.0;
    divergent.postF[2] += 3.0;
    divergent.e[2] = -9.0;
    const QString independent = CfcProfile::encode(divergent);
    QVERIFY(!independent.isEmpty());
    QVERIFY(s.writeTransmit(QByteArrayLiteral("cfcParaEqData"), MirrorWireKind::Utf8,
                            independent).accepted);
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileFForTest().at(2), divergent.f[2]);
    QCOMPARE(s.txChannel.lastTxCfcProfileEForTest().at(2), divergent.e[2]);
    QVERIFY(s.txChannel.lastTxCfcProfileFForTest().at(2) != divergent.postF[2]);
}

void TstRemoteTxEqCfc::profileSwitchRestoresOnlyCoherentCfcCurve()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& tx = s.core->transmitModel();
    MicProfileManager* mgr = s.core->micProfileManager();
    QVERIFY(mgr);
    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:01");
    tx.setMacAddress(mac);
    tx.loadFromSettings(mac);
    mgr->setMacAddress(mac);
    mgr->load();

    const QString eighteen = pairedCfcBlob(18);
    const QString five = pairedCfcBlob(5);
    const QString opaque = QStringLiteral("opaque-imported-cfc-data");
    tx.setCfcParaEqData(eighteen);
    QVERIFY(mgr->saveProfile(QStringLiteral("Cfc18"), &tx));
    tx.setCfcParaEqData(QString());
    tx.setCfcCompression(3, 11);
    QVERIFY(mgr->saveProfile(QStringLiteral("CfcLegacy"), &tx));
    tx.setCfcParaEqData(five);
    QVERIFY(mgr->saveProfile(QStringLiteral("Cfc5"), &tx));
    tx.setCfcParaEqData(opaque);
    QVERIFY(mgr->saveProfile(QStringLiteral("CfcOpaque"), &tx));

    for (const auto& row : {std::pair{QStringLiteral("Cfc18"), 18},
                            std::pair{QStringLiteral("CfcLegacy"), 10},
                            std::pair{QStringLiteral("Cfc5"), 5},
                            std::pair{QStringLiteral("CfcOpaque"), 10}}) {
        const int before = s.txChannel.txCfcProfilePushCountForTest();
        QVERIFY(mgr->setActiveProfile(row.first, &tx));
        QTRY_COMPARE(s.txChannel.txCfcProfilePushCountForTest(), before + 1);
        QCOMPARE(s.txChannel.lastTxCfcProfileFForTest().size(),
                 static_cast<std::size_t>(row.second));
        if (row.first == QStringLiteral("Cfc18")) {
            QCOMPARE(tx.cfcParaEqData(), eighteen);
            QCOMPARE(s.txChannel.lastTxCfcProfileQgForTest().size(), std::size_t(18));
        }
        if (row.first == QStringLiteral("CfcLegacy")) {
            QVERIFY(tx.cfcParaEqData().isEmpty());
            QCOMPARE(s.txChannel.lastTxCfcProfileGForTest().at(3), 11.0);
        }
        if (row.first == QStringLiteral("Cfc5")) { QCOMPARE(tx.cfcParaEqData(), five); }
        if (row.first == QStringLiteral("CfcOpaque")) { QCOMPARE(tx.cfcParaEqData(), opaque); }
    }

    TransmitModel reloaded;
    reloaded.loadFromSettings(mac);
    QCOMPARE(reloaded.cfcParaEqData(), opaque);
    QVERIFY(mgr->setActiveProfile(QStringLiteral("Cfc18"), &reloaded));
    QCOMPARE(reloaded.cfcParaEqData(), eighteen);
}

void TstRemoteTxEqCfc::settingsReloadAppliesFinalCfcEnableAndCurveTogether()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& tx = s.core->transmitModel();
    tx.setCfcEnabled(true);
    tx.setCfcPostEqEnabled(true);
    tx.setCfcParaEqData(pairedCfcBlob(18));
    QTRY_COMPARE(s.txChannel.lastTxCfcProfileFForTest().size(), std::size_t(18));
    QVERIFY(s.txChannel.lastTxCfcRunningForTest());
    QVERIFY(s.txChannel.lastTxCfcPostEqRunningForTest());

    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:02");
    auto& settings = AppSettings::instance();
    const QString prefix = QStringLiteral("hardware/%1/tx/").arg(mac);
    settings.setValue(prefix + QStringLiteral("CFCEnabled"), QStringLiteral("False"));
    settings.setValue(prefix + QStringLiteral("CFCPostEqEnabled"), QStringLiteral("False"));
    settings.setValue(prefix + QStringLiteral("CFCParaEQData"), pairedCfcBlob(5));
    const int before = s.txChannel.txCfcProfilePushCountForTest();
    tx.loadFromSettings(mac);
    QTRY_COMPARE(s.txChannel.txCfcProfilePushCountForTest(), before + 1);
    QCOMPARE(s.txChannel.lastTxCfcProfileFForTest().size(), std::size_t(5));
    QVERIFY(!s.txChannel.lastTxCfcRunningForTest());
    QVERIFY(!s.txChannel.lastTxCfcPostEqRunningForTest());
}

void TstRemoteTxEqCfc::reentrantCfcProjectionKeepsNewestCurve()
{
    TransmitModel tx;
    const QString newest = pairedCfcBlob(5);
    QSignalSpy changed(&tx, &TransmitModel::cfcParaEqDataChanged);
    QObject::connect(&tx, &TransmitModel::cfcPrecompDbChanged, &tx,
                     [&tx, newest](int) { tx.setCfcParaEqData(newest); });
    tx.setCfcParaEqData(pairedCfcBlob(10));
    QCOMPARE(tx.cfcParaEqData(), newest);
    QCOMPARE(tx.cfcPrecompDb(), 6);
    QCOMPARE(tx.cfcPostEqGainDb(), -7);
    QVERIFY(!tx.cfcProfileMutationInProgress());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.at(0).at(0).toString(), newest);
}

// Group A follow-up (group B fix wave): the unkey stops the TX channel on
// its own thread, and tells PureSignal (a main-thread object) the radio is
// going back to receive on the main thread, never on the TX thread. Since
// Task 33 (Thetis's unkey order) both happen at the TX drain's request.
void TstRemoteTxEqCfc::txaFlushedTellsPureSignalOnTheMainThread()
{
    auto core = makeStationRadioModel();
    TxChannel channel(1);
    core->wireTransmitChainForTest(&channel);
    QVERIFY(core->installPureSignalForTest(&channel));
    core->wireTxaFlushedForTest();
    QThread* pureSignalThread = nullptr;
    int told = 0;
    core->setTxaFlushedPureSignalObserverForTest([&]() {
        pureSignalThread = QThread::currentThread();
        ++told;
    });

    QThread worker;
    worker.start();
    channel.moveToThread(&worker);
    emit core->moxController()->txDrainRequested();
    QTRY_COMPARE(told, 1);
    QCOMPARE(pureSignalThread, QCoreApplication::instance()->thread());
    // The channel's own stop still runs on its thread.
    QThread* stopThread = nullptr;
    QMetaObject::invokeMethod(&channel, [&]() {
        stopThread = QThread::currentThread();
        channel.moveToThread(QCoreApplication::instance()->thread());
    }, Qt::BlockingQueuedConnection);
    QCOMPARE(stopThread, &worker);
    worker.quit();
    worker.wait();
    core->injectTxChannelForTest(nullptr);
}

// Group A follow-up (group B fix wave): Thetis sends the parametric TX EQ
// to WDSP at most once per 100 ms (eqform.cs setupWDSPdataFromParaEQ
// marks it pending; dspUpdateTimerTick sends it, setupTimer runs every
// 100 ms). A burst of curve edits makes one rebuild on the TX thread, with
// the last curve; the legacy EQ still pushes at once, as Thetis's
// setTXEQProfile does.
void TstRemoteTxEqCfc::parametricEqPushesAreCoalescedToTheTick()
{
    auto core = makeStationRadioModel();
    TxChannel channel(1);
    core->wireTransmitChainForTest(&channel);
    TransmitModel& tx = core->transmitModel();
    tx.setTxEqUseLegacy(false);
    tx.setTxEqParaEqData(flatParametricBlob(0.0));
    QTest::qWait(150);
    const int before = channel.txEqProfilePushCountForTest();

    for (int i = 0; i < 20; ++i) {
        tx.setTxEqParaEqData(flatParametricBlob((i % 2) ? 4.0 : -4.0));
    }
    tx.setTxEqParaEqData(flatParametricBlob(5.0));
    QCOMPARE(channel.txEqProfilePushCountForTest(), before);
    QTRY_COMPARE(channel.txEqProfilePushCountForTest(), before + 1);
    QVERIFY(channel.lastTxEqProfileGForTest().size() > 1);
    QCOMPARE(channel.lastTxEqProfileGForTest().at(1), 5.0);
    QTest::qWait(150);
    QCOMPARE(channel.txEqProfilePushCountForTest(), before + 1);

    // The legacy EQ: at once, one push per change.
    tx.setTxEqUseLegacy(true);
    QCOMPARE(channel.txEqProfilePushCountForTest(), before + 2);
    tx.setTxEqBand(3, 7);
    QCOMPARE(channel.txEqProfilePushCountForTest(), before + 3);
    core->injectTxChannelForTest(nullptr);
}

// Setup publication (CFC band editor): a Core at transmitSettingsVersion
// 15 takes the dialog's table as one cfc.setProfile command, never as a
// cfcParaEqData write; the 5 and 18-band tables reach it whole; and the
// Core's own change shows in the open dialog.
void TstRemoteTxEqCfc::remoteCfcDialogSendsTheTableAsOneCommand()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    applet.requestOpenCfcDialog();
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg && dlg->isVisible());
    s.coreEnd->clearReceived();

    dlg->precompSpin()->setValue(11.0);
    QTRY_COMPARE(coreTx.cfcPrecompDb(), 11);
    QTRY_COMPARE(s.txChannel.lastTxCfcPrecompDbForTest(), 11.0);

    dlg->bands18Radio()->setChecked(true);
    dlg->findChild<QPushButton*>(QStringLiteral("TxCfcApplyBands"))->click();
    QTRY_COMPARE(coreBandCount(coreTx), 18);
    QTRY_COMPARE(s.window.transmitModel().cfcParaEqData(), coreTx.cfcParaEqData());
    dlg->bands5Radio()->setChecked(true);
    dlg->findChild<QPushButton*>(QStringLiteral("TxCfcApplyBands"))->click();
    QTRY_COMPARE(coreBandCount(coreTx), 5);
    // One command each for the precomp and the two band counts.
    const int beforeBurst = cfcTraffic(s.coreEnd).commands;
    QCOMPARE(beforeBurst, 3);
    // Several quick edits: the newest one wins, one command at a time.
    dlg->postEqGainSpin()->setValue(-2.0);
    dlg->postEqGainSpin()->setValue(-3.0);
    dlg->postEqGainSpin()->setValue(-4.0);
    QTRY_COMPARE(coreTx.cfcPostEqGainDb(), -4);
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());

    // The burst of three post-EQ edits sends at most two commands: the first
    // (unless the band count's answer is still out, which holds it) and the
    // newest.
    const CfcTraffic traffic = cfcTraffic(s.coreEnd);
    QVERIFY2(traffic.commands - beforeBurst >= 1 && traffic.commands - beforeBurst <= 2,
             qPrintable(QString::number(traffic.commands - beforeBurst)));
    QCOMPARE(traffic.propertyWrites, 0);
    QVERIFY(!dlg->profileReasonLabel()->isVisibleTo(dlg));

    // A change made at the Core shows in the open dialog, 18 bands and all.
    coreTx.setCfcParaEqData(pairedCfcBlob(18));
    QTRY_VERIFY(dlg->bands18Radio()->isChecked());
    QTRY_COMPARE(dlg->precompSpin()->value(), 6.0);
    QCOMPARE(dlg->compWidget()->bandCount(), 18);
    coreTx.setCfcPrecompDb(2);
    QTRY_COMPARE(dlg->precompSpin()->value(), 2.0);
    dlg->hide();
}

// A change made from a table the Core has since moved past is refused with
// the reason, and the dialog shows the Core's values again.
void TstRemoteTxEqCfc::remoteCfcDialogShowsAStaleRefusal()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    applet.requestOpenCfcDialog();
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg);
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());

    // The Core changes, and before the window hears of it the dialog sends
    // an edit made on the table it last saw.
    coreTx.setCfcPrecompDb(7);
    dlg->postEqGainSpin()->setValue(-9.0);
    QTRY_COMPARE(dlg->profileReasonLabel()->text(),
                 QStringLiteral("The CFC settings changed on the Core. "
                                "Check the new values and try again."));
    QVERIFY(dlg->profileReasonLabel()->isVisibleTo(dlg));
    QVERIFY(coreTx.cfcPostEqGainDb() != -9);
    QTRY_COMPARE(dlg->precompSpin()->value(), 7.0);
    QCOMPARE(dlg->postEqGainSpin()->value(), static_cast<double>(coreTx.cfcPostEqGainDb()));

    // The next edit, made on the Core's table, goes through and clears it.
    dlg->postEqGainSpin()->setValue(-9.0);
    QTRY_COMPARE(coreTx.cfcPostEqGainDb(), -9);
    QTRY_VERIFY(!dlg->profileReasonLabel()->isVisibleTo(dlg));
    dlg->hide();
}

// A Core before transmitSettingsVersion 15 has no cfc.setProfile; the
// dialog keeps writing cfcParaEqData as before.
void TstRemoteTxEqCfc::remoteCfcDialogKeepsThePropertyWriteForAnOlderCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    s.client->setTransmitSettingsVersionForTest(kTransmitSettingsCfcProfileVersion - 1);
    QVERIFY(!s.client->transmitSettingsAvailable(kTransmitSettingsCfcProfileVersion));
    QVERIFY(s.client->transmitSettingsAvailable(13));
    const IStationLink::CommandOutcome direct =
        s.client->requestCfcProfile(QStringLiteral("{}"), QString());
    QVERIFY(!direct.sent);

    TransmitModel& coreTx = s.core->transmitModel();
    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    applet.requestOpenCfcDialog();
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg);
    s.coreEnd->clearReceived();

    dlg->precompSpin()->setValue(12.0);
    QTRY_COMPARE(coreTx.cfcPrecompDb(), 12);
    dlg->bands18Radio()->setChecked(true);
    dlg->findChild<QPushButton*>(QStringLiteral("TxCfcApplyBands"))->click();
    QTRY_COMPARE(coreBandCount(coreTx), 18);
    const CfcTraffic traffic = cfcTraffic(s.coreEnd);
    QCOMPARE(traffic.commands, 0);
    QVERIFY(traffic.propertyWrites >= 2);
    dlg->hide();
}

// A link lost after the dialog sent a change and before the Core answered:
// the Core may or may not have applied it, and its answer can no longer
// arrive. The dialog drops the change it was waiting on, follows the Core's
// values after the reconnect, and sends the next edit.
void TstRemoteTxEqCfc::remoteCfcDialogFollowsTheCoreAfterAResultIsLost()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    applet.requestOpenCfcDialog();
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg);
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());

    // The Core takes the command, but nothing it says reaches the window.
    s.coreEnd->setDropsOutgoing(true);
    s.coreEnd->clearReceived();
    dlg->precompSpin()->setValue(13.0);
    QTRY_COMPARE(coreTx.cfcPrecompDb(), 13);
    QCOMPARE(cfcTraffic(s.coreEnd).commands, 1);
    s.windowEnd->closeLink(QStringLiteral("test: link lost"));
    QTRY_VERIFY(!s.client->stationLinkReady());

    QVERIFY(s.reconnect(this));
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());
    QTRY_COMPARE(dlg->precompSpin()->value(), 13.0);
    // The Core's next change shows in the dialog.
    coreTx.setCfcPrecompDb(4);
    QTRY_COMPARE(dlg->precompSpin()->value(), 4.0);

    // The next edit goes to the Core as one command.
    s.coreEnd->clearReceived();
    dlg->postEqGainSpin()->setValue(-5.0);
    QTRY_COMPARE(coreTx.cfcPostEqGainDb(), -5);
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());
    const CfcTraffic traffic = cfcTraffic(s.coreEnd);
    QCOMPARE(traffic.commands, 1);
    QCOMPARE(traffic.propertyWrites, 0);
    QVERIFY(!dlg->profileReasonLabel()->isVisibleTo(dlg));
    dlg->hide();
}

// A dialog built before the window reached a Core still sends its table as
// one command once the link comes up.
void TstRemoteTxEqCfc::remoteCfcDialogOpenedBeforeTheLinkSendsWhole()
{
    Session s(m_securityDir.path(), this);
    // No link on the window while the dialog is built.
    s.window.attachStation(nullptr);
    TxApplet applet(&s.window);
    applet.setTxProcessingPermitted(true);
    applet.requestOpenCfcDialog();
    TxCfcDialog* dlg = applet.cfcDialog();
    QVERIFY(dlg);
    s.window.attachStation(s.client.get());
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    QTRY_COMPARE(s.window.transmitModel().cfcProfile(), coreTx.cfcProfile());
    s.coreEnd->clearReceived();

    dlg->precompSpin()->setValue(9.0);
    QTRY_COMPARE(coreTx.cfcPrecompDb(), 9);
    const CfcTraffic traffic = cfcTraffic(s.coreEnd);
    QCOMPARE(traffic.commands, 1);
    QCOMPARE(traffic.propertyWrites, 0);
    dlg->hide();
}

QTEST_MAIN(TstRemoteTxEqCfc)
#include "tst_remote_tx_eq_cfc.moc"
