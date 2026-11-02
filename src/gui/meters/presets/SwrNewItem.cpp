// no-port-check: NereusSDR-original SWR face in the PureSignal meter shape.
#include "SwrNewItem.h"
#include "gui/PureSignalViewStyle.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"
#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <array>
namespace NereusSDR {
namespace {
// The SWR bar's scale (BarPresetItem.h kSwrBarCalibration): 1.0 at the
// left, 5.0 at the right, with 1.5, 2 and 3 on the same fractions.
constexpr std::array<std::pair<double, double>, 5> kScale{{
    {1.0, 0.0}, {1.5, 0.25}, {2.0, 0.5}, {3.0, 0.75}, {5.0, 0.99}}};
constexpr double kGoodSwr = 1.5;
constexpr double kMarginalSwr = 2.0;

double positionOf(double swr)
{
    if (swr <= kScale.front().first) { return kScale.front().second; }
    if (swr >= kScale.back().first) { return kScale.back().second; }
    for (std::size_t i = 1; i < kScale.size(); ++i) {
        if (swr <= kScale[i].first) {
            const double span = kScale[i].first - kScale[i - 1].first;
            const double t = span > 0.0 ? (swr - kScale[i - 1].first) / span : 0.0;
            return kScale[i - 1].second + t * (kScale[i].second - kScale[i - 1].second);
        }
    }
    return kScale.back().second;
}

QColor classColour(double swr)
{
    if (swr <= kGoodSwr) { return WorkspacePreview::statusColour(QColor("#20c060"), "active"); }
    if (swr <= kMarginalSwr) { return WorkspacePreview::statusColour(QColor("#ffd24a"), "warning"); }
    return WorkspacePreview::statusColour(QColor("#ff6060"), "error");
}
} // namespace

SwrNewItem::SwrNewItem(QObject* parent) : MeterItem(parent)
{
    setBindingId(MeterBinding::TxSwr);
}

void SwrNewItem::paint(QPainter& painter, int widgetW, int widgetH)
{
    const QRect area = pixelRect(widgetW, widgetH).adjusted(4, 2, -4, -2);
    if (area.width() < 24 || area.height() < 20) { return; }
    painter.setRenderHint(QPainter::Antialiasing);
    QFont textFont = PureSignalViewStyle::font(painter.font());
    textFont.setPixelSize(11);
    painter.setFont(textFont);
    const QFontMetrics metrics(textFont);
    const int tickBand = metrics.height() + 2;
    const int valueBand = qMin(22, qMax(14, area.height() / 3));
    const QRectF bar(area.left(), area.top() + valueBand, area.width(),
                     qBound(8, area.height() - valueBand - tickBand - 4, 12));
    const auto xOf = [&bar](double swr) {
        return bar.left() + positionOf(swr) / positionOf(kScale.back().first) * bar.width();
    };
    const bool reading = std::isfinite(value()) && value() >= 1.0
        && bindingUnavailableReason(bindingId()).isEmpty();

    painter.setPen(PureSignalViewStyle::muted());
    painter.drawText(QRectF(area.left(), area.top(), area.width() * 0.4, valueBand),
                     Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("SWR"));
    QFont valueFont = textFont;
    valueFont.setPixelSize(qMax(14, valueBand - 4));
    valueFont.setWeight(QFont::Medium);
    painter.setFont(valueFont);
    painter.setPen(reading ? classColour(value()) : PureSignalViewStyle::muted());
    const QString number = reading
        ? QString::number(value(), 'f', value() < 10.0 ? 2 : 1) + QStringLiteral(" :1")
        : QStringLiteral("—");
    painter.drawText(QRectF(area.left(), area.top(), area.width(), valueBand),
                     Qt::AlignVCenter | Qt::AlignRight, number);

    painter.setFont(textFont);
    painter.setPen(Qt::NoPen);
    painter.setBrush(PureSignalViewStyle::border());
    painter.drawRoundedRect(bar, 3, 3);
    const QColor good = classColour(kGoodSwr);
    painter.setBrush(QColor(good.red(), good.green(), good.blue(), 65));
    painter.drawRect(QRectF(bar.left(), bar.top(), xOf(kGoodSwr) - bar.left(), bar.height()));
    if (reading) {
        const qreal marker = xOf(std::min(value(), kScale.back().first));
        painter.setBrush(WorkspacePreview::roleColour(QColor("#7098bb"), "accent"));
        painter.drawRoundedRect(QRectF(bar.left(), bar.top() + bar.height() * 0.33,
                                        marker - bar.left(), bar.height() * 0.34), 2, 2);
        painter.setPen(QPen(classColour(value()), 2));
        painter.drawLine(QPointF(marker, bar.top() - 3), QPointF(marker, bar.bottom() + 3));
    }

    painter.setPen(PureSignalViewStyle::muted());
    const std::array<double, 5> ticks{1.0, 1.5, 2.0, 3.0, 5.0};
    for (double tick : ticks) {
        const QString text = tick >= kScale.back().first ? QStringLiteral("5+") : QString::number(tick);
        const int textWidth = metrics.horizontalAdvance(text);
        const qreal left = std::clamp(xOf(tick) - textWidth / 2.0, static_cast<qreal>(area.left()),
                                       static_cast<qreal>(area.right() - textWidth));
        painter.drawText(QRectF(left, bar.bottom() + 2, textWidth + 1, tickBand), Qt::AlignLeft, text);
    }
}

QString SwrNewItem::serialize() const
{
    return QStringLiteral("meter.swrNew|%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(x()))
        .arg(static_cast<double>(y()))
        .arg(static_cast<double>(itemWidth()))
        .arg(static_cast<double>(itemHeight()))
        .arg(bindingId())
        .arg(zOrder());
}

bool SwrNewItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("meter.swrNew")) { return false; }
    return MeterItem::deserialize(QStringList(parts.mid(1, 6)).join(QLatin1Char('|')));
}
} // namespace NereusSDR
