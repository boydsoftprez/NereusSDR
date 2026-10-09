// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorModels.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/core/RotorModels.h [@551576e], original header from Longpath
//   source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code. Rotor control plan, Task 3a. The list, its
//               order and the baud rates are Longpath's; every model
//               number was checked again against Hamlib master
//               include/hamlib/rotlist.h [@50fc454] (read 2026-10-08) and
//               each entry cites its line there. Names and notes are
//               reworded in plain English for the rotor setup picker,
//               without dashes; the ERC's note names it as the model for
//               an Easy Rotor Control. Namespace Longpath becomes
//               NereusSDR.
// =================================================================
//
// --- From RotorModels.h ---
//
// =================================================================
// src/core/RotorModels.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// The rotator controllers an operator is likely to own, by name, with
// the Hamlib model number each one needs.
//
// Hamlib supports around sixty. Offering all sixty is not help — most
// of the list is telescope mounts and controllers nobody in this hobby
// has seen, and a long list of unfamiliar names is harder to answer
// than a short one. So this is curated, ordered by how common the
// controller is, and there is a free field beside it for the numbers
// that are not here.
//
// Two controllers worth naming because they are asked about and both
// work through an emulation rather than a driver of their own:
//
//   ERC (Easy-Rotor-Control, DF9GR) — has its own Hamlib driver, 404.
//       It also emulates GS-232A/B and DCU-1, so if 404 misbehaves the
//       emulations are a fallback rather than a dead end.
//
//   ARCO (microHAM) — no Hamlib driver of its own, but it speaks
//       Yaesu GS-232A, DCU-1/Rotor-EZ and SPID over LAN, USB and
//       RS-232. GS-232A is the one to pick.
//
// Model numbers verified against the Hamlib 4.7.1 rotator list
// (github.com/Hamlib/Hamlib/wiki/Supported-Rotators, read 2026-08-07).
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — GS-232A note now explains the network form of the
//                 device field (address:port), which is how an ARCO on
//                 the LAN is reached. AI-assisted via Anthropic Claude
//                 (Cowork), operator Martin Fischer.
// =================================================================

#pragma once

#include <QString>
#include <QVector>

