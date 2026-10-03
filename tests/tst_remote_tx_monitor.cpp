// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_remote_tx_monitor.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor
// (MON) goes to the device that holds transmit.
//
// The mixer (AudioEngine with fake devices and recording taps, no sound
// device opened): an owner mix given a monitor route carries MON at the MON
// level in its speakers sum or its headphones sum, another owner carries
// none, none and MON off carry nothing, the Core's own outputs leave it out
// while a remote device holds transmit (JJ's MON ruling of 2026-09-26) and
// carry it as before otherwise, and the monitor ends with the block after
// the last one the transmitter's siphon delivered.
//
// The session (a real Core and a real remote window over the loopback
// control link and a real media connection): the capability, the start's
// declaration, monitor-audio and its one monitor-audio-context, requests of
// the wrong shape ignored, a window whose Core does not offer it keeping
// today's wire, and the Core's rule: only while this device holds transmit
// and MON is on, and the Core's own outputs quiet while it does.
//
// No radio is keyed and nothing reaches a device: monitor blocks are handed
// to txMonitorBlockReady as TxChannel's siphon would while transmitting,
// and holding transmit is a take, which never keys (ruling 8.6).
//
// Modification history (NereusSDR):
//   2026-09-27 - Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code (R-IOS-13, R-R3-49).
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/MoxController.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/RemoteAudioContext.h"
#include "OperatorWording.h"
#include "gui/RemoteMediaController.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/FakeAudioBus.h"
#include "fakes/RemoteAudioSessionHarness.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>

#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;
using NereusSDR::Test::RemoteAudioSessionHarness;

namespace {

constexpr int kFrames = 64;
constexpr float kMonIn = 0.4f;      // what the siphon hands the monitor
constexpr float kMonLevel = 0.5f;   // MON's level (Thetis's 0.5 default)

std::vector<float> stereo(float value)
{
    return std::vector<float>(static_cast<size_t>(kFrames) * 2, value);
}

class RecordingMixTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        last.assign(samples, samples + frames * 2);
        ++calls;
    }
    float peak() const
    {
        float p = 0.0f;
        for (float v : last) {
            p = std::max(p, std::fabs(v));
        }
        return p;
    }
    std::vector<float> last;
    int calls = 0;
};

double decibels(double measured, double expected)
{
    return 20.0 * std::log10(measured / expected);
}

// One receiver feeding silence paces the mix; MON on at kMonLevel, no ramps,
// so every drained block carries its full level at once.
struct MixHarness {
    std::unique_ptr<RadioModel> radio = std::make_unique<RadioModel>();
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    FakeAudioBus* headphones = nullptr;
    int sliceA = -1;
    int sliceB = -1;

    MixHarness()
    {
        engine = radio->audioEngine();
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        auto spk = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        spk->open(format);
        speakers = spk.get();
        engine->setSpeakersBusForTest(std::move(spk));
        auto hp = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        hp->open(format);
        headphones = hp.get();
        engine->setHeadphonesBusForTest(std::move(hp));
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        engine->setTxMonitorVolume(kMonLevel);
        engine->setTxMonitorEnabled(true);
        sliceA = radio->addSlice();
        sliceB = radio->addSlice();
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
    }

    // One audio period: the monitor's block (while "keyed"), then both
    // receivers' silence, whose arrival drains the mix.
    void period(bool keyed)
    {
        const std::vector<float> mono(static_cast<size_t>(kFrames), kMonIn);
        const std::vector<float> silence = stereo(0.0f);
        if (keyed) {
            engine->txMonitorBlockReady(mono.data(), kFrames);
        }
        engine->rxBlockReady(sliceA, silence.data(), kFrames);
        engine->rxBlockReady(sliceB, silence.data(), kFrames);
    }

    static float lastPeak(const FakeAudioBus* bus)
    {
        const QByteArray bytes = bus->buffer();
        const auto* f = reinterpret_cast<const float*>(bytes.constData());
        const int n = static_cast<int>(bytes.size() / static_cast<qsizetype>(sizeof(float)));
        float p = 0.0f;
        for (int i = std::max(0, n - kFrames * 2); i < n; ++i) {
            p = std::max(p, std::fabs(f[i]));
        }
        return p;
    }

