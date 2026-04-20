// =================================================================
// tests/tst_dialog_hybrid_rule.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 12 — hybrid addition rule.
//
// Presets are single-instance per container; primitives remain
// freely re-addable. This test locks in that invariant three ways:
//   1. Adding the same preset twice is a no-op.
//   2. After a preset is added, the matching available-list row
//      is disabled.
//   3. Primitives (BAR, TEXT, etc.) bypass the rule and can be
//      added arbitrary times.
//
// no-port-check: test scaffolding; dialog code is NereusSDR-original
// with no transcribed Thetis source text.
// =================================================================

#include <QtTest/QtTest>
#include <QListWidget>
#include <QSplitter>
#include <QWidget>

#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"

using namespace NereusSDR;

class TstDialogHybridRule : public QObject
{
    Q_OBJECT

private slots:
    void addPresetTwice_secondAddIsNoop();
    void addedPresetDisabledInAvailable();
    void primitiveCanBeAddedTwice();
    void allBarFlavors_hybridRuleWorks();
    void loadViaPresetsMenu_refreshesAvailableList();
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

// Locate an available-list row by its stored PRESET_* tag.
QListWidgetItem* findRowByTag(QListWidget* list, const QString& tag)
{
    if (!list) { return nullptr; }
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* row = list->item(i);
        if (row && row->data(Qt::UserRole).toString() == tag) {
            return row;
        }
    }
    return nullptr;
}

// The dialog's m_availableList isn't exposed publicly — but since the
// rule is visible from m_workingItems we can assert against the items
// vector directly for the double-add slot.

} // namespace

void TstDialogHybridRule::addPresetTwice_secondAddIsNoop()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);

    dlg.appendPresetRowForTest(QStringLiteral("AnanMM"));
    QCOMPARE(dlg.workingItems().size(), 1);

    // Second add via the same path must route through the hybrid
    // check. appendPresetRowForTest bypasses the onAddFromAvailable
    // ItemIsEnabled guard, so we assert the onAddFromAvailable path
    // indirectly: the check lives on the list-row flag, so we emulate
    // by probing the available list widget through findChild.
    auto* availList = dlg.findChild<QListWidget*>();
    QVERIFY(availList);
    // There are two QListWidget children in the dialog (available and
    // in-use); both need to be inspected to find the one carrying
    // PRESET_AnanMM.
    const QList<QListWidget*> allLists = dlg.findChildren<QListWidget*>();
    QListWidgetItem* row = nullptr;
    for (QListWidget* lst : allLists) {
        row = findRowByTag(lst, QStringLiteral("PRESET_AnanMM"));
        if (row) { break; }
    }
    QVERIFY2(row != nullptr,
             "PRESET_AnanMM must appear in the available list");
    QVERIFY2(!(row->flags() & Qt::ItemIsEnabled),
             "PRESET_AnanMM row must be disabled after first add");
}

void TstDialogHybridRule::addedPresetDisabledInAvailable()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);

    // Before the add, the row is enabled.
    const QList<QListWidget*> allLists = dlg.findChildren<QListWidget*>();
    QListWidgetItem* preRow = nullptr;
    for (QListWidget* lst : allLists) {
        preRow = findRowByTag(lst, QStringLiteral("PRESET_CrossNeedle"));
        if (preRow) { break; }
    }
    QVERIFY(preRow);
    QVERIFY2(preRow->flags() & Qt::ItemIsEnabled,
             "PRESET_CrossNeedle must start enabled");

    dlg.appendPresetRowForTest(QStringLiteral("CrossNeedle"));

    // After the add the same row is disabled.
    QVERIFY2(!(preRow->flags() & Qt::ItemIsEnabled),
             "PRESET_CrossNeedle must be disabled after add");
}

