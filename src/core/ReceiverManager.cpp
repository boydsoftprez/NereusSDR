// =================================================================
// src/core/ReceiverManager.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - iqDataForReceiverStamped and enqueueClockNs (monotonic
//                 enqueue stamp for the DSP input delay bound, R-R3-40) by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code. NereusSDR-original; no Thetis
//                 counterpart.
//   2026-09-23 - hardwareIqDataStamped (the stamped raw hardware-DDC batch
//                 for the external-diversity input bound, R-R3-40) by J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code. NereusSDR-original.
//   2026-09-30 - beginIqBatch / endIqBatch: the queued stamped batches of
//                 one socket drain go to the DSP worker as one post per
//                 stream, not one per packet, by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 NereusSDR-original.
//   2026-09-30 - reset() drops a held batch and ends batching. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 NereusSDR-original.
//   2026-10-14 - Complete routing-map synchronization and publish control
//                 snapshots after unlocking, including callback-safe retunes.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 NereusSDR-original synchronization logic.
//   2026-10-22 - noteIqSequenceGap: an I/Q sequence gap routed through the
//                 hardware-to-logical map the samples take, after Longpath
//                 src/core/ReceiverManager.cpp:359 [@383f058] (PR 62
//                 follow-up 7eccc204). J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Claude Code.
//   2026-10-22 - I/Q level log in feedIqData (IqLevelMonitor, one line per
//                 hardware DDC at most every 10 s: RMS and peak dBFS,
//                 non-finite pairs, stream (logical receiver), ADC,
//                 overload flag, DDC map) and noteAdcOverflow. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 NereusSDR-original.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "ReceiverManager.h"
#include "LogCategories.h"

#include <QThread>
#include <optional>
#include <QPointer>

#include "codec/IP1Codec.h"
#include "codec/IP2Codec.h"

namespace NereusSDR {

const ReceiverConfig ReceiverManager::kInvalidConfig{};

ReceiverManager::ReceiverManager(QObject* parent)
    : QObject(parent)
{
}

ReceiverManager::~ReceiverManager() = default;

void ReceiverManager::setMaxReceivers(int max)
{
    QMutexLocker locker(&m_routingMutex);
    m_maxReceivers = qBound(1, max, 7);
}

int ReceiverManager::maxReceivers() const
{
    QMutexLocker locker(&m_routingMutex);
    return m_maxReceivers;
}

void ReceiverManager::setDdcFrequencyLocked(bool locked)
{
    QMutexLocker locker(&m_routingMutex);
    m_ddcFreqLocked = locked;
}

bool ReceiverManager::ddcFrequencyLocked() const
{
    QMutexLocker locker(&m_routingMutex);
    return m_ddcFreqLocked;
}

int ReceiverManager::createReceiver()
{
    QMutexLocker locker(&m_routingMutex);
    if (m_receivers.size() >= m_maxReceivers) {
        qCWarning(lcReceiver) << "Cannot create receiver: at maximum" << m_maxReceivers;
        return -1;
    }

    int index = m_nextWdspChannel++;

    ReceiverConfig config;
    config.receiverIndex = index;
    config.wdspChannel = index;
    config.active = false;

    m_receivers.insert(index, config);
    qCDebug(lcReceiver) << "Created receiver" << index;
    locker.unlock(); // No member-mutex cleanup may span an application callback.
    emit receiverCreated(index);
    return index;
}

void ReceiverManager::destroyReceiver(int receiverIndex)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    bool wasActive = m_receivers[receiverIndex].active;
    m_receivers.remove(receiverIndex);

    HardwareMappingSnapshot snapshot;
    if (wasActive) {
        snapshot = rebuildHardwareMapping();
    }
    locker.unlock();
    if (wasActive) {
        publishHardwareMapping(snapshot);
        if (!self) { return; }
    }

    qCDebug(lcReceiver) << "Destroyed receiver" << receiverIndex;
    emit receiverDestroyed(receiverIndex);
}

