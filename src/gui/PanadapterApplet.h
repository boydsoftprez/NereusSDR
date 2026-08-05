// no-port-check: AetherSDR-derived NereusSDR file. Per-pan container
// (SpectrumWidget host, slice association) is adapted structurally from
// AetherSDR src/gui/PanadapterApplet.{h,cpp} [@0cd4559]. NereusSDR
// preserves the existing single-output-device + per-slice pan from its
// own audio model. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterApplet.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 1.
//                                    Per-pan container skeleton ported
//                                    structurally from AetherSDR
//                                    src/gui/PanadapterApplet.{h,cpp}
//                                    [@0cd4559]. Hosts one SpectrumWidget
//                                    + tracks associated slices for
//                                    overlay rendering. NereusSDR
//                                    preserves the existing single-
//                                    output-device + per-slice pan from
//                                    its own audio model; wideband-
//                                    extended-pan support follows Phase
//                                    3F design. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <QByteArrayList>
#include <QChar>
#include <QSet>

class QContextMenuEvent;
class QMenu;

namespace NereusSDR {

class SpectrumWidget;
class SliceModel;
class SpectrumStatusOverlay;

/// Container for a single panadapter view: spectrum + waterfall (via SpectrumWidget)
/// + associated slice overlays. AetherSDR overlay model: a pan picks one DDC for
/// FFT, any slice whose freq falls within visible range overlays as a flag.
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3
/// (Slice / Pan binding).
class PanadapterApplet : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString panId READ panId CONSTANT)
    Q_PROPERTY(int activeSliceIndex READ activeSliceIndex WRITE setActiveSliceIndex NOTIFY activeSliceChanged)

public:
    explicit PanadapterApplet(const QString& panId, QWidget* parent = nullptr);
    ~PanadapterApplet() override;

    QString panId() const { return m_panId; }
    SpectrumWidget* spectrumWidget() const { return m_spectrum; }

    /// Associate a slice (its flag will overlay when in visible range).
    void addSlice(int sliceIndex);
    void removeSlice(int sliceIndex);
    QSet<int> associatedSlices() const { return m_associatedSlices; }

    /// The "active" slice: receives tune/mode/filter commands from spectrum clicks.
    int activeSliceIndex() const { return m_activeSliceIndex; }
    void setActiveSliceIndex(int sliceIndex);

    /// Display state (client-side, persists via AppSettings)
    double centerMhz() const { return m_centerMhz; }
    void setCenterMhz(double mhz);
    double bandwidthMhz() const { return m_bandwidthMhz; }
    void setBandwidthMhz(double bw);

    /// Phase 3F Sub-Epic E Task 2: refresh per-pan overlay from active slice state.
    ///
    /// `chainIndex` is the ADC chain feeding the slice and must come from
    /// RadioModel::sliceChainIndex, which is also what RadioModel::
    /// panBypassState groups by. It is a parameter rather than something read
    /// off the slice because SliceModel::chainIndex() has no production
    /// writer: reading it painted "CH 0" on every pan and would have let the
    /// CH tag disagree with the WIDE pill sitting beside it. Pass -1 for a
    /// slice bound to no stream; the tag then holds its last value rather
    /// than claiming chain 0.
    void updateStatusOverlay(SliceModel* activeSlice, int chainIndex);

    /// The SliceModel properties updateStatusOverlay reads, by name.
    ///
    /// The overlay is rebuilt wholesale from the model, so it is correct only
    /// while every field it paints has a refresh trigger. MainWindow connects
    /// the NOTIFY signal of each property named here instead of listing
    /// individual signals at the call site, so a new overlay field means
    /// adding its property to this one list and nowhere else. Kept next to
    /// updateStatusOverlay because that is its only reader.
    ///
    /// tst_pan_status_overlay asserts every name resolves to a real
    /// SliceModel property with a NOTIFY signal, so an upstream rename fails
    /// a test rather than silently stranding the overlay.
    static QByteArrayList statusOverlaySliceProperties();

    /// Read-backs for the status overlay, mirroring wideBpf() / wideReason().
    /// The overlay was write-only, so nothing could assert a pan painted its
    /// own slice rather than the placeholders.
    QChar   statusSliceLetter() const;
    qint64  statusFrequencyHz() const;
    QString statusMode() const;
    int     statusChainIndex() const;

    /// Phase 3F: light (or clear) this pan's WIDE pill.
    /// A pan shows WIDE when the RX preselector chain feeding it is bypassed
    /// on the wire. The decision is per chain and is made by
    /// RadioModel::panBypassState; this is the forwarder that keeps
    /// MainWindow out of the pan's widget tree.
    void setWideBpf(bool wide, const QString& reason);
    bool wideBpf() const;
    QString wideReason() const;

    /// Phase 3F Sub-Epic F Task 13: per-pan Extended view toggle.
    /// Operator override of the zoom-driven auto-derive on SpectrumWidget.
    /// Default true (on); persisted per-pan via AppSettings under
    /// "Pan_<panId>_ExtendedView". When false, the embedded SpectrumWidget
    /// is held at extendedMode == false regardless of zoom (forced off);
    /// when true, the zoom auto-derive decides extendedMode dynamically.
    bool extendedViewEnabled() const { return m_extendedViewEnabled; }
    void setExtendedViewEnabled(bool on);

    /// Test-only: build and return the right-click context menu without
    /// exec()-ing it, so a test can find + trigger() its actions without a
    /// live nested event loop. Mirrors
    /// SMeterWidget::buildContextMenuForTesting() (tst_smeter_widget_context_menu.cpp).
    /// NereusSDR-native test seam.
    QMenu* buildContextMenuForTesting() { return buildContextMenu(this); }

