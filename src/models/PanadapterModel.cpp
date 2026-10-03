// =================================================================
// src/models/PanadapterModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-28 - R-IOS-18: the per-band grid, dB step and per-band 3D
//                 floor, one store for every pan, reach every pan's model
//                 when one changes. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - Parity ruling C12: a band crossing applies the per-band
//                 grid only on a pan that follows it (not a remote
//                 window's), and applyStationGridSetting re-reads a band's
//                 dB max and min when the setting changes. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m reads and writes XVTR's grid slot, as Thetis's
//                 per-band switch does (R-IOS-26, R-R3-49). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

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

#include "PanadapterModel.h"

#include "core/AppSettings.h"

#include <algorithm>
#include <vector>

#include <QStringLiteral>

namespace NereusSDR {

namespace {

// Thetis uniform per-band default (console.cs:14242-14436). All 14 bands
// ship with the same values; per-band storage exists so users can
// customise, not because Thetis hand-tunes each band.
constexpr int kThetisDefaultDbMax = -40;
constexpr int kThetisDefaultDbMin = -140;
constexpr int kDefaultGridStep    = 10;  // NereusSDR divergence (§10).

// The grid slot a band reads and writes. Thetis keeps no display grid of
// its own for 2 m: its per-band switch sends B2M, like every band without a
// case, to the XVTR values.
// From Thetis console.cs:9337-9339 [v2.10.3.15]:
//   default:
//       SetupForm.DisplayGridMin = DisplayGridMinXVTR;
//       Display.SpectrumGridMin = (int)DisplayGridMinXVTR;
// and console.cs:9474-9476 for DisplayGridMaxXVTR. The rest of the slot
// (Clarity floor, noise floor estimate, 3D depth) follows the grid.
Band gridSlot(Band b) { return b == Band::Band2m ? Band::XVTR : b; }

QString gridMaxKey(Band b)      { return QStringLiteral("DisplayGridMax_") + bandKeyName(b); }
QString gridMinKey(Band b)      { return QStringLiteral("DisplayGridMin_") + bandKeyName(b); }
QString clarityFloorKey(Band b) { return QStringLiteral("ClarityFloor_")   + bandKeyName(b); }
// NereusSDR-original — no Thetis equivalent.
QString bandNFKey(Band b)       { return QStringLiteral("DisplayBandNFEstimate_") + bandKeyName(b); }
// NereusSDR-original: no Thetis equivalent (3D Stacked-Trace Spectrum Plan
// Task 14). Follows the same no-pan-index convention as the keys above.
QString dss3DFloorDepthKey(Band b) { return QStringLiteral("Display3DFloorDepth_") + bandKeyName(b); }

// Every PanadapterModel. The per-band grid, the dB step and the per-band 3D
// floor are one store for every pan (their keys carry no pan index), so a
// change through one model is given to the others (R-IOS-18); otherwise a
// pan keeps its old slot and shows it again on its next band change.
// GUI thread only.
std::vector<PanadapterModel*>& allPanModels()
{
    static std::vector<PanadapterModel*> models;
    return models;
}

template <typename Apply>
void shareWithOtherModels(PanadapterModel* self, Apply apply)
{
    const std::vector<PanadapterModel*> models = allPanModels();
    for (PanadapterModel* model : models) {
        if (model != self) { apply(model); }
    }
}

} // namespace

PanadapterModel::PanadapterModel(QObject* parent)
    : QObject(parent)
{
    allPanModels().push_back(this);
    // Seed every band slot with Thetis uniform defaults before loading
    // persisted overrides. This matches Q4 resolution (plan §5.3): existing
    // users will see the grid shift from NereusSDR's -20/-160 to Thetis's
    // -40/-140 on first launch after upgrade.
    // Per-band display grid: HF amateur + GEN/WWV/XVTR only.  SWL bands
    // (Phase 3L Band enum extension, indices >= Band::SwlFirst) have no
    // panadapter buttons and inherit the GEN grid slot when tuned.
    for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
        const Band b = static_cast<Band>(i);
        m_perBandGrid.insert(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
    }
    loadPerBandGridFromSettings();

    // Match the initial band to the default center frequency so the
    // dBmFloor/dBmCeiling pair reflects the 20m slot from the start.
    m_band = bandFromFrequency(m_centerFrequency);
    applyBandGrid(m_band);
}

PanadapterModel::~PanadapterModel()
{
    auto& models = allPanModels();
    models.erase(std::remove(models.begin(), models.end(), this), models.end());
}

void PanadapterModel::setCenterFrequency(double freq)
{
    if (!qFuzzyCompare(m_centerFrequency, freq)) {
        m_centerFrequency = freq;
        emit centerFrequencyChanged(freq);

        // Auto-derive the enclosing band. If this crosses a boundary,
        // setBand() updates the dBm pair to the new band's slot.
        const Band derived = bandFromFrequency(freq);
        if (derived != m_band) {
            setBand(derived);
        }
    }
}

void PanadapterModel::setBandwidth(double bw)
{
    if (!qFuzzyCompare(m_bandwidth, bw)) {
        m_bandwidth = bw;
        emit bandwidthChanged(bw);
    }
}

void PanadapterModel::setdBmFloor(int floor)
{
    if (m_dBmFloor != floor) {
        m_dBmFloor = floor;
        emit levelChanged();
    }
}

void PanadapterModel::setdBmCeiling(int ceiling)
{
    if (m_dBmCeiling != ceiling) {
        m_dBmCeiling = ceiling;
        emit levelChanged();
    }
}

void PanadapterModel::setFftSize(int size)
{
    if (m_fftSize != size) {
        m_fftSize = size;
        emit fftSizeChanged(size);
    }
}

void PanadapterModel::setAverageCount(int count)
{
    m_averageCount = count;
}

// ---- Per-band grid (Phase 3G-8 commit 2) ----

void PanadapterModel::setBand(Band b)
{
    if (m_band == b) {
        return;
    }
    m_band = b;
    if (m_followsBandGrid) {
        applyBandGrid(b);
    }
    emit bandChanged(b);
}

BandGridSettings PanadapterModel::perBandGrid(Band b) const
{
    b = gridSlot(b);
    return m_perBandGrid.value(b, BandGridSettings{ -40, -140 });
}

void PanadapterModel::setPerBandDbMax(Band b, int dbMax)
{
    b = gridSlot(b);
    BandGridSettings& slot = m_perBandGrid[b];  // constructor seeded all 14
    if (slot.dbMax == dbMax) {
        return;
    }
    slot.dbMax = dbMax;
    saveBandGridToSettings(b);
    if (b == gridSlot(m_band)) {
        setdBmCeiling(dbMax);
    }
    shareWithOtherModels(this, [b, dbMax](PanadapterModel* pan) { pan->setPerBandDbMax(b, dbMax); });
}

void PanadapterModel::setPerBandDbMin(Band b, int dbMin)
{
    b = gridSlot(b);
    BandGridSettings& slot = m_perBandGrid[b];
    if (slot.dbMin == dbMin) {
        return;
    }
    slot.dbMin = dbMin;
    saveBandGridToSettings(b);
    if (b == gridSlot(m_band)) {
        setdBmFloor(dbMin);
    }
    shareWithOtherModels(this, [b, dbMin](PanadapterModel* pan) { pan->setPerBandDbMin(b, dbMin); });
}

float PanadapterModel::clarityFloor(Band b) const
{
    b = gridSlot(b);
    return m_perBandGrid.value(b).clarityFloor;
}

void PanadapterModel::setClarityFloor(Band b, float floor)
{
    b = gridSlot(b);
    BandGridSettings& slot = m_perBandGrid[b];
    if ((!qIsNaN(floor) && !qIsNaN(slot.clarityFloor) && qFuzzyCompare(slot.clarityFloor, floor)) ||
        (qIsNaN(floor) && qIsNaN(slot.clarityFloor))) {
        return;
    }
    slot.clarityFloor = floor;
    saveBandGridToSettings(b);
}

// NereusSDR-original — no Thetis equivalent.
float PanadapterModel::bandNFEstimate(Band b) const
{
    b = gridSlot(b);
    return m_perBandGrid.value(b).bandNFEstimate;
}

// NereusSDR-original — no Thetis equivalent.
void PanadapterModel::setBandNFEstimate(Band b, float nf)
{
    b = gridSlot(b);
    BandGridSettings& slot = m_perBandGrid[b];
    if ((!qIsNaN(nf) && !qIsNaN(slot.bandNFEstimate) && qFuzzyCompare(slot.bandNFEstimate, nf)) ||
        (qIsNaN(nf) && qIsNaN(slot.bandNFEstimate))) {
        return;
    }
    slot.bandNFEstimate = nf;
    if (!qIsNaN(nf)) {
        AppSettings::instance().setValue(bandNFKey(b), nf);
    }
}

// NereusSDR-original: no Thetis equivalent (3D Stacked-Trace Spectrum
// Plan Task 14).
int PanadapterModel::dss3DFloorDepthForBand(Band b) const
{
    b = gridSlot(b);
    return m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin })
        .dss3DFloorDepth;
}

