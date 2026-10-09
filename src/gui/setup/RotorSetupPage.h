#pragma once
// no-port-check: NereusSDR-original. The antenna rotor's setup page.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/RotorSetupPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 7.
// Settings -> CAT & Network -> Rotor, beside 4O3A (the PGXL and TGXL) and
// RF-Kit (the RF2K-S).
//
// The rotor is the Core's: its setup lives under Rotor/* in the Core's
// own settings, and this page never writes them. It reads the rotor as
// RadioModel::rotorModel() reports it (the `rotor` object: the setup, the
// Core's serial ports, whether rotctld is there) and changes it only
// through RadioModel's rotor commands (RotorCommandSink): Save and connect
// sends configureRotor, Save presets sends setRotorPresets. On a desktop
// running its own radio those go to this process's rotor; in a remote
// window they go to the Core, so the serial port list is the Core's
// computer's, not this one's.
//
// The accessory refusal rule: a refusal shows on this page while it is
// open. A remote window's command is claimed for the page
// (RadioModel::noteAccessoryRequestShownOnPage), so MainWindow's notice
// says it only when the page is no longer on screen.
//
// Disabled, never hidden: with a Core too old for the rotor every control
// stays, greyed, and the page says why. A field the chosen driver does
// not use is greyed, not removed.
//
// Wire contract: docs/architecture/2026-10-07-remote-rotor-control-v1.md.
// Design: docs/architecture/2026-10-07-rotor-control-design.md (Desktop).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 7).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Final review I3: the page says when it
//                                    is on screen, so the rotor's computer
//                                    reads its ports only then.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QPointer>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace NereusSDR {

class RadioModel;
class RotorCommandSink;
namespace RotorLink { class RotorModel; }

class RotorSetupPage : public QWidget {
    Q_OBJECT

public:
    explicit RotorSetupPage(RadioModel* model, QWidget* parent = nullptr);
    ~RotorSetupPage() override;

    // Test seams (tst_rotor_setup_page).
    QComboBox*      driverComboForTesting() const { return m_driver; }
    QComboBox*      serialPortComboForTesting() const { return m_serialPort; }
    QComboBox*      baudComboForTesting() const { return m_baud; }
    QLineEdit*      hostEditForTesting() const { return m_host; }
    QSpinBox*       portSpinForTesting() const { return m_port; }
    QComboBox*      hamlibComboForTesting() const { return m_hamlib; }
    QSpinBox*       hamlibModelSpinForTesting() const { return m_hamlibModel; }
    QComboBox*      axesComboForTesting() const { return m_axes; }
    QComboBox*      endStopComboForTesting() const { return m_endStop; }
    QComboBox*      rangeComboForTesting() const { return m_range; }
    QDoubleSpinBox* offsetSpinForTesting() const { return m_offset; }
    QTableWidget*   presetsTableForTesting() const { return m_presets; }
    QPushButton*    saveButtonForTesting() const { return m_save; }
    QPushButton*    savePresetsButtonForTesting() const { return m_savePresets; }
    QPushButton*    addPresetButtonForTesting() const { return m_addPreset; }
    QPushButton*    removePresetButtonForTesting() const { return m_removePreset; }
    QString         statusTextForTesting() const;
    QString         messageTextForTesting() const;
    QString         availabilityTextForTesting() const;

protected:
    // On screen, the rotor's computer reads its serial ports (and, on a
    // remote Core, is asked to); off screen it stops unless a rotor is set
    // up (RotorCommandSink::setRotorSetupViewOpen).
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setViewOpen(bool open);
    QGroupBox* buildConnectionGroup();
    QGroupBox* buildRotorGroup();
    QGroupBox* buildPresetsGroup();

    bool isRemote() const;
    // The rotor's setup and presets as the rotor reports them, into the
    // fields the operator has not changed since they last went out.
    void refreshFromRotor();
    void refreshSerialPorts();
    void refreshPresetsFromRotor();
    void refreshStatus();
    // Whether this window can set up a rotor at all; greys everything with
    // the reason when it cannot.
    void refreshAvailability();
    // Greys the fields the chosen driver does not use.
    void refreshDriverFields();
    void selectHamlibModel(int model);

    void onSaveClicked();
    void onSavePresetsClicked();
    void onCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    // What the last command's answer says, on the page.
    void showMessage(const QString& text);

    RadioModel* m_model{nullptr};
    RotorCommandSink* m_commands{nullptr};
    // The model the open view was reported to (it may go first).
    QPointer<RadioModel> m_viewModel;
    bool m_viewOpen{false};
    RotorLink::RotorModel* m_rotor{nullptr};

    QLabel* m_availability{nullptr};
    QLabel* m_status{nullptr};
    QLabel* m_message{nullptr};

    QGroupBox* m_connectionBox{nullptr};
    QGroupBox* m_rotorBox{nullptr};
    QGroupBox* m_presetsBox{nullptr};

    QComboBox* m_driver{nullptr};
    QLabel* m_driverNote{nullptr};
    QComboBox* m_serialPort{nullptr};
    QLabel* m_serialNote{nullptr};
    QComboBox* m_baud{nullptr};
    QLineEdit* m_host{nullptr};
    QSpinBox* m_port{nullptr};
    QComboBox* m_hamlib{nullptr};
    QSpinBox* m_hamlibModel{nullptr};
    QLabel* m_hamlibNote{nullptr};

    QComboBox* m_axes{nullptr};
    QComboBox* m_endStop{nullptr};
    QComboBox* m_range{nullptr};
    QDoubleSpinBox* m_offset{nullptr};

    QTableWidget* m_presets{nullptr};
    QPushButton* m_addPreset{nullptr};
    QPushButton* m_removePreset{nullptr};
    QPushButton* m_savePresets{nullptr};
    QPushButton* m_save{nullptr};

    // The operator changed a field and has not sent it; the rotor's values
    // do not overwrite it until the command is answered.
    bool m_setupTouched{false};
    bool m_presetsTouched{false};
    // While the page itself fills the fields.
    bool m_filling{false};
    // A remote window's command waiting on the Core's answer.
    quint32 m_pendingSetupId{0};
    quint32 m_pendingPresetsId{0};
};

} // namespace NereusSDR
