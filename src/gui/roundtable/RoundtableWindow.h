#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableWindow.h  (NereusSDR)
// =================================================================
//
// Roundtable in its own window (design 11.2 Pop-out; mockup
// docs/architecture/reporter-brainstorm/mockups/placement.html, its
// popped-out window). It holds the applet's RoundtablePanel while popped
// out and reshapes with its width:
//
//   under 600 px   one column, like the panel (the room list or the room)
//   600 to 980 px  two columns: the room list and the room with messages
//   wider          three columns: the room list (250 px), the room, and the
//                  messages (320 px), the separate-window layout
//
// The Wide preset is 1080 x 600, and the window opens at it the first
// time. It remembers its size and screen under Roundtable/WindowGeometry
// (this computer's own). Closing it with the close box docks the panel
// back (dockRequested); a close that is not the operator's (the quit
// sweep, close() from code) only hides it, as AppletFloatingWindow does.
//
// Keyboard: / searches the room list, the arrows move in it, Enter goes
// into a room, and Escape steps back (from the room to the list, from a
// search to the whole list).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/roundtable/RoundtablePanel.h"

#include <QJsonObject>
#include <QPointer>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace NereusSDR {

class RoundtableModel;

class NEREUS_GUI_EXPORT RoundtableWindow : public QWidget {
    Q_OBJECT

public:
    /// Design 11.2: the widths where the window changes shape.
    static constexpr int kTwoColumnsFrom = 600;
    static constexpr int kThreeColumnsAbove = 980;
    /// Design 11.2: the room list's width in two and three columns.
    static constexpr int kRoomListWidth = 250;
    /// Design 11.2: the Wide preset.
    static constexpr int kWideWidth = 1080;
    static constexpr int kWideHeight = 600;
    /// This computer's own (SettingsScope OperatorLocal).
    static constexpr const char* kGeometryKey = "Roundtable/WindowGeometry";

    enum class Shape { OneColumn, TwoColumns, ThreeColumns };

    /// `model` is not owned. A top-level window over `parent`.
    explicit RoundtableWindow(RoundtableModel* model, QWidget* parent = nullptr);
    ~RoundtableWindow() override;

    /// The shape for a window `width` px wide.
    static Shape shapeFor(int width);
    Shape shape() const { return m_shape; }

    /// Takes the panel (popped out) or gives it back (docking).
    void takePanel(RoundtablePanel* panel);
    RoundtablePanel* releasePanel();
    RoundtablePanel* panel() const { return m_panel; }

    /// Shows it at its saved size and screen, or at the Wide preset the
    /// first time.
    void showAtSavedGeometry();
    /// The Wide preset, 1080 x 600.
    void applyWidePreset();

    /// In one column: whether the room list shows in place of the room.
    bool isListShown() const;

signals:
    /// The operator closed the window: put the panel back.
    void dockRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyShape();
    void rebuildList();
    void openItem(QListWidgetItem* item);
    void stepBack();
    void showList(bool shown);
    void saveGeometryNow();
    QString stateText(const QJsonObject& summary) const;

    QPointer<RoundtableModel> m_model;
    QPointer<RoundtablePanel> m_panel;
    Shape m_shape = Shape::ThreeColumns;
    bool m_listInOneColumn = false;
    bool m_restoring = false;

    QHBoxLayout* m_body = nullptr;
    QWidget* m_listColumn = nullptr;
    QLabel* m_onAir = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_rooms = nullptr;
    QWidget* m_roomColumn = nullptr;
};

} // namespace NereusSDR
