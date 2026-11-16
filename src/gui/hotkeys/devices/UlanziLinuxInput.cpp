// =================================================================
// src/gui/hotkeys/devices/UlanziLinuxInput.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - Ulanzi Dial on Linux built on AetherSDR
//                 src/core/EvdevEncoderManager.cpp [@1e0718ad] for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. The device sits behind a seam so a test never opens one,
//                 and nothing is looked at until the application allows it. It
//                 lets go of the grab before it closes. An access refusal is
//                 reported on the Knobs page.
// =================================================================

#include "gui/hotkeys/devices/UlanziLinuxInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/DeviceInput.h"

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QTimer>

#include <utility>

#if defined(Q_OS_LINUX)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace {

// From AetherSDR src/core/EvdevEncoderManager.cpp:31 [@1e0718ad]
const char* const kDialKeyboardName = "Ulanzi Dial Keyboard";

// Linux include/uapi/linux/input-event-codes.h:40 [@f66469d9]. Local, since a Mac has
// no such header and its tests run this.
constexpr quint16 kEvKey = 0x01;

UlanziLinuxInput::LayerFactory& testLayerFactory()
{
    static UlanziLinuxInput::LayerFactory kFactory;
    return kFactory;
}

UlanziLinuxInput::Scheduler& testScheduler()
{
    static UlanziLinuxInput::Scheduler kScheduler;
    return kScheduler;
}

// Nothing on it, for a process that hasn't allowed real devices. It reads no name,
// watches no directory and opens nothing.
class NoDialLinuxLayer : public UlanziLinuxLayer {
public:
    QString findNode() override
    {
        return {};
    }

    QString watchedDirectory() override
    {
        return {};
    }

    std::unique_ptr<UlanziEvdevHandle> open(const QString& /*node*/, QString* /*detail*/) override
    {
        return nullptr;
    }
};

#if defined(Q_OS_LINUX)

class RealEvdevHandle : public UlanziEvdevHandle {
public:
    explicit RealEvdevHandle(int fd)
        : m_fd(fd)
        , m_notifier(new QSocketNotifier(fd, QSocketNotifier::Read, this))
    {
        connect(m_notifier, &QSocketNotifier::activated, this, [this] { readEvents(); });
    }

    ~RealEvdevHandle() override
    {
        closeDevice();
    }

    void closeDevice() override
    {
        m_notifier->setEnabled(false);
        if (m_fd >= 0) {
            // From AetherSDR src/core/EvdevEncoderManager.cpp:175-178 [@1e0718ad]
            int grab = 0;
            ioctl(m_fd, EVIOCGRAB, &grab);
            ::close(m_fd);
            m_fd = -1;
        }
    }

private:
    void readEvents()
    {
        if (m_fd < 0) {
            return;
        }
        // From AetherSDR src/core/EvdevEncoderManager.cpp:192-224 [@1e0718ad]
        struct input_event ev[32];
        while (true) {
            const ssize_t n = ::read(m_fd, ev, sizeof(ev));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno != ENODEV) {
                    qCWarning(lcHotKeys) << "Ulanzi Dial read failed:" << std::strerror(errno);
                }
                emit wentAway();
                return;
            }
            if (n == 0) {
                emit wentAway();
                return;
            }
            const size_t count = static_cast<size_t>(n) / sizeof(struct input_event);
            for (size_t i = 0; i < count; ++i) {
                // value: 0 = release, 1 = press, 2 = autorepeat
                // [original inline comment from src/core/EvdevEncoderManager.cpp:216]
                emit inputEvent(ev[i].type, ev[i].code, ev[i].value);
            }
        }
    }

    int m_fd;
    QSocketNotifier* m_notifier;
};

class RealLinuxLayer : public UlanziLinuxLayer {
public:
    QString findNode() override
    {
        return UlanziLinuxInput::findEventNode(QStringLiteral("/sys/class/input"));
    }

    QString watchedDirectory() override
    {
        // From AetherSDR src/core/EvdevEncoderManager.cpp:68 [@1e0718ad]
        return QStringLiteral("/dev/input");
    }

    std::unique_ptr<UlanziEvdevHandle> open(const QString& node, QString* detail) override
    {
        const QByteArray path = QStringLiteral("/dev/input/%1").arg(node).toLocal8Bit();
        // From AetherSDR src/core/EvdevEncoderManager.cpp:140-159 [@1e0718ad]
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            *detail = QString::fromLocal8Bit(std::strerror(errno));
            return nullptr;
        }
        int grab = 1;
        if (ioctl(fd, EVIOCGRAB, &grab) < 0) {
            // Not fatal: events still flow, they just also reach the focused
            // window.  Worth a warning so users know the global-hotkey
            // pollution will be present.
            qCWarning(lcHotKeys) << "Ulanzi Dial grab failed, so its keys also reach the focused window:"
                                 << std::strerror(errno);
        }
        return std::make_unique<RealEvdevHandle>(fd);
    }
};

#endif // Q_OS_LINUX

