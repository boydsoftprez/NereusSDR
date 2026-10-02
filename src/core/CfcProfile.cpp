// =================================================================
// src/core/CfcProfile.cpp (NereusSDR)
// Ported from Thetis source:
//   Project Files/Source/Console/frmCFCConfig.cs
//   Project Files/Source/Console/ucParametricEq.cs
// Modification history (NereusSDR):
//   2026-10-02 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via OpenAI Codex.
// =================================================================

// --- From frmCFCConfig.cs ---
/*  frmCFCConfig.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From ucParametricEq.cs ---
/*  ucParametricEq.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "CfcProfile.h"
#include "ParaEqEnvelope.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace NereusSDR {
namespace {
bool inRange(double value, double low, double high)
{
    return std::isfinite(value) && value >= low && value <= high;
}
bool validCurve(const CfcCurveState& curve, bool compression)
{
    const qsizetype count = curve.frequenciesHz.size();
    if ((count != 5 && count != 10 && count != 18)
        || curve.gainsDb.size() != count || curve.q.size() != count
        || !inRange(curve.frequencyMinHz, 0, 20000)
        || !inRange(curve.frequencyMaxHz, 0, 20000)
        || curve.frequencyMaxHz - curve.frequencyMinHz < 1000
        || !inRange(curve.globalGainDb, compression ? 0 : -24, compression ? 16 : 24)) {
        return false;
    }
    // From Thetis ucParametricEq.cs:3280-3281 [v2.10.3.15] — endpoint anchors.
    if (curve.frequenciesHz.first() != curve.frequencyMinHz
        || curve.frequenciesHz.last() != curve.frequencyMaxHz) { return false; }
    for (qsizetype i = 0; i < count; ++i) {
        if (!inRange(curve.frequenciesHz[i], curve.frequencyMinHz, curve.frequencyMaxHz)
            || !inRange(curve.gainsDb[i], compression ? 0 : -24, compression ? 16 : 24)
            || !inRange(curve.q[i], 0.2, 20)
            || (i > 0 && curve.frequenciesHz[i] < curve.frequenciesHz[i - 1])) {
            return false;
        }
    }
    return true;
}
// From Thetis ucParametricEq.cs:1460-1486 [v2.10.3.15] — saved precision and member names.
QJsonObject encodeCurve(const CfcCurveState& curve)
{
    const auto rounded = [](double v, double scale) { return std::nearbyint(v * scale) / scale; };
    QJsonArray points;
    for (qsizetype i = 0; i < curve.frequenciesHz.size(); ++i) {
        points.append(QJsonObject{{"FrequencyHz", rounded(curve.frequenciesHz[i], 1000)},
                                  {"GainDb", rounded(curve.gainsDb[i], 10)},
                                  {"Q", rounded(curve.q[i], 100)}});
    }
    return {{"BandCount", curve.frequenciesHz.size()}, {"ParametricEQ", curve.useQ},
            {"GlobalGainDb", rounded(curve.globalGainDb, 10)},
            {"FrequencyMinHz", rounded(curve.frequencyMinHz, 1000)},
            {"FrequencyMaxHz", rounded(curve.frequencyMaxHz, 1000)}, {"Points", points}};
}
QJsonValue member(const QJsonObject& object, const char* pascal, const char* snake)
{
    return object.contains(QLatin1String(pascal)) ? object.value(QLatin1String(pascal))
                                                : object.value(QLatin1String(snake));
}
std::optional<CfcCurveState> decodeCurve(const QString& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) { return std::nullopt; }
    const QJsonObject root = doc.object();
    const QJsonValue count = member(root, "BandCount", "band_count");
    const QJsonValue useQ = member(root, "ParametricEQ", "parametric_eq");
    const QJsonValue global = member(root, "GlobalGainDb", "global_gain_db");
    const QJsonValue low = member(root, "FrequencyMinHz", "frequency_min_hz");
    const QJsonValue high = member(root, "FrequencyMaxHz", "frequency_max_hz");
    const QJsonValue points = member(root, "Points", "points");
    if (!count.isDouble() || !useQ.isBool() || !global.isDouble() || !low.isDouble()
        || !high.isDouble() || !points.isArray()
        || (count.toDouble() != 5 && count.toDouble() != 10 && count.toDouble() != 18)
        || count.toDouble() != points.toArray().size()) {
        return std::nullopt;
    }
    CfcCurveState curve;
    curve.useQ = useQ.toBool();
    curve.globalGainDb = global.toDouble();
    curve.frequencyMinHz = low.toDouble();
    curve.frequencyMaxHz = high.toDouble();
    for (const QJsonValue& point : points.toArray()) {
        if (!point.isObject()) { return std::nullopt; }
        const QJsonObject p = point.toObject();
        const QJsonValue f = member(p, "FrequencyHz", "frequency_hz");
        const QJsonValue g = member(p, "GainDb", "gain_db");
        const QJsonValue q = member(p, "Q", "q");
        if (!f.isDouble() || !g.isDouble() || !q.isDouble()) { return std::nullopt; }
        curve.frequenciesHz.append(f.toDouble());
        curve.gainsDb.append(g.toDouble());
        curve.q.append(q.toDouble());
    }
    return curve;
}
} // namespace

bool isValidCfcProfile(const CfcProfile& profile)
{
    return validCurve(profile.compression, true) && validCurve(profile.postEq, false)
        && profile.compression.frequenciesHz == profile.postEq.frequenciesHz
        && profile.compression.frequencyMinHz == profile.postEq.frequencyMinHz
        && profile.compression.frequencyMaxHz == profile.postEq.frequencyMaxHz;
}
// From Thetis frmCFCConfig.cs:492-575 [v2.10.3.15] — two JSON graphs in one gzip envelope.
QString encodeCfcProfile(const CfcProfile& profile)
{
    if (!isValidCfcProfile(profile)) { return {}; }
    const QString comp = QString::fromUtf8(QJsonDocument(encodeCurve(profile.compression)).toJson());
    const QString eq = QString::fromUtf8(QJsonDocument(encodeCurve(profile.postEq)).toJson());
    return ParaEqEnvelope::encode(comp + QStringLiteral("<SEP>") + eq);
}
std::optional<CfcProfile> decodeCfcProfile(const QString& blob)
{
    const std::optional<QString> payload = ParaEqEnvelope::decode(blob);
    if (!payload) { return std::nullopt; }
    const QStringList parts = payload->split(QStringLiteral("<SEP>"));
    if (parts.size() != 2) { return std::nullopt; }
    const std::optional<CfcCurveState> comp = decodeCurve(parts[0]);
    const std::optional<CfcCurveState> eq = decodeCurve(parts[1]);
    if (!comp || !eq) { return std::nullopt; }
    const CfcProfile profile{*comp, *eq};
    return isValidCfcProfile(profile) ? std::optional<CfcProfile>(profile) : std::nullopt;
}
} // namespace NereusSDR
