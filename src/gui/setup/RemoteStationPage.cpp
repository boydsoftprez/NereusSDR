// no-port-check: NereusSDR-original presentation for the local Core.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/setup/RemoteStationPage.h"
#include "gui/StyleConstants.h"
#include "gui/multidevice/ConnectedDevicesList.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {
namespace {
QString unavailableText(const RemoteStationPage::State& state)
{
    if (state.transmitting) {
        return QObject::tr("Setup controls are locked while transmitting.");
    }
    if (state.busy) {
        return QObject::tr("A Core change is in progress.");
    }
    if (!state.available) {
        return state.unavailableReason.isEmpty()
            ? QObject::tr("Connect this window to a local radio to manage its Core.")
            : state.unavailableReason;
    }
    return {};
}
} // namespace

RemoteStationPage::RemoteStationPage(QWidget* parent)
    : SetupPage(QStringLiteral("Remote Access"), parent)
{
    Style::applyDarkPageStyle(this);
    m_reason = new QLabel(this);
    m_reason->setObjectName(QStringLiteral("remoteAccessReason"));
    m_reason->setTextFormat(Qt::PlainText);
    m_reason->setWordWrap(true);
    contentLayout()->insertWidget(0, m_reason);

    // Opening Connections only navigates this window; it must remain
    // available when hosting controls are locked or this Core is unavailable.
    QGroupBox* window = addSection(tr("This window"));
    QVBoxLayout* windowLayout = qobject_cast<QVBoxLayout*>(window->layout());
    auto* connections = new QPushButton(tr("Connections…"), window);
    connections->setObjectName(QStringLiteral("remoteStationConnections"));
    connections->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connections->setAutoDefault(false);
    windowLayout->addWidget(connections, 0, Qt::AlignLeft);
    connect(connections, &QPushButton::clicked, this, &RemoteStationPage::connectionsRequested);

    QGroupBox* core = addSection(tr("Core on this computer"));
    QVBoxLayout* coreLayout = qobject_cast<QVBoxLayout*>(core->layout());
    m_runCore = new QCheckBox(tr("Run a Core on this computer"), core);
    m_runCore->setObjectName(QStringLiteral("remoteAccessRunCore"));
    m_keepRunning = new QCheckBox(tr("Keep it running when NereusSDR is closed"), core);
    m_keepRunning->setObjectName(QStringLiteral("remoteAccessKeepRunning"));
    m_startWithComputer = new QCheckBox(tr("Start it with the computer"), core);
    m_startWithComputer->setObjectName(QStringLiteral("remoteAccessStartWithComputer"));
    coreLayout->addWidget(m_runCore);
    coreLayout->addWidget(m_keepRunning);
    coreLayout->addWidget(m_startWithComputer);

    QGroupBox* station = addSection(tr("Core"));
    QVBoxLayout* stationLayout = qobject_cast<QVBoxLayout*>(station->layout());
    QHBoxLayout* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(tr("Core name:"), station));
    m_name = new QLabel(station);
    m_name->setObjectName(QStringLiteral("remoteAccessStationName"));
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_name->setTextFormat(Qt::PlainText);
    m_name->setWordWrap(true);
    nameRow->addWidget(m_name, 1);
    m_rename = new QPushButton(tr("Rename"), station);
    m_rename->setObjectName(QStringLiteral("remoteAccessRename"));
    m_rename->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_rename->setAutoDefault(false);
    nameRow->addWidget(m_rename);
    stationLayout->addLayout(nameRow);
    m_reachability = new QLabel(station);
    m_reachability->setObjectName(QStringLiteral("remoteAccessReachability"));
    m_reachability->setTextFormat(Qt::PlainText);
    m_reachability->setWordWrap(true);
    stationLayout->addWidget(m_reachability);
    m_pairingCode = new QLabel(station);
    m_pairingCode->setObjectName(QStringLiteral("remoteAccessPairingCode"));
    m_pairingCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pairingCode->setTextFormat(Qt::PlainText);
    m_pairingCode->setWordWrap(true);
    stationLayout->addWidget(m_pairingCode);
    m_pairingInstruction = new QLabel(tr("Enter this code on your device to pair it with this Core."), station);
    m_pairingInstruction->setWordWrap(true);
    stationLayout->addWidget(m_pairingInstruction);
    // LINK-I4: pairing through the remote access service is off.
    m_servicePairingShut = new QLabel(
        tr("Pairing from outside your network is off after too many wrong codes. "
           "Click Add a device to turn it back on."), station);
    m_servicePairingShut->setObjectName(QStringLiteral("remoteAccessServicePairingShut"));
    m_servicePairingShut->setWordWrap(true);
    m_servicePairingShut->setVisible(false);
    stationLayout->addWidget(m_servicePairingShut);

    QGroupBox* devices = addSection(tr("Paired devices"));
    QVBoxLayout* devicesLayout = qobject_cast<QVBoxLayout*>(devices->layout());
    m_deviceRows = new QWidget(devices);
    m_devicesLayout = new QVBoxLayout(m_deviceRows);
    m_devicesLayout->setContentsMargins(0, 0, 0, 0);
    devicesLayout->addWidget(m_deviceRows);
    m_addDevice = new QPushButton(tr("Add a device"), devices);
    m_addDevice->setObjectName(QStringLiteral("remoteAccessAddDevice"));
    m_addDevice->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_addDevice->setAutoDefault(false);
    devicesLayout->addWidget(m_addDevice, 0, Qt::AlignLeft);

    QGroupBox* backup = addSection(tr("Core key backup"));
    m_backupGroup = backup;
    QVBoxLayout* backupLayout = qobject_cast<QVBoxLayout*>(backup->layout());
    m_backupPath = new QLabel(backup);
    m_backupPath->setObjectName(QStringLiteral("remoteAccessBackupPath"));
    m_backupPath->setTextFormat(Qt::PlainText);
    m_backupPath->setWordWrap(true);
    m_backupPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    backupLayout->addWidget(m_backupPath);
    m_backupAcknowledged = new QPushButton(tr("I've backed it up"), backup);
    m_backupAcknowledged->setObjectName(QStringLiteral("remoteAccessBackupAcknowledged"));
    m_backupAcknowledged->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_backupAcknowledged->setAutoDefault(false);
    backupLayout->addWidget(m_backupAcknowledged, 0, Qt::AlignLeft);

    // iPhone app plan Task 78 item 8 (R-IOS-07): who is connected to this
    // Core now, below the rest of the page.
    QGroupBox* connected = addSection(tr("Connected now"));
    QVBoxLayout* connectedLayout = qobject_cast<QVBoxLayout*>(connected->layout());
    m_connectedList = new ConnectedDevicesList(connected);
    m_connectedList->setObjectName(QStringLiteral("remoteAccessConnectedNow"));
    m_connectedList->setNoCoreText(tr("Run a Core on this computer to see who is connected."));
    connectedLayout->addWidget(m_connectedList);

    connect(m_runCore, &QCheckBox::clicked, this, [this](bool value) {
        {
            QSignalBlocker blocker(m_runCore);
            m_runCore->setChecked(m_state.runCore);
        }
        emit runCoreRequested(value);
    });
    connect(m_keepRunning, &QCheckBox::clicked, this, [this](bool value) {
        {
            QSignalBlocker blocker(m_keepRunning);
            m_keepRunning->setChecked(m_state.keepRunning);
        }
        emit keepRunningRequested(value);
    });
    connect(m_startWithComputer, &QCheckBox::clicked, this, [this](bool value) {
        {
            QSignalBlocker blocker(m_startWithComputer);
            m_startWithComputer->setChecked(m_state.startWithComputer);
        }
        emit startWithComputerRequested(value);
    });
    connect(m_rename, &QPushButton::clicked, this, [this]() {
        const QPointer<RemoteStationPage> self(this);
        const QString originalName = m_state.stationName;
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Rename Core"), tr("Core name:"),
                                                   QLineEdit::Normal, m_state.stationName,
                                                   &accepted).trimmed();
        // The modal dialog runs the event loop: transmit, host retirement,
        // or another rename may change permission before it returns.
        if (self && m_rename->isEnabled() && m_state.stationName == originalName
            && accepted && !name.isEmpty() && name != originalName) {
            emit renameRequested(name);
        }
    });
    connect(m_addDevice, &QPushButton::clicked, this, &RemoteStationPage::addDeviceRequested);
    connect(m_backupAcknowledged, &QPushButton::clicked, this,
            &RemoteStationPage::keyBackupAcknowledgedRequested);
    refresh();
}

