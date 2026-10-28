#pragma once

// =================================================================
// src/core/RadioDiscovery.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   HPSDR/clsRadioDiscovery.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-28 - PR 339 detection-only containment and regression coverage;
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-10-12 - RadioInfo keeps the second firmware number a discovery
//                 reply carries (firmwareBetaVersion) and words the two
//                 in one place (firmwareText). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-22 - The NIC walk no longer blocks the thread that starts it:
//                 it runs from the event loop on a poll-window timer and
//                 the socket's readyRead, its quiet-poll counter follows
//                 Thetis, and each attempt has a hard deadline (a NereusSDR
//                 addition). From Longpath PRs 9, 10, 251 and 267 by Martin
//                 Fischer (OE5SOS). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-22 - m_scanHoldOffTimer: a running walk's next attempt waits
//                 out the post-disconnect quiet period. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-22 - isScanRunning() and walkBoundMs() for the startup
//                 auto-connect wait (PinnedRadioWait). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-22 - Connect hold: beginConnectHold() / endConnectHold()
//                 pause discovery while a local connection starts. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  clsRadioDiscovery.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "core/NereusCoreExport.h"
/*   Additions to detect G2 XDMA:
 *   Copyright (C) 2026 -  Martinus Stroomer, CT1IQI
 *
 * Purpose: This source code supports use of the xdma interface on a Saturn board,
 * as used in the Apache-Labs 'Anan G2' Software Defined Radio (SDR).
 *
 * Credits:
 * The documentation provided by the designers of Saturn was used.
 * Code written in C for the G2 in the applications P2app and piHPSDR has
 * been extensively re-used and adapted to fit the Nereus-SDR application.   
 * Main authors of Saturn's SDR design, documentation, and code support in
 * p2app and piHPSDR applications are:   
 * Laurence Barker, G8NJJ
 * Rick Koch, N1GP
 * John Melton, G0ORX
 * Christoph van Wüllen, DL1YCF.
 */

#include "HpsdrModel.h"

#include <QObject>
#include <QDeadlineTimer>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QMutex>
#include <QMetaType>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <optional>

namespace NereusSDR {

// Protocol version supported by the radio.
enum class ProtocolVersion : int {
    Protocol1 = 1,
    Protocol2 = 2,
    G2XDMA = 3
};

enum class IfaceVersion : int {
    IP = 0,
    XDMA = 1
};

// Information about a discovered OpenHPSDR radio.
struct RadioInfo {
    // Identity
    QString name;                        // User-friendly name, e.g. "ANAN-G2"
    QString macAddress;                  // MAC as "AA:BB:CC:DD:EE:FF" (primary key)
    QHostAddress address;                // IP address on local network
    quint16 port{1024};                  // Always 1024 for OpenHPSDR

    // Hardware
    HPSDRHW boardType{HPSDRHW::Unknown};
    IfaceVersion iface{IfaceVersion::IP};
    int pcbVersion{0};
    int firmwareVersion{0};
    // The second firmware number the radio reported in discovery, or 0 when
    // it reported none (a zero byte, a short reply, a board with no such
    // byte, a radio typed in by hand). Thetis's BetaVersion: byte 23 of a
    // Protocol 2 reply, and byte 21 of an HL2's Protocol 1 reply (mi0bot).
    // The Anvelina Pro 3 sends the third number of its release there (14
    // for 2.2.14) and the HL2 its gateware's patch number. The ANAN-G2
    // sends the version of its p2app program instead, which firmwareText()
    // leaves out.
    int firmwareBetaVersion{0};
    int adcCount{1};                     // Derived from boardType (1 or 2)
    int maxReceivers{4};                 // Board-dependent max simultaneous RX
    // The receiver count the radio itself reported in discovery byte 20,
    // or 0 when it reported none (a zero byte, a short reply, a radio typed
    // in by hand or restored from the saved list). maxReceivers holds the
    // table fallback in that case; this keeps the two apart so the stream
    // pool can follow the radio's own number (Protocol 2).
    int reportedReceivers{0};

    // Protocol
    ProtocolVersion protocol{ProtocolVersion::Protocol1};
    bool inUse{false};                   // Another client already connected

    // Capabilities (derived from boardType at parse time)
    bool hasDiversityReceiver{false};    // 2-ADC boards support diversity
    bool hasPureSignal{false};           // Boards supporting PA linearization
    int maxSampleRate{384000};           // Max supported sample rate in Hz

