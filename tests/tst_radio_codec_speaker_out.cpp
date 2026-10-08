// no-port-check: NereusSDR-original unit-test file. The Thetis and mi0bot
// cite comments below document which upstream lines each assertion
// verifies; no upstream logic is ported in this file.
// =================================================================
// tests/tst_radio_codec_speaker_out.cpp  (NereusSDR)
// =================================================================
//
// Radio codec lane (2026-09-30): the receive audio reaches the radio's own
// speaker out.
//
//   P1: the first four bytes of each 8-byte EP2 sample carry L/R as 16-bit
//       big-endian, swapped on every board but the HL2 (Thetis
//       networkproto1.c:726-740 [v2.10.3.15]); on the HL2 swapped only
//       while Swap audio channels is on (mi0bot networkproto1.c:1231-1239
//       [@c26a8a4]).
//   P2: 260-byte packets to port 1028: a 4-byte big-endian sequence number,
//       then 64 L/R pairs as 16-bit big-endian, swapped when the model's
//       lr_audio_swap is set (network.c:1276-1294, 1363-1373 [v2.10.3.15]).
//   Engine: the radio output tap takes the station's program at its own
//       RADIO level, silence while the RADIO is muted; the speakers keep
//       the PC level and mute (R-SPK-01 to R-SPK-04). Thetis has one AF
//       volume for both (cmaster.cs:954-957 [v2.10.3.15]); NereusSDR
//       splits it.
//
// Nothing here keys a radio: MOX is never set. One case runs the P2 send
// thread against a loopback socket with the transmit ring empty.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec: created. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec review: the cushion's
//                                    decay, trim and re-prime, the
//                                    diagnostics counters, and the P2
//                                    sequence number across a restart.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  JJ's ruling: the radio tap takes
//                                    remote-owned slices while the local
//                                    output stays masked. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-06  J.J. Boyd / KG4VCF  Radio speaker plan Task 1 (R-SPK-01 to
//                                    R-SPK-04): the tap cases assert the
//                                    RADIO level and mute, independent of
//                                    PC. AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QScopeGuard>
#include <QUdpSocket>

#include "core/AudioEngine.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

// Thetis's conversion: floor(x * 32767 + 0.5) for x >= 0, else
// ceil(x * 32767 - 0.5). 0.5 -> 16384 (0x4000), -0.25 -> -8192 (0xE000).
constexpr float kLeft = 0.5f;
constexpr float kRight = -0.25f;
constexpr quint16 kLeftWire = 0x4000;
constexpr quint16 kRightWire = 0xE000;

std::vector<float> stereo(int frames, float left, float right)
{
    std::vector<float> v(static_cast<size_t>(frames) * 2);
    for (int i = 0; i < frames; ++i) {
        v[static_cast<size_t>(i) * 2] = left;
        v[static_cast<size_t>(i) * 2 + 1] = right;
    }
    return v;
}

quint16 be16(const QByteArray& b, int offset)
{
    return quint16((quint8(b[offset]) << 8) | quint8(b[offset + 1]));
}

// Pushes 3072 frames: past the cushion (the largest block, 1024, plus
// 20 ms) and under the trim point (the cushion plus a block and 20 ms).
void fill(RadioConnection& conn, float left, float right)
{
    const std::vector<float> block = stereo(1024, left, right);
    for (int i = 0; i < 3; ++i) {
        conn.pushRadioAudio(block.data(), 1024);
    }
}

class RecordingTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    std::vector<float> received;
};

// A model with one streaming slice at unity AF, the speakers on a fake
// 48 kHz stereo bus (passthrough), and the mix ramps cut to one frame.
struct EngineRig {
    EngineRig()
    {
        engine = radio.audioEngine();
        auto owned = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        owned->open(format);
        bus = owned.get();
        engine->setSpeakersBusForTest(std::move(owned));
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        radio.sliceById(slice)->setAfGain(100);
    }

    // The last float the speakers bus took.
    float speakersLast() const
    {
        const QByteArray& b = bus->buffer();
        if (b.size() < int(sizeof(float))) {
            return -1.0f;
        }
        float v = 0.0f;
        std::memcpy(&v, b.constData() + b.size() - int(sizeof(float)), sizeof(float));
        return v;
    }

