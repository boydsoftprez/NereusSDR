#pragma once
// =================================================================
// src/core/session/media/DssWideRow.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original extraction of the calibrated-dBm crop
// boundary already used by SpectrumWidget::buildDssWideRow(). This is only
// crop and peak reduction; detector and avenger processing stay exclusively
// on the exact waterfall plane.
//
// =================================================================

#include <QVector>

namespace NereusSDR {

struct DssWideRowRequest {
    double viewCentreHz {0.0};
    double viewSpanHz {0.0};
    double sourceCentreHz {0.0};
    double sourceSampleRateHz {0.0};
    /// 0 disables a wide row; values > 1 request a bounded overhang.
    double requestedSpanFactor {0.0};
};

struct DssWideRow {
    QVector<float> binsDbm;
    double centreHz {0.0};
    double spanHz {0.0};
};

/// Crops an already calibrated full-source dBm row. The returned row is the
/// same DDC-window calculation local 3D uses; it intentionally does not run
/// the trace/waterfall detector or averager again.
DssWideRow cropDssWideRow(const QVector<float>& fullBinsDbm,
                          const DssWideRowRequest& request);

/// Peak-preserving reduction used before bounded remote transport. It mirrors
/// DssRenderer's raw-row reduction so a one-bin carrier remains represented
/// when a wide crop exceeds the renderer's fixed column count.
QVector<float> peakReduceDssWideRow(const QVector<float>& binsDbm,
                                    int maximumSamples);

} // namespace NereusSDR
