// =================================================================
// tests/tst_remote_transmit_client.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test.
//
// iPhone app plan, desktop remote transmit (R-IOS-13, R-R3-42): the remote
// window's side of the transmit verbs, against a recording sender. The
// copies rule, the epoch, a release before the answer, a key the Core ends
// on its own, a program's key under the operator's, TUNE off before its
// answer, and a lost link.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the desktop remote window's transmit
//                (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 37 (R-IOS-13): the keepalive. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: TX rulings review (I-1): the memory of a release never
//               turns a press into a key while VOX, two-tone or TUNE keeps
//               the radio on the air, is forgotten when the release is
//               refused or cannot be sent, and lasts only the grace (an
//               unanswered release included), and a program key after a
//               release makes the press an unkey. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QElapsedTimer>
#include <QPointer>
#include <QSignalSpy>

#include "core/session/RemoteTransmitClient.h"
#include "core/safety/RemoteTxWatchdog.h"

#include <algorithm>
#include <optional>

using namespace NereusSDR;

namespace {

struct Sent {
    quint32 id;
    QByteArray verb;
    QList<MirrorUpdate> arguments;
};

struct Recorder {
    QList<Sent> sent;
    quint32 next = 100;
    bool linked = true;

    RemoteTransmitClient::Sender sender()
    {
        return [this](const QByteArray& verb, const QList<MirrorUpdate>& arguments) -> quint32 {
            if (!linked) { return 0; }
            const quint32 id = next++;
            sent.append({id, verb, arguments});
            return id;
        };
    }

    QVariant argument(int index, const char* name) const
    {
        for (const MirrorUpdate& a : sent.at(index).arguments) {
            if (a.name == name) { return a.value; }
        }
        return {};
    }
};

QList<MirrorUpdate> epochValue(quint32 epoch)
{
    return {MirrorUpdate{0, QByteArrayLiteral("epoch"), MirrorWireKind::Int64,
                         QVariant(static_cast<qlonglong>(epoch))}};
}

QList<MirrorUpdate> refusal(const char* code, const char* fix)
{
    return {MirrorUpdate{0, QByteArrayLiteral("refusalCode"), MirrorWireKind::Utf8,
                         QVariant(QString::fromLatin1(code))},
            MirrorUpdate{0, QByteArrayLiteral("refusalFix"), MirrorWireKind::Utf8,
                         QVariant(QString::fromLatin1(fix))}};
}

// The Core answers every copy.
void answerCopies(RemoteTransmitClient& client, const Sent& command, bool accepted,
                  const QString& reason, const QList<MirrorUpdate>& values)
{
    for (int c = 0; c < RemoteTransmitClient::kCopies; ++c) {
        client.commandFinished(command.id, command.verb, accepted, reason, values);
    }
}

// A fake primary command transport and independent media TX path. The
// watchdog and transmit client are production classes; only delivery and
// time are controlled here, so a held unkey cannot be rescued by a sleep
// or by the order the host happens to schedule threads.
struct SplitTxPaths {
    const QByteArray device = QByteArrayLiteral("paired-window");
    qint64 nowMs = 0;
    std::optional<qint64> timerDue;
    int stops = 0;
    int channelAccepted = 0;
    int primaryAccepted = 0;
    bool primaryDelivering = false;
    bool keyed = false;
    Recorder primary;
    RemoteTxWatchdog watchdog;
    RemoteTransmitClient client;

    SplitTxPaths()
        : client(primary.sender())
    {
        RemoteTxWatchdog::Hooks hooks;
        hooks.clock = [this] { return nowMs; };
        hooks.startTimer = [this](int ms) { timerDue = nowMs + ms; };
        hooks.stopTimer = [this] { timerDue.reset(); };
        hooks.stop = [this](const QByteArray& stopped, const QString&) {
            if (stopped == device) {
                ++stops;
                keyed = false;
            }
        };
        watchdog.setHooks(std::move(hooks));
        client.setChannelKeepalive([this](quint64 sequence, quint32 epoch) {
            if (watchdog.keepalive(device, sequence, epoch,
                                   RemoteTxWatchdog::Path::TxChannel)) {
                ++channelAccepted;
            }
            // This is the media sender's local success, independent of
            // whether the Core counted a particular sequence.
            return true;
        });
        client.setSessionKeepalive([this](quint64 sequence, quint32 epoch) {
            if (primaryDelivering
                && watchdog.keepalive(device, sequence, epoch,
                                      RemoteTxWatchdog::Path::Session)) {
                ++primaryAccepted;
            }
            return true;
        });
        client.setAvailable(true);
    }

    void advanceTo(qint64 ms)
    {
        while (timerDue && *timerDue <= ms) {
            nowMs = *timerDue;
            timerDue.reset();
            watchdog.onTimer();
        }
        nowMs = ms;
    }
};

} // namespace

