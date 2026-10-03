// no-port-check: NereusSDR-original. Setup > This Core in a remote window.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/ThisCorePage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See ThisCorePage.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 21, R-IOS-18,
//                                    R-R3-38, R-R3-49). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 78 (R-IOS-07,
//                                    R-IOS-02): the Connected now list.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  iPhone app plan Task 25: the Core's
//                                    paired devices (Revoke, Add a device)
//                                    and its identity and key backup line.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The model choice is the Core's list for
//                                    the radio's board (stationRadios'
//                                    models); disabled with a reason on a
//                                    Core that does not send it. AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include "gui/setup/ThisCorePage.h"
#include "gui/multidevice/ConnectedDevicesList.h"

#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteDevicesState.h"
#include "core/station/StationRadios.h"
#include "models/RadioModel.h"

#include <QComboBox>
#include <QDateTime>
#include <QLocale>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

constexpr int kMacRole = Qt::UserRole + 1;
// The models the Core accepts for the radio (stationRadios' models), on
// column 2; empty when the Core did not send them.
constexpr int kModelsRole = Qt::UserRole + 2;

// An ISO 8601 time from the Core, in this computer's words.
QString when(const QString& iso)
{
    const QDateTime parsed = QDateTime::fromString(iso, Qt::ISODate);
    return parsed.isValid() ? QLocale().toString(parsed.toLocalTime(), QLocale::ShortFormat)
                            : QObject::tr("Never");
}

} // namespace

