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
//   2026-09-23 - R-R3-46: save() writes only changed keys, so a remote
//                 window never rewrites the transmit calibration. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 (remote-window parity Task 6): load() announces a
//                 PA forward-power table that changed. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13):
//                 loadTransmitCalibration. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-28 - The volt calibration (AmpVoff / AmpSens): Thetis's
//                 defaults, clamps, per-model default and
//                 initVoltsAmpsCalibration rule, now that the PA current
//                 reading applies it. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - 6 m LNA gain offsets default to Thetis's 13 dB where
//                 nothing is stored. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (found bug): Log Volts/Amps to VALog.txt works:
//                 the controller reads the box (logVoltsAmps), the station's
//                 RadioModel logs through VoltsAmpsLog (Thetis console.cs
//                 LogVA). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - Level Cal: the unused level offset (cal/levelOffset) is
//                 removed; the meter and display offsets live on RadioModel.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "core/HardwareProfile.h"

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

namespace {

// From Thetis console.cs:24949-24958 [v2.10.3.15] AmpSens:
//   float tmp = value;
//   if (tmp < 0.001f) tmp = 0.001f;
double clampAmpSens(double sens)
{
    if (sens < 0.001) { sens = 0.001; }
    return sens;
}

// From Thetis console.cs:24939-24947 [v2.10.3.15] AmpVoff:
//   float tmp = value;
//   if (tmp < 0) tmp = 0.0f;
double clampAmpVoff(double voff)
{
    if (voff < 0) { voff = 0.0; }
    return voff;
}

// Thetis's udAmpSens / udAmpVoff start at 1 (setup.designer.cs:11803-11807,
// 11833-11837 [v2.10.3.15]); a loaded value marks the control set only when
// it differs, since only then does ValueChanged fire (setup.cs:24353-24364).
constexpr double kDesignerAmpValue = 1.0;

} // namespace

// Thetis AmpSens: the PA current reading's sensitivity (mV per amp).
double CalibrationController::paCurrentSensitivity() const
{
    return m_paCurrentSensitivity;
}

void CalibrationController::setPaCurrentSensitivity(double sens)
{
    // udAmpSens_ValueChanged (setup.cs:24359-24364 [v2.10.3.15]):
    //   console.AmpSens = (float)udAmpSens.Value;
    //   _bSensSet = true;
    m_paCurrentSensitivitySet = true;
    sens = clampAmpSens(sens);
    if (m_paCurrentSensitivity == sens) { return; }
    m_paCurrentSensitivity = sens;
    emit changed();
}

// Thetis AmpVoff: the PA current sensor's voltage offset (mV).
double CalibrationController::paCurrentOffset() const
{
    return m_paCurrentOffset;
}

void CalibrationController::setPaCurrentOffset(double offset)
{
    // udAmpVoff_ValueChanged (setup.cs:24353-24358 [v2.10.3.15]):
    //   console.AmpVoff = (float)udAmpVoff.Value;
    //   _bVoffSet = true;
    m_paCurrentOffsetSet = true;
    offset = clampAmpVoff(offset);
    if (m_paCurrentOffset == offset) { return; }
    m_paCurrentOffset = offset;
    emit changed();
}

void CalibrationController::setHardwareModel(HPSDRModel model)
{
    m_hardwareModel = model;
    // The model's defaults stand in until the operator sets a value, as
    // initVoltsAmpsCalibration applies them at start-up (setup.cs:24365-24368
    // [v2.10.3.15]); an edit made since is the operator's and is kept.
    if (!m_paCurrentSensitivitySet && !m_paCurrentOffsetSet) {
        applyModelVoltCalibration();
    }
}

void CalibrationController::restoreDefaultVoltCalibration()
{
    // From Thetis setup.cs:24346-24352 [v2.10.3.15] btnAmpDefault_Click:
    //   (float voff, float sens) = HardwareSpecific.GetDefaultVoltCalibration();
    //   udAmpVoff.Value = (decimal)voff;
    //   udAmpSens.Value = (decimal)sens;
    const VoltCalibration c = defaultVoltCalibrationFor(m_hardwareModel);
    setPaCurrentOffset(static_cast<double>(c.voff));
    setPaCurrentSensitivity(static_cast<double>(c.sens));
}

void CalibrationController::applyModelVoltCalibration()
{
    // btnAmpDefault_Click's values, left unset so a later model (or a
    // saved calibration) still decides; NereusSDR does not save them.
    const VoltCalibration c = defaultVoltCalibrationFor(m_hardwareModel);
    m_paCurrentSensitivitySet = false;
    m_paCurrentOffsetSet = false;
    const double sens = static_cast<double>(c.sens);
    const double voff = static_cast<double>(c.voff);
    if (m_paCurrentSensitivity == sens && m_paCurrentOffset == voff) {
        return;
    }
    m_paCurrentSensitivity = sens;
    m_paCurrentOffset = voff;
    emit changed();
}

