#pragma once
// =================================================================
// src/core/VoltsAmpsLog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:24798-24870 (readMKIIPAVoltsAmps,
//     its VALog.txt lines) and :24892-24916 (LogVA)
//   original licence from Thetis source is included below
//
// Setup > Hardware Config > Calibration's "Log Volts/Amps to VALog.txt":
// on, it writes the version, the title and the Volts/Amps calibration, then
// one line a second of the PA's two ADC readings and what they scale to,
// and turns itself off after an hour.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code (R-R3-49).
// =================================================================

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


#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>

namespace NereusSDR {

class VoltsAmpsLog final : public QObject {
    Q_OBJECT
public:
    explicit VoltsAmpsLog(QObject* parent = nullptr);

    void setFilePath(const QString& path) { m_path = path; }
    QString filePath() const { return m_path; }
    /// The clock the lines are stamped with (UTC). Default: the system's.
    void setClock(std::function<QDateTime()> clock) { m_clock = std::move(clock); }

    /// Thetis LogVA's setter: on writes the header and starts the hour.
    void setEnabled(bool on, const QString& productVersion, const QString& title,
                    double ampVoff, double ampSens);
    bool enabled() const { return m_enabled; }

    /// One PA reading: the raw ADC0 (volts) and ADC1 (amps) samples and
    /// what they scale to. Written at most once a second while on.
    void sample(int adc0, int adc1, double volts, double amps);

signals:
    /// An hour after it was turned on: the log has turned itself off.
    void expired();

private:
    QDateTime now() const;

    QString m_path;
    std::function<QDateTime()> m_clock;
    bool m_enabled = false;
    QDateTime m_lastSave;
    QDateTime m_firstSave;
};

} // namespace NereusSDR
