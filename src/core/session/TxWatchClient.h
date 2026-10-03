// no-port-check: NereusSDR-original direct auxiliary watch transport.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>
#include <utility>

class QTimer;
class QWebSocket;

namespace NereusSDR {

class DataChannelTransport;

// Owner-thread, local application-payload observations for one accepted
// watch attempt. Counts survive close until the next accepted open.
struct AuxiliaryWatchTelemetry {
    quint64 receivedPayloadBytes = 0;
    quint64 submittedPayloadBytes = 0;
};

// Owns one independent WSS or dedicated DTLS watch transport. The caller has already
// authenticated the primary and supplies its current verified Core TLS digest,
// a fresh Core ticket, and the immutable logical primary generation.
// This class has no authority over the primary session or transmit state.
class TxWatchClient final : public QObject {
    Q_OBJECT
public:
    explicit TxWatchClient(QObject* parent = nullptr);
    ~TxWatchClient() override;

    // Deadlines may be shortened by an in-process test, never extended in
    // production. Call before openDirect().
    void setDeadlinesForTesting(int openingMs, int acknowledgementMs);
#ifdef NEREUS_BUILD_TESTS
    void setBacklogBytesForTesting(qint64 bytes) { m_testBacklogBytes = bytes; }
    using BinaryWriterForTesting = std::function<qint64(QWebSocket*, const QByteArray&)>;
    void setBinaryWriterForTesting(BinaryWriterForTesting writer)
    {
        m_binaryWriterForTesting = std::move(writer);
    }
    using RelayWriterForTesting = std::function<bool(DataChannelTransport*, const QByteArray&)>;
    void setRelayWriterForTesting(RelayWriterForTesting writer)
    {
        m_relayWriterForTesting = std::move(writer);
    }
#endif

    bool openDirect(const QUrl& verifiedPrimaryUrl, const QByteArray& actualCorePinSha256,
                    const QByteArray& rawTicket, quint64 primaryGeneration);
    // Consumes a dedicated watch-purpose offerer whose SDP answer has been
    // accepted. It may still be opening. Never owns or closes the primary.
    bool openRelay(DataChannelTransport* transport, const QByteArray& actualCorePinSha256,
                   const QByteArray& rawTicket, quint64 primaryGeneration);
    void close();
    bool isReady() const { return m_ready; }
    quint64 generation() const { return m_generation; }
    // Neither direction is a delivery or wire-byte measurement.
    AuxiliaryWatchTelemetry telemetry() const { return m_telemetry; }
    bool sendKeepalive(quint64 sequence, quint32 epoch);

signals:
    void ready(quint64 primaryGeneration);
    void closed(quint64 primaryGeneration, const QString& reason);

private:
    void finish(const QString& reason);
    bool freshPinMatches(QWebSocket* socket) const;
    bool current(QWebSocket* socket, quint64 generation) const;
    bool current(DataChannelTransport* transport, quint64 generation) const;
    void attachRelay(DataChannelTransport* transport, quint64 generation);
    void receiveAck(const QByteArray& message);

    QPointer<QWebSocket> m_socket;
    QPointer<DataChannelTransport> m_relay;
    QTimer* m_deadline = nullptr;
    QByteArray m_pin;
    QByteArray m_ticket;
    quint64 m_generation = 0;
    quint64 m_revision = 0;
    bool m_active = false;
    bool m_ready = false;
    bool m_attachSent = false;
    AuxiliaryWatchTelemetry m_telemetry;
    int m_openingMs = 10000;
    int m_acknowledgementMs = 5000;
#ifdef NEREUS_BUILD_TESTS
    qint64 m_testBacklogBytes = -1;
    BinaryWriterForTesting m_binaryWriterForTesting;
    RelayWriterForTesting m_relayWriterForTesting;
#endif
};

} // namespace NereusSDR
