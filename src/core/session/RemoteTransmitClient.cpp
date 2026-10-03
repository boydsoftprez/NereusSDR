// =================================================================
// src/core/session/RemoteTransmitClient.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the desktop remote window's transmit
//                (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 37 (R-IOS-13): the keepalive for the
//                Core's transmit watchdog. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: M7 coreStopped. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               setTunerTune. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (item 1): screenReleasePending and
//               tuneReleasePending, the operator's MOX or TUNE let go while
//               the Core's confirmation is on its way, so the next press
//               keys. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-30: TX rulings review (I-1): the memory lasts at most
//               kReleaseConfirmGraceMs from the release, is forgotten when a
//               release is refused or cannot be sent, and never counts while
//               anything else of this window keeps the radio on the air
//               (VOX, two-tone, TUNE, a program key, MOX for TUNE). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-01: Tune-ended lane: tunerTuneEnded, the Core's notice that
//               this window's Tuner Genius autotune ended before its
//               carrier keyed. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/RemoteTransmitClient.h"

#include <QLoggingCategory>
#include <QPointer>

#include <utility>

Q_LOGGING_CATEGORY(lcRemoteTransmit, "nereus.remotetransmit")

namespace NereusSDR {

namespace {

MirrorUpdate utf8Argument(const char* name, const QString& value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value)};
}

MirrorUpdate int64Argument(const char* name, qint64 value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Int64, QVariant(value)};
}

MirrorUpdate boolArgument(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
}

QString utf8Value(const QList<MirrorUpdate>& values, const char* name)
{
    for (const MirrorUpdate& value : values) {
        if (value.name == name && value.value.typeId() == QMetaType::QString) {
            return value.value.toString();
        }
    }
    return {};
}

} // namespace

RemoteTransmitClient::RemoteTransmitClient(Sender sender, QObject* parent)
    : QObject(parent)
    , m_sender(std::move(sender))
{
    // Task 37: the keepalive's timer.
    m_keepaliveTimer.setInterval(kKeepaliveIntervalMs);
    m_keepaliveTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_keepaliveTimer, &QTimer::timeout, this, &RemoteTransmitClient::keepaliveTick);
    // TX rulings (item 1, review I-1): the memory of a release is short.
    // Past the grace from the release (or from the Core's acceptance of
    // it), whatever the Core still reads is not the key let go, and a
    // press is the button's plain toggle again.
    m_releaseGraceTimer.setSingleShot(true);
    m_releaseGraceTimer.setInterval(kReleaseConfirmGraceMs);
    connect(&m_releaseGraceTimer, &QTimer::timeout, this,
            &RemoteTransmitClient::forgetReleases);
}

void RemoteTransmitClient::setVoxArmed(bool armed)
{
    if (m_voxArmed == armed) {
        return;
    }
    m_voxArmed = armed;
    refreshKeepalive();
}

quint32 RemoteTransmitClient::keepaliveEpoch() const
{
    if (m_screen.phase == Phase::On && m_screen.epoch != 0) {
        return m_screen.epoch;
    }
    if (m_program.phase == Phase::On && m_program.epoch != 0) {
        return m_program.epoch;
    }
    // Before a key's answer, or for a key that has no epoch of this
    // window's (VOX, TUNE, two-tone): never older than the Core's.
    return kReleaseAnyEpoch;
}

void RemoteTransmitClient::refreshKeepalive()
{
    const bool wanted = m_available
        && (micKeyDown() || holdsTransmit() || m_tuneAsked || m_twoToneAsked || m_voxArmed);
    if (wanted == m_keepaliveTimer.isActive()) {
        return;
    }
    if (wanted) {
        qCInfo(lcRemoteTransmit) << "Keepalives start";
        // The first goes at once: the Core's watch may already be running.
        m_keepaliveTimer.start();
        keepaliveTick();
    } else {
        m_keepaliveTimer.stop();
        qCInfo(lcRemoteTransmit) << "Keepalives stop after" << m_keepaliveSequence;
    }
}

