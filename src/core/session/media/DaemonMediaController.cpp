// =================================================================
// src/core/session/media/DaemonMediaController.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonMediaController.h.
//
// Modification history (NereusSDR):
//   2026-10-01: Control logging lane: the media connection's selected pair
//               (candidate types and transports, masked addresses) when
//               first known and on every change, and its rtt in that line
//               and in the periodic display diagnostics line. Logging
//               only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-01: TX diagnostics lane, review round: the unkey tail's start,
//               the TX pump's longest wait for a microphone block with the
//               radio's frame sequence step across it, and "RF start not
//               measured" where the send path places nothing. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the unkey line splits the padded
//               silence into the key's start, mid-key and the unkey tail,
//               with the first I/Q block's time and the longest mid-key
//               silence; one line follows for each placed microphone
//               underrun, the first radio ran dry and each catch-up burst.
//               Measurement and logging only. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX stall lane: the unkey line names the device by its id in
//               hex, as the transmit watchdog's line does, not its raw
//               bytes. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: TX stall lane: the unkey line follows MoxController (its
//               moxChanging and moxStateChanged), so it prints on every
//               unkey; TransmitModel::moxChanged only saw the
//               no-controller fallback. The microphone line's packets
//               carry their wait at the Core (heldUs) to the receiver.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX stall lane, fix round 1: only the controller whose
//               device holds the key on its line reports the unkey; the
//               line adds the over's owner waits (mean, max, count over
//               50 ms); a report pending when the controller goes is
//               logged; "Microphone line open for" logs the id as hex.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): the microphone line's
//               packets go through MicRoute, from the transport's own
//               thread when it has one (MediaPeer::setMicPacketSink), so a
//               stall of the Core's event loop never holds them; "tx"
//               keepalives carry their receipt to the watchdog. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the unkey line's microphone
//               waits are the line's ("line waits"), and it adds the
//               over's longest wait of a "tx" keepalive at the Core (the
//               event loop's stall). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: the direct media ladder: a replace may carry
//               "mediaDirectVersion": 1 (STUN and host candidates, no
//               tunnel or relay); the older relay-leg refusal judges the
//               connection in use. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-28: R-IOS-18: the display shift applies normalise only with
//               the endpoint's Average, Sample or RMS trace detector.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone line.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data
//               channel's keepalives to the transmit watchdog, and the
//               line's starvation to the Core's per-mode action. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C2: each controller opens and
//               closes only its own device's line and writes the feed
//               only while it is the writer; the hub routes the keying's
//               view of the lines by device; VOX another device armed is
//               never this one's. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Parity Task 28 (R-R3-49, A11, R-IOS-13): the transmit
//               display for a media peer that declared txDisplayVersion:
//               while keyed, each endpoint on the transmitting pan is a
//               viewer of RadioModel's TxDisplayFeed and is sent its
//               context (transmit true) and frames instead of the
//               receiver's; the fall resumes receive. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the media connection of
//               a session through the remote access service uses its ICE
//               settings. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: at each unkey, one info line with the microphone line's
//               statistics and the transmit I/Q send path's counters
//               (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: Parity Task 31 (A11, R-R3-49): display duplex. An endpoint
//               whose subscribe carries `duplex` true (a peer that declared
//               txDisplayVersion 3) is no viewer while keyed and keeps its
//               receive frames, its contexts `transmit` false; its device's
//               DUP reaches RadioModel, which turns noise blanking off
//               while keyed as Thetis does. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-27: Parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor to
//               the device that holds transmit. handleMonitorAudio answers
//               monitor-audio with monitor-audio-context; refreshTxMonitor
//               puts MON in this device's owner mix (its main stream, or its
//               headphones stream, which then runs) while the device holds
//               transmit and MON is on. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: R-IOS-13, R-R3-42: the unkey line adds the microphone
//               path's added latency (mean, max), the send ring's fill
//               (mean, max) and the silence shed. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): media `replace`: a
//               second peer, audio sent on both, handed over on a keyframe.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: each stream's frames take the
//               receive offset of the ADC that stream is on
//               (RadioModel::rxMeterOffsetDbForStream). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2:
//               ownsSlice split into controlsSlice (owner mix, raw I/Q,
//               headphones), hearsSlice (receiver streams) and seesSlice
//               (display subscriptions). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4: the owner
//               mix and displays follow a leave (listenersChanged) as they
//               follow a change of controller. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 6: the owner mix sums the slices
//               this device listens to at its own level and mute
//               (SliceAccessController::listenLevel), centered.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: radio codec review: the periodic audio diagnostics line
//               carries the radio speaker out's counters
//               (RadioConnection::radioAudioStatsText). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// Modification history (NereusSDR):
// 2026-09-27: Use the approved shared decimation bounds.
// J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.

#include "core/ControlRanges.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/RemoteIqCodec.h"
#include "core/session/media/RemoteSpectrumContext.h"

#include "core/FFTEngine.h"
#include "core/MoxController.h"
#include "core/TxDisplayFeed.h"
#include "core/TxSliceArbiter.h"
#include "core/WdspEngine.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/ControlLog.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RemoteKeying.h"
#include "core/session/SliceAccessController.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonAudioSender.h"
#include "core/session/media/MediaPeer.h"
#include "core/AudioEngine.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/Ps3DisplayCodec.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QtEndian>
#include <QUuid>
#include <QVarLengthArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace NereusSDR {
Q_LOGGING_CATEGORY(lcDaemonMedia, "nereus.daemon.media")
namespace {

constexpr int kMaxEndpoints = 8;
constexpr int kRecentAllocationRecords = 64;
constexpr int kMaxKeyframesPerSecond = 5;
constexpr qint64 kNoiseFloorMinimumIntervalNs = 500'000'000;
constexpr float kNoiseFloorMinimumDbm = -400.0f;
constexpr float kNoiseFloorMaximumDbm = 100.0f;
constexpr float kFftDbmFloor = -200.0f;
constexpr float kFftPowerFloor = 1.0e-20f;
constexpr qint64 kAudioDiagnosticsLogIntervalMs = 2'000;
constexpr int kDisplayDiagnosticsLogIntervalMs = 10'000;
// Control logging lane: how often a "tx" keepalive may look at the media
// connection's selected pair.
constexpr qint64 kMediaPathCheckMs = 1000;
// Distinct media error texts logged per media peer; later display-channel
// errors are still counted in displayTransportErrors.
constexpr qsizetype kMaxLoggedTransportErrorKinds = 16;

// iPhone app plan Task 29: `packet` with its SSRC (bytes 8 to 11) replaced
// through `rewrite`; untouched when its SSRC is not there or it is too short
// to be RTP.
QByteArray rewriteRtpSsrc(const QByteArray& packet, const QHash<quint32, quint32>& rewrite)
{
    if (rewrite.isEmpty() || packet.size() < 12) {
        return packet;
    }
    const quint32 ssrc = qFromBigEndian<quint32>(packet.constData() + 8);
    const auto it = rewrite.constFind(ssrc);
    if (it == rewrite.cend()) {
        return packet;
    }
    QByteArray out = packet;
    qToBigEndian<quint32>(it.value(), out.data() + 8);
    return out;
}

bool exactKeys(const QJsonObject& object, std::initializer_list<const char*> keys)
{
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

bool canonicalConnectionId(const QJsonValue& value)
{
    if (!value.isString()) {
        return false;
    }
    const QString id = value.toString();
    const QUuid uuid = QUuid::fromString(id);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id;
}

bool finiteNumber(const QJsonValue& value, double& result)
{
    if (!value.isDouble()) {
        return false;
    }
    result = value.toDouble();
    return std::isfinite(result);
}

bool exactUnsigned(const QJsonValue& value, quint32& result, bool nonzero = false)
{
    double number = 0.0;
    if (!finiteNumber(value, number) || number < 0.0
        || number > static_cast<double>(std::numeric_limits<quint32>::max())
        || std::floor(number) != number) {
        return false;
    }
    result = static_cast<quint32>(number);
    return !nonzero || result != 0;
}

bool exactInt(const QJsonValue& value, int minimum, int maximum, int& result)
{
    double number = 0.0;
    if (!finiteNumber(value, number) || number < minimum || number > maximum
        || std::floor(number) != number) {
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool parseTier(const QJsonValue& value, FftTier& tier)
{
    if (!value.isString()) {
        return false;
    }
    if (value.toString() == QLatin1String("wide")) {
        tier = FftTier::Wide;
        return true;
    }
    if (value.toString() == QLatin1String("fine")) {
        tier = FftTier::Fine;
        return true;
    }
    return false;
}

bool parseDetector(const QJsonValue& value, SpectrumDetectorMode& detector)
{
    int integer = 0;
    if (!exactInt(value, static_cast<int>(SpectrumDetectorMode::Peak),
                  static_cast<int>(SpectrumDetectorMode::RMS), integer)) {
        return false;
    }
    detector = static_cast<SpectrumDetectorMode>(integer);
    return true;
}

bool parsePlane(const QJsonValue& value, SpectrumPlaneRequest& plane)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!exactKeys(object, {"detector", "averageMode", "averageAlpha"})
        || !parseDetector(object.value(QStringLiteral("detector")), plane.detector)
        || !exactInt(object.value(QStringLiteral("averageMode")), -1, 3, plane.averageMode)
        || !finiteNumber(object.value(QStringLiteral("averageAlpha")), plane.averageAlpha)
        || plane.averageAlpha < 0.0 || plane.averageAlpha > 1.0) {
        return false;
    }
    return true;
}

bool sameSourceConfig(const DaemonSpectrumSourceConfig& left,
                      const DaemonSpectrumSourceConfig& right)
{
    return left.fft.fps == right.fft.fps
        && left.transformsFollowFrameRate == right.transformsFollowFrameRate
        && left.fft.fftSize == right.fft.fftSize
        && left.fft.windowType == right.fft.windowType
        && left.fft.hzPerBinTarget == right.fft.hzPerBinTarget
        && left.decimation == right.decimation
        && left.centreHz == right.centreHz
        && left.sampleRateHz == right.sampleRateHz
        && left.maxPendingIqFloats == right.maxPendingIqFloats;
}

bool supportedFftSize(int size)
{
    FFTEngine validator(-1);
    validator.setFftSize(size);
    return validator.fftSize() == size;
}

bool powerOfTwoFftSize(int size)
{
    return size >= 1024 && (size & (size - 1)) == 0;
}

// Names the root cause of a reduced grant. An FFT limit outranks the pixel
// rule because a smaller engine is what leaves fewer visible bins.
SpectrumLimitReason grantReason(const SpectrumGrant& grant)
{
    const int largest = FFTEngine::maximumFftSize();
    const int clamped = std::min(grant.requestedFftSize, largest);
    // Parity Task 17 follow-up (R-R3-01): a pan beside another runs at the
    // engine's decimation; told as the shared engine it is.
    if (grant.grantedFftSize < clamped
        || grant.grantedDecimation != grant.requestedDecimation) {
        return SpectrumLimitReason::SharedEngine;
    }
    if (grant.requestedFftSize > largest && grant.grantedFftSize < grant.requestedFftSize) {
        return SpectrumLimitReason::LargestSize;
    }
    if (grant.grantedPixels < grant.requestedPixels) {
        return SpectrumLimitReason::SourceBins;
    }
    return SpectrumLimitReason::None;
}

bool staleOrEqualRevision(quint32 candidate, quint32 accepted)
{
    const quint32 difference = candidate - accepted;
    return difference == 0 || difference >= 0x80000000U;
}

bool validRequestedFrequencyRange(double centreHz, double spanHz, double wideSpanFactor)
{
    if (!std::isfinite(centreHz) || !std::isfinite(spanHz) || spanHz <= 0.0
        || !std::isfinite(wideSpanFactor)
        || (wideSpanFactor != 0.0 && wideSpanFactor <= 1.0)) {
        return false;
    }
    const double halfSpan = spanHz * 0.5;
    return std::isfinite(halfSpan) && std::isfinite(centreHz - halfSpan)
        && std::isfinite(centreHz + halfSpan)
        && (wideSpanFactor == 0.0 || std::isfinite(wideSpanFactor * spanHz));
}

bool requestOverlapsSource(const SpectrumEndpointRequest& request,
                           double sourceCentreHz, double sourceSampleRateHz)
{
    if (!std::isfinite(sourceCentreHz) || !std::isfinite(sourceSampleRateHz)
        || sourceSampleRateHz <= 0.0) {
        return false;
    }
    const double sourceHalfRate = sourceSampleRateHz * 0.5;
    const double requestHalfSpan = request.spanHz * 0.5;
    if (!std::isfinite(sourceHalfRate) || !std::isfinite(requestHalfSpan)) {
        return false;
    }
    const double sourceLow = sourceCentreHz - sourceHalfRate;
    const double sourceHigh = sourceCentreHz + sourceHalfRate;
    const double requestLow = request.centreHz - requestHalfSpan;
    const double requestHigh = request.centreHz + requestHalfSpan;
    return std::isfinite(sourceLow) && std::isfinite(sourceHigh)
        && std::isfinite(requestLow) && std::isfinite(requestHigh)
        && requestHigh > sourceLow && requestLow < sourceHigh;
}

bool needsWideband(const SpectrumEndpointRequest& request, double centreHz, double rateHz)
{
    return request.spanHz > rateHz
        || request.centreHz - request.spanHz * 0.5 < centreHz - rateHz * 0.5
        || request.centreHz + request.spanHz * 0.5 > centreHz + rateHz * 0.5;
}

// A replacement endpoint can temporarily coexist with the old one for
// rollback. Unique ownership releases exactly once when either entry retires.
// iPhone app Task 20 (R-IOS-27): a subscription that asks for display
// extras is charged for them too: one more message per frame, at most
// displayExtrasWorstCaseBytes, and the peak hold row's samples.
std::optional<SpectrumDisplayCost> endpointDisplayCost(int pixels, int fps,
                                                       bool includeWidePlane,
                                                       quint8 extrasSections)
{
    // Shared with a window's planner (displayCostWithExtras).
    return displayCostWithExtras(pixels, fps, includeWidePlane, extrasSections);
}

// Parity Task 28: an endpoint's place among the transmit display's viewers,
// given back however the endpoint goes.
struct TxViewerLease {
    QPointer<TxDisplayFeed> feed;
    int id{0};
    ~TxViewerLease() { if (feed) { feed->removeViewer(id); } }
};

// Parity Task 28: the analyzer's pixels brought to the endpoint's sample
// count, each sample the highest of the pixels it covers (the peak, as the
// receive trace's detector keeps a signal's peak). Empty when the analyzer
// has fewer pixels than the endpoint's samples (a poll from before a
// resize).
QVector<float> reduceTransmitPlane(const QVector<float>& dbm, int samples, float floorDbm)
{
    const int count = static_cast<int>(dbm.size());
    if (samples <= 0 || count < samples) {
        return {};
    }
    QVector<float> out(samples, floorDbm);
    for (int i = 0; i < samples; ++i) {
        const int begin = static_cast<int>(static_cast<qint64>(i) * count / samples);
        const int end = std::max(begin + 1,
                                 static_cast<int>(static_cast<qint64>(i + 1) * count / samples));
        float peak = -std::numeric_limits<float>::infinity();
        for (int j = begin; j < end && j < count; ++j) {
            if (std::isfinite(dbm.at(j))) {
                peak = std::max(peak, dbm.at(j));
            }
        }
        out[i] = std::isfinite(peak) ? peak : floorDbm;
    }
    return out;
}

struct WidebandDemandLease {
    QPointer<RadioModel> model;
    RadioModel::WidebandDemandToken token{0};
    ~WidebandDemandLease() { if (model) { model->releaseWidebandDemand(token); } }
};
} // namespace

struct DaemonMediaController::EndpointEntry {
    quint32 revision{0};
    int sliceId{-1};
    int sourceFftSize{0};
    int sourceWindowType{0};
    /// Parity Task 17: what the subscribe asked (`decimation`, 1 without it)
    /// and what the endpoint's engine runs at (its own ask while it is the
    /// engine's only subscriber, the engine's decimation beside another).
    int requestedDecimation{1};
    int sourceDecimation{1};
    double sourceCentreHz{0.0};
    double sourceSampleRateHz{0.0};
    SpectrumEndpointRequest request;
    SpectrumDisplayCost displayCost;
    /// False when the GUI was told of a charge for fewer pixels than it
    /// requested (minor 9 under the display budget); a later re-grant then
    /// keeps within that charge.
    bool chargeCoversRequest{true};
    SpectrumGrant grant;
    AllocationRecord allocation;
    bool widebandNegotiated{false};
    bool widebandWanted{false};
    std::unique_ptr<WidebandDemandLease> widebandDemand;
    SpectrumEndpoint endpoint;
    DisplayCodecEncoder encoder;
    std::optional<DaemonSpectrumFrame> latestInput;
    double stationOffsetDb{0.0};
    bool contextSent{false};
    bool forceKeyframe{true};
    int keyframesInWindow{0};
    QElapsedTimer keyframeWindow;
    qint64 lastNoiseFloorTimestampNs{-1};
    // iPhone app Task 20 (R-IOS-27): what the subscription asked the Core
    // to compute, the computation, and the NSDX datagram waiting to go
    // right after the frame it describes.
    DisplayExtrasRequest extrasRequest;
    std::unique_ptr<DisplayExtrasProcessor> extras;
    QByteArray pendingExtras;
    quint64 pendingExtrasSamples{0};
    // Parity Task 28 (R-R3-49, A11): the transmit display. The window the
    // subscribe asked for it (txMinDbm, txMaxDbm), the endpoint's place
    // among the feed's viewers while it shows it, the context it was last
    // sent (without its generation, to tell when a new one is due), and the
    // analyzer's newest planes waiting to go.
    std::optional<std::pair<float, float>> txWindow;
    // Parity Task 31: the subscribe's `duplex` (txDisplayVersion 3): while
    // keyed this endpoint keeps the receiver.
    bool duplex{false};
    bool miniDisplay{false};
    std::unique_ptr<TxViewerLease> txViewer;
    QJsonObject txSentShape;
    bool txContextSent{false};
    DisplayCodecContext txCodec;
    int txFps{0};
    quint32 txSequence{0};
    qint64 txNextDueNs{0};
    QVector<float> txTrace;
    QVector<float> txWaterfall;
    bool txNewWaterfall{false};
    bool txPending{false};
    qint64 txProducedAtNs{0};
};

// ── Shared spectrum engines (iPhone app Task 76, ruling 9.1) ─────────────

DaemonSharedSpectrum::DaemonSharedSpectrum(RadioModel* radioModel, QObject* parent)
    : QObject(parent)
    , m_source(this)
{
    m_source.setRadioModel(radioModel);
    connect(&m_source, &DaemonSpectrumSource::frameAvailable,
            this, &DaemonSharedSpectrum::onFrameAvailable);
}

DaemonSharedSpectrum::~DaemonSharedSpectrum()
{
    const QList<MediaSourceKey> keys = m_runtimes.keys();
    for (const MediaSourceKey& key : keys) {
        m_source.deactivate(key);
    }
    m_runtimes.clear();
}

QList<DaemonMediaController*> DaemonSharedSpectrum::members() const
{
    QList<DaemonMediaController*> out;
    for (const QPointer<DaemonMediaController>& member : m_members) {
        if (member) {
            out.append(member.data());
        }
    }
    return out;
}

void DaemonSharedSpectrum::join(DaemonMediaController* controller)
{
    if (controller != nullptr && !m_members.contains(controller)) {
        m_members.append(controller);
    }
}

void DaemonSharedSpectrum::leave(DaemonMediaController* controller)
{
    m_members.removeAll(controller);
    m_members.removeAll(QPointer<DaemonMediaController>());
}

void DaemonSharedSpectrum::onFrameAvailable(MediaSourceKey key)
{
    // Taken once; every controller watching this receiver gets the frame.
    const std::optional<DaemonSpectrumFrame> frame = m_source.takeLatest(key);
    if (!frame.has_value()) {
        return;
    }
    const QList<QPointer<DaemonMediaController>> members = m_members;
    for (const QPointer<DaemonMediaController>& member : members) {
        if (member) {
            member->onSourceFrame(*frame);
        }
    }
}

// ── The controller ───────────────────────────────────────────────────────

DaemonMediaController::DaemonMediaController(StationServer* server,
                                               RadioModel* radioModel,
                                               QObject* parent,
                                               MediaPeer::TransportFactory peerFactory,
                                               MonotonicClock monotonicClock)
    : DaemonMediaController(server, radioModel, 0,
                            // Its own engines, owned as a Qt child (below), so
                            // they go with it and are found among its children.
                            std::shared_ptr<DaemonSharedSpectrum>(
                                new DaemonSharedSpectrum(radioModel),
                                [](DaemonSharedSpectrum*) {}),
                            parent, std::move(peerFactory), std::move(monotonicClock))
{
    m_shared->setParent(this);
    if (!m_server || !m_radioModel) {
        return;
    }
    // A controller on its own is the Core's only one: it answers the
    // PureSignal display gate and turns budget enforcement on itself.
    const QPointer<DaemonMediaController> self(this);
    m_server->setPs3DisplayAdmissionHandler([self](bool enabled, QString* refusal) {
        if (!self) {
            if (refusal) { *refusal = QStringLiteral("The Core stopped its display service."); }
            return false;
        }
        return self->admitPs3Display(enabled, refusal);
    });
    // The command gate and accepted-state notification are both installed
    // before capability publication can advertise budget enforcement.
    m_server->setDisplayBudgetEnforcementEnabled(true);
}

DaemonMediaController::DaemonMediaController(StationServer* server,
                                               RadioModel* radioModel,
                                               quint64 boundEpoch,
                                               std::shared_ptr<DaemonSharedSpectrum> spectrum,
                                               QObject* parent,
                                               MediaPeer::TransportFactory peerFactory,
                                               MonotonicClock monotonicClock)
    : QObject(parent)
    , m_server(server)
    , m_radioModel(radioModel)
    , m_boundEpoch(boundEpoch)
    , m_shared(std::move(spectrum))
    , m_source(m_shared->source())
    , m_sources(m_shared->runtimes())
    , m_peerFactory(std::move(peerFactory))
    , m_monotonicClock(std::move(monotonicClock))
    , m_sendTimer(this)
    , m_audioDiagnosticsTimer(this)
    , m_displayDiagnosticsTimer(this)
{
    m_receiverStreamSlice.fill(-1);
    m_receiverNextSequence.fill(1);
    m_receiverNextTimestamp.fill(0);
    m_shared->join(this);
    m_sendTimer.setInterval(kDisplaySenderIntervalMs);
    connect(&m_sendTimer, &QTimer::timeout, this, &DaemonMediaController::onSendTick);
    m_audioDiagnosticsTimer.setInterval(kAudioDiagnosticsLogIntervalMs);
    connect(&m_audioDiagnosticsTimer, &QTimer::timeout, this, [this] {
        maybeLogAudioDiagnostics(false);
    });
    m_displayDiagnosticsTimer.setInterval(kDisplayDiagnosticsLogIntervalMs);
    connect(&m_displayDiagnosticsTimer, &QTimer::timeout, this, [this] {
        // Control logging lane: a pair that changed since is logged.
        logMediaPath();
        logDisplayDiagnostics(false);
    });
    // iPhone app plan Task 29: a replacement's overlap, its connect bound,
    // and the old peer's drain.
    m_replaceOverlapTimer.setSingleShot(true);
    connect(&m_replaceOverlapTimer, &QTimer::timeout, this,
            &DaemonMediaController::finishReplacement);
    m_replaceConnectTimer.setSingleShot(true);
    connect(&m_replaceConnectTimer, &QTimer::timeout, this, [this] {
        failReplacement(QString::fromLatin1(kMediaPeerClosedReason));
    });
    m_retireDrainTimer.setSingleShot(true);
    connect(&m_retireDrainTimer, &QTimer::timeout, this, [this] {
        if (m_retiring) {
            MediaPeer* const peer = m_retiring.release();
            peer->disconnect(this);
            peer->stop();
            peer->deleteLater();
        }
    });
    if (!m_server || !m_radioModel) {
        return;
    }
    connect(m_server, &StationServer::mediaSessionStarted,
            this, &DaemonMediaController::onSessionStarted);
    connect(m_server, &StationServer::mediaSessionEnded,
            this, &DaemonMediaController::onSessionEnded);
    connect(m_server, &StationServer::mediaControlReceived,
            this, &DaemonMediaController::onControl);
    connect(m_server, &StationServer::displayBudgetChanged,
            this, [this] {
        ++m_iqBudgetChangeRevision;
        beginDisplayBudgetIfNeeded();
        refreshDisplayBudgetPacer();
        if (!m_iqStreams.empty() && !m_iqBudgetRecheckScheduled) {
            m_iqBudgetRecheckScheduled = true;
            QTimer::singleShot(0, this, [this] {
                m_iqBudgetRecheckScheduled = false;
                reconcileIqBudget();
            });
        }
        // A Core-busy reason starting or ending changes how every source's
        // transforms advance.
        if (m_transformsFollowFrameRate != coreBusyLimitsSources()) {
            m_transformsFollowFrameRate = coreBusyLimitsSources();
            const QList<MediaSourceKey> keys = m_sources.keys();
            for (const MediaSourceKey& key : keys) {
                if (m_sources.value(key).configured) {
                    reconcileSource(key);
                }
            }
        }
    });
    // Task 76: this session's owner mix follows whose each slice is.
    // Slice control plan Task 4: and who listens to it, so a leave (stop
    // listening, the listener half of a claims removal) reaches the media
    // as a change of controller does.
    const auto followAccess = [this](int sliceId) {
        const QPointer<DaemonMediaController> self(this);
        refreshOwnerMixMask();
        // Fix wave: a slice this device can no longer see is gone from its
        // view (ruling 5.8 sends it object.destroy), so its displays
        // retire as a removed slice's do (ruling 9.1). A former controller
        // that stays a listener still sees it and keeps them.
        if (self && m_epoch != 0 && m_radioModel && m_radioModel->sliceById(sliceId) != nullptr
            && !seesSlice(sliceId)) {
            retireSliceDisplays(sliceId);
        }
    };
    connect(m_radioModel->sliceOwnership(), &SliceOwnership::markChanged, this,
            [followAccess](int sliceId, const QByteArray&, const QByteArray&) {
                followAccess(sliceId);
            });
    connect(m_radioModel->sliceOwnership(), &SliceOwnership::listenersChanged, this,
            followAccess);
    // A slice made for this device is noted without a mark change.
    connect(m_radioModel->sliceOwnership(), &SliceOwnership::activeChanged, this,
            [this] { refreshOwnerMixMask(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { refreshOwnerMixMask(); });
    connect(m_radioModel, &RadioModel::streamCentreChanged,
            this, &DaemonMediaController::onStreamGeometryChanged);
    connect(m_radioModel, &RadioModel::streamBindingsChanged,
            this, &DaemonMediaController::onStreamBindingsChanged);
    connect(m_radioModel, &RadioModel::sliceRemoved,
            this, &DaemonMediaController::onSliceRemoved);
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &DaemonMediaController::onRadioConnectionStateChanged);
    // R-R3-45: the headphones mix runs while some slice plays on the
    // headphones, so follow every slice's output route.
    for (SliceModel* slice : m_radioModel->slices()) {
        watchSliceOutputRoute(slice);
    }
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int sliceId) {
        if (m_radioModel) {
            watchSliceOutputRoute(m_radioModel->sliceById(sliceId));
        }
        refreshOwnerMixMask();
        onOutputRoutesChanged();
    });
    // Capture changes can be emitted inside endpoint lease release. Process
    // them after map replacement/removal completes, then revalidate again at
    // send time so no old source row slips through the queued notification.
    connect(m_radioModel, &RadioModel::widebandSourceChanged, this,
            &DaemonMediaController::onWidebandSourceChanged, Qt::QueuedConnection);
    connect(m_radioModel, &RadioModel::streamAdcRoutingChanged, this,
            [this]() { onWidebandSourceChanged(-1); }, Qt::QueuedConnection);
    connect(m_radioModel->pureSignalFacade(), &PureSignalSessionFacade::displayInvalidated,
            this, [this]() {
        m_ps3CurrentChunks.clear();
        m_ps3LatestChunks.clear();
        m_ps3CurrentAttempted = false;
    });
    connect(m_radioModel->pureSignalFacade(),
            &PureSignalSessionFacade::remoteAmpViewSubscriptionChanged,
            this, &DaemonMediaController::onRemoteAmpViewSubscriptionChanged);
    connect(m_radioModel->pureSignalFacade(), &PureSignalSessionFacade::displaySnapshotReady,
            this, [this](const Ps3Snapshot& snapshot) {
        if (m_epoch == 0 || !m_peer || !m_peer->isReady() || !ps3DisplayHere()) {
            return;
        }
        // One latest snapshot, including headers, remains bounded to 160 KiB.
        // The facade samples at <=10 Hz; ordinary MediaPeer sends preserve
        // the 64 KiB message limit and account every chunk in R35 telemetry.
        QList<QByteArray> encoded = Ps3DisplayCodec::encode(snapshot);
        if (encoded.isEmpty()) {
            return;
        }
        if (m_ps3CurrentChunks.isEmpty() || !m_ps3CurrentAttempted) {
            m_ps3CurrentChunks = std::move(encoded);
            m_ps3CurrentAttempted = false;
        } else {
            m_ps3LatestChunks = std::move(encoded);
        }
        if (!m_sendTimer.isActive()) {
            m_sendTimer.start();
        }
    });
    // iPhone app plan Task 36 (R-IOS-13): keys wait on the microphone line,
    // VOX armed follows the operator's VOX and the session's permission,
    // and starvation is watched while the device is keyed on its line.
    // Fix wave C2: a controller on its own is the Core's only line; under a
    // DaemonMediaHub the hub routes the keying's view by device.
    if (m_boundEpoch == 0 && m_server->remoteKeying() != nullptr) {
        m_server->remoteKeying()->setMicUplink(micUplink());
    }
    connect(&m_radioModel->transmitModel(), &TransmitModel::voxEnabledChanged, this,
            [this](bool) { refreshMicVoxArmed(); });
    connect(m_server, &StationServer::voxArmedByChanged, this,
            [this](const QByteArray&) { refreshMicVoxArmed(); });
    // Fix wave C2: one line writes the transmitter's feed at a time.
    connect(m_radioModel, &RadioModel::remoteMicWriterChanged, this,
            [this](const QByteArray& writer) {
                if (m_micReceiver) {
                    m_micReceiver->setFeedWriter(!m_micDeviceId.isEmpty() && writer == m_micDeviceId);
                }
            });
    if (m_server->transmitHolder() != nullptr) {
        connect(m_server->transmitHolder(), &TransmitHolder::changed, this,
                &DaemonMediaController::refreshMicVoxArmed);
    }
    // Parity Task 32: MON follows the holder and the MON button.
    if (m_server->transmitHolder() != nullptr) {
        connect(m_server->transmitHolder(), &TransmitHolder::changed, this,
                &DaemonMediaController::refreshTxMonitor);
    }
    connect(&m_radioModel->transmitModel(), &TransmitModel::monEnabledChanged, this,
            [this](bool) { refreshTxMonitor(); });
    connect(m_radioModel, &RadioModel::keyedByChanged, this,
            &DaemonMediaController::refreshMicWatching);
    connect(m_radioModel, &RadioModel::remoteMicInUseChanged, this,
            [this](bool) { refreshMicWatching(); });
    // TX stall lane: MoxController owns MOX, so the unkey line follows its
    // walk; TransmitModel::moxChanged only sees the no-controller fallback.
    if (MoxController* moxController = m_radioModel->moxController()) {
        connect(moxController, &MoxController::moxChanging, this,
                [this](int, bool oldMox, bool newMox) {
                    if (oldMox && !newMox) {
                        snapshotUnkeyStats();
                    } else if (!oldMox && newMox) {
                        // A key cut the last unkey's walk short.
                        logUnkeyStats();
                        m_overKeepaliveWaitMaxUs = -1;   // a new over
                    }
                });
        connect(moxController, &MoxController::moxStateChanged, this, [this](bool on) {
            if (!on) {
                logUnkeyStats();
            }
        });
    } else {
        connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged, this,
                [this](bool mox) {
                    if (!mox) {
                        snapshotUnkeyStats();
                        logUnkeyStats();
                    } else {
                        m_overKeepaliveWaitMaxUs = -1;   // a new over
                    }
                });
    }
    // Task 76: a controller bound to a session starts with it at once
    // (DaemonMediaHub makes it as that session's media starts).
    if (m_boundEpoch != 0 && m_server->mediaAvailable(m_boundEpoch)) {
        onSessionStarted(m_boundEpoch);
    }
}

DaemonMediaController::~DaemonMediaController()
{
    // TX stall lane: an unkey whose walk is still running when the
    // controller goes (its session ended in the tail) still gets its line.
    logUnkeyStats();
    // TX mic thread: no packet reaches the receiver from here on, whatever
    // order the members go in.
    {
        const std::lock_guard<std::mutex> lock(m_micRoute->mutex);
        m_micRoute->receiver = nullptr;
    }
    if (m_radioModel) {
        // Parity Task 31: this device's DUP goes with its media.
        if (!m_duplexDevice.isEmpty()) {
            m_radioModel->setDeviceDisplayDuplex(m_duplexDevice, false);
        }
        m_radioModel->pureSignalFacade()->disconnect(this);
        if (m_radioModel->sliceOwnership()) {
            m_radioModel->sliceOwnership()->disconnect(this);
        }
    }
    if (m_server) {
        m_server->disconnect(this);
        if (m_boundEpoch == 0) {
            m_server->setPs3DisplayAdmissionHandler({});
            m_server->setDisplayBudgetEnforcementEnabled(false);
        }
        if (m_boundEpoch == 0 && m_server->remoteKeying() != nullptr) {
            m_server->remoteKeying()->setMicUplink({});
        }
    }
    // As onSessionEnded(): clear the session while the pacer session is
    // live, then end the pacer and its initialised flag together, so the
    // teardown never updates a pacer that has no session.
    clearSession();
    m_displayPacer.endSession();
    m_displayPacerInitialized = false;
    m_epoch = 0;
    releaseOwnerMix();
    m_shared->leave(this);
}

