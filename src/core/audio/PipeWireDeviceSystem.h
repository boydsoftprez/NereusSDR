// =================================================================
// src/core/audio/PipeWireDeviceSystem.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The PipeWire engine's system seam
// and its real adapter (R-AUD-01, R-AUD-03, R-AUD-07, R-AUD-14, R-AUD-32);
// no upstream logic.
//
// IPipeWireDeviceSystem is everything PipeWireDeviceBackend asks of the
// desktop's PipeWire: whether the daemon answers, its nodes, the default
// sink and source, its notices and its streams.  A test installs a fake.
//
// The real adapter (makePipeWireDeviceSystem) is a
// ReconnectingPipeWireDeviceSystem over connections to the daemon.  Each
// connection (IPipeWireConnection) runs its own thread loop.
// Its registry listener binds every audio device node and keeps the node's
// properties as a PipeWireNodeRecord in a PipeWireNodeDirectory; it binds
// the "default" metadata for default.audio.sink and default.audio.source.
// The listener runs on the thread loop's thread with the loop lock held
// and only posts: DevicesChanged when a device node comes, changes or
// goes, the default notices when the metadata's defaults change.  Streams
// are PipeWireStreams on that loop: an output reads a DeviceRateMatcher
// from its process callback, an input hands its pair to an
// IAudioInputSink.  While audioDevicesBarredForTestRun() is true the
// adapter never connects (running() is false, nothing is listed) and every
// open fails, so no test reaches a sound server.
//
// When the daemon goes away (a session restart, a Bluetooth reset), the
// connection marks itself not running, forgets its list, posts
// DevicesChanged and DeviceLost to its open streams, and calls its lost
// handler.  The system then tries a new connection every
// kPipeWireReconnectIntervalMs: a timer on the thread that made it starts
// each try on a worker thread (never a PipeWire callback, never an audio
// or DSP thread, never blocking the system's thread).  When one runs
// it becomes the current connection and the system posts DevicesChanged,
// so the stream supervisor reopens the chosen devices without a restart.
//
// The directory and the record helpers hold no PipeWire types, so the
// listener's logic is tested without a daemon.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 10 (R-AUD-01, R-AUD-03, R-AUD-07,
//               R-AUD-14). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireStream.h"
#endif

namespace NereusSDR {

struct PipeWireNodeRecord {
    std::uint32_t id = 0;
    QString nodeName;          // node.name: the saved DeviceId
    QString description;       // node.description: the display name
    QString mediaClass;        // "Audio/Sink", "Audio/Source", "Audio/Duplex"
    QString deviceApi;         // device.api: "alsa", "bluez5", ...
    QStringList positions;     // the node's audio.position, e.g. FL,FR or AUX0..AUX9
    int alsaCard = -1;         // api.alsa.pcm.card when present
    int alsaDevice = -1;       // api.alsa.pcm.device when present
    friend bool operator==(const PipeWireNodeRecord&, const PipeWireNodeRecord&) = default;
};

class IPipeWireDeviceSystem {
public:
    virtual ~IPipeWireDeviceSystem() = default;
    virtual bool running() = 0;                        // the daemon answers
    virtual QList<PipeWireNodeRecord> nodes() = 0;
    virtual QString defaultNodeName(AudioDeviceDirection) = 0;   // metadata default.audio.sink / default.audio.source
    virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;
    virtual std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord&, const AudioStreamRequest&) = 0;
    virtual std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
};

// The node buffer size asked for with node.latency when the request names
// none (design choice 11: today's default buffer).
inline constexpr int kPipeWireDefaultQuantumFrames = 128;

// How long the real adapter waits, once per connection, for the daemon's
// first list.
inline constexpr int kPipeWireConnectWaitSeconds = 2;

// How long the adapter waits between connection tries while the daemon
// does not answer.
inline constexpr int kPipeWireReconnectIntervalMs = 1500;

// True for a node the device lists show: an "Audio/Sink", "Audio/Source"
// or "Audio/Duplex" node that is not a monitor (node.name ending in
// ".monitor").  Stream nodes ("Stream/...") are programs, not devices.
bool pipeWireNodeIsListed(const PipeWireNodeRecord& node);

// A node's record from its properties (node.name, node.description,
// media.class, device.api, audio.position, audio.channels,
// api.alsa.pcm.card, api.alsa.pcm.device).  Without audio.position the
// positions come from audio.channels: MONO for 1, FL,FR for 2, AUX0 up for
// more.  nullopt when the node has no node.name.
std::optional<PipeWireNodeRecord> pipeWireNodeRecordFromProperties(
    std::uint32_t id, const QHash<QString, QString>& props);

// The node name in a "default" metadata value ({"name":"..."}); empty when
// the value holds none.
QString pipeWireMetadataDefaultName(const QString& value);

// What the registry listener knows.  Updates come from the thread loop;
// nodes() and defaultNodeName() from any thread.  Each update posts its
// notice through the sink after the directory's lock is released.
class PipeWireNodeDirectory {
public:
    void setNoticeSink(std::function<void(AudioNotice)> sink);

