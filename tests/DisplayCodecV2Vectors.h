// no-port-check: NereusSDR-original.
// =================================================================
// tests/DisplayCodecV2Vectors.h  (NereusSDR)
// =================================================================
//
// Display codec v2 Task 1: shared helpers for the NSDC v2 tests and the
// golden vector file tests/data/display_codec_v2/vectors.json, which the
// C++ tests and the phone's NereusKit tests both read.
//
// - vectorsPath(): where the file lives.
// - The scene generator of the v2 document's section 6, written to be
//   portable: splitmix64 seeded with 1234, and the inverse CDF for the
//   exponential noise, so another language can reproduce the rows.
// - The JSON schema (version 1) and a writer that keeps sample arrays on
//   one line so a diff of the file stays readable.
// - A message builder that writes any v2 flag byte around given planes, for
//   the hand-stated decoder sequences and the malformed set.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 golden vectors, scene generator and JSON schema.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/session/media/DisplayCodec.h"

namespace NereusSDR::Test::DisplayCodecV2Vectors {

inline constexpr float kMinDbm = -150.0f;
inline constexpr float kMaxDbm = -30.0f;
inline constexpr quint64 kSceneSeed = 1234;
inline constexpr int kSchemaVersion = 1;
inline constexpr char kFormat[] = "nsdc2-vectors";

inline QString vectorsPath()
{
    return QStringLiteral(NEREUS_TEST_DATA_DIR "/display_codec_v2/vectors.json");
}

// ---- Portable random numbers ----

/// splitmix64 (Steele, Lea and Flood): a 64-bit state advanced by the golden
/// gamma and mixed. Every operation is exact integer arithmetic.
class SplitMix64 {
public:
    static constexpr quint64 kGamma = 0x9E3779B97F4A7C15ULL;

    explicit SplitMix64(quint64 seed) : m_state(seed) {}

