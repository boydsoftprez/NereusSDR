#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationStatusPage.h  (NereusSDR)
// =================================================================
//
// The Core's small status page (iPhone app plan Task 17, R-IOS-08; spec
// section 5.3 item 11; the pairing design,
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md
// sections 4.2 and 4.3).
//
// A read-only web page on TCP 47911 (nereusd.conf `status_page`,
// `status_port`) for a Core with no screen: its label, whether its radio
// is connected (model and name) or off, whether a device has paired with
// it, and, only while the Core is unclaimed, the pairing code with one
// line on how to use it. It refreshes itself every 5 seconds.
//
// What it will not do, each pinned by tst_station_status_page:
//
//   - answer a peer that is not on one of this computer's directly
//     connected networks (StationServer::isOnDirectNetwork, the rule the
//     one-tap pairing uses). Such a peer's connection is closed with no
//     reply at all, whatever address the page is bound to;
//   - show the code on a claimed Core, including while its pairing window
//     is reopened (the code then goes only to the console and to paired
//     devices): the one-click risk is accepted only for a Core that has
//     never been paired (pairing design section 4.2);
//   - show device names, device keys or addresses;
//   - change anything: `GET /` is the only request it answers; every other
//     method or path gets 404, and nothing it serves has a form or a link
//     that acts;
//   - answer a Host it is not: the Host header must be an IP address or
//     this computer's own name, so a page elsewhere on the web cannot read
//     it through a browser on this network by renaming itself (DNS
//     rebinding). Anything else gets 404;
//   - log the code, or anything a request carried.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R2-I1): the Host check accepts
//               only this computer's own names, whole. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QHostAddress>
#include <QObject>
#include <QString>

#include <functional>

class QTcpServer;
class QTcpSocket;

namespace NereusSDR {

class StationServer;

/// The Core's radio as the status page and the console show it.
struct StationRadioStatus {
    bool connected = false;
    QString model;
    QString name;
};

class StationStatusPage : public QObject {
    Q_OBJECT

public:
    static constexpr quint16 kDefaultPort = 47911;
    /// The page reloads itself this often.
    static constexpr int kRefreshSeconds = 5;
    /// The most a request's head may be; a longer one is closed.
    static constexpr int kMaxRequestBytes = 8192;
    /// A request that has not arrived whole by then is closed.
    static constexpr int kRequestTimeoutMs = 5000;

    /// Where the page reads what it shows. Each is called per request.
    struct Sources {
        /// The Core's pairing and devices; null while remote access is off.
        std::function<StationServer*()> server;
        std::function<StationRadioStatus()> radio;
        /// The Core's label as shown (the renamed label, or its name).
        std::function<QString()> label;
    };
    using PeerCheck = std::function<bool(const QHostAddress&)>;

    explicit StationStatusPage(Sources sources, QObject* parent = nullptr);
    ~StationStatusPage() override;

    /// Replaces the directly-connected-network rule (tests only need it to
    /// stand in for a peer elsewhere, since a test serves loopback alone).
    void setPeerCheck(PeerCheck check);

    bool listen(const QHostAddress& address, quint16 port);
    void close();
    bool isListening() const;
    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    QString lastError() const { return m_lastError; }

    /// The page's HTML as a direct peer would get it now.
    QString renderPage() const;

    /// Whether the page shows the code now: only while the Core is
    /// unclaimed and its window is open unclaimed with a code.
    bool showsCode() const;

    /// The address to give an operator for a page bound to `bound`: that
    /// address, or for every interface this computer's first address on a
    /// network. "" when nothing but this computer would reach it.
    static QString addressForOperator(const QHostAddress& bound, quint16 port);

    /// The first start's lines beside the identity key banner: the Core's
    /// label and where its status page is (`pageAddress` "" when off).
    static QString formatFirstStartNotice(const QString& label, const QString& pageAddress);

    /// True when `host` (a Host header, with or without a port) is an IP
    /// address or one of this computer's own names, whole: exactly
    /// QHostInfo::localHostName(), that name with ".local", or that name
    /// with QHostInfo::localDomainName(). Case does not matter.
    static bool isAcceptedHost(const QString& host);

private:
    void onNewConnection();
    void serve(QTcpSocket* socket);

    Sources m_sources;
    PeerCheck m_peerCheck;
    QTcpServer* m_server = nullptr;
    QString m_lastError;
};

} // namespace NereusSDR
