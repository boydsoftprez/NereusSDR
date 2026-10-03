// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/StationLabel.cpp  (NereusSDR)
// =================================================================
// See StationLabel.h for the rules (pairing design section 3.3).
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 13 (R-IOS-08): current() and ruleText().
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "core/security/StationLabel.h"

#include "core/AppSettings.h"

namespace NereusSDR {

namespace {

bool isAsciiAlnum(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9');
}

bool validCallsign(const QString& callsign)
{
    if (callsign.isEmpty() || callsign.size() > StationLabel::kMaxCallsignLength
        || callsign.startsWith(QLatin1Char('/')) || callsign.endsWith(QLatin1Char('/'))) {
        return false;
    }
    for (const QChar c : callsign) {
        if (!isAsciiAlnum(c) && c != QLatin1Char('/')) {
            return false;
        }
    }
    return true;
}

bool validSuffix(const QString& suffix)
{
    if (suffix.size() > StationLabel::kMaxSuffixLength) {
        return false;
    }
    for (const QChar c : suffix) {
        if (!isAsciiAlnum(c) && c != QLatin1Char('_') && c != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

} // namespace

QString StationLabel::display() const
{
    return suffix.isEmpty() ? callsign : callsign + QLatin1Char('/') + suffix;
}

std::optional<StationLabel> StationLabel::parse(const QString& text)
{
    const QString trimmed = text.trimmed();
    StationLabel label;
    const int slash = trimmed.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        label.callsign = trimmed;
    } else {
        label.callsign = trimmed.left(slash);
        label.suffix = trimmed.mid(slash + 1);
    }
    if (!validCallsign(label.callsign) || !validSuffix(label.suffix)) {
        return std::nullopt;
    }
    return label;
}

bool StationLabel::sameLabel(const QString& a, const QString& b)
{
    const std::optional<StationLabel> left = parse(a);
    const std::optional<StationLabel> right = parse(b);
    if (!left || !right) {
        return false;
    }
    return left->display().compare(right->display(), Qt::CaseInsensitive) == 0;
}

std::optional<StationLabel> StationLabel::defaultLabel(const AppSettings& settings)
{
    const QString callsign =
        settings.value(QStringLiteral("StationCallsign"), QString()).toString().trimmed();
    if (!validCallsign(callsign)) {
        return std::nullopt;
    }
    StationLabel label;
    label.callsign = callsign;
    return label;
}

std::optional<StationLabel> StationLabel::current(const AppSettings& settings)
{
    const QString stored =
        settings.value(QLatin1String(kSettingsKey), QString()).toString().trimmed();
    if (!stored.isEmpty()) {
        if (const std::optional<StationLabel> renamed = parse(stored)) {
            return renamed;
        }
    }
    return defaultLabel(settings);
}

QString StationLabel::ruleText()
{
    return QStringLiteral(
        "Name the Core with a callsign of letters, digits and /, then if you like a / and up "
        "to 32 letters, digits, dashes or underscores, for example KG4VCF/shack.");
}

} // namespace NereusSDR