    static quint64 mix(quint64 z)
    {
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    quint64 next()
    {
        m_state += kGamma;
        return mix(m_state);
    }
    /// [0, 1) from the top 53 bits.
    double uniform() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
    /// Exponential with mean 1 by the inverse CDF.
    double exponential() { return -std::log(1.0 - uniform()); }
    /// A quantized sample, 0 to 255, from the top 8 bits.
    quint8 byte() { return static_cast<quint8>(next() >> 56); }

private:
    quint64 m_state;
};

// ---- Scenes (v2 document, sections 3 and 6) ----

enum class Scene {
    PeakBins167,                 // Peak, 167 bins/px
    PeakBins167TraceAveraged,    // Peak, trace averaged
    AverageBins167,              // Average, 167 bins/px
    AverageBins167TraceAveraged, // Average, trace averaged
    PeakIir,                     // Peak IIR 0.3 on both
    AverageBins8,                // Average, 8 bins/px
    AverageBins8TraceAveraged,   // Average, 8 bins/px, trace averaged
    Raw,                         // 1 bin/px
    UniformNoise,
    Flat,
    Ramp,                        // drifting ramp
    Average1024,                 // 1,024 px, 128 bins/px average
    Peak4096,                    // 4,096 px, 32 bins/px peak
};

enum class SceneKind { Band, Noise, Flat, Ramp };

struct SceneShape {
    int pixels {783};
    int binsPerPixel {1};
    bool peak {false};
    /// IIR on dBm applied to the row (trace and waterfall alike); 1 is none.
    double rowAlpha {1.0};
    /// IIR on dBm applied to the trace only; 1 is none.
    double traceAlpha {1.0};
    SceneKind kind {SceneKind::Band};
};

inline constexpr double kTraceAveragingAlpha = 0.12;

inline SceneShape sceneShape(Scene scene)
{
    switch (scene) {
    case Scene::PeakBins167: return {783, 167, true, 1.0, 1.0, SceneKind::Band};
    case Scene::PeakBins167TraceAveraged:
        return {783, 167, true, 1.0, kTraceAveragingAlpha, SceneKind::Band};
    case Scene::AverageBins167: return {783, 167, false, 1.0, 1.0, SceneKind::Band};
    case Scene::AverageBins167TraceAveraged:
        return {783, 167, false, 1.0, kTraceAveragingAlpha, SceneKind::Band};
    case Scene::PeakIir: return {783, 167, true, 0.3, 1.0, SceneKind::Band};
    case Scene::AverageBins8: return {783, 8, false, 1.0, 1.0, SceneKind::Band};
    case Scene::AverageBins8TraceAveraged:
        return {783, 8, false, 1.0, kTraceAveragingAlpha, SceneKind::Band};
    case Scene::Raw: return {783, 1, false, 1.0, 1.0, SceneKind::Band};
    case Scene::UniformNoise: return {783, 1, false, 1.0, 1.0, SceneKind::Noise};
    case Scene::Flat: return {783, 1, false, 1.0, 1.0, SceneKind::Flat};
    case Scene::Ramp: return {783, 1, false, 1.0, 1.0, SceneKind::Ramp};
    case Scene::Average1024: return {1024, 128, false, 1.0, 1.0, SceneKind::Band};
    case Scene::Peak4096: return {4096, 32, true, 1.0, 1.0, SceneKind::Band};
    }
    return {};
}

/// The salt that separates a frame's noise stream from the carrier stream.
inline constexpr quint64 kNoiseSalt = 0x6A09E667F3BCC909ULL;

/// The seed of frame `frame`'s noise stream: the (frame + 1)-th output of
/// splitmix64 seeded with `seed ^ kNoiseSalt`, computed directly.
inline quint64 sceneNoiseSeed(quint64 seed, int frame)
{
    return SplitMix64::mix((seed ^ kNoiseSalt)
                           + SplitMix64::kGamma * (static_cast<quint64>(frame) + 1));
}

/// One frame's row of `scene` at `pixels` pixels, in dBm, before any IIR
/// (SceneRun applies the scene's row and trace averaging). A pure function of
/// its arguments: the carriers come from splitmix64(seed), replayed from
/// frame 0, and the per-bin noise from splitmix64(sceneNoiseSeed(seed, frame)).
///
/// Band scenes: the floor -125 + 3 sin(i*0.004*783/N) + sin(i*0.05*783/N);
/// per 783 pixels 12 carriers (SNR 10 + 40u), 4 keyed CW (toggle p 0.15, SNR
/// 20 + 20u) and 2 SSB 15 px wide (SNR 25, level -60 with p 0.3, else -10u),
/// carriers and CW Gaussian of width 1.2; each bin is floor power times an
/// exponential draw plus the signal power, and a pixel is the max (peak) or
/// the mean (average) of its bins.
inline QVector<float> sceneRow(Scene scene, int pixels, quint64 seed, int frame)
{
    const SceneShape shape = sceneShape(scene);
    const int n = pixels;
    QVector<float> row(n, 0.0f);
    if (n <= 0) { return row; }
    SplitMix64 noise(sceneNoiseSeed(seed, frame));
    const double range = static_cast<double>(kMaxDbm) - static_cast<double>(kMinDbm);
    switch (shape.kind) {
    case SceneKind::Noise:
        for (int i = 0; i < n; ++i) {
            row[i] = static_cast<float>(static_cast<double>(kMinDbm) + range * noise.uniform());
        }
        return row;
    case SceneKind::Flat:
        std::fill(row.begin(), row.end(), -110.0f);
        return row;
    case SceneKind::Ramp:
        for (int i = 0; i < n; ++i) {
            const int step = (i + (frame + 1) * 3) % n;
            row[i] = static_cast<float>(-140.0 + 100.0 * step / static_cast<double>(n));
        }
        return row;
    case SceneKind::Band:
        break;
    }

    enum class CarrierKind { Steady, KeyedCw, Ssb };
    struct Carrier {
        CarrierKind kind {CarrierKind::Steady};
        int pixel {0};
        double snr {0.0};
        bool on {true};
        double level {0.0};
    };
    SplitMix64 carriersRng(seed);
    QVector<Carrier> carriers;
    const int scale = std::max(1, n / 783);
    for (int c = 0; c < 12 * scale; ++c) {
        Carrier carrier;
        carrier.pixel = static_cast<int>(carriersRng.uniform() * n);
        carrier.snr = 10.0 + 40.0 * carriersRng.uniform();
        carriers.append(carrier);
    }
    for (int c = 0; c < 4 * scale; ++c) {
        Carrier carrier;
        carrier.kind = CarrierKind::KeyedCw;
        carrier.pixel = static_cast<int>(carriersRng.uniform() * n);
        carrier.snr = 20.0 + 20.0 * carriersRng.uniform();
        carriers.append(carrier);
    }
    for (int c = 0; c < 2 * scale; ++c) {
        Carrier carrier;
        carrier.kind = CarrierKind::Ssb;
        carrier.pixel = static_cast<int>(carriersRng.uniform() * n);
        carrier.snr = 25.0;
        carriers.append(carrier);
    }
    for (int step = 0; step <= frame; ++step) {
        for (Carrier& carrier : carriers) {
            if (carrier.kind == CarrierKind::KeyedCw) {
                if (carriersRng.uniform() < 0.15) { carrier.on = !carrier.on; }
            } else if (carrier.kind == CarrierKind::Ssb) {
                carrier.level = carriersRng.uniform() < 0.3 ? -60.0 : -10.0 * carriersRng.uniform();
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        const double floorDbm = -125.0 + 3.0 * std::sin(i * 0.004 * 783.0 / n)
            + 1.0 * std::sin(i * 0.05 * 783.0 / n);
        const double floorPower = std::pow(10.0, floorDbm / 10.0);
        double signal = 0.0;
        for (const Carrier& carrier : carriers) {
            if (carrier.kind == CarrierKind::Ssb) {
                if (i >= carrier.pixel && i < carrier.pixel + 15) {
                    signal += floorPower * std::pow(10.0, (carrier.snr + carrier.level) / 10.0);
                }
                continue;
            }
            if (!carrier.on) { continue; }
            const double d = (i - carrier.pixel) / 1.2;
            signal += floorPower * std::pow(10.0, carrier.snr / 10.0) * std::exp(-d * d);
        }
        double accumulated = 0.0;
        for (int b = 0; b < shape.binsPerPixel; ++b) {
            const double bin = floorPower * noise.exponential() + signal;
            accumulated = shape.peak ? std::max(accumulated, bin) : accumulated + bin;
        }
        if (!shape.peak) { accumulated /= shape.binsPerPixel; }
        row[i] = static_cast<float>(10.0 * std::log10(accumulated));
    }
    return row;
}

/// A run of a scene, frame after frame: the row IIR (rowAlpha) on the
/// waterfall and trace alike, then the trace-only IIR (traceAlpha). Both start
/// from the first frame's row.
class SceneRun {
public:
    explicit SceneRun(Scene scene, int pixels = 0, quint64 seed = kSceneSeed)
        : m_scene(scene),
          m_shape(sceneShape(scene)),
          m_pixels(pixels > 0 ? pixels : sceneShape(scene).pixels),
          m_seed(seed) {}

    int pixels() const { return m_pixels; }

    void next(QVector<float>& trace, QVector<float>& waterfall)
    {
        const QVector<float> row = sceneRow(m_scene, m_pixels, m_seed, m_frame);
        if (m_frame == 0) {
            m_row = QVector<double>(row.cbegin(), row.cend());
            m_trace = m_row;
        } else {
            for (int i = 0; i < m_pixels; ++i) {
                m_row[i] = m_shape.rowAlpha * row.at(i) + (1.0 - m_shape.rowAlpha) * m_row.at(i);
                m_trace[i] = m_shape.traceAlpha * m_row.at(i)
                    + (1.0 - m_shape.traceAlpha) * m_trace.at(i);
            }
        }
        ++m_frame;
        trace.resize(m_pixels);
        waterfall.resize(m_pixels);
        for (int i = 0; i < m_pixels; ++i) {
            trace[i] = static_cast<float>(m_trace.at(i));
            waterfall[i] = static_cast<float>(m_row.at(i));
        }
    }

private:
    Scene m_scene;
    SceneShape m_shape;
    int m_pixels;
    quint64 m_seed;
    int m_frame {0};
    QVector<double> m_row;
    QVector<double> m_trace;
};

// ---- Quantized planes ----

inline QVector<quint8> quantizeRow(const QVector<float>& dbm, float minDbm = kMinDbm,
                                   float maxDbm = kMaxDbm)
{
    QVector<quint8> q;
    q.reserve(dbm.size());
    for (float value : dbm) { q.append(displayCodecQuantize(value, minDbm, maxDbm)); }
    return q;
}

inline QVector<float> dequantizeRow(const QVector<quint8>& q, float minDbm = kMinDbm,
                                    float maxDbm = kMaxDbm)
{
    QVector<float> dbm;
    dbm.reserve(q.size());
    for (quint8 value : q) { dbm.append(displayCodecDequantize(value, minDbm, maxDbm)); }
    return dbm;
}

// ---- Messages ----

inline void appendU8(QByteArray& out, quint8 value) { out.append(static_cast<char>(value)); }
inline void appendU16(QByteArray& out, quint16 value)
{
    appendU8(out, static_cast<quint8>(value >> 8));
    appendU8(out, static_cast<quint8>(value));
}
inline void appendU32(QByteArray& out, quint32 value)
{
    appendU16(out, static_cast<quint16>(value >> 16));
    appendU16(out, static_cast<quint16>(value));
}

/// A v2 message: the 42-byte header with `flags` and the frame's fields,
/// then `planes` as given. Writes any flag byte, valid or not.
inline QByteArray buildMessage(const DisplayCodecFrame& frame, quint8 flags,
                               const QList<QByteArray>& planes)
{
    QByteArray out;
    appendU32(out, 0x4e534443U); // "NSDC"
    appendU8(out, 2);
    appendU8(out, flags);
    appendU16(out, DisplayCodecEncoder::kHeaderBytes);
    appendU32(out, frame.context.endpointId);
    appendU32(out, frame.context.contextGeneration);
    appendU32(out, frame.encoderSequence);
    appendU32(out, static_cast<quint32>(frame.producerTimestamp >> 32));
    appendU32(out, static_cast<quint32>(frame.producerTimestamp));
    quint32 bits = 0;
    std::memcpy(&bits, &frame.context.minDbm, sizeof(bits));
    appendU32(out, bits);
    std::memcpy(&bits, &frame.context.maxDbm, sizeof(bits));
    appendU32(out, bits);
    appendU16(out, frame.context.traceSamples);
    appendU16(out, frame.context.waterfallSamples);
    appendU16(out, frame.context.wideSamples);
    for (const QByteArray& plane : planes) { out.append(plane); }
    return out;
}

/// A v2 message whose planes follow `flags`: the trace for 0x08, the
/// waterfall for 0x10 and the wide row for 0x04, each coded by
/// encodeDisplayCodecV2Plane().
inline QByteArray buildMessage(const DisplayCodecFrame& frame, quint8 flags)
{
    const float minDbm = frame.context.minDbm;
    const float maxDbm = frame.context.maxDbm;
    QList<QByteArray> planes;
    if ((flags & 0x08) != 0) {
        planes.append(encodeDisplayCodecV2Plane(quantizeRow(frame.traceDbm, minDbm, maxDbm)));
    }
    if ((flags & 0x10) != 0) {
        planes.append(encodeDisplayCodecV2Plane(quantizeRow(frame.waterfallDbm, minDbm, maxDbm)));
    }
    if ((flags & 0x04) != 0) {
        planes.append(encodeDisplayCodecV2Plane(quantizeRow(frame.wideDbm, minDbm, maxDbm)));
    }
    return buildMessage(frame, flags, planes);
}

// ---- JSON (schema 1) ----

inline QString toHex(const QByteArray& bytes) { return QString::fromLatin1(bytes.toHex()); }
inline QByteArray fromHex(const QString& hex) { return QByteArray::fromHex(hex.toLatin1()); }

inline QJsonArray toJsonArray(const QVector<quint8>& q)
{
    QJsonArray array;
    for (quint8 value : q) { array.append(static_cast<int>(value)); }
    return array;
}

inline QVector<quint8> fromJsonArray(const QJsonArray& array)
{
    QVector<quint8> q;
    q.reserve(array.size());
    for (const QJsonValue& value : array) { q.append(static_cast<quint8>(value.toInt())); }
    return q;
}

/// FRAME: the header fields and the three planes as quantized values.
inline QJsonObject frameToJson(const DisplayCodecFrame& frame)
{
    const DisplayCodecContext& c = frame.context;
    QJsonObject object;
    object.insert(QStringLiteral("endpointId"), static_cast<qint64>(c.endpointId));
    object.insert(QStringLiteral("contextGeneration"), static_cast<qint64>(c.contextGeneration));
    object.insert(QStringLiteral("encoderSequence"), static_cast<qint64>(frame.encoderSequence));
    object.insert(QStringLiteral("producerTimestamp"), QString::number(frame.producerTimestamp));
    object.insert(QStringLiteral("minDbm"), static_cast<double>(c.minDbm));
    object.insert(QStringLiteral("maxDbm"), static_cast<double>(c.maxDbm));
    object.insert(QStringLiteral("traceSamples"), static_cast<int>(c.traceSamples));
    object.insert(QStringLiteral("waterfallSamples"), static_cast<int>(c.waterfallSamples));
    object.insert(QStringLiteral("wideSamples"), static_cast<int>(c.wideSamples));
    object.insert(QStringLiteral("waterfallAdvance"), frame.waterfallAdvance);
    object.insert(QStringLiteral("traceCarried"), frame.traceCarried);
    object.insert(QStringLiteral("trace"),
                  toJsonArray(quantizeRow(frame.traceDbm, c.minDbm, c.maxDbm)));
    object.insert(QStringLiteral("waterfall"),
                  toJsonArray(quantizeRow(frame.waterfallDbm, c.minDbm, c.maxDbm)));
    object.insert(QStringLiteral("wide"),
                  toJsonArray(quantizeRow(frame.wideDbm, c.minDbm, c.maxDbm)));
    return object;
}

/// The frame a FRAME object states, each plane value the dequantization of
/// its q, so encoding it quantizes back to exactly the stated q.
inline DisplayCodecFrame frameFromJson(const QJsonObject& object)
{
    DisplayCodecFrame frame;
    DisplayCodecContext& c = frame.context;
    c.endpointId = static_cast<quint32>(object.value(QStringLiteral("endpointId")).toInteger());
    c.contextGeneration =
        static_cast<quint32>(object.value(QStringLiteral("contextGeneration")).toInteger());
    frame.encoderSequence =
        static_cast<quint32>(object.value(QStringLiteral("encoderSequence")).toInteger());
    frame.producerTimestamp =
        object.value(QStringLiteral("producerTimestamp")).toString().toULongLong();
    c.minDbm = static_cast<float>(object.value(QStringLiteral("minDbm")).toDouble());
    c.maxDbm = static_cast<float>(object.value(QStringLiteral("maxDbm")).toDouble());
    c.traceSamples = static_cast<quint16>(object.value(QStringLiteral("traceSamples")).toInt());
    c.waterfallSamples =
        static_cast<quint16>(object.value(QStringLiteral("waterfallSamples")).toInt());
    c.wideSamples = static_cast<quint16>(object.value(QStringLiteral("wideSamples")).toInt());
    frame.waterfallAdvance = object.value(QStringLiteral("waterfallAdvance")).toBool();
    frame.traceCarried = object.value(QStringLiteral("traceCarried")).toBool();
    frame.traceDbm = dequantizeRow(fromJsonArray(object.value(QStringLiteral("trace")).toArray()),
                                   c.minDbm, c.maxDbm);
    frame.waterfallDbm =
        dequantizeRow(fromJsonArray(object.value(QStringLiteral("waterfall")).toArray()),
                      c.minDbm, c.maxDbm);
    frame.wideDbm = dequantizeRow(fromJsonArray(object.value(QStringLiteral("wide")).toArray()),
                                  c.minDbm, c.maxDbm);
    return frame;
}

inline QJsonArray toJsonHexArray(const std::vector<QByteArray>& messages)
{
    QJsonArray array;
    for (const QByteArray& message : messages) { array.append(toHex(message)); }
    return array;
}

namespace detail {

inline void appendString(QByteArray& out, const QString& value)
{
    out.append('"');
    for (const QChar ch : value) {
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\\')) { out.append('\\'); }
        out.append(ch.toLatin1());
    }
    out.append('"');
}

inline void appendNumber(QByteArray& out, double value)
{
    if (std::floor(value) == value && std::fabs(value) < 9007199254740992.0) {
        out.append(QByteArray::number(static_cast<qint64>(value)));
    } else {
        out.append(QByteArray::number(value, 'g', 17));
    }
}

inline void appendValue(QByteArray& out, const QJsonValue& value, int indent)
{
    const QByteArray pad(indent, ' ');
    const QByteArray inner(indent + 2, ' ');
    switch (value.type()) {
    case QJsonValue::Object: {
        const QJsonObject object = value.toObject();
        if (object.isEmpty()) {
            out.append("{}");
            return;
        }
        out.append("{\n");
        int index = 0;
        for (auto it = object.constBegin(); it != object.constEnd(); ++it, ++index) {
            out.append(inner);
            appendString(out, it.key());
            out.append(": ");
            appendValue(out, it.value(), indent + 2);
            out.append(index + 1 < object.size() ? ",\n" : "\n");
        }
        out.append(pad);
        out.append('}');
        return;
    }
    case QJsonValue::Array: {
        const QJsonArray array = value.toArray();
        if (array.isEmpty()) {
            out.append("[]");
            return;
        }
        const bool numbers = std::all_of(array.cbegin(), array.cend(),
                                         [](const QJsonValue& v) { return v.isDouble(); });
        if (numbers) {
            // Sample arrays stay on one line.
            out.append('[');
            for (int i = 0; i < array.size(); ++i) {
                if (i > 0) { out.append(", "); }
                appendNumber(out, array.at(i).toDouble());
            }
            out.append(']');
            return;
        }
        out.append("[\n");
        for (int i = 0; i < array.size(); ++i) {
            out.append(inner);
            appendValue(out, array.at(i), indent + 2);
            out.append(i + 1 < array.size() ? ",\n" : "\n");
        }
        out.append(pad);
        out.append(']');
        return;
    }
    case QJsonValue::String: appendString(out, value.toString()); return;
    case QJsonValue::Double: appendNumber(out, value.toDouble()); return;
    case QJsonValue::Bool: out.append(value.toBool() ? "true" : "false"); return;
    case QJsonValue::Null:
    case QJsonValue::Undefined: out.append("null"); return;
    }
}

} // namespace detail

/// The file's bytes: two-space indentation, sample arrays on one line, keys
/// in Qt's sorted order, a final newline.
inline QByteArray writeVectorsJson(const QJsonObject& document)
{
    QByteArray out;
    detail::appendValue(out, document, 0);
    out.append('\n');
    return out;
}

/// Reads and parses the file; false with `error` set when it cannot.
inline bool readVectors(const QString& path, QJsonObject& document, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("cannot open %1").arg(path);
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !json.isObject()) {
        error = QStringLiteral("%1: %2").arg(path, parse.errorString());
        return false;
    }
    document = json.object();
    if (document.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)
        || document.value(QStringLiteral("schema")).toInt() != kSchemaVersion) {
        error = QStringLiteral("%1: not %2 schema %3").arg(path, QLatin1String(kFormat))
                    .arg(kSchemaVersion);
        return false;
    }
    return true;
}

} // namespace NereusSDR::Test::DisplayCodecV2Vectors
