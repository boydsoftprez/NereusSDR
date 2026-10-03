// 2026-09-27: shared TX filter geometry and validated band-edge admission.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================
// src/core/safety/BandPlanGuard.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/clsBandStackManager.cs
//   Project Files/Source/Console/setup.designer.cs
//
// Original licences from each Thetis source file are included below,
// verbatim, separated by // --- From [filename] --- markers per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Completed the 24 country HF range tables from Thetis
//                v2.10.3.15 @3759d096 with AI assistance via OpenAI Codex.
//   2026-09-28: Addendum G-42 item 4: each band plan refusal says what is
//                wrong in the operator's words, after Thetis's MOX messages
//                (console.cs:29452-29530 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From setup.designer.cs ---
// (Auto-generated Visual Studio designer file; no upstream copyright header present.
//  Cite stamp v2.10.3.13 @501e3f5 traces back to that designer file in the Thetis repo.
//  The comboFRSRegion 24-region list is sourced from
//  setup.designer.cs:8084-8108 [v2.10.3.13].)

#include "core/safety/BandPlanGuard.h"
#include <array>
#include <limits>

namespace NereusSDR::safety {

namespace {

// ---------------------------------------------------------------------------
// Internal data types
// ---------------------------------------------------------------------------

struct ChannelEntry {
    std::int64_t centerHz;
    std::int64_t bwHz;
};

struct BandRange {
    Band      band;
    std::int64_t loHz;
    std::int64_t hiHz;
};

// ---------------------------------------------------------------------------
// 60m channel tables
// Verbatim from Thetis console.cs:2643-2669 [v2.10.3.13] (Init60mChannels).
// Channel constructor signature: Channel(double MHz, int bwHz).
// ---------------------------------------------------------------------------

// UK: 11 channels with per-channel BW ranging 3–12.5 kHz
// (console.cs:2644-2654 [v2.10.3.13]).
constexpr std::array<ChannelEntry, 11> kUkChannels60m{{
    { 5'261'250,  5'500 },   // console.cs:2644 — Channel(5.26125,  5500)
    { 5'280'000,  8'000 },   // console.cs:2645 — Channel(5.2800,   8000)
    { 5'290'250,  3'500 },   // console.cs:2646 — Channel(5.29025,  3500)
    { 5'302'500,  9'000 },   // console.cs:2647 — Channel(5.3025,   9000)
    { 5'318'000, 10'000 },   // console.cs:2648 — Channel(5.3180,  10000)
    { 5'335'500,  5'000 },   // console.cs:2649 — Channel(5.3355,   5000)
    { 5'356'000,  4'000 },   // console.cs:2650 — Channel(5.3560,   4000)
    { 5'368'250, 12'500 },   // console.cs:2651 — Channel(5.36825, 12500)
    { 5'380'000,  4'000 },   // console.cs:2652 — Channel(5.3800,   4000)
    { 5'398'250,  6'500 },   // console.cs:2653 — Channel(5.39825,  6500)
    { 5'405'000,  3'000 },   // console.cs:2654 — Channel(5.4050,   3000)
}};

// US: 5 channel center frequencies (console.cs:2657-2661 [v2.10.3.13]).
// Used by Thetis only for VFO snap / band-stack display, NOT for TX gating
// (IsOKToTX in clsBandStackManager.cs:1063-1083 [v2.10.3.13] permits TX
// anywhere in the broad B60M 5.1-5.5 MHz range — see kUsBandRanges below).
// Retained here for cite-trail integrity; not consulted by isValidTxFreq.
// See channels60mFor: US falls through to the broad range case.
[[maybe_unused]] constexpr std::array<ChannelEntry, 5> kUsChannels60m{{
    { 5'332'000, 2'800 },   // console.cs:2657 — Channel(5.3320, 2800)
    { 5'348'000, 2'800 },   // console.cs:2658 — Channel(5.3480, 2800)
    { 5'358'500, 2'800 },   // console.cs:2659 — Channel(5.3585, 2800)
    { 5'373'000, 2'800 },   // console.cs:2660 — Channel(5.3730, 2800)
    { 5'405'000, 2'800 },   // console.cs:2661 — Channel(5.4050, 2800)
}};

// Japan 60m: 4.63 MHz discrete allocation.
// (clsBandStackManager.cs:1471 [v2.10.3.13]):
//   BandFrequencyData(4.629995, 4.630005, Band.B60M, BandType.HF, false, region)
// BW = (4.630005 - 4.629995) MHz × 1e6 = 10 Hz — center 4.630000 MHz.
constexpr std::array<ChannelEntry, 1> kJapanChannels60m{{
    { 4'630'000, 10 },   // 4.629995-4.630005 MHz
}};

// ---------------------------------------------------------------------------
// Per-region HF band-edge tables
// clsBandStackManager.cs:1334-1730 [v2.10.3.15 @3759d096].
// IsOKToTX (lines 1063-1083) accepts HF, excluding WWV and BLMF;
// VHF B2M and general-coverage rows are deliberately absent.
// Italy selects the upstream Italy_Plus table (setup.cs:14209-14210).
// UK/Japan 60m rows remain subject to the native channel gates below.
// ---------------------------------------------------------------------------

// US: clsBandStackManager.cs:1335-1345.
constexpr std::array<BandRange, 11> kUsBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1335
    { Band::Band80m,   3'500'000,   4'000'000 },  // :1336
    { Band::Band60m,   5'100'000,   5'500'000 },  // :1337
    { Band::Band40m,   7'000'000,   7'300'000 },  // :1338
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1339
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1340
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1341
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1342
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1343
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1344
    { Band::Band6m,  50'000'000,  54'000'000 },  // :1345
}};

// India: clsBandStackManager.cs:1376-1386.
constexpr std::array<BandRange, 11> kIndiaBandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1376
    { Band::Band80m,   3'500'000,   3'900'000 },  // :1377
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1378
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1379
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1380
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1381
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1382
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1383
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1384
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1385
    { Band::Band6m,  50'000'000,  54'000'000 },  // :1386
}};

// Spain: clsBandStackManager.cs:1391-1401.
constexpr std::array<BandRange, 11> kSpainBandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1391
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1392
    { Band::Band60m,   5'100'000,   5'500'000 },  // :1393
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1394
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1395
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1396
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1397
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1398
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1399
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1400
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1401
}};

// Europe: clsBandStackManager.cs:1406-1416.
constexpr std::array<BandRange, 11> kEuropeBandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1406
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1407
    { Band::Band60m,   5'100'000,   5'500'000 },  // :1408
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1409
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1410
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1411
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1412
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1413
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1414
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1415
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1416
}};

// Israel: clsBandStackManager.cs:1421-1431.
constexpr std::array<BandRange, 11> kIsraelBandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1421
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1422
    { Band::Band60m,   5'000'000,   5'500'000 },  // :1423
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1424
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1425
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1426
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1427
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1428
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1429
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1430
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1431
}};

// UK: clsBandStackManager.cs:1438-1448.
constexpr std::array<BandRange, 11> kUkBandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1438
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1439
    { Band::Band60m,   5'250'000,   5'410'000 },  // :1440
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1441
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1442
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1443
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1444
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1445
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1446
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1447
    { Band::Band6m,  50'030'000,  52'000'000 },  // :1448
}};

// Italy_Plus: clsBandStackManager.cs:1453-1463.
constexpr std::array<BandRange, 11> kItalyBandRanges{{
    { Band::Band160m,   1'830'000,   1'850'000 },  // :1453
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1454
    { Band::Band60m,   5'000'000,   6'975'000 },  // :1455
    { Band::Band40m,   6'975'000,   7'200'000 },  // :1456
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1457
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1458
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1459
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1460
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1461
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1462
    { Band::Band6m,  50'080'000,  51'000'000 },  // :1463
}};

// Japan: clsBandStackManager.cs:1469-1479.
constexpr std::array<BandRange, 11> kJapanBandRanges{{
    { Band::Band160m,   1'830'000,   1'912'500 },  // :1469
    { Band::Band80m,   3'500'000,   3'805'000 },  // :1470
    { Band::Band60m,   4'629'995,   4'630'005 },  // :1471
    { Band::Band40m,   6'975'000,   7'200'000 },  // :1472
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1473
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1474
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1475
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1476
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1477
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1478
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1479
}};

// Australia: clsBandStackManager.cs:1486-1496.
constexpr std::array<BandRange, 11> kAustraliaBandRanges{{
    { Band::Band160m,   1'810'000,   1'875'000 },  // :1486
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1487
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1488
    { Band::Band40m,   7'000'000,   7'300'000 },  // :1489
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1490
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1491
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1492
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1493
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1494
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1495
    { Band::Band6m,  50'000'000,  54'000'000 },  // :1496
}};

// Norway: clsBandStackManager.cs:1501-1511.
constexpr std::array<BandRange, 11> kNorwayBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1501
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1502
    { Band::Band60m,   5'250'000,   5'450'000 },  // :1503
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1504
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1505
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1506
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1507
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1508
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1509
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1510
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1511
}};

// Denmark: clsBandStackManager.cs:1516-1526.
constexpr std::array<BandRange, 11> kDenmarkBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1516
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1517
    { Band::Band60m,   5'250'000,   5'450'000 },  // :1518
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1519
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1520
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1521
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1522
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1523
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1524
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1525
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1526
}};

// Latvia: clsBandStackManager.cs:1531-1541.
constexpr std::array<BandRange, 11> kLatviaBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1531
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1532
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1533
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1534
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1535
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1536
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1537
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1538
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1539
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1540
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1541
}};

