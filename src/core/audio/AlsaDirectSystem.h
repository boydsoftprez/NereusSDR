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
// The real adapter (makeAlsaDirectSystem) is an AlsaDirectCardSystem over
// the machine's ALSA (IAlsaCardApi): it lists cards through each card's
// control device (snd_ctl_*), reads the USB bus from
// /proc/asound/cardN/usbid and the most channels each playback PCM plays
// from a short non-blocking probe open.  While
// audioDevicesBarredForTestRun() is true it lists nothing, watches nothing
// and every open fails, so no test reaches a card.
//
// udev gives a new card's nodes their group (or the seat's ACL) a moment
// after the kernel makes them root-only.  The watcher sees that change
// (IN_ATTRIB), and a listing that a card's node refused (-EACCES) asks the
// watcher for one more listing a moment later, so a card plugged in while
// udev is slow still comes in (R-AUD-25).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-25, R-AUD-30,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: final review fixes (R-AUD-07, R-AUD-25): IN_ATTRIB and one
//               more listing after -EACCES; channel counts from the card;
//               the listing behind IAlsaCardApi. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <mutex>
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
// watched node is created, deleted, moved in or out, or has its
// attributes changed (udev setting its group or ACL).  Events for other
// names are ignored.  When the directory does not exist the watcher
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
    // Posts one more DevicesChanged `ms` from now, from the watcher's
    // thread: a listing a card's node refused asks for it.  One at a
    // time, and once only until the next node event posts a notice, so a
    // card that stays refused is not listed again and again.  Any thread;
    // nothing while stopped.
    void relistOnceAfter(int ms);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// How long after a refused listing the watcher posts its one more
// DevicesChanged: time for udev to give the new nodes their group.
inline constexpr int kAlsaRefusedRelistMs = 1000;

// What the card listing reads from one card's control device.
struct AlsaCardControlInfo {
    QString cardId;                // snd_ctl_card_info_get_id
    QString cardName;              // snd_ctl_card_info_get_name
    QList<int> playbackDevices;    // the PCM devices with a playback stream
};

// The ALSA calls the real adapter makes, so a test lists fake cards
// through the adapter's own logic.  The real one (makeAlsaCardApi) lists
// nothing while audioDevicesBarredForTestRun() is true.
class IAlsaCardApi {
public:
    virtual ~IAlsaCardApi() = default;
    virtual QList<int> cardNumbers() = 0;                       // snd_card_next
    // Reads the card's control device: 0, or the -errno snd_ctl_open (or
    // the info read) answered; -EACCES while udev has not given the node
    // its group.
    virtual int readControl(int card, AlsaCardControlInfo& info) = 0;
    // The most channels hw:<card>,<device> plays
    // (snd_pcm_hw_params_get_channels_max on a SND_PCM_NONBLOCK open,
    // closed at once): the count, or -errno when it did not open (-EBUSY
    // while a program plays on it).
    virtual int playbackChannelsMax(int card, int device) = 0;
    virtual bool cardOnUsb(int card) = 0;                       // /proc/asound/card<N>/usbid exists
    virtual std::optional<int> configuredDefaultCard() = 0;     // defaults.pcm.card when set
};

// One listing of the cards with playback.
struct AlsaCardListing {
    QList<AlsaCardRecord> records;
    bool refused = false;   // a card's node answered -EACCES
};

// Lists the playback PCMs of `api`'s cards.  A card whose control device
// does not open is skipped (refused when it answered -EACCES).  A PCM's
// channels are its probed maximum; while the probe cannot open it (busy,
// often because we play on it) the count last read for its ID in
// `knownChannels` stands, else 2.  Each count read is stored there.
AlsaCardListing listAlsaPlaybackCards(IAlsaCardApi& api, QHash<QString, int>& knownChannels);

// The machine's ALSA.
std::shared_ptr<IAlsaCardApi> makeAlsaCardApi();

// Makes a card's output stream (the real one an AlsaDirectBus on the real
// opener).
using AlsaOutputMaker =
    std::function<std::unique_ptr<IAudioBus>(const AlsaCardRecord&, const AudioStreamRequest&)>;

// The ALSA direct system over an IAlsaCardApi: the real adapter, and a
// test's over a fake API, a temporary directory and fake streams.  The
// watcher runs on `watchDirectory` while a notice sink is set (none for
// an empty directory).  A listing a card refused asks the watcher for one
// more after `refusedRelistMs`.
class AlsaDirectCardSystem final : public IAlsaDirectSystem {
public:
    AlsaDirectCardSystem(std::shared_ptr<IAlsaCardApi> api, QString watchDirectory,
                         AlsaOutputMaker makeOutput, int refusedRelistMs = kAlsaRefusedRelistMs);
    ~AlsaDirectCardSystem() override;

    QList<AlsaCardRecord> playbackCards() override;
    std::optional<int> configuredDefaultCard() override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    std::unique_ptr<IAudioBus> createOutput(const AlsaCardRecord& card,
                                            const AudioStreamRequest& request) override;
    QString defaultTargetPath() override;

private:
    std::shared_ptr<IAlsaCardApi> m_api;
    QString m_watchDirectory;
    AlsaOutputMaker m_makeOutput;
    int m_refusedRelistMs;
    std::mutex m_mutex;
    QHash<QString, int> m_knownChannels;
    std::unique_ptr<AlsaNodeWatcher> m_watcher;
};

// The real adapter on this machine's ALSA cards.  In a test run it lists
// nothing, watches nothing and every open fails.
std::unique_ptr<IAlsaDirectSystem> makeAlsaDirectSystem();

} // namespace NereusSDR
