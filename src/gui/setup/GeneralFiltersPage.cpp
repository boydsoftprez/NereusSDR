// =================================================================
// src/gui/setup/GeneralFiltersPage.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - Setup > General > Filters with Thetis's "Limit filter
//                edges to sidebands" checkbox (setup.designer.cs:12426-12437,
//                setup.cs:22378-22381 [v2.10.3.15]), the Core's
//                LimitFiltersToSidebands setting. J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Claude Code.
//   2026-10-24 - The checkbox carries the Setup description id
//                general.filters.limitFilterEdgesToSidebands, the phone's
//                Filters row. J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
// =================================================================

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

#include "GeneralFiltersPage.h"

#include "core/AppSettings.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>

namespace NereusSDR {

GeneralFiltersPage::GeneralFiltersPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Filters"), model, parent)
{
    // From Thetis setup.designer.cs:12426-12437 [v2.10.3.15]
    // (chkLimitFilterEdgesToSidebands): Text "Limit filter edges to
    // sidebands", tooltip "Prevents crossing the 0 point with certain
    // filters", unchecked by default (console.cs:7516 [v2.10.3.15]).
    m_chkLimitFilterEdgesToSidebands =
        new QCheckBox(tr("Limit filter edges to sidebands"), this);
    m_chkLimitFilterEdgesToSidebands->setObjectName(
        QStringLiteral("chkLimitFilterEdgesToSidebands"));
    m_chkLimitFilterEdgesToSidebands->setToolTip(
        tr("Prevents crossing the 0 point with certain filters"));
    m_chkLimitFilterEdgesToSidebands->setProperty(
        "nereusSetupId", "general.filters.limitFilterEdgesToSidebands");
    syncLimitFromModel();
    connect(m_chkLimitFilterEdgesToSidebands, &QCheckBox::toggled, this, [this](bool on) {
        if (m_lblLimitCore) {
            m_lblLimitCore->setVisible(false);
        }
        // From Thetis setup.cs:22378-22381 [v2.10.3.15]
        // (chkLimitFilterEdgesToSidebands_CheckedChanged):
        //   console.LimitFiltersToSideBands = chkLimitFilterEdgesToSidebands.Checked;
        if (RadioModel* radio = this->model()) {
            radio->setLimitFiltersToSidebands(on);
        } else {
            AppSettings::instance().setValue(
                QLatin1String(RadioModel::kLimitFiltersToSidebandsKey),
                on ? QStringLiteral("True") : QStringLiteral("False"));
        }
    });
    addContent(m_chkLimitFilterEdgesToSidebands);

    // The box follows the model: another window's change through the Core,
    // or this window's own from elsewhere.
    if (RadioModel* radio = this->model()) {
        connect(radio, &RadioModel::limitFiltersToSidebandsChanged, this,
                [this](bool) { syncLimitFromModel(); });
    }

    // An older Core keeps this setting to itself and refuses a window's
    // change; the box goes back and the page says why (as the Network
    // Watchdog box does on General > Options).
    m_lblLimitCore = new QLabel(
        tr("The Core needs updating before this window can change Limit filter "
           "edges to sidebands."), this);
    m_lblLimitCore->setObjectName(QStringLiteral("lblLimitFilterEdgesCore"));
    m_lblLimitCore->setWordWrap(true);
    m_lblLimitCore->setVisible(false);
    addContent(m_lblLimitCore);
    if (auto* proxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend())) {
        connect(proxy, &SettingsProxy::valueRejected, this,
                [this](const QString& key, const QVariant& restored) {
            if (key != QLatin1String(RadioModel::kLimitFiltersToSidebandsKey)) {
                return;
            }
            const bool restoredOn = restored.isValid()
                                    && restored.toString() == QLatin1String("True");
            if (RadioModel* radio = this->model()) {
                radio->applyLimitFiltersToSidebands(restoredOn);
            }
            {
                const QSignalBlocker blocker(m_chkLimitFilterEdgesToSidebands);
                m_chkLimitFilterEdgesToSidebands->setChecked(restoredOn);
            }
            m_lblLimitCore->setVisible(true);
        });
    }
}

void GeneralFiltersPage::syncFromModel()
{
    SetupPage::syncFromModel();
    syncLimitFromModel();
}

void GeneralFiltersPage::syncLimitFromModel()
{
    if (!m_chkLimitFilterEdgesToSidebands) {
        return;
    }
    const bool on = model() ? model()->limitFiltersToSidebands()
                            : RadioModel::limitFiltersToSidebandsSetting();
    const QSignalBlocker blocker(m_chkLimitFilterEdgesToSidebands);
    m_chkLimitFilterEdgesToSidebands->setChecked(on);
}

} // namespace NereusSDR