    // Model override — set by user in ConnectionPanel, persisted per-MAC.
    // When set to a value other than FIRST, overrides the auto-detected model.
    HPSDRModel modelOverride{HPSDRModel::FIRST}; // FIRST = no override (auto-detect)

    // Display helpers
    QString displayName() const;
    QString connectionUnavailableReason() const;
    // The firmware as every place that shows it words it: the number the
    // radio reported, then a dot and its second number when it reported one
    // ("22.14"). A radio that reported no second number reads as the first
    // alone ("22").
    QString firmwareText() const;
    static int adcCountForBoard(HPSDRHW type);
    static int maxReceiversForBoard(HPSDRHW type);

    // Comparison — radios are identified by MAC address
    bool operator==(const RadioInfo& other) const {
        return macAddress == other.macAddress;
    }
};

// -------------------------------------------------------------------------
// Discovery timing profiles
// -------------------------------------------------------------------------
// Source: mi0bot/Thetis@Hermes-Lite clsRadioDiscovery.cs (timing guide comment, lines 49-70)
//
// AttemptsPerNic = how many send/wait cycles per NIC
// QuietPollsBeforeResend = how many consecutive empty polls before retry/give up
// PollTimeoutMs = how long each poll waits (milliseconds)
//
// Effective max wait per NIC ≈ AttemptsPerNic × QuietPollsBeforeResend × PollTimeoutMs
//
// Profile        AttemptsPerNic   QuietPollsBeforeResend   PollTimeoutMs   Approx time per NIC
// -----------    ---------------  ----------------------   -------------   -------------------
// UltraFast      1                2                        40              ~80 ms
// VeryFast       1                3                        60              ~180 ms
// Fast           2                3                        80              ~480 ms
// Balanced       2                4                        100             ~800 ms
// SafeDefault    3                5                        150             ~1800 ms
// VeryTolerant   3                6                        300             ~5400 ms
// -------------------------------------------------------------------------

enum class DiscoveryProfile {
    UltraFast,
    VeryFast,
    Fast,
    Balanced,
    SafeDefault,
    VeryTolerant
};

struct DiscoveryTiming {
    int attemptsPerNic;
    int quietPollsBeforeResend;
    int pollTimeoutMs;
};

// From mi0bot/Thetis@Hermes-Lite clsRadioDiscovery.cs applyScanPerformanceProfile()
constexpr DiscoveryTiming timingFor(DiscoveryProfile p) noexcept {
    switch (p) {
        case DiscoveryProfile::UltraFast:    return {1, 2,  40};
        case DiscoveryProfile::VeryFast:     return {1, 3,  60};
        case DiscoveryProfile::Fast:         return {2, 3,  80};
        case DiscoveryProfile::Balanced:     return {2, 4, 100};
        case DiscoveryProfile::SafeDefault:  return {3, 5, 150};
        case DiscoveryProfile::VeryTolerant: return {3, 6, 300};
    }
    return {3, 5, 150};  // SafeDefault fallback
}

// Discovers OpenHPSDR radios on the local network.
// Both P1 and P2 radios respond to UDP broadcasts on port 1024.
// Uses the mi0bot/Thetis NIC-walk + tunable timing-profile pattern:
// bind per NIC, send P1+P2 probes, poll up to attemptsPerNic×quietPollsBeforeResend×pollTimeoutMs.
//
// The walk runs from the owning thread's event loop (2026-10-22, from
// Longpath PR 251): startDiscovery() emits discoveryStarted and returns at
// once, replies arrive as radioDiscovered / radioUpdated while the walk
// goes on, and discoveryFinished marks its end. The owning thread must run
// an event loop (the GUI thread does; the Core's discovery worker runs a
// QEventLoop until discoveryFinished).
class NEREUS_CORE_EXPORT RadioDiscovery : public QObject {
    Q_OBJECT

public:
    // One NIC the walk probes: the local IPv4 address the walk's socket
    // binds, the directed subnet broadcast (null when the NIC has none),
    // whether 255.255.255.255 is probed too, and the destination port.
    // A production walk builds these from QNetworkInterface; tests may
    // supply their own (setScanNicsForTest) to keep every probe on the
    // loopback.
    struct ScanNic {
        QString      name;
        QHostAddress local;
        QHostAddress directed;
        bool         generalBroadcast{true};
        quint16      port{1024};
    };

