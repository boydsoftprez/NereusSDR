// no-port-check: NereusSDR-original. No Thetis logic is ported here.
// =================================================================
// tests/tst_rade_reason_link.cpp  (NereusSDR)
// =================================================================
//
// RADE reason on the wire: a slice's radeReason (why a RADE slice has no
// working decoder) reaches only a peer that declared radeReason 1,
// radeReasonVersion 1 is advertised to that peer only, as the last entry
// before coreBuildInfo, and a remote window's mirrored slice carries the
// Core's reason and clears with it.
//
// The Core's decoder create is made to fail through WdspEngine's test
// seam. Nothing here keys a radio or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QSignalSpy>

#include "core/WdspEngine.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

namespace {

const QString kCreateFailed = QStringLiteral(
    "RADE could not start on slice A: its RADE decoder could not be created.");

bool sawProperty(const LoopbackTransport* app, const QString& property)
{
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        for (const QJsonValue& p : o.value(QStringLiteral("properties")).toArray()) {
            if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                return true;
            }
        }
    }
    return false;
}

// Slice A into RADE with its decoder create refused: the Core's own model
// gives it the reason, and its mode mask mutes it.
void failSliceARadeStart(RadioModel& model)
{
    model.wdspEngine()->setRadeCreateFailsForTest(true);
    model.sliceById(0)->setDspMode(DSPMode::RADE_U);
    QCoreApplication::processEvents();
}

} // namespace

class TstRadeReasonLink : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    // The version reaches a declaring peer, last before coreBuildInfo, and
    // a peer that did not declare it is sent none; the entry reads back.
    void radeReasonVersion_onlyToADeclaringPeer()
    {
        // coreBuildInfo is sent only when the Core knows its version.
        const QString savedVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&savedVersion]() {
            QCoreApplication::setApplicationVersion(savedVersion);
        });
        QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rxFilterLowPass"), 1);
        asks.insert(QByteArrayLiteral("radeReason"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("radeReasonVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("radeReasonVersion")).has_value());
        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 3);
        QCOMPARE(caps.at(caps.size() - 3).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("rxFilterLowPassVersion"));
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radeReasonVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        StationCapabilities sent;
        sent.radeReasonVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).radeReasonVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .radeReasonVersion, 0);
    }

    // Core-to-device only, gated on radeReason 1.
    void policy_outboundAndGated()
    {
        QVERIFY(MirrorPolicy::hasExplicitEntry("SliceModel", "radeReason"));
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "radeReason"),
                 MirrorDirection::Outbound);
        QVERIFY(!MirrorPolicy::inboundAllowed("SliceModel", "radeReason"));
        const MirrorPolicy::FeatureGate* gate =
            MirrorPolicy::featureGateFor("SliceModel", "radeReason");
        QVERIFY(gate != nullptr);
        QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("radeReason"));
        QCOMPARE(gate->minVersion, 1);
    }

    // A declaring peer (the phone) is sent the reason and its clearing; one
    // that did not declare it never sees the name.
    void reason_onlyToADeclaringPeer()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("radeReason"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));

        failSliceARadeStart(*core.model);
        QCOMPARE(core.model->sliceById(0)->radeReason(), kCreateFailed);
        QVERIFY(core.model->wdspEngine()->radeChannel(0) == nullptr);
        const QString key = QString::fromLatin1(ObjectRegistry::keyForSlice(0));
        QTRY_COMPARE(latest(appA->received(), key, QStringLiteral("radeReason")).toString(),
                     kCreateFailed);

        core.model->wdspEngine()->setRadeCreateFailsForTest(false);
        core.model->sliceById(0)->setDspMode(DSPMode::USB);
        QVERIFY(core.model->sliceById(0)->radeReason().isEmpty());
        QTRY_VERIFY(latest(appA->received(), key, QStringLiteral("radeReason"))
                        .toString().isEmpty());
        QTest::qWait(100);
        QVERIFY(!sawProperty(appB, QStringLiteral("radeReason")));
    }

    // A remote window (which declares radeReason) carries the Core's
    // reason on its mirrored slice, and the reason clears with the Core's.
    void remoteWindow_showsTheCoresReason()
    {
        Core core(/*upgradedWithToken=*/true);
        failSliceARadeStart(*core.model);
        QCOMPARE(core.model->sliceById(0)->radeReason(), kCreateFailed);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("rade-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("rade-client"), this);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), 1);

        QTRY_VERIFY(remote.sliceById(0) != nullptr);
        SliceModel* const mirrored = remote.sliceById(0);
        QTRY_COMPARE(mirrored->dspMode(), DSPMode::RADE_U);
        QTRY_COMPARE(mirrored->radeReason(), kCreateFailed);
        // The remote window makes no decoder of its own.
        QVERIFY(remote.wdspEngine() == nullptr
                || remote.wdspEngine()->radeChannel(0) == nullptr);

        core.model->wdspEngine()->setRadeCreateFailsForTest(false);
        core.model->sliceById(0)->setDspMode(DSPMode::USB);
        QTRY_VERIFY(mirrored->radeReason().isEmpty());
        QTRY_COMPARE(mirrored->dspMode(), DSPMode::USB);
    }
};

QTEST_MAIN(TstRadeReasonLink)
#include "tst_rade_reason_link.moc"
