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
#include <QSet>

class QContextMenuEvent;

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
    void updateStatusOverlay(SliceModel* activeSlice);

    /// Phase 3F Sub-Epic F Task 13: per-pan Extended view toggle.
    /// Operator override of the zoom-driven auto-derive on SpectrumWidget.
    /// Default true (on); persisted per-pan via AppSettings under
    /// "Pan_<panId>_ExtendedView". When false, the embedded SpectrumWidget
    /// is held at extendedMode == false regardless of zoom (forced off);
    /// when true, the zoom auto-derive decides extendedMode dynamically.
    bool extendedViewEnabled() const { return m_extendedViewEnabled; }
    void setExtendedViewEnabled(bool on);

signals:
    void activated(const QString& panId);  // emitted on any click within applet
    void closeRequested(const QString& panId);
    void activeSliceChanged(const QString& panId, int sliceIndex);

    // Phase 3F Sub-Epic E Task 2: forwarded from SpectrumStatusOverlay
    // so MainWindow can wire TX-arbiter handoff, FilterPolicyDialog,
    // and chain-swap menu (later tasks).
    void txBadgeClicked(const QString& panId);
    void wideBadgeClicked(const QString& panId);
    void chainTagClicked(int chainIdx);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
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