std::unique_ptr<UlanziLinuxLayer> makeLayer()
{
    if (testLayerFactory()) {
        return testLayerFactory()();
    }
#if defined(Q_OS_LINUX)
    // Only the application turns this on, so no test can open an input node.
    if (!DeviceInput::realDevicesAllowed()) {
        return std::make_unique<NoDialLinuxLayer>();
    }
    return std::make_unique<RealLinuxLayer>();
#else
    return std::make_unique<NoDialLinuxLayer>();
#endif
}

} // namespace

UlanziLinuxInput::UlanziLinuxInput(QObject* parent)
    : UlanziDialInput(parent)
    , m_layer(makeLayer())
    , m_scheduler(testScheduler())
    , m_debounce(new QTimer(this))
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<DeviceEvent>();
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);  // debounce udev burst
    connect(m_debounce, &QTimer::timeout, this, &UlanziLinuxInput::checkNow);
}

UlanziLinuxInput::~UlanziLinuxInput()
{
    retireHandle();
}

void UlanziLinuxInput::setLayerFactoryForTest(LayerFactory factory)
{
    testLayerFactory() = std::move(factory);
}

void UlanziLinuxInput::setSchedulerForTest(Scheduler scheduler)
{
    testScheduler() = std::move(scheduler);
}

QString UlanziLinuxInput::findEventNode(const QString& sysClassInput)
{
    // Read an input device's name from sysfs (e.g. "Ulanzi Dial Keyboard").  The
    // sysfs `name` attribute is world-readable, so this works even when we lack
    // permission to open the /dev/input/event* node itself — which is exactly how
    // we detect a present-but-inaccessible dial.
    // [original inline comment from src/core/EvdevEncoderManager.cpp:36-39]
    const QDir dir(sysClassInput);
    const QStringList nodes = dir.entryList({QStringLiteral("event*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& node : nodes) {
        QFile file(dir.filePath(node + QStringLiteral("/device/name")));
        if (file.open(QIODevice::ReadOnly) && file.readAll().trimmed() == kDialKeyboardName) {
            return node;
        }
    }
    return {};
}

void UlanziLinuxInput::start()
{
    if (m_wanted) {
        return;
    }
    m_wanted = true;
    notify(false, notPluggedInReason());
    const QString watched = m_layer->watchedDirectory();
    if (!watched.isEmpty() && m_watcher == nullptr) {
        m_watcher = new QFileSystemWatcher(this);
        m_watcher->addPath(watched);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { onDirectoryChanged(); });
    }
    checkNow();
}

void UlanziLinuxInput::stop()
{
    m_wanted = false;
    m_debounce->stop();
    if (m_watcher != nullptr) {
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    const bool wasOpen = m_handle != nullptr;
    retireHandle();
    if (wasOpen) {
        keysGone(QString());
    }
    forgetState();
}

void UlanziLinuxInput::onDirectoryChanged()
{
    if (m_wanted && m_handle == nullptr) {
        scheduleCheck();
    }
}

void UlanziLinuxInput::scheduleCheck()
{
    if (m_scheduler) {
        m_scheduler(kDebounceMs);
    } else {
        m_debounce->start();
    }
}

void UlanziLinuxInput::checkNow()
{
    if (!m_wanted || m_handle != nullptr) {
        return;
    }
    const QString node = m_layer->findNode();
    if (node.isEmpty()) {
        notify(false, notPluggedInReason());
        return;
    }
    QString detail;
    std::unique_ptr<UlanziEvdevHandle> handle = m_layer->open(node, &detail);
    if (!handle) {
        // A recognized dial is present but we can't open its node — the udev
        // access rule isn't installed.  Surface it once so the UI can offer to
        // install the rule, rather than silently reading as "disconnected".
        // [original inline comment from src/core/EvdevEncoderManager.cpp:90-92]
        const QString reason = deviceOpenFailureReason(detail);
        if (reason != lastReason()) {
            qCWarning(lcHotKeys) << "Couldn't open the Ulanzi Dial at" << node << detail;
        }
        notify(false, reason);
        return;
    }
    m_handle = handle.release();
    m_handle->setParent(this);
    connect(m_handle, &UlanziEvdevHandle::inputEvent, this, &UlanziLinuxInput::onInputEvent);
    connect(m_handle, &UlanziEvdevHandle::wentAway, this, &UlanziLinuxInput::onWentAway);
    qCInfo(lcHotKeys) << "Opened the Ulanzi Dial at" << node;
    notify(true, QString());
}

void UlanziLinuxInput::onInputEvent(quint16 type, quint16 code, qint32 value)
{
    // From AetherSDR src/core/EvdevEncoderManager.cpp:215 [@1e0718ad]
    if (type == kEvKey) {
        feedKey(code, value);
    }
}

void UlanziLinuxInput::onWentAway()
{
    qCInfo(lcHotKeys) << "The Ulanzi Dial went away";
    retireHandle();
    keysGone(notPluggedInReason());
    if (m_wanted) {
        scheduleCheck();
    }
}

void UlanziLinuxInput::retireHandle()
{
    if (m_handle == nullptr) {
        return;
    }
    // This can run from the handle's own signal, so not deleted here.
    m_handle->disconnect(this);
    m_handle->closeDevice();
    m_handle->deleteLater();
    m_handle = nullptr;
}

} // namespace NereusSDR
