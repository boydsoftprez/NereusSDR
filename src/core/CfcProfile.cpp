// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
// accept native Thetis PascalCase snapshots alongside the current Core format.
// CFC paired-profile codec (NereusSDR).
// Ported from Thetis frmCFCConfig.cs:333-392,492-557 and
// ucParametricEq.cs:1353-1452 [v2.10.3.15].
// Modification history (NereusSDR): 2026-09-27 J.J. Boyd (KG4VCF),
// AI-assisted via OpenAI Codex: bounded Core codec for the existing
// CFCParaEQData format.
// 2026-09-29 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
// publishedJson / revision / fromPublishedJson / legacyProfile for the
// cfc.setProfile verb (transmitSettingsVersion 15).
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

#include "core/CfcProfile.h"
#include "core/ParaEqEnvelope.h"

#include <QJsonArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QCryptographicHash>

#include <algorithm>
#include <cmath>

namespace NereusSDR::CfcProfile {
namespace {
constexpr qsizetype kMaxEncodedChars = 16 * 1024;
constexpr qsizetype kMaxDecodedBytes = 64 * 1024;

// Native edit snapshots preserve Thetis PascalCase names. Current Core and
// remote clients retain the established snake_case paired-curve contract.
QJsonValue curveValue(const QJsonObject& o, const char* key)
{
    const QString snake = QString::fromLatin1(key);
    if (o.contains(snake)) { return o.value(snake); }
    static const QHash<QString, QString> names{
        {QStringLiteral("band_count"), QStringLiteral("BandCount")},
        {QStringLiteral("parametric_eq"), QStringLiteral("ParametricEQ")},
        {QStringLiteral("global_gain_db"), QStringLiteral("GlobalGainDb")},
        {QStringLiteral("frequency_min_hz"), QStringLiteral("FrequencyMinHz")},
        {QStringLiteral("frequency_max_hz"), QStringLiteral("FrequencyMaxHz")},
        {QStringLiteral("points"), QStringLiteral("Points")},
        {QStringLiteral("frequency_hz"), QStringLiteral("FrequencyHz")},
        {QStringLiteral("gain_db"), QStringLiteral("GainDb")},
        {QStringLiteral("q"), QStringLiteral("Q")}};
    return o.value(names.value(snake));
}

bool number(const QJsonObject& o, const char* key, double lo, double hi, double& out)
{
    const QJsonValue v = curveValue(o, key);
    if (!v.isDouble()) { return false; }
    const double n = v.toDouble();
    if (!std::isfinite(n) || n < lo || n > hi) { return false; }
    out = n;
    return true;
}

bool curve(const QString& json, double gainMin, double gainMax,
           std::vector<double>& f, std::vector<double>& g,
           std::vector<double>& q, double& globalDb,
           double& minHz, double& maxHz, bool& parametric)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { return false; }
    const QJsonObject root = document.object();
    const QJsonValue countValue = curveValue(root, "band_count");
    const QJsonValue flagValue = curveValue(root, "parametric_eq");
    const QJsonValue pointsValue = curveValue(root, "points");
    if (!countValue.isDouble() || !flagValue.isBool() || !pointsValue.isArray()) { return false; }
    const int count = countValue.toInt(-1);
    const QJsonArray points = pointsValue.toArray();
    if ((count != 5 && count != 10 && count != 18) || points.size() != count) { return false; }
    if (!number(root, "frequency_min_hz", 0.0, 20000.0, minHz)
        || !number(root, "frequency_max_hz", 0.0, 20000.0, maxHz)
        || maxHz <= minHz
        || !number(root, "global_gain_db", gainMin, gainMax, globalDb)) { return false; }
    parametric = flagValue.toBool();
    f.reserve(count); g.reserve(count); q.reserve(count);
    double previous = -1.0;
    for (const QJsonValue& value : points) {
        if (!value.isObject()) { return false; }
        const QJsonObject point = value.toObject();
        double hz = 0.0, gain = 0.0, factor = 0.0;
        if (!number(point, "frequency_hz", minHz, maxHz, hz)
            || !number(point, "gain_db", gainMin, gainMax, gain)
            || !number(point, "q", 0.2, 20.0, factor)
            || hz <= previous) { return false; }
        previous = hz;
        f.push_back(hz); g.push_back(gain); q.push_back(factor);
    }
    return f.front() == minHz && f.back() == maxHz;
}

QString json(const Profile& p, bool compression)
{
    const std::vector<double>& frequencies = compression ? p.f : p.postF;
    const std::vector<double>& gains = compression ? p.g : p.e;
    const std::vector<double>& factors = compression ? p.qg : p.qe;
    QJsonArray points;
    for (std::size_t i = 0; i < frequencies.size(); ++i) {
        points.append(QJsonObject{{QStringLiteral("frequency_hz"), frequencies[i]},
                                  {QStringLiteral("gain_db"), gains[i]},
                                  {QStringLiteral("q"), factors[i]}});
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("band_count"), static_cast<int>(p.f.size())},
        {QStringLiteral("parametric_eq"), compression ? p.compParametric : p.eqParametric},
        {QStringLiteral("global_gain_db"), compression ? p.precompDb : p.postEqGainDb},
        {QStringLiteral("frequency_min_hz"), compression ? p.minHz : p.postMinHz},
        {QStringLiteral("frequency_max_hz"), compression ? p.maxHz : p.postMaxHz},
        {QStringLiteral("points"), points}}).toJson(QJsonDocument::Indented));
}
} // namespace

