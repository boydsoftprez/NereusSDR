// =================================================================
// src/core/LevelCalibrationService.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The Core's owner of a level
// calibration run: it points LevelCalibrationRun (the port of Thetis
// CalibrateLevel) at a slice of the RadioModel, feeds it the slice's
// spectrum and meter, and keeps the state the session sends a window.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/LevelCalibrationRun.h"

#include <QObject>
#include <QString>

#include <memory>

namespace NereusSDR {

class RadioModel;

class LevelCalibrationService : public QObject {
    Q_OBJECT
public:
    explicit LevelCalibrationService(RadioModel* model, QObject* parent = nullptr);
    ~LevelCalibrationService() override;

    // Starts a run on `sliceId` (-1: the active slice) for a carrier of
    // `levelDbm` at `frequencyHz`. The slice stands in for Thetis's RX1 and
    // VFO A. Empty when it started, otherwise the reason it did not.
    QString start(float levelDbm, double frequencyHz, int sliceId);
    // Stops a run and puts the receiver back. No-op when nothing runs.
    void cancel();

    bool running() const { return m_running; }
    int percent() const { return m_percent; }
    QString message() const { return m_message; }
    bool succeeded() const { return m_succeeded; }

    // Tests: run on `host` in place of the model's receiver (nullptr puts
    // the model's back). Takes effect while nothing runs.
    void setHostForTest(LevelCalibrationHost* host);
    void setTimingsForTest(const LevelCalibrationRun::Timings& timings);

signals:
    void stateChanged();

private:
    class ModelHost;

    void rebuildRun();
    void onProgress(int percent);
    void onFinished(bool ok, const QString& message);

    RadioModel* m_model = nullptr;
    std::unique_ptr<ModelHost> m_modelHost;
    LevelCalibrationHost* m_testHost = nullptr;
    std::unique_ptr<LevelCalibrationRun> m_run;
    std::optional<LevelCalibrationRun::Timings> m_timings;

    bool m_running = false;
    int m_percent = 0;
    QString m_message;
    bool m_succeeded = false;
};

} // namespace NereusSDR