ThisCorePage::ThisCorePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("This Core"), model, parent)
    , m_radioModel(model)
{
    QGroupBox* section = addSection(tr("Change radio"));
    auto* layout = qobject_cast<QVBoxLayout*>(section->layout());
    if (layout == nullptr) {
        layout = new QVBoxLayout(section);
    }

    auto* intro = new QLabel(
        tr("The radios this Core can see on its network. The Core runs one at a time; "
           "changing it restarts the Core's connection to its radio, and every window "
           "and device reconnects."),
        section);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QTreeWidget(section);
    m_list->setObjectName(QStringLiteral("thisCoreRadioList"));
    m_list->setRootIsDecorated(false);
    m_list->setColumnCount(5);
    m_list->setHeaderLabels({tr("Radio"), tr("Model"), tr("Address"), tr("MAC"), tr("")});
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->setMinimumHeight(140);
    layout->addWidget(m_list);

    auto* buttons = new QHBoxLayout;
    m_useButton = new QPushButton(tr("Use this radio"), section);
    m_useButton->setObjectName(QStringLiteral("thisCoreUseRadio"));
    m_scanButton = new QPushButton(tr("Scan again"), section);
    m_scanButton->setObjectName(QStringLiteral("thisCoreScan"));
    m_forgetButton = new QPushButton(tr("Forget radio"), section);
    m_forgetButton->setObjectName(QStringLiteral("thisCoreForget"));
    buttons->addWidget(m_useButton);
    buttons->addWidget(m_scanButton);
    buttons->addStretch(1);
    buttons->addWidget(m_forgetButton);
    layout->addLayout(buttons);

    // Edit radio: the model the Core runs the selected radio as, the local
    // Connection panel's model override, applied at its next connect.
    auto* modelRow = new QHBoxLayout;
    auto* modelLabel = new QLabel(tr("Model:"), section);
    m_modelCombo = new QComboBox(section);
    m_modelCombo->setObjectName(QStringLiteral("thisCoreModel"));
    modelRow->addWidget(modelLabel);
    modelRow->addWidget(m_modelCombo, 1);
    layout->addLayout(modelRow);

    m_status = new QLabel(section);
    m_status->setObjectName(QStringLiteral("thisCoreStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    connect(m_list, &QTreeWidget::currentItemChanged, this, [this]() { refreshControls(); });
    connect(m_useButton, &QPushButton::clicked, this, [this]() {
        send("station.selectRadio", selectedMac());
    });
    connect(m_scanButton, &QPushButton::clicked, this, [this]() {
        send("station.rescanRadios", QString());
    });
    connect(m_forgetButton, &QPushButton::clicked, this, [this]() {
        send("station.forgetRadio", selectedMac());
    });
    connect(m_modelCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_fillingModels || index < 0) {
            return;
        }
        send("station.setRadioModel", selectedMac(), m_modelCombo->itemData(index).toInt());
    });

    // iPhone app plan Task 78 (R-IOS-07; the several-devices design, section
    // 12 item 8): who is connected to the Core now, then the paired
    // devices, from the Core's connectedDevices and devices objects.
    QGroupBox* connectedSection = addSection(tr("Connected now"));
    auto* connectedLayout = qobject_cast<QVBoxLayout*>(connectedSection->layout());
    if (connectedLayout == nullptr) {
        connectedLayout = new QVBoxLayout(connectedSection);
    }
    m_connectedList = new ConnectedDevicesList(connectedSection);
    m_connectedList->setDevices(m_radioModel ? m_radioModel->stationDevices() : nullptr);
    connectedLayout->addWidget(m_connectedList);

    // iPhone app plan Task 25 (the chosen Option A: This Core holds the
    // devices list): the Core's paired devices, each with Revoke, and Add
    // a device, which opens the Core's pairing window and shows its code.
    QGroupBox* pairedSection = addSection(tr("Paired devices"));
    auto* pairedLayout = qobject_cast<QVBoxLayout*>(pairedSection->layout());
    if (pairedLayout == nullptr) {
        pairedLayout = new QVBoxLayout(pairedSection);
    }
    m_pairedRows = new QWidget(pairedSection);
    m_pairedRows->setObjectName(QStringLiteral("thisCorePairedRows"));
    m_pairedLayout = new QVBoxLayout(m_pairedRows);
    m_pairedLayout->setContentsMargins(0, 0, 0, 0);
    pairedLayout->addWidget(m_pairedRows);
    m_pairingCode = new QLabel(pairedSection);
    m_pairingCode->setObjectName(QStringLiteral("thisCorePairingCode"));
    m_pairingCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pairingCode->setTextFormat(Qt::PlainText);
    pairedLayout->addWidget(m_pairingCode);
    m_pairingInstruction = new QLabel(
        tr("Enter this code on your device to pair it with this Core."), pairedSection);
    m_pairingInstruction->setWordWrap(true);
    pairedLayout->addWidget(m_pairingInstruction);
    m_addDevice = new QPushButton(tr("Add a device"), pairedSection);
    m_addDevice->setObjectName(QStringLiteral("thisCoreAddDevice"));
    pairedLayout->addWidget(m_addDevice, 0, Qt::AlignLeft);

    QGroupBox* identitySection = addSection(tr("Core identity"));
    auto* identityLayout = qobject_cast<QVBoxLayout*>(identitySection->layout());
    if (identityLayout == nullptr) {
        identityLayout = new QVBoxLayout(identitySection);
    }
    m_coreName = new QLabel(identitySection);
    m_coreName->setObjectName(QStringLiteral("thisCoreName"));
    m_coreName->setTextFormat(Qt::PlainText);
    m_coreName->setWordWrap(true);
    identityLayout->addWidget(m_coreName);
    m_keyBackup = new QLabel(identitySection);
    m_keyBackup->setObjectName(QStringLiteral("thisCoreKeyBackup"));
    m_keyBackup->setTextFormat(Qt::PlainText);
    m_keyBackup->setWordWrap(true);
    m_keyBackup->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addWidget(m_keyBackup);
    m_keyBackupDone = new QPushButton(tr("I've backed it up"), identitySection);
    m_keyBackupDone->setObjectName(QStringLiteral("thisCoreKeyBackupDone"));
    identityLayout->addWidget(m_keyBackupDone, 0, Qt::AlignLeft);
    m_devicesStatus = new QLabel(identitySection);
    m_devicesStatus->setObjectName(QStringLiteral("thisCoreDevicesStatus"));
    m_devicesStatus->setWordWrap(true);
    identityLayout->addWidget(m_devicesStatus);

    connect(m_addDevice, &QPushButton::clicked, this,
            [this]() { sendDeviceAdmin("pairing.open"); });
    connect(m_keyBackupDone, &QPushButton::clicked, this,
            [this]() { sendDeviceAdmin("station.acknowledgeKeyBackup"); });
    if (RemoteDevicesState* devices = m_radioModel ? m_radioModel->stationDevices() : nullptr) {
        connect(devices, &RemoteDevicesState::pairedDevicesChanged, this,
                &ThisCorePage::rebuildDevices);
        connect(devices, &RemoteDevicesState::coreInfoChanged, this,
                &ThisCorePage::rebuildDevices);
        connect(devices, &RemoteDevicesState::connectedDevicesChanged, this,
                &ThisCorePage::rebuildDevices);
    }
    if (m_radioModel != nullptr) {
        connect(m_radioModel, &RadioModel::stationCommandFinished, this,
                [this](quint32 commandId, bool accepted, const QString& reason) {
            if (commandId == 0 || commandId != m_devicesCommandId) {
                return;
            }
            m_devicesCommandId = 0;
            m_devicesStatus->setText(accepted ? QString() : reason);
        });
        connect(m_radioModel, &RadioModel::stationLinkStateChanged, this,
                &ThisCorePage::rebuildDevices);
    }

    if (m_radioModel != nullptr) {
        connect(m_radioModel, &RadioModel::stationRadiosChanged, this,
                &ThisCorePage::rebuildList);
        connect(m_radioModel, &RadioModel::stationRadioRefused, this,
                [this](const QString& reason) {
            m_status->setText(reason);
            m_statusIsPage = false;
            rebuildList(); // a refused model change shows the Core's again
        });
        connect(m_radioModel, &RadioModel::stationLinkStateChanged, this,
                &ThisCorePage::refreshControls);
        connect(m_radioModel, &RadioModel::coreOnAirChanged, this,
                &ThisCorePage::refreshControls);
        // Fix wave (M2): the Core's own words for why it waits.
        connect(m_radioModel, &RadioModel::stationRadioWaitingChanged, this,
                &ThisCorePage::refreshControls);
    }
    rebuildList();
    rebuildDevices();
}

