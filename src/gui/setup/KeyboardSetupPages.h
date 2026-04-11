#pragma once

#include "gui/SetupPage.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Keyboard > Shortcuts
// Search edit, shortcut table placeholder, edit / reset / import / export
// buttons. All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class KeyboardShortcutsPage : public SetupPage {
    Q_OBJECT

public:
    explicit KeyboardShortcutsPage(QWidget* parent = nullptr);

private:
    QLineEdit*   m_searchEdit{nullptr};
    QLabel*      m_shortcutTableLabel{nullptr};
    QPushButton* m_editButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_importButton{nullptr};
    QPushButton* m_exportButton{nullptr};

    void buildUI();
};

} // namespace NereusSDR
