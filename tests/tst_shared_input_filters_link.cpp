// no-port-check: NereusSDR-original. No Thetis logic is ported here.
// =================================================================
// tests/tst_shared_input_filters_link.cpp  (NereusSDR)
// =================================================================
//
// Shared-input filters, ruling (d), on the wire: radio's
// rxFilter0LowPassReason and rxFilter0LowPassSlice reach only a peer that
// declared rxFilterLowPass 1, rxFilterLowPassVersion 1 is advertised to
// that peer only, as the last entry before coreBuildInfo, and a remote
// window's RadioModel shows the reason next to the WIDE reason without
// the optional fields changing whether the chain is available.
//
// Nothing here keys a radio or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 - Review fix: the hold is set up with the band-pass forced
//                and the N2ADR pins on the wire, since the HL2's Auto
//                bypass now clears the reason. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - The HL2 hold is set up in Auto again (JJ's ruling: two
//                masks keep the pins of the highest slice). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - A hold alone sends a peer without rxFilterLowPass no
//                radio delta. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QSignalSpy>

#include "core/OcMatrix.h"
#include "core/ReceiverManager.h"
#include "core/accessories/AlexController.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "models/Band.h"

namespace {

// Slice 0 on 80 m and a second slice on 20 m, both on the HL2's one
// receiver input (tst_shared_input_filters' P1Session wiring): the Core's
// own republish holds the low-pass for the 20 m slice. Returns that slice.
int holdOnTwoBands(RadioModel& model)
{
    // On the HL2: different receive pins for 80 m and 20 m. In Auto the
    // pins follow the 20 m slice (JJ's ruling of 2026-09-30) and the 80 m
    // slice is held.
    model.ocMatrixMutable().setPin(Band::Band80m, 2, /*tx=*/false, true);
    model.ocMatrixMutable().setPin(Band::Band20m, 0, /*tx=*/false, true);
    model.configureStreamPool(2, 5, 192000);
    for (int i = 0; i < 2; ++i) {
        model.receiverManager()->createReceiver();
    }
    const int second = model.addSlice(QStringLiteral("pan-0"));
    if (model.sliceById(second) == nullptr) {
        return -1;
    }
    model.sliceById(0)->setFrequency(3700000.0);
    model.sliceById(second)->setFrequency(14200000.0);
    return second;
}

// Both back on 80 m: nothing is held.
void releaseTheHold(RadioModel& model, int second)
{
    model.sliceById(second)->setFrequency(3750000.0);
}

bool sawProperty(const LoopbackTransport* app, const QString& property)
{
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        const QJsonArray lists[] = {o.value(QStringLiteral("properties")).toArray(),
                                    o.value(QStringLiteral("fields")).toArray()};
        for (const QJsonArray& list : lists) {
            for (const QJsonValue& p : list) {
                if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

class TstSharedInputFiltersLink : public QObject {
    Q_OBJECT

private slots:
    // The version reaches a declaring peer, last before coreBuildInfo, and
    // a peer that did not declare it is sent none; the entry reads back.
    void rxFilterLowPassVersion_onlyToADeclaringPeer()
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
        asks.insert(QByteArrayLiteral("radioMic"), 1);
        asks.insert(QByteArrayLiteral("rxFilterLowPass"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("rxFilterLowPassVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("rxFilterLowPassVersion")).has_value());
        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 3);
        QCOMPARE(caps.at(caps.size() - 3).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radioMicVersion"));
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("rxFilterLowPassVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        StationCapabilities sent;
        sent.rxFilterLowPassVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).rxFilterLowPassVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .rxFilterLowPassVersion, 0);
    }

    // Both fields are Core-to-device only and gated on rxFilterLowPass 1.
    void policy_outboundAndGated()
    {
        for (const char* property : {"rxFilter0LowPassReason", "rxFilter0LowPassSlice"}) {
            QVERIFY2(MirrorPolicy::hasExplicitEntry("RadioModel", property), property);
            QCOMPARE(MirrorPolicy::directionFor("RadioModel", property),
                     MirrorDirection::Outbound);
            QVERIFY(!MirrorPolicy::inboundAllowed("RadioModel", property));
            const MirrorPolicy::FeatureGate* gate =
                MirrorPolicy::featureGateFor("RadioModel", property);
            QVERIFY2(gate != nullptr, property);
            QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("rxFilterLowPass"));
            QCOMPARE(gate->minVersion, 1);
        }
    }

    // A declaring peer is sent the held reason and the forcing slice; one
    // that did not declare it never sees either name.
    void fields_onlyToADeclaringPeer()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rxFilterLowPass"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));

        const int second = holdOnTwoBands(*core.model);
        QVERIFY(second > 0);
        const QString held = core.model->rxFilter0LowPassReason();
        QVERIFY(!held.isEmpty());
        QCOMPARE(core.model->rxFilter0LowPassSlice(), second);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0LowPassReason")).toString(), held);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0LowPassSlice")).toInteger(), second);

        releaseTheHold(*core.model, second);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0LowPassSlice")).toInteger(), -1);
        QVERIFY(latest(appA->received(), QStringLiteral("radio"),
                       QStringLiteral("rxFilter0LowPassReason")).toString().isEmpty());

        QVERIFY(!sawProperty(appB, QStringLiteral("rxFilter0LowPassReason")));
        QVERIFY(!sawProperty(appB, QStringLiteral("rxFilter0LowPassSlice")));
    }

