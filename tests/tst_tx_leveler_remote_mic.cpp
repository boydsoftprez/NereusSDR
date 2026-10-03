// =================================================================
// tests/tst_tx_leveler_remote_mic.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It cites Thetis wdsp/TXA.h and
// wdsp/TXA.c only for the meter indices and stage values it reads; no
// upstream logic is ported here.
//
// Leveler lane (2026-10-01): transmit audio from a remote microphone (the
// phone, a desktop remote window) through the real WDSP TX channel, offline,
// with no radio. Real speech (the RADE test recordings, 16 kHz, upsampled to
// 48 kHz) and 1 kHz / 150 Hz tone bursts go through each microphone path the
// Core has:
//
//   Local      the desktop's own microphone: 64-frame blocks straight into
//              the TX channel, as TxWorkerThread hands them over.
//   RemotePcm  a remote window's L16 line: 20 ms packets into RemoteMicFeed
//              (jitter buffer, rmatch), pulled 64 frames a pump block.
//   OpusPhone  the phone's encoder (NereusKit OpusEncoder.swift settings:
//              VOIP, 48 kbit/s constrained VBR, in-band FEC, 10 % expected
//              loss, DTX off, complexity 9, voice), RTP into
//              RemoteMicReceiver (the Core's decoder), then the feed.
//   PhoneSave  the same at 24 kbit/s, the phone's Save data choice (and
//              its only rate before the mic 48k lane).
//   OpusDesk   RemoteMicEncoder (the desktop remote window's encoder), then
//              the same receiver and feed.
//   PhoneStalls OpusPhone over a link that stalls 100 ms every 1.5 s and
//              then delivers what it held at once (feed underruns).
//   OpusAudioN candidates for the phone: the AUDIO application at N kbit/s,
//              the phone's other settings unchanged (measured only).
//
// Each run is one key on a fresh channel opened as the Core opens it (48 kHz
// in, 96 kHz DSP, 192 kHz out, LSB 100-2900 Hz) with the Rock's TX settings
// (Leveler max gain 15 dB, decay 100 ms; ALC max gain 3 dB, decay 10 ms;
// PROC 2 dB) at Mic Gain -3 or +10 dB. Measured on the I/Q: output peak,
// speech level, the level in the gaps, the share of samples at the ALC's
// ceiling, the leveler's gain (the TXA_LVLR_GAIN meter) and its movement,
// distortion per 20 ms speech frame against the same stream at Mic Gain
// -3 dB with the Leveler and PROC off, and THD+N on the tone bursts (the
// tone fitted at its own frequency). feedToneMatrix runs the feed alone on a
// steady tone: the ratio its clock matching runs at, and what is left once
// that pitch offset is taken out.
//
// remoteMicLevelsLikeLocalMic asserts that a remote microphone reaches the
// Leveler as the local one does: hot phone-level speech (peaks -6 dBFS) at
// Mic Gain +10 dB with the Leveler on. Two pairs, each differing only in what
// the Core does with the audio: the L16 remote feed against the local
// microphone (the jitter buffer and rmatch), and the phone's Opus over a link
// that stalls 100 ms every 1.5 s against the same Opus over a clean link (the
// underruns). In each pair the leveler's median and P90 gain stay within
// 1 dB, its movement per 10 ms (the pumping) within 0.1 dB, and no sample
// reaches the ALC's ceiling. The codec itself is the phone's choice and is
// measured in the matrix, not asserted here.
//
// phonePausesStayUnderTheSilencePeak re-measures the pause level the
// microphone feed's silence threshold (RemoteMicConfig::kSilencePeak) was
// set from: speech in 300 ms pieces with 200 ms pauses, over a microphone
// noise floor of -80 to -55 dBFS RMS, through each remote path. In the
// pauses, from 20 ms in (the feed's kSilenceRunMs), it measures each
// 64-frame block's peak; in the speech, the share of active blocks (input
// peak at least -26 dBFS) that the threshold would call silent. It asserts
// that at the phone's 48 kbit/s, over floors up to -60 dBFS, 99 % of pause
// blocks stay under the threshold, and that no more active speech blocks
// fall under it than on the L16 path (none at all for quiet speech). The
// floor is white and full band, the worst case for the full-band path. By
// default only those rows run; NEREUS_LEVELER_MATRIX=1 runs and prints the
// whole table (every floor, PhoneSave and OpusDesk too).
//
// NEREUS_LEVELER_MATRIX=1 prints the whole measurement matrix.
//
// NEREUS_LEVELER_HASH=1 runs one key (the 150 Hz bursts through the L16
// feed, Mic Gain +10 dB, Leveler and PROC on) and prints a hash of the TX
// channel's I/Q. Many processes at once show whether the channel's output
// repeats under CPU load; NEREUS_LEVELER_WISDOM names an FFTW wisdom file
// to import first, as the application does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: original test for NereusSDR by J.J. Boyd (KG4VCF), leveler
//               lane, with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-10-01: Mic 48k lane: OpusPhone at the phone's 48 kbit/s,
//               PhoneSave at its 24 kbit/s Save data, and
//               phonePausesStayUnderTheSilencePeak. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code. Fix wave: the lag
//               found once per path, and only the asserted rows by
//               default.
//   2026-10-01: Leveler meter sampling: the Leveler and ALC gain meters
//               are read once per DSP buffer, after the channel's worker
//               has finished it, so the leveler statistics no longer
//               depend on how late the worker runs. J.J. Boyd (KG4VCF),
//               AI-assisted via OpenAI Codex.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/wdsp_api.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteMicReceiver.h"

#include <opus.h>
#include <fftw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <numbers>
#include <random>
#include <thread>
#include <utility>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kBlock = RemoteMicConfig::kPumpBlockFrames;     // 64
constexpr int kPacket = RemoteMicConfig::kOpusFrameSamples;   // 960
constexpr int kInRate = 48000;
constexpr int kOutRate = 192000;
constexpr int kUp = kOutRate / kInRate;
constexpr quint32 kSsrc = 0x6d696301U;

// WDSP txaMeter indices, from the shared TxMeterType map (WdspTypes.h).
constexpr int kMeterLvlrGain = wdspTxaMeterIndex(TxMeterType::LevelerGain);
constexpr int kMeterAlcGain = wdspTxaMeterIndex(TxMeterType::AlcGain);

// One DSP buffer (2048 samples at 96 kHz, 21.33 ms) takes 1024 input frames
// at 48 kHz: 16 pump blocks. The meters change once per DSP buffer, when the
// channel's worker runs xtxa on it.
constexpr int kBufferFrames = WdspEngine::kTxDspBufferSize * kInRate / WdspEngine::kTxDspSampleRate;
constexpr int kBlocksPerBuffer = kBufferFrames / kBlock;
constexpr double kBufferMs = 1000.0 * WdspEngine::kTxDspBufferSize / WdspEngine::kTxDspSampleRate;
static_assert(kBlocksPerBuffer * kBlock == kBufferFrames, "a DSP buffer is whole pump blocks");

