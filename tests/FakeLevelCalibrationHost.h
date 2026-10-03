// no-port-check: test-only. Thetis file names appear only in source-cite
// comments. No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// A fake receiver for LevelCalibrationRun: a meter that reads a set level
// per preamp setting and a spectrum with one carrier. Shared by the run's
// tests and the station tests of the Core procedure. Nothing here touches
// a radio.
// =================================================================

#pragma once

#include <QStringList>

#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "core/LevelCalibrationRun.h"

namespace NereusSDR::LevelCalTest {

constexpr int kFft = 4096;
constexpr double kRate = 192000.0;
constexpr double kCentre = 7100000.0;
constexpr double kBinWidth = kRate / kFft;  // 46.875 Hz

class FakeHost : public LevelCalibrationHost {
public:
    // Receiver state.
    bool live = true;
    bool tx = false;
    bool alex = false;
    HPSDRModel boardModel = HPSDRModel::HERMES;
    double vfoHz = 14200000.0;
    bool ritOn = true;
    int ritHz = 250;
    DSPMode mode = DSPMode::USB;
    int phoneBuffer = 4096;
    bool att1 = true;
    bool att2 = true;
    PreampMode preamp1 = PreampMode::Minus20;
    PreampMode preamp2 = PreampMode::Off;
    std::optional<double> meterCal = 3.5;
    std::optional<double> displayCal;  // absent key: the model default
    double displayDefault = 1.25;
    std::array<float, 10> rx1Offsets{};
    std::map<int, float> rx2Offsets;
    int filterLo = -5000;
    int filterHi = 5000;

    // Fake signal.
    double signalHz = kCentre + 1500.0;
    float peakPower = 1.0f;       // |X|^2 of the carrier bin
    float noisePower = 1.0e-6f;   // |X|^2 of every other bin
    bool spectrumOn = true;
    std::map<PreampMode, float> meterByMode{
        {PreampMode::On, -73.0f},
        {PreampMode::Off, -93.0f},
        {PreampMode::Minus10, -83.5f},
        {PreampMode::Minus20, -93.25f},
        {PreampMode::Minus30, -103.0f},
        {PreampMode::Minus40, -113.0f},
        {PreampMode::Minus50, -122.5f},
    };

    // Observation.
    QStringList log;
    int meterReads = 0;
    int spectrumReads = 0;
    std::function<void()> onMeterRead;
    std::vector<std::optional<double>> meterCalDuringReads;

    bool radioLive() const override { return live; }
    bool transmitting() const override { return tx; }
    bool alexPresent() const override { return alex; }
    HPSDRModel model() const override { return boardModel; }

    double frequencyHz() const override { return vfoHz; }
    void setFrequencyHz(double hz) override { vfoHz = hz; log << QStringLiteral("vfo"); }
    bool ritEnabled() const override { return ritOn; }
    int ritOffsetHz() const override { return ritHz; }
    void setRit(bool on, int hz) override { ritOn = on; ritHz = hz; log << QStringLiteral("rit"); }
    DSPMode dspMode() const override { return mode; }
    void setDspMode(DSPMode m) override { mode = m; log << QStringLiteral("mode"); }
    int phoneRxBuffer() const override { return phoneBuffer; }
    void setPhoneRxBuffer(int size) override { phoneBuffer = size; log << QStringLiteral("buffer"); }
    bool rx1StepAttEnabled() const override { return att1; }
    bool rx2StepAttEnabled() const override { return att2; }
    void setStepAttEnabled(bool rx1, bool rx2) override { att1 = rx1; att2 = rx2; log << QStringLiteral("att"); }
    PreampMode rx1PreampMode() const override { return preamp1; }
    void setRx1PreampMode(PreampMode m) override { preamp1 = m; log << QStringLiteral("preamp1"); }
    PreampMode rx2PreampMode() const override { return preamp2; }
    void setRx2PreampMode(PreampMode m) override { preamp2 = m; log << QStringLiteral("preamp2"); }

    std::optional<double> meterCalOverride() const override { return meterCal; }
    std::optional<double> displayCalOverride() const override { return displayCal; }
    double meterCalDb() const override { return meterCal.value_or(0.0); }
    double displayCalDb() const override { return displayCal.value_or(displayDefault); }
    void setMeterCalOverride(std::optional<double> db) override { meterCal = db; }
    void setDisplayCalOverride(std::optional<double> db) override { displayCal = db; }
    float rx1PreampOffsetDb(PreampMode m) const override { return rx1Offsets[size_t(m)]; }
    void setRx1PreampOffsetDb(PreampMode m, float db) override { rx1Offsets[size_t(m)] = db; }
    void setRx2PreampOffsetDb(PreampMode m, float db) override { rx2Offsets[int(m)] = db; }

    int filterLowHz() const override { return filterLo; }
    int filterHighHz() const override { return filterHi; }

    float readSignalAverage() override
    {
        ++meterReads;
        meterCalDuringReads.push_back(meterCal);
        if (onMeterRead) {
            onMeterRead();
        }
        const auto it = meterByMode.find(preamp1);
        return it == meterByMode.end() ? -200.0f : it->second;
    }

    std::optional<LevelCalSpectrum> latestSpectrum() override
    {
        ++spectrumReads;
        if (!spectrumOn) {
            return std::nullopt;
        }
        LevelCalSpectrum s;
        s.centreHz = kCentre;
        s.sampleRateHz = kRate;
        s.binsLinear.fill(noisePower, kFft);
        const int bin = kFft / 2 + int(std::lround((signalHz - kCentre) / kBinWidth));
        s.binsLinear[bin] = peakPower;
        return s;
    }
};

inline LevelCalibrationRun::Timings instant()
{
    LevelCalibrationRun::Timings t;
    t.settleBeforeZeroBeatMs = 0;
    t.snapGapMs = 0;
    t.meterPreWaitMs = 0;
    t.meterReadGapMs = 0;
    t.preampSwitchMs = 0;
    t.finalSettleMs = 0;
    return t;
}

} // namespace NereusSDR::LevelCalTest
