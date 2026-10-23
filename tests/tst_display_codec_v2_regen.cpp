// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_display_codec_v2_regen.cpp  (NereusSDR)
// =================================================================
//
// Display codec v2 Task 1: writes tests/data/display_codec_v2/vectors.json,
// the golden vectors the C++ tests and the phone's NereusKit tests both
// read. Into the directory NEREUS_DISPLAY_CODEC_V2_REGEN_OUT names when it
// is set, and otherwise into a temporary directory, so ctest runs it
// without touching the tree (as tst_link_surface_manifest_regen does). Not
// a regression check; tst_display_codec_v2 is.
//
//   cmake --build build --target tst_display_codec_v2_regen
//   NEREUS_DISPLAY_CODEC_V2_REGEN_OUT=tests/data/display_codec_v2 \
//       QT_QPA_PLATFORM=offscreen ./build/tests/tst_display_codec_v2_regen
//
// Read the resulting diff whole before committing it.
//
// The planes and frames come from the encoder. The sequences are the
// decoder's expectations, stated here by hand from the v2 document's
// sections 4.1, 4.2, 9.1 and 9.2 (not computed by a decoder): each starts
// from a fresh decoder, lists what every message must give, and the
// counters after the last one. A released frame's absent planes are the
// last planes released in its context (q 0 when none), in release order.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 golden vector regenerator. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-22: The rejects decided at the version dispatch (bad magic,
//               unsupported version, oversized, truncated in the magic).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-23: The float context fields compare by value (-0.0 equals
//               +0.0 in a context and between a split frame's parts), and
//               a NaN interval is malformed. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <optional>

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"

#include "DisplayCodecV2Vectors.h"

using namespace NereusSDR;
namespace Vectors = NereusSDR::Test::DisplayCodecV2Vectors;

namespace {

// ---- Planes and frames ----

QJsonObject planeEntry(const QString& name, const QVector<quint8>& samples)
{
    const QByteArray plane = encodeDisplayCodecV2Plane(samples);
    QJsonObject entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("samples"), Vectors::toJsonArray(samples));
    entry.insert(QStringLiteral("blockSize"), 16 << static_cast<int>(plane.at(1)));
    entry.insert(QStringLiteral("plane"), Vectors::toHex(plane));
    return entry;
}

QJsonObject frameEntry(const QString& name, const DisplayCodecFrame& frame, quint32 admitted)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("admittedFrameBytes"), static_cast<qint64>(admitted));
    entry.insert(QStringLiteral("frame"), Vectors::frameToJson(frame));
    entry.insert(QStringLiteral("messages"),
                 Vectors::toJsonHexArray(encodeDisplayCodecV2(frame, admitted).messages));
    return entry;
}

QVector<quint8> noiseRow(int n, quint64 seed)
{
    Vectors::SplitMix64 rng(seed);
    QVector<quint8> out(n);
    for (int i = 0; i < n; ++i) { out[i] = rng.byte(); }
    return out;
}

QVector<quint8> rampRow(int n)
{
    QVector<quint8> out(n);
    for (int i = 0; i < n; ++i) { out[i] = static_cast<quint8>(i & 255); }
    return out;
}

QVector<quint8> alternatingRow(int n)
{
    QVector<quint8> out(n);
    for (int i = 0; i < n; ++i) { out[i] = (i % 2) == 0 ? 0 : 255; }
    return out;
}

/// Residual 0 for `period` samples, then +/-100 for `period`, from sample 1.
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

QVector<quint8> qRow(std::initializer_list<int> values)
{
    QVector<quint8> out;
    for (int value : values) { out.append(static_cast<quint8>(value)); }
    return out;
}

DisplayCodecFrame encoderFrame(quint32 endpoint, quint32 generation, quint32 sequence,
                               const QVector<quint8>& trace, const QVector<quint8>& waterfall,
                               const QVector<quint8>& wide, bool advance)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = endpoint;
    frame.context.contextGeneration = generation;
    frame.context.minDbm = Vectors::kMinDbm;
    frame.context.maxDbm = Vectors::kMaxDbm;
    frame.context.traceSamples = static_cast<quint16>(trace.size());
    frame.context.waterfallSamples = static_cast<quint16>(waterfall.size());
    frame.context.wideSamples = static_cast<quint16>(wide.size());
    frame.encoderSequence = sequence;
    frame.producerTimestamp = 0x0123456789ABCDEFULL + sequence;
    frame.waterfallAdvance = advance;
    frame.traceDbm = Vectors::dequantizeRow(trace);
    frame.waterfallDbm = Vectors::dequantizeRow(waterfall);
    frame.wideDbm = Vectors::dequantizeRow(wide);
    return frame;
}

