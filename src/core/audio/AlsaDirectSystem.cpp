// =================================================================
// src/core/audio/AlsaDirectSystem.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AlsaDirectSystem.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-25, R-AUD-30,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: final review fixes (R-AUD-07, R-AUD-25): IN_ATTRIB and one
//               more listing after -EACCES; channel counts from the card.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10: final review fixes round 2 (R-AUD-30): a box that starts
//               into a desktop probes only the card the Core plays on.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AlsaDirectSystem.h"

#include "core/LogCategories.h"
#include "core/audio/AlsaDirectBus.h"
#include "core/audio/AudioTestBarrier.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(Q_OS_LINUX)
#include <alsa/asoundlib.h>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace {

const QString kGraphicalTarget = QStringLiteral("graphical.target");
const QString kDevSnd = QStringLiteral("/dev/snd");
// A sanity cap on a probed channel maximum (some drivers answer UINT_MAX).
constexpr unsigned kAlsaMaxListedChannels = 64;

} // namespace

bool coreBoxStartsIntoDesktop(IAlsaDirectSystem& system)
{
    return defaultTargetIsDesktop(system.defaultTargetPath());
}

bool defaultTargetIsDesktop(const QString& targetPath)
{
    if (targetPath.isEmpty()) {
        return false;
    }
    return QFileInfo(targetPath).fileName() == kGraphicalTarget;
}

QList<DeviceSampleFormat> alsaFormatOrder()
{
    return {DeviceSampleFormat::Int32, DeviceSampleFormat::Int24Packed,
            DeviceSampleFormat::Int24In32Lsb, DeviceSampleFormat::Int16};
}

QStringList systemdDefaultTargetCandidates()
{
    return {QStringLiteral("/etc/systemd/system/default.target"),
            QStringLiteral("/lib/systemd/system/default.target"),
            QStringLiteral("/usr/lib/systemd/system/default.target")};
}

