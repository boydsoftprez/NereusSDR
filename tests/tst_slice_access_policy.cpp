// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_slice_access_policy.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 2: who may see, hear and
// change each slice (SliceAccessPolicy), at every path that writes to a
// slice, carries its media or its transmit:
//
//   - the three predicates and the Core's own position over SliceOwnership,
//     a listener injected by hand (Task 3 adds how devices join);
//   - a device that only listens to another device's slice: its property
//     write, settings write and remove, every verb naming a slice, the
//     receiver verbs and tx.setTxSlice are refused and change nothing,
//     while the controller's same writes go through;
//   - the transmit arbiter never binds a slice its holder only listens to;
//   - the Core's station TCI server refuses a slice nobody controls that a
//     device listens to, and still changes a slice made before any device;
//   - a source scan: no access decision compares mark().owner outside
//     SliceOwnership, SliceAccessPolicy, the anchor code and
//     ReceiveLayoutStore, and every access site names the policy.
//
// No radio: a RadioModel marked connected for the test, the in-process
// loopback, and a loopback TCI app. Nothing keys the radio (MoxController
// with zero timers only marks the holder).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 2,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 3:
//               listeners join through SliceOwnership::join (the test-only
//               listener seam is gone); a change of owner keeps the former
//               controller listening. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4: the
//               form swap's site is onSliceAccessChanged; the access
//               controller's release checks name mayChange. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 15 fix round 1: the hosting
//               desktop's policy site is stationControlsSlice, which
//               desktopSliceAllowed calls (moved in Task 11). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QDirIterator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QWebSocket>

#include "core/safety/TxRefusal.h"
#include "core/session/SliceAccessPolicy.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

const QByteArray kA = QByteArray(32, '\x41');
const QByteArray kB = QByteArray(32, '\x42');
const QByteArray kC = QByteArray(32, '\x43');

quint16 freeTciPort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

// An app on the station network: every text frame the server sends it.
struct TciApp {
    QWebSocket socket;
    QStringList frames;
    explicit TciApp(quint16 port)
    {
        QObject::connect(&socket, &QWebSocket::textMessageReceived, &socket,
                         [this](const QString& text) {
            for (const QString& part : text.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                frames.append(part.trimmed() + QLatin1Char(';'));
            }
        });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    }
    bool has(const QString& frame) const { return frames.contains(frame); }
};

// ---- The source scan ----------------------------------------------------

QString sourceRoot()
{
    return QStringLiteral(NEREUS_SOURCE_DIR);
}

// A source file's code with its comments blanked (string literals kept),
// the same length as the text so offsets and lines still match.
QString codeWithoutComments(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString text = QString::fromUtf8(file.readAll());
    QString code = text;
    qsizetype i = 0;
    while (i < text.size()) {
        const QChar c = text.at(i);
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();
        if (c == QLatin1Char('/') && next == QLatin1Char('/')) {
            while (i < text.size() && text.at(i) != QLatin1Char('\n')) {
                code[i] = QLatin1Char(' ');
                ++i;
            }
            continue;
        }
        if (c == QLatin1Char('/') && next == QLatin1Char('*')) {
            while (i < text.size()
                   && !(text.at(i) == QLatin1Char('*') && i + 1 < text.size()
                        && text.at(i + 1) == QLatin1Char('/'))) {
                if (text.at(i) != QLatin1Char('\n')) {
                    code[i] = QLatin1Char(' ');
                }
                ++i;
            }
            if (i < text.size()) {
                code[i] = QLatin1Char(' ');
                code[i + 1] = QLatin1Char(' ');
                i += 2;
            }
            continue;
        }
        if (c == QLatin1Char('"')) {
            ++i;
            while (i < text.size() && text.at(i) != QLatin1Char('"')
                   && text.at(i) != QLatin1Char('\n')) {
                i += text.at(i) == QLatin1Char('\\') ? 2 : 1;
            }
            ++i;
            continue;
        }
        ++i;
    }
    return code;
}

// The qualified name (Class::member) of the function whose definition
// encloses `offset`: the last definition line (at column 0) before it.
QString enclosingFunction(const QString& code, qsizetype offset)
{
    static const QRegularExpression definition(QStringLiteral(
        "^[A-Za-z_][^;{}\\n]*?\\b([A-Za-z_]\\w*::~?[A-Za-z_]\\w*)\\s*\\("),
        QRegularExpression::MultilineOption);
    QString found;
    QRegularExpressionMatchIterator it = definition.globalMatch(code.left(offset));
    while (it.hasNext()) {
        found = it.next().captured(1);
    }
    return found;
}

