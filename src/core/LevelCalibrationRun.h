// =================================================================
// src/core/LevelCalibrationRun.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.15]:
//   Project Files/Source/Console/console.cs
//     CalibrateLevel (9856-10232), btnZeroBeat_Click (36095-36182),
//     FindPeakFreqInPassband (36184-36258)
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Level Cal: Thetis's receive level
//                calibration as a Core procedure. Thread.Sleep waits
//                become QTimer steps, the progress window becomes the
//                progress signal and cancel(), and the receiver it acts
//                on is reached through LevelCalibrationHost so the same
//                run serves the Core and a fake meter in tests.
//   2026-09-29 - Level Cal fix wave: RX2's preamp is its own mode, set
//                through the controller's RX2PreampMode port. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>
#include <vector>

#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
#include "core/WdspTypes.h"

namespace NereusSDR {

// One spectrum frame: |X|^2 per bin (FFT-shifted, bin N/2 is centreHz),
// the frame's centre and its sample rate. Thetis's SnapSpectrum gives
// re and im; the run only uses re^2 + im^2, which this is.
struct LevelCalSpectrum {
    QVector<float> binsLinear;
    double centreHz = 0.0;
    double sampleRateHz = 0.0;
};

// The receiver a level calibration runs on. Thetis reaches the console's
// own controls; NereusSDR reaches the calibrated slice, the radio's step
// attenuator and preamp, and the station's calibration through this.
class LevelCalibrationHost {
public:
    virtual ~LevelCalibrationHost() = default;

    virtual bool radioLive() const = 0;
    virtual bool transmitting() const = 0;
    virtual bool alexPresent() const = 0;
    virtual HPSDRModel model() const = 0;

    // The calibrated slice (Thetis VFO A, RX1).
    virtual double frequencyHz() const = 0;
    virtual void setFrequencyHz(double hz) = 0;
    virtual bool ritEnabled() const = 0;
    virtual int ritOffsetHz() const = 0;
    virtual void setRit(bool on, int hz) = 0;
    virtual DSPMode dspMode() const = 0;
    virtual void setDspMode(DSPMode mode) = 0;
    virtual int filterLowHz() const = 0;
    virtual int filterHighHz() const = 0;

    // Thetis SetupForm.DSPPhoneRXBuffer.
    virtual int phoneRxBuffer() const = 0;
    virtual void setPhoneRxBuffer(int size) = 0;

    // Thetis SetupForm.RX1EnableAtt / RX2EnableAtt.
    virtual bool rx1StepAttEnabled() const = 0;
    virtual bool rx2StepAttEnabled() const = 0;
    virtual void setStepAttEnabled(bool rx1, bool rx2) = 0;
    // Thetis RX1PreampMode and RX2PreampMode, each set through its own
    // setter's model gate (StepAttenuatorController).
    virtual PreampMode rx1PreampMode() const = 0;
    virtual void setRx1PreampMode(PreampMode mode) = 0;
    virtual PreampMode rx2PreampMode() const = 0;
    virtual void setRx2PreampMode(PreampMode mode) = 0;

    // The station's meter and display calibration. The override is the
    // saved value (nullopt: none saved, the radio's default applies);
    // the Db reading is what is in effect.
    virtual std::optional<double> meterCalOverride() const = 0;
    virtual std::optional<double> displayCalOverride() const = 0;
    virtual double meterCalDb() const = 0;
    virtual double displayCalDb() const = 0;
    virtual void setMeterCalOverride(std::optional<double> db) = 0;
    virtual void setDisplayCalOverride(std::optional<double> db) = 0;
    virtual float rx1PreampOffsetDb(PreampMode mode) const = 0;
    virtual void setRx1PreampOffsetDb(PreampMode mode, float db) = 0;
    virtual void setRx2PreampOffsetDb(PreampMode mode, float db) = 0;

    // WDSP.CalculateRXMeter(0, 0, AVG_SIGNAL_STRENGTH): the raw average
    // signal reading, no calibration added.
    virtual float readSignalAverage() = 0;
    // The latest spectrum of the calibrated slice's receiver, or nullopt.
    virtual std::optional<LevelCalSpectrum> latestSpectrum() = 0;
};

class LevelCalibrationRun : public QObject {
    Q_OBJECT
public:
    // Thetis's Thread.Sleep values, in milliseconds.
    struct Timings {
        int settleBeforeZeroBeatMs = 2000;  // Thread.Sleep(2000)
        int snapGapMs = 20;                 // Thread.Sleep(20)
        int meterPreWaitMs = 1000;          // Thread.Sleep(1000)
        int meterReadGapMs = 50;            // Thread.Sleep(50)
        int preampSwitchMs = 100;           // Thread.Sleep(100)
        int finalSettleMs = 5000;           // Thread.Sleep(5000)
    };

    explicit LevelCalibrationRun(LevelCalibrationHost* host, QObject* parent = nullptr);
    ~LevelCalibrationRun() override;

    void setTimings(const Timings& timings) { m_timings = timings; }

    // Starts a run for a carrier of `levelDbm` at `frequencyHz`. Returns
    // an empty string when it started, or the reason it did not.
    QString start(float levelDbm, double frequencyHz);
    // Stops a run and puts everything back (Thetis closing the progress
    // window). No-op when nothing runs.
    void cancel();
    bool isRunning() const { return m_running; }

    // Whether Thetis also measures the -10 to -50 settings on this radio.
    static bool measuresAlexSteps(bool alexPresent, HPSDRModel model);

signals:
    // Thetis progress.SetPercent(counter / progress_divisor), as percent.
    void progress(int percent);
    void finished(bool ok, const QString& message);

private:
    struct SavedState {
        double vfoHz = 0.0;
        bool ritOn = false;
        int ritHz = 0;
        DSPMode mode = DSPMode::AM;
        int phoneRxBuffer = 0;
        bool rx1Att = false;
        bool rx2Att = false;
        PreampMode rx1Preamp = PreampMode::On;
        PreampMode rx2Preamp = PreampMode::On;
        std::optional<double> meterCal;
        std::optional<double> displayCal;
    };

    struct Step {
        int delayMs = 0;
        std::function<void()> action;
    };

    void buildSteps();
    void runStep(int index, quint64 generation);
    void fail(const QString& message);
    void finish(bool ok, const QString& message);
    void restore(bool ok);
    void zeroBeat();
    bool snapSearch(double calRangeHz, bool logPower);
    void addMeterSteps(float* target);
    int centreBin(const LevelCalSpectrum& s) const;

    LevelCalibrationHost* m_host = nullptr;
    Timings m_timings;
    bool m_running = false;
    quint64 m_generation = 0;
    std::vector<Step> m_steps;
    SavedState m_saved;

    float m_level = 0.0f;
    double m_frequencyHz = 0.0;
    int m_counter = 0;
    int m_progressDivisor = 120;
    int m_fftSize = 0;
    std::vector<double> m_sum;
    int m_searchOffset = 0;
    int m_searchCentre = 0;
    float m_num = 0.0f;
    float m_avg = 0.0f;
    float m_avg2 = 0.0f;
    QString m_failure;
};

} // namespace NereusSDR
