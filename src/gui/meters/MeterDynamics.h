#pragma once
// =================================================================
// MeterDynamics (NereusSDR)
// Ported from Thetis Project Files/Source/Console/MeterManager.cs.
// Modification history (NereusSDR):
//   2026-10-02 — C++20 timestamp scheduling by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via OpenAI Codex.
// =================================================================
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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


#include <QtGlobal>
#include <QVector>
namespace NereusSDR {
// One presentation channel, no timer or source ownership. Call once for each
// shared frame. Missed frames do not manufacture historical samples.
class MeterDynamics {
public:
    void configure(double attack, double release, int intervalMs,
                   int historyMs, int ignoreHistoryMs);
    void reset(double minimum);
    void clearHistory();
    void push(double reading, bool available = true);
    bool advance(qint64 monotonicMs);
    double value() const { return m_value; }
    bool hasReading() const { return m_available; }
    double minHistory() const;
    double maxHistory() const;
    int historySize() const { return m_history.size(); }
private:
    struct Sample { qint64 time; double value; };
    QVector<Sample> m_history;
    double m_attack{0.8}, m_release{0.1}, m_value{-30}, m_minimum{-30}, m_input{-30};
    int m_interval{100}, m_historyMs{2000}, m_ignoreMs{2000}, m_ignoreSamples{0};
    qint64 m_lastTime{-1};
    bool m_available{false};
};
}
