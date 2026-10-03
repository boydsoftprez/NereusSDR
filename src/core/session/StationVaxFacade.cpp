// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationVaxFacade.cpp  (NereusSDR)
// =================================================================
//
// See StationVaxFacade.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 25 (R-IOS-18), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: a remote window's copy. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/StationVaxFacade.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QVariant>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// A level as the applet saves it (three decimals), so a device reads 0.4
// for the engine's float 0.4, not 0.4000000059604645.
double thousandths(float level)
{
    return std::round(static_cast<double>(level) * 1000.0) / 1000.0;
}

} // namespace

QString StationVax::rxGainKey(int channel)
{
    // VaxApplet.cpp kRxGainKey.
    return QStringLiteral("audio/Vax%1/RxGain").arg(channel);
}

QString StationVax::mutedKey(int channel)
{
    // VaxApplet.cpp kMutedKey.
    return QStringLiteral("audio/Vax%1/Muted").arg(channel);
}

QString StationVax::txGainKey()
{
    // VaxApplet.cpp kTxGainKey.
    return QStringLiteral("audio/TxGain");
}

QString StationVax::deviceName(int channel)
{
    // What VaxApplet::deviceLabelFor shows for the channel.
#ifdef Q_OS_WIN
    const QString name = AppSettings::instance()
        .value(QStringLiteral("audio/Vax%1/DeviceName").arg(channel))
        .toString();
    return name.isEmpty() ? QStringLiteral("(no device)") : name;
#else
    // macOS CoreAudioHalBus and Linux LinuxPipeBus register the virtual
    // device under this name (AudioEngine::makeVaxBus).
    return QStringLiteral("NereusSDR VAX %1").arg(channel);
#endif
}

bool StationVax::isTransmitProperty(const QByteArray& name)
{
    return name == "txGain";
}

QString StationVax::levelRefusal(const QByteArray& name, const QVariant& value)
{
    if (!name.endsWith("RxGain") && name != "txGain") {
        return {};
    }
    const double level = value.toDouble();
    if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
        return QStringLiteral("A VAX level goes from 0 to 1.");
    }
    return {};
}

StationVax::StationVax(QObject* parent)
    : QObject(parent)
{
}

StationVax::~StationVax() = default;

void StationVax::bind(RadioModel* model, AudioEngine* audio, AppSettings* settings)
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
        for (SliceModel* slice : m_model->slices()) {
            if (slice != nullptr) {
                disconnect(slice, nullptr, this, nullptr);
            }
        }
    }
    if (m_audio) {
        disconnect(m_audio, nullptr, this, nullptr);
    }
    m_model = model;
    m_audio = audio;
    m_settings = settings;
    if (audio != nullptr) {
        // A change made anywhere (the station computer's applet, a device)
        // reaches every device.
        connect(audio, &AudioEngine::vaxRxGainChanged, this, [this](int, float) {
            emit gainsChanged();
        });
        connect(audio, &AudioEngine::vaxTxGainChanged, this, [this](float) {
            emit gainsChanged();
        });
        connect(audio, &AudioEngine::vaxMutedChanged, this, [this](int, bool) {
            emit mutesChanged();
        });
    }
    if (model != nullptr) {
        // As VaxApplet::connectSliceTagsTracking follows them.
        connect(model, &RadioModel::sliceAdded, this, [this](int index) {
            if (m_model) {
                watchSlice(m_model->sliceById(index));
            }
            refreshSlices();
        });
        connect(model, &RadioModel::sliceRemoved, this, [this](int) { refreshSlices(); });
        for (SliceModel* slice : model->slices()) {
            watchSlice(slice);
        }
    }
    refreshSlices();
    emit gainsChanged();
    emit mutesChanged();
}

void StationVax::watchSlice(QObject* object)
{
    auto* slice = qobject_cast<SliceModel*>(object);
    if (slice == nullptr) {
        return;
    }
    connect(slice, &SliceModel::vaxChannelChanged, this, [this]() { refreshSlices(); });
    connect(slice, &SliceModel::txSliceChanged, this, [this]() { refreshSlices(); });
}

void StationVax::refreshSlices()
{
    // VaxApplet::updateTagsLabels: each channel's slices, and the transmit
    // slice, by the slice's letter.
    QString slices[kChannels];
    QString tx;
    if (m_model) {
        for (SliceModel* slice : m_model->slices()) {
            if (slice == nullptr) {
                continue;
            }
            const QChar letter = slice->sliceLetter();
            const int channel = slice->vaxChannel();
            if (channel >= 1 && channel <= kChannels) {
                slices[channel - 1].append(letter);
            }
            if (slice->isTxSlice()) {
                tx = QString(letter);
            }
        }
    }
    bool changed = tx != m_txSlice;
    for (int i = 0; i < kChannels; ++i) {
        changed = changed || slices[i] != m_slices[i];
        m_slices[i] = slices[i];
    }
    m_txSlice = tx;
    if (changed) {
        emit slicesChanged();
    }
}