void ReceiverManager::reset()
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    const int priorCount = m_receivers.size();
    const QList<int> indices = m_receivers.keys();

    m_receivers.clear();
    m_hwToLogical.clear();
    m_nextWdspChannel = 0;
    m_firstForwardLogged = false;
    m_firstDropLogged = false;

    // Phase 3M-4 Task 6: clear codec pointers + PS state on disconnect.
    // The codecs are owned by P1/P2RadioConnection, which is destroyed
    // during teardownConnection — leaving stale pointers here would crash
    // any subsequent state setter that fires before the next connect.
    m_p1Codec = nullptr;
    m_p2Codec = nullptr;
    m_hpsdrModel = HPSDRModel::HPSDR;
    m_psEnabled = false;
    m_moxState = false;
    m_diversityEnabled = false;
    m_rx1Rate = 48000;
    m_rx2Rate = 48000;
    m_rx2Enabled = false;
    m_rxAdcCtrl1 = 0;
    m_rxAdcCtrl2 = 0;

    // RADE threads review: a batch the connection held when it went away
    // belongs to receivers that are gone; drop it, and end the batch, so
    // nothing held is posted after the reset and no thread stays batching.
    // Lock order as in feedIqData: the routing lock, then the batch lock.
    {
        QMutexLocker batchLock(&m_iqBatchMutex);
        m_iqBatchThread.store(nullptr, std::memory_order_release);
        m_heldIq.clear();
    }

    locker.unlock();
    for (int idx : indices) {
        emit receiverDestroyed(idx);
        if (!self) { return; }
    }

    if (priorCount > 0) {
        emit activeReceiverCountChanged(0);
        if (!self) { return; }
        emit hardwareSlotsChanged(0);
        if (!self) { return; }
        emit hardwareReceiverCountChanged(0);
        if (!self) { return; }
    }

    qCDebug(lcReceiver) << "ReceiverManager reset;" << priorCount << "receivers dropped";
}

void ReceiverManager::activateReceiver(int receiverIndex)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    if (m_receivers[receiverIndex].active) {
        return;
    }

    m_receivers[receiverIndex].active = true;
    const HardwareMappingSnapshot snapshot = rebuildHardwareMapping();
    locker.unlock();
    publishHardwareMapping(snapshot);
    if (!self) { return; }

    qCDebug(lcReceiver) << "Activated receiver" << receiverIndex;
    emit receiverActivated(receiverIndex);
}

void ReceiverManager::deactivateReceiver(int receiverIndex)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    if (!m_receivers[receiverIndex].active) {
        return;
    }

    m_receivers[receiverIndex].active = false;
    const HardwareMappingSnapshot snapshot = rebuildHardwareMapping();
    locker.unlock();
    publishHardwareMapping(snapshot);
    if (!self) { return; }

    qCDebug(lcReceiver) << "Deactivated receiver" << receiverIndex;
    emit receiverDeactivated(receiverIndex);
}

int ReceiverManager::activeReceiverCount() const
{
    QMutexLocker locker(&m_routingMutex);
    int count = 0;
    for (auto it = m_receivers.constBegin(); it != m_receivers.constEnd(); ++it) {
        if (it->active) {
            ++count;
        }
    }
    return count;
}

bool ReceiverManager::isReceiverActive(int receiverIndex) const
{
    QMutexLocker locker(&m_routingMutex);
    auto it = m_receivers.constFind(receiverIndex);
    if (it != m_receivers.constEnd()) {
        return it->active;
    }
    return false;
}

ReceiverConfig ReceiverManager::receiverConfig(int receiverIndex) const
{
    QMutexLocker locker(&m_routingMutex);
    return m_receivers.value(receiverIndex, kInvalidConfig);
}

void ReceiverManager::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    m_receivers[receiverIndex].frequencyHz = frequencyHz;
    locker.unlock();
    emit receiverFrequencyChanged(receiverIndex, frequencyHz);
    if (!self) { return; }

    // If active, notify the hardware — unless DDC is locked (CTUN mode,
    // MainWindow manages DDC frequency directly)
    // The logical callback may remove/remap the receiver or change CTUN.
    // Revalidate without operator[] so removal cannot insert a ghost entry.
    // A nested retune supersedes this call before its hardware notification.
    locker.relock();
    const auto it = m_receivers.constFind(receiverIndex);
    if (m_ddcFreqLocked || it == m_receivers.constEnd()
        || it->frequencyHz != frequencyHz || !it->active || it->hardwareRx < 0) {
        return;
    }
    const int hardwareRx = it->hardwareRx;
    locker.unlock();
    emit hardwareFrequencyChanged(hardwareRx, frequencyHz);
}