void ThisCorePage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationAvailable = available;
    m_stationReason = reason;
    refreshControls();
    rebuildDevices();
}

QString ThisCorePage::devicesUnavailableReason() const
{
    if (!m_stationAvailable) {
        return m_stationReason.isEmpty() ? tr("Connect to the Core to change these.")
                                         : m_stationReason;
    }
    IStationLink* link = m_radioModel != nullptr ? m_radioModel->stationLink() : nullptr;
    if (link == nullptr || !link->stationLinkReady()) {
        return tr("Connect to the Core to change these.");
    }
    if (!link->deviceAdminAvailable()) {
        return link->signedInWithDeviceKey() ? IStationLink::deviceAdminUnavailableReason()
                                             : IStationLink::pairedDeviceAdminReason();
    }
    return {};
}

void ThisCorePage::rebuildDevices()
{
    while (QLayoutItem* item = m_pairedLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    RemoteDevicesState* devices = m_radioModel ? m_radioModel->stationDevices() : nullptr;
    const QString why = devicesUnavailableReason();
    const bool usable = why.isEmpty();
    const RemoteCoreDevicesInfo info = devices ? devices->coreInfo() : RemoteCoreDevicesInfo{};
    const QList<RemotePairedDevice> paired =
        devices ? devices->pairedDevices() : QList<RemotePairedDevice>{};
    const QString selfId = devices ? devices->selfDeviceId() : QString();
    const auto gate = [](QWidget* w, bool enabled, const QString& reason) {
        w->setEnabled(enabled);
        w->setToolTip(enabled ? QString() : reason);
    };
    for (const RemotePairedDevice& device : paired) {
        auto* row = new QWidget(m_pairedRows);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        const bool self = !selfId.isEmpty() && device.id == selfId;
        QString name = device.name;
        if (self) {
            name += tr(" (this window)");
        }
        const QString seen = device.connected ? tr("Connected now") : when(device.lastSeen);
        auto* label = new QLabel(tr("%1\nPaired: %2 · Last seen: %3")
                                     .arg(name, when(device.pairedAt), seen),
                                 row);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        layout->addWidget(label, 1);
        auto* revoke = new QPushButton(tr("Revoke"), row);
        revoke->setObjectName(QStringLiteral("thisCoreRevoke"));
        revoke->setProperty("deviceId", device.id);
        // The Core refuses the last device while no pairing token works,
        // and this window cannot revoke the key it is signed in with.
        const bool last = paired.size() <= 1 && !info.tokenActive;
        const QString reason = !usable ? why
            : self ? tr("This is this computer. Revoke it from another paired device or on "
                        "the Core's computer.")
            : last ? tr("The Core keeps its last paired device.")
                   : QString();
        gate(revoke, reason.isEmpty(), reason);
        connect(revoke, &QPushButton::clicked, this, [this, id = device.id]() {
            sendDeviceAdmin("devices.revoke", id);
        });
        layout->addWidget(revoke);
        m_pairedLayout->addWidget(row);
    }
    if (paired.isEmpty()) {
        m_pairedLayout->addWidget(new QLabel(
            devices && info.received ? tr("No paired devices.") : why.isEmpty()
                ? tr("The Core has not sent its paired devices.") : why,
            m_pairedRows));
    }
    const bool showCode = info.pairingWindowOpen && !info.pairingCode.isEmpty();
    m_pairingCode->setText(tr("Pairing code: %1").arg(info.pairingCode));
    m_pairingCode->setVisible(showCode);
    m_pairingInstruction->setVisible(showCode);
    IStationLink* link = m_radioModel != nullptr ? m_radioModel->stationLink() : nullptr;
    const bool pairing = usable && link != nullptr && link->pairingAvailable();
    gate(m_addDevice, pairing && !showCode,
         !usable ? why : showCode ? tr("The pairing code is shown below the list.")
                                  : IStationLink::deviceAdminUnavailableReason());

    m_coreName->setText(info.stationLabel.isEmpty()
                            ? tr("No Core name")
                            : tr("Core name: %1").arg(info.stationLabel));
    if (info.keyBackupAcknowledged) {
        m_keyBackup->setText(tr("The Core's key is backed up."));
    } else if (!info.keyPath.isEmpty()) {
        m_keyBackup->setText(tr("Back up the Core key file on the Core's computer: %1")
                                 .arg(info.keyPath));
    } else {
        m_keyBackup->setText(tr("Back up this Core's key when it is available."));
    }
    m_keyBackupDone->setVisible(!info.keyBackupAcknowledged);
    gate(m_keyBackupDone, usable && !info.keyPath.isEmpty(),
         !usable ? why : tr("The Core key is not available."));
}

void ThisCorePage::sendDeviceAdmin(const QByteArray& verb, const QString& id)
{
    IStationLink* link = m_radioModel != nullptr ? m_radioModel->stationLink() : nullptr;
    if (link == nullptr) {
        m_devicesStatus->setText(tr("Connect to the Core to change these."));
        return;
    }
    const IStationLink::CommandOutcome outcome = link->requestDeviceAdmin(verb, id);
    if (!outcome.sent) {
        m_devicesCommandId = 0;
        m_devicesStatus->setText(outcome.reason);
        return;
    }
    m_devicesCommandId = outcome.commandId;
    m_devicesStatus->clear();
}

QString ThisCorePage::reconnectToChangeRadioReason()
{
    return tr("Reconnect this window to change the Core's radio.");
}

QString ThisCorePage::modelListUnavailableReason()
{
    return tr("Update the Core to change this radio's model from here.");
}

QString ThisCorePage::unavailableReason() const
{
    if (!m_stationAvailable) {
        return m_stationReason.isEmpty() ? tr("Connect to the Core to change these.")
                                         : m_stationReason;
    }
    IStationLink* link = m_radioModel != nullptr ? m_radioModel->stationLink() : nullptr;
    if (link == nullptr || !link->stationLinkReady()) {
        return tr("Connect to the Core to change these.");
    }
    if (!link->stationRadiosAvailable()) {
        return IStationLink::stationRadiosUnavailableReason();
    }
    // Fix wave (I5): the Core takes these only from a paired device.
    if (!link->signedInWithDeviceKey()) {
        // Follow-up N1: a sign-in that enrolled this computer's key is from
        // a paired device already; its next sign-in is by key.
        return link->enrolledDeviceKeyThisSession() ? reconnectToChangeRadioReason()
                                                    : StationRadios::pairedDeviceReason();
    }
    if (m_radioModel->isCoreOnAir()) {
        return RadioModel::onAirReason();
    }
    return {};
}

void ThisCorePage::selectCoreRadio()
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_list->topLevelItem(i);
        if (!item->text(4).isEmpty()) {
            m_list->setCurrentItem(item);
            return;
        }
    }
}

