// =================================================================
// src/core/LanDiscovery.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native UDP listener for PowerGeniusXL / TeragenXL
// announcements on ports 9008 and 9010. Parses device model,
// IP address, version, serial, and nickname using the official
// FlexRadio regex. Legacy users deduplicate by serial number; station-owned
// identity admission can opt into endpoint-sensitive deduplication.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md (section 6.3)
// Regex pattern and wire format: FlexRadio LAN discovery protocol
//
// AI tooling: Anthropic Claude Code; modified by J.J. Boyd (KG4VCF),
// September 2026, AI-assisted via OpenAI Codex. Station network filter
// (setStationBind, R-R3-22 / R-R3-47) by J.J. Boyd (KG4VCF), 2026-09-24,
// AI-assisted via Anthropic Claude Code.

#include "LanDiscovery.h"
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QHostAddress>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcLan, "nereus.lan")

LanDiscovery::LanDiscovery(QObject* parent) : QObject(parent) {
    connect(&m_sock9008, &QUdpSocket::readyRead, this, &LanDiscovery::on9008Ready);
    connect(&m_sock9010, &QUdpSocket::readyRead, this, &LanDiscovery::on9010Ready);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &LanDiscovery::onTimeout);
}

void LanDiscovery::start(int timeoutMs) {
    m_seenIdentities.clear();
    m_sock9008.bind(QHostAddress::AnyIPv4, 9008,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_sock9010.bind(QHostAddress::AnyIPv4, 9010,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_timeout.start(timeoutMs);
}

void LanDiscovery::stop() {
    m_timeout.stop();
    m_sock9008.close();
    m_sock9010.close();
}

void LanDiscovery::on9008Ready() {
    readSocket(m_sock9008, 9008);
}

void LanDiscovery::on9010Ready() {
    readSocket(m_sock9010, 9010);
}

void LanDiscovery::readSocket(QUdpSocket& socket, quint16 port) {
    while (socket.hasPendingDatagrams()) {
        QByteArray d(int(socket.pendingDatagramSize()), 0);
        QHostAddress sender;
        socket.readDatagram(d.data(), d.size(), &sender);
        parseAnnouncement(QString::fromUtf8(d).trimmed(), port, sender);
    }
}

void LanDiscovery::onTimeout() {
    stop();
    emit scanFinished();
}

void LanDiscovery::injectDatagramForTesting(const QString& payload, quint16 receivedPort,
                                            const QHostAddress& sender) {
    parseAnnouncement(payload, receivedPort, sender);
}

void LanDiscovery::parseAnnouncement(const QString& payload, quint16 port,
                                     const QHostAddress& sender) {
    static const QRegularExpression rx(
        R"(^(?<model>\S+)\s+ip=(?<ip>\d+\.\d+\.\d+\.\d+)\s+v=(?<v>\S+)\s+serial=(?<serial>\S+)\s+nickname=(?<nick>\S+)$)");
    auto m = rx.match(payload);
    if (!m.hasMatch()) return;
    // R-R3-22 / R-R3-47: on the Core, only the station network (and this
    // computer) is heard, both as the sender and as the announced address.
    if (m_stationBind
        && (!m_stationBind->acceptsPeer(sender)
            || !m_stationBind->acceptsPeer(QHostAddress(m.captured("ip"))))) {
        qCDebug(lcLan) << "ignored an announcement from outside the station network:"
                       << sender << m.captured("ip");
        m_ignoredOffNetwork.append({m.captured("model"), m.captured("ip"),
                                    m.captured("serial")});
        return;
    }
    const QString serial = m.captured("serial");
    QString identity = serial;
    if (m_identitySensitiveDeduplication) {
        identity = QStringLiteral("%1\x1f%2\x1f%3\x1f%4")
                       .arg(m.captured("model"), m.captured("ip"),
                            QString::number(port), serial);
    }
    if (m_seenIdentities.contains(identity)) return;
    m_seenIdentities.insert(identity);
    emit deviceDiscovered(m.captured("model"),
                          m.captured("ip"),
                          port,
                          m.captured("v"),
                          serial,
                          m.captured("nick"));
}

}  // namespace NereusSDR