void ReceiverManager::forceHardwareFrequency(int receiverIndex, quint64 frequencyHz)
{
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    // Store as well as emit. ReceiverConfig::frequencyHz is the frequency
    // last commanded to this receiver's DDC, and rebuildHardwareMapping's
    // re-emit loop is its only consumer -- nothing else in the tree reads
    // it. Leaving it behind on the forced path meant a later rebuild
    // resurrected the pre-drag centre and yanked the CTUN pan back.
    //
    // The lock bypass is untouched and deliberate: m_ddcFreqLocked gates
    // setReceiverFrequency's hardware emit so a VFO move inside a pinned
    // CTUN window does not retune the DDC, while the pan drag itself is
    // exactly the operator asking for a retune. receiverFrequencyChanged
    // is likewise still NOT emitted here -- the DDC moved, the receiver's
    // logical tuning did not, and that signal belongs to the latter.
    m_receivers[receiverIndex].frequencyHz = frequencyHz;

    if (m_receivers[receiverIndex].active && m_receivers[receiverIndex].hardwareRx >= 0) {
        const int hardwareRx = m_receivers[receiverIndex].hardwareRx;
        locker.unlock();
        emit hardwareFrequencyChanged(hardwareRx, frequencyHz);
    }
}

void ReceiverManager::setReceiverSampleRate(int receiverIndex, int sampleRate)
{
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    m_receivers[receiverIndex].sampleRate = sampleRate;
}

void ReceiverManager::applyDdcMapping(const std::array<int, 5>& mapping, quint32 activeStreams)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    QList<int> activated;
    QList<int> deactivated;
    bool changed = false;
    for (auto it = m_receivers.begin(); it != m_receivers.end(); ++it) {
        const int st = it->receiverIndex;
        if (st < 0 || st >= int(mapping.size())) { continue; }
        const bool active = mapping[st] >= 0 && (activeStreams & (1u << st));
        if (it->active != active) {
            (active ? activated : deactivated).append(st);
            changed = true;
        }
        changed |= it->active && it->ddcIndex != mapping[st];
        it->ddcIndex = mapping[st];
        it->active = active;
    }
    if (!changed) { return; }
    const HardwareMappingSnapshot snapshot = rebuildHardwareMapping();
    locker.unlock();
    publishHardwareMapping(snapshot);
    if (!self) { return; }
    for (int st : deactivated) {
        emit receiverDeactivated(st);
        if (!self) { return; }
    }
    for (int st : activated) {
        emit receiverActivated(st);
        if (!self) { return; }
    }
}

void ReceiverManager::setDdcMapping(int receiverIndex, int ddcIndex)
{
    const QPointer<ReceiverManager> self(this);
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    // Change gate. Before Phase 3F Sub-Epic I this ran once per connect, so
    // an unconditional rebuild cost nothing. It is now called by
    // the original RadioModel::publishDdcAssignment for every active stream on every
    // requestDdcAssignment, which fires on every slice frequencyChanged --
    // so on every VFO tick, with the mapping unchanged in steady state.
    //
    // rebuildHardwareMapping re-emits every active receiver's STORED
    // frequency, and that is what makes a redundant rebuild harmful rather
    // than merely wasteful: on a non-CTUN pan it is a stream of duplicate
    // P2 command frames on a spun encoder, and under CTUN it retunes the
    // DDC away from where the operator dragged the pan (the drag goes
    // through forceHardwareFrequency, which deliberately bypasses
    // m_ddcFreqLocked). The spectrum jumped out from under the operator on
    // the next VFO click.
    if (m_receivers[receiverIndex].ddcIndex == ddcIndex) {
        return;
    }

    m_receivers[receiverIndex].ddcIndex = ddcIndex;
    const bool active = m_receivers[receiverIndex].active;
    HardwareMappingSnapshot snapshot;
    if (active) {
        snapshot = rebuildHardwareMapping();
    }
    locker.unlock();
    if (active) {
        publishHardwareMapping(snapshot);
        if (!self) { return; }
    }
    qCDebug(lcReceiver) << "Receiver" << receiverIndex << "mapped to DDC" << ddcIndex;
}

