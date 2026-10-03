// =================================================================
// src/core/LevelCalibrationRun.cpp  (NereusSDR)
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
//                calibration as a Core procedure. Differences from Thetis,
//                each forced by the Core having no console form:
//                - Thread.Sleep waits are QTimer steps; the progress
//                  window is the progress signal, and cancel() stands for
//                  closing it. Cancel, transmitting and the radio going
//                  away are checked before every step, not only after
//                  each meter reading.
//                - The Core has no CTUN, so the spectrum searches centre
//                  on the calibrated slice's bin, which is Thetis's
//                  click-tune branch of FindPeakFreqInPassband.
//                - A missing spectrum fails the run in plain words where
//                  Thetis's blocking SnapSpectrum waits for one.
//                - NereusSDR keeps one meter and one display calibration,
//                  so Thetis's separate RX2 calibration terms are not
//                  ported. RX2's OFF / ON preamp offsets are.
//                - Meter mode, display mode and the grid's noise floor
//                  follow are window settings; a window saves and restores
//                  its own around a run.
//   2026-09-29 - Level Cal fix wave: RX2's preamp is its own mode, set
//                through the controller's RX2PreampMode port. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - runStep() runs a copy of the step, so a step that ends
//                the run (cancel() from a host call) is not destroyed while
//                it runs (CI: SIGSEGV in tst_level_calibration_run on
//                Linux). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
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

#include "core/LevelCalibrationRun.h"

#include <QTimer>

#include <cmath>
#include <limits>