    quint32 bit(int slice) const { return 1u << slice; }
};

QList<QJsonObject> opsOf(const QSignalSpy& spy, const QString& op)
{
    QList<QJsonObject> found;
    for (const auto& call : spy) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == op) {
            found << control;
        }
    }
    return found;
}

} // namespace

class TstRemoteTxMonitor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // ── The mixer ────────────────────────────────────────────────────────

    // The holder's main stream (its speakers sum, taken speakers-only as a
    // device with a headphones stream takes it) carries MON at MON's level.
    void holdersMainStreamCarriesItAtTheMonitorLevel()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        QVERIFY(holder >= 0);
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap main;
        RecordingMixTap phones;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &main, /*speakersOnly=*/true));
        QVERIFY(h.engine->setOwnerHeadphonesMixAudioTap(holder, &phones));
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        QCOMPARE(h.engine->ownerMixMonitor(holder), MasterMixer::OwnerMonitor::Speakers);
        for (int p = 0; p < 4; ++p) {
            h.period(true);
        }
        const double expected = double(kMonIn) * double(kMonLevel);
        QVERIFY(main.calls > 0);
        QVERIFY2(std::fabs(decibels(main.peak(), expected)) < 1.0,
                 qPrintable(QStringLiteral("main %1 against %2").arg(main.peak()).arg(expected)));
        QCOMPARE(main.last.front(), float(expected));
        QCOMPARE(phones.peak(), 0.0f);
        h.engine->releaseOwnerMix(holder);
    }

    // Route headphones: MON moves to the holder's headphones stream.
    void headphonesRouteMovesItToTheHeadphonesStream()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap main;
        RecordingMixTap phones;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &main, /*speakersOnly=*/true));
        QVERIFY(h.engine->setOwnerHeadphonesMixAudioTap(holder, &phones));
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Headphones);
        for (int p = 0; p < 4; ++p) {
            h.period(true);
        }
        QCOMPARE(phones.peak(), kMonIn * kMonLevel);
        QCOMPARE(main.peak(), 0.0f);
        h.engine->releaseOwnerMix(holder);
    }

    // A device whose start did not declare the headphones mix takes its
    // whole program on its main stream: MON routed to its headphones sum
    // still reaches it there.
    void aProgramStreamCarriesItFromEitherSum()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap program;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &program, /*speakersOnly=*/false));
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Headphones);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(program.peak(), kMonIn * kMonLevel);
        h.engine->releaseOwnerMix(holder);
    }

    // Route none, and MON off, carry nothing.
    void noneAndMonOffCarryNothing()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap main;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &main));
        QCOMPARE(h.engine->ownerMixMonitor(holder), MasterMixer::OwnerMonitor::None);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QVERIFY(main.calls > 0);
        QCOMPARE(main.peak(), 0.0f);

        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        h.engine->setTxMonitorEnabled(false);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(main.peak(), 0.0f);
        h.engine->releaseOwnerMix(holder);
        // A released mix starts over with no monitor.
        const int again = h.engine->acquireOwnerMix();
        QCOMPARE(h.engine->ownerMixMonitor(again), MasterMixer::OwnerMonitor::None);
        h.engine->releaseOwnerMix(again);
    }

    // A second device on the Core, not holding transmit, hears no MON;
    // nor does a holder that owns no slice go without it.
    void anotherDeviceHearsNone()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        const int other = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, 0u);   // no slice of its own
        h.engine->setOwnerMixSliceMask(other, h.bit(h.sliceB));
        RecordingMixTap holderMain;
        RecordingMixTap otherMain;
        RecordingMixTap otherPhones;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &holderMain));
        QVERIFY(h.engine->setOwnerMixAudioTap(other, &otherMain));
        QVERIFY(h.engine->setOwnerHeadphonesMixAudioTap(other, &otherPhones));
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(holderMain.peak(), kMonIn * kMonLevel);
        QVERIFY(otherMain.calls > 0);
        QCOMPARE(otherMain.peak(), 0.0f);
        QCOMPARE(otherPhones.peak(), 0.0f);
        h.engine->releaseOwnerMix(holder);
        h.engine->releaseOwnerMix(other);
    }

    // JJ's MON ruling: while a remote device holds transmit the Core's own
    // speakers, headphones and master taps leave MON out and the holder
    // still hears it; with the station holding, the local output carries it
    // exactly as before (the default).
    void theCoresOwnOutputsLeaveItOutWhileARemoteDeviceHolds()
    {
        MixHarness h;
        QVERIFY(h.engine->txMonitorLocal());
        RecordingMixTap master;
        RecordingMixTap localPhones;
        h.engine->setMasterMixAudioTap(&master);
        h.engine->setHeadphonesMixAudioTap(&localPhones);
        const int holder = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap holderMain;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &holderMain));

        // The station holds: local as before, the device's mix has none.
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(MixHarness::lastPeak(h.speakers), kMonIn * kMonLevel);
        QCOMPARE(master.peak(), kMonIn * kMonLevel);
        QCOMPARE(holderMain.peak(), 0.0f);

        // A remote device holds.
        h.engine->setTxMonitorLocal(false);
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(MixHarness::lastPeak(h.speakers), 0.0f);
        QCOMPARE(MixHarness::lastPeak(h.headphones), 0.0f);
        QCOMPARE(master.peak(), 0.0f);
        QCOMPARE(localPhones.peak(), 0.0f);
        QCOMPARE(holderMain.peak(), kMonIn * kMonLevel);

        // The local choice of the headphones changes nothing while it does.
        h.engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(MixHarness::lastPeak(h.headphones), 0.0f);
        QCOMPARE(localPhones.peak(), 0.0f);
        QCOMPARE(holderMain.peak(), kMonIn * kMonLevel);

        // Back to the station: MON plays here again, on the headphones now.
        h.engine->setTxMonitorLocal(true);
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::None);
        for (int p = 0; p < 3; ++p) {
            h.period(true);
        }
        QCOMPARE(MixHarness::lastPeak(h.headphones), kMonIn * kMonLevel);
        QCOMPARE(localPhones.peak(), kMonIn * kMonLevel);
        QCOMPARE(holderMain.peak(), 0.0f);
        h.engine->setTxMonitorOutput(TxMonitorOutput::Speakers);
        h.engine->clearMasterMixAudioTap(&master);
        h.engine->clearHeadphonesMixAudioTap(&localPhones);
        h.engine->releaseOwnerMix(holder);
    }

    // Un-keying: the siphon stops, and the very next block the holder is
    // sent carries no MON.
    void unkeyingEndsItWithinOneBlock()
    {
        MixHarness h;
        const int holder = h.engine->acquireOwnerMix();
        h.engine->setOwnerMixSliceMask(holder, h.bit(h.sliceA));
        RecordingMixTap main;
        QVERIFY(h.engine->setOwnerMixAudioTap(holder, &main));
        h.engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        for (int p = 0; p < 4; ++p) {
            h.period(true);
        }
        QCOMPARE(main.peak(), kMonIn * kMonLevel);
        const int before = main.calls;
        h.period(false);
        QCOMPARE(main.calls, before + 1);
        QCOMPARE(main.peak(), 0.0f);
        h.engine->releaseOwnerMix(holder);
    }

    // The transmitting slice is the station's only one: its receive audio
    // is gated while keyed, and its own blocks still drain the mix, so MON
    // is heard (by the Core's own output with the station holding, and by
    // the holder's stream with a remote device holding). Before this task
    // nothing drained and MON was silent with one slice.
    void withTheOnlySliceKeyedMonStillPlays()
    {
        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setTxMonitorVolume(kMonLevel);
        engine->setTxMonitorEnabled(true);
        const int only = radio->addSlice();
        engine->setSliceStreaming(only, true);
        QVERIFY(radio->txBoundSlice() != nullptr);
        QCOMPARE(radio->txBoundSlice()->sliceIndex(), only);
        RecordingMixTap master;
        engine->setMasterMixAudioTap(&master);
        const int holder = engine->acquireOwnerMix();
        engine->setOwnerMixSliceMask(holder, 1u << only);
        RecordingMixTap holderMain;
        QVERIFY(engine->setOwnerMixAudioTap(holder, &holderMain));
        const std::vector<float> mono(static_cast<size_t>(kFrames), kMonIn);
        const std::vector<float> received = stereo(0.3f);
        const auto keyedPeriods = [&](int periods) {
            for (int p = 0; p < periods; ++p) {
                engine->txMonitorBlockReady(mono.data(), kFrames);
                engine->rxBlockReady(only, received.data(), kFrames);
            }
        };

        engine->setMoxStateForTest(true);
        keyedPeriods(3);
        QCOMPARE(master.calls, 3);
        // MON alone: the gated receiver adds nothing.
        QCOMPARE(master.peak(), kMonIn * kMonLevel);
        QCOMPARE(holderMain.peak(), 0.0f);

        engine->setTxMonitorLocal(false);
        engine->setOwnerMixMonitor(holder, MasterMixer::OwnerMonitor::Speakers);
        keyedPeriods(3);
        QCOMPARE(master.peak(), 0.0f);
        QCOMPARE(holderMain.peak(), kMonIn * kMonLevel);
        engine->setMoxStateForTest(false);
        engine->setTxMonitorLocal(true);
        engine->clearMasterMixAudioTap(&master);
        engine->releaseOwnerMix(holder);
    }

    // ── The session ──────────────────────────────────────────────────────

    // The capability, the start's declaration, monitor-audio with the
    // window's route and its one answer; then the Core's rule.
    void theHolderGetsItAndOnlyWhileItHoldsWithMonOn()
    {
        RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.connectSession();
        QCOMPARE(h.client.capabilities().txMonitorAudioVersion, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!opsOf(coreControls, QStringLiteral("monitor-audio")).isEmpty(),
                                 20000);
        QVERIFY(remoteMedia.txMonitorAudioNegotiated());
        const QList<QJsonObject> starts = opsOf(coreControls, QStringLiteral("start"));
        QCOMPARE(int(starts.size()), 1);
        QCOMPARE(starts.constFirst().value(QStringLiteral("txMonitorAudioVersion")).toInteger(),
                 qint64{1});
        const QJsonObject request = opsOf(coreControls, QStringLiteral("monitor-audio")).constLast();
        QCOMPARE(request.keys(), (QStringList{QStringLiteral("connectionId"), QStringLiteral("op"),
                                              QStringLiteral("revision"), QStringLiteral("route")}));
        QCOMPARE(request.value(QStringLiteral("route")).toString(), QStringLiteral("speakers"));
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedMonitorContext().has_value(), 5000);
        QCOMPARE(remoteMedia.acceptedMonitorContext()->route, TxMonitorRoute::Speakers);
        QCOMPARE(remoteMedia.acceptedMonitorContext()->revision,
                 quint32(request.value(QStringLiteral("revision")).toInteger()));
        QCOMPARE(int(opsOf(controls, QStringLiteral("monitor-audio-context")).size()), 1);
        QCOMPARE(opsOf(controls, QStringLiteral("monitor-audio-context")).constFirst().keys(),
                 (QStringList{QStringLiteral("connectionId"), QStringLiteral("op"),
                              QStringLiteral("revision"), QStringLiteral("route")}));

        // Asked for, but nobody holds transmit and MON is off: nowhere.
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        QVERIFY(daemonMedia.ownerMixSlot() >= 0);
        AudioEngine* const core = h.stationAudio;
        const int slot = daemonMedia.ownerMixSlot();
        QCOMPARE(core->ownerMixMonitor(slot), MasterMixer::OwnerMonitor::None);

        // This device takes transmit (a take never keys): still MON off.
        const QByteArray device = h.server.mediaSessionDevice(h.server.mediaSessionEpoch());
        QVERIFY(!device.isEmpty());
        TransmitHolder* const holder = h.server.transmitHolder();
        QVERIFY(holder != nullptr);
        TransmitHolder::Holder self;
        self.deviceId = device;
        holder->transferTo(self, QStringLiteral("test"));
        QVERIFY(holder->isHeldBy(device));
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        // A remote device holds: the Core's own outputs leave MON out.
        QVERIFY(!core->txMonitorLocal());

        // MON on: the holder's main stream.
        h.station.transmitModel().setMonEnabled(true);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers);
        QCOMPARE(core->ownerMixMonitor(slot), MasterMixer::OwnerMonitor::Speakers);

        // The window's headphones: MON moves there (this window declared
        // the headphones mix), and back.
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Headphones);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.txMonitorRoute(), TxMonitorRoute::Headphones, 5000);
        QCOMPARE(core->ownerMixMonitor(slot), MasterMixer::OwnerMonitor::Headphones);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.acceptedMonitorContext()->route,
                                  TxMonitorRoute::Headphones, 5000);
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::None);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.txMonitorRoute(), TxMonitorRoute::None, 5000);
        QCOMPARE(core->ownerMixMonitor(slot), MasterMixer::OwnerMonitor::None);
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Speakers);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers, 5000);

        // MON off: nowhere; on again: back.
        h.station.transmitModel().setMonEnabled(false);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        h.station.transmitModel().setMonEnabled(true);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers);

        // Another device takes transmit: this one hears none, and the
        // Core's own outputs stay quiet for that one.
        TransmitHolder::Holder other;
        other.deviceId = QByteArrayLiteral("another-device");
        holder->transferTo(other, QStringLiteral("test"));
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        QCOMPARE(core->ownerMixMonitor(slot), MasterMixer::OwnerMonitor::None);
        QVERIFY(!core->txMonitorLocal());

        // The station takes it: the Core's own outputs carry MON again.
        TransmitHolder::Holder station;
        station.deviceId = KeyerIdentity::kStationDeviceId;
        holder->transferTo(station, QStringLiteral("test"));
        QVERIFY(core->txMonitorLocal());
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        holder->transferTo(std::nullopt, QStringLiteral("test"));
        QVERIFY(core->txMonitorLocal());
        h.station.transmitModel().setMonEnabled(false);
    }

    // monitor-audio of the wrong shape, a stale or repeated revision, a
    // route the Core does not know or another connection's id: ignored, no
    // answer and nothing changes. Then a right one is answered once.
    void requestsOfTheWrongShapeAreIgnored()
    {
        RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedMonitorContext().has_value(), 20000);
        const QJsonObject sent = opsOf(coreControls, QStringLiteral("monitor-audio")).constLast();
        const QString connectionId = sent.value(QStringLiteral("connectionId")).toString();
        const qint64 revision = sent.value(QStringLiteral("revision")).toInteger();
        const int answers = int(opsOf(controls, QStringLiteral("monitor-audio-context")).size());
        QCOMPARE(answers, 1);

        const auto request = [&](qint64 rev, const QJsonValue& route) {
            return QJsonObject{{QStringLiteral("op"), QStringLiteral("monitor-audio")},
                               {QStringLiteral("connectionId"), connectionId},
                               {QStringLiteral("revision"), double(rev)},
                               {QStringLiteral("route"), route}};
        };
        QList<QJsonObject> wrong;
        wrong << request(revision + 1, QStringLiteral("phones"));
        wrong << request(revision + 1, QJsonValue(1));
        wrong << request(0, QStringLiteral("headphones"));
        wrong << request(revision, QStringLiteral("headphones"));      // not newer
        wrong << request(revision - 1 > 0 ? revision - 1 : 0, QStringLiteral("headphones"));
        QJsonObject extra = request(revision + 1, QStringLiteral("headphones"));
        extra.insert(QStringLiteral("enabled"), true);
        wrong << extra;
        QJsonObject missing = request(revision + 1, QStringLiteral("headphones"));
        missing.remove(QStringLiteral("route"));
        wrong << missing;
        QJsonObject fraction = request(revision + 1, QStringLiteral("headphones"));
        fraction.insert(QStringLiteral("revision"), double(revision) + 1.5);
        wrong << fraction;
        QJsonObject foreign = request(revision + 1, QStringLiteral("headphones"));
        foreign.insert(QStringLiteral("connectionId"),
                       QStringLiteral("00000000-0000-4000-8000-000000000000"));
        wrong << foreign;
        const int received = int(opsOf(coreControls, QStringLiteral("monitor-audio")).size());
        for (const QJsonObject& payload : wrong) {
            QVERIFY(h.client.sendMediaControl(payload, h.client.sessionEpoch()));
        }
        QTRY_COMPARE_WITH_TIMEOUT(int(opsOf(coreControls, QStringLiteral("monitor-audio")).size()),
                                  received + int(wrong.size()), 5000);
        QCoreApplication::processEvents();
        QCOMPARE(int(opsOf(controls, QStringLiteral("monitor-audio-context")).size()), answers);
        QCOMPARE(remoteMedia.acceptedMonitorContext()->route, TxMonitorRoute::Speakers);

        // A right one, answered once with the route as applied.
        QVERIFY(h.client.sendMediaControl(request(revision + 7, QStringLiteral("headphones")),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE_WITH_TIMEOUT(int(opsOf(controls, QStringLiteral("monitor-audio-context")).size()),
                                  answers + 1, 5000);
        const QJsonObject answer = opsOf(controls, QStringLiteral("monitor-audio-context")).constLast();
        QCOMPARE(answer.value(QStringLiteral("revision")).toInteger(), revision + 7);
        QCOMPARE(answer.value(QStringLiteral("route")).toString(), QStringLiteral("headphones"));
        QCOMPARE(answer.value(QStringLiteral("connectionId")).toString(), connectionId);
    }

    // A window whose start did not declare the headphones mix hears MON
    // routed to the headphones on its main stream, and is told so.
    void headphonesWithoutTheHeadphonesMixIsTheMainStream()
    {
        RemoteAudioSessionHarness h;
        h.hideHeadphonesMix = true;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Headphones);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedMonitorContext().has_value(), 20000);
        QCOMPARE(remoteMedia.acceptedMonitorContext()->route, TxMonitorRoute::Speakers);
        TransmitHolder::Holder self;
        self.deviceId = h.server.mediaSessionDevice(h.server.mediaSessionEpoch());
        h.server.transmitHolder()->transferTo(self, QStringLiteral("test"));
        h.station.transmitModel().setMonEnabled(true);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers);
        h.server.transmitHolder()->transferTo(std::nullopt, QStringLiteral("test"));
        h.station.transmitModel().setMonEnabled(false);
    }

    // A Core that does not offer it: the start carries today's keys, no
    // monitor-audio goes out, and the window says it is not negotiated.
    // A Core that offers it to a window that did not declare it ignores a
    // monitor-audio from that window.
    void anOlderPeerKeepsTodaysWire()
    {
        RemoteAudioSessionHarness h;
        h.hideTxMonitorAudio = true;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.connectSession();
        // Media is ready once the window asks for its audio.
        QTRY_VERIFY_WITH_TIMEOUT(!opsOf(coreControls, QStringLiteral("audio")).isEmpty(), 20000);
        QVERIFY(!remoteMedia.txMonitorAudioNegotiated());
        const QList<QJsonObject> starts = opsOf(coreControls, QStringLiteral("start"));
        QCOMPARE(int(starts.size()), 1);
        QVERIFY(!starts.constFirst().contains(QStringLiteral("txMonitorAudioVersion")));
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Headphones);
        QCoreApplication::processEvents();
        QVERIFY(opsOf(coreControls, QStringLiteral("monitor-audio")).isEmpty());

        // Undeclared at start, so a request is ignored by the Core.
        const QString connectionId = starts.constFirst()
                                         .value(QStringLiteral("connectionId")).toString();
        QVERIFY(h.client.sendMediaControl(
            QJsonObject{{QStringLiteral("op"), QStringLiteral("monitor-audio")},
                        {QStringLiteral("connectionId"), connectionId},
                        {QStringLiteral("revision"), 1.0},
                        {QStringLiteral("route"), QStringLiteral("speakers")}},
            h.client.sessionEpoch()));
        QTRY_COMPARE_WITH_TIMEOUT(int(opsOf(coreControls, QStringLiteral("monitor-audio")).size()), 1,
                                  5000);
        QCoreApplication::processEvents();
        QVERIFY(opsOf(controls, QStringLiteral("monitor-audio-context")).isEmpty());
        TransmitHolder::Holder self;
        self.deviceId = h.server.mediaSessionDevice(h.server.mediaSessionEpoch());
        h.server.transmitHolder()->transferTo(self, QStringLiteral("test"));
        h.station.transmitModel().setMonEnabled(true);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::None);
        h.server.transmitHolder()->transferTo(std::nullopt, QStringLiteral("test"));
        h.station.transmitModel().setMonEnabled(false);
    }

    // A remote window on a Core below version 1: the MON output pair is
    // shown disabled with the reason in plain words (MON itself stays on
    // its own gate); the transmit settings' reason wins while they are
    // unavailable too; a Core at 1 enables the pair again.
    void theMonOutputPairSaysWhyOnAnOlderCore()
    {
        RadioModel radio;
        TxApplet applet(&radio);
        QPushButton* const speakers =
            applet.findChild<QPushButton*>(QStringLiteral("TxMonitorSpeakersButton"));
        QPushButton* const phones =
            applet.findChild<QPushButton*>(QStringLiteral("TxMonitorHeadphonesButton"));
        QVERIFY(speakers != nullptr && phones != nullptr);
        const QString reason = TxApplet::monitorOutputUnavailableReason();
        QCOMPARE(reason, QStringLiteral(
            "This Core does not send the transmit monitor. Updating the Core may help."));
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(!reason.contains(QChar(0x2014)));

        applet.setTransmitChainSettingsPermitted(true);
        applet.setMonitorOutputPermitted(false, reason);
        QVERIFY(!speakers->isEnabled());
        QVERIFY(!phones->isEnabled());
        QCOMPARE(speakers->toolTip(), reason);
        QCOMPARE(phones->toolTip(), reason);

        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        applet.setTransmitChainSettingsPermitted(false, onAir);
        QCOMPARE(speakers->toolTip(), onAir);
        applet.setTransmitChainSettingsPermitted(true);
        QCOMPARE(speakers->toolTip(), reason);

        applet.setMonitorOutputPermitted(true);
        QVERIFY(speakers->isEnabled());
        QVERIFY(phones->isEnabled());
        QVERIFY(speakers->toolTip() != reason);
    }

    // The wire codec: each route round-trips; anything else is refused.
    void theCodecTakesExactlyTheFourKeys()
    {
        for (const TxMonitorRoute route :
             {TxMonitorRoute::None, TxMonitorRoute::Speakers, TxMonitorRoute::Headphones}) {
            MonitorAudioMessage message;
            message.connectionId = QStringLiteral("00000000-0000-4000-8000-000000000001");
            message.revision = 42;
            message.route = route;
            const std::optional<MonitorAudioMessage> back =
                decodeMonitorAudioContext(encodeMonitorAudioContext(message));
            QVERIFY(back.has_value());
            QCOMPARE(back->route, route);
            QCOMPARE(back->revision, 42u);
            QVERIFY(decodeMonitorAudioRequest(encodeMonitorAudioRequest(message)).has_value());
            // Each op decodes only as itself.
            QVERIFY(!decodeMonitorAudioRequest(encodeMonitorAudioContext(message)).has_value());
        }
        QVERIFY(!txMonitorRouteFromWire(QStringLiteral("Speakers")).has_value());
        QVERIFY(!txMonitorRouteFromWire(QJsonValue()).has_value());
    }
};

QTEST_MAIN(TstRemoteTxMonitor)
#include "tst_remote_tx_monitor.moc"
