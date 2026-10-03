// no-port-check: NereusSDR-original. Remote daemon R3 reason wording.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/OperatorReasonText.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Display-time translation of the
// reasons the Core sends (and this computer records) into user words.
//
// The reasons themselves never change: older apps compare some of them as
// exact text, and the log keeps the raw text. Only what a user reads is
// translated, here, when it is shown (R-R3-17, R-R3-21, R-R3-23, R-R3-35,
// R-R3-37).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  Link, audio and display, command and
//                                    model refusal reasons; unknown reasons
//                                    shown as sent when plain; the one
//                                    internal-term list. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  Shorter pan forms and a pan's next
//                                    step per reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  Media transport, NNR rate, certificate
//                                    and LAN discovery reasons; the table's
//                                    keys for the source check. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  This app's "does not support"
//                                    refusals in Core words, the NNR
//                                    adapter's reasons and the Core's Tuner
//                                    Genius XL checks. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  Receiver audio stop reasons (R-R3-43).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4b (R-IOS-01): the
//                                    Core's reasons are in operator words
//                                    now; each shows as before, and an
//                                    older Core's text still does.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    the Tuner Genius set-up refusal names
//                                    the device in words, not "TGXL"; the
//                                    Core's old accessory check wording is
//                                    an older Core's now.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QString>
#include <QStringList>

namespace NereusSDR::OperatorReasonText {

/// A sentence in user words saying why, for a reason as the Core sent it
/// or this computer recorded it. A reason the table does not know is shown
/// as sent when it names no internal term; otherwise, and for an empty
/// reason, a general sentence. Each reason shown in other words is written
/// to the log once, raw, so the log always has it.
QString forDisplay(const QString& wireReason);

/// A short line for a pan, for the same reason: the longest of
/// shortFormsForDisplay().
QString shortForDisplay(const QString& wireReason);

/// Every short line for a pan, longest first; the pan paints the longest
/// that fits. Never empty.
QStringList shortFormsForDisplay(const QString& wireReason);

/// The sentence a pan adds after the reason: what happens next, or what the
/// user can do. The Core's refusals are asked again when the pan's view
/// changes; this app's own limits say what lifts them.
QString panNextStep(const QString& wireReason);

/// LAN discovery's status: one or more sentences, each "Station LAN ...",
/// joined by spaces (StationLanDiscovery::refreshLastError). Each is shown
/// in user words and once, as forDisplay() shows it.
QString lanDiscoveryForDisplay(const QString& lastError);

/// Every reason the table knows (one example for each worded pattern), for
/// tests that check its wording.
QStringList knownReasons();

/// The exact reasons the table matches, byte for byte as they are written
/// where they come from, and the media-start prefix: what a test checks is
/// still written in the sources, so a rewording there cannot silently fall
/// back to the general sentence.
QStringList tableKeys();

/// The reasons only an older Core sends (R-IOS-01), byte for byte: the
/// Core now words each in operator words, and this app still shows an older
/// Core's text in the same words as before. None is still written in the
/// sources; a test checks that.
QStringList olderCoreKeys();

/// The words nothing a user reads may use: internal subsystem, roadmap and
/// wire terms. Each matches at the start of a word, case-insensitively, so
/// "capabilit" also catches "capabilities" and "plane" does not catch
/// "airplane". Tests use this same list (tests/OperatorWording.h).
const QStringList& internalTerms();

/// The first internal term `text` names, or an empty string.
QString internalTermIn(const QString& text);

} // namespace NereusSDR::OperatorReasonText