QString systemdDefaultTargetPath(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isSymLink()) {
            // A dangling link still names its target.
            return info.symLinkTarget();
        }
        if (info.exists()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

QString alsaDeviceId(const AlsaCardRecord& record)
{
    return record.cardId + QLatin1Char(',') + QString::number(record.device);
}

bool alsaNodeIsWatched(const QString& name)
{
    static const QRegularExpression kNode(
        QStringLiteral("^(controlC\\d+|pcmC\\d+D\\d+p)$"));
    return kNode.match(name).hasMatch();
}

// ---------------------------------------------------------------------------
// The inotify watcher.
// ---------------------------------------------------------------------------
struct AlsaNodeWatcher::Impl {
    using Clock = std::chrono::steady_clock;

    QString directory;
    std::function<void(AudioNotice)> sink;
    std::thread thread;
    std::atomic<bool> watchingDir{false};

    // relistOnceAfter(): the one more notice asked for, and whether it has
    // been used since the last node event's notice.
    std::mutex retryMutex;
    std::optional<Clock::time_point> retryAt;
    bool retryUsed = false;
    bool threadRunning = false;   // under retryMutex
#if defined(Q_OS_LINUX)
    int inotifyFd = -1;
    int stopFd = -1;
    int wakeFd = -1;     // relistOnceAfter() wakes the poll
    int dirWd = -1;      // the thread's after start()
    int parentWd = -1;
    QByteArray dirPath;
    QByteArray parentPath;
    QByteArray dirName;

    // The directory, else its parent while it is missing.
    void watch()
    {
        // IN_ATTRIB: udev gives a new node its group or ACL after the
        // kernel makes it root-only, and that change is all it raises.
        dirWd = inotify_add_watch(inotifyFd, dirPath.constData(),
                                  IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM | IN_ATTRIB
                                      | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR);
        if (dirWd >= 0) {
            if (parentWd >= 0) {
                inotify_rm_watch(inotifyFd, parentWd);
                parentWd = -1;
            }
            // watchingDir turns true only after the batch's notice is
            // posted (run(), start()), so a caller that sees it true has
            // every notice for the directory's return already.
            return;
        }
        watchingDir.store(false);
        if (parentWd < 0) {
            parentWd = inotify_add_watch(inotifyFd, parentPath.constData(),
                                         IN_CREATE | IN_MOVED_TO | IN_ONLYDIR);
        }
    }

    // The poll timeout: until the asked-for notice is due, else forever.
    int pollTimeoutMs()
    {
        std::lock_guard<std::mutex> lock(retryMutex);
        if (!retryAt) {
            return -1;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(*retryAt - Clock::now());
        return int(std::max<std::int64_t>(0, std::int64_t(left.count()) + 1));
    }

    // True when the asked-for notice is due now (it is then used).
    bool takeDueRetry()
    {
        std::lock_guard<std::mutex> lock(retryMutex);
        if (!retryAt || Clock::now() < *retryAt) {
            return false;
        }
        retryAt.reset();
        retryUsed = true;
        return true;
    }

    // A node event posted its notice: a refused listing may ask again,
    // and a pending ask is covered by the notice just posted.
    void nodeNoticePosted()
    {
        std::lock_guard<std::mutex> lock(retryMutex);
        retryAt.reset();
        retryUsed = false;
    }

    void run()
    {
        // Big enough for many events with names; allocated before the loop.
        std::vector<char> buffer(16 * (sizeof(inotify_event) + NAME_MAX + 1));
        pollfd fds[3] = {{inotifyFd, POLLIN, 0}, {stopFd, POLLIN, 0}, {wakeFd, POLLIN, 0}};
        for (;;) {
            fds[0].revents = 0;
            fds[1].revents = 0;
            fds[2].revents = 0;
            const int ready = poll(fds, 3, pollTimeoutMs());
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return;
            }
            if ((fds[1].revents & POLLIN) != 0) {
                return;
            }
            if ((fds[2].revents & POLLIN) != 0) {
                std::uint64_t count = 0;
                const ssize_t drained = read(wakeFd, &count, sizeof(count));
                static_cast<void>(drained);
            }
            bool changed = false;
            if ((fds[0].revents & POLLIN) != 0) {
                for (;;) {
                    const ssize_t got = read(inotifyFd, buffer.data(), buffer.size());
                    if (got <= 0) {
                        break;
                    }
                    ssize_t offset = 0;
                    while (offset + ssize_t(sizeof(inotify_event)) <= got) {
                        const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);
                        offset += ssize_t(sizeof(inotify_event)) + ssize_t(event->len);
                        const QByteArray name = event->len > 0 ? QByteArray(event->name) : QByteArray();
                        if (event->wd == dirWd && dirWd >= 0) {
                            if ((event->mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
                                // The directory went: wait for it in its parent.
                                if ((event->mask & IN_IGNORED) == 0) {
                                    inotify_rm_watch(inotifyFd, dirWd);
                                }
                                dirWd = -1;
                                watch();
                                changed = true;
                            } else if (alsaNodeIsWatched(QString::fromLatin1(name))) {
                                changed = true;
                            }
                        } else if (event->wd == parentWd && parentWd >= 0 && name == dirName) {
                            watch();
                            changed = changed || dirWd >= 0;
                        }
                    }
                }
            }
            if (changed && sink) {
                sink(AudioNotice::DevicesChanged);
                nodeNoticePosted();
            } else if (takeDueRetry() && sink) {
                sink(AudioNotice::DevicesChanged);
            }
            watchingDir.store(dirWd >= 0);
        }
    }
#endif
};

AlsaNodeWatcher::AlsaNodeWatcher(QString directory, std::function<void(AudioNotice)> sink)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->directory = std::move(directory);
    m_impl->sink = std::move(sink);
}

AlsaNodeWatcher::~AlsaNodeWatcher()
{
    stop();
}

