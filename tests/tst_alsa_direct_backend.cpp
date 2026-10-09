// =================================================================
// tests/tst_alsa_direct_backend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.  The ALSA direct engine on the
// Linux Core (native audio plan Task 12: R-AUD-01, R-AUD-11, R-AUD-25,
// R-AUD-30, R-AUD-32; settled calls 9, 10, 13, 14 and 33).
//
// Every case runs on fakes: a fake system (cards, the configured default,
// notices, desktop target) and a fake PCM behind the stream's opener.  The
// inotify watcher runs on a temporary directory.  No card is opened; the
// real adapter and the real opener are checked only for the test-run
// barrier.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-11, R-AUD-25,
//               R-AUD-30, R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/AlsaDirectBackend.h"
#include "core/audio/AlsaDirectBus.h"
#include "core/audio/AlsaDirectSystem.h"
#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/AudioDeviceCatalog.h"
#include "core/audio/AudioDeviceMatching.h"
#include "core/audio/AudioTestBarrier.h"
#include "fakes/FakeMatcherAudioBus.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kWaitMs = 5000;

AlsaCardRecord card(int number, const QString& id, const QString& name, int device = 0,
                    const QString& bus = QString())
{
    AlsaCardRecord r;
    r.card = number;
    r.cardId = id;
    r.cardName = name;
    r.device = device;
    r.bus = bus;
    return r;
}

AlsaCardRecord headphones()
{
    return card(0, QStringLiteral("Headphones"), QStringLiteral("bcm2835 Headphones"));
}

AlsaCardRecord usbCard()
{
    return card(2, QStringLiteral("Device"), QStringLiteral("USB Audio Device"), 0,
                QStringLiteral("usb"));
}

// The PCM a fake opener hands out.  Scripted results come first; with
// none queued a paced PCM accepts after the frames' real time (a fake
// device clock), and an unpaced one waits for a result or a drop.
struct FakePcmState {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<long> script;   // >= 0: accept; < 0: return that error
    bool paced = false;
    bool dropped = false;
    AlsaPcmSetup setup;
    long delay = 384;
    std::atomic<int> writes{0};
    std::atomic<long> framesWritten{0};
    std::atomic<int> prepares{0};
    std::atomic<int> resumes{0};
    std::atomic<int> drops{0};
    std::atomic<bool> destroyed{false};

    void queue(std::initializer_list<long> results)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            script.insert(script.end(), results);
        }
        cv.notify_all();
    }
    void queueAccepts(int count)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < count; ++i) {
            script.push_back(0);
        }
        cv.notify_all();
    }
    int queued()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return int(script.size());
    }
};

class FakePcm final : public IAlsaPcm {
public:
    explicit FakePcm(std::shared_ptr<FakePcmState> state) : m_state(std::move(state)) {}
    ~FakePcm() override { m_state->destroyed.store(true); }

    AlsaPcmSetup setup() const override { return m_state->setup; }
    long writei(const void*, long frames) override
    {
        FakePcmState& s = *m_state;
        s.writes.fetch_add(1);
        std::unique_lock<std::mutex> lock(s.mutex);
        for (;;) {
            if (s.dropped) {
                return -EBADFD;
            }
            if (!s.script.empty()) {
                const long result = s.script.front();
                s.script.pop_front();
                if (result >= 0) {
                    s.framesWritten.fetch_add(frames);
                    return frames;
                }
                return result;
            }
            if (s.paced) {
                const auto period = std::chrono::microseconds(
                    std::int64_t(frames) * 1000000 / std::max(1, s.setup.rate));
                if (s.cv.wait_for(lock, period, [&s] { return s.dropped || !s.script.empty(); })) {
                    continue;
                }
                s.framesWritten.fetch_add(frames);
                return frames;
            }
            s.cv.wait(lock, [&s] { return s.dropped || !s.script.empty(); });
        }
    }
    int prepare() override
    {
        m_state->prepares.fetch_add(1);
        return 0;
    }
    int resume() override
    {
        m_state->resumes.fetch_add(1);
        return -ENOSYS;
    }
    long delayFrames() override { return m_state->delay; }
    void drop() override
    {
        m_state->drops.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->dropped = true;
        }
        m_state->cv.notify_all();
    }

private:
    std::shared_ptr<FakePcmState> m_state;
};

// An opener over fake PCMs: each open makes a new state (paced or not),
// unless the PCM name is set to fail with an error.
struct FakeOpener {
    struct Call {
        QString name;
        AlsaPcmRequest request;
    };
    std::mutex mutex;
    std::vector<Call> calls;
    std::vector<std::shared_ptr<FakePcmState>> states;
    QHash<QString, int> failing;   // pcm name -> -errno
    bool paced = false;
    int prefill = 0;               // accepts queued on each new PCM

