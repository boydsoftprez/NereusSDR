#pragma once
// no-port-check: NereusSDR-original. The AM Mod Monitor's readings on the
// link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/ModMonitorRecord.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port (the analyzer is NereusSDR's own,
// AmModulationAnalyzer). R-IOS-13, R-R3-49 (iPhone plan Task 39 row A10).
//
// The wire form of one AmModulationAnalyzer::Snapshot, as the Core sends it
// on the `txAmModulation` (the TX I/Q the Core sends its radio) and
// `txAmModulationFeedback` (the PureSignal feedback receiver, the PA's
// output) record streams, one record each, id "0" (the link document,
// section 7.7). The Core encodes, a window decodes and hands the snapshot
// to the applet's own display path, so a remote window shows what a local
// one shows for the same I/Q.
//
// The scope trace travels as `scopePctTenths`: each point's percent
// modulation in tenths, a little-endian int16, in base64, at most
// kWireScopePoints points. A longer trace is reduced by keeping the
// largest-magnitude point of each group, the analyzer's own decimation
// rule, so the peaks the operator looks for survive.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (R-IOS-13, R-R3-49).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/AmModulationAnalyzer.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace NereusSDR::ModMonitorRecord {

/// The two streams, by the applet's source (0 TX I/Q, 1 PA feedback).
inline constexpr const char* kTxStream = "txAmModulation";
inline constexpr const char* kFeedbackStream = "txAmModulationFeedback";
/// The one record each stream holds.
inline constexpr const char* kRecordId = "0";
/// Each stream's capacity: one record.
inline constexpr int kCapacity = 1;
/// How often the Core reads the analyzer while it publishes: the applet's
/// own refresh (ModMonitorApplet kRefreshMs).
inline constexpr int kPublishIntervalMs = 33;
/// The most scope points one record carries.
inline constexpr int kWireScopePoints = 512;

/// The stream for a source, or an empty string for any other value.
QString streamName(int source);
/// The source a stream carries: 0, 1, or -1 for any other name.
int sourceOfStream(const QString& stream);

/// The Core's record: the snapshot's readings, with `atMs` (the Core's
/// clock, ms since the epoch, when it read them).
QJsonObject toFields(const AmModulationAnalyzer::Snapshot& snapshot, qint64 atMs);

/// A window's copy of the Core's snapshot, or nullopt when a field is
/// missing or of the wrong type. `scopeRateHz` is the rate of the points
/// carried (after any reduction).
std::optional<AmModulationAnalyzer::Snapshot> fromFields(const QJsonObject& fields);

/// The scope as it travels: reduced to at most kWireScopePoints points,
/// each rounded to a tenth of a percent and held to the int16 range.
QString encodeScope(const std::vector<float>& pct, int* reducedBy = nullptr);
/// The inverse; false when the text is not base64 of whole int16 values.
bool decodeScope(const QString& text, std::vector<float>* pct);

} // namespace NereusSDR::ModMonitorRecord
