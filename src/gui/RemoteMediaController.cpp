// no-port-check: NereusSDR-original. Remote daemon R3 receive display wiring.
// Modification history (NereusSDR):
//   2026-10-04: Release explicitly retired displays whose subscribe result
//               expired across media replacement; retain uncertain charge
//               until the release result and guard synchronous stack teardown.
//               J.J. Boyd (KG4VCF),
//               AI-assisted via OpenAI Codex.
//   2026-10-02: Carry accepted capture metadata to delayed display presentation.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in numeric RX binding retirement
//                 diagnostics. AI-assisted via OpenAI Codex.
//   2026-09-29: a refused replace keeps the session move it carried: a
//               move folded into a waiting fallback, or one that came
//               while the fallback's replace was under way, stays pending
//               when the Core refuses that replace for transmitting, so
//               media still follows it after the Core unkeys. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: holdAudioRestartForTest, so a test can hold an audio
//               restart's backoff step instead of racing its timer; a
//               refused fallback waiting to be retried ends when media
//               arrives again (its silence is over), or becomes a normal
//               replace when a move was folded into it. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: a playback failure ends a restart waiting on its backoff
//               step (it could never run once the failure moved the audio
//               revision on); a session move keeps a refused fallback
//               waiting to be retried on the tunnel alone. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: audioRestartPendingForTest for the direct silence test.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media, the restart backoff across a move: a pending
//               audio restart is fenced by its own generation, not by the
//               connection id, so a restart waiting when media moves to
//               the tunnel still asks for audio on the new connection.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media follow-up: a fallback the Core refuses while
//               it transmits stays a fallback onto the tunnel alone when
//               it is retried. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: direct media fix wave: the silence fallback runs once per
//               silence while the window wants audio (connected, not
//               muted), replaces onto the tunnel alone, and asks for
//               recovery when media stays away a window after it; the
//               silence clock restarts on the return to receive.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: a direct-only replace while media
//               rides the tunnel, at the PathRacer::kUpgradeRetryMs steps;
//               the no-packets fallback back to the tunnel on a silent
//               direct path; the older relay-leg check judges the
//               connection in use. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: JJ's ruling of 2026-09-28: waterfall AGC and NF-AGC use the
//               Core's display-extras levels, as the phone does, so the dBm
//               window no longer follows them (no new request as AGC
//               settles). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-28: R-IOS-18: the dBm window follows the pan's normalise only
//               when it applies (SpectrumWidget::normalizeActive: Average,
//               Sample or RMS). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone uplink
//               (see RemoteMediaController.h). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan, desktop remote transmit (R-IOS-13): the
//               uplink's production callers (the window's transmit client
//               and the Core's mirrored VOX). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the transmit keepalive
//               goes on this media connection's "tx" data channel while it
//               is open. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-26: transmit group fix wave: I4 setTransmitHolder fed from
//               txState's holder; M6 the microphone streams unkeyed only for
//               VOX this window armed. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: transmit group fix wave 2 (M8): micLineOpen and
//               micLineChanged, so VOX shows disabled with its reason while
//               this computer has no microphone line to the Core. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 28 (R-R3-49, A11, R-IOS-13): see
//               RemoteMediaController.h. A Core at
//               txDisplayVersion 1 is told at media start that this window
//               takes its transmit display; each subscribe carries the pan's
//               transmit window (txMinDbm, txMaxDbm), and a context the Core
//               marks `transmit` and its frames are handed on
//               (transmitContextReceived, transmitFrameReceived) instead of
//               being drawn as receive; drawing them is Task 29. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 29 (A11, R-R3-49): see RemoteMediaController.h
//               (setPanTransmitting, refreshTransmitView). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Tasks 27-29 fix wave (R-R3-49): each binding holds
//               its accepted transmit context (heldTransmitContext). J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: R-R3-21 / R-R3-08: display frames wait in a per-pan
//               presenter and are drawn at their Core time plus the audio's
//               delay (displayClockVersion 1; clock probes run while the
//               display does), gap rows blend or repeat, and the display
//               counters. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-26: merge of parity Tasks 28-29 with R-R3-21 (R-R3-49,
//               R-R3-21, R-R3-08): transmit frames bypass the presenter; the
//               keying hold also applies at presentation time; a key forgets
//               the queued receive frames and an unkey restarts the blend
//               chain; presented frames are checked against
//               acceptedGeneration(). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the media connection of
//               a session through the remote access service uses its ICE
//               settings, and its connection stage the gathering bound
//               more. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-27: the station display keys' defaults and the FFT sizes a
//               pan asks for come from ControlRanges.h, the table the
//               catalogue's `display` key reads (R-IOS-18, R-IOS-27,
//               R-R3-08). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: parity Task 31 (A11, R-R3-49): display duplex; the media
//               start declares txDisplayVersion 3 to a Core that sends 3, and
//               the subscribes carry `duplex` true while DUP is on. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor; the
//               media start declares txMonitorAudioVersion 1 to a Core that
//               sends it, and monitor-audio carries this window's MON output
//               on change and when media is ready. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): media `replace` with
//               dual receive (DualPathAudio, RtpDuplicateFilter) when the
//               session moves; its fix wave follows every move, retrying a
//               pending replacement until the media connection is ready and
//               the radio is on receive. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include "gui/RemoteMediaController.h"
#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/Resampler.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/session/PathRacer.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/session/media/RemoteTciAudioStage.h"
#include "core/session/media/RemoteAudioRestartBackoff.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DualPathAudio.h"
#include "core/session/IceConfiguration.h"
#include "core/ClarityController.h"
#include "core/spectrum/SpectrumReducer.h"
#include "core/ControlRanges.h"
#include "core/FFTEngine.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "models/TransmitModel.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/AudioJitterBuffer.h"
#include "gui/RemoteDisplayPresenter.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/RemoteIqCodec.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/session/media/WidebandDisplayContext.h"
#include "core/spectrum/DisplayFollowers.h"
#include "gui/DssGeometry.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteDisplayAllocator.h"
#include "gui/RemoteGeneration.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/Ps3DisplayCodec.h"
#include "models/SliceModel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QPointer>
#include <QScopeGuard>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QThread>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

namespace NereusSDR {
Q_LOGGING_CATEGORY(lcRemoteMedia, "nereus.remote.media")
namespace {
constexpr int kMaxEndpoints = 8;
constexpr int kDefaultAllocationAckTimeoutMs = kDisplayAllocationAckTimeoutMs;
// The derivation in RemoteMediaController.h, checked: one control heartbeat
// interval for Core's description, then the pinned library's slowest serial
// failure (ICE 39.5 s, DTLS 31 s, SCTP 35 s) for the connection.
constexpr int kLibraryIcePacTimeoutMs = 39'500;   // libjuice agent.h:43 [@3c40a354]
constexpr int kLibraryDtlsHandshakeFailMs = 31'000; // dtlstransport.cpp:1024-1031
constexpr int kLibrarySctpInitFailMs = 35'000;    // sctptransport.cpp:127-142
static_assert(RemoteMediaController::kMediaDescriptionDeadlineMs
                  == StationClient::kDefaultHeartbeatIntervalMs,
              "the description stage must be one control heartbeat interval");
static_assert(RemoteMediaController::kMediaConnectDeadlineMs
                  == kLibraryIcePacTimeoutMs + kLibraryDtlsHandshakeFailMs
                         + kLibrarySctpInitFailMs,
              "the connection stage must be the library's slowest serial failure");
// iPhone app plan Task 28 (R-IOS-16): through the remote access service
// the media connection gathers first (STUN and the relay, up to
// IceConfiguration::kGatheringDeadlineMs) before the library's own stages
// run, so its connection stage is that much longer.
int connectStageMs(int connectDeadlineMs, bool throughService)
{
    return connectDeadlineMs + (throughService ? IceConfiguration::kGatheringDeadlineMs : 0);
}

// The audio status refresh, which runs only while a receiver runs or a
// playback problem awaits recovery.
constexpr int kAudioStatusRefreshMs = 250;
// R-R3-23: how often the lossless link trial samples playback. Its windows
// are RemoteAudioLinkTrial::kWindowMs long; a 1 s sample closes each within
// a second of its end.
constexpr int kLinkTrialSampleMs = 1000;
// R-R3-35: probes kept awaiting their echo; an older one is forgotten.
constexpr std::size_t kMaxPendingClockProbes = 8;
// The receiver restarts that say audio arrived badly, which the link trial
// counts. Speaker, decoder and clock faults are this computer's own.
bool linkInterruption(RemoteAudioReceiver::Fault fault)
{
    // R-R3-21: bursts, gaps and stalls no longer restart; the link trial
    // reads them from the receiver's linkInterruptions instead.
    return fault == RemoteAudioReceiver::Fault::NoPackets;
}

RemoteAudioQualityChoice storedAudioQualityChoice()
{
    const QString value = AppSettings::instance()
        .value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("High"))
        .toString();
    if (value == QLatin1String("Lossless")) { return RemoteAudioQualityChoice::Lossless; }
    if (value == QLatin1String("SaveData")) { return RemoteAudioQualityChoice::SaveData; }
    if (value == QLatin1String("Opus")) {
        AppSettings::instance().setValue(QLatin1String(RemoteMediaController::kAudioProfileSettingKey),
                                         QStringLiteral("High"));
    }
    return RemoteAudioQualityChoice::High;
}
// R-R3-45: the link trial's stream number for the headphones mix (the
// speakers' mix is -1, a receiver stream its slice id).
constexpr int kHeadphonesTrialStream = -2;

// Speaker progress this recent means audio is playing now: the window the
// title bar has always used.
constexpr qint64 kPlaybackProgressWindowMs = 500;

// The speaker device the operator selected. It names the selection, which is
// not proof that the device is the one in use.
QString selectedSpeakerOutput()
{
    const AudioDeviceConfig speakers =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
    return speakers.deviceName.isEmpty() ? QStringLiteral("System default")
                                         : speakers.deviceName;
}

// A clock reading in whole nanoseconds, never negative (R-R3-35).
bool nanoseconds(const QJsonObject& object, const char* key, qint64& value)
{
    const QJsonValue json = object.value(QLatin1String(key));
    value = json.isDouble() ? json.toInteger(-1) : -1;
    return value >= 0;
}

bool number(const QJsonObject& object, const char* key, double low, double high,
            double& value, bool integer = false)
{
    const QJsonValue json = object.value(QLatin1String(key));
    if (!json.isDouble()) {
        return false;
    }
    value = json.toDouble();
    return std::isfinite(value) && value >= low && value <= high
        && (!integer || std::floor(value) == value);
}

bool uint32(const QJsonObject& object, const char* key, quint32& value)
{
    double parsed = 0.0;
    if (!number(object, key, 1, std::numeric_limits<quint32>::max(), parsed, true)) {
        return false;
    }
    value = static_cast<quint32>(parsed);
    return true;
}

bool uint64(const QJsonObject& object, const char* key, quint64 maximum,
            quint64& value, bool allowZero = true)
{
    double parsed = 0.0;
    if (!number(object, key, allowZero ? 0 : 1, static_cast<double>(maximum),
                parsed, true)) {
        return false;
    }
    value = static_cast<quint64>(parsed);
    return true;
}

// Log wording only: the profile Core reported for this context, never a
// profile this GUI assumes.
QString reportedAudioProfile(const RemoteAudioContextMessage& context)
{
    if (const std::optional<PcmEncoderProfile>& lossless = context.losslessEncoder) {
        return QStringLiteral("lossless L16 %1 Hz, %2 channels, %3-sample packets, "
                              "%4-bit, payload type %5")
            .arg(lossless->sampleRate)
            .arg(lossless->channels)
            .arg(lossless->frameSamples)
            .arg(lossless->bitsPerSample)
            .arg(lossless->payloadType);
    }
    const std::optional<OpusEncoderProfile>& encoder = context.encoder;
    if (!encoder) {
        return QStringLiteral("codec profile not reported by Core");
    }
    return QStringLiteral("Opus %1 Hz, %2 channels, %3-sample frames, "
                          "target %4 bit/s, audio bandwidth %5 Hz")
        .arg(encoder->sampleRate)
        .arg(encoder->channels)
        .arg(encoder->frameSamples)
        .arg(encoder->targetBitrate)
        .arg(encoder->audioBandwidthHz);
}

// The zoom-detail limit a grant reports, for the pan's status line
// (R-R3-37); false when nothing limited it.
bool applyGrantLimit(PanDisplayState& state, const std::optional<SpectrumContextGrant>& grant)
{
    if (!grant) {
        return false;
    }
    switch (grant->limit) {
    case SpectrumLimitReason::None:
        return false;
    case SpectrumLimitReason::LargestSize:
        state.zoomLimit = PanDisplayState::ZoomLimit::LargestSize;
        return true;
    case SpectrumLimitReason::SharedEngine:
        state.zoomLimit = PanDisplayState::ZoomLimit::SharedEngine;
        return true;
    case SpectrumLimitReason::SourceBins:
        state.zoomLimit = PanDisplayState::ZoomLimit::SourceBins;
        state.zoomPoints = grant->grantedPixels;
        return true;
    }
    return false;
}

PanDisplayState refusedState(const QString& reason)
{
    PanDisplayState state;
    state.phase = PanDisplayState::Phase::Refused;
    state.refusalReason = reason;
    return state;
}

PanDisplayState phaseState(PanDisplayState::Phase phase)
{
    PanDisplayState state;
    state.phase = phase;
    return state;
}

// Log wording only.
QString grantLogLine(const SpectrumContextGrant& grant)
{
    return QStringLiteral("FFT %1 (%2 tier), %3 of %4 points, limit %5")
        .arg(grant.grantedFftSize)
        .arg(grant.grantedTier == FftTier::Fine ? QStringLiteral("fine") : QStringLiteral("wide"))
        .arg(grant.grantedPixels)
        .arg(grant.requestedPixels)
        .arg(spectrumLimitReasonToWire(grant.limit));
}

bool geometryNeedsWideband(double centreHz, double spanHz,
                           double sourceCentreHz, double sourceRateHz)
{
    const double sourceLow = sourceCentreHz - sourceRateHz * 0.5;
    const double sourceHigh = sourceCentreHz + sourceRateHz * 0.5;
    const double viewLow = centreHz - spanHz * 0.5;
    const double viewHigh = centreHz + spanHz * 0.5;
    // Inactive endpoint geometry is derived from clamped FFT bin edges and
    // can legitimately land exactly on either DDC edge. Expand by one ULP
    // solely for arithmetic roundoff; this is not a bin-width allowance.
    const double lowLimit = std::nextafter(sourceLow,
                                           -std::numeric_limits<double>::infinity());
    const double highLimit = std::nextafter(sourceHigh,
                                            std::numeric_limits<double>::infinity());
    return viewLow < lowLimit || viewHigh > highLimit;
}

// The smallest power of two from ControlRanges::kDisplayFftPlanMinSize up
// that reaches `target`, at most the engine's largest (the catalogue's
// `display.fftPlan`, which an app follows to ask for the same sizes).
int fftSizeFor(double target)
{
    static_assert(ControlRanges::kDisplayFftPlanMaxSize == FFTEngine::maximumFftSize());
    int size = ControlRanges::kDisplayFftPlanMinSize;
    while (size < FFTEngine::maximumFftSize() && size < target) {
        size *= 2;
    }
    return size;
}

QJsonObject plane(int detector, int averageMode, double alpha)
{
    return {{QStringLiteral("detector"), detector},
            {QStringLiteral("averageMode"), averageMode},
            {QStringLiteral("averageAlpha"), alpha}};
}

SliceModel* currentSliceForPan(RadioModel* model, PanadapterStack* stack,
                              SpectrumWidget* widget)
{
    if (!model || !stack || !widget) { return nullptr; }
    for (PanadapterApplet* applet : stack->allApplets()) {
        if (applet->spectrumWidget() == widget) {
            return model->sliceById(applet->activeSliceIndex());
        }
    }
    return nullptr;
}

// Parity Task 17 (R-R3-01, R-R3-04): the quantisation window a pan asks
// the Core for. The codec carries 256 levels between minDbm and maxDbm, so
// the window is what the pan shows: its own dBm range (refLevel down by
// its dynamic range, less the normalise shift the pan adds when it draws)
// widened by the waterfall's low and high levels, which the waterfall
// colours against the frame's values as they come. A signal above 0 dBm is
// then drawn at its level, and a 20 dB pan steps by 20/255 dB.
struct DbmWindow {
    double minDbm = -180.0;
    double maxDbm = 0.0;
    bool operator==(const DbmWindow& other) const
    {
        return minDbm == other.minDbm && maxDbm == other.maxDbm;
    }
    bool operator!=(const DbmWindow& other) const { return !(*this == other); }
};

// What requestFor adds from this window's state: the settled dBm window
// (none: the pan's window as it is now) and, for a Core at
// spectrumGrantVersion 2, Rendering > Decimation.
struct RequestInputs {
    std::optional<DbmWindow> dbmWindow;
    std::optional<int> decimation;
    // Parity Task 28: the transmit window, for a Core told txDisplayVersion.
    std::optional<DbmWindow> txDbmWindow;
    // Parity Task 31: display duplex, for a Core told txDisplayVersion 3.
    bool duplex = false;
    // JJ's ruling of 2026-09-28: the Core offers display extras, so a pan's
    // waterfall AGC or NF-AGC asks it for its levels (`waterfallLevels`).
    bool coreWaterfallLevels = false;
};

// Parity Task 17 follow-up (R-R3-01, R-R3-04): the headroom a waterfall
// level set at run time (waterfall AGC, noise-floor AGC, Clarity) is given
// in the dBm window, and twice it the slack the window keeps before it
// narrows. Those levels move a little every line; the window follows them
// only when they leave it or it is far wider than they need, so the Core
// is not asked again at every line.
constexpr double kRuntimeLevelHeadroomDb = 10.0;

// How far below the pan's floor (or the stored low level, when lower) a
// run-time low level may take the window. Waterfall AGC works on the values
// the pan receives, which the Core clamps to the window: with nothing below
// the window's floor (a receiver passing no noise) its low level sits one
// AGC margin under the edge whatever the edge is, and would walk the window
// down to -400 dBm one request at a time. A real noise floor is found well
// inside this.
constexpr double kRuntimeLowLevelReachDb = 60.0;

// Whether the pan's waterfall levels are set at run time rather than the
// stored low and high levels (SpectrumWidget::composeWaterfallActiveThresholds).
// A pan whose AGC levels come from the Core (coreWaterfallLevelsInUse) is
// coloured against levels the Core computed before it clamped the rows, so
// its window is the stored levels', as with manual levels: AGC settling
// asks the Core nothing.
bool runtimeWaterfallLevels(const SpectrumWidget* widget)
{
    if (widget->coreWaterfallLevelsInUse()) {
        return false;
    }
    return widget->clarityActive() || widget->wfAgcEnabled()
        || widget->waterfallNFAGCEnabled();
}

// The waterfall levels the dBm window must hold: the stored levels while
// they are what colours the waterfall; otherwise the levels in force now
// (wfActiveLow/HighThreshold, which AGC and Clarity set) with headroom,
// kept from `held` while they still fit it.
// `panLowDbm` is the pan's own floor in the frame's values (liveDbmWindow).
DbmWindow waterfallLevelsWindow(const SpectrumWidget* widget,
                                const std::optional<DbmWindow>& held, double panLowDbm)
{
    const DbmWindow stored{double(widget->wfLowThreshold()), double(widget->wfHighThreshold())};
    if (!runtimeWaterfallLevels(widget)) {
        return stored;
    }
    // Clarity's levels come from the Core's noise floor of the whole
    // source, not from these values, so only the AGCs are held to the reach.
    double activeLow = double(widget->wfActiveLowThreshold());
    if (!widget->clarityActive()) {
        const double reach = std::floor(std::min(panLowDbm, stored.minDbm)
                                        - kRuntimeLowLevelReachDb);
        activeLow = std::max(activeLow, reach + kRuntimeLevelHeadroomDb);
    }
    const double activeHigh = std::max(double(widget->wfActiveHighThreshold()), activeLow);
    if (!std::isfinite(activeLow) || !std::isfinite(activeHigh)) {
        return held.value_or(stored);
    }
    if (held && activeLow >= held->minDbm && activeHigh <= held->maxDbm
        && held->minDbm >= activeLow - 2.0 * kRuntimeLevelHeadroomDb
        && held->maxDbm <= activeHigh + 2.0 * kRuntimeLevelHeadroomDb) {
        return *held;
    }
    return {std::floor(activeLow - kRuntimeLevelHeadroomDb),
            std::ceil(activeHigh + kRuntimeLevelHeadroomDb)};
}

// The pan's own floor, in the frame's un-normalised values.
double panLowDbm(const SpectrumWidget* widget, double binWidthHz)
{
    return double(widget->refLevel()) - normalizeShiftDb(widget->normalizeActive(), binWidthHz)
        - double(widget->dynamicRange());
}

DbmWindow liveDbmWindow(const SpectrumWidget* widget, double binWidthHz,
                        const DbmWindow& levels)
{
    const double shift = normalizeShiftDb(widget->normalizeActive(), binWidthHz);
    const double panHigh = double(widget->refLevel()) - shift;
    const double panLow = panLowDbm(widget, binWidthHz);
    // Parity Task 17 follow-up: the waterfall's levels colour the frame's
    // values directly, so the window holds the levels in force (the run-time
    // ones with waterfall AGC or Clarity), and no colour clips at its edge.
    double low = std::min(panLow, levels.minDbm);
    double high = std::max(panHigh, levels.maxDbm);
    if (!std::isfinite(low) || !std::isfinite(high)) {
        return {};
    }
    low = std::clamp(low, kMinDbmLimit, kMaxDbmLimit - 1.0);
    high = std::clamp(high, low + 1.0, kMaxDbmLimit);
    // Whole tenths of a dB: a window that differs by less than that paints
    // the same, and must not ask the Core again.
    return {std::floor(low * 10.0) / 10.0, std::ceil(high * 10.0) / 10.0};
}

int requestedFps()
{
    return qBound(1, AppSettings::instance().value(
                         QLatin1String(ControlRanges::kDisplaySpectrumFpsKey),
                         QString::number(ControlRanges::kDisplaySpectrumFpsDefault))
                         .toString().toInt(), 60);
}

// The FFT size a pan asks for: the stored size, raised by a deep zoom or
// the Hz/bin target. 0 when the pan cannot ask yet.
int plannedFftSize(SpectrumWidget* widget, SliceModel* slice, int* baseSizeOut = nullptr)
{
    auto& settings = AppSettings::instance();
    const int baseSize = fftSizeFor(settings.value(
        QLatin1String(ControlRanges::kDisplayFftSizeKey),
        QString::number(ControlRanges::kDisplayFftSizeDefault)).toString().toInt());
    if (baseSizeOut) { *baseSizeOut = baseSize; }
    const int pixels = qBound(1, widget->width() - widget->reservedRightEdgeWidth(),
                             SpectrumEndpoint::kMaxPixels);
    const double span = widget->bandwidth();
    if (!std::isfinite(span) || span <= 0 || !std::isfinite(widget->centerFrequency())
        || slice->sampleRateHz() <= 0) {
        return 0;
    }
    // R-R3-08: a deep zoom requests its own tier; it cannot lengthen the
    // shared Wide engine. Size is capped to the actual FFT engine limit.
    double target = double(slice->sampleRateHz()) * pixels / span;
    const double hzPerBin = settings.value(
        QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey),
        QString::number(ControlRanges::kDisplayHzPerBinTargetDefault)).toString().toDouble();
    if (std::isfinite(hzPerBin) && hzPerBin > 0) {
        target = std::max(target, slice->sampleRateHz() / hzPerBin);
    }
    return std::max(baseSize, fftSizeFor(target));
}

// Parity Task 17 (R-R3-12): the averaging constants for the rate the Core
// will send this pan at, from the pan's own averaging times, as the Core
// computes an app's (averageAlphaForTimeMs, display extras v1).
void setAveragingFor(QJsonObject& request, const SpectrumWidget* widget, int fps)
{
    for (const auto& [key, timeMs] :
         {std::pair{QStringLiteral("trace"), widget->spectrumAverageTimeMs()},
          std::pair{QStringLiteral("waterfall"), widget->waterfallAverageTimeMs()}}) {
        QJsonObject plane = request.value(key).toObject();
        plane.insert(QStringLiteral("averageAlpha"),
                     double(averageAlphaForTimeMs(timeMs, std::max(1, fps))));
        request.insert(key, plane);
    }
}

QJsonObject requestFor(SpectrumWidget* widget, SliceModel* slice,
                       bool remoteWidebandAvailable, const RequestInputs& inputs = {})
{
    auto& settings = AppSettings::instance();
    const int fps = requestedFps();
    const int window = qBound(0, settings.value(QLatin1String(ControlRanges::kDisplayFftWindowKey),
                                    QString::number(ControlRanges::kDisplayFftWindowDefault))
                                    .toString().toInt(), int(WindowFunction::Count) - 1);
    const int pixels = qBound(1, widget->width() - widget->reservedRightEdgeWidth(),
                             SpectrumEndpoint::kMaxPixels);
    const double span = widget->bandwidth();
    int baseSize = 0;
    const int size = plannedFftSize(widget, slice, &baseSize);
    if (size <= 0) {
        return {};
    }
    const int framesPerLine = qBound(1, int(std::ceil(
        double(widget->wfUpdatePeriodMs()) * fps / 1000.0)), kMaxFramesPerLine);
    const double wideFactor = widget->spectrumRenderMode() == int(SpectrumRenderMode::Mode3D)
        ? dssMaxRowSpanFactor(dssShapeForAngle(0)) : 0.0;
    const double binWidthHz = double(slice->sampleRateHz()) / size;
    const DbmWindow dbm = inputs.dbmWindow.value_or(
        liveDbmWindow(widget, binWidthHz,
                      waterfallLevelsWindow(widget, std::nullopt,
                                            panLowDbm(widget, binWidthHz))));
    QJsonObject request{{QStringLiteral("sliceId"), slice->sliceIndex()},
            {QStringLiteral("tier"), size > baseSize ? QStringLiteral("fine") : QStringLiteral("wide")},
            {QStringLiteral("fftSize"), size}, {QStringLiteral("windowType"), window},
            {QStringLiteral("centreHz"), widget->centerFrequency()},
            {QStringLiteral("spanHz"), span}, {QStringLiteral("pixels"), pixels},
            {QStringLiteral("fps"), fps}, {QStringLiteral("framesPerLine"), framesPerLine},
            {QStringLiteral("trace"), plane(int(widget->spectrumDetector()),
                int(widget->spectrumAveraging()), widget->spectrumAverageAlpha())},
            {QStringLiteral("waterfall"), plane(int(widget->waterfallDetector()),
                int(widget->waterfallAveraging()), widget->waterfallAverageAlpha())},
            {QStringLiteral("minDbm"), dbm.minDbm}, {QStringLiteral("maxDbm"), dbm.maxDbm},
            {QStringLiteral("wideSpanFactor"), wideFactor}};
    setAveragingFor(request, widget, fps);
    if (remoteWidebandAvailable) {
        // This is permission, not current demand. Core derives demand from
        // the accepted span and reports the resulting active state.
        request.insert(QStringLiteral("extendedView"), widget->extendedViewAllowed());
    }
    if (inputs.decimation) {
        // Parity Task 17 (R-R3-01): spectrumGrantVersion 2.
        request.insert(QStringLiteral("decimation"), *inputs.decimation);
    }
    if (inputs.coreWaterfallLevels && widget->coreWaterfallLevelsInUse()) {
        // JJ's ruling of 2026-09-28 (display extras v1, `waterfallLevels`):
        // the Core runs the pan's waterfall AGC (the desktop's follower,
        // 12 dB outside each line's lowest and highest values) or NF-AGC
        // (the line's floor plus the offset, 60 dB above it) and sends the
        // levels beside each frame.
        request.insert(QStringLiteral("waterfallLevels"), QJsonObject{
            {QStringLiteral("mode"), widget->wfAgcEnabled() ? QStringLiteral("agc")
                                                            : QStringLiteral("noiseFloorAgc")},
            {QStringLiteral("lowDbm"), double(widget->wfLowThreshold())},
            {QStringLiteral("highDbm"), double(widget->wfHighThreshold())},
            {QStringLiteral("offsetDb"), widget->waterfallAGCOffsetDb()}});
    }
    if (inputs.txDbmWindow) {
        // Parity Task 28 (R-R3-49, A11): txDisplayVersion 1.
        request.insert(QStringLiteral("txMinDbm"), inputs.txDbmWindow->minDbm);
        request.insert(QStringLiteral("txMaxDbm"), inputs.txDbmWindow->maxDbm);
    }
    if (inputs.duplex) {
        // Parity Task 31 (R-R3-49, A11): txDisplayVersion 3; absent is false.
        request.insert(QStringLiteral("duplex"), true);
    }
    return request;
}

SpectrumWidget* miniSettingsWidget(PanadapterStack* stack, int sliceId)
{
    if (!stack) { return nullptr; }
    SpectrumWidget* fallback = nullptr;
    for (PanadapterApplet* applet : stack->allApplets()) {
        if (!applet || !applet->spectrumWidget()) { continue; }
        if (applet->activeSliceIndex() == sliceId) { return applet->spectrumWidget(); }
        if (!fallback) { fallback = applet->spectrumWidget(); }
    }
    return fallback;
}

// Parity Task 28 (R-R3-49, A11): the window the Core quantises the pan's
// transmit display to: its transmit grid (reference level down by its
// dynamic range) widened by the transmit waterfall levels, which colour the
// frame's values as they come (Thetis display.cs:6420-6427 [v2.10.3.15],
// TXWFAmpMin / TXWFAmpMax while keyed). Each edge is rounded outward to a
// tenth of a dB and held inside -400 to 100, as the receive window is.
DbmWindow transmitDbmWindow(const SpectrumWidget* widget)
{
    const double gridMax = widget->transmitRefLevel();
    const double gridMin = gridMax - widget->transmitDynamicRange();
    double low = std::min(gridMin, double(widget->txWfLowLevel()));
    double high = std::max(gridMax, double(widget->txWfHighLevel()));
    low = std::clamp(std::floor(low * 10.0) / 10.0, kMinDbmLimit, kMaxDbmLimit);
    high = std::clamp(std::ceil(high * 10.0) / 10.0, kMinDbmLimit, kMaxDbmLimit);
    if (high <= low) {
        if (low > kMinDbmLimit) {
            low = high - 0.1;
        } else {
            high = low + 0.1;
        }
    }
    return {low, high};
}

QJsonObject allocatedRequest(QJsonObject request, const RemoteDisplayQuality& quality,
                             const SpectrumWidget* widget)
{
    request.insert(QStringLiteral("pixels"), quality.pixels);
    request.insert(QStringLiteral("fps"), quality.fps);
    request.insert(QStringLiteral("framesPerLine"), quality.framesPerLine);
    // Parity Task 17 (R-R3-12): the averaging times hold at the rate the
    // display budget leaves this pan.
    if (widget) {
        setAveragingFor(request, widget, quality.fps);
    }
    return request;
}

bool sameOriginalIntent(QJsonObject left, QJsonObject right)
{
    for (const QString& key : {QStringLiteral("pixels"), QStringLiteral("fps"),
                               QStringLiteral("framesPerLine")}) {
        left.remove(key);
        right.remove(key);
    }
    // The averaging constants follow the allocated rate (parity Task 17).
    for (const QString& key : {QStringLiteral("trace"), QStringLiteral("waterfall")}) {
        for (QJsonObject* side : {&left, &right}) {
            QJsonObject plane = side->value(key).toObject();
            plane.remove(QStringLiteral("averageAlpha"));
            side->insert(key, plane);
        }
    }
    return left == right;
}

bool zeroCharge(const DisplayBudgetCharge& charge)
{
    return charge.applicationBytesPerSecond == 0
        && charge.spectrumSampleUnitsPerSecond == 0
        && charge.messagesPerSecond == 0;
}

bool nonIncreasing(const DisplayBudgetCharge& next, const DisplayBudgetCharge& previous)
{
    return next.applicationBytesPerSecond <= previous.applicationBytesPerSecond
        && next.spectrumSampleUnitsPerSecond <= previous.spectrumSampleUnitsPerSecond
        && next.messagesPerSecond <= previous.messagesPerSecond;
}

DisplayBudgetCharge maximumCharge(const DisplayBudgetCharge& left,
                                  const DisplayBudgetCharge& right)
{
    return {std::max(left.applicationBytesPerSecond, right.applicationBytesPerSecond),
            std::max(left.spectrumSampleUnitsPerSecond,
                     right.spectrumSampleUnitsPerSecond),
            std::max(left.messagesPerSecond, right.messagesPerSecond)};
}

QString requestIdentity(const QJsonObject& request, quint32 generation, bool ps3Desired)
{
    QJsonObject identified = request;
    identified.insert(QStringLiteral("budgetGeneration"), static_cast<qint64>(generation));
    identified.insert(QStringLiteral("ps3Desired"), ps3Desired);
    return QString::fromUtf8(QJsonDocument(identified).toJson(QJsonDocument::Compact));
}

QString allocationIdentity(const DisplayBudgetLimits& limits,
                           const QList<RemoteDisplayIntent>& intents,
                           bool ps3Enabled, bool retainedPs3ExceedsCap)
{
    QString key = QStringLiteral("%1:%2:%3:%4:%5")
        .arg(limits.applicationBytesPerSecond)
        .arg(limits.spectrumSampleUnitsPerSecond)
        .arg(limits.generation)
        .arg(ps3Enabled)
        .arg(retainedPs3ExceedsCap);
    for (const RemoteDisplayIntent& intent : intents) {
        key += QStringLiteral("|%1:%2:%3:%4:%5:%6:%7:%8")
            .arg(intent.panId).arg(intent.pixels).arg(intent.fps)
            .arg(intent.includeWidePlane).arg(intent.waterfallPeriodMs)
            .arg(intent.active).arg(int(intent.kind)).arg(int(intent.extrasSections));
    }
    return key;
}
} // namespace

// R-R3-21: the display arrival gap reported is the largest of the last 10 s.
constexpr qint64 kDisplayArrivalWindowNs = 10'000'000'000;

