#pragma once

// no-port-check: NereusSDR-native display formatting approved by J.J. Boyd
// (KG4VCF) on October 1, 2026. Numeric step values remain in Hz and the
// upstream tune-step table is unchanged.
// Modification history (NereusSDR):
//   2026-10-01: Added compact STEP labels by J.J. Boyd (KG4VCF),
//                 with AI assistance via OpenAI Codex.

#include <QString>

namespace NereusSDR {

inline QString formatTuneStepLabel(int hz)
{
    if (hz < 1000) {
        return QStringLiteral("%1 Hz").arg(hz);
    }
    const bool megahertz = hz >= 1000000;
    QString value = QString::number(hz / (megahertz ? 1.0e6 : 1.0e3),
                                    'f', megahertz ? 6 : 3);
    while (value.endsWith(QLatin1Char('0'))) {
        value.chop(1);
    }
    if (value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    return value + (megahertz ? QStringLiteral(" MHz") : QStringLiteral(" kHz"));
}

} // namespace NereusSDR