bool AlsaNodeWatcher::start()
{
#if defined(Q_OS_LINUX)
    Impl& d = *m_impl;
    if (d.thread.joinable()) {
        return true;
    }
    const QFileInfo info(d.directory);
    d.dirPath = QFile::encodeName(info.absoluteFilePath());
    d.parentPath = QFile::encodeName(info.absolutePath());
    d.dirName = QFile::encodeName(info.fileName());
    d.inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (d.inotifyFd < 0) {
        qCWarning(lcAudio) << "ALSA direct cannot watch" << d.directory << "for cards";
        return false;
    }
    d.stopFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    d.wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (d.stopFd < 0 || d.wakeFd < 0) {
        for (int* fd : {&d.stopFd, &d.wakeFd, &d.inotifyFd}) {
            if (*fd >= 0) {
                close(*fd);
                *fd = -1;
            }
        }
        qCWarning(lcAudio) << "ALSA direct cannot watch" << d.directory << "for cards";
        return false;
    }
    d.watch();
    d.watchingDir.store(d.dirWd >= 0);
    if (d.dirWd < 0 && d.parentWd < 0) {
        qCWarning(lcAudio) << "ALSA direct cannot watch" << d.directory << "or its parent";
    }
    {
        std::lock_guard<std::mutex> lock(d.retryMutex);
        d.retryAt.reset();
        d.retryUsed = false;
        d.threadRunning = true;
    }
    d.thread = std::thread([&d] { d.run(); });
    return true;
#else
    return false;
#endif
}

void AlsaNodeWatcher::stop()
{
#if defined(Q_OS_LINUX)
    Impl& d = *m_impl;
    {
        std::lock_guard<std::mutex> lock(d.retryMutex);
        d.threadRunning = false;
        d.retryAt.reset();
    }
    if (d.thread.joinable()) {
        const std::uint64_t one = 1;
        const ssize_t written = write(d.stopFd, &one, sizeof(one));
        static_cast<void>(written);
        d.thread.join();
    }
    for (int* fd : {&d.stopFd, &d.wakeFd}) {
        if (*fd >= 0) {
            close(*fd);
            *fd = -1;
        }
    }
    if (d.inotifyFd >= 0) {
        close(d.inotifyFd);   // removes every watch
        d.inotifyFd = -1;
    }
    d.dirWd = -1;
    d.parentWd = -1;
    d.watchingDir.store(false);
#endif
}

bool AlsaNodeWatcher::watchingDirectory() const
{
    return m_impl->watchingDir.load();
}

void AlsaNodeWatcher::relistOnceAfter(int ms)
{
#if defined(Q_OS_LINUX)
    Impl& d = *m_impl;
    std::lock_guard<std::mutex> lock(d.retryMutex);
    if (!d.threadRunning || d.retryUsed || d.retryAt) {
        return;
    }
    d.retryAt = Impl::Clock::now() + std::chrono::milliseconds(std::max(0, ms));
    const std::uint64_t one = 1;
    const ssize_t written = write(d.wakeFd, &one, sizeof(one));
    static_cast<void>(written);
#else
    static_cast<void>(ms);
#endif
}

// ---------------------------------------------------------------------------
// The card listing.
// ---------------------------------------------------------------------------
AlsaCardListing listAlsaPlaybackCards(IAlsaCardApi& api, QHash<QString, int>& knownChannels,
                                      const std::function<bool(const QString&)>& probe)
{
    AlsaCardListing listing;
    for (const int card : api.cardNumbers()) {
        AlsaCardControlInfo control;
        const int rc = api.readControl(card, control);
        if (rc < 0) {
            if (rc == -EACCES) {
                // udev has not given the node its group yet.
                listing.refused = true;
            }
            continue;
        }
        const QString bus = api.cardOnUsb(card) ? QStringLiteral("usb") : QString();
        for (const int device : control.playbackDevices) {
            AlsaCardRecord record;
            record.card = card;
            record.cardId = control.cardId;
            record.cardName = control.cardName;
            record.device = device;
            record.bus = bus;
            const QString id = alsaDeviceId(record);
            // Not probed: as a busy probe, the count read before stands.
            const int channels = !probe || probe(id) ? api.playbackChannelsMax(card, device) : -EBUSY;
            if (channels > 0) {
                record.channels = channels;
                knownChannels.insert(id, channels);
            } else {
                if (channels == -EACCES) {
                    listing.refused = true;
                }
                record.channels = knownChannels.value(id, 2);
            }
            listing.records.append(record);
        }
    }
    return listing;
}

// ---------------------------------------------------------------------------
// The machine's ALSA.
// ---------------------------------------------------------------------------
namespace {

#if defined(Q_OS_LINUX)
class RealAlsaCardApi final : public IAlsaCardApi {
public:
    QList<int> cardNumbers() override
    {
        QList<int> cards;
        if (audioDevicesBarredForTestRun()) {
            return cards;
        }
        int card = -1;
        while (snd_card_next(&card) == 0 && card >= 0) {
            cards.append(card);
        }
        return cards;
    }

