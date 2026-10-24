// no-port-check: NereusSDR-original. No upstream logic is ported here.
// =================================================================
// tests/tst_tune_drive_source_link.cpp  (NereusSDR)
// =================================================================
//
// PR 588 (JJ's decision, 2026-10-21): only Setup > Transmit > Power changes
// TUNE's drive source. A client from before that rule sets transmit's
// tuneDrivePowerSource from its power sliders, so the Core takes a change
// of it only from a peer whose hello declared tuneDriveSource 1. An older
// client's change is refused with the Core's value; a write of the value
// the Core already has changes nothing and is taken; its other transmit
// settings (RF Power, the Tune Power command) are taken as before. A
// refusal is logged once per session.
//
// Nothing here keys a radio or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-21 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QLoggingCategory>

#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

namespace {

constexpr qint64 kDriveSlider = static_cast<qint64>(DrivePowerSource::DriveSlider);
constexpr qint64 kTuneSlider = static_cast<qint64>(DrivePowerSource::TuneSlider);
constexpr qint64 kFixed = static_cast<qint64>(DrivePowerSource::Fixed);

const QString kOlderClientReason =
    QStringLiteral("Update this app to change TUNE's drive source on this Core.");

MirrorUpdate source(qint64 value)
{
    return MirrorUpdate{0, QByteArrayLiteral("tuneDrivePowerSource"), MirrorWireKind::Enum,
                        QVariant(value)};
}

QHash<QByteArray, int> tuneDriveSourceAsks()
{
    QHash<QByteArray, int> asks = kHolder;
    asks.insert(QByteArrayLiteral("tuneDriveSource"), 1);
    return asks;
}

QJsonObject resultFor(const LoopbackTransport* app, qint64 writeId, const QString& property)
{
    const QJsonArray results =
        propertyResult(app, writeId).value(QStringLiteral("results")).toArray();
    for (const QJsonValue& r : results) {
        if (r.toObject().value(QStringLiteral("property")).toString() == property) {
            return r.toObject();
        }
    }
    return {};
}

int g_refusalLines = 0;
QtMessageHandler g_previousHandler = nullptr;

void countRefusalLines(QtMsgType type, const QMessageLogContext& context, const QString& text)
{
    if (text.contains(QStringLiteral("Refused tuneDrivePowerSource"))) {
        ++g_refusalLines;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, text);
    }
}

} // namespace

class TstTuneDriveSourceLink : public QObject {
    Q_OBJECT

private slots:
    // An older client's change of the source is refused and the Core's
    // value is answered back; RF Power in the same write, and the Tune
    // Power command, are taken.
    void olderClient_sourceRefused_otherSettingsTaken()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        TransmitModel& tx = core.model->transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        const int power = tx.power() == 40 ? 41 : 40;

        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {int64("power", power), source(kDriveSlider)}, 81)));
        QTRY_VERIFY(!propertyResult(appB, 81).isEmpty());
        const QJsonObject refused = resultFor(appB, 81, QStringLiteral("tuneDrivePowerSource"));
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kOlderClientReason);
        QCOMPARE(refused.value(QStringLiteral("hasValue")).toBool(false), true);
        QCOMPARE(refused.value(QStringLiteral("value")).toObject()
                     .value(QStringLiteral("value")).toInteger(),
                 kFixed);
        const QJsonObject taken = resultFor(appB, 81, QStringLiteral("power"));
        QCOMPARE(taken.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(tx.power(), power);
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::Fixed);

        const int tunePower = tx.tunePowerForTxBand() == 30 ? 31 : 30;
        const QJsonObject tuneResult =
            core.invoke(appB, "setTunePowerForTxBand", {int64("watts", tunePower)});
        QCOMPARE(tuneResult.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(tx.tunePowerForTxBand(), tunePower);
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::Fixed);
    }

    // An older RF Power slider writes the drive slider source with every
    // move; on a Core already there that changes nothing, so it is taken
    // and the older app shows no refusal.
    void olderClient_sameValue_taken()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        TransmitModel& tx = core.model->transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));

        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {source(kDriveSlider)}, 82)));
        QTRY_VERIFY(!propertyResult(appB, 82).isEmpty());
        const QJsonObject result = resultFor(appB, 82, QStringLiteral("tuneDrivePowerSource"));
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(result.value(QStringLiteral("reason")).toString(), QString());
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::DriveSlider);
    }

    // A client that declared tuneDriveSource 1 changes it.
    void declaringClient_sourceTaken()
    {
        Core core;
        Device a;
        core.pair(a);
        TransmitModel& tx = core.model->transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        LoopbackTransport* appA = core.signIn(a, tuneDriveSourceAsks());
        QVERIFY(admitted(appA));

        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {source(kTuneSlider)}, 83)));
        QTRY_VERIFY(!propertyResult(appA, 83).isEmpty());
        const QJsonObject result = resultFor(appA, 83, QStringLiteral("tuneDrivePowerSource"));
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);

        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {source(kFixed)}, 84)));
        QTRY_COMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::Fixed);
    }

    // The desktop's remote window declares it.
    void desktopWindow_declaresTuneDriveSource()
    {
        Core core(true);
        RadioModel model{RadioModel::Role::Remote};
        SettingsProxy proxy;
        StationClient client{&model, &proxy};
        auto* stationEnd = new LoopbackTransport(QStringLiteral("tune-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("tune-client"), this);
        stationEnd->linkTo(clientEnd);
        QList<QByteArray> sent;
        connect(clientEnd, &LoopbackTransport::outboundText, this,
                [&sent](const QByteArray& wire) { sent.append(wire); });
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QTRY_VERIFY(!firstOfType(sent, QStringLiteral("hello")).isEmpty());
        const QJsonObject hello = firstOfType(sent, QStringLiteral("hello"));
        QCOMPARE(hello.value(QStringLiteral("features")).toObject()
                     .value(QStringLiteral("tuneDriveSource")).toInt(),
                 1);
    }

    // One log line per session however many writes are refused.
    void refusal_loggedOncePerSession()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.station.info=true"));
        g_refusalLines = 0;
        g_previousHandler = qInstallMessageHandler(countRefusalLines);
        const auto restore = qScopeGuard([]() {
            qInstallMessageHandler(g_previousHandler);
            g_previousHandler = nullptr;
            QLoggingCategory::setFilterRules(QString());
        });
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        core.model->transmitModel().setTuneDrivePowerSource(DrivePowerSource::Fixed);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));

        for (qint64 id = 91; id <= 95; ++id) {
            appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                "transmit", {source(id % 2 == 0 ? kDriveSlider : kTuneSlider)}, id)));
            QTRY_VERIFY(!propertyResult(appB, id).isEmpty());
        }
        QCOMPARE(g_refusalLines, 1);
    }
};

QTEST_MAIN(TstTuneDriveSourceLink)
#include "tst_tune_drive_source_link.moc"
