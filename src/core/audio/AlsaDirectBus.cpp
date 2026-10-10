// =================================================================
// src/core/audio/AlsaDirectBus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AlsaDirectBus.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-11, R-AUD-15, R-AUD-25,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: final review fixes (R-AUD-07, R-AUD-25): a lost stream
//               reads closed and a loss before the sink is kept for it;
//               the request asks for the pair's channels. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AlsaDirectBus.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(Q_OS_LINUX)
#include <alsa/asoundlib.h>
#endif

namespace NereusSDR {

namespace {

const QString kBarredError = QStringLiteral("Audio devices are not opened in a test run");

} // namespace

QString alsaPcmName(const AlsaCardRecord& record)
{
    return QStringLiteral("hw:%1,%2").arg(record.card).arg(record.device);
}

AlsaPcmRequest alsaPcmRequest(const AlsaCardRecord& record, const AudioStreamRequest& request)
{
    AlsaPcmRequest out;
    out.rate = 48000;
    // Enough channels for the request's pair (two at least, so a stereo
    // pair stays as today), as many as the card has at most.
    const int cardChannels = std::max(1, record.channels);
    const int pairLast = std::max(1, request.pair.firstChannel)
                         + std::clamp(request.pair.channelCount, 1, 2) - 1;
    out.channels = std::clamp(std::max(2, pairLast), 1, cardChannels);
    out.periodFrames = request.bufferFrames > 0 ? request.bufferFrames : kAlsaDefaultPeriodFrames;
    out.periods = kAlsaPeriods;
    return out;
}

std::optional<DeviceSampleFormat> alsaFirstAcceptedFormat(
    const std::function<bool(DeviceSampleFormat)>& accepts)
{
    for (const DeviceSampleFormat format : alsaFormatOrder()) {
        if (accepts && accepts(format)) {
            return format;
        }
    }
    return std::nullopt;
}

AlsaWriteAction alsaWriteAction(long result)
{
    if (result >= 0 || result == -EAGAIN) {
        return AlsaWriteAction::Continue;
    }
    if (result == -EPIPE) {
        return AlsaWriteAction::Prepare;
    }
    if (result == -ESTRPIPE) {
        return AlsaWriteAction::Resume;
    }
    if (result == -ENODEV) {
        return AlsaWriteAction::Lost;
    }
    return AlsaWriteAction::Recover;
}

AudioChannelPair alsaStreamPair(AudioChannelPair requested, int channels)
{
    if (channels <= 1) {
        return AudioChannelPair{1, 1};
    }
    const int count = std::clamp(requested.channelCount, 1, 2);
    if (requested.firstChannel >= 1 && requested.firstChannel + count - 1 <= channels) {
        return AudioChannelPair{requested.firstChannel, count};
    }
    return AudioChannelPair{1, 2};
}

// ---------------------------------------------------------------------------
// The stream.
// ---------------------------------------------------------------------------
struct AlsaDirectBus::Impl {
    AlsaCardRecord card;
    AudioStreamRequest request;
    AlsaPcmOpener opener;
    std::shared_ptr<void> hold;   // kept while the stream lives

    // Set by open() before the writer starts; read by the writer only.
    std::unique_ptr<IAlsaPcm> pcm;
    AlsaPcmSetup setup;
    AudioChannelPair pair;
    int frameBytes = 0;
    std::vector<float> stereo;            // 2 * period
    std::vector<std::uint8_t> device;     // period * frameBytes
    MatcherReader reader;
    std::unique_ptr<DeviceRateMatcher> matcher;
    std::thread thread;
    std::promise<bool> started;
    bool startedSet = false;              // the writer's

    std::atomic<bool> stop{false};
    std::atomic<bool> open{false};
    std::atomic<bool> writerLost{false};
    std::atomic<int> underruns{0};
    std::atomic<std::int64_t> latencyNs{-1};
    std::atomic<std::uint64_t> consumedFrames{0};
    std::atomic<float> rxLevel{0.0f};

