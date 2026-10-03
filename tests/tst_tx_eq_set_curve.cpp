// no-port-check: NereusSDR-original. R-IOS-13 / R-R3-49 (txEqCurveVersion
// 2): txEq.setCurve and txEq.resetCurve from a device that declared
// txEqCurve 2, spoken over the link directly as the phone speaks it. Each
// is that device's own txEqParaEqData write: the Core's rounding and
// ordering, the curve refused whole outside the TX EQ panel's choices, the
// write's gates, the side-effect delta, and the kept curve in the result.
// Loopback link, no radio, no audio device, never keyed.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created (R-IOS-13 / R-R3-49,
//                                    txEqCurveVersion 2). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  JJ's TX EQ ruling: the curve is taken
//                                    while the radio is on the air, and
//                                    RF Power too at version 13.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/ParaEqCurve.h"
#include "core/ParaEqEnvelope.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// The link document's worked example, as txEqCurve shows it.
const QString kWorked = QStringLiteral(
    "{\"maxHz\":3000,\"minHz\":50,\"parametric\":true,\"points\":["
    "{\"frequencyHz\":50,\"gainDb\":-6,\"q\":1.5},"
    "{\"frequencyHz\":300,\"gainDb\":3,\"q\":2},"
    "{\"frequencyHz\":1200,\"gainDb\":-1.5,\"q\":4},"
    "{\"frequencyHz\":2400,\"gainDb\":4,\"q\":3},"
    "{\"frequencyHz\":3000,\"gainDb\":0,\"q\":1}],"
    "\"preampDb\":-2.5,\"state\":\"saved\"}");

// The same curve sent out of order and unrounded, "state" and all.
const QString kWorkedSent = QStringLiteral(
    "{\"parametric\":true,\"preampDb\":-2.5,\"minHz\":50,\"maxHz\":3000,\"state\":\"saved\","
    "\"points\":[{\"frequencyHz\":2400,\"gainDb\":4,\"q\":3},"
    "{\"frequencyHz\":50,\"gainDb\":-6,\"q\":1.5},"
    "{\"frequencyHz\":1200.0004,\"gainDb\":-1.5,\"q\":4},"
    "{\"frequencyHz\":300,\"gainDb\":3.04,\"q\":2.001},"
    "{\"frequencyHz\":3000,\"gainDb\":0,\"q\":1}]}");

const QString kFourPoints = QStringLiteral(
    "{\"parametric\":true,\"preampDb\":0,\"minHz\":0,\"maxHz\":4000,\"points\":["
    "{\"frequencyHz\":0,\"gainDb\":0,\"q\":4},{\"frequencyHz\":1000,\"gainDb\":0,\"q\":4},"
    "{\"frequencyHz\":2000,\"gainDb\":0,\"q\":4},{\"frequencyHz\":4000,\"gainDb\":0,\"q\":4}]}");

// Reset from the worked example: five flat points from 50 to 3000 Hz.
const QString kWorkedReset = QStringLiteral(
    "{\"maxHz\":3000,\"minHz\":50,\"parametric\":true,\"points\":["
    "{\"frequencyHz\":50,\"gainDb\":0,\"q\":4},"
    "{\"frequencyHz\":787.5,\"gainDb\":0,\"q\":4},"
    "{\"frequencyHz\":1525,\"gainDb\":0,\"q\":4},"
    "{\"frequencyHz\":2262.5,\"gainDb\":0,\"q\":4},"
    "{\"frequencyHz\":3000,\"gainDb\":0,\"q\":4}],"
    "\"preampDb\":0,\"state\":\"saved\"}");

