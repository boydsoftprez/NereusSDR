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
#include <QRect>
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

    /// Read-backs for the fields driven by PanadapterApplet::
    /// updateStatusOverlay. Same reasoning as wideBpf() below: the setters
    /// were write-only, so nothing could assert that a pan painted its own
    /// slice rather than the construction-time placeholders, and the whole
    /// surface shipped unverified. Narrow accessors rather than exposing the
    /// widget to callers.
    qint64 frequencyHz() const { return m_frequencyHz; }
    QString mode() const { return m_mode; }
    int chainIndex() const { return m_chainIndex; }

    void setTxBound(bool tx);
    bool txBound() const { return m_txBound; }

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
    bool diversityActive() const { return m_diversityActive; }

    void setPsPaused(bool paused);
    bool psPaused() const { return m_psPaused; }

    /// The clickable badges, in the order paintEvent lays them out.
    enum class Badge { ChainTag, Tx, Wide };

    /// Hit region of `badge` in widget coordinates, or an invalid QRect when
    /// the badge is not currently clickable.
    ///
    /// The optional pills are laid out sequentially, so an unlit one occupies
    /// no space and is not hit-testable at all -- TX in particular only
    /// exists while this pan's slice already holds the transmitter.
    /// mousePressEvent resolves clicks through this same function, so the
    /// geometry a caller reads back is by construction the geometry that
    /// responds. Added for the badge-click wiring: the layout constants live
    /// in the .cpp, so without it a test could only guess coordinates and
    /// would silently start clicking empty background whenever the layout
    /// moved. Same reasoning as the wideBpf() / chainIndex() read-backs.
    ///
    /// Only the horizontal extent is meaningful; the region spans the full
    /// widget height, which is exactly what mousePressEvent tests.
    QRect badgeRect(Badge badge) const;

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
