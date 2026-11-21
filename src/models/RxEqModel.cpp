// src/models/RxEqModel.cpp
//
// Global RX equalizer values and persistence. DSP application is RadioModel's
// job: it owns the RxChannels and pushes the ten-band graph or the parametric
// profile to WDSP's RXAEQ stage.

#include "RxEqModel.h"

#include "core/AppSettings.h"

#include <QtGlobal>
#include <QTimer>

namespace NereusSDR {

namespace {
QString key(const char* leaf)
{
    return QStringLiteral("Audio/RxEq/") + QLatin1String(leaf);
}
constexpr const char* kEnabledKey = "Audio/RxEqEnabled";   // shared with TX/N bypass
} // namespace

RxEqModel::RxEqModel(QObject* parent)
    : QObject(parent)
{
    m_freqsHz = { 32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    // Slider drags change a value per step; write the settings once the
    // gesture settles instead of on every step.
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &RxEqModel::save);
    load();
}

void RxEqModel::load()
{
    AppSettings& s = AppSettings::instance();
    m_enabled = s.value(QStringLiteral("Audio/RxEqEnabled"), false).toBool();
    m_preampDb = qBound(kPreampDbMin, s.value(key("Preamp"), 0).toInt(), kPreampDbMax);
    for (int i = 0; i < kBandCount; ++i) {
        m_bandsDb[static_cast<std::size_t>(i)] = qBound(
            kBandDbMin, s.value(key("Band%1").arg(i), 0).toInt(), kBandDbMax);
        m_freqsHz[static_cast<std::size_t>(i)] = qBound(
            kFreqHzMin,
            s.value(key("Freq%1").arg(i), kDefaultFreqs[i]).toInt(),
            kFreqHzMax);
    }
    m_useLegacy = s.value(key("UseLegacy"), true).toBool();
    m_paraEqData = s.value(key("ParaEqData"), QString()).toString();
    m_nc = qBound(kNcMin, s.value(key("Nc"), 2048).toInt(), kNcMax);
    m_mp = s.value(key("Mp"), false).toBool();
    m_ctfmode = qBound(0, s.value(key("Ctfmode"), 0).toInt(), kCtfmodeMax);
    m_wintype = qBound(0, s.value(key("Wintype"), 0).toInt(), kWintypeMax);
}

void RxEqModel::save()
{
    AppSettings::instance().save();
}

int RxEqModel::rxEqBand(int index) const noexcept
{
    if (index < 0 || index >= kBandCount) { return 0; }
    return m_bandsDb[static_cast<std::size_t>(index)];
}

int RxEqModel::rxEqFreq(int index) const noexcept
{
    if (index < 0 || index >= kBandCount) { return 0; }
    return m_freqsHz[static_cast<std::size_t>(index)];
}

std::array<int, 11> RxEqModel::graph10() const
{
    std::array<int, 11> eq{};
    eq[0] = m_preampDb;
    for (int i = 0; i < kBandCount; ++i) {
        eq[static_cast<std::size_t>(i) + 1] = m_bandsDb[static_cast<std::size_t>(i)];
    }
    return eq;
}

void RxEqModel::setRxEqEnabled(bool on)
{
    if (m_enabled == on) { return; }
    m_enabled = on;
    AppSettings::instance().setValue(QStringLiteral("Audio/RxEqEnabled"),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqEnabledChanged(on);
}

void RxEqModel::applyBypassEnabled(bool on)
{
    // The bypass writer already stored Audio/RxEqEnabled; reflect it without
    // writing the setting again (which would fight the bypass restore).
    if (m_enabled == on) { return; }
    m_enabled = on;
    emit rxEqEnabledChanged(on);
}

void RxEqModel::setRxEqPreamp(int dB)
{
    dB = qBound(kPreampDbMin, dB, kPreampDbMax);
    if (m_preampDb == dB) { return; }
    m_preampDb = dB;
    AppSettings::instance().setValue(key("Preamp"), dB);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqPreampChanged(dB);
}

void RxEqModel::setRxEqBand(int index, int dB)
{
    if (index < 0 || index >= kBandCount) { return; }
    dB = qBound(kBandDbMin, dB, kBandDbMax);
    if (m_bandsDb[static_cast<std::size_t>(index)] == dB) { return; }
    m_bandsDb[static_cast<std::size_t>(index)] = dB;
    AppSettings::instance().setValue(key("Band%1").arg(index), dB);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqBandChanged(index, dB);
}

void RxEqModel::setRxEqFreq(int index, int hz)
{
    if (index < 0 || index >= kBandCount) { return; }
    hz = qBound(kFreqHzMin, hz, kFreqHzMax);
    if (m_freqsHz[static_cast<std::size_t>(index)] == hz) { return; }
    m_freqsHz[static_cast<std::size_t>(index)] = hz;
    AppSettings::instance().setValue(key("Freq%1").arg(index), hz);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqFreqChanged(index, hz);
}

void RxEqModel::setRxEqUseLegacy(bool legacy)
{
    if (m_useLegacy == legacy) { return; }
    m_useLegacy = legacy;
    AppSettings::instance().setValue(key("UseLegacy"),
                                     legacy ? QStringLiteral("True") : QStringLiteral("False"));
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqUseLegacyChanged(legacy);
}

void RxEqModel::setRxEqParaEqData(const QString& data)
{
    if (m_paraEqData == data) { return; }
    m_paraEqData = data;
    AppSettings::instance().setValue(key("ParaEqData"), data);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqParaEqDataChanged(data);
}

void RxEqModel::setRxEqNc(int nc)
{
    nc = qBound(kNcMin, nc, kNcMax);
    if (m_nc == nc) { return; }
    m_nc = nc;
    AppSettings::instance().setValue(key("Nc"), nc);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqNcChanged(nc);
}

void RxEqModel::setRxEqMp(bool mp)
{
    if (m_mp == mp) { return; }
    m_mp = mp;
    AppSettings::instance().setValue(key("Mp"),
                                     mp ? QStringLiteral("True") : QStringLiteral("False"));
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqMpChanged(mp);
}

void RxEqModel::setRxEqCtfmode(int mode)
{
    mode = qBound(0, mode, kCtfmodeMax);
    if (m_ctfmode == mode) { return; }
    m_ctfmode = mode;
    AppSettings::instance().setValue(key("Ctfmode"), mode);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqCtfmodeChanged(mode);
}

void RxEqModel::setRxEqWintype(int wintype)
{
    wintype = qBound(0, wintype, kWintypeMax);
    if (m_wintype == wintype) { return; }
    m_wintype = wintype;
    AppSettings::instance().setValue(key("Wintype"), wintype);
    if (m_saveTimer) { m_saveTimer->start(); }
    emit rxEqWintypeChanged(wintype);
}

} // namespace NereusSDR