    std::mutex sinkMutex;
    std::function<void(const AudioStreamEvent&)> sink;
    // A loss posted before a sink was set, sent when one is (under sinkMutex).
    std::optional<AudioStreamEvent> pendingEvent;

    AudioFormat format;
    QString error;
    bool inUse = false;

    void post(AudioStreamEvent::Kind kind, const QString& detail)
    {
        // The writer's exit path only, never while it plays.
        AudioStreamEvent event;
        event.kind = kind;
        event.detail = detail;
        std::function<void(const AudioStreamEvent&)> s;
        {
            std::lock_guard<std::mutex> lock(sinkMutex);
            s = sink;
            if (!s) {
                // The engine sets its sink after open() returns; a loss in
                // between waits for it.
                pendingEvent = event;
                return;
            }
        }
        s(event);
    }

    void signalStarted(bool ok)
    {
        if (!startedSet) {
            startedSet = true;
            started.set_value(ok);
        }
    }

    // The writer thread.  No lock, no allocation, no Qt call while it plays.
    void run()
    {
        AudioPriorityToken* priority = elevateAudioThreadPriority();
        const long period = setup.periodFrames;
        int periodsWritten = 0;
        bool lost = false;
        while (!stop.load(std::memory_order_acquire) && !lost) {
            reader.read(stereo.data(), int(period));
            writeStereoToDevice(stereo.data(), int(period), device.data(), setup.format,
                                setup.channels, pair, true, nullptr);
            long done = 0;
            while (done < period && !stop.load(std::memory_order_acquire)) {
                const long result = pcm->writei(device.data() + std::size_t(done) * std::size_t(frameBytes),
                                                period - done);
                if (result >= 0) {
                    done += result;
                    continue;
                }
                if (stop.load(std::memory_order_acquire)) {
                    break;   // close() dropped the PCM under the write
                }
                switch (alsaWriteAction(result)) {
                case AlsaWriteAction::Continue:
                    break;
                case AlsaWriteAction::Prepare:
                    underruns.fetch_add(1, std::memory_order_relaxed);
                    if (pcm->prepare() < 0) {
                        lost = true;
                    }
                    break;
                case AlsaWriteAction::Resume:
                    if (pcm->resume() < 0 && pcm->prepare() < 0) {
                        lost = true;
                    }
                    break;
                case AlsaWriteAction::Lost:
                    lost = true;
                    break;
                case AlsaWriteAction::Recover:
                    if (pcm->prepare() < 0) {
                        lost = true;
                    }
                    break;
                }
                if (lost) {
                    break;
                }
            }
            if (lost || stop.load(std::memory_order_acquire)) {
                break;
            }
            consumedFrames.fetch_add(std::uint64_t(period), std::memory_order_relaxed);
            if (!startedSet && ++periodsWritten >= setup.bufferFrames / std::max<long>(1, period)) {
                // The buffer is full and the card running: its delay now
                // is the device latency.
                const long delay = pcm->delayFrames();
                if (delay >= 0 && setup.rate > 0) {
                    latencyNs.store(std::int64_t(double(delay) * 1e9 / double(setup.rate)),
                                    std::memory_order_relaxed);
                }
                signalStarted(true);
            }
        }
        if (!startedSet) {
            signalStarted(false);
        } else if (lost) {
            // No writer plays any more: the stream is not open (open()
            // reads `writerLost` after it marks the stream open).
            writerLost.store(true);
            open.store(false);
            post(AudioStreamEvent::Kind::DeviceLost, QStringLiteral("The sound card went away"));
        }
        leaveAudioThreadPriority(priority);
    }

    void stopWriter()
    {
        stop.store(true, std::memory_order_release);
        if (thread.joinable()) {
            if (pcm) {
                pcm->drop();   // wakes a blocked write
            }
            thread.join();
        }
    }
};

AlsaDirectBus::AlsaDirectBus(AlsaCardRecord card, AudioStreamRequest request, AlsaPcmOpener opener,
                             std::shared_ptr<void> hold)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->card = std::move(card);
    m_impl->request = std::move(request);
    m_impl->opener = std::move(opener);
    m_impl->hold = std::move(hold);
}