int DaemonMediaController::activeEndpointCount() const
{
    return static_cast<int>(m_endpoints.size());
}

int DaemonMediaController::activeSourceCount() const
{
    int active = 0;
    const QList<MediaSourceKey> keys = m_source.activeSources();
    for (const MediaSourceKey& key : keys) {
        if (m_source.isActive(key)) {
            ++active;
        }
    }
    return active;
}

void DaemonMediaController::setAudioTargetBitrate(int bitsPerSecond)
{
    // Stored only: a peer and sender already built keep the target they
    // were built with, so a live session never changes encoder mid-context.
    m_audioTargetBitrate = bitsPerSecond;
}

DaemonAudioDiagnostics DaemonMediaController::audioDiagnostics() const
{
    return snapshotAudioDiagnostics();
}

DaemonDisplayDiagnostics DaemonMediaController::displayDiagnostics() const
{
    return m_displayDiagnostics;
}

QString daemonDisplayDiagnosticsLine(const DaemonDisplayDiagnostics& diagnostics)
{
    return QStringLiteral("largestKeyframe=%1 bytes/%2 fragments"
                          " largestDelta=%3 bytes/%4 fragments"
                          " maxFragments=%5 sendRefusals=%6 transportErrors=%7"
                          " queuedLate=%8 keyframeRequests=%9"
                          " keyframeRequestsRefused=%10")
        .arg(diagnostics.displayMaxKeyframeBytes)
        .arg(IMediaTransport::sctpFragmentCount(diagnostics.displayMaxKeyframeBytes))
        .arg(diagnostics.displayMaxDeltaBytes)
        .arg(IMediaTransport::sctpFragmentCount(diagnostics.displayMaxDeltaBytes))
        .arg(diagnostics.displayMaxFragments)
        .arg(diagnostics.displaySendRefusals)
        .arg(diagnostics.displayTransportErrors)
        .arg(diagnostics.displayQueuedLate)
        .arg(diagnostics.displayKeyframeRequests)
        .arg(diagnostics.displayKeyframeRequestsRefused);
}

void DaemonMediaController::recordDisplaySent(const QByteArray& spectrumFrame, bool keyframe)
{
    if (spectrumFrame.isEmpty()) {
        return;
    }
    const auto bytes = static_cast<quint32>(spectrumFrame.size());
    quint32& largest = keyframe ? m_displayDiagnostics.displayMaxKeyframeBytes
                                : m_displayDiagnostics.displayMaxDeltaBytes;
    largest = std::max(largest, bytes);
    m_displayDiagnostics.displayMaxFragments = std::max(
        m_displayDiagnostics.displayMaxFragments,
        static_cast<quint32>(IMediaTransport::sctpFragmentCount(bytes)));
}

void DaemonMediaController::onMediaPeerError(const QString& message)
{
    // Signalling, factory and audio errors: logged once per text, not
    // counted as display errors and no reason to restart the display.
    if (!m_loggedTransportErrorKinds.contains(message)
        && m_loggedTransportErrorKinds.size() < kMaxLoggedTransportErrorKinds) {
        m_loggedTransportErrorKinds.insert(message);
        qCWarning(lcDaemonMedia).noquote()
            << QStringLiteral("media peer error: %1 (repeats are not logged)")
                   .arg(message.left(256));
    }
}

void DaemonMediaController::onMediaTransportError(const QString& message)
{
    ++m_displayDiagnostics.displayTransportErrors;
    const QString kind = QStringLiteral("display:") + message;
    if (!m_loggedTransportErrorKinds.contains(kind)
        && m_loggedTransportErrorKinds.size() < kMaxLoggedTransportErrorKinds) {
        m_loggedTransportErrorKinds.insert(kind);
        qCWarning(lcDaemonMedia).noquote()
            << QStringLiteral("media transport error: %1 (repeats are counted, not logged)")
                   .arg(message.left(256));
    }
    // Treat it as a failed display send: nothing already encoded is resent.
    // Every spectrum endpoint restarts from a keyframe, and a PureSignal
    // snapshot already partly sent is abandoned for the next one.
    for (auto& [endpointId, entry] : m_endpoints) {
        Q_UNUSED(endpointId);
        entry.forceKeyframe = true;
    }
    if (m_ps3CurrentAttempted) {
        m_ps3CurrentChunks.clear();
    }
}

void DaemonMediaController::logDisplayDiagnostics(bool final)
{
    if (m_displayDiagnostics == m_displayDiagnosticsLogged) {
        return;
    }
    m_displayDiagnosticsLogged = m_displayDiagnostics;
    // Control logging lane: the media connection's SCTP rtt rides along.
    const std::optional<qint64> rtt = m_peer ? m_peer->rttMs() : std::nullopt;
    qCInfo(lcDaemonMedia).noquote()
        << QStringLiteral("daemon display diagnostics %1 %2 mediaRttMs=%3")
               .arg(final ? QStringLiteral("final") : QStringLiteral("periodic"),
                    daemonDisplayDiagnosticsLine(m_displayDiagnostics),
                    rtt ? QString::number(*rtt) : QStringLiteral("none"));
}

void DaemonMediaController::logMediaPath()
{
    if (!m_peer) {
        return;
    }
    m_mediaPathChecked.start();
    const std::optional<MediaIcePath> path = m_peer->selectedPath();
    if (!path) {
        return;
    }
    const QString text = ControlLog::mediaPathText(*path);
    if (text == m_mediaPathLogged) {
        return;
    }
    const bool changed = !m_mediaPathLogged.isEmpty();
    m_mediaPathLogged = text;
    const std::optional<qint64> rtt = m_peer->rttMs();
    const QByteArray device = m_server ? m_server->mediaSessionDevice(m_epoch) : QByteArray();
    qCInfo(lcDaemonMedia).noquote()
        << QStringLiteral("Media link for %1%2: %3; rtt %4")
               .arg(device.isEmpty() ? QStringLiteral("a device not known")
                                     : QString::fromLatin1(device.toHex()),
                    changed ? QStringLiteral(" (changed)") : QString(), text,
                    rtt ? QStringLiteral("%1 ms").arg(*rtt) : QStringLiteral("not measured"));
}

std::optional<SpectrumGrant> DaemonMediaController::spectrumGrant(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return std::nullopt;
    }
    return it->second.grant;
}

bool DaemonMediaController::coreBusyLimitsSources() const
{
    // Task 76: the engines are shared, so this follows the Core's total:
    // its reason is CoreBusy only while the governor's cut is in force.
    return m_server && m_server->displayBudgetReason() == DisplayBudgetReason::CoreBusy;
}

std::optional<bool> DaemonMediaController::spectrumSourceTransformsFollowFrameRate(
    quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return std::nullopt;
    }
    const auto source = m_sources.constFind(it->second.request.source);
    if (source == m_sources.cend() || !source->configured) {
        return std::nullopt;
    }
    return source->config.transformsFollowFrameRate;
}

std::optional<int> DaemonMediaController::spectrumSourceDecimation(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return std::nullopt;
    }
    const int decimation = m_source.engineDecimation(it->second.request.source);
    return decimation > 0 ? std::optional<int>(decimation) : std::nullopt;
}

std::optional<DaemonMediaController::SpectrumAveraging>
DaemonMediaController::spectrumAveraging(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return std::nullopt;
    }
    return SpectrumAveraging{it->second.request.trace.averageAlpha,
                             it->second.request.waterfall.averageAlpha};
}

DisplayExtrasProcessor* DaemonMediaController::displayExtrasForTest(quint32 endpointId)
{
    const auto it = m_endpoints.find(endpointId);
    return it == m_endpoints.end() ? nullptr : it->second.extras.get();
}

std::optional<int> DaemonMediaController::spectrumSourceFps(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return std::nullopt;
    }
    const auto source = m_sources.constFind(it->second.request.source);
    if (source == m_sources.cend() || !source->configured) {
        return std::nullopt;
    }
    return source->config.fft.fps;
}

qint64 DaemonMediaController::displayNowNs() const
{
    // R-R3-21 (displayClockVersion 1): the producer clock the display frames
    // are stamped from, so audio capture, clock echoes and display frames
    // share one Core clock. It was a per-session QElapsedTimer, whose origin
    // no window could learn.
    return m_monotonicClock ? m_monotonicClock() : DaemonSpectrumSource::monotonicNowNs();
}

bool DaemonMediaController::displayBudgetWireAvailable() const
{
    return m_server && m_server->displayBudgetAvailable(m_epoch);
}

bool DaemonMediaController::displayPacingRequired() const
{
    return m_server && m_server->displayBudgetLimits(m_epoch).has_value();
}

DisplayBudgetCharge DaemonMediaController::currentSpectrumCharge() const
{
    QList<DisplayBudgetCharge> charges;
    charges.reserve(static_cast<qsizetype>(m_endpoints.size()));
    for (const auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        charges.append(entry.displayCost.charge);
    }
    return sumDisplayCharges(charges).value_or(DisplayBudgetCharge{});
}

DisplayBudgetCharge DaemonMediaController::ownDisplayCharge() const
{
    return acceptedDisplayCharge();
}

DisplayBudgetCharge DaemonMediaController::displayDemand() const
{
    QList<DisplayBudgetCharge> charges;
    for (const auto& [unused, demand] : m_displayDemand) {
        Q_UNUSED(unused);
        charges.append(demand);
    }
    const quint64 iq = iqBytesPerSecond();
    if (iq != 0) { charges.append({iq, 0, 0}); }
    return sumDisplayCharges(charges).value_or(DisplayBudgetCharge{});
}

void DaemonMediaController::setDisplayDemand(quint32 endpointId,
                                             std::optional<DisplayBudgetCharge> demand)
{
    const auto it = m_displayDemand.find(endpointId);
    const std::optional<DisplayBudgetCharge> before =
        it == m_displayDemand.end() ? std::nullopt : std::optional(it->second);
    if (before == demand) {
        return;
    }
    if (demand) {
        m_displayDemand[endpointId] = *demand;
    } else {
        m_displayDemand.erase(endpointId);
    }
    // Every device's share follows every device's demand.
    if (m_server && m_boundEpoch != 0) {
        m_server->publishDisplayBudgetCapabilities();
    }
}

void DaemonMediaController::holdRefusedDemand(quint32 endpointId,
                                              std::optional<DisplayBudgetCharge> before)
{
    m_refusedDemand[endpointId] =
        RefusedDemand{before, displayNowNs() + qint64{m_refusedDemandHoldMs} * 1'000'000};
    endRefusedDemands();
}

void DaemonMediaController::endRefusedDemands()
{
    const qint64 now = displayNowNs();
    std::optional<qint64> next;
    QList<QPair<quint32, std::optional<DisplayBudgetCharge>>> ended;
    for (auto it = m_refusedDemand.begin(); it != m_refusedDemand.end();) {
        if (it->second.endsAtNs <= now) {
            ended.append({it->first, it->second.before});
            it = m_refusedDemand.erase(it);
            continue;
        }
        next = next ? std::min(*next, it->second.endsAtNs) : it->second.endsAtNs;
        ++it;
    }
    // Fix wave 2 (Important 2): the client did not ask for it again, so
    // the refused display asks for nothing more than it did before.
    for (const auto& [endpointId, before] : ended) {
        setDisplayDemand(endpointId, before);
    }
    if (!next) {
        if (m_refusedDemandTimer) {
            m_refusedDemandTimer->stop();
        }
        return;
    }
    if (!m_refusedDemandTimer) {
        m_refusedDemandTimer = new QTimer(this);
        m_refusedDemandTimer->setSingleShot(true);
        connect(m_refusedDemandTimer, &QTimer::timeout, this,
                &DaemonMediaController::endRefusedDemands);
    }
    const qint64 waitMs = std::max<qint64>(1, (*next - now + 999'999) / 1'000'000);
    m_refusedDemandTimer->start(static_cast<int>(std::min<qint64>(waitMs, 60'000)));
}

DisplayBudgetCharge DaemonMediaController::acceptedDisplayCharge() const
{
    const bool ps3Enabled = ps3DisplayHere();
    return sumDisplayCharges({currentSpectrumCharge(),
                              ps3Enabled ? ps3DisplayCharge() : DisplayBudgetCharge{},
                              DisplayBudgetCharge{iqBytesPerSecond(), 0, 0}})
        .value_or(DisplayBudgetCharge{});
}

std::optional<DisplayBudgetCharge> DaemonMediaController::proposedSpectrumCharge(
    quint32 endpointId, const DisplayBudgetCharge& replacement) const
{
    QList<DisplayBudgetCharge> charges;
    charges.reserve(static_cast<qsizetype>(m_endpoints.size() + 1));
    bool replaced = false;
    for (const auto& [currentId, entry] : m_endpoints) {
        if (currentId == endpointId) {
            charges.append(replacement);
            replaced = true;
        } else {
            charges.append(entry.displayCost.charge);
        }
    }
    if (!replaced) {
        charges.append(replacement);
    }
    return sumDisplayCharges(charges);
}

bool DaemonMediaController::spectrumAdmissionFits(
    quint32 endpointId, const DisplayBudgetCharge& replacement) const
{
    const auto limits = m_server ? m_server->displayBudgetLimits(m_epoch) : std::nullopt;
    if (!limits) {
        return true;
    }
    const auto existing = m_endpoints.find(endpointId);
    if (existing != m_endpoints.end()) {
        const DisplayBudgetCharge old = existing->second.displayCost.charge;
        if (replacement.applicationBytesPerSecond <= old.applicationBytesPerSecond
            && replacement.spectrumSampleUnitsPerSecond
                <= old.spectrumSampleUnitsPerSecond
            && replacement.messagesPerSecond <= old.messagesPerSecond) {
            return true;
        }
    }
    const auto spectrum = proposedSpectrumCharge(endpointId, replacement);
    if (!spectrum) {
        return false;
    }
    const bool ps3Enabled = ps3DisplayHere();
    const auto combined = sumDisplayCharges(
        {*spectrum, ps3Enabled ? ps3DisplayCharge() : DisplayBudgetCharge{},
         DisplayBudgetCharge{iqBytesPerSecond(), 0, 0}});
    return combined && displayChargeFits(*limits, *combined);
}

void DaemonMediaController::beginDisplayBudgetIfNeeded()
{
    if (m_displayPacerInitialized || m_epoch == 0 || !m_server) {
        return;
    }
    const auto limits = m_server->displayBudgetLimits(m_epoch);
    if (!limits) {
        return;
    }
    m_displayPacerInitialized = m_displayPacer.beginSession(
        m_epoch, *limits, displayNowNs());
}

void DaemonMediaController::refreshDisplayBudgetPacer()
{
    beginDisplayBudgetIfNeeded();
    if (!m_displayPacerInitialized || !m_server) {
        return;
    }
    const auto limits = m_server->displayBudgetLimits(m_epoch);
    if (!limits) {
        return;
    }
    const bool ps3Enabled = ps3DisplayHere();
    if (!m_displayPacer.update(*limits, currentSpectrumCharge(), ps3Enabled,
                               displayNowNs(), iqBytesPerSecond())) {
        qCWarning(lcDaemonMedia) << "refused invalid display pacer state update";
    }
}

bool DaemonMediaController::admitPs3Display(bool enabled, QString* refusal)
{
    const bool current = ps3DisplayHere();
    if (enabled == current || !enabled) {
        return true;
    }
    // Task 76 (ruling 9.3 item 4): measured against the share this session
    // has once the PureSignal display is charged to it.
    const auto limits = m_server ? m_server->displayBudgetLimitsAsPs3Subscriber(m_epoch)
                                 : std::nullopt;
    if (!limits) {
        return true;
    }
    const auto combined = sumDisplayCharges({currentSpectrumCharge(), ps3DisplayCharge()});
    if (combined && displayChargeFits(*limits, *combined)) {
        return true;
    }
    if (refusal) {
        *refusal = QStringLiteral("The Core has no room left for the PureSignal display.");
    }
    return false;
}

void DaemonMediaController::onRemoteAmpViewSubscriptionChanged(bool subscribed)
{
    if (!subscribed) {
        m_ps3CurrentChunks.clear();
        m_ps3LatestChunks.clear();
        m_ps3CurrentAttempted = false;
    }
    refreshDisplayBudgetPacer();
    if (m_server) {
        m_server->publishDisplayBudgetCapabilities();
    }
}

void DaemonMediaController::rememberNonliveOperation(
    quint32 endpointId, const AllocationRecord& record)
{
    forgetNonliveOperation(endpointId);
    m_nonliveOperations.emplace(endpointId, record);
    m_nonliveOperationOrder.append(endpointId);
    while (m_nonliveOperationOrder.size() > kRecentAllocationRecords) {
        const quint32 oldest = m_nonliveOperationOrder.takeFirst();
        m_nonliveOperations.erase(oldest);
    }
}

void DaemonMediaController::forgetNonliveOperation(quint32 endpointId)
{
    m_nonliveOperations.erase(endpointId);
    m_nonliveOperationOrder.removeAll(endpointId);
}

void DaemonMediaController::clearAllocationIdentity()
{
    m_endpointHighWater = 0;
    m_nonliveOperations.clear();
    m_nonliveOperationOrder.clear();
}

void DaemonMediaController::sendAllocationResult(
    const QString& connectionId, quint32 endpointId, quint32 revision,
    bool accepted, const QString& reason)
{
    if (connectionId.isEmpty() || !m_server) {
        return;
    }
    const auto limits = m_server->displayBudgetLimits(m_epoch);
    if (!limits) {
        return;
    }
    quint32 acceptedRevision = 0;
    DisplayBudgetCharge retained;
    const auto current = m_endpoints.find(endpointId);
    if (current != m_endpoints.end()) {
        acceptedRevision = current->second.revision;
        retained = current->second.displayCost.charge;
    }
    sendControl({
        {QStringLiteral("op"), QStringLiteral("allocation-result")},
        {QStringLiteral("connectionId"), connectionId},
        {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
        {QStringLiteral("revision"), static_cast<qint64>(revision)},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("budgetGeneration"), static_cast<qint64>(limits->generation)},
        {QStringLiteral("acceptedRevision"), static_cast<qint64>(acceptedRevision)},
        {QStringLiteral("applicationBytesPerSecond"),
         static_cast<qint64>(retained.applicationBytesPerSecond)},
        {QStringLiteral("spectrumSampleUnitsPerSecond"),
         static_cast<qint64>(retained.spectrumSampleUnitsPerSecond)},
        {QStringLiteral("messagesPerSecond"), static_cast<qint64>(retained.messagesPerSecond)},
    });
}

bool DaemonMediaController::rejectAllocation(
    const QJsonObject& control, quint32 endpointId, quint32 revision,
    const QString& reason, bool remember)
{
    const QString connectionId = control.value(QStringLiteral("connectionId")).toString();
    if (displayBudgetWireAvailable()) {
        if (remember) {
            AllocationRecord record{control, revision, false, false, reason};
            auto active = m_endpoints.find(endpointId);
            if (active != m_endpoints.end()) {
                if (!staleOrEqualRevision(revision, active->second.allocation.revision)) {
                    active->second.allocation = std::move(record);
                }
            } else {
                const auto recent = m_nonliveOperations.find(endpointId);
                if (recent == m_nonliveOperations.end()
                    || (!recent->second.explicitlyRetired
                        && !staleOrEqualRevision(revision, recent->second.revision))) {
                    rememberNonliveOperation(endpointId, record);
                }
            }
        }
        sendAllocationResult(connectionId, endpointId, revision, false, reason);
    } else {
        sendRejected(connectionId, endpointId, revision, reason);
    }
    return false;
}

void DaemonMediaController::onSessionStarted(quint64 epoch)
{
    if (epoch == 0 || epoch <= m_lastSessionEpoch) {
        return;
    }
    // Task 76: a bound controller serves its own session alone; one on its
    // own takes a new session only once the one it serves is gone, so it
    // never takes media away from a live device.
    if (m_boundEpoch != 0 && epoch != m_boundEpoch) {
        return;
    }
    if (m_boundEpoch == 0 && m_epoch != 0 && m_server && m_server->mediaAvailable(m_epoch)) {
        return;
    }
    if (m_epoch != 0) {
        m_displayPacer.endSession();
        m_displayPacerInitialized = false;
    }
    clearSession();
    m_lastSessionEpoch = epoch;
    m_epoch = epoch;
    acquireOwnerMix();
    beginDisplayBudgetIfNeeded();
    refreshDisplayBudgetPacer();
}

void DaemonMediaController::onSessionEnded(quint64 epoch)
{
    if (epoch == m_epoch) {
        clearSession();
        m_displayPacer.endSession();
        m_displayPacerInitialized = false;
        m_epoch = 0;
        releaseOwnerMix();
    }
}

void DaemonMediaController::acquireOwnerMix()
{
    if (m_ownerMix >= 0 || !m_radioModel || !m_radioModel->audioEngine()) {
        refreshOwnerMixMask();
        return;
    }
    m_ownerMix = m_radioModel->audioEngine()->acquireOwnerMix();
    if (m_ownerMix < 0) {
        qCWarning(lcDaemonMedia) << "no owner mix free for media session" << m_epoch;
        return;
    }
    // Slice control plan Task 6: a level or mute this device sets for a
    // slice it listens to reaches its mix.
    if (m_server && m_server->sliceAccessController()) {
        m_listenLevelConnection = connect(
            m_server->sliceAccessController(), &SliceAccessController::listenLevelChanged, this,
            [this](int, const QByteArray&) { refreshOwnerMixMask(); });
    }
    refreshOwnerMixMask();
    // Parity Task 32: and the transmit monitor, if this device has one.
    refreshTxMonitor();
}

void DaemonMediaController::releaseOwnerMix()
{
    QObject::disconnect(m_listenLevelConnection);
    m_listenLevelConnection = {};
    if (m_ownerMix < 0) {
        return;
    }
    // The senders' taps on it are gone already (their captures stopped);
    // release waits for any callback still running.
    if (m_radioModel && m_radioModel->audioEngine()) {
        m_radioModel->audioEngine()->releaseOwnerMix(m_ownerMix);
    }
    m_ownerMix = -1;
}

// Slice control plan Task 2 (SliceAccessPolicy): the controller's (owner
// mix membership until Task 6, raw I/Q, the headphones mix of its own
// slices), a listener's too (receiver streams), and what it may show
// (display subscriptions, ruling Q13).
bool DaemonMediaController::controlsSlice(int sliceId) const
{
    return m_server && m_epoch != 0 && m_server->mediaSessionControlsSlice(m_epoch, sliceId);
}

bool DaemonMediaController::hearsSlice(int sliceId) const
{
    return m_server && m_epoch != 0 && m_server->mediaSessionHearsSlice(m_epoch, sliceId);
}

QList<const DaemonAudioSender*> DaemonMediaController::audioSendersForTest() const
{
    QList<const DaemonAudioSender*> senders;
    if (m_audioSender) {
        senders.append(m_audioSender.get());
    }
    if (m_headphones.sender) {
        senders.append(m_headphones.sender.get());
    }
    for (const auto& [sliceId, stream] : m_receiverStreams) {
        Q_UNUSED(sliceId);
        if (stream.sender) {
            senders.append(stream.sender.get());
        }
    }
    return senders;
}

bool DaemonMediaController::seesSlice(int sliceId) const
{
    return m_server && m_epoch != 0 && m_server->mediaSessionSeesSlice(m_epoch, sliceId);
}

void DaemonMediaController::refreshOwnerMixMask()
{
    // Ruling 9.2: this device's mix sums its own slices only.
    if (m_ownerMix < 0 || !m_radioModel || !m_radioModel->audioEngine()) {
        return;
    }
    AudioEngine* const engine = m_radioModel->audioEngine();
    SliceAccessController* const access = m_server ? m_server->sliceAccessController() : nullptr;
    const QByteArray device =
        m_server && m_epoch != 0 ? m_server->mediaSessionDevice(m_epoch) : QByteArray();
    quint32 mask = 0;
    quint32 listened = 0;
    for (SliceModel* slice : m_radioModel->slices()) {
        const int id = slice ? slice->sliceIndex() : -1;
        if (id < 0 || id >= 32) {
            continue;
        }
        if (controlsSlice(id)) {
            mask |= 1u << id;
        } else if (hearsSlice(id) && access != nullptr && !device.isEmpty()) {
            // Slice control plan Task 6 (rulings Q3, Q4): a slice this
            // device only listens to plays centered at its own level and
            // mute; the controller's AF, mute and pan do not reach it.
            const SliceAccessController::ListenLevel level = access->listenLevel(device, id);
            engine->setOwnerMixListen(m_ownerMix, id, static_cast<float>(level.level),
                                      level.muted);
            listened |= 1u << id;
        }
    }
    // The controlled mask moves before a listen lane is dropped, and a
    // listen lane is set before the mask drops the slice: a controlled
    // slice has no listen lane in the mixer, so a hand-off never leaves a
    // drain period with neither.
    engine->setOwnerMixSliceMask(m_ownerMix, mask);
    for (int id = 0; id < 32; ++id) {
        if ((listened & (1u << id)) == 0) {
            engine->clearOwnerMixListen(m_ownerMix, id);
        }
    }
    // A slice onto or off this device changes whether its headphones mix
    // has a receiver to carry.
    onOutputRoutesChanged();
    // A receiver stream of a slice no longer this device's stops.
    QList<int> lost;
    for (const auto& [sliceId, stream] : m_receiverStreams) {
        if (stream.desiredEnabled && !hearsSlice(sliceId)) {
            lost.append(sliceId);
        }
    }
    for (int sliceId : lost) {
        if (m_receiverStreams.count(sliceId) != 0) {
            reconcileReceiverAudio(sliceId);
        }
    }
}

bool DaemonMediaController::ps3DisplayHere() const
{
    // Ruling 9.3 item 4: the PureSignal display goes to its subscriber.
    if (!m_radioModel || !m_radioModel->pureSignalFacade()->remoteAmpViewSubscribed()
        || m_epoch == 0 || !m_server) {
        return false;
    }
    const quint64 subscriber = m_server->ps3DisplaySubscriberEpoch();
    return subscriber != 0 ? subscriber == m_epoch
                           : m_epoch == m_server->mediaSessionEpoch();
}

template <typename Fn>
void DaemonMediaController::forEachSharedEndpoint(const MediaSourceKey& key, Fn&& fn)
{
    for (DaemonMediaController* member : m_shared->members()) {
        for (auto& [endpointId, entry] : member->m_endpoints) {
            if (entry.request.source == key) {
                fn(*member, endpointId, entry);
            }
        }
    }
}

bool DaemonMediaController::anySharedEndpointOn(const MediaSourceKey& key) const
{
    for (DaemonMediaController* member : m_shared->members()) {
        for (const auto& [unused, entry] : member->m_endpoints) {
            Q_UNUSED(unused);
            if (entry.request.source == key) {
                return true;
            }
        }
    }
    return false;
}

void DaemonMediaController::onRadioConnectionStateChanged(ConnectionState state)
{
    // R-R3-43: the wanted receiver streams, handled after the main one.
    QList<int> wantedReceivers;
    for (const auto& [sliceId, stream] : m_receiverStreams) {
        if (stream.desiredEnabled) { wantedReceivers.append(sliceId); }
    }
    if (state != ConnectionState::Connected) {
        // Keep the accepted intent, but retire the capture bridge before any
        // display or peer lifecycle can tear down the station's audio graph.
        stopAudioCapture();
        for (int sliceId : wantedReceivers) {
            stopReceiverAudioCapture(m_receiverStreams.at(sliceId));
        }
        if (m_audioRevision != 0) {
            // The client's own choice outranks the radio, as in reconcileAudio().
            sendAudioContext(false, m_audioDesiredEnabled
                                        ? RemoteAudioOffReason::RadioOffline
                                        : RemoteAudioOffReason::ClientDisabled);
        }
        // Each wanted receiver stream keeps its stream id and intent, and
        // resumes when the radio returns.
        for (int sliceId : wantedReceivers) {
            const auto it = m_receiverStreams.find(sliceId);
            if (it != m_receiverStreams.end()) {
                sendReceiverAudioContext(sliceId, it->second, false,
                                         RemoteAudioOffReason::RadioOffline);
            }
        }
        // R-R3-45: the headphones mix keeps its intent, as receivers do.
        // The app hears of the drop only when it changes what it was last
        // told: a mix that was sending stops, and an app last told
        // media-not-ready learns radio-offline; one told
        // no-headphones-receiver or client-disabled keeps that reason.
        if (m_headphones.revision != 0) {
            const bool wasSending = m_headphones.sending;
            stopHeadphonesAudioCapture();
            const RemoteAudioOffReason reason =
                headphonesBlockedBy().value_or(RemoteAudioOffReason::RadioOffline);
            if (wasSending || m_headphones.lastOffReason != reason) {
                sendHeadphonesAudioContext(false, reason);
            }
        }
        const bool demanded = !m_displayDemand.empty();
        clearProduction();
        if (demanded && m_server && m_boundEpoch != 0) {
            m_server->publishDisplayBudgetCapabilities();
        }
        return;
    }
    reconcileAudio();
    for (int sliceId : wantedReceivers) {
        if (m_receiverStreams.count(sliceId) != 0) {
            reconcileReceiverAudio(sliceId);
        }
    }
    reconcileWantedIq();
    // R-R3-45: the headphones mix resumes with the radio, when it can run.
    if (m_headphones.revision != 0 && m_headphones.desiredEnabled && headphonesMixNeeded()) {
        reconcileHeadphonesAudio();
    }
}

void DaemonMediaController::onControl(const QJsonObject& control, quint64 epoch)
{
    // R-R3-35: a clock probe's arrival time (t1), read before anything else.
    const qint64 receivedNs = displayNowNs();
    if (!m_server || !m_server->mediaAvailable(epoch) || epoch == 0 || epoch != m_epoch
        || !control.value(QStringLiteral("op")).isString()) {
        return;
    }
    const QString op = control.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("clock-probe")) { handleClockProbe(control, receivedNs); return; }
    if (op == QLatin1String("start")) { handleStart(control); return; }
    // iPhone app plan Task 29: a new peer beside the current one.
    if (op == QLatin1String("replace")) { handleReplace(control); return; }
    if (op == QLatin1String("subscribe")) { handleSubscribe(control); return; }
    if (op == QLatin1String("unsubscribe")) { handleUnsubscribe(control); return; }
    if (op == QLatin1String("keyframe")) { handleKeyframe(control); return; }
    // R-IOS-27, R-IOS-06: only for a peer the Core told displayExtrasVersion
    // 2; any other peer's request goes where an unknown operation always
    // has.
    if (op == QLatin1String("clarity-retune") && m_server && m_server->displayExtrasAvailable()
        && m_server->displayExtrasVersion() >= 2) {
        handleClarityRetune(control);
        return;
    }
    if (op == QLatin1String("audio")) { handleAudio(control); return; }
    if (op == QLatin1String("receiver-audio")) { handleReceiverAudio(control); return; }
    if (op == QLatin1String("iq-stream")) { handleIqStream(control); return; }
    if (op == QLatin1String("headphones-audio")) { handleHeadphonesAudio(control); return; }
    // Parity Task 32: only from a peer that declared it at start.
    if (op == QLatin1String("monitor-audio")) { handleMonitorAudio(control); return; }
    acceptPeerControl(control);
}

bool DaemonMediaController::handleStart(const QJsonObject& control)
{
    // R-R3-23: a GUI that understands the lossless profile adds
    // audioProfileVersion (the Core's advertised capability) to its start,
    // and only then does the offer carry the L16 format. Every other GUI
    // sends today's two keys and gets today's offer.
    QJsonObject legacyShape = control;
    const bool declaresAudioProfile =
        control.contains(QStringLiteral("audioProfileVersion"));
    quint32 audioProfileVersion = 0;
    if (declaresAudioProfile) {
        legacyShape.remove(QStringLiteral("audioProfileVersion"));
        // A whole number, at least 1 (the first version with profiles);
        // zero, fractions, negatives and strings are refused.
        if (!m_server || !m_server->remoteAudioStatusAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("audioProfileVersion")),
                              audioProfileVersion, /*nonzero=*/true)
            || audioProfileVersion < 1) {
            return false;
        }
    }
    // R-R3-43: likewise receiverAudioVersion, the Core's advertised
    // capability; only then does the offer declare the receiver stream ids
    // and only then is a receiver-audio request honoured.
    const bool declaresReceiverAudio =
        control.contains(QStringLiteral("receiverAudioVersion"));
    if (declaresReceiverAudio) {
        legacyShape.remove(QStringLiteral("receiverAudioVersion"));
        quint32 receiverAudioVersion = 0;
        if (!m_server || !m_server->remoteAudioStatusAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("receiverAudioVersion")),
                              receiverAudioVersion, /*nonzero=*/true)
            || receiverAudioVersion < 1) {
            return false;
        }
    }
    // R-R3-45: likewise headphonesMixVersion; only then does the offer
    // declare the headphones stream id, the main stream carry the speakers'
    // mix alone, and a headphones-audio request get honoured.
    const bool declaresHeadphonesMix =
        control.contains(QStringLiteral("headphonesMixVersion"));
    if (declaresHeadphonesMix) {
        legacyShape.remove(QStringLiteral("headphonesMixVersion"));
        quint32 headphonesMixVersion = 0;
        if (!m_server || !m_server->remoteAudioStatusAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("headphonesMixVersion")),
                              headphonesMixVersion, /*nonzero=*/true)
            || headphonesMixVersion < 1) {
            return false;
        }
    }
    // Parity Task 32 (R-IOS-13, R-R3-49): txMonitorAudioVersion, from a
    // peer the Core told it (minor 11, media on); only then is a
    // monitor-audio request honoured.
    const bool declaresTxMonitor = control.contains(QStringLiteral("txMonitorAudioVersion"));
    if (declaresTxMonitor) {
        legacyShape.remove(QStringLiteral("txMonitorAudioVersion"));
        quint32 txMonitorAudioVersion = 0;
        if (!m_server || !m_server->txMonitorAudioAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("txMonitorAudioVersion")),
                              txMonitorAudioVersion, /*nonzero=*/true)
            || txMonitorAudioVersion < 1) {
            return false;
        }
    }
    // Task 29 step 2b (R-IOS-16): mediaTunnelVersion, from a peer the Core
    // told it on a session that carries binary messages; then the media
    // connection's datagrams may also run inside that WebSocket.
    const bool declaresMediaTunnel = control.contains(QStringLiteral("mediaTunnelVersion"));
    if (declaresMediaTunnel) {
        legacyShape.remove(QStringLiteral("mediaTunnelVersion"));
        quint32 mediaTunnelVersion = 0;
        if (!m_server || !m_server->mediaTunnelAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("mediaTunnelVersion")),
                              mediaTunnelVersion, /*nonzero=*/true)
            || mediaTunnelVersion != 1) {
            return false;
        }
    }
    const bool declaresRelayRouting =
        control.contains(QStringLiteral("mediaRelayRoutingVersion"));
    if (declaresRelayRouting) {
        legacyShape.remove(QStringLiteral("mediaRelayRoutingVersion"));
        quint32 version = 0;
        if (!m_server || !m_server->mediaAvailable(m_epoch)
            || !m_server->mediaReplaceAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("mediaRelayRoutingVersion")),
                              version, /*nonzero=*/true)
            || version != 1) {
            return false;
        }
    }
    // iPhone app plan Task 36 (R-IOS-13): remoteTxVersion, from a peer the
    // Core told remoteTxVersion (its hello declared remoteTx at minor 11);
    // only then does the offer carry the microphone line.
    const bool declaresRemoteTx = control.contains(QStringLiteral("remoteTxVersion"));
    if (declaresRemoteTx) {
        legacyShape.remove(QStringLiteral("remoteTxVersion"));
        quint32 remoteTxVersion = 0;
        if (!m_server || !m_server->remoteTxAvailableForMedia(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("remoteTxVersion")),
                              remoteTxVersion, /*nonzero=*/true)
            || remoteTxVersion < 1) {
            return false;
        }
    }
    const bool declaresIq = control.contains(QStringLiteral("remoteIqVersion"));
    if (declaresIq) {
        legacyShape.remove(QStringLiteral("remoteIqVersion"));
        quint32 version = 0;
        if (!m_server || !m_server->remoteIqAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("remoteIqVersion")),
                              version, /*nonzero=*/true) || version != 1) {
            return false;
        }
    }
    // Parity Task 28 (R-R3-49, A11): txDisplayVersion, from a peer the Core
    // told txDisplayVersion 1 (minor 11, media on, a TX analyzer); only then
    // does a subscribe carry the transmit window and a context `transmit`,
    // and only then does the transmitting pan get the transmit display.
    const bool declaresTxDisplay = control.contains(QStringLiteral("txDisplayVersion"));
    quint32 declaredTxDisplay = 0;
    if (declaresTxDisplay) {
        legacyShape.remove(QStringLiteral("txDisplayVersion"));
        quint32 txDisplayVersion = 0;
        if (!m_server || !m_server->txDisplayAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("txDisplayVersion")),
                              txDisplayVersion, /*nonzero=*/true)
            || txDisplayVersion < 1) {
            return false;
        }
        declaredTxDisplay = txDisplayVersion;
    }
    const bool declaresMiniDisplay = control.contains(QStringLiteral("miniDisplayVersion"));
    if (declaresMiniDisplay) {
        legacyShape.remove(QStringLiteral("miniDisplayVersion"));
        quint32 version = 0;
        if (!m_server || !m_server->miniDisplayAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("miniDisplayVersion")),
                              version, /*nonzero=*/true)
            || version != 1) {
            return false;
        }
    }
    if (!exactKeys(legacyShape, {"op", "connectionId"})
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))) {
        return false;
    }
    const QString connectionId = control.value(QStringLiteral("connectionId")).toString();
    if (m_peer) {
        if (m_peer->connectionId() == connectionId) {
            return true;
        }
        // A start on a new connection. This controller serves one device's
        // session, so the peer it holds is that device's own: its media died
        // on the app's side before the Core noticed (a half-open peer still
        // waiting for ICE consent to time out). The new start replaces it,
        // as a new sign-in from the same device replaces its old session
        // (the several-devices design, ruling 4.8): the old peer is torn
        // down, its displays retired and its demand gone. The app already
        // left the old connection, so nothing is sent for it.
        qCInfo(lcDaemonMedia) << "media start on a new connection replaces this device's"
                              << "earlier media connection";
        retirePeerKeepingSession();
        if (!m_server || !m_server->mediaAvailable(m_epoch)) {
            return false;
        }
    }
    m_peer = std::make_unique<MediaPeer>(this, m_peerFactory);
    MediaPeer* const peer = m_peer.get();
    m_displayDiagnostics = {};
    m_displayDiagnosticsLogged = {};
    m_loggedTransportErrorKinds.clear();
    m_displayDiagnosticsTimer.start();
    // Control logging lane: a new session's pair is logged afresh.
    m_mediaPathLogged.clear();
    m_mediaPathChecked.invalidate();
    // Task 29: a new start is a new timeline of SSRCs; no earlier peer's
    // stamp needs rewriting.
    m_sendSsrcRewrite.clear();
    m_micSsrcRewrite.clear();
    syncMicRoute();
    wireCurrentPeer(peer, connectionId);
    const bool offerLossless = declaresAudioProfile && m_audioLosslessAllowed;
    // iPhone app plan Task 28 (R-IOS-16): a session through the remote
    // access service makes its media connection with its control
    // connection's STUN server, and its relay only when the control path
    // is relayed (the fix wave, Important 4).
    // Task 29 step 2b: over the media tunnel when the window declared it.
    m_startTunnel = declaresMediaTunnel;
    m_startRelayRouting = declaresRelayRouting;
    {
        const auto startIce = mediaIceConfiguration();
        // Direct media ladder: what a later replacement judges an older
        // relay leg by is how the connection in use was made.
        m_currentRouted = startIce && startIce->mediaRouting();
        m_replacementRouted = false;
        peer->setIceConfiguration(startIce);
    }
    if (!peer->start(IMediaTransport::Role::Offerer, connectionId,
                     m_audioTargetBitrate, offerLossless, declaresReceiverAudio,
                     declaresHeadphonesMix, declaresRemoteTx, declaresIq)) {
        m_displayDiagnosticsTimer.stop();
        m_peer.reset();
        sendRejected(connectionId, 0, 0, QStringLiteral("The Core could not start audio and display."));
        return false;
    }
    m_receiverAudioNegotiated = declaresReceiverAudio;
    m_iqNegotiated = declaresIq;
    m_headphonesMixNegotiated = declaresHeadphonesMix;
    // Parity Task 32: MON goes nowhere until the peer asks for a route.
    m_txMonitorNegotiated = declaresTxMonitor;
    m_monitorRevision = 0;
    m_monitorRoute = TxMonitorRoute::None;
    refreshTxMonitor();
    m_txDisplayNegotiated = declaresTxDisplay;
    m_miniDisplayNegotiated = declaresMiniDisplay;
    // Task 29: what a replacement of this peer keeps.
    m_startOfferedLossless = offerLossless;
    m_startMicLine = declaresRemoteTx;
    // Parity Task 31: 3 and above may add `duplex` to a subscribe.
    m_txDisplayDeclared = declaredTxDisplay;
    if (declaresTxDisplay) {
        wireTxDisplayFeed();
    }
    if (declaresRemoteTx) {
        startMicLine(peer);
    }
    return true;
}