struct RemoteMediaController::Private {
    struct Binding {
        enum class PendingKind { Subscribe, Unsubscribe };
        struct Pending {
            PendingKind kind = PendingKind::Subscribe;
            quint32 revision = 0;
            QJsonObject request;
            DisplayBudgetCharge charge;
            QString identity;
            qint64 sentAtMs = 0;
            bool timedOut = false;
        };
        QString panId;
        int miniSliceId = -1; // -1 is the existing pan destination.
        bool isMini() const { return miniSliceId >= 0; }
        QPointer<SpectrumWidget> widget;
        QPointer<SliceModel> slice;
        QJsonObject observed;
        QJsonObject desiredOriginal;
        QJsonObject acceptedRequest;
        int observedStream = -1;
        quint64 observedStreamEpoch = 0;
        quint32 revision = 0;
        quint32 acceptedRevision = 0;
        quint32 contextRevision = 0;
        DisplayBudgetCharge acceptedCharge;
        std::optional<Pending> pending;
        SpectrumEndpointContext context;
        RemoteSpectrumCapture capture;
        /// What Core granted, as the accepted minor-9 context reported it.
        std::optional<SpectrumContextGrant> grant;
        /// R-R3-01/R-R3-08/R-R3-37: Core granted fewer pixels than asked
        /// with no limit named (a lone pan kept at the charge it was first
        /// admitted at). The same request goes out once more as an
        /// increase; askedAgain is that request, so a second short answer
        /// to it is not asked again.
        bool askAgain = false;
        QJsonObject askedAgain;
        double sourceCentreHz = 0;
        DisplayCodecDecoder decoder;
        QVector<float> lastMiniTrace;
        // R-R3-21 / R-R3-08: decoded frames waiting for their presentation
        // time, and the rows filling a lost message's gap.
        RemoteDisplayPresenter presenter;
        // This pan's display arrivals (audio clock) and the waits between
        // them, each with its arrival time, for the last 10 s.
        std::optional<qint64> lastArrivalNs;
        std::deque<std::pair<qint64, qint64>> arrivalGaps;
        qint64 lastKeyframeMs = -1000;
        bool accepted = false;
        bool rejected = false;
        bool retiring = false;
        bool suspending = false;
        bool receivedNoiseFloor = false;
        /// Parity Task 28: the Core's transmit display is on this pan
        /// (`transmit` true): its frames, at transmitGeneration, are handed
        /// on and not drawn. `context`, `sourceCentreHz` and `grant` stay the
        /// receive context's until the fall's context replaces them.
        bool transmit = false;
        quint32 transmitGeneration = 0;
        /// The accepted transmit context, while `transmit` is true.
        std::optional<SpectrumContextMessage> transmitContext;
        /// The newest context generation accepted, receive or transmit.
        quint32 acceptedGeneration() const
        {
            return transmit ? transmitGeneration : context.codec.contextGeneration;
        }
        /// R-R3-37: when this pan began waiting for an accepted display
        /// (allocationClock), or -1 while it has one. See kPanWaitingGraceMs.
        qint64 waitingSinceMs = -1;
        QString refusedIdentity;
        QString refusalReason;
        QList<qint64> receivedFrameTimesMs;
        QMetaObject::Connection ctunGesture;
        QMetaObject::Connection centreGesture;
        ~Binding() {
            QObject::disconnect(ctunGesture);
            QObject::disconnect(centreGesture);
        }
    };
    QPointer<StationClient> client;
    QPointer<RadioModel> model;
    QPointer<PanadapterStack> stack;
    QSet<int> miniWanted;
    // Parity Task 28: this media start declared txDisplayVersion.
    bool txDisplayNegotiated = false;
    // Parity Task 31: this media start declared txDisplayVersion 3, and the
    // window's DUP as MoxDisplayController applies it.
    bool displayDuplexNegotiated = false;
    bool displayDuplex = false;
    // Parity Task 29: pans showing the transmit display (receive frames
    // held), and those whose Core sends none (the status line says so).
    QSet<QString> transmittingPans;
    QSet<QString> transmitDisplayMissingPans;
    QPointer<MediaPeer> peer;
    // iPhone app plan Task 29 (R-IOS-16): a replacement media connection
    // under way (its id), the old one still draining after it took over,
    // their bounds, and dual receive: every peer's SSRC to the stream's own
    // (the SSRCs this media session started with, main, receivers 0 to 3
    // and headphones), and each audio packet taken once while two
    // connections carry it.
    QPointer<MediaPeer> replacement;
    QString replacementId;
    QPointer<MediaPeer> retiring;
    QTimer* replaceDeadline = nullptr;
    QTimer* retireTimer = nullptr;
    // Task 29 fix wave (Important 1): a move not yet followed by media.
    bool replacePending = false;
    // The follow-up fix: the kind of the move waiting (a refused silence
    // fallback stays a fallback onto the tunnel alone when it is retried).
    ReplaceKind pendingReplaceKind = ReplaceKind::Normal;
    // A session move came while that refused fallback waited: when media
    // comes back the move is still followed, as a normal replace.
    bool pendingMoveFolded = false;
    // The replace under way carries a session move folded into a waiting
    // fallback (a refusal while transmitting keeps that move pending).
    bool replacementMoveFolded = false;
    int replaceRearms = 0;
    // Task 29 step 2b: this media start declared the media tunnel.
    bool tunnelNegotiated = false;
    bool relayRoutingNegotiated = false;
    // Step 2b: when audio last came, and the check for a stall.
    QElapsedTimer lastAudio;
    QTimer* stallTimer = nullptr;
    // The direct media ladder: whether the connection in use and the one
    // being started route over the tunnel's own framing (the older relay
    // leg refusal judges the connection in use), and whether the one being
    // started is a direct-only replace.
    bool currentRouted = false;
    bool replacementRouted = false;
    bool replacementDirect = false;
    // The direct media ladder: the next direct-only replace while media
    // rides the tunnel (PathRacer::kUpgradeRetryMs steps), and when a media
    // packet last came (allocationClock; -1 before the first).
    QTimer* directUpgrade = nullptr;
    int directUpgradeStep = 0;
    qint64 lastMediaMs = -1;
    // The fix wave: the silence fallback ran for this silence (cleared when
    // a media packet comes), the replace being started is that fallback,
    // and when it finished (allocationClock; -1 when none is waiting on
    // media to return).
    bool silenceFallbackFired = false;
    bool replacementFallback = false;
    qint64 fallbackFinishedMs = -1;
    bool txSilenceSuppressed = false;
    bool coreTransmitting() const
    {
        if (!client || client->capabilities().txStateVersion < 1) { return false; }
        const TransmitState* tx = client->transmitState();
        return tx && (tx->keyed() || tx->tuning() || tx->txEnding());
    }
    bool replaceStartFailed = false;
    QTimer* replaceRetry = nullptr;
    std::unique_ptr<DualPathAudio> dual;
    QPointer<MediaPeer> dualNew;
    QTimer* dualTimer = nullptr;
    QElapsedTimer dualClock;
    quint64 duplicatesDropped = 0;
    quint64 dualFromOld = 0;
    quint64 dualFromNew = 0;
    QHash<quint32, quint32> toLogical;
    QList<quint32> logicalSsrcs;
    MediaPeer::TransportFactory factory;
    QTimer* timer = nullptr;
    QElapsedTimer clock;
    RemoteMediaController::AllocationClock allocationClock;
    int allocationAckTimeoutMs = kDefaultAllocationAckTimeoutMs;
    std::map<quint32, Binding> bindings;
    // The status each pan was last given, before its grant limit is added.
    QHash<QString, PanDisplayState> panBaseStatus;
    // R-R3-08/37: why each pan's display is below what it asked for, when
    // the Core said (CoreBusy); None for a pan at its requested quality.
    // The pan status builder maps it to words.
    QHash<QString, DisplayBudgetReason> panBudgetReason;
    // Parity Task 17 (R-R3-01, R-R3-04): each pan's dBm window, settled
    // once it has held for a frame period so a drag of the dBm strip asks
    // the Core again when it stops, not at every step.
    struct DbmWindowState {
        DbmWindow settled;
        DbmWindow live;
        qint64 liveSinceMs = 0;
        // Parity Task 17 follow-up: the waterfall levels the window holds
        // (waterfallLevelsWindow), kept while the run-time levels fit.
        DbmWindow levels;
    };
    QHash<const SpectrumWidget*, DbmWindowState> dbmWindows;

    /// The request this window would send for the pan now: requestFor with
    /// the pan's settled dBm window and, for a Core at spectrumGrantVersion
    /// 2, Rendering > Decimation (the value the Setup page keeps on this
    /// window's FFT engine, as a local window's page does).
    QJsonObject request(SpectrumWidget* widget, SliceModel* slice) const
    {
        RequestInputs inputs;
        if (const auto found = dbmWindows.constFind(widget); found != dbmWindows.cend()) {
            inputs.dbmWindow = found->settled;
        }
        if (client && client->spectrumDecimationAvailable()) {
            inputs.decimation = model && model->fftEngine()
                ? model->fftEngine()->decimation() : 1;
        }
        if (txDisplayNegotiated && widget) {
            inputs.txDbmWindow = transmitDbmWindow(widget);
        }
        inputs.duplex = displayDuplexNegotiated && displayDuplex;
        inputs.coreWaterfallLevels = coreWaterfallLevelsAvailable();
        return requestFor(widget, slice, client && client->remoteWidebandAvailable(), inputs);
    }

    /// JJ's ruling of 2026-09-28: the Core computes a pan's waterfall AGC
    /// levels when it offers display extras.
    bool coreWaterfallLevelsAvailable() const
    {
        return client && client->capabilities().displayExtrasVersion >= 1;
    }

    QJsonObject requestMini(SliceModel* slice) const
    {
        if (!client || client->capabilities().miniDisplayVersion < 1 || !model
            || !slice || slice->streamIndex() < 0 || slice->sampleRateHz() <= 0) { return {}; }
        // From Thetis console.cs:13221 and MeterManager.cs:43361-43363 [@3759d096],
        // 44463-44477 [v2.10.3.15]: the default 10 kHz RX half-width,
        // 20 kHz TX half-width, 1024 pixels and 30 output frames/s.
        constexpr double kRxSpanHz = 20'000.0;
        constexpr double kTxSpanHz = 40'000.0;
        constexpr int kPixels = 1024;
        constexpr int kFps = 30;
        const bool transmit = model->isTransmitting() && model->txBoundSlice() == slice;
        const double span = transmit ? kTxSpanHz : kRxSpanHz;
        const double centre = transmit ? double(model->txFrequencyForSlice(slice))
                                       : slice->frequency();
        const int base = fftSizeFor(AppSettings::instance().value(
            QLatin1String(ControlRanges::kDisplayFftSizeKey),
            QString::number(ControlRanges::kDisplayFftSizeDefault)).toString().toInt());
        const int size = std::max(base, fftSizeFor(double(slice->sampleRateHz())
                                                   * kPixels / span));
        const int window = qBound(0, AppSettings::instance().value(
            QLatin1String(ControlRanges::kDisplayFftWindowKey),
            QString::number(ControlRanges::kDisplayFftWindowDefault)).toString().toInt(),
            int(WindowFunction::Count) - 1);
        SpectrumWidget* source = miniSettingsWidget(stack, slice->sliceIndex());
        const double binWidthHz = double(slice->sampleRateHz()) / size;
        const DbmWindow dbm = source
            ? liveDbmWindow(source, binWidthHz,
                waterfallLevelsWindow(source, std::nullopt,
                    panLowDbm(source, binWidthHz)))
            : DbmWindow{-140.0, -40.0};
        QJsonObject request{{QStringLiteral("sliceId"), slice->sliceIndex()},
            {QStringLiteral("tier"), size > base ? QStringLiteral("fine")
                                                  : QStringLiteral("wide")},
            {QStringLiteral("fftSize"), size}, {QStringLiteral("windowType"), window},
            {QStringLiteral("centreHz"), centre}, {QStringLiteral("spanHz"), span},
            {QStringLiteral("pixels"), kPixels}, {QStringLiteral("fps"), kFps},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), plane(source ? int(source->spectrumDetector()) : 0,
                source ? int(source->spectrumAveraging()) : 0,
                source ? source->spectrumAverageAlpha() : 0.0)},
            {QStringLiteral("waterfall"), plane(source ? int(source->waterfallDetector()) : 0,
                source ? int(source->waterfallAveraging()) : 0,
                source ? source->waterfallAverageAlpha() : 0.0)},
            {QStringLiteral("minDbm"), dbm.minDbm},
            {QStringLiteral("maxDbm"), dbm.maxDbm},
            {QStringLiteral("wideSpanFactor"), 0.0},
            {QStringLiteral("displayRole"), QStringLiteral("mini")}};
        if (source) { setAveragingFor(request, source, kFps); }
        if (client->spectrumDecimationAvailable()) {
            request.insert(QStringLiteral("decimation"), model->fftEngine()
                ? model->fftEngine()->decimation() : 1);
        }
        if (transmit && source && txDisplayNegotiated) {
            const DbmWindow txWindow = transmitDbmWindow(source);
            request.insert(QStringLiteral("txMinDbm"), txWindow.minDbm);
            request.insert(QStringLiteral("txMaxDbm"), txWindow.maxDbm);
        }
        return request;
    }

    /// Parity Task 17: brings each pan's dBm window up to date. A pan seen
    /// for the first time takes its window at once; a changed window is
    /// taken once it has held for a frame period of the pan's rate (the
    /// planner's tick, kPlannerIntervalMs, is longer, so a range change
    /// asks the Core again at most once a tick and a drag once it stops).
    void settleDbmWindows(qint64 now)
    {
        if (!stack || !model) { return; }
        QSet<const SpectrumWidget*> seen;
        for (PanadapterApplet* applet : stack->allApplets()) {
            SpectrumWidget* widget = applet ? applet->spectrumWidget() : nullptr;
            SliceModel* slice = widget ? model->sliceById(applet->activeSliceIndex()) : nullptr;
            if (!slice) { continue; }
            widget->setCoreWaterfallLevelsAvailable(coreWaterfallLevelsAvailable());
            const int size = plannedFftSize(widget, slice);
            if (size <= 0) { continue; }
            seen.insert(widget);
            auto found = dbmWindows.find(widget);
            const double binWidthHz = double(slice->sampleRateHz()) / size;
            const DbmWindow levels = waterfallLevelsWindow(
                widget,
                found == dbmWindows.end() ? std::nullopt
                                          : std::optional<DbmWindow>(found->levels),
                panLowDbm(widget, binWidthHz));
            const DbmWindow live = liveDbmWindow(widget, binWidthHz, levels);
            if (found == dbmWindows.end()) {
                dbmWindows.insert(widget, {live, live, now, levels});
                continue;
            }
            DbmWindowState& state = *found;
            state.levels = levels;
            if (live != state.live) {
                state.live = live;
                state.liveSinceMs = now;
                continue;
            }
            int fps = requestedFps();
            for (const auto& [id, binding] : bindings) {
                Q_UNUSED(id);
                if (binding.widget == widget && binding.accepted
                    && binding.context.targetFps > 0) {
                    fps = binding.context.targetFps;
                    break;
                }
            }
            if (state.settled != live && now - state.liveSinceMs >= 1000 / std::max(1, fps)) {
                state.settled = live;
            }
        }
        for (auto it = dbmWindows.begin(); it != dbmWindows.end();) {
            it = seen.contains(it.key()) ? std::next(it) : dbmWindows.erase(it);
        }
    }
    struct CtunState {
        quint64 epoch = 0;
        int requestSliceId = -1;
        bool requestedPin = false;
        bool pending = false;
        bool initialized = false;
        quint32 rejectedContext = 0;
        // Slice control plan Task 14a: the Core refused the pin. A fresh
        // picture alone does not ask again, or a window the Core keeps
        // refusing (a listener) would ask on every tune. A C-Tune gesture,
        // a new stream lifetime, or a change in who controls the slice
        // clears it.
        bool refused = false;
    };
    QHash<int, CtunState> ctunStreams;
    // R-R3-18/21: one C-Tune centre request in flight per stream. A gesture
    // runs at mouse or wheel rate; the Core answers each request, and while
    // one is out the newest wanted centre waits here. A refusal drops it, so
    // a refused gesture cannot keep asking.
    struct CentreRequest {
        quint64 epoch = 0;
        bool inFlight = false;
        bool hasQueued = false;
        int queuedSliceId = -1;
        double queuedHz = 0.0;
    };
    QHash<int, CentreRequest> centreRequests;
    QString connectionId;
    quint32 epoch = 0;
    quint32 nextEndpoint = 1;
    quint64 frames = 0;
    // Display drops already written to the log, and when (allocationClock).
    quint64 displayDropsReported = 0;
    qint64 displayDropsReportedAtMs = 0;
    // R-R3-21 / R-R3-08, per media session: the display's presentation map
    // (Core clock to this computer's, following the audio's delay), the
    // audio delay last measured, the presentation timer and the counters.
    DisplayDelayFollower displayDelay;
    std::optional<qint64> lastAudioDelayNs;
    QTimer* presentTimer = nullptr;
    RemoteDisplayTelemetry displayCounters;
    Ps3DisplayAssembler ps3Assembler;
    quint64 ps3Generation = 0;
    std::unique_ptr<RemoteAudioReceiver> audio;
    // R-R3-43: the sinks one receiver stream hands audio to. Its worker
    // delivers under the mutex; release removes a sink under it, so a sink
    // is never called after releaseReceiverAudio() returns.
    struct ReceiverFanout {
        int sliceId = -1;
        std::mutex mutex;
        QList<IReceiverPcmSink*> sinks;
    };
    // R-R3-43: one wanted receiver stream, kept while any sink wants it
    // (across media connections). Everything but the sinks and the receiver
    // belongs to the current media connection and is reset with it.
    struct ReceiverStream {
        QList<IReceiverPcmSink*> sinks;
        std::shared_ptr<ReceiverFanout> fanout;
        std::shared_ptr<RemoteTciAudioStage> tciStage;
        std::unique_ptr<RemoteAudioReceiver> receiver;
        quint32 generation = 0;                        // newest accepted context
        std::optional<RemoteAudioContextMessage> context;
        quint32 ssrc = 0;                              // while the receiver runs
        std::optional<RemoteAudioProfile> runningProfile;
        bool stopped = false;                          // the sinks were told stopReason
        QString stopReason;
        // The Core forgot this request (slice-removed or receiver-limit):
        // asked again only when that can change, never in a loop.
        std::optional<RemoteAudioOffReason> heldBy;
        bool faulted = false; // this computer stopped it; asked again only by a new choice
        bool retryPending = false;
        qint64 lastRequestMs = -1000;
        RemoteAudioRestartBackoff restartBackoff; // R-R3-21
    };
    std::map<int, ReceiverStream> receiverStreams;
    // R-R3-43: the last receiver-audio revision sent for each slice id on
    // this media connection. The Core remembers them for the whole
    // connection, so a slice id's revision only ever grows, even when a new
    // slice reuses the id; a new connection starts afresh.
    QHash<int, quint32> receiverRevisions;
    struct IqStream {
        bool wanted = false;
        bool enabled = false;
        quint32 revision = 0;
        quint32 generation = 0;
        quint32 nextSequence = 0;
        int sampleRate = 0;
    };
    std::map<int, IqStream> iqStreams;
    bool iqNegotiated = false;
    quint64 iqBytesPerSecond = 0;
    // R-R3-43: the receiver stream ids this media connection declared.
    QList<quint32> receiverSsrcs;
    // R-R3-45: the headphones mix, played on this computer's headphones
    // with its own rate matching. Everything but the receiver belongs to
    // the current media connection and is reset with it.
    std::unique_ptr<RemoteAudioReceiver> headphones;
    quint32 headphonesSsrc = 0;               // declared by this connection
    std::optional<RemoteAudioContextMessage> headphonesContext;
    // Parity Task 32: the transmit monitor's route, this window's choice
    // (kept across connections), and per media connection whether the start
    // declared it, the last request's revision and the Core's answer.
    TxMonitorRoute txMonitorRoute = TxMonitorRoute::Speakers;
    bool txMonitorNegotiated = false;
    quint32 monitorRevision = 0;
    std::optional<MonitorAudioMessage> monitorContext;
    quint32 headphonesRevision = 0;           // last request sent
    quint32 headphonesGeneration = 0;         // newest accepted context
    bool headphonesRequested = false;         // last request asked for it
    // This computer's headphones failed: not asked for again until the
    // device changes or the operator chooses the audio quality again.
    bool headphonesFaulted = false;
    bool headphonesRetryPending = false;
    qint64 headphonesLastRequestMs = -1000;
    RemoteAudioRestartBackoff headphonesRestartBackoff; // R-R3-21
    QString headphonesProblem;
    bool destroying = false;
    std::optional<RemoteAudioContextMessage> acceptedAudioContext;
    quint32 audioRevision = 0;
    quint32 audioGeneration = 0;
    bool preparingAudio = false;
    bool audioEnabled = false;
    bool audioRetryPending = false;
    // Direct media: the pending restart's fence. Bumped wherever the
    // pending flag is cleared outside the restart itself, so a stale
    // restart does nothing; a media move leaves it alone, so a restart
    // waiting across the move still runs on the new connection.
    quint64 audioRetryGeneration = 0;
    // Test only: while held, a restart's backoff step that comes due is
    // kept here instead of running, and runs (through its own fence) once
    // released, so a test can order events against the step without
    // racing a real timer.
    bool audioRestartHeldForTest = false;
    std::function<void()> heldAudioRestartStep;
    // R-R3-21: repeated restarts wait 1, 2, 4 s, reset by a healthy 10 s.
    RemoteAudioRestartBackoff audioRestartBackoff;
    // R-R3-23. The operator's choice, stored on this computer. Whether this
    // media session has sent Core a `profile` (its contexts then carry the
    // profile shape). Whether this media session's link trial failed, so
    // Opus is asked for until the session ends or the operator chooses
    // again. The trial itself and its 1 s sampling timer.
    RemoteAudioProfile audioProfileChoice = RemoteAudioProfile::Opus;
    RemoteAudioQualityChoice audioQualityChoice = RemoteAudioQualityChoice::High;
    bool audioProfileRequested = false;
    bool losslessFallback = false;
    RemoteAudioLinkTrial linkTrial;
    QTimer* linkTrialTimer = nullptr;
    // R-R3-35 measured delay, per media session: the clock offset from
    // probe echoes, the Core's newest capture from the latest echo, the
    // probes awaiting an echo (id, t0) and the 1 s probe timer.
    AudioClockEstimator clockEstimator;
    std::optional<AudioCaptureAnchor> captureAnchor;
    std::deque<std::pair<quint32, qint64>> pendingClockProbes;
    quint32 nextClockProbeId = 0;
    QTimer* clockProbeTimer = nullptr;
    // A persistent local playback failure and the identity it was recorded
    // against; only matching recovery with real speaker progress clears it.
    std::optional<RemoteAudioFailure> audioFailure;
    // An interruption whose automatic retry is scheduled, until the next
    // accepted context or stop().
    bool audioRestarting = false;
    QString selectedOutput;
    RemoteAudioStatus audioStatus;
    QTimer* audioStatusTimer = nullptr;
    bool recoveryRequested = false;
    // R-R3-28: bounds a started media session that never becomes ready, in
    // two stages: Core's description, then the connection.
    QTimer* establishTimer = nullptr;
    int descriptionDeadlineMs = RemoteMediaController::kMediaDescriptionDeadlineMs;
    int connectDeadlineMs = RemoteMediaController::kMediaConnectDeadlineMs;
    bool awaitingDescription = false;
    // While MediaPeer::start runs, a backend error is its refusal reason.
    bool startingPeer = false;
    QString startRefusal;
    qint64 lastAudioRequestMs = -1000;
    bool desiredPs3 = false;
    bool accountedPs3 = false;
    bool ps3Refused = false;
    quint32 ps3RefusedGeneration = 0;
    QString ps3RefusalReason;
    struct PendingPs3 {
        quint32 commandId = 0;
        bool enabled = false;
        qint64 sentAtMs = 0;
        bool timedOut = false;
        bool commandAccepted = false;
    };
    std::optional<PendingPs3> pendingPs3;
    bool refreshingBudget = false;
    bool budgetReplanRequested = false;
    QString allocationCacheIdentity;
    std::optional<RemoteDisplayAllocation> cachedAllocation;
    QString cachedAllocationError;
    // iPhone app plan Task 36: the microphone uplink. The lease holds the
    // capture helper open only while the uplink runs; the rest belongs to
    // the current media connection.
    bool holdsTransmit = false;
    bool micKeyDown = false;
    bool voxArmed = false;
    bool micRunning = false;
    bool micLineWasOpen = false;   // fix wave 2 (M8): micLineChanged
    QTimer* micTimer = nullptr;
    CaptureSupervisor::Lease micLease;
    std::unique_ptr<RemoteMicEncoder> micEncoder;
    std::vector<float> micPending;
    std::vector<float> micScratch;
    quint16 micSequence = 0;
    quint32 micTimestamp = 0;
    quint64 micPacketsSent = 0;
    std::vector<float> programPending;
    qint64 programUntilMs = -1;
    std::unique_ptr<Resampler> programResampler;
    int programRateHz = 0;
    /// Fix wave 2 (Critical 1, the several-devices design, ruling 9.3):
    /// the Core splits the display budget by what each device asks for, so
    /// the planner asks for the displays the operator wants, not only what
    /// the share allows. While askingWanted, every pan is subscribed at its
    /// wanted quality; a refusal for the budget (answered by the smaller
    /// share the Core publishes with it) ends the ask, and the planner plans
    /// inside the share again. It asks again whenever what the operator
    /// wants grows (a new pan, a wider or faster one; a resize once it
    /// settles, fix wave 3) and when the transmit holder changes
    /// (setTransmitHolder): wantedCharge is the charge of the displays
    /// wanted at the last plan.
    bool askingWanted = false;
    DisplayBudgetCharge wantedCharge;
    /// Fix wave 3 (Minor 3): the displays wanted at the last plan, and a
    /// resize's growth waiting to settle before it asks: the wanted charge
    /// before the resize began, and when a width last moved.
    QList<RemoteDisplayIntent> wantedIntents;
    struct ResizeAsk {
        DisplayBudgetCharge base;
        qint64 movedAtMs = 0;
    };
    std::optional<ResizeAsk> resizeAsk;
    /// Fix wave 3: the transmit holder last notified (setTransmitHolder);
    /// a change asks again. Kept across media sessions: it is the station's.
    quint64 holderEpoch = 0;
    bool holderAway = false;
};