// ---- Sequences ----

constexpr quint32 kEndpointA = 0x00000101U; // trace 8, waterfall 6, no wide
constexpr quint32 kEndpointB = 0x00000202U; // trace 8, waterfall 6, wide 4
constexpr int kTraceLength = 8;
constexpr int kWaterfallLength = 6;
constexpr int kWideLength = 4;

/// A frame of a sequence: its planes differ by generation and sequence so a
/// plane from the wrong frame never matches.
DisplayCodecFrame seqFrame(quint32 endpoint, quint32 generation, quint32 sequence)
{
    QVector<quint8> trace(kTraceLength);
    QVector<quint8> waterfall(kWaterfallLength);
    QVector<quint8> wide(endpoint == kEndpointB ? kWideLength : 0);
    for (int i = 0; i < trace.size(); ++i) {
        trace[i] = static_cast<quint8>(sequence * 37U + generation * 11U + i * 13U + 1U);
    }
    for (int i = 0; i < waterfall.size(); ++i) {
        waterfall[i] = static_cast<quint8>(sequence * 53U + generation * 7U + i * 29U + 100U);
    }
    for (int i = 0; i < wide.size(); ++i) {
        wide[i] = static_cast<quint8>(sequence * 19U + generation * 3U + i * 5U + 200U);
    }
    DisplayCodecFrame frame = encoderFrame(endpoint, generation, sequence, trace, waterfall, wide, true);
    frame.producerTimestamp = 1'000'000ULL * sequence + generation;
    return frame;
}

/// seqFrame on endpoint A, generation 3, with the interval -150 dBm to
/// +0.0 dBm, or to -0.0 dBm when `negativeZero`: the same context by value,
/// another bit pattern on the wire. The planes are dequantized on it.
DisplayCodecFrame signedZeroFrame(quint32 sequence, bool negativeZero)
{
    DisplayCodecFrame frame = seqFrame(kEndpointA, 3, sequence);
    const QVector<quint8> trace = Vectors::quantizeRow(frame.traceDbm);
    const QVector<quint8> waterfall = Vectors::quantizeRow(frame.waterfallDbm);
    frame.context.maxDbm = negativeZero ? -0.0f : 0.0f;
    frame.traceDbm = Vectors::dequantizeRow(trace, frame.context.minDbm, frame.context.maxDbm);
    frame.waterfallDbm =
        Vectors::dequantizeRow(waterfall, frame.context.minDbm, frame.context.maxDbm);
    return frame;
}

bool hasWide(const DisplayCodecFrame& frame) { return frame.context.wideSamples != 0; }

QByteArray whole(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, hasWide(frame) ? quint8 {0x1E} : quint8 {0x1A});
}
QByteArray rowPart(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, hasWide(frame) ? quint8 {0x36} : quint8 {0x32});
}
QByteArray tracePart(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, quint8 {0x2A});
}
QByteArray traceOnly(const DisplayCodecFrame& frame)
{
    return Vectors::buildMessage(frame, quint8 {0x08});
}

QVector<float> zeros(int n)
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

struct Expected {
    const char* disposition;
    const char* reason;
    QList<DisplayCodecFrame> frames;
};

Expected accepted(const QList<DisplayCodecFrame>& frames) { return {"accepted", "none", frames}; }
Expected held() { return {"held", "none", {}}; }
Expected rejected(const char* reason) { return {"rejected", reason, {}}; }

struct Counters {
    quint64 framesLost {0};
    quint64 partsLost {0};
    quint64 staleMessages {0};
    quint64 duplicateParts {0};
    quint64 malformed {0};
};

struct Step {
    QByteArray message;
    Expected expected;
};

