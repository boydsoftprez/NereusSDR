// =================================================================
// tests/tst_asio_session.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR test.  V-SW-7: the ASIO session rules and the
// buffer switch with FakeAsioDriver (R-AUD-19, R-AUD-20, R-AUD-21,
// R-AUD-11), the window's AsioBackend over a fake helper link, and the
// no-allocation rule of the buffer switch (Task 2's hook).  No ASIO
// driver, sound device or helper process is touched (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (V-SW-7). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 17: each use names its role, the
//               session's buffer and rate. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-07, R-AUD-21):
//               outputs on one pair add together, a reset adopts the
//               driver's size and rate, the mic on the last input, a use
//               the driver lacks, a reset inside createBuffers.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "fakes/FakeAsioDriver.h"
#include "fakes/FakeAudioBus.h"

#include "core/audio/AsioBackend.h"
#include "core/audio/AsioSession.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/IAudioStreamHost.h"
#include "core/audio/MatcherRing.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QStandardPaths>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// The allocation hook of Task 2 (tst_device_rate_matcher_threads.cpp): the
// replaced operator new counts calls made on the watched thread.
// ---------------------------------------------------------------------------
namespace {

std::atomic<bool> g_watching{false};
std::thread::id g_watchedId;
std::atomic<std::uint64_t> g_newCalls{0};
std::atomic<std::uint64_t> g_deleteCalls{0};

bool onWatchedThread()
{
    return g_watching.load(std::memory_order_acquire) && std::this_thread::get_id() == g_watchedId;
}

void* countedAllocate(std::size_t size)
{
    if (onWatchedThread()) {
        g_newCalls.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        std::abort();   // a test binary out of memory: stop here
    }
    return p;
}

void* countedAllocateAligned(std::size_t size, std::align_val_t alignment)
{
    if (onWatchedThread()) {
        g_newCalls.fetch_add(1, std::memory_order_relaxed);
    }
    const std::size_t align = std::max(static_cast<std::size_t>(alignment), sizeof(void*));
    void* p = nullptr;
#if defined(Q_OS_WIN)
    p = _aligned_malloc(size == 0 ? 1 : size, align);
#else
    if (posix_memalign(&p, align, size == 0 ? 1 : size) != 0) {
        p = nullptr;
    }
#endif
    if (p == nullptr) {
        std::abort();
    }
    return p;
}

void countedFree(void* p)
{
    if (p != nullptr && onWatchedThread()) {
        g_deleteCalls.fetch_add(1, std::memory_order_relaxed);
    }
    std::free(p);
}

void countedFreeAligned(void* p)
{
    if (p != nullptr && onWatchedThread()) {
        g_deleteCalls.fetch_add(1, std::memory_order_relaxed);
    }
#if defined(Q_OS_WIN)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

} // namespace

void* operator new(std::size_t size) { return countedAllocate(size); }
void* operator new[](std::size_t size) { return countedAllocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return countedAllocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return countedAllocate(size); }
void* operator new(std::size_t size, std::align_val_t a) { return countedAllocateAligned(size, a); }
void* operator new[](std::size_t size, std::align_val_t a) { return countedAllocateAligned(size, a); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return countedAllocateAligned(size, a);
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return countedAllocateAligned(size, a);
}
void operator delete(void* p) noexcept { countedFree(p); }
void operator delete[](void* p) noexcept { countedFree(p); }
void operator delete(void* p, std::size_t) noexcept { countedFree(p); }
void operator delete[](void* p, std::size_t) noexcept { countedFree(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { countedFree(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { countedFree(p); }
void operator delete(void* p, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { countedFreeAligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { countedFreeAligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { countedFreeAligned(p); }

using namespace NereusSDR;
using NereusSDR::Test::FakeAsioDriver;

namespace {

const QString kFocusrite = QStringLiteral("Focusrite USB ASIO");
const QString kMotu = QStringLiteral("MOTU M Series");

AsioDriverCaps driverCaps(const QString& name, int outputs, int inputs)
{
    AsioDriverCaps caps;
    caps.name = name;
    caps.outputChannels = outputs;
    caps.inputChannels = inputs;
    caps.sampleType = AsioSampleType::Float32Lsb;
    caps.minBufferFrames = 64;
    caps.maxBufferFrames = 2048;
    caps.preferredBufferFrames = 256;
    caps.granularity = -1;
    caps.sampleRates = {44100.0, 48000.0, 96000.0};
    caps.currentRate = 48000.0;
    caps.inputLatencyFrames = 300;
    caps.outputLatencyFrames = 400;
    return caps;
}

AsioUse use(AudioRole role, const QString& driver, int first, AudioDeviceDirection direction)
{
    return AsioUse{role, driver, AudioChannelPair{first, 2}, direction};
}

QList<AsioUse> focusriteUses()
{
    return {use(AudioRole::Speakers, kFocusrite, 3, AudioDeviceDirection::Output),
            use(AudioRole::Headphones, kFocusrite, 1, AudioDeviceDirection::Output),
            use(AudioRole::Vax1, kFocusrite, 1, AudioDeviceDirection::Input)};
}

const AsioUse* findMove(const AsioSwitchPlan& plan, AudioRole role)
{
    for (const AsioUse& moved : plan.moves) {
        if (moved.role == role) {
            return &moved;
        }
    }
    return nullptr;
}

// An input use's sink: copies the stereo it gets into fixed storage and
// counts the posts.  Allocation-free.
class RecordingSink final : public IAudioInputSink {
public:
    void onInput(const float* stereo, int frames, int sampleRate,
                 std::int64_t captureNsOfFrame0) override
    {
        const int n = std::min(frames, kMaxFrames);
        std::memcpy(m_stereo.data(), stereo, sizeof(float) * 2 * static_cast<std::size_t>(n));
        m_frames.store(frames, std::memory_order_release);
        m_rate.store(sampleRate, std::memory_order_release);
        m_captureNs.store(captureNsOfFrame0, std::memory_order_release);
        m_posts.fetch_add(1, std::memory_order_acq_rel);
    }
    static constexpr int kMaxFrames = 4096;
    std::array<float, kMaxFrames * 2> m_stereo{};
    std::atomic<int> m_frames{0};
    std::atomic<int> m_rate{0};
    std::atomic<std::int64_t> m_captureNs{0};
    std::atomic<int> m_posts{0};
};

// A matcher at 48 kHz both sides, for a constant left and right.
std::unique_ptr<DeviceRateMatcher> constantMatcher(int callbackFrames)
{
    DeviceRateMatcher::Config config;
    config.inRate = 48000;
    config.outRate = 48000;
    config.callbackFrames = callbackFrames;
    auto matcher = std::make_unique<DeviceRateMatcher>(config);
    matcher->forceRatioForTest(1.0);
    return matcher;
}

// One buffer switch's worth of a constant left and right, in 64-frame writes.
void feedConstant(DeviceRateMatcher& matcher, float left, float right, int frames)
{
    std::array<float, 64 * 2> block{};
    for (std::size_t i = 0; i < block.size(); i += 2) {
        block[i] = left;
        block[i + 1] = right;
    }
    for (int done = 0; done < frames; done += 64) {
        matcher.write(block.data(), 64, 0);
    }
}

float floatAt(void* buffer, int frame)
{
    return static_cast<const float*>(buffer)[frame];
}

// The helper link of the window's AsioBackend, recorded.
struct FakeLink {
    int describes = 0;
    QStringList describedDrivers;
    QList<CaptureProtocol::AsioOpen> opens;
    int controlPanels = 0;
    QList<bool> demands;

    AsioHelperLink make()
    {
        AsioHelperLink link;
        link.describe = [this](const QString& driver) {
            ++describes;
            describedDrivers.append(driver);
        };
        link.open = [this](const CaptureProtocol::AsioOpen& open) { opens.append(open); };
        link.openControlPanel = [this]() { ++controlPanels; };
        link.setDemanded = [this](bool demanded) { demands.append(demanded); };
        return link;
    }
};

// A backend on a fake link, Focusrite described with 4 outputs and 2
// inputs, the saved buffer and rate the defaults.
std::unique_ptr<AsioBackend> linkedBackend(FakeLink& link)
{
    auto backend = std::make_unique<AsioBackend>();
    backend->setPreferencesSource([] { return AsioSessionPreferences{}; });
    backend->setHelperLink(link.make());
    CaptureProtocol::AsioCapsRecord list;
    list.drivers = {kFocusrite, kMotu};
    backend->onAsioCaps(list);
    CaptureProtocol::AsioCapsRecord focusrite;
    focusrite.drivers = list.drivers;
    focusrite.driver = kFocusrite;
    focusrite.caps = driverCaps(kFocusrite, 4, 2);
    backend->onAsioCaps(focusrite);
    return backend;
}

AudioStreamRequest outputOn(const QString& driver, int first)
{
    AudioStreamRequest request;
    request.direction = AudioDeviceDirection::Output;
    request.deviceId = driver;
    request.pair = AudioChannelPair{first, 2};
    return request;
}

// The window's AsioBackend opens nothing in a test run (TestSandboxInit
// turns test mode on); a backend test over the fake link lifts that for
// its own length, whatever test ran before it.
struct BackendOpensInThisTest {
    const bool was = QStandardPaths::isTestModeEnabled();
    BackendOpensInThisTest() { QStandardPaths::setTestModeEnabled(false); }
    ~BackendOpensInThisTest() { QStandardPaths::setTestModeEnabled(was); }
    BackendOpensInThisTest(const BackendOpensInThisTest&) = delete;
    BackendOpensInThisTest& operator=(const BackendOpensInThisTest&) = delete;
};

// The stream events a bus posted, recorded.
struct EventLog {
    QList<AudioStreamEvent::Kind> kinds;
    std::function<void(const AudioStreamEvent&)> sink()
    {
        return [this](const AudioStreamEvent& event) { kinds.append(event.kind); };
    }
};

CaptureProtocol::AsioState stateOf(quint32 serial, CaptureProtocol::AsioStateKind kind,
                                   int frames = 256, double rate = 48000.0)
{
    CaptureProtocol::AsioState state;
    state.serial = serial;
    state.state = kind;
    state.driver = kFocusrite;
    state.bufferFrames = frames;
    state.rate = rate;
    state.outputLatencyFrames = 480;
    return state;
}

} // namespace

class TestAsioSession : public QObject {
    Q_OBJECT

private slots:
    // ── planAsioSwitch (R-AUD-19, settled call 17) ─────────────────────────

    void planKeepsPairsTheNewDriverHas()
    {
        const AsioUse mic = use(AudioRole::TxInput, kMotu, 1, AudioDeviceDirection::Input);
        const AsioSwitchPlan plan = planAsioSwitch(focusriteUses(), mic, driverCaps(kMotu, 4, 4));
        QCOMPARE(plan.toDriver, kMotu);
        // Every other ASIO use, VAX included.
        QCOMPARE(plan.moves.size(), 3);
        const AsioUse* speakers = findMove(plan, AudioRole::Speakers);
        const AsioUse* headphones = findMove(plan, AudioRole::Headphones);
        const AsioUse* vax = findMove(plan, AudioRole::Vax1);
        QVERIFY(speakers != nullptr && headphones != nullptr && vax != nullptr);
        QCOMPARE(speakers->pair, (AudioChannelPair{3, 2}));
        QCOMPARE(speakers->driver, kMotu);
        QCOMPARE(speakers->direction, AudioDeviceDirection::Output);
        QCOMPARE(headphones->pair, (AudioChannelPair{1, 2}));
        QCOMPARE(vax->pair, (AudioChannelPair{1, 2}));
        QCOMPARE(vax->direction, AudioDeviceDirection::Input);
    }

    void planMovesToTheFirstPairWhenTheNewDriverLacksIt()
    {
        const AsioUse mic = use(AudioRole::TxInput, kMotu, 1, AudioDeviceDirection::Input);
        const AsioSwitchPlan plan = planAsioSwitch(focusriteUses(), mic, driverCaps(kMotu, 2, 2));
        const AsioUse* speakers = findMove(plan, AudioRole::Speakers);
        QVERIFY(speakers != nullptr);
        QCOMPARE(speakers->pair, (AudioChannelPair{1, 2}));
        QCOMPARE(findMove(plan, AudioRole::Headphones)->pair, (AudioChannelPair{1, 2}));
    }

    void planIsEmptyWhenNothingElseUsesAsio()
    {
        const AsioUse mic = use(AudioRole::TxInput, kMotu, 1, AudioDeviceDirection::Input);
        QVERIFY(planAsioSwitch({}, mic, driverCaps(kMotu, 4, 4)).moves.isEmpty());
        // The requested role's own old use is not a move.
        const QList<AsioUse> onlyMic = {use(AudioRole::TxInput, kFocusrite, 1, AudioDeviceDirection::Input)};
        QVERIFY(planAsioSwitch(onlyMic, mic, driverCaps(kMotu, 4, 4)).moves.isEmpty());
    }

    void cancelAppliesNothing()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        driver.addDriver(driverCaps(kMotu, 4, 4));
        AsioSession session(driver);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        driver.clearCalls();
        // The UI plans, shows the prompt, and the operator cancels: the
        // plan is never applied, so the session is not touched.
        const AsioSwitchPlan plan = planAsioSwitch(
            focusriteUses(), use(AudioRole::TxInput, kMotu, 1, AudioDeviceDirection::Input),
            driverCaps(kMotu, 4, 4));
        QCOMPARE(plan.moves.size(), 3);
        QVERIFY(driver.calls().isEmpty());
        QVERIFY(session.isOpen());
        QCOMPARE(session.driverName(), kFocusrite);
        QCOMPARE(driver.current().name, kFocusrite);
    }

    // ── One buffer size and rate (R-AUD-20) ────────────────────────────────

    void bufferAndRateAreTheLatestOpens()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        QCOMPARE(session.bufferFrames(), 256);
        QVERIFY(session.open(kFocusrite, 512, 96000.0, focusriteUses()));
        QCOMPARE(driver.lastBufferFrames(), 512);
        QCOMPARE(session.bufferFrames(), 512);
        QCOMPARE(session.sampleRate(), 96000.0);
        QVERIFY(driver.calls().contains(QStringLiteral("setSampleRate:96000")));
        // Every use runs at the one size: the buffers were made once, for
        // all of their channels.
        QCOMPARE(driver.lastOutputChannels(), (QList<int>{0, 1, 2, 3}));
        QCOMPARE(driver.lastInputChannels(), (QList<int>{0, 1}));
    }

    void fixedBufferSize()
    {
        AsioDriverCaps caps = driverCaps(kFocusrite, 2, 2);
        caps.minBufferFrames = 256;
        caps.maxBufferFrames = 256;
        caps.preferredBufferFrames = 256;
        caps.granularity = 0;
        QVERIFY(asioBufferSizeFixed(caps));
        QVERIFY(!asioBufferSizeFixed(driverCaps(kFocusrite, 2, 2)));
        QCOMPARE(asioSessionBufferFrames(caps, 512), 256);

        FakeAsioDriver driver;
        driver.addDriver(caps);
        AsioSession session(driver);
        QVERIFY(session.open(kFocusrite, 512, 48000.0,
                             {use(AudioRole::Speakers, kFocusrite, 1, AudioDeviceDirection::Output)}));
        QCOMPARE(session.bufferFrames(), 256);
    }

    void bufferSizeSteps()
    {
        AsioDriverCaps caps = driverCaps(kFocusrite, 2, 2);
        QCOMPARE(asioSessionBufferFrames(caps, 0), 256);      // preferred
        QCOMPARE(asioSessionBufferFrames(caps, 300), 512);    // powers of two from 64
        QCOMPARE(asioSessionBufferFrames(caps, 9000), 2048);  // the maximum
        caps.granularity = 48;
        QCOMPARE(asioSessionBufferFrames(caps, 100), 112);    // 64 + 48
    }

    // ── Formats (settled call 28) ──────────────────────────────────────────

    void deviceFormats()
    {
        QCOMPARE(asioDeviceFormat(AsioSampleType::Int16Lsb), std::optional(DeviceSampleFormat::Int16));
        QCOMPARE(asioDeviceFormat(AsioSampleType::Int24Lsb), std::optional(DeviceSampleFormat::Int24Packed));
        QCOMPARE(asioDeviceFormat(AsioSampleType::Int32Lsb), std::optional(DeviceSampleFormat::Int32));
        QCOMPARE(asioDeviceFormat(AsioSampleType::Float32Lsb), std::optional(DeviceSampleFormat::Float32));
        QCOMPARE(asioDeviceFormat(AsioSampleType::Float64Lsb), std::optional(DeviceSampleFormat::Float64));
        QVERIFY(!asioDeviceFormat(AsioSampleType::Unsupported).has_value());

        FakeAsioDriver driver;
        AsioDriverCaps caps = driverCaps(kFocusrite, 2, 2);
        caps.sampleType = AsioSampleType::Unsupported;
        driver.addDriver(caps);
        AsioSession session(driver);
        QVERIFY(!session.open(kFocusrite, 256, 48000.0,
                              {use(AudioRole::Speakers, kFocusrite, 1, AudioDeviceDirection::Output)}));
        QVERIFY(!driver.loaded());
    }

    // ── Reset (R-AUD-21, D14) ──────────────────────────────────────────────

    void resetRequestRestartsOnTheSessionThread_data()
    {
        QTest::addColumn<int>("message");
        QTest::newRow("reset request") << static_cast<int>(AsioMessage::ResetRequest);
        QTest::newRow("buffer size change") << static_cast<int>(AsioMessage::BufferSizeChange);
    }

    void resetRequestRestartsOnTheSessionThread()
    {
        QFETCH(int, message);
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QSignalSpy restarted(session.notifier(), &AsioSessionNotifier::restarted);
        QSignalSpy failed(session.notifier(), &AsioSessionNotifier::failed);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));

        // Another engine's stream, which must keep running.
        FakeAudioBus otherEngine;
        QVERIFY(otherEngine.open(AudioFormat{}));
        const float block[4] = {0.1f, 0.1f, 0.1f, 0.1f};
        otherEngine.push(reinterpret_cast<const char*>(block), sizeof(block));

        // The driver's control panel changed the size and the rate; the
        // next load gives them.
        AsioDriverCaps changed = driverCaps(kFocusrite, 4, 2);
        changed.preferredBufferFrames = 512;
        changed.currentRate = 44100.0;
        driver.setCaps(changed);
        driver.clearCalls();

        std::atomic<bool> returned{false};
        std::thread foreign([&driver, &returned, message]() {
            driver.fireMessage(static_cast<AsioMessage>(message));
            returned.store(true, std::memory_order_release);
        });
        foreign.join();
        QVERIFY(returned.load());
        // Only posted: nothing was torn down on the driver's thread.
        QVERIFY(driver.calls().isEmpty());
        QCOMPARE(session.restartCount(), 0);

        QTRY_COMPARE(restarted.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(session.restartCount(), 1);
        QVERIFY(session.isOpen());
        const QStringList calls = driver.calls();
        const int stop = calls.indexOf(QStringLiteral("stop"));
        const int unload = calls.indexOf(QStringLiteral("disposeAndUnload"));
        const int load = calls.indexOf(QStringLiteral("load:") + kFocusrite);
        const int create = calls.indexOf(QStringLiteral("createBuffers:512"));
        const int start = calls.indexOf(QStringLiteral("start"));
        QVERIFY2(stop >= 0 && stop < unload && unload < load && load < create && create < start,
                 qPrintable(calls.join(QLatin1Char(','))));
        // R-AUD-21: the session restarts with the driver's new settings,
        // its preferred size (512) and its current rate (44100), and does
        // not set the old rate again.
        QCOMPARE(session.caps().preferredBufferFrames, 512);
        QCOMPARE(session.bufferFrames(), 512);
        QCOMPARE(session.sampleRate(), 44100.0);
        QVERIFY2(!calls.contains(QStringLiteral("setSampleRate:48000")),
                 qPrintable(calls.join(QLatin1Char(','))));

        // The other engine never noticed.
        QVERIFY(otherEngine.isOpen());
        otherEngine.push(reinterpret_cast<const char*>(block), sizeof(block));
        QCOMPARE(otherEngine.pushCount(), 2);
    }

    void repeatedResetsRestartOnce()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QSignalSpy restarted(session.notifier(), &AsioSessionNotifier::restarted);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        driver.fireMessage(AsioMessage::ResetRequest);
        driver.fireMessage(AsioMessage::ResetRequest);
        driver.fireMessage(AsioMessage::ResyncRequest);
        QTRY_COMPARE(restarted.count(), 1);
        QCoreApplication::processEvents();
        QCOMPARE(session.restartCount(), 1);
    }

    // Settled call 13 at the session: a reset the driver posts from inside
    // ASIOCreateBuffers reaches the session and restarts it once.
    void resetDuringCreateBuffersRestarts()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QSignalSpy restarted(session.notifier(), &AsioSessionNotifier::restarted);
        driver.sendMessageDuringNextCreateBuffers(AsioMessage::ResetRequest);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        QTRY_COMPARE(restarted.count(), 1);
        QCOMPARE(session.restartCount(), 1);
        QVERIFY(session.isOpen());
    }

    // R-AUD-21: what the session runs at after a reset is what the helper
    // reports in its Restarted state (sendAsioRun reads bufferFrames() and
    // sampleRate()), and the window's buses reopen for it.
    void resetSizeReachesTheWindowsBuses()
    {
        const BackendOpensInThisTest opens;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QSignalSpy restarted(session.notifier(), &AsioSessionNotifier::restarted);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));

        FakeLink link;
        auto backend = linkedBackend(link);
        EventLog log;
        auto speakers = backend->createOutput(outputOn(kFocusrite, 3));
        speakers->setStreamEventSink(log.sink());
        QVERIFY2(speakers->open(AudioFormat{}), qPrintable(speakers->errorString()));
        QCOMPARE(link.opens.last().bufferFrames, 256);
        const quint32 serial = link.opens.last().serial;
        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Running,
                                     session.bufferFrames(), session.sampleRate()));
        QVERIFY(log.kinds.isEmpty());