// Slovakia: clsBandStackManager.cs:1546-1556.
constexpr std::array<BandRange, 11> kSlovakiaBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1546
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1547
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1548
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1549
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1550
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1551
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1552
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1553
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1554
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1555
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1556
}};

// Bulgaria: clsBandStackManager.cs:1561-1571.
constexpr std::array<BandRange, 11> kBulgariaBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1561
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1562
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1563
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1564
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1565
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1566
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1567
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1568
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1569
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1570
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1571
}};

// Greece: clsBandStackManager.cs:1576-1586.
constexpr std::array<BandRange, 11> kGreeceBandRanges{{
    { Band::Band160m,   1'800'000,   1'850'000 },  // :1576
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1577
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1578
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1579
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1580
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1581
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1582
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1583
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1584
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1585
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1586
}};

// Hungary: clsBandStackManager.cs:1591-1601.
constexpr std::array<BandRange, 11> kHungaryBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1591
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1592
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1593
    { Band::Band40m,   7'000'000,   7'100'000 },  // :1594
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1595
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1596
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1597
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1598
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1599
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1600
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1601
}};

// Netherlands: clsBandStackManager.cs:1606-1616.
constexpr std::array<BandRange, 11> kNetherlandsBandRanges{{
    { Band::Band160m,   1'800'000,   1'880'000 },  // :1606
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1607
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1608
    { Band::Band40m,   7'000'000,   7'100'000 },  // :1609
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1610
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1611
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1612
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1613
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1614
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1615
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1616
}};