RemoteMediaController::RemoteMediaController(StationClient* client, RadioModel* model,
    PanadapterStack* stack, QObject* parent, MediaPeer::TransportFactory factory,
    AllocationClock allocationClock, int allocationAckTimeoutMs, int descriptionDeadlineMs,
    int connectDeadlineMs)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->client = client;
    d->model = model;
    d->stack = stack;
    d->factory = std::move(factory);
    d->clock.start();
    d->allocationClock = std::move(allocationClock);
    if (!d->allocationClock) {
        d->allocationClock = [this] { return d->clock.elapsed(); };
    }
    d->allocationAckTimeoutMs = allocationAckTimeoutMs > 0
        ? allocationAckTimeoutMs : kDefaultAllocationAckTimeoutMs;
    d->descriptionDeadlineMs = descriptionDeadlineMs > 0
        ? descriptionDeadlineMs : kMediaDescriptionDeadlineMs;
    d->connectDeadlineMs = connectDeadlineMs > 0
        ? connectDeadlineMs : kMediaConnectDeadlineMs;
    // iPhone app plan Task 29: a replacement that never becomes ready is
    // dropped; the old connection's packets still in flight are taken for
    // a while after the new one took over.
    d->replaceDeadline = new QTimer(this);
    d->replaceDeadline->setSingleShot(true);
    connect(d->replaceDeadline, &QTimer::timeout, this, [this] {
        dropReplacement(QStringLiteral("the new audio and display connection did not open"));
    });
    // Task 29 step 2b: the media stall rule's check.
    d->stallTimer = new QTimer(this);
    d->stallTimer->setInterval(500);
    connect(d->stallTimer, &QTimer::timeout, this, [this] {
        // Not isReady(): an agent that stopped hearing its pair may say it
        // is not ready long before it fails, which is the case to catch.
        // Nor audioEnabled: the audio receiver gives up on a starved
        // stream within a second and waits to be asked again, which a dead
        // connection never does. What counts is that audio came on this
        // connection and this window still wants it (not muted).
        const bool wanted = d->model && !d->model->audioEngine()->masterMuted();
        if (!d->client || !d->peer || !wanted || !d->lastAudio.isValid()) {
            return;
        }
        // The tunnel counts only once ICE settled on it (a host pair on an
        // open network is not a slow path); the control heartbeat follows.
        const std::optional<MediaIcePath> path = d->peer->selectedPath();
        const bool viaTunnel = d->tunnelNegotiated && path && path->viaLoopbackShim();
        d->client->setMediaTunnelInUse(viaTunnel);
        const int rank = d->client->pathRank();
        const bool slowPath = rank == PathRacer::ServiceRelayed || rank == PathRacer::Floor
            || viaTunnel;
        // The Core stops RX audio for the TX-bound slice while keyed. Only
        // its negotiated txState can establish that this silence is expected.
        // Keep the media connection and display alive; the independent TX
        // keepalive watchdog continues to enforce its own deadline. Path
        // tracking above must continue even during TX.
        const QPointer<RemoteMediaController> self(this);
        updateDirectUpgrade(viaTunnel);
        if (!self || !d->peer) { return; }
        if (d->coreTransmitting()) { return; }
        if (slowPath && d->lastAudio.elapsed() > kMediaStallMs) {
            qCInfo(lcRemoteMedia) << "No audio from the Core for" << d->lastAudio.elapsed()
                                  << "ms on a relayed path; starting audio and display again";
            d->lastAudio.invalidate();
            requestRecovery(d->epoch, QStringLiteral("Audio from the Core stopped. Starting "
                                                     "audio and display again."));
            return;
        }
        // The direct media ladder: a direct path that went silent goes back
        // to the connection that includes the tunnel.
        checkMediaSilence();
    });
    // The direct media ladder: the next direct-only replace.
    d->directUpgrade = new QTimer(this);
    d->directUpgrade->setSingleShot(true);
    connect(d->directUpgrade, &QTimer::timeout, this,
            &RemoteMediaController::runDirectUpgradeStep);
    // Task 29 fix wave (Important 1): a pending replacement is tried until
    // it can start.
    d->replaceRetry = new QTimer(this);
    d->replaceRetry->setInterval(kReplaceRetryMs);
    connect(d->replaceRetry, &QTimer::timeout, this, &RemoteMediaController::tryPendingReplace);
    d->retireTimer = new QTimer(this);
    d->retireTimer->setSingleShot(true);
    connect(d->retireTimer, &QTimer::timeout, this, &RemoteMediaController::retireOldPeer);
    // The two connections' audio merged across a replacement
    // (DualPathAudio), handed on as each packet falls due.
    d->dual = std::make_unique<DualPathAudio>([this](const QByteArray& packet) {
        deliverRtp(packet);
    });
    d->dualClock.start();
    d->dualTimer = new QTimer(this);
    d->dualTimer->setInterval(5);
    d->dualTimer->setTimerType(Qt::PreciseTimer);
    connect(d->dualTimer, &QTimer::timeout, this, [this] {
        d->dual->tick(d->dualClock.elapsed());
        // The retired peer can still deliver delayed RTP throughout its drain.
        // Keep the duplicate window until that peer is disconnected.
        if (!d->dual->active() && !d->retiring) {
            d->duplicatesDropped += d->dual->duplicatesDropped();
            d->dualTimer->stop();
        }
    });
    d->establishTimer = new QTimer(this);
    d->establishTimer->setObjectName(QStringLiteral("remoteMediaEstablishTimer"));
    d->establishTimer->setSingleShot(true);
    // Precise: a coarse timer may fire up to 5% early, which at stage two
    // would come before the library's own slowest report.
    d->establishTimer->setTimerType(Qt::PreciseTimer);
    connect(d->establishTimer, &QTimer::timeout, this, [this] {
        // R-R3-28: negotiation that never reaches a terminal state. Only
        // the session the timer was armed for, and only while not ready;
        // stop() disarms it for every other outcome.
        if (!d->peer || d->peer->isReady() || !d->client || !d->client->mediaAvailable()
            || d->client->sessionEpoch() != d->epoch) {
            return;
        }
        requestRecovery(d->epoch, d->awaitingDescription
            ? QStringLiteral("Core sent no station media description within %1 seconds")
                  .arg(QString::number(d->descriptionDeadlineMs / 1000.0))
            : QStringLiteral("Station media did not connect within %1 seconds")
                  .arg(QString::number(connectStageMs(d->connectDeadlineMs,
                                                      d->peer->gathersFromServers())
                                       / 1000.0)));
    });
    d->audio = std::make_unique<RemoteAudioReceiver>(model->audioEngine());
    d->selectedOutput = selectedSpeakerOutput();
    d->audioQualityChoice = storedAudioQualityChoice();
    d->audioProfileChoice = d->audioQualityChoice == RemoteAudioQualityChoice::Lossless
        ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
    d->linkTrialTimer = new QTimer(this);
    d->linkTrialTimer->setObjectName(QStringLiteral("remoteAudioLinkTrialTimer"));
    d->linkTrialTimer->setInterval(kLinkTrialSampleMs);
    connect(d->linkTrialTimer, &QTimer::timeout,
            this, &RemoteMediaController::checkLosslessLink);
    d->clockProbeTimer = new QTimer(this);
    d->clockProbeTimer->setObjectName(QStringLiteral("remoteAudioClockProbeTimer"));
    d->clockProbeTimer->setInterval(kClockProbeIntervalMs);
    connect(d->clockProbeTimer, &QTimer::timeout, this, &RemoteMediaController::sendClockProbe);
    d->presentTimer = new QTimer(this);
    d->presentTimer->setObjectName(QStringLiteral("remoteDisplayPresentTimer"));
    d->presentTimer->setSingleShot(true);
    d->presentTimer->setTimerType(Qt::PreciseTimer);
    connect(d->presentTimer, &QTimer::timeout, this, &RemoteMediaController::presentDueDisplay);
    d->audioStatusTimer = new QTimer(this);
    d->audioStatusTimer->setObjectName(QStringLiteral("remoteAudioStatusTimer"));
    d->audioStatusTimer->setInterval(kAudioStatusRefreshMs);
    connect(d->audioStatusTimer, &QTimer::timeout,
            this, &RemoteMediaController::refreshAudioStatus);
    connect(d->audio.get(), &RemoteAudioReceiver::errorOccurred, this,
            [this](const QString& reason, RemoteAudioReceiver::Fault fault) {
        // Record the failure against this session and context as they stand
        // now, before the disable below advances the audio revision.
        d->audioFailure = RemoteAudioFailure{fault, d->epoch, d->connectionId,
                                             d->audioGeneration,
                                             d->audio->telemetry().generation};
        // The receiver's detail is for diagnosis; the operator sees the
        // plain-English problem below.
        qCWarning(lcRemoteMedia).noquote()
            << QStringLiteral("Remote audio playback failed: %1").arg(reason);
        d->audioEnabled = false;
        d->audio->stop();
        // A failure is lasting: a restart waiting on its backoff step ends
        // here (the disable below moves the revision on, so it could never
        // run), and the next request is the operator's Retry or a change.
        d->audioRetryPending = false;
        ++d->audioRetryGeneration;
        const QPointer<RemoteMediaController> self(this);
        if (d->peer && d->peer->isReady()) {
            ++d->audioRevision;
            if (!d->audioRevision) { ++d->audioRevision; }
            QJsonObject disable{{QStringLiteral("op"), QStringLiteral("audio")},
                                {QStringLiteral("revision"), double(d->audioRevision)},
                                {QStringLiteral("enabled"), false}};
            if (audioProfileNegotiated()) {
                disable.insert(QStringLiteral("profile"), remoteAudioProfileToWire(
                    d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback
                        ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus));
                d->audioProfileRequested = true;
            }
            send(disable);
            if (!self) { return; }
        }
        refreshAudioStatus();
        if (!self) { return; }
        emit errorOccurred(remoteAudioProblemText(fault));
    });
    connect(d->audio.get(), &RemoteAudioReceiver::restartRequested, this,
            [this](const QString& reason, RemoteAudioReceiver::Fault fault) {
        qCWarning(lcRemoteMedia) << reason;
        d->audio->stop();
        // R-R3-23: a lossless stream that arrives badly enough to restart
        // counts against the link trial; failing it asks Core for Opus now.
        if (d->linkTrial.active() && linkInterruption(fault)
            && d->linkTrial.noteInterruption(d->clock.elapsed())
                == RemoteAudioLinkTrial::Verdict::Failed) {
            fallBackToOpus(QStringLiteral("receiver restarts while playing lossless audio"));
            return;
        }
        d->audioRestarting = true;
        if (!d->audioRetryPending) {
            d->audioRetryPending = true;
            const quint64 generation = d->audioRetryGeneration;
            const quint32 revision = d->audioRevision;
            const int delay = int(d->audioRestartBackoff.nextDelayMs(d->clock.elapsed(),
                                                                     d->lastAudioRequestMs));
            const auto step = [this, generation, revision] {
                if (generation != d->audioRetryGeneration
                    || revision != d->audioRevision) { return; }
                d->audioRetryPending = false;
                requestAudio();
            };
            QTimer::singleShot(delay, Qt::PreciseTimer, this, [this, step] {
                if (d->audioRestartHeldForTest) {
                    d->heldAudioRestartStep = step;
                    return;
                }
                step();
            });
        }
        refreshAudioStatus();
    });
    connect(model->audioEngine(), &AudioEngine::masterMutedChanged,
            this, &RemoteMediaController::requestAudio);
    // R-R3-45: the headphones mix plays on this computer's headphones with
    // its own receiver and rate matcher. A failure there stops only it.
    d->headphones = std::make_unique<RemoteAudioReceiver>(model->audioEngine(),
                                                          RemotePlaybackOutput::Headphones);
    connect(d->headphones.get(), &RemoteAudioReceiver::restartRequested,
            this, &RemoteMediaController::onHeadphonesRestart);
    connect(d->headphones.get(), &RemoteAudioReceiver::errorOccurred,
            this, &RemoteMediaController::onHeadphonesError);
    // Headphones opened, closed or moved to another device: a device that
    // failed gets a fresh chance, and the Core is asked accordingly.
    const auto headphonesDeviceChanged = [this] {
        // Opened, closed or moved: whatever played stops now (so a device
        // closed on purpose is not reported as a failure), a device that
        // failed gets a fresh chance, and the Core is asked again so the
        // mix starts on the device as it is now.
        d->headphones->stop();
        d->headphonesFaulted = false;
        d->headphonesRestartBackoff.reset(); // R-R3-21: a new device starts over
        if (d->headphonesProblem != QLatin1String(kHeadphonesMixUnavailableReason)
            && d->headphonesProblem != QLatin1String(kHeadphonesCoreCouldNotStart)) {
            setHeadphonesProblem(QString());
        }
        requestHeadphonesAudio();
    };
    connect(model->audioEngine(), &AudioEngine::headphonesAvailableChanged,
            this, headphonesDeviceChanged);
    connect(model->audioEngine(), &AudioEngine::headphonesConfigChanged,
            this, headphonesDeviceChanged);
    // R-R3-43: a slice id the Core had removed may come back (the Core
    // reuses ids); a consumer still waiting on it is asked for once more.
    connect(model, &RadioModel::sliceAdded, this, [this] {
        if (!d->model || !d->peer || !d->peer->isReady() || !receiverAudioNegotiated()) {
            return;
        }
        QList<int> back;
        for (const auto& [sliceId, stream] : d->receiverStreams) {
            if (stream.heldBy == RemoteAudioOffReason::SliceRemoved
                && d->model->sliceById(sliceId)) {
                back.append(sliceId);
            }
        }
        const QPointer<RemoteMediaController> self(this);
        for (int sliceId : back) {
            sendReceiverAudioRequest(sliceId, true);
            if (!self) { return; }
        }
        if (!back.isEmpty()) { refreshAudioStatus(); }
    });
    connect(model->audioEngine(), &AudioEngine::speakersConfigChanged, this, [this] {
        d->selectedOutput = selectedSpeakerOutput();
        // Opening the speaker for playback can report its configuration from
        // inside the receiver's start(); the accepted context that started it
        // refreshes the status once start() returns.
        if (!d->preparingAudio) { requestAudio(); }
    });
    // Task 36: the microphone uplink's pump. It runs only while this media
    // connection has a microphone line; it pulls audio only while the
    // uplink runs.
    d->micTimer = new QTimer(this);
    d->micTimer->setInterval(kMicPumpIntervalMs);
    connect(d->micTimer, &QTimer::timeout, this, &RemoteMediaController::reconcileMicUplink);
    d->micEncoder = std::make_unique<RemoteMicEncoder>();
    resetMicrophoneQuality();
    d->micScratch.resize(static_cast<size_t>(RemoteMicConfig::kOpusFrameSamples));
    // Desktop remote transmit (R-IOS-13): the uplink's production callers.
    // The window's transmit client says when its key is down and when its
    // key is on at the Core; the Core's VOX, mirrored to the window
    // (transmit.voxEnabled), says when VOX is armed.
    if (RemoteTransmitClient* transmit = client->remoteTransmit()) {
        connect(transmit, &RemoteTransmitClient::micKeyDownChanged, this,
                &RemoteMediaController::setMicKeyDown);
        connect(transmit, &RemoteTransmitClient::holdsTransmitChanged, this,
                &RemoteMediaController::setHoldsTransmit);
        connect(transmit, &RemoteTransmitClient::micSourceChanged, this,
                [this](RemoteMicSource accepted, bool, const QString&) {
            if (accepted == RemoteMicSource::RadioMic) {
                d->micPending.clear();
                d->programPending.clear();
                d->programUntilMs = -1;
            }
            reconcileMicUplink();
        });
        d->micKeyDown = transmit->micKeyDown();
        d->holdsTransmit = transmit->holdsTransmit();
        // Task 37: the keepalive rides this connection's "tx" data channel
        // (unordered, never retransmitted) while it is open; otherwise the
        // transmit client sends it on the session.
        const QPointer<RemoteMediaController> self(this);
        transmit->setChannelKeepalive([self](quint64 sequence, quint32 epoch) {
            return self && self->d->peer && self->d->peer->isReady()
                && self->d->peer->sendTx(RemoteTxWatchdog::channelKeepalive(sequence, epoch));
        });
    }
    // Fix wave M6: the microphone streams unkeyed only for VOX this window
    // armed, never for VOX another device armed on the Core.
    connect(client, &StationClient::voxArmedHereChanged, this,
            &RemoteMediaController::setVoxArmed);
    d->voxArmed = client->voxArmedHere();
    d->timer = new QTimer(this);
    d->timer->setInterval(kPlannerIntervalMs);
    connect(d->timer, &QTimer::timeout, this, &RemoteMediaController::refreshSubscriptions);
    // The merge of the trunk into the transmit lane (fix wave I4): the
    // display budget's holder is the Core's, from `txState`'s holderEpoch
    // and holderAway (0 and not away while unheld or not sent).
    if (TransmitState* txState = client->transmitState()) {
        connect(txState, &TransmitState::stateChanged, this, [this, client] {
            if (d->client != client) { return; }
            const bool onAir = d->coreTransmitting();
            if (onAir == d->txSilenceSuppressed) { return; }
            const bool returningToRx = d->txSilenceSuppressed && !onAir;
            d->txSilenceSuppressed = onAir;
            // Only the actual TX-to-RX transition restarts a clock already
            // armed by RTP. Periodic txState fields cannot defer recovery.
            if (returningToRx && d->lastAudio.isValid()) {
                d->lastAudio.restart();
            }
            // The direct media ladder: the silence while keyed is expected,
            // so the silence fallback's clock (and a finished fallback's
            // wait) restarts on the return to receive too.
            if (returningToRx) {
                const qint64 now = d->allocationClock();
                if (d->lastMediaMs >= 0) { d->lastMediaMs = now; }
                if (d->fallbackFinishedMs >= 0) { d->fallbackFinishedMs = now; }
            }
        });
        connect(txState, &TransmitState::holderChanged, this, [this, txState] {
            setTransmitHolder(static_cast<quint64>(std::max<qint64>(0, txState->holderEpoch())),
                              txState->holderAway());
        });
    }
    connect(client, &StationClient::displayBudgetChanged, this, [this] {
        if (!d->client) { return; }
        if (const auto limits = d->client->remoteDisplayBudgetLimits();
            limits && d->ps3RefusedGeneration != 0
            && limits->generation != d->ps3RefusedGeneration) {
            d->ps3Refused = false;
            d->ps3RefusalReason.clear();
            d->ps3RefusedGeneration = 0;
        }
        const bool advertised = d->client->remotePs3DisplaySubscribed();
        if (advertised) {
            d->accountedPs3 = true;
        } else if (!d->pendingPs3
                   || (d->pendingPs3->commandAccepted
                       && !d->pendingPs3->enabled)) {
            d->accountedPs3 = false;
        }
        if (d->pendingPs3 && d->pendingPs3->commandAccepted
            && advertised == d->pendingPs3->enabled) {
            d->accountedPs3 = advertised;
            d->pendingPs3.reset();
        }
        refreshSubscriptions();
    });
    connect(client, &StationClient::ps3DisplaySubscriptionRequested,
            this, [this](bool enabled) {
        if (!d->client || !d->client->remoteDisplayBudgetLimits()) { return; }
        d->desiredPs3 = enabled;
        d->ps3Refused = false;
        d->ps3RefusalReason.clear();
        d->ps3RefusedGeneration = 0;
        refreshSubscriptions();
    });
    connect(client, &StationClient::ps3DisplaySubscriptionStarted,
            this, [this](quint32 commandId, bool enabled) {
        if (!d->client || !d->client->remoteDisplayBudgetLimits()) { return; }
        d->pendingPs3 = Private::PendingPs3{
            commandId, enabled, d->allocationClock(), false, false};
        if (enabled) {
            // The enable is now in flight. Account its maximum reservation
            // until the command or capabilities prove it was refused.
            d->accountedPs3 = true;
        }
    });
    connect(client, &StationClient::ps3DisplaySubscriptionFinished,
            this, [this](quint32 commandId, bool enabled,
                         bool accepted, const QString& reason) {
        if (!d->pendingPs3 || d->pendingPs3->commandId != commandId
            || d->pendingPs3->enabled != enabled || !d->client) { return; }
        if (!accepted) {
            d->accountedPs3 = d->client->remotePs3DisplaySubscribed();
            d->ps3Refused = enabled;
            d->ps3RefusalReason = reason.left(512);
            qCInfo(lcRemoteMedia).noquote() << "Remote PureSignal display refused:"
                                            << d->ps3RefusalReason;
            d->ps3RefusedGeneration = d->client->remoteDisplayBudgetLimits()
                ? d->client->remoteDisplayBudgetLimits()->generation : 0;
            d->pendingPs3.reset();
        } else {
            d->pendingPs3->commandAccepted = true;
            const bool advertised = d->client->remotePs3DisplaySubscribed();
            if (advertised == enabled) {
                d->accountedPs3 = advertised;
                d->pendingPs3.reset();
            }
        }
        refreshSubscriptions();
    });
    if (stack) {
        connect(stack, &PanadapterStack::panRetired,
                this, &RemoteMediaController::refreshSubscriptions);
        connect(stack, &PanadapterStack::activePanChanged,
                this, &RemoteMediaController::refreshSubscriptions);
        connect(stack, &QObject::destroyed, this, [this] {
            d->stack = nullptr;
            QList<quint32> endpoints;
            for (const auto& [id, binding] : d->bindings) { endpoints.append(id); }
            d->ctunStreams.clear();
            d->centreRequests.clear();
            retireSubscriptions(endpoints);
        });
    }
    connect(client, &StationClient::handshakeComplete, this, &RemoteMediaController::start);
    connect(client, &StationClient::audioOpusBitratesChanged, this, [this] {
        const QPointer<RemoteMediaController> self(this);
        requestAudio();
        if (!self) { return; }
        refreshAudioStatus();
    });
    connect(client, &StationClient::mediaSessionEnded, this, [this](quint32 epoch) {
        if (epoch == d->epoch) {
            stop();
        }
    });
    connect(client, &StationClient::mediaControlReceived,
            this, &RemoteMediaController::receiveControl);
    // iPhone app plan Task 29: the session moved to a better path; media
    // follows it.
    connect(client, &StationClient::pathChanged, this, &RemoteMediaController::markReplacePending);
    connect(client, &StationClient::streamCtunPinFinished, this,
        [this](int sliceId, quint64 epoch, bool pinned, bool accepted) {
            if (!d->model) { return; }
            SliceModel* slice = d->model->sliceById(sliceId);
            if (!slice || slice->streamEpoch() != epoch) { return; }
            auto state = d->ctunStreams.find(slice->streamIndex());
            if (state == d->ctunStreams.end() || state->epoch != epoch
                || state->requestSliceId != sliceId || state->requestedPin != pinned) { return; }
            state->pending = false;
            state->initialized = accepted;
            state->refused = !accepted;
            if (!accepted) {
                // A migration can refuse a request after it leaves the GUI.
                // Wait for fresh source context before retrying this lifetime.
                for (const auto& [id, binding] : d->bindings) {
                    if (binding.slice && binding.slice->streamIndex() == slice->streamIndex()
                        && isNewerGeneration(binding.context.codec.contextGeneration, state->rejectedContext)) {
                        state->rejectedContext = binding.context.codec.contextGeneration;
                    }
                }
            }
            refreshCtunState();
        });
    if (client && client->sliceAccess()) {
        // Slice control plan Task 14a: a change in who controls a slice may
        // turn a refused C-Tune pin into an accepted one, so the next fresh
        // picture may ask again.
        connect(client->sliceAccess(), &SliceAccessMirror::changed, this, [this](int) {
            for (auto& state : d->ctunStreams) { state.refused = false; }
            refreshCtunState();
        });
    }
    connect(client, &StationClient::streamCentreFinished,
            this, &RemoteMediaController::finishCentreRequest);
    connect(client, &QObject::destroyed, this, &RemoteMediaController::stop);
    connect(model, &RadioModel::connectionStateChanged, this, [this](ConnectionState state) {
        if (state != ConnectionState::Connected) {
            // The daemon retires FFT production when the radio disconnects,
            // even if this authenticated station session remains connected.
            // Retire our observations too so identical settings resubscribe.
            QList<quint32> endpoints;
            for (const auto& [id, binding] : d->bindings) { endpoints.append(id); }
            d->ctunStreams.clear();
            d->centreRequests.clear();
            if (!retireSubscriptions(endpoints)) { return; }
            requestAudio();
        } else {
            refreshSubscriptions();
            requestAudio();
        }
    });
    connect(model, &RadioModel::transmittingChanged, this,
            &RemoteMediaController::refreshSubscriptions);
    if (client && client->mediaAvailable()) {
        start();
    }
    refreshAudioStatus();
}

RemoteMediaController::~RemoteMediaController()
{
    // Nothing observes a status change while this controller is destroyed,
    // and no consumer is told anything: they may already be gone.
    const QSignalBlocker blocker(this);
    d->destroying = true;
    // Task 37: the transmit client falls back to the session.
    if (d->client) {
        if (RemoteTransmitClient* transmit = d->client->remoteTransmit()) {
            transmit->setChannelKeepalive({});
        }
    }
    stop();
    for (auto& [sliceId, stream] : d->receiverStreams) {
        {
            std::lock_guard<std::mutex> lock(stream.fanout->mutex);
            stream.fanout->sinks.clear();
        }
        stream.tciStage->invalidate();
        stream.receiver->stop();
    }
    d->receiverStreams.clear();
}
quint64 RemoteMediaController::receivedDisplayFrames() const { return d->frames; }
DisplayBudgetReason RemoteMediaController::panDisplayBudgetReason(const QString& panId) const
{
    return d->panBudgetReason.value(panId, DisplayBudgetReason::None);
}
int RemoteMediaController::activeEndpointCount() const { return int(d->bindings.size()); }

void RemoteMediaController::setMiniDisplaySlices(const QSet<int>& sliceIds)
{
    if (d->miniWanted == sliceIds) { return; }
    for (int id : std::as_const(d->miniWanted)) {
        if (!sliceIds.contains(id)) { emit miniDisplayUnavailable(id); }
    }
    d->miniWanted = sliceIds;
    refreshSubscriptions();
}
std::optional<MediaPeerTelemetry> RemoteMediaController::trafficTelemetry() const
{
    return d->peer ? d->peer->telemetry() : std::nullopt;
}
quint64 RemoteMediaController::displayMessagesDropped() const
{
    const std::optional<MediaPeerTelemetry> traffic = trafficTelemetry();
    return traffic ? traffic->traffic.displayMessagesDropped : 0;
}

void RemoteMediaController::reportDisplayDrops()
{
    // The drop rule itself lives in the transport (32 messages or 256 KiB,
    // oldest first); this only reports it, at most every 10 seconds.
    constexpr qint64 kDisplayDropReportIntervalMs = 10'000;
    const quint64 dropped = displayMessagesDropped();
    if (dropped <= d->displayDropsReported) { return; }
    const qint64 now = d->allocationClock();
    if (d->displayDropsReported != 0
        && now - d->displayDropsReportedAtMs < kDisplayDropReportIntervalMs) { return; }
    qCInfo(lcRemoteMedia).noquote()
        << QStringLiteral("Remote display: skipped %1 late updates on this computer "
                          "to keep the picture current (%2 this session)")
               .arg(dropped - d->displayDropsReported)
               .arg(dropped);
    d->displayDropsReported = dropped;
    d->displayDropsReportedAtMs = now;
}
RemoteAudioReceiverTelemetry RemoteMediaController::audioTelemetry() const
{
    return d->audio->telemetry();
}
std::optional<RemoteAudioContextMessage> RemoteMediaController::acceptedAudioContext() const
{
    return d->acceptedAudioContext;
}
bool RemoteMediaController::audioDetailNegotiated() const
{
    return d->client && d->client->remoteAudioStatusAvailable();
}
void RemoteMediaController::setPanTransmitting(const QString& panId, bool transmitting,
                                               bool displayMissing)
{
    if (panId.isEmpty()) { return; }
    const bool wasTransmitting = d->transmittingPans.contains(panId);
    if (transmitting) {
        d->transmittingPans.insert(panId);
    } else {
        d->transmittingPans.remove(panId);
    }
    if (transmitting != wasTransmitting) {
        // Merge of parity Task 29 with R-R3-21: receive frames queued for
        // the audio's clock before the key would otherwise be drawn for
        // about one audio delay into the over, so the key forgets them;
        // and the unkey starts a new blend chain, so the first receive row
        // after an over never blends from the row before the key.
        for (auto& [id, binding] : d->bindings) {
            Q_UNUSED(id);
            if (binding.panId != panId) { continue; }
            if (transmitting) {
                binding.presenter.reset();
            } else {
                binding.presenter.restartChain();
            }
        }
        scheduleDisplayPresentation(d->displayDelay.mapNs());
    }
    const bool missing = transmitting && displayMissing;
    const bool wasMissing = d->transmitDisplayMissingPans.contains(panId);
    if (missing) {
        d->transmitDisplayMissingPans.insert(panId);
    } else {
        d->transmitDisplayMissingPans.remove(panId);
    }
    if (missing != wasMissing) {
        refreshPanGrantStatus(panId);
    }
}

bool RemoteMediaController::isPanTransmitting(const QString& panId) const
{
    return d->transmittingPans.contains(panId);
}

void RemoteMediaController::refreshTransmitView()
{
    refreshSubscriptions();
}

void RemoteMediaController::setDisplayDuplex(bool on)
{
    if (d->displayDuplex == on) {
        return;
    }
    d->displayDuplex = on;
    if (d->displayDuplexNegotiated) {
        refreshSubscriptions();
    }
}

bool RemoteMediaController::displayDuplex() const
{
    return d->displayDuplex;
}

bool RemoteMediaController::displayDuplexNegotiated() const
{
    return d->displayDuplexNegotiated;
}

std::optional<SpectrumContextMessage> RemoteMediaController::heldTransmitContext(
    const QString& panId) const
{
    for (const auto& [id, binding] : d->bindings) {
        Q_UNUSED(id);
        if (binding.panId == panId && binding.transmit && binding.transmitContext) {
            return binding.transmitContext;
        }
    }
    return std::nullopt;
}

// ── Parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor ───────────────

bool RemoteMediaController::txMonitorAudioNegotiated() const
{
    return d->txMonitorNegotiated;
}

void RemoteMediaController::setTxMonitorRoute(TxMonitorRoute route)
{
    if (d->txMonitorRoute == route) {
        return;
    }
    d->txMonitorRoute = route;
    requestMonitorAudio();
}

TxMonitorRoute RemoteMediaController::txMonitorRoute() const
{
    return d->txMonitorRoute;
}

std::optional<MonitorAudioMessage> RemoteMediaController::acceptedMonitorContext() const
{
    return d->monitorContext;
}

void RemoteMediaController::requestMonitorAudio()
{
    if (!d->peer || !d->peer->isReady() || !d->txMonitorNegotiated) { return; }
    ++d->monitorRevision;
    if (!d->monitorRevision) { ++d->monitorRevision; }
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("monitor-audio")},
                     {QStringLiteral("revision"), double(d->monitorRevision)},
                     {QStringLiteral("route"), txMonitorRouteToWire(d->txMonitorRoute)}});
}

void RemoteMediaController::receiveMonitorAudioContext(const QJsonObject& payload)
{
    const std::optional<MonitorAudioMessage> context = decodeMonitorAudioContext(payload);
    if (!context || context->revision != d->monitorRevision
        || context->connectionId != d->connectionId) { return; }
    d->monitorContext = context;
}

bool RemoteMediaController::txDisplayNegotiated() const
{
    return d->txDisplayNegotiated;
}

bool RemoteMediaController::spectrumGrantNegotiated() const
{
    return d->client && d->client->spectrumGrantAvailable();
}
RemoteAudioStatus RemoteMediaController::audioStatus() const
{
    return d->audioStatus;
}
RemoteAudioProfile RemoteMediaController::audioProfileChoice() const
{
    return d->audioProfileChoice;
}
RemoteAudioQualityChoice RemoteMediaController::audioQualityChoice() const
{
    return d->audioQualityChoice;
}

bool RemoteMediaController::audioQualityNegotiated() const
{
    return audioProfileNegotiated() && d->client->agreedMinor() >= 11
        && d->client->capabilities().audioQualityVersion >= 1;
}

QString RemoteMediaController::audioQualityUnavailableReason(RemoteAudioQualityChoice choice) const
{
    if (choice == RemoteAudioQualityChoice::High || !d->client
        || !d->client->isHandshakeComplete()) { return {}; }
    if (choice == RemoteAudioQualityChoice::SaveData) {
        if (!audioQualityNegotiated()) {
            return QStringLiteral("This Core does not offer a choice of audio quality. Updating the Core may help.");
        }
        const auto& bitrates = d->client->audioOpusBitrates();
        if (!bitrates) { return QStringLiteral("Checking what this Core offers."); }
        if (!bitrates->contains(24000)) {
            return QStringLiteral("This Core does not offer this audio quality.");
        }
    } else {
        if (!audioProfileNegotiated()) {
            return remoteAudioQualityReasonText(RemoteAudioQualityReason::CoreCannotSend);
        }
        if (d->acceptedAudioContext && d->acceptedAudioContext->profileRefusal
            == RemoteAudioProfileRefusal::NotAllowed) {
            return remoteAudioQualityReasonText(RemoteAudioQualityReason::CoreNotAllowed);
        }
    }
    return {};
}

bool RemoteMediaController::audioProfileNegotiated() const
{
    return audioDetailNegotiated() && d->client->capabilities().audioProfileVersion >= 1;
}

bool RemoteMediaController::audioClockNegotiated() const
{
    return d->client && d->client->mediaAvailable()
        && d->client->capabilities().audioClockVersion >= 1;
}

bool RemoteMediaController::receiverAudioNegotiated() const
{
    return audioProfileNegotiated() && d->client->capabilities().receiverAudioVersion >= 1;
}

bool RemoteMediaController::remoteIqNegotiated() const
{
    return d->client && d->client->mediaAvailable()
        && d->client->capabilities().remoteIqVersion >= 1;
}

QHash<int, RemoteAudioReceiverTelemetry> RemoteMediaController::receiverAudioTelemetry() const
{
    QHash<int, RemoteAudioReceiverTelemetry> telemetry;
    for (const auto& [sliceId, stream] : d->receiverStreams) {
        telemetry.insert(sliceId, stream.receiver->telemetry());
    }
    return telemetry;
}

RemoteAudioDelayReport RemoteMediaController::audioDelay() const
{
    RemoteAudioDelayReport report;
    report.measurable = !d->peer.isNull() && audioClockNegotiated();
    if (!report.measurable || !d->audioEnabled) {
        return report;
    }
    const RemoteAudioReceiverTelemetry playback = d->audio->telemetry();
    if (!playback.running) {
        return report;
    }
    AudioDelayInputs inputs;
    inputs.offset = d->clockEstimator.offset(d->audio->nowNs());
    inputs.capture = d->captureAnchor;
    inputs.playingGeneration = d->audioGeneration;
    inputs.playout = playback.playout;
    inputs.release = playback.release;
    report.estimate = measureAudioDelay(inputs);
    return report;
}

void RemoteMediaController::reconcileClockProbe()
{
    // R-R3-35: probe once a second while this computer plays the Core's
    // audio, and only to a Core that answers. R-R3-21: also while a display
    // is shown from a Core on one clock, so the display keeps its map (the
    // last audio delay, or the base one) with the audio muted.
    const bool displaying = displayClockNegotiated()
        && std::any_of(d->bindings.cbegin(), d->bindings.cend(),
                       [](const auto& entry) { return entry.second.accepted; });
    const bool probe = d->peer && audioClockNegotiated()
        && ((d->audioEnabled && d->audio->isRunning()) || displaying);
    if (probe && !d->clockProbeTimer->isActive()) {
        d->clockProbeTimer->start();
    } else if (!probe && d->clockProbeTimer->isActive()) {
        d->clockProbeTimer->stop();
    }
}

void RemoteMediaController::sendClockProbe()
{
    if (!d->peer || !audioClockNegotiated()) {
        return;
    }
    const quint32 id = ++d->nextClockProbeId;
    const qint64 sentNs = d->audio->nowNs();
    d->pendingClockProbes.emplace_back(id, sentNs);
    while (d->pendingClockProbes.size() > kMaxPendingClockProbes) {
        d->pendingClockProbes.pop_front();
    }
    send({{QStringLiteral("op"), QStringLiteral("clock-probe")},
          {QStringLiteral("id"), qint64(id)},
          {QStringLiteral("t0"), sentNs}});
}

void RemoteMediaController::receiveClockEcho(const QJsonObject& payload, qint64 receivedNs)
{
    double idValue = 0;
    double generation = 0;
    double rtpTimestamp = 0;
    qint64 t0 = 0, t1 = 0, t2 = 0, capturedNs = 0;
    if (payload.size() != 9
        || !number(payload, "id", 0, std::numeric_limits<quint32>::max(), idValue, true)
        || !nanoseconds(payload, "t0", t0) || !nanoseconds(payload, "t1", t1)
        || !nanoseconds(payload, "t2", t2) || !nanoseconds(payload, "capturedNs", capturedNs)
        || !number(payload, "generation", 0, std::numeric_limits<quint32>::max(), generation, true)
        || !number(payload, "rtpTimestamp", 0, std::numeric_limits<quint32>::max(),
                   rtpTimestamp, true)) {
        return;
    }
    const quint32 id = quint32(idValue);
    // Only an answer to a probe this session sent, with the time it sent.
    const auto pending = std::find_if(d->pendingClockProbes.begin(), d->pendingClockProbes.end(),
        [id](const std::pair<quint32, qint64>& probe) { return probe.first == id; });
    if (pending == d->pendingClockProbes.end() || pending->second != t0) {
        return;
    }
    d->pendingClockProbes.erase(pending);
    if (!d->clockEstimator.addSample({t0, t1, t2, receivedNs})) {
        return;
    }
    if (generation != 0) {
        d->captureAnchor = AudioCaptureAnchor{quint32(generation), quint32(rtpTimestamp), capturedNs};
    } else {
        d->captureAnchor.reset();
    }
}

void RemoteMediaController::setTransmitHolder(quint64 holderEpoch, bool holderAway)
{
    if (d->holderEpoch == holderEpoch && d->holderAway == holderAway) {
        return;
    }
    d->holderEpoch = holderEpoch;
    d->holderAway = holderAway;
    // Fix wave 3 (ruling 9.3): the split changed its rule for this device
    // or the others, and the demand the Core holds is the plan made inside
    // the old share. Asking for what the operator wants again is what lets
    // a new present holder keep its whole request, and the others their
    // equal shares once a holder lets go.
    d->askingWanted = true;
    QTimer::singleShot(0, this, &RemoteMediaController::refreshSubscriptions);
}

void RemoteMediaController::resetMicrophoneQuality()
{
    d->micPending.clear();
    d->programPending.clear();
    d->programUntilMs = -1;
    if (d->micEncoder) {
        d->micEncoder->setBitrate(d->audioQualityChoice == RemoteAudioQualityChoice::SaveData
            ? 24000 : 48000);
        d->micEncoder->reset();
    }
}

void RemoteMediaController::setAudioProfileChoice(RemoteAudioProfile profile)
{
    setAudioQualityChoice(profile == RemoteAudioProfile::Lossless
        ? RemoteAudioQualityChoice::Lossless : RemoteAudioQualityChoice::High);
}

void RemoteMediaController::setAudioQualityChoice(RemoteAudioQualityChoice choice)
{
    const bool changed = choice != d->audioQualityChoice;
    d->audioQualityChoice = choice;
    d->audioProfileChoice = choice == RemoteAudioQualityChoice::Lossless
        ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
    AppSettings::instance().setValue(QLatin1String(kAudioProfileSettingKey),
        choice == RemoteAudioQualityChoice::SaveData ? QStringLiteral("SaveData")
                                                   : remoteAudioQualityChoiceName(choice));
    // Choosing again gives lossless a fresh chance without changing saved intent.
    const bool wasFallback = std::exchange(d->losslessFallback, false);
    d->linkTrial.end();
    d->linkTrialTimer->stop();
    if (changed || wasFallback) { resetMicrophoneQuality(); }
    const bool askAgain = (changed || wasFallback) && audioProfileNegotiated() && d->peer
        && d->peer->isReady();
    if (askAgain) {
        const QPointer<RemoteMediaController> self(this);
        // The main audio request sets this device's bitrate even when muted,
        // before headphones consumes it. Receiver-for-app streams stay fixed48.
        requestAudio();
        if (!self) { return; }
        for (auto& [sliceId, stream] : d->receiverStreams) { stream.faulted = false; }
        requestWantedReceiverAudio();
        if (!self) { return; }
        d->headphonesFaulted = false;
        requestHeadphonesAudio();
        if (!self) { return; }
    }
    refreshAudioStatus();
}

void RemoteMediaController::checkLosslessLink()
{
    if (!d->linkTrial.active()) {
        d->linkTrialTimer->stop();
        return;
    }
    // R-R3-43: every lossless stream the link carries, the speakers' mix
    // and each receiver stream, judged together.
    std::vector<RemoteAudioLinkTrial::StreamSample> samples;
    if (d->acceptedAudioContext && d->acceptedAudioContext->losslessEncoder) {
        samples.push_back({-1, d->audio->telemetry()});
    }
    for (const auto& [sliceId, stream] : d->receiverStreams) {
        if (stream.context && stream.context->losslessEncoder) {
            samples.push_back({sliceId, stream.receiver->telemetry()});
        }
    }
    // R-R3-45: and the headphones mix.
    if (d->headphonesContext && d->headphonesContext->enabled
        && d->headphonesContext->losslessEncoder) {
        samples.push_back({kHeadphonesTrialStream, d->headphones->telemetry()});
    }
    if (d->linkTrial.observe(d->clock.elapsed(), samples)
        == RemoteAudioLinkTrial::Verdict::Failed) {
        fallBackToOpus(d->linkTrial.failedOnInterruptions()
            ? QStringLiteral("the link stalled or burst while lossless audio played")
            : QStringLiteral("%1% of lossless packets lost or filled in over %2 s")
                  .arg(100.0 * d->linkTrial.lastWindowLoss().value_or(0.0), 0, 'f', 1)
                  .arg(RemoteAudioLinkTrial::kWindowMs / 1000));
    }
}

void RemoteMediaController::fallBackToOpus(const QString& cause)
{
    d->linkTrial.end();
    d->linkTrialTimer->stop();
    d->losslessFallback = true;
    resetMicrophoneQuality();
    const QString text = remoteAudioQualityReasonText(RemoteAudioQualityReason::NetworkTooSlow);
    qCInfo(lcRemoteMedia).noquote()
        << QStringLiteral("Remote audio: lossless link trial failed (%1); asking Core for Opus")
               .arg(cause);
    const QPointer<RemoteMediaController> self(this);
    requestAudio();
    if (!self) { return; }
    // R-R3-43: one fallback moves every receiver stream to Opus too, with
    // this one notice.
    requestWantedReceiverAudio();
    if (!self) { return; }
    // R-R3-45: and the headphones mix, with the same one notice.
    requestHeadphonesAudio();
    if (!self) { return; }
    emit errorOccurred(text);
}

// ---- R-R3-45: the headphones mix ----

QString RemoteMediaController::headphonesFaultText(RemoteAudioReceiver::Fault fault)
{
    // The receiver names its faults after the speaker; for the headphones
    // receiver they mean the headphones device. A fault stops the
    // headphones until they are turned off and on again (or another device
    // or audio quality is chosen), so each says how to try again.
    using Fault = RemoteAudioReceiver::Fault;
    const QString again =
        QStringLiteral(" Turn the headphones off and on in Setup, Audio, Devices to try again.");
    switch (fault) {
    case Fault::SpeakerOpenFailed:
        return QStringLiteral("The headphones could not be opened.") + again;
    case Fault::SpeakerTimingUnavailable:
        return QStringLiteral("The headphones stopped reporting their timing.") + again;
    case Fault::SpeakerCallbackTooLarge:
        return QStringLiteral("The headphones buffer is larger than remote playback "
                              "supports. Choose a smaller buffer or other headphones.");
    case Fault::SpeakerStalled:
        return QStringLiteral("The headphones stopped playing audio.") + again;
    case Fault::SpeakerWriteFailed:
        return QStringLiteral("Audio could not be sent to the headphones.") + again;
    case Fault::DecoderUnavailable:
        return QStringLiteral("The audio decoder for the headphones could not start on "
                              "this computer. Choosing the audio quality again tries once more.");
    case Fault::NoPackets:
    case Fault::DecodeFailed:
    case Fault::ClockBuffer:
        // The receiver restarts itself after these; none of them persists.
        return QStringLiteral("Audio on the headphones was interrupted.");
    }
    return {};
}

bool RemoteMediaController::headphonesMixNegotiated() const
{
    return audioProfileNegotiated() && d->client->capabilities().headphonesMixVersion >= 1;
}

// ── iPhone app plan Task 36 (R-IOS-13): the microphone uplink ──────────────

bool RemoteMediaController::micLineNegotiated() const
{
    return d->client && d->client->capabilities().remoteTxVersion >= 1;
}

void RemoteMediaController::setHoldsTransmit(bool holds)
{
    if (d->holdsTransmit == holds) {
        return;
    }
    d->holdsTransmit = holds;
    reconcileMicUplink();
}

void RemoteMediaController::setMicKeyDown(bool down)
{
    if (d->micKeyDown == down) {
        return;
    }
    d->micKeyDown = down;
    reconcileMicUplink();
}

void RemoteMediaController::setVoxArmed(bool armed)
{
    if (d->voxArmed == armed) {
        return;
    }
    d->voxArmed = armed;
    reconcileMicUplink();
}

bool RemoteMediaController::micUplinkRunning() const
{
    return d->micRunning;
}

quint64 RemoteMediaController::micPacketsSent() const
{
    return d->micPacketsSent;
}

bool RemoteMediaController::micLineOpen() const
{
    return micLineNegotiated() && d->peer && d->peer->isReady() && d->peer->micAudioSsrc() != 0;
}

void RemoteMediaController::noteMicLine()
{
    const bool open = micLineOpen();
    if (open == d->micLineWasOpen) {
        return;
    }
    d->micLineWasOpen = open;
    emit micLineChanged(open);
}

bool RemoteMediaController::micUplinkWanted() const
{
    if (const auto* tx = d->client->remoteTransmit();
        tx && tx->acceptedMicSource() == RemoteMicSource::RadioMic) {
        return false;
    }
    if (!micLineNegotiated() || !d->peer || !d->peer->isReady() || d->peer->micAudioSsrc() == 0
        || !d->model || d->model->audioEngine() == nullptr) {
        return false;
    }
    // VOX armed: the Core's VOX is on and this session may transmit now.
    const bool voxArmed = d->voxArmed && d->client->capabilities().txPermitted;
    return d->holdsTransmit || d->micKeyDown || voxArmed;
}

void RemoteMediaController::reconcileMicUplink()
{
    const bool wanted = micUplinkWanted();
    if (wanted != d->micRunning) {
        d->micRunning = wanted;
        if (wanted) {
            // The capture helper opens on the microphone chosen in Audio >
            // Devices, only now.
            d->micLease = d->model->audioEngine()->acquireCaptureDemand(
                CaptureSupervisor::Demand::RemoteWindow);
            qCInfo(lcRemoteMedia) << "Microphone uplink started";
        } else {
            d->micLease.release();
            d->micPending.clear();
            d->programPending.clear();
            d->programUntilMs = -1;
            qCInfo(lcRemoteMedia) << "Microphone uplink stopped";
        }
    }
    const QPointer<RemoteMediaController> self(this);
    refreshAudioStatus();
    if (!self || !d->micRunning) {
        return;
    }
    // A program's audio, while it keeps coming, replaces the microphone:
    // what the microphone captured meanwhile is drained and dropped.
    const bool program = d->clock.elapsed() < d->programUntilMs;
    AudioEngine* engine = d->model->audioEngine();
    for (;;) {
        const int got = engine->pullTxMic(d->micScratch.data(),
                                          static_cast<int>(d->micScratch.size()));
        if (got <= 0) {
            break;
        }
        if (!program) {
            d->micPending.insert(d->micPending.end(), d->micScratch.begin(),
                                 d->micScratch.begin() + got);
        }
    }
    sendMicAudio(program ? d->programPending : d->micPending);
}