        AsioDriverCaps changed = driverCaps(kFocusrite, 4, 2);
        changed.preferredBufferFrames = 512;
        driver.setCaps(changed);
        driver.fireMessage(AsioMessage::ResetRequest);
        QTRY_COMPARE(restarted.count(), 1);
        QCOMPARE(session.bufferFrames(), 512);

        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Restarted,
                                     session.bufferFrames(), session.sampleRate()));
        QCOMPARE(log.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::FormatChanged});
        QCOMPARE(backend->sessionBufferFrames(), 512);
        // The bus the stream supervisor opens again runs at the driver's size.
        auto again = backend->createOutput(outputOn(kFocusrite, 1));
        QVERIFY2(again->open(AudioFormat{}), qPrintable(again->errorString()));
        QCOMPARE(link.opens.last().bufferFrames, 512);
    }

    void resetThatCannotReloadFails()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        QSignalSpy failed(session.notifier(), &AsioSessionNotifier::failed);
        QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        driver.setHeldByAnotherProgram(kFocusrite, true);
        driver.fireMessage(AsioMessage::ResetRequest);
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!session.isOpen());
        QCOMPARE(session.lastFailure(), AsioDriverFailure::InUse);
    }

    // ── Busy (R-AUD-11) ────────────────────────────────────────────────────

    void heldDriverIsInUse()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        driver.setHeldByAnotherProgram(kFocusrite, true);
        AsioSession session(driver);
        QVERIFY(!session.open(kFocusrite, 256, 48000.0, focusriteUses()));
        QCOMPARE(session.lastFailure(), AsioDriverFailure::InUse);
        QVERIFY(!driver.loaded());
    }

    // ── The buffer switch ──────────────────────────────────────────────────

    void bufferSwitchMovesEveryUse()
    {
        constexpr int kFrames = 256;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        auto speakers = constantMatcher(kFrames);
        auto headphones = constantMatcher(kFrames);
        MatcherReader speakersReader = speakers->makeReader();
        MatcherReader headphonesReader = headphones->makeReader();
        RecordingSink vax;
        QList<AsioEndpoint> endpoints(3);
        endpoints[0].reader = &speakersReader;
        endpoints[1].reader = &headphonesReader;
        endpoints[2].sink = &vax;
        endpoints[2].pick = MicChannelPick::Both;
        QVERIFY(session.open(kFocusrite, kFrames, 48000.0, focusriteUses(), endpoints));

        // The driver's input half 1 holds 0.5 on input 1 and 0.25 on input 2.
        for (int f = 0; f < kFrames; ++f) {
            static_cast<float*>(driver.buffer(AudioDeviceDirection::Input, 0, 1))[f] = 0.5f;
            static_cast<float*>(driver.buffer(AudioDeviceDirection::Input, 1, 1))[f] = 0.25f;
        }
        // The DSP side keeps up: one switch's frames before each switch.
        for (int i = 0; i < 8; ++i) {
            feedConstant(*speakers, 0.25f, -0.5f, kFrames);
            feedConstant(*headphones, 0.125f, 0.75f, kFrames);
            driver.fireBufferSwitch(i & 1);
        }
        // Half 1 was written last: every use on its own pair, interleaved
        // false, none over another's.
        const int last = kFrames - 1;
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 2, 1), last), 0.25f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 3, 1), last), -0.5f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 0, 1), last), 0.125f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 1, 1), last), 0.75f);
        // The input use's stereo went to its sink, once per switch.
        QCOMPARE(vax.m_posts.load(), 8);
        QCOMPARE(vax.m_frames.load(), kFrames);
        QCOMPARE(vax.m_rate.load(), 48000);
        QCOMPARE(vax.m_stereo[0], 0.75f);
        QCOMPARE(vax.m_stereo[1], 0.75f);
    }

    // R-AUD-07: speakers and headphones on the same pair play together.
    // Every channel in use starts at silence each switch and each use adds
    // in; a use with no reader leaves its channels silent.
    void outputsOnOnePairAddTogether()
    {
        constexpr int kFrames = 256;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        auto speakers = constantMatcher(kFrames);
        auto headphones = constantMatcher(kFrames);
        MatcherReader speakersReader = speakers->makeReader();
        MatcherReader headphonesReader = headphones->makeReader();
        const QList<AsioUse> uses = {
            use(AudioRole::Speakers, kFocusrite, 1, AudioDeviceDirection::Output),
            use(AudioRole::Headphones, kFocusrite, 1, AudioDeviceDirection::Output),
            use(AudioRole::Vax1, kFocusrite, 3, AudioDeviceDirection::Output)};
        QList<AsioEndpoint> endpoints(3);
        endpoints[0].reader = &speakersReader;
        endpoints[1].reader = &headphonesReader;   // endpoints[2]: no reader
        QVERIFY(session.open(kFocusrite, kFrames, 48000.0, uses, endpoints));
        QCOMPARE(driver.lastOutputChannels(), (QList<int>{0, 1, 2, 3}));
        const int last = kFrames - 1;

        // Something left in the third pair's buffers is overwritten with silence.
        for (int half = 0; half < 2; ++half) {
            for (int channel = 2; channel < 4; ++channel) {
                for (int f = 0; f < kFrames; ++f) {
                    static_cast<float*>(driver.buffer(AudioDeviceDirection::Output, channel, half))[f] = 0.9f;
                }
            }
        }
        for (int i = 0; i < 8; ++i) {
            feedConstant(*speakers, 0.25f, -0.5f, kFrames);
            feedConstant(*headphones, 0.125f, 0.75f, kFrames);
            driver.fireBufferSwitch(i & 1);
        }
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 0, 1), last), 0.375f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 1, 1), last), 0.25f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 2, 1), last), 0.0f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 3, 1), last), 0.0f);

        // One muted (silence from its matcher): the other is still heard, alone.
        for (int i = 0; i < 8; ++i) {
            feedConstant(*speakers, 0.0f, 0.0f, kFrames);
            feedConstant(*headphones, 0.125f, 0.75f, kFrames);
            driver.fireBufferSwitch(i & 1);
        }
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 0, 1), last), 0.125f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 1, 1), last), 0.75f);
        for (int i = 0; i < 8; ++i) {
            feedConstant(*speakers, 0.25f, -0.5f, kFrames);
            feedConstant(*headphones, 0.0f, 0.0f, kFrames);
            driver.fireBufferSwitch(i & 1);
        }
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 0, 1), last), 0.25f);
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 1, 1), last), -0.5f);
    }

    // A one-channel output (the last odd output) gets left plus right,
    // halved, added to any other use on it.
    void oneChannelOutputFoldsIntoTheMix()
    {
        constexpr int kFrames = 256;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 3, 2));
        AsioSession session(driver);
        auto speakers = constantMatcher(kFrames);
        auto headphones = constantMatcher(kFrames);
        MatcherReader speakersReader = speakers->makeReader();
        MatcherReader headphonesReader = headphones->makeReader();
        const QList<AsioUse> uses = {
            AsioUse{AudioRole::Speakers, kFocusrite, AudioChannelPair{3, 1}, AudioDeviceDirection::Output},
            AsioUse{AudioRole::Headphones, kFocusrite, AudioChannelPair{3, 1}, AudioDeviceDirection::Output}};
        QList<AsioEndpoint> endpoints(2);
        endpoints[0].reader = &speakersReader;
        endpoints[1].reader = &headphonesReader;
        QVERIFY(session.open(kFocusrite, kFrames, 48000.0, uses, endpoints));
        for (int i = 0; i < 8; ++i) {
            feedConstant(*speakers, 0.25f, -0.5f, kFrames);    // folds to -0.125
            feedConstant(*headphones, 0.5f, 0.25f, kFrames);   // folds to 0.375
            driver.fireBufferSwitch(i & 1);
        }
        QCOMPARE(floatAt(driver.buffer(AudioDeviceDirection::Output, 2, 1), kFrames - 1), 0.25f);
    }

    // R-AUD-07 with ASIO: a mic pair that starts on the driver's last input
    // runs on that one channel, as the native engines open it; a pair not
    // on the driver at all is dropped, and runningPair() says so.
    void micOnTheLastInputRunsOnOneChannel_data()
    {
        QTest::addColumn<int>("inputs");
        QTest::newRow("third of three inputs") << 3;
        QTest::newRow("a one-input driver") << 1;
    }

    void micOnTheLastInputRunsOnOneChannel()
    {
        QFETCH(int, inputs);
        constexpr int kFrames = 256;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 2, inputs));
        AsioSession session(driver);
        RecordingSink mic;
        QList<AsioEndpoint> endpoints(1);
        endpoints[0].sink = &mic;
        endpoints[0].pick = MicChannelPick::Both;
        const QList<AsioUse> uses = {use(AudioRole::TxInput, kFocusrite, inputs, AudioDeviceDirection::Input)};
        QVERIFY(session.open(kFocusrite, kFrames, 48000.0, uses, endpoints));
        QCOMPARE(driver.lastInputChannels(), QList<int>{inputs - 1});
        QCOMPARE(session.runningPair(0), std::optional<AudioChannelPair>(AudioChannelPair{inputs, 1}));
        for (int f = 0; f < kFrames; ++f) {
            static_cast<float*>(driver.buffer(AudioDeviceDirection::Input, inputs - 1, 0))[f] = 0.5f;
        }
        driver.fireBufferSwitch(0);
        QCOMPARE(mic.m_posts.load(), 1);
        QCOMPARE(mic.m_stereo[0], 0.5f);   // the one channel, heard once on both sides
        QCOMPARE(mic.m_stereo[1], 0.5f);
    }

    void useNotOnTheDriverIsDropped()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 2, 2));
        AsioSession session(driver);
        RecordingSink mic;
        QList<AsioEndpoint> endpoints(3);
        endpoints[2].sink = &mic;
        const QList<AsioUse> uses = {
            use(AudioRole::Speakers, kFocusrite, 1, AudioDeviceDirection::Output),
            use(AudioRole::Headphones, kFocusrite, 3, AudioDeviceDirection::Output),
            use(AudioRole::TxInput, kFocusrite, 5, AudioDeviceDirection::Input)};
        // The outputs run; only the speakers' pair is on the driver.
        QVERIFY(session.open(kFocusrite, 256, 48000.0, uses, endpoints));
        QCOMPARE(session.runningPair(0), std::optional<AudioChannelPair>(AudioChannelPair{1, 2}));
        QVERIFY(!session.runningPair(1).has_value());
        QVERIFY(!session.runningPair(2).has_value());
        QVERIFY(!session.runningPair(3).has_value());
        QVERIFY(driver.lastInputChannels().isEmpty());
        driver.fireBufferSwitch(0);
        QCOMPARE(mic.m_posts.load(), 0);
        session.close();
        QVERIFY(!session.runningPair(0).has_value());
    }

    void callbacksGoThroughTheOneSessionPointer()
    {
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        RecordingSink vax;
        QList<AsioEndpoint> endpoints(3);
        endpoints[2].sink = &vax;
        {
            AsioSession session(driver);
            QVERIFY(session.open(kFocusrite, 256, 48000.0, focusriteUses(), endpoints));
            driver.fireBufferSwitch(0);
            QCOMPARE(vax.m_posts.load(), 1);
            session.close();
            driver.fireBufferSwitch(0);           // no session: nothing runs
            QCOMPARE(vax.m_posts.load(), 1);
        }
        driver.fireBufferSwitch(1);
        driver.fireMessage(AsioMessage::ResetRequest);
        QCOMPARE(vax.m_posts.load(), 1);
    }

    void bufferSwitchTakesNoAllocation()
    {
        constexpr int kFrames = 512;
        FakeAsioDriver driver;
        driver.addDriver(driverCaps(kFocusrite, 4, 2));
        AsioSession session(driver);
        auto speakers = constantMatcher(kFrames);
        auto headphones = constantMatcher(kFrames);
        MatcherReader speakersReader = speakers->makeReader();
        MatcherReader headphonesReader = headphones->makeReader();
        RecordingSink vax;
        QList<AsioEndpoint> endpoints(3);
        endpoints[0].reader = &speakersReader;
        endpoints[1].reader = &headphonesReader;
        endpoints[2].sink = &vax;
        QVERIFY(session.open(kFocusrite, kFrames, 48000.0, focusriteUses(), endpoints));

        std::uint64_t allocations = 0;
        std::uint64_t frees = 0;
        std::thread callbackThread([&]() {
            g_watchedId = std::this_thread::get_id();
            g_newCalls.store(0);
            g_deleteCalls.store(0);
            // Only the buffer switch is watched; the DSP side's writes
            // between switches keep the readers playing real frames.
            for (int i = 0; i < 200; ++i) {
                feedConstant(*speakers, 0.25f, -0.5f, kFrames);
                feedConstant(*headphones, 0.125f, 0.75f, kFrames);
                g_watching.store(true, std::memory_order_release);
                driver.fireBufferSwitch(i & 1);
                g_watching.store(false, std::memory_order_release);
            }
            allocations = g_newCalls.load();
            frees = g_deleteCalls.load();
        });
        callbackThread.join();
        QCOMPARE(vax.m_posts.load(), 200);
        QCOMPARE(allocations, std::uint64_t(0));
        QCOMPARE(frees, std::uint64_t(0));
    }

    // The hook above can fail: an allocation on the watched thread counts.
    void allocationHookCountsAnAllocation()
    {
        std::uint64_t allocations = 0;
        std::thread watched([&allocations]() {
            g_watchedId = std::this_thread::get_id();
            g_newCalls.store(0);
            g_watching.store(true, std::memory_order_release);
            int* volatile probe = new int(7);
            delete probe;
            g_watching.store(false, std::memory_order_release);
            allocations = g_newCalls.load();
        });
        watched.join();
        QCOMPARE(allocations, std::uint64_t(1));
    }

    // ── The window's AsioBackend (R-AUD-19, R-AUD-11, R-AUD-32) ────────────

    void backendIsBarredInATestRun()
    {
        QStandardPaths::setTestModeEnabled(true);
        FakeLink link;
        AsioBackend backend;
        backend.setHelperLink(link.make());
        const bool running = backend.running();
        const bool listed = !backend.enumerate().isEmpty();
        auto bus = backend.createOutput(outputOn(kFocusrite, 1));
        const bool opened = bus->open(AudioFormat{});
        QStandardPaths::setTestModeEnabled(false);
        QVERIFY(!running);
        QVERIFY(!listed);
        QVERIFY(!opened);
        QCOMPARE(bus->errorString(), asioTestRunError());
        QCOMPARE(link.describes, 0);
        QVERIFY(link.opens.isEmpty());
        QVERIFY(link.demands.isEmpty());
    }

    void backendWithoutTheHelperOpensNothing()
    {
        AsioBackend backend;
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        auto bus = backend.createOutput(outputOn(kFocusrite, 1));
        QVERIFY(!bus->open(AudioFormat{}));
        QVERIFY(!bus->openRefusedInUse());
        QVERIFY(backend.createInput(outputOn(kFocusrite, 1), MicChannelPick::Both, nullptr) == nullptr);
    }

    void backendListsTheDescribedDrivers()
    {
        FakeLink link;
        AsioBackend backend;
        int notices = 0;
        backend.setNoticeSink([&notices](AudioNotice) { ++notices; });
        backend.setHelperLink(link.make());
        QVERIFY(backend.running());
        QCOMPARE(backend.id(), AudioBackendId::Asio);
        QVERIFY(backend.hasControlPanel());
        QVERIFY(backend.opensOneStreamAtATime());
        QVERIFY(!backend.defaultDeviceId(AudioDeviceDirection::Output).has_value());

        QVERIFY(backend.enumerate().isEmpty());
        QCOMPARE(link.describedDrivers, QStringList{QString()});
        QVERIFY(backend.enumerate().isEmpty());
        QCOMPARE(link.describes, 1);                       // asked once

        notices = 0;
        CaptureProtocol::AsioCapsRecord list;
        list.drivers = {kFocusrite, kMotu};
        backend.onAsioCaps(list);
        QCOMPARE(notices, 1);
        backend.onAsioCaps(list);
        QCOMPARE(notices, 1);                              // no change, no notice

        QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(link.describedDrivers, (QStringList{QString(), kFocusrite, kMotu}));
        QCOMPARE(devices.size(), 4);
        backend.enumerate();
        QCOMPARE(link.describes, 3);                       // each driver once

        CaptureProtocol::AsioCapsRecord focusrite;
        focusrite.drivers = list.drivers;
        focusrite.driver = kFocusrite;
        focusrite.caps = driverCaps(kFocusrite, 4, 2);
        backend.onAsioCaps(focusrite);
        QCOMPARE(notices, 2);
        devices = backend.enumerate();
        QCOMPARE(devices.size(), 4);
        QCOMPARE(devices[0].backend, AudioBackendId::Asio);
        QCOMPARE(devices[0].id, kFocusrite);
        QCOMPARE(devices[0].name, kFocusrite);
        QCOMPARE(devices[0].direction, AudioDeviceDirection::Output);
        QCOMPARE(devices[0].channelCount, 4);
        QCOMPARE(devices[1].direction, AudioDeviceDirection::Input);
        QCOMPARE(devices[1].channelCount, 2);
        QCOMPARE(devices[2].id, kMotu);
        QCOMPARE(devices[2].channelCount, 2);              // not described yet

        // A held driver lists as in use.
        CaptureProtocol::AsioCapsRecord held;
        held.drivers = list.drivers;
        held.driver = kMotu;
        held.inUse = true;
        backend.onAsioCaps(held);
        QVERIFY(backend.driverInUse(kMotu));
        devices = backend.enumerate();
        QCOMPARE(devices[2].state, AudioDeviceState::InUse);
        QCOMPARE(devices[0].state, AudioDeviceState::Present);
        QCOMPARE(link.describedDrivers.last(), kMotu);      // asked again, it may be free now

        backend.rescan();
        QVERIFY(!backend.driverCaps(kFocusrite).has_value());
        QVERIFY(!backend.driverInUse(kMotu));
        // A rescan asks again: the list, then each driver it still shows.
        const int before = link.describes;
        devices = backend.enumerate();
        QCOMPARE(devices.size(), 4);
        QCOMPARE(link.describes, before + 3);
        QCOMPARE(link.describedDrivers.mid(before), (QStringList{QString(), kFocusrite, kMotu}));
    }

    void backendSendsEveryUseOfTheSession()
    {
        FakeLink link;
        auto backend = linkedBackend(link);
        auto speakers = backend->createOutput(outputOn(kFocusrite, 3));
        QVERIFY(speakers->takesStereoMix());
        QVERIFY(speakers->open(AudioFormat{}));
        QCOMPARE(link.demands, QList<bool>{true});
        QCOMPARE(link.opens.size(), 1);
        const CaptureProtocol::AsioOpen first = link.opens.last();
        QCOMPARE(first.driver, kFocusrite);
        QCOMPARE(first.bufferFrames, 256);                 // the driver's preferred
        QCOMPARE(first.rate, 48000.0);
        QCOMPARE(first.uses.size(), 1);
        QCOMPARE(first.uses[0].pair, (AudioChannelPair{3, 2}));
        QCOMPARE(first.uses[0].direction, AudioDeviceDirection::Output);
        QVERIFY(!CaptureProtocol::encodeAsioOpen(first).isEmpty());
        QCOMPARE(backend->sessionDriver(), kFocusrite);

        // The helper's side attaches the ring the bus writes and reads it;
        // the bus's pacing sees what the reader asked for.
        const CaptureShmNames names{first.uses[0].memory, first.uses[0].wake};
        auto region = CaptureShmRegion::attach(names, static_cast<std::size_t>(first.uses[0].bytes));
        QVERIFY(region);
        MatcherRingHeader* ring =
            attachMatcherRing(region->data(), static_cast<std::size_t>(first.uses[0].bytes));
        QVERIFY(ring != nullptr);
        MatcherReader reader(ring);
        QVERIFY(reader.valid());
        std::vector<float> block(64 * 2, 0.25f);
        for (int i = 0; i < 32; ++i) {
            speakers->push(reinterpret_cast<const char*>(block.data()),
                           static_cast<qint64>(block.size() * sizeof(float)));
        }
        std::vector<float> out(256 * 2, 0.0f);
        reader.read(out.data(), 256);
        QCOMPARE(speakers->outputPacing()->consumedFrames, quint64(256));
        QCOMPARE(speakers->outputPacing()->callbackFrames, 256);

        // A second use joins the same session, both listed, one demand.
        auto headphones = backend->createOutput(outputOn(kFocusrite, 1));
        QVERIFY(headphones->open(AudioFormat{}));
        QCOMPARE(link.opens.size(), 2);
        QVERIFY(link.opens.last().serial > first.serial);
        QCOMPARE(link.opens.last().uses.size(), 2);
        QCOMPARE(link.opens.last().uses[1].pair, (AudioChannelPair{1, 2}));
        QCOMPARE(link.demands, QList<bool>{true});

        // One driver at a time; a pair the driver lacks is refused.
        auto other = backend->createOutput(outputOn(kMotu, 1));
        QVERIFY(!other->open(AudioFormat{}));
        QVERIFY(!other->openRefusedInUse());
        auto beyond = backend->createOutput(outputOn(kFocusrite, 5));
        QVERIFY(!beyond->open(AudioFormat{}));
        QCOMPARE(link.opens.size(), 2);

        // The control panel opens for the session's driver only.
        backend->openControlPanel(kMotu);
        QCOMPARE(link.controlPanels, 0);
        backend->openControlPanel(kFocusrite);
        QCOMPARE(link.controlPanels, 1);

        headphones->close();
        QCOMPARE(link.opens.size(), 3);
        QCOMPARE(link.opens.last().uses.size(), 1);
        QCOMPARE(link.demands, QList<bool>{true});
        speakers->close();
        QCOMPARE(link.opens.size(), 4);
        QVERIFY(link.opens.last().uses.isEmpty());
        QVERIFY(link.opens.last().driver.isEmpty());
        QCOMPARE(link.demands, (QList<bool>{true, false}));
        QVERIFY(backend->sessionDriver().isEmpty());
    }

    // Native audio plan Task 17: each use names the role it plays, so the
    // helper no longer reads every output as the speakers; the session's
    // buffer and rate, and the control panel while no output runs.
    void backendNamesEachUsesRole()
    {
        FakeLink link;
        auto backend = linkedBackend(link);
        backend->openControlPanel(kFocusrite);   // no output: the helper decides
        QCOMPARE(link.controlPanels, 1);
        QCOMPARE(backend->sessionBufferFrames(), 0);
        AudioStreamRequest headphones = outputOn(kFocusrite, 3);
        headphones.role = AudioRole::Headphones;
        auto bus = backend->createOutput(headphones);
        QVERIFY(bus->open(AudioFormat{}));
        AudioStreamRequest vax = outputOn(kFocusrite, 1);
        vax.role = AudioRole::Vax2;
        auto second = backend->createOutput(vax);
        QVERIFY(second->open(AudioFormat{}));
        const CaptureProtocol::AsioOpen open = link.opens.last();
        QCOMPARE(open.uses.size(), 2);
        QCOMPARE(open.uses[0].role, std::optional<AudioRole>(AudioRole::Headphones));
        QCOMPARE(open.uses[1].role, std::optional<AudioRole>(AudioRole::Vax2));
        // tst_capture_protocol's asioOpenRoundTrip carries the roles over the wire.
        QVERIFY(!CaptureProtocol::encodeAsioOpen(open).isEmpty());
        backend->onAsioState(stateOf(open.serial, CaptureProtocol::AsioStateKind::Running, 512, 96000.0));
        QCOMPARE(backend->sessionBufferFrames(), 512);
        QCOMPARE(backend->sessionRate(), 96000.0);
        second->close();
        bus->close();
        QCOMPARE(backend->sessionBufferFrames(), 0);
        QCOMPARE(backend->sessionRate(), 0.0);
    }

    void backendMapsInUseToEveryRoleOnTheDriver()
    {
        FakeLink link;
        auto backend = linkedBackend(link);
        EventLog speakersLog;
        EventLog headphonesLog;
        auto speakers = backend->createOutput(outputOn(kFocusrite, 3));
        auto headphones = backend->createOutput(outputOn(kFocusrite, 1));
        speakers->setStreamEventSink(speakersLog.sink());
        headphones->setStreamEventSink(headphonesLog.sink());
        QVERIFY(speakers->open(AudioFormat{}));
        QVERIFY(headphones->open(AudioFormat{}));
        const quint32 serial = link.opens.last().serial;

        // An answer to an older open is ignored.
        backend->onAsioState(stateOf(serial - 1, CaptureProtocol::AsioStateKind::InUse));
        QVERIFY(speakersLog.kinds.isEmpty());

        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::InUse));
        QCOMPARE(speakersLog.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::DeviceBusy});
        QCOMPARE(headphonesLog.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::DeviceBusy});
        QVERIFY(backend->driverInUse(kFocusrite));
        speakers->close();
        headphones->close();

        // The next open on the held driver reads in use (R-AUD-11).
        auto again = backend->createOutput(outputOn(kFocusrite, 3));
        QVERIFY(!again->open(AudioFormat{}));
        QVERIFY(again->openRefusedInUse());
        for (const AudioDeviceInfo& device : backend->enumerate()) {
            if (device.id == kFocusrite) {
                QCOMPARE(device.state, AudioDeviceState::InUse);
            }
        }
    }

    void backendReopensOnANewBufferOrRate()
    {
        FakeLink link;
        auto backend = linkedBackend(link);
        EventLog log;
        auto speakers = backend->createOutput(outputOn(kFocusrite, 3));
        speakers->setStreamEventSink(log.sink());
        QVERIFY(speakers->open(AudioFormat{}));
        const quint32 serial = link.opens.last().serial;

        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Running));
        QVERIFY(log.kinds.isEmpty());                       // what the matcher was built for
        QCOMPARE(speakers->outputPacing()->deviceLatencyNs, std::optional<qint64>(10'000'000));

        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Restarted, 512));
        QCOMPARE(log.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::FormatChanged});
        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Restarted, 256, 96000.0));
        QCOMPARE(log.kinds.size(), 2);
        QCOMPARE(log.kinds.last(), AudioStreamEvent::Kind::FormatChanged);

        backend->onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Failed));
        QCOMPARE(log.kinds.last(), AudioStreamEvent::Kind::DeviceLost);

        // The session's values reach the next open (R-AUD-20).
        auto headphones = backend->createOutput(outputOn(kFocusrite, 1));
        QVERIFY(headphones->open(AudioFormat{}));
        QCOMPARE(link.opens.last().bufferFrames, 256);
        QCOMPARE(link.opens.last().rate, 96000.0);
    }

    // A saved Outputs 3-4 opened before the driver's caps came, on a driver
    // with two outputs: the helper drops that use and sends the caps with
    // its running state; the bus is lost with the reason, the other plays.
    void backendLosesABusWhosePairTheDriverLacks()
    {
        const BackendOpensInThisTest opens;
        FakeLink link;
        AsioBackend backend;
        backend.setPreferencesSource([] { return AsioSessionPreferences{}; });
        backend.setHelperLink(link.make());
        CaptureProtocol::AsioCapsRecord list;
        list.drivers = {kFocusrite};
        backend.onAsioCaps(list);
        EventLog beyondLog;
        EventLog firstLog;
        auto beyond = backend.createOutput(outputOn(kFocusrite, 3));
        auto first = backend.createOutput(outputOn(kFocusrite, 1));
        beyond->setStreamEventSink(beyondLog.sink());
        first->setStreamEventSink(firstLog.sink());
        QVERIFY(beyond->open(AudioFormat{}));              // caps not known: allowed
        QVERIFY(first->open(AudioFormat{}));
        const quint32 serial = link.opens.last().serial;

        CaptureProtocol::AsioCapsRecord described;
        described.drivers = list.drivers;
        described.driver = kFocusrite;
        described.caps = driverCaps(kFocusrite, 2, 2);
        backend.onAsioCaps(described);
        backend.onAsioState(stateOf(serial, CaptureProtocol::AsioStateKind::Running));
        QCOMPARE(beyondLog.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::DeviceLost});
        QVERIFY(firstLog.kinds.isEmpty());

        // Its reopen is refused with the reason.
        beyond->close();
        QVERIFY(!beyond->open(AudioFormat{}));
        QCOMPARE(beyond->errorString(), QStringLiteral("The chosen channels are not on %1").arg(kFocusrite));
    }

    void backendLosesItsOutputsWithTheHelper()
    {
        FakeLink link;
        auto backend = linkedBackend(link);
        EventLog log;
        auto speakers = backend->createOutput(outputOn(kFocusrite, 3));
        speakers->setStreamEventSink(log.sink());
        QVERIFY(speakers->open(AudioFormat{}));
        backend->setHelperLink(AsioHelperLink{});
        QCOMPARE(log.kinds, QList<AudioStreamEvent::Kind>{AudioStreamEvent::Kind::DeviceLost});
        QVERIFY(!backend->running());
        const int opens = link.opens.size();
        speakers->close();
        QCOMPARE(link.opens.size(), opens);                 // nothing sent without the helper
    }
};

QTEST_MAIN(TestAsioSession)
#include "tst_asio_session.moc"
