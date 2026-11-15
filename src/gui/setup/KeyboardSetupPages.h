#pragma once

// =================================================================
// src/gui/setup/KeyboardSetupPages.h  (Longpath)
// =================================================================
//
// Setup > Keyboard > Shortcuts: alle Tastenkuerzel sehen, suchen,
// aendern, zuruecksetzen, als Datei sichern und laden -- und der eine
// Schalter, der sie alle ausmacht.
//
// Bis 2026-11-10 ein Platzhalter (Suchfeld, Tabelle und vier Knoepfe,
// alle gesperrt mit „NYI"). Die Belegung selbst ist HotKeyBindings aus
// dem Upstream NereusSDR; die Seite ist Longpath-eigen und kleiner als
// Upstreams Keyboard-Seite (keine Tastaturkarte, keine Bindung an einen
// bestimmten Empfaenger).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Platzhalter durch die echte Seite ersetzt von Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/SetupPage.h"

#include <QHash>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Longpath {

class HotKeyBindings;

// ---------------------------------------------------------------------------
// Keyboard > Shortcuts
// ---------------------------------------------------------------------------
class KeyboardShortcutsPage : public SetupPage {
    Q_OBJECT

public:
    // bindings darf null sein (Pruefstand ohne Funkmodell): dann bleibt
    // die Seite stehen, aber gesperrt, mit einem Satz warum.
    explicit KeyboardShortcutsPage(HotKeyBindings* bindings, QWidget* parent = nullptr);

    // Fuer den Pruefstand: die Taste einer Handlung so, wie die Tabelle
    // sie zeigt.
    QString shownKeysFor(const QString& id) const;
    QTreeWidget* table() const { return m_table; }

    // Die Taste einer Handlung so, wie der Betreiber sie liest (auf dem
    // Mac mit ⌘ und ⇧). Leer bleibt leer.
    static QString nativeKeyText(const QString& chord);

private:
    void buildUI();
    void refresh();
    void applyFilter(const QString& text);
    void updateButtons();
    QString selectedId() const;

    void changeSelected();
    void clearSelected();
    void resetSelected();
    void resetAll();
    void importFile();
    void exportFile();
    void say(const QString& text, bool warning);

    HotKeyBindings* m_bindings{nullptr};

    QCheckBox*   m_enabledCheck{nullptr};
    QCheckBox*   m_fKeysCheck{nullptr};   // nur macOS sichtbar
    QCheckBox*   m_fKeysHoldCheck{nullptr};  // nur macOS sichtbar
    QLineEdit*   m_searchEdit{nullptr};
    QTreeWidget* m_table{nullptr};
    QLabel*      m_status{nullptr};
    QPushButton* m_editButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_resetAllButton{nullptr};
    QPushButton* m_importButton{nullptr};
    QPushButton* m_exportButton{nullptr};

    QHash<QString, QTreeWidgetItem*> m_itemById;
};

} // namespace Longpath