void DaemonMediaController::wireCurrentPeer(MediaPeer* peer, const QString& connectionId)
{
    const quint64 peerEpoch = m_epoch;
    connect(peer, &MediaPeer::errorOccurred, this,
            [this, peer, peerEpoch](const QString& message) {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            onMediaPeerError(message);
        }
    });
    connect(peer, &MediaPeer::displayErrorOccurred, this,
            [this, peer, peerEpoch](const QString& message) {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            onMediaTransportError(message);
        }
    });
    // The display channel took its held message: offer the next one now
    // rather than at the next tick, so a PureSignal snapshot advances at
    // the link's acknowledgement pace. Nothing waits here; the tick sends
    // whatever is latest.
    connect(peer, &MediaPeer::displayWritable, this, [this, peer, peerEpoch]() {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            onSendTick();
        }
    });
    connect(peer, &MediaPeer::controlReady, this,
            [this, peer, peerEpoch](const QJsonObject& outbound) {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            sendControl(outbound);
        }
    });
    // The transport reports a failed connection (ICE consent lost, a failed
    // DTLS handshake) before it closes; a closing connection only closes.
    m_peerLost = false;
    connect(peer, &MediaPeer::connectionFailed, this,
            [this, peer, peerEpoch](const QString& message) {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            m_peerLost = true;
            qCInfo(lcDaemonMedia).noquote()
                << QStringLiteral("media connection failed: %1").arg(message.left(256));
        }
    });
    connect(peer, &MediaPeer::closed, this, [this, peer, peerEpoch, connectionId]() {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            // The Core dropped this peer on its own. Tell the app at once,
            // with the whole-peer refusal (endpointId 0, revision 0), so it
            // starts media again now rather than when its own peer times
            // out; then clear. The peer's own connection id is gone by now
            // (MediaPeer clears it before it reports closed), so the one it
            // started with is used.
            const QPointer<DaemonMediaController> self(this);
            sendRejected(connectionId, 0, 0,
                         QString::fromLatin1(m_peerLost ? kMediaPeerLostReason
                                                        : kMediaPeerClosedReason));
            // Sending can end the session (its control link closing), which
            // clears the peer already.
            if (!self || m_peer.get() != peer || m_epoch != peerEpoch) {
                return;
            }
            retirePeerKeepingSession();
        }
    });
    connect(peer, &MediaPeer::ready, this, [this, peer, peerEpoch]() {
        if (m_peer.get() == peer && m_epoch == peerEpoch) {
            // Control logging lane: the pair it settled on.
            logMediaPath();
            reconcileAudio();
            // R-R3-43: each wanted receiver stream on its own, after the
            // main one; neither restarts the other.
            QList<int> wanted;
            for (const auto& [sliceId, stream] : m_receiverStreams) {
                if (stream.desiredEnabled) { wanted.append(sliceId); }
            }
            for (int sliceId : wanted) {
                if (m_peer.get() != peer || m_epoch != peerEpoch) { return; }
                reconcileReceiverAudio(sliceId);
            }
            reconcileWantedIq();
            // R-R3-45: and the headphones mix, on its own.
            if (m_peer.get() != peer || m_epoch != peerEpoch) { return; }
            if (m_headphones.revision != 0) {
                reconcileHeadphonesAudio();
            }
            // Task 36: whether the line agreed the lossless format is known
            // once both descriptions are.
            if (m_peer.get() != peer || m_epoch != peerEpoch) { return; }
            if (m_micReceiver) {
                m_micReceiver->setLosslessNegotiated(peer->micLosslessNegotiated());
            }
        }
    });
    // Task 37: the "tx" data channel's keepalives go to the Core's
    // transmit watchdog, for the device this media session is for.
    connect(peer, &MediaPeer::txReceived, this,
            [this, peer, peerEpoch](const QByteArray& message, qint64 heldUs) {
        if (m_peer.get() == peer && m_epoch == peerEpoch && m_server) {
            noteKeepaliveWait(heldUs);
            m_server->txChannelMessage(peerEpoch, message, heldUs);
            // Control logging lane: while keepalives come, a change of the
            // pair is logged within a second, beside the keepalive lines.
            // After the watchdog has the keepalive: reading the pair and
            // rtt takes the ICE and SCTP library locks.
            if (!m_mediaPathChecked.isValid()
                || m_mediaPathChecked.elapsed() >= kMediaPathCheckMs) {
                logMediaPath();
            }
        }
    });
    // Task 36: the microphone line's packets go to its receiver. Task 29:
    // after a replacement they carry this peer's SSRC, which the receiver
    // started before it takes as its own.
    connect(peer, &MediaPeer::micRtpReceived, this,
            [this, peer, peerEpoch](const QByteArray& packet, qint64 heldUs) {
        if (m_peer.get() == peer && m_epoch == peerEpoch && m_micReceiver) {
            m_micRoute->deliver(packet, heldUs);
        }
    });
    // TX mic thread: on a transport with a thread of its own for the line,
    // the packets go from that thread to the receiver, never through this
    // event loop. Every peer with the sink is this controller's current,
    // replacement or retiring one; any other is stopped, which ends its
    // thread.
    peer->setMicPacketSink(micRouteSink());
}

void DaemonMediaController::MicRoute::deliver(const QByteArray& packet, qint64 heldUs)
{
    const std::lock_guard<std::mutex> lock(mutex);
    if (receiver != nullptr) {
        receiver->submit(rewriteRtpSsrc(packet, rewrite), heldUs);
    }
}

void DaemonMediaController::syncMicRoute()
{
    const std::lock_guard<std::mutex> lock(m_micRoute->mutex);
    m_micRoute->receiver = m_micReceiver.get();
    m_micRoute->rewrite = m_micSsrcRewrite;
}

IMediaTransport::MicPacketSink DaemonMediaController::micRouteSink() const
{
    const std::shared_ptr<MicRoute> route = m_micRoute;
    return [route](const QByteArray& packet, qint64 heldUs) { route->deliver(packet, heldUs); };
}

// ---- iPhone app plan Task 29 (R-IOS-16): replacing the media connection ----
//
// The media document, "Replacing the media connection". A GUI whose control
// session moved to a better path asks for a new peer beside the current
// one. Once it is ready every audio packet goes out on both, with the same
// sequence number and timestamp and each peer's own SSRC, so the app hears
// no gap and drops the copies by timestamp; kReplaceOverlapMs later the new
// peer takes over (displays next, each on a keyframe), the Core says so
// with `replace`, and the old peer drains microphone packets and "tx"
// keepalives for kReplaceDrainMs before it closes. Nothing here keys, and
// no replacement starts or finishes while the radio is keyed or MOX's
// delay timers run.

QList<quint32> DaemonMediaController::audioSsrcsOf(const MediaPeer* peer)
{
    QList<quint32> ssrcs;
    if (peer == nullptr) {
        return ssrcs;
    }
    ssrcs.append(peer->audioSsrc());
    const QList<quint32> receivers = peer->receiverAudioSsrcs();
    for (int i = 0; i < IMediaTransport::kMaxReceiverAudioStreams; ++i) {
        ssrcs.append(receivers.value(i, 0));
    }
    ssrcs.append(peer->headphonesAudioSsrc());
    return ssrcs;
}

bool DaemonMediaController::radioIdleForReplace() const
{
    const MoxController* mox = m_radioModel ? m_radioModel->moxController() : nullptr;
    return mox == nullptr || mox->state() == MoxState::Rx;
}

bool DaemonMediaController::sendAudioRtp(MediaPeer* peer, const QByteArray& packet)
{
    // A sender started before a replacement stamps the peer it started
    // with; the current peer sends and accepts only its own SSRCs.
    const QByteArray current = rewriteRtpSsrc(packet, m_sendSsrcRewrite);
    const bool accepted = peer->sendRtp(current);
    if (m_replacement && m_replacementReady && peer == m_peer.get() && current.size() >= 12) {
        const QList<quint32> from = audioSsrcsOf(peer);
        const QList<quint32> to = audioSsrcsOf(m_replacement.get());
        const quint32 ssrc = qFromBigEndian<quint32>(current.constData() + 8);
        const qsizetype index = ssrc != 0 ? from.indexOf(ssrc) : -1;
        if (index >= 0 && to.value(index) != 0) {
            m_replacement->sendRtp(rewriteRtpSsrc(current, {{ssrc, to.value(index)}}));
        }
    }
    return accepted;
}

std::optional<IceConfiguration> DaemonMediaController::mediaIceConfiguration()
{
    if (!m_server) {
        return std::nullopt;
    }
    // Task 29 step 2b: a media start that declared the tunnel runs over it
    // (with its host candidates) while the session is on a WebSocket; on a
    // data channel (after a move) the session's own settings, as before.
    if (m_startTunnel) {
        if (auto tunnel = m_server->mediaTunnelIceConfiguration(m_epoch)) {
            return tunnel;
        }
    }
    auto ice = m_server->sessionIceConfiguration(m_epoch);
    if (ice) {
        ice->setMediaRouting(m_startRelayRouting);
    }
    return ice;
}

bool DaemonMediaController::handleReplace(const QJsonObject& control)
{
    if (!m_server || !m_server->mediaReplaceAvailable(m_epoch)) {
        return false;
    }
    // Direct media ladder: a device the Core told mediaDirectVersion 1 may
    // add "mediaDirectVersion": 1 to ask for a replacement without the
    // tunnel or a relay (STUN and host candidates only). Every other shape
    // stays the exact three fields.
    const bool direct = control.contains(QStringLiteral("mediaDirectVersion"));
    if (direct) {
        quint32 version = 0;
        if (!exactKeys(control, {"op", "connectionId", "replaces", "mediaDirectVersion"})
            || !m_server->mediaDirectAvailable(m_epoch)
            || !exactUnsigned(control.value(QStringLiteral("mediaDirectVersion")), version,
                              /*nonzero=*/true)
            || version != 1) {
            return false;
        }
    } else if (!exactKeys(control, {"op", "connectionId", "replaces"})) {
        return false;
    }
    if (!canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || !canonicalConnectionId(control.value(QStringLiteral("replaces")))) {
        return false;
    }
    const QString connectionId = control.value(QStringLiteral("connectionId")).toString();
    const QString replaces = control.value(QStringLiteral("replaces")).toString();
    const auto refuse = [this, &connectionId](const QString& reason) {
        sendRejected(connectionId, 0, 0, reason);
        return false;
    };
    if (!m_peer || m_peer->connectionId() != replaces || connectionId == replaces
        || !m_peer->isReady() || m_replacement) {
        return refuse(QStringLiteral(
            "The Core did not move audio and display: that connection is not the current one."));
    }
    std::optional<IceConfiguration> nextIce;
    if (direct) {
        nextIce = m_server->mediaDirectIceConfiguration();
    } else {
        nextIce = mediaIceConfiguration();
    }
    // An older relay leg (a connection made without media routing that runs
    // through the loopback shim) cannot move to a connection that does not
    // route media either. Judged by the connection in use, not by what the
    // media start declared: a tunnel start routes media over the tunnel.
    if (!m_currentRouted) {
        const auto path = m_peer->selectedPath();
        if (path && path->viaLoopbackShim() && (!nextIce || !nextIce->mediaRouting())) {
            return refuse(QStringLiteral(
                "This older relay media path cannot move while it is still in use."));
        }
    }
    if (!radioIdleForReplace()) {
        // The words of kReplaceTransmittingReason, which a device matches.
        return refuse(QStringLiteral(
            "The Core did not move audio and display: the radio is transmitting."));
    }
    // The old peer still draining from an earlier replacement goes now.
    if (m_retiring) {
        m_retireDrainTimer.stop();
        MediaPeer* retiring = m_retiring.release();
        retiring->disconnect(this);
        retiring->stop();
        retiring->deleteLater();
    }
    m_replacement = std::make_unique<MediaPeer>(this, m_peerFactory);
    m_replacementId = connectionId;
    MediaPeer* const peer = m_replacement.get();
    const quint64 peerEpoch = m_epoch;
    m_replacementReady = false;
    const auto current = [this, peer, peerEpoch] {
        return m_replacement.get() == peer && m_epoch == peerEpoch;
    };
    connect(peer, &MediaPeer::controlReady, this, [this, current](const QJsonObject& outbound) {
        if (current()) {
            sendControl(outbound);
        }
    });
    connect(peer, &MediaPeer::ready, this, [this, current] {
        if (current()) {
            onReplacementReady();
        }
    });
    connect(peer, &MediaPeer::connectionFailed, this, [this, current](const QString& message) {
        if (current()) {
            qCInfo(lcDaemonMedia).noquote()
                << QStringLiteral("replacement media connection failed: %1").arg(message.left(256));
            failReplacement(QString::fromLatin1(kMediaPeerLostReason));
        }
    });
    connect(peer, &MediaPeer::closed, this, [this, current] {
        if (current()) {
            failReplacement(QString::fromLatin1(kMediaPeerClosedReason));
        }
    });
    connect(peer, &MediaPeer::errorOccurred, this, [this, current](const QString& message) {
        if (current()) {
            qCInfo(lcDaemonMedia).noquote()
                << QStringLiteral("replacement media error: %1").arg(message.left(256));
            failReplacement(QString::fromLatin1(kMediaPeerLostReason));
        }
    });
    // Microphone packets and "tx" keepalives are taken from the new peer as
    // soon as it carries them.
    connect(peer, &MediaPeer::txReceived, this,
            [this, current, peerEpoch](const QByteArray& message, qint64 heldUs) {
        if (current() && m_server) {
            noteKeepaliveWait(heldUs);
            m_server->txChannelMessage(peerEpoch, message, heldUs);
        }
    });
    connect(peer, &MediaPeer::micRtpReceived, this, [this, current](const QByteArray& packet,
                                                                      qint64 heldUs) {
        if (current() && m_micReceiver) {
            m_micRoute->deliver(packet, heldUs);
        }
    });
    peer->setMicPacketSink(micRouteSink());
    m_replacementRouted = nextIce && nextIce->mediaRouting();
    peer->setIceConfiguration(nextIce);
    if (!peer->start(IMediaTransport::Role::Offerer, connectionId, m_audioTargetBitrate,
                     m_startOfferedLossless, m_receiverAudioNegotiated,
                     m_headphonesMixNegotiated, m_startMicLine, m_iqNegotiated)) {
        peer->disconnect(this);
        m_replacement.release()->deleteLater();
        sendRejected(connectionId, 0, 0,
                     QStringLiteral("The Core could not start audio and display."));
        return false;
    }
    m_replaceConnectTimer.start(IceConfiguration::kConnectDeadlineMs);
    qCInfo(lcDaemonMedia) << "media replacement started";
    return true;
}

void DaemonMediaController::onReplacementReady()
{
    m_replaceConnectTimer.stop();
    m_replacementReady = true;
    // The new peer's microphone packets reach the receiver as its own.
    if (m_micReceiver && m_replacement && m_replacement->micAudioSsrc() != 0) {
        m_micSsrcRewrite.insert(m_replacement->micAudioSsrc(), m_micReceiver->ssrc());
        syncMicRoute();
    }
    m_replaceOverlapTimer.start(kReplaceOverlapMs);
    qCInfo(lcDaemonMedia) << "media replacement ready; audio on both connections";
}

void DaemonMediaController::finishReplacement()
{
    if (!m_replacement || !m_replacementReady || !m_peer) {
        return;
    }
    // Nothing moves while the radio is on the air: both peers keep
    // carrying until it is idle.
    if (!radioIdleForReplace()) {
        m_replaceOverlapTimer.start(kReplaceOverlapMs);
        return;
    }
    const QString replaces = m_peer->connectionId();
    // Every packet a sender stamps from now on goes to the new peer under
    // its own SSRC: the earlier peers' stamps, and the retiring peer's.
    const QList<quint32> from = audioSsrcsOf(m_peer.get());
    const QList<quint32> to = audioSsrcsOf(m_replacement.get());
    QHash<quint32, quint32> rewrite;
    for (auto it = m_sendSsrcRewrite.cbegin(); it != m_sendSsrcRewrite.cend(); ++it) {
        const qsizetype index = from.indexOf(it.value());
        if (index >= 0 && to.value(index) != 0) {
            rewrite.insert(it.key(), to.value(index));
        }
    }
    for (qsizetype i = 0; i < from.size(); ++i) {
        if (from.at(i) != 0 && to.value(i) != 0) {
            rewrite.insert(from.at(i), to.value(i));
        }
    }
    m_sendSsrcRewrite = rewrite;
    // The old peer stops sending; its microphone packets and keepalives are
    // still taken while it drains.
    std::unique_ptr<MediaPeer> old = std::move(m_peer);
    old->disconnect(this);
    MediaPeer* const oldPeer = old.get();
    const quint64 peerEpoch = m_epoch;
    connect(oldPeer, &MediaPeer::txReceived, this,
            [this, oldPeer, peerEpoch](const QByteArray& message, qint64 heldUs) {
        if (m_retiring.get() == oldPeer && m_epoch == peerEpoch && m_server) {
            noteKeepaliveWait(heldUs);
            m_server->txChannelMessage(peerEpoch, message, heldUs);
        }
    });
    connect(oldPeer, &MediaPeer::micRtpReceived, this, [this, oldPeer, peerEpoch](const QByteArray& packet,
                                                                         qint64 heldUs) {
        if (m_retiring.get() == oldPeer && m_epoch == peerEpoch && m_micReceiver) {
            m_micRoute->deliver(packet, heldUs);
        }
    });
    m_retiring = std::move(old);
    m_retireDrainTimer.start(kReplaceDrainMs);

    m_replaceOverlapTimer.stop();
    m_replacement->disconnect(this);
    m_peer = std::move(m_replacement);
    m_replacementId.clear();
    m_replacementReady = false;
    m_currentRouted = std::exchange(m_replacementRouted, false);
    MediaPeer* const peer = m_peer.get();
    const QString connectionId = peer->connectionId();
    wireCurrentPeer(peer, connectionId);
    // Control logging lane: the new connection's pair, as a change.
    logMediaPath();
    // Displays go on over the new peer, each starting on a keyframe.
    for (auto& [endpointId, entry] : m_endpoints) {
        Q_UNUSED(endpointId);
        entry.forceKeyframe = true;
    }
    m_displayDiagnostics = {};
    m_displayDiagnosticsLogged = {};
    m_loggedTransportErrorKinds.clear();
    sendControl({{QStringLiteral("op"), QStringLiteral("replace")},
                 {QStringLiteral("connectionId"), connectionId},
                 {QStringLiteral("replaces"), replaces}});
    reconcileWantedIq();
    qCInfo(lcDaemonMedia) << "media moved to its new connection";
}

void DaemonMediaController::failReplacement(const QString& reason)
{
    m_replaceConnectTimer.stop();
    m_replaceOverlapTimer.stop();
    if (!m_replacement) {
        return;
    }
    // The id it started with: a peer that closed has cleared its own.
    const QString connectionId = std::exchange(m_replacementId, QString());
    MediaPeer* const peer = m_replacement.release();
    m_replacementReady = false;
    if (m_micReceiver) {
        m_micSsrcRewrite.remove(peer->micAudioSsrc());
        syncMicRoute();
    }
    peer->disconnect(this);
    peer->stop();
    peer->deleteLater();
    qCInfo(lcDaemonMedia) << "media replacement dropped; the current connection carries on";
    // The id the replacement started with (its own may be cleared by now).
    if (!connectionId.isEmpty()) {
        sendRejected(connectionId, 0, 0, reason);
    }
}

void DaemonMediaController::clearReplacement()
{
    m_replaceConnectTimer.stop();
    m_replaceOverlapTimer.stop();
    m_retireDrainTimer.stop();
    m_replacementReady = false;
    m_replacementId.clear();
    for (std::unique_ptr<MediaPeer>* slot : {&m_replacement, &m_retiring}) {
        if (*slot) {
            MediaPeer* const peer = slot->release();
            peer->disconnect(this);
            peer->stop();
            peer->deleteLater();
        }
    }
}

// ---- iPhone app plan Task 36 (R-IOS-13): the microphone line ---------------

void DaemonMediaController::startMicLine(MediaPeer* peer)
{
    stopMicLine();
    if (!m_radioModel || m_radioModel->remoteMicFeed() == nullptr || !m_server) {
        return;
    }
    const MonotonicClock clock = m_monotonicClock;
    RemoteMicReceiver::Clock ms;
    if (clock) {
        ms = [clock]() { return clock() / 1'000'000; };
    }
    m_micReceiver = std::make_unique<RemoteMicReceiver>(m_radioModel->remoteMicFeed(), nullptr, ms);
    if (!m_micReceiver->start(peer->micAudioSsrc(), peer->micLosslessNegotiated())) {
        m_micReceiver.reset();
        return;
    }
    m_micDeviceId = m_server->mediaSessionDevice(m_epoch);
    if (m_micDeviceId.isEmpty()) {
        m_micReceiver.reset();
        return;
    }
    // Task 37 (remote design section 12.3): starvation while the device is
    // keyed on its line takes the Core's per-mode action.
    connect(m_micReceiver.get(), &RemoteMicReceiver::starved, this, [this](bool starved) {
        if (m_server && !m_micDeviceId.isEmpty()) {
            m_server->remoteMicStarved(m_micDeviceId, starved);
        }
    });
    // Fix wave C2: this device's line, by device; it writes the feed only
    // while it is the writer.
    m_micReceiver->setFeedWriter(false);
    m_radioModel->openRemoteMicLine(m_micDeviceId);
    m_micReceiver->setFeedWriter(m_radioModel->remoteMicWriter() == m_micDeviceId);
    refreshMicVoxArmed();
    refreshMicWatching();
    syncMicRoute();
    qCInfo(lcDaemonMedia) << "Microphone line open for" << m_micDeviceId.toHex();
}

void DaemonMediaController::stopMicLine()
{
    if (!m_micReceiver && m_micDeviceId.isEmpty()) {
        return;
    }
    // TX mic thread: out of the route first, so no packet in delivery on
    // the line's thread reaches it after this.
    {
        const std::lock_guard<std::mutex> lock(m_micRoute->mutex);
        m_micRoute->receiver = nullptr;
    }
    if (m_micReceiver) {
        m_micReceiver->stop();
    }
    m_micReceiver.reset();
    const QByteArray device = std::exchange(m_micDeviceId, QByteArray());
    if (m_radioModel && !device.isEmpty()) {
        // Fix wave C2: only this device's line closes (its priming and VOX
        // with its last line); every other device's stays as it is.
        m_radioModel->closeRemoteMicLine(device);
    }
}

void DaemonMediaController::refreshMicVoxArmed()
{
    if (!m_radioModel || !m_server || !m_micReceiver) {
        return;
    }
    // VOX armed for the device: the operator's VOX is on and its session
    // may transmit now (transmit unheld, or held by it). Its client then
    // streams the microphone unkeyed and VOX listens to it. Fix wave C2:
    // VOX another device armed is that device's, never this one's. Fix
    // wave 2: VOX nobody armed from a device (turned on at the Core
    // itself) listens to no device's line either.
    const QByteArray armer = m_server->voxArmedBy();
    m_radioModel->setRemoteMicVoxArmed(
        m_micDeviceId, m_radioModel->transmitModel().voxEnabled()
                           && m_server->mediaSessionTxPermitted(m_epoch)
                           && !armer.isEmpty() && armer == m_micDeviceId);
}

void DaemonMediaController::refreshMicWatching()
{
    if (!m_micReceiver || !m_radioModel) {
        return;
    }
    // Starvation is watched while the device is keyed on its line.
    const MoxController* mox = m_radioModel->moxController();
    const bool keyed = mox != nullptr && mox->isMox() && m_radioModel->remoteMicInUse()
        && m_radioModel->keyedBy().deviceId == m_micDeviceId;
    m_micReceiver->setWatching(keyed);
}

void DaemonMediaController::snapshotUnkeyStats()
{
    // Only the controller whose device holds the key on its microphone
    // line reports (refreshMicWatching's test, taken here before the walk
    // clears the keyer), so an unkey gives one line, under the keyer's id,
    // and a key that is not on a remote line gives none.
    if (!m_micReceiver || !m_radioModel || !m_radioModel->remoteMicInUse()
        || m_radioModel->keyedBy().deviceId != m_micDeviceId) {
        m_unkeySnapshot.reset();
        return;
    }
    UnkeySnapshot snapshot;
    snapshot.deviceId = m_micDeviceId;
    snapshot.rx = m_micReceiver->stats();
    snapshot.keepaliveWaitMaxUs = m_overKeepaliveWaitMaxUs;
    snapshot.haveFeed = m_radioModel->remoteMicFeed() != nullptr;
    if (snapshot.haveFeed) {
        snapshot.feed = m_radioModel->remoteMicFeed()->stats();
    }
    m_unkeySnapshot = std::move(snapshot);
}

void DaemonMediaController::logUnkeyStats()
{
    if (!m_unkeySnapshot || !m_radioModel) {
        return;
    }
    const UnkeySnapshot snapshot = std::move(*m_unkeySnapshot);
    m_unkeySnapshot.reset();
    RadioConnection::TxSendStats send;
    if (const RadioConnection* conn = m_radioModel->connection()) {
        send = conn->txSendStats();
    }
    qCInfo(lcDaemonMedia).noquote()
        << unkeyStatsLine(snapshot.deviceId.toHex(), snapshot.rx,
                          snapshot.haveFeed ? &snapshot.feed : nullptr, send,
                          snapshot.keepaliveWaitMaxUs);
    // TX diagnostics lane: the over's dropouts placed in time, a line each.
    const QStringList events = unkeyEventLines(
        snapshot.deviceId.toHex(), snapshot.haveFeed ? &snapshot.feed : nullptr, send);
    for (const QString& event : events) {
        qCInfo(lcDaemonMedia).noquote() << event;
    }
}

void DaemonMediaController::noteKeepaliveWait(qint64 heldUs)
{
    m_overKeepaliveWaitMaxUs = std::max(m_overKeepaliveWaitMaxUs, std::max<qint64>(0, heldUs));
}

