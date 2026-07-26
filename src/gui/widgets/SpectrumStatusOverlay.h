// no-port-check: NereusSDR-original. No upstream port. Top-right
// per-pan overlay widget for the Phase 3F multi-slice UI atlas; see
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/SpectrumStatusOverlay.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Top-right per-pan overlay
// widget for Phase 3F multi-slice UI atlas. Paint-based (QPainter,
// not a QPushButton tree) for performance. Shows the slice letter
// badge, frequency.kHz + mode text, CH N tag, and optional pills
// for TX, WIDE BPF, DIV (diversity), and PS HOLD. Hit-tests in
// mousePressEvent emit txBadgeClicked / wideBadgeClicked /
// chainTagClicked signals for parent consumption.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic E Task 1.
//                                    Created in C++20/Qt6 for NereusSDR;
//                                    NereusSDR-original widget, no
//                                    upstream port. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================
#pragma once

#include <QWidget>
#include <QChar>
#include <QString>

namespace NereusSDR {

/// Top-right per-pan overlay widget. Mirror of SpectrumOverlayPanel pattern.
/// Shows: slice letter badge, freq, mode, CH N tag, TX/WIDE/DIV/PS HOLD pills.
/// Click WIDE -> opens FilterPolicyDialog (parent-wired). Click TX -> requests
/// TxSliceArbiter handoff (parent-wired). Click CH tag -> chain swap menu
/// (parent-wired).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
/// section 11 (UI Atlas Surfaces).
class SpectrumStatusOverlay : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumStatusOverlay(QWidget* parent = nullptr);
    ~SpectrumStatusOverlay() override;

    void setSliceLetter(QChar letter);
    QChar sliceLetter() const { return m_sliceLetter; }

    void setFrequencyHz(qint64 hz);
    void setMode(const QString& mode);
    void setChainIndex(int chainIdx);

    void setTxBound(bool tx);

    /// Light (or clear) the WIDE pill. `reason` is the operator-facing
    /// sentence naming the cause of the bypass; it becomes this overlay's
    /// tooltip while the pill is lit, and is cleared with it. Composed by
    /// RadioModel::panBypassState, wording per design doc §16.4.4.
    void setWideBpf(bool wide, const QString& reason);

    /// Narrow accessors for the WIDE pill. The pill state was write-only
    /// until Phase 3F wired it, so nothing could assert that it lit and the
    /// badge shipped inspection-only. Mirrors the existing sliceLetter()
    /// accessor rather than exposing the whole widget to callers.
    bool wideBpf() const { return m_wideBpf; }
    QString wideReason() const { return m_wideReason; }

    void setDiversityActive(bool div);
    void setPsPaused(bool paused);

signals:
    void txBadgeClicked();
    void wideBadgeClicked();
    void chainTagClicked(int chainIdx);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QChar    m_sliceLetter {'A'};
    qint64   m_frequencyHz {0};
    QString  m_mode {QStringLiteral("USB")};
    int      m_chainIndex {0};
    bool     m_txBound {false};
    bool     m_wideBpf {false};
    QString  m_wideReason;
    bool     m_diversityActive {false};
    bool     m_psPaused {false};
};

} // namespace NereusSDR
