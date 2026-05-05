#pragma once

// =================================================================
// src/gui/SetupDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Qt6 navigation shell for the Settings dialog.
// Independently implemented from Thetis Setup Form interface design;
// no direct C# port. See SetupDialog.cpp for inline citations to
// Thetis behavior rules consulted during implementation.
// =================================================================

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSplitter>

namespace NereusSDR {

class RadioModel;
class SetupPage;
class PaGainByBandPage;
class PaWattMeterPage;
class PaValuesPage;
struct BoardCapabilities;
struct RadioInfo;

// Main settings dialog with tree-based navigation.
// Left pane: QTreeWidget with top-level category items.
// Right pane: QStackedWidget showing the selected page.
class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(RadioModel* model, QWidget* parent = nullptr);

    // Navigate to a page by its label text (e.g. "AGC/ALC").
    void selectPage(const QString& label);

signals:
    // Phase 3M-3a-ii Batch 6 (Task 3): forwarded from CfcSetupPage's
    // [Configure CFC bands…] button.  MainWindow connects this to the
    // TxApplet::requestOpenCfcDialog() slot so the modeless TxCfcDialog
    // instance is shared between the Setup page and the [CFC] right-click
    // on the TxApplet.
    void cfcDialogRequested();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    // Phase 8 of #167: drives per-SKU PA category + page visibility from
    // RadioModel::currentRadioChanged. Mirrors Thetis
    // comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
    // [v2.10.3.13+501e3f51]) per-SKU PA tab visibility.
    void onCurrentRadioChanged(const RadioInfo& info);

private:
    void buildTree();

    // Phase 8 of #167: applies BoardCapabilities to the PA category +
    // sub-page visibility. Hides the category root when caps.isRxOnlySku
    // or !caps.hasPaProfile; forwards the caps struct to each PA page so
    // page-level controls can self-toggle (warning rows, banner labels).
    void applyPaVisibility(const BoardCapabilities& caps);

    RadioModel*      m_model   = nullptr;
    QTreeWidget*     m_tree    = nullptr;
    QStackedWidget*  m_stack   = nullptr;

    // Phase 8 of #167: PA category nav-tree root + 3 child items, plus
    // the page widgets themselves so applyPaVisibility() can toggle each.
    QTreeWidgetItem* m_paCategoryItem  = nullptr;
    QTreeWidgetItem* m_paGainItem      = nullptr;
    QTreeWidgetItem* m_paWattMeterItem = nullptr;
    QTreeWidgetItem* m_paValuesItem    = nullptr;
    PaGainByBandPage* m_paGainPage     = nullptr;
    PaWattMeterPage*  m_paWattMeterPage= nullptr;
    PaValuesPage*     m_paValuesPage   = nullptr;

#ifdef NEREUS_BUILD_TESTS
public:
    // Phase 9 of #167: test seams for verifying the cross-page wiring
    // connect() between PaWattMeterPage::resetPaValuesRequested and
    // PaValuesPage::resetPaValues() set up at the end of buildTree().
    PaWattMeterPage* paWattMeterPageForTest() const { return m_paWattMeterPage; }
    PaValuesPage*    paValuesPageForTest()    const { return m_paValuesPage;    }
#endif
};

} // namespace NereusSDR
