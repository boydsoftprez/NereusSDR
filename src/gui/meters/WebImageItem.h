#pragma once

#include "MeterItem.h"
#include <QColor>
#include <QImage>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace NereusSDR {

// From Thetis clsWebImage (MeterManager.cs:14165+)
// Displays an image fetched from a URL with periodic refresh.
class WebImageItem : public MeterItem {
    Q_OBJECT

public:
    explicit WebImageItem(QObject* parent = nullptr);
    ~WebImageItem() override;

    void setUrl(const QString& url);
    QString url() const { return m_url; }

    void setRefreshInterval(int seconds);
    int refreshInterval() const { return m_refreshInterval; }

    void setFallbackColor(const QColor& c) { m_fallbackColor = c; }
    QColor fallbackColor() const { return m_fallbackColor; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private slots:
    void fetchImage();
    void onFetchFinished(QNetworkReply* reply);

private:
    QString m_url;
    int     m_refreshInterval{300}; // seconds
    QColor  m_fallbackColor{0x20, 0x20, 0x20};
    QImage  m_image;
    bool    m_fetchInProgress{false};

    QNetworkAccessManager* m_nam{nullptr};
    QTimer m_refreshTimer;
};

} // namespace NereusSDR
