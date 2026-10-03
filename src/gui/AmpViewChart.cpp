// no-port-check: NereusSDR-original QPainter presentation for source-backed
// PS3 display curves.  Axis modes and colours retain Thetis AmpView behavior.
//
// =================================================================
// src/gui/AmpViewChart.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 9 created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-22 — Accept owning PS3 curves with independent coordinates and
//                 series visibility; J.J. Boyd (KG4VCF), OpenAI Codex.
// =================================================================

#include "AmpViewChart.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace NereusSDR {
namespace {

// AmpView.Designer.cs:56-65 [v2.10.3.13]: black background, DimGray grid.
constexpr QRgb kBackground = qRgb(0x00, 0x00, 0x00);
constexpr QRgb kGrid = qRgb(0x69, 0x69, 0x69);
// AmpView.cs:112-119 [v2.10.3.13]: LightSalmon axis labels.
constexpr QRgb kLabel = qRgb(0xFF, 0xA0, 0x7A);
constexpr int kLeft = 50;
constexpr int kRight = 50;
constexpr int kTop = 18;
constexpr int kBottom = 36;

const QColor kReference(0x69, 0x69, 0x69);
const QColor kMeasuredMagnitude(0x1E, 0x90, 0xFF);
const QColor kMeasuredPhase(0xFF, 0xD7, 0x00);
const QColor kCorrectionMagnitude(0xDC, 0x14, 0x3C);
const QColor kCorrectionPhase(0x00, 0xFF, 0x00);

QPointF mapPoint(const QRectF& plot, const Ps3PlotPoint& point,
                 double yMin, double yMax)
{
    const double x = std::clamp(point.x, 0.0, 1.0);
    const double y = std::clamp(point.y, yMin, yMax);
    return {plot.left() + x * plot.width(),
            plot.bottom() - (y - yMin) / (yMax - yMin) * plot.height()};
}

void drawPoints(QPainter& painter, const QRectF& plot,
                const std::vector<Ps3PlotPoint>& points, QColor colour,
                double yMin, double yMax, int stride)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    const std::size_t step = static_cast<std::size_t>(std::max(1, stride));
    for (std::size_t i = 0; i < points.size(); i += step) {
        if (std::isfinite(points[i].x) && std::isfinite(points[i].y)) {
            painter.drawEllipse(mapPoint(plot, points[i], yMin, yMax), 1.6, 1.6);
        }
    }
}

void drawLine(QPainter& painter, const QRectF& plot,
              const std::vector<Ps3PlotPoint>& points, QColor colour,
              double yMin, double yMax)
{
    QPainterPath path;
    bool started = false;
    for (const Ps3PlotPoint& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            started = false;
            continue;
        }
        const QPointF mapped = mapPoint(plot, point, yMin, yMax);
        if (started) {
            path.lineTo(mapped);
        } else {
            path.moveTo(mapped);
            started = true;
        }
    }
    QPen pen(colour, 1.8);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

std::size_t indexOf(AmpViewChart::Series series)
{
    return static_cast<std::size_t>(series);
}

} // namespace

