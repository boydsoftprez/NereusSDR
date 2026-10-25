// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_display_codec_v2.cpp  (NereusSDR)
// =================================================================
//
// Display codec v2 (docs/architecture/2026-10-22-display-codec-v2-intra-rows.md).
//
// Task 1, the encoder (sections 4 to 6 and 8): hand-checked planes as
// literal bytes, the encoder's deterministic choices, the plane bound for
// every length, the frame layout, the one-message-or-two rule, the scene
// sizes of section 6, and the golden vector file, which every plane and
// frame entry must re-encode to exactly.
//
// Task 2, the decoder (sections 4.1, 4.2, 9.1, 9.2 and 10): the version
// dispatch, the malformed set as literal messages, truncation at every
// byte, structure before state, every row of the pairing table with its
// disposition, frame count and counters, the golden sequences, a loss and
// reordering simulation, and encode and decode benchmarks.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 encoder, bound, split rule, scene sizes and golden
//               vector checks. J.J. Boyd (KG4VCF), AI-assisted via Claude
//               Code.
//   2026-10-22: NSDC v2 decoder: dispatch, malformed set, pairing, golden
//               sequences, loss simulation and benchmarks. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest>

#include <QSet>

#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/IMediaTransport.h"

#include "DisplayCodecV2Vectors.h"

using namespace NereusSDR;
namespace Vectors = NereusSDR::Test::DisplayCodecV2Vectors;

namespace {

constexpr quint32 kHeader = DisplayCodecEncoder::kHeaderBytes;

QByteArray bytesOf(std::initializer_list<int> values)
{
    QByteArray out;
    for (int value : values) { out.append(static_cast<char>(value)); }
    return out;
}

QVector<quint8> qRow(std::initializer_list<int> values)
{
    QVector<quint8> out;
    for (int value : values) { out.append(static_cast<quint8>(value)); }
    return out;
}

QVector<quint8> noiseRow(int n, quint64 seed)
{
    Vectors::SplitMix64 rng(seed);
    QVector<quint8> out(n);
    for (int i = 0; i < n; ++i) { out[i] = rng.byte(); }
    return out;
}

/// Residual 0 for `period` samples, then +/-100 for `period`, from sample 1:
/// a row on which blocks of exactly `period` samples win.
QVector<quint8> burstRow(int n, int period)
{
    QVector<quint8> out(n);
    out[0] = 50;
    for (int i = 1; i < n; ++i) {
        const bool burst = ((i - 1) / period) % 2 == 1;
        const int step = burst ? ((i % 2) == 0 ? 100 : -100) : 0;
        out[i] = static_cast<quint8>(out.at(i - 1) + step);
    }
    return out;
}

DisplayCodecFrame frameOf(const QVector<quint8>& trace, const QVector<quint8>& waterfall,
                          const QVector<quint8>& wide, bool advance, quint32 sequence = 9)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 0x01020304U;
    frame.context.contextGeneration = 0x0A0B0C0DU;
    frame.context.minDbm = Vectors::kMinDbm;
    frame.context.maxDbm = Vectors::kMaxDbm;
    frame.context.traceSamples = static_cast<quint16>(trace.size());
    frame.context.waterfallSamples = static_cast<quint16>(waterfall.size());
    frame.context.wideSamples = static_cast<quint16>(wide.size());
    frame.encoderSequence = sequence;
    frame.producerTimestamp = 0x1122334455667788ULL;
    frame.waterfallAdvance = advance;
    frame.traceDbm = Vectors::dequantizeRow(trace);
    frame.waterfallDbm = Vectors::dequantizeRow(waterfall);
    frame.wideDbm = Vectors::dequantizeRow(wide);
    return frame;
}

quint32 u32At(const QByteArray& bytes, int offset)
{
    const auto* d = reinterpret_cast<const unsigned char*>(bytes.constData()) + offset;
    return (static_cast<quint32>(d[0]) << 24) | (static_cast<quint32>(d[1]) << 16)
        | (static_cast<quint32>(d[2]) << 8) | d[3];
}

quint16 u16At(const QByteArray& bytes, int offset)
{
    const auto* d = reinterpret_cast<const unsigned char*>(bytes.constData()) + offset;
    return static_cast<quint16>((static_cast<quint16>(d[0]) << 8) | d[1]);
}

quint8 flagsOf(const QByteArray& message)
{
    return static_cast<quint8>(message.at(5));
}

/// The header fields of a v2 message other than its flags, against `frame`.
void verifyHeader(const QByteArray& message, const DisplayCodecFrame& frame)
{
    QVERIFY(message.size() >= static_cast<int>(kHeader));
    QCOMPARE(u32At(message, 0), 0x4e534443U);
    QCOMPARE(static_cast<int>(static_cast<quint8>(message.at(4))), 2);
    QCOMPARE(u16At(message, 6), static_cast<quint16>(kHeader));
    QCOMPARE(u32At(message, 8), frame.context.endpointId);
    QCOMPARE(u32At(message, 12), frame.context.contextGeneration);
    QCOMPARE(u32At(message, 16), frame.encoderSequence);
    const quint64 timestamp = (static_cast<quint64>(u32At(message, 20)) << 32) | u32At(message, 24);
    QCOMPARE(timestamp, frame.producerTimestamp);
    quint32 minBits = 0;
    quint32 maxBits = 0;
    std::memcpy(&minBits, &frame.context.minDbm, sizeof(minBits));
    std::memcpy(&maxBits, &frame.context.maxDbm, sizeof(maxBits));
    QCOMPARE(u32At(message, 28), minBits);
    QCOMPARE(u32At(message, 32), maxBits);
    QCOMPARE(u16At(message, 36), frame.context.traceSamples);
    QCOMPARE(u16At(message, 38), frame.context.waterfallSamples);
    QCOMPARE(u16At(message, 40), frame.context.wideSamples);
}

/// Reads the planes after the header, in order, and requires the message to
/// end exactly at the last one.
void verifyPlanes(const QByteArray& message, const QList<QVector<quint8>>& expected)
{
    int offset = static_cast<int>(kHeader);
    for (const QVector<quint8>& plane : expected) {
        QVector<quint8> decoded;
        QVERIFY(decodeDisplayCodecV2Plane(message, offset, static_cast<int>(plane.size()), decoded));
        QCOMPARE(decoded, plane);
    }
    QCOMPARE(offset, static_cast<int>(message.size()));
}

QByteArray bitsToBytes(const QString& bits)
{
    QByteArray out((bits.size() + 7) / 8, '\0');
    for (int i = 0; i < bits.size(); ++i) {
        if (bits.at(i) == QLatin1Char('1')) {
            out[i / 8] = static_cast<char>(static_cast<unsigned char>(out.at(i / 8)) | (0x80U >> (i % 8)));
        }
    }
    return out;
}

QByteArray planeWithPayload(int blockCode, const QByteArray& payload)
{
    QByteArray plane = bytesOf({2, blockCode, static_cast<int>(payload.size() >> 8),
                                static_cast<int>(payload.size() & 0xff)});
    plane.append(payload);
    return plane;
}

constexpr quint32 v1Plane(quint32 n) { return displayCodecWorstCasePlaneBytes(n); }
constexpr quint32 v2Plane(quint32 n) { return displayCodecV2WorstCasePlaneBytes(n); }

// ---- Decoder helpers ----

constexpr quint32 kEndpointA = 0x00000101U; // trace 8, waterfall 6, no wide
constexpr quint32 kEndpointB = 0x00000202U; // trace 8, waterfall 6, wide 4
constexpr int kTraceLength = 8;
constexpr int kWaterfallLength = 6;
constexpr int kWideLength = 4;

/// A small frame whose planes differ by generation and sequence, so a plane
/// from the wrong frame never matches.
DisplayCodecFrame pairFrame(quint32 sequence, quint32 generation = 3, bool withWide = false)
{
    QVector<quint8> trace(kTraceLength);
    QVector<quint8> waterfall(kWaterfallLength);
    QVector<quint8> wide(withWide ? kWideLength : 0);
    for (int i = 0; i < trace.size(); ++i) {
        trace[i] = static_cast<quint8>(sequence * 41U + generation * 13U + i * 17U + 3U);
    }
    for (int i = 0; i < waterfall.size(); ++i) {
        waterfall[i] = static_cast<quint8>(sequence * 59U + generation * 5U + i * 31U + 90U);
    }
    for (int i = 0; i < wide.size(); ++i) {
        wide[i] = static_cast<quint8>(sequence * 23U + generation * 7U + i * 11U + 180U);
    }
    DisplayCodecFrame frame = frameOf(trace, waterfall, wide, true, sequence);
    frame.context.endpointId = withWide ? kEndpointB : kEndpointA;
    frame.context.contextGeneration = generation;
    frame.producerTimestamp = 1'000'000ULL * sequence + generation;
    return frame;
}

DisplayCodecFrame wideFrame(quint32 sequence, quint32 generation = 5)
{
    return pairFrame(sequence, generation, true);
}

QByteArray wholeMessage(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, frame.context.wideSamples != 0 ? quint8 {0x1E} : quint8 {0x1A});
}
QByteArray rowPartMessage(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, frame.context.wideSamples != 0 ? quint8 {0x36} : quint8 {0x32});
}
QByteArray tracePartMessage(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, quint8 {0x2A});
}
QByteArray traceOnlyMessage(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, quint8 {0x08});
}

QVector<float> zeroRow(int n)
{
    return Vectors::dequantizeRow(QVector<quint8>(n, 0));
}

/// The trace of `frame`, waterfall advance clear, its waterfall and wide the
/// given fills (a trace part whose row was lost, or a 0x08 frame).
DisplayCodecFrame traceOnlyRelease(const DisplayCodecFrame& frame, const QVector<float>& waterfall,
                                   const QVector<float>& wide)
{
    DisplayCodecFrame out = frame;
    out.waterfallAdvance = false;
    out.traceCarried = false;
    out.waterfallDbm = waterfall;
    out.wideDbm = wide;
    return out;
}

/// The row planes of `frame`, waterfall advance set, the trace carried.
DisplayCodecFrame rowOnlyRelease(const DisplayCodecFrame& frame, const QVector<float>& trace)
{
    DisplayCodecFrame out = frame;
    out.waterfallAdvance = true;
    out.traceCarried = true;
    out.traceDbm = trace;
    return out;
}

QString dispositionName(DisplayCodecDisposition disposition)
{
    switch (disposition) {
    case DisplayCodecDisposition::Accepted: return QStringLiteral("accepted");
    case DisplayCodecDisposition::NeedKeyframe: return QStringLiteral("needKeyframe");
    case DisplayCodecDisposition::Rejected: return QStringLiteral("rejected");
    case DisplayCodecDisposition::Held: return QStringLiteral("held");
    }
    return QString();
}