QString DaemonMediaController::unkeyStatsLine(const QByteArray& deviceId,
                                              const RemoteMicReceiver::Stats& rx,
                                              const RemoteMicFeed::Stats* feedStats,
                                              const RadioConnection::TxSendStats& send,
                                              qint64 keepaliveWaitMaxUs)
{
    // One line (log only, never shown to a device).
    QString text;
    {
        QDebug line(&text);
        line.nospace().noquote();
        line << "Transmit ended (" << QString::fromLatin1(deviceId) << "): microphone ";
        if (feedStats != nullptr) {
            const RemoteMicFeed::Stats& feed = *feedStats;
            // R-IOS-13 (2026-09-27): what the path added to the microphone
            // over the over (this buffer plus the send ring), the ring
            // alone, and the silence shed or inserted to keep it minimal.
            const auto ms = [](quint64 frames) {
                return QString::number(static_cast<double>(frames)
                                           / RemoteMicConfig::kFramesPerMs,
                                       'f', 1);
            };
            const auto oneDecimal = [](double v) { return QString::number(v, 'f', 1); };
            line << "depth " << (feed.fillFrames * 1000 / RemoteMicConfig::kSampleRate)
                 << " ms, target " << (feed.targetFrames / RemoteMicConfig::kFramesPerMs)
                 << " ms, underruns " << feed.underflows << ", overflows " << feed.overflows
                 << ", dropped " << feed.droppedFrames << " frames; added latency mean "
                 << oneDecimal(feed.addedMeanMs) << " ms, max " << oneDecimal(feed.addedMaxMs)
                 << " ms; send ring ";
            if (feed.ringMeanMs >= 0.0) {
                line << "mean " << oneDecimal(feed.ringMeanMs) << " ms, max "
                     << oneDecimal(feed.ringMaxMs) << " ms";
            } else {
                line << "unknown";
            }
            line << "; shed " << ms(feed.shedFrames + feed.shedForRingFrames) << " ms ("
                 << ms(feed.shedForRingFrames) << " ms for the ring), inserted "
                 << ms(feed.insertedFrames) << " ms, target grew " << feed.grows
                 << " times, held for DEXP " << feed.heldBlocks << " blocks";
            // TX stall lane: how long the over's packets waited at the Core
            // before the feed had them. Measured only. TX mic thread fix
            // round 2: the line's own thread hands them over now, so this
            // is the line's wait, not the event loop's.
            line << "; line waits mean " << oneDecimal(feed.ownerWaitMeanMs) << " ms, max "
                 << oneDecimal(feed.ownerWaitMaxMs) << " ms, " << feed.ownerWaitsLong
                 << " over " << RemoteMicConfig::kLongOwnerWaitMs << " ms";
        } else {
            line << "no feed";
        }
        // TX mic thread fix round 2: the event loop's own stall, as the
        // longest wait at the Core of a "tx" keepalive over the over.
        line << "; keepalive waits ";
        if (keepaliveWaitMaxUs >= 0) {
            line << "max " << QString::number(static_cast<double>(keepaliveWaitMaxUs) / 1000.0, 'f', 1)
                 << " ms";
        } else {
            line << "none";
        }
        line << "; packets concealed " << rx.concealedPackets << ", recovered "
             << rx.recoveredPackets << ", late " << rx.latePackets << ", long gaps " << rx.longGaps
             << "; transmit I/Q ";
        if (send.valid && send.overflowOnly) {
            line << "lost " << send.overflowSamples << " samples (no other counters)";
        } else if (send.valid) {
            line << "frames " << send.framesSent << ", silence " << send.zeroPaddedSamples
                 << " samples";
            // TX diagnostics lane: where the silence fell.
            if (send.placed) {
                line << " (start " << send.padStartSamples << ", mid-key " << send.padMidSamples
                     << ", unkey tail " << send.padTailSamples;
                if (send.padTailSamples > 0 && send.padTailAtMs >= 0.0) {
                    line << " from +" << QString::number(send.padTailAtMs, 'f', 1) << " ms";
                }
                line << ")";
            }
            line << ", late wakes " << send.lateWakes << ", catch-up bursts "
                 << send.catchUpBursts << ", radio ran dry " << send.radioRanDry << ", lost "
                 << send.overflowSamples << " samples, send errors " << send.sendErrors
                 << ", deepest queue " << send.maxRingMs << " ms";
            if (send.placed) {
                const auto oneDecimal = [](double v) { return QString::number(v, 'f', 1); };
                if (send.firstBlockAtMs >= 0.0) {
                    line << ", first I/Q block at +" << oneDecimal(send.firstBlockAtMs)
                         << " ms of the key";
                } else {
                    line << ", no I/Q block came";
                }
                if (send.longestMidPadSamples > 0) {
                    line << ", longest mid-key silence "
                         << oneDecimal(static_cast<double>(send.longestMidPadSamples) / 192.0)
                         << " ms at +" << oneDecimal(send.longestMidPadAtMs) << " ms";
                }
                // The TX pump's longest wait for the radio's microphone
                // block, and the radio's frame sequence step across it.
                if (send.longestWakeGapMs >= 0.0) {
                    const double at = send.longestWakeGapAtMs;
                    line << ", longest wait for a microphone block "
                         << oneDecimal(send.longestWakeGapMs) << " ms at "
                         << (at >= 0.0 ? "+" : "") << oneDecimal(at) << " ms of the key, ";
                    if (send.wakeGapSequenceStep >= 0) {
                        line << "radio frame sequence step " << send.wakeGapSequenceStep;
                    } else {
                        line << "no radio frame sequence seen";
                    }
                } else {
                    line << ", no wait for a microphone block measured";
                }
            }
        } else {
            line << "no counters";
        }
    }
    return text;
}

QStringList DaemonMediaController::unkeyEventLines(const QByteArray& deviceId,
                                                   const RemoteMicFeed::Stats* feed,
                                                   const RadioConnection::TxSendStats& send)
{
    // TX diagnostics lane: one line an event, bounded by what the feed and
    // the send path place (4 underruns, 1 ran dry, 4 bursts). Log only.
    QStringList lines;
    const QString prefix =
        QStringLiteral("Transmit ended (%1): ").arg(QString::fromLatin1(deviceId));
    const auto oneDecimal = [](double v) { return QString::number(v, 'f', 1); };
    // RF started when the TX channel's first I/Q block went out, on the
    // steady clock the microphone's underruns are stamped with.
    const bool haveRf = send.valid && send.placed && send.keySteadyNs >= 0
        && send.firstBlockAtMs >= 0.0;
    const double rfSteadyMs = haveRf
        ? static_cast<double>(send.keySteadyNs) / 1.0e6 + send.firstBlockAtMs
        : 0.0;
    if (feed != nullptr) {
        for (int k = 0; k < feed->underrunsPlacedCount; ++k) {
            const RemoteMicFeed::Stats::Underrun& event =
                feed->underrunsPlaced[static_cast<size_t>(k)];
            QString text = prefix
                + QStringLiteral("microphone underrun %1 at +%2 ms of the line")
                      .arg(k + 1)
                      .arg(oneDecimal(event.atLineMs));
            if (!send.valid || !send.placed) {
                text += QStringLiteral(", RF start not measured");
            } else if (!haveRf) {
                text += QStringLiteral(", RF never started");
            } else if (event.atSteadyUs >= 0) {
                const double sinceRf = static_cast<double>(event.atSteadyUs) / 1000.0 - rfSteadyMs;
                text += sinceRf >= 0.0
                    ? QStringLiteral(" and +%1 ms of RF").arg(oneDecimal(sinceRf))
                    : QStringLiteral(", %1 ms before RF started").arg(oneDecimal(-sinceRf));
            }
            text += event.silentMs >= 0.0
                ? QStringLiteral(", silent %1 ms").arg(oneDecimal(event.silentMs))
                : QStringLiteral(", still silent at unkey");
            text += event.arrivalGapMs >= 0.0
                ? QStringLiteral("; largest gap between packets %1 ms")
                      .arg(oneDecimal(event.arrivalGapMs))
                : QStringLiteral("; no gap between packets measured");
            text += event.lateMs >= 0.0
                ? QStringLiteral(", latest packet %1 ms behind its timestamp")
                      .arg(oneDecimal(event.lateMs))
                : QStringLiteral(", no packet timestamps measured");
            lines << text;
        }
    }
    if (send.valid && send.placed) {
        if (send.firstDryAtMs >= 0.0) {
            lines << prefix
                    + QStringLiteral("radio ran dry at +%1 ms of the key, after a send gap of %2 ms")
                          .arg(oneDecimal(send.firstDryAtMs), oneDecimal(send.firstDryGapMs));
        }
        const int bursts = std::clamp(send.burstEvents, 0,
                                      RadioConnection::TxSendStats::kMaxBurstEvents);
        for (int k = 0; k < bursts; ++k) {
            const RadioConnection::TxSendStats::Burst& burst =
                send.bursts[static_cast<size_t>(k)];
            lines << prefix
                    + QStringLiteral("catch-up burst %1 at +%2 ms of the key, %3 frames after a "
                                     "send gap of %4 ms")
                          .arg(k + 1)
                          .arg(oneDecimal(burst.atMs))
                          .arg(burst.frames)
                          .arg(oneDecimal(burst.gapMs));
        }
    }
    return lines;
}

bool DaemonMediaController::carriesMicFor(const QByteArray& deviceId) const
{
    return m_micReceiver && m_micReceiver->isRunning() && !deviceId.isEmpty()
        && deviceId == m_micDeviceId;
}

void DaemonMediaController::primeMic(std::function<void(bool)> done)
{
    if (!m_micReceiver || !m_radioModel || m_micDeviceId.isEmpty()) {
        done(false);
        return;
    }
    // The ring is the source while the key waits (this line its writer),
    // so the buffer fills.
    m_radioModel->setRemoteMicPriming(m_micDeviceId, true);
    m_micReceiver->awaitReady(std::move(done));
}

void DaemonMediaController::endMicPriming()
{
    if (m_micReceiver) {
        m_micReceiver->cancelWait();
    }
    if (m_radioModel && !m_micDeviceId.isEmpty()) {
        m_radioModel->setRemoteMicPriming(m_micDeviceId, false);
    }
}

RemoteKeying::MicUplink DaemonMediaController::micUplink()
{
    RemoteKeying::MicUplink uplink;
    const QPointer<DaemonMediaController> self(this);
    uplink.carriesMic = [self](const QByteArray& deviceId) {
        return self && self->carriesMicFor(deviceId);
    };
    uplink.prime = [self](const QByteArray& deviceId, std::function<void(bool)> done) {
        if (!self || !self->carriesMicFor(deviceId)) {
            done(false);
            return;
        }
        self->primeMic(std::move(done));
    };
    uplink.endPriming = [self](const QByteArray& deviceId) {
        if (self && deviceId == self->m_micDeviceId) {
            self->endMicPriming();
        }
    };
    return uplink;
}

bool DaemonMediaController::acceptPeerControl(const QJsonObject& control)
{
    // iPhone app plan Task 29: a replacement's own description and
    // candidates, under its new connection id.
    if (m_replacement && canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        && control.value(QStringLiteral("connectionId")).toString() == m_replacementId) {
        return m_replacement->acceptControl(control);
    }
    if (!m_peer || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !m_peer->acceptControl(control)) {
        return false;
    }
    return true;
}

bool DaemonMediaController::handleSubscribe(const QJsonObject& control)
{
    const bool revisioned = displayBudgetWireAvailable();
    quint32 endpointId = 0;
    quint32 revision = 0;
    const bool validIdentity = m_peer
        && canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        && control.value(QStringLiteral("connectionId")).toString() == m_peer->connectionId()
        && exactUnsigned(control.value(QStringLiteral("endpointId")), endpointId, true)
        && exactUnsigned(control.value(QStringLiteral("revision")), revision, true);
    const bool widebandNegotiated = control.contains(QStringLiteral("extendedView"));
    QJsonObject legacyShape = control;
    if (widebandNegotiated) { legacyShape.remove(QStringLiteral("extendedView")); }
    // Parity Task 17 (R-R3-01): `decimation`, only from a peer the Core told
    // spectrumGrantVersion 2 (it tells every peer at the grant minor).
    const bool decimationPresent = control.contains(QStringLiteral("decimation"));
    int decimation = 1;
    if (decimationPresent) { legacyShape.remove(QStringLiteral("decimation")); }
    // iPhone app Task 20 (R-IOS-27): the display extras fields, only from a
    // peer the Core told displayExtrasVersion 1 (display extras v1).
    bool extrasPresent = false;
    for (const QString& key : displayExtrasSubscribeKeys()) {
        if (legacyShape.contains(key)) {
            extrasPresent = true;
            legacyShape.remove(key);
        }
    }
    // Parity Task 28 (R-R3-49, A11): the transmit window, only from a peer
    // whose start declared txDisplayVersion, and both edges or neither.
    const bool txWindowPresent = control.contains(QStringLiteral("txMinDbm"))
        || control.contains(QStringLiteral("txMaxDbm"));
    if (txWindowPresent) {
        legacyShape.remove(QStringLiteral("txMinDbm"));
        legacyShape.remove(QStringLiteral("txMaxDbm"));
    }
    // Parity Task 31 (R-R3-49, A11): `duplex`, a boolean, only from a peer
    // whose start declared txDisplayVersion 3 or more.
    const bool duplexPresent = control.contains(QStringLiteral("duplex"));
    if (duplexPresent) {
        legacyShape.remove(QStringLiteral("duplex"));
    }
    const bool rolePresent = control.contains(QStringLiteral("displayRole"));
    if (rolePresent) {
        legacyShape.remove(QStringLiteral("displayRole"));
    }
    DisplayExtrasRequest extrasRequest;
    if ((rolePresent && (!m_miniDisplayNegotiated
                         || control.value(QStringLiteral("displayRole")).toString()
                                != QLatin1String("mini")))
        || (duplexPresent && (!m_txDisplayNegotiated || m_txDisplayDeclared < 3
                           || !control.value(QStringLiteral("duplex")).isBool()))
        || (txWindowPresent && (!m_txDisplayNegotiated
                             || !control.contains(QStringLiteral("txMinDbm"))
                             || !control.contains(QStringLiteral("txMaxDbm"))))
        || (widebandNegotiated && (!m_server || !m_server->remoteWidebandAvailable(m_epoch)
                               || !control.value(QStringLiteral("extendedView")).isBool()))
        || (extrasPresent && (!m_server || !m_server->displayExtrasAvailable(m_epoch)
                              || !parseDisplayExtrasRequest(control, extrasRequest)))
        || (decimationPresent && (!m_server || !m_server->spectrumGrantAvailable(m_epoch)))
        || !exactKeys(legacyShape, {"op", "connectionId", "endpointId", "revision", "sliceId",
                             "tier", "fftSize", "windowType", "centreHz", "spanHz", "pixels",
                             "fps", "framesPerLine", "trace", "waterfall", "minDbm", "maxDbm",
                             "wideSpanFactor"})
        || !validIdentity) {
        if (revisioned && validIdentity) {
            const bool remember = m_endpoints.contains(endpointId)
                || m_nonliveOperations.contains(endpointId)
                || endpointId > m_endpointHighWater;
            m_endpointHighWater = std::max(m_endpointHighWater, endpointId);
            return rejectAllocation(control, endpointId, revision,
                                    QStringLiteral("The Core could not read this display request."), remember);
        }
        return false;
    }

    if (revisioned) {
        auto active = m_endpoints.find(endpointId);
        if (active != m_endpoints.end()) {
            const AllocationRecord& prior = active->second.allocation;
            if (revision == prior.revision && control == prior.request) {
                const bool accepted = prior.accepted;
                sendAllocationResult(m_peer->connectionId(), endpointId, revision,
                                     accepted, prior.reason);
                return accepted;
            }
            if (staleOrEqualRevision(revision, prior.revision)) {
                return rejectAllocation(control, endpointId, revision,
                                        QStringLiteral("A newer request for this display had already arrived."), false);
            }
        } else {
            auto recent = m_nonliveOperations.find(endpointId);
            if (recent != m_nonliveOperations.end()) {
                const AllocationRecord prior = recent->second;
                if (revision == prior.revision && control == prior.request) {
                    sendAllocationResult(m_peer->connectionId(), endpointId, revision,
                                         prior.accepted, prior.reason);
                    return prior.accepted;
                }
                if (prior.explicitlyRetired
                    || staleOrEqualRevision(revision, prior.revision)) {
                    return rejectAllocation(control, endpointId, revision,
                                            prior.explicitlyRetired
                                                ? QStringLiteral("This display had already been closed.")
                                                : QStringLiteral("A newer request for this display had already arrived."),
                                            false);
                }
            } else if (endpointId <= m_endpointHighWater) {
                return rejectAllocation(control, endpointId, revision,
                                        QStringLiteral("This display had already been closed."), false);
            }
        }
        m_endpointHighWater = std::max(m_endpointHighWater, endpointId);
    }

    int sliceId = -1;
    int fftSize = 0;
    int windowType = 0;
    int pixels = 0;
    int fps = 0;
    int framesPerLine = 0;
    double minDbm = 0.0;
    double maxDbm = 0.0;
    double txMinDbm = 0.0;
    double txMaxDbm = 0.0;
    FftTier tier = FftTier::Wide;
    SpectrumEndpointRequest request;
    if (!exactInt(control.value(QStringLiteral("sliceId")), 0, std::numeric_limits<int>::max(), sliceId)
        || !parseTier(control.value(QStringLiteral("tier")), tier)
        || !exactInt(control.value(QStringLiteral("fftSize")), 1024,
                     std::numeric_limits<int>::max(), fftSize)
        || !powerOfTwoFftSize(fftSize)
        || (fftSize <= FFTEngine::maximumFftSize() && !supportedFftSize(fftSize))
        || !exactInt(control.value(QStringLiteral("windowType")),
                     static_cast<int>(WindowFunction::Rectangular),
                     static_cast<int>(WindowFunction::Count) - 1, windowType)
        || !finiteNumber(control.value(QStringLiteral("centreHz")), request.centreHz)
        || !finiteNumber(control.value(QStringLiteral("spanHz")), request.spanHz)
        || !exactInt(control.value(QStringLiteral("pixels")), 1, SpectrumEndpoint::kMaxPixels, pixels)
        || !exactInt(control.value(QStringLiteral("fps")), 1, 60, fps)
        || !exactInt(control.value(QStringLiteral("framesPerLine")), 1, kMaxFramesPerLine,
                     framesPerLine)
        || !parsePlane(control.value(QStringLiteral("trace")), request.trace)
        || !parsePlane(control.value(QStringLiteral("waterfall")), request.waterfall)
        || !finiteNumber(control.value(QStringLiteral("minDbm")), minDbm)
        || !finiteNumber(control.value(QStringLiteral("maxDbm")), maxDbm)
        || !finiteNumber(control.value(QStringLiteral("wideSpanFactor")), request.requestedWideSpanFactor)
        || (decimationPresent
            && !exactInt(control.value(QStringLiteral("decimation")),
                         ControlRanges::kDisplayDecimationMin,
                         ControlRanges::kDisplayDecimationMax, decimation))
        || minDbm < kMinDbmLimit || minDbm > kMaxDbmLimit
        || maxDbm < kMinDbmLimit || maxDbm > kMaxDbmLimit
        || maxDbm <= minDbm
        // Parity Task 28: the transmit window by the receive window's rule.
        || (txWindowPresent
            && (!finiteNumber(control.value(QStringLiteral("txMinDbm")), txMinDbm)
                || !finiteNumber(control.value(QStringLiteral("txMaxDbm")), txMaxDbm)
                || txMinDbm < kMinDbmLimit || txMinDbm > kMaxDbmLimit
                || txMaxDbm < kMinDbmLimit || txMaxDbm > kMaxDbmLimit
                || txMaxDbm <= txMinDbm))
        || !validRequestedFrequencyRange(request.centreHz, request.spanHz,
                                         request.requestedWideSpanFactor)) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The Core could not read this display request."));
    }
    SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
    if (!slice || slice->streamIndex() < 0 || !m_radioModel->streamActive(slice->streamIndex())) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("This display's receiver is not on the Core."));
    }
    // Task 76 (ruling 9.1): a device subscribes displays only for its own
    // slices; its pans ride their receivers, shared or not.
    if (!seesSlice(sliceId)) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("That slice belongs to another device."));
    }
    const MediaSourceKey source{slice->streamIndex(), tier};
    const double sourceCentreHz = m_radioModel->streamCentreHz(source.streamIndex);
    const double sourceSampleRateHz = m_radioModel->streamSampleRateHz(source.streamIndex);
    const double sourceHalfRate = sourceSampleRateHz * 0.5;
    if (!std::isfinite(sourceCentreHz) || !std::isfinite(sourceSampleRateHz)
        || sourceSampleRateHz <= 0.0 || !std::isfinite(sourceHalfRate)
        || !std::isfinite(sourceCentreHz - sourceHalfRate)
        || !std::isfinite(sourceCentreHz + sourceHalfRate)) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The receiver's spectrum is not ready."));
    }
    auto existing = m_endpoints.find(endpointId);
    if (existing != m_endpoints.end()
        && staleOrEqualRevision(revision, existing->second.revision)) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("A newer request for this display had already arrived."), false);
    }
    if (existing == m_endpoints.end() && m_endpoints.size() >= kMaxEndpoints) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The Core is already sending as many displays as it can."));
    }
    // Task 76: the engine is shared with every device watching this
    // receiver, so its window is everyone's.
    bool windowConflict = false;
    forEachSharedEndpoint(source, [&](DaemonMediaController& owner, quint32 otherId,
                                      EndpointEntry& other) {
        if ((&owner != this || otherId != endpointId) && other.sourceWindowType != windowType) {
            windowConflict = true;
        }
    });
    if (windowConflict) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The receiver's spectrum settings changed first."));
    }

    request.extendedView = widebandNegotiated
        && control.value(QStringLiteral("extendedView")).toBool();
    // The Core computes the averaging constants from the times the app
    // asked for, at this endpoint's frame rate, as the desktop does from its
    // own (DisplayFollowers: Thetis AvTau / AvTauWF). averageTimeMs is the
    // spectrum's, and the waterfall's too unless waterfallAverageTimeMs is
    // present.
    if (extrasRequest.averageTimeMs) {
        request.trace.averageAlpha = averageAlphaForTimeMs(*extrasRequest.averageTimeMs, fps);
    }
    const std::optional<int> waterfallTimeMs = extrasRequest.waterfallAverageTimeMs
        ? extrasRequest.waterfallAverageTimeMs : extrasRequest.averageTimeMs;
    if (waterfallTimeMs) {
        request.waterfall.averageAlpha = averageAlphaForTimeMs(*waterfallTimeMs, fps);
    }
    request.endpointId = endpointId;
    request.source = source;
    request.pixels = pixels;
    request.targetFps = fps;
    request.framesPerLine = framesPerLine;
    request.minDbm = static_cast<float>(minDbm);
    request.maxDbm = static_cast<float>(maxDbm);
    const auto adcRate = m_radioModel->widebandAdcRateHz(m_radioModel->sliceAdcIndex(sliceId));
    const bool extendedAllowed = request.extendedView && adcRate.has_value();
    if ((!requestOverlapsSource(request, sourceCentreHz, sourceSampleRateHz) && !extendedAllowed)
        || (extendedAllowed && request.spanHz > std::max(sourceSampleRateHz, *adcRate / 2.0))) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("This view reaches past what the receiver covers."));
    }

    // R-R3-01/R-R3-08: a request may size its (stream, tier) engine only
    // while it is that engine's only subscriber. Otherwise it is granted
    // the engine's current size, so no pan's spectrum changes to satisfy
    // another pan's request. Sizes above the engine limit get the largest.
    SpectrumGrant grant;
    grant.requestedFftSize = fftSize;
    grant.grantedTier = tier;
    grant.requestedPixels = pixels;
    int sharedFftSize = 0;
    // Parity Task 17: the engine's decimation follows the same rule as its
    // size: a request sets it only while it is the engine's only
    // subscriber; beside another pan it runs at the engine's decimation.
    int sharedDecimation = 0;
    // Task 76 (ruling 9.1): another device's pan on this receiver shares
    // the engine too.
    forEachSharedEndpoint(source, [&](DaemonMediaController& owner, quint32 otherId,
                                      EndpointEntry& other) {
        if (&owner != this || otherId != endpointId) {
            sharedFftSize = std::max(sharedFftSize, other.sourceFftSize);
            sharedDecimation = std::max(sharedDecimation, other.sourceDecimation);
        }
    });
    grant.grantedFftSize = sharedFftSize > 0
        ? sharedFftSize : std::min(fftSize, FFTEngine::maximumFftSize());
    grant.requestedDecimation = decimation;
    grant.grantedDecimation = sharedDecimation > 0 ? sharedDecimation : decimation;
    // The pixel grant is fixed here, where the source geometry and granted
    // FFT size are known, and the display budget charges what is granted.
    const bool extendedActive = widebandNegotiated && request.extendedView
        && extendedAllowed && needsWideband(request, sourceCentreHz, sourceSampleRateHz);
    grant.grantedPixels = SpectrumEndpoint::grantedPixels(
        request, grant.grantedFftSize, sourceCentreHz, sourceSampleRateHz, extendedActive);
    if (grant.grantedPixels <= 0) {
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("This view reaches past what the receiver covers."));
    }
    grant.reason = grantReason(grant);
    // A GUI that negotiated the grant accepts a charge no larger than it
    // asked for, so the budget holds what is granted. An older GUI requires
    // the charge for exactly its requested pixels; it is held to that, which
    // still covers every sample the endpoint emits.
    const int chargedPixels = m_server && m_server->spectrumGrantAvailable(m_epoch)
        ? grant.grantedPixels : pixels;
    const auto displayCost = endpointDisplayCost(
        chargedPixels, fps, request.requestedWideSpanFactor > 1.0, extrasRequest.sections());
    // Fix wave I5 (ruling 9.3): the device's request is what it asks for,
    // before the budget (fix wave 3: at the pixels its window can carry,
    // below). It is recorded first,
    // so the shares are split again with it and this display is admitted
    // against the share the device has with its new demand (for a holder,
    // what rule 1 gives it), not the share it had. A display refused for
    // the budget stays in the demand until it is closed or asked again.
    // Fix wave 2 (Important 2): asking for this endpoint again ends a
    // refused request's hold; what it asked for before that refusal is
    // what this ask replaces.
    std::optional<DisplayBudgetCharge> previousDemand =
        m_displayDemand.count(endpointId) != 0 ? std::optional(m_displayDemand.at(endpointId))
                                               : std::nullopt;
    if (const auto refused = m_refusedDemand.find(endpointId); refused != m_refusedDemand.end()) {
        previousDemand = refused->second.before;
        m_refusedDemand.erase(refused);
    }
    // Fix wave 3 (the re-review's third out-of-scope item): the pixels its
    // window can carry, not more. The grant's clamp here is geometry only
    // (the source bins in the window at the engine's size, never the
    // budget), so a device asking for more pixels than its window has bins
    // does not keep a demand it cannot use.
    const auto asked = endpointDisplayCost(grant.grantedPixels, fps,
                                           request.requestedWideSpanFactor > 1.0,
                                           extrasRequest.sections());
    if (asked) {
        setDisplayDemand(endpointId, asked->charge);
    }
    // A display refused for any other reason than the budget leaves the
    // demand as it was.
    const auto undoDemand = [this, endpointId, previousDemand]() {
        setDisplayDemand(endpointId, previousDemand);
    };
    if (!displayCost || !spectrumAdmissionFits(endpointId, displayCost->charge)) {
        // Refused for the budget: the request stays in this device's
        // demand, held until the client asks again or the hold runs out.
        holdRefusedDemand(endpointId, previousDemand);
        return rejectAllocation(control, endpointId, revision,
                                QString::fromLatin1(kDisplayBudgetRefusalReason));
    }
    // The endpoint never emits more samples than its admitted charge.
    request.pixels = grant.grantedPixels;

    EndpointEntry entry;
    entry.widebandNegotiated = widebandNegotiated;
    entry.revision = revision;
    entry.sliceId = sliceId;
    entry.sourceFftSize = grant.grantedFftSize;
    entry.sourceWindowType = windowType;
    entry.requestedDecimation = decimation;
    entry.sourceDecimation = grant.grantedDecimation;
    entry.request = request;
    entry.displayCost = *displayCost;
    entry.chargeCoversRequest = chargedPixels >= pixels || !displayBudgetWireAvailable();
    entry.grant = grant;
    entry.allocation = {control, revision, true, false, {}};
    entry.extrasRequest = extrasRequest;
    if (txWindowPresent) {
        entry.txWindow = std::pair{static_cast<float>(txMinDbm), static_cast<float>(txMaxDbm)};
    }
    entry.duplex = duplexPresent && control.value(QStringLiteral("duplex")).toBool();
    entry.miniDisplay = rolePresent;
    if (!extrasRequest.empty()) {
        entry.extras = std::make_unique<DisplayExtrasProcessor>(extrasRequest);
    }
    if (!reconcileWidebandDemand(entry)) {
        undoDemand();
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The extended view is not available right now."));
    }
    std::optional<EndpointEntry> replaced;
    MediaSourceKey replacedSource;
    if (existing != m_endpoints.end()) {
        replacedSource = existing->second.request.source;
        replaced.emplace(std::move(existing->second));
        m_endpoints.erase(existing);
    }
    m_endpoints.emplace(endpointId, std::move(entry));
    if (!reconcileSource(source)) {
        m_endpoints.erase(endpointId);
        releaseSourceIfUnused(source);
        if (replaced.has_value()) {
            m_endpoints.emplace(endpointId, std::move(*replaced));
            reconcileSource(replacedSource);
        }
        undoDemand();
        return rejectAllocation(control, endpointId, revision,
                                QStringLiteral("The Core could not set up this receiver's spectrum."));
    }
    // Parity Task 28: an endpoint showing the transmit display keeps its
    // place among the viewers, and its new request moves its view (a pan or
    // zoom on the transmitting pan moves the analyzer when it governs).
    if (replaced.has_value() && replaced->txViewer
        && replaced->miniDisplay == m_endpoints.at(endpointId).miniDisplay
        && !m_endpoints.at(endpointId).duplex) {
        EndpointEntry& current = m_endpoints.at(endpointId);
        current.txViewer = std::move(replaced->txViewer);
        current.endpoint.reset();
        current.contextSent = false;
        current.latestInput.reset();
        if (TxDisplayFeed* feed = current.txViewer->feed.data()) {
            feed->updateViewer(current.txViewer->id, current.request.centreHz,
                               current.request.spanHz, current.request.pixels);
        }
    }
    if (replaced.has_value() && !(replacedSource == source)) {
        releaseSourceIfUnused(replacedSource);
        rebalanceSourceAfterDeparture(replacedSource);
    }
    forgetNonliveOperation(endpointId);
    refreshDisplayBudgetPacer();
    refreshDeviceDisplayDuplex();
    if (revisioned) {
        sendAllocationResult(m_peer->connectionId(), endpointId, revision, true, {});
    }
    // Parity Task 28: while keyed, an endpoint on the transmitting pan shows
    // the transmit display from its first context.
    if (m_txDisplayNegotiated) {
        const QPointer<DaemonMediaController> self(this);
        reconcileTransmitDisplay();
        if (!self) { return true; }
    }
    return true;
}

bool DaemonMediaController::handleUnsubscribe(const QJsonObject& control)
{
    const bool revisioned = displayBudgetWireAvailable();
    quint32 endpointId = 0;
    quint32 revision = 0;
    const bool shapeValid = revisioned
        ? exactKeys(control, {"op", "connectionId", "endpointId", "revision"})
        : exactKeys(control, {"op", "connectionId", "endpointId"});
    const bool validIdentity = m_peer
        && canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        && control.value(QStringLiteral("connectionId")).toString() == m_peer->connectionId()
        && exactUnsigned(control.value(QStringLiteral("endpointId")), endpointId, true)
        && (!revisioned
            || exactUnsigned(control.value(QStringLiteral("revision")), revision, true));
    if (!shapeValid || !validIdentity) {
        if (revisioned && validIdentity) {
            const bool remember = m_endpoints.contains(endpointId)
                || m_nonliveOperations.contains(endpointId)
                || endpointId > m_endpointHighWater;
            m_endpointHighWater = std::max(m_endpointHighWater, endpointId);
            return rejectAllocation(control, endpointId, revision,
                                    QStringLiteral("The Core could not read the request to stop this display."), remember);
        }
        return false;
    }
    if (!revisioned) {
        removeEndpoint(endpointId);
        return true;
    }

    auto active = m_endpoints.find(endpointId);
    if (active != m_endpoints.end()) {
        const AllocationRecord& prior = active->second.allocation;
        if (revision == prior.revision && control == prior.request) {
            const bool accepted = prior.accepted;
            sendAllocationResult(m_peer->connectionId(), endpointId, revision,
                                 accepted, prior.reason);
            return accepted;
        }
        if (staleOrEqualRevision(revision, prior.revision)) {
            return rejectAllocation(control, endpointId, revision,
                                    QStringLiteral("A newer request for this display had already arrived."), false);
        }
    } else {
        auto recent = m_nonliveOperations.find(endpointId);
        if (recent != m_nonliveOperations.end()) {
            const AllocationRecord prior = recent->second;
            if (revision == prior.revision && control == prior.request) {
                sendAllocationResult(m_peer->connectionId(), endpointId, revision,
                                     prior.accepted, prior.reason);
                return prior.accepted;
            }
            if (prior.explicitlyRetired || staleOrEqualRevision(revision, prior.revision)) {
                return rejectAllocation(control, endpointId, revision,
                                        prior.explicitlyRetired
                                            ? QStringLiteral("This display had already been closed.")
                                            : QStringLiteral("A newer request for this display had already arrived."),
                                        false);
            }
        } else if (endpointId <= m_endpointHighWater) {
            return rejectAllocation(control, endpointId, revision,
                                    QStringLiteral("This display had already been closed."), false);
        }
    }
    m_endpointHighWater = std::max(m_endpointHighWater, endpointId);
    removeEndpoint(endpointId, false);
    rememberNonliveOperation(endpointId,
        AllocationRecord{control, revision, true, true, {}});
    sendAllocationResult(m_peer->connectionId(), endpointId, revision, true, {});
    return true;
}

