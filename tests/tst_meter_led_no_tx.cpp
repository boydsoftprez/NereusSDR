// =================================================================
// tests/tst_meter_led_no_tx.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test.
//
// Container item parity Task 27: LED "No TX" (Thetis clsLed NoTxTrue,
// MeterManager.cs:21177-21211 [v2.10.3.15]). It only ever stops transmit:
//   - an LED with No TX on asks for one stop per transition to true while
//     transmitting, with Thetis's message; No TX off, receiving, an entry
//     bound to another session, a preview copy and a hidden meter's LED
//     (unless processed when hidden) ask nothing;
//   - ledStopAppliesToSession: an empty or matching session applies, a
//     foreign one does not, and a foreign entry's stop reaches no radio;
//   - a local radio with MOX on is stopped (StopAllTx) and nothing keys;
//   - a remote window's stop reaches the Core over the loopback session
//     (the window's own release) and the Core stops; nothing keys again;
//   - No TX is saved after the condition; records without it are unchanged.
//   - Fix round 1: No TX when false (Thetis NoTxFalse) mirrors it; neither
//     acts while the condition has an error; the editor's two check boxes
//     write their fields and survive a round trip; the user's warning is
//     plain NereusSDR wording while the signal keeps Thetis's text.
// No device: a mock connection and the in-process loopback Core.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Created for Task 27. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Fix round 1: No TX when false, the condition error, the
//                 editor check boxes and the user wording. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/LedItemEditor.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/RemoteAudioSessionHarness.h"
#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteAudioSessionHarness;

namespace {

// The signal's text (Thetis's) and the warning the user reads.
const QString kStop = QStringLiteral("Led Indicator stopped MOX");
const QString kShown = QStringLiteral("An LED stopped transmitting.");

// A connection that records MOX writes; nothing reaches a radio.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

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
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override { log.append(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
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

// A local radio on 20 m USB keying at once (no MOX delays).
struct LocalRig {
    RadioModel model;
    MockConnection conn;

    LocalRig()
    {
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }
    }
    ~LocalRig() { model.injectConnectionForTest(nullptr); }
};

// An LED that is true at a reading of 1 and false at -1 (the green
// threshold at 0), with No TX on.
void makeThresholdLed(LEDItem& led)
{
    led.setGreenThreshold(0.0);
    led.setNoTx(true);
    led.setValue(-1.0);
}

// MainWindow's forwarding (onContainerItemAdded), for a given context and
// session: the session rule, then the stop's route.
// The signal's text is Thetis's; the stop carries NereusSDR's wording.
LedStopRoute forward(const QJsonObject& context, const QString& session, RadioModel& radio,
                     const QString& signalMessage)
{
    if (signalMessage != QLatin1String(kLedNoTxStopMessage)) {
        return LedStopRoute::None;
    }
    if (!ledStopAppliesToSession(context, session)) {
        return LedStopRoute::None;
    }
    return stopTransmitForLed(radio, QString::fromLatin1(kLedNoTxStopUserText));
}

// The command.invoke messages of `verb` the Core received.
int commandCount(const Test::LoopbackTransport* core, const QString& verb)
{
    int n = 0;
    for (const QByteArray& wire : core->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == QLatin1String("command.invoke")
            && o.value(QStringLiteral("verb")).toString() == verb) {
            ++n;
        }
    }
    return n;
}

} // namespace

