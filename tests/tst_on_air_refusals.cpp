// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_on_air_refusals.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (the several-devices design, ruling 7.4; D60):
// while the holder is on the air, a change from another device to the
// transmit path (the amplifier and its settings, the tuner, an antenna,
// PureSignal, the interlock, the power cap), a Protocol 1 sample-rate
// change, and a C-Tune move of the holder's transmit slice are refused,
// never asked, with "<holder's short name> is on the air. Try again when
// they stop." The holder's own change is not refused by this rule, and with
// the holder unkeyed none is.
//
// Two layers: the dispatcher's verbs (with the Core's access rule stood in,
// so each verb's own availability checks do not hide the rule), and the
// Core itself over the loopback with two devices (property writes and the
// verbs that need no accessory).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave I2: the freeze and the station
//               PTT's refusal while a device holds transmit. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave 2, Important 1: the Core's own
//               key is a holder on the air like any device's; the holder
//               changes its transmit antennas on the air; the saved
//               accessory addresses go ahead; a hosting desktop's key is
//               named after the desktop. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               radio's PTT takes transmit; the station keeps it after its
//               key. J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: the Tune Power command, TX profile save and delete and the
//               RADE vocoder reset are the holder's while transmit is held.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/session/SessionCommandDispatcher.h"

namespace {

const QString kOnAir = QStringLiteral("iPhone is on the air. Try again when they stop.");

struct Dispatch {
    RadioModel model;
    SessionCommandDispatcher dispatcher{&model};
    bool keyed{true};
    QList<SessionMessage> results;
    quint32 nextId{1};

    Dispatch()
    {
        model.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
        info.boardType = HPSDRHW::HermesLite;
        info.protocol = ProtocolVersion::Protocol1;
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.addSlice(QStringLiteral("pan-0"));
        SessionCommandDispatcher::TransmitAccess access;
        access.onAir = [this](const QByteArray& requester) -> TxRefusal {
            if (!keyed || requester == "phone") {
                return {};
            }
            return TxRefusals::holderOnAir(QStringLiteral("iPhone"), false);
        };
        dispatcher.setTransmitAccess(access);
        QObject::connect(&dispatcher, &SessionCommandDispatcher::commandResultReady,
                         [this](const SessionMessage& m) { results.append(m); });
    }

    SessionMessage invoke(const QByteArray& requester, const QByteArray& verb,
                          const QList<MirrorUpdate>& arguments = {})
    {
        results.clear();
        dispatcher.setRequester(requester);
        dispatcher.dispatch(SessionMessages::commandInvoke(verb, nextId++, arguments));
        dispatcher.setRequester({});
        return results.isEmpty() ? SessionMessage{} : results.first();
    }
};

bool refusedOnAir(const SessionMessage& result)
{
    if (result.accepted || result.reason != kOnAir) {
        return false;
    }
    bool code = false;
    for (const MirrorUpdate& value : result.updates) {
        code = code || (value.name == "refusalCode" && value.value.toString() == QStringLiteral("holderOnAir"));
    }
    return code;
}

struct Verb {
    QByteArray name;
    QList<MirrorUpdate> arguments;
};

QList<Verb> transmitPathVerbs()
{
    return {
        {"configurePgxl", {utf8("host", QStringLiteral("192.0.2.9")), int64("port", 9008)}},
        {"setPgxlName", {utf8("name", QStringLiteral("Amp"))}},
        {"configureTgxl", {utf8("host", QStringLiteral("192.0.2.9")), int64("port", 9010)}},
        {"setTgxlAntenna", {int64("port", 2)}},
        {"setAlexRxAntenna", {int64("band", 5), int64("antenna", 2),
                              MirrorUpdate{0, "rxOnly", MirrorWireKind::Bool, QVariant(false)}}},
        {"ps3.off", {}},
        {"setTxInterlockPolicy", {int64("mode", 2), int64("graceMs", 1000),
                                  MirrorUpdate{0, "swrGateEnabled", MirrorWireKind::Bool, QVariant(true)},
                                  f64("swrGateMax", 2.5)}},
        {"setPgxlPowerCap", {MirrorUpdate{0, "enabled", MirrorWireKind::Bool, QVariant(true)},
                             int64("watts", 800)}},
        {"requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)}},
        {"requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 14150000.0)}},
    };
}

} // namespace

class TstOnAirRefusals : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxRefusal>(); }

