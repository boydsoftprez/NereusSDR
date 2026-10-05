// no-port-check: NereusSDR-native face geometry following the approved study.
#pragma once
#include <QRectF>
#include "../ResponsiveText.h"
namespace NereusSDR {
struct PresetGeometry {
    QRectF face; double scale, left, width, top, baseline, bottom;
    explicit PresetGeometry(QRectF r) : face(r), scale(objectTextScale(r,{260,72})), left(r.left()+18*scale), width(qMax(0.0,r.width()-36*scale)), top(r.top()+29*scale), baseline(r.bottom()-15*scale), bottom(baseline+5*scale) {}
};
}
