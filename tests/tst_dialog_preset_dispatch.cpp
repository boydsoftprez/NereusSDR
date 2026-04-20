// =================================================================
// tests/tst_dialog_preset_dispatch.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 11 — preset dispatch verification.
//
// The old appendPresetRow() path expanded each PRESET_* tag into N
// discrete ItemGroup children (8 for ANAN MM, 3 for Cross-Needle,
// etc.). Task 11 swaps that for direct construction of the first-
// class preset classes; each PRESET_* tag now lands as exactly one
// row in the in-use list. These tests assert that the swap is in
// effect and that each composite tag resolves to the expected
// MeterItem subclass.
//
// no-port-check: test scaffolding — exercises NereusSDR-original
// dialog API only, no Thetis source text transcribed.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QSplitter>
#include <QWidget>

#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/SMeterPresetItem.h"
#include "gui/meters/presets/PowerSwrPresetItem.h"

using namespace NereusSDR;

class TstDialogPresetDispatch : public QObject
{
    Q_OBJECT

private slots:
    void addAnanMm_createsOneAnanMultiMeterItem();
    void addCrossNeedle_createsOneCrossNeedleItem();
    void addSMeter_createsOneSMeterPresetItem();
    void addPowerSwr_createsOnePowerSwrPresetItem();
    void addSignalBar_createsOneBarPresetItem();
    // Critical-fix coverage — every preset class must route to
    // PresetItemEditor rather than falling through to the empty page
    // when the in-use row is selected. The JSON-format serializer
    // lacks a '|' pipe so the legacy tag-dispatch path missed these.
    void selectAnanMmRow_propertyEditorNotEmpty();
    void selectSMeterRow_propertyEditorNotEmpty();
    void selectAlcGainRow_propertyEditorNotEmpty();
    void selectVfoDisplayRow_propertyEditorNotEmpty();

    // Follow-up-bug coverage — preset + primitive in the same container
    // must both render. Before the Background-layer fix, the preset
    // painted on top of the primitive (opaque OverlayStatic backdrop
    // over a Geometry/OverlayDynamic Bar) and the primitive disappeared.
    void presetCoexistsWithPrimitive_bothRender();
};

namespace {

struct Harness {
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr{&dockParent, &splitter};
    ContainerWidget* container{nullptr};
    MeterWidget* meter{nullptr};

    Harness()
    {
        container = mgr.createContainer(1, DockMode::Floating);
        Q_ASSERT(container);
        meter = new MeterWidget();
        container->setContent(meter);
    }
};

} // namespace

void TstDialogPresetDispatch::addAnanMm_createsOneAnanMultiMeterItem()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("AnanMM"));

    const QVector<MeterItem*> items = dlg.workingItems();
    QCOMPARE(items.size(), 1);
    QVERIFY2(dynamic_cast<AnanMultiMeterItem*>(items.first()) != nullptr,
             "PRESET_AnanMM must construct AnanMultiMeterItem, "
             "not flatten through the old ItemGroup path");
}

void TstDialogPresetDispatch::addCrossNeedle_createsOneCrossNeedleItem()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("CrossNeedle"));

    const QVector<MeterItem*> items = dlg.workingItems();
    QCOMPARE(items.size(), 1);
    QVERIFY(dynamic_cast<CrossNeedleItem*>(items.first()) != nullptr);
}

void TstDialogPresetDispatch::addSMeter_createsOneSMeterPresetItem()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("SMeter"));

    const QVector<MeterItem*> items = dlg.workingItems();
    QCOMPARE(items.size(), 1);
    QVERIFY(dynamic_cast<SMeterPresetItem*>(items.first()) != nullptr);
}

void TstDialogPresetDispatch::addPowerSwr_createsOnePowerSwrPresetItem()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("PowerSwr"));

    const QVector<MeterItem*> items = dlg.workingItems();
    QCOMPARE(items.size(), 1);
    QVERIFY(dynamic_cast<PowerSwrPresetItem*>(items.first()) != nullptr);
}

void TstDialogPresetDispatch::addSignalBar_createsOneBarPresetItem()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("SignalBar"));

    const QVector<MeterItem*> items = dlg.workingItems();
    QCOMPARE(items.size(), 1);
    auto* bar = dynamic_cast<BarPresetItem*>(items.first());
    QVERIFY2(bar != nullptr,
             "Bar-row preset must route to BarPresetItem");
    QCOMPARE(bar->presetKind(), BarPresetItem::Kind::SignalBar);
}

// ---------------------------------------------------------------------------
// Critical-fix coverage — property editor is NOT the empty page.
//
// Bug: preset classes serialize as JSON ("{"kind":...) with no '|' so
// the tag-dispatch path in buildTypeSpecificEditor() failed to match
// and returned nullptr, leaving the property pane on the empty page.
// Fix routes JSON-prefixed payloads to the shared PresetItemEditor.
// ---------------------------------------------------------------------------