// The bodies of `qualified`'s definitions in `code` (each from its opening
// brace to the matching one; overloads joined), or empty when not found. A
// constructor's initializer list is skipped.
QString functionBody(const QString& code, const QString& qualified)
{
    QString bodies;
    const QRegularExpression head(QStringLiteral("^[^;{}\\n]*\\b%1\\s*\\(")
                                      .arg(QRegularExpression::escape(qualified)),
                                  QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = head.globalMatch(code);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        qsizetype i = match.capturedEnd() - 1;
        int depth = 0;
        for (; i < code.size(); ++i) {
            if (code.at(i) == QLatin1Char('(')) {
                ++depth;
            } else if (code.at(i) == QLatin1Char(')') && --depth == 0) {
                break;
            }
        }
        qsizetype j = i + 1;
        while (j < code.size() && code.at(j) != QLatin1Char('{') && code.at(j) != QLatin1Char(';')) {
            ++j;
        }
        if (j >= code.size() || code.at(j) != QLatin1Char('{')) {
            continue;   // a declaration or a call
        }
        const qsizetype start = j;
        depth = 0;
        for (; j < code.size(); ++j) {
            const QChar c = code.at(j);
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                const QChar quote = c;
                ++j;
                while (j < code.size() && code.at(j) != quote && code.at(j) != QLatin1Char('\n')) {
                    j += code.at(j) == QLatin1Char('\\') ? 2 : 1;
                }
                continue;
            }
            if (c == QLatin1Char('{')) {
                ++depth;
            } else if (c == QLatin1Char('}') && --depth == 0) {
                bodies += code.mid(start, j - start + 1);
                break;
            }
        }
    }
    return bodies;
}

} // namespace

class SliceAccessPolicyTest : public QObject {
    Q_OBJECT

private slots:
    // ── The predicates over SliceOwnership ──────────────────────────────

    void theControllerMaySeeHearAndChange()
    {
        SliceOwnership ownership;
        {
            SliceOwnership::CreatorScope scope(&ownership, kA);
            ownership.noteSliceAdded(0);
        }
        QVERIFY(SliceAccessPolicy::maySee(ownership, kA, 0));
        QVERIFY(SliceAccessPolicy::mayHear(ownership, kA, 0));
        QVERIFY(SliceAccessPolicy::mayChange(ownership, kA, 0));
        QVERIFY(SliceAccessPolicy::mayTransmitOn(ownership, kA, 0));
        QVERIFY(ownership.isListening(kA, 0));
        QCOMPARE(ownership.listenersOf(0), QList<QByteArray>{kA});
        // Another device, and nobody, may do none of it.
        for (const QByteArray& other : {kB, QByteArray()}) {
            QVERIFY(!SliceAccessPolicy::maySee(ownership, other, 0));
            QVERIFY(!SliceAccessPolicy::mayHear(ownership, other, 0));
            QVERIFY(!SliceAccessPolicy::mayChange(ownership, other, 0));
            QVERIFY(!SliceAccessPolicy::mayTransmitOn(ownership, other, 0));
        }
        QVERIFY(!SliceAccessPolicy::stationMayChangeUnclaimed(ownership, 0));
    }

    void aListenerMaySeeAndHearButNeverChangeOrTransmit()
    {
        SliceOwnership ownership;
        {
            SliceOwnership::CreatorScope scope(&ownership, kA);
            ownership.noteSliceAdded(0);
        }
        QVERIFY(ownership.join(kB, 0));
        QVERIFY(ownership.join(kC, 0));
        QVERIFY(ownership.join(kB, 0));   // already joined: no change
        QCOMPARE(ownership.listenersOf(0), (QList<QByteArray>{kA, kB, kC}));
        QVERIFY(ownership.isListening(kB, 0));
        QVERIFY(SliceAccessPolicy::maySee(ownership, kB, 0));
        QVERIFY(SliceAccessPolicy::mayHear(ownership, kB, 0));
        QVERIFY(!SliceAccessPolicy::mayChange(ownership, kB, 0));
        QVERIFY(!SliceAccessPolicy::mayTransmitOn(ownership, kB, 0));
        // The controller keeps all of it.
        QVERIFY(SliceAccessPolicy::mayChange(ownership, kA, 0));
        QVERIFY(SliceAccessPolicy::mayTransmitOn(ownership, kA, 0));
        // An owner change leaves the listeners as they were, and the
        // former controller listening (Task 3).
        ownership.setOwner(0, kB);
        QCOMPARE(ownership.listenersOf(0), (QList<QByteArray>{kB, kA, kC}));
        QVERIFY(ownership.isListening(kA, 0));
        QVERIFY(!SliceAccessPolicy::mayChange(ownership, kA, 0));
        QVERIFY(SliceAccessPolicy::mayChange(ownership, kB, 0));
        QVERIFY(!SliceAccessPolicy::mayChange(ownership, kC, 0));
    }