void RemoteStationPage::setState(const State& state)
{
    m_state = state;
    QSignalBlocker runBlocker(m_runCore);
    QSignalBlocker keepBlocker(m_keepRunning);
    QSignalBlocker startBlocker(m_startWithComputer);
    m_runCore->setChecked(state.runCore);
    m_keepRunning->setChecked(state.keepRunning);
    m_startWithComputer->setChecked(state.startWithComputer);
    refresh();
}

void RemoteStationPage::setConnectedDevices(RemoteDevicesState* devices)
{
    m_connectedList->setDevices(devices);
}

bool RemoteStationPage::confirm(const QString& title, const QString& text, const QString& goAhead)
{
    if (m_confirmation) {
        return m_confirmation(title, text, goAhead);
    }
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Cancel, this);
    QPushButton* proceed = box.addButton(goAhead, QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == proceed;
}

void RemoteStationPage::applyGate(QWidget* control, bool allowed, const QString& reason)
{
    control->setEnabled(allowed);
    control->setToolTip(allowed ? QString() : reason);
    control->setAccessibleDescription(allowed ? QString() : reason);
}

void RemoteStationPage::refresh()
{
    const QString unavailable = unavailableText(m_state);
    m_reason->setText(unavailable);
    m_reason->setVisible(!unavailable.isEmpty());
    const bool allowed = unavailable.isEmpty();
    applyGate(m_runCore, allowed, unavailable);
    const QString coreReason = allowed && !m_state.runCore
        ? tr("Run a Core on this computer first.") : unavailable;
    applyGate(m_keepRunning, allowed && m_state.runCore, coreReason);
    applyGate(m_startWithComputer, allowed && m_state.runCore, coreReason);
    m_name->setText(m_state.stationName.isEmpty() ? tr("No Core name") : m_state.stationName);
    m_reachability->setText(m_state.reachabilityText);
    m_reachability->setVisible(!m_state.reachabilityText.isEmpty());
    m_pairingCode->setText(tr("Pairing code: %1").arg(m_state.pairingCode));
    const bool showCode = m_state.runCore && m_state.pairingOpen && !m_state.pairingCode.isEmpty();
    m_pairingCode->setVisible(showCode);
    m_pairingInstruction->setVisible(showCode);
    m_servicePairingShut->setVisible(m_state.runCore && m_state.servicePairingShut);
    applyGate(m_rename, allowed && m_state.runCore, coreReason);
    applyGate(m_addDevice, allowed && m_state.runCore, coreReason);
    m_backupPath->setText(m_state.keyBackupPath.isEmpty()
        ? tr("Back up this Core's key when it is available.")
        : tr("Back up the Core key file: %1").arg(m_state.keyBackupPath));
    m_backupPath->setVisible(!m_state.keyBackupAcknowledged);
    m_backupAcknowledged->setVisible(!m_state.keyBackupAcknowledged);
    m_backupGroup->setVisible(!m_state.keyBackupAcknowledged);
    applyGate(m_backupAcknowledged,
              allowed && m_state.runCore && !m_state.keyBackupPath.isEmpty(),
              !m_state.keyBackupPath.isEmpty() ? coreReason : tr("The Core key is not available."));
    rebuildDevices();
}

