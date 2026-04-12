#pragma once

#include "Band.h"

#include <QHash>
#include <QObject>

namespace NereusSDR {

// Per-band grid scale storage. Added in Phase 3G-8 (commit 2).
// dbStep is NOT per-band — Thetis keeps it as a single global value
// (verified console.cs:14242-14436).
struct BandGridSettings {
    int dbMax;
    int dbMin;
};

// Represents a single panadapter display.
// In NereusSDR, panadapters are entirely client-side — the radio sends
// raw I/Q, and the client computes FFT data for display. This model
// holds display state (center frequency, bandwidth, dBm range, per-band
// grid slots, current band).
class PanadapterModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(double centerFrequency READ centerFrequency WRITE setCenterFrequency NOTIFY centerFrequencyChanged)
    Q_PROPERTY(double bandwidth       READ bandwidth       WRITE setBandwidth       NOTIFY bandwidthChanged)
    Q_PROPERTY(int    dBmFloor        READ dBmFloor        WRITE setdBmFloor        NOTIFY levelChanged)
    Q_PROPERTY(int    dBmCeiling      READ dBmCeiling      WRITE setdBmCeiling      NOTIFY levelChanged)

public:
    explicit PanadapterModel(QObject* parent = nullptr);
    ~PanadapterModel() override;

    double centerFrequency() const { return m_centerFrequency; }
    // Setting the center frequency auto-derives the band via
    // bandFromFrequency() and calls setBand() on boundary crossing.
    // Direct setBand() is also callable when that's the wrong choice
    // (e.g. XVTR mode, or band button click that shouldn't auto-switch
    // on every subsequent VFO tweak).
    void setCenterFrequency(double freq);

    double bandwidth() const { return m_bandwidth; }
    void setBandwidth(double bw);

    int dBmFloor() const { return m_dBmFloor; }
    void setdBmFloor(int floor);

    int dBmCeiling() const { return m_dBmCeiling; }
    void setdBmCeiling(int ceiling);

    int fftSize() const { return m_fftSize; }
    void setFftSize(int size);

    int averageCount() const { return m_averageCount; }
    void setAverageCount(int count);

    // ---- Per-band grid (Phase 3G-8 commit 2) ----

    // Currently-active band. Changing the band updates dBmFloor/dBmCeiling
    // to the stored slot for the new band and emits levelChanged()
    // automatically so existing SpectrumWidget wiring reacts.
    Band band() const { return m_band; }
    void setBand(Band b);

    // Per-band grid slot accessors. dbMax/dbMin are persisted to
    // AppSettings on every write. Reading an unset band returns the
    // Thetis uniform default (-40 / -140) that was installed at
    // construction time.
    BandGridSettings perBandGrid(Band b) const;
    void setPerBandDbMax(Band b, int dbMax);
    void setPerBandDbMin(Band b, int dbMin);

    // Grid step (single global value, matches Thetis). Persisted under
    // the "DisplayGridStep" key.
    int gridStep() const { return m_gridStep; }
    void setGridStep(int step);

signals:
    void centerFrequencyChanged(double freq);
    void bandwidthChanged(double bw);
    void levelChanged();
    void fftSizeChanged(int size);
    void bandChanged(NereusSDR::Band band);
    void gridStepChanged(int step);

private:
    // Reads m_perBandGrid[b] and pushes it into dBmFloor/dBmCeiling.
    // Called by setBand(); does NOT emit bandChanged itself.
    void applyBandGrid(Band b);

    // AppSettings persistence helpers.
    void loadPerBandGridFromSettings();
    void saveBandGridToSettings(Band b) const;

    double m_centerFrequency{14225000.0};
    double m_bandwidth{200000.0};  // 200 kHz default
    int m_dBmFloor{-140};          // Matches Thetis default (was -130).
    int m_dBmCeiling{-40};         // Matches Thetis default (was -40).
    int m_fftSize{4096};
    int m_averageCount{3};

    Band m_band{Band::Band20m};    // Matches default center freq 14.225 MHz.
    QHash<Band, BandGridSettings> m_perBandGrid;
    int m_gridStep{10};            // NereusSDR divergence from Thetis 2.
};

} // namespace NereusSDR
