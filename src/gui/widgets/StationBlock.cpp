// no-port-check: NereusSDR-original compact station presentation.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// src/gui/widgets/StationBlock.cpp
#include "StationBlock.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <algorithm>

namespace NereusSDR {

StationBlock::StationBlock(QWidget* parent) : QWidget(parent)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(10, 2, 10, 2);
    hbox->setSpacing(0);

    auto* vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("StationBlock_Label"));
    // Both rows centre. This label predates the hardware row and had no
    // alignment, so it defaulted to left while the row added beneath it was
    // centred. With the hardware line ("HermesC10 · v110") wider than the
    // name ("ANAN-G2E"), the name visibly hung to the left inside its own
    // box. Bench report, 2026-08-03.
    m_label->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_label);

    m_hardwareLabel = new QLabel(this);
    m_hardwareLabel->setAlignment(Qt::AlignCenter);
    m_hardwareLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 10px; background: transparent; }"));
    m_hardwareLabel->setVisible(false);
    // Presses must reach StationBlock::mousePressEvent, not stop here.
    m_hardwareLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    vbox->addWidget(m_hardwareLabel);

    m_audioLabel = new QLabel(this);
    m_audioLabel->setObjectName(QStringLiteral("StationBlock_AudioPath"));
    m_hardwareLabel->setObjectName(QStringLiteral("StationBlock_ControlPath"));
    m_audioLabel->setAlignment(Qt::AlignCenter);
    m_audioLabel->setStyleSheet(m_hardwareLabel->styleSheet());
    m_audioLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_audioLabel->hide();
    vbox->addWidget(m_audioLabel);
    for (QLabel* label : {m_label, m_hardwareLabel, m_audioLabel}) {
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    hbox->addLayout(vbox);

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
    if (name.isEmpty()) {
        setHardwareLine(QString(), QString());
    }
    m_label->setText(name.isEmpty() ? QStringLiteral("Click to connect") : name);
    applyStyle();
    updateElision();
}

void StationBlock::setHardwareLine(const QString& model, const QString& firmware)
{
    m_corePresentation = false; setMaximumWidth(QWIDGETSIZE_MAX);
    m_audioLine.clear(); m_audioLabel->hide();
    QString line = model;
    if (!model.isEmpty() && !firmware.isEmpty()) {
        line += QStringLiteral(" · ");
    }
    line += firmware;

    if (m_hardwareLine == line) {
        return;
    }
    m_hardwareLine = line;
    m_hardwareLabel->setText(line);
    m_hardwareLabel->setVisible(!line.isEmpty());
    updateElision();
}

void StationBlock::setConnectionLines(const QString& controls, const QString& audio)
{
    m_corePresentation = true; setMaximumWidth(320);
    m_hardwareLine = controls; m_audioLine = audio;
    m_hardwareLabel->setVisible(!controls.isEmpty()); m_audioLabel->setVisible(!audio.isEmpty());
    updateElision(); updateGeometry();
}
QSize StationBlock::sizeHint() const
{
    const int textWidth = std::max({m_label->fontMetrics().horizontalAdvance(m_radioName),
        m_hardwareLabel->fontMetrics().horizontalAdvance(m_hardwareLine),
        m_audioLabel->fontMetrics().horizontalAdvance(m_audioLine)});
    return {m_corePresentation ? std::clamp(textWidth + 20, 140, 320) : std::max(textWidth + 20, 140),
            QWidget::sizeHint().height()};
}
void StationBlock::updateElision()
{
    const int available = std::max(0, width() - 20);
    const auto set = [available](QLabel* label, const QString& text) {
        label->setText(label->fontMetrics().elidedText(text, Qt::ElideRight, available));
        label->setToolTip(text);
    };
    set(m_label, m_radioName.isEmpty() ? QStringLiteral("Click to connect") : m_radioName);
    set(m_hardwareLabel, m_hardwareLine); set(m_audioLabel, m_audioLine);
}
void StationBlock::resizeEvent(QResizeEvent* event)
{ QWidget::resizeEvent(event); updateElision(); }

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
