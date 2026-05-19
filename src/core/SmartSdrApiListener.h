// =================================================================
// src/core/SmartSdrApiListener.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native class. No upstream port.
//
// AI tooling: Anthropic Claude Code.

#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>

namespace NereusSDR {

// Minimal TCP listener on port 4992 (SmartSDR API port). Currently passive:
// accepts incoming connections, logs every line received, sends nothing back.
// Purpose: bench-capture PGXL's outbound queries so we can design the
// response layer in a follow-up dispatch.
//
// Phase 3P-II follow-up: replace this stub with a full SmartSDR API server
// that responds to `sub slice all`, slice tune queries, etc. The pairing
// path (PGXL discovery beacon -> manual serial entry -> Port A binding) is
// already working; only the band/freq pull from PGXL needs the API server
// to complete.

class SmartSdrApiListener : public QObject {
    Q_OBJECT
public:
    explicit SmartSdrApiListener(QObject* parent = nullptr);

    // Start listening on TCP port 4992 (bind to all interfaces).
    // Returns true on successful bind. Idempotent: stop+restart if already
    // listening.
    bool start();
    void stop();

    bool isListening() const;

signals:
    void clientConnected(const QString& peerHost, quint16 peerPort);
    void lineReceived(const QString& peerHost, quint16 peerPort,
                      const QString& line);

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();

private:
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_readBuffers;
};

}  // namespace NereusSDR
