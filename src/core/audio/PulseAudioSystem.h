// =================================================================
// src/core/audio/PulseAudioSystem.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The PulseAudio engine's system seam
// and its real adapter (R-AUD-01, R-AUD-07, R-AUD-31, R-AUD-32); no
// upstream logic.
//
// IPulseAudioSystem is everything PulseAudioBackend asks of the desktop's
// PulseAudio server: whether it answers (and its name, which the Linux
// engine selection reads), its sinks and sources, the default sink and
// source, its notices and its streams.  A test installs a fake.
//
// The real adapter (makePulseAudioSystem) runs one pa_context on a
// pa_threaded_mainloop of its own.  It connects when made, without
// spawning a server, and waits at most kPulseConnectWaitMs for the
// context and the server's info; a server that does not answer in that
// time gives a system that is not running and has no server name.  The
// context subscribes to sinks, sources and the server.  The subscribe
// callback runs on the mainloop's thread with its lock held and only
// posts: a sink or source that comes or goes posts DevicesChanged; a
// server event asks
// for the server's info, and that answer posts DefaultOutputChanged or
// DefaultInputChanged when the default sink or source changed.  devices()
// asks the server for its lists on the caller's thread (the catalogue's),
// bounded by the same wait.  Streams are pa_streams on the same mainloop
// (PulseAudioBus.h).  When the server goes away the system marks itself
// not running, forgets its name and defaults, posts DevicesChanged and
// posts DeviceLost to its open streams.  While
// audioDevicesBarredForTestRun() is true the adapter never connects and
// every open fails, so no test reaches a sound server.
//
// PulseServerState and the record helpers hold no libpulse types, so the
// subscribe logic is tested without a server.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-07, R-AUD-31,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace NereusSDR {

struct PulseDeviceRecord {
    QString name;              // pa_sink_info / pa_source_info name: the saved DeviceId
    QString description;       // the display name
    bool isSink = false;       // a sink is an output, a source an input
    bool isMonitor = false;    // a source that monitors a sink
    QStringList channelMap;    // pa_channel_position_to_string of each channel, e.g. front-left
    QString busProperty;       // device.bus: "usb", "bluetooth", "pci", ...
    int alsaCard = -1;         // alsa.card when present
    int alsaDevice = -1;       // alsa.device when present
    friend bool operator==(const PulseDeviceRecord&, const PulseDeviceRecord&) = default;
};

class IPulseAudioSystem {
public:
    virtual ~IPulseAudioSystem() = default;
    virtual bool running() = 0;                                 // the server answers
    virtual std::optional<QString> serverName() = 0;            // pa_server_info.server_name; nullopt when none answered
    virtual QList<PulseDeviceRecord> devices() = 0;             // every sink and source, monitors too (catalogue thread)
    virtual QString defaultName(AudioDeviceDirection direction) = 0;   // the server's default sink / source
    virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;
    virtual std::unique_ptr<IAudioBus> createOutput(const PulseDeviceRecord&, const AudioStreamRequest&) = 0;
    virtual std::unique_ptr<IAudioInputStream> createInput(const PulseDeviceRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
};

// How long the real adapter waits for a server to answer (the context and
// its server info), and for each list it asks for.
inline constexpr int kPulseConnectWaitMs = 1000;

// The stream buffer asked for (tlength and minreq, or fragsize for an
// input) when the request names none (design choice 11: today's default).
inline constexpr int kPulseDefaultBufferFrames = 128;

// True for a record the device lists show: it has a name and is not a monitor.
bool pulseDeviceIsListed(const PulseDeviceRecord& record);

// A record from a sink's or source's fields and its property list
// (device.bus, alsa.card, alsa.device).
PulseDeviceRecord pulseDeviceRecordFrom(const QString& name, const QString& description,
                                        bool isSink, bool isMonitor,
                                        const QStringList& channelMap,
                                        const QHash<QString, QString>& props);

// The subscription facilities the adapter listens to, and the event types.
enum class PulseFacility { Sink, Source, Server, Other };
enum class PulseEventType { New, Change, Remove };

// What the subscribe callback knows: the server's name and defaults.
// Every update posts its notice after its lock is released.
class PulseServerState {
public:
    void setNoticeSink(std::function<void(AudioNotice)> sink);

    // A subscription event.  A sink or source that comes or goes (New,
    // Remove) posts DevicesChanged; a Change (its volume, its port) posts
    // nothing, since the lists do not change with it.  Returns true for a
    // server event: the caller asks for the server's info, whose answer
    // goes to serverInfo().
    bool subscriptionEvent(PulseFacility facility, PulseEventType type);
    // The server's info.  The first answer only records it; a later one
    // posts DefaultOutputChanged when the default sink changed and
    // DefaultInputChanged when the default source changed.
    void serverInfo(const QString& serverName, const QString& defaultSink,
                    const QString& defaultSource);
    // The server went away: name and defaults are forgotten and
    // DevicesChanged is posted.
    void clear();

    std::optional<QString> serverName() const;
    QString defaultName(AudioDeviceDirection direction) const;

private:
    void post(AudioNotice notice);

    mutable QMutex m_mutex;
    std::optional<QString> m_serverName;
    QString m_defaultSink;
    QString m_defaultSource;

    std::mutex m_sinkMutex;
    std::function<void(AudioNotice)> m_sink;
};

// The real adapter on this desktop's PulseAudio server.  In a test run it
// never connects.
std::unique_ptr<IPulseAudioSystem> makePulseAudioSystem();

} // namespace NereusSDR