signals:
    void activated(const QString& panId);  // emitted on any click within applet
    void closeRequested(const QString& panId);
    void activeSliceChanged(const QString& panId, int sliceIndex);

    // Phase 3F Sub-Epic E Task 2: forwarded from SpectrumStatusOverlay
    // so MainWindow can wire TX-arbiter handoff, FilterPolicyDialog,
    // and chain-swap menu (later tasks).
    void txBadgeClicked(const QString& panId);
    void wideBadgeClicked(const QString& panId);
    /// Carries the pan id as well as the chain the tag was showing. The id is
    /// what lets MainWindow resolve the chain the same way the WIDE pill
    /// beside it does -- live, through RadioModel::sliceChainIndex on this
    /// pan's active slice -- rather than trusting a second, cached answer
    /// that can disagree with the one on screen.
    void chainTagClicked(const QString& panId, int chainIdx);

    /// Task B5 (bottom-banner + pan-menu epic, design doc s8.5): both carry
    /// THIS applet's own panId(), never routed through PanadapterStack::
    /// activePanId(). "Add slice on this pan" / "Float this pan" used to
    /// live on the +PAN button's dropdown (Phase 3F Sub-Epic D Task 10,
    /// retired in Task B4), which sat nowhere near any pan, so "active pan"
    /// meant whichever pan happened to be active when the operator reached
    /// the far side of the window. A control drawn on a pan targets THAT
    /// pan.
    void addSliceRequested(const QString& panId);
    void floatRequested(const QString& panId);

protected:
    /// Right-align the status strip clear of the dBm scale strip. Re-run
    /// whenever a pill lights or goes dark: the strip's minimum width grows to
    /// fit its pills, and setGeometry clamps up to that minimum by expanding
    /// rightward, which walks the strip back under the dBm range arrows.
    void repositionStatusOverlay();
    void resizeEvent(QResizeEvent* event) override;
    /// Delegates to buildContextMenu(). Task B5 added the add-slice / float
    /// entries at the top of the menu; the pre-existing Extended-view
    /// toggle stays below a separator.
    void contextMenuEvent(QContextMenuEvent* event) override;
    /// Emits activated(panId) on any press in this pan, so clicking a pan makes
    /// it the active one. Watches the spectrum host and overlay too -- they take
    /// the press before this widget sees it. AetherSDR PanadapterApplet.cpp:628
    /// [@6a142807] does the same in its own eventFilter.
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    /// Build the right-click context menu: add-slice / float (Task B5, both
    /// carrying this applet's own panId()) then a separator then the
    /// pre-existing Extended-view toggle. Returns a heap-allocated QMenu*
    /// parented to `parent`; contextMenuEvent() exec()s it and
    /// deleteLater()s it, buildContextMenuForTesting() hands it to a test
    /// untouched. Mirrors SMeterWidget::buildContextMenu().
    QMenu* buildContextMenu(QObject* parent);

    QString                 m_panId;
    SpectrumWidget*         m_spectrum {nullptr};
    SpectrumStatusOverlay*  m_statusOverlay {nullptr};
    int                     m_activeSliceIndex {-1};
    QSet<int>               m_associatedSlices;
    double                  m_centerMhz {14.225};
    double                  m_bandwidthMhz {0.192};
    bool                    m_extendedViewEnabled {true};  // Phase 3F Sub-Epic F Task 13
};

} // namespace NereusSDR
