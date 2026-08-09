// no-port-check: AetherSDR-derived NereusSDR file. Pan layout manager
// (9-template QSplitter tree, active-pan tracking, float-pan signal) is
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
//   2026-08-08  J.J. Boyd / KG4VCF  Three bench-reported defects.
//                                    (1) Splitter handles were left at
//                                    the style metric, so the drag
//                                    target between two pans was a few
//                                    logical px; every splitter now goes
//                                    through makeSplitter() at
//                                    kSplitterHandleWidth and is
//                                    non-collapsible. (2) A floated pan
//                                    stopped rendering: float / dock now
//                                    tear the SpectrumWidget's render
//                                    context down before the reparent
//                                    and re-realize it after, ported
//                                    from AetherSDR
//                                    src/gui/PanadapterStack.cpp:22-43,
//                                    785-860 [@1e0718ad]. (3) dockPanadapter
//                                    becomes a named slot so the window
//                                    close box and the pan's own Dock
//                                    button share one path. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
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

/// 9-template pan layout manager. Templates: "1", "2v", "2h", "12h", "2h1",
/// "3v", "2x2", "4v", "3h2" (design doc
/// 2026-08-02-bottom-banner-and-pan-menu-design.md §8.3, which grew this
/// from the original 5).
/// Ported structurally from AetherSDR PanadapterStack (12 templates; we use 9 of them).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §11.
class PanadapterStack : public QWidget {
    Q_OBJECT
public:
    explicit PanadapterStack(QWidget* parent = nullptr);
    ~PanadapterStack() override;

    /// Width of every splitter handle in the pan tree, in logical pixels.
    ///
    /// Bench report 2026-08-08: "the mouse over area for resizing the space
    /// between two pans is so small it is hard to hit". A QSplitter's drag
    /// target is exactly its handle rect -- there is no separate hit area to
    /// widen -- and nothing here ever called setHandleWidth, so the handles
    /// fell back to the style metric (a few logical px, i.e. a couple of
    /// physical mm on a Retina panel).
    ///
    /// 8 rather than AetherSDR's 3 (PanadapterStack.cpp:86 [@1e0718ad])
    /// because AetherSDR pairs its 3 px handle with a themed 2 px painted
    /// line so the operator can at least SEE where to aim; ours was an
    /// unpainted gap. PanSplitterHandle keeps the thin painted look at this
    /// wider grab size, so the visual weight is unchanged and only the
    /// target grows.
    static constexpr int kSplitterHandleWidth = 8;

    PanadapterApplet* addPanadapter(const QString& panId);
    void removePanadapter(const QString& panId);
    void removeAll();

    /// Apply one of the 9 templates: "1", "2v", "2h", "12h", "2h1", "3v",
    /// "2x2", "4v", "3h2"
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

    /// Bring a floated pan back into the splitter tree.
    ///
    /// A named slot rather than the lambda this used to be, because there are
    /// now two ways back -- the window's close box and the Dock button on the
    /// pan's floating title strip -- and both have to run the same
    /// teardown-before-reparent sequence. Public so a test can drive the
    /// round trip without a window manager.
    void dockPanadapter(const QString& panId);

    PanFloatingWindow* floatingWindowForTest(const QString& panId) const
    {
        return m_floating.value(panId, nullptr);
    }

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
    /// Returns the ids of the pans that came back from a floating window,
    /// so the caller can re-realize exactly those render contexts.
    QStringList dockAllFloatingPans();
    void clearSplitters();

    /// Single construction point for every splitter in the pan tree.
    ///
    /// A factory rather than setHandleWidth at each of the 11 call sites: the
    /// grab-area defect was one forgotten setter, and the multi-row layouts
    /// (12h / 2x2 / 2h1 / 3h2) each build their own nested row splitters, so
    /// per-site setup is a defect waiting for the tenth layout. Routing every
    /// construction through here means a new layout cannot be born
    /// ungrabbable.
    QSplitter* makeSplitter(Qt::Orientation orientation, QWidget* parent);

    /// Re-realize the render contexts of the named pans, which have just
    /// returned from a floating window. Deliberately not "every docked pan":
    /// a widget whose top-level never changed must not be touched.
    void refreshReturnedFromFloat(const QStringList& panIds);

    QSplitter*                                 m_rootSplitter {nullptr};
    QMap<QString, PanadapterApplet*>           m_pans;
    QMap<QString, PanFloatingWindow*>          m_floating;
    QString                                    m_currentLayoutId {"1"};
    QString                                    m_activePanId;
};

} // namespace NereusSDR
