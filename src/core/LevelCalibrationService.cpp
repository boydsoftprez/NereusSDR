// =================================================================
// src/core/LevelCalibrationService.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See LevelCalibrationService.h; the
// calibration procedure itself is the port in LevelCalibrationRun.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29 - Reads and writes the connected model's own meter and
//                display calibration. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: RX2's preamp is its own mode, set
//                through the controller's RX2PreampMode port. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/LevelCalibrationService.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/RxChannel.h"
#include "core/StepAttenuatorController.h"
#include "core/session/media/DaemonSpectrumSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QPointer>

namespace NereusSDR {

namespace {

// The phone receive buffer (Thetis SetupForm.DSPPhoneRXBuffer), the key
// RxChannel reads for the SSB/AM group, default 64.
const QString kPhoneRxBufferKey = QStringLiteral("DspOptionsBufferSizePhoneRx");
constexpr int kPhoneRxBufferDefault = 64;

} // namespace

// The Core's receiver for a run: one slice of the RadioModel (Thetis RX1
// and VFO A), its stream's spectrum, and the station's step attenuator,
// preamp and calibration.
class LevelCalibrationService::ModelHost final : public LevelCalibrationHost {
public:
    explicit ModelHost(RadioModel* model)
        : m_model(model)
        , m_source(std::make_unique<DaemonSpectrumSource>())
    {
        m_source->setRadioModel(model);
    }

    ~ModelHost() override { end(); }

    void begin(SliceModel* slice)
    {
        end();
        m_slice = slice;
        m_stream = slice->streamIndex();
        m_lastFrame.reset();
        if (m_stream < 0) {
            return;
        }
        m_active = m_source->activate(key(), config());
        m_centreConnection = QObject::connect(
            m_model, &RadioModel::streamCentreChanged, m_source.get(),
            [this](int stream, double, int) {
                if (m_active && stream == m_stream) {
                    m_source->update(key(), config());
                }
            });
    }

    void end()
    {
        QObject::disconnect(m_centreConnection);
        if (m_active) {
            m_source->deactivate(key());
        }
        m_active = false;
        m_lastFrame.reset();
        m_slice = nullptr;
        m_stream = -1;
    }

    bool radioLive() const override { return m_model->isConnected(); }
    bool transmitting() const override
    {
        QString reason;
        return m_model->isTransmitting() || m_model->stationOnAirRefusal(&reason);
    }
    bool alexPresent() const override { return m_model->boardCapabilities().hasAlexFilters; }
    HPSDRModel model() const override { return m_model->hardwareProfile().model; }

    double frequencyHz() const override { return m_slice ? m_slice->frequency() : 0.0; }
    void setFrequencyHz(double hz) override
    {
        if (m_slice) {
            m_slice->setFrequency(hz);
        }
    }
    bool ritEnabled() const override { return m_slice && m_slice->ritEnabled(); }
    int ritOffsetHz() const override { return m_slice ? m_slice->ritHz() : 0; }
    void setRit(bool on, int hz) override
    {
        if (m_slice) {
            m_slice->setRitHz(hz);
            m_slice->setRitEnabled(on);
        }
    }
    DSPMode dspMode() const override { return m_slice ? m_slice->dspMode() : DSPMode::AM; }
    void setDspMode(DSPMode mode) override
    {
        if (m_slice) {
            m_slice->setDspMode(mode);
        }
    }
    int filterLowHz() const override { return m_slice ? m_slice->filterLow() : 0; }
    int filterHighHz() const override { return m_slice ? m_slice->filterHigh() : 0; }

    int phoneRxBuffer() const override
    {
        return AppSettings::instance().value(kPhoneRxBufferKey, kPhoneRxBufferDefault).toInt();
    }
    void setPhoneRxBuffer(int size) override
    {
        AppSettings::instance().setValue(kPhoneRxBufferKey, QString::number(size));
        m_model->scheduleRemoteDspOptionsApply(kPhoneRxBufferKey);
    }

    bool rx1StepAttEnabled() const override
    {
        const StepAttenuatorController* c = m_model->stepAttController();
        return c && c->stepAttEnabled();
    }
    bool rx2StepAttEnabled() const override
    {
        const StepAttenuatorController* c = m_model->stepAttController();
        return c && c->rx2StepAttEnabled();
    }
    void setStepAttEnabled(bool rx1, bool rx2) override
    {
        if (StepAttenuatorController* c = m_model->stepAttController()) {
            c->setStepAttEnabled(rx1);
            c->setRx2StepAttEnabled(rx2);
        }
    }
    PreampMode rx1PreampMode() const override
    {
        const StepAttenuatorController* c = m_model->stepAttController();
        return c ? c->preampMode() : PreampMode::Off;
    }
    void setRx1PreampMode(PreampMode mode) override
    {
        if (StepAttenuatorController* c = m_model->stepAttController()) {
            c->setPreampMode(mode);
        }
    }
    // RX2's preamp mode, through the controller's RX2PreampMode port: the
    // HPSDR alone takes a preamp bit, the listed boards the mode's
    // attenuation on RX2's ADC.
    PreampMode rx2PreampMode() const override
    {
        const StepAttenuatorController* c = m_model->stepAttController();
        return c ? c->rx2PreampMode() : PreampMode::On;
    }
    void setRx2PreampMode(PreampMode mode) override
    {
        if (StepAttenuatorController* c = m_model->stepAttController()) {
            c->setRx2PreampMode(mode);
        }
    }