    int readControl(int card, AlsaCardControlInfo& info) override
    {
        if (audioDevicesBarredForTestRun()) {
            return -ENODEV;
        }
        snd_ctl_card_info_t* cardInfo = nullptr;
        snd_pcm_info_t* pcmInfo = nullptr;
        if (snd_ctl_card_info_malloc(&cardInfo) < 0) {
            return -ENOMEM;
        }
        if (snd_pcm_info_malloc(&pcmInfo) < 0) {
            snd_ctl_card_info_free(cardInfo);
            return -ENOMEM;
        }
        const QByteArray ctlName = QByteArrayLiteral("hw:") + QByteArray::number(card);
        snd_ctl_t* ctl = nullptr;
        int rc = snd_ctl_open(&ctl, ctlName.constData(), SND_CTL_NONBLOCK);
        if (rc >= 0) {
            rc = snd_ctl_card_info(ctl, cardInfo);
            if (rc >= 0) {
                info.cardId = QString::fromUtf8(snd_ctl_card_info_get_id(cardInfo));
                info.cardName = QString::fromUtf8(snd_ctl_card_info_get_name(cardInfo));
                int device = -1;
                while (snd_ctl_pcm_next_device(ctl, &device) == 0 && device >= 0) {
                    snd_pcm_info_set_device(pcmInfo, unsigned(device));
                    snd_pcm_info_set_subdevice(pcmInfo, 0);
                    snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_PLAYBACK);
                    if (snd_ctl_pcm_info(ctl, pcmInfo) < 0) {
                        continue;   // capture only
                    }
                    info.playbackDevices.append(device);
                }
            }
            snd_ctl_close(ctl);
        }
        snd_pcm_info_free(pcmInfo);
        snd_ctl_card_info_free(cardInfo);
        return rc < 0 ? rc : 0;
    }

    int playbackChannelsMax(int card, int device) override
    {
        if (audioDevicesBarredForTestRun()) {
            return -ENODEV;
        }
        const QByteArray name = QStringLiteral("hw:%1,%2").arg(card).arg(device).toUtf8();
        snd_pcm_t* pcm = nullptr;
        // Non-blocking: a PCM a program plays on answers -EBUSY at once.
        int rc = snd_pcm_open(&pcm, name.constData(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (rc < 0) {
            return rc;
        }
        snd_pcm_hw_params_t* hw = nullptr;
        rc = snd_pcm_hw_params_malloc(&hw);
        if (rc >= 0) {
            rc = snd_pcm_hw_params_any(pcm, hw);
            unsigned channels = 0;
            if (rc >= 0) {
                rc = snd_pcm_hw_params_get_channels_max(hw, &channels);
            }
            if (rc >= 0) {
                rc = int(std::min<unsigned>(channels, kAlsaMaxListedChannels));
            }
            snd_pcm_hw_params_free(hw);
        }
        snd_pcm_close(pcm);
        return rc;
    }

    bool cardOnUsb(int card) override
    {
        return QFileInfo::exists(QStringLiteral("/proc/asound/card%1/usbid").arg(card));
    }

    std::optional<int> configuredDefaultCard() override
    {
        if (audioDevicesBarredForTestRun()) {
            return std::nullopt;
        }
        snd_config_t* top = nullptr;
        if (snd_config_update_ref(&top) < 0 || top == nullptr) {
            return std::nullopt;
        }
        std::optional<int> result;
        snd_config_t* node = nullptr;
        if (snd_config_search(top, "defaults.pcm.card", &node) >= 0 && node != nullptr) {
            // An integer, or a card ID that names a card now.
            const int card = snd_config_get_card(node);
            if (card >= 0) {
                result = card;
            }
        }
        snd_config_unref(top);
        return result;
    }
};
#else
class NoAlsaCardApi final : public IAlsaCardApi {
public:
    QList<int> cardNumbers() override { return {}; }
    int readControl(int, AlsaCardControlInfo&) override { return -ENODEV; }
    int playbackChannelsMax(int, int) override { return -ENODEV; }
    bool cardOnUsb(int) override { return false; }
    std::optional<int> configuredDefaultCard() override { return std::nullopt; }
};
#endif

} // namespace

std::shared_ptr<IAlsaCardApi> makeAlsaCardApi()
{
#if defined(Q_OS_LINUX)
    return std::make_shared<RealAlsaCardApi>();
#else
    return std::make_shared<NoAlsaCardApi>();
#endif
}

// ---------------------------------------------------------------------------
// The system.
// ---------------------------------------------------------------------------
AlsaDirectCardSystem::AlsaDirectCardSystem(std::shared_ptr<IAlsaCardApi> api, QString watchDirectory,
                                           AlsaOutputMaker makeOutput, int refusedRelistMs,
                                           bool startsIntoDesktop)
    : m_api(std::move(api))
    , m_watchDirectory(std::move(watchDirectory))
    , m_makeOutput(std::move(makeOutput))
    , m_refusedRelistMs(refusedRelistMs)
    , m_startsIntoDesktop(startsIntoDesktop)
{
}

AlsaDirectCardSystem::~AlsaDirectCardSystem()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watcher.reset();
}

