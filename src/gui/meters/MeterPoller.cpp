// =================================================================
// src/gui/meters/MeterPoller.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

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

#include "MeterPoller.h"
#include "MeterWidget.h"
#include "MeterItem.h"
#include "core/RxChannel.h"
#include "core/RadioStatus.h"
#include "core/LogCategories.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"

namespace NereusSDR {

MeterPoller::MeterPoller(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(100);  // 10 fps default (from Thetis UpdateInterval=100ms)
    connect(&m_timer, &QTimer::timeout, this, &MeterPoller::poll);
}

MeterPoller::~MeterPoller() = default;

void MeterPoller::setRxChannel(RxChannel* channel)
{
    m_rxChannel = channel;
    qCDebug(lcMeter) << "MeterPoller: RxChannel set, channelId:"
                      << (channel ? channel->channelId() : -1);
}

void MeterPoller::addTarget(MeterWidget* widget)
{
    if (!widget) { return; }
    // Drop any stale entries whose widgets have been destroyed, then add
    // only if not already present.
    m_targets.removeAll(QPointer<MeterWidget>(nullptr));
    for (const auto& p : m_targets) {
        if (p.data() == widget) { return; }
    }
    m_targets.append(QPointer<MeterWidget>(widget));
}

void MeterPoller::removeTarget(MeterWidget* widget)
{
    m_targets.removeAll(QPointer<MeterWidget>(widget));
    m_targets.removeAll(QPointer<MeterWidget>(nullptr));
}

void MeterPoller::setInterval(int ms)
{
    m_timer.setInterval(ms);
}

int MeterPoller::interval() const
{
    return m_timer.interval();
}

void MeterPoller::start()
{
    // Phase 3G-6 block 5: poll runs regardless of m_rxChannel so
    // MMIO-bound items update even before a radio is connected.
    m_timer.start();
    qCDebug(lcMeter) << "MeterPoller: started at" << m_timer.interval() << "ms";
}

void MeterPoller::stop()
{
    m_timer.stop();
    qCDebug(lcMeter) << "MeterPoller: stopped";
}

void MeterPoller::poll()
{
    // Phase 3G-6 block 5: MMIO item polling is independent of the
    // RX channel, so this branch runs even when m_rxChannel is
    // unset (e.g. no radio connected). For every target widget,
    // walk its items and push the latest value from the bound
    // endpoint's variable cache into each item with an MMIO
    // binding.
    auto& engine = ExternalVariableEngine::instance();
    for (auto& guarded : m_targets) {
        MeterWidget* target = guarded.data();
        if (!target) { continue; }
        for (MeterItem* item : target->items()) {
            if (!item || !item->hasMmioBinding()) { continue; }
            MmioEndpoint* ep = engine.endpoint(item->mmioGuid());
            if (!ep) { continue; }
            const QVariant v = ep->valueForName(item->mmioVariable());
            if (!v.isValid()) { continue; }
            bool ok = false;
            const double d = v.toDouble(&ok);
            if (!ok) { continue; }
            item->setValue(d);
        }
        target->update();
    }

    if (!m_rxChannel) { return; }

    // Poll all RX meter types. GetRXAMeter is lock-free.
    double smeterDbm = -140.0;
    for (int bindingId = MeterBinding::SignalPeak;
         bindingId <= MeterBinding::AgcAvg; ++bindingId) {
        double value = m_rxChannel->getMeter(static_cast<RxMeterType>(bindingId));
        if (bindingId == MeterBinding::SignalAvg) {
            smeterDbm = value;
        }
        for (auto& guarded : m_targets) {
            MeterWidget* target = guarded.data();
            if (!target) { continue; }
            target->updateMeterValue(bindingId, value);
        }
    }

    // Push S-meter value to VfoWidget level bar.
    // smeterUpdated connects to VfoWidget::setSmeter in MainWindow.
    emit smeterUpdated(smeterDbm);
}

void MeterPoller::setRadioStatus(RadioStatus* status)
{
    // Disconnect any previous connection before re-wiring.
    if (m_powerConn) {
        QObject::disconnect(m_powerConn);
        m_powerConn = QMetaObject::Connection{};
    }
    m_radioStatus = status;
    if (m_radioStatus) {
        // From Thetis console.cs PollPAPWR loop [v2.10.3.13]:
        // RadioStatus::powerChanged aggregates forward/reflected/swr from
        // PollPAPWR's alex_fwd / alex_rev / swr locals and emits them
        // together. Fan values out to all registered MeterWidget targets
        // via updateMeterValue() — same pattern as the RX poll() loop.
        m_powerConn = connect(
            m_radioStatus, &RadioStatus::powerChanged,
            this, [this](double fwd, double rev, double swr) {
                for (auto& guarded : m_targets) {
                    MeterWidget* target = guarded.data();
                    if (!target) { continue; }
                    target->updateMeterValue(MeterBinding::TxPower,        fwd);
                    target->updateMeterValue(MeterBinding::TxReversePower, rev);
                    target->updateMeterValue(MeterBinding::TxSwr,          swr);
                }
            });
    }
}

} // namespace NereusSDR
