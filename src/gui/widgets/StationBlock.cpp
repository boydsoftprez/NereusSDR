// src/gui/widgets/StationBlock.cpp
#include "StationBlock.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace NereusSDR {

StationBlock::StationBlock(QWidget* parent) : QWidget(parent)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(10, 2, 10, 2);
    hbox->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("StationBlock_Label"));
    hbox->addWidget(m_label);

    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);  // QSS background paint on QWidget subclass

    // Minimum width: tightened from 180 → 140 after live-test revealed
    // layout overflow on 1280-px window. 140 px holds "ANAN-G2 (Sat"
    // before Qt elides; longer names show fully when window is wider.
    // The "Click to connect" disconnected placeholder fits comfortably.
    setMinimumWidth(140);

    // Initialise label + style for disconnected appearance up-front so
    // the first paint already has the placeholder text and dashed-red
    // border. m_radioName remains default-constructed (empty) so
    // isConnectedAppearance() correctly returns false.
    m_label->setText(QStringLiteral("Click to connect"));
    applyStyle();
}

void StationBlock::setRadioName(const QString& name)
{
    if (m_radioName == name) { return; }
    m_radioName = name;
    m_label->setText(name.isEmpty() ? QStringLiteral("Click to connect") : name);
    applyStyle();
}

void StationBlock::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    } else if (event->button() == Qt::RightButton) {
        // Only emit contextMenu when in connected appearance — disconnected
        // STATION has nothing to disconnect / edit / forget.
        if (isConnectedAppearance()) {
            emit contextMenuRequested(event->globalPosition().toPoint());
        }
    }
    QWidget::mousePressEvent(event);
}

void StationBlock::applyStyle()
{
    if (isConnectedAppearance()) {
        setStyleSheet(QStringLiteral(
            "NereusSDR--StationBlock { border: 1px solid rgba(0,180,216,80);"
            " background: #0a0a14; border-radius: 3px; }"
            "QLabel { color: #c8d8e8; font-family: 'SF Mono', Menlo, monospace;"
            " font-size: 12px; font-weight: bold; background: transparent; border: none; }"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "NereusSDR--StationBlock { border: 1px dashed rgba(255,96,96,102);"
            " background: #0a0a14; border-radius: 3px; }"
            "QLabel { color: #607080; font-family: 'SF Mono', Menlo, monospace;"
            " font-size: 12px; font-style: italic; background: transparent; border: none; }"
        ));
    }
}

} // namespace NereusSDR
