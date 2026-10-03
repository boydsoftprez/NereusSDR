// =================================================================
// src/core/session/media/DisplayExtras.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. iPhone app Task 20 (R-IOS-27).
// See DisplayExtras.h and docs/architecture/2026-09-23-display-extras-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  displayCostWithExtras, shared with a
//                                    remote window's planner. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Created for iPhone app Task 20.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: Clarity's
//                                    Re-tune for one endpoint
//                                    (displayExtrasVersion 2).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-18: version 4, the noise floor
//                                    state section (fast attack) asked by
//                                    noiseFloor.fastAttack. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-18: normalise applies only
//                                    with the Average, Sample and RMS
//                                    trace detectors, as the desktop and
//                                    Thetis do. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "core/session/media/DisplayExtras.h"

#include "core/ClarityController.h"

#include <QJsonValue>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>

namespace NereusSDR {

namespace {

constexpr quint32 kMagic = 0x4e534458U; // "NSDX"

bool finiteNumber(const QJsonValue& value, double& result)
{
    if (!value.isDouble()) {
        return false;
    }
    result = value.toDouble();
    return std::isfinite(result);
}

bool numberIn(const QJsonValue& value, double minimum, double maximum, double& result)
{
    double number = 0.0;
    if (!finiteNumber(value, number) || number < minimum || number > maximum) {
        return false;
    }
    result = number;
    return true;
}

bool intIn(const QJsonValue& value, int minimum, int maximum, int& result)
{
    double number = 0.0;
    if (!finiteNumber(value, number) || number < minimum || number > maximum
        || std::floor(number) != number) {
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool boolValue(const QJsonValue& value, bool& result)
{
    if (!value.isBool()) {
        return false;
    }
    result = value.toBool();
    return true;
}

bool objectWithKeys(const QJsonValue& value, std::initializer_list<const char*> keys,
                    QJsonObject& object)
{
    if (!value.isObject()) {
        return false;
    }
    object = value.toObject();
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const char* key : keys) {
        if (!object.contains(QLatin1String(key))) {
            return false;
        }
    }
    return true;
}

bool finite(float value)
{
    return std::isfinite(static_cast<double>(value));
}

void appendU8(QByteArray& bytes, quint8 value)
{
    bytes.append(static_cast<char>(value));
}

void appendU16(QByteArray& bytes, quint16 value)
{
    appendU8(bytes, static_cast<quint8>(value >> 8));
    appendU8(bytes, static_cast<quint8>(value));
}

void appendU32(QByteArray& bytes, quint32 value)
{
    appendU16(bytes, static_cast<quint16>(value >> 16));
    appendU16(bytes, static_cast<quint16>(value));
}

void appendF32(QByteArray& bytes, float value)
{
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(bytes, bits);
}

class Reader {
public:
    explicit Reader(const QByteArray& bytes) : m_bytes(bytes) {}
    bool u8(quint8& value)
    {
        if (m_pos >= m_bytes.size()) { return false; }
        value = static_cast<quint8>(static_cast<unsigned char>(m_bytes.at(m_pos++)));
        return true;
    }
    bool u16(quint16& value)
    {
        quint8 a = 0;
        quint8 b = 0;
        if (!u8(a) || !u8(b)) { return false; }
        value = static_cast<quint16>((static_cast<quint16>(a) << 8) | b);
        return true;
    }
    bool u32(quint32& value)
    {
        quint16 a = 0;
        quint16 b = 0;
        if (!u16(a) || !u16(b)) { return false; }
        value = (static_cast<quint32>(a) << 16) | b;
        return true;
    }
    bool f32(float& value)
    {
        quint32 bits = 0;
        if (!u32(bits)) { return false; }
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    int position() const { return m_pos; }
    void setPosition(int position) { m_pos = position; }
    bool atEnd() const { return m_pos == m_bytes.size(); }

private:
    const QByteArray& m_bytes;
    int m_pos {0};
};

DisplayExtrasDecodeResult rejected(DisplayExtrasReason reason)
{
    DisplayExtrasDecodeResult result;
    result.accepted = false;
    result.reason = reason;
    return result;
}

int planeWorstCaseBytes(int samples)
{
    // Every block absolute at 128 samples: the 3-byte plane prefix, a
    // 5-byte header per block and one byte per sample (display codec v1,
    // "Measured sizes and fragments").
    return samples <= 0 ? 0 : 3 + 5 * ((samples + 127) / 128) + samples;
}

} // namespace

// ── The subscription fields ─────────────────────────────────────────────

bool DisplayExtrasRequest::empty() const
{
    return !peakBlobs && !activePeakHold && !noiseFloor && !waterfallLevels
        && !normalize && !calibrationOffsetDb && !averageTimeMs && !waterfallAverageTimeMs;
}

quint8 DisplayExtrasRequest::sections() const
{
    quint8 sections = 0;
    if (peakBlobs) { sections |= kDisplayExtrasPeakBlobs; }
    if (activePeakHold && activePeakHold->enabled) { sections |= kDisplayExtrasPeakHold; }
    if (noiseFloor && noiseFloor->enabled) { sections |= kDisplayExtrasNoiseFloor; }
    if (waterfallLevels) { sections |= kDisplayExtrasWaterfallLevels; }
    if (noiseFloor && noiseFloor->enabled && noiseFloor->fastAttack) {
        sections |= kDisplayExtrasNoiseFloorState;
    }
    return sections;
}

const QStringList& displayExtrasSubscribeKeys()
{
    static const QStringList keys{
        QStringLiteral("peakBlobs"), QStringLiteral("activePeakHold"),
        QStringLiteral("noiseFloor"), QStringLiteral("waterfallLevels"),
        QStringLiteral("normalize"), QStringLiteral("calibrationOffsetDb"),
        QStringLiteral("averageTimeMs"), QStringLiteral("waterfallAverageTimeMs")};
    return keys;
}

bool parseDisplayExtrasRequest(const QJsonObject& subscribe, DisplayExtrasRequest& request)
{
    DisplayExtrasRequest parsed;
    QJsonObject object;
    if (subscribe.contains(QStringLiteral("peakBlobs"))) {
        DisplayExtrasRequest::PeakBlobs blobs;
        // The desktop's Setup ranges (SpectrumPeaksPage): 1..20 peaks, a
        // hold of 100..60000 ms, a fall of 1..60 dB/s. Zero is its "off".
        if (!objectWithKeys(subscribe.value(QStringLiteral("peakBlobs")),
                            {"count", "holdMs", "fallDbPerSec", "insideOnly"}, object)
            || !intIn(object.value(QStringLiteral("count")), 1, kDisplayExtrasMaxBlobs,
                      blobs.count)
            || !intIn(object.value(QStringLiteral("holdMs")), 0, 60000, blobs.holdMs)
            || (blobs.holdMs != 0 && blobs.holdMs < 100)
            || !numberIn(object.value(QStringLiteral("fallDbPerSec")), 0.0, 60.0,
                         blobs.fallDbPerSec)
            || (blobs.fallDbPerSec != 0.0 && blobs.fallDbPerSec < 1.0)
            || !boolValue(object.value(QStringLiteral("insideOnly")), blobs.insideOnly)) {
            return false;
        }
        parsed.peakBlobs = blobs;
    }
    if (subscribe.contains(QStringLiteral("activePeakHold"))) {
        DisplayExtrasRequest::ActivePeakHold hold;
        // SpectrumWidget's setter bounds: 100..60000 ms, 0.1..120 dB/s.
        // onTx is optional (displayExtrasVersion 3); absent keeps it true.
        const QJsonValue raw = subscribe.value(QStringLiteral("activePeakHold"));
        const bool withOnTx = raw.isObject()
            && raw.toObject().contains(QStringLiteral("onTx"));
        if (!(withOnTx ? objectWithKeys(raw, {"enabled", "holdMs", "fallDbPerSec", "onTx"}, object)
                       : objectWithKeys(raw, {"enabled", "holdMs", "fallDbPerSec"}, object))
            || (withOnTx && !boolValue(object.value(QStringLiteral("onTx")), hold.onTx))
            || !boolValue(object.value(QStringLiteral("enabled")), hold.enabled)
            || !intIn(object.value(QStringLiteral("holdMs")), 100, 60000, hold.holdMs)
            || !numberIn(object.value(QStringLiteral("fallDbPerSec")), 0.1, 120.0,
                         hold.fallDbPerSec)) {
            return false;
        }
        parsed.activePeakHold = hold;
    }
    if (subscribe.contains(QStringLiteral("noiseFloor"))) {
        DisplayExtrasRequest::NoiseFloor floor;
        // fastAttack is optional (displayExtrasVersion 4); absent is false.
        const QJsonValue raw = subscribe.value(QStringLiteral("noiseFloor"));
        const bool withFastAttack = raw.isObject()
            && raw.toObject().contains(QStringLiteral("fastAttack"));
        if (!(withFastAttack ? objectWithKeys(raw, {"enabled", "shiftDb", "fastAttack"}, object)
                             : objectWithKeys(raw, {"enabled", "shiftDb"}, object))
            || (withFastAttack
                && !boolValue(object.value(QStringLiteral("fastAttack")), floor.fastAttack))
            || !boolValue(object.value(QStringLiteral("enabled")), floor.enabled)
            || !numberIn(object.value(QStringLiteral("shiftDb")),
                         NoiseFloorFollower::kShiftMinDb, NoiseFloorFollower::kShiftMaxDb,
                         floor.shiftDb)) {
            return false;
        }
        parsed.noiseFloor = floor;
    }
    if (subscribe.contains(QStringLiteral("waterfallLevels"))) {
        DisplayExtrasRequest::WaterfallLevels levels;
        if (!objectWithKeys(subscribe.value(QStringLiteral("waterfallLevels")),
                            {"mode", "lowDbm", "highDbm", "offsetDb"}, object)
            || !object.value(QStringLiteral("mode")).isString()
            || !numberIn(object.value(QStringLiteral("lowDbm")), -400.0, 100.0, levels.lowDbm)
            || !numberIn(object.value(QStringLiteral("highDbm")), -400.0, 100.0,
                         levels.highDbm)
            || !intIn(object.value(QStringLiteral("offsetDb")),
                      WaterfallLevelFollower::kNoiseFloorAgcOffsetMinDb,
                      WaterfallLevelFollower::kNoiseFloorAgcOffsetMaxDb, levels.offsetDb)) {
            return false;
        }
        const QString mode = object.value(QStringLiteral("mode")).toString();
        if (mode == QLatin1String("manual")) {
            levels.mode = WaterfallLevelMode::Manual;
        } else if (mode == QLatin1String("agc")) {
            levels.mode = WaterfallLevelMode::Agc;
        } else if (mode == QLatin1String("noiseFloorAgc")) {
            levels.mode = WaterfallLevelMode::NoiseFloorAgc;
        } else if (mode == QLatin1String("clarity")) {
            levels.mode = WaterfallLevelMode::Clarity;
        } else {
            return false;
        }
        parsed.waterfallLevels = levels;
    }
    if (subscribe.contains(QStringLiteral("normalize"))) {
        bool normalize = false;
        if (!boolValue(subscribe.value(QStringLiteral("normalize")), normalize)) {
            return false;
        }
        parsed.normalize = normalize;
    }
    if (subscribe.contains(QStringLiteral("calibrationOffsetDb"))) {
        double offset = 0.0;
        if (!numberIn(subscribe.value(QStringLiteral("calibrationOffsetDb")),
                      kCalibrationOffsetMinDb, kCalibrationOffsetMaxDb, offset)) {
            return false;
        }
        parsed.calibrationOffsetDb = offset;
    }
    if (subscribe.contains(QStringLiteral("averageTimeMs"))) {
        int timeMs = 0;
        if (!intIn(subscribe.value(QStringLiteral("averageTimeMs")), kAverageTimeMinMs,
                   kAverageTimeMaxMs, timeMs)) {
            return false;
        }
        parsed.averageTimeMs = timeMs;
    }
    if (subscribe.contains(QStringLiteral("waterfallAverageTimeMs"))) {
        int timeMs = 0;
        if (!intIn(subscribe.value(QStringLiteral("waterfallAverageTimeMs")),
                   kAverageTimeMinMs, kAverageTimeMaxMs, timeMs)) {
            return false;
        }
        parsed.waterfallAverageTimeMs = timeMs;
    }
    request = parsed;
    return true;
}

// ── The NSDX datagram ───────────────────────────────────────────────────

quint8 DisplayExtrasFrame::sections() const
{
    quint8 sections = 0;
    if (peakBlobs) { sections |= kDisplayExtrasPeakBlobs; }
    if (peakHoldDbm) { sections |= kDisplayExtrasPeakHold; }
    if (noiseFloorDbm) { sections |= kDisplayExtrasNoiseFloor; }
    if (waterfallLevelsDbm) { sections |= kDisplayExtrasWaterfallLevels; }
    if (noiseFloorFastAttack) { sections |= kDisplayExtrasNoiseFloorState; }
    return sections;
}

std::optional<SpectrumDisplayCost> displayCostWithExtras(int pixels, int fps,
                                                         bool includeWidePlane,
                                                         quint8 sections)
{
    std::optional<SpectrumDisplayCost> cost =
        spectrumDisplayCost(pixels, fps, includeWidePlane);
    if (!cost || sections == 0) {
        return cost;
    }
    const quint64 frames = static_cast<quint64>(fps);
    cost->charge.applicationBytesPerSecond +=
        static_cast<quint64>(displayExtrasWorstCaseBytes(sections, pixels)) * frames;
    cost->charge.messagesPerSecond += static_cast<quint32>(fps);
    if ((sections & kDisplayExtrasPeakHold) != 0) {
        cost->charge.spectrumSampleUnitsPerSecond += static_cast<quint64>(pixels) * frames;
    }
    return cost;
}

quint32 displayExtrasWorstCaseBytes(quint8 sections, int traceSamples)
{
    quint32 bytes = kDisplayExtrasHeaderBytes;
    if ((sections & kDisplayExtrasPeakBlobs) != 0) {
        bytes += 1 + 6 * kDisplayExtrasMaxBlobs;
    }
    if ((sections & kDisplayExtrasPeakHold) != 0) {
        bytes += static_cast<quint32>(planeWorstCaseBytes(traceSamples));
    }
    if ((sections & kDisplayExtrasNoiseFloor) != 0) {
        bytes += 4;
    }
    if ((sections & kDisplayExtrasWaterfallLevels) != 0) {
        bytes += 8;
    }
    if ((sections & kDisplayExtrasNoiseFloorState) != 0) {
        bytes += 1;
    }
    return bytes;
}

QByteArray encodeDisplayExtras(const DisplayExtrasFrame& frame,
                               const DisplayCodecContext& context)
{
    const quint8 sections = frame.sections();
    if (sections == 0 || frame.endpointId != context.endpointId
        || frame.contextGeneration != context.contextGeneration) {
        return {};
    }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(
        displayExtrasWorstCaseBytes(sections, context.traceSamples)));
    appendU32(bytes, kMagic);
    appendU8(bytes, kDisplayExtrasVersion);
    appendU8(bytes, sections);
    appendU16(bytes, kDisplayExtrasHeaderBytes);
    appendU32(bytes, frame.endpointId);
    appendU32(bytes, frame.contextGeneration);
    appendU32(bytes, frame.encoderSequence);
    if (frame.peakBlobs) {
        if (frame.peakBlobs->size() > kDisplayExtrasMaxBlobs) { return {}; }
        appendU8(bytes, static_cast<quint8>(frame.peakBlobs->size()));
        for (const DisplayExtrasBlob& blob : *frame.peakBlobs) {
            if (blob.pixel >= context.traceSamples || !finite(blob.dbm)) { return {}; }
            appendU16(bytes, blob.pixel);
            appendF32(bytes, blob.dbm);
        }
    }
    if (frame.peakHoldDbm) {
        if (frame.peakHoldDbm->size() != context.traceSamples) { return {}; }
        // A bin the hold has not reached yet (below every value, -inf) is
        // drawn at the bottom of the interval, where the codec clips it.
        QVector<float> row = *frame.peakHoldDbm;
        for (float& value : row) {
            if (!finite(value)) { value = context.minDbm; }
        }
        const QByteArray plane =
            encodeDisplayCodecAbsolutePlane(row, context.minDbm, context.maxDbm);
        if (plane.isEmpty()) { return {}; }
        bytes.append(plane);
    }
    if (frame.noiseFloorDbm) {
        if (!finite(*frame.noiseFloorDbm)) { return {}; }
        appendF32(bytes, *frame.noiseFloorDbm);
    }
    if (frame.waterfallLevelsDbm) {
        if (!finite(frame.waterfallLevelsDbm->first)
            || !finite(frame.waterfallLevelsDbm->second)) {
            return {};
        }
        appendF32(bytes, frame.waterfallLevelsDbm->first);
        appendF32(bytes, frame.waterfallLevelsDbm->second);
    }
    if (frame.noiseFloorFastAttack) {
        appendU8(bytes, *frame.noiseFloorFastAttack ? kDisplayExtrasNoiseFloorFastAttack : 0);
    }
    if (bytes.size() > kDisplayExtrasMaxBytes) {
        return {};
    }
    return bytes;
}

DisplayExtrasDecodeResult decodeDisplayExtras(const QByteArray& bytes,
                                              const DisplayCodecContext& context)
{
    if (bytes.size() > kDisplayExtrasMaxBytes) {
        return rejected(DisplayExtrasReason::Oversized);
    }
    Reader reader(bytes);
    quint32 magic = 0;
    if (!reader.u32(magic)) { return rejected(DisplayExtrasReason::Truncated); }
    if (magic != kMagic) { return rejected(DisplayExtrasReason::BadMagic); }
    quint8 version = 0;
    quint8 sections = 0;
    quint16 headerBytes = 0;
    DisplayExtrasFrame frame;
    if (!reader.u8(version)) { return rejected(DisplayExtrasReason::Truncated); }
    if (version != kDisplayExtrasVersion) {
        return rejected(DisplayExtrasReason::UnsupportedVersion);
    }
    if (!reader.u8(sections) || !reader.u16(headerBytes) || !reader.u32(frame.endpointId)
        || !reader.u32(frame.contextGeneration) || !reader.u32(frame.encoderSequence)) {
        return rejected(DisplayExtrasReason::Truncated);
    }
    if ((sections & ~kDisplayExtrasKnownSections) != 0) {
        return rejected(DisplayExtrasReason::UnknownSections);
    }
    if (sections == 0 || headerBytes != kDisplayExtrasHeaderBytes) {
        return rejected(DisplayExtrasReason::Malformed);
    }
    if (frame.endpointId != context.endpointId
        || frame.contextGeneration != context.contextGeneration) {
        return rejected(DisplayExtrasReason::ContextMismatch);
    }
    if ((sections & kDisplayExtrasPeakBlobs) != 0) {
        quint8 count = 0;
        if (!reader.u8(count)) { return rejected(DisplayExtrasReason::Truncated); }
        if (count > kDisplayExtrasMaxBlobs) { return rejected(DisplayExtrasReason::Malformed); }
        QVector<DisplayExtrasBlob> blobs;
        blobs.reserve(count);
        for (int i = 0; i < count; ++i) {
            DisplayExtrasBlob blob;
            if (!reader.u16(blob.pixel) || !reader.f32(blob.dbm)) {
                return rejected(DisplayExtrasReason::Truncated);
            }
            if (blob.pixel >= context.traceSamples || !finite(blob.dbm)) {
                return rejected(DisplayExtrasReason::Malformed);
            }
            blobs.append(blob);
        }
        frame.peakBlobs = std::move(blobs);
    }
    if ((sections & kDisplayExtrasPeakHold) != 0) {
        int offset = reader.position();
        QVector<float> row;
        if (!decodeDisplayCodecAbsolutePlane(bytes, offset, context.traceSamples,
                                             context.minDbm, context.maxDbm, row)) {
            return rejected(DisplayExtrasReason::Malformed);
        }
        reader.setPosition(offset);
        frame.peakHoldDbm = std::move(row);
    }
    if ((sections & kDisplayExtrasNoiseFloor) != 0) {
        float floor = 0.0f;
        if (!reader.f32(floor)) { return rejected(DisplayExtrasReason::Truncated); }
        if (!finite(floor)) { return rejected(DisplayExtrasReason::Malformed); }
        frame.noiseFloorDbm = floor;
    }
    if ((sections & kDisplayExtrasWaterfallLevels) != 0) {
        float low = 0.0f;
        float high = 0.0f;
        if (!reader.f32(low) || !reader.f32(high)) {
            return rejected(DisplayExtrasReason::Truncated);
        }
        if (!finite(low) || !finite(high)) { return rejected(DisplayExtrasReason::Malformed); }
        frame.waterfallLevelsDbm = std::make_pair(low, high);
    }
    if ((sections & kDisplayExtrasNoiseFloorState) != 0) {
        quint8 state = 0;
        if (!reader.u8(state)) { return rejected(DisplayExtrasReason::Truncated); }
        if ((state & ~kDisplayExtrasNoiseFloorFastAttack) != 0) {
            return rejected(DisplayExtrasReason::Malformed);
        }
        frame.noiseFloorFastAttack = state != 0;
    }
    if (!reader.atEnd()) {
        return rejected(DisplayExtrasReason::Malformed);
    }
    DisplayExtrasDecodeResult result;
    result.accepted = true;
    result.reason = DisplayExtrasReason::None;
    result.frame = std::move(frame);
    return result;
}

// ── The Core's computation ──────────────────────────────────────────────

DisplayExtrasProcessor::DisplayExtrasProcessor(const DisplayExtrasRequest& request)
    : m_request(request)
{
    if (m_request.peakBlobs) {
        const auto& blobs = *m_request.peakBlobs;
        // The desktop's two switches ride on the numbers: a hold of 0 is
        // hold off, a fall of 0 is a hard cut after the hold.
        m_blobs.setCount(blobs.count);
        m_blobs.setInsideFilterOnly(blobs.insideOnly);
        m_blobs.setHoldEnabled(blobs.holdMs > 0);
        if (blobs.holdMs > 0) { m_blobs.setHoldMs(blobs.holdMs); }
        m_blobs.setHoldDrop(blobs.fallDbPerSec > 0.0);
        if (blobs.fallDbPerSec > 0.0) { m_blobs.setFallDbPerSec(blobs.fallDbPerSec); }
        m_blobs.setEnabled(true);
        // A new display starts with a clearing reset, as the desktop's new
        // context (SpectrumWidget::invalidateRemoteSpectrumFrame) and
        // Thetis's clearBuffers (display.cs:2835-2837, 879-882
        // [v2.10.3.15]): the blobs wait out the 500 ms display delay, as
        // the peak hold does from its first resize.
        m_blobs.clearMaximums();
    }
    if (m_request.activePeakHold) {
        m_peakHold.setDurationMs(m_request.activePeakHold->holdMs);
        m_peakHold.setDropDbPerSec(m_request.activePeakHold->fallDbPerSec);
        m_peakHold.setOnTx(m_request.activePeakHold->onTx);
        m_peakHold.setEnabled(m_request.activePeakHold->enabled);
    }
    if (m_request.waterfallLevels) {
        m_activeLow = static_cast<float>(m_request.waterfallLevels->lowDbm);
        m_activeHigh = static_cast<float>(m_request.waterfallLevels->highDbm);
        if (m_request.waterfallLevels->mode == WaterfallLevelMode::Clarity) {
            m_clarity = std::make_unique<ClarityController>();
            // As SpectrumWidget::setClarityWaterfallThresholds: Clarity
            // writes the levels in force, and composition leaves them.
            QObject::connect(m_clarity.get(), &ClarityController::waterfallThresholdsChanged,
                             m_clarity.get(), [this](float low, float high) {
                                 m_activeLow = low;
                                 m_activeHigh = high;
                             });
            m_clarity->setEnabled(true);
        }
    }
}

DisplayExtrasProcessor::~DisplayExtrasProcessor() = default;

void DisplayExtrasProcessor::newContext()
{
    // SpectrumWidget::invalidateRemoteSpectrumFrame: the peak hold empties
    // and the blobs start again; the noise floor and the levels carry on.
    // Both wait out the 500 ms display delay (Thetis display.cs:859-877
    // [v2.10.3.15]) before they are sent again.
    m_peakHold.resize(0);
    if (m_blobs.enabled()) {
        m_blobs.clearMaximums();
    }
}

void DisplayExtrasProcessor::feedNoiseFloor(float floorDbm, qint64 nowMs)
{
    if (m_clarity) {
        m_clarity->feedNoiseFloor(floorDbm, nowMs);
    }
}

bool DisplayExtrasProcessor::retuneClarity()
{
    if (!m_clarity) {
        return false;
    }
    m_clarity->retuneNow();
    return true;
}

float DisplayExtrasProcessor::displayShiftDb(double binWidthHz, int traceDetector) const
{
    const float calibration = m_request.calibrationOffsetDb
        ? static_cast<float>(*m_request.calibrationOffsetDb) : 0.0f;
    return calibration + normalizeShiftDb(
        m_request.normalize.value_or(false) && normalizeAppliesToDetector(traceDetector),
        binWidthHz);
}

DisplayExtrasFrame DisplayExtrasProcessor::process(const DisplayCodecFrame& frame,
                                                   const DisplayExtrasInputs& inputs)
{
    DisplayExtrasFrame out;
    out.endpointId = frame.context.endpointId;
    out.contextGeneration = frame.context.contextGeneration;
    out.encoderSequence = frame.encoderSequence;

    const int fps = std::max(1, inputs.fps);
    const float shift = displayShiftDb(inputs.binWidthHz, inputs.traceDetector);
    const QVector<float>& trace = frame.traceDbm;

    // The desktop's triggers: fast attack on a band change, a tune of more
    // than half a megahertz or a MOX edge (MainWindow), and on a MOX edge
    // the waterfall follower re-primes and Clarity pauses while keyed.
    if (m_fastAttackTrigger.observe(inputs.sliceHz, inputs.band, inputs.mox)) {
        m_noiseFloor.setFastAttack(true, inputs.nowMs);
    }
    if (inputs.mox != m_mox) {
        m_mox = inputs.mox;
        m_levels.resetAgc();
    }
    if (m_clarity) {
        m_clarity->setTransmitting(inputs.mox);
    }

    // In SpectrumWidget::updateReducedSpectrumOverlays' order: the active
    // peak hold, the blobs, then the noise floor.
    // As SpectrumWidget::setMoxOverlay: the trace's transmit gate is this
    // endpoint's slice transmitting (Thetis display.cs:5011 [v2.10.3.15]:
    //   bSpectralPeakHold = (!local_mox || _activePeakInTxRX1) && m_bSpectralPeakHoldRX1 && ...).
    // Off while transmitting without onTx, and inside the display delay
    // after a reset: no section is sent, and the app draws no trace. The
    // frame still runs, so the delay and the hold keep time.
    m_peakHold.setTxActive(inputs.transmitting);
    if (m_request.activePeakHold && m_request.activePeakHold->enabled && !trace.isEmpty()) {
        if (m_peakHold.size() != trace.size()) {
            m_peakHold.resize(trace.size());
        }
        m_peakHold.update(trace);
        m_peakHold.tickFrame(fps);
        if (m_peakHold.active()) {
            QVector<float> row = m_peakHold.peaks();
            for (float& value : row) { value += shift; }
            out.peakHoldDbm = std::move(row);
        }
    }
    if (m_request.peakBlobs && !trace.isEmpty()) {
        const int n = trace.size();
        int filterLowPx = 0;
        int filterHighPx = n - 1;
        if (m_blobs.insideOnly() && inputs.spanHz > 0.0) {
            std::tie(filterLowPx, filterHighPx) = passbandPixels(
                n, inputs.centreHz, inputs.spanHz,
                inputs.sliceHz + inputs.filterLowHz, inputs.sliceHz + inputs.filterHighHz);
        }
        m_blobs.update(trace, filterLowPx, filterHighPx);
        m_blobs.tickFrame(fps, 1000 / fps);
        QVector<DisplayExtrasBlob> blobs;
        for (const PeakBlob& blob : m_blobs.blobs()) {
            if (!blob.enabled || blob.binIndex < 0 || blob.binIndex >= n
                || blobs.size() >= kDisplayExtrasMaxBlobs) {
                continue;
            }
            blobs.append({static_cast<quint16>(blob.binIndex), blob.max_dBm + shift});
        }
        out.peakBlobs = std::move(blobs);
    }
    if (m_request.noiseFloor && m_request.noiseFloor->enabled) {
        m_noiseFloor.process(trace, fps, inputs.nowMs);
        if (m_request.noiseFloor->fastAttack) {
            out.noiseFloorFastAttack = m_noiseFloor.fastAttack();
        }
        out.noiseFloorDbm = m_noiseFloor.lerpAverage()
            + NoiseFloorFollower::clampShiftDb(static_cast<float>(m_request.noiseFloor->shiftDb))
            + shift;
    }
    if (m_request.waterfallLevels) {
        const auto& levels = *m_request.waterfallLevels;
        // As SpectrumWidget::pushWaterfallRow: a new waterfall line, and
        // nothing while keyed (composeWaterfallActiveThresholds' MOX return).
        if (frame.waterfallAdvance && !inputs.mox) {
            WaterfallLevelSettings settings;
            settings.lowDbm = static_cast<float>(levels.lowDbm);
            settings.highDbm = static_cast<float>(levels.highDbm);
            settings.agc = levels.mode == WaterfallLevelMode::Agc;
            settings.noiseFloorAgc = levels.mode == WaterfallLevelMode::NoiseFloorAgc;
            settings.noiseFloorAgcOffsetDb = levels.offsetDb;
            settings.clarityActive = levels.mode == WaterfallLevelMode::Clarity;
            m_levels.compose(frame.waterfallDbm, settings, m_activeLow, m_activeHigh);
        }
        out.waterfallLevelsDbm = std::make_pair(m_activeLow + shift, m_activeHigh + shift);
    }
    return out;
}

} // namespace NereusSDR
