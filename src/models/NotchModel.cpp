// =================================================================
// src/models/NotchModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources [v2.10.3.15] (commit 3759d096):
//   Project Files/Source/Console/radio.cs
//     MNotchDB spatial helpers NotchNearFreq / NotchesInBW /
//     NotchThatSurroundsFrequencyInBW at radio.cs:4260-4325.
//   Project Files/Source/Console/console.cs
//     NotchAdminBusy guards, ChangeNotchBW, ChangeNotchCentreFrequency,
//     changeNotchActive, removeNotch, AddNotch, notchSidebandShift,
//     notchMouseWheel clamps, _max_filter_width and max_freq at
//     console.cs:13221; 15552; 33299-33321; 39987-40005; 40007-40047;
//     40050-40120; 40123-40156; 40198-40219; 40222-40280; 40281-40307.
// The original licences from both Thetis sources are included below.
//
// Source attribution (AetherSDR, GPL-3.0-or-later):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//     - per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   AetherSDR has no per-file copyright header, so per
//   docs/attribution/HOW-TO-PORT.md rule 6 the project URL and primary
//   author are cited at NereusSDR block level rather than copying a
//   verbatim header that does not exist. The stable-id notch-store shape
//   is a port of AetherSDR src/models/TnfModel.{h,cpp} [@c6481cbf].
//   AetherSDR is licensed under the GNU General Public License v3 or
//   later. NereusSDR is also GPLv3. Attribution follows GPLv3 section 5
//   requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-01  J.J. Boyd / KG4VCF  TNF (tunable notch filter) Task 3.
//                 Reimplemented in C++20/Qt6 for NereusSDR, with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 Thetis's index-is-identity MNotchDB gains a stable
//                 monotonic `id` (the AetherSDR TnfModel addition), which
//                 removes the upstream GetFirstNotchThatMatches
//                 selection-recovery dance. AetherSDR's TnfEntry::depthDb
//                 and ::permanent are dropped: both are SmartSDR
//                 capabilities with no WDSP equivalent (design section
//                 1.2). The Thetis CW-pitch correction
//                 (console.cs:40228) and the XVTR min/max override
//                 (console.cs:40051-40077, :40232-40254) are deliberately
//                 NOT ported; see design sections 1.2 and 5.4.
// =================================================================

// --- From radio.cs ---

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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

#include "NotchModel.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QLatin1String>
#include <QVariant>

#include <cmath>

