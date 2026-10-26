// no-port-check: NereusSDR-original tests for the filter display's edge
// drags, notches, TNF buttons and zoom; Thetis cites name the behaviour checked.
// Modification history (NereusSDR):
//   2026-10-26: first version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>
#include <QAction>
#include <QImage>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QWheelEvent>
#include <QWidget>
#include <cmath>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/FilterDisplayItem.h"
#include "models/NotchModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 800;
constexpr int kH = 300;
constexpr int kSlice = 2;
constexpr double kVfoHz = 14'200'000.0;

FilterDisplayItem::SliceFilterState stateFor(DSPMode mode, int low, int high)
{
    FilterDisplayItem::SliceFilterState s;
    s.sliceIndex = kSlice;
    s.mode = mode;
    s.vfoHz = kVfoHz;
    s.rxLowHz = low;
    s.rxHighHz = high;
    s.txLowHz = 100;
    s.txHighHz = 2900;
    s.filterName = QStringLiteral("2.7k");
    return s;
}

std::unique_ptr<FilterDisplayItem> makeItem(DSPMode mode, int low, int high)
{
    auto item = std::make_unique<FilterDisplayItem>();
    item->applyNewItemDefaults();
    item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    item->setRequestsEnabled(true);
    item->setSliceFilterState(stateFor(mode, low, high));
    return item;
}

void mouse(FilterDisplayItem& item, QEvent::Type type, const QPointF& pos,
           Qt::MouseButton button, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease
        ? Qt::MouseButtons(Qt::NoButton)
        : (type == QEvent::MouseMove ? Qt::MouseButtons(button) : Qt::MouseButtons(button));
    QMouseEvent event(type, pos, pos, type == QEvent::MouseMove ? Qt::NoButton : button,
                      buttons, mods);
    if (type == QEvent::MouseButtonPress) {
        item.handleMousePress(&event, kW, kH);
    } else if (type == QEvent::MouseButtonRelease) {
        item.handleMouseRelease(&event, kW, kH);
    } else {
        item.handleMouseMove(&event, kW, kH);
    }
}

void click(FilterDisplayItem& item, const QPointF& pos, Qt::MouseButton button,
           Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    mouse(item, QEvent::MouseMove, pos, Qt::NoButton, mods);
    mouse(item, QEvent::MouseButtonPress, pos, button, mods);
    mouse(item, QEvent::MouseButtonRelease, pos, button, mods);
}

void drag(FilterDisplayItem& item, const QPointF& from, const QPointF& to,
          Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    mouse(item, QEvent::MouseMove, from, Qt::NoButton, mods);
    mouse(item, QEvent::MouseButtonPress, from, Qt::LeftButton, mods);
    mouse(item, QEvent::MouseMove, to, Qt::LeftButton, mods);
    mouse(item, QEvent::MouseButtonRelease, to, Qt::LeftButton, mods);
}

QPointF midpoint(const QPointF& a, const QPointF& b)
{
    return (a + b) / 2.0;
}

float edgeX(const FilterDisplayItem& item, const FilterDisplayItem::ThetisLayout& l, double hz)
{
    return l.centre + item.hzToPixels(std::abs(hz), l.pixelSpan, l.hzSpan) * (hz < 0 ? -1.0f : 1.0f);
}

void presentFlatFrame(FilterDisplayItem& item)
{
    const QVector<float> trace(FilterDisplayItem::kSpectrumPixels, -100.0f);
    item.presentFrame(trace, trace, kVfoHz, 20000.0, false, false);
}

QImage render(FilterDisplayItem& item)
{
    QImage image(kW, kH, QImage::Format_ARGB32);
    image.fill(Qt::magenta);
    QPainter p(&image);
    item.paint(p, kW, kH);
    p.end();
    return image;
}

bool imageContains(const QImage& image, const QColor& colour)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).rgb() == colour.rgb()) { return true; }
        }
    }
    return false;
}

} // namespace

class TestFilterDisplayItemInput : public QObject {
    Q_OBJECT

private slots:
    // Acceptance: dragging the USB high edge +100 Hz asks for only the high
    // edge to change. From Thetis MeterManager.cs:36250-36256, 19279-19350.
    void usbHighEdgeDragChangesOnlyHigh()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        const auto l = item->thetisLayout(kW, kH);
        QVERIFY(l.valid);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        const QPointF grab = midpoint(l.highTop, l.highBot);
        const QPointF to(edgeX(*item, l, 3000.0), grab.y());
        drag(*item, grab, to);
        QVERIFY(!spy.isEmpty());
        const QList<QVariant> last = spy.takeLast();
        QCOMPARE(last.at(0).toInt(), kSlice);
        QCOMPARE(last.at(1).toInt(), 100);
        QVERIFY2(std::abs(last.at(2).toInt() - 3000) <= 1, qPrintable(last.at(2).toString()));
        QCOMPARE(last.at(3).toBool(), false);