void RemoteTransmitClient::keepaliveTick()
{
    if (!m_available) {
        return;
    }
    const quint64 sequence = ++m_keepaliveSequence;
    const quint32 epoch = keepaliveEpoch();
    // A command ID does not prove the off was queued: the transport can
    // silently drop it. Until each off's own accepted Core result, neither
    // heartbeat path may keep an old key alive. Continue one sequence.
    if (m_releaseDispatches != 0 || m_pendingReleases != 0 || m_releaseFailureSticky) {
        return;
    }
    const QPointer<RemoteTransmitClient> self(this);
    const quint64 sessionGeneration = m_sessionGeneration;
    if (m_auxiliaryKeepalive) {
        const KeepaliveSender auxiliary = m_auxiliaryKeepalive;
        const bool sent = auxiliary(sequence, epoch);
        if (!self || m_sessionGeneration != sessionGeneration) { return; }
        if (sent) { ++m_auxiliaryKeepalives; }
    }
    // The auxiliary callback may have sent an off or reset the session.
    // It never supplants the existing media/primary delivery path.
    if (m_releaseDispatches != 0 || m_pendingReleases != 0 || m_releaseFailureSticky) {
        return;
    }
    if (m_channelKeepalive) {
        const KeepaliveSender channel = m_channelKeepalive;
        const bool sent = channel(sequence, epoch);
        if (!self || m_sessionGeneration != sessionGeneration) { return; }
        if (sent) {
            ++m_channelKeepalives;
            return;
        }
    }
    // A channel callback can issue an off before declining the send.
    // Recheck its new fence before attempting another path in this tick.
    if (m_releaseDispatches != 0 || m_pendingReleases != 0 || m_releaseFailureSticky) {
        return;
    }
    if (m_sessionKeepalive) {
        const KeepaliveSender session = m_sessionKeepalive;
        const bool sent = session(sequence, epoch);
        if (!self || m_sessionGeneration != sessionGeneration) { return; }
        if (sent) { ++m_sessionKeepalives; }
    }
}

void RemoteTransmitClient::setAvailable(bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    const QPointer<RemoteTransmitClient> self(this);
    reset();
    if (!self || m_available != available) {
        return;
    }
    if (!available) {
        // The Core unkeys a device whose link drops and never keys it
        // again by itself; nothing here survives.
        // Task 37: a new link starts its keepalives from 1.
        m_keepaliveSequence = 0;
        m_channelKeepalives = 0;
        m_sessionKeepalives = 0;
        m_auxiliaryKeepalives = 0;
    }
    refreshKeepalive();
}

quint32 RemoteTransmitClient::send(const QByteArray& verb, const QList<MirrorUpdate>& arguments,
                                   Kind kind, bool releaseIntent)
{
    if (m_releaseFailureSticky && !releaseIntent) {
        return 0;
    }
    const quint64 sessionGeneration = m_sessionGeneration;
    if (releaseIntent) {
        // The sender can reenter the event loop. Once it returns, the
        // pending-off gate still waits for Core's own accepted result.
        ++m_releaseDispatches;
    }
    // Keep the callable alive if it synchronously destroys its owner.
    const Sender sender = m_sender;
    const QPointer<RemoteTransmitClient> self(this);
    const quint32 id = m_available && sender ? sender(verb, arguments) : 0;
    if (!self) {
        return 0;
    }
    if (m_sessionGeneration != sessionGeneration) {
        // The sender closed or replaced the session while reentering. The
        // reset retired this attempt; never insert its old command id.
        return 0;
    }
    if (releaseIntent) {
        --m_releaseDispatches;
    }
    if (id != 0) {
        m_pending.insert(id, Pending{kind, verb, releaseIntent});
        if (releaseIntent) {
            ++m_pendingReleases;
        }
    } else if (releaseIntent) {
        m_releaseFailureSticky = true;
        // TX rulings review (I-1): a release that never went leaves the
        // radio as it was; the next press is the unkey again.
        forgetReleases();
        if (!m_releaseFailureNotified) {
            m_releaseFailureNotified = true;
            emit refused(QString::fromLatin1(kReleaseFailedReason), QString(), QString());
        }
    }
    return id;
}