// France: clsBandStackManager.cs:1621-1631.
constexpr std::array<BandRange, 11> kFranceBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1621
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1622
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1623
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1624
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1625
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1626
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1627
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1628
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1629
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1630
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1631
}};

// Russia: clsBandStackManager.cs:1636-1646.
constexpr std::array<BandRange, 11> kRussiaBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1636
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1637
    { Band::Band60m,   5'000'000,   7'000'000 },  // :1638
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1639
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1640
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1641
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1642
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1643
    { Band::Band12m,  24'890'000,  25'140'000 },  // :1644
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1645
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1646
}};

// Sweden: clsBandStackManager.cs:1651-1661.
constexpr std::array<BandRange, 11> kSwedenBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1651
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1652
    { Band::Band60m,   5'310'000,   5'930'000 },  // :1653
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1654
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1655
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1656
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1657
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1658
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1659
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1660
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1661
}};

// Germany: clsBandStackManager.cs:1667-1677.
constexpr std::array<BandRange, 11> kGermanyBandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1667
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1668
    { Band::Band60m,   5'351'500,   5'366'500 },  // :1669
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1670
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1671
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1672
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1673
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1674
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1675
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1676
    { Band::Band6m,  50'000'000,  51'000'000 },  // :1677
}};

// Region1: clsBandStackManager.cs:1684-1694.
constexpr std::array<BandRange, 11> kRegion1BandRanges{{
    { Band::Band160m,   1'810'000,   2'000'000 },  // :1684
    { Band::Band80m,   3'500'000,   3'800'000 },  // :1685
    { Band::Band60m,   5'351'500,   5'366'500 },  // :1686
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1687
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1688
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1689
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1690
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1691
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1692
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1693
    { Band::Band6m,  50'000'000,  52'000'000 },  // :1694
}};

