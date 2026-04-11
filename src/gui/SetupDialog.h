#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QMap>

namespace NereusSDR {

class RadioModel;
class SetupPage;

// Main settings dialog with tree-based navigation.
// Left pane: QTreeWidget with 10 top-level categories.
// Right pane: QStackedWidget showing the selected page.
class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(RadioModel* model, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildTree();
    // Creates a SetupPage, adds a tree item for it, and pushes it onto the stack.
    // Returns the new tree item.
    QTreeWidgetItem* addPage(QTreeWidgetItem* parent,
                             const QString& label,
                             RadioModel* model);

    RadioModel*      m_model   = nullptr;
    QTreeWidget*     m_tree    = nullptr;
    QStackedWidget*  m_stack   = nullptr;
};

} // namespace NereusSDR
