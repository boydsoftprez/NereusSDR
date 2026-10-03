// no-port-check: NereusSDR-original. The one list of features not built yet.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/UnbuiltFeatures.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// R-R3-49: every control a user can see does what its label says. A control
// whose feature is not built yet is hidden, in local and remote windows,
// through this one list. Each entry names one unbuilt feature from the
// operator's decisions of 2026-09-23 (the "Hide" rows of the R3 unfinished
// controls plan). Every menu item, Setup page or group, applet control,
// status bar item and container control that fronts such a feature asks
// isBuilt() before it is shown.
//
// Building a feature later is one change here (drop its entry, or return
// true for it) plus the feature itself: its surfaces appear again, because
// a hidden control keeps its code and its saved settings. Nothing here
// removes a control or touches a saved value.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27 - Task 25 dead-control audit adds explicit missing-port
//                 entries. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-49, R-R3-21). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Fix wave: one entry per feature for the
//                                    container buttons and the review's
//                                    unfinished controls. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (remote-window parity Task 6): LocalNetworkStats
//                 retired; the rows are measured and shown. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (remote-window parity Task 13): AlexTxFilterOptions
//                 and Hl2TxTiming (plan C5, C6). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 - A11 / R-R3-49 (remote-window parity Task 31): the container
//                 DUP button leaves Fdx (display duplex is built); Fdx is the
//                 status bar's FDX, full duplex, still unbuilt. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - iPhone app plan Task 25 (D41): the enum, all(), key(),
//                 isBuilt() and the test seams moved unchanged to
//                 src/core/UnbuiltFeatureList.h, which this header includes,
//                 so the Core's catalogue reads the same list. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/UnbuiltFeatureList.h"

#include <QList>
#include <QString>

class QAction;
class QLayout;
class QWidget;

namespace NereusSDR {


namespace UnbuiltFeatures {

// Hide `widget` while `feature` is not built. A widget in a QFormLayout
// hides its whole form row (label and field). Nothing happens once the
// feature is built, so a surface keeps whatever other gate it has.
void hideUnlessBuilt(QWidget* widget, UnbuiltFeature feature);

/// JJ's rule "disabled, never hidden": the control stays in view, disabled,
/// with `reason` as its tooltip, until `feature` is built.
void disableUnlessBuilt(QWidget* widget, UnbuiltFeature feature, const QString& reason);

/// The words an unbuilt control shows: what it does today, no promise.
QString notBuiltReason();

// Hide the labelled row `control` sits in: its QFormLayout row, its
// QGridLayout row, or the horizontal row layout holding it and its label.
// A control directly in a vertical layout hides alone. The row is found
// from the control's parent widget, or from `searchFrom` for a layout not
// yet installed on a widget.
void hideRowUnlessBuilt(QWidget* control, UnbuiltFeature feature,
                        QLayout* searchFrom = nullptr);

// Hide every widget inside `layout` (a row built as its own layout).
void hideLayoutUnlessBuilt(QLayout* layout, UnbuiltFeature feature);

// Hide a menu item (for a submenu, pass menu->menuAction()).
void hideUnlessBuilt(QAction* action, UnbuiltFeature feature);

} // namespace UnbuiltFeatures
} // namespace NereusSDR