enum class Path { Local, RemotePcm, OpusPhone, OpusPhoneSaveData, OpusPhoneStalls, OpusDesk,
                  OpusAudio32, OpusAudio48, OpusAudio64 };

const char* pathName(Path p)
{
    switch (p) {
    case Path::Local: return "Local";
    case Path::RemotePcm: return "RemotePcm";
    case Path::OpusPhone: return "OpusPhone";
    case Path::OpusPhoneSaveData: return "PhoneSave";
    case Path::OpusPhoneStalls: return "PhoneStalls";
    case Path::OpusDesk: return "OpusDesk";
    case Path::OpusAudio32: return "OpusAudio32";
    case Path::OpusAudio48: return "OpusAudio48";
    case Path::OpusAudio64: return "OpusAudio64";
    }
    return "?";
}

// ---- Test audio ----

std::vector<float> readWav16(const QString& path, int* rate)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray d = f.readAll();
    auto u32 = [&](int at) {
        return static_cast<quint32>(static_cast<uint8_t>(d[at]))
            | (static_cast<quint32>(static_cast<uint8_t>(d[at + 1])) << 8)
            | (static_cast<quint32>(static_cast<uint8_t>(d[at + 2])) << 16)
            | (static_cast<quint32>(static_cast<uint8_t>(d[at + 3])) << 24);
    };
    std::vector<float> out;
    int at = 12;
    while (at + 8 <= d.size()) {
        const QByteArray id = d.mid(at, 4);
        const int size = static_cast<int>(u32(at + 4));
        if (id == "fmt ") {
            *rate = static_cast<int>(u32(at + 12));
        } else if (id == "data") {
            const int n = std::min(size, static_cast<int>(d.size()) - at - 8) / 2;
            out.resize(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                const auto lo = static_cast<uint8_t>(d[at + 8 + 2 * i]);
                const auto hi = static_cast<uint8_t>(d[at + 9 + 2 * i]);
                const auto s = static_cast<int16_t>(static_cast<uint16_t>(lo | (hi << 8)));
                out[static_cast<size_t>(i)] = static_cast<float>(s) / 32768.0f;
            }
            break;
        }
        at += 8 + size + (size & 1);
    }
    return out;
}

// 16 kHz to 48 kHz: zero-stuff by 3, then a Blackman-windowed sinc low-pass
// at 7.5 kHz.
std::vector<float> upsample3(const std::vector<float>& in)
{
    constexpr int kHalf = 96;
    std::vector<double> h(2 * kHalf + 1);
    const double fc = 7500.0 / 48000.0;
    for (int k = -kHalf; k <= kHalf; ++k) {
        const double x = 2.0 * fc * k;
        const double sinc = k == 0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
        const double w = 0.42 + 0.5 * std::cos(std::numbers::pi * k / kHalf)
            + 0.08 * std::cos(2.0 * std::numbers::pi * k / kHalf);
        h[static_cast<size_t>(k + kHalf)] = 2.0 * fc * sinc * w * 3.0;
    }
    const int n = static_cast<int>(in.size()) * 3;
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        double acc = 0.0;
        for (int k = -kHalf; k <= kHalf; ++k) {
            const int j = i - k;
            if (j >= 0 && j < n && j % 3 == 0) {
                acc += h[static_cast<size_t>(k + kHalf)] * in[static_cast<size_t>(j / 3)];
            }
        }
        out[static_cast<size_t>(i)] = static_cast<float>(acc);
    }
    return out;
}

// Speech at 48 kHz, peak 1.0: the RADE test talkers back to back.
const std::vector<float>& speechClip()
{
    static std::vector<float> clip;
    if (!clip.empty()) {
        return clip;
    }
    const QString dir = QStringLiteral(NEREUS_SOURCE_DIR "/third_party/rade/wav/");
    for (const char* name : {"peter.wav", "david_vk5dgr.wav"}) {
        int rate = 0;
        const std::vector<float> s = readWav16(dir + QLatin1String(name), &rate);
        if (s.empty() || rate != 16000) {
            clip.clear();
            return clip;
        }
        const std::vector<float> up = upsample3(s);
        clip.insert(clip.end(), up.begin(), up.end());
    }
    float peak = 0.0f;
    for (float v : clip) {
        peak = std::max(peak, std::abs(v));
    }
    for (float& v : clip) {
        v /= peak;
    }
    return clip;
}

// Tone bursts at 48 kHz, peak 1.0: 300 ms of silence (a -70 dBFS floor
// relative to the peak), then six of 500 ms on and 250 ms off.
constexpr int kBurstLead = 14400;
constexpr int kBurstOn = 24000;
constexpr int kBurstPeriod = 36000;
constexpr int kBursts = 6;
float noiseFloor(qint64 n)
{
    quint32 h = static_cast<quint32>(n) * 2654435761U;
    h ^= h >> 15;
    h *= 2246822519U;
    h ^= h >> 13;
    return 3.0e-4f * (static_cast<float>(h & 0xffffU) / 32768.0f - 1.0f);
}
std::vector<float> toneBursts(double hz)
{
    std::vector<float> s(static_cast<size_t>(kBurstLead + kBursts * kBurstPeriod));
    for (qint64 n = 0; n < static_cast<qint64>(s.size()); ++n) {
        const qint64 t = n - kBurstLead;
        float v = noiseFloor(n);
        if (t >= 0 && (t % kBurstPeriod) < kBurstOn) {
            v += static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * double(n) / kInRate));
        }
        s[static_cast<size_t>(n)] = v;
    }
    return s;
}

// ---- The microphone paths ----

struct FakeTime {
    qint64 nowMs = 0;
    std::multimap<qint64, std::function<void()>> due;
    RemoteMicReceiver::Clock clock()
    {
        return [this] { return nowMs; };
    }
    RemoteMicReceiver::Scheduler scheduler()
    {
        return [this](int ms, std::function<void()> fire) { due.emplace(nowMs + ms, std::move(fire)); };
    }
    void advanceTo(qint64 ms)
    {
        while (!due.empty() && due.begin()->first <= ms) {
            auto next = due.begin();
            nowMs = next->first;
            std::function<void()> fire = std::move(next->second);
            due.erase(next);
            fire();
        }
        nowMs = ms;
    }
};

