#include "TuneStepButtonItem.h"

namespace NereusSDR {

// From Thetis TuneStepList with "Hz" stripped (MeterManager.cs:7999+)
static const char* const kStepLabels[] = {
    "1", "10", "100", "1k", "10k", "100k", "1M"
};

TuneStepButtonItem::TuneStepButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kStepCount);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kStepCount; ++i) {
        setupButton(i, QString::fromLatin1(kStepLabels[i]));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked, this, &TuneStepButtonItem::onButtonClicked);
}

void TuneStepButtonItem::setActiveStep(int index)
{
    if (m_activeStep == index) { return; }
    if (m_activeStep >= 0 && m_activeStep < buttonCount()) { button(m_activeStep).on = false; }
    m_activeStep = index;
    if (m_activeStep >= 0 && m_activeStep < buttonCount()) { button(m_activeStep).on = true; }
}

void TuneStepButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton) { emit tuneStepSelected(index); }
}

QString TuneStepButtonItem::serialize() const
{
    return QStringLiteral("TUNESTEPBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeStep).arg(visibleBits());
}

bool TuneStepButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("TUNESTEPBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveStep(parts[8].toInt()); }
    if (parts.size() > 9) { setVisibleBits(parts[9].toUInt()); }
    return true;
}

} // namespace NereusSDR