    // The connected model's own entries (Thetis keeps one per model).
    std::optional<double> meterCalOverride() const override { return m_model->rxMeterCalOverrideDb(); }
    std::optional<double> displayCalOverride() const override { return m_model->rxDisplayCalOverrideDb(); }
    double meterCalDb() const override { return m_model->rxMeterCalOffsetDb(); }
    double displayCalDb() const override { return m_model->rxDisplayCalOffsetDb(); }
    void setMeterCalOverride(std::optional<double> db) override { m_model->setRxMeterCalOverrideDb(db); }
    void setDisplayCalOverride(std::optional<double> db) override { m_model->setRxDisplayCalOverrideDb(db); }
    float rx1PreampOffsetDb(PreampMode mode) const override
    {
        return m_model->rx1PreampOffsetDbFor(mode);
    }
    void setRx1PreampOffsetDb(PreampMode mode, float db) override
    {
        m_model->setRx1PreampOffsetDb(mode, db);
    }
    void setRx2PreampOffsetDb(PreampMode mode, float db) override
    {
        m_model->setRx2PreampOffsetDb(mode, db);
    }

    float readSignalAverage() override
    {
        RxChannel* channel = m_slice ? m_model->rxChannelForSlice(m_slice->sliceIndex()) : nullptr;
        return channel ? static_cast<float>(channel->getMeter(RxMeterType::SignalAvg)) : -200.0f;
    }

    std::optional<LevelCalSpectrum> latestSpectrum() override
    {
        if (!m_active) {
            return std::nullopt;
        }
        // takeLatest empties the slot; between frames the run reads the
        // last one again.
        if (std::optional<DaemonSpectrumFrame> frame = m_source->takeLatest(key())) {
            LevelCalSpectrum s;
            s.binsLinear = frame->binsLinear;
            s.centreHz = frame->centreHz;
            s.sampleRateHz = frame->sampleRateHz;
            m_lastFrame = std::move(s);
        }
        return m_lastFrame;
    }

private:
    MediaSourceKey key() const { return {m_stream, FftTier::Wide}; }

    DaemonSpectrumSourceConfig config() const
    {
        DaemonSpectrumSourceConfig c;
        c.centreHz = m_model->streamCentreHz(m_stream);
        c.sampleRateHz = m_model->streamSampleRateHz(m_stream);
        c.maxPendingIqFloats = c.fft.fftSize * 4;
        return c;
    }

    RadioModel* m_model = nullptr;
    std::unique_ptr<DaemonSpectrumSource> m_source;
    QPointer<SliceModel> m_slice;
    int m_stream = -1;
    bool m_active = false;
    QMetaObject::Connection m_centreConnection;
    std::optional<LevelCalSpectrum> m_lastFrame;
};

LevelCalibrationService::LevelCalibrationService(RadioModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_modelHost(std::make_unique<ModelHost>(model))
{
    rebuildRun();
}

LevelCalibrationService::~LevelCalibrationService()
{
    // The run's restore reaches the host, so it goes first.
    m_run.reset();
}

void LevelCalibrationService::rebuildRun()
{
    LevelCalibrationHost* host = m_testHost
        ? m_testHost
        : static_cast<LevelCalibrationHost*>(m_modelHost.get());
    m_run = std::make_unique<LevelCalibrationRun>(host);
    if (m_timings.has_value()) {
        m_run->setTimings(*m_timings);
    }
    connect(m_run.get(), &LevelCalibrationRun::progress,
            this, &LevelCalibrationService::onProgress);
    connect(m_run.get(), &LevelCalibrationRun::finished,
            this, &LevelCalibrationService::onFinished);
}

void LevelCalibrationService::setHostForTest(LevelCalibrationHost* host)
{
    if (m_running) {
        return;
    }
    m_testHost = host;
    rebuildRun();
}

void LevelCalibrationService::setTimingsForTest(const LevelCalibrationRun::Timings& timings)
{
    m_timings = timings;
    m_run->setTimings(timings);
}

QString LevelCalibrationService::start(float levelDbm, double frequencyHz, int sliceId)
{
    if (m_running) {
        return m_run->start(levelDbm, frequencyHz);  // the run's "already running"
    }
    SliceModel* slice = nullptr;
    if (sliceId < 0) {
        slice = m_model->activeSlice();
        if (slice == nullptr) {
            return QStringLiteral("Open a slice before calibrating the receive level.");
        }
    } else {
        slice = m_model->sliceById(sliceId);
        if (slice == nullptr) {
            return QStringLiteral("The slice to calibrate is not open.");
        }
    }
    if (m_testHost == nullptr) {
        m_modelHost->begin(slice);
    }
    const QString refused = m_run->start(levelDbm, frequencyHz);
    if (!refused.isEmpty()) {
        m_modelHost->end();
        return refused;
    }
    m_running = true;
    m_percent = 0;
    m_message.clear();
    m_succeeded = false;
    emit stateChanged();
    return {};
}

void LevelCalibrationService::cancel()
{
    if (m_running) {
        m_run->cancel();
    }
}

void LevelCalibrationService::onProgress(int percent)
{
    if (percent == m_percent) {
        return;
    }
    m_percent = percent;
    emit stateChanged();
}

void LevelCalibrationService::onFinished(bool ok, const QString& message)
{
    m_running = false;
    m_succeeded = ok;
    m_message = message;
    if (ok) {
        m_percent = 100;
    }
    m_modelHost->end();
    emit stateChanged();
}

} // namespace NereusSDR
