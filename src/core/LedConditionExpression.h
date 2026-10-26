#pragma once

// =================================================================
// src/core/LedConditionExpression.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Independently implemented from the Thetis LED
// condition behaviour: Thetis compiles each LED condition as a C# script
// and expands %name% placeholders into a per-receiver variable table.
// NereusSDR does not run C#. It evaluates a small, defined expression
// language instead, and anything outside it is an error with a plain
// reason, never an exception, a crash or a hang.
//
// The language:
//   numbers           12, -73, 0.5, 1e3
//   text              "double quoted", with \" \\ \n \t escapes
//   names             %signal_strength%, %time_utc%, %ABCD.temperature%
//   arithmetic        + - * /   (+ also joins two texts)
//   comparisons       < <= > >=  (numbers)   == !=  (two of one kind)
//   logic             && || !    (true or false parts)
//   brackets          ( )
// The whole condition must work out to true or false. Both sides of
// && and || are always worked out, so a mistake on either side shows.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Created for the LED condition by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QVariant>

#include <functional>
#include <optional>

namespace NereusSDR {

struct LedConditionResult {
    bool ok{false};      // the condition worked out to true or false
    bool value{false};   // that result; false whenever ok is false
    QString error;       // a plain reason when ok is false, else empty
};

// Looks up the name inside %...% (without the % signs).
//   std::nullopt     the name is not known: an error naming it
//   invalid QVariant the name is known but has no value now: an error
//   bool             true or false
//   QString          text
//   any number type  a number
using LedConditionLookup = std::function<std::optional<QVariant>(const QString& name)>;

// Brackets and ! nest at most this deep.
inline constexpr int kLedConditionMaxDepth = 64;

LedConditionResult evaluateLedCondition(const QString& expression, const LedConditionLookup& lookup);

} // namespace NereusSDR