    RadioModel radio;
    AudioEngine* engine = nullptr;
    FakeAudioBus* bus = nullptr;
    int slice = -1;
};

} // namespace

class TestRadioCodecSpeakerOut : public QObject {
    Q_OBJECT
private slots:

    // ── P1 ───────────────────────────────────────────────────────────────

    void p1_carriesRadioAudio()
    {
        P1RadioConnection conn;
        QVERIFY(conn.carriesRadioAudio());
    }

    // Thetis swaps L and R on every non-HL2 board: bytes 0-1 carry R.
    void p1_nonHl2_alwaysSwapsLr()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        fill(conn, kLeft, kRight);
        const QByteArray frame = conn.sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(frame.size(), 1032);
        for (int zone : {16, 528}) {
            for (int s = 0; s < 63; ++s) {
                const int o = zone + s * 8;
                QCOMPARE(be16(frame, o), kRightWire);
                QCOMPARE(be16(frame, o + 2), kLeftWire);
                // The I/Q bytes stay the transmit ring's (empty: zero).
                QCOMPARE(be16(frame, o + 4), quint16(0));
                QCOMPARE(be16(frame, o + 6), quint16(0));
            }
        }
    }

    // mi0bot: the HL2 sends L then R unless Swap audio channels is on.
    void p1_hl2_swapOff_sendsLThenR()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        fill(conn, kLeft, kRight);
        const QByteArray frame = conn.sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(be16(frame, 16), kLeftWire);
        QCOMPARE(be16(frame, 18), kRightWire);
        QCOMPARE(be16(frame, 528 + 62 * 8), kLeftWire);
    }

    void p1_hl2_swapOn_sendsRThenL()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2SwapAudioChannels(true);
        fill(conn, kLeft, kRight);
        const QByteArray frame = conn.sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(be16(frame, 16), kRightWire);
        QCOMPARE(be16(frame, 18), kLeftWire);
    }

    // Until the ring holds its cushion (the largest block plus 20 ms), the
    // L/R bytes stay zero.
    void p1_beforeCushion_lrBytesZero()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        const std::vector<float> block = stereo(1024, kLeft, kRight);
        conn.pushRadioAudio(block.data(), 1024);  // cushion is 1984
        const QByteArray frame = conn.sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(be16(frame, 16), quint16(0));
        QCOMPARE(be16(frame, 18), quint16(0));
    }

    // Full scale and past it: 1.0 -> 32767, -1.0 -> -32767, and a value
    // past full scale is held there.
    void p1_fullScale_heldAt32767()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // no swap
        fill(conn, 1.5f, -1.0f);
        const QByteArray frame = conn.sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(be16(frame, 16), quint16(0x7FFF));
        QCOMPARE(be16(frame, 18), quint16(0x8001));
    }

    // ── P2 ───────────────────────────────────────────────────────────────

    void p2_packetLayout_seqAndSamples()
    {
        P2RadioConnection conn;
        conn.setLrAudioSwapForTest(false);
        fill(conn, kLeft, kRight);
        P2RadioConnection::TxIqCapture capture;
        const int sent = conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture);
        // A 20 ms lead at 48 kHz is 15 packets of 64.
        QCOMPARE(sent, 15);
        QCOMPARE(capture.frames.size(), 15);
        for (int k = 0; k < capture.frames.size(); ++k) {
            const QByteArray& p = capture.frames.at(k);
            QCOMPARE(p.size(), 260);
            const quint32 seq = (quint32(quint8(p[0])) << 24) | (quint32(quint8(p[1])) << 16)
                | (quint32(quint8(p[2])) << 8) | quint32(quint8(p[3]));
            QCOMPARE(seq, quint32(k));
            for (int s = 0; s < 64; ++s) {
                QCOMPARE(be16(p, 4 + s * 4), kLeftWire);
                QCOMPARE(be16(p, 6 + s * 4), kRightWire);
            }
        }
    }

    void p2_lrAudioSwap_swapsPairs()
    {
        P2RadioConnection conn;
        conn.setLrAudioSwapForTest(true);
        fill(conn, kLeft, kRight);
        P2RadioConnection::TxIqCapture capture;
        conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture);
        QVERIFY(!capture.frames.isEmpty());
        const QByteArray& p = capture.frames.first();
        QCOMPARE(be16(p, 4), kRightWire);
        QCOMPARE(be16(p, 6), kLeftWire);
    }

    // The radio's buffer drains at 48 kHz: 4 ms later, 3 more packets fit
    // (192 frames), no more.
    void p2_pacing_followsElapsedTime()
    {
        P2RadioConnection conn;
        fill(conn, kLeft, kRight);
        P2RadioConnection::TxIqCapture capture;
        QCOMPARE(conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture), 15);
        QCOMPARE(conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture), 0);
        QCOMPARE(conn.serviceRadioAudioSendForTest(1'004'000'000LL, &capture), 3);
    }

    // ── The ring: cushion, trim, re-prime, counters ─────────────────────

    // One oversized block raises the cushion for about two seconds of
    // pushed audio, then it falls back to the blocks now arriving.
    void ring_cushionDecaysAfterAnOversizedBlock()
    {
        P2RadioConnection conn;
        const std::vector<float> big = stereo(4096, kLeft, kRight);
        const std::vector<float> small = stereo(256, kLeft, kRight);
        conn.pushRadioAudio(small.data(), 256);
        QCOMPARE(conn.radioAudioStats().cushionFrames, 256 + 960);
        conn.pushRadioAudio(big.data(), 4096);
        QCOMPARE(conn.radioAudioStats().cushionFrames, 4096 + 960);
        // Still held one window on (48000 frames pushed, ~1 s).
        for (int i = 0; i < 48000 / 256; ++i) {
            conn.pushRadioAudio(small.data(), 256);
        }
        QCOMPARE(conn.radioAudioStats().cushionFrames, 4096 + 960);
        // Gone after the second window.
        for (int i = 0; i < 48000 / 256 + 1; ++i) {
            conn.pushRadioAudio(small.data(), 256);
        }
        QCOMPARE(conn.radioAudioStats().cushionFrames, 256 + 960);
        // The full ring refused the rest, and said so.
        QVERIFY(conn.radioAudioStats().droppedFrames > 0);
    }

    // A ring deeper than the cushion plus a block and 20 ms is cut back to
    // the cushion; once it runs dry it waits for the cushion again.
    void ring_trimsDriftAndPrimesAgainAfterRunningDry()
    {
        P2RadioConnection conn;
        const std::vector<float> block = stereo(1024, kLeft, kRight);
        for (int i = 0; i < 5; ++i) {
            conn.pushRadioAudio(block.data(), 1024);  // 5120 > 1984 + 1024 + 960
        }
        P2RadioConnection::TxIqCapture capture;
        qint64 now = 1'000'000'000LL;
        QCOMPARE(conn.serviceRadioAudioSendForTest(now, &capture), 15);
        RadioConnection::RadioAudioStats st = conn.radioAudioStats();
        QCOMPARE(st.trimmedFrames, quint64(5120 - 1984));
        QCOMPARE(st.underruns, quint64(0));
        QCOMPARE(st.packetsSent, quint64(15));
        // 1984 - 960 = 1024 left: 16 more packets as the radio drains.
        int sent = 0;
        for (int pass = 0; pass < 4; ++pass) {
            now += 1'000'000'000LL;
            sent += conn.serviceRadioAudioSendForTest(now, &capture);
        }
        QCOMPARE(sent, 16);
        st = conn.radioAudioStats();
        QCOMPARE(st.underruns, quint64(1));
        QCOMPARE(st.packetsSent, quint64(31));
        QCOMPARE(st.sendErrors, quint64(0));
        // Primes again: one block (under the 1984 cushion) waits...
        conn.pushRadioAudio(block.data(), 1024);
        now += 1'000'000'000LL;
        QCOMPARE(conn.serviceRadioAudioSendForTest(now, &capture), 0);
        // ...and a second one starts the stream.
        conn.pushRadioAudio(block.data(), 1024);
        now += 1'000'000'000LL;
        QCOMPARE(conn.serviceRadioAudioSendForTest(now, &capture), 15);
        QCOMPARE(conn.radioAudioStats().underruns, quint64(1));
    }

    void ring_sendErrorsCountedAndStatsText()
    {
        P2RadioConnection conn;
        fill(conn, kLeft, kRight);
        P2RadioConnection::TxIqCapture capture;
        capture.refuse = 2;
        QCOMPARE(conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture), 13);
        const RadioConnection::RadioAudioStats st = conn.radioAudioStats();
        QVERIFY(st.valid);
        QCOMPARE(st.sendErrors, quint64(2));
        QCOMPARE(RadioConnection::radioAudioStatsText(st),
                 QStringLiteral("radioOut dropped=0 underruns=0 trimmed=0 cushion=1984"
                                " sent=13 sendErrors=2"));
        // P1 has no packet counters of its own.
        P1RadioConnection p1;
        const RadioConnection::RadioAudioStats p1st = p1.radioAudioStats();
        QVERIFY(p1st.valid);
        QVERIFY(!p1st.hasPackets);
        QCOMPARE(RadioConnection::radioAudioStatsText(p1st),
                 QStringLiteral("radioOut dropped=0 underruns=0 trimmed=0 cushion=960"));
        QCOMPARE(RadioConnection::radioAudioStatsText({}), QStringLiteral("radioOut=none"));
    }

    // Thetis zeroes rx_out_seq_no once, in create_rnet (netInterface.c:1492
    // [v2.10.3.15]); a restart of the send thread carries on counting.
    void p2_seqNo_notResetBySenderRestart()
    {
        QUdpSocket radio;
        QVERIFY(radio.bind(QHostAddress::LocalHost, 0));
        const quint16 port = radio.localPort();
        QVERIFY(port > 5);
        P2RadioConnection conn;
        conn.setPortBasesForTest(static_cast<quint16>(port - 5), 20000);
        conn.init();
        fill(conn, kLeft, kRight);
        P2RadioConnection::TxIqCapture capture;
        QCOMPARE(conn.serviceRadioAudioSendForTest(1'000'000'000LL, &capture), 15);
        conn.startTxIqSenderForTest(QHostAddress::LocalHost);
        conn.stopTxIqSenderForTest();
        fill(conn, kLeft, kRight);
        capture.frames.clear();
        QVERIFY(conn.serviceRadioAudioSendForTest(3'000'000'000LL, &capture) > 0);
        const QByteArray& p = capture.frames.first();
        const quint32 seq = (quint32(quint8(p[0])) << 24) | (quint32(quint8(p[1])) << 16)
            | (quint32(quint8(p[2])) << 8) | quint32(quint8(p[3]));
        QVERIFY2(seq >= 15, qPrintable(QString::number(seq)));
    }

    // ── Engine and model ─────────────────────────────────────────────────

    // R-SPK-01 to R-SPK-03: the radio output tap takes the program at the
    // RADIO level, the speakers at the PC level, each from the same mix.
    void engine_radioOutputTap_takesRadioLevel_speakersTakePcLevel()
    {
        EngineRig rig;
        AudioEngine* engine = rig.engine;
        RecordingTap tap;
        engine->setRadioOutputTap(&tap);
        auto clear = qScopeGuard([&] { engine->clearRadioOutputTap(&tap); });
        const std::vector<float> block = stereo(64, 0.5f, 0.5f);

        engine->setVolume(0.30f);
        engine->setRadioSpeakerVolume(0.80f);
        engine->rxBlockReady(rig.slice, block.data(), 64);
        QCOMPARE(tap.received.size(), size_t(128));
        QVERIFY(qAbs(tap.received.back() - 0.5f * 0.80f) < 1e-4f);
        QVERIFY(qAbs(rig.speakersLast() - 0.5f * 0.30f) < 1e-4f);

        // After clear the tap takes nothing.
        engine->clearRadioOutputTap(&tap);
        tap.received.clear();
        engine->rxBlockReady(rig.slice, block.data(), 64);
        QVERIFY(tap.received.empty());
    }

    // R-SPK-02: PC mute silences only the speakers; RADIO mute gives the
    // radio zero-filled blocks of the normal length, one per mixed block,
    // while the speakers play at the PC level.
    void engine_radioOutputTap_mutesAreIndependent()
    {
        EngineRig rig;
        AudioEngine* engine = rig.engine;
        RecordingTap tap;
        engine->setRadioOutputTap(&tap);
        auto clear = qScopeGuard([&] { engine->clearRadioOutputTap(&tap); });
        const std::vector<float> block = stereo(64, 0.5f, 0.5f);
        engine->setVolume(0.30f);
        engine->setRadioSpeakerVolume(0.80f);

        // PC muted, RADIO unmuted.
        engine->setMasterMuted(true);
        const int pushesBefore = rig.bus->pushCount();
        engine->rxBlockReady(rig.slice, block.data(), 64);
        QCOMPARE(rig.bus->pushCount(), pushesBefore);
        QCOMPARE(tap.received.size(), size_t(128));
        QVERIFY(qAbs(tap.received.back() - 0.5f * 0.80f) < 1e-4f);
        engine->setMasterMuted(false);

        // RADIO muted, PC unmuted: three blocks, three zero-filled blocks.
        tap.received.clear();
        engine->setRadioSpeakerMuted(true);
        for (int i = 0; i < 3; ++i) {
            engine->rxBlockReady(rig.slice, block.data(), 64);
            QCOMPARE(tap.received.size(), size_t(128) * size_t(i + 1));
            QVERIFY(qAbs(rig.speakersLast() - 0.5f * 0.30f) < 1e-4f);
        }
        for (const float v : tap.received) {
            QCOMPARE(v, 0.0f);
        }

        // Unmuted again: the RADIO level is back.
        engine->setRadioSpeakerMuted(false);
        tap.received.clear();
        engine->rxBlockReady(rig.slice, block.data(), 64);
        QVERIFY(qAbs(tap.received.back() - 0.5f * 0.80f) < 1e-4f);
    }

    // R-SPK-04: a slice's own AF level and mute act on both outputs alike.
    void engine_radioOutputTap_sliceLevelAndMuteActOnBoth()
    {
        EngineRig rig;
        AudioEngine* engine = rig.engine;
        RecordingTap tap;
        engine->setRadioOutputTap(&tap);
        auto clear = qScopeGuard([&] { engine->clearRadioOutputTap(&tap); });
        const std::vector<float> block = stereo(64, 0.5f, 0.5f);
        engine->setVolume(0.30f);
        engine->setRadioSpeakerVolume(0.80f);

        rig.radio.sliceById(rig.slice)->setAfGain(50);
        engine->rxBlockReady(rig.slice, block.data(), 64);
        engine->rxBlockReady(rig.slice, block.data(), 64);
        const float radioAt50 = tap.received.back();
        const float pcAt50 = rig.speakersLast();
        QVERIFY(radioAt50 > 0.0f && radioAt50 < 0.5f * 0.80f - 1e-3f);
        QVERIFY(qAbs(radioAt50 / 0.80f - pcAt50 / 0.30f) < 1e-4f);

        rig.radio.sliceById(rig.slice)->setMuted(true);
        tap.received.clear();
        engine->rxBlockReady(rig.slice, block.data(), 64);
        engine->rxBlockReady(rig.slice, block.data(), 64);
        QVERIFY(qAbs(tap.received.back()) < 1e-6f);
        QVERIFY(qAbs(rig.speakersLast()) < 1e-6f);
    }

    // R-SPK-01, R-SPK-02: PC and RADIO are separate values; setting one
    // never moves the other, and each signal fires only on a change.
    void engine_radioSpeakerSetters_independentOfPc()
    {
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        QSignalSpy volSpy(engine, &AudioEngine::radioSpeakerVolumeChanged);
        QSignalSpy muteSpy(engine, &AudioEngine::radioSpeakerMutedChanged);

        engine->setRadioSpeakerVolume(0.80f);
        engine->setRadioSpeakerMuted(true);
        QCOMPARE(volSpy.count(), 1);
        QCOMPARE(muteSpy.count(), 1);
        QCOMPARE(volSpy.at(0).at(0).toFloat(), 0.80f);
        QCOMPARE(muteSpy.at(0).at(0).toBool(), true);

        engine->setVolume(0.10f);
        engine->setMasterMuted(true);
        engine->setMasterMuted(false);
        QCOMPARE(engine->radioSpeakerVolume(), 0.80f);
        QCOMPARE(engine->radioSpeakerMuted(), true);

        engine->setRadioSpeakerVolume(0.25f);
        engine->setRadioSpeakerMuted(false);
        QCOMPARE(engine->volume(), 0.10f);
        QCOMPARE(engine->masterMuted(), false);

        // No change, no signal; out of range is clamped.
        volSpy.clear();
        muteSpy.clear();
        engine->setRadioSpeakerVolume(0.25f);
        engine->setRadioSpeakerMuted(false);
        QCOMPARE(volSpy.count(), 0);
        QCOMPARE(muteSpy.count(), 0);
        engine->setRadioSpeakerVolume(1.5f);
        QCOMPARE(engine->radioSpeakerVolume(), 1.0f);
        engine->setRadioSpeakerVolume(-0.5f);
        QCOMPARE(engine->radioSpeakerVolume(), 0.0f);
        QCOMPARE(volSpy.count(), 2);
    }

    // JJ's ruling (2026-09-30): with every slice owned by remote devices
    // (this computer's mask empty), the radio's speaker still carries their
    // audio, as Thetis's mixer 0, while the local output stays masked, at
    // the RADIO level; the RADIO mute gives zeros (R-SPK-01, R-SPK-02).
    void engine_radioOutputTap_takesRemoteOwnedSlices()
    {
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        bus->open(format);
        engine->setSpeakersBusForTest(std::move(bus));
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        const int slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        radio.sliceById(slice)->setAfGain(100);
        engine->setLocalOutputSliceMask(0u);

        RecordingTap tap;
        RecordingTap local;
        engine->setRadioOutputTap(&tap);
        engine->setMasterMixAudioTap(&local);
        auto clear = qScopeGuard([&] {
            engine->clearRadioOutputTap(&tap);
            engine->clearMasterMixAudioTap(&local);
        });
        const std::vector<float> block = stereo(64, 0.5f, 0.5f);

        engine->setVolume(0.30f);
        engine->setRadioSpeakerVolume(0.80f);
        engine->rxBlockReady(slice, block.data(), 64);
        QVERIFY(!tap.received.empty());
        QVERIFY(qAbs(tap.received.back() - 0.5f * 0.80f) < 1e-4f);
        QVERIFY(!local.received.empty());
        for (const float v : local.received) {
            QCOMPARE(v, 0.0f);
        }

        tap.received.clear();
        engine->setRadioSpeakerMuted(true);
        engine->rxBlockReady(slice, block.data(), 64);
        QCOMPARE(tap.received.size(), size_t(128));
        for (const float v : tap.received) {
            QCOMPARE(v, 0.0f);
        }
        engine->setRadioSpeakerMuted(false);
    }

    // RadioModel installs the tap for a connection that carries the audio,
    // and the receive audio reaches the P1 EP2 L/R bytes.
    void model_p1_receiveAudioReachesEp2()
    {
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        engine->setRadioSpeakerVolume(1.0f);  // R-SPK-01: the radio's own level
        const int slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        radio.sliceById(slice)->setAfGain(100);

        auto conn = std::make_unique<P1RadioConnection>();
        conn->setBoardForTest(HPSDRHW::HermesLite);  // no swap
        radio.injectConnectionForTest(conn.get());
        auto detach = qScopeGuard([&] {
            radio.unwireRadioSpeakerOutputForTest();
            radio.injectConnectionForTest(nullptr);
        });
        radio.wireRadioSpeakerOutputForTest();

        const std::vector<float> block = stereo(1024, kLeft, kRight);
        for (int i = 0; i < 4; ++i) {
            engine->rxBlockReady(slice, block.data(), 1024);
        }
        const QByteArray frame = conn->sendTxIqAndCapture(nullptr, 0);
        QCOMPARE(be16(frame, 16), kLeftWire);
        QCOMPARE(be16(frame, 18), kRightWire);
    }
};

QTEST_MAIN(TestRadioCodecSpeakerOut)
#include "tst_radio_codec_speaker_out.moc"
