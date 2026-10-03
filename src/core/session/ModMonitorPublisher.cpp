// no-port-check: NereusSDR-original. The Core's side of the AM Mod
// Monitor's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/ModMonitorPublisher.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See ModMonitorPublisher.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (R-IOS-13, R-R3-49).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/ModMonitorPublisher.h"

#include "core/session/ModMonitorRecord.h"
#include "core/session/RecordStream.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDateTime>

#include <algorithm>

namespace NereusSDR {

ModMonitorPublisher::ModMonitorPublisher(RadioModel* radio, RecordStream* tx,
                                         RecordStream* feedback,
                                         std::function<void()> scheduleFlush, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_streams{tx, feedback}
    , m_scheduleFlush(std::move(scheduleFlush))
{
    m_timer.setInterval(ModMonitorRecord::kPublishIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ModMonitorPublisher::tick);
    // The Core feeds its analyzers only while a device watches.
    if (m_radio) {
        m_radio->setAmModTxTapEnabled(false);
        m_radio->setAmModFeedbackWanted(false);
    }
}

ModMonitorPublisher::~ModMonitorPublisher() = default;

bool ModMonitorPublisher::isAmFamily(DSPMode mode)
{
    return mode == DSPMode::AM || mode == DSPMode::SAM || mode == DSPMode::DSB;
}

RecordStream* ModMonitorPublisher::stream(int source) const
{
    return source == 0 || source == 1 ? m_streams[static_cast<std::size_t>(source)] : nullptr;
}

bool ModMonitorPublisher::sourceOpen(int source) const
{
    return (source == 0 || source == 1) && m_open[static_cast<std::size_t>(source)];
}

void ModMonitorPublisher::subscriptionsChanged()
{
    const bool watched = (m_streams[0] != nullptr && m_streams[0]->subscriberCount() > 0)
        || (m_streams[1] != nullptr && m_streams[1]->subscriberCount() > 0);
    if (watched && !m_timer.isActive()) {
        m_timer.start();
    }
    // Open or close each source now, not at the next read: a source nobody
    // watches is detached at once.
    tick();
    if (!watched) {
        m_timer.stop();
    }
}

void ModMonitorPublisher::flushed()
{
    m_unsent[0].reset();
    m_unsent[1].reset();
}

bool ModMonitorPublisher::resetSource(int source)
{
    if (source != 0 && source != 1) {
        return false;
    }
    if (m_radio) {
        if (AmModulationAnalyzer* analyzer = m_radio->amModulationAnalyzer(source)) {
            analyzer->reset();
        }
    }
    m_unsent[static_cast<std::size_t>(source)].reset();
    return true;
}

bool ModMonitorPublisher::keyedInAmFamily() const
{
    if (!m_radio || !m_radio->isTransmitting()) {
        return false;
    }
    const SliceModel* slice = m_radio->txBoundSlice();
    return slice != nullptr && isAmFamily(slice->dspMode());
}

void ModMonitorPublisher::setOpen(int source, bool open)
{
    const auto index = static_cast<std::size_t>(source);
    if (m_open[index] == open) {
        return;
    }
    m_open[index] = open;
    if (m_radio) {
        if (open) {
            // A fresh carrier estimate: the analyzer was not fed while
            // closed, so what it holds is from an earlier transmission.
            if (AmModulationAnalyzer* analyzer = m_radio->amModulationAnalyzer(source)) {
                analyzer->reset();
                if (source == 1) {
                    const int fs = m_radio->streamSampleRateHz(m_radio->amModFeedbackStream());
                    if (fs > 0) {
                        analyzer->setSampleRate(fs);
                    }
                }
            }
        }
        if (source == 0) {
            m_radio->setAmModTxTapEnabled(open);
        } else {
            m_radio->setAmModFeedbackWanted(open);
        }
    }
    m_unsent[index].reset();
    if (!open) {
        // The window's copy goes: no carrier while nothing is measured.
        if (RecordStream* s = stream(source); s != nullptr
            && s->contains(QLatin1String(ModMonitorRecord::kRecordId))) {
            s->remove(QLatin1String(ModMonitorRecord::kRecordId));
            if (m_scheduleFlush) {
                m_scheduleFlush();
            }
        }
    }
}

void ModMonitorPublisher::tick()
{
    const bool keyed = keyedInAmFamily();
    bool published = false;
    for (int source = 0; source < 2; ++source) {
        RecordStream* s = stream(source);
        const bool watched = s != nullptr && s->subscriberCount() > 0;
        setOpen(source, watched && keyed);
        if (!m_open[static_cast<std::size_t>(source)] || !m_radio) {
            continue;
        }
        AmModulationAnalyzer* analyzer = m_radio->amModulationAnalyzer(source);
        if (analyzer == nullptr) {
            continue;
        }
        AmModulationAnalyzer::Snapshot snap = analyzer->snapshot();
        // Two reads before one send: keep the larger window peaks.
        auto& unsent = m_unsent[static_cast<std::size_t>(source)];
        if (unsent) {
            snap.posPeakPct = std::max(snap.posPeakPct, unsent->posPeakPct);
            snap.negPeakPct = std::max(snap.negPeakPct, unsent->negPeakPct);
        }
        unsent = snap;
        s->upsert(QLatin1String(ModMonitorRecord::kRecordId),
                  ModMonitorRecord::toFields(snap, QDateTime::currentMSecsSinceEpoch()));
        published = true;
    }
    if (published && m_scheduleFlush) {
        m_scheduleFlush();
    }
}

} // namespace NereusSDR
