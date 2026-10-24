#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RxOnlyLinkFit.h  (NereusSDR)
// =================================================================
//
// RX-only antenna values 4 and 5 on the station link (JJ's ruling
// 2026-10-22: the Alex tab's RX-only boxes are Thetis checkboxes again).
//
// Thetis stores the RX-only antenna as the sum of the checked boxes'
// (index + 1): 0 none, 1 and 2 the two inputs (which rule each other out),
// 3 XVTR, 4 the first input with XVTR, 5 the second with XVTR
// (setup.cs:13682 ProcessAlexAntCheckBox [v2.10.3.15]). A peer built before
// that reads `rxOnlyAntennas` entries 0 to 3 only (the phone refuses the
// whole RX antenna grid on anything else), so the Core sends a peer that
// did not declare `radioAntennaRows` 2 a 0 to 3 view: 4 and 5 read 3, the
// XVTR box, which Thetis names first (setup.cs:13728-13733 [v2.10.3.15]).
//
//   narrowRxOnly          one value as such a peer sees it.
//   forPeerWithoutPairs   the encoded message as such a peer sees it; the
//                         wire unchanged when no `rxOnlyAntennas` entry
//                         holds 4 or 5.
//   keepPairsFromNarrowList
//                         a whole `rxOnlyAntennas` list such a peer wrote,
//                         with each entry it saw as 3 and left alone kept
//                         at the Core's 4 or 5, and an entry above 3 (which
//                         it cannot mean) kept at the Core's value.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QString>

namespace NereusSDR::RxOnlyLinkFit {

inline constexpr const char* kProperty = "rxOnlyAntennas";
/// The highest value a peer without radioAntennaRows 2 reads.
inline constexpr int kNarrowMax = 3;

int narrowRxOnly(int value);

QByteArray forPeerWithoutPairs(const QByteArray& wire);

QString keepPairsFromNarrowList(const QString& current, const QString& written);

} // namespace NereusSDR::RxOnlyLinkFit
