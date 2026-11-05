// no-port-check: NereusSDR-original native Qt instrument drawing operations.
// Modification history (NereusSDR):
//   2026-11-03 - Share the existing PureSignal track geometry with an optional
//                 bar face, adapting Brian Sutton's PR #731 proposal.
//                 J.J. Boyd (KG4VCF), with OpenAI Codex.
#pragma once
#include <QPainter>
namespace NereusSDR::InstrumentBarDrawing {
// Geometry, colors and range interpretation belong to the caller. In particular,
// a bar's high point is unrelated to PureSignal's feedback range boundaries.
inline void track(QPainter& painter, const QRectF& bar, const QColor& background,
                  const QRectF& band, const QColor& bandColour)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(bar, 3, 3);
    painter.setBrush(bandColour);
    painter.drawRect(band);
}
inline void fill(QPainter& painter, const QRectF& bar, const QColor& colour)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    painter.drawRoundedRect(bar, 2, 2);
}
inline void marker(QPainter& painter, const QPointF& top, const QPointF& bottom,
                   const QColor& colour, qreal thickness)
{
    painter.setPen(QPen(colour, thickness));
    painter.drawLine(top, bottom);
}
} // namespace NereusSDR::InstrumentBarDrawing
