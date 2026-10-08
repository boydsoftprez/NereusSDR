// no-port-check: NereusSDR-original tests for the compass meter item's drag
// to turn (rotor control plan, Task 5).
//
// The behaviour under test is ported from Thetis renderRotator() and
// clsRotatorItem (MeterManager.cs:16475-16717, 36697-37217 [v2.10.3.15]):
// a drag shows the would-be heading and sends nothing; letting go sends one
// turn; a press on the centre circle stops. With no rotor the dial does
// nothing. The elevation face shows the elevation it is given.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 5).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  The fake answers the rest of the sink
//                                    (rotor control plan, Task 6).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTest>

#include "gui/meters/RotatorItem.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"

#include <cmath>
#include <numbers>

using namespace NereusSDR;
using RotorLink::RotorModel;

namespace {

constexpr int kW = 400;
constexpr int kH = 200;

class FakeCommands : public RotorCommandSink {
public:
    struct Target {
        double azimuth;
        double elevation;
    };
    QList<Target> targets;
    int stops = 0;
    bool accept = true;

    bool requestRotorTarget(double azimuthDeg, double elevationDeg, QString* reason) override
    {
        if (!accept) {
            if (reason) { *reason = QStringLiteral("refused"); }
            return false;
        }
        targets.append({azimuthDeg, elevationDeg});
        return true;
    }
    bool requestStopRotor(QString* reason) override
    {
        if (!accept) {
            if (reason) { *reason = QStringLiteral("refused"); }
            return false;
        }
        ++stops;
        return true;
    }
    bool rotorControlAvailable(QString*) const override { return true; }
    bool requestTurnRotorToCall(const QString&, bool, QString*) override { return accept; }
    bool requestNudgeRotor(Nudge, bool, QString*) override { return accept; }
    quint32 lastRotorCommandId() const override { return 0; }
};

RotorModel::State connectedRotor(RotorModel::Axes axes = RotorModel::Axes::AzimuthElevation)
{
    RotorModel::State s;
    s.driver = RotorModel::Driver::Gs232b;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.axes = axes;
    s.positionFresh = true;
    s.azimuthDeg = 90.0;
    s.elevationDeg = axes == RotorModel::Axes::AzimuthElevation ? 10.0 : -1.0;
    return s;
}

// The point `radius` from `centre` at compass `degrees` (0 up, clockwise).
QPointF compassPoint(const QPointF& centre, double degrees, double radius)
{
    const double rad = degrees * std::numbers::pi / 180.0;
    return {centre.x() + radius * std::sin(rad), centre.y() - radius * std::cos(rad)};
}

// The point on the elevation face at `degrees` above the horizon.
QPointF elevationPoint(const QPointF& centre, double degrees, double radius)
{
    const double rad = degrees * std::numbers::pi / 180.0;
    return {centre.x() + radius * std::cos(rad), centre.y() - radius * std::sin(rad)};
}

bool press(RotatorItem& item, const QPointF& at)
{
    QMouseEvent e(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    return item.handleMousePress(&e, kW, kH);
}

bool move(RotatorItem& item, const QPointF& at)
{
    QMouseEvent e(QEvent::MouseMove, at, at, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    return item.handleMouseMove(&e, kW, kH);
}

bool release(RotatorItem& item, const QPointF& at)
{
    QMouseEvent e(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    return item.handleMouseRelease(&e, kW, kH);
}

} // namespace

class TestRotatorItemDrag : public QObject {
    Q_OBJECT

private slots:
    void aDragShowsTheHeadingAndLettingGoTurns()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;
        item.setRotor(&rotor, &commands);
        QVERIFY(item.rotorControllable());

        const QPointF az = item.azimuthCentre(kW, kH);
        const float r = item.pointerRadius(kW, kH);
        QVERIFY(press(item, compassPoint(az, 10.0, r * 0.6)));
        QVERIFY(move(item, compassPoint(az, 80.0, r * 0.6)));
        QVERIFY(move(item, compassPoint(az, 135.5, r * 0.9)));
        QVERIFY(std::abs(item.dragDegrees() - 135.5f) < 0.01f);
        QVERIFY(!item.draggingElevation());
        // Nothing is sent while dragging.
        QVERIFY(commands.targets.isEmpty());
        QCOMPARE(commands.stops, 0);

        // Let go outside the item: still one turn, to the whole degree.
        QVERIFY(item.hitTest(QPointF(kW + 50, kH + 50), kW, kH));
        QVERIFY(release(item, compassPoint(az, 135.5, r * 3.0)));
        QCOMPARE(commands.targets.size(), 1);
        QCOMPARE(commands.targets.first().azimuth, 135.0);
        QCOMPARE(commands.targets.first().elevation, -1.0);
        QCOMPARE(commands.stops, 0);
        QCOMPARE(item.targetAzimuthMarker(), 135.0f);
        QCOMPARE(item.dragDegrees(), RotatorItem::kNoAngle);
        QVERIFY(!item.hitTest(QPointF(kW + 50, kH + 50), kW, kH));

        // The marker stays until the arrow comes within 3 degrees.
        for (int i = 0; i < 10; ++i) { item.setValue(135.0); }
        QVERIFY(std::abs(item.displayedAzimuth() - 135.0f) > 3.0f);
        QCOMPARE(item.targetAzimuthMarker(), 135.0f);
        for (int i = 0; i < 60; ++i) { item.setValue(135.0); }
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);
    }

    void aDragAcrossNorthWrapsToTheShortWay()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;
        item.setMode(RotatorItem::RotatorMode::Az);
        item.setRotor(&rotor, &commands);
        const QPointF az = item.azimuthCentre(kW, kH);
        const float r = item.pointerRadius(kW, kH);
        QVERIFY(press(item, compassPoint(az, 350.5, r * 0.8)));
        QVERIFY(release(item, compassPoint(az, 350.5, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);
        QCOMPARE(commands.targets.first().azimuth, 350.0);
        // At 359 the target is reached from 1 (the short way round).
        item.setValue(1.0);
        for (int i = 0; i < 60; ++i) { item.setValue(1.0); }
        QVERIFY(press(item, compassPoint(az, 359.5, r * 0.8)));
        QVERIFY(release(item, compassPoint(az, 359.5, r * 0.8)));
        QCOMPARE(commands.targets.last().azimuth, 359.0);
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);
    }

    void aPressOnTheCentreCircleStops()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;
        item.setRotor(&rotor, &commands);
        const QPointF az = item.azimuthCentre(kW, kH);
        const float stop = item.stopCircleRadius(kW, kH);

        QVERIFY(press(item, az + QPointF(stop * 0.5, 0.0)));
        QCOMPARE(commands.stops, 1);
        QVERIFY(commands.targets.isEmpty());
        // Thetis's MouseUp on the circle stops as well.
        QVERIFY(release(item, az));
        QCOMPARE(commands.stops, 2);
        QVERIFY(commands.targets.isEmpty());

        // A drag that comes back onto the circle and lets go stops, and
        // sends no heading.
        const float r = item.pointerRadius(kW, kH);
        QVERIFY(press(item, compassPoint(az, 200.0, r * 0.8)));
        QVERIFY(move(item, az));
        QCOMPARE(item.dragDegrees(), RotatorItem::kNoAngle);
        QVERIFY(release(item, az));
        QCOMPARE(commands.stops, 3);
        QVERIFY(commands.targets.isEmpty());
    }

