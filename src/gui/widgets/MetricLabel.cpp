#include "MetricLabel.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>

namespace NereusSDR {

MetricLabel::MetricLabel(const QString& label, const QString& initialValue,
                         QWidget* parent)
    : QWidget(parent), m_label(label), m_value(initialValue)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(4, 0, 4, 0);
    hbox->setSpacing(3);

    m_labelPart = new QLabel(label, this);
    m_labelPart->setObjectName(QStringLiteral("MetricLabel_Label"));
    hbox->addWidget(m_labelPart);

    m_valuePart = new QLabel(initialValue, this);
    m_valuePart->setObjectName(QStringLiteral("MetricLabel_Value"));
    hbox->addWidget(m_valuePart);

    applyStyle();
}

void MetricLabel::setLabel(const QString& l)
{
    if (m_label == l) { return; }
    m_label = l;
    m_labelPart->setText(l);
}

void MetricLabel::setValue(const QString& v)
{
    if (m_value == v) { return; }
    m_value = v;
    m_valuePart->setText(v);
}

void MetricLabel::setWarning(bool warning)
{
    if (m_warning == warning) { return; }
    m_warning = warning;
    // Parity ruling C9: the value alone takes the warning colour, set on
    // the value label itself so it repaints at once.
    m_valuePart->setStyleSheet(warning
        ? QStringLiteral("QLabel#MetricLabel_Value { color: %1; }")
              .arg(QLatin1String(Style::kAmberWarn))
        : QString());
}

QString MetricLabel::valueStyleSheet() const
{
    return m_valuePart->styleSheet();
}

void MetricLabel::applyStyle()
{
    setStyleSheet(QStringLiteral(
        "QLabel#MetricLabel_Label { color: #607080;"
        " font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 9px; font-weight: 500; letter-spacing: 0.5px; }"
        "QLabel#MetricLabel_Value { color: #a0b8c8;"
        " font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 11px; font-weight: 600; }"
    ));
}

} // namespace NereusSDR