// NereusSDR-original: no Thetis equivalent (3D Stacked-Trace Spectrum
// Plan Task 14). Writes its own key directly (does NOT go through
// saveBandGridToSettings()) so that touching only the per-band grid range
// via setPerBandDbMax/setPerBandDbMin never writes a Display3DFloorDepth_
// key for a band the operator has not touched in 3D mode. Mirrors the
// setBandNFEstimate() pattern above.
void PanadapterModel::setDss3DFloorDepthForBand(Band b, int depth)
{
    b = gridSlot(b);
    BandGridSettings& slot = m_perBandGrid[b];
    if (slot.dss3DFloorDepth == depth) {
        return;
    }
    slot.dss3DFloorDepth = depth;
    AppSettings::instance().setValue(dss3DFloorDepthKey(b), depth);
    shareWithOtherModels(this, [b, depth](PanadapterModel* pan) {
        pan->setDss3DFloorDepthForBand(b, depth);
    });
}

void PanadapterModel::setGridStep(int step)
{
    if (step <= 0 || m_gridStep == step) {
        return;
    }
    m_gridStep = step;
    AppSettings::instance().setValue(QStringLiteral("DisplayGridStep"), step);
    emit gridStepChanged(step);
    shareWithOtherModels(this, [step](PanadapterModel* pan) { pan->setGridStep(step); });
}

