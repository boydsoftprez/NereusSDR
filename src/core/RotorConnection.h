// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorConnection.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native. The Core's link to an antenna rotor controller:
// Yaesu GS-232A and GS-232B over a serial port, and Hamlib's rotctld
// over TCP, either already running or started here (RotctldProcess).
//
// Protocol facts, cited, not ported:
//   GS-232A: Hamlib rotators/gs232a/gs232a.c (master). Commands end in
//            CR, replies in LF; `C2` reads `+0aaa+0eee`; `Waaa eee`
//            sets; `S` stops; `L` `R` `U` `D` move.
//   GS-232B: Hamlib rotators/gs232a/gs232b.c (master). As GS-232A, with
//            replies `AZ=aaa EL=eee` or `AZ=aaa` ending CR LF; bare CR LF
//            and `>` are not replies.
//   rotctld: Hamlib rotctld(1). One command per line ending LF; `p`
//            answers azimuth and elevation on two lines, `P az el` sets,
//            `S` stops, `M dir speed` moves (2 up, 4 down, 8 left,
//            16 right); `RPRT n` acknowledges (0 success, negative error).
//   JJ's (KG4VCF) Easy Rotor Control, observed 2026-10-07 in GS-232B
//            mode (tests/data/rotor/erc-gs232b-capture-2026-10-07.log and
//            erc-gs232b-range-2026-10-07.log): `AZ=302  EL=000` with two
//            spaces, `AZ=302` for `C`, a bare CR about 25 ms after each
//            command, replies modulo 360 with north as 360, replies up to
//            about 210 ms while turning.
// The rotctld `P` number format (two decimals, C locale) and "an RPRT
// line is never a position" follow Longpath src/core/RotctldClient.cpp
// [@551576e] (reference, not ported).
//
// Design: docs/architecture/2026-10-07-rotor-control-design.md (Core).
// Contract: docs/architecture/2026-10-07-remote-rotor-control-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code. Rotor control plan, Task 3b.
//   2026-10-08: Bench fix: a GS-232 link is connected only after its
//               first position reply, and a fault when none comes. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
#include "core/RotorRoute.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

#include <deque>
#include <functional>
#include <memory>

namespace NereusSDR {

class RotctldProcess;

// The wire's `driver` enum (remote rotor control v1). Values are only
// ever appended.
enum class RotorDriver {
    None           = 0,
    Gs232a         = 1,   // serial
    Gs232b         = 2,   // serial
    Rotctld        = 3,   // Hamlib rotctld already running (host, port)
    RotctldStarted = 4,   // rotctld started by the Core on its loopback
};

// The wire's `axes` enum.
enum class RotorAxes {
    Azimuth          = 0,
    AzimuthElevation = 1,
};

// The wire's `nudgeRotor` direction enum.
enum class RotorDirection {
    Ccw  = 0,
    Cw   = 1,
    Down = 2,
    Up   = 3,
};

// Everything the connection needs. Defaults are the contract's
// (remote rotor control v1, Core-owned settings).
struct RotorConfig {
    RotorDriver driver{RotorDriver::None};
    QString     serialPort;        // GS-232 drivers and driver 4
    int         baud{9600};
    QString     host;              // driver 3
    quint16     port{4533};        // driver 3, and driver 4's preferred port
    int         hamlibModel{0};    // driver 4
    RotorAxes   axes{RotorAxes::Azimuth};
    RotorRoute::EndStop endStop{RotorRoute::EndStop::North};
    double      rangeDeg{RotorRoute::kFullTurnDeg};
    double      offsetDeg{0.0};
};

// Where the transport goes: a serial port, or a TCP host and port.
struct RotorTransportTarget {
    bool    serial{false};
    QString serialPort;
    int     baud{9600};
    QString host;
    quint16 port{0};
};

// The byte stream under the connection: a serial port or a TCP socket in
// production, a fake in tests (no test opens a real serial port or
// starts the real rotctld).
class NEREUS_CORE_EXPORT RotorTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~RotorTransport() override = default;

    // Starts opening; ends in opened() or failed().
    virtual void open() = 0;
    virtual void close() = 0;
    virtual qint64 write(const QByteArray& bytes) = 0;
    virtual QByteArray readAll() = 0;

signals:
    void opened();
    void failed(const QString& reason);
    void readyRead();
    // An open link dropped (not emitted for close()).
    void dropped(const QString& reason);
};

