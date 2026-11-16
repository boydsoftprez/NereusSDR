// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 18.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Setup > Keys and controllers > Knobs: the eight devices with their state, a drawing
// of the chosen one, a row per control and gesture, and its behavior. Knobs plug into
// this computer, so it works the same in a remote window. Every device says Untested
// until someone tries a real one (R-HK-19).
#pragma once

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"

#include <QHash>
#include <QPointer>
#include <QString>

#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QSlider;
class QTableWidget;
class QWidget;

namespace NereusSDR {

class ControllerHost;
class HotKeyBindings;

class NEREUS_GUI_EXPORT KnobsSetupPage : public SetupPage {
    Q_OBJECT

public:
    enum class Platform { Mac, Windows, Linux };

    using ReasonProvider = std::function<QString(const QString&)>;

    // The slider's slow end: one step per this many clicks of the wheel.
    static constexpr int kSpeedSlowest = 10;

    // A null host or bindings leaves the rows disabled, with the reason.
    KnobsSetupPage(ControllerHost* host, HotKeyBindings* bindings, QWidget* parent = nullptr);
    ~KnobsSetupPage() override;

    // Why an action can't be used on this window; empty when it can.
    void setUnavailableReasonProvider(ReasonProvider provider);
    void refreshAvailability();

    // False for a device this build can't use, which stays listed and disabled.
    bool selectDevice(DeviceId id);
    DeviceId selectedDevice() const { return m_device; }
    ControllerHost* controllerHost() const;

    // What each device's line and message say. Free of the running system so a test
    // on one computer can read all three.
    static Platform thisPlatform();
    // The short word for the device list.
    static QString stateWord(bool connected, const QString& reason);
    // The app a user would quit, or empty when there is nothing to name but "the other app".
    static QString appToQuit(DeviceId id, Platform platform);
    // The whole message, with the fix, or empty when nothing needs saying.
    static QString stateFix(DeviceId id, bool connected, const QString& reason, Platform platform);
    // What is always true of this device on this system. Input Monitoring on a Mac
    // shows only with the dial there, so nobody without one reads about it.
    static QString deviceNote(DeviceId id, bool connected, const QString& reason, Platform platform);

    // A test build has both, so a test says it doesn't.
    void setSupportedForTest(bool usb, bool serial);
    void setPlatformForTest(Platform platform);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        int control{0};     // index into the device's controls
        QString controlKey;
        ControlGesture gesture{ControlGesture::Press};
        QComboBox* action{nullptr};
        QComboBox* down{nullptr};
        QComboBox* actsOn{nullptr};
        QCheckBox* staysOn{nullptr};
    };

    void buildList(QWidget* parent);
    void buildBehavior();
    void showDevice(DeviceId id);
    void buildRows();
    void addRow(int control, const QString& controlKey, ControlGesture gesture);
    void markActions(QComboBox* combo, const Row& row) const;
    void syncRows();
    void syncRow(Row& row);
    void syncBehavior();
    void chooseControl(int control);
    void refreshStates();
    void applyAvailability();
    void setStatus(const QString& text);

    void onActionChosen(int rowIndex, const QString& id);
    void onDownChosen(int rowIndex, const QString& id);
    void onActsOnChosen(int rowIndex, const QString& letter);

    bool deviceSupported(DeviceId id) const;
    QString unsupportedReason(DeviceId id) const;
    QString unavailableReason() const;
    HotKeyDeviceBindings* store() const;
    HotKeyDeviceBinding bindingOf(const Row& row) const;
    bool connectedNow(DeviceId id) const;
    QString reasonNow(DeviceId id) const;

    QPointer<ControllerHost> m_host;
    QPointer<HotKeyBindings> m_bindings;
    ReasonProvider m_reason;

    bool m_usbSupported{false};
    bool m_serialSupported{false};
    Platform m_platform{Platform::Mac};
    DeviceId m_device{DeviceId::Rc28};

    QHash<DeviceId, QWidget*> m_deviceRows;
    QWidget* m_drawing{nullptr};
    std::vector<QWidget*> m_controlCaps;
    QLabel* m_state{nullptr};
    QLabel* m_note{nullptr};
    QTableWidget* m_table{nullptr};
    std::vector<Row> m_rows;
    QLabel* m_status{nullptr};
    QGroupBox* m_behavior{nullptr};
    QSlider* m_speed{nullptr};
    QLabel* m_speedValue{nullptr};
    QCheckBox* m_reverse{nullptr};
    QCheckBox* m_lights{nullptr};
};

} // namespace NereusSDR