// The phone's encoder, NereusKit OpusEncoder.swift:58-73 (claude/
// iphone-audioquality at bf72cdb30): 48 kbit/s, or 24 kbit/s under Save
// data (`voipBitrate`); `audioKbps` > 0 selects the AUDIO application at
// that rate instead.
constexpr int kPhoneMicBitrate = 48000;
constexpr int kPhoneMicSaveDataBitrate = 24000;
struct PhoneEncoder {
    OpusEncoder* enc{nullptr};
    std::vector<unsigned char> payload = std::vector<unsigned char>(1500);
    explicit PhoneEncoder(int audioKbps, int voipBitrate = kPhoneMicBitrate)
    {
        int error = OPUS_OK;
        enc = opus_encoder_create(kInRate, 1,
                                  audioKbps > 0 ? OPUS_APPLICATION_AUDIO : OPUS_APPLICATION_VOIP,
                                  &error);
        if (enc == nullptr) {
            return;
        }
        opus_encoder_ctl(enc, OPUS_SET_BITRATE(audioKbps > 0 ? audioKbps * 1000 : voipBitrate));
        opus_encoder_ctl(enc, OPUS_SET_VBR(1));
        opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(1));
        opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(1));
        opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(10));
        opus_encoder_ctl(enc, OPUS_SET_DTX(0));
        opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(9));
        opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    }
    ~PhoneEncoder()
    {
        if (enc != nullptr) {
            opus_encoder_destroy(enc);
        }
    }
    QByteArray encode(const float* mono, quint16 seq, quint32 ts)
    {
        const opus_int32 bytes = opus_encode_float(enc, mono, kPacket, payload.data(),
                                                   static_cast<opus_int32>(payload.size()));
        if (bytes <= 0) {
            return {};
        }
        return buildAudioRtp(RemoteMicConfig::kOpusPayloadType, seq, ts, kSsrc,
                             QByteArray(reinterpret_cast<const char*>(payload.data()), bytes));
    }
};

// The 48 kHz stream the TX channel receives on `path`, block by block, for
// `input` (already at its level): one 20 ms packet every 15 pump blocks for
// the remote paths, on simulated time, the feed's priming silence included.
std::vector<float> micStream(Path path, const std::vector<float>& input,
                             std::vector<double>* ratios = nullptr)
{
    if (path == Path::Local) {
        return input;
    }
    const int packets = static_cast<int>(input.size()) / kPacket;
    std::vector<float> out;
    out.reserve(static_cast<size_t>(packets) * kPacket);
    FakeTime time;
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed, nullptr, time.clock(), time.scheduler());
    std::unique_ptr<PhoneEncoder> phone;
    std::unique_ptr<RemoteMicEncoder> desk;
    switch (path) {
    case Path::OpusPhone:
    case Path::OpusPhoneStalls: phone = std::make_unique<PhoneEncoder>(0); break;
    case Path::OpusPhoneSaveData:
        phone = std::make_unique<PhoneEncoder>(0, kPhoneMicSaveDataBitrate);
        break;
    case Path::OpusAudio32: phone = std::make_unique<PhoneEncoder>(32); break;
    case Path::OpusAudio48: phone = std::make_unique<PhoneEncoder>(48); break;
    case Path::OpusAudio64: phone = std::make_unique<PhoneEncoder>(64); break;
    case Path::OpusDesk: desk = std::make_unique<RemoteMicEncoder>(); break;
    default: break;
    }
    if (path != Path::RemotePcm && !receiver.start(kSsrc, false)) {
        return {};
    }
    feed.setInUse(true);
    std::vector<float> block(kBlock);
    std::vector<QByteArray> held;
    for (int k = 0; k < packets; ++k) {
        const float* frame = input.data() + static_cast<size_t>(k) * kPacket;
        if (path == Path::RemotePcm) {
            feed.write(frame, kPacket);
        } else {
            const QByteArray rtp = phone
                ? phone->encode(frame, static_cast<quint16>(k), static_cast<quint32>(k * kPacket))
                : desk->encode(frame, static_cast<quint16>(k), static_cast<quint32>(k * kPacket), kSsrc);
            if (path == Path::OpusPhoneStalls && (k % 75) >= 70) {
                // The link stalls 100 ms every 1.5 s, then delivers what it
                // held at once (the feed runs dry: an underrun).
                held.push_back(rtp);
            } else {
                for (const QByteArray& late : held) {
                    receiver.submit(late);
                }
                held.clear();
                receiver.submit(rtp);
            }
        }
        for (int b = 0; b < kPacket / kBlock; ++b) {
            feed.pullBlock(block.data(), kBlock, -1.0);
            out.insert(out.end(), block.begin(), block.end());
        }
        if (ratios != nullptr) {
            ratios->push_back(feed.stats().ratio);
        }
        time.advanceTo(time.nowMs + 20);
    }
    return out;
}

// ---- One key through the TX channel ----

struct Key {
    double micGainDb{-3.0};
    bool leveler{true};
    bool proc{false};
};

struct TxRun {
    std::vector<float> envelope;    // |I/Q| at 192 kHz
    std::vector<std::complex<float>> iq;
    std::vector<double> lvlrGainDb; // per DSP buffer, once the worker has finished it
    std::vector<double> alcGainDb;  // per DSP buffer, once the worker has finished it
    bool metersFinal{false};        // every buffer's meters were read after its xtxa
};

} // namespace

class TestTxLevelerRemoteMic : public QObject {
    Q_OBJECT

public:
    static TxRun runTx(const std::vector<float>& mic, const Key& key, bool keepIq);

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void remoteMicLevelsLikeLocalMic();
    void phonePausesStayUnderTheSilencePeak();
    void measurementMatrix();
    void feedToneMatrix();
    void outputHash();
};