void RemoteTransmitClient::setScreenKey(bool down)
{
    if (down) {
        if (m_screen.phase != Phase::Idle) {
            return;  // already pressed
        }
        m_screenReleasePending = false;
        const QPointer<RemoteTransmitClient> self(this);
        const quint32 id = send(QByteArrayLiteral("tx.key"),
                                {utf8Argument("trigger", QString::fromLatin1(kScreenTrigger))},
                                Kind::ScreenKey);
        if (!self) { return; }
        if (id == 0) {
            emit refused(QString::fromLatin1(m_releaseFailureSticky ? kReleaseFailedReason
                                                                    : kNoLinkReason),
                         QString(), QString());
            return;
        }
        m_screen = Key{};
        m_screen.phase = Phase::Waiting;
        m_screen.commandId = id;
        qCInfo(lcRemoteTransmit) << "Key sent, command" << id;
        publish();
        return;
    }
    if (m_screen.phase == Phase::Idle) {
        // MOX off while a program keys through this window: the MOX
        // button's off unkeys it, as it does locally.
        if (m_program.phase == Phase::On) {
            const quint32 epoch = m_program.epoch;
            m_program = Key{};
            const QPointer<RemoteTransmitClient> self(this);
            release(epoch != 0 ? epoch : kReleaseAnyEpoch);
            if (!self) { return; }
            publish();
        } else if (m_coreTransmitting) {
            // MOX off while the Core transmits for no key of this window's
            // own (its VOX key, say): the MOX button's off unkeys whatever
            // keys, locally. The Core ends this device's own key, and
            // refuses another device's in its words (the holder's rule).
            release(kReleaseAnyEpoch);
        }
        return;
    }
    // Released: the key's own epoch, or before its answer any epoch of
    // this device's (the key may already be on at the Core).
    const quint32 epoch = m_screen.phase == Phase::On ? m_screen.epoch : kReleaseAnyEpoch;
    m_screen = Key{};
    // TX rulings (item 1): until the Core says it stopped, a `transmitting`
    // that still reads true is this key's, and a press is a new key.
    m_screenReleasePending = true;
    m_releaseGraceTimer.start();
    // The program's key is the same key at the Core; the operator's
    // release ends it too, as the MOX button's off does locally.
    if (m_program.phase == Phase::On) {
        m_program = Key{};
    }
    const QPointer<RemoteTransmitClient> self(this);
    release(epoch);
    if (!self) { return; }
    publish();
}

void RemoteTransmitClient::forgetReleases()
{
    m_screenReleasePending = false;
    m_tuneReleasePending = false;
    m_releaseGraceTimer.stop();
}

bool RemoteTransmitClient::othersKeepTransmitting(bool forTune) const
{
    // TX rulings review (I-1): what else of this window's own may be
    // keeping the radio on the air after a release. While any is, the
    // Core's `transmitting` is not the key let go, and a press stops it.
    if (m_voxArmed || m_twoToneAsked || m_program.phase != Phase::Idle) {
        return true;
    }
    return forTune ? m_screen.phase != Phase::Idle : m_tuneAsked;
}

bool RemoteTransmitClient::screenReleasePending() const
{
    return m_screenReleasePending && !m_releaseFailureSticky
        && !othersKeepTransmitting(/*forTune=*/false);
}

bool RemoteTransmitClient::tuneReleasePending() const
{
    return m_tuneReleasePending && !m_releaseFailureSticky
        && !othersKeepTransmitting(/*forTune=*/true);
}

void RemoteTransmitClient::release(quint32 epoch)
{
    send(QByteArrayLiteral("tx.unkey"), {int64Argument("epoch", static_cast<qint64>(epoch))},
         Kind::Release, /*releaseIntent=*/true);
    qCInfo(lcRemoteTransmit) << "Release sent, epoch" << epoch;
}

