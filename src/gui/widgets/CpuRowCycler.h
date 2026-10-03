// no-port-check: NereusSDR-original. Parity ruling C9: which computer's CPU
// the System tile's CPU row shows in a remote window.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/CpuRowCycler.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Thetis has one computer.
//
// In a remote window the System tile's CPU row shows this computer's CPU
// ("CPU n%") and the Core's ("Core n%"), each labelled. By default it
// cycles between them every kCycleMs; the row's right-click pins either.
// A reading above kHotPercent stops the cycle on it (drawn in the warning
// colour) until it drops back. With no Core reading the row shows this
// computer only, and its tooltip says why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - Created for parity ruling C9 by J.J. Boyd (KG4VCF).
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

class CpuRowCycler {
public:
    enum class Source { Cycle, ThisComputer, Core };
    static constexpr qint64 kCycleMs = 3000;
    static constexpr double kHotPercent = 80.0;

    struct Row {
        QString label;          // "CPU" (this computer) or "Core"
        double percent{0.0};
        bool core{false};       // the Core's reading
        bool warning{false};    // above kHotPercent
        QString toolTip;
    };

    void setSource(Source source);
    Source source() const { return m_source; }

    void setThisComputer(double percent);
    // nullopt: no Core reading; `unavailableReason` is the tooltip's words.
    void setCore(std::optional<double> percent, const QString& unavailableReason);

    // Moves the cycle on by `elapsedMs` (the CPU timer's period).
    void advance(qint64 elapsedMs);

    Row row() const;

    // AppSettings "CpuRowSource": "Cycle" (default), "ThisComputer", "Core".
    static QString sourceKey(Source source);
    static Source sourceFromKey(const QString& key);

private:
    Source m_source{Source::Cycle};
    double m_local{0.0};
    std::optional<double> m_core;
    QString m_coreReason;
    bool m_showingCore{false};
    qint64 m_sinceSwitchMs{0};
};

} // namespace NereusSDR
