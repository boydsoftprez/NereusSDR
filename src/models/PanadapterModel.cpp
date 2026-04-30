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

#include <QStringLiteral>

namespace NereusSDR {

namespace {

// Thetis uniform per-band default (console.cs:14242-14436). All 14 bands
// ship with the same values; per-band storage exists so users can
// customise, not because Thetis hand-tunes each band.
constexpr int kThetisDefaultDbMax = -40;
constexpr int kThetisDefaultDbMin = -140;
constexpr int kDefaultGridStep    = 10;  // NereusSDR divergence (§10).

QString gridMaxKey(Band b)      { return QStringLiteral("DisplayGridMax_") + bandKeyName(b); }
QString gridMinKey(Band b)      { return QStringLiteral("DisplayGridMin_") + bandKeyName(b); }
QString clarityFloorKey(Band b) { return QStringLiteral("ClarityFloor_")   + bandKeyName(b); }

} // namespace

PanadapterModel::PanadapterModel(QObject* parent)
    : QObject(parent)
{
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

PanadapterModel::~PanadapterModel() = default;

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
    applyBandGrid(b);
    emit bandChanged(b);
}

BandGridSettings PanadapterModel::perBandGrid(Band b) const
{
    return m_perBandGrid.value(b, BandGridSettings{ -40, -140 });
}

void PanadapterModel::setPerBandDbMax(Band b, int dbMax)
{
    BandGridSettings& slot = m_perBandGrid[b];  // constructor seeded all 14
    if (slot.dbMax == dbMax) {
        return;
    }
    slot.dbMax = dbMax;
    saveBandGridToSettings(b);
    if (b == m_band) {
        setdBmCeiling(dbMax);
    }
}

void PanadapterModel::setPerBandDbMin(Band b, int dbMin)
{
    BandGridSettings& slot = m_perBandGrid[b];
    if (slot.dbMin == dbMin) {
        return;
    }
    slot.dbMin = dbMin;
    saveBandGridToSettings(b);
    if (b == m_band) {
        setdBmFloor(dbMin);
    }
}

float PanadapterModel::clarityFloor(Band b) const
{
    return m_perBandGrid.value(b).clarityFloor;
}

void PanadapterModel::setClarityFloor(Band b, float floor)
{
    BandGridSettings& slot = m_perBandGrid[b];
    if ((!qIsNaN(floor) && !qIsNaN(slot.clarityFloor) && qFuzzyCompare(slot.clarityFloor, floor)) ||
        (qIsNaN(floor) && qIsNaN(slot.clarityFloor))) {
        return;
    }
    slot.clarityFloor = floor;
    saveBandGridToSettings(b);
}

void PanadapterModel::setGridStep(int step)
{
    if (step <= 0 || m_gridStep == step) {
        return;
    }
    m_gridStep = step;
    AppSettings::instance().setValue(QStringLiteral("DisplayGridStep"), step);
    emit gridStepChanged(step);
}

void PanadapterModel::applyBandGrid(Band b)
{
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
        const QVariant maxV = s.value(gridMaxKey(b));
        const QVariant minV = s.value(gridMinKey(b));
        const QVariant cfV  = s.value(clarityFloorKey(b));
        BandGridSettings slot = m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
        if (maxV.isValid()) { slot.dbMax = maxV.toInt(); }
        if (minV.isValid()) { slot.dbMin = minV.toInt(); }
        if (cfV.isValid())  { slot.clarityFloor = cfV.toFloat(); }
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
