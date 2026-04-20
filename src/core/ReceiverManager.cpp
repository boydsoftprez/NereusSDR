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
    const int priorCount = m_receivers.size();
    const QList<int> indices = m_receivers.keys();

    m_receivers.clear();
    m_hwToLogical.clear();
    m_nextWdspChannel = 0;
    m_firstForwardLogged = false;
    m_firstDropLogged = false;

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

} // namespace NereusSDR
