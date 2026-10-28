// =================================================================
// src/gui/meters/OverlayTokens.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (clsTextOverlay parseText,
//   GetPlaceholders, formatNumber, the custom text readings and
//   clsMMIO.VariableValueType), original licence from Thetis source is
//   included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Text overlay token table: Thetis parseText's precision,
//                 reading, time and date, newline and meter data source
//                 tokens, case-insensitive, with slice-letter tokens in
//                 place of the VFO and receiver ones, and a translator
//                 for Thetis overlay text. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

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

#include "OverlayTokens.h"

#include "ClockItem.h"
#include "MeterPoller.h"
#include "ThetisReadingNames.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"

#include <QMetaType>
#include <QVariant>

#include <algorithm>
#include <optional>

namespace NereusSDR {

namespace {

// From Thetis MeterManager.cs:20594 [v2.10.3.15] (parseText):
//   string precision_format = "0.0#####";//"f6";
// [original inline comment from MeterManager.cs:20594]
// At least one decimal, at most six, trailing zeros dropped.
constexpr int kDefaultMaxDecimals = 6;
constexpr int kDefaultMinDecimals = 1;
// From Thetis MeterManager.cs:20639-20640 [v2.10.3.15]: precis is kept to 0..20.
constexpr int kMinPrecision = 0;
constexpr int kMaxPrecision = 20;
// NereusSDR: the slices the editor offers tokens for, Slice A through the
// most any board opens (VfoDisplayItem::kMaxSliceCount).
constexpr int kOfferedSliceCount = VfoDisplayItem::kMaxSliceCount;

const QString kPrecisPrefix = QStringLiteral("precis=");

QString formatDefaultDecimals(double v)
{
    QString s = QString::number(v, 'f', kDefaultMaxDecimals);
    const qsizetype dot = s.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        return s;
    }
    qsizetype end = s.size();
    while (end > dot + 1 + kDefaultMinDecimals && s.at(end - 1) == QLatin1Char('0')) {
        --end;
    }
    s.truncate(end);
    return s;
}

// A float or double reading as parseText writes it: "f<precis>" when the
// line has %precis=N%, else "0.0#####".
// From Thetis MeterManager.cs:20655-20664, 20679 [v2.10.3.15]
QString formatReading(double v, int precision)
{
    if (precision >= 0) {
        return QString::number(v, 'f', precision);
    }
    return formatDefaultDecimals(v);
}

// From Thetis MeterManager.cs:839-848 [v2.10.3.15] (formatNumber): MHz to
// six places with a point after the kHz, 14.074.000.
QString formatNumber(double number)
{
    QString numberString = QString::number(number, 'f', 6);
    const qsizetype decimalPointIndex = numberString.indexOf(QLatin1Char('.'));
    if (decimalPointIndex != -1 && numberString.size() > decimalPointIndex + 3) {
        numberString.insert(decimalPointIndex + 4, QLatin1Char('.'));
    }
    return numberString;
}

// A meter data source value as clsMMIO.VariableValueType writes it.
// From Thetis MeterManager.cs:41795-41810 [v2.10.3.15]
QString variableValueType(const QVariant& obj, int precision)
{
    switch (obj.typeId()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return obj.toString();
        case QMetaType::Float:
        case QMetaType::Double:
            return formatReading(obj.toDouble(), precision);
        case QMetaType::Bool:
            return obj.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        default:
            return obj.toString();
    }
}

// Hour*10000 + Minute*100 + Second.
// From Thetis MeterManager.cs:1038-1043 [v2.10.3.15]
int timeInt(const QDateTime& now)
{
    const QTime t = now.time();
    return t.hour() * 10000 + t.minute() * 100 + t.second();
}

// NereusSDR: yyyyMMdd. Thetis's date_utc_int and date_loc_int produce the
// time integer above (MeterManager.cs:1044-1049 [v2.10.3.15]), the same
// value as time_utc_int and time_loc_int; the token names a date, so
// NereusSDR gives the date.
int dateInt(const QDateTime& now)
{
    const QDate d = now.date();
    return d.year() * 10000 + d.month() * 100 + d.day();
}

const VfoSliceState* sliceFor(const OverlayContext& context, int index)
{
    for (const VfoSliceState& s : context.slices) {
        if (s.index == index && s.open) {
            return &s;
        }
    }
    return nullptr;
}

// %slice<letter>_<field>%. A slice that is not open shows nothing, as
// Thetis's VFO tokens do for a VFO the meter's receiver does not have
// (MeterManager.cs:1050-1061 [v2.10.3.15]).
std::optional<QString> sliceToken(const QString& lower, const OverlayContext& context)
{
    static const QString kPrefix = QStringLiteral("slice");
    if (!lower.startsWith(kPrefix) || lower.size() < kPrefix.size() + 3
        || lower.at(kPrefix.size() + 1) != QLatin1Char('_')) {
        return std::nullopt;
    }
    const QChar letter = lower.at(kPrefix.size());
    if (letter < QLatin1Char('a') || letter > QLatin1Char('z')) {
        return std::nullopt;
    }
    const QString field = lower.mid(kPrefix.size() + 2);
    static const QStringList kFields{QStringLiteral("freq"), QStringLiteral("mode"),
                                     QStringLiteral("filter"), QStringLiteral("band"),
                                     QStringLiteral("step"), QStringLiteral("open")};
    if (!kFields.contains(field)) {
        return std::nullopt;
    }
    const int index = letter.unicode() - u'a';
    const VfoSliceState* s = sliceFor(context, index);
    if (!s) {
        return QString();
    }
    if (field == QLatin1String("freq")) {
        // From Thetis MeterManager.cs:1050-1055 [v2.10.3.15] (vfoa)
        return formatNumber(static_cast<double>(s->frequencyHz) / 1.0e6);
    }
    if (field == QLatin1String("mode")) {
        // From Thetis MeterManager.cs:1095-1097 [v2.10.3.15] (mode_vfoa)
        return s->mode;
    }
    if (field == QLatin1String("filter")) {
        // From Thetis MeterManager.cs:1104-1106 [v2.10.3.15] (filter_vfoa)
        return s->filter;
    }
    if (field == QLatin1String("band")) {
        // From Thetis MeterManager.cs:1086-1088 [v2.10.3.15] (band_vfoa)
        return s->band;
    }
    if (field == QLatin1String("step")) {
        return QString::number(s->stepHz);
    }
    // From Thetis MeterManager.cs:1161-1163 [v2.10.3.15] (rx2): "RX2 On"
    // while the second receiver is on; here the slice by its letter.
    return QStringLiteral("Slice %1 on").arg(QChar(letter.toUpper()));
}

// The value of one token, or nullopt when `name` is not a token.
std::optional<QString> tokenValue(const QString& name, const OverlayContext& context, int precision)
{
    const QString key = name.toLower();

    // From Thetis MeterManager.cs:20684-20686 [v2.10.3.15]
    // replace newline
    if (key == QLatin1String("nl")) {
        return QStringLiteral("\n");
    }
    // From Thetis MeterManager.cs:20620-20628 [v2.10.3.15]: every precis
    // token is removed; the first valid one sets the precision.
    if (key.startsWith(kPrecisPrefix)) {
        return QString();
    }

    // additional string based custom readings such as time_utc etc, we can just update these
    // as they are obtained
    // From Thetis MeterManager.cs:1019-1049 [v2.10.3.15]
    if (key == QLatin1String("time_utc")) {
        return ClockItem::thetisTime(context.nowUtc, true);
    }
    if (key == QLatin1String("time_loc")) {
        return ClockItem::thetisTime(context.nowLocal, true);
    }
    if (key == QLatin1String("date_utc")) {
        return ClockItem::thetisDate(context.nowUtc);
    }
    if (key == QLatin1String("date_loc")) {
        return ClockItem::thetisDate(context.nowLocal);
    }
    if (key == QLatin1String("time_utc_int")) {
        return QString::number(timeInt(context.nowUtc));
    }
    if (key == QLatin1String("time_loc_int")) {
        return QString::number(timeInt(context.nowLocal));
    }
    if (key == QLatin1String("date_utc_int")) {
        // NereusSDR differs from MeterManager.cs:1044-1046 [v2.10.3.15],
        // which gives the time integer: see dateInt().
        return QString::number(dateInt(context.nowUtc));
    }
    if (key == QLatin1String("date_loc_int")) {
        // NereusSDR differs from MeterManager.cs:1047-1049 [v2.10.3.15],
        // which gives the time integer: see dateInt().
        return QString::number(dateInt(context.nowLocal));
    }
    // From Thetis MeterManager.cs:1149-1151 [v2.10.3.15]
    if (key == QLatin1String("mox")) {
        return context.mox ? QStringLiteral("MOX") : QString();
    }
    // NereusSDR: the transmit slice by its letter.
    if (key == QLatin1String("tx_slice")) {
        return context.txSlice.isNull() ? QString() : QString(context.txSlice);
    }
    if (std::optional<QString> slice = sliceToken(key, context)) {
        return slice;
    }

    // NereusSDR: the item's own values.
    const auto named = context.named.constFind(key);
    if (named != context.named.cend()) {
        return named.value();
    }

    // replace readings
    // From Thetis MeterManager.cs:20672-20681 [v2.10.3.15]: a reading the
    // meter has no value for reads 0 (GetReading, :1006-1015).
    const int binding = bindingForThetisReadingName(key);
    if (binding >= 0) {
        return formatReading(context.readings.value(binding, 0.0), precision);
    }

    // replace MultiMeter IO
    // From Thetis MeterManager.cs:20688-20704 [v2.10.3.15]: only a value
    // the source holds is replaced.
    const qsizetype dot = key.indexOf(QLatin1Char('.'));
    if (dot > 0 && dot + 1 < key.size()) {
        if (MmioEndpoint* ep = ExternalVariableEngine::instance().endpointByFourChar(name.left(dot))) {
            const QVariant value = ep->valueForName(name.mid(dot + 1));
            if (value.isValid()) {
                return variableValueType(value, precision);
            }
        }
    }
    return std::nullopt;
}

// Walks `text` for %name% pairs as Thetis GetPlaceholders does
// (MeterManager.cs:899-921 [v2.10.3.15]), calling valueFor on each name.
// NereusSDR: a name that is not a token gives back its first % as text
// and the walk goes on from the next character, so in "%1 and %swr%" the
// %swr% still resolves (Thetis pairs the two percent signs around
// "1 and " and loses it). One pass: a value is never read again as tokens,
// where Thetis's repeated replaces would.
template<typename ValueFor>
QString walkTokens(const QString& text, ValueFor&& valueFor)
{
    QString out;
    out.reserve(text.size());
    const qsizetype length = text.size();
    qsizetype i = 0;
    while (i < length) {
        const QChar c = text.at(i);
        if (c != QLatin1Char('%')) {
            out += c;
            ++i;
            continue;
        }
        // Ensure there's at least one character after the current %
        // [original inline comment from MeterManager.cs:907]
        const qsizetype end = text.indexOf(QLatin1Char('%'), i + 1);
        if (end < 0) {
            out += QStringView(text).mid(i);
            break;
        }
        std::optional<QString> value;
        if (end - i > 1) // Ensure there's something between the %
        {
            value = valueFor(text.mid(i + 1, end - i - 1));
        }
        if (value) {
            out += *value;
            i = end + 1;
        } else {
            out += c;
            ++i;
        }
    }
    return out;
}

} // namespace