    // A node's properties arrived or changed: DevicesChanged when a listed
    // node is added or its record changes.  A node that is not listed is
    // forgotten (DevicesChanged if it was listed before).
    void nodeProperties(std::uint32_t id, const QHash<QString, QString>& props);
    // A registry global went away: DevicesChanged when it was a listed node.
    void nodeRemoved(std::uint32_t id);
    // A "default" metadata property (subject 0).  default.audio.sink posts
    // DefaultOutputChanged and default.audio.source DefaultInputChanged when
    // the name changes; a null key (every property cleared) clears both.
    void metadataProperty(std::uint32_t subject, const QString& key, bool keyIsNull,
                          const QString& value);
    // The daemon went away: everything is forgotten, DevicesChanged when
    // anything was listed.
    void clear();

    QList<PipeWireNodeRecord> nodes() const;
    QString defaultNodeName(AudioDeviceDirection direction) const;

private:
    void post(AudioNotice notice);

    mutable QMutex m_mutex;
    QMap<std::uint32_t, PipeWireNodeRecord> m_nodes;
    QString m_defaultSink;
    QString m_defaultSource;

    std::mutex m_sinkMutex;
    std::function<void(AudioNotice)> m_sink;
};

// One connection to the daemon.  The real one owns a thread loop and its
// registry listener; a test installs a fake.  directory() is what its
// listener knows.
class IPipeWireConnection {
public:
    virtual ~IPipeWireConnection() = default;
    virtual bool running() const = 0;
    virtual PipeWireNodeDirectory& directory() = 0;
    // Called at most once, from any thread, when the daemon goes away; by
    // then running() is false and the directory is clear.
    virtual void setLostHandler(std::function<void()> handler) = 0;
    virtual std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord&, const AudioStreamRequest&) = 0;
    virtual std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
};

// Makes a connection, running or not (a daemon that does not answer gives
// one that is not running).  The first call is on the system's thread when
// it is made; every retry calls it on a worker thread of its own.
using PipeWireConnector = std::function<std::shared_ptr<IPipeWireConnection>()>;

// The engine's system over a connector: it connects when made, on the
// calling thread and bounded by the connection's own wait, so the engine's
// running state is known at once.  While its connection is not running a
// timer on the event loop of the thread that made it (retryIntervalMs; 0:
// never again) starts one try on a worker thread; the next timer starts
// only after that try reports back, so one try runs at a time and the
// system's thread never waits on the daemon.  A try's connection is
// adopted on the system's thread; one that runs replaces the old one, and
// DevicesChanged and both default notices follow.  Streams keep the connection they opened
// on, so an old one lives until its last stream closes.  Every call but
// retrying() and the constructor may come from any thread.
class ReconnectingPipeWireDeviceSystem final : public IPipeWireDeviceSystem {
public:
    explicit ReconnectingPipeWireDeviceSystem(PipeWireConnector connector,
                                              int retryIntervalMs = kPipeWireReconnectIntervalMs);
    ~ReconnectingPipeWireDeviceSystem() override;   // stop()

    ReconnectingPipeWireDeviceSystem(const ReconnectingPipeWireDeviceSystem&) = delete;
    ReconnectingPipeWireDeviceSystem& operator=(const ReconnectingPipeWireDeviceSystem&) = delete;

    bool running() override;
    QList<PipeWireNodeRecord> nodes() override;
    QString defaultNodeName(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord& node,
                                            const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord& node,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

    // Cancels a pending try at once and detaches the current connection's
    // notices; the connector is never called again.  A try still running
    // on its worker is abandoned, never waited for: when it ends its
    // connection is discarded, never adopted.  From another thread (a
    // destructor there) stop() waits only for a call already running on
    // the system's thread, which never waits on the daemon.
    void stop();
    // A try is scheduled or running (the system's thread).
    bool retrying() const;

private:
    struct Forward;
    struct Relay;

    std::shared_ptr<IPipeWireConnection> current() const;
    void adopt(const std::shared_ptr<IPipeWireConnection>& connection);
    void onLost(const std::weak_ptr<IPipeWireConnection>& lost);
    void scheduleRetry();
    void retryNow();
    void finishTry(const std::shared_ptr<IPipeWireConnection>& next);

    PipeWireConnector m_connector;
    int m_retryIntervalMs;
    std::shared_ptr<Forward> m_forward;
    std::shared_ptr<Relay> m_relay;
    std::unique_ptr<QTimer> m_retryTimer;   // the system's thread; also the relay's target
    std::atomic<bool> m_stopped{false};
    bool m_tryRunning = false;   // the system's thread

    mutable std::mutex m_mutex;
    std::shared_ptr<IPipeWireConnection> m_current;
};

// The real adapter on this desktop's PipeWire: a
// ReconnectingPipeWireDeviceSystem whose connections wait at most
// kPipeWireConnectWaitSeconds for their first list.  In a test run it never
// connects and never retries.
std::unique_ptr<IPipeWireDeviceSystem> makePipeWireDeviceSystem();

#ifdef NEREUS_HAVE_PIPEWIRE
// The stream a device opens on `node` (an empty node follows the system
// default): our node name and class for the direction, target.object the
// node, node.latency from the request's buffer (kPipeWireDefaultQuantumFrames
// when it names none) and rate, and the request's pair.  A node of more than
// two positions opens with all of them, audio.position the node's own and
// stream.dont-remix, so the pair lands on its channels; a node of one or two
// opens as today.
StreamConfig pipeWireDeviceStreamConfig(const PipeWireNodeRecord& node,
                                        const AudioStreamRequest& request,
                                        AudioDeviceDirection direction);
#endif

} // namespace NereusSDR