    // Counts from the most recent walk, for tests and logging. A quiet
    // window is one poll window that ended with nothing readable; a
    // readable window is one that a datagram ended early.
    struct ScanStats {
        int quietPolls{0};
        int readableWindows{0};
        int datagrams{0};
        int attempts{0};
        int deadlineEnds{0};
    };

    explicit RadioDiscovery(QObject* parent = nullptr);
    ~RadioDiscovery() override;

    void startDiscovery();
    void stopDiscovery();

    // True from the start of a walk until its discoveryFinished. A
    // startDiscovery() deferred by the quiet period is not running yet.
    bool isScanRunning() const { return m_scanRunning; }
    // The longest the running walk can still take, in ms: the quiet period
    // left plus every NIC's attempts at their hard deadline. 0 when no walk
    // runs. PinnedRadioWait sizes its backstop from it.
    qint64 walkBoundMs() const;

    // Post-disconnect quiet period (2026-07-27, ANAN-G2E lockup).
    //
    // Wire captures show NereusSDR fires a broadcast discovery burst 7-15 ms
    // after sending run=0 (disconnect reopens the ConnectionPanel, whose ctor
    // auto-scans) and both observed G2E lockups happened inside that window,
    // while Thetis goes completely silent after its stop frame and never
    // wedges the same radio.  Probing a radio while its stop-transition state
    // machines are still settling is a race Thetis never enters.
    //
    // Calling holdOffScans() defers — never drops — any startDiscovery() or
    // probeAddress() issued before the deadline: the work runs when the quiet
    // period expires, so the panel still populates and Connect flows still
    // probe, just after the radio has finished stopping.
    void holdOffScans(std::chrono::milliseconds quiet);
    qint64 holdOffRemainingMs() const;

    // Connect hold (2026-10-22). The walk runs from the event loop, so a
    // local connection can start while it is still broadcasting on later
    // NICs: the startup auto-connect does so every time. A radio that is
    // starting up is in the same window as one that is stopping (the
    // 2026-07-27 ANAN-G2E lockups), so RadioModel holds discovery from the
    // moment it creates a connection until that connection reports
    // Connected (its first I/Q frame) or ends, and then arms the quiet
    // period above. While any hold is up, holdOffRemainingMs() reports at
    // least kConnectHoldRecheckMs, so every path that already waits out the
    // quiet period (startDiscovery(), probeAddress(), a running walk's next
    // attempt, the Core's scans) waits for the hold too, and re-checks.
    // Process-wide, like the quiet period; keyed by owner so a second
    // begin is a no-op and an end with no begin arms nothing.
    static void beginConnectHold(const void* owner);
    static void endConnectHold(const void* owner, std::chrono::milliseconds quiet);
    static bool connectHoldActive();
    static constexpr qint64 kConnectHoldRecheckMs = 100;

    DiscoveryProfile profile() const { return m_profile; }
    void setProfile(DiscoveryProfile profile) { m_profile = profile; }

    QList<RadioInfo> discoveredRadios() const;

    // Mark a MAC as currently-connected so onStaleCheck will not emit
    // radioLost for it. Once a radio is streaming (P1 ep6 / P2 DDC),
    // it stops replying to discovery broadcasts, so the stale timer
    // would otherwise fire after the discovered-only timeout.
    void setConnectedMac(const QString& mac) { m_connectedMac = mac; }
    void clearConnectedMac() { m_connectedMac.clear(); }

    // Saved-MAC tracking — design §7.4.
    // Radios in this set are never removed by onStaleCheck; they stay in the
    // panel (pill transitions Stale → Offline) but the row is permanent.
    // Call setSavedMacs from ConnectionPanel after seeding from AppSettings.
    void setSavedMacs(const QStringList& macs) { m_savedMacs = QSet<QString>(macs.begin(), macs.end()); }
    void addSavedMac(const QString& mac) { m_savedMacs.insert(mac); }
    void removeSavedMac(const QString& mac) { m_savedMacs.remove(mac); }

#ifdef NEREUS_BUILD_TESTS
    // Test-only hooks — only compiled when NEREUS_BUILD_TESTS is defined.
    // Allow unit tests to inject a stale lastSeen entry and trigger the sweep
    // without needing a real UDP scan. Not part of the public API.
    void injectLastSeenForTest(const QString& mac, const RadioInfo& info, qint64 lastSeenMs) {
        m_radios.insert(mac, info);
        m_lastSeen.insert(mac, lastSeenMs);
    }
    void forceStaleCheckForTest() { onStaleCheck(); }

