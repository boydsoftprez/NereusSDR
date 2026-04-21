// =================================================================
// src/core/CalibrationController.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs (udHPSDRFreqCorrectFactor,
//     chkUsing10MHzRef, udHPSDRFreqCorrectFactor10MHz, udTXDisplayCalOffset,
//     ud6mLNAGainOffset, ud6mRx2LNAGainOffset — lines 5137-5144; 14036-14050;
//     22690-22706; 14325-14333; 17243-17248; 18315-18317),
//     original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs (CalibrateFreq, CalibrateLevel,
//     RXCalibrationOffset — lines 9764-9844; 21022-21086),
//     original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

#include "CalibrationController.h"

#include "core/AppSettings.h"

namespace NereusSDR {

// ── Constructor ───────────────────────────────────────────────────────────────

CalibrationController::CalibrationController(QObject* parent)
    : QObject(parent)
{
}

// ── Frequency correction factor ───────────────────────────────────────────────

// Source: setup.cs:5137-5144 HPSDRFreqCorrectFactorViaAutoCalibration [@501e3f5]
double CalibrationController::freqCorrectionFactor() const
{
    return m_freqCorrectionFactor;
}

void CalibrationController::setFreqCorrectionFactor(double factor)
{
    if (m_freqCorrectionFactor == factor) { return; }
    m_freqCorrectionFactor = factor;
    emit changed();
}

// Source: setup.cs:22704 udHPSDRFreqCorrectFactor10MHz_ValueChanged [@501e3f5]
double CalibrationController::freqCorrectionFactor10M() const
{
    return m_freqCorrectionFactor10M;
}

void CalibrationController::setFreqCorrectionFactor10M(double factor)
{
    if (m_freqCorrectionFactor10M == factor) { return; }
    m_freqCorrectionFactor10M = factor;
    emit changed();
}

// Source: setup.cs:22690-22696 chkUsing10MHzRef_CheckedChanged
//   udHPSDRFreqCorrectFactor10MHz.Enabled = chkUsing10MHzRef.Checked [@501e3f5]
bool CalibrationController::using10MHzRef() const
{
    return m_using10MHzRef;
}

void CalibrationController::setUsing10MHzRef(bool on)
{
    if (m_using10MHzRef == on) { return; }
    m_using10MHzRef = on;
    emit changed();
}

// Effective factor: picks 10 MHz factor when using 10 MHz ref, otherwise normal.
// Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged:
//   if (!_freqCorrectFactorChangedViaAutoCalibration || !chkUsing10MHzRef.Checked)
//     NetworkIO.FreqCorrectionFactor = (double)udHPSDRFreqCorrectFactor.Value;
//   else if (chkUsing10MHzRef.Checked)
//     NetworkIO.FreqCorrectionFactor = (double)udHPSDRFreqCorrectFactor10MHz.Value;
//   [@501e3f5]
double CalibrationController::effectiveFreqCorrectionFactor() const
{
    return m_using10MHzRef ? m_freqCorrectionFactor10M : m_freqCorrectionFactor;
}

// ── Level calibration offset ──────────────────────────────────────────────────

// Source: console.cs:21074-21086 _rx1_display_cal_offset
//   RXCalibrationOffset(int rx) [@501e3f5]
double CalibrationController::levelOffsetDb() const
{
    return m_levelOffsetDb;
}

void CalibrationController::setLevelOffsetDb(double db)
{
    if (m_levelOffsetDb == db) { return; }
    m_levelOffsetDb = db;
    emit changed();
}

// ── RX1 / RX2 6m LNA gain offsets ────────────────────────────────────────────

// Source: setup.cs:17243-17248 ud6mLNAGainOffset → console.RX6mGainOffset_RX1 [@501e3f5]
double CalibrationController::rx1_6mLnaOffset() const
{
    return m_rx1_6mLnaOffset;
}

void CalibrationController::setRx1_6mLnaOffset(double db)
{
    if (m_rx1_6mLnaOffset == db) { return; }
    m_rx1_6mLnaOffset = db;
    emit changed();
}

// Source: setup.cs:18315-18317 ud6mRx2LNAGainOffset → console.RX6mGainOffset_RX2 [@501e3f5]
double CalibrationController::rx2_6mLnaOffset() const
{
    return m_rx2_6mLnaOffset;
}

void CalibrationController::setRx2_6mLnaOffset(double db)
{
    if (m_rx2_6mLnaOffset == db) { return; }
    m_rx2_6mLnaOffset = db;
    emit changed();
}

// ── TX display calibration offset ─────────────────────────────────────────────

// Source: setup.cs:14325-14328 udTXDisplayCalOffset → Display.TXDisplayCalOffset [@501e3f5]
double CalibrationController::txDisplayOffsetDb() const
{
    return m_txDisplayOffsetDb;
}

void CalibrationController::setTxDisplayOffsetDb(double db)
{
    if (m_txDisplayOffsetDb == db) { return; }
    m_txDisplayOffsetDb = db;
    emit changed();
}

// ── PA current calculation parameters ─────────────────────────────────────────

// Source: console.cs:6691-6724 CalibratedPAPower — sensitivity constant [@501e3f5]
double CalibrationController::paCurrentSensitivity() const
{
    return m_paCurrentSensitivity;
}

void CalibrationController::setPaCurrentSensitivity(double sens)
{
    if (m_paCurrentSensitivity == sens) { return; }
    m_paCurrentSensitivity = sens;
    emit changed();
}

double CalibrationController::paCurrentOffset() const
{
    return m_paCurrentOffset;
}

void CalibrationController::setPaCurrentOffset(double offset)
{
    if (m_paCurrentOffset == offset) { return; }
    m_paCurrentOffset = offset;
    emit changed();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void CalibrationController::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

void CalibrationController::load()
{
    if (m_mac.isEmpty()) { return; }

    auto& s = AppSettings::instance();
    const QString base = QStringLiteral("hardware/%1/cal/").arg(m_mac);

    m_freqCorrectionFactor   = s.value(base + QStringLiteral("freqFactor"),   QStringLiteral("1.0")).toDouble();
    m_freqCorrectionFactor10M = s.value(base + QStringLiteral("freqFactor10M"), QStringLiteral("1.0")).toDouble();
    m_using10MHzRef          = s.value(base + QStringLiteral("using10M"),      QStringLiteral("False")).toString() == QStringLiteral("True");
    m_levelOffsetDb          = s.value(base + QStringLiteral("levelOffset"),   QStringLiteral("0.0")).toDouble();
    m_rx1_6mLnaOffset        = s.value(base + QStringLiteral("rx1_6mLna"),     QStringLiteral("0.0")).toDouble();
    m_rx2_6mLnaOffset        = s.value(base + QStringLiteral("rx2_6mLna"),     QStringLiteral("0.0")).toDouble();
    m_txDisplayOffsetDb      = s.value(base + QStringLiteral("txDisplayOffset"), QStringLiteral("0.0")).toDouble();
    m_paCurrentSensitivity   = s.value(base + QStringLiteral("paSens"),        QStringLiteral("1.0")).toDouble();
    m_paCurrentOffset        = s.value(base + QStringLiteral("paOffset"),      QStringLiteral("0.0")).toDouble();
}

void CalibrationController::save()
{
    if (m_mac.isEmpty()) { return; }

    auto& s = AppSettings::instance();
    const QString base = QStringLiteral("hardware/%1/cal/").arg(m_mac);

    s.setValue(base + QStringLiteral("freqFactor"),      QString::number(m_freqCorrectionFactor, 'g', 15));
    s.setValue(base + QStringLiteral("freqFactor10M"),   QString::number(m_freqCorrectionFactor10M, 'g', 15));
    s.setValue(base + QStringLiteral("using10M"),        m_using10MHzRef ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(base + QStringLiteral("levelOffset"),     QString::number(m_levelOffsetDb));
    s.setValue(base + QStringLiteral("rx1_6mLna"),       QString::number(m_rx1_6mLnaOffset));
    s.setValue(base + QStringLiteral("rx2_6mLna"),       QString::number(m_rx2_6mLnaOffset));
    s.setValue(base + QStringLiteral("txDisplayOffset"), QString::number(m_txDisplayOffsetDb));
    s.setValue(base + QStringLiteral("paSens"),          QString::number(m_paCurrentSensitivity));
    s.setValue(base + QStringLiteral("paOffset"),        QString::number(m_paCurrentOffset));
}

} // namespace NereusSDR