double StationVax::rxGain(int channel) const
{
    if (channel < 1 || channel > kChannels) {
        return 1.0;
    }
    return m_audio ? thousandths(m_audio->vaxRxGain(channel)) : m_rxGain[channel - 1];
}

bool StationVax::muted(int channel) const
{
    if (channel < 1 || channel > kChannels) {
        return false;
    }
    return m_audio ? m_audio->vaxMuted(channel) : m_muted[channel - 1];
}

double StationVax::txGain() const
{
    return m_audio ? thousandths(m_audio->vaxTxGain()) : m_txGainCopy;
}

QString StationVax::device(int channel) const
{
    if (channel < 1 || channel > kChannels) {
        return {};
    }
    // The Core's own object names this computer's devices; a window's copy
    // shows the names the Core sent.
    return m_audio || m_model ? deviceName(channel) : m_devices[channel - 1];
}

bool StationVax::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    static const char* const kSlices[] = {"ch1Slices", "ch2Slices", "ch3Slices", "ch4Slices"};
    static const char* const kDevices[] = {"ch1Device", "ch2Device", "ch3Device", "ch4Device"};
    for (int i = 0; i < kChannels; ++i) {
        if (propertyName == kSlices[i]) {
            if (m_slices[i] != value.toString()) {
                m_slices[i] = value.toString();
                emit slicesChanged();
            }
            return true;
        }
        if (propertyName == kDevices[i]) {
            if (m_devices[i] != value.toString()) {
                m_devices[i] = value.toString();
                emit devicesChanged();
            }
            return true;
        }
    }
    if (propertyName == "txSlice") {
        if (m_txSlice != value.toString()) {
            m_txSlice = value.toString();
            emit slicesChanged();
        }
        return true;
    }
    return false;
}

void StationVax::clearStationValues()
{
    for (int i = 0; i < kChannels; ++i) {
        m_slices[i].clear();
        m_devices[i].clear();
        m_rxGain[i] = 1.0;
        m_muted[i] = false;
        m_levels[i] = 0.0;
    }
    m_txSlice.clear();
    m_txGainCopy = 1.0;
    m_txLevel = 0.0;
    emit slicesChanged();
    emit devicesChanged();
    emit gainsChanged();
    emit mutesChanged();
    emit levelsChanged();
}

void StationVax::setStationLevels(const double* rx, double tx)
{
    for (int i = 0; i < kChannels; ++i) {
        m_levels[i] = rx[i];
    }
    m_txLevel = tx;
    emit levelsChanged();
}

double StationVax::stationLevel(int channel) const
{
    return channel >= 1 && channel <= kChannels ? m_levels[channel - 1] : 0.0;
}

void StationVax::save(const QString& key, const QString& value)
{
    if (m_settings == nullptr) {
        return;
    }
    m_settings->setValue(key, value);
    m_settings->save();
}

void StationVax::setRxGain(int channel, double gain)
{
    if (channel < 1 || channel > kChannels) {
        return;
    }
    if (!m_audio) {
        // A window's copy: the Core's value arriving, or this window's own
        // change, which the link sends on.
        if (m_rxGain[channel - 1] != gain) {
            m_rxGain[channel - 1] = gain;
            emit gainsChanged();
        }
        return;
    }
    const float level = std::clamp(static_cast<float>(gain), 0.0f, 1.0f);
    // As the applet's slider does: the engine, then the saved key.
    m_audio->setVaxRxGain(channel, level);
    save(rxGainKey(channel), QString::number(level, 'f', 3));
}

void StationVax::setMuted(int channel, bool on)
{
    if (channel < 1 || channel > kChannels) {
        return;
    }
    if (!m_audio) {
        if (m_muted[channel - 1] != on) {
            m_muted[channel - 1] = on;
            emit mutesChanged();
        }
        return;
    }
    // As the applet's Mute button does.
    m_audio->setVaxMuted(channel, on);
    save(mutedKey(channel), on ? QStringLiteral("True") : QStringLiteral("False"));
}

void StationVax::setTxGain(double gain)
{
    if (!m_audio) {
        if (m_txGainCopy != gain) {
            m_txGainCopy = gain;
            emit gainsChanged();
        }
        return;
    }
    const float level = std::clamp(static_cast<float>(gain), 0.0f, 1.0f);
    // As the applet's TX slider does.
    m_audio->setVaxTxGain(level);
    save(txGainKey(), QString::number(level, 'f', 3));
}

} // namespace NereusSDR
