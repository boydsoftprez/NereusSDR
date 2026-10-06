// no-port-check: NereusSDR-original. No upstream logic is ported here.
// =================================================================
// tests/tst_radio_speaker_link.cpp  (NereusSDR)
// =================================================================
//
// The radio speaker on the wire (R-SPK-06 remote half, R-SPK-13, R-SPK-14,
// R-SPK-16; V-SW-4): radio's radioSpeakerVolume, radioSpeakerMuted and
// speakerAmplifierMode are Bidirectional and the two reports Outbound, all
// five only to a peer that declared radioSpeaker 1, which alone is sent
// radioSpeakerVersion 1 (last before coreBuildInfo). A remote window's
// change lands through the Core's own setter and is saved on the Core under
// its radio's MAC; another device follows. A window of a Core without the
// capability shows RADIO disabled with the update reason and sends nothing.
//
// Nothing here keys a radio or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-06 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QScopeGuard>
#include <QSignalSpy>

#include "core/RadioConnection.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

namespace {

const QString kCoreMac = QStringLiteral("AA:BB:CC:DD:EE:01");
const char* const kSpeakerProperties[] = {"radioSpeakerVolume", "radioSpeakerMuted",
                                          "speakerAmplifierMode",
                                          "radioSpeakerAvailability",
                                          "speakerAmplifierAvailable"};
const QString kOlderCore =
    QStringLiteral("This Core can't set the radio speaker. Update the Core.");
const QString kRemoteTip =
    QStringLiteral("Radio speaker at the Core (shared with every window and the phone)");

// A connected radio that carries radio audio, so the Core's reports say
// there is a radio speaker (the harness Core is a Hermes Lite 2).
class SpeakerConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit SpeakerConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    int protocolVersion() const override { return 1; }
    bool carriesRadioAudio() const noexcept override { return true; }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

// A link with nothing negotiated: IStationLink's defaults, as a window of
// a Core that predates the radio speaker sees them.
class OlderCoreLink : public IStationLink {
public:
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};

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

int radioDeltasFrom(const LoopbackTransport* app, qsizetype mark)
{
    int count = 0;
    const QList<QByteArray> received = app->received();
    for (qsizetype i = mark; i < received.size(); ++i) {
        const QJsonObject o = QJsonDocument::fromJson(received.at(i)).object();
        if (o.value(QStringLiteral("type")).toString() == QStringLiteral("delta")
            && o.value(QStringLiteral("key")).toString() == QStringLiteral("radio")) {
            ++count;
        }
    }
    return count;
}

QVariant savedOnCore(const char* name)
{
    return AppSettings::instance().hardwareValue(
        kCoreMac, QStringLiteral("RadioSpeaker/") + QLatin1String(name));
}

MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
}

QHash<QByteArray, int> speakerAsks()
{
    QHash<QByteArray, int> asks = kHolder;
    asks.insert(QByteArrayLiteral("radioSpeaker"), 1);
    return asks;
}

// A remote window on the loopback, signed in with the Core's token.
struct Window {
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&model, &proxy};
    LoopbackTransport* stationEnd = nullptr;
    LoopbackTransport* clientEnd = nullptr;

    bool open(Core& core, QObject* owner)
    {
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        stationEnd = new LoopbackTransport(QStringLiteral("speaker-station"), owner);
        clientEnd = new LoopbackTransport(QStringLiteral("speaker-client"), owner);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        return QTest::qWaitFor([&completed]() { return completed.count() == 1; }, 5000);
    }
};

} // namespace