namespace NereusSDR {

// From Longpath src/core/RotorModels.h:48-55 [@551576e]
struct RotorModel {
    int     hamlibId;
    QString name;
    // Shown under the picker. Says what the entry actually is, because
    // "GS-232A" means nothing to someone holding an ARCO box.
    QString note;
};

// Hamlib numbers its rotators ROT_MAKE_MODEL(family, n) = 100 * family + n
// (Hamlib include/hamlib/rotlist.h:68 [@50fc454]).
//
// From Longpath src/core/RotorModels.h:56-108 [@551576e]
// Ordered by how likely an operator is to own one, not alphabetically.
inline QVector<RotorModel> commonRotorModels()
{
    return {
        // Hamlib rotlist.h:373 [@50fc454], ROT_MODEL_GS232A
        {601,  QStringLiteral("Yaesu GS-232A"),
               QStringLiteral("Also the choice for microHAM ARCO and ARCO "
                              "Junior, and for an ERC set to GS-232A. For an "
                              "ARCO on the network, enter its address and "
                              "GS-232A port in place of the serial port, for "
                              "example 192.168.1.50:4001.")},
        // Hamlib rotlist.h:375 [@50fc454], ROT_MODEL_GS232B
        {603,  QStringLiteral("Yaesu GS-232B"),
               QStringLiteral("G-800DXA, G-1000DXA and G-2800DXA with the "
                              "B series controller.")},
        // Hamlib rotlist.h:234 [@50fc454], ROT_MODEL_ERC
        {404,  QStringLiteral("ERC Easy Rotor Control (DF9GR)"),
               QStringLiteral("ERC Mini DX, ERC-DUO and ERC-M. If this one "
                              "gives trouble, the GS-232 entries above work "
                              "too.")},
        // Hamlib rotlist.h:233 [@50fc454], ROT_MODEL_DCU
        {403,  QStringLiteral("Hy-Gain DCU-1 / DCU-1X"),
               QStringLiteral("Also ARCO set to DCU-1, and Idiom Press "
                              "Rotor-EZ boards.")},
        // Hamlib rotlist.h:231 [@50fc454], ROT_MODEL_ROTOREZ
        {401,  QStringLiteral("Idiom Press Rotor-EZ"),
               QStringLiteral("Hy-Gain Ham-IV or T2X with a Rotor-EZ board.")},
        // Hamlib rotlist.h:235 [@50fc454], ROT_MODEL_RT21
        {405,  QStringLiteral("Green Heron RT-21"),
               QStringLiteral("RT-21, RT-21D and RT-990.")},
        // Hamlib rotlist.h:458 [@50fc454], ROT_MODEL_SPID_ROT2PROG
        {901,  QStringLiteral("SPID Rot2Prog"),
               QStringLiteral("Azimuth and elevation.")},
        // Hamlib rotlist.h:459 [@50fc454], ROT_MODEL_SPID_ROT1PROG
        {902,  QStringLiteral("SPID Rot1Prog"),
               QStringLiteral("Azimuth only. Also ARCO set to SPID.")},
        // Hamlib rotlist.h:460 [@50fc454], ROT_MODEL_SPID_MD01_ROT2PROG
        {903,  QStringLiteral("SPID MD-01 / MD-02"),
               QStringLiteral("Set to ROT2.")},
        // Hamlib rotlist.h:492 [@50fc454], ROT_MODEL_RC2800
        {1001, QStringLiteral("M2 RC2800"),
               QStringLiteral("M2 Antenna Systems and Orion.")},
        // Hamlib rotlist.h:639, 641 [@50fc454], ROT_MODEL_PROSISTEL_D_AZ
        // and ROT_MODEL_PROSISTEL_COMBI_TRACK_AZEL (1703)
        {1701, QStringLiteral("Prosistel D azimuth"),
               QStringLiteral("For a Combi-Track, enter model 1703.")},
        // Hamlib rotlist.h:518-519 [@50fc454], ROT_MODEL_RCI_AZEL and
        // ROT_MODEL_RCI_AZ (1102)
        {1101, QStringLiteral("EA4TX ARS RCI azimuth and elevation"),
               QStringLiteral("For azimuth only, enter model 1102.")},
        // Hamlib rotlist.h:378 [@50fc454], ROT_MODEL_GS232 ("Not A or B")
        {606,  QStringLiteral("Yaesu / Kenpro GS-232"),
               QStringLiteral("The original, neither A nor B.")},
        // Hamlib rotlist.h:377 [@50fc454], ROT_MODEL_GS23
        {605,  QStringLiteral("Yaesu / Kenpro GS-23"),
               QStringLiteral("Also some older GS-232 controllers.")},
        // Hamlib rotlist.h:374 [@50fc454], ROT_MODEL_GS232_GENERIC
        {602,  QStringLiteral("GS-232 generic"),
               QStringLiteral("Home-built and Arduino controllers that "
                              "speak GS-232, such as the K3NG.")},
        // Hamlib rotlist.h:583 [@50fc454], ROT_MODEL_ETHER6
        {1501, QStringLiteral("Ether6 (network)"),
               QStringLiteral("Connects over Ethernet rather than a serial "
                              "port.")},
        // Hamlib rotlist.h:123 [@50fc454], ROT_MODEL_DUMMY
        {1,    QStringLiteral("Hamlib dummy"),
               QStringLiteral("A rotor that does not exist, for checking "
                              "the link works without turning the mast.")},
    };
}

// From Longpath src/core/RotorModels.h:110-115 [@551576e]
// Baud rates worth offering. Most controllers ship at 9600; the ERC
// family and some GS-232 boxes are configurable.
inline QVector<int> commonRotorBauds()
{
    return {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
}

} // namespace NereusSDR
