#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/floor/TurnTlsShimPrototype.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 Step 1 (R-IOS-16): option (C) of the relay floor,
// prototyped far enough to measure. Not product code: it lives with the
// measurement tool (nereus_floor_probe) only.
//
// libjuice speaks TURN over UDP only (libdatachannel v0.24.5
// src/impl/icetransport.cpp:159 refuses TCP and TLS relays with libjuice).
// The shim gives libjuice a TURN server on a loopback UDP port and carries
// every datagram libjuice sends there (STUN requests and ChannelData) over
// one TLS connection to the relay's TLS listener, and every STUN message or
// ChannelData the relay sends back to libjuice as one datagram. Over a
// stream, TURN frames itself (RFC 8656 section 12.5): a STUN message is
// 20 bytes plus its length; ChannelData is 4 bytes plus its length, padded
// to a multiple of 4 on a stream.
//
// The allocation belongs to the TLS connection (RFC 8656 section 3): when
// the connection ends, the relay deletes it, so resetConnection() models a
// TCP reset and the measurement shows what libjuice does after it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QSslConfiguration>
#include <QString>

QT_BEGIN_NAMESPACE
class QSslSocket;
class QUdpSocket;
QT_END_NAMESPACE

namespace NereusSDR::FloorPrototype {

class TurnTlsShimPrototype : public QObject {
    Q_OBJECT

public:
    TurnTlsShimPrototype(QString relayHost, quint16 relayPort, QObject* parent = nullptr);
    ~TurnTlsShimPrototype() override;

    /// Binds the loopback UDP port libjuice is told to use as its TURN
    /// server and opens the TLS connection. 0 when either fails.
    quint16 start();
    /// Aborts the TLS connection (the far end sees a reset) and opens a new
    /// one at once, as a shim would after a middlebox reset.
    void resetConnection();

    quint64 bytesToRelay() const { return m_bytesToRelay; }
    quint64 bytesFromRelay() const { return m_bytesFromRelay; }
    int connections() const { return m_connections; }

    /// Splits whole TURN frames off the front of a stream buffer, each
    /// without its stream padding; leaves a partial frame in place. False
    /// when the buffer starts with something that is neither.
    static bool takeFrames(QByteArray& buffer, QList<QByteArray>& frames);
    /// The frame as it goes on a stream: ChannelData padded to 4 bytes.
    static QByteArray streamFrame(const QByteArray& datagram);

signals:
    void encrypted();
    void failed(const QString& reason);

private:
    void connectRelay();
    void onDatagrams();
    void onRelayReadable();

    QString m_relayHost;
    quint16 m_relayPort = 0;
    QUdpSocket* m_udp = nullptr;
    QSslSocket* m_tls = nullptr;
    QHostAddress m_agentAddress;
    quint16 m_agentPort = 0;
    QByteArray m_pending;
    QByteArray m_stream;
    quint64 m_bytesToRelay = 0;
    quint64 m_bytesFromRelay = 0;
    int m_connections = 0;
};

} // namespace NereusSDR::FloorPrototype
