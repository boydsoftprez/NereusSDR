// =================================================================
// src/core/audio/CaptureAudioBus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Lock-free reader over the capture
// helper's shared clock matcher ring; no Thetis logic.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): reads the
//               helper's clock matcher ring through a MatcherReader; the
//               Pcm-fed ring is gone.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: mic drain fix (R-AUD-17, R-R3-36): pull() is paced by the
//               48 kHz clock; a drain until 0 now ends.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureAudioBus.h"

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>

namespace NereusSDR {

namespace {

constexpr int kBytesPerFrame = static_cast<int>(sizeof(float));
constexpr int kScratchFrames = 1024;

} // namespace

struct CaptureAudioBus::Source {
    explicit Source(MatcherRingHeader* r) : ring(r), reader(r) {}

    MatcherRingHeader* ring;
    MatcherReader reader;
    std::array<float, static_cast<std::size_t>(kScratchFrames) * 2> stereo{};
    // pull()'s pacing, the pulling thread only.  Credit is in frames and
    // runs from -kPaceSlackFrames to kPaceCreditCapFrames.
    bool paced = false;
    std::int64_t lastPullNs = 0;
    double creditFrames = 0.0;
};

CaptureAudioBus::CaptureAudioBus()
    : m_clock(&audioProbeNowNs)
{
}

void CaptureAudioBus::setClockForTest(Clock clock)
{
    m_clock = clock != nullptr ? clock : &audioProbeNowNs;
}

CaptureAudioBus::~CaptureAudioBus()
{
    detachRing();
}

template <typename Fn, typename T>
T CaptureAudioBus::withSource(Fn&& fn, T fallback) const
{
    // A hazard count: detachRing() clears the pointer, then waits for the
    // count to reach zero, so a source seen here stays alive until the
    // count drops.  Both sides use sequentially consistent order.
    m_busy.fetch_add(1);
    Source* source = m_source.load();
    T result = fallback;
    if (source != nullptr) {
        result = fn(*source);
    }
    m_busy.fetch_sub(1);
    return result;
}

bool CaptureAudioBus::open(const AudioFormat& /*format*/)
{
    return true;
}

void CaptureAudioBus::close()
{
}

bool CaptureAudioBus::isOpen() const
{
    return m_available.load(std::memory_order_acquire);
}

qint64 CaptureAudioBus::push(const char* /*data*/, qint64 /*bytes*/)
{
    return 0;
}

qint64 CaptureAudioBus::pull(char* data, qint64 maxBytes)
{
    if (data == nullptr || maxBytes < kBytesPerFrame || !m_available.load(std::memory_order_acquire)) {
        return 0;
    }
    const qint64 asked = maxBytes / kBytesPerFrame;
    return withSource(
        [&](Source& source) -> qint64 {
            // The 48 kHz clock's frames since the last pull, plus the slack.
            // Without this bound the reader, which pads a dry run, would
            // hand a caller draining until 0 padding forever.
            const std::int64_t now = m_clock();
            if (source.paced) {
                const double elapsedFrames =
                    double(std::max<std::int64_t>(0, now - source.lastPullNs))
                    * double(kSampleRate) / 1e9;
                source.creditFrames = std::min(source.creditFrames + elapsedFrames,
                                               double(kPaceCreditCapFrames));
            }
            source.paced = true;
            source.lastPullNs = now;
            const qint64 wanted = std::clamp<qint64>(
                static_cast<qint64>(std::floor(source.creditFrames)) + kPaceSlackFrames, 0, asked);
            source.creditFrames -= double(wanted);
            auto* out = reinterpret_cast<float*>(data);
            qint64 done = 0;
            while (done < wanted) {
                const int frames = static_cast<int>(std::min<qint64>(wanted - done, kScratchFrames));
                source.reader.read(source.stereo.data(), frames);
                for (int f = 0; f < frames; ++f) {
                    out[done + f] = source.stereo[static_cast<std::size_t>(2 * f)];
                }
                done += frames;
            }
            return done * kBytesPerFrame;
        },
        qint64(0));
}

void CaptureAudioBus::flush()
{
}

float CaptureAudioBus::rxLevel() const
{
    return 0.0f;
}

float CaptureAudioBus::txLevel() const
{
    if (!m_available.load(std::memory_order_acquire)) {
        return 0.0f;
    }
    return m_level.load(std::memory_order_acquire);
}

QString CaptureAudioBus::backendName() const
{
    return QStringLiteral("Capture helper");
}

AudioFormat CaptureAudioBus::negotiatedFormat() const
{
    AudioFormat format;
    format.sampleRate = kSampleRate;
    format.channels = 1;
    format.sample = AudioFormat::Sample::Float32;
    return format;
}

bool CaptureAudioBus::attachRing(MatcherRingHeader* ring)
{
    if (ring == nullptr || m_owned != nullptr) {
        return false;
    }
    m_owned = std::make_unique<Source>(ring);
    if (!m_owned->reader.valid()) {
        m_owned.reset();
        return false;
    }
    m_levelSeen = ring->written.load(std::memory_order_acquire);
    m_source.store(m_owned.get());
    return true;
}

void CaptureAudioBus::detachRing()
{
    m_source.store(nullptr);
    while (m_busy.load() != 0) {
        std::this_thread::yield();
    }
    m_owned.reset();
    m_level.store(0.0f, std::memory_order_release);
}

void CaptureAudioBus::setAvailable(bool available)
{
    m_available.store(available, std::memory_order_release);
    if (!available) {
        m_level.store(0.0f, std::memory_order_release);
    }
}

void CaptureAudioBus::noteWake()
{
    withSource(
        [&](Source& source) -> int {
            const MatcherRingHeader& ring = *source.ring;
            const std::uint64_t written = ring.written.load(std::memory_order_acquire);
            if (written <= m_levelSeen) {
                return 0;
            }
            // The newest frames only: the writer works past written, never
            // on the frames below it, while the ring holds more than this.
            const std::uint64_t window = std::min<std::uint64_t>(
                {written - m_levelSeen, static_cast<std::uint64_t>(kLevelWindowFrames),
                 static_cast<std::uint64_t>(ring.capacityFrames / 4)});
            const std::uint64_t mask = static_cast<std::uint64_t>(ring.capacityFrames) - 1;
            const float* frames = ring.frames();
            float peak = 0.0f;
            for (std::uint64_t i = written - window; i < written; ++i) {
                peak = std::max(peak, std::fabs(frames[(i & mask) * 2]));
            }
            m_levelSeen = written;
            m_level.store(peak, std::memory_order_release);
            return 0;
        },
        0);
}

bool CaptureAudioBus::ringAttached() const
{
    return withSource([](Source&) { return true; }, false);
}

std::optional<double> CaptureAudioBus::fillFrames() const
{
    return withSource(
        [](Source& source) -> std::optional<double> {
            return source.ring->fillFrames.load(std::memory_order_acquire);
        },
        std::optional<double>{});
}

quint64 CaptureAudioBus::overruns() const
{
    return withSource(
        [](Source& source) -> quint64 {
            return source.ring->overruns.load(std::memory_order_acquire);
        },
        quint64(0));
}

quint64 CaptureAudioBus::dryRuns() const
{
    return withSource(
        [](Source& source) -> quint64 {
            return source.ring->dryRuns.load(std::memory_order_acquire);
        },
        quint64(0));
}

std::int64_t CaptureAudioBus::lastWriteNs() const
{
    return withSource(
        [](Source& source) -> std::int64_t {
            return source.ring->lastWriteNs.load(std::memory_order_acquire);
        },
        std::int64_t(0));
}

} // namespace NereusSDR