class NEREUS_CORE_EXPORT RotorConnection : public QObject {
    Q_OBJECT
public:
    // 1500 ms without a position reply marks the heading stale (design,
    // "Fresh or stale"; contract `positionFresh`).
    static constexpr int kStaleMs = 1500;
    // Poll about once a second when still, faster while turning. The ERC
    // took up to about 210 ms to answer while turning, and a poll is
    // never sent while one is outstanding.
    static constexpr int kPollStillMs = 1000;
    static constexpr int kPollTurningMs = 250;
    // A poll unanswered this long is given up (serial) or the link is cut
    // and dialled again (rotctld, as Longpath's reply watchdog).
    static constexpr int kReplyTimeoutMs = 1000;
    // From Longpath src/core/RotctldClient.cpp:30 [@551576e]
    // (design, "Target and arrival").
    static constexpr double kArrivedDeg = 1.5;
    // A heading that has not changed for this long has stopped (the rotor
    // has arrived, hit a stop, or coasted to rest after `S`). The ERC
    // started moving within a second of a command.
    static constexpr int kSettleMs = 2000;
    // Bench fix: a GS-232 serial link is connected only once a position
    // reply parses. An open port that has not answered one this long (three
    // polls) is a fault (the wrong port, the wrong baud, or a controller
    // that is off), and it is dialled again on the reconnect schedule.
    static constexpr int kAnswerDeadlineMs = 3000;
    // rotctld(1) default port.
    static constexpr quint16 kRotctldDefaultPort = 4533;
    // rotctld `M` speed (rotctld(1): 1 to 100).
    static constexpr int kRotctldMoveSpeed = 100;

    using TransportFactory =
        std::function<std::unique_ptr<RotorTransport>(const RotorTransportTarget&)>;

    explicit RotorConnection(QObject* parent = nullptr);
    ~RotorConnection() override;

    // New setup. Takes effect on the next connectToRotor(); the end stop,
    // range and offset reset the span tracker at once.
    void configure(const RotorConfig& config);
    const RotorConfig& config() const { return m_config; }

    // Opens the link for the configured driver. False (with
    // connectionFailed) for driver none or when driver 4 cannot start
    // rotctld (RotctldProcess::notInstalledReason() when it is missing).
    // A failed or dropped link is dialled again on the 1, 2, 5, 10, 30,
    // 60 s schedule the other accessories use.
    bool connectToRotor();
    // Closes the link, stops retrying and stops a rotctld it started.
    void disconnectFromRotor();

    // A GS-232 link is connected once a position reply has parsed; while
    // its port is open with no reply yet it is still connecting.
    bool isConnected() const { return m_connected; }
    bool reconnectPending() const { return m_reconnectTimer.isActive(); }
    QString lastError() const { return m_lastError; }

    // A position reply within the last kStaleMs.
    bool positionFresh() const { return m_positionFresh; }
    // Compass heading after the offset, 0 to under 360; -1 before the
    // first reply and off connected. A stale heading keeps the last value.
    double azimuthDeg() const;
    // 0 to 90 on an az/el rotor; -1 on an azimuth rotor or when unknown.
    double elevationDeg() const;
    // Where the rotor is on its span (RotorRoute::SpanTracker); -1 when
    // unknown or with no end stop.
    double spanPositionDeg() const { return m_tracker.spanPositionDeg(); }
    bool   spanKnown() const { return m_tracker.spanKnown(); }
    const RotorRoute::SpanTracker& tracker() const { return m_tracker; }

    // The target being turned to, after the offset; -1 when none.
    double targetAzimuthDeg() const { return m_hasTarget ? m_targetAz : -1.0; }
    double targetElevationDeg() const { return m_hasTarget ? m_targetEl : -1.0; }
    // True from a set or a move until the rotor has arrived or stopped
    // (and after a stop until it has coasted to rest). Polls are faster
    // while it is.
    bool turning() const { return m_turning; }
    // A move (stop wins over it) is in progress.
    bool moveActive() const { return m_moveActive; }

    // The port the started rotctld listens on (driver 4), else 0.
    quint16 rotctldPort() const;
    RotctldProcess* rotctldProcess() const { return m_rotctld.get(); }

    // Turn to a compass heading (strict: not a number, below 0 or above
    // 360 is refused, 360 is north). `elevationDeg` -1 leaves elevation;
    // anything else is refused on an azimuth rotor and outside 0 to 90.
    // False, with nothing sent, when refused or not connected.
    bool setTarget(double azimuthDeg, double elevationDeg = -1.0);
    // Stop now: written ahead of anything queued, and queued turns are
    // dropped. Nothing happens off connected.
    void stop();
    // Start turning in one direction until stop(). Up and down are refused
    // on an azimuth rotor. False, with nothing sent, when refused or not
    // connected.
    bool startMove(RotorDirection direction);