int overlayTokenPrecision(const QString& text)
{
    // check for precision
    // From Thetis MeterManager.cs:20620-20649 [v2.10.3.15]
    int precision = -1;
    walkTokens(text, [&](const QString& name) -> std::optional<QString> {
        if (!name.startsWith(kPrecisPrefix, Qt::CaseInsensitive)) {
            // Other names are walked past only when they are tokens; the
            // values do not matter here.
            static const OverlayContext kEmpty;
            return tokenValue(name, kEmpty, -1);
        }
        if (precision < 0) {
            bool ok = false;
            const int precis = name.mid(kPrecisPrefix.size()).toInt(&ok);
            if (ok) {
                precision = std::clamp(precis, kMinPrecision, kMaxPrecision);
            }
        }
        return QString();
    });
    return precision;
}

QString resolveOverlayTokens(const QString& text, const OverlayContext& context)
{
    if (text.isEmpty()) {
        return {};
    }
    // From Thetis MeterManager.cs:20587-20716 [v2.10.3.15] (parseText).
    // Thetis's passes, in one walk here:
    // check for precision
    // [original inline comment from MeterManager.cs:20620]
    const int precision = overlayTokenPrecision(text);
    // replace non readings
    // [original inline comment from MeterManager.cs:20651]
    // replace readings
    // [original inline comment from MeterManager.cs:20672]
    // replace newline
    // [original inline comment from MeterManager.cs:20684]
    // replace MultiMeter IO
    // [original inline comment from MeterManager.cs:20688]
    // Not carried (out of scope): //replace cat variables
    // [original inline comment from MeterManager.cs:20706]
    return walkTokens(text, [&](const QString& name) { return tokenValue(name, context, precision); });
}