QJsonObject sequenceEntry(const QString& name, const QList<Step>& steps, const Counters& counters)
{
    QJsonArray messages;
    QJsonArray results;
    for (const Step& step : steps) {
        messages.append(Vectors::toHex(step.message));
        QJsonObject result;
        result.insert(QStringLiteral("disposition"), QLatin1String(step.expected.disposition));
        result.insert(QStringLiteral("reason"), QLatin1String(step.expected.reason));
        QJsonArray frames;
        for (const DisplayCodecFrame& frame : step.expected.frames) {
            frames.append(Vectors::frameToJson(frame));
        }
        result.insert(QStringLiteral("frames"), frames);
        results.append(result);
    }
    QJsonObject counterObject;
    counterObject.insert(QStringLiteral("framesLost"), static_cast<qint64>(counters.framesLost));
    counterObject.insert(QStringLiteral("partsLost"), static_cast<qint64>(counters.partsLost));
    counterObject.insert(QStringLiteral("staleMessages"), static_cast<qint64>(counters.staleMessages));
    counterObject.insert(QStringLiteral("duplicateParts"), static_cast<qint64>(counters.duplicateParts));
    counterObject.insert(QStringLiteral("malformed"), static_cast<qint64>(counters.malformed));
    QJsonObject entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("messages"), messages);
    entry.insert(QStringLiteral("results"), results);
    entry.insert(QStringLiteral("counters"), counterObject);
    return entry;
}

Counters counters(quint64 framesLost, quint64 partsLost, quint64 staleMessages,
                  quint64 duplicateParts, quint64 malformed)
{
    return {framesLost, partsLost, staleMessages, duplicateParts, malformed};
}

void setU16(QByteArray& bytes, int offset, int value)
{
    bytes[offset] = static_cast<char>((value >> 8) & 0xff);
    bytes[offset + 1] = static_cast<char>(value & 0xff);
}