TxRun TestTxLevelerRemoteMic::runTx(const std::vector<float>& mic, const Key& key, bool keepIq)
{
    TxRun run;
#ifdef HAVE_WDSP
    WdspEngine engine;
    engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId, kBlock,
                                           WdspEngine::kTxDspBufferSize, kInRate,
                                           WdspEngine::kTxDspSampleRate, kOutRate);
    if (tx == nullptr) {
        return run;
    }
    const int ch = WdspEngine::kTxChannelId;
    TXASetSipMode(ch, 0);
    tx->setTxMode(DSPMode::LSB);
    tx->setTxBandpass(-2900, -100);
    // RadioModel's chain restore, with the Rock's values.
    tx->setTxLevelerOn(key.leveler);
    tx->setTxLevelerTopDb(15.0);
    tx->setTxLevelerDecayMs(100);
    tx->setTxAlcMaxGainDb(3.0);
    tx->setTxAlcDecayMs(10);
    tx->setTxCpdrOn(key.proc);
    tx->setTxCpdrGainDb(2.0);
    tx->setMicPreamp(std::pow(10.0, key.micGainDb / 20.0));
    SetChannelState(ch, 1, 0);
    // The meters are written by the channel's worker thread, which runs xtxa
    // on a DSP buffer after fexchange0 has already returned (dexchange
    // releases Sem_OutReady before xtxa, iobuffs.c and main.c), so a meter
    // read right after fexchange0 lands anywhere on the buffer's update
    // depending on how late the worker runs. They are read instead once per
    // DSP buffer, right after the call that completes its input, once the
    // worker's completed-block count (GetChannelDspLoad, bumped after xtxa
    // returns) shows that buffer done. The worker cannot start the next
    // buffer until this loop hands it 16 more blocks. dsplock.c publishes
    // load.blocks with release ordering and reads it with acquire ordering,
    // after xtxa (including both xmeter calls in txa.c), so the read is that
    // buffer's final value on every run.
    WdspChannelLoad load{};
    if (GetChannelDspLoad(ch, &load) < 0) {
        SetChannelState(ch, 0, 0);
        engine.destroyTxChannel(ch);
        return run;
    }
    const long long workerBlocksAtStart = load.blocks;
    bool metersFinal = true;
    const int blocks = static_cast<int>(mic.size()) / kBlock;
    std::vector<double> in(2 * kBlock);
    std::vector<double> out(2 * kBlock * kUp);
    run.envelope.reserve(static_cast<size_t>(blocks) * kBlock * kUp);
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < kBlock; ++i) {
            in[static_cast<size_t>(2 * i)] = mic[static_cast<size_t>(b * kBlock + i)];
            in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        int error = 0;
        std::fill(out.begin(), out.end(), 0.0);
        fexchange0(ch, in.data(), out.data(), &error);
        for (int k = 0; k < kBlock * kUp; ++k) {
            const double i = out[static_cast<size_t>(2 * k)];
            const double q = out[static_cast<size_t>(2 * k + 1)];
            run.envelope.push_back(static_cast<float>(std::hypot(i, q)));
            if (keepIq) {
                run.iq.emplace_back(static_cast<float>(i), static_cast<float>(q));
            }
        }
        if ((b + 1) % kBlocksPerBuffer != 0) {
            continue;
        }
        const long long buffersDone = workerBlocksAtStart + (b + 1) / kBlocksPerBuffer;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        int loadResult = GetChannelDspLoad(ch, &load);
        while (loadResult >= 0 && load.blocks < buffersDone
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            loadResult = GetChannelDspLoad(ch, &load);
        }
        if (loadResult < 0 || load.blocks != buffersDone) {
            metersFinal = false;
            break;
        }
        run.lvlrGainDb.push_back(GetTXAMeter(ch, kMeterLvlrGain));
        run.alcGainDb.push_back(GetTXAMeter(ch, kMeterAlcGain));
    }
    run.metersFinal = metersFinal;
    SetChannelState(ch, 0, 0);
    engine.destroyTxChannel(ch);
#else
    Q_UNUSED(mic);
    Q_UNUSED(key);
    Q_UNUSED(keepIq);
#endif
    return run;
}

