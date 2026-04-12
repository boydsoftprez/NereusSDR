#pragma once
// Phase 3G-6 block 5 — TCP listener transport worker.
// Binds a QTcpServer to the endpoint's (host, port), accepts a single
// inbound client connection, reads newline-delimited messages, dispatches
// each message through the endpoint's format parser, and emits the resulting
// (key → QVariant) batch via valueBatchReceived().
//
// Porting intent from Thetis MeterManager.cs:39899-40160 (TcpListener inner
// class), but uses Qt signal/slot (QTcpServer::newConnection,
// QTcpSocket::readyRead) instead of Thetis's polling loop.

#include "ITransportWorker.h"
#include <QByteArray>

class QTcpServer;
class QTcpSocket;

namespace NereusSDR {

class MmioEndpoint;

class TcpListenerEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit TcpListenerEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~TcpListenerEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    MmioEndpoint* m_endpoint;
    QTcpServer*   m_server{nullptr};
    QTcpSocket*   m_client{nullptr};
    QByteArray    m_lineBuffer;
};

} // namespace NereusSDR
