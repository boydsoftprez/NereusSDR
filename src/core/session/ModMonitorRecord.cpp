// no-port-check: NereusSDR-original. The AM Mod Monitor's readings on the
// link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/ModMonitorRecord.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See ModMonitorRecord.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27  J.J. Boyd / KG4VCF  Created (R-IOS-13, R-R3-49).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/ModMonitorRecord.h"

#include <QByteArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace NereusSDR::ModMonitorRecord {

namespace {

const QString kAtMs = QStringLiteral("atMs");
const QString kPosPeak = QStringLiteral("posPeakPct");
const QString kNegPeak = QStringLiteral("negPeakPct");
const QString kPosHold = QStringLiteral("posHoldPct");
const QString kNegHold = QStringLiteral("negHoldPct");
const QString kCarrierLevel = QStringLiteral("carrierLevel");
const QString kCarrierDbfs = QStringLiteral("carrierDbfs");
const QString kCarrierPresent = QStringLiteral("carrierPresent");
const QString kCarrierLow = QStringLiteral("carrierLow");
const QString kCarrierHigh = QStringLiteral("carrierHigh");
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
        ? (n + kWireScopePoints - 1) / kWireScopePoints
        : 1;
    if (reducedBy != nullptr) {
        *reducedBy = static_cast<int>(group);
    }
    QByteArray bytes;
    bytes.reserve(static_cast<int>((n + group - 1) / group * 2));
    for (std::size_t start = 0; start < n; start += group) {
        // The largest-magnitude point of the group, as the analyzer keeps
        // the largest of each decimation group (AmModulationAnalyzer.cpp,
        // pushScopeLocked).
        float pick = pct[start];
        for (std::size_t i = start + 1; i < std::min(n, start + group); ++i) {
            if (std::fabs(pct[i]) > std::fabs(pick)) {
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

QJsonObject toFields(const AmModulationAnalyzer::Snapshot& s, qint64 atMs)
{
    int reducedBy = 1;
    const QString scope = encodeScope(s.scope, &reducedBy);
    return QJsonObject{
        {kAtMs, static_cast<double>(atMs)},
        {kPosPeak, finiteOr(s.posPeakPct, 0.0)},
        {kNegPeak, finiteOr(s.negPeakPct, 0.0)},
        {kPosHold, finiteOr(s.posHoldPct, 0.0)},
        {kNegHold, finiteOr(s.negHoldPct, 0.0)},
        {kCarrierLevel, finiteOr(s.carrierLevel, 0.0)},
        {kCarrierDbfs, finiteOr(s.carrierDbfs, -120.0)},
        {kCarrierPresent, s.carrierPresent},
        {kCarrierLow, s.carrierLow},
        {kCarrierHigh, s.carrierHigh},
        {kScopeRate, reducedBy > 0 ? s.scopeRateHz / reducedBy : s.scopeRateHz},
        {kScope, scope},
    };
}

std::optional<AmModulationAnalyzer::Snapshot> fromFields(const QJsonObject& f)
{
    for (const QString& key : {kPosPeak, kNegPeak, kPosHold, kNegHold, kCarrierLevel,
                               kCarrierDbfs, kScopeRate}) {
        if (!f.value(key).isDouble()) {
            return std::nullopt;
        }
    }
    for (const QString& key : {kCarrierPresent, kCarrierLow, kCarrierHigh}) {
        if (!f.value(key).isBool()) {
            return std::nullopt;
        }
    }
    if (!f.value(kScope).isString()) {
        return std::nullopt;
    }
    AmModulationAnalyzer::Snapshot s;
    s.posPeakPct = f.value(kPosPeak).toDouble();
    s.negPeakPct = f.value(kNegPeak).toDouble();
    s.posHoldPct = f.value(kPosHold).toDouble();
    s.negHoldPct = f.value(kNegHold).toDouble();
    s.carrierLevel = f.value(kCarrierLevel).toDouble();
    s.carrierDbfs = f.value(kCarrierDbfs).toDouble();
    s.carrierPresent = f.value(kCarrierPresent).toBool();
    s.carrierLow = f.value(kCarrierLow).toBool();
    s.carrierHigh = f.value(kCarrierHigh).toBool();
    s.scopeRateHz = f.value(kScopeRate).toInt();
    if (!decodeScope(f.value(kScope).toString(), &s.scope)) {
        return std::nullopt;
    }
    return s;
}

} // namespace NereusSDR::ModMonitorRecord
