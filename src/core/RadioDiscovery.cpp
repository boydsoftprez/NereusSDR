// =================================================================
// src/core/RadioDiscovery.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   HPSDR/clsRadioDiscovery.cs, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header block does not name them):
//   Reid Campbell (MI0BOT) — HermesLite 2 board-ID 6 discovery mapping
//     (preserved via inline marker on case 6 branch in parseDiscoveryReply)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-25 - Receiver and transmit gaps plan, Task 5: a Protocol 1
//                 reply's top rate follows the board (384 kHz for the HL2,
//                 192 kHz for the others) instead of 384 kHz for all.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The HL2's receiver count is discovery byte 19, as mi0bot
//                 reads it (clsRadioDiscovery.cs:1176 [v2.10.3.13-beta2]);
//                 other Protocol 1 boards keep byte 20 (Thetis
//                 clsRadioDiscovery.cs:1166 [v2.10.3.15]). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-12 - The second firmware number a reply carries is read and
//                 logged: byte 23 of a Protocol 2 reply (Thetis
//                 clsRadioDiscovery.cs:1184 [v2.10.3.15]) and byte 21 of
//                 an HL2's Protocol 1 reply (mi0bot
//                 clsRadioDiscovery.cs:1175 [v2.10.3.13-beta2]).
//                 RadioInfo::firmwareText words the two numbers. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - Discovery no longer freezes the thread that starts it. The
//                 NIC walk runs from the event loop (a poll-window timer and
//                 the socket's readyRead in place of waitForReadyRead), the
//                 quiet-poll counter follows Thetis (a readable poll leaves
//                 it as it was, clsRadioDiscovery.cs:964-976 [v2.10.3.15]),
//                 and each attempt has a hard deadline, a NereusSDR addition
//                 since Thetis has none. From Longpath PRs 9 (988fa77e),
//                 10 (2e967e3f), 251 (279ab925) and 267 (1ca46ba2) by Martin
//                 Fischer (OE5SOS). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-22 - A running walk honors the post-disconnect quiet period:
//                 an attempt (and so a NIC's first attempt) that would start
//                 inside it waits for it to end, then sends. The probes are
//                 delayed, never dropped. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-22 - A reply that arrives while an attempt waits out the quiet
//                 period opens no poll window, so it cannot end that
//                 attempt before it sends. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-22 - walkBoundMs(), the longest a running walk can still take,
//                 for the startup auto-connect wait. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-22 - Connect hold: while a local connection starts, every
//                 scan, probe and running walk's next attempt waits, then
//                 the quiet period follows. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

