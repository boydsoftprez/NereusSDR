// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_hosting_slice_operations.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 10: the hosting desktop's
// slice requests run as the station device through
// StationServer::invokeAsStationDevice() (HostingSliceActions), over one
// Core and the in-process loopback, with remote devices signed in by keys
// made at run time:
//   - an add at full capacity gets the question a remote device gets, field
//     by field, and nothing closes before the proceed;
//   - a take of a remote device's slice on the air is refused in the words a
//     remote take gets;
//   - a close with a remote listener keeps the slice (ruling Q6); with
//     nobody it closes, to none when it was the last;
//   - a stale take (an old incarnation) and a select of a slice no longer on
//     the Core are refused;
//   - a verb the host does not run is refused as unknown; without a
//     server, nothing runs;
//   - take-over parity: a remote device takes the host's slice under the
//     same rules, the host hears controlTaken with Take it back, and one
//     tap returns control; with nobody at the desktop the Core's own slice
//     stays its own; the desktop's key never lands on the slice it lost;
//   - core-slice take-over: with nobody at the desktop a device at
//     sliceAccessVersion 3 takes the Core's own slice with no question,
//     not on the air, and an older one is refused as before; with someone
//     there, the take-over rules hold.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 10,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: the hosting desktop's refusal
//               names who holds a slice as a remote window's does, and a
//               remote window names the hosting desktop by its name.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over parity: a remote device takes the host's slice
//               and the host takes it back from its controlTaken notice.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over fix wave: the desktop's key never lands on the
//               slice a remote device took from it (I-2); its Take it back
//               refused while transmitting keeps the card (M-3). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: take-over re-review (N-1): nor on a phone's slice the
//               flag stayed on after the phone let go. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings: a slice-sharing keyer's key moves the flag
//               from another device's slice once admitted, and in the
//               unkey tail is told the radio is on the air (item 4); a
//               closed card's Take it back is forgotten; the desktop's
//               footswitch and mic PTT key its active slice (ruling 8.11
//               on a hosting desktop), and with no desktop key where the
//               flag is. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over (JJ, 2026-09-30): with nobody at the
//               desktop a device at sliceAccessVersion 3 takes the Core's
//               own slice at once, not on the air; with someone there the
//               take-over rules hold. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/session/SliceAccessPolicy.h"
#include "gui/HostingSliceActions.h"

#include <QSignalSpy>

namespace {

const QHash<QByteArray, int> kShares{{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}};
// Take-over parity: a device at sliceAccess 2 (Take it back on
// controlTaken).
const QHash<QByteArray, int> kSharesBack{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}};
// Take-over fix wave (M-3): sliceAccess 2 with remote transmit.
const QHash<QByteArray, int> kSharesBackTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}, {"remoteTx", 1}};
// Core-slice take-over (JJ, 2026-09-30): a device at sliceAccess 3 may take
// the Core's own slice with nobody at the Core's desktop.
const QHash<QByteArray, int> kSharesCore{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 3}};
const QHash<QByteArray, int> kSharesCoreTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 3}, {"remoteTx", 1}};
const QHash<QByteArray, int> kSharesTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}, {"remoteTx", 1}};

const QString kHostName = QStringLiteral("Mac");

// The desktop begins hosting, as StationHost does: the station device is a
// connected device named `kHostName`, and adopts every slice nobody owns.
void startHosting(Core& core)
{
    const QByteArray& station = SliceOwnership::stationDevice();
    core.server->deviceSessions()->registerHostingDevice(station, kHostName, kHostName);
    core.server->setStationDeviceWords(kHostName, kHostName);
    SliceOwnership* ownership = core.model->sliceOwnership();
    if (!ownership->adoptUnowned(station).isEmpty()) {
        core.model->setActiveSliceByIdFor(station, ownership->activeFor(station));
    }
}