QString reasonName(DisplayCodecReason reason)
{
    switch (reason) {
    case DisplayCodecReason::None: return QStringLiteral("none");
    case DisplayCodecReason::InvalidInput: return QStringLiteral("invalidInput");
    case DisplayCodecReason::NoHistory: return QStringLiteral("noHistory");
    case DisplayCodecReason::SequenceGap: return QStringLiteral("sequenceGap");
    case DisplayCodecReason::StaleSequence: return QStringLiteral("staleSequence");
    case DisplayCodecReason::OldContext: return QStringLiteral("oldContext");
    case DisplayCodecReason::ContextMismatch: return QStringLiteral("contextMismatch");
    case DisplayCodecReason::BadMagic: return QStringLiteral("badMagic");
    case DisplayCodecReason::UnsupportedVersion: return QStringLiteral("unsupportedVersion");
    case DisplayCodecReason::UnknownFlags: return QStringLiteral("unknownFlags");
    case DisplayCodecReason::Truncated: return QStringLiteral("truncated");
    case DisplayCodecReason::Oversized: return QStringLiteral("oversized");
    case DisplayCodecReason::Malformed: return QStringLiteral("malformed");
    }
    return QString();
}

/// "accepted/none/1" and the like, for one QCOMPARE per result.
QString outcome(const DisplayCodecDecodeResult& result)
{
    return QStringLiteral("%1/%2/%3")
        .arg(dispositionName(result.disposition), reasonName(result.reason))
        .arg(result.frameCount);
}

DisplayCodecDecoderCounters counterValues(quint64 framesLost, quint64 partsLost,
                                          quint64 staleMessages, quint64 duplicateParts,
                                          quint64 malformed)
{
    DisplayCodecDecoderCounters counters;
    counters.framesLost = framesLost;
    counters.partsLost = partsLost;
    counters.staleMessages = staleMessages;
    counters.duplicateParts = duplicateParts;
    counters.malformed = malformed;
    return counters;
}

QString counterText(const DisplayCodecDecoderCounters& c)
{
    return QStringLiteral("framesLost %1 partsLost %2 staleMessages %3 duplicateParts %4 malformed %5")
        .arg(c.framesLost)
        .arg(c.partsLost)
        .arg(c.staleMessages)
        .arg(c.duplicateParts)
        .arg(c.malformed);
}

/// Empty when the two frames carry the same header fields, flags and
/// quantized planes; otherwise both, as JSON.
QString frameDifference(const DisplayCodecFrame& actual, const DisplayCodecFrame& expected)
{
    const QJsonObject a = Vectors::frameToJson(actual);
    const QJsonObject e = Vectors::frameToJson(expected);
    if (a == e) { return QString(); }
    return QStringLiteral("actual %1\nexpected %2")
        .arg(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)),
             QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact)));
}

/// What every v2 result must satisfy: no NeedKeyframe or NoHistory, and a
/// frame count of 1 or 2 exactly on Accepted.
bool wellFormed(const DisplayCodecDecodeResult& result)
{
    if (result.disposition == DisplayCodecDisposition::NeedKeyframe
        || result.reason == DisplayCodecReason::NoHistory) {
        return false;
    }
    if (result.disposition == DisplayCodecDisposition::Accepted) {
        return result.reason == DisplayCodecReason::None && result.frameCount >= 1
            && result.frameCount <= 2;
    }
    if (result.disposition == DisplayCodecDisposition::Held) {
        return result.reason == DisplayCodecReason::None && result.frameCount == 0;
    }
    return result.frameCount == 0;
}

// ---- Loss simulation ----

/// One scene run encoded frame by frame: each frame's quantized planes and
/// timestamp, and its messages.
struct EncodedRun {
    int pixels {0};
    std::vector<QVector<quint8>> traces;
    std::vector<QVector<quint8>> waterfalls;
    std::vector<quint64> timestamps;
    std::vector<std::vector<QByteArray>> messages;
    int splitFrames {0};
};

DisplayCodecFrame sceneFrame(int pixels, quint32 sequence, const QVector<float>& trace,
                             const QVector<float>& waterfall)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 7;
    frame.context.contextGeneration = 1;
    frame.context.minDbm = Vectors::kMinDbm;
    frame.context.maxDbm = Vectors::kMaxDbm;
    frame.context.traceSamples = static_cast<quint16>(pixels);
    frame.context.waterfallSamples = static_cast<quint16>(pixels);
    frame.encoderSequence = sequence;
    frame.producerTimestamp = 33'000'000ULL * sequence;
    frame.waterfallAdvance = true;
    frame.traceDbm = trace;
    frame.waterfallDbm = waterfall;
    return frame;
}

EncodedRun encodeRun(Vectors::Scene scene, int frames)
{
    EncodedRun run;
    Vectors::SceneRun source(scene);
    run.pixels = source.pixels();
    const quint32 admitted = spectrumDisplayCost(run.pixels, 30, false)->maximumFrameBytes;
    QVector<float> trace;
    QVector<float> waterfall;
    for (int f = 0; f < frames; ++f) {
        source.next(trace, waterfall);
        const DisplayCodecFrame frame =
            sceneFrame(run.pixels, static_cast<quint32>(f + 1), trace, waterfall);
        DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
        if (encoding.split()) { ++run.splitFrames; }
        run.traces.push_back(Vectors::quantizeRow(trace));
        run.waterfalls.push_back(Vectors::quantizeRow(waterfall));
        run.timestamps.push_back(frame.producerTimestamp);
        run.messages.push_back(std::move(encoding.messages));
    }
    return run;
}

enum class LossModel { GilbertElliott, Random };

/// Drops whole messages by datagram (ceil(bytes / 876) datagrams each), then
/// swaps adjacent delivered messages with probability 0.1, and checks every
/// released frame against the section 9.2 rules (see lossSimulation()).
void runLoss(const EncodedRun& run, LossModel model, quint64 seed)
{
    constexpr double kLossRate = 0.02;
    constexpr double kGoodFromBad = 1.0 / 3.0;                              // r: mean burst 3
    constexpr double kBadFromGood = kGoodFromBad * kLossRate / (1.0 - kLossRate); // p
    constexpr double kSwapProbability = 0.1;
    constexpr int kDatagramBytes = IMediaTransport::kSctpDataPayloadBytes;

    const int frames = static_cast<int>(run.messages.size());
    Vectors::SplitMix64 rng(seed);
    bool bad = false;
    std::vector<std::pair<QByteArray, int>> delivered;
    std::vector<bool> touched(static_cast<size_t>(frames), false);
    int droppedMessages = 0;
    for (int f = 0; f < frames; ++f) {
        for (const QByteArray& message : run.messages[static_cast<size_t>(f)]) {
            const int datagrams = static_cast<int>((message.size() + kDatagramBytes - 1) / kDatagramBytes);
            bool lost = false;
            for (int d = 0; d < datagrams; ++d) {
                if (model == LossModel::GilbertElliott) {
                    bad = bad ? rng.uniform() >= kGoodFromBad : rng.uniform() < kBadFromGood;
                    lost = lost || bad;
                } else if (rng.uniform() < kLossRate) {
                    lost = true;
                }
            }
            if (lost) {
                touched[static_cast<size_t>(f)] = true;
                ++droppedMessages;
            } else {
                delivered.emplace_back(message, f);
            }
        }
    }
    for (size_t i = 0; i + 1 < delivered.size();) {
        if (rng.uniform() < kSwapProbability) {
            std::swap(delivered[i], delivered[i + 1]);
            touched[static_cast<size_t>(delivered[i].second)] = true;
            touched[static_cast<size_t>(delivered[i + 1].second)] = true;
            i += 2;
        } else {
            ++i;
        }
    }
    QVERIFY(droppedMessages > 0);

    DisplayCodecDecoder decoder;
    std::vector<bool> complete(static_cast<size_t>(frames), false);
    QVector<quint8> lastTrace;
    QVector<quint8> lastWaterfall;
    qint64 first = -1;
    qint64 last = -1;
    quint64 released = 0;
    quint64 partials = 0;
    for (const auto& delivery : delivered) {
        const DisplayCodecDecodeResult result = decoder.decode(delivery.first);
        QVERIFY2(wellFormed(result), qPrintable(outcome(result)));
        if (result.disposition == DisplayCodecDisposition::Rejected) {
            // Only reordering can reject: a part or frame now older than L.
            QCOMPARE(reasonName(result.reason), QStringLiteral("staleSequence"));
            continue;
        }
        for (int k = 0; k < result.frameCount; ++k) {
            const DisplayCodecFrame& frame = k == 0 ? result.frame : result.secondFrame;
            const qint64 sequence = frame.encoderSequence;
            QVERIFY2(sequence > last, qPrintable(QStringLiteral("%1 after %2").arg(sequence).arg(last)));
            QVERIFY(sequence >= 1 && sequence <= frames);
            const auto f = static_cast<size_t>(sequence - 1);
            QCOMPARE(frame.producerTimestamp, run.timestamps[f]);
            const QVector<quint8> trace = Vectors::quantizeRow(frame.traceDbm);
            const QVector<quint8> waterfall = Vectors::quantizeRow(frame.waterfallDbm);
            QVERIFY(frame.wideDbm.isEmpty());
            const QVector<quint8> noRow(run.pixels, 0);
            if (frame.waterfallAdvance && !frame.traceCarried) {
                // Whole or merged: the encoder's quantized input.
                QVERIFY2(trace == run.traces[f], qPrintable(QString::number(sequence)));
                QVERIFY2(waterfall == run.waterfalls[f], qPrintable(QString::number(sequence)));
                complete[f] = true;
            } else if (frame.traceCarried) {
                // Row only: its row, the last released trace (q 0 when none).
                QVERIFY(frame.waterfallAdvance);
                QVERIFY2(waterfall == run.waterfalls[f], qPrintable(QString::number(sequence)));
                QVERIFY2(trace == (lastTrace.isEmpty() ? noRow : lastTrace),
                         qPrintable(QString::number(sequence)));
                ++partials;
            } else {
                // Trace only: its trace, the last released row (q 0 when none).
                QVERIFY2(trace == run.traces[f], qPrintable(QString::number(sequence)));
                QVERIFY2(waterfall == (lastWaterfall.isEmpty() ? noRow : lastWaterfall),
                         qPrintable(QString::number(sequence)));
                ++partials;
            }
            lastTrace = trace;
            lastWaterfall = waterfall;
            if (first < 0) { first = sequence; }
            last = sequence;
            ++released;
        }
    }

    const DisplayCodecDecoderCounters& counters = decoder.counters();
    QVERIFY(released > 0);
    QCOMPARE(counters.framesLost + released, static_cast<quint64>(last - first + 1));
    QCOMPARE(counters.partsLost, partials);
    QCOMPARE(counters.malformed, quint64 {0});
    QCOMPARE(counters.duplicateParts, quint64 {0});
    int untouched = 0;
    for (int f = 0; f < frames; ++f) {
        if (touched[static_cast<size_t>(f)]) { continue; }
        ++untouched;
        QVERIFY2(complete[static_cast<size_t>(f)],
                 qPrintable(QStringLiteral("frame %1 arrived intact but was not released whole")
                                .arg(f + 1)));
    }
    QVERIFY(untouched > 0);
    qInfo().noquote() << QStringLiteral(
                             "%1 px %2: %3 frames, %4 split, %5 messages dropped, %6 released, "
                             "%7 partial; %8")
                             .arg(run.pixels)
                             .arg(model == LossModel::GilbertElliott ? QStringLiteral("Gilbert-Elliott")
                                                                     : QStringLiteral("random"))
                             .arg(frames)
                             .arg(run.splitFrames)
                             .arg(droppedMessages)
                             .arg(released)
                             .arg(partials)
                             .arg(counterText(counters));
}

} // namespace

