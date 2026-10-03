// =================================================================
// tests/tst_slice_access_client.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Slice control and shared listening
// plan Task 5: the remote window's side of listening to and taking a slice.
//
// A real StationClient (the remote window's), signed in with its own key to
// a real StationServer over a loopback link, beside a second device played
// by the test on the same Core. Checks the window's copy of the Core's
// `access:<id>` objects (SliceAccessMirror), its four verbs, the read-only
// mark on a slice it only listens to (a change is held back with the Core's
// listener words and nothing is sent), and that MultiDeviceController shows
// a refusal and a held change as one refusal each, and a controlTaken
// notice as a card whose Take it back returns control (take-over parity),
// shown off with a reason on a Core that cannot run it.
// Nothing keys a radio: the Core's model has no radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control and shared listening
//                                    plan Task 5. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Task 14b: a listened slice's "Your
//                                    volume" (slice.setListenLevel) sets
//                                    this device's level only. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Task 17: the window's copy of the
//                                    listener words names the controller
//                                    as the Core does. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Take-over parity: controlTaken is a
//               card with Take it back, and an older Core's is shown off
//               with the reason. AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Take-over fix wave (M-2, M-3): the
//               card stays when Take it back may be tried again and goes
//               when it worked or never can; an ended take-back's card.
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: the window reads sliceAccessVersion
//               3 and learns whether the Core's own slice may be taken.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: desktop listening lane: Take it back of a slice taken
//               again answers "That can no longer be taken back." J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QWidget>

#include "core/security/ClientDeviceIdentity.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "gui/multidevice/MultiDeviceController.h"
#include "gui/multidevice/NoticeCard.h"

namespace {

const QHash<QByteArray, int> kShares{{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}};
// Take-over parity: a device at sliceAccess 2 (Take it back on
// controlTaken).
const QHash<QByteArray, int> kSharesBack{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}};
// Take-over fix wave (M-3): the same, with remote transmit.
const QHash<QByteArray, int> kSharesBackTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}, {"remoteTx", 1}};

// The window: this computer's own key, a remote model and its client.
struct Window {
    QTemporaryDir keyDir;
    std::shared_ptr<const ClientDeviceIdentity> key = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(keyDir.path()));
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};
    QWidget host;
    /// The Core's end of the link: what the window sent arrives here.
    LoopbackTransport* coreEnd = nullptr;

    Window()
    {
        client.setDeviceIdentity(key, QStringLiteral("Shack MacBook"), QStringLiteral("MacBook"));
        host.resize(900, 500);
    }

    PairedDevice record() const
    {
        PairedDevice device;
        device.id = key->fingerprint();
        device.publicKeySpki = key->publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        return device;
    }

    QString id() const { return StationIdentity::toBase64Url(key->fingerprint()); }

    bool connectTo(Core& core)
    {
        coreEnd = new LoopbackTransport(QStringLiteral("station"));
        auto* peer = new LoopbackTransport(QStringLiteral("window"));
        coreEnd->setPeerAddress(QStringLiteral("192.0.2.30"));
        peer->setPeerCertificateSha256(core.certSha256());
        coreEnd->linkTo(peer);
        client.startSession(peer, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        core.server->acceptTransport(coreEnd);
        return QTest::qWaitFor([this]() { return client.stationLinkReady(); }, 5000);
    }

    SliceAccessMirror& access() { return *client.sliceAccess(); }

    /// Every command the window sent with a verb starting `prefix`.
    QStringList sentVerbs(const QString& prefix) const
    {
        QStringList verbs;
        for (const QJsonObject& o : ofType(coreEnd->received(), QStringLiteral("command.invoke"))) {
            const QString verb = o.value(QStringLiteral("verb")).toString();
            if (verb.startsWith(prefix)) {
                verbs.append(verb);
            }
        }
        return verbs;
    }

    /// Property writes the window sent for `key`.
    int writesFor(const QString& key) const
    {
        int count = 0;
        for (const QJsonObject& o : ofType(coreEnd->received(), QStringLiteral("property.write"))) {
            if (o.value(QStringLiteral("key")).toString() == key) {
                ++count;
            }
        }
        return count;
    }
};