void ThisCorePage::rebuildList()
{
    const QString keep = selectedMac();
    m_list->clear();
    const QList<StationRadioEntry> radios =
        m_radioModel != nullptr ? m_radioModel->stationRadios() : QList<StationRadioEntry>{};
    QTreeWidgetItem* current = nullptr;
    for (const StationRadioEntry& radio : radios) {
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, radio.name);
        item->setText(1, QString::fromLatin1(displayName(static_cast<HPSDRModel>(radio.model))));
        item->setText(2, radio.address);
        item->setText(3, radio.mac);
        item->setText(4, radio.inUse ? tr("The Core's radio") : QString());
        item->setData(0, kMacRole, radio.mac);
        item->setData(1, kMacRole, radio.model);
        QVariantList models;
        for (int m : radio.models) {
            models.append(m);
        }
        item->setData(2, kModelsRole, models);
        if (radio.mac == keep || (keep.isEmpty() && radio.inUse)) {
            current = item;
        }
    }
    if (current == nullptr && m_list->topLevelItemCount() > 0) {
        current = m_list->topLevelItem(0);
    }
    if (current != nullptr) {
        m_list->setCurrentItem(current);
    }
    for (int c = 1; c < m_list->columnCount(); ++c) {
        m_list->resizeColumnToContents(c);
    }
    refreshControls();
}

