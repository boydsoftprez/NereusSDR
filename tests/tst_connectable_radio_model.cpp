// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here; this exercises NereusSDR's own test-only WdspEngine seam
// (remote-daemon R2 Task 2).
// =================================================================
// tests/tst_connectable_radio_model.cpp  (NereusSDR)
// =================================================================
//
// Remote-daemon R2 Task 2 -- a connectable RadioModel for tests.
//
// RadioModel::connectToRadio() has never been called by any test in this
// suite (see tst_daemon_app.cpp's header comment): on a cold config
// directory -- which tests/TestSandboxInit.cpp forces on every run -- it
// blocks the calling thread inside a QEventLoop until WdspEngine finishes
// generating FFTW wisdom, which takes minutes. Tasks 3, 12 and 20 all need
// a genuinely connected RadioModel (task 12 specifically needs a live
// RxChannel), so this file builds and proves the harness that makes that
// possible inside QtTest's per-function timeout (TIMEOUT 120, set in
// tests/CMakeLists.txt).
//
// Spike (must run before the harness is trusted): confirms that skipping
// WDSPwisdom() does not just move the FFTW planning cost into WDSP's own
// channel-open path and blow the timeout anyway. See
// synchronousInitTimingSpike() below and task-2-report.md for the
// measured number.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-03 -- New test file for remote-daemon R2 Task 2. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 -- iPhone app Part A fix wave (R-IOS-01): PureSignal's
//                 readiness follows the receive-only policy at once.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- R-R3-39: wait for the receive lane before reading
//                 state it owns (NNR tuning, NR slot). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 -- A connect keeps the saved FFT plans in the folder the
//                 process was given (DspCacheLocation). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-17 -- Teardown stops the TX worker before the audio engine
//                 drops the input it reads (main CI crash at 4066a86b6).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-24 -- The AGC-T and RF gain sliders save the value their lane
//                 readback gives. Before, an AGC-T move's RF gain came back
//                 stale on restart (the AGC did not move), and an RF gain
//                 move's band threshold came back stale. The AUTO tick
//                 still schedules no save.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <thread>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/DspCacheLocation.h"
#include "core/PureSignal.h"
#include "core/RxChannel.h"
#include "core/dsp/DspAssetService.h"
#include "core/WdspEngine.h"
#include "core/daemon/DaemonAgcSource.h"
#include "core/audio/TxMicSource.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/FakeAudioBus.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

namespace {

// What the TX worker did with a HeldReadBus. Shared with the test, so it
// outlives the bus.
struct HeldReadState {
    // Set by the test: the next read stays inside the bus until this mic
    // source stops, which is the first thing TxWorkerThread::stopPump does.
    std::atomic<const TxMicSource*> holdUntilStopped{nullptr};
    std::atomic<bool> held{false};
    std::atomic<int> readsInside{0};
    std::atomic<bool> destroyed{false};
    std::atomic<bool> destroyedDuringRead{false};
};

// A TX input (PC microphone or VAX TX) for the worker to read. Its
// destructor records whether the worker was still inside a read, then
// waits for that read to leave, so a wrong teardown order fails the test
// instead of crashing it.
class HeldReadBus : public FakeAudioBus {
public:
    explicit HeldReadBus(std::shared_ptr<HeldReadState> state)
        : FakeAudioBus(QStringLiteral("Held TX input")), m_state(std::move(state)) {}