bool DaemonMediaController::handleKeyframe(const QJsonObject& control)
{
    quint32 endpointId = 0;
    quint32 contextGeneration = 0;
    if (!exactKeys(control, {"op", "connectionId", "endpointId", "contextGeneration"}) || !m_peer
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactUnsigned(control.value(QStringLiteral("endpointId")), endpointId, true)
        || !exactUnsigned(control.value(QStringLiteral("contextGeneration")), contextGeneration, true)) {
        return false;
    }
    auto it = m_endpoints.find(endpointId);
    // Parity Task 28: a transmit context's generation while it shows the
    // transmit display, the receive context's otherwise.
    const bool transmitContext = it != m_endpoints.end() && it->second.txViewer
        && it->second.txContextSent
        && it->second.txCodec.contextGeneration == contextGeneration;
    if (it == m_endpoints.end()
        || (!transmitContext
            && (!it->second.endpoint.configured()
                || it->second.endpoint.context().codec.contextGeneration != contextGeneration))) {
        return false;
    }
    if (!it->second.keyframeWindow.isValid() || it->second.keyframeWindow.elapsed() >= 1000) {
        it->second.keyframeWindow.start();
        it->second.keyframesInWindow = 0;
    }
    // R-R3-21: each request is a window that lost a display message and
    // waits for this keyframe, so every one is logged and counted.
    ++m_displayDiagnostics.displayKeyframeRequests;
    if (it->second.keyframesInWindow >= kMaxKeyframesPerSecond) {
        ++m_displayDiagnostics.displayKeyframeRequestsRefused;
        // The peer drives these: at most one line every 10 s, with the count.
        if (!m_keyframeRefusalLog.isValid() || m_keyframeRefusalLog.elapsed() >= 10'000) {
            qCInfo(lcDaemonMedia).noquote()
                << QStringLiteral("display keyframe request refused endpoint=%1 generation=%2"
                                  " (more than %3 a second) refused=%4 requests=%5")
                       .arg(endpointId).arg(contextGeneration).arg(kMaxKeyframesPerSecond)
                       .arg(m_displayDiagnostics.displayKeyframeRequestsRefused)
                       .arg(m_displayDiagnostics.displayKeyframeRequests);
            m_keyframeRefusalLog.start();
        }
        return false;
    }
    ++it->second.keyframesInWindow;
    it->second.forceKeyframe = true;
    qCInfo(lcDaemonMedia).noquote()
        << QStringLiteral("display keyframe requested endpoint=%1 generation=%2 requests=%3")
               .arg(endpointId).arg(contextGeneration)
               .arg(m_displayDiagnostics.displayKeyframeRequests);
    return true;
}

bool DaemonMediaController::handleClarityRetune(const QJsonObject& control)
{
    quint32 endpointId = 0;
    if (!exactKeys(control, {"op", "connectionId", "endpointId"})
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || !exactUnsigned(control.value(QStringLiteral("endpointId")), endpointId, true)) {
        return false;
    }
    // A refusal names the endpoint with revision 0: never a subscription's
    // revision, so it retires nothing, and never the whole-peer refusal
    // (endpoint and revision both 0).
    const QString connectionId = control.value(QStringLiteral("connectionId")).toString();
    if (!m_peer || connectionId != m_peer->connectionId()) {
        sendRejected(connectionId, endpointId, 0,
                     QStringLiteral("That display is not one this app opened."));
        return false;
    }
    const auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        sendRejected(connectionId, endpointId, 0,
                     QStringLiteral("That display is no longer open on the Core."));
        return false;
    }
    if (!it->second.extras || !it->second.extras->retuneClarity()) {
        sendRejected(connectionId, endpointId, 0,
                     QStringLiteral("Clarity is not setting this display's waterfall levels."));
        return false;
    }
    return true;
}

bool DaemonMediaController::handleAudio(const QJsonObject& control)
{
    quint32 revision = 0;
    // R-R3-23: `profile` ("opus" or "lossless") only from a GUI the Core can
    // answer in the profile shape, which needs the minor-8 detail.
    const bool hasProfile = control.contains(QStringLiteral("profile"));
    std::optional<RemoteAudioProfile> requestedProfile;
    if (hasProfile) {
        requestedProfile = remoteAudioProfileFromWire(control.value(QStringLiteral("profile")));
    }
    // iPhone app plan Task 23 (audioQualityVersion 1): `opusBitrate`, a
    // whole number, only beside `profile` and only from a device the Core
    // told it may ask.
    const bool hasBitrate = control.contains(QStringLiteral("opusBitrate"));
    int requestedBitrate = 0;
    // The shape is checked without it, as the display extras are.
    QJsonObject shape = control;
    shape.remove(QStringLiteral("opusBitrate"));
    if (!(hasProfile ? exactKeys(shape, {"op", "connectionId", "revision", "enabled",
                                         "profile"})
                     : exactKeys(shape, {"op", "connectionId", "revision", "enabled"}))
        || (hasBitrate && (!hasProfile || !m_server || !m_server->audioQualityAvailable(m_epoch)
                           || !exactInt(control.value(QStringLiteral("opusBitrate")), 0,
                                        std::numeric_limits<int>::max(), requestedBitrate)))
        || (hasProfile && (!requestedProfile || !m_server
                           || !m_server->remoteAudioStatusAvailable(m_epoch)))
        || !m_peer
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactUnsigned(control.value(QStringLiteral("revision")), revision, true)
        || !control.value(QStringLiteral("enabled")).isBool()
        || (m_audioRevision != 0 && staleOrEqualRevision(revision, m_audioRevision))) {
        return false;
    }

    m_audioRevision = revision;
    m_audioDesiredEnabled = control.value(QStringLiteral("enabled")).toBool();
    if (hasProfile) {
        m_audioProfileNegotiated = true;
        m_audioRequestedProfile = *requestedProfile;
    }
    // iPhone app plan Task 23: a bitrate in the measured table becomes this
    // device's; any other is refused with its reason, and the running one
    // stays. A request without one keeps the device's earlier choice.
    m_audioBitrateRefusal.clear();
    if (hasBitrate) {
        if (isMeasuredOpusBitrate(requestedBitrate)) {
            m_audioRequestedBitrate = requestedBitrate;
        } else {
            m_audioBitrateRefusal = opusBitrateNotOfferedReason();
        }
    }
    // An accepted control is a fresh audio context even if it leaves actual
    // capture unavailable pending peer readiness or station reconnect.
    reconcileAudio();
    return true;
}

bool DaemonMediaController::handleReceiverAudio(const QJsonObject& control)
{
    // An app that did not declare receiver audio at start gets nothing:
    // its offer carries no receiver stream ids, and packets on an id it
    // does not know would be refused and reported one by one.
    if (!m_receiverAudioNegotiated || !m_peer) {
        return false;
    }
    int sliceId = -1;
    quint32 revision = 0;
    const std::optional<RemoteAudioProfile> profile =
        remoteAudioProfileFromWire(control.value(QStringLiteral("profile")));
    if (!exactKeys(control, {"op", "connectionId", "sliceId", "revision", "enabled", "profile"})
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactInt(control.value(QStringLiteral("sliceId")), 0,
                     std::numeric_limits<int>::max(), sliceId)
        || !exactUnsigned(control.value(QStringLiteral("revision")), revision, true)
        || !control.value(QStringLiteral("enabled")).isBool() || !profile) {
        return false;
    }
    const bool enabled = control.value(QStringLiteral("enabled")).toBool();
    auto it = m_receiverStreams.find(sliceId);
    if (it == m_receiverStreams.end()) {
        if (!m_radioModel || m_radioModel->sliceById(sliceId) == nullptr
            || !hearsSlice(sliceId)) {
            // No such slice, or (Task 76, ruling 9.2) not this device's:
            // answer, but keep no entry, so requests for made-up slice ids
            // cannot grow this map.
            ReceiverAudioStream absent;
            absent.revision = revision;
            absent.requestedProfile = *profile;
            const AdmittedAudioProfile admitted = admitProfile(*profile);
            absent.activeProfile = admitted.active;
            absent.profileRefusal = admitted.refusal;
            sendReceiverAudioContext(sliceId, absent, false,
                                     enabled ? RemoteAudioOffReason::SliceRemoved
                                             : RemoteAudioOffReason::ClientDisabled);
            return true;
        }
        it = m_receiverStreams.emplace(sliceId, ReceiverAudioStream{}).first;
    } else if (staleOrEqualRevision(revision, it->second.revision)) {
        return false;
    }
    it->second.revision = revision;
    it->second.desiredEnabled = enabled;
    it->second.requestedProfile = *profile;
    reconcileReceiverAudio(sliceId);
    return true;
}

quint64 DaemonMediaController::iqBytesPerSecond() const
{
    quint64 bytes = 0;
    for (const auto& [sliceId, stream] : m_iqStreams) {
        Q_UNUSED(sliceId);
        if (stream.sending) { bytes += stream.bytesPerSecond; }
    }
    return bytes;
}

bool DaemonMediaController::handleIqStream(const QJsonObject& control)
{
    if (!m_iqNegotiated || !m_peer) { return false; }
    int sliceId = -1;
    quint32 revision = 0;
    if (!exactKeys(control, {"op", "connectionId", "sliceId", "revision", "enabled"})
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactInt(control.value(QStringLiteral("sliceId")), 0,
                     std::numeric_limits<int>::max(), sliceId)
        || !exactUnsigned(control.value(QStringLiteral("revision")), revision, true)
        || !control.value(QStringLiteral("enabled")).isBool()) { return false; }
    const bool enabled = control.value(QStringLiteral("enabled")).toBool();
    auto it = m_iqStreams.find(sliceId);
    if (it == m_iqStreams.end()) {
        if (!m_radioModel || !m_radioModel->sliceById(sliceId) || !controlsSlice(sliceId)) {
            IqStream absent;
            absent.revision = revision;
            absent.generation = ++m_nextIqGeneration;
            if (absent.generation == 0) { absent.generation = ++m_nextIqGeneration; }
            sendIqContext(sliceId, absent,
                          enabled ? QStringLiteral("This receiver is not available to this device.")
                                  : QString());
            return true;
        }
        it = m_iqStreams.emplace(sliceId, IqStream{}).first;
    } else if (staleOrEqualRevision(revision, it->second.revision)) {
        return false;
    }
    it->second.revision = revision;
    it->second.desired = enabled;
    reconcileIqStream(sliceId);
    return true;
}

void DaemonMediaController::stopIqStream(IqStream& stream)
{
    if (stream.ingress) {
        stream.ingress->stopped.store(true, std::memory_order_release);
    }
    QObject::disconnect(stream.tap);
    stream.tap = {};
    stream.ingress.reset();
    stream.pending.clear();
    stream.pendingDebited = false;
    stream.blockedAtNs = -1;
    stream.sending = false;
    stream.budgetRefused = false;
    stream.lastBudgetRefusalRevision = 0;
    stream.bytesPerSecond = 0;
    stream.sampleRate = 0;
    stream.generation = ++m_nextIqGeneration;
    if (stream.generation == 0) { stream.generation = ++m_nextIqGeneration; }
}

void DaemonMediaController::sendIqContext(int sliceId, const IqStream& stream,
                                          const QString& reason)
{
    if (!m_peer) { return; }
    sendControl({{QStringLiteral("op"), QStringLiteral("iq-stream-context")},
                 {QStringLiteral("connectionId"), m_peer->connectionId()},
                 {QStringLiteral("sliceId"), sliceId},
                 {QStringLiteral("revision"), qint64(stream.revision)},
                 {QStringLiteral("enabled"), stream.sending},
                 {QStringLiteral("generation"), qint64(stream.generation)},
                 {QStringLiteral("sampleRateHz"), stream.sampleRate},
                 {QStringLiteral("reason"), reason}});
}

void DaemonMediaController::reconcileIqStream(int sliceId)
{
    auto it = m_iqStreams.find(sliceId);
    if (it == m_iqStreams.end()) { return; }
    IqStream& stream = it->second;
    stopIqStream(stream);
    stream.sequence = 0;
    const SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
    stream.streamIndex = slice ? slice->streamIndex() : -1;
    stream.sampleRate = stream.streamIndex >= 0 && m_radioModel
        ? m_radioModel->streamSampleRateHz(stream.streamIndex) : 0;
    QString reason;
    bool budgetRefused = false;
    if (!stream.desired) {
        reason.clear();
    } else if (!slice || !controlsSlice(sliceId) || stream.streamIndex < 0
               || !m_radioModel->streamActive(stream.streamIndex)) {
        reason = QStringLiteral("This receiver is not available on the Core.");
    } else if (stream.sampleRate < 48000 || stream.sampleRate > 384000) {
        reason = QStringLiteral("This receiver's hardware I/Q rate is unsupported (48-384 kHz).");
    } else if (!m_peer || !m_peer->isReady()) {
        reason = QStringLiteral("The media connection is not ready for raw I/Q.");
    } else {
        const quint64 charge = quint64(stream.sampleRate) * 8
            + quint64((stream.sampleRate + 1023) / 1024) * 24;
        QList<DisplayBudgetCharge> floor{
            ps3DisplayHere() ? ps3DisplayCharge() : DisplayBudgetCharge{},
            DisplayBudgetCharge{iqBytesPerSecond() + charge, 0, 0}};
        for (const auto& [endpointId, endpoint] : m_endpoints) {
            Q_UNUSED(endpointId);
            if (const auto cost = spectrumDisplayCost(1, 1, endpoint.request.extendedView)) {
                floor.append(cost->charge);
            }
        }
        const auto limits = m_server
            ? m_server->displayBudgetLimitsWithAdditionalDemand(m_epoch, charge)
            : std::nullopt;
        const auto minimum = sumDisplayCharges(floor);
        if (!limits || !minimum || !displayChargeFits(*limits, *minimum)) {
            reason = QStringLiteral("The link to the Core is too busy to send raw I/Q for this receiver.");
            budgetRefused = true;
        } else {
            stream.bytesPerSecond = charge;
            stream.ingress = std::make_shared<RemoteIqIngress>();
            const std::shared_ptr<RemoteIqIngress> ingress = stream.ingress;
            const int source = stream.streamIndex;
            stream.tap = connect(m_radioModel, &RadioModel::rawIqDataForStream,
                                 this, [ingress, source](int arrived,
                                                        const QVector<float>& samples) {
                if (arrived == source) { ingress->push(samples); }
            }, Qt::DirectConnection);
            stream.sending = true;
            if (!m_sendTimer.isActive()) { m_sendTimer.start(); }
        }
    }
    stream.budgetRefused = budgetRefused;
    if (!stream.sending) { stream.sampleRate = 0; }
    const QPointer<DaemonMediaController> self(this);
    const quint64 epoch = m_epoch;
    MediaPeer* const peer = m_peer.get();
    sendIqContext(sliceId, stream, reason);
    if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
    refreshDisplayBudgetPacer();
    if (m_server) { m_server->publishDisplayBudgetCapabilities(); }
    if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
    if (budgetRefused) {
        const auto found = m_iqStreams.find(sliceId);
        if (found != m_iqStreams.end()) {
            found->second.lastBudgetRefusalRevision = m_iqBudgetChangeRevision;
        }
    }
}

void DaemonMediaController::reconcileWantedIq()
{
    QList<int> wanted;
    for (const auto& [sliceId, stream] : m_iqStreams) {
        if (stream.desired) { wanted.append(sliceId); }
    }
    const QPointer<DaemonMediaController> self(this);
    const quint64 epoch = m_epoch;
    MediaPeer* const peer = m_peer.get();
    for (int sliceId : wanted) {
        if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
        const auto found = m_iqStreams.find(sliceId);
        if (found != m_iqStreams.end() && found->second.desired) {
            reconcileIqStream(sliceId);
        }
    }
}

bool DaemonMediaController::iqFitsCurrentShare() const
{
    const auto limits = m_server ? m_server->displayBudgetLimits(m_epoch) : std::nullopt;
    if (!limits) { return false; }
    QList<DisplayBudgetCharge> floor{
        ps3DisplayHere() ? ps3DisplayCharge() : DisplayBudgetCharge{},
        DisplayBudgetCharge{iqBytesPerSecond(), 0, 0}};
    for (const auto& [endpointId, endpoint] : m_endpoints) {
        Q_UNUSED(endpointId);
        const auto cost = spectrumDisplayCost(1, 1, endpoint.request.extendedView);
        if (!cost) { return false; }
        floor.append(cost->charge);
    }
    const auto total = sumDisplayCharges(floor);
    return total && displayChargeFits(*limits, *total);
}

void DaemonMediaController::reconcileIqBudget()
{
    if (!m_iqNegotiated || !m_peer || !m_peer->isReady()) { return; }
    QList<int> wanted;
    for (const auto& [sliceId, stream] : m_iqStreams) {
        if (stream.desired) { wanted.append(sliceId); }
    }
    const QPointer<DaemonMediaController> self(this);
    const quint64 epoch = m_epoch;
    MediaPeer* const peer = m_peer.get();
    for (int sliceId : wanted) {
        if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
        const auto found = m_iqStreams.find(sliceId);
        if (found == m_iqStreams.end() || !found->second.desired) { continue; }
        if (found->second.sending && !iqFitsCurrentShare()) {
            stopIqStream(found->second);
            found->second.budgetRefused = true;
            sendIqContext(sliceId, found->second,
                          QStringLiteral("The link to the Core is too busy to send raw I/Q for this receiver."));
            if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
            refreshDisplayBudgetPacer();
            if (m_server) { m_server->publishDisplayBudgetCapabilities(); }
            if (!self || epoch != m_epoch || peer != m_peer.get()) { return; }
            const auto current = m_iqStreams.find(sliceId);
            if (current != m_iqStreams.end() && current->second.budgetRefused) {
                current->second.lastBudgetRefusalRevision = m_iqBudgetChangeRevision;
            }
        } else if (!found->second.sending && found->second.budgetRefused
                   && found->second.lastBudgetRefusalRevision
                       != m_iqBudgetChangeRevision) {
            reconcileIqStream(sliceId);
        }
    }
}

void DaemonMediaController::reconcileReceiverAudio(int sliceId)
{
    const auto it = m_receiverStreams.find(sliceId);
    if (it == m_receiverStreams.end()) {
        return;
    }
    ReceiverAudioStream& stream = it->second;
    // Only this slice's capture restarts; the main stream and every other
    // receiver stream carry on untouched.
    stopReceiverAudioCapture(stream);
    std::optional<RemoteAudioOffReason> blockedBy;
    if (!stream.desiredEnabled) {
        blockedBy = RemoteAudioOffReason::ClientDisabled;
    } else if (!m_radioModel || m_radioModel->sliceById(sliceId) == nullptr
               || !hearsSlice(sliceId)) {
        // Gone, or (Task 76) no longer this device's.
        stream.desiredEnabled = false;
        blockedBy = RemoteAudioOffReason::SliceRemoved;
    } else if (stream.streamIndex < 0) {
        for (int index = 0; index < IMediaTransport::kMaxReceiverAudioStreams; ++index) {
            if (m_receiverStreamSlice[static_cast<size_t>(index)] < 0) {
                m_receiverStreamSlice[static_cast<size_t>(index)] = sliceId;
                stream.streamIndex = index;
                break;
            }
        }
        if (stream.streamIndex < 0) {
            // Refused, not queued: the GUI asks again once it has let a
            // stream go.
            stream.desiredEnabled = false;
            blockedBy = RemoteAudioOffReason::ReceiverLimit;
        }
    }
    if (!blockedBy) {
        if (!m_radioModel->isConnected()) {
            blockedBy = RemoteAudioOffReason::RadioOffline;
        } else if (!m_peer || !m_peer->isReady()) {
            blockedBy = RemoteAudioOffReason::MediaNotReady;
        }
    }
    const AdmittedAudioProfile admitted = admitProfile(stream.requestedProfile);
    stream.activeProfile = admitted.active;
    stream.profileRefusal = admitted.refusal;
    if (!stream.desiredEnabled) {
        // Off for the client's own choice, a removed slice or the limit: the
        // context names the stream id the packets stopped on (none for the
        // limit), then the id goes free for another slice.
        sendReceiverAudioContext(sliceId, stream, false, *blockedBy);
        releaseReceiverStreamIndex(stream);
        return;
    }
    bool actualEnabled = false;
    if (!blockedBy) {
        if (!stream.sender) {
            // Opus at the receiver streams' own 48 kbit/s, or lossless: the
            // session's one quality choice. The rate is fixed here, not
            // taken from audio_bitrate, so a lossless stream that falls back
            // (refused, or the window asks for Opus after its link trial)
            // encodes at 48 kbit/s too; the speakers' mix keeps its own.
            OpusAudioCodecConfig codecConfig;
            codecConfig.bitrate = kReceiverAudioOpusBitrate;
            // Parented, so a sender retired with deleteLater() (see
            // retireReceiverSender) is still reclaimed with this controller.
            stream.sender = std::make_unique<DaemonAudioSender>(
                m_radioModel->audioEngine(), codecConfig, this);
            stream.sender->setSliceSource(sliceId);
            stream.sender->setCaptureClock([this] { return displayNowNs(); });
            DaemonAudioSender* const sender = stream.sender.get();
            connect(sender, &DaemonAudioSender::packetReady, this,
                    [this, sliceId, sender](const QByteArray& packet) {
                onReceiverAudioPacket(sliceId, sender, packet);
            });
        }
        const auto index = static_cast<size_t>(stream.streamIndex);
        const QList<quint32> ssrcs = m_peer->receiverAudioSsrcs();
        stream.sender->setProfile(stream.activeProfile);
        actualEnabled = stream.streamIndex < ssrcs.size()
            && stream.sender->start(ssrcs.at(stream.streamIndex),
                                    m_receiverNextSequence[index],
                                    m_receiverNextTimestamp[index]);
        stream.sending = actualEnabled;
        if (actualEnabled) {
            qCInfo(lcDaemonMedia).noquote().nospace()
                << "receiver audio for slice " << sliceId << " on stream "
                << stream.streamIndex << ", "
                << remoteAudioProfileToWire(stream.activeProfile)
                << ", revision " << stream.revision;
        }
    }
    sendReceiverAudioContext(sliceId, stream, actualEnabled,
                             blockedBy.value_or(RemoteAudioOffReason::EncoderUnavailable));
}

void DaemonMediaController::stopReceiverAudioCapture(ReceiverAudioStream& stream)
{
    if (!stream.sender) {
        stream.sending = false;
        return;
    }
    if (stream.streamIndex >= 0 && stream.sender->isRunning()) {
        // As the main stream: the stream id's timeline continues from here.
        const auto index = static_cast<size_t>(stream.streamIndex);
        m_receiverNextSequence[index] = stream.sender->nextSequence();
        m_receiverNextTimestamp[index] = stream.sender->nextTimestamp();
    }
    stream.sender->stop();
    stream.sending = false;
}

void DaemonMediaController::releaseReceiverStreamIndex(ReceiverAudioStream& stream)
{
    if (stream.streamIndex >= 0) {
        m_receiverStreamSlice[static_cast<size_t>(stream.streamIndex)] = -1;
        stream.streamIndex = -1;
    }
}

void DaemonMediaController::sendReceiverAudioContext(int sliceId,
                                                     const ReceiverAudioStream& stream,
                                                     bool enabled, RemoteAudioOffReason reason)
{
    if (!m_peer) {
        return;
    }
    const bool lossless = stream.activeProfile == RemoteAudioProfile::Lossless;
    RemoteReceiverAudioContextMessage message;
    message.sliceId = sliceId;
    RemoteAudioContextMessage& context = message.context;
    context.connectionId = m_peer->connectionId();
    context.revision = stream.revision;
    context.generation = nextReceiverContextGeneration();
    context.enabled = enabled;
    const QList<quint32> ssrcs = m_peer->receiverAudioSsrcs();
    if (stream.streamIndex >= 0 && stream.streamIndex < ssrcs.size()) {
        const auto index = static_cast<size_t>(stream.streamIndex);
        context.ssrc = ssrcs.at(stream.streamIndex);
        context.firstSequence = m_receiverNextSequence[index];
        context.firstTimestamp = m_receiverNextTimestamp[index];
    }
    if (enabled && lossless && stream.sender) {
        context.losslessEncoder = stream.sender->losslessProfile();
    } else if (enabled && stream.sender) {
        context.encoder = stream.sender->encoderProfile();
    }
    if (!enabled) {
        context.offReason = reason;
    }
    context.profile = stream.activeProfile;
    context.profileRefusal = stream.profileRefusal;
    sendControl(encodeReceiverAudioContext(message));
}

void DaemonMediaController::onReceiverAudioPacket(int sliceId, DaemonAudioSender* sender,
                                                  const QByteArray& packet)
{
    // Emitted on the sender's owner thread. The identity checks keep a
    // stopped or retired stream from reaching a replacement peer.
    const auto it = m_receiverStreams.find(sliceId);
    if (it == m_receiverStreams.end() || it->second.sender.get() != sender
        || !it->second.sending || !sender->isRunning() || m_epoch == 0 || !m_peer
        || !m_peer->isReady() || !m_receiverAudioNegotiated) {
        return;
    }
    sendAudioRtp(m_peer.get(), packet);
}

void DaemonMediaController::retireReceiverSender(ReceiverAudioStream& stream)
{
    stopReceiverAudioCapture(stream);
    if (!stream.sender) {
        return;
    }
    // Retirement can run inside the sender's own packetReady emission (a
    // send that synchronously closes the peer ends the session), so the
    // sender is not destroyed under its drain(): stopped here (its slice tap
    // is already cleared, so the DSP thread no longer reaches it) and
    // deleted from the event loop.
    DaemonAudioSender* const sender = stream.sender.release();
    sender->disconnect(this);
    sender->deleteLater();
}

void DaemonMediaController::resetReceiverAudioSession()
{
    for (auto& [sliceId, stream] : m_receiverStreams) {
        Q_UNUSED(sliceId);
        retireReceiverSender(stream);
    }
    m_receiverStreams.clear();
    m_receiverAudioNegotiated = false;
    // A different media peer has different stream ids and a new timeline.
    m_receiverStreamSlice.fill(-1);
    m_receiverNextSequence.fill(1);
    m_receiverNextTimestamp.fill(0);
}

quint32 DaemonMediaController::nextReceiverContextGeneration()
{
    ++m_nextReceiverContextGeneration;
    if (m_nextReceiverContextGeneration == 0) {
        ++m_nextReceiverContextGeneration;
    }
    return m_nextReceiverContextGeneration;
}

int DaemonMediaController::activeReceiverAudioStreamCount() const
{
    int count = 0;
    for (const auto& [sliceId, stream] : m_receiverStreams) {
        Q_UNUSED(sliceId);
        if (stream.sending && stream.sender && stream.sender->isRunning()) {
            ++count;
        }
    }
    return count;
}

std::optional<RemoteAudioProfile> DaemonMediaController::receiverAudioProfile(int sliceId) const
{
    const auto it = m_receiverStreams.find(sliceId);
    if (it == m_receiverStreams.end() || !it->second.sending) {
        return std::nullopt;
    }
    return it->second.activeProfile;
}

// ---- R-R3-45: the headphones mix ----

bool DaemonMediaController::handleHeadphonesAudio(const QJsonObject& control)
{
    // An app that did not declare the headphones mix at start gets
    // nothing: its offer carries no headphones stream id.
    if (!m_headphonesMixNegotiated || !m_peer) {
        return false;
    }
    quint32 revision = 0;
    const std::optional<RemoteAudioProfile> profile =
        remoteAudioProfileFromWire(control.value(QStringLiteral("profile")));
    if (!exactKeys(control, {"op", "connectionId", "revision", "enabled", "profile"})
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactUnsigned(control.value(QStringLiteral("revision")), revision, true)
        || !control.value(QStringLiteral("enabled")).isBool() || !profile
        || (m_headphones.revision != 0
            && staleOrEqualRevision(revision, m_headphones.revision))) {
        return false;
    }
    m_headphones.revision = revision;
    m_headphones.desiredEnabled = control.value(QStringLiteral("enabled")).toBool();
    m_headphones.requestedProfile = *profile;
    reconcileHeadphonesAudio();
    return true;
}

// ---- Parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor ----
//
// Thetis mixes the transmitter's stream into its one audio output at a fixed
// 0.5 while MON is on, and chkMON_CheckedChanged sets it:
// From Thetis audio.cs:407-424 [v2.10.3.15] (Audio.MON's setter):
//   cmaster.SetAAudioMixVol((void*)0, 0, WDSP.id(1, 0), 0.5);
//   cmaster.SetAAudioMixWhat((void*)0, 0, WDSP.id(1, 0), value);
// From Thetis console.cs:29040-29066 [v2.10.3.15] (chkMON_CheckedChanged):
//   Audio.MON = chkMON.Checked;
// NereusSDR's slot gain is the MON level (TransmitModel::monitorVolume,
// default 0.5 from that literal). On a Core the device that holds transmit
// hears it in its own mix, as a desktop hears it on its own output.

bool DaemonMediaController::handleMonitorAudio(const QJsonObject& control)
{
    if (!m_txMonitorNegotiated || !m_peer) {
        return false;
    }
    const std::optional<MonitorAudioMessage> request = decodeMonitorAudioRequest(control);
    if (!request || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || request->connectionId != m_peer->connectionId()
        || (m_monitorRevision != 0 && staleOrEqualRevision(request->revision, m_monitorRevision))) {
        return false;
    }
    m_monitorRevision = request->revision;
    m_monitorRoute = request->route;
    const QPointer<DaemonMediaController> self(this);
    MonitorAudioMessage context;
    context.connectionId = m_peer->connectionId();
    context.revision = m_monitorRevision;
    context.route = appliedMonitorRoute();
    sendControl(encodeMonitorAudioContext(context));
    // Sending can end the session (its control link closing).
    if (!self || !m_peer) {
        return true;
    }
    refreshTxMonitor();
    return true;
}

TxMonitorRoute DaemonMediaController::appliedMonitorRoute() const
{
    // The brief's rule: headphones is this device's headphones stream when
    // it declared the headphones mix, else its main stream.
    if (m_monitorRoute == TxMonitorRoute::Headphones && !m_headphonesMixNegotiated) {
        return TxMonitorRoute::Speakers;
    }
    return m_monitorRoute;
}

void DaemonMediaController::refreshTxMonitor()
{
    TxMonitorRoute route = TxMonitorRoute::None;
    if (m_txMonitorNegotiated && m_monitorRevision != 0 && m_peer && m_epoch != 0 && m_server
        && m_radioModel && m_radioModel->transmitModel().monEnabled()) {
        // Only the device that holds transmit (txState's holder) hears it;
        // the tap itself carries audio only while the radio is keyed.
        const TransmitHolder* holder = m_server->transmitHolder();
        const std::optional<TransmitHolder::Holder> held =
            holder != nullptr ? holder->holder() : std::nullopt;
        const QByteArray device = m_server->mediaSessionDevice(m_epoch);
        if (held && held->source == TransmitHolder::Source::Device && !device.isEmpty()
            && held->deviceId == device) {
            route = appliedMonitorRoute();
        }
    }
    const bool headphonesChanged = (route == TxMonitorRoute::Headphones)
        != (m_monitorApplied == TxMonitorRoute::Headphones);
    m_monitorApplied = route;
    if (m_ownerMix >= 0 && m_radioModel && m_radioModel->audioEngine() != nullptr) {
        const MasterMixer::OwnerMonitor monitor = route == TxMonitorRoute::Speakers
            ? MasterMixer::OwnerMonitor::Speakers
            : route == TxMonitorRoute::Headphones ? MasterMixer::OwnerMonitor::Headphones
                                                   : MasterMixer::OwnerMonitor::None;
        m_radioModel->audioEngine()->setOwnerMixMonitor(m_ownerMix, monitor);
    }
    // MON onto or off the headphones starts or stops that mix, as the first
    // receiver onto them or the last one off does.
    if (headphonesChanged) {
        onOutputRoutesChanged();
    }
}

bool DaemonMediaController::headphonesMixNeeded() const
{
    return anySliceOnHeadphones() || m_monitorApplied == TxMonitorRoute::Headphones;
}

bool DaemonMediaController::anySliceOnHeadphones() const
{
    if (!m_radioModel) {
        return false;
    }
    // Task 76: of this device's own slices; another device's receiver on
    // its headphones is in that device's mix, not this one's.
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice && slice->outputRoute() == SliceModel::OutputRoute::Headphones
            && controlsSlice(slice->sliceIndex())) {
            return true;
        }
    }
    return false;
}

void DaemonMediaController::watchSliceOutputRoute(SliceModel* slice)
{
    if (!slice) {
        return;
    }
    connect(slice, &SliceModel::outputRouteChanged, this,
            &DaemonMediaController::onOutputRoutesChanged, Qt::UniqueConnection);
}

void DaemonMediaController::onOutputRoutesChanged()
{
    // Only the first receiver onto the headphones starts the mix and only
    // the last one off stops it; the mix itself follows every route change
    // in AudioEngine without a new context.
    // Parity Task 32: MON routed to the headphones needs the mix too.
    const bool routed = headphonesMixNeeded();
    if (routed == m_headphonesRouted) {
        return;
    }
    m_headphonesRouted = routed;
    if (m_headphones.revision != 0) {
        reconcileHeadphonesAudio();
    }
}

void DaemonMediaController::reconcileHeadphonesAudio()
{
    // Only the headphones capture restarts; the main stream and every
    // receiver stream carry on untouched.
    stopHeadphonesAudioCapture();
    m_headphonesRouted = headphonesMixNeeded();
    const std::optional<RemoteAudioOffReason> blockedBy = headphonesBlockedBy();
    const AdmittedAudioProfile admitted = admitProfile(m_headphones.requestedProfile);
    m_headphones.activeProfile = admitted.active;
    m_headphones.profileRefusal = admitted.refusal;
    bool actualEnabled = false;
    if (!blockedBy) {
        if (!m_headphones.sender) {
            // Opus at the speakers' mix's setting (audio_bitrate), not the
            // receiver streams' rate, or lossless: the session's one quality
            // choice. Parented, so a sender retired with
            // deleteLater() is still reclaimed with this controller.
            OpusAudioCodecConfig codecConfig;
            codecConfig.bitrate = m_audioTargetBitrate;
            m_headphones.sender = std::make_unique<DaemonAudioSender>(
                m_radioModel->audioEngine(), codecConfig, this);
            m_headphones.sender->setSliceSource(DaemonAudioSource::kHeadphonesMix);
            m_headphones.sender->setCaptureClock([this] { return displayNowNs(); });
            DaemonAudioSender* const sender = m_headphones.sender.get();
            connect(sender, &DaemonAudioSender::packetReady, this,
                    [this, sender](const QByteArray& packet) {
                onHeadphonesAudioPacket(sender, packet);
            });
        }
        m_headphones.sender->setProfile(m_headphones.activeProfile);
        // Task 76 (ruling 9.2): this device's own headphones mix.
        m_headphones.sender->setOwnerMix(m_ownerMix);
        actualEnabled = m_peer->headphonesAudioSsrc() != 0
            && m_headphones.sender->start(m_peer->headphonesAudioSsrc(),
                                          m_headphones.nextSequence,
                                          m_headphones.nextTimestamp);
        m_headphones.sending = actualEnabled;
        if (actualEnabled) {
            qCInfo(lcDaemonMedia).noquote().nospace()
                << "headphones mix on, "
                << remoteAudioProfileToWire(m_headphones.activeProfile)
                << ", revision " << m_headphones.revision;
        }
    }
    sendHeadphonesAudioContext(actualEnabled,
                               blockedBy.value_or(RemoteAudioOffReason::EncoderUnavailable));
}

