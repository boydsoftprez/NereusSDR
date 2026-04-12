#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QHash>
#include <QVariant>
#include <QReadWriteLock>
#include <QDateTime>

namespace NereusSDR {

class ITransportWorker;

// Phase 3G-6 block 5 — one MMIO endpoint (connection + format +
// variable cache). Endpoints are the primary entity in the MMIO
// subsystem; variables are discovered by parsing incoming payloads,
// not pre-configured.
//
// Matches Thetis's `clsMMIO` from MeterManager.cs but NereusSDR uses
// a QHash + QReadWriteLock instead of Thetis's ConcurrentDictionary.
// Reads are read-locked (MeterPoller hot path); writes are
// write-locked (called from the worker thread on every parsed batch).
class MmioEndpoint : public QObject {
    Q_OBJECT

public:
    // Transport kind. Thetis splits TCP into listener (accepts
    // inbound) and client (dials outbound) — NereusSDR follows suit.
    enum class Transport {
        UdpListener = 0,
        TcpListener = 1,
        TcpClient   = 2,
        Serial      = 3,
    };
    Q_ENUM(Transport)

    // Payload format. Thetis MeterManager.cs:40320-40353.
    enum class Format {
        Json = 0,
        Xml  = 1,
        Raw  = 2,
    };
    Q_ENUM(Format)

    explicit MmioEndpoint(QObject* parent = nullptr);
    ~MmioEndpoint() override;

    // Identity
    QUuid guid() const { return m_guid; }
    void setGuid(const QUuid& g) { m_guid = g; }
    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    // Transport + format
    Transport transport() const { return m_transport; }
    void setTransport(Transport t) { m_transport = t; }
    Format format() const { return m_format; }
    void setFormat(Format f) { m_format = f; }

    // Common transport params — not every field applies to every
    // transport; unused ones stay at their defaults.
    QString host() const { return m_host; }
    void setHost(const QString& h) { m_host = h; }
    quint16 port() const { return m_port; }
    void setPort(quint16 p) { m_port = p; }
    QString serialDevice() const { return m_serialDevice; }
    void setSerialDevice(const QString& d) { m_serialDevice = d; }
    int serialBaud() const { return m_serialBaud; }
    void setSerialBaud(int b) { m_serialBaud = b; }

    // --- Variable cache ---
    // Read a variable by its (already-prefixed) key. Returns an
    // invalid QVariant if the key isn't present. Thread-safe.
    QVariant value(const QString& key) const;

    // Read a variable by its raw name (no guid prefix). Internally
    // prepends the guid and delegates to value().
    QVariant valueForName(const QString& variableName) const;

    // Last time a value was written for this variable. Used for the
    // stale-value overlay in meter items.
    QDateTime lastUpdate(const QString& variableName) const;

    // Snapshot the variable names currently in the cache — used by
    // the MmioVariablePickerPopup to build its tree. Thread-safe.
    QStringList variableNames() const;

    // Merge a batch of newly-parsed values. Called by
    // ExternalVariableEngine on a queued signal from the worker
    // thread. Write-locked.
    void mergeBatch(const QHash<QString, QVariant>& batch);

signals:
    // Fired when the variable set grows (new keys discovered). The
    // picker popup listens to this to refresh its tree.
    void variablesDiscovered();

private:
    // Strip the "<guid>." prefix off a stored key to get the raw
    // variable name. Returns the key unchanged if the prefix is
    // absent.
    QString stripGuidPrefix(const QString& key) const;

    QUuid    m_guid;
    QString  m_name;
    Transport m_transport{Transport::UdpListener};
    Format   m_format{Format::Json};
    QString  m_host{QStringLiteral("0.0.0.0")};
    quint16  m_port{0};
    QString  m_serialDevice;
    int      m_serialBaud{9600};

    mutable QReadWriteLock m_lock;
    QHash<QString, QVariant> m_values;
    QHash<QString, QDateTime> m_lastUpdate;
};

} // namespace NereusSDR