AlsaDirectBus::~AlsaDirectBus()
{
    close();
}

bool AlsaDirectBus::open(const AudioFormat& format)
{
    close();
    Impl& d = *m_impl;
    d.inUse = false;
    if (!d.opener) {
        d.error = kBarredError;
        return false;
    }
    const QString pcmName = alsaPcmName(d.card);
    AlsaPcmOpenResult opened = d.opener(pcmName, alsaPcmRequest(d.card, d.request));
    if (!opened.pcm) {
        d.inUse = opened.error == -EBUSY;
        d.error = d.inUse ? QStringLiteral("%1 is in use by another program").arg(d.card.cardName)
                          : (opened.detail.isEmpty() ? QStringLiteral("%1 did not open").arg(pcmName)
                                                     : opened.detail);
        qCWarning(lcAudio) << "ALSA direct did not open" << pcmName << ":" << d.error;
        return false;
    }
    d.pcm = std::move(opened.pcm);
    d.setup = d.pcm->setup();
    d.setup.periodFrames = std::max(1, d.setup.periodFrames);
    d.setup.bufferFrames = std::max(d.setup.periodFrames, d.setup.bufferFrames);
    d.setup.channels = std::max(1, d.setup.channels);
    d.pair = alsaStreamPair(d.request.pair, d.setup.channels);
    d.frameBytes = deviceSampleBytes(d.setup.format) * d.setup.channels;

    DeviceRateMatcher::Config config;
    config.inRate = 48000;
    config.outRate = d.setup.rate;
    config.callbackFrames = d.setup.periodFrames;
    config.delayMs = d.request.delayMs;
    d.matcher = std::make_unique<DeviceRateMatcher>(config);
    if (!d.matcher->valid()) {
        d.error = QStringLiteral("Clock matcher cannot run at %1 Hz").arg(d.setup.rate);
        d.matcher.reset();
        d.pcm.reset();
        return false;
    }
    d.reader = d.matcher->makeReader();
    d.stereo.assign(std::size_t(2 * d.setup.periodFrames), 0.0f);
    d.device.assign(std::size_t(d.setup.periodFrames) * std::size_t(d.frameBytes), 0);
    d.underruns.store(0);
    d.consumedFrames.store(0);
    d.latencyNs.store(-1);
    d.stop.store(false);
    d.writerLost.store(false);
    {
        std::lock_guard<std::mutex> lock(d.sinkMutex);
        d.pendingEvent.reset();
    }
    d.started = std::promise<bool>();
    d.startedSet = false;
    std::future<bool> started = d.started.get_future();
    d.thread = std::thread([&d] { d.run(); });

    const bool ok = started.wait_for(std::chrono::milliseconds(kAlsaStartWaitMs))
                        == std::future_status::ready
                    && started.get();
    if (!ok) {
        d.stopWriter();
        d.reader = MatcherReader();
        d.matcher.reset();
        d.pcm.reset();
        d.error = QStringLiteral("%1 did not start playing").arg(pcmName);
        qCWarning(lcAudio) << "ALSA direct:" << d.error;
        return false;
    }
    d.format = format;
    d.format.sampleRate = d.setup.rate;
    d.format.channels = 2;
    d.format.sample = AudioFormat::Sample::Float32;
    d.error.clear();
    d.open.store(true);
    if (d.writerLost.load()) {
        // Lost between the start and here: the event waits for the sink.
        d.open.store(false);
    }
    return true;
}

void AlsaDirectBus::close()
{
    Impl& d = *m_impl;
    d.open.store(false, std::memory_order_release);
    d.stopWriter();
    if (d.pcm) {
        const int underruns = d.underruns.load();
        if (underruns > 0) {
            qCInfo(lcAudio) << "ALSA direct" << alsaPcmName(d.card) << "recovered" << underruns
                            << "underruns";
        }
    }
    d.pcm.reset();   // snd_pcm_close
    d.reader = MatcherReader();
    d.matcher.reset();
}

bool AlsaDirectBus::isOpen() const
{
    return m_impl->open.load(std::memory_order_acquire);
}

