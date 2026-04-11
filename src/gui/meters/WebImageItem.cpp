#include "WebImageItem.h"

// From Thetis clsWebImage (MeterManager.cs:14165+)

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWebImageItem, "nereus.gui.meters.webimageitem")

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Local serialization helpers (mirror pattern from SpacerItem.cpp)
// ---------------------------------------------------------------------------

static QString webImageBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

static bool webImageParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

// ---------------------------------------------------------------------------
// Constructor
// From Thetis clsWebImage constructor (MeterManager.cs:14165+)
// ---------------------------------------------------------------------------
WebImageItem::WebImageItem(QObject* parent)
    : MeterItem(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &WebImageItem::onFetchFinished);

    connect(&m_refreshTimer, &QTimer::timeout,
            this, &WebImageItem::fetchImage);

    m_refreshTimer.setSingleShot(false);
    // Timer is not started until a URL is set.
}

// ---------------------------------------------------------------------------
// Destructor — QObject parent ownership handles m_nam and timer.
// ---------------------------------------------------------------------------
WebImageItem::~WebImageItem() = default;

// ---------------------------------------------------------------------------
// setUrl()
// From Thetis clsWebImage (MeterManager.cs:14165+)
// ---------------------------------------------------------------------------
void WebImageItem::setUrl(const QString& url)
{
    m_url = url;
    if (m_url.isEmpty()) {
        m_refreshTimer.stop();
        m_image = QImage();
    } else {
        fetchImage();
        m_refreshTimer.start(m_refreshInterval * 1000);
    }
}

// ---------------------------------------------------------------------------
// setRefreshInterval()
// ---------------------------------------------------------------------------
void WebImageItem::setRefreshInterval(int seconds)
{
    m_refreshInterval = seconds;
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.start(m_refreshInterval * 1000);
    }
}

// ---------------------------------------------------------------------------
// fetchImage()
// From Thetis clsWebImage — periodic HTTP GET
// ---------------------------------------------------------------------------
void WebImageItem::fetchImage()
{
    if (m_url.isEmpty() || m_fetchInProgress) {
        return;
    }
    m_fetchInProgress = true;
    m_nam->get(QNetworkRequest(QUrl(m_url)));
}

// ---------------------------------------------------------------------------
// onFetchFinished()
// From Thetis clsWebImage — handle HTTP reply, update image
// ---------------------------------------------------------------------------
void WebImageItem::onFetchFinished(QNetworkReply* reply)
{
    m_fetchInProgress = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcWebImageItem) << "Failed to fetch image from" << m_url
                                  << ":" << reply->errorString();
        return; // Keep the existing image on failure.
    }

    QImage img;
    img.loadFromData(reply->readAll());
    if (img.isNull()) {
        qCWarning(lcWebImageItem) << "Could not decode image data from" << m_url;
        return;
    }

    m_image = img;
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsWebImage renderImage (MeterManager.cs:14165+)
// ---------------------------------------------------------------------------
void WebImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (m_image.isNull()) {
        p.fillRect(rect, m_fallbackColor);
    } else {
        p.drawImage(rect, m_image);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: WEBIMAGE|x|y|w|h|bindingId|zOrder|refreshInterval|fallbackColor|url
//         [0]      [1-6]             [7]            [8]           [9]
// ---------------------------------------------------------------------------
QString WebImageItem::serialize() const
{
    return QStringLiteral("WEBIMAGE|%1|%2|%3|%4")
        .arg(webImageBaseFields(*this))
        .arg(m_refreshInterval)
        .arg(m_fallbackColor.name(QColor::HexArgb))
        .arg(m_url);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected: WEBIMAGE|x|y|w|h|bindingId|zOrder|refreshInterval|fallbackColor|url
//           [0]      [1-6]             [7]            [8]           [9]
// ---------------------------------------------------------------------------
bool WebImageItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 10 || parts[0] != QLatin1String("WEBIMAGE")) {
        return false;
    }
    if (!webImageParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;
    const int refreshInterval = parts[7].toInt(&ok);
    if (!ok) {
        return false;
    }

    const QColor fallbackColor(parts[8]);
    if (!fallbackColor.isValid()) {
        return false;
    }

    // Field 9 onwards: URL (plain text; URLs should not contain '|').
    const QString url = parts[9];

    m_refreshInterval = refreshInterval;
    m_fallbackColor = fallbackColor;
    // Use setUrl() so the timer and fetch are kicked off correctly.
    setUrl(url);

    return true;
}

} // namespace NereusSDR
