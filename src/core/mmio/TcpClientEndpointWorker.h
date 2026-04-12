#pragma once
// Phase 3G-6 block 5 — TCP client transport worker.
// Dials an outbound QTcpSocket to the endpoint's (host, port), reads
// newline-delimited messages from the remote server, dispatches each through
// the format parser (JSON/XML/RAW), and emits batches via valueBatchReceived().
// Auto-reconnects on drop with a fixed backoff timer.
//
// Porting intent from Thetis MeterManager.cs:40160-40403 (TcpClientHandler
// inner class), but uses Qt signal/slot (QTcpSocket::connected,
// QTcpSocket::readyRead, QTcpSocket::disconnected, QTcpSocket::errorOccurred)
// instead of Thetis's polling loop.

#include "ITransportWorker.h"
#include <QByteArray>

class QTcpSocket;
class QTimer;

namespace NereusSDR {

class MmioEndpoint;

class TcpClientEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit TcpClientEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~TcpClientEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError();
    void attemptReconnect();

private:
    MmioEndpoint* m_endpoint;
    QTcpSocket*   m_socket{nullptr};
    QTimer*       m_reconnectTimer{nullptr};
    QByteArray    m_lineBuffer;
    bool          m_stopping{false};

    // From Thetis MeterManager.cs:40180 — reconnect delay is 3 seconds.
    static constexpr int kReconnectDelayMs = 3000;
};

} // namespace NereusSDR
