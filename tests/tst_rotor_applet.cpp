// no-port-check: NereusSDR-original tests for the Rotor applet and its dial
// (rotor control plan, Task 6).
//
// One case per state the rotor mockup draws
// (docs/architecture/2026-10-07-rotor-control-mockup.html): an azimuth
// rotor turning, an az/el rotor stopped, no rotor (greyed, with the reason)
// and a Core too old to control a rotor. Then the behaviour: the turn
// buttons send the contract's hold dead man (repeats every 250 ms, active
// false on release), a drag on the dial sends nothing until it is let go and
// then one turn, Stop is the only red control, and every string a user reads
// is in plain words. A fake command sink; no rotor is turned and no serial
// port is opened.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 6).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 8: a remote
//                                    refusal of the applet's command is
//                                    shown on the applet while it is on
//                                    screen. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>

#include "core/AppSettings.h"
#include "core/session/IStationLink.h"
#include "gui/applets/RotorApplet.h"
#include "gui/widgets/RotorDialWidget.h"
#include "models/RadioModel.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;
using RotorLink::RotorModel;

namespace {

const QString kNoRotor = QStringLiteral("No rotor is set up on this Core.");
const QString kNotConnected = QStringLiteral("The rotor is not connected.");

class FakeSink : public RotorCommandSink {
public:
    struct Target {
        double azimuth;
        double elevation;
    };
    struct NudgeCall {
        Nudge direction;
        bool active;
    };
    QList<Target> targets;
    QList<NudgeCall> nudges;
    QList<QPair<QString, bool>> calls;
    int stops = 0;
    bool available = true;
    QString unavailableReason;
    bool accept = true;
    QString refusal = QStringLiteral("That callsign could not be placed.");

    bool rotorControlAvailable(QString* reason) const override
    {
        if (!available && reason) { *reason = unavailableReason; }
        return available;
    }
    bool requestRotorTarget(double azimuthDeg, double elevationDeg, QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        targets.append({azimuthDeg, elevationDeg});
        return true;
    }
    bool requestStopRotor(QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        ++stops;
        return true;
    }
    bool requestTurnRotorToCall(const QString& call, bool longPath, QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        calls.append({call, longPath});
        return true;
    }
    bool requestNudgeRotor(Nudge direction, bool active, QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        nudges.append({direction, active});
        return true;
    }
    bool requestConfigureRotor(const Setup&, QString*) override { return accept; }
    bool requestRotorPresets(const QString&, QString*) override { return accept; }
    quint32 lastId = 0;
    quint32 lastRotorCommandId() const override { return lastId; }

private:
    bool refuse(QString* reason) const
    {
        if (reason) { *reason = refusal; }
        return false;
    }
};

RotorModel::State azimuthTurning()
{
    // The mockup's first applet: Easy Rotor Control on COM4, turning from
    // 047 to 120, 73 degrees to go.
    RotorModel::State s;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.driver = RotorModel::Driver::Gs232b;
    s.label = QStringLiteral("Easy Rotor Control on COM4");
    s.axes = RotorModel::Axes::Azimuth;
    s.endStop = RotorModel::EndStop::None;
    s.positionFresh = true;
    s.azimuthDeg = 47.0;
    s.targetAzimuthDeg = 120.0;
    s.travelDeg = 73.0;
    s.routeKnown = true;
    s.motion = RotorModel::Motion::Turning;
    s.presets = QStringLiteral("EU\t45\nAF\t100\nSA\t160\nVK\t230\nJA\t330\nW6\t285");
    return s;
}

RotorModel::State azElStopped()
{
    RotorModel::State s;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.driver = RotorModel::Driver::Rotctld;
    s.label = QStringLiteral("rotctld 192.168.1.40:4533");
    s.axes = RotorModel::Axes::AzimuthElevation;
    s.endStop = RotorModel::EndStop::None;
    s.positionFresh = true;
    s.azimuthDeg = 212.0;
    s.elevationDeg = 34.0;
    s.motion = RotorModel::Motion::Stopped;
    return s;
}

QString plain(const QLabel* label)
{
    if (label->textFormat() == Qt::PlainText) { return label->text(); }
    QTextDocument doc;
    doc.setHtml(label->text());
    return doc.toPlainText();
}

QPushButton* button(const QWidget& w, const char* name)
{
    return w.findChild<QPushButton*>(QString::fromLatin1(name));
}

QLabel* label(const QWidget& w, const char* name)
{
    return w.findChild<QLabel*>(QString::fromLatin1(name));
}

QList<QWidget*> controls(const RotorApplet& applet)
{
    QList<QWidget*> out;
    for (const char* name : {"rotorCcw", "rotorStop", "rotorCw", "rotorShortPath",
                             "rotorLongPath", "rotorTurnTo"}) {
        out.append(button(applet, name));
    }
    out.append(applet.findChild<QLineEdit*>(QStringLiteral("rotorCall")));
    out.append(applet.dial());
    return out;
}

} // namespace