    void theCoreChangesOnlyASliceNobodyControlsOrListensTo()
    {
        SliceOwnership ownership;
        ownership.noteSliceAdded(0);   // made before any device
        ownership.noteSliceAdded(1);
        ownership.noteSliceAdded(2);
        ownership.noteSliceAdded(3);
        QVERIFY(ownership.join(kB, 1));   // released, B still listens
        ownership.setOwner(2, kA);
        ownership.hold(3, kC);
        QVERIFY(SliceAccessPolicy::stationMayChangeUnclaimed(ownership, 0));
        QVERIFY(!SliceAccessPolicy::stationMayChangeUnclaimed(ownership, 1));
        QVERIFY(!SliceAccessPolicy::stationMayChangeUnclaimed(ownership, 2));
        QVERIFY(!SliceAccessPolicy::stationMayChangeUnclaimed(ownership, 3));
        // The released slice with a listener is nobody's to change.
        for (const QByteArray& device : {kA, kB, kC, SliceOwnership::stationDevice()}) {
            QVERIFY(!SliceAccessPolicy::mayChange(ownership, device, 1));
        }
        QVERIFY(SliceAccessPolicy::maySee(ownership, kB, 1));
        // A held slice: the station device runs it and may change it; it
        // carries the transmit of the device it is held for.
        QVERIFY(SliceAccessPolicy::mayChange(ownership, SliceOwnership::stationDevice(), 3));
        QVERIFY(!SliceAccessPolicy::mayChange(ownership, kC, 3));
        QVERIFY(SliceAccessPolicy::mayTransmitOn(ownership, kC, 3));
        QVERIFY(!SliceAccessPolicy::mayTransmitOn(ownership, SliceOwnership::stationDevice(), 3));
    }

    void listenersAreKeptWhileASliceIsRemovedAndForgottenAfter()
    {
        SliceOwnership ownership;
        {
            SliceOwnership::CreatorScope scope(&ownership, kA);
            ownership.noteSliceAdded(0);
        }
        QVERIFY(ownership.join(kB, 0));
        ownership.beginRemove(0);
        // Its removal still reaches whoever saw it.
        QVERIFY(SliceAccessPolicy::maySee(ownership, kB, 0));
        QVERIFY(SliceAccessPolicy::maySee(ownership, kA, 0));
        QVERIFY(!ownership.join(kC, 0));   // not live: refused
        QCOMPARE(ownership.listenersOf(0), (QList<QByteArray>{kA, kB}));
        ownership.endRemove(0);
        QVERIFY(ownership.listenersOf(0).isEmpty());
        QVERIFY(!SliceAccessPolicy::maySee(ownership, kB, 0));
        // The id made again is a new slice with no listeners.
        {
            SliceOwnership::CreatorScope scope(&ownership, kA);
            ownership.noteSliceAdded(0);
        }
        QVERIFY(!ownership.isListening(kB, 0));
    }

    // ── Transmit ────────────────────────────────────────────────────────

    // bindForHolder never binds a slice its holder only listens to, even
    // when the active lookup names it (Task 3's active receive slice).
    void bindForHolderNeverBindsASliceTheHolderOnlyListensTo()
    {
        SliceOwnership ownership;
        QVector<SliceModel*> slices;
        for (int id : {0, 1}) {
            auto* slice = new SliceModel(this);
            slice->setSliceIndex(id);
            slices.append(slice);
            SliceOwnership::CreatorScope scope(&ownership, id == 0 ? kA : kB);
            ownership.noteSliceAdded(id);
        }
        slices[1]->setTxSlice(true);
        QVERIFY(ownership.join(kB, 0));
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.syncToSliceList();
        QCOMPARE(arb.txBoundSliceId(), 1);
        arb.setTransmitAccess(
            [&ownership](const QByteArray& device, int sliceId) {
                return SliceAccessPolicy::mayTransmitOn(ownership, device, sliceId);
            },
            [](const QByteArray&) { return 0; });   // B's active receive slice: A's
        QVERIFY(!arb.bindForHolder(kB, 0));
        QCOMPARE(arb.txBoundSliceId(), 1);
        QVERIFY(!arb.requestHandoff(0, kB));
        QCOMPARE(arb.txBoundSliceId(), 1);
        // The controller's own choice binds.
        QVERIFY(arb.bindForHolder(kA, 0));
        QCOMPARE(arb.txBoundSliceId(), 0);
    }

