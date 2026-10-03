#pragma once

#include <QWidget>
#include <QString>

class QLabel;

namespace NereusSDR {

// MetricLabel — labelled-metric pair widget for the right-side status strip.
// Renders [LABEL] [value] horizontally where LABEL is uppercase 9px and
// value is the actual number. Used for PSU/PA/CPU readouts.
class MetricLabel : public QWidget {
    Q_OBJECT

public:
    MetricLabel(const QString& label, const QString& initialValue,
                QWidget* parent = nullptr);

    void setLabel(const QString& l);
    void setValue(const QString& v);
    /// Parity ruling C9: the value in the warning colour
    /// (Style::kAmberWarn) while on.
    void setWarning(bool warning);
    bool warning() const noexcept { return m_warning; }
    QString valueStyleSheet() const;
    QString label() const noexcept { return m_label; }
    QString value() const noexcept { return m_value; }

private:
    void applyStyle();

    QString  m_label;
    QString  m_value;
    bool     m_warning{false};
    QLabel*  m_labelPart{nullptr};
    QLabel*  m_valuePart{nullptr};
};

} // namespace NereusSDR
