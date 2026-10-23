// no-port-check: NereusSDR-original. Authenticated GUI subscription lifecycle.
// Modification history (NereusSDR):
//   2026-10-23  J.J. Boyd / KG4VCF  waterfallLevelsFromActualRequestMatchLocal:
//                                    the run level starts at the seed's
//                                    target, -155.5 and -154.3, and the
//                                    local pan runs on the rows' clock.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  waterfallLevelsFromActualRequestMatchLocal:
//                                    the seed starts the floor's fast
//                                    attack, so NF-AGC's low is the AGC's
//                                    run level, -152.2 and -152.32.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  coreWaterfallAgcWindowContainsAcceptedLevels
//                                    also widens the window for an NF-AGC
//                                    low under it. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  waterfallLevelsFromActualRequestMatchLocal:
//                                    the noise floor reads the first row, so
//                                    NF-AGC's low is -155.5 and -155.476 (the
//                                    seed is where the estimate settles, not
//                                    the median), the local pan asks for no
//                                    averaging, and its window needs no
//                                    second request.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  displayTelemetry().rowsBlanked is the
//                                    sum of every bound pan's NB Blank
//                                    count. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  NSDC v2: displayCodecV2StartSkips-
//                                    ContextKeyframes (the start field, no
//                                    context keyframe, held parts, two
//                                    frames a release, lost counts) and
//                                    miniDisplayTakesARowOnlyPartialFrame;
//                                    the tests counting v1 context
//                                    keyframes withhold displayCodec.
//                                    AI-assisted via Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  The stall test also checks the pan's
//                                    own keyframe waits and blended and
//                                    repeated rows. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  The same with the Core folding the
//                                    higher receiver's slice down onto the
//                                    lower one; the rejoin rows install a
//                                    codec. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  The pan keeps its history across an A/B
//                                    switch after the Core puts two slices
//                                    back on one receiver (JJ's ruling
//                                    2026-10-22). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-16  J.J. Boyd / KG4VCF  A display allocation pending across
//                                    a media move is asked again on the
//                                    new connection; a lost re-ask still
//                                    times out and releases. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-10-16  J.J. Boyd / KG4VCF  The rest entry ran past ctest's 300 s
//                                    on the Linux CI runner (no hang: every
//                                    wait is bounded). The display rows
//                                    feed eight I/Q blocks a poll
//                                    (kFeedBlocksPerPoll), so a frame no
//                                    longer waits several polls for its
//                                    FFT, and the three restart backoff
//                                    cases join the audio entry.
//                                    AI-assisted via Claude Code.
//   2026-10-16  J.J. Boyd / KG4VCF  The pixel-grant rows place their pans
//                                    with applyLayout so each fills the
//                                    600 px stack; the crop row's 4 kHz
//                                    overlap is restored. AI-assisted via
//                                    Claude Code.
//   2026-10-16  J.J. Boyd / KG4VCF  2026.10.1 ship blocker: a speaker
//                                    failure is asked for again on its own,
//                                    toasting once, until it plays
//                                    (speakerFaultRetriesOnItsOwnUntilIt-
//                                    Plays); speakers waiting for this
//                                    computer's device list show no problem
//                                    (speakersWaitForTheDeviceListWithout-
//                                    AProblem); the lasting-failure cases
//                                    now expect the quiet retry; a speaker
//                                    problem clears once it plays on a new
//                                    media connection (speakerProblem-
//                                    ClearsOncePlayingOnANewConnection).
//                                    Review round: a format remote playback
//                                    cannot play, a callback too large and a
//                                    decoder that cannot start are not asked
//                                    for again (aSpeakerFormatRemotePlayback-
//                                    CannotPlayIsNotAskedForAgain, lasting-
//                                    SpeakerFaultsAreNotAskedForAgain); a
//                                    failure after a recovery toasts again;
//                                    the retry ends with the session
//                                    (aSpeakerFailureRetryEndsWithThe-
//                                    Session); the device-list wait is
//                                    bounded (aDeviceListThatNeverComes-
//                                    ShowsTheProblemAfterTheWait); the quiet
//                                    retry is waited on, not timed.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-16  J.J. Boyd / KG4VCF  Remote audio rides out short stalls
//                                    (JJ, 2026-10-16): new
//                                    aShortAudioGapKeepsTheContextAndPlaysOn,
//                                    anAudioGapPastTheBoundAsksForAFreshContext
//                                    and aTransportFailureDuringAGapRestarts-
//                                    AtOnce. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-10-15  J.J. Boyd / KG4VCF  A remote window's rigctld key sends
//                                    what programs play into VAX TX, never
//                                    the PC microphone, whichever source
//                                    the window has chosen, with the VAX
//                                    device read by one reader (aRigctld-
//                                    KeySendsVaxTxNeverTheMicrophone).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-14  J.J. Boyd / KG4VCF  Wait for the asynchronous microphone
//                                    pump to start the default-PC preview
//                                    collector after media readiness.
//                                    AI-assisted via OpenAI Codex.
//   2026-10-15  J.J. Boyd / KG4VCF  Fix wave 2: a program key under the
//                                    screen key takes the VAX TX device
//                                    before keyForProgram returns, with no
//                                    second reader (aProgramAudioKeyUnder-
//                                    TheScreenKeyTakesTheVaxDeviceAtOnce);
//                                    the program test and the collector
//                                    pause test wait on conditions, not a
//                                    fixed time. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-15  J.J. Boyd / KG4VCF  Fix wave: a program key leaves the
//                                    VAX TX device to the program and the
//                                    VAX feed after it sends nothing old
//                                    (aProgramAudioKeyLeavesTheVaxDevice-
//                                    ToTheProgram); the VAX test's settle
//                                    wait is a condition. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-15  J.J. Boyd / KG4VCF  VAX as the transmit source sends
//                                    the VAX TX device's audio, at the
//                                    local VAX path's level
//                                    (vaxSourceSendsTheVaxTransmitAudio);
//                                    the failed-capture and VOX-lease cases
//                                    that used VAX as a no-preview source
//                                    now prove VAX takes no capture and
//                                    move the running line onto the
//                                    microphone for the lease.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  A receiver removed on the Core is not
//                                    asked for a display again (a Core with
//                                    a display budget).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  Check cross-DLL signal metadata and
//                                    explicitly simulate VAX availability
//                                    in microphone pause fixtures.
//                                    AI-assisted via OpenAI Codex.
//   2026-10-12: Cover pin deltas arriving after their command result on a
//               budgeted display. J.J. Boyd / KG4VCF, AI-assisted via Codex.
//   2026-10-12  J.J. Boyd / KG4VCF  Fixed-window TX mini admission and
//                                    stale-domain/frame regressions across
//                                    legacy/budget and picture negotiation.
//                                    AI-assisted via OpenAI Codex.
//   2026-10-11  J.J. Boyd / KG4VCF  The key edge hold test (display pill
//                                    plan Task 15) sends its context by
//                                    hand, so it too stands for a Core
//                                    that states no picture.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  Verify collector pause/resume does not
//                                    report an intentional pause as a stall.
//                                    AI-assisted via OpenAI Codex.
//   2026-10-11  J.J. Boyd / KG4VCF  A slice the Core moves to another
//                                    receiver: the pan's next ask is for
//                                    its view on that receiver, in a window
//                                    whose pan follows at once, one whose
//                                    pan follows late, and one that never
//                                    sees the slice move.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF  R-DISP-RETUNE (fourth wave): the
//                                    contexts the window must not draw
//                                    (another stream, a changed stream or
//                                    epoch, transmit, wideband that does
//                                    not fit).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF  R-DISP-RETUNE (third wave): tests that
//                                    send contexts by hand stand for a Core
//                                    that states no picture; a context of
//                                    the picture shown is drawn though it
//                                    answers an earlier request.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-10: the microphone's collector: a pump stalled for 150 ms of
//               the microphone's clock sends all of it afterwards; the
//               collector's thread runs exactly while the capture lease
//               is held. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: Fix round 2: a key retries a Failed capture that another
//               demand holds open; a lost media connection during a
//               program key leaves the next key its lead. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: Review fixes for program keys that bring their own audio:
//               the unkey edge sends no microphone; a link loss leaves the
//               next key its lead; a Failed capture from no demand starts
//               one generation; a program key keeps a lease VOX holds; the
//               lapse case has a plain row that hears the tone; the failed
//               capture case sends silence with no lease and ends its
//               program key at the Core. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-08: TCI program keys that bring their own audio: with no
//               microphone the line carries silence at real time and the
//               Core admits the key; the microphone never reaches the line
//               before, during or after the program's audio; a screen key
//               after it sends the microphone again; a failed capture
//               retries once for a key that sends the microphone; a pump
//               stall sends a bounded catch-up. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-07: R-R3-21, R-R3-51: the silence while the Core transmits
//               holds the speakers' and the headphones' no-packets
//               restarts without a backoff step, and the unkey asks for
//               them again at once; a receiver stream asks for nothing; an
//               older Core, other faults and audio already playing again
//               behave as before.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-05 — J.J. Boyd (KG4VCF). Independent per-pan Clarity ownership.
//                 AI-assisted via OpenAI Codex.
//   2026-10-04: Hold accepted Core waterfall levels in the remote codec window.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04: Genuine delayed allocation results across peer promotion,
//               retired display release and survivor rendering regression.
//               Synchronous stack teardown at the overdue release boundary.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Unkeyed TX-letter shared-pan history
//                 lifecycle regression. AI-assisted via OpenAI Codex.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone uplink.
//               The media start carries remoteTxVersion only to a Core that
//               takes this computer's microphone; the window sends its
//               chosen microphone while it holds transmit, its key is down
//               or it has VOX armed, and never otherwise (counted in
//               packets, and at the Core); the microphone and a program's
//               audio reach the Core's transmit ring over real DTLS/SRTP.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 28 (R-R3-49, A11): the window declares
//               txDisplayVersion to a Core with a TX analyzer, sends the
//               pan's transmit window, and hands the Core's transmit context
//               and frames on without drawing them as receive. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Tasks 27-29 fix wave (R-R3-49): the test runs as two
//               ctest entries, the audio functions (kAudioFunctions) in
//               tst_remote_media_controller_audio and the rest in
//               tst_remote_media_controller (NEREUS_REMOTE_MEDIA_GROUP), so
//               neither carries the other's time against the 120 s limit;
//               run by hand with no group, it runs everything. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: merge of the transmit display with R-R3-21 (R-R3-49,
//               R-R3-21, R-R3-08): transmitDisplayBypassesTheAudioClockPresenter,
//               one part for each behavioural check the display-sync review
//               set for the merge. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: R-R3-49 load round: sharedWindowChangeAndRadioReconnectResumeBothPanes
//               takes its reference trace once the trace holds across new
//               frames, not at the first frame. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media: repeatedAudioRestartsBackOff keeps the
//               Core's display running through the audio outage, as the
//               Core does, so it measures the restart backoff and not the
//               direct path's silence fallback; new
//               directSilenceFallsBackOnceWhileAudioRestartsBackOff for the
//               case that hid (both streams silent on a direct path while
//               the backoff runs). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: new aPlaybackFailureEndsAWaitingRestart: a playback
//               failure while a restart waits leaves nothing waiting.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: aPlaybackFailureEndsAWaitingRestart holds the restart's
//               4 s step instead of racing it;
//               directSilenceFallsBackOnceWhileAudioRestartsBackOff judges
//               the move back to direct by the direct-only schedule's first
//               step (with a coarse timer's 5% slack), not by the silence
//               window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-12: new savedCtunIsNotSentForAReceiverAnotherHolds (a slice
//               that shares the Core's receiver, and one only listened to)
//               and panMenuShowsAHeldCtunSwitchDisabledWithTheReason;
//               remoteCtunProjectionPreservesPreferenceWithoutEcho covers
//               the held reason. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-12: savedCtunIsNotSentForAReceiverAnotherHolds gains a Core
//               from before the holder was named: the window asks once and
//               is refused, as it always was. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-17: CI speed round: the test runs as five ctest entries, not
//               two (TestFunctionGroups): the audio functions in two, every
//               other function in three. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-21  J.J. Boyd (KG4VCF)  A pan's waterfall row reaches its
//                                    widget when its frame is pushed, due
//                                    when its trace is drawn; a key forgets
//                                    rows not yet due (panRowsAreHanded-
//                                    OverEarlyWithTheirDueTime).
//                                    AI-assisted via Claude Code.
//   2026-10-21  J.J. Boyd (KG4VCF)  Fix wave: that case also shows a mini
//                                    display gets row and trace together
//                                    at due; a pan that keeps its history
//                                    as its display moves to the other
//                                    slice forgets the old binding's rows
//                                    not yet due. AI-assisted via Claude
//                                    Code.
#include <QTest>
#include <QApplication>
#include <QMetaMethod>
#include <QComboBox>
#include <QAbstractItemView>
#include "TestFunctionGroups.h"
#include "gui/RemoteAudioWidget.h"
#include "gui/widgets/GuardedSlider.h"
#include <QStandardItemModel>
#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QPointer>
#include <QtEndian>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QThread>
#include <QTimer>
#include <QPushButton>
#include "core/TxSliceArbiter.h"
#include "core/safety/TransmitHolder.h"
#include "gui/applets/TxApplet.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <thread>
#include <utility>
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/DisplayFftSizing.h"
#include "gui/setup/DisplaySetupPages.h"
#include <QCheckBox>
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/ClarityController.h"
#include "gui/PanClarityRegistry.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "core/session/media/DisplayCodec.h"
#include "core/spectrum/SpectrumReducer.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/session/media/WidebandDisplayContext.h"
#include "core/spectrum/DisplayFollowers.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/Ps3DisplayCodec.h"
#include "core/FFTEngine.h"
#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/codec/P2CodecSaturn.h"
#include "core/StepAttenuatorController.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/PathRacer.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteConnectionController.h"
#include "core/SliceOwnership.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "gui/RemoteMediaController.h"
#include "models/RadeWaveformModel.h"
#include "core/session/media/WaveformCodec.h"
#include "gui/RemoteRigctlServer.h"
#include "gui/RemoteTransmitForwarder.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumOverlayMenu.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/CapacityLimitedTransport.h"
#include "fakes/LoopbackTransport.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/RemoteTransmitClient.h"
#include "OperatorWording.h"
#include "gui/OperatorReasonText.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "core/audio/CaptureAudioBus.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/FakeCaptureChild.h"
#include <atomic>
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

class DisplayTransport final : public IMediaTransport {
public:
    explicit DisplayTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions& options) override { startOptions=options; return true; }
    void stop() override { active = false; }
    bool acceptDescription(const QString&, const QString&) override
    {
        if (!descriptionClock.isValid()) { descriptionClock.start(); }
        return true;
    }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& packet) override {
        if (!active) { return false; }
        displayPackets.append(packet);
        if (other) { other->deliver(packet); }
        return true;
    }
    bool sendRtp(const QByteArray&) override { return active; }
    bool isReady() const override { return active; }
    std::optional<MediaIcePath> selectedPath() const override { return route; }
    std::optional<MediaTransportTelemetry> telemetry() const override { return traffic; }
    void activate() { active = true; emit ready(); }
    void deliver(const QByteArray& packet) { emit displayReceived(packet); }
    void failConnection(const QString& reason) { emit connectionFailed(reason); }
    void closeUnexpectedly() { active = false; emit closed(); }
    void reportGenericError(const QString& reason) { emit errorOccurred(reason); }
    DisplaySendResult submitWaveform(const QByteArray& bytes) override {
        if (!active || !other) { return DisplaySendResult::Refused; }
        waveformPackets.append(bytes); emit other->waveformReceived(bytes); return DisplaySendResult::Sent;
    }
    StartOptions startOptions;
    QList<QByteArray> waveformPackets;
    bool active = false;
    // Started when Core's media description first reaches this side.
    QElapsedTimer descriptionClock;
    QPointer<DisplayTransport> other;
    QList<QByteArray> displayPackets;
    std::optional<MediaTransportTelemetry> traffic;
    std::optional<MediaIcePath> route;
};

// A backend that refuses to start, optionally saying why first, as
// LibDataChannelMediaTransport::start does when the peer cannot be built.
class RefusingTransport final : public IMediaTransport {
public:
    RefusingTransport(QObject* parent, QString reason)
        : IMediaTransport(parent), m_reason(std::move(reason)) {}
    bool start(const StartOptions&) override
    {
        if (!m_reason.isEmpty()) { emit errorOccurred(m_reason); }
        return false;
    }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return false; }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return false; }
private:
    QString m_reason;
};

// Core's side of a session that offers media and then never connects: its
// description reaches the GUI, nothing after it does.
class OfferingTransport final : public IMediaTransport {
public:
    explicit OfferingTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions& options) override
    {
        if (options.role == Role::Offerer) {
            QTimer::singleShot(0, this, [this] {
                emit localDescription(QStringLiteral("v=0\r\n"), QStringLiteral("offer"));
            });
        }
        return true;
    }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return false; }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return false; }
};

// The profile each audio control this GUI sent asked for, in order ("" for
// a control without one).
QStringList requestedProfiles(const QSignalSpy& coreControls)
{
    QStringList profiles;
    for (const auto& call : coreControls) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == QLatin1String("audio")) {
            profiles << control.value(QStringLiteral("profile")).toString();
        }
    }
    return profiles;
}

// Puts the stored remote audio choice back to Opus when a test ends.
struct RestoreAudioChoice {
    ~RestoreAudioChoice()
    {
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("Opus"));
    }
};

QStringList g_remoteMediaMessages;
void captureRemoteMediaMessages(QtMsgType, const QMessageLogContext& context,
                                const QString& message)
{
    if (context.category && QByteArray(context.category) == "nereus.remote.media") {
        g_remoteMediaMessages.append(message);
    }
}

namespace {
// I/Q blocks a display test hands the Core per poll of a QTRY wait. One
// block per poll (QTRY polls every 50 ms) fed a few thousand samples a
// poll, far below the stream's 192 kHz, so each display frame waited out
// several polls for its FFT to fill: about 0.3 to 0.6 s a frame, most of
// the time of the display rows, and enough over the suite to pass ctest's
// limit on the CI runner (runs 38974441798 and 38974523656). Eight blocks
// fill an 8192-point FFT in one poll; the waits stay bounded by frames,
// not counted in a fixed time.
constexpr int kFeedBlocksPerPoll = 8;

class ClosingGuiControlTransport final : public Test::LoopbackTransport {
public:
    ClosingGuiControlTransport() : LoopbackTransport(QStringLiteral("client")) {}
    void sendText(const QByteArray& wire) override
    {
        if (!closeOnOp.isEmpty()
            && wire.contains(QByteArray("\"op\":\"") + closeOnOp + '"')) {
            closeOnOp.clear();
            if (beforeClose) { beforeClose(); }
            closeLink(QStringLiteral("test synchronous GUI send closure"));
            return;
        }
        LoopbackTransport::sendText(wire);
    }
    QByteArray closeOnOp;
    std::function<void()> beforeClose;
};

// R-R3-21: a Core from before displayClockVersion: the station's
// capabilities go out without that entry.
class OlderDisplayClockTransport final : public Test::LoopbackTransport {
public:
    OlderDisplayClockTransport() : LoopbackTransport(QStringLiteral("station")) {}
    void sendText(const QByteArray& wire) override
    {
        if (wire.contains("\"displayClockVersion\"")) {
            QJsonObject message = QJsonDocument::fromJson(wire).object();
            QJsonArray kept;
            for (const QJsonValue& entry : message.value(QStringLiteral("properties")).toArray()) {
                if (entry.toObject().value(QStringLiteral("name")).toString()
                    != QLatin1String("displayClockVersion")) {
                    kept.append(entry);
                }
            }
            message.insert(QStringLiteral("properties"), kept);
            ++stripped;
            LoopbackTransport::sendText(QJsonDocument(message).toJson(QJsonDocument::Compact));
            return;
        }
        LoopbackTransport::sendText(wire);
    }
    int stripped = 0;
};

// The command result may precede its coalesced pin delta. Keep that real
// mirror message until the test has observed the result on the old value.
class HoldingCtunDeltaTransport final : public Test::LoopbackTransport {
public:
    HoldingCtunDeltaTransport() : LoopbackTransport(QStringLiteral("station")) {}
    void sendText(const QByteArray& wire) override
    {
        SessionMessage message;
        if (hold && SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::Delta) {
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == "streamCtunPinned") {
                    held.append(wire);
                    return;
                }
            }
        }
        LoopbackTransport::sendText(wire);
    }
    void releaseHeld()
    {
        hold = false;
        const QList<QByteArray> messages = std::exchange(held, {});
        for (const QByteArray& wire : messages) { LoopbackTransport::sendText(wire); }
    }
    bool hold = false;
    QList<QByteArray> held;
};

class HoldingAllocationResultTransport final : public Test::LoopbackTransport {
public:
    HoldingAllocationResultTransport() : LoopbackTransport(QStringLiteral("station")) {}
    void sendText(const QByteArray& wire) override
    {
        if (wire.contains("\"op\":\"allocation-result\"") && !passNext) {
            held.append(wire);
            return;
        }
        passNext = false;
        LoopbackTransport::sendText(wire);
    }
    void passNextAllocationResult() { passNext = true; }
    void releaseHeld()
    {
        const QList<QByteArray> messages = std::exchange(held, {});
        for (const QByteArray& wire : messages) { LoopbackTransport::sendText(wire); }
    }
    QList<QByteArray> held;
    bool passNext = false;
};

class StackTeardownControlTransport final : public Test::LoopbackTransport {
public:
    StackTeardownControlTransport() : LoopbackTransport(QStringLiteral("client")) {}
    void sendText(const QByteArray& wire) override
    {
        if (beforeUnsubscribe && wire.contains("\"op\":\"unsubscribe\"")) {
            const std::function<void()> callback = std::exchange(beforeUnsubscribe, {});
            callback();
        }
        LoopbackTransport::sendText(wire);
    }
    std::function<void()> beforeUnsubscribe;
};

QJsonObject lastControl(const QSignalSpy& spy, const QString& op)
{
    for (auto it = spy.crbegin(); it != spy.crend(); ++it) {
        const QJsonObject control = it->at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == op) { return control; }
    }
    return {};
}
// budgetModePaintsWhenCoreGrantsFewerPixels: what happens to the pan that
// sized a shared engine.
constexpr int kStays = 0;
constexpr int kLeaves = 1;
constexpr int kLeavesCoreRefuses = 2;
// selectingTheOtherSliceOnOneReceiverKeepsThePansHistory: how its two slices
// come to share one receiver.
constexpr int kOneReceiver = 0;       // they always did
constexpr int kJoinedOnTheCore = 1;   // the higher one's slice joins
constexpr int kFoldedOnTheCore = 2;   // the lower one retunes; the other folds
// sliceMovedToAnotherReceiverAsksForTheViewOnThatReceiver: when the pan
// follows its slice, and whether this window sees the slice move at all.
constexpr int kFollowsWithTheUpdate = 0;
constexpr int kFollowsLate = 1;
constexpr int kAwayAndBack = 2;

int countControl(const QSignalSpy& spy, const QString& op)
{
    int count = 0;
    for (const auto& call : spy) {
        if (call.at(0).toJsonObject().value(QStringLiteral("op")) == op) { ++count; }
    }
    return count;
}
QList<QJsonObject> controlsFor(const QSignalSpy& spy, const QString& op)
{
    QList<QJsonObject> controls;
    for (const auto& call : spy) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == op) { controls.append(control); }
    }
    return controls;
}

// A Core from before 2026-10-12 does not say who holds a slice's receiver.
// This is what such a Core sends where a current one sends `wire`: the same
// message with no receiverHolderDeviceId among its values, and no message
// at all where that value was the only change. For
// LoopbackTransport::setRewritesIncoming on the window's end of the link.
QByteArray asACoreThatNamesNoReceiverHolderSends(const QByteArray& wire)
{
    static const QLatin1String kHolder("receiverHolderDeviceId");
    if (!wire.contains(kHolder.data())) { return wire; }
    QJsonObject message = QJsonDocument::fromJson(wire).object();
    for (const QString& list : {QStringLiteral("properties"), QStringLiteral("fields")}) {
        if (!message.value(list).isArray()) { continue; }
        QJsonArray kept;
        for (const QJsonValue& entry : message.value(list).toArray()) {
            if (entry.toObject().value(QStringLiteral("name")).toString() != kHolder) {
                kept.append(entry);
            }
        }
        if (kept.isEmpty() && message.value(QStringLiteral("type")).toString()
                                  == QStringLiteral("delta")) {
            return {};
        }
        message.insert(list, kept);
    }
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

// The station's two-slice tone and this computer's speaker, each paced
// every 10 ms, as the real audio session test drives them.
struct PacedRemoteAudio {
    QTimer source;
    QTimer speaker;

    explicit PacedRemoteAudio(Test::RemoteAudioSessionHarness& h)
    {
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] {
            h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        source.start();
        speaker.start();
    }
    void stop()
    {
        source.stop();
        speaker.stop();
    }
};

// Direct media: the Core's display stream, as it runs in production while
// audio is out. The harness has no panadapter on the Core, so this sends a
// display packet for an endpoint the window never bound (dropped after it
// counts as media) every 100 ms on each of the Core's media connections.
struct CoreDisplayKeepAlive {
    QList<QPointer<LibDataChannelMediaTransport>> transports;
    QTimer timer;

    CoreDisplayKeepAlive()
    {
        timer.setInterval(100);
        QObject::connect(&timer, &QTimer::timeout, &timer, [this] {
            QByteArray packet(42, '\0');
            packet.replace(0, 4, QByteArrayLiteral("NSDC"));
            qToBigEndian<quint32>(0xFFFFFFFFu, packet.data() + 8);
            for (const QPointer<LibDataChannelMediaTransport>& link : std::as_const(transports)) {
                if (link && link->isReady()) { link->sendDisplay(packet); }
            }
        });
    }
    MediaPeer::TransportFactory factory()
    {
        return [this](QObject* parent) -> IMediaTransport* {
            auto* link = new LibDataChannelMediaTransport(parent);
            transports << link;
            return link;
        };
    }
};

// Task 36: the remote window's microphone: an open capture device that
// delivers a tone at its real rate, as the capture helper does (48 kHz mono
// float, as much as the wall clock says has been captured).
class PacedMicrophone final : public IAudioBus {
public:
    PacedMicrophone(float amplitude, double hz) : m_amplitude(amplitude), m_hz(hz) {}
    bool open(const AudioFormat& format) override
    {
        m_format = format;
        m_open = true;
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    qint64 push(const char*, qint64) override { return 0; }
    void flush() override {}
    qint64 pull(char* data, qint64 maxBytes) override
    {
        if (!m_open || data == nullptr || maxBytes < 4) {
            return 0;
        }
        // The helper opens when the uplink starts and closes when it stops:
        // a pull after a pause starts afresh, with nothing kept from it.
        if (!m_clock.isValid() || m_clock.elapsed() - m_lastPullMs > 100) {
            m_clock.start();
            m_delivered = 0;
        }
        m_lastPullMs = m_clock.elapsed();
        const qint64 due = m_clock.elapsed() * 48;
        const qint64 frames = std::min(due - m_delivered, maxBytes / 4);
        if (frames <= 0) {
            return 0;
        }
        auto* out = reinterpret_cast<float*>(data);
        for (qint64 i = 0; i < frames; ++i) {
            out[i] = m_amplitude * static_cast<float>(std::sin(
                2.0 * 3.14159265358979323846 * m_hz * static_cast<double>(m_delivered + i) / 48000.0));
        }
        m_delivered += frames;
        return frames * 4;
    }
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("PacedMicrophone"); }
    AudioFormat negotiatedFormat() const override { return m_format; }

private:
    float m_amplitude;
    double m_hz;
    AudioFormat m_format;
    bool m_open{false};
    QElapsedTimer m_clock;
    qint64 m_lastPullMs{0};
    qint64 m_delivered{0};
};

// 2026-10-15: the remote window's VAX transmit device: what an app writes
// to "NereusSDR TX", 48 kHz interleaved stereo float at its real rate
// (AudioEngine::pullVaxTxMic's contract), a tone of `hz` with a different
// level on each channel, so a downmix that is not the local one shows.
class PacedVaxTxDevice final : public IAudioBus {
public:
    PacedVaxTxDevice(float left, float right, double hz) : m_left(left), m_right(right), m_hz(hz) {}
    bool open(const AudioFormat& format) override
    {
        m_format = format;
        m_open = true;
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    qint64 push(const char*, qint64) override { return 0; }
    void flush() override {}
    qint64 pull(char* data, qint64 maxBytes) override
    {
        // 2026-10-15 (fix wave 2): the device has one reader. Two reads at
        // once, or a read from another thread once a program's pump (the
        // marked thread) reads it, are counted.
        if (m_inside.fetch_add(1) > 0) {
            m_overlaps.fetch_add(1);
        }
        const auto leave = qScopeGuard([this] { m_inside.fetch_sub(1); });
        const Qt::HANDLE program = m_programReader.load();
        if (program != nullptr && QThread::currentThreadId() != program) {
            m_otherReads.fetch_add(1);
        }
        if (!m_open || data == nullptr || maxBytes < 8) {
            return 0;
        }
        if (!m_clock.isValid()) {
            m_clock.start();
        }
        const qint64 due = m_clock.elapsed() * 48;
        const qint64 frames = std::min(due - m_delivered, maxBytes / 8);
        if (frames <= 0) {
            return 0;
        }
        auto* out = reinterpret_cast<float*>(data);
        for (qint64 i = 0; i < frames; ++i) {
            const float s = static_cast<float>(std::sin(
                2.0 * 3.14159265358979323846 * m_hz * static_cast<double>(m_delivered + i) / 48000.0));
            out[2 * i] = m_left * s;
            out[2 * i + 1] = m_right * s;
        }
        m_delivered += frames;
        m_pulled.fetch_add(frames, std::memory_order_relaxed);
        return frames * 8;
    }
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("PacedVaxTxDevice"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    qint64 pulledFrames() const { return m_pulled.load(std::memory_order_relaxed); }
    /// From now on the calling thread is the program's pump, the device's
    /// one reader; `false` ends that.
    void setProgramReaderIsThisThread(bool on)
    {
        m_programReader.store(on ? QThread::currentThreadId() : nullptr);
    }
    int overlappingReads() const { return m_overlaps.load(); }
    int readsBesideTheProgram() const { return m_otherReads.load(); }

private:
    float m_left;
    float m_right;
    double m_hz;
    AudioFormat m_format;
    bool m_open{false};
    QElapsedTimer m_clock;
    qint64 m_delivered{0};
    std::atomic<qint64> m_pulled{0};
    std::atomic<int> m_inside{0};
    std::atomic<int> m_overlaps{0};
    std::atomic<int> m_otherReads{0};
    std::atomic<Qt::HANDLE> m_programReader{nullptr};
};

// The power of `hz` in 48 kHz samples (Goertzel), for telling two tones
// apart.
double tonePower(const std::vector<float>& samples, double hz)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / 48000.0;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (float x : samples) {
        const double s0 = static_cast<double>(x) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

// 2026-10-10: a microphone paced as the PC microphone's reader is
// (CaptureAudioBus::pull), on a clock the test moves: a pull gives at most
// the clock's frames since the last pull plus kPaceSlackFrames, and unused
// credit stops at kPaceCreditCapFrames. 48 kHz mono float.
class ClockPacedMicrophone final : public IAudioBus {
public:
    explicit ClockPacedMicrophone(const qint64* nowFrames) : m_nowFrames(nowFrames) {}
    bool open(const AudioFormat& format) override
    {
        m_format = format;
        m_open = true;
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    qint64 push(const char*, qint64) override { return 0; }
    void flush() override {}
    qint64 pull(char* data, qint64 maxBytes) override
    {
        if (!m_open || data == nullptr || maxBytes < 4) {
            return 0;
        }
        if (!m_started) {
            m_started = true;
            m_lastFrames = *m_nowFrames;
        }
        m_credit = std::min<qint64>(m_credit + (*m_nowFrames - m_lastFrames),
                                    CaptureAudioBus::kPaceCreditCapFrames);
        m_lastFrames = *m_nowFrames;
        const qint64 frames = std::clamp<qint64>(m_credit + CaptureAudioBus::kPaceSlackFrames,
                                                 0, maxBytes / 4);
        m_credit -= frames;
        auto* out = reinterpret_cast<float*>(data);
        for (qint64 i = 0; i < frames; ++i) {
            out[i] = 0.25f;
        }
        return frames * 4;
    }
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("ClockPacedMicrophone"); }
    AudioFormat negotiatedFormat() const override { return m_format; }

private:
    const qint64* m_nowFrames;
    AudioFormat m_format;
    bool m_open{false};
    bool m_started{false};
    qint64 m_lastFrames{0};
    qint64 m_credit{0};
};

void attachRemoteMicrophone(Test::RemoteAudioSessionHarness& h, float amplitude, double hz)
{
    AudioFormat fmt{};
    fmt.sample = AudioFormat::Sample::Float32;
    fmt.channels = 1;
    fmt.sampleRate = 48000;
    auto bus = std::make_unique<PacedMicrophone>(amplitude, hz);
    bus->open(fmt);
    h.remote.audioEngine()->setTxInputBusForTest(std::move(bus));
}

// Task 36: the RMS of `blocks` pump blocks the Core's transmit ring gives,
// from its last `measure` blocks.
double pumpRms(RemoteMicFeed* feed, int blocks, int measure)
{
    std::vector<float> block(RemoteMicConfig::kPumpBlockFrames);
    double sum = 0.0;
    int count = 0;
    for (int b = 0; b < blocks; ++b) {
        feed->pull(block.data(), RemoteMicConfig::kPumpBlockFrames);
        if (b >= blocks - measure) {
            for (float v : block) {
                sum += static_cast<double>(v) * v;
                ++count;
            }
        }
    }
    return count > 0 ? std::sqrt(sum / count) : 0.0;
}

// 2026-10-08: `blocks` pump blocks of the Core's transmit ring, in order.
std::vector<float> pullFeed(RemoteMicFeed* feed, int blocks)
{
    std::vector<float> out;
    std::vector<float> block(RemoteMicConfig::kPumpBlockFrames);
    for (int b = 0; b < blocks; ++b) {
        feed->pull(block.data(), RemoteMicConfig::kPumpBlockFrames);
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

// 2026-10-08: a program keys only while this window holds transmit; the
// window takes it with a TUNE press and release (TUNE needs no
// microphone), as an operator would before starting the program.
bool takeTransmit(Test::RemoteAudioSessionHarness& h)
{
    h.remote.setTune(true);
    if (!QTest::qWaitFor([&h] { return h.station.moxController()->isMox(); }, 5000)) {
        return false;
    }
    h.remote.setTune(false);
    if (!QTest::qWaitFor([&h] { return !h.station.moxController()->isMox(); }, 5000)) {
        return false;
    }
    return h.server.transmitHolder()->isHeldBy(h.windowKey->fingerprint());
}

// 2026-10-08: a program keying through this window's TCI server (its trx
// with ",tci" when `programAudio`), answered by the in-process Core.
std::optional<RemoteTransmitClient::Answer> keyProgram(Test::RemoteAudioSessionHarness& h,
                                                       bool programAudio)
{
    std::optional<RemoteTransmitClient::Answer> answer;
    h.client.remoteTransmit()->keyForProgram(programAudio,
        [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
    QElapsedTimer waited;
    waited.start();
    while (!answer && waited.elapsed() < 5000) {
        QTest::qWait(10);
    }
    return answer;
}

// 2026-10-09: `frames` of a 1 kHz tone at `amplitude`, 48 kHz mono float,
// as a capture bus gives them.
QByteArray toneBytes(int frames, float amplitude)
{
    QByteArray bytes(frames * int(sizeof(float)), Qt::Uninitialized);
    auto* out = reinterpret_cast<float*>(bytes.data());
    for (int i = 0; i < frames; ++i) {
        out[i] = amplitude * static_cast<float>(
            std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0));
    }
    return bytes;
}

// 2026-10-08: a program's audio, `chunks` of 20 ms at 48 kHz, each sample
// `level` (never negative, unlike the test microphone's tone).
void pushProgramChunks(RemoteMediaController& media, int chunks, float level)
{
    const std::vector<float> program(960, level);
    for (int chunk = 0; chunk < chunks; ++chunk) {
        media.pushProgramAudio(program.data(), 960, 1, 48000);
        QTest::qWait(20);
    }
}

// Every audio status the controller announced, in order.
struct AudioStatusHistory {
    QList<RemoteAudioStatus> statuses;
    QMetaObject::Connection connection;

    explicit AudioStatusHistory(RemoteMediaController& media)
    {
        connection = QObject::connect(&media, &RemoteMediaController::audioStatusChanged,
                                      &media, [this, &media] {
            statuses.append(media.audioStatus());
        });
    }
    ~AudioStatusHistory() { QObject::disconnect(connection); }
    AudioStatusHistory(const AudioStatusHistory&) = delete;
    AudioStatusHistory& operator=(const AudioStatusHistory&) = delete;

    // Every status from the first one in `state` on is in `allowed`.
    bool onlyFromFirst(RemoteAudioStatus::State state,
                       const QList<RemoteAudioStatus::State>& allowed) const
    {
        const auto first = std::find_if(statuses.cbegin(), statuses.cend(),
            [state](const RemoteAudioStatus& status) { return status.state == state; });
        if (first == statuses.cend()) { return false; }
        return std::all_of(first, statuses.cend(), [&allowed](const RemoteAudioStatus& status) {
            return allowed.contains(status.state);
        });
    }
};

// The status poll the controller owns; it must run only while needed.
QTimer* audioStatusTimer(RemoteMediaController& media)
{
    return media.findChild<QTimer*>(QStringLiteral("remoteAudioStatusTimer"));
}

// The accepted context is off for `reason` and is not the context
// `generation` (0, never a real generation, matches any context).
bool acceptedOff(const RemoteMediaController& media, quint32 generation,
                 RemoteAudioOffReason reason)
{
    const std::optional<RemoteAudioContextMessage> context = media.acceptedAudioContext();
    return context && context->generation != generation && !context->enabled
        && context->offReason == reason;
}

// What the controller logs when this computer's speaker cannot start remote
// playback because it reports no playback timing (R-R3-23: any rate and
// channel count it offers is accepted).
const char* const kSpeakerOpenFailedLog =
    "Remote audio playback failed: The selected speaker device does not report "
    "its playback timing";

// R-R3-23: the amplitude of a `hz` tone in one channel of audio heard at
// `rateHz` with `channels` interleaved, over its last `frames` frames.
double lastToneAmplitude(const QVector<float>& heard, int channels, int channel, double hz,
                         int rateHz, qint64 frames)
{
    const qint64 total = heard.size() / channels;
    double cosine = 0.0;
    double sine = 0.0;
    qint64 counted = 0;
    for (qint64 frame = std::max<qint64>(0, total - frames); frame < total; ++frame) {
        const double phase = 2.0 * 3.14159265358979323846 * hz * double(frame) / double(rateHz);
        const double sample = heard.at(frame * channels + channel);
        cosine += sample * std::cos(phase);
        sine += sample * std::sin(phase);
        ++counted;
    }
    return counted > 0 ? 2.0 * std::hypot(cosine, sine) / double(counted) : 0.0;
}

// What it logs when a playing speaker stops reporting timing: the worker's
// pacing check or its next write notices first, with the bounded detail.
QRegularExpression speakerTimingLostLog()
{
    return QRegularExpression(QStringLiteral(
        "^Remote audio playback failed: (Speaker device timing became unavailable"
        "|Could not write remote audio to the speaker device) \\[ageMs="));
}

// R-R3-37: the Core accepted this pan's display and it is being shown.
bool showsDisplay(const RemoteMediaController& controller, const PanadapterApplet* applet)
{
    return controller.panDisplayState(applet->panId()).phase
        == PanDisplayState::Phase::Showing;
}

// The pan was refused for exactly `reason`, as the Core sent it, and shows
// that refusal in user words rather than the raw text.
bool refusedFor(const RemoteMediaController& controller, const PanadapterApplet* applet,
                const QString& reason)
{
    const PanDisplayState state = controller.panDisplayState(applet->panId());
    return state.phase == PanDisplayState::Phase::Refused && state.refusalReason == reason
        && !applet->remoteDisplayStatus().isEmpty()
        && OperatorWording::isPlain(applet->remoteDisplayStatus())
        && OperatorWording::isPlain(applet->remoteDisplayExplanation())
        // R-R3-21: the reason in user words; a reason already in user words
        // is shown as sent, one in internal terms never is.
        && applet->remoteDisplayExplanation().contains(OperatorReasonText::forDisplay(reason))
        && (OperatorWording::isPlain(reason)
            || !applet->remoteDisplayExplanation().contains(reason));
}
// R-R3-43: the receiver-audio requests this GUI sent for one slice, in order.
QList<QJsonObject> receiverRequests(const QSignalSpy& coreControls, int sliceId)
{
    QList<QJsonObject> requests;
    for (const auto& call : coreControls) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == QLatin1String("receiver-audio")
            && control.value(QStringLiteral("sliceId")).toInt() == sliceId) {
            requests << control;
        }
    }
    return requests;
}

// R-R3-51: how many requests this GUI sent that turn `op` on (for
// receiver-audio, only those for slice `sliceId`).
int enabledRequests(const QSignalSpy& coreControls, const QString& op, int sliceId = -1)
{
    int count = 0;
    for (const QJsonObject& control : controlsFor(coreControls, op)) {
        if (sliceId >= 0 && control.value(QStringLiteral("sliceId")).toInt() != sliceId) {
            continue;
        }
        if (control.value(QStringLiteral("enabled")).toBool()) { ++count; }
    }
    return count;
}

// R-R3-43: a receiver the status shows as receiving in `profile`.
bool receivesIn(const RemoteMediaController& media, int sliceId, RemoteAudioProfile profile)
{
    for (const RemoteReceiverAudioStatus& receiver : media.audioStatus().receivers) {
        if (receiver.sliceId == sliceId) {
            return receiver.state == RemoteReceiverAudioStatus::State::Receiving
                && receiver.runningProfile == profile;
        }
    }
    return false;
}
} // namespace

class TestRemoteMediaController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-media-controller-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void crossDllSignalsResolveToTheirDeclaredMetadata()
    {
        const QMetaMethod ctun = QMetaMethod::fromSignal(&SpectrumWidget::ctunEnabledChanged);
        QVERIFY(ctun.isValid());
        QCOMPARE(ctun.methodIndex(), SpectrumWidget::staticMetaObject.indexOfSignal(
                                         "ctunEnabledChanged(bool)"));
        const QMetaMethod frequency = QMetaMethod::fromSignal(&SpectrumWidget::frequencyClicked);
        QVERIFY(frequency.isValid());
        QCOMPARE(frequency.methodIndex(), SpectrumWidget::staticMetaObject.indexOfSignal(
                                              "frequencyClicked(double)"));
        const QMetaMethod centre = QMetaMethod::fromSignal(&SpectrumWidget::centerChanged);
        QVERIFY(centre.isValid());
        QCOMPARE(centre.methodIndex(), SpectrumWidget::staticMetaObject.indexOfSignal(
                                           "centerChanged(double)"));
        const QMetaMethod menu = QMetaMethod::fromSignal(&SpectrumOverlayMenu::ctunChanged);
        QVERIFY(menu.isValid());
        QCOMPARE(menu.methodIndex(), SpectrumOverlayMenu::staticMetaObject.indexOfSignal(
                                         "ctunChanged(bool)"));
        const QMetaMethod text = QMetaMethod::fromSignal(&SessionTransport::textReceived);
        QVERIFY(text.isValid());
        QCOMPARE(text.methodIndex(), SessionTransport::staticMetaObject.indexOfSignal(
                                         "textReceived(QByteArray)"));
    }

    void waveformRemoteDemandClockAndIdentity()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("station.settings"));
        RadioModel station; station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5,5,192000); station.setConnectionStateForTest(ConnectionState::Connected);
        const int slice=station.addSlice(); QVERIFY(slice>=0);
        StationServer server(&station,settings,NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true); QVERIFY(server.setDisplayBudgetLimits({10000000,10000000,1}));
        QPointer<DisplayTransport> coreMedia,guiMedia;
        constexpr qint64 offset=1000000000;
        DaemonMediaController core(&server,&station,nullptr,[&](QObject* parent)->IMediaTransport* {
            coreMedia=new DisplayTransport(parent); return coreMedia;
        },[] { return waveformNowNs()+offset; });
        RadioModel remote(RadioModel::Role::Remote); remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy; StationClient client(&remote,&proxy);
        client.withholdFeatureForTest("remoteTx"); // This observer fixture opens no microphone line/capture.
        RemoteMediaController gui(&client,&remote,nullptr,nullptr,[&](QObject* parent)->IMediaTransport* {
            guiMedia=new DisplayTransport(parent); return guiMedia;
        });
        auto* stationLink=new Test::LoopbackTransport("station", &server);
        auto* clientLink=new Test::LoopbackTransport("client", &client);
        stationLink->linkTo(clientLink); client.startSession(clientLink,server.token()); server.acceptTransport(stationLink);
        QTRY_VERIFY(coreMedia && guiMedia && remote.sliceById(slice));
        coreMedia->other=guiMedia; guiMedia->other=coreMedia; coreMedia->activate(); guiMedia->activate();
        QVERIFY(gui.waveformNegotiated()); QVERIFY(guiMedia->startOptions.waveformChannel);
        WaveformPublisher publisher(WaveformKind::DecodedSpeech,24000);
        station.radeWaveformModel()->setEndpoint(slice,WaveformKind::DecodedSpeech,&publisher,UINT64_MAX,true);
        auto* model=remote.radeWaveformModel(); model->setDemand(100,{{slice,WaveformKind::DecodedSpeech}});
        model->setDemand(101,{{slice,WaveformKind::DecodedSpeech}});
        QTRY_VERIFY(publisher.bindingToken()!=0);
        auto* probe=gui.findChild<QTimer*>("remoteAudioClockProbeTimer"); QVERIFY(probe); QVERIFY(probe->isActive());
        QVERIFY(QMetaObject::invokeMethod(probe,"timeout",Qt::DirectConnection));
        QTRY_COMPARE(model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs())->reason,
                     QStringLiteral("Waiting for observed Core PCM"));
        std::vector<float> pcm(240,0.375f);
        QVERIFY(publisher.appendFloat(publisher.bindingToken(),pcm.data(),240,1,waveformNowNs()+offset,true));
        QTRY_VERIFY(!coreMedia->waveformPackets.empty());
        QTRY_VERIFY(!model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs())->samples.empty());
        const auto snapshot=model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs());
        QCOMPARE(snapshot->samples[0],0.375f);
        QVERIFY(std::abs(snapshot->segments[0].startNs-waveformNowNs())<500000000);
        const auto packet=WaveformCodec::decode(coreMedia->waveformPackets[0]); QVERIFY(packet);
        const quint64 before=snapshot->revision;
        emit guiMedia->waveformReceived(coreMedia->waveformPackets[0]); // Duplicate discarded.
        QCOMPARE(model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs())->revision,before);
        WaveformPacket stale=*packet; stale.deliveryGeneration+=1; const auto bad=WaveformCodec::encode(stale); QVERIFY(bad);
        emit guiMedia->waveformReceived(*bad);
        QCOMPARE(model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs())->revision,before);
        WaveformPacket hole=*packet; hole.batch.serial+=2; hole.batch.segments[0].firstSequence+=1000;
        hole.batch.segments[0].startNs+=100000000; const auto gap=WaveformCodec::encode(hole); QVERIFY(gap);
        emit guiMedia->waveformReceived(*gap);
        QVERIFY(model->snapshot(slice,WaveformKind::DecodedSpeech,waveformNowNs())->droppedFrames>=760);
        model->clearDemand(100); QVERIFY(publisher.bindingToken()!=0);
        model->clearDemand(101); QTRY_COMPARE(publisher.bindingToken(),quint64(0)); QVERIFY(!probe->isActive());
        client.disconnectFromStation("test complete");
    }

    void waveformOlderCoreIsUnsupported()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("station.settings")); RadioModel station;
        StationServer server(&station,settings,NereusSDR::Test::seedUpgradedCoreToken(dir.path())); server.setMediaEnabled(true);
        DaemonMediaController core(&server,&station,nullptr,[](QObject* parent)->IMediaTransport* { return new DisplayTransport(parent); });
        RadioModel remote(RadioModel::Role::Remote); remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy; StationClient client(&remote,&proxy); client.withholdFeatureForTest("waveform");
        RemoteMediaController gui(&client,&remote,nullptr,nullptr,[](QObject* parent)->IMediaTransport* { return new DisplayTransport(parent); });
        auto* stationLink=new Test::LoopbackTransport("station", &server);
        auto* clientLink=new Test::LoopbackTransport("client", &client);
        stationLink->linkTo(clientLink); client.startSession(clientLink,server.token()); server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete()); QVERIFY(!gui.waveformNegotiated());
        const auto snapshot=remote.radeWaveformModel()->snapshot(0,WaveformKind::RawMicrophone,waveformNowNs());
        QCOMPARE(snapshot->availability,WaveformAvailability::Unsupported); QVERIFY(!snapshot->reason.isEmpty());
        client.disconnectFromStation("test complete");
    }

    void pureSignalChunksShareMediaWithoutChangingMessageLimit()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("station.settings"));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> coreMedia;
        DaemonMediaController core(&server, &station, nullptr,
            [&coreMedia](QObject* owner) -> IMediaTransport* {
                coreMedia = new DisplayTransport(owner);
                return coreMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> guiMedia;
        RemoteMediaController gui(&client, &remote, nullptr, nullptr,
            [&guiMedia](QObject* owner) -> IMediaTransport* {
                guiMedia = new DisplayTransport(owner);
                return guiMedia;
            });
        auto* stationLink = new Test::LoopbackTransport("station");
        auto* clientLink = new Test::LoopbackTransport("client");
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(coreMedia && guiMedia);
        coreMedia->other = guiMedia;
        guiMedia->other = coreMedia;
        coreMedia->activate();
        guiMedia->activate();
        QVERIFY(!gui.currentNetworkPath());
        MediaIcePath selected;
        selected.localType = QStringLiteral("host");
        selected.remoteType = QStringLiteral("srflx");
        selected.localAddress = QStringLiteral("2001:db8::8");
        selected.localPort = 42000;
        selected.remoteAddress = QStringLiteral("192.0.2.8");
        selected.remotePort = 42001;
        guiMedia->route = selected;
        QTRY_VERIFY(gui.currentNetworkPath());
        QCOMPARE(gui.currentNetworkPath()->localPort, quint16(42000));
        QCOMPARE(gui.currentNetworkPath()->remoteAddress, QStringLiteral("192.0.2.8"));
        guiMedia->route->remoteAddress = QStringLiteral("2001:db8::9");
        QCOMPARE(gui.currentNetworkPath()->remoteAddress, QStringLiteral("2001:db8::9"));
        QCOMPARE(client.capabilities().psDisplayVersion, 1);
        PureSignalSessionFacade* coreFacade = station.pureSignalFacade();
        PureSignalSessionFacade* guiFacade = remote.pureSignalFacade();
        QTRY_COMPARE(guiFacade->displayGeneration(), coreFacade->displayGeneration());
        Ps3Snapshot frame;
        frame.channelId = 3;
        frame.sessionGeneration = coreFacade->displayGeneration();
        frame.sequence = 1;
        frame.sampleCount = Ps3Snapshot::kMaxSampleCount;
        frame.correctionCount = Ps3Snapshot::kMaxCorrectionCount;
        frame.x.assign(frame.sampleCount, 0.5);
        frame.ym.assign(frame.sampleCount, 0.45);
        frame.yc.assign(frame.sampleCount, 1.0);
        frame.ys.assign(frame.sampleCount, 0.0);
        frame.xmCorrection.assign(frame.correctionCount, 0.6);
        frame.ymCorrection.assign(frame.correctionCount, 0.65);
        frame.xaCorrection.assign(frame.correctionCount, 0.7);
        frame.yaCorrection.assign(frame.correctionCount, 2.0);
        // Only the DSP sample source is synthetic. Authenticated control,
        // subscription, chunk scheduling, peer bounds and GUI assembly are real.
        emit coreFacade->displaySnapshotReady(frame);
        QCoreApplication::processEvents();
        QVERIFY(coreMedia->displayPackets.isEmpty());
        guiFacade->setAmpViewSubscribed(true);
        QTRY_VERIFY(coreFacade->remoteAmpViewSubscribed());
        emit coreFacade->displaySnapshotReady(frame);
        QTRY_VERIFY(guiFacade->displaySnapshot().has_value());
        const Ps3Snapshot received = *guiFacade->displaySnapshot();
        QCOMPARE(received.sequence, 1u);
        QCOMPARE(received.sampleCount, 4096);
        QCOMPARE(received.xaCorrection.at(0), 0.7);
        QCOMPARE(received.xmCorrection.at(0), 0.6);
        const QList<QByteArray> expected = Ps3DisplayCodec::encode(frame);
        QCOMPARE(coreMedia->displayPackets, expected);
        for (const QByteArray& packet : coreMedia->displayPackets) {
            QVERIFY(packet.size() <= 64 * 1024);
        }
        guiFacade->setAmpViewSubscribed(false);
        QTRY_VERIFY(!coreFacade->remoteAmpViewSubscribed());
        ++frame.sequence;
        emit coreFacade->displaySnapshotReady(frame);
        QCoreApplication::processEvents();
        QCOMPARE(coreMedia->displayPackets.size(), expected.size());
        client.disconnectFromStation(QStringLiteral("test completed"));
        QVERIFY(!gui.currentNetworkPath());
        QVERIFY(!guiFacade->displaySnapshot());
    }

    void preReadyTypedFailureRequestsRecovery()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(media);
        const quint32 epoch = client.sessionEpoch();

        // PeerFailed is transient connectivity even before ICE reaches ready.
        media->failConnection(QStringLiteral("media peer connection failed"));
        QTRY_COMPARE(recoveries.size(), 1);
        QCOMPARE(recoveries.constFirst().at(0).toUInt(), epoch);
    }

    // The Core tells this computer when it dropped its media peer on its own
    // (the whole-peer refusal with the Core's drop reason): media starts
    // over at once through recovery, as when this computer's own peer
    // fails. Every other whole-peer refusal still settles for good.
    void coreDroppedPeerStartsMediaOverOtherRefusalsSettle_data()
    {
        QTest::addColumn<QString>("reason");
        QTest::addColumn<bool>("recovers");
        QTest::newRow("lost") << QString::fromLatin1(kMediaPeerLostReason) << true;
        QTest::newRow("closed") << QString::fromLatin1(kMediaPeerClosedReason) << true;
        QTest::newRow("could-not-start")
            << QStringLiteral("The Core could not start audio and display.") << false;
    }

    void coreDroppedPeerStartsMediaOverOtherRefusalsSettle()
    {
        QFETCH(QString, reason);
        QFETCH(bool, recovers);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        QSignalSpy errors(&controller, &RemoteMediaController::errorOccurred);
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        QTRY_COMPARE(countControl(controls, QStringLiteral("start")), 1);
        media->activate();
        const quint32 epoch = client.sessionEpoch();
        const QJsonValue connectionId =
            lastControl(controls, QStringLiteral("start")).value(QStringLiteral("connectionId"));

        // Exactly what DaemonMediaController sends for a whole peer.
        QVERIFY(server.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("rejected")},
            {QStringLiteral("connectionId"), connectionId},
            {QStringLiteral("endpointId"), 0},
            {QStringLiteral("revision"), 0},
            {QStringLiteral("reason"), reason}}, server.mediaSessionEpoch()));
        QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(errors.constFirst().at(0).toString(), reason);
        QTRY_VERIFY(!media);
        if (recovers) {
            QTRY_COMPARE(recoveries.size(), 1);
            QCOMPARE(recoveries.constFirst().at(0).toUInt(), epoch);
        } else {
            QCoreApplication::processEvents();
            QCOMPARE(recoveries.size(), 0);
        }
        client.disconnectFromStation(QStringLiteral("test completed"));
    }

    void diagnosticConsumerMayDeleteControllerDuringRecovery()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        QPointer<RemoteMediaController> controller = new RemoteMediaController(
            &client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        int recoveries = 0;
        connect(controller, &RemoteMediaController::recoveryRequested,
                this, [&recoveries](quint32, const QString&) { ++recoveries; });
        connect(controller, &RemoteMediaController::errorOccurred,
                this, [controller](const QString&) { delete controller.data(); });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(media);
        media->activate();

        media->closeUnexpectedly();
        QVERIFY(controller.isNull());
        QCOMPARE(recoveries, 0);
    }

    void establishedMediaCloseRequestsOneEpochScopedRecovery()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);

        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(media);
        media->activate();
        const quint32 epoch = client.sessionEpoch();

        media->closeUnexpectedly();
        QTRY_COMPARE(recoveries.size(), 1);
        QCOMPARE(recoveries.constFirst().at(0).toUInt(), epoch);
        QVERIFY(recoveries.constFirst().at(1).toString().contains(
            QStringLiteral("media"), Qt::CaseInsensitive));

        // A terminal failure may be followed by the backend's closed event.
        // The controller must request one full-session recovery, not two.
        if (media) {
            media->failConnection(QStringLiteral("media peer connection failed"));
            media->closeUnexpectedly();
        }
        QCoreApplication::processEvents();
        QCOMPARE(recoveries.size(), 1);
    }

    void displayDropsAreCountedApartFromBytesAndReported()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        qint64 nowMs = 1'000;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            },
            [&nowMs] { return nowMs; });
        QCOMPARE(controller.displayMessagesDropped(), quint64(0));

        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(media);
        media->activate();

        g_remoteMediaMessages.clear();
        const QtMessageHandler previous = qInstallMessageHandler(captureRemoteMediaMessages);
        const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });
        const auto reports = [] {
            return g_remoteMediaMessages.filter(QStringLiteral("Remote display: skipped"));
        };

        MediaTransportTelemetry traffic;
        traffic.receivedDisplayPayloadBytes = 90'000;
        media->traffic = traffic;
        media->deliver(QByteArrayLiteral("not-a-display-frame"));
        QCOMPARE(controller.displayMessagesDropped(), quint64(0));
        QVERIFY(reports().isEmpty());

        // Drops are their own count; received bytes still count every arrival.
        traffic.displayMessagesDropped = 3;
        media->traffic = traffic;
        media->deliver(QByteArrayLiteral("not-a-display-frame"));
        QCOMPARE(controller.displayMessagesDropped(), quint64(3));
        QCOMPARE(controller.trafficTelemetry()->traffic.receivedDisplayPayloadBytes,
                 quint64(90'000));
        QCOMPARE(reports().size(), 1);
        QCOMPARE(reports().constLast(),
                 QStringLiteral("Remote display: skipped 3 late updates on this computer "
                                "to keep the picture current (3 this session)"));

        // At most one report every ten seconds.
        traffic.displayMessagesDropped = 5;
        media->traffic = traffic;
        nowMs += 1'000;
        media->deliver(QByteArrayLiteral("not-a-display-frame"));
        QCOMPARE(reports().size(), 1);
        nowMs += 10'000;
        media->deliver(QByteArrayLiteral("not-a-display-frame"));
        QCOMPARE(reports().size(), 2);
        QCOMPARE(reports().constLast(),
                 QStringLiteral("Remote display: skipped 2 late updates on this computer "
                                "to keep the picture current (5 this session)"));

        client.disconnectFromStation(QStringLiteral("test complete"));
        QTRY_COMPARE(controller.displayMessagesDropped(), quint64(0));
    }

    void deliberateSessionEndAndGenericMediaErrorDoNotRequestRecovery()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        auto connectSession = [&] {
            auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
            auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
            stationLink->linkTo(clientLink);
            client.startSession(clientLink, server.token());
            server.acceptTransport(stationLink);
            QTRY_VERIFY(client.isHandshakeComplete());
            QTRY_VERIFY(media);
            media->activate();
        };

        connectSession();
        media->reportGenericError(QStringLiteral("invalid media packet"));
        QCoreApplication::processEvents();
        QCOMPARE(recoveries.size(), 0);

        client.disconnectFromStation(QStringLiteral("reset after generic error"));
        connectSession();
        QVERIFY(media && media->active);
        // This disconnect retires a live current media peer. Its local stop
        // must not be reclassified as an unexpected transport close.
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QCoreApplication::processEvents();
        QCOMPARE(recoveries.size(), 0);
    }

    // R-R3-28. Media that never reaches ready is bounded in two stages and
    // then enters the same epoch-scoped recovery a typed failure does.
    // Core's media description must arrive within one control heartbeat
    // interval; once it has, the pinned library's own slowest serial
    // failure report bounds the connection (review minor 1).
    void mediaThatNeverConnectsRequestsRecoveryAtTheDeadline()
    {
        // The derivations, checked against their named sources.
        QCOMPARE(RemoteMediaController::kMediaDescriptionDeadlineMs,
                 StationClient::kDefaultHeartbeatIntervalMs);
        // ICE 39.5 s + DTLS 31 s + SCTP 35 s; see the constant's derivation.
        QCOMPARE(RemoteMediaController::kMediaConnectDeadlineMs, 39'500 + 31'000 + 35'000);
        QCOMPARE(RemoteMediaController::kMediaEstablishmentDeadlineMs,
                 RemoteMediaController::kMediaDescriptionDeadlineMs
                     + RemoteMediaController::kMediaConnectDeadlineMs);

        constexpr int kDescriptionMs = 150;
        constexpr int kConnectMs = 1500;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        // Core answers media only when a test case wants its description.
        bool coreOffers = false;
        std::unique_ptr<DaemonMediaController> core;
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            }, {}, 10'000, kDescriptionMs, kConnectMs);
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        QSignalSpy errors(&controller, &RemoteMediaController::errorOccurred);
        // Started before the session, so it can only overstate the time
        // since media started. The establish timer is a precise timer, so
        // it does not fire early; the 20 ms covers millisecond rounding and
        // the gap between these clocks starting and the timer being armed.
        QElapsedTimer sinceStart;
        const auto connectSession = [&] {
            media = nullptr;
            if (coreOffers && !core) {
                core = std::make_unique<DaemonMediaController>(&server, &station, nullptr,
                    [](QObject* owner) -> IMediaTransport* {
                        return new OfferingTransport(owner);
                    });
            }
            auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
            auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
            stationLink->linkTo(clientLink);
            sinceStart.start();
            client.startSession(clientLink, server.token());
            server.acceptTransport(stationLink);
            QTRY_VERIFY(client.isHandshakeComplete());
            QTRY_VERIFY(media);
        };

        // Stage one: no description from Core within the first stage.
        connectSession();
        const quint32 silentEpoch = client.sessionEpoch();
        QTRY_COMPARE_WITH_TIMEOUT(recoveries.size(), 1, 5000);
        QVERIFY(sinceStart.elapsed() >= kDescriptionMs - 20);
        QVERIFY2(sinceStart.elapsed() < kConnectMs, qPrintable(QString::number(sinceStart.elapsed())));
        QCOMPARE(recoveries.constLast().at(0).toUInt(), silentEpoch);
        QCOMPARE(recoveries.constLast().at(1).toString(),
                 QStringLiteral("Core sent no station media description within 0.15 seconds"));
        QCOMPARE(errors.size(), 1);
        // One recovery per establishment, not one per elapsed deadline.
        QTest::qWait(kDescriptionMs * 2);
        QCOMPARE(recoveries.size(), 1);

        // Stage two: the description arrives, then nothing. The first stage
        // stands down and the connection gets the whole second stage,
        // counted from the description.
        client.disconnectFromStation(QStringLiteral("next case"));
        coreOffers = true;
        connectSession();
        const quint32 offeredEpoch = client.sessionEpoch();
        QTRY_VERIFY(media && media->descriptionClock.isValid());
        // The recovery this stage waits for stops the media peer, which
        // deleteLater()s this transport; the event loop QTRY spins runs
        // that delete, so `media` can be gone by the time the wait returns
        // (R-R3-21: 3 of 3 SIGSEGV on Linux). The clock starts once and is
        // never restarted, so a copy taken now measures the same interval.
        const QElapsedTimer descriptionClock = media->descriptionClock;
        QTRY_COMPARE_WITH_TIMEOUT(recoveries.size(), 2, 5000);
        QVERIFY2(descriptionClock.elapsed() >= kConnectMs - 20,
                 qPrintable(QString::number(descriptionClock.elapsed())));
        QCOMPARE(recoveries.constLast().at(0).toUInt(), offeredEpoch);
        QCOMPARE(recoveries.constLast().at(1).toString(),
                 QStringLiteral("Station media did not connect within 1.5 seconds"));
        QCOMPARE(errors.size(), 2);
        QTest::qWait(kDescriptionMs * 2);
        QCOMPARE(recoveries.size(), 2);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-28. The deadline never pre-empts what the library reports, never
    // fires once media is ready, is cancelled by a deliberate end, and a
    // retired session's deadline cannot fire into a newer session.
    void establishmentDeadlineYieldsToReadyTypedFailureAndDisconnect()
    {
        constexpr int kDeadlineMs = 150;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QPointer<DisplayTransport> media;
        // No Core media controller answers, so the first stage is the one
        // that runs.
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            }, {}, 10'000, kDeadlineMs, kDeadlineMs * 10);
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        QElapsedTimer sinceStart;
        auto connectSession = [&] {
            media = nullptr;
            auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
            auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
            stationLink->linkTo(clientLink);
            sinceStart.start();
            client.startSession(clientLink, server.token());
            server.acceptTransport(stationLink);
            QTRY_VERIFY(client.isHandshakeComplete());
            QTRY_VERIFY(media);
        };

        // The library's own typed reason arrives first and is the one reported.
        connectSession();
        media->failConnection(QStringLiteral("media peer connection failed"));
        QTRY_COMPARE(recoveries.size(), 1);
        QCOMPARE(recoveries.constFirst().at(1).toString(),
                 QStringLiteral("media peer connection failed"));
        QTest::qWait(kDeadlineMs * 3);
        QCOMPARE(recoveries.size(), 1);

        // Media that becomes ready is established; the deadline stands down.
        client.disconnectFromStation(QStringLiteral("next case"));
        connectSession();
        media->activate();
        QTest::qWait(kDeadlineMs * 3);
        QCOMPARE(recoveries.size(), 1);

        // Manual Disconnect during the wait cancels it.
        client.disconnectFromStation(QStringLiteral("next case"));
        connectSession();
        QTest::qWait(kDeadlineMs / 2);
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QTest::qWait(kDeadlineMs * 3);
        QCOMPARE(recoveries.size(), 1);

        // A newer session's deadline is its own. The retired session's wait
        // is still half run when the newer one starts; were it left armed
        // it would fire about half a deadline into the newer session.
        connectSession();
        QTest::qWait(kDeadlineMs / 2);
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        connectSession();
        const quint32 newer = client.sessionEpoch();
        QTRY_COMPARE_WITH_TIMEOUT(recoveries.size(), 2, 5000);
        // sinceStart restarted with the newer session, before its media.
        QVERIFY2(sinceStart.elapsed() >= kDeadlineMs - 20,
                 qPrintable(QString::number(sinceStart.elapsed())));
        QCOMPARE(recoveries.constLast().at(0).toUInt(), newer);
        QTest::qWait(kDeadlineMs * 3);
        QCOMPARE(recoveries.size(), 2);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-28, amended 2026-09-23. A start refusal is retried only when the
    // transport could not be built (the factory threw, or the transport
    // threw building its peer); every other refusal is permanent and keeps
    // stop-and-error with the reason shown and control left up.
    void backendStartRefusalRetriesOnlyWhenTheTransportCouldNotBeBuilt()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        enum class Build { ThrowsBuildingPeer, FactoryThrows, RefusesWithoutError, NoTransport };
        Build build = Build::ThrowsBuildingPeer;
        int transportsBuilt = 0;
        RemoteMediaController controller(&client, &remote, nullptr, nullptr,
            [&build, &transportsBuilt](QObject* owner) -> IMediaTransport* {
                ++transportsBuilt;
                switch (build) {
                case Build::ThrowsBuildingPeer:
                    // LibDataChannelMediaTransport::start's catch: the
                    // peer could not be built, reported, then refused.
                    return new RefusingTransport(owner,
                        QStringLiteral("could not create the peer connection"));
                case Build::FactoryThrows:
                    throw std::runtime_error("no transport");
                case Build::RefusesWithoutError:
                    // A precondition refusal, like an SSRC of zero.
                    return new RefusingTransport(owner, QString());
                case Build::NoTransport:
                    return nullptr;
                }
                return nullptr;
            });
        QSignalSpy recoveries(&controller, &RemoteMediaController::recoveryRequested);
        QSignalSpy errors(&controller, &RemoteMediaController::errorOccurred);
        auto connectSession = [&] {
            auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
            auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
            stationLink->linkTo(clientLink);
            client.startSession(clientLink, server.token());
            server.acceptTransport(stationLink);
            QTRY_VERIFY(client.isHandshakeComplete());
        };

        // Transient: the transport threw while building its peer.
        connectSession();
        QTRY_COMPARE(recoveries.size(), 1);
        QCOMPARE(transportsBuilt, 1);
        QCOMPARE(recoveries.constLast().at(0).toUInt(), client.sessionEpoch());
        QCOMPARE(recoveries.constLast().at(1).toString(),
                 QStringLiteral("Station media could not start on this computer: "
                                "could not create the peer connection"));
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.constLast().at(0).toString(), recoveries.constLast().at(1).toString());

        // Transient: the factory itself threw.
        client.disconnectFromStation(QStringLiteral("next case"));
        build = Build::FactoryThrows;
        connectSession();
        QTRY_COMPARE(recoveries.size(), 2);
        QCOMPARE(recoveries.constLast().at(1).toString(),
                 QStringLiteral("Station media could not start on this computer: "
                                "media transport factory failed"));
        QCOMPARE(errors.size(), 2);

        // Permanent: a refusal without an error, as for an SSRC of zero.
        // Stop and error, once, with the reason; control stays up.
        client.disconnectFromStation(QStringLiteral("next case"));
        build = Build::RefusesWithoutError;
        connectSession();
        QTRY_COMPARE(errors.size(), 3);
        QCOMPARE(errors.constLast().at(0).toString(),
                 QStringLiteral("Station media could not start on this computer"));
        QCoreApplication::processEvents();
        QCOMPARE(recoveries.size(), 2);
        QVERIFY(client.isHandshakeComplete());
        QCOMPARE(controller.activeEndpointCount(), 0);

        // Permanent: the factory returned no transport.
        client.disconnectFromStation(QStringLiteral("next case"));
        build = Build::NoTransport;
        connectSession();
        QTRY_COMPARE(errors.size(), 4);
        QCOMPARE(errors.constLast().at(0).toString(),
                 QStringLiteral("Station media could not start on this computer: "
                                "media transport factory returned an invalid object"));
        QCoreApplication::processEvents();
        QCOMPARE(recoveries.size(), 2);
        QVERIFY(client.isHandshakeComplete());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void remoteCtunProjectionPreservesPreferenceWithoutEcho()
    {
        SpectrumWidget widget;
        widget.setCtunEnabled(true);
        QSignalSpy gestures(&widget, &SpectrumWidget::ctunEnabledChanged);
        QSignalSpy centres(&widget, &SpectrumWidget::centerChanged);
        QVERIFY(gestures.isValid());
        QVERIFY(centres.isValid());
        widget.applyRemoteCtunState(false, false);
        QVERIFY(!widget.ctunAvailable());
        QVERIFY(!widget.ctunEnabled());
        QVERIFY(widget.ctunPreference());
        widget.setCtunEnabled(true); // An unsupported Core cannot pretend to pin.
        QVERIFY(!widget.ctunEnabled());
        widget.applyRemoteCtunState(true, true);
        QVERIFY(widget.ctunEnabled());
        QCOMPARE(gestures.size(), 0);
        QCOMPARE(centres.size(), 0);

        // A receiver whose C-Tune setting is another holder's: the pan
        // shows the receiver's setting, and its own switch changes neither
        // that nor the saved setting.
        const QString held = QStringLiteral(
            "This panadapter shows Shack PC's receiver. Its C-Tune setting is Shack PC's.");
        widget.applyRemoteCtunState(true, false, held);
        QVERIFY(widget.ctunAvailable());
        QCOMPARE(widget.ctunHeldReason(), held);
        QVERIFY(!widget.ctunEnabled());
        widget.setCtunEnabled(true);
        QVERIFY(!widget.ctunEnabled());
        widget.applyRemoteCtunState(true, true, held);
        QVERIFY(widget.ctunEnabled());
        widget.setCtunEnabled(false);
        QVERIFY(widget.ctunEnabled());
        QVERIFY(widget.ctunPreference());
        // No Core to pin on: that reason stands alone.
        widget.applyRemoteCtunState(false, false, held);
        QVERIFY(widget.ctunHeldReason().isEmpty());
        widget.applyRemoteCtunState(true, true);
        QVERIFY(widget.ctunHeldReason().isEmpty());
        widget.setCtunEnabled(false);
        QVERIFY(!widget.ctunEnabled());
        QVERIFY(!widget.ctunPreference());
        QCOMPARE(gestures.size(), 1);
    }

    // The pan menu's C-Tune switch on a receiver whose setting is another
    // holder's: shown, disabled, with the reason, and showing the
    // receiver's setting.
    void panMenuShowsAHeldCtunSwitchDisabledWithTheReason()
    {
        SpectrumOverlayMenu menu;
        QCheckBox* ctun = nullptr;
        for (QCheckBox* box : menu.findChildren<QCheckBox*>()) {
            if (box->text().startsWith(QStringLiteral("CTUN"))) { ctun = box; }
        }
        QVERIFY(ctun);
        QSignalSpy changes(&menu, &SpectrumOverlayMenu::ctunChanged);
        QVERIFY(changes.isValid());
        const QString held = QStringLiteral(
            "This panadapter shows Shack PC's receiver. Its C-Tune setting is Shack PC's.");
        menu.setCtunAvailable(true, held);
        menu.setCtunState(true);
        QVERIFY(!ctun->isHidden());
        QVERIFY(!ctun->isEnabled());
        QVERIFY(ctun->isChecked());
        QCOMPARE(ctun->toolTip(), held);
        menu.setCtunAvailable(true);
        QVERIFY(ctun->isEnabled());
        QVERIFY(ctun->toolTip() != held);
        const QString free = ctun->toolTip();
        // No Core to pin on: its own reason, whatever the holder.
        menu.setCtunAvailable(false, held);
        QVERIFY(!ctun->isEnabled());
        QVERIFY(ctun->toolTip() != held);
        QVERIFY(ctun->toolTip() != free);
        QCOMPARE(changes.size(), 0);
    }

    void acceptedWidebandContextControlsRemoteZoomWithoutLocalDemand()
    {
        SpectrumWidget widget;
        widget.resize(500, 300);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setConnectionState(ConnectionState::Connected);
        widget.setExtendedViewAllowed(true);
        widget.setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        // This single-frame identity fixture uses one 3D row per accepted frame.
        widget.setDssRowDivider(1);
        widget.setWfUpdatePeriodMs(20);
        QSignalSpy localDemand(&widget, &SpectrumWidget::widebandExtensionStateChanged);

        SpectrumEndpointContext context;
        context.codec = {41, 1, -180, 0, 128, 128, 96};
        context.exactCentreHz = 14225000;
        context.exactSpanHz = 192000;
        context.wideCentreHz = context.exactCentreHz;
        context.wideSpanHz = 96000;
        context.wideband.available = true;
        context.wideband.active = false;
        context.wideband.physicalAdcIndex = 1;
        context.wideband.filterChainIndex = 0;
        context.wideband.adcRateHz = 4000000;
        widget.setRemoteSpectrumContext(context, context.exactCentreHz, 192000);

        QVERIFY(widget.remoteWidebandAvailable());
        QVERIFY(!widget.remoteWidebandActive());
        QCOMPARE(widget.maxZoomOutBandwidthHz(), 2000000.0);
        QVERIFY(!widget.extendedMode());
        QCOMPARE(localDemand.size(), 0);

        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(128, -80);
        frame.waterfallDbm = QVector<float>(128, -120);
        frame.wideDbm = QVector<float>(96, -105);
        frame.waterfallAdvance = true;
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QTRY_COMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(widget.dssNewestRowWideBandwidthForTest(), 0.096);

        // Subscription renewal retires live planes but keeps the accepted
        // availability and painted RF history while the gesture is in flight.
        widget.setDisplayWindowPreservingHistory(context.exactCentreHz, 1000000);
        widget.invalidateRemoteSpectrumFrame();
        QCOMPARE(widget.maxZoomOutBandwidthHz(), 2000000.0);
        QCOMPARE(widget.dssRowsPushedForTest(), 1);

        ++context.codec.contextGeneration;
        context.exactSpanHz = 1000000;
        context.wideband.active = true;
        context.wideband.sourceGeneration = 7;
        widget.setRemoteSpectrumContext(context, 14225000, 192000);
        QVERIFY(widget.remoteWidebandActive());
        QVERIFY(widget.extendedMode());
        QCOMPARE(widget.dssRowsPushedForTest(), 1);
        QCOMPARE(localDemand.size(), 0);

        frame.context = context.codec;
        QVERIFY(widget.updateRemoteSpectrum(frame));
        QTRY_COMPARE(widget.dssRowsPushedForTest(), 2);
        // The exact row may be composite; the optional wide row remains the
        // separately described DDC-only 3D history plane.
        QCOMPARE(widget.dssNewestRowWideBandwidthForTest(), 0.096);

        widget.setDisplayWindowPreservingHistory(context.exactCentreHz, 192000);
        widget.invalidateRemoteSpectrumFrame();
        ++context.codec.contextGeneration;
        context.exactSpanHz = 192000;
        context.wideband.active = false;
        context.wideband.sourceGeneration = 0;
        widget.setRemoteSpectrumContext(context, 14225000, 192000);
        QVERIFY(!widget.extendedMode());
        QCOMPARE(widget.dssRowsPushedForTest(), 2);
        QCOMPARE(localDemand.size(), 0);

        widget.clearRemoteSpectrum();
        QVERIFY(!widget.remoteWidebandAvailable());
        QVERIFY(!widget.remoteWidebandActive());
        QCOMPARE(widget.maxZoomOutBandwidthHz(), 192000.0);
        QVERIFY(widget.extendedViewAllowed());
        QCOMPARE(widget.dssRowsPushedForTest(), 0);
        QCOMPARE(localDemand.size(), 0);
    }

    // A quiet budgeted display must follow the actual mirrored pin, even
    // when that delta arrives after the command answer. No pan reselection
    // or fresh picture is needed to complete the operator's change.
    void budgetedCtunFollowsPinDeltaAfterTheCommandResult()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        SliceModel* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10000000, 10000000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* first = stack.addPanadapter(QStringLiteral("first"));
        PanadapterApplet* cohost = stack.addPanadapter(QStringLiteral("cohost"));
        stack.setActivePan(QStringLiteral("first"));
        for (PanadapterApplet* applet : {first, cohost}) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setVfoFrequency(centre);
            applet->spectrumWidget()->setCtunEnabled(false);
            applet->spectrumWidget()->setWfAgcEnabled(false);
        }
        SpectrumWidget* widget = first->spectrumWidget();
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new HoldingCtunDeltaTransport();
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QVERIFY(client.remoteDisplayBudgetLimits().has_value());
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty()
                && !cohost->spectrumWidget()->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(widget->ctunAvailable());
        QVERIFY(widget->ctunHeldReason().isEmpty());
        SliceModel* mirrored = remote.sliceById(sliceId);
        QVERIFY(mirrored);
        QVERIFY(!mirrored->streamCtunPinned());
        QVERIFY(!widget->ctunEnabled());
        const int contexts = countControl(inbound, QStringLiteral("context"));
        const auto pinRequests = [&] {
            int count = 0;
            for (const QByteArray& wire : stationLink->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::CommandInvoke
                    && message.commandVerb == "requestStreamCtunPinned") {
                    ++count;
                }
            }
            return count;
        };
        const int requestsBefore = pinRequests();
        QSignalSpy answers(&client, &StationClient::streamCtunPinFinished);
        QSignalSpy gestures(widget, &SpectrumWidget::ctunEnabledChanged);
        QVERIFY(gestures.isValid());
        stationLink->hold = true;
        widget->setCtunEnabled(true);
        QTRY_VERIFY(!answers.isEmpty());
        QCOMPARE(answers.constLast().at(0).toInt(), sliceId);
        QVERIFY(answers.constLast().at(3).toBool());
        QTRY_VERIFY(!stationLink->held.isEmpty());
        QVERIFY(sourceSlice->streamCtunPinned());
        QVERIFY(!mirrored->streamCtunPinned());
        stationLink->releaseHeld();
        QTRY_VERIFY(mirrored->streamCtunPinned());
        gui.planNowForTest();
        QCOMPARE(countControl(inbound, QStringLiteral("context")), contexts);
        QCOMPARE(gestures.count(), 1);
        QVERIFY2(widget->ctunEnabled(), "the model pin arrived, but the budgeted pan stayed off");
        QVERIFY(cohost->spectrumWidget()->ctunEnabled());
        QCOMPARE(answers.count(), 1);
        QCOMPARE(pinRequests(), requestsBefore + 1);
        // Turning it off uses the same late-delta path, without a second
        // gesture on the cohost or another request from mirror projection.
        stationLink->hold = true;
        widget->setCtunEnabled(false);
        QTRY_COMPARE(answers.count(), 2);
        QVERIFY(answers.constLast().at(3).toBool());
        QTRY_VERIFY(!stationLink->held.isEmpty());
        QVERIFY(!sourceSlice->streamCtunPinned());
        QVERIFY(mirrored->streamCtunPinned());
        stationLink->releaseHeld();
        QTRY_VERIFY(!mirrored->streamCtunPinned());
        QVERIFY(!widget->ctunEnabled());
        QVERIFY(!cohost->spectrumWidget()->ctunEnabled());
        gui.planNowForTest();
        QCOMPARE(countControl(inbound, QStringLiteral("context")), contexts);
        QCOMPARE(gestures.count(), 2);
        QCOMPARE(pinRequests(), requestsBefore + 2);
        client.disconnectFromStation(QStringLiteral("test complete"));
        QVERIFY(!widget->ctunAvailable());
    }

    void ctunWheelKeepsSourceAndDragMovesCoreCentre()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        auto* cohost = stack.addPanadapter(QStringLiteral("cohost"));
        stack.setActivePan(QStringLiteral("first"));
        for (auto* applet : {first, cohost}) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setVfoFrequency(centre);
        }
        auto* widget = first->spectrumWidget();
        widget->setCtunEnabled(true);
        cohost->spectrumWidget()->setCtunEnabled(false);
        // Parity Task 17 follow-up: with waterfall AGC the dBm window follows
        // the levels AGC sets from the first frames and asks again once,
        // blanking the trace until the new context. This test is about the
        // C-Tune centre, so its pans use the stored levels.
        widget->setWfAgcEnabled(false);
        cohost->spectrumWidget()->setWfAgcEnabled(false);
        // This is the existing MainWindow click/wheel -> mirrored VFO path.
        connect(widget, &SpectrumWidget::frequencyClicked, &remote,
            [&remote, sliceId](double hz) { remote.sliceById(sliceId)->setFrequency(hz); });
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        const auto connectSession = [&] {
            auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
            auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
            stationLink->linkTo(clientLink);
            client.startSession(clientLink, server.token());
            server.acceptTransport(stationLink);
        };
        connectSession();
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        QTRY_VERIFY(widget->ctunEnabled());
        QTRY_VERIFY(cohost->spectrumWidget()->ctunEnabled());
        const int contexts = countControl(inbound, QStringLiteral("context"));
        QSignalSpy centreChanges(&station, &RadioModel::streamCentreChanged);
        widget->frequencyClicked(centre + 100);
        QTRY_COMPARE(sourceSlice->frequency(), centre + 100);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QCOMPARE(sourceSlice->shiftOffsetHz(), 100.0);
        QCOMPARE(centreChanges.size(), 0);
        QTest::qWait(150);
        QCOMPARE(countControl(inbound, QStringLiteral("context")), contexts);
        QVERIFY(feed());

        // Pan dragging applies the view then emits centerChanged. The plain
        // setter deliberately does not emit a user gesture.
        widget->setCenterFrequency(centre + 1000.25);
        widget->centerChanged(centre + 1000.25); // Fractional pixel -> whole-Hz DDC.
        QTRY_COMPARE(station.streamCentreHz(stream), centre + 1000);
        QCOMPARE(sourceSlice->frequency(), centre + 100);
        QCOMPARE(sourceSlice->shiftOffsetHz(), -900.0);
        QTRY_VERIFY_WITH_TIMEOUT(feed() && widget->ddcCenterFrequency() == centre + 1000, 5000);
        QVERIFY(countControl(inbound, QStringLiteral("context")) > contexts);
        QVERIFY(widget->ctunEnabled());

        // A cohost near the far edge makes this otherwise valid pan move
        // unsafe. Core refuses it; every affected view returns to Core truth.
        const int cohostId = station.addSlice();
        auto* cohostSlice = station.sliceById(cohostId);
        QVERIFY(cohostSlice);
        cohostSlice->setFrequency(centre + 80000);
        QCOMPARE(cohostSlice->streamIndex(), stream);
        QTRY_VERIFY(remote.sliceById(cohostId));
        // A selection followed immediately by a gesture precedes the next
        // subscription poll. Resolve the pan's current slice for both verbs.
        QSignalSpy pinResults(&client, &StationClient::streamCtunPinFinished);
        QSignalSpy centreResults(&client, &StationClient::streamCentreFinished);
        first->setActiveSliceIndex(cohostId);
        widget->ctunEnabledChanged(true);
        widget->centerChanged(centre + 1000);
        QTRY_VERIFY(!pinResults.isEmpty());
        QTRY_VERIFY(!centreResults.isEmpty());
        QCOMPARE(pinResults.first().at(0).toInt(), cohostId);
        QCOMPARE(centreResults.first().at(0).toInt(), cohostId);
        first->setActiveSliceIndex(sliceId);
        const double rejectedCentre = centre - 50000;
        widget->setCenterFrequency(rejectedCentre);
        widget->centerChanged(rejectedCentre);
        QTRY_VERIFY_WITH_TIMEOUT(feed()
            && std::abs(widget->centerFrequency() - (centre + 1000)) < 50, 5000);
        QCOMPARE(widget->ddcCenterFrequency(), centre + 1000);
        QCOMPARE(station.streamCentreHz(stream), centre + 1000);
        QCOMPARE(sourceSlice->frequency(), centre + 100);
        QCOMPARE(cohostSlice->frequency(), centre + 80000);
        station.removeSlice(cohostId);
        QTRY_VERIFY(!remote.sliceById(cohostId));

        widget->setCtunEnabled(false);
        QTRY_VERIFY(!sourceSlice->streamCtunPinned());
        QTRY_VERIFY(!cohost->spectrumWidget()->ctunEnabled());
        widget->frequencyClicked(centre + 2000);
        QTRY_COMPARE(station.streamCentreHz(stream), centre + 2000);
        QCOMPARE(sourceSlice->shiftOffsetHz(), 0.0);
        widget->setCtunEnabled(true);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        client.disconnectFromStation(QStringLiteral("C-Tune reconnect test"));
        QTRY_VERIFY(!sourceSlice->streamCtunPinned());
        QVERIFY(!widget->ctunAvailable());
        QVERIFY(widget->ctunPreference());
        connectSession();
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        QTRY_VERIFY(widget->ctunEnabled());

        // Retire and reuse stream 0 without an intervening empty GUI poll.
        // A stream index is reusable; its Core lifetime identity is not.
        const quint64 previousEpoch = sourceSlice->streamEpoch();
        const int spareId = station.addSlice();
        station.sliceById(spareId)->setFrequency(7100000);
        station.removeSlice(sliceId);
        const int replacementId = station.addSlice();
        auto* replacement = station.sliceById(replacementId);
        replacement->setFrequency(centre + 2000);
        QCOMPARE(replacement->streamIndex(), stream);
        QVERIFY(replacement->streamEpoch() != previousEpoch);
        QVERIFY(!replacement->streamCtunPinned());
        first->setActiveSliceIndex(replacementId);
        cohost->setActiveSliceIndex(replacementId);
        // Keep producing I/Q across retirement: an old painted frame may
        // still be visible before the new source/context is established.
        QTRY_VERIFY_WITH_TIMEOUT(feed() && replacement->streamCtunPinned(), 5000);
        QTRY_VERIFY(widget->ctunEnabled());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Slice control plan Task 14a (carried from Task 8): a C-Tune pin the
    // Core refused is not asked again on its own. Each fresh picture from
    // the Core used to re-send the saved preference, so a window the Core
    // keeps refusing (a listener, say) asked on every tune. A C-Tune gesture,
    // a new stream lifetime, or a change in who controls the slice may ask
    // again.
    void ctunRefusalIsNotResentOnAFreshPicture()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        stack.setActivePan(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceId);
        auto* widget = first->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        widget->setVfoFrequency(centre);
        widget->setCtunEnabled(true);
        widget->setWfAgcEnabled(false);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        QTRY_VERIFY(widget->ctunEnabled());
        SliceModel* mirrored = remote.sliceById(sliceId);
        QVERIFY(mirrored);
        const quint64 epoch = mirrored->streamEpoch();

        // The Core refuses the pin; then its picture moves (a fresh context).
        QSignalSpy pinResults(&client, &StationClient::streamCtunPinFinished);
        emit client.streamCtunPinFinished(sliceId, epoch, true, false);
        QCOMPARE(pinResults.size(), 1);
        const auto moveCoreCentre = [&](double hz) {
            QVERIFY(station.requestStreamCentre(sliceId, hz));
            QTRY_VERIFY_WITH_TIMEOUT(feed() && widget->ddcCenterFrequency() == hz, 5000);
        };
        moveCoreCentre(centre + 1000);
        for (int i = 0; i < 6; ++i) { QTest::qWait(50); feed(); }
        QCOMPARE(pinResults.size(), 1);  // Not asked again.

        // A C-Tune gesture asks again.
        widget->ctunEnabledChanged(true);
        QTRY_COMPARE(pinResults.size(), 2);
        QVERIFY(pinResults.last().at(3).toBool());

        // A change in who controls the slice lets a refused pin be asked
        // again on the next fresh picture.
        emit client.streamCtunPinFinished(sliceId, epoch, true, false);
        QCOMPARE(pinResults.size(), 3);
        moveCoreCentre(centre + 2000);
        for (int i = 0; i < 6; ++i) { QTest::qWait(50); feed(); }
        QCOMPARE(pinResults.size(), 3);
        emit client.sliceAccess()->changed(sliceId);
        moveCoreCentre(centre + 3000);
        QTRY_COMPARE(pinResults.size(), 4);
        QVERIFY(pinResults.last().at(3).toBool());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A window signing in sends its pan's saved C-Tune setting once for each
    // receiver its pans show. On a receiver another holder got to first the
    // Core refuses that (ruling 6.3: the pin is the holder's), so the window
    // asked at every launch for what it could not have. Here the Core's own
    // position holds the receiver, as the desktop that hosts a Core does. The
    // window's own slice shares it, or the window listens to the Core's
    // slice: no pin request may reach the Core. When the receiver becomes the
    // window's (the Core's slice leaves, or the window takes control of it)
    // its saved setting goes in.
    //
    // The window knows who holds a receiver from the access entry's
    // receiverHolderDeviceId, which a Core from before 2026-10-12 does not
    // send. On such a Core the window cannot know, so it asks once and is
    // refused, exactly as it always was, and the switch is not held.
    void savedCtunIsNotSentForAReceiverAnotherHolds_data()
    {
        QTest::addColumn<bool>("listens");
        QTest::addColumn<bool>("olderCore");
        QTest::newRow("its slice shares the receiver the Core's slice holds") << false << false;
        QTest::newRow("it listens to the Core's slice") << true << false;
        QTest::newRow("an older Core names no holder: asked once and refused, as before")
            << false << true;
    }

    void savedCtunIsNotSentForAReceiverAnotherHolds()
    {
        QFETCH(bool, listens);
        QFETCH(bool, olderCore);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        SliceOwnership* ownership = station.sliceOwnership();
        int coreSliceId = -1;
        {
            const SliceOwnership::CreatorScope coreCreates(ownership,
                                                           SliceOwnership::stationDevice());
            coreSliceId = station.addSlice();
        }
        SliceModel* coreSlice = station.sliceById(coreSliceId);
        QVERIFY(coreSlice);
        const int stream = coreSlice->streamIndex();
        QVERIFY(stream >= 0);
        QCOMPARE(ownership->anchorOf(stream), SliceOwnership::stationDevice());
        const double center = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QTemporaryDir keyDir;
        const auto identity = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(identity->isValid());
        PairedDevice paired;
        paired.id = identity->fingerprint();
        paired.publicKeySpki = identity->publicKeySpki();
        paired.name = QStringLiteral("Sharing window");
        paired.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(paired));
        client.setDeviceIdentity(identity, paired.name, QStringLiteral("Window"));
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        stack.setActivePan(QStringLiteral("first"));
        auto* widget = first->spectrumWidget();
        // The pan's saved setting: C-Tune on.
        widget->setCtunEnabled(true);
        QVERIFY(widget->ctunPreference());
        widget->setWfAgcEnabled(false);
        widget->setConnectionState(ConnectionState::Connected);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        QString certificatePin = server.certificateFingerprint();
        certificatePin.remove(QLatin1Char(':'));
        clientLink->setPeerCertificateSha256(QByteArray::fromHex(certificatePin.toLatin1()));
        stationLink->setPeerAddress(QStringLiteral("192.0.2.30"));
        stationLink->linkTo(clientLink);
        if (olderCore) {
            clientLink->setRewritesIncoming(asACoreThatNamesNoReceiverHolderSends);
        }
        // Every pin request the Core receives from the window, and its answers.
        int pinAsks = 0;
        connect(stationLink, &SessionTransport::textReceived, this, [&pinAsks](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.commandVerb == "requestStreamCtunPinned") {
                ++pinAsks;
            }
        });
        QStringList pinAnswers;
        connect(&client, &StationClient::commandResponse, this,
            [&pinAnswers](const NereusSDR::SessionMessage& message) {
                if (message.commandVerb == "requestStreamCtunPinned") {
                    pinAnswers.append(message.accepted ? QStringLiteral("accepted")
                                                       : QStringLiteral("refused: ") + message.reason);
                }
            });
        const auto asked = [&] {
            return QStringLiteral("the window sent %1 C-Tune request(s); the Core answered: %2")
                .arg(pinAsks).arg(pinAnswers.join(QStringLiteral(" | "))).toUtf8();
        };
        // A change the window held back itself, as it shows one to the user.
        QSignalSpy heldNotices(&client, &StationClient::sliceAccessHeld);
        client.startSession(clientLink, QString(), QString(),
                            server.stationIdentity().fingerprint());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_VERIFY(client.stationLinkReady());

        int ownId = -1;
        for (SliceModel* slice : station.slices()) {
            if (ownership->mark(slice->sliceIndex()).owner == identity->fingerprint()) {
                ownId = slice->sliceIndex();
            }
        }
        QVERIFY(ownId >= 0);
        SliceModel* ownSlice = station.sliceById(ownId);
        int shownId = ownId;
        if (listens) {
            // The window's own slice goes to another band and receiver; its
            // pan shows the Core's slice, which it joins to listen.
            ownSlice->setFrequency(center + 7'000'000);
            QVERIFY(ownSlice->streamIndex() >= 0);
            QVERIFY(ownSlice->streamIndex() != stream);
            QTRY_VERIFY(client.sliceAccess()->entry(coreSliceId).has_value());
            const auto listen = client.requestListen(
                coreSliceId, client.sliceAccess()->entry(coreSliceId)->incarnation);
            QVERIFY2(listen.sent, qPrintable(listen.reason));
            QTRY_VERIFY(client.sliceAccess()->listeningHere(coreSliceId));
            QVERIFY(!client.sliceAccess()->controlledHere(coreSliceId));
            shownId = coreSliceId;
        } else {
            // The window's own slice, put on the receiver the Core's slice holds.
            ownSlice->setFrequency(center + 20000);
            QCOMPARE(ownSlice->streamIndex(), stream);
            QTRY_VERIFY(client.sliceAccess()->controlledHere(ownId));
        }
        SliceModel* shown = station.sliceById(shownId);
        QCOMPARE(ownership->anchorOf(stream), SliceOwnership::stationDevice());
        QVERIFY(!shown->streamCtunPinned());
        QTRY_VERIFY(remote.sliceById(shownId));
        SliceModel* mirrored = remote.sliceById(shownId);
        QTRY_COMPARE(mirrored->streamIndex(), stream);
        QTRY_COMPARE(mirrored->frequency(), shown->frequency());
        QCOMPARE(pinAsks, 0);

        first->setActiveSliceIndex(shownId);
        widget->setDisplayWindowPreservingHistory(center, 48000);
        widget->setVfoFrequency(shown->frequency());
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&](int receiver) {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, receiver), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(stream), 5000);
        for (int i = 0; i < 10; ++i) {
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs);
            feed(stream);
        }
        if (olderCore) {
            // The window was told of no holder, so it cannot know the
            // setting is another's. It asks once, the Core refuses in its
            // own words, nothing is asked again, and the switch is not held.
            QVERIFY(client.sliceAccess()->entry(shownId).has_value());
            QVERIFY(client.sliceAccess()->receiverHeldReason(shownId).isEmpty());
            QTRY_VERIFY2(pinAnswers.size() == 1, asked().constData());
            QCOMPARE(pinAnswers.first(),
                     QStringLiteral("refused: This panadapter shows the Core's receiver. Its "
                                    "C-Tune setting is the Core's."));
            for (int i = 0; i < 5; ++i) {
                QTest::qWait(RemoteMediaController::kPlannerIntervalMs);
                feed(stream);
            }
            QVERIFY2(pinAsks == 1, asked().constData());
            QVERIFY(!shown->streamCtunPinned());
            QVERIFY(widget->ctunHeldReason().isEmpty());
            client.disconnectFromStation(QStringLiteral("test complete"));
            return;
        }
        QVERIFY2(pinAsks == 0, asked().constData());
        // Nor is the window told of a change held back that nobody made.
        QCOMPARE(heldNotices.size(), 0);
        QVERIFY(!shown->streamCtunPinned());
        QVERIFY(!widget->ctunEnabled());
        // The switch is there, held, with the Core's own words for why.
        QVERIFY(widget->ctunAvailable());
        QCOMPARE(widget->ctunHeldReason(),
                 listens ? QStringLiteral("Slice A is controlled by the Core. Take control to "
                                          "change it.")
                         : QStringLiteral("This panadapter shows the Core's receiver. Its C-Tune "
                                          "setting is the Core's."));
        widget->setCtunEnabled(false);
        for (int i = 0; i < 3; ++i) {
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs);
            feed(stream);
        }
        QVERIFY2(pinAsks == 0, asked().constData());
        QCOMPARE(heldNotices.size(), 0);
        // The saved setting is kept for the day the receiver is the window's.
        QVERIFY(widget->ctunPreference());
        // The words on the switch are the Core's own: asked past the window
        // (which never asks), the Core refuses with the same sentence.
        const auto direct = client.requestStreamCtunPinned(shownId, true);
        QVERIFY2(direct.sent, qPrintable(direct.reason));
        QTRY_COMPARE(pinAnswers.size(), 1);
        QCOMPARE(pinAnswers.first(), QStringLiteral("refused: ") + widget->ctunHeldReason());
        QCOMPARE(pinAsks, 1);
        pinAsks = 0;
        pinAnswers.clear();

        if (listens) {
            // The window takes control of the slice, and the receiver with it.
            const auto access = *client.sliceAccess()->entry(coreSliceId);
            const auto take = client.requestTakeControl(coreSliceId, access.incarnation,
                                                        access.controlRevision);
            QVERIFY2(take.sent, qPrintable(take.reason));
            QTRY_COMPARE(ownership->mark(coreSliceId).owner, identity->fingerprint());
        } else {
            // The Core's slice goes to another band and another receiver.
            coreSlice->setFrequency(center + 7'000'000);
            QVERIFY(coreSlice->streamIndex() >= 0);
            QVERIFY(coreSlice->streamIndex() != stream);
        }
        QCOMPARE(shown->streamIndex(), stream);
        QCOMPARE(ownership->anchorOf(stream), identity->fingerprint());
        QTRY_VERIFY2_WITH_TIMEOUT((feed(stream), shown->streamCtunPinned()), asked().constData(),
                                  5000);
        QTRY_VERIFY(widget->ctunEnabled());
        QVERIFY(widget->ctunHeldReason().isEmpty());
        QVERIFY2(pinAsks == 1, asked().constData());
        QCOMPARE(pinAnswers, QStringList{QStringLiteral("accepted")});
        QCOMPARE(heldNotices.size(), 0);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-18, R-R3-19, R-R3-21: the zoom flash seen on checkpoint 80f45e28.
    // With C-Tune on and the pan's receiver sharing its stream, a zoom
    // re-centres the view on the VFO. That must stay a view change: the Core
    // refuses to move a shared window there, and each refusal used to snap
    // the view back and blank the trace, at wheel rate. A pan drag still asks
    // the Core; one refusal stops that drag's requests and keeps the picture.
    void refusedDragWithAwayCohostStopsGesture_data()
    {
        QTest::addColumn<bool>("scale");
        QTest::newRow("frequency-scale") << true;
        QTest::newRow("pan-body") << false;
    }

    void refusedDragWithAwayCohostStopsGesture()
    {
        QFETCH(bool, scale);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        const int hiddenId = station.addSlice();
        SliceModel* hidden = station.sliceById(hiddenId);
        QVERIFY(hidden);
        hidden->setFrequency(centre + 80000);
        QCOMPARE(hidden->streamIndex(), stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QTemporaryDir keyDir;
        const auto identity = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(identity->isValid());
        PairedDevice paired;
        paired.id = identity->fingerprint();
        paired.publicKeySpki = identity->publicKeySpki();
        paired.name = QStringLiteral("Pan drag fixture");
        paired.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(paired));
        client.setDeviceIdentity(identity, paired.name, QStringLiteral("Fixture"));
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        stack.setActivePan(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceId);
        auto* widget = first->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        widget->setVfoFrequency(centre);
        widget->setCtunEnabled(true);
        // MainWindow's connection wiring; a disconnected pan swallows clicks.
        widget->setConnectionState(ConnectionState::Connected);
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QJsonObject latestContext;
        connect(&client, &StationClient::mediaControlReceived, this,
            [&latestContext](const QJsonObject& payload, quint32 epoch) {
                if (payload.value(QStringLiteral("op")) == QLatin1String("context")) {
                    latestContext = payload;
                    qInfo() << "AWAY_CONTEXT epoch=" << epoch << payload;
                }
            });
        QStringList centreVerdicts;
        QList<SessionMessage> centreAnswers;
        connect(&client, &StationClient::commandResponse, this,
            [&centreVerdicts, &centreAnswers](const NereusSDR::SessionMessage& message) {
                if (message.commandVerb == "requestStreamCentre") {
                    centreAnswers.append(message);
                    qInfo() << "AWAY_CENTRE_RESULT id=" << message.commandId << "reason=" << message.reason;
                    centreVerdicts.append(message.accepted ? QStringLiteral("accepted")
                                                           : QStringLiteral("refused"));
                }
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        QString certificatePin = server.certificateFingerprint();
        certificatePin.remove(QLatin1Char(':'));
        clientLink->setPeerCertificateSha256(QByteArray::fromHex(certificatePin.toLatin1()));
        stationLink->setPeerAddress(QStringLiteral("192.0.2.30"));
        stationLink->linkTo(clientLink);
        connect(stationLink, &SessionTransport::textReceived, this, [](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.commandVerb == "requestStreamCentre") {
                qInfo() << "AWAY_CENTRE_INVOKE id=" << message.commandId
                        << "arguments=" << wire;
            }
        });
        connect(widget, &SpectrumWidget::centerChanged, this, [widget](double wantedHz) {
            qInfo() << "AWAY_CENTRE_INTENT wanted=" << wantedHz
                    << "zoomGesture=" << widget->isZoomRecentring()
                    << "view=" << widget->centerFrequency() << "span=" << widget->bandwidth();
        });
        client.startSession(clientLink, QString(), QString(),
                            server.stationIdentity().fingerprint());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT((feed(), sourceSlice->streamCtunPinned()
            && widget->ctunEnabled()), 5000);

        QTRY_VERIFY(client.stationLinkReady());
        QCOMPARE(station.sliceOwnership()->mark(sliceId).owner, identity->fingerprint());
        QCOMPARE(station.sliceOwnership()->mark(hiddenId).owner, identity->fingerprint());
        QCOMPARE(station.sliceOwnership()->anchorOf(stream), identity->fingerprint());
        QTRY_VERIFY(remote.sliceById(hiddenId));
        QCOMPARE(remote.slicesOnStream(stream).size(), 2);
        RadioModel remoteB(RadioModel::Role::Remote);
        remoteB.audioEngine()->setMasterMuted(true);
        SettingsProxy proxyB;
        StationClient clientB(&remoteB, &proxyB);
        QTemporaryDir keyDirB;
        const auto identityB = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDirB.path()));
        QVERIFY(identityB->isValid());
        PairedDevice pairedB;
        pairedB.id = identityB->fingerprint();
        pairedB.publicKeySpki = identityB->publicKeySpki();
        pairedB.name = QStringLiteral("Away cohost B");
        pairedB.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(pairedB));
        clientB.setDeviceIdentity(identityB, pairedB.name, QStringLiteral("B"));
        auto* stationLinkB = new Test::LoopbackTransport(QStringLiteral("station B"));
        auto* clientLinkB = new Test::LoopbackTransport(QStringLiteral("client B"));
        clientLinkB->setPeerCertificateSha256(QByteArray::fromHex(certificatePin.toLatin1()));
        stationLinkB->setPeerAddress(QStringLiteral("192.0.2.31"));
        stationLinkB->linkTo(clientLinkB);
        clientB.startSession(clientLinkB, QString(), QString(), server.stationIdentity().fingerprint());
        server.acceptTransport(stationLinkB);
        QTRY_VERIFY(clientB.stationLinkReady());
        // B's default admission slice is closed by B's ordinary authenticated
        // command, after checking its actual ownership and mirrored identity.
        QList<int> extras;
        for (int member : station.slicesOnStream(stream)) {
            if (member != sliceId && member != hiddenId) {
                extras.append(member);
            }
        }
        QCOMPARE(extras.size(), 1);
        const int extraId = extras.first();
        QCOMPARE(station.sliceOwnership()->mark(extraId).owner, identityB->fingerprint());
        QTRY_VERIFY(remoteB.sliceById(extraId));
        QTRY_VERIFY(clientB.sliceAccess()->entry(extraId).has_value());
        const auto extraAccess = *clientB.sliceAccess()->entry(extraId);
        QVERIFY(extraAccess.incarnation > 0);
        QVERIFY(extraAccess.controlRevision > 0);
        QVERIFY(clientB.sliceAccess()->controlledHere(extraId));
        QList<SessionMessage> closeAnswers;
        const auto closeConnection = connect(&clientB, &StationClient::commandResponse, this,
            [&closeAnswers](const SessionMessage& message) {
                if (message.commandVerb == "removeSlice") {
                    closeAnswers.append(message);
                }
            });
        const auto close = clientB.requestRemoveSlice(extraId);
        QVERIFY2(close.sent, qPrintable(close.reason));
        QTRY_COMPARE(closeAnswers.size(), 1);
        QCOMPARE(closeAnswers.first().commandId, qint64(close.commandId));
        QVERIFY2(closeAnswers.first().accepted, qPrintable(closeAnswers.first().reason));
        QTRY_VERIFY(!station.sliceById(extraId));
        QTRY_VERIFY(!remoteB.sliceById(extraId));
        disconnect(closeConnection);
        QCOMPARE(station.slicesOnStream(stream).size(), 2);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QCOMPARE(station.streamSampleRateHz(stream), 192000);
        QCOMPARE(sourceSlice->streamIndex(), stream);
        QCOMPARE(hidden->streamIndex(), stream);
        QCOMPARE(sourceSlice->streamEpoch(), hidden->streamEpoch());
        QCOMPARE(hidden->frequency(), centre + 80000);
        QCOMPARE(station.sliceOwnership()->anchorOf(stream), identity->fingerprint());
        qInfo() << "AWAY_NORMALIZED extra=" << extraId << "incarnation=" << extraAccess.incarnation
                << "revision=" << extraAccess.controlRevision << "closeCommand=" << close.commandId;
        QTRY_VERIFY(clientB.sliceAccess()->entry(hiddenId).has_value());
        const auto accessB = *clientB.sliceAccess()->entry(hiddenId);
        QSignalSpy accessAnswersB(&clientB, &StationClient::deviceCommandFinished);
        const auto take = clientB.requestTakeControl(hiddenId, accessB.incarnation,
                                                    accessB.controlRevision);
        QVERIFY2(take.sent, qPrintable(take.reason));
        QTRY_COMPARE(accessAnswersB.size(), 1);
        QCOMPARE(accessAnswersB.first().at(1).toUInt(), take.commandId);
        QVERIFY(accessAnswersB.first().at(2).toBool());
        QTRY_COMPARE(station.sliceOwnership()->mark(hiddenId).owner, identityB->fingerprint());
        QCOMPARE(station.sliceOwnership()->mark(sliceId).owner, identity->fingerprint());
        QCOMPARE(station.sliceOwnership()->anchorOf(stream), identity->fingerprint());
        QTRY_VERIFY(client.sliceAccess()->entry(hiddenId).has_value());
        QSignalSpy accessAnswersA(&client, &StationClient::deviceCommandFinished);
        const auto stop = client.requestStopListening(hiddenId,
            client.sliceAccess()->entry(hiddenId)->incarnation);
        QVERIFY2(stop.sent, qPrintable(stop.reason));
        QTRY_COMPARE(accessAnswersA.size(), 1);
        QCOMPARE(accessAnswersA.first().at(1).toUInt(), stop.commandId);
        QVERIFY(accessAnswersA.first().at(2).toBool());
        QTRY_VERIFY(!remote.sliceById(hiddenId));
        QCOMPARE(remote.slicesOnStream(stream).size(), 1);
        QCOMPARE(station.slicesOnStream(stream).size(), 2);
        clientLinkB->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(server.deviceSessions()->entry(identityB->fingerprint()).has_value());
        QTRY_COMPARE(server.deviceSessions()->entry(identityB->fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        const auto away = *server.deviceSessions()->entry(identityB->fingerprint());
        QVERIFY(away.awayGeneration > 0);
        QVERIFY(!away.session);
        QVERIFY(station.sliceOwnership()->isAwaySlice(hiddenId));
        QCOMPARE(station.sliceOwnership()->mark(hiddenId).owner, identityB->fingerprint());
        QCOMPARE(hidden->streamIndex(), stream);
        QCOMPARE(hidden->frequency(), centre + 80000);
        QVERIFY(server.deviceSessions()->now() - away.awaySinceMs < DeviceSessionRegistry::kGraceMs);
        qInfo() << "AWAY_SETUP generation=" << away.awayGeneration << "since=" << away.awaySinceMs
                << "now=" << server.deviceSessions()->now() << "owner=" << identityB->fingerprint().toHex();
        sourceSlice->setFrequency(centre - 30000);
        QTRY_COMPARE(remote.sliceById(sliceId)->frequency(), centre - 30000);
        widget->setVfoFrequency(remote.sliceById(sliceId)->frequency());
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        QTRY_VERIFY_WITH_TIMEOUT((feed(), widget->waterfallHistoryRowsForTest() >= 5), 10000);
        QCOMPARE(station.streamSampleRateHz(stream), 192000);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QCOMPARE(widget->ddcCenterFrequency(), centre);
        const int grantedFftSize = latestContext.value(QStringLiteral("grantedFftSize")).toInt();
        QVERIFY(grantedFftSize > 0);
        const double sourceBinHz = 192000.0 / grantedFftSize;
        // The accepted crop is rounded to source-bin boundaries; source/DDC
        // stay exact while the view centre/span may differ by those bins.
        QVERIFY(std::abs(widget->centerFrequency() - centre) <= sourceBinHz);
        QVERIFY(std::abs(widget->bandwidth() - 48000.0) <= 2 * sourceBinHz);
        QVERIFY(widget->ctunEnabled());
        QVERIFY(!latestContext.isEmpty());
        QCOMPARE(latestContext.value(QStringLiteral("sourceCentreHz")).toDouble(), centre);
        const int historyBefore = widget->waterfallHistoryRowsForTest();
        const quint64 streamEpoch = remote.sliceById(sliceId)->streamEpoch();
        QSignalSpy centres(widget, &SpectrumWidget::centerChanged);
        QSignalSpy finishes(&client, &StationClient::streamCentreFinished);
        QSignalSpy notices(&remote, &RadioModel::sliceAddRejected);
        const int gpuDivider = static_cast<int>((widget->height() - 32) * 0.40);
        const int cpuDivider = static_cast<int>(widget->height() * 0.40);
        const int scaleTop = std::max(gpuDivider, cpuDivider) + 4 + 6;
        const int scaleBottom = std::min(gpuDivider, cpuDivider) + 4 + 28;
        QVERIFY(scaleTop < scaleBottom);
        const QPointF onScale(widget->width() * 0.5, (scaleTop + scaleBottom) / 2);
        const auto sendMouse = [&](QEvent::Type type, QPointF at, Qt::MouseButtons held) {
            QMouseEvent mouse(type, at, widget->mapToGlobal(at),
                type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                held, Qt::NoModifier);
            QCoreApplication::sendEvent(widget, &mouse);
        };
        const QPointF grab = scale ? onScale
            : QPointF(widget->width() * 0.2, widget->height() * 0.2);
        const QPointF firstMove = scale ? grab - QPointF(30, 0) : grab + QPointF(300, 0);
        const QString hardReason = QStringLiteral(
            "C-Tune cannot centre there while other receivers share this spectrum.");
        const auto record = [&](const char* phase, int step) {
            const double captureCentre = latestContext.value(QStringLiteral("centreHz")).toDouble();
            const double captureSpan = latestContext.value(QStringLiteral("spanHz")).toDouble();
            const double overlap = std::max(0.0,
                std::min(captureCentre + captureSpan / 2, widget->centerFrequency() + widget->bandwidth() / 2)
                - std::max(captureCentre - captureSpan / 2, widget->centerFrequency() - widget->bandwidth() / 2));
            qInfo() << "AWAY_OBSERVATION phase=" << phase << "step=" << step << "scale=" << scale
                    << "zoomGesture=" << widget->isZoomRecentring() << "finishes=" << finishes.size()
                    << "centres=" << centres.size() << "answers=" << centreAnswers.size()
                    << "view=" << widget->centerFrequency() << "span=" << widget->bandwidth()
                    << "ddc=" << widget->ddcCenterFrequency() << "source=" << stream << "epoch=" << streamEpoch
                    << "context=" << latestContext << "contextOverlapHz=" << overlap
                    << "pixels=" << widget->renderedPixels().size()
                    << "history=" << widget->waterfallHistoryRowsForTest();
        };
        // The view centre as the refusal is delivered. The controller's own
        // streamCentreFinished slot was connected first, so it has restored
        // the view by the time this one runs. Reading the widget later is a
        // race: the restore asks for a fresh subscription, and once its
        // context lands the view takes the Core's bin-aligned crop, up to
        // one source bin away (as before the drag).
        QList<double> viewAtRefusal;
        const auto refusalView = connect(&client, &StationClient::streamCentreFinished, this,
            [&viewAtRefusal, widget](int, quint64, bool accepted) {
                if (!accepted) { viewAtRefusal.append(widget->centerFrequency()); }
            });
        sendMouse(QEvent::MouseButtonPress, grab, Qt::LeftButton);
        QCOMPARE(widget->isZoomRecentring(), scale);
        sendMouse(QEvent::MouseMove, firstMove, Qt::LeftButton);
        QTRY_COMPARE(finishes.size(), 1);
        QTRY_COMPARE(centreAnswers.size(), 1);
        QCOMPARE(finishes.first().at(0).toInt(), sliceId);
        QCOMPARE(finishes.first().at(1).toULongLong(), streamEpoch);
        QCOMPARE(finishes.first().at(2).toBool(), false);
        QCOMPARE(centreAnswers.first().reason, hardReason);
        for (const auto& update : centreAnswers.first().updates) {
            QVERIFY(update.name != "phase" || update.value.toString() != "needsConfirmation");
        }
        QCOMPARE(station.streamCentreHz(stream), centre);
        disconnect(refusalView);
        QCOMPARE(viewAtRefusal.size(), 1);
        QVERIFY(std::abs(viewAtRefusal.first() - centre) < 1.0);
        // The refusal returns the view to the Core's source. A context still
        // settling from before the drag may land after that and move the view
        // to its bin-aligned crop, as the check before the drag allows; the
        // refused drag itself was 300 px away.
        QVERIFY(std::abs(widget->centerFrequency() - centre) <= sourceBinHz);
        record("first-final-refusal", 0);
        for (int step = 1; step <= 5; ++step) {
            const int beforeFinishes = finishes.size();
            const int beforeCentres = centres.size();
            sendMouse(QEvent::MouseMove, scale ? grab - QPointF(30 + step * 10, 0)
                                               : grab + QPointF(300 + step * 10, 0), Qt::LeftButton);
            if (centres.size() > beforeCentres) {
                QTRY_COMPARE(finishes.size(), beforeFinishes + 1);
                QTRY_COMPARE(centreAnswers.size(), finishes.size());
            }
            feed();
            QTest::qWait(40);
            record("same-held-move", step);
        }
        const int heldFinishes = finishes.size();
        sendMouse(QEvent::MouseButtonRelease, scale ? grab - QPointF(80, 0) : grab + QPointF(350, 0), Qt::NoButton);
        QVERIFY(server.deviceSessions()->now() - away.awaySinceMs < DeviceSessionRegistry::kGraceMs);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QCOMPARE(hidden->streamEpoch(), streamEpoch);
        QCOMPARE(sourceSlice->streamEpoch(), streamEpoch);
        QCOMPARE(hidden->frequency(), centre + 80000);
        QCOMPARE(sourceSlice->frequency(), centre - 30000);
        QVERIFY(widget->waterfallHistoryRowsForTest() >= historyBefore);
        // A new safe pan gesture must still send and be accepted.
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        const QPointF safeGrab(widget->width() * 0.2, widget->height() * 0.2);
        sendMouse(QEvent::MouseButtonPress, safeGrab, Qt::LeftButton);
        sendMouse(QEvent::MouseMove, safeGrab - QPointF(50, 0), Qt::LeftButton);
        sendMouse(QEvent::MouseButtonRelease, safeGrab - QPointF(50, 0), Qt::NoButton);
        QTRY_COMPARE(finishes.size(), heldFinishes + 1);
        QTRY_COMPARE(centreAnswers.size(), finishes.size());
        QVERIFY(centreAnswers.last().accepted);
        record("fresh-safe-gesture", 0);
        QCOMPARE(heldFinishes, 1);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void ctunZoomOnSharedStreamKeepsCoreCentreAndRefusalKeepsPicture()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        stack.setActivePan(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceId);
        auto* widget = first->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        widget->setVfoFrequency(centre);
        widget->setCtunEnabled(true);
        // MainWindow's connection wiring; a disconnected pan swallows clicks.
        widget->setConnectionState(ConnectionState::Connected);
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QStringList centreVerdicts;
        connect(&client, &StationClient::commandResponse, this,
            [&centreVerdicts](const NereusSDR::SessionMessage& message) {
                if (message.commandVerb == "requestStreamCentre") {
                    centreVerdicts.append(message.accepted ? QStringLiteral("accepted")
                                                           : QStringLiteral("refused"));
                }
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        QTRY_VERIFY(widget->ctunEnabled());

        // The operator's shape: a second receiver on the same stream, near
        // the far edge, and the pan's own receiver tuned off-centre inside
        // the window (C-Tune keeps the Core centre while tuning in-window).
        const int cohostId = station.addSlice();
        auto* cohostSlice = station.sliceById(cohostId);
        QVERIFY(cohostSlice);
        cohostSlice->setFrequency(centre + 80000);
        QCOMPARE(cohostSlice->streamIndex(), stream);
        sourceSlice->setFrequency(centre - 30000);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QTRY_VERIFY(remote.sliceById(cohostId)
            && remote.sliceById(cohostId)->streamIndex() == stream);
        QTRY_COMPARE(remote.sliceById(sliceId)->frequency(), centre - 30000);
        widget->setVfoFrequency(centre - 30000);
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);

        QSignalSpy notices(&remote, &RadioModel::sliceAddRejected);
        QSignalSpy centreResults(&client, &StationClient::streamCentreFinished);
        const QPointF inSpectrum(widget->width() * 0.25, widget->height() * 0.2);
        const auto sendWheel = [&](int delta) {
            QWheelEvent wheel(inSpectrum, widget->mapToGlobal(inSpectrum), QPoint(),
                              QPoint(0, delta), Qt::NoButton, Qt::ControlModifier,
                              Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(widget, &wheel);
        };
        // Wheel zoom in and out, as on the bench.
        const double startBandwidth = widget->bandwidth();
        for (int step = 0; step < 4; ++step) {
            sendWheel(120);
            feed();
            QTest::qWait(20);
        }
        QVERIFY(widget->bandwidth() < startBandwidth);
        for (int step = 0; step < 4; ++step) {
            sendWheel(-120);
            feed();
            QTest::qWait(20);
        }
        // Frequency-scale drag zoom.
        widget->setDisplayWindowPreservingHistory(centre, widget->bandwidth());
        // The frequency scale sits under the spectrum/waterfall divider; the
        // GPU and CPU layouts place the divider a little differently, so aim
        // at the band both put inside the scale (past the divider grab).
        const int gpuDivider = static_cast<int>((widget->height() - 32) * 0.40);
        const int cpuDivider = static_cast<int>(widget->height() * 0.40);
        const int scaleTop = std::max(gpuDivider, cpuDivider) + 4 + 6;
        const int scaleBottom = std::min(gpuDivider, cpuDivider) + 4 + 28;
        QVERIFY(scaleTop < scaleBottom);
        const QPointF onScale(widget->width() * 0.5, (scaleTop + scaleBottom) / 2);
        const auto sendMouse = [&](QEvent::Type type, QPointF at, Qt::MouseButtons held) {
            QMouseEvent event(type, at, widget->mapToGlobal(at), Qt::LeftButton, held,
                              Qt::NoModifier);
            QCoreApplication::sendEvent(widget, &event);
        };
        const double zoomedOutBandwidth = widget->bandwidth();
        sendMouse(QEvent::MouseButtonPress, onScale, Qt::LeftButton);
        sendMouse(QEvent::MouseMove, onScale - QPointF(60, 0), Qt::LeftButton);
        sendMouse(QEvent::MouseMove, onScale - QPointF(120, 0), Qt::LeftButton);
        sendMouse(QEvent::MouseButtonRelease, onScale - QPointF(120, 0), Qt::NoButton);
        QVERIFY(widget->bandwidth() > zoomedOutBandwidth);
        QCOMPARE(widget->centerFrequency(), centre - 30000);
        for (int i = 0; i < 10; ++i) {
            feed();
            QTest::qWait(30);
        }
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        // Nothing asked of the Core, so nothing refused or snapped back. The
        // view stays on the VFO (within the Core's bin-aligned crop).
        QCOMPARE(centreVerdicts, QStringList{});
        QCOMPARE(centreResults.size(), 0);
        QCOMPARE(notices.size(), 0);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QVERIFY(std::abs(widget->centerFrequency() - (centre - 30000)) < 50);
        QCOMPARE(widget->ddcCenterFrequency(), centre);

        // An explicit pan drag still asks. This one would push the cohost
        // out of the window, so the Core refuses: the picture stays where
        // the Core has it (the view does not follow the rest of the drag),
        // one notice is shown,
        // and the rest of the drag sends nothing more.
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        const QPointF grab(widget->width() * 0.2, widget->height() * 0.2);
        const double hzPerPx = 48000.0 / widget->width();
        sendMouse(QEvent::MouseButtonPress, grab, Qt::LeftButton);
        // 300 px right = view centre about 24 kHz lower: cohost falls outside.
        sendMouse(QEvent::MouseMove, grab + QPointF(300, 0), Qt::LeftButton);
        QTRY_COMPARE(centreVerdicts, QStringList{QStringLiteral("refused")});
        QTRY_VERIFY(std::abs(widget->centerFrequency() - centre) < hzPerPx);
        for (int step = 1; step <= 5; ++step) {
            sendMouse(QEvent::MouseMove, grab + QPointF(300 + step * 10, 0), Qt::LeftButton);
            QVERIFY(std::abs(widget->centerFrequency() - centre) < hzPerPx);
            feed();
            QTest::qWait(30);
        }
        sendMouse(QEvent::MouseButtonRelease, grab + QPointF(350, 0), Qt::NoButton);
        QTest::qWait(150);
        QCOMPARE(centreVerdicts, QStringList{QStringLiteral("refused")});
        QCOMPARE(notices.size(), 1);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QCOMPARE(widget->ddcCenterFrequency(), centre);
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);

        // A new drag to a centre that keeps both receivers inside is asked
        // for and accepted.
        sendMouse(QEvent::MouseButtonPress, grab, Qt::LeftButton);
        sendMouse(QEvent::MouseMove, grab - QPointF(50, 0), Qt::LeftButton);
        sendMouse(QEvent::MouseButtonRelease, grab - QPointF(50, 0), Qt::NoButton);
        QTRY_COMPARE(centreVerdicts,
                     (QStringList{QStringLiteral("refused"), QStringLiteral("accepted")}));
        QVERIFY(station.streamCentreHz(stream) > centre);
        QCOMPARE(notices.size(), 1);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Parity Task 18, B3.5 (R-R3-09): on a pan with two slices on one
    // receiver, selecting the other slice's flag moves the pan's display to
    // that slice without losing what the pan has drawn: the waterfall, its
    // rewind history and the 3D stack stay, as they do in a local window.
    void selectingTheOtherSliceOnOneReceiverKeepsThePansHistory_data()
    {
        QTest::addColumn<bool>("threeD");
        QTest::addColumn<bool>("budget");
        QTest::addColumn<int>("rejoin");
        QTest::newRow("waterfall") << false << false << kOneReceiver;
        QTest::newRow("3D stack") << true << false << kOneReceiver;
        QTest::newRow("waterfall, display budget") << false << true << kOneReceiver;
        QTest::newRow("3D stack, display budget") << true << true << kOneReceiver;
        // JJ's ruling 2026-10-22: the two slices start on receivers of their
        // own, and the Core puts them back on one when one is tuned into the
        // other's range during the session. Joined: the second slice, alone
        // on the higher receiver, is tuned next to the first. Folded: the
        // first, alone on the lower receiver, is tuned next to the second,
        // which comes down onto it.
        QTest::newRow("waterfall, rejoined on the Core") << false << false << kJoinedOnTheCore;
        QTest::newRow("waterfall, folded on the Core") << false << false << kFoldedOnTheCore;
    }
    void selectingTheOtherSliceOnOneReceiverKeepsThePansHistory()
    {
        QFETCH(bool, threeD);
        QFETCH(bool, budget);
        QFETCH(int, rejoin);
        const bool rejoined = rejoin != kOneReceiver;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        // The rejoin rows need a codec: a rejoin compares the ADC each
        // receiver's DDC reads, which only the codec's assignment says.
        P2CodecSaturn codec;
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        if (rejoined) {
            station.receiverManager()->setP2Codec(&codec);
        }
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int firstId = station.addSlice();
        auto* firstSlice = station.sliceById(firstId);
        QVERIFY(firstSlice);
        const int stream = firstSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        const int secondId = station.addSlice();
        auto* secondSlice = station.sliceById(secondId);
        QVERIFY(secondSlice);
        secondSlice->setFrequency(centre + 20000);
        QCOMPARE(secondSlice->streamIndex(), stream);
        if (rejoined) {
            // Out of the first receiver's window: a receiver of its own.
            secondSlice->setFrequency(centre + 3000000);
            QVERIFY(secondSlice->streamIndex() >= 0 && secondSlice->streamIndex() != stream);
            QCOMPARE(station.streamAllocator().activeStreamCount(), 2);
        }
        if (rejoin == kFoldedOnTheCore) {
            // The first slice alone on the lower receiver, away from the
            // pan's view; the second alone on the higher one, back at its
            // place next to where the first will be tuned.
            firstSlice->setFrequency(centre - 3000000);
            QCOMPARE(firstSlice->streamIndex(), stream);
            secondSlice->setFrequency(centre + 20000);
            QVERIFY(secondSlice->streamIndex() != stream);
            QCOMPARE(station.streamAllocator().activeStreamCount(), 2);
        }
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        if (budget) {
            QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        }
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("pan"));
        stack.setActivePan(QStringLiteral("pan"));
        pan->addSlice(firstId);
        pan->addSlice(secondId);
        pan->setActiveSliceIndex(firstId);
        auto* widget = pan->spectrumWidget();
        remote.setClarityEnabled(true);
        PanClarityRegistry registry(&remote);
        registry.registerPan(QStringLiteral("pan"),widget,nullptr);
        ClarityController* clarity=registry.controllerForPan(QStringLiteral("pan"));
        QSignalSpy floors(clarity,&ClarityController::noiseFloorChanged);
        widget->setDisplayWindowPreservingHistory(centre, 96000);
        widget->setVfoFrequency(centre);
        widget->setConnectionState(ConnectionState::Connected);
        if (threeD) {
            widget->setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        }
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        gui.setClarityRegistry(&registry);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        if (rejoined) {
            // The window sees the two receivers first.
            QTRY_VERIFY(remote.sliceById(secondId)
                && remote.sliceById(secondId)->streamIndex() >= 0
                && remote.sliceById(secondId)->streamIndex() != stream);
            if (rejoin == kJoinedOnTheCore) {
                // Tuned back into the first receiver's range: the Core puts
                // the second slice back on it and frees its own receiver.
                secondSlice->setFrequency(centre + 20000);
            } else {
                // The first slice's receiver retunes next to the second, and
                // the second comes down onto it; the higher one is freed.
                firstSlice->setFrequency(centre);
                QCOMPARE(firstSlice->streamIndex(), stream);
                QCOMPARE(station.streamCentreHz(stream), centre);
            }
            QCOMPARE(secondSlice->streamIndex(), stream);
            QCOMPARE(station.streamAllocator().activeStreamCount(), 1);
        }
        QTRY_VERIFY(remote.sliceById(secondId)
            && remote.sliceById(secondId)->streamIndex() == stream);
        QCOMPARE(client.remoteDisplayBudgetLimits().has_value(), budget);
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            for (int block = 0; block < kFeedBlocksPerPoll; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            }
        };
        const auto drawn = [&] {
            return threeD ? widget->dssRowsPushedForTest()
                          : widget->waterfallHistoryRowsForTest();
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), drawn() >= 5), 10000);
        const int before = drawn();
        QTRY_VERIFY(!floors.isEmpty());
        clarity->notifyManualOverride();
        const float retainedLow=clarity->lastLow(), retainedFloor=clarity->smoothedFloor();

        // Fix wave finding 4: the old binding's rows handed over early and
        // not yet due go with it; what is drawn stays.
        const auto rowsNotYetDue = [widget] {
            const qint64 now = widget->remoteRowNowNsForTest();
            int count = 0;
            for (int i = 0; i < widget->remoteRowQueueDepthForTest(); ++i) {
                if (widget->remoteRowDueNsForTest(i) > now) { ++count; }
            }
            return count;
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), rowsNotYetDue() > 0), 10000);

        // The operator selects the other slice's flag on this pan.
        pan->setActiveSliceIndex(secondId);
        gui.planNowForTest();
        QCOMPARE(rowsNotYetDue(), 0);
        // The pan asks for the new slice's display; until it arrives, and
        // once it does, nothing drawn so far is lost.
        for (int i = 0; i < 20; ++i) {
            QVERIFY2(drawn() >= before,
                     qPrintable(QStringLiteral("history fell from %1 to %2")
                                    .arg(before).arg(drawn())));
            feed();
            QTest::qWait(25);
        }
        QTRY_VERIFY_WITH_TIMEOUT((feed(), drawn() > before), 10000);
        // R-R3-21: this Core answers clock probes, so the new slice's trace
        // is drawn at its time on the audio's clock, a little after rows
        // captured before the switch.
        QTRY_VERIFY_WITH_TIMEOUT((feed(), !widget->renderedPixels().isEmpty()), 10000);
        QVERIFY(clarity->isPaused());
        QCOMPARE(clarity->lastLow(),retainedLow); QCOMPARE(clarity->smoothedFloor(),retainedFloor);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A slice that shares its receiver and changes band is put on another
    // receiver by the Core, which retires the pan's display on the old one.
    // The window's next ask for that pan is for the pan's view on the new
    // receiver. An ask still carrying the view the pan had on the old band
    // lies outside the new receiver, so the Core reads it as an extended
    // view: it takes the wideband path and the filter input goes to bypass
    // until the window's next ask lands. A local window never asks that way.
    // Found from a signed-in window, 2026-10-11.
    //
    // The Core says at once that the display is retired; the slice's new
    // frequency and receiver come with its next mirrored update, and the
    // pan follows its slice then. The rows: the pan follows as the update
    // lands; the pan follows only after the planner has run again; and the
    // slice is sent away and back between two updates, so this window never
    // sees it move, and its pan is asked for and drawn again all the same.
    void sliceMovedToAnotherReceiverAsksForTheViewOnThatReceiver_data()
    {
        QTest::addColumn<bool>("budget");
        QTest::addColumn<int>("scenario");
        for (const bool budget : {false, true}) {
            const QByteArray mode = budget ? QByteArrayLiteral(", display budget")
                                           : QByteArrayLiteral(", no display budget");
            QTest::newRow(QByteArray("pan follows with the update" + mode).constData())
                << budget << kFollowsWithTheUpdate;
            QTest::newRow(QByteArray("pan follows after the planner ran" + mode).constData())
                << budget << kFollowsLate;
            QTest::newRow(QByteArray("away and back between updates" + mode).constData())
                << budget << kAwayAndBack;
        }
    }
    void sliceMovedToAnotherReceiverAsksForTheViewOnThatReceiver()
    {
        QFETCH(bool, budget);
        QFETCH(int, scenario);
        constexpr double kSixMetersHz = 50'125'000.0;
        constexpr double kFortyMetersHz = 7'214'100.0;
        constexpr double kSpanHz = 96'000.0;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        // A Protocol 2 Core with a wideband ADC path: a view outside the
        // receiver is one this Core accepts and switches the filters for.
        P2RadioConnection p2;
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int movedId = station.addSlice();
        auto* movedSlice = station.sliceById(movedId);
        QVERIFY(movedSlice);
        movedSlice->setFrequency(kSixMetersHz);
        const int oldStream = movedSlice->streamIndex();
        QVERIFY(oldStream >= 0);
        const int sharingId = station.addSlice();
        auto* sharingSlice = station.sliceById(sharingId);
        QVERIFY(sharingSlice);
        sharingSlice->setFrequency(kSixMetersHz + 20'000.0);
        QCOMPARE(sharingSlice->streamIndex(), oldStream);
        QCOMPARE(movedSlice->streamIndex(), oldStream);
        int otherStream = -1;
        if (scenario == kAwayAndBack) {
            // A receiver on 40 m for the slice to join and leave again.
            auto* fortySlice = station.sliceById(station.addSlice());
            QVERIFY(fortySlice);
            fortySlice->setFrequency(kFortyMetersHz + 20'000.0);
            otherStream = fortySlice->streamIndex();
            QVERIFY(otherStream >= 0 && otherStream != oldStream);
        }
        p2.setBoardForTest(HPSDRHW::Saturn);
        station.injectConnectionForTest(&p2);
        RadioInfo info;
        info.protocol = ProtocolVersion::Protocol2;
        station.setLastRadioInfoForTest(info);
        station.wireWidebandConnectionForTest();
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        if (budget) {
            QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        }
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("pan"));
        stack.setActivePan(QStringLiteral("pan"));
        pan->addSlice(movedId);
        pan->setActiveSliceIndex(movedId);
        auto* widget = pan->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(kSixMetersHz, kSpanHz);
        widget->setVfoFrequency(kSixMetersHz);
        widget->setConnectionState(ConnectionState::Connected);
        QVERIFY(widget->extendedViewAllowed());
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        // The window's clock is the test's from the move on, so how long an
        // ask may wait is stated here and not left to how busy the machine is.
        QElapsedTimer wallClock;
        wallClock.start();
        bool clockHeld = false;
        qint64 heldClockMs = 0;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            },
            [&] { return clockHeld ? heldClockMs : wallClock.elapsed(); });
        QSignalSpy frames(&gui, &RemoteMediaController::displayFrameReceived);
        QSignalSpy windowControls(&client, &StationClient::mediaControlReceived);

        // Every ask the Core receives for the moved slice, held against the
        // receiver the Core has that slice on at that moment.
        bool moved = false;
        QStringList strays;
        QList<double> askedCenters;
        connect(&server, &StationServer::mediaControlReceived, &station,
                [&](const QJsonObject& control) {
            if (!moved || control.value(QStringLiteral("op")) != QLatin1String("subscribe")
                || control.value(QStringLiteral("sliceId")).toInt(-1) != movedId) { return; }
            const int stream = movedSlice->streamIndex();
            const double receiverCenter = station.streamCentreHz(stream);
            const double rate = station.streamSampleRateHz(stream);
            const double viewCenter = control.value(QStringLiteral("centreHz")).toDouble();
            const double span = control.value(QStringLiteral("spanHz")).toDouble();
            askedCenters.append(viewCenter);
            if (viewCenter - span / 2.0 < receiverCenter - rate / 2.0
                || viewCenter + span / 2.0 > receiverCenter + rate / 2.0) {
                strays.append(QStringLiteral("%1 Hz wide at %2 Hz, receiver %3 at %4 Hz")
                    .arg(span, 0, 'f', 0).arg(viewCenter, 0, 'f', 0)
                    .arg(stream).arg(receiverCenter, 0, 'f', 0));
            }
        });
        int wideLocks = 0;
        connect(&station.alexController(), &AlexController::bpfStateChanged, &station,
                [&wideLocks](int, const AlexController::AlexAdcState& state) {
            if (state.effective == AlexController::BpfEffective::WidebandLocked) { ++wideLocks; }
        });
        const auto strayAsks = [&strays, &wideLocks] {
            return QStringLiteral("asked for a view outside the slice's receiver: %1 "
                                  "(filters to the wideband state %2 times)")
                .arg(strays.join(QStringLiteral("; "))).arg(wideLocks);
        };

        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_VERIFY(remote.sliceById(movedId)
            && remote.sliceById(movedId)->streamIndex() == oldStream);
        SliceModel* const mirrored = remote.sliceById(movedId);
        QVERIFY(client.remoteWidebandAvailable());
        QCOMPARE(client.remoteDisplayBudgetLimits().has_value(), budget);
        // The window follows its mirrored slice as MainWindow does
        // (followRemoteSliceFrequency): a slice tuned off the pan takes the
        // pan's view with it.
        const auto follow = [widget](double hz) {
            const double halfSpan = widget->bandwidth() / 2.0;
            const bool offScreen = hz < widget->centerFrequency() - halfSpan
                || hz > widget->centerFrequency() + halfSpan;
            if (!widget->ctunEnabled() || offScreen) {
                widget->setDisplayWindowPreservingHistory(hz, widget->bandwidth());
            }
            widget->setVfoFrequency(hz);
        };
        if (scenario != kFollowsLate) {
            connect(mirrored, &SliceModel::frequencyChanged, widget, follow);
        }
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            for (int block = 0; block < kFeedBlocksPerPoll; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, movedSlice->streamIndex()), Q_ARG(QVector<float>, iq));
            }
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() >= 3), 10000);
        QCOMPARE(wideLocks, 0);
        // The Core's word to this window that the pan's display is retired
        // because its slice left the receiver.
        const auto retirements = [&windowControls] {
            int count = 0;
            for (const auto& call : windowControls) {
                if (call.at(0).toJsonObject().value(QStringLiteral("reason")).toString()
                        == QLatin1String(kRetireReasonStreamBindingChanged)) { ++count; }
            }
            return count;
        };
        QCOMPARE(retirements(), 0);

        heldClockMs = wallClock.elapsed();
        clockHeld = true;
        moved = true;
        if (scenario == kAwayAndBack) {
            // Onto the 40 m receiver and back onto its own, with no mirrored
            // update in between: to this window the slice never moved.
            const quint64 epoch = station.streamEpoch(oldStream);
            movedSlice->setFrequency(kFortyMetersHz);
            QCOMPARE(movedSlice->streamIndex(), otherStream);
            movedSlice->setFrequency(kSixMetersHz);
            QCOMPARE(movedSlice->streamIndex(), oldStream);
            QCOMPARE(station.streamEpoch(oldStream), epoch);
            QTRY_VERIFY(retirements() > 0);
            gui.planNowForTest();
            QCOMPARE(mirrored->streamIndex(), oldStream);
            QCOMPARE(mirrored->streamEpoch(), epoch);
            // The window waits for word of where the slice went, and no
            // longer than kReceiverMoveSettleMs: the pan is then asked for
            // as it stands.
            heldClockMs += RemoteMediaController::kReceiverMoveSettleMs;
            gui.planNowForTest();
            QTRY_VERIFY_WITH_TIMEOUT(!askedCenters.isEmpty(), 10000);
        } else {
            // The operator sends the slice to 40 m. It shared its receiver,
            // so the Core places it on another one.
            movedSlice->setFrequency(kFortyMetersHz);
            const int newStream = movedSlice->streamIndex();
            QVERIFY(newStream >= 0 && newStream != oldStream);
            QCOMPARE(sharingSlice->streamIndex(), oldStream);
            if (scenario == kFollowsLate) {
                // The mirrored slice shows its new receiver and the planner
                // runs, when told to and on its own tick, before the pan has
                // followed the slice: the old view is not asked for.
                QTRY_COMPARE(mirrored->streamIndex(), newStream);
                QCOMPARE(mirrored->frequency(), kFortyMetersHz);
                QTRY_VERIFY(retirements() > 0);
                gui.planNowForTest();
                QTest::qWait(3 * RemoteMediaController::kPlannerIntervalMs);
                QVERIFY2(strays.isEmpty(), qPrintable(strayAsks()));
                QCOMPARE(wideLocks, 0);
                follow(mirrored->frequency());
            }
            // The pan is asked for at its view on the new receiver.
            QTRY_VERIFY_WITH_TIMEOUT(!askedCenters.isEmpty()
                && std::abs(askedCenters.last() - kFortyMetersHz) < kSpanHz, 10000);
        }
        // It is drawn again, from the receiver the slice is on.
        const int framesAtAsk = frames.count();
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() > framesAtAsk), 10000);
        for (int i = 0; i < 12; ++i) {
            feed();
            QTest::qWait(25);
        }
        QVERIFY2(strays.isEmpty(), qPrintable(strayAsks()));
        QCOMPARE(wideLocks, 0);
        QCOMPARE(p2.wbEnableMask(), quint8(0));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // JJ's reported trigger is the TX applet's unkeyed A/B letters, not
    // active receive-slice selection. Exercise actual media subscriptions.
    void unkeyedTransmitLettersKeepSharedReceiveHistory_data()
    {
        QTest::addColumn<bool>("threeD");
        QTest::addColumn<bool>("budget");
        QTest::newRow("3D stack") << true << false;
        QTest::newRow("3D stack, display budget") << true << true;
    }
    void unkeyedTransmitLettersKeepSharedReceiveHistory()
    {
        QFETCH(bool, threeD);
        QFETCH(bool, budget);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int firstId = station.addSlice();
        auto* firstSlice = station.sliceById(firstId);
        QVERIFY(firstSlice);
        const int stream = firstSlice->streamIndex();
        QVERIFY(stream >= 0);
        const int secondId = station.addSlice();
        auto* secondSlice = station.sliceById(secondId);
        QVERIFY(secondSlice);
        firstSlice->setFrequency(3650000);
        secondSlice->setFrequency(3651000);
        QVERIFY(station.moveSlicesToStream({firstId, secondId}, stream, 3650000));
        const double centre = station.streamCentreHz(stream);
        const quint64 streamEpoch = firstSlice->streamEpoch();
        QCOMPARE(secondSlice->streamIndex(), stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        server.setRemoteTransmitAllowed(true);
        server.setTokenSessionsMayTransmitForTest(true);
        if (budget) {
            QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        }
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setTokenSessionHolderForTest(QStringLiteral("token:1"));
        client.declareFeatureForTest(QByteArrayLiteral("deviceAuth"), 1);
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("pan"));
        stack.setActivePan(QStringLiteral("pan"));
        pan->addSlice(firstId);
        pan->addSlice(secondId);
        pan->setActiveSliceIndex(firstId);
        auto* widget = pan->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 96000);
        widget->setVfoFrequency(centre);
        widget->setConnectionState(ConnectionState::Connected);
        if (threeD) {
            widget->setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        }
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_VERIFY(remote.sliceById(secondId)
            && remote.sliceById(secondId)->streamIndex() == stream);
        QCOMPARE(client.remoteDisplayBudgetLimits().has_value(), budget);
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            for (int block = 0; block < kFeedBlocksPerPoll; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            }
        };
        const auto drawn = [&] {
            return threeD ? widget->dssRowsPushedForTest()
                          : widget->waterfallHistoryRowsForTest();
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), drawn() >= 5), 10000);
        const int before = drawn();
        const double viewCentre = widget->centerFrequency();

        TransmitHolder::Holder self;
        self.deviceId = QByteArrayLiteral("token:1");
        self.name = QStringLiteral("History media bench");
        server.transmitHolder()->transferTo(self, QStringLiteral("test"));
        QTRY_VERIFY(client.holdsTransmitHere());
        QVERIFY(client.sessionHolderAvailable());
        QVERIFY(client.remoteTransmitAvailable());
        TxApplet applet(&remote);
        applet.setTransmitSliceChoices({}, [&client](int id) { client.requestTxSlice(id); });
        QSignalSpy finished(&client, &StationClient::deviceCommandFinished);
        const int active = pan->activeSliceIndex();
        const double span = widget->bandwidth();
        int minimum = before;
        QTimer sampler;
        QObject::connect(&sampler, &QTimer::timeout, &gui, [&]() {
            minimum = std::min(minimum, drawn());
        });
        sampler.start(1);
        for (int id : {firstId, secondId, firstId}) {
            QPushButton* letter = nullptr;
            for (QPushButton* button : applet.transmitSliceButtons()) {
                if (button->property("sliceId").toInt() == id) { letter = button; }
            }
            QVERIFY(letter && letter->isEnabled());
            finished.clear();
            letter->click();
            QTRY_VERIFY(!finished.isEmpty());
            QCOMPARE(finished.last().at(0).toByteArray(), QByteArrayLiteral("tx.setTxSlice"));
            QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
            QTRY_VERIFY(remote.sliceById(id)->isTxSlice());
            QCOMPARE(station.txSliceArbiter()->txBoundSliceId(), id);
            for (int i = 0; i < 8; ++i) {
                QVERIFY2(drawn() >= before,
                         qPrintable(QStringLiteral("history fell from %1 to %2")
                                        .arg(before).arg(drawn())));
                QVERIFY(!station.mox() && !remote.mox());
                QCOMPARE(firstSlice->streamEpoch(), streamEpoch);
                QCOMPARE(secondSlice->streamEpoch(), streamEpoch);
                QCOMPARE(pan->activeSliceIndex(), active);
                QCOMPARE(widget->centerFrequency(), viewCentre);
                QCOMPARE(widget->bandwidth(), span);
                feed();
                QTest::qWait(25);
            }
            QCOMPARE(minimum, before);
        }
        sampler.stop();
        QTRY_VERIFY_WITH_TIMEOUT((feed(), drawn() > before), 10000);
        // R-R3-21: this Core answers clock probes, so the new slice's trace
        // is drawn at its time on the audio's clock, a little after rows
        // captured before the switch.
        QTRY_VERIFY_WITH_TIMEOUT((feed(), !widget->renderedPixels().isEmpty()), 10000);
        client.disconnectFromStation(QStringLiteral("test complete"));
        // Genuine session retirement must still clear the old source.
        QTRY_COMPARE(widget->dssRowsPushedForTest(), 0);
        QCOMPARE(widget->waterfallHistoryRowsForTest(), 0);
    }

    // R-R3-19: a pan whose stream is its own still re-centres the Core on a
    // zoom, as before this hotfix; the Core always accepts that centre.
    void ctunZoomOnOwnStreamStillMovesCoreCentre()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* sourceSlice = station.sliceById(sliceId);
        QVERIFY(sourceSlice);
        const int stream = sourceSlice->streamIndex();
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        stack.setActivePan(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceId);
        auto* widget = first->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        widget->setVfoFrequency(centre);
        widget->setCtunEnabled(true);
        // MainWindow's connection wiring; a disconnected pan swallows clicks.
        widget->setConnectionState(ConnectionState::Connected);
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048, 0.001f);
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !widget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feed(), 5000);
        QTRY_VERIFY(sourceSlice->streamCtunPinned());
        sourceSlice->setFrequency(centre - 30000);
        QCOMPARE(station.streamCentreHz(stream), centre);
        QTRY_COMPARE(remote.sliceById(sliceId)->frequency(), centre - 30000);
        // The window's pan takes C-Tune from the Core's pin once the pin is
        // mirrored and the pan's spectrum context is accepted; until then a
        // zoom is a view change only. Wait for it, as a user would see it.
        QTRY_VERIFY(widget->ctunEnabled());
        widget->setVfoFrequency(centre - 30000);
        QSignalSpy notices(&remote, &RadioModel::sliceAddRejected);
        const QPointF inSpectrum(widget->width() * 0.25, widget->height() * 0.2);
        QWheelEvent wheel(inSpectrum, widget->mapToGlobal(inSpectrum), QPoint(),
                          QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(widget, &wheel);
        QTRY_COMPARE(station.streamCentreHz(stream), centre - 30000);
        QCOMPARE(notices.size(), 0);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A receiver that leaves the Core takes its display with it, and the
    // window does not ask the Core for that display again. It used to: the
    // Core's word that the display had ended could arrive before the word
    // that the slice was gone, the window planned at once with the slice
    // still on its list, and the Core refused the new request ("This
    // display's receiver is not on the Core.").
    void removedReceiverIsNotAskedForADisplayAgain()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceA = station.addSlice();
        QVERIFY(station.sliceById(sliceA));
        station.sliceById(sliceA)->setFrequency(14200000.0);
        const int sliceB = station.addSlice();
        QVERIFY(station.sliceById(sliceB));
        station.sliceById(sliceB)->setFrequency(7150000.0);
        const int streamA = station.sliceById(sliceA)->streamIndex();
        const int streamB = station.sliceById(sliceB)->streamIndex();
        QVERIFY(streamA >= 0 && streamB >= 0 && streamA != streamB);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        // A Core with a display budget, roomy enough to refuse nothing.
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        auto* second = stack.addPanadapter(QStringLiteral("second"));
        stack.setActivePan(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceA);
        first->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(streamA), 48000);
        second->setActiveSliceIndex(sliceB);
        second->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(streamB), 48000);
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        // As the main window does: a slice that is removed leaves every pan.
        connect(&remote, &RadioModel::sliceRemoved, &stack, [&stack](int sliceIndex) {
            for (PanadapterApplet* applet : stack.allApplets()) {
                if (applet) { applet->removeSlice(sliceIndex); }
            }
        });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        QTRY_COMPARE(gui.activeEndpointCount(), 2);
        // Let the planner settle: both displays accepted, nothing pending.
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 2);
        QTest::qWait(300);
        outbound.clear();
        inbound.clear();

        station.removeSlice(sliceB);
        QTRY_VERIFY(!remote.sliceById(sliceB));
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_COMPARE(gui.activeEndpointCount(), 1);
        // Well past the window's 100 ms planning tick.
        QTest::qWait(500);

        // The Core ends the display once and says why: its receiver was
        // removed. That is the word the window waits on.
        const QList<QJsonObject> answers =
            controlsFor(inbound, QStringLiteral("allocation-result"));
        QVERIFY(!answers.isEmpty());
        QCOMPARE(answers.first().value(QStringLiteral("reason")).toString(),
                 QString::fromLatin1(kRetireReasonSliceRemoved));
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QVERIFY2(asked.value(QStringLiteral("sliceId")).toInt() != sliceB,
                     "the window asked for the removed receiver's display");
        }
        for (const QJsonObject& answer : answers) {
            QVERIFY2(answer.value(QStringLiteral("reason")).toString()
                         != QStringLiteral("This display's receiver is not on the Core."),
                     "the Core refused a display request for the removed receiver");
        }
        // The first pan's display was never touched.
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 0);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The other side of the wait above: the Core says a display ended
    // because its slice was removed, and the removal never reaches this
    // window. The pan does not sit blank for good. Once the grace in which a
    // waiting pan says nothing is over, the display is asked for after all,
    // and the Core's answer decides.
    void displayEndedForARemovedSliceIsAskedForAfterTheGrace()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        station.sliceById(sliceId)->setFrequency(14200000.0);
        const int stream = station.sliceById(sliceId)->streamIndex();
        QVERIFY(stream >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(stream), 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 nowMs = 0;
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            }, [&nowMs] { return nowMs; }, 10'000);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_COMPARE(gui.activeEndpointCount(), 1);
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 1);
        // Let the planner settle: the display accepted, nothing pending.
        QTest::qWait(300);
        const QJsonObject accepted = lastControl(inbound, QStringLiteral("allocation-result"));
        QVERIFY(accepted.value(QStringLiteral("accepted")).toBool());
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : gui.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        const int asksBefore = countControl(outbound, QStringLiteral("subscribe"));
        const int answersBefore = countControl(inbound, QStringLiteral("allocation-result"));

        // The Core's word that the display ended, as DaemonMediaController
        // sends it for a removed slice: nothing held, and the reason. The
        // slice stays on both lists.
        QJsonObject ended = accepted;
        ended.insert(QStringLiteral("accepted"), false);
        ended.insert(QStringLiteral("reason"), QString::fromLatin1(kRetireReasonSliceRemoved));
        ended.insert(QStringLiteral("acceptedRevision"), 0);
        ended.insert(QStringLiteral("applicationBytesPerSecond"), 0);
        ended.insert(QStringLiteral("spectrumSampleUnitsPerSecond"), 0);
        ended.insert(QStringLiteral("messagesPerSecond"), 0);
        QVERIFY(server.sendMediaControl(ended, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")),
                     answersBefore + 1);
        QVERIFY(remote.sliceById(sliceId) != nullptr);

        // Inside the grace: nothing is asked, and the pan says nothing.
        nowMs = RemoteMediaController::kPanWaitingGraceMs - 1;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QTest::qWait(250);
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), asksBefore);
        QVERIFY2(applet->remoteDisplayStatus().isEmpty(),
                 qPrintable(applet->remoteDisplayStatus()));

        // The grace is over and the slice is still here: ask.
        nowMs = RemoteMediaController::kPanWaitingGraceMs;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), asksBefore + 1);
        QCOMPARE(lastControl(outbound, QStringLiteral("subscribe"))
                     .value(QStringLiteral("sliceId")).toInt(), sliceId);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void sharedWindowChangeAndRadioReconnectResumeBothPanes()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadWindow = appSettings.contains(QStringLiteral("DisplayFftWindow"));
        const bool hadFft = appSettings.contains(QStringLiteral("DisplayFftSize"));
        const QVariant savedWindow = appSettings.value(QStringLiteral("DisplayFftWindow"));
        const QVariant savedFft = appSettings.value(QStringLiteral("DisplayFftSize"));
        appSettings.setValue(QStringLiteral("DisplayFftWindow"), QString::number(int(WindowFunction::Hann)));
        appSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("4096"));
        const auto restore = qScopeGuard([&] {
            if (hadWindow) { appSettings.setValue(QStringLiteral("DisplayFftWindow"), savedWindow); }
            else { appSettings.remove(QStringLiteral("DisplayFftWindow")); }
            if (hadFft) { appSettings.setValue(QStringLiteral("DisplayFftSize"), savedFft); }
            else { appSettings.remove(QStringLiteral("DisplayFftSize")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        StepAttenuatorController stationAttenuator(&station);
        stationAttenuator.setStepAttEnabled(true);
        stationAttenuator.setAttenuation(0);
        station.setStepAttController(&stationAttenuator);
        const auto detachStationAttenuator = qScopeGuard([&] {
            station.setStepAttController(nullptr);
        });
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        PanClarityRegistry registry(&remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        auto* second = stack.addPanadapter(QStringLiteral("second"));
        stack.setActivePan(QStringLiteral("first"));
        registry.registerPan(QStringLiteral("first"), first->spectrumWidget(), nullptr);
        registry.registerPan(QStringLiteral("second"), second->spectrumWidget(), nullptr);
        QSignalSpy liveFloors(registry.controllerForPan(QStringLiteral("first")), &ClarityController::noiseFloorChanged);
        QSignalSpy otherFloors(registry.controllerForPan(QStringLiteral("second")), &ClarityController::noiseFloorChanged);
        for (auto* applet : {first, second}) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            // Parity Task 17: the Core quantises on the pan's own range, so
            // the pans show -180 to 0 dBm, where the fixture's noise lies;
            // and it averages at the pan's averaging time, so the shortest
            // lets a level step show within a frame or two.
            applet->spectrumWidget()->setDbmRange(-180.0f, 0.0f);
            applet->spectrumWidget()->setSpectrumAverageTimeMs(10);
        }
        stack.resize(600, 700);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        gui.setClarityRegistry(&registry);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        // Feed real tagged ingress in bounded radio-sized chunks. Repeated
        // calls also wait for the asynchronous source configuration to finish.
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            // Keep the station-calibration matrix well below the codec's
            // 0 dBm ceiling; otherwise a positive offset can hide a shift.
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        const auto bothHaveFrames = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !first->spectrumWidget()->renderedPixels().isEmpty()
                && !second->spectrumWidget()->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(bothHaveFrames(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!liveFloors.isEmpty() && !otherFloors.isEmpty(), 5000);
        QCOMPARE(countControl(inbound, QStringLiteral("rejected")), 0);

        // DaemonMediaController applies the station offset before reducing
        // and encoding. Exercise all authoritative receiver paths using the
        // same tagged I/Q, rather than relying on the remote client's local
        // DisplayCalOffset preference.
        const auto traceLevels = [&] {
            QVector<float> bins = first->spectrumWidget()->renderedPixels();
            std::sort(bins.begin(), bins.end());
            return qMakePair(bins.last(), bins.at(bins.size() / 2));
        };
        const auto feedAndAwaitOffset = [&](double expectedOffsetDb,
                                            const QPair<float, float>& referenceDbm) {
            return QTest::qWaitFor([&] {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
                const float offset = static_cast<float>(expectedOffsetDb);
                const auto levels = traceLevels();
                return std::abs(levels.first - (referenceDbm.first + offset)) < 1.0f
                    && std::abs(levels.second - (referenceDbm.second + offset)) < 1.0f;
            }, 5000);
        };
        // R-R3-49 load round: the first frames of a new stream come from a
        // part-filled FFT with the Core's averaging still rising, so a trace
        // read at the first frame can sit 3.5 dB under the steady one. A
        // loaded run took that as its reference (6 of 53 at load 50 to 90)
        // and then waited for a level the steady trace never reaches. The
        // reference is the trace once it holds across newly arrived frames
        // (a waterfall row is pushed only for a frame that arrived).
        const auto steadyTrace = [&](QPair<float, float>& steady) {
            QPair<float, float> previous = traceLevels();
            quint64 previousRows = first->spectrumWidget()->remoteRowsPushedForTest();
            return QTest::qWaitFor([&] {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
                const quint64 rows = first->spectrumWidget()->remoteRowsPushedForTest();
                if (rows < previousRows + 2) {
                    return false;
                }
                const auto levels = traceLevels();
                const bool held = std::abs(levels.first - previous.first) < 0.1f
                    && std::abs(levels.second - previous.second) < 0.1f;
                previous = levels;
                previousRows = rows;
                if (held) {
                    steady = levels;
                }
                return held;
            }, 5000);
        };
        QPair<float, float> baseTraceDbm;
        QVERIFY(steadyTrace(baseTraceDbm));
        const double baseOffsetDb = station.rxMeterOffsetDb();
        stationAttenuator.setAttenuation(10);
        QVERIFY(feedAndAwaitOffset(station.rxMeterOffsetDb() - baseOffsetDb, baseTraceDbm));
        const auto stepAttTraceDbm = traceLevels();
        stationAttenuator.setStepAttEnabled(false);
        stationAttenuator.setPreampMode(PreampMode::Off);
        QVERIFY(feedAndAwaitOffset(station.rxMeterOffsetDb() - baseOffsetDb, baseTraceDbm));
        const auto preampOffTraceDbm = traceLevels();
        stationAttenuator.setPreampMode(PreampMode::On);
        QVERIFY(feedAndAwaitOffset(station.rxMeterOffsetDb() - baseOffsetDb, baseTraceDbm));
        const auto preampOnTraceDbm = traceLevels();
        QVERIFY(std::abs(stepAttTraceDbm.first - baseTraceDbm.first) > 1.0f);
        QVERIFY(std::abs(preampOffTraceDbm.first - stepAttTraceDbm.first) > 1.0f);
        QVERIFY(std::abs(preampOnTraceDbm.first - preampOffTraceDbm.first) > 1.0f);
        outbound.clear();
        inbound.clear();
        appSettings.setValue(QStringLiteral("DisplayFftWindow"), QString::number(int(WindowFunction::BlackmanHarris4)));
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 2);
        // Both old users of the source must retire before either replacement.
        QStringList operations;
        for (const auto& call : outbound) {
            const QString op = call.at(0).toJsonObject().value(QStringLiteral("op")).toString();
            if (op == QStringLiteral("subscribe") || op == QStringLiteral("unsubscribe")) {
                operations.append(op);
            }
        }
        QCOMPARE(operations, QStringList({QStringLiteral("unsubscribe"), QStringLiteral("unsubscribe"),
                                         QStringLiteral("subscribe"), QStringLiteral("subscribe")}));
        QTRY_VERIFY_WITH_TIMEOUT(bothHaveFrames(), 5000);
        QTRY_COMPARE(countControl(inbound, QStringLiteral("context")), 2);
        QCOMPARE(countControl(inbound, QStringLiteral("rejected")), 0);

        station.setConnectionStateForTest(ConnectionState::LinkLost);
        QTRY_COMPARE(gui.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        QVERIFY(first->spectrumWidget()->renderedPixels().isEmpty());
        QVERIFY(second->spectrumWidget()->renderedPixels().isEmpty());
        station.setConnectionStateForTest(ConnectionState::Disconnected);
        QCOMPARE(daemon.activeEndpointCount(), 0);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(bothHaveFrames(), 5000);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void synchronousControlClosureRetiresGuiBindings_data()
    {
        QTest::addColumn<QString>("trigger");
        QTest::newRow("pane removal") << QStringLiteral("remove");
        QTest::newRow("stack destruction") << QStringLiteral("destroy");
        QTest::newRow("radio disconnect") << QStringLiteral("disconnect");
        QTest::newRow("subscription renewal") << QStringLiteral("renew");
    }

    void synchronousControlClosureRetiresGuiBindings()
    {
        QFETCH(QString, trigger);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto stack = std::make_unique<PanadapterStack>();
        stack->applyLayout(QStringLiteral("2v"),
                           {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        for (PanadapterApplet* applet : stack->allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(
                station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        }
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, stack.get(), nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new ClosingGuiControlTransport;
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(controller.activeEndpointCount(), 2);
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        int endpointCountAtClose = -1;
        clientLink->beforeClose = [&] { endpointCountAtClose = controller.activeEndpointCount(); };
        clientLink->closeOnOp = trigger == QLatin1String("renew") ? "subscribe" : "unsubscribe";
        if (trigger == QLatin1String("remove")) {
            stack->removePanadapter(QStringLiteral("pan-0"));
        } else if (trigger == QLatin1String("destroy")) {
            stack.reset();
        } else if (trigger == QLatin1String("disconnect")) {
            station.setConnectionStateForTest(ConnectionState::Disconnected);
        } else {
            SpectrumWidget* widget = stack->spectrum(QStringLiteral("pan-0"));
            widget->setDisplayWindowPreservingHistory(widget->centerFrequency(), 24000);
        }
        QTRY_VERIFY(endpointCountAtClose >= 0);
        // A removal releases its binding before the send can re-enter stop().
        QCOMPARE(endpointCountAtClose, trigger == QLatin1String("renew") ? 2 : 1);
        QTRY_VERIFY(!client.mediaAvailable());
        QCOMPARE(controller.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeSourceCount(), 0);
    }

    // NB Blank's replaced rows reach the diagnostics line as the sum over
    // every bound pan (displayRowsBlanked): one pan blanks one row, the
    // other two, and the window reports three.
    void displayTelemetrySumsEveryPansBlankedRows()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        stack.resize(800, 800);
        stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(
                station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(controller.activeEndpointCount(), 2);
        QCOMPARE(controller.displayTelemetry().rowsBlanked, quint64(0));

        // Each pan: 10 quiet rows of history, then `pulses` rows 10 dB up
        // across the whole row, each followed by a quiet row.
        const auto blankRows = [](SpectrumWidget* widget, int pulses) {
            const int n = widget->liveWaterfallForTest().width();
            QVERIFY(n > 0);
            widget->setWaterfallBlankingEnabled(true);
            for (int i = 0; i < 10; ++i) {
                widget->pushWaterfallRowForTest(QVector<float>(n, -110.0f));
            }
            for (int i = 0; i < pulses; ++i) {
                widget->pushWaterfallRowForTest(QVector<float>(n, -100.0f));
                widget->pushWaterfallRowForTest(QVector<float>(n, -110.0f));
            }
            QCOMPARE(widget->waterfallRowsBlanked(), quint64(pulses));
        };
        blankRows(stack.spectrum(QStringLiteral("pan-0")), 1);
        blankRows(stack.spectrum(QStringLiteral("pan-1")), 2);
        QCOMPARE(controller.displayTelemetry().rowsBlanked, quint64(3));
    }

    void logicalPanLifecycleKeepsMediaAcrossReparentAndRetiresItOnRemoval()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        SliceModel* stationSlice = station.sliceById(sliceId);
        QVERIFY(stationSlice);
        const int stream = stationSlice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto stack = std::make_unique<PanadapterStack>();
        // A failed check returns straight out of this test, and the stack
        // would be destroyed without the event loop having run since the
        // pan last moved between windows. The offscreen platform's backing
        // store then faults: the floating window and the stack have both
        // flushed the pan's native windows, Qt's static map remembers only
        // the store that flushed last, and whichever store is destroyed
        // second looks up a key the first one erased (QOffscreenBackingStore
        // ::clearHash has no end() check). One failure then took the whole
        // binary down with SIGSEGV. On a failed check, put the pan back in
        // the stack and wait for the retired floating window to be deleted,
        // which PanadapterStack does only once the pan has painted in the
        // stack; then the stack owns those keys and teardown is orderly, as
        // it is at the end of a passing run.
        QPointer<PanFloatingWindow> floatedWindow;
        const auto stackTeardown = qScopeGuard([&stack, &floatedWindow] {
            if (!stack) { return; }
            if (floatedWindow) {
                if (stack->floatingWindowForTest(QStringLiteral("pan-0"))) {
                    stack->dockPanadapter(QStringLiteral("pan-0"));
                }
                if (!QTest::qWaitFor([&floatedWindow] { return floatedWindow.isNull(); },
                                     5000)) {
                    qWarning("The floating pan window was not retired before teardown.");
                }
            }
            stack.reset();
        });
        PanadapterApplet* first = stack->addPanadapter(QStringLiteral("pan-0"));
        first->setActiveSliceIndex(sliceId);
        SpectrumWidget* firstWidget = first->spectrumWidget();
        firstWidget->setDisplayWindowPreservingHistory(centre, 48000);
        firstWidget->setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        firstWidget->setWfUpdatePeriodMs(20);
        stack->resize(600, 400);
        stack->show();
        QVERIFY(QTest::qWaitForWindowExposed(stack.get()));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, stack.get(), nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->other = sinkMedia;
        sinkMedia->other = sourceMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_COMPARE(daemon.activeSourceCount(), 1);
        QTRY_COMPARE(controller.activeEndpointCount(), 1);
        QVERIFY(!client.remoteDisplayBudgetLimits());
        QVERIFY(first->remoteDisplayStatus().isEmpty());
        const QJsonObject firstSubscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 firstEndpoint = quint32(firstSubscription.value(QStringLiteral("endpointId")).toDouble());
        QVERIFY(firstEndpoint != 0);

        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        const auto feedFirst = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !firstWidget->renderedPixels().isEmpty()
                && firstWidget->dssRowsPushedForTest() > 0;
        };
        QTRY_VERIFY_WITH_TIMEOUT(feedFirst(), 5000);
        QVERIFY(first->remoteDisplayStatus().isEmpty());

        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) {
                QVERIFY(!subscriptionTimer);
                subscriptionTimer = timer;
            }
        }
        QVERIFY(subscriptionTimer);
        const auto fireSubscriptionTimer = [&] {
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        };
        // The first frames move the pan's waterfall AGC levels, and the dBm
        // window the pan asks the Core for follows them only once they have
        // held for a frame period (33 ms at 30 fps) between two planner
        // passes (settleDbmWindows). That renewal of this endpoint can still
        // be owed here; left to whichever planner tick comes next, it landed
        // inside the float below about 3 runs in 8 and was counted as a
        // reparent subscribe. Run the planner until neither a subscribe nor
        // a painted row has come for 250 ms, so the baseline below is the
        // settled one and the reparent checks stay exact.
        int lastSubscriptions = -1;
        int lastRows = -1;
        QElapsedTimer quietFor;
        QVERIFY(QTest::qWaitFor([&] {
            QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection);
            const int subscriptions = countControl(controls, QStringLiteral("subscribe"));
            const int rows = firstWidget->dssRowsPushedForTest();
            if (subscriptions != lastSubscriptions || rows != lastRows) {
                lastSubscriptions = subscriptions;
                lastRows = rows;
                quietFor.start();
            }
            return quietFor.elapsed() >= 250;
        }, 5000));
        const int rowsBeforeReparent = firstWidget->dssRowsPushedForTest();
        const QByteArray stalePacket = sourceMedia->displayPackets.constLast();
        QVERIFY(!stalePacket.isEmpty());
        const int subscriptionsBeforeReparent = countControl(controls, QStringLiteral("subscribe"));
        const int unsubscriptionsBeforeReparent = countControl(controls, QStringLiteral("unsubscribe"));

        stack->floatPanadapter(QStringLiteral("pan-0"));
        floatedWindow = stack->floatingWindowForTest(QStringLiteral("pan-0"));
        QVERIFY(floatedWindow);
        QVERIFY(!firstWidget->isVisible());
        fireSubscriptionTimer();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), subscriptionsBeforeReparent);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), unsubscriptionsBeforeReparent);
        QCOMPARE(controller.activeEndpointCount(), 1);
        QCOMPARE(daemon.activeEndpointCount(), 1);
        QCOMPARE(firstWidget->dssRowsPushedForTest(), rowsBeforeReparent);
        QTRY_VERIFY(firstWidget->isVisible());

        stack->dockPanadapter(QStringLiteral("pan-0"));
        QVERIFY(!firstWidget->isVisible());
        fireSubscriptionTimer();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), subscriptionsBeforeReparent);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), unsubscriptionsBeforeReparent);
        QCOMPARE(controller.activeEndpointCount(), 1);
        QCOMPARE(daemon.activeEndpointCount(), 1);
        QCOMPARE(firstWidget->dssRowsPushedForTest(), rowsBeforeReparent);
        QTRY_VERIFY(firstWidget->isVisible());

        // Rebuilding a layout around the same logical pan may renew runtime
        // geometry, but it must retain the endpoint identity and its history.
        stack->applyLayout(QStringLiteral("1"), {QStringLiteral("pan-0")});
        QTRY_VERIFY(firstWidget->isVisible());
        QTRY_COMPARE(controller.activeEndpointCount(), 1);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), unsubscriptionsBeforeReparent);
        for (const auto& call : controls) {
            const QJsonObject control = call.at(0).toJsonObject();
            if (control.value(QStringLiteral("op")) == QLatin1String("subscribe")) {
                QCOMPARE(quint32(control.value(QStringLiteral("endpointId")).toDouble()), firstEndpoint);
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(feedFirst()
            && firstWidget->dssRowsPushedForTest() > rowsBeforeReparent, 5000);

        // A layout shrink is a real retirement, unlike the reparenting above.
        PanadapterApplet* second = stack->addPanadapter(QStringLiteral("pan-1"));
        second->setActiveSliceIndex(sliceId);
        second->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
        stack->applyLayout(QStringLiteral("2v"),
                           {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        QTRY_COMPARE(daemon.activeSourceCount(), 1);
        QPointer<PanadapterApplet> retiredByLayout(second);
        QPointer<SpectrumWidget> retiredWidgetByLayout(second->spectrumWidget());
        stack->applyLayout(QStringLiteral("1"), {QStringLiteral("pan-0")});
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_COMPARE(daemon.activeSourceCount(), 1);
        QTRY_COMPARE(controller.activeEndpointCount(), 1);
        QTRY_COMPARE(countControl(controls, QStringLiteral("unsubscribe")),
                     unsubscriptionsBeforeReparent + 1);
        QTRY_VERIFY(retiredByLayout.isNull());
        QTRY_VERIFY(retiredWidgetByLayout.isNull());

        // Last logical pan removal releases its endpoint and source. Do not
        // touch the pointers after this deferred QObject destruction path.
        QPointer<PanadapterApplet> retiredFirst(first);
        QPointer<SpectrumWidget> retiredFirstWidget(firstWidget);
        stack->removePanadapter(QStringLiteral("pan-0"));
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeSourceCount(), 0);
        QTRY_COMPARE(controller.activeEndpointCount(), 0);
        QTRY_COMPARE(countControl(controls, QStringLiteral("unsubscribe")),
                     unsubscriptionsBeforeReparent + 2);
        QTRY_VERIFY(retiredFirst.isNull());
        QTRY_VERIFY(retiredFirstWidget.isNull());

        // Reusing the pan id creates a fresh endpoint. A retained packet for
        // the retired id must not be decoded into that replacement widget.
        PanadapterApplet* replacement = stack->addPanadapter(QStringLiteral("pan-0"));
        replacement->setActiveSliceIndex(sliceId);
        SpectrumWidget* replacementWidget = replacement->spectrumWidget();
        replacementWidget->setDisplayWindowPreservingHistory(centre, 48000);
        replacementWidget->setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        replacementWidget->setWfUpdatePeriodMs(20);
        replacement->show();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_COMPARE(daemon.activeSourceCount(), 1);
        const QJsonObject replacementSubscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 replacementEndpoint = quint32(
            replacementSubscription.value(QStringLiteral("endpointId")).toDouble());
        QVERIFY(replacementEndpoint != 0);
        QVERIFY(replacementEndpoint != firstEndpoint);
        const int framesBeforeStaleDelivery = frames.size();
        sinkMedia->deliver(stalePacket);
        QCOMPARE(frames.size(), framesBeforeStaleDelivery);
        QVERIFY(replacementWidget->renderedPixels().isEmpty());
        const auto feedReplacement = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return !replacementWidget->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(feedReplacement(), 5000);

        // Controller outlives the stack. Stack destruction must retire its
        // current logical pan rather than retaining a dangling binding.
        QPointer<PanadapterStack> destroyedStack(stack.get());
        QPointer<SpectrumWidget> destroyedWidget(replacementWidget);
        stack.reset();
        QTRY_VERIFY(destroyedStack.isNull());
        QTRY_VERIFY(destroyedWidget.isNull());
        QTRY_COMPARE(controller.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeSourceCount(), 0);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void staleZeroAllocationResultCannotRetireNewerReservation()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        // Refused row: Core's state moves ahead of the GUI's view. When the
        // survivor asks again, Core already holds PureSignal display, which
        // the GUI has not heard of yet. Connected before Core's own handler,
        // so it runs first.
        const auto armedEndpoint = std::make_shared<quint32>(0);
        connect(&server, &StationServer::mediaControlReceived, &station,
            [&station, armedEndpoint](const QJsonObject& control) {
                if (*armedEndpoint != 0
                    && control.value(QStringLiteral("op")) == QLatin1String("subscribe")
                    && quint32(control.value(QStringLiteral("endpointId")).toDouble())
                        == *armedEndpoint) {
                    *armedEndpoint = 0;
                    station.pureSignalFacade()->setRemoteAmpViewSubscribed(true);
                }
            });
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        SpectrumWidget* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 1);
        const QJsonObject first = lastControl(outbound, QStringLiteral("subscribe"));
        const quint32 firstRevision = quint32(first.value(QStringLiteral("revision")).toDouble());
        const quint32 endpointId = quint32(first.value(QStringLiteral("endpointId")).toDouble());

        widget->setCenterFrequency(widget->centerFrequency() + 500);
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 2);
        const QJsonObject second = lastControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(quint32(second.value(QStringLiteral("revision")).toDouble()) > firstRevision);
        // Accepted at the requested quality: no line, as in legacy mode.
        QVERIFY(showsDisplay(controller, applet));
        QVERIFY(applet->remoteDisplayStatus().isEmpty());

        const QJsonObject stale{
            {QStringLiteral("op"), QStringLiteral("allocation-result")},
            {QStringLiteral("connectionId"), first.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), static_cast<qint64>(firstRevision)},
            {QStringLiteral("accepted"), false},
            {QStringLiteral("reason"), QStringLiteral("delayed refusal")},
            {QStringLiteral("budgetGeneration"), 1},
            {QStringLiteral("acceptedRevision"), 0},
            {QStringLiteral("applicationBytesPerSecond"), 0},
            {QStringLiteral("spectrumSampleUnitsPerSecond"), 0},
            {QStringLiteral("messagesPerSecond"), 0}};
        QVERIFY(server.sendMediaControl(stale, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 3);

        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QCOMPARE(controller.activeEndpointCount(), 1);
        QVERIFY(showsDisplay(controller, applet));
        QVERIFY(applet->remoteDisplayStatus().isEmpty());
    }

    void missingAllocationAcknowledgementStallsThenLateResultReconciles()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        // Refused row: Core's state moves ahead of the GUI's view. When the
        // survivor asks again, Core already holds PureSignal display, which
        // the GUI has not heard of yet. Connected before Core's own handler,
        // so it runs first.
        const auto armedEndpoint = std::make_shared<quint32>(0);
        connect(&server, &StationServer::mediaControlReceived, &station,
            [&station, armedEndpoint](const QJsonObject& control) {
                if (*armedEndpoint != 0
                    && control.value(QStringLiteral("op")) == QLatin1String("subscribe")
                    && quint32(control.value(QStringLiteral("endpointId")).toDouble())
                        == *armedEndpoint) {
                    *armedEndpoint = 0;
                    station.pureSignalFacade()->setRemoteAmpViewSubscribed(true);
                }
            });
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        qint64 nowMs = 0;
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            }, [&nowMs] { return nowMs; }, 10'000);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        // Lane B carry: inside the grace the pan says nothing yet.
        QCOMPARE(controller.panDisplayState(applet->panId()).phase,
                 PanDisplayState::Phase::None);
        QVERIFY(applet->remoteDisplayStatus().isEmpty());
        nowMs = RemoteMediaController::kPanWaitingGraceMs - 1;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QVERIFY(applet->remoteDisplayStatus().isEmpty());
        // A wait past the grace says so.
        nowMs = RemoteMediaController::kPanWaitingGraceMs;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(controller.panDisplayState(applet->panId()).phase,
                 PanDisplayState::Phase::Waiting);
        QCOMPARE(applet->remoteDisplayStatus(), QStringLiteral("Waiting for the Core"));

        nowMs = 10'000;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QCOMPARE(controller.panDisplayState(applet->panId()).phase,
                 PanDisplayState::Phase::Stalled);
        QCOMPARE(applet->remoteDisplayStatus(), QStringLiteral("Core not answering"));
        QVERIFY(client.mediaAvailable());

        stationLink->releaseHeld();
        // The late result is accepted: the stalled line clears.
        QTRY_VERIFY(showsDisplay(controller, applet)
                    && applet->remoteDisplayStatus().isEmpty());
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QCOMPARE(controller.activeEndpointCount(), 1);
    }

    // Lane B carry (R-R3-37): at session start in budget mode, a Core that
    // answers within kPanWaitingGraceMs never makes the pan say "Waiting
    // for the Core"; the pan goes straight to its display, with no line.
    void budgetModeAnsweredWithinGraceNeverSaysWaiting()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        qint64 nowMs = 0;
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            }, [&nowMs] { return nowMs; }, 10'000);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);

        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        const auto tickAt = [&](qint64 ms) {
            nowMs = ms;
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout",
                                              Qt::DirectConnection));
            QVERIFY2(controller.panDisplayState(applet->panId()).phase
                         != PanDisplayState::Phase::Waiting,
                     qPrintable(QString::number(ms)));
            QVERIFY2(applet->remoteDisplayStatus().isEmpty(),
                     qPrintable(applet->remoteDisplayStatus()));
        };
        tickAt(0);
        tickAt(RemoteMediaController::kPanWaitingGraceMs / 2);
        tickAt(RemoteMediaController::kPanWaitingGraceMs - 1);

        // The Core answers inside the grace: the display shows, no line, and
        // later ticks never bring "Waiting" back.
        stationLink->releaseHeld();
        QTRY_VERIFY(showsDisplay(controller, applet));
        QVERIFY(applet->remoteDisplayStatus().isEmpty());
        tickAt(RemoteMediaController::kPanWaitingGraceMs);
        tickAt(RemoteMediaController::kPanWaitingGraceMs * 3);
        QVERIFY(showsDisplay(controller, applet));
    }

    // Fix wave 2 (Critical 1, the several-devices design, ruling 9.3): the
    // Core splits its display budget by what each device asks for, so the
    // planner asks for what the operator wants (every pan at its wanted
    // quality), takes the budget's refusal as the answer, and plans inside
    // its share. It does not ask again while what it wants stays the same,
    // and asks again when that grows (a pan is added).
    void thePlannerAsksForWhatTheOperatorWantsOnlyWhenThatGrows()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        const double centre = station.streamCentreHz(station.sliceById(sliceId)->streamIndex());
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1200, 800);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        // Room for the active pan as wanted and the other at the useful
        // floor: less than both as wanted.
        const int pixels = qBound(1, stack.allApplets().first()->spectrumWidget()->width()
                                         - stack.allApplets().first()->spectrumWidget()
                                               ->reservedRightEdgeWidth(),
                                  SpectrumEndpoint::kMaxPixels);
        const auto active = spectrumDisplayCost(pixels, 30, false);
        const auto floorPan = spectrumDisplayCost(std::min(pixels, 256), 10, false);
        QVERIFY(active && floorPan);
        const auto budget = sumDisplayCharges({active->charge, floorPan->charge});
        QVERIFY(budget.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({budget->applicationBytesPerSecond,
                                               budget->spectrumSampleUnitsPerSecond, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();

        // First, both pans as wanted: 30 frames a second, full width.
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QCOMPARE(asked.value(QStringLiteral("fps")).toInt(), 30);
        }
        QTRY_COMPARE(stationLink->held.size(), 2);
        int budgetRefusals = 0;
        for (const QByteArray& wire : stationLink->held) {
            budgetRefusals += wire.contains("no room left") ? 1 : 0;
        }
        QCOMPARE(budgetRefusals, 1);
        // The refusal is the answer: the planner plans inside the share.
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            return showsDisplay(controller, stack.allApplets().first())
                && stationLink->held.isEmpty()
                && controller.panDisplayState(QStringLiteral("pan-1")).phase
                       != PanDisplayState::Phase::Refused;
        }());
        QTest::qWait(300);
        stationLink->releaseHeld();
        QTest::qWait(300);
        QVERIFY(stationLink->held.isEmpty());
        const int settled = countControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(settled > 2);

        // What the operator wants has not grown: no new ask, however often
        // the planner runs or the budget's generation moves.
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        for (int i = 0; i < 3; ++i) {
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        }
        QVERIFY(server.setDisplayBudgetLimits({budget->applicationBytesPerSecond,
                                               budget->spectrumSampleUnitsPerSecond, 2}));
        QTest::qWait(400);
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), settled);

        // A third pan: what the operator wants grew, so the planner asks
        // for it as wanted.
        PanadapterApplet* third = stack.addPanadapter(QStringLiteral("pan-2"));
        third->setActiveSliceIndex(sliceId);
        third->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
        third->spectrumWidget()->setWfUpdatePeriodMs(20);
        QTRY_VERIFY(countControl(outbound, QStringLiteral("subscribe")) > settled);
        const QList<QJsonObject> after = controlsFor(outbound, QStringLiteral("subscribe"));
        QList<quint32> earlier;
        for (int i = 0; i < settled; ++i) {
            earlier.append(quint32(after.at(i).value(QStringLiteral("endpointId")).toInteger()));
        }
        bool askedForTheNewPanAsWanted = false;
        for (int i = settled; i < after.size(); ++i) {
            if (!earlier.contains(quint32(after.at(i).value(QStringLiteral("endpointId"))
                                              .toInteger()))) {
                askedForTheNewPanAsWanted = after.at(i).value(QStringLiteral("fps")).toInt() == 30;
                break;
            }
        }
        QVERIFY(askedForTheNewPanAsWanted);
        stationLink->releaseHeld();
    }

    void thePlannerAsksAgainWhenTheTransmitHolderChanges()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        const double centre = station.streamCentreHz(station.sliceById(sliceId)->streamIndex());
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1200, 800);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        // Room for the active pan as wanted and the other at the useful
        // floor: less than both as wanted.
        const int pixels = qBound(1, stack.allApplets().first()->spectrumWidget()->width()
                                         - stack.allApplets().first()->spectrumWidget()
                                               ->reservedRightEdgeWidth(),
                                  SpectrumEndpoint::kMaxPixels);
        const auto active = spectrumDisplayCost(pixels, 30, false);
        const auto floorPan = spectrumDisplayCost(std::min(pixels, 256), 10, false);
        QVERIFY(active && floorPan);
        const auto budget = sumDisplayCharges({active->charge, floorPan->charge});
        QVERIFY(budget.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({budget->applicationBytesPerSecond,
                                               budget->spectrumSampleUnitsPerSecond, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();

        // First, both pans as wanted: 30 frames a second, full width.
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QCOMPARE(asked.value(QStringLiteral("fps")).toInt(), 30);
        }
        QTRY_COMPARE(stationLink->held.size(), 2);
        int budgetRefusals = 0;
        for (const QByteArray& wire : stationLink->held) {
            budgetRefusals += wire.contains("no room left") ? 1 : 0;
        }
        QCOMPARE(budgetRefusals, 1);
        // The refusal is the answer: the planner plans inside the share.
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            return showsDisplay(controller, stack.allApplets().first())
                && stationLink->held.isEmpty()
                && controller.panDisplayState(QStringLiteral("pan-1")).phase
                       != PanDisplayState::Phase::Refused;
        }());
        QTest::qWait(300);
        stationLink->releaseHeld();
        QTest::qWait(300);
        QVERIFY(stationLink->held.isEmpty());
        const int settled = countControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(settled > 2);

        // Fix wave 3 (ruling 9.3): a change of transmit holder asks again for
        // what the operator wants: here this device becomes the holder.
        const auto askedAgainAsWanted = [&](int from) {
            const QList<QJsonObject> all = controlsFor(outbound, QStringLiteral("subscribe"));
            for (int i = from; i < all.size(); ++i) {
                if (all.at(i).value(QStringLiteral("fps")).toInt() == 30) { return true; }
            }
            return false;
        };
        controller.setTransmitHolder(1, false);
        QTRY_VERIFY(countControl(outbound, QStringLiteral("subscribe")) > settled);
        QVERIFY(askedAgainAsWanted(settled));
        // Answered (refused for the budget again): planned inside the share.
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            return stationLink->held.isEmpty()
                && controller.panDisplayState(QStringLiteral("pan-1")).phase
                       != PanDisplayState::Phase::Refused;
        }());
        QTest::qWait(300);
        stationLink->releaseHeld();
        QTest::qWait(300);
        QVERIFY(stationLink->held.isEmpty());
        const int afterHolder = countControl(outbound, QStringLiteral("subscribe"));

        // The same holder again asks nothing.
        controller.setTransmitHolder(1, false);
        QTest::qWait(400);
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), afterHolder);

        // The holder goes away: asked again.
        controller.setTransmitHolder(1, true);
        QTRY_VERIFY(countControl(outbound, QStringLiteral("subscribe")) > afterHolder);
        QVERIFY(askedAgainAsWanted(afterHolder));
        stationLink->releaseHeld();
    }

    void aResizeAsksOnceItSettlesNotAtEveryStep()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        const double centre = station.streamCentreHz(station.sliceById(sliceId)->streamIndex());
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1200, 800);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        // Room for the active pan as wanted and the other at the useful
        // floor: less than both as wanted.
        const int pixels = qBound(1, stack.allApplets().first()->spectrumWidget()->width()
                                         - stack.allApplets().first()->spectrumWidget()
                                               ->reservedRightEdgeWidth(),
                                  SpectrumEndpoint::kMaxPixels);
        const auto active = spectrumDisplayCost(pixels, 30, false);
        const auto floorPan = spectrumDisplayCost(std::min(pixels, 256), 10, false);
        QVERIFY(active && floorPan);
        const auto budget = sumDisplayCharges({active->charge, floorPan->charge});
        QVERIFY(budget.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({budget->applicationBytesPerSecond,
                                               budget->spectrumSampleUnitsPerSecond, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();

        // First, both pans as wanted: 30 frames a second, full width.
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QCOMPARE(asked.value(QStringLiteral("fps")).toInt(), 30);
        }
        QTRY_COMPARE(stationLink->held.size(), 2);
        int budgetRefusals = 0;
        for (const QByteArray& wire : stationLink->held) {
            budgetRefusals += wire.contains("no room left") ? 1 : 0;
        }
        QCOMPARE(budgetRefusals, 1);
        // The refusal is the answer: the planner plans inside the share.
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            return showsDisplay(controller, stack.allApplets().first())
                && stationLink->held.isEmpty()
                && controller.panDisplayState(QStringLiteral("pan-1")).phase
                       != PanDisplayState::Phase::Refused;
        }());
        QTest::qWait(300);
        stationLink->releaseHeld();
        QTest::qWait(300);
        QVERIFY(stationLink->held.isEmpty());
        const int settled = countControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(settled > 2);

        // Fix wave 3 (Minor 3): the pan the planner cut back (below 30 frames
        // a second inside the share) is the one an ask sends at 30 again.
        QSet<qint64> reduced;
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            const qint64 endpoint = asked.value(QStringLiteral("endpointId")).toInteger();
            if (asked.value(QStringLiteral("fps")).toInt() == 30) { reduced.remove(endpoint); }
            else { reduced.insert(endpoint); }
        }
        QVERIFY(!reduced.isEmpty());
        // Parity Task 17 (B3.10): each request's averaging constants are for
        // the rate it asks, including the rates the budget cut pans back to.
        const int traceTimeMs = stack.allApplets().first()->spectrumWidget()
                                    ->spectrumAverageTimeMs();
        for (const QJsonObject& each : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QCOMPARE(each.value(QStringLiteral("trace")).toObject()
                         .value(QStringLiteral("averageAlpha")).toDouble(),
                     double(averageAlphaForTimeMs(traceTimeMs,
                                                  each.value(QStringLiteral("fps")).toInt())));
        }
        const auto askedAsWanted = [&](int from) {
            const QList<QJsonObject> all = controlsFor(outbound, QStringLiteral("subscribe"));
            for (int i = from; i < all.size(); ++i) {
                if (reduced.contains(all.at(i).value(QStringLiteral("endpointId")).toInteger())
                    && all.at(i).value(QStringLiteral("fps")).toInt() == 30) {
                    return true;
                }
            }
            return false;
        };
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == RemoteMediaController::kPlannerIntervalMs) {
                subscriptionTimer = timer;
                break;
            }
        }
        QVERIFY(subscriptionTimer);

        // A drag: the window grows in steps far quicker than the settle
        // time, and the planner runs at each. No ask while it moves.
        int width = 1200;
        for (int step = 0; step < 6; ++step) {
            width += 40;
            stack.resize(width, 800);
            QCoreApplication::processEvents();
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
            stationLink->releaseHeld();
            QTest::qWait(RemoteMediaController::kResizeSettleMs / 4);
            QVERIFY2(!askedAsWanted(settled), qPrintable(QString::number(step)));
        }
        // It stops: one ask once the widths have settled.
        QTest::qWait(RemoteMediaController::kResizeSettleMs + 50);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            return askedAsWanted(settled);
        }());
        stationLink->releaseHeld();
    }

    // Fix wave 2 (Important 2): a pan the Core refused, which the planner
    // then pauses (no room even for one more useful pan), is closed on the
    // Core with an unsubscribe, so its request stops counting against the
    // other devices there.
    void aDisplayTheCoreRefusedIsUnsubscribedWhenThePlannerDropsIt()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"), {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        const double centre = station.streamCentreHz(station.sliceById(sliceId)->streamIndex());
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1200, 800);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        // Room for one useful pan and nothing more: the planner keeps the
        // active pan at the floor and pauses the other.
        const int pixels = qBound(1, stack.allApplets().first()->spectrumWidget()->width()
                                         - stack.allApplets().first()->spectrumWidget()
                                               ->reservedRightEdgeWidth(),
                                  SpectrumEndpoint::kMaxPixels);
        const auto active = spectrumDisplayCost(std::min(pixels, 256), 10, false);
        QVERIFY(active.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({active->charge.applicationBytesPerSecond,
                                               active->charge.spectrumSampleUnitsPerSecond, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();

        QTRY_VERIFY(showsDisplay(controller, stack.allApplets().first()));
        QTRY_COMPARE(controller.panDisplayState(QStringLiteral("pan-1")).phase,
                     PanDisplayState::Phase::Paused);
        // Both pans were asked for as wanted and refused; paused, pan-1's
        // (the second asked for) is closed on the Core.
        const QList<QJsonObject> asked = controlsFor(outbound, QStringLiteral("subscribe"));
        QVERIFY(asked.size() >= 2);
        QCOMPARE(asked.at(0).value(QStringLiteral("fps")).toInt(), 30);
        QCOMPARE(asked.at(1).value(QStringLiteral("fps")).toInt(), 30);
        const quint32 refused = quint32(asked.at(1).value(QStringLiteral("endpointId")).toInteger());
        QTRY_COMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
        QCOMPARE(quint32(lastControl(outbound, QStringLiteral("unsubscribe"))
                             .value(QStringLiteral("endpointId")).toInteger()),
                 refused);
        QCOMPARE(daemon.activeEndpointCount(), 1);
    }

    void budgetFocusSwapReducesBeforeGrowthAndRestoresOnRecovery()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        const QStringList panIds{QStringLiteral("pan-0"), QStringLiteral("pan-1"),
                                 QStringLiteral("pan-2"), QStringLiteral("pan-3")};
        stack.applyLayout(QStringLiteral("2x2"), panIds);
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(
                station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1600, 900);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        QList<DisplayBudgetCharge> constrainedCharges;
        for (PanadapterApplet* applet : stack.allApplets()) {
            const int requestedPixels = qBound(
                1, applet->spectrumWidget()->width()
                    - applet->spectrumWidget()->reservedRightEdgeWidth(),
                SpectrumEndpoint::kMaxPixels);
            const int fps = applet->panId() == QStringLiteral("pan-0") ? 30 : 10;
            const auto cost = spectrumDisplayCost(requestedPixels, fps, false);
            QVERIFY(cost.has_value());
            constrainedCharges.append(cost->charge);
        }
        const auto constrained = sumDisplayCharges(constrainedCharges);
        QVERIFY(constrained.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({constrained->applicationBytesPerSecond,
                                               constrained->spectrumSampleUnitsPerSecond, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 4);
        QTRY_COMPARE(stationLink->held.size(), 4);
        // Fix wave 2 (Critical 1, ruling 9.3): the planner first asks for
        // what the operator wants, every pan at its wanted rate.
        for (const QJsonObject& asked : controlsFor(outbound, QStringLiteral("subscribe"))) {
            QCOMPARE(asked.value(QStringLiteral("fps")).toInt(), 30);
        }
        // The budget refuses what does not fit, and the planner plans
        // inside the share. Every pan accepted: a reduced one says so, one
        // at its requested quality shows no line.
        QTRY_VERIFY([&] {
            stationLink->releaseHeld();
            for (PanadapterApplet* applet : stack.allApplets()) {
                if (!showsDisplay(controller, applet)) {
                    return false;
                }
            }
            return stationLink->held.isEmpty();
        }());
        QVERIFY(countControl(outbound, QStringLiteral("subscribe")) > 4);
        const int settled = countControl(outbound, QStringLiteral("subscribe"));
        // Lane B carry (R-R3-08, R-R3-37): a cut for the Core's display
        // limit, with no busy reason from the Core, keeps the limit wording.
        int reducedPans = 0;
        for (PanadapterApplet* applet : stack.allApplets()) {
            const PanDisplayState state = controller.panDisplayState(applet->panId());
            QCOMPARE(state.budgetReason, DisplayBudgetReason::None);
            QCOMPARE(controller.panDisplayBudgetReason(applet->panId()),
                     DisplayBudgetReason::None);
            if (!state.reduced()) {
                QVERIFY(applet->remoteDisplayStatus().isEmpty());
                continue;
            }
            ++reducedPans;
            QVERIFY2(applet->remoteDisplayStatus().endsWith(QStringLiteral(": Core limit")),
                     qPrintable(applet->remoteDisplayStatus()));
            QVERIFY(!applet->remoteDisplayExplanation().contains(QStringLiteral("busy")));
        }
        QVERIFY(reducedPans > 0);

        // The active pan's endpoint: the one whose latest request is at 30.
        QHash<quint32, int> latestFps;
        for (const QJsonObject& control : controlsFor(outbound, QStringLiteral("subscribe"))) {
            latestFps.insert(quint32(control.value(QStringLiteral("endpointId")).toInteger()),
                             control.value(QStringLiteral("fps")).toInt());
        }
        QCOMPARE(std::count(latestFps.cbegin(), latestFps.cend(), 30), 1);
        const quint32 oldActiveEndpoint = latestFps.key(30);

        stack.setActivePan(QStringLiteral("pan-1"));
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), settled + 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        const QJsonObject reduction = lastControl(outbound, QStringLiteral("subscribe"));
        QCOMPARE(quint32(reduction.value(QStringLiteral("endpointId")).toInteger()),
                 oldActiveEndpoint);
        QCOMPARE(reduction.value(QStringLiteral("fps")).toInt(), 10);
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), settled + 1);

        stationLink->releaseHeld();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), settled + 2);
        QTRY_COMPARE(stationLink->held.size(), 1);
        const QJsonObject growth = lastControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(quint32(growth.value(QStringLiteral("endpointId")).toInteger())
                != oldActiveEndpoint);
        QCOMPARE(growth.value(QStringLiteral("fps")).toInt(), 30);
        stationLink->releaseHeld();

        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 2}));
        QTRY_VERIFY(countControl(outbound, QStringLiteral("subscribe")) > settled + 2);
        QTRY_VERIFY(!stationLink->held.isEmpty());
        stationLink->releaseHeld();
        QTRY_VERIFY([&] {
            for (PanadapterApplet* applet : stack.allApplets()) {
                const PanDisplayState status = controller.panDisplayState(applet->panId());
                if (status.phase != PanDisplayState::Phase::Showing || status.reduced()
                    || !applet->remoteDisplayStatus().isEmpty()) {
                    return false;
                }
            }
            return true;
        }());
    }

    // R-R3-08/37: a Core-busy cut slows the background pans first and leaves
    // the active pan alone; each reduced pan carries the reason in its state
    // and says so in its status line. Restore clears both.
    void coreBusyCutReducesBackgroundPansFirstAndRestoreClearsIt()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        const QStringList panIds{QStringLiteral("pan-0"), QStringLiteral("pan-1"),
                                 QStringLiteral("pan-2"), QStringLiteral("pan-3")};
        stack.applyLayout(QStringLiteral("2x2"), panIds);
        for (PanadapterApplet* applet : stack.allApplets()) {
            applet->setActiveSliceIndex(sliceId);
            applet->spectrumWidget()->setDisplayWindowPreservingHistory(
                station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
            applet->spectrumWidget()->setWfUpdatePeriodMs(20);
        }
        stack.resize(1600, 900);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        stack.setActivePan(QStringLiteral("pan-0"));
        // Exactly the active pan at 30 fps and three background pans at the
        // 10 fps floor.
        QList<DisplayBudgetCharge> cutCharges;
        for (PanadapterApplet* applet : stack.allApplets()) {
            const int requestedPixels = qBound(
                1, applet->spectrumWidget()->width()
                    - applet->spectrumWidget()->reservedRightEdgeWidth(),
                SpectrumEndpoint::kMaxPixels);
            const int fps = applet->panId() == QStringLiteral("pan-0") ? 30 : 10;
            const auto cost = spectrumDisplayCost(requestedPixels, fps, false);
            QVERIFY(cost.has_value());
            cutCharges.append(cost->charge);
        }
        const auto cut = sumDisplayCharges(cutCharges);
        QVERIFY(cut.has_value());

        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 4);
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")), 4);

        const auto statusOf = [&stack](const QString& panId) {
            for (PanadapterApplet* applet : stack.allApplets()) {
                if (applet->panId() == panId) { return applet->remoteDisplayStatus(); }
            }
            return QString();
        };
        // At the requested quality with nothing waiting a pan shows no line
        // (legacy mode's look), and carries no reason.
        const auto allAtRequestedQuality = [&] {
            for (const QString& panId : panIds) {
                if (!statusOf(panId).isEmpty()
                    || controller.panDisplayBudgetReason(panId) != DisplayBudgetReason::None) {
                    return false;
                }
            }
            return true;
        };
        QTRY_VERIFY(allAtRequestedQuality());

        QVERIFY(server.setDisplayBudgetLimits(
            {cut->applicationBytesPerSecond, cut->spectrumSampleUnitsPerSecond, 2},
            DisplayBudgetReason::CoreBusy));
        QTRY_COMPARE(client.remoteDisplayBudgetReason(), DisplayBudgetReason::CoreBusy);
        QTRY_VERIFY([&] {
            for (const QString& panId : panIds) {
                const QString status = statusOf(panId);
                const bool active = panId == QStringLiteral("pan-0");
                if (active) {
                    if (!status.isEmpty()
                        || controller.panDisplayBudgetReason(panId)
                            != DisplayBudgetReason::None) {
                        return false;
                    }
                } else {
                    const PanDisplayState state = controller.panDisplayState(panId);
                    if (state.phase != PanDisplayState::Phase::Showing || state.fps != 10
                        || !state.reduced()
                        || state.budgetReason != DisplayBudgetReason::CoreBusy
                        || !status.contains(QStringLiteral("Core busy"))
                        || controller.panDisplayBudgetReason(panId)
                            != DisplayBudgetReason::CoreBusy) {
                        return false;
                    }
                }
            }
            return true;
        }());
        // Three reductions, all background pans to 10 fps; the active pan
        // was never asked to change.
        const QList<QJsonObject> subscribes = controlsFor(outbound, QStringLiteral("subscribe"));
        QCOMPARE(subscribes.size(), 7);
        for (int i = 4; i < subscribes.size(); ++i) {
            QCOMPARE(subscribes.at(i).value(QStringLiteral("fps")).toInt(), 10);
        }

        // Final review M4: deeper cuts. The active pan changes only once
        // every background pan is at its floor (256 px at 10 fps) or paused.
        int activePixels = 0;
        QList<DisplayBudgetCharge> floorCharges;
        for (PanadapterApplet* applet : stack.allApplets()) {
            const int requestedPixels = qBound(
                1, applet->spectrumWidget()->width()
                    - applet->spectrumWidget()->reservedRightEdgeWidth(),
                SpectrumEndpoint::kMaxPixels);
            if (applet->panId() == QStringLiteral("pan-0")) {
                activePixels = requestedPixels;
            } else {
                QVERIFY(requestedPixels > 256);
                floorCharges.append(spectrumDisplayCost(256, 10, false)->charge);
            }
        }
        const auto backgroundsAtFloorOrPaused = [&] {
            for (const QString& panId : panIds) {
                if (panId == QStringLiteral("pan-0")) {
                    continue;
                }
                const QString status = statusOf(panId);
                const PanDisplayState state = controller.panDisplayState(panId);
                const bool atFloor = state.phase == PanDisplayState::Phase::Showing
                    && state.pixels == 256 && state.fps == 10
                    && state.budgetReason == DisplayBudgetReason::CoreBusy
                    && status.contains(QStringLiteral("Core busy"));
                const bool paused = state.phase == PanDisplayState::Phase::Paused
                    && state.budgetReason == DisplayBudgetReason::CoreBusy
                    && status == QStringLiteral("Paused: Core busy");
                if (!(atFloor || paused)
                    || controller.panDisplayBudgetReason(panId) != DisplayBudgetReason::CoreBusy) {
                    return false;
                }
            }
            return true;
        };
        // Room for the active pan as asked plus three floors: the active pan
        // is untouched while the background pans go to their floor.
        QList<DisplayBudgetCharge> roomForActive = floorCharges;
        roomForActive.append(spectrumDisplayCost(activePixels, 30, false)->charge);
        const auto deeper = sumDisplayCharges(roomForActive);
        QVERIFY(deeper.has_value());
        QVERIFY(server.setDisplayBudgetLimits(
            {deeper->applicationBytesPerSecond, deeper->spectrumSampleUnitsPerSecond, 3},
            DisplayBudgetReason::CoreBusy));
        QTRY_VERIFY(backgroundsAtFloorOrPaused());
        QTRY_VERIFY(statusOf(QStringLiteral("pan-0")).isEmpty());
        QCOMPARE(controller.panDisplayBudgetReason(QStringLiteral("pan-0")),
                 DisplayBudgetReason::None);
        // Less than that: now the active pan gives way too, and the
        // background pans stay at their floor or paused.
        QList<DisplayBudgetCharge> activeCut = floorCharges;
        activeCut.append(spectrumDisplayCost(activePixels, 10, false)->charge);
        const auto deepest = sumDisplayCharges(activeCut);
        QVERIFY(deepest.has_value());
        QVERIFY(server.setDisplayBudgetLimits(
            {deepest->applicationBytesPerSecond, deepest->spectrumSampleUnitsPerSecond, 4},
            DisplayBudgetReason::CoreBusy));
        QTRY_VERIFY(controller.panDisplayState(QStringLiteral("pan-0")).reduced()
                    && statusOf(QStringLiteral("pan-0")).contains(QStringLiteral("Core busy")));
        QCOMPARE(controller.panDisplayBudgetReason(QStringLiteral("pan-0")),
                 DisplayBudgetReason::CoreBusy);
        QVERIFY(backgroundsAtFloorOrPaused());
        // Replay what the app sent, in order: whenever the active pan's
        // latest request is below what it asked for, every background pan's
        // latest request is at its floor or released.
        const quint32 activeEndpoint = [&] {
            for (const QJsonObject& control : subscribes) {
                if (control.value(QStringLiteral("fps")).toInt() == 30) {
                    return quint32(control.value(QStringLiteral("endpointId")).toInteger());
                }
            }
            return quint32{0};
        }();
        QVERIFY(activeEndpoint != 0);
        QHash<quint32, std::pair<int, int>> latest; // endpoint -> pixels, fps
        bool activeReduced = false;
        for (const auto& call : outbound) {
            const QJsonObject control = call.at(0).toJsonObject();
            const QString op = control.value(QStringLiteral("op")).toString();
            const quint32 endpoint = quint32(control.value(QStringLiteral("endpointId")).toInteger());
            if (op == QStringLiteral("unsubscribe")) {
                latest.remove(endpoint);
            } else if (op == QStringLiteral("subscribe")) {
                latest.insert(endpoint, {control.value(QStringLiteral("pixels")).toInt(),
                                         control.value(QStringLiteral("fps")).toInt()});
            } else {
                continue;
            }
            const auto active = latest.constFind(activeEndpoint);
            if (active == latest.cend()
                || (active->first >= activePixels && active->second >= 30)) {
                continue;
            }
            activeReduced = true;
            for (auto it = latest.cbegin(); it != latest.cend(); ++it) {
                if (it.key() != activeEndpoint) {
                    QVERIFY2(it->first <= 256 && it->second <= 10,
                             qPrintable(QStringLiteral("endpoint %1 at %2 px @ %3 fps while the "
                                                       "active pan was reduced")
                                            .arg(it.key()).arg(it->first).arg(it->second)));
                }
            }
        }
        QVERIFY(activeReduced);

        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 5},
                                              DisplayBudgetReason::None));
        QTRY_COMPARE(client.remoteDisplayBudgetReason(), DisplayBudgetReason::None);
        QTRY_VERIFY(allAtRequestedQuality());
    }

    void budgetRetirementWaitsForPendingSubscribeThenReleasesReservation()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        // Refused row: Core's state moves ahead of the GUI's view. When the
        // survivor asks again, Core already holds PureSignal display, which
        // the GUI has not heard of yet. Connected before Core's own handler,
        // so it runs first.
        const auto armedEndpoint = std::make_shared<quint32>(0);
        connect(&server, &StationServer::mediaControlReceived, &station,
            [&station, armedEndpoint](const QJsonObject& control) {
                if (*armedEndpoint != 0
                    && control.value(QStringLiteral("op")) == QLatin1String("subscribe")
                    && quint32(control.value(QStringLiteral("endpointId")).toDouble())
                        == *armedEndpoint) {
                    *armedEndpoint = 0;
                    station.pureSignalFacade()->setRemoteAmpViewSubscribed(true);
                }
            });
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QCOMPARE(controller.activeEndpointCount(), 1);

        QPointer<PanadapterApplet> retiredApplet(applet);
        stack.removePanadapter(QStringLiteral("pan-0"));
        QTRY_VERIFY(retiredApplet.isNull());
        QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 0);
        QCOMPARE(controller.activeEndpointCount(), 1);

        stationLink->releaseHeld();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QCOMPARE(controller.activeEndpointCount(), 1);

        stationLink->releaseHeld();
        QTRY_COMPARE(controller.activeEndpointCount(), 0);
        QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
    }

    void retiredPendingDisplayAfterPeerReplacementReleasesBeforeSurvivorGrowth_data()
    {
        QTest::addColumn<bool>("removePan");
        QTest::addColumn<bool>("reaskLost");
        QTest::newRow("slice-rebind") << false << false;
        QTest::newRow("pan-removal") << true << false;
        // The re-ask's answer is lost too: the acknowledgement timeout
        // still releases the closed display.
        QTest::newRow("re-ask-lost") << false << true;
    }

    void retiredPendingDisplayAfterPeerReplacementReleasesBeforeSurvivorGrowth()
    {
        QFETCH(bool, removePan);
        QFETCH(bool, reaskLost);
        // A genuine accepted result is held on the old media connection,
        // then delivered after both production controllers promote a peer.
        // The window asks again on the new connection (2026-10-16), so
        // closing that endpoint releases its reservation at once; it cannot
        // leave the surviving pan behind a permanent pending gate.
        AppSettings& appSettings = AppSettings::instance();
        const QString fpsKey = QStringLiteral("DisplaySpectrumFps");
        const bool hadFps = appSettings.contains(fpsKey);
        const QVariant savedFps = appSettings.value(fpsKey);
        appSettings.setValue(fpsKey, QStringLiteral("10"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(fpsKey, savedFps); }
            else { appSettings.remove(fpsKey); }
        });
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int firstId = station.addSlice();
        const int survivorId = station.addSlice();
        QVERIFY(station.sliceById(firstId));
        QVERIFY(station.sliceById(survivorId));
        const int stream = station.sliceById(survivorId)->streamIndex();
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QList<QPointer<DisplayTransport>> sourcePeers;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourcePeers](QObject* owner) -> IMediaTransport* {
                auto* transport = new DisplayTransport(owner);
                sourcePeers.append(transport);
                return transport;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(firstId);
        SpectrumWidget* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        widget->setWfUpdatePeriodMs(20);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 nowMs = 0;
        QList<QPointer<DisplayTransport>> sinkPeers;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkPeers](QObject* owner) -> IMediaTransport* {
                auto* transport = new DisplayTransport(owner);
                sinkPeers.append(transport);
                return transport;
            }, [&nowMs] { return nowMs; }, 10'000);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_COMPARE(sourcePeers.size(), 1);
        QTRY_COMPARE(sinkPeers.size(), 1);
        sourcePeers.first()->other = sinkPeers.first();
        sourcePeers.first()->activate();
        sinkPeers.first()->activate();
        QTRY_COMPARE(stationLink->held.size(), 1);
        stationLink->releaseHeld();
        QTRY_VERIFY(showsDisplay(controller, applet));
        const DisplayBudgetCharge originalCharge = daemon.acceptedDisplayCharge();
        const QJsonObject original = lastControl(outbound, QStringLiteral("subscribe"));
        const quint32 retiredEndpoint = quint32(original.value(QStringLiteral("endpointId")).toDouble());
        QVERIFY(retiredEndpoint != 0);

        // The Core really accepted a larger reservation than the GUI knows.
        appSettings.setValue(fpsKey, QStringLiteral("30"));
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QVERIFY(daemon.acceptedDisplayCharge().applicationBytesPerSecond
                > originalCharge.applicationBytesPerSecond);
        const QJsonObject pending = lastControl(outbound, QStringLiteral("subscribe"));
        const quint32 pendingRevision = quint32(pending.value(QStringLiteral("revision")).toDouble());
        const QString oldConnection = controller.mediaConnectionId();
        QVERIFY(controller.replaceConnection());
        QTRY_COMPARE(sourcePeers.size(), 2);
        QTRY_COMPARE(sinkPeers.size(), 2);
        sourcePeers.last()->other = sinkPeers.last();
        sourcePeers.last()->activate();
        sinkPeers.last()->activate();
        QTRY_VERIFY_WITH_TIMEOUT(controller.mediaConnectionId() != oldConnection
                                && !controller.replacingConnection(),
                                DaemonMediaController::kReplaceOverlapMs + 3000);
        // 2026-10-16: the window asks again at once on the promoted
        // connection, with a newer revision: the Core's answer to the first
        // ask comes on the old connection and is fenced out.
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 3);
        const QJsonObject again = lastControl(outbound, QStringLiteral("subscribe"));
        QCOMPARE(again.value(QStringLiteral("connectionId")).toString(),
                 controller.mediaConnectionId());
        QCOMPARE(quint32(again.value(QStringLiteral("endpointId")).toDouble()), retiredEndpoint);
        QCOMPARE(quint32(again.value(QStringLiteral("revision")).toDouble()), pendingRevision + 1);
        QTRY_COMPARE(stationLink->held.size(), 2);
        // No identities or result fields are manufactured: the first is the
        // original Core reply, now fenced out by the promoted connection.
        if (reaskLost) { stationLink->held.removeLast(); }
        stationLink->releaseHeld();
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")),
                     reaskLost ? 2 : 3);
        const QJsonObject oldResult =
            controlsFor(inbound, QStringLiteral("allocation-result")).at(1);
        QCOMPARE(oldResult.value(QStringLiteral("connectionId")).toString(), oldConnection);
        QCOMPARE(quint32(oldResult.value(QStringLiteral("revision")).toDouble()), pendingRevision);
        if (!reaskLost) {
            const QJsonObject againResult =
                lastControl(inbound, QStringLiteral("allocation-result"));
            QCOMPARE(againResult.value(QStringLiteral("connectionId")).toString(),
                     controller.mediaConnectionId());
            QCOMPARE(quint32(againResult.value(QStringLiteral("revision")).toDouble()),
                     pendingRevision + 1);
        }

        QPointer<SpectrumWidget> retiredWidget(widget);
        if (removePan) {
            stack.removePanadapter(QStringLiteral("pan-0"));
            QTRY_VERIFY(retiredWidget.isNull());
            applet = stack.addPanadapter(QStringLiteral("pan-0"));
            widget = applet->spectrumWidget();
            widget->setDisplayWindowPreservingHistory(centre, 48000);
            widget->setWfUpdatePeriodMs(20);
        }
        applet->setActiveSliceIndex(survivorId);
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        if (reaskLost) {
            // Still waiting on the lost re-ask: nothing is released or grown
            // until its acknowledgement times out.
            QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 0);
            QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 3);
            QVERIFY(widget->renderedPixels().isEmpty());
            nowMs = 10'000;
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout",
                                              Qt::DirectConnection));
        }
        // The answered endpoint is released at once; no pending gate is left.
        QTRY_COMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
        const QJsonObject release = lastControl(outbound, QStringLiteral("unsubscribe"));
        QCOMPARE(release.value(QStringLiteral("connectionId")).toString(), controller.mediaConnectionId());
        QCOMPARE(quint32(release.value(QStringLiteral("endpointId")).toDouble()), retiredEndpoint);
        QCOMPARE(quint32(release.value(QStringLiteral("revision")).toDouble()), pendingRevision + 2);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        // Until the authoritative release result arrives, the larger
        // reservation stands. No survivor growth may pass that gate.
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 3);
        QVERIFY(widget->renderedPixels().isEmpty());
        stationLink->releaseHeld();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 4);
        const QJsonObject survivor = lastControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(survivor.value(QStringLiteral("endpointId")) != original.value(QStringLiteral("endpointId")));
        QCOMPARE(survivor.value(QStringLiteral("sliceId")).toInt(), survivorId);
        QTRY_COMPARE(stationLink->held.size(), 1);
        stationLink->releaseHeld();
        QTRY_COMPARE(controller.activeEndpointCount(), 1);
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QTRY_VERIFY(showsDisplay(controller, applet));
        // A genuine late result for the tombstoned endpoint cannot undo
        // the new binding, even after its own reservation was confirmed.
        stationLink->passNextAllocationResult();
        QVERIFY(server.sendMediaControl(oldResult, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(inbound, QStringLiteral("allocation-result")),
                     reaskLost ? 5 : 6);
        QCOMPARE(controller.activeEndpointCount(), 1);
        QCOMPARE(daemon.activeEndpointCount(), 1);

        QVector<float> iq(2048, 0.001f);
        QTRY_VERIFY_WITH_TIMEOUT((QMetaObject::invokeMethod(&station, "rawIqDataForStream",
            Qt::DirectConnection, Q_ARG(int, stream), Q_ARG(QVector<float>, iq)),
            !widget->renderedPixels().isEmpty() && widget->waterfallHistoryRowsForTest() > 0), 5000);
        QVERIFY(!station.mox() && !remote.mox());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void overdueRetirementSendSurvivesSynchronousStackDestruction()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto stack = std::make_unique<PanadapterStack>();
        PanadapterApplet* applet = stack->addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        qint64 nowMs = 0;
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, stack.get(), nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            }, [&nowMs] { return nowMs; }, 10'000);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new StackTeardownControlTransport;
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(stationLink->held.size(), 1);
        QPointer<PanadapterApplet> retiredApplet(applet);
        stack->removePanadapter(QStringLiteral("pan-0"));
        QTRY_VERIFY(retiredApplet.isNull());
        QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 0);
        const QString connectionId = controller.mediaConnectionId();
        QPointer<RemoteMediaController> controllerLifetime(&controller);
        QPointer<PanadapterStack> stackLifetime(stack.get());
        bool identitySurvived = false;
        clientLink->beforeUnsubscribe = [&] {
            stack.reset();
            identitySurvived = stackLifetime.isNull() && controllerLifetime
                && client.mediaAvailable() && controller.mediaConnectionId() == connectionId;
            qInfo() << "Stack destroyed during overdue release; controller and media identity survived:"
                    << identitySurvived;
        };
        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        nowMs = 10'000;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QVERIFY(identitySurvived);
        QVERIFY(stackLifetime.isNull());
        QVERIFY(controllerLifetime);
        QTRY_COMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 2);
        stationLink->releaseHeld();
        QTRY_COMPARE(controller.activeEndpointCount(), 0);
        QTRY_COMPARE(daemon.activeEndpointCount(), 0);
        nowMs = 20'000;
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("unsubscribe")), 1);
        QVERIFY(!station.mox() && !remote.mox());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void matchingSourceRetirementClearsAcceptedReservationDuringPendingUpdate()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        // Refused row: Core's state moves ahead of the GUI's view. When the
        // survivor asks again, Core already holds PureSignal display, which
        // the GUI has not heard of yet. Connected before Core's own handler,
        // so it runs first.
        const auto armedEndpoint = std::make_shared<quint32>(0);
        connect(&server, &StationServer::mediaControlReceived, &station,
            [&station, armedEndpoint](const QJsonObject& control) {
                if (*armedEndpoint != 0
                    && control.value(QStringLiteral("op")) == QLatin1String("subscribe")
                    && quint32(control.value(QStringLiteral("endpointId")).toDouble())
                        == *armedEndpoint) {
                    *armedEndpoint = 0;
                    station.pureSignalFacade()->setRemoteAmpViewSubscribed(true);
                }
            });
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        SpectrumWidget* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        stationLink->releaseHeld();
        QTRY_VERIFY(showsDisplay(controller, applet)
                    && applet->remoteDisplayStatus().isEmpty());

        widget->setCenterFrequency(widget->centerFrequency() + 500);
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QTRY_COMPARE(stationLink->held.size(), 1);
        const QJsonObject pending = lastControl(outbound, QStringLiteral("subscribe"));
        const int subscriptions = countControl(outbound, QStringLiteral("subscribe"));
        const QJsonObject retired{
            {QStringLiteral("op"), QStringLiteral("allocation-result")},
            {QStringLiteral("connectionId"), pending.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), pending.value(QStringLiteral("endpointId"))},
            {QStringLiteral("revision"), pending.value(QStringLiteral("revision"))},
            {QStringLiteral("accepted"), false},
            {QStringLiteral("reason"), QStringLiteral("source retired")},
            {QStringLiteral("budgetGeneration"), 1},
            {QStringLiteral("acceptedRevision"), 0},
            {QStringLiteral("applicationBytesPerSecond"), 0},
            {QStringLiteral("spectrumSampleUnitsPerSecond"), 0},
            {QStringLiteral("messagesPerSecond"), 0}};
        stationLink->passNextAllocationResult();
        QVERIFY(server.sendMediaControl(retired, server.mediaSessionEpoch()));
        QTRY_VERIFY(refusedFor(controller, applet, QStringLiteral("source retired")));
        QCOMPARE(controller.activeEndpointCount(), 1);

        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), subscriptions);

        stationLink->releaseHeld();
        QCoreApplication::processEvents();
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), subscriptions);
        QVERIFY(refusedFor(controller, applet, QStringLiteral("source retired")));
    }

    void ps3EnableWaitsForReductionAndRefusalRestoresQualityWithoutRetry()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("30"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        QVERIFY(station.sliceById(sliceId));
        const auto floor = spectrumDisplayCost(256, 10, false);
        QVERIFY(floor.has_value());
        const auto ps3AndFloor = sumDisplayCharges({ps3DisplayCharge(), floor->charge});
        QVERIFY(ps3AndFloor.has_value());
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({ps3AndFloor->applicationBytesPerSecond,
                                               10'000'000, 1}));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        server.setPs3DisplayAdmissionHandler([](bool enabled, QString* refusal) {
            if (!enabled) { return true; }
            if (refusal) { *refusal = QStringLiteral("test PS3 refusal"); }
            return false;
        });

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        PanadapterApplet* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(sliceId);
        SpectrumWidget* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(
            station.streamCentreHz(station.sliceById(sliceId)->streamIndex()), 48000);
        widget->setWfUpdatePeriodMs(20);
        stack.resize(1200, 600);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy started(&client, &StationClient::ps3DisplaySubscriptionStarted);
        QSignalSpy finished(&client, &StationClient::ps3DisplaySubscriptionFinished);
        auto* stationLink = new HoldingAllocationResultTransport;
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 1);
        QTRY_COMPARE(stationLink->held.size(), 1);
        const QJsonObject original = lastControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(original.value(QStringLiteral("pixels")).toInt() > 256
                || original.value(QStringLiteral("fps")).toInt() > 10);
        stationLink->releaseHeld();
        QTRY_VERIFY(showsDisplay(controller, applet)
                    && applet->remoteDisplayStatus().isEmpty());

        remote.pureSignalFacade()->setAmpViewSubscribed(true);
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QTRY_COMPARE(stationLink->held.size(), 1);
        QCOMPARE(started.size(), 0);
        const QJsonObject reduction = lastControl(outbound, QStringLiteral("subscribe"));
        QVERIFY(reduction.value(QStringLiteral("pixels")).toInt()
                <= original.value(QStringLiteral("pixels")).toInt());
        QVERIFY(reduction.value(QStringLiteral("fps")).toInt()
                <= original.value(QStringLiteral("fps")).toInt());

        QTimer* subscriptionTimer = nullptr;
        for (QTimer* timer : controller.findChildren<QTimer*>()) {
            if (timer->interval() == 100) { subscriptionTimer = timer; break; }
        }
        QVERIFY(subscriptionTimer);
        QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 2);
        QCOMPARE(started.size(), 0);

        stationLink->releaseHeld();
        QTRY_COMPARE(started.size(), 1);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(1).toBool(), true);
        QCOMPARE(finished.first().at(2).toBool(), false);
        QVERIFY(finished.first().at(3).toString().contains(QStringLiteral("test PS3 refusal")));
        QTRY_COMPARE(countControl(outbound, QStringLiteral("subscribe")), 3);
        QTRY_COMPARE(stationLink->held.size(), 1);
        const QJsonObject restored = lastControl(outbound, QStringLiteral("subscribe"));
        QCOMPARE(restored.value(QStringLiteral("pixels")), original.value(QStringLiteral("pixels")));
        QCOMPARE(restored.value(QStringLiteral("fps")), original.value(QStringLiteral("fps")));
        stationLink->releaseHeld();
        QTRY_VERIFY(controller.panDisplayState(applet->panId()).pureSignalRefusalReason
                        .contains(QStringLiteral("test PS3 refusal")));
        QCOMPARE(controller.panDisplayState(applet->panId()).pureSignal,
                 PanDisplayState::PureSignal::Refused);
        // A reason already in user words is shown as sent (R-R3-21); the raw
        // reason is also logged.
        QVERIFY(applet->remoteDisplayExplanation().contains(QStringLiteral("test PS3 refusal")));
        // The refusal is already visible while the restored allocation's
        // queued acknowledgment is still in flight. Wait for accepted quality.
        QTRY_VERIFY(!controller.panDisplayState(applet->panId()).reduced());
        QTRY_COMPARE(applet->remoteDisplayStatus(), QStringLiteral("PureSignal: refused"));
        QVERIFY(!client.remotePs3DisplaySubscribed());

        for (int attempt = 0; attempt < 3; ++attempt) {
            QVERIFY(QMetaObject::invokeMethod(subscriptionTimer, "timeout", Qt::DirectConnection));
        }
        QCOMPARE(countControl(outbound, QStringLiteral("subscribe")), 3);
        QCOMPARE(started.size(), 1);
        QCOMPARE(finished.size(), 1);
    }

    void fourAcceptedPanFloorsOwnIndependentPlannerWindows()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5,5,192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QList<int> sliceIds;
        for(int n=0;n<4;++n) {
            const int sliceId=station.addSlice(QStringLiteral("pan-%1").arg(n));
            SliceModel* source=station.sliceById(sliceId);
            QVERIFY(source); source->setStreamIndex(0); source->setFrequency(14225000);
            sliceIds.append(sliceId);
        }
        StationServer server(&station, settings, Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        remote.setClarityEnabled(true);
        PanClarityRegistry registry(&remote);
        SettingsProxy proxy; StationClient client(&remote, &proxy); PanadapterStack stack;
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        for (int n=0; n<4; ++n) {
            const QString id = QStringLiteral("pan-%1").arg(n);
            auto* pan=stack.addPanadapter(id); pan->setActiveSliceIndex(sliceIds[n]);
            auto* widget=pan->spectrumWidget();
            widget->setDisplayWindowPreservingHistory(14225000,24000);
            widget->setDbmRange(-100,-60); widget->setWfAgcEnabled(false);
            widget->setWaterfallNFAGCEnabled(false); widget->setDispNormalize(false);
            widget->setWfUseSpectrumMinMax(false); widget->setWfLowThreshold(-110); widget->setWfHighThreshold(-70);
            registry.registerPan(id,widget,nullptr);
            registry.controllerForPan(id)->setPollIntervalMs(0);
        }
        stack.setActivePan(QStringLiteral("pan-0")); stack.resize(600,800); stack.show();
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client,&remote,&stack,nullptr,
            [&media](QObject* owner)->IMediaTransport* { media=new DisplayTransport(owner); return media; });
        controller.setClarityRegistry(&registry);
        QSignalSpy controls(&server,&StationServer::mediaControlReceived);
        QSignalSpy inbound(&client,&StationClient::mediaControlReceived);
        auto* stationLink=new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink=new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink); client.startSession(clientLink,server.token()); server.acceptTransport(stationLink);
        QTRY_VERIFY(media); media->activate();
        QTRY_COMPARE(countControl(controls,QStringLiteral("subscribe")),4);
        QList<QJsonObject> requests;
        for (const auto& event : controls) {
            const QJsonObject payload=event.first().toJsonObject();
            if (payload.value(QStringLiteral("op"))==QStringLiteral("subscribe")) { requests.append(payload); }
        }
        QCOMPARE(requests.size(),4);
        for (int n=0;n<4;++n) {
            const auto request=requests[n];
            const int hosted=sliceIds.indexOf(request.value(QStringLiteral("sliceId")).toInt());
            QVERIFY(hosted>=0);
            const QString id=QStringLiteral("pan-%1").arg(hosted);
            // The request identifies its real widget, including inactive panes.
            QVERIFY(stack.spectrum(id));
            SpectrumContextMessage message;
            message.connectionId=request.value(QStringLiteral("connectionId")).toString();
            message.endpointId=quint32(request.value(QStringLiteral("endpointId")).toDouble());
            message.revision=quint32(request.value(QStringLiteral("revision")).toDouble());
            message.contextGeneration=1; message.sourceStream=0; message.sourceCentreHz=14225000; message.sampleRateHz=192000;
            message.centreHz=14225023.4375; message.spanHz=24046.875;
            message.traceSamples=128; message.waterfallSamples=128; message.minDbm=-180; message.maxDbm=0;
            message.fps=30; message.framesPerLine=1; message.wideband=WidebandDisplayContext{};
            SpectrumContextGrant grant; grant.grantedFftSize=4096; grant.requestedPixels=128; grant.grantedPixels=128;
            message.grant=grant;
            QVERIFY(server.sendMediaControl(encodeRemoteSpectrumContext(message,true),server.mediaSessionEpoch()));
            const float floor=-150.0f+hosted*15.0f;
            QJsonObject event{{QStringLiteral("op"),QStringLiteral("noise-floor")},
                {QStringLiteral("connectionId"),message.connectionId},{QStringLiteral("endpointId"),double(message.endpointId)},
                {QStringLiteral("revision"),double(message.revision)},{QStringLiteral("contextGeneration"),1},{QStringLiteral("floorDbm"),floor}};
            QVERIFY(server.sendMediaControl(event,server.mediaSessionEpoch()));
            QTRY_VERIFY(stack.spectrum(id)->clarityActive()
                && stack.spectrum(id)->wfActiveLowThreshold()==floor-5
                && stack.spectrum(id)->wfActiveHighThreshold()==floor+55);
            QCOMPARE(stack.spectrum(id)->wfActiveHighThreshold(),floor+55);
        }
        // R-DISP-HOLD: each pan first asked for -120 to -50 dBm (its scale
        // and stored levels with the headroom). Pans 0 to 2 take levels
        // below that and ask again; pan 3's (-110 to -50) lie inside what it
        // holds, so it asks nothing. (This test waited for a second ask from
        // every pan and for 10 dB outside every pan's levels: the old rule
        // gave run-time levels their own headroom whenever they arrived.
        // A held range is kept while it holds what the pan needs; a new
        // range is what gets the headroom.)
        QTRY_VERIFY(countControl(controls,QStringLiteral("subscribe"))>=7);
        for(int n=0;n<4;++n) {
            const QString id=QStringLiteral("pan-%1").arg(n);
            QJsonObject latest;
            int asks=0;
            for(const auto& event:controls) { const auto payload=event.first().toJsonObject();
                if(payload.value(QStringLiteral("op"))==QStringLiteral("subscribe") && payload.value(QStringLiteral("sliceId")).toInt()==sliceIds[n]) { latest=payload; ++asks; } }
            QVERIFY(!latest.isEmpty());
            QCOMPARE(asks, n<3 ? 2 : 1);
            const double low=stack.spectrum(id)->wfActiveLowThreshold();
            const double high=stack.spectrum(id)->wfActiveHighThreshold();
            QVERIFY(latest.value(QStringLiteral("minDbm")).toDouble() <= low);
            QVERIFY(latest.value(QStringLiteral("maxDbm")).toDouble() >= high);
            if (n<3) {
                // A new range: the headroom outside what the pan needs.
                QCOMPARE(latest.value(QStringLiteral("minDbm")).toDouble(), low-10);
                QCOMPARE(latest.value(QStringLiteral("maxDbm")).toDouble(), std::max(high,-60.0)+10);
            }
        }
        const int settled=countControl(controls,QStringLiteral("subscribe"));
        stack.setActivePan(QStringLiteral("pan-2"));
        for(int n=0;n<4;++n) { const QString id=QStringLiteral("pan-%1").arg(n); QCOMPARE(stack.spectrum(id)->wfActiveLowThreshold(),-155.0f+n*15); }
        QVERIFY(countControl(controls,QStringLiteral("subscribe"))>=settled);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Display pill plan Task 15 (design 5.6): a pan a Core feeds, with
    // Clarity on. A key and an un-key ask the Core nothing (Clarity stays on
    // the pan, so coreWaterfallLevelsInUse() and the dBm window do not
    // change); the hold after the un-key is twice the fill time of the FFT
    // the Core runs for the pan (granted size, context rate, the decimation
    // asked); the Core's noise-floor messages do not move the window's
    // Clarity floor while keyed or inside the hold, and do after it. The
    // Core here does nothing at the edges (no transmit context, noise-floor
    // sent throughout), so nothing depends on what a newer Core does.
    void remoteKeyEdgesAskNothingAndHoldForTheCoresFft_data()
    {
        QTest::addColumn<int>("grantedFftSize");
        QTest::addColumn<double>("sampleRateHz");
        QTest::addColumn<int>("decimation");
        QTest::addColumn<qint64>("holdMs");
        QTest::newRow("262144 at 48 kHz") << 262144 << 48000.0 << 1 << qint64(10922);
        QTest::newRow("4096 at 192 kHz") << 4096 << 192000.0 << 1 << qint64(1000);
        // 65536 x 4 samples at 192 kHz fill in 1365 ms.
        QTest::newRow("65536 at 192 kHz, decimation 4") << 65536 << 192000.0 << 4 << qint64(2730);
    }

    void remoteKeyEdgesAskNothingAndHoldForTheCoresFft()
    {
        QFETCH(int, grantedFftSize);
        QFETCH(double, sampleRateHz);
        QFETCH(int, decimation);
        QFETCH(qint64, holdMs);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.sliceById(sliceId);
        QVERIFY(stationSlice);
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        NereusSDR::FFTEngine windowEngine(-1); // Rendering > Decimation keeps its value here.
        windowEngine.setDecimation(decimation);
        remote.setFftEngine(&windowEngine);
        const auto clearEngine = qScopeGuard([&remote] { remote.setFftEngine(nullptr); });
        remote.setClarityEnabled(true);
        PanClarityRegistry registry(&remote);
        qint64 clock = 100'000;
        registry.setClockForTest([&clock] { return clock; });
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // R-DISP-RETUNE: the context this test sends by hand states no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        const QString panId = QStringLiteral("pan-0");
        auto* applet = stack.addPanadapter(panId);
        applet->setActiveSliceIndex(sliceId);
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        // A pan range that takes in every level Clarity sets below with its
        // headroom, so the request count is about the key edges, not about
        // the dBm window (runtimeWaterfallLevelsWidenTheDbmWindow).
        widget->setDbmRange(-170.0f, -40.0f);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDispNormalize(false);
        widget->setWfUseSpectrumMinMax(false);
        widget->setWfLowThreshold(-110.0f);
        widget->setWfHighThreshold(-70.0f);
        registry.registerPan(panId, widget, nullptr);
        // As MainWindow wires it: the pan's own hold length.
        registry.setHoldLengthProvider([widget](const QString&) {
            return widget->moxHoldLengthMs();
        });
        ClarityController& clarity = *registry.controllerForPan(panId);
        clarity.setPollIntervalMs(0);
        clarity.setSmoothingTauSec(0);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        controller.setClarityRegistry(&registry);
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(client.spectrumDecimationAvailable());
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(subscription.value(QStringLiteral("decimation")).toInt(), decimation);

        // Before a context the pan has no Core shape: the minimum.
        QCOMPARE(widget->moxHoldLengthMs(), qint64(1000));

        SpectrumContextMessage message;
        message.connectionId = subscription.value(QStringLiteral("connectionId")).toString();
        message.endpointId = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        message.revision = quint32(subscription.value(QStringLiteral("revision")).toDouble());
        message.contextGeneration = 1;
        message.sourceStream = 0;
        message.sourceCentreHz = 14225000;
        message.sampleRateHz = sampleRateHz;
        message.centreHz = 14225000;
        message.spanHz = 24000;
        message.traceSamples = 128;
        message.waterfallSamples = 128;
        message.minDbm = -180;
        message.maxDbm = 0;
        message.fps = 30;
        message.framesPerLine = 1;
        message.wideband = WidebandDisplayContext{};
        SpectrumContextGrant grant;
        grant.grantedFftSize = grantedFftSize;
        grant.requestedPixels = 128;
        grant.grantedPixels = 128;
        message.grant = grant;
        QVERIFY(server.sendMediaControl(encodeRemoteSpectrumContext(message, true),
                                        server.mediaSessionEpoch()));
        QTRY_COMPARE(widget->remoteGrantedFftSize(), grantedFftSize);
        QCOMPARE(widget->remoteDecimation(), decimation);
        QCOMPARE(widget->moxHoldLengthMs(), holdMs);
        // A window with its own engines resets the local shape at every
        // refresh; the Core's shape is kept apart from it.
        widget->setLocalFftShape(0, 0, 0.0);
        QCOMPARE(widget->moxHoldLengthMs(), holdMs);
        widget->setLocalFftShape(4096, 1, 192000.0);
        QCOMPARE(widget->moxHoldLengthMs(), holdMs);

        const auto deliverFloor = [&](double floorDbm) {
            const QJsonObject event{
                {QStringLiteral("op"), QStringLiteral("noise-floor")},
                {QStringLiteral("connectionId"), message.connectionId},
                {QStringLiteral("endpointId"), double(message.endpointId)},
                {QStringLiteral("revision"), double(message.revision)},
                {QStringLiteral("contextGeneration"), 1},
                {QStringLiteral("floorDbm"), floorDbm}};
            const int before = countControl(receivedControls, QStringLiteral("noise-floor"));
            QVERIFY(server.sendMediaControl(event, server.mediaSessionEpoch()));
            QTRY_COMPARE(countControl(receivedControls, QStringLiteral("noise-floor")),
                         before + 1);
        };
        const auto settle = [] {
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs * 2 + 30);
        };

        deliverFloor(-130.0);
        QTRY_VERIFY(widget->clarityActive());
        QCOMPARE(clarity.smoothedFloor(), -130.0f);
        QCOMPARE(widget->wfActiveLowThreshold(), -135.0f);
        settle();
        const int asked = countControl(controls, QStringLiteral("subscribe"));
        const int released = countControl(controls, QStringLiteral("unsubscribe"));
        const bool coreLevels = widget->coreWaterfallLevelsInUse();
        QSignalSpy floors(&clarity, &ClarityController::noiseFloorChanged);

        // The key, as the window makes it on every pan.
        constexpr int kBand = 5;
        widget->setDisplayKeyed(true, widget->moxHoldLengthMs(), kBand);
        registry.setKeyed(true);
        controller.setPanTransmitting(panId, true, false);
        QVERIFY(widget->clarityActive());
        QCOMPARE(widget->coreWaterfallLevelsInUse(), coreLevels);
        // The Core goes on sending its floor, with the transmitter in it.
        clock += 500;
        deliverFloor(-60.0);
        QCOMPARE(floors.count(), 0);
        QCOMPARE(clarity.smoothedFloor(), -130.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), asked);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), released);

        // The un-key.
        clock += 500;
        const qint64 unkeyedAt = clock;
        widget->setDisplayKeyed(false, widget->moxHoldLengthMs(), kBand);
        registry.setKeyed(false);
        controller.setPanTransmitting(panId, false, false);
        QVERIFY(widget->clarityActive());
        QCOMPARE(widget->coreWaterfallLevelsInUse(), coreLevels);
        QVERIFY(clarity.inHold(unkeyedAt));
        QVERIFY(clarity.inHold(unkeyedAt + holdMs - 1));
        QVERIFY(!clarity.inHold(unkeyedAt + holdMs));

        // Inside the hold the Core's floor is not taken.
        deliverFloor(-70.0);
        clock = unkeyedAt + holdMs - 1;
        deliverFloor(-70.0);
        QCOMPARE(floors.count(), 0);
        QCOMPARE(clarity.smoothedFloor(), -130.0f);
        QCOMPARE(widget->wfActiveLowThreshold(), -135.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), asked);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), released);

        // After it, it is.
        clock = unkeyedAt + holdMs;
        deliverFloor(-118.0);
        QCOMPARE(floors.count(), 1);
        QCOMPARE(clarity.smoothedFloor(), -118.0f);
        QVERIFY(widget->clarityActive());

        // The pan still draws the Core's frames.
        DisplayCodecFrame frame;
        frame.context = {message.endpointId, 1, -180, 0, 128, 128, 0};
        frame.traceDbm = QVector<float>(128, -75);
        frame.waterfallDbm = QVector<float>(128, -125);
        frame.waterfallAdvance = true;
        DisplayCodecEncoder encoder;
        media->deliver(encoder.encode(frame));
        QTRY_COMPARE(frames.count(), 1);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), asked);
        QCOMPARE(countControl(controls, QStringLiteral("unsubscribe")), released);

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void contextMediaAndRetirementStayInAuthenticatedSession()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        PanClarityRegistry registry(&remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        registry.registerPan(QStringLiteral("pan-0"), widget, nullptr);
        ClarityController& clarity = *registry.controllerForPan(QStringLiteral("pan-0"));
        clarity.setPollIntervalMs(0); clarity.setSmoothingTauSec(0);
        QSignalSpy floors(&clarity, &ClarityController::noiseFloorChanged);
        // Parity Task 17 follow-up: the dBm window holds the run-time
        // waterfall levels with headroom. Levels that already take in the
        // ones Clarity sets below (-137.375 to -77.375) keep this test's
        // request count about the context, not about the window.
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        const float savedLow = widget->wfLowThreshold();
        const float savedHigh = widget->wfHighThreshold();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setSpectrumRenderMode(int(SpectrumRenderMode::Mode3D));
        // This single-frame identity fixture uses one 3D row per accepted frame.
        widget->setDssRowDivider(1);
        widget->setWfUpdatePeriodMs(20);
        stack.resize(600, 400);
        // Budget mechanics: fixed waterfall levels, so no pan asks the
        // Core for its waterfall AGC levels, which the Core charges on top
        // of the frames (coreWaterfallAgcAsksNothingAsItSettles).
        for (PanadapterApplet* pan : stack.allApplets()) {
            pan->spectrumWidget()->setWfAgcEnabled(false);
        }
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        controller.setClarityRegistry(&registry);
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        QVERIFY(!media); // No pre-authentication peer or subscription.
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        QTRY_COMPARE(countControl(controls, QStringLiteral("start")), 1);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 0);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        QVERIFY(client.remoteWidebandAvailable());
        QVERIFY(!widget->extendedMode());
        QVERIFY(subscription.value(QStringLiteral("extendedView")).isBool());
        QVERIFY(subscription.value(QStringLiteral("extendedView")).toBool());
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        QJsonObject noiseFloor{
            {QStringLiteral("op"), QStringLiteral("noise-floor")},
            {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1},
            {QStringLiteral("floorDbm"), -132.375}};
        const auto deliverFloor = [&](const QJsonObject& message) {
            const int before = countControl(receivedControls, QStringLiteral("noise-floor"));
            QVERIFY(server.sendMediaControl(message, server.mediaSessionEpoch()));
            QTRY_COMPARE(countControl(receivedControls, QStringLiteral("noise-floor")), before + 1);
        };
        deliverFloor(noiseFloor); // No accepted display context yet.
        QCOMPARE(floors.size(), 0);
        DisplayCodecFrame frame;
        frame.context = {id, 1, -180, 0, 128, 128, 0};
        frame.traceDbm = QVector<float>(128, -75);
        frame.waterfallDbm = QVector<float>(128, -125);
        frame.waterfallAdvance = true;
        DisplayCodecEncoder encoder;
        const QByteArray packet = encoder.encode(frame);
        QVERIFY(!packet.isEmpty());
        media->deliver(packet); // Media may race its reliable context.
        QCOMPARE(frames.count(), 0);
        QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
            {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), WidebandDisplayContext{}.toJson()},
            // Minor 9: what Core granted this endpoint.
            {QStringLiteral("grantedFftSize"), 4096}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(controller.spectrumGrantNegotiated());
        QJsonObject invalid = context;
        // The minor-8 shape is refused once minor 9 is agreed.
        for (const char* key : {"grantedFftSize", "grantedTier", "requestedPixels",
                                "grantedPixels", "limit"}) {
            invalid.remove(QLatin1String(key));
        }
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        media->deliver(packet);
        QCOMPARE(frames.count(), 0);
        invalid = context;
        invalid.insert(QStringLiteral("traceSamples"), 128.5);
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        media->deliver(packet);
        QCOMPARE(frames.count(), 0);
        invalid = context;
        invalid.remove(QStringLiteral("wideband"));
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        media->deliver(packet);
        QCOMPARE(frames.count(), 0);
        invalid = context;
        QJsonObject malformedWideband = WidebandDisplayContext{}.toJson();
        malformedWideband.insert(QStringLiteral("active"), true);
        invalid.insert(QStringLiteral("wideband"), malformedWideband);
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        media->deliver(packet);
        QCOMPARE(frames.count(), 0);

        const auto availableWideband = [](bool active) {
            WidebandDisplayContext wideband;
            wideband.available = true;
            wideband.active = active;
            wideband.physicalAdcIndex = 0;
            wideband.filterChainIndex = 0;
            wideband.sourceGeneration = active ? 7 : 0;
            wideband.adcRateHz = 4000000;
            return wideband.toJson();
        };

        // An active context needs both the saved request permission and RF
        // geometry that actually leaves the DDC source window.
        widget->setDisplayWindowPreservingHistory(14500000, 24000);
        widget->setExtendedViewAllowed(false);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        const QJsonObject permissionOff = lastControl(controls, QStringLiteral("subscribe"));
        QVERIFY(!permissionOff.value(QStringLiteral("extendedView")).toBool());
        invalid = context;
        invalid.insert(QStringLiteral("revision"),
                       permissionOff.value(QStringLiteral("revision")));
        invalid.insert(QStringLiteral("centreHz"), 14500000);
        invalid.insert(QStringLiteral("spanHz"), 24000);
        invalid.insert(QStringLiteral("wideband"), availableWideband(true));
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), 0);

        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setExtendedViewAllowed(true);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        const QJsonObject permissionOn = lastControl(controls, QStringLiteral("subscribe"));
        QVERIFY(permissionOn.value(QStringLiteral("extendedView")).toBool());
        context.insert(QStringLiteral("revision"),
                       permissionOn.value(QStringLiteral("revision")));
        noiseFloor.insert(QStringLiteral("revision"),
                          permissionOn.value(QStringLiteral("revision")));

        // A narrow inactive view is still extended geometry when it sits
        // wholly outside sourceCentre +/- sampleRate/2.
        invalid = context;
        invalid.insert(QStringLiteral("centreHz"), 14500000);
        invalid.insert(QStringLiteral("spanHz"), 24000);
        invalid.insert(QStringLiteral("wideband"), availableWideband(false));
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), 0);

        // Permission alone cannot make an in-DDC accepted crop active.
        invalid = context;
        invalid.insert(QStringLiteral("centreHz"), 14225000);
        invalid.insert(QStringLiteral("spanHz"), 24000);
        invalid.insert(QStringLiteral("wideband"), availableWideband(true));
        QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
        QTest::qWait(30);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), 0);

        // SpectrumEndpoint's real inactive crop rounds outward to bin edges:
        // 14,225,023.4375 +/- 12,023.4375 Hz. It remains inside the DDC
        // bounds and must be admitted without any guessed bin-width margin.
        context.insert(QStringLiteral("wideband"), availableWideband(false));
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        // Temporary render hides also preserve the active pan's Clarity
        // observations. Logical retirement below still rejects later samples.
        widget->hide();
        deliverFloor(noiseFloor);
        QCOMPARE(floors.size(), 1);
        widget->show();
        // A valid accepted recipient keeps adapting when another full pan is selected.
        auto* other = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(other);
        stack.setActivePan(QStringLiteral("pan-1"));
        deliverFloor(noiseFloor);
        QCOMPARE(floors.size(), 2);
        floors.removeLast();
        stack.setActivePan(QStringLiteral("pan-0"));
        stack.removePanadapter(QStringLiteral("pan-1"));
        QCOMPARE(clarity.smoothedFloor(), -132.375f);
        QCOMPARE(widget->wfActiveLowThreshold(), -137.375f);
        QCOMPARE(widget->wfActiveHighThreshold(), -77.375f);
        QCOMPARE(widget->wfLowThreshold(), savedLow);
        QCOMPARE(widget->wfHighThreshold(), savedHigh);
        const QList<QPair<QString, QJsonValue>> invalidFields{
            {QStringLiteral("connectionId"), QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("endpointId"), double(id + 1000)},
            {QStringLiteral("revision"), permissionOn.value(QStringLiteral("revision")).toDouble() + 1},
            {QStringLiteral("contextGeneration"), 2},
            {QStringLiteral("contextGeneration"), 1.5},
            {QStringLiteral("floorDbm"), QStringLiteral("-120")},
            {QStringLiteral("floorDbm"), -401},
            {QStringLiteral("floorDbm"), 101},
            {QStringLiteral("floorDbm"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("extra"), 1}};
        for (const auto& [key, value] : invalidFields) {
            QJsonObject badFloor = noiseFloor;
            badFloor.insert(key, value);
            deliverFloor(badFloor);
            QCOMPARE(floors.size(), 1);
        }
        client.mediaControlReceived(noiseFloor, client.sessionEpoch() - 1);
        QCOMPARE(floors.size(), 1);
        media->deliver(packet);
        QCOMPARE(frames.count(), 1);
        QCOMPARE(widget->renderedPixels().size(), 128);
        QVERIFY(std::abs(widget->renderedPixels().first() + 75) < 0.4);
        QVERIFY(std::abs(widget->wfRenderedPixels().first() + 125) < 0.4);
        QTRY_COMPARE(widget->dssRowsPushedForTest(), 1);
        QTest::qWait(250);
        // Bin-aligned accepted geometry must not create a resubscribe loop.
        const int acceptedSubscriptionCount = 3;
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")),
                 acceptedSubscriptionCount);

        // A regular tune must keep painted history even during the request/ACK
        // gap. Old media is retired immediately; only the new generation paints.
        // R-R3-21: the last trace stays, drawn at the frequency it was
        // captured at, so the tune never blanks it.
        widget->setCenterFrequency(widget->centerFrequency() + 500);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")),
                     acceptedSubscriptionCount + 1);
        QCOMPARE(widget->dssRowsPushedForTest(), 1);
        QCOMPARE(widget->renderedPixels().size(), 128);
        QVERIFY(std::abs(widget->renderedPixels().first() + 75) < 0.4);
        media->deliver(packet);
        QCOMPARE(frames.count(), 1);
        const auto tuned = lastControl(controls, QStringLiteral("subscribe"));
        context.insert(QStringLiteral("revision"), tuned.value(QStringLiteral("revision")));
        context.insert(QStringLiteral("contextGeneration"), 2);
        context.insert(QStringLiteral("centreHz"), widget->centerFrequency());
        context.insert(QStringLiteral("spanHz"), widget->bandwidth());
        const int contextsBefore = countControl(receivedControls, QStringLiteral("context"));
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(receivedControls, QStringLiteral("context")), contextsBefore + 1);
        QCOMPARE(widget->dssRowsPushedForTest(), 1);
        media->deliver(packet);
        QCOMPARE(frames.count(), 1);
        frame.context.contextGeneration = 2;
        media->deliver(encoder.encode(frame));
        QCOMPARE(frames.count(), 2);
        QTRY_COMPARE(widget->dssRowsPushedForTest(), 2);
        noiseFloor.insert(QStringLiteral("revision"), tuned.value(QStringLiteral("revision")));
        noiseFloor.insert(QStringLiteral("contextGeneration"), 2);
        noiseFloor.insert(QStringLiteral("floorDbm"), -131);
        deliverFloor(noiseFloor);
        QCOMPARE(floors.size(), 2);

        widget->setDisplayWindowPreservingHistory(14425000, 24000);
        deliverFloor(noiseFloor); // A gesture retires old RF meaning before its ACK.
        QCOMPARE(floors.size(), 2);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")),
                     acceptedSubscriptionCount + 2);
        const auto extended = lastControl(controls, QStringLiteral("subscribe"));
        context.insert(QStringLiteral("revision"), extended.value(QStringLiteral("revision")));
        context.insert(QStringLiteral("contextGeneration"), 3);
        context.insert(QStringLiteral("centreHz"), widget->centerFrequency());
        context.insert(QStringLiteral("spanHz"), widget->bandwidth());
        context.insert(QStringLiteral("wideband"), availableWideband(true));
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_VERIFY(widget->remoteWidebandActive());
        QVERIFY(widget->extendedMode());
        QCOMPARE(widget->dssRowsPushedForTest(), 2);
        frame.context.contextGeneration = 3;
        media->deliver(encoder.encode(frame));
        QCOMPARE(frames.count(), 3);
        QTRY_COMPARE(widget->dssRowsPushedForTest(), 3);
        // Logical pan retirement, rather than a transient QWidget hide,
        // terminates this endpoint. Keep guarded pointers because the stack
        // deletes the applet after emitting panRetired().
        QPointer<PanadapterApplet> retiredApplet(applet);
        QPointer<SpectrumWidget> retiredWidget(widget);
        stack.removePanadapter(QStringLiteral("pan-0"));
        QTRY_COMPARE(countControl(controls, QStringLiteral("unsubscribe")), 1);
        QCOMPARE(controller.activeEndpointCount(), 0);
        deliverFloor(noiseFloor);
        QCOMPARE(floors.size(), 2);
        media->deliver(packet);
        QCOMPARE(frames.count(), 3);
        QTRY_VERIFY(retiredApplet.isNull());
        QTRY_VERIFY(retiredWidget.isNull());
        client.disconnectFromStation(QStringLiteral("test complete"));
        QVERIFY(!media || !media->active);
    }

    // R-DISP-RETUNE (third wave): the window draws what it is sent. The Core
    // states the picture each context continues; a context of the picture
    // the pan shows is taken for drawing though it answers an earlier
    // request, and its frames are drawn. That is not an accepted answer:
    // the window asks nothing more, the pan's own context is untouched and
    // no keyframe is asked for until a frame is found missing, and then
    // against the generation drawn. A context of another picture answering
    // an earlier request is ignored, as it always was, and so is a context
    // older than the one drawn and a context for a pan that has gone.
    void aContextOfThePictureShownIsDrawnThoughItAnswersAnEarlierRequest()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        // This Core states the picture, and the window asked it to.
        QCOMPARE(client.capabilities().spectrumPictureVersion, 1);
        const QJsonObject first = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(first.value(QStringLiteral("endpointId")).toDouble());
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const auto contextFor = [&](const QJsonObject& ask, int generation, int picture,
                                    double centerHz, double spanHz) {
            return QJsonObject{
                {QStringLiteral("op"), QStringLiteral("context")},
                {QStringLiteral("connectionId"), ask.value(QStringLiteral("connectionId"))},
                {QStringLiteral("endpointId"), double(id)},
                {QStringLiteral("revision"), ask.value(QStringLiteral("revision"))},
                {QStringLiteral("contextGeneration"), generation},
                {QStringLiteral("pictureGeneration"), picture},
                {QStringLiteral("sourceStream"), 0},
                {QStringLiteral("sourceCentreHz"), 14225000},
                {QStringLiteral("sampleRateHz"), 192000},
                {QStringLiteral("centreHz"), centerHz}, {QStringLiteral("spanHz"), spanHz},
                {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
                {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
                {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
                {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
                {QStringLiteral("framesPerLine"), 1},
                {QStringLiteral("wideband"), wideband.toJson()},
                {QStringLiteral("grantedFftSize"), 8192},
                {QStringLiteral("grantedTier"), QStringLiteral("wide")},
                {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
                {QStringLiteral("limit"), QStringLiteral("none")}};
        };
        // Sent, and read by the window.
        const auto deliverContext = [&](const QJsonObject& context) {
            const int before = countControl(receivedControls, QStringLiteral("context"));
            return server.sendMediaControl(context, server.mediaSessionEpoch())
                && QTest::qWaitFor([&] {
                       return countControl(receivedControls, QStringLiteral("context")) > before;
                   }, 5'000);
        };
        // Each context's encoder counts its own frames, as the Core's does.
        QHash<quint32, quint32> sequences;
        quint32 made = 0;
        const auto frameOf = [&](quint32 generation, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, generation, -180, 0, 128, 128, 0};
            frame.encoderSequence = ++sequences[generation];
            frame.producerTimestamp = quint64(1'000'000'000LL + ++made * 33'333'333LL);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            frame.waterfallDbm = QVector<float>(128, level);
            return frame;
        };
        const auto shows = [&](float level) {
            return widget->renderedPixels().size() == 128
                && std::abs(widget->renderedPixels().at(64) - level) < 0.4f;
        };

        // The picture the pan shows: context 1, which starts picture 1.
        QVERIFY(deliverContext(contextFor(first, 1, 1, 14225023.4375, 24046.875)));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        QCOMPARE(widget->remoteCodecForTest().contextGeneration, quint32(1));
        DisplayCodecEncoder encoder1;
        media->deliver(encoder1.encode(frameOf(1, -100.0f), true));
        QCOMPARE(frames.count(), 1);
        QVERIFY(shows(-100.0f));

        // Two steps of a wheel, the second before the first is answered.
        widget->setCenterFrequency(widget->centerFrequency() + 500);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        const QJsonObject second = lastControl(controls, QStringLiteral("subscribe"));
        widget->setCenterFrequency(widget->centerFrequency() + 500);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        const QJsonObject third = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 codecWhileAsking = widget->remoteCodecForTest().contextGeneration;
        const double secondCenter = second.value(QStringLiteral("centreHz")).toDouble();
        const double secondSpan = second.value(QStringLiteral("spanHz")).toDouble();

        // The answer to the first step: picture 1 carried on. Its frames
        // are drawn; nothing else of the window moves.
        QVERIFY(deliverContext(contextFor(second, 2, 1, secondCenter, secondSpan)));
        DisplayCodecEncoder encoder2;
        media->deliver(encoder2.encode(frameOf(2, -60.0f), true));
        QCOMPARE(frames.count(), 2);
        QVERIFY(shows(-60.0f));
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        QCOMPARE(widget->remoteCodecForTest().contextGeneration, codecWhileAsking);

        // Another picture answering that earlier request: ignored. The
        // generation drawn is still drawn.
        QVERIFY(deliverContext(contextFor(second, 3, 3, secondCenter, secondSpan)));
        DisplayCodecEncoder encoder3;
        media->deliver(encoder3.encode(frameOf(3, -20.0f), true));
        QCOMPARE(frames.count(), 2);
        media->deliver(encoder2.encode(frameOf(2, -61.0f)));
        QCOMPARE(frames.count(), 3);
        QVERIFY(shows(-61.0f));

        // Out of order: context 5 arrives, then context 4. The older one is
        // ignored.
        QVERIFY(deliverContext(contextFor(second, 5, 1, secondCenter, secondSpan)));
        DisplayCodecEncoder encoder5;
        media->deliver(encoder5.encode(frameOf(5, -50.0f), true));
        QCOMPARE(frames.count(), 4);
        QVERIFY(deliverContext(contextFor(second, 4, 1, secondCenter, secondSpan)));
        DisplayCodecEncoder encoder4;
        media->deliver(encoder4.encode(frameOf(4, -40.0f), true));
        QCOMPARE(frames.count(), 4);
        media->deliver(encoder5.encode(frameOf(5, -51.0f)));
        QCOMPARE(frames.count(), 5);
        QVERIFY(shows(-51.0f));

        // A frame of the generation drawn is missing: the keyframe is asked
        // for against that generation.
        QTest::qWait(250); // past the window's 200 ms keyframe-request spacing
        QVERIFY(deliverContext(contextFor(second, 6, 1, secondCenter, secondSpan)));
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        DisplayCodecEncoder encoder6;
        const QByteArray lost = encoder6.encode(frameOf(6, -45.0f), true);
        media->deliver(encoder6.encode(frameOf(6, -46.0f)));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 2);
        QCOMPARE(lastControl(controls, QStringLiteral("keyframe"))
                     .value(QStringLiteral("contextGeneration")).toInt(), 6);
        QCOMPARE(frames.count(), 5);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);

        // The answer to the latest request, picture 1 still: accepted, as a
        // moved picture, with its keyframe asked for.
        QTest::qWait(250);
        QVERIFY(deliverContext(contextFor(third, 7, 1, widget->centerFrequency(),
                                          widget->bandwidth())));
        QTRY_COMPARE(widget->remoteCodecForTest().contextGeneration, quint32(7));
        QCOMPARE(widget->remoteContextPicture(), SpectrumWidget::RemoteContextPicture::Moved);
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 3);
        QCOMPARE(lastControl(controls, QStringLiteral("keyframe"))
                     .value(QStringLiteral("contextGeneration")).toInt(), 7);
        // The generation that was only drawn is retired with it.
        media->deliver(lost);
        QCOMPARE(frames.count(), 5);
        DisplayCodecEncoder encoder7;
        media->deliver(encoder7.encode(frameOf(7, -30.0f), true));
        QCOMPARE(frames.count(), 6);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);

        // The pan goes: a context for its endpoint is for nobody.
        QPointer<PanadapterApplet> retiredApplet(applet);
        QPointer<SpectrumWidget> retiredWidget(widget);
        stack.removePanadapter(QStringLiteral("pan-0"));
        QTRY_COMPARE(countControl(controls, QStringLiteral("unsubscribe")), 1);
        QVERIFY(deliverContext(contextFor(third, 8, 1, 14226000, 24046.875)));
        DisplayCodecEncoder encoder8;
        media->deliver(encoder8.encode(frameOf(8, -10.0f), true));
        QCOMPARE(frames.count(), 6);
        QTRY_VERIFY(retiredApplet.isNull());
        QTRY_VERIFY(retiredWidget.isNull());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-DISP-RETUNE (fourth wave): what the window must not draw. Each row
    // is a context that states the picture the pan shows, is newer than the
    // one drawn and answers a request the window has since replaced, so
    // nothing but the one rule the row names keeps it out of the pan:
    //   - it is of another receiver's stream;
    //   - the pan's receiver has moved to another stream since;
    //   - the pan's stream was given up and claimed again since (its epoch
    //     changed), so the picture shown is no longer this binding's;
    //   - it is a transmit display's context;
    //   - the pan shows the transmit display;
    //   - its wideband state does not fit the frequencies it states.
    // None of it is drawn: no frame of it is counted, the pan shows what it
    // showed and no keyframe is asked for it.
    void aContextTheWindowMustNotDrawIsNotDrawn_data()
    {
        QTest::addColumn<QString>("rule");
        QTest::newRow("of another stream") << QStringLiteral("other-stream");
        QTest::newRow("after the receiver changed stream") << QStringLiteral("stream-changed");
        QTest::newRow("after the stream epoch changed") << QStringLiteral("epoch-changed");
        QTest::newRow("a transmit context") << QStringLiteral("transmit-context");
        QTest::newRow("while the transmit display is shown") << QStringLiteral("transmit-shown");
        QTest::newRow("wideband that does not fit") << QStringLiteral("wideband");
    }

    void aContextTheWindowMustNotDrawIsNotDrawn()
    {
        QFETCH(QString, rule);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        // A TX analyzer: this Core offers the transmit display, so a context
        // says whether it is one.
        TxAnalyzer txAnalyzer(TxAnalyzer::kTxDispId);
        station.setTxAnalyzer(&txAnalyzer);
        const auto clearAnalyzer = qScopeGuard([&station] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        QSignalSpy transmitContexts(&controller, &RemoteMediaController::transmitContextReceived);
        QSignalSpy transmitFrames(&controller, &RemoteMediaController::transmitFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QCOMPARE(client.capabilities().spectrumPictureVersion, 1);
        QVERIFY(controller.txDisplayNegotiated());
        const QJsonObject first = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(first.value(QStringLiteral("endpointId")).toDouble());
        SliceModel* slice = remote.sliceById(stationSlice->sliceIndex());
        QVERIFY(slice);
        QCOMPARE(slice->streamIndex(), 0);
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        constexpr double kCenterHz = 14225023.4375;
        constexpr double kSpanHz = 24046.875;
        // A context of picture 1 answering the pan's first request.
        const auto contextOf = [&](int generation, int sourceStream, bool transmit,
                                   double centerHz) {
            return QJsonObject{
                {QStringLiteral("op"), QStringLiteral("context")},
                {QStringLiteral("connectionId"), first.value(QStringLiteral("connectionId"))},
                {QStringLiteral("endpointId"), double(id)},
                {QStringLiteral("revision"), first.value(QStringLiteral("revision"))},
                {QStringLiteral("contextGeneration"), generation},
                {QStringLiteral("pictureGeneration"), 1},
                {QStringLiteral("sourceStream"), sourceStream},
                {QStringLiteral("sourceCentreHz"), 14225000},
                {QStringLiteral("sampleRateHz"), 192000},
                {QStringLiteral("centreHz"), centerHz}, {QStringLiteral("spanHz"), kSpanHz},
                {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
                {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
                {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
                {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
                {QStringLiteral("framesPerLine"), 1},
                {QStringLiteral("wideband"), wideband.toJson()},
                {QStringLiteral("grantedFftSize"), 8192},
                {QStringLiteral("grantedTier"), QStringLiteral("wide")},
                {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
                {QStringLiteral("limit"), QStringLiteral("none")},
                {QStringLiteral("transmit"), transmit}};
        };
        // Sent, and read by the window.
        const auto deliverContext = [&](const QJsonObject& context) {
            const int before = countControl(receivedControls, QStringLiteral("context"));
            return server.sendMediaControl(context, server.mediaSessionEpoch())
                && QTest::qWaitFor([&] {
                       return countControl(receivedControls, QStringLiteral("context")) > before;
                   }, 5'000);
        };
        quint32 made = 0;
        // The first frame of a context, as its own encoder makes it.
        const auto keyframeOf = [&](quint32 generation, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, generation, -180, 0, 128, 128, 0};
            frame.encoderSequence = 1;
            frame.producerTimestamp = quint64(1'000'000'000LL + ++made * 33'333'333LL);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            frame.waterfallDbm = QVector<float>(128, level);
            DisplayCodecEncoder encoder;
            return encoder.encode(frame, true);
        };
        const auto shows = [&](float level) {
            return widget->renderedPixels().size() == 128
                && std::abs(widget->renderedPixels().at(64) - level) < 0.4f;
        };

        // The picture the pan shows: context 1, which starts picture 1.
        QVERIFY(deliverContext(contextOf(1, 0, false, kCenterHz)));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        QCOMPARE(widget->remoteCodecForTest().contextGeneration, quint32(1));
        media->deliver(keyframeOf(1, -100.0f));
        QCOMPARE(frames.count(), 1);
        QVERIFY(shows(-100.0f));

        quint32 generation = 2;
        if (rule == QLatin1String("transmit-shown")) {
            // The Core's transmit display for this pan, answering the
            // request the pan has: taken, and its keyframe asked for.
            QTest::qWait(250); // past the window's 200 ms keyframe-request spacing
            QVERIFY(deliverContext(contextOf(int(generation), 0, true, kCenterHz)));
            QTRY_COMPARE(transmitContexts.count(), 1);
            QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 2);
            ++generation;
        }
        // The window's request moves on, so a context that answers the
        // first one answers an earlier request.
        if (rule == QLatin1String("stream-changed")) {
            slice->setStreamIndex(1);
        } else if (rule == QLatin1String("epoch-changed")) {
            slice->setStreamEpoch(slice->streamEpoch() + 1);
        } else {
            widget->setCenterFrequency(widget->centerFrequency() + 500);
        }
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        const QJsonObject second = lastControl(controls, QStringLiteral("subscribe"));
        QVERIFY(second.value(QStringLiteral("revision")) != first.value(QStringLiteral("revision")));
        QCOMPARE(quint32(second.value(QStringLiteral("endpointId")).toDouble()), id);
        const quint32 codecBefore = widget->remoteCodecForTest().contextGeneration;
        const int keyframesBefore = countControl(controls, QStringLiteral("keyframe"));

        // The context of the row: picture 1, newer than the one drawn.
        const bool otherStream = rule == QLatin1String("other-stream");
        const bool transmitContext = rule == QLatin1String("transmit-context");
        // 150 kHz above the receiver's center is outside its 192 kHz of
        // samples: frequencies only the wideband source has, in a context
        // that says the wideband source is not running.
        const double centerHz = rule == QLatin1String("wideband") ? kCenterHz + 150'000.0
                                                                  : kCenterHz;
        QVERIFY(deliverContext(contextOf(int(generation), otherStream ? 1 : 0,
                                         transmitContext, centerHz)));
        media->deliver(keyframeOf(generation, -60.0f));
        QCOMPARE(frames.count(), 1);
        QCOMPARE(transmitFrames.count(), 0);
        QVERIFY(shows(-100.0f));
        QCOMPARE(widget->remoteCodecForTest().contextGeneration, codecBefore);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), keyframesBefore);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        QCOMPARE(transmitContexts.count(), rule == QLatin1String("transmit-shown") ? 1 : 0);

        if (otherStream || transmitContext || rule == QLatin1String("wideband")) {
            // Nothing else stood in the way: the same context without the
            // row's fault is drawn.
            ++generation;
            QVERIFY(deliverContext(contextOf(int(generation), 0, false, kCenterHz)));
            media->deliver(keyframeOf(generation, -40.0f));
            QCOMPARE(frames.count(), 2);
            QVERIFY(shows(-40.0f));
            QCOMPARE(countControl(controls, QStringLiteral("keyframe")), keyframesBefore);
        }
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-21 / R-R3-08: a remote pan's display rides a stalling link and
    // plays in step with the audio's clock.
    //  1. The diagnosis's pure-delay burst (40 frames at 33 ms, 10 to 23
    //     held and released back to back): 40 rows, no keyframe request.
    //  2. One lost delta: one keyframe request, the gap's rows blended (or
    //     repeated while waiting) up to the keyframe, one row per slot.
    //  3. With clock echoes from a Core on one clock, frames wait for their
    //     Core time plus the delay; a tune meanwhile moves the axis at once
    //     and the waiting row is drawn at the frequency it was captured at.
    //  4. An older Core (no displayClockVersion): drawn on arrival.
    void displayRidesAStallAndPlaysOnTheAudioClock()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        // Fixed levels: runtime ones would widen the dBm window as rows
        // arrive and renew the subscription mid-test.
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        widget->setWaterfallTickerPausedForTest(true);
        widget->setDisplayDuplex(true);
        widget->setMoxOverlay(true);
        widget->setTestingIMD(true);
        widget->setShowIMDMeasurements(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        QSignalSpy imdChanges(widget, &SpectrumWidget::imdMeasurementChanged);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QCOMPARE(client.capabilities().displayClockVersion, 1);
        QVERIFY(controller.displayClockNegotiated());
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
            {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), wideband.toJson()},
            {QStringLiteral("grantedFftSize"), 8192}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        const int keyframesAtStart = 1;

        constexpr qint64 kPeriodNs = 1'000'000'000 / 30;
        DisplayCodecEncoder encoder;
        const auto frameAt = [&](int index, qint64 producerNs, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, 1, -180, 0, 128, 128, 0};
            frame.encoderSequence = quint32(index + 1);
            frame.producerTimestamp = quint64(producerNs);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            if (index == 44) {
                frame.traceDbm[16] = -88.0f;
                frame.traceDbm[35] = -65.0f;
                frame.traceDbm[54] = -22.0f;
                frame.traceDbm[73] = -24.0f;
                frame.traceDbm[92] = -66.0f;
                frame.traceDbm[111] = -89.0f;
            }
            frame.waterfallDbm = QVector<float>(128, level);
            return frame;
        };
        const auto drainRows = [&] {
            for (int i = 0; i < 64 && widget->remoteRowQueueDepthForTest() > 0; ++i) {
                widget->tickWaterfallForTest();
            }
        };

        // 1. Pure delay.
        const qint64 base = 1'000'000'000;
        for (int i = 0; i < 40; ++i) {
            media->deliver(encoder.encode(frameAt(i, base + i * kPeriodNs, float(-130 + i))));
            if (i < 10 || i > 23) {
                widget->tickWaterfallForTest();
            }
        }
        drainRows();
        QCOMPARE(frames.count(), 40);
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(40));
        QCOMPARE(widget->remoteRowsDroppedForTest(), quint64(0));
        QTest::qWait(50);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), keyframesAtStart);
        QCOMPARE(controller.displayTelemetry().keyframeWaits, quint64(0));

        // 2. One lost delta: frame 45 never arrives; 46..51 need a keyframe,
        //    which the Core sends as frame 52.
        QTest::qWait(250); // past the window's 200 ms keyframe-request spacing
        int imdChangesAtLoss = -1;
        for (int i = 40; i < 60; ++i) {
            const bool keyframe = i == 52;
            const QByteArray packet =
                encoder.encode(frameAt(i, base + i * kPeriodNs, i < 52 ? -120.0f : -60.0f), keyframe);
            if (i == 45) {
                continue;
            }
            media->deliver(packet);
            if (i == 44) {
                QCOMPARE(widget->imdMeasurementFrame().status, ImdStatus::Valid);
                QVERIFY(widget->imdMeasurementFrame().summary.has_value());
                imdChangesAtLoss = imdChanges.count();
            }
            if (i == 46) {
                QCOMPARE(widget->imdMeasurementFrame().status,
                         ImdStatus::WaitingForSpectrum);
                QVERIFY(!widget->imdMeasurementFrame().context.sourceEligible);
                QCOMPARE(imdChanges.count(), imdChangesAtLoss + 1);
                imdChangesAtLoss = imdChanges.count();
            }
            if (i >= 47 && i <= 51) {
                QCOMPARE(widget->imdMeasurementFrame().status,
                         ImdStatus::WaitingForSpectrum);
                QCOMPARE(imdChanges.count(), imdChangesAtLoss);
            }
            if (i == 52) {
                QVERIFY(widget->imdMeasurementFrame().context.sourceEligible);
                QVERIFY(imdChanges.count() > imdChangesAtLoss);
            }
            widget->tickWaterfallForTest();
        }
        drainRows();
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), keyframesAtStart + 1);
        QTest::qWait(250);
        QCOMPARE(countControl(controls, QStringLiteral("keyframe")), keyframesAtStart + 1);
        drainRows();
        RemoteDisplayTelemetry display = controller.displayTelemetry();
        QCOMPARE(display.keyframeWaits, quint64(1));
        QCOMPARE(display.keyframeRequests, quint64(keyframesAtStart + 1));
        QCOMPARE(display.rowsBlended + display.rowsRepeated, quint64(7));
        QVERIFY(display.rowsBlended > 0);
        // The one pan's own counts are the totals.
        QCOMPARE(display.pans.size(), 1);
        QCOMPARE(display.pans.firstKey(), QStringLiteral("pan-0"));
        QCOMPARE(display.pans.first(),
                 (RemoteDisplayPanTelemetry{display.keyframeWaits, display.rowsBlended,
                                            display.rowsRepeated}));
        // One row per slot: 40 before, 20 slots after (13 frames, 7 filled).
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(60));
        QCOMPARE(frames.count(), 40 + 13);
        QVERIFY(!display.displayDelayMs.has_value()); // no echo yet: on arrival
        QVERIFY(display.largestArrivalGapMs.has_value());

        // 3. Clock echoes from a Core whose clock reads 10 s ahead.
        constexpr qint64 kCoreAheadNs = 10'000'000'000;
        QTRY_VERIFY_WITH_TIMEOUT(countControl(controls, QStringLiteral("clock-probe")) >= 1, 3000);
        const QJsonObject probe = lastControl(controls, QStringLiteral("clock-probe"));
        QElapsedTimer sinceProbe;
        sinceProbe.start();
        const qint64 t0 = probe.value(QStringLiteral("t0")).toInteger();
        const QJsonObject echo{
            {QStringLiteral("op"), QStringLiteral("clock-echo")},
            {QStringLiteral("connectionId"), probe.value(QStringLiteral("connectionId"))},
            {QStringLiteral("id"), probe.value(QStringLiteral("id"))},
            {QStringLiteral("t0"), t0},
            {QStringLiteral("t1"), t0 + kCoreAheadNs},
            {QStringLiteral("t2"), t0 + kCoreAheadNs},
            {QStringLiteral("generation"), 0},
            {QStringLiteral("rtpTimestamp"), 0},
            {QStringLiteral("capturedNs"), 0}};
        QVERIFY(server.sendMediaControl(echo, server.mediaSessionEpoch()));
        QTest::qWait(30);
        const auto coreNow = [&] { return t0 + kCoreAheadNs + sinceProbe.nsecsElapsed(); };
        const int framesBefore = int(frames.count());
        encoder.reset();
        QElapsedTimer waited;
        waited.start();
        media->deliver(encoder.encode(frameAt(60, coreNow(), -90.0f)));
        QCOMPARE(int(frames.count()), framesBefore); // waits for its time
        QTRY_COMPARE_WITH_TIMEOUT(int(frames.count()), framesBefore + 1, 2000);
        // The base delay: the one-way trip plus the audio's 80 ms base hold.
        QVERIFY2(waited.elapsed() >= 60, qPrintable(QString::number(waited.elapsed())));
        display = controller.displayTelemetry();
        QVERIFY(display.displayDelayMs.has_value());
        QVERIFY2(*display.displayDelayMs >= 79.0 && *display.displayDelayMs < 200.0,
                 qPrintable(QString::number(*display.displayDelayMs)));
        drainRows();

        // A tune while a frame waits: the axis moves at once, and the trace
        // is never blank: the last trace is drawn at the frequency it was
        // captured at, then the waiting frame's trace and row are, when it
        // presents (shifted 12 kHz, half the span).
        const quint64 rowsBefore = widget->remoteRowsPushedForTest();
        QVector<float> split(128, -140.0f);
        for (int x = 64; x < 128; ++x) { split[x] = -60.0f; }
        DisplayCodecFrame waiting = frameAt(61, coreNow(), -140.0f);
        waiting.waterfallDbm = split;
        waiting.traceDbm = split;
        media->deliver(encoder.encode(waiting));
        const int framesAtTune = int(frames.count());
        const double centreBefore = widget->centerFrequency();
        widget->setCenterFrequency(centreBefore + 12023.4375);
        QCOMPARE(widget->centerFrequency(), centreBefore + 12023.4375);
        QVERIFY(!widget->renderedPixels().isEmpty());
        // Frame 60's flat -90 dBm trace, reprojected: the right half is past
        // its capture and shows its floor, which is -90 too.
        QVERIFY(std::abs(widget->renderedPixels().at(10) + 90.0f) < 1.0f);
        // The rows between the two frames (time the Core sent nothing, as
        // far as this window knows) blend in before it, captured there too.
        QTRY_VERIFY_WITH_TIMEOUT(widget->remoteRowQueueDepthForTest() >= 1, 2000);
        QTest::qWait(300);
        QVERIFY(!widget->renderedPixels().isEmpty());
        QVERIFY(std::abs(widget->renderedPixels().at(10) + 60.0f) < 1.0f);
        QVERIFY(std::abs(widget->renderedPixels().at(70) + 140.0f) < 1.0f);
        const int queuedAtTune = widget->remoteRowQueueDepthForTest();
        drainRows();
        QCOMPARE(widget->remoteRowsPushedForTest(), rowsBefore + quint64(queuedAtTune));
        QCOMPARE(int(frames.count()), framesAtTune);
        const QVector<float> drawn = widget->lastRemoteRowPushedForTest();
        QCOMPARE(drawn.size(), 128);
        // The strong half now fills the left half of the new window.
        // (The codec carries dBm in 0.7 dB steps.)
        QVERIFY(std::abs(drawn.at(10) + 60.0f) < 1.0f);
        QVERIFY(std::abs(drawn.at(60) + 60.0f) < 1.0f);
        QVERIFY(std::abs(drawn.at(70) + 140.0f) < 1.0f);
        QVERIFY(std::abs(drawn.at(120) + 140.0f) < 1.0f);

        // A pan drag: the axis moves every step while old-window frames keep
        // arriving and presenting an audio delay later. The trace is never
        // empty at any step or between them.
        for (int step = 0; step < 12; ++step) {
            widget->setCenterFrequency(widget->centerFrequency() + 500.0);
            QVERIFY2(!widget->renderedPixels().isEmpty(), qPrintable(QString::number(step)));
            media->deliver(encoder.encode(frameAt(62 + step, coreNow(), -100.0f)));
            for (int wait = 0; wait < 5; ++wait) {
                QTest::qWait(8);
                QVERIFY2(!widget->renderedPixels().isEmpty(),
                         qPrintable(QStringLiteral("step %1 wait %2").arg(step).arg(wait)));
            }
        }
        QTest::qWait(300);
        QVERIFY(!widget->renderedPixels().isEmpty());

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A pan's waterfall row goes to its widget when its frame is pushed,
    // with the time the presenter will present the frame's trace, so the
    // widget's row clock can slide the row in by then; the trace is still
    // drawn at that time. A key forgets the rows handed over before it
    // that are not due yet.
    void panRowsAreHandedOverEarlyWithTheirDueTime()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        // Rows stay queued: neither the ticker nor a frame puts them in.
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        // When each trace is drawn, on the widget's row clock.
        std::vector<qint64> drawnAtNs;
        connect(&controller, &RemoteMediaController::displayFrameReceived, widget,
                [&drawnAtNs, widget] { drawnAtNs.push_back(widget->remoteRowNowNsForTest()); });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(controller.displayClockNegotiated());
        const QString panId = applet->panId();
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
            {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), wideband.toJson()},
            {QStringLiteral("grantedFftSize"), 8192}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);

        DisplayCodecEncoder encoder;
        const auto frameAt = [&](int index, qint64 producerNs, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, 1, -180, 0, 128, 128, 0};
            frame.encoderSequence = quint32(index + 1);
            frame.producerTimestamp = quint64(producerNs);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            frame.waterfallDbm = QVector<float>(128, level);
            return frame;
        };

        // No echo yet: a first frame draws on arrival, and its row, with no
        // due time, is queued at once.
        media->deliver(encoder.encode(frameAt(0, 1'000'000'000, -100.0f)));
        QCOMPARE(int(drawnAtNs.size()), 1);
        const int queuedBefore = widget->remoteRowQueueDepthForTest();
        QVERIFY(queuedBefore >= 1);

        // Clock echoes from a Core whose clock reads 10 s ahead: frames now
        // wait for their Core time plus the audio's delay.
        constexpr qint64 kCoreAheadNs = 10'000'000'000;
        QTRY_VERIFY_WITH_TIMEOUT(countControl(controls, QStringLiteral("clock-probe")) >= 1, 3000);
        const QJsonObject probe = lastControl(controls, QStringLiteral("clock-probe"));
        QElapsedTimer sinceProbe;
        sinceProbe.start();
        const qint64 t0 = probe.value(QStringLiteral("t0")).toInteger();
        const QJsonObject echo{
            {QStringLiteral("op"), QStringLiteral("clock-echo")},
            {QStringLiteral("connectionId"), probe.value(QStringLiteral("connectionId"))},
            {QStringLiteral("id"), probe.value(QStringLiteral("id"))},
            {QStringLiteral("t0"), t0},
            {QStringLiteral("t1"), t0 + kCoreAheadNs},
            {QStringLiteral("t2"), t0 + kCoreAheadNs},
            {QStringLiteral("generation"), 0},
            {QStringLiteral("rtpTimestamp"), 0},
            {QStringLiteral("capturedNs"), 0}};
        QVERIFY(server.sendMediaControl(echo, server.mediaSessionEpoch()));
        QTest::qWait(30);
        const auto coreNow = [&] { return t0 + kCoreAheadNs + sinceProbe.nsecsElapsed(); };

        // The row is in the widget's queue the moment its frame is pushed,
        // before its trace is drawn, due when the trace will be.
        const qint64 handedAtNs = widget->remoteRowNowNsForTest();
        media->deliver(encoder.encode(frameAt(1, coreNow(), -90.0f)));
        QCOMPARE(int(drawnAtNs.size()), 1);
        QCOMPARE(widget->remoteRowQueueDepthForTest(), queuedBefore + 1);
        const qint64 dueNs = widget->remoteRowDueNsForTest(queuedBefore);
        // An audio delay on: the base delay is the one-way trip plus the
        // audio's 80 ms base hold.
        QVERIFY2(dueNs - handedAtNs >= 60'000'000,
                 qPrintable(QString::number(dueNs - handedAtNs)));
        QTRY_COMPARE_WITH_TIMEOUT(int(drawnAtNs.size()), 2, 2000);
        // The trace is drawn at that time: never before it, and within the
        // presentation timer's millisecond and an event-loop turn after.
        QVERIFY2(drawnAtNs[1] >= dueNs - 1'000'000,
                 qPrintable(QString::number(drawnAtNs[1] - dueNs)));
        QVERIFY2(drawnAtNs[1] < dueNs + 50'000'000,
                 qPrintable(QString::number(drawnAtNs[1] - dueNs)));
        // Its row was not queued a second time at the trace.
        QCOMPARE(widget->remoteRowQueueDepthForTest(), queuedBefore + 1);

        // A key forgets the rows handed over before it that are not due
        // yet: their slots would fall in the over. The rows already due
        // stay. The next frame comes some periods after the last, so rows
        // blended across the gap come with it, some of them already due.
        media->deliver(encoder.encode(frameAt(2, coreNow(), -80.0f)));
        const int queuedAtKey = widget->remoteRowQueueDepthForTest();
        QVERIFY(queuedAtKey >= queuedBefore + 2);
        const qint64 keyedAtNs = widget->remoteRowNowNsForTest();
        int notYetDue = 0;
        for (int i = queuedBefore + 1; i < queuedAtKey; ++i) {
            if (widget->remoteRowDueNsForTest(i) > keyedAtNs) {
                ++notYetDue;
            }
        }
        QVERIFY(notYetDue >= 1); // the frame's own row
        controller.setPanTransmitting(panId, true, false);
        QCOMPARE(widget->remoteRowQueueDepthForTest(), queuedAtKey - notYetDue);
        QTest::qWait(300);
        QCOMPARE(int(drawnAtNs.size()), 2);
        controller.setPanTransmitting(panId, false, false);

        // A mini display still gets row and trace together, at due: its
        // rows are not handed over early.
        std::vector<qint64> miniAtNs;
        QSignalSpy minis(&controller, &RemoteMediaController::miniDisplayFrame);
        connect(&controller, &RemoteMediaController::miniDisplayFrame, widget,
                [&miniAtNs, widget] { miniAtNs.push_back(widget->remoteRowNowNsForTest()); });
        controller.setMiniDisplaySlices({stationSlice->sliceIndex()});
        const auto miniAsk = [&] {
            for (const QJsonObject& ask : controlsFor(controls, QStringLiteral("subscribe"))) {
                if (ask.value(QStringLiteral("displayRole")).toString() == QLatin1String("mini")) {
                    return ask;
                }
            }
            return QJsonObject{};
        };
        QTRY_VERIFY(!miniAsk().isEmpty());
        const QJsonObject ask = miniAsk();
        const quint32 miniId = quint32(ask.value(QStringLiteral("endpointId")).toDouble());
        QVERIFY(miniId != id);
        // The Core's crop of the asked window, as the controller checks it.
        const int fftSize = ask.value(QStringLiteral("fftSize")).toInt();
        ReducerConfig crop;
        crop.centreHz = ask.value(QStringLiteral("centreHz")).toDouble();
        crop.spanHz = ask.value(QStringLiteral("spanHz")).toDouble();
        crop.streamCentreHz = 14225000;
        crop.sampleRateHz = 192000;
        const auto bins = SpectrumReducer::visibleBinRange(fftSize, crop);
        const double binHz = 192000.0 / fftSize;
        QJsonObject miniContext = context;
        miniContext.insert(QStringLiteral("endpointId"), double(miniId));
        miniContext.insert(QStringLiteral("revision"), ask.value(QStringLiteral("revision")));
        miniContext.insert(QStringLiteral("centreHz"),
                           14225000.0 - 96000.0 + (bins.first + bins.second + 1) * binHz * 0.5);
        miniContext.insert(QStringLiteral("spanHz"), (bins.second - bins.first + 1) * binHz);
        miniContext.insert(QStringLiteral("grantedFftSize"), fftSize);
        miniContext.insert(QStringLiteral("grantedTier"), ask.value(QStringLiteral("tier")));
        // A mini display asks for no extended view: no wideband block.
        miniContext.remove(QStringLiteral("wideband"));
        QVERIFY(server.sendMediaControl(miniContext, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 2);
        DisplayCodecEncoder miniEncoder;
        const qint64 producerNs = coreNow();
        const int depthBefore = widget->remoteRowQueueDepthForTest();
        media->deliver(encoder.encode(frameAt(3, producerNs, -70.0f)));
        DisplayCodecFrame miniFrame = frameAt(0, producerNs, -70.0f);
        miniFrame.context = {miniId, 1, -180, 0, 128, 128, 0};
        media->deliver(miniEncoder.encode(miniFrame));
        // The pan's row is handed over; the mini display's waits for due.
        QVERIFY(widget->remoteRowQueueDepthForTest() > depthBefore);
        const qint64 miniDueNs =
            widget->remoteRowDueNsForTest(widget->remoteRowQueueDepthForTest() - 1);
        QCOMPARE(minis.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(minis.size(), 1, 2000);
        QVERIFY2(miniAtNs.front() >= miniDueNs - 1'000'000,
                 qPrintable(QString::number(miniAtNs.front() - miniDueNs)));
        QVERIFY2(miniAtNs.front() < miniDueNs + 50'000'000,
                 qPrintable(QString::number(miniAtNs.front() - miniDueNs)));
        // Trace and row in the one call, the row advancing the waterfall.
        const QList<QVariant> shown = minis.first();
        const auto isFrame = [](const QVector<float>& plane) {
            return plane.size() == 128
                && std::all_of(plane.cbegin(), plane.cend(),
                               [](float dbm) { return std::abs(dbm + 70.0f) < 0.5f; });
        };
        QVERIFY(isFrame(shown.at(1).value<QVector<float>>()));
        QVERIFY(isFrame(shown.at(2).value<QVector<float>>()));
        QVERIFY(shown.at(6).toBool());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Merge of parity Tasks 28-29 (the transmit display) with R-R3-21 (the
    // display on the audio's clock): the checks the display-sync review set
    // for it, one part each.
    //  1. Transmit frames bypass the presenter: handed on at once while the
    //     clock map holds receive frames back, and no receive row moves.
    //  2. The keying hold at presentation time: a receive frame waiting for
    //     its time when the pan keys is never drawn.
    //  3. The unkey restarts the blend chain: no rows are invented across an
    //     over shorter than the presenter's 64-row gap.
    //  4. The generation: a receive frame queued under the receive context
    //     is not drawn once the Core's transmit context supersedes it.
    //  6. The widget's row queue: receive rows queued when the transmit
    //     waterfall takes over are cleared, and none plays out late.
    void transmitDisplayBypassesTheAudioClockPresenter()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        // A TX analyzer: this Core offers the transmit display.
        TxAnalyzer txAnalyzer(TxAnalyzer::kTxDispId);
        station.setTxAnalyzer(&txAnalyzer);
        const auto clearAnalyzer = qScopeGuard([&station] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        QSignalSpy transmitFrames(&controller, &RemoteMediaController::transmitFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(controller.displayClockNegotiated());
        QVERIFY(controller.txDisplayNegotiated());
        const QString panId = applet->panId();
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const auto contextFor = [&](int generation, bool transmit) {
            return QJsonObject{
                {QStringLiteral("op"), QStringLiteral("context")},
                {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
                {QStringLiteral("endpointId"), double(id)},
                {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
                {QStringLiteral("contextGeneration"), generation}, {QStringLiteral("sourceStream"), 0},
                {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
                {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
                {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
                {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
                {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
                {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
                {QStringLiteral("framesPerLine"), 1},
                {QStringLiteral("wideband"), wideband.toJson()},
                {QStringLiteral("grantedFftSize"), 8192}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
                {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
                {QStringLiteral("limit"), QStringLiteral("none")},
                {QStringLiteral("transmit"), transmit}};
        };
        QVERIFY(server.sendMediaControl(contextFor(1, false), server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);

        constexpr qint64 kPeriodNs = 1'000'000'000 / 30;
        DisplayCodecEncoder encoder;
        const auto frameAt = [&](quint32 generation, int index, qint64 producerNs, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, generation, -180, 0, 128, 128, 0};
            frame.encoderSequence = quint32(index + 1);
            frame.producerTimestamp = quint64(producerNs);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            frame.waterfallDbm = QVector<float>(128, level);
            return frame;
        };
        const auto drainRows = [&] {
            for (int i = 0; i < 64 && widget->remoteRowQueueDepthForTest() > 0; ++i) {
                widget->tickWaterfallForTest();
            }
        };
        const auto filledRows = [&] {
            const RemoteDisplayTelemetry display = controller.displayTelemetry();
            return display.rowsBlended + display.rowsRepeated;
        };

        // 3. No echo yet, so frames draw on arrival. Two receive rows, then
        //    an over of ten frames (decoded, never drawn), then the unkey.
        const qint64 base = 1'000'000'000;
        for (int i = 0; i < 2; ++i) {
            media->deliver(encoder.encode(frameAt(1, i, base + i * kPeriodNs, -130.0f)));
            widget->tickWaterfallForTest();
        }
        QCOMPARE(frames.count(), 2);
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(2));
        controller.setPanTransmitting(panId, true, false);
        for (int i = 2; i < 12; ++i) {
            media->deliver(encoder.encode(frameAt(1, i, base + i * kPeriodNs, -60.0f)));
            widget->tickWaterfallForTest();
        }
        QCOMPARE(frames.count(), 2);
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(2));
        controller.setPanTransmitting(panId, false, false);
        media->deliver(encoder.encode(frameAt(1, 12, base + 12 * kPeriodNs, -120.0f)));
        drainRows();
        QCOMPARE(frames.count(), 3);
        // Only the frame itself: the ten slots of the over are not blended
        // from the row before the key.
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(3));
        QCOMPARE(filledRows(), quint64(0));

        // 6. Receive rows queued when the transmit waterfall takes over are
        //    cleared on the next tick; none is drawn late.
        for (int i = 13; i < 16; ++i) {
            media->deliver(encoder.encode(frameAt(1, i, base + i * kPeriodNs, -110.0f)));
        }
        QCOMPARE(widget->remoteRowQueueDepthForTest(), 3);
        widget->setTxExternalWaterfall(true);
        widget->tickWaterfallForTest();
        QCOMPARE(widget->remoteRowQueueDepthForTest(), 0);
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(3));
        widget->setTxExternalWaterfall(false);
        widget->tickWaterfallForTest();
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(3));

        // Clock echoes from a Core whose clock reads 10 s ahead: receive
        // frames now wait for their Core time plus the audio's delay.
        constexpr qint64 kCoreAheadNs = 10'000'000'000;
        QTRY_VERIFY_WITH_TIMEOUT(countControl(controls, QStringLiteral("clock-probe")) >= 1, 3000);
        const QJsonObject probe = lastControl(controls, QStringLiteral("clock-probe"));
        QElapsedTimer sinceProbe;
        sinceProbe.start();
        const qint64 t0 = probe.value(QStringLiteral("t0")).toInteger();
        const QJsonObject echo{
            {QStringLiteral("op"), QStringLiteral("clock-echo")},
            {QStringLiteral("connectionId"), probe.value(QStringLiteral("connectionId"))},
            {QStringLiteral("id"), probe.value(QStringLiteral("id"))},
            {QStringLiteral("t0"), t0},
            {QStringLiteral("t1"), t0 + kCoreAheadNs},
            {QStringLiteral("t2"), t0 + kCoreAheadNs},
            {QStringLiteral("generation"), 0},
            {QStringLiteral("rtpTimestamp"), 0},
            {QStringLiteral("capturedNs"), 0}};
        QVERIFY(server.sendMediaControl(echo, server.mediaSessionEpoch()));
        QTest::qWait(30);
        const auto coreNow = [&] { return t0 + kCoreAheadNs + sinceProbe.nsecsElapsed(); };
        encoder.reset();
        const quint64 rowsBefore = widget->remoteRowsPushedForTest();

        // The clock is in force: a receive frame waits, then draws.
        int framesBefore = int(frames.count());
        media->deliver(encoder.encode(frameAt(1, 16, coreNow(), -100.0f)));
        QCOMPARE(int(frames.count()), framesBefore);
        QTRY_COMPARE_WITH_TIMEOUT(int(frames.count()), framesBefore + 1, 2000);
        drainRows();

        // 2. A receive frame waiting for its time when the pan keys.
        framesBefore = int(frames.count());
        const quint64 fillsBeforeDelivery = filledRows();
        media->deliver(encoder.encode(frameAt(1, 17, coreNow() + 3 * kPeriodNs, -90.0f)));
        QCOMPARE(int(frames.count()), framesBefore);
        QTRY_VERIFY_WITH_TIMEOUT(widget->remoteRowQueueDepthForTest() > 0, 2000);
        QCOMPARE(int(frames.count()), framesBefore);
        qInfo("receive key boundary: pushed=%llu queued=%d fillsBefore=%llu fillsAfter=%llu",
              static_cast<unsigned long long>(widget->remoteRowsPushedForTest()),
              widget->remoteRowQueueDepthForTest(),
              static_cast<unsigned long long>(fillsBeforeDelivery),
              static_cast<unsigned long long>(filledRows()));
        QVERIFY(filledRows() > fillsBeforeDelivery);
        // The presenter has handed out a due blend before the key. Drain
        // that receive row before measuring the actual key boundary; the
        // future real frame and further fills must stay suppressed after it.
        drainRows();
        const quint64 rowsAtKey = widget->remoteRowsPushedForTest();
        const quint64 fillsAtKey = filledRows();
        controller.setPanTransmitting(panId, true, false);
        QTest::qWait(400);
        drainRows();
        QCOMPARE(int(frames.count()), framesBefore);
        QCOMPARE(widget->remoteRowsPushedForTest(), rowsAtKey);
        QCOMPARE(filledRows(), fillsAtKey);
        controller.setPanTransmitting(panId, false, false);

        // 4 and 1. A receive frame waits; the Core's transmit context then
        //    supersedes the receive one. Transmit frames are handed on at
        //    once (no wait for the audio's clock) and the waiting receive
        //    frame is never drawn.
        media->deliver(encoder.encode(frameAt(1, 18, coreNow(), -80.0f)));
        framesBefore = int(frames.count());
        const quint64 filledBefore = filledRows();
        const quint64 rowsAtTransmit = widget->remoteRowsPushedForTest();
        QVERIFY(server.sendMediaControl(contextFor(2, true), server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 2);
        DisplayCodecEncoder transmitEncoder;
        for (int i = 0; i < 3; ++i) {
            media->deliver(transmitEncoder.encode(frameAt(2, i, coreNow(), -30.0f)));
            // Handed on in the same turn, not an audio delay later.
            QCOMPARE(transmitFrames.count(), i + 1);
        }
        QTest::qWait(400);
        drainRows();
        // The transmit frames count as display frames; the receive frame
        // that waited does not.
        QCOMPARE(int(frames.count()), framesBefore + 3);
        QCOMPARE(widget->remoteRowsPushedForTest(), rowsAtTransmit);
        QCOMPARE(filledRows(), filledBefore);
        QVERIFY(rowsAtTransmit >= rowsBefore);

        QVERIFY(!controller.micLineOpen());
        QVERIFY(!remote.audioEngine()->hasCaptureDemand());
        QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // NSDC v2 (display codec v2 document, sections 7 and 9.3): a window
    // whose Core offers displayCodecVersion 2 asks for it in its start and
    // then asks no keyframe for a receive context, a re-grant or a transmit
    // context; a window that withholds the feature asks one for each, as
    // before. Either way a binding that has decoded a v2 frame asks none for
    // its next context, and a v1 frame's NeedKeyframe still asks. A held part
    // presents nothing; a decode that releases two frames presents both, in
    // order, to the receive presenter or the transmit display; the decoders'
    // lost frames and parts reach the display telemetry, and no loss waits
    // for a keyframe.
    void displayCodecV2StartSkipsContextKeyframes_data()
    {
        QTest::addColumn<bool>("agreed");
        QTest::newRow("Core offers v2") << true;
        QTest::newRow("window withholds displayCodec") << false;
    }

    void displayCodecV2StartSkipsContextKeyframes()
    {
        QFETCH(bool, agreed);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        // A TX analyzer: this Core offers the transmit display.
        TxAnalyzer txAnalyzer(TxAnalyzer::kTxDispId);
        station.setTxAnalyzer(&txAnalyzer);
        const auto clearAnalyzer = qScopeGuard([&station] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        // The contexts this test sends by hand state no picture.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        if (!agreed) { client.withholdFeatureForTest(QByteArrayLiteral("displayCodec")); }
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        QSignalSpy transmitFrames(&controller, &RemoteMediaController::transmitFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(controller.txDisplayNegotiated());

        // The start asks for v2 exactly when the Core offers it.
        QCOMPARE(client.capabilities().displayCodecVersion, agreed ? 2 : 0);
        QCOMPARE(countControl(controls, QStringLiteral("start")), 1);
        const QJsonObject start = lastControl(controls, QStringLiteral("start"));
        QCOMPARE(start.contains(QStringLiteral("displayCodecVersion")), agreed);
        if (agreed) {
            QCOMPARE(start.value(QStringLiteral("displayCodecVersion")).toInt(), 2);
        }

        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        // 512 pixels of noise do not fit one SCTP DATA chunk: v2 splits them.
        constexpr int kPixels = 512;
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const auto contextFor = [&](int generation, bool transmit) {
            return QJsonObject{
                {QStringLiteral("op"), QStringLiteral("context")},
                {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
                {QStringLiteral("endpointId"), double(id)},
                {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
                {QStringLiteral("contextGeneration"), generation}, {QStringLiteral("sourceStream"), 0},
                {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
                {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
                {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
                {QStringLiteral("traceSamples"), kPixels}, {QStringLiteral("waterfallSamples"), kPixels},
                {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
                {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
                {QStringLiteral("framesPerLine"), 1},
                {QStringLiteral("wideband"), wideband.toJson()},
                {QStringLiteral("grantedFftSize"), 8192}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
                {QStringLiteral("requestedPixels"), kPixels}, {QStringLiteral("grantedPixels"), kPixels},
                {QStringLiteral("limit"), QStringLiteral("none")},
                {QStringLiteral("transmit"), transmit}};
        };
        // Sent, and read by the window.
        const auto deliverContext = [&](const QJsonObject& context) {
            const int before = countControl(receivedControls, QStringLiteral("context"));
            return server.sendMediaControl(context, server.mediaSessionEpoch())
                && QTest::qWaitFor([&] {
                       return countControl(receivedControls, QStringLiteral("context")) > before;
                   }, 5'000);
        };
        const auto keyframes = [&] { return countControl(controls, QStringLiteral("keyframe")); };
        quint32 noiseState = 1;
        const auto noise = [&noiseState] {
            QVector<float> values(kPixels);
            for (float& value : values) {
                noiseState = noiseState * 1664525U + 1013904223U;
                value = -180.0f + 180.0f * float(noiseState >> 8) / float(1U << 24);
            }
            return values;
        };
        constexpr qint64 kPeriodNs = 1'000'000'000 / 30;
        const qint64 base = 1'000'000'000;
        const auto frameOf = [&](quint32 generation, quint32 sequence, bool advance) {
            DisplayCodecFrame frame;
            frame.context = {id, generation, -180, 0, kPixels, kPixels, 0};
            frame.encoderSequence = sequence;
            frame.producerTimestamp = quint64(base + qint64(sequence) * kPeriodNs);
            frame.waterfallAdvance = advance;
            frame.traceDbm = noise();
            frame.waterfallDbm = noise();
            return frame;
        };
        const auto v2 = [](const DisplayCodecFrame& frame) {
            return encodeDisplayCodecV2(frame, DisplayCodecEncoder::kMaxEncodedBytes).messages;
        };

        // A receive context, a re-grant (a newer context for the same
        // request) and a transmit context. Each wait is also past the
        // window's 200 ms keyframe request spacing.
        QVERIFY(deliverContext(contextFor(1, false)));
        QTRY_COMPARE(keyframes(), agreed ? 0 : 1);
        QTest::qWait(250);
        QCOMPARE(keyframes(), agreed ? 0 : 1);
        QVERIFY(deliverContext(contextFor(2, false)));
        QTRY_COMPARE(keyframes(), agreed ? 0 : 2);
        QTest::qWait(250);
        QCOMPARE(keyframes(), agreed ? 0 : 2);
        QVERIFY(deliverContext(contextFor(3, true)));
        QTRY_COMPARE(keyframes(), agreed ? 0 : 3);
        QTest::qWait(250);
        QCOMPARE(keyframes(), agreed ? 0 : 3);
        const int contextKeyframes = keyframes();

        // The transmit display: a held part hands on nothing; each frame a
        // decode releases is handed on, in order.
        const std::vector<QByteArray> t1 = v2(frameOf(3, 1, true));
        const std::vector<QByteArray> t2 = v2(frameOf(3, 2, true));
        const std::vector<QByteArray> t3 = v2(frameOf(3, 3, true));
        const std::vector<QByteArray> t4 = v2(frameOf(3, 4, false));
        QCOMPARE(t1.size(), size_t(2));
        QCOMPARE(t4.size(), size_t(1));
        media->deliver(t1.front()); // its trace part is lost
        QCOMPARE(transmitFrames.count(), 0);
        media->deliver(t2.front()); // releases t1 without its trace; t2 held
        QCOMPARE(transmitFrames.count(), 1);
        media->deliver(t2.back());
        QCOMPARE(transmitFrames.count(), 2);
        media->deliver(t3.front()); // its trace part is lost; held
        QCOMPARE(transmitFrames.count(), 2);
        media->deliver(t4.front()); // releases t3 and t4 at once
        QCOMPARE(transmitFrames.count(), 4);
        const QList<quint32> handedOn{
            transmitFrames.at(0).at(1).value<DisplayCodecFrame>().encoderSequence,
            transmitFrames.at(1).at(1).value<DisplayCodecFrame>().encoderSequence,
            transmitFrames.at(2).at(1).value<DisplayCodecFrame>().encoderSequence,
            transmitFrames.at(3).at(1).value<DisplayCodecFrame>().encoderSequence};
        QCOMPARE(handedOn, (QList<quint32>{1, 2, 3, 4}));
        QVERIFY(transmitFrames.at(0).at(1).value<DisplayCodecFrame>().traceCarried);
        QVERIFY(transmitFrames.at(2).at(1).value<DisplayCodecFrame>().traceCarried);
        QVERIFY(!transmitFrames.at(3).at(1).value<DisplayCodecFrame>().traceCarried);
        RemoteDisplayTelemetry display = controller.displayTelemetry();
        QCOMPARE(display.partsLost, quint64(2));
        QCOMPARE(display.messagesLost, quint64(0));
        QCOMPARE(keyframes(), contextKeyframes);

        // Back to receive: this binding has decoded v2, so no keyframe is
        // asked for its context, agreed or not.
        QVERIFY(deliverContext(contextFor(4, false)));
        QTest::qWait(250);
        QCOMPARE(keyframes(), contextKeyframes);

        // Receive frames: held parts present nothing; a lost trace part, a
        // lost frame and a release of two frames present every frame
        // released, and nothing waits for a keyframe.
        const std::vector<QByteArray> r1 = v2(frameOf(4, 1, true));
        const std::vector<QByteArray> r2 = v2(frameOf(4, 2, true));
        const std::vector<QByteArray> r3 = v2(frameOf(4, 3, true));
        const std::vector<QByteArray> r5 = v2(frameOf(4, 5, true)); // frame 4 is lost
        QCOMPARE(r1.size(), size_t(2));
        const qsizetype shownBefore = frames.count();
        media->deliver(r1.front());
        QTest::qWait(50);
        QCOMPARE(frames.count(), shownBefore);
        media->deliver(r1.back());
        QTRY_COMPARE(frames.count(), shownBefore + 1);
        media->deliver(r2.front()); // its trace part is lost
        media->deliver(r3.front()); // releases r2 without its trace; r3 held
        QTRY_COMPARE(frames.count(), shownBefore + 2);
        media->deliver(r3.back());
        QTRY_COMPARE(frames.count(), shownBefore + 3);
        media->deliver(r5.front());
        media->deliver(r5.back());
        QTRY_COMPARE(frames.count(), shownBefore + 4);
        display = controller.displayTelemetry();
        QCOMPARE(display.partsLost, quint64(3));
        QCOMPARE(display.messagesLost, quint64(1));
        QCOMPARE(display.keyframeWaits, quint64(0));
        QCOMPARE(keyframes(), contextKeyframes);

        // A v1 frame this binding cannot decode still asks for a keyframe.
        DisplayCodecEncoder encoder;
        QVERIFY(!encoder.encode(frameOf(4, 10, true)).isEmpty());
        const QByteArray delta = encoder.encode(frameOf(4, 11, true));
        QVERIFY(!delta.isEmpty());
        QVERIFY(!encoder.lastEncodedKeyframe());
        media->deliver(delta);
        QTRY_COMPARE(keyframes(), contextKeyframes + 1);
        QCOMPARE(controller.displayTelemetry().keyframeWaits, quint64(1));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-21 / R-R3-08: a Core that omits displayClockVersion (an older
    // one): each frame is drawn on arrival, and the waterfall's row queue
    // and gap filling still apply, so a late burst still plays out row by
    // row instead of collapsing.
    void olderCoreDrawsOnArrivalAndKeepsTheRowQueue()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // NSDC v2: this test's frames are v1 and it counts the keyframes a
        // context asks for, which a window that agreed v2 does not ask.
        client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setWfLowThreshold(-140.0f);
        widget->setWfHighThreshold(-70.0f);
        // Fixed levels: runtime ones would widen the dBm window as rows
        // arrive and renew the subscription mid-test.
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setWfUpdatePeriodMs(20);
        widget->setWaterfallTickerPausedForTest(true);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy frames(&controller, &RemoteMediaController::displayFrameReceived);
        auto* stationLink = new OlderDisplayClockTransport();
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(stationLink->stripped > 0);
        QCOMPARE(client.capabilities().displayClockVersion, 0);
        QVERIFY(controller.audioClockNegotiated());
        QVERIFY(!controller.displayClockNegotiated());
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        WidebandDisplayContext wideband;
        wideband.available = true;
        wideband.physicalAdcIndex = 0;
        wideband.filterChainIndex = 0;
        wideband.adcRateHz = 4000000;
        const QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
            {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 30},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), wideband.toJson()},
            {QStringLiteral("grantedFftSize"), 8192}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        QTRY_COMPARE(countControl(controls, QStringLiteral("keyframe")), 1);
        const int keyframesAtStart = 1;

        DisplayCodecEncoder encoder;
        const auto frameAt = [&](int index, qint64 producerNs, float level) {
            DisplayCodecFrame frame;
            frame.context = {id, 1, -180, 0, 128, 128, 0};
            frame.encoderSequence = quint32(index + 1);
            frame.producerTimestamp = quint64(producerNs);
            frame.waterfallAdvance = true;
            frame.traceDbm = QVector<float>(128, level);
            frame.waterfallDbm = QVector<float>(128, level);
            return frame;
        };
        constexpr qint64 kPeriodNs = 1'000'000'000 / 30;
        for (int i = 0; i < 8; ++i) {
            const int before = int(frames.count());
            media->deliver(encoder.encode(frameAt(i, 1'000'000'000 + i * kPeriodNs, -100.0f)));
            QCOMPARE(int(frames.count()), before + 1); // drawn on arrival
        }
        QCOMPARE(widget->remoteRowQueueDepthForTest(), 8);
        for (int i = 0; i < 8; ++i) { widget->tickWaterfallForTest(); }
        QCOMPARE(widget->remoteRowsPushedForTest(), quint64(8));
        QVERIFY(!controller.displayTelemetry().displayDelayMs.has_value());
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-01/08: while Core reports a limited grant the pan shows one plain
    // line; nothing for an unlimited grant, and the line goes with the
    // endpoint.
    void limitedGrantShowsOnePlainPanStatusLine()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(controller.spectrumGrantNegotiated());
        QVERIFY(!client.remoteDisplayBudgetLimits().has_value());
        const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
        const quint32 id = quint32(subscription.value(QStringLiteral("endpointId")).toDouble());
        QVERIFY(applet->remoteDisplayStatus().isEmpty());

        SpectrumContextMessage message;
        message.connectionId = subscription.value(QStringLiteral("connectionId")).toString();
        message.endpointId = id;
        message.revision = quint32(subscription.value(QStringLiteral("revision")).toDouble());
        message.sourceStream = 0;
        message.sourceCentreHz = 14225000;
        message.sampleRateHz = 192000;
        // SpectrumEndpoint's bin-aligned crop of this window (see
        // contextMediaAndRetirementStayInAuthenticatedSession).
        message.centreHz = 14225023.4375;
        message.spanHz = 24046.875;
        message.traceSamples = 128;
        message.waterfallSamples = 128;
        message.minDbm = -180;
        message.maxDbm = 0;
        message.fps = 30;
        message.framesPerLine = 1;
        message.wideband = WidebandDisplayContext{};
        const auto sendGrant = [&](SpectrumLimitReason limit, bool grantShape = true) {
            ++message.contextGeneration;
            SpectrumContextGrant grant;
            grant.grantedFftSize = 4096;
            grant.requestedPixels = 600;
            grant.grantedPixels = 128;
            grant.limit = limit;
            message.grant = grant;
            const int before = countControl(receivedControls, QStringLiteral("context"));
            QVERIFY(server.sendMediaControl(encodeRemoteSpectrumContext(message, grantShape),
                                            server.mediaSessionEpoch()));
            QTRY_COMPARE(countControl(receivedControls, QStringLiteral("context")), before + 1);
        };
        const QString sourceBins = QStringLiteral("Showing 128 points");
        const QString shared = QStringLiteral("Less detail: shared");
        const QString largest = QStringLiteral("Finest detail reached");

        sendGrant(SpectrumLimitReason::SourceBins);
        QTRY_COMPARE(applet->remoteDisplayStatus(), sourceBins);
        QVERIFY(OperatorWording::isPlain(applet->remoteDisplayExplanation()));
        sendGrant(SpectrumLimitReason::SharedEngine);
        QTRY_COMPARE(applet->remoteDisplayStatus(), shared);
        sendGrant(SpectrumLimitReason::LargestSize);
        QTRY_COMPARE(applet->remoteDisplayStatus(), largest);
        // No longer limited: nothing is shown, and the periodic refresh
        // does not bring an old line back.
        sendGrant(SpectrumLimitReason::None);
        QTRY_VERIFY(applet->remoteDisplayStatus().isEmpty());
        QTest::qWait(250);
        QVERIFY(applet->remoteDisplayStatus().isEmpty());
        sendGrant(SpectrumLimitReason::SharedEngine);
        QTRY_COMPARE(applet->remoteDisplayStatus(), shared);
        // A context without the grant is refused, so it cannot clear the line.
        sendGrant(SpectrumLimitReason::None, false);
        QTest::qWait(250);
        QCOMPARE(applet->remoteDisplayStatus(), shared);

        // The endpoint goes away with the session; so does its line.
        client.disconnectFromStation(QStringLiteral("test complete"));
        QTRY_VERIFY2(applet->remoteDisplayStatus().isEmpty(),
                     qPrintable(applet->remoteDisplayStatus()));
        QVERIFY(!media || !media->active);
    }

    // Parity Task 17 (B3.3, B3.7, B3.10): a pan asks the Core for a dBm
    // window that is what it shows (its own range widened by the
    // waterfall's levels), so a signal above 0 dBm is not flattened and a
    // 20 dB pan steps by less than 0.1 dB; a range change asks again once
    // it has held for a frame period, never at every step of a drag; the
    // averaging constants are for the rate asked; Rendering > Decimation
    // goes with the request to a Core at spectrumGrantVersion 2.
    void subscribeCarriesThePansDbmWindowAveragingAndDecimation()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("20"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        // Merge of R-DSP-59 with R-DISP-HOLD (2026-10-11): the pan's FFT
        // size no longer follows its width in pixels, so the stored size
        // sets the bin width here. FFT sizing mode (2026-10-11): it does so
        // in the Fixed mode only, and exactly. 4096 bins at a 192 kHz
        // receiver are 46.9 Hz wide, where normalise moves the pan by
        // 16.7 dB: more than the 10 dB of headroom a held range has, so the
        // normalise step below has to ask again. In Auto, the 24 kHz span
        // asks 32768 bins (7.7 dB) and the held range would hold it and ask
        // nothing. (This test stored 1024, which the zoom plan of the day
        // raised to 8192 bins; 1024 is not one of the FFT Size control's
        // sizes.)
        const QString sizingModeKey = QLatin1String(ControlRanges::kDisplayFftSizingModeKey);
        const bool hadFftSize = appSettings.contains(QStringLiteral("DisplayFftSize"));
        const QVariant savedFftSize = appSettings.value(QStringLiteral("DisplayFftSize"));
        const bool hadSizingMode = appSettings.contains(sizingModeKey);
        const QVariant savedSizingMode = appSettings.value(sizingModeKey);
        appSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("4096"));
        appSettings.setValue(sizingModeKey,
                             QLatin1String(ControlRanges::kDisplayFftSizingModeFixedToken));
        const auto restoreFftSize = qScopeGuard([&] {
            if (hadFftSize) { appSettings.setValue(QStringLiteral("DisplayFftSize"), savedFftSize); }
            else { appSettings.remove(QStringLiteral("DisplayFftSize")); }
            if (hadSizingMode) { appSettings.setValue(sizingModeKey, savedSizingMode); }
            else { appSettings.remove(sizingModeKey); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        NereusSDR::FFTEngine windowEngine(-1); // Rendering > Decimation keeps its value here.
        windowEngine.setDecimation(4);
        remote.setFftEngine(&windowEngine);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setDispNormalize(false);
        // Normalise applies with the Average, Sample and RMS detectors only
        // (Thetis updateNormalizePan); the normalise step below needs one.
        widget->setSpectrumDetector(SpectrumDetector::Average);
        widget->setWfUseSpectrumMinMax(false);
        // The stored levels colour the waterfall (no AGC, no Clarity); the
        // run-time levels are runtimeWaterfallLevelsWidenTheDbmWindow's.
        widget->setWfAgcEnabled(false);
        widget->setDbmRange(-100.0f, 20.0f); // Ref Level above 0 dBm.
        widget->setWfLowThreshold(-130.0f);
        widget->setWfHighThreshold(-70.0f);
        widget->setSpectrumAverageTimeMs(200);
        widget->setWaterfallAverageTimeMs(500);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 now = 1'000;
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            },
            [&now] { return now; });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QVERIFY(client.spectrumDecimationAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(!client.remoteDisplayBudgetLimits().has_value());

        QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        // The pan shows -100 to +20 dBm and the waterfall -130 to -70: the
        // window spans both, so +20 dBm is not clipped at 0.
        // R-DISP-HOLD: with 10 dB either side (this test held the bare
        // -130 to +20; a range with no headroom is asked again at every
        // 2 dB move of the grid, and each ask restarted the picture).
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -140.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), 30.0);
        // The averaging times at the 20 frames a second asked.
        QCOMPARE(asked.value(QStringLiteral("fps")).toInt(), 20);
        QCOMPARE(asked.value(QStringLiteral("trace")).toObject()
                     .value(QStringLiteral("averageAlpha")).toDouble(),
                 double(averageAlphaForTimeMs(200, 20)));
        QCOMPARE(asked.value(QStringLiteral("waterfall")).toObject()
                     .value(QStringLiteral("averageAlpha")).toDouble(),
                 double(averageAlphaForTimeMs(500, 20)));
        QCOMPARE(asked.value(QStringLiteral("decimation")).toInt(), 4);

        // Rendering > Decimation changed: asked again with it.
        windowEngine.setDecimation(8);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        QCOMPARE(lastControl(controls, QStringLiteral("subscribe"))
                     .value(QStringLiteral("decimation")).toInt(), 8);

        // A drag of the dBm strip: a new range at every planner tick, the
        // clock never a frame period (50 ms at 20 frames a second) past the
        // last step. Nothing is asked while it moves.
        widget->setWfLowThreshold(-96.0f);
        widget->setWfHighThreshold(-78.0f);
        for (int step = 0; step < 5; ++step) {
            widget->setDbmRange(-100.0f + float(step), -80.0f + float(step));
            now += 10;
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        }
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        // The last range holds for a frame period: asked once, for it.
        now += 60;
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        const double low = asked.value(QStringLiteral("minDbm")).toDouble();
        const double high = asked.value(QStringLiteral("maxDbm")).toDouble();
        // R-DISP-HOLD: the pan now needs -96 to -76 dBm, far inside the
        // -140 to +30 held, so the range is drawn in to it with the headroom
        // (this test held the bare -96 to -76 and steps under 0.1 dB).
        QCOMPARE(low, -106.0);
        QCOMPARE(high, -66.0);
        // A 20 dB range with its headroom in 256 levels: steps under 0.2 dB,
        // the cost the hold rule states for not restarting the picture.
        QVERIFY((high - low) / 255.0 < 0.2);
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs * 2);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);

        // Normalise moves what the pan shows by -10 log10(bin width): the
        // window follows, in the frame's own (un-normalised) values.
        widget->setDispNormalize(true);
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        now += 60;
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 4);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        SliceModel* mirrored = remote.sliceById(stationSlice->sliceIndex());
        QVERIFY(mirrored);
        const double binWidth = double(mirrored->sampleRateHz())
            / asked.value(QStringLiteral("fftSize")).toDouble();
        const double shift = -10.0 * std::log10(binWidth);
        // R-DISP-HOLD: on whole dB with the headroom (was a tenth of a dB
        // and none).
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(),
                 std::ceil(-76.0 - shift + 10.0));
        // The waterfall's stored -96 dBm low is a frame value already and
        // does not move with normalise: it stays the bottom of what is needed.
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -106.0);

        client.disconnectFromStation(QStringLiteral("test complete"));
        remote.setFftEngine(nullptr);
    }

    // A remote pan asks the size a local pan runs, in each sizing mode
    // (design D38, D42; R-DSP-59, R-DSP-66). In Auto and Hz/bin a deep zoom
    // (or the target) grows the FFT no further than about 0.7 s per
    // transform (ControlRanges::autoZoomMaxFftSize), 131072 at the fixture's
    // 192 kHz, not the engine's 262144, and the stored size plays no part.
    // In Fixed the stored size is asked for exactly, past the cap too, on
    // the Wide tier whatever the pan shows.  2026-10-09: the capped rows
    // asked 65536 under the fixed cap of 2026-05-08.  2026-10-11: before the
    // sizing mode the stored size was a floor, so "300 Hz zoom, stored
    // 131072" asked 131072 on the Wide tier with no mode stored; that is the
    // Fixed row now, and with no mode (Auto) the same size is a zoom's, on
    // the Fine tier.
    void subscribeCapsAZoomedPansFftSizeLikeALocalPan_data()
    {
        QTest::addColumn<double>("spanHz");
        QTest::addColumn<QString>("sizingMode"); // empty: the key is absent
        QTest::addColumn<QString>("storedSize");
        QTest::addColumn<QString>("hzPerBin");
        QTest::addColumn<int>("panWidth");
        QTest::addColumn<int>("fftSize");
        QTest::addColumn<QString>("tier");
        const QString none;
        const QString automatic = QStringLiteral("Auto");
        const QString hzPerBinMode = QStringLiteral("HzPerBin");
        const QString fixed = QStringLiteral("Fixed");
        QTest::newRow("300 Hz zoom, stored 4096") << 300.0 << none << QStringLiteral("4096")
            << QStringLiteral("0") << 600 << 131072 << QStringLiteral("fine");
        QTest::newRow("300 Hz zoom, stored 131072, no mode") << 300.0 << none
            << QStringLiteral("131072") << QStringLiteral("0") << 600 << 131072
            << QStringLiteral("fine");
        QTest::newRow("Hz/bin 0.5, stored 4096, no mode") << 24000.0 << none
            << QStringLiteral("4096") << QStringLiteral("0.5") << 600 << 131072
            << QStringLiteral("fine");
        // 2026-10-10: the size follows the span, not the pan's width in
        // pixels: 4096 x 192000 / 24000 = 32768 at either width. (The
        // per-pixel rule asked 192000 x pixels / 24000 = 8 points a pixel,
        // rounded up: 8192 for any pan 513 to 1024 px wide.)
        QTest::newRow("24 kHz zoom, 600 px wide") << 24000.0 << none << QStringLiteral("4096")
            << QStringLiteral("0") << 600 << 32768 << QStringLiteral("fine");
        QTest::newRow("24 kHz zoom, 1000 px wide") << 24000.0 << none << QStringLiteral("4096")
            << QStringLiteral("0") << 1000 << 32768 << QStringLiteral("fine");
        // Auto: 4096 across the span whatever is stored. 48 kHz of 192 kHz
        // is 16384, though 65536 is stored.
        QTest::newRow("Auto, 48 kHz, stored 65536") << 48000.0 << automatic
            << QStringLiteral("65536") << QStringLiteral("0") << 600 << 16384
            << QStringLiteral("fine");
        QTest::newRow("Auto, full span, stored 65536") << 192000.0 << automatic
            << QStringLiteral("65536") << QStringLiteral("0") << 600 << 4096
            << QStringLiteral("wide");
        // Hz/bin: the target's size at any span. 192000 / 5 = 38400 points,
        // 65536 as a power of two.
        QTest::newRow("Hz/bin 5, 300 Hz zoom") << 300.0 << hzPerBinMode << QStringLiteral("4096")
            << QStringLiteral("5") << 600 << 65536 << QStringLiteral("fine");
        QTest::newRow("Hz/bin 5, full span, stored 262144") << 192000.0 << hzPerBinMode
            << QStringLiteral("262144") << QStringLiteral("5") << 600 << 65536
            << QStringLiteral("fine");
        // Fixed: exactly the stored size, on the Wide tier.
        QTest::newRow("Fixed 4096, 300 Hz zoom") << 300.0 << fixed << QStringLiteral("4096")
            << QStringLiteral("0") << 600 << 4096 << QStringLiteral("wide");
        QTest::newRow("Fixed 16384, full span") << 192000.0 << fixed << QStringLiteral("16384")
            << QStringLiteral("0") << 600 << 16384 << QStringLiteral("wide");
        QTest::newRow("Fixed 131072, 300 Hz zoom") << 300.0 << fixed << QStringLiteral("131072")
            << QStringLiteral("0") << 600 << 131072 << QStringLiteral("wide");
        QTest::newRow("Fixed 262144, 24 kHz zoom") << 24000.0 << fixed
            << QStringLiteral("262144") << QStringLiteral("0") << 600 << 262144
            << QStringLiteral("wide");
    }

    void subscribeCapsAZoomedPansFftSizeLikeALocalPan()
    {
        QFETCH(double, spanHz);
        QFETCH(QString, sizingMode);
        QFETCH(QString, storedSize);
        QFETCH(QString, hzPerBin);
        QFETCH(int, panWidth);
        QFETCH(int, fftSize);
        QFETCH(QString, tier);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const QString sizeKey = QLatin1String(ControlRanges::kDisplayFftSizeKey);
        const QString hzPerBinKey = QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey);
        const QString modeKey = QLatin1String(ControlRanges::kDisplayFftSizingModeKey);
        appSettings.setValue(sizeKey, storedSize);
        appSettings.setValue(hzPerBinKey, hzPerBin);
        appSettings.remove(modeKey);
        if (!sizingMode.isEmpty()) {
            appSettings.setValue(modeKey, sizingMode);
        }
        const auto restore = qScopeGuard([&] {
            appSettings.remove(sizeKey);
            appSettings.remove(hzPerBinKey);
            appSettings.remove(modeKey);
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setExtendedViewAllowed(false);
        widget->setDisplayWindowPreservingHistory(14225000, spanHz);
        stack.resize(panWidth, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_VERIFY(countControl(controls, QStringLiteral("subscribe")) >= 1);

        const QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        SliceModel* mirrored = remote.sliceById(stationSlice->sliceIndex());
        QVERIFY(mirrored);
        // The rate is 192 kHz (cap 131072). The size asked is the shared
        // rule's, from the sizing mode, the stored size, the slice's rate,
        // the pan's span and the stored Hz/bin target; the pan's width is
        // not in it.
        QCOMPARE(mirrored->sampleRateHz(), 192000);
        QVERIFY(asked.value(QStringLiteral("pixels")).toInt() > panWidth / 2);
        // What a local window plans for the same pan, as
        // MainWindow::replanFftForStream calls the planner: the mode, the
        // engine's baseline for it and the target in force.
        const DisplayFftSizing sizing = readDisplayFftSizing(appSettings);
        QCOMPARE(asked.value(QStringLiteral("fftSize")).toInt(),
                 ControlRanges::planDisplayFft(sizing.mode, sizing.baseline(),
                                               double(mirrored->sampleRateHz()), spanHz,
                                               sizing.hzPerBinTarget).size);
        QCOMPARE(asked.value(QStringLiteral("fftSize")).toInt(), fftSize);
        QCOMPARE(asked.value(QStringLiteral("tier")).toString(), tier);
        QCOMPARE(ControlRanges::autoZoomMaxFftSize(mirrored->sampleRateHz()), 131072);

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // FFT sizing mode (design D42; R-DSP-66). A zoom on a remote pan: in
    // Fixed every request carries exactly the stored size on the Wide tier,
    // so the Core's shared engine keeps its size; in Auto the zoom asks a
    // larger size on a tier of its own.
    void aZoomAsksANewSizeInAutoAndNeverInFixed_data()
    {
        QTest::addColumn<QString>("sizingMode");
        QTest::addColumn<int>("zoomedSize");
        QTest::addColumn<QString>("zoomedTier");
        QTest::newRow("Fixed") << QStringLiteral("Fixed") << 16384 << QStringLiteral("wide");
        // 4096 x 192000 / 6000 = 131072.
        QTest::newRow("Auto") << QStringLiteral("Auto") << 131072 << QStringLiteral("fine");
    }

    void aZoomAsksANewSizeInAutoAndNeverInFixed()
    {
        QFETCH(QString, sizingMode);
        QFETCH(int, zoomedSize);
        QFETCH(QString, zoomedTier);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const QString sizeKey = QLatin1String(ControlRanges::kDisplayFftSizeKey);
        const QString hzPerBinKey = QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey);
        const QString modeKey = QLatin1String(ControlRanges::kDisplayFftSizingModeKey);
        appSettings.setValue(sizeKey, QStringLiteral("16384"));
        appSettings.remove(hzPerBinKey);
        appSettings.setValue(modeKey, sizingMode);
        const auto restore = qScopeGuard([&] {
            appSettings.remove(sizeKey);
            appSettings.remove(modeKey);
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setExtendedViewAllowed(false);
        widget->setDisplayWindowPreservingHistory(14225000, 192000.0);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_VERIFY(countControl(controls, QStringLiteral("subscribe")) >= 1);
        // Full span: the size the mode's own control chose, on the Wide tier.
        const int fullSpanSize = sizingMode == QStringLiteral("Fixed") ? 16384 : 4096;
        QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("fftSize")).toInt(), fullSpanSize);
        QCOMPARE(asked.value(QStringLiteral("tier")).toString(), QStringLiteral("wide"));

        // Zoom to 6 kHz, then to 300 Hz.
        widget->setDisplayWindowPreservingHistory(14225000, 6000.0);
        QTRY_COMPARE(lastControl(controls, QStringLiteral("subscribe"))
                         .value(QStringLiteral("spanHz")).toDouble(), 6000.0);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("fftSize")).toInt(), zoomedSize);
        QCOMPARE(asked.value(QStringLiteral("tier")).toString(), zoomedTier);
        widget->setDisplayWindowPreservingHistory(14225000, 300.0);
        QTRY_COMPARE(lastControl(controls, QStringLiteral("subscribe"))
                         .value(QStringLiteral("spanHz")).toDouble(), 300.0);
        if (sizingMode == QStringLiteral("Fixed")) {
            // No request, at any zoom, asked a size other than the stored
            // one or left the Wide tier.
            for (const QJsonObject& request :
                 controlsFor(controls, QStringLiteral("subscribe"))) {
                QCOMPARE(request.value(QStringLiteral("fftSize")).toInt(), 16384);
                QCOMPARE(request.value(QStringLiteral("tier")).toString(),
                         QStringLiteral("wide"));
            }
        }

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Parity Task 17 follow-up (R-R3-01, R-R3-04): with Clarity the
    // waterfall is coloured by the levels it sets at run time,
    // so the dBm window holds those levels (with headroom), not the stored
    // ones: no colour clips at the window's edge and a remote pan colours as
    // a local one does. The levels move a little every line; the window
    // follows only when they leave it or it is far wider than they need.
    void runtimeWaterfallLevelsWidenTheDbmWindow()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFps = appSettings.contains(QStringLiteral("DisplaySpectrumFps"));
        const QVariant savedFps = appSettings.value(QStringLiteral("DisplaySpectrumFps"));
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("20"));
        const auto restoreFps = qScopeGuard([&] {
            if (hadFps) { appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), savedFps); }
            else { appSettings.remove(QStringLiteral("DisplaySpectrumFps")); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setDispNormalize(false);
        widget->setWfUseSpectrumMinMax(false);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setDbmRange(-100.0f, -60.0f);
        widget->setWfLowThreshold(-110.0f);
        widget->setWfHighThreshold(-70.0f);
        remote.setClarityEnabled(true);
        PanClarityRegistry registry(&remote);
        registry.registerPan(QStringLiteral("pan-0"),widget,nullptr);
        registry.bindRemote(QStringLiteral("pan-0"),widget,stationSlice,{0,stationSlice->streamEpoch(),14225000,192000},1,1,1);
        ClarityController* clarity=registry.controllerForPan(QStringLiteral("pan-0"));
        // Clarity drives the levels, below and above what the pan shows.
        emit clarity->waterfallThresholdsChanged(-150.0f, -40.0f);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 now = 1'000;
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            },
            [&now] { return now; });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // Clarity's levels with 10 dB of headroom, not the stored -110..-70.
        QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -160.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -30.0);

        const auto settle = [&] {
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
            now += 60; // past a frame period (50 ms at 20 frames a second)
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        };

        // A few dB of movement inside the window asks nothing.
        emit clarity->waterfallThresholdsChanged(-147.0f, -44.0f);
        settle();
        emit clarity->waterfallThresholdsChanged(-153.0f, -37.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // The low level leaves the window: asked again, with headroom.
        emit clarity->waterfallThresholdsChanged(-175.0f, -37.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -185.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -27.0);

        // The levels close in far inside the window: it narrows to them,
        // never inside what the pan shows.
        emit clarity->waterfallThresholdsChanged(-120.0f, -90.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        // R-DISP-HOLD: the pan's own -60 dBm top takes the headroom too
        // (this test held -60: the scale had none, so each move of the grid
        // asked again).
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -130.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);

        // Back to the stored levels (-110 to -70): what the pan needs is
        // now -110 to -60, which the held -130 to -50 holds, so nothing is
        // asked. (This test held a fourth ask here for the bare -110 to
        // -60; a range that still holds what the pan needs is kept.)
        // (Waterfall AGC's levels come from a Core that offers display
        // extras, as this one does: coreWaterfallAgcAsksNothingAsItSettles.)
        registry.setEnabled(false);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        // The stored levels are the ones in force: moving one past the
        // held range asks for the pan and those levels.
        widget->setWfLowThreshold(-135.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 4);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -145.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // JJ's ruling of 2026-09-28: a remote pan's waterfall AGC and NF-AGC use
    // the levels the Core computes (display extras `waterfallLevels`), as the
    // phone does. The subscribe asks for them; the waterfall colours against
    // the stored levels until the first arrive and then against the Core's;
    // the local follower does not run, so AGC settling asks the Core nothing
    // and never blanks the pan with a new request.
    void waterfallLevelsFromActualRequestMatchLocal_data()
    {
        QTest::addColumn<bool>("agc");
        QTest::addColumn<bool>("nfAgc");
        QTest::addColumn<bool>("bothUi");
        QTest::addColumn<bool>("restored");
        QTest::addColumn<bool>("clarity");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<float>("low1");
        QTest::addColumn<float>("high1");
        QTest::addColumn<float>("low2");
        QTest::addColumn<float>("high2");
        // R-DSP-20: AGC moves only the low edge (12 under the smoothed
        // minimum, -160 then -159); High stays the stored 0.
        QTest::newRow("agc-only") << true << false << false << false << false << QStringLiteral("agc") << -172.0f << 0.0f << -171.0f << 0.0f;
        // S7: NF-AGC is an option under AGC. Alone it leaves the stored
        // levels and asks the Core for none.
        QTest::newRow("nf-only") << false << true << false << false << false << QString() << -180.0f << 0.0f << -180.0f << 0.0f;
        // With AGC: the low is the pan's settled noise floor plus the offset
        // of -5, and the AGC's run level until the floor settles. The first
        // row seeds the floor where the estimate settles on it, the pixel
        // with 3 in 20 of the 128 below it: pixel 19 of -160 + 0.5 i,
        // -150.5, and starts its fast attack (as Thetis starts a display,
        // display.cs:3698-3700 [v2.10.3.15]). The run level starts at the
        // seed's target, -155.5, not at Thetis's restart floor of -150
        // (PR 628). The second row is 20 dB up, with no pixel under -150.5,
        // so in fast attack the estimate climbs 3 dB and the line follows at
        // once, and the run level moves 0.4 of the way from -155.5 to
        // -147.5 - 5: -154.3.
        QTest::newRow("both-normal-ui") << true << true << true << false << false << QStringLiteral("noiseFloorAgc") << -155.5f << 0.0f << -154.3f << 0.0f;
        QTest::newRow("both-restored") << true << true << false << true << false << QStringLiteral("noiseFloorAgc") << -155.5f << 0.0f << -154.3f << 0.0f;
        QTest::newRow("manual") << false << false << false << false << false << QString() << -180.0f << 0.0f << -180.0f << 0.0f;
        QTest::newRow("clarity-both-on") << true << true << true << false << true << QString() << -150.0f << -90.0f << -150.0f << -90.0f;
    }

    // Catch a request mode that makes the Core colour identical rows differently.
    void waterfallLevelsFromActualRequestMatchLocal()
    {
        QFETCH(bool, agc);
        QFETCH(bool, nfAgc);
        QFETCH(bool, bothUi);
        QFETCH(bool, restored);
        QFETCH(bool, clarity);
        QFETCH(QString, mode);
        QFETCH(float, low1);
        QFETCH(float, high1);
        QFETCH(float, low2);
        QFETCH(float, high2);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        AppSettings& appSettings = AppSettings::instance();
        QMap<QString, QVariant> saved;
        for (const QString& key : appSettings.allKeys()) { saved.insert(key, appSettings.value(key)); }
        const auto restoreSettings = qScopeGuard([&] {
            appSettings.clear();
            for (auto it = saved.cbegin(); it != saved.cend(); ++it) { appSettings.setValue(it.key(), it.value()); }
        });
        appSettings.clear();
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("20"));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setDispNormalize(false);
        widget->setWfUseSpectrumMinMax(false);
        widget->setClarityActive(false);
        SpectrumWidget local;
        for (SpectrumWidget* view : {&local, widget}) {
            view->loadSettings();
            view->setDispNormalize(false);
            view->setWfUseSpectrumMinMax(false);
            view->setClarityActive(false);
            view->setDbmRange(-180.0f, 0.0f);
            view->setWfLowThreshold(-180.0f);
            view->setWfHighThreshold(0.0f);
            view->setWaterfallAGCOffsetDb(-5);
            if (bothUi || restored) {
                remote.setSpectrumWidget(view);
                const auto detach = qScopeGuard([&] { remote.setSpectrumWidget(nullptr); });
                {
                    WaterfallDefaultsPage page(&remote);
                    QCheckBox* nf = nullptr;
                    for (QCheckBox* box : page.findChildren<QCheckBox*>()) {
                        if (box->property("nereusSetupId").toString()
                            == QStringLiteral("display.waterfallDefaults.nfAgc")) { nf = box; }
                    }
                    QVERIFY(nf && nf->isEnabled());
                    QVERIFY(view->wfAgcEnabled());
                    if (!nf->isChecked()) { nf->click(); }
                    QVERIFY(view->wfAgcEnabled() && view->waterfallNFAGCEnabled());
                }
                remote.setSpectrumWidget(nullptr);
                if (restored) {
                    view->saveSettings();
                    view->setWfAgcEnabled(false);
                    view->setWaterfallNFAGCEnabled(false);
                    view->loadSettings();
                    QVERIFY(view->wfAgcEnabled() && view->waterfallNFAGCEnabled());
                }
            } else {
                view->setWfAgcEnabled(agc);
                view->setWaterfallNFAGCEnabled(nfAgc);
            }
            view->setClarityActive(clarity);
            if (clarity) { view->setClarityWaterfallThresholds(-150.0f, -90.0f); }
        }
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 now = 1'000;
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = std::make_unique<DisplayTransport>(owner).release();
                return media;
            },
            [&now] { return now; });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = std::make_unique<Test::LoopbackTransport>(QStringLiteral("station")).release();
        auto* clientLink = std::make_unique<Test::LoopbackTransport>(QStringLiteral("client")).release();
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QVERIFY(client.capabilities().displayExtrasVersion >= 1);
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        const QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        DisplayExtrasRequest request;
        QVERIFY(parseDisplayExtrasRequest(asked, request));
        DisplayExtrasProcessor processor(request);
        const quint32 id = quint32(asked.value(QStringLiteral("endpointId")).toDouble());
        QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), asked.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), asked.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -180},
            {QStringLiteral("maxDbm"), 0}, {QStringLiteral("fps"), 20},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), WidebandDisplayContext{}.toJson()},
            {QStringLiteral("grantedFftSize"), 4096}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        const DisplayCodecContext codec{id, 1, -180.0f, 0.0f, 128, 128, 0};
        DisplayExtrasInputs inputs;
        inputs.fps = 20;
        inputs.centreHz = 14225023.4375;
        inputs.spanHz = 24046.875;
        inputs.sliceHz = 14225000;
        inputs.binWidthHz = 192000.0 / 4096.0;
        QVector<DisplayExtrasFrame> computed;
        QVector<std::pair<float, float>> localLevels;
        // The local pan takes each frame as a window does, so its own noise
        // floor (NF-AGC's low level) has seen the rows the Core's has.
        SpectrumEndpointContext localContext;
        localContext.codec = codec;
        localContext.exactCentreHz = 14225023.4375;
        localContext.exactSpanHz = 24046.875;
        local.setDisplayFps(20);
        // The local pan's display clock is the Core's: the noise floor's
        // fast attack, which the first row starts, runs on the same time.
        qint64 localNowMs = 1000;
        local.setDisplayClockForTest([&localNowMs] { return localNowMs; });
        // These rows are the Core's trace as sent, and nothing averages them
        // here: the pan asks for no averaging, so it takes no starting value
        // out of them (at Log Recursive it would, as for a new picture).
        local.setSpectrumAveraging(SpectrumAveraging::None);
        local.setRemoteSpectrumContext(localContext, 14225023.4375, 192000);
        for (int rowIndex = 0; rowIndex < 2; ++rowIndex) {
            QVector<float> row(128);
            for (int i = 0; i < row.size(); ++i) { row[i] = -160.0f + 0.5f * i + 20.0f * rowIndex; }
            DisplayCodecFrame frame;
            frame.context = codec;
            frame.encoderSequence = static_cast<quint32>(rowIndex + 1);
            frame.waterfallAdvance = true;
            frame.traceDbm = row;
            frame.waterfallDbm = row;
            inputs.nowMs = 1000 + rowIndex * 50;
            localNowMs = inputs.nowMs;
            QVERIFY(local.updateRemoteSpectrum(frame));
            local.composeWaterfallActiveThresholds(row);
            localLevels.append({local.wfActiveLowThreshold(), local.wfActiveHighThreshold()});
            computed.append(processor.process(frame, inputs));
            qInfo() << "mode" << asked.value(QStringLiteral("waterfallLevels")).toObject().value(QStringLiteral("mode"))
                    << "row" << rowIndex << "local" << localLevels.last().first << localLevels.last().second
                    << "Core" << processor.waterfallLevels().first << processor.waterfallLevels().second;
        }
        // Literal oracle: the row's values are worked in the data rows.
        QCOMPARE(localLevels[0].first, low1);
        QCOMPARE(localLevels[0].second, high1);
        QCOMPARE(localLevels[1].first, low2);
        QCOMPARE(localLevels[1].second, high2);
        if (mode.isEmpty()) {
            QVERIFY(!asked.contains(QStringLiteral("waterfallLevels")));
            QVERIFY(!computed[0].waterfallLevelsDbm && !computed[1].waterfallLevelsDbm);
            for (int rowIndex = 0; rowIndex < 2; ++rowIndex) {
                widget->composeWaterfallActiveThresholds(QVector<float>(128, -160.0f + 20.0f * rowIndex));
                QCOMPARE(widget->wfActiveLowThreshold(), low1);
                QCOMPARE(widget->wfActiveHighThreshold(), high1);
            }
        } else {
            QVERIFY(computed[0].waterfallLevelsDbm && computed[1].waterfallLevelsDbm);
            QCOMPARE(computed[0].waterfallLevelsDbm->first, low1);
            QCOMPARE(computed[0].waterfallLevelsDbm->second, high1);
            QCOMPARE(computed[1].waterfallLevelsDbm->first, low2);
            QCOMPARE(computed[1].waterfallLevelsDbm->second, high2);
            QCOMPARE(asked.value(QStringLiteral("waterfallLevels")).toObject().value(QStringLiteral("mode")).toString(), mode);
            for (int rowIndex = 0; rowIndex < 2; ++rowIndex) {
                const QByteArray datagram = encodeDisplayExtras(computed[rowIndex], codec);
                QVERIFY(!datagram.isEmpty());
                const auto decoded = decodeDisplayExtras(datagram, codec);
                QVERIFY(decoded.accepted && decoded.frame.waterfallLevelsDbm);
                QCOMPARE(*decoded.frame.waterfallLevelsDbm, *computed[rowIndex].waterfallLevelsDbm);
                QTRY_VERIFY_WITH_TIMEOUT([&] {
                    media->deliver(datagram);
                    return widget->wfActiveLowThreshold() == computed[rowIndex].waterfallLevelsDbm->first;
                }(), 5000);
                QCOMPARE(widget->wfActiveHighThreshold(), computed[rowIndex].waterfallLevelsDbm->second);
            }
        }
        QCOMPARE(widget->wfLowThreshold(), -180.0f);
        QCOMPARE(widget->wfHighThreshold(), 0.0f);
        QCOMPARE(widget->wfAgcEnabled(), agc);
        QCOMPARE(widget->waterfallNFAGCEnabled(), nfAgc);
        // R-DISP-HOLD: the first range asked already has 10 dB of headroom
        // (the pan's and the stored levels' -180 to 0 dBm with the headroom).
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -190.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), 10.0);
        // NF-AGC's accepted low (-155.5 dBm, the floor read from the first
        // row) fits the -190 dBm window asked first, so it asks nothing more;
        // it asked again for -215 only while the floor started at -200
        // (coreWaterfallAgcWindowContainsAcceptedLevels covers a low under
        // the window). AGC's accepted pair (-171 and the stored High, 0)
        // fits the window asked first now that AGC leaves the high level
        // alone, so it asks nothing more; the request it used to make
        // followed its high level (-83.5), not its low. Manual and Clarity
        // fit theirs too.
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        now += 60;
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        const int expectedRequests = 1;
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), expectedRequests);
        now += 60;
        QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), expectedRequests);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void coreWaterfallAgcWindowContainsAcceptedLevels()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        QMap<QString, QVariant> savedSettings;
        for (const QString& key : appSettings.allKeys()) {
            savedSettings.insert(key, appSettings.value(key));
        }
        const auto restoreSettings = qScopeGuard([&] {
            appSettings.clear();
            for (auto it = savedSettings.cbegin(); it != savedSettings.cend(); ++it) {
                appSettings.setValue(it.key(), it.value());
            }
        });
        appSettings.setValue(QStringLiteral("DisplaySpectrumFps"), QStringLiteral("20"));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // R-DISP-RETUNE: the contexts this test sends by hand state no picture,
        // as an older Core's do, so the window is one that was told none.
        client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture"));
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        widget->setDispNormalize(false);
        widget->setWfUseSpectrumMinMax(false);
        widget->setClarityActive(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setWfAgcEnabled(true);
        widget->setDbmRange(-100.0f, -60.0f);
        widget->setWfLowThreshold(-109.5f);
        widget->setWfHighThreshold(-70.0f);
        widget->setWfBlackLevel(96);
        widget->setWfColorGain(31);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        qint64 now = 1'000;
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            },
            [&now] { return now; });
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QVERIFY(client.capabilities().displayExtrasVersion >= 1);
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // The subscribe asks the Core for the AGC's levels, and its window
        // uses stored levels until the first accepted Core pair arrives.
        QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("waterfallLevels")).toObject(),
                 (QJsonObject{{QStringLiteral("mode"), QStringLiteral("agc")},
                              {QStringLiteral("lowDbm"), -109.5},
                              {QStringLiteral("highDbm"), -70.0},
                              {QStringLiteral("offsetDb"), 0}}));
        // R-DISP-HOLD: the pan's -100 to -60 dBm and the stored levels,
        // with 10 dB either side on whole dB. (This test held the bare
        // -109.5 to -60, here and in the contexts and codecs below; every
        // range below now carries the pan's headroom at its top, -50 dBm.)
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -120.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);
        QVERIFY(widget->coreWaterfallLevelsInUse());

        const auto settle = [&] {
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
            now += 60; // past a frame period (50 ms at 20 frames a second)
            QTest::qWait(RemoteMediaController::kPlannerIntervalMs + 30);
        };

        // Lines arrive as the AGC would settle on them (nothing below the
        // window, then a floor and a signal): the local follower does not
        // run, the stored levels colour the waterfall until the Core's
        // arrive, and nothing is asked.
        widget->composeWaterfallActiveThresholds(QVector<float>(64, -300.0f));
        QCOMPARE(widget->wfActiveLowThreshold(), -109.5f);
        QCOMPARE(widget->wfActiveHighThreshold(), -70.0f);
        settle();
        QVector<float> line(64, -125.0f);
        line[10] = -20.0f;
        for (int i = 0; i < 20; ++i) {
            widget->composeWaterfallActiveThresholds(line);
        }
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // Quantization really collapses the observed -119 dBm population
        // under the stored window; metadata arrives before codec clamping.
        const auto decodedRow = [](double low, double high) {
            const QByteArray encoded = encodeDisplayCodecAbsolutePlane(
                QVector<float>{-120.0f, -119.0f, -118.0f}, float(low), float(high));
            QVector<float> decoded;
            int offset = 0;
            if (!decodeDisplayCodecAbsolutePlane(encoded, offset, 3,
                                                  float(low), float(high), decoded)) {
                return QVector<float>{};
            }
            return decoded;
        };
        QCOMPARE(decodedRow(-109.5, -60.0), QVector<float>(3, -109.5f));

        const quint32 id = quint32(asked.value(QStringLiteral("endpointId")).toDouble());
        QJsonObject context{
            {QStringLiteral("op"), QStringLiteral("context")},
            {QStringLiteral("connectionId"), asked.value(QStringLiteral("connectionId"))},
            {QStringLiteral("endpointId"), double(id)},
            {QStringLiteral("revision"), asked.value(QStringLiteral("revision"))},
            {QStringLiteral("contextGeneration"), 1}, {QStringLiteral("sourceStream"), 0},
            {QStringLiteral("sourceCentreHz"), 14225000}, {QStringLiteral("sampleRateHz"), 192000},
            {QStringLiteral("centreHz"), 14225023.4375}, {QStringLiteral("spanHz"), 24046.875},
            {QStringLiteral("wideCentreHz"), 0}, {QStringLiteral("wideSpanHz"), 0},
            {QStringLiteral("traceSamples"), 128}, {QStringLiteral("waterfallSamples"), 128},
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -120},
            {QStringLiteral("maxDbm"), -50}, {QStringLiteral("fps"), 20},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), WidebandDisplayContext{}.toJson()},
            {QStringLiteral("grantedFftSize"), 4096}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        const DisplayCodecContext codec{id, 1, -120.0f, -50.0f, 128, 128, 0};
        const auto deliverLevels = [&](float low, float high, quint32 sequence) {
            DisplayExtrasFrame extras;
            extras.endpointId = id;
            extras.contextGeneration = 1;
            extras.encoderSequence = sequence;
            extras.waterfallLevelsDbm = std::make_pair(low, high);
            const QByteArray datagram = encodeDisplayExtras(extras, codec);
            QVERIFY(!datagram.isEmpty());
            media->deliver(datagram);
        };
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                deliverLevels(-131.742f, -88.2206f, 1);
                return widget->wfActiveLowThreshold() == -131.742f;
            }(), 5000);
        QCOMPARE(widget->wfActiveHighThreshold(), -88.2206f);
        widget->composeWaterfallActiveThresholds(line);
        QCOMPARE(widget->wfActiveLowThreshold(), -131.742f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -142.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);
        const QVector<float> represented = decodedRow(
            asked.value(QStringLiteral("minDbm")).toDouble(),
            asked.value(QStringLiteral("maxDbm")).toDouble());
        QCOMPARE(represented.size(), 3);
        QVERIFY(represented[0] < represented[1]);
        QVERIFY(represented[1] < represented[2]);
        QVERIFY(std::abs(represented[1] + 119.0f) < 0.4f);
        QCOMPARE(widget->wfBlackLevel(), 96);
        QCOMPARE(widget->wfColorGain(), 31);
        QCOMPARE(widget->wfLowThreshold(), -109.5f);
        QCOMPARE(widget->wfHighThreshold(), -70.0f);
        QVERIFY(widget->wfAgcEnabled());
        QVERIFY(!widget->waterfallNFAGCEnabled());

        // The renderer keeps the last valid Core pair while the new
        // revision is pending. Old-context extras cannot replace that pair.
        deliverLevels(-180.0f, 20.0f, 2);
        QCOMPARE(widget->wfActiveLowThreshold(), -131.742f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        context.insert(QStringLiteral("revision"), asked.value(QStringLiteral("revision")));
        context.insert(QStringLiteral("contextGeneration"), 2);
        context.insert(QStringLiteral("minDbm"), -142.0);
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        const DisplayCodecContext freshCodec{id, 2, -142.0f, -50.0f, 128, 128, 0};
        DisplayExtrasFrame fresh;
        fresh.endpointId = id;
        fresh.contextGeneration = 2;
        fresh.encoderSequence = 3;
        fresh.waterfallLevelsDbm = std::make_pair(-133.0f, -87.0f);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            media->deliver(encodeDisplayExtras(fresh, freshCodec));
            return widget->wfActiveLowThreshold() == -133.0f;
        }(), 5000);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 2);

        // Malformed/non-finite metadata and an unrelated endpoint cannot
        // poison the accepted cache or trigger a new transport window.
        QByteArray invalid = encodeDisplayExtras(fresh, freshCodec);
        invalid.replace(kDisplayExtrasHeaderBytes, 4, QByteArray::fromHex("7fc00000"));
        media->deliver(invalid);
        invalid = encodeDisplayExtras(fresh, freshCodec);
        qToBigEndian<quint32>(id + 1, invalid.data() + 8);
        media->deliver(invalid);
        QCOMPARE(widget->wfActiveLowThreshold(), -133.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 2);

        // Exercise the real media decoder, row queue and RGB writer with
        // lower samples that used to collapse to the old codec endpoint.
        widget->setWaterfallTickerPausedForTest(true);
        DisplayCodecFrame frame;
        frame.context = freshCodec;
        frame.encoderSequence = 4;
        frame.waterfallAdvance = true;
        frame.traceDbm = QVector<float>(128, -119.0f);
        frame.waterfallDbm = QVector<float>(128, -119.0f);
        for (int i = 0; i < 42; ++i) { frame.waterfallDbm[i] = -120.0f; }
        for (int i = 86; i < 128; ++i) { frame.waterfallDbm[i] = -118.0f; }
        DisplayCodecEncoder encoder;
        const QByteArray rowPacket = encoder.encode(frame);
        QVERIFY(!rowPacket.isEmpty());
        media->deliver(rowPacket);
        QTRY_VERIFY_WITH_TIMEOUT(widget->remoteRowQueueDepthForTest() > 0, 5000);
        widget->tickWaterfallForTest();
        const QVector<float> pushed = widget->lastRemoteRowPushedForTest();
        QCOMPARE(pushed.size(), 128);
        QVERIFY(pushed[20] < pushed[64]);
        QVERIFY(pushed[64] < pushed[100]);
        QVERIFY(std::abs(pushed[64] + 119.0f) < 0.4f);
        const QImage& image = widget->liveWaterfallForTest();
        QVERIFY(!image.isNull());
        const QRgb centre = image.pixel(image.width() / 2,
                                         widget->liveWaterfallWriteRowForTest());
        QCOMPARE(centre, widget->dbmToRgbForTest(pushed[64]));
        QVERIFY(centre != widget->dbmToRgbForTest(-109.5f));

        // Core metadata can update its cache while Clarity owns the active
        // mirror. On handoff the transport must use that cache immediately,
        // before another row composes the active thresholds. Pre-codec Core
        // levels cannot walk down from codec-floor feedback, so no local
        // AGC reach cap may clip this accepted pair.
        widget->setClarityActive(true);
        widget->setClarityWaterfallThresholds(-95.0f, -30.0f);
        fresh.encoderSequence = 5;
        fresh.waterfallLevelsDbm = std::make_pair(-250.0f, -80.0f);
        media->deliver(encodeDisplayExtras(fresh, freshCodec));
        QCOMPARE(widget->wfActiveLowThreshold(), -95.0f);
        widget->setClarityActive(false);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -260.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);
        widget->composeWaterfallActiveThresholds(line);
        QCOMPARE(widget->wfActiveLowThreshold(), -250.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        QCOMPARE(widget->wfBlackLevel(), 96);
        QCOMPARE(widget->wfColorGain(), 31);
        QCOMPARE(widget->waterfallAGCOffsetDb(), 0);

        // NF-AGC, an option under AGC, asks for its own levels; the AGC's
        // are not kept. The changed mode and subsequent settled window can
        // each request once.
        widget->setWaterfallNFAGCEnabled(true);
        widget->setWaterfallAGCOffsetDb(6);
        settle();
        QTRY_VERIFY(countControl(controls, QStringLiteral("subscribe")) >= 4);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        const QJsonObject levels = asked.value(QStringLiteral("waterfallLevels")).toObject();
        QCOMPARE(levels.value(QStringLiteral("mode")).toString(), QStringLiteral("noiseFloorAgc"));
        QCOMPARE(levels.value(QStringLiteral("offsetDb")).toInt(), 6);
        // The stored levels again, with the headroom: the -260 dBm held for
        // the AGC's pair is far wider than they need.
        QTRY_COMPARE(lastControl(controls, QStringLiteral("subscribe"))
                         .value(QStringLiteral("minDbm")).toDouble(), -120.0);
        widget->composeWaterfallActiveThresholds(line);
        QCOMPARE(widget->wfActiveLowThreshold(), -109.5f);

        // NF-AGC's accepted low under the window (PR 628 re-review: this
        // test said it covered NF-AGC and turned it off): the Core's pair
        // for the new context, its low under the -120 dBm window asked,
        // takes over the waterfall and widens the window to contain it,
        // once, as the AGC's did.
        const int beforeNfAgcPair = countControl(controls, QStringLiteral("subscribe"));
        context.insert(QStringLiteral("revision"),
                       lastControl(controls, QStringLiteral("subscribe"))
                           .value(QStringLiteral("revision")));
        context.insert(QStringLiteral("contextGeneration"), 3);
        context.insert(QStringLiteral("minDbm"), -120.0);
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        const DisplayCodecContext nfAgcCodec{id, 3, -120.0f, -50.0f, 128, 128, 0};
        DisplayExtrasFrame nfAgcPair;
        nfAgcPair.endpointId = id;
        nfAgcPair.contextGeneration = 3;
        nfAgcPair.encoderSequence = 6;
        nfAgcPair.waterfallLevelsDbm = std::make_pair(-131.742f, -70.0f);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            media->deliver(encodeDisplayExtras(nfAgcPair, nfAgcCodec));
            return widget->wfActiveLowThreshold() == -131.742f;
        }(), 5000);
        QCOMPARE(widget->wfActiveHighThreshold(), -70.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), beforeNfAgcPair + 1);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("waterfallLevels")).toObject()
                     .value(QStringLiteral("mode")).toString(),
                 QStringLiteral("noiseFloorAgc"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -142.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -50.0);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), beforeNfAgcPair + 1);

        // NF-AGC without AGC is manual levels, and manual levels ask for
        // none.
        widget->setWfAgcEnabled(false);
        settle();
        QTRY_VERIFY(!lastControl(controls, QStringLiteral("subscribe"))
                         .contains(QStringLiteral("waterfallLevels")));
        QVERIFY(!widget->coreWaterfallLevelsInUse());
        widget->setWaterfallNFAGCEnabled(false);
        settle();
        QTRY_VERIFY(!lastControl(controls, QStringLiteral("subscribe"))
                         .contains(QStringLiteral("waterfallLevels")));

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-01/08/37: outside budget mode Core's five-key per-pan refusal
    // puts the budget-mode line on the pan, with no toast, until the pan's
    // request changes. A retirement for a slice the operator removed or
    // rebound is not shown as a refusal, even while this window still has
    // the slice.
    void perPanRefusalShowsPlainStatusLineOutsideBudgetMode()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(!station.slices().isEmpty());
        SliceModel* stationSlice = station.slices().first();
        stationSlice->setStreamIndex(0);
        stationSlice->setFrequency(14225000);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(14225000, 24000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> media;
        RemoteMediaController controller(&client, &remote, &stack, nullptr,
            [&media](QObject* owner) -> IMediaTransport* {
                media = new DisplayTransport(owner);
                return media;
            });
        QSignalSpy errors(&controller, &RemoteMediaController::errorOccurred);
        QSignalSpy controls(&server, &StationServer::mediaControlReceived);
        QSignalSpy receivedControls(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.mediaAvailable());
        QTRY_VERIFY(media);
        media->activate();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 1);
        QVERIFY(!client.remoteDisplayBudgetLimits().has_value());
        QVERIFY(applet->remoteDisplayStatus().isEmpty());

        const auto refuse = [&](const QString& reason) {
            const QJsonObject subscription = lastControl(controls, QStringLiteral("subscribe"));
            const int before = countControl(receivedControls, QStringLiteral("rejected"));
            // Exactly what DaemonMediaController::sendRejected() sends.
            QVERIFY(server.sendMediaControl({
                {QStringLiteral("op"), QStringLiteral("rejected")},
                {QStringLiteral("connectionId"), subscription.value(QStringLiteral("connectionId"))},
                {QStringLiteral("endpointId"), subscription.value(QStringLiteral("endpointId"))},
                {QStringLiteral("revision"), subscription.value(QStringLiteral("revision"))},
                {QStringLiteral("reason"), reason}}, server.mediaSessionEpoch()));
            QTRY_COMPARE(countControl(receivedControls, QStringLiteral("rejected")), before + 1);
        };
        // Translated when shown; the Core's reason itself is unchanged.
        const QString refused = QStringLiteral("Refused: out of range");

        refuse(QStringLiteral("requested crop is outside source coverage"));
        QTRY_COMPARE(applet->remoteDisplayStatus(), refused);
        QVERIFY(refusedFor(controller, applet,
                           QStringLiteral("requested crop is outside source coverage")));
        // The periodic refresh keeps the reason while the request stands.
        QTest::qWait(250);
        QCOMPARE(applet->remoteDisplayStatus(), refused);
        QCOMPARE(errors.count(), 0);

        // A new request clears it when it goes out.
        widget->setDisplayWindowPreservingHistory(14226000, 24000);
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        QTRY_VERIFY2(applet->remoteDisplayStatus().isEmpty(),
                     qPrintable(applet->remoteDisplayStatus()));
        QTest::qWait(250);
        QVERIFY(applet->remoteDisplayStatus().isEmpty());

        // Core retires the endpoint because the slice went or was rebound,
        // and this window has not yet seen that change: no refusal line.
        for (const QString& operatorChange :
             {QString::fromLatin1(kRetireReasonSliceRemoved),
              QString::fromLatin1(kRetireReasonStreamBindingChanged)}) {
            refuse(operatorChange);
            QTest::qWait(250);
            QVERIFY2(applet->remoteDisplayStatus().isEmpty(),
                     qPrintable(applet->remoteDisplayStatus()));
            QVERIFY(remote.sliceById(stationSlice->sliceIndex()) != nullptr);
            const int subscribes = countControl(controls, QStringLiteral("subscribe"));
            widget->setDisplayWindowPreservingHistory(
                widget->centerFrequency() + 1000, 24000);
            QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), subscribes + 1);
        }
        QCOMPARE(errors.count(), 0);

        client.disconnectFromStation(QStringLiteral("test complete"));
        QVERIFY(!media || !media->active);
    }

    // R-R3-01/09: a Core and a GUI that agree minor 8 keep today's context
    // and keep painting; minor 9 carries the grant. Each GUI accepts only
    // the shape it negotiated, so neither direction of a mixed pair breaks.
    void spectrumContextShapeFollowsTheAgreedMinor_data()
    {
        QTest::addColumn<int>("minor");
        QTest::newRow("minor 8") << int(kRemoteSpectrumGrantSessionProtocolMinor - 1);
        QTest::newRow("minor 9") << int(kRemoteSpectrumGrantSessionProtocolMinor);
    }

    void spectrumContextShapeFollowsTheAgreedMinor()
    {
        QFETCH(int, minor);
        const bool grantAgreed = minor >= kRemoteSpectrumGrantSessionProtocolMinor;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("only"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&gui, &RemoteMediaController::displayFrameReceived);
        // Both ends announce the row's minor, so each believes the other is
        // a build of that minor.
        auto* stationLink = new Test::RewritingTransport(QStringLiteral("station"),
                                                         quint16(minor));
        auto* clientLink = new Test::RewritingTransport(QStringLiteral("client"),
                                                        quint16(minor));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QCOMPARE(client.agreedMinor(), quint16(minor));
        QCOMPARE(server.spectrumGrantAvailable(), grantAgreed);
        QCOMPARE(client.spectrumGrantAvailable(), grantAgreed);
        QCOMPARE(gui.spectrumGrantNegotiated(), grantAgreed);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        const auto painted = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return frames.count() > 0
                && !applet->spectrumWidget()->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(painted(), 5000);
        QCOMPARE(countControl(inbound, QStringLiteral("rejected")), 0);
        const QList<QJsonObject> contexts = controlsFor(inbound, QStringLiteral("context"));
        QVERIFY(!contexts.isEmpty());
        for (const QJsonObject& context : contexts) {
            const bool wideband = context.contains(QStringLiteral("wideband"));
            QCOMPARE(context.size(), (grantAgreed ? 24 : 19) + (wideband ? 1 : 0));
            QCOMPARE(context.contains(QStringLiteral("limit")), grantAgreed);
            QVERIFY(decodeRemoteSpectrumContext(context, grantAgreed).has_value());
            QVERIFY(!decodeRemoteSpectrumContext(context, !grantAgreed).has_value());
        }
        if (!grantAgreed) {
            // No grant was reported, so no grant line can appear.
            QCOMPARE(gui.panDisplayState(applet->panId()).zoomLimit,
                     PanDisplayState::ZoomLimit::None);
        }
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The TX mini is the Core's fixed analyzer window, not an RX FFT crop.
    // Requiring the latter drops valid TX contexts before any frame reaches
    // the mini. The transport is fake; context admission and presentation
    // are production code, with no radio, sound device or network backend.
    void transmitMiniAdmitsItsFixedWindow_data()
    {
        QTest::addColumn<bool>("budget");
        QTest::addColumn<bool>("picture");
        QTest::addColumn<double>("originHz");
        QTest::addColumn<bool>("txState");
        for (bool budget : {false, true}) {
            for (bool picture : {false, true}) {
                for (const auto& origin : {std::pair{"ordinary", 14'074'000.0},
                                           std::pair{"DIGU", 14'073'400.0},
                                           std::pair{"DIGL", 14'074'600.0}}) {
                    const QByteArray name = QByteArray(budget ? "budget-" : "legacy-")
                        + (picture ? "picture-" : "no-picture-") + origin.first;
                    QTest::newRow(name.constData()) << budget << picture << origin.second << false;
                    const QByteArray versionedName = name + "-tx-state";
                    QTest::newRow(versionedName.constData()) << budget << picture << origin.second << true;
                }
            }
        }
    }

    void transmitMiniAdmitsItsFixedWindow()
    {
        QFETCH(bool, budget);
        QFETCH(bool, picture);
        QFETCH(double, originHz);
        QFETCH(bool, txState);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        SliceModel* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        slice->setFrequency(14'074'000.0);
        QCOMPARE(station.txBoundSlice(), slice);
        QVERIFY(slice->isTxSlice());
        station.sliceOwnership()->setOwner(sliceId, QByteArrayLiteral("token:1"));
        const int otherId = station.addSlice();
        station.sliceOwnership()->setOwner(otherId, QByteArrayLiteral("token:1"));
        QCOMPARE(station.txBoundSlice(), slice);
        const int stream = slice->streamIndex();
        TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
        station.setTxAnalyzer(&analyzer); // Declares the TX display; opens no TX channel.
        const auto clearAnalyzer = qScopeGuard([&] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings, Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        server.setRemoteTransmitAllowed(true);
        server.setTokenSessionsMayTransmitForTest(true);
        TransmitHolder::Holder holder;
        holder.deviceId = QByteArrayLiteral("token:1");
        holder.name = QStringLiteral("Mini display Core fixture");
        server.transmitHolder()->transferTo(holder, QStringLiteral("test"));
        QVERIFY(slice->txSliceMarked());
        if (budget) { server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}); }
        QPointer<DisplayTransport> coreMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&](QObject* owner) -> IMediaTransport* {
                coreMedia = new DisplayTransport(owner);
                return coreMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        CaptureSupervisor::Options capture;
        capture.program = dir.filePath(QStringLiteral("no-real-capture-helper"));
        remote.audioEngine()->setCaptureSupervisorOptionsForTest(capture);
        if (txState) {
            // Local ClientAudio uses this fake non-PC source: the session's
            // accepted ClientAudio cannot change it to a PC preview demand.
            remote.transmitModel().setVaxSourceAvailable(true);
            remote.transmitModel().setMicSource(MicSource::Vax);
            QCOMPARE(remote.transmitModel().micSource(), MicSource::Vax);
        }
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        // This display-only peer offers no microphone line or keying verbs.
        if (!txState) { client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); }
        if (!picture) { client.withholdFeatureForTest(QByteArrayLiteral("spectrumPicture")); }
        PanadapterStack stack; // No main pan: this fixture subscribes only the mini.
        QPointer<DisplayTransport> windowMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&](QObject* owner) -> IMediaTransport* {
                windowMedia = new DisplayTransport(owner);
                return windowMedia;
            });
        QSignalSpy requests(&server, &StationServer::mediaControlReceived);
        QSignalSpy contexts(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&gui, &RemoteMediaController::miniDisplayFrame);
        QSignalSpy unavailable(&gui, &RemoteMediaController::miniDisplayUnavailable);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* windowLink = new Test::LoopbackTransport(QStringLiteral("window"));
        stationLink->linkTo(windowLink);
        client.startSession(windowLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(coreMedia && windowMedia);
        QVERIFY(gui.txDisplayNegotiated());
        QCOMPARE(client.capabilities().txStateVersion >= 1, txState);
        coreMedia->other = windowMedia;
        coreMedia->activate();
        windowMedia->activate();
        gui.setMiniDisplaySlices({sliceId});
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        const QVector<float> iq(4096, 0.01f);
        const auto feedRx = [&] {
            for (int block = 0; block < kFeedBlocksPerPoll; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            }
        };
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            feedRx();
            return !frames.isEmpty();
        }()), 5'000);
        const QJsonObject rxContext = lastControl(contexts, QStringLiteral("context"));
        QVERIFY(!rxContext.isEmpty());
        QCOMPARE(frames.last().at(5).toBool(), false);
        if (txState) { QCOMPARE(remote.transmitModel().micSource(), MicSource::Vax); }
        else { QVERIFY(!gui.micLineOpen()); }
        QVERIFY(!remote.audioEngine()->hasCaptureDemand());
        QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
        unavailable.clear();
        frames.clear();
        // Inbound Core observations, not a local key or an outbound TX request.
        QCOMPARE(station.txBoundSlice(), slice);
        QVERIFY(slice->txSliceMarked());
        QTRY_VERIFY(remote.sliceById(sliceId)->isTxSlice());
        if (txState) {
            // Publish on the fake Core facade, then observe real session
            // deltas on the window. Legacy radio.transmitting stays false.
            QVERIFY(server.transmitState()->applyStationValue("txSliceId", sliceId));
            QVERIFY(server.transmitState()->applyStationValue("keyed", true));
            QTRY_VERIFY(client.transmitState()->keyed());
            QTRY_COMPARE(client.transmitState()->txSliceId(), sliceId);
            QVERIFY(!remote.isTransmitting());
        } else {
            QCOMPARE(client.capabilities().txStateVersion, 0);
            QVERIFY(remote.applyMirroredValue("transmitting", true).isEmpty());
        }
        gui.refreshTransmitView();
        QTRY_VERIFY_WITH_TIMEOUT(lastControl(requests, QStringLiteral("subscribe"))
                        .value(QStringLiteral("spanHz")).toDouble() == 40'000.0, 1'000);
        const QJsonObject ask = lastControl(requests, QStringLiteral("subscribe"));
        const auto pendingCleared = [&] {
            return !budget || lastControl(contexts, QStringLiteral("allocation-result"))
                .value(QStringLiteral("revision")) == ask.value(QStringLiteral("revision"));
        };
        QTRY_VERIFY(pendingCleared());
        // RX frames received before the versioned key delta are not the
        // TX result being awaited below.
        frames.clear();
        // Picture-aware requests retain RX pixels until a TX context is admitted.
        QCOMPARE(unavailable.isEmpty(), picture);
        QJsonObject txContext = rxContext;
        txContext.insert(QStringLiteral("revision"), ask.value(QStringLiteral("revision")));
        constexpr quint32 kGeneration = 10'000;
        txContext.insert(QStringLiteral("contextGeneration"), static_cast<qint64>(kGeneration));
        if (picture) { txContext.insert(QStringLiteral("pictureGeneration"), static_cast<qint64>(kGeneration)); }
        txContext.insert(QStringLiteral("transmit"), true);
        txContext.insert(QStringLiteral("sourceCentreHz"), originHz);
        txContext.insert(QStringLiteral("centreHz"), originHz);
        txContext.insert(QStringLiteral("sampleRateHz"), 96'000.0);
        txContext.insert(QStringLiteral("spanHz"), 40'000.0);
        txContext.insert(QStringLiteral("traceSamples"), 1024);
        txContext.insert(QStringLiteral("waterfallSamples"), 1024);
        txContext.insert(QStringLiteral("wideSamples"), 0);
        txContext.insert(QStringLiteral("wideCentreHz"), 0);
        txContext.insert(QStringLiteral("wideSpanHz"), 0);
        txContext.insert(QStringLiteral("grantedFftSize"), 32768);
        txContext.insert(QStringLiteral("requestedPixels"), 1024);
        txContext.insert(QStringLiteral("grantedPixels"), 1024);
        const auto decoded = decodeRemoteSpectrumContext(txContext, true, true, picture);
        QVERIFY(decoded);
        DisplayCodecFrame frame;
        frame.context.endpointId = decoded->endpointId;
        frame.context.contextGeneration = kGeneration;
        frame.context.traceSamples = frame.context.waterfallSamples = 1024;
        frame.context.minDbm = float(decoded->minDbm);
        frame.context.maxDbm = float(decoded->maxDbm);
        frame.traceDbm = frame.waterfallDbm = QVector<float>(1024, -100.0f);
        frame.traceDbm[527] = frame.waterfallDbm[527] = -50.0f;
        frame.waterfallAdvance = true;
        DisplayCodecEncoder encoder;
        QVERIFY(server.sendMediaControl(txContext, server.mediaSessionEpoch()));
        // Control and media are independent: do not consume the encoder's
        // first keyframe before the window has processed its context.
        QTRY_COMPARE(lastControl(contexts, QStringLiteral("context"))
            .value(QStringLiteral("contextGeneration")).toInt(), int(kGeneration));
        const auto sendFrame = [&] {
            ++frame.encoderSequence;
            frame.producerTimestamp += 33'333'333;
            windowMedia->deliver(encoder.encode(frame));
            return std::any_of(frames.begin(), frames.end(), [](const auto& delivered) {
                return delivered.at(5).toBool();
            });
        };
        QTRY_VERIFY_WITH_TIMEOUT(sendFrame(), 1'000);
        QCOMPARE(frames.last().at(0).toInt(), sliceId);
        QCOMPARE(frames.last().at(3).toDouble(), originHz);
        QCOMPARE(frames.last().at(4).toDouble(), 40'000.0);
        QCOMPARE(frames.last().at(5).toBool(), true);
        const QVector<float> received = frames.last().at(1).value<QVector<float>>();
        QCOMPARE(std::distance(received.begin(), std::max_element(received.begin(), received.end())), 527);

        // A shifted domain, wrong span/rate/state/stream/shape or old request
        // must neither install a new context nor deliver its recognizable peak.
        const QList<std::pair<QString, QJsonValue>> invalidFields{
            {QStringLiteral("centreHz"), originHz + 100.0},
            {QStringLiteral("spanHz"), 40'003.0},
            {QStringLiteral("sampleRateHz"), 48'000.0},
            {QStringLiteral("transmit"), false},
            {QStringLiteral("sourceStream"), stream + 1},
            {QStringLiteral("traceSamples"), 512},
            {QStringLiteral("revision"), ask.value(QStringLiteral("revision")).toInt() - 1}};
        quint32 invalidGeneration = kGeneration;
        for (const auto& field : invalidFields) {
            QJsonObject invalid = txContext;
            invalid.insert(QStringLiteral("contextGeneration"), static_cast<qint64>(++invalidGeneration));
            if (picture) { invalid.insert(QStringLiteral("pictureGeneration"), static_cast<qint64>(invalidGeneration)); }
            invalid.insert(field.first, field.second);
            frames.clear();
            QVERIFY(server.sendMediaControl(invalid, server.mediaSessionEpoch()));
            QTRY_COMPARE(lastControl(contexts, QStringLiteral("context"))
                             .value(QStringLiteral("contextGeneration")).toInt(), int(invalidGeneration));
            DisplayCodecFrame badFrame = frame;
            badFrame.context.contextGeneration = invalidGeneration;
            badFrame.encoderSequence = 1;
            badFrame.traceDbm[527] = -100.0f;
            badFrame.traceDbm[100] = -50.0f;
            DisplayCodecEncoder badEncoder;
            windowMedia->deliver(badEncoder.encode(badFrame));
            QTRY_VERIFY_WITH_TIMEOUT(sendFrame(), 1'000);
            for (const auto& delivered : frames) {
                QCOMPARE(delivered.at(3).toDouble(), originHz);
                QCOMPARE(delivered.at(4).toDouble(), 40'000.0);
                QCOMPARE(delivered.at(5).toBool(), true);
                const QVector<float> bins = delivered.at(1).value<QVector<float>>();
                QCOMPARE(std::distance(bins.begin(), std::max_element(bins.begin(), bins.end())), 527);
            }
        }
        // Late RX and old TX generations cannot replace the admitted TX frame.
        for (const quint32 old : {quint32(rxContext.value(QStringLiteral("contextGeneration")).toInt()),
                                 kGeneration - 1}) {
            frames.clear();
            DisplayCodecFrame stale = frame;
            stale.context.contextGeneration = old;
            stale.encoderSequence = 1;
            stale.traceDbm[527] = -100.0f;
            stale.traceDbm[100] = -50.0f;
            DisplayCodecEncoder staleEncoder;
            windowMedia->deliver(staleEncoder.encode(stale));
            QTRY_VERIFY_WITH_TIMEOUT(sendFrame(), 1'000);
            const QVector<float> bins = frames.last().at(1).value<QVector<float>>();
            QCOMPARE(std::distance(bins.begin(), std::max_element(bins.begin(), bins.end())), 527);
        }

        // Absent/ambiguous legacy flags, or a versioned identity not yet
        // resolvable from the slice snapshot, never fall back to another
        // slice. Old TX packets stop; a fresh RX context still presents.
        SliceModel* remoteOther = remote.sliceById(otherId);
        QVERIFY(remoteOther);
        const auto receiveRx = [&] {
            feedRx();
            return std::any_of(frames.begin(), frames.end(), [](const auto& delivered) {
                return !delivered.at(5).toBool();
            });
        };
        quint32 installedGeneration = kGeneration;
        for (int identityCase = 0; identityCase < 2; ++identityCase) {
            if (txState) {
                const int unresolved = identityCase == 0 ? -1 : otherId + 1000;
                QVERIFY(server.transmitState()->applyStationValue("txSliceId", unresolved));
                QTRY_COMPARE(client.transmitState()->txSliceId(), unresolved);
                QVERIFY(client.transmitState()->keyed());
            } else if (identityCase == 0) {
                // Same assignments as the real inbound slice mirror uses.
                remote.sliceById(sliceId)->setTxSlice(false);
            } else {
                remoteOther->setTxSlice(true);
                QVERIFY(remote.sliceById(sliceId)->isTxSlice());
            }
            QTRY_VERIFY_WITH_TIMEOUT(lastControl(requests, QStringLiteral("subscribe"))
                .value(QStringLiteral("spanHz")).toDouble() == 20'000.0, 1'000);
            frames.clear();
            sendFrame();
            QTRY_VERIFY_WITH_TIMEOUT(receiveRx(), 5'000);
            for (const auto& delivered : frames) { QVERIFY(!delivered.at(5).toBool()); }
            if (txState) {
                QVERIFY(server.transmitState()->applyStationValue("txSliceId", sliceId));
                QTRY_COMPARE(client.transmitState()->txSliceId(), sliceId);
            } else {
                remoteOther->setTxSlice(false);
                remote.sliceById(sliceId)->setTxSlice(true);
            }
            QTRY_VERIFY_WITH_TIMEOUT(lastControl(requests, QStringLiteral("subscribe"))
                .value(QStringLiteral("spanHz")).toDouble() == 40'000.0, 1'000);
            const QJsonObject renewedAsk = lastControl(requests, QStringLiteral("subscribe"));
            if (budget) {
                QTRY_COMPARE(lastControl(contexts, QStringLiteral("allocation-result"))
                    .value(QStringLiteral("revision")), renewedAsk.value(QStringLiteral("revision")));
            }
            txContext.insert(QStringLiteral("revision"), renewedAsk.value(QStringLiteral("revision")));
            installedGeneration += 10'000;
            txContext.insert(QStringLiteral("contextGeneration"), static_cast<qint64>(installedGeneration));
            if (picture) { txContext.insert(QStringLiteral("pictureGeneration"), static_cast<qint64>(installedGeneration)); }
            frame.context.contextGeneration = installedGeneration;
            frame.encoderSequence = 0;
            encoder.reset();
            frames.clear();
            QVERIFY(server.sendMediaControl(txContext, server.mediaSessionEpoch()));
            QTRY_COMPARE(lastControl(contexts, QStringLiteral("context"))
                .value(QStringLiteral("contextGeneration")).toInt(), int(installedGeneration));
            QTRY_VERIFY_WITH_TIMEOUT(sendFrame(), 1'000);
            QCOMPARE(frames.last().at(3).toDouble(), originHz);
            QCOMPARE(frames.last().at(5).toBool(), true);
        }
        // The versioned unkey also wins over stale legacy TX observations.
        if (txState) {
            QVERIFY(remote.applyMirroredValue("transmitting", true).isEmpty());
            QVERIFY(server.transmitState()->applyStationValue("keyed", false));
            QTRY_VERIFY(!client.transmitState()->keyed());
        } else {
            QVERIFY(remote.applyMirroredValue("transmitting", false).isEmpty());
        }
        QTRY_VERIFY_WITH_TIMEOUT(lastControl(requests, QStringLiteral("subscribe"))
            .value(QStringLiteral("spanHz")).toDouble() == 20'000.0, 1'000);
        frames.clear();
        sendFrame();
        QTRY_VERIFY_WITH_TIMEOUT(receiveRx(), 5'000);
        for (const auto& delivered : frames) { QVERIFY(!delivered.at(5).toBool()); }
        client.disconnectFromStation(QStringLiteral("test complete"));
        QVERIFY(!remote.audioEngine()->hasCaptureDemand());
        QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
    }

    void miniSlicesShareOneStreamButKeepIndependentEndpoints()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int a = station.addSlice();
        const int b = station.addSlice();
        SliceModel* first = station.sliceById(a);
        SliceModel* second = station.sliceById(b);
        QVERIFY(first && second);
        const int stream = first->streamIndex();
        QVERIFY(stream >= 0);
        const double sourceHz = station.streamCentreHz(stream);
        first->setFrequency(sourceHz - 12'000.0);
        second->setFrequency(sourceHz + 12'000.0);
        second->setStreamIndex(stream);
        StationServer server(&station, settings,
            NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("main"));
        applet->setActiveSliceIndex(a);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(sourceHz, 48'000.0);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy miniFrames(&gui, &RemoteMediaController::miniDisplayFrame);
        QSignalSpy unavailable(&gui, &RemoteMediaController::miniDisplayUnavailable);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QTRY_COMPARE(client.capabilities().miniDisplayVersion, 1);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        gui.setMiniDisplaySlices({a, b});
        QTRY_COMPARE(daemon.activeEndpointCount(), 3);
        const QList<QJsonObject> requests = controlsFor(outbound, QStringLiteral("subscribe"));
        int miniCount = 0;
        for (const QJsonObject& request : requests) {
            if (request.value(QStringLiteral("displayRole")).toString() != QLatin1String("mini")) {
                continue;
            }
            ++miniCount;
            QCOMPARE(request.value(QStringLiteral("spanHz")).toDouble(), 20'000.0);
            QCOMPARE(request.value(QStringLiteral("pixels")).toInt(), 1024);
            QCOMPARE(request.value(QStringLiteral("fps")).toInt(), 30);
        }
        QCOMPARE(miniCount, 2);
        QVector<float> iq(4096);
        for (int n = 0; n < iq.size() / 2; ++n) {
            const double phaseA = 2.0 * std::numbers::pi * (-10'000.0 / 192'000.0) * n;
            const double phaseB = 2.0 * std::numbers::pi * (9'000.0 / 192'000.0) * n;
            iq[2*n] = float(0.02 * (std::cos(phaseA) + std::cos(phaseB)));
            iq[2*n+1] = float(0.02 * (std::sin(phaseA) + std::sin(phaseB)));
        }
        const auto seenBoth = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            bool seenA = false;
            bool seenB = false;
            for (const auto& frame : miniFrames) {
                seenA |= frame.at(0).toInt() == a;
                seenB |= frame.at(0).toInt() == b;
            }
            return seenA && seenB;
        };
        const auto miniStage = qScopeGuard([&] {
            bool seenA = false;
            bool seenB = false;
            for (const auto& frame : miniFrames) {
                seenA |= frame.at(0).toInt() == a;
                seenB |= frame.at(0).toInt() == b;
            }
            if (seenA && seenB) { return; }
            qWarning() << "mini frame stage:" << "endpoints"
                       << daemon.activeEndpointCount() << gui.activeEndpointCount()
                       << "sourcePackets" << sourceMedia->displayPackets.size()
                       << "sinkFrames" << miniFrames.size()
                       << "allGuiFrames" << gui.receivedDisplayFrames()
                       << "seen" << seenA << seenB
                       << "contexts" << controlsFor(inbound, QStringLiteral("context")).size()
                       << "rejections" << countControl(inbound, QStringLiteral("rejected"));
            for (const QJsonObject& context : controlsFor(inbound, QStringLiteral("context"))) {
                qWarning() << "mini context" << context.value(QStringLiteral("endpointId")).toInt()
                           << context.value(QStringLiteral("centreHz")).toDouble()
                           << context.value(QStringLiteral("spanHz")).toDouble()
                           << context.value(QStringLiteral("traceSamples")).toInt()
                           << context.value(QStringLiteral("sourceStream")).toInt()
                           << context.value(QStringLiteral("contextGeneration")).toInt()
                           << "revision" << context.value(QStringLiteral("revision")).toInt()
                           << "fft" << context.value(QStringLiteral("grantedFftSize")).toInt()
                           << "tx" << context.value(QStringLiteral("transmit")).toBool()
                           << "wide" << context.value(QStringLiteral("wideSamples")).toInt()
                           << "rate" << context.value(QStringLiteral("sampleRateHz")).toDouble();
            }
            for (const QJsonObject& request : controlsFor(outbound, QStringLiteral("subscribe"))) {
                qWarning() << "mini request" << request.value(QStringLiteral("endpointId")).toInt()
                           << request.value(QStringLiteral("displayRole")).toString()
                           << request.value(QStringLiteral("centreHz")).toDouble()
                           << request.value(QStringLiteral("spanHz")).toDouble()
                           << "revision" << request.value(QStringLiteral("revision")).toInt()
                           << "fft" << request.value(QStringLiteral("fftSize")).toInt();
            }
        });
        QTRY_VERIFY_WITH_TIMEOUT(seenBoth(), 5'000);
        const auto framesSince = [&](int previous, int sliceId) {
            for (int index = previous; index < miniFrames.size(); ++index) {
                if (miniFrames.at(index).at(0).toInt() == sliceId) { return true; }
            }
            return false;
        };
        QJsonObject acceptedMiniContext;
        for (const QJsonObject& context : controlsFor(inbound, QStringLiteral("context"))) {
            if (context.value(QStringLiteral("endpointId")).toInt() == 2) {
                acceptedMiniContext = context;
            }
        }
        QVERIFY(!acceptedMiniContext.isEmpty());
        const double binHz = acceptedMiniContext.value(QStringLiteral("sampleRateHz")).toDouble()
            / acceptedMiniContext.value(QStringLiteral("grantedFftSize")).toInt();
        // Core's inclusive floor/ceil crop here exceeds two FFT bins beyond
        // the requested 20 kHz. The GUI accepted exactly that crop.
        QVERIFY(acceptedMiniContext.value(QStringLiteral("spanHz")).toDouble()
                - 20'000.0 > 2.0 * binHz);
        const int beforeInvalid = miniFrames.size();
        QJsonObject shifted = acceptedMiniContext;
        shifted.insert(QStringLiteral("contextGeneration"),
            acceptedMiniContext.value(QStringLiteral("contextGeneration")).toInt() + 1);
        shifted.insert(QStringLiteral("centreHz"),
            acceptedMiniContext.value(QStringLiteral("centreHz")).toDouble() + binHz);
        QVERIFY(server.sendMediaControl(shifted, server.mediaSessionEpoch()));
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return framesSince(beforeInvalid, a);
        }(), 5'000);
        const int beforePanZoom = miniFrames.size();
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(sourceHz + 4'000.0,
                                                                     12'000.0);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return framesSince(beforePanZoom, a) && framesSince(beforePanZoom, b);
        }(), 5'000);
        QByteArray retiredPacket;
        for (const QByteArray& packet : sourceMedia->displayPackets) {
            if (packet.size() >= 16 && packet.first(4) == QByteArrayLiteral("NSDC")
                && qFromBigEndian<quint32>(packet.constData() + 8) == 3) {
                retiredPacket = packet;
            }
        }
        QVERIFY(!retiredPacket.isEmpty());
        stack.removePanadapter(QStringLiteral("main"));
        QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        const int beforeNoPan = miniFrames.size();
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return framesSince(beforeNoPan, a) && framesSince(beforeNoPan, b);
        }(), 5'000);
        gui.setMiniDisplaySlices({a});
        QTRY_VERIFY(!unavailable.isEmpty());
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        const int beforeStale = miniFrames.size();
        sinkMedia->deliver(retiredPacket);
        QTest::qWait(100);
        QVERIFY(!framesSince(beforeStale, b));
        QVERIFY(!gui.micLineOpen());
        QVERIFY(!remote.audioEngine()->hasCaptureDemand());
        QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Parity Task 28 (R-R3-49, A11): a Core with a TX analyzer is told at
    // start that this window takes its transmit display; the pan's
    // subscribe carries its transmit window; while the Core is keyed the
    // transmit context and its frames are handed on, and the pan keeps its
    // receive context; the fall brings receive frames back. A Core without
    // one sees today's start and subscribe.
    // NSDC v2 (sections 9.2 and 9.3): a mini display on a window whose start
    // agreed v2 asks no keyframe for its context and is sent v2 by its Core
    // (v1 and a keyframe as before when the window withholds the feature).
    // A released frame whose trace part was lost has full-length planes, so
    // the mini display's check takes it like any other: its row with the
    // trace carried, then the next frame.
    void miniDisplayTakesARowOnlyPartialFrame_data()
    {
        QTest::addColumn<bool>("agreed");
        QTest::newRow("Core offers v2") << true;
        QTest::newRow("window withholds displayCodec") << false;
    }

    void miniDisplayTakesARowOnlyPartialFrame()
    {
        QFETCH(bool, agreed);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        SliceModel* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double sourceHz = station.streamCentreHz(stream);
        slice->setFrequency(sourceHz - 12'000.0);
        StationServer server(&station, settings,
            NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        if (!agreed) { client.withholdFeatureForTest(QByteArrayLiteral("displayCodec")); }
        PanadapterStack stack; // No main pan: only the mini display subscribes.
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy miniFrames(&gui, &RemoteMediaController::miniDisplayFrame);
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QTRY_COMPARE(client.capabilities().miniDisplayVersion, 1);
        QCOMPARE(client.capabilities().displayCodecVersion, agreed ? 2 : 0);
        const QJsonObject start = lastControl(outbound, QStringLiteral("start"));
        QVERIFY(!start.isEmpty());
        QCOMPARE(start.contains(QStringLiteral("displayCodecVersion")), agreed);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        gui.setMiniDisplaySlices({sliceId});
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QVector<float> iq(4096);
        for (int n = 0; n < iq.size() / 2; ++n) {
            const double phase = 2.0 * std::numbers::pi * (-10'000.0 / 192'000.0) * n;
            iq[2 * n] = float(0.02 * std::cos(phase));
            iq[2 * n + 1] = float(0.02 * std::sin(phase));
        }
        const auto feed = [&] {
            for (int block = 0; block < kFeedBlocksPerPoll; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            }
        };
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            feed();
            return miniFrames.size() >= 3;
        }()), 5'000);
        // Feeding stops; whatever the Core still sends arrives.
        QTest::qWait(300);
        const qsizetype sent = sourceMedia->displayPackets.size();
        QTest::qWait(150);
        QCOMPARE(sourceMedia->displayPackets.size(), sent);

        // The Core sends the version the start agreed, and the mini
        // display's context asked for a keyframe only without v2.
        DisplayCodecDecoder reader;
        std::optional<DisplayCodecFrame> last;
        int displays = 0;
        for (const QByteArray& packet : sourceMedia->displayPackets) {
            if (!packet.startsWith("NSDC")) { continue; }
            ++displays;
            QCOMPARE(int(quint8(packet.at(4))), agreed ? 2 : 1);
            const DisplayCodecDecodeResult decoded = reader.decode(packet);
            if (decoded.frameCount >= 1) {
                last = decoded.frameCount == 2 ? decoded.secondFrame : decoded.frame;
            }
        }
        QVERIFY(displays > 0);
        QVERIFY(last);
        if (agreed) {
            QCOMPARE(countControl(outbound, QStringLiteral("keyframe")), 0);
        } else {
            QTRY_VERIFY(countControl(outbound, QStringLiteral("keyframe")) >= 1);
        }
        const DisplayCodecContext context = last->context;
        QVERIFY(context.traceSamples > 0);
        QCOMPARE(context.waterfallSamples, context.traceSamples);
        QCOMPARE(context.wideSamples, quint16(0));
        const QVector<float> shownTrace = miniFrames.last().at(1).value<QVector<float>>();
        QCOMPARE(shownTrace.size(), int(context.traceSamples));
        const RemoteDisplayTelemetry before = gui.displayTelemetry();

        // The next two frames as the Core would send them, one row period
        // apart: the first loses its trace part.
        quint32 noiseState = 7;
        const float stepDbm = (context.maxDbm - context.minDbm) / 255.0f;
        const auto noise = [&] {
            QVector<float> values(context.traceSamples);
            for (float& value : values) {
                noiseState = noiseState * 1664525U + 1013904223U;
                value = context.minDbm
                    + (context.maxDbm - context.minDbm) * float(noiseState >> 8) / float(1U << 24);
            }
            return values;
        };
        const auto frameAfter = [&](quint32 step) {
            DisplayCodecFrame frame;
            frame.context = context;
            frame.encoderSequence = last->encoderSequence + step;
            frame.producerTimestamp = last->producerTimestamp + quint64(step) * 33'333'333ULL;
            frame.waterfallAdvance = true;
            frame.traceDbm = noise();
            frame.waterfallDbm = noise();
            return frame;
        };
        const DisplayCodecFrame rowOnly = frameAfter(1);
        const DisplayCodecFrame whole = frameAfter(2);
        const std::vector<QByteArray> rowOnlyMessages =
            encodeDisplayCodecV2(rowOnly, DisplayCodecEncoder::kMaxEncodedBytes).messages;
        const std::vector<QByteArray> wholeMessages =
            encodeDisplayCodecV2(whole, DisplayCodecEncoder::kMaxEncodedBytes).messages;
        QCOMPARE(rowOnlyMessages.size(), size_t(2));
        const auto near = [stepDbm](const QVector<float>& got, const QVector<float>& sent) {
            if (got.size() != sent.size()) { return false; }
            for (int i = 0; i < got.size(); ++i) {
                if (std::abs(got.at(i) - sent.at(i)) > stepDbm) { return false; }
            }
            return true;
        };
        const auto shownIndex = [&](const QVector<float>& row, qsizetype from) {
            for (qsizetype i = from; i < miniFrames.size(); ++i) {
                if (near(miniFrames.at(i).at(2).value<QVector<float>>(), row)) { return i; }
            }
            return qsizetype(-1);
        };
        const qsizetype from = miniFrames.size();
        sinkMedia->deliver(rowOnlyMessages.front()); // the trace part is lost
        for (const QByteArray& message : wholeMessages) {
            sinkMedia->deliver(message);
        }
        QTRY_VERIFY_WITH_TIMEOUT(shownIndex(whole.waterfallDbm, from) >= 0, 5'000);
        const qsizetype rowAt = shownIndex(rowOnly.waterfallDbm, from);
        const qsizetype wholeAt = shownIndex(whole.waterfallDbm, from);
        QVERIFY(rowAt >= 0);
        QVERIFY(rowAt < wholeAt);
        const QList<QVariant>& rowShown = miniFrames.at(rowAt);
        QCOMPARE(rowShown.at(0).toInt(), sliceId);
        QVERIFY(rowShown.at(6).toBool()); // the waterfall advances
        const QVector<float> carried = rowShown.at(1).value<QVector<float>>();
        QCOMPARE(carried.size(), int(context.traceSamples));
        if (agreed) {
            // The trace carried is the last one the Core sent.
            QCOMPARE(carried, shownTrace);
        }
        QVERIFY(near(miniFrames.at(wholeAt).at(1).value<QVector<float>>(), whole.traceDbm));
        const RemoteDisplayTelemetry after = gui.displayTelemetry();
        QCOMPARE(after.partsLost - before.partsLost, quint64(1));
        QCOMPARE(after.messagesLost, before.messagesLost);
        QCOMPARE(after.keyframeWaits, before.keyframeWaits);
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void transmitDisplayIsDeclaredAndHandedOn_data()
    {
        QTest::addColumn<bool>("analyzer");
        QTest::newRow("core with a TX analyzer") << true;
        QTest::newRow("core without one") << false;
    }

    void transmitDisplayIsDeclaredAndHandedOn()
    {
        QFETCH(bool, analyzer);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        const double centre = station.streamCentreHz(stream);
        std::unique_ptr<TxAnalyzer> txAnalyzer;
        if (analyzer) {
            txAnalyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId);
            station.setTxAnalyzer(txAnalyzer.get());
        }
        const auto clearAnalyzer = qScopeGuard([&station] { station.setTxAnalyzer(nullptr); });
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("only"));
        applet->setActiveSliceIndex(sliceId);
        SpectrumWidget* widget = applet->spectrumWidget();
        widget->setDisplayWindowPreservingHistory(centre, 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&gui, &RemoteMediaController::displayFrameReceived);
        QSignalSpy transmitContexts(&gui, &RemoteMediaController::transmitContextReceived);
        QSignalSpy transmitFrames(&gui, &RemoteMediaController::transmitFrameReceived);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QCOMPARE(client.capabilities().txDisplayVersion, analyzer ? 3 : 0);
        QCOMPARE(gui.txDisplayNegotiated(), analyzer);
        const QJsonObject start = lastControl(outbound, QStringLiteral("start"));
        QCOMPARE(start.contains(QStringLiteral("txDisplayVersion")), analyzer);
        // Parity Task 31: a Core at 3 is told 3 (its subscribes may then
        // carry `duplex`, sent only while DUP is on).
        if (analyzer) {
            QCOMPARE(start.value(QStringLiteral("txDisplayVersion")).toInt(), 3);
        }
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QCOMPARE(daemon.txDisplayNegotiated(), analyzer);
        const QJsonObject asked = lastControl(outbound, QStringLiteral("subscribe"));
        QCOMPARE(asked.contains(QStringLiteral("txMinDbm")), analyzer);
        QCOMPARE(asked.contains(QStringLiteral("txMaxDbm")), analyzer);
        QVERIFY(!asked.contains(QStringLiteral("duplex")));
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        const auto feed = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() > 0), 5000);
        if (!analyzer) {
            for (const QJsonObject& context : controlsFor(inbound, QStringLiteral("context"))) {
                QVERIFY(!context.contains(QStringLiteral("transmit")));
            }
            QVERIFY(!gui.micLineOpen());
            QVERIFY(!remote.audioEngine()->hasCaptureDemand());
            QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
            client.disconnectFromStation(QStringLiteral("test complete"));
            return;
        }
        // The transmit window: the pan's transmit grid widened by its
        // transmit waterfall levels, rounded outward to a tenth of a dB.
        const double gridMax = widget->transmitRefLevel();
        const double gridMin = gridMax - widget->transmitDynamicRange();
        QCOMPARE(asked.value(QStringLiteral("txMinDbm")).toDouble(),
                 std::floor(std::min(gridMin, double(widget->txWfLowLevel())) * 10.0) / 10.0);
        QCOMPARE(asked.value(QStringLiteral("txMaxDbm")).toDouble(),
                 std::ceil(std::max(gridMax, double(widget->txWfHighLevel())) * 10.0) / 10.0);
        const double receiveRate = widget->sampleRate();

        // Keyed at the Core (its MoxController, the receive-only pre-check
        // lifted): the transmit context is handed on, not applied.
        MoxController* mox = station.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QTRY_COMPARE(transmitContexts.count(), 1);
        QCOMPARE(transmitContexts.last().at(0).toString(), applet->panId());
        const auto context = transmitContexts.last().at(1).value<SpectrumContextMessage>();
        QCOMPARE(context.transmit, std::optional<bool>(true));
        QCOMPARE(context.sourceCentreHz, double(station.txFrequencyForSlice(slice)));
        QCOMPARE(widget->sampleRate(), receiveRate);
        const int receivedBefore = frames.count();
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QVector<float> trace(context.traceSamples, -30.0f);
            emit txAnalyzer->txFftReady(-1, trace);
            emit txAnalyzer->txWaterfallReady(-1, trace);
            feed();
            return transmitFrames.count() > 0;
        }()), 5000);
        const auto frame = transmitFrames.last().at(1).value<DisplayCodecFrame>();
        QCOMPARE(transmitFrames.last().at(0).toString(), applet->panId());
        QCOMPARE(frame.traceDbm.size(), context.traceSamples);
        QVERIFY(std::abs(frame.traceDbm.first() + 30.0f) < 1.0f);
        QVERIFY(frames.count() > receivedBefore);
        QCOMPARE(widget->sampleRate(), receiveRate);

        // The fall: the receive context again, and receive frames draw.
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            feed();
            const QList<QJsonObject> contexts = controlsFor(inbound, QStringLiteral("context"));
            return !contexts.isEmpty()
                && contexts.last().value(QStringLiteral("transmit")) == QJsonValue(false);
        }()), 5000);
        const int transmitFramesAtFall = transmitFrames.count();
        const int framesAtFall = frames.count();
        QTRY_VERIFY_WITH_TIMEOUT((feed(), frames.count() > framesAtFall + 2), 5000);
        QCOMPARE(transmitFrames.count(), transmitFramesAtFall);
        QVERIFY(!gui.micLineOpen());
        QVERIFY(!remote.audioEngine()->hasCaptureDemand());
        QCOMPARE(remote.audioEngine()->captureHelperProcessIdForTest(), qint64(0));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-08/37 (final review I4): the budget nereusd computes when adaptation
    // is on and nothing is configured reaches only an app that knows the
    // budget reason. An older app keeps legacy mode exactly: no budget, no
    // allocation results, no status line. A current app plans in budget mode
    // and, at the quality it asked for with nothing waiting, shows no line
    // either.
    void computedCeilingLeavesOlderAppsInLegacyMode_data()
    {
        QTest::addColumn<int>("minor");
        QTest::newRow("minor 10") << int(kDisplayBudgetReasonSessionProtocolMinor - 1);
        QTest::newRow("minor 11") << int(kDisplayBudgetReasonSessionProtocolMinor);
    }

    void computedCeilingLeavesOlderAppsInLegacyMode()
    {
        QFETCH(int, minor);
        const bool budgetMode = minor >= kDisplayBudgetReasonSessionProtocolMinor;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        // What DaemonApp does with display_adaptive on and no limits set.
        server.setDisplayBudgetForReasonPeersOnly(true);
        QVERIFY(server.setDisplayBudgetLimits(DisplayLoadGovernor::computedCeiling()));
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("only"));
        applet->setActiveSliceIndex(sliceId);
        applet->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        QSignalSpy frames(&gui, &RemoteMediaController::displayFrameReceived);
        auto* stationLink = new Test::RewritingTransport(QStringLiteral("station"),
                                                         quint16(minor));
        auto* clientLink = new Test::RewritingTransport(QStringLiteral("client"),
                                                        quint16(minor));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QCOMPARE(client.agreedMinor(), quint16(minor));
        QCOMPARE(server.displayBudgetLimits().has_value(), budgetMode);
        QCOMPARE(server.buildCapabilities().displayBudget.has_value(), budgetMode);
        QCOMPARE(client.remoteDisplayBudgetLimits().has_value(), budgetMode);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QTRY_COMPARE(daemon.activeEndpointCount(), 1);
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        const auto painted = [&] {
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            return frames.count() > 0
                && !applet->spectrumWidget()->renderedPixels().isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(painted(), 5000);
        QCOMPARE(countControl(inbound, QStringLiteral("allocation-result")) > 0, budgetMode);
        QTRY_VERIFY2(applet->remoteDisplayStatus().isEmpty(),
                     qPrintable(applet->remoteDisplayStatus()));
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // C1 (R-R3-01, R-R3-08, R-R3-09, R-R3-37): in budget mode a pan Core
    // grants fewer pixels than it asked for still paints. A minor 9 GUI
    // accepts the smaller granted charge; a minor 8 GUI, which knows nothing
    // of grants, is charged exactly what it requested, as before.
    void budgetModePaintsWhenCoreGrantsFewerPixels_data()
    {
        QTest::addColumn<int>("minor");
        QTest::addColumn<bool>("shared");
        QTest::addColumn<int>("leave");
        QTest::addColumn<QString>("sizingMode");
        QTest::addColumn<double>("cropSpanHz");
        QTest::addColumn<bool>("normalize");
        const int grant = int(kRemoteSpectrumGrantSessionProtocolMinor);
        QTest::newRow("minor 9 crop past the source edge") << grant << false << kStays << QStringLiteral("Auto") << 48000.0 << false;
        QTest::newRow("minor 9 shared engine") << grant << true << kStays << QStringLiteral("Auto") << 48000.0 << false;
        QTest::newRow("minor 8 crop past the source edge") << grant - 1 << false << kStays << QStringLiteral("Auto") << 48000.0 << false;
        QTest::newRow("minor 8 shared engine") << grant - 1 << true << kStays << QStringLiteral("Auto") << 48000.0 << false;
        // R-R3-01, R-R3-08, R-R3-37: the pan that sized the engine leaves.
        QTest::newRow("minor 9 shared engine, first pan leaves")
            << grant << true << kLeaves << QStringLiteral("Auto") << 48000.0 << false;
        QTest::newRow("minor 9 shared engine, first pan leaves, Core refuses more")
            << grant << true << kLeavesCoreRefuses << QStringLiteral("Auto") << 48000.0 << false;
        for (int minor : {grant, grant - 1}) {
            const QByteArray name = QStringLiteral("minor %1 normalized full-span source edge")
                .arg(minor).toUtf8();
            QTest::newRow(name.constData()) << minor << false << kStays
                << QStringLiteral("Auto") << 192000.0 << true;
        }
        for (const QString& mode : {QStringLiteral("Fixed"), QStringLiteral("HzPerBin")}) {
            for (int minor : {grant, grant - 1}) {
                const QByteArray name = QStringLiteral("minor %1 source edge %2").arg(minor).arg(mode).toUtf8();
                QTest::newRow(name.constData()) << minor << false << kStays << mode << 48000.0 << false;
            }
        }
    }

    void budgetModePaintsWhenCoreGrantsFewerPixels()
    {
        QFETCH(int, minor);
        QFETCH(bool, shared);
        QFETCH(int, leave);
        QFETCH(QString, sizingMode);
        constexpr int kSourceSampleRateHz = 192000;
        QFETCH(double, cropSpanHz);
        QFETCH(bool, normalize);
        constexpr double kCropOverlapHz = 1000.0;
        const bool grantAgreed = minor >= kRemoteSpectrumGrantSessionProtocolMinor;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFft = appSettings.contains(QStringLiteral("DisplayFftSize"));
        const QVariant savedFft = appSettings.value(QStringLiteral("DisplayFftSize"));
        const QString modeKey = QLatin1String(ControlRanges::kDisplayFftSizingModeKey);
        const QString targetKey = QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey);
        const bool hadMode = appSettings.contains(modeKey);
        const QVariant savedMode = appSettings.value(modeKey);
        const bool hadTarget = appSettings.contains(targetKey);
        const QVariant savedTarget = appSettings.value(targetKey);
        appSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("4096"));
        appSettings.setValue(modeKey, sizingMode);
        appSettings.setValue(targetKey, QStringLiteral("8"));
        const auto restoreFft = qScopeGuard([&] {
            if (hadFft) { appSettings.setValue(QStringLiteral("DisplayFftSize"), savedFft); }
            else { appSettings.remove(QStringLiteral("DisplayFftSize")); }
            if (hadMode) { appSettings.setValue(modeKey, savedMode); }
            else { appSettings.remove(modeKey); }
            if (hadTarget) { appSettings.setValue(targetKey, savedTarget); }
            else { appSettings.remove(targetKey); }
        });
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice();
        auto* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const double centre = station.streamCentreHz(stream);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY(server.setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
        // Refused row: Core's state moves ahead of the GUI's view. When the
        // survivor asks again, Core already holds PureSignal display, which
        // the GUI has not heard of yet. Connected before Core's own handler,
        // so it runs first.
        const auto armedEndpoint = std::make_shared<quint32>(0);
        connect(&server, &StationServer::mediaControlReceived, &station,
            [&station, armedEndpoint](const QJsonObject& control) {
                if (*armedEndpoint != 0
                    && control.value(QStringLiteral("op")) == QLatin1String("subscribe")
                    && quint32(control.value(QStringLiteral("endpointId")).toDouble())
                        == *armedEndpoint) {
                    *armedEndpoint = 0;
                    station.pureSignalFacade()->setRemoteAmpViewSubscribed(true);
                }
            });
        QPointer<DisplayTransport> sourceMedia;
        DaemonMediaController daemon(&server, &station, nullptr,
            [&sourceMedia](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        // 2026-10-16: the pans are placed by applyLayout, as MainWindow
        // places them. addPanadapter alone leaves a pan after the stack's
        // own pan-0 outside the splitter, at its own default width rather
        // than the stack's: 400 px, which the 320 px narrow minimum
        // (PanadapterApplet) made 320 and the crop row's 4 kHz overlap
        // (about 342 bins) then outnumbered the 284 pixels it asked for.
        stack.applyLayout(QStringLiteral("1"), {QStringLiteral("first")});
        auto* first = stack.panadapter(QStringLiteral("first"));
        QVERIFY(first);
        first->setActiveSliceIndex(sliceId);
        first->spectrumWidget()->setExtendedViewAllowed(false);
        // 2026-10-16: placed in the stack, a pan's waterfall runs, and with
        // waterfall AGC its dBm window follows the levels AGC sets from the
        // first frames and asks again (as the C-Tune rows note), which the
        // refused row would count as a second ask. This test is about
        // pixel grants, so its pans use the stored levels.
        first->spectrumWidget()->setWfAgcEnabled(false);
        // Shared: a "fine" pan that sizes the engine. Edge: a crop that
        // overlaps the source by 4 kHz, so fewer bins than pixels exist.
        // 2026-10-10: the shared pan's span was 12 kHz, which the per-pixel
        // rule sized at 8192 (192000 x pixels / 12000 = 16 points a pixel on
        // a pan 257 to 512 px wide). Under the shared rule
        // (ControlRanges::planAutoZoomFft) 12 kHz is 65536, whose points in
        // the second pan's 6 kHz outnumber its pixels; 96 kHz gives the 8192
        // engine again (4096 x 192000 / 96000), and the second pan's 6 kHz
        // then has 256 of its points, fewer than its pixels, as before.
        if (shared) {
            first->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 96000);
        } else {
            const double edgeCentreHz = centre + double(kSourceSampleRateHz) * 0.5
                + cropSpanHz * 0.5 - kCropOverlapHz;
            first->spectrumWidget()->setDisplayWindowPreservingHistory(edgeCentreHz,
                                                                        cropSpanHz);
        }
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        // The pan fills its stack, so it asks for the stack's width.
        QTRY_COMPARE(first->spectrumWidget()->width(), stack.width());
        first->spectrumWidget()->setDispNormalize(normalize);
        if (normalize) {
            first->spectrumWidget()->setSpectrumDetector(SpectrumDetector::Average);
            first->spectrumWidget()->setDbmRange(-140.0f, 0.0f);
            first->spectrumWidget()->setWfAgcEnabled(false);
            first->spectrumWidget()->setWfUseSpectrumMinMax(false);
            first->spectrumWidget()->setClarityActive(false);
            first->spectrumWidget()->setWfLowThreshold(-140.0f);
            first->spectrumWidget()->setWfHighThreshold(-70.0f);
        }
        qint64 plannerNow = 1000;
        QPointer<DisplayTransport> sinkMedia;
        RemoteMediaController gui(&client, &remote, &stack, nullptr,
            [&sinkMedia](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            }, [&plannerNow] { return plannerNow; });
        QSignalSpy outbound(&server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&client, &StationClient::mediaControlReceived);
        auto* stationLink = new Test::RewritingTransport(QStringLiteral("station"),
                                                         quint16(minor));
        auto* clientLink = new Test::RewritingTransport(QStringLiteral("client"),
                                                        quint16(minor));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());
        QTRY_VERIFY(sourceMedia && sinkMedia);
        QCOMPARE(gui.spectrumGrantNegotiated(), grantAgreed);
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        // Enough samples per poll for the longest FFT these rows use.
        const auto feed = [&] {
            for (int block = 0; block < 32; ++block) {
                QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                    Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
            }
        };
        const auto paints = [&](PanadapterApplet* applet) {
            feed();
            return !applet->spectrumWidget()->renderedPixels().isEmpty();
        };
        QTRY_VERIFY2_WITH_TIMEOUT(paints(first), qPrintable(first->remoteDisplayStatus()), 5000);

        PanadapterApplet* limited = first;
        if (shared) {
            // A deeper zoom on the same receiver asks for a longer "fine"
            // FFT than the first pan's engine; it is granted that engine.
            stack.applyLayout(QStringLiteral("2v"),
                              {QStringLiteral("first"), QStringLiteral("second")});
            limited = stack.panadapter(QStringLiteral("second"));
            QVERIFY(limited);
            limited->setActiveSliceIndex(sliceId);
            limited->spectrumWidget()->setExtendedViewAllowed(false);
            limited->spectrumWidget()->setWfAgcEnabled(false);
            limited->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 6000);
            QTRY_COMPARE(limited->spectrumWidget()->width(), stack.width());
            QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        }
        QTRY_VERIFY2_WITH_TIMEOUT(paints(limited), qPrintable(limited->remoteDisplayStatus()),
                                  5000);
        QVERIFY2(gui.panDisplayState(limited->panId()).phase != PanDisplayState::Phase::Stalled,
                 qPrintable(limited->remoteDisplayStatus()));

        // The limited pan really was granted fewer pixels than it asked for.
        if (!shared) {
            // A clipped answer is not a new zoom. Exercise several planner
            // turns so their FFT stays sized for the original requested span.
            for (int turn = 0; turn < 3; ++turn) {
                plannerNow += 200;
                gui.planNowForTest();
                QCoreApplication::processEvents();
            }
            const QList<QJsonObject> requests =
                controlsFor(outbound, QStringLiteral("subscribe"));
            QVERIFY(!requests.isEmpty());
            QCOMPARE(first->spectrumWidget()->normalizeActive(), normalize);
            const double initialMaxDbm = requests.constFirst().value(QStringLiteral("maxDbm")).toDouble();
            const DisplayFftSizing sizing = readDisplayFftSizing(appSettings);
            const int expectedSize = sizing.plan(kSourceSampleRateHz, cropSpanHz).size;
            for (const QJsonObject& request : requests) {
                qInfo().noquote() << "source-edge planning" << sizingMode
                    << "answeredSpan" << first->spectrumWidget()->bandwidth()
                    << "request" << QJsonDocument(request).toJson(QJsonDocument::Compact);
                QCOMPARE(request.value(QStringLiteral("spanHz")).toDouble(), cropSpanHz);
                QCOMPARE(request.value(QStringLiteral("fftSize")).toInt(), expectedSize);
                if (normalize) {
                    // The clipped answer must not change the source bin width
                    // used for normalization or narrow away its pan headroom.
                    QCOMPARE(request.value(QStringLiteral("maxDbm")).toDouble(), initialMaxDbm);
                }
            }
            const QJsonObject& edgeRequest = requests.constLast();
            const int plannedFftSize = edgeRequest.value(QStringLiteral("fftSize")).toInt();
            QVERIFY(plannedFftSize > 0);
            const int edgeBinUpperBound = int(std::ceil(
                kCropOverlapHz * double(plannedFftSize) / double(kSourceSampleRateHz))) + 1;
            QVERIFY(edgeRequest.value(QStringLiteral("pixels")).toInt() > edgeBinUpperBound);
        }
        bool reduced = false;
        quint32 limitedEndpoint = 0;
        QJsonObject limitedRequest;
        int limitedGrantedPixels = 0;
        for (const QJsonObject& context : controlsFor(inbound, QStringLiteral("context"))) {
            const auto decoded = decodeRemoteSpectrumContext(context, grantAgreed);
            QVERIFY(decoded.has_value());
            for (const QJsonObject& request : controlsFor(outbound, QStringLiteral("subscribe"))) {
                if (quint32(request.value(QStringLiteral("endpointId")).toDouble())
                        == decoded->endpointId
                    && decoded->traceSamples
                        < request.value(QStringLiteral("pixels")).toInt()) {
                    reduced = true;
                    limitedEndpoint = decoded->endpointId;
                    limitedRequest = request;
                    limitedGrantedPixels = decoded->traceSamples;
                    if (grantAgreed) {
                        QCOMPARE(decoded->grant->limit, shared
                            ? SpectrumLimitReason::SharedEngine
                            : SpectrumLimitReason::SourceBins);
                    }
                }
            }
        }
        QVERIFY(reduced);
        QCOMPARE(countControl(inbound, QStringLiteral("rejected")), 0);

        if (leave != kStays) {
            const auto requestsFromLimited = [&] {
                int count = 0;
                for (const QJsonObject& request
                     : controlsFor(outbound, QStringLiteral("subscribe"))) {
                    if (quint32(request.value(QStringLiteral("endpointId")).toDouble())
                        == limitedEndpoint) { ++count; }
                }
                return count;
            };
            const int requestedPixels = limitedRequest.value(QStringLiteral("pixels")).toInt();
            QVERIFY(limitedGrantedPixels < requestedPixels);
            if (leave == kLeavesCoreRefuses) {
                // Core takes PureSignal display just as the survivor asks
                // again (armedEndpoint above): the GUI's own budget check
                // passes, and Core's admission refuses the survivor any more
                // pixels. The limits leave room for the survivor's granted
                // pixels plus PureSignal, and for both pans as they were.
                const int fps = limitedRequest.value(QStringLiteral("fps")).toInt();
                const bool wide = limitedRequest.value(QStringLiteral("wideSpanFactor"))
                    .toDouble() > 1.0;
                const auto granted = spectrumDisplayCost(limitedGrantedPixels, fps, wide);
                const auto full = spectrumDisplayCost(requestedPixels, fps, wide);
                QVERIFY(granted && full);
                quint64 others = 0;
                for (const QJsonObject& request
                     : controlsFor(outbound, QStringLiteral("subscribe"))) {
                    if (quint32(request.value(QStringLiteral("endpointId")).toDouble())
                        != limitedEndpoint) {
                        const auto cost = spectrumDisplayCost(
                            request.value(QStringLiteral("pixels")).toInt(),
                            request.value(QStringLiteral("fps")).toInt(),
                            request.value(QStringLiteral("wideSpanFactor")).toDouble() > 1.0);
                        QVERIFY(cost);
                        others = std::max(others, cost->charge.applicationBytesPerSecond);
                    }
                }
                const quint64 ps3 = ps3DisplayCharge().applicationBytesPerSecond;
                const quint64 limit = granted->charge.applicationBytesPerSecond + ps3;
                QVERIFY(full->charge.applicationBytesPerSecond
                        > granted->charge.applicationBytesPerSecond);
                QVERIFY(others + full->charge.applicationBytesPerSecond <= limit);
                const int before = requestsFromLimited();
                QVERIFY(server.setDisplayBudgetLimits({limit, 10'000'000, 2}));
                QTRY_COMPARE(client.remoteDisplayBudgetLimits()->generation, quint32(2));
                QTest::qWait(100);
                QCOMPARE(requestsFromLimited(), before);
                *armedEndpoint = limitedEndpoint;
            }
            const int requestsBeforeLeave = requestsFromLimited();
            stack.removePanadapter(QStringLiteral("first"));
            QTRY_COMPARE(daemon.activeEndpointCount(), 1);
            const auto survivorContext = [&]() -> std::optional<SpectrumContextMessage> {
                std::optional<SpectrumContextMessage> latest;
                for (const QJsonObject& context
                     : controlsFor(inbound, QStringLiteral("context"))) {
                    const auto decoded = decodeRemoteSpectrumContext(context, grantAgreed);
                    if (decoded && decoded->endpointId == limitedEndpoint) { latest = decoded; }
                }
                return latest;
            };
            if (leave == kLeaves) {
                // The same request goes out again and Core grants all of it.
                // Core renews the survivor's context on the engine's next frame.
                QTRY_VERIFY_WITH_TIMEOUT(
                    (feed(), requestsFromLimited() == requestsBeforeLeave + 1), 5000);
                const QJsonObject again = controlsFor(outbound, QStringLiteral("subscribe")).last();
                QCOMPARE(quint32(again.value(QStringLiteral("endpointId")).toDouble()),
                         limitedEndpoint);
                QCOMPARE(again.value(QStringLiteral("pixels")).toInt(), requestedPixels);
                QTRY_VERIFY_WITH_TIMEOUT((feed(), survivorContext()
                    && survivorContext()->traceSamples == requestedPixels), 5000);
                QCOMPARE(survivorContext()->grant->grantedPixels, requestedPixels);
                QCOMPARE(survivorContext()->grant->limit, SpectrumLimitReason::None);
                QTRY_VERIFY2_WITH_TIMEOUT(paints(limited),
                    qPrintable(limited->remoteDisplayStatus()), 5000);
                QVERIFY2(gui.panDisplayState(limited->panId()).zoomLimit
                             == PanDisplayState::ZoomLimit::None,
                         qPrintable(limited->remoteDisplayStatus()));
            } else {
                // Core refuses once; the refusal is not asked again.
                const auto refusals = [&] {
                    int count = 0;
                    for (const QJsonObject& result
                         : controlsFor(inbound, QStringLiteral("allocation-result"))) {
                        if (quint32(result.value(QStringLiteral("endpointId")).toDouble())
                                == limitedEndpoint
                            && !result.value(QStringLiteral("accepted")).toBool()) {
                            ++count;
                        }
                    }
                    return count;
                };
                QTRY_COMPARE_WITH_TIMEOUT((feed(), refusals()), 1, 5000);
                QCOMPARE(requestsFromLimited(), requestsBeforeLeave + 1);
                QElapsedTimer settle;
                settle.start();
                while (settle.elapsed() < 1000) {
                    feed();
                    QTest::qWait(20);
                }
                QCOMPARE(requestsFromLimited(), requestsBeforeLeave + 1);
                QCOMPARE(refusals(), 1);
                for (const QJsonObject& result
                     : controlsFor(inbound, QStringLiteral("allocation-result"))) {
                    if (!result.value(QStringLiteral("accepted")).toBool()) {
                        QCOMPARE(result.value(QStringLiteral("reason")).toString(),
                                 QStringLiteral("The Core's display limit has no room left."));
                    }
                }
                // The pan keeps painting what Core already granted.
                QVERIFY(paints(limited));
                QVERIFY(survivorContext());
                QCOMPARE(survivorContext()->traceSamples, limitedGrantedPixels);
            }
        }
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23: a remote window plays the Core's audio on whatever speaker
    // format this computer's Devices page set: 44.1 or 96 kHz stereo, or a
    // mono device, over the real encrypted session. The speaker plays at
    // its own rate and the slice tones come out at their own pitch; a mono
    // speaker hears both slices. No refusal and no problem is shown.
    void playsOnTheSpeakerFormatThisComputerChose_data()
    {
        QTest::addColumn<int>("rate");
        QTest::addColumn<int>("channels");
        QTest::newRow("44.1 kHz stereo") << 44100 << 2;
        QTest::newRow("96 kHz stereo") << 96000 << 2;
        QTest::newRow("48 kHz mono") << 48000 << 1;
    }
    void playsOnTheSpeakerFormatThisComputerChose()
    {
        using State = RemoteAudioStatus::State;
        QFETCH(int, rate);
        QFETCH(int, channels);
        Test::RemoteAudioSessionHarness h;
        QVERIFY(h.remoteBus->open(AudioFormat{rate, channels, AudioFormat::Sample::Float32}));
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker; // 10 ms of the speaker's own rate at a time
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h, rate] { h.remoteBus->render(rate / 100); });
        source.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const qsizetype heardAtPlaying = h.remoteBus->heard.size() / channels;
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / channels >= heardAtPlaying + rate, 10000);
        source.stop();
        speaker.stop();
        QCOMPARE(errors.size(), 0);
        QCOMPARE(remoteMedia.audioStatus().state, State::Playing);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        const RemoteAudioReceiverTelemetry playback = remoteMedia.audioTelemetry();
        QVERIFY(playback.running);
        QVERIFY(playback.deviceConsumedFrames > 0);
        QVERIFY(playback.speakerQueuedMs && *playback.speakerQueuedMs < 100.0);

        // The last half second heard: each slice's tone at its own pitch
        // (at a wrong rate it would be heard at pitch x 48 kHz / rate).
        const QVector<float> heard = h.remoteBus->heard;
        const qint64 half = rate / 2;
        const double toneA = Test::RemoteAudioSessionHarness::kSliceAToneHz;
        const double toneB = Test::RemoteAudioSessionHarness::kSliceBToneHz;
        const int channelA = 0;
        const int channelB = channels == 2 ? 1 : 0;
        const double a = lastToneAmplitude(heard, channels, channelA, toneA, rate, half);
        const double b = lastToneAmplitude(heard, channels, channelB, toneB, rate, half);
        qInfo().noquote() << QStringLiteral("%1 Hz %2 ch: slice A %3, slice B %4")
                                 .arg(rate).arg(channels).arg(a, 0, 'f', 4).arg(b, 0, 'f', 4);
        // Heard stereo levels at 48 kHz are about 0.062 and 0.042; a mono
        // speaker hears each at about half (tst_remote_audio_receiver
        // checks the mix exactly).
        QVERIFY2(a > 0.02 && b > 0.015, qPrintable(QStringLiteral("%1 %2").arg(a).arg(b)));
        if (channels == 2) {
            // Each slice stays on its side of the pan.
            const double aRight = lastToneAmplitude(heard, channels, 1, toneA, rate, half);
            QVERIFY2(aRight < a / 3.0, qPrintable(QStringLiteral("%1 %2").arg(aRight).arg(a)));
        }
        if (rate != 48000) {
            const double wrongA = lastToneAmplitude(heard, channels, channelA,
                                                    toneA * 48000.0 / rate, rate, half);
            QVERIFY2(wrongA < a / 10.0, qPrintable(QStringLiteral("%1 %2").arg(wrongA).arg(a)));
        }
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (2026.10.1 ship blocker): a speaker that fails is asked
    // for again on its own, on the restart backoff (1, 2, 4 s), so nobody
    // has to mute and unmute it back. The operator is told once: the
    // attempts that fail again are quiet, the problem shows until the
    // speaker plays, and it clears with nothing done here.
    void speakerFaultRetriesOnItsOwnUntilItPlays()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        int enabledContexts = 0;
        connect(&remoteMedia, &RemoteMediaController::audioContextAccepted, this, [&] {
            const std::optional<RemoteAudioContextMessage> context =
                remoteMedia.acceptedAudioContext();
            if (context && context->enabled) { ++enabledContexts; }
        });
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        // The speaker stops reporting its timing while it plays.
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        const int atFault = enabledContexts;

        // Nothing here mutes, retries or changes a device. The window asks
        // the Core again by itself, twice, and both attempts fail quietly.
        QTRY_VERIFY_WITH_TIMEOUT(enabledContexts >= atFault + 2, 8000);
        QCOMPARE(errors.size(), 1);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.retryAvailable);

        // The speaker comes back: the next automatic attempt plays, and the
        // problem clears.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        status = remoteMedia.audioStatus();
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QCOMPARE(errors.size(), 1);
        QVERIFY(!h.remote.audioEngine()->masterMuted());

        // A failure after the recovery is news: the operator is told again.
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 2, 5000);
        QCOMPARE(errors.at(1).at(0).toString(),
                 remoteAudioProblemText(RemoteAudioReceiver::Fault::SpeakerTimingUnavailable));
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        h.remoteBus->setOutputPacingAvailableForTesting(true);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (review): a device format remote playback cannot play (here
    // six channels) is told once and not asked for again: asking cannot
    // change the format. The operator's change of device or format asks
    // again, and plays.
    void aSpeakerFormatRemotePlaybackCannotPlayIsNotAskedForAgain()
    {
        using State = RemoteAudioStatus::State;
        using Fault = RemoteAudioReceiver::Fault;
        Test::RemoteAudioSessionHarness h;
        QVERIFY(h.remoteBus->open(AudioFormat{48000, 6, AudioFormat::Sample::Float32}));
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        remoteMedia.holdAudioRestartForTest(true);
        QTest::ignoreMessage(QtWarningMsg,
                             "Remote audio playback failed: Remote audio cannot play on a "
                             "speaker device set to 6 channels at 48000 Hz");
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, h.kMediaConnectionWaitMs);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.problem == Fault::SpeakerFormatUnsupported);
        QVERIFY(status.retryAvailable);
        QCOMPARE(errors.at(0).at(0).toString(),
                 remoteAudioProblemText(Fault::SpeakerFormatUnsupported));
        // No retry waits on the backoff.
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QCOMPARE(remoteMedia.audioRestartStepsHeldForTest(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, 0, RemoteAudioOffReason::ClientDisabled),
                                 5000);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());

        // The operator sets the device to stereo: that asks again, and plays.
        remoteMedia.holdAudioRestartForTest(false);
        QVERIFY(h.remoteBus->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
        emit h.remote.audioEngine()->speakersConfigChanged(
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")));
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (review): the speaker faults asking again cannot change
    // wait for the operator; a speaker that could not open is asked for
    // again on its own (the row that shows the check can fail).
    void lastingSpeakerFaultsAreNotAskedForAgain_data()
    {
        using Fault = RemoteAudioReceiver::Fault;
        QTest::addColumn<RemoteAudioReceiver::Fault>("fault");
        QTest::addColumn<bool>("retried");
        QTest::newRow("callback too large") << Fault::SpeakerCallbackTooLarge << false;
        QTest::newRow("decoder unavailable") << Fault::DecoderUnavailable << false;
        QTest::newRow("format unsupported") << Fault::SpeakerFormatUnsupported << false;
        QTest::newRow("open failed") << Fault::SpeakerOpenFailed << true;
    }
    void lastingSpeakerFaultsAreNotAskedForAgain()
    {
        using State = RemoteAudioStatus::State;
        QFETCH(RemoteAudioReceiver::Fault, fault);
        QFETCH(bool, retried);
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        remoteMedia.holdAudioRestartForTest(true);
        QTest::ignoreMessage(QtWarningMsg, "Remote audio playback failed: raised by a test");
        remoteMedia.raiseAudioFailureForTest(fault);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QCOMPARE(remoteMedia.audioRestartPendingForTest(), retried);

        remoteMedia.holdAudioRestartForTest(false);
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (review): a speaker failure's own retry ends with the
    // session. Its step, come due after the disconnect, asks for nothing,
    // on that session or the next.
    void aSpeakerFailureRetryEndsWithTheSession()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        int requests = 0;
        connect(&h.server, &StationServer::mediaControlReceived, this,
                [&](const QJsonObject& control) {
            if (control.value(QStringLiteral("op")) == QLatin1String("audio")
                && control.value(QStringLiteral("enabled")).toBool()) {
                ++requests;
            }
        });
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        // The failure's step is held, so it comes due after the session ends.
        remoteMedia.holdAudioRestartForTest(true);
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QVERIFY(remoteMedia.audioRestartPendingForTest());
        h.client.disconnectFromStation(QStringLiteral("end the session"));
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioRestartStepsHeldForTest(), 1, 10000);

        // The next session plays. The old step, released, asks for nothing:
        // a mute and unmute after it is the only request the Core sees.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const int atPlaying = requests;
        remoteMedia.holdAudioRestartForTest(false);
        QCOMPARE(remoteMedia.audioRestartStepsHeldForTest(), 0);
        const quint32 beforeMute = remoteMedia.acceptedAudioContext()->generation;
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, beforeMute,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QCOMPARE(requests, atPlaying + 1);
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16: a speaker problem is this computer's, not the network
    // path's. The window moves its media to a new connection while the
    // problem shows (it does so by itself after a silence on a direct
    // path), and the speaker playing on the new connection clears it.
    void speakerProblemClearsOncePlayingOnANewConnection()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        // The window's own retry waits until the move has finished.
        remoteMedia.holdAudioRestartForTest(true);
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);

        const QString before = remoteMedia.mediaConnectionId();
        QVERIFY(remoteMedia.replaceConnection());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.mediaConnectionId() != before
                                     && !remoteMedia.replacingConnection(),
                                 DaemonMediaController::kReplaceOverlapMs + 3000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);

        // The speaker comes back and the retry asks again: it plays on the
        // new connection, and the problem clears with nothing done here.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartStepHeldForTest(), 5000);
        remoteMedia.holdAudioRestartForTest(false);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16: the Core's audio can arrive before this computer's first
    // device list is in (on macOS 27 the Voice Isolation storm holds it for
    // many seconds). The speakers wait for the list: no problem, no toast,
    // Waiting for audio, and the window keeps asking quietly. The list's
    // arrival opens the speakers (speakersConfigChanged), which asks the
    // Core again, and audio plays.
    void speakersWaitForTheDeviceListWithoutAProblem()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        AudioStatusHistory history(remoteMedia);
        int enabledContexts = 0;
        connect(&remoteMedia, &RemoteMediaController::audioContextAccepted, this, [&] {
            const std::optional<RemoteAudioContextMessage> context =
                remoteMedia.acceptedAudioContext();
            if (context && context->enabled) { ++enabledContexts; }
        });
        // The device layer has not listed the speakers' device: playback
        // cannot start on them (the injected speaker reports no timing),
        // and the engine says why.
        AudioEngine* const engine = h.remote.audioEngine();
        engine->setSpeakersAwaitDeviceListForTest(true);
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(enabledContexts >= 1, h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        // The window asks again by itself, on the 1 s step.
        QTRY_VERIFY_WITH_TIMEOUT(enabledContexts >= 2, 5000);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::WaitingForAudio);
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QCOMPARE(errors.size(), 0);

        // The list comes in and the device layer opens the speakers. The
        // automatic step is held, so it is the open that asks again.
        remoteMedia.holdAudioRestartForTest(true);
        engine->setSpeakersAwaitDeviceListForTest(std::nullopt);
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        emit engine->speakersConfigChanged(
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")));
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QCOMPARE(errors.size(), 0);
        QVERIFY(std::none_of(history.statuses.cbegin(), history.statuses.cend(),
                             [](const RemoteAudioStatus& seen) {
                                 return seen.state == State::PlaybackProblem
                                     || seen.problem.has_value();
                             }));
        remoteMedia.holdAudioRestartForTest(false);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (review): the wait for the device list is bounded, from the
    // first failure that waited. Past it the wait shows as the speaker
    // problem, with Retry, told once; the window keeps asking quietly, and
    // the list's arrival plays.
    void aDeviceListThatNeverComesShowsTheProblemAfterTheWait()
    {
        using State = RemoteAudioStatus::State;
        using Fault = RemoteAudioReceiver::Fault;
        constexpr qint64 kWaitMs = 1500;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        int enabledContexts = 0;
        quint32 lastEnabledGeneration = 0;
        connect(&remoteMedia, &RemoteMediaController::audioContextAccepted, this, [&] {
            const std::optional<RemoteAudioContextMessage> context =
                remoteMedia.acceptedAudioContext();
            if (context && context->enabled) {
                ++enabledContexts;
                lastEnabledGeneration = context->generation;
            }
        });
        remoteMedia.setSpeakerDeviceListWaitForTest(kWaitMs);
        AudioEngine* const engine = h.remote.audioEngine();
        engine->setSpeakersAwaitDeviceListForTest(true);
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        // The window's own retry is held: the first failure is the only
        // attempt until the wait has run out.
        remoteMedia.holdAudioRestartForTest(true);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioRestartStepsHeldForTest(), 1,
                                  h.kMediaConnectionWaitMs);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::WaitingForAudio);
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QCOMPARE(errors.size(), 0);

        // At least the whole wait passes after that first failure; the held
        // retry, released, fails past the wait and shows the problem.
        QTest::qWait(int(kWaitMs) + 100);
        QTest::ignoreMessage(QtWarningMsg, kSpeakerOpenFailedLog);
        QTest::ignoreMessage(QtWarningMsg,
                             "Remote audio: this computer's audio device list is still not "
                             "in after 1500 ms");
        remoteMedia.holdAudioRestartForTest(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 10000);
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.problem == Fault::SpeakerOpenFailed);
        QVERIFY(status.retryAvailable);
        QCOMPARE(errors.at(0).at(0).toString(),
                 remoteAudioProblemText(Fault::SpeakerOpenFailed));

        // The next attempt fails again, quietly.
        const int atProblem = enabledContexts;
        QTRY_VERIFY_WITH_TIMEOUT(enabledContexts >= atProblem + 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, lastEnabledGeneration,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        QCOMPARE(errors.size(), 1);

        // The list comes in and opens the speakers: audio plays and the
        // problem clears.
        engine->setSpeakersAwaitDeviceListForTest(std::nullopt);
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        emit engine->speakersConfigChanged(
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")));
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23 (a): a real speaker loss becomes a playback problem with
    // Retry. Mute, unmute, a device change, a disabled context and the
    // window's own retries (2026-10-16) do not clear it while the speaker
    // is still gone, and the retries that fail again toast nothing.
    void speakerLossPersistsAcrossMuteDeviceChangeAndDisabledContext()
    {
        using State = RemoteAudioStatus::State;
        using Fault = RemoteAudioReceiver::Fault;
        Test::RemoteAudioSessionHarness h;
        // Both controllers go before either AudioEngine: the receiver owns
        // worker callbacks and each peer owns libdatachannel.
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        AudioStatusHistory history(remoteMedia);
        int enabledContexts = 0;
        quint32 lastEnabledGeneration = 0;
        connect(&remoteMedia, &RemoteMediaController::audioContextAccepted, this, [&] {
            const std::optional<RemoteAudioContextMessage> context =
                remoteMedia.acceptedAudioContext();
            if (context && context->enabled) {
                ++enabledContexts;
                lastEnabledGeneration = context->generation;
            }
        });
        QTimer* const statusTimer = audioStatusTimer(remoteMedia);
        QVERIFY(statusTimer);
        QCOMPARE(remoteMedia.audioStatus().state, State::NotConnected);
        QCOMPARE(remoteMedia.audioStatus().selectedOutput, QStringLiteral("System default"));
        QVERIFY(!statusTimer->isActive());
        remoteMedia.retryAudio(); // No media session: nothing to retry.
        QCOMPARE(remoteMedia.audioStatus().state, State::NotConnected);

        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(remoteMedia.audioStatus().detailNegotiated);
        QVERIFY(!remoteMedia.audioStatus().retryAvailable);
        QVERIFY(statusTimer->isActive());
        QCOMPARE(errors.size(), 0);

        // The speaker goes away mid-play and stops reporting its timing. The
        // real receiver notices through AudioEngine; nothing fakes a fault.
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.problem.has_value());
        QVERIFY(*status.problem == Fault::SpeakerTimingUnavailable
                || *status.problem == Fault::SpeakerWriteFailed);
        QVERIFY(status.retryAvailable);
        const QString toast = errors.at(0).at(0).toString();
        QCOMPARE(toast, remoteAudioProblemText(*status.problem));
        QVERIFY(!toast.contains(QLatin1Char('[')));

        // Core confirms the disable the fault sent. A disabled context
        // clears nothing, and neither do the window's own retries while the
        // speaker is gone: each fails again, quietly.
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, 0, RemoteAudioOffReason::ClientDisabled),
                                 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        // The window's own retry asks once more; wait for that attempt and
        // for the Core to confirm the disable its failure sent.
        const int atFault = enabledContexts;
        QTRY_VERIFY_WITH_TIMEOUT(enabledContexts >= atFault + 1, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, lastEnabledGeneration,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QVERIFY(statusTimer->isActive()); // The problem awaits recovery.
        QCOMPARE(errors.size(), 1);

        // Muted here: the mute is what shows, and the problem stays behind it.
        const quint32 beforeMute = remoteMedia.acceptedAudioContext()->generation;
        h.remote.audioEngine()->setMasterMuted(true);
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::MutedHere);
        QVERIFY(status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, beforeMute,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::MutedHere);

        // Unmuting asks Core again, but the speaker is still gone.
        QTest::ignoreMessage(QtWarningMsg, kSpeakerOpenFailedLog);
        h.remote.audioEngine()->setMasterMuted(false);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 2, 5000);
        QCOMPARE(errors.at(1).at(0).toString(),
                 QStringLiteral("The selected speaker device could not be opened."));
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.problem == Fault::SpeakerOpenFailed);
        QVERIFY(status.retryAvailable);

        // A device change asks again too, and names the new selection. It is
        // only the selection: the speaker is still gone.
        AppSettings::instance().setValue(QStringLiteral("audio/Speakers/DeviceName"),
                                         QStringLiteral("Desk headphones"));
        const auto restoreSelection = qScopeGuard([] {
            AppSettings::instance().setValue(QStringLiteral("audio/Speakers/DeviceName"),
                                             QString());
        });
        QTest::ignoreMessage(QtWarningMsg, kSpeakerOpenFailedLog);
        emit h.remote.audioEngine()->speakersConfigChanged(
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")));
        QCOMPARE(remoteMedia.audioStatus().selectedOutput, QStringLiteral("Desk headphones"));
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 3, 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QVERIFY(history.onlyFromFirst(State::PlaybackProblem,
                                      {State::PlaybackProblem, State::MutedHere}));

        // The problem belongs to its session, and ends with it.
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::NotConnected);
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QVERIFY(!statusTimer->isActive());
    }

    // R-R3-21 (review M5): the window's retry timer backs off. The Core's
    // audio stops (a true outage), so each context the window asks for
    // ends with no packets and asks again: 1 s after the first request
    // (already past), then 2 s, then 4 s, then 4 s again, each counted from
    // the request before. The operator's Retry starts the count over.
    void repeatedAudioRestartsBackOff()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        // The Core's display keeps running through the audio outage, as it
        // does in production, so the direct path is not silent and this
        // measures the restart backoff alone (the silence fallback has its
        // own test below).
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QElapsedTimer clock;
        clock.start();
        QList<qint64> requestMs;
        connect(&h.server, &StationServer::mediaControlReceived, this,
                [&](const QJsonObject& control) {
            if (control.value(QStringLiteral("op")) == QLatin1String("audio")
                && control.value(QStringLiteral("enabled")).toBool()) {
                requestMs << clock.elapsed();
            }
        });
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const qsizetype before = requestMs.size();
        const QString connectionId = remoteMedia.mediaConnectionId();

        // The Core has no audio to send from here on; its display goes on.
        display.timer.start();
        audio.source.stop();
        QTRY_VERIFY_WITH_TIMEOUT(requestMs.size() >= before + 4, 15000);
        QList<qint64> waits;
        for (qsizetype i = before + 1; i < before + 4; ++i) {
            waits << requestMs.at(i) - requestMs.at(i - 1);
        }
        const QString evidence = QStringLiteral("waits between requests: %1 ms").arg(
            [&] { QStringList parts; for (qint64 w : waits) parts << QString::number(w);
                  return parts.join(QStringLiteral(", ")); }());
        qInfo().noquote() << evidence;
        // 2 s, 4 s, 4 s, each from the request before. Never sooner (the
        // retry timers are Qt::PreciseTimer, which does not fire early; a
        // coarse one may, by up to 5%); a loaded machine may be later, so
        // the upper bounds only tell the steps apart.
        QVERIFY2(waits.at(0) >= 1990 && waits.at(0) < 3500, qPrintable(evidence));
        QVERIFY2(waits.at(1) >= 3990 && waits.at(1) < 6000, qPrintable(evidence));
        QVERIFY2(waits.at(2) >= 3990 && waits.at(2) < 6000, qPrintable(evidence));

        // Retry starts over: the next automatic retry is 1 s out again.
        const qsizetype beforeRetry = requestMs.size();
        remoteMedia.retryAudio();
        QTRY_VERIFY_WITH_TIMEOUT(requestMs.size() >= beforeRetry + 2, 5000);
        const qint64 afterRetry = requestMs.at(beforeRetry + 1) - requestMs.at(beforeRetry);
        QVERIFY2(afterRetry >= 990 && afterRetry < 3500, qPrintable(QString::number(afterRetry)));
        // Audio and display stayed on the connection they started on.
        QCOMPARE(remoteMedia.mediaConnectionId(), connectionId);

        display.timer.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A playback failure while an automatic restart waits on its backoff
    // step ends that restart. The failure disables audio (a new revision),
    // so the waiting restart could never run; before this fix it stayed
    // marked waiting until something else asked for audio. The Core sends a
    // context of its own at the current revision while the restart waits
    // (today it does so only on a radio change or a new media connection,
    // each of which the window answers with its own request first), and that
    // context cannot open this computer's speaker. 2026-10-16: the failure
    // takes the next backoff step itself (its own retry, fenced apart from
    // the restart's), so the ended restart's step asks for nothing while
    // the failure's asks once, fails again quietly, and plays once the
    // speaker is back, with no Retry.
    void aPlaybackFailureEndsAWaitingRestart()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        // The Core's display keeps running, so the silence fallback stays
        // out of it (as in repeatedAudioRestartsBackOff).
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        int requests = 0;
        connect(&h.server, &StationServer::mediaControlReceived, this,
                [&](const QJsonObject& control) {
            if (control.value(QStringLiteral("op")) == QLatin1String("audio")
                && control.value(QStringLiteral("enabled")).toBool()) {
                ++requests;
            }
        });
        // The last enabled context the Core sent, and the epoch it came on.
        QJsonObject lastContext;
        quint32 contextEpoch = 0;
        connect(&h.client, &StationClient::mediaControlReceived, this,
                [&](const QJsonObject& payload, quint32 epoch) {
            if (payload.value(QStringLiteral("op")) == QLatin1String("audio-context")
                && payload.value(QStringLiteral("enabled")).toBool()) {
                lastContext = payload;
                contextEpoch = epoch;
            }
        });
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const int before = requests;

        // The Core's audio stops: restarts step through the backoff until
        // one waits on the 4 s step.
        display.timer.start();
        audio.source.stop();
        QTRY_VERIFY_WITH_TIMEOUT(requests >= before + 2
                                     && remoteMedia.audioRestartPendingForTest(), 10000);
        // The 4 s step is held, not raced: however long what follows
        // takes, the step cannot run before the context and the failure
        // have landed.
        remoteMedia.holdAudioRestartForTest(true);
        QCOMPARE(requests, before + 2);
        QVERIFY(errors.isEmpty());
        QVERIFY(!lastContext.isEmpty());

        // While it waits, this computer's speaker goes, and the Core sends a
        // newer context at the revision the window last asked for.
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTest::ignoreMessage(QtWarningMsg, kSpeakerOpenFailedLog);
        const int atContext = requests;
        QJsonObject context = lastContext;
        context.insert(QStringLiteral("generation"),
                       context.value(QStringLiteral("generation")).toDouble() + 1);
        QVERIFY(h.server.sendMediaControl(context, contextEpoch));
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 2000);
        QCOMPARE(requests, atContext);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        // The failure's own retry waits on the next step (still 4 s: the
        // streak carries on).
        QVERIFY(remoteMedia.audioRestartPendingForTest());
        // Both steps come due while held. Released, the ended restart's
        // finds itself fenced off and asks for nothing; the failure's asks
        // once and, the speaker still gone, fails again with no toast.
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioRestartStepsHeldForTest(), 2, 10000);
        QCOMPARE(requests, atContext);
        remoteMedia.holdAudioRestartForTest(false);
        QVERIFY(!remoteMedia.audioRestartStepHeldForTest());
        QTRY_COMPARE_WITH_TIMEOUT(requests, atContext + 1, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartPendingForTest(), 2000);
        QCOMPARE(requests, atContext + 1);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);

        // The speaker is back and the Core's audio too: the next automatic
        // step plays again, with no Retry.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        audio.source.start();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QCOMPARE(errors.size(), 1);

        display.timer.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-21, R-R3-51: the Core sends the keying device no receive audio
    // while it transmits, so every stream this window plays runs out of
    // packets. That silence is expected, not an outage: the speakers and
    // the headphones mix each stop and wait, with no backoff step and no
    // problem shown, and the Core's unkey asks for both again in the same
    // pass of the event loop, each with a fresh backoff. Before this, each
    // restart backed off (1, 2, 4 s) while keyed, and audio came back up
    // to 4 s after the unkey. A receiver stream for apps never restarts
    // for no packets (it idles, R-R3-43): it asks for nothing, keyed or at
    // the unkey, and plays again as the packets return.
    void transmitSilenceWaitsForTheUnkeyAndResumesAtOnce()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        // The Core's display runs through the silence, as in production.
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        Test::CollectingReceiverSink app;
        const int sliceA = h.sliceA;
        const auto release = qScopeGuard([&] { remoteMedia.releaseReceiverAudio(sliceA, &app); });
        PacedRemoteAudio audio(h);
        QTimer headphones;
        headphones.setInterval(10);
        headphones.setTimerType(Qt::PreciseTimer);
        connect(&headphones, &QTimer::timeout, &headphones, [&h] {
            h.remoteHeadphonesBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        headphones.start();
        h.station.sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        h.connectSession();
        QVERIFY(h.client.capabilities().txStateVersion >= 1);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                     && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                 15000);
        remoteMedia.requestReceiverAudio(sliceA, &app);
        QTRY_VERIFY_WITH_TIMEOUT(app.frames(sliceA) >= 9600, 10000);
        // A speakers' backoff step that comes due waits here instead of
        // running, so a speakers' request below cannot come from one.
        remoteMedia.holdAudioRestartForTest(true);

        // The Core keys and its receive audio stops; its display goes on.
        TransmitState* tx = h.client.transmitState();
        QVERIFY(tx);
        QVERIFY(tx->applyStationValue("keyed", true));
        display.timer.start();
        audio.source.stop();
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));
        const int phones = enabledRequests(coreControls, QStringLiteral("headphones-audio"));
        const int receiver =
            enabledRequests(coreControls, QStringLiteral("receiver-audio"), sliceA);
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.audioTelemetry().running
                                     && !remoteMedia.headphonesTelemetry().running,
                                 5000);
        // Well past the first 1 s step: nothing is asked for while keyed,
        // nothing waits on a backoff step, and nothing says audio failed.
        QTest::qWait(1500);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("headphones-audio")), phones);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("receiver-audio"), sliceA),
                 receiver);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QVERIFY(!remoteMedia.audioRestartStepHeldForTest());
        const RemoteAudioStatus keyed = remoteMedia.audioStatus();
        QCOMPARE(keyed.state, State::WaitingForAudio);
        QVERIFY(!keyed.problem.has_value());
        QVERIFY(errors.isEmpty());
        QVERIFY(remoteMedia.headphonesProblem().isEmpty());
        QVERIFY(app.stops().isEmpty());
        QVERIFY(remoteMedia.receiverAudioTelemetry().value(sliceA).running);
        const int appFrames = app.frames(sliceA);

        // The Core unkeys: the speakers and the headphones are asked for
        // again with no timer between, only the queued delivery to the
        // Core.
        audio.source.start();
        QVERIFY(tx->applyStationValue("keyed", false));
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        const auto allAskedAgain = [&] {
            return enabledRequests(coreControls, QStringLiteral("audio")) > speakers
                && enabledRequests(coreControls, QStringLiteral("headphones-audio")) > phones;
        };
        for (int pass = 0; pass < 100 && !allAskedAgain(); ++pass) {
            QCoreApplication::processEvents();
        }
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers + 1);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("headphones-audio")), phones + 1);

        // And all three play again; no backoff step ever came due, and the
        // receiver stream asked for nothing.
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                     && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                 10000);
        QTRY_VERIFY_WITH_TIMEOUT(app.frames(sliceA) >= appFrames + 9600, 10000);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("receiver-audio"), sliceA),
                 receiver);
        QVERIFY(!remoteMedia.audioRestartStepHeldForTest());
        QVERIFY(errors.isEmpty());

        remoteMedia.holdAudioRestartForTest(false);
        display.timer.stop();
        headphones.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-51: a Core without the mirrored transmit state (txStateVersion
    // 0) gives the window no way to tell that the silence is expected, so
    // its restarts back off as before, keyed or not, and the unkey asks for
    // nothing by itself.
    void transmitSilenceFromAnOlderCoreBacksOffAsBefore()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        h.hideTxState = true;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QCOMPARE(h.client.capabilities().txStateVersion, 0);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        remoteMedia.holdAudioRestartForTest(true);

        TransmitState* tx = h.client.transmitState();
        QVERIFY(tx);
        QVERIFY(tx->applyStationValue("keyed", true));
        display.timer.start();
        audio.source.stop();
        // The restart waits on its backoff step, which comes due (held).
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartPendingForTest(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartStepHeldForTest(), 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::Reconnecting);
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));

        QVERIFY(tx->applyStationValue("keyed", false));
        for (int pass = 0; pass < 100; ++pass) {
            QCoreApplication::processEvents();
        }
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);
        QVERIFY(remoteMedia.audioRestartPendingForTest());

        // Released, the step asks again as it always did.
        audio.source.start();
        remoteMedia.holdAudioRestartForTest(false);
        QTRY_COMPARE_WITH_TIMEOUT(enabledRequests(coreControls, QStringLiteral("audio")),
                                  speakers + 1, 2000);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);

        display.timer.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-51: only a no-packets restart is expected while the Core
    // transmits. Any other restart backs off as before, and the unkey asks
    // for nothing by itself.
    void otherRestartsWhileTransmittingBackOffAsBefore_data()
    {
        QTest::addColumn<int>("fault");
        QTest::newRow("decode failed") << int(RemoteAudioReceiver::Fault::DecodeFailed);
        QTest::newRow("clock buffer") << int(RemoteAudioReceiver::Fault::ClockBuffer);
    }

    void otherRestartsWhileTransmittingBackOffAsBefore()
    {
        using State = RemoteAudioStatus::State;
        QFETCH(int, fault);
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QVERIFY(h.client.capabilities().txStateVersion >= 1);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        remoteMedia.holdAudioRestartForTest(true);

        TransmitState* tx = h.client.transmitState();
        QVERIFY(tx);
        QVERIFY(tx->applyStationValue("keyed", true));
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));
        remoteMedia.raiseAudioRestartForTest(RemoteAudioReceiver::Fault(fault));
        QVERIFY(remoteMedia.audioRestartPendingForTest());
        QCOMPARE(remoteMedia.audioStatus().state, State::Reconnecting);

        QVERIFY(tx->applyStationValue("keyed", false));
        for (int pass = 0; pass < 100; ++pass) {
            QCoreApplication::processEvents();
        }
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);
        QVERIFY(remoteMedia.audioRestartPendingForTest());

        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartStepHeldForTest(), 5000);
        remoteMedia.holdAudioRestartForTest(false);
        QTRY_COMPARE_WITH_TIMEOUT(enabledRequests(coreControls, QStringLiteral("audio")),
                                  speakers + 1, 2000);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (JJ): a short stall keeps the audio context. The Core's
    // audio stops for 0.7 s with the media connection and the control
    // heartbeat up (as on a Wi-Fi, cellular or VPN link that goes quiet),
    // then goes on from where it stopped. The window asks the Core for
    // nothing, keeps the context and its receiver, and plays again.
    void aShortAudioGapKeepsTheContextAndPlaysOn()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioTelemetry().decodedPackets > 25, 5000);
        const std::optional<RemoteAudioContextMessage> context = remoteMedia.acceptedAudioContext();
        QVERIFY(context);
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));
        const RemoteAudioReceiverTelemetry before = remoteMedia.audioTelemetry();

        audio.source.stop();
        QTest::qWait(700);
        audio.source.start();
        QTRY_VERIFY_WITH_TIMEOUT(
            remoteMedia.audioTelemetry().decodedPackets > before.decodedPackets + 25, 5000);

        const RemoteAudioReceiverTelemetry after = remoteMedia.audioTelemetry();
        QVERIFY(after.running);
        QCOMPARE(after.generation, before.generation);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);
        QVERIFY(remoteMedia.acceptedAudioContext());
        QCOMPARE(remoteMedia.acceptedAudioContext()->generation, context->generation);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QCOMPARE(remoteMedia.audioStatus().state, State::Playing);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (JJ): a gap past the approved 2.5 s still asks the Core
    // for a fresh context; 2 s in, nothing has been asked for.
    void anAudioGapPastTheBoundAsksForAFreshContext()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioTelemetry().decodedPackets > 25, 5000);
        const quint64 generation = remoteMedia.audioTelemetry().generation;
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));

        display.timer.start();
        audio.source.stop();
        QTest::qWait(2'000);
        QVERIFY(remoteMedia.audioTelemetry().running);
        QCOMPARE(remoteMedia.audioTelemetry().generation, generation);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);

        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartPendingForTest()
                                     || enabledRequests(coreControls, QStringLiteral("audio"))
                                         > speakers,
                                 2'000);
        audio.source.start();
        QTRY_COMPARE_WITH_TIMEOUT(enabledRequests(coreControls, QStringLiteral("audio")),
                                  speakers + 1, 6'000);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing
                                      && remoteMedia.audioTelemetry().generation > generation,
                                  h.mediaStage(remoteMedia).constData(), 10'000);

        display.timer.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-16 (JJ): the media transport's own failure still ends the
    // audio at once, inside a gap the receiver would otherwise ride out:
    // recovery is asked for and the receiver stopped in the same call,
    // with no wait for the 2.5 s bound.
    void aTransportFailureDuringAGapRestartsAtOnce()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioTelemetry().decodedPackets > 25, 5000);
        QSignalSpy recoveries(&remoteMedia, &RemoteMediaController::recoveryRequested);

        audio.source.stop();
        QTest::qWait(300);
        QVERIFY(remoteMedia.audioTelemetry().running);
        remoteMedia.dropMediaPeerForTest();
        QCOMPARE(recoveries.size(), 1);
        QVERIFY(!remoteMedia.audioTelemetry().running);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-51: a restart held while keyed that something else already
    // answered (here the operator's Retry while keyed, with the Core's
    // audio back, as with the monitor on) is over: the unkey asks for
    // nothing more.
    void unkeyAsksNothingWhenAudioAlreadyPlaysAgain()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        CoreDisplayKeepAlive display;
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, display.factory());
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QVERIFY(h.client.capabilities().txStateVersion >= 1);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        remoteMedia.holdAudioRestartForTest(true);

        TransmitState* tx = h.client.transmitState();
        QVERIFY(tx);
        QVERIFY(tx->applyStationValue("keyed", true));
        display.timer.start();
        audio.source.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.audioTelemetry().running, 5000);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());

        audio.source.start();
        remoteMedia.retryAudio();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        const int speakers = enabledRequests(coreControls, QStringLiteral("audio"));

        QVERIFY(tx->applyStationValue("keyed", false));
        for (int pass = 0; pass < 100; ++pass) {
            QCoreApplication::processEvents();
        }
        QTest::qWait(300);
        QCOMPARE(enabledRequests(coreControls, QStringLiteral("audio")), speakers);
        QCOMPARE(remoteMedia.audioStatus().state, State::Playing);
        QVERIFY(!remoteMedia.audioRestartStepHeldForTest());

        remoteMedia.holdAudioRestartForTest(false);
        display.timer.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Direct media: the case the harness hid. On a direct path the Core's
    // audio and display both go silent while control stays up, and the
    // window's audio restart backoff is running. The silence fallback moves
    // media to the tunnel once; the restart waiting at the move still asks
    // for audio on the new connection at its backoff step (no storm, no
    // restart lost); the direct-only schedule starts again from its first
    // step, and nothing moves back to a direct path before that step. The Core's audio comes back once media is on the
    // tunnel, so the window plays again only if that waiting restart ran.
    //
    // The ordering is arranged, not left to timing: the silence clock (the
    // injected allocation clock) is held still while the audio restarts
    // step through the backoff, and moved on by one silence window only
    // once a restart is waiting on the 4 s step. That step is held until
    // the move has finished, so the move finishes while that restart
    // waits, which the test asserts.
    void directSilenceFallsBackOnceWhileAudioRestartsBackOff()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        QElapsedTimer clock;
        clock.start();
        bool held = false;
        qint64 heldAt = 0;
        const RemoteMediaController::AllocationClock silenceClock = [&] {
            return held ? heldAt : clock.elapsed();
        };
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr, {}, silenceClock);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QList<qint64> requestMs;
        QList<qint64> fallbackMs;
        QList<qint64> directMs;
        // Every time the window says its path changed, as it says it: the
        // first after the fallback went out is at or before the promotion,
        // so at or before the direct-only schedule could start again.
        QList<qint64> pathChangeMs;
        connect(&remoteMedia, &RemoteMediaController::networkPathChanged, &remoteMedia,
                [&] { pathChangeMs << clock.elapsed(); }, Qt::DirectConnection);
        connect(&h.server, &StationServer::mediaControlReceived, this,
                [&](const QJsonObject& control) {
            const QString op = control.value(QStringLiteral("op")).toString();
            if (op == QLatin1String("audio") && control.value(QStringLiteral("enabled")).toBool()) {
                requestMs << clock.elapsed();
            } else if (op == QLatin1String("replace")) {
                (control.contains(QStringLiteral("mediaDirectVersion")) ? directMs : fallbackMs)
                    << clock.elapsed();
            }
        });
        QSignalSpy recoveries(&remoteMedia, &RemoteMediaController::recoveryRequested);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QVERIFY(h.client.mediaDirectAvailable());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(!h.client.mediaTunnelInUse());
        const QString directId = remoteMedia.mediaConnectionId();

        // The move, seen once it has finished: whether the restart still
        // waits then, and the Core's audio returns from here, on the tunnel.
        qint64 movedMs = -1;
        bool pendingAtMove = false;
        qsizetype requestsAtMove = 0;
        connect(&remoteMedia, &RemoteMediaController::networkPathChanged, &remoteMedia, [&] {
            if (movedMs < 0 && remoteMedia.mediaConnectionId() != directId) {
                movedMs = clock.elapsed();
                pendingAtMove = remoteMedia.audioRestartPendingForTest();
                requestsAtMove = requestMs.size();
                audio.source.start();
            }
        }, Qt::QueuedConnection);

        // Audio and display both stop; control stays up. The silence clock
        // holds, so no fallback yet.
        heldAt = clock.elapsed();
        held = true;
        const qsizetype before = requestMs.size();
        audio.source.stop();
        // Two restarts (the 1 s and 2 s steps), then the next one waiting
        // on the 4 s step.
        QTRY_VERIFY_WITH_TIMEOUT(requestMs.size() >= before + 2
                                     && remoteMedia.audioRestartPendingForTest(), 10000);
        QCOMPARE(requestMs.size(), before + 2);
        const qint64 lastBeforeMove = requestMs.last();
        QCOMPARE(remoteMedia.mediaConnectionId(), directId);
        QVERIFY(fallbackMs.isEmpty());
        // The restart's 4 s step is held until the move has finished: the
        // move takes real time, and under load it can outlast the 4 s step
        // (the restart then ran on the old connection, and the next one
        // was waiting at the move instead).
        remoteMedia.holdAudioRestartForTest(true);
        // A silence window passes on the silence clock: the fallback runs.
        heldAt += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        QTRY_VERIFY_WITH_TIMEOUT(movedMs >= 0, 10000);
        QVERIFY2(pendingAtMove, qPrintable(QStringLiteral(
            "the restart ran before the move finished (moved at %1 ms, last request at %2 ms)")
            .arg(movedMs).arg(lastBeforeMove)));
        QCOMPARE(requestsAtMove, before + 2);
        // Released: the step runs now if it came due during the move, or
        // at its time if not.
        remoteMedia.holdAudioRestartForTest(false);
        // Right after the move, on the tunnel, the direct-only schedule
        // starts again at its first step (that step cannot have fired yet).
        QTRY_VERIFY_WITH_TIMEOUT(h.client.mediaTunnelInUse(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.directUpgradeDelayMs(),
                                  PathRacer::kUpgradeRetryMs[0], 2000);
        QVERIFY(directMs.isEmpty());
        // The waiting restart ran on the new connection and the window
        // plays again.
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);
        // Hold to a silence window after the move before judging the
        // restarts.
        const qint64 windowEnd = movedMs + RemoteMediaController::kDirectMediaSilenceFallbackMs;
        if (clock.elapsed() < windowEnd) { QTest::qWait(int(windowEnd - clock.elapsed())); }

        QStringList parts;
        for (qsizetype i = before; i < requestMs.size(); ++i) {
            parts << QString::number(requestMs.at(i));
        }
        const QString evidence = QStringLiteral(
            "audio requests at %1 ms; fallback at %2; direct at %3; moved at %4 ms")
            .arg(parts.join(QStringLiteral(", ")),
                 fallbackMs.isEmpty() ? QStringLiteral("none")
                                      : QString::number(fallbackMs.first()),
                 directMs.isEmpty() ? QStringLiteral("none") : QString::number(directMs.first()))
            .arg(movedMs);
        qInfo().noquote() << evidence;

        // The fallback ran once, to the tunnel.
        QVERIFY2(fallbackMs.size() == 1, qPrintable(evidence));
        QVERIFY(remoteMedia.mediaConnectionId() != directId);
        // The waiting restart asked for audio after the move, at its 4 s
        // step from the request before (the move neither lost nor hurried
        // it), and every restart kept at least the first step: no storm.
        QVERIFY2(requestMs.size() > before + 2, qPrintable(evidence));
        QVERIFY2(requestMs.at(before + 2) > movedMs, qPrintable(evidence));
        QVERIFY2(requestMs.at(before + 2) - lastBeforeMove >= 3990, qPrintable(evidence));
        for (qsizetype i = before + 1; i < requestMs.size(); ++i) {
            QVERIFY2(requestMs.at(i) - requestMs.at(i - 1) >= 990, qPrintable(evidence));
        }
        // Audio came back on the tunnel: no recovery was needed.
        QCOMPARE(recoveries.count(), 0);

        // Nothing moves back to a direct path before the direct-only
        // schedule's first step, judged from that step and not from the
        // silence window: the step's timer is a coarse one, which Qt keeps
        // within 5% of its interval and may fire that much early, and it
        // starts on the first stall tick after the promotion, which is at
        // or after the first path change after the fallback went out.
        const int firstStep = PathRacer::kUpgradeRetryMs[0];
        QTRY_VERIFY_WITH_TIMEOUT(!directMs.isEmpty(), firstStep + 2000);
        qint64 promotionLower = -1;
        for (qint64 at : std::as_const(pathChangeMs)) {
            if (at >= fallbackMs.first()) {
                promotionLower = at;
                break;
            }
        }
        QVERIFY(promotionLower >= 0);
        const qint64 earliestStep = promotionLower + firstStep - firstStep / 20;
        const QString stepEvidence = QStringLiteral(
            "promoted no earlier than %1 ms; first direct step at %2 ms; earliest allowed %3 ms;"
            " a silence window after the move ends at %4 ms")
            .arg(promotionLower).arg(directMs.first()).arg(earliestStep).arg(windowEnd);
        qInfo().noquote() << stepEvidence;
        for (qint64 at : std::as_const(directMs)) {
            QVERIFY2(at >= earliestStep, qPrintable(stepEvidence));
        }

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23 (b): Retry sends a newer request, and the problem clears only
    // once the new context's receiver has made the speaker consume audio.
    void retryClearsTheProblemOnlyOnceTheNewContextPlays()
    {
        using State = RemoteAudioStatus::State;
        using Fault = RemoteAudioReceiver::Fault;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        // Mute, and while muted the speaker goes away, so the next start
        // cannot open it. Retry does nothing while muted.
        const quint32 playingContext = remoteMedia.acceptedAudioContext()->generation;
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, playingContext,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        const int requestsWhileMuted = countControl(coreControls, QStringLiteral("audio"));
        remoteMedia.retryAudio();
        QTest::qWait(100);
        QCOMPARE(countControl(coreControls, QStringLiteral("audio")), requestsWhileMuted);
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTest::ignoreMessage(QtWarningMsg, kSpeakerOpenFailedLog);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        // This case is the operator's Retry: the failure's own automatic
        // step (2026-10-16) is held so it cannot ask first.
        remoteMedia.holdAudioRestartForTest(true);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::PlaybackProblem);
        QVERIFY(status.problem == Fault::SpeakerOpenFailed);
        QVERIFY(status.retryAvailable);
        // Core's answer to the disable the failure sent. The failed start
        // was for an enabled context, so any disabled one is that answer.
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, 0,
                                             RemoteAudioOffReason::ClientDisabled), 5000);

        // The speaker is back. Retry asks Core again with a newer revision.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        const QList<QJsonObject> requestsBefore = controlsFor(coreControls, QStringLiteral("audio"));
        QVERIFY(!requestsBefore.isEmpty());
        std::optional<RemoteAudioStatus> statusAtStart;
        std::optional<RemoteAudioReceiverTelemetry> playbackAtStart;
        const QMetaObject::Connection probe = connect(
            &remoteMedia, &RemoteMediaController::audioContextAccepted, this, [&] {
                const std::optional<RemoteAudioContextMessage> context =
                    remoteMedia.acceptedAudioContext();
                if (!statusAtStart && context && context->enabled) {
                    statusAtStart = remoteMedia.audioStatus();
                    playbackAtStart = remoteMedia.audioTelemetry();
                }
            });
        const auto dropProbe = qScopeGuard([probe] { QObject::disconnect(probe); });
        remoteMedia.retryAudio();
        QTRY_VERIFY_WITH_TIMEOUT(controlsFor(coreControls, QStringLiteral("audio")).size()
                                     > requestsBefore.size(), 5000);
        const QJsonObject retry = controlsFor(coreControls, QStringLiteral("audio")).constLast();
        QVERIFY(retry.value(QStringLiteral("revision")).toDouble()
                > requestsBefore.constLast().value(QStringLiteral("revision")).toDouble());
        QVERIFY(retry.value(QStringLiteral("enabled")).toBool());

        // Core's new context starts a new receiver. Until that receiver has
        // made the speaker consume audio, the problem is still shown.
        QTRY_VERIFY_WITH_TIMEOUT(statusAtStart.has_value(), 5000);
        QVERIFY(playbackAtStart->running);
        QCOMPARE(playbackAtStart->deviceConsumedFrames, quint64(0));
        QCOMPARE(statusAtStart->state, State::PlaybackProblem);
        QVERIFY(statusAtStart->problem == Fault::SpeakerOpenFailed);

        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::Playing, 10000);
        status = remoteMedia.audioStatus();
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QVERIFY(remoteMedia.audioTelemetry().deviceConsumedFrames > 0);
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23 (c): a fault still queued from an ended session changes
    // nothing in the next one, and a recorded problem ends with its session.
    void lateFaultFromAnEndedSessionChangesNothing()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const RemoteAudioStatus playing = remoteMedia.audioStatus();

        // The speaker's timing goes while this session plays. The event loop
        // is held, so the receiver's report is still queued when the session
        // ends: the worker has stopped, and the controller has not heard.
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QElapsedTimer held;
        held.start();
        while (remoteMedia.audioTelemetry().running && held.elapsed() < 5000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QVERIFY(!remoteMedia.audioTelemetry().running);
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Its report is posted.
        QVERIFY(remoteMedia.audioStatus() == playing);
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        h.client.disconnectFromStation(QStringLiteral("end the first session"));
        QCOMPARE(remoteMedia.audioStatus().state, State::NotConnected);

        // The next session plays; the old report arrives and changes nothing.
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTest::qWait(250);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(remoteMedia.audioStatus().state, State::Playing);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());

        // A problem recorded in this session does not outlive it either.
        QTest::ignoreMessage(QtWarningMsg, speakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        h.client.disconnectFromStation(QStringLiteral("end the second session"));
        QCOMPARE(remoteMedia.audioStatus().state, State::NotConnected);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        QVERIFY(!remoteMedia.audioStatus().retryAvailable);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(!remoteMedia.audioStatus().problem.has_value());
        QCOMPARE(errors.size(), 1);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23 (e): a minor-7 Core still plays; it just reports no codec.
    void minorSevenCorePlaysWithoutCodecDetail()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        PacedRemoteAudio audio(h);
        h.connectSession(quint16{7});
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!remoteMedia.audioDetailNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const RemoteAudioStatus status = remoteMedia.audioStatus();
        QVERIFY(!status.detailNegotiated);
        QVERIFY(!status.encoder.has_value());
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Not reported by this Core"));
        QVERIFY(!status.problem.has_value());
        QVERIFY(!status.retryAvailable);
        QCOMPARE(errors.size(), 0);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23 (f): Core's reasons reach the status. encoder-unavailable is
    // "Core could not start audio" with Retry; radio-offline is "Radio
    // offline", and stays so when this GUI's own withdrawal makes Core's
    // latest reason client-disabled.
    void coreReasonsShowAsCoreCouldNotStartAndRadioOffline()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy guiControls(&h.client, &StationClient::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        // A Core whose encoder cannot start answers a request with audio off
        // for encoder-unavailable. That answer goes on the wire ahead of this
        // Core's real one, which then arrives with the same generation.
        h.stationLink->forgeNextAudioContext = [](const QJsonObject& real) {
            QJsonObject unavailable = real;
            unavailable.insert(QStringLiteral("enabled"), false);
            unavailable.remove(QStringLiteral("encoder"));
            unavailable.insert(QStringLiteral("reason"), QStringLiteral("encoder-unavailable"));
            return unavailable;
        };
        const quint32 playingContext = remoteMedia.acceptedAudioContext()->generation;
        remoteMedia.retryAudio();
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, playingContext,
                                             RemoteAudioOffReason::EncoderUnavailable), 5000);
        QCOMPARE(h.stationLink->forgedContexts, 1);
        const quint32 refusedGeneration = remoteMedia.acceptedAudioContext()->generation;
        const auto contextsWith = [&guiControls](quint32 generation) {
            int count = 0;
            for (const QJsonObject& context :
                 controlsFor(guiControls, QStringLiteral("audio-context"))) {
                if (context.value(QStringLiteral("generation")).toInteger() == generation) {
                    ++count;
                }
            }
            return count;
        };
        QTRY_COMPARE_WITH_TIMEOUT(contextsWith(refusedGeneration), 2, 5000);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::CoreCouldNotStart);
        QVERIFY(status.retryAvailable);
        QVERIFY(status.detailNegotiated);
        QVERIFY(!status.encoder.has_value());
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Audio is off"));
        QVERIFY(!status.problem.has_value());

        // Retry asks again, and this Core's encoder answers.
        remoteMedia.retryAudio();
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::Playing, 10000);
        QVERIFY(remoteMedia.audioStatus().encoder.has_value());

        // The station radio drops with audio playing. Core says why at once;
        // this GUI's mirror follows and withdraws its own request, so Core's
        // latest reason becomes client-disabled. The radio is still the
        // reason shown, from the first moment to the last.
        AudioStatusHistory history(remoteMedia);
        const quint32 beforeDrop = remoteMedia.acceptedAudioContext()->generation;
        h.station.setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::RadioOffline, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!h.remote.isConnected(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, beforeDrop,
                                             RemoteAudioOffReason::ClientDisabled), 5000);
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, State::RadioOffline);
        QVERIFY(!status.retryAvailable);
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Audio is off"));
        QVERIFY(history.onlyFromFirst(State::RadioOffline, {State::RadioOffline}));

        // The radio returns, and so does audio.
        h.station.setConnectionStateForTest(ConnectionState::Connected);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::Playing, 15000);
        QCOMPARE(errors.size(), 0);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }
    // Wrong desktop default or missing migration must fail at the user's control.
    void savedAudioChoicesMigrateToHighAndLossless()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        RemoteConnectionController controls(&h.client, &h.remote, {});
        for (const auto& fixture : {std::pair{QStringLiteral("Opus"), 0},
                                   std::pair{QStringLiteral("Lossless"), 2},
                                   std::pair{QStringLiteral("SaveData"), 1},
                                   std::pair{QString(), 0}}) {
            AppSettings::instance().setValue(
                QLatin1String(RemoteMediaController::kAudioProfileSettingKey), fixture.first);
            RemoteMediaController media(&h.client, &h.remote, nullptr);
            RemoteConnectionPanel panel(&controls, nullptr, &media);
            auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
            QVERIFY(choice);
            QCOMPARE(choice->count(), 3);
            QCOMPARE(choice->itemText(0), QStringLiteral("High — 48 kbps"));
            QCOMPARE(choice->itemText(1), QStringLiteral("Save data — 24 kbps"));
            QCOMPARE(choice->itemText(2), QStringLiteral("Lossless"));
            QCOMPARE(choice->currentIndex(), fixture.second);
        }
    }

    // Wrong wire gating must fail even though High remains selectable on an old Core.
    void legacyCoreKeepsHighUsableAndSaveDataUnavailable()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.client.withholdFeatureForTest(QByteArrayLiteral("audioQuality"));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        daemon.setAudioTargetBitrate(24000);
        QSignalSpy outbound(&h.server, &StationServer::mediaControlReceived);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()
            && media.acceptedAudioContext()->encoder, 15000);
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        auto* details = panel.findChild<QLabel*>(QStringLiteral("remoteAudioDetails"));
        QVERIFY(choice && details);
        QCOMPARE(choice->count(), 3);
        auto* rows = qobject_cast<QStandardItemModel*>(choice->model());
        QVERIFY(rows);
        QVERIFY(rows->item(0)->isEnabled());
        QVERIFY(!rows->item(1)->isEnabled());
        QVERIFY(rows->item(1)->toolTip().contains(QStringLiteral("Core")));
        QCOMPARE(media.acceptedAudioContext()->encoder->targetBitrate, 24000);
        for (const QJsonObject& control : controlsFor(outbound, QStringLiteral("audio"))) {
            QVERIFY(!control.contains(QStringLiteral("opusBitrate")));
        }
        QVERIFY(details->text().contains(QStringLiteral("Requested quality: High")));
        QVERIFY(details->text().contains(QStringLiteral("24\u00A0kbit/s target")));
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void missingCatalogueKeepsHighUsableWithoutClaiming48()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.hideAudioCatalogue = true;
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        daemon.setAudioTargetBitrate(24000);
        QSignalSpy outbound(&h.server, &StationServer::mediaControlReceived);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()
            && media.acceptedAudioContext()->encoder, 15000);
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        QVERIFY(choice);
        QCOMPARE(choice->count(), 3);
        auto* rows = qobject_cast<QStandardItemModel*>(choice->model());
        QVERIFY(rows && rows->item(0)->isEnabled());
        QVERIFY(!rows->item(1)->isEnabled());
        QVERIFY(rows->item(1)->toolTip().contains(QStringLiteral("Checking")));
        QCOMPARE(media.acceptedAudioContext()->encoder->targetBitrate, 24000);
        for (const QJsonObject& control : controlsFor(outbound, QStringLiteral("audio"))) {
            QVERIFY(!control.contains(QStringLiteral("opusBitrate")));
        }
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void preAuthCatalogueCannotOfferAudioRates_data()
    {
        QTest::addColumn<bool>("delta");
        QTest::newRow("object-create") << false;
        QTest::newRow("delta") << true;
    }

    void preAuthCatalogueCannotOfferAudioRates()
    {
        QFETCH(bool, delta);
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        auto* station = new Test::RewritingTransport(QStringLiteral("station"), std::nullopt);
        auto* clientEnd = new Test::RewritingTransport(QStringLiteral("client"), std::nullopt);
        station->hideAudioCatalogue = true;
        h.stationLink = station;
        station->linkTo(clientEnd);
        h.client.startSession(clientEnd, h.server.token());
        QSignalSpy rates(&h.client, &StationClient::audioOpusBitratesChanged);
        const QList<MirrorUpdate> bag{{0, QByteArrayLiteral("json"), MirrorWireKind::Utf8, QStringLiteral(
            "{\"audio\":{\"opusProfiles\":[{\"bitrate\":24000},{\"bitrate\":48000}]}}")}};
        const auto catalogue = delta
            ? SessionMessages::delta(QByteArrayLiteral("catalog"), bag)
            : SessionMessages::objectCreate(QByteArrayLiteral("catalog"),
                                            QByteArrayLiteral("StationCatalog"), bag);
        // Active transport, before authentication: neither mirrored shape
        // may advertise a rate to the subsequently authenticated session.
        emit clientEnd->textReceived(SessionMessages::encode(catalogue));
        QVERIFY2(!h.client.audioOpusBitrates(), "Pre-auth catalogue became an offered audio table");
        QCOMPARE(rates.size(), 0);
        h.server.acceptTransport(station);
        QTRY_VERIFY(h.client.isHandshakeComplete());
        QVERIFY(!h.client.audioOpusBitrates());
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        QVERIFY(choice);
        auto* rows = qobject_cast<QStandardItemModel*>(choice->model());
        QVERIFY(rows && !rows->item(1)->isEnabled());
        // The same session can accept its legitimate catalogue after auth;
        // admission must not wait for some unrelated reconnect or UI action.
        station->hideAudioCatalogue = false;
        station->sendText(SessionMessages::encode(catalogue));
        QTRY_VERIFY(h.client.audioOpusBitrates().has_value());
        QCOMPARE(*h.client.audioOpusBitrates(), QList<int>({24000, 48000}));
        QCOMPARE(rates.size(), 1);
        QVERIFY(rows->item(1)->isEnabled());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void savedSaveDataIntentSurvivesAnUnsupportedCore()
    {
        const RestoreAudioChoice restore;
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("SaveData"));
        Test::RemoteAudioSessionHarness h;
        h.client.withholdFeatureForTest(QByteArrayLiteral("audioQuality"));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        daemon.setAudioTargetBitrate(24000);
        QSignalSpy outbound(&h.server, &StationServer::mediaControlReceived);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()
            && media.acceptedAudioContext()->encoder, 15000);
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        auto* details = panel.findChild<QLabel*>(QStringLiteral("remoteAudioDetails"));
        QVERIFY(choice && details);
        QCOMPARE(choice->currentIndex(), 1);
        QVERIFY(details->text().contains(QStringLiteral("Requested quality: Save data")));
        QCOMPARE(AppSettings::instance().value(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey)).toString(), QStringLiteral("SaveData"));
        for (const QJsonObject& control : controlsFor(outbound, QStringLiteral("audio"))) {
            QVERIFY(!control.contains(QStringLiteral("opusBitrate")));
        }
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A quality switch must discard an incomplete uplink packet before
    // samples for its new encoder arrive. Exercise the actual packet sender.
    void radioSourceAckAloneStopsMacMicrophoneAndRestoresCleanClientPackets()
    {
        const RestoreAudioChoice restore;
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("High"));
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.server.setRemoteTransmitAllowed(true);
        // Keep capture empty so a resumed wall-clock fake cannot obscure
        // the partial program packet boundary exercised below.
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QVERIFY(h.client.remoteMicSourceAvailable());
        media.setHoldsTransmit(true);
        QVERIFY(media.micUplinkRunning());
        auto* uplink = h.stationLink->peerForTest();
        QVERIFY(uplink);
        uplink->setHoldsOutgoing(true);
        QVERIFY(h.client.requestMicSource(MicSource::Radio).sent);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!h.client.remoteTransmit()->micSourceSettled());
        const std::vector<float> samples(960, 0.25f);
        media.pushProgramAudio(samples.data(), 959, 1, 48000);
        uplink->setHoldsOutgoing(false);
        QTRY_VERIFY(h.client.remoteTransmit()->micSourceSettled());
        QVERIFY(!media.micUplinkRunning());
        QVERIFY(media.audioStatus().microphoneFormat.contains(QStringLiteral("Radio microphone at the Core")));
        QVERIFY(!media.audioStatus().microphoneFormat.contains(QStringLiteral("Opus")));
        const quint64 stopped = media.micPacketsSent();
        media.pushProgramAudio(samples.data(), 960, 1, 48000);
        media.setMicKeyDown(true);
        media.setMicKeyDown(false);
        QCOMPARE(media.micPacketsSent(), stopped);
        QVERIFY(h.client.requestMicSource(MicSource::Pc).sent);
        QTRY_VERIFY(h.client.remoteTransmit()->micSourceSettled());
        QVERIFY(media.micUplinkRunning());
        QCOMPARE(media.micPacketsSent(), stopped);
        media.pushProgramAudio(samples.data(), 1, 1, 48000);
        media.setMicKeyDown(true);
        media.setMicKeyDown(false);
        QCOMPARE(media.micPacketsSent(), stopped);
        media.pushProgramAudio(samples.data(), 959, 1, 48000);
        media.setMicKeyDown(true);
        media.setMicKeyDown(false);
        QCOMPARE(media.micPacketsSent(), stopped + 1);
        media.setHoldsTransmit(false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void microphoneQualitySwitchDiscardsPartialPackets()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        attachRemoteMicrophone(h, 0.0f, 1000.0);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        media.setHoldsTransmit(true);
        QVERIFY(media.micUplinkRunning());
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        QVERIFY(choice);
        const std::vector<float> samples(960, 0.25f);
        for (const int index : {1, 0}) {
            media.pushProgramAudio(samples.data(), 959, 1, 48000);
            choice->setCurrentIndex(index);
            const quint64 before = media.micPacketsSent();
            const int target = index == 1 ? 24 : 48;
            QVERIFY(media.audioStatus().microphoneFormat.contains(
                QStringLiteral("%1\u00A0kbit/s target").arg(target)));
            media.pushProgramAudio(samples.data(), 1, 1, 48000);
            // These public uplink state changes synchronously drain available
            // samples while holdsTransmit keeps the microphone line active.
            media.setMicKeyDown(true);
            media.setMicKeyDown(false);
            QCOMPARE(media.micPacketsSent(), before);
            media.pushProgramAudio(samples.data(), 959, 1, 48000);
            media.setMicKeyDown(true);
            media.setMicKeyDown(false);
            QCOMPARE(media.micPacketsSent(), before + 1);
        }
        media.setHoldsTransmit(false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A missing per-device request, stale headphone encoder, or applying the
    // listening rate to receiver streams must fail the real Core's answers.
    void qualityChangesListeningAndKeepsReceiverAppsAt48()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        h.station.sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        daemon.setAudioTargetBitrate(24000);
        QSignalSpy outbound(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy inbound(&h.client, &StationClient::mediaControlReceived);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        RemoteConnectionPanel panel(&controls, nullptr, &media);
        Test::CollectingReceiverSink sink;
        media.requestReceiverAudio(h.sliceA, &sink);
        const auto release = qScopeGuard([&] { media.releaseReceiverAudio(h.sliceA, &sink); });
        PacedRemoteAudio audio(h);
        h.connectSession();
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        QVERIFY(choice);
        const auto heardHeadphones = [&]() -> std::optional<RemoteAudioContextMessage> {
            const auto all = controlsFor(inbound, QStringLiteral("headphones-audio-context"));
            return all.isEmpty() ? std::nullopt : decodeHeadphonesAudioContext(all.constLast());
        };
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()
            && media.acceptedAudioContext()->encoder
            && media.acceptedAudioContext()->encoder->targetBitrate == 48000, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(heardHeadphones() && heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 48000, 10000);
        const QString captureDirectory = qEnvironmentVariable("NEREUS_AUDIO_CAPTURE_DIR");
        if (!captureDirectory.isEmpty()) {
            QDir().mkpath(captureDirectory);
            panel.show();
            QCoreApplication::processEvents();
            QVERIFY(panel.grab().save(captureDirectory + QStringLiteral("/high.png")));
        }
        const quint32 firstHeadphoneGeneration = heardHeadphones()->generation;
        const quint32 firstReceiveGeneration = media.acceptedAudioContext()->generation;
        QCOMPARE(choice->count(), 3);
        choice->setCurrentIndex(1);
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()->encoder
            && media.acceptedAudioContext()->encoder->targetBitrate == 24000, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(heardHeadphones() && heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 24000, 10000);
        QVERIFY(heardHeadphones()->generation != firstHeadphoneGeneration);
        QVERIFY(media.acceptedAudioContext()->generation != firstReceiveGeneration);
        QCOMPARE(controlsFor(outbound, QStringLiteral("audio")).constLast()
            .value(QStringLiteral("opusBitrate")).toInt(), 24000);
        QTRY_VERIFY_WITH_TIMEOUT(!media.audioStatus().receivers.isEmpty()
            && media.audioStatus().receivers.constFirst().encoder, 10000);
        QCOMPARE(media.audioStatus().receivers.constFirst().encoder->targetBitrate, 48000);
        if (!captureDirectory.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(panel.grab().save(captureDirectory + QStringLiteral("/save-data.png")));
        }
        QCOMPARE(AppSettings::instance().value(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey)).toString(),
            QStringLiteral("SaveData"));
        // Muting the main stream leaves headphones running at the selected rate.
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext() && !media.acceptedAudioContext()->enabled, 5000);
        const quint32 headphoneGenerationAt24 = heardHeadphones()->generation;
        const qsizetype headphoneRequests = controlsFor(outbound, QStringLiteral("headphones-audio")).size();
        // An app/phone main-only request must also update the existing headset.
        const quint32 mainRevision = media.acceptedAudioContext()->revision;
        QJsonObject mainOnly{{QStringLiteral("op"), QStringLiteral("audio")},
            {QStringLiteral("connectionId"), media.acceptedAudioContext()->connectionId},
            {QStringLiteral("revision"), double(mainRevision + 1)},
            {QStringLiteral("enabled"), false}, {QStringLiteral("profile"), QStringLiteral("opus")},
            {QStringLiteral("opusBitrate"), 48000}};
        QVERIFY(h.client.sendMediaControl(mainOnly, h.client.sessionEpoch()));
        QTRY_VERIFY_WITH_TIMEOUT(heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 48000, 10000);
        QVERIFY(heardHeadphones()->generation != headphoneGenerationAt24);
        QCOMPARE(controlsFor(outbound, QStringLiteral("headphones-audio")).size(), headphoneRequests);
        const quint32 unchangedHeadphoneGeneration = heardHeadphones()->generation;
        mainOnly.insert(QStringLiteral("revision"), double(mainRevision + 2));
        QVERIFY(h.client.sendMediaControl(mainOnly, h.client.sessionEpoch()));
        QTRY_VERIFY_WITH_TIMEOUT(controlsFor(inbound, QStringLiteral("audio-context")).constLast()
            .value(QStringLiteral("revision")).toInteger() == mainRevision + 2, 5000);
        QCOMPARE(heardHeadphones()->generation, unchangedHeadphoneGeneration);

        // The saved Save data intent replays on a new session, including while muted.
        h.client.disconnectFromStation(QStringLiteral("test saved quality replay"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.isHandshakeComplete(), 5000);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemon.audioStreamBitrate() == 24000 && heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 24000, 10000);
        QCOMPARE(choice->currentIndex(), 1);
        QVERIFY(media.audioStatus().headphonesFormat
            && media.audioStatus().headphonesFormat->contains(QStringLiteral("24\u00A0kbit/s target")));
        if (!captureDirectory.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(panel.grab().save(captureDirectory + QStringLiteral("/save-data-headphones-only.png")));
        }
        choice->setCurrentIndex(0);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.audioStreamBitrate() == 48000 && heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 48000, 10000);
        QCOMPARE(controlsFor(outbound, QStringLiteral("audio")).constLast()
            .value(QStringLiteral("enabled")).toBool(), false);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_VERIFY_WITH_TIMEOUT(media.acceptedAudioContext()->encoder
            && media.acceptedAudioContext()->encoder->targetBitrate == 48000, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(heardHeadphones() && heardHeadphones()->encoder
            && heardHeadphones()->encoder->targetBitrate == 48000, 10000);
        QCOMPARE(media.audioStatus().receivers.constFirst().encoder->targetBitrate, 48000);
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23: the app's link trial. Lossless is chosen and the Core
    // accepts it, but the network cannot carry it: about 5 s in, the
    // trial returns this computer to Opus with the plain reason, and Opus
    // plays on over the same link. The choice stays stored, and the next
    // connection asks for lossless again.
    void losslessFallsBackToOpusWhenTheNetworkCannotCarryIt()
    {
        using State = RemoteAudioStatus::State;
        const RestoreAudioChoice restore;
        // Chosen earlier on this computer: a new controller starts from it.
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("Lossless"));
        Test::RemoteAudioSessionHarness h;
        QList<QPointer<CapacityLimitedTransport>> links;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr,
            [&links](QObject* parent) -> IMediaTransport* {
                auto* link = new CapacityLimitedTransport(parent);
                links.append(link);
                return link;
            });
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QCOMPARE(remoteMedia.audioProfileChoice(), RemoteAudioProfile::Lossless);
        QCOMPARE(remoteMedia.audioStatus().chosenProfile, RemoteAudioProfile::Lossless);
        // Stored on this computer: this key is never the Core's.
        QCOMPARE(classifySettingsKey(
                     QString::fromLatin1(RemoteMediaController::kAudioProfileSettingKey)),
                 SettingsScope::OperatorLocal);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QVERIFY(remoteMedia.audioProfileNegotiated());
        // R-R3-49: the window's media start reaches the Core one queued
        // delivery after the handshake; read it once it has (a busy
        // computer read an empty list here and crashed on constFirst()).
        QTRY_VERIFY_WITH_TIMEOUT(!controlsFor(coreControls, QStringLiteral("start")).isEmpty(),
                                 5000);
        QCOMPARE(controlsFor(coreControls, QStringLiteral("start")).constFirst()
                     .value(QStringLiteral("audioProfileVersion")).toInteger(), qint64{1});

        // The Core accepts lossless and it starts to play, badly.
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().losslessEncoder.has_value(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QElapsedTimer lossless;
        lossless.start();
        QCOMPARE(requestedProfiles(coreControls).constFirst(), QStringLiteral("lossless"));
        QCOMPARE(remoteMedia.audioStatus().runningProfile,
                 std::optional<RemoteAudioProfile>(RemoteAudioProfile::Lossless));
        QVERIFY(!remoteMedia.audioStatus().qualityReason.has_value());

        // The trial decides after its first window, not before.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().qualityReason
                                     == RemoteAudioQualityReason::NetworkTooSlow, 12000);
        const qint64 decidedMs = lossless.elapsed();
        qInfo() << "lossless link trial decided after" << decidedMs << "ms; link passed"
                << links.constLast()->passed << "dropped" << links.constLast()->dropped;
        QVERIFY2(decidedMs >= RemoteAudioLinkTrial::kWindowMs - 1000,
                 qPrintable(QString::number(decidedMs)));
        QVERIFY(links.constLast()->dropped > 0);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.constFirst().at(0).toString(),
                 QStringLiteral("The network could not carry lossless audio; staying on Opus."));
        // The request crosses the loopback session; a loaded machine may
        // deliver it a moment after the verdict.
        QTRY_COMPARE_WITH_TIMEOUT(requestedProfiles(coreControls).constLast(),
                                  QStringLiteral("opus"), 5000);

        // Opus plays over the same link, and the section says why.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing
                                     && remoteMedia.audioStatus().runningProfile
                                         == RemoteAudioProfile::Opus, 10000);
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QVERIFY(status.encoder.has_value());
        QVERIFY(!status.losslessEncoder.has_value());
        QCOMPARE(status.chosenProfile, RemoteAudioProfile::Lossless);
        QVERIFY(formatRemoteAudioDetails(status, remoteMedia.audioTelemetry()).contains(
            QStringLiteral("Requested quality: Lossless\nThe network could not carry lossless audio; "
                           "staying on Opus.\n")));
        QCOMPARE(remoteMedia.audioProfileChoice(), RemoteAudioProfile::Lossless);
        QCOMPARE(AppSettings::instance()
                     .value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey))
                     .toString(),
                 QStringLiteral("Lossless"));
        // Opus fits the link: nothing more is lost once it runs.
        const int droppedAtOpus = links.constLast()->dropped;
        QTest::qWait(1500);
        QCOMPARE(links.constLast()->dropped, droppedAtOpus);
        QCOMPARE(errors.count(), 1);

        // The shared Settings/panel control deliberately reselects the saved
        // Lossless item to retry the actual link trial, even under ControlsLock.
        // Wheel protection must not change this computer-wide preference path.
        RemoteAudioWidget qualityWidget(&remoteMedia, nullptr);
        qualityWidget.show();
        auto* quality = qualityWidget.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        QVERIFY(quality);
        const bool previousLock = ControlsLock::isLocked();
        const auto restoreLock = qScopeGuard([previousLock] { ControlsLock::setLocked(previousLock); });
        for (const bool locked : {false, true}) {
            QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().qualityReason,
                std::optional<RemoteAudioQualityReason>(RemoteAudioQualityReason::NetworkTooSlow), 12000);
            // As above: the request for Opus that follows the verdict crosses
            // the loopback session, and a loaded machine may deliver it a
            // moment later. Counted before it arrived, it took the place the
            // retry's request is read from below.
            QTRY_COMPARE_WITH_TIMEOUT(requestedProfiles(coreControls).constLast(),
                                      QStringLiteral("opus"), 5000);
            ControlsLock::setLocked(locked);
            QCOMPARE(quality->currentData().toInt(), int(RemoteAudioQualityChoice::Lossless));
            const int requestsBeforeRetry = int(requestedProfiles(coreControls).size());
            QTest::mouseClick(quality, Qt::LeftButton);
            QTRY_VERIFY(quality->view()->isVisible());
            QTest::keyClick(quality->view(), Qt::Key_Return);
            QTRY_VERIFY_WITH_TIMEOUT(requestedProfiles(coreControls).size() > requestsBeforeRetry, 5000);
            QCOMPARE(requestedProfiles(coreControls).at(requestsBeforeRetry), QStringLiteral("lossless"));
            QVERIFY(!remoteMedia.audioStatus().qualityReason.has_value());
            QCOMPARE(remoteMedia.audioQualityChoice(), RemoteAudioQualityChoice::Lossless);
        }

        // The next connection replays the stored choice.
        const int controlsBefore = int(requestedProfiles(coreControls).size());
        h.client.disconnectFromStation(QStringLiteral("test reconnect"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.mediaAvailable(), 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::NotConnected);
        QVERIFY(!remoteMedia.audioStatus().qualityReason.has_value());
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(requestedProfiles(coreControls).size() > controlsBefore,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QCOMPARE(requestedProfiles(coreControls).at(controlsBefore), QStringLiteral("lossless"));
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().losslessEncoder.has_value(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-43 (risk E3): a receiver stream the Core retires stops with its
    // reason, and the app plays silence: nothing asks again in a loop. A
    // slice id that never existed is answered once. When the Core brings
    // the slice id back, the waiting app is asked for once more, with a
    // revision above every one sent for that id.
    void retiredReceiverStreamStopsWithItsReasonAndDoesNotLoop()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        Test::CollectingReceiverSink app;
        Test::CollectingReceiverSink ghost;
        constexpr int kMadeUpSlice = 7;
        const int sliceB = h.sliceB;
        const auto release = qScopeGuard([&] {
            remoteMedia.releaseReceiverAudio(sliceB, &app);
            remoteMedia.releaseReceiverAudio(kMadeUpSlice, &ghost);
        });
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        remoteMedia.requestReceiverAudio(sliceB, &app);
        QTRY_VERIFY_WITH_TIMEOUT(app.frames(sliceB) >= 9600, 10000);
        QVERIFY(receivesIn(remoteMedia, sliceB, RemoteAudioProfile::Opus));

        // The slice goes away at the station.
        h.station.removeSlice(sliceB);
        QTRY_VERIFY_WITH_TIMEOUT(!app.stops().isEmpty(), 5000);
        QCOMPARE(app.stops().constLast(), std::make_pair(sliceB, QStringLiteral("slice-removed")));
        QTRY_VERIFY(!remoteMedia.receiverAudioTelemetry().value(sliceB).running);
        const int requestsAtStop = int(receiverRequests(coreControls, sliceB).size());
        QTest::qWait(300);
        const int framesAtStop = app.frames(sliceB);
        // Well past the 500 ms a speaker would wait, and a retry's 1 s.
        QTest::qWait(2000);
        QCOMPARE(app.frames(sliceB), framesAtStop);
        QCOMPARE(int(receiverRequests(coreControls, sliceB).size()), requestsAtStop);
        QCOMPARE(app.stops().size(), 1);
        const RemoteAudioStatus stopped = remoteMedia.audioStatus();
        QCOMPARE(stopped.receivers.size(), 1);
        QCOMPARE(stopped.receivers.constFirst().state, RemoteReceiverAudioStatus::State::Stopped);
        QCOMPARE(stopped.receivers.constFirst().stopReason, QStringLiteral("slice-removed"));
        const QString details = formatRemoteAudioDetails(stopped, remoteMedia.audioTelemetry(), {},
                                                         remoteMedia.receiverAudioTelemetry());
        QVERIFY2(details.contains(QStringLiteral(
                     "Receiver B for apps: Stopped. This receiver is no longer on the Core.")),
                 qPrintable(details));
        // The speakers never noticed.
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::Playing);

        // A slice id the Core never had: answered once, never asked again.
        remoteMedia.requestReceiverAudio(kMadeUpSlice, &ghost);
        QTRY_VERIFY_WITH_TIMEOUT(!ghost.stops().isEmpty(), 5000);
        QCOMPARE(ghost.stops().constLast(),
                 std::make_pair(kMadeUpSlice, QStringLiteral("slice-removed")));
        QTest::qWait(1500);
        QCOMPARE(receiverRequests(coreControls, kMadeUpSlice).size(), qsizetype(1));
        QCOMPARE(ghost.frames(kMadeUpSlice), 0);

        // The Core reuses the id for a new slice: the app waiting on it is
        // asked for once, and hears the new slice.
        const int reused = h.station.addSlice();
        QCOMPARE(reused, sliceB);
        h.station.sliceById(reused)->setAudioPan(0.95);
        QTRY_VERIFY_WITH_TIMEOUT(app.frames(sliceB) >= framesAtStop + 9600, 15000);
        QVERIFY(receivesIn(remoteMedia, sliceB, RemoteAudioProfile::Opus));
        const QList<QJsonObject> requests = receiverRequests(coreControls, sliceB);
        QCOMPARE(requests.size(), qsizetype(requestsAtStop + 1));
        for (qsizetype i = 1; i < requests.size(); ++i) {
            QVERIFY(requests.at(i).value(QStringLiteral("revision")).toInteger()
                    > requests.at(i - 1).value(QStringLiteral("revision")).toInteger());
        }
        // And a release and a new request still only ever go up.
        remoteMedia.releaseReceiverAudio(sliceB, &app);
        remoteMedia.requestReceiverAudio(sliceB, &app);
        QTRY_VERIFY_WITH_TIMEOUT(receiverRequests(coreControls, sliceB).size()
                                     == requests.size() + 2, 5000);
        const QList<QJsonObject> after = receiverRequests(coreControls, sliceB);
        QVERIFY(!after.at(after.size() - 2).value(QStringLiteral("enabled")).toBool());
        QVERIFY(after.constLast().value(QStringLiteral("enabled")).toBool());
        for (qsizetype i = 1; i < after.size(); ++i) {
            QVERIFY(after.at(i).value(QStringLiteral("revision")).toInteger()
                    > after.at(i - 1).value(QStringLiteral("revision")).toInteger());
        }
        QTRY_VERIFY_WITH_TIMEOUT(receivesIn(remoteMedia, sliceB, RemoteAudioProfile::Opus), 10000);
        QCOMPARE(errors.count(), 0);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-43, R-R3-23: the one link trial counts every lossless stream,
    // here two receiver streams with the speakers muted (so the speakers'
    // stream is not lossless at all). A link that cannot carry them fails
    // the trial once, and one fallback moves every stream, receivers and
    // speakers alike, to Opus with one notice.
    void losslessTrialCountsEveryLosslessStreamAndFallsBackOnce()
    {
        const RestoreAudioChoice restore;
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("Lossless"));
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr,
            [](QObject* parent) -> IMediaTransport* {
                return new CapacityLimitedTransport(parent);
            });
        DaemonMediaController daemonMedia(&h.server, &h.station);
        Test::CollectingReceiverSink appA;
        Test::CollectingReceiverSink appB;
        const auto release = qScopeGuard([&] {
            remoteMedia.releaseReceiverAudio(h.sliceA, &appA);
            remoteMedia.releaseReceiverAudio(h.sliceB, &appB);
        });
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        auto* const trialTimer =
            remoteMedia.findChild<QTimer*>(QStringLiteral("remoteAudioLinkTrialTimer"));
        QVERIFY(trialTimer);
        PacedRemoteAudio audio(h);
        h.remote.audioEngine()->setMasterMuted(true);
        remoteMedia.requestReceiverAudio(h.sliceA, &appA);
        remoteMedia.requestReceiverAudio(h.sliceB, &appB);
        h.connectSession();
        QVERIFY(remoteMedia.receiverAudioNegotiated());

        // Both ask for lossless, the one choice; the Core grants it and the
        // trial runs although the speakers play nothing.
        QTRY_VERIFY_WITH_TIMEOUT(!receiverRequests(coreControls, h.sliceA).isEmpty()
                                     && !receiverRequests(coreControls, h.sliceB).isEmpty(), 15000);
        QCOMPARE(receiverRequests(coreControls, h.sliceA).constFirst()
                     .value(QStringLiteral("profile")).toString(), QStringLiteral("lossless"));
        QCOMPARE(receiverRequests(coreControls, h.sliceB).constFirst()
                     .value(QStringLiteral("profile")).toString(), QStringLiteral("lossless"));
        QTRY_VERIFY_WITH_TIMEOUT(trialTimer->isActive(), 15000);
        QVERIFY(!remoteMedia.acceptedAudioContext() || !remoteMedia.acceptedAudioContext()->enabled);

        // The link cannot carry them: the trial fails, once.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().qualityReason
                                     == RemoteAudioQualityReason::NetworkTooSlow, 15000);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.constFirst().at(0).toString(),
                 QStringLiteral("The network could not carry lossless audio; staying on Opus."));
        // Every stream asks for Opus: both receivers and the speakers.
        QTRY_VERIFY_WITH_TIMEOUT(
            receiverRequests(coreControls, h.sliceA).constLast()
                    .value(QStringLiteral("profile")).toString() == QLatin1String("opus")
                && receiverRequests(coreControls, h.sliceB).constLast()
                    .value(QStringLiteral("profile")).toString() == QLatin1String("opus")
                && requestedProfiles(coreControls).constLast() == QLatin1String("opus"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(receivesIn(remoteMedia, h.sliceA, RemoteAudioProfile::Opus)
                                     && receivesIn(remoteMedia, h.sliceB, RemoteAudioProfile::Opus),
                                 10000);
        const int aFrames = appA.frames(h.sliceA);
        const int bFrames = appB.frames(h.sliceB);
        QTRY_VERIFY_WITH_TIMEOUT(appA.frames(h.sliceA) >= aFrames + 19200
                                     && appB.frames(h.sliceB) >= bFrames + 19200, 10000);
        QVERIFY(!trialTimer->isActive());
        const QString details = formatRemoteAudioDetails(remoteMedia.audioStatus(),
                                                         remoteMedia.audioTelemetry(), {},
                                                         remoteMedia.receiverAudioTelemetry());
        QCOMPARE(details.count(QStringLiteral("The network could not carry lossless audio")), 1);
        QVERIFY2(details.contains(QStringLiteral("Receiver A for apps: Receiving, Opus")),
                 qPrintable(details));
        QVERIFY2(details.contains(QStringLiteral("Receiver B for apps: Receiving, Opus")),
                 qPrintable(details));

        // Opus fits: no second notice, and unmuting asks for Opus too.
        QTest::qWait(1500);
        QCOMPARE(errors.count(), 1);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state == RemoteAudioStatus::State::Playing
                                     && remoteMedia.audioStatus().runningProfile
                                         == RemoteAudioProfile::Opus, 10000);
        QCOMPARE(requestedProfiles(coreControls).constLast(), QStringLiteral("opus"));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(remoteMedia.audioProfileChoice(), RemoteAudioProfile::Lossless);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-23: the Remote audio section offers Opus and Lossless. A Core
    // whose setting denies lossless keeps Opus and the section says so in
    // plain words; choosing Opus again clears it. Choosing is stored here
    // and asked for at once.
    void losslessRefusedByTheCoreKeepsOpusAndSaysWhy()
    {
        using State = RemoteAudioStatus::State;
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        daemonMedia.setAudioLosslessAllowed(false);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(remoteMedia.audioStatus().profileChoiceAvailable);
        QCOMPARE(remoteMedia.audioStatus().runningProfile,
                 std::optional<RemoteAudioProfile>(RemoteAudioProfile::Opus));
        QCOMPARE(requestedProfiles(coreControls).constFirst(), QStringLiteral("opus"));

        RemoteConnectionPanel panel(&controls, nullptr, &remoteMedia);
        panel.show();
        QTRY_VERIFY(panel.isVisible());
        auto* choice = panel.findChild<QComboBox*>(QStringLiteral("remoteAudioQuality"));
        auto* details = panel.findChild<QLabel*>(QStringLiteral("remoteAudioDetails"));
        QVERIFY(choice && details);
        QCOMPARE(choice->count(), 3);
        QCOMPARE(choice->itemText(0), QStringLiteral("High — 48 kbps"));
        QCOMPARE(choice->itemText(2), QStringLiteral("Lossless"));
        QCOMPARE(choice->currentText(), QStringLiteral("High — 48 kbps"));
        QVERIFY(details->text().contains(QStringLiteral("Current receive format: Opus")));

        choice->setCurrentIndex(2);
        QCOMPARE(remoteMedia.audioProfileChoice(), RemoteAudioProfile::Lossless);
        QCOMPARE(AppSettings::instance()
                     .value(QLatin1String(RemoteMediaController::kAudioProfileSettingKey))
                     .toString(),
                 QStringLiteral("Lossless"));
        QTRY_COMPARE_WITH_TIMEOUT(requestedProfiles(coreControls).constLast(),
                                  QStringLiteral("lossless"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().qualityReason
                                     == RemoteAudioQualityReason::CoreNotAllowed, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing, 10000);
        QCOMPARE(remoteMedia.audioStatus().runningProfile,
                 std::optional<RemoteAudioProfile>(RemoteAudioProfile::Opus));
        QTRY_VERIFY(details->text().contains(QStringLiteral(
            "Requested quality: Lossless\nThis Core does not allow lossless audio.\n")));
        // A refusal is an answer, not a fault: no alert, no trial.
        QCOMPARE(errors.count(), 0);
        QVERIFY(!remoteMedia.findChild<QTimer*>(
            QStringLiteral("remoteAudioLinkTrialTimer"))->isActive());

        choice->setCurrentIndex(0);
        QTRY_COMPARE_WITH_TIMEOUT(requestedProfiles(coreControls).constLast(),
                                  QStringLiteral("opus"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.audioStatus().qualityReason.has_value()
                                     && remoteMedia.audioStatus().state == State::Playing, 10000);
        QTRY_VERIFY(!details->text().contains(QStringLiteral("This Core")));

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-45, R-R3-23: the one link trial counts the headphones mix, here
    // the only lossless stream (the speakers are muted). A link that cannot
    // carry it fails the trial once, and the one fallback moves it to Opus
    // with the speakers' stream, with one notice.
    void losslessTrialCountsTheHeadphonesMixAndFallsBackOnce()
    {
        const RestoreAudioChoice restore;
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey), QStringLiteral("Lossless"));
        Test::RemoteAudioSessionHarness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr, nullptr,
            [](QObject* parent) -> IMediaTransport* {
                return new CapacityLimitedTransport(parent);
            });
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        auto* const trialTimer =
            remoteMedia.findChild<QTimer*>(QStringLiteral("remoteAudioLinkTrialTimer"));
        QVERIFY(trialTimer);
        PacedRemoteAudio audio(h);
        QTimer headphones;
        headphones.setInterval(10);
        headphones.setTimerType(Qt::PreciseTimer);
        connect(&headphones, &QTimer::timeout, &headphones, [&h] {
            h.remoteHeadphonesBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        headphones.start();
        h.remote.audioEngine()->setMasterMuted(true);
        h.station.sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        h.connectSession();
        QVERIFY(remoteMedia.headphonesMixNegotiated());

        // Asked for in the one choice, lossless; the Core grants it and the
        // trial runs although the speakers play nothing.
        QTRY_VERIFY_WITH_TIMEOUT(!controlsFor(coreControls, QStringLiteral("headphones-audio"))
                                      .isEmpty(), 15000);
        QCOMPARE(controlsFor(coreControls, QStringLiteral("headphones-audio")).constFirst()
                     .value(QStringLiteral("profile")).toString(), QStringLiteral("lossless"));
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.headphonesMixProfile()
                                     == std::optional{RemoteAudioProfile::Lossless}, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(trialTimer->isActive(), 15000);

        // The link cannot carry it: the trial fails, once.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().qualityReason
                                     == RemoteAudioQualityReason::NetworkTooSlow, 15000);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.constFirst().at(0).toString(),
                 QStringLiteral("The network could not carry lossless audio; staying on Opus."));
        // Both the headphones mix and the speakers' stream ask for Opus.
        QTRY_VERIFY_WITH_TIMEOUT(
            controlsFor(coreControls, QStringLiteral("headphones-audio")).constLast()
                    .value(QStringLiteral("profile")).toString() == QLatin1String("opus")
                && requestedProfiles(coreControls).constLast() == QLatin1String("opus"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.headphonesMixProfile()
                                     == std::optional{RemoteAudioProfile::Opus}, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                     && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                 10000);
        QVERIFY(!trialTimer->isActive());
        QTest::qWait(1500);
        QCOMPARE(errors.count(), 1);

        headphones.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-45: the headphones device fails on this computer (it goes away
    // and stops reporting its timing). Only the headphones stop: the Core is asked to stop
    // the mix, the problem is said in plain words (a notice and the flag's
    // text), and the speakers play on without a new context. Opening the
    // headphones again asks for the mix again.
    void headphonesFailureIsReportedAndLeavesTheSpeakersPlaying()
    {
        Test::RemoteAudioSessionHarness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy errors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy problems(&remoteMedia, &RemoteMediaController::headphonesProblemChanged);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        QTimer headphones;
        headphones.setInterval(10);
        headphones.setTimerType(Qt::PreciseTimer);
        connect(&headphones, &QTimer::timeout, &headphones, [&h] {
            h.remoteHeadphonesBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        headphones.start();
        h.station.sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                     && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                 15000);
        const qsizetype mainContexts = controlsFor(coreControls, QStringLiteral("audio")).size();

        // The headphones device goes away.
        h.remoteHeadphonesBus->setOutputPacingAvailableForTesting(false);
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.headphonesProblem().isEmpty(), 5000);
        const QString problem = remoteMedia.headphonesProblem();
        // Which fault the receiver reports depends on where its worker is
        // when the timing goes: at its pacing read it reports the timing,
        // inside its write loop the write fails first (the headphones bus
        // refuses a write without timing). Either stops the headphones, and
        // the worker returns after the first, so exactly one is reported.
        const QString timingText = QStringLiteral(
            "The headphones stopped reporting their timing. Turn the headphones off and on in "
            "Setup, Audio, Outputs to try again.");
        const QString writeText = QStringLiteral(
            "Audio could not be sent to the headphones. Turn the headphones off and on in "
            "Setup, Audio, Outputs to try again.");
        QCOMPARE(RemoteMediaController::headphonesFaultText(
                     RemoteAudioReceiver::Fault::SpeakerTimingUnavailable), timingText);
        QCOMPARE(RemoteMediaController::headphonesFaultText(
                     RemoteAudioReceiver::Fault::SpeakerWriteFailed), writeText);
        QVERIFY2(problem == timingText || problem == writeText, qPrintable(problem));
        QVERIFY(OperatorWording::isPlain(problem));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.constFirst().at(0).toString(), problem);
        QCOMPARE(OperatorReasonText::forDisplay(problem), problem);
        QVERIFY(!remoteMedia.headphonesTelemetry().running);
        // The Core is asked to stop the mix, and does.
        QTRY_VERIFY_WITH_TIMEOUT(!controlsFor(coreControls, QStringLiteral("headphones-audio"))
                                      .constLast().value(QStringLiteral("enabled")).toBool(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!daemonMedia.headphonesMixSending(), 5000);

        // The speakers played on: still playing, no new request for them,
        // and their device keeps hearing audio.
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::Playing);
        QCOMPARE(controlsFor(coreControls, QStringLiteral("audio")).size(), mainContexts);
        const int heard = int(h.remoteBus->heard.size());
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() >= heard + 48000 * 2, 5000);
        QVERIFY(remoteMedia.audioTelemetry().running);
        // Nothing asks again in a loop.
        const qsizetype requestsAfter =
            controlsFor(coreControls, QStringLiteral("headphones-audio")).size();
        QTest::qWait(1500);
        QCOMPARE(controlsFor(coreControls, QStringLiteral("headphones-audio")).size(),
                 requestsAfter);
        QCOMPARE(errors.count(), 1);

        // R-R3-45 fix wave: a media reconnect keeps the fault. The failed
        // device is not asked for again, and there is no second notice.
        h.client.disconnectFromStation(QStringLiteral("test reconnect"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.mediaAvailable(), 5000);
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTest::qWait(1000);
        QCOMPARE(controlsFor(coreControls, QStringLiteral("headphones-audio")).size(),
                 requestsAfter);
        QVERIFY(!daemonMedia.headphonesMixSending());
        QCOMPARE(remoteMedia.headphonesProblem(), problem);
        QCOMPARE(errors.count(), 1);

        // Choosing the audio quality again does retry them (the fix wave's
        // review: a decoder fault depends on it). The device is back, so
        // they play, and the problem clears.
        {
            const RestoreAudioChoice restore;
            h.remoteHeadphonesBus->setOutputPacingAvailableForTesting(true);
            remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Lossless);
            QTRY_VERIFY_WITH_TIMEOUT(controlsFor(coreControls, QStringLiteral("headphones-audio"))
                                         .constLast().value(QStringLiteral("enabled")).toBool(),
                                     5000);
            QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                         && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                     10000);
            QVERIFY(remoteMedia.headphonesProblem().isEmpty());
            remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Opus);
        }
        QCOMPARE(errors.count(), 1);

        // The headphones are closed and opened again (the Enabled box, or
        // another device): asked for again, and they play.
        h.remote.audioEngine()->setHeadphonesBusForTest(nullptr);
        QVERIFY(!h.remote.audioEngine()->headphonesAvailable());
        h.attachRemoteHeadphones();
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesProblem().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.headphonesMixSending(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.headphonesTelemetry().running
                                     && remoteMedia.headphonesTelemetry().decodedPackets > 10,
                                 10000);
        QCOMPARE(errors.count(), 1);

        headphones.stop();
        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-45: a Core from before the headphones mix. The media start and
    // every audio control are today's, no headphones request goes out, and
    // the flag's reason says in plain words that this Core cannot send
    // audio for the headphones.
    void olderCoreCannotSendTheHeadphonesMixAndSaysSo()
    {
        Test::RemoteAudioSessionHarness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        PacedRemoteAudio audio(h);
        h.hideHeadphonesMix = true;
        h.connectSession();
        QVERIFY(remoteMedia.audioProfileNegotiated());
        QVERIFY(!remoteMedia.headphonesMixNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const QString reason =
            QString::fromLatin1(RemoteMediaController::kHeadphonesMixUnavailableReason);
        QCOMPARE(remoteMedia.headphonesProblem(), reason);
        QCOMPARE(reason, QStringLiteral("This Core cannot send audio for the headphones, so "
                                        "this receiver plays on the speakers."));
        QVERIFY(OperatorWording::isPlain(reason));

        h.remote.sliceById(h.sliceB)->setOutputRoute(SliceModel::OutputRoute::Headphones);
        QTRY_COMPARE_WITH_TIMEOUT(h.station.sliceById(h.sliceB)->outputRoute(),
                                  SliceModel::OutputRoute::Headphones, 5000);
        QTest::qWait(300);
        QVERIFY(controlsFor(coreControls, QStringLiteral("headphones-audio")).isEmpty());
        QVERIFY(!daemonMedia.headphonesMixSending());
        const QList<QJsonObject> starts = controlsFor(coreControls, QStringLiteral("start"));
        QCOMPARE(starts.size(), qsizetype(1));
        QVERIFY(!starts.constFirst().contains(QStringLiteral("headphonesMixVersion")));
        QCOMPARE(remoteMedia.headphonesProblem(), reason);

        audio.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // ---- iPhone app plan Task 36 (R-IOS-13): the microphone uplink ----

    // Older Cores see today's start; a Core that takes this computer's
    // microphone gets remoteTxVersion in it and a microphone line.
    void micLineOnlyWithACoreThatTakesTheMicrophone()
    {
        for (const bool declare : {false, true}) {
            Test::RemoteAudioSessionHarness h;
            h.declareRemoteTx = declare;
            RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
            DaemonMediaController daemonMedia(&h.server, &h.station);
            QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
            h.connectSession();
            QTRY_VERIFY_WITH_TIMEOUT(!controlsFor(coreControls, QStringLiteral("start")).isEmpty(),
                                     5000);
            const QJsonObject start = controlsFor(coreControls, QStringLiteral("start")).constFirst();
            QCOMPARE(remoteMedia.micLineNegotiated(), declare);
            QCOMPARE(start.contains(QStringLiteral("remoteTxVersion")), declare);
            if (declare) {
                QCOMPARE(start.value(QStringLiteral("remoteTxVersion")).toInt(), 1);
                QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
                // VOX armed but this session not permitted to transmit (a
                // token sign-in): nothing is sent.
                QVERIFY(!h.client.capabilities().txPermitted);
                remoteMedia.setVoxArmed(true);
                QTest::qWait(300);
                QVERIFY(!remoteMedia.micUplinkRunning());
                QCOMPARE(remoteMedia.micPacketsSent(), quint64(0));
            } else {
                QTest::qWait(100);
                QVERIFY(daemonMedia.micReceiver() == nullptr);
            }
            h.client.disconnectFromStation(QStringLiteral("test complete"));
        }
    }

    // The window sends its chosen microphone while it holds transmit, while
    // its key is down, and while it has VOX armed, and never otherwise:
    // counted in packets here and at the Core, over real DTLS/SRTP.
    void micUplinkRunsOnlyWhileTransmittingOrVoxArmed()
    {
        Test::RemoteAudioSessionHarness h;
        h.declareRemoteTx = true;
        h.grantTransmit = true;
        attachRemoteMicrophone(h, 0.3f, 1000.0);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(h.client.capabilities().txPermitted, 5000);
        QVERIFY(remoteMedia.micLineNegotiated());
        // Media ready: give the connection time to settle, idle.
        QTest::qWait(1500);
        QCOMPARE(remoteMedia.micPacketsSent(), quint64(0));
        QVERIFY(!remoteMedia.micUplinkRunning());
        QCOMPARE(daemonMedia.micReceiver()->stats().accepted, quint64(0));

        const auto runsFor = [&](const std::function<void(bool)>& set) {
            const quint64 before = remoteMedia.micPacketsSent();
            const quint64 coreBefore = daemonMedia.micReceiver()->stats().accepted;
            set(true);
            QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.micPacketsSent() >= before + 20, 5000);
            QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver()->stats().accepted >= coreBefore + 20,
                                     5000);
            QVERIFY(remoteMedia.micUplinkRunning());
            set(false);
            QVERIFY(!remoteMedia.micUplinkRunning());
            const quint64 stopped = remoteMedia.micPacketsSent();
            QTest::qWait(300);
            QCOMPARE(remoteMedia.micPacketsSent(), stopped);
        };
        runsFor([&remoteMedia](bool on) { remoteMedia.setHoldsTransmit(on); });
        runsFor([&remoteMedia](bool on) { remoteMedia.setMicKeyDown(on); });
        // VOX armed (the Core's VOX on, told to this window by its transmit
        // controls), with this session permitted to transmit.
        runsFor([&remoteMedia](bool on) { remoteMedia.setVoxArmed(on); });
        // Idle again: nothing more goes.
        const quint64 idle = remoteMedia.micPacketsSent();
        const quint64 coreIdle = daemonMedia.micReceiver()->stats().accepted;
        QTest::qWait(500);
        QCOMPARE(remoteMedia.micPacketsSent(), idle);
        QTest::qWait(100);
        QCOMPARE(daemonMedia.micReceiver()->stats().accepted, coreIdle);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The window's microphone reaches the Core's transmit ring at its level
    // over real DTLS/SRTP, as Opus and, with lossless chosen, as L16; and a
    // program keying through the window's TCI server is sent in place of
    // the microphone while its audio comes.
    void microphoneAndProgramReachTheCoresRing_data()
    {
        QTest::addColumn<bool>("lossless");
        QTest::newRow("opus") << false;
        QTest::newRow("lossless") << true;
    }

    void microphoneAndProgramReachTheCoresRing()
    {
        QFETCH(bool, lossless);
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.declareRemoteTx = true;
        attachRemoteMicrophone(h, 0.1f, 500.0);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        remoteMedia.setAudioProfileChoice(lossless ? RemoteAudioProfile::Lossless
                                                   : RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const QByteArray micDevice = daemonMedia.micDeviceId();
        QVERIFY(h.station.remoteMicLineOpen(micDevice));
        QTest::qWait(1500);

        // The Core's ring in use, as for a key waiting on it.
        h.station.setRemoteMicPriming(micDevice, true);
        QVERIFY(feed->inUse());
        remoteMedia.setHoldsTransmit(true);
        QTRY_VERIFY_WITH_TIMEOUT(feed->framesSinceInUse() >= 4 * RemoteMicConfig::kTargetDepthFrames,
                                 5000);
        const double mic = pumpRms(feed, 40, 20);
        QVERIFY2(std::abs(20.0 * std::log10(mic / (0.1 / std::sqrt(2.0)))) < 1.5,
                 qPrintable(QString::number(mic)));
        if (lossless) {
            QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver()->stats().decodedPackets > 100, 5000);
        }

        // A program's audio, 20 ms at a time at 48 kHz, in place of the
        // microphone.
        h.station.setRemoteMicPriming(micDevice, false);
        h.station.setRemoteMicPriming(micDevice, true);
        // The pump's next block takes the change (it drops what came before).
        pumpRms(feed, 1, 1);
        for (int chunk = 0; chunk < 25; ++chunk) {
            std::vector<float> program(960);
            for (int i = 0; i < 960; ++i) {
                program[static_cast<size_t>(i)] =
                    0.4f * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * 1000.0
                                                       * (chunk * 960 + i) / 48000.0));
            }
            remoteMedia.pushProgramAudio(program.data(), 960, 1, 48000);
            QTest::qWait(20);
        }
        QTRY_VERIFY_WITH_TIMEOUT(feed->framesSinceInUse() >= 4 * RemoteMicConfig::kTargetDepthFrames,
                                 5000);
        const double programLevel = pumpRms(feed, 40, 20);
        QVERIFY2(std::abs(20.0 * std::log10(programLevel / (0.4 / std::sqrt(2.0)))) < 1.5,
                 qPrintable(QString::number(programLevel)));
        remoteMedia.setHoldsTransmit(false);
        h.station.setRemoteMicPriming(micDevice, false);
        QVERIFY(!feed->inUse());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-15: VAX as the transmit source in a remote window. With VAX
    // chosen the uplink carries the VAX transmit device's audio, never the
    // PC microphone, and takes no microphone capture; with the PC
    // microphone it carries the microphone as before. The VAX audio reaches
    // the Core's ring at the level and in the form a local window's VAX
    // path feeds its transmitter (AudioEngine::pullVaxTxMic: left and right
    // averaged, no gain), measured against that path on the same signal.
    void vaxSourceSendsTheVaxTransmitAudio_data()
    {
        QTest::addColumn<bool>("vax");
        QTest::newRow("vax") << true;
        QTest::newRow("pc microphone") << false;
    }

    void vaxSourceSendsTheVaxTransmitAudio()
    {
        QFETCH(bool, vax);
        constexpr float kVaxLeft = 0.4f;
        constexpr float kVaxRight = 0.2f;
        constexpr double kVaxHz = 700.0;
        constexpr float kMicAmplitude = 0.1f;
        constexpr double kMicHz = 500.0;
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.declareRemoteTx = true;
        attachRemoteMicrophone(h, kMicAmplitude, kMicHz);
        auto device = std::make_unique<PacedVaxTxDevice>(kVaxLeft, kVaxRight, kVaxHz);
        QVERIFY(device->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
        const PacedVaxTxDevice* vaxDevice = device.get();
        h.remote.audioEngine()->setVaxTxBusForTest(std::move(device));
        h.remote.transmitModel().setVaxSourceAvailable(true);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.micReceiver() != nullptr, 5000);
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const QByteArray micDevice = daemonMedia.micDeviceId();
        QVERIFY(h.station.remoteMicLineOpen(micDevice));
        const MicSource source = vax ? MicSource::Vax : MicSource::Pc;
        h.remote.transmitModel().setMicSource(source);
        QTRY_COMPARE(h.remote.transmitModel().micSource(), source);
        // Settled: the Core took the window's audio, and the idle line
        // drains the chosen source (VAX read, or the microphone's preview
        // lease held).
        QTRY_VERIFY_WITH_TIMEOUT(
            h.client.remoteTransmit()->acceptedMicSource() == RemoteMicSource::ClientAudio
                && (vax ? vaxDevice->pulledFrames() > 0 : remoteMedia.micCaptureLeaseHeldForTest()),
            5000);

        h.station.setRemoteMicPriming(micDevice, true);
        QVERIFY(feed->inUse());
        remoteMedia.setHoldsTransmit(true);
        QVERIFY(remoteMedia.micUplinkRunning());
        // VAX takes no PC microphone capture; the microphone keeps its own.
        QCOMPARE(remoteMedia.micCaptureLeaseHeldForTest(), !vax);
        QTRY_VERIFY_WITH_TIMEOUT(feed->framesSinceInUse() >= 4 * RemoteMicConfig::kTargetDepthFrames,
                                 5000);
        // The ring's first blocks after the start are its lead.
        pullFeed(feed, 10);
        const std::vector<float> heard = pullFeed(feed, 20);
        const double vaxPower = tonePower(heard, kVaxHz);
        const double micPower = tonePower(heard, kMicHz);
        // The level is the median of the ring's blocks' RMS: under load a
        // block the ring ran short of is part silence and the splice after
        // it rings, so neither the whole RMS nor the peak is the level.
        std::vector<double> blockRms;
        for (size_t at = 0; at + RemoteMicConfig::kPumpBlockFrames <= heard.size();
             at += RemoteMicConfig::kPumpBlockFrames) {
            double sum = 0.0;
            for (int i = 0; i < RemoteMicConfig::kPumpBlockFrames; ++i) {
                sum += static_cast<double>(heard[at + static_cast<size_t>(i)]) * heard[at + static_cast<size_t>(i)];
            }
            blockRms.push_back(std::sqrt(sum / RemoteMicConfig::kPumpBlockFrames));
        }
        std::sort(blockRms.begin(), blockRms.end());
        const double level = blockRms[blockRms.size() / 2];
        if (vax) {
            QVERIFY2(vaxPower > 100.0 * micPower,
                     qPrintable(QStringLiteral("VAX %1, microphone %2").arg(vaxPower).arg(micPower)));
            QVERIFY(vaxDevice->pulledFrames() > 0);
            // The local window's VAX path (TxWorkerThread's VAX override
            // feeds the transmitter what pullVaxTxMic gives) on the same
            // signal.
            AudioEngine local;
            auto localDevice = std::make_unique<FakeAudioBus>();
            QVERIFY(localDevice->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
            constexpr int kFrames = 4800;
            QByteArray stereo(kFrames * 8, '\0');
            auto* out = reinterpret_cast<float*>(stereo.data());
            for (int i = 0; i < kFrames; ++i) {
                const float s = static_cast<float>(std::sin(
                    2.0 * 3.14159265358979323846 * kVaxHz * i / 48000.0));
                out[2 * i] = kVaxLeft * s;
                out[2 * i + 1] = kVaxRight * s;
            }
            localDevice->setPullData(stereo);
            local.setVaxTxBusForTest(std::move(localDevice));
            std::vector<float> localMono(kFrames);
            QCOMPARE(local.pullVaxTxMic(localMono.data(), kFrames), kFrames);
            double localSum = 0.0;
            for (float v : localMono) { localSum += static_cast<double>(v) * v; }
            const double localRms = std::sqrt(localSum / kFrames);
            // Left and right averaged: a 0.3 tone, where the left channel
            // alone would be 0.4 (2.5 dB more).
            QVERIFY2(std::abs(localRms - 0.3 / std::sqrt(2.0)) < 0.001,
                     qPrintable(QString::number(localRms)));
            qInfo("VAX uplink level %.4f, local VAX path %.4f", level, localRms);
            QVERIFY2(std::abs(20.0 * std::log10(level / localRms)) < 0.5,
                     qPrintable(QStringLiteral("uplink %1, local %2").arg(level).arg(localRms)));
        } else {
            QVERIFY2(micPower > 100.0 * vaxPower,
                     qPrintable(QStringLiteral("VAX %1, microphone %2").arg(vaxPower).arg(micPower)));
            QVERIFY2(std::abs(20.0 * std::log10(level / (kMicAmplitude / std::sqrt(2.0)))) < 0.5,
                     qPrintable(QString::number(level)));
        }
        remoteMedia.setHoldsTransmit(false);
        h.station.setRemoteMicPriming(micDevice, false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-15 (fix wave): the VAX TX device has one reader. With VAX the
    // transmit source and VOX armed, a program key that brings its own
    // audio leaves the device alone (a remote window's rigctld key reads it
    // for the program's audio), and the program's audio still reaches the
    // Core. When the key ends with VAX still chosen and VOX still wanting
    // the line, the uplink reads the VAX device again, and what an app
    // wrote while nothing read it is not sent: the packets after the key
    // are no more than the time since it.
    void aProgramAudioKeyLeavesTheVaxDeviceToTheProgram()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.1f, 500.0);
        auto device = std::make_unique<PacedVaxTxDevice>(0.1f, 0.1f, 700.0);
        QVERIFY(device->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
        const PacedVaxTxDevice* vaxDevice = device.get();
        h.remote.audioEngine()->setVaxTxBusForTest(std::move(device));
        h.remote.transmitModel().setVaxSourceAvailable(true);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QTRY_COMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        media.setVoxArmed(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        const qint64 beforeKey = vaxDevice->pulledFrames();
        QTRY_VERIFY_WITH_TIMEOUT(vaxDevice->pulledFrames() > beforeKey, 5000);

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, true);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QVERIFY(h.client.remoteTransmit()->programAudioKey());
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        // Nothing here reads the VAX device for the whole key.
        const qint64 duringKey = vaxDevice->pulledFrames();
        pushProgramChunks(media, 15, 0.25f);
        QCOMPARE(vaxDevice->pulledFrames(), duringKey);
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const std::vector<float> heard = pullFeed(feed, 900);
        const float highest = *std::max_element(heard.begin(), heard.end());
        QVERIFY2(highest > 0.2f, qPrintable(QString::number(highest)));   // the program came
        // Past the program's hold, the key still on: the device fills.
        QTest::qWait(RemoteMediaController::kProgramAudioHoldMs + 100);
        QCOMPARE(vaxDevice->pulledFrames(), duringKey);

        QElapsedTimer sinceKey;
        sinceKey.start();
        const quint64 sentBefore = media.micPacketsSent();
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QVERIFY(!h.client.remoteTransmit()->programAudioKey());
        QVERIFY(media.micUplinkRunning());
        QTRY_VERIFY_WITH_TIMEOUT(vaxDevice->pulledFrames() > duringKey, 5000);
        QTest::qWait(10 * RemoteMediaController::kMicPumpIntervalMs);
        // The collector reads on (not only the drop at the restart).
        const qint64 resumed = vaxDevice->pulledFrames();
        QTRY_VERIFY_WITH_TIMEOUT(vaxDevice->pulledFrames() > resumed, 5000);
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        // 2026-10-15 (fix wave 2): the VAX feed's packets are waited for,
        // not assumed within a fixed time (a starved collector thread).
        QTRY_VERIFY_WITH_TIMEOUT(media.micPacketsSent() > sentBefore, 5000);
        // 4 ms packets: no more than the time since the key, and some slack;
        // the program run's backlog alone would be over 125.
        const quint64 sentAfter = media.micPacketsSent() - sentBefore;
        const qint64 elapsedMs = sinceKey.elapsed();
        QVERIFY2(sentAfter <= quint64(elapsedMs * 48 / PcmAudioCodecConfig::kPacketFrames + 10),
                 qPrintable(QStringLiteral("%1 packets in %2 ms").arg(sentAfter).arg(elapsedMs)));
        media.setVoxArmed(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-15 (fix wave 2): with the screen key down and the line on
    // VAX, a program key that brings its own audio does not move the key
    // down, yet the collector is joined before keyForProgram returns: the
    // program's pump reads the VAX device on its first tick right after,
    // and nothing else reads it then or later in the key. When the program
    // lets go with the screen key still down, the collector reads VAX
    // again at once.
    void aProgramAudioKeyUnderTheScreenKeyTakesTheVaxDeviceAtOnce()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        // The collector's own thread (the harness collects inline).
        RemoteMediaController::setMicCollectorInlineForTest(false);
        const auto inlineAgain = qScopeGuard([] {
            RemoteMediaController::setMicCollectorInlineForTest(true);
        });
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.1f, 500.0);
        auto device = std::make_unique<PacedVaxTxDevice>(0.1f, 0.1f, 700.0);
        QVERIFY(device->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
        PacedVaxTxDevice* vaxDevice = device.get();
        AudioEngine* engine = h.remote.audioEngine();
        engine->setVaxTxBusForTest(std::move(device));
        h.remote.transmitModel().setVaxSourceAvailable(true);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QTRY_COMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        RemoteTransmitClient* transmit = h.client.remoteTransmit();
        transmit->setScreenKey(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        const qint64 beforeKey = vaxDevice->pulledFrames();
        QTRY_VERIFY_WITH_TIMEOUT(vaxDevice->pulledFrames() > beforeKey, 5000);
        QVERIFY(media.micCollectorThreadRunningForTest());

        std::optional<RemoteTransmitClient::Answer> answer;
        transmit->keyForProgram(true, [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        // Right after the call, no event loop in between: the key is still
        // the screen's, and the collector is already joined.
        QVERIFY(transmit->micKeyDown());
        QVERIFY(transmit->programAudioKey());
        QVERIFY(!media.micCollectorThreadRunningForTest());
        // The program's pump reads the device from its first tick on.
        vaxDevice->setProgramReaderIsThisThread(true);
        const qint64 atKey = vaxDevice->pulledFrames();
        std::vector<float> scratch(960);
        const std::vector<float> program(960, 0.25f);
        engine->pullVaxTxMic(scratch.data(), 960);
        for (int tick = 0; tick < 15; ++tick) {
            QTest::qWait(20);
            engine->pullVaxTxMic(scratch.data(), 960);
            media.pushProgramAudio(program.data(), 960, 1, 48000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(answer.has_value(), 5000);
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QVERIFY(vaxDevice->pulledFrames() > atKey);   // the program's own reads
        QCOMPARE(vaxDevice->readsBesideTheProgram(), 0);
        QCOMPARE(vaxDevice->overlappingReads(), 0);
        QVERIFY(!media.micCollectorThreadRunningForTest());
        // The program's audio goes out.
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const std::vector<float> heard = pullFeed(feed, 900);
        const float highest = *std::max_element(heard.begin(), heard.end());
        QVERIFY2(highest > 0.2f, qPrintable(QString::number(highest)));

        // The program stops reading, then lets go; the screen key stays.
        vaxDevice->setProgramReaderIsThisThread(false);
        transmit->unkeyForProgram(answer->epoch);
        QVERIFY(!transmit->programAudioKey());
        QVERIFY(transmit->micKeyDown());
        QVERIFY(media.micCollectorThreadRunningForTest());
        const qint64 afterKey = vaxDevice->pulledFrames();
        QTRY_VERIFY_WITH_TIMEOUT(vaxDevice->pulledFrames() > afterKey, 5000);
        QVERIFY(h.station.moxController()->isMox());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QCOMPARE(vaxDevice->overlappingReads(), 0);

        transmit->setScreenKey(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-15: a remote window's rigctld (WSJT-X's CAT) keys as the
    // window's program and sends what programs play into VAX TX, never the
    // PC microphone, whether the window's transmit source is the PC
    // microphone or VAX. Two tones tell them apart at the Core (Goertzel),
    // not a flag. While the key is on, only the rigctld's pump reads the
    // VAX device; the collector runs on its own thread, as in the app.
    void aRigctldKeySendsVaxTxNeverTheMicrophone_data()
    {
        QTest::addColumn<bool>("vaxSource");
        QTest::newRow("pc microphone chosen") << false;
        QTest::newRow("vax chosen") << true;
    }

    void aRigctldKeySendsVaxTxNeverTheMicrophone()
    {
        QFETCH(bool, vaxSource);
        constexpr double kVaxHz = 700.0;
        constexpr double kMicHz = 500.0;
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController::setMicCollectorInlineForTest(false);
        const auto inlineAgain = qScopeGuard([] {
            RemoteMediaController::setMicCollectorInlineForTest(true);
        });
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.3f, kMicHz);
        auto device = std::make_unique<PacedVaxTxDevice>(0.4f, 0.2f, kVaxHz);
        QVERIFY(device->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32}));
        PacedVaxTxDevice* vaxDevice = device.get();
        h.remote.audioEngine()->setVaxTxBusForTest(std::move(device));
        h.remote.transmitModel().setVaxSourceAvailable(true);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        const MicSource source = vaxSource ? MicSource::Vax : MicSource::Pc;
        h.remote.transmitModel().setMicSource(source);
        QTRY_COMPARE(h.remote.transmitModel().micSource(), source);
        QTRY_VERIFY_WITH_TIMEOUT(
            h.client.remoteTransmit()->acceptedMicSource() == RemoteMicSource::ClientAudio, 5000);

        // Wired as MainWindow::refreshRigctlRemoteTransmit wires it.
        RemoteRigctlServer rig(&h.remote);
        const QPointer<RemoteTransmitClient> client(h.client.remoteTransmit());
        rig.setProgramAudioKey([client] { return client && client->programAudioKey(); });
        rig.setRemoteTransmit(remoteTransmitForwarder(h.client.remoteTransmit(), &media));
        QVERIFY(rig.requestKey(1));
        QVERIFY(h.client.remoteTransmit()->programAudioKey());
        // keyForProgram has returned: from here the rigctld's pump, on this
        // thread, is the VAX device's one reader.
        vaxDevice->setProgramReaderIsThisThread(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QVERIFY(rig.keyed());
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        // Everything the Core takes from key-up on, about in real time:
        // 120 steps of four pump blocks (5.3 ms) each, one letter a step.
        // The feed starts silent while it fills ('.'), and the block where
        // the audio starts can be part silence ('?'); after that every step
        // is the VAX tone ('V'), and no step is ever the microphone's ('M').
        constexpr int kStepFrames = 4 * RemoteMicConfig::kPumpBlockFrames;
        QString timeline;
        std::vector<float> steady;
        for (int step = 0; step < 120; ++step) {
            QTest::qWait(5);
            const std::vector<float> chunk = pullFeed(feed, 4);
            double sum = 0.0;
            for (float v : chunk) { sum += static_cast<double>(v) * v; }
            const double rms = std::sqrt(sum / kStepFrames);
            const double mic = tonePower(chunk, kMicHz);
            const double vax = tonePower(chunk, kVaxHz);
            const QChar mark = rms < 0.02 ? QChar('.')
                : mic > 4.0 * vax         ? QChar('M')
                : vax > 4.0 * mic         ? QChar('V')
                                          : QChar('?');
            timeline += mark;
            // The steady part: from the fifth step after the VAX tone came.
            if (timeline.count(QChar('V')) > 4) {
                steady.insert(steady.end(), chunk.begin(), chunk.end());
            }
        }
        QVERIFY2(!timeline.contains(QChar('M')), qPrintable(timeline));
        QVERIFY2(timeline.count(QChar('V')) > 60, qPrintable(timeline));
        const double vaxPower = tonePower(steady, kVaxHz);
        const double micPower = tonePower(steady, kMicHz);
        QVERIFY2(vaxPower > 100.0 * micPower,
                 qPrintable(QStringLiteral("VAX %1, microphone %2").arg(vaxPower).arg(micPower)));
        QCOMPARE(vaxDevice->readsBesideTheProgram(), 0);
        QCOMPARE(vaxDevice->overlappingReads(), 0);

        // T 0: the pump stops before the unkey, and the key ends at the Core.
        rig.releaseKey(1);
        vaxDevice->setProgramReaderIsThisThread(false);
        QVERIFY(!rig.keyed());
        QVERIFY(!h.client.remoteTransmit()->programAudioKey());
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QCOMPARE(vaxDevice->overlappingReads(), 0);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // ---- 2026-10-08: TCI program keys that bring their own audio ----

    // Case A: a program key with its own audio (",tci") and a microphone
    // that delivers nothing: the line starts with the key and carries
    // silence at the line's real-time rate, every sample of it zero at the
    // Core, and the Core admits the key. Without ",tci" the same window is
    // refused for no sound, as a screen key would be.
    void aProgramAudioKeyWithNoMicrophoneIsAdmittedOnSilence_data()
    {
        QTest::addColumn<bool>("programAudio");
        QTest::addColumn<bool>("lossless");
        QTest::newRow("tci opus") << true << false;
        QTest::newRow("tci lossless") << true << true;
        QTest::newRow("plain") << false << false;
    }

    void aProgramAudioKeyWithNoMicrophoneIsAdmittedOnSilence()
    {
        QFETCH(bool, programAudio);
        QFETCH(bool, lossless);
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(lossless ? RemoteAudioProfile::Lossless
                                             : RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, programAudio);
        QVERIFY(answer.has_value());
        if (!programAudio) {
            QVERIFY(!answer->accepted);
            QCOMPARE(answer->code, QStringLiteral("micNotReady"));
            QVERIFY(!h.station.moxController()->isMox());
            h.client.disconnectFromStation(QStringLiteral("test complete"));
            return;
        }
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QCOMPARE(h.station.keyedBy().trigger, QByteArrayLiteral("tci"));
        QVERIFY(media.micUplinkRunning());

        // Real time: one packet per packet's length of wall clock.
        const int packetMs = lossless ? PcmAudioCodecConfig::kPacketFrames / 48
                                      : RemoteMicConfig::kOpusFrameSamples / 48;
        const quint64 before = media.micPacketsSent();
        QElapsedTimer span;
        span.start();
        QTest::qWait(600);
        const qint64 expected = span.elapsed() / packetMs;
        const qint64 sent = qint64(media.micPacketsSent() - before);
        // Within two pump intervals of real time at either end.
        const qint64 slack = 2 * RemoteMediaController::kMicPumpIntervalMs / packetMs + 1;
        QVERIFY2(sent >= expected - slack && sent <= expected + slack,
                 qPrintable(QStringLiteral("%1 packets in %2 ms").arg(sent).arg(span.elapsed())));

        // Every sample the Core decoded is zero: exactly, over lossless;
        // over Opus, within the codec's own noise of digital silence
        // (measured near -89 dBFS), far below any microphone.
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr && feed->inUse());
        QVERIFY(feed->framesSinceInUse() >= 4 * RemoteMicConfig::kTargetDepthFrames);
        float loudest = 0.0f;
        for (float v : pullFeed(feed, 400)) { loudest = std::max(loudest, std::abs(v)); }
        if (lossless) {
            QCOMPARE(loudest, 0.0f);
        } else {
            QVERIFY2(loudest < 1.0e-3f, qPrintable(QString::number(loudest)));
        }

        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QVERIFY(!media.micUplinkRunning());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Case B: a program key with its own audio and a microphone delivering
    // a tone: no tone sample reaches the Core before, during or after the
    // program's audio, only silence and the program. The same key without
    // ",tci" sends the tone, so the check can fail.
    void aProgramAudioKeyNeverSendsTheMicrophone_data()
    {
        QTest::addColumn<bool>("programAudio");
        QTest::newRow("tci") << true;
        QTest::newRow("plain") << false;
    }

    void aProgramAudioKeyNeverSendsTheMicrophone()
    {
        QFETCH(bool, programAudio);
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.3f, 1000.0);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, programAudio);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        QTest::qWait(200);
        pushProgramChunks(media, 15, 0.25f);
        // Ends inside the program's hold: what follows it is not this case.
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const std::vector<float> heard = pullFeed(feed, 900);
        const float lowest = *std::min_element(heard.begin(), heard.end());
        const float highest = *std::max_element(heard.begin(), heard.end());
        QVERIFY2(highest > 0.2f, qPrintable(QString::number(highest)));   // the program came
        if (programAudio) {
            QVERIFY2(lowest > -0.05f, qPrintable(QString::number(lowest)));
        } else {
            QVERIFY2(lowest < -0.2f, qPrintable(QString::number(lowest)));
        }
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Case C: the program's audio stops for longer than its hold during
    // the key: the line keeps going with silence, never the tone. The same
    // key without ",tci" hears the tone in the lapse, so the check can fail.
    void aProgramAudioLapseSendsSilenceNotTheMicrophone_data()
    {
        QTest::addColumn<bool>("programAudio");
        QTest::newRow("tci") << true;
        QTest::newRow("plain") << false;
    }

    void aProgramAudioLapseSendsSilenceNotTheMicrophone()
    {
        QFETCH(bool, programAudio);
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.3f, 1000.0);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, programAudio);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        pushProgramChunks(media, 10, 0.25f);
        // The lapse: past the hold, the line goes on at real time.
        QTest::qWait(RemoteMediaController::kProgramAudioHoldMs + 50);
        const quint64 lapseStart = media.micPacketsSent();
        QTest::qWait(300);
        const quint64 lapsed = media.micPacketsSent() - lapseStart;
        QVERIFY2(lapsed >= 300 / (PcmAudioCodecConfig::kPacketFrames / 48) - 5,
                 qPrintable(QString::number(lapsed)));
        pushProgramChunks(media, 5, 0.25f);
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const std::vector<float> heard = pullFeed(feed, 1200);
        const float lowest = *std::min_element(heard.begin(), heard.end());
        if (programAudio) {
            QVERIFY2(lowest > -0.05f, qPrintable(QString::number(lowest)));
        } else {
            QVERIFY2(lowest < -0.2f, qPrintable(QString::number(lowest)));
        }
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Case D: after a program key with its own audio ends, a screen key
    // sends the microphone again; nothing stays silenced.
    void aScreenKeyAfterAProgramAudioKeySendsTheMicrophone()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        attachRemoteMicrophone(h, 0.3f, 1000.0);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, true);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QVERIFY(!media.micUplinkRunning());
        QVERIFY(!h.client.remoteTransmit()->programAudioKey());

        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const QByteArray micDevice = daemon.micDeviceId();
        h.station.setRemoteMicPriming(micDevice, true);
        QVERIFY(feed->inUse());
        media.setMicKeyDown(true);
        QVERIFY(media.micUplinkRunning());
        QTRY_VERIFY_WITH_TIMEOUT(feed->framesSinceInUse() >= 4 * RemoteMicConfig::kTargetDepthFrames,
                                 5000);
        const double mic = pumpRms(feed, 40, 20);
        QVERIFY2(mic > 0.1, qPrintable(QString::number(mic)));
        media.setMicKeyDown(false);
        h.station.setRemoteMicPriming(micDevice, false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Case F: a capture in Failed is retried once when a key starts the
    // line with the microphone, never on later pumps and never for a
    // program key that brings its own audio.
    void aFailedCaptureRetriesOnceForAKeyThatSendsTheMicrophone()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        // This fake session uses VAX to stop preview capture on every platform.
        h.remote.transmitModel().setVaxSourceAvailable(true);
        h.pairWindow = true;
        h.makeTransmitReady();
        AudioEngine* engine = h.remote.audioEngine();
        CaptureSupervisor::Options options;
        options.program = QDir(h.directory.path()).filePath(QStringLiteral("no-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        // Demand that outlives every key, so only a retry starts a new
        // generation.
        CaptureSupervisor::Lease held = engine->acquireCaptureDemand(
            CaptureSupervisor::Demand::TestMic);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        const quint32 failed = engine->captureStatus().generation;

        // A program key with its own audio: no retry. With the local
        // preview off the line holds no capture lease at all, and the key
        // still sends its silence.
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        QTRY_VERIFY(!media.micCaptureLeaseHeldForTest());
        QVERIFY(takeTransmit(h));
        const quint64 beforeProgram = media.micPacketsSent();
        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, true);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QTRY_VERIFY_WITH_TIMEOUT(media.micPacketsSent() >= beforeProgram + 10, 5000);
        QCOMPARE(engine->captureStatus().generation, failed);
        QCOMPARE(engine->captureStatus().state, CaptureSupervisor::Status::State::Failed);
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QTRY_VERIFY(!media.micUplinkRunning());
        // The program key ended at the Core before the next leg.
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QVERIFY(!h.client.remoteTransmit()->micKeyDown());

        // The preview back on: the lease is held again before the key, so
        // the key's start is what retries.
        h.remote.transmitModel().setMicSource(MicSource::Pc);
        QTRY_VERIFY(media.micCaptureLeaseHeldForTest());
        QCOMPARE(engine->captureStatus().generation, failed);

        // A key that sends the microphone: one retry, on its start.
        media.setMicKeyDown(true);
        QVERIFY(media.micUplinkRunning());
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > failed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        const quint32 retried = engine->captureStatus().generation;
        // Many pumps later, still that one retry.
        QTest::qWait(30 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, retried);
        media.setMicKeyDown(false);
        held.release();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Case G: a program key's silence is paced by the clock, not by pump
    // ticks: it starts with the Core's target depth at once, follows real
    // time, sends at most the Core's deepest buffer after a 300 ms stall,
    // and is back at real time after it.
    void programSilenceIsPacedByTheClockAndBoundedAfterAStall()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        qint64 nowMs = 100000;
        media.setMicClockForTest([&nowMs] { return nowMs; });
        constexpr int kPacketMs = RemoteMicConfig::kOpusFrameSamples / 48;   // 20 ms

        // The start: the Core's target depth (30 ms) at once, in whole
        // packets: two, before any pump.
        const quint64 idle = media.micPacketsSent();
        std::optional<RemoteTransmitClient::Answer> answer;
        h.client.remoteTransmit()->keyForProgram(true,
            [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        QVERIFY(media.micUplinkRunning());
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));
        // Pumps with the clock standing still send nothing more.
        QTest::qWait(5 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));
        QElapsedTimer waited;
        waited.start();
        while (!answer && waited.elapsed() < 5000) {
            nowMs += 10;
            QTest::qWait(10);
        }
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));

        // Real time on the clock: one packet per 20 ms of it.
        const auto sentOver = [&](int steps) {
            const quint64 before = media.micPacketsSent();
            for (int i = 0; i < steps; ++i) {
                nowMs += 10;
                QTest::qWait(2 * RemoteMediaController::kMicPumpIntervalMs);
            }
            return qint64(media.micPacketsSent() - before);
        };
        qint64 steady = sentOver(20);
        QVERIFY2(steady >= 200 / kPacketMs - 1 && steady <= 200 / kPacketMs + 1,
                 qPrintable(QString::number(steady)));

        // A 300 ms stall: the next pump sends at most kMaxDepthMs of
        // silence (six packets), not the 300 ms the clock moved.
        const quint64 beforeStall = media.micPacketsSent();
        nowMs += 300;
        QTest::qWait(5 * RemoteMediaController::kMicPumpIntervalMs);
        const qint64 caughtUp = qint64(media.micPacketsSent() - beforeStall);
        QVERIFY2(caughtUp >= RemoteMicConfig::kMaxDepthMs / kPacketMs - 1
                     && caughtUp <= RemoteMicConfig::kMaxDepthMs / kPacketMs,
                 qPrintable(QString::number(caughtUp)));

        // Back at real time.
        steady = sentOver(20);
        QVERIFY2(steady >= 200 / kPacketMs - 1 && steady <= 200 / kPacketMs + 1,
                 qPrintable(QString::number(steady)));

        media.setMicClockForTest({});
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-09 (review item 1): the end of a program key reaches the line
    // as key-down off, then holds-transmit off. Between them, with the
    // program's audio lapsed past its hold and the capture holding a tone,
    // nothing is sent: no microphone sample follows the program key.
    void aProgramAudioUnkeySendsNoMicrophone()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        FakeAudioBus* capture = microphone.get();
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        // The local preview holds the capture, as it does by default.
        QTRY_VERIFY(media.micCaptureLeaseHeldForTest());

        const std::optional<RemoteTransmitClient::Answer> answer = keyProgram(h, true);
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTRY_VERIFY_WITH_TIMEOUT(h.station.moxController()->isMox(), 5000);
        pushProgramChunks(media, 5, 0.25f);
        QTest::qWait(RemoteMediaController::kProgramAudioHoldMs + 100);

        // 200 ms of tone waiting in the capture as the key ends.
        capture->setPullData(toneBytes(9600, 0.3f));
        const quint64 before = media.micPacketsSent();
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QVERIFY(!media.micUplinkRunning());
        QCOMPARE(media.micPacketsSent(), before);

        RemoteMicFeed* feed = h.station.remoteMicFeed();
        QVERIFY(feed != nullptr);
        const std::vector<float> heard = pullFeed(feed, 1200);
        const float lowest = *std::min_element(heard.begin(), heard.end());
        QVERIFY2(lowest > -0.05f, qPrintable(QString::number(lowest)));
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-09 (review item 2): the link lost in the middle of a program
    // key's silence; the next program key starts with the Core's target
    // depth (two Opus packets), not a capped burst of the old key's debt.
    void aLinkLossDuringProgramSilenceLeavesTheNextKeyItsLead()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        qint64 nowMs = 100000;
        media.setMicClockForTest([&nowMs] { return nowMs; });

        std::optional<RemoteTransmitClient::Answer> answer;
        h.client.remoteTransmit()->keyForProgram(true,
            [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        QVERIFY(media.micUplinkRunning());
        QElapsedTimer waited;
        waited.start();
        while (!answer && waited.elapsed() < 5000) {
            nowMs += 10;
            QTest::qWait(10);
        }
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        for (int i = 0; i < 10; ++i) {
            nowMs += 10;
            QTest::qWait(2 * RemoteMediaController::kMicPumpIntervalMs);
        }

        // The link goes in the middle of the key.
        h.stationLink->closeLink(QStringLiteral("link lost"));
        QTRY_VERIFY_WITH_TIMEOUT(!media.micUplinkRunning(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        nowMs += 5000;

        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        const quint64 idle = media.micPacketsSent();
        std::optional<RemoteTransmitClient::Answer> next;
        h.client.remoteTransmit()->keyForProgram(true,
            [&next](const RemoteTransmitClient::Answer& a) { next = a; });
        QVERIFY(media.micUplinkRunning());
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));
        QTest::qWait(5 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));

        media.setMicClockForTest({});
        QTRY_VERIFY_WITH_TIMEOUT(next.has_value(), 5000);
        if (next->accepted) {
            h.client.remoteTransmit()->unkeyForProgram(next->epoch);
            QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        }
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-09 (review item 3): a capture in Failed with no demand at
    // all: a key that sends the microphone takes the lease, which opens a
    // new generation by itself; no retry opens a second one.
    void aFailedCaptureFromNoDemandStartsOneGeneration()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        // This fake session uses VAX to stop preview capture on every platform.
        h.remote.transmitModel().setVaxSourceAvailable(true);
        h.pairWindow = true;
        h.makeTransmitReady();
        AudioEngine* engine = h.remote.audioEngine();
        CaptureSupervisor::Options options;
        options.program = QDir(h.directory.path()).filePath(QStringLiteral("no-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        // The preview's own generation has failed by now.
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > 0
                                     && engine->captureStatus().state
                                            == CaptureSupervisor::Status::State::Failed,
                                 5000);
        // No local preview: nothing of this window demands the capture.
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        QTRY_VERIFY(!media.micCaptureLeaseHeldForTest());
        {
            // A demand of its own opens (and fails) a generation, then goes.
            const quint32 previewed = engine->captureStatus().generation;
            CaptureSupervisor::Lease probe = engine->acquireCaptureDemand(
                CaptureSupervisor::Demand::TestMic);
            QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > previewed, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                      CaptureSupervisor::Status::State::Failed, 5000);
        }
        QTest::qWait(50);
        QCOMPARE(engine->captureStatus().state, CaptureSupervisor::Status::State::Failed);
        const quint32 failed = engine->captureStatus().generation;

        // 2026-10-15: a key with VAX the source sends VAX and takes no
        // capture at all; the running line moved onto the microphone takes
        // the lease from no demand, which opens one generation by itself.
        media.setMicKeyDown(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QTest::qWait(10 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, failed);
        h.remote.transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > failed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        QTest::qWait(30 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, failed + 1);
        media.setMicKeyDown(false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-09 (fix round 2): a capture in Failed held open by another
    // demand (Test Mic here) while this window holds no lease: a key that
    // sends the microphone takes a lease, which opens nothing by itself,
    // so the key retries once: exactly one new generation.
    void aFailedCaptureHeldByAnotherDemandRetriesOnceForAKey()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        // This fake session uses VAX to stop preview capture on every platform.
        h.remote.transmitModel().setVaxSourceAvailable(true);
        h.pairWindow = true;
        h.makeTransmitReady();
        AudioEngine* engine = h.remote.audioEngine();
        CaptureSupervisor::Options options;
        options.program = QDir(h.directory.path()).filePath(QStringLiteral("no-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        // The preview's own generation has failed by now.
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > 0
                                     && engine->captureStatus().state
                                            == CaptureSupervisor::Status::State::Failed,
                                 5000);
        // No local preview: this window holds no lease.
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        QTRY_VERIFY(!media.micCaptureLeaseHeldForTest());
        const quint32 previewed = engine->captureStatus().generation;
        CaptureSupervisor::Lease other = engine->acquireCaptureDemand(
            CaptureSupervisor::Demand::TestMic);
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > previewed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        const quint32 failed = engine->captureStatus().generation;

        // 2026-10-15: a key with VAX the source takes no lease and retries
        // nothing; the running line moved onto the microphone takes a
        // lease, which opens nothing by itself, so it retries once.
        media.setMicKeyDown(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QTest::qWait(10 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, failed);
        h.remote.transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > failed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        QTest::qWait(30 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, failed + 1);
        media.setMicKeyDown(false);
        other.release();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-09 (fix round 2): only the media connection is lost in the
    // middle of a program key's silence; the control session and the key
    // survive it, and the line stops with the media. The window then
    // recovers as it does (a new session); the next program key starts
    // with the Core's target depth (two Opus packets), not a capped burst
    // of the old key's debt.
    void aMediaLossDuringProgramSilenceLeavesTheNextKeyItsLead()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        auto microphone = std::make_unique<FakeAudioBus>();
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Opus);
        QSignalSpy recoveries(&media, &RemoteMediaController::recoveryRequested);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        qint64 nowMs = 100000;
        media.setMicClockForTest([&nowMs] { return nowMs; });

        std::optional<RemoteTransmitClient::Answer> answer;
        h.client.remoteTransmit()->keyForProgram(true,
            [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        QVERIFY(media.micUplinkRunning());
        QElapsedTimer waited;
        waited.start();
        while (!answer && waited.elapsed() < 5000) {
            nowMs += 10;
            QTest::qWait(10);
        }
        QVERIFY(answer.has_value());
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        for (int i = 0; i < 10; ++i) {
            nowMs += 10;
            QTest::qWait(2 * RemoteMediaController::kMicPumpIntervalMs);
        }

        // Only the media connection goes; the session and the key stay.
        media.dropMediaPeerForTest();
        QCOMPARE(recoveries.size(), 1);
        QVERIFY(!media.micUplinkRunning());
        QVERIFY(h.client.isHandshakeComplete());
        QVERIFY(h.client.remoteTransmit()->programAudioKey());
        nowMs += 5000;

        // The window's recovery: a new session (RemoteConnectionController
        // answers recoveryRequested with a disconnect and a reconnect).
        h.client.disconnectFromStation(QStringLiteral("station media connection closed"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QVERIFY(takeTransmit(h));
        const quint64 idle = media.micPacketsSent();
        std::optional<RemoteTransmitClient::Answer> next;
        h.client.remoteTransmit()->keyForProgram(true,
            [&next](const RemoteTransmitClient::Answer& a) { next = a; });
        QVERIFY(media.micUplinkRunning());
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));
        QTest::qWait(5 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(media.micPacketsSent() - idle, quint64(2));

        media.setMicClockForTest({});
        QTRY_VERIFY_WITH_TIMEOUT(next.has_value(), 5000);
        if (next->accepted) {
            h.client.remoteTransmit()->unkeyForProgram(next->epoch);
            QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        }
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // 2026-10-10 (bench: a Windows window on a remote Core lost microphone
    // audio on every key): the pump does not run for 150 ms of the
    // microphone's clock while keyed. With nobody collecting meanwhile the
    // microphone's pacing gives the late pump 30 ms of it; with the
    // collector stepping every 5 ms, all 150 ms is sent afterwards.
    void aStalledPumpSendsAllTheMicrophoneAudioTheCollectorPulled()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        qint64 nowFrames = 0;
        auto microphone = std::make_unique<ClockPacedMicrophone>(&nowFrames);
        QVERIFY(microphone->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        h.remote.audioEngine()->setTxInputBusForTest(std::move(microphone));
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        constexpr int kPacketFrames = RemoteMicConfig::kOpusFrameSamples;   // 20 ms
        constexpr int kStallMs = 150;

        // From here on no event runs, so the pump's timer never does: the
        // test runs the pump and the collector's steps itself.
        media.setHoldsTransmit(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QVERIFY(!media.micCollectorThreadRunningForTest());   // inline in tests
        media.pumpMicUplinkForTest();
        const quint64 start = media.micPacketsSent();

        // Nobody collects during the stall: the late pump gets the pacing's
        // credit and slack (30 ms), one packet, and the rest is lost.
        nowFrames += kStallMs * 48;
        media.pumpMicUplinkForTest();
        const quint64 lost = media.micPacketsSent();
        QCOMPARE(lost - start, quint64((CaptureAudioBus::kPaceCreditCapFrames
                                        + CaptureAudioBus::kPaceSlackFrames) / kPacketFrames));
        // The half packet that pump left waiting.
        const int waiting = (CaptureAudioBus::kPaceCreditCapFrames
                             + CaptureAudioBus::kPaceSlackFrames) % kPacketFrames;

        // The collector steps every 5 ms of the same stall: the pump then
        // sends every frame of the 150 ms.
        int collected = 0;
        for (int ms = 0; ms < kStallMs; ms += 5) {
            nowFrames += 5 * 48;
            collected += media.collectMicNowForTest();
        }
        QCOMPARE(collected, kStallMs * 48);
        QCOMPARE(media.micPacketsSent(), lost);                // the pump has not run
        media.pumpMicUplinkForTest();
        QCOMPARE(media.micPacketsSent() - lost,
                 quint64((waiting + kStallMs * 48) / kPacketFrames));
        QVERIFY((waiting + kStallMs * 48) % kPacketFrames == 0);

        media.setHoldsTransmit(false);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The collector can intentionally pause while Test Mic keeps shared
    // capture Ready. Only time in the current collector run is a stall.
    void anIntentionalCollectorPauseIsNotReportedAsAStall()
    {
        class GatedMicrophone final : public FakeAudioBus {
        public:
            qint64 pull(char* destination, qint64 bytes) override
            {
                if (bytes < qint64(sizeof(float)) || !block.exchange(false)) {
                    return 0;
                }
                const float sample = 0.25f;
                std::memcpy(destination, &sample, sizeof(sample));
                return sizeof(sample);
            }
            std::atomic<bool> block{false};
        };
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController::setMicCollectorInlineForTest(false);
        const auto inlineAgain = qScopeGuard([] {
            RemoteMediaController::setMicCollectorInlineForTest(true);
        });
        AudioEngine* engine = h.remote.audioEngine();
        CaptureSupervisor::Options options;
        options.program = QCoreApplication::applicationFilePath();
        options.arguments = {QStringLiteral("--fake-capture-child"), QStringLiteral("ready")};
        options.helloTimeoutMs = Test::kFakeCaptureChildStartMs;
        engine->setCaptureSupervisorOptionsForTest(options);
        auto microphone = std::make_unique<GatedMicrophone>();
        GatedMicrophone* source = microphone.get();
        QVERIFY(source->open(AudioFormat{48000, 1, AudioFormat::Sample::Float32}));
        engine->setTxInputBusForTest(std::move(microphone));
        CaptureSupervisor::Lease other = engine->acquireCaptureDemand(
            CaptureSupervisor::Demand::TestMic);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        qint64 nowMs = 10000;
        media.setMicClockForTest([&nowMs] { return nowMs; });
        const auto resetClock = qScopeGuard([&media] { media.setMicClockForTest({}); });
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        h.remote.transmitModel().setMicSource(MicSource::Pc);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Ready,
                                  Test::kFakeCaptureChildStartMs);
        const quint32 generation = engine->captureStatus().generation;
        // Media readiness and the capture helper can both settle before the
        // 20 ms microphone pump reconciles the default PC-preview lease.
        // 2026-10-15 (fix wave 2): Ready comes from the Test Mic demand,
        // which can be before this window's preview takes its lease (the
        // Core's answer to the source change); the collector is waited for.
        QTRY_VERIFY_WITH_TIMEOUT(media.micCollectorThreadRunningForTest(), 5000);
        media.pumpMicUplinkForTest();

        g_remoteMediaMessages.clear();
        const QtMessageHandler previous = qInstallMessageHandler(captureRemoteMediaMessages);
        const auto restoreMessages = qScopeGuard([previous] { qInstallMessageHandler(previous); });
        const auto stalls = [] {
            return g_remoteMediaMessages.filter(QStringLiteral("Microphone collector took no audio"));
        };
        const auto recoveries = [] {
            return g_remoteMediaMessages.filter(QStringLiteral("Microphone collector is taking audio again"));
        };
        // This fake session needs VAX as its no-preview source on every platform.
        h.remote.transmitModel().setVaxSourceAvailable(true);
        const auto pause = [&] {
            h.remote.transmitModel().setMicSource(MicSource::Vax);
            media.pumpMicUplinkForTest();
        };
        const auto resume = [&] {
            h.remote.transmitModel().setMicSource(MicSource::Pc);
            media.pumpMicUplinkForTest();
        };
        pause();
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        QVERIFY(!media.micCollectorThreadRunningForTest());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QCOMPARE(engine->captureStatus().state, CaptureSupervisor::Status::State::Ready);
        QCOMPARE(engine->captureStatus().generation, generation);
        nowMs += 5000;
        resume();
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Pc);
        QVERIFY(media.micCollectorThreadRunningForTest());
        QCOMPARE(engine->captureStatus().generation, generation);
        QCOMPARE(stalls().size(), 0);
        QCOMPARE(recoveries().size(), 0);
        nowMs += 999;
        media.pumpMicUplinkForTest();
        QCOMPARE(stalls().size(), 0);
        ++nowMs;
        media.pumpMicUplinkForTest();
        QCOMPARE(stalls().size(), 1);
        media.pumpMicUplinkForTest();
        QCOMPARE(stalls().size(), 1);

        // A reported stall from the old run must not become a recovery
        // attributed to the intentional pause either.
        pause();
        nowMs += 5000;
        resume();
        QCOMPARE(stalls().size(), 1);
        QCOMPARE(recoveries().size(), 0);
        nowMs += 1000;
        media.pumpMicUplinkForTest();
        QCOMPARE(stalls().size(), 2);
        nowMs += 100;
        source->block.store(true);
        QTRY_VERIFY_WITH_TIMEOUT((media.pumpMicUplinkForTest(), recoveries().size() == 1), 5000);
        QVERIFY(recoveries().first().contains(QStringLiteral("1100 ms")));
        media.pumpMicUplinkForTest();
        QCOMPARE(recoveries().size(), 1);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        other.release();
    }

    // 2026-10-10: production, with no inline seam: the collector's thread
    // runs exactly while the capture lease is held, the pump sends what it
    // collected, and the thread is joined when the line goes.
    void theMicrophoneIsCollectedOnItsOwnThreadWhileTheLeaseIsHeld()
    {
        const RestoreAudioChoice restore;
        RemoteMediaController::setMicCollectorInlineForTest(false);
        const auto inlineAgain = qScopeGuard([] {
            RemoteMediaController::setMicCollectorInlineForTest(true);
        });
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController::setMicCollectorInlineForTest(false);   // the harness set it
        // Pulled only by the collector's thread from here on.
        attachRemoteMicrophone(h, 0.3f, 1000.0);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        media.setAudioProfileChoice(RemoteAudioProfile::Opus);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QCOMPARE(media.micCollectorThreadRunningForTest(), media.micCaptureLeaseHeldForTest());

        media.setHoldsTransmit(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QVERIFY(media.micCollectorThreadRunningForTest());
        const quint64 before = media.micPacketsSent();
        QTRY_VERIFY_WITH_TIMEOUT(media.micPacketsSent() >= before + 10, 10000);

        media.setHoldsTransmit(false);
        QVERIFY(!media.micUplinkRunning());
        QCOMPARE(media.micCollectorThreadRunningForTest(), media.micCaptureLeaseHeldForTest());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        QTRY_VERIFY_WITH_TIMEOUT(!media.micCaptureLeaseHeldForTest(), 5000);
        QVERIFY(!media.micCollectorThreadRunningForTest());
    }

    // 2026-10-09 (review item 6): VOX armed holds the capture lease; a
    // program key with its own audio comes and goes and the lease stays
    // held throughout, with no capture generation restarted.
    void aProgramAudioKeyKeepsTheLeaseVoxHolds()
    {
        const RestoreAudioChoice restore;
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        AudioEngine* engine = h.remote.audioEngine();
        CaptureSupervisor::Options options;
        options.program = QDir(h.directory.path()).filePath(QStringLiteral("no-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(media.micLineOpen(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(daemon.micReceiver() != nullptr, 5000);
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        // The preview's own generation has failed by now.
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > 0
                                     && engine->captureStatus().state
                                            == CaptureSupervisor::Status::State::Failed,
                                 5000);
        // No local preview: only VOX holds the lease.
        h.remote.transmitModel().setVaxSourceAvailable(true);
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(h.remote.transmitModel().micSource(), MicSource::Vax);
        QTRY_VERIFY(!media.micCaptureLeaseHeldForTest());
        QVERIFY(takeTransmit(h));
        const quint32 previewed = engine->captureStatus().generation;

        // 2026-10-15: VOX with VAX the source streams VAX and takes no
        // capture; moved onto the microphone, the line takes the lease
        // from no demand: one new generation, Failed.
        media.setVoxArmed(true);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        QTest::qWait(10 * RemoteMediaController::kMicPumpIntervalMs);
        QCOMPARE(engine->captureStatus().generation, previewed);
        h.remote.transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().generation > previewed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Failed, 5000);
        const quint32 generation = engine->captureStatus().generation;

        std::optional<RemoteTransmitClient::Answer> answer;
        h.client.remoteTransmit()->keyForProgram(true,
            [&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        QVERIFY(h.client.remoteTransmit()->programAudioKey());
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QTRY_VERIFY_WITH_TIMEOUT(answer.has_value(), 5000);
        QVERIFY2(answer->accepted, qPrintable(answer->reason));
        QTest::qWait(20 * RemoteMediaController::kMicPumpIntervalMs);
        QVERIFY(media.micCaptureLeaseHeldForTest());
        h.client.remoteTransmit()->unkeyForProgram(answer->epoch);
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QVERIFY(media.micUplinkRunning());
        QTRY_VERIFY_WITH_TIMEOUT(!h.station.moxController()->isMox(), 5000);
        QTest::qWait(30 * RemoteMediaController::kMicPumpIntervalMs);
        QVERIFY(media.micCaptureLeaseHeldForTest());
        QCOMPARE(engine->captureStatus().generation, generation);
        QCOMPARE(engine->captureStatus().state, CaptureSupervisor::Status::State::Failed);

        // Back on VAX, VOX still armed: the lease goes.
        h.remote.transmitModel().setMicSource(MicSource::Vax);
        QVERIFY(media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        media.setVoxArmed(false);
        QVERIFY(!media.micUplinkRunning());
        QVERIFY(!media.micCaptureLeaseHeldForTest());
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-45: every new string the headphones add is in the operator's words.
    void headphonesWordingIsPlain()
    {
        using Fault = RemoteAudioReceiver::Fault;
        for (Fault fault : {Fault::SpeakerOpenFailed, Fault::SpeakerTimingUnavailable,
                            Fault::SpeakerCallbackTooLarge, Fault::SpeakerStalled,
                            Fault::SpeakerWriteFailed, Fault::DecoderUnavailable,
                            Fault::SpeakerFormatUnsupported, Fault::NoPackets,
                            Fault::DecodeFailed, Fault::ClockBuffer}) {
            const QString text = RemoteMediaController::headphonesFaultText(fault);
            QVERIFY(!text.isEmpty());
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        for (const char* text : {RemoteMediaController::kHeadphonesMixUnavailableReason,
                                 RemoteMediaController::kHeadphonesCoreCouldNotStart}) {
            QVERIFY2(OperatorWording::isPlain(QString::fromLatin1(text)), text);
        }
        const QString wire = remoteAudioOffReasonToWire(RemoteAudioOffReason::NoHeadphonesReceiver);
        QCOMPARE(wire, QStringLiteral("no-headphones-receiver"));
        QVERIFY(OperatorReasonText::knownReasons().contains(wire));
        const QString shown = OperatorReasonText::forDisplay(wire);
        QVERIFY(shown != wire);
        QVERIFY2(OperatorWording::isPlain(shown), qPrintable(shown));
    }
};

namespace {

// The audio functions: the speakers, headphones, lossless trial and the
// microphone uplink (each starts real audio and a real DTLS/SRTP link, and
// together they take about half the run). They run as two ctest entries,
// every other function as three (main()); unset, NEREUS_REMOTE_MEDIA_GROUP
// runs everything. A name here that is not a test function stops the run,
// so a rename cannot quietly drop a test from every entry.
const QStringList kAudioFunctions{
    QStringLiteral("anIntentionalCollectorPauseIsNotReportedAsAStall"),
    QStringLiteral("playsOnTheSpeakerFormatThisComputerChose"),
    QStringLiteral("speakerFaultRetriesOnItsOwnUntilItPlays"),
    QStringLiteral("speakerProblemClearsOncePlayingOnANewConnection"),
    QStringLiteral("speakersWaitForTheDeviceListWithoutAProblem"),
    QStringLiteral("aDeviceListThatNeverComesShowsTheProblemAfterTheWait"),
    QStringLiteral("aSpeakerFormatRemotePlaybackCannotPlayIsNotAskedForAgain"),
    QStringLiteral("lastingSpeakerFaultsAreNotAskedForAgain"),
    QStringLiteral("aSpeakerFailureRetryEndsWithTheSession"),
    QStringLiteral("speakerLossPersistsAcrossMuteDeviceChangeAndDisabledContext"),
    QStringLiteral("retryClearsTheProblemOnlyOnceTheNewContextPlays"),
    QStringLiteral("lateFaultFromAnEndedSessionChangesNothing"),
    QStringLiteral("minorSevenCorePlaysWithoutCodecDetail"),
    QStringLiteral("coreReasonsShowAsCoreCouldNotStartAndRadioOffline"),
    QStringLiteral("savedAudioChoicesMigrateToHighAndLossless"),
    QStringLiteral("legacyCoreKeepsHighUsableAndSaveDataUnavailable"),
    QStringLiteral("qualityChangesListeningAndKeepsReceiverAppsAt48"),
    QStringLiteral("missingCatalogueKeepsHighUsableWithoutClaiming48"),
    QStringLiteral("preAuthCatalogueCannotOfferAudioRates"),
    QStringLiteral("savedSaveDataIntentSurvivesAnUnsupportedCore"),
    QStringLiteral("microphoneQualitySwitchDiscardsPartialPackets"),
    QStringLiteral("radioSourceAckAloneStopsMacMicrophoneAndRestoresCleanClientPackets"),
    QStringLiteral("losslessFallsBackToOpusWhenTheNetworkCannotCarryIt"),
    QStringLiteral("retiredReceiverStreamStopsWithItsReasonAndDoesNotLoop"),
    QStringLiteral("losslessTrialCountsEveryLosslessStreamAndFallsBackOnce"),
    QStringLiteral("losslessRefusedByTheCoreKeepsOpusAndSaysWhy"),
    QStringLiteral("losslessTrialCountsTheHeadphonesMixAndFallsBackOnce"),
    QStringLiteral("headphonesFailureIsReportedAndLeavesTheSpeakersPlaying"),
    QStringLiteral("olderCoreCannotSendTheHeadphonesMixAndSaysSo"),
    QStringLiteral("micLineOnlyWithACoreThatTakesTheMicrophone"),
    QStringLiteral("micUplinkRunsOnlyWhileTransmittingOrVoxArmed"),
    QStringLiteral("microphoneAndProgramReachTheCoresRing"),
    QStringLiteral("vaxSourceSendsTheVaxTransmitAudio"),
    QStringLiteral("aProgramAudioKeyLeavesTheVaxDeviceToTheProgram"),
    QStringLiteral("aProgramAudioKeyUnderTheScreenKeyTakesTheVaxDeviceAtOnce"),
    QStringLiteral("aRigctldKeySendsVaxTxNeverTheMicrophone"),
    QStringLiteral("aProgramAudioKeyWithNoMicrophoneIsAdmittedOnSilence"),
    QStringLiteral("aProgramAudioKeyNeverSendsTheMicrophone"),
    QStringLiteral("aProgramAudioLapseSendsSilenceNotTheMicrophone"),
    QStringLiteral("aScreenKeyAfterAProgramAudioKeySendsTheMicrophone"),
    QStringLiteral("aFailedCaptureRetriesOnceForAKeyThatSendsTheMicrophone"),
    QStringLiteral("programSilenceIsPacedByTheClockAndBoundedAfterAStall"),
    QStringLiteral("aProgramAudioUnkeySendsNoMicrophone"),
    QStringLiteral("aLinkLossDuringProgramSilenceLeavesTheNextKeyItsLead"),
    QStringLiteral("aFailedCaptureFromNoDemandStartsOneGeneration"),
    QStringLiteral("aProgramAudioKeyKeepsTheLeaseVoxHolds"),
    QStringLiteral("aFailedCaptureHeldByAnotherDemandRetriesOnceForAKey"),
    QStringLiteral("aMediaLossDuringProgramSilenceLeavesTheNextKeyItsLead"),
    QStringLiteral("headphonesWordingIsPlain"),
    // 2026-10-16: the three restart backoff cases start real audio over the
    // real session and wait out the 1, 2 and 4 s steps, about 30 s together
    // that the rest entry carried.
    QStringLiteral("repeatedAudioRestartsBackOff"),
    QStringLiteral("aPlaybackFailureEndsAWaitingRestart"),
    QStringLiteral("directSilenceFallsBackOnceWhileAudioRestartsBackOff"),
    QStringLiteral("transmitSilenceWaitsForTheUnkeyAndResumesAtOnce"),
    QStringLiteral("transmitSilenceFromAnOlderCoreBacksOffAsBefore"),
    QStringLiteral("otherRestartsWhileTransmittingBackOffAsBefore"),
    QStringLiteral("unkeyAsksNothingWhenAudioAlreadyPlaysAgain"),
    QStringLiteral("aShortAudioGapKeepsTheContextAndPlaysOn"),
    QStringLiteral("anAudioGapPastTheBoundAsksForAFreshContext"),
    QStringLiteral("aTransportFailureDuringAGapRestartsAtOnce"),
};

} // namespace

int main(int argc, char** argv)
{
    if (argc > 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--fake-capture-child")) {
        return Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TestRemoteMediaController test;
    QTEST_SET_MAIN_SOURCE_PATH
    // CI speed round, 2026-10-17: as two entries, the audio functions and
    // the rest, the rest took 250 s and then passed the 300 s limit in a
    // hosted Linux shard running four tests at once, and the audio
    // functions took 171 to 204 s. So five entries, each function in one by
    // its place in the list, so a new function lands in one with no list to
    // edit: the audio functions alternate between
    // tst_remote_media_controller_audio and _audio2, and every other
    // function, in declaration order, goes in turn to
    // tst_remote_media_controller (the rest), _part2 and _part3
    // (tests/CMakeLists.txt).
    constexpr int kAudioEntries = 2;
    constexpr int kOtherEntries = 3;
    QList<NereusSDR::TestFunctionGroups::Group> groups{
        {QStringLiteral("audio"), {}}, {QStringLiteral("audio2"), {}},
        {QStringLiteral("part2"), {}}, {QStringLiteral("part3"), {}}};
    for (int i = 0; i < kAudioFunctions.size(); ++i) {
        groups[i % kAudioEntries].entries.append(kAudioFunctions.at(i));
    }
    int other = 0;
    for (const QString& name : NereusSDR::TestFunctionGroups::testFunctions(test.metaObject())) {
        if (kAudioFunctions.contains(name)) {
            continue;
        }
        // Every third stays in the rest, so the rest is never empty.
        const int slot = other++ % kOtherEntries;
        if (slot > 0) {
            groups[kAudioEntries + slot - 1].entries.append(name);
        }
    }
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_REMOTE_MEDIA_GROUP", groups);
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}

#include "tst_remote_media_controller.moc"
