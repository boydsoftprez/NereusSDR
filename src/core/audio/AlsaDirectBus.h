// =================================================================
// src/core/audio/AlsaDirectBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The ALSA direct engine's output
// stream (R-AUD-11, R-AUD-15, R-AUD-25, R-AUD-32); no upstream logic.
//
// An output opens the card's own hw:<card>,<device> PCM: interleaved, the
// first format of alsaFormatOrder() the card accepts, its nearest rate to
// 48000 (the clock matcher resamples), its nearest channel count to two,
// a period of the request's buffer frames (kAlsaDefaultPeriodFrames when
// it names none) and kAlsaPeriods periods.  The PCM is opened with
// SND_PCM_NONBLOCK, so a card another program holds answers -EBUSY at
// once (a blocking open of a busy hw: PCM waits until it is free), and is
// switched to blocking writes straight after.  -EBUSY makes open() fail
// with openRefusedInUse() true (R-AUD-11).
//
// A writer thread raised to the audio priority (elevateAudioThreadPriority,
// SCHED_FIFO on Linux) loops: MatcherReader::read one period of stereo,
// writeStereoToDevice into the card's format and channels, snd_pcm_writei
// it (blocking).  It takes no lock, allocates nothing and makes no Qt
// call while it plays.  open() returns once the first kAlsaPeriods periods
// are written (the card is running) and snd_pcm_delay has been read: that
// delay is the device latency, the period the device buffer.  -EPIPE (an
// underrun) is recovered with snd_pcm_prepare and counted; -ESTRPIPE
// (suspend) resumes, else prepares; -ENODEV (the card was unplugged), or
// an error snd_pcm_prepare cannot clear, ends the thread, marks the
// stream not open and posts DeviceLost once (kept until a sink is set when
// none is yet).  close() drops the PCM, which wakes a blocked write, and
// joins the thread.
//
// The PCM sits behind IAlsaPcm and AlsaPcmOpener, so the stream's open,
// writer and error handling are tested with a fake PCM; the real opener
// (makeAlsaHwPcmOpener) refuses every open while
// audioDevicesBarredForTestRun() is true.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-11, R-AUD-15, R-AUD-25,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: final review fixes (R-AUD-07, R-AUD-25). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10: setClockMatchWritePacket(): a writer of whole packets
//               (remote playback) tells the bus's clock matcher its packet
//               (R-AUD-15, bench regression). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AlsaDirectSystem.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

// The period and period count asked for when the request names no buffer.
inline constexpr int kAlsaDefaultPeriodFrames = 128;
inline constexpr int kAlsaPeriods = 3;
// How long open() waits for the writer to start the card.
inline constexpr int kAlsaStartWaitMs = 1000;

// What the stream asks the PCM for.
struct AlsaPcmRequest {
    int rate = 48000;
    int channels = 2;
    int periodFrames = kAlsaDefaultPeriodFrames;
    int periods = kAlsaPeriods;
};

// What the PCM was set up with (the card's answers to the request).
struct AlsaPcmSetup {
    DeviceSampleFormat format = DeviceSampleFormat::Int16;
    int rate = 48000;
    int channels = 2;
    int periodFrames = kAlsaDefaultPeriodFrames;
    int bufferFrames = kAlsaDefaultPeriodFrames * kAlsaPeriods;
};

// One opened, prepared playback PCM.  The real one wraps snd_pcm_t; a
// test installs a fake.  writei, prepare, resume and delayFrames are
// called on the writer thread only; drop() from any thread.
class IAlsaPcm {
public:
    virtual ~IAlsaPcm() = default;           // snd_pcm_close
    virtual AlsaPcmSetup setup() const = 0;
    virtual long writei(const void* buffer, long frames) = 0;   // snd_pcm_writei: frames written, or -errno
    virtual int prepare() = 0;                                   // snd_pcm_prepare
    virtual int resume() = 0;                                    // snd_pcm_resume
    virtual long delayFrames() = 0;                              // snd_pcm_delay; -errno on failure
    virtual void drop() = 0;                                     // snd_pcm_drop: a blocked writei returns
};

struct AlsaPcmOpenResult {
    std::unique_ptr<IAlsaPcm> pcm;   // null on failure
    int error = 0;                   // -errno on failure (-EBUSY: another program holds the card)
    QString detail;                  // why, on failure
};

using AlsaPcmOpener = std::function<AlsaPcmOpenResult(const QString& pcmName,
                                                      const AlsaPcmRequest& request)>;

// "hw:<card>,<device>".
QString alsaPcmName(const AlsaCardRecord& record);

// The PCM a request asks for: 48000 Hz (the card's nearest is taken),
// channels enough for the request's pair (two at least, the card's count
// at most), the request's buffer frames as
// the period (kAlsaDefaultPeriodFrames when it names none), kAlsaPeriods.
AlsaPcmRequest alsaPcmRequest(const AlsaCardRecord& record, const AudioStreamRequest& request);

// The first format of alsaFormatOrder() that `accepts` takes.
std::optional<DeviceSampleFormat> alsaFirstAcceptedFormat(
    const std::function<bool(DeviceSampleFormat)>& accepts);

// What the writer does with a snd_pcm_writei result.
enum class AlsaWriteAction {
    Continue,   // frames written, or -EAGAIN: write again
    Prepare,    // -EPIPE: an underrun; snd_pcm_prepare and count it
    Resume,     // -ESTRPIPE: the card was suspended; resume, else prepare
    Lost,       // -ENODEV: the card went away; post DeviceLost
    Recover     // any other error: snd_pcm_prepare; lost when that fails
};
AlsaWriteAction alsaWriteAction(long result);

// The pair the stream plays on its `channels`: the request's pair when it
// fits, else the first pair (one channel on a one-channel card).
AudioChannelPair alsaStreamPair(AudioChannelPair requested, int channels);

class AlsaDirectBus final : public IAudioBus {
public:
    // `hold` is kept until the stream is destroyed (AlsaOutputMaker).
    AlsaDirectBus(AlsaCardRecord card, AudioStreamRequest request, AlsaPcmOpener opener,
                  std::shared_ptr<void> hold = {});
    ~AlsaDirectBus() override;   // close()

    AlsaDirectBus(const AlsaDirectBus&) = delete;
    AlsaDirectBus& operator=(const AlsaDirectBus&) = delete;

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override;

    qint64 push(const char* data, qint64 bytes) override;   // 48 kHz stereo float into the matcher
    qint64 pull(char*, qint64) override { return 0; }
    void flush() override;
    std::optional<OutputPacing> outputPacing() const override;

    float rxLevel() const override;
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("ALSA direct"); }
    AudioFormat negotiatedFormat() const override;
    QString errorString() const override;
    bool openRefusedInUse() const override;

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override;
    AudioDelayParts delayParts() const override;
    bool takesStereoMix() const override { return true; }
    std::optional<DeviceRateMatcherStats> matcherStats() const override;
    void restartClockMatch() override;
    void setClockMatchWritePacket(int frames, bool waited) override;
    void requestFadeOut() override;
    bool fadedOut() const override;

    // Underruns recovered with snd_pcm_prepare since open().  Any thread.
    int underruns() const;
    // What the PCM was set up with; nullopt while closed.
    std::optional<AlsaPcmSetup> pcmSetup() const;
    // The device latency read at open, in ns; -1 while closed.
    std::int64_t deviceLatencyNs() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// The real opener: snd_pcm_open on the hw: PCM and its hw and sw params.
// Every open fails while audioDevicesBarredForTestRun() is true.
AlsaPcmOpener makeAlsaHwPcmOpener();

} // namespace NereusSDR
