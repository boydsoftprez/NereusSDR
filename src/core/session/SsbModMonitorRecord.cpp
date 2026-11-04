// no-port-check: NereusSDR-original. The SSB Mod Monitor's readings on the
// link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/SsbModMonitorRecord.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-11-04  J.J. Boyd / KG4VCF  Created for the SSB Mod Monitor.
//                                    AI-assisted via xAI Grok.
// =================================================================

#include "core/session/SsbModMonitorRecord.h"

#include <QByteArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace NereusSDR::SsbModMonitorRecord {

namespace {

const QString kAtMs = QStringLiteral("atMs");
const QString kPeakPct = QStringLiteral("peakPct");
const QString kAvgPct = QStringLiteral("avgPct");
const QString kPeakHoldPct = QStringLiteral("peakHoldPct");
const QString kPeakDbfs = QStringLiteral("peakDbfs");
const QString kAvgDbfs = QStringLiteral("avgDbfs");
const QString kPeakHoldDbfs = QStringLiteral("peakHoldDbfs");
const QString kCrestDb = QStringLiteral("crestDb");
const QString kSignalPresent = QStringLiteral("signalPresent");
const QString kScopeRate = QStringLiteral("scopeRateHz");
const QString kScope = QStringLiteral("scopePctTenths");

double finiteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

QString streamName(int source)
{
    if (source == 0) {
        return QString::fromLatin1(kTxStream);
    }
    if (source == 1) {
        return QString::fromLatin1(kFeedbackStream);
    }
    return {};
}

int sourceOfStream(const QString& stream)
{
    if (stream == QLatin1String(kTxStream)) {
        return 0;
    }
    if (stream == QLatin1String(kFeedbackStream)) {
        return 1;
    }
    return -1;
}

QString encodeScope(const std::vector<float>& pct, int* reducedBy)
{
    const std::size_t n = pct.size();
    const std::size_t group = n > static_cast<std::size_t>(kWireScopePoints)
        ? (n + static_cast<std::size_t>(kWireScopePoints) - 1) / static_cast<std::size_t>(kWireScopePoints)
        : 1;
    if (reducedBy != nullptr) {
        *reducedBy = static_cast<int>(group);
    }
    QByteArray bytes;
    bytes.reserve(static_cast<int>((n + group - 1) / group * 2));
    for (std::size_t start = 0; start < n; start += group) {
        float pick = pct[start];
        for (std::size_t i = start + 1; i < std::min(n, start + group); ++i) {
            if (pct[i] > pick) {
                pick = pct[i];
            }
        }
        const double tenths = std::isfinite(pick) ? std::round(pick * 10.0) : 0.0;
        const auto value = static_cast<std::int16_t>(std::clamp(
            tenths, static_cast<double>(std::numeric_limits<std::int16_t>::min()),
            static_cast<double>(std::numeric_limits<std::int16_t>::max())));
        const auto bits = static_cast<std::uint16_t>(value);
        bytes.append(static_cast<char>(bits & 0xFF));
        bytes.append(static_cast<char>((bits >> 8) & 0xFF));
    }
    return QString::fromLatin1(bytes.toBase64());
}

bool decodeScope(const QString& text, std::vector<float>* pct)
{
    const auto decoded = QByteArray::fromBase64Encoding(
        text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() % 2 != 0
        || decoded.decoded.size() > kWireScopePoints * 2) {
        return false;
    }
    const QByteArray& bytes = decoded.decoded;
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(bytes.size() / 2));
    for (int i = 0; i + 1 < bytes.size(); i += 2) {
        const auto bits = static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(bytes[i])
            | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[i + 1])) << 8));
        out.push_back(static_cast<float>(static_cast<std::int16_t>(bits)) / 10.0f);
    }
    if (pct != nullptr) {
        *pct = std::move(out);
    }
    return true;
}

QJsonObject toFields(const SsbModulationAnalyzer::Snapshot& s, qint64 atMs)
{
    int reducedBy = 1;
    const QString scope = encodeScope(s.scope, &reducedBy);
    return QJsonObject{
        {kAtMs, static_cast<double>(atMs)},
        {kPeakPct, finiteOr(s.peakPct, 0.0)},
        {kAvgPct, finiteOr(s.avgPct, 0.0)},
        {kPeakHoldPct, finiteOr(s.peakHoldPct, 0.0)},
        {kPeakDbfs, finiteOr(s.peakDbfs, -120.0)},
        {kAvgDbfs, finiteOr(s.avgDbfs, -120.0)},
        {kPeakHoldDbfs, finiteOr(s.peakHoldDbfs, -120.0)},
        {kCrestDb, finiteOr(s.crestDb, 0.0)},
        {kSignalPresent, s.signalPresent},
        {kScopeRate, reducedBy > 0 ? s.scopeRateHz / reducedBy : s.scopeRateHz},
        {kScope, scope},
    };
}

std::optional<SsbModulationAnalyzer::Snapshot> fromFields(const QJsonObject& f)
{
    for (const QString& key : {kPeakPct, kAvgPct, kPeakHoldPct, kPeakDbfs, kAvgDbfs,
                               kPeakHoldDbfs, kCrestDb, kScopeRate}) {
        if (!f.value(key).isDouble()) {
            return std::nullopt;
        }
    }
    if (!f.value(kSignalPresent).isBool() || !f.value(kScope).isString()) {
        return std::nullopt;
    }
    SsbModulationAnalyzer::Snapshot s;
    s.peakPct = f.value(kPeakPct).toDouble();
    s.avgPct = f.value(kAvgPct).toDouble();
    s.peakHoldPct = f.value(kPeakHoldPct).toDouble();
    s.peakDbfs = f.value(kPeakDbfs).toDouble();
    s.avgDbfs = f.value(kAvgDbfs).toDouble();
    s.peakHoldDbfs = f.value(kPeakHoldDbfs).toDouble();
    s.crestDb = f.value(kCrestDb).toDouble();
    s.signalPresent = f.value(kSignalPresent).toBool();
    s.scopeRateHz = f.value(kScopeRate).toInt();
    if (!decodeScope(f.value(kScope).toString(), &s.scope)) {
        return std::nullopt;
    }
    return s;
}

} // namespace NereusSDR::SsbModMonitorRecord
