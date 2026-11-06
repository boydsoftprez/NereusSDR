#pragma once
// no-port-check: NereusSDR-original. The Core's side of the SSB Mod
// Monitor's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/SsbModMonitorPublisher.h  (NereusSDR)
// =================================================================
//
// The Core runs its two SSB modulation analyzers only for devices that
// watch them, and only while the radio is keyed in USB or LSB. With no
// subscriber the timer is stopped and neither analyzer is fed.
//
// Modification history (NereusSDR):
//   2026-11-04  J.J. Boyd / KG4VCF  Created for the SSB Mod Monitor.
//                                    AI-assisted via xAI Grok.
// =================================================================

#include "core/SsbModulationAnalyzer.h"
#include "core/WdspTypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <array>
#include <functional>
#include <optional>

namespace NereusSDR {

class RadioModel;
class RecordStream;

class SsbModMonitorPublisher : public QObject {
    Q_OBJECT

public:
    SsbModMonitorPublisher(RadioModel* radio, RecordStream* tx, RecordStream* feedback,
                           std::function<void()> scheduleFlush, QObject* parent = nullptr);
    ~SsbModMonitorPublisher() override;

    static bool isSsbVoice(DSPMode mode);

    void subscriptionsChanged();
    void flushed();
    bool resetSource(int source);

    bool publishing() const { return m_timer.isActive(); }
    bool sourceOpen(int source) const;
    void tickForTest() { tick(); }

private:
    void tick();
    bool keyedInSsb() const;
    RecordStream* stream(int source) const;
    void setOpen(int source, bool open);

    QPointer<RadioModel> m_radio;
    std::array<RecordStream*, 2> m_streams{};
    std::function<void()> m_scheduleFlush;
    QTimer m_timer;
    std::array<bool, 2> m_open{false, false};
    std::array<std::optional<SsbModulationAnalyzer::Snapshot>, 2> m_unsent;
};

} // namespace NereusSDR
