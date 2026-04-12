#pragma once

#include <QDialog>
#include <QUuid>
#include <QString>

class QTreeWidget;
class QPushButton;

namespace NereusSDR {

// Phase 3G-6 block 5 — popup that lets the user pick an MMIO
// (endpoint, variableName) pair to bind a meter item to. Opened
// by the "Variable…" button next to the Binding row in each
// BaseItemEditor subclass. Matches Thetis frmVariablePicker
// (Console/frmVariablePicker.cs) semantics: OK writes the chosen
// pair back to the caller, Clear resets to null guid + empty name
// (Thetis sentinel "--DEFAULT--"), Cancel leaves the caller's
// state untouched.
class MmioVariablePickerPopup : public QDialog {
    Q_OBJECT

public:
    // Construct pre-selecting an existing (guid, name) pair.
    // Pass null guid and empty name to start with no selection.
    MmioVariablePickerPopup(const QUuid& initialGuid,
                            const QString& initialVariable,
                            QWidget* parent = nullptr);
    ~MmioVariablePickerPopup() override;

    // Outputs — call these after exec() returns Accepted.
    QUuid   selectedGuid() const { return m_selectedGuid; }
    QString selectedVariable() const { return m_selectedVariable; }
    bool    wasCleared() const { return m_cleared; }

private slots:
    void onAccept();
    void onClear();

private:
    void buildTree();

    QTreeWidget* m_tree{nullptr};
    QPushButton* m_btnOk{nullptr};
    QPushButton* m_btnClear{nullptr};
    QPushButton* m_btnCancel{nullptr};

    QUuid   m_selectedGuid;
    QString m_selectedVariable;
    bool    m_cleared{false};
};

} // namespace NereusSDR
