// =================================================================
// src/models/NotchModel.h  (NereusSDR)
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

#pragma once

#include "core/dsp/Notch.h"

#include <QList>
#include <QObject>
#include <QString>

namespace NereusSDR {

/// Canonical store for operator-placed notches (TNF).
///
/// Global and slice-agnostic: Thetis fans one notch set to three fixed WDSP
/// channel ids (console.cs:40271-40273 [v2.10.3.15]); NereusSDR fans it to
/// every live RxChannel. The list is an ORDERED QList and its position IS the
/// WDSP notch index; every mutation keeps the two in lockstep (design
/// section 5.2).
///
/// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
/// section 5.
class NotchModel : public QObject {
    Q_OBJECT

public:
    explicit NotchModel(QObject* parent = nullptr);
    ~NotchModel() override = default;

    // ── Ported guard constants (design section 5.4) ──────────────────────

    // From Thetis console.cs:40259-40260 [v2.10.3.15]: "if there is a notch
    // within 10hz ignore"
    static constexpr int kNotchDedupeWindowHz = 10;

    // From Thetis console.cs:40268 [v2.10.3.15]: original value 200
    static constexpr double kDefaultNotchWidthHz = 200.0;

    // From Thetis console.cs:40269 [v2.10.3.15]: original value 100, taken
    // when the Shift key is down at add time.
    static constexpr double kNarrowNotchWidthHz = 100.0;

    // From Thetis console.cs:13221 [v2.10.3.15]: _max_filter_width = 10000
    static constexpr double kMaxNotchWidthHz = 10000.0;

    /// Hard capacity. Every WDSP notch database is created with room for
    /// exactly this many (third_party/wdsp/src/RXA.c:88), and RXANBPAddNotch
    /// returns -1 with no mutation once nn reaches it (nbp.c:368). The model
    /// must refuse past this point rather than append and report success:
    /// otherwise the UI and AppSettings hold a notch the DSP has never
    /// applied, and syncNotches truncates at the same limit, so the two can
    /// never be reconciled. Codex review of PR #313.
    static constexpr int kMaxNotches = 1024;

    // Wheel-resize step per detent, applied by the panadapter wheel handler
    // and folded through setWidth (which owns the clamping).
    // From Thetis console.cs:33305-33310 [v2.10.3.15]: Shift held adds the
    // raw detent count, no modifier multiplies it by 10.
    static constexpr double kWheelWidthStepHz     = 10.0;
    static constexpr double kWheelWidthStepFineHz = 1.0;

    // Thetis constrains the notch centre to min_freq..max_freq
    // (console.cs:40256-40257 and :40076-40077 [v2.10.3.15], where
    // max_freq = 61.44 at console.cs:15552 [v2.10.3.15]). NereusSDR has no
    // radio frequency-range capability field yet (design section 5.4), so the
    // notch constrain reuses the bounds VfoWidget already clamps operator
    // frequency entry to.
    static constexpr double kMinNotchCentreHz = 100000.0;
    static constexpr double kMaxNotchCentreHz = 61440000.0;

    // ── Queries ──────────────────────────────────────────────────────────
    const QList<Notch>& notches() const { return m_notches; }
    const Notch*        notchById(int id) const;
    int                 indexOfId(int id) const;

    bool globalEnabled() const { return m_globalEnabled; }
    bool autoIncrease()  const { return m_autoIncrease; }
    bool visualEnabled() const { return m_visualEnabled; }
    bool adminBusy()     const { return m_adminBusy; }

    // ── Thetis-ported spatial helpers [v2.10.3.15] ───────────────────────
    // Upstream's own description comments are kept verbatim above each
    // declaration; the bodies are in the .cpp beside their cites.

    //MW0LGE check if notch close by
    //   MNotchDB.NotchNearFreq                   radio.cs:4260-4272
    bool         notchNearFreq(double hz, int deltaHz) const;

    //MW0LGE return list of notches in given bandwidth
    //notch is included if filter width is enough to be within the BW
    //   MNotchDB.NotchesInBW                     radio.cs:4274-4293
    QList<Notch> notchesInBandwidth(double centreHz, int lowHz, int highHz) const;

    //MW0LGE return first notch found that surrounds a given frequency in the given bandwidth
    //   MNotchDB.NotchThatSurroundsFrequencyInBW radio.cs:4296-4325
    const Notch* notchSurrounding(double centreHz, int lowHz, int highHz,
                                  double hz, int padWidthHz = 0) const;

    // Pure helper for the +TNF button. Static because a global,
    // slice-agnostic NotchModel cannot reach per-slice filter edges; the
    // caller supplies the active slice's edges. Upstream reads them off the
    // DSP object (console.cs:40289-40295 [v2.10.3.15]).
    static int notchSidebandShift(int filterLowHz, int filterHighHz);

