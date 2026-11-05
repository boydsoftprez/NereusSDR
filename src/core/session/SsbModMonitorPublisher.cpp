// no-port-check: NereusSDR-original. The Core's side of the SSB Mod
// Monitor's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/SsbModMonitorPublisher.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-11-04  Created for the SSB Mod Monitor. Brian Sutton (W4KZO),
//               AI-assisted via xAI Grok.
//   2026-11-05  Corrections for review findings.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/SsbModMonitorPublisher.h"

#include "core/session/SsbModMonitorRecord.h"
#include "core/session/RecordStream.h"
#include "models/RadioModel.h"

#include <QDateTime>

#include <algorithm>

namespace NereusSDR {

SsbModMonitorPublisher::SsbModMonitorPublisher(RadioModel* radio, RecordStream* tx,
                                               RecordStream* feedback,
                                               std::function<void()> scheduleFlush, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_streams{tx, feedback}
    , m_scheduleFlush(std::move(scheduleFlush))
{
    m_timer.setInterval(SsbModMonitorRecord::kPublishIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &SsbModMonitorPublisher::tick);
    if (m_radio) {
        m_radio->setSsbModTxTapEnabled(false);
        m_radio->setSsbModFeedbackWanted(false);
    }
}

SsbModMonitorPublisher::~SsbModMonitorPublisher() = default;

RecordStream* SsbModMonitorPublisher::stream(int source) const
{
    return source == 0 || source == 1 ? m_streams[static_cast<std::size_t>(source)] : nullptr;
}

bool SsbModMonitorPublisher::sourceOpen(int source) const
{
    return (source == 0 || source == 1) && m_open[static_cast<std::size_t>(source)];
}

void SsbModMonitorPublisher::subscriptionsChanged()
{
    const bool watched = (m_streams[0] != nullptr && m_streams[0]->subscriberCount() > 0)
        || (m_streams[1] != nullptr && m_streams[1]->subscriberCount() > 0);
    if (watched && !m_timer.isActive()) {
        m_timer.start();
    }
    tick();
    if (!watched) {
        m_timer.stop();
    }
}

void SsbModMonitorPublisher::flushed()
{
    m_unsent[0].reset();
    m_unsent[1].reset();
}

bool SsbModMonitorPublisher::resetSource(int source)
{
    if (source != 0 && source != 1) {
        return false;
    }
    if (m_radio) {
        if (SsbModulationAnalyzer* analyzer = m_radio->ssbModulationAnalyzer(source)) {
            analyzer->reset();
        }
    }
    m_unsent[static_cast<std::size_t>(source)].reset();
    return true;
}

void SsbModMonitorPublisher::setOpen(int source, bool open)
{
    const auto index = static_cast<std::size_t>(source);
    const bool changed = m_open[index] != open;
    m_open[index] = open;
    if (m_radio) {
        if (changed && open) {
            if (SsbModulationAnalyzer* analyzer = m_radio->ssbModulationAnalyzer(source)) {
                analyzer->reset();
            }
        }
        // Re-applied every read. A one-time set was left off if the
        // feedback flag was cleared while this source stayed open.
        if (source == 0) {
            m_radio->setSsbModTxTapEnabled(open);
        } else {
            m_radio->setSsbModFeedbackWanted(open);
        }
    }
    if (!changed) {
        return;
    }
    m_unsent[index].reset();
    if (!open) {
        if (RecordStream* s = stream(source); s != nullptr
            && s->contains(QLatin1String(SsbModMonitorRecord::kRecordId))) {
            s->remove(QLatin1String(SsbModMonitorRecord::kRecordId));
            if (m_scheduleFlush) {
                m_scheduleFlush();
            }
        }
    }
}

void SsbModMonitorPublisher::tick()
{
    const bool keyed = m_radio && m_radio->ssbTxActive();
    const bool feedback = m_radio && m_radio->ssbModFeedbackInUse();
    bool published = false;
    for (int source = 0; source < 2; ++source) {
        RecordStream* s = stream(source);
        const bool watched = s != nullptr && s->subscriberCount() > 0;
        setOpen(source, watched && keyed && (source == 0 || feedback));
        if (!m_open[static_cast<std::size_t>(source)] || !m_radio) {
            continue;
        }
        SsbModulationAnalyzer* analyzer = m_radio->ssbModulationAnalyzer(source);
        if (analyzer == nullptr) {
            continue;
        }
        SsbModulationAnalyzer::Snapshot snap = analyzer->snapshot();
        auto& unsent = m_unsent[static_cast<std::size_t>(source)];
        if (unsent) {
            snap.peakPct = std::max(snap.peakPct, unsent->peakPct);
            if (!snap.crestDb) {
                snap.crestDb = unsent->crestDb;
            }
            snap.signalPresent = snap.signalPresent || unsent->signalPresent;
        }
        unsent = snap;
        s->upsert(QLatin1String(SsbModMonitorRecord::kRecordId),
                  SsbModMonitorRecord::toFields(snap, QDateTime::currentMSecsSinceEpoch()));
        published = true;
    }
    if (published && m_scheduleFlush) {
        m_scheduleFlush();
    }
}

} // namespace NereusSDR