qint64 AlsaDirectBus::push(const char* data, qint64 bytes)
{
    Impl& d = *m_impl;
    if (!d.matcher || !isOpen() || data == nullptr || bytes <= 0) {
        return 0;
    }
    const int frames = int(bytes / qint64(2 * sizeof(float)));
    if (frames <= 0) {
        return 0;
    }
    const auto* in = reinterpret_cast<const float*>(data);
    float peak = 0.0f;
    for (int i = 0; i < frames * 2; ++i) {
        peak = std::max(peak, std::abs(in[i]));
    }
    d.matcher->write(in, frames, audioProbeNowNs());
    d.rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

void AlsaDirectBus::flush()
{
    if (m_impl->matcher) {
        m_impl->matcher->requestFlush();
    }
}

std::optional<IAudioBus::OutputPacing> AlsaDirectBus::outputPacing() const
{
    const Impl& d = *m_impl;
    if (!d.matcher) {
        return std::nullopt;
    }
    const MatcherRingHeader* ring = d.matcher->ring();
    OutputPacing pacing;
    pacing.consumedFrames = ring->requested.load(std::memory_order_acquire);
    pacing.queuedFrames = std::max(0, int(std::lround(d.matcher->fillFrames())));
    pacing.capacityFrames = int(ring->rsizeFrames.load(std::memory_order_acquire));
    pacing.callbackFrames = d.setup.periodFrames;
    const std::int64_t latency = d.latencyNs.load(std::memory_order_relaxed);
    if (latency > 0) {
        pacing.deviceLatencyNs = qint64(latency);
    }
    return pacing;
}

float AlsaDirectBus::rxLevel() const
{
    return m_impl->rxLevel.load(std::memory_order_acquire);
}

AudioFormat AlsaDirectBus::negotiatedFormat() const
{
    return m_impl->format;
}

QString AlsaDirectBus::errorString() const
{
    return m_impl->error;
}

bool AlsaDirectBus::openRefusedInUse() const
{
    return m_impl->inUse;
}

void AlsaDirectBus::setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink)
{
    std::optional<AudioStreamEvent> pending;
    std::function<void(const AudioStreamEvent&)> s;
    {
        std::lock_guard<std::mutex> lock(m_impl->sinkMutex);
        m_impl->sink = std::move(sink);
        if (m_impl->sink && m_impl->pendingEvent) {
            pending = std::exchange(m_impl->pendingEvent, std::nullopt);
            s = m_impl->sink;
        }
    }
    if (pending) {
        s(*pending);
    }
}

// The device buffer is one period; the device latency the delay read at open.
AudioDelayParts AlsaDirectBus::delayParts() const
{
    const Impl& d = *m_impl;
    if (!d.matcher || d.setup.rate <= 0) {
        return {};
    }
    const double bufferMs = 1000.0 * double(d.setup.periodFrames) / double(d.setup.rate);
    const std::int64_t latency = d.latencyNs.load(std::memory_order_relaxed);
    const double latencyMs = latency > 0 ? double(latency) / 1e6 : 0.0;
    return d.matcher->delayParts(bufferMs, latencyMs);
}

std::optional<DeviceRateMatcherStats> AlsaDirectBus::matcherStats() const
{
    if (!m_impl->matcher) {
        return std::nullopt;
    }
    return m_impl->matcher->stats();
}

void AlsaDirectBus::restartClockMatch()
{
    if (m_impl->matcher) {
        m_impl->matcher->requestRestart();
    }
}

void AlsaDirectBus::requestFadeOut()
{
    if (m_impl->matcher) {
        m_impl->reader.requestFadeOut();
    }
}

bool AlsaDirectBus::fadedOut() const
{
    return m_impl->matcher ? m_impl->reader.fadedOut() : true;
}

int AlsaDirectBus::underruns() const
{
    return m_impl->underruns.load();
}

std::optional<AlsaPcmSetup> AlsaDirectBus::pcmSetup() const
{
    if (!m_impl->pcm) {
        return std::nullopt;
    }
    return m_impl->setup;
}

