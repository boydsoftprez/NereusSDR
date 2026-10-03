#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/BandLinkFit.h  (NereusSDR)
// =================================================================
//
// 2 m on the station link (JJ's ruling 2026-09-28: "Yes 2 meters is its own
// band"; R-IOS-26, R-R3-49).
//
// Band::Band2m is 27 on the link, appended after every band that existed
// before it, so no existing band's number moves. A peer built before it
// knows no band 27 (its enum domain refuses one) and holds per-band lists
// of 14 bands, so the link's rule applies: older peers see exactly the
// wire they were built for (link document, section 17). A client that
// knows 2 m declares the hello feature `band2m` 1; a Core that knows it
// advertises `band2mVersion` 1 (link document, sections 6.1 and 6.3).
//
//   forPeerWithout2m    what the Core sends a peer that did not declare
//                       band2m: every 2 m band number reads GEN (11), as
//                       2 m always has for it; 2 m's entry is left out of
//                       the catalogue's `bands`, out of the per-band watts
//                       maps and off the per-band antenna lists.
//   forStationWithout2m what a window writes to a Core without
//                       band2mVersion: the per-band watts maps and antenna
//                       lists without their 2 m entry, as that Core reads
//                       them. A window never sends band 27 itself to such a
//                       Core; its 2 m controls say why they cannot.
//
// Both return the wire unchanged when nothing in it names 2 m.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), 2 m as its own band (R-IOS-26, R-R3-49), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>

namespace NereusSDR::BandLinkFit {

/// The hello feature a client declares when it knows the 2 m band.
inline constexpr const char* kFeature = "band2m";
/// 2 m's band number on the link (Band::Band2m).
inline constexpr int k2mNumber = 27;
/// What a peer without 2 m reads in its place (Band::GEN).
inline constexpr int kGenNumber = 11;
/// The number of per-band antenna entries a peer without 2 m reads.
inline constexpr int kListEntriesWithout2m = 14;

/// The Core's encoded message for a peer that did not declare band2m.
QByteArray forPeerWithout2m(const QByteArray& wire);

/// A window's encoded message for a Core without band2mVersion.
QByteArray forStationWithout2m(const QByteArray& wire);

} // namespace NereusSDR::BandLinkFit