    ~HeldReadBus() override
    {
        m_state->destroyed.store(true);
        if (m_state->readsInside.load() > 0) {
            m_state->destroyedDuringRead.store(true);
        }
        while (m_state->readsInside.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    qint64 pull(char* data, qint64 maxBytes) override
    {
        Q_UNUSED(data);
        Q_UNUSED(maxBytes);
        read();
        return 0;
    }

    qint64 pullKeepingBacklog(char* data, qint64 maxBytes, qint64 maxBacklogBytes) override
    {
        Q_UNUSED(maxBacklogBytes);
        return pull(data, maxBytes);
    }

private:
    void read()
    {
        m_state->readsInside.fetch_add(1);
        if (const TxMicSource* const source = m_state->holdUntilStopped.exchange(nullptr)) {
            m_state->held.store(true);
            const QDeadlineTimer deadline(10000);
            while (source->isRunning() && !m_state->destroyed.load()
                   && !deadline.hasExpired()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        m_state->readsInside.fetch_sub(1);
    }

    std::shared_ptr<HeldReadState> m_state;
};

AudioFormat stereoFloat48k()
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    return format;
}

std::unique_ptr<IAudioBus> openedBus(std::unique_ptr<FakeAudioBus> bus)
{
    const bool opened = bus->open(stereoFloat48k());
    Q_ASSERT(opened);
    Q_UNUSED(opened);
    return bus;
}

// The band's AGC as WDSP holds it: the threshold read back from the channel
// (what the AGC runs on, not the slice's or the channel's cached copy).
// The reading lives in shared state, so a readback that lands after the
// deadline writes there and not into a returned stack frame.
std::optional<int> wdspAgcThreshold(RadioModel& radio, SliceModel* slice)
{
    RxChannel* channel = radio.rxChannelForSlice(slice->sliceIndex());
    if (channel == nullptr) {
        return std::nullopt;
    }
    auto reading = std::make_shared<std::optional<int>>();
    channel->requestAgcReadBack(slice, [reading](double, double thresh) {
        *reading = static_cast<int>(std::round(thresh));
    });
    QDeadlineTimer deadline(5000);
    while (!reading->has_value() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return *reading;
}

// Fixed broad-band pseudo-noise for a stream's noise floor, as
// tst_radio_slice_settings_restore feeds it.
QVector<float> pseudoNoise()
{
    QVector<float> noise;
    noise.reserve(4096 * 2);
    quint32 state = 0x4d595df4U;
    for (int i = 0; i < 4096 * 2; ++i) {
        state = state * 1664525U + 1013904223U;
        noise.append((static_cast<float>((state >> 8) & 0xffffU) / 32767.5f - 1.0f) * 0.20f);
    }
    return noise;
}

} // namespace

class TestConnectableRadioModel : public QObject {
    Q_OBJECT

private slots:
    void nonActiveSliceDspEditsSurviveTheProductionBandCrossingSignals()
    {
        AppSettings::instance().clear();
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
        QVERIFY(harness);
        RadioModel& radio = harness->model();
        SliceModel* first = radio.slices().first();
        SliceModel* second = radio.sliceById(radio.addSlice());
        QVERIFY(second);
        QVERIFY(radio.setActiveSliceById(second->sliceIndex()));
        second->setFrequency(14225000.0);
        second->setFilter(200, 2800);
        second->setAgcThreshold(-20);
        radio.requestSettingsSave();
        radio.flushPendingSettingsSave();
        const QString prefix = QStringLiteral("Slice%1/").arg(second->sliceIndex());
        QVERIFY(radio.setActiveSliceById(first->sliceIndex()));

        // No requestSettingsSave seam here: connectToRadio has installed
        // the actual filter/AGC/frequency handlers for this non-active slice.
        second->setFilter(350, 2450);
        second->setAgcThreshold(-37);
        second->setFrequency(7100000.0);
        radio.flushPendingSettingsSave();

        AppSettings onDisk(AppSettings::instance().filePath());
        onDisk.load();
        QCOMPARE(onDisk.value(prefix + "Band20m/FilterLow").toInt(), 350);
        QCOMPARE(onDisk.value(prefix + "Band20m/AgcThreshold").toInt(), -37);
        QCOMPARE(onDisk.value(prefix + "Band20m/Frequency").toDouble(), 14225000.0);
        QCOMPARE(onDisk.value(prefix + "Band40m/Frequency").toDouble(), 7100000.0);
        QCOMPARE(onDisk.value(prefix + "LastBand").toString(), QStringLiteral("40m"));
        QCOMPARE(radio.activeSlice(), first);
        QCOMPARE(second->frequency(), 7100000.0);
        QCOMPARE(second->filterLow(), 350);
        QCOMPARE(second->filterHigh(), 2450);
        QCOMPARE(second->agcThreshold(), -37);
        QVERIFY(radio.waitForReceiveLaneForTest());
        RxChannel* channel = radio.rxChannelForSlice(second->sliceIndex());
        QVERIFY(channel);
        QCOMPARE(channel->filterLow(), 350.0);
        QCOMPARE(channel->filterHigh(), 2450.0);
        QCOMPARE(channel->agcThreshold(), -37);
    }

    // The desktop AGC-T slider sets the threshold; the receive lane reads
    // back the AGC top it gives and sets the slice's RF gain. A save that
    // ran before that readback kept the old RF gain, and on restart the RF
    // gain came back stale. The AGC itself came back as the slider left it
    // (the restored threshold reaches WDSP after the RF gain).
    void agcThresholdMoveSavesTheRfGainFromTheReadback()
    {
        AppSettings::instance().clear();
        int movedThreshold = 0;
        int movedRfGain = 0;
        std::optional<int> movedWdspThreshold;
        {
            auto harness = ConnectableRadioModel::create(
                10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
            QVERIFY(harness);
            RadioModel& radio = harness->model();
            SliceModel* slice = radio.slices().first();
            QVERIFY(slice);
            slice->setFrequency(14225000.0);
            radio.requestSettingsSave();
            radio.flushPendingSettingsSave();
            QVERIFY(radio.waitForReceiveLaneForTest());
            const int rfGainBefore = slice->rfGain();

            // The slider: outside the AGC sync guard.
            movedThreshold = slice->agcThreshold() - 25;
            slice->setAgcThreshold(movedThreshold);
            // The coalesced save can run before the lane's readback lands: flush
            // here, before the readback reaches this thread, as that timer would.
            radio.flushPendingSettingsSave();
            QVERIFY(radio.waitForReceiveLaneForTest());
            QTRY_VERIFY_WITH_TIMEOUT(slice->rfGain() != rfGainBefore, 5000);
            radio.flushPendingSettingsSave();
            movedRfGain = slice->rfGain();
            QCOMPARE(slice->agcThreshold(), movedThreshold);
            movedWdspThreshold = wdspAgcThreshold(radio, slice);
            QVERIFY(movedWdspThreshold.has_value());
        }

        // Restart: the settings as the file holds them, then a fresh model.
        AppSettings::instance().load();
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
        QVERIFY(harness);
        RadioModel& radio = harness->model();
        SliceModel* slice = radio.slices().first();
        QVERIFY(slice);
        QVERIFY(radio.waitForReceiveLaneForTest());
        QCoreApplication::processEvents();
        QVERIFY(radio.waitForReceiveLaneForTest());
        QCoreApplication::processEvents();
        QCOMPARE(wdspAgcThreshold(radio, slice), movedWdspThreshold);
        QCOMPARE(slice->agcThreshold(), movedThreshold);
        QCOMPARE(slice->rfGain(), movedRfGain);
    }

    // The desktop RF gain slider sets the AGC top; the receive lane reads
    // back the threshold it gives and sets the band's AGC-T. A save that
    // ran before that readback kept the band's old threshold, and on
    // restart that threshold reached WDSP after the RF gain and moved the
    // band's AGC away from where the slider left it.
    void rfGainMoveSavesTheBandsThresholdFromTheReadback()
    {
        AppSettings::instance().clear();
        int movedThreshold = 0;
        int movedRfGain = 0;
        std::optional<int> movedWdspThreshold;
        {
            auto harness = ConnectableRadioModel::create(
                10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
            QVERIFY(harness);
            RadioModel& radio = harness->model();
            SliceModel* slice = radio.slices().first();
            QVERIFY(slice);
            slice->setFrequency(14225000.0);
            // A threshold of the operator's own first, saved with the RF gain
            // it gives. A fresh channel skips a threshold equal to its -20
            // default, so the band starts away from it.
            const int rfGainAtStart = slice->rfGain();
            slice->setAgcThreshold(-35);
            QVERIFY(radio.waitForReceiveLaneForTest());
            QTRY_VERIFY_WITH_TIMEOUT(slice->rfGain() != rfGainAtStart, 5000);
            radio.requestSettingsSave();
            radio.flushPendingSettingsSave();
            QVERIFY(radio.waitForReceiveLaneForTest());
            const int thresholdBefore = slice->agcThreshold();

            // The slider: outside the AGC sync guard.
            movedRfGain = slice->rfGain() + 30;
            slice->setRfGain(movedRfGain);
            // The coalesced save can run before the lane's readback lands: flush
            // here, before the readback reaches this thread, as that timer would.
            radio.flushPendingSettingsSave();
            QVERIFY(radio.waitForReceiveLaneForTest());
            QTRY_VERIFY_WITH_TIMEOUT(slice->agcThreshold() != thresholdBefore, 5000);
            radio.flushPendingSettingsSave();
            movedThreshold = slice->agcThreshold();
            QCOMPARE(slice->rfGain(), movedRfGain);
            movedWdspThreshold = wdspAgcThreshold(radio, slice);
            QVERIFY(movedWdspThreshold.has_value());
        }

        // Restart: the settings as the file holds them, then a fresh model.
        AppSettings::instance().load();
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
        QVERIFY(harness);
        RadioModel& radio = harness->model();
        SliceModel* slice = radio.slices().first();
        QVERIFY(slice);
        QVERIFY(radio.waitForReceiveLaneForTest());
        QCoreApplication::processEvents();
        QVERIFY(radio.waitForReceiveLaneForTest());
        QCoreApplication::processEvents();
        QCOMPARE(wdspAgcThreshold(radio, slice), movedWdspThreshold);
        QCOMPARE(slice->agcThreshold(), movedThreshold);
        QCOMPARE(slice->rfGain(), movedRfGain);
    }

    // The AUTO tick writes the threshold from the noise floor, and the lane
    // readback then changes the slice's RF gain. Neither is the operator's
    // change, so neither schedules a save (and so neither can mark receiver
    // edits a station handover would refuse to drop).
    void theAutoTicksRfGainReadbackSchedulesNoSave()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        AppSettings::instance().clear();
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, false);
        QVERIFY(harness);
        RadioModel& radio = harness->model();
        SliceModel* slice = radio.slices().first();
        QVERIFY(slice);
        QVERIFY(slice->streamIndex() >= 0);
        // Only the explicit tick below may run: a timed tick while the lane
        // is awaited would land the tick's RF gain before the flush.
        QVERIFY(radio.autoAgcTimer());
        radio.autoAgcTimer()->stop();
        // The station's AGC source measures the slice's stream noise floor.
        DaemonAgcSource source(&radio);
        QTRY_VERIFY(source.activeStreamCount() >= 1);
        const QVector<float> noise = pseudoNoise();
        for (int frame = 0; frame < 90 && !slice->stationAutoAgcNoiseFloorValid(); ++frame) {
            QVERIFY(QMetaObject::invokeMethod(&radio, "rawIqDataForStream", Qt::DirectConnection,
                                              Q_ARG(int, slice->streamIndex()),
                                              Q_ARG(QVector<float>, noise)));
            QTest::qWait(35);
        }
        QVERIFY(slice->stationAutoAgcNoiseFloorValid());

        // A threshold the tick will certainly move, then AUTO on, and all
        // of that written once the readback has landed.
        const int rfGainAtStart = slice->rfGain();
        slice->setAgcThreshold(-42);
        slice->setAutoAgcEnabled(true);
        QVERIFY(radio.waitForReceiveLaneForTest());
        QTRY_VERIFY_WITH_TIMEOUT(slice->rfGain() != rfGainAtStart, 5000);
        radio.flushPendingSettingsSave();
        const QString rfGainKey = QStringLiteral("Slice%1/RfGain").arg(slice->sliceIndex());
        const int savedRfGain = AppSettings::instance().value(rfGainKey).toInt();
        QCOMPARE(savedRfGain, slice->rfGain());

        radio.runAutoAgcTickForTest();
        QVERIFY(slice->agcThreshold() != -42);   // the tick did write
        QVERIFY(slice->autoAgcEnabled());        // and it is not a manual change
        QVERIFY(radio.waitForReceiveLaneForTest());
        QTRY_VERIFY_WITH_TIMEOUT(slice->rfGain() != savedRfGain, 5000);

        // Nothing was scheduled, so a flush writes nothing: what is saved is
        // still the RF gain the operator's own change gave.
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(rfGainKey).toInt(), savedRfGain);
    }

    void reconnectPreservesLiveReceivers()
    {
        auto harness = ConnectableRadioModel::create();
        QVERIFY(harness);
        RadioModel& model = harness->model();
        model.setReceiveOnlyStationPolicy(true);
        SliceModel* const a = model.slices().first();
        SliceModel* const b = model.sliceById(model.addSlice());
        QVERIFY(b);
        a->setFrequency(3865100);
        a->setDspMode(DSPMode::LSB);
        b->setFrequency(14225000);
        b->setDspMode(DSPMode::USB);
        b->setPanKey(QStringLiteral("pan-1"));
        a->setNnrAlpha(1.75);
        b->setNnrAlpha(2.25);
        model.setActiveSlice(b->sliceIndex());
        QSignalSpy activeChanges(&model, &RadioModel::activeSliceChanged);
        QSignalSpy removed(&model, &RadioModel::sliceRemoved);

        model.disconnectFromRadio();
        RadioDiscovery::clearHoldOffForTest();
        model.connectToRadioPreservingSlices(harness->radioInfo());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Connected, 10000);
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(model.slices().at(0), a);
        QCOMPARE(model.slices().at(1), b);
        QCOMPARE(model.activeSlice(), b);
        QCOMPARE(a->frequency(), 3865100.0);
        QCOMPARE(b->frequency(), 14225000.0);
        QCOMPARE(a->dspMode(), DSPMode::LSB);
        QCOMPARE(b->dspMode(), DSPMode::USB);
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(removed.count(), 0);
        QCOMPARE(activeChanges.count(), 0);
        QVERIFY(a->streamIndex() >= 0);
        QVERIFY(b->streamIndex() >= 0);
        QVERIFY(!model.mox());
        QCOMPARE(a->nnrAlpha(), 1.75);
        QCOMPARE(b->nnrAlpha(), 2.25);
        // R-R3-39: a receiver's NNR tuning is what the receive lane last
        // wrote, and the reconnect only queued it there.
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(model.rxChannelForSlice(a->sliceIndex())->nnrTuning().alpha, 1.75);
        QCOMPARE(model.rxChannelForSlice(b->sliceIndex())->nnrTuning().alpha, 2.25);

        // Model application uses this same preserving seam while B remains
        // active; receiver A must still seed its own native configuration.
        QString reason;
        RadioDiscovery::clearHoldOffForTest();
        QVERIFY2(model.applyNnrModelSelection(model.dspAssets()->selectionRevision(), &reason),
                 qPrintable(reason));
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Connected, 10000);
        QCOMPARE(model.activeSlice(), b);
        QCOMPARE(model.slices().first(), a);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(model.rxChannelForSlice(a->sliceIndex())->nnrTuning().alpha, 1.75);
        QCOMPARE(model.rxChannelForSlice(b->sliceIndex())->nnrTuning().alpha, 2.25);
        QCOMPARE(activeChanges.count(), 0);
        QCOMPARE(removed.count(), 0);

        RadioInfo other = harness->radioInfo();
        other.macAddress = QStringLiteral("bb:bb:cc:11:22:33");
        model.connectToRadioPreservingSlices(other);
        QCOMPARE(model.currentRadioMac(), harness->radioInfo().macAddress);
        QCOMPARE(model.connectionState(), ConnectionState::Connected);
    }

