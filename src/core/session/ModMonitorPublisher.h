#pragma once
// no-port-check: NereusSDR-original. The Core's side of the AM Mod
// Monitor's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/ModMonitorPublisher.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port (the analyzer is NereusSDR's own).
// R-IOS-13, R-R3-49 (iPhone plan Task 39 row A10; txModMonitorVersion 1).
//
// The Core runs its two AM modulation analyzers (RadioModel's TX I/Q tap
// and PureSignal feedback analyzer) only for devices that watch them. A
// source is open while at least one peer subscribes to its stream
// (ModMonitorRecord::kTxStream, kFeedbackStream) and the radio is keyed in
// an AM-family mode (AM, SAM or DSB, the transmit slice's mode). While a
// source is open the Core reads its analyzer at the applet's own refresh
// (ModMonitorRecord::kPublishIntervalMs) and upserts the stream's one
// record; when it closes, the analyzer is detached (the TX tap removed from
// the transmit channel, the feedback fork turned off) and the record is
// removed, so a watching window shows no carrier.
//
// With no subscriber no timer runs and neither analyzer is fed: nothing is
// spent on the monitor. The analyzer is reset each time a source opens, so
// a device that starts watching mid-key starts from a fresh carrier
// estimate rather than one left from an earlier transmission.
//
// Window peaks: AmModulationAnalyzer::snapshot() hands out the peaks of the
// window since the previous read. Records are sent at most every 50 ms
// (the link's delta flush), so two reads can merge into one send; the
// publisher keeps the largest window peaks of the reads not yet sent, and
// flushed() starts a new window, so no peak is lost between sends.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (R-IOS-13, R-R3-49).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/AmModulationAnalyzer.h"
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

class ModMonitorPublisher : public QObject {
    Q_OBJECT

public:
    /// `tx` and `feedback` are the two streams (owned by the station
    /// server); `scheduleFlush` asks for the next record send. Detaches the
    /// Core's TX tap at once: the Core feeds the analyzer only for a
    /// watching device.
    ModMonitorPublisher(RadioModel* radio, RecordStream* tx, RecordStream* feedback,
                        std::function<void()> scheduleFlush, QObject* parent = nullptr);
    ~ModMonitorPublisher() override;

    /// AM, SAM and DSB: the modes the monitor measures.
    static bool isAmFamily(DSPMode mode);

    /// A subscription changed (records.subscribe, records.unsubscribe, a
    /// dropped peer): starts or stops the reads.
    void subscriptionsChanged();
    /// The record send took what waited: the next window starts.
    void flushed();
    /// txModMonitor.reset: clears the source's analyzer (its carrier
    /// estimate, peaks, hold and scope), as the local applet's RESET does.
    /// False for a source other than 0 or 1.
    bool resetSource(int source);

    /// Whether the reads run (at least one subscriber).
    bool publishing() const { return m_timer.isActive(); }
    /// Whether a source is open (subscribed, and keyed in an AM mode).
    bool sourceOpen(int source) const;
    /// For a test: one read now, as the timer's would.
    void tickForTest() { tick(); }

private:
    void tick();
    bool keyedInAmFamily() const;
    RecordStream* stream(int source) const;
    void setOpen(int source, bool open);

    QPointer<RadioModel> m_radio;
    std::array<RecordStream*, 2> m_streams{};
    std::function<void()> m_scheduleFlush;
    QTimer m_timer;
    std::array<bool, 2> m_open{false, false};
    // The reads not yet sent, merged (the largest window peaks).
    std::array<std::optional<AmModulationAnalyzer::Snapshot>, 2> m_unsent;
};

} // namespace NereusSDR
