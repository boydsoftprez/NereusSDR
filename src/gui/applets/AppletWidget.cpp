#include "AppletWidget.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"

#include <QPushButton>
#include <QSlider>
#include <QFrame>
#include <QHBoxLayout>

namespace NereusSDR {

AppletWidget::AppletWidget(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(Style::kTextPrimary)
    + Style::buttonBaseStyle()
    + Style::sliderHStyle());
}

QIcon AppletWidget::appletIcon() const { return {}; }

QWidget* AppletWidget::appletTitleBar(const QString& text)
{
    auto* bar = new QWidget(this);
    bar->setFixedHeight(Style::kTitleBarH);
    bar->setStyleSheet(Style::titleBarStyle());

    auto* hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(2, 0, 4, 0);
    hbox->setSpacing(4);

    auto* grip = new QLabel(QStringLiteral("\u22EE\u22EE"), bar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; background: transparent; }"
    ).arg(Style::kTextScale));
    hbox->addWidget(grip);

    auto* label = new QLabel(text, bar);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    hbox->addWidget(label);
    hbox->addStretch();

    return bar;
}

QHBoxLayout* AppletWidget::sliderRow(const QString& labelText,
                                      QSlider* slider, QLabel* valueLabel,
                                      int labelWidth)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(labelText, this);
    lbl->setFixedWidth(labelWidth);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
    row->addWidget(lbl);

    slider->setFixedHeight(18);
    row->addWidget(slider, 1);

    if (valueLabel) {
        valueLabel->setFixedWidth(36);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(valueLabel);
    }

    return row;
}

QPushButton* AppletWidget::styledButton(const QString& text, int w, int h)
{
    auto* btn = new QPushButton(text, this);
    btn->setCheckable(true);
    btn->setFixedHeight(h);
    if (w > 0) { btn->setFixedWidth(w); }
    return btn;
}

QPushButton* AppletWidget::greenToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::greenCheckedStyle());
    return btn;
}

QPushButton* AppletWidget::blueToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::blueCheckedStyle());
    return btn;
}

QPushButton* AppletWidget::amberToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::amberCheckedStyle());
    return btn;
}

QLabel* AppletWidget::insetValue(const QString& text, int w)
{
    auto* lbl = new QLabel(text, this);
    lbl->setFixedWidth(w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(Style::insetValueStyle());
    return lbl;
}

QFrame* AppletWidget::divider()
{
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(2);
    line->setStyleSheet(QStringLiteral("QFrame { color: %1; }").arg(Style::kInsetBorder));
    return line;
}

} // namespace NereusSDR