class TstDisplayCodecV2 : public QObject {
    Q_OBJECT

private slots:
    void quantizeHelpersRoundTrip();
    void handCheckedPlanes();
    void riceParameterTiesAndEscape();
    void eachBlockSizeCanWin();
    void planeDecoderRejectsMalformed();
    void boundArithmetic();
    void noiseRoundTripsWithinTheBoundForEveryLength();
    void structuredRowsRoundTrip();
    void encodingIsDeterministic();
    void invalidFramesEncodeNothing();
    void wholeFrameLayout();
    void splitRule();
    void framesStayWithinTheirBound();
    void sceneSizes_data();
    void sceneSizes();
    void sceneRowIsAPureFunction();
    void goldenVectorsReencode();

    // Task 2: the decoder.
    void versionDispatch();
    void malformedSet();
    void truncationAtEveryByte();
    void structureBeforeState();
    void pairingWholeFrames();
    void pairingSplitParts();
    void pairingAroundTheHeldPart();
    void pairingPartialFramesAndFills();
    void pairingGenerationChange();
    void resetClearsStateButNotCounters();
    void goldenSequencesDecode();
    void lossSimulation783();
    void lossSimulation4096();
    void benchmarkEncode_data();
    void benchmarkEncode();
    void benchmarkDecode_data();
    void benchmarkDecode();
};

void TstDisplayCodecV2::quantizeHelpersRoundTrip()
{
    for (int q = 0; q <= 255; ++q) {
        const float dbm = displayCodecDequantize(static_cast<quint8>(q), -150.0f, -30.0f);
        QCOMPARE(static_cast<int>(displayCodecQuantize(dbm, -150.0f, -30.0f)), q);
    }
    QCOMPARE(static_cast<int>(displayCodecQuantize(-200.0f, -150.0f, -30.0f)), 0);
    QCOMPARE(static_cast<int>(displayCodecQuantize(0.0f, -150.0f, -30.0f)), 255);
    QCOMPARE(displayCodecDequantize(0, -150.0f, -30.0f), -150.0f);
    QCOMPARE(displayCodecDequantize(255, -150.0f, -30.0f), -30.0f);
}

void TstDisplayCodecV2::handCheckedPlanes()
{
    // {100}: the first sample only.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({100})), bytesOf({0x02, 0x00, 0x00, 0x01, 0x64}));
    // {100, 101, 99}: z = 2 and 3. k = 1 (9 bits) ties k = 2 and wins as the
    // smaller; bits 01100100 001 010 011, then pad.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({100, 101, 99})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x64, 0x29, 0x80}));
    // {0, 255}: residual -1, z = 1. k = 0 (01) ties k = 1 (11) and wins;
    // bits 00000000 000 01, then pad.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 255})),
             bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x08}));

    for (const QVector<quint8>& row : {qRow({100}), qRow({100, 101, 99}), qRow({0, 255})}) {
        const QByteArray plane = encodeDisplayCodecV2Plane(row);
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY(decodeDisplayCodecV2Plane(plane, offset, static_cast<int>(row.size()), decoded));
        QCOMPARE(decoded, row);
        QCOMPARE(offset, static_cast<int>(plane.size()));
    }
}

void TstDisplayCodecV2::riceParameterTiesAndEscape()
{
    // {0, 32}: z = 64. k = 5 (00 1 00000) and k = 6 (01 000000) are both 8
    // bits, as is the escape: the smallest k wins and the escape is not
    // taken. Bits 00000000 101 00100000.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 32})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x00, 0xA4, 0x00}));
    // {0, 128}: residual -128, z = 255. Every k is more than 8 bits (k = 6 is
    // 10), so the escape: bits 00000000 111 10000000.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 128})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x00, 0xF0, 0x00}));
}

void TstDisplayCodecV2::eachBlockSizeCanWin()
{
    // Equal bytes at every size: the smallest (code 0).
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(qRow({100, 101, 99})).at(1)), 0);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(burstRow(513, 16)).at(1)), 0);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(burstRow(769, 32)).at(1)), 1);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(noiseRow(783, 77)).at(1)), 2);
}

void TstDisplayCodecV2::planeDecoderRejectsMalformed()
{
    const QByteArray good = encodeDisplayCodecV2Plane(qRow({100, 101, 99}));
    const auto rejects = [](const QByteArray& bytes, int length) {
        int offset = 0;
        QVector<quint8> samples = qRow({7});
        const bool ok = decodeDisplayCodecV2Plane(bytes, offset, length, samples);
        return !ok && offset == 0 && samples == qRow({7});
    };
    QVERIFY(!rejects(good, 3));

    QByteArray coding = good;
    coding[0] = 1;
    QVERIFY(rejects(coding, 3));
    coding[0] = 3;
    QVERIFY(rejects(coding, 3));
    QByteArray block = good;
    block[1] = 3;
    QVERIFY(rejects(block, 3));
    block[1] = static_cast<char>(0xFF);
    QVERIFY(rejects(block, 3));

    // A payload length past the end, and a prefix cut short.
    QByteArray overrun = good;
    overrun[3] = 4;
    QVERIFY(rejects(overrun, 3));
    QVERIFY(rejects(good.left(3), 3));
    for (int cut = 0; cut < good.size(); ++cut) { QVERIFY(rejects(good.left(cut), 3)); }

    // Payload bytes left after the last sample.
    QByteArray trailing = good;
    trailing[3] = 4;
    trailing.append('\0');
    QVERIFY(rejects(trailing, 3));
    // Fewer samples than coded leaves payload bytes too.
    QVERIFY(rejects(good, 2));
    // More samples than coded runs out of payload.
    QVERIFY(rejects(good, 4));

    // A nonzero pad bit: {0, 255} uses 13 bits of 16.
    QVERIFY(rejects(bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x09}), 2));
    QVERIFY(rejects(bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x0C}), 2));

    // z above 255: k = 0 with 256 zeros before the one.
    const QString zBits = QStringLiteral("00000000") + QStringLiteral("000")
        + QString(256, QLatin1Char('0')) + QStringLiteral("1");
    QVERIFY(rejects(planeWithPayload(0, bitsToBytes(zBits)), 2));
    // The same stream with 255 zeros is z = 255, which decodes.
    const QString maxBits = QStringLiteral("00000000") + QStringLiteral("000")
        + QString(255, QLatin1Char('0')) + QStringLiteral("1");
    int offset = 0;
    QVector<quint8> samples;
    QVERIFY(decodeDisplayCodecV2Plane(planeWithPayload(0, bitsToBytes(maxBits)), offset, 2, samples));
    QCOMPARE(samples, qRow({0, 128}));

    // Lengths outside 1..4096 and offsets outside the bytes.
    QVERIFY(rejects(good, 0));
    QVERIFY(rejects(good, DisplayCodecEncoder::kMaxSamplesPerPlane + 1));
    offset = static_cast<int>(good.size()) + 1;
    QVERIFY(!decodeDisplayCodecV2Plane(good, offset, 3, samples));
    offset = -1;
    QVERIFY(!decodeDisplayCodecV2Plane(good, offset, 3, samples));

    // A plane read at an offset advances past it only.
    QByteArray twoPlanes = good + encodeDisplayCodecV2Plane(qRow({0, 255}));
    offset = static_cast<int>(good.size());
    QVERIFY(decodeDisplayCodecV2Plane(twoPlanes, offset, 2, samples));
    QCOMPARE(samples, qRow({0, 255}));
    QCOMPARE(offset, static_cast<int>(twoPlanes.size()));

    QVERIFY(encodeDisplayCodecV2Plane({}).isEmpty());
    QVERIFY(encodeDisplayCodecV2Plane(QVector<quint8>(DisplayCodecEncoder::kMaxSamplesPerPlane + 1))
                .isEmpty());
}

void TstDisplayCodecV2::boundArithmetic()
{
    QCOMPARE(v2Plane(0), 0U);
    QCOMPARE(v2Plane(1), 5U);
    QCOMPARE(v2Plane(783), 792U);
    QCOMPARE(v2Plane(4096), 4124U);
    for (quint32 n = 1; n <= static_cast<quint32>(DisplayCodecEncoder::kMaxSamplesPerPlane); ++n) {
        const quint32 blocks = (n - 1 + 63) / 64;
        QCOMPARE(v2Plane(n), 4 + n + (3 * blocks + 7) / 8);
        QVERIFY2(v2Plane(n) <= v1Plane(n), qPrintable(QString::number(n)));
    }
    static_assert(displayCodecV2WorstCasePlaneBytes(783) == 792);
    static_assert(displayCodecV2WorstCasePlaneBytes(4096) == 4124);
}

void TstDisplayCodecV2::noiseRoundTripsWithinTheBoundForEveryLength()
{
    Vectors::SplitMix64 rng(1234);
    for (int n = 1; n <= DisplayCodecEncoder::kMaxSamplesPerPlane; ++n) {
        QVector<quint8> row(n);
        for (int i = 0; i < n; ++i) { row[i] = rng.byte(); }
        const QByteArray plane = encodeDisplayCodecV2Plane(row);
        QVERIFY2(static_cast<quint32>(plane.size()) <= v2Plane(static_cast<quint32>(n)),
                 qPrintable(QStringLiteral("n %1: %2 bytes").arg(n).arg(plane.size())));
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY2(decodeDisplayCodecV2Plane(plane, offset, n, decoded), qPrintable(QString::number(n)));
        QVERIFY2(decoded == row, qPrintable(QString::number(n)));
        QCOMPARE(offset, static_cast<int>(plane.size()));
    }
}