    AlsaPcmOpener opener()
    {
        return [this](const QString& name, const AlsaPcmRequest& request) {
            std::lock_guard<std::mutex> lock(mutex);
            calls.push_back({name, request});
            AlsaPcmOpenResult result;
            if (failing.contains(name)) {
                result.error = failing.value(name);
                result.detail = QStringLiteral("fake failure");
                return result;
            }
            auto state = std::make_shared<FakePcmState>();
            state->paced = paced;
            state->setup.format = DeviceSampleFormat::Int16;
            state->setup.rate = request.rate;
            state->setup.channels = request.channels;
            state->setup.periodFrames = request.periodFrames;
            state->setup.bufferFrames = request.periodFrames * request.periods;
            state->delay = state->setup.bufferFrames;
            for (int i = 0; i < prefill; ++i) {
                state->script.push_back(0);
            }
            states.push_back(state);
            result.pcm = std::make_unique<FakePcm>(state);
            return result;
        };
    }

    std::shared_ptr<FakePcmState> last()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return states.empty() ? nullptr : states.back();
    }
    std::shared_ptr<FakePcmState> lastFor(const QString& name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t i = calls.size(), s = states.size(); i > 0; --i) {
            if (!failing.contains(calls[i - 1].name)) {
                if (s == 0) {
                    return nullptr;
                }
                --s;
                if (calls[i - 1].name == name) {
                    return states[s];
                }
            }
        }
        return nullptr;
    }
    int openCount(const QString& name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        int n = 0;
        for (const Call& call : calls) {
            n += call.name == name ? 1 : 0;
        }
        return n;
    }
};

// The system seam's fake.  createOutput hands out FakeMatcherAudioBus, or
// real AlsaDirectBus streams over a FakeOpener when one is given.
class FakeAlsaSystem final : public IAlsaDirectSystem {
public:
    struct OutputCall {
        AlsaCardRecord card;
        AudioStreamRequest request;
    };

    QList<AlsaCardRecord> playbackCards() override
    {
        std::lock_guard<std::mutex> lock(mutex);
        return records;
    }
    std::optional<int> configuredDefaultCard() override { return configured; }
    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        noticeSink = std::move(sink);
    }
    std::unique_ptr<IAudioBus> createOutput(const AlsaCardRecord& c,
                                            const AudioStreamRequest& request) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            outputs.push_back({c, request});
        }
        if (pcmOpener) {
            return std::make_unique<AlsaDirectBus>(c, request, pcmOpener->opener());
        }
        return std::make_unique<FakeMatcherAudioBus>(request);
    }
    QString defaultTargetPath() override { return defaultTarget; }

    void setRecords(QList<AlsaCardRecord> list)
    {
        std::lock_guard<std::mutex> lock(mutex);
        records = std::move(list);
    }
    void post(AudioNotice notice)
    {
        std::function<void(AudioNotice)> sink;
        {
            std::lock_guard<std::mutex> lock(mutex);
            sink = noticeSink;
        }
        if (sink) {
            sink(notice);
        }
    }

    std::mutex mutex;
    QList<AlsaCardRecord> records;
    std::optional<int> configured;
    std::function<void(AudioNotice)> noticeSink;
    std::vector<OutputCall> outputs;
    QString defaultTarget;
    FakeOpener* pcmOpener = nullptr;
};

class NullInputSink final : public IAudioInputSink {
public:
    void onInput(const float*, int, int, std::int64_t) override {}
};

struct NoticeLog {
    std::mutex mutex;
    QList<AudioNotice> notices;
    std::function<void(AudioNotice)> sink()
    {
        return [this](AudioNotice notice) {
            std::lock_guard<std::mutex> lock(mutex);
            notices.append(notice);
        };
    }
    int count()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return int(notices.size());
    }
};

struct EventLog {
    std::mutex mutex;
    QList<AudioStreamEvent::Kind> kinds;
    std::function<void(const AudioStreamEvent&)> sink()
    {
        return [this](const AudioStreamEvent& event) {
            std::lock_guard<std::mutex> lock(mutex);
            kinds.append(event.kind);
        };
    }
    QList<AudioStreamEvent::Kind> take()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return kinds;
    }
};

const AudioDeviceInfo* findId(const QList<AudioDeviceInfo>& devices, const QString& id)
{
    for (const AudioDeviceInfo& info : devices) {
        if (info.id == id) {
            return &info;
        }
    }
    return nullptr;
}

bool touch(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

AudioStreamRequest outputRequest(const QString& id = QString(), int bufferFrames = 0)
{
    AudioStreamRequest request;
    request.deviceId = id;
    request.bufferFrames = bufferFrames;
    return request;
}

void expectOpenWarnings()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("ALSA direct did not open")));
}

} // namespace

