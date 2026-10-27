// no-port-check: NereusSDR-original tests for the dial item's tuning.
// Modification history (NereusSDR):
//   2026-10-26 - Drag and wheel tuning per degrees per change, the lock,
//                 acceleration and its undo on release, the thresholds kept
//                 90 apart, slice select and the corner buttons, preview
//                 copies, old and new records, and MainWindow's routing
//                 (align, step, select, lock, another radio's entry).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fix round 1: an old record keeps its look (top-left lit,
//                 no marker but while dragged, no centre letter) and still
//                 tunes by drag; a new dial draws Thetis's look.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Screenshot fix: on a 72 px stack row ACCEL and LOCK sit
//                 under the dial, a space apart; a large dial keeps them
//                 in its corners. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QWheelEvent>
#include <cmath>
#include <memory>
#include <numbers>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 200;
constexpr int kH = 200;
// The dial fills the item: centre (100, 100), the marker's ring radius
// 200 * 0.65 / 2 = 65.
constexpr double kCentre = 100.0;
constexpr double kRing = 65.0;

QPointF onRing(double degrees)
{
    const double rad = degrees * std::numbers::pi / 180.0;
    return QPointF(kCentre + std::cos(rad) * kRing, kCentre - std::sin(rad) * kRing);
}

// The corner buttons, away from the marker at 0 degrees and the centre.
const QPointF kTopLeft(40, 40);
const QPointF kTopRight(160, 40);
const QPointF kBottomLeft(40, 160);
const QPointF kBottomRight(160, 160);

DialSliceState sliceAt(int index, int stepHz, bool locked = false)
{
    DialSliceState state;
    state.index = index;
    state.stepHz = stepHz;
    state.locked = locked;
    return state;
}

void setUp(DialItem& dial, int stepHz, bool locked = false)
{
    dial.setRect(0, 0, 1, 1);
    dial.setSliceState(sliceAt(0, stepHz, locked));
    dial.setSourceSliceIndex(0);
    dial.setActiveSliceIndex(0);
}