QList<AlsaCardRecord> AlsaDirectCardSystem::playbackCards()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_api) {
        return {};
    }
    std::function<bool(const QString&)> probe;
    if (m_startsIntoDesktop) {
        // D31: the desktop's cards are left alone; only the card of the
        // Core's live stream (none while nothing is picked) is probed.
        const QString picked = m_pickHold.expired() ? QString() : m_pickedId;
        probe = [picked](const QString& id) { return !picked.isEmpty() && id == picked; };
    }
    AlsaCardListing listing = listAlsaPlaybackCards(*m_api, m_knownChannels, probe);
    if (listing.refused && m_watcher) {
        m_watcher->relistOnceAfter(m_refusedRelistMs);
    }
    return listing.records;
}

std::optional<int> AlsaDirectCardSystem::configuredDefaultCard()
{
    return m_api ? m_api->configuredDefaultCard() : std::nullopt;
}

void AlsaDirectCardSystem::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watcher.reset();
    if (!sink || m_watchDirectory.isEmpty()) {
        return;
    }
    m_watcher = std::make_unique<AlsaNodeWatcher>(m_watchDirectory, std::move(sink));
    if (!m_watcher->start()) {
        m_watcher.reset();
    }
}

std::unique_ptr<IAudioBus> AlsaDirectCardSystem::createOutput(const AlsaCardRecord& card,
                                                              const AudioStreamRequest& request)
{
    AlsaCardRecord picked = card;
    std::shared_ptr<void> hold = std::make_shared<int>(0);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const QString id = alsaDeviceId(card);
        if (m_startsIntoDesktop && m_api && !m_knownChannels.contains(id)) {
            // The pick's card is the Core's now: its count is read once,
            // here, before its stream opens it.
            const int channels = m_api->playbackChannelsMax(card.card, card.device);
            if (channels > 0) {
                m_knownChannels.insert(id, channels);
                picked.channels = channels;
            }
        }
        m_pickedId = id;
        m_pickHold = hold;
    }
    return m_makeOutput ? m_makeOutput(picked, request, std::move(hold)) : nullptr;
}

QString AlsaDirectCardSystem::defaultTargetPath()
{
    return systemdDefaultTargetPath(systemdDefaultTargetCandidates());
}

std::unique_ptr<IAlsaDirectSystem> makeAlsaDirectSystem()
{
#if defined(Q_OS_LINUX)
    // In a test run nothing is watched; the API lists nothing and every
    // open fails as well.
    const QString directory = audioDevicesBarredForTestRun() ? QString() : kDevSnd;
    const bool desktop =
        defaultTargetIsDesktop(systemdDefaultTargetPath(systemdDefaultTargetCandidates()));
    return std::make_unique<AlsaDirectCardSystem>(
        makeAlsaCardApi(), directory,
        [](const AlsaCardRecord& card, const AudioStreamRequest& request,
           std::shared_ptr<void> hold) -> std::unique_ptr<IAudioBus> {
            return std::make_unique<AlsaDirectBus>(card, request, makeAlsaHwPcmOpener(),
                                                   std::move(hold));
        },
        kAlsaRefusedRelistMs, desktop);
#else
    return nullptr;
#endif
}

} // namespace NereusSDR