AmpViewChart::AmpViewChart(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ampViewChart"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

AmpViewChart::~AmpViewChart() = default;

void AmpViewChart::setPlotData(const Ps3PlotData& data)
{
    m_plot = data;
    update();
}

void AmpViewChart::clearData()
{
    m_plot = {};
    update();
}

void AmpViewChart::setShowGain(bool on)
{
    if (m_showGain == on) {
        return;
    }
    m_showGain = on;
    update();
}

void AmpViewChart::setPhaseZoom(bool on)
{
    if (m_phaseZoom == on) {
        return;
    }
    m_phaseZoom = on;
    update();
}

void AmpViewChart::setLowRes(bool on)
{
    if (m_lowRes == on) {
        return;
    }
    m_lowRes = on;
    update();
}

void AmpViewChart::setSeriesVisible(Series series, bool visible)
{
    const std::size_t index = indexOf(series);
    if (index >= m_visible.size() || m_visible[index] == visible) {
        return;
    }
    m_visible[index] = visible;
    update();
}

bool AmpViewChart::seriesVisible(Series series) const noexcept
{
    const std::size_t index = indexOf(series);
    return index < m_visible.size() && m_visible[index];
}

QStringList AmpViewChart::seriesNames() const
{
    return {QStringLiteral("Reference"),
            QStringLiteral("Measured magnitude"),
            QStringLiteral("Measured phase"),
            QStringLiteral("Correction magnitude"),
            QStringLiteral("Correction phase")};
}

void AmpViewChart::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(kBackground));

    const QRectF plot(kLeft, kTop, width() - kLeft - kRight,
                      height() - kTop - kBottom);
    if (!plot.isValid()) {
        return;
    }

    painter.setPen(QPen(QColor(kGrid), 1.0));
    for (int i = 1; i < 5; ++i) {
        const double x = plot.left() + plot.width() * i / 5.0;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
        const double y = plot.top() + plot.height() * i / 4.0;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    painter.drawRect(plot);

    QFont labels = painter.font();
    labels.setPointSizeF(labels.pointSizeF() * 0.85);
    painter.setFont(labels);
    const QFontMetrics metrics(labels);
    painter.setPen(QColor(kLabel));

    const double magnitudeMax = magYMax();
    const double phaseMax = phaseYMax();
    painter.drawText(QPointF(4, plot.top() + metrics.ascent()),
                     QString::number(magnitudeMax, 'f', 1));
    painter.drawText(QPointF(4, plot.center().y() + metrics.ascent() / 2.0),
                     QString::number(magnitudeMax / 2.0, 'f', 1));
    painter.drawText(QPointF(4, plot.bottom() - 2), QStringLiteral("0.0"));
    painter.drawText(QPointF(plot.right() + 4, plot.top() + metrics.ascent()),
                     QString::number(phaseMax, 'f', 0));
    painter.drawText(QPointF(plot.right() + 4,
                             plot.center().y() + metrics.ascent() / 2.0),
                     QStringLiteral("0"));
    painter.drawText(QPointF(plot.right() + 4, plot.bottom() - 2),
                     QString::number(-phaseMax, 'f', 0));
    painter.drawText(QRectF(plot.left(), plot.bottom() + 2,
                            plot.width(), metrics.height()),
                     Qt::AlignCenter, QStringLiteral("Input magnitude"));

    if (seriesVisible(Series::Reference)) {
        QPen referencePen(kReference, 1.5, Qt::DashLine);
        painter.setPen(referencePen);
        const double startY = m_showGain ? 1.0 : 0.0;
        const double endY = 1.0;
        painter.drawLine(mapPoint(plot, {0.0, startY}, 0.0, magnitudeMax),
                         mapPoint(plot, {1.0, endY}, 0.0, magnitudeMax));
    }

    const auto& measuredMagnitude = m_showGain
        ? m_plot.measuredGain : m_plot.measuredMagnitude;
    const auto& correctionMagnitude = m_showGain
        ? m_plot.correctionGain : m_plot.correctionMagnitude;
    if (seriesVisible(Series::MeasuredMagnitude)) {
        drawPoints(painter, plot, measuredMagnitude, kMeasuredMagnitude,
                   0.0, magnitudeMax, lowResStride());
    }
    if (seriesVisible(Series::MeasuredPhase)) {
        drawPoints(painter, plot, m_plot.measuredPhase, kMeasuredPhase,
                   -phaseMax, phaseMax, lowResStride());
    }
    if (seriesVisible(Series::CorrectionMagnitude)) {
        drawLine(painter, plot, correctionMagnitude, kCorrectionMagnitude,
                 0.0, magnitudeMax);
    }
    if (seriesVisible(Series::CorrectionPhase)) {
        drawLine(painter, plot, m_plot.correctionPhase, kCorrectionPhase,
                 -phaseMax, phaseMax);
    }

    struct LegendItem {
        Series series;
        QColor colour;
        QString label;
        bool dashed;
    };
    const std::array<LegendItem, 5> legend{{
        {Series::Reference, kReference, QStringLiteral("Reference"), true},
        {Series::MeasuredMagnitude, kMeasuredMagnitude,
         m_showGain ? QStringLiteral("Measured gain") : QStringLiteral("Measured magnitude"), false},
        {Series::MeasuredPhase, kMeasuredPhase, QStringLiteral("Measured phase"), false},
        {Series::CorrectionMagnitude, kCorrectionMagnitude,
         m_showGain ? QStringLiteral("Correction gain") : QStringLiteral("Correction magnitude"), false},
        {Series::CorrectionPhase, kCorrectionPhase, QStringLiteral("Correction phase"), false},
    }};
    int visibleRows = 0;
    int widest = 0;
    for (const LegendItem& item : legend) {
        if (seriesVisible(item.series)) {
            ++visibleRows;
            widest = std::max(widest, metrics.horizontalAdvance(item.label));
        }
    }
    if (visibleRows == 0) {
        return;
    }
    const int rowHeight = metrics.height();
    const int boxWidth = widest + 34;
    const int boxX = static_cast<int>(plot.right()) - boxWidth - 6;
    const int boxY = static_cast<int>(plot.top()) + 6;
    painter.fillRect(QRect(boxX, boxY, boxWidth, visibleRows * rowHeight + 8),
                     QColor(0, 0, 0, 205));
    painter.setPen(QColor(kGrid));
    painter.drawRect(QRect(boxX, boxY, boxWidth, visibleRows * rowHeight + 8));

    int y = boxY + 4 + metrics.ascent();
    for (const LegendItem& item : legend) {
        if (!seriesVisible(item.series)) {
            continue;
        }
        QPen swatch(item.colour, 2.0, item.dashed ? Qt::DashLine : Qt::SolidLine);
        painter.setPen(swatch);
        painter.drawLine(boxX + 6, y - metrics.ascent() / 3,
                         boxX + 22, y - metrics.ascent() / 3);
        painter.setPen(QColor(kLabel));
        painter.drawText(boxX + 27, y, item.label);
        y += rowHeight;
    }
}

} // namespace NereusSDR