// Region2: clsBandStackManager.cs:1701-1711.
constexpr std::array<BandRange, 11> kRegion2BandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1701
    { Band::Band80m,   3'500'000,   4'000'000 },  // :1702
    { Band::Band60m,   5'351'500,   5'366'500 },  // :1703
    { Band::Band40m,   7'000'000,   7'300'000 },  // :1704
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1705
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1706
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1707
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1708
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1709
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1710
    { Band::Band6m,  50'000'000,  54'000'000 },  // :1711
}};

// Region3: clsBandStackManager.cs:1718-1728.
constexpr std::array<BandRange, 11> kRegion3BandRanges{{
    { Band::Band160m,   1'800'000,   2'000'000 },  // :1718
    { Band::Band80m,   3'500'000,   3'900'000 },  // :1719
    { Band::Band60m,   5'351'500,   5'366'500 },  // :1720
    { Band::Band40m,   7'000'000,   7'200'000 },  // :1721
    { Band::Band30m,  10'100'000,  10'150'000 },  // :1722
    { Band::Band20m,  14'000'000,  14'350'000 },  // :1723
    { Band::Band17m,  18'068'000,  18'168'000 },  // :1724
    { Band::Band15m,  21'000'000,  21'450'000 },  // :1725
    { Band::Band12m,  24'890'000,  24'990'000 },  // :1726
    { Band::Band10m,  28'000'000,  29'700'000 },  // :1727
    { Band::Band6m,  50'000'000,  54'000'000 },  // :1728
}};
// ---------------------------------------------------------------------------
// Helper: return 60m channels for region (empty span → no channelization)
// ---------------------------------------------------------------------------

// NOTE: Apple-clang < 16 cannot deduce std::span from a constexpr switch
// return whose arms have different array element counts. We return a
// ChannelEntry const* + std::size_t pair and wrap at call-site to avoid
// the deduction issue while preserving constexpr semantics.
struct ChannelSpan {
    const ChannelEntry* data;
    std::size_t         size;
};

static ChannelSpan channels60mFor(Region region) noexcept
{
    switch (region) {
    case Region::UnitedKingdom:
        return { kUkChannels60m.data(), kUkChannels60m.size() };
    case Region::Japan:
        return { kJapanChannels60m.data(), kJapanChannels60m.size() };
    // US is intentionally NOT channelized for TX gating. Thetis allows TX
    // anywhere in the broad B60M 5.1-5.5 MHz range with the USB/CWL/CWU/DIGU
    // mode restriction (IsOKToTX at clsBandStackManager.cs:1063-1083
    // [v2.10.3.13] + US mode switch at console.cs:29416-29432 [v2.10.3.13]).
    // The FCC dial convention (47 CFR 97.303(h)) places USB suppressed-carrier
    // at channel_center - 1.5 kHz, which sits OUTSIDE a 2.8 kHz wide channel
    // window centred on Thetis's channel-center constants — channelizing the
    // gate at those centers rejected the very dial frequencies operators use
    // (issue #271, ANAN-10E on macOS, 2026-05-18). US returns {nullptr, 0}
    // so the broad-range branch in isValidTxFreq governs.
    //
    default:
        return { nullptr, 0 };
    }
}

// ---------------------------------------------------------------------------
// Helper: return band-edge ranges for region
// ---------------------------------------------------------------------------

struct RangeSpan {
    const BandRange* data;
    std::size_t      size;
};

