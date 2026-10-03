// no-port-check: NereusSDR-original test helper.
//
// OperatorWording: the shared check that words a user reads are written for
// the user (R-R3-21, R-R3-37; operator directive of 2026-09-23). A string
// passes when it is not empty and names none of the internal terms. Log
// lines and source comments may use them; nothing a user reads may.
//
// The term list itself belongs to the product (OperatorReasonText), which
// uses it to decide whether a reason it does not know may be shown as sent.
// One list, so a test and the app can never disagree about a word.
//
// coreCalledStationIn() is the tests' one rule for "Core" and "station" in
// user text (R-R3-21): both wording tests use it.
#pragma once

#include "gui/OperatorReasonText.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace NereusSDR::OperatorWording {

/// Internal subsystem, roadmap and wire terms (the product's list).
inline const QStringList& internalTerms()
{
    return OperatorReasonText::internalTerms();
}

/// The first internal term `text` names, or an empty string.
inline QString internalTermIn(const QString& text)
{
    return OperatorReasonText::internalTermIn(text);
}

/// True when `text` is written for the user: not empty, no internal term.
inline bool isPlain(const QString& text)
{
    return !text.trimmed().isEmpty() && internalTermIn(text).isEmpty();
}

/// "Core" names the NereusSDR computer a user connects to; "station" is only
/// the operator's radio station in its ham sense (R-R3-21, operator decision
/// of 2026-09-24). The ham-sense phrases a user may read, and the
/// command-line option names (--station, --station-fingerprint, ...), are
/// set aside; any other "station" in `text` is returned, or an empty string.
inline QString coreCalledStationIn(const QString& text)
{
    static const QRegularExpression hamSense(
        QStringLiteral("--station[-A-Za-z]*"
                       "|\\bstation's (amplifier|tuner|transmitter|Power Genius|Tuner Genius)\\b"
                       "|\\bthe Core's station\\b|\\b(your|my) station\\b|\\bstation callsign\\b"
                       "|\\bQSY to this station\\b|\\blive stations on qso\\.freedv\\.org\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression station(QStringLiteral("\\bstations?\\b"),
                                            QRegularExpression::CaseInsensitiveOption);
    QString rest = text;
    rest.replace(hamSense, QStringLiteral(" "));
    return station.match(rest).captured(0);
}

} // namespace NereusSDR::OperatorWording