        // Ctrl rounds to 10 Hz (Thetis Common.CtrlKeyDown): exactly 3000.
        item->setSliceFilterState(stateFor(DSPMode::USB, 100, 2900));
        spy.clear();
        drag(*item, grab, to, Qt::ControlModifier);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(1).toInt(), 100);
        QCOMPARE(spy.last().at(2).toInt(), 3000);
    }

    // Acceptance: in AM the low edge mirrors unless Shift is held.
    // From Thetis MeterManager.cs:19219-19235 (mirror = two sidebands &&
    // !Common.ShiftKeyDown).
    void amLowEdgeMirrorsUnlessShift()
    {
        auto item = makeItem(DSPMode::AM, -4000, 4000);
        const auto l = item->thetisLayout(kW, kH);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        const QPointF grab = midpoint(l.lowTop, l.lowBot);
        const QPointF to(edgeX(*item, l, -3000.0), grab.y());
        drag(*item, grab, to, Qt::ControlModifier);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(1).toInt(), -3000);
        QCOMPARE(spy.last().at(2).toInt(), 3000);
        QCOMPARE(spy.last().at(3).toBool(), false);

        spy.clear();
        drag(*item, grab, to, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(1).toInt(), -3000);
        QCOMPARE(spy.last().at(2).toInt(), 4000);
    }

    // Acceptance: while transmitting the request has tx = true, and the
    // routing MainWindow uses sets TransmitModel, not the slice.
    void transmittingDragAsksForTxFilter()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        item->setTransmitting(true);
        const auto l = item->thetisLayout(kW, kH);
        QCOMPARE(l.lowHz, 100.0f);
        QCOMPARE(l.highHz, 2900.0f);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        const QPointF grab = midpoint(l.highTop, l.highBot);
        drag(*item, grab, QPointF(edgeX(*item, l, 3000.0), grab.y()), Qt::ControlModifier);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(0).toInt(), kSlice);
        QCOMPARE(spy.last().at(1).toInt(), 100);
        QCOMPARE(spy.last().at(2).toInt(), 3000);
        QCOMPARE(spy.last().at(3).toBool(), true);

        SliceModel slice(kSlice);
        slice.setFilter(200, 2600);
        const int sliceLow = slice.filterLow();
        const int sliceHigh = slice.filterHigh();
        TransmitModel tx;
        tx.setFilterLow(100);
        tx.setFilterHigh(2900);
        FilterDisplayRequests::applyEdges(&slice, &tx, 100, 3000, true);
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 3000);
        QCOMPARE(slice.filterLow(), sliceLow);
        QCOMPARE(slice.filterHigh(), sliceHigh);

        // An RX request goes to the slice and leaves the TX filter alone.
        const auto expected = slice.filterForRequest(300, 2500);
        FilterDisplayRequests::applyEdges(&slice, &tx, 300, 2500, false);
        QCOMPARE(slice.filterLow(), expected.first);
        QCOMPARE(slice.filterHigh(), expected.second);
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 3000);
    }

    // LSB while transmitting: the passband is drawn at -TXFilterHigh..
    // -TXFilterLow, and the low edge writes TXFilterHigh (Thetis Low setter,
    // MeterManager.cs:19188-19208).
    void transmittingLsbLowEdgeWritesTxHigh()
    {
        auto item = makeItem(DSPMode::LSB, -2900, -100);
        item->setTransmitting(true);
        const auto l = item->thetisLayout(kW, kH);
        QCOMPARE(l.lowHz, -2900.0f);
        QCOMPARE(l.highHz, -100.0f);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        const QPointF grab = midpoint(l.lowTop, l.lowBot);
        drag(*item, grab, QPointF(edgeX(*item, l, -3000.0), grab.y()), Qt::ControlModifier);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(1).toInt(), 100);
        QCOMPARE(spy.last().at(2).toInt(), 3000);
        QCOMPARE(spy.last().at(3).toBool(), true);
    }

    // Acceptance: Ctrl+right adds a notch only when not transmitting.
    // From Thetis MeterManager.cs:18946-18964.
    void ctrlRightAddsNotchOnlyWhenReceiving()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        const auto l = item->thetisLayout(kW, kH);
        const QPointF at(edgeX(*item, l, -5000.0), l.y + l.h / 2.0f);
        QSignalSpy added(item.get(), &FilterDisplayItem::notchAddRequested);
        click(*item, at, Qt::RightButton, Qt::ControlModifier);
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.first().at(0).toInt(), kSlice);
        QVERIFY(std::abs(added.first().at(1).toDouble() - (kVfoHz - 5000.0)) <= 2.0);

        added.clear();
        item->setTransmitting(true);
        click(*item, at, Qt::RightButton, Qt::ControlModifier);
        QCOMPARE(added.count(), 0);
    }

    // Acceptance: +MNF adds a TNF only when not transmitting.
    // From Thetis MeterManager.cs:18997-19003.
    void mnfPlusAddsNothingWhileTransmitting()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        const auto l = item->thetisLayout(kW, kH);
        QVERIFY(!l.mnfPlusRect.isEmpty());
        QSignalSpy added(item.get(), &FilterDisplayItem::tnfAddRequested);
        QSignalSpy edges(item.get(), &FilterDisplayItem::filterEdgesRequested);
        click(*item, l.mnfPlusRect.center(), Qt::LeftButton);
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.first().at(0).toInt(), kSlice);

        added.clear();
        item->setTransmitting(true);
        click(*item, item->thetisLayout(kW, kH).mnfPlusRect.center(), Qt::LeftButton);
        QCOMPARE(added.count(), 0);
        QCOMPARE(edges.count(), 0);
    }

    // Acceptance: MNF toggles the global TNF (Thetis console.TNFActive,
    // MeterManager.cs:18990-18996).
    void mnfTogglesGlobalTnf()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        const auto l = item->thetisLayout(kW, kH);
        QSignalSpy toggled(item.get(), &FilterDisplayItem::tnfToggleRequested);
        click(*item, l.mnfRect.center(), Qt::LeftButton);
        QCOMPARE(toggled.count(), 1);

        NotchModel notches;
        const bool before = notches.globalEnabled();
        FilterDisplayRequests::applyTnfToggle(&notches);
        QCOMPARE(notches.globalEnabled(), !before);
        FilterDisplayRequests::applyTnfToggle(&notches);
        QCOMPARE(notches.globalEnabled(), before);
    }

    // FIT toggles autozoom and SNAP the snap lines; SNAP does nothing while
    // autozoom is on (MeterManager.cs:19004-19011).
    void fitAndSnapToggle()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        auto l = item->thetisLayout(kW, kH);
        QCOMPARE(l.zoom, 1.0f);
        click(*item, l.snapRect.center(), Qt::LeftButton);
        QVERIFY(item->snapLines());
        click(*item, l.fitRect.center(), Qt::LeftButton);
        QVERIFY(item->autoZoom());
        l = item->thetisLayout(kW, kH);
        QVERIFY(l.zoom > 1.0f);
        click(*item, l.snapRect.center(), Qt::LeftButton);
        QVERIFY(item->snapLines());  // unchanged while autozoom is on
        click(*item, l.fitRect.center(), Qt::LeftButton);
        QVERIFY(!item->autoZoom());
    }

    // Right click on the top line puts the passband back at the default
    // low cut, keeping its width (MeterManager.cs:18871-18920).
    void rightClickTopLineResetsPosition()
    {
        auto item = makeItem(DSPMode::USB, 500, 3200);
        const auto l = item->thetisLayout(kW, kH);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        click(*item, midpoint(l.topLeft, l.topRight), Qt::RightButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toInt(), FilterDisplayItem::kDefaultLowCutHz);
        QCOMPARE(spy.first().at(2).toInt(), FilterDisplayItem::kDefaultLowCutHz + 2700);
    }

    // A click nearest the low edge's number steps it 10 Hz (Thetis
    // MeterManager.cs:18966-18988).
    void clickNearLowNumberSteps()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        const auto l = item->thetisLayout(kW, kH);
        QSignalSpy spy(item.get(), &FilterDisplayItem::filterEdgesRequested);
        const QPointF at(l.filterL - 15.0f, l.textY - l.textHeight * 2.0f);
        click(*item, at, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toInt(), 90);
        QCOMPARE(spy.first().at(2).toInt(), 2900);
    }

    // Notches: drag moves the centre, the wheel steps the width 10 Hz, right
    // click asks for the menu, and the menu removes.
    void notchDragWheelAndMenu()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        auto state = stateFor(DSPMode::USB, 100, 2900);
        state.tnfActive = true;
        state.notches = {{7, kVfoHz + 1500.0, 200.0, true}};
        item->setSliceFilterState(state);
        const auto l = item->thetisLayout(kW, kH);
        const QPointF on(edgeX(*item, l, 1500.0), l.y + l.h / 2.0f);

        QSignalSpy changed(item.get(), &FilterDisplayItem::notchChangeRequested);
        drag(*item, on, QPointF(on.x() + item->hzToPixels(50.0, l.pixelSpan, l.hzSpan), on.y()),
             Qt::ControlModifier);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(changed.last().at(0).toInt(), 7);
        QCOMPARE(changed.last().at(1).toDouble(), kVfoHz + 1550.0);
        QCOMPARE(changed.last().at(2).toDouble(), 200.0);

        item->setSliceFilterState(state);
        changed.clear();
        mouse(*item, QEvent::MouseMove, on, Qt::NoButton);
        QWheelEvent wheel(on, on, QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        item->handleWheel(&wheel, kW, kH);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().at(1).toDouble(), kVfoHz + 1500.0);
        QCOMPARE(changed.first().at(2).toDouble(), 210.0);

        QSignalSpy menuAsked(item.get(), &FilterDisplayItem::notchMenuRequested);
        click(*item, on, Qt::RightButton);
        QCOMPARE(menuAsked.count(), 1);
        QCOMPARE(menuAsked.first().at(0).toInt(), 7);

        QMenu menu;
        item->buildNotchMenu(7, menu);
        QSignalSpy removed(item.get(), &FilterDisplayItem::notchRemoveRequested);
        QAction* remove = nullptr;
        for (QAction* a : menu.actions()) {
            if (a->text() == QStringLiteral("Remove Notch")) { remove = a; }
        }
        QVERIFY(remove);
        remove->trigger();
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.first().at(0).toInt(), 7);

        // Notches are not shown or grabbed while transmitting.
        item->setTransmitting(true);
        changed.clear();
        mouse(*item, QEvent::MouseMove, on, Qt::NoButton);
        item->handleWheel(&wheel, kW, kH);
        QCOMPARE(changed.count(), 0);
    }

    // A preview copy (the settings dialog and workspace preview path) is
    // never armed, so it asks for nothing, whatever input it gets.
    void previewCopyIsInert()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("FILTERDISPLAY"));
        QWidget host;
        std::unique_ptr<QWidget> preview(registry.createPreview(entry, &host));
        QVERIFY(preview);
        auto* item = preview->findChild<FilterDisplayItem*>();
        QVERIFY(item);
        QVERIFY(item->thetisLook());
        QVERIFY(!item->requestsEnabled());
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        auto state = stateFor(DSPMode::USB, 100, 2900);
        state.notches = {{7, kVfoHz + 1500.0, 200.0, true}};
        item->setSliceFilterState(state);

        QSignalSpy edges(item, &FilterDisplayItem::filterEdgesRequested);
        QSignalSpy shift(item, &FilterDisplayItem::filterShiftRequested);
        QSignalSpy notchAdd(item, &FilterDisplayItem::notchAddRequested);
        QSignalSpy notchChange(item, &FilterDisplayItem::notchChangeRequested);
        QSignalSpy notchMenu(item, &FilterDisplayItem::notchMenuRequested);
        QSignalSpy tnfToggle(item, &FilterDisplayItem::tnfToggleRequested);
        QSignalSpy tnfAdd(item, &FilterDisplayItem::tnfAddRequested);
        const auto l = item->thetisLayout(kW, kH);
        drag(*item, midpoint(l.highTop, l.highBot), QPointF(edgeX(*item, l, 3000.0), l.y + l.h / 2.0f));
        click(*item, l.mnfRect.center(), Qt::LeftButton);
        click(*item, l.mnfPlusRect.center(), Qt::LeftButton);
        click(*item, l.fitRect.center(), Qt::LeftButton);
        click(*item, QPointF(edgeX(*item, l, -5000.0), l.y + l.h / 2.0f), Qt::RightButton,
              Qt::ControlModifier);
        const QPointF notch(edgeX(*item, l, 1500.0), l.y + l.h / 2.0f);
        click(*item, notch, Qt::RightButton);
        QWheelEvent wheel(notch, notch, QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        item->handleWheel(&wheel, kW, kH);
        QCOMPARE(edges.count() + shift.count() + notchAdd.count() + notchChange.count()
                     + notchMenu.count() + tnfToggle.count() + tnfAdd.count(),
                 0);
        QVERIFY(!item->autoZoom());
    }

    // Old records (no Thetis-look field) paint as before: the legacy full
    // height RX edge line and no Thetis item background. A re-save keeps them
    // old. New items paint the Thetis look.
    void oldRecordPaintsAsBefore()
    {
        FilterDisplayItem source;
        const QStringList fields = source.serialize().split(QLatin1Char('|'));
        QVERIFY(fields.size() > 21);
        const QString oldRecord = fields.mid(0, 21).join(QLatin1Char('|'));

        FilterDisplayItem old;
        QVERIFY(old.deserialize(oldRecord));
        QVERIFY(!old.thetisLook());
        old.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        presentFlatFrame(old);
        old.setFilterEdgesRx(256, 768);
        old.setSliceFilterState(stateFor(DSPMode::USB, 100, 2900));
        const QImage legacy = render(old);
        const QColor yellow(0xff, 0xff, 0x00);
        const int x = 256 * kW / FilterDisplayItem::kSpectrumPixels;
        const auto yellowNear = [&](int y) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (legacy.pixelColor(x + dx, y).rgb() == yellow.rgb()) { return true; }
            }
            return false;
        };
        QVERIFY(yellowNear(2));
        QVERIFY(yellowNear(kH / 2));
        QVERIFY(yellowNear(kH - 3));
        QVERIFY(!imageContains(legacy, QColor(32, 32, 32)));

        // Re-saved, it stays old.
        QVERIFY(old.serialize().endsWith(QStringLiteral("|0")));
        FilterDisplayItem reloaded;
        QVERIFY(reloaded.deserialize(old.serialize()));
        QVERIFY(!reloaded.thetisLook());

        // A new item takes the Thetis look, and keeps it across a save.
        auto fresh = makeItem(DSPMode::USB, 100, 2900);
        presentFlatFrame(*fresh);
        fresh->setSliceFilterState(stateFor(DSPMode::USB, 100, 2900));
        QVERIFY(fresh->thetisLook());
        QVERIFY(imageContains(render(*fresh), QColor(32, 32, 32)));
        FilterDisplayItem saved;
        QVERIFY(saved.deserialize(fresh->serialize()));
        QVERIFY(saved.thetisLook());
    }

    // The buttons are drawn where they are hit: a setting that is on fills
    // its button with the setting-on colour inside the same rectangle the
    // click handling uses (MeterManager.cs:36195-36198).
    void buttonsDrawnWhereTheyAreHit()
    {
        auto item = makeItem(DSPMode::USB, 100, 2900);
        auto state = stateFor(DSPMode::USB, 100, 2900);
        state.tnfActive = true;
        item->setSliceFilterState(state);
        item->setSnapLines(true);
        presentFlatFrame(*item);
        item->setSliceFilterState(state);
        const auto l = item->thetisLayout(kW, kH);
        const QImage image = render(*item);
        const auto inside = [](const QRectF& r) { return QPoint(int(r.left()) + 2, int(r.top()) + 2); };
        QCOMPARE(image.pixelColor(inside(l.mnfRect)).rgb(), item->settingOnColour().rgb());
        QCOMPARE(image.pixelColor(inside(l.snapRect)).rgb(), item->settingOnColour().rgb());
        QCOMPARE(image.pixelColor(inside(l.fitRect)).rgb(), QColor(32, 32, 32).rgb());
        QVERIFY(l.mnfRect.right() <= l.centre);
        QVERIFY(l.mnfPlusRect.left() >= l.centre);
        QVERIFY(l.snapRect.right() <= l.mnfRect.left());
        QVERIFY(l.fitRect.left() >= l.mnfPlusRect.right());

        click(*item, l.fitRect.center(), Qt::LeftButton);
        const auto zoomed = item->thetisLayout(kW, kH);
        QCOMPARE(render(*item).pixelColor(inside(zoomed.fitRect)).rgb(), item->settingOnColour().rgb());
    }
};

QTEST_MAIN(TestFilterDisplayItemInput)
#include "tst_filter_display_item_input.moc"
