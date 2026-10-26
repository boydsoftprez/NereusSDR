#pragma once

// =================================================================
// src/gui/containers/FloatingContainer.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/frmMeterDisplay.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - onMainWindowStateChanged replaces onConsoleWindowStateChanged:
//                 Minimizes and Hide when slice not open apply to every
//                 slice, not only the second receiver. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 - setScreenSource, sizeWithin, fittedSize and
//                 refreshDesktopLimit: a floating window is never wider or
//                 taller than all the screens together. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - ensureVisiblePosition reads the screens through a
//                 ScreenSource and says whether it moved the form, by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  frmMeterDisplay.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QWidget>
#include <QString>
#include "gui/NereusGuiExport.h"

namespace NereusSDR {

class ContainerWidget;
class ScreenSource;

class NEREUS_GUI_EXPORT FloatingContainer : public QWidget {
    Q_OBJECT

public:
    explicit FloatingContainer(int rxSource, QWidget* parent = nullptr);
    ~FloatingContainer() override;

    QString id() const { return m_id; }
    void setId(const QString& id);

    int rxSource() const { return m_rxSource; }

    // From Thetis frmMeterDisplay.cs:168-179
    void takeOwner(ContainerWidget* container);

    bool containerMinimises() const { return m_containerMinimises; }
    void setContainerMinimises(bool minimises);
    bool containerHidesWhenRxNotUsed() const { return m_containerHidesWhenRxNotUsed; }
    void setContainerHidesWhenRxNotUsed(bool hides);

    bool isFormEnabled() const { return m_formEnabled; }
    void setFormEnabled(bool enabled);

    bool isHiddenByMacro() const { return m_hiddenByMacro; }
    void setHiddenByMacro(bool hidden);

    bool isContainerFloating() const { return m_floating; }
    void setContainerFloating(bool floating);

    // From Thetis frmMeterDisplay.cs:114-139 [v2.10.3.15]
    // The main window was minimized or restored. A floating, enabled form
    // that minimizes hides with it, and on restore shows again unless a
    // macro hid it or its slice is not open while it hides then. Thetis
    // asks this only of RX2 (`rx2Enabled`); here `sliceOpen` is whether
    // the form's own slice is open, whichever slice that is.
    void onMainWindowStateChanged(Qt::WindowStates state, bool sliceOpen);

    void saveGeometry();
    void restoreGeometry();

    // Ensure the form will be shown on a visible screen. Repositions the
    // form when (a) no geometry was saved, or (b) saved geometry lands at
    // the (0,0) origin or outside every connected screen. Pass the main
    // window (or any widget on the target screen) as anchor; the form is
    // centered on the anchor's screen. No-op when geometry is already on
    // a screen and not at (0,0). The screens are read through `screens`
    // (the application's own when null). Returns true when the form was
    // moved, so the caller can keep it off another window's spot.
    bool ensureVisiblePosition(QWidget* anchor, const ScreenSource* screens = nullptr);
    static QRect clampedGeometry(const QRect& geometry, const QRect& available, const QSize& minimum);

    // The screens the size limit is read from (the application's own
    // when null). Sets the limit.
    void setScreenSource(const ScreenSource* screens);
    // `size`, no larger than `limit` and no smaller than `minimum` (the
    // minimum wins). Unbounded when `limit` is not valid.
    static QSize sizeWithin(const QSize& size, const QSize& limit, const QSize& minimum);
    // `wanted`, no wider and no taller than all the screens together and
    // no smaller than the form's minimum. A window stretched across
    // monitors keeps its size; a runaway or absurd saved size does not.
    QSize fittedSize(const QSize& wanted) const;
    // Sets the form's maximum size to all the screens together. Called
    // when the screens are set and whenever one comes, goes or changes.
    void refreshDesktopLimit();

signals:
    void aboutToClose();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Collapse the top-level window to title-bar height when the owner
    // ContainerWidget enters the minimised runtime state, restore the
    // previous height when it leaves. From Thetis frmMeterDisplay
    // minimise path (MeterManager.cs ContainerMinimised handler).
    void onOwnerMinimisedChanged(bool minimised);

private:
    void updateTitle();

    QString m_id;
    int m_rxSource{1};
    bool m_containerMinimises{true};
    bool m_containerHidesWhenRxNotUsed{true};
    bool m_formEnabled{true};
    bool m_floating{false};
    bool m_hiddenByMacro{false};
    int  m_unminimisedHeight{0};   // cached height before minimise collapse
    const ScreenSource* m_screens{nullptr};
};

} // namespace NereusSDR