static RangeSpan bandRangesFor(Region region) noexcept
{
    switch (region) {
    case Region::UnitedStates:
        return { kUsBandRanges.data(), kUsBandRanges.size() };
    case Region::India:
        return { kIndiaBandRanges.data(), kIndiaBandRanges.size() };
    case Region::Spain:
        return { kSpainBandRanges.data(), kSpainBandRanges.size() };
    case Region::Europe:
        return { kEuropeBandRanges.data(), kEuropeBandRanges.size() };
    case Region::Israel:
        return { kIsraelBandRanges.data(), kIsraelBandRanges.size() };
    case Region::UnitedKingdom:
        return { kUkBandRanges.data(), kUkBandRanges.size() };
    case Region::Italy:
        return { kItalyBandRanges.data(), kItalyBandRanges.size() };
    case Region::Japan:
        return { kJapanBandRanges.data(), kJapanBandRanges.size() };
    case Region::Australia:
        return { kAustraliaBandRanges.data(), kAustraliaBandRanges.size() };
    case Region::Norway:
        return { kNorwayBandRanges.data(), kNorwayBandRanges.size() };
    case Region::Denmark:
        return { kDenmarkBandRanges.data(), kDenmarkBandRanges.size() };
    case Region::Latvia:
        return { kLatviaBandRanges.data(), kLatviaBandRanges.size() };
    case Region::Slovakia:
        return { kSlovakiaBandRanges.data(), kSlovakiaBandRanges.size() };
    case Region::Bulgaria:
        return { kBulgariaBandRanges.data(), kBulgariaBandRanges.size() };
    case Region::Greece:
        return { kGreeceBandRanges.data(), kGreeceBandRanges.size() };
    case Region::Hungary:
        return { kHungaryBandRanges.data(), kHungaryBandRanges.size() };
    case Region::Netherlands:
        return { kNetherlandsBandRanges.data(), kNetherlandsBandRanges.size() };
    case Region::France:
        return { kFranceBandRanges.data(), kFranceBandRanges.size() };
    case Region::Russia:
        return { kRussiaBandRanges.data(), kRussiaBandRanges.size() };
    case Region::Sweden:
        return { kSwedenBandRanges.data(), kSwedenBandRanges.size() };
    case Region::Germany:
        return { kGermanyBandRanges.data(), kGermanyBandRanges.size() };
    case Region::Region1:
        return { kRegion1BandRanges.data(), kRegion1BandRanges.size() };
    case Region::Region2:
        return { kRegion2BandRanges.data(), kRegion2BandRanges.size() };
    case Region::Region3:
        return { kRegion3BandRanges.data(), kRegion3BandRanges.size() };
    default:
        return { nullptr, 0 };  // Unsupported enum: fail closed.
    }
}

// ---------------------------------------------------------------------------
// Predicate helpers
// ---------------------------------------------------------------------------

// US 60m mode restriction — console.cs:29416-29432 [v2.10.3.13]:
//   _tx_band == Band.B60M && current_region == FRSRegion.US && !extended
//   → switch on CurrentDSPMode: allow USB / CWL / CWU / DIGU, reject all others.
static bool isUs60mModeAllowed(DSPMode mode) noexcept
{
    return mode == DSPMode::USB  ||   // console.cs:29420
           mode == DSPMode::CWL  ||   // console.cs:29421
           mode == DSPMode::CWU  ||   // console.cs:29422
           mode == DSPMode::DIGU;     // console.cs:29423
}

// Addendum G-42 item 4: the region as the operator knows it, for a refusal
// the operator reads: General Options' names (the comboFRSRegion list
// above), with the three IARU regions spelled out.
QString regionWords(Region region)
{
    switch (region) {
    case Region::Australia:     return QStringLiteral("Australia");
    case Region::Europe:        return QStringLiteral("Europe");
    case Region::India:         return QStringLiteral("India");
    case Region::Italy:         return QStringLiteral("Italy");
    case Region::Israel:        return QStringLiteral("Israel");
    case Region::Japan:         return QStringLiteral("Japan");
    case Region::Spain:         return QStringLiteral("Spain");
    case Region::UnitedKingdom: return QStringLiteral("United Kingdom");
    case Region::UnitedStates:  return QStringLiteral("United States");
    case Region::Norway:        return QStringLiteral("Norway");
    case Region::Denmark:       return QStringLiteral("Denmark");
    case Region::Sweden:        return QStringLiteral("Sweden");
    case Region::Latvia:        return QStringLiteral("Latvia");
    case Region::Slovakia:      return QStringLiteral("Slovakia");
    case Region::Bulgaria:      return QStringLiteral("Bulgaria");
    case Region::Greece:        return QStringLiteral("Greece");
    case Region::Hungary:       return QStringLiteral("Hungary");
    case Region::Netherlands:   return QStringLiteral("Netherlands");
    case Region::France:        return QStringLiteral("France");
    case Region::Russia:        return QStringLiteral("Russia");
    // The combo keeps Thetis's Region1-3; a sentence names the IARU region.
    case Region::Region1:       return QStringLiteral("IARU Region 1");
    case Region::Region2:       return QStringLiteral("IARU Region 2");
    case Region::Region3:       return QStringLiteral("IARU Region 3");
    case Region::Germany:       return QStringLiteral("Germany");
    }
    return QStringLiteral("your region");
}