int u16At(const QByteArray& bytes, int offset)
{
    return (static_cast<int>(static_cast<quint8>(bytes.at(offset))) << 8)
        | static_cast<quint8>(bytes.at(offset + 1));
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

QByteArray planeWithPayload(const QByteArray& payload)
{
    QByteArray plane;
    plane.append(static_cast<char>(2));
    plane.append(static_cast<char>(0));
    plane.append(static_cast<char>((payload.size() >> 8) & 0xff));
    plane.append(static_cast<char>(payload.size() & 0xff));
    plane.append(payload);
    return plane;
}

QJsonArray buildSequences()
{
    QJsonArray out;
    const auto add = [&out](const QString& name, const QList<Step>& steps, const Counters& c) {
        out.append(sequenceEntry(name, steps, c));
    };
    const auto a = [](quint32 sequence, quint32 generation = 3) {
        return seqFrame(kEndpointA, generation, sequence);
    };
    const auto b = [](quint32 sequence, quint32 generation = 5) {
        return seqFrame(kEndpointB, generation, sequence);
    };
    const QVector<float> noTrace = zeros(kTraceLength);
    const QVector<float> noWaterfall = zeros(kWaterfallLength);
    const QVector<float> noWide = zeros(kWideLength);

    // ---- Section 9.2, nothing held ----
    // A whole frame newer than L releases; framesLost counts the gap 2..3.
    add(QStringLiteral("whole-then-whole-after-a-gap"),
        {{whole(a(1)), accepted({a(1)})}, {whole(a(4)), accepted({a(4)})}},
        counters(2, 0, 0, 0, 0));
    // Stale whole and part messages change nothing.
    add(QStringLiteral("stale-whole-and-stale-parts"),
        {{whole(a(5)), accepted({a(5)})},
         {whole(a(5)), rejected("staleSequence")},
         {whole(a(3)), rejected("staleSequence")},
         {rowPart(a(4)), rejected("staleSequence")},
         {tracePart(a(5)), rejected("staleSequence")}},
        counters(0, 0, 4, 0, 0));

    // ---- Pairing: either part first ----
    add(QStringLiteral("row-part-first-then-trace-part"),
        {{rowPart(a(5)), held()}, {tracePart(a(5)), accepted({a(5)})}},
        counters(0, 0, 0, 0, 0));
    add(QStringLiteral("trace-part-first-then-row-part"),
        {{tracePart(a(5)), held()}, {rowPart(a(5)), accepted({a(5)})}},
        counters(0, 0, 0, 0, 0));
    add(QStringLiteral("parts-with-wide-either-order"),
        {{rowPart(b(5)), held()}, {tracePart(b(5)), accepted({b(5)})},
         {tracePart(b(6)), held()}, {rowPart(b(6)), accepted({b(6)})}},
        counters(0, 0, 0, 0, 0));

    // ---- A duplicate part; H stays ----
    add(QStringLiteral("duplicate-row-part"),
        {{rowPart(a(6)), held()},
         {rowPart(a(6)), rejected("staleSequence")},
         {tracePart(a(6)), accepted({a(6)})}},
        counters(0, 0, 0, 1, 0));
    add(QStringLiteral("duplicate-trace-part"),
        {{tracePart(a(6)), held()},
         {tracePart(a(6)), rejected("staleSequence")},
         {rowPart(a(6)), accepted({a(6)})}},
        counters(0, 0, 0, 1, 0));

    // ---- Disagreeing headers; a whole frame with the held sequence ----
    DisplayCodecFrame otherTimestamp = a(6);
    otherTimestamp.producerTimestamp += 1;
    add(QStringLiteral("parts-whose-headers-disagree"),
        {{rowPart(a(6)), held()},
         {tracePart(otherTimestamp), rejected("malformed")},
         {tracePart(a(6)), accepted({a(6)})}},
        counters(0, 0, 0, 0, 1));
    add(QStringLiteral("whole-frame-with-the-held-sequence"),
        {{rowPart(a(6)), held()},
         {whole(a(6)), rejected("malformed")},
         {tracePart(a(6)), accepted({a(6)})}},
        counters(0, 0, 0, 0, 1));

    // ---- Older than the held part, newer than L: released now, H stays ----
    add(QStringLiteral("whole-older-than-the-held-part"),
        {{whole(a(4)), accepted({a(4)})},
         {rowPart(a(6)), held()},
         {whole(a(5)), accepted({a(5)})},
         {tracePart(a(6)), accepted({a(6)})}},
        counters(0, 0, 0, 0, 0));
    add(QStringLiteral("part-older-than-the-held-part"),
        {{whole(a(4)), accepted({a(4)})},
         {rowPart(a(6)), held()},
         {rowPart(a(5)), accepted({rowOnlyRelease(a(5), a(4).traceDbm)})},
         {tracePart(a(6)), accepted({a(6)})},
         {tracePart(a(5)), rejected("staleSequence")}},
        counters(0, 1, 1, 0, 0));

    // ---- A lost trace part (row only released) ----
    add(QStringLiteral("lost-trace-part-then-newer-whole"),
        {{whole(a(5)), accepted({a(5)})},
         {rowPart(a(6)), held()},
         {whole(a(8)), accepted({rowOnlyRelease(a(6), a(5).traceDbm), a(8)})}},
        counters(1, 1, 0, 0, 0));
    // Fresh decoder: the carried trace is minimum dBm (q 0).
    add(QStringLiteral("lost-trace-part-then-newer-part-fresh"),
        {{rowPart(a(6)), held()},
         {tracePart(a(7)), accepted({rowOnlyRelease(a(6), noTrace)})},
         {rowPart(a(7)), accepted({a(7)})}},
        counters(0, 1, 0, 0, 0));

    // ---- A lost row part (trace only released) ----
    add(QStringLiteral("lost-row-part-then-newer-whole"),
        {{whole(a(2)), accepted({a(2)})},
         {tracePart(a(3)), held()},
         {whole(a(4)), accepted({traceOnlyRelease(a(3), a(2).waterfallDbm, {}), a(4)})}},
        counters(0, 1, 0, 0, 0));
    add(QStringLiteral("lost-row-part-fresh"),
        {{tracePart(a(3)), held()},
         {whole(a(4)), accepted({traceOnlyRelease(a(3), noWaterfall, {}), a(4)})}},
        counters(0, 1, 0, 0, 0));
    add(QStringLiteral("lost-row-part-with-wide"),
        {{whole(b(2)), accepted({b(2)})},
         {tracePart(b(3)), held()},
         {rowPart(b(4)), accepted({traceOnlyRelease(b(3), b(2).waterfallDbm, b(2).wideDbm)})},
         {tracePart(b(4)), accepted({b(4)})}},
        counters(0, 1, 0, 0, 0));

    // ---- A whole frame without a row (0x08) ----
    DisplayCodecFrame b3 = b(3);
    add(QStringLiteral("trace-only-whole-fills-from-the-last-row"),
        {{whole(b(2)), accepted({b(2)})},
         {traceOnly(b3), accepted({traceOnlyRelease(b3, b(2).waterfallDbm, b(2).wideDbm)})}},
        counters(0, 0, 0, 0, 0));
    add(QStringLiteral("trace-only-whole-fresh"),
        {{traceOnly(b(1)), accepted({traceOnlyRelease(b(1), noWaterfall, noWide)})}},
        counters(0, 0, 0, 0, 0));

    // ---- A newer generation while a part is held ----
    // H is released under the old context (fills from it, framesLost 6
    // against 4); the new context starts with no L and empty fills.
    add(QStringLiteral("generation-change-while-held-then-trace-only"),
        {{whole(a(4)), accepted({a(4)})},
         {rowPart(a(6)), held()},
         {traceOnly(a(1, 4)),
          accepted({rowOnlyRelease(a(6), a(4).traceDbm),
                    traceOnlyRelease(a(1, 4), noWaterfall, {})})}},
        counters(1, 1, 0, 0, 0));
    add(QStringLiteral("generation-change-while-held-then-part"),
        {{whole(a(4)), accepted({a(4)})},
         {tracePart(a(6)), held()},
         {rowPart(a(2, 4)), accepted({traceOnlyRelease(a(6), a(4).waterfallDbm, {})})},
         {tracePart(a(2, 4)), accepted({a(2, 4)})}},
        counters(1, 1, 0, 0, 0));
    add(QStringLiteral("generation-change-clears-the-fills"),
        {{whole(a(1)), accepted({a(1)})},
         {rowPart(a(1, 4)), held()},
         {whole(a(2, 4)), accepted({rowOnlyRelease(a(1, 4), noTrace), a(2, 4)})}},
        counters(0, 1, 0, 0, 0));
    add(QStringLiteral("older-generation"),
        {{whole(a(1, 4)), accepted({a(1, 4)})},
         {whole(a(9, 3)), rejected("oldContext")},
         {rowPart(a(9, 3)), rejected("oldContext")}},
        counters(0, 0, 0, 0, 0));

    // ---- The float context fields compare by value ----
    // Both decoders compare minDbm and maxDbm as values, not bit patterns:
    // a maxDbm of -0.0 is the context +0.0 started, and parts whose headers
    // differ only there agree.
    add(QStringLiteral("context-floats-compare-by-value"),
        {{whole(signedZeroFrame(1, false)), accepted({signedZeroFrame(1, false)})},
         {whole(signedZeroFrame(2, true)), accepted({signedZeroFrame(2, true)})},
         {rowPart(signedZeroFrame(3, false)), held()},
         {tracePart(signedZeroFrame(3, true)), accepted({signedZeroFrame(3, false)})}},
        counters(0, 0, 0, 0, 0));

    // ---- framesLost across the 2^32 sequence wrap ----
    add(QStringLiteral("frames-lost-across-the-sequence-wrap"),
        {{whole(a(0xFFFFFFFEU)), accepted({a(0xFFFFFFFEU)})},
         {whole(a(1)), accepted({a(1)})}},
        counters(2, 0, 0, 0, 0));

    // ---- Structure before state ----
    QByteArray staleAndMalformed = whole(a(3));
    staleAndMalformed.append('\0');
    add(QStringLiteral("structure-before-state-stale"),
        {{whole(a(4)), accepted({a(4)})}, {staleAndMalformed, rejected("malformed")}},
        counters(0, 0, 0, 0, 1));
    QByteArray oldAndMalformed = whole(a(9, 3));
    oldAndMalformed.append('\0');
    add(QStringLiteral("structure-before-state-old-generation"),
        {{whole(a(1, 4)), accepted({a(1, 4)})}, {oldAndMalformed, rejected("malformed")}},
        counters(0, 0, 0, 0, 1));

    // ---- The malformed set, one message each, from a fresh decoder ----
    // A structural reject counts malformed (section 9.2), whether the v2
    // path finds it or the version dispatch does before any path is chosen
    // (oversized, cut inside the magic or the version, bad magic, another
    // version).
    const QByteArray base = whole(a(1));        // 0x1A, trace 8, waterfall 6
    const QByteArray baseWide = whole(b(1));    // 0x1E, wide 4
    const int tracePlaneBytes =
        static_cast<int>(encodeDisplayCodecV2Plane(Vectors::quantizeRow(a(1).traceDbm)).size());
    const int lastPlane = DisplayCodecEncoder::kHeaderBytes + tracePlaneBytes;
    const auto oneMalformed = [&add](const QString& name, const QByteArray& message,
                                     const char* reason) {
        add(name, {{message, rejected(reason)}}, counters(0, 0, 0, 0, 1));
    };
    for (const int flags : {0x01, 0x40, 0x80}) {
        QByteArray message = base;
        message[5] = static_cast<char>(flags);
        oneMalformed(QStringLiteral("malformed-unknown-flags-%1")
                         .arg(flags, 2, 16, QLatin1Char('0')),
                     message, "unknownFlags");
    }
    const QList<int> valid {0x08, 0x1A, 0x1E, 0x2A, 0x32, 0x36};
    for (int flags = 0; flags <= 0x3E; flags += 2) {
        if (valid.contains(flags)) { continue; }
        QByteArray message = base;
        message[5] = static_cast<char>(flags);
        oneMalformed(QStringLiteral("malformed-flags-%1").arg(flags, 2, 16, QLatin1Char('0')),
                     message, "malformed");
    }
    {
        QByteArray message = base;
        setU16(message, 40, kWideLength);
        oneMalformed(QStringLiteral("malformed-whole-without-wide-flag-but-wide-length"), message,
                     "malformed");
        message = baseWide;
        setU16(message, 40, 0);
        oneMalformed(QStringLiteral("malformed-wide-flag-with-zero-wide-length"), message,
                     "malformed");
    }
    for (const int coding : {1, 3}) {
        QByteArray message = base;
        message[DisplayCodecEncoder::kHeaderBytes] = static_cast<char>(coding);
        oneMalformed(QStringLiteral("malformed-plane-coding-%1").arg(coding), message, "malformed");
    }
    // A NaN interval end is malformed, as in v1 (the interval is finite
    // with maxDbm above minDbm), so no context comparison ever sees one.
    for (const int offset : {28, 32}) {
        QByteArray message = base;
        message[offset] = static_cast<char>(0x7F);
        message[offset + 1] = static_cast<char>(0xC0);
        message[offset + 2] = '\0';
        message[offset + 3] = '\0';
        oneMalformed(offset == 28 ? QStringLiteral("malformed-nan-min-dbm")
                                  : QStringLiteral("malformed-nan-max-dbm"),
                     message, "malformed");
    }
    for (const int code : {3, 255}) {
        QByteArray message = base;
        message[DisplayCodecEncoder::kHeaderBytes + 1] = static_cast<char>(code);
        oneMalformed(QStringLiteral("malformed-block-code-%1").arg(code), message, "malformed");
    }
    {
        QByteArray message = base;
        setU16(message, lastPlane + 2, u16At(message, lastPlane + 2) + 1);
        oneMalformed(QStringLiteral("malformed-payload-overrun"), message, "malformed");

        message = base;
        message.append('\0');
        oneMalformed(QStringLiteral("malformed-byte-after-the-last-plane"), message, "malformed");

        message = base;
        setU16(message, lastPlane + 2, u16At(message, lastPlane + 2) + 1);
        message.append('\0');
        oneMalformed(QStringLiteral("malformed-payload-bytes-after-the-last-sample"), message,
                     "malformed");
    }
    {
        // Trace and waterfall of two samples each; the trace plane is
        // {0, 255} (13 bits) with a pad bit set, or a z above 255.
        DisplayCodecFrame small = encoderFrame(kEndpointA, 3, 1, qRow({0, 255}), qRow({0, 255}),
                                               {}, true);
        const QByteArray good = encodeDisplayCodecV2Plane(qRow({0, 255}));
        QByteArray pad = good;
        pad[5] = static_cast<char>(0x09);
        oneMalformed(QStringLiteral("malformed-nonzero-pad"),
                     Vectors::buildMessage(small, 0x1A, {pad, good}), "malformed");
        const QString zBits = QStringLiteral("00000000") + QStringLiteral("000")
            + QString(256, QLatin1Char('0')) + QStringLiteral("1");
        oneMalformed(QStringLiteral("malformed-z-above-255"),
                     Vectors::buildMessage(small, 0x1A,
                                           {planeWithPayload(bitsToBytes(zBits)), good}),
                     "malformed");
    }
    {
        oneMalformed(QStringLiteral("truncated-in-the-header"), base.left(20), "truncated");
        oneMalformed(QStringLiteral("truncated-in-the-last-plane"), base.left(base.size() - 1),
                     "malformed");
        oneMalformed(QStringLiteral("truncated-split-part-in-the-plane"),
                     rowPart(a(1)).left(DisplayCodecEncoder::kHeaderBytes + 2), "malformed");
        QByteArray message = base;
        setU16(message, 6, DisplayCodecEncoder::kHeaderBytes + 1);
        oneMalformed(QStringLiteral("malformed-header-size"), message, "malformed");
    }
    {
        // Decided at the version dispatch.
        QByteArray message = base;
        message[0] = 'X';
        oneMalformed(QStringLiteral("bad-magic"), message, "badMagic");
        message = base;
        message[4] = static_cast<char>(3);
        oneMalformed(QStringLiteral("unsupported-version"), message, "unsupportedVersion");
        message = base;
        message.append(QByteArray(DisplayCodecEncoder::kMaxEncodedBytes + 1 - message.size(), '\0'));
        oneMalformed(QStringLiteral("oversized"), message, "oversized");
        oneMalformed(QStringLiteral("truncated-in-the-magic"), base.left(2), "truncated");
    }
    return out;
}

} // namespace

