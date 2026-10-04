// no-port-check: NereusSDR logical-object typography; upstream studied, no port.
#pragma once
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
        if(points) { font.setPointSizeF(qMax(.1,value)); }
        else { font.setPixelSize(qMax(1,int(std::floor(value)))); }
    };
    size(requested);
    // Binary search final metrics, including bearings and wrapped lines.
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
inline void drawObjectText(QPainter& painter,const QRectF& box,const QString& text,
                           double requested,int alignment=Qt::AlignCenter) {
    if(box.isEmpty() || text.isEmpty() || requested<=0) { return; }
    painter.setFont(fitObjectText(painter.font(),requested,text,box,false,Qt::TextSingleLine,painter.device()));
    painter.drawText(box,alignment,text);
}
}