void TstDisplayCodecV2::structuredRowsRoundTrip()
{
    for (int n : {1, 2, 3, 16, 17, 64, 65, 129, 783, 1024, 4096}) {
        QVector<quint8> equal(n, 93);
        QVector<quint8> ramp(n);
        QVector<quint8> alternating(n);
        QVector<quint8> full(n, 255);
        for (int i = 0; i < n; ++i) {
            ramp[i] = static_cast<quint8>(i & 255);
            alternating[i] = (i % 2) == 0 ? 0 : 255;
        }
        for (const QVector<quint8>& row : {equal, ramp, alternating, full, burstRow(n, 16)}) {
            const QByteArray plane = encodeDisplayCodecV2Plane(row);
            QVERIFY(static_cast<quint32>(plane.size()) <= v2Plane(static_cast<quint32>(n)));
            int offset = 0;
            QVector<quint8> decoded;
            QVERIFY(decodeDisplayCodecV2Plane(plane, offset, n, decoded));
            QCOMPARE(decoded, row);
        }
    }
}

void TstDisplayCodecV2::encodingIsDeterministic()
{
    const QVector<quint8> row = noiseRow(783, 5);
    QCOMPARE(encodeDisplayCodecV2Plane(row), encodeDisplayCodecV2Plane(row));
    Vectors::SceneRun run(Vectors::Scene::PeakBins167);
    QVector<float> trace;
    QVector<float> waterfall;
    run.next(trace, waterfall);
    const DisplayCodecFrame frame =
        frameOf(Vectors::quantizeRow(trace), Vectors::quantizeRow(waterfall), {}, true);
    const quint32 admitted = spectrumDisplayCost(783, 30, false)->maximumFrameBytes;
    QCOMPARE(encodeDisplayCodecV2(frame, admitted).messages,
             encodeDisplayCodecV2(frame, admitted).messages);
}

void TstDisplayCodecV2::invalidFramesEncodeNothing()
{
    const DisplayCodecFrame good = frameOf(qRow({1, 2, 3}), qRow({4, 5, 6}), {}, true);
    QCOMPARE(encodeDisplayCodecV2(good, 10'000).messages.size(), size_t {1});

    DisplayCodecFrame frame = good;
    frame.traceDbm[1] = std::numeric_limits<float>::quiet_NaN();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.waterfallDbm.removeLast();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.maxDbm = frame.context.minDbm;
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.minDbm = std::numeric_limits<float>::infinity();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.traceSamples = 0;
    frame.traceDbm.clear();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.wideSamples = 2;
    frame.wideDbm = {-100.0f};
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.waterfallSamples = DisplayCodecEncoder::kMaxSamplesPerPlane + 1;
    frame.waterfallDbm = QVector<float>(DisplayCodecEncoder::kMaxSamplesPerPlane + 1, -100.0f);
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
}

void TstDisplayCodecV2::wholeFrameLayout()
{
    const QVector<quint8> trace = qRow({10, 20, 30, 40, 50});
    const QVector<quint8> waterfall = qRow({200, 190, 180, 170});
    const QVector<quint8> wide = qRow({1, 2, 3});

    // Trace and waterfall: 0x1A, planes in order, the message ending at the
    // waterfall plane.
    DisplayCodecFrame frame = frameOf(trace, waterfall, {}, true);
    DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QVERIFY(!encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace, waterfall});
    QCOMPARE(encoding.totalBytes(), encoding.messages[0].size());

    // With a wide row: 0x1E and three planes.
    frame = frameOf(trace, waterfall, wide, true);
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1E});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace, waterfall, wide});

    // No waterfall advance: 0x08, the trace only, the header still carrying
    // every row length.
    frame = frameOf(trace, waterfall, wide, false);
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x08});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace});

    // The exact bytes of a small whole frame.
    frame = frameOf(qRow({100}), qRow({0, 255}), {}, true, 0x01020304U);
    frame.producerTimestamp = 5;
    const QByteArray expected = bytesOf({0x4E, 0x53, 0x44, 0x43, 0x02, 0x1A, 0x00, 0x2A,
                                         0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D,
                                         0x01, 0x02, 0x03, 0x04,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
                                         0xC3, 0x16, 0x00, 0x00, 0xC1, 0xF0, 0x00, 0x00,
                                         0x00, 0x01, 0x00, 0x02, 0x00, 0x00,
                                         0x02, 0x00, 0x00, 0x01, 0x64,
                                         0x02, 0x00, 0x00, 0x02, 0x00, 0x08});
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(encoding.messages[0], expected);
}

void TstDisplayCodecV2::splitRule()
{
    static_assert(IMediaTransport::kSctpDataPayloadBytes == 876);
    QVector<float> traceDbm;
    QVector<float> waterfallDbm;

    // A 783-pixel peak-scene frame splits within the budget's admitted cost:
    // the row part (0x32) first, then the trace part (0x2A), both with the
    // frame's header fields.
    Vectors::SceneRun peak(Vectors::Scene::PeakBins167);
    peak.next(traceDbm, waterfallDbm);
    const QVector<quint8> trace = Vectors::quantizeRow(traceDbm);
    const QVector<quint8> waterfall = Vectors::quantizeRow(waterfallDbm);
    const quint32 admitted = spectrumDisplayCost(783, 30, false)->maximumFrameBytes;
    DisplayCodecFrame frame = frameOf(trace, waterfall, {}, true);
    DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
    QVERIFY(encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x32});
    QCOMPARE(flagsOf(encoding.messages[1]), quint8 {0x2A});
    verifyHeader(encoding.messages[0], frame);
    verifyHeader(encoding.messages[1], frame);
    verifyPlanes(encoding.messages[0], {waterfall});
    verifyPlanes(encoding.messages[1], {trace});
    QVERIFY(encoding.totalBytes() <= admitted);
    QCOMPARE(encoding.totalBytes(),
             encodeDisplayCodecV2(frame, 0).totalBytes() + static_cast<qsizetype>(kHeader));

    // With a wide row: 0x36 then 0x2A.
    Vectors::SceneRun wideRun(Vectors::Scene::PeakBins167, 768, 99);
    QVector<float> wideDbm;
    QVector<float> unused;
    wideRun.next(unused, wideDbm);
    const QVector<quint8> wide = Vectors::quantizeRow(wideDbm);
    frame = frameOf(trace, waterfall, wide, true);
    encoding = encodeDisplayCodecV2(frame, spectrumDisplayCost(783, 30, true)->maximumFrameBytes);
    QVERIFY(encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x36});
    QCOMPARE(flagsOf(encoding.messages[1]), quint8 {0x2A});
    verifyPlanes(encoding.messages[0], {waterfall, wide});
    verifyPlanes(encoding.messages[1], {trace});

    // Too big for one datagram but the parts over the admitted cost: whole.
    frame = frameOf(trace, waterfall, {}, true);
    const qsizetype whole = encodeDisplayCodecV2(frame, 0).totalBytes();
    QVERIFY(whole > IMediaTransport::kSctpDataPayloadBytes);
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(whole + kHeader - 1));
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(whole + kHeader));
    QVERIFY(encoding.split());

    // A flat 783-pixel frame fits one datagram whole.
    Vectors::SceneRun flat(Vectors::Scene::Flat);
    flat.next(traceDbm, waterfallDbm);
    frame = frameOf(Vectors::quantizeRow(traceDbm), Vectors::quantizeRow(waterfallDbm), {}, true);
    encoding = encodeDisplayCodecV2(frame, admitted);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    QVERIFY(encoding.totalBytes() <= IMediaTransport::kSctpDataPayloadBytes);

    // A 600-pixel uniform-noise frame admitted exactly its whole size stays
    // whole.
    frame = frameOf(noiseRow(600, 11), noiseRow(600, 12), {}, true);
    const qsizetype noiseWhole = encodeDisplayCodecV2(frame, 0).totalBytes();
    QVERIFY(noiseWhole > IMediaTransport::kSctpDataPayloadBytes);
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(noiseWhole));
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(encoding.totalBytes(), noiseWhole);

    // A trace-only frame is one message however large.
    frame = frameOf(noiseRow(4096, 13), noiseRow(4096, 14), {}, false);
    encoding = encodeDisplayCodecV2(frame, 100'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x08});
    QVERIFY(encoding.totalBytes() > IMediaTransport::kSctpDataPayloadBytes);
}

void TstDisplayCodecV2::framesStayWithinTheirBound()
{
    // Noise rows at every length, with wide 0 and 768: the total never
    // passes what the budget admits, and a whole message never passes the v1
    // keyframe of its shape.
    const QVector<quint8> wide768 = noiseRow(SpectrumEndpoint::kMaxWideSamples, 21);
    Vectors::SplitMix64 rng(1234);
    for (int n = 1; n <= DisplayCodecEncoder::kMaxSamplesPerPlane; ++n) {
        QVector<quint8> trace(n);
        QVector<quint8> waterfall(n);
        for (int i = 0; i < n; ++i) {
            trace[i] = rng.byte();
            waterfall[i] = rng.byte();
        }
        for (const bool withWide : {false, true}) {
            const QVector<quint8> wide = withWide ? wide768 : QVector<quint8>();
            const DisplayCodecFrame frame = frameOf(trace, waterfall, wide, true);
            const quint32 admitted = spectrumDisplayCost(n, 30, withWide)->maximumFrameBytes;
            const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
            const quint32 keyframe = kHeader + 2 * v1Plane(static_cast<quint32>(n))
                + v1Plane(static_cast<quint32>(wide.size()));
            QVERIFY2(!encoding.messages.empty(), qPrintable(QString::number(n)));
            QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(admitted),
                     qPrintable(QStringLiteral("n %1 wide %2").arg(n).arg(wide.size())));
            if (!encoding.split()) {
                QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(keyframe),
                         qPrintable(QString::number(n)));
            }
        }
    }

    // Trace 783 with every wide length: admitted by the budget (which
    // charges A(768) whatever the wide length) and by the frame's own shape.
    const QVector<quint8> trace = noiseRow(783, 31);
    const QVector<quint8> waterfall = noiseRow(783, 32);
    const quint32 budget = spectrumDisplayCost(783, 30, true)->maximumFrameBytes;
    for (int w = 1; w <= SpectrumEndpoint::kMaxWideSamples; ++w) {
        const DisplayCodecFrame frame = frameOf(trace, waterfall, noiseRow(w, 1000 + w), true);
        const quint32 own = kHeader + 2 * v1Plane(783) + v1Plane(static_cast<quint32>(w));
        for (const quint32 admitted : {budget, own}) {
            const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
            QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(admitted),
                     qPrintable(QStringLiteral("wide %1").arg(w)));
            for (const QByteArray& message : encoding.messages) {
                QVERIFY(static_cast<quint32>(message.size()) <= own);
            }
        }
    }
}

