#include "DataOutItemEditor.h"
#include "../../meters/DataOutItem.h"

#include <QComboBox>
#include <QLineEdit>

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
    addRow(QStringLiteral("Transport"), m_comboTransport);
    connect(m_comboTransport, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setTransportMode(static_cast<DataOutItem::TransportMode>(
            m_comboTransport->currentData().toInt()));
        notifyChanged();
    });

    addHeader(QStringLiteral("MMIO binding"));

    m_editGuid = new QLineEdit(this);
    m_editGuid->setStyleSheet(kLineStyle);
    m_editGuid->setPlaceholderText(QStringLiteral("MMIO GUID"));
    addRow(QStringLiteral("GUID"), m_editGuid);
    connect(m_editGuid, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setMmioGuid(m_editGuid->text());
        notifyChanged();
    });

    m_editVariable = new QLineEdit(this);
    m_editVariable->setStyleSheet(kLineStyle);
    m_editVariable->setPlaceholderText(QStringLiteral("Variable name"));
    addRow(QStringLiteral("Variable"), m_editVariable);
    connect(m_editVariable, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        DataOutItem* d = qobject_cast<DataOutItem*>(m_item);
        if (!d) { return; }
        d->setMmioVariable(m_editVariable->text());
        notifyChanged();
    });
}

} // namespace NereusSDR