void ThisCorePage::refreshControls()
{
    const QString why = unavailableReason();
    const bool usable = why.isEmpty();
    const bool picked = !selectedMac().isEmpty();
    const bool coresRadio = selectedIsCoresRadio();

    // The model choice follows the selected radio: the models the Core
    // accepts for its board (station.setRadioModel's check), as the Core
    // sent them. The model alone does not name the board (Red Pitaya runs on
    // a Hermes or an Orion MkII board), so without the Core's list the
    // choice shows the model the Core runs it as and waits.
    m_fillingModels = true;
    m_modelCombo->clear();
    bool haveModelList = false;
    if (QTreeWidgetItem* item = m_list->currentItem()) {
        const int model = item->data(1, kMacRole).toInt();
        const QVariantList models = item->data(2, kModelsRole).toList();
        haveModelList = !models.isEmpty();
        if (haveModelList) {
            for (const QVariant& candidate : models) {
                m_modelCombo->addItem(
                    QString::fromLatin1(displayName(static_cast<HPSDRModel>(candidate.toInt()))),
                    candidate.toInt());
            }
        } else {
            m_modelCombo->addItem(QString::fromLatin1(displayName(static_cast<HPSDRModel>(model))),
                                  model);
        }
        m_modelCombo->setCurrentIndex(m_modelCombo->findData(model));
    }
    m_fillingModels = false;

    const auto gate = [](QWidget* w, bool enabled, const QString& reason,
                         const QString& tip) {
        w->setEnabled(enabled);
        w->setToolTip(enabled ? tip : reason);
    };
    const QString noRadio = tr("Choose a radio in the list first.");
    gate(m_scanButton, usable, why, tr("Look for radios on the Core's network again."));
    gate(m_useButton, usable && picked && !coresRadio,
         !usable ? why : !picked ? noRadio : tr("The Core is already using this radio."),
         tr("Make this the Core's radio."));
    gate(m_modelCombo, usable && picked && haveModelList,
         !usable ? why : !picked ? noRadio : modelListUnavailableReason(),
         tr("The model the Core runs this radio as, from its next connect."));
    gate(m_forgetButton, usable && picked && !coresRadio,
         !usable ? why : !picked ? noRadio : StationRadios::inUseReason(),
         tr("Remove this radio and its saved model from the Core."));
    gate(m_list, usable, why, QString());

    // The page's own line (why it cannot be used, or what the Core is
    // doing) until a request's answer replaces it.
    bool hasCore = false;
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        hasCore = hasCore || !m_list->topLevelItem(i)->text(4).isEmpty();
    }
    if (!usable) {
        m_status->setText(why);
        m_statusIsPage = true;
    } else if (m_statusIsPage || m_status->text().isEmpty()) {
        // Fix wave (M2): the Core says why it waits (for a choice, for its
        // chosen radio to appear, for a radio another program holds); the
        // page's own words stand in for a Core that does not.
        const QString waiting =
            m_radioModel != nullptr ? m_radioModel->stationRadioWaiting() : QString();
        const QString line = !hasCore && !waiting.isEmpty() ? waiting
            : m_list->topLevelItemCount() == 0
                ? tr("The Core has not found a radio. Scan again.")
            : !hasCore ? tr("The Core is waiting for you to choose its radio.")
                       : QString();
        m_status->setText(line);
        m_statusIsPage = !line.isEmpty();
    }
}

QString ThisCorePage::selectedMac() const
{
    const QTreeWidgetItem* item = m_list != nullptr ? m_list->currentItem() : nullptr;
    return item != nullptr ? item->data(0, kMacRole).toString() : QString();
}

bool ThisCorePage::selectedIsCoresRadio() const
{
    const QTreeWidgetItem* item = m_list != nullptr ? m_list->currentItem() : nullptr;
    return item != nullptr && !item->text(4).isEmpty();
}

void ThisCorePage::send(const QByteArray& verb, const QString& mac, int model)
{
    IStationLink* link = m_radioModel != nullptr ? m_radioModel->stationLink() : nullptr;
    if (link == nullptr) {
        m_status->setText(tr("Connect to the Core to change these."));
        return;
    }
    m_status->clear();
    m_statusIsPage = false;
    const IStationLink::CommandOutcome outcome = link->requestStationRadio(verb, mac, model);
    if (!outcome.sent) {
        m_status->setText(outcome.reason);
    } else if (verb == "station.selectRadio") {
        m_status->setText(tr("The Core is changing its radio. This window reconnects when "
                             "it is ready."));
    } else if (verb == "station.rescanRadios") {
        m_status->setText(tr("The Core is looking for radios."));
    }
}

} // namespace NereusSDR