void TstDisplayCodecV2::sceneSizes_data()
{
    QTest::addColumn<int>("scene");
    QTest::addColumn<double>("bytes");
    using S = Vectors::Scene;
    QTest::newRow("peak 167") << static_cast<int>(S::PeakBins167) << 930.0;
    QTest::newRow("peak trace averaged") << static_cast<int>(S::PeakBins167TraceAveraged) << 784.0;
    QTest::newRow("average 167") << static_cast<int>(S::AverageBins167) << 757.0;
    QTest::newRow("average trace averaged") << static_cast<int>(S::AverageBins167TraceAveraged)
                                            << 710.0;
    QTest::newRow("peak IIR 0.3") << static_cast<int>(S::PeakIir) << 743.0;
    QTest::newRow("average 8") << static_cast<int>(S::AverageBins8) << 1064.0;
    QTest::newRow("average 8 trace averaged") << static_cast<int>(S::AverageBins8TraceAveraged)
                                              << 949.0;
    QTest::newRow("raw") << static_cast<int>(S::Raw) << 1312.0;
    QTest::newRow("uniform noise") << static_cast<int>(S::UniformNoise) << 1668.0;
    QTest::newRow("flat") << static_cast<int>(S::Flat) << 258.0;
    QTest::newRow("ramp") << static_cast<int>(S::Ramp) << 386.0;
    QTest::newRow("1024 px average") << static_cast<int>(S::Average1024) << 988.0;
    QTest::newRow("4096 px peak") << static_cast<int>(S::Peak4096) << 4821.0;
}

void TstDisplayCodecV2::sceneSizes()
{
    QFETCH(int, scene);
    QFETCH(double, bytes);
    constexpr int kFrames = 300;
    Vectors::SceneRun run(static_cast<Vectors::Scene>(scene));
    const int pixels = run.pixels();
    const quint32 admitted = spectrumDisplayCost(pixels, 30, false)->maximumFrameBytes;
    QVector<float> trace;
    QVector<float> waterfall;
    double total = 0.0;
    for (int f = 0; f < kFrames; ++f) {
        run.next(trace, waterfall);
        DisplayCodecFrame frame;
        frame.context.endpointId = 1;
        frame.context.contextGeneration = 1;
        frame.context.minDbm = Vectors::kMinDbm;
        frame.context.maxDbm = Vectors::kMaxDbm;
        frame.context.traceSamples = static_cast<quint16>(pixels);
        frame.context.waterfallSamples = static_cast<quint16>(pixels);
        frame.encoderSequence = static_cast<quint32>(f + 1);
        frame.producerTimestamp = static_cast<quint64>(f + 1);
        frame.waterfallAdvance = true;
        frame.traceDbm = trace;
        frame.waterfallDbm = waterfall;
        const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
        QVERIFY(!encoding.messages.empty());
        QVERIFY(encoding.totalBytes() <= static_cast<qsizetype>(admitted));
        total += static_cast<double>(encoding.totalBytes());
    }
    const double mean = total / kFrames;
    QVERIFY2(mean <= 1.03 * bytes,
             qPrintable(QStringLiteral("mean %1 bytes against %2").arg(mean, 0, 'f', 1).arg(bytes)));
}

void TstDisplayCodecV2::sceneRowIsAPureFunction()
{
    using S = Vectors::Scene;
    QCOMPARE(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17),
             Vectors::sceneRow(S::PeakBins167, 783, 1234, 17));
    QVERIFY(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17)
            != Vectors::sceneRow(S::PeakBins167, 783, 1234, 18));
    QVERIFY(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17)
            != Vectors::sceneRow(S::PeakBins167, 783, 1235, 17));
    // splitmix64's first outputs from 0 (its published reference values).
    Vectors::SplitMix64 rng(0);
    QCOMPARE(rng.next(), 0xE220A8397B1DCDAFULL);
    QCOMPARE(rng.next(), 0x6E789E6AA1B965F4ULL);
}

void TstDisplayCodecV2::goldenVectorsReencode()
{
    QJsonObject document;
    QString error;
    QVERIFY2(Vectors::readVectors(Vectors::vectorsPath(), document, error), qPrintable(error));

    const QJsonArray planes = document.value(QStringLiteral("planes")).toArray();
    QSet<QString> planeNames;
    QSet<int> blockSizes;
    for (const QJsonValue& value : planes) {
        const QJsonObject plane = value.toObject();
        const QString name = plane.value(QStringLiteral("name")).toString();
        planeNames.insert(name);
        const QVector<quint8> samples =
            Vectors::fromJsonArray(plane.value(QStringLiteral("samples")).toArray());
        const QByteArray encoded = encodeDisplayCodecV2Plane(samples);
        QVERIFY2(Vectors::toHex(encoded) == plane.value(QStringLiteral("plane")).toString(),
                 qPrintable(name));
        const int blockSize = plane.value(QStringLiteral("blockSize")).toInt();
        QCOMPARE(blockSize, 16 << static_cast<int>(encoded.at(1)));
        blockSizes.insert(blockSize);
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY2(decodeDisplayCodecV2Plane(encoded, offset, static_cast<int>(samples.size()), decoded),
                 qPrintable(name));
        QCOMPARE(decoded, samples);
    }
    for (const char* name : {"hand-one-sample", "hand-three-samples", "hand-wrap"}) {
        QVERIFY2(planeNames.contains(QLatin1String(name)), name);
    }
    QCOMPARE(blockSizes, (QSet<int> {16, 32, 64}));

    const QJsonArray frames = document.value(QStringLiteral("frames")).toArray();
    QSet<QList<int>> flagShapes;
    for (const QJsonValue& value : frames) {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const DisplayCodecFrame frame =
            Vectors::frameFromJson(entry.value(QStringLiteral("frame")).toObject());
        const auto admitted =
            static_cast<quint32>(entry.value(QStringLiteral("admittedFrameBytes")).toInteger());
        const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
        const QJsonArray expected = entry.value(QStringLiteral("messages")).toArray();
        QVERIFY2(Vectors::toJsonHexArray(encoding.messages) == expected, qPrintable(name));
        QList<int> shape;
        for (const QByteArray& message : encoding.messages) {
            shape.append(flagsOf(message));
            // The builder the hand-stated sequences use writes the same bytes.
            QVERIFY2(Vectors::buildMessage(frame, flagsOf(message)) == message, qPrintable(name));
        }
        flagShapes.insert(shape);
    }
    for (const QList<int>& shape : {QList<int> {0x08}, QList<int> {0x1A}, QList<int> {0x1E},
                                    QList<int> {0x32, 0x2A}, QList<int> {0x36, 0x2A}}) {
        QVERIFY(flagShapes.contains(shape));
    }

    const QJsonArray sequences = document.value(QStringLiteral("sequences")).toArray();
    QVERIFY(!sequences.isEmpty());
    for (const QJsonValue& value : sequences) {
        const QJsonObject entry = value.toObject();
        QCOMPARE(entry.value(QStringLiteral("messages")).toArray().size(),
                 entry.value(QStringLiteral("results")).toArray().size());
        QVERIFY(entry.value(QStringLiteral("counters")).isObject());
    }
}

// ---- Task 2: the decoder ----

void TstDisplayCodecV2::versionDispatch()
{
    // Version 1 takes today's path: one frame, no counter moves, even for its
    // own rejects. Version 2 takes the v2 path. The two keep separate state.
    DisplayCodecDecoder decoder;
    DisplayCodecEncoder encoder;
    DisplayCodecFrame v1Frame = frameOf(qRow({10, 20, 30, 40}), qRow({50, 60, 70, 80}), {}, true, 1);
    const QByteArray keyframe = encoder.encode(v1Frame);
    QVERIFY(!keyframe.isEmpty());
    DisplayCodecDecodeResult result = decoder.decode(keyframe);
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    QVERIFY(result.secondFrame.traceDbm.isEmpty());
    QCOMPARE(outcome(decoder.decode(keyframe)), QStringLiteral("rejected/staleSequence/0"));
    QByteArray v1Malformed = keyframe;
    v1Malformed.append('\0');
    QCOMPARE(outcome(decoder.decode(v1Malformed)), QStringLiteral("rejected/malformed/0"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 0)));

    // A v2 message of another endpoint and generation between two v1 frames
    // disturbs neither path.
    result = decoder.decode(wholeMessage(pairFrame(100, 9)));
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    v1Frame.encoderSequence = 2;
    v1Frame.traceDbm = Vectors::dequantizeRow(qRow({11, 21, 31, 41}));
    const QByteArray delta = encoder.encode(v1Frame);
    QVERIFY(!encoder.lastEncodedKeyframe());
    result = decoder.decode(delta);
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    QCOMPARE(Vectors::quantizeRow(result.frame.traceDbm), qRow({11, 21, 31, 41}));
    result = decoder.decode(wholeMessage(pairFrame(101, 9)));
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 0)));

    // reset() clears both paths: v1 needs a keyframe again, and the v2
    // context is gone (an older generation is now a first message).
    decoder.reset();
    v1Frame.encoderSequence = 3;
    QCOMPARE(outcome(decoder.decode(encoder.encode(v1Frame))),
             QStringLiteral("needKeyframe/noHistory/0"));
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(1, 8)))), QStringLiteral("accepted/none/1"));

    // Another version, before either path.
    QByteArray other = wholeMessage(pairFrame(2, 8));
    for (const int version : {0, 3, 255}) {
        other[4] = static_cast<char>(version);
        QCOMPARE(outcome(decoder.decode(other)), QStringLiteral("rejected/unsupportedVersion/0"));
    }
    QCOMPARE(decoder.counters().malformed, quint64 {3});
}

