// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ReplaceDeviceDialog.cpp  (NereusSDR)
// =================================================================
//
// See ReplaceDeviceDialog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 item 7 (G-53), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/ReplaceDeviceDialog.h"

#include "gui/multidevice/DeviceWords.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kOnAirRole = Qt::UserRole + 2;

bool onAir(const RemoteConnectedDevice& device)
{
    return device.state == QStringLiteral("transmitting");
}

QString sliceWords(const RemoteDeviceSlice& slice)
{
    QString text = DeviceWords::sliceShort(slice);
    const QString f = DeviceWords::frequency(slice.frequencyHz);
    if (!f.isEmpty()) {
        text += QLatin1Char(' ') + f;
    }
    return text;
}

// "just now", "5 minutes ago".
QString ago(qint64 seconds)
{
    return seconds < 60 ? QStringLiteral("just now")
                        : QStringLiteral("%1 ago").arg(DeviceWords::duration(seconds));
}

} // namespace

QString ReplaceDeviceDialog::replaceButtonText(bool onAir)
{
    return onAir ? QStringLiteral("Unkey and replace") : QStringLiteral("Replace");
}

QString ReplaceDeviceDialog::introText(const RemoteHeldList& held)
{
    QString text;
    if (!held.placeTakenByName.isEmpty()) {
        text = held.placeTakenSecondsAgo
            ? QStringLiteral("%1 took this window's place %2. ")
                  .arg(held.placeTakenByName, ago(*held.placeTakenSecondsAgo))
            : QStringLiteral("%1 took this window's place. ").arg(held.placeTakenByName);
    } else if (held.placeFreedSecondsAgo) {
        text = QStringLiteral("This window's place was freed after 3 minutes away, %1. ")
                   .arg(ago(*held.placeFreedSecondsAgo));
    }
    text += QStringLiteral("The Core already has four devices connected. Choose one for this "
                           "window to replace. That device is disconnected, its slices close, "
                           "and it is told who took its place.");
    return text;
}

QString ReplaceDeviceDialog::entryText(const RemoteHeldEntry& entry)
{
    const RemoteConnectedDevice& d = entry.device;
    QString title = d.name.isEmpty() ? d.shortName : d.name;
    if (!d.shortName.isEmpty() && d.shortName != title) {
        title += QStringLiteral(" (") + d.shortName + QLatin1Char(')');
    }
    QString doing;
    if (d.state == QStringLiteral("away")) {
        doing = QStringLiteral("Away for %1").arg(DeviceWords::duration(d.awayForSeconds));
    } else if (onAir(d)) {
        doing = QStringLiteral("On the air for %1")
                    .arg(DeviceWords::duration(d.transmittingForSeconds));
        if (d.transmittingOn) {
            doing += QStringLiteral(" on slice ") + sliceWords(*d.transmittingOn);
        }
    } else {
        QStringList slices;
        for (const RemoteDeviceSlice& s : d.listeningOn) {
            slices << sliceWords(s);
        }
        doing = slices.isEmpty() ? QStringLiteral("Listening, no slices")
                                 : QStringLiteral("Listening on ") + slices.join(QStringLiteral(", "));
        if (d.holdsTransmit) {
            doing += QStringLiteral(", has transmit");
        }
    }
    const QString last = d.lastActivitySeconds < 60
        ? QStringLiteral("last active just now")
        : QStringLiteral("last active %1 ago").arg(DeviceWords::duration(d.lastActivitySeconds));
    QString text = title + QStringLiteral("\n    ") + doing + QStringLiteral("\n    Connected ")
        + DeviceWords::duration(d.connectedForSeconds) + QStringLiteral(", ") + last;
    if (!entry.replaceable) {
        text += QStringLiteral("\n    Runs the Core, so it cannot be replaced.");
    }
    return text;
}

ReplaceDeviceDialog::ReplaceDeviceDialog(const RemoteHeldList& held, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ReplaceDeviceDialog"));
    setWindowTitle(QStringLiteral("The Core is full"));
    auto* layout = new QVBoxLayout(this);
    m_intro = new QLabel(this);
    m_intro->setObjectName(QStringLiteral("replaceDeviceIntro"));
    m_intro->setWordWrap(true);
    layout->addWidget(m_intro);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("replaceDeviceChoices"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("replaceDeviceCancel"));
    m_replace = new QPushButton(replaceButtonText(false), this);
    m_replace->setObjectName(QStringLiteral("replaceDeviceConfirm"));
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_replace);
    layout->addLayout(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_replace, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() { refreshReplaceButton(); });
    setMinimumWidth(480);
    setHeld(held);
}

void ReplaceDeviceDialog::setHeld(const RemoteHeldList& held)
{
    const QString keep = pickedDeviceId();
    m_intro->setText(introText(held));
    const QSignalBlocker block(m_list);
    m_list->clear();
    int first = -1;
    int wanted = -1;
    const QString want = keep.isEmpty() ? held.preselectId : keep;
    for (const RemoteHeldEntry& entry : held.entries) {
        auto* item = new QListWidgetItem(entryText(entry), m_list);
        item->setData(kIdRole, entry.device.deviceId);
        item->setData(kOnAirRole, onAir(entry.device));
        if (!entry.replaceable) {
            item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
            continue;
        }
        const int row = m_list->count() - 1;
        if (first < 0) {
            first = row;
        }
        if (!want.isEmpty() && entry.device.deviceId == want) {
            wanted = row;
        }
    }
    const int pick = wanted >= 0 ? wanted : first;
    if (pick >= 0) {
        m_list->setCurrentRow(pick);
    }
    refreshReplaceButton();
}

QString ReplaceDeviceDialog::pickedDeviceId() const
{
    if (m_list == nullptr) {
        return {};
    }
    const QListWidgetItem* item = m_list->currentItem();
    if (item == nullptr || !(item->flags() & Qt::ItemIsEnabled)) {
        return {};
    }
    return item->data(kIdRole).toString();
}

bool ReplaceDeviceDialog::pickedIsOnAir() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item != nullptr && (item->flags() & Qt::ItemIsEnabled)
        && item->data(kOnAirRole).toBool();
}

void ReplaceDeviceDialog::refreshReplaceButton()
{
    const bool picked = !pickedDeviceId().isEmpty();
    const bool red = pickedIsOnAir();
    m_replace->setEnabled(picked);
    m_replace->setText(replaceButtonText(red));
    m_replace->setToolTip(picked ? QString()
                                 : QStringLiteral("Choose a device to replace first."));
    // The same red as taking transmit from a device on the air.
    m_replace->setStyleSheet(red ? QStringLiteral(
        "QPushButton { background: #b02020; color: white; border: 1px solid #ff4444;"
        " border-radius: 3px; padding: 4px 12px; font-weight: bold; }") : QString());
    m_replace->setDefault(picked && !red);
    m_cancel->setDefault(red);
}

} // namespace NereusSDR
