// =================================================================
// tests/tst_capture_admission.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PC-microphone MOX admission tests
// (R-R3-36, R-R3-21): keying that would read the PC microphone is refused
// before any RF effect while capture is not Ready, and never queued; Radio
// mic, VAX, TCI audio, Tune and two-tone key normally; losing capture while
// keyed releases MOX once, the ordinary way; unkey is never refused.  The
// capture helper is always the scripted fake, run by re-executing this
// binary with --fake-capture-child <scenario>; no real microphone is opened
// and no radio is keyed (the connection is a counting mock).
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QThread>
#include <QSignalSpy>
#include <QUrl>
#include <QWebSocket>

#include <cstring>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TciServer.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/safety/BandPlanGuard.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/FakeCaptureChild.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#endif

using namespace NereusSDR;
using State = CaptureSupervisor::Status::State;

namespace {

const QString kRefusal =
    QStringLiteral("Microphone is not ready. Check Audio settings and retry.");

CaptureSupervisor::Options fakeOptions(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    return options;
}

void killProcess(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (handle != nullptr) {
        TerminateProcess(handle, 9);
        CloseHandle(handle);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif
}

// Connection that records every RF-relevant command RadioModel sends.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
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
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override { ++antennaCalls; }
    void setMox(bool on) override { on ? ++moxOnCalls : ++moxOffCalls; }
    void setTrxRelay(bool) override { ++trxRelayCalls; }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override {}

    int antennaCalls = 0;
    int moxOnCalls = 0;
    int moxOffCalls = 0;
    int trxRelayCalls = 0;
};

enum class Ptt { Mox, RadioMicPtt, Cat, Vox, Space, X2 };

void pressPtt(MoxController* mox, Ptt source)
{
    switch (source) {
    case Ptt::Mox:         mox->setMox(true); break;
    case Ptt::RadioMicPtt: mox->onMicPttFromRadio(true); break;
    case Ptt::Cat:         mox->onCatPtt(true); break;
    case Ptt::Vox:         mox->onVoxActive(true); break;
    case Ptt::Space:       mox->onSpacePtt(true); break;
    case Ptt::X2:          mox->onX2Ptt(true); break;
    }
}

// A local RadioModel standing in for a connected station: counting mock
// connection, a TxChannel with no WDSP channel behind it, the production
// MOX pre-check, zero MOX timers and one USB slice in the 20 m phone band.
// The capture helper is the fake; the test takes capture demand itself.
struct Rig {
    MockConnection conn;
    TxChannel tx{/*channelId=*/1};
    std::unique_ptr<RadioModel> model;
    CaptureSupervisor::Lease lease;
    SliceModel* slice = nullptr;

    explicit Rig(const QString& scenario)
        : model(std::make_unique<RadioModel>())
    {
        model->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5, 192000);
        model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->audioEngine()->setCaptureSupervisorOptionsForTest(fakeOptions(scenario));
        model->injectConnectionForTest(&conn);
        model->injectTxChannelForTest(&tx);
        model->installBandPlanMoxCheckForTest();
        const int id = model->addSlice();
        slice = model->sliceById(id);
        Q_ASSERT(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14'200'000.0);
        model->setActiveSliceById(id);
        QCoreApplication::processEvents();
    }

    ~Rig()
    {
        if (model->moxController()->isMox()) {
            model->moxController()->setMox(false);
            QTest::qWait(20);
        }
        lease.release();
        model->injectTxChannelForTest(nullptr);
        model->injectConnectionForTest(nullptr);
        model.reset();
    }

    MoxController* mox() const { return model->moxController(); }
    AudioEngine* engine() const { return model->audioEngine(); }

    void takeDemand()
    {
        lease = engine()->acquireCaptureDemand(CaptureSupervisor::Demand::LocalSession);
    }
};

// Brings the rig's capture to `state` ("closed" takes no demand).
bool reachCaptureState(Rig& rig, const QString& state)
{
    if (state == QLatin1String("closed")) {
        return rig.engine()->captureStatus().state == State::Closed;
    }
    rig.takeDemand();
    const State want = state == QLatin1String("ready")     ? State::Ready
                     : state == QLatin1String("opening")   ? State::Opening
                                                            : State::Failed;
    return QTest::qWaitFor([&] { return rig.engine()->captureStatus().state == want; }, 8000);
}

QString scenarioFor(const QString& state)
{
    if (state == QLatin1String("opening")) {
        return QStringLiteral("hang-open");
    }
    if (state == QLatin1String("failed")) {
        // An unknown scenario makes the fake exit at once with code 2.
        return QStringLiteral("no-such-scenario");
    }
    return QStringLiteral("ready");
}

} // namespace

