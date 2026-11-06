// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Setup > Keys and controllers > MIDI: the device, the profile, learn, and the
// list of learned controls. It is this computer's, so it works the same in a
// remote window. A controller starts empty, because its controls differ per model.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"
#include "gui/hotkeys/midi/MidiProfile.h"

#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QWidget;

namespace NereusSDR {

class ControllerHost;
class HotKeyBindings;

class NEREUS_GUI_EXPORT MidiSetupPage : public SetupPage {
    Q_OBJECT

public:
    using ReasonProvider = std::function<QString(const QString&)>;

    // How often the device list is looked at again while the page shows.
    static constexpr int kDeviceCheckMs = 2000;

    // Null host or bindings leave every control disabled, with the reason.
    MidiSetupPage(ControllerHost* host, HotKeyBindings* bindings, QWidget* parent = nullptr);
    ~MidiSetupPage() override;

    // Why an action can't be used on this window; empty when it can. The window
    // gives this; on a page on its own, every action is available.
    void setUnavailableReasonProvider(ReasonProvider provider);
    void refreshAvailability();

    // The timer's slot; public for tests.
    void refreshDevices();

    // A test build always has RtMidi, so a test says it doesn't.
    void setSupportedForTest(bool supported);

    // Why the page's controls are disabled; empty when they are not.
    QString unavailableReason() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct Row {
        QString controlKey;
        QWidget* cells[4]{nullptr, nullptr, nullptr, nullptr};
        QComboBox* kind{nullptr};
        QComboBox* action{nullptr};
        QComboBox* down{nullptr};
        QComboBox* actsOn{nullptr};
        QPushButton* clear{nullptr};
    };

    void buildController();
    void buildControls();
    void fillActionCombo(QComboBox* combo, const QString& noneText) const;
    void markUnavailableActions(QComboBox* combo) const;

    void loadActiveProfile();
    void fillProfileCombo();
    void persist();
    void createProfile();
    void renameProfile();
    void deleteProfile();
    void switchProfile(const QString& name);
    QString nameRefusal() const;

    void addRow(const MidiBinding& binding);
    void retireRow(int index);
    void syncRows();
    void syncRow(Row& row, const MidiBinding& binding);
    Row* rowFor(const QString& controlKey);
    const MidiBinding* bindingFor(const QString& controlKey) const;
    void commit(const MidiBinding& binding, const QString& doneText);
    void onKindChosen(const QString& controlKey, MidiControlKind kind);
    void onActionChosen(const QString& controlKey, const QString& id);
    void onDownChosen(const QString& controlKey, const QString& id);
    void onActsOnChosen(const QString& controlKey, const QString& letter);

    void chooseDevice(const QString& name);
    void applyAvailability();

    void beginLearn(const QString& id);
    void endLearn();
    void onLearned(const QString& controlKey, MidiControlKind kind);
    void setStatus(const QString& text);

    QPointer<ControllerHost> m_host;
    QPointer<HotKeyBindings> m_bindings;
    ReasonProvider m_reason;
    MidiProfile m_profile;

    bool m_supported{false};
    QStringList m_ports;
    QString m_connectionReason;

    QTimer* m_deviceTimer{nullptr};
    QGroupBox* m_controlsGroup{nullptr};
    QComboBox* m_device{nullptr};
    QComboBox* m_profileCombo{nullptr};
    QLineEdit* m_name{nullptr};
    QPushButton* m_new{nullptr};
    QPushButton* m_rename{nullptr};
    QPushButton* m_delete{nullptr};
    QLabel* m_availability{nullptr};
    QLabel* m_last{nullptr};

    QComboBox* m_learn{nullptr};
    QLabel* m_learnText{nullptr};
    QPushButton* m_learnCancel{nullptr};
    QTableWidget* m_table{nullptr};
    std::vector<Row> m_rows;     // table row is index + 1; the first says when it's empty
    QLabel* m_status{nullptr};

    QString m_learnId;
};

} // namespace NereusSDR