void RemoteMediaController::sendMicAudio(std::vector<float>& pending)
{
    const float* mono = pending.data();
    const int frames = static_cast<int>(pending.size());
    // Whole packets only; the rest waits for the next pump.
    const bool lossless = d->peer && d->peer->micLosslessNegotiated()
        && d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback;
    const int packetFrames = lossless ? PcmAudioCodecConfig::kPacketFrames
                                      : RemoteMicConfig::kOpusFrameSamples;
    const quint32 ssrc = d->peer ? d->peer->micAudioSsrc() : 0;
    int sent = 0;
    const QPointer<RemoteMediaController> self(this);
    while (ssrc != 0 && frames - sent >= packetFrames) {
        const float* frame = mono + sent;
        QByteArray packet;
        if (lossless) {
            // The window's microphone in both channels (the Core takes their
            // mean).
            QVector<float> stereo(packetFrames * 2);
            for (int i = 0; i < packetFrames; ++i) {
                const float sample = std::clamp(frame[i], -1.0f, 1.0f);
                stereo[2 * i] = sample;
                stereo[2 * i + 1] = sample;
            }
            packet = PcmAudioPacketiser{}.encode(stereo, d->micSequence, d->micTimestamp, ssrc).packet;
        } else {
            packet = d->micEncoder->encode(frame, d->micSequence, d->micTimestamp, ssrc);
        }
        ++d->micSequence;
        d->micTimestamp += static_cast<quint32>(packetFrames);
        sent += packetFrames;
        if (!packet.isEmpty() && d->peer && d->peer->sendMicRtp(packet)) {
            ++d->micPacketsSent;
        }
        if (!self) {
            return;
        }
    }
    pending.erase(pending.begin(), pending.begin() + std::min<size_t>(pending.size(),
                                                                      static_cast<size_t>(sent)));
}

void RemoteMediaController::pushProgramAudio(const float* samples, int frames, int channels,
                                             int sampleRateHz)
{
    if (const auto* tx = d->client->remoteTransmit();
        tx && tx->acceptedMicSource() == RemoteMicSource::RadioMic) {
        return;
    }
    if (samples == nullptr || frames <= 0 || channels < 1 || channels > 2 || sampleRateHz <= 0
        || !micLineNegotiated()) {
        return;
    }
    // The left channel, as the Core's own TCI transmit takes it.
    std::vector<float> mono(static_cast<size_t>(frames));
    for (int f = 0; f < frames; ++f) {
        mono[static_cast<size_t>(f)] = channels == 2 ? samples[2 * f] : samples[f];
    }
    if (sampleRateHz != RemoteMicConfig::kSampleRate) {
        if (!d->programResampler || d->programRateHz != sampleRateHz) {
            d->programResampler = std::make_unique<Resampler>(
                sampleRateHz, RemoteMicConfig::kSampleRate, std::max(frames, 4096));
            d->programRateHz = sampleRateHz;
        }
        const QByteArray out = d->programResampler->process(mono.data(), frames);
        const auto* resampled = reinterpret_cast<const float*>(out.constData());
        mono.assign(resampled, resampled + out.size() / static_cast<qsizetype>(sizeof(float)));
    }
    d->programUntilMs = d->clock.elapsed() + kProgramAudioHoldMs;
    if (!d->micRunning) {
        return;
    }
    d->programPending.insert(d->programPending.end(), mono.begin(), mono.end());
}

QString RemoteMediaController::headphonesProblem() const
{
    return d->headphonesProblem;
}

RemoteAudioReceiverTelemetry RemoteMediaController::headphonesTelemetry() const
{
    return d->headphones->telemetry();
}

std::optional<RemoteAudioContextMessage> RemoteMediaController::acceptedHeadphonesContext() const
{
    return d->headphonesContext;
}

void RemoteMediaController::setHeadphonesProblem(const QString& problem)
{
    if (problem == d->headphonesProblem) { return; }
    d->headphonesProblem = problem;
    emit headphonesProblemChanged(problem);
}

bool RemoteMediaController::headphonesWanted() const
{
    // This computer can play a headphones mix: headphones open here and
    // none of them failed. The Core sends it only while some receiver is
    // routed to the headphones.
    return d->model && d->model->audioEngine()->headphonesAvailable()
        && !d->headphonesFaulted;
}

void RemoteMediaController::requestHeadphonesAudio()
{
    if (!d->peer || !d->peer->isReady() || !headphonesMixNegotiated()) { return; }
    const bool wanted = headphonesWanted();
    // Nothing asked for yet on this connection: nothing to stop.
    if (!wanted && d->headphonesRevision == 0) { return; }
    ++d->headphonesRevision;
    if (!d->headphonesRevision) { ++d->headphonesRevision; }
    d->headphonesRequested = wanted;
    d->headphonesRetryPending = false;
    d->headphonesLastRequestMs = d->clock.elapsed();
    // The one quality choice, as the speakers' stream asks for it.
    const RemoteAudioProfile profile =
        d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback
        ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("headphones-audio")},
                     {QStringLiteral("revision"), double(d->headphonesRevision)},
                     {QStringLiteral("enabled"), wanted},
                     {QStringLiteral("profile"), remoteAudioProfileToWire(profile)}});
}

void RemoteMediaController::receiveHeadphonesAudioContext(const QJsonObject& payload)
{
    const std::optional<RemoteAudioContextMessage> context =
        decodeHeadphonesAudioContext(payload);
    if (!context || context->revision != d->headphonesRevision
        || !isNewerGeneration(context->generation, d->headphonesGeneration)
        || d->headphonesSsrc == 0 || context->ssrc != d->headphonesSsrc) { return; }
    d->headphonesGeneration = context->generation;
    d->headphonesContext = context;
    d->headphonesRetryPending = false;
    d->headphones->stop();
    // start() can report a headphones failure synchronously, and a listener
    // to that report may retire this controller.
    const QPointer<RemoteMediaController> self(this);
    if (context->enabled && headphonesWanted()) {
        const RemoteAudioProfile profile = context->losslessEncoder
            ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
        if (d->headphones->start(context->ssrc, context->firstTimestamp, profile)) {
            setHeadphonesProblem(QString());
            qCInfo(lcRemoteMedia).noquote()
                << QStringLiteral("Remote headphones audio receiving: %1, context %2")
                       .arg(reportedAudioProfile(*context)).arg(context->generation);
        }
        if (!self) { return; }
    } else if (!context->enabled) {
        const RemoteAudioOffReason reason =
            context->offReason.value_or(RemoteAudioOffReason::EncoderUnavailable);
        if (reason == RemoteAudioOffReason::EncoderUnavailable) {
            setHeadphonesProblem(QString::fromLatin1(kHeadphonesCoreCouldNotStart));
        } else if (!d->headphonesFaulted) {
            // No receiver on the headphones, this computer asked it off, or
            // the radio or media is not ready: the speakers' status says
            // the last two, and the first two leave nothing to explain.
            setHeadphonesProblem(QString());
        }
        if (!self) { return; }
    }
    reconcileLinkTrial();
    refreshAudioStatus();
}

void RemoteMediaController::onHeadphonesRestart(const QString& reason,
                                                RemoteAudioReceiver::Fault fault)
{
    qCWarning(lcRemoteMedia).noquote()
        << QStringLiteral("Remote headphones audio: %1").arg(reason);
    d->headphones->stop();
    // R-R3-23: a lossless headphones mix's restart counts against the one
    // link trial, as the speakers' does.
    if (d->headphonesContext && d->headphonesContext->losslessEncoder
        && d->linkTrial.active() && linkInterruption(fault)
        && d->linkTrial.noteInterruption(d->clock.elapsed())
            == RemoteAudioLinkTrial::Verdict::Failed) {
        fallBackToOpus(QStringLiteral("headphones stream restarts while lossless audio plays"));
        return;
    }
    if (!d->headphonesRetryPending) {
        d->headphonesRetryPending = true;
        const QString connection = d->connectionId;
        const quint32 revision = d->headphonesRevision;
        const int delay = int(d->headphonesRestartBackoff.nextDelayMs(
            d->clock.elapsed(), d->headphonesLastRequestMs));
        QTimer::singleShot(delay, Qt::PreciseTimer, this, [this, connection, revision] {
            if (!d->headphonesRetryPending || connection != d->connectionId
                || revision != d->headphonesRevision) { return; }
            d->headphonesRetryPending = false;
            requestHeadphonesAudio();
        });
    }
    reconcileLinkTrial();
    refreshAudioStatus();
}

void RemoteMediaController::onHeadphonesError(const QString& reason,
                                              RemoteAudioReceiver::Fault fault)
{
    // The headphones device failed on this computer. Only the headphones
    // stop: the speakers' receiver and stream are untouched.
    qCWarning(lcRemoteMedia).noquote()
        << QStringLiteral("Remote headphones playback failed: %1").arg(reason);
    d->headphones->stop();
    d->headphonesFaulted = true;
    d->headphonesRetryPending = false;
    const QPointer<RemoteMediaController> self(this);
    // headphonesWanted() is false now: the Core is asked to stop the mix.
    requestHeadphonesAudio();
    if (!self) { return; }
    const QString text = headphonesFaultText(fault);
    setHeadphonesProblem(text);
    if (!self) { return; }
    reconcileLinkTrial();
    refreshAudioStatus();
    if (!self) { return; }
    emit errorOccurred(text);
}

void RemoteMediaController::retryAudio()
{
    if (!d->peer || !d->model || d->model->audioEngine()->masterMuted()) { return; }
    // R-R3-21: the operator asked again: the backoff starts over.
    d->audioRestartBackoff.reset();
    requestAudio();
}

void RemoteMediaController::refreshAudioStatus()
{
    const RemoteAudioReceiverTelemetry playback = d->audio->telemetry();
    if (d->audioFailure
        && remoteAudioFailureRecovered(*d->audioFailure, d->epoch, d->connectionId,
                                       d->acceptedAudioContext, playback)) {
        d->audioFailure.reset();
    }
    RemoteAudioStatusInputs inputs;
    inputs.mediaSession = !d->peer.isNull();
    inputs.muted = d->model && d->model->audioEngine()->masterMuted();
    inputs.radioConnected = d->model && d->model->isConnected();
    inputs.context = d->acceptedAudioContext;
    inputs.receiverRunning = playback.running;
    inputs.playing = playback.running && playback.decodedPackets > 0
        && playback.lastDeviceProgressAgeMs
        && *playback.lastDeviceProgressAgeMs < kPlaybackProgressWindowMs;
    inputs.restarting = d->audioRestarting;
    if (d->audioFailure) { inputs.problem = d->audioFailure->fault; }

    RemoteAudioStatus status;
    status.state = deriveRemoteAudioState(inputs);
    status.detailNegotiated = audioDetailNegotiated();
    if (d->acceptedAudioContext) { status.encoder = d->acceptedAudioContext->encoder; }
    status.selectedOutput = d->selectedOutput;
    status.problem = inputs.problem;
    status.retryAvailable = inputs.mediaSession && !inputs.muted
        && (status.state == RemoteAudioStatus::State::PlaybackProblem
            || status.state == RemoteAudioStatus::State::CoreCouldNotStart);
    // R-R3-23: the choice, what Core runs, and why Lossless is not running.
    status.chosenProfile = d->audioProfileChoice;
    status.chosenQuality = d->audioQualityChoice;
    status.saveDataUnavailableReason = audioQualityUnavailableReason(RemoteAudioQualityChoice::SaveData);
    status.losslessUnavailableReason = audioQualityUnavailableReason(RemoteAudioQualityChoice::Lossless);
    const auto* transmit = d->client->remoteTransmit();
    if (transmit && transmit->acceptedMicSource() == RemoteMicSource::RadioMic) {
        status.microphoneFormat = QStringLiteral("Radio microphone at the Core (no microphone stream from this computer)");
    } else if (micLineOpen()) {
        const bool losslessMic = d->peer->micLosslessNegotiated()
            && d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback;
        if (losslessMic) {
            status.microphoneFormat = QStringLiteral("Lossless stereo, 16-bit, 48 kHz, 4 ms packets");
        } else if (d->micEncoder && d->micEncoder->isReady()) {
            status.microphoneFormat = QStringLiteral("Opus mono, %1\u00A0kbit/s target, 20\u00A0ms packets")
                .arg(d->micEncoder->targetBitrate() / 1000);
        } else {
            status.microphoneFormat = QStringLiteral("The microphone encoder could not start");
        }
        status.microphoneFormat += d->micRunning ? QStringLiteral(" (sending)")
                                                : QStringLiteral(" (not sending)");
    }
    if (d->headphonesContext) {
        RemoteAudioStatus headphones;
        headphones.detailNegotiated = true;
        headphones.encoder = d->headphonesContext->encoder;
        headphones.losslessEncoder = d->headphonesContext->losslessEncoder;
        status.headphonesFormat = remoteAudioCodecText(headphones);
    }
    status.profileChoiceAvailable = inputs.mediaSession && audioProfileNegotiated();
    if (const auto& context = d->acceptedAudioContext) {
        if (context->profile) {
            status.runningProfile = context->profile;
        } else if (context->enabled) {
            status.runningProfile = RemoteAudioProfile::Opus; // a Core without the choice
        }
        status.losslessEncoder = context->losslessEncoder;
        status.bitrateRefusal = context->opusBitrateRefusal;
    }
    if (d->audioProfileChoice == RemoteAudioProfile::Lossless && inputs.mediaSession) {
        if (d->losslessFallback) {
            status.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        } else if (!audioProfileNegotiated()) {
            status.qualityReason = RemoteAudioQualityReason::CoreCannotSend;
        } else if (d->acceptedAudioContext && d->acceptedAudioContext->profileRefusal) {
            status.qualityReason =
                *d->acceptedAudioContext->profileRefusal == RemoteAudioProfileRefusal::NotAllowed
                ? RemoteAudioQualityReason::CoreNotAllowed
                : RemoteAudioQualityReason::ConnectionUnavailable;
        }
    }

    // R-R3-43: each wanted receiver stream, by slice id.
    for (const auto& [sliceId, stream] : d->receiverStreams) {
        RemoteReceiverAudioStatus receiver;
        receiver.sliceId = sliceId;
        if (stream.stopped) {
            receiver.state = RemoteReceiverAudioStatus::State::Stopped;
            receiver.stopReason = stream.stopReason;
        } else if (stream.receiver->isRunning()) {
            receiver.state = RemoteReceiverAudioStatus::State::Receiving;
            receiver.runningProfile = stream.runningProfile;
            if (stream.runningProfile == RemoteAudioProfile::Opus && stream.context) {
                receiver.encoder = stream.context->encoder;
            }
        }
        status.receivers.append(receiver);
    }

    reconcileClockProbe();
    // Speaker progress and recovery are observed, not signalled, so poll
    // them while there is something to watch, and only then.
    const bool watch = playback.running || d->audioFailure.has_value();
    if (watch && !d->audioStatusTimer->isActive()) {
        d->audioStatusTimer->start();
    } else if (!watch && d->audioStatusTimer->isActive()) {
        d->audioStatusTimer->stop();
    }
    if (status == d->audioStatus) { return; }
    d->audioStatus = status;
    emit audioStatusChanged();
}

void RemoteMediaController::stop()
{
    // iPhone app plan Task 29: a replacement under way and a peer still
    // draining go with the media session.
    d->replaceDeadline->stop();
    d->retireTimer->stop();
    d->replacePending = false;
    d->pendingReplaceKind = ReplaceKind::Normal;
    d->pendingMoveFolded = false;
    d->replacementMoveFolded = false;
    d->replaceRearms = 0;
    d->replaceRetry->stop();
    if (d->stallTimer) {
        d->stallTimer->stop();
    }
    d->lastAudio.invalidate();
    if (d->directUpgrade) {
        d->directUpgrade->stop();
    }
    d->directUpgradeStep = 0;
    d->lastMediaMs = -1;
    d->currentRouted = false;
    d->replacementRouted = false;
    d->replacementDirect = false;
    d->silenceFallbackFired = false;
    d->replacementFallback = false;
    d->fallbackFinishedMs = -1;
    d->txSilenceSuppressed = false;
    if (d->client) {
        d->client->setMediaTunnelInUse(false);
    }
    for (QPointer<MediaPeer>* slot : {&d->replacement, &d->retiring}) {
        if (MediaPeer* other = slot->data()) {
            *slot = nullptr;
            disconnect(other, nullptr, this, nullptr);
            other->stop();
            other->deleteLater();
        }
    }
    d->replacementId.clear();
    d->dual->newPathGone();
    d->dualTimer->stop();
    d->dualNew = nullptr;
    d->toLogical.clear();
    d->logicalSsrcs.clear();
    d->establishTimer->stop();
    d->awaitingDescription = false;
    // Task 36: the microphone line goes with the media connection; the
    // capture helper closes.
    d->micTimer->stop();
    d->micLease.release();
    d->micRunning = false;
    d->micPending.clear();
    d->programPending.clear();
    d->programUntilMs = -1;
    d->micPacketsSent = 0;
    d->ps3Generation = 0;
    d->ps3Assembler.reset(0);
    d->timer->stop();
    d->audio->stop();
    d->audioEnabled = false;
    d->audioRetryPending = false;
    ++d->audioRetryGeneration;
    d->audioRestartBackoff.reset();
    d->headphonesRestartBackoff.reset();
    d->audioRevision = 0;
    d->audioGeneration = 0;
    d->acceptedAudioContext.reset();
    // The choice outlives the session and is replayed on the next one; a
    // fallback and its trial belong to this one.
    d->audioProfileRequested = false;
    d->losslessFallback = false;
    d->linkTrial.end();
    d->linkTrialTimer->stop();
    // R-R3-35: the clock offset and the Core's capture belong to this
    // session; a reconnect measures afresh.
    d->clockProbeTimer->stop();
    d->clockEstimator.reset();
    d->captureAnchor.reset();
    d->pendingClockProbes.clear();
    // R-R3-21: so do the display's map and counters.
    d->presentTimer->stop();
    d->displayDelay.reset();
    d->lastAudioDelayNs.reset();
    d->displayCounters = {};
    // A playback problem belongs to its session and ends with it.
    d->audioFailure.reset();
    d->audioRestarting = false;
    // R-R3-43: receiver streams stop with the media connection. Their
    // sinks stay registered and are asked for again on the next one.
    d->receiverRevisions.clear();
    for (auto& [sliceId, stream] : d->iqStreams) {
        Q_UNUSED(sliceId);
        stream.enabled = false;
        stream.revision = 0;
        stream.generation = 0;
        stream.nextSequence = 0;
        stream.sampleRate = 0;
    }
    d->iqBytesPerSecond = 0;
    d->iqNegotiated = false;
    d->receiverSsrcs.clear();
    // R-R3-45: the headphones mix stops with the media connection and is
    // asked for again on the next one. A headphones device that failed
    // stays failed (fix wave): it is asked for again only when the device
    // or its configuration changes, not on every reconnect.
    d->headphones->stop();
    d->headphonesSsrc = 0;
    d->headphonesContext.reset();
    d->headphonesRevision = 0;
    d->headphonesGeneration = 0;
    d->headphonesRequested = false;
    d->headphonesRetryPending = false;
    // Parity Task 32: the monitor route is asked for again on the next one.
    d->txMonitorNegotiated = false;
    d->monitorRevision = 0;
    d->monitorContext.reset();
    QList<int> interrupted;
    for (auto& [sliceId, stream] : d->receiverStreams) {
        stream.tciStage->invalidate();
        stream.receiver->stop();
        stream.ssrc = 0;
        stream.generation = 0;
        stream.context.reset();
        stream.runningProfile.reset();
        stream.heldBy.reset();
        stream.faulted = false;
        stream.retryPending = false;
        stream.restartBackoff.reset();
        interrupted.append(sliceId);
    }
    d->connectionId.clear();
    d->pendingPs3.reset();
    d->ps3Refused = false;
    d->ps3RefusalReason.clear();
    d->ps3RefusedGeneration = 0;
    d->accountedPs3 = false;
    d->allocationCacheIdentity.clear();
    d->cachedAllocation.reset();
    d->cachedAllocationError.clear();
    d->askingWanted = false;
    d->wantedCharge = {};
    d->wantedIntents.clear();
    d->resizeAsk.reset();
    if (d->peer) {
        MediaPeer* old = d->peer;
        d->peer = nullptr;
        disconnect(old, nullptr, this, nullptr);
        old->stop();
        old->deleteLater();
    }
    emit networkPathChanged();
    noteMicLine();
    QList<QPair<QPointer<SpectrumWidget>, QString>> retiredWidgets;
    retiredWidgets.reserve(static_cast<qsizetype>(d->bindings.size()));
    for (const auto& [id, binding] : d->bindings) {
        if (binding.widget) {
            binding.widget->traceRxHistoryEvent(SpectrumWidget::RxHistoryEvent::MediaRetired,
                                                binding.observedStream, binding.observedStreamEpoch);
        }
        retiredWidgets.append({binding.widget, binding.panId});
        if (binding.isMini()) { emit miniDisplayUnavailable(binding.miniSliceId); }
    }
    d->bindings.clear();
    d->ctunStreams.clear();
    d->centreRequests.clear();
    const QPointer<RemoteMediaController> self(this);
    if (!d->destroying) {
        // The Core's reasons end with the session; a failed device's stays.
        if (!d->headphonesFaulted) {
            setHeadphonesProblem(QString());
            if (!self) { return; }
        }
        for (int sliceId : interrupted) {
            notifyReceiverStopped(sliceId, remoteAudioOffReasonToWire(
                RemoteAudioOffReason::MediaNotReady));
            if (!self) { return; }
        }
    }
    refreshAudioStatus();
    if (!self) { return; }
    for (const auto& [widget, panId] : retiredWidgets) {
        if (widget) {
            widget->clearRemoteSpectrum();
            if (!self || !widget) { return; }
            widget->applyRemoteCtunState(false, false);
            if (!self) { return; }
        }
        if (!panId.isEmpty()) {
            setPanStatus(panId, PanDisplayState{});
            if (!self) { return; }
        }
    }
    // Endpoints retired before this point left their pans; drop any grant
    // line they still show, keeping the rest of each pan's status.
    for (const QString& panId : d->panBaseStatus.keys()) {
        refreshPanGrantStatus(panId);
        if (!self) { return; }
    }
    d->panBaseStatus.clear();
    d->panBudgetReason.clear();
}

void RemoteMediaController::requestRecovery(quint32 expectedEpoch, const QString& reason)
{
    if (d->recoveryRequested) { return; }
    d->recoveryRequested = true;
    QPointer<RemoteMediaController> self(this);
    stop();
    if (!self) { return; }
    emit self->errorOccurred(reason);
    // A diagnostic consumer may synchronously destroy this controller (or
    // its StationClient parent). Never continue through a deleted sender.
    if (!self) { return; }
    emit self->recoveryRequested(expectedEpoch, reason);
}

void RemoteMediaController::settleWithoutRetry(quint32 expectedEpoch, const QString& reason)
{
    // R-R3-28, review I1. Media ends here for good, with no retry, and the
    // session carries on as control only, which its handshake has already
    // proven: the reconnect backoff starts over, so a later, unrelated drop
    // retries at the first step. Epoch-scoped in StationClient, so a
    // retired session's error cannot reset a newer session's schedule.
    if (d->client) { d->client->noteMediaEstablished(expectedEpoch); }
    const QPointer<RemoteMediaController> self(this);
    stop();
    if (!self) { return; }
    emit errorOccurred(reason);
}

// ── iPhone app plan Task 29 (R-IOS-16): replacing the media connection ─────
//
// The remote media control document, "Replacing the media connection". A
// session that moved to a better path asks for a new media connection
// beside the current one; audio then arrives on both (each stream's packets
// taken once, by RTP timestamp) until the Core says the new one took over,
// and the old one's packets still in flight are taken for a while after.

namespace {

// A peer's audio SSRCs in stream order: main, receiver 0 to 3, headphones.
QList<quint32> audioSsrcsOf(const MediaPeer* peer)
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

} // namespace

QString RemoteMediaController::mediaConnectionId() const
{
    return d->connectionId;
}

std::optional<NetworkPathSnapshot> RemoteMediaController::currentNetworkPath() const
{
    if (QThread::currentThread() != thread() || !d->client
        || !d->client->isHandshakeComplete() || !d->client->mediaAvailable()
        || d->epoch != d->client->sessionEpoch() || !d->peer || !d->peer->isReady()) {
        return std::nullopt;
    }
    const auto selected = d->peer->selectedPath();
    return selected ? selected->networkPathSnapshot() : std::nullopt;
}

bool RemoteMediaController::replacingConnection() const
{
    return !d->replacement.isNull();
}

quint64 RemoteMediaController::duplicateAudioDropped() const
{
    return d->duplicatesDropped
        + ((d->dual->active() || d->retiring) ? d->dual->duplicatesDropped() : 0);
}

bool RemoteMediaController::replaceConnection()
{
    return startReplacement(ReplaceKind::Normal);
}

bool RemoteMediaController::upgradeToDirectConnection()
{
    return startReplacement(ReplaceKind::Direct);
}

bool RemoteMediaController::startReplacement(ReplaceKind kind, bool carriesFoldedMove)
{
    const bool direct = kind == ReplaceKind::Direct;
    if (!d->client || !d->client->mediaAvailable() || !d->peer || !d->peer->isReady()
        || d->replacement || d->retiring || d->epoch != d->client->sessionEpoch()
        || d->client->capabilities().mediaReplaceVersion < 1 || d->logicalSsrcs.isEmpty()) {
        return false;
    }
    // Not while this window is keyed or has VOX armed, nor while the Core is
    // on the air (the Core refuses then too).
    if (RemoteTransmitClient* tx = d->client->remoteTransmit();
        tx != nullptr && tx->keepaliveRunning()) {
        return false;
    }
    if (d->model && d->model->isTransmitting()) {
        return false;
    }
    // The direct media ladder: a direct-only replace only to a Core that
    // takes one, and never while this window is keyed or has VOX armed or
    // the Core is on the air (a refused one is not tried again until the
    // next step).
    if (direct) {
        const bool voxArmed = d->voxArmed && d->client->capabilities().txPermitted;
        if (!d->client->mediaDirectAvailable() || d->holdsTransmit || d->micKeyDown || voxArmed
            || d->coreTransmitting()) {
            return false;
        }
    }
    std::optional<IceConfiguration> nextIce;
    if (direct) {
        nextIce = d->client->mediaDirectIceConfiguration();
    } else if (kind == ReplaceKind::TunnelFallback && d->tunnelNegotiated) {
        // The fix wave: the tunnel's own candidate alone (no STUN, no host
        // candidates), so ICE cannot settle on the silent direct pair
        // again. The replace on the wire is the plain three-field one.
        nextIce = d->client->mediaTunnelOnlyIceConfiguration();
        if (!nextIce) { nextIce = mediaIceConfiguration(); }
    } else {
        nextIce = mediaIceConfiguration();
    }
    // A raw tag-2 relay leg in use has one agent destination: a new
    // connection without the tunnel's own framing would redirect it.
    if (direct && !d->currentRouted) {
        const auto path = d->peer->selectedPath();
        if (path && path->viaLoopbackShim() && (!nextIce || !nextIce->mediaRouting())) {
            return false;
        }
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* peer = new MediaPeer(this, d->factory);
    d->replacement = peer;
    d->replacementId = id;
    const quint32 epoch = d->epoch;
    const auto current = [this, peer, epoch] {
        return d->replacement == peer && d->client && d->client->mediaAvailable()
            && d->client->sessionEpoch() == epoch;
    };
    // The replacement's own description and candidates go under its own
    // id (send() would name the current connection).
    connect(peer, &MediaPeer::controlReady, this, [this, current](const QJsonObject& payload) {
        if (current() && d->client) { d->client->sendMediaControl(payload, d->epoch); }
    });
    connect(peer, &MediaPeer::displayReceived, this, [this, current](const QByteArray& packet) {
        if (!current()) { return; }
        reportDisplayDrops();
        receiveDisplay(packet);
    });
    connect(peer, &MediaPeer::rtpReceived, this, [this, current, peer](const QByteArray& packet) {
        if (current()) { routeRtp(packet, peer); }
    });
    connect(peer, &MediaPeer::ready, this, [this, current] {
        if (current()) {
            d->replaceDeadline->stop();
            qCInfo(lcRemoteMedia) << "The new audio and display connection is open";
        }
    });
    connect(peer, &MediaPeer::connectionFailed, this, [this, current](const QString& reason) {
        if (current()) { dropReplacement(reason); }
    });
    connect(peer, &MediaPeer::closed, this, [this, current] {
        if (current()) { dropReplacement(QStringLiteral("the new connection closed")); }
    });
    connect(peer, &MediaPeer::errorOccurred, this, [this, current](const QString& reason) {
        if (current()) { dropReplacement(reason); }
    });
    d->replacementRouted = nextIce && nextIce->mediaRouting();
    d->replacementDirect = direct;
    d->replacementFallback = kind == ReplaceKind::TunnelFallback;
    d->replacementMoveFolded = carriesFoldedMove;
    peer->setIceConfiguration(nextIce);
    const QPointer<MediaPeer> started(peer);
    const bool ok = peer->start(IMediaTransport::Role::Answerer, id,
                                IMediaTransport::kDefaultAudioTargetBitrate,
                                /*offerLosslessAudio=*/false, receiverAudioNegotiated(),
                                headphonesMixNegotiated(), micLineNegotiated(),
                                d->iqNegotiated);
    if (!ok || !started) {
        dropReplacement(QStringLiteral("the new connection could not start"));
        // Re-review: a connection that cannot start is tried again only a
        // few times (tryPendingReplace counts it).
        d->replaceStartFailed = true;
        return false;
    }
    // The new peer's streams are the streams this session started with.
    const QList<quint32> theirs = audioSsrcsOf(peer);
    for (qsizetype i = 0; i < theirs.size() && i < d->logicalSsrcs.size(); ++i) {
        if (theirs.at(i) != 0 && d->logicalSsrcs.at(i) != 0) {
            d->toLogical.insert(theirs.at(i), d->logicalSsrcs.at(i));
        }
    }
    d->dualNew = peer;
    d->dualFromNew = 0;
    d->dualFromOld = 0;
    d->dual->start(d->dualClock.elapsed());
    d->dualTimer->start();
    d->replaceDeadline->start(d->descriptionDeadlineMs + IceConfiguration::kConnectDeadlineMs);
    qCInfo(lcRemoteMedia) << (direct ? "Trying audio and display on a direct connection"
                                     : "Moving audio and display to a new connection");
    const QPointer<RemoteMediaController> self(this);
    QJsonObject replace{{QStringLiteral("op"), QStringLiteral("replace")},
                        {QStringLiteral("connectionId"), id},
                        {QStringLiteral("replaces"), d->connectionId}};
    // The direct media ladder: only a direct-only replace names the field;
    // every other replace stays the three fields an older Core expects.
    if (direct) {
        replace.insert(QStringLiteral("mediaDirectVersion"), 1);
    }
    d->client->sendMediaControl(replace, d->epoch);
    return self && d->replacement == peer;
}

void RemoteMediaController::receiveReplacementControl(const QJsonObject& payload)
{
    const QString op = payload.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("description") || op == QLatin1String("candidate")) {
        d->replacement->acceptControl(payload);
        return;
    }
    // The Core did not move media, or dropped the new connection: the
    // current connection carries on.
    if (op == QLatin1String("rejected") && payload.value(QStringLiteral("endpointId")).toDouble() == 0
        && payload.value(QStringLiteral("reason")).isString()) {
        const QString reason = payload.value(QStringLiteral("reason")).toString();
        const bool wasDirect = d->replacementDirect;
        const bool wasFallback = d->replacementFallback;
        const bool wasMoveFolded = d->replacementMoveFolded;
        dropReplacement(reason.left(512));
        // The direct media ladder: a refused direct-only replace waits for
        // the next step of its own schedule; it never marks a move pending.
        if (wasDirect) {
            return;
        }
        // Task 29 fix wave (Important 1), re-review: a Core that went on the
        // air just as the replacement arrived refused it; try again once it
        // is back on receive (tryPendingReplace waits while the window sees
        // it transmitting). Only that refusal: any other stays refused.
        if (reason == QLatin1String(DaemonMediaController::kReplaceTransmittingReason)) {
            // A session move this replace carried (folded into the waiting
            // fallback), or one that came while it was under way (pending
            // now), is still followed when media comes back on the direct
            // pair: the refusal keeps it folded into the retried fallback.
            const bool moveWaiting = wasMoveFolded || d->replacePending;
            d->replacePending = true;
            d->pendingReplaceKind = wasFallback ? ReplaceKind::TunnelFallback
                                                : ReplaceKind::Normal;
            d->pendingMoveFolded = wasFallback && moveWaiting;
            d->replaceRetry->start();
        }
        return;
    }
    if (op == QLatin1String("replace")
        && payload.value(QStringLiteral("replaces")) == d->connectionId && payload.size() == 3) {
        promoteReplacement();
    }
}

void RemoteMediaController::promoteReplacement()
{
    d->replaceDeadline->stop();
    MediaPeer* const next = d->replacement;
    MediaPeer* const old = d->peer;
    if (next == nullptr || old == nullptr) {
        return;
    }
    // The old connection's packets still in flight are taken for a while.
    disconnect(old, nullptr, this, nullptr);
    connect(old, &MediaPeer::rtpReceived, this, [this, old](const QByteArray& packet) {
        if (d->retiring == old) { routeRtp(packet, old); }
    });
    connect(old, &MediaPeer::displayReceived, this, [this, old](const QByteArray& packet) {
        if (d->retiring == old) { receiveDisplay(packet); }
    });
    d->retiring = old;
    d->retireTimer->start(DaemonMediaController::kReplaceDrainMs);
    disconnect(next, nullptr, this, nullptr);
    d->replacement = nullptr;
    d->peer = next;
    // The direct media ladder: the connection in use is now the new one,
    // and its silence is counted from here.
    d->currentRouted = std::exchange(d->replacementRouted, false);
    d->replacementDirect = false;
    d->replacementMoveFolded = false;
    d->lastMediaMs = d->allocationClock();
    // The fix wave: a finished fallback waits one window for media; the
    // tunnel's stall rule counts from here too, not from the old silence.
    if (std::exchange(d->replacementFallback, false)) {
        d->fallbackFinishedMs = d->lastMediaMs;
        if (d->lastAudio.isValid()) { d->lastAudio.restart(); }
    }
    emit networkPathChanged();
    d->connectionId = d->replacementId;
    d->replacementId.clear();
    connectPeer(next, d->epoch);
    for (const auto& [sliceId, stream] : d->iqStreams) {
        if (stream.wanted) { sendIqRequest(sliceId, true); }
    }
    // The old connection sends nothing new now; the new one's lead eases.
    d->dual->oldPathDone(d->dualClock.elapsed());
    qCInfo(lcRemoteMedia).noquote()
        << QStringLiteral("Audio and display moved to the new connection (audio packets on "
                          "both: %1 on the old, %2 on the new, copies dropped %3, the new "
                          "one %4 ms ahead)")
               .arg(d->dualFromOld).arg(d->dualFromNew)
               .arg(d->dual->duplicatesDropped()).arg(d->dual->leadMs());
    noteMicLine();
}

void RemoteMediaController::retireOldPeer()
{
    d->retireTimer->stop();
    if (MediaPeer* old = d->retiring.data()) {
        d->retiring = nullptr;
        disconnect(old, nullptr, this, nullptr);
        old->stop();
        old->deleteLater();
    }
    // Once the old callback is disconnected, the overlap filter may settle.
    // A path change queued during the drain uses today's ICE configuration.
    if (!d->dual->active() && d->dualTimer->isActive()) {
        d->duplicatesDropped += d->dual->duplicatesDropped();
        d->dualTimer->stop();
    }
    if (d->replacePending) {
        QTimer::singleShot(0, this, &RemoteMediaController::tryPendingReplace);
    }
}

