#include "MultiDeviceHarness.h"

#include "core/accessories/AlexController.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/StepAttenuatorController.h"
#include "core/MoxController.h"

namespace {
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:01");
// A phone that knows 2 m (R-IOS-26): 15-entry lists, band 27 in the verbs.
const QHash<QByteArray, int> kRows{{"deviceAuth", 1}, {"sessionHolder", 1},
                                   {"radioAntennaRows", 1}, {"band2m", 1}};
// A phone built before 2 m: the 14 entries without 2 m's.
const QHash<QByteArray, int> kRowsWithout2m{{"deviceAuth", 1}, {"sessionHolder", 1},
                                            {"radioAntennaRows", 1}};
QString without2m(const QString& list)
{
    return list.left(list.lastIndexOf(QLatin1Char(',')));
}
QList<MirrorUpdate> rxArgs(const QString& mac, Band band, int antenna, bool rxOnly = false)
{
    return {utf8("mac", mac), int64("band", static_cast<int>(band)), int64("antenna", antenna),
            MirrorUpdate{0, "rxOnly", MirrorWireKind::Bool, QVariant(rxOnly)}};
}
QList<MirrorUpdate> txArgs(const QString& mac, Band band, int antenna)
{
    return {utf8("mac", mac), int64("band", static_cast<int>(band)), int64("antenna", antenna)};
}
void readyAlex(Core& core, StepAttenuatorController& step)
{
    step.setTickTimerEnabled(false);
    core.model->setStepAttController(&step);
    core.model->setBoardForTest(HPSDRHW::Hermes);
}
}

class TstRadioBoundAntennaRows : public QObject {
    Q_OBJECT
private slots:
    void declaredPeerEditsOneBand()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        Device a(QStringLiteral("A"), QStringLiteral("phone"));
        Device b(QStringLiteral("B"), QStringLiteral("phone"));
        core.pair(a);
        core.pair(b);
        auto* first = core.signIn(a, kRows);
        QVERIFY(admitted(first));
        QCOMPARE(capability(first->received(), QStringLiteral("radioAntennaRowsVersion")),
                 std::optional<qint64>(1));

