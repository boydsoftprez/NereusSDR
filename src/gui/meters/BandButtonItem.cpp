#include "BandButtonItem.h"

namespace NereusSDR {

// From Thetis clsBandButtonBox (MeterManager.cs:11482+).
// Order matches NereusSDR::Band enum (src/models/Band.h) — keep in sync
// when adding/reordering bands. WWV and XVTR added in Phase 3G-8 commit 2.
static const char* const kBandLabels[] = {
    "160m", "80m", "60m", "40m", "30m", "20m",
    "17m",  "15m", "12m", "10m", "6m",  "GEN",
    "WWV",  "XVTR"
};

BandButtonItem::BandButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kBandCount);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kBandCount; ++i) {
        setupButton(i, QString::fromLatin1(kBandLabels[i]));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked,
            this, &BandButtonItem::onButtonClicked);
}

void BandButtonItem::setActiveBand(int index)
{
    if (m_activeBand == index) { return; }
    if (m_activeBand >= 0 && m_activeBand < buttonCount()) {
        button(m_activeBand).on = false;
    }
    m_activeBand = index;
    if (m_activeBand >= 0 && m_activeBand < buttonCount()) {
        button(m_activeBand).on = true;
    }
}

void BandButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::RightButton) {
        emit bandStackRequested(index);
    } else {
        emit bandClicked(index);
    }
}

QString BandButtonItem::serialize() const
{
    return QStringLiteral("BANDBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeBand).arg(visibleBits());
}

bool BandButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("BANDBTNS")) { return false; }
    m_x = parts[1].toFloat();
    m_y = parts[2].toFloat();
    m_w = parts[3].toFloat();
    m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt();
    m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveBand(parts[8].toInt()); }
    if (parts.size() > 9) { setVisibleBits(parts[9].toUInt()); }
    return true;
}

} // namespace NereusSDR
