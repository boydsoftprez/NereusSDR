// no-port-check: NereusSDR-native face geometry following the approved study.
#pragma once
#include <QRectF>
namespace NereusSDR {
struct PresetGeometry {
    QRectF face; double left, width, top, baseline, bottom;
    explicit PresetGeometry(QRectF r) : face(r), left(r.left()+18), width(qMax(1.0,r.width()-36)), top(r.top()+29), baseline(r.bottom()-15), bottom(baseline+5) {}
};
}
