// no-port-check: AetherSDR-derived NereusSDR file. Pan layout manager
// (5-template QSplitter tree, active-pan tracking, float-pan signal) is
// adapted structurally from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterStack.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 3.
//                                    Layout manager skeleton ported
//                                    structurally from AetherSDR
//                                    src/gui/PanadapterStack.{h,cpp}
//                                    [@0cd4559]. Default 'Single'
//                                    layout constructed in ctor;
//                                    addPanadapter / removePanadapter /
//                                    activePan tracking live.
//                                    applyLayout / floatPanadapter /
//                                    rebuildSplitters stubbed for
//                                    Tasks 4-8. AetherSDR ships 12
//                                    templates; NereusSDR uses 5
//                                    (1 / 2v / 2h / 12h / 2x2) per
//                                    Phase 3F design. AI-assisted
//                                    transformation via Anthropic
//                                    Claude Code.
// =================================================================
#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QMap>

class QSplitter;

namespace NereusSDR {

class PanadapterApplet;
class PanFloatingWindow;
class SpectrumWidget;

/// 5-template pan layout manager. Templates: "1", "2v", "2h", "12h", "2x2".
/// Ported structurally from AetherSDR PanadapterStack (12 templates; we use 5 of them).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §11.
class PanadapterStack : public QWidget {
    Q_OBJECT
public:
    explicit PanadapterStack(QWidget* parent = nullptr);
    ~PanadapterStack() override;

    PanadapterApplet* addPanadapter(const QString& panId);
    void removePanadapter(const QString& panId);
    void removeAll();

    /// Apply one of the 5 templates: "1", "2v", "2h", "12h", "2x2"
    void applyLayout(const QString& layoutId, const QStringList& panIds);

    PanadapterApplet* panadapter(const QString& panId) const;
    QList<PanadapterApplet*> allApplets() const;

    /// Convenience: the SpectrumWidget hosted by pan `panId`, or nullptr if no
    /// such pan exists. Mirrors AetherSDR's PanadapterStack::spectrum(panId)
    /// so MainWindow::spectrumForSlice reads as a single lookup.
    SpectrumWidget* spectrum(const QString& panId) const;
    int count() const { return m_pans.size(); }
    QString currentLayoutId() const { return m_currentLayoutId; }

    QString activePanId() const { return m_activePanId; }
    void setActivePan(const QString& panId);

    /// Point the pan that hosts this slice at it, and leave every other pan
    /// alone.
    ///
    /// Bench report 2026-07-28, two slices on one pan: "when I click to tune
    /// it always tunes flag A, not the last selected." There are two
    /// independent notions of "active slice" -- RadioModel::activeSlice()
    /// (global) and PanadapterApplet::activeSliceIndex() (per pan) -- and
    /// only the global one had a writer once the pan was seeded.
    /// PanadapterApplet::addSlice sets the pan's value exactly once, when the
    /// pan has none, so a pan latched onto the first slice added to it for the
    /// session. MainWindow::sliceForPan resolves the per-pan value, so
    /// click-to-tune, the filter-edge drag, the CH tag and the pan TX pill all
    /// kept acting on that first slice however many times the operator
    /// selected another flag. This is the writer that was missing.
    ///
    /// Takes a slice ID (see RadioModel::sliceById), matching what
    /// PanadapterApplet::activeSliceIndex() and associatedSlices() hold.
    ///
    /// Deliberately only the HOSTING pan: the standing project rule is that a
    /// control drawn on a pan acts on that pan, so retargeting every pan at
    /// the globally active slice would be the same defect wearing the other
    /// hat -- pan-1's click-to-tune would jump to a slice pan-1 does not even
    /// show. A slice no pan hosts moves nothing, so a pan is never left
    /// pointing at something it cannot display.
    void setActiveSliceOnHostingPan(int sliceId);

    /// Re-home a slice from whichever pan(s) list it onto `destPanId`.
    ///
    /// The association is what setActiveSliceOnHostingPan and
    /// MainWindow::sliceForPan both key off, so it has to survive a slice
    /// changing pans. The SliceModel::panKeyChanged handler moved the
    /// VfoWidget and nothing else, which left the old pan still listing the
    /// slice in associatedSlices() -- and still able to hold it as that pan's
    /// active slice, i.e. a pan tuning and painting the CH tag for a slice it
    /// no longer hosts.
    ///
    /// PanadapterApplet::removeSlice does the re-pick on the pan being left
    /// (promoting a co-hosted slice, or -1 when that was the last one), so
    /// nothing here reproduces it.
    ///
    /// An unknown destination leaves every association untouched rather than
    /// detaching the slice from the pan it is currently on: a slice hosted
    /// nowhere has no click-to-tune at all, which is worse than one hosted by
    /// the pan it started on.
    void moveSliceToPan(int sliceId, const QString& destPanId);

    /// Detach a pan into a top-level PanFloatingWindow.
    void floatPanadapter(const QString& panId);

    /// Phase 3F Sub-Epic D Task 6: persist splitter geometry across launches.
    /// Keyed under AppSettings "PanSplitter0Sizes" + "PanLayoutId" (and per-row
    /// children for 12h / 2x2). Restore loads the layout AND the sizes.
    void saveSplitterState();
    void restoreSplitterState();

    /// Phase 3F Sub-Epic D Task 6 test seam: read the root splitter's current
    /// sizes (used by tests and saveSplitterState).
    QList<int> rootSplitterSizes() const;

    /// Phase 3F Sub-Epic D Task 6 test seam: drive the root splitter to a
    /// known size pattern so the persistence round-trip can be asserted.
    void rootSplitterSetSizesForTest(const QList<int>& sizes);

signals:
    void activePanChanged(const QString& panId);
    void countChanged(int count);

private:
    void rebuildSplitters(const QString& layoutId, const QStringList& panIds);
    void clearSplitters();

    QSplitter*                                 m_rootSplitter {nullptr};
    QMap<QString, PanadapterApplet*>           m_pans;
    QMap<QString, PanFloatingWindow*>          m_floating;
    QString                                    m_currentLayoutId {"1"};
    QString                                    m_activePanId;
};

} // namespace NereusSDR