namespace NereusSDR {

namespace {

// Boolean to the AppSettings canonical string, matching how SliceModel
// stores its own flags.
QString boolStr(bool v)
{
    return v ? QStringLiteral("True") : QStringLiteral("False");
}

bool boolFrom(const QVariant& v)
{
    return v.toString() == QLatin1String("True");
}

}  // namespace

NotchModel::NotchModel(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int NotchModel::indexOfId(int id) const
{
    for (int i = 0; i < m_notches.size(); ++i) {
        if (m_notches.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

const Notch* NotchModel::notchById(int id) const
{
    const int index = indexOfId(id);
    if (index < 0) {
        return nullptr;
    }
    return &m_notches.at(index);
}

//MW0LGE check if notch close by
// From Thetis radio.cs:4261-4272 [v2.10.3.15], MNotchDB.NotchNearFreq.
// Strict `<` at radio.cs:4267, so a notch exactly deltaHz away does NOT
// block an add.
bool NotchModel::notchNearFreq(double hz, int deltaHz) const
{
    for (const Notch& n : m_notches) {
        if (std::abs(hz - n.centerHz) < deltaHz) {
            return true;
        }
    }
    return false;
}

//MW0LGE return list of notches in given bandwidth
//notch is included if filter width is enough to be within the BW
// From Thetis radio.cs:4276-4293 [v2.10.3.15], MNotchDB.NotchesInBW.
// Overlap test at radio.cs:4286 is inclusive on both sides.
QList<Notch> NotchModel::notchesInBandwidth(double centreHz,
                                            int lowHz, int highHz) const
{
    QList<Notch> l;
    const double min = centreHz + lowHz;
    const double max = centreHz + highHz;

    for (const Notch& n : m_notches) {
        if (((n.centerHz + n.widthHz / 2) >= min)
            && ((n.centerHz - n.widthHz / 2) <= max)) {
            l.append(n);
        }
    }

    return l;
}

//MW0LGE return first notch found that surrounds a given frequency in the given bandwidth
// From Thetis radio.cs:4297-4325 [v2.10.3.15],
// MNotchDB.NotchThatSurroundsFrequencyInBW.
// Upstream materialises NotchesInBW() and walks the copy. NereusSDR folds
// the same predicate into one pass over m_notches so the returned pointer
// stays valid; iteration order is identical because NotchesInBW preserves
// list order.
const Notch* NotchModel::notchSurrounding(double centreHz, int lowHz,
                                          int highHz, double hz,
                                          int padWidthHz) const
{
    const double min = centreHz + lowHz;
    const double max = centreHz + highHz;

    for (int i = 0; i < m_notches.size(); ++i) {
        const Notch& n = m_notches.at(i);

        if (!(((n.centerHz + n.widthHz / 2) >= min)
              && ((n.centerHz - n.widthHz / 2) <= max))) {
            continue;
        }

        double dLf = n.centerHz - n.widthHz / 2;
        double dHf = n.centerHz + n.widthHz / 2;

        if (n.widthHz < (padWidthHz * 2)) {
            dLf -= padWidthHz;
            dHf += padWidthHz;
        }

        if (hz >= dLf && hz <= dHf) {
            return &m_notches.at(i);
        }
    }

    return nullptr;
}

// used when adding a notch to shift it into the middle of the sideband
// From Thetis console.cs:40281-40307 [v2.10.3.15], notchSidebandShift(rx).
// Upstream reads the edges off radio.GetDSPRX(...).RXFilterLow /
// RXFilterHigh per rx index; a global, slice-agnostic NotchModel cannot, so
// the caller supplies the active slice's edges (SliceModel::filterLow() /
// filterHigh()).
//
// The Thetis CW-pitch term is deliberately absent (design section 1.2):
// upstream TNFAdd's +cw_pitch and AddNotch's -cw_pitch cancel, and
// NereusSDR keeps the CW pitch in the filter passband rather than on the
// DDC, so this shift alone is already correct.
int NotchModel::notchSidebandShift(int filterLowHz, int filterHighHz)
{
    int middle = filterLowHz + ((filterHighHz - filterLowHz) / 2);
    if (middle == 0) { // probably symetric filter such as AM
        middle = filterHighHz / 2;
    }
    return middle;
}

// From Thetis console.cs:40313-40331 [v2.10.3.15], TNFAdd(rx). Upstream reads
// the VFO off the console and adds RIT before the shift; a global NotchModel
// has neither, so the RIT-inclusive frequency arrives as the argument.
//
// The RIT term is the design section 7.5 divergence: upstream's
// `vfoHz += (double)RITValue * 1e-6` at console.cs:40321 scales an already-Hz
// RITValue onto an already-Hz quantity, so the term is inert. Section 4.1
// puts RIT inside the WDSP shift, which means the slice's demodulated RF
// already carries it and a notch placed at the bare VFO would land rit_hz off
// the signal. Callers pass SliceModel::effectiveRxFrequency().
double NotchModel::tnfAddCenterHz(double effectiveRxFrequencyHz,
                                  int filterLowHz, int filterHighHz)
{
    // shift into sideband
    // From Thetis console.cs:40329 [v2.10.3.15]:
    //     vfoHz += notchSidebandShift(rx); //MW0LGE_21k9rc4
    return effectiveRxFrequencyHz
         + static_cast<double>(notchSidebandShift(filterLowHz, filterHighHz));
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

// From Thetis console.cs:40222-40280 [v2.10.3.15], AddNotch(fFreqHZ, sourceRX).
// Guard order is upstream's: admin-busy, round, constrain, dedupe, with the
// round-before-constrain ordering that //[2.10.3.7]MW0LGE introduced. The
// CW-pitch shift (:40228, GetDSPcwPitchShiftToZero) and the XVTR min/max
// override (:40232-40254, //MW0LGE_21e XVTR) are deliberately not ported;
// see design sections 1.2 and 5.4.
int NotchModel::addNotch(double centerHz, double widthHz)
{
    // From Thetis console.cs:40224 [v2.10.3.15]
    if (m_adminBusy) { // dont add if using add/edit on the setup form
        emit notchAddRejected(
            QStringLiteral("The MNF settings page is mid-edit"));
        return -1;
    }

    // From Thetis console.cs:40230 [v2.10.3.15]. C# Math.Round(double) is
    // MidpointRounding.ToEven, which is what std::nearbyint does under the
    // default FE_TONEAREST rounding mode.
    centerHz = std::nearbyint(centerHz); //[2.10.3.7]MW0LGE moved from below

    //constrain
    // From Thetis console.cs:40256-40257 [v2.10.3.15]
    if (centerHz < kMinNotchCentreHz || centerHz > kMaxNotchCentreHz) {
        emit notchAddRejected(
            QStringLiteral("Frequency is outside the radio tuning range"));
        return -1;
    }

    // if there is a notch within 10hz ignore
    // From Thetis console.cs:40259-40260 [v2.10.3.15]
    if (notchNearFreq(centerHz, kNotchDedupeWindowHz)) {
        emit notchAddRejected(
            QStringLiteral("A notch already exists within 10 Hz"));
        return -1;
    }

    // Design section 5.2: append at position n, so the list position IS the
    // WDSP notch index the fan-out will pass to RXANBPAddNotch.
    Notch n;
    n.id       = m_nextId++;
    n.centerHz = centerHz;
    n.widthHz  = widthHz;
    n.active   = true;
    m_notches.append(n);

    persist();
    emit notchAdded(n.id);
    return n.id;
}

// From Thetis console.cs:40050-40120 [v2.10.3.15],
// ChangeNotchCentreFrequency(notch, newCentreFrequencyHz, sourceRX).
// Upstream orders the guards constrain, then admin-busy, then round; the
// order is preserved. The XVTR override (:40054-40074, //MW0LGE_21e XVTR)
// is not ported (design section 5.4), and neither is the selection-recovery
// call it guards: //MW0LGE [2.9.0.7] fix old bug, we need to find the notch
// for the updated freq (console.cs:40109) exists only because Thetis
// identifies a notch by its list position. Stable ids retire it.
bool NotchModel::setCenter(int id, double centerHz)
{
    //constrain
    // From Thetis console.cs:40076-40077 [v2.10.3.15]
    if (centerHz < kMinNotchCentreHz || centerHz > kMaxNotchCentreHz) {
        return false;
    }

    // From Thetis console.cs:40079 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    // From Thetis console.cs:40081 [v2.10.3.15]
    centerHz = std::nearbyint(centerHz);

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    // Upstream returns true whenever the index resolved and fires its change
    // handler only when the value actually moved (console.cs:40112-40115).
    if (m_notches.at(index).centerHz == centerHz) {
        return true;
    }
    m_notches[index].centerHz = centerHz;
    persist();
    emit notchChanged(id);
    return true;
}

// From Thetis console.cs:40007-40047 [v2.10.3.15], ChangeNotchBW(notch,
// newWidth, notch_index), composed with the clamps its only interactive
// caller applies first (notchMouseWheel, console.cs:33312-33318). Folding
// them in here is what lets the panadapter wheel handler be a bare
// setWidth(id, current + delta * step) call.
bool NotchModel::setWidth(int id, double widthHz)
{
    // From Thetis console.cs:40009 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    // From Thetis console.cs:33312-33313 [v2.10.3.15]
    if (widthHz < 0.0) {
        widthHz = 0.0;
    }
    if (widthHz > kMaxNotchWidthHz) {
        widthHz = kMaxNotchWidthHz;
    }

    // check to see if outside frequency limits
    // From Thetis console.cs:33315-33318 [v2.10.3.15]. A width whose edges
    // would leave the range is rejected outright, not clamped down. The
    // lower-edge arm is unreachable on this tree (kMinNotchCentreHz 100 kHz
    // minus half of kMaxNotchWidthHz is still 95 kHz), unlike Thetis where
    // min_freq can be 0.0; ported verbatim rather than dropped.
    const double centreHz = m_notches.at(index).centerHz;
    if (centreHz - (widthHz / 2) < 0) {
        return false;
    }
    if (centreHz + (widthHz / 2) > kMaxNotchCentreHz) {
        return false;
    }

    if (m_notches.at(index).widthHz == widthHz) {
        return true;
    }
    m_notches[index].widthHz = widthHz;
    persist();
    emit notchChanged(id);
    return true;
}

// From Thetis console.cs:40123-40156 [v2.10.3.15],
// changeNotchActive(notch, bActive).
bool NotchModel::setActive(int id, bool active)
{
    // From Thetis console.cs:40125 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    if (m_notches.at(index).active == active) {
        return true;
    }
    m_notches[index].active = active;
    persist();
    emit notchChanged(id);
    return true;
}

// From Thetis console.cs:40198-40219 [v2.10.3.15], removeNotch(notch).
// WDSP shifts its own notch array down inside RXANBPDeleteNotch, so erasing
// at the same position keeps the two in lockstep (design section 5.2).
bool NotchModel::removeNotch(int id)
{
    // From Thetis console.cs:40200 [v2.10.3.15]
    if (m_adminBusy) { // cant remove it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    m_notches.removeAt(index);
    persist();
    emit notchRemoved(id, index);
    return true;
}

void NotchModel::setAdminBusy(bool busy)
{
    // Transient edit lock, not persisted: it exists only for the lifetime of
    // an open MNF settings-page edit.
    m_adminBusy = busy;
}

// ---------------------------------------------------------------------------
// Global flags
// ---------------------------------------------------------------------------

// From Thetis console.cs:39987-40005 [v2.10.3.15], TNFActive. Upstream gates
// its change handler on `if (old_tnf != value)` at console.cs:40004;
// preserved so a repeated set does not re-push the run flag to every channel.
void NotchModel::setGlobalEnabled(bool on)
{
    if (m_globalEnabled == on) {
        return;
    }
    m_globalEnabled = on;
    persist();
    emit globalEnabledChanged(on);
}

// Fanned out to RXANBPSetAutoIncrease. Starts ON because WDSP creates nbp0
// with autoincr = 1 (From WDSP RXA.c:105).
void NotchModel::setAutoIncrease(bool on)
{
    if (m_autoIncrease == on) {
        return;
    }
    m_autoIncrease = on;
    persist();
    emit autoIncreaseChanged(on);
}

// Thetis's equivalent operator switch is the chkVisualNotch checkbox on the
// Display settings page, which drives its display layer directly. NereusSDR
// keeps the flag on the model and lets SpectrumWidget observe it; the
// checkbox and its upstream cite live on the settings page.
void NotchModel::setVisualEnabled(bool on)
{
    if (m_visualEnabled == on) {
        return;
    }
    m_visualEnabled = on;
    persist();
    emit visualEnabledChanged(on);
}

void NotchModel::clear()
{
    m_notches.clear();
    persist();
    // Design section 5.3 clear() contract: the RadioModel fan-out is purely
    // signal-driven, so a silent clear() would leave every channel's notch
    // set installed while the model showed none. Emitted unconditionally,
    // including when the list was already empty, because the signal is the
    // reconcile trigger rather than a change notification.
    emit notchesReset();
}

// ---------------------------------------------------------------------------
// Persistence (AppSettings, global scope; design section 5.5)
// ---------------------------------------------------------------------------

void NotchModel::persist()
{
    if (m_restoring) {
        return;
    }
    saveToSettings();
}

void NotchModel::saveToSettings() const
{
    auto& s = AppSettings::instance();

    // Prune the tail left by a previously longer list before writing the new
    // count, otherwise a shrink leaves orphan Notch<i>* entries behind and a
    // later grow would read stale values back.
    const int previousCount =
        s.value(QStringLiteral("NotchCount"), QStringLiteral("0"))
            .toString().toInt();
    for (int i = m_notches.size(); i < previousCount; ++i) {
        s.remove(QStringLiteral("Notch%1Center").arg(i));
        s.remove(QStringLiteral("Notch%1Width").arg(i));
        s.remove(QStringLiteral("Notch%1Active").arg(i));
    }

    s.setValue(QStringLiteral("NotchGlobalEnabled"), boolStr(m_globalEnabled));
    s.setValue(QStringLiteral("NotchVisualEnabled"), boolStr(m_visualEnabled));
    s.setValue(QStringLiteral("NotchAutoIncrease"),  boolStr(m_autoIncrease));
    s.setValue(QStringLiteral("NotchCount"),
               QString::number(m_notches.size()));

    for (int i = 0; i < m_notches.size(); ++i) {
        const Notch& n = m_notches.at(i);
        // Explicit fixed formatting: AppSettings stores QVariant::toString()
        // of whatever it is handed, and an 'f' with 6 decimals is lossless
        // for the whole-Hz centres this model produces.
        s.setValue(QStringLiteral("Notch%1Center").arg(i),
                   QString::number(n.centerHz, 'f', 6));
        s.setValue(QStringLiteral("Notch%1Width").arg(i),
                   QString::number(n.widthHz, 'f', 6));
        s.setValue(QStringLiteral("Notch%1Active").arg(i), boolStr(n.active));
    }
}

void NotchModel::restoreFromSettings()
{
    auto& s = AppSettings::instance();

    // Suppress save-on-mutate for the duration: the setters below would
    // otherwise write the half-restored list straight back over the keys
    // still being read out of it.
    m_restoring = true;

    // Each key: if absent, leave the current default unchanged. Restores go
    // through the public setters so observers see the change.
    if (s.contains(QStringLiteral("NotchGlobalEnabled"))) {
        setGlobalEnabled(boolFrom(s.value(QStringLiteral("NotchGlobalEnabled"))));
    }
    if (s.contains(QStringLiteral("NotchVisualEnabled"))) {
        setVisualEnabled(boolFrom(s.value(QStringLiteral("NotchVisualEnabled"))));
    }
    if (s.contains(QStringLiteral("NotchAutoIncrease"))) {
        setAutoIncrease(boolFrom(s.value(QStringLiteral("NotchAutoIncrease"))));
    }

    const bool hasList = s.contains(QStringLiteral("NotchCount"));
    if (hasList) {
        const int count =
            s.value(QStringLiteral("NotchCount")).toString().toInt();
        m_notches.clear();

        for (int i = 0; i < count; ++i) {
            const QString centerKey = QStringLiteral("Notch%1Center").arg(i);
            if (!s.contains(centerKey)) {
                qCWarning(lcDsp) << "NotchModel: missing" << centerKey
                                 << "- stopping notch restore at index" << i;
                break;
            }

            // ids are session-local hit-test keys and are deliberately not
            // persisted (design section 5.1); they are re-minted
            // monotonically here.
            Notch n;
            n.id       = m_nextId++;
            n.centerHz = s.value(centerKey).toDouble();
            n.widthHz  = s.value(QStringLiteral("Notch%1Width").arg(i),
                                 QString::number(kDefaultNotchWidthHz, 'f', 6))
                             .toDouble();
            n.active   = boolFrom(s.value(QStringLiteral("Notch%1Active").arg(i),
                                          QStringLiteral("True")));
            m_notches.append(n);
        }
    }

    m_restoring = false;

    if (!hasList) {
        return;
    }

    // Whole-list replacement (design section 5.3): RadioModel reconciles
    // every open channel off this signal.
    emit notchesReset();
}

}  // namespace NereusSDR