std::optional<RemoteAudioOffReason> DaemonMediaController::headphonesBlockedBy() const
{
    // Why the headphones mix is off, first cause wins: the client's own
    // choice, no receiver on the headphones, then the station radio, then
    // media readiness. Empty when it can run.
    if (!m_headphones.desiredEnabled) {
        return RemoteAudioOffReason::ClientDisabled;
    }
    // Parity Task 32: or MON on its way there.
    if (!headphonesMixNeeded()) {
        return RemoteAudioOffReason::NoHeadphonesReceiver;
    }
    if (!m_radioModel || !m_radioModel->isConnected()) {
        return RemoteAudioOffReason::RadioOffline;
    }
    if (!m_peer || !m_peer->isReady()) {
        return RemoteAudioOffReason::MediaNotReady;
    }
    return std::nullopt;
}

void DaemonMediaController::stopHeadphonesAudioCapture()
{
    m_headphones.sending = false;
    if (!m_headphones.sender) {
        return;
    }
    if (m_headphones.sender->isRunning()) {
        // As the main stream: the stream id's timeline continues from here.
        m_headphones.nextSequence = m_headphones.sender->nextSequence();
        m_headphones.nextTimestamp = m_headphones.sender->nextTimestamp();
    }
    m_headphones.sender->stop();
}

void DaemonMediaController::sendHeadphonesAudioContext(bool enabled,
                                                       RemoteAudioOffReason reason)
{
    if (!m_peer || m_headphones.revision == 0 || m_peer->headphonesAudioSsrc() == 0) {
        return;
    }
    const bool lossless = m_headphones.activeProfile == RemoteAudioProfile::Lossless;
    RemoteAudioContextMessage context;
    context.connectionId = m_peer->connectionId();
    context.revision = m_headphones.revision;
    context.generation = nextHeadphonesContextGeneration();
    context.enabled = enabled;
    context.ssrc = m_peer->headphonesAudioSsrc();
    context.firstSequence = m_headphones.nextSequence;
    context.firstTimestamp = m_headphones.nextTimestamp;
    if (enabled && lossless && m_headphones.sender) {
        context.losslessEncoder = m_headphones.sender->losslessProfile();
    } else if (enabled && m_headphones.sender) {
        context.encoder = m_headphones.sender->encoderProfile();
    }
    if (!enabled) {
        context.offReason = reason;
    }
    m_headphones.lastOffReason = context.offReason;
    context.profile = m_headphones.activeProfile;
    context.profileRefusal = m_headphones.profileRefusal;
    sendControl(encodeHeadphonesAudioContext(context));
}

void DaemonMediaController::onHeadphonesAudioPacket(DaemonAudioSender* sender,
                                                    const QByteArray& packet)
{
    // Emitted on the sender's owner thread. The identity checks keep a
    // stopped or retired stream from reaching a replacement peer.
    if (m_headphones.sender.get() != sender || !m_headphones.sending || !sender->isRunning()
        || m_epoch == 0 || !m_peer || !m_peer->isReady() || !m_headphonesMixNegotiated) {
        return;
    }
    sendAudioRtp(m_peer.get(), packet);
}

void DaemonMediaController::resetHeadphonesAudioSession()
{
    stopHeadphonesAudioCapture();
    if (m_headphones.sender) {
        // As a receiver stream: this can run inside the sender's own
        // packetReady emission, so it goes from the event loop.
        DaemonAudioSender* const sender = m_headphones.sender.release();
        sender->disconnect(this);
        sender->deleteLater();
    }
    // A different media peer has a different stream id and a new timeline.
    m_headphones = HeadphonesAudioStream{};
    m_headphonesMixNegotiated = false;
}

quint32 DaemonMediaController::nextHeadphonesContextGeneration()
{
    ++m_nextHeadphonesContextGeneration;
    if (m_nextHeadphonesContextGeneration == 0) {
        ++m_nextHeadphonesContextGeneration;
    }
    return m_nextHeadphonesContextGeneration;
}

bool DaemonMediaController::headphonesMixSending() const
{
    return m_headphones.sending && m_headphones.sender && m_headphones.sender->isRunning();
}

std::optional<RemoteAudioProfile> DaemonMediaController::headphonesMixProfile() const
{
    if (!headphonesMixSending()) {
        return std::nullopt;
    }
    return m_headphones.activeProfile;
}

bool DaemonMediaController::handleClockProbe(const QJsonObject& control, qint64 receivedNs)
{
    quint32 id = 0;
    const QJsonValue t0 = control.value(QStringLiteral("t0"));
    const qint64 sentNs = t0.isDouble() ? t0.toInteger(-1) : -1;
    if (!exactKeys(control, {"op", "connectionId", "id", "t0"}) || !m_peer
        || !canonicalConnectionId(control.value(QStringLiteral("connectionId")))
        || control.value(QStringLiteral("connectionId")).toString() != m_peer->connectionId()
        || !exactUnsigned(control.value(QStringLiteral("id")), id) || sentNs < 0) {
        return false;
    }
    // The newest captured block of the running audio context, so the GUI
    // can map what it plays onto this clock. Nothing while audio is off.
    qint64 generation = 0;
    qint64 rtpTimestamp = 0;
    qint64 capturedNs = 0;
    if (m_audioDiagnostics.activeContext && m_audioSender && m_audioSender->isRunning()) {
        const DaemonAudioSenderTelemetry sender = m_audioSender->telemetry();
        if (sender.hasCaptureStamp) {
            generation = m_audioDiagnostics.contextGeneration;
            rtpTimestamp = sender.captureTimestamp;
            capturedNs = sender.captureNs;
        }
    }
    return sendControl({
        {QStringLiteral("op"), QStringLiteral("clock-echo")},
        {QStringLiteral("connectionId"), m_peer->connectionId()},
        {QStringLiteral("id"), static_cast<qint64>(id)},
        {QStringLiteral("t0"), sentNs},
        {QStringLiteral("t1"), receivedNs},
        {QStringLiteral("generation"), generation},
        {QStringLiteral("rtpTimestamp"), rtpTimestamp},
        {QStringLiteral("capturedNs"), capturedNs},
        // Last, just before the reply leaves.
        {QStringLiteral("t2"), displayNowNs()},
    });
}

DaemonMediaController::AdmittedAudioProfile DaemonMediaController::admitProfile(
    RemoteAudioProfile requested) const
{
    AdmittedAudioProfile admitted;
    if (requested != RemoteAudioProfile::Lossless) {
        return admitted;
    }
    if (!m_audioLosslessAllowed) {
        admitted.refusal = RemoteAudioProfileRefusal::NotAllowed;
    } else if (m_peer && !m_peer->isReady()) {
        // The answer that settles the formats is not in yet; audio is
        // off as media-not-ready, and admission is decided on ready.
        admitted.active = RemoteAudioProfile::Lossless;
    } else if (!m_peer || !m_peer->losslessAudioNegotiated()
               || !PcmAudioPacketiser{}.isReady()) {
        admitted.refusal = RemoteAudioProfileRefusal::Unavailable;
    } else {
        admitted.active = RemoteAudioProfile::Lossless;
    }
    return admitted;
}

void DaemonMediaController::admitAudioProfile()
{
    const RemoteAudioProfile previous = m_audioActiveProfile;
    const std::optional<RemoteAudioProfileRefusal> previousRefusal = m_audioProfileRefusal;
    const AdmittedAudioProfile admitted = admitProfile(m_audioRequestedProfile);
    m_audioActiveProfile = admitted.active;
    m_audioProfileRefusal = admitted.refusal;
    if (m_audioProfileRefusal && m_audioProfileRefusal != previousRefusal) {
        qCInfo(lcDaemonMedia).noquote().nospace()
            << "lossless audio refused ("
            << remoteAudioProfileRefusalToWire(*m_audioProfileRefusal)
            << (*m_audioProfileRefusal == RemoteAudioProfileRefusal::NotAllowed
                    ? ": nereusd.conf audio_lossless = deny"
                    : ": the media description carries no L16/48000/2 rtpmap")
            << "); Opus keeps running, revision " << m_audioRevision;
    } else if (m_audioActiveProfile != previous) {
        qCInfo(lcDaemonMedia).noquote().nospace()
            << "audio profile " << remoteAudioProfileToWire(m_audioActiveProfile)
            << ", revision " << m_audioRevision;
    }
}

void DaemonMediaController::onSourceFrame(const DaemonSpectrumFrame& sharedFrame)
{
    // Task 76: DaemonSharedSpectrum took this frame once for every device.
    const MediaSourceKey key = sharedFrame.source;
    const DaemonSpectrumFrame* const sourceFrame = &sharedFrame;
    if (!m_radioModel || m_endpoints.empty()) {
        return;
    }

    // The source-worker frame is in raw FFT dBFS plus window compensation.
    // This controller and RadioModel share the station thread, so sample the
    // authoritative meter calibration here before either reducer quantizes it.
    // Reading it per frame keeps remote planes current across station preamp
    // and step-attenuator changes without reading RadioModel from the worker.
    // R-R3-46 / R-R3-11: the offset of the ADC this stream is on, so a pan
    // on the other ADC reads that ADC's attenuator, not slice A's.
    const double stationOffsetDb = m_radioModel->rxMeterOffsetDbForStream(key.streamIndex);
    if (!std::isfinite(stationOffsetDb)
        || !std::isfinite(sourceFrame->dbmOffset + stationOffsetDb)) {
        return;
    }
    const DaemonSpectrumFrame& frame = *sourceFrame;
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    const QList<quint32> ids = endpointIds();
    // Parity Task 31 (A11): keyed with display duplex on, the transmitting
    // pan's receive trace takes Thetis's keyed calibration (RX1Offset,
    // display.cs:4820-4850 [v2.10.3.15]) in place of the receive one.
    const bool keyedNow = m_txDisplayNegotiated && !m_txFeed.isNull() && m_txFeed->isKeyed();
    const double duplexOffsetDb = keyedNow ? m_radioModel->keyedDisplayOffsetDb(true) : 0.0;

    for (quint32 endpointId : ids) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        EndpointEntry& entry = it->second;
        if (!(entry.request.source == key)) {
            continue;
        }
        // Parity Task 28: an endpoint showing the transmit display gets no
        // receive frame until the fall.
        if (entry.txViewer) {
            continue;
        }
        entry.stationOffsetDb = keyedNow && entry.duplex && endpointOnTransmitPan(entry)
                                    && std::isfinite(duplexOffsetDb)
            ? duplexOffsetDb
            : stationOffsetDb;
        entry.latestInput = frame;
        const auto wideband = widebandContext(entry);
        if (!wideband) {
            entry.contextSent = false;
            continue; // Await capture enable acknowledgement, never ADC data.
        }
        if (!entry.endpoint.configured()
            || entry.endpoint.context().sourceGeneration != frame.generation
            || entry.endpoint.context().codec.contextGeneration == 0
            || entry.endpoint.context().wideband != *wideband) {
            const QPointer<DaemonMediaController> self(this);
            configureEndpointFromFrame(entry, frame);
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        }
    }

    bool needsNoiseFloor = false;
    for (const auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        if (entry.request.source == key && entry.contextSent && entry.endpoint.configured()
            && entry.endpoint.context().sourceGeneration == frame.generation
            && (entry.lastNoiseFloorTimestampNs < 0
                || frame.producedAtNs - entry.lastNoiseFloorTimestampNs
                    >= kNoiseFloorMinimumIntervalNs)) {
            needsNoiseFloor = true;
            break;
        }
    }
    if (needsNoiseFloor) {
        const std::optional<float> floorDbm = fullSourceNoiseFloor(*sourceFrame, stationOffsetDb);
        if (floorDbm.has_value() && m_peer) {
            for (quint32 endpointId : ids) {
                auto it = m_endpoints.find(endpointId);
                if (it == m_endpoints.end()) { continue; }
                EndpointEntry& entry = it->second;
                if (entry.request.source != key || !entry.contextSent || !entry.endpoint.configured()
                    || entry.endpoint.context().sourceGeneration != frame.generation
                    || (entry.lastNoiseFloorTimestampNs >= 0
                        && frame.producedAtNs - entry.lastNoiseFloorTimestampNs
                            < kNoiseFloorMinimumIntervalNs)) {
                    continue;
                }
                const QJsonObject message{
                    {QStringLiteral("op"), QStringLiteral("noise-floor")},
                    {QStringLiteral("connectionId"), m_peer->connectionId()},
                    {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
                    {QStringLiteral("revision"), static_cast<qint64>(entry.revision)},
                    {QStringLiteral("contextGeneration"), static_cast<qint64>(
                        entry.endpoint.context().codec.contextGeneration)},
                    {QStringLiteral("floorDbm"), *floorDbm},
                };
                const quint32 revision = entry.revision;
                const quint32 generation = entry.endpoint.context().codec.contextGeneration;
                if (entry.extras) {
                    // iPhone app Task 20: Clarity at the Core takes the same
                    // full-source floor a desktop window's Clarity is sent.
                    entry.extras->feedNoiseFloor(*floorDbm, displayNowNs() / 1'000'000);
                }
                const QPointer<DaemonMediaController> self(this);
                const bool sent = sendControl(message);
                if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
                it = m_endpoints.find(endpointId);
                if (sent && it != m_endpoints.end() && it->second.revision == revision
                    && it->second.endpoint.configured()
                    && it->second.endpoint.context().codec.contextGeneration == generation) {
                    it->second.lastNoiseFloorTimestampNs = frame.producedAtNs;
                }
            }
        }
    }

    for (auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        if (entry.request.source == key && !entry.txViewer
            && entry.endpoint.configured() && entry.contextSent
            && entry.endpoint.context().sourceGeneration == frame.generation) {
            entry.latestInput = frame;
        }
    }
}

void DaemonMediaController::configureEndpointFromFrame(EndpointEntry& entry,
                                                        const DaemonSpectrumFrame& frame)
{
    SpectrumEndpointSourceContext sourceContext;
    sourceContext.source = frame.source;
    sourceContext.sourceGeneration = frame.generation;
    sourceContext.fftBins = frame.binsLinear.size();
    sourceContext.centreHz = frame.centreHz;
    sourceContext.sampleRateHz = frame.sampleRateHz;
    const auto wideband = widebandContext(entry);
    if (!wideband) { entry.contextSent = false; return; }
    sourceContext.wideband = *wideband;
    sourceContext.contextGeneration = nextContextGeneration();
    if (!entry.endpoint.configure(entry.request, sourceContext)) {
        return;
    }
    // The frame carries the actual engine size and geometry; record what
    // this context really delivers.
    entry.grant.grantedFftSize = sourceContext.fftBins;
    entry.grant.grantedPixels = entry.endpoint.context().codec.traceSamples;
    // The reason names what the source or engine limits. A pan held to its
    // own admitted display charge is not told the receiver lacks detail.
    SpectrumEndpointRequest asked = entry.request;
    asked.pixels = entry.grant.requestedPixels;
    // The decimation the endpoint's engine runs at now (a lone pan's is
    // its own again once a neighbour leaves).
    entry.grant.grantedDecimation = entry.sourceDecimation;
    SpectrumGrant reasonGrant = entry.grant;
    reasonGrant.grantedPixels = SpectrumEndpoint::grantedPixels(
        asked, sourceContext.fftBins, sourceContext.centreHz, sourceContext.sampleRateHz,
        sourceContext.wideband.active);
    entry.grant.reason = grantReason(reasonGrant);
    entry.encoder.reset();
    entry.sourceCentreHz = frame.centreHz;
    entry.sourceSampleRateHz = frame.sampleRateHz;
    entry.latestInput = frame;
    entry.forceKeyframe = true;
    entry.contextSent = false;
    entry.lastNoiseFloorTimestampNs = -1;
    entry.pendingExtras.clear();
    entry.pendingExtrasSamples = 0;
    if (entry.extras) {
        entry.extras->newContext();
    }
    sendContext(entry);
}

std::optional<float> DaemonMediaController::fullSourceNoiseFloor(
    const DaemonSpectrumFrame& sourceFrame, double stationOffsetDb)
{
    if (!std::isfinite(sourceFrame.dbmOffset) || !std::isfinite(stationOffsetDb)
        || sourceFrame.binsLinear.isEmpty()) {
        return std::nullopt;
    }

    // Match FFTEngine::fftReady exactly: it floors tiny raw FFT powers to
    // -200 dBFS before station calibration, rather than adding the window
    // coherent-gain offset to an underflow bin. Clarity estimates the complete
    // source row before any endpoint crop, detector, averaging or quantization.
    QVector<float> binsDbm;
    binsDbm.reserve(sourceFrame.binsLinear.size());
    for (float power : sourceFrame.binsLinear) {
        if (!std::isfinite(power) || power < 0.0f) {
            return std::nullopt;
        }
        const double rawDbm = power < kFftPowerFloor
            ? static_cast<double>(kFftDbmFloor)
            : 10.0 * std::log10(static_cast<double>(power)) + sourceFrame.dbmOffset;
        const double calibratedDbm = rawDbm + stationOffsetDb;
        if (!std::isfinite(calibratedDbm)) {
            return std::nullopt;
        }
        binsDbm.append(static_cast<float>(calibratedDbm));
    }
    const float floorDbm = m_noiseFloorEstimator.estimate(binsDbm);
    if (!std::isfinite(floorDbm) || floorDbm < kNoiseFloorMinimumDbm
        || floorDbm > kNoiseFloorMaximumDbm) {
        return std::nullopt;
    }
    return floorDbm;
}

void DaemonMediaController::sendContext(EndpointEntry& entry)
{
    if (!m_peer || !entry.endpoint.configured()) {
        return;
    }
    const SpectrumEndpointContext& context = entry.endpoint.context();
    SpectrumContextMessage contextMessage;
    contextMessage.connectionId = m_peer->connectionId();
    contextMessage.endpointId = context.codec.endpointId;
    contextMessage.revision = entry.revision;
    contextMessage.contextGeneration = context.codec.contextGeneration;
    contextMessage.sourceStream = context.source.streamIndex;
    contextMessage.sourceCentreHz = entry.sourceCentreHz;
    contextMessage.sampleRateHz = entry.sourceSampleRateHz;
    contextMessage.centreHz = context.exactCentreHz;
    contextMessage.spanHz = context.exactSpanHz;
    contextMessage.wideCentreHz = context.wideCentreHz;
    contextMessage.wideSpanHz = context.wideSpanHz;
    contextMessage.traceSamples = context.codec.traceSamples;
    contextMessage.waterfallSamples = context.codec.waterfallSamples;
    contextMessage.wideSamples = context.codec.wideSamples;
    contextMessage.minDbm = context.codec.minDbm;
    contextMessage.maxDbm = context.codec.maxDbm;
    contextMessage.fps = context.targetFps;
    contextMessage.framesPerLine = context.framesPerLine;
    if (entry.widebandNegotiated) {
        contextMessage.wideband = context.wideband;
    }
    contextMessage.grant = spectrumContextGrant(entry.grant);
    // Parity Task 28: a peer that declared txDisplayVersion is told this is
    // the receiver's display; every other peer's context is unchanged.
    if (m_txDisplayNegotiated) {
        contextMessage.transmit = false;
    }
    // A minor-8 peer receives exactly the context it already parses.
    const QJsonObject message = encodeRemoteSpectrumContext(
        contextMessage, m_server && m_server->spectrumGrantAvailable(m_epoch));
    // A failed send can synchronously close the session. Copy identity before
    // crossing the transport and never retain an endpoint reference across it.
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    const quint32 endpointId = context.codec.endpointId;
    const quint32 revision = entry.revision;
    const quint32 generation = context.codec.contextGeneration;
    const QPointer<DaemonMediaController> self(this);
    const bool sent = sendControl(message);
    if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
    auto it = m_endpoints.find(endpointId);
    if (it != m_endpoints.end() && it->second.revision == revision
        && it->second.endpoint.configured()
        && it->second.endpoint.context().codec.contextGeneration == generation) {
        it->second.contextSent = sent;
    }
}

void DaemonMediaController::promoteLatestPs3Frame()
{
    m_ps3CurrentChunks = std::move(m_ps3LatestChunks);
    m_ps3LatestChunks.clear();
    m_ps3CurrentAttempted = false;
}

bool DaemonMediaController::trySendPs3(MediaPeer* peer, quint64 epoch, qint64 nowNs)
{
    if (m_ps3CurrentChunks.isEmpty()) {
        promoteLatestPs3Frame();
    }
    if (m_ps3CurrentChunks.isEmpty()) {
        return false;
    }
    const QByteArray chunk = m_ps3CurrentChunks.constFirst();
    if (chunk.isEmpty() || chunk.size() > static_cast<qsizetype>(kMaximumPs3DisplayChunkBytes)) {
        m_ps3CurrentChunks.clear();
        promoteLatestPs3Frame();
        return false;
    }
    // While the library still holds a display message the chunk would not
    // be taken. Keep it, spend nothing, and offer it again when the display
    // channel clears (displayWritable) or at a later tick.
    if (peer->displayBusy()) {
        return false;
    }
    if (displayPacingRequired()
        && (!m_displayPacerInitialized
            || !m_displayPacer.canSpendPs3(static_cast<quint64>(chunk.size()), nowNs))) {
        return false;
    }
    if (displayPacingRequired()
        && !m_displayPacer.spendPs3(static_cast<quint64>(chunk.size()), nowNs)) {
        return false;
    }
    m_ps3CurrentAttempted = true;
    m_ps3CurrentChunks.removeFirst();
    m_lastDisplayAttemptWasPs3 = true;
    const QPointer<DaemonMediaController> self(this);
    const IMediaTransport::DisplaySendResult result = peer->submitDisplay(chunk);
    if (!self || m_peer.get() != peer || m_epoch != epoch) {
        return true;
    }
    switch (result) {
    case IMediaTransport::DisplaySendResult::Sent:
        break;
    case IMediaTransport::DisplaySendResult::Queued:
        // Taken: the library sends it once SCTP has room.
        ++m_displayDiagnostics.displayQueuedLate;
        break;
    case IMediaTransport::DisplaySendResult::Busy:
        // Not taken, so nothing was sent: the same chunk goes next.
        ++m_displayDiagnostics.displaySendRefusals;
        m_ps3CurrentChunks.prepend(chunk);
        break;
    case IMediaTransport::DisplaySendResult::Refused:
        // Never retry a failed sequence or any of its remaining chunks.
        ++m_displayDiagnostics.displaySendRefusals;
        m_ps3CurrentChunks.clear();
        break;
    }
    if (m_ps3CurrentChunks.isEmpty()) {
        promoteLatestPs3Frame();
    }
    return true;
}

bool DaemonMediaController::trySendSpectrum(MediaPeer* peer, quint64 epoch, qint64 nowNs)
{
    // A frame would not be taken while the library holds a display message;
    // each endpoint keeps only its latest input until the channel clears.
    if (m_endpoints.empty() || peer->displayBusy()) {
        return false;
    }
    // iPhone app Task 20: a frame's display extras go on the next send,
    // before any other frame, so they arrive beside the frame they describe.
    if (trySendDisplayExtras(peer, epoch, nowNs)) {
        return true;
    }
    const QList<quint32> ids = endpointIds();
    // R-R3-08, R-R3-37: in a budget session, earliest deadline first.
    // Admission keeps the planned load within the sender's 200 messages a
    // second there, so ordering decides only whether each endpoint gets its
    // planned rate. Each endpoint holds only its latest frame and the
    // sender takes one message a tick, so at full use the endpoint whose
    // frame stops being worth sending first goes first: a pan that takes
    // every source frame loses one at the next frame, a slower pan only at
    // the end of its own period. The deadline is counted in whole source
    // frames after the held one, so endpoints of one source that fall due
    // on the same frame tie exactly. A tie goes to the faster endpoint (the
    // app plans its active pan fastest), then to round-robin order. A frame
    // consume() would drop sorts last.
    //
    // An older app's session (no budget) keeps plain round-robin. Nothing
    // limits what it asks for, so it can ask for more than the sender
    // carries, and there earliest deadline would starve pans: a pan whose
    // source frame falls due earlier always wins again, and pans on a
    // source whose frames arrive later never go (eight pans on two
    // sources got 60/60/0/0/60/20/0/0 fps). Round-robin gives each its
    // even share.
    const bool earliestDeadlineFirst = displayPacingRequired();
    struct Candidate {
        qint64 deadlineNs;
        int targetFps;
        int offset;
        int index;
    };
    QVarLengthArray<Candidate, 16> candidates;
    for (int offset = 0; offset < ids.size(); ++offset) {
        const int index = (m_roundRobinCursor + offset) % ids.size();
        const auto found = m_endpoints.find(ids.at(index));
        // Parity Task 28: an endpoint showing the transmit display is a
        // candidate with the analyzer's newest planes, due one of its own
        // frame periods after they arrived.
        if (found != m_endpoints.end() && found->second.txViewer) {
            const EndpointEntry& tx = found->second;
            if (!tx.txPending || !tx.txContextSent || tx.txFps <= 0) {
                continue;
            }
            if (!earliestDeadlineFirst) {
                candidates.append({0, 0, offset, index});
            } else {
                candidates.append({tx.txProducedAtNs + 1'000'000'000LL / tx.txFps, tx.txFps,
                                   offset, index});
            }
            continue;
        }
        if (found == m_endpoints.end() || !found->second.latestInput.has_value()
            || !found->second.contextSent) {
            continue;
        }
        const SpectrumEndpoint& endpoint = found->second.endpoint;
        if (!earliestDeadlineFirst) {
            candidates.append({0, 0, offset, index});
            continue;
        }
        const qint64 producedAtNs = found->second.latestInput->producedAtNs;
        const std::optional<qint64> deadlineNs = endpoint.outputDeadlineNs(producedAtNs);
        qint64 key = std::numeric_limits<qint64>::max();
        if (deadlineNs) {
            key = *deadlineNs;
            const auto source = m_sources.constFind(found->second.request.source);
            if (source != m_sources.cend() && source->config.fft.fps > 0) {
                const qint64 sourcePeriodNs = 1'000'000'000LL / source->config.fft.fps;
                const qint64 frames = std::max<qint64>(
                    0, (*deadlineNs - producedAtNs + sourcePeriodNs / 2) / sourcePeriodNs);
                key = producedAtNs + frames * sourcePeriodNs;
            }
        }
        candidates.append({key, endpoint.context().targetFps, offset, index});
    }
    if (earliestDeadlineFirst) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) {
                      if (a.deadlineNs != b.deadlineNs) {
                          return a.deadlineNs < b.deadlineNs;
                      }
                      if (a.targetFps != b.targetFps) { return a.targetFps > b.targetFps; }
                      return a.offset < b.offset;
                  });
    }
    for (const Candidate& candidate : candidates) {
        const int index = candidate.index;
        const quint32 endpointId = ids.at(index);
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        EndpointEntry& entry = it->second;
        if (entry.txViewer) {
            const int cursor = m_roundRobinCursor;
            m_roundRobinCursor = (index + 1) % ids.size();
            if (trySendTransmitFrame(endpointId, peer, epoch, nowNs)) {
                return true;
            }
            m_roundRobinCursor = cursor;
            continue;
        }
        if (!entry.latestInput.has_value() || !entry.contextSent) {
            continue;
        }
        if (displayPacingRequired()
            && (!m_displayPacerInitialized
                || !m_displayPacer.canSpendSpectrum(entry.displayCost.maximumFrameBytes,
                                                    entry.displayCost.maximumFrameSampleUnits,
                                                    nowNs))) {
            continue;
        }
        const DaemonSpectrumFrame frame = *entry.latestInput;
        const quint32 revision = entry.revision;
        const auto currentWideband = widebandContext(entry);
        if (!currentWideband) { entry.contextSent = false; continue; }
        if (*currentWideband != entry.endpoint.context().wideband) {
            const QPointer<DaemonMediaController> self(this);
            configureEndpointFromFrame(entry, frame);
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return true; }
        }
        // The reliable context send may also retire/rebind this endpoint.
        it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end() || it->second.revision != revision
            || !it->second.contextSent) { continue; }
        EndpointEntry& current = it->second;
        if (displayPacingRequired()
            && (!m_displayPacerInitialized
                || !m_displayPacer.canSpendSpectrum(current.displayCost.maximumFrameBytes,
                                                    current.displayCost.maximumFrameSampleUnits,
                                                    nowNs))) {
            continue;
        }
        current.latestInput.reset();
        const auto adcFrame = currentWideband->active && m_radioModel
            ? m_radioModel->latestWidebandSpectrum(currentWideband->physicalAdcIndex)
            : std::nullopt;
        std::optional<DisplayCodecFrame> reduced =
            current.endpoint.consume(frame, current.stationOffsetDb, adcFrame);
        m_roundRobinCursor = (index + 1) % ids.size();
        if (!reduced.has_value()) {
            continue;
        }
        // iPhone app Task 20 (R-IOS-27): the display extras run on the frame
        // as reduced, as the desktop's SpectrumWidget runs them on its own;
        // then calibration and normalise move every plane, so the app draws
        // what it receives.
        std::optional<DisplayExtrasFrame> extrasFrame;
        if (current.extras) {
            const double binWidthHz = frame.binsLinear.isEmpty()
                ? 0.0 : frame.sampleRateHz / static_cast<double>(frame.binsLinear.size());
            if (current.extrasRequest.sections() != 0) {
                extrasFrame = current.extras->process(
                    *reduced, displayExtrasInputs(current, binWidthHz, nowNs));
            }
            const float shift = current.extras->displayShiftDb(
                binWidthHz, static_cast<int>(current.request.trace.detector));
            if (shift != 0.0f) {
                for (QVector<float>* plane : {&reduced->traceDbm, &reduced->waterfallDbm,
                                              &reduced->wideDbm}) {
                    for (float& value : *plane) { value += shift; }
                }
            }
        }
        const QByteArray bytes = current.encoder.encode(*reduced, current.forceKeyframe);
        const bool keyframe = current.encoder.lastEncodedKeyframe();
        const quint32 generation = current.endpoint.context().codec.contextGeneration;
        const quint64 samples = static_cast<quint64>(reduced->traceDbm.size())
            + static_cast<quint64>(reduced->waterfallDbm.size())
            + static_cast<quint64>(reduced->wideDbm.size());
        if (bytes.isEmpty()) {
            continue;
        }
        if (bytes.size() > static_cast<qsizetype>(current.displayCost.maximumFrameBytes)
            || samples > current.displayCost.maximumFrameSampleUnits
            || (displayPacingRequired()
                && !m_displayPacer.spendSpectrum(static_cast<quint64>(bytes.size()),
                                                 samples, nowNs))) {
            qCWarning(lcDaemonMedia) << "spectrum codec exceeded admitted display cost";
            current.forceKeyframe = true;
            continue;
        }
        current.forceKeyframe = false;
        m_lastDisplayAttemptWasPs3 = false;
        const QPointer<DaemonMediaController> self(this);
        const IMediaTransport::DisplaySendResult result = peer->submitDisplay(bytes);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return true; }
        const bool sent = result == IMediaTransport::DisplaySendResult::Sent
            || result == IMediaTransport::DisplaySendResult::Queued;
        if (sent) {
            // A queued frame is still delivered, so it is measured and the
            // delta chain stands.
            recordDisplaySent(bytes, keyframe);
            if (result == IMediaTransport::DisplaySendResult::Queued) {
                ++m_displayDiagnostics.displayQueuedLate;
            }
            it = m_endpoints.find(endpointId);
            if (extrasFrame && it != m_endpoints.end() && it->second.revision == revision
                && it->second.endpoint.configured()
                && it->second.endpoint.context().codec.contextGeneration == generation) {
                // The newest frame's extras replace any not yet sent.
                it->second.pendingExtras =
                    encodeDisplayExtras(*extrasFrame, it->second.endpoint.context().codec);
                it->second.pendingExtrasSamples = extrasFrame->peakHoldDbm
                    ? static_cast<quint64>(extrasFrame->peakHoldDbm->size()) : 0;
            }
        } else {
            // Not taken (busy or failed): this frame is gone, never queued
            // behind the link.
            ++m_displayDiagnostics.displaySendRefusals;
        }
        it = m_endpoints.find(endpointId);
        if (!sent && it != m_endpoints.end() && it->second.revision == revision
            && it->second.endpoint.configured()
            && it->second.endpoint.context().codec.contextGeneration == generation) {
            // The receiver never gets this frame: do not resend it, and
            // restart the delta chain from a keyframe.
            it->second.forceKeyframe = true;
        }
        return true;
    }
    return false;
}

DisplayExtrasInputs DaemonMediaController::displayExtrasInputs(const EndpointEntry& entry,
                                                                double binWidthHz,
                                                                qint64 nowNs) const
{
    DisplayExtrasInputs inputs;
    const SpectrumEndpointContext& context = entry.endpoint.context();
    inputs.fps = context.targetFps;
    inputs.nowMs = nowNs / 1'000'000;
    inputs.centreHz = context.exactCentreHz;
    inputs.spanHz = context.exactSpanHz;
    inputs.binWidthHz = binWidthHz;
    inputs.traceDetector = static_cast<int>(entry.request.trace.detector);
    if (m_radioModel) {
        if (const SliceModel* slice = m_radioModel->sliceById(entry.sliceId)) {
            inputs.sliceHz = slice->frequency();
            inputs.filterLowHz = slice->filterLow();
            inputs.filterHighHz = slice->filterHigh();
            inputs.band = static_cast<int>(bandFromFrequency(slice->frequency()));
        }
        inputs.mox = m_radioModel->transmitModel().isMox();
        if (const SliceModel* slice = m_radioModel->sliceById(entry.sliceId)) {
            inputs.transmitting = inputs.mox && slice->isTxSlice();
        }
    }
    return inputs;
}