namespace {

// ---- Measures ----

double pct(std::vector<double> v, double p)
{
    if (v.empty()) {
        return 0.0;
    }
    std::sort(v.begin(), v.end());
    const size_t i = static_cast<size_t>(std::clamp(p, 0.0, 1.0) * double(v.size() - 1));
    return v[i];
}

double toDb(double x) { return 20.0 * std::log10(x + 1e-12); }

// Distortion against a linear reference (the same stream at Mic Gain -3 dB
// with the Leveler and PROC off, where the ALC only lifts by its fixed 3 dB): the test output
// is aligned to the reference, and per 20 ms speech frame the best complex
// gain is fitted; what it leaves is distortion and fast gain movement. A
// gain that moves slowly within 20 ms leaves almost nothing.
struct Sdr {
    double p50{0.0};
    double p10{0.0};
    int lag{0};
};

Sdr sdrAgainst(const std::vector<std::complex<float>>& t,
               const std::vector<std::complex<float>>& r)
{
    Sdr out;
    const size_t n = std::min(t.size(), r.size());
    if (n < 192000) {
        return out;
    }
    // Lag (test behind reference), coarse on |x| at 12 kHz, then exact.
    constexpr int kDec = 16;
    std::vector<float> te(n / kDec);
    std::vector<float> re(n / kDec);
    for (size_t i = 0; i < te.size(); ++i) {
        te[i] = std::abs(t[i * kDec]);
        re[i] = std::abs(r[i * kDec]);
    }
    int best = 0;
    double bestC = -1.0;
    for (int lag = 0; lag < 800; ++lag) {
        double c = 0.0;
        for (size_t i = 0; i + static_cast<size_t>(lag) < te.size(); ++i) {
            c += double(te[i + static_cast<size_t>(lag)]) * re[i];
        }
        if (c > bestC) {
            bestC = c;
            best = lag;
        }
    }
    bestC = -1.0;
    int fine = best * kDec;
    for (int lag = std::max(0, best * kDec - kDec); lag <= best * kDec + kDec; ++lag) {
        std::complex<double> c{0.0, 0.0};
        for (size_t i = 0; i + static_cast<size_t>(lag) < n; i += 7) {
            c += std::complex<double>(t[i + static_cast<size_t>(lag)])
                * std::conj(std::complex<double>(r[i]));
        }
        if (std::abs(c) > bestC) {
            bestC = std::abs(c);
            fine = lag;
        }
    }
    out.lag = fine;
    constexpr size_t kFrame = 3840;
    std::vector<double> refDb;
    std::vector<double> sdr;
    for (size_t at = 57600; at + kFrame + static_cast<size_t>(fine) <= n; at += kFrame) {
        double rr = 0.0;
        for (size_t k = 0; k < kFrame; ++k) {
            rr += std::norm(std::complex<double>(r[at + k]));
        }
        refDb.push_back(10.0 * std::log10(rr / kFrame + 1e-30));
    }
    const double p95 = pct(refDb, 0.95);
    size_t idx = 0;
    for (size_t at = 57600; at + kFrame + static_cast<size_t>(fine) <= n; at += kFrame, ++idx) {
        if (refDb[idx] < p95 - 25.0) {
            continue;
        }
        std::complex<double> tr{0.0, 0.0};
        double rr = 0.0;
        for (size_t k = 0; k < kFrame; ++k) {
            const std::complex<double> rv(r[at + k]);
            tr += std::complex<double>(t[at + k + static_cast<size_t>(fine)]) * std::conj(rv);
            rr += std::norm(rv);
        }
        const std::complex<double> g = tr / rr;
        double res = 0.0;
        for (size_t k = 0; k < kFrame; ++k) {
            res += std::norm(std::complex<double>(t[at + k + static_cast<size_t>(fine)])
                             - g * std::complex<double>(r[at + k]));
        }
        sdr.push_back(10.0 * std::log10(std::norm(g) * rr / std::max(res, 1e-30)));
    }
    out.p50 = pct(sdr, 0.50);
    out.p10 = pct(sdr, 0.10);
    return out;
}

struct SpeechMeasure {
    double inPeakDb{0}, inGapDb{0};
    double peakDb{0}, speechDb{0}, gapDb{0}, s2gDb{0}, gapSpreadDb{0};
    double ceilingShare{0};          // share of speech samples at >= 0.98
    double lvlrP10{0}, lvlrP50{0}, lvlrP90{0};
    double lvlrMoveDbPer10ms{0};     // mean |change| of the leveler gain per 10 ms
    double alcP10{0};
    bool metersFinal{false};         // TxRun::metersFinal
};

std::vector<double> frameDb(const std::vector<float>& x, int frame)
{
    std::vector<double> f;
    for (size_t at = 0; at + static_cast<size_t>(frame) <= x.size(); at += static_cast<size_t>(frame)) {
        double p = 0.0;
        for (int k = 0; k < frame; ++k) {
            p += double(x[at + static_cast<size_t>(k)]) * x[at + static_cast<size_t>(k)];
        }
        f.push_back(10.0 * std::log10(p / frame + 1e-24));
    }
    return f;
}

SpeechMeasure measureSpeech(const std::vector<float>& mic, const TxRun& r)
{
    SpeechMeasure m;
    if (!r.metersFinal) {
        return m;
    }
    // Skip the first 300 ms (priming, the channel's start).
    const size_t skipIn = 14400;
    const std::vector<float> micBody(mic.begin() + static_cast<std::ptrdiff_t>(skipIn), mic.end());
    float inPeak = 0.0f;
    for (float v : micBody) {
        inPeak = std::max(inPeak, std::abs(v));
    }
    m.inPeakDb = toDb(inPeak);
    m.inGapDb = pct(frameDb(micBody, 480), 0.10);

    const size_t skipOut = skipIn * kUp;
    const std::vector<float> env(r.envelope.begin() + static_cast<std::ptrdiff_t>(skipOut),
                                 r.envelope.end());
    const std::vector<double> fr = frameDb(env, 1920);
    float peak = 0.0f;
    for (float v : env) {
        peak = std::max(peak, v);
    }
    m.peakDb = toDb(peak);
    const double p95 = pct(fr, 0.95);
    double sp = 0.0;
    int nsp = 0;
    size_t at = 0;
    quint64 speechSamples = 0;
    quint64 atCeiling = 0;
    for (double l : fr) {
        if (l > p95 - 20.0) {
            sp += std::pow(10.0, l / 10.0);
            ++nsp;
            for (size_t k = 0; k < 1920; ++k) {
                ++speechSamples;
                if (env[at + k] >= 0.98f) {
                    ++atCeiling;
                }
            }
        }
        at += 1920;
    }
    m.speechDb = 10.0 * std::log10(sp / std::max(1, nsp) + 1e-24);
    m.gapDb = pct(fr, 0.10);
    m.s2gDb = m.speechDb - m.gapDb;
    std::vector<double> gaps;
    const double p30 = pct(fr, 0.30);
    for (double l : fr) {
        if (l <= p30) {
            gaps.push_back(l);
        }
    }
    double mean = 0.0;
    for (double g : gaps) {
        mean += g;
    }
    mean /= std::max<size_t>(1, gaps.size());
    double var = 0.0;
    for (double g : gaps) {
        var += (g - mean) * (g - mean);
    }
    m.gapSpreadDb = std::sqrt(var / std::max<size_t>(1, gaps.size()));
    m.ceilingShare = speechSamples > 0 ? double(atCeiling) / double(speechSamples) : 0.0;

    // The meters are one value per DSP buffer; skip every buffer that
    // starts inside the first 300 ms.
    const size_t skipBuffers = std::min<size_t>((skipIn + kBufferFrames - 1) / kBufferFrames,
                                                r.lvlrGainDb.size());
    std::vector<double> lv(r.lvlrGainDb.begin() + static_cast<std::ptrdiff_t>(skipBuffers),
                           r.lvlrGainDb.end());
    m.lvlrP10 = pct(lv, 0.10);
    m.lvlrP50 = pct(lv, 0.50);
    m.lvlrP90 = pct(lv, 0.90);
    // The gain steps once per DSP buffer (21.33 ms): the mean |step| per
    // buffer, scaled to 10 ms.
    double move = 0.0;
    int nmove = 0;
    for (size_t i = 1; i < lv.size(); ++i) {
        move += std::abs(lv[i] - lv[i - 1]);
        ++nmove;
    }
    m.lvlrMoveDbPer10ms = nmove > 0 ? move / nmove * 10.0 / kBufferMs : 0.0;
    m.metersFinal = r.metersFinal;
    std::vector<double> alc(r.alcGainDb.begin() + static_cast<std::ptrdiff_t>(skipBuffers),
                            r.alcGainDb.end());
    m.alcP10 = pct(alc, 0.10);
    return m;
}

// THD+N of each burst's middle 200 ms, in dB, the worst of the bursts.
// The output's delay is found from the first burst's onset.
double burstThdN(const TxRun& r, double hz, double* fundamentalDb)
{
    float peak = 0.0f;
    for (float v : r.envelope) {
        peak = std::max(peak, v);
    }
    qint64 onset = -1;
    for (size_t k = 0; k < r.envelope.size(); ++k) {
        if (r.envelope[k] >= 0.3f * peak) {
            onset = static_cast<qint64>(k);
            break;
        }
    }
    if (onset < 0) {
        return 0.0;
    }
    const qint64 delay = onset - static_cast<qint64>(kBurstLead) * kUp;
    constexpr qint64 kWin = 38400;   // 200 ms at 192 kHz: whole cycles of 150 Hz and 1 kHz
    double worst = -200.0;
    double fund = 0.0;
    for (int b = 0; b < kBursts; ++b) {
        const qint64 mid = (static_cast<qint64>(kBurstLead) + b * kBurstPeriod + kBurstOn / 2) * kUp
            + delay;
        const qint64 from = mid - kWin / 2;
        if (from < 0 || from + kWin > static_cast<qint64>(r.iq.size())) {
            continue;
        }
        double total = 0.0;
        for (qint64 n = 0; n < kWin; ++n) {
            total += std::norm(std::complex<double>(r.iq[static_cast<size_t>(from + n)]));
        }
        total /= kWin;
        // The remote feed's clock matching shifts pitch by up to 500 ppm
        // (RemoteMicConfig::kMaxRatioPpm), so the tone is fitted at its own
        // frequency, searched over +-1000 ppm, not at exactly `hz`.
        double f = 0.0;
        for (int ppm = -1000; ppm <= 1000; ppm += 25) {
            const double fhz = hz * (1.0 + 1e-6 * ppm);
            std::complex<double> pos{0.0, 0.0};
            std::complex<double> neg{0.0, 0.0};
            for (qint64 n = 0; n < kWin; ++n) {
                const std::complex<double> z(r.iq[static_cast<size_t>(from + n)]);
                const double ph = 2.0 * std::numbers::pi * fhz * double(from + n) / kOutRate;
                const std::complex<double> rot = std::polar(1.0, ph);
                pos += z * std::conj(rot);
                neg += z * rot;
            }
            f = std::max(f, std::max(std::norm(pos), std::norm(neg)) / (double(kWin) * kWin));
        }
        const double thd = 10.0 * std::log10(std::max(total - f, 1e-30) / std::max(f, 1e-30));
        worst = std::max(worst, thd);
        fund = f;
    }
    if (fundamentalDb != nullptr) {
        *fundamentalDb = 10.0 * std::log10(fund + 1e-30);
    }
    return worst;
}

std::vector<float> scaled(const std::vector<float>& x, double peakDbfs)
{
    const float g = static_cast<float>(std::pow(10.0, peakDbfs / 20.0));
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] * g;
    }
    return y;
}