int ReceiverManager::ddcIndex(int receiverIndex) const
{
    QMutexLocker locker(&m_routingMutex);
    auto it = m_receivers.constFind(receiverIndex);
    if (it != m_receivers.constEnd()) {
        return it->hardwareRx;
    }
    return -1;
}

void ReceiverManager::setAdcForReceiver(int receiverIndex, int adcIndex)
{
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    m_receivers[receiverIndex].adcIndex = adcIndex;
    qCDebug(lcReceiver) << "Receiver" << receiverIndex << "using ADC" << adcIndex;
}

void ReceiverManager::feedIqData(int hwReceiverIndex, const QVector<float>& samples)
{
    // Lever 2 (2026-05-24): this function now runs on the Connection thread
    // via Qt::DirectConnection from RadioConnection::iqDataReceived (see the
    // wire-up in RadioModel.cpp).  Main-thread writers of m_hwToLogical /
    // m_receivers (createReceiver / destroyReceiver / reset / rebuildHardwareMapping)
    // serialize through m_routingMutex; the read here takes the same lock so
    // the hash structure can not flip mid-lookup.  Hot-path cost: one
    // uncontended mutex acquire per packet (~100 ns) plus the existing
    // hash lookups + emit setup.
    // R-R3-40: the external-diversity fork needs every hardware leg,
    // mapped or not; emitting outside the routing lock keeps it short.
    QVector<HeldIqBatch> full;
    const bool held =
        m_iqBatchThread.load(std::memory_order_acquire) == QThread::currentThreadId();
    const auto hold = [&](bool hardware, int index) {
        // Joins this packet to the stream's held batch, in arrival order.
        QMutexLocker batchLock(&m_iqBatchMutex);
        HeldIqBatch* batch = nullptr;
        for (HeldIqBatch& candidate : m_heldIq) {
            if (candidate.hardware == hardware && candidate.index == index) {
                batch = &candidate;
                break;
            }
        }
        if (!batch) {
            m_heldIq.append(HeldIqBatch{hardware, index, {}});
            batch = &m_heldIq.last();
        }
        batch->samples += samples;
        if (batch->samples.size() / 2 >= kIqBatchMaxSamples) {
            full.append(std::move(*batch));
            batch->samples = QVector<float>();
        }
    };
    if (held) {
        hold(true, hwReceiverIndex);
    } else {
        emit hardwareIqDataStamped(hwReceiverIndex, samples, enqueueClockNs());
    }

    QMutexLocker locker(&m_routingMutex);
    // I/Q level diagnostics: every hardware DDC is measured, mapped or not;
    // the lines are built under the lock (they name the map) and logged
    // after it is released.
    m_iqLevels.accumulate(hwReceiverIndex, samples.constData(), samples.size());
    QStringList levelLines;
    if (const std::optional<IqLevelMonitor::Report> report =
            m_iqLevels.takeIfDue(enqueueClockNs())) {
        levelLines = describeIqLevels(*report);
    }
    const auto logLevels = [&levelLines]() {
        for (const QString& line : std::as_const(levelLines)) {
            qCInfo(lcReceiver).noquote() << line;
        }
    };
    auto it = m_hwToLogical.constFind(hwReceiverIndex);
    if (it == m_hwToLogical.constEnd()) {
        if (!m_firstDropLogged) {
            m_firstDropLogged = true;
            QStringList mapped;
            for (auto mi = m_hwToLogical.constBegin(); mi != m_hwToLogical.constEnd(); ++mi) {
                mapped << QString("hw%1->rx%2").arg(mi.key()).arg(mi.value());
            }
            qCWarning(lcReceiver) << "ReceiverManager: first feedIqData dropped;"
                                  << "hwReceiverIndex=" << hwReceiverIndex
                                  << "map=" << (mapped.isEmpty() ? QStringLiteral("(empty)") : mapped.join(','));
        }
        locker.unlock();
        logLevels();
        flushHeldIq(full);
        return;
    }

    int logicalIndex = it.value();
    auto rxIt = m_receivers.constFind(logicalIndex);
    if (rxIt != m_receivers.constEnd()) {
        if (!m_firstForwardLogged) {
            m_firstForwardLogged = true;
            qCInfo(lcReceiver) << "ReceiverManager: first feedIqData forwarded;"
                               << "hw=" << hwReceiverIndex
                               << "logical=" << logicalIndex
                               << "wdspChannel=" << rxIt->wdspChannel
                               << "samples=" << samples.size();
        }
        emit iqDataForReceiver(logicalIndex, samples);
        if (held) {
            hold(false, logicalIndex);
        } else {
            emit iqDataForReceiverStamped(logicalIndex, samples, enqueueClockNs());
        }
        if (rxIt->wdspChannel >= 0) {
            emit iqDataForChannel(rxIt->wdspChannel, samples);
        }
    }
    locker.unlock();
    logLevels();
    flushHeldIq(full);
}

