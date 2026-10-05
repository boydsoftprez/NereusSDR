// =================================================================
// src/core/session/media/DisplayBudget.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original accounting of NereusSDR display codecs.
//
// Modification history (NereusSDR):
//   2026-10-04: Pace byte-only display extras with the shared spectrum
//               budget; retain ordinary spectrum sample validation.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//
// =================================================================

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"

#include <algorithm>
#include <limits>

namespace NereusSDR {
namespace {

constexpr quint64 kNanosecondsPerSecond = 1'000'000'000ULL;

bool addChecked(quint64 left, quint64 right, quint64& result)
{
    if (left > std::numeric_limits<quint64>::max() - right) {
        return false;
    }
    result = left + right;
    return true;
}

bool multiplyChecked(quint64 left, quint64 right, quint64& result)
{
    if (left != 0 && right > std::numeric_limits<quint64>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

bool zeroCharge(const DisplayBudgetCharge& charge)
{
    return charge.applicationBytesPerSecond == 0
        && charge.spectrumSampleUnitsPerSecond == 0
        && charge.messagesPerSecond == 0;
}

bool validSpectrumCharge(const DisplayBudgetCharge& charge)
{
    return charge.applicationBytesPerSecond != 0
        && charge.applicationBytesPerSecond <= kDisplayBudgetJsonSafePositiveLimit
        && charge.spectrumSampleUnitsPerSecond != 0
        && charge.spectrumSampleUnitsPerSecond <= kDisplayBudgetJsonSafePositiveLimit
        && charge.messagesPerSecond != 0
        && charge.messagesPerSecond <= kDisplaySenderMessagesPerSecond;
}

bool spendableSpectrumCost(quint64 bytes, quint64 samples, bool allowZeroSamples = false)
{
    return bytes != 0 && (allowZeroSamples || samples != 0)
        && bytes <= kMaximumSpectrumDisplayFrameBytes
        && samples <= kMaximumSpectrumDisplayFrameSampleUnits;
}

bool spendablePs3Cost(quint64 bytes)
{
    return bytes != 0 && bytes <= kMaximumPs3DisplayChunkBytes;
}

} // namespace

QString displayBudgetReasonWireName(DisplayBudgetReason reason)
{
    switch (reason) {
    case DisplayBudgetReason::CoreBusy:
        return QStringLiteral("coreBusy");
    case DisplayBudgetReason::SharedConnection:
        return QStringLiteral("sharedConnection");
    case DisplayBudgetReason::SharedProcessing:
        return QStringLiteral("sharedProcessing");
    case DisplayBudgetReason::None:
        break;
    }
    return QStringLiteral("none");
}

DisplayBudgetReason displayBudgetReasonForOlderDevice(DisplayBudgetReason reason)
{
    switch (reason) {
    case DisplayBudgetReason::SharedProcessing:
        return DisplayBudgetReason::CoreBusy;
    case DisplayBudgetReason::SharedConnection:
        return DisplayBudgetReason::None;
    case DisplayBudgetReason::None:
    case DisplayBudgetReason::CoreBusy:
        break;
    }
    return reason;
}

std::optional<DisplayBudgetReason> displayBudgetReasonFromWireName(const QString& name)
{
    if (name == QLatin1String("none")) {
        return DisplayBudgetReason::None;
    }
    if (name == QLatin1String("coreBusy")) {
        return DisplayBudgetReason::CoreBusy;
    }
    if (name == QLatin1String("sharedConnection")) {
        return DisplayBudgetReason::SharedConnection;
    }
    if (name == QLatin1String("sharedProcessing")) {
        return DisplayBudgetReason::SharedProcessing;
    }
    return std::nullopt;
}

bool DisplayBudgetLimits::isValid() const
{
    return applicationBytesPerSecond != 0
        && applicationBytesPerSecond <= kDisplayBudgetJsonSafePositiveLimit
        && spectrumSampleUnitsPerSecond != 0
        && spectrumSampleUnitsPerSecond <= kDisplayBudgetJsonSafePositiveLimit
        && generation != 0;
}

std::optional<SpectrumDisplayCost> spectrumDisplayCost(int pixels, int fps,
                                                        bool includeWidePlane)
{
    if (pixels <= 0 || pixels > DisplayCodecEncoder::kMaxSamplesPerPlane
        || fps <= 0 || fps > static_cast<int>(kMaximumSpectrumDisplayFramesPerSecond)) {
        return std::nullopt;
    }

    const quint64 pixelCount = static_cast<quint64>(pixels);
    const quint64 wideSamples = includeWidePlane ? SpectrumEndpoint::kMaxWideSamples : 0;
    const quint64 frameBytes = kDisplayCodecHeaderBytes
        + 2 * displayCodecWorstCasePlaneBytes(pixelCount)
        + displayCodecWorstCasePlaneBytes(wideSamples);
    const quint64 frameSamples = 2 * pixelCount + wideSamples;
    quint64 bytesPerSecond = 0;
    quint64 samplesPerSecond = 0;
    if (frameBytes > std::numeric_limits<quint32>::max()
        || frameSamples > std::numeric_limits<quint32>::max()
        || !multiplyChecked(frameBytes, static_cast<quint64>(fps), bytesPerSecond)
        || !multiplyChecked(frameSamples, static_cast<quint64>(fps), samplesPerSecond)) {
        return std::nullopt;
    }

    SpectrumDisplayCost result;
    result.maximumFrameBytes = static_cast<quint32>(frameBytes);
    result.maximumFrameSampleUnits = static_cast<quint32>(frameSamples);
    result.charge = {bytesPerSecond, samplesPerSecond, static_cast<quint32>(fps)};
    return result;
}

DisplayBudgetCharge ps3DisplayCharge()
{
    const quint64 sampleAndCorrectionCount = static_cast<quint64>(Ps3Snapshot::kMaxSampleCount)
        + static_cast<quint64>(Ps3Snapshot::kMaxCorrectionCount);
    const quint64 payloadBytes = 4 * sampleAndCorrectionCount * sizeof(double);
    const quint64 payloadPerChunk = Ps3DisplayCodec::kMaxChunkBytes
        - Ps3DisplayCodec::kHeaderBytes;
    const quint64 chunksPerFrame = (payloadBytes + payloadPerChunk - 1) / payloadPerChunk;
    const quint64 frameBytes = payloadBytes
        + chunksPerFrame * static_cast<quint64>(Ps3DisplayCodec::kHeaderBytes);
    const quint64 framesPerSecond = kNanosecondsPerSecond
        / static_cast<quint64>(kPs3DisplayPollIntervalNs);
    return {frameBytes * framesPerSecond, 0,
            static_cast<quint32>(chunksPerFrame * framesPerSecond)};
}

std::optional<DisplayBudgetCharge> sumDisplayCharges(
    const QList<DisplayBudgetCharge>& charges)
{
    DisplayBudgetCharge sum;
    for (const DisplayBudgetCharge& charge : charges) {
        quint64 bytes = 0;
        quint64 samples = 0;
        if (!addChecked(sum.applicationBytesPerSecond, charge.applicationBytesPerSecond, bytes)
            || !addChecked(sum.spectrumSampleUnitsPerSecond,
                           charge.spectrumSampleUnitsPerSecond, samples)
            || sum.messagesPerSecond
                > std::numeric_limits<quint32>::max() - charge.messagesPerSecond) {
            return std::nullopt;
        }
        sum.applicationBytesPerSecond = bytes;
        sum.spectrumSampleUnitsPerSecond = samples;
        sum.messagesPerSecond += charge.messagesPerSecond;
    }
    return sum;
}

bool displayChargeFits(const DisplayBudgetLimits& limits, const DisplayBudgetCharge& charge)
{
    return limits.isValid()
        && charge.applicationBytesPerSecond <= limits.applicationBytesPerSecond
        && charge.spectrumSampleUnitsPerSecond <= limits.spectrumSampleUnitsPerSecond
        && charge.messagesPerSecond <= kDisplaySenderMessagesPerSecond;
}

void DisplayBudgetPacer::refill(Bucket& bucket, qint64 elapsedNs)
{
    if (elapsedNs <= 0 || bucket.rate == 0 || bucket.credit >= bucket.capacity) {
        if (bucket.credit >= bucket.capacity) {
            bucket.credit = bucket.capacity;
            bucket.remainder = 0;
        }
        return;
    }

    const quint64 room = bucket.capacity - bucket.credit;
    // The most a bucket can ever need is its fixed burst room: one display
    // message for the global bucket (<= 65,536), one worst-case spectrum
    // frame for the spectrum buckets, and one whole worst-case PureSignal
    // snapshot for PureSignal's bucket (147,648, the largest; see
    // beginSession). Decide saturation before multiplying a valid JSON rate
    // by a qint64 duration.  Below this threshold the exact product is
    // bounded by room * 1e9 (under 1.5e14), so portable quint64 arithmetic
    // cannot overflow.
    const quint64 neededNumerator = room * kNanosecondsPerSecond - bucket.remainder;
    const quint64 timeToFullNs = (neededNumerator + bucket.rate - 1) / bucket.rate;
    if (static_cast<quint64>(elapsedNs) >= timeToFullNs) {
        bucket.credit = bucket.capacity;
        bucket.remainder = 0;
        return;
    }
    const quint64 accrued = bucket.rate * static_cast<quint64>(elapsedNs)
        + bucket.remainder;
    bucket.credit += accrued / kNanosecondsPerSecond;
    bucket.remainder = accrued % kNanosecondsPerSecond;
}

void DisplayBudgetPacer::accrue(qint64 nowNs)
{
    if (!m_active || nowNs <= m_lastNs) {
        return;
    }
    const qint64 elapsedNs = nowNs - m_lastNs;
    refill(m_globalBytes, elapsedNs);
    refill(m_spectrumBytes, elapsedNs);
    refill(m_ps3Bytes, elapsedNs);
    refill(m_iqBytes, elapsedNs);
    refill(m_spectrumSamples, elapsedNs);
    m_lastNs = nowNs;
}

bool DisplayBudgetPacer::beginSession(quint64 epoch, DisplayBudgetLimits limits, qint64 nowNs)
{
    if (epoch == 0 || epoch <= m_lastEpoch || !limits.isValid() || nowNs < 0) {
        return false;
    }

    m_lastEpoch = epoch;
    m_epoch = epoch;
    m_limits = limits;
    m_lastNs = nowNs;
    m_active = true;
    m_spectrumActive = false;
    m_ps3Active = false;
    m_iqActive = false;
    m_spectrumCharge = {};
    m_globalBytes = {kMaximumDisplayMessageBytes, limits.applicationBytesPerSecond, 0,
                     kMaximumDisplayMessageBytes};
    m_spectrumBytes = {kMaximumSpectrumDisplayFrameBytes, 0, 0,
                       kMaximumSpectrumDisplayFrameBytes};
    // R-R3-08/37: PureSignal's burst is one whole snapshot, not one chunk.
    // Its bucket refills at exactly its charge (one worst-case snapshot per
    // 100 ms poll), so with room for one chunk it filled while a chunk
    // waited for the sender and lost that credit: about one snapshot in
    // twenty was overtaken by the next before it went out. One snapshot of
    // room lets each go whole; the charge still bounds the rate.
    const quint64 ps3FrameBytes = ps3DisplayCharge().applicationBytesPerSecond
        * static_cast<quint64>(kPs3DisplayPollIntervalNs) / kNanosecondsPerSecond;
    m_ps3Bytes = {ps3FrameBytes, 0, 0, ps3FrameBytes};
    m_iqBytes = {3 * IMediaTransport::kMaxIqMessageBytes, 0, 0,
                 3 * IMediaTransport::kMaxIqMessageBytes};
    m_spectrumSamples = {kMaximumSpectrumDisplayFrameSampleUnits, 0, 0,
                         kMaximumSpectrumDisplayFrameSampleUnits};
    return true;
}

void DisplayBudgetPacer::endSession()
{
    m_active = false;
    m_spectrumActive = false;
    m_ps3Active = false;
    m_iqActive = false;
}

bool DisplayBudgetPacer::update(DisplayBudgetLimits limits, DisplayBudgetCharge spectrumCharge,
                                bool ps3Enabled, qint64 nowNs, quint64 iqBytesPerSecond)
{
    if (!m_active || !limits.isValid() || nowNs < 0
        || (!zeroCharge(spectrumCharge) && !validSpectrumCharge(spectrumCharge))) {
        return false;
    }
    const DisplayBudgetCharge ps3Charge = ps3DisplayCharge();
    const auto combined = sumDisplayCharges(
        {spectrumCharge, ps3Enabled ? ps3Charge : DisplayBudgetCharge{}});
    if (!combined || combined->messagesPerSecond > kDisplaySenderMessagesPerSecond) {
        return false;
    }

    // Credit through this instant belongs to the prior accepted rates. Changing
    // a subscription or descriptor can never create a new burst.
    accrue(nowNs);
    m_limits = limits;
    m_spectrumCharge = spectrumCharge;
    m_spectrumActive = !zeroCharge(spectrumCharge);
    m_ps3Active = ps3Enabled;
    m_iqActive = iqBytesPerSecond != 0;
    m_globalBytes.rate = limits.applicationBytesPerSecond;
    // R-R3-37 (final review, the ceiling run): spectrum is paced to the
    // budget, less what PureSignal's display holds, not to the charge the
    // endpoints were admitted at. Pacing to the admitted charge with a
    // one-frame burst refilled exactly one frame per 5 ms sender tick at the
    // computed ceiling, so a tick that came early lost its frame and eight
    // wide pans received about three quarters of what legacy mode sends.
    // The budget still holds: every class stays within the limits, and the
    // endpoints' own cadence keeps each to its admitted frame rate.
    const quint64 ps3Reserve = m_ps3Active ? ps3Charge.applicationBytesPerSecond : 0;
    const quint64 reserved = ps3Reserve + iqBytesPerSecond;
    const quint64 spectrumByteRoom = limits.applicationBytesPerSecond > reserved
        ? limits.applicationBytesPerSecond - reserved : 0;
    m_spectrumBytes.rate = m_spectrumActive
        ? std::max(spectrumCharge.applicationBytesPerSecond, spectrumByteRoom) : 0;
    m_spectrumSamples.rate = m_spectrumActive ? limits.spectrumSampleUnitsPerSecond : 0;
    m_ps3Bytes.rate = m_ps3Active ? ps3Charge.applicationBytesPerSecond : 0;
    m_iqBytes.rate = iqBytesPerSecond;
    return true;
}

bool DisplayBudgetPacer::canSpendIq(quint64 bytes, qint64 nowNs)
{
    if (!m_active || !m_iqActive || nowNs < 0 || bytes == 0
        || bytes > IMediaTransport::kMaxIqMessageBytes) { return false; }
    accrue(nowNs);
    return bytes <= m_globalBytes.credit && bytes <= m_iqBytes.credit;
}

bool DisplayBudgetPacer::spendIq(quint64 bytes, qint64 nowNs)
{
    if (!canSpendIq(bytes, nowNs)) { return false; }
    m_globalBytes.credit -= bytes;
    m_iqBytes.credit -= bytes;
    return true;
}

bool DisplayBudgetPacer::canSpendSpectrumAfterAccrual(quint64 bytes, quint64 samples) const
{
    return m_spectrumActive && m_globalBytes.rate != 0 && m_spectrumBytes.rate != 0
        && m_spectrumSamples.rate != 0 && bytes <= m_globalBytes.credit
        && bytes <= m_spectrumBytes.credit && samples <= m_spectrumSamples.credit;
}

bool DisplayBudgetPacer::canSpendPs3AfterAccrual(quint64 bytes) const
{
    return m_ps3Active && m_globalBytes.rate != 0 && m_ps3Bytes.rate != 0
        && bytes <= m_globalBytes.credit && bytes <= m_ps3Bytes.credit;
}

bool DisplayBudgetPacer::canSpendSpectrum(quint64 bytes, quint64 samples, qint64 nowNs)
{
    if (!m_active || nowNs < 0 || !spendableSpectrumCost(bytes, samples)) {
        return false;
    }
    accrue(nowNs);
    return canSpendSpectrumAfterAccrual(bytes, samples);
}

bool DisplayBudgetPacer::spendSpectrum(quint64 bytes, quint64 samples, qint64 nowNs)
{
    if (!m_active || nowNs < 0 || !spendableSpectrumCost(bytes, samples)) {
        return false;
    }
    accrue(nowNs);
    return spendSpectrumAfterAccrual(bytes, samples);
}

bool DisplayBudgetPacer::spendDisplayExtras(quint64 bytes, quint64 samples, qint64 nowNs)
{
    if (!m_active || nowNs < 0 || !spendableSpectrumCost(bytes, samples, true)) {
        return false;
    }
    accrue(nowNs);
    return spendSpectrumAfterAccrual(bytes, samples);
}

bool DisplayBudgetPacer::spendSpectrumAfterAccrual(quint64 bytes, quint64 samples)
{
    if (!canSpendSpectrumAfterAccrual(bytes, samples)) {
        return false;
    }
    m_globalBytes.credit -= bytes;
    m_spectrumBytes.credit -= bytes;
    m_spectrumSamples.credit -= samples;
    return true;
}

bool DisplayBudgetPacer::canSpendPs3(quint64 bytes, qint64 nowNs)
{
    if (!m_active || nowNs < 0 || !spendablePs3Cost(bytes)) {
        return false;
    }
    accrue(nowNs);
    return canSpendPs3AfterAccrual(bytes);
}

bool DisplayBudgetPacer::spendPs3(quint64 bytes, qint64 nowNs)
{
    if (!m_active || nowNs < 0 || !spendablePs3Cost(bytes)) {
        return false;
    }
    accrue(nowNs);
    if (!canSpendPs3AfterAccrual(bytes)) {
        return false;
    }
    m_globalBytes.credit -= bytes;
    m_ps3Bytes.credit -= bytes;
    return true;
}

} // namespace NereusSDR
