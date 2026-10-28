// Modification history (NereusSDR):
//   2026-10-26 - The 4-char ID row reads "Data source 4-char ID" and its
//                 tooltip names the meter data source, in the words the meter
//                 data source windows use. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - "MMIO 4-char ID" picks the endpoint data out sends to;
//                 "Send interval (ms)" 100-5000 in steps of 10. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "DataOutItemEditor.h"
#include "../../meters/DataOutItem.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

namespace NereusSDR {

namespace {
constexpr const char* kComboStyle =
    "QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}";

constexpr const char* kLineStyle =
    "QLineEdit { background: #0a0a18; color: #c8d8e8;"
    " border: 1px solid #1e2e3e; border-radius: 3px;"
    " padding: 1px 4px; min-height: 18px; }";
} // namespace

DataOutItemEditor::DataOutItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void DataOutItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    DataOutItem* d = qobject_cast<DataOutItem*>(item);
    if (!d) { return; }

    beginProgrammaticUpdate();
    m_comboFormat->setCurrentIndex(
        m_comboFormat->findData(static_cast<int>(d->outputFormat())));
    m_comboTransport->setCurrentIndex(
        m_comboTransport->findData(static_cast<int>(d->transportMode())));
    m_editGuid->setText(d->mmioGuid());
    m_editVariable->setText(d->mmioVariable());
    const MmioEndpoint* ep = d->endpointGuid().isNull()
        ? nullptr : ExternalVariableEngine::instance().endpoint(d->endpointGuid());
    m_editFourChar->setText(ep ? ep->fourChar() : QString());
    m_spinSendInterval->setValue(d->sendIntervalMs());
    endProgrammaticUpdate();
}

void DataOutItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Data Output"));

    m_comboFormat = new QComboBox(this);
    m_comboFormat->setStyleSheet(kComboStyle);
    m_comboFormat->addItem(QStringLiteral("JSON"),
        static_cast<int>(DataOutItem::OutputFormat::Json));
    m_comboFormat->addItem(QStringLiteral("XML"),
        static_cast<int>(DataOutItem::OutputFormat::Xml));
    m_comboFormat->addItem(QStringLiteral("Raw"),
        static_cast<int>(DataOutItem::OutputFormat::Raw));
    addRow(QStringLiteral("Format"), m_comboFormat);
    connect(m_comboFormat, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setOutputFormat(static_cast<DataOutItem::OutputFormat>(
            m_comboFormat->currentData().toInt()));
        notifyChanged();
    });

    m_comboTransport = new QComboBox(this);
    m_comboTransport->setStyleSheet(kComboStyle);
    m_comboTransport->addItem(QStringLiteral("UDP"),
        static_cast<int>(DataOutItem::TransportMode::Udp));
    m_comboTransport->addItem(QStringLiteral("TCP"),
        static_cast<int>(DataOutItem::TransportMode::Tcp));
    m_comboTransport->addItem(QStringLiteral("Serial"),
        static_cast<int>(DataOutItem::TransportMode::Serial));
    m_comboTransport->addItem(QStringLiteral("TCP Client"),
        static_cast<int>(DataOutItem::TransportMode::TcpClient));
    addRow(QStringLiteral("Connection type"), m_comboTransport);
    connect(m_comboTransport, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setTransportMode(static_cast<DataOutItem::TransportMode>(
            m_comboTransport->currentData().toInt()));
        notifyChanged();
    });

    // The endpoint the readings go to, by its 4-char ID (Thetis
    // txtDataOutNode_4charID, MaxLength 4). An ID no endpoint has sends
    // nothing.
    m_editFourChar = new QLineEdit(this);
    m_editFourChar->setObjectName(QStringLiteral("dataOutFourCharId"));
    m_editFourChar->setStyleSheet(kLineStyle);
    m_editFourChar->setMaxLength(4);
    m_editFourChar->setPlaceholderText(QStringLiteral("ID"));
    m_editFourChar->setToolTip(QStringLiteral(
        "The 4-character ID of the meter data source the readings are sent to"));
    addRow(QStringLiteral("Data source 4-char ID"), m_editFourChar);
    connect(m_editFourChar, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        const MmioEndpoint* ep =
            ExternalVariableEngine::instance().endpointByFourChar(m_editFourChar->text().trimmed());
        const QUuid guid = ep ? ep->guid() : QUuid();
        if (guid == d->endpointGuid()) { return; }
        d->setEndpointGuid(guid);
        notifyChanged();
    });

    m_spinSendInterval = makeIntRow(QStringLiteral("Send interval (ms)"),
                                    kDataOutMinMs, kDataOutMaxMs);
    m_spinSendInterval->setObjectName(QStringLiteral("dataOutSendIntervalMs"));
    m_spinSendInterval->setSingleStep(kDataOutStepMs);
    m_spinSendInterval->setValue(kDataOutNewIntervalMs);
    m_spinSendInterval->setToolTip(QStringLiteral("The interval that the data is sent."));
    connect(m_spinSendInterval, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setSendIntervalMs(v);
        notifyChanged();
    });

    addHeader(QStringLiteral("Data source (MMIO)"));

    m_editGuid = new QLineEdit(this);
    m_editGuid->setStyleSheet(kLineStyle);
    m_editGuid->setPlaceholderText(QStringLiteral("Data source ID"));
    addRow(QStringLiteral("Data source ID"), m_editGuid);
    connect(m_editGuid, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setMmioGuid(m_editGuid->text());
        notifyChanged();
    });

    m_editVariable = new QLineEdit(this);
    m_editVariable->setStyleSheet(kLineStyle);
    m_editVariable->setPlaceholderText(QStringLiteral("Value name"));
    addRow(QStringLiteral("Value name"), m_editVariable);
    connect(m_editVariable, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setMmioVariable(m_editVariable->text());
        notifyChanged();
    });
}

} // namespace NereusSDR
