// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original. Kernel facts carry their cites here.
#include "gui/hotkeys/devices/PowerMateLinuxInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/DeviceInput.h"

#include <QDir>
#include <QFile>
#include <QSocketNotifier>

#if defined(Q_OS_LINUX)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace {

// Why hidraw isn't there: both ids are on hid_ignore_list. Linux
// drivers/hid/hid-quirks.c:824, 864-865 [@f66469d9]. The driver that takes them is
// drivers/input/misc/powermate.c:38-40, 431-432 [@f66469d9].

// Linux include/uapi/linux/input-event-codes.h:40-41, 345, 851 [@f66469d9]. Local,
// since a Mac has no such header and its tests run decode().
constexpr quint16 kEvKey = 0x01;
constexpr quint16 kEvRel = 0x02;
constexpr quint16 kBtn0 = 0x100;
constexpr quint16 kRelDial = 0x07;

// Linux drivers/input/misc/powermate.c:80-81 [@f66469d9]
const char* const kPowerMateName = "Griffin PowerMate";
const char* const kSoundKnobName = "Griffin SoundKnob";

} // namespace

PowerMateLinuxInput::PowerMateLinuxInput(QObject* parent)
    : QObject(parent)
{
}

PowerMateLinuxInput::~PowerMateLinuxInput()
{
    close();
}

QList<DeviceEvent> PowerMateLinuxInput::decode(quint16 type, quint16 code, qint32 value)
{
    // Linux drivers/input/misc/powermate.c:110-111 [@f66469d9]: BTN_0 is the press,
    // REL_DIAL the turn. A key's value is 1 down and 0 up; anything else isn't a
    // change of state.
    QList<DeviceEvent> events;
    if (type == kEvKey && code == kBtn0 && (value == 0 || value == 1)) {
        DeviceEvent event;
        event.kind = value == 1 ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
        event.control = QStringLiteral("press");
        events.append(event);
    } else if (type == kEvRel && code == kRelDial && value != 0) {
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = QStringLiteral("wheel");
        event.value = value;
        events.append(event);
    }
    return events;
}

QString PowerMateLinuxInput::findEventNode(const QString& sysClassInput)
{
    // Linux drivers/input/evdev.c:1371, 1380 [@f66469d9]: eventN is a child of the
    // input device, and drivers/input/input.c:1345, 1516 [@f66469d9] gives that device a
    // name file anyone can read. Reading it needs no access to the device itself, and
    // no other input device gets opened on the way, a keyboard above all.
    const QDir dir(sysClassInput);
    const QStringList nodes = dir.entryList({QStringLiteral("event*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& node : nodes) {
        QFile file(dir.filePath(node + QStringLiteral("/device/name")));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray name = file.readAll().trimmed();
        if (name == kPowerMateName || name == kSoundKnobName) {
            return node;
        }
    }
    return {};
}

bool PowerMateLinuxInput::isOpen() const
{
    return m_fd >= 0;
}

void PowerMateLinuxInput::check()
{
#if defined(Q_OS_LINUX)
    if (isOpen()) {
        return;
    }
    const QString node = findEventNode(QStringLiteral("/sys/class/input"));
    if (node.isEmpty()) {
        m_reason.clear();
        return;
    }
    const QByteArray path = QStringLiteral("/dev/input/%1").arg(node).toLocal8Bit();
    m_fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        const QString detail = QString::fromLocal8Bit(std::strerror(errno));
        const QString reason = deviceOpenFailureReason(detail);
        // It stays there, so this runs again at the next check.
        if (reason != m_reason) {
            qCWarning(lcHotKeys) << "Couldn't open" << path << detail;
        }
        m_reason = reason;
        return;
    }
    m_reason.clear();
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PowerMateLinuxInput::readEvents);
    qCInfo(lcHotKeys) << "Opened" << path;
#endif
}

void PowerMateLinuxInput::close()
{
    if (m_notifier != nullptr) {
        // This can run from the notifier's own signal, so not deleted here.
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
#if defined(Q_OS_LINUX)
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

void PowerMateLinuxInput::readEvents()
{
#if defined(Q_OS_LINUX)
    while (isOpen()) {
        input_event raw{};
        const ssize_t got = ::read(m_fd, &raw, sizeof(raw));
        if (got == static_cast<ssize_t>(sizeof(raw))) {
            const QList<DeviceEvent> events = decode(raw.type, raw.code, raw.value);
            for (const DeviceEvent& event : events) {
                emit deviceEvent(event);
            }
            continue;
        }
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        if (got < 0 && errno == EINTR) {
            continue;
        }
        close();
        emit wentAway();
        return;
    }
#endif
}

} // namespace NereusSDR