// The modes a US 60 m refusal can name: the transmit modes the mode list
// admits other than USB and DIGU.
QString modeWords(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:    return QStringLiteral("LSB");
    case DSPMode::DIGL:   return QStringLiteral("DIGL");
    case DSPMode::AM:     return QStringLiteral("AM");
    case DSPMode::SAM:    return QStringLiteral("SAM");
    case DSPMode::DSB:    return QStringLiteral("DSB");
    case DSPMode::RADE_U: return QStringLiteral("RADE-U");
    case DSPMode::RADE_L: return QStringLiteral("RADE-L");
    default:              return QStringLiteral("This mode");
    }
}

// A band as a sentence names it: "40 m", "general coverage", "WWV".
QString bandWords(Band band)
{
    if (band == Band::GEN) {
        return QStringLiteral("general coverage");
    }
    const QString label = bandLabel(band);
    if (!label.isEmpty() && label.front().isDigit() && label.endsWith(QLatin1Char('m'))) {
        return label.left(label.size() - 1) + QStringLiteral(" m");
    }
    return label;
}

QString mhzWords(std::int64_t freqHz)
{
    return QString::number(static_cast<double>(freqHz) / 1e6, 'f', 6);
}

static bool isInChannel(std::int64_t freqHz, const ChannelEntry& ch) noexcept
{
    const auto half = ch.bwHz / 2;
    return freqHz >= (ch.centerHz - half) &&
           freqHz <= (ch.centerHz + half);
}

static bool isInBandRange(std::int64_t freqHz, const BandRange& range) noexcept
{
    return freqHz >= range.loHz && freqHz <= range.hiHz;
}

} // namespace

// ---------------------------------------------------------------------------
// BandPlanGuard predicates
// ---------------------------------------------------------------------------

bool BandPlanGuard::isValidTxFreq(Region region, std::int64_t freqHz,
                                  DSPMode mode, bool extended) const noexcept
{
    // Extended toggle bypass — console.cs:6772, 6810 [v2.10.3.13].
    if (extended) {
        return true;
    }

    // Channelized 60m gating (UK, Japan): per-channel allocations are the
    // authoritative TX window. NereusSDR-native safety net for the strict
    // per-channel allocations defined by those regulators. Thetis itself
    // permits TX anywhere in the broad B60M range; we retain channelization
    // for UK / Japan because the regulator-defined allocation IS the channel
    // set, not a wider band.
    const ChannelSpan ch60m = channels60mFor(region);
    for (std::size_t i = 0; i < ch60m.size; ++i) {
        if (isInChannel(freqHz, ch60m.data[i])) {
            return true;
        }
    }

    // Per-region band edges.
    //
    // US 60m mode restriction — console.cs:29416-29432 [v2.10.3.13]:
    //   _tx_band == Band.B60M && current_region == FRSRegion.US && !extended
    //   → switch on CurrentDSPMode: allow USB / CWL / CWU / DIGU, reject all
    //     others.
    // Applied inline below when the matched range is Band60m for US. Thetis
    // gates US 60m TX on the broad 5.1-5.5 MHz range (IsOKToTX at
    // clsBandStackManager.cs:1063-1083 [v2.10.3.13]); channel centers in
    // kUsChannels60m are dial-convention markers for band-stack snap, NOT TX
    // gates. FCC 47 CFR 97.303(h) places the USB suppressed-carrier dial at
    // channel_center - 1.5 kHz, which is OUTSIDE a 2.8 kHz wide window
    // centred on Thetis's channel-center constants — channelizing the gate
    // there rejected the standard USB dial frequency (issue #271).
    //
    // Skip B60M rows ONLY when channelization was authoritative above
    // (ch60m.size > 0 — UK, Japan). For regions without channelization
    // (US, Australia, etc.) the B60M range row IS the authoritative source.
    const RangeSpan ranges = bandRangesFor(region);
    for (std::size_t i = 0; i < ranges.size; ++i) {
        if (ranges.data[i].band == Band::Band60m && ch60m.size > 0) {
            continue;
        }
        if (isInBandRange(freqHz, ranges.data[i])) {
            if (region == Region::UnitedStates &&
                ranges.data[i].band == Band::Band60m &&
                !isUs60mModeAllowed(mode))
            {
                return false;
            }
            return true;
        }
    }

    return false;
}

