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
// not a QPushButton tree) for performance. Shows the CH N tag and
// optional pills for TX, WIDE BPF, DIV (diversity), and PS HOLD,
// with an optional second row for remote display status. Hit-tests in
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
//   2026-09-23  J.J. Boyd / KG4VCF  Remote display row paints the longest
//                                    form of its short line that fits,
//                                    never an elided one (R-R3-37).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 78 (R-IOS-02,
//                R-IOS-30): the TX pill offers Take transmit while another
//                device holds it. AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include "gui/PanStatusText.h"

#include <QWidget>
#include <QChar>
#include <QRect>
#include <QString>
#include <QStringList>

namespace NereusSDR {

/// Top-right per-pan overlay widget. Mirror of SpectrumOverlayPanel pattern.
/// Shows CH N, TX/WIDE/DIV/PS HOLD pills and optional remote display status.
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

    /// iPhone app plan Task 78 (the several-devices design, section 12
    /// item 2): another device (or the radio's own PTT) holds transmit and
    /// this window can take it. While this pan's slice is not the TX
    /// slice, the TX pill reads TAKE TX (outlined red while `holderOnAir`)
    /// and a click emits takeTransmitClicked instead of txBadgeClicked.
    void setTakeTransmitOffered(bool offered, const QString& holderName, bool holderOnAir);
    bool takeTransmitOffered() const { return m_takeOffered && !m_txBound; }
    /// The pill's hover sentence while it offers a take, else empty.
    QString takeTransmitToolTip() const;

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

    /// Remote display observation, distinct from the radio's RF/status pills
    /// (R-R3-37). The short line is painted on a second row; empty hides the
    /// row. The explanation is the hover text.
    void setRemoteDisplayStatus(const PanStatusText& status);
    /// The longest form of the short line.
    QString remoteDisplayStatus() const { return m_remoteDisplayStatus; }
    /// Every form of the short line, longest first.
    QStringList remoteDisplayForms() const { return m_remoteDisplayForms; }
    QString remoteDisplayExplanation() const { return m_remoteDisplayExplanation; }

    /// The form the second row paints at the current width: the longest
    /// that fits, measured in the row's font, never elided (the shortest
    /// when none fits). paintEvent draws exactly this, so a test can read
    /// back which form a narrow pan shows.
    QString visibleRemoteDisplayStatus() const;

    /// The second row's width for text, and the width `text` takes in the
    /// row's font rounded up: the two numbers visibleRemoteDisplayStatus
    /// compares, so a test can show the painted form fits.
    int remoteStatusRowWidth() const;
    int remoteStatusTextWidth(const QString& text) const;

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
    /// The region covers the badge row only; the remote display status row
    /// is observational and cannot activate a radio command.
    QRect badgeRect(Badge badge) const;

signals:
    void txBadgeClicked();
    /// Task 78: the TAKE TX pill was clicked.
    void takeTransmitClicked();
    void wideBadgeClicked();
    void chainTagClicked(int chainIdx);

public:
    /// Valid size for the strip, so a parent can place it.
    ///
    /// Without this, QWidget::sizeHint() returns QSize(-1, -1) for a widget
    /// with no layout. PanadapterApplet::resizeEvent positions the strip as
    /// `width() - hint.width() - 8`, so a -1 hint put it 7px from the right
    /// edge with all but a sliver of its 246px hanging off the pan -- which is
    /// why the status strip, and with it the WIDE badge, appeared to be
    /// missing entirely. Bench-caught 2026-07-26.
    ///
    /// Width derives from current pill flags and remote status text before
    /// painting, so a newly lit pill is placed correctly on its first frame.
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QChar    m_sliceLetter {'A'};
    qint64   m_frequencyHz {0};
    QString  m_mode {QStringLiteral("USB")};
    int      m_chainIndex {0};
    bool     m_txBound {false};
    bool     m_takeOffered {false};
    QString  m_takeHolderName;
    bool     m_takeHolderOnAir {false};
    bool     txPillLit() const { return m_txBound || m_takeOffered; }
    bool     m_wideBpf {false};
    QString  m_wideReason;
    bool     m_diversityActive {false};
    bool     m_psPaused {false};
    QString  m_remoteDisplayStatus;
    QStringList m_remoteDisplayForms;
    QString  m_remoteDisplayExplanation;
    void updateStatusToolTip();
    QRect remoteStatusRect() const;
};

} // namespace NereusSDR