void RemoteTransmitClient::setTune(bool on)
{
    // TX rulings (item 1): TUNE let go, as MOX above.
    m_tuneReleasePending = !on && (m_tuneAsked || m_tuneReleasePending);
    if (m_tuneReleasePending) {
        m_releaseGraceTimer.start();
    }
    const QPointer<RemoteTransmitClient> self(this);
    const quint32 id = send(QByteArrayLiteral("tx.tune"), {boolArgument("on", on)}, Kind::Tune,
                            /*releaseIntent=*/!on);
    if (!self) { return; }
    m_tuneAsked = on && id != 0;
    if (id == 0 && on) {
        emit refused(QString::fromLatin1(m_releaseFailureSticky ? kReleaseFailedReason
                                                                : kNoLinkReason),
                     QString(), QString());
    }
    refreshKeepalive();
}

void RemoteTransmitClient::setTunerTune(bool on)
{
    // Answered as TUNE is (Kind::Tune): its carrier keeps this window's
    // keepalives going while it waits for the amplifier and while it is on.
    const QPointer<RemoteTransmitClient> self(this);
    const quint32 id =
        send(QByteArrayLiteral("tx.tunerTune"), {boolArgument("on", on)}, Kind::Tune,
             /*releaseIntent=*/!on);
    if (!self) { return; }
    m_tuneAsked = on && id != 0;
    if (id == 0 && on) {
        emit refused(QString::fromLatin1(m_releaseFailureSticky ? kReleaseFailedReason
                                                                : kNoLinkReason),
                     QString(), QString());
    }
    refreshKeepalive();
}

void RemoteTransmitClient::tunerTuneEnded()
{
    // Tune-ended lane (2026-10-01): with no carrier keyed, no `transmitting`
    // or stop comes to end the tune this window asked for; the Core's
    // notice does.
    if (!m_tuneAsked) {
        return;
    }
    m_tuneAsked = false;
    qCInfo(lcRemoteTransmit) << "The Core ended this window's tuner tune before it keyed";
    refreshKeepalive();
}

void RemoteTransmitClient::setTwoTone(bool on)
{
    const QPointer<RemoteTransmitClient> self(this);
    const quint32 id =
        send(QByteArrayLiteral("tx.twoTone"), {boolArgument("on", on)}, Kind::TwoTone,
             /*releaseIntent=*/!on);
    if (!self) { return; }
    m_twoToneAsked = on && id != 0;
    if (id == 0 && on) {
        emit refused(QString::fromLatin1(m_releaseFailureSticky ? kReleaseFailedReason
                                                                : kNoLinkReason),
                     QString(), QString());
    }
    refreshKeepalive();
}

void RemoteTransmitClient::keyForProgram(std::function<void(const Answer&)> answer)
{
    if (m_program.phase == Phase::Waiting) {
        // One program key at a time (the TCI server asks one at a time).
        Answer refusedAnswer;
        refusedAnswer.reason = QStringLiteral("Another program is already asking to transmit.");
        if (answer) { answer(refusedAnswer); }
        return;
    }
    const QPointer<RemoteTransmitClient> self(this);
    const quint32 id = send(QByteArrayLiteral("tx.key"),
                            {utf8Argument("trigger", QString::fromLatin1(kProgramTrigger))},
                            Kind::ProgramKey);
    if (!self) { return; }
    if (id == 0) {
        Answer refusedAnswer;
        refusedAnswer.reason = QString::fromLatin1(m_releaseFailureSticky
                                                       ? kReleaseFailedReason : kNoLinkReason);
        if (answer) { answer(refusedAnswer); }
        return;
    }
    m_program = Key{};
    m_program.phase = Phase::Waiting;
    m_program.commandId = id;
    m_programAnswer = std::move(answer);
    publish();
}

void RemoteTransmitClient::unkeyForProgram(quint32 epoch)
{
    const bool wasOn = m_program.phase != Phase::Idle;
    m_program = Key{};
    // While the operator's MOX holds the same key on, the program letting
    // go leaves it on: a manual key is the operator's until released
    // (MoxController's rule for a TCI release under a manual key).
    if (m_screen.phase == Phase::Idle && (wasOn || epoch != 0)) {
        const QPointer<RemoteTransmitClient> self(this);
        release(epoch != 0 ? epoch : kReleaseAnyEpoch);
        if (!self) { return; }
    }
    publish();
}

