#pragma once

#include <QDialog>
#include <QUuid>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTreeWidget;

namespace NereusSDR {

class MmioEndpoint;

// Phase 3G-6 block 5 — MMIO endpoint manager dialog. Lists all
// endpoints from ExternalVariableEngine, lets the user add /
// remove / edit endpoints inline, and shows the discovered-
// variables tree for the currently-selected endpoint.
//
// Replaces the block 3 commit 18 stub that informed the user
// "MMIO is not yet implemented."
class MmioEndpointsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MmioEndpointsDialog(QWidget* parent = nullptr);
    ~MmioEndpointsDialog() override;

private slots:
    void onEndpointSelectionChanged();
    void onAddEndpoint();
    void onRemoveEndpoint();
    void onApplyEdits();
    void onVariablesDiscovered();

private:
    void rebuildEndpointList();
    void loadEditorFromEndpoint(MmioEndpoint* ep);
    void refreshVariablesTree(MmioEndpoint* ep);
    MmioEndpoint* currentEndpoint() const;

    // Left column: endpoint list + Add/Remove
    QListWidget* m_list{nullptr};
    QPushButton* m_btnAdd{nullptr};
    QPushButton* m_btnRemove{nullptr};

    // Center column: endpoint editor
    QLineEdit*   m_editName{nullptr};
    QComboBox*   m_comboTransport{nullptr};
    QComboBox*   m_comboFormat{nullptr};
    QLineEdit*   m_editHost{nullptr};
    QSpinBox*    m_spinPort{nullptr};
    QLineEdit*   m_editDevice{nullptr};
    QSpinBox*    m_spinBaud{nullptr};
    QPushButton* m_btnApply{nullptr};
    QLabel*      m_lblStatus{nullptr};

    // Right column: discovered variables tree
    QTreeWidget* m_treeVariables{nullptr};

    // Bottom
    QPushButton* m_btnClose{nullptr};
};

} // namespace NereusSDR
