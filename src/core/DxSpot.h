// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DX spot value type
//
// Ported from AetherSDR src/core/DxClusterClient.h:13-23 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B1. Extracted DxSpot
//                                    from AetherSDR's DxClusterClient.h
//                                    so that multiple spot-ingest clients
//                                    (SpotCollector, RBN, WSJT-X, POTA,
//                                    FreeDV, PSK) can share the type
//                                    without pulling in DX-cluster code.
//                                    Added "source" comment to list the
//                                    full set of expected source labels.
//                                    AI tooling: Anthropic Claude Code.
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B4. Added
//                                    Q_DECLARE_METATYPE so DxSpot can flow
//                                    through QSignalSpy in the
//                                    tst_wsjtx_decoder test (the WSJT-X
//                                    parser tests are the first ones that
//                                    spy on `spotReceived(DxSpot)` rather
//                                    than calling a parser seam
//                                    synchronously).

#pragma once

#include <QMetaType>
#include <QString>
#include <QTime>

namespace NereusSDR {

// From AetherSDR src/core/DxClusterClient.h:13-23 [@0cd4559]
struct DxSpot {
    QString spotterCall;     // W3LPL
    double  freqMhz{0.0};    // 14.025 (converted from kHz)
    QString dxCall;          // JA1ABC
    QString comment;         // "CW big signal"
    QTime   utcTime;         // 18:24 UTC
    QString source;          // "Cluster", "RBN", "WSJT-X", "SpotCollector", "POTA", "FreeDV", "PSK"
    QString color;           // #AARRGGBB for radio spot color (optional)
    int     snr{0};          // signal-to-noise ratio (dB)
    int     lifetimeSec{0};  // 0 = use source default from AppSettings
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::DxSpot)