QList<int> ownedBy(const Core& core, const QByteArray& device)
{
    return core.model->sliceOwnership()->ownedBy(device);
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString();
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

QList<MirrorUpdate> refOf(const Core& core, int sliceId, qint64 incarnationShift = 0)
{
    return {int64("sliceId", sliceId),
            int64("incarnation",
                  static_cast<qint64>(core.model->sliceOwnership()->incarnation(sliceId))
                      + incarnationShift)};
}

QJsonObject lastOfType(const LoopbackTransport* app, const QString& type)
{
    const QList<QJsonObject> all = ofType(app->received(), type);
    return all.isEmpty() ? QJsonObject{} : all.last();
}

// A question with what differs by who asked taken out: its ids, and the
// asker's device id wherever it appears.
QJsonObject comparable(QJsonObject question, const QString& askerWireId)
{
    question.remove(QStringLiteral("id"));
    question.remove(QStringLiteral("forCommandId"));
    QString text = QString::fromUtf8(QJsonDocument(question).toJson(QJsonDocument::Compact));
    if (!askerWireId.isEmpty()) {
        text.replace(askerWireId, QStringLiteral("ASKER"));
    }
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

// Both receivers held: the asker's slice 0 on 7.074 MHz, the phone's slice
// on 14.074 MHz.
void holdBothReceivers(Core& core, const Device& phone)
{
    for (int id : ownedBy(core, phone.key.fingerprint())) {
        core.model->sliceById(id)->setFrequency(14074000.0);
    }
}

// The takeReceiver choice for `stream`, or -1.
qint64 choiceFor(const QJsonObject& ask, int stream)
{
    for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
        if (v.toObject().value(QStringLiteral("streamIndex")).toInt(-1) == stream) {
            return v.toObject().value(QStringLiteral("choice")).toInteger(-1);
        }
    }
    return -1;
}

// A phone that chose its own slice for transmit and let go of it: the flag
// stays on the phone's slice with nobody holding transmit (ruling 8.10).
// `p` is the phone's slice.
void phoneLeavesTheFlag(Core& core, const Device& phone, LoopbackTransport* appP, int* p)
{
    *p = -1;
    SliceOwnership* ownership = core.model->sliceOwnership();
    if (ownership->ownedBy(phone.key.fingerprint()).isEmpty()) {
        core.invoke(appP, "addSlice", {utf8("initialPanId", QString())});
    }
    const QList<int> own = ownership->ownedBy(phone.key.fingerprint());
    QVERIFY(!own.isEmpty());
    *p = own.first();
    SliceModel* slice = core.model->sliceById(*p);
    slice->setDspMode(DSPMode::USB);
    slice->setFrequency(14210000.0);
    core.invoke(appP, "tx.take");
    QTRY_VERIFY_WITH_TIMEOUT(core.server->transmitHolder()->isHeldBy(phone.key.fingerprint()),
                             5000);
    core.invoke(appP, "tx.setTxSlice", {int64("sliceId", *p)});
    QTRY_COMPARE_WITH_TIMEOUT(core.model->txSliceArbiter()->txBoundSliceId(), *p, 5000);
    core.server->releaseTransmitFor(phone.key.fingerprint(), QStringLiteral("The test let go."));
    QTRY_VERIFY_WITH_TIMEOUT(!core.server->transmitHolder()->holder().has_value(), 5000);
}

} // namespace