std::optional<IceConfiguration> RemoteMediaController::mediaIceConfiguration()
{
    // Task 29 step 2b: over the tunnel while it was declared and the
    // session still runs on a WebSocket (as the Core decides the same).
    if (d->tunnelNegotiated) {
        if (auto tunnel = d->client->mediaTunnelIceConfiguration()) {
            return tunnel;
        }
    }
    auto ice = d->client->sessionIceConfiguration();
    if (ice) {
        ice->setMediaRouting(d->relayRoutingNegotiated);
    }
    return ice;
}

bool RemoteMediaController::replacePending() const
{
    return d->replacePending;
}

void RemoteMediaController::markReplacePending()
{
    // A refused fallback waiting to be retried stays a fallback onto the
    // tunnel alone: the move changes the session's path, not the direct
    // pair's silence, and a normal replace could pick that pair again.
    const bool fallbackWaiting = d->replacePending
        && d->pendingReplaceKind == ReplaceKind::TunnelFallback;
    d->replacePending = true;
    d->pendingReplaceKind = fallbackWaiting ? ReplaceKind::TunnelFallback
                                            : ReplaceKind::Normal;
    d->pendingMoveFolded = fallbackWaiting;
    d->replaceRearms = 0;
    tryPendingReplace();
}

void RemoteMediaController::endWaitingFallback()
{
    // Media arrived: the silence a refused fallback answered is over, so a
    // retry onto the tunnel alone would move off a working pair for
    // nothing. A move folded into the wait is still followed, as a normal
    // replace.
    if (!d->replacePending || d->pendingReplaceKind != ReplaceKind::TunnelFallback) {
        return;
    }
    if (d->pendingMoveFolded) {
        d->pendingReplaceKind = ReplaceKind::Normal;
        d->pendingMoveFolded = false;
        return;
    }
    d->replacePending = false;
    d->replaceRetry->stop();
}

void RemoteMediaController::tryPendingReplace()
{
    if (!d->replacePending) {
        d->replaceRetry->stop();
        return;
    }
    // Media that has gone, or a Core that cannot replace it: nothing to
    // follow.
    if (!d->client || !d->client->mediaAvailable()
        || d->client->capabilities().mediaReplaceVersion < 1) {
        d->replacePending = false;
        d->replaceRetry->stop();
        return;
    }
    // A raw tag-2 relay leg has one agent destination. Replacing it would
    // redirect the live connection before the new one can take over.
    // The direct media ladder: judged by the connection in use (a tunnel
    // start routes over the tunnel's framing without the relay's).
    if (d->peer && !d->currentRouted) {
        const auto path = d->peer->selectedPath();
        const auto nextIce = mediaIceConfiguration();
        if (path && path->viaLoopbackShim() && (!nextIce || !nextIce->mediaRouting())) {
            d->replacePending = false;
            d->replaceRetry->stop();
            qCInfo(lcRemoteMedia) << "The older relay media path cannot overlap a replacement";
            return;
        }
    }
    // One under way: the next move is followed once it finishes.
    if (d->replacement || d->retiring) {
        d->replaceRetry->start();
        return;
    }
    d->replaceStartFailed = false;
    // The move folded into a fallback rides with the replace; a refusal
    // while transmitting puts it back (even one that arrives during the
    // send, when startReplacement returns false and this leaves it pending).
    if (startReplacement(d->pendingReplaceKind, d->pendingMoveFolded)) {
        d->pendingMoveFolded = false;
        d->replacePending = false;
        d->replaceRetry->stop();
        return;
    }
    // A new connection that could not start counts against the budget.
    if (d->replaceStartFailed && ++d->replaceRearms >= kMaxReplaceRearms) {
        d->replacePending = false;
        d->replaceRetry->stop();
        qCInfo(lcRemoteMedia) << "Audio and display stay on their connection: a new one could"
                              << "not start";
        return;
    }
    // Not yet (media not ready, keyed, VOX armed, on the air): again soon.
    d->replaceRetry->start();
}

void RemoteMediaController::dropReplacement(const QString& why)
{
    d->replaceDeadline->stop();
    MediaPeer* const peer = d->replacement;
    if (peer == nullptr) {
        return;
    }
    d->replacement = nullptr;
    d->replacementId.clear();
    d->replacementRouted = false;
    d->replacementDirect = false;
    d->replacementMoveFolded = false;
    if (std::exchange(d->replacementFallback, false)) {
        d->fallbackFinishedMs = d->allocationClock();
    }
    for (const quint32 ssrc : audioSsrcsOf(peer)) {
        d->toLogical.remove(ssrc);
    }
    disconnect(peer, nullptr, this, nullptr);
    peer->stop();
    peer->deleteLater();
    d->duplicatesDropped += d->dual->duplicatesDropped();
    d->dual->newPathGone();
    d->dualTimer->stop();
    d->dualNew = nullptr;
    qCInfo(lcRemoteMedia).noquote()
        << QStringLiteral("Audio and display stay on their connection: %1").arg(why.left(256));
}

int RemoteMediaController::directUpgradeDelayMs() const
{
    if (!d->directUpgrade->isActive()) {
        return -1;
    }
    return d->directUpgrade->interval();
}

bool RemoteMediaController::audioRestartPendingForTest() const
{
    return d->audioRetryPending;
}

void RemoteMediaController::holdAudioRestartForTest(bool held)
{
    d->audioRestartHeldForTest = held;
    if (held) {
        return;
    }
    if (const std::function<void()> step = std::exchange(d->heldAudioRestartStep, {})) {
        step();
    }
}

bool RemoteMediaController::audioRestartStepHeldForTest() const
{
    return bool(d->heldAudioRestartStep);
}

void RemoteMediaController::updateDirectUpgrade(bool viaTunnel)
{
    // The direct media ladder: while media rides the tunnel to a Core that
    // takes a direct-only replace, one is tried at each step of
    // PathRacer::kUpgradeRetryMs (the last step repeats).
    if (!viaTunnel || !d->client || !d->client->mediaDirectAvailable()
        || d->client->capabilities().mediaReplaceVersion < 1) {
        d->directUpgrade->stop();
        return;
    }
    if (d->directUpgrade->isActive() || d->replacement) {
        return;
    }
    const int last = int(PathRacer::kUpgradeRetryMs.size()) - 1;
    d->directUpgrade->start(PathRacer::kUpgradeRetryMs.at(
        std::size_t(std::min(d->directUpgradeStep, last))));
}

void RemoteMediaController::runDirectUpgradeStep()
{
    d->directUpgrade->stop();
    if (!d->peer) {
        return;
    }
    const auto onTunnel = [this] {
        const std::optional<MediaIcePath> path = d->peer->selectedPath();
        return d->tunnelNegotiated && path && path->viaLoopbackShim();
    };
    if (!onTunnel()) {
        updateDirectUpgrade(false);
        return;
    }
    const int last = int(PathRacer::kUpgradeRetryMs.size()) - 1;
    const QPointer<RemoteMediaController> self(this);
    // A move already waiting, or one under way, goes first.
    if (!d->replacePending && !d->replacement && !d->retiring) {
        upgradeToDirectConnection();
        if (!self) { return; }
    }
    d->directUpgradeStep = std::min(d->directUpgradeStep + 1, last);
    if (!d->peer || d->replacement) {
        return; // the replacement's end, or the next stall tick, re-arms it
    }
    updateDirectUpgrade(onTunnel());
}

void RemoteMediaController::checkMediaSilence()
{
    // The direct media ladder: a direct path whose media stopped for
    // kDirectMediaSilenceFallbackMs while control still runs goes back to
    // the tunnel alone (a plain replace whose connection offers only the
    // tunnel's candidate), and the direct-only schedule starts over. That
    // runs once per silence: if media has not returned a window after the
    // fallback finished, the window asks for recovery instead. The Core
    // keyed follows the existing link-loss path instead (its own starvation
    // rule), so nothing here runs while it transmits.
    if (!d->client || !d->peer || !d->peer->isReady() || d->replacement || d->retiring
        || d->epoch != d->client->sessionEpoch() || !d->client->mediaAvailable()
        || !d->client->mediaDirectAvailable()
        || d->client->capabilities().mediaReplaceVersion < 1) {
        return;
    }
    // The same test the audio request makes: silence is expected from a
    // Core this window does not ask for audio.
    const bool wanted = d->model && d->model->isConnected()
        && !d->model->audioEngine()->masterMuted();
    if (!wanted || !d->lastAudio.isValid() || d->coreTransmitting()) {
        return;
    }
    const qint64 now = d->allocationClock();
    if (d->fallbackFinishedMs >= 0) {
        if (now - d->fallbackFinishedMs < kDirectMediaSilenceFallbackMs) {
            return;
        }
        qCInfo(lcRemoteMedia) << "No audio or display from the Core for"
                              << (now - d->fallbackFinishedMs)
                              << "ms after moving back to the tunnel; starting audio and "
                                 "display again";
        d->fallbackFinishedMs = -1;
        d->lastAudio.invalidate();
        requestRecovery(d->epoch, QStringLiteral("Audio from the Core stopped. Starting "
                                                 "audio and display again."));
        return;
    }
    const std::optional<MediaIcePath> path = d->peer->selectedPath();
    const int rank = d->client->pathRank();
    if (!path || path->viaLoopbackShim() || rank == PathRacer::ServiceRelayed
        || rank == PathRacer::Floor) {
        return; // the tunnel and relays have the stall rule
    }
    if (d->lastMediaMs < 0) {
        d->lastMediaMs = now;
        return;
    }
    if (d->silenceFallbackFired || now - d->lastMediaMs < kDirectMediaSilenceFallbackMs) {
        return;
    }
    qCInfo(lcRemoteMedia) << "No audio or display from the Core for" << (now - d->lastMediaMs)
                          << "ms on a direct path; moving back to the tunnel";
    d->lastMediaMs = now;
    d->silenceFallbackFired = true;
    d->directUpgradeStep = 0;
    d->directUpgrade->stop();
    if (!startReplacement(ReplaceKind::TunnelFallback)) {
        // Nothing started, so nothing will finish: wait one window from here.
        d->fallbackFinishedMs = now;
    }
}

void RemoteMediaController::connectPeer(MediaPeer* peer, quint32 epoch)
{
    const auto current = [this, peer, epoch] {
        return d->peer == peer && d->client && d->client->mediaAvailable()
            && d->client->sessionEpoch() == epoch;
    };
    connect(peer, &MediaPeer::controlReady, this, [this, current](const QJsonObject& payload) {
        if (current()) { send(payload); }
    });
    connect(peer, &MediaPeer::displayReceived, this, [this, current](const QByteArray& packet) {
        if (!current()) { return; }
        // Rendering can end this session; report drops for it first.
        reportDisplayDrops();
        receiveDisplay(packet);
    });
    connect(peer, &MediaPeer::iqReceived, this, [this, current](const QByteArray& packet) {
        if (current()) { receiveIqFrame(packet); }
    });
    connect(peer, &MediaPeer::iqErrorOccurred, this, [this, current](const QString& reason) {
        if (!current()) { return; }
        QList<int> affected;
        for (const auto& [sliceId, stream] : d->iqStreams) {
            if (stream.enabled) { affected.append(sliceId); }
        }
        const QPointer<RemoteMediaController> self(this);
        for (int sliceId : affected) {
            if (!self || !current()) { return; }
            failIqStream(sliceId, reason);
        }
    });
    connect(peer, &MediaPeer::rtpReceived, this, [this, current, peer](const QByteArray& packet) {
        if (current()) { routeRtp(packet, peer); }
    });
    connect(peer, &MediaPeer::ready, this, [this, current, epoch] {
        if (current()) {
            emit networkPathChanged();
            // Established: the deadline stands down, and only now does the
            // session count as working for the reconnect backoff (R-R3-28).
            d->establishTimer->stop();
            d->client->noteMediaEstablished(epoch);
            noteMicLine();
            // Task 29 fix wave (Important 1): a move that came before media
            // was ready is followed now (after this handler's requests).
            if (d->replacePending) {
                QTimer::singleShot(0, this, &RemoteMediaController::tryPendingReplace);
            }
            if (!d->client->remoteDisplayBudgetLimits()) {
                qCDebug(lcRemoteMedia)
                    << "Core supplied no aggregate display limits; using per-display subscriptions";
            }
            d->timer->start();
            refreshSubscriptions();
            const QPointer<RemoteMediaController> self(this);
            requestAudio();
            if (!self || !current()) { return; }
            // R-R3-43: each receiver stream an app wants, after the mix.
            requestWantedReceiverAudio();
            if (!self || !current()) { return; }
            for (const auto& [sliceId, stream] : d->iqStreams) {
                if (stream.wanted) { sendIqRequest(sliceId, true); }
            }
            if (!self || !current()) { return; }
            // R-R3-45: and the headphones mix, when headphones are here.
            requestHeadphonesAudio();
            if (!self || !current()) { return; }
            // Parity Task 32: and where MON goes while this window holds
            // transmit.
            requestMonitorAudio();
        }
    });
    connect(peer, &MediaPeer::connectionFailed, this,
            [this, current, epoch](const QString& reason) {
        if (current()) {
            requestRecovery(epoch, reason);
        }
    });
    connect(peer, &MediaPeer::closed, this, [this, current, epoch] {
        if (current()) {
            emit networkPathChanged();
            requestRecovery(epoch, QStringLiteral("Station media connection closed"));
        }
    });
    const auto onPeerError = [this, current, epoch](const QString& reason) {
        if (current()) {
            if (d->startingPeer) {
                // Decided below, once start() says whether it refused.
                if (d->startRefusal.isEmpty()) { d->startRefusal = reason; }
                return;
            }
            settleWithoutRetry(epoch, reason);
        }
    };
    connect(peer, &MediaPeer::errorOccurred, this, onPeerError);
    // The transport reports a display-channel error only as a display
    // error; this computer handles it as it handles any media error.
    connect(peer, &MediaPeer::displayErrorOccurred, this, onPeerError);
}

void RemoteMediaController::routeRtp(const QByteArray& arrived, const MediaPeer* from)
{
    // Step 2b: the stall rule's clock.
    d->lastAudio.start();
    d->lastMediaMs = d->allocationClock();
    d->silenceFallbackFired = false;
    d->fallbackFinishedMs = -1;
    endWaitingFallback();
    if (d->stallTimer && !d->stallTimer->isActive()) {
        d->stallTimer->start();
    }
    // iPhone app plan Task 29: every peer's packets as the streams this
    // media session started with (their own SSRCs), and while two
    // connections carry them, merged (DualPathAudio): each packet once, on
    // the old connection's schedule.
    QByteArray packet = arrived;
    if (!d->toLogical.isEmpty() && packet.size() >= 12) {
        const quint32 ssrc = qFromBigEndian<quint32>(packet.constData() + 8);
        const auto it = d->toLogical.constFind(ssrc);
        if (it != d->toLogical.cend()) {
            qToBigEndian<quint32>(it.value(), packet.data() + 8);
        }
    }
    if (d->dual->active() || d->retiring) {
        const bool fromNew = from != nullptr && from == d->dualNew.data();
        ++(fromNew ? d->dualFromNew : d->dualFromOld);
        d->dual->submit(packet, fromNew, d->dualClock.elapsed());
        return;
    }
    deliverRtp(packet);
}

void RemoteMediaController::deliverRtp(const QByteArray& packet)
{
    // R-R3-43: split by stream id before the speakers' gate, so a
    // receiver stream reaches its consumers while the speakers are
    // muted. A receiver stream nobody plays now (stopping, restarting)
    // is dropped; everything else goes to the speakers' receiver as
    // before.
    if ((!d->receiverSsrcs.isEmpty() || d->headphonesSsrc != 0) && packet.size() >= 12) {
        const quint32 ssrc = qFromBigEndian<quint32>(packet.constData() + 8);
        // R-R3-45: the headphones mix goes to its own receiver, whatever
        // the speakers do; while that receiver is stopped it is dropped.
        if (d->headphonesSsrc != 0 && ssrc == d->headphonesSsrc) {
            if (d->headphones->isRunning()) { d->headphones->submit(packet); }
            return;
        }
        if (d->receiverSsrcs.contains(ssrc)) {
            for (auto& [sliceId, stream] : d->receiverStreams) {
                if (stream.ssrc == ssrc) {
                    stream.receiver->submit(packet);
                    break;
                }
            }
            return;
        }
    }
    if (d->audioEnabled) { d->audio->submit(packet); }
}

void RemoteMediaController::start()
{
    // stop() reports the retired audio status, and a listener may retire
    // this controller in turn.
    const QPointer<RemoteMediaController> self(this);
    stop();
    if (!self) { return; }
    if (!d->client || !d->client->mediaAvailable()) {
        return;
    }
    d->recoveryRequested = false;
    d->epoch = d->client->sessionEpoch();
    // start() first stops the previous peer. Seed the current Core state so
    // a media start during an existing TX still gets a full RX grace period.
    d->txSilenceSuppressed = d->coreTransmitting();
    d->desiredPs3 = d->model && d->model->pureSignalFacade()
        && d->model->pureSignalFacade()->ampViewSubscribed();
    d->accountedPs3 = d->client->remotePs3DisplaySubscribed();
    d->connectionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* peer = new MediaPeer(this, d->factory);
    d->peer = peer;
    emit networkPathChanged();
    const quint32 epoch = d->epoch;
    d->displayDropsReported = 0;
    d->displayDropsReportedAtMs = 0;
    connectPeer(peer, epoch);
    d->startingPeer = true;
    d->startRefusal.clear();
    const QPointer<MediaPeer> startedPeer(peer);
    // R-R3-43: the receiver stream ids are declared only when this GUI will
    // say so in its start below.
    // R-R3-45: likewise the headphones mix's stream id.
    // Task 36: likewise the microphone line.
    const bool micLine = micLineNegotiated();
    d->iqNegotiated = remoteIqNegotiated();
    // iPhone app plan Task 28 (R-IOS-16): a session through the remote
    // access service makes its media connection with the same ICE settings
    // as its control connection. Task 29 step 2b: a direct WebSocket
    // session to a Core that carries the media tunnel also offers the
    // tunnel (ICE takes it only when no UDP pair works).
    d->tunnelNegotiated = d->client->mediaTunnelAvailable();
    d->relayRoutingNegotiated = d->client->capabilities().mediaRelayRoutingVersion >= 1;
    // The window pings faster once media rides the tunnel (step 2b; the
    // stall check sets it when ICE has settled on the tunnel's pair).
    d->client->setMediaTunnelInUse(false);
    const auto startIce = mediaIceConfiguration();
    // The direct media ladder: a new media start begins its direct-only
    // schedule at the first step.
    d->currentRouted = startIce && startIce->mediaRouting();
    d->replacementRouted = false;
    d->replacementDirect = false;
    d->directUpgrade->stop();
    d->directUpgradeStep = 0;
    d->lastMediaMs = -1;
    d->silenceFallbackFired = false;
    d->replacementFallback = false;
    d->fallbackFinishedMs = -1;
    peer->setIceConfiguration(startIce);
    const bool started = peer->start(IMediaTransport::Role::Answerer, d->connectionId,
                                     IMediaTransport::kDefaultAudioTargetBitrate,
                                     /*offerLosslessAudio=*/false, receiverAudioNegotiated(),
                                     headphonesMixNegotiated(), micLine, d->iqNegotiated);
    if (!self) { return; }
    d->startingPeer = false;
    d->receiverSsrcs = started && startedPeer ? startedPeer->receiverAudioSsrcs() : QList<quint32>{};
    d->headphonesSsrc = started && startedPeer ? startedPeer->headphonesAudioSsrc() : 0;
    // Task 29: the streams' own SSRCs, which a replacement's map onto.
    d->logicalSsrcs = started && startedPeer ? audioSsrcsOf(startedPeer) : QList<quint32>{};
    const QString startError = std::exchange(d->startRefusal, QString());
    if (!started) {
        // R-R3-28, amended 2026-09-23: a refusal is never a silent stop, and
        // only a transport that could not be built is retried, through the
        // same path as any media failure, until retries reach the backoff
        // ceiling. Every other refusal is permanent, as is a transient one
        // past the ceiling: stop, show the reason, keep control.
        const QString base = QStringLiteral("Station media could not start on this computer");
        const QString reason = startError.isEmpty()
            ? base : QStringLiteral("%1: %2").arg(base, startError);
        const bool transient = startedPeer
            && startedPeer->lastStartRefusal()
                == MediaPeer::StartRefusal::TransportConstructionFailed;
        if (transient && !d->client->reconnectBackoffExhausted()) {
            requestRecovery(epoch, reason);
            return;
        }
        if (transient) {
            qCWarning(lcRemoteMedia).noquote()
                << QStringLiteral("%1; automatic retries stopped at the longest wait,"
                                  " the connection stays up without media").arg(reason);
        }
        settleWithoutRetry(epoch, reason);
        return;
    }
    if (!startError.isEmpty()) {
        // Started, but reported an error on the way: handled exactly as an
        // error after start always has been.
        settleWithoutRetry(epoch, startError);
        return;
    }
    // Stage one: Core's description (receiveControl() starts stage two).
    d->awaitingDescription = true;
    d->establishTimer->start(d->descriptionDeadlineMs);
    // R-R3-23: a Core that can send lossless audio offers it only to a GUI
    // that says it understands it; every other Core sees today's start.
    QJsonObject startControl{{QStringLiteral("op"), QStringLiteral("start")}};
    if (audioProfileNegotiated()) {
        startControl.insert(QStringLiteral("audioProfileVersion"), 1);
    }
    // R-R3-43: likewise receiver audio, only to a Core that offers it.
    if (receiverAudioNegotiated()) {
        startControl.insert(QStringLiteral("receiverAudioVersion"), 1);
    }
    if (d->iqNegotiated) {
        startControl.insert(QStringLiteral("remoteIqVersion"), 1);
    }
    // R-R3-45: likewise the headphones mix, only to a Core that offers it.
    if (headphonesMixNegotiated()) {
        startControl.insert(QStringLiteral("headphonesMixVersion"), 1);
    }
    // Parity Task 28 (R-R3-49, A11): likewise the transmit display, only to
    // a Core that sends it.
    d->txDisplayNegotiated = d->client && d->client->capabilities().txDisplayVersion >= 1;
    // Parity Task 31: a Core at 3 is told 3, so its subscribes may carry
    // `duplex`; any other declares 1, as before.
    d->displayDuplexNegotiated = d->txDisplayNegotiated
        && d->client->capabilities().txDisplayVersion >= 3;
    if (d->txDisplayNegotiated) {
        startControl.insert(QStringLiteral("txDisplayVersion"),
                            d->displayDuplexNegotiated ? 3 : 1);
    }
    if (d->client && d->client->capabilities().miniDisplayVersion >= 1) {
        startControl.insert(QStringLiteral("miniDisplayVersion"), 1);
    }
    // Parity Task 32 (R-IOS-13, R-R3-49): likewise the transmit monitor,
    // only to a Core that sends it.
    d->txMonitorNegotiated = d->client && d->client->capabilities().txMonitorAudioVersion >= 1;
    if (d->txMonitorNegotiated) {
        startControl.insert(QStringLiteral("txMonitorAudioVersion"), 1);
    }
    // Declare tunnel support even if this session currently uses the
    // service data channel; a later direct-wss move may need it.
    if (d->tunnelNegotiated) {
        startControl.insert(QStringLiteral("mediaTunnelVersion"), 1);
    }
    if (d->relayRoutingNegotiated) {
        startControl.insert(QStringLiteral("mediaRelayRoutingVersion"), 1);
    }
    // Task 36: likewise the microphone line, only to a Core that takes it.
    if (micLine) {
        startControl.insert(QStringLiteral("remoteTxVersion"), 1);
        d->micSequence = 0;
        d->micTimestamp = 0;
        d->micEncoder->reset();
        d->micTimer->start();
    }
    send(startControl);
    if (!self) { return; }
    // R-R3-45: a Core that cannot send the headphones mix says so on every
    // flag routed to the headphones.
    if (!headphonesMixNegotiated()) {
        setHeadphonesProblem(QString::fromLatin1(kHeadphonesMixUnavailableReason));
    } else if (!d->headphonesFaulted) {
        setHeadphonesProblem(QString());
    }
    if (!self) { return; }
    if (!receiverAudioNegotiated()) {
        // An older Core: no request goes out, and each consumer is told why.
        QList<int> wanted;
        for (const auto& [sliceId, stream] : d->receiverStreams) { wanted.append(sliceId); }
        for (int sliceId : wanted) {
            notifyReceiverStopped(sliceId, QString::fromLatin1(kReceiverAudioUnavailableReason));
            if (!self) { return; }
        }
    }
    // A media session now exists: audio is awaited from Core.
    refreshAudioStatus();
}

bool RemoteMediaController::send(QJsonObject payload)
{
    if (!d->client || d->connectionId.isEmpty()) { return false; }
    payload.insert(QStringLiteral("connectionId"), d->connectionId);
    return d->client->sendMediaControl(payload, d->epoch);
}