class TstDisplayCodecV2Regen : public QObject {
    Q_OBJECT

private slots:
    void writeVectors();
};

void TstDisplayCodecV2Regen::writeVectors()
{
    // ---- planes ----
    QJsonArray planes;
    planes.append(planeEntry(QStringLiteral("hand-one-sample"), qRow({100})));
    planes.append(planeEntry(QStringLiteral("hand-three-samples"), qRow({100, 101, 99})));
    planes.append(planeEntry(QStringLiteral("hand-wrap"), qRow({0, 255})));
    planes.append(planeEntry(QStringLiteral("rice-tie-to-smaller-k"), qRow({0, 32})));
    planes.append(planeEntry(QStringLiteral("raw-escape"), qRow({0, 128})));
    for (const int n : {783, 4096}) {
        planes.append(planeEntry(QStringLiteral("noise-%1").arg(n), noiseRow(n, 1234)));
        planes.append(planeEntry(QStringLiteral("flat-%1").arg(n), QVector<quint8>(n, 93)));
        planes.append(planeEntry(QStringLiteral("ramp-%1").arg(n), rampRow(n)));
        planes.append(planeEntry(QStringLiteral("alternating-%1").arg(n), alternatingRow(n)));
        planes.append(planeEntry(QStringLiteral("full-scale-%1").arg(n), QVector<quint8>(n, 255)));
    }
    planes.append(planeEntry(QStringLiteral("block-16-wins"), burstRow(513, 16)));
    planes.append(planeEntry(QStringLiteral("block-32-wins"), burstRow(769, 32)));
    planes.append(planeEntry(QStringLiteral("block-64-wins"), noiseRow(783, 77)));
    QSet<int> blockSizes;
    for (const QJsonValue& plane : planes) {
        blockSizes.insert(plane.toObject().value(QStringLiteral("blockSize")).toInt());
    }
    QCOMPARE(blockSizes, (QSet<int> {16, 32, 64}));

    // ---- frames ----
    QJsonArray frames;
    {
        // 0x08: the waterfall input is q 0, so a fresh decoder's fill and the
        // input agree.
        const DisplayCodecFrame frame = encoderFrame(0x00000303U, 9, 77, noiseRow(40, 5),
                                                     QVector<quint8>(40, 0), {}, false);
        frames.append(frameEntry(QStringLiteral("whole-trace-only"), frame,
                                 spectrumDisplayCost(40, 30, false)->maximumFrameBytes));
    }
    QVector<quint8> smallTrace(48);
    QVector<quint8> smallWaterfall(48);
    for (int i = 0; i < 48; ++i) {
        smallTrace[i] = static_cast<quint8>(60 + (i * 7) % 23);
        smallWaterfall[i] = static_cast<quint8>(40 + (i * 11) % 31);
    }
    frames.append(frameEntry(QStringLiteral("whole-trace-and-waterfall"),
                             encoderFrame(0x00000303U, 9, 78, smallTrace, smallWaterfall, {}, true),
                             spectrumDisplayCost(48, 30, false)->maximumFrameBytes));
    frames.append(frameEntry(QStringLiteral("whole-with-wide"),
                             encoderFrame(0x00000303U, 9, 79, smallTrace, smallWaterfall,
                                          noiseRow(16, 6), true),
                             spectrumDisplayCost(48, 30, true)->maximumFrameBytes));
    QVector<float> traceDbm;
    QVector<float> waterfallDbm;
    Vectors::SceneRun peak(Vectors::Scene::PeakBins167);
    peak.next(traceDbm, waterfallDbm);
    const QVector<quint8> peakTrace = Vectors::quantizeRow(traceDbm);
    const QVector<quint8> peakWaterfall = Vectors::quantizeRow(waterfallDbm);
    frames.append(frameEntry(QStringLiteral("split-peak-783"),
                             encoderFrame(0x00000404U, 2, 1000, peakTrace, peakWaterfall, {}, true),
                             spectrumDisplayCost(783, 30, false)->maximumFrameBytes));
    Vectors::SceneRun wideRun(Vectors::Scene::PeakBins167, 768, 99);
    QVector<float> wideDbm;
    QVector<float> unused;
    wideRun.next(unused, wideDbm);
    frames.append(frameEntry(QStringLiteral("split-peak-783-with-wide-768"),
                             encoderFrame(0x00000404U, 2, 1001, peakTrace, peakWaterfall,
                                          Vectors::quantizeRow(wideDbm), true),
                             spectrumDisplayCost(783, 30, true)->maximumFrameBytes));
    Vectors::SceneRun flat(Vectors::Scene::Flat);
    flat.next(traceDbm, waterfallDbm);
    frames.append(frameEntry(QStringLiteral("whole-flat-783"),
                             encoderFrame(0x00000404U, 2, 1002, Vectors::quantizeRow(traceDbm),
                                          Vectors::quantizeRow(waterfallDbm), {}, true),
                             spectrumDisplayCost(783, 30, false)->maximumFrameBytes));
    {
        const DisplayCodecFrame frame = encoderFrame(0x00000404U, 2, 1003, noiseRow(600, 11),
                                                     noiseRow(600, 12), {}, true);
        const auto wholeBytes = static_cast<quint32>(encodeDisplayCodecV2(frame, 0).totalBytes());
        frames.append(frameEntry(QStringLiteral("whole-noise-600-admitted-its-whole-size"), frame,
                                 wholeBytes));
    }
    QSet<QString> shapes;
    for (const QJsonValue& value : frames) {
        const QJsonArray messages = value.toObject().value(QStringLiteral("messages")).toArray();
        QStringList flags;
        for (const QJsonValue& message : messages) {
            flags.append(message.toString().mid(10, 2));
        }
        shapes.insert(flags.join(QLatin1Char('+')));
    }
    for (const char* shape : {"08", "1a", "1e", "32+2a", "36+2a"}) {
        QVERIFY2(shapes.contains(QLatin1String(shape)), shape);
    }

    QJsonObject document;
    document.insert(QStringLiteral("format"), QLatin1String(Vectors::kFormat));
    document.insert(QStringLiteral("schema"), Vectors::kSchemaVersion);
    document.insert(QStringLiteral("planes"), planes);
    document.insert(QStringLiteral("frames"), frames);
    document.insert(QStringLiteral("sequences"), buildSequences());

    QTemporaryDir scratch;
    const QByteArray requested = qgetenv("NEREUS_DISPLAY_CODEC_V2_REGEN_OUT");
    const QString directory =
        requested.isEmpty() ? scratch.path() : QString::fromLocal8Bit(requested);
    QVERIFY2(!directory.isEmpty(), "no output directory");
    QVERIFY(QDir().mkpath(directory));
    const QString path = QDir(directory).filePath(QStringLiteral("vectors.json"));
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(path));
    const QByteArray json = Vectors::writeVectorsJson(document);
    QCOMPARE(file.write(json), json.size());
    file.close();

    // What was written parses back to the same document.
    QJsonObject reread;
    QString error;
    QVERIFY2(Vectors::readVectors(path, reread, error), qPrintable(error));
    QCOMPARE(reread, document);
}

QTEST_APPLESS_MAIN(TstDisplayCodecV2Regen)
#include "tst_display_codec_v2_regen.moc"
