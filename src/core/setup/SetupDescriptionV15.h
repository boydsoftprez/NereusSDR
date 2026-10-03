#pragma once
// no-port-check: NereusSDR-original Setup description version 15 rules.
// =================================================================
// src/core/setup/SetupDescriptionV15.h  (NereusSDR)
// =================================================================
//
// Setup description version 15 (R-IOS-18, R-IOS-27, R-R3-49): the rows
// that describe the rest of Setup > DSP, Transmit, Audio, Diagnostics and
// CAT & Network for a remote window or the phone. Each row names a value
// the Core already sends (a mirrored property, a Station setting, radio
// telemetry) or a verb the Core already runs, so the Core validates the
// row against those sources instead of against a copy of itself.
//
// Modification history (NereusSDR):
//   2026-09-29 - Created. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
// =================================================================

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace NereusSDR::SetupDescriptionV15 {

/// The description version these rows need.
constexpr int kVersion = 15;

/// True for the categories that carry version 15 rows.
bool isCategory(const QString& categoryId);

/// Checks one version 15 row: its closed fields, its binding against the
/// Core's mirrored objects, settings scopes, telemetry and verbs, and its
/// gates. `why` names the first problem.
bool validateControl(const QString& categoryId, const QJsonObject& control,
                     QString* why = nullptr);

/// Checks every version 15 row of one section, including the staged rows
/// a button's `$control` arguments name.
bool validateSection(const QString& categoryId, const QJsonArray& controls,
                     QString* why = nullptr);

/// Fills the board-dependent fields of a version 15 row for the Core's
/// radio. Returns false when the row does not apply to this radio (the
/// desktop does not show it there).
bool projectForRadio(QJsonObject* control, const BoardCapabilities& caps, HPSDRModel model);

/// Fills the board-dependent fields of a page or section: `boardFamily`
/// on a section keeps it only on those radios. Returns false to drop it.
bool keepSectionForRadio(QJsonObject* section, const BoardCapabilities& caps);

} // namespace NereusSDR::SetupDescriptionV15
