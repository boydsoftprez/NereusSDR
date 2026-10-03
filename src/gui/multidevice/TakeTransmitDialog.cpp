// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/TakeTransmitDialog.cpp  (NereusSDR)
// =================================================================
//
// See TakeTransmitDialog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TX badge take: shortNameOf. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/TakeTransmitDialog.h"

#include "core/session/TransmitStateFacade.h"
#include "gui/multidevice/DeviceWords.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

QString shortOf(const TakeTransmitDialog::Holder& h)
{
    if (!h.shortName.isEmpty()) { return h.shortName; }
    if (!h.name.isEmpty()) { return h.name; }
    return QStringLiteral("another device");
}

QString nameOf(const TakeTransmitDialog::Holder& h)
{
    return h.name.isEmpty() ? shortOf(h) : h.name;
}

} // namespace

QString TakeTransmitDialog::shortNameOf(const Holder& holder)
{
    return shortOf(holder);
}

TakeTransmitDialog::Holder TakeTransmitDialog::fromTransmitState(const TransmitState& tx)
{
    Holder h;
    const bool radio = tx.holderSource() == QStringLiteral("radioPtt");
    h.name = radio ? QStringLiteral("Radio") : tx.holderName();
    h.shortName = radio ? QStringLiteral("Radio") : tx.holderShortName();
    h.onAir = tx.keyed();
    h.away = tx.holderAway();
    return h;
}

TakeTransmitDialog::Holder TakeTransmitDialog::fromHolderEntry(const QJsonObject& holder)
{
    Holder h;
    h.name = holder.value(QStringLiteral("name")).toString();
    h.shortName = holder.value(QStringLiteral("shortName")).toString();
    const QString state = holder.value(QStringLiteral("state")).toString();
    h.onAir = holder.value(QStringLiteral("keyed")).toBool()
        || state == QStringLiteral("transmitting");
    h.away = state == QStringLiteral("away");
    h.awayForSeconds = holder.value(QStringLiteral("awayForSeconds")).toInteger();
    return h;
}

QString TakeTransmitDialog::questionText(const Holder& holder)
{
    return QStringLiteral("Take transmit from %1?").arg(shortOf(holder));
}

QString TakeTransmitDialog::detailText(const Holder& holder)
{
    const QString name = nameOf(holder);
    if (holder.onAir) {
        return QStringLiteral("%1 is on the air now. Taking over unkeys it first. "
                              "Press MOX here when you want to transmit.")
            .arg(name);
    }
    if (holder.away) {
        return holder.awayForSeconds > 0
            ? QStringLiteral("%1 is away (for %2). Nothing is on the air.")
                  .arg(name, DeviceWords::duration(holder.awayForSeconds))
            : QStringLiteral("%1 is away. Nothing is on the air.").arg(name);
    }
    return QStringLiteral("%1 has the transmitter and is not on the air. Press MOX here "
                          "when you want to transmit.")
        .arg(name);
}

QString TakeTransmitDialog::takeButtonText(bool onAir)
{
    return onAir ? QStringLiteral("Unkey and take over") : QStringLiteral("Take transmit");
}

TakeTransmitDialog::TakeTransmitDialog(const Holder& holder, QWidget* parent)
    : QDialog(parent)
    , m_red(holder.onAir)
{
    setObjectName(QStringLiteral("TakeTransmitDialog"));
    setWindowTitle(QStringLiteral("Take transmit"));
    auto* layout = new QVBoxLayout(this);
    m_question = new QLabel(questionText(holder), this);
    m_question->setObjectName(QStringLiteral("takeTransmitQuestion"));
    QFont bold = m_question->font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.15);
    m_question->setFont(bold);
    layout->addWidget(m_question);
    m_detail = new QLabel(detailText(holder), this);
    m_detail->setWordWrap(true);
    layout->addWidget(m_detail);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("takeTransmitCancel"));
    m_take = new QPushButton(takeButtonText(holder.onAir), this);
    m_take->setObjectName(QStringLiteral("takeTransmitConfirm"));
    m_take->setDefault(!holder.onAir);
    m_cancel->setDefault(holder.onAir);
    if (holder.onAir) {
        m_take->setStyleSheet(QStringLiteral(
            "QPushButton { background: #b02020; color: white; border: 1px solid #ff4444;"
            " border-radius: 3px; padding: 4px 12px; font-weight: bold; }"));
    }
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_take);
    layout->addLayout(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_take, &QPushButton::clicked, this, &QDialog::accept);
    setMinimumWidth(380);
}

} // namespace NereusSDR