class TestRotorApplet : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The dial's shape is a window preference; start every run on the
        // rose.
        AppSettings::instance().setValue(
            QString::fromLatin1(RotorDialWidget::kShapeSettingsKey), QStringLiteral("Rose"));
    }

    // ── The mockup's states ────────────────────────────────────────

    void azimuthRotorTurning()
    {
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.resize(330, 620);
        applet.show();

        QCOMPARE(plain(label(applet, "rotorStatus")),
                 QStringLiteral("Easy Rotor Control on COM4 · Turning"));
        QVERIFY(label(applet, "rotorStatusDot")->styleSheet().contains(QStringLiteral("#ffb800")));
        QCOMPARE(plain(label(applet, "rotorHeading")), QStringLiteral("047°"));
        QVERIFY(label(applet, "rotorHeading")->text().contains(QStringLiteral("#ffb800")));
        QCOMPARE(plain(label(applet, "rotorTarget")), QStringLiteral("120° · 73° to go"));
        QCOMPARE(applet.dial()->state(), RotorDialWidget::State::Turning);

        for (QWidget* w : controls(applet)) {
            QVERIFY2(w->isEnabled(), qPrintable(w->objectName()));
            QVERIFY2(w->isVisible(), qPrintable(w->objectName()));
        }
        QVERIFY(applet.disabledReason().isEmpty());
        QVERIFY(!label(applet, "rotorReason")->isVisible());
        // Azimuth only: no Down / Up.
        QVERIFY(!button(applet, "rotorDown")->isVisible());
        QVERIFY(!button(applet, "rotorUp")->isVisible());
        // Short path is the starting choice.
        QVERIFY(button(applet, "rotorShortPath")->isChecked());
        QVERIFY(!applet.longPath());

        // The six presets, as the mockup labels them.
        QStringList presets;
        for (QPushButton* b : applet.findChildren<QPushButton*>(QStringLiteral("rotorPreset"))) {
            presets.append(b->text());
        }
        QCOMPARE(presets, (QStringList{QStringLiteral("EU 45°"), QStringLiteral("AF 100°"),
                                       QStringLiteral("SA 160°"), QStringLiteral("VK 230°"),
                                       QStringLiteral("JA 330°"), QStringLiteral("W6 285°")}));
        QVERIFY(!label(applet, "rotorNoPresets")->isVisible());

        // A preset is one tap: a turn to its heading.
        applet.findChildren<QPushButton*>(QStringLiteral("rotorPreset")).at(4)->click();
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 330.0);
        QCOMPARE(sink.targets.at(0).elevation, -1.0);

        // On the tape the route says which way.
        applet.dial()->setShape(RotorDialWidget::Shape::Tape);
        rotor.setState(azimuthTurning());   // a refresh
        applet.syncFromModel();
        QCOMPARE(plain(label(applet, "rotorTarget")),
                 QStringLiteral("120° · CW 73° to go"));
        applet.dial()->setShape(RotorDialWidget::Shape::Rose);
    }

    void theLongWayRoundAndAnUnknownRoute()
    {
        RotorModel rotor;
        RotorModel::State s = azimuthTurning();
        s.endStop = RotorModel::EndStop::South;
        s.azimuthDeg = 170.0;
        s.targetAzimuthDeg = 190.0;
        s.travelDeg = -340.0;
        rotor.setState(s);
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        QCOMPARE(plain(label(applet, "rotorTarget")),
                 QStringLiteral("190° · 340° to go · long way round"));

        s.routeKnown = false;
        rotor.setState(s);
        QCOMPARE(plain(label(applet, "rotorTarget")),
                 QStringLiteral("190° · route known once the rotor moves"));
    }

    void azElRotorStopped()
    {
        RotorModel rotor;
        rotor.setState(azElStopped());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.resize(330, 640);
        applet.show();

        QCOMPARE(plain(label(applet, "rotorStatus")), QStringLiteral("rotctld 192.168.1.40:4533"));
        QVERIFY(label(applet, "rotorStatusDot")->styleSheet().contains(QStringLiteral("#5fff8a")));
        QCOMPARE(plain(label(applet, "rotorHeading")), QStringLiteral("212° el 34°"));
        QVERIFY(plain(label(applet, "rotorTarget")).isEmpty());
        QCOMPARE(applet.dial()->state(), RotorDialWidget::State::Idle);
        QVERIFY(applet.dial()->showsElevation());

        QVERIFY(button(applet, "rotorDown")->isVisible());
        QVERIFY(button(applet, "rotorUp")->isVisible());
        QVERIFY(button(applet, "rotorDown")->isEnabled());
        QVERIFY(label(applet, "rotorNoPresets")->isVisible());
        QVERIFY(applet.findChildren<QPushButton*>(QStringLiteral("rotorPreset")).isEmpty());

        // An elevation target shows its own "to go".
        RotorModel::State s = azElStopped();
        s.targetElevationDeg = 45.0;
        s.motion = RotorModel::Motion::Turning;
        rotor.setState(s);
        QCOMPARE(plain(label(applet, "rotorTarget")), QStringLiteral("el 45° · 11° to go"));

        // A drag on the elevation gauge turns to that elevation and keeps
        // the azimuth where it is.
        rotor.setState(azElStopped());
        QWidget* dial = applet.dial();
        const QPoint at = applet.dial()->pointForElevation(60.0).toPoint();
        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, at);
        QVERIFY(applet.dial()->selectingElevation());
        QVERIFY(sink.targets.isEmpty());
        QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 212.0);
        QVERIFY(std::abs(sink.targets.at(0).elevation - 60.0) <= 1.0);
    }

    void noRotorIsGreyedWithItsReason()
    {
        RotorModel rotor;   // driver None, disabled
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.resize(260, 520);
        applet.show();

        QCOMPARE(plain(label(applet, "rotorStatus")), QStringLiteral("Not connected"));
        QVERIFY(label(applet, "rotorStatusDot")->styleSheet().contains(QStringLiteral("#404858")));
        QCOMPARE(plain(label(applet, "rotorHeading")), QStringLiteral("---°"));
        QVERIFY(label(applet, "rotorHeading")->text().contains(QStringLiteral("#506070")));
        QCOMPARE(applet.disabledReason(), kNoRotor);
        QCOMPARE(label(applet, "rotorReason")->text(), kNoRotor);
        QVERIFY(label(applet, "rotorReason")->isVisible());
        // Disabled, never hidden.
        for (QWidget* w : controls(applet)) {
            QVERIFY2(!w->isEnabled(), qPrintable(w->objectName()));
            QVERIFY2(w->isVisible(), qPrintable(w->objectName()));
        }

        // A greyed dial sends nothing.
        const QPoint at = applet.dial()->pointForBearing(90.0).toPoint();
        QTest::mousePress(applet.dial(), Qt::LeftButton, Qt::NoModifier, at);
        QTest::mouseRelease(applet.dial(), Qt::LeftButton, Qt::NoModifier, at);
        QVERIFY(sink.targets.isEmpty());

        // Set up but not connected: its own reason.
        RotorModel::State s = azimuthTurning();
        s.connectionPhase = TunerModel::ConnectionPhase::Retrying;
        rotor.setState(s);
        QCOMPARE(applet.disabledReason(), kNotConnected);
        QCOMPARE(label(applet, "rotorReason")->text(), kNotConnected);
        QCOMPARE(plain(label(applet, "rotorStatus")),
                 QStringLiteral("Easy Rotor Control on COM4 · Connecting"));
        QVERIFY(!button(applet, "rotorStop")->isEnabled());
    }

    void aCoreTooOldIsGreyedWithItsReason()
    {
        // A window on a remote Core with no rotor control (here no link at
        // all, which RadioModel answers the same way).
        RadioModel window(RadioModel::Role::Remote);
        RotorApplet applet(&window);
        applet.resize(330, 520);
        applet.show();

        QString reason;
        QVERIFY(!window.rotorControlAvailable(&reason));
        QCOMPARE(reason, IStationLink::rotorUnavailableReason());
        QCOMPARE(applet.disabledReason(),
                 QStringLiteral("This Core does not control a rotor. Updating the Core may help."));
        QCOMPARE(label(applet, "rotorReason")->text(), applet.disabledReason());
        for (QWidget* w : controls(applet)) {
            QVERIFY2(!w->isEnabled(), qPrintable(w->objectName()));
            QVERIFY2(w->isVisible(), qPrintable(w->objectName()));
        }

        // The Core's reason comes first even while the rotor object says a
        // rotor is connected.
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        sink.available = false;
        sink.unavailableReason = IStationLink::rotorUnavailableReason();
        RotorApplet faked(nullptr, &rotor, &sink);
        QCOMPARE(faked.disabledReason(), IStationLink::rotorUnavailableReason());
        QVERIFY(!faked.dial()->isEnabled());
    }

    void aStaleHeadingIsMutedWithItsAge()
    {
        RotorModel rotor;
        RotorModel::State s = azimuthTurning();
        rotor.setState(s);
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        s.positionFresh = false;
        rotor.setState(s);
        QVERIFY(label(applet, "rotorHeading")->text().contains(QStringLiteral("#a6b0bc")));
        QVERIFY(!label(applet, "rotorHeading")->text().contains(QStringLiteral("#ffb800")));
        QVERIFY(plain(label(applet, "rotorTarget")).startsWith(QStringLiteral("Last heard ")));
        QVERIFY(plain(label(applet, "rotorTarget")).endsWith(QStringLiteral("s ago")));
    }

    // ── Behaviour ──────────────────────────────────────────────────

    void theTurnButtonsSendTheHoldDeadMan()
    {
        RotorModel rotor;
        rotor.setState(azElStopped());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.show();

        const struct { const char* name; RotorCommandSink::Nudge direction; } kButtons[] = {
            {"rotorCw", RotorCommandSink::Nudge::Cw},
            {"rotorCcw", RotorCommandSink::Nudge::Ccw},
            {"rotorUp", RotorCommandSink::Nudge::Up},
            {"rotorDown", RotorCommandSink::Nudge::Down},
        };
        for (const auto& b : kButtons) {
            sink.nudges.clear();
            QPushButton* btn = button(applet, b.name);
            QTest::mousePress(btn, Qt::LeftButton);
            QCOMPARE(sink.nudges.size(), 1);
            QVERIFY(sink.nudges.at(0).active);
            QCOMPARE(sink.nudges.at(0).direction, b.direction);
            // Repeats every 250 ms while held.
            QTRY_VERIFY_WITH_TIMEOUT(sink.nudges.size() >= 3, 2000);
            for (const auto& n : sink.nudges) {
                QVERIFY(n.active);
                QCOMPARE(n.direction, b.direction);
            }
            QTest::mouseRelease(btn, Qt::LeftButton);
            QCOMPARE(sink.nudges.last().active, false);
            QCOMPARE(sink.nudges.last().direction, b.direction);
            // Nothing more once let go.
            const qsizetype after = sink.nudges.size();
            QTest::qWait(RotorApplet::kHoldRepeatMs * 2);
            QCOMPARE(sink.nudges.size(), after);
        }

        // A hold ends when the rotor goes away mid-hold.
        sink.nudges.clear();
        QTest::mousePress(button(applet, "rotorCw"), Qt::LeftButton);
        RotorModel::State gone = azElStopped();
        gone.connectionPhase = TunerModel::ConnectionPhase::Retrying;
        rotor.setState(gone);
        QCOMPARE(sink.nudges.last().active, false);
        const qsizetype after = sink.nudges.size();
        QTest::qWait(RotorApplet::kHoldRepeatMs * 2);
        QCOMPARE(sink.nudges.size(), after);
    }

    // Final review M3: a hold whose release never arrives still ends. The
    // repeat finds the button no longer down; hiding or losing activation
    // ends it at once.
    void aHoldEndsWhenTheButtonIsNoLongerDown()
    {
        RotorModel rotor;
        rotor.setState(azElStopped());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.show();
        QPushButton* cw = button(applet, "rotorCw");
        QTest::mousePress(cw, Qt::LeftButton);
        QCOMPARE(sink.nudges.size(), 1);
        {
            // The release is lost: the button comes up with no signal.
            QSignalBlocker quiet(cw);
            cw->setDown(false);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!sink.nudges.last().active, 2000);
        QCOMPARE(sink.nudges.last().direction, RotorCommandSink::Nudge::Cw);
        const qsizetype after = sink.nudges.size();
        QTest::qWait(RotorApplet::kHoldRepeatMs * 2);
        QCOMPARE(sink.nudges.size(), after);
    }

    void aHoldEndsWhenTheAppletHides()
    {
        RotorModel rotor;
        rotor.setState(azElStopped());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.show();
        QTest::mousePress(button(applet, "rotorCcw"), Qt::LeftButton);
        QCOMPARE(sink.nudges.size(), 1);
        applet.hide();
        QCOMPARE(sink.nudges.size(), 2);
        QCOMPARE(sink.nudges.last().active, false);
        QCOMPARE(sink.nudges.last().direction, RotorCommandSink::Nudge::Ccw);
        QTest::qWait(RotorApplet::kHoldRepeatMs * 2);
        QCOMPARE(sink.nudges.size(), 2);
    }

    void aHoldEndsWhenTheWindowLosesActivation()
    {
        RotorModel rotor;
        rotor.setState(azElStopped());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.show();
        QTest::mousePress(button(applet, "rotorCw"), Qt::LeftButton);
        QCOMPARE(sink.nudges.size(), 1);
        QWidget other;
        if (applet.isActiveWindow()) {
            // Another window takes the activation.
            other.show();
            other.activateWindow();
            QTRY_VERIFY(!applet.isActiveWindow());
        } else {
            // The window was never active here; say it changed.
            QEvent deactivated(QEvent::ActivationChange);
            QCoreApplication::sendEvent(&applet, &deactivated);
        }
        QTRY_COMPARE(sink.nudges.size(), 2);
        QCOMPARE(sink.nudges.last().active, false);
    }

    // Rotor control plan Task 8: a remote Core's refusal of the applet's
    // command comes the accessory way (device "rotor"). While the applet is
    // on screen it shows the refusal itself, so MainWindow adds no notice;
    // once hidden, the notice says it.
    void aRefusalOfTheAppletsCommandIsShownOnThePage()
    {
        RadioModel window(RadioModel::Role::Remote);
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        sink.lastId = 77;
        RotorApplet applet(&window, &rotor, &sink);
        applet.resize(330, 620);
        applet.show();
        QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);

        applet.findChildren<QPushButton*>(QStringLiteral("rotorPreset")).at(0)->click();
        QCOMPARE(sink.targets.size(), 1);
        window.reportStationAccessoryRefusal(QStringLiteral("rotor"), kNotConnected, 77);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("rotor"));
        QVERIFY(refused.at(0).at(2).toBool());

        sink.lastId = 78;
        applet.findChildren<QPushButton*>(QStringLiteral("rotorPreset")).at(1)->click();
        applet.hide();
        window.reportStationAccessoryRefusal(QStringLiteral("rotor"), kNotConnected, 78);
        QCOMPARE(refused.count(), 2);
        QVERIFY(!refused.at(1).at(2).toBool());
    }

    void stopSendsStop()
    {
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        button(applet, "rotorStop")->click();
        QCOMPARE(sink.stops, 1);
    }

    void draggingTheDialSendsOneTurnOnRelease()
    {
        RotorModel rotor;
        RotorModel::State s = azimuthTurning();
        s.targetAzimuthDeg = -1.0;
        s.motion = RotorModel::Motion::Stopped;
        rotor.setState(s);
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        applet.resize(330, 620);
        applet.show();
        RotorDialWidget* dial = applet.dial();
        QSignalSpy released(dial, &RotorDialWidget::targetReleased);

        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier,
                          dial->pointForBearing(90.0).toPoint());
        QVERIFY(dial->selecting());
        for (double deg : {100.0, 120.0, 150.0, 180.0}) {
            QMouseEvent move(QEvent::MouseMove, dial->pointForBearing(deg),
                             dial->mapToGlobal(dial->pointForBearing(deg)),
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(dial, &move);
            // Nothing is sent during the drag.
            QVERIFY(sink.targets.isEmpty());
        }
        QVERIFY(std::abs(dial->selectionDeg() - 180.0) <= 1.0);
        QCOMPARE(dial->state(), RotorDialWidget::State::Targeted);
        // The readout follows the drag.
        QVERIFY(plain(label(applet, "rotorTarget")).startsWith(QStringLiteral("180°")));

        QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier,
                            dial->pointForBearing(180.0).toPoint());
        QCOMPARE(sink.targets.size(), 1);
        QVERIFY(std::abs(sink.targets.at(0).azimuth - 180.0) <= 1.0);
        QCOMPARE(sink.targets.at(0).elevation, -1.0);
        QCOMPARE(released.size(), 1);
        QVERIFY(!dial->selecting());

        // A refusal drops the drawn target and says why.
        sink.accept = false;
        sink.refusal = kNotConnected;
        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier,
                          dial->pointForBearing(270.0).toPoint());
        QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier,
                            dial->pointForBearing(270.0).toPoint());
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(dial->state(), RotorDialWidget::State::Idle);
        QCOMPARE(label(applet, "rotorReason")->text(), kNotConnected);
    }

    void turnToACallsignTakesThePath()
    {
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        auto* call = applet.findChild<QLineEdit*>(QStringLiteral("rotorCall"));

        // Empty sends nothing.
        button(applet, "rotorTurnTo")->click();
        QVERIFY(sink.calls.isEmpty());

        call->setText(QStringLiteral(" ja1abc "));
        button(applet, "rotorTurnTo")->click();
        QCOMPARE(sink.calls.size(), 1);
        QCOMPARE(sink.calls.at(0).first, QStringLiteral("JA1ABC"));
        QCOMPARE(sink.calls.at(0).second, false);

        button(applet, "rotorLongPath")->click();
        QVERIFY(applet.longPath());
        QVERIFY(!button(applet, "rotorShortPath")->isChecked());
        QTest::keyClick(call, Qt::Key_Return);
        QCOMPARE(sink.calls.size(), 2);
        QCOMPARE(sink.calls.at(1).second, true);

        // The Core's refusal, word for word.
        sink.accept = false;
        sink.refusal = QStringLiteral("Set your grid square in Setup to turn the beam to spots.");
        button(applet, "rotorTurnTo")->click();
        QCOMPARE(label(applet, "rotorReason")->text(), sink.refusal);
        QVERIFY(!label(applet, "rotorReason")->isHidden());
        // The next command that goes clears it.
        sink.accept = true;
        button(applet, "rotorStop")->click();
        QVERIFY(label(applet, "rotorReason")->text().isEmpty());
    }

    void stopIsTheOnlyRedControl()
    {
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        const QString red = QStringLiteral("#7a1c1c");
        int reds = 0;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->styleSheet().contains(red, Qt::CaseInsensitive)) {
                ++reds;
                QCOMPARE(b->objectName(), QStringLiteral("rotorStop"));
            }
        }
        QCOMPARE(reds, 1);
    }

    void everyStringIsPlain()
    {
        RotorModel rotor;
        rotor.setState(azimuthTurning());
        FakeSink sink;
        RotorApplet applet(nullptr, &rotor, &sink);
        QStringList texts{applet.appletTitle(), kNoRotor, kNotConnected,
                          IStationLink::rotorUnavailableReason()};
        for (QPushButton* b : applet.findChildren<QPushButton*>()) { texts.append(b->text()); }
        for (QLabel* l : applet.findChildren<QLabel*>()) {
            if (!plain(l).isEmpty()) { texts.append(plain(l)); }
        }
        texts.append(applet.findChild<QLineEdit*>(QStringLiteral("rotorCall"))->placeholderText());
        for (const QString& t : texts) {
            QVERIFY2(OperatorWording::isPlain(t), qPrintable(t));
            QVERIFY2(!t.contains(QChar(0x2014)), qPrintable(t));
        }
    }
};

QTEST_MAIN(TestRotorApplet)
#include "tst_rotor_applet.moc"