    void withNoRotorTheDialDoesNothing()
    {
        RotatorItem bare;
        const QPointF az = bare.azimuthCentre(kW, kH);
        QVERIFY(!bare.rotorControllable());
        QVERIFY(!press(bare, compassPoint(az, 90.0, 20.0)));

        RotorModel rotor;  // driver none
        FakeCommands commands;
        RotatorItem item;
        item.setRotor(&rotor, &commands);
        QVERIFY(!item.rotorControllable());
        QVERIFY(!press(item, compassPoint(az, 90.0, 20.0)));
        QVERIFY(!press(item, az));
        QVERIFY(!release(item, az));
        QVERIFY(commands.targets.isEmpty());
        QCOMPARE(commands.stops, 0);
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);

        // A rotor that drops mid drag sends nothing on release and keeps no
        // target.
        rotor.setState(connectedRotor());
        QVERIFY(item.rotorControllable());
        const float r = item.pointerRadius(kW, kH);
        QVERIFY(press(item, compassPoint(az, 45.5, r * 0.8)));
        QVERIFY(release(item, compassPoint(az, 45.5, r * 0.8)));
        QCOMPARE(item.targetAzimuthMarker(), 45.0f);
        QVERIFY(press(item, compassPoint(az, 100.5, r * 0.8)));
        RotorModel::State gone = connectedRotor();
        gone.connectionPhase = TunerModel::ConnectionPhase::Disconnected;
        rotor.setState(gone);
        QVERIFY(!item.rotorControllable());
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);
        QVERIFY(!release(item, compassPoint(az, 100.5, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);

        // A refused turn shows no target.
        rotor.setState(connectedRotor());
        commands.accept = false;
        QVERIFY(press(item, compassPoint(az, 200.5, r * 0.8)));
        QTest::ignoreMessage(QtWarningMsg, "The rotor did not take the turn: \"refused\"");
        QVERIFY(release(item, compassPoint(az, 200.5, r * 0.8)));
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);
    }

    void elevationShowsTheValueSet()
    {
        RotatorItem item;
        QCOMPARE(item.displayedElevation(), 0.0f);
        for (int i = 0; i < 80; ++i) { item.setElevation(30.0f); }
        QVERIFY(std::abs(item.displayedElevation() - 30.0f) < 0.01f);
        QCOMPARE(item.elevation(), 30.0f);
        // Straight up reads 90, not 0 (Thetis's % 90f).
        for (int i = 0; i < 80; ++i) { item.setElevation(90.0f); }
        QVERIFY(std::abs(item.displayedElevation() - 90.0f) < 0.01f);
        // Out of range is held to 0..90 as Thetis holds Reading.ELE.
        for (int i = 0; i < 80; ++i) { item.setElevation(120.0f); }
        QVERIFY(item.displayedElevation() <= 90.0f);
        QVERIFY(std::abs(item.displayedElevation() - 90.0f) < 0.01f);

        // The rotor's elevation reaches the dial.
        RotorModel rotor;
        RotatorItem fed;
        FakeCommands commands;
        fed.setRotor(&rotor, &commands);
        RotorModel::State s = connectedRotor();
        s.elevationDeg = 45.0;
        rotor.setState(s);
        QVERIFY(fed.displayedElevation() > 0.0f);
        // The feed re-applies the heading every 100 ms until the dial is there.
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(fed.displayedElevation() - 45.0f) < 0.05f
                                 && std::abs(fed.displayedAzimuth() - 90.0f) < 0.05f, 10000);

        // And the face draws it: something is painted on the elevation face
        // where the 45 degree pointer runs.
        QImage image(kW, kH, QImage::Format_ARGB32);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            fed.paint(p, kW, kH);
        }
        const QPointF ele = fed.elevationCentre(kW, kH);
        const QPointF onPointer = elevationPoint(ele, 45.0, fed.pointerRadius(kW, kH) * 0.6);
        QVERIFY(image.pixelColor(onPointer.toPoint()) != QColor(Qt::black));
    }

    void dragOnTheElevationFaceSetsElevation()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;  // Both
        item.setRotor(&rotor, &commands);
        QVERIFY(item.elevationControllable());
        const QPointF ele = item.elevationCentre(kW, kH);
        const float r = 0.39f * kH;
        QVERIFY(press(item, elevationPoint(ele, 30.0, r * 0.8)));
        QVERIFY(move(item, elevationPoint(ele, 60.5, r * 0.8)));
        QVERIFY(item.draggingElevation());
        // Below the horizon holds at 0 once the drag is showing.
        QVERIFY(move(item, elevationPoint(ele, -20.0, r * 0.8)));
        QCOMPARE(item.dragDegrees(), 0.0f);
        QVERIFY(move(item, elevationPoint(ele, 60.5, r * 0.8)));
        QVERIFY(commands.targets.isEmpty());
        QVERIFY(release(item, elevationPoint(ele, 60.5, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);
        // The azimuth goes with it: where the rotor points (no target).
        QCOMPARE(commands.targets.first().azimuth, 90.0);
        QCOMPARE(commands.targets.first().elevation, 60.0);
        QCOMPARE(item.targetElevationMarker(), 60.0f);

        // An azimuth-only rotor takes no elevation drag.
        rotor.setState(connectedRotor(RotorModel::Axes::Azimuth));
        QVERIFY(!item.elevationControllable());
        QVERIFY(!press(item, elevationPoint(ele, 30.0, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);
    }
};

QTEST_MAIN(TestRotatorItemDrag)
#include "tst_rotator_item_drag.moc"