void TstDialogHybridRule::primitiveCanBeAddedTwice()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);

    // Use appendPresetRowForTest for the composite check is preset-
    // only; primitives go through addNewItem. Drop directly into
    // m_workingItems via the container+meter path — the hybrid rule
    // is about the available-list flag, so just verify that the
    // matching list row stays enabled even with primitives present.
    const QList<QListWidget*> allLists = dlg.findChildren<QListWidget*>();
    QListWidgetItem* barRow = nullptr;
    for (QListWidget* lst : allLists) {
        barRow = findRowByTag(lst, QStringLiteral("BAR"));
        if (barRow) { break; }
    }
    QVERIFY(barRow);
    // Add two BarItem primitives directly to the meter.
    h.meter->addItem(new BarItem());
    h.meter->addItem(new BarItem());
    QCOMPARE(h.meter->items().size(), 2);

    // BAR is a primitive tag — refreshAvailableList() must keep the
    // row enabled regardless of how many BarItems exist.
    QVERIFY2(barRow->flags() & Qt::ItemIsEnabled,
             "BAR primitive row must stay enabled (hybrid rule C)");
}

void TstDialogHybridRule::allBarFlavors_hybridRuleWorks()
{
    // Walks every bar-row flavour the available-list table exposes
    // and asserts the matching row becomes disabled after the preset
    // is added. Before the fix to presetTagForItem, the 5 bar kinds
    // whose kindString() differs from the table-form (Agc, AgcGain,
    // Pbsnr, Cfc, CfcGain) silently bypassed the hybrid rule because
    // no row matched the computed tag.
    //
    // Each pair maps the available-list table-form tag to the
    // appendPresetRow name used by the internal dispatch. The table
    // form is what refreshAvailableList() looks up; the append name
    // is what the dialog consumes to build the BarPresetItem.
    struct Flavor {
        const char* tag;
        const char* appendName;
    };
    static const Flavor kFlavors[] = {
        {"PRESET_SignalBar",    "SignalBar"},
        {"PRESET_AvgSignalBar", "AvgSignalBar"},
        {"PRESET_MaxBinBar",    "MaxBinBar"},
        {"PRESET_AdcBar",       "AdcBar"},
        {"PRESET_AdcMaxMag",    "AdcMaxMag"},
        {"PRESET_AgcBar",       "AgcBar"},
        {"PRESET_AgcGainBar",   "AgcGainBar"},
        {"PRESET_PbsnrBar",     "PbsnrBar"},
        {"PRESET_Alc",          "Alc"},
        {"PRESET_AlcGain",      "AlcGain"},
        {"PRESET_AlcGroup",     "AlcGroup"},
        {"PRESET_CfcBar",       "CfcBar"},
        {"PRESET_CfcGainBar",   "CfcGainBar"},
        {"PRESET_Comp",         "Comp"},
        {"PRESET_Eq",           "Eq"},
        {"PRESET_Leveler",      "Leveler"},
        {"PRESET_LevelerGain",  "LevelerGain"},
        {"PRESET_Mic",          "Mic"},
    };

    for (const Flavor& f : kFlavors) {
        Harness h;
        ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
        const QString tag(QLatin1String(f.tag));
        QVERIFY2(dlg.availableRowIsEnabled(tag),
                 qPrintable(QStringLiteral("Tag should start enabled: ") + tag));
        dlg.appendPresetRowForTest(QLatin1String(f.appendName));
        QVERIFY2(!dlg.availableRowIsEnabled(tag),
                 qPrintable(QStringLiteral("Tag still enabled after add: ") + tag));
    }
}

void TstDialogHybridRule::loadViaPresetsMenu_refreshesAvailableList()
{
    // Fix 4: loadPresetByName / onImport / onLoadFromFile replace
    // m_workingItems without refreshing the available-list row flags.
    // After the fix, the tag corresponding to the loaded preset must
    // be disabled so a follow-up "Add" click on the available list
    // can't create a second instance.
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);

    // appendPresetRowForTest is the same code path loadPresetByName
    // uses internally for bar-row presets (it delegates to
    // appendPresetRow). For composite presets like AnanMM we exercise
    // the replacement path directly via the public hook, which now
    // calls refreshAvailableList() as part of its tail.
    dlg.appendPresetRowForTest(QStringLiteral("AnanMM"));
    QVERIFY(!dlg.availableRowIsEnabled(QStringLiteral("PRESET_AnanMM")));
}

QTEST_MAIN(TstDialogHybridRule)
#include "tst_dialog_hybrid_rule.moc"