bool DaemonMediaController::trySendDisplayExtras(MediaPeer* peer, quint64 epoch, qint64 nowNs)
{
    const QList<quint32> ids = endpointIds();
    for (int offset = 0; offset < ids.size(); ++offset) {
        const quint32 endpointId = ids.at((m_roundRobinCursor + offset) % ids.size());
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end() || it->second.pendingExtras.isEmpty()) {
            continue;
        }
        const QByteArray bytes = std::exchange(it->second.pendingExtras, {});
        const quint64 samples = std::exchange(it->second.pendingExtrasSamples, 0);
        if (displayPacingRequired()
            && (!m_displayPacerInitialized
                || !m_displayPacer.spendSpectrum(static_cast<quint64>(bytes.size()),
                                                 samples, nowNs))) {
            // No credit now: the extras of a frame already on its way are
            // worth nothing later, and the next frame brings its own.
            continue;
        }
        m_lastDisplayAttemptWasPs3 = false;
        const QPointer<DaemonMediaController> self(this);
        const IMediaTransport::DisplaySendResult result = peer->submitDisplay(bytes);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return true; }
        if (result == IMediaTransport::DisplaySendResult::Queued) {
            ++m_displayDiagnostics.displayQueuedLate;
        } else if (result != IMediaTransport::DisplaySendResult::Sent) {
            // Never resent: the next frame carries its own extras.
            ++m_displayDiagnostics.displaySendRefusals;
        }
        return true;
    }
    return false;
}

void DaemonMediaController::onSendTick()
{
    if (!m_peer || !m_peer->isReady()) {
        return;
    }
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    if (!ps3DisplayHere()) {
        m_ps3CurrentChunks.clear();
        m_ps3LatestChunks.clear();
        m_ps3CurrentAttempted = false;
    }
    const qint64 nowNs = displayNowNs();
    const QPointer<DaemonMediaController> self(this);
    // A continuously pending PureSignal snapshot must leave the next
    // writable display slot to spectrum. Whichever display kind last sent
    // yields first, with the other kind as fallback when it has no frame.
    if (m_lastDisplayAttemptWasPs3) {
        const bool sentSpectrum = trySendSpectrum(peer, epoch, nowNs);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        if (!sentSpectrum) {
            trySendPs3(peer, epoch, nowNs);
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        }
    } else {
        const bool sentPs3 = trySendPs3(peer, epoch, nowNs);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        if (!sentPs3) {
            trySendSpectrum(peer, epoch, nowNs);
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        }
    }
    for (int attempt = 0; attempt < 16; ++attempt) {
        const bool sentIq = trySendIq(peer, epoch, nowNs);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        if (!sentIq) { break; }
    }
}

bool DaemonMediaController::trySendIq(MediaPeer* peer, quint64 epoch, qint64 nowNs)
{
    if (!m_iqNegotiated || !peer || !peer->isReady()) { return false; }
    const auto fail = [this, peer, epoch](int sliceId, IqStream& stream,
                                          const QString& reason, bool budgetRefused = false) {
        stopIqStream(stream);
        stream.budgetRefused = budgetRefused;
        const QPointer<DaemonMediaController> self(this);
        sendIqContext(sliceId, stream, reason);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        refreshDisplayBudgetPacer();
        if (m_server) { m_server->publishDisplayBudgetCapabilities(); }
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        if (budgetRefused) {
            const auto found = m_iqStreams.find(sliceId);
            if (found != m_iqStreams.end() && found->second.budgetRefused) {
                found->second.lastBudgetRefusalRevision = m_iqBudgetChangeRevision;
            }
        }
    };
    for (auto& [sliceId, stream] : m_iqStreams) {
        if (!stream.sending || !stream.ingress) { continue; }
        if (!iqFitsCurrentShare()) {
            fail(sliceId, stream,
                 QStringLiteral("The link to the Core is too busy to send raw I/Q for this receiver."),
                 true);
            return false;
        }
        if (stream.ingress->failed.load(std::memory_order_acquire)) {
            fail(sliceId, stream, QStringLiteral("Raw I/Q capture overflowed on the Core."));
            return false;
        }
        if (stream.pending.isEmpty()) {
            const auto samples = stream.ingress->pop();
            if (!samples) { continue; }
            const auto encoded = RemoteIqCodec::encode(
                static_cast<quint32>(sliceId), stream.generation, stream.sequence, *samples);
            if (!encoded) {
                fail(sliceId, stream, QStringLiteral("Invalid raw I/Q samples from the Core."));
                return false;
            }
            stream.pending = *encoded;
        }
        if (peer->iqBusy()) {
            if (stream.blockedAtNs < 0) { stream.blockedAtNs = nowNs; }
            if (nowNs - stream.blockedAtNs >= 250'000'000LL) {
                fail(sliceId, stream,
                     QStringLiteral("Raw I/Q stopped because the media link stalled."));
            }
            return false;
        }
        if (!stream.pendingDebited && displayPacingRequired()
            && (!m_displayPacerInitialized
                || !m_displayPacer.spendIq(quint64(stream.pending.size()), nowNs))) {
            if (stream.blockedAtNs < 0) { stream.blockedAtNs = nowNs; }
            if (nowNs - stream.blockedAtNs >= 250'000'000LL) {
                fail(sliceId, stream,
                     QStringLiteral("Raw I/Q stopped because the display share was exhausted."));
            }
            return false;
        }
        if (displayPacingRequired()) { stream.pendingDebited = true; }
        const QPointer<DaemonMediaController> self(this);
        const auto result = peer->submitIq(stream.pending);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return false; }
        if (result == IMediaTransport::DisplaySendResult::Busy) {
            if (stream.blockedAtNs < 0) { stream.blockedAtNs = nowNs; }
            if (nowNs - stream.blockedAtNs >= 250'000'000LL) {
                fail(sliceId, stream,
                     QStringLiteral("Raw I/Q stopped because the media link stalled."));
            }
            return false;
        }
        if (result == IMediaTransport::DisplaySendResult::Refused) {
            fail(sliceId, stream, QStringLiteral("The raw I/Q media channel refused a frame."));
            return false;
        }
        stream.pending.clear();
        stream.pendingDebited = false;
        stream.blockedAtNs = -1;
        ++stream.sequence;
        return true;
    }
    return false;
}

// ---- Parity Task 28 (R-R3-49, A11): the transmit display --------------------

bool DaemonMediaController::transmitDisplayActive(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    return it != m_endpoints.end() && it->second.txViewer != nullptr;
}

void DaemonMediaController::wireTxDisplayFeed()
{
    TxDisplayFeed* feed = m_radioModel ? m_radioModel->txDisplayFeed() : nullptr;
    if (feed != m_txFeed.data()) {
        if (!m_txFeed.isNull()) {
            disconnect(m_txFeed.data(), nullptr, this, nullptr);
        }
        m_txFeed = feed;
        if (feed != nullptr) {
            // Queued: an endpoint's viewer is given back from inside the
            // endpoint map's own erase, and the feed answers at once.
            const auto reconcile = [this]() { reconcileTransmitDisplay(); };
            // Direct: the pan is recorded on the edge itself, before a
            // rebinding or a layout change queued behind it can move the
            // transmit slice.
            connect(feed, &TxDisplayFeed::keyedChanged, this,
                    [this](bool keyed) { recordTransmitPan(keyed); });
            connect(feed, &TxDisplayFeed::keyedChanged, this, reconcile, Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::viewChanged, this, reconcile, Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::governorChanged, this, reconcile,
                    Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::traceReady, this,
                    [this](const QVector<float>& dbm) { onTransmitPlane(dbm, false); },
                    Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::waterfallReady, this,
                    [this](const QVector<float>& dbm) { onTransmitPlane(dbm, true); },
                    Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::miniViewChanged, this, reconcile,
                    Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::miniTraceReady, this,
                    [this](const QVector<float>& dbm) { onTransmitPlane(dbm, false, true); },
                    Qt::QueuedConnection);
            connect(feed, &TxDisplayFeed::miniWaterfallReady, this,
                    [this](const QVector<float>& dbm) { onTransmitPlane(dbm, true, true); },
                    Qt::QueuedConnection);
        }
    }
    // Which pan transmits follows the transmit slice.
    if (!m_txArbiterConnection && m_radioModel) {
        if (TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter()) {
            m_txArbiterConnection = connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this,
                                            [this](int, int) { reconcileTransmitDisplay(); },
                                            Qt::QueuedConnection);
        }
    }
}

void DaemonMediaController::recordTransmitPan(bool keyed)
{
    if (!keyed) {
        m_txRiseRecorded = false;
        m_txRisePanKey.clear();
        m_txRiseSliceId = -1;
        return;
    }
    if (m_txRiseRecorded || !m_radioModel) {
        return;
    }
    const SliceModel* const tx = m_radioModel->txBoundSlice();
    if (tx == nullptr) {
        return;
    }
    m_txRiseRecorded = true;
    m_txRisePanKey = tx->panKey();
    m_txRiseSliceId = tx->sliceIndex();
}

bool DaemonMediaController::endpointDuplex(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    return it != m_endpoints.end() && it->second.duplex;
}

double DaemonMediaController::endpointDisplayOffsetDb(quint32 endpointId) const
{
    const auto it = m_endpoints.find(endpointId);
    return it != m_endpoints.end() ? it->second.stationOffsetDb : 0.0;
}

void DaemonMediaController::refreshDeviceDisplayDuplex()
{
    if (!m_radioModel) {
        return;
    }
    bool duplex = false;
    for (const auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        duplex = duplex || entry.duplex;
    }
    const QByteArray device = duplex && m_server ? m_server->mediaSessionDevice(m_epoch)
                                                 : QByteArray();
    if (!m_duplexDevice.isEmpty() && m_duplexDevice != device) {
        m_radioModel->setDeviceDisplayDuplex(m_duplexDevice, false);
        m_duplexDevice.clear();
    }
    if (!device.isEmpty() && m_duplexDevice != device) {
        m_radioModel->setDeviceDisplayDuplex(device, true);
        m_duplexDevice = device;
    }
}

bool DaemonMediaController::endpointOnTransmitPan(const EndpointEntry& entry) const
{
    if (!m_radioModel || !m_txRiseRecorded) {
        return false;
    }
    // From Thetis MeterManager.cs:5252-5265,43659-43666 [v2.10.3.15]:
    // meter and mini MOX follows its receiver, while a pan can host several.
    if (entry.miniDisplay) {
        return entry.sliceId == m_txRiseSliceId;
    }
    // The pan hosting the transmitting slice at the rise, as the window
    // takes it over (MoxDisplayController::rise records it and restores
    // THAT pan at the fall): the transmit slice, or a slice sharing its pan
    // on the Core. Recorded rather than re-resolved, so a layout change
    // that rehomes slices, or a rebinding, while keyed leaves the transmit
    // display where the window shows it until the fall.
    const SliceModel* const slice = m_radioModel->sliceById(entry.sliceId);
    if (slice == nullptr) {
        return false;
    }
    if (m_txRisePanKey.isEmpty()) {
        return slice->sliceIndex() == m_txRiseSliceId;
    }
    return slice->panKey() == m_txRisePanKey;
}

std::optional<QJsonObject> DaemonMediaController::transmitContextFor(
    const EndpointEntry& entry) const
{
    if (!m_peer || m_txFeed.isNull() || !entry.txViewer) {
        return std::nullopt;
    }
    const bool mini = entry.miniDisplay;
    const TxDisplayView view = mini ? m_txFeed->miniView() : m_txFeed->currentView();
    if (view.empty() || (mini && !m_txFeed->miniReady())) {
        return std::nullopt;
    }
    // The view's pixels, never more than the endpoint was admitted for (a
    // viewer that does not govern shares the governing one's view).
    const int samples = std::clamp(
        view.pixels > 0 ? std::min(view.pixels, entry.request.pixels) : entry.request.pixels,
        1, SpectrumEndpoint::kMaxPixels);
    // The analyzer's output rate, never faster than the endpoint asked.
    const int fps = std::clamp(std::min(std::max(1, mini ? m_txFeed->miniOutputFps()
                                                       : m_txFeed->outputFps()),
                                        entry.request.targetFps),
                               1, 60);
    const std::pair<float, float> window =
        entry.txWindow.value_or(std::pair{entry.request.minDbm, entry.request.maxDbm});
    SpectrumContextMessage message;
    message.connectionId = m_peer->connectionId();
    message.endpointId = entry.request.endpointId;
    message.revision = entry.revision;
    message.contextGeneration = 0; // given when it is sent
    message.sourceStream = entry.request.source.streamIndex;
    message.sourceCentreHz = view.carrierHz;
    message.sampleRateHz = static_cast<double>(WdspEngine::kTxDspSampleRate);
    message.centreHz = view.centreHz();
    message.spanHz = view.spanHz();
    message.traceSamples = samples;
    message.waterfallSamples = samples;
    message.wideSamples = 0;
    message.minDbm = window.first;
    message.maxDbm = window.second;
    message.fps = fps;
    // The analyzer hands one waterfall row with each frame.
    message.framesPerLine = 1;
    if (entry.widebandNegotiated) {
        message.wideband = WidebandDisplayContext{};
    }
    SpectrumContextGrant grant;
    grant.grantedFftSize = std::clamp(mini ? m_txFeed->miniFftSize() : m_txFeed->fftSize(),
                                     1, FFTEngine::maximumFftSize());
    grant.grantedTier = entry.request.source.tier;
    grant.requestedPixels = std::max(entry.grant.requestedPixels, samples);
    grant.grantedPixels = samples;
    grant.limit = (mini || m_txFeed->isGoverning(entry.txViewer->id))
        ? SpectrumLimitReason::None : SpectrumLimitReason::SharedEngine;
    message.grant = grant;
    message.transmit = true;
    return encodeRemoteSpectrumContext(message, /*grantNegotiated=*/true);
}

void DaemonMediaController::reconcileTransmitDisplay()
{
    if (!m_peer) {
        return;
    }
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    TxDisplayFeed* const feed = m_txFeed.data();
    const bool keyed = m_txDisplayNegotiated && feed != nullptr && feed->isKeyed();
    // A feed keyed before this controller watched it (a peer that started
    // mid-key) records its pan here.
    recordTransmitPan(feed != nullptr && feed->isKeyed());
    for (quint32 endpointId : endpointIds()) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        EndpointEntry& entry = it->second;
        // Parity Task 31: an endpoint subscribed with `duplex` keeps the
        // receiver while keyed (Thetis DisplayThread, console.cs:24281-24338
        // [v2.10.3.15]: `if (bLocalMox && !_display_duplex)`).
        const bool wanted = keyed && !entry.duplex && endpointOnTransmitPan(entry);
        if (wanted && !entry.txViewer) {
            // The rise, for this endpoint: no receive frame from now until
            // the fall. It views the transmit display centred on the carrier
            // at its own span, as the local window re-centres the
            // transmitting pan on the carrier at the rise.
            auto lease = std::make_unique<TxViewerLease>();
            lease->feed = feed;
            lease->id = feed->addViewer(feed->currentView().carrierHz, entry.request.spanHz,
                                        entry.request.pixels, /*local=*/false,
                                        entry.miniDisplay);
            entry.txViewer = std::move(lease);
            entry.endpoint.reset();
            entry.contextSent = false;
            entry.latestInput.reset();
            entry.pendingExtras.clear();
            entry.pendingExtrasSamples = 0;
            entry.txSentShape = {};
            entry.txContextSent = false;
            entry.txPending = false;
            entry.txTrace.clear();
            entry.txWaterfall.clear();
        } else if (!wanted && entry.txViewer) {
            // The fall: back to the receiver. The next source frame
            // configures the endpoint for the request it holds (the receive
            // view it had before the rise) and sends its context, `transmit`
            // false; then receive frames.
            entry.txViewer.reset();
            entry.txSentShape = {};
            entry.txContextSent = false;
            entry.txPending = false;
            entry.txTrace.clear();
            entry.txWaterfall.clear();
            entry.endpoint.reset();
            entry.contextSent = false;
            entry.latestInput.reset();
            entry.forceKeyframe = true;
        }
    }
    // Each viewer's context follows the view and whether it governs.
    for (quint32 endpointId : endpointIds()) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end() || !it->second.txViewer) { continue; }
        std::optional<QJsonObject> message = transmitContextFor(it->second);
        if (!message) { continue; }
        QJsonObject shape = *message;
        shape.remove(QStringLiteral("contextGeneration"));
        EndpointEntry& entry = it->second;
        if (entry.txContextSent && shape == entry.txSentShape) { continue; }
        const quint32 generation = nextContextGeneration();
        message->insert(QStringLiteral("contextGeneration"), static_cast<qint64>(generation));
        const int samples = message->value(QStringLiteral("traceSamples")).toInt();
        entry.txSentShape = shape;
        entry.txCodec = DisplayCodecContext{};
        entry.txCodec.endpointId = endpointId;
        entry.txCodec.contextGeneration = generation;
        entry.txCodec.minDbm = static_cast<float>(message->value(QStringLiteral("minDbm")).toDouble());
        entry.txCodec.maxDbm = static_cast<float>(message->value(QStringLiteral("maxDbm")).toDouble());
        entry.txCodec.traceSamples = static_cast<quint16>(samples);
        entry.txCodec.waterfallSamples = static_cast<quint16>(samples);
        entry.txCodec.wideSamples = 0;
        entry.txFps = message->value(QStringLiteral("fps")).toInt();
        entry.txSequence = 0;
        entry.txNextDueNs = 0;
        entry.txPending = false;
        entry.txTrace.clear();
        entry.txWaterfall.clear();
        entry.txNewWaterfall = false;
        entry.encoder.reset();
        entry.forceKeyframe = true;
        entry.txContextSent = false;
        const quint32 revision = entry.revision;
        const QPointer<DaemonMediaController> self(this);
        const bool sent = sendControl(*message);
        if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        it = m_endpoints.find(endpointId);
        if (it != m_endpoints.end() && it->second.revision == revision && it->second.txViewer
            && it->second.txCodec.contextGeneration == generation) {
            it->second.txContextSent = sent;
        }
    }
}

void DaemonMediaController::onTransmitPlane(const QVector<float>& dbm, bool waterfall,
                                            bool mini)
{
    if (!m_peer || m_txFeed.isNull() || !m_txFeed->isKeyed()) {
        return;
    }
    // A poll made before the analyzer's pixel count last changed describes
    // another view: drop it rather than draw it across this one.
    if (dbm.size() != (mini ? m_txFeed->miniView().pixels
                           : m_txFeed->currentView().pixels)) {
        return;
    }
    const qint64 nowNs = displayNowNs();
    // Parity Task 31 (A11): the transmit display's calibration, Thetis
    // tx_display_cal_offset (RX1Offset keyed with display duplex off,
    // display.cs:4829-4832 [v2.10.3.15]; Setup > Calibration's TX Display
    // Cal Offset, setup.cs:14364 [v2.10.3.15]). A remote pan adds none of
    // its own, so the Core adds it here, as it adds the receive one.
    const float txCalDb = m_radioModel
        ? static_cast<float>(m_radioModel->keyedDisplayOffsetDb(false)) : 0.0f;
    QVector<float> calibrated;
    const QVector<float>* source = &dbm;
    if (std::isfinite(txCalDb) && txCalDb != 0.0f) {
        calibrated = dbm;
        for (float& value : calibrated) {
            value += txCalDb;
        }
        source = &calibrated;
    }
    for (auto& [endpointId, entry] : m_endpoints) {
        Q_UNUSED(endpointId);
        if (!entry.txViewer || !entry.txContextSent || entry.miniDisplay != mini) {
            continue;
        }
        QVector<float> plane = reduceTransmitPlane(*source, entry.txCodec.traceSamples,
                                                   entry.txCodec.minDbm);
        if (plane.isEmpty()) {
            continue;
        }
        if (waterfall) {
            entry.txWaterfall = std::move(plane);
            entry.txNewWaterfall = true;
        } else {
            entry.txTrace = std::move(plane);
        }
        entry.txPending = true;
        entry.txProducedAtNs = std::max(nowNs, entry.txProducedAtNs + 1);
    }
}

bool DaemonMediaController::trySendTransmitFrame(quint32 endpointId, MediaPeer* peer,
                                                 quint64 epoch, qint64 nowNs)
{
    auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return false;
    }
    EndpointEntry& entry = it->second;
    if (!entry.txViewer || !entry.txPending || !entry.txContextSent || entry.txFps <= 0) {
        return false;
    }
    // The endpoint's own cadence, as a receive endpoint keeps one: an
    // advancing schedule with 5% early tolerance.
    const qint64 periodNs = 1'000'000'000LL / entry.txFps;
    if (entry.txNextDueNs != 0 && nowNs < entry.txNextDueNs - periodNs / 20) {
        return false;
    }
    // Charged to the display budget exactly as a receive frame of its size.
    if (displayPacingRequired()
        && (!m_displayPacerInitialized
            || !m_displayPacer.canSpendSpectrum(entry.displayCost.maximumFrameBytes,
                                                entry.displayCost.maximumFrameSampleUnits,
                                                nowNs))) {
        return false;
    }
    DisplayCodecFrame frame;
    frame.context = entry.txCodec;
    frame.encoderSequence = entry.txSequence++;
    frame.producerTimestamp = static_cast<quint64>(std::max<qint64>(0, entry.txProducedAtNs));
    frame.waterfallAdvance = entry.txNewWaterfall;
    frame.traceDbm = entry.txTrace.isEmpty() ? entry.txWaterfall : entry.txTrace;
    frame.waterfallDbm = entry.txWaterfall.isEmpty() ? entry.txTrace : entry.txWaterfall;
    entry.txPending = false;
    entry.txNewWaterfall = false;
    const QByteArray bytes = entry.encoder.encode(frame, entry.forceKeyframe);
    const bool keyframe = entry.encoder.lastEncodedKeyframe();
    const quint64 samples = static_cast<quint64>(frame.traceDbm.size())
        + static_cast<quint64>(frame.waterfallDbm.size());
    if (bytes.isEmpty()) {
        return false;
    }
    if (bytes.size() > static_cast<qsizetype>(entry.displayCost.maximumFrameBytes)
        || samples > entry.displayCost.maximumFrameSampleUnits
        || (displayPacingRequired()
            && !m_displayPacer.spendSpectrum(static_cast<quint64>(bytes.size()), samples,
                                             nowNs))) {
        qCWarning(lcDaemonMedia) << "transmit display frame exceeded admitted display cost";
        entry.forceKeyframe = true;
        return false;
    }
    entry.forceKeyframe = false;
    const qint64 followingDueNs = entry.txNextDueNs + periodNs;
    entry.txNextDueNs = entry.txNextDueNs == 0 || nowNs >= followingDueNs
        ? nowNs + periodNs : followingDueNs;
    const quint32 revision = entry.revision;
    const quint32 generation = entry.txCodec.contextGeneration;
    m_lastDisplayAttemptWasPs3 = false;
    const QPointer<DaemonMediaController> self(this);
    const IMediaTransport::DisplaySendResult result = peer->submitDisplay(bytes);
    if (!self || m_peer.get() != peer || m_epoch != epoch) {
        return true;
    }
    const bool sent = result == IMediaTransport::DisplaySendResult::Sent
        || result == IMediaTransport::DisplaySendResult::Queued;
    if (sent) {
        recordDisplaySent(bytes, keyframe);
        if (result == IMediaTransport::DisplaySendResult::Queued) {
            ++m_displayDiagnostics.displayQueuedLate;
        }
        return true;
    }
    ++m_displayDiagnostics.displaySendRefusals;
    it = m_endpoints.find(endpointId);
    if (it != m_endpoints.end() && it->second.revision == revision && it->second.txViewer
        && it->second.txCodec.contextGeneration == generation) {
        // Never resent: the delta chain restarts from a keyframe.
        it->second.forceKeyframe = true;
    }
    return true;
}

bool DaemonMediaController::reconcileWidebandDemand(EndpointEntry& entry)
{
    if (!m_radioModel) { return false; }
    const int adc = m_radioModel->sliceAdcIndex(entry.sliceId);
    const auto rate = m_radioModel->widebandAdcRateHz(adc);
    const double centre = m_radioModel->streamCentreHz(entry.request.source.streamIndex);
    const double ddcRate = m_radioModel->streamSampleRateHz(entry.request.source.streamIndex);
    entry.widebandWanted = entry.widebandNegotiated && entry.request.extendedView
        && rate.has_value() && needsWideband(entry.request, centre, ddcRate);
    if (!entry.widebandWanted) {
        entry.widebandDemand.reset();
        return true;
    }
    if (!entry.widebandDemand) {
        const auto token = m_radioModel->acquireWidebandDemand(entry.sliceId);
        if (!token) { return false; }
        entry.widebandDemand = std::make_unique<WidebandDemandLease>();
        entry.widebandDemand->model = m_radioModel;
        entry.widebandDemand->token = token;
    }
    return m_radioModel->setWidebandDemandActive(entry.widebandDemand->token, true);
}

std::optional<WidebandDisplayContext>
DaemonMediaController::widebandContext(const EndpointEntry& entry) const
{
    WidebandDisplayContext context;
    if (!entry.widebandNegotiated || !m_radioModel) { return context; }
    const int adc = m_radioModel->sliceAdcIndex(entry.sliceId);
    const int chain = m_radioModel->sliceChainIndex(entry.sliceId);
    const auto rate = m_radioModel->widebandAdcRateHz(adc);
    if (!rate || chain < 0 || chain >= m_radioModel->boardCapabilities().rxFilterChainCount) {
        return context;
    }
    context.available = true;
    context.physicalAdcIndex = adc;
    context.filterChainIndex = chain;
    context.adcRateHz = *rate;
    if (entry.widebandWanted) {
        const auto source = m_radioModel->widebandSourceDescriptor(adc);
        if (!source) { return std::nullopt; }
        context.active = true;
        context.sourceGeneration = source->sourceGeneration;
    }
    return context.valid() ? std::optional(context) : std::nullopt;
}

void DaemonMediaController::onWidebandSourceChanged(int)
{
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    for (quint32 endpointId : endpointIds()) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        EndpointEntry& entry = it->second;
        if (!entry.widebandNegotiated) { continue; }
        // Parity Task 28: the fall configures a transmitting endpoint anew.
        if (entry.txViewer) { continue; }
        if (!reconcileWidebandDemand(entry)) {
            entry.contextSent = false;
            entry.latestInput.reset();
            continue;
        }
        const auto context = widebandContext(entry);
        if (!context || (entry.endpoint.configured()
                         && entry.endpoint.context().wideband != *context)) {
            entry.contextSent = false;
        }
        if (context && entry.latestInput && !entry.contextSent) {
            const auto frame = *entry.latestInput;
            const QPointer<DaemonMediaController> self(this);
            configureEndpointFromFrame(entry, frame);
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        }
    }
}

void DaemonMediaController::removeEndpoint(quint32 endpointId, bool retainOperation)
{
    // Fix wave I5: a closed display (or one refused and then closed) asks
    // for nothing more.
    m_refusedDemand.erase(endpointId);
    setDisplayDemand(endpointId, std::nullopt);
    auto it = m_endpoints.find(endpointId);
    if (it == m_endpoints.end()) {
        return;
    }
    const MediaSourceKey key = it->second.request.source;
    if (retainOperation && displayBudgetWireAvailable()) {
        AllocationRecord retired = it->second.allocation;
        retired.accepted = false;
        retired.reason = QStringLiteral("This display's spectrum was closed.");
        rememberNonliveOperation(endpointId, retired);
    }
    m_endpoints.erase(it);
    releaseSourceIfUnused(key);
    rebalanceSourceAfterDeparture(key);
    refreshDisplayBudgetPacer();
    refreshDeviceDisplayDuplex();
}

void DaemonMediaController::rebalanceSourceAfterDeparture(const MediaSourceKey& key)
{
    // A retired or unusable source is not rebuilt for its departed endpoint.
    const auto runtime = m_sources.constFind(key);
    if (runtime == m_sources.cend() || !runtime->configured) {
        return;
    }
    if (!anySharedEndpointOn(key)) {
        return;
    }
    // R-R3-01/R-R3-08/R-R3-09/R-R3-37: every pan still held to the departed
    // neighbour's engine size is granted its own request. The engine then
    // runs at the largest of those requests (a pan that asked for less gets
    // at least what it asked for), and the renewed contexts carry the new
    // grants. A pan never gains pixels its admitted display charge does not
    // cover (the GUI asks again for the rest, as it does for a lone pan).
    // When no pan is held, only the rate can fall, which renews no context.
    // Task 76: every device's pans on the engine, not only this one's.
    struct Regrant {
        EndpointEntry* entry = nullptr;
        int previousFftSize = 0;
        int previousPixels = 0;
        SpectrumDisplayCost previousCost;
    };
    std::vector<Regrant> regrants;
    int engineFftSize = 0;
    // Parity Task 17: a pan left alone on the engine runs at the
    // decimation it asked for, as it is granted its own size.
    int remaining = 0;
    EndpointEntry* lone = nullptr;
    forEachSharedEndpoint(key, [&](DaemonMediaController&, quint32, EndpointEntry& entry) {
        ++remaining;
        lone = &entry;
    });
    const bool decimationRegrant = remaining == 1 && lone
        && lone->sourceDecimation != lone->requestedDecimation;
    const int previousDecimation = lone ? lone->sourceDecimation : 1;
    if (decimationRegrant) {
        lone->sourceDecimation = lone->requestedDecimation;
    }
    forEachSharedEndpoint(key, [&](DaemonMediaController&, quint32, EndpointEntry& entry) {
        if (entry.grant.reason != SpectrumLimitReason::SharedEngine) {
            return;
        }
        const int fftSize = std::min(entry.grant.requestedFftSize,
                                     FFTEngine::maximumFftSize());
        const int pixels = entry.chargeCoversRequest
            ? entry.grant.requestedPixels : entry.request.pixels;
        const auto displayCost = endpointDisplayCost(
            pixels, entry.request.targetFps, entry.request.requestedWideSpanFactor > 1.0,
            entry.extrasRequest.sections());
        if (fftSize <= entry.sourceFftSize || !displayCost) {
            return;
        }
        regrants.push_back({&entry, entry.sourceFftSize, entry.request.pixels,
                            entry.displayCost});
        engineFftSize = std::max(engineFftSize, fftSize);
        entry.request.pixels = pixels;
        entry.displayCost = *displayCost;
    });
    if (!regrants.empty()) {
        // Every re-granted pan records the engine it now shares, as a pan
        // that joins an engine does, so a later departure among them does
        // not shrink the engine under the others.
        for (const Regrant& regrant : regrants) {
            regrant.entry->sourceFftSize = engineFftSize;
        }
        if (!reconcileSource(key)) {
            // A source that cannot be reconfigured keeps every pan as it was.
            for (const Regrant& regrant : regrants) {
                regrant.entry->sourceFftSize = regrant.previousFftSize;
                regrant.entry->request.pixels = regrant.previousPixels;
                regrant.entry->displayCost = regrant.previousCost;
            }
            if (decimationRegrant) {
                lone->sourceDecimation = previousDecimation;
            }
        }
        // Another device's re-granted pans changed its charge too.
        for (DaemonMediaController* member : m_shared->members()) {
            if (member != this) {
                member->refreshDisplayBudgetPacer();
            }
        }
        return;
    }
    // Otherwise only the rate (or a lone pan's decimation) can change; a
    // rate alone renews nothing. A source that cannot be reconfigured keeps
    // running as it was.
    if (!reconcileSource(key) && decimationRegrant) {
        lone->sourceDecimation = previousDecimation;
    }
}

void DaemonMediaController::releaseSourceIfUnused(const MediaSourceKey& key)
{
    // Task 76: the engine goes only when no device watches it.
    if (anySharedEndpointOn(key)) {
        return;
    }
    m_source.deactivate(key);
    m_sources.remove(key);
}

bool DaemonMediaController::reconcileSource(const MediaSourceKey& key)
{
    // Task 76 (ruling 9.1): one engine for everyone watching this receiver,
    // at the largest size and highest rate any device's pans were granted.
    int maximumFft = 0;
    int maximumFps = 0;
    int windowType = -1;
    // Parity Task 17: every endpoint on the engine holds its decimation
    // (handleSubscribe), so they agree; the largest is taken for safety.
    int decimation = 0;
    forEachSharedEndpoint(key, [&](DaemonMediaController&, quint32, EndpointEntry& entry) {
        maximumFft = std::max(maximumFft, entry.sourceFftSize);
        maximumFps = std::max(maximumFps, entry.request.targetFps);
        decimation = std::max(decimation, entry.sourceDecimation);
        if (windowType == -1) {
            windowType = entry.sourceWindowType;
        }
    });
    // The wire request's FFT/window values are represented in the source
    // runtime by the controller after strict validation. They are filled by
    // subscribe before this method is called.
    SourceRuntime& runtime = m_sources[key];
    if (maximumFft == 0 || !m_radioModel || !m_radioModel->streamActive(key.streamIndex)) {
        return false;
    }
    DaemonSpectrumSourceConfig config = runtime.config;
    config.centreHz = m_radioModel->streamCentreHz(key.streamIndex);
    config.sampleRateHz = m_radioModel->streamSampleRateHz(key.streamIndex);
    config.fft.fftSize = maximumFft;
    config.fft.fps = maximumFps;
    config.fft.windowType = windowType;
    config.decimation = std::clamp(decimation, ControlRanges::kDisplayDecimationMin,
                                   ControlRanges::kDisplayDecimationMax);
    config.maxPendingIqFloats = maximumFft * 4;
    // R-R3-08/40: while the budget is lowered because the Core is busy, a
    // lower frame rate must save FFT work too, so transforms follow the
    // frame rate. The app's detail step changes pixels only: FFT work is
    // set by the sample rate and frame rate, and a smaller FFT would not
    // save any (it runs more often for the same samples).
    config.transformsFollowFrameRate = coreBusyLimitsSources();
    if (!std::isfinite(config.centreHz) || config.sampleRateHz <= 0.0) {
        return false;
    }
    // R-R3-01/R-R3-08: the engine runs at the highest rate its endpoints
    // ask for, and each endpoint keeps its own cadence (SpectrumEndpoint::
    // consume), whose averaging advances per emitted frame. A change of rate
    // alone therefore renews no context: a neighbour joining, changing rate
    // or leaving neither moves another pan nor holds it below its own rate.
    DaemonSpectrumSourceConfig sameRate = config;
    sameRate.fft.fps = runtime.config.fft.fps;
    sameRate.transformsFollowFrameRate = runtime.config.transformsFollowFrameRate;
    const bool rateOnly = runtime.configured && sameSourceConfig(runtime.config, sameRate)
        && (runtime.config.fft.fps != config.fft.fps
            || runtime.config.transformsFollowFrameRate != config.transformsFollowFrameRate);
    const bool changed = !runtime.configured
        || (!rateOnly && !sameSourceConfig(runtime.config, config));
    bool accepted = true;
    if (!runtime.configured) {
        accepted = m_source.activate(key, config);
    } else if (rateOnly) {
        accepted = m_source.updateFrameRate(key, config.fft.fps,
                                            config.transformsFollowFrameRate);
    } else if (changed) {
        accepted = m_source.update(key, config);
    }
    if (!accepted) {
        return false;
    }
    runtime.config = config;
    runtime.configured = true;
    if (changed) {
        // Every device's pans on this engine take the new context.
        forEachSharedEndpoint(key, [](DaemonMediaController&, quint32, EndpointEntry& entry) {
            entry.endpoint.reset();
            entry.encoder.reset();
            entry.latestInput.reset();
            entry.contextSent = false;
            entry.forceKeyframe = true;
            entry.lastNoiseFloorTimestampNs = -1;
        });
    }
    for (DaemonMediaController* member : m_shared->members()) {
        if (!member->m_sendTimer.isActive() && !member->m_endpoints.empty()) {
            member->m_sendTimer.start();
        }
    }
    if (!m_sendTimer.isActive()) {
        m_sendTimer.start();
    }
    return true;
}

