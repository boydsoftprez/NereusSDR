// =================================================================
// src/core/Rf2ksConnection.h  (NereusSDR)
// =================================================================
// NereusSDR-native. No upstream port.
//   2026-05-24  Initial implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Patterns mirror src/core/PgxlConnection.{h,cpp}
//                 (which is itself an AetherSDR port); wire format is REST
//                 not C/R/S/V text.
// =================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace NereusSDR {

struct RfKitPowerSnapshot {
    int   forwardW       = 0;
    int   reflectedW     = 0;
    int   forwardMaxW    = 0;
    int   reflectedMaxW  = 0;
    float swr            = 1.0f;
    float swrMax         = 1.0f;
    float temperatureC   = 0.0f;
    float voltageV       = 0.0f;
    float currentA       = 0.0f;
};

struct RfKitTunerSnapshot {
    enum class Mode { Bypass, Manual, AutoTuning, Auto, Unknown };
    Mode    mode               = Mode::Unknown;
    QString setup;
    int     lValuenH           = 0;
    int     cValuepF           = 0;
    int     tunedFrequencyKHz  = 0;
    int     segmentSizeKHz     = 0;
};

struct RfKitAntenna {
    enum class Type  { Internal, External };
    enum class State { Active, Available, Disabled };
    Type  type    = Type::Internal;
    int   number  = 0;
    State state   = State::Available;
};

class Rf2ksConnection : public QObject {
    Q_OBJECT
public:
    explicit Rf2ksConnection(QObject* parent = nullptr);
    ~Rf2ksConnection() override;

    bool    isConnected()      const { return m_connected; }
    QString deviceName()       const { return m_deviceName; }
    QString softwareVersion()  const { return m_softwareVersion; }
    QString peerAddress()      const { return m_host; }
    quint16 peerPort()         const { return m_port; }

    qint64  connectedSinceMs()    const noexcept { return m_connectedSinceMs; }
    qint64  lastPollMs()          const noexcept { return m_lastPollMs; }
    int     pollsSucceeded()      const noexcept { return m_pollsSucceeded; }
    int     pollsFailed()         const noexcept { return m_pollsFailed; }
    int     rttAvgLast10Ms()      const noexcept { return m_rttAvgMs; }
    int     reconnectAttempts()   const noexcept { return m_reconnectAttempts; }
    bool    autoReconnect()       const noexcept { return m_autoReconnect; }

    RfKitPowerSnapshot  lastPower()             const { return m_lastPower; }
    RfKitTunerSnapshot  lastTuner()             const { return m_lastTuner; }
    QList<RfKitAntenna> antennas()              const { return m_antennas; }
    RfKitAntenna        activeAntenna()         const { return m_active; }
    QString             operateMode()           const { return m_operateMode; }
    QString             operationalInterface()  const { return m_opIfx; }
    QString             lastError()             const { return m_lastError; }

    // Test seam: feed a raw JSON body into the response dispatcher without
    // a real network request. Production code never calls this.
    void injectJsonForTesting(const QString& path, const QByteArray& json);