std::int64_t AlsaDirectBus::deviceLatencyNs() const
{
    return m_impl->pcm ? m_impl->latencyNs.load() : -1;
}

// ---------------------------------------------------------------------------
// The real PCM.
// ---------------------------------------------------------------------------
#if defined(Q_OS_LINUX)
namespace {

snd_pcm_format_t toAlsaFormat(DeviceSampleFormat format)
{
    switch (format) {
    case DeviceSampleFormat::Int32:
        return SND_PCM_FORMAT_S32_LE;
    case DeviceSampleFormat::Int24Packed:
        return SND_PCM_FORMAT_S24_3LE;
    case DeviceSampleFormat::Int24In32Lsb:
        return SND_PCM_FORMAT_S24_LE;
    case DeviceSampleFormat::Int16:
        return SND_PCM_FORMAT_S16_LE;
    case DeviceSampleFormat::Float32:
        return SND_PCM_FORMAT_FLOAT_LE;
    case DeviceSampleFormat::Float64:
        return SND_PCM_FORMAT_FLOAT64_LE;
    }
    return SND_PCM_FORMAT_UNKNOWN;
}

class AlsaHwPcm final : public IAlsaPcm {
public:
    AlsaHwPcm(snd_pcm_t* pcm, AlsaPcmSetup setup) : m_pcm(pcm), m_setup(setup) {}
    ~AlsaHwPcm() override { snd_pcm_close(m_pcm); }

    AlsaHwPcm(const AlsaHwPcm&) = delete;
    AlsaHwPcm& operator=(const AlsaHwPcm&) = delete;

