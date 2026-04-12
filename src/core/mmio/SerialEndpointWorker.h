#pragma once

#include "ITransportWorker.h"

#ifdef HAVE_SERIALPORT

#include <QByteArray>

class QSerialPort;

namespace NereusSDR {

class MmioEndpoint;

// Phase 3G-6 block 5 — Serial transport worker for MMIO endpoints.
// Opens a QSerialPort on the endpoint's configured device and baud rate,
// accumulates newline-delimited messages from the port's readyRead signal,
// dispatches each complete line through the endpoint's configured
// FormatParser, and emits valueBatchReceived() with the resulting
// key->value map.
//
// Modelled after Thetis MeterManager.cs SerialPortHandler (lines
// 40589-40816), which uses the DataReceived event — the Qt equivalent
// is QSerialPort::readyRead.
//
// Runs entirely on ExternalVariableEngine::m_workerThread. All slots
// execute on that thread; no cross-thread synchronisation is needed.
class SerialEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit SerialEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~SerialEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onReadyRead();
    void onErrorOccurred();

private:
    MmioEndpoint* m_endpoint;
    QSerialPort*  m_port{nullptr};
    QByteArray    m_lineBuffer;
};

} // namespace NereusSDR

#endif // HAVE_SERIALPORT