// From Thetis frmCFCConfig.cs:492-557 [v2.10.3.15], with bounds before
// the Core accepts a new remote write. Runtime uses both parametric flags
// (frmCFCConfig.cs:378), so mismatched flags remain valid and disable Q.
bool decode(const QString& blob, Profile& out)
{
    if (blob.isEmpty() || blob.size() > kMaxEncodedChars) { return false; }
    const std::optional<QString> decoded = ParaEqEnvelope::decode(blob, kMaxDecodedBytes);
    if (!decoded) { return false; }
    const qsizetype separator = decoded->indexOf(QStringLiteral("<SEP>"));
    if (separator < 0 || decoded->indexOf(QStringLiteral("<SEP>"), separator + 5) >= 0) {
        return false;
    }
    Profile candidate;
    if (!curve(decoded->left(separator), 0.0, 16.0,
               candidate.f, candidate.g, candidate.qg,
               candidate.precompDb, candidate.minHz, candidate.maxHz,
               candidate.compParametric)
        || !curve(decoded->mid(separator + 5), -24.0, 24.0,
                  candidate.postF, candidate.e, candidate.qe,
                  candidate.postEqGainDb, candidate.postMinHz, candidate.postMaxHz,
                  candidate.eqParametric)
        || candidate.postF.size() != candidate.f.size()) { return false; }
    out = std::move(candidate);
    return true;
}

// From Thetis frmCFCConfig.cs:492-504 [v2.10.3.15].
QString encode(const Profile& p)
{
    const std::size_t count = p.f.size();
    if ((count != 5 && count != 10 && count != 18)
        || p.postF.size() != count || p.g.size() != count || p.e.size() != count
        || p.qg.size() != count || p.qe.size() != count) { return {}; }
    const QString blob = ParaEqEnvelope::encode(json(p, true) + QStringLiteral("<SEP>")
                                                  + json(p, false));
    if (blob.size() > kMaxEncodedChars) { return {}; }
    Profile check;
    return decode(blob, check) ? blob : QString();
}

namespace {
// .NET Math.Round(value, digits), as ucParametricEq's PointsFromJson rounds
// a saved curve (ucParametricEq.cs:1434-1452 [v2.10.3.15]).
double roundDigits(double value, int digits)
{
    const double power10 = std::pow(10.0, digits);
    return std::nearbyint(value * power10) / power10;
}

QJsonObject publishedObject(const Profile& p)
{
    QJsonArray bands;
    for (std::size_t i = 0; i < p.f.size(); ++i) {
        bands.append(QJsonObject{{QStringLiteral("frequencyHz"), p.f[i]},
                                 {QStringLiteral("compressionDb"), p.g[i]},
                                 {QStringLiteral("compressionQ"), p.qg[i]},
                                 {QStringLiteral("postEqGainDb"), p.e[i]},
                                 {QStringLiteral("postEqQ"), p.qe[i]}});
    }
    return QJsonObject{{QStringLiteral("bands"), bands},
                       {QStringLiteral("minHz"), p.minHz},
                       {QStringLiteral("maxHz"), p.maxHz},
                       {QStringLiteral("parametric"), p.usesQ()},
                       {QStringLiteral("precompDb"), p.precompDb},
                       {QStringLiteral("postEqGainDb"), p.postEqGainDb}};
}
} // namespace