void PanadapterModel::applyBandGrid(Band b)
{
    b = gridSlot(b);
    const BandGridSettings s = m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
    setdBmCeiling(s.dbMax);
    setdBmFloor(s.dbMin);
}

void PanadapterModel::loadPerBandGridFromSettings()
{
    auto& s = AppSettings::instance();
    // Per-band display grid: HF amateur + GEN/WWV/XVTR only.  SWL bands
    // (Phase 3L Band enum extension, indices >= Band::SwlFirst) have no
    // panadapter buttons and inherit the GEN grid slot when tuned.
    for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
        const Band b = static_cast<Band>(i);
        const QVariant maxV  = s.value(gridMaxKey(b));
        const QVariant minV  = s.value(gridMinKey(b));
        const QVariant cfV   = s.value(clarityFloorKey(b));
        const QVariant nfV   = s.value(bandNFKey(b));
        const QVariant dssV  = s.value(dss3DFloorDepthKey(b));
        BandGridSettings slot = m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
        if (maxV.isValid())  { slot.dbMax          = maxV.toInt();   }
        if (minV.isValid())  { slot.dbMin          = minV.toInt();   }
        if (cfV.isValid())   { slot.clarityFloor   = cfV.toFloat();  }
        // NereusSDR-original — no Thetis equivalent.
        // Load per-band NF estimates persisted from previous sessions for priming.
        if (nfV.isValid())   { slot.bandNFEstimate = nfV.toFloat();  }
        // NereusSDR-original: no Thetis equivalent (3D Stacked-Trace
        // Spectrum Plan Task 14). Absent key keeps the struct's default
        // member initializer (6), same pattern as dbMax/dbMin above.
        if (dssV.isValid())  { slot.dss3DFloorDepth = dssV.toInt();  }
        m_perBandGrid.insert(b, slot);
    }

    const QVariant stepV = s.value(QStringLiteral("DisplayGridStep"));
    if (stepV.isValid()) {
        const int step = stepV.toInt();
        if (step > 0) { m_gridStep = step; }
    } else {
        m_gridStep = kDefaultGridStep;
    }
}

void PanadapterModel::applyStationGridSetting(const QString& key)
{
    // NereusSDR-original (parity ruling C12): no Thetis equivalent; Thetis
    // has one window per radio.
    auto& s = AppSettings::instance();
    const auto reload = [this, &s](Band b) {
        BandGridSettings& slot = m_perBandGrid[b];
        slot.dbMax = s.value(gridMaxKey(b), kThetisDefaultDbMax).toInt();
        slot.dbMin = s.value(gridMinKey(b), kThetisDefaultDbMin).toInt();
        if (b == m_band && m_followsBandGrid) {
            applyBandGrid(b);
        }
    };
    for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
        const Band b = static_cast<Band>(i);
        if (key.isEmpty() || key == gridMaxKey(b) || key == gridMinKey(b)) {
            reload(b);
        }
    }
}

void PanadapterModel::saveBandGridToSettings(Band b) const
{
    const BandGridSettings slot = m_perBandGrid.value(b);
    auto& s = AppSettings::instance();
    s.setValue(gridMaxKey(b), slot.dbMax);
    s.setValue(gridMinKey(b), slot.dbMin);
    if (!qIsNaN(slot.clarityFloor)) {
        s.setValue(clarityFloorKey(b), slot.clarityFloor);
    }
}

} // namespace NereusSDR