    AlsaPcmSetup setup() const override { return m_setup; }
    long writei(const void* buffer, long frames) override
    {
        return long(snd_pcm_writei(m_pcm, buffer, snd_pcm_uframes_t(frames)));
    }
    int prepare() override { return snd_pcm_prepare(m_pcm); }
    int resume() override { return snd_pcm_resume(m_pcm); }
    long delayFrames() override
    {
        snd_pcm_sframes_t delay = 0;
        const int rc = snd_pcm_delay(m_pcm, &delay);
        return rc < 0 ? long(rc) : long(delay);
    }
    // A hw: PCM's calls go straight to the kernel, which lets another
    // thread drop the stream under a blocked write.
    void drop() override { snd_pcm_drop(m_pcm); }

private:
    snd_pcm_t* m_pcm = nullptr;
    AlsaPcmSetup m_setup;
};

struct HwParamsDeleter {
    void operator()(snd_pcm_hw_params_t* p) const { snd_pcm_hw_params_free(p); }
};
struct SwParamsDeleter {
    void operator()(snd_pcm_sw_params_t* p) const { snd_pcm_sw_params_free(p); }
};

AlsaPcmOpenResult failed(snd_pcm_t* pcm, int error, const QString& detail)
{
    if (pcm != nullptr) {
        snd_pcm_close(pcm);
    }
    AlsaPcmOpenResult result;
    result.error = error;
    result.detail = detail;
    return result;
}

AlsaPcmOpenResult openHwPcm(const QString& pcmName, const AlsaPcmRequest& request)
{
    if (audioDevicesBarredForTestRun()) {
        return failed(nullptr, -EACCES, kBarredError);
    }
    const QByteArray name = pcmName.toUtf8();
    snd_pcm_t* pcm = nullptr;
    // Non-blocking open: a busy hw: PCM answers -EBUSY instead of waiting
    // until it is free.  Writes then block.
    int rc = snd_pcm_open(&pcm, name.constData(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (rc < 0) {
        return failed(nullptr, rc, QString::fromUtf8(snd_strerror(rc)));
    }
    rc = snd_pcm_nonblock(pcm, 0);
    if (rc < 0) {
        return failed(pcm, rc, QString::fromUtf8(snd_strerror(rc)));
    }

    snd_pcm_hw_params_t* rawHw = nullptr;
    if (snd_pcm_hw_params_malloc(&rawHw) < 0) {
        return failed(pcm, -ENOMEM, QStringLiteral("No memory for the card's parameters"));
    }
    const std::unique_ptr<snd_pcm_hw_params_t, HwParamsDeleter> hw(rawHw);
    rc = snd_pcm_hw_params_any(pcm, hw.get());
    if (rc >= 0) {
        rc = snd_pcm_hw_params_set_access(pcm, hw.get(), SND_PCM_ACCESS_RW_INTERLEAVED);
    }
    if (rc < 0) {
        return failed(pcm, rc, QStringLiteral("%1 does not play interleaved").arg(pcmName));
    }
    snd_pcm_hw_params_set_rate_resample(pcm, hw.get(), 0);
    const std::optional<DeviceSampleFormat> format =
        alsaFirstAcceptedFormat([&](DeviceSampleFormat f) {
            return snd_pcm_hw_params_test_format(pcm, hw.get(), toAlsaFormat(f)) == 0;
        });
    if (!format) {
        return failed(pcm, -EINVAL, QStringLiteral("%1 plays none of the formats NereusSDR writes")
                                         .arg(pcmName));
    }
    snd_pcm_hw_params_set_format(pcm, hw.get(), toAlsaFormat(*format));
    unsigned channels = unsigned(request.channels);
    snd_pcm_hw_params_set_channels_near(pcm, hw.get(), &channels);
    unsigned rate = unsigned(request.rate);
    snd_pcm_hw_params_set_rate_near(pcm, hw.get(), &rate, nullptr);
    snd_pcm_uframes_t period = snd_pcm_uframes_t(request.periodFrames);
    snd_pcm_hw_params_set_period_size_near(pcm, hw.get(), &period, nullptr);
    unsigned periods = unsigned(request.periods);
    snd_pcm_hw_params_set_periods_near(pcm, hw.get(), &periods, nullptr);
    rc = snd_pcm_hw_params(pcm, hw.get());
    if (rc < 0) {
        return failed(pcm, rc, QString::fromUtf8(snd_strerror(rc)));
    }
    snd_pcm_uframes_t buffer = 0;
    snd_pcm_hw_params_get_period_size(hw.get(), &period, nullptr);
    snd_pcm_hw_params_get_buffer_size(hw.get(), &buffer);
    snd_pcm_hw_params_get_channels(hw.get(), &channels);
    snd_pcm_hw_params_get_rate(hw.get(), &rate, nullptr);

    snd_pcm_sw_params_t* rawSw = nullptr;
    if (snd_pcm_sw_params_malloc(&rawSw) < 0) {
        return failed(pcm, -ENOMEM, QStringLiteral("No memory for the card's parameters"));
    }
    const std::unique_ptr<snd_pcm_sw_params_t, SwParamsDeleter> sw(rawSw);
    rc = snd_pcm_sw_params_current(pcm, sw.get());
    if (rc >= 0) {
        // The card starts once the whole buffer is written.
        rc = snd_pcm_sw_params_set_start_threshold(pcm, sw.get(), buffer);
    }
    if (rc >= 0) {
        rc = snd_pcm_sw_params_set_avail_min(pcm, sw.get(), period);
    }
    if (rc >= 0) {
        rc = snd_pcm_sw_params(pcm, sw.get());
    }
    if (rc >= 0) {
        rc = snd_pcm_prepare(pcm);
    }
    if (rc < 0) {
        return failed(pcm, rc, QString::fromUtf8(snd_strerror(rc)));
    }

    AlsaPcmSetup setup;
    setup.format = *format;
    setup.rate = int(rate);
    setup.channels = int(channels);
    setup.periodFrames = int(period);
    setup.bufferFrames = int(buffer);
    AlsaPcmOpenResult result;
    result.pcm = std::make_unique<AlsaHwPcm>(pcm, setup);
    return result;
}

} // namespace
#endif

AlsaPcmOpener makeAlsaHwPcmOpener()
{
#if defined(Q_OS_LINUX)
    return &openHwPcm;
#else
    return [](const QString&, const AlsaPcmRequest&) {
        AlsaPcmOpenResult result;
        result.error = -ENODEV;
        result.detail = QStringLiteral("ALSA is not on this system");
        return result;
    };
#endif
}

} // namespace NereusSDR
