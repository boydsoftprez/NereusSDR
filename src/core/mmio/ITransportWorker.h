#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QVariant>

namespace NereusSDR {

// Phase 3G-6 block 5 — abstract base for every MMIO transport worker.
// Concrete subclasses (UdpEndpointWorker, TcpListenerEndpointWorker,
// TcpClientEndpointWorker, SerialEndpointWorker) implement start/stop
// and emit valueBatchReceived() whenever a parsed payload yields a
// batch of (key -> QVariant) pairs.
//
// Workers are moved onto ExternalVariableEngine::m_workerThread via
// QObject::moveToThread before start() is called. Never run on the
// main thread.
class ITransportWorker : public QObject {
    Q_OBJECT

public:
    // Connection state surfaced in MmioEndpointsDialog status column.
    enum class State {
        Disconnected,
        Listening,       // TCP listener waiting for inbound connection
        Connecting,      // TCP client dialing out / reconnecting
        Connected,       // UDP bound and listening; TCP connected;
                         // serial port open
        Error
    };
    Q_ENUM(State)

    explicit ITransportWorker(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransportWorker() override = default;

    virtual State state() const { return m_state; }

public slots:
    // Start the transport. Must be called on the worker thread.
    virtual void start() = 0;
    // Stop and tear down all sockets / ports.
    virtual void stop() = 0;

signals:
    // Emitted on every parsed payload. Keys are already formatted as
    // "<endpointGuidStr>.<dottedPath>" so the engine can merge the
    // batch into the endpoint's variable cache without further
    // rewriting.
    void valueBatchReceived(const QHash<QString, QVariant>& values);

    // Emitted whenever the connection state changes.
    void connectionStateChanged(ITransportWorker::State state);

    // Emitted on any non-fatal error (parse failure, drop, etc.).
    // Fatal errors also fire this before transitioning to Error state.
    void errorOccurred(const QString& message);

protected:
    void setState(State s) {
        if (m_state == s) { return; }
        m_state = s;
        emit connectionStateChanged(s);
    }

private:
    State m_state{State::Disconnected};
};

} // namespace NereusSDR