    // Follow-up item 1 (R-R3-21): a saved NR3 choice on a Core with no NR3
    // model file. The slice comes up with NR off and the plain reason set,
    // and the receiver's WDSP channel is never running NR3, whether the
    // model was already gone when the Core started or went missing between
    // the start and the connect (the connect's own model load is what finds
    // it gone).
    void pureSignalReadinessFollowsTheReceiveOnlyPolicyAtOnce()
    {
        // iPhone app Part A fix wave (Task 4b finding, R-IOS-01): the
        // station's pureSignal object says whether PureSignal can run
        // (canActuate), and follows a receive-only change on the call that
        // makes it, before any event is processed.
        // R-R3-49 (parity Task 7): a receive-only Core permits PureSignal
        // (a window arms it there off the air; arming keys nothing), so
        // canActuate stays true through the change.
        auto harness = ConnectableRadioModel::create();
        QVERIFY(harness);
        RadioModel& model = harness->model();
        PureSignal* const coordinator = model.pureSignal();
        PureSignalSessionFacade* const facade = model.pureSignalFacade();
        QVERIFY(coordinator != nullptr && facade != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(facade->canActuate(), 5000);

        QSignalSpy changed(facade, &PureSignalSessionFacade::statusChanged);
        model.setReceiveOnlyStationPolicy(true);
        QVERIFY(coordinator->canActuate());
        QVERIFY(facade->canActuate());
        QVERIFY(!changed.isEmpty());

        changed.clear();
        model.setReceiveOnlyStationPolicy(false);
        QVERIFY(facade->canActuate());
        QVERIFY(!changed.isEmpty());

        // Setting the policy it already has changes nothing.
        changed.clear();
        model.setReceiveOnlyStationPolicy(false);
        QVERIFY(changed.isEmpty());
    }

    void savedNr3OnACoreWithNoModelComesUpOff_data()
    {
        QTest::addColumn<bool>("goneBeforeStart");
        QTest::newRow("gone before the Core starts") << true;
        QTest::newRow("gone by the connect") << false;
    }

    void savedNr3OnACoreWithNoModelComesUpOff()
    {
        QFETCH(bool, goneBeforeStart);
        const QString none =
            QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
        const QString prefix = QStringLiteral("hardware/AA:BB:CC:11:22:33/slices/0/nnr/");
        auto& settings = AppSettings::instance();
        const auto cleanup = qScopeGuard([&settings, prefix] {
            DspAssetService::setBundledNr3ModelPathsForTest({});
            for (const QString& key : settings.allKeys()) {
                if (key.startsWith(prefix)) { settings.remove(key); }
            }
        });
        settings.setValue(prefix + QStringLiteral("NrActive"), static_cast<int>(NrSlot::NR3));
        const auto noFiles = [](const QString&) { return QString(); };
        if (goneBeforeStart) {
            DspAssetService::setBundledNr3ModelPathsForTest(noFiles);
        }

        QList<NrSlot> sliceHistory;
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, [&](RadioModel& model) {
                QCOMPARE(model.dspAssets()->nr3Runnable(), goneBeforeStart ? false : true);
                if (!goneBeforeStart) {
                    DspAssetService::setBundledNr3ModelPathsForTest(noFiles);
                }
                QObject::connect(&model, &RadioModel::sliceAdded, &model,
                                 [&model, &sliceHistory](int id) {
                    SliceModel* slice = model.sliceById(id);
                    sliceHistory.append(slice->activeNr());
                    QObject::connect(slice, &SliceModel::activeNrChanged, slice,
                                     [&sliceHistory](NrSlot slot) { sliceHistory.append(slot); });
                });
            });
        QVERIFY(harness);
        RadioModel& model = harness->model();
        QVERIFY(!model.dspAssets()->nr3Runnable());
        QCOMPARE(model.slices().size(), 1);
        SliceModel* slice = model.slices().first();
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(slice->nnrLastError(), none);
        RxChannel* channel = model.rxChannelForSlice(slice->sliceIndex());
        QVERIFY(channel != nullptr);
        // R-R3-39: the receive lane settles the channel's NR slot, so read
        // it only once the lane has run what the connect queued.
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(channel->activeNr(), NrSlot::Off);
        // The slice never held NR3, so no receiver push could carry it.
        QVERIFY(!sliceHistory.isEmpty());
        QVERIFY(!sliceHistory.contains(NrSlot::NR3));
        QCOMPARE(sliceHistory.constLast(), NrSlot::Off);

        // Turning NR3 on is still refused with the same reason.
        slice->setActiveNr(NrSlot::NR3);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(channel->activeNr(), NrSlot::Off);
        QCOMPARE(slice->nnrLastError(), none);
    }

