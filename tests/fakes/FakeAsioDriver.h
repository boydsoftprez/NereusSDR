// =================================================================
// tests/fakes/FakeAsioDriver.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR test fake.  An IAsioDriver that loads nothing:
// the drivers it lists are the test's, every call is recorded, and the
// test calls the buffer switch and the driver's messages itself (V-SW-7).
// No real ASIO driver is ever instantiated (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (V-SW-7). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAsioDriver.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace NereusSDR::Test {

class FakeAsioDriver final : public IAsioDriver {
public:
    // A driver the fake lists, with the caps load() returns.
    void addDriver(const AsioDriverCaps& caps) { m_drivers.insert(caps.name, caps); m_order.append(caps.name); }
    // load() of this driver fails as a driver held by another program does.
    void setHeldByAnotherProgram(const QString& name, bool held)
    {
        if (held) {
            m_held.insert(name);
        } else {
            m_held.remove(name);
        }
    }
    // The next load() of name returns these caps (a driver whose settings
    // changed in its control panel).
    void setCaps(const AsioDriverCaps& caps) { m_drivers.insert(caps.name, caps); }

    QStringList installedDrivers() override
    {
        record(QStringLiteral("installedDrivers"));
        return m_order;
    }

    std::optional<AsioDriverCaps> load(const QString& name) override
    {
        record(QStringLiteral("load:") + name);
        if (m_loaded) {
            m_failure = AsioDriverFailure::Failed;   // one driver at a time
            return std::nullopt;
        }
        const auto it = m_drivers.constFind(name);
        if (it == m_drivers.constEnd()) {
            m_failure = AsioDriverFailure::NotInstalled;
            return std::nullopt;
        }
        if (m_held.contains(name)) {
            m_failure = AsioDriverFailure::InUse;
            return std::nullopt;
        }
        m_failure = AsioDriverFailure::None;
        m_loaded = true;
        m_current = *it;
        return m_current;
    }

    bool createBuffers(const QList<int>& inputChannels, const QList<int>& outputChannels,
                       int bufferFrames) override
    {
        record(QStringLiteral("createBuffers:%1").arg(bufferFrames));
        m_lastInputs = inputChannels;
        m_lastOutputs = outputChannels;
        m_lastFrames = bufferFrames;
        const std::optional<DeviceSampleFormat> format = formatOf(m_current.sampleType);
        const std::size_t bytes = static_cast<std::size_t>(bufferFrames)
                                  * static_cast<std::size_t>(format ? deviceSampleBytes(*format) : 4);
        m_inputBuffers.clear();
        m_outputBuffers.clear();
        for (int channel : inputChannels) {
            for (auto& half : m_inputBuffers[channel]) {
                half.assign(bytes, std::byte{0});
            }
        }
        for (int channel : outputChannels) {
            for (auto& half : m_outputBuffers[channel]) {
                half.assign(bytes, std::byte{0});
            }
        }
        return true;
    }

    bool setSampleRate(double rate) override
    {
        record(QStringLiteral("setSampleRate:%1").arg(rate));
        if (!m_current.sampleRates.contains(rate)) {
            return false;
        }
        m_current.currentRate = rate;
        return true;
    }

    bool start() override
    {
        record(QStringLiteral("start"));
        m_started = true;
        return true;
    }

    void stop() override
    {
        record(QStringLiteral("stop"));
        m_started = false;
    }

    void disposeAndUnload() override
    {
        record(QStringLiteral("disposeAndUnload"));
        m_inputBuffers.clear();
        m_outputBuffers.clear();
        m_loaded = false;
    }

    void openControlPanel() override { record(QStringLiteral("openControlPanel")); }

    void setCallbacks(std::function<void(int bufferIndex)> bufferSwitch,
                      std::function<void(AsioMessage)> message) override
    {
        m_bufferSwitch = std::move(bufferSwitch);
        m_message = std::move(message);
    }

    void* buffer(AudioDeviceDirection direction, int channel, int bufferIndex) override
    {
        auto& buffers = direction == AudioDeviceDirection::Output ? m_outputBuffers : m_inputBuffers;
        const auto it = buffers.find(channel);
        if (it == buffers.end() || bufferIndex < 0 || bufferIndex > 1) {
            return nullptr;
        }
        return it->second[static_cast<std::size_t>(bufferIndex)].data();
    }

    AsioDriverFailure lastFailure() const override { return m_failure; }

    // The driver's side, called by the test.
    void fireBufferSwitch(int bufferIndex)
    {
        if (m_bufferSwitch) {
            m_bufferSwitch(bufferIndex);
        }
    }
    void fireMessage(AsioMessage message)
    {
        if (m_message) {
            m_message(message);
        }
    }

    QStringList calls() const
    {
        QMutexLocker lock(&m_logMutex);
        return m_calls;
    }
    void clearCalls()
    {
        QMutexLocker lock(&m_logMutex);
        m_calls.clear();
    }
    bool loaded() const { return m_loaded; }
    bool started() const { return m_started; }
    int lastBufferFrames() const { return m_lastFrames; }
    QList<int> lastInputChannels() const { return m_lastInputs; }
    QList<int> lastOutputChannels() const { return m_lastOutputs; }
    const AsioDriverCaps& current() const { return m_current; }

private:
    static std::optional<DeviceSampleFormat> formatOf(AsioSampleType type)
    {
        switch (type) {
        case AsioSampleType::Int16Lsb: return DeviceSampleFormat::Int16;
        case AsioSampleType::Int24Lsb: return DeviceSampleFormat::Int24Packed;
        case AsioSampleType::Int32Lsb: return DeviceSampleFormat::Int32;
        case AsioSampleType::Float32Lsb: return DeviceSampleFormat::Float32;
        case AsioSampleType::Float64Lsb: return DeviceSampleFormat::Float64;
        case AsioSampleType::Unsupported: break;
        }
        return std::nullopt;
    }

    void record(const QString& call)
    {
        QMutexLocker lock(&m_logMutex);
        m_calls.append(call);
    }

    QHash<QString, AsioDriverCaps> m_drivers;
    QStringList m_order;
    QSet<QString> m_held;
    AsioDriverCaps m_current;
    bool m_loaded = false;
    bool m_started = false;
    AsioDriverFailure m_failure = AsioDriverFailure::None;
    int m_lastFrames = 0;
    QList<int> m_lastInputs;
    QList<int> m_lastOutputs;
    std::map<int, std::array<std::vector<std::byte>, 2>> m_inputBuffers;
    std::map<int, std::array<std::vector<std::byte>, 2>> m_outputBuffers;
    std::function<void(int)> m_bufferSwitch;
    std::function<void(AsioMessage)> m_message;
    mutable QMutex m_logMutex;
    QStringList m_calls;
};

} // namespace NereusSDR::Test