QString translateThetisOverlayText(const QString& text)
{
    // Thetis's VFO and receiver tokens (MeterManager.cs:796-811, 823
    // [v2.10.3.15]) by slice letter: VFO A is Slice A, VFO B and RX2 are
    // Slice B. Thetis forms with no slice token here (vfoasub, bandtext,
    // bw, split, ...) stay as written.
    static const QHash<QString, QString> kMap{
        {QStringLiteral("vfoa"), QStringLiteral("sliceA_freq")},
        {QStringLiteral("vfob"), QStringLiteral("sliceB_freq")},
        {QStringLiteral("vfoa_double"), QStringLiteral("sliceA_freq")},
        {QStringLiteral("vfob_double"), QStringLiteral("sliceB_freq")},
        {QStringLiteral("band_vfoa"), QStringLiteral("sliceA_band")},
        {QStringLiteral("band_vfob"), QStringLiteral("sliceB_band")},
        {QStringLiteral("mode_vfoa"), QStringLiteral("sliceA_mode")},
        {QStringLiteral("mode_vfob"), QStringLiteral("sliceB_mode")},
        {QStringLiteral("filter_vfoa"), QStringLiteral("sliceA_filter")},
        {QStringLiteral("filter_vfob"), QStringLiteral("sliceB_filter")},
        {QStringLiteral("filter_vfoa_name"), QStringLiteral("sliceA_filter")},
        {QStringLiteral("filter_vfob_name"), QStringLiteral("sliceB_filter")},
        {QStringLiteral("rx2"), QStringLiteral("sliceB_open")},
    };
    return walkTokens(text, [](const QString& name) -> std::optional<QString> {
        const auto it = kMap.constFind(name.toLower());
        if (it == kMap.cend()) {
            return std::nullopt;
        }
        return QLatin1Char('%') + it.value() + QLatin1Char('%');
    });
}

