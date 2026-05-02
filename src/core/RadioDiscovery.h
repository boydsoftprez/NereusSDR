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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
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

#include "HpsdrModel.h"

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <chrono>

namespace NereusSDR {

// Protocol version supported by the radio.
enum class ProtocolVersion : int {
    Protocol1 = 1,
    Protocol2 = 2
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
    int firmwareVersion{0};
    int adcCount{1};                     // Derived from boardType (1 or 2)
    int maxReceivers{4};                 // Board-dependent max simultaneous RX

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
class RadioDiscovery : public QObject {
    Q_OBJECT

public:
    explicit RadioDiscovery(QObject* parent = nullptr);
    ~RadioDiscovery() override;

    void startDiscovery();
    void stopDiscovery();

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
#endif

    // Public static parsers — exposed for unit-testing in Task 5.
    // Both return true on a valid discovery reply and populate 'out'.
    // From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1/P2 branches.
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

    // Per-NIC synchronous scan (mi0bot pattern).
    // Sends P1+P2 probes, polls for replies using the timing profile,
    // calls parseP1Reply / parseP2Reply and de-duplicates by MAC.
    void scanAllNics();

    DiscoveryProfile m_profile{DiscoveryProfile::SafeDefault};

    QTimer m_continuousTimer;   // drives ongoing NIC re-scans while monitoring
    QTimer m_staleTimer;
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
    std::atomic<bool> m_stopRequested{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioInfo)
Q_DECLARE_METATYPE(NereusSDR::ProtocolVersion)
Q_DECLARE_METATYPE(NereusSDR::DiscoveryProfile)
