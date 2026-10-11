// =================================================================
// src/core/audio/AsioSession.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The portable ASIO rules: one driver
// at a time (R-AUD-19), one buffer size and rate for the whole session
// (R-AUD-20), a reset restarts outside the callback (R-AUD-21), and the
// buffer switch that moves every use's audio between the driver's planar
// buffers and the shared rings.  It runs in the mic helper (D33) over an
// IAsioDriver; the Windows adapter and the tests' fake are the drivers.
//
// The set-up order follows cmASIO's create_cmasio (cmasio.cpp, the port);
// the host calls follow the Steinberg SDK's host sample.
//
// Design: docs/architecture/2026-10-08-native-audio-engines-design.md
// ("ASIO"; D3, D9, D14, D15, D33).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-07, R-AUD-21):
//               outputs on one pair mix; a reset adopts the driver's new
//               buffer size and rate; a mic on the driver's last input runs
//               on that one channel; runningPair(); the session pointer is
//               set before the buffers are made.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AsioSessionNotifier.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAsioDriver.h"
#include "core/audio/IAudioStreamHost.h"

#include <QList>
#include <QString>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace NereusSDR {

class IAudioInputSink;
class MatcherReader;

struct AsioUse { AudioRole role; QString driver; AudioChannelPair pair; AudioDeviceDirection direction; };
struct AsioSwitchPlan { QString toDriver; QList<AsioUse> moves; };   // every other ASIO use and the pair it moves to

// Settled call 17: each use keeps its pair numbers where the new driver has them, else its first pair.
AsioSwitchPlan planAsioSwitch(const QList<AsioUse>& current, const AsioUse& requested, const AsioDriverCaps& newDriver);
bool asioBufferSizeFixed(const AsioDriverCaps&);                     // min == max (asio.h:646-648)
std::optional<DeviceSampleFormat> asioDeviceFormat(AsioSampleType);   // nullopt: unsupported (settled call 28)

// The buffer size the session runs at for a request (R-AUD-20): the
// driver's only size when it is fixed, its preferred size for 0, else the
// request moved onto the driver's steps between its minimum and maximum.
int asioSessionBufferFrames(const AsioDriverCaps& caps, int requestedFrames);

// Where one use's audio comes from or goes, parallel to open()'s uses.
// An output use reads its reader into its pair; an input use hands its
// pair, picked to stereo, to its sink (which writes the use's ring and
// posts its wake).  A use with neither plays silence or is dropped.
struct AsioEndpoint {
    MatcherReader* reader = nullptr;         // an output use
    IAudioInputSink* sink = nullptr;         // an input use
    MicChannelPick pick = MicChannelPick::Both;
};

class AsioSession {                       // runs in the helper; one driver at a time (R-AUD-19)
public:
    explicit AsioSession(IAsioDriver& driver);
    ~AsioSession();
    AsioSession(const AsioSession&) = delete;
    AsioSession& operator=(const AsioSession&) = delete;

    // Opens the driver with these uses, closing whatever ran before.  The
    // buffer size and rate are the session's: the latest open's values
    // apply to every use (R-AUD-20).  rate 0 keeps the driver's own.
    // endpoints is parallel to uses (missing entries play silence).  Uses
    // on the same output channels are added together (R-AUD-07).  An input
    // pair that starts on the driver's last input runs on that one channel;
    // a use whose channels are not on the driver is dropped (runningPair).
    bool open(const QString& driver, int bufferFrames, double rate, const QList<AsioUse>& uses,
              const QList<AsioEndpoint>& endpoints = {});
    void close();
    void onDriverMessage(AsioMessage message);      // from the driver's thread: only posts
    int restartCount() const;
    AsioDriverCaps caps() const;

    // The QObject the session signals through: restarted(), failed(QString).
    // It lives on the thread that made the session, where a reset runs.
    AsioSessionNotifier* notifier() const;

    // Beyond the plan's list (Task 15 report).
    bool isOpen() const;
    QString driverName() const;
    int bufferFrames() const;              // the size the session runs at
    double sampleRate() const;             // the rate the session runs at
    AsioDriverFailure lastFailure() const; // why the last open or restart failed
    QString errorString() const;
    // The channels use `index` (of open()'s uses) runs on now; nullopt while
    // the session is closed or when the use was dropped.
    std::optional<AudioChannelPair> runningPair(int index) const;

    // The buffer switch, as the driver's callback calls it (through the
    // one static pointer the SDK's C callbacks need).  No lock, no
    // allocation, no Qt call.
    void bufferSwitch(int bufferIndex);

private:
    struct OutputOp {
        MatcherReader* reader = nullptr;
        int leftSlot = -1;                // its first channel's mix slot
        int rightSlot = -1;               // -1: a one-channel pair, folded to one
    };
    // One output channel some use plays on: its mix, written to the driver
    // once per switch.
    struct MixChannel {
        int channel = 0;                                // 0-based
        std::array<std::vector<void*>, 2> planes;       // only this channel is set
    };
    struct InputOp {
        IAudioInputSink* sink = nullptr;
        AudioChannelPair pair;
        MicChannelPick pick = MicChannelPick::Both;
        std::array<std::vector<void*>, 2> planes;
    };

    static void dispatchBufferSwitch(int bufferIndex);
    static void dispatchMessage(AsioMessage message);

    bool start();
    void stopAndUnload();
    void restart();
    void fail(AsioDriverFailure failure, const QString& detail);

    static std::atomic<AsioSession*> s_current;

    IAsioDriver& m_driver;
    std::unique_ptr<AsioSessionNotifier> m_notifier;

    // What the session was asked for (the latest open).
    QString m_driverName;
    int m_requestedFrames = 0;
    double m_requestedRate = 0.0;
    QList<AsioUse> m_uses;
    QList<AsioEndpoint> m_endpoints;

    // What runs now; written only while no callback runs.
    bool m_loaded = false;
    bool m_running = false;
    AsioDriverCaps m_caps;
    int m_frames = 0;
    double m_rate = 0.0;
    DeviceSampleFormat m_format = DeviceSampleFormat::Int32;
    std::int64_t m_inputLatencyNs = 0;
    std::vector<OutputOp> m_outputs;
    std::vector<InputOp> m_inputs;
    std::vector<MixChannel> m_mixChannels;
    std::vector<float> m_mix;         // m_mixChannels.size() buffers of m_frames, sized at start
    std::vector<float> m_stereo;      // one buffer of stereo, sized at start
    QList<std::optional<AudioChannelPair>> m_runningPairs;   // parallel to m_uses
    std::atomic<bool> m_switchReady{false};   // the ops above are complete
    int m_restarts = 0;
    AsioDriverFailure m_failure = AsioDriverFailure::None;
    QString m_error;
    std::atomic<bool> m_resetPosted{false};
};

} // namespace NereusSDR
