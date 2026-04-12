#pragma once

#include "ITransportWorker.h"

class QUdpSocket;

namespace NereusSDR {

class MmioEndpoint;

// Phase 3G-6 block 5 — UDP transport worker for MMIO endpoints.
// Binds a QUdpSocket to the endpoint's host/port on start(), reads
// incoming datagrams via the readyRead signal, dispatches each payload
// through the endpoint's configured FormatParser, and emits
// valueBatchReceived() with the resulting key->value map.
//
// Modelled after Thetis MeterManager.cs UdpListener inner class
// (lines 40403-40588) but uses Qt signal/slot (readyRead) instead of
// a polling loop.
class UdpEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit UdpEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~UdpEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onReadyRead();

private:
    MmioEndpoint* m_endpoint;
    QUdpSocket*   m_socket{nullptr};
};

} // namespace NereusSDR