void ReceiverManager::noteIqSequenceGap(int hwReceiverIndex)
{
    // NereusSDR, 2026-10-22, after Longpath PR 62 and its 2026-10-08
    // follow-up (7eccc204): the gap goes through the map the samples take,
    // or it could reach another stream's engine than the samples it broke.
    if (hwReceiverIndex < 0) {
        emit iqSequenceGapForReceiver(-1);
        return;
    }
    int logicalIndex = -1;
    {
        QMutexLocker locker(&m_routingMutex);
        const auto it = m_hwToLogical.constFind(hwReceiverIndex);
        if (it != m_hwToLogical.constEnd()) {
            logicalIndex = it.value();
        }
    }
    if (logicalIndex >= 0) {
        emit iqSequenceGapForReceiver(logicalIndex);
    }
}

void ReceiverManager::noteAdcOverflow(int adc)
{
    m_iqLevels.noteAdcOverflow(adc);
}

void ReceiverManager::setIqLevelReportIntervalNsForTest(qint64 intervalNs)
{
    QMutexLocker locker(&m_routingMutex);
    m_iqLevels.setReportIntervalNsForTest(intervalNs);
}

QStringList ReceiverManager::describeIqLevels(const IqLevelMonitor::Report& report) const
{
    QStringList mapped;
    for (auto mi = m_hwToLogical.constBegin(); mi != m_hwToLogical.constEnd(); ++mi) {
        mapped << QStringLiteral("DDC%1->stream%2").arg(mi.key()).arg(mi.value());
    }
    const QString map = mapped.isEmpty() ? QStringLiteral("(empty)") : mapped.join(',');
    QStringList overloaded;
    for (int adc = 0; adc < 32; ++adc) {
        if ((report.overloadMask & (quint32{1} << adc)) != 0) {
            overloaded << QStringLiteral("ADC%1").arg(adc);
        }
    }
    QStringList lines;
    for (const IqLevelMonitor::DdcLevel& level : report.levels) {
        QString receiver = QStringLiteral("no stream");
        QString overload = overloaded.isEmpty()
                               ? QStringLiteral("no ADC overload")
                               : QStringLiteral("ADC overload on %1").arg(overloaded.join(','));
        const auto hw = m_hwToLogical.constFind(level.ddc);
        if (hw != m_hwToLogical.constEnd()) {
            const auto rx = m_receivers.constFind(hw.value());
            const int adc = rx != m_receivers.constEnd() ? rx->adcIndex : -1;
            receiver = adc >= 0 ? QStringLiteral("stream %1 (ADC%2)").arg(hw.value()).arg(adc)
                                : QStringLiteral("stream %1").arg(hw.value());
            if (adc >= 0 && adc < 32) {
                overload = QStringLiteral("ADC%1 overload %2")
                               .arg(adc)
                               .arg((report.overloadMask & (quint32{1} << adc)) != 0
                                        ? QStringLiteral("yes")
                                        : QStringLiteral("no"));
            }
        }
        lines << QStringLiteral("I/Q level: DDC%1 -> %2: RMS %3 dBFS, peak %4 dBFS over %5 pairs, "
                                "non-finite pairs %6; %7; DDC map %8")
                     .arg(level.ddc)
                     .arg(receiver)
                     .arg(level.rmsDbfs, 0, 'f', 1)
                     .arg(level.peakDbfs, 0, 'f', 1)
                     .arg(level.pairs)
                     .arg(level.nonFinite)
                     .arg(overload)
                     .arg(map);
    }
    return lines;
}

void ReceiverManager::beginIqBatch()
{
    // A batch this thread left open (it cannot, but be safe) goes out first.
    endIqBatch();
    m_iqBatchThread.store(QThread::currentThreadId(), std::memory_order_release);
}

