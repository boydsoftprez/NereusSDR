// =================================================================
// src/gui/hotkeys/devices/UlanziWindowsInput.cpp  (NereusSDR)
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
//   2026-11-06 - Ulanzi Dial on Windows built on AetherSDR
//                 src/core/UlanziDialWindowsManager.h and .cpp [@1e0718ad] for
//                 NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. The device sits behind a seam so a test never opens one, and
//                 nothing is enumerated until the application allows it. Report
//                 decoding is a static function a Mac test can run. A refused open
//                 is reported on the Knobs page.
// =================================================================

#include "gui/hotkeys/devices/UlanziWindowsInput.h"

#include "core/LogCategories.h"

#include <QTimer>
#include <QtGlobal>

#include <optional>
#include <utility>

#if defined(Q_OS_WIN) && defined(NEREUS_HAVE_HIDAPI)
#include "gui/hotkeys/devices/UsbPresence.h"
#include "hidapi/hidapi.h"

#include <QMutexLocker>

#include <cwchar>
#endif

namespace NereusSDR {

namespace {

constexpr quint8 kConsumerPage = 0x0C;
constexpr quint8 kKeyboardPage = 0x07;

UlanziWindowsInput::LayerFactory& testLayerFactory()
{
    static UlanziWindowsInput::LayerFactory kFactory;
    return kFactory;
}

// Nothing on it, for a process that hasn't allowed real devices. It never calls hidapi.
class NoDialWindowsLayer : public UlanziWindowsLayer {
public:
    QStringList findPaths() override
    {
        return {};
    }

    std::unique_ptr<HidDeviceHandle> open(const QString& /*path*/, QString* /*error*/) override
    {
        return nullptr;
    }
};

#if defined(Q_OS_WIN) && defined(NEREUS_HAVE_HIDAPI)

// From AetherSDR src/core/UlanziDialWindowsManager.cpp:46 [@1e0718ad]
constexpr const wchar_t* kProductMatch = L"Ulanzi Dial";

class RealWindowsHandle : public HidDeviceHandle {
public:
    explicit RealWindowsHandle(hid_device* device)
        : m_device(device)
    {
    }

    ~RealWindowsHandle() override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        hid_close(m_device);
    }

    int read(char* buffer, int size) override
    {
        return hid_read(m_device, reinterpret_cast<unsigned char*>(buffer), static_cast<size_t>(size));
    }

    int write(const QByteArray& /*report*/) override
    {
        return -1;  // the dial has nothing to show
    }

private:
    hid_device* m_device;
};

// hid_exit stays with DeviceInput's layer, which outlives this one's handles.
class RealWindowsLayer : public UlanziWindowsLayer {
public:
    QStringList findPaths() override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        // From AetherSDR src/core/UlanziDialWindowsManager.cpp:89-92 [@1e0718ad]
        QStringList paths;
        hid_device_info* const infos = hid_enumerate(0, 0);
        for (const hid_device_info* info = infos; info != nullptr; info = info->next) {
            if (info->product_string != nullptr && std::wcsstr(info->product_string, kProductMatch) != nullptr) {
                paths.append(QString::fromUtf8(info->path));
            }
        }
        hid_free_enumeration(infos);
        return paths;
    }

    std::unique_ptr<HidDeviceHandle> open(const QString& path, QString* error) override
    {
        const QMutexLocker locker(&UsbPresence::hidApiLock());
        // From AetherSDR src/core/UlanziDialWindowsManager.cpp:93-95 [@1e0718ad]
        hid_device* const device = hid_open_path(path.toUtf8().constData());
        if (device == nullptr) {
            *error = QString::fromWCharArray(hid_error(nullptr));
            return nullptr;
        }
        hid_set_nonblocking(device, 1);
        return std::make_unique<RealWindowsHandle>(device);
    }
};

#endif // Q_OS_WIN && NEREUS_HAVE_HIDAPI

std::unique_ptr<UlanziWindowsLayer> makeLayer()
{
    if (testLayerFactory()) {
        return testLayerFactory()();
    }
#if defined(Q_OS_WIN) && defined(NEREUS_HAVE_HIDAPI)
    // Only the application turns this on, so no test can enumerate or open a device.
    if (!DeviceInput::realDevicesAllowed()) {
        return std::make_unique<NoDialWindowsLayer>();
    }
    return std::make_unique<RealWindowsLayer>();
#else
    return std::make_unique<NoDialWindowsLayer>();
#endif
}

} // namespace

UlanziWindowsInput::UlanziWindowsInput(QObject* parent)
    : UlanziDialInput(parent)
    , m_layer(makeLayer())
    , m_pollTimer(new QTimer(this))
    , m_hotplugTimer(new QTimer(this))
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<DeviceEvent>();
    m_pollTimer->setInterval(kPollMs);
    connect(m_pollTimer, &QTimer::timeout, this, &UlanziWindowsInput::poll);
    m_hotplugTimer->setInterval(kHotplugMs);
    connect(m_hotplugTimer, &QTimer::timeout, this, &UlanziWindowsInput::hotplugCheck);
}

UlanziWindowsInput::~UlanziWindowsInput() = default;

void UlanziWindowsInput::setLayerFactoryForTest(LayerFactory factory)
{
    testLayerFactory() = std::move(factory);
}

void UlanziWindowsInput::start()
{
    if (m_wanted) {
        return;
    }
    m_wanted = true;
    notify(false, notPluggedInReason());
    hotplugCheck();
    m_hotplugTimer->start();
}

