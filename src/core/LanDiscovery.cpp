// =================================================================
// src/core/LanDiscovery.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native UDP listener for PowerGeniusXL / TeragenXL
// announcements on ports 9008 and 9010. Parses device model,
// IP address, version, serial, and nickname using the official
// FlexRadio regex. Deduplicates by serial number before emitting
// the deviceDiscovered signal.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md (section 6.3)
// Regex pattern and wire format: FlexRadio LAN discovery protocol
//
// AI tooling: Anthropic Claude Code.

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
    m_seenSerials.clear();
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
    while (m_sock9008.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9008.pendingDatagramSize()), 0);
        m_sock9008.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9008);
    }
}

void LanDiscovery::on9010Ready() {
    while (m_sock9010.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9010.pendingDatagramSize()), 0);
        m_sock9010.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9010);
    }
}

void LanDiscovery::onTimeout() {
    stop();
    emit scanFinished();
}

void LanDiscovery::injectDatagramForTesting(const QString& payload) {
    parseAnnouncement(payload, 9008);  // port arbitrary for tests
}

void LanDiscovery::parseAnnouncement(const QString& payload, quint16 port) {
    static const QRegularExpression rx(
        R"(^(?<model>\S+)\s+ip=(?<ip>\d+\.\d+\.\d+\.\d+)\s+v=(?<v>\S+)\s+serial=(?<serial>\S+)\s+nickname=(?<nick>\S+)$)");
    auto m = rx.match(payload);
    if (!m.hasMatch()) return;
    QString serial = m.captured("serial");
    if (m_seenSerials.contains(serial)) return;
    m_seenSerials.insert(serial);
    emit deviceDiscovered(m.captured("model"),
                          m.captured("ip"),
                          port,
                          m.captured("v"),
                          serial,
                          m.captured("nick"));
}

}  // namespace NereusSDR
