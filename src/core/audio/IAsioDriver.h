// =================================================================
// src/core/audio/IAsioDriver.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The seam between the portable ASIO
// session rules (AsioSession) and one loaded ASIO driver.  The Windows
// adapter (AsioDriverWin, in the mic helper only) calls the Steinberg
// SDK; the tests use FakeAsioDriver.  Nothing here includes the SDK.
//
// Design: docs/architecture/2026-10-08-native-audio-engines-design.md
// ("ASIO"; D3, D9, D14, D15, D33).  Requirements R-AUD-19 to R-AUD-22.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDeviceTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <optional>

namespace NereusSDR {

// The driver's one sample type for every channel.  Only the five the
// session converts are named (settled call 28); every other ASIOSampleType
// reads as Unsupported.
enum class AsioSampleType { Int16Lsb, Int24Lsb, Int32Lsb, Float32Lsb, Float64Lsb, Unsupported };

struct AsioDriverCaps {
    QString name;
    int inputChannels = 0;
    int outputChannels = 0;
    AsioSampleType sampleType = AsioSampleType::Int32Lsb;
    int minBufferFrames = 0, maxBufferFrames = 0, preferredBufferFrames = 0, granularity = 0;
    QList<double> sampleRates;      // the rates canSampleRate accepted, from 44100, 48000, 88200, 96000, 176400, 192000
    double currentRate = 0.0;
    std::int64_t inputLatencyFrames = 0, outputLatencyFrames = 0;
    friend bool operator==(const AsioDriverCaps&, const AsioDriverCaps&) = default;
};

enum class AsioMessage { ResetRequest, BufferSizeChange, ResyncRequest, LatenciesChanged };

// Why the last load() or createBuffers() failed.  InUse: the driver's
// object was created but refused to start, which is how a driver held by
// another program answers (settled call, Task 15 report).
enum class AsioDriverFailure { None, NotInstalled, InUse, Failed };

class IAsioDriver {
public:
    virtual ~IAsioDriver() = default;
    virtual QStringList installedDrivers() = 0;
    virtual std::optional<AsioDriverCaps> load(const QString& name) = 0;      // loads and initialises
    virtual bool createBuffers(const QList<int>& inputChannels, const QList<int>& outputChannels, int bufferFrames) = 0;
    virtual bool setSampleRate(double rate) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void disposeAndUnload() = 0;
    virtual void openControlPanel() = 0;
    virtual void setCallbacks(std::function<void(int bufferIndex)> bufferSwitch, std::function<void(AsioMessage)> message) = 0;

    // Beyond the plan's list (Task 15 report): the session needs the
    // driver's planar buffers and the reason a load failed.
    //
    // The buffer of one created channel (0-based, as passed to
    // createBuffers) for buffer half bufferIndex (0 or 1); nullptr for a
    // channel that was not created.  Valid from createBuffers() until
    // disposeAndUnload().
    virtual void* buffer(AudioDeviceDirection direction, int channel, int bufferIndex) = 0;
    virtual AsioDriverFailure lastFailure() const = 0;
};

} // namespace NereusSDR