bool BandPlanGuard::isValidTxPassband(Region region, std::int64_t freqHz, DSPMode mode,
                                      int filterLowHz, int filterHighHz, bool extended,
                                      bool ignoreFilter) const noexcept
{
    // From Thetis console.cs:6778-6814 [v2.10.3.15], CheckValidTXFreq.
    if (extended) { return true; }
    //MW0LGE_21d filter outside band, ignore option
    const std::int64_t low = ignoreFilter ? 0 : filterLowHz;
    const std::int64_t high = ignoreFilter ? 0 : filterHighHz;
    if (mode == DSPMode::CWL || mode == DSPMode::CWU) {
        return isValidTxFreq(region, freqHz, mode, false);
    }
    // NereusSDR-native arithmetic guard: invalid wire/state values cannot wrap.
    const auto inRange = [&](std::int64_t offset) {
        if ((offset > 0 && freqHz > std::numeric_limits<std::int64_t>::max() - offset)
            || (offset < 0 && freqHz < std::numeric_limits<std::int64_t>::min() - offset)) {
            return false;
        }
        return isValidTxFreq(region, freqHz + offset, mode, false);
    };
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::DIGL:
    case DSPMode::USB:
    case DSPMode::DIGU:
    case DSPMode::DSB:
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::FM:
    case DSPMode::SPEC:
    // NereusSDR-native RADE modes use the TX chain's USB/LSB geometry.
    case DSPMode::RADE_U:
    case DSPMode::RADE_L:
        return low <= high && inRange(low) && inRange(high);
    case DSPMode::DRM:
        return low <= high && inRange(low - 12000) && inRange(high - 12000);
    default:
        return false;
    }
}

bool BandPlanGuard::isValidTxBand(Band rxBand, Band txBand,
                                  bool preventDifferentBand) const noexcept
{
    // From Thetis console.cs:29451-29465 [v2.10.3.15]
    //MW0LGE [2.9.0.7]
    if (!preventDifferentBand) {
        return true;
    }
    return rxBand == txBand;
}

// ---------------------------------------------------------------------------
// 3M-1b SSB-mode allow-list (NereusSDR-native)
// ---------------------------------------------------------------------------

bool BandPlanGuard::isModeAllowedForTx(DSPMode mode) const noexcept
{
    // 3M-1b ships SSB voice only. CW is 3M-2; AM/SAM/FM/DSB/DRM are 3M-3.
    // SPEC is never a TX mode.
    //
    // Phase 3R K-bench: RADE_U / RADE_L are digital voice modes that ride
    // on USB/LSB carriers (TxChannel::setTxMode maps to USB/LSB before
    // SetTXAMode per c1c1dce0). Band-plan etiquette is equivalent to
    // DIGU/DIGL.
    switch (mode) {
        case DSPMode::LSB:
        case DSPMode::USB:
        case DSPMode::DIGL:
        case DSPMode::DIGU:
        case DSPMode::RADE_U:
        case DSPMode::RADE_L:
        // AM / SAM / DSB TX: WDSP TXA ammod stage, run-gated by SetTXAMode
        // (TXA.c:753-789 [v2.10.3.13]).  TxChannel::applyTxFilterForMode
        // already maps these to a symmetric IQ bandpass and
        // TransmitModel::amCarrierLevel drives SetTXAAMCarrierLevel, so the
        // only gate that was still closed was this allow-list.
        case DSPMode::AM:
        case DSPMode::SAM:
        case DSPMode::DSB:
            return true;
        default:
            return false;
    }
}