void RemoteStationPage::rebuildDevices()
{
    ++m_deviceGeneration;
    while (QLayoutItem* item = m_devicesLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->disconnect(this);
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    const QString unavailable = unavailableText(m_state);
    const bool allowed = unavailable.isEmpty() && m_state.runCore;
    for (const Device& device : m_state.devices) {
        QWidget* row = new QWidget(m_deviceRows);
        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        QLabel* label = new QLabel(tr("%1\nPaired: %2 · Last seen: %3")
            .arg(device.name, device.pairedText, device.lastSeenText), row);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        layout->addWidget(label, 1);
        QPushButton* revoke = new QPushButton(tr("Revoke"), row);
        revoke->setObjectName(QStringLiteral("remoteAccessRevoke"));
        revoke->setProperty("deviceId", device.id);
        revoke->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        revoke->setAutoDefault(false);
        layout->addWidget(revoke);
        const QString reason = !device.revocable
            ? (device.revokeReason.isEmpty() ? tr("This device cannot be revoked here.")
                                             : device.revokeReason)
            : allowed ? QString() : unavailable.isEmpty()
                ? tr("Run a Core on this computer first.") : unavailable;
        applyGate(revoke, allowed && device.revocable, reason);
        connect(revoke, &QPushButton::clicked, this,
                [this, revoke, id = device.id, generation = m_deviceGeneration]() {
            if (generation != m_deviceGeneration || !revoke->isEnabled()) { return; }
            for (const Device& current : m_state.devices) {
                if (current.id != id || !current.revocable) {
                    continue;
                }
                if (!current.removalStopsPairingToken) {
                    emit revokeRequested(id);
                    return;
                }
                // Slice control plan Task 8b: it joined with the pairing
                // token, which still works; while it does, a computer
                // removed this way could join again, so the Core stops
                // accepting the token first. Said plainly before it happens.
                const QPointer<RemoteStationPage> self(this);
                const QString name = current.name;
                const bool goAhead = confirm(
                    tr("Remove %1").arg(name),
                    tr("%1 joined this Core with its pairing token. While the Core accepts "
                       "that token, %1 could join again, so the Core stops accepting it "
                       "first.\n\n"
                       "This is permanent: the pairing token will never again let anyone "
                       "join or connect, and anything connected with it now is "
                       "disconnected. Paired devices keep working, and new devices pair with "
                       "a code from Add a device.")
                        .arg(name),
                    tr("Stop Accepting the Pairing Token and Remove"));
                // The question runs the event loop: the list may change.
                if (!self || generation != m_deviceGeneration || !goAhead) { return; }
                emit revokeStoppingPairingTokenRequested(id);
                return;
            }
        });
        m_devicesLayout->addWidget(row);
    }
    if (m_state.devices.isEmpty()) {
        QLabel* empty = new QLabel(tr("No paired devices."), m_deviceRows);
        m_devicesLayout->addWidget(empty);
    }
}

} // namespace NereusSDR
