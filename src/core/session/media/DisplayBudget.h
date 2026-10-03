// =================================================================
// src/core/session/media/DisplayBudget.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original accounting of NereusSDR display codecs.
//
// The constants below are derived from the bounded NSDC and PS3D wire formats;
// they are not a claim about a radio, link, or host's sustainable capacity.
//
// =================================================================

#pragma once

#include "core/session/Ps3DisplayCodec.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/SpectrumEndpoint.h"

#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

/// Largest positive integer that JSON's IEEE-754 number representation carries
/// exactly. Budget descriptors travel as JSON Int64-compatible values.
inline constexpr quint64 kDisplayBudgetJsonSafePositiveLimit = 9'007'199'254'740'991ULL;

/// Existing daemon media scheduler cadence (DaemonMediaController.cpp).
inline constexpr quint32 kDisplaySenderIntervalMs = 5;
inline constexpr qint64 kDisplaySenderIntervalNs = qint64{kDisplaySenderIntervalMs} * 1'000'000;
inline constexpr quint32 kDisplaySenderMessagesPerSecond =
    1'000 / kDisplaySenderIntervalMs;

/// How long the app waits for the Core to acknowledge a display allocation
/// before it calls the allocation stalled (RemoteMediaController.cpp). The
/// Core's display load governor lets a budget step settle at least this long
/// before judging it.
inline constexpr int kDisplayAllocationAckTimeoutMs = 10'000;

/// The reason a display subscription is refused because it does not fit
/// the device's display budget share (DaemonMediaController). Fix wave 2
/// (Critical 1, ruling 9.3): the desktop's planner reads it as the answer to
/// asking for what the operator wants, and plans inside the share the Core
/// published with it. Clients compare it exactly, so it never changes.
inline constexpr char kDisplayBudgetRefusalReason[] = "The Core's display limit has no room left.";

/// Current NSDC schema-1 header and daemon subscribe validation maximum
/// (DisplayCodec.cpp and DaemonMediaController.cpp respectively).
inline constexpr quint32 kDisplayCodecHeaderBytes = DisplayCodecEncoder::kHeaderBytes;
inline constexpr quint32 kMaximumSpectrumDisplayFramesPerSecond = 60;

/// Existing remote PureSignal display producer cadence
/// (PureSignalSessionFacade.cpp).
inline constexpr quint32 kPs3DisplayPollIntervalMs = 100;
inline constexpr qint64 kPs3DisplayPollIntervalNs =
    qint64{kPs3DisplayPollIntervalMs} * 1'000'000;

constexpr quint32 displayCodecWorstCasePlaneBytes(quint32 samples)
{
    return samples == 0 ? 0
        : 3 + 5 * ((samples + 127) / 128) + samples;
}

/// Current bounded NSDC frame, using the actual endpoint/codec public maxima.
inline constexpr quint32 kMaximumSpectrumDisplayFrameBytes = kDisplayCodecHeaderBytes
    + 2 * displayCodecWorstCasePlaneBytes(DisplayCodecEncoder::kMaxSamplesPerPlane)
    + displayCodecWorstCasePlaneBytes(SpectrumEndpoint::kMaxWideSamples);
inline constexpr quint32 kMaximumSpectrumDisplayFrameSampleUnits =
    2 * DisplayCodecEncoder::kMaxSamplesPerPlane + SpectrumEndpoint::kMaxWideSamples;

/// PS3D's public message limit also fixes the common global-byte burst.
inline constexpr quint32 kMaximumPs3DisplayChunkBytes = Ps3DisplayCodec::kMaxChunkBytes;
inline constexpr quint32 kMaximumDisplayMessageBytes = kMaximumPs3DisplayChunkBytes;

struct DisplayBudgetLimits {
    quint64 applicationBytesPerSecond = 0;
    quint64 spectrumSampleUnitsPerSecond = 0;
    quint32 generation = 1;
    bool isValid() const;
    bool operator==(const DisplayBudgetLimits&) const = default;
};

