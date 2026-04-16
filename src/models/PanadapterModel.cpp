#include "PanadapterModel.h"

#include "core/AppSettings.h"

#include <QStringLiteral>

namespace NereusSDR {

namespace {

// Thetis uniform per-band default (console.cs:14242-14436). All 14 bands
// ship with the same values; per-band storage exists so users can
// customise, not because Thetis hand-tunes each band.
constexpr int kThetisDefaultDbMax = -40;
constexpr int kThetisDefaultDbMin = -140;
constexpr int kDefaultGridStep    = 10;  // NereusSDR divergence (§10).

QString gridMaxKey(Band b)      { return QStringLiteral("DisplayGridMax_") + bandKeyName(b); }
QString gridMinKey(Band b)      { return QStringLiteral("DisplayGridMin_") + bandKeyName(b); }
QString clarityFloorKey(Band b) { return QStringLiteral("ClarityFloor_")   + bandKeyName(b); }

} // namespace

PanadapterModel::PanadapterModel(QObject* parent)
    : QObject(parent)
{
    // Seed every band slot with Thetis uniform defaults before loading
    // persisted overrides. This matches Q4 resolution (plan §5.3): existing
    // users will see the grid shift from NereusSDR's -20/-160 to Thetis's
    // -40/-140 on first launch after upgrade.
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band b = static_cast<Band>(i);
        m_perBandGrid.insert(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
    }
    loadPerBandGridFromSettings();

    // Match the initial band to the default center frequency so the
    // dBmFloor/dBmCeiling pair reflects the 20m slot from the start.
    m_band = bandFromFrequency(m_centerFrequency);
    applyBandGrid(m_band);
}

PanadapterModel::~PanadapterModel() = default;

void PanadapterModel::setCenterFrequency(double freq)
{
    if (!qFuzzyCompare(m_centerFrequency, freq)) {
        m_centerFrequency = freq;
        emit centerFrequencyChanged(freq);

        // Auto-derive the enclosing band. If this crosses a boundary,
        // setBand() updates the dBm pair to the new band's slot.
        const Band derived = bandFromFrequency(freq);
        if (derived != m_band) {
            setBand(derived);
        }
    }
}

void PanadapterModel::setBandwidth(double bw)
{
    if (!qFuzzyCompare(m_bandwidth, bw)) {
        m_bandwidth = bw;
        emit bandwidthChanged(bw);
    }
}

void PanadapterModel::setdBmFloor(int floor)
{
    if (m_dBmFloor != floor) {
        m_dBmFloor = floor;
        emit levelChanged();
    }
}

void PanadapterModel::setdBmCeiling(int ceiling)
{
    if (m_dBmCeiling != ceiling) {
        m_dBmCeiling = ceiling;
        emit levelChanged();
    }
}

void PanadapterModel::setFftSize(int size)
{
    if (m_fftSize != size) {
        m_fftSize = size;
        emit fftSizeChanged(size);
    }
}

void PanadapterModel::setAverageCount(int count)
{
    m_averageCount = count;
}

// ---- Per-band grid (Phase 3G-8 commit 2) ----

void PanadapterModel::setBand(Band b)
{
    if (m_band == b) {
        return;
    }
    m_band = b;
    applyBandGrid(b);
    emit bandChanged(b);
}

BandGridSettings PanadapterModel::perBandGrid(Band b) const
{
    return m_perBandGrid.value(b, BandGridSettings{ -40, -140 });
}

void PanadapterModel::setPerBandDbMax(Band b, int dbMax)
{
    BandGridSettings& slot = m_perBandGrid[b];  // constructor seeded all 14
    if (slot.dbMax == dbMax) {
        return;
    }
    slot.dbMax = dbMax;
    saveBandGridToSettings(b);
    if (b == m_band) {
        setdBmCeiling(dbMax);
    }
}

void PanadapterModel::setPerBandDbMin(Band b, int dbMin)
{
    BandGridSettings& slot = m_perBandGrid[b];
    if (slot.dbMin == dbMin) {
        return;
    }
    slot.dbMin = dbMin;
    saveBandGridToSettings(b);
    if (b == m_band) {
        setdBmFloor(dbMin);
    }
}

float PanadapterModel::clarityFloor(Band b) const
{
    return m_perBandGrid.value(b).clarityFloor;
}

void PanadapterModel::setClarityFloor(Band b, float floor)
{
    BandGridSettings& slot = m_perBandGrid[b];
    if ((!qIsNaN(floor) && !qIsNaN(slot.clarityFloor) && qFuzzyCompare(slot.clarityFloor, floor)) ||
        (qIsNaN(floor) && qIsNaN(slot.clarityFloor))) {
        return;
    }
    slot.clarityFloor = floor;
    saveBandGridToSettings(b);
}

void PanadapterModel::setGridStep(int step)
{
    if (step <= 0 || m_gridStep == step) {
        return;
    }
    m_gridStep = step;
    AppSettings::instance().setValue(QStringLiteral("DisplayGridStep"), step);
    emit gridStepChanged(step);
}

void PanadapterModel::applyBandGrid(Band b)
{
    const BandGridSettings s = m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
    setdBmCeiling(s.dbMax);
    setdBmFloor(s.dbMin);
}

void PanadapterModel::loadPerBandGridFromSettings()
{
    auto& s = AppSettings::instance();
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band b = static_cast<Band>(i);
        const QVariant maxV = s.value(gridMaxKey(b));
        const QVariant minV = s.value(gridMinKey(b));
        const QVariant cfV  = s.value(clarityFloorKey(b));
        BandGridSettings slot = m_perBandGrid.value(b, BandGridSettings{ kThetisDefaultDbMax, kThetisDefaultDbMin });
        if (maxV.isValid()) { slot.dbMax = maxV.toInt(); }
        if (minV.isValid()) { slot.dbMin = minV.toInt(); }
        if (cfV.isValid())  { slot.clarityFloor = cfV.toFloat(); }
        m_perBandGrid.insert(b, slot);
    }

    const QVariant stepV = s.value(QStringLiteral("DisplayGridStep"));
    if (stepV.isValid()) {
        const int step = stepV.toInt();
        if (step > 0) { m_gridStep = step; }
    } else {
        m_gridStep = kDefaultGridStep;
    }
}

void PanadapterModel::saveBandGridToSettings(Band b) const
{
    const BandGridSettings slot = m_perBandGrid.value(b);
    auto& s = AppSettings::instance();
    s.setValue(gridMaxKey(b), slot.dbMax);
    s.setValue(gridMinKey(b), slot.dbMin);
    if (!qIsNaN(slot.clarityFloor)) {
        s.setValue(clarityFloorKey(b), slot.clarityFloor);
    }
}

} // namespace NereusSDR
