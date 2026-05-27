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
    int count() const { return m_pans.size(); }
    QString currentLayoutId() const { return m_currentLayoutId; }

    QString activePanId() const { return m_activePanId; }
    void setActivePan(const QString& panId);

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