void UlanziWindowsInput::stop()
{
    m_wanted = false;
    m_pollTimer->stop();
    m_hotplugTimer->stop();
    const bool wasOpen = !m_open.empty();
    m_open.clear();
    if (wasOpen) {
        keysGone(QString());
    }
    forgetState();
}

void UlanziWindowsInput::hotplugCheck()
{
    if (!m_wanted || !m_open.empty()) {
        return;
    }
    const QStringList paths = m_layer->findPaths();
    if (paths.isEmpty()) {
        notify(false, notPluggedInReason());
        return;
    }
    // Every interface: Windows keeps a keyboard collection for itself, so that one
    // can fail while the rest open.
    QString firstError;
    for (const QString& path : paths) {
        QString error;
        std::unique_ptr<HidDeviceHandle> handle = m_layer->open(path, &error);
        if (handle) {
            m_open.push_back(OpenInterface{std::move(handle), QByteArray()});
        } else if (firstError.isEmpty()) {
            firstError = error;
        }
    }
    if (m_open.empty()) {
        const QString reason = deviceOpenFailureReason(firstError);
        if (reason != lastReason()) {
            qCWarning(lcHotKeys) << "Couldn't open the Ulanzi Dial:" << firstError;
        }
        notify(false, reason);
        return;
    }
    qCInfo(lcHotKeys) << "Opened the Ulanzi Dial," << m_open.size() << "of" << paths.size() << "interfaces";
    m_pollTimer->start();
    notify(true, QString());
}

void UlanziWindowsInput::poll()
{
    // From AetherSDR src/core/UlanziDialWindowsManager.cpp:137-158 [@1e0718ad]
    char buf[64];
    for (auto it = m_open.begin(); it != m_open.end();) {
        const int n = it->handle->read(buf, static_cast<int>(sizeof(buf)));
        if (n < 0) {
            it = m_open.erase(it);
            continue;
        }
        if (n > 0) {
            const QByteArray report(buf, n);
            const QList<QPair<int, int>> keys = decodeReport(it->lastReport, report);
            it->lastReport = report;
            for (const QPair<int, int>& key : keys) {
                feedKey(key.first, key.second);
            }
        }
        ++it;
    }
    if (m_open.empty()) {
        qCInfo(lcHotKeys) << "The Ulanzi Dial went away";
        m_pollTimer->stop();
        keysGone(notPluggedInReason());
    }
}

QList<QPair<int, int>> UlanziWindowsInput::decodeReport(const QByteArray& previous, const QByteArray& report)
{
    QList<QPair<int, int>> keys;
    const qsizetype len = report.size();
    const auto byteAt = [](const QByteArray& bytes, qsizetype at) { return static_cast<unsigned char>(bytes[at]); };
    const auto add = [&keys](std::optional<int> key, int value) {
        if (key.has_value()) {
            keys.append({*key, value});
        }
    };

    // Heuristic: discriminate between Consumer (Page 0x0C) and Keyboard
    // (Page 0x07) reports by length + content.  Real HID stacks tag with
    // a report id in byte 0; without parsing the descriptor we use the
    // typical shapes Ulanzi Dial firmware emits.
    //
    // Consumer page report — 2-byte usage code in little-endian:
    //   [report_id?] [usage_lo] [usage_hi]   (3 bytes total typical)
    // Keyboard report (8 bytes):
    //   [modifier_mask] [reserved] [key1] [key2] ... [key6]
    if (len == 8) {
        // Standard keyboard report.
        const unsigned char mod = byteAt(report, 0);
        const unsigned char prevMod = previous.size() == 8 ? byteAt(previous, 0) : 0;
        // Ctrl transitions.
        const bool nowCtrl = (mod & 0x01) != 0 || (mod & 0x10) != 0;
        const bool prevCtrl = (prevMod & 0x01) != 0 || (prevMod & 0x10) != 0;
        if (nowCtrl != prevCtrl) {
            keys.append({UlanziKey::LeftCtrl, nowCtrl ? 1 : 0});
        }
        // Pressed-keys diff: keys 2..7.
        const auto contains = [&byteAt](const QByteArray& r, unsigned char k) -> bool {
            for (qsizetype i = 2; i < r.size() && i < 8; ++i) {
                if (byteAt(r, i) == k && k != 0) {
                    return true;
                }
            }
            return false;
        };
        for (qsizetype i = 2; i < len && i < 8; ++i) {
            const unsigned char k = byteAt(report, i);
            if (k != 0 && !contains(previous, k)) {
                add(UlanziChordDecoder::keyCodeFromHidUsage(kKeyboardPage, k), 1);
            }
        }
        for (qsizetype i = 2; i < previous.size() && i < 8; ++i) {
            const unsigned char k = byteAt(previous, i);
            if (k != 0 && !contains(report, k)) {
                add(UlanziChordDecoder::keyCodeFromHidUsage(kKeyboardPage, k), 0);
            }
        }
    } else if (len >= 2) {
        // Consumer-page-ish report; treat first usage as the active key.
        const quint16 usage = static_cast<quint16>(byteAt(report, 0) | (byteAt(report, 1) << 8));
        // Synthesise press + release (consumer-control reports usually
        // appear once per dial detent; nonzero means pressed).
        const std::optional<int> key = UlanziChordDecoder::keyCodeFromHidUsage(kConsumerPage, usage);
        add(key, 1);
        add(key, 0);
    }
    return keys;
}

} // namespace NereusSDR
