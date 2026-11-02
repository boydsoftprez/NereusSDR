// no-port-check: NereusSDR-original PureSignal-shaped copies of the ready-made bars.
#include "PsStyleBarItem.h"
#include "gui/PureSignalViewStyle.h"
#include "gui/styles/WorkspacePreview.h"
#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
namespace NereusSDR {
namespace {
QColor tone(double value, double middle, double high)
{
    if (value > high) { return WorkspacePreview::statusColour(QColor("#ff6060"), "error"); }
    if (middle < high && value > middle) { return WorkspacePreview::statusColour(QColor("#ffd24a"), "warning"); }
    return WorkspacePreview::statusColour(QColor("#20c060"), "active");
}
QString tickText(double tick, bool top)
{
    const double rounded = std::round(tick);
    const QString body = qAbs(tick - rounded) < 0.05 ? QString::number(rounded, 'f', 0) : QString::number(tick, 'f', 1);
    return top ? body + QStringLiteral("+") : body;
}
} // namespace

const PsStyleBarItem::Spec* PsStyleBarItem::specs(int& count)
{
    static constexpr Spec kSpecs[] = {
        {"meter.sMeterNew", "S-Meter New", "SignalAvg", "S-Meter"},
        {"meter.compNew", "Compression New", "Comp", "Compression"},
        {"meter.eqNew", "EQ New", "Eq", "EQ"},
        {"meter.levelerNew", "Leveler New", "Leveler", "Leveler"},
        {"meter.cfcNew", "CFC New", "Cfc", "CFC"},
        {"meter.cfcGainNew", "CFC gain New", "CfcGain", "CFC gain"},
        {"meter.levelerGainNew", "Leveler gain New", "LevelerGain", "Leveler gain"},
        {"meter.alcGainNew", "ALC gain New", "AlcGain", "ALC gain"},
        {"meter.alcGroupNew", "ALC group New", "AlcGroup", "ALC group"},
        {"meter.agcNew", "AGC New", "Agc", "AGC"},
        {"meter.agcGainNew", "AGC gain New", "AgcGain", "AGC gain"},
        {"meter.signalNew", "Signal peak New", "Signal", "Signal peak"},
        {"meter.signalAvgNew", "Signal average New", "SignalAvg", "Signal average"},
        {"meter.signalMaxBinNew", "Signal max bin New", "SignalMaxBin", "Signal max bin"},
        {"meter.adcNew", "ADC New", "Adc", "ADC"},
        {"meter.adcMaxNew", "ADC maximum New", "AdcMax", "ADC maximum"},
        {"meter.pbSnrNew", "PB SNR New", "PbSnr", "PB SNR"},
        {"meter.pwrNew", "Power New", "Pwr", "Power"},
        {"meter.reversePwrNew", "Reverse power New", "ReversePwr", "Reverse power"},
        {"meter.micNew", "Mic New", "Mic", "Mic"},
        {"meter.alcNew", "ALC New", "Alc", "ALC"},
        {"meter.customBarNew", "Custom bar face New", "Custom", "Custom"},
    };
    count = int(sizeof(kSpecs) / sizeof(kSpecs[0]));
    return kSpecs;
}

PsStyleBarItem::PsStyleBarItem(const Spec& spec, QObject* parent)
    : BarPresetItem(parent), m_catalogType(QString::fromLatin1(spec.typeId)), m_caption(QString::fromLatin1(spec.caption))
{
    const QString flavor = QString::fromLatin1(spec.flavor);
    if (flavor == QLatin1String("Mic")) { configureAsMic(); }
    else if (flavor == QLatin1String("Alc")) { configureAsAlc(); }
    else if (flavor == QLatin1String("Custom")) { configureAsCustom(-1, 0, 10, QStringLiteral("Custom")); }
    else { configureVariant(flavor); }
}

std::unique_ptr<MeterItem> PsStyleBarItem::create(const QString& typeId)
{
    int count = 0;
    const Spec* rows = specs(count);
    for (int i = 0; i < count; ++i) {
        if (typeId == QLatin1String(rows[i].typeId)) { return std::unique_ptr<PsStyleBarItem>(new PsStyleBarItem(rows[i])); }
    }
    return {};
}

void PsStyleBarItem::eachSpec(const std::function<void(const QString& typeId, const QString& title)>& visit)
{
    int count = 0;
    const Spec* rows = specs(count);
    for (int i = 0; i < count; ++i) { visit(QString::fromLatin1(rows[i].typeId), QString::fromLatin1(rows[i].title)); }
}

QString PsStyleBarItem::typeId() const { return m_catalogType; }

void PsStyleBarItem::paint(QPainter& painter, int widgetW, int widgetH)
{
    paintForLayer(painter, widgetW, widgetH, Layer::OverlayDynamic);
}

void PsStyleBarItem::paintForLayer(QPainter& painter, int widgetW, int widgetH, Layer layer)
{
    if (layer != Layer::OverlayDynamic) { return; }
    const QRect area = pixelRect(widgetW, widgetH).adjusted(4, 2, -4, -2);
    if (area.width() < 24 || area.height() < 20) { return; }
    painter.save();
    if (stageDisabled()) { painter.setOpacity(0.45); }
    painter.setRenderHint(QPainter::Antialiasing);
    QFont textFont = PureSignalViewStyle::font(painter.font());
    textFont.setPixelSize(11);
    painter.setFont(textFont);
    const QFontMetrics metrics(textFont);
    const int tickBand = metrics.height() + 2;
    const int valueBand = qMin(22, qMax(14, area.height() / 3));
    const QRectF bar(area.left(), area.top() + valueBand, area.width(),
                     qBound(8, area.height() - valueBand - tickBand - 4, 12));
    const auto xOf = [&](double reading) {
        return bar.left() + std::clamp(calibratedPosition(reading), 0.0, 0.99) / 0.99 * bar.width();
    };
    const bool reading = hasPrimaryReading();
    const double shown = primaryValue();
    const QString reason = bindingUnavailableReason(bindingId());

    painter.setPen(PureSignalViewStyle::muted());
    painter.drawText(QRectF(area.left(), area.top(), area.width() * 0.45, valueBand),
                     Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(m_caption, Qt::ElideRight, int(area.width() * 0.45)));
    QFont valueFont = textFont;
    valueFont.setPixelSize(qMax(13, valueBand - 4));
    valueFont.setWeight(QFont::Medium);
    painter.setFont(valueFont);
    QString number = QStringLiteral("—");
    if (reading) {
        const double span = qAbs(m_maximum - m_minimum);
        const int decimals = m_units == QLatin1String(":1") ? 2 : (span <= 20.0 ? 1 : 0);
        number = QString::number(shown, 'f', decimals);
        if (!m_units.isEmpty()) { number += QStringLiteral(" ") + m_units; }
        painter.setPen(tone(shown, m_middle, m_redThreshold));
    } else {
        painter.setPen(PureSignalViewStyle::muted());
        if (!reason.isEmpty()) { number = reason; }
    }
    painter.drawText(QRectF(area.left() + area.width() * 0.4, area.top(), area.width() * 0.6, valueBand),
                     Qt::AlignVCenter | Qt::AlignRight, metrics.elidedText(number, Qt::ElideRight, int(area.width() * 0.6)));

    painter.setFont(textFont);
    painter.setPen(Qt::NoPen);
    painter.setBrush(PureSignalViewStyle::border());
    painter.drawRoundedRect(bar, 3, 3);
    const QColor good = tone(m_minimum, m_middle, m_redThreshold);
    const qreal band = xOf(m_redThreshold) - bar.left();
    if (band > 1.0) {
        painter.setBrush(QColor(good.red(), good.green(), good.blue(), 65));
        painter.drawRect(QRectF(bar.left(), bar.top(), band, bar.height()));
    }
    if (reading) {
        const qreal marker = xOf(shown);
        painter.setBrush(WorkspacePreview::roleColour(QColor("#7098bb"), "accent"));
        painter.drawRoundedRect(QRectF(bar.left(), bar.top() + bar.height() * 0.33,
                                        qMax(0.0, marker - bar.left()), bar.height() * 0.34), 2, 2);
        painter.setPen(QPen(tone(shown, m_middle, m_redThreshold), 2));
        painter.drawLine(QPointF(marker, bar.top() - 3), QPointF(marker, bar.bottom() + 3));
    }

    painter.setPen(PureSignalViewStyle::muted());
    QList<double> ticks = m_major;
    if (ticks.size() > 5) {
        const QList<double> full = ticks;
        ticks.clear();
        ticks << full.front() << full[full.size() / 4] << full[full.size() / 2] << full[(full.size() * 3) / 4] << full.back();
    }
    for (int i = 0; i < ticks.size(); ++i) {
        const bool top = qAbs(ticks[i] - m_maximum) < 0.05;
        const QString text = tickText(ticks[i], top);
        const int textWidth = metrics.horizontalAdvance(text);
        const qreal left = std::clamp(xOf(ticks[i]) - textWidth / 2.0, static_cast<qreal>(area.left()),
                                       static_cast<qreal>(area.right() - textWidth));
        painter.drawText(QRectF(left, bar.bottom() + 2, textWidth + 1, tickBand), Qt::AlignLeft, text);
    }
    painter.restore();
}
} // namespace NereusSDR