class TestAlsaDirectBackend : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(audioDevicesBarredForTestRun());
    }

    void init() { AppSettings::instance().clear(); }

    // Design choice 4, settled call 10: cards to outputs; ID card ID and
    // device, name the card's; Usb for the USB bus, Hdmi for vc4hdmi; the
    // default the configured card when it plays, else the lowest card.
    void cardsBecomeOutputs()
    {
        const QList<AlsaCardRecord> records{headphones(), usbCard()};
        const QList<AudioDeviceInfo> devices = alsaDevicesFromRecords(records, std::nullopt);
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.at(0).id, QStringLiteral("Headphones,0"));
        QCOMPARE(devices.at(0).name, QStringLiteral("bcm2835 Headphones"));
        QCOMPARE(devices.at(0).backend, AudioBackendId::AlsaDirect);
        QCOMPARE(devices.at(0).direction, AudioDeviceDirection::Output);
        QCOMPARE(devices.at(0).transport, AudioTransport::Unknown);
        QCOMPARE(devices.at(0).alsaCard, 0);
        QCOMPARE(devices.at(0).alsaDevice, 0);
        QVERIFY(devices.at(0).isDefault);
        QCOMPARE(devices.at(1).id, QStringLiteral("Device,0"));
        QCOMPARE(devices.at(1).name, QStringLiteral("USB Audio Device"));
        QCOMPARE(devices.at(1).transport, AudioTransport::Usb);
        QCOMPARE(devices.at(1).alsaCard, 2);
        QCOMPARE(devices.at(1).alsaDevice, 0);
        QVERIFY(!devices.at(1).isDefault);

        // defaults.pcm.card 2 makes the USB card the default.
        const QList<AudioDeviceInfo> configured = alsaDevicesFromRecords(records, 2);
        QVERIFY(!configured.at(0).isDefault);
        QVERIFY(configured.at(1).isDefault);
        // A configured card that has no playback falls to the lowest card.
        const QList<AudioDeviceInfo> missing = alsaDevicesFromRecords(records, 5);
        QVERIFY(missing.at(0).isDefault);
        QVERIFY(!missing.at(1).isDefault);
        // The lowest card wins whatever the list order.
        const QList<AudioDeviceInfo> reversed = alsaDevicesFromRecords({usbCard(), headphones()},
                                                                       std::nullopt);
        QVERIFY(findId(reversed, QStringLiteral("Headphones,0"))->isDefault);
        QVERIFY(!findId(reversed, QStringLiteral("Device,0"))->isDefault);

        // A Pi's HDMI card.
        const QList<AudioDeviceInfo> hdmi = alsaDevicesFromRecords(
            {card(1, QStringLiteral("vc4hdmi0"), QStringLiteral("vc4-hdmi-0"))}, std::nullopt);
        QCOMPARE(hdmi.size(), 1);
        QCOMPARE(hdmi.at(0).id, QStringLiteral("vc4hdmi0,0"));
        QCOMPARE(hdmi.at(0).transport, AudioTransport::Hdmi);
        QVERIFY(hdmi.at(0).isDefault);

        // A card with two playback devices: two outputs, the lower the default.
        const QList<AudioDeviceInfo> two = alsaDevicesFromRecords(
            {card(3, QStringLiteral("Multi"), QStringLiteral("Multi Card"), 1),
             card(3, QStringLiteral("Multi"), QStringLiteral("Multi Card"), 0)},
            std::nullopt);
        QCOMPARE(two.size(), 2);
        QVERIFY(!findId(two, QStringLiteral("Multi,1"))->isDefault);
        QVERIFY(findId(two, QStringLiteral("Multi,0"))->isDefault);

        // No cards: no outputs and no default.
        QVERIFY(alsaDevicesFromRecords({}, 0).isEmpty());
        QVERIFY(!alsaDefaultRecord({}, 0).has_value());
    }

    // The backend lists through the system; it always runs; no inputs.
    void backendListsThroughTheSystem()
    {
        auto system = std::make_shared<FakeAlsaSystem>();
        system->setRecords({headphones(), usbCard()});
        AlsaDirectBackend backend(system);
        QCOMPARE(backend.id(), AudioBackendId::AlsaDirect);
        QVERIFY(backend.running());
        QCOMPARE(backend.enumerate().size(), 2);
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Output), QStringLiteral("Headphones,0"));
        QVERIFY(!backend.defaultDeviceId(AudioDeviceDirection::Input).has_value());
        system->configured = 2;
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Output), QStringLiteral("Device,0"));
        system->setRecords({});
        QVERIFY(!backend.defaultDeviceId(AudioDeviceDirection::Output).has_value());

        NullInputSink sink;
        QVERIFY(backend.createInput(outputRequest(), MicChannelPick::Left, &sink) == nullptr);
        QVERIFY(!AlsaDirectBackend(nullptr).running());
    }

    // An empty id opens the default card; an id opens its card; an id
    // not listed opens nothing.
    void opensResolveTheirCard()
    {
        auto system = std::make_shared<FakeAlsaSystem>();
        system->setRecords({headphones(), usbCard()});
        AlsaDirectBackend backend(system);
        QVERIFY(backend.createOutput(outputRequest()) != nullptr);
        QCOMPARE(system->outputs.back().card, headphones());
        QVERIFY(backend.createOutput(outputRequest(QStringLiteral("Device,0"), 256)) != nullptr);
        QCOMPARE(system->outputs.back().card, usbCard());
        QCOMPARE(system->outputs.back().request.bufferFrames, 256);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("does not list the card")));
        QVERIFY(backend.createOutput(outputRequest(QStringLiteral("Gone,0"))) == nullptr);
        QCOMPARE(system->outputs.size(), std::size_t(2));
    }

    // Settled call 33: an old "(hw:C,D)" name matches the card by number.
    void oldHwNameMatchesTheCard()
    {
        AudioDeviceConfig saved;
        saved.engine = AudioEngineKind::AlsaDirect;
        saved.deviceName = QStringLiteral("USB Audio Device: - (hw:2,0)");
        const QList<AudioDeviceInfo> devices =
            alsaDevicesFromRecords({headphones(), usbCard()}, std::nullopt);
        const std::optional<AudioDeviceMatch> match = matchSavedAudioDevice(saved, devices);
        QVERIFY(match.has_value());
        QCOMPARE(match->device.id, QStringLiteral("Device,0"));
        QCOMPARE(match->device.alsaCard, 2);
        QCOMPARE(match->device.alsaDevice, 0);
    }

    // Notices: only a card's control node and a playback PCM count.
    void nodeFilter()
    {
        QVERIFY(alsaNodeIsWatched(QStringLiteral("controlC0")));
        QVERIFY(alsaNodeIsWatched(QStringLiteral("controlC12")));
        QVERIFY(alsaNodeIsWatched(QStringLiteral("pcmC2D0p")));
        QVERIFY(alsaNodeIsWatched(QStringLiteral("pcmC10D3p")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("pcmC2D0c")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("timer")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("seq")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("hwC0D0")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("controlC")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("controlC0x")));
        QVERIFY(!alsaNodeIsWatched(QStringLiteral("by-path")));
    }

    // The watcher on a real directory: a watched node created or deleted
    // posts DevicesChanged from its thread; other names post nothing.
    void watcherPostsForCardNodes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString snd = root.filePath(QStringLiteral("snd"));
        QVERIFY(QDir().mkpath(snd));
        NoticeLog log;
        AlsaNodeWatcher watcher(snd, log.sink());
        QVERIFY(watcher.start());
        QVERIFY(watcher.watchingDirectory());

        // Ignored names, then a watched one: one notice, for the batch
        // that holds the watched one.
        QVERIFY(touch(snd + QStringLiteral("/timer")));
        QVERIFY(touch(snd + QStringLiteral("/pcmC1D0c")));
        QVERIFY(touch(snd + QStringLiteral("/controlC1")));
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 1, kWaitMs);
        QVERIFY(touch(snd + QStringLiteral("/pcmC1D0p")));
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 2, kWaitMs);
        QVERIFY(QFile::remove(snd + QStringLiteral("/controlC1")));
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 3, kWaitMs);
        // A deleted ignored node, then a watched one: again exactly one more.
        QVERIFY(QFile::remove(snd + QStringLiteral("/timer")));
        QVERIFY(QFile::remove(snd + QStringLiteral("/pcmC1D0p")));
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 4, kWaitMs);
        watcher.stop();
        QVERIFY(!watcher.watchingDirectory());
        // Stopped: nothing more.
        QVERIFY(touch(snd + QStringLiteral("/controlC2")));
        QCOMPARE(log.count(), 4);
    }

    // No /dev/snd at boot: the watcher waits for it in its parent, then
    // watches it; when it is deleted it waits again.
    void watcherWaitsForTheDirectory()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString snd = root.filePath(QStringLiteral("snd"));
        NoticeLog log;
        AlsaNodeWatcher watcher(snd, log.sink());
        QVERIFY(watcher.start());
        QVERIFY(!watcher.watchingDirectory());
        QVERIFY(touch(root.filePath(QStringLiteral("other"))));
        QVERIFY(QDir().mkpath(snd));
        QTRY_VERIFY_WITH_TIMEOUT(watcher.watchingDirectory(), kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 1, kWaitMs);
        QVERIFY(touch(snd + QStringLiteral("/controlC0")));
        QTRY_COMPARE_WITH_TIMEOUT(log.count(), 2, kWaitMs);
        QVERIFY(QDir(snd).removeRecursively());
        QTRY_VERIFY_WITH_TIMEOUT(!watcher.watchingDirectory(), kWaitMs);
        QVERIFY(QDir().mkpath(snd));
        QTRY_VERIFY_WITH_TIMEOUT(watcher.watchingDirectory(), kWaitMs);
        QVERIFY(touch(snd + QStringLiteral("/controlC3")));
        const int before = log.count();
        QTRY_VERIFY_WITH_TIMEOUT(log.count() > before, kWaitMs);
    }

    // R-AUD-25: a card plugged in shows in the catalogue within the
    // debounce plus 100 ms of its notice, and goes the same way.
    void pluggedCardReachesTheCatalogue()
    {
        auto system = std::make_shared<FakeAlsaSystem>();
        system->setRecords({headphones()});
        auto backend = std::make_shared<AlsaDirectBackend>(system);
        AudioDeviceCatalog catalogue({backend});
        catalogue.start();
        QCOMPARE(catalogue.devices(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output).size(), 1);
        QVERIFY(catalogue.backendRunning(AudioBackendId::AlsaDirect));

        const int bound = AudioDeviceCatalog::kDebounceMs + 100;
        system->setRecords({headphones(), usbCard()});
        QElapsedTimer timer;
        timer.start();
        system->post(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(
            catalogue.devices(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output).size(), 2,
            bound);
        QVERIFY2(timer.elapsed() <= bound, qPrintable(QString::number(timer.elapsed())));
        const QList<AudioDeviceInfo> listed =
            catalogue.devices(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output);
        QVERIFY(findId(listed, QStringLiteral("Device,0")) != nullptr);

        system->setRecords({headphones()});
        timer.restart();
        system->post(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(
            catalogue.devices(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output).size(), 1,
            bound);
        QVERIFY2(timer.elapsed() <= bound, qPrintable(QString::number(timer.elapsed())));
        catalogue.stop();
    }

    // Settled call 14 and the PCM a request asks for.
    void formatsAndRequest()
    {
        QCOMPARE(alsaFormatOrder(),
                 (QList<DeviceSampleFormat>{DeviceSampleFormat::Int32, DeviceSampleFormat::Int24Packed,
                                            DeviceSampleFormat::Int24In32Lsb,
                                            DeviceSampleFormat::Int16}));
        QCOMPARE(alsaFirstAcceptedFormat([](DeviceSampleFormat f) {
                     return f == DeviceSampleFormat::Int16 || f == DeviceSampleFormat::Int32;
                 }),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int32));
        QCOMPARE(alsaFirstAcceptedFormat([](DeviceSampleFormat f) {
                     return f == DeviceSampleFormat::Int24In32Lsb || f == DeviceSampleFormat::Int16;
                 }),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int24In32Lsb));
        QCOMPARE(alsaFirstAcceptedFormat([](DeviceSampleFormat f) {
                     return f == DeviceSampleFormat::Int16;
                 }),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int16));
        QVERIFY(!alsaFirstAcceptedFormat([](DeviceSampleFormat) { return false; }).has_value());

        QCOMPARE(alsaPcmName(usbCard()), QStringLiteral("hw:2,0"));
        const AlsaPcmRequest plain = alsaPcmRequest(usbCard(), outputRequest());
        QCOMPARE(plain.rate, 48000);
        QCOMPARE(plain.channels, 2);
        QCOMPARE(plain.periodFrames, 128);
        QCOMPARE(plain.periods, 3);
        QCOMPARE(alsaPcmRequest(usbCard(), outputRequest(QString(), 256)).periodFrames, 256);

        QCOMPARE(alsaWriteAction(128), AlsaWriteAction::Continue);
        QCOMPARE(alsaWriteAction(-EAGAIN), AlsaWriteAction::Continue);
        QCOMPARE(alsaWriteAction(-EPIPE), AlsaWriteAction::Prepare);
        QCOMPARE(alsaWriteAction(-ESTRPIPE), AlsaWriteAction::Resume);
        QCOMPARE(alsaWriteAction(-ENODEV), AlsaWriteAction::Lost);
        QCOMPARE(alsaWriteAction(-EIO), AlsaWriteAction::Recover);

        QCOMPARE(alsaStreamPair({1, 2}, 2), (AudioChannelPair{1, 2}));
        QCOMPARE(alsaStreamPair({3, 2}, 2), (AudioChannelPair{1, 2}));
        QCOMPARE(alsaStreamPair({3, 2}, 4), (AudioChannelPair{3, 2}));
        QCOMPARE(alsaStreamPair({1, 2}, 1), (AudioChannelPair{1, 1}));
    }

    // The stream: opens hw:C,D through the opener; open() returns once the
    // buffer's periods are written; the period is the device buffer and
    // the delay read then the device latency; it takes the stereo mix.
    void streamOpensAndPlays()
    {
        FakeOpener opener;
        opener.prefill = 3;
        AlsaDirectBus bus(usbCard(), outputRequest(QStringLiteral("Device,0")), opener.opener());
        QVERIFY(!bus.isOpen());
        QVERIFY(bus.open(AudioFormat{}));
        QVERIFY(bus.isOpen());
        QCOMPARE(opener.calls.size(), std::size_t(1));
        QCOMPARE(opener.calls.front().name, QStringLiteral("hw:2,0"));
        QCOMPARE(opener.calls.front().request.periodFrames, 128);
        QCOMPARE(opener.calls.front().request.periods, 3);
        const std::shared_ptr<FakePcmState> pcm = opener.last();
        QVERIFY(pcm->writes.load() >= 3);
        QCOMPARE(pcm->framesWritten.load(), 384L);
        QVERIFY(bus.takesStereoMix());
        QVERIFY(bus.pcmSetup().has_value());
        QCOMPARE(bus.negotiatedFormat().sampleRate, 48000);
        QCOMPARE(bus.negotiatedFormat().channels, 2);
        // 384 frames at 48 kHz.
        QCOMPARE(bus.deviceLatencyNs(), std::int64_t(8000000));
        const AudioDelayParts parts = bus.delayParts();
        QVERIFY(std::abs(parts.deviceBufferMs - 1000.0 * 128.0 / 48000.0) < 1e-6);
        QVERIFY(std::abs(parts.deviceLatencyMs - 8.0) < 1e-6);
        QVERIFY(parts.matcherFillMs >= 0.0);
        const std::optional<IAudioBus::OutputPacing> pacing = bus.outputPacing();
        QVERIFY(pacing.has_value());
        QCOMPARE(pacing->callbackFrames, 128);
        QCOMPARE(pacing->deviceLatencyNs, std::optional<qint64>(8000000));

        // The mix goes into the matcher; the writer keeps writing.
        const std::vector<float> block(2 * 64, 0.25f);
        QCOMPARE(bus.push(reinterpret_cast<const char*>(block.data()), qint64(block.size() * sizeof(float))),
                 qint64(block.size() * sizeof(float)));
        QVERIFY(bus.rxLevel() > 0.2f);
        pcm->queueAccepts(4);
        QTRY_COMPARE_WITH_TIMEOUT(pcm->framesWritten.load(), 7L * 128L, kWaitMs);
        QCOMPARE(bus.underruns(), 0);

        // close() drops the PCM under the blocked write and closes it.
        bus.close();
        QVERIFY(!bus.isOpen());
        QCOMPARE(pcm->drops.load(), 1);
        QVERIFY(pcm->destroyed.load());
        QVERIFY(!bus.pcmSetup().has_value());
    }

    // A buffer size in the request is the period.
    void requestBufferIsThePeriod()
    {
        FakeOpener opener;
        opener.prefill = 3;
        AlsaDirectBus bus(usbCard(), outputRequest(QStringLiteral("Device,0"), 256), opener.opener());
        QVERIFY(bus.open(AudioFormat{}));
        QCOMPARE(opener.calls.front().request.periodFrames, 256);
        QCOMPARE(bus.outputPacing()->callbackFrames, 256);
        QCOMPARE(opener.last()->framesWritten.load(), 768L);
    }

    // -EPIPE: snd_pcm_prepare and count it, then write on.
    void underrunPreparesAndCounts()
    {
        FakeOpener opener;
        opener.prefill = 3;
        AlsaDirectBus bus(usbCard(), outputRequest(), opener.opener());
        EventLog events;
        bus.setStreamEventSink(events.sink());
        QVERIFY(bus.open(AudioFormat{}));
        const std::shared_ptr<FakePcmState> pcm = opener.last();
        pcm->queue({-EPIPE, 0, -EPIPE, 0, 0});
        QTRY_COMPARE_WITH_TIMEOUT(pcm->queued(), 0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(bus.underruns(), 2, kWaitMs);
        QCOMPARE(pcm->prepares.load(), 2);
        QVERIFY(events.take().isEmpty());
        bus.close();
    }

    // -ESTRPIPE resumes, else prepares; any other error prepares.
    void suspendAndOtherErrorsRecover()
    {
        FakeOpener opener;
        opener.prefill = 3;
        AlsaDirectBus bus(usbCard(), outputRequest(), opener.opener());
        EventLog events;
        bus.setStreamEventSink(events.sink());
        QVERIFY(bus.open(AudioFormat{}));
        const std::shared_ptr<FakePcmState> pcm = opener.last();
        pcm->queue({-ESTRPIPE, 0, -EIO, 0});
        QTRY_COMPARE_WITH_TIMEOUT(pcm->queued(), 0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(pcm->prepares.load(), 2, kWaitMs);
        QCOMPARE(pcm->resumes.load(), 1);
        QCOMPARE(bus.underruns(), 0);
        QVERIFY(events.take().isEmpty());
        bus.close();
    }

    // -ENODEV from a write: DeviceLost, once, and the writer stops.
    void unpluggedCardPostsDeviceLost()
    {
        FakeOpener opener;
        opener.prefill = 3;
        AlsaDirectBus bus(usbCard(), outputRequest(), opener.opener());
        EventLog events;
        bus.setStreamEventSink(events.sink());
        QVERIFY(bus.open(AudioFormat{}));
        const std::shared_ptr<FakePcmState> pcm = opener.last();
        pcm->queue({0, -ENODEV, 0, 0});
        QTRY_COMPARE_WITH_TIMEOUT(events.take().size(), 1, kWaitMs);
        QCOMPARE(events.take().front(), AudioStreamEvent::Kind::DeviceLost);
        // The writer ended: the rest of the script stays unread.
        QCOMPARE(pcm->queued(), 2);
        bus.close();
        QCOMPARE(events.take().size(), 1);
    }

    // R-AUD-11: -EBUSY at open fails as in use; other failures do not.
    void busyCardOpensInUse()
    {
        FakeOpener opener;
        opener.failing.insert(QStringLiteral("hw:2,0"), -EBUSY);
        opener.failing.insert(QStringLiteral("hw:0,0"), -ENOENT);
        AlsaDirectBus busy(usbCard(), outputRequest(), opener.opener());
        expectOpenWarnings();
        QVERIFY(!busy.open(AudioFormat{}));
        QVERIFY(busy.openRefusedInUse());
        QVERIFY(!busy.isOpen());
        QVERIFY(busy.errorString().contains(QStringLiteral("in use")));
        AlsaDirectBus gone(headphones(), outputRequest(), opener.opener());
        expectOpenWarnings();
        QVERIFY(!gone.open(AudioFormat{}));
        QVERIFY(!gone.openRefusedInUse());
    }

    // A card that never starts (no write returns) fails the open within
    // the wait, and the writer is stopped.
    void cardThatNeverStartsFailsTheOpen()
    {
        FakeOpener opener;
        opener.prefill = 1;
        AlsaDirectBus bus(usbCard(), outputRequest(), opener.opener());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not start playing")));
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!bus.open(AudioFormat{}));
        QVERIFY(timer.elapsed() < kAlsaStartWaitMs + 1000);
        QVERIFY(!bus.isOpen());
        QVERIFY(opener.last()->destroyed.load());
        QCOMPARE(opener.last()->drops.load(), 1);
    }

    // Settled call 13: graphical.target is a desktop; anything else or
    // nothing is not.
    void desktopDetection()
    {
        FakeAlsaSystem system;
        system.defaultTarget = QStringLiteral("/lib/systemd/system/graphical.target");
        QVERIFY(coreBoxStartsIntoDesktop(system));
        system.defaultTarget = QStringLiteral("/lib/systemd/system/multi-user.target");
        QVERIFY(!coreBoxStartsIntoDesktop(system));
        system.defaultTarget.clear();
        QVERIFY(!coreBoxStartsIntoDesktop(system));

        QCOMPARE(systemdDefaultTargetCandidates(),
                 (QStringList{QStringLiteral("/etc/systemd/system/default.target"),
                              QStringLiteral("/lib/systemd/system/default.target"),
                              QStringLiteral("/usr/lib/systemd/system/default.target")}));

        // The symlinks themselves, read in order, with no systemctl.
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString etc = root.filePath(QStringLiteral("etc.target"));
        const QString lib = root.filePath(QStringLiteral("lib.target"));
        const QString usr = root.filePath(QStringLiteral("usr.target"));
        const QStringList candidates{etc, lib, usr};
        QCOMPARE(systemdDefaultTargetPath(candidates), QString());
        QVERIFY(QFile::link(root.filePath(QStringLiteral("multi-user.target")), usr));
        QCOMPARE(QFileInfo(systemdDefaultTargetPath(candidates)).fileName(),
                 QStringLiteral("multi-user.target"));
        QVERIFY(QFile::link(root.filePath(QStringLiteral("graphical.target")), lib));
        QCOMPARE(QFileInfo(systemdDefaultTargetPath(candidates)).fileName(),
                 QStringLiteral("graphical.target"));
        QVERIFY(QFile::link(root.filePath(QStringLiteral("multi-user.target")), etc));
        QCOMPARE(QFileInfo(systemdDefaultTargetPath(candidates)).fileName(),
                 QStringLiteral("multi-user.target"));
    }

    // R-AUD-01, settled call 9: the Linux Core registers ALSA direct
    // alone, and it is the Core's default engine (R-AUD-02).
    void coreRegistersAlsaDirectOnly()
    {
        AudioBackendContext daemon;
        daemon.daemon = true;
        const auto core = makeSystemAudioBackends(daemon);
        QCOMPARE(core.size(), std::size_t(1));
        QCOMPARE(core.front()->id(), AudioBackendId::AlsaDirect);
        QVERIFY(core.front()->running());
        QCOMPARE(defaultAudioEngine(core), AudioEngineKind::AlsaDirect);
        // In a test run the real adapter lists nothing.
        QVERIFY(core.front()->enumerate().isEmpty());

        const auto window = makeSystemAudioBackends(AudioBackendContext{});
        for (const auto& backend : window) {
            QVERIFY(backend->id() != AudioBackendId::AlsaDirect);
        }
    }

    // R-AUD-32: the real adapter and the real opener open nothing in a
    // test run.
    void realAdapterIsBarredInATestRun()
    {
        const std::unique_ptr<IAlsaDirectSystem> system = makeAlsaDirectSystem();
        QVERIFY(system != nullptr);
        QVERIFY(system->playbackCards().isEmpty());
        system->setNoticeSink([](AudioNotice) {});
        std::unique_ptr<IAudioBus> bus = system->createOutput(headphones(), outputRequest());
        QVERIFY(bus != nullptr);
        expectOpenWarnings();
        QVERIFY(!bus->open(AudioFormat{}));
        QVERIFY(bus->errorString().contains(QStringLiteral("test run")));
        QVERIFY(!bus->openRefusedInUse());

        const AlsaPcmOpenResult opened = makeAlsaHwPcmOpener()(QStringLiteral("hw:0,0"), AlsaPcmRequest{});
        QVERIFY(opened.pcm == nullptr);
        QVERIFY(opened.detail.contains(QStringLiteral("test run")));
    }

    // R-AUD-25, R-AUD-08 through the engine and the supervisor on the
    // Core: an old "(hw:2,0)" choice migrates to ALSA direct and opens
    // the USB card; -ENODEV from a write falls back to the default card at
    // once; the card listed again plays again.
    void unpluggedCardFallsBackAndReturns()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Speakers/DriverApi"), QStringLiteral("ALSA"));
        s.setValue(QStringLiteral("audio/Speakers/DeviceName"),
                   QStringLiteral("USB Audio Device: - (hw:2,0)"));

        FakeOpener opener;
        opener.paced = true;
        auto system = std::make_shared<FakeAlsaSystem>();
        system->pcmOpener = &opener;
        system->setRecords({headphones(), usbCard()});
        auto backend = std::make_shared<AlsaDirectBackend>(system);

        auto engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendContext({.daemon = true});
        engine->setAudioBackendsForTest({backend});
        engine->start();

        QCOMPARE(s.value(QStringLiteral("audio/Speakers/Engine")).toString(), QStringLiteral("AlsaDirect"));
        QCOMPARE(engine->defaultEngine(), AudioEngineKind::AlsaDirect);
        QTRY_COMPARE_WITH_TIMEOUT(engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::Playing, kWaitMs);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).playingName, QStringLiteral("USB Audio Device"));
        QCOMPARE(opener.openCount(QStringLiteral("hw:2,0")), 1);
        // The name match saved the card's ID back.
        QTRY_COMPARE_WITH_TIMEOUT(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(),
                                  QStringLiteral("Device,0"), kWaitMs);

        // Unplugged: the write answers -ENODEV, the default opens at once.
        const std::shared_ptr<FakePcmState> usb = opener.lastFor(QStringLiteral("hw:2,0"));
        QVERIFY(usb != nullptr);
        usb->queue({-ENODEV});
        QTRY_COMPARE_WITH_TIMEOUT(engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, kWaitMs);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).reason, AudioRoleReason::NotConnected);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).playingName, QStringLiteral("bcm2835 Headphones"));
        QCOMPARE(opener.openCount(QStringLiteral("hw:0,0")), 1);

        // The card leaves the list, then comes back: it plays again.
        system->setRecords({headphones()});
        system->post(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(
            engine->catalogue()->devices(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output).size() == 1,
            kWaitMs);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        system->setRecords({headphones(), usbCard()});
        system->post(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::Playing, kWaitMs);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).playingName, QStringLiteral("USB Audio Device"));
        QCOMPARE(opener.openCount(QStringLiteral("hw:2,0")), 2);
        engine->stop();
        engine.reset();
    }

    // R-AUD-11 on the Core: a card another program holds reads in use,
    // and the default card plays meanwhile.
    void busyCardReadsInUseAndPlaysTheDefault()
    {
        AudioDeviceConfig chosen;
        chosen.engine = AudioEngineKind::AlsaDirect;
        chosen.deviceId = QStringLiteral("Device,0");
        chosen.deviceName = QStringLiteral("USB Audio Device");
        chosen.saveToSettings(QStringLiteral("audio/Speakers"));

        FakeOpener opener;
        opener.paced = true;
        opener.failing.insert(QStringLiteral("hw:2,0"), -EBUSY);
        auto system = std::make_shared<FakeAlsaSystem>();
        system->pcmOpener = &opener;
        system->setRecords({headphones(), usbCard()});
        auto backend = std::make_shared<AlsaDirectBackend>(system);

        auto engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendContext({.daemon = true});
        engine->setAudioBackendsForTest({backend});
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("ALSA direct did not open")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("IAudioBus open failed")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not open")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not open")));
        engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, kWaitMs);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).reason, AudioRoleReason::InUse);
        QCOMPARE(engine->roleStatus(AudioRole::Speakers).playingName, QStringLiteral("bcm2835 Headphones"));
        engine->stop();
        engine.reset();
    }
};

QTEST_MAIN(TestAlsaDirectBackend)
#include "tst_alsa_direct_backend.moc"