QString keyName(const Key& k)
{
    return QStringLiteral("gain %1 lev %2 proc %3")
        .arg(k.micGainDb, 3, 'f', 0)
        .arg(k.leveler ? QStringLiteral("on ") : QStringLiteral("off"))
        .arg(k.proc ? QStringLiteral("on ") : QStringLiteral("off"));
}

} // namespace

// A remote microphone levels as the local one does (the bounds are in the
// header comment).
void TestTxLevelerRemoteMic::remoteMicLevelsLikeLocalMic()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    const std::vector<float>& speech = speechClip();
    QVERIFY(!speech.empty());
    const std::vector<float> hot = scaled(speech, -6.0);
    const Key key{10.0, true, false};
    const auto measure = [&](Path path) {
        const std::vector<float> mic = micStream(path, hot);
        const TxRun r = runTx(mic, key, false);
        return measureSpeech(mic, r);
    };
    const std::pair<Path, Path> pairs[] = {{Path::Local, Path::RemotePcm},
                                           {Path::OpusPhone, Path::OpusPhoneStalls}};
    for (const auto& [basePath, path] : pairs) {
        const SpeechMeasure base = measure(basePath);
        const SpeechMeasure m = measure(path);
        const QByteArray what = QByteArray(pathName(path)) + " against "
            + QByteArray(pathName(basePath));
        qInfo().noquote() << QStringLiteral("LEVEL %1: lvlr p50 %2 / %3, p90 %4 / %5, "
                                            "move/10ms %6 / %7")
                                 .arg(QString::fromLatin1(what))
                                 .arg(m.lvlrP50, 0, 'f', 2).arg(base.lvlrP50, 0, 'f', 2)
                                 .arg(m.lvlrP90, 0, 'f', 2).arg(base.lvlrP90, 0, 'f', 2)
                                 .arg(m.lvlrMoveDbPer10ms, 0, 'f', 3)
                                 .arg(base.lvlrMoveDbPer10ms, 0, 'f', 3);
        QVERIFY2(base.metersFinal && m.metersFinal, what.constData());
        QVERIFY2(base.peakDb < 0.0 && m.peakDb < 0.0, what.constData());
        QVERIFY2(base.ceilingShare == 0.0 && m.ceilingShare == 0.0, what.constData());
        QVERIFY2(std::abs(m.lvlrP50 - base.lvlrP50) <= 1.0, what.constData());
        QVERIFY2(std::abs(m.lvlrP90 - base.lvlrP90) <= 1.0, what.constData());
        QVERIFY2(std::abs(m.lvlrMoveDbPer10ms - base.lvlrMoveDbPer10ms) <= 0.1,
                 what.constData());
    }
#endif
}

// The pause level after each remote path (the bounds are in the header
// comment).
void TestTxLevelerRemoteMic::phonePausesStayUnderTheSilencePeak()
{
    const std::vector<float>& speech = speechClip();
    QVERIFY(!speech.empty());
    constexpr int kWord = 14400;   // 300 ms
    constexpr int kPause = 9600;   // 200 ms
    constexpr int kRunFrames = RemoteMicConfig::kFramesPerMs * RemoteMicConfig::kSilenceRunMs;
    constexpr float kActivePeak = 0.05f;   // -26 dBFS
    const int words = static_cast<int>(speech.size()) / kWord;
    const bool print = qEnvironmentVariableIntValue("NEREUS_LEVELER_MATRIX") == 1;
    if (print) {
        qInfo().noquote() << "PAUSE path speechPk floorRms | pause block peak p50 p90 p99 max "
                             "| pause>=thr% | active<thr% | lag";
    }
    // A path's delay is constant (the codec's and the feed's priming: 636
    // frames for L16, 933 for Opus), so it is found once per path.
    std::map<Path, int> lags;
    for (const double speechPeakDb : {-6.0, -20.0}) {
        for (const double floorDb : {-80.0, -70.0, -60.0, -55.0}) {
            // By default only the asserted rows run: the phone's 48 kbit/s
            // over floors up to -60 dBFS, and L16 beside it where speech
            // blocks can fall under the threshold (speech at -6 dBFS). The
            // full table runs with NEREUS_LEVELER_MATRIX=1.
            const bool asserted = floorDb <= -60.0;
            if (!print && !asserted) {
                continue;
            }
            // Words and pauses, the floor under both (Gaussian, a fixed
            // seed, RMS at floorDb).
            std::vector<float> clean;
            std::vector<char> inPause;
            const float g = static_cast<float>(std::pow(10.0, speechPeakDb / 20.0));
            for (int w = 0; w < words; ++w) {
                for (int n = 0; n < kWord; ++n) {
                    clean.push_back(g * speech[static_cast<size_t>(w * kWord + n)]);
                    inPause.push_back(0);
                }
                for (int n = 0; n < kPause; ++n) {
                    clean.push_back(0.0f);
                    inPause.push_back(n >= kRunFrames ? 1 : 0);
                }
            }
            std::mt19937 rng(7);
            std::normal_distribution<float> gauss(0.0f,
                                                  static_cast<float>(std::pow(10.0, floorDb / 20.0)));
            std::vector<float> input(clean.size());
            for (size_t i = 0; i < clean.size(); ++i) {
                input[i] = clean[i] + gauss(rng);
            }
            double pcmActiveUnder = 0.0;
            for (const Path path : {Path::RemotePcm, Path::OpusPhoneSaveData, Path::OpusPhone,
                                    Path::OpusDesk}) {
                const bool needed = path == Path::OpusPhone
                    || (path == Path::RemotePcm && speechPeakDb > -10.0);
                if (!print && !needed) {
                    continue;
                }
                const std::vector<float> out = micStream(path, input);
                QVERIFY(!out.empty());
                // The path's delay: the lag that best matches the output to
                // the input over the first 4 s, the first time the path runs.
                if (lags.count(path) == 0) {
                    int found = 0;
                    double best = -1.0;
                    const size_t span = std::min<size_t>(192000, out.size() - 9600);
                    for (int l = 0; l < 9600; ++l) {
                        double c = 0.0;
                        for (size_t i = 0; i < span; i += 4) {
                            c += double(out[i + static_cast<size_t>(l)]) * input[i];
                        }
                        if (c > best) {
                            best = c;
                            found = l;
                        }
                    }
                    lags[path] = found;
                }
                const int lag = lags[path];
                std::vector<double> pausePeaks;
                int pauseOver = 0;
                int active = 0;
                int activeUnder = 0;
                for (size_t at = 0; at + kBlock + static_cast<size_t>(lag) <= out.size()
                     && at + kBlock <= input.size();
                     at += kBlock) {
                    bool pause = true;
                    float cleanPeak = 0.0f;
                    for (int k = 0; k < kBlock; ++k) {
                        pause = pause && inPause[at + static_cast<size_t>(k)] != 0;
                        cleanPeak = std::max(cleanPeak, std::abs(clean[at + static_cast<size_t>(k)]));
                    }
                    float peak = 0.0f;
                    for (int k = 0; k < kBlock; ++k) {
                        peak = std::max(peak, std::abs(out[at + static_cast<size_t>(lag + k)]));
                    }
                    if (pause) {
                        pausePeaks.push_back(toDb(peak));
                        pauseOver += peak >= RemoteMicConfig::kSilencePeak ? 1 : 0;
                    } else if (cleanPeak >= kActivePeak) {
                        ++active;
                        activeUnder += peak < RemoteMicConfig::kSilencePeak ? 1 : 0;
                    }
                }
                QVERIFY(!pausePeaks.empty() && active > 0);
                const double overPct = 100.0 * pauseOver / double(pausePeaks.size());
                const double underPct = 100.0 * activeUnder / double(active);
                if (print) {
                    qInfo().noquote() << QStringLiteral("PAUSE %1 %2 %3 | %4 %5 %6 %7 | %8 | %9 | %10")
                        .arg(QLatin1String(pathName(path)), -10)
                        .arg(speechPeakDb, 4, 'f', 0).arg(floorDb, 4, 'f', 0)
                        .arg(pct(pausePeaks, 0.50), 6, 'f', 1).arg(pct(pausePeaks, 0.90), 6, 'f', 1)
                        .arg(pct(pausePeaks, 0.99), 6, 'f', 1).arg(pct(pausePeaks, 1.0), 6, 'f', 1)
                        .arg(overPct, 6, 'f', 2).arg(underPct, 6, 'f', 2).arg(lag);
                }
                if (path == Path::RemotePcm) {
                    pcmActiveUnder = underPct;
                }
                if (path == Path::OpusPhone && asserted) {
                    const QByteArray what = QByteArray("speech ")
                        + QByteArray::number(speechPeakDb) + " floor "
                        + QByteArray::number(floorDb) + " p99 "
                        + QByteArray::number(pct(pausePeaks, 0.99));
                    QVERIFY2(toDb(RemoteMicConfig::kSilencePeak) > pct(pausePeaks, 0.99),
                             what.constData());
                    if (speechPeakDb > -10.0) {
                        QVERIFY2(underPct <= pcmActiveUnder + 0.5, what.constData());
                    } else {
                        // Quiet speech: no active block falls under on any
                        // path (0.00 % in the full table), L16 not run.
                        QVERIFY2(underPct == 0.0, what.constData());
                    }
                }
            }
        }
    }
}