void TstDisplayCodecV2::malformedSet()
{
    // Each a literal message to a fresh decoder: rejected with its reason,
    // no frame, and the malformed counter alone at 1.
    struct Case {
        QString name;
        QByteArray message;
        DisplayCodecReason reason;
    };
    QList<Case> cases;
    const DisplayCodecFrame frame = pairFrame(1);
    const QByteArray base = wholeMessage(frame);          // 0x1A, trace 8, waterfall 6
    const QByteArray baseWide = wholeMessage(wideFrame(1)); // 0x1E, wide 4
    const int tracePlaneBytes =
        static_cast<int>(encodeDisplayCodecV2Plane(Vectors::quantizeRow(frame.traceDbm)).size());
    const int waterfallPlane = static_cast<int>(kHeader) + tracePlaneBytes;
    const auto withByte = [](QByteArray message, int offset, int value) {
        message[offset] = static_cast<char>(value);
        return message;
    };
    const auto withU16 = [](QByteArray message, int offset, int value) {
        message[offset] = static_cast<char>((value >> 8) & 0xff);
        message[offset + 1] = static_cast<char>(value & 0xff);
        return message;
    };

    // The version dispatch.
    cases.append({QStringLiteral("bad magic"), withByte(base, 0, 'X'), DisplayCodecReason::BadMagic});
    cases.append({QStringLiteral("bad magic, last byte"), withByte(base, 3, 'D'),
                  DisplayCodecReason::BadMagic});
    for (const int version : {0, 3, 255}) {
        cases.append({QStringLiteral("version %1").arg(version), withByte(base, 4, version),
                      DisplayCodecReason::UnsupportedVersion});
    }
    QByteArray oversized = base;
    oversized.append(QByteArray(DisplayCodecEncoder::kMaxEncodedBytes + 1 - oversized.size(), '\0'));
    cases.append({QStringLiteral("oversized"), oversized, DisplayCodecReason::Oversized});
    for (int cut = 0; cut <= 4; ++cut) {
        cases.append({QStringLiteral("cut at %1").arg(cut), base.left(cut), DisplayCodecReason::Truncated});
    }

    // Flags.
    for (const int flags : {0x01, 0x40, 0x80, 0x1B, 0x5A, 0x9A, 0xFF}) {
        cases.append({QStringLiteral("flags %1").arg(flags, 2, 16, QLatin1Char('0')),
                      withByte(base, 5, flags), DisplayCodecReason::UnknownFlags});
    }
    const QList<int> valid {0x08, 0x1A, 0x1E, 0x2A, 0x32, 0x36};
    for (int flags = 0; flags <= 0x3E; flags += 2) {
        if (valid.contains(flags)) { continue; }
        cases.append({QStringLiteral("flags %1").arg(flags, 2, 16, QLatin1Char('0')),
                      withByte(base, 5, flags), DisplayCodecReason::Malformed});
    }
    cases.append({QStringLiteral("0x1A with a wide length"), withU16(base, 40, kWideLength),
                  DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("0x1E with no wide length"), withU16(baseWide, 40, 0),
                  DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("0x32 with a wide length"),
                  withU16(rowPartMessage(frame), 40, kWideLength), DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("0x36 with no wide length"),
                  withU16(rowPartMessage(wideFrame(1)), 40, 0), DisplayCodecReason::Malformed});

    // The header.
    cases.append({QStringLiteral("header size 41"), withU16(base, 6, 41), DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("header size 43"), withU16(base, 6, 43), DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("trace length 0"), withU16(base, 36, 0), DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("waterfall length 0 on a trace-only message"),
                  withU16(traceOnlyMessage(frame), 38, 0), DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("trace length 4097"), withU16(base, 36, 4097),
                  DisplayCodecReason::Malformed});
    {
        DisplayCodecFrame inverted = frame;
        std::swap(inverted.context.minDbm, inverted.context.maxDbm);
        cases.append({QStringLiteral("interval inverted"), Vectors::buildMessage(inverted, 0x1A, {}),
                      DisplayCodecReason::Malformed});
        DisplayCodecFrame infinite = frame;
        infinite.context.maxDbm = std::numeric_limits<float>::infinity();
        cases.append({QStringLiteral("interval not finite"), Vectors::buildMessage(infinite, 0x1A, {}),
                      DisplayCodecReason::Malformed});
    }

    // The planes.
    for (const int coding : {0, 1, 3, 255}) {
        cases.append({QStringLiteral("trace plane coding %1").arg(coding),
                      withByte(base, static_cast<int>(kHeader), coding), DisplayCodecReason::Malformed});
        cases.append({QStringLiteral("waterfall plane coding %1").arg(coding),
                      withByte(base, waterfallPlane, coding), DisplayCodecReason::Malformed});
    }
    for (const int code : {3, 255}) {
        cases.append({QStringLiteral("block code %1").arg(code),
                      withByte(base, static_cast<int>(kHeader) + 1, code), DisplayCodecReason::Malformed});
    }
    const int lastPayload = u16At(base, waterfallPlane + 2);
    cases.append({QStringLiteral("payload overrun"), withU16(base, waterfallPlane + 2, lastPayload + 1),
                  DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("byte after the last plane"), base + QByteArray(1, '\0'),
                  DisplayCodecReason::Malformed});
    cases.append({QStringLiteral("payload bytes after the last sample"),
                  withU16(base, waterfallPlane + 2, lastPayload + 1) + QByteArray(1, '\0'),
                  DisplayCodecReason::Malformed});
    {
        const int wideTraceBytes = static_cast<int>(
            encodeDisplayCodecV2Plane(Vectors::quantizeRow(wideFrame(1).traceDbm)).size());
        const int wideWaterfallPlane = static_cast<int>(kHeader) + wideTraceBytes;
        const int widePlane = wideWaterfallPlane + 4 + u16At(baseWide, wideWaterfallPlane + 2);
        QVERIFY(widePlane < baseWide.size());
        cases.append({QStringLiteral("missing wide plane"), baseWide.left(widePlane),
                      DisplayCodecReason::Malformed});
    }
    {
        const DisplayCodecFrame small = frameOf(qRow({0, 255}), qRow({0, 255}), {}, true, 1);
        const QByteArray good = encodeDisplayCodecV2Plane(qRow({0, 255}));
        QByteArray pad = good;
        pad[5] = static_cast<char>(0x09);
        cases.append({QStringLiteral("nonzero pad"), Vectors::buildMessage(small, 0x1A, {pad, good}),
                      DisplayCodecReason::Malformed});
        pad[5] = static_cast<char>(0x0C);
        cases.append({QStringLiteral("nonzero pad, high"), Vectors::buildMessage(small, 0x1A, {good, pad}),
                      DisplayCodecReason::Malformed});
        const QString zBits = QStringLiteral("00000000") + QStringLiteral("000")
            + QString(256, QLatin1Char('0')) + QStringLiteral("1");
        cases.append({QStringLiteral("z above 255"),
                      Vectors::buildMessage(small, 0x1A, {planeWithPayload(0, bitsToBytes(zBits)), good}),
                      DisplayCodecReason::Malformed});
        // The same with 255 zeros is z = 255, a valid plane: the message is
        // accepted, which shows the case above fails only on z.
        const QString maxBits = QStringLiteral("00000000") + QStringLiteral("000")
            + QString(255, QLatin1Char('0')) + QStringLiteral("1");
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(Vectors::buildMessage(
                     small, 0x1A, {planeWithPayload(0, bitsToBytes(maxBits)), good}))),
                 QStringLiteral("accepted/none/1"));
    }

    for (const Case& c : cases) {
        DisplayCodecDecoder decoder;
        const DisplayCodecDecodeResult result = decoder.decode(c.message);
        QVERIFY2(outcome(result) == QStringLiteral("rejected/%1/0").arg(reasonName(c.reason)),
                 qPrintable(QStringLiteral("%1: %2").arg(c.name, outcome(result))));
        QVERIFY2(counterText(decoder.counters()) == counterText(counterValues(0, 0, 0, 0, 1)),
                 qPrintable(c.name));
    }
}

void TstDisplayCodecV2::truncationAtEveryByte()
{
    // A cut at every byte of a whole frame and of each split part: Truncated
    // in the header, Malformed in a plane, never an accept or a hold, and
    // the decoder untouched (the whole message decodes after).
    QList<std::pair<QString, QByteArray>> messages {
        {QStringLiteral("whole"), wholeMessage(pairFrame(1))},
        {QStringLiteral("whole with wide"), wholeMessage(wideFrame(1))},
        {QStringLiteral("row part"), rowPartMessage(wideFrame(1))},
        {QStringLiteral("trace part"), tracePartMessage(wideFrame(1))},
        {QStringLiteral("trace only"), traceOnlyMessage(pairFrame(1))},
    };
    QVector<float> trace;
    QVector<float> waterfall;
    Vectors::SceneRun peak(Vectors::Scene::PeakBins167);
    peak.next(trace, waterfall);
    const DisplayCodecV2Encoding split = encodeDisplayCodecV2(
        sceneFrame(783, 1, trace, waterfall), spectrumDisplayCost(783, 30, false)->maximumFrameBytes);
    QVERIFY(split.split());
    messages.append({QStringLiteral("peak row part"), split.messages[0]});
    messages.append({QStringLiteral("peak trace part"), split.messages[1]});

    for (const auto& [name, message] : messages) {
        DisplayCodecDecoder decoder;
        for (int cut = 0; cut < message.size(); ++cut) {
            const DisplayCodecDecodeResult result = decoder.decode(message.left(cut));
            const QString expected = cut < static_cast<int>(kHeader) ? QStringLiteral("rejected/truncated/0")
                                                                     : QStringLiteral("rejected/malformed/0");
            QVERIFY2(outcome(result) == expected,
                     qPrintable(QStringLiteral("%1 cut at %2: %3").arg(name).arg(cut).arg(outcome(result))));
        }
        QCOMPARE(decoder.counters().malformed, static_cast<quint64>(message.size()));
        const DisplayCodecDecodeResult whole = decoder.decode(message);
        QVERIFY2(whole.disposition != DisplayCodecDisposition::Rejected, qPrintable(name));
    }
}

void TstDisplayCodecV2::structureBeforeState()
{
    // A structurally bad message rejects as malformed whatever the state
    // would have said: stale, old generation, another endpoint, another
    // shape, or a duplicate of the held part. The held part stays.
    DisplayCodecDecoder decoder;
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4, 4)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6, 4)))), QStringLiteral("held/none/0"));
    const auto broken = [](QByteArray message) {
        message.append('\0');
        return message;
    };
    DisplayCodecFrame otherEndpoint = pairFrame(7, 4);
    otherEndpoint.context.endpointId = 0x999U;
    for (const QByteArray& message : {broken(wholeMessage(pairFrame(3, 4))),
                                      broken(wholeMessage(pairFrame(9, 3))),
                                      broken(wholeMessage(otherEndpoint)),
                                      broken(wholeMessage(wideFrame(7, 4))),
                                      broken(rowPartMessage(pairFrame(6, 4)))}) {
        QCOMPARE(outcome(decoder.decode(message)), QStringLiteral("rejected/malformed/0"));
    }
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 5)));
    // The same messages whole: the state's own answers, counting nothing
    // but the stale one.
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(3, 4)))),
             QStringLiteral("rejected/staleSequence/0"));
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(9, 3)))), QStringLiteral("rejected/oldContext/0"));
    QCOMPARE(outcome(decoder.decode(wholeMessage(otherEndpoint))),
             QStringLiteral("rejected/contextMismatch/0"));
    DisplayCodecFrame otherShape = wideFrame(7, 4);
    otherShape.context.endpointId = kEndpointA;
    QCOMPARE(outcome(decoder.decode(wholeMessage(otherShape))),
             QStringLiteral("rejected/contextMismatch/0"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 1, 0, 5)));
    const DisplayCodecDecodeResult merged = decoder.decode(tracePartMessage(pairFrame(6, 4)));
    QCOMPARE(outcome(merged), QStringLiteral("accepted/none/1"));
    QVERIFY(frameDifference(merged.frame, pairFrame(6, 4)).isEmpty());
}

