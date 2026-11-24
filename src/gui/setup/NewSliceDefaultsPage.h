#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/NewSliceDefaultsPage.h  (NereusSDR)
// =================================================================
//
// Setup > General > New Slice. The Core's choices for a slice the
// operator opens (+RX, Ctrl+R, or Add slice on the pan).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-24 - The operator's defaults for a slice they open.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                xAI Grok.
// =================================================================

#include "gui/SetupPage.h"
#include "gui/NereusGuiExport.h"

class QCheckBox;
class QComboBox;
class QShowEvent;
class QSpinBox;
class QWidget;

namespace NereusSDR {

class NEREUS_GUI_EXPORT NewSliceDefaultsPage : public SetupPage {
    Q_OBJECT
public:
    explicit NewSliceDefaultsPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    QComboBox* addChoice(const QString& label, const QString& objectName);
    void addItem(QComboBox* box, const QString& label, int value);
    void select(QComboBox* box, int value);
    void reload();
    void save();
    void installNrDetails();
    void syncNrDetails();
    void showNrDetails();

    QComboBox* m_band{nullptr};
    QComboBox* m_mode{nullptr};
    QComboBox* m_filter{nullptr};
    QComboBox* m_agcMode{nullptr};
    QComboBox* m_autoAgc{nullptr};
    QComboBox* m_agcOffsetChoice{nullptr};
    QSpinBox*  m_agcOffset{nullptr};
    QComboBox* m_agcTChoice{nullptr};
    QSpinBox*  m_agcT{nullptr};
    QComboBox* m_step{nullptr};
    QComboBox* m_volumeChoice{nullptr};
    QSpinBox*  m_volume{nullptr};
    QComboBox* m_nr{nullptr};
    QCheckBox* m_nrCustomize{nullptr};
    QWidget*   m_nrDetails{nullptr};
    QComboBox* m_anf{nullptr};
    QComboBox* m_nb{nullptr};
    QComboBox* m_snb{nullptr};
    QComboBox* m_squelch{nullptr};
    QComboBox* m_lock{nullptr};
    QComboBox* m_mute{nullptr};
    QComboBox* m_rit{nullptr};
    QComboBox* m_xit{nullptr};

    bool m_reloading{false};
};

} // namespace NereusSDR