class TstHostingSliceOperations : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("hosting-slice-ops-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void anAddAtFullCapacityAsksWhatARemoteDeviceIsAsked()
    {
        Device phone;

        // The host asks.
        QJsonObject hostQuestion;
        QString hostWireId;
        QString hostRefusal;
        QString remoteRefusal;
        {
            Core core;
            core.model->configureStreamPool(2, 5, 192000);
            core.model->sliceById(0)->setFrequency(7074000.0);
            startHosting(core);
            const QByteArray station = SliceOwnership::stationDevice();
            QCOMPARE(ownedBy(core, station), QList<int>{0});
            core.pair(phone);
            LoopbackTransport* appP = core.signIn(phone, kShares);
            QVERIFY(admitted(appP));
            holdBothReceivers(core, phone);
            QCOMPARE(receiversInUse(*core.model), 2);
            const int slicesBefore = static_cast<int>(core.model->slices().size());

            HostingSliceActions host(core.server.get(), core.model.get());
            QSignalSpy asked(&host, &HostingSliceActions::question);
            QSignalSpy refused(&host, &HostingSliceActions::refused);
            QSignalSpy finished(&host, &HostingSliceActions::finished);
            host.addOnPan(QStringLiteral("new-pan"));
            QTRY_COMPARE(asked.count(), 1);
            // As a remote device's: the Add is answered with who holds the
            // receivers, and the question follows.
            QCOMPARE(finished.count(), 1);
            QCOMPARE(finished.first().at(0).toByteArray(), QByteArrayLiteral("addSliceOnPan"));
            QCOMPARE(finished.first().at(2).toBool(), false);
            hostRefusal = finished.first().at(3).toString();
            QCOMPARE(refused.count(), 1);
            QCOMPARE(refused.first().at(0).toString(), hostRefusal);
            // Nothing closed before the proceed.
            QCOMPARE(static_cast<int>(core.model->slices().size()), slicesBefore);
            QVERIFY(!ownedBy(core, phone.key.fingerprint()).isEmpty());

            const SessionMessage question = asked.first().at(0).value<SessionMessage>();
            hostQuestion = QJsonDocument::fromJson(SessionMessages::encode(question)).object();
            hostWireId = StationIdentity::toBase64Url(station);

            // Proceed on the phone's receiver: the host's new slice opens on it.
            const int phoneStream =
                core.model->sliceById(ownedBy(core, phone.key.fingerprint()).first())->streamIndex();
            const qint64 choice = choiceFor(hostQuestion, phoneStream);
            QVERIFY(choice >= 0);
            host.proceed(hostQuestion.value(QStringLiteral("id")).toInteger(), choice);
            QTRY_COMPARE(finished.count(), 2);
            QCOMPARE(finished.last().at(0).toByteArray(), QByteArrayLiteral("confirm.proceed"));
            QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
            QTRY_COMPARE(ownedBy(core, station).size(), 2);
            bool onNewPan = false;
            for (int id : ownedBy(core, station)) {
                if (core.model->sliceById(id)->panKey() == QStringLiteral("new-pan")) {
                    onNewPan = true;
                }
            }
            QVERIFY(onNewPan);
        }

        // A remote device in the same place asks.
        QJsonObject remoteQuestion;
        Device mac(kHostName, QStringLiteral("computer"), kHostName);
        {
            Core core;
            core.model->configureStreamPool(2, 5, 192000);
            core.model->sliceById(0)->setFrequency(7074000.0);
            core.pair(mac);
            core.pair(phone);
            LoopbackTransport* appM = core.signIn(mac, kShares);
            QVERIFY(admitted(appM));
            QCOMPARE(ownedBy(core, mac.key.fingerprint()), QList<int>{0});
            LoopbackTransport* appP = core.signIn(phone, kShares);
            QVERIFY(admitted(appP));
            holdBothReceivers(core, phone);
            const QJsonObject result =
                core.invoke(appM, "addSliceOnPan", {utf8("panId", QStringLiteral("new-pan"))});
            QVERIFY(!accepted(result));
            remoteRefusal = reasonOf(result);
            QTRY_VERIFY(!lastOfType(appM, QStringLiteral("confirm.request")).isEmpty());
            remoteQuestion = lastOfType(appM, QStringLiteral("confirm.request"));
        }

        QVERIFY(!hostRefusal.isEmpty());
        QCOMPARE(hostRefusal, remoteRefusal);
        QVERIFY(OperatorWording::isPlain(hostRefusal));
        const QJsonObject host = comparable(hostQuestion, hostWireId);
        const QJsonObject remote = comparable(remoteQuestion, mac.id());
        QVERIFY2(host == remote,
                 qPrintable(QStringLiteral("host %1\nremote %2")
                                .arg(QString::fromUtf8(QJsonDocument(host).toJson()),
                                     QString::fromUtf8(QJsonDocument(remote).toJson()))));
    }

    void aTakeOfATransmittingSliceIsRefusedInTheRemoteWords()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        startHosting(core);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appB));

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy refused(&host, &HostingSliceActions::refused);
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.listen(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().at(2).toBool(), qPrintable(finished.first().at(3).toString()));
        QVERIFY(core.model->sliceOwnership()->listenersOf(0).contains(SliceOwnership::stationDevice()));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));

        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        const QString words = QStringLiteral("Slice A is transmitting. Take control once it stops.");
        const QJsonObject remote = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision",
                         static_cast<qint64>(core.model->sliceOwnership()->controlRevision(0))));
        QVERIFY(!accepted(remote));
        QCOMPARE(reasonOf(remote), words);

        host.takeControl(0);
        QTRY_COMPARE(finished.count(), 2);
        QCOMPARE(finished.last().at(2).toBool(), false);
        QCOMPARE(finished.last().at(3).toString(), words);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), words);
        QVERIFY(OperatorWording::isPlain(words));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        QVERIFY(mox->isMox());

        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void aCloseWithARemoteListenerKeepsTheSlice()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        Device l;
        core.pair(l);
        LoopbackTransport* appL = core.signIn(l, kShares);
        QVERIFY(admitted(appL));
        QVERIFY(accepted(core.invoke(appL, "slice.listen", refOf(core, 0))));

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.close(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().at(2).toBool(), qPrintable(finished.first().at(3).toString()));
        QVERIFY(core.model->sliceById(0) != nullptr);
        QVERIFY(!ownedBy(core, station).contains(0));
        QVERIFY(core.model->sliceOwnership()->listenersOf(0).contains(l.key.fingerprint()));
    }

    void aCloseWithNobodyListeningClosesTheLastSlice()
    {
        Core core;
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        QCOMPARE(static_cast<int>(core.model->slices().size()), 1);

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.close(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().at(2).toBool(), qPrintable(finished.first().at(3).toString()));
        QTRY_COMPARE(static_cast<int>(core.model->slices().size()), 0);
    }

    void aStaleTakeAndAStaleSelectAreRefused()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        startHosting(core);

        // A take that names an incarnation the slice no longer has.
        QList<MirrorUpdate> stale = refOf(core, 0, -1);
        stale << int64("controlRevision",
                       static_cast<qint64>(core.model->sliceOwnership()->controlRevision(0)));
        SessionMessage answer;
        bool answered = false;
        core.server->invokeAsStationDevice(
            SessionMessages::commandInvoke("slice.takeControl", 7, stale),
            [&](const SessionMessage& result) {
                answer = result;
                answered = true;
            },
            {});
        QTRY_VERIFY(answered);
        QVERIFY(!answer.accepted);
        QVERIFY(!answer.reason.isEmpty());
        QVERIFY(OperatorWording::isPlain(answer.reason));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());

        // A select of a slice no longer on the Core.
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy refused(&host, &HostingSliceActions::refused);
        host.select(42);
        // Answered before select() returns: the window reads it at once.
        QCOMPARE(refused.count(), 1);
        QVERIFY(OperatorWording::isPlain(refused.first().at(0).toString()));
    }

    // Slice control plan Task 17: the hosting desktop's own refusal of a
    // slice it does not hold names the holder as a remote window's does:
    // the device's name, the plain word for its kind when it has no name,
    // "another device" when the Core knows neither, the Core for nobody.
    // A remote window refused a slice the hosting desktop holds reads the
    // desktop's name, not the Core.
    void theHostsRefusalNamesWhoHoldsTheSlice()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        startHosting(core);
        QObject namelessSession;
        QObject kindlessSession;
        DeviceSessionRegistry::Entry nameless;
        nameless.deviceId = QByteArrayLiteral("nameless-phone");
        nameless.deviceKind = QStringLiteral("phone");
        QCOMPARE(core.sessions().admit(nameless, &namelessSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        DeviceSessionRegistry::Entry kindless;
        kindless.deviceId = QByteArrayLiteral("kindless-device");
        QCOMPARE(core.sessions().admit(kindless, &kindlessSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        SliceOwnership* ownership = core.model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        const int slice = core.model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(slice >= 0);
        const int stationActive = ownership->activeFor(station);
        // The window takes its notices, as HostingSliceActions does: the
        // station device then shares slices like a remote window.
        core.server->setStationNoticeHandler([](const SessionMessage&) {});

        qint64 id = 700;
        const auto hostRefusal = [&](const QByteArray& verb) {
            SessionMessage answer;
            bool answered = false;
            core.server->invokeAsStationDevice(
                SessionMessages::commandInvoke(verb, ++id, {int64("sliceId", slice)}),
                [&](const SessionMessage& result) {
                    answer = result;
                    answered = true;
                },
                {});
            if (!QTest::qWaitFor([&] { return answered; }) || answer.accepted) {
                return QStringLiteral("answered=%1 accepted=%2")
                    .arg(answered)
                    .arg(answer.accepted);
            }
            return answer.reason;
        };
        const auto belongsTo = [](const QString& holder) {
            return QStringLiteral("That slice belongs to %1. It can be changed only there.")
                .arg(holder);
        };
        const QList<QPair<QByteArray, QString>> holders{
            {QByteArray(), QStringLiteral("the Core")},
            {a.key.fingerprint(), QStringLiteral("iPhone")},
            {nameless.deviceId, QStringLiteral("a phone")},
            {kindless.deviceId, QStringLiteral("another device")},
            {QByteArrayLiteral("token:99"), QStringLiteral("another device")},
        };
        const QString letter = QString(QChar(QLatin1Char('A').unicode() + slice));
        for (const auto& [holder, words] : holders) {
            ownership->setOwner(slice, holder);
            // Not listening: whose slice it is.
            const QString reason = belongsTo(words);
            QVERIFY(OperatorWording::isPlain(reason));
            QCOMPARE(hostRefusal("removeSlice"), reason);
            // Listening: who controls it.
            QVERIFY(ownership->join(station, slice));
            const QString controlled =
                holder.isEmpty()
                    ? QStringLiteral("Nobody controls slice %1. Take control to change it.")
                          .arg(letter)
                    : QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
                          .arg(letter, words);
            QVERIFY(OperatorWording::isPlain(controlled));
            QCOMPARE(hostRefusal("removeSlice"), controlled);
            ownership->leave(station, slice);
            QVERIFY(core.model->sliceById(slice) != nullptr);
            QCOMPARE(ownership->activeFor(station), stationActive);
        }

        // The hosting desktop holds it: a remote window reads its name.
        ownership->setOwner(slice, station);
        const QJsonObject remote = core.invoke(appB, "requestSliceSampleRate",
                                               {int64("sliceId", slice), int64("rateHz", 96000)});
        QVERIFY(!accepted(remote));
        QCOMPARE(reasonOf(remote), belongsTo(kHostName));
    }

    // The window's Select waits for no event loop: the answer is in by the
    // time select() returns, and the station device's active slice moved.
    void aSelectIsAnsweredAtOnce()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.addOnPan(QStringLiteral("pan-0"));
        QCOMPARE(finished.count(), 1);
        QVERIFY2(finished.first().at(2).toBool(), qPrintable(finished.first().at(3).toString()));
        QCOMPARE(ownedBy(core, station).size(), 2);
        const int second = ownedBy(core, station).last();
        core.model->setActiveSliceByIdFor(station, ownedBy(core, station).first());

        host.select(second);
        QCOMPARE(finished.count(), 2);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(core.model->sliceOwnership()->activeFor(station), second);
        QCOMPARE(host.invoking(), false);
    }

    void theHostTakesALiveSliceAndItsControllerIsTold()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        startHosting(core);

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.listen(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        const int noticesBefore = static_cast<int>(ofType(appA->received(), QStringLiteral("notice")).size());
        host.takeControl(0);
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, SliceOwnership::stationDevice());
        // A stays on as a listener and is told who took it.
        QVERIFY(core.model->sliceOwnership()->listenersOf(0).contains(a.key.fingerprint()));
        QTRY_VERIFY(ofType(appA->received(), QStringLiteral("notice")).size() > noticesBefore);
        const QJsonObject told = lastOfType(appA, QStringLiteral("notice"));
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        QVERIFY(told.value(QStringLiteral("reason")).toString().contains(kHostName));
    }

    void theHostListeningIsToldWhenATakeClosesTheSlice()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        startHosting(core);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        holdBothReceivers(core, b);
        QCOMPARE(receiversInUse(*core.model), 2);

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        host.listen(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));

        const quint64 incarnation = core.model->sliceOwnership()->incarnation(0);
        // B takes A's receiver for a new pan: slice 0 closes.
        const int before = static_cast<int>(ofType(appB->received(), QStringLiteral("confirm.request")).size());
        const QJsonObject refused =
            core.invoke(appB, "addSliceOnPan", {utf8("panId", QStringLiteral("b-new-pan"))});
        QVERIFY(!accepted(refused));
        QTRY_VERIFY(ofType(appB->received(), QStringLiteral("confirm.request")).size() > before);
        const QJsonObject ask = lastOfType(appB, QStringLiteral("confirm.request"));
        const qint64 choice = choiceFor(ask, core.model->sliceById(0)->streamIndex());
        QVERIFY(choice >= 0);
        const QJsonObject proceeded = core.invoke(
            appB, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});
        QVERIFY2(accepted(proceeded), qPrintable(reasonOf(proceeded)));
        QTRY_VERIFY(!core.model->sliceOwnership()->matches(SliceOwnership::SliceRef{0, incarnation}));

        // The host, a listener, is told as every listener is (Task 9).
        QTRY_COMPARE(notices.count(), 1);
        const SessionMessage told = notices.first().at(0).value<SessionMessage>();
        QVERIFY(!told.reason.isEmpty());
        QVERIFY(OperatorWording::isPlain(told.reason));
        QCOMPARE(told.prompt.kind, QStringLiteral("sliceClosed"));
        QCOMPARE(told.reason, QStringLiteral("iPad took the receiver slice A was on. You were "
                                             "listening to it."));
    }

    void aRemoteDeviceTakesTheHostsSliceAndTheHostTakesItBack()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, station);
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy refused(&host, &HostingSliceActions::refused);
        QSignalSpy finished(&host, &HostingSliceActions::finished);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QVERIFY(ownership->listenersOf(0).contains(station));

        // The desktop is told, with Take it back, and not as a refusal.
        QTRY_COMPARE(notices.count(), 1);
        const SessionMessage told = notices.first().at(0).value<SessionMessage>();
        QCOMPARE(told.prompt.kind, QStringLiteral("controlTaken"));
        QCOMPARE(told.reason,
                 QStringLiteral("iPad took control of slice A. You are still listening."));
        QVERIFY(OperatorWording::isPlain(told.reason));
        QVERIFY(told.prompt.takeBack);
        QCOMPARE(refused.count(), 0);

        // One tap.
        host.takeBack(told.prompt.id);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(ownership->mark(0).owner, station);
        QVERIFY(ownership->listenersOf(0).contains(b.key.fingerprint()));

        // The iPad is told in turn, with a Take it back of its own.
        QTRY_VERIFY(!ofType(appB->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject toldB = lastOfType(appB, QStringLiteral("notice"));
        QCOMPARE(toldB.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        QCOMPARE(toldB.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("%1 took control of slice A. You are still listening.")
                     .arg(kHostName));
        QCOMPARE(toldB.value(QStringLiteral("takeBack")).toBool(false), true);
    }

    // Take-over fix wave (M-3): the desktop's Take it back refused while
    // the slice transmits may be tried again, so its card stays
    // (takeBackAnswered ended false); once it stops the same tap works and
    // the card goes.
    void theHostsTakeItBackRefusedWhileTransmittingKeepsItsCard()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy refused(&host, &HostingSliceActions::refused);
        QSignalSpy answered(&host, &HostingSliceActions::takeBackAnswered);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBackTx);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        QVERIFY(accepted(core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))))));
        QTRY_COMPARE(notices.count(), 1);
        const qint64 id = notices.first().at(0).value<SessionMessage>().prompt.id;

        MoxController* mox = core.model->moxController();
        QVERIFY(accepted(core.invoke(appB, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", 0)})));
        mox->setMox(true, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        host.takeBack(id);
        QTRY_COMPARE(answered.count(), 1);
        QCOMPARE(answered.last().at(0).toLongLong(), id);
        QCOMPARE(answered.last().at(1).toBool(), false);
        QCOMPARE(refused.last().at(0).toString(),
                 QStringLiteral("Slice A is transmitting. Take control once it stops."));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());

        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        host.takeBack(id);
        QTRY_COMPARE(answered.count(), 2);
        QCOMPARE(answered.last().at(0).toLongLong(), id);
        QCOMPARE(answered.last().at(1).toBool(), true);
        QCOMPARE(ownership->mark(0).owner, station);
    }

    // TX safety (take-over re-review, N-1): the flag was never on the
    // desktop's slice. A phone chose its own slice P and let go of transmit,
    // so the flag stays on P (ruling 8.10). The iPad takes the desktop's
    // only slice. The desktop's MOX must not key the phone's slice.
    void theDesktopsKeyNeverLandsOnAnotherDevicesSlice()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        if (ownership->ownedBy(phone.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appP, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int p = ownership->ownedBy(phone.key.fingerprint()).first();
        QVERIFY(p != 0);
        QVERIFY(accepted(core.invoke(appP, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(phone.key.fingerprint()));
        QVERIFY(accepted(core.invoke(appP, "tx.setTxSlice", {int64("sliceId", p)})));
        QTRY_COMPARE(arbiter->txBoundSliceId(), p);
        core.server->releaseTransmitFor(phone.key.fingerprint(), QStringLiteral("The test let go."));
        QTRY_VERIFY(!core.server->transmitHolder()->holder().has_value());
        QCOMPARE(arbiter->txBoundSliceId(), p);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QTRY_COMPARE(notices.count(), 1);
        QVERIFY(ownedBy(core, station).isEmpty());
        QCOMPARE(arbiter->txBoundSliceId(), p);

        QSignalSpy moxChanges(mox, &MoxController::moxChanged);
        mox->setMox(true);
        QTest::qWait(50);
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(station));
        QCOMPARE(arbiter->txBoundSliceId(), p);
        QCOMPARE(ownership->mark(p).owner, phone.key.fingerprint());
    }

    // TX safety (take-over review, I-2): the desktop's only slice is its
    // idle transmit slice, and a remote device takes it. The desktop's MOX
    // must never key on the slice the iPad now controls (ruling Q8): it is
    // refused, and nothing is keyed or held.
    void theDesktopsKeyNeverLandsOnTheSliceItLost()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QVERIFY(!core.server->transmitHolder()->holder().has_value());
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QTRY_COMPARE(notices.count(), 1);
        QVERIFY(!ownedBy(core, station).contains(arbiter->txBoundSliceId()));

        // The desktop presses MOX.
        QSignalSpy moxChanges(mox, &MoxController::moxChanged);
        mox->setMox(true);
        QTest::qWait(50);
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(station));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    // TX rulings (item 4, test a; take-over re-review N-1 and N-2): a
    // keyer that shares slices, with a slice of its own, finds the flag on
    // another device's slice it never had (not a lost one). Its key is
    // admitted, the flag moves to its own slice, and it keys there.
    void aSharingKeyersKeyMovesTheFlagToItsOwnSliceOnceAdmitted()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        int p = -1;
        phoneLeavesTheFlag(core, phone, appP, &p);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(p >= 0);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBackTx);
        QVERIFY(admitted(appB));
        if (ownership->ownedBy(b.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int own = ownership->ownedBy(b.key.fingerprint()).first();
        QVERIFY(own != p);
        core.model->sliceById(own)->setDspMode(DSPMode::USB);
        core.model->sliceById(own)->setFrequency(14220000.0);
        QCOMPARE(arbiter->txBoundSliceId(), p);

        mox->setMox(true, keyerFor(b));
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), own);
        QVERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QCOMPARE(ownership->mark(p).owner, phone.key.fingerprint());
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // TX rulings (item 4): a key in the TX-to-RX tail. The desktop's
    // footswitch keyed on the phone's slice, its active one (ruling 8.11 on
    // a hosting desktop), and was let go. The desktop's MOX a moment later,
    // before the radio is back in receive, cannot move the flag to the
    // desktop's own slice, so it is refused in words that say the radio is
    // on the air, not "There is no slice to transmit on"; nothing moves or
    // keys. Back in receive the same key moves the flag and keys.
    void aKeyInTheUnkeyTailIsToldTheRadioIsOnTheAir()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        int p = -1;
        phoneLeavesTheFlag(core, phone, appP, &p);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(p >= 0);
        host.listen(p);
        QTRY_COMPARE(finished.count(), 1);
        host.select(p);
        QTRY_COMPARE(finished.count(), 2);
        QCOMPARE(ownership->activeRxFor(station), p);

        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(arbiter->txBoundSliceId(), p);
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(!mox->isMox());
        QVERIFY(mox->state() != MoxState::Rx);
        QVERIFY(core.server->transmitHolder()->isHeldBy(station));

        QSignalSpy moxChanges(mox, &MoxController::moxChanged);
        mox->setMox(true);
        QVERIFY(!mox->isMox());
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kHolderOnAir));
        QCOMPARE(mox->lastRefusal().text, TxRefusals::radioOnAir().text);
        QCOMPARE(arbiter->txBoundSliceId(), p);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(!mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), p);

        mox->setMox(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // TX rulings (item 4, test b; take-over re-review N-3): closing a
    // hosting card forgets its Take it back, so a later answer to it is
    // never read as that card's.
    void closingAHostingCardForgetsItsTakeBack()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        QSignalSpy answered(&host, &HostingSliceActions::takeBackAnswered);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        QVERIFY(accepted(core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))))));
        QTRY_COMPARE(notices.count(), 1);
        const qint64 id = notices.first().at(0).value<SessionMessage>().prompt.id;

        host.forgetNotice(id);
        host.takeBack(id);
        QTRY_COMPARE(finished.count(), 1);
        QTest::qWait(20);
        QCOMPARE(answered.count(), 0);
    }

    // TX rulings (JJ, 2026-09-30, ruling 8.11 on a hosting desktop): the
    // desktop lost its only slice, where the flag stayed, and listens to the
    // phone's slice as its active slice. Its MOX keeps the take-over rule
    // and is refused; its footswitch or mic PTT moves the flag to its active
    // slice, the phone's, and keys there.
    void theDesktopsFootswitchKeysItsActiveSliceWhereverItIs()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownedBy(core, station), QList<int>{0});
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy finished(&host, &HostingSliceActions::finished);

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        if (ownership->ownedBy(phone.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appP, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int p = ownership->ownedBy(phone.key.fingerprint()).first();
        core.model->sliceById(p)->setDspMode(DSPMode::USB);
        core.model->sliceById(p)->setFrequency(14210000.0);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        QVERIFY(accepted(core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))))));
        QTRY_COMPARE(notices.count(), 1);
        QVERIFY(ownedBy(core, station).isEmpty());
        QCOMPARE(arbiter->txBoundSliceId(), 0);

        host.listen(p);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        host.select(p);
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(ownership->activeRxFor(station), p);
        QCOMPARE(ownership->mark(p).owner, phone.key.fingerprint());

        // The desktop's MOX (a Device-source key) keeps the refusal.
        mox->setMox(true);
        QTest::qWait(20);
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QCOMPARE(arbiter->txBoundSliceId(), 0);

        // The footswitch moves the flag to the desktop's active slice.
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().source, PttMode::Mic);
        QCOMPARE(arbiter->txBoundSliceId(), p);
        QVERIFY(core.server->transmitHolder()->isHeldBy(station));
        QCOMPARE(ownership->mark(p).owner, phone.key.fingerprint());
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(arbiter->txBoundSliceId(), p);
    }

    // TX rulings review: the radio's own PTT on a hosting desktop is
    // refused rather than keyed elsewhere. With no active slice it has no
    // slice to transmit on; while the flag is frozen (or the radio still
    // coming back to receive) the radio is on the air.
    void theDesktopsFootswitchIsRefusedWhenItCannotMoveTheFlag()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy finished(&host, &HostingSliceActions::finished);

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        if (ownership->ownedBy(phone.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appP, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int p = ownership->ownedBy(phone.key.fingerprint()).first();
        core.model->sliceById(p)->setDspMode(DSPMode::USB);
        core.model->sliceById(p)->setFrequency(14210000.0);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        QVERIFY(accepted(core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))))));
        QTRY_COMPARE(notices.count(), 1);
        QVERIFY(ownedBy(core, station).isEmpty());
        QCOMPARE(arbiter->txBoundSliceId(), 0);

        // The desktop leaves slice A: it has no slice at all.
        host.stopListening(0);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(ownership->activeRxFor(station), -1);
        mox->onMicPttFromRadio(true);
        QTest::qWait(20);
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        mox->onMicPttFromRadio(false);

        // It listens to the phone's slice and selects it; the flag frozen.
        host.listen(p);
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        host.select(p);
        QTRY_COMPARE(finished.count(), 3);
        QCOMPARE(ownership->activeRxFor(station), p);
        bool frozen = true;
        arbiter->setFrozen([&frozen] { return frozen; });
        mox->onMicPttFromRadio(true);
        QTest::qWait(20);
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kHolderOnAir));
        QCOMPARE(mox->lastRefusal().text, TxRefusals::radioOnAir().text);
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        mox->onMicPttFromRadio(false);

        // Thawed, the same footswitch moves the flag and keys.
        frozen = false;
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), p);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Ruling 8.11 on a hosting desktop (JJ, 2026-09-30): the flag on one of
    // the desktop's own slices that is not its active one (split transmit)
    // is its chosen transmit slice. The footswitch keys that slice and the
    // flag stays.
    void theDesktopsFootswitchKeysItsOwnSplitTransmitSlice()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.addOnPan(QStringLiteral("pan-0"));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().at(2).toBool(), qPrintable(finished.first().at(3).toString()));
        QCOMPARE(ownedBy(core, station).size(), 2);
        const int first = ownedBy(core, station).first();
        const int second = ownedBy(core, station).last();
        core.model->sliceById(second)->setDspMode(DSPMode::USB);
        core.model->sliceById(second)->setFrequency(14210000.0);
        host.select(first);
        QTRY_COMPARE(finished.count(), 2);
        QCOMPARE(ownership->activeRxFor(station), first);
        QVERIFY(arbiter->requestHandoff(second, station));
        QCOMPARE(arbiter->txBoundSliceId(), second);

        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), second);
        QCOMPARE(ownership->activeRxFor(station), first);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(arbiter->txBoundSliceId(), second);
    }

    // Ruling 8.11 on a Core with no desktop is as it was: the radio's own
    // PTT transmits where the flag is, a phone's slice included.
    void withNoDesktopTheFootswitchKeysWhereTheFlagIs()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();

        Device phone(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        LoopbackTransport* appP = core.signIn(phone, kSharesTx);
        QVERIFY(admitted(appP));
        int p = -1;
        phoneLeavesTheFlag(core, phone, appP, &p);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(p >= 0);

        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), p);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Core-slice take-over: a peer below sliceAccessVersion 3 (here 2) is
    // refused in today's words.
    void withNobodyAtTheDesktopTheCoresSliceStaysItsOwn()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, SliceOwnership::stationDevice());
        // No HostingSliceActions: nobody takes the station device's notices.
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refOf(core, 0))));
        const QJsonObject r = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Slice A is run by the Core itself, so control of "
                                             "it cannot pass to this device."));
        QCOMPARE(ownership->mark(0).owner, SliceOwnership::stationDevice());
    }

    // Core-slice take-over (JJ, 2026-09-30): on a headless Core (no
    // HostingSliceActions, so nobody takes the station device's notices)
    // a device at sliceAccessVersion 3 takes the Core's own slice at once,
    // with no question, and is sent 3.
    void withNobodyAtTheDesktopADeviceTakesTheCoresSlice()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, station);
        QVERIFY(!ownership->mark(0).isHeld());

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesCore);
        QVERIFY(admitted(appB));
        QCOMPARE(capability(appB->received(), QStringLiteral("sliceAccessVersion")), 3);

        const quint64 before = ownership->controlRevision(0);
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0) << int64("controlRevision", static_cast<qint64>(before)));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QVERIFY(ofType(appB->received(), QStringLiteral("confirm.request")).isEmpty());
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QVERIFY(ownership->mark(0).owner != station);
        QCOMPARE(ownership->controlRevision(0), before + 1);
        QVERIFY(ownership->listenersOf(0).contains(b.key.fingerprint()));
        // The station device stays joined, as any former controller does.
        QVERIFY(ownership->listenersOf(0).contains(station));
        QVERIFY(ownership->ownedBy(station).isEmpty());
        // The iPad's own form of the slice now says it controls it.
        QTRY_VERIFY(SliceAccessPolicy::mayChange(*ownership, b.key.fingerprint(), 0));
    }

    // Core-slice take-over: the Core's own slice on the air is refused in
    // the take's words, as any slice on the air is.
    void withNobodyAtTheDesktopATakeOnTheAirIsRefused()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        allowTransmit(core);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, station);
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QVERIFY(arbiter->requestHandoff(0, station));
        QCOMPARE(arbiter->txBoundSliceId(), 0);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesCoreTx);
        QVERIFY(admitted(appB));

        // The radio's own PTT keys the slice the flag is on (ruling 8.11).
        MoxController* mox = core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        const QJsonObject refused = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY(!accepted(refused));
        QCOMPARE(reasonOf(refused),
                 QStringLiteral("Slice A is transmitting. Take control once it stops."));
        QCOMPARE(ownership->mark(0).owner, station);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);

        // Once it stops, the same take goes through.
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    // Core-slice take-over: with someone at the desktop (it takes the
    // station device's notices), a device at 3 takes the desktop's slice
    // by today's take-over rules: the desktop is told with Take it back,
    // and its one tap takes it back.
    void withSomeoneAtTheDesktopTheTakeFollowsTheTakeOverRules()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        startHosting(core);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceOwnership* ownership = core.model->sliceOwnership();
        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy notices(&host, &HostingSliceActions::notice);
        QSignalSpy finished(&host, &HostingSliceActions::finished);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesCore);
        QVERIFY(admitted(appB));
        QCOMPARE(capability(appB->received(), QStringLiteral("sliceAccessVersion")), 3);
        const QJsonObject taken = core.invoke(
            appB, "slice.takeControl",
            refOf(core, 0)
                << int64("controlRevision", static_cast<qint64>(ownership->controlRevision(0))));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QVERIFY(ownership->listenersOf(0).contains(station));

        QTRY_COMPARE(notices.count(), 1);
        const SessionMessage told = notices.first().at(0).value<SessionMessage>();
        QCOMPARE(told.prompt.kind, QStringLiteral("controlTaken"));
        QCOMPARE(told.reason,
                 QStringLiteral("iPad took control of slice A. You are still listening."));
        QVERIFY(told.prompt.takeBack);

        host.takeBack(told.prompt.id);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        QCOMPARE(ownership->mark(0).owner, station);
    }

    void aVerbTheHostDoesNotRunIsRefusedAsUnknown()
    {
        Core core;
        startHosting(core);
        SessionMessage answer;
        bool answered = false;
        core.server->invokeAsStationDevice(
            SessionMessages::commandInvoke("tx.key", 3, {}),
            [&](const SessionMessage& result) {
                answer = result;
                answered = true;
            },
            {});
        QVERIFY(answered);
        QVERIFY(!answer.accepted);
        QCOMPARE(answer.reason, kUnknownVerb);
    }

    void withoutAServerNothingRuns()
    {
        HostingSliceActions host(nullptr, nullptr);
        QSignalSpy refused(&host, &HostingSliceActions::refused);
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.select(0);
        QCOMPARE(refused.count(), 1);
        QVERIFY(OperatorWording::isPlain(refused.first().at(0).toString()));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(2).toBool(), false);
    }
};

QTEST_MAIN(TstHostingSliceOperations)
#include "tst_hosting_slice_operations.moc"
