#pragma once

// =================================================================
// src/core/RadioDiscovery.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/Thetis-HL2 source:
//   HPSDR/clsRadioDiscovery.cs
//
// Original copyright and license (preserved per GNU GPL):
//
//   Thetis / Thetis-HL2 is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — Thetis heavy modifications
//   Copyright (C) 2020-2026  Reid Campbell (MI0BOT) — Hermes-Lite fork contributions
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions in the upstream Thetis source — preserved verbatim from
// Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "HpsdrModel.h"

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QMetaType>

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
    // would otherwise fire ~15s after every healthy connect.
    void setConnectedMac(const QString& mac) { m_connectedMac = mac; }
    void clearConnectedMac() { m_connectedMac.clear(); }

    // Public static parsers — exposed for unit-testing in Task 5.
    // Both return true on a valid discovery reply and populate 'out'.
    // From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1/P2 branches.
    static bool parseP1Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);
    static bool parseP2Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out);

signals:
    void discoveryStarted();
    void discoveryFinished();
    void radioDiscovered(const NereusSDR::RadioInfo& info);
    void radioUpdated(const NereusSDR::RadioInfo& info);
    void radioLost(const QString& macAddress);

private slots:
    void onStaleCheck();

private:
    static constexpr quint16 kDiscoveryPort   = 1024;
    static constexpr int kContinuousIntervalMs = 5000;   // re-run NIC walk every 5s during active monitoring
    static constexpr int kStaleTimeoutMs       = 15000;

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
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioInfo)
Q_DECLARE_METATYPE(NereusSDR::ProtocolVersion)
Q_DECLARE_METATYPE(NereusSDR::DiscoveryProfile)
