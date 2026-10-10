// =================================================================
// src/core/audio/AsioDriverWin.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The IAsioDriver over the Steinberg
// ASIO SDK (third_party/asiosdk, asiosdk_host), Windows only and built
// into the mic helper nereus-audio-capture alone (D9, D33): nothing in the
// window, the Core or a test ever loads an ASIO driver.  The host calls
// follow the SDK's host sample (host/sample/hostsample.cpp) as cmASIO's
// patched copy does (Thetis cmASIO/hostsample.cpp); no logic is ported.
//
// Design: docs/architecture/2026-10-08-native-audio-engines-design.md
// ("ASIO"; D3, D9, D14, D15, D33).  Requirements R-AUD-01, R-AUD-21,
// R-AUD-22.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-21, R-AUD-22).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-19): the driver
//               list is read again on each listing while no driver is
//               loaded, so a driver installed later is seen.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAsioDriver.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// The SDK's driver list (third_party/asiosdk/host/asiodrivers.h).
class AsioDrivers;

namespace NereusSDR {

// One driver loaded at a time, as the SDK's single driver object allows.
// Every call but the callbacks is made on the helper's main thread, which
// is where the SDK's driver list initialises COM.
class AsioDriverWin final : public IAsioDriver {
public:
    AsioDriverWin();
    ~AsioDriverWin() override;
    AsioDriverWin(const AsioDriverWin&) = delete;
    AsioDriverWin& operator=(const AsioDriverWin&) = delete;

    QStringList installedDrivers() override;
    std::optional<AsioDriverCaps> load(const QString& name) override;
    bool createBuffers(const QList<int>& inputChannels, const QList<int>& outputChannels,
                       int bufferFrames) override;
    bool setSampleRate(double rate) override;
    bool start() override;
    void stop() override;
    void disposeAndUnload() override;
    void openControlPanel() override;
    void setCallbacks(std::function<void(int bufferIndex)> bufferSwitch,
                      std::function<void(AsioMessage)> message) override;
    void* buffer(AudioDeviceDirection direction, int channel, int bufferIndex) override;
    AsioDriverFailure lastFailure() const override;

    // The SDK's C callbacks (ASIOCallbacks, in the .cpp) land here, on the
    // driver's thread, through the one static pointer they need.
    static void dispatchSwitch(long doubleBufferIndex);
    static void dispatchMessage(AsioMessage message);

private:
    static std::atomic<AsioDriverWin*> s_current;

    // The registry's driver list, read again on each listing while no
    // driver is loaded; the SDK's global (asioDrivers) points at it.
    std::unique_ptr<AsioDrivers> m_list;
    bool m_loaded = false;
    bool m_buffers = false;
    bool m_running = false;
    int m_inputChannels = 0;
    int m_outputChannels = 0;
    AsioDriverFailure m_failure = AsioDriverFailure::None;
    // Per device channel, the two halves of its double buffer; null for a
    // channel without a buffer.
    std::vector<std::array<void*, 2>> m_inputBuffers;
    std::vector<std::array<void*, 2>> m_outputBuffers;
    std::function<void(int)> m_bufferSwitch;
    std::function<void(AsioMessage)> m_message;
};

} // namespace NereusSDR
