// =================================================================
// src/core/audio/AlsaDirectSystem.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AlsaDirectSystem.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-25, R-AUD-30,
//               R-AUD-32). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/audio/AlsaDirectSystem.h"

#include "core/LogCategories.h"
#include "core/audio/AlsaDirectBus.h"
#include "core/audio/AudioTestBarrier.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(Q_OS_LINUX)
#include <alsa/asoundlib.h>

#include <cerrno>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace {

const QString kGraphicalTarget = QStringLiteral("graphical.target");
const QString kDevSnd = QStringLiteral("/dev/snd");

} // namespace

bool coreBoxStartsIntoDesktop(IAlsaDirectSystem& system)
{
    const QString target = system.defaultTargetPath();
    if (target.isEmpty()) {
        return false;
    }
    return QFileInfo(target).fileName() == kGraphicalTarget;
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
    QString directory;
    std::function<void(AudioNotice)> sink;
    std::thread thread;
    std::atomic<bool> watchingDir{false};
#if defined(Q_OS_LINUX)
    int inotifyFd = -1;
    int stopFd = -1;
    int dirWd = -1;      // the thread's after start()
    int parentWd = -1;
    QByteArray dirPath;
    QByteArray parentPath;
    QByteArray dirName;

    // The directory, else its parent while it is missing.
    void watch()
    {
        dirWd = inotify_add_watch(inotifyFd, dirPath.constData(),
                                  IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM
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

    void run()
    {
        // Big enough for many events with names; allocated before the loop.
        std::vector<char> buffer(16 * (sizeof(inotify_event) + NAME_MAX + 1));
        pollfd fds[2] = {{inotifyFd, POLLIN, 0}, {stopFd, POLLIN, 0}};
        for (;;) {
            fds[0].revents = 0;
            fds[1].revents = 0;
            const int ready = poll(fds, 2, -1);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return;
            }
            if ((fds[1].revents & POLLIN) != 0) {
                return;
            }
            if ((fds[0].revents & POLLIN) == 0) {
                continue;
            }
            bool changed = false;
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
            if (changed && sink) {
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
    if (d.stopFd < 0) {
        close(d.inotifyFd);
        d.inotifyFd = -1;
        qCWarning(lcAudio) << "ALSA direct cannot watch" << d.directory << "for cards";
        return false;
    }
    d.watch();
    d.watchingDir.store(d.dirWd >= 0);
    if (d.dirWd < 0 && d.parentWd < 0) {
        qCWarning(lcAudio) << "ALSA direct cannot watch" << d.directory << "or its parent";
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
    if (d.thread.joinable()) {
        const std::uint64_t one = 1;
        const ssize_t written = write(d.stopFd, &one, sizeof(one));
        static_cast<void>(written);
        d.thread.join();
    }
    if (d.stopFd >= 0) {
        close(d.stopFd);
        d.stopFd = -1;
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

// ---------------------------------------------------------------------------
// The real adapter.
// ---------------------------------------------------------------------------
namespace {

#if defined(Q_OS_LINUX)
class RealAlsaDirectSystem final : public IAlsaDirectSystem {
public:
    ~RealAlsaDirectSystem() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_watcher.reset();
    }

    QList<AlsaCardRecord> playbackCards() override
    {
        QList<AlsaCardRecord> records;
        if (audioDevicesBarredForTestRun()) {
            return records;
        }
        snd_ctl_card_info_t* cardInfo = nullptr;
        snd_pcm_info_t* pcmInfo = nullptr;
        if (snd_ctl_card_info_malloc(&cardInfo) < 0 || snd_pcm_info_malloc(&pcmInfo) < 0) {
            snd_ctl_card_info_free(cardInfo);
            return records;
        }
        int card = -1;
        while (snd_card_next(&card) == 0 && card >= 0) {
            const QByteArray ctlName = QByteArrayLiteral("hw:") + QByteArray::number(card);
            snd_ctl_t* ctl = nullptr;
            if (snd_ctl_open(&ctl, ctlName.constData(), SND_CTL_NONBLOCK) < 0) {
                continue;
            }
            if (snd_ctl_card_info(ctl, cardInfo) < 0) {
                snd_ctl_close(ctl);
                continue;
            }
            const QString cardId = QString::fromUtf8(snd_ctl_card_info_get_id(cardInfo));
            const QString cardName = QString::fromUtf8(snd_ctl_card_info_get_name(cardInfo));
            const QString bus =
                QFileInfo::exists(QStringLiteral("/proc/asound/card%1/usbid").arg(card))
                    ? QStringLiteral("usb")
                    : QString();
            int device = -1;
            while (snd_ctl_pcm_next_device(ctl, &device) == 0 && device >= 0) {
                snd_pcm_info_set_device(pcmInfo, unsigned(device));
                snd_pcm_info_set_subdevice(pcmInfo, 0);
                snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_PLAYBACK);
                if (snd_ctl_pcm_info(ctl, pcmInfo) < 0) {
                    continue;   // capture only
                }
                AlsaCardRecord record;
                record.card = card;
                record.cardId = cardId;
                record.cardName = cardName;
                record.device = device;
                record.channels = 2;
                record.bus = bus;
                records.append(record);
            }
            snd_ctl_close(ctl);
        }
        snd_pcm_info_free(pcmInfo);
        snd_ctl_card_info_free(cardInfo);
        return records;
    }

    std::optional<int> configuredDefaultCard() override
    {
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

    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_watcher.reset();
        if (!sink || audioDevicesBarredForTestRun()) {
            return;
        }
        m_watcher = std::make_unique<AlsaNodeWatcher>(kDevSnd, std::move(sink));
        if (!m_watcher->start()) {
            m_watcher.reset();
        }
    }

    std::unique_ptr<IAudioBus> createOutput(const AlsaCardRecord& card,
                                            const AudioStreamRequest& request) override
    {
        return std::make_unique<AlsaDirectBus>(card, request, makeAlsaHwPcmOpener());
    }

    QString defaultTargetPath() override
    {
        return systemdDefaultTargetPath(systemdDefaultTargetCandidates());
    }

private:
    std::mutex m_mutex;
    std::unique_ptr<AlsaNodeWatcher> m_watcher;
};
#endif

} // namespace

std::unique_ptr<IAlsaDirectSystem> makeAlsaDirectSystem()
{
#if defined(Q_OS_LINUX)
    return std::make_unique<RealAlsaDirectSystem>();
#else
    return nullptr;
#endif
}

} // namespace NereusSDR
