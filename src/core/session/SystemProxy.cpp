// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SystemProxy.cpp  (NereusSDR)
// =================================================================
//
// See SystemProxy.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/SystemProxy.h"

#include <QHostAddress>
#include <QList>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QStandardPaths>

namespace NereusSDR::SystemProxy {

namespace {
std::optional<QNetworkProxy>& proxyForTest()
{
    static std::optional<QNetworkProxy> proxy;
    return proxy;
}
}

void setProxyForTest(const std::optional<QNetworkProxy>& proxy)
{
    if (QStandardPaths::isTestModeEnabled()) {
        proxyForTest() = proxy;
    }
}

QNetworkProxy forUrl(const QUrl& url)
{
    if (QStandardPaths::isTestModeEnabled() && proxyForTest()) {
        return *proxyForTest();
    }
    // This computer itself is never reached through a proxy.
    const QHostAddress host(url.host());
    if (url.host() == QLatin1String("localhost") || (!host.isNull() && host.isLoopback())) {
        return QNetworkProxy(QNetworkProxy::NoProxy);
    }
    QUrl asked = url;
    if (url.scheme() == QLatin1String("wss")) {
        asked.setScheme(QStringLiteral("https"));
    } else if (url.scheme() == QLatin1String("ws")) {
        asked.setScheme(QStringLiteral("http"));
    }
    const QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(asked));
    for (const QNetworkProxy& proxy : proxies) {
        // An HTTP proxy (CONNECT) or SOCKS 5 carries a WebSocket; a caching
        // proxy or FTP one does not.
        if (proxy.type() == QNetworkProxy::HttpProxy
            || proxy.type() == QNetworkProxy::Socks5Proxy) {
            return proxy;
        }
        if (proxy.type() == QNetworkProxy::NoProxy) {
            break;
        }
    }
    return QNetworkProxy(QNetworkProxy::NoProxy);
}

} // namespace NereusSDR::SystemProxy
