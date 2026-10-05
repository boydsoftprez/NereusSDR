// no-port-check: NereusSDR-original GUI presentation value records.
#include "ContainerDocument.h"

#include <QJsonArray>
#include <cmath>
namespace NereusSDR {
std::optional<QRectF> ContentEntry::freeCanvasRect() const
{
    const auto values=extensions.value("freeCanvasRect").toArray();
    if(values.size()!=4) {return std::nullopt;}
    for(const auto& value:values) {if(!value.isDouble() || !std::isfinite(value.toDouble())) {return std::nullopt;}}
    if(values[2].toDouble()<0 || values[3].toDouble()<0) {return std::nullopt;}
    return QRectF(values[0].toDouble(),values[1].toDouble(),values[2].toDouble(),values[3].toDouble());
}
void ContentEntry::setFreeCanvasRect(const QRectF& rect)
{
    extensions["freeCanvasRect"]=QJsonArray{rect.x(),rect.y(),rect.width(),rect.height()};
}
QSizeF ContainerDocument::freeCanvasExtent() const
{
    const auto values=extensions.value("freeCanvasExtent").toArray();
    if(values.size()!=2 || !values[0].isDouble() || !values[1].isDouble()
       || !std::isfinite(values[0].toDouble()) || !std::isfinite(values[1].toDouble())
       || values[0].toDouble()<0 || values[1].toDouble()<0) {return {};}
    return QSizeF(values[0].toDouble(),values[1].toDouble());
}
void ContainerDocument::setFreeCanvasExtent(const QSizeF& extent)
{
    extensions["freeCanvasExtent"]=QJsonArray{extent.width(),extent.height()};
}
}