    // The holdOffScans deadline is process-wide (see s_scanHoldOff), so it
    // survives across test functions and would defer probes in unrelated
    // cases. Call from a QTest init() for a clean slate.
    static void clearHoldOffForTest();   // also drops every connect hold

    // Walk these NICs instead of the machine's interfaces (an empty list
    // restores the real ones), with this timing instead of the profile's.
    void setScanNicsForTest(const QList<ScanNic>& nics) {
        if (nics.isEmpty()) { m_scanNicsOverride.reset(); } else { m_scanNicsOverride = nics; }
    }
    void setTimingForTest(const DiscoveryTiming& timing) { m_timingOverride = timing; }
    ScanStats scanStatsForTest() const { return m_scanStats; }
    bool scanRunningForTest() const { return m_scanRunning; }
    static constexpr int attemptDeadlineFactorForTest() { return kAttemptDeadlineFactor; }
#endif

    // Public static parsers — exposed for unit-testing in Task 5.
    // Both return true on a valid discovery reply and populate 'out'.
    // From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1/P2 branches.
    // Local hardware identity only; the XDMA sample/control backend is unfinished.
    static bool parseG2XDMA(RadioInfo& out);
    static bool parseG2XdmaRegisters(quint32 software, quint32 product, RadioInfo& out);
    static bool parseP1Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);
    static bool parseP2Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);

    // Send a unicast P1+P2 probe in parallel to the given address.
    // On reply: emits radioDiscovered() with parsed RadioInfo.
    // On timeout: emits probeFailed() with the address that was probed.
    //
    // Used by the Add Radio dialog and saved-row Connect path to validate
    // reachability before opening a full connection. Times out fast (1.5 s
    // default) so users get a clear error instead of a UDP-ghost long wait.
    //
    // From Phase 3Q design §7.2.
    void probeAddress(const QHostAddress& addr,
                      quint16 port = 1024,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));

signals:
    void discoveryStarted();
    void discoveryFinished();
    void radioDiscovered(const NereusSDR::RadioInfo& info);
    void radioUpdated(const NereusSDR::RadioInfo& info);
    void radioLost(const QString& macAddress);
    void probeFailed(const QHostAddress& addr, quint16 port);

private slots:
    void onStaleCheck();