    // ── A listener's writes at the Core ─────────────────────────────────

    void aListenersWritesAndVerbsAreRefusedAndTheControllersGoThrough()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QVERIFY(ownership->join(b.key.fingerprint(), 0));
        QVERIFY(ownership->isListening(b.key.fingerprint(), 0));
        SliceModel* hers = core.model->sliceById(0);
        const double frequency = hers->frequency();
        const int afGain = hers->afGain();
        const Band band = hers->band();
        const QString reason = ownedElsewhere(QStringLiteral("iPhone"));
        const QString sliceKey = QStringLiteral("Slice0/AfGain");
        core.settings->setValue(sliceKey, QStringLiteral("17"));
        const int notches = static_cast<int>(core.model->notchModel()->notches().size());
        const int bound = core.model->txSliceArbiter()->txBoundSliceId();

        // A property write.
        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0", {f64("frequency", 7074000.0), int64("afGain", 5)}, 601)));
        QTRY_VERIFY(!propertyResult(appB, 601).isEmpty());
        const QJsonArray results =
            propertyResult(appB, 601).value(QStringLiteral("results")).toArray();
        QCOMPARE(results.size(), 2);
        for (const QJsonValue& r : results) {
            QCOMPARE(r.toObject().value(QStringLiteral("accepted")).toBool(true), false);
            QCOMPARE(r.toObject().value(QStringLiteral("reason")).toString(), reason);
        }

