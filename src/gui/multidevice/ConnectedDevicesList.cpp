// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ConnectedDevicesList.cpp  (NereusSDR)
// =================================================================
//
// See ConnectedDevicesList.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-07, R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/ConnectedDevicesList.h"

#include "core/session/RemoteDevicesState.h"
#include "gui/multidevice/DeviceWords.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace NereusSDR {

ConnectedDevicesList::ConnectedDevicesList(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ConnectedDevicesList"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_empty = new QLabel(this);
    m_empty->setWordWrap(true);
    layout->addWidget(m_empty);
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("connectedDevicesTree"));
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({QStringLiteral("Device"), QStringLiteral("Name"),
                             QStringLiteral("Connected"), QStringLiteral("Last active"),
                             QStringLiteral("Slices"), QStringLiteral("Transmit")});
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setMinimumHeight(140);
    layout->addWidget(m_tree);
    rebuild();
}

void ConnectedDevicesList::setDevices(RemoteDevicesState* devices, const QString& selfDeviceId)
{
    if (m_devices) {
        disconnect(m_devices, nullptr, this, nullptr);
    }
    m_devices = devices;
    m_selfId = selfDeviceId;
    if (m_devices) {
        connect(m_devices, &RemoteDevicesState::connectedDevicesChanged, this,
                &ConnectedDevicesList::rebuild);
        connect(m_devices, &RemoteDevicesState::pairedDevicesChanged, this,
                &ConnectedDevicesList::rebuild);
    }
    rebuild();
}

void ConnectedDevicesList::setSelfDeviceId(const QString& selfDeviceId)
{
    if (m_selfId == selfDeviceId) { return; }
    m_selfId = selfDeviceId;
    rebuild();
}

QStringList ConnectedDevicesList::rowFor(const RemoteConnectedDevice& device, bool self)
{
    QString shortName = device.shortName.isEmpty() ? device.name : device.shortName;
    if (self) {
        shortName += QStringLiteral(" (this window)");
    }
    QString name = device.name;
    if (device.hostsCore) {
        name += QStringLiteral(" (runs the Core)");
    }
    QString last;
    if (device.state == QStringLiteral("away")) {
        last = QStringLiteral("away for %1").arg(DeviceWords::duration(device.awayForSeconds));
    } else {
        last = device.lastActivitySeconds < 60
            ? QStringLiteral("just now")
            : QStringLiteral("%1 ago").arg(DeviceWords::duration(device.lastActivitySeconds));
    }
    QStringList slices;
    for (const RemoteDeviceSlice& s : device.listeningOn) {
        slices << DeviceWords::sliceShort(s);
    }
    QString tx;
    if (device.state == QStringLiteral("transmitting")) {
        tx = QStringLiteral("TX, on the air %1").arg(
            DeviceWords::duration(device.transmittingForSeconds));
        if (device.transmittingOn) {
            tx += QStringLiteral(" (slice ") + device.transmittingOn->letter + QLatin1Char(')');
        }
    } else if (device.holdsTransmit) {
        tx = QStringLiteral("TX");
    }
    return {shortName, name, DeviceWords::duration(device.connectedForSeconds), last,
            slices.isEmpty() ? QStringLiteral("none") : slices.join(QStringLiteral(", ")), tx};
}

void ConnectedDevicesList::setNoCoreText(const QString& text)
{
    m_noCoreText = text;
    rebuild();
}

QString ConnectedDevicesList::notSentText()
{
    return QStringLiteral("The Core has not said who is connected. It says so to a computer "
                          "paired with it, once the Core is up to date.");
}

void ConnectedDevicesList::rebuild()
{
    m_tree->clear();
    if (!m_devices) {
        m_empty->setText(m_noCoreText.isEmpty()
                             ? QStringLiteral("Connect to the Core to see who is connected.")
                             : m_noCoreText);
        m_empty->setVisible(true);
        return;
    }
    const QList<RemoteConnectedDevice> connected = m_devices->connectedDevices();
    if (connected.isEmpty()) {
        m_empty->setText(m_noCoreText.isEmpty() ? ConnectedDevicesList::notSentText()
                                                : m_noCoreText);
        m_empty->setVisible(true);
        return;
    }
    const int limit = m_devices->deviceLimit();
    m_empty->setText(limit > 0 ? QStringLiteral("Connected now: %1 of %2 places.")
                                     .arg(connected.size())
                                     .arg(limit)
                               : QStringLiteral("Connected now: %1.").arg(connected.size()));
    m_empty->setVisible(true);
    QSet<QString> shown;
    for (const RemoteConnectedDevice& device : connected) {
        const QString self = m_selfId.isEmpty() ? m_devices->selfDeviceId() : m_selfId;
        auto* item = new QTreeWidgetItem(m_tree,
                                         rowFor(device, !self.isEmpty() && device.deviceId == self));
        if (device.holdsTransmit) {
            item->setForeground(5, QColor(0xff, 0x60, 0x60));
        }
        if (device.state == QStringLiteral("away")) {
            for (int c = 0; c < 4; ++c) {
                item->setForeground(c, QColor(0x90, 0x98, 0xa4));
            }
        }
        shown.insert(device.deviceId);
    }
    bool pairedHeader = false;
    for (const RemotePairedDevice& paired : m_devices->pairedDevices()) {
        if (shown.contains(paired.id)) {
            continue;
        }
        if (!pairedHeader) {
            auto* header = new QTreeWidgetItem(m_tree, {QStringLiteral("Paired, not connected")});
            QFont bold = header->font(0);
            bold.setBold(true);
            header->setFont(0, bold);
            header->setFirstColumnSpanned(true);
            pairedHeader = true;
        }
        new QTreeWidgetItem(m_tree, {paired.shortName.isEmpty() ? paired.name : paired.shortName,
                                     paired.name, QString(), QString(), QString(), QString()});
    }
    for (int c = 0; c < m_tree->columnCount(); ++c) {
        m_tree->resizeColumnToContents(c);
    }
}

} // namespace NereusSDR