void ReceiverManager::endIqBatch()
{
    QVector<HeldIqBatch> batches;
    {
        QMutexLocker batchLock(&m_iqBatchMutex);
        m_iqBatchThread.store(nullptr, std::memory_order_release);
        batches.swap(m_heldIq);
    }
    flushHeldIq(batches);
}

void ReceiverManager::flushHeldIq(QVector<HeldIqBatch>& batches)
{
    for (const HeldIqBatch& batch : batches) {
        if (batch.samples.isEmpty()) {
            continue;
        }
        if (batch.hardware) {
            emit hardwareIqDataStamped(batch.index, batch.samples, enqueueClockNs());
        } else {
            emit iqDataForReceiverStamped(batch.index, batch.samples, enqueueClockNs());
        }
    }
    batches.clear();
}

ReceiverManager::HardwareMappingSnapshot ReceiverManager::rebuildHardwareMapping()
{
    HardwareMappingSnapshot snapshot;
    m_hwToLogical.clear();

    // Assign hardware DDC indices to active receivers.
    // If a receiver has an explicit ddcIndex set (e.g., DDC2 for ANAN-G2 RX1),
    // use that. Otherwise fall back to sequential assignment.
    // From Thetis console.cs:8216 UpdateDDCs — DDC mapping is board-dependent.
    int nextAutoHw = 0;
    for (auto it = m_receivers.begin(); it != m_receivers.end(); ++it) {
        if (it->active) {
            int hwIdx = (it->ddcIndex >= 0) ? it->ddcIndex : nextAutoHw++;
            it->hardwareRx = hwIdx;
            m_hwToLogical.insert(hwIdx, it->receiverIndex);
            ++snapshot.count;
            if (hwIdx >= 0 && hwIdx < 32) {
                snapshot.slotMask |= (1u << hwIdx);
            }
        } else {
            it->hardwareRx = -1;
        }
    }

    qCDebug(lcReceiver) << "Hardware mapping rebuilt:" << snapshot.count << "active receivers";

    for (auto it = m_receivers.constBegin(); it != m_receivers.constEnd(); ++it) {
        if (it->active && it->hardwareRx >= 0) {
            snapshot.frequencies.append({it->hardwareRx, it->frequencyHz});
        }
    }
    return snapshot;
}

void ReceiverManager::publishHardwareMapping(const HardwareMappingSnapshot& snapshot)
{
    const QPointer<ReceiverManager> self(this);
    emit activeReceiverCountChanged(snapshot.count);
    if (!self) { return; }
    // Slots first, then the count: a Protocol 1 connection sizes the frame
    // from both, and seeing the slots first means the count arrives against
    // the slot set it belongs to (Phase 3F section 16.3.2).
    emit hardwareSlotsChanged(snapshot.slotMask);
    if (!self) { return; }
    emit hardwareReceiverCountChanged(snapshot.count);
    if (!self) { return; }

    // Re-emit the frequencies of this mapping, without a live-map iterator
    // or member-mutex scope spanning a synchronous notification.
    for (const auto& frequency : snapshot.frequencies) {
        emit hardwareFrequencyChanged(frequency.first, frequency.second);
        if (!self) { return; }
    }
}

// =====================================================================
// Phase 3M-4 Task 6: PureSignal DDC orchestration
//
// Setters shadow the live state and call updateDdcAssignment() on actual
// change.  updateDdcAssignment() dispatches to the injected per-board
// codec's applyPureSignalDdcConfig() (Task 5) and re-emits the wire-byte
// PsDdcConfig via ddcConfigChanged for RadioConnection to consume.
//
// Source: orchestration of Thetis console.cs:8186-8538 UpdateDDCs()
// [v2.10.3.13]; the per-board switch lives in the codec layer.
// =====================================================================

void ReceiverManager::setP1Codec(IP1Codec* codec)
{
    if (m_p1Codec == codec) {
        return;
    }
    m_p1Codec = codec;
    qCDebug(lcReceiver) << "ReceiverManager: P1 codec" << (codec ? "set" : "cleared");
    updateDdcAssignment();
    emit ddcCodecChanged();
}

void ReceiverManager::setP2Codec(IP2Codec* codec)
{
    if (m_p2Codec == codec) {
        return;
    }
    m_p2Codec = codec;
    qCDebug(lcReceiver) << "ReceiverManager: P2 codec" << (codec ? "set" : "cleared");
    updateDdcAssignment();
    emit ddcCodecChanged();
}

