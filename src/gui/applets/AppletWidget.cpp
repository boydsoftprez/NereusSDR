#include "AppletWidget.h"

namespace NereusSDR {

AppletWidget::AppletWidget(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    // Dark theme base styling — applets inherit this
    setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 11px; }"
        "QComboBox {"
        "  background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #205070; border-radius: 3px;"
        "  padding: 2px 6px; font-size: 10px;"
        "}"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8;"
        "}"
        "QPushButton {"
        "  background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #205070; border-radius: 3px;"
        "  padding: 2px 8px; font-size: 10px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:checked { background: #006040; color: #00ff88; }"
        "QSlider::groove:horizontal { background: #203040; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  background: #00b4d8; width: 10px; height: 10px;"
        "  margin: -3px 0; border-radius: 5px;"
        "}"));
}

QIcon AppletWidget::appletIcon() const
{
    return QIcon();
}

} // namespace NereusSDR