void CalibrationController::applyLoadedVoltCalibration(const QString& sensText,
                                                       const QString& voffText)
{
    bool sensOk = false;
    bool voffOk = false;
    const double sens = sensText.toDouble(&sensOk);
    const double voff = voffText.toDouble(&voffOk);
    // getOptions restores the controls, then initVoltsAmpsCalibration
    // (setup.cs:419 and 24365-24368 [v2.10.3.15]) applies the model's
    // defaults unless both were set. Older NereusSDR builds saved
    // sensitivity 1 and offset 0 without applying them; the sensitivity's
    // 1 leaves both on the model's defaults, as Thetis would.
    const bool sensSet = sensOk && sens != kDesignerAmpValue;
    const bool voffSet = voffOk && voff != kDesignerAmpValue;
    if (!sensSet || !voffSet) {
        applyModelVoltCalibration();
        return;
    }
    const double clampedSens = clampAmpSens(sens);
    const double clampedVoff = clampAmpVoff(voff);
    m_paCurrentSensitivitySet = true;
    m_paCurrentOffsetSet = true;
    if (m_paCurrentSensitivity == clampedSens && m_paCurrentOffset == clampedVoff) {
        return;
    }
    m_paCurrentSensitivity = clampedSens;
    m_paCurrentOffset = clampedVoff;
    emit changed();
}

// ── PA forward-power calibration profile ──────────────────────────────────────

// Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
//   wholesale replacement of the per-board cal table (boardClass + 11 watts
//   entries). De-dup on equality so callers can blind-restore a persisted
//   profile without re-firing every `paCalProfileChanged` consumer.
void CalibrationController::setPaCalProfile(const PaCalProfile& p)
{
    if (m_paCalProfile.boardClass == p.boardClass
        && m_paCalProfile.watts    == p.watts) {
        return;
    }
    m_paCalProfile = p;
    emit paCalProfileChanged();
    emit changed();
}

// Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
//   per-spinbox cal-point edit (e.g. setup.cs:5404-5594 ud{N}PA{W}W
//   ValueChanged → CalibratedPAPower table[idx]). Idx 0 is hard-coded
//   0.0 in PaCalProfile so it's never user-editable; out-of-range
//   indices silent no-op rather than asserting (production-safe).
void CalibrationController::setPaCalPoint(int idx, float watts)
{
    if (idx < 1 || idx > 10) { return; }
    if (m_paCalProfile.watts[static_cast<std::size_t>(idx)] == watts) { return; }
    m_paCalProfile.watts[static_cast<std::size_t>(idx)] = watts;
    emit paCalPointChanged(idx, watts);
    emit changed();
}

// Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
//   PowerKernel interpolation forwarder. Implementation lives in
//   PaCalProfile::interpolate (already cited there); this controller owns
//   the table lifecycle + persistence and exposes the read for
//   RadioModel's alex_fwd routing (P1 full-parity plan §3.5).
float CalibrationController::calibratedFwdPowerWatts(float rawAdcWatts) const noexcept
{
    return m_paCalProfile.interpolate(rawAdcWatts);
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
    // Thetis's 13 dB where nothing is stored (setup.designer.cs:12070-12074,
    // 12112-12116 [v2.10.3.15]); a stored value, 0 included, is kept.
    m_rx1_6mLnaOffset        = s.value(base + QStringLiteral("rx1_6mLna"),     QStringLiteral("13")).toDouble();
    m_rx2_6mLnaOffset        = s.value(base + QStringLiteral("rx2_6mLna"),     QStringLiteral("13")).toDouble();
    m_txDisplayOffsetDb      = s.value(base + QStringLiteral("txDisplayOffset"), QStringLiteral("0.0")).toDouble();
    applyLoadedVoltCalibration(s.value(base + QStringLiteral("paSens")).toString(),
                               s.value(base + QStringLiteral("paOffset")).toString());
    // R-R3-49: the Volts/Amps log box.
    readLogVoltsAmps();

    // PA forward-power cal profile under hardware/<mac>/paCalibration/.
    // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
    //   `boardClass` selects the cal-table family; `calPoint{1..10}` are the
    //   user-editable spinbox values. Index 0 is hard-coded to 0.0 in
    //   PaCalProfile (`PaCalProfile.h` "Index 0 is always 0 W"), so we
    //   skip persisting it.  Absent / `None`-class keys leave the profile
    //   default-constructed so `RadioModel::connectToRadio` can substitute
    //   `PaCalProfile::defaults(paCalBoardClassFor(model))` on first connect.
    const QString paBase = QStringLiteral("hardware/%1/paCalibration/").arg(m_mac);
    // R-R3-46 (remote-window parity Task 6): a reload after another
    // window's change announces a PA table that moved, so the Watt Meter
    // page shows it.
    const PaCalProfile before = m_paCalProfile;
    const int storedClassInt = s.value(paBase + QStringLiteral("boardClass"),
                                       QStringLiteral("0")).toInt();
    const auto storedClass = static_cast<PaCalBoardClass>(storedClassInt);
    if (storedClass == PaCalBoardClass::None) {
        m_paCalProfile = PaCalProfile{};
    } else {
        m_paCalProfile = PaCalProfile::defaults(storedClass);
        for (int i = 1; i <= 10; ++i) {
            const QString key = paBase + QStringLiteral("calPoint%1").arg(i);
            const QString fallback = QString::number(m_paCalProfile.watts[
                                          static_cast<std::size_t>(i)]);
            m_paCalProfile.watts[static_cast<std::size_t>(i)] =
                s.value(key, fallback).toFloat();
        }
    }
    if (before.boardClass != m_paCalProfile.boardClass
        || before.watts != m_paCalProfile.watts) {
        emit paCalProfileChanged();
        emit changed();
    }
}

