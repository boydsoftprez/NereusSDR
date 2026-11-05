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
// Modification history (NereusSDR):
//   2026-11-04  J.J. Boyd / KG4VCF  Created for the SSB Mod Monitor.
//                                    AI-assisted via xAI Grok.
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

QString streamName(int source);
int sourceOfStream(const QString& stream);

QJsonObject toFields(const SsbModulationAnalyzer::Snapshot& snapshot, qint64 atMs);
std::optional<SsbModulationAnalyzer::Snapshot> fromFields(const QJsonObject& fields);

QString encodeScope(const std::vector<float>& pct, int* reducedBy = nullptr);
bool decodeScope(const QString& text, std::vector<float>* pct);

} // namespace NereusSDR::SsbModMonitorRecord