    // ---- The dispatcher's verbs ---------------------------------------------

    void anotherDevicesTransmitPathChangeIsRefusedWhileTheHolderIsOnTheAir()
    {
        Dispatch d;
        for (const Verb& verb : transmitPathVerbs()) {
            const SessionMessage result = d.invoke("pad", verb.name, verb.arguments);
            QVERIFY2(refusedOnAir(result),
                     qPrintable(QString::fromLatin1(verb.name) + QStringLiteral(": ") + result.reason));
        }
    }

    void theHoldersOwnChangeIsNotRefusedByTheRule()
    {
        Dispatch d;
        for (const Verb& verb : transmitPathVerbs()) {
            const SessionMessage result = d.invoke("phone", verb.name, verb.arguments);
            QVERIFY2(result.reason != kOnAir, verb.name.constData());
        }
    }

    void withTheHolderUnkeyedNoneIsRefusedByTheRule()
    {
        Dispatch d;
        d.keyed = false;
        for (const Verb& verb : transmitPathVerbs()) {
            const SessionMessage result = d.invoke("pad", verb.name, verb.arguments);
            QVERIFY2(result.reason != kOnAir, verb.name.constData());
        }
    }

    void twoToneAndAProtocol2RateChangeAreNotThisRules()
    {
        Dispatch d;
        QVERIFY(d.invoke("pad", "ps3.twoTone",
                         {MirrorUpdate{0, "enabled", MirrorWireKind::Bool, QVariant(true)}})
                    .reason != kOnAir);
        RadioInfo info = d.model.currentRadioInfo();
        info.protocol = ProtocolVersion::Protocol2;
        d.model.setLastRadioInfoForTest(info);
        QVERIFY(d.invoke("pad", "requestSliceSampleRate",
                         {int64("sliceId", 0), int64("rateHz", 96000)}).reason != kOnAir);
    }

    void aCTuneMoveOffTheTransmitSlicesReceiverIsNotThisRules()
    {
        Dispatch d;
        d.model.configureStreamPool(5, 5, 192000);
        const int far = d.model.addSlice(QStringLiteral("pan-0"));
        d.model.sliceById(0)->setFrequency(14200000.0);
        d.model.sliceById(far)->setFrequency(7100000.0);
        QVERIFY(d.model.txBoundSlice() == d.model.sliceById(0));
        QVERIFY(d.model.sliceById(far)->streamIndex() != d.model.sliceById(0)->streamIndex());
        QVERIFY(d.invoke("pad", "requestStreamCentre",
                         {int64("sliceId", far), f64("centreHz", 7150000.0)}).reason != kOnAir);
        QVERIFY(refusedOnAir(d.invoke("pad", "requestStreamCentre",
                                      {int64("sliceId", 0), f64("centreHz", 14150000.0)})));
    }