    // Spike: WdspEngine::setSynchronousInitForTest(true) + initialize()
    // must run finishInitialization(false) for real (impulse cache init,
    // PS feedback channel open) and then let the caller open one more RX
    // channel, all without ever calling WDSPwisdom(). Skipping WDSPwisdom()
    // does not remove FFTW's planning work -- WDSP's own filter/channel
    // construction calls fftw_plan_dft_1d(..., FFTW_PATIENT) with no
    // wisdom to draw on (third_party/wdsp/src/fir.c and friends), so the
    // work simply moves to first channel use. This measures that moved
    // cost directly, on a config directory TestSandboxInit.cpp guarantees
    // is cold (no on-disk wisdom file, and this test process has never
    // planned an FFTW transform of any of these sizes before).
    void synchronousInitTimingSpike() {
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);

        const QString configDir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

        QElapsedTimer timer;
        timer.start();

        QVERIFY(engine.initialize(configDir));
        QVERIFY(engine.isInitialized());

        RxChannel* rx = engine.createRxChannel(0);

        const qint64 elapsedMs = timer.elapsed();
        QVERIFY(rx != nullptr);

        qInfo("synchronousInitTimingSpike: finishInitialization(false) + "
              "one RX channel took %lld ms", static_cast<long long>(elapsedMs));