const QString kUpdateApp =
    QStringLiteral("Update this app to change the TX EQ curve on this Core.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// One device signed in with the Core's token, speaking the link directly.
struct Device {
    Device(StationServer* server, QObject* parent, const QHash<QByteArray, int>& features)
    {
        app = new LoopbackTransport(QStringLiteral("app"), parent);
        station = new LoopbackTransport(QStringLiteral("station"), server);
        station->linkTo(app);
        server->acceptTransport(station);
        static_cast<void>(QTest::qWaitFor([this]() { return !app->received().isEmpty(); }, 5000));
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("EQ phone"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
    }
    bool ready() const
    {
        return QTest::qWaitFor([this]() {
            return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
        }, 5000);
    }
    QList<SessionMessage> messages() const
    {
        QList<SessionMessage> out;
        for (const QByteArray& wire : app->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                out.append(message);
            }
        }
        return out;
    }
    // The capability's value, or -1 when the entry is absent.
    int capability(const QByteArray& name) const
    {
        int value = -1;
        for (const SessionMessage& message : messages()) {
            if (message.kind != SessionMessageKind::Capabilities) {
                continue;
            }
            value = -1;
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == name) {
                    value = update.value.toInt();
                }
            }
        }
        return value;
    }
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        found.reason = QStringLiteral("no command.result arrived");
        static_cast<void>(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : messages()) {
                if (message.kind == SessionMessageKind::CommandResult
                    && message.commandId == id) {
                    found = message;
                    return true;
                }
            }
            return false;
        }, 3000));
        return found;
    }
    SessionMessage setCurve(const QString& curveJson)
    {
        return invoke(QByteArrayLiteral("txEq.setCurve"),
                      {MirrorUpdate{0, "curveJson", MirrorWireKind::Utf8, QVariant(curveJson)}});
    }
    // The index in messages() of the command.result for `id`, or -1.
    int resultIndex(quint32 id) const
    {
        const QList<SessionMessage> all = messages();
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].kind == SessionMessageKind::CommandResult && all[i].commandId == id) {
                return i;
            }
        }
        return -1;
    }
    // The last `transmit` value this device heard for `name`, at or before
    // message `upTo` (all when -1).
    QVariant transmitValue(const QByteArray& name, int upTo = -1) const
    {
        QVariant value;
        const QList<SessionMessage> all = messages();
        for (int i = 0; i < all.size() && (upTo < 0 || i <= upTo); ++i) {
            const SessionMessage& message = all[i];
            if ((message.kind != SessionMessageKind::ObjectCreate
                 && message.kind != SessionMessageKind::Delta)
                || message.objectKey != QByteArrayLiteral("transmit")) {
                continue;
            }
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == name) {
                    value = update.value;
                }
            }
        }
        return value;
    }

    LoopbackTransport* app = nullptr;
    LoopbackTransport* station = nullptr;
    quint32 nextId = 8800;
};

QString resultValue(const SessionMessage& result, const QByteArray& name)
{
    for (const MirrorUpdate& value : result.updates) {
        if (value.name == name) {
            return value.value.toString();
        }
    }
    return {};
}

} // namespace