bool press(DialItem& dial, QPointF pos)
{
    QMouseEvent e(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    return dial.handleMousePress(&e, kW, kH);
}

bool release(DialItem& dial, QPointF pos)
{
    QMouseEvent e(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    return dial.handleMouseRelease(&e, kW, kH);
}

bool move(DialItem& dial, QPointF pos)
{
    QMouseEvent e(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    return dial.handleMouseMove(&e, kW, kH);
}

bool click(DialItem& dial, QPointF pos)
{
    const bool taken = press(dial, pos);
    release(dial, pos);
    return taken;
}

bool wheel(DialItem& dial, int angle)
{
    const QPointF pos(kCentre, kCentre);
    QWheelEvent e(pos, pos, QPoint(), QPoint(0, angle), Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    return dial.handleWheel(&e, kW, kH);
}

QImage render(DialItem& dial)
{
    QImage image(kW, kH, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    dial.paint(painter, kW, kH);
    painter.end();
    return image;
}

qint64 sumOf(const QSignalSpy& spy)
{
    qint64 sum = 0;
    for (const QList<QVariant>& args : spy) { sum += args.at(1).toLongLong(); }
    return sum;
}

// Today's (17-part) record.
const QString kOldRecord = QStringLiteral(
    "DIAL|0.1|0.2|0.5|0.25|-1|3|#ffc8d8e8|#ff1a2a3a|#ff152030|#ff405060"
    "|#ff0070c0|#ff304050|#ff0090e0|#ff00ff88|#ffffb800|#ffff4444");

} // namespace

class TestDialItemTuning : public QObject {
    Q_OBJECT

private slots:
    // A 25 degree turn at 5 degrees per change with a 100 Hz step tunes
    // +500 Hz, one step per change, clockwise up.
    void dragTunesOneStepPerChange()
    {
        DialItem dial;
        setUp(dial, 100);
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        dial.setPressed(true);
        dial.setDegrees(10.0f, 0);  // the first update after a press only moves
        QCOMPARE(tune.count(), 0);
        for (int k = 1; k <= 25; ++k) {
            dial.setDegrees(10.0f - static_cast<float>(k), k);
        }
        QCOMPARE(tune.count(), 5);
        for (const QList<QVariant>& args : tune) {
            QCOMPARE(args.at(0).toInt(), 0);
            QCOMPARE(args.at(1).toLongLong(), 100LL);
        }
        QCOMPARE(sumOf(tune), 500LL);
        // Anticlockwise tunes down.
        for (int k = 1; k <= 5; ++k) {
            dial.setDegrees(-15.0f + static_cast<float>(k), 30 + k);
        }
        QCOMPARE(tune.count(), 6);
        QCOMPARE(tune.last().at(1).toLongLong(), -100LL);
    }

    // The mouse: a press on the marker holds it, and turning it clockwise
    // tunes up one step at a time.
    void mouseDragOnTheMarkerTunes()
    {
        DialItem dial;
        dial.setThetisLook(true);
        setUp(dial, 100);
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        QVERIFY(press(dial, onRing(0)));
        QVERIFY(dial.pressed());
        for (int deg = 2; deg <= 60; deg += 2) {
            QVERIFY(move(dial, onRing(-deg)));
        }
        QVERIFY(tune.count() >= 8);
        for (const QList<QVariant>& args : tune) { QCOMPARE(args.at(1).toLongLong(), 100LL); }
        // A held marker keeps the release even off the item.
        QVERIFY(dial.hitTest(QPointF(-50, -50), kW, kH));
        QVERIFY(release(dial, QPointF(-50, -50)));
        QVERIFY(!dial.pressed());
        // A press off the marker holds nothing.
        dial.setDegrees(0.0f, 1000);
        QVERIFY(press(dial, kTopLeft));
        QVERIFY(!dial.pressed());
        release(dial, QPointF(-50, -50));
    }

    void wheelTunesOneStepPerNotch()
    {
        DialItem dial;
        setUp(dial, 1000);
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        QVERIFY(wheel(dial, 120));
        QCOMPARE(tune.count(), 1);
        QCOMPARE(tune.at(0).at(1).toLongLong(), 1000LL);
        QVERIFY(wheel(dial, -120));
        QCOMPARE(tune.count(), 2);
        QCOMPARE(tune.at(1).at(1).toLongLong(), -1000LL);
    }

    // A locked slice gets no tune signal (if (Lock) return;).
    void lockedSliceGetsNoTune()
    {
        DialItem dial;
        setUp(dial, 100, true);
        QVERIFY(dial.lockEnabled());
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        dial.setPressed(true);
        for (int k = 0; k <= 30; ++k) { dial.setDegrees(-static_cast<float>(k), k); }
        dial.setPressed(false);
        wheel(dial, 120);
        QCOMPARE(tune.count(), 0);
        // Unlocked, the same slice tunes again.
        dial.setSliceState(sliceAt(0, 100, false));
        QVERIFY(!dial.lockEnabled());
        dial.setPressed(true);
        dial.setDegrees(1.0f, 100);
        for (int k = 1; k <= 5; ++k) { dial.setDegrees(1.0f - static_cast<float>(k), 100 + k); }
        QCOMPARE(tune.count(), 1);
    }

    // Turning faster than 540 degrees a second moves the step up one
    // entry; letting go restores the original step.
    void accelerationStepsUpAndReleaseRestores()
    {
        DialItem dial;
        setUp(dial, 100);
        dial.setAccelEnabled(true);
        QSignalSpy step(&dial, &DialItem::stepChangeRequested);
        dial.setPressed(true);
        qint64 t = 0;
        float degrees = 10.0f;
        dial.setDegrees(degrees, t);
        // 15 degrees every 20 ms: 750 degrees a second.
        for (int i = 0; i < 400 && step.isEmpty(); ++i) {
            t += 20;
            degrees -= 15.0f;
            dial.updateSpeed(t);
            dial.setDegrees(degrees, t);
        }
        QCOMPARE(step.count(), 1);
        QVERIFY(dial.smoothedSpeed() >= 540.0f);
        QCOMPARE(step.at(0).at(0).toInt(), 0);
        QCOMPARE(step.at(0).at(1).toInt(), 250);  // kTuneStepList[6]
        QCOMPARE(dial.sliceState(0).stepHz, 250);
        QCOMPARE(dial.tuneStepDirection(), 1);
        QCOMPARE(dial.speedState(), DialItem::SpeedState::Fast);
        // Inside the interval no second change.
        t += 20;
        degrees -= 15.0f;
        dial.updateSpeed(t);
        dial.setDegrees(degrees, t);
        QCOMPARE(step.count(), 1);
        dial.setPressed(false);
        QCOMPARE(step.count(), 2);
        QCOMPARE(step.at(1).at(1).toInt(), 100);
        QCOMPARE(dial.sliceState(0).stepHz, 100);
    }

    // The step index wraps round the list (ChangeTuneStepUp).
    void accelerationWrapsTheStepList()
    {
        DialItem dial;
        setUp(dial, kTuneStepList[kTuneStepListSize - 1].stepHz);
        dial.setAccelEnabled(true);
        QSignalSpy step(&dial, &DialItem::stepChangeRequested);
        dial.setPressed(true);
        qint64 t = 0;
        float degrees = 10.0f;
        dial.setDegrees(degrees, t);
        for (int i = 0; i < 400 && step.isEmpty(); ++i) {
            t += 20;
            degrees -= 15.0f;
            dial.updateSpeed(t);
            dial.setDegrees(degrees, t);
        }
        QCOMPARE(step.count(), 1);
        QCOMPARE(step.at(0).at(1).toInt(), kTuneStepList[0].stepHz);
    }

    // Up and down stay 90 apart.
    void thresholdsStayNinetyApart()
    {
        DialItem dial;
        QCOMPARE(dial.upThreshold(), 540);
        QCOMPARE(dial.downThreshold(), 360);
        dial.setUpThreshold(400);  // under down + 90
        QCOMPARE(dial.upThreshold(), 450);
        dial.setDownThreshold(300);
        dial.setUpThreshold(400);
        QCOMPARE(dial.upThreshold(), 400);
        dial.setDownThreshold(360);  // over up - 90
        QCOMPARE(dial.downThreshold(), 310);
        dial.setDegreesPerChange(0);
        QCOMPARE(dial.degreesPerChange(), 1);
        dial.setMaxTuneStepChanges(99);
        QCOMPARE(dial.maxTuneStepChanges(), 30);
        dial.setTuneStepIntervalSeconds(0);
        QCOMPARE(dial.tuneStepIntervalSeconds(), 1);
        dial.setVerticalRatio(2.0f);
        QCOMPARE(dial.verticalRatio(), 1.0f);
        dial.setFontScale(2.0f);
        QCOMPARE(dial.fontScale(), 1.1f);
    }

    void alignRoundsToTheStep()
    {
        QCOMPARE(DialItem::tunedFrequencyHz(7'074'030, 100, true), 7'074'100LL);
        QCOMPARE(DialItem::tunedFrequencyHz(7'074'030, 100, false), 7'074'130LL);
        QCOMPARE(DialItem::tunedFrequencyHz(7'074'030, -100, true), 7'073'900LL);
        // Math.Round: a half goes to the even step.
        QCOMPARE(DialItem::tunedFrequencyHz(7'074'050, 100, true), 7'074'200LL);
        QCOMPARE(DialItem::tunedFrequencyHz(7'074'150, 100, true), 7'074'200LL);
    }

    // Source slice or Any slice; the top buttons step to the slice before
    // or after when selecting moves the dial.
    void sliceSelectAndCornerButtons()
    {
        DialItem dial;
        dial.setRect(0, 0, 1, 1);
        for (int i = 0; i < 3; ++i) { dial.setSliceState(sliceAt(i, 100, i == 2)); }
        dial.setSourceSliceIndex(0);
        dial.setActiveSliceIndex(1);
        QSignalSpy select(&dial, &DialItem::sliceSelectRequested);
        QSignalSpy lock(&dial, &DialItem::lockToggleRequested);
        QCOMPARE(dial.tunedSlice(), 0);
        QVERIFY(!dial.sliceStepEnabled());
        QVERIFY(!dial.reasonAt(kTopRight, kW, kH).isEmpty());
        click(dial, kTopRight);
        QCOMPARE(select.count(), 0);

        dial.setSliceSelect(DialItem::SliceSelect::AnySlice);
        QCOMPARE(dial.tunedSlice(), 1);
        QCOMPARE(dial.previousSlice(), 0);
        QCOMPARE(dial.nextSlice(), 2);
        QVERIFY(dial.reasonAt(kTopRight, kW, kH).isEmpty());
        click(dial, kTopRight);
        click(dial, kTopLeft);
        QCOMPARE(select.count(), 2);
        QCOMPARE(select.at(0).at(0).toInt(), 2);
        QCOMPARE(select.at(1).at(0).toInt(), 0);
        click(dial, kBottomRight);
        QCOMPARE(lock.count(), 1);
        QCOMPARE(lock.at(0).at(0).toInt(), 1);
        QVERIFY(!dial.accelEnabled());
        click(dial, kBottomLeft);
        QVERIFY(dial.accelEnabled());

        // Slice C is locked: the dial on C locks with it.
        dial.setActiveSliceIndex(2);
        QVERIFY(dial.lockEnabled());
        QCOMPARE(dial.nextSlice(), -1);
        click(dial, kTopRight);
        QCOMPARE(select.count(), 2);

        // A Source that follows the selected slice steps too.
        dial.setSliceSelect(DialItem::SliceSelect::SourceSlice);
        dial.setSourceFollowsSelection(true);
        QVERIFY(dial.sliceStepEnabled());
        click(dial, kTopRight);
        QCOMPARE(select.count(), 3);
        QCOMPARE(select.at(2).at(0).toInt(), 1);

        // A slice that is not open tunes nothing.
        dial.setSourceSliceIndex(3);
        QCOMPARE(dial.tunedSlice(), -1);
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        wheel(dial, 120);
        QCOMPARE(tune.count(), 0);
    }

    // A preview copy (signals blocked) takes no input and acts on nothing.
    void previewCopyNeverActs()
    {
        DialItem dial;
        setUp(dial, 100);
        dial.blockSignals(true);
        QVERIFY(!press(dial, onRing(0)));
        QVERIFY(!dial.pressed());
        QVERIFY(!wheel(dial, 120));
        QVERIFY(!click(dial, kBottomLeft));
        QVERIFY(!dial.accelEnabled());
    }

    // Today's 17-part record loads unchanged and keeps the constructor's
    // tuning; the saved record appends the new fields and round-trips.
    void oldRecordRoundTrips()
    {
        DialItem dial;
        QVERIFY(dial.deserialize(kOldRecord));
        QCOMPARE(dial.textColour(), QColor(QStringLiteral("#ffc8d8e8")));
        QCOMPARE(dial.fastColour(), QColor(QStringLiteral("#ffff4444")));
        QCOMPARE(dial.padPressedColour(), QColor(255, 165, 0));
        QCOMPARE(dial.maxTuneStepChanges(), 3);
        QVERIFY(dial.alignWithTuneStep());
        QCOMPARE(dial.upThreshold(), 540);
        QCOMPARE(dial.downThreshold(), 360);
        QCOMPARE(dial.tuneStepIntervalSeconds(), 2);
        QCOMPARE(dial.degreesPerChange(), 5);
        QCOMPARE(dial.verticalRatio(), 1.0f);
        QCOMPARE(dial.fontScale(), 1.0f);
        QVERIFY(!dial.accelEnabled());
        QCOMPARE(dial.sliceSelect(), DialItem::SliceSelect::SourceSlice);

        const QString saved = dial.serialize();
        const QStringList parts = saved.split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 29);
        QCOMPARE(parts.last(), QStringLiteral("0"));  // the saved look
        QCOMPARE(QStringList(parts.mid(0, 17)).join(QLatin1Char('|')), kOldRecord);
        DialItem again;
        QVERIFY(again.deserialize(saved));
        QCOMPARE(again.serialize(), saved);

        // Every appended field survives.
        DialItem edited;
        QVERIFY(edited.deserialize(kOldRecord));
        edited.setPadPressedColour(QColor(1, 2, 3));
        edited.setVerticalRatio(0.5f);
        edited.setFontScale(0.75f);
        edited.setDownThreshold(200);
        edited.setUpThreshold(300);
        edited.setTuneStepIntervalSeconds(4);
        edited.setMaxTuneStepChanges(7);
        edited.setDegreesPerChange(12);
        edited.setAlignWithTuneStep(false);
        edited.setAccelEnabled(true);
        edited.setSliceSelect(DialItem::SliceSelect::AnySlice);
        DialItem loaded;
        QVERIFY(loaded.deserialize(edited.serialize()));
        QCOMPARE(loaded.serialize(), edited.serialize());
        QCOMPARE(loaded.upThreshold(), 300);
        QCOMPARE(loaded.downThreshold(), 200);
        QCOMPARE(loaded.sliceSelect(), DialItem::SliceSelect::AnySlice);
        QVERIFY(loaded.accelEnabled());
        QVERIFY(!loaded.alignWithTuneStep());

        QVERIFY(!dial.deserialize(QStringLiteral("DIAL|0|0|1|1|-1|0")));
    }

    // A new dial takes Thetis's table.
    void newDialGetsTheThetisTable()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("DIAL"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* dial = qobject_cast<DialItem*>(item.get());
        QVERIFY(dial != nullptr);
        QCOMPARE(dial->textColour(), QColor(Qt::white));
        QCOMPARE(dial->circleColour(), QColor(Qt::black));
        QCOMPARE(dial->padColour(), QColor(0, 0, 255));
        QCOMPARE(dial->padPressedColour(), QColor(255, 165, 0));
        QCOMPARE(dial->buttonOnColour(), QColor(100, 149, 237));
        QCOMPARE(dial->buttonOffColour(), QColor(Qt::black));
        QCOMPARE(dial->buttonHighlightColour(), QColor(128, 128, 128));
        QCOMPARE(dial->ringColour(), QColor(128, 128, 128));
        QCOMPARE(dial->slowColour(), QColor(0, 0, 255));
        QCOMPARE(dial->holdColour(), QColor(0, 128, 0));
        QCOMPARE(dial->fastColour(), QColor(255, 0, 0));
        QCOMPARE(dial->maxTuneStepChanges(), 3);
        QVERIFY(dial->alignWithTuneStep());
        QCOMPARE(dial->upThreshold(), 540);
        QCOMPARE(dial->downThreshold(), 360);
        QCOMPARE(dial->tuneStepIntervalSeconds(), 2);
        QCOMPARE(dial->degreesPerChange(), 5);
        QCOMPARE(dial->verticalRatio(), 1.0f);
        QCOMPARE(dial->fontScale(), 1.0f);
        QVERIFY(!dial->accelEnabled());
        QVERIFY(dial->thetisLook());
        QVERIFY(dial->drawsMarker());
        // Thetis's marker (pad colour) sits on the ring at 0 degrees.
        dial->setRect(0, 0, 1, 1);
        QCOMPARE(render(*dial).pixelColor(165, 112), QColor(0, 0, 255));
        DialItem again;
        QVERIFY(again.deserialize(dial->serialize()));
        QVERIFY(again.thetisLook());
    }

    // Ruling D5: today's record paints as it did before the tuning port:
    // the top-left quadrant lit, no marker, no centre letter. It still
    // tunes by drag (the marker shows only while dragged) and wheel.
    void oldRecordPaintsAsBefore()
    {
        DialItem dial;
        QVERIFY(dial.deserialize(kOldRecord));
        dial.setRect(0, 0, 1, 1);
        QVERIFY(!dial.thetisLook());
        QCOMPARE(dial.activeQuadrant(), 0);
        QVERIFY(!dial.drawsMarker());
        const QColor circle(QStringLiteral("#ff1a2a3a"));
        const QImage unfed = render(dial);
        // Top-right is the bare circle; top-left carries the lit tint.
        QCOMPARE(unfed.pixelColor(130, 75), circle);
        QVERIFY(unfed.pixelColor(70, 75) != circle);
        // Where a marker at 0 degrees would be: the bare circle.
        QCOMPARE(unfed.pixelColor(165, 112), circle);

        // Fed with slices, the centre is unchanged (no centre letter).
        for (int i = 0; i < 3; ++i) { dial.setSliceState(sliceAt(i, 100)); }
        dial.setSourceSliceIndex(0);
        dial.setActiveSliceIndex(0);
        const QImage fed = render(dial);
        QCOMPARE(fed.copy(70, 70, 60, 60), unfed.copy(70, 70, 60, 60));
        QCOMPARE(fed.pixelColor(165, 112), circle);

        // Round trip keeps the look and the picture.
        DialItem again;
        QVERIFY(again.deserialize(dial.serialize()));
        again.setRect(0, 0, 1, 1);
        QVERIFY(!again.thetisLook());
        QCOMPARE(again.activeQuadrant(), 0);
        QCOMPARE(render(again), unfed);

        // A press and a move drag it; the marker shows only meanwhile.
        QSignalSpy tune(&dial, &DialItem::tuneRequested);
        QVERIFY(press(dial, onRing(90)));
        QVERIFY(!dial.pressed());
        for (int k = 1; k <= 30; ++k) { move(dial, onRing(90 - 2 * k)); }
        QVERIFY(dial.pressed());
        QVERIFY(dial.drawsMarker());
        QVERIFY(tune.count() >= 8);
        for (const QList<QVariant>& args : tune) { QCOMPARE(args.at(1).toLongLong(), 100LL); }
        QVERIFY(release(dial, onRing(30)));
        QVERIFY(!dial.drawsMarker());
        QCOMPARE(render(dial).pixelColor(165, 112), circle);
        // A press and release without moving is still the button.
        QVERIFY(!dial.accelEnabled());
        click(dial, kBottomLeft);
        QVERIFY(dial.accelEnabled());
    }

    // A dial on a 72 px stack row (the scene's 408 x 72) is too small for
    // ACCEL and LOCK in its corners at the readable text size: they sit
    // under it, a space apart, clear of the dial and of each other. A
    // 200 px dial keeps them in its corners and its whole square.
    void captionsSitClearOfASmallDial()
    {
        const bool wasEnabled = WorkspacePreview::enabled();
        WorkspacePreview::setEnabledForTest(true);
        const auto restore = qScopeGuard([wasEnabled] { WorkspacePreview::setEnabledForTest(wasEnabled); });
        for (const bool thetis : {true, false}) {
            DialItem dial;
            if (thetis) {
                dial.applyNewItemDefaults();
            }
            dial.setRect(0, 0, 1, 1);
            QCOMPARE(dial.thetisLook(), thetis);
            for (const QSize size : {QSize(408, 72), QSize(260, 72), QSize(620, 72)}) {
                const DialItem::CaptionLayout small = dial.captionLayout(size.width(), size.height());
                const QString where = QStringLiteral("%1 look at %2x%3")
                    .arg(thetis ? QStringLiteral("Thetis") : QStringLiteral("saved"))
                    .arg(size.width()).arg(size.height());
                QVERIFY2(small.below, qPrintable(where));
                QVERIFY2(!small.accel.intersects(small.lock), qPrintable(where));
                QVERIFY2(small.lock.left() - small.accel.right() > 0.0, qPrintable(where));
                QVERIFY2(small.accel.top() > small.dial.bottom(), qPrintable(where));
                QVERIFY2(small.lock.top() > small.dial.bottom(), qPrintable(where));
                QVERIFY2(small.accel.bottom() <= size.height() && small.lock.bottom() <= size.height(),
                         qPrintable(where));
                QVERIFY2(small.dial.height() > 0 && small.dial.width() == small.dial.height(), qPrintable(where));
                QVERIFY2(small.font.pixelSize() >= 11, qPrintable(where));
                // The words are drawn at the dial's centre line.
                const double centre = small.dial.left() + small.dial.width() / 2.0;
                QVERIFY2(small.accel.right() <= centre && small.lock.left() >= centre, qPrintable(where));
            }
            const DialItem::CaptionLayout large = dial.captionLayout(kW, kH);
            QVERIFY(!large.below);
            QCOMPARE(large.dial, QRect(0, 0, kW, kH));
            QVERIFY(!large.accel.intersects(large.lock));
        }
    }

    // MainWindow: each request acts on the slice it names; Align rounds;
    // a locked slice and another radio's entry change nothing; nothing keys.
    void mainWindowRoutesDialRequests()
    {
        AppSettings::setProfileOverride(QStringLiteral("dial-tuning-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes(5));
        const auto cleanup = qScopeGuard([] {
            QFile::remove(AppSettings::instance().filePath());
            RadioDiscovery::clearHoldOffForTest();
        });
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->configureStreamPool(5, 5, 192000);
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* a = model->sliceById(0);
        SliceModel* b = model->sliceById(1);
        QVERIFY(a && b);
        a->setFrequency(14'074'000.0);
        b->setFrequency(7'074'050.0);

        ContainerManager* cm = model->containerManager();
        QVERIFY(cm);
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* dial = new DialItem();
        dial->setRect(0, 0, 1, 1);
        meter->addItem(dial);
        QVERIFY(dial->property("dialRequestsWired").toBool());
        // The feed: every open slice, with its step.
        QCOMPARE(dial->sliceState(1).index, 1);
        QCOMPARE(dial->sliceState(1).stepHz, b->stepHz());

        QSignalSpy mox(&model->transmitModel(), &TransmitModel::moxChanged);
        QSignalSpy tune(&model->transmitModel(), &TransmitModel::tuneChanged);

        // Align on: a half step rounds to the even step.
        emit dial->tuneRequested(1, 100);
        QCOMPARE(b->frequency(), 7'074'200.0);
        QCOMPARE(a->frequency(), 14'074'000.0);
        dial->setAlignWithTuneStep(false);
        emit dial->tuneRequested(1, -100);
        QCOMPARE(b->frequency(), 7'074'100.0);

        // Steps on the list only.
        emit dial->stepChangeRequested(1, 250);
        QCOMPARE(b->stepHz(), 250);
        emit dial->stepChangeRequested(1, 123);
        QCOMPARE(b->stepHz(), 250);

        // Lock toggles; a locked slice does not tune.
        emit dial->lockToggleRequested(1);
        QVERIFY(b->locked());
        emit dial->tuneRequested(1, 100);
        QCOMPARE(b->frequency(), 7'074'100.0);
        emit dial->lockToggleRequested(1);
        QVERIFY(!b->locked());

        // Select: the slice becomes the active one.
        emit dial->sliceSelectRequested(1);
        QCOMPARE(model->activeSlice(), b);

        // A slice that is not open: nothing.
        emit dial->tuneRequested(3, 100);
        emit dial->sliceSelectRequested(3);
        QCOMPARE(model->activeSlice(), b);

        // Another radio's entry drives nothing in this window.
        dial->setProperty("containerSourceContext",
                          QJsonObject{{"sessionId", "another-radio"}, {"sliceId", 0}});
        const double before = a->frequency();
        const int stepBefore = a->stepHz();
        emit dial->tuneRequested(0, 1000);
        emit dial->lockToggleRequested(0);
        emit dial->stepChangeRequested(0, 1000);
        emit dial->sliceSelectRequested(0);
        QCOMPARE(a->frequency(), before);
        QVERIFY(!a->locked());
        QCOMPARE(a->stepHz(), stepBefore);
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tune.count(), 0);
    }
};

QTEST_MAIN(TestDialItemTuning)
#include "tst_dial_item_tuning.moc"