        // A settings write and a settings remove.
        const auto rejectsFor = [appB](const QString& key) {
            QList<QJsonObject> found;
            for (const QJsonObject& o : ofType(appB->received(), QStringLiteral("settings.reject"))) {
                if (o.value(QStringLiteral("key")).toString() == key) {
                    found.append(o);
                }
            }
            return found;
        };
        appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(sliceKey, QStringLiteral("5"), QStringLiteral("b-1"))));
        QTRY_COMPARE(rejectsFor(sliceKey).size(), 1);
        QCOMPARE(rejectsFor(sliceKey).last().value(QStringLiteral("reason")).toString(), reason);
        appB->sendText(SessionMessages::encode(SessionMessages::settingsRemove(sliceKey)));
        QTRY_COMPARE(rejectsFor(sliceKey).size(), 2);
        QCOMPARE(rejectsFor(sliceKey).last().value(QStringLiteral("reason")).toString(), reason);
        QCOMPARE(core.settings->value(sliceKey).toString(), QStringLiteral("17"));

        // Every verb that names a slice.
        const QList<QPair<QByteArray, QList<MirrorUpdate>>> verbs{
            {"removeSlice", {int64("sliceId", 0)}},
            {"setActiveSliceById", {int64("sliceId", 0)}},
            {"nnr.resetTuning", {int64("sliceId", 0)}},
            {"nnr.tryAgain", {int64("sliceId", 0)}},
            {"nnr.setDiagnostics",
             {int64("sliceId", 0), int64("testMode", 1), int64("outputMode", 1)}},
            {"notch.add",
             {int64("sliceId", 0), f64("centreHz", frequency + 500.0), f64("widthHz", 100.0)}},
            {"requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)}},
            {"requestStreamCentre", {int64("sliceId", 0), f64("centreHz", frequency + 1000.0)}},
            {"requestStreamCtunPinned",
             {int64("sliceId", 0), MirrorUpdate{0, "pinned", MirrorWireKind::Bool, true}}},
            {"slice.selectBand",
             {int64("sliceId", 0),
              int64("band", static_cast<int>(band == Band::Band40m ? Band::Band20m
                                                                   : Band::Band40m))}},
            {"notch.addAtSlice", {int64("sliceId", 0)}},
        };
        for (const auto& verb : verbs) {
            const QJsonObject refused = core.invoke(appB, verb.first, verb.second);
            QVERIFY2(!refused.value(QStringLiteral("accepted")).toBool(true),
                     verb.first.constData());
            QCOMPARE(refused.value(QStringLiteral("reason")).toString(), reason);
        }
        QTest::qWait(50);   // nothing a later turn could apply either

        // Nothing changed.
        QVERIFY(core.model->sliceById(0) == hers);
        QCOMPARE(hers->frequency(), frequency);
        QCOMPARE(hers->afGain(), afGain);
        QCOMPARE(hers->band(), band);
        QVERIFY(hers->isActive());
        QCOMPARE(static_cast<int>(core.model->notchModel()->notches().size()), notches);
        QCOMPARE(core.model->txSliceArbiter()->txBoundSliceId(), bound);
        QCOMPARE(core.settings->value(sliceKey).toString(), QStringLiteral("17"));
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QVERIFY(ownership->isListening(b.key.fingerprint(), 0));

        // The controller's same writes go through.
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0", {f64("frequency", 7074000.0), int64("afGain", 5)}, 602)));
        QTRY_COMPARE(hers->frequency(), 7074000.0);
        QTRY_COMPARE(hers->afGain(), 5);
        appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(sliceKey, QStringLiteral("21"), QStringLiteral("a-1"))));
        QTRY_COMPARE(core.settings->value(sliceKey).toString(), QStringLiteral("21"));
        appA->sendText(SessionMessages::encode(SessionMessages::settingsRemove(sliceKey)));
        QTRY_VERIFY(!core.settings->contains(sliceKey));
        QVERIFY(ofType(appA->received(), QStringLiteral("settings.reject")).isEmpty());
        for (const QByteArray verb : {QByteArrayLiteral("setActiveSliceById"),
                                      QByteArrayLiteral("notch.addAtSlice")}) {
            const QJsonObject done = core.invoke(appA, verb, {int64("sliceId", 0)});
            QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false), verb.constData());
        }
        QTRY_COMPARE(static_cast<int>(core.model->notchModel()->notches().size()), notches + 1);
        const Band other = band == Band::Band40m ? Band::Band20m : Band::Band40m;
        QVERIFY(core.invoke(appA, "slice.selectBand",
                            {int64("sliceId", 0), int64("band", static_cast<int>(other))})
                    .value(QStringLiteral("accepted")).toBool(false));
        QTRY_COMPARE(hers->band(), other);
    }

    // The receiver verbs, with receivers in use (the Core's receiver path).
    void aListenersReceiverVerbsAreRefusedWithReceiversInUse()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        QVERIFY(core.model->streamAllocator().streamCount() > 0);
        QVERIFY(core.model->sliceOwnership()->join(b.key.fingerprint(), 0));
        SliceModel* slice = core.model->sliceById(0);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const int rate = slice->sampleRateHz();
        const double centre = core.model->streamAllocator().streamCentreHz(stream);
        const QString reason = ownedElsewhere(QStringLiteral("iPhone"));
        const QList<QPair<QByteArray, QList<MirrorUpdate>>> verbs{
            {"requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)}},
            {"requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 14210000.0)}},
            {"requestStreamCtunPinned",
             {int64("sliceId", 0), MirrorUpdate{0, "pinned", MirrorWireKind::Bool, true}}},
        };
        for (const auto& verb : verbs) {
            const QJsonObject refused = core.invoke(appB, verb.first, verb.second);
            QVERIFY2(!refused.value(QStringLiteral("accepted")).toBool(true),
                     verb.first.constData());
            QCOMPARE(refused.value(QStringLiteral("reason")).toString(), reason);
        }
        // A frequency write that would leave the receiver's window.
        appB->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 7074000.0)}, 611)));
        QTRY_VERIFY(!propertyResult(appB, 611).isEmpty());
        QTest::qWait(50);
        QCOMPARE(slice->sampleRateHz(), rate);
        QCOMPARE(core.model->streamAllocator().streamCentreHz(slice->streamIndex()), centre);
        QCOMPARE(slice->frequency(), 14200000.0);
        QVERIFY(ofType(appB->received(), QStringLiteral("confirm.request")).isEmpty());
    }

    // tx.setTxSlice from the holder, naming a slice it only listens to.
    void aHolderCannotMoveTransmitToASliceItOnlyListensTo()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        const QByteArray bKey = b.key.fingerprint();
        const int own = ownership->ownedBy(bKey).first();
        QVERIFY(ownership->join(bKey, 0));
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        // B holds transmit (its key, released), and its flag is on its own.
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(b));
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", own)})
                    .value(QStringLiteral("accepted")).toBool(false));
        QCOMPARE(arbiter->txBoundSliceId(), own);

        const QJsonObject refused = core.invoke(appB, "tx.setTxSlice", {int64("sliceId", 0)});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
                 ownedElsewhere(QStringLiteral("iPhone")));
        QTest::qWait(50);
        QCOMPARE(arbiter->txBoundSliceId(), own);
        // The arbiter itself, at the next change of holder, never binds it.
        QVERIFY(arbiter->bindForHolder(bKey, 0));
        QCOMPARE(arbiter->txBoundSliceId(), own);
        QVERIFY(!arbiter->requestHandoff(0, bKey));
        QCOMPARE(arbiter->txBoundSliceId(), own);
        QVERIFY(!core.model->sliceById(0)->isTxSlice());
        Q_UNUSED(appA);
    }

    // ── The Core's station TCI server ───────────────────────────────────

    void theStationTciGateRefusesASliceNobodyControlsThatADeviceListensTo()
    {
        const quint16 port = freeTciPort();
        QVERIFY(port != 0);
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        station.sliceById(0)->setFrequency(7074000.0);
        station.sliceById(1)->setFrequency(14074000.0);
        // Slice 0 was made before any device; slice 1 has no controller
        // and one listener.
        SliceOwnership* ownership = station.sliceOwnership();
        QVERIFY(ownership->mark(0).owner.isEmpty());
        QVERIFY(ownership->mark(1).owner.isEmpty());
        QVERIFY(ownership->join(kB, 1));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));

        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        // The listened slice: nothing changes, the app hears its value.
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("vfo:1,0,14100000;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:1,0,14074000;")), 3000);
        QTest::qWait(100);
        QCOMPARE(station.sliceById(1)->frequency(), 14074000.0);
        // The slice made before any device still changes.
        app.socket.sendTextMessage(QStringLiteral("vfo:0,0,7100000;"));
        QTRY_COMPARE_WITH_TIMEOUT(station.sliceById(0)->frequency(), 7100000.0, 3000);
        app.socket.close();
    }

    // ── The source scan ─────────────────────────────────────────────────

    // No access decision compares a mark's owner itself: every comparison
    // of an `owner` in src/ sits in SliceOwnership, SliceAccessPolicy,
    // ReceiveLayoutStore, or a function named here with why it is not an
    // access decision.
    void noCallSiteComparesAnOwnerForAccess()
    {
        const QSet<QString> wholeFiles{
            QStringLiteral("src/core/SliceOwnership.h"),
            QStringLiteral("src/core/SliceOwnership.cpp"),
            QStringLiteral("src/core/session/SliceAccessPolicy.h"),
            QStringLiteral("src/core/session/SliceAccessPolicy.cpp"),
            QStringLiteral("src/core/ReceiveLayoutStore.h"),
            QStringLiteral("src/core/ReceiveLayoutStore.cpp"),
        };
        const QHash<QString, QString> notAccess{
            // Anchors and placement: which receiver a new slice lands on.
            {QStringLiteral("src/core/session/ReceiverPlanner.cpp|ReceiverPlanner::planAddAfterClosing"),
             QStringLiteral("anchor planning")},
            {QStringLiteral("src/models/RadioModel.cpp|RadioModel::panHasSlicesFor"),
             QStringLiteral("layout placement")},
            // Saving: owners restored from a saved layout.
            {QStringLiteral("src/models/RadioModel.cpp|RadioModel::hydrateReceiveLayout"),
             QStringLiteral("restoring saved owners")},
            // Notices: the mark before an owner change.
            {QStringLiteral("src/core/session/StationServer.cpp|StationServer::onSliceOwnerChanged"),
             QStringLiteral("the mark before a change")},
            // A confirmed change's closing slices still have the owner they
            // had when it was confirmed.
            {QStringLiteral("src/core/session/SessionCommandDispatcher.cpp|"
                            "SessionCommandDispatcher::handleRequestSliceSampleRate"),
             QStringLiteral("changed since asked")},
            // Owners of other things, not slices: a pending PureSignal
            // command's session, a DSP asset transfer's device, a retired
            // PureSignal save's coordinator.
            {QStringLiteral("src/core/session/SessionCommandDispatcher.cpp|"
                            "SessionCommandDispatcher::handlePureSignalAction"),
             QStringLiteral("not a slice")},
            {QStringLiteral("src/core/dsp/DspAssetService.cpp|DspAssetService::execute"),
             QStringLiteral("not a slice")},
            {QStringLiteral("src/core/dsp/DspAssetService.cpp|DspAssetService::cancelOwner"),
             QStringLiteral("not a slice")},
            {QStringLiteral("src/core/session/PureSignalSessionFacade.cpp|"
                            "PureSignalSessionFacade::refreshStatus"),
             QStringLiteral("not a slice")},
        };
        static const QRegularExpression comparison(QStringLiteral(
            "(?:\\.|->)owner\\b\\s*(?:==|!=)|(?:==|!=)\\s*[\\w\\.\\->\\(\\)\\[\\]]*(?:\\.|->)owner\\b"));
        QStringList found;
        int scanned = 0;
        QDirIterator it(QDir(sourceRoot()).filePath(QStringLiteral("src")),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relative = QDir(sourceRoot()).relativeFilePath(path);
            ++scanned;
            if (wholeFiles.contains(relative)) {
                continue;
            }
            const QString code = codeWithoutComments(path);
            QRegularExpressionMatchIterator hits = comparison.globalMatch(code);
            while (hits.hasNext()) {
                const QRegularExpressionMatch hit = hits.next();
                const QString function = enclosingFunction(code, hit.capturedStart());
                if (notAccess.contains(relative + QLatin1Char('|') + function)) {
                    continue;
                }
                const int line = static_cast<int>(code.left(hit.capturedStart()).count(QLatin1Char('\n'))) + 1;
                found.append(QStringLiteral("%1:%2 (%3): %4")
                                 .arg(relative).arg(line).arg(function, hit.captured(0)));
            }
        }
        QVERIFY2(scanned > 100, "the scan found no sources");
        QVERIFY2(found.isEmpty(),
                 qPrintable(QStringLiteral("An owner comparison outside SliceAccessPolicy; call "
                                           "maySee, mayHear, mayChange or mayTransmitOn:\n")
                            + found.join(QLatin1Char('\n'))));
    }

    // Every place that decides who may see, hear or change a slice, or
    // carry its transmit, names the policy (or a Core query built on it).
    void everyAccessSiteNamesThePolicy()
    {
        struct Site {
            const char* file;
            const char* function;
            QStringList names;   // any one of these
        };
        const QStringList policy{QStringLiteral("SliceAccessPolicy::")};
        const QStringList change{QStringLiteral("changeRefusal("),
                                 QStringLiteral("SliceAccessPolicy::mayChange(")};
        const QList<Site> sites{
            // Session writes, settings keys, mirror delivery, media queries.
            {"src/core/session/StationServer.cpp", "StationServer::changeRefusal",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            {"src/core/session/StationServer.cpp", "StationServer::handlePropertyWrite", change},
            {"src/core/session/StationServer.cpp", "StationServer::sliceSettingsRefusal",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            {"src/core/session/StationServer.cpp", "StationServer::ownershipAllows",
             {QStringLiteral("SliceAccessPolicy::maySee(")}},
            // Slice control plan Task 4: the form swap moved from the owner
            // change to every change of controller or listeners.
            {"src/core/session/StationServer.cpp", "StationServer::onSliceAccessChanged",
             {QStringLiteral("SliceAccessPolicy::maySee(")}},
            {"src/core/session/SliceAccessController.cpp",
             "SliceAccessController::closeIsRelease",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            {"src/core/session/SliceAccessController.cpp", "SliceAccessController::release",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            {"src/core/session/StationServer.cpp", "StationServer::mediaSessionControlsSlice",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            {"src/core/session/StationServer.cpp", "StationServer::mediaSessionHearsSlice",
             {QStringLiteral("SliceAccessPolicy::mayHear(")}},
            {"src/core/session/StationServer.cpp", "StationServer::mediaSessionSeesSlice",
             {QStringLiteral("SliceAccessPolicy::maySee(")}},
            // The dispatcher's slice check is changeRefusal.
            {"src/core/session/StationServer.cpp", "StationServer::StationServer",
             {QStringLiteral("return changeRefusal(requester, sliceId);")}},
            {"src/core/session/SessionCommandDispatcher.cpp",
             "SessionCommandDispatcher::refusedForAnotherDevice",
             {QStringLiteral("m_sliceAccess(m_requester, sliceId)")}},
            {"src/core/session/SessionCommandDispatcher.cpp",
             "SessionCommandDispatcher::handleSetTxSlice",
             {QStringLiteral("m_sliceAccess(m_requester, sliceId)")}},
            {"src/core/session/SessionCommandDispatcher.cpp",
             "SessionCommandDispatcher::handleRequestSliceSampleRate",
             {QStringLiteral("access(requester, sliceId)")}},
            // Receiver commands and shared settings.
            {"src/core/session/StationReceivers.cpp", "StationServer::handleReceiverCommand", change},
            {"src/core/session/StationReceivers.cpp", "StationServer::handleCentreMove", change},
            {"src/core/session/StationReceivers.cpp", "StationServer::checkPanMove", change},
            {"src/core/session/StationReceivers.cpp", "StationServer::handleSliceRetune", change},
            {"src/core/session/StationReceivers.cpp", "StationServer::answerConfirm", change},
            {"src/core/session/StationSharedSettings.cpp", "StationServer::handleSharedSetting",
             change},
            // Media.
            {"src/core/session/media/DaemonMediaController.cpp",
             "DaemonMediaController::controlsSlice",
             {QStringLiteral("mediaSessionControlsSlice(")}},
            {"src/core/session/media/DaemonMediaController.cpp", "DaemonMediaController::hearsSlice",
             {QStringLiteral("mediaSessionHearsSlice(")}},
            {"src/core/session/media/DaemonMediaController.cpp", "DaemonMediaController::seesSlice",
             {QStringLiteral("mediaSessionSeesSlice(")}},
            // Transmit.
            {"src/core/session/StationTransmitTake.cpp", "StationServer::refreshTxMarks",
             {QStringLiteral("SliceAccessPolicy::mayTransmitOn(")}},
            {"src/core/session/StationTransmitTake.cpp", "StationServer::onSliceClosedForHolder",
             {QStringLiteral("SliceAccessPolicy::mayTransmitOn(")}},
            {"src/core/TxSliceArbiter.cpp", "TxSliceArbiter::requestHandoff",
             {QStringLiteral("m_mayTransmit(")}},
            {"src/core/TxSliceArbiter.cpp", "TxSliceArbiter::bindForHolder",
             {QStringLiteral("m_mayTransmit(")}},
            {"src/models/RadioModel.cpp", "RadioModel::RadioModel",
             {QStringLiteral("SliceAccessPolicy::mayTransmitOn(")}},
            {"src/models/RadioModel.cpp", "RadioModel::removeSliceImpl",
             {QStringLiteral("SliceAccessPolicy::mayTransmitOn(")}},
            {"src/models/RadioModel.cpp", "RadioModel::setActiveSliceByIdFor",
             {QStringLiteral("SliceAccessPolicy::mayChange(")}},
            // The Core's own position and the hosting desktop.
            {"src/core/StationTciController.cpp", "StationTciController::StationTciController",
             {QStringLiteral("SliceAccessPolicy::stationMayChangeUnclaimed(")}},
            {"src/core/TciServer.cpp", "TciServer::desktopSliceForReceiver", policy},
            {"src/gui/containers/ContainerButtonDispatcher.cpp", "ContainerButtonDispatcher::sliceFor",
             policy},
            // Slice control plan Task 11 moved the hosting desktop's check
            // into stationControlsSlice; desktopSliceAllowed goes through it.
            {"src/gui/MainWindow.cpp", "MainWindow::stationControlsSlice", policy},
            {"src/gui/MainWindow.cpp", "MainWindow::desktopSliceAllowed",
             {QStringLiteral("stationControlsSlice(")}},
            {"src/gui/MainWindow.cpp", "MainWindow::refreshDesktopStationState",
             {QStringLiteral("desktopSliceAllowed(")}},
        };
        QStringList missing;
        QHash<QString, QString> codes;
        for (const Site& site : sites) {
            const QString file = QString::fromLatin1(site.file);
            if (!codes.contains(file)) {
                codes.insert(file, codeWithoutComments(QDir(sourceRoot()).filePath(file)));
            }
            const QString body = functionBody(codes.value(file), QString::fromLatin1(site.function));
            if (body.isEmpty()) {
                missing.append(QStringLiteral("%1: %2 not found").arg(file, QLatin1String(site.function)));
                continue;
            }
            const bool named = std::any_of(site.names.cbegin(), site.names.cend(),
                                           [&body](const QString& name) { return body.contains(name); });
            if (!named) {
                missing.append(QStringLiteral("%1: %2 does not name %3")
                                   .arg(file, QLatin1String(site.function), site.names.join(QStringLiteral(" or "))));
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(SliceAccessPolicyTest)
#include "tst_slice_access_policy.moc"
