// =================================================================
// src/core/accessories/ApolloController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs:15566-15590
//   (chkApolloPresent_CheckedChanged, chkApolloFilter_CheckedChanged,
//    chkApolloTuner_CheckedChanged — Apollo PA + ATU + LPF accessory toggles)
//   Project Files/Source/Console/console.cs:19060-19105
//   (ApolloPresent property, ApolloTunerEnabled property)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Models the Apollo PA + ATU + LPF accessory
//                board present/filter/tuner booleans. No tuner-state enum
//                found in Thetis source — only a bool enable flag. Per-MAC
//                persistence via AppSettings. Setup UI deferred to a
//                follow-up if Apollo owners request it.
// =================================================================
//
// === Verbatim Thetis Console/setup.cs header (lines 1-43) ===
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
//
// =================================================================

#pragma once

#include <QObject>
#include <QString>

namespace NereusSDR {

// Apollo PA + ATU + LPF accessory board state model.
//
// Source: setup.cs:15566-15590 + console.cs:19060-19105 [@501e3f5]
//
// Thetis stores these as three check-boxes in the Setup dialog:
//   chkApolloPresent  → ApolloPresent property on console
//   chkApolloFilter   → NetworkIO.SelectApolloFilter / EnableApolloFilter
//   chkApolloTuner    → ApolloTunerEnabled property on console
//                       → NetworkIO.EnableApolloTuner(1|0)
//
// NereusSDR: model-only, no Setup page in this task.  Protocol
// wire-up (NetworkIO equivalents) deferred to the codec layer.
class ApolloController : public QObject {
    Q_OBJECT

public:
    explicit ApolloController(QObject* parent = nullptr);

    // ── Apollo board presence ────────────────────────────────────────────────
    // Source: console.cs:19070 ApolloPresent, setup.cs:15567 chkApolloPresent [@501e3f5]
    // "if (chkApolloPresent.Checked) { if (chkAlexPresent.Checked) chkAlexPresent.Checked = false;
    //   NetworkIO.SelectApolloFilter(1); ... }"
    bool present() const;
    void setPresent(bool on);

    // ── Apollo filter enable ─────────────────────────────────────────────────
    // Source: setup.cs:15582 chkApolloFilter_CheckedChanged [@501e3f5]
    // "if (chkApolloFilter.Checked) NetworkIO.EnableApolloFilter(1); else NetworkIO.EnableApolloFilter(0);"
    bool filterEnabled() const;
    void setFilterEnabled(bool on);

    // ── Apollo tuner enable ──────────────────────────────────────────────────
    // Source: setup.cs:15587 chkApolloTuner_CheckedChanged [@501e3f5]
    //         console.cs:19097 ApolloTunerEnabled property [@501e3f5]
    // "apollo_tuner_enabled = value; if (apollo_tuner_enabled) NetworkIO.EnableApolloTuner(1);
    //  else NetworkIO.EnableApolloTuner(0);"
    // Note: Thetis has only a bool enable; no tuner-state enum was found in upstream source.
    bool tunerEnabled() const;
    void setTunerEnabled(bool on);

    // ── Persistence ──────────────────────────────────────────────────────────
    void setMacAddress(const QString& mac);
    void load();   // hydrate from AppSettings under hardware/<mac>/apollo/{present,...}
    void save();   // persist current state to AppSettings

signals:
    void presentChanged(bool on);       // fires when presence toggle changes
    void filterEnabledChanged(bool on); // fires when filter toggle changes
    void tunerEnabledChanged(bool on);  // fires when tuner toggle changes

private:
    bool m_present{false};
    bool m_filterEnabled{false};
    bool m_tunerEnabled{false};

    QString m_mac;

    QString persistenceKey() const;  // "hardware/<mac>/apollo"
};

} // namespace NereusSDR