bool RemoteMediaController::sendRefusedRelease(quint32 endpointId, quint32 lastRevision)
{
    // The binding is already gone here: its answer finds no binding and is
    // ignored. False when the session ended while sending.
    const QPointer<RemoteMediaController> self(this);
    const QPointer<MediaPeer> peer = d->peer;
    const quint32 epoch = d->epoch;
    const QString connectionId = d->connectionId;
    quint32 revision = lastRevision + 1;
    if (revision == 0) { ++revision; }
    send({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
          {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
          {QStringLiteral("revision"), static_cast<qint64>(revision)}});
    return self && d->peer == peer && d->epoch == epoch && d->connectionId == connectionId;
}

bool RemoteMediaController::retireSubscriptions(const QList<quint32>& endpointIds,
                                                const QSet<SpectrumWidget*>& keepHistory)
{
    const bool budgetMode = d->client && d->client->remoteDisplayBudgetLimits().has_value();
    const QPointer<RemoteMediaController> self(this);
    const QPointer<MediaPeer> peer = d->peer;
    const quint32 epoch = d->epoch;
    const QString connectionId = d->connectionId;
    for (quint32 id : endpointIds) {
        auto found = d->bindings.find(id);
        if (found == d->bindings.end()) { continue; }
        if (found->second.isMini()) {
            emit miniDisplayUnavailable(found->second.miniSliceId);
        }
        if (budgetMode) {
            QPointer<SpectrumWidget> widget = found->second.widget;
            if (widget) {
                widget->traceRxHistoryEvent(keepHistory.contains(widget.data())
                    ? SpectrumWidget::RxHistoryEvent::BindingPreserved
                    : SpectrumWidget::RxHistoryEvent::BudgetBindingRetired,
                    found->second.observedStream, found->second.observedStreamEpoch);
            }
            QObject::disconnect(found->second.ctunGesture);
            QObject::disconnect(found->second.centreGesture);
            found->second.ctunGesture = {};
            found->second.centreGesture = {};
            found->second.widget = nullptr;
            found->second.slice = nullptr;
            found->second.retiring = true;
            found->second.suspending = false;
            const QString panId = found->second.panId;
            if (widget) {
                // Parity Task 18 (B3.5): another slice on the same receiver
                // takes this pan's display; what it has drawn stays.
                if (keepHistory.contains(widget.data())) {
                    widget->invalidateRemoteSpectrumFrame();
                } else {
                    widget->clearRemoteSpectrum();
                }
                if (!self || !widget) { return false; }
                widget->applyRemoteCtunState(false, false);
                if (!self) { return false; }
            }
            refreshPanGrantStatus(panId);
            if (!self) { return false; }
            found = d->bindings.find(id);
            if (found == d->bindings.end()) { continue; }
            if (found->second.pending) {
                continue;
            }
            if (found->second.acceptedRevision == 0) {
                // Fix wave 2 (Important 2): a display the Core refused
                // still counts in this device's request there until it is
                // asked for again or closed, so close it.
                const quint32 sent = found->second.revision;
                d->bindings.erase(found);
                if (sent != 0 && !sendRefusedRelease(id, sent)) { return false; }
                continue;
            }
            ++found->second.revision;
            if (found->second.revision == 0) { ++found->second.revision; }
            const quint32 revision = found->second.revision;
            found->second.pending = Private::Binding::Pending{
                Private::Binding::PendingKind::Unsubscribe, revision, {}, {},
                QStringLiteral("retire"), d->allocationClock(), false};
            send({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
                  {QStringLiteral("endpointId"), static_cast<qint64>(id)},
                  {QStringLiteral("revision"), static_cast<qint64>(revision)}});
            if (!self || d->peer != peer || d->epoch != epoch
                || d->connectionId != connectionId) { return false; }
            continue;
        }
        QPointer<SpectrumWidget> widget = found->second.widget;
        const QString panId = found->second.panId;
        if (widget) {
            widget->traceRxHistoryEvent(keepHistory.contains(widget.data())
                ? SpectrumWidget::RxHistoryEvent::BindingPreserved
                : SpectrumWidget::RxHistoryEvent::LegacyBindingRetired,
                found->second.observedStream, found->second.observedStreamEpoch);
        }
        // Retire local ownership before sending: a synchronous transport
        // failure can end the session and clear every binding inside send().
        d->bindings.erase(found);
        if (widget) {
            // Parity Task 18 (B3.5): as above.
            if (keepHistory.contains(widget.data())) {
                widget->invalidateRemoteSpectrumFrame();
            } else {
                widget->clearRemoteSpectrum();
            }
            if (!self || !widget) { return false; }
            widget->applyRemoteCtunState(false, false);
            if (!self) { return false; }
        }
        refreshPanGrantStatus(panId);
        if (!self) { return false; }
        send({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
              {QStringLiteral("endpointId"), double(id)}});
        if (!self || d->peer != peer || d->epoch != epoch
            || d->connectionId != connectionId) { return false; }
    }
    return true;
}

void RemoteMediaController::refreshSubscriptions()
{
    if (!d->client || !d->client->mediaAvailable() || d->epoch != d->client->sessionEpoch()
        || !d->peer || !d->peer->isReady() || !d->model || !d->stack) {
        return;
    }
    if (!d->model->isConnected()) { return; }
    d->settleDbmWindows(d->allocationClock());
    if (d->client->remoteDisplayBudgetLimits()) {
        refreshBudgetSubscriptions();
        return;
    }
    for (PanadapterApplet* applet : d->stack->allApplets()) {
        if (applet) {
            // This supported subscription mode does not indicate a display
            // failure or require operator action. Keep capability details in
            // diagnostics, and clear any status from a previous allocation.
            // Only a limited grant or a refusal is shown.
            if (applet->panId().isEmpty()) {
                applet->setRemoteDisplayStatus(PanStatusText{});
            } else {
                setPanStatus(applet->panId(), perPanRefusalStatus(applet->panId()));
            }
        }
    }
    const QPointer<RemoteMediaController> self(this);
    const QPointer<MediaPeer> peer = d->peer;
    const quint32 epoch = d->epoch;
    const QString connectionId = d->connectionId;
    const auto current = [this, self, peer, epoch, connectionId] {
        return self && d->peer == peer && d->epoch == epoch
            && d->connectionId == connectionId && d->stack && d->model;
    };
    struct Desired {
        QString panId;
        QPointer<SpectrumWidget> widget;
        QPointer<SliceModel> slice;
        QJsonObject request;
    };
    QList<Desired> desired;
    for (PanadapterApplet* applet : d->stack->allApplets()) {
        SpectrumWidget* widget = applet->spectrumWidget();
        SliceModel* slice = d->model->sliceById(applet->activeSliceIndex());
        // The stack's membership is pane intent. Float/dock and layout
        // rebuilding temporarily hide the same renderer without disabling it.
        if (!widget || !slice || slice->streamIndex() < 0) { continue; }
        QJsonObject request = d->request(widget, slice);
        if (!request.isEmpty()) {
            desired.append({applet->panId(), widget, slice, std::move(request)});
        }
    }
    // Retire obsolete bindings before creating any replacement. In particular,
    // a global FFT-window change must release every old shared-source window
    // first; updating them individually would reject each against its peers.
    // The reliable control stream preserves all unsubscriptions before adds.
    QHash<SpectrumWidget*, double> retainedSourceCentres;
    // Parity Task 18 (B3.5): pans whose display moves to another slice on
    // the same receiver keep what they have drawn.
    QSet<SpectrumWidget*> keepHistory;
    QList<quint32> retiredEndpoints;
    for (auto it = d->bindings.begin(); it != d->bindings.end(); ++it) {
        if (it->second.isMini()) { continue; }
        const auto next = std::find_if(desired.cbegin(), desired.cend(),
            [&it](const Desired& item) {
                return item.widget == it->second.widget && item.slice == it->second.slice;
            });
        const bool windowChanged = next != desired.cend()
            && next->request.value(QStringLiteral("windowType"))
                != it->second.observed.value(QStringLiteral("windowType"));
        if (next != desired.cend() && !windowChanged) { continue; }
        // A cohost selection changes the endpoint's slice identity, not its
        // physical receive window. Retain that accepted geometry for an
        // immediate rejected gesture while the replacement awaits its FFT.
        for (const Desired& item : desired) {
            if (item.widget == it->second.widget && it->second.sourceCentreHz > 0
                && item.slice->streamIndex() == it->second.observedStream
                && item.slice->streamEpoch() == it->second.observedStreamEpoch) {
                retainedSourceCentres.insert(item.widget, it->second.sourceCentreHz);
            }
            if (!windowChanged && item.widget && item.widget == it->second.widget
                && item.slice != it->second.slice
                && item.slice->streamIndex() == it->second.observedStream
                && item.slice->streamEpoch() == it->second.observedStreamEpoch) {
                keepHistory.insert(item.widget.data());
            }
        }
        retiredEndpoints.append(it->first);
    }
    if (!retireSubscriptions(retiredEndpoints, keepHistory) || !current()) { return; }
    for (const Desired& item : desired) {
        SpectrumWidget* widget = item.widget;
        SliceModel* slice = item.slice;
        if (!widget || !slice || currentSliceForPan(d->model, d->stack, widget) != slice) {
            continue;
        }
        auto found = std::find_if(d->bindings.begin(), d->bindings.end(),
            [widget, slice](const auto& entry) {
                return !entry.second.isMini() && entry.second.widget == widget
                    && entry.second.slice == slice;
            });
        if (found == d->bindings.end()) {
            if (d->bindings.size() >= kMaxEndpoints || d->nextEndpoint == 0) { continue; }
            const quint32 id = d->nextEndpoint++;
            found = d->bindings.try_emplace(id).first;
            found->second.panId = item.panId;
            found->second.widget = widget;
            found->second.slice = slice;
            found->second.sourceCentreHz = retainedSourceCentres.value(widget, 0);
            auto& binding = found->second;
            // Per-pan sender and gesture-time slice lookup: selection may
            // have changed since the last subscription poll. Never capture
            // its former slice as the command target.
            SpectrumWidget* const sw = item.widget;
            binding.ctunGesture = connect(sw, &SpectrumWidget::ctunEnabledChanged,
                this, [this, id](bool pinned) {
                    auto current = d->bindings.find(id);
                    if (current == d->bindings.end() || !current->second.slice
                        || !d->model
                        || !d->client || !d->client->remoteCtunAvailable()) { return; }
                    SliceModel* slice = currentSliceForPan(
                        d->model, d->stack, current->second.widget);
                    if (!slice || slice->streamIndex() < 0) { return; }
                    auto& state = d->ctunStreams[slice->streamIndex()];
                    state.epoch = slice->streamEpoch();
                    state.requestSliceId = slice->sliceIndex();
                    state.requestedPin = pinned;
                    state.pending = true;
                    state.initialized = false;
                    state.rejectedContext = 0;
                    state.refused = false;
                    if (!d->model->requestStreamCtunPinned(slice->sliceIndex(), pinned)) {
                        state.pending = false;
                        state.rejectedContext = current->second.context.codec.contextGeneration;
                    }
                });
            binding.centreGesture = connect(sw, &SpectrumWidget::centerChanged,
                this, [this, id](double centreHz) {
                    auto current = d->bindings.find(id);
                    if (current == d->bindings.end() || !current->second.slice
                        || !current->second.widget
                        || !current->second.widget->ctunEnabled() || !d->model
                        || !d->client || !d->client->remoteCtunAvailable()) { return; }
                    requestCentreFromGesture(id, centreHz);
            });
            widget->traceRxHistoryEvent(keepHistory.contains(widget)
                ? SpectrumWidget::RxHistoryEvent::BindingPreserved
                : SpectrumWidget::RxHistoryEvent::LegacyBindingCreated,
                slice->streamIndex(), slice->streamEpoch());
            // Parity Task 18 (B3.5): a pan taking another slice on the same
            // receiver keeps what it has drawn.
            if (keepHistory.contains(widget)) {
                widget->invalidateRemoteSpectrumFrame();
            } else {
                widget->clearRemoteSpectrum();
            }
            if (!current() || !widget) { return; }
            widget->applyRemoteCtunState(false, false);
            if (!current()) { return; }
        }
        const quint32 id = found->first;
        auto& binding = found->second;
        QJsonObject request = item.request;
        // observed is the last attempted geometry, not proof of acceptance.
        // A rejected request remains blank until its inputs change; reconnect
        // retires the binding, and source-window changes are batched above.
        if (request == binding.observed
            && binding.observedStream == slice->streamIndex()
            && binding.observedStreamEpoch == slice->streamEpoch()) { continue; }
        binding.observed = request;
        binding.observedStream = slice->streamIndex();
        binding.observedStreamEpoch = slice->streamEpoch();
        ++binding.revision;
        if (binding.revision == 0) { ++binding.revision; }
        binding.accepted = false;
        binding.rejected = false;
        binding.refusalReason.clear();
        binding.decoder.reset();
        binding.presenter.restartChain();
        // A tune/zoom renews this binding's codec, not its painted history.
        // New/replaced bindings were fully cleared above; rejection and
        // session retirement still clear them through their lifecycle paths.
        widget->invalidateRemoteSpectrumFrame();
        if (!current()) { return; }
        request.insert(QStringLiteral("op"), QStringLiteral("subscribe"));
        request.insert(QStringLiteral("endpointId"), double(id));
        const quint32 revision = binding.revision;
        request.insert(QStringLiteral("revision"), double(revision));
        const bool sent = send(request);
        if (!current()) { return; }
        // An observer can retire or renew this endpoint during the send.
        // Never retain a Binding reference across that callback boundary.
        found = d->bindings.find(id);
        if (!sent && found != d->bindings.end() && found->second.revision == revision) {
            found->second.observed = {};
        }
    }
    refreshLegacyMiniSubscriptions();
    refreshCtunState();
}

void RemoteMediaController::refreshLegacyMiniSubscriptions()
{
    if (!d->model || !d->client || !d->peer || !d->peer->isReady()) { return; }
    QList<quint32> retired;
    for (const auto& [endpointId, binding] : d->bindings) {
        if (!binding.isMini()) { continue; }
        SliceModel* current = d->model->sliceById(binding.miniSliceId);
        if (!d->miniWanted.contains(binding.miniSliceId) || !current
            || current != binding.slice || d->requestMini(current).isEmpty()) {
            emit miniDisplayUnavailable(binding.miniSliceId);
            retired.append(endpointId);
        }
    }
    if (!retired.isEmpty() && !retireSubscriptions(retired)) { return; }
    QList<int> ids = d->miniWanted.values();
    std::sort(ids.begin(), ids.end());
    for (int sliceId : ids) {
        SliceModel* slice = d->model->sliceById(sliceId);
        const QJsonObject desired = d->requestMini(slice);
        if (desired.isEmpty()) { emit miniDisplayUnavailable(sliceId); continue; }
        auto found = std::find_if(d->bindings.begin(), d->bindings.end(),
            [sliceId](const auto& entry) {
                return entry.second.miniSliceId == sliceId && !entry.second.retiring;
            });
        if (found == d->bindings.end()) {
            if (d->bindings.size() >= kMaxEndpoints || d->nextEndpoint == 0) {
                emit miniDisplayUnavailable(sliceId);
                continue;
            }
            const quint32 endpointId = d->nextEndpoint++;
            found = d->bindings.try_emplace(endpointId).first;
            found->second.panId = QStringLiteral("mini:%1").arg(sliceId);
            found->second.miniSliceId = sliceId;
            found->second.slice = slice;
        }
        auto& binding = found->second;
        if (binding.observed == desired && binding.observedStream == slice->streamIndex()
            && binding.observedStreamEpoch == slice->streamEpoch()) { continue; }
        emit miniDisplayUnavailable(sliceId);
        binding.observed = desired;
        binding.observedStream = slice->streamIndex();
        binding.observedStreamEpoch = slice->streamEpoch();
        ++binding.revision;
        if (binding.revision == 0) { ++binding.revision; }
        binding.accepted = false;
        binding.rejected = false;
        binding.decoder.reset();
        binding.presenter.reset();
        QJsonObject wire = desired;
        wire.insert(QStringLiteral("op"), QStringLiteral("subscribe"));
        wire.insert(QStringLiteral("endpointId"), double(found->first));
        wire.insert(QStringLiteral("revision"), double(binding.revision));
        send(wire);
    }
}

void RemoteMediaController::setPanStatus(const QString& panId, const PanDisplayState& status)
{
    if (!d->stack || panId.isEmpty()) { return; }
    d->panBaseStatus.insert(panId, status);
    for (PanadapterApplet* applet : d->stack->allApplets()) {
        if (applet && applet->panId() == panId) {
            applet->setRemoteDisplayStatus(buildPanStatusText(panDisplayState(panId)));
            return;
        }
    }
}

PanDisplayState RemoteMediaController::panDisplayState(const QString& panId) const
{
    PanDisplayState state = statusWithGrant(panId, d->panBaseStatus.value(panId));
    // Parity Task 29: while keyed on a Core that sends no transmit display.
    state.transmitDisplayMissing = d->transmitDisplayMissingPans.contains(panId);
    return state;
}

void RemoteMediaController::refreshPanGrantStatus(const QString& panId)
{
    setPanStatus(panId, d->panBaseStatus.value(panId));
}

PanDisplayState RemoteMediaController::perPanRefusalStatus(const QString& panId) const
{
    // R-R3-01/08/37: outside budget mode a refused pan keeps the budget-mode
    // line while the refused request is still the one it would send. A new
    // request clears the reason when it goes out.
    if (!d->model || !d->stack || !d->client) { return {}; }
    for (const auto& [id, binding] : d->bindings) {
        Q_UNUSED(id);
        if (binding.panId != panId || !binding.rejected || binding.refusalReason.isEmpty()
            || !binding.widget || !binding.slice
            || currentSliceForPan(d->model, d->stack, binding.widget) != binding.slice
            || binding.observedStream != binding.slice->streamIndex()
            || binding.observedStreamEpoch != binding.slice->streamEpoch()
            || d->request(binding.widget, binding.slice) != binding.observed) {
            continue;
        }
        return refusedState(binding.refusalReason.left(384));
    }
    return {};
}

PanDisplayState RemoteMediaController::statusWithGrant(const QString& panId,
                                                      PanDisplayState status) const
{
    // Only a live, accepted endpoint's grant is shown: the limit goes when
    // the grant is no longer limited or the endpoint does.
    status.zoomLimit = PanDisplayState::ZoomLimit::None;
    status.zoomPoints = 0;
    for (const auto& [id, binding] : d->bindings) {
        if (binding.panId == panId && binding.accepted && !binding.retiring
            && !binding.suspending && applyGrantLimit(status, binding.grant)) {
            break;
        }
    }
    return status;
}

void RemoteMediaController::refreshBudgetSubscriptions()
{
    if (d->refreshingBudget) {
        d->budgetReplanRequested = true;
        return;
    }
    const QPointer<RemoteMediaController> self(this);
    d->refreshingBudget = true;
    d->budgetReplanRequested = false;
    const auto refreshGuard = qScopeGuard([this, self] {
        if (!self) { return; }
        d->refreshingBudget = false;
        if (d->budgetReplanRequested) {
            QTimer::singleShot(0, self, &RemoteMediaController::refreshSubscriptions);
        }
    });

    if (!d->client || !d->model || !d->stack || !d->peer || !d->peer->isReady()) {
        return;
    }
    const auto limits = d->client->remoteDisplayBudgetLimits();
    if (!limits) { return; }
    const qint64 now = d->allocationClock();

    QList<quint32> overdueRetirements;
    for (auto& [id, binding] : d->bindings) {
        if (binding.pending && !binding.pending->timedOut
            && now - binding.pending->sentAtMs >= d->allocationAckTimeoutMs) {
            binding.pending->timedOut = true;
            qCWarning(lcRemoteMedia) << "Remote display allocation acknowledgement stalled"
                                    << id << binding.pending->revision;
        }
        if (binding.retiring && binding.pending && binding.pending->timedOut
            && binding.pending->kind == Private::Binding::PendingKind::Subscribe) {
            overdueRetirements.append(id);
        }
    }
    const QPointer<MediaPeer> retirementPeer = d->peer;
    const quint32 retirementEpoch = d->epoch;
    const QString retirementConnectionId = d->connectionId;
    for (quint32 endpointId : overdueRetirements) {
        auto found = d->bindings.find(endpointId);
        if (found == d->bindings.end()) { continue; }
        Private::Binding& binding = found->second;
        if (!binding.retiring || !binding.pending || !binding.pending->timedOut
            || binding.pending->kind != Private::Binding::PendingKind::Subscribe) {
            continue;
        }
        // A delayed reply from the old media peer cannot reconcile a closed
        // display after promotion. Its newer explicit release can. Keep the
        // largest possible reservation until Core confirms zero charge;
        // the pending barrier below still blocks every display increase.
        const DisplayBudgetCharge uncertainCharge =
            maximumCharge(binding.acceptedCharge, binding.pending->charge);
        ++binding.revision;
        if (binding.revision == 0) { ++binding.revision; }
        const quint32 revision = binding.revision;
        binding.pending = Private::Binding::Pending{
            Private::Binding::PendingKind::Unsubscribe, revision, {}, uncertainCharge,
            QStringLiteral("retire"), now, false};
        send({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
              {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
              {QStringLiteral("revision"), static_cast<qint64>(revision)}});
        if (!self || d->peer != retirementPeer || d->epoch != retirementEpoch
            || d->connectionId != retirementConnectionId || !d->model || !d->stack) {
            return;
        }
    }
    if (d->pendingPs3 && !d->pendingPs3->timedOut
        && now - d->pendingPs3->sentAtMs >= d->allocationAckTimeoutMs) {
        d->pendingPs3->timedOut = true;
        qCWarning(lcRemoteMedia) << "Remote PureSignal display acknowledgement stalled"
                                << d->pendingPs3->commandId;
    }
    if (!d->desiredPs3 && d->accountedPs3 && !d->pendingPs3) {
        const QPointer<StationClient> requestClient = d->client;
        const quint32 requestEpoch = d->epoch;
        const quint32 commandId = requestClient->requestPs3DisplaySubscription(false);
        if (!self || d->client != requestClient || d->epoch != requestEpoch) { return; }
        if (commandId == 0 && !d->pendingPs3) {
            d->ps3RefusalReason = QStringLiteral("Unable to request PureSignal display release.");
        }
        return;
    }

    struct Desired {
        QString panId;
        QPointer<SpectrumWidget> widget;
        QPointer<SliceModel> slice;
        QJsonObject original;
        RemoteDisplayIntent intent;
    };
    QList<Desired> desired;
    for (PanadapterApplet* applet : d->stack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* widget = applet->spectrumWidget();
        SliceModel* slice = d->model->sliceById(applet->activeSliceIndex());
        if (!widget || !slice || slice->streamIndex() < 0) { continue; }
        QJsonObject original = d->request(widget, slice);
        if (original.isEmpty()) { continue; }
        RemoteDisplayIntent intent;
        intent.panId = applet->panId();
        intent.pixels = original.value(QStringLiteral("pixels")).toInt();
        intent.fps = original.value(QStringLiteral("fps")).toInt();
        intent.includeWidePlane = original.value(QStringLiteral("wideSpanFactor")).toDouble() > 1.0;
        // JJ's ruling of 2026-09-28: the Core's waterfall levels are
        // charged too.
        intent.extrasSections = original.contains(QStringLiteral("waterfallLevels"))
            ? kDisplayExtrasWaterfallLevels : 0;
        intent.waterfallPeriodMs = qBound(1, widget->wfUpdatePeriodMs(), 65'535);
        intent.active = intent.panId == d->stack->activePanId();
        desired.append({intent.panId, widget, slice, std::move(original), intent});
    }
    QList<int> miniIds = d->miniWanted.values();
    std::sort(miniIds.begin(), miniIds.end());
    for (int sliceId : miniIds) {
        SliceModel* slice = d->model->sliceById(sliceId);
        QJsonObject original = d->requestMini(slice);
        if (original.isEmpty()) { emit miniDisplayUnavailable(sliceId); continue; }
        RemoteDisplayIntent intent;
        intent.panId = QStringLiteral("mini:%1").arg(sliceId);
        intent.pixels = original.value(QStringLiteral("pixels")).toInt();
        intent.fps = original.value(QStringLiteral("fps")).toInt();
        intent.waterfallPeriodMs = 1000 / 30;
        intent.kind = RemoteDisplayIntent::Kind::Mini;
        desired.append({intent.panId, nullptr, slice, std::move(original), intent});
    }

    // Refuse an impossible PS3 enable before any pan operation can be
    // attributed to that request. Existing pan allocations remain intact.
    if (d->desiredPs3 && !d->accountedPs3 && !d->pendingPs3 && !d->ps3Refused) {
        QString reason;
        if (!allocateRemoteDisplay(*limits, {}, true, &reason)) {
            d->ps3Refused = true;
            d->ps3RefusalReason = reason;
            qCInfo(lcRemoteMedia).noquote() << "Remote PureSignal display does not fit:"
                                            << reason;
            d->ps3RefusedGeneration = limits->generation;
        }
    }

    QList<quint32> obsolete;
    // Parity Task 18 (B3.5): pans whose display moves to another slice on
    // the same receiver keep what they have drawn.
    QSet<SpectrumWidget*> keepHistory;
    for (const auto& [id, binding] : d->bindings) {
        if (binding.retiring || binding.suspending) { continue; }
        const auto item = std::find_if(desired.cbegin(), desired.cend(),
            [&binding](const Desired& candidate) {
                return candidate.panId == binding.panId
                    && candidate.widget == binding.widget
                    && candidate.slice == binding.slice;
            });
        if (item == desired.cend()) {
            obsolete.append(id);
            for (const Desired& candidate : desired) {
                if (candidate.widget && candidate.widget == binding.widget
                    && candidate.slice != binding.slice
                    && candidate.slice->streamIndex() == binding.observedStream
                    && candidate.slice->streamEpoch() == binding.observedStreamEpoch) {
                    keepHistory.insert(candidate.widget.data());
                }
            }
        }
    }
    if (!obsolete.isEmpty()) {
        retireSubscriptions(obsolete, keepHistory);
        return;
    }

    QList<quint32> incompatibleWindows;
    for (const auto& [id, binding] : d->bindings) {
        if (binding.retiring || binding.suspending || binding.acceptedRevision == 0
            || binding.acceptedRequest.isEmpty()) { continue; }
        const auto item = std::find_if(desired.cbegin(), desired.cend(),
            [&binding](const Desired& candidate) {
                return candidate.panId == binding.panId
                    && candidate.widget == binding.widget
                    && candidate.slice == binding.slice;
            });
        if (item != desired.cend()
            && item->original.value(QStringLiteral("windowType"))
                != binding.acceptedRequest.value(QStringLiteral("windowType"))) {
            incompatibleWindows.append(id);
        }
    }
    if (!incompatibleWindows.isEmpty()) {
        const QPointer<MediaPeer> peer = d->peer;
        const quint32 epoch = d->epoch;
        const QString connectionId = d->connectionId;
        for (quint32 endpointId : incompatibleWindows) {
            auto found = d->bindings.find(endpointId);
            if (found == d->bindings.end()) { continue; }
            Private::Binding& binding = found->second;
            binding.suspending = true; // Preserve the widget/history across the release.
            setPanStatus(binding.panId, phaseState(PanDisplayState::Phase::ChangingWindow));
            if (!self) { return; }
            if (binding.pending) { continue; }
            ++binding.revision;
            if (binding.revision == 0) { ++binding.revision; }
            const quint32 revision = binding.revision;
            binding.pending = Private::Binding::Pending{
                Private::Binding::PendingKind::Unsubscribe, revision, {}, {},
                QStringLiteral("source-window:%1").arg(limits->generation),
                now, false};
            send({{QStringLiteral("op"), QStringLiteral("unsubscribe")},
                  {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
                  {QStringLiteral("revision"), static_cast<qint64>(revision)}});
            if (!self || d->peer != peer || d->epoch != epoch
                || d->connectionId != connectionId) { return; }
        }
        return;
    }

    QList<RemoteDisplayIntent> intents;
    intents.reserve(desired.size());
    for (const Desired& item : desired) { intents.append(item.intent); }

    const bool targetPs3 = d->accountedPs3
        || (d->desiredPs3 && !d->ps3Refused);
    const bool retainedPs3ExceedsCap = d->accountedPs3
        && !displayChargeFits(*limits, ps3DisplayCharge());
    // Fix wave 2 (Critical 1, ruling 9.3): ask for what the operator wants
    // whenever that grows. The Core counts what a device asks for when it
    // splits the budget, so a planner that only ever asked for what its
    // share allows would never show its demand, and a device that joined
    // second would stay at the share it was first given.
    {
        QList<DisplayBudgetCharge> wantedCharges;
        for (const RemoteDisplayIntent& intent : intents) {
            if (const auto cost = displayCostWithExtras(intent.pixels, intent.fps,
                                                        intent.includeWidePlane,
                                                        intent.extrasSections)) {
                wantedCharges.append(cost->charge);
            }
        }
        const DisplayBudgetCharge wanted =
            sumDisplayCharges(wantedCharges).value_or(DisplayBudgetCharge{});
        // Fix wave 3 (Minor 3): a resize (the same pans at the same frame
        // rates, only their widths changed) asks once its widths have
        // settled for kResizeSettleMs, not at every step of a drag; any
        // other growth asks at once.
        bool sameDisplays = !d->wantedIntents.isEmpty()
            && d->wantedIntents.size() == intents.size();
        bool widthsMoved = false;
        for (qsizetype i = 0; sameDisplays && i < intents.size(); ++i) {
            const RemoteDisplayIntent& was = d->wantedIntents.at(i);
            const RemoteDisplayIntent& is = intents.at(i);
            sameDisplays = was.panId == is.panId && was.fps == is.fps
                && was.includeWidePlane == is.includeWidePlane
                && was.extrasSections == is.extrasSections
                && was.kind == is.kind;
            widthsMoved = widthsMoved || was.pixels != is.pixels;
        }
        if (d->resizeAsk && !sameDisplays) {
            // The displays changed shape mid-resize: the resize's growth is
            // asked for now, with whatever else changed.
            if (!nonIncreasing(wanted, d->resizeAsk->base)) {
                d->askingWanted = true;
            }
            d->resizeAsk.reset();
        } else if (d->resizeAsk && widthsMoved) {
            d->resizeAsk->movedAtMs = now;
        }
        if (!nonIncreasing(wanted, d->wantedCharge)) {
            if (sameDisplays) {
                if (!d->resizeAsk) {
                    d->resizeAsk = Private::ResizeAsk{d->wantedCharge, now};
                }
            } else {
                d->askingWanted = true;
            }
        }
        if (d->resizeAsk && now - d->resizeAsk->movedAtMs >= kResizeSettleMs) {
            if (!nonIncreasing(wanted, d->resizeAsk->base)) {
                d->askingWanted = true;
            }
            d->resizeAsk.reset();
        }
        d->wantedCharge = wanted;
        d->wantedIntents = intents;
    }
    const bool askWanted = d->askingWanted && !retainedPs3ExceedsCap
        && d->iqBytesPerSecond == 0;
    // While asking, plan without the share: every pan at its wanted quality.
    // The Core admits or refuses each against the share it gives this
    // device with the new request.
    DisplayBudgetLimits planLimits = askWanted
        ? DisplayBudgetLimits{kDisplayBudgetJsonSafePositiveLimit,
                              kDisplayBudgetJsonSafePositiveLimit, limits->generation}
        : *limits;
    if (d->iqBytesPerSecond != 0) {
        planLimits.applicationBytesPerSecond =
            planLimits.applicationBytesPerSecond > d->iqBytesPerSecond
                ? planLimits.applicationBytesPerSecond - d->iqBytesPerSecond : 1;
    }
    const QString cacheIdentity = allocationIdentity(
        planLimits, intents, targetPs3, retainedPs3ExceedsCap);
    if (d->allocationCacheIdentity != cacheIdentity) {
        d->allocationCacheIdentity = cacheIdentity;
        d->cachedAllocation.reset();
        d->cachedAllocationError.clear();
        if (retainedPs3ExceedsCap) {
            RemoteDisplayAllocation paused;
            paused.total = ps3DisplayCharge();
            for (const RemoteDisplayIntent& intent : intents) {
                RemoteDisplayQuality quality;
                quality.panId = intent.panId;
                quality.suspended = true;
                paused.pans.append(quality);
            }
            d->cachedAllocation = paused;
        } else {
            d->cachedAllocation = allocateRemoteDisplay(
                planLimits, intents, targetPs3, &d->cachedAllocationError,
                d->iqBytesPerSecond != 0);
            if (!d->cachedAllocation) {
                qCInfo(lcRemoteMedia).noquote() << "Remote display allocation failed:"
                                                << d->cachedAllocationError;
            }
        }
    }
    const std::optional<RemoteDisplayAllocation>& allocation = d->cachedAllocation;
    if (!allocation) {
        const PanDisplayState status = refusedState(d->cachedAllocationError.left(384));
        for (const Desired& item : desired) { setPanStatus(item.panId, status); }
        return;
    }

    QHash<QString, RemoteDisplayQuality> qualities;
    for (const RemoteDisplayQuality& quality : allocation->pans) {
        qualities.insert(quality.panId, quality);
    }

    struct Candidate {
        quint32 endpointId = 0;
        QJsonObject request;
        DisplayBudgetCharge charge;
        QString identity;
        bool unsubscribe = false;
        bool reduction = false;
    };
    QList<Candidate> reductions;
    QList<Candidate> increases;
    QList<quint32> eraseUnaccepted;

    // R-R3-08/37: a reduction the Core made because it is busy says so;
    // any other reduction keeps the general capacity wording. The pan status
    // builder words it from PanDisplayState::budgetReason.
    const DisplayBudgetReason budgetReason = d->client->remoteDisplayBudgetReason();
    const auto statusFor = [now](const Private::Binding& binding, const Desired& item) {
        if (binding.pending && binding.pending->timedOut) {
            return phaseState(PanDisplayState::Phase::Stalled);
        }
        if (!binding.refusalReason.isEmpty()) {
            return refusedState(binding.refusalReason.left(384));
        }
        if (binding.acceptedRevision == 0 || binding.acceptedRequest.isEmpty()) {
            // R-R3-37: no "Waiting for the Core" flash while the Core
            // answers within the grace, as it does at session start.
            if (binding.waitingSinceMs >= 0
                && now - binding.waitingSinceMs < kPanWaitingGraceMs) {
                return phaseState(PanDisplayState::Phase::None);
            }
            return phaseState(PanDisplayState::Phase::Waiting);
        }
        PanDisplayState status = phaseState(PanDisplayState::Phase::Showing);
        status.pixels = binding.acceptedRequest.value(QStringLiteral("pixels")).toInt();
        status.fps = binding.acceptedRequest.value(QStringLiteral("fps")).toInt();
        int firstRecent = 0;
        while (firstRecent < binding.receivedFrameTimesMs.size()
               && binding.receivedFrameTimesMs.at(firstRecent) < now - 2'000) {
            ++firstRecent;
        }
        const int recentCount = binding.receivedFrameTimesMs.size() - firstRecent;
        if (recentCount >= 2 && now - binding.receivedFrameTimesMs.constLast() <= 1'000
            && binding.receivedFrameTimesMs.constLast()
                > binding.receivedFrameTimesMs.at(firstRecent)) {
            const double received = double(recentCount - 1) * 1000.0
                / double(binding.receivedFrameTimesMs.constLast()
                         - binding.receivedFrameTimesMs.at(firstRecent));
            status.receivedFps = received;
        }
        status.requestedPixels = item.original.value(QStringLiteral("pixels")).toInt();
        status.requestedFps = item.original.value(QStringLiteral("fps")).toInt();
        status.extendedView =
            binding.acceptedRequest.value(QStringLiteral("wideSpanFactor")).toDouble() > 1.0;
        return status;
    };

    for (const Desired& item : desired) {
        const RemoteDisplayQuality quality = qualities.value(item.panId);
        auto found = std::find_if(d->bindings.begin(), d->bindings.end(),
            [&item](const auto& entry) {
                return !entry.second.retiring && entry.second.panId == item.panId
                    && entry.second.widget == item.widget && entry.second.slice == item.slice
                    && entry.second.isMini()
                        == (item.intent.kind == RemoteDisplayIntent::Kind::Mini);
            });
        if (quality.suspended) {
            if (item.intent.kind == RemoteDisplayIntent::Kind::Mini) {
                emit miniDisplayUnavailable(item.slice->sliceIndex());
            }
            const QString suspendIdentity = QStringLiteral("suspend:%1").arg(limits->generation);
            if (found != d->bindings.end()
                && found->second.refusedIdentity == suspendIdentity) {
                setPanStatus(item.panId,
                             refusedState(found->second.refusalReason.left(384)));
                continue;
            }
            d->panBudgetReason.insert(item.panId, budgetReason);
            PanDisplayState paused = phaseState(PanDisplayState::Phase::Paused);
            paused.pureSignalOverLimit = retainedPs3ExceedsCap;
            paused.budgetReason = budgetReason;
            setPanStatus(item.panId, paused);
            if (!self) { return; }
            if (found == d->bindings.end()) { continue; }
            found->second.suspending = true;
            found->second.refusalReason.clear();
            if (found->second.pending) { continue; }
            if (found->second.acceptedRevision == 0) {
                eraseUnaccepted.append(found->first);
                continue;
            }
            reductions.append({found->first, {}, {},
                               suspendIdentity,
                               true, true});
            continue;
        }

        if (found == d->bindings.end()) {
            if (d->bindings.size() >= kMaxEndpoints || d->nextEndpoint == 0) {
                setPanStatus(item.panId, phaseState(PanDisplayState::Phase::TooManyPans));
                continue;
            }
            const quint32 endpointId = d->nextEndpoint++;
            found = d->bindings.try_emplace(endpointId).first;
            Private::Binding& binding = found->second;
            binding.panId = item.panId;
            binding.widget = item.widget;
            binding.slice = item.slice;
            if (item.intent.kind == RemoteDisplayIntent::Kind::Mini) {
                binding.miniSliceId = item.slice->sliceIndex();
            }
            SpectrumWidget* const sw = item.widget;
            if (sw) {
            binding.ctunGesture = connect(sw, &SpectrumWidget::ctunEnabledChanged,
                this, [this, endpointId](bool pinned) {
                    auto current = d->bindings.find(endpointId);
                    if (current == d->bindings.end() || !current->second.slice
                        || !d->model || !d->client || !d->client->remoteCtunAvailable()) { return; }
                    SliceModel* slice = currentSliceForPan(
                        d->model, d->stack, current->second.widget);
                    if (!slice || slice->streamIndex() < 0) { return; }
                    auto& state = d->ctunStreams[slice->streamIndex()];
                    state.epoch = slice->streamEpoch();
                    state.requestSliceId = slice->sliceIndex();
                    state.requestedPin = pinned;
                    state.pending = true;
                    state.initialized = false;
                    state.rejectedContext = 0;
                    state.refused = false;
                    if (!d->model->requestStreamCtunPinned(slice->sliceIndex(), pinned)) {
                        state.pending = false;
                        state.rejectedContext = current->second.context.codec.contextGeneration;
                    }
                });
            binding.centreGesture = connect(sw, &SpectrumWidget::centerChanged,
                this, [this, endpointId](double centreHz) {
                    auto current = d->bindings.find(endpointId);
                    if (current == d->bindings.end() || !current->second.slice
                        || !current->second.widget || !current->second.widget->ctunEnabled()
                        || !d->model || !d->client
                        || !d->client->remoteCtunAvailable()) { return; }
                    requestCentreFromGesture(endpointId, centreHz);
                });
            sw->traceRxHistoryEvent(SpectrumWidget::RxHistoryEvent::BudgetBindingCreated,
                                    item.slice->streamIndex(), item.slice->streamEpoch());
            sw->invalidateRemoteSpectrumFrame();
            if (!self || !sw) { return; }
            sw->applyRemoteCtunState(false, false);
            if (!self) { return; }
            found = d->bindings.find(endpointId);
            if (found == d->bindings.end()) { return; }
            }
        }

        Private::Binding& binding = found->second;
        binding.suspending = false;
        binding.desiredOriginal = item.original;
        const QJsonObject target = allocatedRequest(item.original, quality,
            item.widget ? item.widget.data()
                        : miniSettingsWidget(d->stack, item.slice->sliceIndex()));
        const QString identity = requestIdentity(target, limits->generation, targetPs3);
        if (!binding.refusedIdentity.isEmpty() && binding.refusedIdentity != identity) {
            binding.refusedIdentity.clear();
            binding.refusalReason.clear();
        }
        if (binding.acceptedRevision == 0 || binding.acceptedRequest.isEmpty()) {
            if (binding.waitingSinceMs < 0) { binding.waitingSinceMs = now; }
        } else {
            binding.waitingSinceMs = -1;
        }
        PanDisplayState status = statusFor(binding, item);
        status.budgetReason = status.reduced() ? budgetReason : DisplayBudgetReason::None;
        d->panBudgetReason.insert(item.panId, status.budgetReason);
        if (d->ps3Refused && !d->ps3RefusalReason.isEmpty()) {
            status.pureSignal = PanDisplayState::PureSignal::Refused;
            status.pureSignalRefusalReason = d->ps3RefusalReason.left(256);
        } else if (d->pendingPs3 && d->pendingPs3->timedOut) {
            status.pureSignal = PanDisplayState::PureSignal::Stalled;
        }
        setPanStatus(item.panId, status);
        if (!self) { return; }
        if (binding.pending || binding.refusedIdentity == identity) { continue; }
        if (binding.askAgain && binding.acceptedRequest != target) {
            binding.askAgain = false; // A new request supersedes it.
        }
        if (binding.acceptedRevision != 0 && binding.acceptedRequest == target
            && !binding.askAgain
            && binding.observedStream == item.slice->streamIndex()
            && binding.observedStreamEpoch == item.slice->streamEpoch()) {
            continue;
        }
        Candidate candidate{found->first, target, quality.charge, identity, false,
                            binding.acceptedRevision != 0
                                && nonIncreasing(quality.charge, binding.acceptedCharge)};
        (candidate.reduction ? reductions : increases).append(std::move(candidate));
    }

    for (quint32 endpointId : eraseUnaccepted) {
        // Fix wave 2 (Important 2): a pan paused before the Core accepted
        // it; a display the Core refused is closed there too, so its
        // request stops counting against the other devices.
        const auto found = d->bindings.find(endpointId);
        const quint32 sent = found == d->bindings.end() ? 0 : found->second.revision;
        d->bindings.erase(endpointId);
        if (sent != 0 && !sendRefusedRelease(endpointId, sent)) { return; }
    }

    if (askWanted && reductions.isEmpty() && increases.isEmpty()
        && std::none_of(d->bindings.cbegin(), d->bindings.cend(),
                        [](const auto& entry) { return entry.second.pending.has_value(); })) {
        // Every pan was answered at what the operator wants (or refused for
        // a reason other than the budget, which asking again does not
        // change): the ask is over, and the planner plans inside the share.
        d->askingWanted = false;
        d->budgetReplanRequested = true;
    }

    const QPointer<MediaPeer> peer = d->peer;
    const quint32 epoch = d->epoch;
    const QString connectionId = d->connectionId;
    const auto current = [this, self, peer, epoch, connectionId] {
        return self && d->peer == peer && d->epoch == epoch
            && d->connectionId == connectionId;
    };
    const auto sendCandidate = [this, &current](const Candidate& candidate) {
        auto found = d->bindings.find(candidate.endpointId);
        if (found == d->bindings.end() || found->second.pending) { return; }
        Private::Binding& binding = found->second;
        if (binding.isMini()) {
            emit miniDisplayUnavailable(binding.miniSliceId);
            binding.accepted = false;
            binding.decoder.reset();
            binding.presenter.reset();
            binding.lastMiniTrace.clear();
        }
        if (binding.askAgain) {
            binding.askAgain = false;
            binding.askedAgain = candidate.request;
        }
        ++binding.revision;
        if (binding.revision == 0) { ++binding.revision; }
        const quint32 revision = binding.revision;
        binding.pending = Private::Binding::Pending{
            candidate.unsubscribe ? Private::Binding::PendingKind::Unsubscribe
                                  : Private::Binding::PendingKind::Subscribe,
            revision, candidate.request, candidate.charge,
            candidate.identity, d->allocationClock(), false};
        QJsonObject wire = candidate.request;
        wire.insert(QStringLiteral("op"), candidate.unsubscribe
            ? QStringLiteral("unsubscribe") : QStringLiteral("subscribe"));
        wire.insert(QStringLiteral("endpointId"), static_cast<qint64>(candidate.endpointId));
        wire.insert(QStringLiteral("revision"), static_cast<qint64>(revision));
        const bool sent = send(wire);
        if (!current()) { return; }
        found = d->bindings.find(candidate.endpointId);
        if (!sent && found != d->bindings.end() && found->second.pending
            && found->second.pending->revision == revision) {
            found->second.pending.reset();
            found->second.refusedIdentity = candidate.identity;
            found->second.refusalReason = QStringLiteral("Unable to send the allocation request.");
        }
    };

    if (!reductions.isEmpty()) {
        for (const Candidate& candidate : reductions) {
            sendCandidate(candidate);
            if (!current()) { return; }
        }
        return;
    }

    bool endpointPending = false;
    for (const auto& [id, binding] : d->bindings) {
        if (binding.pending) { endpointPending = true; break; }
    }
    if (endpointPending) { return; }

    if (targetPs3 && !d->accountedPs3 && !d->pendingPs3) {
        QList<DisplayBudgetCharge> confirmed;
        confirmed.append(ps3DisplayCharge());
        for (const auto& [id, binding] : d->bindings) {
            if (binding.acceptedRevision != 0) { confirmed.append(binding.acceptedCharge); }
        }
        const auto total = sumDisplayCharges(confirmed);
        if (total && displayChargeFits(*limits, *total)) {
            const QPointer<RemoteMediaController> requestSelf(this);
            const QPointer<StationClient> requestClient = d->client;
            const quint32 requestEpoch = d->epoch;
            const quint32 commandId = requestClient->requestPs3DisplaySubscription(true);
            if (!requestSelf || d->client != requestClient || d->epoch != requestEpoch) { return; }
            if (commandId == 0 && !d->pendingPs3) {
                d->ps3Refused = true;
                d->ps3RefusalReason = QStringLiteral("Unable to request PureSignal display admission.");
                d->ps3RefusedGeneration = limits->generation;
            }
        }
        return;
    }
    if (d->pendingPs3) { return; }

    for (const Candidate& candidate : increases) {
        if (askWanted) {
            // Asking for what the operator wants: the Core decides.
            sendCandidate(candidate);
            if (!current()) { return; }
            continue;
        }
        QList<DisplayBudgetCharge> potential;
        if (d->accountedPs3) { potential.append(ps3DisplayCharge()); }
        for (const auto& [id, binding] : d->bindings) {
            DisplayBudgetCharge charge = binding.acceptedCharge;
            if (id == candidate.endpointId) {
                charge = maximumCharge(charge, candidate.charge);
            } else if (binding.pending) {
                charge = maximumCharge(charge, binding.pending->charge);
            }
            if (!zeroCharge(charge)) { potential.append(charge); }
        }
        const auto total = sumDisplayCharges(potential);
        if (!total || !displayChargeFits(*limits, *total)) { continue; }
        sendCandidate(candidate);
        if (!current()) { return; }
    }
}

void RemoteMediaController::requestCentreFromGesture(quint32 endpointId, double centreHz)
{
    auto current = d->bindings.find(endpointId);
    if (current == d->bindings.end() || !current->second.widget || !d->model) { return; }
    SpectrumWidget* const widget = current->second.widget;
    SliceModel* slice = currentSliceForPan(d->model, d->stack, widget);
    if (!slice || slice->streamIndex() < 0) { return; }
    const int stream = slice->streamIndex();
    // R-R3-18/19: a zoom re-centres the view on the VFO. With other
    // receivers on this stream the Core keeps its window where it is (it
    // refuses a centre that would drop one of them), so the zoom stays a
    // view change. On a stream of its own the VFO is always a valid centre,
    // and the Core follows the zoom as before.
    if (widget->isZoomRecentring() && d->model->slicesOnStream(stream).size() > 1) {
        return;
    }
    auto& request = d->centreRequests[stream];
    if (request.epoch != slice->streamEpoch()) {
        request = {};
        request.epoch = slice->streamEpoch();
    }
    const double wantedHz = std::round(centreHz);
    if (request.inFlight) {
        request.hasQueued = true;
        request.queuedSliceId = slice->sliceIndex();
        request.queuedHz = wantedHz;
        return;
    }
    request.inFlight = d->model->requestStreamCentre(slice->sliceIndex(), wantedHz);
}

void RemoteMediaController::finishCentreRequest(int sliceId, quint64 streamEpoch, bool accepted)
{
    if (!d->model) { return; }
    SliceModel* slice = d->model->sliceById(sliceId);
    if (!slice || slice->streamIndex() < 0) { return; }
    const int stream = slice->streamIndex();
    auto request = d->centreRequests.find(stream);
    if (request != d->centreRequests.end() && request->epoch == streamEpoch) {
        request->inFlight = false;
        if (accepted && request->hasQueued && slice->streamEpoch() == streamEpoch) {
            // The gesture moved on while the Core answered: ask for where
            // it is now, once.
            const int queuedSliceId = request->queuedSliceId;
            const double queuedHz = request->queuedHz;
            request->hasQueued = false;
            request->inFlight = d->model->requestStreamCentre(queuedSliceId, queuedHz);
            return;
        }
        request->hasQueued = false;
    }
    if (accepted || slice->streamEpoch() != streamEpoch) { return; }
    const QPointer<RemoteMediaController> self(this);
    QList<quint32> endpointIds;
    for (const auto& [id, binding] : d->bindings) { endpointIds.append(id); }
    for (quint32 id : endpointIds) {
        auto found = d->bindings.find(id);
        if (found == d->bindings.end()) { continue; }
        Private::Binding& binding = found->second;
        if (!binding.widget || !binding.slice
            || binding.slice->streamIndex() != stream
            || binding.slice->streamEpoch() != streamEpoch
            || binding.sourceCentreHz <= 0) { continue; }
        // A drag is optimistic view movement. A refused hardware move
        // returns the view to the last accepted Core source and ends that
        // drag, so the rest of the gesture neither asks again nor moves the
        // view away from what the Core is sending. The current plane is
        // kept: the view's own move retires it when the geometry changes,
        // and the subscription observer follows the restored window.
        QPointer<SpectrumWidget> widget = binding.widget;
        const double sourceCentreHz = binding.sourceCentreHz;
        widget->endPanDrag();
        if (!self || !widget) { return; }
        widget->setDisplayWindowPreservingHistory(sourceCentreHz, widget->bandwidth());
        if (!self || !widget) { return; }
        widget->setDdcCenterFrequency(sourceCentreHz);
        if (!self) { return; }
    }
    refreshSubscriptions();
}

void RemoteMediaController::refreshCtunState()
{
    if (!d->model || !d->client || !d->stack) { return; }
    const QPointer<RemoteMediaController> self(this);
    QSet<int> occupied;
    for (SliceModel* slice : d->model->slices()) {
        if (slice->streamIndex() >= 0) { occupied.insert(slice->streamIndex()); }
    }
    for (auto it = d->ctunStreams.begin(); it != d->ctunStreams.end();) {
        if (!occupied.contains(it.key())) { it = d->ctunStreams.erase(it); }
        else { ++it; }
    }
    for (auto it = d->centreRequests.begin(); it != d->centreRequests.end();) {
        if (!occupied.contains(it.key())) { it = d->centreRequests.erase(it); }
        else { ++it; }
    }
    QList<quint32> endpointIds;
    for (const auto& [id, binding] : d->bindings) { endpointIds.append(id); }
    for (quint32 id : endpointIds) {
        auto found = d->bindings.find(id);
        if (found == d->bindings.end()) { continue; }
        Private::Binding& binding = found->second;
        if (!binding.widget || !binding.slice) { continue; }
        const int stream = binding.slice->streamIndex();
        const quint64 epoch = binding.slice->streamEpoch();
        auto& state = d->ctunStreams[stream];
        if (state.epoch != epoch) { state = {}; state.epoch = epoch; }
        const bool currentContext = binding.accepted
            && binding.context.source.streamIndex == stream
            && binding.observedStreamEpoch == epoch;
        const bool available = d->client->remoteCtunAvailable()
            && d->model->isConnected() && stream >= 0 && epoch != 0
            && (state.initialized || state.pending || currentContext);
        if (available && currentContext && !state.initialized && !state.pending
            && !state.refused
            && (state.rejectedContext == 0
                || isNewerGeneration(binding.context.codec.contextGeneration, state.rejectedContext))) {
            // One hardware stream has one effective pin, even when several
            // pans show it. Restore once; mirrored truth then updates cohosts.
            // Never reassert competing saved preferences on every frame/ACK.
            bool preference = binding.widget->ctunPreference();
            // The active pan owns the initial preference when several pans
            // share a stream; ACK arrival order must not choose the winner.
            for (PanadapterApplet* applet : d->stack->allApplets()) {
                SliceModel* member = d->model->sliceById(applet->activeSliceIndex());
                if (member && member->streamIndex() == stream
                    && applet->panId() == d->stack->activePanId()) {
                    preference = applet->spectrumWidget()->ctunPreference();
                    break;
                }
            }
            state.requestSliceId = binding.slice->sliceIndex();
            state.requestedPin = preference;
            state.pending = true;
            const int requestSliceId = state.requestSliceId;
            const quint32 contextGeneration = binding.context.codec.contextGeneration;
            const bool requested = d->model->requestStreamCtunPinned(
                requestSliceId, preference);
            if (!self) { return; }
            auto currentState = d->ctunStreams.find(stream);
            if (!requested && currentState != d->ctunStreams.end()
                && currentState->epoch == epoch
                && currentState->requestSliceId == requestSliceId) {
                currentState->pending = false;
                currentState->rejectedContext = contextGeneration;
            }
        }
        found = d->bindings.find(id);
        if (found == d->bindings.end() || !found->second.widget
            || !found->second.slice) { continue; }
        QPointer<SpectrumWidget> widget = found->second.widget;
        const bool pinned = found->second.slice->streamCtunPinned();
        widget->applyRemoteCtunState(available, pinned);
        if (!self) { return; }
    }
}

void RemoteMediaController::requestAudio()
{
    d->audio->stop();
    d->audioEnabled = false;
    d->audioRetryPending = false;
    ++d->audioRetryGeneration;
    // Every request follows a mute, speaker, radio or retry change (or the
    // media link becoming ready), each of which the status reflects.
    if (!d->peer || !d->peer->isReady() || !d->model || !d->client
        || !d->client->mediaAvailable()) {
        refreshAudioStatus();
        return;
    }
    ++d->audioRevision;
    if (!d->audioRevision) { ++d->audioRevision; }
    d->lastAudioRequestMs = d->clock.elapsed();
    const bool enabled = d->model->isConnected() && !d->model->audioEngine()->masterMuted();
    const QPointer<RemoteMediaController> self(this);
    QJsonObject control{{QStringLiteral("op"), QStringLiteral("audio")},
                        {QStringLiteral("revision"), double(d->audioRevision)},
                        {QStringLiteral("enabled"), enabled}};
    // R-R3-23: the choice, replayed with every request, only to a Core that
    // offers it; Opus after this session's link trial failed.
    if (audioProfileNegotiated()) {
        control.insert(QStringLiteral("profile"), remoteAudioProfileToWire(
            d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback
                ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus));
        d->audioProfileRequested = true;
        const int bitrate = d->audioQualityChoice == RemoteAudioQualityChoice::SaveData ? 24000 : 48000;
        const auto& offered = d->client->audioOpusBitrates();
        if (audioQualityNegotiated() && offered && offered->contains(bitrate)) {
            control.insert(QStringLiteral("opusBitrate"), bitrate);
        }
    }
    send(control);
    if (!self) { return; }
    refreshAudioStatus();
}

// ---- R-R3-43: receiver audio streams for apps on this computer ----

std::shared_ptr<RemoteTciAudioStage> RemoteMediaController::requestReceiverAudio(
    int sliceId, IReceiverPcmSink* sink)
{
    if (!sink || sliceId < 0) { return {}; }
    auto found = d->receiverStreams.find(sliceId);
    const bool first = found == d->receiverStreams.end();
    if (first) {
        Private::ReceiverStream stream;
        stream.fanout = std::make_shared<Private::ReceiverFanout>();
        stream.fanout->sliceId = sliceId;
        stream.tciStage = std::make_shared<RemoteTciAudioStage>(sliceId);
        const std::shared_ptr<Private::ReceiverFanout> fanout = stream.fanout;
        // Runs on the receiver's worker thread; see IReceiverPcmSink.
        stream.receiver = std::make_unique<RemoteAudioReceiver>(
            RemoteAudioReceiver::PcmSinkMode{[fanout](const float* pcm, int frames) {
                std::lock_guard<std::mutex> lock(fanout->mutex);
                for (IReceiverPcmSink* consumer : std::as_const(fanout->sinks)) {
                    consumer->receiverAudioBlock(fanout->sliceId, pcm, frames);
                }
            }, stream.tciStage});
        RemoteAudioReceiver* const receiver = stream.receiver.get();
        connect(receiver, &RemoteAudioReceiver::restartRequested, this,
                [this, sliceId, receiver](const QString& reason, RemoteAudioReceiver::Fault fault) {
            onReceiverRestart(sliceId, receiver, reason, fault);
        });
        connect(receiver, &RemoteAudioReceiver::errorOccurred, this,
                [this, sliceId, receiver](const QString& reason, RemoteAudioReceiver::Fault fault) {
            onReceiverError(sliceId, receiver, reason, fault);
        });
        found = d->receiverStreams.emplace(sliceId, std::move(stream)).first;
    } else if (found->second.sinks.contains(sink)) {
        return found->second.tciStage;
    }
    Private::ReceiverStream& stream = found->second;
    const std::shared_ptr<RemoteTciAudioStage> stage = stream.tciStage;
    stream.sinks.append(sink);
    {
        std::lock_guard<std::mutex> lock(stream.fanout->mutex);
        stream.fanout->sinks.append(sink);
    }
    const QPointer<RemoteMediaController> self(this);
    if (first || stream.heldBy) {
        // A new consumer is a new demand: a stream the Core forgot (its
        // slice was gone, or it had no room) is asked for again, once.
        if (!d->peer) {
            notifyReceiverStopped(sliceId, remoteAudioOffReasonToWire(
                RemoteAudioOffReason::MediaNotReady));
        } else if (!receiverAudioNegotiated()) {
            notifyReceiverStopped(sliceId, QString::fromLatin1(kReceiverAudioUnavailableReason));
        } else if (d->peer->isReady()) {
            sendReceiverAudioRequest(sliceId, true);
        }
    } else if (stream.stopped) {
        // A later consumer hears why the shared stream is stopped.
        const QString reason = stream.stopReason;
        sink->receiverAudioStopped(sliceId, reason);
    }
    if (!self) { return {}; }
    refreshAudioStatus();
    return stage;
}

void RemoteMediaController::requestRawIq(int sliceId)
{
    if (sliceId < 0 || sliceId > 1) { return; }
    auto& stream = d->iqStreams[sliceId];
    if (stream.wanted) { return; }
    stream.wanted = true;
    if (!remoteIqNegotiated()) {
        emit rawIqUnavailable(sliceId,
            QStringLiteral("This Core does not send raw I/Q to this window. Updating the Core may help."));
        return;
    }
    sendIqRequest(sliceId, true);
}

void RemoteMediaController::releaseRawIq(int sliceId)
{
    const auto found = d->iqStreams.find(sliceId);
    if (found == d->iqStreams.end() || !found->second.wanted) { return; }
    found->second.wanted = false;
    found->second.enabled = false;
    found->second.sampleRate = 0;
    sendIqRequest(sliceId, false);
    d->iqBytesPerSecond = 0;
    for (const auto& [unused, stream] : d->iqStreams) {
        Q_UNUSED(unused);
        if (stream.enabled) {
            d->iqBytesPerSecond += quint64(stream.sampleRate) * 8
                + quint64((stream.sampleRate + 1023) / 1024) * 24;
        }
    }
    refreshSubscriptions();
}

void RemoteMediaController::sendIqRequest(int sliceId, bool enabled)
{
    if (!d->iqNegotiated || !d->peer || !d->peer->isReady()) { return; }
    auto& stream = d->iqStreams[sliceId];
    ++stream.revision;
    if (stream.revision == 0) { ++stream.revision; }
    send({{QStringLiteral("op"), QStringLiteral("iq-stream")},
          {QStringLiteral("sliceId"), sliceId},
          {QStringLiteral("revision"), qint64(stream.revision)},
          {QStringLiteral("enabled"), enabled}});
}

void RemoteMediaController::receiveIqContext(const QJsonObject& payload)
{
    QStringList keys = payload.keys();
    keys.sort();
    if (keys != QStringList{QStringLiteral("connectionId"), QStringLiteral("enabled"),
                            QStringLiteral("generation"), QStringLiteral("op"),
                            QStringLiteral("reason"), QStringLiteral("revision"),
                            QStringLiteral("sampleRateHz"), QStringLiteral("sliceId")}
        || !payload.value(QStringLiteral("enabled")).isBool()
        || !payload.value(QStringLiteral("reason")).isString()) { return; }
    quint32 revision = 0, generation = 0;
    double rawSlice = 0;
    if (!number(payload, "sliceId", 0, 1, rawSlice, true)
        || !uint32(payload, "revision", revision)
        || !uint32(payload, "generation", generation)) { return; }
    const int slice = int(rawSlice);
    double rawRate = 0;
    if (!number(payload, "sampleRateHz", 0, 384000, rawRate, true)) { return; }
    const auto found = d->iqStreams.find(slice);
    if (found == d->iqStreams.end() || found->second.revision != revision
        || !isNewerGeneration(generation, found->second.generation)) { return; }
    auto& stream = found->second;
    const bool enabled = payload.value(QStringLiteral("enabled")).toBool();
    const int rate = int(rawRate);
    if (enabled && (!stream.wanted || rate < 48000
                    || !payload.value(QStringLiteral("reason")).toString().isEmpty())) { return; }
    if (!enabled && rate != 0) { return; }
    stream.generation = generation;
    stream.enabled = enabled;
    stream.nextSequence = 0;
    stream.sampleRate = enabled ? rate : 0;
    d->iqBytesPerSecond = 0;
    for (const auto& [unused, current] : d->iqStreams) {
        Q_UNUSED(unused);
        if (current.enabled) {
            d->iqBytesPerSecond += quint64(current.sampleRate) * 8
                + quint64((current.sampleRate + 1023) / 1024) * 24;
        }
    }
    if (enabled) {
        emit rawIqRate(slice, rate);
    } else if (stream.wanted && !payload.value(QStringLiteral("reason")).toString().isEmpty()) {
        emit rawIqUnavailable(slice,
                              payload.value(QStringLiteral("reason")).toString());
    }
    refreshSubscriptions();
}

void RemoteMediaController::receiveIqFrame(const QByteArray& message)
{
    const auto frame = RemoteIqCodec::decode(message);
    if (!frame) {
        QList<int> affected;
        for (const auto& [sliceId, stream] : d->iqStreams) {
            if (stream.enabled) { affected.append(sliceId); }
        }
        const QPointer<RemoteMediaController> self(this);
        for (int sliceId : affected) {
            if (!self) { return; }
            failIqStream(sliceId, QStringLiteral("Malformed raw I/Q media frame."));
        }
        return;
    }
    const auto found = d->iqStreams.find(int(frame->sliceId));
    if (found == d->iqStreams.end() || !found->second.enabled
        || found->second.generation != frame->generation) { return; }
    auto& stream = found->second;
    if (frame->sequence != stream.nextSequence) {
        failIqStream(int(frame->sliceId),
                     QStringLiteral("Raw I/Q media sequence lost a frame."));
        return;
    }
    ++stream.nextSequence;
    emit rawIqBlock(int(frame->sliceId), stream.sampleRate, frame->samples);
}

void RemoteMediaController::failIqStream(int sliceId, const QString& reason)
{
    const auto found = d->iqStreams.find(sliceId);
    if (found == d->iqStreams.end() || !found->second.enabled) { return; }
    found->second.enabled = false;
    found->second.sampleRate = 0;
    d->iqBytesPerSecond = 0;
    for (const auto& [unused, stream] : d->iqStreams) {
        Q_UNUSED(unused);
        if (stream.enabled) {
            d->iqBytesPerSecond += quint64(stream.sampleRate) * 8
                + quint64((stream.sampleRate + 1023) / 1024) * 24;
        }
    }
    const QPointer<RemoteMediaController> self(this);
    emit rawIqUnavailable(sliceId, reason);
    if (!self) { return; }
    sendIqRequest(sliceId, false);
    if (self) { refreshSubscriptions(); }
}

void RemoteMediaController::releaseReceiverAudio(int sliceId, IReceiverPcmSink* sink)
{
    const auto found = d->receiverStreams.find(sliceId);
    if (found == d->receiverStreams.end() || !found->second.sinks.contains(sink)) { return; }
    Private::ReceiverStream& stream = found->second;
    stream.sinks.removeAll(sink);
    {
        // After this, the worker cannot reach the sink.
        std::lock_guard<std::mutex> lock(stream.fanout->mutex);
        stream.fanout->sinks.removeAll(sink);
    }
    if (!stream.sinks.isEmpty()) { return; }
    // The last consumer went: stop the stream here and at the Core.
    std::unique_ptr<RemoteAudioReceiver> receiver = std::move(stream.receiver);
    stream.tciStage->invalidate();
    receiver->stop();
    disconnect(receiver.get(), nullptr, this, nullptr);
    // This may run inside the receiver's own signal; it goes when that is over.
    receiver.release()->deleteLater();
    const bool askedThisConnection = d->receiverRevisions.contains(sliceId);
    const bool forgotten = stream.heldBy.has_value();
    d->receiverStreams.erase(found);
    const QPointer<RemoteMediaController> self(this);
    if (askedThisConnection && !forgotten && d->peer && d->peer->isReady()
        && receiverAudioNegotiated()) {
        sendReceiverAudioRequest(sliceId, false);
        if (!self) { return; }
    }
    // A stream the Core had no room for may fit now.
    if (d->peer && d->peer->isReady() && receiverAudioNegotiated()) {
        QList<int> waiting;
        for (const auto& [id, other] : d->receiverStreams) {
            if (other.heldBy == RemoteAudioOffReason::ReceiverLimit) { waiting.append(id); }
        }
        for (int id : waiting) {
            if (d->receiverStreams.count(id) == 0) { continue; }
            sendReceiverAudioRequest(id, true);
            if (!self) { return; }
        }
    }
    reconcileLinkTrial();
    refreshAudioStatus();
}

void RemoteMediaController::sendReceiverAudioRequest(int sliceId, bool enabled)
{
    if (!d->peer || !d->peer->isReady() || !receiverAudioNegotiated()) { return; }
    // Never lower than any revision this connection sent for the slice id.
    quint32& revision = d->receiverRevisions[sliceId];
    ++revision;
    if (!revision) { ++revision; }
    const auto found = d->receiverStreams.find(sliceId);
    if (found != d->receiverStreams.end()) {
        found->second.heldBy.reset();
        found->second.retryPending = false;
        found->second.lastRequestMs = d->clock.elapsed();
    }
    // The one quality choice, as the speakers' stream asks for it.
    const RemoteAudioProfile profile =
        d->audioProfileChoice == RemoteAudioProfile::Lossless && !d->losslessFallback
        ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("receiver-audio")},
                     {QStringLiteral("sliceId"), sliceId},
                     {QStringLiteral("revision"), double(revision)},
                     {QStringLiteral("enabled"), enabled},
                     {QStringLiteral("profile"), remoteAudioProfileToWire(profile)}});
}

void RemoteMediaController::requestWantedReceiverAudio()
{
    if (!d->peer || !d->peer->isReady() || !receiverAudioNegotiated()) { return; }
    QList<int> wanted;
    for (const auto& [sliceId, stream] : d->receiverStreams) {
        if (!stream.faulted) { wanted.append(sliceId); }
    }
    const QPointer<RemoteMediaController> self(this);
    for (int sliceId : wanted) {
        if (d->receiverStreams.count(sliceId) == 0) { continue; }
        sendReceiverAudioRequest(sliceId, true);
        if (!self) { return; }
    }
    refreshAudioStatus();
}

void RemoteMediaController::receiveReceiverAudioContext(const QJsonObject& payload)
{
    const std::optional<RemoteReceiverAudioContextMessage> message =
        decodeReceiverAudioContext(payload);
    if (!message) { return; }
    const int sliceId = message->sliceId;
    const auto found = d->receiverStreams.find(sliceId);
    // A released stream's late answer changes nothing.
    if (found == d->receiverStreams.end()) { return; }
    Private::ReceiverStream& stream = found->second;
    const RemoteAudioContextMessage& context = message->context;
    if (context.revision != d->receiverRevisions.value(sliceId)
        || !isNewerGeneration(context.generation, stream.generation)) { return; }
    if (context.enabled && !d->receiverSsrcs.contains(context.ssrc)) { return; }
    stream.generation = context.generation;
    stream.context = context;
    stream.retryPending = false;
    stream.tciStage->invalidate();
    stream.receiver->stop();
    stream.ssrc = 0;
    stream.runningProfile.reset();
    const QPointer<RemoteMediaController> self(this);
    if (context.enabled) {
        const RemoteAudioProfile profile = context.losslessEncoder
            ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
        if (stream.receiver->start(context.ssrc, context.firstTimestamp, profile)) {
            stream.ssrc = context.ssrc;
            stream.runningProfile = profile;
            stream.stopped = false;
            stream.stopReason.clear();
            stream.faulted = false;
            qCInfo(lcRemoteMedia).noquote()
                << QStringLiteral("Remote receiver audio for slice %1: %2, context %3")
                       .arg(sliceId).arg(reportedAudioProfile(context)).arg(context.generation);
        }
    } else {
        const RemoteAudioOffReason reason =
            context.offReason.value_or(RemoteAudioOffReason::EncoderUnavailable);
        if (reason == RemoteAudioOffReason::SliceRemoved
            || reason == RemoteAudioOffReason::ReceiverLimit) {
            // The Core dropped the request; see sliceAdded and release.
            stream.heldBy = reason;
        }
        // The Core confirming a stop this computer made keeps this
        // computer's own reason.
        if (!(reason == RemoteAudioOffReason::ClientDisabled && stream.faulted)) {
            notifyReceiverStopped(sliceId, remoteAudioOffReasonToWire(reason));
            if (!self) { return; }
        }
    }
    reconcileLinkTrial();
    refreshAudioStatus();
}

void RemoteMediaController::notifyReceiverStopped(int sliceId, const QString& reason)
{
    const auto found = d->receiverStreams.find(sliceId);
    if (found == d->receiverStreams.end()) { return; }
    Private::ReceiverStream& stream = found->second;
    if (stream.stopped && stream.stopReason == reason) { return; }
    stream.stopped = true;
    stream.stopReason = reason;
    qCInfo(lcRemoteMedia).noquote()
        << QStringLiteral("Remote receiver audio for slice %1 stopped: %2").arg(sliceId).arg(reason);
    // A consumer may release itself, or another, from inside the notice.
    const QList<IReceiverPcmSink*> sinks = stream.sinks;
    const QPointer<RemoteMediaController> self(this);
    for (IReceiverPcmSink* sink : sinks) {
        const auto still = d->receiverStreams.find(sliceId);
        if (still == d->receiverStreams.end()) { return; }
        if (!still->second.sinks.contains(sink)) { continue; }
        sink->receiverAudioStopped(sliceId, reason);
        if (!self) { return; }
    }
}

void RemoteMediaController::onReceiverRestart(int sliceId, RemoteAudioReceiver* receiver,
                                              const QString& reason,
                                              RemoteAudioReceiver::Fault fault)
{
    const auto found = d->receiverStreams.find(sliceId);
    if (found == d->receiverStreams.end() || found->second.receiver.get() != receiver) { return; }
    Private::ReceiverStream& stream = found->second;
    qCWarning(lcRemoteMedia).noquote()
        << QStringLiteral("Remote receiver audio for slice %1: %2").arg(sliceId).arg(reason);
    stream.tciStage->invalidate();
    stream.receiver->stop();
    stream.ssrc = 0;
    // R-R3-23: a lossless receiver stream's restart counts against the one
    // link trial, as the speakers' does.
    if (stream.context && stream.context->losslessEncoder && d->linkTrial.active()
        && linkInterruption(fault)
        && d->linkTrial.noteInterruption(d->clock.elapsed())
            == RemoteAudioLinkTrial::Verdict::Failed) {
        fallBackToOpus(QStringLiteral("receiver stream restarts while lossless audio plays"));
        return;
    }
    if (!stream.retryPending) {
        stream.retryPending = true;
        const QString connection = d->connectionId;
        const quint32 revision = d->receiverRevisions.value(sliceId);
        const int delay = int(stream.restartBackoff.nextDelayMs(d->clock.elapsed(),
                                                                stream.lastRequestMs));
        QTimer::singleShot(delay, Qt::PreciseTimer, this, [this, sliceId, connection, revision] {
            const auto again = d->receiverStreams.find(sliceId);
            if (again == d->receiverStreams.end() || !again->second.retryPending
                || connection != d->connectionId
                || revision != d->receiverRevisions.value(sliceId)) { return; }
            again->second.retryPending = false;
            sendReceiverAudioRequest(sliceId, true);
        });
    }
    refreshAudioStatus();
}

void RemoteMediaController::onReceiverError(int sliceId, RemoteAudioReceiver* receiver,
                                            const QString& reason,
                                            RemoteAudioReceiver::Fault fault)
{
    const auto found = d->receiverStreams.find(sliceId);
    if (found == d->receiverStreams.end() || found->second.receiver.get() != receiver) { return; }
    Private::ReceiverStream& stream = found->second;
    qCWarning(lcRemoteMedia).noquote()
        << QStringLiteral("Remote receiver audio for slice %1 failed: %2").arg(sliceId).arg(reason);
    stream.tciStage->invalidate();
    stream.receiver->stop();
    stream.ssrc = 0;
    stream.runningProfile.reset();
    stream.faulted = true;
    stream.retryPending = false;
    const QPointer<RemoteMediaController> self(this);
    sendReceiverAudioRequest(sliceId, false);
    if (!self) { return; }
    notifyReceiverStopped(sliceId, remoteAudioProblemText(fault));
    if (!self) { return; }
    reconcileLinkTrial();
    refreshAudioStatus();
}

void RemoteMediaController::reconcileLinkTrial()
{
    // R-R3-23, R-R3-43: one trial while any lossless stream plays.
    bool lossless = d->audioEnabled && d->acceptedAudioContext
        && d->acceptedAudioContext->losslessEncoder;
    for (const auto& [sliceId, stream] : d->receiverStreams) {
        if (stream.receiver->isRunning() && stream.runningProfile == RemoteAudioProfile::Lossless) {
            lossless = true;
        }
    }
    // R-R3-45: the headphones mix counts too.
    if (d->headphones->isRunning() && d->headphonesContext
        && d->headphonesContext->losslessEncoder) {
        lossless = true;
    }
    if (lossless && !d->linkTrial.active()) {
        d->linkTrial.begin(d->clock.elapsed());
        d->linkTrialTimer->start();
    } else if (!lossless && d->linkTrial.active()) {
        d->linkTrial.end();
        d->linkTrialTimer->stop();
    }
}

void RemoteMediaController::receiveAllocationResult(const QJsonObject& payload)
{
    if (payload.size() != 11 || !payload.value(QStringLiteral("accepted")).isBool()
        || !payload.value(QStringLiteral("reason")).isString()) {
        return;
    }
    quint32 endpointId = 0;
    quint32 revision = 0;
    quint32 budgetGeneration = 0;
    quint64 acceptedRevisionValue = 0;
    DisplayBudgetCharge retained;
    quint64 messages = 0;
    if (!uint32(payload, "endpointId", endpointId)
        || !uint32(payload, "revision", revision)
        || !uint32(payload, "budgetGeneration", budgetGeneration)
        || !uint64(payload, "acceptedRevision", std::numeric_limits<quint32>::max(),
                   acceptedRevisionValue)
        || !uint64(payload, "applicationBytesPerSecond",
                   kDisplayBudgetJsonSafePositiveLimit,
                   retained.applicationBytesPerSecond)
        || !uint64(payload, "spectrumSampleUnitsPerSecond",
                   kDisplayBudgetJsonSafePositiveLimit,
                   retained.spectrumSampleUnitsPerSecond)
        || !uint64(payload, "messagesPerSecond", kDisplaySenderMessagesPerSecond,
                   messages)) {
        return;
    }
    retained.messagesPerSecond = static_cast<quint32>(messages);
    (void)budgetGeneration; // Parsed for exact shape; outcomes reconcile by endpoint/revision.
    const quint32 acceptedRevision = static_cast<quint32>(acceptedRevisionValue);
    if ((acceptedRevision == 0) != zeroCharge(retained)
        || (acceptedRevision != 0
            && (retained.applicationBytesPerSecond == 0
                || retained.spectrumSampleUnitsPerSecond == 0
                || retained.messagesPerSecond == 0))) {
        return;
    }
    auto found = d->bindings.find(endpointId);
    if (found == d->bindings.end()) { return; }
    Private::Binding& binding = found->second;
    const bool outcomeAccepted = payload.value(QStringLiteral("accepted")).toBool();
    const QString reason = payload.value(QStringLiteral("reason")).toString().left(512);

    if (!binding.pending || binding.pending->revision != revision) {
        // Core may reconcile a resource to zero after source/radio retirement.
        // A rejection that merely restates the retained current reservation is
        // not the outcome of a newer GUI operation and cannot erase it.
        if (revision == binding.revision
            && acceptedRevision == 0 && zeroCharge(retained)
            && binding.acceptedRevision != 0) {
            const QPointer<RemoteMediaController> self(this);
            QPointer<SpectrumWidget> widget = binding.widget;
            const bool erase = binding.retiring || binding.suspending || !binding.widget;
            binding.acceptedRevision = 0;
            binding.acceptedCharge = {};
            binding.acceptedRequest = {};
            binding.accepted = false;
            binding.contextRevision = 0;
            binding.decoder.reset();
            binding.presenter.reset();
            if (erase) { d->bindings.erase(found); }
            if (widget) { widget->invalidateRemoteSpectrumFrame(); }
            if (!self) { return; }
            refreshSubscriptions();
        }
        return;
    }

    const Private::Binding::Pending pending = *binding.pending;
    if (pending.kind == Private::Binding::PendingKind::Subscribe) {
        if (outcomeAccepted) {
            // Under the grant report Core charges the pixels it granted,
            // which may be fewer than requested; it never charges more.
            const bool chargeAccepted = spectrumGrantNegotiated()
                ? nonIncreasing(retained, pending.charge) : retained == pending.charge;
            if (acceptedRevision != revision || !chargeAccepted) { return; }
            binding.acceptedRevision = acceptedRevision;
            binding.acceptedCharge = retained;
            binding.acceptedRequest = pending.request;
            binding.refusedIdentity.clear();
            binding.refusalReason.clear();
            if (binding.slice) {
                binding.observedStream = binding.slice->streamIndex();
                binding.observedStreamEpoch = binding.slice->streamEpoch();
            }
        } else {
            const bool sourceRetired = acceptedRevision == 0 && zeroCharge(retained)
                && binding.acceptedRevision != 0;
            if (!sourceRetired
                && (acceptedRevision != binding.acceptedRevision
                    || retained != binding.acceptedCharge)) { return; }
            binding.refusedIdentity = pending.identity;
            binding.refusalReason = reason.isEmpty()
                ? QStringLiteral("Core refused the display allocation.") : reason;
            if (binding.isMini()) { emit miniDisplayUnavailable(binding.miniSliceId); }
            if (reason == QLatin1String(kDisplayBudgetRefusalReason)) {
                // Fix wave 2 (Critical 1): the ask for what the operator
                // wants is answered. The share the Core published with it
                // is what the planner now plans inside.
                d->askingWanted = false;
            }
            // The raw reason is kept for the log; the pan shows it translated.
            qCInfo(lcRemoteMedia).noquote() << "Remote display allocation refused:"
                                            << endpointId << binding.refusalReason;
            if (sourceRetired) {
                const QPointer<RemoteMediaController> self(this);
                QPointer<SpectrumWidget> widget = binding.widget;
                const bool erase = binding.retiring || binding.suspending || !binding.widget;
                binding.acceptedRevision = 0;
                binding.acceptedCharge = {};
                binding.acceptedRequest = {};
                binding.accepted = false;
                binding.contextRevision = 0;
                binding.decoder.reset();
                binding.presenter.reset();
                binding.pending.reset();
                if (erase) { d->bindings.erase(found); }
                if (widget) {
                    widget->invalidateRemoteSpectrumFrame();
                    if (!self || !widget) { return; }
                    widget->applyRemoteCtunState(false, false);
                    if (!self) { return; }
                }
                refreshSubscriptions();
                return;
            }
        }
        binding.pending.reset();
        if (binding.retiring) {
            retireSubscriptions({endpointId});
            return;
        }
    } else {
        const bool sourceRetired = !outcomeAccepted
            && acceptedRevision == 0 && zeroCharge(retained)
            && binding.acceptedRevision != 0;
        if (outcomeAccepted || sourceRetired) {
            if (!sourceRetired && (acceptedRevision != 0 || !zeroCharge(retained))) { return; }
            const QString panId = binding.panId;
            QPointer<SpectrumWidget> widget = binding.widget;
            const bool suspended = binding.suspending;
            const QPointer<RemoteMediaController> self(this);
            binding.pending.reset();
            binding.acceptedRevision = 0;
            binding.acceptedCharge = {};
            binding.acceptedRequest = {};
            binding.accepted = false;
            binding.contextRevision = 0;
            binding.decoder.reset();
            binding.presenter.reset();
            d->bindings.erase(found);
            if (widget) {
                // Capacity suspension freezes painted history. Only reject
                // frames for the released endpoint; do not clear its rows.
                widget->invalidateRemoteSpectrumFrame();
                if (!self || !widget) { return; }
                widget->applyRemoteCtunState(false, false);
                if (!self) { return; }
            }
            if (suspended) {
                const DisplayBudgetReason reason = d->client
                    ? d->client->remoteDisplayBudgetReason() : DisplayBudgetReason::None;
                d->panBudgetReason.insert(panId, reason);
                PanDisplayState paused = phaseState(PanDisplayState::Phase::Paused);
                paused.budgetReason = reason;
                setPanStatus(panId, paused);
            }
        } else {
            if (acceptedRevision != binding.acceptedRevision
                || retained != binding.acceptedCharge) { return; }
            binding.pending.reset();
            binding.refusedIdentity = pending.identity;
            binding.refusalReason = reason.isEmpty()
                ? QStringLiteral("Core refused the display release.") : reason;
            qCInfo(lcRemoteMedia).noquote() << "Remote display release refused:"
                                            << endpointId << binding.refusalReason;
        }
    }
    refreshSubscriptions();
}

void RemoteMediaController::receiveControl(const QJsonObject& payload, quint32 epoch)
{
    // R-R3-35: a clock echo's arrival time (t3), read before anything else.
    const qint64 receivedNs = d->audio->nowNs();
    if (!d->client || !d->client->mediaAvailable() || epoch != d->epoch || !d->peer) { return; }
    // iPhone app plan Task 29: the replacement's own operations, and the
    // Core's word that it took over.
    if (d->replacement && !d->replacementId.isEmpty()
        && payload.value(QStringLiteral("connectionId")) == d->replacementId) {
        receiveReplacementControl(payload);
        return;
    }
    if (payload.value(QStringLiteral("connectionId")) != d->connectionId) { return; }
    // Task 29: a context naming a replacement peer's SSRC names the
    // stream's own.
    QJsonObject normalised;
    if (!d->toLogical.isEmpty() && payload.value(QStringLiteral("ssrc")).isDouble()) {
        const auto ssrc = static_cast<quint32>(payload.value(QStringLiteral("ssrc")).toDouble());
        const auto it = d->toLogical.constFind(ssrc);
        if (it != d->toLogical.cend()) {
            normalised = payload;
            normalised.insert(QStringLiteral("ssrc"), static_cast<qint64>(it.value()));
        }
    }
    if (!normalised.isEmpty()) {
        receiveControl(normalised, epoch);
        return;
    }
    const QString op = payload.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("allocation-result")) {
        receiveAllocationResult(payload);
        return;
    }
    if (op == QLatin1String("clock-echo")) {
        if (audioClockNegotiated()) { receiveClockEcho(payload, receivedNs); }
        return;
    }
    if (op == QLatin1String("audio-context")) {
        // The shape the agreed minor selects, then this session's identity.
        // Anything refused leaves generation, playback and signals untouched.
        const std::optional<RemoteAudioContextMessage> context =
            decodeRemoteAudioContext(payload, audioDetailNegotiated(),
                                     d->audioProfileRequested);
        if (!context || context->revision != d->audioRevision
            || !isNewerGeneration(context->generation, d->audioGeneration)
            || context->ssrc != d->logicalSsrcs.value(0, d->peer->audioSsrc())) { return; }
        d->audioGeneration = context->generation;
        d->acceptedAudioContext = context;
        // R-R3-35: the Core's capture of the previous context no longer
        // describes what plays; the next echo brings this one's.
        d->captureAnchor.reset();
        // Core has answered: any automatic retry in flight is over.
        d->audioRestarting = false;
        d->audio->stop();
        d->audioEnabled = false;
        // start() can report a speaker failure synchronously, and a listener
        // to that report, or to the status change, may retire this controller.
        const QPointer<RemoteMediaController> self(this);
        if (context->enabled && d->model
            && d->model->isConnected() && !d->model->audioEngine()->masterMuted()) {
            d->preparingAudio = true;
            const RemoteAudioProfile profile = context->losslessEncoder
                ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
            const bool started = d->audio->start(context->ssrc, context->firstTimestamp, profile);
            if (!self) { return; }
            d->audioEnabled = started;
            d->preparingAudio = false;
            if (d->audioEnabled) {
                qCInfo(lcRemoteMedia).noquote()
                    << QStringLiteral("Remote audio receiving: %1, context %2")
                           .arg(reportedAudioProfile(*context))
                           .arg(context->generation);
            }
        }
        // R-R3-23: the link trial runs while lossless audio plays. A
        // restart's new lossless context continues it; anything else ends
        // it (mute, a radio drop, Opus), and lossless later begins anew.
        // R-R3-43: unless a receiver stream still plays lossless.
        reconcileLinkTrial();
        refreshAudioStatus();
        if (!self) { return; }
        emit audioContextAccepted();
        return;
    }
    if (op == QLatin1String("monitor-audio-context")) {
        // Parity Task 32: only from a Core this connection declared it to.
        if (d->txMonitorNegotiated) { receiveMonitorAudioContext(payload); }
        return;
    }
    if (op == QLatin1String("headphones-audio-context")) {
        // R-R3-45: only from a Core this connection declared it to.
        if (headphonesMixNegotiated()) { receiveHeadphonesAudioContext(payload); }
        return;
    }
    if (op == QLatin1String("receiver-audio-context")) {
        if (receiverAudioNegotiated()) { receiveReceiverAudioContext(payload); }
        return;
    }
    if (op == QLatin1String("iq-stream-context")) {
        if (d->iqNegotiated) { receiveIqContext(payload); }
        return;
    }
    if (op == QLatin1String("description") || op == QLatin1String("candidate")) {
        const QPointer<MediaPeer> peer(d->peer);
        const bool accepted = peer->acceptControl(payload);
        if (accepted && op == QLatin1String("description") && d->peer == peer
            && d->awaitingDescription && !peer->isReady()) {
            // Stage two: Core's description is here; the connection now
            // has the pinned library's own slowest failure report.
            d->awaitingDescription = false;
            d->establishTimer->start(connectStageMs(d->connectDeadlineMs, peer->gathersFromServers()));
        }
        return;
    }
    // Core refused this connection's media (its peer could not start), or
    // dropped it: op, connectionId, endpointId 0, revision 0 and reason, as
    // DaemonMediaController::sendRejected() sends them.
    if (op == QLatin1String("rejected") && payload.size() == 5
        && payload.value(QStringLiteral("endpointId")).isDouble()
        && payload.value(QStringLiteral("endpointId")).toDouble() == 0
        && payload.value(QStringLiteral("revision")).isDouble()
        && payload.value(QStringLiteral("revision")).toDouble() == 0
        && payload.value(QStringLiteral("reason")).isString()) {
        const QString reason = payload.value(QStringLiteral("reason")).toString().left(512);
        // The Core dropped its peer on its own (the connection failed or
        // closed on its side): start media over now, as when this
        // computer's own peer fails, instead of waiting for it to time out.
        if (reason == QLatin1String(kMediaPeerLostReason)
            || reason == QLatin1String(kMediaPeerClosedReason)) {
            requestRecovery(epoch, reason);
            return;
        }
        settleWithoutRetry(epoch, reason);
        return;
    }
    quint32 endpointId = 0, revision = 0;
    if (!uint32(payload, "endpointId", endpointId) || !uint32(payload, "revision", revision)) { return; }
    auto it = d->bindings.find(endpointId);
    if (it == d->bindings.end() || (!it->second.widget && !it->second.isMini())) { return; }
    auto& binding = it->second;
    const bool budgetMode = d->client->remoteDisplayBudgetLimits().has_value();
    if (op == QLatin1String("noise-floor")) {
        quint32 generation = 0;
        double floor = 0;
        if (payload.size() != 6 || !binding.accepted || binding.rejected
            || revision != (budgetMode ? binding.contextRevision : binding.revision)
            || !uint32(payload, "contextGeneration", generation)
            || generation != binding.context.codec.contextGeneration
            || !number(payload, "floorDbm", -400, 100, floor)
            || !d->model || !d->model->isConnected() || !d->stack
            || !binding.slice
            || d->stack->spectrum(d->stack->activePanId()) != binding.widget
            || binding.observedStream != binding.slice->streamIndex()
            || binding.observedStreamEpoch != binding.slice->streamEpoch()
            || (budgetMode
                ? !sameOriginalIntent(
                    d->request(binding.widget, binding.slice),
                    binding.acceptedRequest)
                : d->request(binding.widget, binding.slice) != binding.observed)) {
            return;
        }
        // Clarity remains the GUI's existing active-pan controller. Core
        // supplies the full-source percentile, before any display detector
        // or codec can bias it; palette and operator overrides stay local.
        if (ClarityController* clarity = d->model->clarityController()) {
            if (!binding.receivedNoiseFloor) {
                binding.receivedNoiseFloor = true;
                qCInfo(lcRemoteMedia) << "Core noise floor received for Clarity:" << floor << "dBm";
            }
            clarity->feedNoiseFloor(static_cast<float>(floor));
        }
        return;
    }
    if (op == QLatin1String("rejected")) {
        // Core's per-endpoint refusal or retirement: op, connectionId,
        // endpointId, revision and reason, as DaemonMediaController::
        // sendRejected() sends them. R-R3-01/08/37: the pan says why, as a
        // status line only (no toast), until its request changes.
        if (!budgetMode && revision == binding.revision && payload.size() == 5
            && payload.value(QStringLiteral("reason")).isString()) {
            const QString reason = payload.value(QStringLiteral("reason")).toString();
            // Core retires an endpoint whose slice it removed or rebound
            // (DaemonMediaController::onSliceRemoved / onStreamBindingsChanged).
            // That state reaches this window over the control session, which
            // is not ordered with the media channel, so the refusal can land
            // while the slice still looks unchanged here. Those are the
            // operator's own changes, not refusals: the pan goes blank and
            // the mirrored change then retires or renews the binding. The
            // wording comes from SpectrumEndpoint.h, the same constants the
            // Core sends. A source retune (kRetireReasonSourceRetune) keeps
            // its status line: a fixed-view pan may not re-request when the
            // mirrored retune lands, and a blank pan needs its reason.
            const bool operatorChange = reason == QLatin1String(kRetireReasonSliceRemoved)
                || reason == QLatin1String(kRetireReasonStreamBindingChanged);
            binding.accepted = false;
            binding.rejected = true;
            binding.refusalReason = operatorChange ? QString()
                : (reason.isEmpty() ? QStringLiteral("Core refused the display allocation.")
                                    : reason.left(512));
            qCInfo(lcRemoteMedia).noquote() << "Remote display endpoint rejected:"
                                            << endpointId << reason.left(512);
            binding.decoder.reset();
            binding.presenter.reset();
            if (binding.isMini()) {
                emit miniDisplayUnavailable(binding.miniSliceId);
                return;
            }
            const QPointer<RemoteMediaController> self(this);
            const QString panId = binding.panId;
            binding.widget->traceRxHistoryEvent(SpectrumWidget::RxHistoryEvent::EndpointRejected,
                                                binding.observedStream, binding.observedStreamEpoch);
            binding.widget->clearRemoteSpectrum();
            if (!self) { return; }
            setPanStatus(panId, perPanRefusalStatus(panId));
        }
        return;
    }
    const bool widebandRequested = d->client->remoteWidebandAvailable()
        && (budgetMode ? binding.acceptedRequest : binding.observed)
               .value(QStringLiteral("extendedView")).isBool();
    if (op != QLatin1String("context") || binding.rejected
        || revision != (budgetMode ? binding.acceptedRevision : binding.revision)) { return; }
    // The shape the agreed minor selects, with wideband exactly when this
    // subscription negotiated it.
    const std::optional<SpectrumContextMessage> decoded =
        decodeRemoteSpectrumContext(payload, spectrumGrantNegotiated(),
                                    d->txDisplayNegotiated);
    if (!decoded || decoded->wideband.has_value() != widebandRequested) { return; }
    // A gesture/rebind may arrive between the outgoing request and its ACK.
    // Issue the newer request before accepting an old view over that gesture.
    if (binding.slice) {
        const QJsonObject currentRequest = binding.isMini()
            ? d->requestMini(binding.slice)
            : d->request(binding.widget, binding.slice);
        const bool requestChanged = budgetMode
            ? !sameOriginalIntent(currentRequest, binding.acceptedRequest)
            : currentRequest != binding.observed;
        if (requestChanged || binding.observedStream != binding.slice->streamIndex()
            || binding.observedStreamEpoch != binding.slice->streamEpoch()) {
            refreshSubscriptions();
            return;
        }
    }
    SpectrumEndpointContext context;
    context.codec.endpointId = endpointId;
    context.codec.contextGeneration = decoded->contextGeneration;
    if (decoded->wideband) { context.wideband = *decoded->wideband; }
    context.exactCentreHz = decoded->centreHz;
    context.exactSpanHz = decoded->spanHz;
    context.wideCentreHz = decoded->wideCentreHz;
    context.wideSpanHz = decoded->wideSpanHz;
    const double sourceCentre = decoded->sourceCentreHz;
    const double rate = decoded->sampleRateHz;
    if (widebandRequested) {
        const bool permission = (budgetMode ? binding.acceptedRequest : binding.observed).value(
            QStringLiteral("extendedView")).toBool();
        const bool needsWideband = geometryNeedsWideband(
            context.exactCentreHz, context.exactSpanHz, sourceCentre, rate);
        if (context.wideband.active != needsWideband
            || (context.wideband.active && !permission)) { return; }
    }
    if (binding.accepted && !isNewerGeneration(context.codec.contextGeneration,
                                  binding.acceptedGeneration())) { return; }
    context.codec.traceSamples = quint16(decoded->traceSamples);
    context.codec.waterfallSamples = quint16(decoded->waterfallSamples);
    context.codec.wideSamples = quint16(decoded->wideSamples);
    context.codec.minDbm = float(decoded->minDbm);
    context.codec.maxDbm = float(decoded->maxDbm);
    context.source.streamIndex = decoded->sourceStream;
    context.targetFps = decoded->fps;
    context.framesPerLine = decoded->framesPerLine;
    if (binding.isMini()) {
        const QJsonObject target = budgetMode ? binding.acceptedRequest : binding.observed;
        const bool wantsTx = d->model && binding.slice && d->model->isTransmitting()
            && d->model->txBoundSlice() == binding.slice;
        const double expectedCentre = target.value(QStringLiteral("centreHz")).toDouble();
        const double expectedSpan = target.value(QStringLiteral("spanHz")).toDouble();
        const int fftSize = decoded->grant ? decoded->grant->grantedFftSize
            : target.value(QStringLiteral("fftSize")).toInt();
        // Core reports the exact inclusive FFT-bin crop, which can differ
        // from the requested RF window by several bins. Recompute that crop
        // from the same SpectrumReducer rule rather than allowing a broad
        // tolerance that could admit a stale, shifted context.
        ReducerConfig crop;
        crop.centreHz = expectedCentre;
        crop.spanHz = expectedSpan;
        crop.streamCentreHz = decoded->sourceCentreHz;
        crop.sampleRateHz = decoded->sampleRateHz;
        const auto bins = fftSize > 0
            ? SpectrumReducer::visibleBinRange(fftSize, crop)
            : std::pair<int, int>{0, -1};
        const double binHz = fftSize > 0 ? decoded->sampleRateHz / fftSize : 0.0;
        const double cropCentre = decoded->sourceCentreHz - decoded->sampleRateHz * 0.5
            + (bins.first + bins.second + 1) * binHz * 0.5;
        const double cropSpan = (bins.second - bins.first + 1) * binHz;
        const double rfEpsilon = std::max(1.0e-6, binHz * 1.0e-9);
        if (!d->miniWanted.contains(binding.miniSliceId) || !binding.slice
            || binding.slice->sliceIndex() != binding.miniSliceId
            || decoded->sourceStream != binding.slice->streamIndex()
            || binding.observedStreamEpoch != binding.slice->streamEpoch()
            || decoded->transmit.value_or(false) != wantsTx
            || bins.second < bins.first
            || std::abs(decoded->centreHz - cropCentre) > rfEpsilon
            || std::abs(decoded->spanHz - cropSpan) > rfEpsilon
            || decoded->traceSamples <= 0
            || decoded->traceSamples != decoded->waterfallSamples
            || decoded->wideSamples != 0) {
            return;
        }
        emit miniDisplayUnavailable(binding.miniSliceId);
        binding.context = context;
        binding.contextRevision = revision;
        binding.sourceCentreHz = sourceCentre;
        binding.capture = {context, sourceCentre, rate};
        binding.grant = decoded->grant;
        binding.decoder.reset();
        binding.presenter.restartChain();
        binding.lastMiniTrace.clear();
        binding.accepted = true;
        binding.rejected = false;
        binding.transmit = wantsTx;
        binding.transmitGeneration = wantsTx ? context.codec.contextGeneration : 0;
        binding.transmitContext = wantsTx ? decoded : std::nullopt;
        requestKeyframe(endpointId);
        return;
    }
    if (decoded->transmit.value_or(false)) {
        // Parity Task 28 (R-R3-49, A11): the Core's transmit display for
        // this pan while it is keyed. The pan's receive context, grant and
        // status stay as they were; the transmit context and its frames go
        // to whoever draws the transmit display (Task 29).
        binding.contextRevision = revision;
        binding.decoder.reset();
        binding.accepted = true;
        binding.rejected = false;
        binding.transmit = true;
        binding.transmitGeneration = context.codec.contextGeneration;
        binding.transmitContext = *decoded;
        // Merge with R-R3-21: a transmit context that beats the window's
        // own rise also forgets the receive frames waiting to be drawn.
        binding.presenter.reset();
        binding.receivedFrameTimesMs.clear();
        const QPointer<RemoteMediaController> self(this);
        const QString connectionId = d->connectionId;
        emit transmitContextReceived(binding.panId, *decoded);
        if (!self || d->connectionId != connectionId) { return; }
        requestKeyframe(endpointId);
        return;
    }
    binding.transmit = false;
    binding.transmitGeneration = 0;
    binding.transmitContext.reset();
    if (decoded->grant && decoded->grant != binding.grant) {
        qCInfo(lcRemoteMedia).noquote()
            << QStringLiteral("Remote spectrum grant for %1: %2")
                   .arg(binding.panId, grantLogLine(*decoded->grant));
    }
    binding.grant = decoded->grant;
    // A grant short of the request with no limit named is not final: ask
    // for the same request once more. Core then grants it, names a limit,
    // or refuses it (refusedIdentity stops a repeat).
    const bool shortWithoutReason = budgetMode && decoded->grant
        && decoded->grant->limit == SpectrumLimitReason::None
        && decoded->grant->grantedPixels < decoded->grant->requestedPixels;
    binding.askAgain = shortWithoutReason && binding.askedAgain != binding.acceptedRequest;
    const bool askAgain = binding.askAgain;
    binding.context = context;
    binding.contextRevision = revision;
    binding.sourceCentreHz = sourceCentre;
    binding.capture = {context, sourceCentre, rate};
    binding.decoder.reset();
    binding.presenter.restartChain();
    binding.accepted = true;
    binding.rejected = false;
    binding.receivedNoiseFloor = false;
    binding.receivedFrameTimesMs.clear();
    const QPointer<RemoteMediaController> self(this);
    const QString connectionId = d->connectionId;
    const quint32 acceptedContextRevision = revision;
    const QString panId = binding.panId;
    // Parity Task 17 (R-R3-01, R-R3-08): the pan's bin width follows the
    // FFT size the Core runs for it: its grant, or what an older Core that
    // reports none was asked for.
    const int grantedFftSize = decoded->grant ? decoded->grant->grantedFftSize
        : (budgetMode ? binding.acceptedRequest : binding.observed)
              .value(QStringLiteral("fftSize")).toInt();
    binding.widget->setRemoteSpectrumContext(context, sourceCentre, rate, grantedFftSize);
    if (!self || d->connectionId != connectionId) { return; }
    // Show or clear this pan's grant line now rather than on the next refresh.
    refreshPanGrantStatus(panId);
    if (!self || d->connectionId != connectionId) { return; }
    it = d->bindings.find(endpointId);
    if (it == d->bindings.end() || it->second.contextRevision != acceptedContextRevision
        || it->second.context.codec.contextGeneration
            != context.codec.contextGeneration) { return; }
    auto& installed = it->second;
    // The source crop may be bin-aligned. Remember the displayed accepted
    // window so the polling observer does not feed an ACK back as a new zoom.
    if (installed.slice) {
        const QJsonObject normalized = d->request(installed.widget, installed.slice);
        if (budgetMode) {
            installed.desiredOriginal = normalized;
            installed.acceptedRequest.insert(QStringLiteral("centreHz"),
                                           normalized.value(QStringLiteral("centreHz")));
            installed.acceptedRequest.insert(QStringLiteral("spanHz"),
                                           normalized.value(QStringLiteral("spanHz")));
        } else {
            installed.observed = normalized;
        }
    }
    refreshCtunState();
    requestKeyframe(endpointId);
    if (askAgain) {
        if (!self || d->connectionId != connectionId) { return; }
        refreshSubscriptions();
    }
}