QJsonValue accessOf(const LoopbackTransport* app, int sliceId, const char* property)
{
    return latest(app->received(), QStringLiteral("access:%1").arg(sliceId),
                  QString::fromLatin1(property));
}

QList<MirrorUpdate> refArgs(const LoopbackTransport* app, int sliceId)
{
    return {int64("sliceId", sliceId), int64("incarnation", accessOf(app, sliceId, "incarnation").toInteger())};
}

QList<MirrorUpdate> revisionArgs(const LoopbackTransport* app, int sliceId)
{
    return {int64("sliceId", sliceId), int64("incarnation", accessOf(app, sliceId, "incarnation").toInteger()),
            int64("controlRevision", accessOf(app, sliceId, "controlRevision").toInteger())};
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

// Every answer the window's several-devices verbs got, by command id,
// collected from before the first is sent.
struct Answer {
    bool accepted = false;
    QString reason;
};

struct Answers {
    QHash<quint32, Answer> byId;
    QMetaObject::Connection connection;

    explicit Answers(StationClient& client)
    {
        connection = QObject::connect(
            &client, &StationClient::deviceCommandFinished,
            [this](const QByteArray&, quint32 id, bool ok, const QString& reason, bool) {
                byId.insert(id, Answer{ok, reason});
            });
    }
    ~Answers() { QObject::disconnect(connection); }
    Answers(const Answers&) = delete;
    Answers& operator=(const Answers&) = delete;

    Answer waitFor(quint32 commandId)
    {
        const bool arrived =
            QTest::qWaitFor([this, commandId]() { return byId.contains(commandId); }, 5000);
        Q_UNUSED(arrived);
        return byId.value(commandId, Answer{false, QStringLiteral("no answer")});
    }
};

// The first entry the mirror held for `sliceId` after it was armed: what
// the first access message about that slice carried.
struct FirstChange {
    std::optional<SliceAccessMirror::Entry> entry;
    int count = 0;
    QMetaObject::Connection connection;

    FirstChange(SliceAccessMirror& mirror, int sliceId)
    {
        SliceAccessMirror* m = &mirror;
        connection = QObject::connect(m, &SliceAccessMirror::changed, [this, m, sliceId](int id) {
            if (id != sliceId) {
                return;
            }
            if (count++ == 0) {
                entry = m->entry(sliceId);
            }
        });
    }
    ~FirstChange() { QObject::disconnect(connection); }
    FirstChange(const FirstChange&) = delete;
    FirstChange& operator=(const FirstChange&) = delete;
};

// The Core's listener words for a slice another device controls.
QString controlledBy(const QString& letter, const QString& name)
{
    return QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
        .arg(letter, name);
}

} // namespace

class TstSliceAccessClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const QString profile =
            QStringLiteral("slice-access-client-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    // The window holds the Core's access objects: its own slice controlled
    // here, the other device's slice neither controlled nor listened to.
    // Listening makes that slice a read-only slice of the window's: its
    // frequency setter changes nothing, sends nothing, and says who
    // controls it in the Core's words, once. Take control, and the same
    // setter sends. Release and stop listening leave the slice. Each change
    // is in the mirror by the access delta that follows the answer.
    void listenTakeReleaseAndStopListeningRoundTrip()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        const QList<int> bSlices = core.model->sliceOwnership()->ownedBy(b.key.fingerprint());
        QCOMPARE(bSlices.size(), 1);
        const int shared = bSlices.first();

        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QVERIFY(w.client.remoteSliceAccessAvailable());
        Answers answers(w.client);
        MultiDeviceController controller(&w.client, &w.host);
        QSignalSpy refusals(&controller, &MultiDeviceController::refusal);

        const QList<int> wSlices = core.model->sliceOwnership()->ownedBy(w.key->fingerprint());
        QCOMPARE(wSlices.size(), 1);
        const int own = wSlices.first();
        QTRY_VERIFY(w.access().entry(own).has_value());
        QTRY_VERIFY(w.access().entry(shared).has_value());
        QVERIFY(w.access().controlledHere(own));
        QVERIFY(w.access().listeningHere(own));
        QVERIFY(!w.access().controlledHere(shared));
        QVERIFY(!w.access().listeningHere(shared));
        const SliceAccessMirror::Entry sharedEntry = *w.access().entry(shared);
        QCOMPARE(sharedEntry.controllerDeviceId, b.id());
        QCOMPARE(sharedEntry.listeners, QStringList{b.id()});
        QCOMPARE(sharedEntry.incarnation, core.model->sliceOwnership()->incarnation(shared));
        QCOMPARE(sharedEntry.controlRevision, core.model->sliceOwnership()->controlRevision(shared));
        QVERIFY(w.remote.sliceById(shared) == nullptr);
        QVERIFY(!w.remote.sliceById(own)->isReadOnlyListener());

        // ── Listen ──
        std::optional<FirstChange> first;
        first.emplace(w.access(), shared);
        IStationLink::CommandOutcome sent =
            w.client.requestListen(shared, sharedEntry.incarnation);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        Answer answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QTRY_VERIFY(w.access().listeningHere(shared));
        // The first access message about the slice carried the join.
        QVERIFY(first->entry.has_value());
        QCOMPARE(first->entry->listeners, (QStringList{b.id(), w.id()}));
        QCOMPARE(w.access().entry(shared)->listeners, (QStringList{b.id(), w.id()}));
        QVERIFY(!w.access().controlledHere(shared));
        QTRY_VERIFY(w.remote.sliceById(shared) != nullptr);
        SliceModel* listened = w.remote.sliceById(shared);
        QVERIFY(listened->isReadOnlyListener());
        const QString letter = QString(QChar(QLatin1Char('A').unicode() + shared));
        QCOMPARE(listened->readOnlyListenerReason(), controlledBy(letter, QStringLiteral("iPad")));

        // A change on the listened slice is held back: nothing changes here,
        // nothing is sent, and the listener words are shown once.
        const double coreFrequency = core.model->sliceById(shared)->frequency();
        const double before = listened->frequency();
        QSignalSpy held(&w.client, &StationClient::sliceAccessHeld);
        QSignalSpy frequencyChanged(listened, &SliceModel::frequencyChanged);
        listened->setFrequency(before + 5000.0);
        QCOMPARE(listened->frequency(), before);
        QCOMPARE(frequencyChanged.count(), 0);
        QCOMPARE(held.count(), 1);
        QCOMPARE(held.first().at(1).toString(), controlledBy(letter, QStringLiteral("iPad")));
        QCOMPARE(refusals.count(), 1);
        QCOMPARE(refusals.first().first().toString(), controlledBy(letter, QStringLiteral("iPad")));
        // The same value is no change and holds nothing.
        listened->setFrequency(before);
        QCOMPARE(held.count(), 1);
        // A slice request (close, band) is held too, not sent.
        w.remote.removeSlice(shared);
        QCOMPARE(held.count(), 2);
        w.remote.onBandButtonClicked(listened, Band::Band40m);
        QCOMPARE(held.count(), 3);
        // Each held change is shown; the window's toast shows a repeat once.
        QCOMPARE(refusals.count(), 3);
        // The pan that shows it is this window's layout: moved here, never
        // held, never sent.
        const QString corePan = core.model->sliceById(shared)->panKey();
        listened->setPanKey(QStringLiteral("pan-7"));
        QCOMPARE(listened->panKey(), QStringLiteral("pan-7"));
        QCOMPARE(held.count(), 3);
        QTest::qWait(100);
        QCOMPARE(core.model->sliceById(shared)->panKey(), corePan);
        QCOMPARE(w.writesFor(QStringLiteral("slice:%1").arg(shared)), 0);
        QVERIFY(w.sentVerbs(QStringLiteral("removeSlice")).isEmpty());
        QVERIFY(w.sentVerbs(QStringLiteral("slice.selectBand")).isEmpty());
        QCOMPARE(listened->frequency(), before);
        QCOMPARE(core.model->sliceById(shared)->frequency(), coreFrequency);
        // The controller's change still reaches the listener.
        const qint64 bWrite = 9101;
        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QStringLiteral("slice:%1").arg(shared).toUtf8(),
            {f64("frequency", coreFrequency + 1000.0)}, bWrite)));
        QTRY_COMPARE(listened->frequency(), coreFrequency + 1000.0);
        QVERIFY(listened->isReadOnlyListener());

        // ── Take control ──
        const SliceAccessMirror::Entry beforeTake = *w.access().entry(shared);
        first.emplace(w.access(), shared);
        sent = w.client.requestTakeControl(shared, beforeTake.incarnation,
                                           beforeTake.controlRevision);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QTRY_VERIFY(w.access().controlledHere(shared));
        QVERIFY(first->entry.has_value());
        QCOMPARE(first->entry->controllerDeviceId, w.id());
        QCOMPARE(first->entry->controlRevision, beforeTake.controlRevision + 1);
        QCOMPARE(w.access().entry(shared)->controlRevision, beforeTake.controlRevision + 1);
        QVERIFY(!listened->isReadOnlyListener());
        QVERIFY(listened->readOnlyListenerReason().isEmpty());
        // B, still listening, was told; that is B's notice, not this window's.
        QTRY_COMPARE(ofType(appB->received(), QStringLiteral("notice")).size(), 1);
        QCOMPARE(refusals.count(), 3);
        // Now the setter sends.
        const double tuned = listened->frequency() + 2000.0;
        listened->setFrequency(tuned);
        QCOMPARE(held.count(), 3);
        QTRY_COMPARE(core.model->sliceById(shared)->frequency(), tuned);
        QVERIFY(w.writesFor(QStringLiteral("slice:%1").arg(shared)) >= 1);

        // ── Release (B still listens: the slice stays for B) ──
        const SliceAccessMirror::Entry beforeRelease = *w.access().entry(shared);
        first.emplace(w.access(), shared);
        sent = w.client.requestRelease(shared, beforeRelease.incarnation,
                                       beforeRelease.controlRevision);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QTRY_VERIFY(!w.access().listeningHere(shared));
        QVERIFY(first->entry.has_value());
        QVERIFY(first->entry->controllerDeviceId.isEmpty());
        QCOMPARE(first->entry->listeners, QStringList{b.id()});
        QVERIFY(w.access().entry(shared)->controllerDeviceId.isEmpty());
        QCOMPARE(w.access().entry(shared)->listeners, QStringList{b.id()});
        QTRY_VERIFY(w.remote.sliceById(shared) == nullptr);
        QVERIFY(core.model->sliceById(shared) != nullptr);

        // ── Listen again, then stop listening ──
        sent = w.client.requestListen(shared, w.access().entry(shared)->incarnation);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        QVERIFY(answers.waitFor(sent.commandId).accepted);
        QTRY_VERIFY(w.access().listeningHere(shared));
        QTRY_VERIFY(w.remote.sliceById(shared) != nullptr);
        // Nobody controls it now; the words say so.
        QVERIFY(w.remote.sliceById(shared)->isReadOnlyListener());
        QCOMPARE(w.remote.sliceById(shared)->readOnlyListenerReason(),
                 QStringLiteral("Nobody controls slice %1. Take control to change it.").arg(letter));
        first.emplace(w.access(), shared);
        sent = w.client.requestStopListening(shared, w.access().entry(shared)->incarnation);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QTRY_VERIFY(!w.access().listeningHere(shared));
        QVERIFY(first->entry.has_value());
        QCOMPARE(first->entry->listeners, QStringList{b.id()});
        QTRY_VERIFY(w.remote.sliceById(shared) == nullptr);

        // The window's own slice was never marked.
        QVERIFY(!w.remote.sliceById(own)->isReadOnlyListener());
    }

    // Task 14b: a listened flag's "Your volume" sends slice.setListenLevel.
    // The Core sets this device's own level and mute for the slice; the
    // controller's level, the slice's AF and mute, and every other
    // listener's level are unchanged.
    void yourVolumeSetsOnlyThisDevicesLevel()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        const int shared = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();

        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        Answers answers(w.client);
        QTRY_VERIFY(w.access().entry(shared).has_value());
        IStationLink::CommandOutcome sent =
            w.client.requestListen(shared, w.access().entry(shared)->incarnation);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        QVERIFY(answers.waitFor(sent.commandId).accepted);
        QTRY_VERIFY(w.access().listeningHere(shared));

        SliceModel* coreSlice = core.model->sliceById(shared);
        const int afBefore = coreSlice->afGain();
        const bool mutedBefore = coreSlice->muted();
        SliceAccessController* access = core.server->sliceAccessController();
        QVERIFY(access != nullptr);
        const SliceAccessController::ListenLevel bBefore =
            access->listenLevel(b.key.fingerprint(), shared);
        QSignalSpy af(coreSlice, &SliceModel::afGainChanged);
        QSignalSpy mute(coreSlice, &SliceModel::mutedChanged);

        sent = w.client.requestListenLevel(shared, w.access().entry(shared)->incarnation,
                                           0.3, true);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        const Answer answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(w.sentVerbs(QStringLiteral("slice.setListenLevel")).size(), 1);

        const SliceAccessController::ListenLevel mine =
            access->listenLevel(w.key->fingerprint(), shared);
        QVERIFY(qAbs(mine.level - 0.3) < 1e-9);
        QVERIFY(mine.muted);
        const SliceAccessController::ListenLevel bAfter =
            access->listenLevel(b.key.fingerprint(), shared);
        QCOMPARE(bAfter.level, bBefore.level);
        QCOMPARE(bAfter.muted, bBefore.muted);
        QCOMPARE(coreSlice->afGain(), afBefore);
        QCOMPARE(coreSlice->muted(), mutedBefore);
        QCOMPARE(af.count(), 0);
        QCOMPARE(mute.count(), 0);
    }

    // A refused verb is the Core's refusal, shown once through the
    // window's refusal path: a take with a control revision that changed.
    // Slice control plan Task 17: the window's copy of the listener words
    // names the controller as the Core's own refusal does: the device's
    // name, the plain word for its kind when it has none, "another device"
    // when neither is known, the Core only for the station device on a
    // Core no desktop hosts. Each matches the Core's refusal of the same
    // change.
    void theHeldChangeNamesTheControllerAsTheCoreDoes()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        const int shared = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
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

        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        Answers answers(w.client);
        QTRY_VERIFY(w.access().entry(shared).has_value());
        const IStationLink::CommandOutcome sent =
            w.client.requestListen(shared, w.access().entry(shared)->incarnation);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        const Answer answer = answers.waitFor(sent.commandId);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QTRY_VERIFY(w.remote.sliceById(shared) != nullptr);
        SliceModel* listened = w.remote.sliceById(shared);
        const QString letter = QString(QChar(QLatin1Char('A').unicode() + shared));
        QCOMPARE(listened->readOnlyListenerReason(), controlledBy(letter, QStringLiteral("iPad")));

        SliceOwnership* ownership = core.model->sliceOwnership();
        const QList<QPair<QByteArray, QString>> controllers{
            {nameless.deviceId, QStringLiteral("a phone")},
            {kindless.deviceId, QStringLiteral("another device")},
            {QByteArrayLiteral("token:99"), QStringLiteral("another device")},
            {SliceOwnership::stationDevice(), QStringLiteral("the Core")},
        };
        for (const auto& [controller, words] : controllers) {
            ownership->setOwner(shared, controller);
            QVERIFY(ownership->join(w.key->fingerprint(), shared));
            const QString expected = controlledBy(letter, words);
            QVERIFY(OperatorWording::isPlain(expected));
            QTRY_COMPARE(listened->readOnlyListenerReason(), expected);
            QCOMPARE(w.access().listenerReason(shared), expected);
        }
    }

    void aRefusedVerbIsShownOnce()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        const int shared = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        Answers answers(w.client);
        MultiDeviceController controller(&w.client, &w.host);
        QSignalSpy refusals(&controller, &MultiDeviceController::refusal);
        QTRY_VERIFY(w.access().entry(shared).has_value());
        const SliceAccessMirror::Entry e = *w.access().entry(shared);
        const IStationLink::CommandOutcome sent =
            w.client.requestTakeControl(shared, e.incarnation, e.controlRevision + 7);
        QVERIFY(sent.sent);
        const Answer answer = answers.waitFor(sent.commandId);
        QVERIFY(!answer.accepted);
        QVERIFY(!answer.reason.isEmpty());
        QTRY_COMPARE(refusals.count(), 1);
        QCOMPARE(refusals.first().first().toString(), answer.reason);
        QCOMPARE(core.model->sliceOwnership()->mark(shared).owner, b.key.fingerprint());
    }

    // Another device takes control of a slice this window controlled: the
    // window keeps listening, the slice is read-only here, and a card says
    // so with Take it back (take-over parity). One tap returns control.
    void controlTakenIsACardWhoseTakeItBackReturnsControl()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QVERIFY(w.client.controlTakeBackAvailable());
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        QSignalSpy refusals(&controller, &MultiDeviceController::refusal);
        const int own = core.model->sliceOwnership()->ownedBy(w.key->fingerprint()).first();
        QTRY_VERIFY(w.access().controlledHere(own));
        SliceModel* slice = w.remote.sliceById(own);
        QVERIFY(slice != nullptr);
        QVERIFY(!slice->isReadOnlyListener());

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, QStringLiteral("access:%1").arg(own)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(appB, own))));
        QTRY_VERIFY(holds(appB, QStringLiteral("slice:%1").arg(own)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(appB, own))));

        const QString letter = QString(QChar(QLatin1Char('A').unicode() + own));
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        NoticeCard* card = controller.noticeCards().first();
        QVERIFY(card->textLabel()->text().endsWith(
            QStringLiteral("iPad took control of slice %1. You are still listening.").arg(letter)));
        QVERIFY(card->takeBackButton() != nullptr);
        QVERIFY(card->takeBackButton()->isEnabled());
        QCOMPARE(refusals.count(), 0);
        QTRY_VERIFY(!w.access().controlledHere(own));
        QVERIFY(w.access().listeningHere(own));
        QCOMPARE(w.remote.sliceById(own), slice);
        QVERIFY(slice->isReadOnlyListener());
        QCOMPARE(slice->readOnlyListenerReason(), controlledBy(letter, QStringLiteral("iPad")));

        // One tap sends notice.takeBack; control comes back here, and the
        // card goes (take-over fix wave, M-3).
        card->takeBackButton()->click();
        QTRY_COMPARE(core.model->sliceOwnership()->mark(own).owner, w.key->fingerprint());
        QVERIFY(w.sentVerbs(QStringLiteral("notice.takeBack")).size() == 1);
        QTRY_COMPARE(controller.noticeCards().size(), 0);
        QTRY_VERIFY(w.access().controlledHere(own));
        QTRY_VERIFY(!slice->isReadOnlyListener());
        QCOMPARE(refusals.count(), 0);
        QVERIFY(core.model->sliceOwnership()->listenersOf(own).contains(b.key.fingerprint()));
    }

    // Take-over fix wave (M-3): Take it back refused while the slice
    // transmits may be tried again, so the card stays; once it stops, the
    // same tap works and the card goes. A refusal that ends the take-back
    // (control moved on) takes the card away.
    void aTakeItBackRefusedWhileTheSliceTransmitsKeepsTheCard()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        allowTransmit(core);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        QSignalSpy refusals(&controller, &MultiDeviceController::refusal);
        const int own = core.model->sliceOwnership()->ownedBy(w.key->fingerprint()).first();
        QTRY_VERIFY(w.access().controlledHere(own));
        core.model->sliceById(own)->setDspMode(DSPMode::USB);
        core.model->sliceById(own)->setFrequency(14200000.0);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBackTx);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, QStringLiteral("access:%1").arg(own)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(appB, own))));
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        NoticeCard* card = controller.noticeCards().first();

        // B chooses the slice for transmit and keys on it.
        MoxController* mox = core.model->moxController();
        QVERIFY(accepted(core.invoke(appB, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", own)})));
        mox->setMox(true, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        const QString letter = QString(QChar(QLatin1Char('A').unicode() + own));
        const QString transmitting =
            QStringLiteral("Slice %1 is transmitting. Take control once it stops.").arg(letter);
        card->takeBackButton()->click();
        QTRY_COMPARE(refusals.count(), 1);
        QCOMPARE(refusals.first().at(0).toString(), transmitting);
        QCOMPARE(controller.noticeCards().size(), 1);
        QCOMPARE(controller.noticeCards().first(), card);
        QVERIFY(card->takeBackButton()->isEnabled());
        QCOMPARE(core.model->sliceOwnership()->mark(own).owner, b.key.fingerprint());

        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        card->takeBackButton()->click();
        QTRY_COMPARE(core.model->sliceOwnership()->mark(own).owner, w.key->fingerprint());
        QTRY_COMPARE(controller.noticeCards().size(), 0);
        QCOMPARE(refusals.count(), 1);
    }

    void aTakeItBackRefusedAfterControlMovedOnTakesTheCardAway()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        QSignalSpy refusals(&controller, &MultiDeviceController::refusal);
        const int own = core.model->sliceOwnership()->ownedBy(w.key->fingerprint()).first();
        QTRY_VERIFY(w.access().controlledHere(own));

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        Device c(QStringLiteral("Mac"), QStringLiteral("computer"), QStringLiteral("Mac"));
        core.pair(b);
        core.pair(c);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        LoopbackTransport* appC = core.signIn(c, kSharesBack);
        QVERIFY(admitted(appB) && admitted(appC));
        QTRY_VERIFY(holds(appB, QStringLiteral("access:%1").arg(own)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(appB, own))));
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        NoticeCard* card = controller.noticeCards().first();
        // C takes it from B before the window taps.
        QTRY_VERIFY(holds(appC, QStringLiteral("access:%1").arg(own)));
        QTRY_COMPARE(accessOf(appC, own, "controllerDeviceId").toString(), b.id());
        QVERIFY(accepted(core.invoke(appC, "slice.takeControl", revisionArgs(appC, own))));
        QTRY_COMPARE(w.access().entry(own)->controllerDeviceId, c.id());

        card->takeBackButton()->click();
        QTRY_COMPARE(refusals.count(), 1);
        // Desktop listening lane (JJ, 2026-09-30): C took the slice again,
        // so the take-back record is void and the first tap says so (it
        // used to answer with slice.takeControl's stale-revision words).
        QCOMPARE(refusals.first().at(0).toString(),
                 QStringLiteral("That can no longer be taken back."));
        QTRY_COMPARE(controller.noticeCards().size(), 0);
        QCOMPARE(core.model->sliceOwnership()->mark(own).owner, c.key.fingerprint());
    }

    // The enabled card's other off state: a Core at 2 that sends takeBack
    // false (the take-back ended while this window was away) shows Take it
    // back off with "That can no longer be taken back.", in the style
    // guide's disabled look.
    void aTakeBackThatEndedIsShownOffWithTheReason()
    {
        RemotePrompt notice;
        notice.prompt.id = 7;
        notice.prompt.kind = QStringLiteral("controlTaken");
        notice.prompt.takeBack = false;
        notice.reason = QStringLiteral("iPad took control of slice A. You are still listening.");
        notice.receivedAt = QDateTime::currentDateTime();
        const QString ended = MultiDeviceController::controlTakeBackOff(notice, true);
        QCOMPARE(ended, QStringLiteral("That can no longer be taken back."));
        QVERIFY(OperatorWording::isPlain(ended));
        QCOMPARE(MultiDeviceController::controlTakeBackOff(notice, false),
                 StationClient::controlTakeBackUnavailableReason());
        notice.prompt.takeBack = true;
        QVERIFY(MultiDeviceController::controlTakeBackOff(notice, true).isEmpty());
        notice.prompt.takeBack = false;

        QWidget host;
        NoticeCard card(notice, &host, ended);
        QVERIFY(card.takeBackButton() != nullptr);
        QVERIFY(!card.takeBackButton()->isEnabled());
        QCOMPARE(card.takeBackButton()->toolTip(), ended);
        QVERIFY(card.styleSheet().contains(QStringLiteral("QPushButton:disabled")));
        QSignalSpy asked(&card, &NoticeCard::takeBackRequested);
        card.takeBackButton()->click();
        QCOMPARE(asked.count(), 0);
    }

    // A window whose link is at sliceAccessVersion 1, as an older Core's is
    // (here the window declares sliceAccess 1, so the Core sends it 1 and
    // offers no Take it back): the card shows Take it back off, with the
    // reason, never hidden.
    void onALinkAtSliceAccess1TakeItBackIsShownOffWithTheReason()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        w.client.setSliceAccessDeclaredForTest(1);
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QVERIFY(w.client.remoteSliceAccessAvailable());
        QVERIFY(!w.client.controlTakeBackAvailable());
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        const int own = core.model->sliceOwnership()->ownedBy(w.key->fingerprint()).first();
        QTRY_VERIFY(w.access().controlledHere(own));

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, QStringLiteral("access:%1").arg(own)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(appB, own))));

        QTRY_COMPARE(controller.noticeCards().size(), 1);
        NoticeCard* card = controller.noticeCards().first();
        QVERIFY(card->takeBackButton() != nullptr);
        QVERIFY(!card->takeBackButton()->isEnabled());
        const QString why = card->takeBackButton()->toolTip();
        QCOMPARE(why, StationClient::controlTakeBackUnavailableReason());
        QVERIFY(OperatorWording::isPlain(why));
        card->takeBackButton()->click();
        QTest::qWait(100);
        QVERIFY(w.sentVerbs(QStringLiteral("notice.takeBack")).isEmpty());
        QCOMPARE(core.model->sliceOwnership()->mark(own).owner, b.key.fingerprint());
    }

    // Core-slice take-over (JJ, 2026-09-30): the window learns from the
    // Core's sliceAccessVersion whether the Core's own slice may be taken.
    // This window declares 3 and reads 3; one that declares 2 reads 2 and
    // shows Take control on the Core's own slice off with the Core's words.
    void theCoresOwnSliceTakeFollowsTheLinksVersion()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QCOMPARE(w.client.capabilities().sliceAccessVersion, 3);
        QVERIFY(w.client.coreSliceTakeAvailable());
        QVERIFY(w.access().coreSliceTakeable());

        Window older;
        older.client.setSliceAccessDeclaredForTest(2);
        QVERIFY(core.server->deviceStore()->add(older.record()));
        QVERIFY(older.connectTo(core));
        QCOMPARE(older.client.capabilities().sliceAccessVersion, 2);
        QVERIFY(older.client.controlTakeBackAvailable());
        QVERIFY(!older.client.coreSliceTakeAvailable());
        QVERIFY(!older.access().coreSliceTakeable());
        const QString words = StationClient::coreSliceTakeUnavailableReason(QLatin1Char('A'));
        QCOMPARE(words, QStringLiteral("Slice A is run by the Core itself, so control of it "
                                       "cannot pass to this device."));
        QVERIFY(OperatorWording::isPlain(words));
    }

    // A window whose link has no sliceAccessVersion (here: a window that
    // does not share the Core as a device, so the Core offers none) sends
    // none of the four verbs; each is refused here with the update words,
    // and nothing it holds is read-only.
    void withoutTheFeatureNoNewVerbIsSent()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        w.client.setDeclaresSessionHolder(false);
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QVERIFY(!w.client.remoteSliceAccessAvailable());
        QVERIFY(w.access().entries().isEmpty());
        const QList<IStationLink::CommandOutcome> outcomes{
            w.client.requestListen(0, 1), w.client.requestStopListening(0, 1),
            w.client.requestTakeControl(0, 1, 1), w.client.requestRelease(0, 1, 1),
            w.client.requestListenLevel(0, 1, 0.5, false)};
        for (const IStationLink::CommandOutcome& outcome : outcomes) {
            QVERIFY(!outcome.sent);
            QCOMPARE(outcome.reason, IStationLink::sliceAccessUnavailableReason());
            QVERIFY(OperatorWording::isPlain(outcome.reason));
        }
        QTest::qWait(100);
        QVERIFY(w.sentVerbs(QStringLiteral("slice.")).isEmpty());
        for (SliceModel* slice : w.remote.slices()) {
            QVERIFY(!slice->isReadOnlyListener());
        }
    }
};

QTEST_MAIN(TstSliceAccessClient)
#include "tst_slice_access_client.moc"
