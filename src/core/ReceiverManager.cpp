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
    m_maxReceivers = qBound(1, max, 7);
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
    emit receiverCreated(index);
    return index;
}

void ReceiverManager::destroyReceiver(int receiverIndex)
{
    QMutexLocker locker(&m_routingMutex);
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    bool wasActive = m_receivers[receiverIndex].active;
    m_receivers.remove(receiverIndex);

    if (wasActive) {
        rebuildHardwareMapping();
    }

    qCDebug(lcReceiver) << "Destroyed receiver" << receiverIndex;
    emit receiverDestroyed(receiverIndex);
}

void ReceiverManager::reset()
{
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

    for (int idx : indices) {
        emit receiverDestroyed(idx);
    }

    if (priorCount > 0) {
        emit activeReceiverCountChanged(0);
        emit hardwareReceiverCountChanged(0);
    }

    qCDebug(lcReceiver) << "ReceiverManager reset;" << priorCount << "receivers dropped";
}

void ReceiverManager::activateReceiver(int receiverIndex)
{
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    if (m_receivers[receiverIndex].active) {
        return;
    }

    m_receivers[receiverIndex].active = true;
    rebuildHardwareMapping();

    qCDebug(lcReceiver) << "Activated receiver" << receiverIndex;
    emit receiverActivated(receiverIndex);
}

void ReceiverManager::deactivateReceiver(int receiverIndex)
{
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    if (!m_receivers[receiverIndex].active) {
        return;
    }

    m_receivers[receiverIndex].active = false;
    rebuildHardwareMapping();

    qCDebug(lcReceiver) << "Deactivated receiver" << receiverIndex;
    emit receiverDeactivated(receiverIndex);
}

int ReceiverManager::activeReceiverCount() const
{
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
    auto it = m_receivers.constFind(receiverIndex);
    if (it != m_receivers.constEnd()) {
        return it->active;
    }
    return false;
}

ReceiverConfig ReceiverManager::receiverConfig(int receiverIndex) const
{
    return m_receivers.value(receiverIndex, kInvalidConfig);
}

void ReceiverManager::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    m_receivers[receiverIndex].frequencyHz = frequencyHz;
    emit receiverFrequencyChanged(receiverIndex, frequencyHz);

    // If active, notify the hardware — unless DDC is locked (CTUN mode,
    // MainWindow manages DDC frequency directly)
    if (!m_ddcFreqLocked
        && m_receivers[receiverIndex].active
        && m_receivers[receiverIndex].hardwareRx >= 0) {
        emit hardwareFrequencyChanged(m_receivers[receiverIndex].hardwareRx, frequencyHz);
    }
}

void ReceiverManager::forceHardwareFrequency(int receiverIndex, quint64 frequencyHz)
{
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
        emit hardwareFrequencyChanged(m_receivers[receiverIndex].hardwareRx, frequencyHz);
    }
}

void ReceiverManager::setReceiverSampleRate(int receiverIndex, int sampleRate)
{
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }
    m_receivers[receiverIndex].sampleRate = sampleRate;
}

void ReceiverManager::setDdcMapping(int receiverIndex, int ddcIndex)
{
    if (!m_receivers.contains(receiverIndex)) {
        return;
    }

    // Change gate. Before Phase 3F Sub-Epic I this ran once per connect, so
    // an unconditional rebuild cost nothing. It is now called by
    // RadioModel::publishDdcAssignment for every active stream on every
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
    if (m_receivers[receiverIndex].active) {
        rebuildHardwareMapping();
    }
    qCDebug(lcReceiver) << "Receiver" << receiverIndex << "mapped to DDC" << ddcIndex;
}

int ReceiverManager::ddcIndex(int receiverIndex) const
{
    auto it = m_receivers.constFind(receiverIndex);
    if (it != m_receivers.constEnd()) {
        return it->hardwareRx;
    }
    return -1;
}

void ReceiverManager::setAdcForReceiver(int receiverIndex, int adcIndex)
{
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
    QMutexLocker locker(&m_routingMutex);
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
        if (rxIt->wdspChannel >= 0) {
            emit iqDataForChannel(rxIt->wdspChannel, samples);
        }
    }
}

void ReceiverManager::rebuildHardwareMapping()
{
    QMutexLocker locker(&m_routingMutex);
    m_hwToLogical.clear();

    // Assign hardware DDC indices to active receivers.
    // If a receiver has an explicit ddcIndex set (e.g., DDC2 for ANAN-G2 RX1),
    // use that. Otherwise fall back to sequential assignment.
    // From Thetis console.cs:8216 UpdateDDCs — DDC mapping is board-dependent.
    int nextAutoHw = 0;
    int count = 0;
    for (auto it = m_receivers.begin(); it != m_receivers.end(); ++it) {
        if (it->active) {
            int hwIdx = (it->ddcIndex >= 0) ? it->ddcIndex : nextAutoHw++;
            it->hardwareRx = hwIdx;
            m_hwToLogical.insert(hwIdx, it->receiverIndex);
            ++count;
        } else {
            it->hardwareRx = -1;
        }
    }

    qCDebug(lcReceiver) << "Hardware mapping rebuilt:" << count << "active receivers";

    emit activeReceiverCountChanged(count);
    emit hardwareReceiverCountChanged(count);

    // Re-emit frequency for each active receiver
    for (auto it = m_receivers.constBegin(); it != m_receivers.constEnd(); ++it) {
        if (it->active && it->hardwareRx >= 0) {
            emit hardwareFrequencyChanged(it->hardwareRx, it->frequencyHz);
        }
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