    // The centre the +TNF button asks for. Static and slice-agnostic for the
    // same reason notchSidebandShift is: a global NotchModel (design D1)
    // cannot reach per-slice filter edges or per-slice RIT, so the caller
    // supplies both.
    //
    // From Thetis console.cs:40313-40331 [v2.10.3.15], TNFAdd(rx):
    //     vfoHz = VFOAFreq * 1.0e6;
    //     if (RITOn) vfoHz += (double)RITValue * 1e-6;   // check for RIT
    //     vfoHz += notchSidebandShift(rx);               //MW0LGE_21k9rc4
    //     AddNotch(vfoHz, rx);
    //
    // Deliberate divergence, design section 7.5: upstream's `* 1e-6` scales
    // an already-Hz RITValue onto an already-Hz VFO, which makes the RIT term
    // inert (100 Hz of RIT moves the notch by 0.0001 Hz). We port the evident
    // intent, place the notch where the operator is listening, and fix the
    // unit by taking the RIT-inclusive frequency as the argument.
    // Flagged for maintainer review: reverting to upstream's effective
    // behaviour means passing the bare VFO instead.
    static double tnfAddCenterHz(double effectiveRxFrequencyHz,
                                 int filterLowHz, int filterHighHz);

    // ── Mutations ────────────────────────────────────────────────────────
    int  addNotch(double centerHz, double widthHz = kDefaultNotchWidthHz);
    bool setCenter(int id, double centerHz);
    bool setWidth(int id, double widthHz);
    bool setActive(int id, bool active);
    bool removeNotch(int id);

    void setGlobalEnabled(bool on);
    void setAutoIncrease(bool on);
    void setVisualEnabled(bool on);

    // Settings-page edit lock. From Thetis SetupForm.NotchAdminBusy
    // (console.cs:40009 [v2.10.3.15]): while the MNF page is mid-edit every
    // panadapter-side mutation is a no-op, so a drag cannot reorder the list
    // underneath the table's index mapping.
    void setAdminBusy(bool busy);

    void clear();

    // ── Persistence (AppSettings, global scope; design section 5.5) ──────
    // Every mutator above is save-on-mutate, so callers never have to
    // remember to call saveToSettings(). It stays public because the MNF
    // settings page commits a batch and because the tests assert the key
    // layout directly.
    void saveToSettings() const;
    void restoreFromSettings();

signals:
    void notchAdded(int id);
    void notchChanged(int id);
    // formerIndex is the positional (WDSP) index the entry occupied; it is
    // gone from the list by the time the signal lands, and the fan-out needs
    // it for RXANBPDeleteNotch.
    void notchRemoved(int id, int formerIndex);
    void globalEnabledChanged(bool on);
    void autoIncreaseChanged(bool on);
    void visualEnabledChanged(bool on);
    void notchAddRejected(const QString& reason);
    // Whole-list replacement. clear() and restoreFromSettings() emit this;
    // RadioModel handles it as syncNotches({}) / syncNotches(notches()) on
    // every channel (design section 5.3 clear() contract).
    void notchesReset();

private:
    // Persist the whole store. Called at the tail of every mutation that
    // actually changed something; suppressed while restoreFromSettings() is
    // repopulating so a restore cannot write back over its own source.
    void persist();

    // Main-thread-only state. WDSP owns the authoritative notch database
    // (From WDSP RXA.c:85-88); this list is the operator-facing source of
    // truth and is never read from the audio thread, so no atomics.
    QList<Notch> m_notches;
    int  m_nextId{1};

    // Master TNF switch. Ships OFF: Thetis's chkTNF is unchecked at startup
    // and WDSP creates the notch database with master run 0
    // (From WDSP RXA.c:87). AetherSDR defaults its equivalent flag to true
    // (TnfModel.h:52 [@c6481cbf]) because its list mirrors radio state
    // rather than owning it; ours is the source of truth, so it follows
    // Thetis and WDSP instead (maintainer decision D-a, 2026-07-29).
    bool m_globalEnabled{false};

    // WDSP creates nbp0 with autoincr = 1 (From WDSP RXA.c:105) and Thetis
    // ships chkMNFAutoIncrease checked, so the settings-page control starts
    // ON, not OFF.
    bool m_autoIncrease{true};

    // Thetis chkVisualNotch carries no designer Checked assignment, so
    // WinForms leaves it unchecked.
    bool m_visualEnabled{false};

    bool m_adminBusy{false};

    // Re-entrancy guard for persist(); see restoreFromSettings().
    bool m_restoring{false};
};

}  // namespace NereusSDR
