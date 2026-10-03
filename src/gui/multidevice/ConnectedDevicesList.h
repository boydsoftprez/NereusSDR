#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ConnectedDevicesList.h  (NereusSDR)
// =================================================================
//
// Who is connected to the Core now (iPhone app plan Task 78, R-IOS-07,
// R-IOS-02; the several-devices design, section 12 item 8): each device
// with its short name and name, how long it has been connected, when it
// was last active, its slice letters and bands, TX on the holder, and
// away; then the paired devices that are not connected. A remote window
// fills it from the Core's connectedDevices and devices objects. No
// action here: Revoke on the Core's device list already drops a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-07, R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: setNoCoreText() for the hosting desktop's Remote Access
//               page (Task 78 item 8). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QPointer>
#include <QWidget>

class QLabel;
class QTreeWidget;

namespace NereusSDR {

class RemoteDevicesState;
struct RemoteConnectedDevice;

class ConnectedDevicesList : public QWidget {
    Q_OBJECT

public:
    explicit ConnectedDevicesList(QWidget* parent = nullptr);

    /// The state to show; null shows why the list is empty.
    void setDevices(RemoteDevicesState* devices, const QString& selfDeviceId = QString());
    void setSelfDeviceId(const QString& selfDeviceId);

    /// One connected device's row, column by column (for tests and the
    /// screenshot's reader): short name, name, connected, last active,
    /// slices, transmit.
    static QStringList rowFor(const RemoteConnectedDevice& device, bool self);

    /// Why the list is empty while connected.
    static QString notSentText();
    /// The words shown while there is no Core to ask (no state, or an
    /// empty list); the remote window's own words by default.
    void setNoCoreText(const QString& text);

    QTreeWidget* tree() const { return m_tree; }
    QLabel* emptyLabel() const { return m_empty; }

private:
    void rebuild();

    QPointer<RemoteDevicesState> m_devices;
    QString m_selfId;
    QString m_noCoreText;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_empty = nullptr;
};

} // namespace NereusSDR