class TstTxEqSetCurve : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("tx-eq-set-curve-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }
    void init()
    {
        AppSettings::instance().clear();
        m_core = makeStationRadioModel();
        m_server = std::make_unique<StationServer>(
            m_core.get(), AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    }
    void cleanup()
    {
        m_server.reset();
        m_core.reset();
    }

    // A device is offered the version it declared, up to 2; one that
    // declared nothing sees no entry, as before.
    void theVersionFollowsTheDeclaredFeature()
    {
        Device none(m_server.get(), this, {});
        QVERIFY(none.ready());
        QCOMPARE(none.capability("txEqCurveVersion"), -1);
        Device one(m_server.get(), this, {{"txEqCurve", 1}});
        QVERIFY(one.ready());
        QCOMPARE(one.capability("txEqCurveVersion"), 1);
        Device two(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(two.ready());
        QCOMPARE(two.capability("txEqCurveVersion"), 2);
        Device three(m_server.get(), this, {{"txEqCurve", 3}});
        QVERIFY(three.ready());
        QCOMPARE(three.capability("txEqCurveVersion"), 2);
    }

    // The curve is taken, rounded and ordered as the Core keeps it, saved
    // as the dialog saves it, and comes back first in the side-effect
    // delta, then in the result. A device at version 1 sees the delta too.
    void aCurveIsTakenAndComesBack()
    {
        Device phone(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(phone.ready());
        Device reader(m_server.get(), this, {{"txEqCurve", 1}});
        QVERIFY(reader.ready());
        const SessionMessage result = phone.setCurve(kWorkedSent);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(resultValue(result, "curve"), kWorked);

        const TransmitModel& tx = m_core->transmitModel();
        QCOMPARE(tx.txEqCurve(), kWorked);
        // What the TX EQ dialog saves for these points.
        ParaEqCurve::TxEqPoints points;
        QString refusal;
        QVERIFY(ParaEqCurve::txEqPointsFromCurveJson(kWorked, points, &refusal));
        QCOMPARE(ParaEqEnvelope::decode(tx.txEqParaEqData()).value_or(QString()),
                 ParaEqCurve::saveToJsonFromPoints(points));

        // The delta came before the result, with both properties.
        const int at = phone.resultIndex(result.commandId);
        QVERIFY(at > 0);
        QCOMPARE(phone.transmitValue("txEqCurve", at).toString(), kWorked);
        QCOMPARE(phone.transmitValue("txEqParaEqData", at).toString(), tx.txEqParaEqData());
        QTRY_COMPARE(reader.transmitValue("txEqCurve").toString(), kWorked);
    }

    // A curve outside the panel's choices is refused whole with the
    // reason, and nothing changes or is sent.
    void aCurveOutsideThePanelsChoicesChangesNothing()
    {
        Device phone(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(phone.ready());
        const QString before = m_core->transmitModel().txEqParaEqData();
        const qsizetype heard = phone.messages().size();
        const SessionMessage result = phone.setCurve(kFourPoints);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose a curve of 5, 10 or 18 points."));
        QVERIFY(resultValue(result, "curve").isEmpty());
        QCOMPARE(m_core->transmitModel().txEqParaEqData(), before);
        QTest::qWait(100);
        // Nothing about the curve followed (a heartbeat or another object's
        // delta may).
        const QList<SessionMessage> after = phone.messages().mid(heard);
        for (const SessionMessage& message : after) {
            if (message.objectKey != QByteArrayLiteral("transmit")) {
                continue;
            }
            for (const MirrorUpdate& update : message.updates) {
                QVERIFY2(update.name != "txEqCurve" && update.name != "txEqParaEqData",
                         update.name.constData());
            }
        }

        const SessionMessage garbled = phone.setCurve(QStringLiteral("[1,2]"));
        QVERIFY(!garbled.accepted);
        QCOMPARE(garbled.reason, QStringLiteral("The TX EQ curve was not understood."));
        const SessionMessage renamed = phone.invoke(
            QByteArrayLiteral("txEq.setCurve"),
            {MirrorUpdate{0, "curve", MirrorWireKind::Utf8, QVariant(kWorkedSent)}});
        QVERIFY(!renamed.accepted);
        QCOMPARE(renamed.reason, QStringLiteral("The TX EQ curve was not understood."));
        QCOMPARE(m_core->transmitModel().txEqParaEqData(), before);
    }

    // The panel's Reset: flat, keeping the points, range and Use Q.
    void resetKeepsTheRangeAndPoints()
    {
        Device phone(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(phone.ready());
        QVERIFY(phone.setCurve(kWorkedSent).accepted);
        const SessionMessage reset = phone.invoke(QByteArrayLiteral("txEq.resetCurve"), {});
        QVERIFY2(reset.accepted, qPrintable(reset.reason));
        QCOMPARE(resultValue(reset, "curve"), kWorkedReset);
        QCOMPARE(m_core->transmitModel().txEqCurve(), kWorkedReset);
        QCOMPARE(phone.transmitValue("txEqCurve", phone.resultIndex(reset.commandId)).toString(),
                 kWorkedReset);

        const SessionMessage withArgs = phone.invoke(
            QByteArrayLiteral("txEq.resetCurve"),
            {MirrorUpdate{0, "curveJson", MirrorWireKind::Utf8, QVariant(kWorkedSent)}});
        QVERIFY(!withArgs.accepted);
        QCOMPARE(withArgs.reason,
                 QStringLiteral("The request to reset the TX EQ curve was not understood."));
        QCOMPARE(m_core->transmitModel().txEqCurve(), kWorkedReset);
    }

    // A device that declared version 1, or none, is refused both verbs.
    void anOlderAppIsRefused()
    {
        const QString before = m_core->transmitModel().txEqParaEqData();
        for (const QHash<QByteArray, int>& features :
             {QHash<QByteArray, int>{}, QHash<QByteArray, int>{{"txEqCurve", 1}}}) {
            Device older(m_server.get(), this, features);
            QVERIFY(older.ready());
            const SessionMessage set = older.setCurve(kWorkedSent);
            QVERIFY(!set.accepted);
            QCOMPARE(set.reason, kUpdateApp);
            const SessionMessage reset = older.invoke(QByteArrayLiteral("txEq.resetCurve"), {});
            QVERIFY(!reset.accepted);
            QCOMPARE(reset.reason, kUpdateApp);
        }
        QCOMPARE(m_core->transmitModel().txEqParaEqData(), before);
    }

    // On a Core that allows remote transmit, the curve is the transmit
    // gate's, as the txEqParaEqData write is: a session it does not permit
    // gets the gate's words, and nothing changes.
    void theTransmitGateIsTheWritesGate()
    {
        m_server->setRemoteTransmitAllowed(true);
        Device phone(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(phone.ready());
        const QString before = m_core->transmitModel().txEqParaEqData();
        const SessionMessage result = phone.setCurve(kWorkedSent);
        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());

        // The same words a raw write of the value gets.
        const quint32 writeId = 4242;
        phone.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("transmit"),
            {MirrorUpdate{1, "txEqParaEqData", MirrorWireKind::Utf8,
                          QVariant(ParaEqEnvelope::encode(QStringLiteral("{}")))}},
            writeId)));
        QString writeReason;
        QVERIFY(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : phone.messages()) {
                if (message.kind == SessionMessageKind::PropertyResult
                    && message.writeId == writeId && !message.propertyResults.isEmpty()) {
                    writeReason = message.propertyResults.first().reason;
                    return true;
                }
            }
            return false;
        }, 3000));
        QCOMPARE(result.reason, writeReason);
        QCOMPARE(m_core->transmitModel().txEqParaEqData(), before);
    }

    // JJ's TX EQ ruling (2026-09-28): a local window changes the TX EQ
    // while transmitting, so the Core takes the curve on the air too, from
    // a device it takes transmit settings from; since
    // transmitSettingsVersion 13 so is RF Power, as locally.
    void theCurveIsTakenOnTheAir()
    {
        Device phone(m_server.get(), this, {{"txEqCurve", 2}});
        QVERIFY(phone.ready());
        MoxController* const mox = m_core->moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QTRY_VERIFY(mox->state() == MoxState::Tx);

        const SessionMessage set = phone.setCurve(kWorkedSent);
        QVERIFY2(set.accepted, qPrintable(set.reason));
        QCOMPARE(m_core->transmitModel().txEqCurve(), kWorked);
        const SessionMessage reset = phone.invoke(QByteArrayLiteral("txEq.resetCurve"), {});
        QVERIFY2(reset.accepted, qPrintable(reset.reason));
        QCOMPARE(m_core->transmitModel().txEqCurve(), kWorkedReset);

        const int power = m_core->transmitModel().power();
        const quint32 writeId = 4343;
        phone.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("transmit"),
            {MirrorUpdate{1, "power", MirrorWireKind::Int64,
                          QVariant(qlonglong(power == 21 ? 22 : 21))}},
            writeId)));
        QString reason;
        QVERIFY(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : phone.messages()) {
                if (message.kind == SessionMessageKind::PropertyResult
                    && message.writeId == writeId && !message.propertyResults.isEmpty()) {
                    reason = message.propertyResults.first().reason;
                    return true;
                }
            }
            return false;
        }, 3000));
        QVERIFY2(reason.isEmpty(), qPrintable(reason));
        QCOMPARE(m_core->transmitModel().power(), power == 21 ? 22 : 21);
        mox->setMox(false);
        QTRY_VERIFY(mox->state() == MoxState::Rx);
    }

private:
    QTemporaryDir m_securityDir;
    std::unique_ptr<RadioModel> m_core;
    std::unique_ptr<StationServer> m_server;
};

QTEST_MAIN(TstTxEqSetCurve)
#include "tst_tx_eq_set_curve.moc"