void DaemonMediaController::onStreamGeometryChanged(int streamIndex, double, int)
{
    QList<int> changedIq;
    for (const auto& [sliceId, iq] : m_iqStreams) {
        const SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
        if (iq.desired && (iq.streamIndex == streamIndex
            || (slice && slice->streamIndex() == streamIndex))
            && (!slice || iq.streamIndex != slice->streamIndex()
                || iq.sampleRate != m_radioModel->streamSampleRateHz(streamIndex))) {
            changedIq.append(sliceId);
        }
    }
    const quint64 iqEpoch = m_epoch;
    const QPointer<DaemonMediaController> iqSelf(this);
    for (int sliceId : changedIq) {
        if (!iqSelf || iqEpoch != m_epoch) { return; }
        reconcileIqStream(sliceId);
    }
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    // Task 76: this device's own pans on the stream; the engines they share
    // with other devices are reconciled for everyone.
    QList<MediaSourceKey> keys;
    for (const auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        if (entry.request.source.streamIndex == streamIndex
            && !keys.contains(entry.request.source)) {
            keys.append(entry.request.source);
        }
    }
    for (const MediaSourceKey& key : keys) {
        const double centreHz = m_radioModel ? m_radioModel->streamCentreHz(streamIndex) : 0.0;
        const double sampleRateHz = m_radioModel
            ? m_radioModel->streamSampleRateHz(streamIndex) : 0.0;
        for (quint32 endpointId : endpointIds()) {
            auto it = m_endpoints.find(endpointId);
            if (it == m_endpoints.end()) { continue; }
            const EndpointEntry& entry = it->second;
            const auto adcRate = m_radioModel
                ? m_radioModel->widebandAdcRateHz(m_radioModel->sliceAdcIndex(entry.sliceId))
                : std::nullopt;
            if (entry.request.source == key
                && !requestOverlapsSource(entry.request, centreHz, sampleRateHz)
                && !(entry.request.extendedView && adcRate)) {
                const quint32 revision = displayBudgetWireAvailable()
                    ? entry.allocation.revision : entry.revision;
                removeEndpoint(endpointId);
                const QPointer<DaemonMediaController> self(this);
                sendRejected(m_peer ? m_peer->connectionId() : QString(), endpointId, revision,
                             QString::fromLatin1(kRetireReasonSourceRetune));
                if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
            }
        }
        if (!anySharedEndpointOn(key)) {
            continue;
        }
        for (auto& [unused, entry] : m_endpoints) {
            Q_UNUSED(unused);
            if (entry.request.source == key && !reconcileWidebandDemand(entry)) {
                entry.contextSent = false;
                entry.latestInput.reset();
            }
        }
        // Another device found this engine unusable first and retired it:
        // this device's pans on it go the same way.
        const bool retiredByAnother = !m_sources.contains(key);
        if (retiredByAnother || !reconcileSource(key)) {
            // The old geometry is no longer usable. Retire its reservations
            // explicitly so a client can retry instead of believing a source
            // that no longer exists still owns a live display allocation.
            m_source.deactivate(key);
            m_sources.remove(key);
            for (quint32 endpointId : endpointIds()) {
                const auto it = m_endpoints.find(endpointId);
                if (it == m_endpoints.end() || !(it->second.request.source == key)) { continue; }
                const quint32 revision = displayBudgetWireAvailable()
                    ? it->second.allocation.revision : it->second.revision;
                removeEndpoint(endpointId);
                const QPointer<DaemonMediaController> self(this);
                sendRejected(m_peer ? m_peer->connectionId() : QString(), endpointId, revision,
                             QStringLiteral("This receiver's spectrum stopped on the Core."));
                if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
            }
        }
    }
}

void DaemonMediaController::onStreamBindingsChanged(int streamIndex, const QVector<int>&)
{
    QList<int> changedIq;
    for (const auto& [sliceId, iq] : m_iqStreams) {
        const SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
        if (iq.desired && (iq.streamIndex == streamIndex
            || (slice && slice->streamIndex() == streamIndex))
            && (!slice || iq.streamIndex != slice->streamIndex())) {
            changedIq.append(sliceId);
        }
    }
    const quint64 iqEpoch = m_epoch;
    const QPointer<DaemonMediaController> iqSelf(this);
    for (int sliceId : changedIq) {
        if (!iqSelf || iqEpoch != m_epoch) { return; }
        reconcileIqStream(sliceId);
    }
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    for (quint32 endpointId : endpointIds()) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        const EndpointEntry& entry = it->second;
        SliceModel* slice = m_radioModel ? m_radioModel->sliceById(entry.sliceId) : nullptr;
        if (entry.request.source.streamIndex == streamIndex
            && (!slice || slice->streamIndex() != streamIndex)) {
            const quint32 revision = displayBudgetWireAvailable()
                ? entry.allocation.revision : entry.revision;
            removeEndpoint(endpointId);
            const QPointer<DaemonMediaController> self(this);
            sendRejected(m_peer ? m_peer->connectionId() : QString(), endpointId, revision,
                         QString::fromLatin1(kRetireReasonStreamBindingChanged));
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return; }
        }
    }
}

void DaemonMediaController::onSliceRemoved(int sliceId)
{
    if (auto iq = m_iqStreams.find(sliceId); iq != m_iqStreams.end()) {
        stopIqStream(iq->second);
        iq->second.desired = false;
        const QPointer<DaemonMediaController> self(this);
        sendIqContext(sliceId, iq->second, QStringLiteral("This receiver was removed."));
        if (!self) { return; }
        refreshDisplayBudgetPacer();
        if (m_server) { m_server->publishDisplayBudgetCapabilities(); }
    }
    // R-R3-43: the slice's receiver stream retires with its reason, and its
    // stream id goes free. The revision stays, so a stale request stays
    // refused.
    const auto receiver = m_receiverStreams.find(sliceId);
    if (receiver != m_receiverStreams.end()
        && (receiver->second.desiredEnabled || receiver->second.streamIndex >= 0)) {
        ReceiverAudioStream& stream = receiver->second;
        stopReceiverAudioCapture(stream);
        stream.desiredEnabled = false;
        // The context names the stream id the packets stopped on.
        sendReceiverAudioContext(sliceId, stream, false, RemoteAudioOffReason::SliceRemoved);
        releaseReceiverStreamIndex(stream);
        retireReceiverSender(stream);
    }
    // R-R3-45: the slice may have been the last one on the headphones.
    {
        const QPointer<DaemonMediaController> self(this);
        MediaPeer* const before = m_peer.get();
        onOutputRoutesChanged();
        if (!self || m_peer.get() != before) { return; }
    }
    retireSliceDisplays(sliceId);
}

bool DaemonMediaController::retireSliceDisplays(int sliceId)
{
    MediaPeer* const peer = m_peer.get();
    const quint64 epoch = m_epoch;
    for (quint32 endpointId : endpointIds()) {
        auto it = m_endpoints.find(endpointId);
        if (it == m_endpoints.end()) { continue; }
        const EndpointEntry& entry = it->second;
        if (entry.sliceId == sliceId) {
            const quint32 revision = displayBudgetWireAvailable()
                ? entry.allocation.revision : entry.revision;
            removeEndpoint(endpointId);
            const QPointer<DaemonMediaController> self(this);
            sendRejected(m_peer ? m_peer->connectionId() : QString(), endpointId, revision,
                         QString::fromLatin1(kRetireReasonSliceRemoved));
            if (!self || m_peer.get() != peer || m_epoch != epoch) { return false; }
        }
    }
    return true;
}

void DaemonMediaController::reconcileAudio()
{
    // Every accepted control and every readiness transition begins by
    // flushing bounded captured PCM.  The saved next values preserve RTP
    // ordering across false/true contexts for this same MediaPeer.
    stopAudioCapture();
    // Why audio is off, first cause wins: the client's own choice, then the
    // station radio, then media readiness.
    std::optional<RemoteAudioOffReason> blockedBy;
    if (!m_audioDesiredEnabled) {
        blockedBy = RemoteAudioOffReason::ClientDisabled;
    } else if (!m_radioModel || !m_radioModel->isConnected()) {
        blockedBy = RemoteAudioOffReason::RadioOffline;
    } else if (!m_peer || !m_peer->isReady()) {
        blockedBy = RemoteAudioOffReason::MediaNotReady;
    }
    const bool shouldRun = !blockedBy.has_value();
    bool actualEnabled = false;
    // Capture is stopped above, so queued audio of the old profile is gone
    // and a new profile starts at the next capture block (R-R3-23).
    admitAudioProfile();
    // iPhone app plan Task 23: a device's own bitrate switches the encoder
    // here, at the next capture block (capture is stopped above), with the
    // sequence and timestamp carried on.
    if (m_audioSender && m_audioSenderBitrate != audioStreamBitrate()) {
        m_audioSender.reset();
    }
    if (shouldRun) {
        if (!m_audioSender) {
            OpusAudioCodecConfig codecConfig;
            codecConfig.bitrate = audioStreamBitrate();
            m_audioSenderBitrate = codecConfig.bitrate;
            m_audioSender = std::make_unique<DaemonAudioSender>(
                m_radioModel->audioEngine(), codecConfig);
            // R-R3-35: capture times on the clock clock-echo reports. The
            // sender stops its DSP tap before this controller goes away.
            m_audioSender->setCaptureClock([this] { return displayNowNs(); });
            DaemonAudioSender* const sender = m_audioSender.get();
            connect(sender, &DaemonAudioSender::packetReady, this,
                    [this, sender](const QByteArray& packet) {
                // packetReady is emitted by the sender's owner thread.  The
                // identity checks make a stopped/retired session unable to
                // forward a late signal to a replacement peer.
                if (m_audioSender.get() == sender && sender->isRunning()
                    && m_epoch != 0 && m_peer && m_peer->isReady()) {
                    MediaPeer* const peer = m_peer.get();
                    const quint64 epoch = m_epoch;
                    const quint32 contextGeneration = m_audioDiagnostics.contextGeneration;
                    const bool activeContext = m_audioDiagnostics.activeContext;
                    if (activeContext) {
                        ++m_audioDiagnostics.sendAttempts;
                        ++m_audioDiagnostics.sendInFlight;
                    }
                    const QPointer<DaemonMediaController> self(this);
                    const bool accepted = sendAudioRtp(peer, packet);
                    // The transport can synchronously retire or replace this
                    // session. Resolve only into the context that initiated
                    // this send, never a replacement that appeared mid-call.
                    // Retirement finalizes any remaining in-flight attempt as
                    // unresolved, without treating it as a packet-loss event.
                    if (self && activeContext && m_audioSender.get() == sender
                        && m_epoch == epoch && m_peer.get() == peer
                        && m_audioDiagnostics.activeContext
                        && m_audioDiagnostics.contextGeneration == contextGeneration) {
                        --m_audioDiagnostics.sendInFlight;
                        if (accepted) {
                            ++m_audioDiagnostics.sendAccepted;
                        } else {
                            ++m_audioDiagnostics.sendRejected;
                        }
                        maybeLogAudioDiagnostics(false);
                    }
                }
            });
        }
        m_audioSender->setProfile(m_audioActiveProfile);
        // R-R3-45: an app that plays the headphones mix on its own stream
        // gets the speakers' mix alone here; any other app the station's
        // whole program, as before. Capture is stopped above.
        m_audioSender->setSliceSource(m_headphonesMixNegotiated
                                          ? DaemonAudioSource::kSpeakersMix
                                          : DaemonAudioSource::kMasterMix);
        // Task 76 (ruling 9.2): this device's own mix, its own slices.
        m_audioSender->setOwnerMix(m_ownerMix);
        actualEnabled = m_audioSender->start(m_peer->audioSsrc(), m_audioNextSequence,
                                              m_audioNextTimestamp);
        if (actualEnabled) {
            m_audioNextSequence = m_audioSender->nextSequence();
            m_audioNextTimestamp = m_audioSender->nextTimestamp();
        }
    }
    // Nothing blocked audio, so a context that is still off means the sender
    // could not start.
    sendAudioContext(actualEnabled,
                     blockedBy.value_or(RemoteAudioOffReason::EncoderUnavailable));
}

void DaemonMediaController::stopAudioCapture()
{
    if (!m_audioSender) {
        return;
    }
    // Sender advances timestamp over every consumed block, including an
    // encode failure; retaining both fields before stop preserves the next
    // audio-context boundary across a pause or reconnect. Stop quiesces the
    // source bridge before the final diagnostics sample, so ingress/drop
    // counters include any admitted DSP callback.
    m_audioNextSequence = m_audioSender->nextSequence();
    m_audioNextTimestamp = m_audioSender->nextTimestamp();
    m_audioSender->stop();
    finalizeAudioDiagnostics();
}

void DaemonMediaController::sendAudioContext(bool enabled, RemoteAudioOffReason reason)
{
    if (!m_peer || m_audioRevision == 0) {
        return;
    }
    const bool detailNegotiated = m_server && m_server->remoteAudioStatusAvailable(m_epoch);
    const bool profileNegotiated = detailNegotiated && m_audioProfileNegotiated;
    const bool lossless = m_audioActiveProfile == RemoteAudioProfile::Lossless;
    if (enabled && detailNegotiated
        && !(m_audioSender && (lossless ? m_audioSender->profileReady()
                                        : m_audioSender->encoderProfile().has_value()))) {
        // A minor-8 GUI refuses an enabled context without its encoder
        // profile. With no profile to report, stop the sender so no RTP
        // flows under the context and say plainly why audio is off.
        stopAudioCapture();
        enabled = false;
        reason = RemoteAudioOffReason::EncoderUnavailable;
    }
    const quint32 contextGeneration = nextContextGeneration();
    if (enabled) {
        beginAudioDiagnostics(contextGeneration);
    }
    RemoteAudioContextMessage message;
    message.connectionId = m_peer->connectionId();
    message.revision = m_audioRevision;
    message.generation = contextGeneration;
    message.enabled = enabled;
    message.ssrc = m_peer->audioSsrc();
    message.firstSequence = m_audioNextSequence;
    message.firstTimestamp = m_audioNextTimestamp;
    if (enabled && lossless) {
        message.losslessEncoder = m_audioSender->losslessProfile();
    } else if (enabled) {
        // A started sender always has a ready encoder, so this is the
        // profile the context's packets are coded with.
        message.encoder = m_audioSender ? m_audioSender->encoderProfile() : std::nullopt;
    } else {
        message.offReason = reason;
    }
    message.profile = m_audioActiveProfile;
    message.profileRefusal = m_audioProfileRefusal;
    // iPhone app plan Task 23: why this device's bitrate was not taken.
    message.opusBitrateRefusal = m_audioBitrateRefusal;
    // A minor-7 peer gets exactly the eight keys it has always parsed, and a
    // GUI that never sent `profile` exactly the minor-8 shape.
    sendControl(encodeRemoteAudioContext(message, detailNegotiated, profileNegotiated));
}

DaemonAudioDiagnostics DaemonMediaController::snapshotAudioDiagnostics() const
{
    DaemonAudioDiagnostics snapshot = m_audioDiagnostics;
    if (snapshot.activeContext) {
        if (m_audioDiagnosticsClock.isValid()) {
            snapshot.elapsedMs = m_audioDiagnosticsClock.elapsed();
        }
        if (m_audioSender) {
            snapshot.sender = m_audioSender->telemetry();
        }
    }
    return snapshot;
}

void DaemonMediaController::beginAudioDiagnostics(quint32 contextGeneration)
{
    m_audioDiagnostics = {};
    m_audioDiagnostics.contextGeneration = contextGeneration;
    m_audioDiagnostics.revision = m_audioRevision;
    m_audioDiagnostics.activeContext = true;
    m_audioDiagnosticsClock.start();
    m_audioDiagnosticsLastLogMs = 0;
    m_audioDiagnosticsTimer.start();
}

void DaemonMediaController::finalizeAudioDiagnostics()
{
    if (!m_audioDiagnostics.activeContext) {
        return;
    }
    m_audioDiagnostics = snapshotAudioDiagnostics();
    m_audioDiagnostics.sendUnresolvedAtRetirement += m_audioDiagnostics.sendInFlight;
    m_audioDiagnostics.sendInFlight = 0;
    maybeLogAudioDiagnostics(true);
    m_audioDiagnostics.activeContext = false;
    m_audioDiagnosticsTimer.stop();
}

void DaemonMediaController::maybeLogAudioDiagnostics(bool final)
{
    if (!m_audioDiagnostics.activeContext || !m_audioDiagnosticsClock.isValid()) {
        return;
    }
    const qint64 elapsedMs = m_audioDiagnosticsClock.elapsed();
    if (!final && elapsedMs - m_audioDiagnosticsLastLogMs < kAudioDiagnosticsLogIntervalMs) {
        return;
    }
    const DaemonAudioDiagnostics snapshot = snapshotAudioDiagnostics();
    qCInfo(lcDaemonMedia).nospace()
        << "daemon audio diagnostics " << (final ? "final" : "periodic")
        << " context=" << snapshot.contextGeneration
        << " revision=" << snapshot.revision
        << " elapsedMs=" << snapshot.elapsedMs
        << " sourceFrames=" << snapshot.sender.source.capturedValidRateFrames
        << " sourceDropEvents=" << snapshot.sender.source.sourceDropEvents
        << " sourceContentionRetries=" << snapshot.sender.source.contentionRetries
        << " sourceContentionLosses=" << snapshot.sender.source.contentionLosses
        << " sourceRingFullDrops=" << snapshot.sender.source.ringFullDrops
        << " sourceInvalidIngressDrops=" << snapshot.sender.source.invalidIngressDrops
        << " consumed=" << snapshot.sender.consumedBlocks
        << " encoded=" << snapshot.sender.encodedPackets
        << " encodeFailures=" << snapshot.sender.encodeFailures
        // Lossless send ticks held to the per-tick cap with audio waiting:
        // the Core's send timer ran late (a slow Core), not the network.
        << " losslessCappedTicks=" << snapshot.sender.losslessCappedTicks
        << " sendAttempts=" << snapshot.sendAttempts
        << " sendAccepted=" << snapshot.sendAccepted
        << " sendRejected=" << snapshot.sendRejected
        << " sendInFlight=" << snapshot.sendInFlight
        << " sendUnresolvedAtRetirement=" << snapshot.sendUnresolvedAtRetirement
        << " hasLastPacket=" << snapshot.sender.hasLastEmittedPacket
        << " lastSequence=" << snapshot.sender.lastEmittedSequence
        << " lastTimestamp=" << snapshot.sender.lastEmittedTimestamp
        // Radio codec: the radio's own speaker out (the P1 L/R bytes, the P2
        // port 1028 stream), its ring's drift counters.
        << " " << qPrintable(RadioConnection::radioAudioStatsText(
               m_radioModel && m_radioModel->connection() != nullptr
                   ? m_radioModel->connection()->radioAudioStats()
                   : RadioConnection::RadioAudioStats{}));
    m_audioDiagnosticsLastLogMs = elapsedMs;
}

void DaemonMediaController::resetAudioSession()
{
    stopAudioCapture();
    m_audioDesiredEnabled = false;
    m_audioRevision = 0;
    m_audioProfileNegotiated = false;
    m_audioRequestedProfile = RemoteAudioProfile::Opus;
    m_audioActiveProfile = RemoteAudioProfile::Opus;
    m_audioProfileRefusal.reset();
    // iPhone app plan Task 23: a new peer starts at the Core's bitrate.
    m_audioRequestedBitrate = 0;
    m_audioBitrateRefusal.clear();
    // A different MediaPeer has a different SSRC identity, so it may start
    // a new RTP timeline. Existing peers always retain the saved values.
    m_audioNextSequence = 1;
    m_audioNextTimestamp = 0;
}

void DaemonMediaController::clearSession()
{
    // Detach and move ownership before stop(): MediaPeer::stop() can emit
    // closed synchronously. Its callbacks are identity/epoch guarded, but
    // disconnecting them too prevents recursive clearSession(). Defer deletion
    // until its send/signal stack has unwound. QObject parent ownership still
    // reclaims a retired peer if the controller dies before deferred deletion.
    // Sender remains owned by this controller; stop its timer/capture while
    // the current peer is still identifiable, then retire the peer.
    // Parity Task 32: MON leaves this device's mix with its peer; the next
    // peer declares its own.
    m_txMonitorNegotiated = false;
    m_monitorRevision = 0;
    m_monitorRoute = TxMonitorRoute::None;
    m_monitorApplied = TxMonitorRoute::None;
    if (m_ownerMix >= 0 && m_radioModel && m_radioModel->audioEngine() != nullptr) {
        m_radioModel->audioEngine()->setOwnerMixMonitor(m_ownerMix,
                                                       MasterMixer::OwnerMonitor::None);
    }
    resetAudioSession();
    // R-R3-43: every receiver stream's sender and slice tap go with the
    // session; there is no GUI left to tell.
    resetReceiverAudioSession();
    for (auto& [sliceId, stream] : m_iqStreams) {
        Q_UNUSED(sliceId);
        stopIqStream(stream);
    }
    m_iqStreams.clear();
    m_iqNegotiated = false;
    // R-R3-45: and the headphones mix's.
    resetHeadphonesAudioSession();
    // Task 36: and the microphone line; the ring goes out of use with it.
    stopMicLine();
    // Task 29: a replacement under way, and a peer still draining.
    clearReplacement();
    m_sendSsrcRewrite.clear();
    m_micSsrcRewrite.clear();
    syncMicRoute();
    if (m_peer) {
        logDisplayDiagnostics(true);
    }
    m_displayDiagnosticsTimer.stop();
    MediaPeer* const peer = m_peer.release();
    if (peer) {
        peer->disconnect(this);
        peer->stop();
        peer->deleteLater();
    }
    clearProduction();
    clearAllocationIdentity();
    // Parity Task 28: the next peer declares its own.
    m_txDisplayNegotiated = false;
    m_miniDisplayNegotiated = false;
}

void DaemonMediaController::retirePeerKeepingSession()
{
    // The peer goes and the session stays: every other device's display
    // share follows this one's demand going (ruling 9.3), as when the radio
    // drops.
    const bool demanded = !m_displayDemand.empty();
    clearSession();
    if (demanded && m_server && m_boundEpoch != 0) {
        m_server->publishDisplayBudgetCapabilities();
    }
}

QList<quint32> DaemonMediaController::endpointIds() const
{
    QList<quint32> ids;
    ids.reserve(static_cast<qsizetype>(m_endpoints.size()));
    for (const auto& [endpointId, unused] : m_endpoints) {
        Q_UNUSED(unused);
        ids.append(endpointId);
    }
    return ids;
}

void DaemonMediaController::clearProduction()
{
    // Fix wave I5: no display is asked for once production stops. Split
    // again by the caller that keeps its session (the session's own end
    // splits again on the Core's side).
    m_displayDemand.clear();
    m_refusedDemand.clear();
    if (m_refusedDemandTimer) {
        m_refusedDemandTimer->stop();
    }
    QList<int> activeIq;
    for (const auto& [sliceId, stream] : m_iqStreams) {
        if (stream.sending) { activeIq.append(sliceId); }
    }
    const QPointer<DaemonMediaController> iqSelf(this);
    for (int sliceId : activeIq) {
        if (!iqSelf) { return; }
        const auto found = m_iqStreams.find(sliceId);
        if (found == m_iqStreams.end() || !found->second.sending) { continue; }
        stopIqStream(found->second);
        sendIqContext(sliceId, found->second, QStringLiteral("The Core radio is offline."));
    }
    m_sendTimer.stop();
    m_ps3CurrentChunks.clear();
    m_ps3LatestChunks.clear();
    m_ps3CurrentAttempted = false;
    // Task 76: the engines this device's pans used, which other devices
    // may still be watching.
    QList<MediaSourceKey> keys;
    for (const auto& [unused, entry] : m_endpoints) {
        Q_UNUSED(unused);
        if (!keys.contains(entry.request.source)) {
            keys.append(entry.request.source);
        }
    }
    if (displayBudgetWireAvailable()) {
        for (const auto& [endpointId, entry] : m_endpoints) {
            AllocationRecord retired = entry.allocation;
            retired.accepted = false;
            retired.reason = QStringLiteral("The Core stopped sending displays.");
            rememberNonliveOperation(endpointId, retired);
        }
    }
    m_endpoints.clear();
    m_roundRobinCursor = 0;
    refreshDeviceDisplayDuplex();
    // An engine nobody else watches stops; one another device still
    // watches keeps running for it, sized to its own pans.
    for (const MediaSourceKey& key : keys) {
        if (anySharedEndpointOn(key)) {
            rebalanceSourceAfterDeparture(key);
        } else {
            m_source.deactivate(key);
            m_sources.remove(key);
        }
    }
    refreshDisplayBudgetPacer();
}

bool DaemonMediaController::sendControl(const QJsonObject& payload) const
{
    return m_server && m_epoch != 0 && m_server->sendMediaControl(payload, m_epoch);
}

void DaemonMediaController::sendRejected(const QString& connectionId, quint32 endpointId,
                                         quint32 revision, const QString& reason)
{
    if (connectionId.isEmpty()) {
        return;
    }
    if (endpointId != 0 && revision != 0 && displayBudgetWireAvailable()) {
        // Involuntary source retirement uses the same authoritative resource
        // result as a requested allocation. Budget-aware clients ignore the
        // legacy endpoint rejection shape and must learn the released charge.
        sendAllocationResult(connectionId, endpointId, revision, false, reason);
        return;
    }
    sendControl({
        {QStringLiteral("op"), QStringLiteral("rejected")},
        {QStringLiteral("connectionId"), connectionId},
        {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
        {QStringLiteral("revision"), static_cast<qint64>(revision)},
        {QStringLiteral("reason"), reason},
    });
}

quint32 DaemonMediaController::nextContextGeneration()
{
    ++m_nextContextGeneration;
    if (m_nextContextGeneration == 0) {
        ++m_nextContextGeneration;
    }
    return m_nextContextGeneration;
}

// ── One controller per admitted session (iPhone app Task 76) ────────────

DaemonMediaHub::DaemonMediaHub(StationServer* server, RadioModel* radioModel, QObject* parent,
                               MediaPeer::TransportFactory peerFactory,
                               DaemonMediaController::MonotonicClock monotonicClock)
    : QObject(parent)
    , m_server(server)
    , m_radioModel(radioModel)
    , m_peerFactory(std::move(peerFactory))
    , m_monotonicClock(std::move(monotonicClock))
    , m_spectrum(std::make_shared<DaemonSharedSpectrum>(radioModel))
{
    if (!m_server || !m_radioModel) {
        return;
    }
    connect(m_server, &StationServer::mediaSessionStarted,
            this, &DaemonMediaHub::onSessionStarted);
    connect(m_server, &StationServer::mediaSessionEnded,
            this, &DaemonMediaHub::onSessionEnded);
    const QPointer<DaemonMediaHub> self(this);
    // Fix wave C2: the keying's view of the microphone lines, by device: a
    // key waits on, and is answered by, the line of the device it is for,
    // whichever controller carries it; one controller's teardown never
    // clears another's.
    if (m_server->remoteKeying() != nullptr) {
        RemoteKeying::MicUplink uplink;
        const auto lineFor = [self](const QByteArray& deviceId) -> DaemonMediaController* {
            if (!self) {
                return nullptr;
            }
            for (const auto& [epoch, controller] : self->m_controllers) {
                Q_UNUSED(epoch);
                if (controller && controller->carriesMicFor(deviceId)) {
                    return controller.get();
                }
            }
            return nullptr;
        };
        uplink.carriesMic = [lineFor](const QByteArray& deviceId) {
            return lineFor(deviceId) != nullptr;
        };
        uplink.prime = [lineFor](const QByteArray& deviceId, std::function<void(bool)> done) {
            DaemonMediaController* controller = lineFor(deviceId);
            if (controller == nullptr) {
                done(false);
                return;
            }
            controller->primeMic(std::move(done));
        };
        uplink.endPriming = [self](const QByteArray& deviceId) {
            if (!self) {
                return;
            }
            for (const auto& [epoch, controller] : self->m_controllers) {
                Q_UNUSED(epoch);
                if (controller && controller->micDeviceId() == deviceId) {
                    controller->endMicPriming();
                }
            }
            if (self->m_radioModel) {
                // A line that closed while its key waited leaves no priming.
                self->m_radioModel->setRemoteMicPriming(deviceId, false);
            }
        };
        m_server->remoteKeying()->setMicUplink(uplink);
    }
    // The PureSignal display gate: the asking session's controller answers
    // (ruling 9.3 item 4).
    m_server->setSessionPs3DisplayAdmissionHandler(
        [self](quint64 epoch, bool enabled, QString* refusal) {
        DaemonMediaController* controller = self ? self->controllerFor(epoch) : nullptr;
        if (!controller) {
            if (refusal) { *refusal = QStringLiteral("The Core stopped its display service."); }
            return false;
        }
        return controller->admitPs3DisplayForSession(enabled, refusal);
    });
    // Fix wave I5 (ruling 9.3): each session's request is its controller's
    // display demand.
    m_server->setDisplayDemandProvider([self](quint64 epoch) -> std::optional<DisplayBudgetCharge> {
        DaemonMediaController* controller = self ? self->controllerFor(epoch) : nullptr;
        if (!controller) {
            return std::nullopt;
        }
        return controller->displayDemand();
    });
    // The gate is in place before capability publication can advertise
    // budget enforcement.
    m_server->setDisplayBudgetEnforcementEnabled(true);
    // A session already live (none at the Core's start) gets its own.
    for (quint64 epoch : m_server->mediaSessionEpochs()) {
        if (m_server->mediaAvailable(epoch)) {
            onSessionStarted(epoch);
        }
    }
}

DaemonMediaHub::~DaemonMediaHub()
{
    if (m_server) {
        m_server->disconnect(this);
        if (m_server->remoteKeying() != nullptr) {
            m_server->remoteKeying()->setMicUplink({});
        }
        m_server->setSessionPs3DisplayAdmissionHandler({});
        m_server->setDisplayDemandProvider({});
        m_server->setDisplayBudgetEnforcementEnabled(false);
    }
    m_controllers.clear();
}

void DaemonMediaHub::onSessionStarted(quint64 epoch)
{
    if (epoch == 0 || m_controllers.count(epoch) != 0 || !m_server) {
        return;
    }
    auto controller = std::make_unique<DaemonMediaController>(
        m_server, m_radioModel, epoch, m_spectrum, nullptr, m_peerFactory, m_monotonicClock);
    if (m_audioTargetBitrate) {
        controller->setAudioTargetBitrate(*m_audioTargetBitrate);
    }
    if (m_audioLosslessAllowed) {
        controller->setAudioLosslessAllowed(*m_audioLosslessAllowed);
    }
    m_controllers.emplace(epoch, std::move(controller));
}

void DaemonMediaHub::onSessionEnded(quint64 epoch)
{
    auto it = m_controllers.find(epoch);
    if (it == m_controllers.end()) {
        return;
    }
    // The controller hears this same signal and clears its session itself;
    // it goes once the signal has unwound.
    DaemonMediaController* controller = it->second.release();
    m_controllers.erase(it);
    controller->deleteLater();
}

void DaemonMediaHub::setAudioTargetBitrate(int bitsPerSecond)
{
    m_audioTargetBitrate = bitsPerSecond;
    for (auto& [unused, controller] : m_controllers) {
        Q_UNUSED(unused);
        controller->setAudioTargetBitrate(bitsPerSecond);
    }
}

void DaemonMediaHub::setAudioLosslessAllowed(bool allowed)
{
    m_audioLosslessAllowed = allowed;
    for (auto& [unused, controller] : m_controllers) {
        Q_UNUSED(unused);
        controller->setAudioLosslessAllowed(allowed);
    }
}

DaemonMediaController* DaemonMediaHub::controllerFor(quint64 epoch) const
{
    const auto it = m_controllers.find(epoch);
    return it != m_controllers.end() ? it->second.get() : nullptr;
}

QList<DaemonMediaController*> DaemonMediaHub::controllers() const
{
    QList<DaemonMediaController*> out;
    for (const auto& [unused, controller] : m_controllers) {
        Q_UNUSED(unused);
        out.append(controller.get());
    }
    return out;
}

DisplayBudgetCharge DaemonMediaHub::acceptedDisplayCharge() const
{
    // Each controller's own charge; the PureSignal display is in its
    // subscriber's alone, so it is counted once.
    QList<DisplayBudgetCharge> charges;
    for (const auto& [unused, controller] : m_controllers) {
        Q_UNUSED(unused);
        charges.append(controller->ownDisplayCharge());
    }
    return sumDisplayCharges(charges).value_or(DisplayBudgetCharge{});
}

DaemonAudioDiagnostics DaemonMediaHub::audioDiagnostics(quint64 epoch) const
{
    DaemonMediaController* controller = controllerFor(epoch);
    return controller ? controller->audioDiagnostics() : DaemonAudioDiagnostics{};
}

} // namespace NereusSDR