// NereusSDR-original (transmitSettingsVersion 15).
QString revision(const Profile& p)
{
    const QByteArray canonical = QJsonDocument(publishedObject(p)).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex().left(16));
}

// NereusSDR-original (transmitSettingsVersion 15).
QString publishedJson(const Profile& p, const QString& state)
{
    QJsonObject root = publishedObject(p);
    root.insert(QStringLiteral("revision"), revision(p));
    root.insert(QStringLiteral("state"), state);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// NereusSDR-original (R-R3-49, transmitSettingsVersion 15): the band
// editor an app sends, checked against the CFC dialog's choices (the
// limits in CfcProfile.h, each from frmCFCConfig [v2.10.3.15]) and refused
// whole otherwise.
bool fromPublishedJson(const QString& text, Profile& out, QString* refusal)
{
    const auto refuse = [refusal](const QString& why) {
        if (refusal) { *refusal = why; }
        return false;
    };
    const QString notUnderstood = QStringLiteral("The CFC settings were not understood.");
    const auto finite = [](double v) { return std::isfinite(v); };

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        return refuse(notUnderstood);
    }
    const QJsonObject o = doc.object();
    const QJsonValue bandsValue = o.value(QStringLiteral("bands"));
    const QJsonValue minValue = o.value(QStringLiteral("minHz"));
    const QJsonValue maxValue = o.value(QStringLiteral("maxHz"));
    const QJsonValue parametric = o.value(QStringLiteral("parametric"));
    const QJsonValue precomp = o.value(QStringLiteral("precompDb"));
    const QJsonValue postGain = o.value(QStringLiteral("postEqGainDb"));
    if (!bandsValue.isArray() || !minValue.isDouble() || !maxValue.isDouble()
        || !parametric.isBool() || !precomp.isDouble() || !postGain.isDouble()) {
        return refuse(notUnderstood);
    }
    const QJsonArray bands = bandsValue.toArray();
    struct Row { double f, g, qg, e, qe; };
    std::vector<Row> rows;
    for (const QJsonValue& value : bands) {
        if (!value.isObject()) { return refuse(notUnderstood); }
        const QJsonObject b = value.toObject();
        const QJsonValue f = b.value(QStringLiteral("frequencyHz"));
        const QJsonValue g = b.value(QStringLiteral("compressionDb"));
        const QJsonValue qg = b.value(QStringLiteral("compressionQ"));
        const QJsonValue e = b.value(QStringLiteral("postEqGainDb"));
        const QJsonValue qe = b.value(QStringLiteral("postEqQ"));
        if (!f.isDouble() || !g.isDouble() || !qg.isDouble() || !e.isDouble() || !qe.isDouble()) {
            return refuse(notUnderstood);
        }
        rows.push_back({f.toDouble(), g.toDouble(), qg.toDouble(), e.toDouble(), qe.toDouble()});
    }

    const int count = static_cast<int>(rows.size());
    if (std::find(std::begin(kBandCounts), std::end(kBandCounts), count) == std::end(kBandCounts)) {
        return refuse(QStringLiteral("Choose 5, 10 or 18 bands."));
    }
    const double minHz = minValue.toDouble();
    const double maxHz = maxValue.toDouble();
    const double minRounded = finite(minHz) ? roundDigits(minHz, 3) : 0.0;
    const double maxRounded = finite(maxHz) ? roundDigits(maxHz, 3) : 0.0;
    if (!finite(minHz) || !finite(maxHz) || minHz < kFrequencyMinHz || maxHz > kFrequencyMaxHz
        || maxRounded - minRounded < kMinRangeSpreadHz) {
        return refuse(QStringLiteral("Choose a low and a high end from 0 to 20000 Hz, the "
                                     "high end at least 1000 Hz above the low end."));
    }
    const double precompDb = precomp.toDouble();
    if (!finite(precompDb) || precompDb < kCompressionMinDb || precompDb > kCompressionMaxDb) {
        return refuse(QStringLiteral("Choose a pre-compression from 0 to 16 dB."));
    }
    const double postEqGainDb = postGain.toDouble();
    if (!finite(postEqGainDb) || postEqGainDb < kPostEqGainMinDb || postEqGainDb > kPostEqGainMaxDb) {
        return refuse(QStringLiteral("Choose a post-EQ gain from -24 to 24 dB."));
    }

    Profile r;
    r.minHz = r.postMinHz = minRounded;
    r.maxHz = r.postMaxHz = maxRounded;
    r.precompDb = roundDigits(precompDb, 1);
    r.postEqGainDb = roundDigits(postEqGainDb, 1);
    r.compParametric = r.eqParametric = parametric.toBool();
    double previous = minRounded;
    for (int i = 0; i < count; ++i) {
        const Row& row = rows[static_cast<std::size_t>(i)];
        if (!finite(row.f) || row.f < kFrequencyMinHz || row.f > kFrequencyMaxHz) {
            return refuse(QStringLiteral("Choose each band's frequency between the low and "
                                         "high ends."));
        }
        if (!finite(row.g) || row.g < kCompressionMinDb || row.g > kCompressionMaxDb) {
            return refuse(QStringLiteral("Choose each band's compression from 0 to 16 dB."));
        }
        if (!finite(row.e) || row.e < kPostEqGainMinDb || row.e > kPostEqGainMaxDb) {
            return refuse(QStringLiteral("Choose each band's post-EQ gain from -24 to 24 dB."));
        }
        if (!finite(row.qg) || !finite(row.qe) || row.qg < kQMin || row.qg > kQMax
            || row.qe < kQMin || row.qe > kQMax) {
            return refuse(QStringLiteral("Choose each Q from 0.2 to 20."));
        }
        // The first band sits at the low end and the last at the high end,
        // as the dialog keeps them; each band between lies inside the
        // range, above the one before it.
        double hz = roundDigits(row.f, 3);
        if (i == 0) {
            hz = minRounded;
        } else if (i == count - 1) {
            hz = maxRounded;
            if (hz <= previous) {
                return refuse(QStringLiteral("Keep each band's frequency above the one before it."));
            }
        } else {
            if (hz <= minRounded || hz >= maxRounded) {
                return refuse(QStringLiteral("Choose each band's frequency between the low and "
                                             "high ends."));
            }
            if (hz <= previous) {
                return refuse(QStringLiteral("Keep each band's frequency above the one before it."));
            }
        }
        previous = hz;
        r.f.push_back(hz);
        r.postF.push_back(hz);
        r.g.push_back(roundDigits(row.g, 1));
        r.e.push_back(roundDigits(row.e, 1));
        r.qg.push_back(roundDigits(row.qg, 2));
        r.qe.push_back(roundDigits(row.qe, 2));
    }
    out = std::move(r);
    return true;
}