quint32 RemoteTransmitClient::epochOf(const QList<MirrorUpdate>& values)
{
    for (const MirrorUpdate& value : values) {
        if (value.name == "epoch" && value.value.typeId() == QMetaType::LongLong) {
            const qlonglong raw = value.value.toLongLong();
            if (raw > 0 && raw <= 0xFFFFFFFFLL) {
                return static_cast<quint32>(raw);
            }
        }
    }
    return 0;
}

void RemoteTransmitClient::commandFinished(quint32 commandId, const QByteArray& verb,
                                           bool accepted, const QString& reason,
                                           const QList<MirrorUpdate>& values)
{
    // Only the first answer per command counts; the copies' answers are
    // the same answer again.
    const auto pending = m_pending.constFind(commandId);
    if (pending == m_pending.cend() || pending->verb != verb) {
        return;
    }
    const Pending command = pending.value();
    const Kind kind = command.kind;
    m_pending.erase(pending);
    if (command.releaseIntent) {
        --m_pendingReleases;
        // TX rulings (item 1): the Core took the release; a `transmitting`
        // still on its way from the key arrives within the grace.
        if (accepted && (m_screenReleasePending || m_tuneReleasePending)) {
            m_releaseGraceTimer.start();
        }
        if (!accepted) {
            m_releaseFailureSticky = true;
            // TX rulings review (I-1): a refused release stopped nothing;
            // the next press is the unkey again.
            forgetReleases();
            m_releaseFailureNotified = true;  // The Core's refusal is emitted below.
        }
        // Acceptance proves only this off reached the primary in order.
        // Another unresolved off may concern a different transmit mode.
    }
    const QString code = utf8Value(values, "refusalCode");
    const QString fix = utf8Value(values, "refusalFix");
    const QString shown = reason.isEmpty()
        ? QStringLiteral("The Core refused the request without giving a reason.")
        : reason;
    qCInfo(lcRemoteTransmit).noquote() << "Core answered" << QString::fromUtf8(verb) << commandId
                                       << (accepted ? "accepted" : "refused:") << reason;

    switch (kind) {
    case Kind::ScreenKey:
        if (m_screen.phase != Phase::Waiting || m_screen.commandId != commandId) {
            // Released before the answer: the release already went.
            // TX rulings (item 1): a key the Core refused never transmitted,
            // so nothing of it is left to stop.
            if (!accepted && m_screen.phase == Phase::Idle) {
                m_screenReleasePending = false;
            }
            return;
        }
        if (accepted) {
            m_screen.phase = Phase::On;
            m_screen.epoch = epochOf(values);
            // The Core may already say it transmits (its answer and its
            // state travel separately).
            m_screen.sawTransmitting = m_coreTransmitting;
        } else {
            m_screen = Key{};
            emit refused(shown, code, fix);
        }
        publish();
        return;
    case Kind::ProgramKey: {
        if (m_program.phase != Phase::Waiting || m_program.commandId != commandId) {
            return;
        }
        Answer answer;
        answer.accepted = accepted;
        answer.reason = accepted ? QString() : shown;
        answer.code = code;
        answer.fix = fix;
        if (accepted) {
            m_program.phase = Phase::On;
            m_program.epoch = epochOf(values);
            m_program.sawTransmitting = m_coreTransmitting;
            answer.epoch = m_program.epoch;
        } else {
            m_program = Key{};
        }
        auto reply = std::exchange(m_programAnswer, {});
        const QPointer<RemoteTransmitClient> self(this);
        publish();
        if (reply && self) {
            reply(answer);
        }
        return;
    }
    case Kind::Tune:
        if (!command.releaseIntent && !accepted && m_tuneAsked) {
            m_tuneAsked = false;
            refreshKeepalive();
        }
        if (!command.releaseIntent && !accepted) {
            m_tuneReleasePending = false;
        }
        [[fallthrough]];
    case Kind::Release:
    case Kind::TwoTone:
        if (kind == Kind::TwoTone && !command.releaseIntent && !accepted && m_twoToneAsked) {
            m_twoToneAsked = false;
            refreshKeepalive();
        }
        if (!accepted) {
            emit refused(shown, code, fix);
        }
        return;
    }
}