    // A hold that starts while the band-pass stays where it was changes
    // nothing a peer without rxFilterLowPass can see, so that peer is sent
    // no radio delta for it: older peers see the wire they saw before
    // (link document, section 17). Hermes, slices on 20 m and 17 m: one
    // 13 MHz high-pass for both, so the chain stays filtered on 20 m, while
    // the 17 m slice holds the low-pass. Then a move to 40 m, which bypasses
    // the band-pass, is a change that peer does see, as one radio delta.
    void holdAlone_sendsAPeerWithoutTheFeatureNoRadioDelta()
    {
        Core core;
        core.model->setBoardForTest(HPSDRHW::Hermes);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rxFilterLowPass"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));

        RadioModel& model = *core.model;
        model.configureStreamPool(2, 5, 192000);
        for (int i = 0; i < 2; ++i) {
            model.receiverManager()->createReceiver();
        }
        const int second = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(second > 0 && model.sliceById(second) != nullptr);
        model.sliceById(0)->setFrequency(14200000.0);
        model.sliceById(second)->setFrequency(14250000.0);
        QCOMPARE(model.rxFilter0LowPassSlice(), -1);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QTRY_COMPARE(latest(appB->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0Band")).toInt(), int(Band::Band20m));
        QTest::qWait(200);
        const qsizetype mark = appB->received().size();

        // The hold alone.
        model.sliceById(second)->setFrequency(18100000.0);
        QCOMPARE(model.rxFilter0LowPassSlice(), second);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QCOMPARE(model.rxFilter0Band(), int(Band::Band20m));
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0LowPassSlice")).toInteger(), second);
        QTest::qWait(200);

        // A change the peer sees.
        model.sliceById(second)->setFrequency(7100000.0);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        QTRY_COMPARE(latest(appB->received(), QStringLiteral("radio"),
                            QStringLiteral("rxFilter0Effective")).toInt(),
                     int(AlexController::BpfEffective::Bypass));
        QTest::qWait(200);

        QList<QJsonObject> radioDeltas;
        const QList<QByteArray> received = appB->received();
        for (qsizetype i = mark; i < received.size(); ++i) {
            const QJsonObject o = QJsonDocument::fromJson(received.at(i)).object();
            if (o.value(QStringLiteral("type")).toString() == QStringLiteral("delta")
                && o.value(QStringLiteral("key")).toString() == QStringLiteral("radio")) {
                radioDeltas.append(o);
            }
        }
        QCOMPARE(radioDeltas.size(), 1);
        QVERIFY(!sawProperty(appB, QStringLiteral("rxFilter0LowPassReason")));
    }

    // A remote window (which declares rxFilterLowPass) shows what the Core
    // holds; the optional fields leave the chain available as before.
    //
    // The WIDE tooltip carries the low-pass sentence only when the chain is
    // bypassed, and on the HL2 a bypass clears the pins and the hold, so
    // this Core is an Alex board (Hermes) in Auto: the band-pass bypasses
    // for the two bands while the Alex low-pass is held for the 20 m slice.
    void remoteWindow_showsTheHeldReason()
    {
        Core core(/*upgradedWithToken=*/true);
        core.model->setBoardForTest(HPSDRHW::Hermes);
        const int second = holdOnTwoBands(*core.model);
        QVERIFY(second > 0);
        const QString held = core.model->rxFilter0LowPassReason();
        QVERIFY(!held.isEmpty());

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("lpf-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("lpf-client"), this);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), 1);

        QTRY_COMPARE(remote.rxFilter0LowPassReason(), held);
        QCOMPARE(remote.rxFilter0LowPassSlice(), second);
        QTRY_VERIFY(remote.filterChainStateAvailable(0));
        const RadioModel::PanBypassState shown = remote.panBypassState({0});
        QVERIFY(shown.bypassed);
        QVERIFY2(shown.reason.endsWith(held), qPrintable(shown.reason));

        releaseTheHold(*core.model, second);
        QTRY_VERIFY(remote.rxFilter0LowPassReason().isEmpty());
        QCOMPARE(remote.rxFilter0LowPassSlice(), -1);
        QVERIFY(remote.filterChainStateAvailable(0));

        // Out of range or too long is refused and leaves the state alone.
        QVERIFY(!remote.applyStationFilterValue("rxFilter0LowPassSlice", 64));
        QVERIFY(!remote.applyStationFilterValue("rxFilter0LowPassSlice", -2));
        QVERIFY(!remote.applyStationFilterValue("rxFilter0LowPassReason",
                                                QString(513, QLatin1Char('x'))));
        QCOMPARE(remote.rxFilter0LowPassSlice(), -1);
        QVERIFY(remote.filterChainStateAvailable(0));
    }
};

QTEST_MAIN(TstSharedInputFiltersLink)
#include "tst_shared_input_filters_link.moc"