namespace NereusSDR {

namespace {

// From Thetis console.cs:9883 [v2.10.3.15]
//   int iterations = 50; // number of samples to average //[2.10.3.9]MW0LGE changed from 20 to 50
constexpr int kIterations = 50;
// From Thetis console.cs:9995-10003 [v2.10.3.15]: 50 meter readings per setting.
constexpr int kMeterReads = 50;
// From Thetis console.cs:9900 [v2.10.3.15]
//   SetupForm.DSPPhoneRXBuffer = 16384; // set DSP Buffer Size to 16384
constexpr int kCalBufferSize = 16384;
// From Thetis console.cs:9925-9932 [v2.10.3.15]: progress_divisor 390 with
// Alex, 120 without.
constexpr int kProgressDivisorAlex = 390;
constexpr int kProgressDivisorNoAlex = 120;
// From Thetis console.cs:9943 [v2.10.3.15]
//   double cal_range = 20000.0; // look +/- this much from current freq
constexpr double kSearchRangeHz = 20000.0;
// From Thetis console.cs:10154 [v2.10.3.15]
//   cal_range = 2500.0;
constexpr double kPeakRangeHz = 2500.0;
// From Thetis console.cs:9973 [v2.10.3.15]: the peak must be 30 dB above
// the average bin.
constexpr double kMinPeakOverNoiseDb = 30.0;

QString radioOffMessage()
{
    // Thetis: "Power must be on in order to calibrate RX Level."
    return QStringLiteral("Turn the radio on before calibrating the receive level.");
}

QString transmittingMessage()
{
    return QStringLiteral("Stop transmitting before calibrating the receive level.");
}

QString noSpectrumMessage()
{
    return QStringLiteral(
        "No spectrum from the receiver. Check that the radio is running, then try again.");
}

QString outsideMessage()
{
    return QStringLiteral(
        "The calibration frequency is too close to the edge of the receiver's spectrum. "
        "Center the display on it, then try again.");
}

QString weakSignalMessage()
{
    // Thetis: "Peak is less than 30dB from the noise floor.  Please use a
    // larger signal for frequency calibration."
    return QStringLiteral(
        "The signal peak is less than 30 dB above the noise floor. "
        "Use a stronger signal for level calibration.");
}

} // namespace

LevelCalibrationRun::LevelCalibrationRun(LevelCalibrationHost* host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
}

LevelCalibrationRun::~LevelCalibrationRun()
{
    // A run left going puts the receiver back, as Thetis's end: label does.
    cancel();
}

// From Thetis console.cs:10031-10041 [v2.10.3.15]
//   if (alexpresent &&
//       HardwareSpecific.Model != HPSDRModel.ANAN10 && ... ANAN10E ...
//       ANAN7000D ... ANAN8000D ... ORIONMKII ...
//       HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
//       ... ANAN_G2 ... ANAN_G2_1K ... ANVELINAPRO3 ...
//       HardwareSpecific.Model != HPSDRModel.REDPITAYA) //DH1KLM
bool LevelCalibrationRun::measuresAlexSteps(bool alexPresent, HPSDRModel model)
{
    return alexPresent
        && model != HPSDRModel::ANAN10
        && model != HPSDRModel::ANAN10E
        && model != HPSDRModel::ANAN7000D
        && model != HPSDRModel::ANAN8000D
        && model != HPSDRModel::ORIONMKII
        && model != HPSDRModel::ANAN_G2E  //N1GP G2E added
        && model != HPSDRModel::ANAN_G2
        && model != HPSDRModel::ANAN_G2_1K
        && model != HPSDRModel::ANVELINAPRO3
        && model != HPSDRModel::REDPITAYA; //DH1KLM
}

QString LevelCalibrationRun::start(float levelDbm, double frequencyHz)
{
    if (m_host == nullptr) {
        return radioOffMessage();
    }
    if (m_running) {
        return QStringLiteral("Level calibration is already running.");
    }
    // From Thetis console.cs:9861 [v2.10.3.15]: if (!chkPower.Checked)
    // refuse with "Power must be on in order to calibrate RX Level."
    if (!m_host->radioLive()) {
        return radioOffMessage();
    }
    if (m_host->transmitting()) {
        return transmittingMessage();
    }

    m_level = levelDbm;
    m_frequencyHz = frequencyHz;
    m_counter = 0;
    m_fftSize = 0;
    m_sum.clear();
    m_num = 0.0f;
    m_avg = 0.0f;
    m_avg2 = 0.0f;
    m_failure.clear();

    // From Thetis console.cs:9887-9923 [v2.10.3.15]: save, then set up.
    // int iterations = 50; // number of samples to average //[2.10.3.9]MW0LGE changed from 20 to 50
    //   [original inline comment from console.cs:9883; kIterations above]
    m_saved.vfoHz = m_host->frequencyHz();
    m_saved.ritOn = m_host->ritEnabled();
    m_saved.ritHz = m_host->ritOffsetHz();
    m_saved.phoneRxBuffer = m_host->phoneRxBuffer();
    m_saved.mode = m_host->dspMode();
    m_saved.rx1Att = m_host->rx1StepAttEnabled();
    m_saved.rx2Att = m_host->rx2StepAttEnabled();  //MW0LGE_[2.9.0.6]
    m_saved.rx1Preamp = m_host->rx1PreampMode();
    m_saved.rx2Preamp = m_host->rx2PreampMode();
    m_saved.meterCal = m_host->meterCalOverride();
    m_saved.displayCal = m_host->displayCalOverride();

    m_running = true;
    ++m_generation;

    m_host->setRit(false, m_saved.ritHz);          // turn RIT off
    m_host->setPhoneRxBuffer(kCalBufferSize);      // set DSP Buffer Size to 16384
    m_host->setDspMode(DSPMode::AM);
    m_host->setFrequencyHz(frequencyHz);           // set VFOA frequency
    m_host->setStepAttEnabled(false, false);       // SetupForm.RX2EnableAtt = false; //MW0LGE_[2.9.0.6]
    m_host->setRx1PreampMode(PreampMode::On);      // set to high
    m_host->setRx2PreampMode(PreampMode::On);      // RX2PreampMode = PreampMode.HPSDR_ON;        //MW0LGE_[2.9.0.6]

    m_progressDivisor = m_host->alexPresent() ? kProgressDivisorAlex : kProgressDivisorNoAlex;

    buildSteps();
    emit progress(0);  // progress.SetPercent(0.0f)
    const quint64 generation = m_generation;
    QTimer::singleShot(m_steps.front().delayMs, this,
                       [this, generation]() { runStep(0, generation); });
    return {};
}

void LevelCalibrationRun::cancel()
{
    if (m_running) {
        finish(false, QStringLiteral("Level calibration was canceled."));
    }
}

void LevelCalibrationRun::runStep(int index, quint64 generation)
{
    if (!m_running || generation != m_generation) {
        return;
    }
    if (!m_host->radioLive()) {
        fail(QStringLiteral("Level calibration stopped because the radio was disconnected."));
        return;
    }
    if (m_host->transmitting()) {
        fail(QStringLiteral("Level calibration stopped because the radio started transmitting."));
        return;
    }
    // A step can end the run while it runs: a host call it makes can
    // cancel(), and finish() clears m_steps, destroying the closure being
    // executed. Run a copy so the step outlives that (Linux CI: a SIGSEGV
    // in cancel_restoresEverything, where libstdc++ keeps the closure on
    // the heap).
    const std::function<void()> action = m_steps[size_t(index)].action;
    action();
    if (!m_running || generation != m_generation) {
        return;
    }
    if (!m_failure.isEmpty()) {
        fail(m_failure);
        return;
    }
    const int next = index + 1;
    if (next >= int(m_steps.size())) {
        return;
    }
    QTimer::singleShot(m_steps[size_t(next)].delayMs, this,
                       [this, next, generation]() { runStep(next, generation); });
}

void LevelCalibrationRun::fail(const QString& message)
{
    finish(false, message);
}

void LevelCalibrationRun::finish(bool ok, const QString& message)
{
    if (!m_running) {
        return;
    }
    m_running = false;
    ++m_generation;
    restore(ok);
    m_steps.clear();
    emit finished(ok, message);
}

// From Thetis console.cs:10194-10232 [v2.10.3.15], the end: label.
void LevelCalibrationRun::restore(bool ok)
{
    if (!ok) {
        m_host->setMeterCalOverride(m_saved.meterCal);
        m_host->setDisplayCalOverride(m_saved.displayCal);
    }
    m_host->setRit(m_saved.ritOn, m_saved.ritHz);        // restore RIT on / value
    m_host->setRx1PreampMode(m_saved.rx1Preamp);         // restore preamp value
    m_host->setRx2PreampMode(m_saved.rx2Preamp);         // restore preamp value MW0LGE_[2.9.0.6]
    m_host->setStepAttEnabled(m_saved.rx1Att, m_saved.rx2Att);  //MW0LGE_[2.9.0.6]
    m_host->setDspMode(m_saved.mode);                    // restore DSP mode
    m_host->setPhoneRxBuffer(m_saved.phoneRxBuffer);     // restore DSP Buffer Size
    m_host->setFrequencyHz(m_saved.vfoHz);               // restore vfo frequency
}

int LevelCalibrationRun::centreBin(const LevelCalSpectrum& s) const
{
    const int fft = int(s.binsLinear.size());
    const double hzPerBucket = s.sampleRateHz / double(fft);
    // From Thetis console.cs:36197-36201 [v2.10.3.15]
    //   if (_click_tune_display) //MW0LGE_21d
    //       zero_hz_bucket += (int)dBucketOffset;
    return fft / 2 + int((m_host->frequencyHz() - s.centreHz) / hzPerBucket);
}

// From Thetis console.cs:36095-36258 [v2.10.3.15]: btnZeroBeat_Click in AM
// The comment just above btnZeroBeat_Click in Thetis belongs to the XIT
// MIDI handler, kept for attribution: //-W2PA Prevent the lowest LED from
// going out completely.  [original inline comment from console.cs:36090]
// The spectrum snap: //[2.10.2.3]MW0LGE timeout version used, with 10 times
// frame rate to give some additional time  [console.cs:36230; here the
// latest frame is taken]
// (delta_hz = peak_hz) with FindPeakFreqInPassband. Any failure leaves the
// VFO where it is, as Thetis's early returns do.
void LevelCalibrationRun::zeroBeat()
{
    if (!m_host->radioLive()) {
        return;
    }
    const std::optional<LevelCalSpectrum> s = m_host->latestSpectrum();
    if (!s || s->binsLinear.isEmpty() || s->sampleRateHz <= 0.0) {
        return;  // if (flag == 0) return -1;
    }
    const int fft = int(s->binsLinear.size());
    const double hzPerBucket = s->sampleRateHz / double(fft);
    const int zeroBucket = centreBin(*s);
    const int loBucket = int(m_host->filterLowHz() / hzPerBucket) + zeroBucket;
    const int hiBucket = int(m_host->filterHighHz() / hzPerBucket) + zeroBucket;
    //MW0LGE_21d belts and braces
    if (loBucket < 0 || hiBucket > fft - 1) {
        return;
    }
    double maxVal = std::numeric_limits<double>::lowest();
    int maxBucket = 0;
    for (int i = loBucket; i <= hiBucket; ++i) {
        const double magSqr = s->binsLinear[i];
        if (magSqr > maxVal) {
            maxBucket = i;
            maxVal = magSqr;
        }
    }
    const int peakHz = int((maxBucket - zeroBucket) * hzPerBucket);
    m_host->setFrequencyHz(m_host->frequencyHz() + peakHz);
}

// One SnapSpectrum pass of console.cs 9953-9960 (logPower: sum of dB) or
// 10160-10165 (sum of magnitude squared), Thetis v2.10.3.15.
bool LevelCalibrationRun::snapSearch(double calRangeHz, bool logPower)
{
    const std::optional<LevelCalSpectrum> s = m_host->latestSpectrum();
    if (!s || s->binsLinear.isEmpty() || s->sampleRateHz <= 0.0) {
        m_failure = noSpectrumMessage();
        return false;
    }
    const int fft = int(s->binsLinear.size());
    if (m_fftSize == 0) {
        m_fftSize = fft;
        m_sum.assign(size_t(fft), 0.0);
    } else if (fft != m_fftSize) {
        m_failure = noSpectrumMessage();
        return false;
    }
    const double binWidth = s->sampleRateHz / double(fft);
    const int offset = int(calRangeHz / binWidth);
    const int centre = centreBin(*s);
    if (centre - offset < 0 || centre + offset > fft - 1) {
        m_failure = outsideMessage();
        return false;
    }
    m_searchOffset = offset;
    m_searchCentre = centre;
    for (int j = centre - offset; j <= centre + offset; ++j) {
        const double p = s->binsLinear[j];
        m_sum[size_t(j)] += logPower ? 10.0 * std::log10(p) : p;
    }
    return true;
}

void LevelCalibrationRun::addMeterSteps(float* target)
{
    // Thread.Sleep(1000), then 50 readings with Thread.Sleep(50) after each
    // and progress.SetPercent(++counter / progress_divisor).
    for (int i = 0; i < kMeterReads; ++i) {
        Step step;
        step.delayMs = (i == 0) ? m_timings.meterPreWaitMs : m_timings.meterReadGapMs;
        step.action = [this, i, target]() {
            if (i == 0) {
                m_num = 0.0f;
            }
            m_num += m_host->readSignalAverage();
            if (!m_running) {
                return;
            }
            ++m_counter;
            emit progress(m_counter * 100 / m_progressDivisor);
            if (i == kMeterReads - 1) {
                *target = m_num / float(kMeterReads);
            }
        };
        m_steps.push_back(std::move(step));
    }
}

void LevelCalibrationRun::buildSteps()
{
    m_steps.clear();

    // Thread.Sleep(2000); btnZeroBeat_Click(this, EventArgs.Empty);
    m_steps.push_back({m_timings.settleBeforeZeroBeatMs, [this]() { zeroBeat(); }});

    // From Thetis console.cs:9943-9984 [v2.10.3.15]: 50 spectra summed in
    // dB over +/- 20 kHz, the peak bin against the average bin.
    for (int i = 0; i < kIterations; ++i) {
        m_steps.push_back({i == 0 ? 0 : m_timings.snapGapMs,
                           [this]() { snapSearch(kSearchRangeHz, true); }});
    }
    m_steps.push_back({m_timings.snapGapMs, [this]() {
        const int lo = m_searchCentre - m_searchOffset;
        const int hi = m_searchCentre + m_searchOffset;
        double maxsumsq = std::numeric_limits<double>::lowest();
        double avgmag = 0.0;
        for (int i = lo; i <= hi; ++i) {
            m_sum[size_t(i)] /= kIterations;
            avgmag += m_sum[size_t(i)];
            if (m_sum[size_t(i)] > maxsumsq) {
                maxsumsq = m_sum[size_t(i)];
            }
        }
        avgmag /= m_searchOffset * 2.0;
        if ((maxsumsq - avgmag) < kMinPeakOverNoiseDb) {
            m_failure = weakSignalMessage();
            return;
        }
        // clean variables for next use
        m_sum.assign(m_sum.size(), 0.0);
        // From Thetis console.cs:9987-9991 [v2.10.3.15]
        //   RX1MeterCalOffset = 0.0f; RX1DisplayCalOffset = 0.0f;
        //   //MW0LGE_[2.9.0.6] RX2MeterCalOffset = 0.0f; RX2DisplayCalOffset = 0.0f;
        m_host->setMeterCalOverride(0.0);
        m_host->setDisplayCalOverride(0.0);
    }});

    // Preamp ON, then OFF (console.cs:9994-10029).
    addMeterSteps(&m_avg);
    m_steps.push_back({m_timings.meterReadGapMs, [this]() {
        m_host->setRx1PreampMode(PreampMode::Off);
    }});
    addMeterSteps(&m_avg2);
    m_steps[m_steps.size() - kMeterReads].delayMs =
        m_timings.preampSwitchMs + m_timings.meterPreWaitMs;
    m_steps.push_back({m_timings.meterReadGapMs, [this]() {
        const float offOffset = m_avg2 - m_avg;
        m_host->setRx1PreampOffsetDb(PreampMode::Off, -offOffset);
        m_host->setRx1PreampOffsetDb(PreampMode::On, 0.0f);
        m_host->setRx2PreampOffsetDb(PreampMode::Off, -offOffset);
        m_host->setRx2PreampOffsetDb(PreampMode::On, 0.0f);
    }});

    // From Thetis console.cs:10031-10138 [v2.10.3.15]: -10 to -50 on Alex
    // boards outside the list, RX1 only.
    //   HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
    //   HardwareSpecific.Model != HPSDRModel.REDPITAYA) //DH1KLM
    //   [original inline comments from console.cs:10037 and 10041; the list
    //   is in measuresAlexSteps]
    if (measuresAlexSteps(m_host->alexPresent(), m_host->model())) {
        const PreampMode steps[] = {PreampMode::Minus10, PreampMode::Minus20,
                                    PreampMode::Minus30, PreampMode::Minus40,
                                    PreampMode::Minus50};
        for (PreampMode mode : steps) {
            m_steps.push_back({0, [this, mode]() { m_host->setRx1PreampMode(mode); }});
            addMeterSteps(&m_avg2);
            m_steps[m_steps.size() - kMeterReads].delayMs =
                m_timings.preampSwitchMs + m_timings.meterPreWaitMs;
            m_steps.push_back({m_timings.meterReadGapMs, [this, mode]() {
                m_host->setRx1PreampOffsetDb(mode, -(m_avg2 - m_avg));
            }});
        }
    }

    // From Thetis console.cs:10151-10174 [v2.10.3.15]: preamp ON, Thread.Sleep(5000),
    // 50 spectra summed as magnitude squared over +/- 2500 Hz, the peak bin.
    m_steps.push_back({0, [this]() { m_host->setRx1PreampMode(PreampMode::On); }});
    for (int i = 0; i < kIterations; ++i) {
        m_steps.push_back({i == 0 ? m_timings.finalSettleMs : m_timings.snapGapMs,
                           [this]() { snapSearch(kPeakRangeHz, false); }});
    }
    m_steps.push_back({m_timings.snapGapMs, [this]() {
        const int lo = m_searchCentre - m_searchOffset;
        const int hi = m_searchCentre + m_searchOffset;
        double maxsumsq = 0.0;  // maxsumsq = 0.0; (cleaned for next use)
        for (int i = lo; i <= hi; ++i) {
            if (m_sum[size_t(i)] > maxsumsq) {
                maxsumsq = m_sum[size_t(i)];
            }
        }
        const float avg2 = 10.0f * float(std::log10(
            maxsumsq / kIterations / std::pow(double(m_fftSize), 2.0)));

        // From Thetis console.cs:10177-10189 [v2.10.3.15]
        //   float diff = level - (avg + _rx1_meter_cal_offset + rx1_preamp_offset[(int)rx1_preamp_mode]);
        //   _rx1_meter_cal_offset += diff;
        //   rx_meter_cal_offset_by_radio[...] = _rx1_meter_cal_offset;  // MW0LGE_[2.9.0.7] re-instated
        //   diff = level - (avg2 + _rx1_display_cal_offset + rx1_preamp_offset[(int)rx1_preamp_mode]);
        //   RX1DisplayCalOffset += diff;
        //   RX2DisplayCalOffset += diffRX2; // MW0LGE_[2.9.0.6]
        const float preampOffset = m_host->rx1PreampOffsetDb(m_host->rx1PreampMode());
        const float meterCal = float(m_host->meterCalDb());
        const float meterDiff = m_level - (m_avg + meterCal + preampOffset);
        m_host->setMeterCalOverride(double(meterCal + meterDiff));
        const float displayCal = float(m_host->displayCalDb());
        const float displayDiff = m_level - (avg2 + displayCal + preampOffset);
        m_host->setDisplayCalOverride(double(displayCal + displayDiff));

        finish(true, QStringLiteral("Level calibration finished."));
    }});
}

} // namespace NereusSDR
