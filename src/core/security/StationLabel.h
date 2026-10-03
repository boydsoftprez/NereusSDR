#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/StationLabel.h  (NereusSDR)
// =================================================================
//
// The Core's label, `<callsign>/<suffix>` (iPhone app plan Task 12,
// R-IOS-08; the pairing design, docs/architecture/2026-08-02-remote-
// station-identity-and-pairing-design.md section 3.3):
//
//   - the callsign defaults to the StationCallsign setting, so nothing is
//     asked of the operator;
//   - the suffix is at most 32 characters of [A-Za-z0-9_-];
//   - comparison is case-insensitive; display keeps what was typed;
//   - an empty suffix is legal and displays as the bare callsign.
//
// The label is not an identifier: nothing matches on it but a person
// reading it. A callsign may hold '/' itself (a portable call, KG4VCF/P),
// so the suffix is what follows the LAST '/'.
//
// iPhone app Task 13: a rename stores the label under the Core-owned
// setting kSettingsKey ("StationLabel"), which no window writes directly
// (the link document's section 8.2). Until the first rename the setting is
// empty and the label follows StationCallsign (current()).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 13 (R-IOS-08): the Core-owned StationLabel
//               setting, current() and the rule in plain words. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QString>

#include <optional>

namespace NereusSDR {

class AppSettings;

struct StationLabel {
    static constexpr int kMaxSuffixLength = 32;
    static constexpr int kMaxCallsignLength = 32;
    /// iPhone app Task 13: the Core-owned setting a rename writes.
    static constexpr const char* kSettingsKey = "StationLabel";

    /// As typed (trimmed).
    QString callsign;
    QString suffix;

    /// `callsign/suffix`, or the bare callsign when the suffix is empty.
    QString display() const;

    /// nullopt when `text` is not a label: an empty callsign, a callsign of
    /// other than letters, digits and '/', either part too long, or a
    /// suffix with a character outside [A-Za-z0-9_-]. Text with no '/' is a
    /// bare callsign.
    static std::optional<StationLabel> parse(const QString& text);

    /// Case-insensitive equality of two labels as displayed. False when
    /// either is not a label.
    static bool sameLabel(const QString& a, const QString& b);

    /// The label a Core starts with: its StationCallsign setting, with no
    /// suffix. nullopt when the setting is empty or is not a callsign.
    static std::optional<StationLabel> defaultLabel(const AppSettings& settings);

    /// iPhone app Task 13: the Core's label now: the renamed one stored
    /// under kSettingsKey when it is set and is a label, else
    /// defaultLabel(). nullopt when neither gives one.
    static std::optional<StationLabel> current(const AppSettings& settings);

    /// The label rule in the operator's words, for a refused rename.
    static QString ruleText();
};

} // namespace NereusSDR
