// no-port-check: NereusSDR logical-object typography; upstream studied, no port.
// Modification history (NereusSDR):
//   2026-10-26 - drawObjectTextElided: text that does not fit its box at the
//                 11 px minimum ends in an ellipsis instead of being cut on
//                 both sides. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#pragma once
#include "gui/styles/WorkspacePreview.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <cmath>
namespace NereusSDR {
// Reference dimensions are stable native design calibrations, never a saved
// height, a physical texture size or the shared canvas dimensions.
inline double objectTextScale(const QRectF& object, const QSizeF& reference) {
    return qMax(0.0,qMin(object.width()/reference.width(),object.height()/reference.height()));
}
inline QFont fitObjectText(QFont font, double requested, const QString& text,
                           const QRectF& box, bool points=false, int flags=Qt::TextSingleLine, QPaintDevice* device=nullptr) {
    const auto size=[&](double value) {
        if(points) {
            font.setPointSizeF(qMax(.1,value));
            // Point fonts resolve against the actual paint device. Preserve
            // their unit and exact size whenever they already meet the floor.
            font = WorkspacePreview::readableFont(font,device);
        }
        else { font.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1,int(std::floor(value))))); }
    };
    size(requested);
    // Binary search final metrics, including bearings and wrapped lines.
    // Every candidate respects the workspace floor. Keep the original search
    // interval and pass count so already-readable fitted point sizes remain
    // bit-for-bit unchanged; an undersized box returns the minimum font.
    double low=0,high=qMax(.1,requested);
    const auto fits=[&]() {
        const QFontMetricsF metrics(font,device);
        const QRectF ink=metrics.boundingRect(box,flags,text);
        return metrics.height()<=box.height() && ink.width()<=box.width() && ink.height()<=box.height();
    };
    if(fits()) { return font; }
    for(int pass=0;pass<16;++pass) {
        const double candidate=(low+high)/2; size(candidate);
        if(fits()) { low=candidate; } else { high=candidate; }
    }
    size(low); return font;
}
// The text as drawn in `box` with `font`: whole when it fits, otherwise cut
// at the end with an ellipsis so it never spills past the box on either side.
inline QString elideObjectText(const QFont& font,const QString& text,const QRectF& box,
                               QPaintDevice* device=nullptr) {
    const QFontMetricsF metrics(font,device);
    if(metrics.horizontalAdvance(text)<=box.width()) { return text; }
    return metrics.elidedText(text,Qt::ElideRight,box.width());
}
// drawObjectText for a caption: fitted down to the readable minimum, then
// ended with an ellipsis if it still does not fit. Returns the text drawn.
inline QString drawObjectTextElided(QPainter& painter,const QRectF& box,const QString& text,
                                    double requested,int alignment=Qt::AlignCenter) {
    if(box.isEmpty() || text.isEmpty() || requested<=0) { return QString(); }
    const QFont font=fitObjectText(painter.font(),requested,text,box,false,Qt::TextSingleLine,painter.device());
    const QString shown=elideObjectText(font,text,box,painter.device());
    painter.setFont(font);
    painter.drawText(box,alignment,shown);
    return shown;
}
inline void drawObjectText(QPainter& painter,const QRectF& box,const QString& text,
                           double requested,int alignment=Qt::AlignCenter) {
    if(box.isEmpty() || text.isEmpty() || requested<=0) { return; }
    painter.setFont(fitObjectText(painter.font(),requested,text,box,false,Qt::TextSingleLine,painter.device()));
    painter.drawText(box,alignment,text);
}
}