void TstDisplayCodecV2::pairingWholeFrames()
{
    DisplayCodecDecoder decoder;
    DisplayCodecDecodeResult result = decoder.decode(wholeMessage(pairFrame(1)));
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    QVERIFY2(frameDifference(result.frame, pairFrame(1)).isEmpty(),
             qPrintable(frameDifference(result.frame, pairFrame(1))));
    QCOMPARE(decoder.counters().framesLost, quint64 {0});
    result = decoder.decode(wholeMessage(pairFrame(4)));
    QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
    QCOMPARE(result.frame.encoderSequence, 4U);
    QCOMPARE(decoder.counters().framesLost, quint64 {2});

    // A stale whole and a stale part, each counted once.
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))), QStringLiteral("rejected/staleSequence/0"));
    QCOMPARE(decoder.counters().staleMessages, quint64 {1});
    QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(2)))),
             QStringLiteral("rejected/staleSequence/0"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(2, 0, 2, 0, 0)));

    // framesLost across the 2^32 sequence wrap.
    DisplayCodecDecoder wrap;
    QCOMPARE(outcome(wrap.decode(wholeMessage(pairFrame(0xFFFFFFFEU)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(outcome(wrap.decode(wholeMessage(pairFrame(1)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(wrap.counters().framesLost, quint64 {2});
    // A fresh context: nothing is stale and nothing is lost.
    DisplayCodecDecoder fresh;
    QCOMPARE(outcome(fresh.decode(wholeMessage(pairFrame(0x80000000U)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(fresh.counters().framesLost, quint64 {0});
}

void TstDisplayCodecV2::pairingSplitParts()
{
    // Row part first, then the trace part: held, then merged.
    for (const bool withWide : {false, true}) {
        const DisplayCodecFrame five = pairFrame(5, 3, withWide);
        DisplayCodecDecoder rowFirst;
        QCOMPARE(outcome(rowFirst.decode(rowPartMessage(five))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult merged = rowFirst.decode(tracePartMessage(five));
        QCOMPARE(outcome(merged), QStringLiteral("accepted/none/1"));
        QVERIFY(merged.frame.waterfallAdvance);
        QVERIFY(!merged.frame.traceCarried);
        QVERIFY2(frameDifference(merged.frame, five).isEmpty(),
                 qPrintable(frameDifference(merged.frame, five)));

        // The trace part first gives the same frame.
        DisplayCodecDecoder traceFirst;
        QCOMPARE(outcome(traceFirst.decode(tracePartMessage(five))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult again = traceFirst.decode(rowPartMessage(five));
        QCOMPARE(outcome(again), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(again.frame, merged.frame).isEmpty());
        QCOMPARE(counterText(rowFirst.counters()), counterText(counterValues(0, 0, 0, 0, 0)));
        QCOMPARE(counterText(traceFirst.counters()), counterText(counterValues(0, 0, 0, 0, 0)));
    }

    // A duplicate part: StaleSequence, duplicateParts, and H stays.
    for (const bool rowHeld : {true, false}) {
        const QByteArray part = rowHeld ? rowPartMessage(pairFrame(6)) : tracePartMessage(pairFrame(6));
        const QByteArray partner = rowHeld ? tracePartMessage(pairFrame(6)) : rowPartMessage(pairFrame(6));
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(part)), QStringLiteral("held/none/0"));
        QCOMPARE(outcome(decoder.decode(part)), QStringLiteral("rejected/staleSequence/0"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 1, 0)));
        QCOMPARE(outcome(decoder.decode(partner)), QStringLiteral("accepted/none/1"));
    }

    // The other part with a different timestamp: Malformed, and H stays.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        DisplayCodecFrame otherTimestamp = pairFrame(6);
        otherTimestamp.producerTimestamp += 1;
        QCOMPARE(outcome(decoder.decode(tracePartMessage(otherTimestamp))),
                 QStringLiteral("rejected/malformed/0"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 1)));
        const DisplayCodecDecodeResult merged = decoder.decode(tracePartMessage(pairFrame(6)));
        QCOMPARE(outcome(merged), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(merged.frame, pairFrame(6)).isEmpty());
    }

    // A whole frame with the held sequence: Malformed, and H stays.
    for (const bool traceOnly : {false, true}) {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        const QByteArray whole = traceOnly ? traceOnlyMessage(pairFrame(6)) : wholeMessage(pairFrame(6));
        QCOMPARE(outcome(decoder.decode(whole)), QStringLiteral("rejected/malformed/0"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 1)));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(6)))), QStringLiteral("accepted/none/1"));
    }
}

void TstDisplayCodecV2::pairingAroundTheHeldPart()
{
    // L = 4 and row part 6 held: whole 5 is released now and H stays.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult five = decoder.decode(wholeMessage(pairFrame(5)));
        QCOMPARE(outcome(five), QStringLiteral("accepted/none/1"));
        QCOMPARE(five.frame.encoderSequence, 5U);
        QVERIFY(frameDifference(five.frame, pairFrame(5)).isEmpty());
        const DisplayCodecDecodeResult six = decoder.decode(tracePartMessage(pairFrame(6)));
        QCOMPARE(outcome(six), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(six.frame, pairFrame(6)).isEmpty());
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 0)));
    }
    // A part older than the held one is released now as a partial frame.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult five = decoder.decode(tracePartMessage(pairFrame(5)));
        QCOMPARE(outcome(five), QStringLiteral("accepted/none/1"));
        const DisplayCodecFrame expected =
            traceOnlyRelease(pairFrame(5), pairFrame(4).waterfallDbm, {});
        QVERIFY2(frameDifference(five.frame, expected).isEmpty(),
                 qPrintable(frameDifference(five.frame, expected)));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(6)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 1, 0, 0, 0)));
    }
    // L = 5 and row part 6 held, then whole 8: two frames, the partial 6
    // (its trace carried from 5) then 8; 7 is lost.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(5)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult two = decoder.decode(wholeMessage(pairFrame(8)));
        QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
        QVERIFY(two.frame.traceCarried);
        QVERIFY(two.frame.waterfallAdvance);
        const DisplayCodecFrame partial = rowOnlyRelease(pairFrame(6), pairFrame(5).traceDbm);
        QVERIFY2(frameDifference(two.frame, partial).isEmpty(),
                 qPrintable(frameDifference(two.frame, partial)));
        QVERIFY(frameDifference(two.secondFrame, pairFrame(8)).isEmpty());
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(1, 1, 0, 0, 0)));
    }
    // Row part 6 held, then trace part 7: the partial 6, and 7 is held.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(5)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult six = decoder.decode(tracePartMessage(pairFrame(7)));
        QCOMPARE(outcome(six), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(six.frame, rowOnlyRelease(pairFrame(6), pairFrame(5).traceDbm)).isEmpty());
        const DisplayCodecDecodeResult seven = decoder.decode(rowPartMessage(pairFrame(7)));
        QCOMPARE(outcome(seven), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(seven.frame, pairFrame(7)).isEmpty());
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 1, 0, 0, 0)));
    }
    // A part older than L while H is held: stale, and H stays.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(5)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(7)))), QStringLiteral("held/none/0"));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(5)))),
                 QStringLiteral("rejected/staleSequence/0"));
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))),
                 QStringLiteral("rejected/staleSequence/0"));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(7)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(1, 0, 2, 0, 0)));
    }
}

void TstDisplayCodecV2::pairingPartialFramesAndFills()
{
    const QVector<float> noTrace = zeroRow(kTraceLength);
    const QVector<float> noWaterfall = zeroRow(kWaterfallLength);
    const QVector<float> noWide = zeroRow(kWideLength);

    // Row only from a fresh decoder: the trace is minimum dBm (q 0).
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(rowPartMessage(wideFrame(6)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult two = decoder.decode(wholeMessage(wideFrame(8)));
        QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
        QVERIFY(frameDifference(two.frame, rowOnlyRelease(wideFrame(6), noTrace)).isEmpty());
        QVERIFY(frameDifference(two.secondFrame, wideFrame(8)).isEmpty());
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(1, 1, 0, 0, 0)));
    }
    // Trace only (its row lost), released by a later message: waterfall
    // advance clear, the row planes the last decoded ones (q 0 when none).
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(tracePartMessage(wideFrame(3)))), QStringLiteral("held/none/0"));
        DisplayCodecDecodeResult two = decoder.decode(wholeMessage(wideFrame(4)));
        QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
        QVERIFY(!two.frame.waterfallAdvance);
        QVERIFY(!two.frame.traceCarried);
        DisplayCodecFrame expected = traceOnlyRelease(wideFrame(3), noWaterfall, noWide);
        QVERIFY2(frameDifference(two.frame, expected).isEmpty(),
                 qPrintable(frameDifference(two.frame, expected)));

        QCOMPARE(outcome(decoder.decode(tracePartMessage(wideFrame(5)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult one = decoder.decode(rowPartMessage(wideFrame(6)));
        QCOMPARE(outcome(one), QStringLiteral("accepted/none/1"));
        // The fills are the last released planes (4), not the row part just
        // held (6).
        expected = traceOnlyRelease(wideFrame(5), wideFrame(4).waterfallDbm, wideFrame(4).wideDbm);
        QVERIFY2(frameDifference(one.frame, expected).isEmpty(),
                 qPrintable(frameDifference(one.frame, expected)));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 2, 0, 0, 0)));
    }
    // A 0x08 whole frame: its trace, waterfall advance clear, the fills.
    {
        DisplayCodecDecoder decoder;
        DisplayCodecDecodeResult result = decoder.decode(traceOnlyMessage(wideFrame(1)));
        QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(result.frame, traceOnlyRelease(wideFrame(1), noWaterfall, noWide)).isEmpty());
        QCOMPARE(outcome(decoder.decode(wholeMessage(wideFrame(2)))), QStringLiteral("accepted/none/1"));
        result = decoder.decode(traceOnlyMessage(wideFrame(3)));
        QCOMPARE(outcome(result), QStringLiteral("accepted/none/1"));
        const DisplayCodecFrame expected =
            traceOnlyRelease(wideFrame(3), wideFrame(2).waterfallDbm, wideFrame(2).wideDbm);
        QVERIFY2(frameDifference(result.frame, expected).isEmpty(),
                 qPrintable(frameDifference(result.frame, expected)));
        // A 0x08 frame is whole: no part lost.
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 0)));
        // And a later row-only frame carries the 0x08 frame's trace.
        QCOMPARE(outcome(decoder.decode(rowPartMessage(wideFrame(4)))), QStringLiteral("held/none/0"));
        result = decoder.decode(wholeMessage(wideFrame(5)));
        QCOMPARE(outcome(result), QStringLiteral("accepted/none/2"));
        QVERIFY(frameDifference(result.frame, rowOnlyRelease(wideFrame(4), wideFrame(3).traceDbm)).isEmpty());
    }
}

