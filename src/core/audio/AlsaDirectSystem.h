// =================================================================
// src/core/audio/AlsaDirectSystem.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The ALSA direct engine's system seam
// and its real adapter (R-AUD-01, R-AUD-25, R-AUD-30, R-AUD-32, D18, D31);
// no upstream logic.
//
// The headless Linux Core has no desktop session, so no PipeWire or
// PulseAudio: it plays straight to a sound card's hw: PCM (D18).
// IAlsaDirectSystem is everything AlsaDirectBackend asks of the machine:
// its cards with a playback PCM, ALSA's configured default card, card
// notices, its output streams and where systemd's default.target points.
// A test installs a fake.
//
// Cards are named as ALSA names them and saved by card ID and device
// number (design choice 4).  Cards coming and going are seen through
// inotify on /dev/snd (design choice 5, no library added): a controlC* or
// pcmC*D*p node created or deleted posts DevicesChanged from the watcher's
// own thread, which only posts.  When /dev/snd does not exist yet (no card
// at boot) the watcher waits for it in its parent directory.
//
// The real adapter (makeAlsaDirectSystem) lists cards through each card's
// control device (snd_ctl_*), never a PCM, and reads the USB bus from
// /proc/asound/cardN/usbid.  While audioDevicesBarredForTestRun() is true
// it lists nothing, watches nothing and every open fails, so no test
// reaches a card.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-25, R-AUD-30,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

struct AlsaCardRecord {
    int card = -1;
    QString cardId;            // snd_ctl_card_info_get_id, e.g. "Headphones", "Device"
    QString cardName;          // snd_ctl_card_info_get_name, e.g. "bcm2835 Headphones"
    int device = 0;            // PCM device number with playback
    int channels = 2;
    QString bus;               // "usb" when the card's /proc/asound/cardN/usbid exists, else ""
    friend bool operator==(const AlsaCardRecord&, const AlsaCardRecord&) = default;
};

class IAlsaDirectSystem {
public:
    virtual ~IAlsaDirectSystem() = default;
    virtual QList<AlsaCardRecord> playbackCards() = 0;
    virtual std::optional<int> configuredDefaultCard() = 0;   // ALSA's defaults.pcm.card when set
    virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;  // inotify on /dev/snd, own thread
    virtual std::unique_ptr<IAudioBus> createOutput(const AlsaCardRecord&, const AudioStreamRequest&) = 0;
    virtual QString defaultTargetPath() = 0;                  // where default.target points, "" when absent
};

// Settled call 13: a box "starts into a desktop" when default.target resolves to
// graphical.target, read from /etc/systemd/system/default.target, else
// /lib/systemd/system/default.target, else /usr/lib/systemd/system/default.target.
bool coreBoxStartsIntoDesktop(IAlsaDirectSystem& system);
// Settled call 14: S32_LE, S24_3LE, S24_LE, S16_LE.
QList<DeviceSampleFormat> alsaFormatOrder();

// Settled call 13's candidates, in the order they are read.
QStringList systemdDefaultTargetCandidates();

// Where the first candidate that exists points (its symlink target, made
// absolute); a candidate that exists and is not a symlink gives its own
// path.  "" when none exists.  No systemctl call.
QString systemdDefaultTargetPath(const QStringList& candidates);

// The saved identity of a card's PCM device: "<cardId>,<device>".
QString alsaDeviceId(const AlsaCardRecord& record);

// True for a /dev/snd node whose coming or going changes the card list:
// a card's control node (controlC<n>) or a playback PCM (pcmC<n>D<m>p).
bool alsaNodeIsWatched(const QString& name);

// Watches a directory (/dev/snd) with inotify on a thread of its own and
// calls the sink with DevicesChanged, once per batch of events, when a
// watched node is created or deleted (or moved in or out).  Events for
// other names are ignored.  When the directory does not exist the watcher
// waits for it to be created in its parent and then watches it (posting
// DevicesChanged then too); when it is deleted it waits again.  The thread
// only posts.  Linux only: elsewhere start() returns false.
class AlsaNodeWatcher {
public:
    AlsaNodeWatcher(QString directory, std::function<void(AudioNotice)> sink);
    ~AlsaNodeWatcher();   // stop()

    AlsaNodeWatcher(const AlsaNodeWatcher&) = delete;
    AlsaNodeWatcher& operator=(const AlsaNodeWatcher&) = delete;

    // False when inotify cannot start.  Once it returns true, the
    // directory (or its parent, while it is missing) is watched.
    bool start();
    void stop();
    // The directory itself is watched (not only its parent), and every
    // notice for its coming back has been posted: a node change made after
    // this reads true posts a notice of its own.  Any thread.
    bool watchingDirectory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// The real adapter on this machine's ALSA cards.  In a test run it lists
// nothing, watches nothing and every open fails.
std::unique_ptr<IAlsaDirectSystem> makeAlsaDirectSystem();

} // namespace NereusSDR