void RemoteMediaController::requestKeyframe(quint32 endpointId)
{
    auto it = d->bindings.find(endpointId);
    if (it == d->bindings.end() || !it->second.accepted) { return; }
    auto& binding = it->second;
    const qint64 now = d->clock.elapsed();
    if (now - binding.lastKeyframeMs < 200) { return; }
    binding.lastKeyframeMs = now;
    ++d->displayCounters.keyframeRequests;
    send({{QStringLiteral("op"), QStringLiteral("keyframe")},
          {QStringLiteral("endpointId"), double(endpointId)},
          {QStringLiteral("contextGeneration"), double(binding.acceptedGeneration())}});
}

void RemoteMediaController::receiveDisplay(const QByteArray& packet)
{
    // The direct media ladder: a display packet is media too.
    d->lastMediaMs = d->allocationClock();
    d->silenceFallbackFired = false;
    d->fallbackFinishedMs = -1;
    endWaitingFallback();
    if (packet.startsWith("PS3D")) {
        if (!d->client || !d->model || d->client->capabilities().psDisplayVersion != 1
            || !d->model->pureSignalFacade()->ampViewSubscribed()) {
            return;
        }
        PureSignalSessionFacade* facade = d->model->pureSignalFacade();
        if (d->ps3Generation != facade->displayGeneration()) {
            d->ps3Generation = facade->displayGeneration();
            d->ps3Assembler.reset(d->ps3Generation);
        }
        if (const auto snapshot = d->ps3Assembler.accept(packet)) {
            facade->receiveDisplaySnapshot(*snapshot);
        }
        return;
    }
    if (packet.startsWith("NSDX")) {
        receiveDisplayExtras(packet);
        return;
    }
    // Route only the documented v1 prefix. The decoder validates the complete
    // envelope and bounds before any plane can reach the renderer.
    if (packet.size() < 42 || packet.size() > DisplayCodecEncoder::kMaxEncodedBytes
        || packet.first(4) != QByteArrayLiteral("NSDC")) { return; }
    const quint32 id = qFromBigEndian<quint32>(packet.constData() + 8);
    const quint32 generation = qFromBigEndian<quint32>(packet.constData() + 12);
    auto it = d->bindings.find(id);
    if (it == d->bindings.end() || !it->second.accepted
        || (!it->second.widget && !it->second.isMini())
        || !it->second.slice
        || it->second.observedStream != it->second.slice->streamIndex()
        || it->second.observedStreamEpoch != it->second.slice->streamEpoch()
        || generation != it->second.acceptedGeneration()) { return; }
    if (it->second.isMini()) {
        const auto& mini = it->second;
        const bool budgetMode = d->client->remoteDisplayBudgetLimits().has_value();
        const QJsonObject wanted = d->requestMini(mini.slice);
        if (!d->miniWanted.contains(mini.miniSliceId) || wanted.isEmpty()
            || mini.transmit != (d->model->isTransmitting()
                && d->model->txBoundSlice() == mini.slice)
            || (budgetMode ? !sameOriginalIntent(wanted, mini.acceptedRequest)
                           : wanted != mini.observed)) { return; }
    }
    auto& binding = it->second;
    // R-R3-21: this pan's waits between display messages, the last 10 s.
    const qint64 arrivalNs = d->audio->nowNs();
    if (binding.lastArrivalNs) {
        binding.arrivalGaps.emplace_back(arrivalNs, arrivalNs - *binding.lastArrivalNs);
    }
    binding.lastArrivalNs = arrivalNs;
    while (!binding.arrivalGaps.empty()
           && binding.arrivalGaps.front().first < arrivalNs - kDisplayArrivalWindowNs) {
        binding.arrivalGaps.pop_front();
    }
    const DisplayCodecDecodeResult decoded = binding.decoder.decode(packet);
    if (decoded.disposition == DisplayCodecDisposition::NeedKeyframe) {
        if (binding.transmit && !binding.isMini()) {
            // Merge of parity Task 28 with R-R3-21: the transmit display is
            // drawn on arrival, not on the audio's clock (it has no audio
            // playout to follow), so a lost transmit message only asks for
            // a keyframe and never touches the receive presenter.
            requestKeyframe(id);
            return;
        }
        // A lost message: the rows wait for a keyframe, repeating the last
        // good row while none comes, and blend in when it does.
        if (!binding.presenter.waitingForKeyframe()) {
            ++d->displayCounters.keyframeWaits;
        }
        binding.presenter.noteLoss();
        requestKeyframe(id);
        scheduleDisplayPresentation(d->displayDelay.mapNs());
    } else if (decoded.disposition == DisplayCodecDisposition::Accepted) {
        if (binding.transmit && !binding.isMini()) {
            // Parity Task 28: a transmit display frame, handed on at once.
            // Merge with R-R3-21: it bypasses the presenter (no audio to
            // follow), so it is never stamped against the audio clock.
            const QPointer<RemoteMediaController> self(this);
            const QString connectionId = d->connectionId;
            const QString panId = binding.panId;
            emit transmitFrameReceived(panId, decoded.frame);
            if (!self || d->connectionId != connectionId) { return; }
            emit displayFrameReceived(id);
            return;
        }
        if (!binding.isMini() && d->transmittingPans.contains(binding.panId)) {
            // Parity Task 29 (A11): the pan shows the transmit display; a
            // receive frame now is the receiver hearing its own
            // transmitter. Decoded (the next delta needs it), never drawn:
            // the trace and waterfall hold. Not queued either, so nothing
            // from the over reaches the presenter.
            return;
        }
        const SpectrumEndpointContext& context = binding.context;
        binding.presenter.setRowPeriodNs(context.targetFps > 0 && context.framesPerLine > 0
            ? qint64(context.framesPerLine) * 1'000'000'000LL / context.targetFps : 0);
        const quint64 droppedBefore = binding.presenter.counters().itemsDropped;
        binding.presenter.push(decoded.frame, binding.capture);
        d->displayCounters.itemsDropped += binding.presenter.counters().itemsDropped - droppedBefore;
        reconcileClockProbe();
        presentDueDisplay();
    }
}

// JJ's ruling of 2026-09-28: the Core's display extras for a pan (display
// extras v1, section 3). Only the waterfall levels are used here: a pan whose
// waterfall AGC or NF-AGC the Core runs takes the levels the Core sent for
// its latest line. A datagram for no accepted pan, an older context or
// another shape is dropped, as a stale frame is.
void RemoteMediaController::receiveDisplayExtras(const QByteArray& packet)
{
    if (packet.size() < int(kDisplayExtrasHeaderBytes)) { return; }
    const quint32 id = qFromBigEndian<quint32>(packet.constData() + 8);
    auto it = d->bindings.find(id);
    if (it == d->bindings.end() || !it->second.accepted || it->second.isMini()
        || !it->second.widget || it->second.transmit) { return; }
    auto& binding = it->second;
    const DisplayExtrasDecodeResult decoded = decodeDisplayExtras(packet, binding.context.codec);
    if (!decoded.accepted || !decoded.frame.waterfallLevelsDbm) { return; }
    if (d->transmittingPans.contains(binding.panId)) { return; }
    binding.widget->setCoreWaterfallLevels(decoded.frame.waterfallLevelsDbm->first,
                                           decoded.frame.waterfallLevelsDbm->second);
}

RemoteDisplayTelemetry RemoteMediaController::displayTelemetry() const
{
    RemoteDisplayTelemetry telemetry = d->displayCounters;
    const qint64 now = d->audio->nowNs();
    for (const auto& [id, binding] : d->bindings) {
        for (const auto& [atNs, gapNs] : binding.arrivalGaps) {
            if (atNs < now - kDisplayArrivalWindowNs) { continue; }
            const double gapMs = double(gapNs) / 1'000'000.0;
            if (!telemetry.largestArrivalGapMs || gapMs > *telemetry.largestArrivalGapMs) {
                telemetry.largestArrivalGapMs = gapMs;
            }
        }
        if (binding.widget) {
            telemetry.rowsDropped += binding.widget->remoteRowsDropped();
        }
    }
    if (const std::optional<qint64> map = d->displayDelay.mapNs()) {
        const std::optional<AudioClockOffset> offset =
            d->clockEstimator.offset(d->audio->nowNs());
        if (offset) {
            telemetry.displayDelayMs = double(*map + offset->offsetNs) / 1'000'000.0;
        } else if (d->lastAudioDelayNs) {
            telemetry.displayDelayMs = double(*d->lastAudioDelayNs) / 1'000'000.0;
        }
    }
    return telemetry;
}

bool RemoteMediaController::displayClockNegotiated() const
{
    return audioClockNegotiated()
        && d->client->capabilities().displayClockVersion >= 1;
}

std::optional<qint64> RemoteMediaController::displayMapNs()
{
    // An older Core: each frame is drawn on arrival.
    if (!displayClockNegotiated()) {
        return std::nullopt;
    }
    const qint64 now = d->audio->nowNs();
    const std::optional<AudioClockOffset> offset = d->clockEstimator.offset(now);
    if (offset && d->audioEnabled) {
        const RemoteAudioReceiverTelemetry playback = d->audio->telemetry();
        if (playback.running) {
            AudioDelayInputs inputs;
            inputs.offset = offset;
            inputs.capture = d->captureAnchor;
            inputs.playingGeneration = d->audioGeneration;
            inputs.playout = playback.playout;
            inputs.release = playback.release;
            if (const std::optional<qint64> map = audioPresentationMapNs(inputs)) {
                // The reading's own accuracy sets what counts as a change;
                // a change of the jitter hold is one at once.
                const std::optional<qint64> holdNs = playback.jitterHoldMs
                    ? std::optional<qint64>(std::llround(*playback.jitterHoldMs * 1'000'000.0))
                    : std::nullopt;
                d->displayDelay.observe(*map, now,
                                        playback.playout ? playback.playout->accuracyNs() : 0,
                                        holdNs);
                d->lastAudioDelayNs = *d->displayDelay.mapNs() + offset->offsetNs;
                return d->displayDelay.mapNs();
            }
        }
    }
    if (offset) {
        // No audio playing (muted, no stream, a restart before its first
        // echo). The map needs no offset while it holds: keep it, and take
        // a new one (the last audio delay, or the base one: the one-way trip
        // plus the audio's base hold, against the current offset) only
        // when it differs by more than the offset's own error bound, which
        // is clock drift, not the estimate's noise.
        const qint64 delayNs = d->lastAudioDelayNs.value_or(
            offset->roundTripNs / 2 + AudioJitterBuffer::kHoldNs);
        const qint64 candidate = delayNs - offset->offsetNs;
        const std::optional<qint64> held = d->displayDelay.mapNs();
        if (!held || double(std::llabs(candidate - *held)) > offset->boundNsAt(now)) {
            d->displayDelay.set(candidate, now);
        }
    }
    // Echoes stopped: keep the last map; none yet: draw on arrival.
    return d->displayDelay.mapNs();
}

void RemoteMediaController::presentDueDisplay()
{
    d->presentTimer->stop();
    const std::optional<qint64> map = displayMapNs();
    const qint64 now = d->audio->nowNs();
    const QPointer<RemoteMediaController> self(this);
    const QString connectionId = d->connectionId;
    std::vector<quint32> ids;
    ids.reserve(d->bindings.size());
    for (const auto& [id, binding] : d->bindings) {
        ids.push_back(id);
    }
    for (const quint32 id : ids) {
        auto it = d->bindings.find(id);
        if (it == d->bindings.end()) { continue; }
        std::vector<RemoteDisplayPresenter::Item> items = it->second.presenter.takeDue(now, map);
        for (RemoteDisplayPresenter::Item& item : items) {
            it = d->bindings.find(id);
            if (it == d->bindings.end()) { break; }
            if (it->second.isMini()) {
                auto& mini = it->second;
                const bool currentTx = d->model && mini.slice
                    && d->model->isTransmitting()
                    && d->model->txBoundSlice() == mini.slice;
                const bool budgetMode = d->client
                    && d->client->remoteDisplayBudgetLimits().has_value();
                const QJsonObject wanted = d->requestMini(mini.slice);
                if (!mini.accepted || !mini.slice
                    || !d->miniWanted.contains(mini.miniSliceId)
                    || wanted.isEmpty() || mini.transmit != currentTx
                    || (budgetMode ? !sameOriginalIntent(wanted, mini.acceptedRequest)
                                   : wanted != mini.observed)
                    || mini.slice->sliceIndex() != mini.miniSliceId
                    || mini.observedStream != mini.slice->streamIndex()
                    || mini.observedStreamEpoch != mini.slice->streamEpoch()
                    || item.frame.context.contextGeneration != mini.acceptedGeneration()
                    || item.centreHz != mini.context.exactCentreHz
                    || item.spanHz != mini.context.exactSpanHz
                    || item.frame.waterfallDbm.size() != mini.context.codec.waterfallSamples) {
                    continue;
                }
                if (item.kind == RemoteDisplayPresenter::Kind::Frame) {
                    if (item.frame.traceDbm.size() != mini.context.codec.traceSamples) {
                        continue;
                    }
                    mini.lastMiniTrace = item.frame.traceDbm;
                }
                if (mini.lastMiniTrace.isEmpty()) { continue; }
                emit miniDisplayFrame(mini.miniSliceId, mini.lastMiniTrace,
                    item.frame.waterfallDbm, mini.context.exactCentreHz,
                    mini.context.exactSpanHz, mini.transmit,
                    item.kind == RemoteDisplayPresenter::Kind::Frame
                        ? item.frame.waterfallAdvance : true);
                if (!self || d->connectionId != connectionId) { return; }
                ++d->frames;
                emit displayFrameReceived(id);
                if (!self || d->connectionId != connectionId) { return; }
                continue;
            }
            if (!it->second.widget) { break; }
            // Merge of parity Tasks 28-29 with R-R3-21: the keying hold is
            // checked at presentation time too. While the Core's transmit
            // context holds this binding (acceptedGeneration() is the
            // transmit one) or the pan is keyed, no receive item is drawn.
            // An older receive generation (a tune) still draws at the
            // frequency it was captured at, below.
            if (it->second.transmit || d->transmittingPans.contains(it->second.panId)) {
                continue;
            }
            const QPointer<SpectrumWidget> widget = it->second.widget;
            if (item.kind != RemoteDisplayPresenter::Kind::Frame) {
                if (widget->enqueueRemoteWaterfallRow(item.frame.waterfallDbm, item.frame.wideDbm,
                                                      item.capture)) {
                    if (item.kind == RemoteDisplayPresenter::Kind::Blended) {
                        ++d->displayCounters.rowsBlended;
                    } else {
                        ++d->displayCounters.rowsRepeated;
                    }
                }
                continue;
            }
            const bool rendered = widget->updateRemoteSpectrum(item.frame, item.capture);
            // Rendering emits application signals; a receiver may end this session.
            if (!self || d->connectionId != connectionId) { return; }
            if (!rendered) {
                // Captured before this pan was tuned or renewed: its trace and
                // waterfall row are drawn at the frequency they were captured
                // at, so a tune or drag never blanks the trace (R-R3-21).
                if (widget) {
                    widget->presentRemoteTraceCaptured(item.frame.traceDbm,
                                                       item.centreHz, item.spanHz);
                }
                if (item.frame.waterfallAdvance && widget) {
                    widget->enqueueRemoteWaterfallRow(item.frame.waterfallDbm,
                                                      item.frame.wideDbm,
                                                      item.capture);
                }
                continue;
            }
            it = d->bindings.find(id);
            if (it == d->bindings.end() || !it->second.accepted
                || it->second.acceptedGeneration()
                    != item.frame.context.contextGeneration) { continue; }
            const qint64 receivedAt = d->allocationClock();
            it->second.receivedFrameTimesMs.append(receivedAt);
            while (!it->second.receivedFrameTimesMs.isEmpty()
                   && it->second.receivedFrameTimesMs.constFirst() < receivedAt - 2'000) {
                it->second.receivedFrameTimesMs.removeFirst();
            }
            ++d->frames;
            if (d->frames == 1) { qCInfo(lcRemoteMedia) << "First encrypted remote spectrum frame received"; }
            emit displayFrameReceived(id);
            if (!self || d->connectionId != connectionId) { return; }
        }
    }
    scheduleDisplayPresentation(map);
}

void RemoteMediaController::scheduleDisplayPresentation(std::optional<qint64> mapNs)
{
    const qint64 now = d->audio->nowNs();
    std::optional<qint64> next;
    for (const auto& [id, binding] : d->bindings) {
        if (const std::optional<qint64> due = binding.presenter.nextDueNs(now, mapNs)) {
            next = next ? std::min(*next, *due) : *due;
        }
    }
    if (!next) {
        d->presentTimer->stop();
        return;
    }
    const qint64 waitMs = std::clamp<qint64>((*next - now + 999'999) / 1'000'000, 0, 1'000);
    d->presentTimer->start(int(waitMs));
}
} // namespace NereusSDR
