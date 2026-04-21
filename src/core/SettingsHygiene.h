// =================================================================
// src/core/SettingsHygiene.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Validates persisted AppSettings (stored under
// hardware/<mac>/...) against the connected board's BoardCapabilities.
// Surfaces mismatches (e.g. persisted S-ATT value exceeds board range)
// as a QVector<Issue> that the Diagnostics → Radio Status page can
// display.
//
// No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include "BoardCapabilities.h"
#include <QObject>
#include <QString>
#include <QVector>

namespace NereusSDR {

// SettingsHygiene: validates AppSettings persisted under hardware/<mac>/...
// against the BoardCapabilities of the currently connected radio.
//
// Call validate() after each successful connect. The resulting issue list
// is available via issues() and is signalled via issuesChanged().
class SettingsHygiene : public QObject {
    Q_OBJECT
public:
    enum class Severity {
        Info,
        Warning,
        Critical
    };

    struct Issue {
        Severity severity;
        QString  key;          // AppSettings key (e.g. "hardware/AABBCC/sAtt")
        QString  summary;      // short label (e.g. "S-ATT setting clamped")
        QString  detail;       // full diagnostic sentence
        QString  fixActionId;  // optional — UI may trigger a fix-it action
    };

    explicit SettingsHygiene(QObject* parent = nullptr);

    // Run validation: reads AppSettings under hardware/<mac>/..., compares
    // each persisted value against caps, and builds the issue list.
    // Idempotent — clears previous results on each call.
    void validate(const QString& mac, const BoardCapabilities& caps);

    QVector<Issue> issues() const;
    int  issueCount() const;
    bool hasIssues() const;

    // Fix-it actions ───────────────────────────────────────────────────────

    // Reset all per-MAC hardware settings to safe defaults matching caps.
    // Emits issuesChanged() after the reset.
    void resetSettingsToDefaults(const QString& mac, const BoardCapabilities& caps);

    // Remove all AppSettings keys under hardware/<mac>/  (i.e. "forget radio").
    // Emits issuesChanged() after the purge.
    void forgetRadio(const QString& mac);

signals:
    void issuesChanged();

private:
    QVector<Issue> m_issues;

    // ── Individual validation rules ────────────────────────────────────────

    // S-ATT: persisted attenuator dB value exceeds caps.attenuator.maxDb.
    // no-port-check: NereusSDR-original rule.
    void checkStepAtt(const QString& mac, const BoardCapabilities& caps);

    // Saturn BPF1 settings persisted for a non-Saturn board.
    // no-port-check: NereusSDR-original rule.
    void checkSaturnBpf1(const QString& mac, const BoardCapabilities& caps);

    // N2ADR filter enabled but board is not HL2.
    // no-port-check: NereusSDR-original rule.
    void checkN2adrFilter(const QString& mac, const BoardCapabilities& caps);

    // Apollo settings persisted but caps.hasApollo = false.
    // no-port-check: NereusSDR-original rule.
    void checkApolloSettings(const QString& mac, const BoardCapabilities& caps);

    // Per-band Alex TX antenna unset (any band has no TX antenna assigned).
    // no-port-check: NereusSDR-original rule.
    void checkAlexAntenna(const QString& mac, const BoardCapabilities& caps);

    // Helper: emit a Warning that a persisted dB value exceeds the board range.
    void addAttClampIssue(const QString& mac, int persisted, int maxDb);
};

} // namespace NereusSDR