void TstDialogPresetDispatch::selectAnanMmRow_propertyEditorNotEmpty()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("AnanMM"));
    dlg.selectInUseRowForTest(0);
    QVERIFY2(!dlg.propertyStackCurrentIsEmpty(),
             "AnanMultiMeterItem row must populate PresetItemEditor, "
             "not fall through to the empty page");
}

void TstDialogPresetDispatch::selectSMeterRow_propertyEditorNotEmpty()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("SMeter"));
    dlg.selectInUseRowForTest(0);
    QVERIFY(!dlg.propertyStackCurrentIsEmpty());
}

void TstDialogPresetDispatch::selectAlcGainRow_propertyEditorNotEmpty()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("AlcGain"));
    dlg.selectInUseRowForTest(0);
    QVERIFY2(!dlg.propertyStackCurrentIsEmpty(),
             "BarPresetItem (AlcGain flavour) must populate "
             "PresetItemEditor with kindString readout");
}

void TstDialogPresetDispatch::selectVfoDisplayRow_propertyEditorNotEmpty()
{
    // Generic preset path — no class-specific controls, just X/Y/W/H.
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    dlg.appendPresetRowForTest(QStringLiteral("VfoDisplayPreset"));
    dlg.selectInUseRowForTest(0);
    QVERIFY(!dlg.propertyStackCurrentIsEmpty());
}

// ---------------------------------------------------------------------------
// Follow-up bug 1 — preset + primitive coexistence.
//
// Root cause (pre-fix): preset classes rendered via Layer::OverlayStatic
// and primitives (BarItem) via Layer::OverlayDynamic. The preset's
// opaque backdrop painted over the primitive and the primitive
// disappeared. The fix moves every preset class to Layer::Background,
// so the render order is Background (presets) -> Geometry ->
// OverlayStatic -> OverlayDynamic (primitives), and primitives now
// layer on top of the preset.
//
// The check here is a smoke-level assertion: paint both items into
// an offscreen image in the documented render order and confirm the
// pixel buffer is non-empty (the pre-fix bug path left bars invisible,
// but this test targets the simpler invariant that the order doesn't
// erase either item's contribution). Pixel-perfect colour matching is
// out of scope.
// ---------------------------------------------------------------------------
void TstDialogPresetDispatch::presetCoexistsWithPrimitive_bothRender()
{
    // Construct a preset and a primitive that would share a container.
    SMeterPresetItem preset;
    preset.setRect(0.0f, 0.0f, 1.0f, 1.0f);

    BarItem bar;
    bar.setRect(0.1f, 0.3f, 0.8f, 0.2f);
    bar.setRange(0.0, 1.0);
    bar.setValue(0.5);

    // Mirror MeterWidget's CPU draw order (paintEvent -> drawItems):
    // items paint in list order. With the fix, preset renderLayer is
    // Background and bar renderLayer is Geometry (participating in
    // OverlayDynamic); both are single-layer callers, so a direct
    // paint() on each is sufficient to exercise the CPU fallback path
    // that tst_smoke.cpp and friends run under.
    const int W = 400;
    const int H = 200;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        preset.paint(p, W, H);
        bar.paint(p, W, H);
    }
    QVERIFY(!img.isNull());
    QCOMPARE(img.width(),  W);
    QCOMPARE(img.height(), H);

    // Smoke check: at least one non-black pixel somewhere in the
    // frame. Both the preset face and the bar fill contribute
    // colour, so an all-black result would mean the render order
    // erased every item's output.
    bool anyNonBlack = false;
    for (int y = 0; y < H && !anyNonBlack; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < W; ++x) {
            const QRgb px = row[x];
            if (qRed(px) != 0 || qGreen(px) != 0 || qBlue(px) != 0) {
                anyNonBlack = true;
                break;
            }
        }
    }
    QVERIFY2(anyNonBlack,
             "Preset + Bar render produced no visible pixels; the "
             "preset's Background layer + BarItem's OverlayDynamic "
             "layer should both contribute colour to the frame");

    // Layer-assignment regression guard: with the Background-layer
    // fix, the preset must no longer claim the OverlayStatic layer.
    // BarItem keeps its existing OverlayDynamic layer so primitives
    // paint on top of presets.
    QCOMPARE(preset.renderLayer(), MeterItem::Layer::Background);
    QVERIFY(bar.participatesIn(MeterItem::Layer::OverlayDynamic));
    QVERIFY(!preset.participatesIn(MeterItem::Layer::OverlayStatic));
}

QTEST_MAIN(TstDialogPresetDispatch)
#include "tst_dialog_preset_dispatch.moc"