        engine.destroyRxChannel(0);
    }

    // Core assertion for the task: RadioModel::connectToRadio() must reach
    // ConnectionState::Connected against a real (fake) radio within
    // QtTest's per-function timeout, using nothing but the
    // ConnectableRadioModel helper that tasks 3, 12 and 20 will also use.
    void radioModelReachesConnectedAgainstFake() {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);

        QCOMPARE(harness->model().connectionState(), ConnectionState::Connected);
        // Belt-and-braces: ConnectionState::Connected is RadioConnection's
        // own state-machine output, but confirm the fake agrees the
        // metis-start handshake actually completed -- the same check
        // tst_p1_loopback_connection.cpp makes for a bare P1RadioConnection.
        QVERIFY(harness->fake().isRunning());

        // Step 3: prove the synchronous init seam never spawned the async
        // "WisdomThread" QThread. See WdspEngine::
        // wisdomThreadSpawnedForTest()'s doc comment (WdspEngine.h) for why
        // this flag -- rather than an OS-level thread-table scan, which
        // Qt has no cross-platform API for anyway -- is the correct and
        // sufficient check: WdspEngine.cpp is the only place in the tree
        // that ever constructs a thread with that name, and the flag is
        // sticky (only ever set true, never reset), so this single read,
        // taken immediately before we intentionally tear the harness down
        // below, stands for "true throughout connect AND teardown".
        QVERIFY(!harness->model().wdspEngine()->wisdomThreadSpawnedForTest());