    // Ruling 7.7 (scoped review of transmitSettingsVersion 13): the
    // transmitter's own settings are the holder's while transmit is held,
    // on the air or not. txProfile.select always followed it; the Tune
    // Power command, TX profile save and delete and the RADE vocoder reset
    // follow it too now that the air no longer refuses them.
    void theTransmittersOwnSettingsAreTheHolders()
    {
        Dispatch d;
        SessionCommandDispatcher::TransmitAccess access;
        access.transmitter = [](const QByteArray& requester) -> TxRefusal {
            return requester == "phone" ? TxRefusal{}
                                        : TxRefusals::otherDeviceHolds(QStringLiteral("iPhone"));
        };
        d.dispatcher.setTransmitAccess(access);
        const QString holds = TxRefusals::otherDeviceHolds(QStringLiteral("iPhone")).text;
        const QList<Verb> verbs{
            {"setTunePowerForTxBand", {int64("watts", 5)}},
            {"txProfile.select", {utf8("name", QStringLiteral("Default"))}},
            {"txProfile.save", {utf8("name", QStringLiteral("Mine"))}},
            {"txProfile.delete", {utf8("name", QStringLiteral("Mine"))}},
            {"rade.resetVocoder", {}},
        };
        for (const Verb& verb : verbs) {
            const SessionMessage other = d.invoke("pad", verb.name, verb.arguments);
            QVERIFY2(!other.accepted && other.reason == holds,
                     qPrintable(QString::fromLatin1(verb.name) + QStringLiteral(": ")
                                + other.reason));
            const SessionMessage own = d.invoke("phone", verb.name, verb.arguments);
            QVERIFY2(own.reason != holds, verb.name.constData());
        }
    }

    // transmitSettingsVersion 13: the holder's own Tune Power change is
    // taken while the radio is on the air when the Core says the peer may
    // change the transmit settings; without that, the air refuses it.
    void theHoldersTunePowerIsTakenOnTheAir()
    {
        Dispatch d;
        SessionCommandDispatcher::TransmitAccess access;
        access.transmitter = [](const QByteArray& requester) -> TxRefusal {
            return requester == "phone" ? TxRefusal{}
                                        : TxRefusals::otherDeviceHolds(QStringLiteral("iPhone"));
        };
        d.dispatcher.setTransmitAccess(access);
        d.model.transmitModel().setTune(true);
        QVERIFY(d.model.stationOnAirRefusal(nullptr));
        d.dispatcher.setTransmitSettingsOnAir(false);
        SessionMessage result = d.invoke("phone", "setTunePowerForTxBand", {int64("watts", 5)});
        QCOMPARE(result.reason, RadioModel::onAirReason());
        d.dispatcher.setTransmitSettingsOnAir(true);
        result = d.invoke("phone", "setTunePowerForTxBand", {int64("watts", 5)});
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(d.model.transmitModel().tunePowerForTxBand(), 5);
        d.model.transmitModel().setTune(false);
    }

    // ---- The Core, two devices over the loopback -------------------------------

    void propertyWritesToTheTransmitPathWaitWhileTheHolderIsOnTheAir()
    {
        Core core;
        allowTransmit(core);
        Device a(QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA) && admitted(appB));
        const QStringList bSlices = heldKeys(appB, QStringLiteral("slice:"));
        QCOMPARE(bSlices.size(), 1);
        const QByteArray bSlice = bSlices.first().toUtf8();

        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        qint64 writeId = 100;
        const auto write = [&](LoopbackTransport* app, const QByteArray& key,
                               const MirrorUpdate& update) -> QJsonObject {
            const qint64 id = ++writeId;
            app->sendText(SessionMessages::encode(
                SessionMessages::propertyWrite(key, {update}, static_cast<quint32>(id))));
            const bool answered =
                QTest::qWaitFor([app, id]() { return !propertyResult(app, id).isEmpty(); }, 5000);
            Q_UNUSED(answered);
            const QJsonArray results = propertyResult(app, id).value(QStringLiteral("results")).toArray();
            return results.isEmpty() ? QJsonObject{} : results.first().toObject();
        };

