#pragma once
// no-port-check: NereusSDR-original. The SSB Mod Monitor's readings on the
// link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/SsbModMonitorRecord.h  (NereusSDR)
// =================================================================
//
// The wire form of one SsbModulationAnalyzer::Snapshot. The Core sends it
// on txSsbModulation (the TX I/Q the Core sends its radio) and
// txSsbModulationFeedback (the PureSignal feedback receiver). One record
// each, id "0". A window decodes it and hands the snapshot to the applet.
//
// The scope trace travels as scopePctTenths: each point's percent of full
// scale, in tenths, a little-endian int16, base64, at most kWireScopePoints
// points. A longer trace keeps the largest point of each group.
//
// crestDb is peak over RMS of the envelope over the analyzer's crest window.
// The field is left out while that window holds no signal. A decoder takes
// only values in the ranges below and refuses the record otherwise.
//
// Modification history (NereusSDR):
//   2026-11-04  Created for the SSB Mod Monitor. Brian Sutton (W4KZO),
//               AI-assisted via xAI Grok.
//   2026-11-05  Corrections for review findings.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/SsbModulationAnalyzer.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace NereusSDR::SsbModMonitorRecord {

inline constexpr const char* kTxStream = "txSsbModulation";
inline constexpr const char* kFeedbackStream = "txSsbModulationFeedback";
inline constexpr const char* kRecordId = "0";
inline constexpr int kCapacity = 1;
inline constexpr int kPublishIntervalMs = 33;
inline constexpr int kWireScopePoints = 512;
// What the wire form can carry: int16 tenths for percent, and the dB those
// percentages make.
inline constexpr double kMaxPct = 3276.7;
inline constexpr double kMinDbfs = -120.0;
inline constexpr double kMaxDbfs = 71.0;
inline constexpr double kMaxCrestDb = 200.0;
inline constexpr int kMaxScopeRateHz = 1536000;

QString streamName(int source);
int sourceOfStream(const QString& stream);

QJsonObject toFields(const SsbModulationAnalyzer::Snapshot& snapshot, qint64 atMs);
std::optional<SsbModulationAnalyzer::Snapshot> fromFields(const QJsonObject& fields);

QString encodeScope(const std::vector<float>& pct, int* reducedBy = nullptr);
bool decodeScope(const QString& text, std::vector<float>* pct);

} // namespace NereusSDR::SsbModMonitorRecord