    // Test-only: drive the backoff calculation without waiting on real
    // network failures or timers. Production code never calls these.
    void testForceBackoffSequence();
    void testForceBackoffReset();
    int  testCurrentBackoffMs() const noexcept { return m_reconnectBackoffMs; }
    // Sets m_connected to true without going through the network stack,
    // so tests that need disconnect() to emit disconnected() can arm the
    // guard without spinning up a real HTTP server.
    void testForceConnectedForTesting() { m_connected = true; }
    // Arms a real reconnect (unlike testForceBackoffSequence, which cancels
    // it immediately because it only wants the backoff arithmetic), and
    // reports whether one is currently pending.  Together these let a test
    // assert that disconnect() actually cancels a scheduled reconnect.
    void testScheduleReconnect() { scheduleReconnect(); }
    bool testReconnectPending() const { return m_reconnectTimer.isActive(); }
    int testInFlightReplyCount() const { return m_inFlight.size(); }

public slots:
    void connectToAmp(const QString& host, quint16 port = 8080);
    void disconnect();
    void setPollIntervalMs(int ms);
    // When false, a dropped link is NOT retried.  Backs the "Auto-reconnect"
    // checkbox on Setup -> Peripherals -> RF-Kit, which was previously saved
    // to AppSettings and never read (review blocker [P2] on PR #291).
    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    void setActiveAntenna(RfKitAntenna::Type type, int number);
    void setOperateMode(const QString& mode);
    void setOperationalInterface(const QString& iface);
    void resetError();

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);

    void powerUpdated(const RfKitPowerSnapshot& snap);
    void tunerUpdated(const RfKitTunerSnapshot& snap);
    void antennasUpdated(const QList<RfKitAntenna>& list);
    void activeAntennaUpdated(const RfKitAntenna& a);
    void operateModeUpdated(const QString& mode);
    void operationalInterfaceUpdated(const QString& iface, const QString& errorField);
    void infoUpdated(const QString& deviceName, const QString& softwareVersion,
                     const QString& nicknameFromAmp);
    void dataUpdated(int bandM, int frequencyKHz, const QString& status);

    void faultObserved(const QString& kind, const QString& detail);

private slots:
    void pollOnce();
    void scheduleReconnect();
    void onReplyFinished();
    // Fired by the owned m_reconnectTimer. Was an inline lambda passed to
    // the static QTimer::singleShot, which disconnect() could not cancel.
    void onReconnectTimeout();

private:
    void   handleResponse(const QString& path, const QByteArray& body);
    void   issueGet(const QString& path);
    void   issuePut(const QString& path, const QByteArray& body);
    void   issuePost(const QString& path);
    void   trackReply(QNetworkReply* reply);
    void   markPollSuccess(int rttMs);
    void   markPollFailure();
    void   parseInfo(const QByteArray& body);
    void   parsePower(const QByteArray& body);
    void   parseTuner(const QByteArray& body);
    void   parseAntennas(const QByteArray& body);
    void   parseActiveAntenna(const QByteArray& body);
    void   parseOperateMode(const QByteArray& body);
    void   parseOperationalInterface(const QByteArray& body);
    void   parseData(const QByteArray& body);

    // Declared before m_nam so it outlives replies destroyed by the network
    // manager during member teardown; their destroyed handlers remove from it.
    QSet<QNetworkReply*> m_inFlight;
    std::unique_ptr<QNetworkAccessManager> m_nam;
    QTimer  m_pollTimer;
    QTimer  m_reconnectTimer;

    QString m_host;
    quint16 m_port               = 8080;
    bool    m_connected          = false;
    bool    m_operatorDisconnected = true;
    bool    m_autoReconnect      = true;   // default matches prior behaviour
    quint64 m_generation         = 0;
    int     m_pollIntervalMs     = 1000;
    // Half of the first real reconnect delay (500 ms).  scheduleReconnect()
    // doubles before scheduling, so the first actual retry fires after 1 s,
    // the second after 2 s, third after 4 s, capping at 60 s.
    int     m_reconnectBackoffMs = 500;

    QString m_deviceName;
    QString m_softwareVersion;
    QString m_operateMode;
    QString m_opIfx;
    QString m_opIfxErrorField;
    QString m_lastError;

    RfKitPowerSnapshot  m_lastPower;
    RfKitTunerSnapshot  m_lastTuner;
    QList<RfKitAntenna> m_antennas;
    RfKitAntenna        m_active;

    qint64 m_connectedSinceMs    = 0;
    qint64 m_lastPollMs          = 0;
    int    m_pollsSucceeded      = 0;
    int    m_pollsFailed         = 0;
    int    m_rttAvgMs            = 0;
    int    m_reconnectAttempts   = 0;
    int    m_consecutiveFailures = 0;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RfKitPowerSnapshot)
Q_DECLARE_METATYPE(NereusSDR::RfKitTunerSnapshot)
Q_DECLARE_METATYPE(NereusSDR::RfKitAntenna)
Q_DECLARE_METATYPE(QList<NereusSDR::RfKitAntenna>)