class TstRadioSpeakerLink : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clearHardwareValues(kCoreMac);
    }

    // R-SPK-13 / R-SPK-14: three Bidirectional, two Outbound, all five
    // gated on radioSpeaker 1.
    void policy_directionsAndGates()
    {
        for (const char* property : {"radioSpeakerVolume", "radioSpeakerMuted",
                                     "speakerAmplifierMode"}) {
            QVERIFY2(MirrorPolicy::hasExplicitEntry("RadioModel", property), property);
            QCOMPARE(MirrorPolicy::directionFor("RadioModel", property),
                     MirrorDirection::Bidirectional);
            QVERIFY2(MirrorPolicy::inboundAllowed("RadioModel", property), property);
        }
        for (const char* property : {"radioSpeakerAvailability", "speakerAmplifierAvailable"}) {
            QVERIFY2(MirrorPolicy::hasExplicitEntry("RadioModel", property), property);
            QCOMPARE(MirrorPolicy::directionFor("RadioModel", property),
                     MirrorDirection::Outbound);
            QVERIFY2(!MirrorPolicy::inboundAllowed("RadioModel", property), property);
        }
        for (const char* property : kSpeakerProperties) {
            const MirrorPolicy::FeatureGate* gate =
                MirrorPolicy::featureGateFor("RadioModel", property);
            QVERIFY2(gate != nullptr, property);
            QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("radioSpeaker"));
            QCOMPARE(gate->minVersion, 1);
        }
    }

    // radioSpeakerVersion reaches a declaring peer, last before
    // coreBuildInfo; a peer that did not declare it is sent none; the entry
    // reads back.
    void capability_onlyToADeclaringPeer()
    {
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
        QHash<QByteArray, int> asks = speakerAsks();
        asks.insert(QByteArrayLiteral("radeReason"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        QHash<QByteArray, int> plain = kHolder;
        plain.insert(QByteArrayLiteral("radeReason"), 1);
        plain.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b, plain);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("radioSpeakerVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("radioSpeakerVersion")).has_value());

        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 3);
        QCOMPARE(caps.at(caps.size() - 3).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radeReasonVersion"));
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radioSpeakerVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        // The other peer's list is the one it had before: the same entries,
        // in the same order, without the new one.
        const QJsonArray plainCaps = firstOfType(appB->received(), QStringLiteral("capabilities"))
                                         .value(QStringLiteral("properties")).toArray();
        QCOMPARE(plainCaps.size(), caps.size() - 1);
        QCOMPARE(plainCaps.at(plainCaps.size() - 2).toObject().value(QStringLiteral("name"))
                     .toString(),
                 QStringLiteral("radeReasonVersion"));

        StationCapabilities sent;
        sent.radioSpeakerVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).radioSpeakerVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .radioSpeakerVersion, 0);
    }

    // A declaring peer is sent the five properties and their changes; one
    // that did not declare it never sees a name, and a change to them alone
    // sends it no radio delta.
    void properties_onlyToADeclaringPeer()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, speakerAsks());
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        for (const char* property : kSpeakerProperties) {
            QVERIFY2(sawProperty(appA, QString::fromLatin1(property)), property);
        }
        QTest::qWait(200);
        const qsizetype mark = appB->received().size();

        core.model->setRadioSpeakerVolume(33);
        core.model->setRadioSpeakerMuted(true);
        core.model->setSpeakerAmplifierMode(2);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("radioSpeakerVolume")).toInteger(), 33);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("radioSpeakerMuted")).toBool(), true);
        QTRY_COMPARE(latest(appA->received(), QStringLiteral("radio"),
                            QStringLiteral("speakerAmplifierMode")).toInteger(), 2);
        QTest::qWait(200);

        for (const char* property : kSpeakerProperties) {
            QVERIFY2(!sawProperty(appB, QString::fromLatin1(property)), property);
        }
        QCOMPARE(radioDeltasFrom(appB, mark), 0);
    }

    // A peer that did not declare radioSpeaker is refused a write of them,
    // and the Core's value stays; a declaring peer's write lands.
    void write_refusedFromAPeerThatDidNotDeclare()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, speakerAsks());
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        const int before = core.model->radioSpeakerVolume();

        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {int64("radioSpeakerVolume", 12)}, 71)));
        QTRY_VERIFY(!propertyResult(appB, 71).isEmpty());
        const QJsonArray refused =
            propertyResult(appB, 71).value(QStringLiteral("results")).toArray();
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.at(0).toObject().value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.at(0).toObject().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Update this app to change the radio speaker on this Core."));
        QCOMPARE(core.model->radioSpeakerVolume(), before);

        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {int64("radioSpeakerVolume", 12)}, 72)));
        QTRY_COMPARE(core.model->radioSpeakerVolume(), 12);
        // The reports are the Core's own.
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {int64("radioSpeakerAvailability", 1)}, 73)));
        QTRY_VERIFY(!propertyResult(appA, 73).isEmpty());
        QCOMPARE(propertyResult(appA, 73).value(QStringLiteral("results")).toArray().at(0)
                     .toObject().value(QStringLiteral("accepted")).toBool(true),
                 false);
        QCOMPARE(core.model->radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNoRadio));
    }

    // V-SW-4: a remote window's change lands through the Core's setter, is
    // saved under the Core's radio's MAC and reaches another device; that
    // device's change reaches the window. The reports and the tooltip are
    // the Core's.
    void remoteWindow_roundTrip()
    {
        Core core(/*upgradedWithToken=*/true);
        SpeakerConnection conn;
        core.model->injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&core]() { core.model->injectConnectionForTest(nullptr); });
        // Already Connected: the board sets the reports again with the
        // connection in place.
        core.model->setBoardForTest(HPSDRHW::HermesLite);
        QCOMPARE(core.model->radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNeedsAddOn));
        QVERIFY(!core.model->speakerAmplifierAvailable());

        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, speakerAsks());
        QVERIFY(admitted(app));

        // Values that differ from every default, so the snapshot the
        // window opens with is the Core's and not its own.
        core.model->setRadioSpeakerVolume(72);
        core.model->setRadioSpeakerMuted(true);
        core.model->setSpeakerAmplifierMode(2);

        Window window;
        QVERIFY(window.open(core, this));
        RadioModel& remote = window.model;
        QVERIFY(window.client.radioSpeakerAvailable());
        QTRY_COMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNeedsAddOn));
        QCOMPARE(remote.radioSpeakerVolume(), 72);
        QCOMPARE(remote.radioSpeakerMuted(), true);
        QCOMPARE(remote.speakerAmplifierMode(), 2);
        // Back to unmuted, so the other device's mute below is a change.
        core.model->setRadioSpeakerMuted(false);
        QTRY_VERIFY(!remote.radioSpeakerMuted());
        QVERIFY(remote.radioSpeakerUnavailableReason().isEmpty());
        QCOMPARE(remote.radioSpeakerToolTip(),
                 kRemoteTip + QLatin1Char('\n') + RadioModel::radioSpeakerAddOnNote());
        QCOMPARE(core.model->radioSpeakerToolTip(),
                 QStringLiteral("Radio speaker\n") + RadioModel::radioSpeakerAddOnNote());
        QCOMPARE(remote.speakerAmplifierUnavailableReason(),
                 QStringLiteral("This radio has no switchable speaker amplifier."));

        QSignalSpy coreVolume(core.model.get(), &RadioModel::radioSpeakerVolumeChanged);
        remote.setRadioSpeakerVolume(40);
        QTRY_COMPARE(core.model->radioSpeakerVolume(), 40);
        QCOMPARE(coreVolume.count(), 1);
        QCOMPARE(savedOnCore("Volume").toInt(), 40);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("radioSpeakerVolume")).toInteger(), 40);

        remote.setSpeakerAmplifierMode(1);
        QTRY_COMPARE(core.model->speakerAmplifierMode(), 1);
        QCOMPARE(savedOnCore("AmplifierMode").toInt(), 1);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("speakerAmplifierMode")).toInteger(), 1);

        // The other device mutes it; the Core saves it and the window
        // follows.
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "radio", {boolean("radioSpeakerMuted", true)}, 81)));
        QTRY_VERIFY(core.model->radioSpeakerMuted());
        QCOMPARE(savedOnCore("Muted").toString(), QStringLiteral("True"));
        QTRY_VERIFY(remote.radioSpeakerMuted());
        QCOMPARE(remote.radioSpeakerVolume(), 40);
        QCOMPARE(remote.speakerAmplifierMode(), 1);

        // A value out of range is clamped by the Core's setter.
        remote.setRadioSpeakerVolume(100);
        QTRY_COMPARE(core.model->radioSpeakerVolume(), 100);
        QVERIFY(!remote.applyStationRadioSpeakerValue("radioSpeakerAvailability", 3));
        QVERIFY(!remote.applyStationRadioSpeakerValue("radioSpeakerAvailability", -1));
        QCOMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNeedsAddOn));

        // The radio goes: no radio speaker in the window either.
        core.model->injectConnectionForTest(nullptr);
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_COMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNoRadio));
        QCOMPARE(remote.radioSpeakerUnavailableReason(), QStringLiteral("No radio connected"));
        QCOMPARE(remote.radioSpeakerToolTip(), QStringLiteral("No radio connected"));
    }

    // A window of a Core without the capability: RADIO disabled with the
    // update reason, and the setters change nothing and send nothing.
    void olderCore_disabledAndSilent()
    {
        RadioModel remote(RadioModel::Role::Remote);
        OlderCoreLink link;
        remote.attachStation(&link);
        remote.setStationConnectionState(ConnectionState::Connected);
        QCOMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNoRadio));
        QVERIFY(!remote.speakerAmplifierAvailable());
        QCOMPARE(remote.radioSpeakerUnavailableReason(), kOlderCore);
        QCOMPARE(remote.speakerAmplifierUnavailableReason(), kOlderCore);
        QCOMPARE(remote.radioSpeakerToolTip(), kOlderCore);

        QSignalSpy volume(&remote, &RadioModel::radioSpeakerVolumeChanged);
        QSignalSpy muted(&remote, &RadioModel::radioSpeakerMutedChanged);
        QSignalSpy mode(&remote, &RadioModel::speakerAmplifierModeChanged);
        const int before = remote.radioSpeakerVolume();
        remote.setRadioSpeakerVolume(before == 10 ? 20 : 10);
        remote.setRadioSpeakerMuted(!remote.radioSpeakerMuted());
        remote.setSpeakerAmplifierMode(2);
        QCOMPARE(volume.count(), 0);
        QCOMPARE(muted.count(), 0);
        QCOMPARE(mode.count(), 0);
        QCOMPARE(remote.radioSpeakerVolume(), before);
        // A report from such a Core cannot make RADIO available.
        QVERIFY(remote.applyStationRadioSpeakerValue("radioSpeakerAvailability", 1));
        QCOMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNoRadio));
        remote.attachStation(nullptr);
    }

    // The same over the wire: a Core whose capabilities do not carry
    // radioSpeakerVersion turns RADIO off in the window, and the window's
    // change reaches neither the wire nor the Core.
    void olderCore_overTheWire_sendsNothing()
    {
        Core core(/*upgradedWithToken=*/true);
        Window window;
        QVERIFY(window.open(core, this));
        RadioModel& remote = window.model;
        QVERIFY(window.client.radioSpeakerAvailable());

        // The Core's own descriptor, sent again without the entry.
        QJsonObject caps = firstOfType(window.clientEnd->received(), QStringLiteral("capabilities"));
        QJsonArray entries = caps.value(QStringLiteral("properties")).toArray();
        for (qsizetype i = entries.size() - 1; i >= 0; --i) {
            if (entries.at(i).toObject().value(QStringLiteral("name")).toString()
                == QStringLiteral("radioSpeakerVersion")) {
                entries.removeAt(i);
            }
        }
        caps.insert(QStringLiteral("properties"), entries);
        window.stationEnd->sendText(QJsonDocument(caps).toJson(QJsonDocument::Compact));
        QTRY_VERIFY(!window.client.radioSpeakerAvailable());
        QCOMPARE(remote.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNoRadio));
        QCOMPARE(remote.radioSpeakerToolTip(), kOlderCore);

        const qsizetype mark = window.stationEnd->received().size();
        const int coreBefore = core.model->radioSpeakerVolume();
        remote.setRadioSpeakerVolume(coreBefore == 10 ? 20 : 10);
        remote.setRadioSpeakerMuted(true);
        QTest::qWait(300);
        const QList<QByteArray> sent = window.stationEnd->received();
        for (qsizetype i = mark; i < sent.size(); ++i) {
            QVERIFY2(!sent.at(i).contains("radioSpeaker"), sent.at(i).constData());
        }
        QCOMPARE(core.model->radioSpeakerVolume(), coreBefore);
        QVERIFY(!core.model->radioSpeakerMuted());
    }

    // R-SPK-16 locally: "Radio speaker" with no add-on note off an HL2, and
    // the reason with no radio.
    void localToolTip()
    {
        RadioModel model;
        QCOMPARE(model.radioSpeakerToolTip(), QStringLiteral("No radio connected"));
        model.setBoardForTest(HPSDRHW::Hermes);
        SpeakerConnection conn;
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        model.setConnectionStateForTest(ConnectionState::Connected);
        QCOMPARE(model.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerAvailable));
        QCOMPARE(model.radioSpeakerToolTip(), QStringLiteral("Radio speaker"));
    }
};

QTEST_MAIN(TstRadioSpeakerLink)
#include "tst_radio_speaker_link.moc"
