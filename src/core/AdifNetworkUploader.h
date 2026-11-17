#pragma once

// =================================================================
// src/core/AdifNetworkUploader.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Hands an ADIF record to a logger running on the network — Log4OM and
// DXKeeper both have an "ADIF message" listener, as do several contest
// loggers, and they all want the same thing: the record as text on a
// socket.
//
// Be clear about what this does and does not promise. UDP is fire and
// forget: the datagram leaves, and that is all anyone can know. There
// is no acknowledgement in the protocol, so a "sent" result here means
// sent, not filed. TCP at least tells us the logger accepted the
// connection and read the bytes, which is why it is worth offering
// both and saying which is which in the UI.
//
// Reporting "uploaded" for a UDP datagram that fell on the floor would
// be worse than useless: the operator would stop checking.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "QsoUploader.h"

namespace Longpath {

class AdifNetworkUploader : public QsoUploader {
    Q_OBJECT
public:
    enum class Transport { Udp, Tcp };

    explicit AdifNetworkUploader(QObject* parent = nullptr);

    void setTarget(const QString& host, quint16 port, Transport t);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    Transport transport() const { return m_transport; }

    // Shown in the UI beside the result, so nobody reads a UDP "sent"
    // as a confirmation the logger has the contact.
    QString serviceName() const override;
    QString shortName() const override { return QStringLiteral("local logger"); }
    QString serviceId() const override { return QStringLiteral("adifnet"); }
    bool isConfigured() const override
    {
        return !m_host.isEmpty() && m_port != 0;
    }
    void upload(const LogEntry& entry) override;

    // How long to wait for a TCP connection before giving up. Short:
    // a logger on the same desk answers immediately, and a long stall
    // during a contact is worse than a clear failure.
    static constexpr int kTcpTimeoutMs = 3000;

private:
    QString   m_host;
    quint16   m_port{0};
    Transport m_transport{Transport::Udp};
};

} // namespace Longpath
