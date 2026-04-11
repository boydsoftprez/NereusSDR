#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSplitter>

namespace NereusSDR {

class RadioModel;
class SetupPage;

// Main settings dialog with tree-based navigation.
// Left pane: QTreeWidget with top-level category items.
// Right pane: QStackedWidget showing the selected page.
class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(RadioModel* model, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildTree();

    RadioModel*      m_model   = nullptr;
    QTreeWidget*     m_tree    = nullptr;
    QStackedWidget*  m_stack   = nullptr;
};

} // namespace NereusSDR
