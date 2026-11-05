// no-port-check: NereusSDR-original. The SSB Mod Monitor's readings on the
// link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/SsbModMonitorRecord.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-11-04  Created for the SSB Mod Monitor. Brian Sutton (W4KZO),
//               AI-assisted via xAI Grok.
//   2026-11-05  Corrections for review findings.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

double wirePct(double value)
{
    return std::clamp(finiteOr(value, 0.0), 0.0, kMaxPct);
}

double wireDbfs(double value)
{
    return std::clamp(finiteOr(value, kMinDbfs), kMinDbfs, kMaxDbfs);
}

bool inRange(double value, double low, double high)
{
    return value >= low && value <= high;
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
        const auto tenths = static_cast<std::int16_t>(bits);
        if (tenths < 0) {
            return false;
        }
        out.push_back(static_cast<float>(tenths) / 10.0f);
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
    QJsonObject fields{
        {kAtMs, static_cast<double>(atMs)},
        {kPeakPct, wirePct(s.peakPct)},
        {kAvgPct, wirePct(s.avgPct)},
        {kPeakHoldPct, wirePct(s.peakHoldPct)},
        {kPeakDbfs, wireDbfs(s.peakDbfs)},
        {kAvgDbfs, wireDbfs(s.avgDbfs)},
        {kPeakHoldDbfs, wireDbfs(s.peakHoldDbfs)},
        {kSignalPresent, s.signalPresent},
        {kScopeRate, reducedBy > 0 ? s.scopeRateHz / reducedBy : s.scopeRateHz},
        {kScope, scope},
    };
    if (s.crestDb && std::isfinite(*s.crestDb)) {
        fields.insert(kCrestDb, std::clamp(*s.crestDb, -kMaxCrestDb, kMaxCrestDb));
    }
    return fields;
}

std::optional<SsbModulationAnalyzer::Snapshot> fromFields(const QJsonObject& f)
{
    for (const QString& key : {kPeakPct, kAvgPct, kPeakHoldPct}) {
        if (!f.value(key).isDouble() || !inRange(f.value(key).toDouble(), 0.0, kMaxPct)) {
            return std::nullopt;
        }
    }
    for (const QString& key : {kPeakDbfs, kAvgDbfs, kPeakHoldDbfs}) {
        if (!f.value(key).isDouble() || !inRange(f.value(key).toDouble(), kMinDbfs, kMaxDbfs)) {
            return std::nullopt;
        }
    }
    // toInt() answers -1 for a number that is not a whole one.
    const int scopeRate = f.value(kScopeRate).toInt(-1);
    if (!inRange(scopeRate, 0, kMaxScopeRateHz)) {
        return std::nullopt;
    }
    const QJsonValue crest = f.value(kCrestDb);
    if (!crest.isUndefined()
        && (!crest.isDouble() || !inRange(crest.toDouble(), -kMaxCrestDb, kMaxCrestDb))) {
        return std::nullopt;
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
    if (crest.isDouble()) {
        s.crestDb = crest.toDouble();
    }
    s.signalPresent = f.value(kSignalPresent).toBool();
    s.scopeRateHz = scopeRate;
    if (!decodeScope(f.value(kScope).toString(), &s.scope)) {
        return std::nullopt;
    }
    return s;
}

} // namespace NereusSDR::SsbModMonitorRecord