void RemoteTransmitClient::setCoreTransmitting(bool on)
{
    m_coreTransmitting = on;
    if (!on) {
        // The Core stopped transmitting: a TUNE or two-tone this window
        // asked for is over too.
        m_tuneAsked = false;
        m_twoToneAsked = false;
        // TX rulings (item 1): and a key let go has stopped.
        forgetReleases();
    }
    bool ended = false;
    for (Key* key : {&m_screen, &m_program}) {
        if (key->phase != Phase::On) {
            continue;
        }
        if (on) {
            key->sawTransmitting = true;
        } else if (key->sawTransmitting) {
            // The Core ended the key on its own (a safety stop, a take):
            // the next press is a new command.
            *key = Key{};
            ended = true;
        }
    }
    if (ended) {
        qCInfo(lcRemoteTransmit) << "The Core ended this window's key";
        publish();
    }
    refreshKeepalive();
}

void RemoteTransmitClient::coreStopped(quint32 stopSerial, bool coreKeyed, quint32 stopEpoch)
{
    if (stopSerial == m_coreStopSerial) {
        return;
    }
    m_coreStopSerial = stopSerial;
    // TX rulings (item 1): a stop while the Core reads not transmitting
    // ends a release waiting to be confirmed (a key that never reached
    // `transmitting`). While it still reads true the tail runs on, and
    // the release waits for its false.
    if (!m_coreTransmitting) {
        forgetReleases();
    }
    bool ended = false;
    for (Key* key : {&m_screen, &m_program}) {
        if (key->phase != Phase::On) {
            continue;
        }
        // Fix wave 2 (the M7 race): the stop names the key it ended; a key
        // of this window's pressed after it (a newer epoch, answered
        // before the stop reached here) goes on. Without the epochs, a key
        // on at the Core by now is not this stop's.
        const bool stopped = stopEpoch != 0 && key->epoch != 0 ? key->epoch <= stopEpoch
                                                               : !coreKeyed;
        if (stopped) {
            *key = Key{};
            ended = true;
        }
    }
    if (ended) {
        m_tuneAsked = false;
        m_twoToneAsked = false;
        qCInfo(lcRemoteTransmit) << "The Core stopped this window's key";
        publish();
        refreshKeepalive();
    }
}

bool RemoteTransmitClient::micKeyDown() const
{
    return m_screen.phase != Phase::Idle || m_program.phase != Phase::Idle;
}

bool RemoteTransmitClient::holdsTransmit() const
{
    return m_screen.phase == Phase::On || m_program.phase == Phase::On;
}

void RemoteTransmitClient::reset()
{
    m_screen = Key{};
    m_program = Key{};
    m_tuneAsked = false;
    m_twoToneAsked = false;
    forgetReleases();
    m_pending.clear();
    m_pendingReleases = 0;
    m_releaseDispatches = 0;
    m_releaseFailureSticky = false;
    m_releaseFailureNotified = false;
    ++m_sessionGeneration;
    auto reply = std::exchange(m_programAnswer, {});
    const QPointer<RemoteTransmitClient> self(this);
    publish();
    if (reply && self) {
        Answer answer;
        answer.reason = QString::fromLatin1(kNoLinkReason);
        reply(answer);
    }
}

void RemoteTransmitClient::publish()
{
    const QPointer<RemoteTransmitClient> self(this);
    const bool down = micKeyDown();
    if (down != m_publishedKeyDown) {
        m_publishedKeyDown = down;
        emit micKeyDownChanged(down);
        if (!self) { return; }
    }
    const bool holds = holdsTransmit();
    if (holds != m_publishedHolds) {
        m_publishedHolds = holds;
        emit holdsTransmitChanged(holds);
        if (!self) { return; }
    }
    refreshKeepalive();
}

} // namespace NereusSDR