        // Explicit teardown rather than letting `harness` fall out of
        // scope at the end of the slot, so RadioModel::~RadioModel() (via
        // teardownConnection()) and P1FakeRadio's destructor both run here,
        // under this test's watch, rather than implicitly after the last
        // assertion above.
        harness.reset();
    }

    // A background Core started by the app is told where the app keeps its
    // saved FFT plans (nereusd --dsp-cache-dir). The radio connection has to
    // look there, or the Core builds every plan again, for minutes, while
    // the radio waits. The engine makes the folder it is handed and nothing
    // else makes this one, so its existence after a connect says which
    // folder the connection asked for.
    void connectKeepsThePlansInTheFolderTheProcessWasGiven() {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString folder = QDir(scratch.path()).filePath(QStringLiteral("the app's plans"));
        DspCacheLocation::setDirectory(folder);
        const auto restore = qScopeGuard([] { DspCacheLocation::setDirectory({}); });
        QVERIFY(!QFileInfo::exists(folder));

        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        QCOMPARE(harness->model().connectionState(), ConnectionState::Connected);
        QVERIFY2(QFileInfo(folder).isDir(), qPrintable(folder));
        harness.reset();
    }

    // Main CI at 4066a86b6: AudioEngine::stop() dropped the TX input while
    // the TX worker was still inside pullTxMic, because teardownConnection
    // stopped the audio engine before the TX pump. The worker's read here
    // stays inside the bus until stopPump begins, so with the old order the
    // bus is always destroyed under it. The PC row is the injected input
    // the CI crash read; the VAX row is the VAX TX bus a user's session
    // reads (the PC microphone itself is the capture reader, which outlives
    // stop()).
    void teardownStopsTheTxWorkerBeforeItsInputGoes_data()
    {
        QTest::addColumn<bool>("vax");
        QTest::newRow("PC microphone") << false;
        QTest::newRow("VAX TX") << true;
    }

    void teardownStopsTheTxWorkerBeforeItsInputGoes()
    {
        QFETCH(bool, vax);
        const auto state = std::make_shared<HeldReadState>();

        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, [state, vax](RadioModel& model) {
                model.audioEngine()->setStartInitializerForTest(
                    [state, vax](AudioEngine& engine) {
                        engine.setSpeakersBusForTest(openedBus(
                            std::make_unique<FakeAudioBus>(QStringLiteral("Fake speakers"))));
                        std::unique_ptr<IAudioBus> held =
                            openedBus(std::make_unique<HeldReadBus>(state));
                        if (vax) {
                            engine.setTxInputBusForTest(openedBus(
                                std::make_unique<FakeAudioBus>(QStringLiteral("Fake TX input"))));
                            engine.setVaxTxBusForTest(std::move(held));
                        } else {
                            engine.setTxInputBusForTest(std::move(held));
                        }
                    });
            });
        QVERIFY(harness != nullptr);
        RadioModel& radio = harness->model();
        if (vax) {
            radio.transmitModel().setVaxSourceAvailable(true);
        }
        radio.transmitModel().setMicSource(vax ? MicSource::Vax : MicSource::Pc);
        QCOMPARE(radio.transmitModel().micSource(), vax ? MicSource::Vax : MicSource::Pc);

        const TxMicSource* const source = radio.txMicSourceForTest();
        QVERIFY(source != nullptr);
        QVERIFY(source->isRunning());
        state->holdUntilStopped.store(source);
        QVERIFY2(QTest::qWaitFor([&state]() { return state->held.load(); }, 10000),
                 "the TX worker never read the selected input");

        harness.reset();

        QVERIFY(state->destroyed.load());
        QVERIFY2(!state->destroyedDuringRead.load(),
                 "the audio engine destroyed the TX input while the TX worker was reading it");
    }
};

QTEST_MAIN(TestConnectableRadioModel)
#include "tst_connectable_radio_model.moc"