class TstMeterLedNoTx : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString profile = QStringLiteral("meter-led-no-tx-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ---- The LED ----------------------------------------------------------

    void oneStopPerTransitionToTrueWhileTransmitting()
    {
        LEDItem led;
        makeThresholdLed(led);
        led.setTransmitting(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);

        led.setValue(1.0);
        QCOMPARE(stops.count(), 1);
        QCOMPARE(stops.at(0).at(0).toString(), kStop);
        // Still true: no second stop.
        led.setValue(2.0);
        led.setValue(1.5);
        QCOMPARE(stops.count(), 1);
        // False, then true again: one more.
        led.setValue(-1.0);
        QCOMPARE(stops.count(), 1);
        led.setValue(1.0);
        QCOMPARE(stops.count(), 2);
    }

    void aConditionTurningTrueStopsOnce()
    {
        LEDItem led;
        led.setNoTx(true);
        led.setTransmitting(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        led.setCondition(QStringLiteral("1 > 2"));
        QVERIFY(!led.isTrue());
        QCOMPARE(stops.count(), 0);
        led.setCondition(QStringLiteral("2 > 1"));
        QVERIFY(led.isTrue());
        QCOMPARE(stops.count(), 1);
        QCOMPARE(stops.at(0).at(0).toString(), kStop);
        QVERIFY(led.evaluateCondition() == false);   // unchanged, still true
        QCOMPARE(stops.count(), 1);
    }

    void aKeyWhileTheLedIsTrueIsStopped()
    {
        // Thetis tests MOX on every update, so keying while the LED is
        // already true is stopped too.
        LEDItem led;
        makeThresholdLed(led);
        led.setValue(1.0);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        QCOMPARE(stops.count(), 0);   // receiving
        led.setTransmitting(true);
        QCOMPARE(stops.count(), 1);
        led.setTransmitting(false);
        led.setTransmitting(true);
        QCOMPARE(stops.count(), 2);
    }

    void noTxOffOrReceivingAsksNothing()
    {
        LEDItem off;
        off.setGreenThreshold(0.0);
        off.setValue(-1.0);
        off.setTransmitting(true);
        QSignalSpy offStops(&off, &LEDItem::txStopRequested);
        off.setValue(1.0);
        off.setValue(-1.0);
        off.setValue(1.0);
        QCOMPARE(offStops.count(), 0);

        LEDItem receiving;
        makeThresholdLed(receiving);
        QSignalSpy rxStops(&receiving, &LEDItem::txStopRequested);
        receiving.setValue(1.0);
        receiving.setValue(-1.0);
        receiving.setValue(1.0);
        receiving.setCondition(QStringLiteral("2 > 1"));
        QCOMPARE(rxStops.count(), 0);
    }

    void anotherSessionsEntryAsksNothing()
    {
        LEDItem led;
        makeThresholdLed(led);
        led.setForeignSession(true);
        led.setTransmitting(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        led.setValue(1.0);
        QCOMPARE(stops.count(), 0);
    }

    void aHiddenMetersLedAsksOnlyWhenProcessedWhenHidden()
    {
        MeterWidget meter;
        auto* led = new LEDItem(&meter);
        meter.addItem(led);
        makeThresholdLed(*led);
        led->setTransmitting(true);
        QSignalSpy stops(led, &LEDItem::txStopRequested);
        QVERIFY(!meter.isVisible());
        led->setValue(1.0);
        QCOMPARE(stops.count(), 0);
        // Process when hidden (Thetis UpdateAlways): it acts.
        led->setProcessWhenHidden(true);
        QCOMPARE(stops.count(), 1);
        led->setValue(-1.0);
        led->setProcessWhenHidden(false);
        // Shown: it acts without the option.
        meter.show();
        QVERIFY(meter.isVisible());
        led->setValue(1.0);
        QCOMPARE(stops.count(), 2);
    }

    void aPreviewCopyNeverAsks()
    {
        LEDItem shape;
        makeThresholdLed(shape);
        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.typeId = QStringLiteral("LED");
        entry.config.insert(QStringLiteral("legacyRecord"), shape.serialize());

        for (ContentRenderMode mode : {ContentRenderMode::Preview, ContentRenderMode::Validation}) {
            std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr, mode));
            auto* copy = qobject_cast<LEDItem*>(item.get());
            QVERIFY(copy);
            QVERIFY(copy->noTx());
            QVERIFY(!copy->stopEnabled());
            QSignalSpy stops(copy, &LEDItem::txStopRequested);
            copy->setTransmitting(true);
            copy->setValue(1.0);
            copy->setValue(-1.0);
            copy->setValue(1.0);
            QCOMPARE(stops.count(), 0);
        }
        // The live copy asks.
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* live = qobject_cast<LEDItem*>(item.get());
        QVERIFY(live && live->stopEnabled());
        QSignalSpy stops(live, &LEDItem::txStopRequested);
        live->setTransmitting(true);
        live->setValue(1.0);
        QCOMPARE(stops.count(), 1);
    }

    // ---- The session rule ---------------------------------------------------

    void theSessionRule()
    {
        const QString mine = QStringLiteral("00:1c:c0:a2:13:dd");
        QVERIFY(ledStopAppliesToSession(QJsonObject{}, mine));
        QVERIFY(ledStopAppliesToSession(QJsonObject{}, QString()));
        QVERIFY(ledStopAppliesToSession(QJsonObject{{QStringLiteral("sliceId"), 0}}, mine));
        QVERIFY(ledStopAppliesToSession(QJsonObject{{QStringLiteral("sessionId"), QString()}}, mine));
        QVERIFY(ledStopAppliesToSession(QJsonObject{{QStringLiteral("sessionId"), mine}}, mine));
        QVERIFY(!ledStopAppliesToSession(
            QJsonObject{{QStringLiteral("sessionId"), QStringLiteral("other")}}, mine));
        QVERIFY(!ledStopAppliesToSession(
            QJsonObject{{QStringLiteral("sessionId"), QStringLiteral("other")}}, QString()));
    }

    // ---- A local radio ------------------------------------------------------

    void aLocalStopEndsMoxAndNothingKeys()
    {
        AppSettings::instance().clear();
        LocalRig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.model.isTransmitting());
        QVERIFY(rig.model.mox());
        // The key's walk has reached the radio.
        QTRY_VERIFY(rig.conn.log.contains(QStringLiteral("MOX on")));
        QTRY_COMPARE(rig.model.moxController()->state(), MoxState::Tx);

        LEDItem led;
        makeThresholdLed(led);
        led.setTransmitting(rig.model.isTransmitting());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        QObject::connect(&led, &LEDItem::txStopRequested, &rig.model,
                         [&rig](const QString& message) {
                             QCOMPARE(forward(QJsonObject{}, rig.model.currentRadioMac(),
                                              rig.model, message),
                                      LedStopRoute::Local);
                         }, Qt::QueuedConnection);
        rig.conn.log.clear();
        led.setValue(1.0);
        // Queued, as MainWindow connects it.
        QVERIFY(rig.model.mox());
        QTRY_VERIFY(!rig.model.isTransmitting());
        QVERIFY(!rig.model.mox());
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), kShown);
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX off")));
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));
        QTest::qWait(50);
        QVERIFY(!rig.model.mox());
    }

    void aForeignEntryReachesNoRadio()
    {
        AppSettings::instance().clear();
        LocalRig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY(rig.model.isTransmitting());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        const QJsonObject foreign{{QStringLiteral("sessionId"), QStringLiteral("another-core")}};
        QCOMPARE(forward(foreign, rig.model.currentRadioMac(), rig.model, kStop),
                 LedStopRoute::None);
        QTest::qWait(20);
        QVERIFY(rig.model.mox());
        QVERIFY(rig.model.isTransmitting());
        QCOMPARE(stopped.count(), 0);
        rig.model.stopAllTx(QString());
        QTRY_VERIFY(!rig.model.isTransmitting());
    }

    void aLocalStopWithNothingKeyedDoesNothing()
    {
        AppSettings::instance().clear();
        LocalRig rig;
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.conn.log.clear();
        QCOMPARE(stopTransmitForLed(rig.model, kStop), LedStopRoute::Local);
        QTest::qWait(20);
        QCOMPARE(stopped.count(), 0);
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));
        QVERIFY(!rig.model.mox());
    }

    // ---- A remote window ----------------------------------------------------

    void aRemoteWindowsStopReachesTheCore()
    {
        RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QTRY_VERIFY(h.remote.remoteTransmitRouted());
        MoxController* coreMox = h.station.moxController();

        // The window keys the Core (its own MOX press).
        h.remote.setMoxFromButton(true);
        QTRY_VERIFY(coreMox->isMox());
        QTRY_VERIFY(h.remote.isTransmitting());
        const int keysBefore = commandCount(h.stationLink, QStringLiteral("tx.key"));
        QVERIFY(keysBefore > 0);

        // The window's LED, wired as MainWindow wires it.
        const QString session = QStringLiteral("window-session");
        LEDItem led;
        makeThresholdLed(led);
        led.setTransmitting(h.remote.isTransmitting());
        QList<LedStopRoute> routes;
        QObject::connect(&led, &LEDItem::txStopRequested, &h.remote,
                         [&](const QString& message) {
                             routes.append(forward(QJsonObject{{QStringLiteral("sessionId"), session}},
                                                   session, h.remote, message));
                         }, Qt::QueuedConnection);
        led.setValue(1.0);
        QTRY_COMPARE(routes.size(), 1);
        QCOMPARE(routes.first(), LedStopRoute::Remote);

        // The Core stopped, and the window reads it.
        QTRY_VERIFY(!coreMox->isMox());
        QTRY_VERIFY(!h.station.isTransmitting());
        QTRY_VERIFY(!h.remote.isTransmitting());
        QVERIFY(commandCount(h.stationLink, QStringLiteral("tx.unkey")) > 0);
        led.setTransmitting(h.remote.isTransmitting());

        // Nothing keyed again, and the window's own controller never keyed.
        QTest::qWait(200);
        QVERIFY(!coreMox->isMox());
        QCOMPARE(commandCount(h.stationLink, QStringLiteral("tx.key")), keysBefore);
        QVERIFY(!h.remote.moxController()->isMox());
        QCOMPARE(routes.size(), 1);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void aRemoteWindowWithoutRemoteTransmitSendsNothing()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.remoteTransmitRouted());
        QTest::ignoreMessage(QtWarningMsg,
                             "LED No TX: this window cannot reach the Core's transmit; "
                             "nothing was stopped");
        QCOMPARE(stopTransmitForLed(remote, kStop), LedStopRoute::None);
        QVERIFY(!remote.isTransmitting());
    }

    // ---- Fix round 1 --------------------------------------------------------

    void noTxWhenFalseStopsOncePerTransitionToFalse()
    {
        LEDItem led;
        led.setGreenThreshold(0.0);
        led.setValue(1.0);
        led.setNoTxFalse(true);
        QVERIFY(!led.noTx());
        led.setTransmitting(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        QCOMPARE(stops.count(), 0);   // true: nothing
        led.setValue(-1.0);
        QCOMPARE(stops.count(), 1);
        QCOMPARE(stops.at(0).at(0).toString(), kStop);
        led.setValue(-2.0);
        QCOMPARE(stops.count(), 1);
        led.setValue(1.0);
        QCOMPARE(stops.count(), 1);
        led.setValue(-1.0);
        QCOMPARE(stops.count(), 2);
        // A key made while false is stopped, as Thetis's every-update test.
        led.setTransmitting(false);
        led.setTransmitting(true);
        QCOMPARE(stops.count(), 3);
        // Receiving: nothing.
        led.setTransmitting(false);
        led.setValue(1.0);
        led.setValue(-1.0);
        QCOMPARE(stops.count(), 3);
    }

    void bothOnStopOnEachTransition()
    {
        LEDItem led;
        makeThresholdLed(led);   // No TX when true, false now
        led.setValue(1.0);
        led.setNoTxFalse(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        led.setTransmitting(true);   // true: No TX when true
        QCOMPARE(stops.count(), 1);
        led.setValue(-1.0);          // false: No TX when false
        QCOMPARE(stops.count(), 2);
        led.setValue(1.0);
        QCOMPARE(stops.count(), 3);
    }

    void noStopWhileTheConditionHasAnError()
    {
        // Thetis runs the LED's Update only for a valid script.
        LEDItem led;
        led.setNoTx(true);
        led.setNoTxFalse(true);
        led.setTransmitting(true);
        QSignalSpy stops(&led, &LEDItem::txStopRequested);
        led.setCondition(QStringLiteral("Math.Abs(1) > 0"));
        QVERIFY(!led.conditionError().isEmpty());
        QVERIFY(!led.isTrue());
        QCOMPARE(stops.count(), 0);
        led.setCondition(QStringLiteral("1 > 2"));   // valid and false
        QCOMPARE(stops.count(), 1);
    }

    void noTxWhenFalseIsInertInAPreview()
    {
        LEDItem shape;
        shape.setNoTxFalse(true);
        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.typeId = QStringLiteral("LED");
        entry.config.insert(QStringLiteral("legacyRecord"), shape.serialize());
        std::unique_ptr<MeterItem> item(
            registry.createMeterItem(entry, nullptr, ContentRenderMode::Preview));
        auto* copy = qobject_cast<LEDItem*>(item.get());
        QVERIFY(copy && copy->noTxFalse());
        QSignalSpy stops(copy, &LEDItem::txStopRequested);
        copy->setTransmitting(true);
        copy->setValue(-1.0);
        QCOMPARE(stops.count(), 0);
    }

    void theEditorsCheckBoxesWriteAndRoundTrip()
    {
        LEDItem led;
        LedItemEditor editor;
        editor.setItem(&led);
        auto* onBox = editor.findChild<QCheckBox*>(QStringLiteral("ledNoTxTrue"));
        auto* offBox = editor.findChild<QCheckBox*>(QStringLiteral("ledNoTxFalse"));
        QVERIFY(onBox);
        QVERIFY(offBox);
        QVERIFY(!onBox->isChecked());
        QVERIFY(!offBox->isChecked());
        // The rows' labels and the boxes' tooltips are plain words.
        QStringList labels;
        for (const QLabel* label : editor.findChildren<QLabel*>()) {
            labels << label->text();
        }
        for (const QString& text : {QStringLiteral("Stop transmitting when the LED turns on"),
                                    QStringLiteral("Stop transmitting when the LED turns off")}) {
            QVERIFY2(labels.contains(text), qPrintable(text));
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        for (QCheckBox* box : {onBox, offBox}) {
            QVERIFY2(OperatorWording::isPlain(box->toolTip()), qPrintable(box->toolTip()));
        }

        const QString plain = led.serialize();
        onBox->click();
        QVERIFY(led.noTx());
        QVERIFY(!led.noTxFalse());
        QCOMPARE(led.serialize(), plain + QStringLiteral("||1"));
        offBox->click();
        QVERIFY(led.noTxFalse());
        QCOMPARE(led.serialize(), plain + QStringLiteral("||1|1"));
        onBox->click();
        QVERIFY(!led.noTx());
        const QString offOnly = led.serialize();
        QCOMPARE(offOnly, plain + QStringLiteral("||0|1"));

        // A round trip, and a new editor shows what was saved.
        LEDItem back;
        QVERIFY(back.deserialize(offOnly));
        QVERIFY(!back.noTx());
        QVERIFY(back.noTxFalse());
        QCOMPARE(back.serialize(), offOnly);
        LedItemEditor again;
        again.setItem(&back);
        QVERIFY(!again.findChild<QCheckBox*>(QStringLiteral("ledNoTxTrue"))->isChecked());
        QVERIFY(again.findChild<QCheckBox*>(QStringLiteral("ledNoTxFalse"))->isChecked());

        offBox->click();
        QVERIFY(!led.noTxFalse());
        QCOMPARE(led.serialize(), plain);
    }

    void theUsersWarningIsPlain()
    {
        QCOMPARE(QString::fromLatin1(kLedNoTxStopMessage), kStop);
        QCOMPARE(QString::fromLatin1(kLedNoTxStopUserText), kShown);
        QVERIFY(OperatorWording::isPlain(kShown));
    }

    // ---- The record ---------------------------------------------------------

    void noTxIsSavedAfterTheCondition()
    {
        LEDItem plain;
        const QString plainRecord = plain.serialize();
        QVERIFY(!plainRecord.endsWith(QStringLiteral("|1|1")));

        // No TX without a condition: an empty condition field, then 1.
        LEDItem led;
        led.setNoTx(true);
        const QString record = led.serialize();
        QCOMPARE(record, plainRecord + QStringLiteral("||1"));
        LEDItem back;
        QVERIFY(back.deserialize(record));
        QVERIFY(back.noTx());
        QVERIFY(!back.hasCondition());
        QCOMPARE(back.serialize(), record);

        // With a condition.
        LEDItem cond;
        cond.setCondition(QStringLiteral("%swr% > 2"));
        const QString condOnly = cond.serialize();
        cond.setNoTx(true);
        const QString both = cond.serialize();
        QCOMPARE(both, condOnly + QStringLiteral("|1"));
        LEDItem back2;
        QVERIFY(back2.deserialize(both));
        QVERIFY(back2.noTx());
        QCOMPARE(back2.condition(), QStringLiteral("%swr% > 2"));
        QCOMPARE(back2.serialize(), both);

        // A record without the fields is off for both; a damaged field is
        // refused.
        LEDItem old;
        QVERIFY(old.deserialize(condOnly));
        QVERIFY(!old.noTx());
        QVERIFY(old.deserialize(plainRecord));
        QVERIFY(!old.noTx());
        LEDItem bad;
        QVERIFY(!bad.deserialize(plainRecord + QStringLiteral("||yes")));
        QVERIFY(!bad.deserialize(plainRecord + QStringLiteral("||0|yes")));
        QVERIFY(old.deserialize(both));
        QVERIFY(!old.noTxFalse());
    }
};

QTEST_MAIN(TstMeterLedNoTx)
#include "tst_meter_led_no_tx.moc"