void ReceiverManager::setHpsdrModel(HPSDRModel model)
{
    if (m_hpsdrModel == model) {
        return;
    }
    m_hpsdrModel = model;
    updateDdcAssignment();
}

void ReceiverManager::setPureSignalEnabled(bool on)
{
    if (m_psEnabled == on) {
        return;
    }
    m_psEnabled = on;
    updateDdcAssignment();
}

void ReceiverManager::setMox(bool on)
{
    if (m_moxState == on) {
        return;
    }
    m_moxState = on;
    updateDdcAssignment();
}

void ReceiverManager::setDiversityEnabled(bool on)
{
    if (m_diversityEnabled == on) {
        return;
    }
    m_diversityEnabled = on;
    updateDdcAssignment();
}

void ReceiverManager::setRx1Rate(int rateHz)
{
    if (m_rx1Rate == rateHz) {
        return;
    }
    m_rx1Rate = rateHz;
    updateDdcAssignment();
}

void ReceiverManager::setRx2Rate(int rateHz)
{
    if (m_rx2Rate == rateHz) {
        return;
    }
    m_rx2Rate = rateHz;
    updateDdcAssignment();
}

void ReceiverManager::setRx2Enabled(bool on)
{
    if (m_rx2Enabled == on) {
        return;
    }
    m_rx2Enabled = on;
    updateDdcAssignment();
}

void ReceiverManager::setRxAdcCtrl1(quint8 reg)
{
    if (m_rxAdcCtrl1 == reg) {
        return;
    }
    m_rxAdcCtrl1 = reg;
    updateDdcAssignment();
}

void ReceiverManager::setRxAdcCtrl2(quint8 reg)
{
    if (m_rxAdcCtrl2 == reg) {
        return;
    }
    m_rxAdcCtrl2 = reg;
    updateDdcAssignment();
}

void ReceiverManager::updateDdcAssignment()
{
    // Defensive: when both codecs are non-null, P2 wins.  RadioModel only
    // ever sets one based on the connected protocol; pinning the order
    // means a future bug that sets both can't silently route through the
    // wrong protocol layer.  When neither is set (pre-connect / post-reset),
    // skip silently — RadioModel will install the codec on the next
    // applyHardwareInfo() and re-trigger via the seeded state setters.
    PsDdcConfig config{};
    if (m_p2Codec) {
        config = m_p2Codec->applyPureSignalDdcConfig(
            m_hpsdrModel,
            m_psEnabled,
            m_diversityEnabled,
            m_moxState,
            m_rx1Rate,
            m_rx2Rate,
            m_rx2Enabled,
            m_rxAdcCtrl1,
            m_rxAdcCtrl2);
    } else if (m_p1Codec) {
        config = m_p1Codec->applyPureSignalDdcConfig(
            m_hpsdrModel,
            m_psEnabled,
            m_diversityEnabled,
            m_moxState,
            m_rx1Rate,
            m_rx2Rate,
            m_rx2Enabled,
            m_rxAdcCtrl1,
            m_rxAdcCtrl2);
    } else {
        // No codec injected — nothing to emit.
        return;
    }

    // BENCH DIAG (G2E PS rate-flap): log every updateDdcAssignment fire with
    // input state + output rates so we can see what's flipping rate[0] between
    // PS rate (192k) and user rate (768k) during MOX-active windows.  Remove
    // once PS lock works on G2E.
    qCInfo(lcReceiver).nospace()
        << "DDCAssign fire: psEn=" << m_psEnabled
        << " mox=" << m_moxState
        << " div=" << m_diversityEnabled
        << " rx1Rate=" << m_rx1Rate
        << " rx2Rate=" << m_rx2Rate
        << " rx2En=" << m_rx2Enabled
        << " adcCtrl1=" << static_cast<int>(m_rxAdcCtrl1)
        << " → cfg.rate[0]=" << config.rate[0]
        << " rate[1]=" << config.rate[1]
        << " rate[2]=" << config.rate[2]
        << " ddcEn=" << static_cast<int>(config.ddcEnable)
        << " syncEn=" << static_cast<int>(config.syncEnable);

    emit ddcConfigChanged(config);
}

} // namespace NereusSDR