QStringList offeredOverlayTokens()
{
    QStringList tokens{
        QStringLiteral("%nl%"),
        QStringLiteral("%precis=1%"),
        QStringLiteral("%value%"),
        QStringLiteral("%group%"),
        // From Thetis MeterManager.cs:788-795 [v2.10.3.15]
        QStringLiteral("%time_utc%"),
        QStringLiteral("%time_utc_int%"),
        QStringLiteral("%time_loc%"),
        QStringLiteral("%time_loc_int%"),
        QStringLiteral("%date_utc%"),
        QStringLiteral("%date_utc_int%"),
        QStringLiteral("%date_loc%"),
        QStringLiteral("%date_loc_int%"),
        // From Thetis MeterManager.cs:819 [v2.10.3.15]
        QStringLiteral("%mox%"),
        QStringLiteral("%tx_slice%"),
    };
    static const char* const kFields[] = {"freq", "mode", "filter", "band", "step", "open"};
    for (int i = 0; i < kOfferedSliceCount; ++i) {
        const QChar letter(u'A' + i);
        for (const char* field : kFields) {
            tokens << QStringLiteral("%slice") + letter + QLatin1Char('_')
                    + QLatin1String(field) + QLatin1Char('%');
        }
    }
    // The readings by name, in MeterBinding order.
    for (int binding = 0; binding <= MeterBinding::RotatorEle; ++binding) {
        const QString name = thetisReadingName(binding);
        if (!name.isEmpty()) {
            tokens << QLatin1Char('%') + name + QLatin1Char('%');
        }
    }
    return tokens;
}

} // namespace NereusSDR