/// Why the Core's current display budget is below its ceiling (R-R3-08,
/// R-R3-37). CoreBusy: the Core computer is short of processing time and
/// lowered spectrum quality so audio and receive processing keep priority.
/// Carried in the capability descriptor's displayBudgetReason entry, only to
/// peers that negotiated kDisplayBudgetReasonSessionProtocolMinor.
///
/// iPhone app Task 76 (the several-devices design, ruling 9.3 and design
/// ruling 9.3a): while another device is admitted and a device's share is
/// below its request, one of two values takes the place of CoreBusy, and
/// only a device that declared sessionHolder receives them:
/// SharedConnection: the devices share what the Core sends and the load
/// governor has cut nothing (the total is the configured display allowance
/// or the Core's computed ceiling). SharedProcessing: the governor has cut
/// the total because the Core computer is short of processing time.
enum class DisplayBudgetReason : quint8 {
    None = 0,
    CoreBusy = 1,
    SharedConnection = 2,
    SharedProcessing = 3,
};

/// "none", "coreBusy", "sharedConnection" or "sharedProcessing".
QString displayBudgetReasonWireName(DisplayBudgetReason reason);
/// The reason a wire name names, or nullopt for a name this build does not know.
std::optional<DisplayBudgetReason> displayBudgetReasonFromWireName(const QString& name);
/// What a device that did not declare sessionHolder is told instead of a
/// shared reason (Task 76): SharedProcessing is the governor's cut, so
/// CoreBusy; SharedConnection cuts nothing of the Core's own, so None. The
/// other two values are returned unchanged.
DisplayBudgetReason displayBudgetReasonForOlderDevice(DisplayBudgetReason reason);

struct DisplayBudgetCharge {
    quint64 applicationBytesPerSecond = 0;
    quint64 spectrumSampleUnitsPerSecond = 0;
    quint32 messagesPerSecond = 0;
    bool operator==(const DisplayBudgetCharge&) const = default;
};

struct SpectrumDisplayCost {
    DisplayBudgetCharge charge;
    quint32 maximumFrameBytes = 0;
    quint32 maximumFrameSampleUnits = 0;
};

std::optional<SpectrumDisplayCost> spectrumDisplayCost(int pixels, int fps,
                                                        bool includeWidePlane);
DisplayBudgetCharge ps3DisplayCharge();
std::optional<DisplayBudgetCharge> sumDisplayCharges(
    const QList<DisplayBudgetCharge>& charges);
bool displayChargeFits(const DisplayBudgetLimits&, const DisplayBudgetCharge&);

/// Session-scoped, fixed-burst token accounting for actual display sends.
/// It deliberately has no transport outcome: callers debit immediately before
/// a new nonempty message can enter the transport, because a failed transport
/// can already own bytes. A Busy retry of the same pending I/Q message keeps
/// its first debit until that message is accepted or the stream ends.
class DisplayBudgetPacer {
public:
    bool beginSession(quint64 epoch, DisplayBudgetLimits limits, qint64 nowNs);
    void endSession();
    bool update(DisplayBudgetLimits limits, DisplayBudgetCharge spectrumCharge,
                bool ps3Enabled, qint64 nowNs, quint64 iqBytesPerSecond = 0);

    bool canSpendSpectrum(quint64 bytes, quint64 samples, qint64 nowNs);
    bool spendSpectrum(quint64 bytes, quint64 samples, qint64 nowNs);
    bool canSpendPs3(quint64 bytes, qint64 nowNs);
    bool spendPs3(quint64 bytes, qint64 nowNs);
    bool canSpendIq(quint64 bytes, qint64 nowNs);
    bool spendIq(quint64 bytes, qint64 nowNs);

private:
    struct Bucket {
        quint64 credit = 0;
        quint64 rate = 0;
        quint64 remainder = 0;
        quint64 capacity = 0;
    };

    static void refill(Bucket& bucket, qint64 elapsedNs);
    void accrue(qint64 nowNs);
    bool canSpendSpectrumAfterAccrual(quint64 bytes, quint64 samples) const;
    bool canSpendPs3AfterAccrual(quint64 bytes) const;

    quint64 m_lastEpoch = 0;
    quint64 m_epoch = 0;
    qint64 m_lastNs = 0;
    bool m_active = false;
    bool m_spectrumActive = false;
    bool m_ps3Active = false;
    bool m_iqActive = false;
    DisplayBudgetLimits m_limits;
    DisplayBudgetCharge m_spectrumCharge;
    Bucket m_globalBytes;
    Bucket m_spectrumBytes;
    Bucket m_ps3Bytes;
    Bucket m_iqBytes;
    Bucket m_spectrumSamples;
};

} // namespace NereusSDR
