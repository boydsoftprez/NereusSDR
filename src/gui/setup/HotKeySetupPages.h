// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Setup > Keys and controllers. The Keyboard page shows and changes the keys.
// It is this computer's, so it works the same in a remote window.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace NereusSDR {

class HotKeyBindings;
class KeyboardMapWidget;
struct HotKeyAction;

class NEREUS_GUI_EXPORT KeyboardPage : public SetupPage {
    Q_OBJECT

public:
    using ReasonProvider = std::function<QString(const QString&)>;

    // Null bindings leave every control disabled, with the reason.
    explicit KeyboardPage(HotKeyBindings* bindings, QWidget* parent = nullptr);

    // Why an action can't be used on this window; empty when it can. The window
    // gives this; on a page on its own, every action is available.
    void setUnavailableReasonProvider(ReasonProvider provider);
    void refreshAvailability();

    // What the Import and Export buttons do after their file dialogs.
    bool importFromFile(const QString& path);
    bool exportToFile(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct Row {
        QString id;
        int index{0};
        QWidget* nameCell{nullptr};
        QLabel* name{nullptr};
        QFrame* messageFrame{nullptr};
        QLabel* message{nullptr};
        QPushButton* replace{nullptr};
        QPushButton* dismiss{nullptr};
        QTableWidgetItem* key{nullptr};
        QComboBox* actsOn{nullptr};
        QPushButton* set{nullptr};
        QPushButton* clear{nullptr};
        QPushButton* toDefault{nullptr};
    };

    void buildSwitch();
    void buildMap();
    void buildActions();
    void buildRow(Row& row, const HotKeyAction& action);

    void refreshAll();
    void refreshRow(Row& row);
    void refreshPick();
    void applyFilter();
    void applyEnabled(bool on);
    Row* rowFor(const QString& id);
    QString reasonFor(const QString& id) const;

    void beginCapture(const QString& id);
    void endCapture();
    void tryBind(const QString& id, const QString& chord);
    void showMessage(const QString& id, const QString& text, const QString& replaceChord);
    void dismissMessage();
    void fitMessageRow();
    void revealRow(Row& row);

    void onKeyClicked(const QString& chord);
    void onImportClicked();
    void onExportClicked();
    void setStatus(const QString& text);

    QPointer<HotKeyBindings> m_bindings;
    ReasonProvider m_reason;

    QCheckBox* m_useShortcuts{nullptr};
    QGroupBox* m_mapGroup{nullptr};
    QGroupBox* m_actionsGroup{nullptr};
    KeyboardMapWidget* m_map{nullptr};
    QButtonGroup* m_layers{nullptr};
    std::vector<QPushButton*> m_chips;

    QLabel* m_pickChord{nullptr};
    QLabel* m_pickText{nullptr};
    QLabel* m_pickNote{nullptr};
    QPushButton* m_pickChange{nullptr};
    QPushButton* m_pickClear{nullptr};
    QComboBox* m_pickAssign{nullptr};
    QString m_pick;

    QLineEdit* m_search{nullptr};
    QComboBox* m_groupFilter{nullptr};
    QTableWidget* m_table{nullptr};
    int m_noMatchRow{0};
    std::vector<Row> m_rows;
    QLabel* m_status{nullptr};
    QPlainTextEdit* m_skipped{nullptr};

    QString m_captureId;
    QString m_messageId;
    QString m_replaceChord;
};

} // namespace NereusSDR
