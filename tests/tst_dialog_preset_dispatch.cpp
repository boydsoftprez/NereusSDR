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

QTEST_MAIN(TstDialogPresetDispatch)
#include "tst_dialog_preset_dispatch.moc"