// --- From piHPSDR src/saturnmain.c [@4aa95c5] ---
/* Copyright (C)
*  2021 - Laurence Barker G8NJJ
*  2025 - Christoph van Wüllen, DL1YCF
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

// Modification history (NereusSDR):
//   2026-10-28 - Recover the piHPSDR discovery notice and contain the
//                 unfinished XDMA backend to opt-in, read-only Linux detection.
//                 Original adaptation: Martinus Stroomer (CT1IQI), Codeconvert.ai.
//                 Repair: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//
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

#include "RadioDiscovery.h"
#include <QThread>
#include <QMutexLocker>
#include "BoardCapabilities.h"
#include "HardwareProfile.h"
#include "LogCategories.h"

#include <QDateTime>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif


#if defined(Q_OS_LINUX) && defined(NEREUS_ENABLE_XDMA_DETECTION)
#include <QScopeGuard>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace NereusSDR {

// --- RadioInfo static helpers ---

QString RadioInfo::connectionUnavailableReason() const
{
    if (protocol == ProtocolVersion::G2XDMA) {
        return QStringLiteral("XDMA hardware detected; direct radio connection is not implemented");
    }
    return {};
}

QString RadioInfo::displayName() const
{
    if (!name.isEmpty()) {
        return name;
    }
    return QString::fromLatin1(BoardCapsTable::forBoard(boardType).displayName)
           + " (" + macAddress + ")";
}

QString RadioInfo::firmwareText() const
{
    QString text = QString::number(firmwareVersion);
    // On the ANAN-G2 the second number is the version of the radio's p2app
    // program, not a part of the gateware's number, and Thetis words it
    // apart for that radio:
    //   From Thetis setup.cs:14039-14043 [v2.10.3.15]
    //     case HPSDRModel.ANAN_G2:
    //     case HPSDRModel.ANAN_G2_1K:
    //         if (NetworkIO.BetaVersion >= 39) // added for p2app v39
    //         {
    //             sRet = "fpga(v" + NetworkIO.FWCodeVersion.ToString() + ") p2app(v" + NetworkIO.BetaVersion.ToString() + ")";
    // So a Saturn board's text stays the gateware number alone.
    const bool secondNumberIsAProgramVersion =
        boardType == HPSDRHW::Saturn || boardType == HPSDRHW::SaturnMKII;
    if (firmwareBetaVersion > 0 && !secondNumberIsAProgramVersion) {
        text += QLatin1Char('.') + QString::number(firmwareBetaVersion);
    }
    return text;
}

int RadioInfo::adcCountForBoard(HPSDRHW type)
{
    switch (type) {
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
    case HPSDRHW::SaturnMKII:
        return 2;
    default:
        return 1;
    }
}

int RadioInfo::maxReceiversForBoard(HPSDRHW type)
{
    switch (type) {
    case HPSDRHW::Atlas:        return 3;
    case HPSDRHW::Hermes:       return 4;
    case HPSDRHW::HermesII:     return 4;
    case HPSDRHW::HermesLite:   return 4;
    case HPSDRHW::HermesLiteRxOnly: return 4; // Plan Task 15: the kit is an HL2
                                              // (mi0bot console.cs:8409 HERMESLITE
                                              // P1_rxcount = 4 [v2.10.3.13-beta2])
    case HPSDRHW::HermesC10:    return 4; // ANAN-G2E: HERMES-class single-ADC nrx=4
                                          // [N1GP G2E added; Thetis network.h:425 v2.10.3.15]
    case HPSDRHW::Angelia:      return 7;
    case HPSDRHW::Orion:        return 7;
    case HPSDRHW::OrionMKII:    return 7;
    case HPSDRHW::Saturn:       return 7;
    case HPSDRHW::SaturnMKII:   return 7;
    default:                    return 1;
    }
}

// --- RadioDiscovery ---

// From Thetis clsRadioDiscovery.cs formatNicMac() / equivalent MAC string helper
QString RadioDiscovery::macToString(const char* bytes)
{
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(static_cast<quint8>(bytes[0]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[1]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[2]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[3]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[4]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[5]), 2, 16, QChar('0'))
        .toUpper();
}

RadioDiscovery::RadioDiscovery(QObject* parent)
    : QObject(parent)
{
    // Stale-sweep timer only. The continuous NIC walker that a 3I-4 subagent
    // added was blocking the main thread for 15-20s every 5s — removed. Scans
    // are now one-shot, user-triggered via ConnectionPanel. Async rewrite is
    // a follow-up.
    connect(&m_staleTimer, &QTimer::timeout, this, &RadioDiscovery::onStaleCheck);

    // The async rewrite (2026-10-22, from Longpath PR 251 [@279ab925]): these
    // two timers stand in for the waitForReadyRead() the walk used to block
    // in. A poll window is pollTimeoutMs; PreciseTimer because its ticks
    // measure the profile's documented bound. The deadline timer ends an
    // attempt that never goes quiet.
    m_scanPollTimer.setSingleShot(true);
    m_scanPollTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_scanPollTimer, &QTimer::timeout, this, &RadioDiscovery::onScanPollWindow);
    m_scanDeadlineTimer.setSingleShot(true);
    connect(&m_scanDeadlineTimer, &QTimer::timeout, this, &RadioDiscovery::onScanAttemptDeadline);
    // An attempt that met the post-disconnect quiet period waits on this
    // timer and starts again when it fires (see beginAttempt()).
    m_scanHoldOffTimer.setSingleShot(true);
    m_scanHoldOffTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_scanHoldOffTimer, &QTimer::timeout, this, &RadioDiscovery::beginAttempt);
}

RadioDiscovery::~RadioDiscovery()
{
    stopDiscovery();
}

// Process-wide quiet deadline — see the declaration for why this is not
// per-instance, and why it is monotonic rather than wall-clock.
QDeadlineTimer RadioDiscovery::s_scanHoldOff;
QMutex RadioDiscovery::s_scanHoldOffMutex;
QSet<quintptr> RadioDiscovery::s_connectHolds;

bool RadioDiscovery::scanCancelled() const
{
    return m_stopRequested.load(std::memory_order_acquire)
        || QThread::currentThread()->isInterruptionRequested();
}

#ifdef NEREUS_BUILD_TESTS
void RadioDiscovery::clearHoldOffForTest()
{
    const QMutexLocker lock(&s_scanHoldOffMutex);
    s_scanHoldOff = QDeadlineTimer();
    s_connectHolds.clear();
}
#endif

void RadioDiscovery::beginConnectHold(const void* owner)
{
    const QMutexLocker lock(&s_scanHoldOffMutex);
    s_connectHolds.insert(reinterpret_cast<quintptr>(owner));
}

void RadioDiscovery::endConnectHold(const void* owner, std::chrono::milliseconds quiet)
{
    const QMutexLocker lock(&s_scanHoldOffMutex);
    if (!s_connectHolds.remove(reinterpret_cast<quintptr>(owner))) {
        return;
    }
    // The quiet period starts when the hold ends, as holdOffScans() keeps
    // the later deadline.
    const QDeadlineTimer candidate(quiet, Qt::PreciseTimer);
    if (candidate > s_scanHoldOff) {
        s_scanHoldOff = candidate;
    }
}

bool RadioDiscovery::connectHoldActive()
{
    const QMutexLocker lock(&s_scanHoldOffMutex);
    return !s_connectHolds.isEmpty();
}

void RadioDiscovery::holdOffScans(std::chrono::milliseconds quiet)
{
    // Keep whichever deadline is later so a short holdoff can never pull in a
    // longer one already in flight.  Qt::PreciseTimer because this bounds a
    // radio-safety interval, not a UI refresh.
    const QDeadlineTimer candidate(quiet, Qt::PreciseTimer);
    const QMutexLocker lock(&s_scanHoldOffMutex);
    if (candidate > s_scanHoldOff) {
        s_scanHoldOff = candidate;
    }
}

// Remaining quiet time, 0 when scans may run now.  See holdOffScans() decl
// for the ANAN-G2E rationale.
qint64 RadioDiscovery::holdOffRemainingMs() const
{
    // remainingTime() is monotonic and already clamps to 0 once expired; the
    // guard covers the -1 "forever" encoding, which we never construct.
    const QMutexLocker lock(&s_scanHoldOffMutex);
    const qint64 remaining = s_scanHoldOff.remainingTime();
    const qint64 quiet = remaining > 0 ? remaining : 0;
    // A connect hold has no deadline of its own: report a short wait, so
    // the caller waits and checks again (see beginConnectHold()).
    if (!s_connectHolds.isEmpty()) {
        return qMax(quiet, kConnectHoldRecheckMs);
    }
    return quiet;
}

void RadioDiscovery::startDiscovery()
{
    if (QThread::currentThread()->isInterruptionRequested()) {
        emit discoveryFinished();
        return;
    }
    // Post-disconnect quiet period: defer, never drop.  One pending deferred
    // scan is enough — the scan that eventually runs walks every NIC anyway.
    if (const qint64 waitMs = holdOffRemainingMs(); waitMs > 0) {
        if (!m_deferredScanPending) {
            m_deferredScanPending = true;
            QTimer::singleShot(int(waitMs), this, [this]() {
                m_deferredScanPending = false;
                startDiscovery();
            });
        }
        return;
    }
    // Phase 3I rewrote discovery to walk all NICs per scan with ephemeral
    // per-NIC sockets (mi0bot clsRadioDiscovery pattern). Main's older
    // single-persistent-m_socket path was replaced entirely in Task 4;
    // main's cross-platform socket header fix (b3c2961) is preserved at
    // the top of this file, which is what scanAllNics() needs when it
    // does its own setsockopt(SO_BROADCAST) per NIC.
    //
    // One walk at a time (2026-10-22). The blocking walk ruled out a second
    // walk on the same object by itself; now a start while a walk runs joins
    // it: the running walk's replies and its discoveryFinished serve both.
    // Two walks on one NIC would take each other's replies.
    if (m_scanRunning) {
        qCDebug(lcDiscovery) << "startDiscovery: a walk is already running; joining it";
        return;
    }
    m_stopRequested.store(false, std::memory_order_release);
    emit discoveryStarted();
    // Starts the walk and returns; the walk goes on from the event loop.
    // discoveryFinished and the stale timer now come from finishScan(). When
    // no NIC is usable finishScan() runs inside this call.
    scanAllNics();
}

void RadioDiscovery::stopDiscovery()
{
    // Cooperative cancel for any scanAllNics() currently in flight (or
    // queued via a recently-fired m_staleTimer). Without this flip the
    // SafeDefault profile blocks shutdown for ~4.5 s on a 2-NIC machine
    // while the scan completes its attempts × pollTimeoutMs walk; with
    // it the scan exits within one pollTimeoutMs window. m_staleTimer.stop()
    // alone isn't enough — already-queued QTimer events still execute
    // after the close path returns to the event loop.
    m_stopRequested.store(true, std::memory_order_release);
    m_staleTimer.stop();
    // A running walk ends now (timers stopped, socket closed) and
    // finishScan() emits discoveryFinished for it, once.
    if (m_scanRunning) {
        finishScan();
        return;
    }
    emit discoveryFinished();
}

qint64 RadioDiscovery::walkBoundMs() const
{
    if (!m_scanRunning) {
        return 0;
    }
    // The same bound as the attempt deadline in beginAttempt(): NICs ×
    // attempts × kAttemptDeadlineFactor × quietPollsBeforeResend ×
    // pollTimeoutMs, counted over every NIC of the walk.
    const qint64 perAttempt = qint64(kAttemptDeadlineFactor)
        * qMax(1, m_scanTiming.quietPollsBeforeResend)
        * qMax(10, m_scanTiming.pollTimeoutMs);
    const qint64 attempts = qint64(qMax(1, int(m_scanNics.size())))
        * qMax(1, m_scanTiming.attemptsPerNic);
    return holdOffRemainingMs() + attempts * perAttempt;
}

QList<RadioInfo> RadioDiscovery::discoveredRadios() const
{
    return m_radios.values();
}

// From piHPSDR src/saturnmain.c:310-370 [@4aa95c5].
// Only decode identity. Unlike upstream saturn_discovery, do not initialize
// registers, advertise a working data path, or select this backend for a connection.
bool RadioDiscovery::parseG2XdmaRegisters(quint32 software, quint32 product, RadioInfo& out)
{
    // From piHPSDR src/saturnmain.c:310-315 [@4aa95c5].
    constexpr quint32 kSaturnProductId = 1; // Saturn, any version
    constexpr quint32 kGoldenConfigId = 3; // "golden" configuration id
    constexpr quint32 kPrimaryConfigId = 4; // "primary" configuration id
    // From piHPSDR src/saturnmain.c:335-350 [@4aa95c5].
    // All version info is contained in two 32-bit string SI and PI, to be read
    // from two FPGA registers. The layout is
    //
    //     FPGA_MajorVersion  SW[31:25], only valid if MinorVersion >= 18
    //     FPGA_SWID          SW[24:20]
    //     FPGA_MinorVersion  SW[19:4]
    //     FPGA_ClockInfo     SW[3:0]
    //     FPGA_ProdID        PR[31:16]
    //     Saturn_PCB_Version PR[15:0]
    //
    constexpr quint32 kClockMask = 0xF; // 4 clock bits
    constexpr quint32 kSoftwareIdMask = 0x1F; //  5 bit software ID
    constexpr quint32 kVersionMask = 0xFFFF; // 16 bit product ID
    constexpr int kSoftwareIdShift = 20;
    constexpr int kProductShift = 16;
    constexpr int kMinorShift = 4;
    const quint32 softwareId = (software >> kSoftwareIdShift) & kSoftwareIdMask;
    if ((product >> kProductShift) != kSaturnProductId
        || (softwareId != kGoldenConfigId && softwareId != kPrimaryConfigId)
        || (software & kClockMask) != kClockMask) { // not all clocks are present
        return false;
    }
    RadioInfo detected;
    detected.protocol = ProtocolVersion::G2XDMA;
    detected.iface = IfaceVersion::XDMA;
    detected.boardType = HPSDRHW::Saturn;
    detected.pcbVersion = int(product & kVersionMask); // 16 bit board ID
    detected.firmwareVersion = int((software >> kMinorShift) & kVersionMask); // 16 bit minor sw version
    detected.name = QString::fromLatin1(BoardCapsTable::forBoard(detected.boardType).displayName);
    // Preserve CT1IQI's synthetic local identity, not the radio's Ethernet MAC.
    detected.macAddress = QStringLiteral("41:6E:61:6E:47:32");
    detected.address = QHostAddress::AnyIPv4;
    detected.port = 0;
    out = detected;
    return true;
}

bool RadioDiscovery::parseG2XDMA(RadioInfo& out)
{
#if defined(Q_OS_LINUX) && defined(NEREUS_ENABLE_XDMA_DETECTION)
    // From piHPSDR src/saturnmain.c:319-323 [@4aa95c5]: discover only
    // the local XDMA character device. No shell command or radio writes.
    struct stat deviceStat {};
    if (::stat("/dev/xdma0_user", &deviceStat) != 0 || !S_ISCHR(deviceStat.st_mode)) {
        return false;
    }
    // NereusSDR detection only: upstream saturndrivers.c:72-94 opens O_RDWR
    // for later control. A scoped O_RDONLY descriptor cannot write registers.
    const int device = ::open("/dev/xdma0_user", O_RDONLY | O_CLOEXEC);
    if (device < 0) {
        qCWarning(lcDiscovery) << "Cannot read local XDMA hardware identity";
        return false;
    }
    const auto closeDevice = qScopeGuard([device]() { ::close(device); });
    // From piHPSDR src/saturnmain.c:314-315,344-345 [@4aa95c5].
    constexpr off_t kSoftwareRegister = 0xC000; // user defined s/w version register
    constexpr off_t kProductRegister = 0xC004; // user defined product version register
    quint32 software = 0;
    quint32 product = 0;
    if (::pread(device, &software, sizeof(software), kSoftwareRegister) != sizeof(software)
        || ::pread(device, &product, sizeof(product), kProductRegister) != sizeof(product)) {
        qCWarning(lcDiscovery) << "Cannot read complete local XDMA version registers";
        return false;
    }
    // Same native uint32 register reads as piHPSDR saturndrivers.c:144-153
    // [@4aa95c5]; no firmware writes or initialization are performed.
    return parseG2XdmaRegisters(software, product, out);
#else
    Q_UNUSED(out);
    return false;
#endif
}

// ---------------------------------------------------------------------------
// parseP1Reply — extract RadioInfo from a P1 discovery response.
// From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1 branch (line ~1155).
//
// P1 reply layout (OpenHPSDR Protocol 1):
//   [0]    0xEF
//   [1]    0xFE
//   [2]    0x02 (available) or 0x03 (in use)
//   [3..8] MAC address (6 bytes)
//   [9]    firmware version (CodeVersion)
//   [10]   board type byte (mapP1DeviceType)
//   ... (optional extra fields depending on board)
// ---------------------------------------------------------------------------
bool RadioDiscovery::parseP1Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out)
{
    // From Thetis: data[0]==0xef && data[1]==0xfe && (data[2]==0x2 || data[2]==0x3)
    if (bytes.size() < 11) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(bytes[0]);
    const quint8 b1 = static_cast<quint8>(bytes[1]);
    const quint8 b2 = static_cast<quint8>(bytes[2]);

    if (b0 != 0xEF || b1 != 0xFE || (b2 != 0x02 && b2 != 0x03)) {
        return false;
    }

    out = RadioInfo{};
    out.protocol  = ProtocolVersion::Protocol1;
    out.inUse     = (b2 == 0x03);
    out.port      = 1024;

    // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
    bool ok = false;
    quint32 ipv4 = source.toIPv4Address(&ok);
    out.address = ok ? QHostAddress(ipv4) : source;

    // MAC: bytes 3-8
    out.macAddress = macToString(bytes.constData() + 3);

    // From Thetis: r.CodeVersion = data[9]
    out.firmwareVersion = static_cast<quint8>(bytes[9]);

    // From Thetis: r.DeviceType = mapP1DeviceType(data[10])
    // From Thetis ChannelMaster/network.h:420-425 [v2.10.3.15] — upstream enum context:
    //   HermesLite = 6,     // MI0BOT
    //   Saturn = 10,        // ANAN-G2: added G8NJJ
    //   HermesC10 = 20      // ANAN-G2E //N1GP G2E added (HermesC10)
    // mapP1DeviceType: 0=Atlas, 1=Hermes, 2=HermesII, 4=Angelia, 5=Orion,
    //   6=HermesLite, 10=OrionMKII, 20=HermesC10
    quint8 boardByte = static_cast<quint8>(bytes[10]);
    switch (boardByte) {
    case 0:  out.boardType = HPSDRHW::Atlas;      break;
    case 1:  out.boardType = HPSDRHW::Hermes;     break;
    case 2:  out.boardType = HPSDRHW::HermesII;   break;
    case 4:  out.boardType = HPSDRHW::Angelia;    break;
    case 5:  out.boardType = HPSDRHW::Orion;      break;
    case 6:  out.boardType = HPSDRHW::HermesLite; break;  // MI0BOT: HL2 added [Thetis clsRadioDiscovery.cs:1239]
    case 10: out.boardType = HPSDRHW::OrionMKII;  break;
    // network.h:423 upstream context: Saturn = 10  //G8NJJ (ANAN-G2 added by G8NJJ)
    case 20: out.boardType = HPSDRHW::HermesC10;  break;  // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
    default: out.boardType = static_cast<HPSDRHW>(boardByte); break;
    }

    // Optional extra fields (len > 20) — From Thetis parseDiscoveryReply P1 branch
    //
    // The receiver count's byte depends on the board. The HL2 carries it in
    // byte 19; mi0bot reads it there in the HL2 case of its device switch:
    //   From mi0bot clsRadioDiscovery.cs:1169-1176 [v2.10.3.13-beta2]
    //     case HPSDRHW.HermesLite:
    //         byte[] fixedIp = new byte[4];                   // MI0BOT: Extra info from discovery for HL2
    //         ...
    //         r.BetaVersion = data[21];
    //         r.NumRxs = data[19];
    // The bench capture's HL2 reply has byte 19 = 0x04 and byte 20 = 0x45
    // (docs/protocols/openhpsdr-protocol1-capture-reference.md section 2.2),
    // so byte 20 is not a receiver count on the HL2. (mi0bot's later len > 20
    // block writes data[20] over NumRxs, which only its radio list shows;
    // NereusSDR takes the HL2 case's byte, as the MI0BOT comment intends.)
    // Every other Protocol 1 board carries it in byte 20:
    //   From Thetis clsRadioDiscovery.cs:1166 [v2.10.3.15]
    //     r.NumRxs = data[20];
    // 0 means no report and keeps the board's own count.
    if (bytes.size() > 20) {
        const int countByte = (out.boardType == HPSDRHW::HermesLite) ? 19 : 20;
        out.reportedReceivers = static_cast<quint8>(bytes[countByte]);
        out.maxReceivers = out.reportedReceivers > 0
            ? out.reportedReceivers
            : RadioInfo::maxReceiversForBoard(out.boardType);
    }

    // The HL2 also carries its gateware's patch number, in byte 21 (the
    // mi0bot HL2 case quoted above: r.BetaVersion = data[21]). The bench
    // capture's HL2 reply has byte 9 = 0x4a and byte 21 = 0x02, gateware
    // 74.2. No other Protocol 1 board has a second number:
    //   From Thetis clsRadioDiscovery.cs:1156 [v2.10.3.15]
    //     r.BetaVersion = 0;
    if (out.boardType == HPSDRHW::HermesLite && bytes.size() > 21) {
        out.firmwareBetaVersion = static_cast<quint8>(bytes[21]);
    }

    // Populate derived capabilities
    out.adcCount = RadioInfo::adcCountForBoard(out.boardType);
    if (out.maxReceivers <= 0) {
        out.maxReceivers = RadioInfo::maxReceiversForBoard(out.boardType);
    }
    out.name                = QString::fromLatin1(BoardCapsTable::forBoard(out.boardType).displayName);
    out.hasDiversityReceiver = (out.adcCount >= 2);
    out.hasPureSignal        = (out.boardType != HPSDRHW::Atlas && out.boardType != HPSDRHW::Unknown);
    // Plan Task 5: the top Protocol 1 rate for this board, not 384 kHz for
    // every reply. On Protocol 1 only the RedPitaya (Thetis) and the HL2
    // (mi0bot) reach 384 kHz; every other board tops out at 192 kHz. The
    // cited ladder is BoardCapsTable::sampleRatesFor. A reply carries the board, not the model, so the board's default
    // model stands in. A RedPitaya answers as a Hermes or OrionMKII board
    // and is told apart only by the model the operator picks; the rate
    // list and Radio Info read that model.
    out.maxSampleRate = BoardCapsTable::maxSampleRateFor(
        BoardCapsTable::forBoard(out.boardType), ProtocolVersion::Protocol1,
        defaultModelForBoard(out.boardType));

    return true;
}

// ---------------------------------------------------------------------------
// parseP2Reply — extract RadioInfo from a P2 discovery response.
// From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P2 branch (line ~1201).
//
// P2 reply layout (OpenHPSDR Protocol 2):
//   [0..3] 0x00 0x00 0x00 0x00
//   [4]    0x02 (available) or 0x03 (in use)
//   [5..10] MAC address (6 bytes)
//   [11]   board type (HPSDRHW enum value directly)
//   [12]   protocol supported byte
//   [13]   firmware version (CodeVersion)
//   [14..17] Mercury versions 0-3
//   [18]   Penny version
//   [19]   Metis version
//   [20]   number of hardware receivers
//   [23]   beta version (the firmware's second number; 0 = none)
// ---------------------------------------------------------------------------
bool RadioDiscovery::parseP2Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out)
{
    // From Thetis: data[0]==0x0 && data[1]==0x0 && data[2]==0x0 && data[3]==0x0 && (data[4]==0x2 || data[4]==0x3)
    if (bytes.size() < 21) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(bytes[0]);
    const quint8 b1 = static_cast<quint8>(bytes[1]);
    const quint8 b2 = static_cast<quint8>(bytes[2]);
    const quint8 b3 = static_cast<quint8>(bytes[3]);
    const quint8 b4 = static_cast<quint8>(bytes[4]);

    if (b0 != 0x00 || b1 != 0x00 || b2 != 0x00 || b3 != 0x00 || (b4 != 0x02 && b4 != 0x03)) {
        return false;
    }

    out = RadioInfo{};
    out.protocol  = ProtocolVersion::Protocol2;
    out.inUse     = (b4 == 0x03);
    out.port      = 1024;

    // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
    bool ok = false;
    quint32 ipv4 = source.toIPv4Address(&ok);
    out.address = ok ? QHostAddress(ipv4) : source;

    // MAC: bytes 5-10
    out.macAddress = macToString(bytes.constData() + 5);

    // From Thetis: r.DeviceType = (HPSDRHW)data[11]
    out.boardType       = static_cast<HPSDRHW>(static_cast<quint8>(bytes[11]));
    // data[12] = ProtocolSupported (not stored in RadioInfo currently)
    out.firmwareVersion = static_cast<quint8>(bytes[13]);  // CodeVersion

    // From Thetis: if (len > 20) — receivers count
    if (bytes.size() > 20) {
        int hwRx = static_cast<quint8>(bytes[20]);
        // From Thetis clsRadioDiscovery.cs:1194 [v2.10.3.15]:
        //   r.NumRxs = data[20];
        // Kept apart from the fallback: RadioModel sizes the Protocol 2
        // stream pool from the radio's own number when it gives one.
        out.reportedReceivers = hwRx;
        out.maxReceivers = (hwRx > 0) ? hwRx : RadioInfo::maxReceiversForBoard(out.boardType);
    }

    // The firmware's second number, byte 23 ("Beta version, 0 = false" in
    // the Discovery Reply table of the OpenHPSDR Ethernet Protocol v4.4
    // document; reserved until its v3.8).
    // From Thetis clsRadioDiscovery.cs:1184 [v2.10.3.15]:
    //   r.BetaVersion = len > 23 ? data[23] : (byte)0;
    // The Anvelina Pro 3 fills it with the last number of its release, 14
    // for 2.2.14 beside a code version of 22:
    //   From n1gp-Anvelina_PROIII Orion.v:634, 965-966 [@8e86a61]
    //   From n1gp-Anvelina_PROIII Ethernet/sdr_send.v:171-172 [@8e86a61]
    // A reply that ends before the byte, or holds zero there, reports none.
    out.firmwareBetaVersion = bytes.size() > 23 ? static_cast<quint8>(bytes[23]) : 0;

    // Populate derived capabilities
    out.adcCount = RadioInfo::adcCountForBoard(out.boardType);
    if (out.maxReceivers <= 0) {
        out.maxReceivers = RadioInfo::maxReceiversForBoard(out.boardType);
    }
    out.name                = QString::fromLatin1(BoardCapsTable::forBoard(out.boardType).displayName);
    out.hasDiversityReceiver = (out.adcCount >= 2);
    out.hasPureSignal        = (out.boardType != HPSDRHW::Atlas && out.boardType != HPSDRHW::Unknown);
    out.maxSampleRate        = 1536000;  // P2 supports higher sample rates

    qCDebug(lcDiscovery) << "P2 response from" << out.address.toString()
                         << "board:" << BoardCapsTable::forBoard(out.boardType).displayName
                         << "fw:" << out.firmwareVersion
                         << "beta:" << out.firmwareBetaVersion;

    return true;
}

// ---------------------------------------------------------------------------
// scanAllNics — mi0bot NIC-walk + per-NIC poll loop (event-driven since 2026-10-22).
// From Thetis clsRadioDiscovery.cs DiscoverUsingAllNics() + discoverOnNic().
//
// For each eligible NIC:
//   1. Bind a temporary QUdpSocket to that NIC's IPv4 address.
//   2. For each attempt (attemptsPerNic): send P1+P2 broadcast probes.
//   3. Poll up to quietPollsBeforeResend × pollTimeoutMs for replies.
//   4. Parse replies, de-duplicate by MAC, emit radioDiscovered / radioUpdated.
// ---------------------------------------------------------------------------

// Byte 4 of the P1 discovery frame. Thetis leaves the whole tail zeroed
// (clsRadioDiscovery.cs:1301-1309 buildDiscoveryPacketP1); NereusSDR sets a
// non-zero pad here deliberately.
//
// Why: a P1 discovery probe is broadcast to UDP 1024, which is also the P2
// "General" command port. Protocol 2 gateware claims any port-1024 datagram
// whose byte 4 is zero and parses the rest as a General command --
// General_CC.v:90/106 [TAPR OpenHPSDR-Firmware, Hermes_Protocol_2_C10_v11.0.5,
// ANAN-G2E]:
//
//     if (udp_rx_active && to_port == port)              // 1024
//         4: if (udp_rx_data != 8'd0)  state <= END;     // not for this module
//
// Our all-zero tail then lands as a valid config: byte 38 clears
// HW_timer_enable (General_CC.v:141), which freezes the board's ~2 s deadman
// (Hermes.v:406-411) -- the only automatic path that can clear a stuck `run`
// (High_Priority_CC.v:145-147). Bytes 58/59 clear PA_enable and Alex_enable.
// So merely scanning the LAN reconfigures every P2 radio on it and disarms
// their watchdog.
//
// A non-zero byte 4 makes General_CC bail at the command check while leaving
// P1 discovery untouched: every P1 gateware tests only the command byte and
// never reads byte 4. Verified across the TAPR P1 archives for Hermes v3.3,
// Angelia (ANAN-100D), Orion (ANAN-200D), ANAN-10E/100B and HermesC10
// (ANAN-G2E) -- all are `if (PHY_output[47:40] == 8'h02) // check for Metis
// Discovery` in Rx_MAC.v, which then captures only the requester's IP/MAC/port.
//
// Choosing the value: byte 4 is also the *P2 command* byte, decoded in
// sdr_receive.v (same archive) as
//
//     3: case (udp_rx_data)          // packet byte 4
//         2: state <= ST_DISCOVERY;
//         3: if (broadcast)  state <= ST_SETIP;          // writes IP to EEPROM
//         4: if (!broadcast) state <= ST_ERASE;
//         5: if (!broadcast) state <= ST_PROGRAM_FIFO;
//         6: if (!broadcast) state <= ST_RESET;          // resets the FPGA
//         default: state <= ST_WAIT;
//
// so the pad must avoid 0x00 (General_CC claims it) *and* 0x02..0x06. These
// probes go to the subnet broadcast, which is exactly the case ST_SETIP is
// gated on. An earlier revision used 0x02 to "mirror the P1 command byte";
// that made every P1 probe read as a second discovery request and doubled the
// discovery replies each radio emits per scan (verified in the app log: 2 per
// attempt before, 4 after). 0xFF falls through to ST_WAIT and is claimed by
// nothing.
//
// Gateware cited as hardware fact only, per CLAUDE.md; no gateware logic is
// translated here.
static constexpr char kP1ProbeByte4Pad = static_cast<char>(0xFF);

static QByteArray buildP1DiscoveryProbe()
{
    QByteArray p(63, 0);
    p[0] = static_cast<char>(0xEF);
    p[1] = static_cast<char>(0xFE);
    p[2] = static_cast<char>(0x02);
    p[4] = kP1ProbeByte4Pad;   // keep P2 General_CC from claiming this frame
    return p;
}

// ---------------------------------------------------------------------------
// The walk as a state machine, not a blocking loop (2026-10-22).
//
// Until 2026-10-22 scanAllNics() was one loop that waited in
// QAbstractSocket::waitForReadyRead() for attemptsPerNic ×
// quietPollsBeforeResend × pollTimeoutMs per NIC, on the thread that called
// startDiscovery() (the GUI thread for ConnectionPanel and MainWindow).
// Longpath measured the GUI held for 4656 ms per scan on a two-NIC machine
// (Longpath PR 251 [@279ab925]). The constructor's comment had promised
// this rewrite ("Async rewrite is a follow-up").
//
// What stays the same: NICs are walked one after another, each with
// attemptsPerNic attempts, each attempt sending the same probes to the same
// destinations and listening until quietPollsBeforeResend poll windows of
// pollTimeoutMs pass with nothing readable. The NIC candidates and the
// reply handling are unchanged. Only the waiting moved from poll() to a
// timer, and the event loop runs between windows.
//
// The event-driven shape follows Longpath PR 251 [@279ab925]; the code is
// fitted to NereusSDR's cancel, holdOffScans and stale-timer design.
// ---------------------------------------------------------------------------
void RadioDiscovery::scanAllNics()
{
    // From Thetis clsRadioDiscovery.cs buildDiscoveryPacketP1()
    m_scanP1 = buildP1DiscoveryProbe();

    // From Thetis clsRadioDiscovery.cs buildDiscoveryPacketP2()
    m_scanP2 = QByteArray(60, 0);
    m_scanP2[4] = static_cast<char>(0x02);

    m_scanTiming = m_timingOverride.value_or(timingFor(m_profile));

    m_scanNowMs = QDateTime::currentMSecsSinceEpoch();

    // Global MAC de-duplication across all NICs for this scan cycle
    m_scanSeen.clear();
    RadioInfo localXdma;
    if (parseG2XDMA(localXdma)) {
        m_scanSeen.insert(localXdma.macAddress);
        m_lastSeen[localXdma.macAddress] = m_scanNowMs;
        const bool known = m_radios.contains(localXdma.macAddress);
        m_radios.insert(localXdma.macAddress, localXdma);
        if (known) {
            emit radioUpdated(localXdma);
        } else {
            emit radioDiscovered(localXdma);
        }
    }
    m_scanStats = ScanStats{};

    // The candidates are taken once per walk, as the blocking loop read
    // QNetworkInterface::allInterfaces() once.
    m_scanNics.clear();
    if (m_scanNicsOverride) {
        m_scanNics = *m_scanNicsOverride;
    } else {
        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& iface : interfaces) {
            // From Thetis isNicCandidate(): Up/Running, not loopback
            if (!(iface.flags() & QNetworkInterface::IsUp)
                || !(iface.flags() & QNetworkInterface::IsRunning)
                || (iface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }

            // Find the first IPv4 address on this NIC
            const auto entries = iface.addressEntries();
            for (const QNetworkAddressEntry& entry : entries) {
                if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                    continue;
                }
                ScanNic nic;
                nic.name = iface.name();
                nic.local = entry.ip();
                nic.directed = entry.broadcast();
                nic.generalBroadcast = true;
                nic.port = kDiscoveryPort;
                m_scanNics.append(nic);
                break;
            }
        }
    }

    m_scanRunning  = true;
    m_scanNicIndex = -1;
    beginNextNic();
}

void RadioDiscovery::beginNextNic()
{
    // Close the previous NIC's socket before the next one binds.
    if (m_scanSock) {
        m_scanSock->disconnect(this);
        m_scanSock->close();
        m_scanSock->deleteLater();
        m_scanSock = nullptr;
    }

    // Cooperative cancel — see stopDiscovery(). Bail before touching
    // a new NIC if a shutdown was requested. (Inner loop also checks.)
    if (scanCancelled()) {
        finishScan();
        return;
    }

    ++m_scanNicIndex;
    while (m_scanNicIndex < m_scanNics.size()) {
        const ScanNic& nic = m_scanNics.at(m_scanNicIndex);

        // From Thetis discoverOnNic(): bind to local IPv4 on ephemeral port
        auto* sock = new QUdpSocket(this);
        sock->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);
        if (!sock->bind(nic.local, 0)) {
            qCWarning(lcDiscovery) << "Failed to bind to" << nic.local.toString()
                                   << "on" << nic.name << "— skipping";
            sock->deleteLater();
            ++m_scanNicIndex;
            continue;
        }

        // Enable SO_BROADCAST. Windows setsockopt takes optval as
        // `const char*`; POSIX takes `const void*`. Casting to `const char*`
        // is valid on both (main's fix b3c2961 for the legacy startDiscovery
        // path — Phase 3I's scanAllNics added a second call site that needs
        // the same treatment for Windows CI to build).
        const auto fd = sock->socketDescriptor();
        if (fd >= 0) {
            int broadcastEnable = 1;
            ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_BROADCAST,
                         reinterpret_cast<const char*>(&broadcastEnable),
                         sizeof(broadcastEnable));
        }

        connect(sock, &QUdpSocket::readyRead, this, &RadioDiscovery::onScanReadyRead);
        m_scanSock = sock;

        qCDebug(lcDiscovery) << "Scanning NIC" << nic.name
                             << nic.local.toString() << "profile" << static_cast<int>(m_profile);

        m_scanAttempt = 0;
        beginAttempt();
        return;
    }

    finishScan();
}

// From Thetis discoverOnNic(): attempts × (send + quiet-poll loop).
// This is the send half of one attempt; onScanPollWindow() and
// onScanReadyRead() are the quiet-poll loop.
void RadioDiscovery::beginAttempt()
{
    if (scanCancelled()) {
        finishScan();
        return;
    }
    if (!m_scanSock || m_scanNicIndex < 0 || m_scanNicIndex >= m_scanNics.size()) {
        finishScan();
        return;
    }

    // Post-disconnect quiet period (2026-07-27, ANAN-G2E lockup). The walk
    // no longer blocks the thread, so a disconnect can arm holdOffScans()
    // while it runs. startDiscovery() only checks the quiet period when a
    // walk starts; every attempt checks it again here, including a NIC's
    // first. The attempt waits, with no poll or deadline timer running, and
    // starts again (this function) when the quiet period ends: the probes
    // are delayed, never dropped, and the walk does not end early. A
    // stopDiscovery() meanwhile ends the walk and finishScan() stops this
    // wait. The check is repeated on resume, since a later disconnect may
    // have extended the deadline.
    if (const qint64 waitMs = holdOffRemainingMs(); waitMs > 0) {
        m_scanPollTimer.stop();
        m_scanDeadlineTimer.stop();
        qCDebug(lcDiscovery) << "Discovery attempt" << m_scanAttempt + 1
                             << "waits" << waitMs << "ms for the post-disconnect quiet period or a connect hold";
        m_scanHoldOffTimer.start(int(waitMs));
        return;
    }

    const ScanNic& nic = m_scanNics.at(m_scanNicIndex);

    // Send P1 and P2 probes to directed subnet broadcast and 255.255.255.255
    if (!nic.directed.isNull()) {
        m_scanSock->writeDatagram(m_scanP1, nic.directed, nic.port);
        m_scanSock->writeDatagram(m_scanP2, nic.directed, nic.port);
    }
    if (nic.generalBroadcast) {
        m_scanSock->writeDatagram(m_scanP1, QHostAddress::Broadcast, nic.port);
        m_scanSock->writeDatagram(m_scanP2, QHostAddress::Broadcast, nic.port);
    }

    // From Thetis clsRadioDiscovery.cs:935-939 [v2.10.3.15]:
    //   if (pollMs < 10) pollMs = 10;
    //   if (quietBeforeResend < 1) quietBeforeResend = 1;
    const int quietBeforeStop = qMax(1, m_scanTiming.quietPollsBeforeResend);
    const int pollMs          = qMax(10, m_scanTiming.pollTimeoutMs);

    // From Thetis clsRadioDiscovery.cs:964 [v2.10.3.15]: int quietPolls = 0;
    m_scanQuietPolls = 0;
    ++m_scanStats.attempts;

    // NereusSDR addition: a hard deadline per attempt. Thetis has none
    // (clsRadioDiscovery.cs:966 [v2.10.3.15], `while (quietPolls <
    // quietBeforeResend)` is the only exit), so a datagram arriving more
    // often than once per poll window holds an attempt open forever; Longpath
    // found the same gap and bounded it (PR 9 [@988fa77e]). The limit is
    // kAttemptDeadlineFactor quiet budgets: a walk whose replies all come in
    // early ends at one quiet budget plus the time the replies took, so the
    // deadline only ends attempts that would never have gone quiet. A walk
    // is then bounded by NICs × attempts × kAttemptDeadlineFactor ×
    // quietPollsBeforeResend × pollTimeoutMs.
    m_scanDeadlineTimer.start(kAttemptDeadlineFactor * quietBeforeStop * pollMs);
    // From Thetis: s.Poll(pollMs * 1000, SelectMode.SelectRead)
    m_scanPollTimer.start(pollMs);
}

// One attempt is over: the next attempt on this NIC, or the next NIC.
void RadioDiscovery::endAttempt()
{
    m_scanPollTimer.stop();
    m_scanDeadlineTimer.stop();
    ++m_scanAttempt;
    if (m_scanAttempt < qMax(1, m_scanTiming.attemptsPerNic)) {
        beginAttempt();
    } else {
        beginNextNic();
    }
}

// A poll window passed with nothing readable: the place the blocking loop
// came back from waitForReadyRead() with false.
void RadioDiscovery::onScanPollWindow()
{
    if (!m_scanRunning) {
        return;
    }
    // Cooperative cancel — see stopDiscovery(). Checked after
    // each waitForReadyRead window so shutdown latency is at
    // most one pollTimeoutMs (~150 ms on SafeDefault).
    if (scanCancelled()) {
        finishScan();
        return;
    }
    // An attempt waiting out the quiet period has no poll window open:
    // nothing counts toward it until it sends (beginAttempt() starts its
    // quiet count from zero then).
    if (m_scanHoldOffTimer.isActive()) {
        return;
    }

    // From Thetis clsRadioDiscovery.cs:970-975 [v2.10.3.15]:
    //   bool readable = s.Poll(pollMs * 1000, SelectMode.SelectRead);
    //   if (!readable)
    //   {
    //       quietPolls++;
    //       d.QuietPolls++;
    //       continue;
    //   }
    ++m_scanQuietPolls;
    ++m_scanStats.quietPolls;

    const int quietBeforeStop = qMax(1, m_scanTiming.quietPollsBeforeResend);
    if (m_scanQuietPolls >= quietBeforeStop) {
        endAttempt();
        return;
    }
    m_scanPollTimer.start(qMax(10, m_scanTiming.pollTimeoutMs));
}

// The socket became readable: the blocking loop's readable branch.
void RadioDiscovery::onScanReadyRead()
{
    if (!m_scanRunning || !m_scanSock) {
        return;
    }
    if (scanCancelled()) {
        finishScan();
        return;
    }

    // From Thetis clsRadioDiscovery.cs:964-976 [v2.10.3.15]: only the
    // not-readable branch touches quietPolls. A readable poll neither counts
    // as quiet nor resets the counter; the next Poll() starts a fresh
    // pollMs window after the receive. Restarting the window timer here is
    // that fresh window. Until 2026-10-22 NereusSDR reset the counter on
    // every readable poll (`quietPolls = 0;`, commented "Reset quiet
    // counter on activity — replies may be bursty"), which Thetis does not
    // do, so a late reply bought a whole extra quiet budget (Longpath PR 10
    // [@2e967e3f]).
    //
    // While an attempt waits out the quiet period (see beginAttempt()) a
    // late reply is still taken, but opens no poll window: the quiet count
    // is still full from the attempt that ended, and one window would end
    // the waiting attempt without its probes ever being sent.
    if (!m_scanHoldOffTimer.isActive()) {
        ++m_scanStats.readableWindows;
        m_scanPollTimer.start(qMax(10, m_scanTiming.pollTimeoutMs));
    }

    while (m_scanSock && m_scanSock->hasPendingDatagrams()) {
        QHostAddress senderAddr;
        quint16 senderPort = 0;
        QByteArray data;
        data.resize(static_cast<int>(m_scanSock->pendingDatagramSize()));
        m_scanSock->readDatagram(data.data(), data.size(), &senderAddr, &senderPort);
        ++m_scanStats.datagrams;
        handleScanDatagram(data, senderAddr);
        // A slot on radioDiscovered may have stopped the walk.
        if (!m_scanRunning) {
            return;
        }
    }
}

// The attempt never went quiet within its deadline: treated exactly like a
// quiet end, so the walk moves on to the next attempt or NIC.
void RadioDiscovery::onScanAttemptDeadline()
{
    if (!m_scanRunning) {
        return;
    }
    ++m_scanStats.deadlineEnds;
    qCDebug(lcDiscovery) << "Discovery attempt" << m_scanAttempt + 1 << "on"
                         << (m_scanNicIndex >= 0 && m_scanNicIndex < m_scanNics.size()
                                 ? m_scanNics.at(m_scanNicIndex).name : QString())
                         << "reached its deadline without going quiet";
    if (scanCancelled()) {
        finishScan();
        return;
    }
    endAttempt();
}

// The blocking loop's per-datagram body, unchanged.
void RadioDiscovery::handleScanDatagram(const QByteArray& data, const QHostAddress& senderAddr)
{
    if (data.size() < 11) {
        return;
    }

    RadioInfo info;
    bool parsed = false;

    const quint8 firstByte = static_cast<quint8>(data[0]);
    if (firstByte == 0xEF) {
        parsed = parseP1Reply(data, senderAddr, info);
    } else if (firstByte == 0x00) {
        parsed = parseP2Reply(data, senderAddr, info);
    }

    if (!parsed) {
        return;
    }

    // From Thetis: MAC-based de-duplication (seen set)
    if (info.macAddress.isEmpty()
        || info.macAddress == "00:00:00:00:00:00") {
        return;
    }

    if (m_scanSeen.contains(info.macAddress)) {
        return;
    }
    m_scanSeen.insert(info.macAddress);

    m_lastSeen[info.macAddress] = m_scanNowMs;

    if (!m_radios.contains(info.macAddress)) {
        m_radios.insert(info.macAddress, info);
        qCDebug(lcDiscovery) << "Discovered:" << info.displayName()
                             << "P" << static_cast<int>(info.protocol)
                             << "fw" << info.firmwareText()
                             << "at" << info.address.toString();
        emit radioDiscovered(info);
    } else {
        m_radios[info.macAddress] = info;
        emit radioUpdated(info);
    }
}

void RadioDiscovery::finishScan()
{
    m_scanPollTimer.stop();
    m_scanDeadlineTimer.stop();
    m_scanHoldOffTimer.stop();
    if (m_scanSock) {
        m_scanSock->disconnect(this);
        m_scanSock->close();
        m_scanSock->deleteLater();
        m_scanSock = nullptr;
    }
    m_scanNics.clear();
    m_scanNicIndex = -1;

    const bool wasRunning = m_scanRunning;
    m_scanRunning = false;
    if (!wasRunning) {
        return;
    }

    // Sweep interval matches the discovered-only timeout so the first sweep
    // fires no earlier than a radio can be considered stale (design §7.4).
    // Not after stopDiscovery(), which has just stopped it.
    if (!m_stopRequested.load(std::memory_order_acquire) && !m_staleTimer.isActive()) {
        m_staleTimer.start(kDiscoveredOnlyTimeoutMs);
    }
    emit discoveryFinished();
}

void RadioDiscovery::onStaleCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList stale;

    // Design §7.4: two permanent exemptions from stale removal —
    //   1. Connected MAC (already connected, stops sending beacons; existing rule).
    //   2. Saved MACs (AppSettings entries): rows stay in the panel forever;
    //      the pill transitions Stale → Offline on age but the entry is never removed.
    // Discovered-only radios (not in m_savedMacs) age out at kDiscoveredOnlyTimeoutMs.
    for (auto it = m_lastSeen.constBegin(); it != m_lastSeen.constEnd(); ++it) {
        if (it.key() == m_connectedMac) { continue; }
        if (m_savedMacs.contains(it.key())) { continue; }  // saved never age out
        if (now - it.value() > kDiscoveredOnlyTimeoutMs) {
            stale.append(it.key());
        }
    }

    for (const QString& mac : stale) {
        qCDebug(lcDiscovery) << "Radio lost:" << mac;
        m_radios.remove(mac);
        m_lastSeen.remove(mac);
        emit radioLost(mac);
    }
}

// probeAddress — unicast P1+P2 probe for a single IP:port.
// From Phase 3Q design §7.2.
// Reuses parseP1Reply / parseP2Reply exactly as the broadcast scan path does.
void RadioDiscovery::probeAddress(const QHostAddress& addr,
                                  quint16 port,
                                  std::chrono::milliseconds timeout)
{
    // Same post-disconnect quiet period as startDiscovery(): a unicast probe
    // at a radio mid-stop-transition is the same race as a broadcast one.
    // Defer the whole call; the caller's timeout starts when the probe is
    // actually sent, so Connect flows just see a slightly longer probe.
    if (const qint64 waitMs = holdOffRemainingMs(); waitMs > 0) {
        QTimer::singleShot(int(waitMs), this, [this, addr, port, timeout]() {
            probeAddress(addr, port, timeout);
        });
        return;
    }

    auto* sock = new QUdpSocket(this);
    sock->bind(QHostAddress::AnyIPv4, 0);

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);

    auto cleanup = [sock, timer]() {
        timer->stop();
        timer->deleteLater();
        sock->close();
        sock->deleteLater();
    };

    // Reply handler — try P1 first, then P2.
    // Reply path doesn't need addr/port; the parsers read them from the datagram.
    connect(sock, &QUdpSocket::readyRead, this, [this, sock, cleanup]() {
        while (sock->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(sock->pendingDatagramSize()));
            QHostAddress src;
            quint16 srcPort = 0;
            sock->readDatagram(buf.data(), buf.size(), &src, &srcPort);

            RadioInfo info;
            if (parseP1Reply(buf, src, info) || parseP2Reply(buf, src, info)) {
                m_radios.insert(info.macAddress, info);
                m_lastSeen.insert(info.macAddress, QDateTime::currentMSecsSinceEpoch());
                emit radioDiscovered(info);
                cleanup();
                return;
            }
        }
    });

    // Timeout → emit failure and clean up.
    connect(timer, &QTimer::timeout, this, [this, addr, port, cleanup]() {
        emit probeFailed(addr, port);
        cleanup();
    });

    // Send P1 + P2 probes in parallel.
    // Both must be padded to the full discovery-frame size — real OpenHPSDR
    // firmware ignores short probes (only the broadcast scan path was sending
    // padded frames before; this matches scanAllNics's p1Packet/p2Packet shape).
    // Same byte-4 pad as the broadcast scan path; rationale at
    // buildP1DiscoveryProbe() above.
    QByteArray p1Packet = buildP1DiscoveryProbe();

    QByteArray p2Packet(60, 0);
    p2Packet[4] = static_cast<char>(0x02);

    sock->writeDatagram(p1Packet, addr, port);
    sock->writeDatagram(p2Packet, addr, port);

    timer->start(int(timeout.count()));
}

} // namespace NereusSDR
