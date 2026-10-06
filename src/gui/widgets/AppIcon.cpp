// src/gui/widgets/AppIcon.cpp
// no-port-check: NereusSDR-original GUI helper.
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 5.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
#include "gui/widgets/AppIcon.h"

#include "core/LogCategories.h"

#include <QAbstractButton>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QWidget>

#include <cmath>

namespace NereusSDR::AppIcon {

namespace {

QString resourcePath(const QString& name)
{
    return QStringLiteral(":/icons/emoji/%1.svg").arg(name);
}

QHash<QString, QPixmap>& cache()
{
    static QHash<QString, QPixmap> pixmaps;
    return pixmaps;
}

} // namespace

QPixmap pixmap(const QString& name, int logicalPx, qreal dpr)
{
    if (logicalPx <= 0 || dpr <= 0.0) {
        qCWarning(lcApp) << "AppIcon: bad size for icon" << name << logicalPx << dpr;
        return {};
    }
    const QString key = QStringLiteral("%1|%2|%3").arg(name).arg(logicalPx).arg(dpr);
    auto it = cache().constFind(key);
    if (it != cache().constEnd()) {
        return it.value();
    }

    const QString path = resourcePath(name);
    if (!QFile::exists(path)) {
        qCWarning(lcApp) << "AppIcon: no icon named" << name;
        return {};
    }
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        qCWarning(lcApp) << "AppIcon: icon" << name << "is not a valid SVG";
        return {};
    }

    const int devicePx = static_cast<int>(std::lround(logicalPx * dpr));
    QImage image(devicePx, devicePx, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&painter, QRectF(0, 0, devicePx, devicePx));
    }
    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(dpr);
    cache().insert(key, result);
    return result;
}

QIcon icon(const QString& name, int logicalPx, const QWidget* forDpr)
{
    QIcon result;
    QList<qreal> ratios{1.0, 2.0};
    if (forDpr) {
        const qreal widgetDpr = forDpr->devicePixelRatioF();
        if (!ratios.contains(widgetDpr)) {
            ratios.append(widgetDpr);
        }
    }
    for (const qreal ratio : ratios) {
        const QPixmap pm = pixmap(name, logicalPx, ratio);
        if (pm.isNull()) {
            return {};
        }
        result.addPixmap(pm);
    }
    return result;
}

void apply(QAbstractButton* button, const QString& name, int logicalPx)
{
    if (!button) {
        return;
    }
    button->setIcon(icon(name, logicalPx, button));
    button->setIconSize(QSize(logicalPx, logicalPx));
    button->setProperty(kIconProperty, name);
}

} // namespace NereusSDR::AppIcon
