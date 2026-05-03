// =================================================================
// src/gui/setup/hardware/PaCalibrationGroup.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs
//     (ud10PA1W..ud10PA10W, ud100PA10W..ud100PA100W, ud200PA20W..ud200PA200W
//      spinbox blocks driving CalibratedPAPower lookup -- lines 5404-5594),
//      original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs
//     (CalibratedPAPower + PowerKernel -- per-board PA forward-power
//      calibration -- lines 6691-6768),
//      original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 -- Original implementation. Per-board PA forward-power
//                  cal-point spinbox group; populates ud{10|100|200}PA{N}W
//                  equivalent based on PaCalBoardClass. Section 3.3 of
//                  P1 full-parity epic. J.J. Boyd (KG4VCF), with
//                  AI-assisted transformation via Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

// --- From console.cs ---

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "PaCalibrationGroup.h"

#include "core/CalibrationController.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>

#include <cstddef>

namespace NereusSDR {

namespace {

// Per-class spinbox range/step. Mirrors the per-board ud{10|100|200}PA{N}W
// blocks in setup.cs:5404-5594 [v2.10.3.13]. HL2 maps to Anan10 per mi0bot
// setup.cs:5463-5466 [v2.10.3.13-beta2] (HL2 grouped with ANAN10/ANAN10E
// for PA cal: shared ud10PA1W..ud10PA10W spinbox set).
struct ClassSpec {
    double  rangeMax;
    double  step;
    int     decimals;
};

ClassSpec specFor(PaCalBoardClass cls) noexcept
{
    switch (cls) {
        case PaCalBoardClass::Anan10:     return {15.0,  0.1,  2};
        case PaCalBoardClass::Anan100:    return {150.0, 1.0,  2};
        case PaCalBoardClass::Anan8000:   return {300.0, 1.0,  2};
        case PaCalBoardClass::None:
        default:                          return {0.0,   0.0,  0};
    }
}

// Format the spinbox label as "<value> W" with trailing zeros stripped:
// "0.5 W" / "10 W" / "100 W" -- not "0.500 W" / "10.0 W". Uses 'g' format
// with 4 significant digits (max value 200 in production = 3 sig figs).
QString labelForLabelValue(double v)
{
    return QStringLiteral("%1 W").arg(QString::number(v, 'g', 4));
}

}  // namespace

PaCalibrationGroup::PaCalibrationGroup(QWidget* parent)
    : QGroupBox(tr("PA Forward Power Calibration"), parent)
{
    // Layout is created lazily inside rebuildLayout() so the QFormLayout
    // can be torn down and replaced when the board class changes.
    auto* form = new QFormLayout(this);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(4);
}

void PaCalibrationGroup::populate(CalibrationController* controller,
                                  PaCalBoardClass boardClass)
{
    // Disconnect prior controller bindings -- safe even if QMetaObject::Connection
    // is default-constructed (disconnect() returns false).
    if (m_controller) {
        QObject::disconnect(m_pointConn);
        QObject::disconnect(m_profileConn);
    }

    m_controller = controller;
    m_boardClass = boardClass;

    rebuildLayout(boardClass);

    if (boardClass == PaCalBoardClass::None) {
        setVisible(false);
        return;
    }

    setVisible(true);

    if (m_controller) {
        // Source: CalibrationController::paCalPointChanged (P1 full-parity
        // §3.2). External writes (setPaCalPoint via the controller's setter
        // path) propagate back into our spinbox values, with the echo guard
        // preventing the spinbox edit from re-entering setPaCalPoint.
        m_pointConn = QObject::connect(
            m_controller, &CalibrationController::paCalPointChanged,
            this, [this](int idx, float watts) {
                if (idx < 1 || idx > 10) { return; }
                if (!m_spins[static_cast<std::size_t>(idx)]) { return; }
                m_updatingFromController = true;
                {
                    QSignalBlocker blocker(m_spins[static_cast<std::size_t>(idx)]);
                    m_spins[static_cast<std::size_t>(idx)]->setValue(static_cast<double>(watts));
                }
                m_updatingFromController = false;
            });

        // Wholesale profile swap (e.g. radio reconnect with persisted state)
        // re-syncs the entire spinbox row from the controller.
        m_profileConn = QObject::connect(
            m_controller, &CalibrationController::paCalProfileChanged,
            this, [this]() { syncFromProfile(); });

        syncFromProfile();
    }
}

void PaCalibrationGroup::rebuildLayout(PaCalBoardClass cls)
{
    // Tear down existing widgets. We hold parent ownership so deleteLater()
    // would also work, but explicit delete keeps the array slots clean
    // immediately so spinBoxCountForTest() is correct after rebuild.
    for (std::size_t i = 0; i < m_spins.size(); ++i) {
        if (m_spins[i]) {
            delete m_spins[i];
            m_spins[i] = nullptr;
        }
        if (m_labels[i]) {
            delete m_labels[i];
            m_labels[i] = nullptr;
        }
    }

    if (cls == PaCalBoardClass::None) {
        return;
    }

    auto* form = qobject_cast<QFormLayout*>(layout());
    if (!form) {
        // Defensive -- ctor installs a QFormLayout, so this branch should
        // never fire in practice. If layout was replaced externally, abort
        // rather than crash.
        return;
    }

    const ClassSpec spec = specFor(cls);
    PaCalProfile defaults = PaCalProfile::defaults(cls);

    for (int i = 1; i <= 10; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        const double labelValue = static_cast<double>(defaults.watts[idx]);

        // Source: Thetis setup.cs:5404-5594 ud{10|100|200}PA{N}W [v2.10.3.13]
        //   -- spinbox initial Value defaults to its label (e.g. ud100PA50W
        //   defaults to 50.0). The CalibrationController seeds the profile
        //   from PaCalProfile::defaults() on first connect, so the spinbox
        //   value mirrors the label until the user calibrates.
        auto* label = new QLabel(labelForLabelValue(labelValue), this);
        auto* spin  = new QDoubleSpinBox(this);
        spin->setRange(0.0, spec.rangeMax);
        spin->setSingleStep(spec.step);
        spin->setDecimals(spec.decimals);
        spin->setValue(labelValue);
        spin->setSuffix(tr(" W"));

        m_labels[idx] = label;
        m_spins[idx]  = spin;

        form->addRow(label, spin);

        QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         this, [this, i](double v) { onSpinChanged(i, v); });
    }
}