BandPlanGuard::MoxCheckResult
BandPlanGuard::checkMoxAllowed(Region region, std::int64_t freqHz,
                                DSPMode mode, Band rxBand, Band txBand,
                                bool preventDifferentBand,
                                bool extended, int filterLowHz, int filterHighHz,
                                bool ignoreFilter) const noexcept
{
    // Mode check first — cheaper and more directly user-facing.
    if (!isModeAllowedForTx(mode)) {
        QString reason;
        switch (mode) {
            // R-R3-17 / R-R3-21: user words. CW transmit is planned work
            // (Phase 3M-2); FM transmit waits on pre-emphasis (Phase 3M-3b).
            // DRM has its own sentence, so a DRM refusal names DRM.
            case DSPMode::CWL:
            case DSPMode::CWU:
                reason = QStringLiteral("CW transmit is not available on this Core");
                break;
            case DSPMode::FM:
                reason = QStringLiteral("FM transmit is not available on this Core");
                break;
            case DSPMode::DRM:
                reason = QStringLiteral("DRM transmit is not available on this Core");
                break;
            default:
                reason = QStringLiteral("This mode cannot transmit.");
                break;
        }
        return {false, reason};
    }

    // Band-mismatch check. Thetis runs it before the US 60 m mode check and
    // the band edges, so a key on another band refuses for that first.
    // From Thetis console.cs:29451-29465 [v2.10.3.15]
    //MW0LGE [2.9.0.7]
    //   if (_preventTXonDifferentBandToRXband && ((!RX2Enabled && VFOBTX && RX1Band != TXBand) || ...
    //   // note RX2 enabled with a TXvfoB will always TX
    // Thetis compares the split TX band with the RX band; NereusSDR has no
    // split, so rxBand is the band of the device's active slice when the
    // transmitting slice is not it, else the TX band
    // (RadioModel::installBandPlanMoxCheck picks it). The sentence names both.
    if (!isValidTxBand(rxBand, txBand, preventDifferentBand)) {
        return {false, QStringLiteral("Transmit would be on %1 while another slice you have open is on %2, and Setup "
                                      "is set to prevent transmitting on a different band.")
                           .arg(bandWords(txBand), bandWords(rxBand))};
    }

    // Addendum G-42 item 4: each refusal below says what is wrong, after
    // Thetis's MOX messages, in the operator's words.
    // From Thetis console.cs:29467-29484 [v2.10.3.15]:
    //   if (_tx_band == Band.B60M && current_region == FRSRegion.US && !extended)
    //   ... default: MessageBox.Show(... + " mode is not allowed on 60M band." ...
    if (!extended && region == Region::UnitedStates && txBand == Band::Band60m
        && !isUs60mModeAllowed(mode)) {
        return {false, QStringLiteral("%1 is not allowed on 60 m in the United States.")
                           .arg(modeWords(mode))};
    }

    // Frequency / band-edge check.
    if (!isValidTxPassband(region, freqHz, mode, filterLowHz, filterHighHz,
                           extended, ignoreFilter)) {
        // From Thetis console.cs:29486-29528 [v2.10.3.15]: the US 60 m
        // filter limit when the carrier itself may transmit; the carrier for
        // CW and TUNE; the carrier with the TX filter edges otherwise.
        //   if (_tx_band == Band.B60M && current_region == FRSRegion.US &&
        //       checkValidTXFreq_local(current_region, freq) && !extended)
        if (region == Region::UnitedStates && txBand == Band::Band60m
            && isValidTxFreq(region, freqHz, mode, false)) {
            return {false, QStringLiteral("The transmit filter is wider than the 2.8 kHz "
                                          "allowed on 60 m in the United States.")};
        }
        const bool carrierOnly = ignoreFilter || mode == DSPMode::CWL || mode == DSPMode::CWU
            || (filterLowHz == 0 && filterHighHz == 0);
        if (carrierOnly) {
            return {false, QStringLiteral("%1 MHz is outside the transmit bands for your "
                                          "region (%2).")
                               .arg(mhzWords(freqHz), regionWords(region))};
        }
        return {false, QStringLiteral("%1 MHz with the transmit filter from %2 to %3 Hz "
                                      "reaches outside the transmit bands for your region "
                                      "(%4).")
                           .arg(mhzWords(freqHz)).arg(filterLowHz).arg(filterHighHz)
                           .arg(regionWords(region))};
    }

    return {true, QString()};
}

} // namespace NereusSDR::safety