void TstDisplayCodecV2::pairingGenerationChange()
{
    // A newer generation while a part is held: H goes as a partial frame
    // under the old context, then the new message is a first message.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4, 3)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(6, 3)))), QStringLiteral("held/none/0"));
        const DisplayCodecDecodeResult two = decoder.decode(wholeMessage(pairFrame(1, 4)));
        QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
        QVERIFY(frameDifference(two.frame, rowOnlyRelease(pairFrame(6, 3), pairFrame(4, 3).traceDbm)).isEmpty());
        QCOMPARE(two.frame.context.contextGeneration, 3U);
        QVERIFY(frameDifference(two.secondFrame, pairFrame(1, 4)).isEmpty());
        // framesLost 1 (5, against the old L); none across the change.
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(1, 1, 0, 0, 0)));
    }
    // The new context's fills start empty, even after the held part was
    // released under the old one, and it has no L.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4, 3)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(6, 3)))), QStringLiteral("held/none/0"));
        DisplayCodecDecodeResult one = decoder.decode(rowPartMessage(pairFrame(2, 4)));
        QCOMPARE(outcome(one), QStringLiteral("accepted/none/1"));
        QVERIFY(frameDifference(one.frame, traceOnlyRelease(pairFrame(6, 3), pairFrame(4, 3).waterfallDbm, {}))
                    .isEmpty());
        const DisplayCodecDecodeResult two = decoder.decode(wholeMessage(pairFrame(5, 4)));
        QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
        const DisplayCodecFrame expected = rowOnlyRelease(pairFrame(2, 4), zeroRow(kTraceLength));
        QVERIFY2(frameDifference(two.frame, expected).isEmpty(),
                 qPrintable(frameDifference(two.frame, expected)));
        // 1 (frame 5 of generation 3), then 2 (3 and 4 of generation 4).
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(3, 2, 0, 0, 0)));
    }
    // An older generation: OldContext, no counter, and H stays.
    {
        DisplayCodecDecoder decoder;
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(1, 4)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(2, 4)))), QStringLiteral("held/none/0"));
        QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(9, 3)))), QStringLiteral("rejected/oldContext/0"));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(9, 3)))),
                 QStringLiteral("rejected/oldContext/0"));
        QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(2, 4)))), QStringLiteral("accepted/none/1"));
        QCOMPARE(counterText(decoder.counters()), counterText(counterValues(0, 0, 0, 0, 0)));
    }
}

void TstDisplayCodecV2::resetClearsStateButNotCounters()
{
    DisplayCodecDecoder decoder;
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(1)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))), QStringLiteral("accepted/none/1"));
    QCOMPARE(outcome(decoder.decode(wholeMessage(pairFrame(4)))), QStringLiteral("rejected/staleSequence/0"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(2, 0, 1, 0, 0)));

    // After reset() the first row-only partial carries a trace of q 0, not
    // the trace of 4, and there is no L (2 is not stale).
    decoder.reset();
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(2, 0, 1, 0, 0)));
    QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(2)))), QStringLiteral("held/none/0"));
    const DisplayCodecDecodeResult two = decoder.decode(wholeMessage(pairFrame(3)));
    QCOMPARE(outcome(two), QStringLiteral("accepted/none/2"));
    const DisplayCodecFrame expected = rowOnlyRelease(pairFrame(2), zeroRow(kTraceLength));
    QVERIFY2(frameDifference(two.frame, expected).isEmpty(), qPrintable(frameDifference(two.frame, expected)));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(2, 1, 1, 0, 0)));

    // reset() discards a held part rather than releasing it.
    QCOMPARE(outcome(decoder.decode(rowPartMessage(pairFrame(10)))), QStringLiteral("held/none/0"));
    decoder.reset();
    QCOMPARE(outcome(decoder.decode(tracePartMessage(pairFrame(10)))), QStringLiteral("held/none/0"));
    QCOMPARE(counterText(decoder.counters()), counterText(counterValues(2, 1, 1, 0, 0)));
}

void TstDisplayCodecV2::goldenSequencesDecode()
{
    QJsonObject document;
    QString error;
    QVERIFY2(Vectors::readVectors(Vectors::vectorsPath(), document, error), qPrintable(error));
    const QJsonArray sequences = document.value(QStringLiteral("sequences")).toArray();
    QVERIFY(!sequences.isEmpty());

    // Every disagreement is listed before failing, so a wrong expectation
    // and a wrong decoder can be told apart from one run.
    QStringList disagreements;
    QSet<QString> reasonsSeen;
    for (const QJsonValue& value : sequences) {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const QJsonArray messages = entry.value(QStringLiteral("messages")).toArray();
        const QJsonArray results = entry.value(QStringLiteral("results")).toArray();
        QCOMPARE(messages.size(), results.size());
        DisplayCodecDecoder decoder;
        for (int i = 0; i < messages.size(); ++i) {
            const DisplayCodecDecodeResult result = decoder.decode(Vectors::fromHex(messages.at(i).toString()));
            if (!wellFormed(result)) {
                disagreements.append(QStringLiteral("%1 [%2]: not well formed: %3").arg(name).arg(i).arg(outcome(result)));
            }
            const QJsonObject expected = results.at(i).toObject();
            const QString wantDisposition = expected.value(QStringLiteral("disposition")).toString();
            const QString wantReason = expected.value(QStringLiteral("reason")).toString();
            reasonsSeen.insert(wantReason);
            const QJsonArray frames = expected.value(QStringLiteral("frames")).toArray();
            if (dispositionName(result.disposition) != wantDisposition
                || reasonName(result.reason) != wantReason || result.frameCount != frames.size()) {
                disagreements.append(QStringLiteral("%1 [%2]: got %3, expected %4/%5/%6")
                                         .arg(name).arg(i).arg(outcome(result), wantDisposition, wantReason)
                                         .arg(frames.size()));
                continue;
            }
            for (int k = 0; k < frames.size(); ++k) {
                const QJsonObject actual =
                    Vectors::frameToJson(k == 0 ? result.frame : result.secondFrame);
                if (actual != frames.at(k).toObject()) {
                    disagreements.append(
                        QStringLiteral("%1 [%2] frame %3: got %4, expected %5")
                            .arg(name).arg(i).arg(k)
                            .arg(QString::fromUtf8(QJsonDocument(actual).toJson(QJsonDocument::Compact)),
                                 QString::fromUtf8(QJsonDocument(frames.at(k).toObject())
                                                       .toJson(QJsonDocument::Compact))));
                }
            }
        }
        const QJsonObject want = entry.value(QStringLiteral("counters")).toObject();
        const DisplayCodecDecoderCounters wanted = counterValues(
            static_cast<quint64>(want.value(QStringLiteral("framesLost")).toInteger()),
            static_cast<quint64>(want.value(QStringLiteral("partsLost")).toInteger()),
            static_cast<quint64>(want.value(QStringLiteral("staleMessages")).toInteger()),
            static_cast<quint64>(want.value(QStringLiteral("duplicateParts")).toInteger()),
            static_cast<quint64>(want.value(QStringLiteral("malformed")).toInteger()));
        if (counterText(decoder.counters()) != counterText(wanted)) {
            disagreements.append(QStringLiteral("%1 counters: got %2, expected %3")
                                     .arg(name, counterText(decoder.counters()), counterText(wanted)));
        }
    }
    QVERIFY2(disagreements.isEmpty(), qPrintable(disagreements.join(QLatin1Char('\n'))));
    // The file covers each dispatch reject as well as the v2 path's.
    for (const char* reason : {"badMagic", "unsupportedVersion", "oversized", "truncated", "unknownFlags",
                               "malformed", "staleSequence", "oldContext", "none"}) {
        QVERIFY2(reasonsSeen.contains(QLatin1String(reason)), reason);
    }
}

void TstDisplayCodecV2::lossSimulation783()
{
    // 3,000 frames of the peak scene at 783 px, Gilbert-Elliott bursts and
    // random drops, each with adjacent reordering. Checks, for each run:
    // every released frame is the encoder's quantized input (whole or
    // merged) or follows the partial and fill rules plane for plane;
    // released sequences strictly increase and framesLost + released spans
    // the first to the last; partsLost is the number of partial frames; and
    // every frame none of whose messages was dropped or moved is released
    // whole or merged.
    const EncodedRun run = encodeRun(Vectors::Scene::PeakBins167, 3000);
    QVERIFY(run.splitFrames > 0);
    runLoss(run, LossModel::GilbertElliott, 0x5EED0783ULL);
    if (QTest::currentTestFailed()) { return; }
    runLoss(run, LossModel::Random, 0x5EED1783ULL);
}

void TstDisplayCodecV2::lossSimulation4096()
{
    const EncodedRun run = encodeRun(Vectors::Scene::Peak4096, 1000);
    QCOMPARE(run.splitFrames, 1000);
    runLoss(run, LossModel::GilbertElliott, 0x5EED4096ULL);
    if (QTest::currentTestFailed()) { return; }
    runLoss(run, LossModel::Random, 0x5EED5096ULL);
}

void TstDisplayCodecV2::benchmarkEncode_data()
{
    QTest::addColumn<int>("scene");
    QTest::newRow("783 px peak") << static_cast<int>(Vectors::Scene::PeakBins167);
    QTest::newRow("4096 px peak") << static_cast<int>(Vectors::Scene::Peak4096);
}

void TstDisplayCodecV2::benchmarkEncode()
{
    // Recorded, not asserted (section 10).
    QFETCH(int, scene);
    Vectors::SceneRun run(static_cast<Vectors::Scene>(scene));
    QVector<float> trace;
    QVector<float> waterfall;
    run.next(trace, waterfall);
    const DisplayCodecFrame frame = sceneFrame(run.pixels(), 1, trace, waterfall);
    const quint32 admitted = spectrumDisplayCost(run.pixels(), 30, false)->maximumFrameBytes;
    qsizetype bytes = 0;
    QBENCHMARK {
        bytes = encodeDisplayCodecV2(frame, admitted).totalBytes();
    }
    QVERIFY(bytes > 0);
}

void TstDisplayCodecV2::benchmarkDecode_data()
{
    benchmarkEncode_data();
}

void TstDisplayCodecV2::benchmarkDecode()
{
    // Recorded, not asserted (section 10): every message of one frame
    // through a decoder reset before each pass.
    QFETCH(int, scene);
    Vectors::SceneRun run(static_cast<Vectors::Scene>(scene));
    QVector<float> trace;
    QVector<float> waterfall;
    run.next(trace, waterfall);
    const DisplayCodecFrame frame = sceneFrame(run.pixels(), 1, trace, waterfall);
    const DisplayCodecV2Encoding encoding =
        encodeDisplayCodecV2(frame, spectrumDisplayCost(run.pixels(), 30, false)->maximumFrameBytes);
    QVERIFY(!encoding.messages.empty());
    DisplayCodecDecoder decoder;
    int frames = 0;
    QBENCHMARK {
        decoder.reset();
        for (const QByteArray& message : encoding.messages) {
            frames += decoder.decode(message).frameCount;
        }
    }
    QVERIFY(frames > 0);
}

QTEST_APPLESS_MAIN(TstDisplayCodecV2)
#include "tst_display_codec_v2.moc"
