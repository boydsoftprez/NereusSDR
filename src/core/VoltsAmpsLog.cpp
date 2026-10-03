// =================================================================
// src/core/VoltsAmpsLog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:24798-24870, :24892-24916
//   original licence from Thetis source is included below
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


#include "core/VoltsAmpsLog.h"

#include <QFile>
#include <QTextStream>

namespace NereusSDR {

VoltsAmpsLog::VoltsAmpsLog(QObject* parent) : QObject(parent) {}

QDateTime VoltsAmpsLog::now() const
{
    return m_clock ? m_clock() : QDateTime::currentDateTimeUtc();
}

// From Thetis console.cs:24892-24916 [v2.10.3.15] LogVA
//   if (value)
//   {
//       string sVALog = Path.Combine(AppDataPath, "VALog.txt");
//       using (StreamWriter writer = File.AppendText(sVALog))
//       {
//           writer.WriteLine(ProductVersion + "\n" + BasicTitleBar);
//           writer.WriteLine($"Volts/Amps Log \t_amp_voff={_amp_voff}\t_amp_sens={_amp_sens}");
//       }
//       _firstSaveTime = DateTime.UtcNow;
//   }
//   _logVA = value;
void VoltsAmpsLog::setEnabled(bool on, const QString& productVersion, const QString& title,
                              double ampVoff, double ampSens)
{
    if (on && !m_enabled) {
        QFile file(m_path);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << productVersion << '\n' << title << '\n';
            out << QStringLiteral("Volts/Amps Log \t_amp_voff=%1\t_amp_sens=%2")
                       .arg(QString::number(ampVoff), QString::number(ampSens))
                << '\n';
        }
        m_firstSave = now();
        m_lastSave = QDateTime();
    }
    m_enabled = on;
}

// From Thetis console.cs:24838-24870 [v2.10.3.15] readMKIIPAVoltsAmps
//   // [2.10.1.0]MW0LGE log data to VALog.txt
//   if (_logVA)
//   {
//       DateTime now = DateTime.UtcNow;
//       if (now.Subtract(_lastSaveTime).TotalSeconds >= 1)
//       {
//           ... writer.WriteLine($"{now:yyyy-MM-dd HH:mm:ss}\tadc0(v)={adc0}\tadc1(a)={adc1}\tvolts={convertToVolts(adc0).ToString("f2")}\tamps={convertToAmps(adc1).ToString("f2")}");
//           ... finally { _lastSaveTime = DateTime.UtcNow; }
//           if (now.Subtract(_firstSaveTime).TotalMinutes >= 60)
//           {
//               // we left it on, turn it off
//               if (!IsSetupFormNull) SetupForm.LogVA = false;
//           }
//       }
//   }
void VoltsAmpsLog::sample(int adc0, int adc1, double volts, double amps)
{
    if (!m_enabled) {
        return;
    }
    const QDateTime at = now();
    if (m_lastSave.isValid() && m_lastSave.msecsTo(at) < 1000) {
        return;
    }
    QFile file(m_path);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << at.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            << QStringLiteral("\tadc0(v)=%1\tadc1(a)=%2\tvolts=%3\tamps=%4")
                   .arg(adc0)
                   .arg(adc1)
                   .arg(QString::number(volts, 'f', 2), QString::number(amps, 'f', 2))
            << '\n';
    }
    m_lastSave = at;
    // we left it on, turn it off  [original comment from console.cs:24864]
    if (m_firstSave.isValid() && m_firstSave.secsTo(at) >= 60 * 60) {
        m_enabled = false;
        emit expired();
    }
}

} // namespace NereusSDR