// The measurement matrix (printed only with NEREUS_LEVELER_MATRIX=1).
void TestTxLevelerRemoteMic::measurementMatrix()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    if (qEnvironmentVariableIntValue("NEREUS_LEVELER_MATRIX") != 1) {
        QSKIP("set NEREUS_LEVELER_MATRIX=1 to print the measurement matrix");
    }
    const std::vector<float>& speech = speechClip();
    QVERIFY(!speech.empty());
    const QList<Path> paths = {Path::Local, Path::RemotePcm, Path::OpusPhone,
                               Path::OpusPhoneSaveData, Path::OpusPhoneStalls, Path::OpusDesk, Path::OpusAudio32,
                               Path::OpusAudio48, Path::OpusAudio64};
    const QList<Path> tonePaths = {Path::Local, Path::RemotePcm, Path::OpusPhone, Path::OpusDesk,
                                   Path::OpusAudio64};
    const QList<double> levels = {-20.0, -12.0, -6.0};
    const QList<double> gains = {-3.0, 10.0};
    qInfo().noquote() << "SPEECH path peakIn | key | inPk inGap | outPk speech gap s2g gapSd ceil% "
                         "| lvlr p10 p50 p90 move/10ms | alc p10 | sdr p50 p10 lag";
    for (Path path : paths) {
        for (double level : levels) {
            const std::vector<float> mic = micStream(path, scaled(speech, level));
            QVERIFY(!mic.empty());
            std::vector<std::complex<float>> reference;
            for (double gain : gains) {
                for (bool lev : {false, true}) {
                    for (bool proc : {false, true}) {
                        const Key key{gain, lev, proc};
                        TxRun r = runTx(mic, key, true);
                        if (gain < 0.0 && !lev && !proc) {
                            reference = r.iq;
                        }
                        const Sdr sdr = sdrAgainst(r.iq, reference);
                        r.iq.clear();
                        r.iq.shrink_to_fit();
                        QVERIFY(!r.envelope.empty());
                        QVERIFY(r.metersFinal);
                        const SpeechMeasure m = measureSpeech(mic, r);
                        qInfo().noquote() << QStringLiteral(
                            "SPEECH %1 %2 | %3 | %4 %5 | %6 %7 %8 %9 %10 %11 | %12 %13 %14 %15 | %16 | %17 %18 %19")
                            .arg(QLatin1String(pathName(path)), -11)
                            .arg(level, 4, 'f', 0)
                            .arg(keyName(key))
                            .arg(m.inPeakDb, 6, 'f', 1).arg(m.inGapDb, 6, 'f', 1)
                            .arg(m.peakDb, 6, 'f', 1).arg(m.speechDb, 6, 'f', 1)
                            .arg(m.gapDb, 6, 'f', 1).arg(m.s2gDb, 5, 'f', 1)
                            .arg(m.gapSpreadDb, 5, 'f', 1).arg(100.0 * m.ceilingShare, 6, 'f', 2)
                            .arg(m.lvlrP10, 5, 'f', 1).arg(m.lvlrP50, 5, 'f', 1)
                            .arg(m.lvlrP90, 5, 'f', 1).arg(m.lvlrMoveDbPer10ms, 5, 'f', 2)
                            .arg(m.alcP10, 5, 'f', 1)
                            .arg(sdr.p50, 5, 'f', 1).arg(sdr.p10, 5, 'f', 1).arg(sdr.lag);
                    }
                }
            }
        }
    }
    qInfo().noquote() << "TONE path hz peakIn | key | THD+N(worst burst) fund";
    for (Path path : tonePaths) {
        for (double hz : {150.0, 1000.0}) {
            for (double level : levels) {
                const std::vector<float> mic = micStream(path, scaled(toneBursts(hz), level));
                for (double gain : gains) {
                    for (bool lev : {false, true}) {
                        for (bool proc : {false, true}) {
                            const Key key{gain, lev, proc};
                            const TxRun r = runTx(mic, key, true);
                            QVERIFY(r.metersFinal);
                            double fund = 0.0;
                            const double thd = burstThdN(r, hz, &fund);
                            qInfo().noquote() << QStringLiteral("TONE %1 %2 %3 | %4 | %5 %6")
                                .arg(QLatin1String(pathName(path)), -11)
                                .arg(hz, 5, 'f', 0).arg(level, 4, 'f', 0)
                                .arg(keyName(key))
                                .arg(thd, 6, 'f', 1).arg(fund, 6, 'f', 1);
                        }
                    }
                }
            }
        }
    }
#endif
}

