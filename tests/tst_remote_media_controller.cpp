// no-port-check: NereusSDR-original. Authenticated GUI subscription lifecycle.
// Modification history (NereusSDR):
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
#include <QTest>
#include <QApplication>
#include <QMetaMethod>
#include <QComboBox>
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
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QScopeGuard>
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
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/ClarityController.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "core/session/media/DisplayCodec.h"
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
#include "core/StepAttenuatorController.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/PathRacer.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteConnectionController.h"
#include "gui/RemoteMediaController.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/CapacityLimitedTransport.h"
#include "fakes/LoopbackTransport.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "OperatorWording.h"
#include "gui/OperatorReasonText.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

class DisplayTransport final : public IMediaTransport {
public:
    explicit DisplayTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
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

    // R-R3-18, R-R3-19, R-R3-21: the zoom flash seen on checkpoint 80f45e28.
    // With C-Tune on and the pan's receiver sharing its stream, a zoom
    // re-centres the view on the VFO. That must stay a view change: the Core
    // refuses to move a shared window there, and each refusal used to snap
    // the view back and blank the trace, at wheel rate. A pan drag still asks
    // the Core; one refusal stops that drag's requests and keeps the picture.
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
        QTest::newRow("waterfall") << false << false;
        QTest::newRow("3D stack") << true << false;
        QTest::newRow("waterfall, display budget") << false << true;
        QTest::newRow("3D stack, display budget") << true << true;
    }
    void selectingTheOtherSliceOnOneReceiverKeepsThePansHistory()
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
        const double centre = station.streamCentreHz(stream);
        const int secondId = station.addSlice();
        auto* secondSlice = station.sliceById(secondId);
        QVERIFY(secondSlice);
        secondSlice->setFrequency(centre + 20000);
        QCOMPARE(secondSlice->streamIndex(), stream);
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
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        };
        const auto drawn = [&] {
            return threeD ? widget->dssRowsPushedForTest()
                          : widget->waterfallHistoryRowsForTest();
        };
        QTRY_VERIFY_WITH_TIMEOUT((feed(), drawn() >= 5), 10000);
        const int before = drawn();

        // The operator selects the other slice's flag on this pan.
        pan->setActiveSliceIndex(secondId);
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
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
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
        ClarityController clarity;
        remote.setClarityController(&clarity);
        const auto detachClarity = qScopeGuard([&] { remote.setClarityController(nullptr); });
        clarity.setEnabled(true);
        QSignalSpy liveFloors(&clarity, &ClarityController::noiseFloorChanged);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        auto* second = stack.addPanadapter(QStringLiteral("second"));
        stack.setActivePan(QStringLiteral("first"));
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
        QTRY_VERIFY_WITH_TIMEOUT(!liveFloors.isEmpty(), 5000);
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
        ClarityController clarity;
        remote.setClarityController(&clarity);
        const auto detachClarity = qScopeGuard([&] { remote.setClarityController(nullptr); });
        clarity.setEnabled(true);
        clarity.setPollIntervalMs(0);
        clarity.setSmoothingTauSec(0);
        clarity.setDeadbandDb(0);
        QSignalSpy floors(&clarity, &ClarityController::noiseFloorChanged);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        PanadapterStack stack;
        auto* applet = stack.addPanadapter(QStringLiteral("pan-0"));
        applet->setActiveSliceIndex(stationSlice->sliceIndex());
        auto* widget = applet->spectrumWidget();
        connect(&clarity, &ClarityController::waterfallThresholdsChanged,
                widget, [widget](float low, float high) {
            widget->setClarityActive(true);
            widget->setClarityWaterfallThresholds(low, high);
        });
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
        for (int i = 40; i < 60; ++i) {
            const bool keyframe = i == 52;
            const QByteArray packet =
                encoder.encode(frameAt(i, base + i * kPeriodNs, i < 52 ? -120.0f : -60.0f), keyframe);
            if (i == 45) {
                continue;
            }
            media->deliver(packet);
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
        const quint64 rowsAtKey = widget->remoteRowsPushedForTest();
        media->deliver(encoder.encode(frameAt(1, 17, coreNow(), -90.0f)));
        QCOMPARE(int(frames.count()), framesBefore);
        controller.setPanTransmitting(panId, true, false);
        QTest::qWait(400);
        drainRows();
        QCOMPARE(int(frames.count()), framesBefore);
        QCOMPARE(widget->remoteRowsPushedForTest(), rowsAtKey);
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
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -130.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), 20.0);
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
        QCOMPARE(low, -96.0);
        QCOMPARE(high, -76.0);
        // A 20 dB range in 256 levels: steps under 0.1 dB.
        QVERIFY((high - low) / 255.0 < 0.1);
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
        QVERIFY(std::abs(asked.value(QStringLiteral("maxDbm")).toDouble()
                         - std::ceil((-76.0 - shift) * 10.0) / 10.0) < 1.0e-9);

        client.disconnectFromStation(QStringLiteral("test complete"));
        remote.setFftEngine(nullptr);
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
        // Clarity drives the levels, below and above what the pan shows.
        widget->setClarityActive(true);
        widget->setClarityWaterfallThresholds(-150.0f, -40.0f);
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
        widget->setClarityWaterfallThresholds(-147.0f, -44.0f);
        settle();
        widget->setClarityWaterfallThresholds(-153.0f, -37.0f);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // The low level leaves the window: asked again, with headroom.
        widget->setClarityWaterfallThresholds(-175.0f, -37.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 2);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -185.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -27.0);

        // The levels close in far inside the window: it narrows to them,
        // never inside what the pan shows.
        widget->setClarityWaterfallThresholds(-120.0f, -90.0f);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 3);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -130.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -60.0);

        // Back to the stored levels: the window is the pan and those levels.
        // (Waterfall AGC's levels come from a Core that offers display
        // extras, as this one does: coreWaterfallAgcAsksNothingAsItSettles.)
        widget->setClarityActive(false);
        settle();
        QTRY_COMPARE(countControl(controls, QStringLiteral("subscribe")), 4);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -110.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -60.0);

        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // JJ's ruling of 2026-09-28: a remote pan's waterfall AGC and NF-AGC use
    // the levels the Core computes (display extras `waterfallLevels`), as the
    // phone does. The subscribe asks for them; the waterfall colours against
    // the stored levels until the first arrive and then against the Core's;
    // the local follower does not run, so AGC settling asks the Core nothing
    // and never blanks the pan with a new request.
    void coreWaterfallAgcAsksNothingAsItSettles()
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
        widget->setClarityActive(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setWfAgcEnabled(true);
        widget->setDbmRange(-100.0f, -60.0f);
        widget->setWfLowThreshold(-110.0f);
        widget->setWfHighThreshold(-70.0f);
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
        // is the pan's and the stored levels', as with manual levels.
        QJsonObject asked = lastControl(controls, QStringLiteral("subscribe"));
        QCOMPARE(asked.value(QStringLiteral("waterfallLevels")).toObject(),
                 (QJsonObject{{QStringLiteral("mode"), QStringLiteral("agc")},
                              {QStringLiteral("lowDbm"), -110.0},
                              {QStringLiteral("highDbm"), -70.0},
                              {QStringLiteral("offsetDb"), 0}}));
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -110.0);
        QCOMPARE(asked.value(QStringLiteral("maxDbm")).toDouble(), -60.0);
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
        QCOMPARE(widget->wfActiveLowThreshold(), -110.0f);
        QCOMPARE(widget->wfActiveHighThreshold(), -70.0f);
        settle();
        QVector<float> line(64, -125.0f);
        line[10] = -20.0f;
        for (int i = 0; i < 20; ++i) {
            widget->composeWaterfallActiveThresholds(line);
        }
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // The Core's levels arrive beside a frame: the waterfall takes them,
        // and as they move, nothing is asked either.
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
            {QStringLiteral("wideSamples"), 0}, {QStringLiteral("minDbm"), -110},
            {QStringLiteral("maxDbm"), -60}, {QStringLiteral("fps"), 20},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("wideband"), WidebandDisplayContext{}.toJson()},
            {QStringLiteral("grantedFftSize"), 4096}, {QStringLiteral("grantedTier"), QStringLiteral("wide")},
            {QStringLiteral("requestedPixels"), 128}, {QStringLiteral("grantedPixels"), 128},
            {QStringLiteral("limit"), QStringLiteral("none")}};
        QVERIFY(server.sendMediaControl(context, server.mediaSessionEpoch()));
        const DisplayCodecContext codec{id, 1, -110.0f, -60.0f, 128, 128, 0};
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
                deliverLevels(-137.0f, -8.0f, 1);
                return widget->wfActiveLowThreshold() == -137.0f;
            }(), 5000);
        QCOMPARE(widget->wfActiveHighThreshold(), -8.0f);
        widget->composeWaterfallActiveThresholds(line);
        QCOMPARE(widget->wfActiveLowThreshold(), -137.0f);
        QCOMPARE(widget->wfActiveHighThreshold(), -8.0f);
        deliverLevels(-135.5f, -11.0f, 2);
        QCOMPARE(widget->wfActiveLowThreshold(), -135.5f);
        settle();
        deliverLevels(-180.0f, 20.0f, 3);
        settle();
        QCOMPARE(countControl(controls, QStringLiteral("subscribe")), 1);

        // NF-AGC asks for its own levels once; the AGC's are not kept.
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(true);
        widget->setWaterfallAGCOffsetDb(6);
        settle();
        QTRY_VERIFY(countControl(controls, QStringLiteral("subscribe")) >= 2);
        asked = lastControl(controls, QStringLiteral("subscribe"));
        const QJsonObject levels = asked.value(QStringLiteral("waterfallLevels")).toObject();
        QCOMPARE(levels.value(QStringLiteral("mode")).toString(), QStringLiteral("noiseFloorAgc"));
        QCOMPARE(levels.value(QStringLiteral("offsetDb")).toInt(), 6);
        QCOMPARE(asked.value(QStringLiteral("minDbm")).toDouble(), -110.0);
        widget->composeWaterfallActiveThresholds(line);
        QCOMPARE(widget->wfActiveLowThreshold(), -110.0f);

        // Manual levels ask for none.
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
        client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Parity Task 28 (R-R3-49, A11): a Core with a TX analyzer is told at
    // start that this window takes its transmit display; the pan's
    // subscribe carries its transmit window; while the Core is keyed the
    // transmit context and its frames are handed on, and the pan keeps its
    // receive context; the fall brings receive frames back. A Core without
    // one sees today's start and subscribe.
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
        const int grant = int(kRemoteSpectrumGrantSessionProtocolMinor);
        QTest::newRow("minor 9 crop past the source edge") << grant << false << kStays;
        QTest::newRow("minor 9 shared engine") << grant << true << kStays;
        QTest::newRow("minor 8 crop past the source edge") << grant - 1 << false << kStays;
        QTest::newRow("minor 8 shared engine") << grant - 1 << true << kStays;
        // R-R3-01, R-R3-08, R-R3-37: the pan that sized the engine leaves.
        QTest::newRow("minor 9 shared engine, first pan leaves")
            << grant << true << kLeaves;
        QTest::newRow("minor 9 shared engine, first pan leaves, Core refuses more")
            << grant << true << kLeavesCoreRefuses;
    }

    void budgetModePaintsWhenCoreGrantsFewerPixels()
    {
        QFETCH(int, minor);
        QFETCH(bool, shared);
        QFETCH(int, leave);
        const bool grantAgreed = minor >= kRemoteSpectrumGrantSessionProtocolMinor;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        auto& appSettings = AppSettings::instance();
        const bool hadFft = appSettings.contains(QStringLiteral("DisplayFftSize"));
        const QVariant savedFft = appSettings.value(QStringLiteral("DisplayFftSize"));
        appSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("4096"));
        const auto restoreFft = qScopeGuard([&] {
            if (hadFft) { appSettings.setValue(QStringLiteral("DisplayFftSize"), savedFft); }
            else { appSettings.remove(QStringLiteral("DisplayFftSize")); }
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
        auto* first = stack.addPanadapter(QStringLiteral("first"));
        first->setActiveSliceIndex(sliceId);
        first->spectrumWidget()->setExtendedViewAllowed(false);
        // Shared: a "fine" pan that sizes the engine. Edge: a crop that
        // overlaps the source by 4 kHz, so fewer bins than pixels exist.
        if (shared) {
            first->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 12000);
        } else {
            first->spectrumWidget()->setDisplayWindowPreservingHistory(centre + 116000, 48000);
        }
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
            limited = stack.addPanadapter(QStringLiteral("second"));
            limited->setActiveSliceIndex(sliceId);
            limited->spectrumWidget()->setExtendedViewAllowed(false);
            limited->spectrumWidget()->setDisplayWindowPreservingHistory(centre, 6000);
            QTRY_COMPARE(daemon.activeEndpointCount(), 2);
        }
        QTRY_VERIFY2_WITH_TIMEOUT(paints(limited), qPrintable(limited->remoteDisplayStatus()),
                                  5000);
        QVERIFY2(gui.panDisplayState(limited->panId()).phase != PanDisplayState::Phase::Stalled,
                 qPrintable(limited->remoteDisplayStatus()));

        // The limited pan really was granted fewer pixels than it asked for.
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

    // R-R3-23 (a): a real speaker loss becomes a lasting playback problem
    // with Retry. Mute, unmute, a device change, a disabled context and
    // time do not clear it while the speaker is still gone.
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
        // clears nothing, and neither does time.
        QTRY_VERIFY_WITH_TIMEOUT(acceptedOff(remoteMedia, 0, RemoteAudioOffReason::ClientDisabled),
                                 5000);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QTest::qWait(3 * 250);
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);
        QVERIFY(statusTimer->isActive()); // The problem awaits recovery.

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
    // context cannot open this computer's speaker. The failure is lasting, as
    // any other: no automatic request follows it, and the operator's Retry
    // plays again once the speaker is back.
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
        // Nothing is left waiting.
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        // The 4 s step comes due while held; released, it finds the
        // restart ended and asks for nothing: the failure waits for Retry.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioRestartStepHeldForTest(), 10000);
        QCOMPARE(requests, atContext);
        remoteMedia.holdAudioRestartForTest(false);
        QVERIFY(!remoteMedia.audioRestartStepHeldForTest());
        QTest::qWait(200);
        QCOMPARE(requests, atContext);
        QVERIFY(!remoteMedia.audioRestartPendingForTest());
        QCOMPARE(remoteMedia.audioStatus().state, State::PlaybackProblem);

        // The speaker is back and the Core's audio too: Retry plays again.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        audio.source.start();
        remoteMedia.retryAudio();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(), 10000);

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
            QStringLiteral("Audio quality: Opus\nThe network could not carry lossless audio; "
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
        QCOMPARE(choice->count(), 2);
        QCOMPARE(choice->itemText(0), QStringLiteral("Opus"));
        QCOMPARE(choice->itemText(1), QStringLiteral("Lossless"));
        QCOMPARE(choice->currentText(), QStringLiteral("Opus"));
        QVERIFY(details->text().contains(QStringLiteral("Audio quality: Opus\nAudio format: Opus")));

        choice->setCurrentIndex(1);
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
            "Audio quality: Opus\nThis Core does not allow lossless audio.\n")));
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
            "Setup, Audio, Devices to try again.");
        const QString writeText = QStringLiteral(
            "Audio could not be sent to the headphones. Turn the headphones off and on in "
            "Setup, Audio, Devices to try again.");
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

    // R-R3-45: every new string the headphones add is in the operator's words.
    void headphonesWordingIsPlain()
    {
        using Fault = RemoteAudioReceiver::Fault;
        for (Fault fault : {Fault::SpeakerOpenFailed, Fault::SpeakerTimingUnavailable,
                            Fault::SpeakerCallbackTooLarge, Fault::SpeakerStalled,
                            Fault::SpeakerWriteFailed, Fault::DecoderUnavailable,
                            Fault::NoPackets,
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
// together they take about half the run). NEREUS_REMOTE_MEDIA_GROUP=audio
// runs exactly these; =rest runs every other test function; unset runs
// everything. A name here that is not a test function stops the run, so a
// rename cannot quietly drop a test from both entries.
const QStringList kAudioFunctions{
    QStringLiteral("playsOnTheSpeakerFormatThisComputerChose"),
    QStringLiteral("speakerLossPersistsAcrossMuteDeviceChangeAndDisabledContext"),
    QStringLiteral("retryClearsTheProblemOnlyOnceTheNewContextPlays"),
    QStringLiteral("lateFaultFromAnEndedSessionChangesNothing"),
    QStringLiteral("minorSevenCorePlaysWithoutCodecDetail"),
    QStringLiteral("coreReasonsShowAsCoreCouldNotStartAndRadioOffline"),
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
    QStringLiteral("headphonesWordingIsPlain"),
};

// The test functions QtTest would run: private slots with no arguments,
// other than the four fixtures and the _data functions.
QStringList testFunctions(const QMetaObject* meta)
{
    QStringList names;
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const QMetaMethod method = meta->method(i);
        const QString name = QString::fromLatin1(method.name());
        if (method.methodType() != QMetaMethod::Slot
            || method.access() != QMetaMethod::Private || method.parameterCount() != 0
            || name.endsWith(QStringLiteral("_data"))
            || name == QStringLiteral("initTestCase") || name == QStringLiteral("cleanupTestCase")
            || name == QStringLiteral("init") || name == QStringLiteral("cleanup")) {
            continue;
        }
        names.append(name);
    }
    return names;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TestRemoteMediaController test;
    QTEST_SET_MAIN_SOURCE_PATH
    QStringList arguments = app.arguments();
    const QString group = qEnvironmentVariable("NEREUS_REMOTE_MEDIA_GROUP");
    // A run that names its own functions (or options) runs as asked.
    if (!group.isEmpty() && arguments.size() == 1) {
        const QStringList all = testFunctions(test.metaObject());
        for (const QString& name : kAudioFunctions) {
            if (!all.contains(name)) {
                qCritical("kAudioFunctions names %s, which is not a test function",
                          qPrintable(name));
                return 1;
            }
        }
        if (group == QStringLiteral("audio")) {
            arguments += kAudioFunctions;
        } else if (group == QStringLiteral("rest")) {
            for (const QString& name : all) {
                if (!kAudioFunctions.contains(name)) {
                    arguments += name;
                }
            }
        } else {
            qCritical("NEREUS_REMOTE_MEDIA_GROUP must be audio or rest, not %s",
                      qPrintable(group));
            return 1;
        }
    }
    return QTest::qExec(&test, arguments);
}

#include "tst_remote_media_controller.moc"