        const auto rx = core.invoke(first, "setAlexRxAntennaForRadio",
                                    rxArgs(kMac, Band::Band40m, 2));
        QVERIFY2(rx.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(rx.value(QStringLiteral("reason")).toString()));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QVERIFY(core.invoke(first, "setAlexRxAntennaForRadio",
                            rxArgs(kMac, Band::Band20m, 3)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band20m), 3);
        QVERIFY(core.invoke(first, "setAlexRxAntennaForRadio",
                            rxArgs(kMac, Band::Band40m, 2, true)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().rxOnlyAnt(Band::Band40m), 2);
        // 2 m has antennas of its own (band 27, R-IOS-26).
        QVERIFY(core.invoke(first, "setAlexRxAntennaForRadio",
                            rxArgs(kMac, Band::Band2m, 3)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().rxAnt(Band::Band2m), 3);
        QCOMPARE(core.model->alexController().rxAnt(Band::GEN), 1);
        QVERIFY(core.model->alexAntennaFacade()->rxAntennas().endsWith(QStringLiteral(",3")));
        QVERIFY(core.invoke(first, "setAlexTxAntennaForRadio",
                            txArgs(kMac, Band::Band40m, 3)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 3);
        core.model->alexControllerMutable().setBlockTxAnt3(true);
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 1);
        QVERIFY(!core.invoke(first, "setAlexTxAntennaForRadio",
                             txArgs(kMac, Band::Band40m, 3)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 1);
        core.model->alexControllerMutable().setBlockTxAnt2(true);
        QVERIFY(!core.invoke(first, "setAlexTxAntennaForRadio",
                             txArgs(kMac, Band::Band40m, 2)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 1);

        // A phone built before 2 m reads the lists without 2 m's entry.
        auto* second = core.signIn(b, kRowsWithout2m);
        QVERIFY(admitted(second));
        QCOMPARE(capability(second->received(), QStringLiteral("radioAntennaRowsVersion")),
                 std::optional<qint64>(1));
        QCOMPARE(capability(second->received(), QStringLiteral("band2mVersion")), std::nullopt);
        QCOMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                        QStringLiteral("rxAntennas")).toString(),
                 without2m(core.model->alexAntennaFacade()->rxAntennas()));
        QCOMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                        QStringLiteral("rxOnlyAntennas")).toString(),
                 without2m(core.model->alexAntennaFacade()->rxOnlyAntennas()));
        QCOMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                        QStringLiteral("txAntennas")).toString(),
                 without2m(core.model->alexAntennaFacade()->txAntennas()));
        // The phone that knows 2 m was offered it.
        QCOMPARE(capability(first->received(), QStringLiteral("band2mVersion")),
                 std::optional<qint64>(1));
        const int capabilitiesBefore = ofType(first->received(), QStringLiteral("capabilities")).size();
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_VERIFY(ofType(first->received(), QStringLiteral("capabilities")).size()
                    > capabilitiesBefore);
        const QJsonObject withdrawn =
            ofType(first->received(), QStringLiteral("capabilities")).last();
        for (const QJsonValue& value : withdrawn.value(QStringLiteral("properties")).toArray()) {
            QVERIFY(value.toObject().value(QStringLiteral("name")).toString()
                    != QStringLiteral("radioAntennaRowsVersion"));
        }
    }

    void missingFeatureAndWrongIdentityRefuseWithoutChangingModel()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        Device a;
        core.pair(a);
        auto* oldPeer = core.signIn(a);
        QVERIFY(admitted(oldPeer));
        QVERIFY(!capability(oldPeer->received(), QStringLiteral("radioAntennaRowsVersion")));
        QVERIFY(!core.invoke(oldPeer, "setAlexRxAntennaForRadio",
                             rxArgs(kMac, Band::Band40m, 2)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 1);

        // An Alex-only test Core lacks the existing complete antenna path.
        // The new row feature does not bypass that readiness boundary.
        Core stripped;
        stripped.model->setBoardForTest(HPSDRHW::Hermes);
        Device strippedDevice;
        stripped.pair(strippedDevice);
        auto* strippedPeer = stripped.signIn(strippedDevice, kRows);
        QVERIFY(admitted(strippedPeer));
        QVERIFY(!capability(strippedPeer->received(), QStringLiteral("radioAntennaRowsVersion")));
        const QJsonObject strippedRefusal = stripped.invoke(strippedPeer,
            "setAlexRxAntennaForRadio", rxArgs(kMac, Band::Band40m, 2));
        QVERIFY(!strippedRefusal.value("accepted").toBool());
        QCOMPARE(strippedRefusal.value("reason").toString(),
                 QStringLiteral("The Core has no antenna settings ready."));
        QCOMPARE(stripped.model->alexController().rxAnt(Band::Band40m), 1);

        Core unsupported;
        Device other;
        unsupported.pair(other);
        auto* noBoard = unsupported.signIn(other, kRows);
        QVERIFY(admitted(noBoard));
        QVERIFY(!capability(noBoard->received(), QStringLiteral("radioAntennaRowsVersion")));
        QVERIFY(!unsupported.invoke(noBoard, "setAlexRxAntennaForRadio",
                                    rxArgs(kMac, Band::Band40m, 2)).value("accepted").toBool());

        StepAttenuatorController oldStep;
        Core oldMinor;
        readyAlex(oldMinor, oldStep);
        Device legacy;
        oldMinor.pair(legacy);
        auto* beforeEleven = oldMinor.signIn(legacy, kRows, 10);
        QVERIFY(admitted(beforeEleven));
        QVERIFY(!capability(beforeEleven->received(), QStringLiteral("radioAntennaRowsVersion")));

        StepAttenuatorController unknownStep;
        Core unknownVersion;
        readyAlex(unknownVersion, unknownStep);
        Device future;
        unknownVersion.pair(future);
        auto futureFeature = kRows;
        futureFeature.insert("radioAntennaRows", 2);
        auto* unsupportedVersion = unknownVersion.signIn(future, futureFeature);
        QVERIFY(admitted(unsupportedVersion));
        QVERIFY(!capability(unsupportedVersion->received(),
                            QStringLiteral("radioAntennaRowsVersion")));
        QVERIFY(!unknownVersion.invoke(unsupportedVersion, "setAlexRxAntennaForRadio",
                                       rxArgs(kMac, Band::Band40m, 2))
                     .value("accepted").toBool());
    }

    void malformedAndSwitchedRadioRefuse()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        Device a;
        core.pair(a);
        auto* app = core.signIn(a, kRows);
        QVERIFY(admitted(app));
        const auto refuse = [&](const QList<MirrorUpdate>& args) {
            const QJsonObject result = core.invoke(app, "setAlexRxAntennaForRadio", args);
            QVERIFY(!result.isEmpty());
            QVERIFY(!result.value("accepted").toBool());
            QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 1);
        };
        refuse(rxArgs(QString(), Band::Band40m, 2));
        refuse(rxArgs(QStringLiteral("aa:bb:cc:dd:ee:01"), Band::Band40m, 2));
        refuse(rxArgs(QStringLiteral("AA:BB:CC:DD:EE:02"), Band::Band40m, 2));
        auto missing = rxArgs(kMac, Band::Band40m, 2);
        missing.removeFirst();
        refuse(missing);
        auto extra = rxArgs(kMac, Band::Band40m, 2);
        extra.append(int64("extra", 1));
        refuse(extra);
        auto wrongKind = rxArgs(kMac, Band::Band40m, 2);
        wrongKind[0].kind = MirrorWireKind::Int64;
        refuse(wrongKind);
        auto wrongBandKind = rxArgs(kMac, Band::Band40m, 2);
        wrongBandKind[1].kind = MirrorWireKind::Bool;
        refuse(wrongBandKind);
        auto wrongOnlyKind = rxArgs(kMac, Band::Band40m, 2);
        wrongOnlyKind[3].kind = MirrorWireKind::Int64;
        refuse(wrongOnlyKind);
        refuse(rxArgs(kMac, Band::Count, 2));
        refuse(rxArgs(kMac, Band::Band40m, 7));

        RadioInfo other;
        other.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
        other.boardType = HPSDRHW::HermesLite;
        core.model->setLastRadioInfoForTest(other);
        refuse(rxArgs(kMac, Band::Band40m, 2));
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        refuse(rxArgs(kMac, Band::Band40m, 2));
    }

    void confirmationRechecksTheRadioBeforeReadingItsRow()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        core.model->configureStreamPool(3, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        auto* first = core.signIn(a, kRows);
        auto* second = core.signIn(b, kRows);
        QVERIFY(admitted(first));
        QVERIFY(admitted(second));

        first->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "setAlexRxAntennaForRadio", 501, rxArgs(kMac, Band::Band40m, 2))));
        QJsonObject question;
        QTRY_VERIFY_WITH_TIMEOUT(([&]() {
            question = firstOfType(first->received(), QStringLiteral("confirm.request"));
            return !question.isEmpty();
        })(), 5000);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 1);

        RadioInfo other;
        other.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
        other.boardType = HPSDRHW::HermesLite;
        core.model->setLastRadioInfoForTest(other);
        const auto answer = core.invoke(first, "confirm.proceed",
            {int64("id", question.value(QStringLiteral("id")).toInteger()),
             int64("choice", -1)});
        QVERIFY(!answer.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 1);
        // The A->B->A case names A again and uses the usual row policy.
        other.macAddress = kMac;
        core.model->setLastRadioInfoForTest(other);
        QVERIFY(core.invoke(first, "setAlexTxAntennaForRadio",
                            txArgs(kMac, Band::Band20m, 2)).value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), 2);
    }

    void admittedDevicesKeepDifferentQueuedRowsAndInvalidEditsAskNobody()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        core.model->configureStreamPool(3, 5, 192000);
        Device a(QStringLiteral("A"), QStringLiteral("phone"));
        Device b(QStringLiteral("B"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        auto* first = core.signIn(a, kRows);
        auto* second = core.signIn(b, kRows);
        QVERIFY(admitted(first) && admitted(second));

        const int askedBefore = ofType(first->received(), QStringLiteral("confirm.request")).size();
        QVERIFY(!core.invoke(first, "setAlexRxAntennaForRadio",
                             rxArgs(kMac, Band::Band40m, 7)).value("accepted").toBool());
        QVERIFY(!core.invoke(first, "setAlexRxAntennaForRadio",
                             rxArgs(kMac, Band::Band40m, 4, true)).value("accepted").toBool());
        QVERIFY(!core.invoke(first, "setAlexTxAntennaForRadio",
                             txArgs(kMac, Band::Band40m, 0)).value("accepted").toBool());
        QCOMPARE(ofType(first->received(), QStringLiteral("confirm.request")).size(),
                 askedBefore);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 1);
        QCOMPARE(core.model->alexController().rxOnlyAnt(Band::Band40m), 0);
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 1);

        first->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "setAlexRxAntennaForRadio", 701, rxArgs(kMac, Band::Band40m, 2))));
        second->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "setAlexRxAntennaForRadio", 702, rxArgs(kMac, Band::Band20m, 3))));
        QTRY_VERIFY(ofType(first->received(), QStringLiteral("confirm.request")).size()
                    > askedBefore);
        QTRY_VERIFY(!ofType(second->received(), QStringLiteral("confirm.request")).isEmpty());
        const QJsonObject firstQuestion =
            ofType(first->received(), QStringLiteral("confirm.request")).last();
        const QJsonObject secondQuestion =
            ofType(second->received(), QStringLiteral("confirm.request")).last();
        const auto proceed = [&](LoopbackTransport* app, const QJsonObject& question) {
            return core.invoke(app, "confirm.proceed",
                {int64("id", question.value(QStringLiteral("id")).toInteger()),
                 int64("choice", -1)});
        };
        const auto firstAccepted = proceed(first, firstQuestion);
        QVERIFY2(firstAccepted.value("accepted").toBool(),
                 qPrintable(firstAccepted.value("reason").toString()));
        const auto secondAccepted = proceed(second, secondQuestion);
        QVERIFY2(secondAccepted.value("accepted").toBool(),
                 qPrintable(secondAccepted.value("reason").toString()));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band20m), 3);
        const QString rows = core.model->alexAntennaFacade()->rxAntennas();
        QTRY_COMPARE(latest(first->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("rxAntennas")).toString(), rows);
        QTRY_COMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("rxAntennas")).toString(), rows);
    }

    void unknownAndMalformedVersionAreNotUsable()
    {
        StationCapabilities offered;
        offered.radioIdentityEntries = true;
        offered.radioAntennaRowsVersion = 1;
        auto updates = offered.toUpdates();
        QCOMPARE(StationCapabilities::fromUpdates(updates).radioAntennaRowsVersion, 1);
        for (auto& entry : updates) {
            if (entry.name != "radioAntennaRowsVersion") {
                continue;
            }
            entry.value = QVariant(qlonglong(2));
            QCOMPARE(StationCapabilities::fromUpdates(updates).radioAntennaRowsVersion, 0);
            entry.kind = MirrorWireKind::Utf8;
            entry.value = QVariant(QStringLiteral("1"));
            QCOMPARE(StationCapabilities::fromUpdates(updates).radioAntennaRowsVersion, 0);
            return;
        }
        QFAIL("The offered version was not appended.");
    }

    void withdrawalSurvivesSynchronousSessionReplacement()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        Device a(QStringLiteral("A"), QStringLiteral("phone"));
        Device b(QStringLiteral("B"), QStringLiteral("tablet"));
        Device c(QStringLiteral("C"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(b);
        core.pair(c);
        LoopbackTransport* first = core.signIn(a, kRows);
        LoopbackTransport* second = core.signIn(b, kRows);
        LoopbackTransport* third = core.signIn(c, kRows);
        QVERIFY(admitted(first) && admitted(second) && admitted(third));

        LoopbackTransport* closedApp = nullptr;
        LoopbackTransport* replacement = nullptr;
        const auto arm = [&](LoopbackTransport* app, const Device* device) {
            LoopbackTransport* station = app->peerForTest();
            QVERIFY(station != nullptr);
            connect(station, &LoopbackTransport::outboundText, this,
                    [&, app, station, device](const QByteArray& wire) {
                        const QJsonObject message = QJsonDocument::fromJson(wire).object();
                        if (closedApp != nullptr
                            || message.value(QStringLiteral("type"))
                                   != QStringLiteral("capabilities")) {
                            return;
                        }
                        // A real transport can close inside sendText. Rejoin
                        // the same device while the old broadcast is still
                        // on the stack: it must receive only its own snapshot.
                        closedApp = app;
                        station->closeLink(QStringLiteral("callback closed its session"));
                        replacement = core.signIn(*device, kRows);
                    });
        };
        arm(first, &a);
        arm(second, &b);
        arm(third, &c);

        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(closedApp != nullptr);
        QVERIFY(replacement != nullptr);
        QVERIFY(admitted(replacement));
        for (LoopbackTransport* app : {first, second, third}) {
            if (app == closedApp) {
                continue;
            }
            QTRY_COMPARE(ofType(app->received(), QStringLiteral("capabilities")).size(), 2);
            const QJsonObject withdrawal =
                ofType(app->received(), QStringLiteral("capabilities")).last();
            for (const QJsonValue& entry :
                 withdrawal.value(QStringLiteral("properties")).toArray()) {
                QVERIFY(entry.toObject().value(QStringLiteral("name")).toString()
                        != QStringLiteral("radioAntennaRowsVersion"));
            }
        }
        QCOMPARE(ofType(replacement->received(), QStringLiteral("capabilities")).size(), 1);
        QVERIFY(!capability(replacement->received(), QStringLiteral("radioAntennaRowsVersion")));
    }

    void anotherHolderOnAirStillBlocksTheBoundTxRow()
    {
        StepAttenuatorController step;
        Core core;
        readyAlex(core, step);
        allowTransmit(core);
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        auto features = kTransmitter;
        features.insert("radioAntennaRows", 1);
        auto* holder = core.signIn(a, features);
        auto* other = core.signIn(b, features);
        QVERIFY(admitted(holder) && admitted(other));

        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const auto denied = core.invoke(other, "setAlexTxAntennaForRadio",
                                        txArgs(kMac, Band::Band20m, 2));
        QVERIFY(!denied.value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), 1);
        const auto accepted = core.invoke(holder, "setAlexTxAntennaForRadio",
                                          txArgs(kMac, Band::Band20m, 2));
        QVERIFY2(accepted.value("accepted").toBool(),
                 qPrintable(accepted.value("reason").toString()));
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), 2);
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }
};

QTEST_GUILESS_MAIN(TstRadioBoundAntennaRows)
#include "tst_radio_bound_antenna_rows.moc"