void CalibrationController::setLogVoltsAmps(bool on)
{
    if (m_logVoltsAmps == on) { return; }
    m_logVoltsAmps = on;
    emit logVoltsAmpsChanged(on);
}

void CalibrationController::readLogVoltsAmps()
{
    const QString stored = AppSettings::instance()
        .value(QStringLiteral("hardware/%1/paCalibration/cal/logVoltsAmps").arg(m_mac),
               QStringLiteral("False")).toString();
    setLogVoltsAmps(stored.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0);
}

void CalibrationController::loadTransmitCalibration()
{
    if (m_mac.isEmpty()) { return; }
    // R-R3-49: the Volts/Amps log box, taken on the air as Thetis takes it.
    readLogVoltsAmps();

    // The same keys and defaults load() reads, through the setters, so a
    // change announces itself (changed()).
    auto& s = AppSettings::instance();
    const QString base = QStringLiteral("hardware/%1/cal/").arg(m_mac);
    setTxDisplayOffsetDb(s.value(base + QStringLiteral("txDisplayOffset"),
                                 QStringLiteral("0.0")).toDouble());
    applyLoadedVoltCalibration(s.value(base + QStringLiteral("paSens")).toString(),
                               s.value(base + QStringLiteral("paOffset")).toString());
}

void CalibrationController::save()
{
    if (m_mac.isEmpty()) { return; }

    auto& s = AppSettings::instance();
    const QString base = QStringLiteral("hardware/%1/cal/").arg(m_mac);

    // R-R3-46: write only the keys whose saved value differs, an absent
    // key reading as load()'s default in the form save() writes it (as
    // OcMatrix::save does). The Calibration tab
    // saves after every edit, so in a remote window a frequency calibration
    // change used to rewrite every key on the Core, including the transmit
    // ones a receive-only Core refuses (StationServer's transmit hardware
    // keys). What load() reads back is unchanged.
    const auto store = [&s](const QString& key, const QString& value, const QString& fallback) {
        if (s.value(key, fallback).toString() != value) {
            s.setValue(key, value);
        }
    };
    store(base + QStringLiteral("freqFactor"),      QString::number(m_freqCorrectionFactor, 'g', 15),
          QString::number(1.0, 'g', 15));
    store(base + QStringLiteral("freqFactor10M"),   QString::number(m_freqCorrectionFactor10M, 'g', 15),
          QString::number(1.0, 'g', 15));
    store(base + QStringLiteral("using10M"),        m_using10MHzRef ? QStringLiteral("True") : QStringLiteral("False"),
          QStringLiteral("False"));
    store(base + QStringLiteral("rx1_6mLna"),       QString::number(m_rx1_6mLnaOffset),     QString::number(13.0));
    store(base + QStringLiteral("rx2_6mLna"),       QString::number(m_rx2_6mLnaOffset),     QString::number(13.0));
    store(base + QStringLiteral("txDisplayOffset"), QString::number(m_txDisplayOffsetDb),   QString::number(0.0));
    // The volt calibration once the operator set either value (Thetis saves
    // both controls, so the other one's default is kept with it); the
    // model's defaults alone stay unsaved so they follow the model.
    if (m_paCurrentSensitivitySet || m_paCurrentOffsetSet) {
        store(base + QStringLiteral("paSens"),   QString::number(m_paCurrentSensitivity), QString());
        store(base + QStringLiteral("paOffset"), QString::number(m_paCurrentOffset),      QString());
    }

    // PA forward-power cal profile -- see load() for schema.
    // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13]
    const QString paBase = QStringLiteral("hardware/%1/paCalibration/").arg(m_mac);
    const QString classKey = paBase + QStringLiteral("boardClass");
    const auto storedClass = static_cast<PaCalBoardClass>(
        s.value(classKey, QStringLiteral("0")).toInt());
    const bool classChanged = storedClass != m_paCalProfile.boardClass;
    store(classKey, QString::number(static_cast<int>(m_paCalProfile.boardClass)),
          QStringLiteral("0"));
    // load() reads the points only under a stored class, falling back to
    // that class's factory table; a new class writes its whole table.
    const PaCalProfile storedDefaults = PaCalProfile::defaults(storedClass);
    for (int i = 1; i <= 10; ++i) {
        const QString key = paBase + QStringLiteral("calPoint%1").arg(i);
        const QString value = QString::number(m_paCalProfile.watts[static_cast<std::size_t>(i)]);
        if (classChanged) {
            s.setValue(key, value);
        } else if (m_paCalProfile.boardClass != PaCalBoardClass::None) {
            store(key, value,
                  QString::number(storedDefaults.watts[static_cast<std::size_t>(i)]));
        }
    }
}

} // namespace NereusSDR
