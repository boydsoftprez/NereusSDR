#pragma once

// =================================================================
// src/gui/containers/FloatingContainer.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmMeterDisplay.cs
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include <QWidget>
#include <QString>

namespace NereusSDR {

class ContainerWidget;

class FloatingContainer : public QWidget {
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

    // From Thetis frmMeterDisplay.cs:114-139
    void onConsoleWindowStateChanged(Qt::WindowStates state, bool rx2Enabled);

    void saveGeometry();
    void restoreGeometry();

    // Ensure the form will be shown on a visible screen. Repositions the
    // form when (a) no geometry was saved, or (b) saved geometry lands at
    // the (0,0) origin or outside every connected screen. Pass the main
    // window (or any widget on the target screen) as anchor; the form is
    // centered on the anchor's screen. No-op when geometry is already on
    // a screen and not at (0,0).
    void ensureVisiblePosition(QWidget* anchor);

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
};

} // namespace NereusSDR