class TestRemoteTransmitClient : public QObject {
    Q_OBJECT

private slots:
    void micSourcePendingRefusedAndMalformedAckCannotKeyPreviousInput()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setMicSourceCapability(true);
        QVERIFY(client.requestMicSource(RemoteMicSource::RadioMic));
        const Sent request = core.sent.last();
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 1);
        QVERIFY(!client.micSourceSettled());
        client.commandFinished(request.id, "tx.key", true, {}, {});
        QVERIFY(!client.micSourceSettled());
        client.commandFinished(request.id, request.verb, false, QStringLiteral("Refused"), {});
        QCOMPARE(client.acceptedMicSource(), RemoteMicSource::ClientAudio);
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 1);
        QVERIFY(client.requestMicSource(RemoteMicSource::RadioMic));
        const Sent malformed = core.sent.last();
        client.commandFinished(malformed.id, malformed.verb, true, {},
            {{0, "source", MirrorWireKind::Utf8, QStringLiteral("ClientAudio")}});
        QVERIFY(!client.micSourceSettled());
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 2);
    }

    void pendingSourceDoesNotBlockGeneratedStartsOrNonMicrophoneKeys()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setMicSourceCapability(true);
        client.setMicrophoneRequirement([] { return false; }); // CW uses the existing non-microphone path
        QVERIFY(client.requestMicSource(RemoteMicSource::RadioMic));
        QVERIFY(client.micSourcePending());
        const auto generated = [&](bool twoTone, quint32 epoch) {
            const QByteArray verb = twoTone ? QByteArray("tx.twoTone") : QByteArray("tx.tune");
            if (twoTone) { client.setTwoTone(true); } else { client.setTune(true); }
            QCOMPARE(core.sent.last().verb, verb);
            QVERIFY(core.argument(core.sent.size()-1, "on").toBool());
            answerCopies(client, core.sent.last(), true, {}, epochValue(epoch));
            if (twoTone) { client.setTwoTone(false); } else { client.setTune(false); }
            QCOMPARE(core.sent.last().verb, verb);
            QVERIFY(!core.argument(core.sent.size()-1, "on").toBool());
            answerCopies(client, core.sent.last(), true, {}, {});
            QVERIFY(client.micSourcePending());
        };
        generated(false, 8);
        generated(true, 9);
        client.setScreenKey(true);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.key"));
        answerCopies(client, core.sent.last(), true, {}, epochValue(10));
        client.setScreenKey(false);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.unkey"));
        answerCopies(client, core.sent.last(), true, {}, {});
        bool programAccepted = false;
        client.keyForProgram([&](const RemoteTransmitClient::Answer& answer) {
            programAccepted = answer.accepted;
        });
        QCOMPARE(core.sent.last().verb, QByteArray("tx.key"));
        QCOMPARE(core.argument(core.sent.size()-1, "trigger").toString(), QStringLiteral("tci"));
        answerCopies(client, core.sent.last(), true, {}, epochValue(11));
        QVERIFY(programAccepted);
        client.unkeyForProgram(11);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.unkey"));
        QCOMPARE(core.argument(core.sent.size()-1, "epoch").toLongLong(), 11LL);
        QVERIFY(client.micSourcePending());
    }

    void radioMicAcceptedAckKeepsKeyWatchAliveAndRejectsProgram()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setMicSourceCapability(true);
        QVERIFY(client.requestMicSource(RemoteMicSource::RadioMic));
        const Sent request = core.sent.last();
        client.commandFinished(request.id, request.verb, true, {},
            {{0, "source", MirrorWireKind::Utf8, QStringLiteral("RadioMic")}});
        QVERIFY(client.micSourceSettled());
        QCOMPARE(client.acceptedMicSource(), RemoteMicSource::RadioMic);
        int keepalives = 0;
        client.setSessionKeepalive([&](quint64, quint32) { ++keepalives; return true; });
        client.setScreenKey(true);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.key"));
        client.commandFinished(core.sent.last().id, "tx.key", true, {}, epochValue(7));
        client.keepaliveTick();
        QVERIFY(keepalives > 0);
        bool programRefused = false;
        client.keyForProgram([&](const RemoteTransmitClient::Answer& answer) {
            programRefused = !answer.accepted && !answer.reason.isEmpty();
        });
        QVERIFY(programRefused);
        client.setScreenKey(false);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.unkey"));
    }

    void reconnectRetiresOldSourceReplyAndRestoresClientAudio()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setMicSourceCapability(true);
        QVERIFY(client.requestMicSource(RemoteMicSource::RadioMic));
        const Sent old = core.sent.last();
        client.setAvailable(false);
        client.setAvailable(true);
        client.setMicSourceCapability(true);
        client.commandFinished(old.id, old.verb, true, {},
            {{0, "source", MirrorWireKind::Utf8, QStringLiteral("RadioMic")}});
        QCOMPARE(client.acceptedMicSource(), RemoteMicSource::ClientAudio);
        QVERIFY(client.micSourceSettled());
        client.setScreenKey(true);
        QCOMPARE(core.sent.last().verb, QByteArray("tx.key"));
    }

    void sourceSenderReplacingSessionCannotInstallItsOldRequest()
    {
        RemoteTransmitClient* client = nullptr;
        RemoteTransmitClient tx([&](const QByteArray&, const QList<MirrorUpdate>&) {
            client->setAvailable(false);
            client->setAvailable(true);
            return quint32(99);
        });
        client = &tx;
        tx.setAvailable(true);
        tx.setMicSourceCapability(true);
        QVERIFY(!tx.requestMicSource(RemoteMicSource::RadioMic));
        tx.commandFinished(99, "tx.setMicSource", true, {},
            {{0, "source", MirrorWireKind::Utf8, QStringLiteral("RadioMic")}});
        QCOMPARE(tx.acceptedMicSource(), RemoteMicSource::ClientAudio);
        QVERIFY(tx.micSourceSettled());
    }

    void auxiliarySuccessStillSendsExistingChannelCopy()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        QList<QPair<quint64, quint32>> auxiliary;
        QList<QPair<quint64, quint32>> channel;
        int primary = 0;
        client.setAuxiliaryKeepalive([&](quint64 sequence, quint32 epoch) {
            auxiliary.append({sequence, epoch});
            return true;
        });
        client.setChannelKeepalive([&](quint64 sequence, quint32 epoch) {
            channel.append({sequence, epoch});
            return true;
        });
        client.setSessionKeepalive([&](quint64, quint32) {
            ++primary;
            return true;
        });
        client.setAvailable(true);
        client.setVoxArmed(true);
        QCOMPARE(auxiliary.size(), 1);
        QCOMPARE(channel.size(), 1);
        QCOMPARE(auxiliary.first(), channel.first());
        QCOMPARE(primary, 0);
        client.keepaliveTick();
        QCOMPARE(auxiliary.size(), 2);
        QCOMPARE(channel.size(), 2);
        QCOMPARE(auxiliary.last(), channel.last());
    }

    void auxiliaryCallbackReleaseFencesEveryOtherPath()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        int auxiliary = 0;
        int channel = 0;
        int primary = 0;
        client.setAuxiliaryKeepalive([&](quint64, quint32) {
            ++auxiliary;
            client.setTune(false);
            return true;
        });
        client.setChannelKeepalive([&](quint64, quint32) {
            ++channel;
            return true;
        });
        client.setSessionKeepalive([&](quint64, quint32) {
            ++primary;
            return true;
        });
        client.setAvailable(true);
        client.setVoxArmed(true);
        QCOMPARE(auxiliary, 1);
        QCOMPARE(channel, 0);
        QCOMPARE(primary, 0);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.tune"));
        client.keepaliveTick();
        QCOMPARE(auxiliary, 1);
        QCOMPARE(channel, 0);
        QCOMPARE(primary, 0);
    }

    void auxiliaryCallbackResetOrDeleteCannotSendOldSessionCopy()
    {
        Recorder core;
        auto* client = new RemoteTransmitClient(core.sender());
        QPointer<RemoteTransmitClient> alive(client);
        int channel = 0;
        client->setAuxiliaryKeepalive([&](quint64, quint32) {
            client->setAvailable(false);
            delete client;
            return true;
        });
        client->setChannelKeepalive([&](quint64, quint32) {
            ++channel;
            return true;
        });
        client->setAvailable(true);
        client->setVoxArmed(true);
        QVERIFY(alive.isNull());
        QCOMPARE(channel, 0);
    }

    void releaseDuringChannelSendPreventsPrimaryFallback()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        bool releaseDuringSend = true;
        int primarySends = 0;
        client.setChannelKeepalive([&](quint64, quint32) {
            if (releaseDuringSend) {
                releaseDuringSend = false;
                client.setTune(false);
            }
            return false;
        });
        client.setSessionKeepalive([&](quint64, quint32) {
            ++primarySends;
            return true;
        });
        client.setVoxArmed(true);
        QCOMPARE(core.sent.size(), 1);
        QCOMPARE(primarySends, 0);
        answerCopies(client, core.sent.last(), true, {}, {});
        client.keepaliveTick();
        QCOMPARE(primarySends, 1);
    }

    void aPressKeysWithTheScreenTriggerAndTheReleaseNamesItsEpoch()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        QSignalSpy keyDown(&client, &RemoteTransmitClient::micKeyDownChanged);
        QSignalSpy holds(&client, &RemoteTransmitClient::holdsTransmitChanged);

        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 1);
        QCOMPARE(core.sent.at(0).verb, QByteArrayLiteral("tx.key"));
        QCOMPARE(core.argument(0, "trigger").toString(), QStringLiteral("screen"));
        QVERIFY(client.micKeyDown());
        QVERIFY(!client.holdsTransmit());
        QCOMPARE(keyDown.count(), 1);

        answerCopies(client, core.sent.at(0), true, {}, epochValue(7));
        QVERIFY(client.holdsTransmit());
        QCOMPARE(client.screenEpoch(), 7u);
        QCOMPARE(holds.count(), 1);

        client.setScreenKey(true);   // still down: nothing more
        QCOMPARE(core.sent.size(), 1);

        client.setScreenKey(false);
        QCOMPARE(core.sent.size(), 2);
        QCOMPARE(core.sent.at(1).verb, QByteArrayLiteral("tx.unkey"));
        QCOMPARE(core.argument(1, "epoch").toLongLong(), 7LL);
        QVERIFY(!client.micKeyDown());
        QVERIFY(!client.holdsTransmit());
    }

    void aReleaseBeforeTheAnswerStopsWhateverKeyed()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        QCOMPARE(core.sent.size(), 2);
        QCOMPARE(core.argument(1, "epoch").toLongLong(),
                 qlonglong(RemoteTransmitClient::kReleaseAnyEpoch));
        // The key's answer after the release changes nothing.
        answerCopies(client, core.sent.at(0), true, {}, epochValue(3));
        QVERIFY(!client.holdsTransmit());
        QVERIFY(!client.micKeyDown());
        QCOMPARE(core.sent.size(), 2);
    }

    void aRefusalIsShownOnceWithItsCodeAndFix()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        QSignalSpy refused(&client, &RemoteTransmitClient::refused);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), false, QStringLiteral("Radio has the transmitter."),
                     refusal("otherDeviceHolds", "takeTransmit"));
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("Radio has the transmitter."));
        QCOMPARE(refused.at(0).at(1).toString(), QStringLiteral("otherDeviceHolds"));
        QCOMPARE(refused.at(0).at(2).toString(), QStringLiteral("takeTransmit"));
        QVERIFY(!client.micKeyDown());
        // The next press is a new command.
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 2);
        QVERIFY(core.sent.at(1).id != core.sent.at(0).id);
    }

    void aKeyTheCoreEndsMakesTheNextPressANewCommand()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        client.setCoreTransmitting(true);   // the state can come first
        answerCopies(client, core.sent.at(0), true, {}, epochValue(4));
        QVERIFY(client.holdsTransmit());
        client.setCoreTransmitting(false);   // a safety stop at the Core
        QVERIFY(!client.holdsTransmit());
        QVERIFY(!client.micKeyDown());
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), 2);
        QCOMPARE(core.sent.at(1).verb, QByteArrayLiteral("tx.key"));
        QVERIFY(core.sent.at(1).id != core.sent.at(0).id);
    }

    // Fix wave M7: a key the Core ends so soon that the mirrored
    // `transmitting` never rose still ends here: the Core's recorded stop
    // (txState's stopSerial) with nothing keyed ends it, and the
    // keepalives stop.
    void aShortKeyTheCoreStoppedEndsOnItsStop()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(4));
        QVERIFY(client.holdsTransmit());
        QVERIFY(client.keepaliveRunning());
        client.coreStopped(1, /*coreKeyed=*/false);
        QVERIFY(!client.holdsTransmit());
        QVERIFY(!client.micKeyDown());
        QVERIFY(!client.keepaliveRunning());
        // The next press is a new command.
        client.setScreenKey(true);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.key"));
        QVERIFY(core.sent.last().id != core.sent.at(0).id);
        // A stop while another key is on at the Core ends nothing here.
        answerCopies(client, core.sent.last(), true, {}, epochValue(5));
        QVERIFY(client.holdsTransmit());
        client.coreStopped(2, /*coreKeyed=*/true);
        QVERIFY(client.holdsTransmit());
    }

    // Fix wave 2 (the M7 race): a stop that names the key it ended by its
    // epoch never ends a newer key of this window's, pressed after the
    // stop and answered before the stop's update arrived, even with the
    // Core not yet transmitting it; the stop of that key itself does.
    void aStopForAnOlderKeyLeavesTheNewKeyOn()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(7));
        QVERIFY(client.holdsTransmit());
        // The Core stopped key 6 (an earlier one); key 7 is not keyed yet.
        client.coreStopped(1, /*coreKeyed=*/false, /*stopEpoch=*/6);
        QVERIFY(client.holdsTransmit());
        QVERIFY(client.keepaliveRunning());
        // Its own stop ends it.
        client.coreStopped(2, /*coreKeyed=*/true, /*stopEpoch=*/7);
        QVERIFY(!client.holdsTransmit());
        QVERIFY(!client.keepaliveRunning());
    }

    void aProgramsReleaseUnderTheOperatorsKeySendsNothing()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(5));
        RemoteTransmitClient::Answer answer;
        client.keyForProgram([&answer](const RemoteTransmitClient::Answer& a) { answer = a; });
        QCOMPARE(core.argument(1, "trigger").toString(), QStringLiteral("tci"));
        answerCopies(client, core.sent.at(1), true, {}, epochValue(5));
        QVERIFY(answer.accepted);
        QCOMPARE(answer.epoch, 5u);
        client.unkeyForProgram(5);
        QCOMPARE(core.sent.size(), 2);   // the operator's MOX holds it
        QVERIFY(client.holdsTransmit());
        // The operator's MOX off ends it.
        client.setScreenKey(false);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
        QCOMPARE(core.argument(2, "epoch").toLongLong(), 5LL);
    }

    void aProgramKeyAloneIsReleasedByItsUnkeyOrByMoxOff()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        bool accepted = false;
        client.keyForProgram([&accepted](const RemoteTransmitClient::Answer& a) {
            accepted = a.accepted;
        });
        QVERIFY(client.micKeyDown());
        answerCopies(client, core.sent.at(0), true, {}, epochValue(9));
        QVERIFY(accepted);
        client.setScreenKey(false);   // MOX off while the program keys
        QCOMPARE(core.sent.size(), 2);
        QCOMPARE(core.argument(1, "epoch").toLongLong(), 9LL);
        QVERIFY(!client.holdsTransmit());
    }

    void tuneOffGoesEvenBeforeItsAnswer()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setTune(true);
        QVERIFY(client.tuneAsked());
        QVERIFY(core.argument(0, "on").toBool());
        client.setTune(false);
        QVERIFY(!client.tuneAsked());
        QVERIFY(!core.argument(1, "on").toBool());
        // A refused TUNE on is no longer asked.
        client.setTune(true);
        answerCopies(client, core.sent.at(2), false, QStringLiteral("x"), {});
        QVERIFY(!client.tuneAsked());
        // Neither TUNE nor two-tone takes the microphone.
        QVERIFY(!client.micKeyDown());
    }

    void moxOffWhileTheCoreTransmitsForAnotherKeyAsksTheCore()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setCoreTransmitting(true);   // this device's VOX key, say
        client.setScreenKey(false);
        QCOMPARE(core.sent.size(), 1);
        QCOMPARE(core.sent.at(0).verb, QByteArrayLiteral("tx.unkey"));
        QCOMPARE(core.argument(0, "epoch").toLongLong(),
                 qlonglong(RemoteTransmitClient::kReleaseAnyEpoch));
        client.setCoreTransmitting(false);
        client.setScreenKey(false);   // nothing transmits: nothing to ask
        QCOMPARE(core.sent.size(), 1);
    }

    void aLostLinkForgetsEveryKeyAndAnswersAWaitingProgram()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        QString programReason;
        bool programAnswered = false;
        client.keyForProgram([&](const RemoteTransmitClient::Answer& a) {
            programAnswered = true;
            programReason = a.reason;
        });
        client.setAvailable(false);
        QVERIFY(programAnswered);
        QCOMPARE(programReason, QString::fromLatin1(RemoteTransmitClient::kNoLinkReason));
        QVERIFY(!client.micKeyDown());
        // A late answer for the old key does nothing.
        answerCopies(client, core.sent.at(0), true, {}, epochValue(2));
        QVERIFY(!client.holdsTransmit());
        // With no link a press is refused here.
        core.linked = false;
        client.setAvailable(true);
        QSignalSpy refused(&client, &RemoteTransmitClient::refused);
        client.setScreenKey(true);
        QCOMPARE(refused.count(), 1);
        QVERIFY(!client.micKeyDown());
    }

    // ---- Task 37: the keepalive -------------------------------------------

    // Keepalives run from the press to the release, every 100 ms, on the
    // "tx" channel when it takes them and otherwise on the session; the
    // epoch is 4294967295 until the key's answer, then the key's; the
    // sequence rises by one and never starts again while the link lasts.
    void keepalivesRunWhileKeyedOnTheChannelFirst()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        struct Keepalive {
            bool channel;
            quint64 sequence;
            quint32 epoch;
        };
        QList<Keepalive> sent;
        bool channelOpen = false;
        client.setChannelKeepalive([&](quint64 sequence, quint32 epoch) {
            if (!channelOpen) { return false; }
            sent.append({true, sequence, epoch});
            return true;
        });
        client.setSessionKeepalive([&](quint64 sequence, quint32 epoch) {
            sent.append({false, sequence, epoch});
            return true;
        });
        client.setAvailable(true);
        QVERIFY(!client.keepaliveRunning());
        QVERIFY(sent.isEmpty());

        client.setScreenKey(true);
        QVERIFY(client.keepaliveRunning());
        QCOMPARE(sent.size(), 1);   // the first goes at once
        QCOMPARE(sent.at(0).channel, false);
        QCOMPARE(sent.at(0).sequence, quint64(1));
        QCOMPARE(sent.at(0).epoch, RemoteTransmitClient::kReleaseAnyEpoch);

        answerCopies(client, core.sent.at(0), true, QString(), epochValue(7));
        channelOpen = true;
        client.keepaliveTick();
        QCOMPARE(sent.size(), 2);
        QCOMPARE(sent.at(1).channel, true);
        QCOMPARE(sent.at(1).sequence, quint64(2));
        QCOMPARE(sent.at(1).epoch, 7u);
        QCOMPARE(client.channelKeepalivesSent(), quint64(1));
        QCOMPARE(client.sessionKeepalivesSent(), quint64(1));

        client.setScreenKey(false);
        QVERIFY(!client.keepaliveRunning());
        const Sent firstOff = core.sent.last();

        // A later key still advances the sequence, but cannot send a
        // heartbeat until the earlier release reaches Core.
        client.setScreenKey(true);
        QCOMPARE(sent.last().sequence, quint64(2));
        answerCopies(client, firstOff, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(sent.last().sequence, quint64(4));
        client.setScreenKey(false);

        // A new link starts again from 1.
        client.setAvailable(false);
        client.setAvailable(true);
        client.setScreenKey(true);
        QCOMPARE(sent.last().sequence, quint64(1));
    }

    // VOX armed, TUNE and two-tone asked keep the keepalive going; nothing
    // runs without the link.
    void voxTuneAndTwoToneKeepTheKeepaliveGoing()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        int sent = 0;
        client.setSessionKeepalive([&](quint64, quint32) { ++sent; return true; });
        client.setVoxArmed(true);
        QVERIFY(!client.keepaliveRunning());   // no link yet
        client.setAvailable(true);
        QVERIFY(client.keepaliveRunning());
        client.setVoxArmed(false);
        QVERIFY(!client.keepaliveRunning());

        client.setTune(true);
        QVERIFY(client.keepaliveRunning());
        client.setTune(false);
        QVERIFY(!client.keepaliveRunning());
        answerCopies(client, core.sent.last(), true, QString(), {});

        client.setTwoTone(true);
        QVERIFY(client.keepaliveRunning());
        // The Core stopped transmitting: two-tone is over.
        client.setCoreTransmitting(true);
        client.setCoreTransmitting(false);
        QVERIFY(!client.keepaliveRunning());

        client.setScreenKey(true);
        QVERIFY(client.keepaliveRunning());
        client.setAvailable(false);
        QVERIFY(!client.keepaliveRunning());
        QVERIFY(sent >= 4);
    }

    // Reproduction: the primary command connection holds tx.unkey, while
    // the independent media TX path keeps delivering fresh heartbeats.
    // VOX stays armed after the local key release, so the Core must still
    // fail safe within its existing 400 ms deadline.
    void heldUnkeyWithVoxArmedCannotBeSustainedByIndependentHeartbeats()
    {
        SplitTxPaths paths;
        paths.watchdog.setVoxArmed(paths.device, true);
        paths.client.setVoxArmed(true);
        paths.client.setScreenKey(true);
        QCOMPARE(paths.primary.sent.size(), 1);
        QCOMPARE(paths.primary.sent.at(0).verb, QByteArrayLiteral("tx.key"));

        // The fake Core accepted the primary key before the primary path
        // stalls. It names the same device/epoch as the real Core would.
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.at(0), true, QString(), epochValue(7));
        paths.advanceTo(100);
        paths.client.keepaliveTick();
        QCOMPARE(paths.stops, 0);

        paths.client.setScreenKey(false);
        QCOMPARE(paths.primary.sent.size(), 2);
        QCOMPARE(paths.primary.sent.at(1).verb, QByteArrayLiteral("tx.unkey"));
        QVERIFY(paths.client.keepaliveRunning());  // VOX is still armed.
        const int channelBeforeRelease = paths.channelAccepted;
        const quint64 primaryBeforeRelease = paths.client.sessionKeepalivesSent();
        // Deliberately do not deliver the queued tx.unkey to the Core.
        for (qint64 at = 200; at <= 600; at += RemoteTxWatchdog::kKeepaliveIntervalMs) {
            paths.advanceTo(at);
            paths.client.keepaliveTick();
        }
        QCOMPARE(paths.channelAccepted, channelBeforeRelease);
        QCOMPARE(paths.client.sessionKeepalivesSent(), primaryBeforeRelease);
        QCOMPARE(paths.stops, 1);
        QVERIFY(!paths.keyed);
    }

    void heldProgramUnkeyWithVoxArmedCannotBeSustainedByIndependentHeartbeats()
    {
        SplitTxPaths paths;
        paths.watchdog.setVoxArmed(paths.device, true);
        paths.client.setVoxArmed(true);
        RemoteTransmitClient::Answer keyAnswer;
        paths.client.keyForProgram([&keyAnswer](const RemoteTransmitClient::Answer& answer) {
            keyAnswer = answer;
        });
        QCOMPARE(paths.primary.sent.size(), 1);
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.at(0), true, QString(), epochValue(7));
        QVERIFY(keyAnswer.accepted);
        paths.advanceTo(100);
        paths.client.keepaliveTick();

        paths.client.unkeyForProgram(7);
        QCOMPARE(paths.primary.sent.size(), 2);
        QCOMPARE(paths.primary.sent.at(1).verb, QByteArrayLiteral("tx.unkey"));
        QVERIFY(paths.client.keepaliveRunning());
        const int channelBeforeRelease = paths.channelAccepted;
        const quint64 primaryBeforeRelease = paths.client.sessionKeepalivesSent();
        for (qint64 at = 200; at <= 600; at += RemoteTxWatchdog::kKeepaliveIntervalMs) {
            paths.advanceTo(at);
            paths.client.keepaliveTick();
        }
        QCOMPARE(paths.channelAccepted, channelBeforeRelease);
        QCOMPARE(paths.client.sessionKeepalivesSent(), primaryBeforeRelease);
        QCOMPARE(paths.stops, 1);
        QVERIFY(!paths.keyed);
    }

    void assignedOffIdWithoutDeliveryCannotBeSustainedByPrimaryHeartbeats()
    {
        SplitTxPaths paths;
        paths.watchdog.setVoxArmed(paths.device, true);
        paths.client.setVoxArmed(true);
        paths.client.setScreenKey(true);
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.last(), true, QString(), epochValue(7));
        paths.advanceTo(100);
        paths.client.keepaliveTick();

        paths.primaryDelivering = true;
        paths.client.setScreenKey(false);
        const Sent droppedOff = paths.primary.sent.last();
        QCOMPARE(droppedOff.verb, QByteArrayLiteral("tx.unkey"));
        QVERIFY(droppedOff.id != 0);  // StationClient can assign an ID before a silent drop.
        // The fake transport never applies that command to the Core, but
        // can deliver subsequent primary keepalives. Media also works.
        const int primaryBefore = paths.primaryAccepted;
        const int channelBefore = paths.channelAccepted;
        for (qint64 at = 200; at <= 600; at += RemoteTxWatchdog::kKeepaliveIntervalMs) {
            paths.advanceTo(at);
            paths.client.keepaliveTick();
        }
        QCOMPARE(paths.primaryAccepted, primaryBefore);
        QCOMPARE(paths.channelAccepted, channelBefore);
        QCOMPARE(paths.stops, 1);
        QVERIFY(!paths.keyed);
    }

    void heldUnkeyThenRapidRekeyWithVoxOffCannotSustainOldKey()
    {
        SplitTxPaths paths;
        paths.client.setScreenKey(true);
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.at(0), true, QString(), epochValue(7));
        paths.advanceTo(100);
        paths.client.keepaliveTick();

        paths.client.setScreenKey(false);
        const Sent heldRelease = paths.primary.sent.last();
        QCOMPARE(heldRelease.verb, QByteArrayLiteral("tx.unkey"));
        QVERIFY(!paths.client.keepaliveRunning());
        // A new press restarts the timer while the old release remains
        // unresolved; neither path may sustain the old Core key.
        paths.client.setScreenKey(true);
        QCOMPARE(paths.primary.sent.last().verb, QByteArrayLiteral("tx.key"));
        QVERIFY(paths.client.keepaliveRunning());
        QCOMPARE(paths.client.keepaliveEpoch(), RemoteTransmitClient::kReleaseAnyEpoch);
        const int channelBeforeRelease = paths.channelAccepted;
        const quint64 primaryBeforeRelease = paths.client.sessionKeepalivesSent();
        for (qint64 at = 200; at <= 600; at += RemoteTxWatchdog::kKeepaliveIntervalMs) {
            paths.advanceTo(at);
            paths.client.keepaliveTick();
        }
        QCOMPARE(paths.channelAccepted, channelBeforeRelease);
        QCOMPARE(paths.client.sessionKeepalivesSent(), primaryBeforeRelease);
        QCOMPARE(paths.stops, 1);
        QVERIFY(!paths.keyed);
    }

    void acceptedReleaseStopsKeyAndLeavesVoxWatchRunning()
    {
        SplitTxPaths paths;
        paths.watchdog.setVoxArmed(paths.device, true);
        paths.client.setVoxArmed(true);
        paths.client.setScreenKey(true);
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.at(0), true, QString(), epochValue(7));
        paths.client.setScreenKey(false);
        const Sent release = paths.primary.sent.last();
        QCOMPARE(release.verb, QByteArrayLiteral("tx.unkey"));
        const quint64 channelBefore = paths.client.channelKeepalivesSent();
        const quint64 sessionBefore = paths.client.sessionKeepalivesSent();
        paths.client.keepaliveTick();
        QCOMPARE(paths.client.channelKeepalivesSent(), channelBefore);
        QCOMPARE(paths.client.sessionKeepalivesSent(), sessionBefore);

        // Delivery of the held primary command is the point at which the
        // Core's key actually ends; the accepted result follows it.
        paths.keyed = false;
        paths.watchdog.released(paths.device);
        answerCopies(paths.client, release, true, QString(), {});
        QVERIFY(paths.watchdog.isWatching(paths.device));
        QVERIFY(paths.client.keepaliveRunning());
        paths.advanceTo(100);
        paths.client.keepaliveTick();
        QCOMPARE(paths.stops, 0);
        QVERIFY(!paths.keyed);
        QCOMPARE(paths.client.channelKeepalivesSent(), channelBefore + 1);
    }

    void releaseFenceIsSetBeforeSendingOffCommand()
    {
        Recorder core;
        RemoteTransmitClient* clientPtr = nullptr;
        quint64 channelAtReentrantTick = 0;
        quint64 sessionAtReentrantTick = 0;
        RemoteTransmitClient client(
            [&](const QByteArray& verb, const QList<MirrorUpdate>& arguments) -> quint32 {
                const quint32 id = core.sender()(verb, arguments);
                if (verb == "tx.unkey") {
                    clientPtr->keepaliveTick();
                    channelAtReentrantTick = clientPtr->channelKeepalivesSent();
                    sessionAtReentrantTick = clientPtr->sessionKeepalivesSent();
                }
                return id;
            });
        clientPtr = &client;
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        const quint64 beforeChannel = client.channelKeepalivesSent();
        client.setScreenKey(false);
        QCOMPARE(channelAtReentrantTick, beforeChannel);
        QCOMPARE(sessionAtReentrantTick, quint64(0));
        // A returned command id alone still does not prove Core delivery.
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), beforeChannel);
        QCOMPARE(client.sessionKeepalivesSent(), quint64(0));
    }

    void senderResetCannotInsertAnOldSessionsOffResult()
    {
        Recorder core;
        RemoteTransmitClient* clientPtr = nullptr;
        quint32 oldOffId = 0;
        RemoteTransmitClient client(
            [&](const QByteArray& verb, const QList<MirrorUpdate>& arguments) -> quint32 {
                const quint32 id = core.sender()(verb, arguments);
                if (verb == "tx.unkey") {
                    oldOffId = id;
                    clientPtr->setAvailable(false);
                    clientPtr->setAvailable(true);
                }
                return id;
            });
        clientPtr = &client;
        client.setAvailable(true);
        client.setScreenKey(true);
        QSignalSpy refused(&client, &RemoteTransmitClient::refused);
        client.setScreenKey(false);
        QVERIFY(oldOffId != 0);
        client.commandFinished(oldOffId, QByteArrayLiteral("tx.unkey"), false,
                               QStringLiteral("stale refusal"), {});
        QCOMPARE(refused.count(), 0);
    }

    void senderDeletingClientDoesNotResumeAKeyCall()
    {
        RemoteTransmitClient* raw = nullptr;
        auto* client = new RemoteTransmitClient(
            [&](const QByteArray&, const QList<MirrorUpdate>&) -> quint32 {
                delete raw;
                raw = nullptr;
                return 100;
            });
        raw = client;
        QPointer<RemoteTransmitClient> alive(client);
        client->setAvailable(true);
        client->setScreenKey(true);
        QVERIFY(alive.isNull());
    }

    void senderDeletionStopsEveryTransmitCaller()
    {
        for (int scenario = 0; scenario < 7; ++scenario) {
            RemoteTransmitClient* raw = nullptr;
            bool deleteNext = false;
            quint32 nextId = 100;
            auto* client = new RemoteTransmitClient(
                [&](const QByteArray&, const QList<MirrorUpdate>&) -> quint32 {
                    const quint32 id = nextId++;
                    if (deleteNext) {
                        delete raw;
                        raw = nullptr;
                    }
                    return id;
                });
            raw = client;
            QPointer<RemoteTransmitClient> alive(client);
            client->setAvailable(true);
            if (scenario == 0) { client->setScreenKey(true); }
            deleteNext = true;
            switch (scenario) {
            case 0: client->setScreenKey(false); break;
            case 1: client->setTune(true); break;
            case 2: client->setTune(false); break;
            case 3: client->setTunerTune(true); break;
            case 4: client->setTwoTone(true); break;
            case 5: client->keyForProgram({}); break;
            case 6: client->unkeyForProgram(7); break;
            }
            QVERIFY2(alive.isNull(), qPrintable(QStringLiteral("scenario %1").arg(scenario)));
        }
    }

    void resetSignalDeletingClientDoesNotResumeAvailabilityCall()
    {
        Recorder core;
        auto* client = new RemoteTransmitClient(core.sender());
        QPointer<RemoteTransmitClient> alive(client);
        client->setAvailable(true);
        client->setScreenKey(true);
        QObject::connect(client, &RemoteTransmitClient::micKeyDownChanged, client,
                         [client](bool down) {
                             if (!down) { delete client; }
                         }, Qt::DirectConnection);
        client->setAvailable(false);
        QVERIFY(alive.isNull());
    }

    void availabilityHeartbeatCallbackCanDeleteClient()
    {
        Recorder core;
        auto* client = new RemoteTransmitClient(core.sender());
        QPointer<RemoteTransmitClient> alive(client);
        bool primaryCalled = false;
        client->setVoxArmed(true);
        client->setSessionKeepalive([&](quint64, quint32) {
            primaryCalled = true;
            return true;
        });
        client->setChannelKeepalive([client](quint64, quint32) {
            delete client;
            return false;
        });
        client->setAvailable(true);  // Starts the VOX heartbeat immediately.
        QVERIFY(alive.isNull());
        QVERIFY(!primaryCalled);
    }

    void refusedUnknownOrWrongVerbReplyCannotLiftReleaseFence()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent release = core.sent.last();
        const quint64 channelBefore = client.channelKeepalivesSent();

        client.commandFinished(release.id + 10, release.verb, true, {}, {});
        client.commandFinished(release.id, QByteArrayLiteral("tx.key"), true, {}, {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        client.commandFinished(release.id, release.verb, false, QStringLiteral("refused"), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        // A later duplicate accepted reply cannot turn the refusal into a
        // delivery barrier.
        client.commandFinished(release.id, release.verb, true, {}, {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
    }

    void oldCoreStopObservationCannotStandInForOffDelivery()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent release = core.sent.last();
        const quint64 channelBefore = client.channelKeepalivesSent();
        // Either observation can have been queued ahead of the off intent.
        client.setCoreTransmitting(false);
        client.coreStopped(1, false, 7);
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        answerCopies(client, release, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore + 1);
    }

    void sendFailureKeepsReleaseFenceUntilSessionReset()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        const quint64 channelBefore = client.channelKeepalivesSent();
        core.linked = false;
        client.setScreenKey(false);  // sender returns 0; no Core reply can arrive.
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        client.setAvailable(false);
        core.linked = true;
        client.setAvailable(true);
        QVERIFY(client.channelKeepalivesSent() > 0);  // VOX restarts on the new session.
    }

    void failedReleaseWarnsOnceAndRejectsNewTransmitIntents()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        core.linked = false;
        QSignalSpy refused(&client, &RemoteTransmitClient::refused);
        client.setScreenKey(false);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.last().at(0).toString(),
                 QString::fromLatin1(RemoteTransmitClient::kReleaseFailedReason));
        client.setTune(false);  // A second failed off needs no second notice.
        QCOMPARE(refused.count(), 1);

        core.linked = true;
        const int sentBefore = core.sent.size();
        client.setScreenKey(true);
        client.setTune(true);
        client.setTunerTune(true);
        client.setTwoTone(true);
        RemoteTransmitClient::Answer programAnswer;
        client.keyForProgram([&](const RemoteTransmitClient::Answer& answer) {
            programAnswer = answer;
        });
        QCOMPARE(core.sent.size(), sentBefore);
        QVERIFY(!client.screenKeyDown());
        QVERIFY(!client.tuneAsked());
        QVERIFY(!programAnswer.accepted);
        QCOMPARE(programAnswer.reason,
                 QString::fromLatin1(RemoteTransmitClient::kReleaseFailedReason));

        client.setAvailable(false);
        client.setAvailable(true);
        client.setScreenKey(true);
        QCOMPARE(core.sent.size(), sentBefore + 1);
    }

    void failedMoxUnkeyIsNotClearedByAcceptedNoopTuneOff()
    {
        SplitTxPaths paths;
        paths.watchdog.setVoxArmed(paths.device, true);
        paths.client.setVoxArmed(true);
        paths.client.setScreenKey(true);
        paths.keyed = true;
        paths.watchdog.setKeyed(paths.device, true, 7);
        answerCopies(paths.client, paths.primary.sent.last(), true, QString(), epochValue(7));
        paths.advanceTo(100);
        paths.client.keepaliveTick();

        paths.primary.linked = false;
        paths.primaryDelivering = true;  // A recovered primary can carry heartbeats.
        paths.client.setScreenKey(false);  // sender returns 0; Core keeps MOX on.
        paths.primary.linked = true;
        paths.client.setTune(false);  // Core's non-TUNE off is accepted, but a no-op.
        const Sent laterOff = paths.primary.sent.last();
        QCOMPARE(laterOff.verb, QByteArrayLiteral("tx.tune"));
        answerCopies(paths.client, laterOff, true, QString(), {});
        const int channelBefore = paths.channelAccepted;
        const int primaryBefore = paths.primaryAccepted;
        for (qint64 at = 200; at <= 600; at += RemoteTxWatchdog::kKeepaliveIntervalMs) {
            paths.advanceTo(at);
            paths.client.keepaliveTick();
        }
        QCOMPARE(paths.channelAccepted, channelBefore);
        QCOMPARE(paths.primaryAccepted, primaryBefore);
        QCOMPARE(paths.stops, 1);
        QVERIFY(!paths.keyed);
    }

    void refusedMoxUnkeyIsNotClearedByAcceptedNoopTwoToneOff()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent unkey = core.sent.last();
        answerCopies(client, unkey, false, QStringLiteral("refused"), {});
        client.setTwoTone(false);
        const Sent laterOff = core.sent.last();
        QCOMPARE(laterOff.verb, QByteArrayLiteral("tx.twoTone"));
        answerCopies(client, laterOff, true, QString(), {});
        const quint64 channelBefore = client.channelKeepalivesSent();
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
    }

    void previousSessionReplyCannotLiftTheNewSessionsReleaseFence()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent oldOff = core.sent.last();
        client.setAvailable(false);
        client.setAvailable(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent newOff = core.sent.last();
        const quint64 channelBefore = client.channelKeepalivesSent();
        answerCopies(client, oldOff, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        answerCopies(client, newOff, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore + 1);
    }

    void olderOffReplyCannotLiftNewerOffFence()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent firstOff = core.sent.last();
        client.setScreenKey(true);
        client.setScreenKey(false);
        const Sent secondOff = core.sent.last();
        const quint64 channelBefore = client.channelKeepalivesSent();
        answerCopies(client, firstOff, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        answerCopies(client, secondOff, true, QString(), {});
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore + 1);
    }

    void offVariantsUseThePrimaryUntilTheirAcceptedReply()
    {
        const QList<QByteArray> verbs = {QByteArrayLiteral("tx.tune"),
                                         QByteArrayLiteral("tx.tunerTune"),
                                         QByteArrayLiteral("tx.twoTone")};
        for (const QByteArray& verb : verbs) {
            Recorder core;
            RemoteTransmitClient client(core.sender());
            client.setChannelKeepalive([](quint64, quint32) { return true; });
            client.setSessionKeepalive([](quint64, quint32) { return true; });
            client.setAvailable(true);
            client.setVoxArmed(true);
            if (verb == "tx.tune") {
                client.setTune(true);
                client.setTune(false);
            } else if (verb == "tx.tunerTune") {
                client.setTunerTune(true);
                client.setTunerTune(false);
            } else {
                client.setTwoTone(true);
                client.setTwoTone(false);
            }
            const Sent off = core.sent.last();
            QCOMPARE(off.verb, verb);
            QCOMPARE(core.argument(core.sent.size() - 1, "on").toBool(), false);
            const quint64 channelBefore = client.channelKeepalivesSent();
            client.keepaliveTick();
            QCOMPARE(client.channelKeepalivesSent(), channelBefore);
            answerCopies(client, off, true, QString(), {});
            client.keepaliveTick();
            QCOMPARE(client.channelKeepalivesSent(), channelBefore + 1);
        }
    }

    void acceptedReleaseAndRapidRekeyRepliesKeepNewKeyState()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setChannelKeepalive([](quint64, quint32) { return true; });
        client.setSessionKeepalive([](quint64, quint32) { return true; });
        client.setAvailable(true);
        client.setVoxArmed(true);
        client.setScreenKey(true);
        const Sent firstKey = core.sent.last();
        answerCopies(client, firstKey, true, QString(), epochValue(7));
        client.setScreenKey(false);
        const Sent release = core.sent.last();
        QCOMPARE(release.verb, QByteArrayLiteral("tx.unkey"));
        QCOMPARE(core.argument(core.sent.size() - 1, "epoch").toLongLong(), qlonglong(7));

        // The operator presses again while release is still in flight.
        client.setScreenKey(true);
        const Sent nextKey = core.sent.last();
        QCOMPARE(nextKey.verb, QByteArrayLiteral("tx.key"));
        answerCopies(client, nextKey, true, QString(), epochValue(8));
        QVERIFY(client.holdsTransmit());
        QCOMPARE(client.keepaliveEpoch(), 8u);
        const quint64 channelBefore = client.channelKeepalivesSent();
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore);
        answerCopies(client, release, true, QString(), {});
        QVERIFY(client.holdsTransmit());
        QCOMPARE(client.keepaliveEpoch(), 8u);
        // An accepted old-epoch release proves ordered delivery, not that
        // the newer epoch 8 key was turned off by the Core.
        client.keepaliveTick();
        QCOMPARE(client.channelKeepalivesSent(), channelBefore + 1);
        client.setScreenKey(false);
        QCOMPARE(core.argument(core.sent.size() - 1, "epoch").toLongLong(), qlonglong(8));
    }
    // TX rulings review (I-1a): after MOX is let go, this window's VOX,
    // two-tone or TUNE may keep the Core transmitting. The release memory
    // then does not count, and the lit button's press is the unkey.
    void somethingElseOnTheAirMakesThePressAnUnkey()
    {
        for (int what = 0; what < 3; ++what) {
            Recorder core;
            RemoteTransmitClient client(core.sender());
            client.setAvailable(true);
            client.setScreenKey(true);
            answerCopies(client, core.sent.at(0), true, {}, epochValue(5));
            client.setCoreTransmitting(true);
            switch (what) {
            case 0: client.setVoxArmed(true); break;
            case 1: client.setTwoTone(true); break;
            default: client.setTune(true); break;
            }
            client.setScreenKey(false);
            QVERIFY2(!client.screenReleasePending(), qPrintable(QString::number(what)));
            // The press: the button's off, while the Core still transmits.
            const int keys = std::count_if(core.sent.cbegin(), core.sent.cend(),
                [](const Sent& c) { return c.verb == "tx.key"; });
            const int unkeys = std::count_if(core.sent.cbegin(), core.sent.cend(),
                [](const Sent& c) { return c.verb == "tx.unkey"; });
            client.setScreenKey(false);
            QCOMPARE(std::count_if(core.sent.cbegin(), core.sent.cend(),
                [](const Sent& c) { return c.verb == "tx.key"; }), keys);
            QCOMPARE(std::count_if(core.sent.cbegin(), core.sent.cend(),
                [](const Sent& c) { return c.verb == "tx.unkey"; }), unkeys + 1);
            QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
            QCOMPARE(core.argument(core.sent.size() - 1, "epoch").toLongLong(),
                     qlonglong(RemoteTransmitClient::kReleaseAnyEpoch));
        }

        // TUNE let go while this window's MOX is still down: not a TUNE
        // press to turn it back on.
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setTune(true);
        client.setScreenKey(true);
        client.setTune(false);
        QVERIFY(!client.tuneReleasePending());
    }

    // TX rulings review (I-1b): a release the Core refuses, or one that
    // cannot be sent, stopped nothing. The memory is forgotten, so the
    // next press is the unkey again, and it goes.
    void aFailedReleaseLetsThePressRetryTheUnkey()
    {
        {
            Recorder core;
            RemoteTransmitClient client(core.sender());
            client.setAvailable(true);
            client.setScreenKey(true);
            answerCopies(client, core.sent.at(0), true, {}, epochValue(9));
            client.setCoreTransmitting(true);
            client.setScreenKey(false);
            QVERIFY(client.screenReleasePending());
            answerCopies(client, core.sent.last(), false, QStringLiteral("refused"), {});
            QVERIFY(!client.screenReleasePending());
            const int sent = core.sent.size();
            client.setScreenKey(false);
            QCOMPARE(core.sent.size(), sent + 1);
            QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
        }
        {
            Recorder core;
            RemoteTransmitClient client(core.sender());
            client.setAvailable(true);
            client.setScreenKey(true);
            answerCopies(client, core.sent.at(0), true, {}, epochValue(9));
            client.setCoreTransmitting(true);
            core.linked = false;
            client.setScreenKey(false);
            QVERIFY(!client.screenReleasePending());
            core.linked = true;
            const int sent = core.sent.size();
            client.setScreenKey(false);
            QCOMPARE(core.sent.size(), sent + 1);
            QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
        }
        {
            // TUNE off refused: its memory goes too.
            Recorder core;
            RemoteTransmitClient client(core.sender());
            client.setAvailable(true);
            client.setTune(true);
            client.setTune(false);
            QVERIFY(client.tuneReleasePending());
            answerCopies(client, core.sent.last(), false, QStringLiteral("refused"), {});
            QVERIFY(!client.tuneReleasePending());
        }
    }

    // TX rulings review (I-1): the memory lasts the grace from the release
    // and its acceptance, whatever the Core still reads after it.
    void theReleaseMemoryEndsAfterTheGraceEvenWhileTheCoreTransmits()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(4));
        client.setCoreTransmitting(true);
        client.setScreenKey(false);
        answerCopies(client, core.sent.last(), true, {}, {});
        client.setCoreTransmitting(true);   // a late `true`
        QVERIFY(client.screenReleasePending());
        QTRY_VERIFY_WITH_TIMEOUT(!client.screenReleasePending(),
                                 RemoteTransmitClient::kReleaseConfirmGraceMs * 4);
        const int sent = core.sent.size();
        client.setScreenKey(false);   // the lit button's press unkeys
        QCOMPARE(core.sent.size(), sent + 1);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
    }
    // TX rulings re-review: a release sent and never answered is
    // forgotten at the grace too; the lit button's press then unkeys.
    void anUnansweredReleaseIsForgottenAfterTheGrace()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(6));
        client.setCoreTransmitting(true);
        client.setScreenKey(false);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
        QVERIFY(client.screenReleasePending());
        QElapsedTimer waited;
        waited.start();
        QTRY_VERIFY_WITH_TIMEOUT(!client.screenReleasePending(),
                                 RemoteTransmitClient::kReleaseConfirmGraceMs * 4);
        QVERIFY(waited.elapsed() >= RemoteTransmitClient::kReleaseConfirmGraceMs - 10);
        const int sent = core.sent.size();
        client.setScreenKey(false);
        QCOMPARE(core.sent.size(), sent + 1);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
    }

    // TX rulings re-review: a program that keys through this window after
    // MOX was let go keeps the radio on the air; the press unkeys it.
    void aProgramKeyAfterAReleaseMakesThePressAnUnkey()
    {
        Recorder core;
        RemoteTransmitClient client(core.sender());
        client.setAvailable(true);
        client.setScreenKey(true);
        answerCopies(client, core.sent.at(0), true, {}, epochValue(3));
        client.setCoreTransmitting(true);
        client.setScreenKey(false);
        QVERIFY(client.screenReleasePending());
        client.keyForProgram([](const RemoteTransmitClient::Answer&) {});
        answerCopies(client, core.sent.last(), true, {}, epochValue(4));
        QVERIFY(!client.screenReleasePending());
        const int keys = std::count_if(core.sent.cbegin(), core.sent.cend(),
            [](const Sent& c) { return c.verb == "tx.key"; });
        client.setScreenKey(false);   // the press
        QCOMPARE(std::count_if(core.sent.cbegin(), core.sent.cend(),
            [](const Sent& c) { return c.verb == "tx.key"; }), keys);
        QCOMPARE(core.sent.last().verb, QByteArrayLiteral("tx.unkey"));
        QCOMPARE(core.argument(core.sent.size() - 1, "epoch").toLongLong(), 4LL);
        QVERIFY(!client.holdsTransmit());
    }
};

QTEST_MAIN(TestRemoteTransmitClient)
#include "tst_remote_transmit_client.moc"