// The feed alone on a steady 1 kHz tone (printed only with
// NEREUS_LEVELER_MATRIX=1): the ratio rmatch runs at, the tone's frequency
// at the feed's output per 100 ms, and per 20 ms the residual against a
// sinusoid fitted at exactly 1 kHz and against one fitted at the frame's own
// frequency (what is left once the clock matching's pitch offset is taken
// out: noise and distortion).
void TestTxLevelerRemoteMic::feedToneMatrix()
{
    if (qEnvironmentVariableIntValue("NEREUS_LEVELER_MATRIX") != 1) {
        QSKIP("set NEREUS_LEVELER_MATRIX=1 to print the measurement matrix");
    }
    constexpr double kHz = 1000.0;
    constexpr int kSeconds = 10;
    std::vector<float> tone(static_cast<size_t>(kInRate * kSeconds));
    for (size_t n = 0; n < tone.size(); ++n) {
        tone[n] = static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * kHz * double(n) / kInRate));
    }
    qInfo().noquote() << "FEED path | ratio ppm min max last | freq ppm min p50 max "
                         "| resid@1k p50 p90 | resid@own p50 p90";
    for (Path path : {Path::Local, Path::RemotePcm, Path::OpusPhone}) {
        std::vector<double> ratios;
        const std::vector<float> y = micStream(path, tone, &ratios);
        QVERIFY(!y.empty());
        // Skip the first second (the feed's start).
        const size_t from = static_cast<size_t>(kInRate);
        std::vector<double> freqPpm;
        std::complex<double> prev{0.0, 0.0};
        constexpr size_t kWin = 4800;  // 100 ms
        for (size_t at = from; at + kWin <= y.size(); at += kWin) {
            std::complex<double> p{0.0, 0.0};
            for (size_t k = 0; k < kWin; ++k) {
                const double w = 2.0 * std::numbers::pi * kHz * double(at + k) / kInRate;
                p += double(y[at + k]) * std::complex<double>(std::cos(w), -std::sin(w));
            }
            if (std::abs(prev) > 0.0) {
                const double dphi = std::arg(p * std::conj(prev));
                freqPpm.push_back(1e6 * dphi / (2.0 * std::numbers::pi * 0.1) / kHz);
            }
            prev = p;
        }
        const auto residualDb = [&](size_t at, double hz) {
            constexpr size_t kFrame = 960;
            double c = 0.0;
            double sn = 0.0;
            double cc = 0.0;
            double ss = 0.0;
            double cs = 0.0;
            double e = 0.0;
            for (size_t k = 0; k < kFrame; ++k) {
                const double w = 2.0 * std::numbers::pi * hz * double(at + k) / kInRate;
                const double v = y[at + k];
                c += v * std::cos(w);
                sn += v * std::sin(w);
                cc += std::cos(w) * std::cos(w);
                ss += std::sin(w) * std::sin(w);
                cs += std::cos(w) * std::sin(w);
                e += v * v;
            }
            // Least squares on cos and sin.
            const double det = cc * ss - cs * cs;
            const double a = (c * ss - sn * cs) / det;
            const double b = (sn * cc - c * cs) / det;
            double r = 0.0;
            for (size_t k = 0; k < kFrame; ++k) {
                const double w = 2.0 * std::numbers::pi * hz * double(at + k) / kInRate;
                const double d = y[at + k] - a * std::cos(w) - b * std::sin(w);
                r += d * d;
            }
            return 10.0 * std::log10(std::max(r, 1e-30) / std::max(e, 1e-30));
        };
        std::vector<double> at1k;
        std::vector<double> atOwn;
        for (size_t at = from; at + 960 <= y.size(); at += 960) {
            at1k.push_back(residualDb(at, kHz));
            double best = 0.0;
            for (int step = -100; step <= 100; ++step) {
                const double r = residualDb(at, kHz + 0.02 * step);
                best = (step == -100) ? r : std::min(best, r);
            }
            atOwn.push_back(best);
        }
        double rMin = 1e9;
        double rMax = -1e9;
        for (size_t k = 50; k < ratios.size(); ++k) {
            rMin = std::min(rMin, 1e6 * (ratios[k] - 1.0));
            rMax = std::max(rMax, 1e6 * (ratios[k] - 1.0));
        }
        const double rLast = ratios.empty() ? 0.0 : 1e6 * (ratios.back() - 1.0);
        if (ratios.empty()) {
            rMin = 0.0;
            rMax = 0.0;
        }
        std::vector<double> fs = freqPpm;
        std::sort(fs.begin(), fs.end());
        qInfo().noquote() << QStringLiteral("FEED %1 | %2 %3 %4 | %5 %6 %7 | %8 %9 | %10 %11")
            .arg(QLatin1String(pathName(path)), -11)
            .arg(rMin, 7, 'f', 0).arg(rMax, 7, 'f', 0).arg(rLast, 7, 'f', 0)
            .arg(fs.front(), 7, 'f', 0).arg(pct(fs, 0.5), 7, 'f', 0).arg(fs.back(), 7, 'f', 0)
            .arg(pct(at1k, 0.5), 6, 'f', 1).arg(pct(at1k, 0.9), 6, 'f', 1)
            .arg(pct(atOwn, 0.5), 6, 'f', 1).arg(pct(atOwn, 0.9), 6, 'f', 1);
        QString trace;
        for (size_t k = 0; k < ratios.size() && k < 100; k += 5) {
            trace += QString::number(1e6 * (ratios[k] - 1.0), 'f', 0) + QLatin1Char(' ');
        }
        qInfo().noquote() << "FEED ratio ppm, every 100 ms of the first 2 s:" << trace;
    }
}

// One key's output hash (printed only with NEREUS_LEVELER_HASH=1).
void TestTxLevelerRemoteMic::outputHash()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    if (qEnvironmentVariableIntValue("NEREUS_LEVELER_HASH") != 1) {
        QSKIP("set NEREUS_LEVELER_HASH=1 to print the output hash");
    }
    const QByteArray wisdom = qgetenv("NEREUS_LEVELER_WISDOM");
    if (!wisdom.isEmpty()) {
        QVERIFY(fftw_import_wisdom_from_filename(wisdom.constData()) != 0);
    }
    const std::vector<float> mic = micStream(Path::RemotePcm, scaled(toneBursts(150.0), -12.0));
    QVERIFY(!mic.empty());
    const TxRun r = runTx(mic, Key{10.0, true, true}, true);
    QVERIFY(r.metersFinal);
    QVERIFY(!r.iq.empty());
    quint64 h = 1469598103934665603ULL;   // FNV-1a over the I/Q's bits
    for (const std::complex<float>& z : r.iq) {
        std::array<quint32, 2> bits{};
        std::memcpy(bits.data(), &z, sizeof(bits));
        for (quint32 w : bits) {
            h = (h ^ w) * 1099511628211ULL;
        }
    }
    qInfo().noquote() << "HASH" << QString::number(h, 16);
#endif
}

QTEST_GUILESS_MAIN(TestTxLevelerRemoteMic)
#include "tst_tx_leveler_remote_mic.moc"