void PaCalibrationGroup::onSpinChanged(int idx, double v)
{
    if (m_updatingFromController) { return; }   // echo guard
    if (!m_controller) { return; }
    m_controller->setPaCalPoint(idx, static_cast<float>(v));
    // Persist immediately. Mirrors CalibrationTab's groups 1-4 pattern --
    // every UI-driven setter call is followed by an explicit save(); the
    // model-layer setters intentionally do NOT auto-save (see
    // CalibrationController::setLevelOffsetDb / setFreqCorrectionFactor
    // which only emit changed()). Pre-Phase-3A migration this lived in the
    // CalibrationTab spinbox lambda; lost during the move to PaWattMeterPage
    // and surfaced by Codex review on PR #165.
    m_controller->save();
}

void PaCalibrationGroup::syncFromProfile()
{
    if (!m_controller) { return; }

    const PaCalProfile profile = m_controller->paCalProfile();

    m_updatingFromController = true;
    for (int i = 1; i <= 10; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        if (!m_spins[idx]) { continue; }
        QSignalBlocker blocker(m_spins[idx]);
        m_spins[idx]->setValue(static_cast<double>(profile.watts[idx]));
    }
    m_updatingFromController = false;
}

#ifdef NEREUS_BUILD_TESTS

int PaCalibrationGroup::spinBoxCountForTest() const
{
    int count = 0;
    for (std::size_t i = 1; i < m_spins.size(); ++i) {
        if (m_spins[i]) { ++count; }
    }
    return count;
}

QString PaCalibrationGroup::labelTextForTest(int idx) const
{
    if (idx < 1 || idx > 10) { return {}; }
    if (!m_labels[static_cast<std::size_t>(idx)]) { return {}; }
    return m_labels[static_cast<std::size_t>(idx)]->text();
}

double PaCalibrationGroup::spinValueForTest(int idx) const
{
    if (idx < 1 || idx > 10) { return 0.0; }
    if (!m_spins[static_cast<std::size_t>(idx)]) { return 0.0; }
    return m_spins[static_cast<std::size_t>(idx)]->value();
}

void PaCalibrationGroup::setSpinValueForTest(int idx, double v)
{
    if (idx < 1 || idx > 10) { return; }
    if (!m_spins[static_cast<std::size_t>(idx)]) { return; }
    m_spins[static_cast<std::size_t>(idx)]->setValue(v);
}

#endif  // NEREUS_BUILD_TESTS

}  // namespace NereusSDR