private:
    static constexpr quint16 kDiscoveryPort    = 1024;
    static constexpr int kContinuousIntervalMs = 5000;   // re-run NIC walk every 5s during active monitoring
    // Design §7.4: discovered-only radios age out at 60 s (raised from 15 s).
    // Connected MAC and saved MACs are always exempt regardless of this value.
    static constexpr int kDiscoveredOnlyTimeoutMs = 60 * 1000;

    // MAC extraction from raw bytes
    static QString macToString(const char* bytes);

    // Per-NIC scan (mi0bot pattern), driven by the event loop.
    // Sends P1+P2 probes, polls for replies using the timing profile,
    // calls parseP1Reply / parseP2Reply and de-duplicates by MAC.
    // scanAllNics() takes the NIC candidates and starts the first one,
    // then returns; beginNextNic() binds a NIC's socket, beginAttempt()
    // sends the probes and opens the first poll window, onScanPollWindow()
    // closes a quiet window, onScanReadyRead() takes replies,
    // onScanAttemptDeadline() ends an attempt that never went quiet, and
    // finishScan() cleans up and emits discoveryFinished.
    void scanAllNics();
    void beginNextNic();
    void beginAttempt();
    void endAttempt();
    void onScanPollWindow();
    void onScanReadyRead();
    void onScanAttemptDeadline();
    void finishScan();
    void handleScanDatagram(const QByteArray& data, const QHostAddress& sender);

    // Hard limit on one attempt's length, as a multiple of its quiet
    // budget (quietPollsBeforeResend × pollTimeoutMs). NereusSDR addition:
    // Thetis has no wall-clock limit (see onScanAttemptDeadline()).
    static constexpr int kAttemptDeadlineFactor = 2;

    // Walk state. One walk at a time per object; m_scanRunning is true
    // from scanAllNics() until finishScan().
    QList<ScanNic> m_scanNics;
    int  m_scanNicIndex{-1};
    int  m_scanAttempt{0};
    int  m_scanQuietPolls{0};
    bool m_scanRunning{false};
    QPointer<QUdpSocket> m_scanSock;   // child of this object, one per NIC
    QTimer m_scanPollTimer;            // one poll window, single shot
    QTimer m_scanDeadlineTimer;        // one attempt's hard limit, single shot
    QTimer m_scanHoldOffTimer;         // an attempt waiting out holdOffScans, single shot
    QSet<QString> m_scanSeen;          // MAC de-duplication for this walk
    qint64 m_scanNowMs{0};
    QByteArray m_scanP1;
    QByteArray m_scanP2;
    DiscoveryTiming m_scanTiming{timingFor(DiscoveryProfile::SafeDefault)};
    ScanStats m_scanStats;
    std::optional<QList<ScanNic>> m_scanNicsOverride;   // tests only
    std::optional<DiscoveryTiming> m_timingOverride;    // tests only

    DiscoveryProfile m_profile{DiscoveryProfile::SafeDefault};

    QTimer m_continuousTimer;   // drives ongoing NIC re-scans while monitoring
    QTimer m_staleTimer;

    // holdOffScans deadline.  Default-constructed QDeadlineTimer is already
    // expired, which is the "no holdoff armed" state.
    //
    // PROCESS-WIDE, not per-instance (Codex review, PR #306).  The constraint
    // belongs to the radio and the wire, not to any one RadioDiscovery object,
    // and RadioModel::discovery() is not the only instance: e.g.
    // AddCustomRadioDialog.cpp:589 constructs its own.  A per-object deadline
    // would let that dialog probe a just-disconnected radio inside the quiet
    // window and re-enter the post-stop race this exists to prevent.
    //
    // MONOTONIC, not wall-clock (Codex review, PR #306).  This was
    // QDateTime::currentMSecsSinceEpoch() arithmetic, which an NTP step could
    // move underneath us: a forward correction expires the holdoff early and
    // lets a probe land inside the G2E stop-transition window this exists to
    // avoid, and a backward correction defers discovery for as long as the
    // correction was large.  QDeadlineTimer measures against a monotonic
    // source, so clock synchronisation cannot shorten a radio-safety interval
    // or wedge discovery.
    static QDeadlineTimer s_scanHoldOff;
    static QMutex s_scanHoldOffMutex;
    // Owners of a connect hold (beginConnectHold), under the same mutex.
    static QSet<quintptr> s_connectHolds;
    bool scanCancelled() const;

    // Per-instance: stops a burst of startDiscovery() calls on THIS object
    // from queueing multiple delayed scans.
    bool m_deferredScanPending{false};
    QMap<QString, RadioInfo> m_radios;   // keyed by MAC address
    QMap<QString, qint64> m_lastSeen;    // MAC -> timestamp
    QString m_connectedMac;              // MAC currently in use by a RadioConnection; exempt from stale-removal
    QSet<QString>  m_savedMacs;          // Design §7.4: MACs that are saved in AppSettings; exempt from stale-removal

    // Cooperative cancel for scanAllNics(). The synchronous NIC walk does
    // attemptsPerNic × quietPollsBeforeResend × pollTimeoutMs of blocking
    // waitForReadyRead per NIC — up to ~4.5 s for the SafeDefault profile
    // on a 2-NIC machine. stopDiscovery() flips this flag so an in-flight
    // (or queued-by-timer) scan exits within one pollTimeoutMs window
    // instead of blocking shutdown for several seconds. Atomic because
    // stopDiscovery may run from any thread context (main, in practice,
    // but cheap defense-in-depth).
    //
    // Since 2026-10-22 the walk runs from the event loop and blocks nothing:
    // stopDiscovery() ends a running walk at once (finishScan()), and the
    // poll-window and readyRead handlers still check this flag and thread
    // interruption (the Core's discovery worker) on every event.
    std::atomic<bool> m_stopRequested{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioInfo)
Q_DECLARE_METATYPE(NereusSDR::ProtocolVersion)
Q_DECLARE_METATYPE(NereusSDR::DiscoveryProfile)
