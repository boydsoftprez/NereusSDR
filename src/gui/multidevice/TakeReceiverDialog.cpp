// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/TakeReceiverDialog.cpp  (NereusSDR)
// =================================================================
//
// See TakeReceiverDialog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/TakeReceiverDialog.h"

#include "gui/multidevice/DeviceWords.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

constexpr int kChoiceRole = Qt::UserRole + 1;

QString deviceNames(const QJsonArray& devices)
{
    QStringList names;
    for (const QJsonValue& v : devices) {
        const QJsonObject d = v.toObject();
        QString name = d.value(QStringLiteral("name")).toString();
        if (d.value(QStringLiteral("state")).toString() == QStringLiteral("away")) {
            name += QStringLiteral(" (away)");
        }
        names << name;
    }
    return names.join(QStringLiteral(", "));
}

} // namespace

QString TakeReceiverDialog::choiceText(const QString& kind, const QJsonObject& choice)
{
    QString text;
    if (kind == QStringLiteral("takeSlice")) {
        if (choice.value(QStringLiteral("sliceId")).toInt(-1) < 0) {
            text = QStringLiteral("A free slice");
        } else {
            text = DeviceWords::sliceLine(choice);
            QString who = choice.value(QStringLiteral("deviceName")).toString();
            if (choice.value(QStringLiteral("state")).toString() == QStringLiteral("away")) {
                who += QStringLiteral(" (away)");
            }
            if (!who.isEmpty()) {
                text += QStringLiteral(", ") + who;
            }
            const QString rx =
                DeviceWords::receiver(choice.value(QStringLiteral("streamIndex")).toInt(-1));
            if (!rx.isEmpty()) {
                text += QStringLiteral(" (") + rx + QLatin1Char(')');
            }
            if (choice.value(QStringLiteral("txSlice")).toBool()) {
                text += QStringLiteral(" TX");
            }
        }
    } else {
        text = DeviceWords::receiver(choice.value(QStringLiteral("streamIndex")).toInt(-1));
        const QJsonArray slices = choice.value(QStringLiteral("slices")).toArray();
        if (slices.isEmpty()) {
            text += QStringLiteral(": free");
        } else {
            const QString who = deviceNames(choice.value(QStringLiteral("devices")).toArray());
            if (!who.isEmpty()) {
                text += QStringLiteral(": ") + who;
            }
            for (const QJsonValue& v : slices) {
                const QJsonObject s = v.toObject();
                QString line = DeviceWords::sliceLine(s);
                const QString owner = s.value(QStringLiteral("deviceName")).toString();
                if (!owner.isEmpty()) {
                    line += QStringLiteral(", ") + owner;
                }
                if (s.value(QStringLiteral("txSlice")).toBool()) {
                    line += QStringLiteral(" TX");
                }
                text += QStringLiteral("\n    ") + line;
            }
        }
    }
    if (choice.contains(QStringLiteral("takeable"))
        && !choice.value(QStringLiteral("takeable")).toBool()) {
        const QString why = choice.value(QStringLiteral("why")).toString();
        text += QStringLiteral("\n    Cannot be taken: ")
            + (why.isEmpty() ? QStringLiteral("the Core did not say why.") : why);
    }
    return text;
}

TakeReceiverDialog::TakeReceiverDialog(const SessionPrompt& prompt, QWidget* parent)
    : QDialog(parent)
    , m_id(prompt.id)
{
    setObjectName(QStringLiteral("TakeReceiverDialog"));
    const bool slices = prompt.kind == QStringLiteral("takeSlice");
    setWindowTitle(slices ? QStringLiteral("Take a slice") : QStringLiteral("Take a receiver"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        slices ? QStringLiteral("Every slice the radio has is in use. Choose one to take. It "
                                "closes, and its device is told.")
               : QStringLiteral("The radio's receivers are all in use. Choose one to take. "
                                "Other devices' slices on it close, and each device is told."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("takeReceiverChoices"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    int first = -1;
    const QJsonArray choices = prompt.choices.value_or(QJsonArray{});
    for (const QJsonValue& v : choices) {
        const QJsonObject choice = v.toObject();
        auto* item = new QListWidgetItem(choiceText(prompt.kind, choice), m_list);
        item->setData(kChoiceRole, choice.value(QStringLiteral("choice")).toInteger(-1));
        const bool takeable = !choice.contains(QStringLiteral("takeable"))
            || choice.value(QStringLiteral("takeable")).toBool();
        if (!takeable) {
            item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
        } else if (first < 0) {
            first = m_list->count() - 1;
        }
    }
    layout->addWidget(m_list);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("takeReceiverCancel"));
    m_take = new QPushButton(slices ? QStringLiteral("Take slice")
                                    : QStringLiteral("Take receiver"),
                             this);
    m_take->setObjectName(QStringLiteral("takeReceiverConfirm"));
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_take);
    layout->addLayout(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_take, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() { refreshTakeButton(); });
    if (first >= 0) {
        m_list->setCurrentRow(first);
    }
    refreshTakeButton();
    setMinimumWidth(460);
}

qint64 TakeReceiverDialog::pickedChoice() const
{
    const QListWidgetItem* item = m_list->currentItem();
    if (item == nullptr || !(item->flags() & Qt::ItemIsEnabled)) {
        return -1;
    }
    return item->data(kChoiceRole).toLongLong();
}

void TakeReceiverDialog::refreshTakeButton()
{
    const bool picked = pickedChoice() >= 0;
    m_take->setEnabled(picked);
    m_take->setToolTip(picked ? QString() : QStringLiteral("Choose a receiver to take first."));
}

} // namespace NereusSDR