    // ── Exact bytes, for the tests ──────────────────────────────────
    static QByteArray pollCommand(RotorDriver driver);
    static QByteArray setCommand(RotorDriver driver, double azimuthDeg,
                                 double elevationDeg);
    static QByteArray stopCommand(RotorDriver driver);
    static QByteArray moveCommand(RotorDriver driver, RotorDirection direction);

    // ── Test seams ──────────────────────────────────────────────────
    void setTransportFactoryForTesting(TransportFactory factory);
    // Compresses the timing; production keeps the constants above.
    struct Timing {
        int pollStillMs{kPollStillMs};
        int pollTurningMs{kPollTurningMs};
        int replyTimeoutMs{kReplyTimeoutMs};
        int settleMs{kSettleMs};
        int staleMs{kStaleMs};
        int answerDeadlineMs{kAnswerDeadlineMs};
        int reconnectUnitMs{1000};
        int rotctldStartDelayMs{500};
    };
    void setTimingForTesting(const Timing& timing);
    // One poll tick now (a poll unless one is outstanding or a command is
    // queued), so a test steps the exchange instead of racing a timer.
    void pollNowForTesting() { onPollTick(); }
    // The poll timer's current interval.
    int pollIntervalMs() const { return m_pollTimer.interval(); }

signals:
    void connected();
    // The link closed or dropped after connected().
    void disconnected();
    // Opening failed, the link dropped, a started rotctld exited, or a
    // GS-232 controller never answered; the reason in plain words.
    void connectionFailed(const QString& reason);
    void reconnectScheduled(int attempt, int delayMs);
    // Every accepted position reply.
    void positionUpdated();
    void positionFreshChanged(bool fresh);
    void turningChanged(bool turning);
    // rotctld answered `RPRT n` with n not 0.
    void rotorError(int code, const QString& reason);

private:
    enum class Expect { Nothing, Position, Report };
    struct Outgoing {
        QByteArray bytes;
        Expect     expect{Expect::Nothing};
        bool       motion{false};   // dropped by stop()
    };

    bool isSerialDriver() const;
    bool isRotctldDriver() const;
    RotorDriver wireDriver() const;   // driver 4 speaks as driver 3
    void openTransport();
    void closeTransport();
    void onOpened();
    void onTransportFailed(const QString& reason);
    void onDropped(const QString& reason);
    void onReadyRead();
    void parseGs232();
    void parseRotctld();
    void acceptPosition(double reportedAz, double reportedEl, bool haveEl);
    void enqueue(const Outgoing& out);
    void writeNow(const Outgoing& out);
    void pump();
    void onPollTick();
    void onReplyTimeout();
    void onStale();
    void onAnswerDeadline();
    static QString notAnsweringReason(const QString& serialPort);
    void scheduleReconnect();
    void onReconnectTimeout();
    void setTurning(bool turning);
    void updatePollInterval();
    void resetPosition();
    void fail(const QString& reason);
    bool startRotctld(QString* error);

    RotorConfig m_config;
    Timing      m_timing;
    TransportFactory m_factory;
    std::unique_ptr<RotorTransport> m_transport;
    std::unique_ptr<RotctldProcess> m_rotctld;

    QTimer m_pollTimer;
    QTimer m_replyTimer;
    QTimer m_staleTimer;
    QTimer m_reconnectTimer;
    QTimer m_dialTimer;          // driver 4: rotctld's moment to bind
    QTimer m_answerTimer;        // GS-232: the first reply's deadline
    int    m_reconnectAttempts{0};
    bool   m_wantConnected{false};
    bool   m_connected{false};
    bool   m_linkOpen{false};    // transport open (polls go out)
    QString m_lastError;

    QByteArray m_rx;
    std::deque<Outgoing> m_queue;      // not yet written
    std::deque<Expect>   m_inFlight;   // written, reply awaited, in order

    RotorRoute::SpanTracker m_tracker;
    bool   m_positionFresh{false};
    bool   m_haveAzimuth{false};
    double m_azimuthDeg{-1.0};
    double m_elevationDeg{-1.0};

    bool   m_hasTarget{false};
    double m_targetAz{0.0};
    double m_targetEl{-1.0};
    bool   m_moveActive{false};
    bool   m_turning{false};
    double m_lastChangeHeading{-1.0};
    qint64 m_lastChangeMs{0};
};

} // namespace NereusSDR