class TstCaptureAdmission : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(
            QStringLiteral("BandPlanRegion"),
            QString::number(static_cast<int>(safety::Region::UnitedStates)));
    }

    void cleanup() { AppSettings::instance().clear(); }

    // PC mic selected, capture not Ready: every PTT source is refused with
    // the exact text and nothing reaches the radio or the state machine.
    void remoteRadioCandidateDoesNotExemptIdleOrNextLocalPcKey()
    {
        Rig rig(QStringLiteral("ready"));
        const QByteArray device("radio-window");
        const QString owner("station:radio-session");
        rig.model->setRemoteMicSelection(owner, device, RemoteMicSource::RadioMic);
        QVERIFY(reachCaptureState(rig, QStringLiteral("closed")));
        rig.mox()->setMox(true);
        QVERIFY(!rig.mox()->isMox());
        KeyerIdentity keyer;
        keyer.deviceId = device;
        keyer.session = owner;
        rig.model->beginRemoteRadioKeyAttempt(owner, device, 1);
        rig.mox()->setMox(true, keyer);
        QVERIFY(rig.mox()->isMox());
        rig.model->finishRemoteRadioKeyAttempt(owner, device, 1, 7);
        rig.mox()->setMox(false, keyer);
        QTRY_COMPARE(rig.mox()->state(), MoxState::Rx);
        rig.mox()->setMox(true);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.model->transmitModel().micSource(), MicSource::Pc);
    }

    void remoteRadioKeySurvivesPcCaptureLossButRetiresAtHardwareOff()
    {
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        const QByteArray device("radio-window");
        const QString owner("station:radio-session");
        KeyerIdentity keyer;
        keyer.deviceId = device;
        keyer.session = owner;
        rig.model->setRemoteMicSelection(owner, device, RemoteMicSource::RadioMic);
        rig.model->beginRemoteRadioKeyAttempt(owner, device, 1);
        rig.mox()->setMox(true, keyer);
        QVERIFY(rig.mox()->isMox());
        RadioModel::KeyedBy keyed;
        keyed.deviceId = device;
        keyed.epoch = 7;
        rig.model->setKeyedBy(keyed);
        rig.model->finishRemoteRadioKeyAttempt(owner, device, 1, 7);
        QSignalSpy ending(rig.mox(), &MoxController::txAboutToEnd);
        const qint64 pid = rig.engine()->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);
        killProcess(pid);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Failed, 5000);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(ending.count(), 0);
        QVERIFY(rig.model->remoteRadioMicKeyActive(device));
        rig.mox()->setMox(false, keyer);
        QTRY_COMPARE(rig.mox()->state(), MoxState::Rx);
        QVERIFY(!rig.model->remoteRadioMicKeyActive(device));
        rig.mox()->setMox(true);
        QVERIFY(!rig.mox()->isMox());
    }

    void explicitKeyScopeMasksReentrantLocalCallAndRollsBack()
    {
        Rig rig(QStringLiteral("ready"));
        KeyerIdentity keyer;
        keyer.deviceId = QByteArray("radio-window");
        keyer.session = QStringLiteral("station:radio-session");
        bool nested = false;
        bool sawStation = false;
        rig.mox()->setMoxCheck([&]() {
            const auto identity = rig.mox()->keyAttemptIdentity();
            if (!nested && identity && identity->deviceId == keyer.deviceId) {
                nested = true;
                rig.mox()->setMox(true);
                nested = false;
            } else if (nested) {
                sawStation = identity && identity->isStation();
            }
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = false;
            result.reason = QStringLiteral("Refused scoped attempt");
            return result;
        });
        rig.mox()->setMox(true, keyer);
        QVERIFY(sawStation);
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(!rig.mox()->keyAttemptIdentity().has_value());
    }

    void refusalHasNoRfEffect_data()
    {
        QTest::addColumn<int>("ptt");
        QTest::addColumn<QString>("capture");
        QTest::newRow("mox/closed")          << int(Ptt::Mox)         << "closed";
        QTest::newRow("radio-mic-ptt/closed") << int(Ptt::RadioMicPtt) << "closed";
        QTest::newRow("cat/closed")          << int(Ptt::Cat)         << "closed";
        QTest::newRow("vox/closed")          << int(Ptt::Vox)         << "closed";
        QTest::newRow("space/closed")        << int(Ptt::Space)       << "closed";
        QTest::newRow("x2/closed")           << int(Ptt::X2)          << "closed";
        QTest::newRow("mox/opening")         << int(Ptt::Mox)         << "opening";
        QTest::newRow("mox/failed")          << int(Ptt::Mox)         << "failed";
        QTest::newRow("vox/failed")          << int(Ptt::Vox)         << "failed";
    }
    void refusalHasNoRfEffect()
    {
        QFETCH(int, ptt);
        QFETCH(QString, capture);
        Rig rig(scenarioFor(capture));
        QCOMPARE(rig.model->transmitModel().micSource(), MicSource::Pc);
        QVERIFY(rig.model->pcCaptureRequired());
        QVERIFY(reachCaptureState(rig, capture));

        const int antenna0 = rig.conn.antennaCalls;
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        QSignalSpy changing(rig.mox(), &MoxController::moxChanging);
        QSignalSpy aboutToBegin(rig.mox(), &MoxController::txAboutToBegin);
        QSignalSpy flipped(rig.mox(), &MoxController::hardwareFlipped);
        QSignalSpy stateChanged(rig.mox(), &MoxController::stateChanged);
        QSignalSpy txReady(rig.mox(), &MoxController::txReady);

        pressPtt(rig.mox(), static_cast<Ptt>(ptt));
        QTest::qWait(50);

        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), kRefusal);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.mox()->state(), MoxState::Rx);
        QCOMPARE(changing.count(), 0);
        QCOMPARE(aboutToBegin.count(), 0);
        QCOMPARE(flipped.count(), 0);
        QCOMPARE(stateChanged.count(), 0);
        QCOMPARE(txReady.count(), 0);
        QCOMPARE(rig.conn.antennaCalls, antenna0);
        QCOMPARE(rig.conn.moxOnCalls, 0);
        QCOMPARE(rig.conn.trxRelayCalls, 0);

        // Unkey is never refused.
        rig.mox()->setMox(false);
        QCOMPARE(rejected.count(), 1);
    }

    // A refused press is not queued: capture reaching Ready afterwards
    // does not key. A new press with Ready keys through the same check.
    void readyLaterDoesNotKeyANewPressDoes_data()
    {
        QTest::addColumn<int>("ptt");
        QTest::newRow("mox") << int(Ptt::Mox);
        QTest::newRow("vox") << int(Ptt::Vox);
    }
    void readyLaterDoesNotKeyANewPressDoes()
    {
        QFETCH(int, ptt);
        Rig rig(QStringLiteral("ready"));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        QSignalSpy flipped(rig.mox(), &MoxController::hardwareFlipped);

        pressPtt(rig.mox(), static_cast<Ptt>(ptt));
        QCOMPARE(rejected.count(), 1);

        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        QTest::qWait(300);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(flipped.count(), 0);
        QCOMPARE(rig.conn.moxOnCalls, 0);

        // A new press: VOX goes inactive first (DEXP pushes only changes),
        // as a held source refused for the microphone is not tried again
        // until it is released (heldRefusedPressIsNotQueued).
        if (static_cast<Ptt>(ptt) == Ptt::Vox) {
            rig.mox()->onVoxActive(false);
        }
        pressPtt(rig.mox(), static_cast<Ptt>(ptt));
        QTest::qWait(50);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(flipped.count(), 1);
        QCOMPARE(rig.conn.moxOnCalls, 1);

        rig.mox()->setMox(false);
        QTest::qWait(50);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rejected.count(), 1);
    }

    // Keying that does not read the PC microphone keys normally with
    // capture Closed or Failed, and none of it changes the session's
    // capture requirement (so the session demand does not open and close
    // with Tune, two-tone or TCI audio).
    void exemptKeyingKeysWithCaptureNotReady_data()
    {
        QTest::addColumn<QString>("keying");
        QTest::addColumn<QString>("capture");
        for (const char* keying : {"radio-mic", "vax", "tci-audio", "tune", "two-tone"}) {
            for (const char* capture : {"closed", "failed"}) {
                QTest::newRow(qPrintable(QStringLiteral("%1/%2").arg(QLatin1String(keying),
                                                                    QLatin1String(capture))))
                    << QString::fromLatin1(keying) << QString::fromLatin1(capture);
            }
        }
    }
    void exemptKeyingKeysWithCaptureNotReady()
    {
        QFETCH(QString, keying);
        QFETCH(QString, capture);
        Rig rig(scenarioFor(capture));
        QVERIFY(reachCaptureState(rig, capture));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        TwoToneController* const twoTone = rig.model->twoToneController();

        if (keying == QLatin1String("radio-mic")) {
            rig.model->transmitModel().setMicSource(MicSource::Radio);
            QVERIFY(!rig.model->pcCaptureRequired());
            rig.mox()->setMox(true);
        } else if (keying == QLatin1String("vax")) {
            rig.model->transmitModel().setMicSource(MicSource::Vax);
            QVERIFY(!rig.model->pcCaptureRequired());
            rig.mox()->setMox(true);
        } else if (keying == QLatin1String("tci-audio")) {
            rig.tx.setTciAudioActive(true);
            rig.mox()->setMox(true);
            QVERIFY(rig.model->pcCaptureRequired());
        } else if (keying == QLatin1String("tune")) {
            rig.model->setTune(true);
            QVERIFY(rig.mox()->isManualMox());
            QVERIFY(rig.model->pcCaptureRequired());
        } else {
            twoTone->setTxChannel(&rig.tx);
            twoTone->setSettleDelaysMs(0, 0);
            twoTone->setActive(true);
            QVERIFY(twoTone->isActive());
            QVERIFY(rig.model->pcCaptureRequired());
        }
        QTest::qWait(50);

        QCOMPARE(rejected.count(), 0);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 1);

        if (keying == QLatin1String("tune")) {
            rig.model->setTune(false);
        } else if (keying == QLatin1String("two-tone")) {
            twoTone->setActive(false);
        } else {
            rig.mox()->setMox(false);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QTest::qWait(250);  // let the tune-off and two-tone settle timers run
        rig.tx.setTciAudioActive(false);
        twoTone->setTxChannel(nullptr);
    }

    // Losing capture while PC-mic keyed releases MOX once, the ordinary
    // way; the TX input reads nothing while it is not Ready, and capture
    // coming back does not key again.
    void inputLossWhileKeyedReleasesOnce_data()
    {
        QTest::addColumn<QString>("loss");
        QTest::newRow("helper-killed") << "kill";
        QTest::newRow("demand-released") << "release";
    }
    void inputLossWhileKeyedReleasesOnce()
    {
        QFETCH(QString, loss);
        Rig rig(QStringLiteral("ready"));
        rig.engine()->onMicSourceChanged(true);
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);
        QSignalSpy aboutToBegin(rig.mox(), &MoxController::txAboutToBegin);
        QSignalSpy status(rig.engine(), &AudioEngine::captureStatusChanged);

        rig.mox()->onVoxActive(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(aboutToBegin.count(), 1);

        // The release is logged once (the raw state stays in the log).
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral(
                                 "^PC microphone left Ready while keyed; releasing MOX\\.")));
        if (loss == QLatin1String("kill")) {
            const qint64 pid = rig.engine()->captureHelperProcessIdForTest();
            QVERIFY(pid > 0);
            killProcess(pid);
        } else {
            rig.lease.release();
        }
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            rig.engine()->captureStatus().state == State::Failed
                || rig.engine()->captureStatus().state == State::Closed,
            5000);
        QTest::qWait(300);
        QVERIFY(status.count() >= 1);
        QCOMPARE(aboutToEnd.count(), 1);
        QCOMPARE(rig.conn.moxOffCalls, 1);
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(rig.mox()->state(), MoxState::Rx);

        // The TX input reads nothing while capture is not Ready.
        QVERIFY(!rig.engine()->isPcMicOverrideActive());
        float buffer[64] = {};
        QCOMPARE(rig.engine()->pullTxMic(buffer, 64), 0);

        // Capture coming back does not key.
        if (loss == QLatin1String("kill")) {
            rig.engine()->retryCapture();
        } else {
            rig.takeDemand();
        }
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Ready, 8000);
        QTest::qWait(300);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(aboutToBegin.count(), 1);
        QCOMPARE(rig.conn.moxOnCalls, 1);
    }

    // A refused Tune leaves no tune intent behind: a voice press that
    // follows with capture not Ready is refused with the microphone text.
    void refusedTuneThenVoicePressIsRefused_data()
    {
        QTest::addColumn<int>("ptt");
        QTest::newRow("mox") << int(Ptt::Mox);
        QTest::newRow("vox") << int(Ptt::Vox);
        QTest::newRow("cat") << int(Ptt::Cat);
    }
    void refusedTuneThenVoicePressIsRefused()
    {
        QFETCH(int, ptt);
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("closed")));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        // Out of band: the band plan refuses the Tune press.
        rig.slice->setFrequency(4'500'000.0);
        rig.model->setTune(true);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(rejected.at(0).at(0).toString() != kRefusal);
        QVERIFY(!rig.mox()->isMox());

        // Back in band, a voice press reads the PC microphone.
        rig.slice->setFrequency(14'200'000.0);
        pressPtt(rig.mox(), static_cast<Ptt>(ptt));
        QTest::qWait(50);
        QCOMPARE(rejected.count(), 2);
        QCOMPARE(rejected.at(1).at(0).toString(), kRefusal);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 0);
        QCOMPARE(rig.conn.trxRelayCalls, 0);

        rig.model->setTune(false);
        QTest::qWait(250);
    }

    // Two-tone's 200 ms MOX-release settle is not two-tone keying: a voice
    // press inside it with capture not Ready is refused, and two-tone's own
    // key at the end of the walk is still admitted.
    void twoToneSettleWindowVoicePressIsRefused()
    {
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("closed")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setSettleDelaysMs(400, 0);

        // Key with TCI audio (exempt), then drop TCI audio so two-tone's
        // activation starts with MOX on and has to release and settle.
        rig.tx.setTciAudioActive(true);
        rig.mox()->setMox(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        rig.tx.setTciAudioActive(false);
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        twoTone->setActive(true);
        QVERIFY(twoTone->isActivationInFlight());
        QVERIFY(!rig.mox()->isMox());

        rig.mox()->setMox(true);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), kRefusal);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 1);

        twoTone->setActive(false);
        QTest::qWait(600);
        QVERIFY(!rig.mox()->isMox());
        twoTone->setTxChannel(nullptr);
    }

    // Two-tone's own key is admitted with capture not Ready even when it
    // follows the settle (the scoped key call, not a stale flag).
    void twoToneKeysAfterSettleWithCaptureNotReady()
    {
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("closed")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setSettleDelaysMs(100, 0);

        rig.tx.setTciAudioActive(true);
        rig.mox()->setMox(true);
        QTest::qWait(50);
        rig.tx.setTciAudioActive(false);
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        twoTone->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(twoTone->isActive(), 2000);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(rig.conn.moxOnCalls, 2);

        twoTone->setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QTest::qWait(300);
        twoTone->setTxChannel(nullptr);
    }

    // Input loss during a stale-flag window still releases a PC-mic key:
    // after a refused Tune, and inside two-tone's MOX-release settle.
    void inputLossDuringStaleWindowReleases_data()
    {
        QTest::addColumn<QString>("window");
        QTest::newRow("refused-tune") << "refused-tune";
        QTest::newRow("two-tone-settle") << "two-tone-settle";
    }
    void inputLossDuringStaleWindowReleases()
    {
        QFETCH(QString, window);
        Rig rig(QStringLiteral("ready"));
        rig.engine()->onMicSourceChanged(true);
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        TwoToneController* const twoTone = rig.model->twoToneController();

        if (window == QLatin1String("refused-tune")) {
            rig.slice->setFrequency(4'500'000.0);
            rig.model->setTune(true);
            QVERIFY(!rig.mox()->isMox());
            rig.slice->setFrequency(14'200'000.0);
        } else {
            twoTone->setTxChannel(&rig.tx);
            twoTone->setSettleDelaysMs(4000, 0);
            rig.mox()->onVoxActive(true);
            QTest::qWait(50);
            QVERIFY(rig.mox()->isMox());
            twoTone->setActive(true);
            QVERIFY(twoTone->isActivationInFlight());
            QVERIFY(!rig.mox()->isMox());
        }

        // PC-mic keying admitted with capture Ready.
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        rig.mox()->setMox(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rejected.count(), 0);
        const int off0 = rig.conn.moxOffCalls;
        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral(
                                 "^PC microphone left Ready while keyed; releasing MOX\\.")));
        const qint64 pid = rig.engine()->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);
        killProcess(pid);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 3000);
        QTest::qWait(50);
        QCOMPARE(aboutToEnd.count(), 1);
        QCOMPARE(rig.conn.moxOffCalls, off0 + 1);
        QVERIFY(twoTone->isActivationInFlight() == (window == QLatin1String("two-tone-settle")));

        if (window == QLatin1String("refused-tune")) {
            rig.model->setTune(false);
        } else {
            twoTone->setActive(false);
        }
        QTest::qWait(300);
        QVERIFY(!rig.mox()->isMox());
        twoTone->setTxChannel(nullptr);
    }

    // Keying that does not read the PC microphone is not released by a
    // capture change.
    void captureLossDoesNotReleaseTune_data()
    {
        QTest::addColumn<QString>("keying");
        QTest::newRow("tune") << "tune";
        QTest::newRow("two-tone") << "two-tone";
    }
    void captureLossDoesNotReleaseTune()
    {
        QFETCH(QString, keying);
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        if (keying == QLatin1String("tune")) {
            rig.model->setTune(true);
        } else {
            twoTone->setTxChannel(&rig.tx);
            twoTone->setSettleDelaysMs(0, 0);
            twoTone->setActive(true);
        }
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);

        killProcess(rig.engine()->captureHelperProcessIdForTest());
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Failed, 5000);
        QTest::qWait(100);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(aboutToEnd.count(), 0);

        if (keying == QLatin1String("tune")) {
            rig.model->setTune(false);
        } else {
            twoTone->setActive(false);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QTest::qWait(250);
        twoTone->setTxChannel(nullptr);
    }

    // A press refused while two-tone is live (here by the band plan after
    // the VFO moves out of band) is not two-tone's key: two-tone keeps
    // running on its own key, and input loss leaves it alone.
    void refusedPressDuringLiveTwoToneKeepsTwoTone()
    {
        Rig rig(QStringLiteral("ready"));
        rig.engine()->onMicSourceChanged(true);
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        QTest::qWait(50);
        QVERIFY(twoTone->isActive());
        QVERIFY(rig.mox()->isMox());
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        QSignalSpy activeChanged(twoTone, &TwoToneController::twoToneActiveChanged);

        rig.slice->setFrequency(4'500'000.0);
        rig.mox()->setMox(true);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(rejected.at(0).at(0).toString() != kRefusal);
        QVERIFY(twoTone->isActive());
        QCOMPARE(activeChanged.count(), 0);
        QVERIFY(rig.mox()->isMox());

        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);
        killProcess(rig.engine()->captureHelperProcessIdForTest());
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Failed, 5000);
        QTest::qWait(100);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(aboutToEnd.count(), 0);

        rig.slice->setFrequency(14'200'000.0);
        twoTone->setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QTest::qWait(300);
        QVERIFY(!twoTone->isActive());
        twoTone->setTxChannel(nullptr);
    }

    // Two-tone going inactive while MOX stays keyed leaves no generated-key
    // record behind: whatever holds the key now reads the PC microphone,
    // so input loss releases it.
    void twoToneInactiveWhileKeyedReleasesOnInputLoss()
    {
        Rig rig(QStringLiteral("ready"));
        rig.engine()->onMicSourceChanged(true);
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        QTest::qWait(50);
        QVERIFY(twoTone->isActive());
        QVERIFY(rig.mox()->isMox());

        // No production path stops two-tone without unkeying once its
        // rejection handler reacts only to its own key; drive the signal
        // directly so the record's reaction is covered on its own.
        emit twoTone->twoToneActiveChanged(false);
        QVERIFY(rig.mox()->isMox());

        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral(
                                 "^PC microphone left Ready while keyed; releasing MOX\\.")));
        killProcess(rig.engine()->captureHelperProcessIdForTest());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 3000);
        QTest::qWait(50);
        QCOMPARE(aboutToEnd.count(), 1);

        twoTone->setActive(false);
        QTest::qWait(300);
        twoTone->setTxChannel(nullptr);
    }

    // A voice key admitted inside two-tone's MOX-release settle becomes
    // two-tone's key when the walk's own setMox(true) finds MOX already on
    // (an idempotent repeat that commits nothing). Input loss then leaves
    // the live two-tone alone.
    void voiceKeyInSettleAdoptedByTwoToneSurvivesInputLoss()
    {
        Rig rig(QStringLiteral("ready"));
        rig.engine()->onMicSourceChanged(true);
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        TwoToneController* const twoTone = rig.model->twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setSettleDelaysMs(400, 0);

        rig.mox()->onVoxActive(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        twoTone->setActive(true);
        QVERIFY(twoTone->isActivationInFlight());
        QVERIFY(!rig.mox()->isMox());

        // Voice press inside the settle, capture Ready: admitted.
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        rig.mox()->setMox(true);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rejected.count(), 0);

        QTRY_VERIFY_WITH_TIMEOUT(twoTone->isActive(), 2000);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rejected.count(), 0);

        QSignalSpy aboutToEnd(rig.mox(), &MoxController::txAboutToEnd);
        killProcess(rig.engine()->captureHelperProcessIdForTest());
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Failed, 5000);
        QTest::qWait(100);
        QVERIFY(rig.mox()->isMox());
        QVERIFY(twoTone->isActive());
        QCOMPARE(aboutToEnd.count(), 0);

        twoTone->setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QTest::qWait(300);
        twoTone->setTxChannel(nullptr);
    }

    // The owner's status copy lags the capture thread by one queued call.
    // A press right after a retry, a configuration change or a helper
    // failure must not see the old Ready: it is refused, and a press once
    // capture is Ready again keys.
    void pressRightAfterRestartIsRefused_data()
    {
        QTest::addColumn<QString>("restart");
        QTest::newRow("retry") << "retry";
        QTest::newRow("configure") << "configure";
        QTest::newRow("helper-failure") << "helper-failure";
    }
    void pressRightAfterRestartIsRefused()
    {
        QFETCH(QString, restart);
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        if (restart == QLatin1String("retry")) {
            rig.engine()->retryCapture();
        } else if (restart == QLatin1String("configure")) {
            AudioDeviceConfig cfg = rig.engine()->txInputConfig();
            cfg.bufferSamples = cfg.bufferSamples == 1024 ? 2048 : 1024;
            rig.engine()->setTxInputConfig(cfg);
        } else {
            // Wait, without running this thread's event loop, until the
            // capture thread has retired the reader; the owner's status
            // copy still says Ready.
            QVERIFY(rig.engine()->isCaptureReaderOpen());
            killProcess(rig.engine()->captureHelperProcessIdForTest());
            QElapsedTimer waited;
            waited.start();
            while (rig.engine()->isCaptureReaderOpen() && waited.elapsed() < 5000) {
                QThread::msleep(5);
            }
            QVERIFY(!rig.engine()->isCaptureReaderOpen());
            QCOMPARE(rig.engine()->captureStatus().state, State::Ready);
        }
        // No event-loop turn between the restart and the press.
        rig.mox()->setMox(true);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), kRefusal);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 0);

        if (restart == QLatin1String("helper-failure")) {
            QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Failed, 5000);
            rig.engine()->retryCapture();
        }
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine()->captureStatus().state, State::Ready, 8000);
        rig.mox()->setMox(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rig.conn.moxOnCalls, 1);
    }

    // R-R3-36, never queued, for a source that stays held: a mic PTT or VOX
    // refused because the microphone is not ready is not tried again on
    // later status frames. Capture reaching Ready does not key it; the
    // operator lets go and presses again. (A held mic or VOX is re-reported
    // on every status frame, so each frame below is a PollPTT pass.)
    void heldRefusedPressIsNotQueued_data()
    {
        QTest::addColumn<int>("ptt");
        QTest::newRow("radio-mic-ptt") << int(Ptt::RadioMicPtt);
        QTest::newRow("vox")           << int(Ptt::Vox);
    }
    void heldRefusedPressIsNotQueued()
    {
        QFETCH(int, ptt);
        const Ptt source = static_cast<Ptt>(ptt);
        Rig rig(QStringLiteral("ready"));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        const auto heldFrame = [&rig, source]() {
            if (source == Ptt::RadioMicPtt) {
                rig.mox()->onMicPttFromRadio(true);
            } else {
                rig.mox()->onMicPttFromRadio(false);   // a pass, VOX still active
            }
        };

        pressPtt(rig.mox(), source);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(!rig.mox()->isMox());

        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        for (int i = 0; i < 5; ++i) {
            heldFrame();
            QTest::qWait(10);
        }
        QVERIFY2(!rig.mox()->isMox(), "a held press refused for the microphone keyed later");
        QCOMPARE(rig.conn.moxOnCalls, 0);

        // Let go, press again: that press keys.
        if (source == Ptt::RadioMicPtt) {
            rig.mox()->onMicPttFromRadio(false);
        } else {
            rig.mox()->onVoxActive(false);
        }
        pressPtt(rig.mox(), source);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 1);
        QCOMPARE(rejected.count(), 1);

        if (source == Ptt::RadioMicPtt) {
            rig.mox()->onMicPttFromRadio(false);
        } else {
            rig.mox()->onVoxActive(false);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
    }

    // The same through the TCI fallback (M6): the TCI key ends because the
    // held mic it would fall back to has no ready microphone, and that mic
    // is not keyed later when the microphone becomes ready.
    void tciFallbackRefusalIsNotQueued()
    {
        Rig rig(QStringLiteral("ready"));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        rig.tx.setTciAudioActive(true);
        rig.mox()->onTciPtt(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        rig.mox()->onMicPttFromRadio(true);
        QCOMPARE(rig.mox()->pttMode(), PttMode::Tci);

        // The app lets go; its TCI audio stops with it.
        rig.tx.setTciAudioActive(false);
        rig.mox()->onTciPtt(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), kRefusal);

        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        for (int i = 0; i < 5; ++i) {
            rig.mox()->onMicPttFromRadio(true);
            QTest::qWait(10);
        }
        QVERIFY2(!rig.mox()->isMox(), "the held mic keyed once the microphone was ready");
        QCOMPARE(rig.conn.moxOnCalls, 1);   // the TCI key only

        rig.mox()->onMicPttFromRadio(false);
        rig.mox()->onMicPttFromRadio(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 2);
        rig.mox()->onMicPttFromRadio(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
    }

    // R-R3-49 with R-R3-36 (Task 7 follow-up, item 5): an app's
    // trx:0,true,tci; that keys nothing gives the TX audio back. Otherwise
    // the lock stays with the app and a later MOX-button or mic key
    // transmits the TCI buffer (the TX worker reads TCI audio while an app
    // holds it) and skips the microphone-ready check. MainWindow's wiring
    // of the lock to the TX input is reproduced here.
    void refusedTrxReleasesTciAudio_data()
    {
        QTest::addColumn<int>("ptt");
        QTest::newRow("mox button")    << int(Ptt::Mox);
        QTest::newRow("radio-mic-ptt") << int(Ptt::RadioMicPtt);
    }
    void refusedTrxReleasesTciAudio()
    {
        QFETCH(int, ptt);
        const Ptt source = static_cast<Ptt>(ptt);
        Rig rig(QStringLiteral("ready"));   // capture Closed: no demand yet
        TciServer server(rig.model.get());
        QObject::connect(&server, &TciServer::txAudioActiveClientChanged, &server,
                         [&rig](QWebSocket* owner) { rig.tx.setTciAudioActive(owner != nullptr); });
        QVERIFY(server.start(0));
        QWebSocket app;
        QSignalSpy connected(&app, &QWebSocket::connected);
        QSignalSpy chrono(&app, &QWebSocket::binaryMessageReceived);
        app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connected.wait(2000));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        const auto press = [&rig, source]() {
            if (source == Ptt::Mox) {
                rig.model->setMoxFromButton(true);
            } else {
                rig.mox()->onMicPttFromRadio(true);
            }
        };
        const auto release = [&rig, source]() {
            if (source == Ptt::Mox) {
                rig.model->setMoxFromButton(false);
            } else {
                rig.mox()->onMicPttFromRadio(false);
            }
        };

        // The band plan refuses the app's key: nothing is keyed.
        rig.slice->setFrequency(14'400'000.0);
        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 2000);
        QTest::qWait(100);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(server.activeTxClientCount(), 0);
        QVERIFY2(!rig.tx.isTciAudioActive(), "the TX input stayed on the app's audio");
        const int chronoFrames = int(chrono.count());
        QTest::qWait(150);
        QCOMPARE(int(chrono.count()), chronoFrames);   // TX_CHRONO stopped

        // Back in band, the operator keys: the microphone check applies.
        rig.slice->setFrequency(14'200'000.0);
        press();
        QTest::qWait(50);
        QVERIFY2(!rig.mox()->isMox(), "keyed on the app's audio past the microphone check");
        QCOMPARE(rejected.count(), 2);
        QCOMPARE(rejected.at(1).at(0).toString(), kRefusal);
        release();

        // With the microphone ready a new press keys, on the microphone.
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        press();
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QVERIFY(!rig.tx.isTciAudioActive());
        release();
        QTRY_VERIFY_WITH_TIMEOUT(!rig.mox()->isMox(), 2000);
        app.close();
        server.stop();
    }

    // PC-mic keying with capture Ready is admitted, and unkey is never
    // refused.
    void readyCaptureKeysAndUnkeys()
    {
        Rig rig(QStringLiteral("ready"));
        QVERIFY(reachCaptureState(rig, QStringLiteral("ready")));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        rig.mox()->setMox(true);
        QTest::qWait(50);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOnCalls, 1);
        QCOMPARE(rig.conn.trxRelayCalls, 1);

        rig.mox()->setMox(false);
        QTest::qWait(50);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rig.conn.moxOffCalls, 1);
        QCOMPARE(rejected.count(), 0);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstCaptureAdmission test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_capture_admission.moc"