        // B: its own slice's antennas, PureSignal, the transmitter's
        // PureSignal switch.
        for (const auto& [key, update] :
             QList<QPair<QByteArray, MirrorUpdate>>{
                 {bSlice, utf8("rxAntenna", QStringLiteral("ANT2"))},
                 {bSlice, utf8("txAntenna", QStringLiteral("ANT2"))},
                 {"pureSignalSettings", MirrorUpdate{0, "autoAttenuate", MirrorWireKind::Bool, QVariant(false)}},
                 {"transmit", MirrorUpdate{0, "pureSig", MirrorWireKind::Bool, QVariant(true)}}}) {
            const QJsonObject result = write(appB, key, update);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(true), key.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kOnAir);
        }
        // A's own change is not refused by this rule.
        const QStringList aSlices = heldKeys(appA, QStringLiteral("slice:"));
        QVERIFY(!aSlices.isEmpty());
        const QJsonObject own = write(appA, aSlices.first().toUtf8(),
                                      utf8("rxAntenna", QStringLiteral("ANT2")));
        QVERIFY(own.value(QStringLiteral("reason")).toString() != kOnAir);

        // The Protocol 1 rate change and a C-Tune move of the holder's
        // transmit slice's receiver, through the Core.
        QJsonObject r = core.invoke(appB, "requestSliceSampleRate",
                                    {int64("sliceId", bSlice.mid(6).toInt()), int64("rateHz", 96000)});
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kOnAir);
        // A C-Tune move of the holder's transmit slice itself is refused;
        // since the several-devices ownership rule (Task 73, merged from the
        // trunk) the slice's owner answers first: only A changes its slice.
        const int txSlice = core.model->txSliceArbiter()->txBoundSliceId();
        const double centreBefore =
            core.model->streamCentreHz(core.model->sliceById(txSlice)->streamIndex());
        r = core.invoke(appB, "requestStreamCentre",
                        {int64("sliceId", txSlice), f64("centreHz", 14150000.0)});
        QVERIFY(!r.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(r.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That slice belongs to Grant's iPhone. It can be changed only there."));
        QCOMPARE(core.model->streamCentreHz(core.model->sliceById(txSlice)->streamIndex()),
                 centreBefore);

        // Unkeyed: none of them is refused by this rule.
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        const QJsonObject after = write(appB, bSlice, utf8("rxAntenna", QStringLiteral("ANT2")));
        QVERIFY(after.value(QStringLiteral("reason")).toString() != kOnAir);
        r = core.invoke(appB, "requestSliceSampleRate",
                        {int64("sliceId", bSlice.mid(6).toInt()), int64("rateHz", 96000)});
        QVERIFY(r.value(QStringLiteral("reason")).toString() != kOnAir);
    }

    void theRadiosOwnPttSaysTheRadioIsOnTheAir()
    {
        Core core;
        allowTransmit(core);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appB));
        core.model->moxController()->onMicPttFromRadio(true);
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Tx);
        const int txSlice = core.model->txSliceArbiter()->txBoundSliceId();
        const QJsonObject r = core.invoke(appB, "requestStreamCentre",
                                          {int64("sliceId", txSlice), f64("centreHz", 14150000.0)});
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), QStringLiteral("The radio is on the air. Try again when it stops."));
        core.model->moxController()->onMicPttFromRadio(false);
        QTRY_COMPARE(core.model->moxController()->state(), MoxState::Rx);
    }

    // ---- Fix wave 2, Important 1: exempt by change, not by holder -------

    // The Core's own key (its MOX, TUNE or a Tuner Genius hardware TUNE)
    // is a holder on the air like any device's (rulings 7.4, 8.1): another
    // device's transmit antennas, receive antennas and Protocol 1 rate
    // change wait, and the slice it transmits on is frozen.
    // The saved accessory addresses and the LAN scans go ahead on the air
    // (the operator's parity ruling). With the key ended every one applies.
    void theCoresOwnKeyHoldsAnotherDevicesChangesLikeAnyHolder()
    {
        Core core;
        allowTransmit(core);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appB));
        const QStringList bSlices = heldKeys(appB, QStringLiteral("slice:"));
        QCOMPARE(bSlices.size(), 1);
        const QByteArray bSlice = bSlices.first().toUtf8();
        const QString kRadioOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");

        MoxController* mox = core.model->moxController();
        mox->setMox(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const std::optional<TransmitHolder::Holder> holder = core.server->transmitHolder()->holder();
        QVERIFY(holder.has_value() && holder->keyed);
        QCOMPARE(holder->deviceId, QByteArray(KeyerIdentity::kStationDeviceId));
        QVERIFY(holder->source == TransmitHolder::Source::Device);

        qint64 writeId = 800;
        const auto write = [&](const QByteArray& key, const MirrorUpdate& update) {
            const qint64 id = ++writeId;
            appB->sendText(SessionMessages::encode(
                SessionMessages::propertyWrite(key, {update}, static_cast<quint32>(id))));
            const bool answered =
                QTest::qWaitFor([appB, id]() { return !propertyResult(appB, id).isEmpty(); }, 5000);
            Q_UNUSED(answered);
            const QJsonArray results = propertyResult(appB, id).value(QStringLiteral("results")).toArray();
            return results.isEmpty() ? QJsonObject{} : results.first().toObject();
        };
        for (const MirrorUpdate& update : {utf8("rxAntenna", QStringLiteral("ANT2")),
                                           utf8("txAntenna", QStringLiteral("ANT2"))}) {
            const QJsonObject result = write(bSlice, update);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(true), update.name.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        }
        const int txAnt = core.model->alexController().txAnt(Band::Band20m);
        QJsonObject r = core.invoke(appB, "setAlexTxAntenna",
                                    {int64("band", static_cast<int>(Band::Band20m)),
                                     int64("antenna", txAnt == 2 ? 3 : 2)});
        QCOMPARE(r.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), txAnt);
        r = core.invoke(appB, "requestSliceSampleRate",
                        {int64("sliceId", bSlice.mid(6).toInt()), int64("rateHz", 96000)});
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        // (The amplifier and tuner switches, and the saved addresses going
        // ahead, over a Core with the accessories: tst_remote_peripherals.)

        // The saved addresses and the scans are not this rule's.
        r = core.invoke(appB, "setPgxlAddress",
                        {utf8("host", QStringLiteral("192.0.2.88")), int64("port", 9008)});
        QVERIFY2(r.value(QStringLiteral("reason")).toString() != kRadioOnAir,
                 qPrintable(r.value(QStringLiteral("reason")).toString()));
        QVERIFY(r.value(QStringLiteral("reason")).toString()
                != QStringLiteral("Waiting for you to confirm."));
        r = core.invoke(appB, "scanPgxlLan");
        QVERIFY(r.value(QStringLiteral("reason")).toString() != kRadioOnAir);

        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        // Task 77 (ruling 8.1): the Core keeps transmit, unkeyed, after its
        // own key ends; off the air the change goes ahead.
        QTRY_VERIFY(core.server->transmitHolder()->holder().has_value()
                    && !core.server->transmitHolder()->holder()->keyed);
        const QJsonObject after = write(bSlice, utf8("txAntenna", QStringLiteral("ANT2")));
        QVERIFY2(after.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(after.value(QStringLiteral("reason")).toString()));
        r = core.invoke(appB, "setAlexTxAntenna",
                        {int64("band", static_cast<int>(Band::Band20m)),
                         int64("antenna", txAnt == 2 ? 3 : 2)});
        QVERIFY2(r.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(r.value(QStringLiteral("reason")).toString()));
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), txAnt == 2 ? 3 : 2);
    }

    // Ruling 8.1: a desktop that hosts the Core names its own key after
    // itself, on txState and in the on-air refusal.
    void aHostingDesktopsKeyIsNamedAfterTheDesktop()
    {
        Core core;
        allowTransmit(core);
        core.server->setStationDeviceWords(QStringLiteral("Shack Mac mini"), QStringLiteral("Shack"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appB));
        MoxController* mox = core.model->moxController();
        mox->setMox(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const std::optional<TransmitHolder::Holder> holder = core.server->transmitHolder()->holder();
        QVERIFY(holder.has_value());
        QCOMPARE(holder->name, QStringLiteral("Shack Mac mini"));
        QCOMPARE(holder->shortName, QStringLiteral("Shack"));
        QCOMPARE(holder->kind, QStringLiteral("station"));
        QTRY_COMPARE(latest(appB->received(), QStringLiteral("txState"),
                            QStringLiteral("holderName")).toString(),
                     QStringLiteral("Shack Mac mini"));
        QCOMPARE(latest(appB->received(), QStringLiteral("txState"),
                        QStringLiteral("holderSource")).toString(),
                 QStringLiteral("device"));
        QCOMPARE(latest(appB->received(), QStringLiteral("txState"),
                        QStringLiteral("holderDeviceId")).toString(),
                 QStringLiteral("station"));
        const QJsonObject r = core.invoke(appB, "setAlexTxAntenna",
                                          {int64("band", static_cast<int>(Band::Band20m)),
                                           int64("antenna", 2)});
        QCOMPARE(r.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Shack is on the air. Try again when they stop."));
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        // Task 77 (ruling 8.1): a press of the radio's own PTT while the
        // station device already holds transmit is a key, not a take: the
        // names and the source stay the desktop's.
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(core.server->transmitHolder()->holder()->name, QStringLiteral("Shack Mac mini"));
        QCOMPARE(core.server->transmitHolder()->holder()->source, TransmitHolder::Source::Device);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        // On unheld transmit it is "Radio", source radioPtt.
        Core fresh;
        allowTransmit(fresh);
        fresh.server->setStationDeviceWords(QStringLiteral("Shack Mac mini"), QStringLiteral("Shack"));
        fresh.model->moxController()->onMicPttFromRadio(true);
        QTRY_COMPARE(fresh.model->moxController()->state(), MoxState::Tx);
        QCOMPARE(fresh.server->transmitHolder()->holder()->name, QStringLiteral("Radio"));
        QCOMPARE(fresh.server->transmitHolder()->holder()->source,
                 TransmitHolder::Source::RadioPtt);
        fresh.model->moxController()->onMicPttFromRadio(false);
        QTRY_COMPARE(fresh.model->moxController()->state(), MoxState::Rx);
    }

    // The controller's ruling (as in Thetis, for the operator who is
    // transmitting): the device that holds transmit changes the transmit
    // antennas on the air; another device's change waits.
    void theHolderChangesTheTransmitAntennasOnTheAir()
    {
        Core core;
        allowTransmit(core);
        Device a(QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA) && admitted(appB));
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const int txAnt = core.model->alexController().txAnt(Band::Band20m);
        const int next = txAnt == 2 ? 3 : 2;
        QJsonObject r = core.invoke(appB, "setAlexTxAntenna",
                                    {int64("band", static_cast<int>(Band::Band20m)),
                                     int64("antenna", next)});
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kOnAir);
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), txAnt);
        r = core.invoke(appA, "setAlexTxAntenna",
                        {int64("band", static_cast<int>(Band::Band20m)), int64("antenna", next)});
        QVERIFY2(r.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(r.value(QStringLiteral("reason")).toString()));
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), next);
        QVERIFY(mox->isMox());
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // ---- Fix wave I2 (rulings 8.11 and 8.9) ------------------------------

    // Ruling 8.11 (D64): while the radio's own PTT keys the transmit slice
    // (another device's), that slice is frozen for every device, its owner
    // included: frequency, mode, filter, band, transmit antenna, closing.
    // The freeze ends with the press.
    void theRadiosPttFreezesTheTransmitSlice()
    {
        Core core;
        allowTransmit(core);
        Device a(QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        const int txSlice = core.model->txSliceArbiter()->txBoundSliceId();
        QVERIFY(txSlice >= 0);
        QCOMPARE(core.model->sliceOwnership()->mark(txSlice).owner, a.key.fingerprint());
        const QByteArray key = QByteArrayLiteral("slice:") + QByteArray::number(txSlice);
        const QString kRadioOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        qint64 writeId = 700;
        const auto write = [&](const MirrorUpdate& update) {
            const qint64 id = ++writeId;
            appA->sendText(SessionMessages::encode(
                SessionMessages::propertyWrite(key, {update}, static_cast<quint32>(id))));
            const bool answered =
                QTest::qWaitFor([appA, id]() { return !propertyResult(appA, id).isEmpty(); }, 5000);
            Q_UNUSED(answered);
            const QJsonArray results = propertyResult(appA, id).value(QStringLiteral("results")).toArray();
            return results.isEmpty() ? QJsonObject{} : results.first().toObject();
        };
        MoxController* mox = core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const double before = core.model->sliceById(txSlice)->frequency();
        for (const MirrorUpdate& update :
             {MirrorUpdate{0, "frequency", MirrorWireKind::Float64, QVariant(14250000.0)},
              MirrorUpdate{0, "dspMode", MirrorWireKind::Enum, QVariant(qlonglong(0))},
              MirrorUpdate{0, "filterLow", MirrorWireKind::Int64, QVariant(qlonglong(200))},
              MirrorUpdate{0, "filterHigh", MirrorWireKind::Int64, QVariant(qlonglong(2500))},
              MirrorUpdate{0, "txAntenna", MirrorWireKind::Utf8, QVariant(QStringLiteral("ANT2"))}}) {
            const QJsonObject result = write(update);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(true), update.name.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        }
        QCOMPARE(core.model->sliceById(txSlice)->frequency(), before);
        const QJsonObject close = core.invoke(appA, "removeSlice", {int64("sliceId", txSlice)});
        QVERIFY(!close.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(close.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        const QJsonObject band = core.invoke(appA, "slice.selectBand",
                                             {int64("sliceId", txSlice), int64("band", 3)});
        QVERIFY(!band.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(band.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QVERIFY(core.model->sliceById(txSlice) != nullptr);
        // The press ends: the owner retunes again.
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        const QJsonObject after =
            write(MirrorUpdate{0, "frequency", MirrorWireKind::Float64, QVariant(14250000.0)});
        QVERIFY2(after.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(after.value(QStringLiteral("reason")).toString()));
    }

    // Task 77 (rulings 8.8, 8.9): the radio's own PTT takes transmit from a
    // device that holds it, unkeyed, present or away, without a question;
    // the press then keys as the station device.
    void theRadiosPttTakesTransmitFromADeviceThatHoldsIt()
    {
        Core core;
        allowTransmit(core);
        Device a(QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        TransmitHolder* holder = core.server->transmitHolder();
        TransmitHolder::KeyRequest take;
        take.deviceId = a.key.fingerprint();
        QCOMPARE(holder->askKey(take).verdict, KeyingVerdict::Admit);
        MoxController* mox = core.model->moxController();
        QSignalSpy refused(mox, &MoxController::moxRefused);
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(refused.count(), 0);
        QVERIFY(holder->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));
        QCOMPARE(holder->holder()->source, TransmitHolder::Source::RadioPtt);
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(holder->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));

        // Away: A takes it back, then drops; the press still takes.
        const QJsonObject back = core.invoke(appA, "tx.take", {});
        QCOMPARE(back.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Waiting for you to confirm."));
        const QJsonObject asked = firstOfType(appA->received(), QStringLiteral("confirm.request"));
        const QJsonObject proceed = core.invoke(
            appA, "confirm.proceed",
            {int64("id", asked.value(QStringLiteral("id")).toInteger()), int64("choice", -1)});
        QVERIFY2(proceed.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(proceed.value(QStringLiteral("reason")).toString()));
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        appA->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(holder->holder().has_value() && holder->holder()->away);
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        QVERIFY(holder->isHeldBy(QByteArray(KeyerIdentity::kStationDeviceId)));
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }
};

QTEST_MAIN(TstOnAirRefusals)
#include "tst_on_air_refusals.moc"
