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
//   2026-10-26  J.J. Boyd / KG4VCF  Thetis rotator settings (container item
//                                    parity): older records, the 100 ms new
//                                    item, the beam width rows, padding,
//                                    full background, dark mode, the
//                                    elevation on variable 2 and inert
//                                    preview copies. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Allow control (off by default) gates
//                                    the rotor link too; the drags allow
//                                    it, the preview and new live copies
//                                    show it off. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/RotatorItemEditor.h"
#include "gui/meters/MeterWidget.h"
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
    bool requestConfigureRotor(const Setup&, QString*) override { return accept; }
    bool requestRotorPresets(const QString&, QString*) override { return accept; }
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
        item.setAllowControl(true);
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
        item.setAllowControl(true);
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
        item.setAllowControl(true);
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
        item.setAllowControl(true);
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
        // Not the black canvas, nor the full background under the face.
        QVERIFY(image.pixelColor(onPointer.toPoint()) != QColor(Qt::black));
        QVERIFY(image.pixelColor(onPointer.toPoint()) != fed.backgroundColour());
    }

    void dragOnTheElevationFaceSetsElevation()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;  // Both
        item.setAllowControl(true);
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

    // ── Thetis rotator settings (container item parity) ──

    void anOlderRecordRoundTripsUnchanged()
    {
        // Saved before fields 22 and 23: Azimuth, beam shown at 45.5,
        // dark mode, padding 0.5.
        const QString record = QStringLiteral(
            "ROTATOR|0.25|0.5|0.5|0.25|300|3|0|1|1|1|45.5|0.75|1|0.5|"
            "#ffff0000|#ffffffff|#ff808080|#ffffffff|#ff404040|#ff202020|");
        QCOMPARE(record.split(QLatin1Char('|')).size(), 22);
        RotatorItem item;
        QVERIFY(item.deserialize(record));
        QVERIFY(!item.fullBackground());
        QVERIFY(!item.paddingLayout());
        QCOMPARE(item.mode(), RotatorItem::RotatorMode::Az);
        QCOMPARE(item.beamWidth(), 45.5f);
        QVERIFY(item.darkMode());
        // The 22 fields come back as saved; the appended pair reads off,
        // and control stays on, on the rotor link path (fields 24-30).
        QVERIFY(item.allowControl());
        QVERIFY(item.serialize().startsWith(record + QStringLiteral("|0|0|1|")));
        QCOMPARE(item.serialize().split(QLatin1Char('|')).size(), 31);
        RotatorItem again;
        QVERIFY(again.deserialize(item.serialize()));
        QCOMPARE(again.serialize(), item.serialize());

        // Through the content registry: no overrides, the record unchanged.
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        entry.config.insert(QStringLiteral("legacyRecord"), record);
        entry.canvasRect = {};
        entry.paintOrder = 3;  // the record's z-order
        MeterWidget widget;
        MeterItem* restored = registry.createMeterItem(entry, &widget, ContentRenderMode::Live);
        QVERIFY(restored);
        auto* rotator = qobject_cast<RotatorItem*>(restored);
        QVERIFY(rotator);
        QVERIFY(!rotator->fullBackground());
        QVERIFY(!rotator->paddingLayout());
        const ContentEntry captured = registry.captureMeterItem(*restored, entry);
        QCOMPARE(captured.config.value(QStringLiteral("legacyRecord")).toString(), record);
        const QJsonObject overrides = captured.config.value(QStringLiteral("overrides")).toObject();
        QVERIFY2(overrides.isEmpty(), qPrintable(QJsonDocument(overrides).toJson(QJsonDocument::Compact)));
        // A setting changed on it later saves and restores over the record.
        rotator->setFullBackground(true);
        const ContentEntry changed = registry.captureMeterItem(*restored, captured);
        QCOMPARE(changed.config.value(QStringLiteral("legacyRecord")).toString(), record);
        auto* reloaded = qobject_cast<RotatorItem*>(
            registry.createMeterItem(changed, &widget, ContentRenderMode::Live));
        QVERIFY(reloaded);
        QVERIFY(reloaded->fullBackground());
        QVERIFY(!reloaded->paddingLayout());

        // The appended fields round-trip.
        RotatorItem both;
        QCOMPARE(both.serialize().split(QLatin1Char('|')).size(), 31);
        both.setPaddingLayout(false);
        RotatorItem copy;
        QVERIFY(copy.deserialize(both.serialize()));
        QVERIFY(copy.fullBackground());
        QVERIFY(!copy.paddingLayout());
        QCOMPARE(copy.serialize(), both.serialize());
    }

    void aNewItemUpdatesAt100Ms()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        QCOMPARE(entry.context.value(QStringLiteral("updateIntervalMs")).toInt(), 100);
        MeterWidget widget;
        auto* item = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &widget, ContentRenderMode::Live));
        QVERIFY(item);
        QCOMPARE(item->updateIntervalMs(), RotatorItem::kThetisUpdateIntervalMs);
        QCOMPARE(item->updateIntervalMs(), 100);
        // A new item draws as Thetis does; the existing defaults are kept.
        QVERIFY(item->fullBackground());
        QVERIFY(item->paddingLayout());
        QCOMPARE(item->mode(), RotatorItem::RotatorMode::Both);
        QCOMPARE(item->beamWidth(), 30.0f);
        QCOMPARE(item->padding(), 0.5f);
    }

    void theBeamWidthRowsFollowShowBeamWidth()
    {
        RotatorItem item;
        RotatorItemEditor editor;
        editor.setItem(&item);
        auto* show = editor.findChild<QCheckBox*>(QStringLiteral("rotatorShowBeamWidth"));
        auto* width = editor.findChild<QDoubleSpinBox*>(QStringLiteral("rotatorBeamWidth"));
        auto* alpha = editor.findChild<QDoubleSpinBox*>(QStringLiteral("rotatorBeamAlpha"));
        auto* colour = editor.findChild<QPushButton*>(QStringLiteral("rotatorBeamWidthColour"));
        QVERIFY(show && width && alpha && colour);
        // Thetis nudMeterItemRotatorBeamWidth: 0 to 180 in 0.05 steps.
        QCOMPARE(width->minimum(), 0.0);
        QCOMPARE(width->maximum(), 180.0);
        QCOMPARE(width->singleStep(), 0.05);
        QVERIFY(!item.showBeamWidth());
        QVERIFY(!show->isChecked());
        QVERIFY(!width->isEnabled());
        QVERIFY(!alpha->isEnabled());
        QVERIFY(!colour->isEnabled());
        show->setChecked(true);
        QVERIFY(item.showBeamWidth());
        QVERIFY(width->isEnabled());
        QVERIFY(alpha->isEnabled());
        QVERIFY(colour->isEnabled());
        width->setValue(120.5);
        QCOMPARE(item.beamWidth(), 120.5f);
        show->setChecked(false);
        QVERIFY(!width->isEnabled());

        // An item loaded with the beam on shows the rows enabled.
        RotatorItem shown;
        shown.setShowBeamWidth(true);
        editor.setItem(&shown);
        QVERIFY(width->isEnabled());
    }

    void editorRowsForPaddingAndFullBackground()
    {
        RotatorItem item;
        QVERIFY(item.deserialize(QStringLiteral(
            "ROTATOR|0|0|1|1|300|3|0|1|0|0|30|0.6|0|0.5|"
            "#ffff0000|#ffffffff|#ff808080|#ffffffff|#ff404040|#ff202020|")));
        RotatorItemEditor editor;
        editor.setItem(&item);
        auto* padding = editor.findChild<QDoubleSpinBox*>(QStringLiteral("rotatorPadding"));
        auto* full = editor.findChild<QCheckBox*>(QStringLiteral("rotatorFullBackground"));
        QVERIFY(padding && full);
        QCOMPARE(padding->minimum(), 0.001);
        QCOMPARE(padding->maximum(), 1.0);
        QVERIFY(!full->isChecked());
        QVERIFY(!item.paddingLayout());
        // Setting padding on an older item lets it size the face.
        padding->setValue(0.75);
        QCOMPARE(item.padding(), 0.75f);
        QVERIFY(item.paddingLayout());
        full->setChecked(true);
        QVERIFY(item.fullBackground());
        // Thetis holds padding to at least 0.001.
        item.setPadding(0.0f);
        QCOMPARE(item.padding(), RotatorItem::kMinPadding);
    }

    void paddingSizesTheAzimuthFace()
    {
        RotatorItem item;  // a new item: padding sizes the face
        item.setMode(RotatorItem::RotatorMode::Az);
        item.setPadding(0.3f);
        // 0.3 of 400 wide is 120, centred across and at the top.
        QCOMPARE(item.azimuthCentre(kW, kH), QPointF(200.0, 60.0));
        item.setPadding(1.0f);
        // Held to the item's height.
        QCOMPARE(item.azimuthCentre(kW, kH), QPointF(200.0, 100.0));
        QCOMPARE(item.stopCircleRadius(kW, kH), (kH * 0.15f) / 2.0f);

        // An older item keeps the largest square, centred.
        item.setPadding(0.3f);
        item.setPaddingLayout(false);
        QCOMPARE(item.azimuthCentre(kW, kH), QPointF(200.0, 100.0));

        // Both is laid out the same either way.
        RotatorItem both;
        const QPointF before = both.azimuthCentre(kW, kH);
        both.setPadding(0.3f);
        both.setPaddingLayout(false);
        QCOMPARE(both.azimuthCentre(kW, kH), before);
    }

    void fullBackgroundFillsTheItem()
    {
        RotatorItem item;
        item.setMode(RotatorItem::RotatorMode::Az);
        item.setBackgroundColour(QColor(10, 80, 30));
        QImage image(kW, kH, QImage::Format_ARGB32);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            item.paint(p, kW, kH);
        }
        QCOMPARE(image.pixelColor(2, kH - 2), QColor(10, 80, 30));
        QCOMPARE(image.pixelColor(kW - 2, 2), QColor(10, 80, 30));

        item.setFullBackground(false);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            item.paint(p, kW, kH);
        }
        QCOMPARE(image.pixelColor(2, kH - 2), QColor(Qt::black));
    }

    void darkModeDrawsTheDarkImage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage plain(64, 64, QImage::Format_ARGB32);
        plain.fill(QColor(200, 0, 0));
        QImage dark(64, 64, QImage::Format_ARGB32);
        dark.fill(QColor(0, 0, 200));
        const QString plainPath = dir.filePath(QStringLiteral("face.png"));
        const QString darkPath = dir.filePath(QStringLiteral("face-dark.png"));
        QVERIFY(plain.save(plainPath));
        QVERIFY(dark.save(darkPath));

        RotatorItem item;
        item.setMode(RotatorItem::RotatorMode::Az);
        item.setBackgroundImagePath(plainPath);
        const QPointF centre = item.azimuthCentre(kW, kH);
        // Inside the disc, clear of the ring, the arrow and the readout.
        const QPoint probe = (centre - QPointF(item.pointerRadius(kW, kH) * 0.5, 0.0)).toPoint();
        const auto drawn = [&] {
            QImage image(kW, kH, QImage::Format_ARGB32);
            image.fill(Qt::black);
            QPainter p(&image);
            item.paint(p, kW, kH);
            p.end();
            return image.pixelColor(probe);
        };
        QCOMPARE(drawn(), QColor(200, 0, 0));
        QCOMPARE(item.effectiveBackgroundImagePath(), plainPath);
        item.setDarkMode(true);
        QCOMPARE(drawn(), QColor(0, 0, 200));
        QCOMPARE(item.effectiveBackgroundImagePath(), darkPath);
        // Without a "-dark" file, dark mode draws the chosen one.
        QVERIFY(QFile::remove(darkPath));
        RotatorItem other;
        other.setMode(RotatorItem::RotatorMode::Az);
        other.setDarkMode(true);
        other.setBackgroundImagePath(plainPath);
        QImage image(kW, kH, QImage::Format_ARGB32);
        QPainter p(&image);
        other.paint(p, kW, kH);
        p.end();
        QCOMPARE(other.effectiveBackgroundImagePath(), plainPath);
    }

    void variableTwoFeedsTheElevation()
    {
        // From Thetis MeterManager.cs:22720 [v2.10.3.15]: ROTATOR reads 2.
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("ROTATOR")), 2);
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        const QUuid guid = QUuid::createUuid();
        entry.context.insert(QStringLiteral("mmioVariables"),
                             QJsonArray{QJsonObject{{"guid", guid.toString(QUuid::WithoutBraces)}, {"variable", "ele"}}});
        MeterWidget widget;
        auto* item = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &widget, ContentRenderMode::Live));
        QVERIFY(item);
        QVERIFY(item->hasMmioBinding(1));
        widget.addItem(item);
        for (int i = 0; i < 80; ++i) { widget.updateMmioValue(item, 1, 30.0); }
        QVERIFY2(std::abs(item->displayedElevation() - 30.0f) < 0.01f,
                 qPrintable(QString::number(item->displayedElevation())));
        QCOMPARE(item->elevation(), 30.0f);
        // A source without the reading leaves the dial where it was.
        widget.updateMmioValue(item, 1, 70.0, QStringLiteral("External meter source is unavailable."));
        QCOMPARE(item->elevation(), 30.0f);
        // The azimuth stays on variable 1.
        QVERIFY(std::abs(item->displayedAzimuth()) < 0.01f);
    }

    void aPreviewCopyNeverTurnsTheRotor()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        MeterWidget previewWidget;
        auto* preview = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &previewWidget, ContentRenderMode::Preview));
        QVERIFY(preview);
        QVERIFY(!preview->controlEnabled());
        // Even with control allowed, a preview copy sends nothing.
        preview->setAllowControl(true);
        preview->setRotor(&rotor, &commands);
        QVERIFY(!preview->rotorControllable());
        const QPointF az = preview->azimuthCentre(kW, kH);
        const float r = preview->pointerRadius(kW, kH);
        QVERIFY(!press(*preview, compassPoint(az, 90.0, r * 0.8)));
        QVERIFY(!release(*preview, compassPoint(az, 90.0, r * 0.8)));
        QVERIFY(!press(*preview, az));
        QVERIFY(commands.targets.isEmpty());
        QCOMPARE(commands.stops, 0);

        // The live copy of the same entry turns it.
        MeterWidget liveWidget;
        auto* live = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &liveWidget, ContentRenderMode::Live));
        QVERIFY(live);
        // A new item has control off; the live copy sends once it is allowed.
        live->setRotor(&rotor, &commands);
        QVERIFY(!live->rotorControllable());
        QVERIFY(!press(*live, compassPoint(az, 90.5, r * 0.8)));
        QVERIFY(commands.targets.isEmpty());
        live->setAllowControl(true);
        QVERIFY(live->rotorControllable());
        QVERIFY(press(*live, compassPoint(az, 90.5, r * 0.8)));
        QVERIFY(release(*live, compassPoint(az, 90.5, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);
        // Turning control off mid-way cancels and sends nothing more.
        QVERIFY(press(*live, compassPoint(az, 200.5, r * 0.8)));
        live->setControlEnabled(false);
        QVERIFY(!release(*live, compassPoint(az, 200.5, r * 0.8)));
        QCOMPARE(commands.targets.size(), 1);
        QCOMPARE(commands.stops, 0);
    }
};

QTEST_MAIN(TestRotatorItemDrag)
#include "tst_rotator_item_drag.moc"