// NereusSDR-original: what TxCfcDialog::seedWidgetsFromTransmitModel shows
// for the ten-band values (its 0..4000 Hz default range from
// frmCFCConfig.cs:89-99 [v2.10.3.15], widened to cover every band).
Profile legacyProfile(const std::array<int, 10>& frequencyHz,
                      const std::array<int, 10>& compressionDb,
                      const std::array<int, 10>& postEqGainDb,
                      int precompDb, int postEqGainDbGlobal)
{
    Profile p;
    double lowest = kFrequencyMaxHz;
    double highest = 0.0;
    for (int hz : frequencyHz) {
        lowest = std::min(lowest, static_cast<double>(hz));
        highest = std::max(highest, static_cast<double>(hz));
    }
    p.minHz = p.postMinHz = std::min(0.0, lowest);
    p.maxHz = p.postMaxHz = std::max(4000.0, highest);
    p.precompDb = precompDb;
    p.postEqGainDb = postEqGainDbGlobal;
    for (std::size_t i = 0; i < frequencyHz.size(); ++i) {
        double hz = frequencyHz[i];
        if (i == 0) { hz = p.minHz; }
        if (i + 1 == frequencyHz.size()) { hz = p.maxHz; }
        p.f.push_back(hz);
        p.postF.push_back(hz);
        p.g.push_back(compressionDb[i]);
        p.e.push_back(postEqGainDb[i]);
        p.qg.push_back(4.0);
        p.qe.push_back(4.0);
    }
    return p;
}

} // namespace NereusSDR::CfcProfile
