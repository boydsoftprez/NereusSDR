// =================================================================
// src/gui/hotkeys/midi/MidiInput.cpp  (NereusSDR)
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
//   2026-11-06 - MIDI input built on AetherSDR src/core/MidiControlManager.cpp
//                 [@1e0718ad] (RtMidi open sequence, callback hand-off, port
//                 name match, message decoding, learn) for NereusSDR by J.J.
//                 Boyd (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code (Claude Opus 5.5). Hot keys plan Task 10. Its own
//                 thread instead of the main one, a port list comparison so an
//                 unplug is seen, and a release is never eaten by learn.
//   2026-11-06 - One fewer port with the open port's name counts as losing it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude
//                 Opus 5.5).
//   2026-11-06 - A process that hasn't allowed real devices gets no device layer.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude
//                 Opus 5.5).
// =================================================================

#include "gui/hotkeys/midi/MidiInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/DeviceInput.h"

#ifdef NEREUS_HAVE_RTMIDI
#include "RtMidi.h"
#endif

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTimer>

#include <exception>
#include <string>
#include <vector>

namespace NereusSDR {

namespace {

constexpr quint8 kTypeMask = 0xF0;
constexpr quint8 kChannelMask = 0x0F;
constexpr quint8 kDataMask = 0x7F;
// From AetherSDR src/core/MidiControlManager.cpp:313-322 [@1e0718ad]
constexpr quint8 kNoteOff = 0x80;
constexpr quint8 kNoteOn = 0x90;
constexpr quint8 kControlChange = 0xB0;
constexpr quint8 kPitchBend = 0xE0;

// Guards RtMidi's setup and the test factory. getCoreMidiClientSingleton fills a
// plain static, and the Setup page lists ports from the main thread while the worker
// checks them. Never taken by the device layer's callback, so closing can't deadlock
// on a callback waiting for it.
QMutex& deviceMutex()
{
    static QMutex kMutex;
    return kMutex;
}

MidiInput::BackendFactory& testFactory()
{
    static MidiInput::BackendFactory kFactory;
    return kFactory;
}

const QString& notPluggedInReason()
{
    static const QString kReason = QStringLiteral("Not plugged in.");
    return kReason;
}

const QString& noSupportReason()
{
    static const QString kReason = QStringLiteral("This build has no MIDI support.");
    return kReason;
}

#ifdef NEREUS_HAVE_RTMIDI

class RtMidiBackend : public MidiPortBackend {
public:
    ~RtMidiBackend() override
    {
        close();
        const QMutexLocker locker(&deviceMutex());
        m_probe.reset();
    }

    QStringList portNames() override
    {
        QStringList result;
        const QMutexLocker locker(&deviceMutex());
        try {
            if (!m_probe) {
                m_probe = std::make_unique<RtMidiIn>();
            }
            // From AetherSDR src/core/MidiControlManager.cpp:99-101 [@1e0718ad]
            const unsigned int n = m_probe->getPortCount();
            for (unsigned int i = 0; i < n; ++i) {
                result.append(QString::fromStdString(m_probe->getPortName(i)));
            }
        } catch (const RtMidiError& e) {
            qCWarning(lcHotKeys) << "MIDI: error enumerating ports:" << e.what();
            m_probe.reset();
        }
        return result;
    }

    bool open(const QString& exactName, MessageHandler handler, QString* error) override
    {
        close();
        const QMutexLocker locker(&deviceMutex());
        try {
            m_handler = std::move(handler);
            // From AetherSDR src/core/MidiControlManager.cpp:112-121 [@1e0718ad]
            auto midiIn = std::make_unique<RtMidiIn>();
            const unsigned int n = midiIn->getPortCount();
            for (unsigned int i = 0; i < n; ++i) {
                if (QString::fromStdString(midiIn->getPortName(i)) == exactName) {
                    midiIn->openPort(i);
                    midiIn->setCallback(&RtMidiBackend::onMessage, this);
                    midiIn->ignoreTypes(true, true, true); // ignore sysex, timing, active sensing
                    m_in = std::move(midiIn);
                    return true;
                }
            }
            *error = QStringLiteral("no such port");
        } catch (const RtMidiError& e) {
            *error = QString::fromStdString(e.getMessage());
        }
        return false;
    }

    void close() override
    {
        const QMutexLocker locker(&deviceMutex());
        if (!m_in) {
            return;
        }
        try {
            m_in->closePort();
        } catch (const std::exception& e) {
            qCWarning(lcHotKeys) << "MIDI: error closing a port:" << e.what();
        }
        m_in.reset();
    }

private:
    // Runs on RtMidi's own thread: copy the bytes and hand them on.
    // From AetherSDR src/core/MidiControlManager.cpp:271-279 [@1e0718ad]
    static void onMessage(double /*deltatime*/, std::vector<unsigned char>* message, void* userData)
    {
        if (message == nullptr || message->size() < 2) {
            return;
        }
        const auto* const self = static_cast<const RtMidiBackend*>(userData);
        self->m_handler(MidiMessage{(*message)[0], (*message)[1],
                                    message->size() > 2 ? (*message)[2] : static_cast<unsigned char>(0)});
    }

    std::unique_ptr<RtMidiIn> m_probe;
    std::unique_ptr<RtMidiIn> m_in;
    MessageHandler m_handler;
};

#endif // NEREUS_HAVE_RTMIDI

std::unique_ptr<MidiPortBackend> makeBackend()
{
    MidiInput::BackendFactory factory;
    {
        const QMutexLocker locker(&deviceMutex());
        factory = testFactory();
    }
    if (factory) {
        return factory();
    }
    // Only the application allows real devices (src/main.cpp). As DeviceInput, FlexControl
    // and the Ulanzi Dial.
    if (!DeviceInput::realDevicesAllowed()) {
        return nullptr;
    }
#ifdef NEREUS_HAVE_RTMIDI
    return std::make_unique<RtMidiBackend>();
#else
    return nullptr;
#endif
}

// RtMidi's ALSA names end in " client:port" (third_party/rtmidi/RtMidi.cpp:2126-2129),
// and the client number is handed out when the device appears.
QString withoutAlsaAddress(const QString& name)
{
    static const QRegularExpression kAddress(QStringLiteral(" \\d+:\\d+$"));
    QString stable = name;
    stable.remove(kAddress);
    return stable;
}

} // namespace

std::optional<MidiControlMessage> decodeMidiMessage(const MidiMessage& message)
{
    const int channel = (message.status & kChannelMask) + 1;
    const int data1 = message.data1 & kDataMask;
    const int data2 = message.data2 & kDataMask;

    // From AetherSDR src/core/MidiControlManager.cpp:313-325 [@1e0718ad]
    switch (message.status & kTypeMask) {
    case kControlChange:    // Control Change
        return MidiControlMessage{QStringLiteral("ch%1.cc%2").arg(channel).arg(data1),
                                  MidiControlKind::AbsoluteKnob, data2, true};
    case kNoteOn:           // Note On
        return MidiControlMessage{QStringLiteral("ch%1.note%2").arg(channel).arg(data1),
                                  MidiControlKind::Button, data2, false};
    case kNoteOff:          // Note Off
        return MidiControlMessage{QStringLiteral("ch%1.note%2").arg(channel).arg(data1),
                                  MidiControlKind::Button, 0, false};
    case kPitchBend:        // Pitch Bend
        return MidiControlMessage{QStringLiteral("ch%1.bend").arg(channel),
                                  MidiControlKind::AbsoluteKnob, data1 | (data2 << 7), false};
    default:
        return std::nullopt; // ignore other message types
    }
}

int midiControlRange(const QString& controlKey)
{
    return controlKey.endsWith(QLatin1String(".bend")) ? kMidiBendMax : kMidiValueMax;
}

bool midiPortNameMatches(const QString& portName, const QString& wanted)
{
    // From AetherSDR src/core/MidiControlManager.cpp:140 [@1e0718ad]
    return !wanted.isEmpty()
        && withoutAlsaAddress(portName).contains(withoutAlsaAddress(wanted), Qt::CaseInsensitive);
}

MidiInput::MidiInput(QObject* parent)
    : QObject(parent)
    , m_backend(makeBackend())
    , m_portTimer(new QTimer(this))
{
    qRegisterMetaType<MidiControlKind>();
    m_portTimer->setInterval(kPortCheckMs);
    connect(m_portTimer, &QTimer::timeout, this, &MidiInput::checkPorts);
}

MidiInput::~MidiInput()
{
    closeDevice();
}

QStringList MidiInput::portNames()
{
    const std::unique_ptr<MidiPortBackend> backend = makeBackend();
    return backend ? backend->portNames() : QStringList{};
}

bool MidiInput::supported()
{
#ifdef NEREUS_HAVE_RTMIDI
    return true;
#else
    const QMutexLocker locker(&deviceMutex());
    return static_cast<bool>(testFactory());
#endif
}

void MidiInput::setBackendFactoryForTest(BackendFactory factory)
{
    const QMutexLocker locker(&deviceMutex());
    testFactory() = std::move(factory);
}

void MidiInput::openPort(const QString& name)
{
    closeDevice();
    if (!m_backend) {
        notify(false, noSupportReason());
        return;
    }
    notify(false, QString());
    m_wanted = name;
    tryOpen(m_backend->portNames());
    m_portTimer->start();
}

void MidiInput::closePort()
{
    m_portTimer->stop();
    m_wanted.clear();
    m_learning = false;
    closeDevice();
    notify(false, QString());
}

void MidiInput::beginLearn()
{
    m_learning = true;
}

void MidiInput::cancelLearn()
{
    m_learning = false;
}

void MidiInput::setRelativeControls(const QStringList& controlKeys)
{
    const QSet<QString> relative(controlKeys.cbegin(), controlKeys.cend());
    // A control whose kind was corrected starts fresh: a value from before would
    // read as one big turn.
    const QSet<QString> changed = (relative - m_relative) + (m_relative - relative);
    for (const QString& key : changed) {
        m_lastValue.remove(key);
        m_encoding.remove(key);
    }
    m_relative = relative;
}

void MidiInput::checkPorts()
{
    if (m_wanted.isEmpty() || !m_backend) {
        return;
    }
    const QStringList names = m_backend->portNames();
    if (m_openName.isEmpty()) {
        tryOpen(names);
        return;
    }
    // CoreMIDI and Windows list two same-named controllers under one name (only ALSA
    // adds an address; CMakeLists.txt sets RTMIDI_DO_NOT_ENSURE_UNIQUE_PORTNAMES), and the list
    // doesn't say which left. If it might be the open one its hold has to drop, so one
    // fewer closes the port and the next check opens what is left.
    const qsizetype count = names.count(m_openName);
    if (count < m_openNameCount) {
        closeDevice();
        notify(false, notPluggedInReason());
    } else {
        m_openNameCount = count;
    }
}

void MidiInput::tryOpen(const QStringList& names)
{
    // From AetherSDR src/core/MidiControlManager.cpp:139-142 [@1e0718ad]
    for (const QString& candidate : names) {
        if (!midiPortNameMatches(candidate, m_wanted)) {
            continue;
        }
        const quint64 generation = ++m_generation;
        QString error;
        const bool opened = m_backend->open(
            candidate,
            [this, generation](const MidiMessage& message) {
                // Bridge to Qt main thread  [original inline comment from MidiControlManager.cpp:293]
                // Here, to this object's thread, the MIDI worker.
                QMetaObject::invokeMethod(
                    this,
                    [this, generation, message] {
                        // Queued before a close: nothing would ever release its press.
                        if (generation == m_generation) {
                            handleMessage(message);
                        }
                    },
                    Qt::QueuedConnection);
            },
            &error);
        if (!opened) {
            ++m_generation;
            qCWarning(lcHotKeys) << "MIDI: open failed:" << candidate << error;
            notify(false, QStringLiteral("%1 could not be opened. Close any other app using it, then try again.")
                              .arg(candidate));
            return;
        }
        m_openName = candidate;
        m_openNameCount = names.count(candidate);
        m_wanted = withoutAlsaAddress(candidate);   // what comes back is this device, not a near name
        emit portOpened(candidate);
        notify(true, QString());
        return;
    }
    notify(false, notPluggedInReason());
}

void MidiInput::closeDevice()
{
    ++m_generation;
    m_openName.clear();
    m_openNameCount = 0;
    if (m_backend) {
        m_backend->close();
    }
    m_encoding.clear();
    m_lastValue.clear();
}

void MidiInput::notify(bool connected, const QString& reason)
{
    if (connected == m_connected && reason == m_reason) {
        return;
    }
    m_connected = connected;
    m_reason = reason;
    emit connectionChanged(connected, reason);
}

void MidiInput::handleMessage(const MidiMessage& message)
{
    const std::optional<MidiControlMessage> control = decodeMidiMessage(message);
    if (!control) {
        return;
    }
    const QString& key = control->controlKey;
    const int value = control->value;

    if (control->kind == MidiControlKind::Button) {
        // A note's release is no use of a control, and learn binds the next use.
        // if (msgType == MidiBinding::NoteOff) return; // ignore NoteOff during learn
        //   [original inline comment from MidiControlManager.cpp:347]
        // Upstream drops it here. Ours goes on, or a button held across beginLearn
        // would never let go. A Note On of velocity 0 is a release too.
        if (m_learning && value > 0) {
            m_learning = false;
            emit learned(key, MidiControlKind::Button);
            return;
        }
        emit controlEvent(key, MidiControlKind::Button, value, 0);
        return;
    }

    if (m_learning) {
        m_learning = false;
        // 1 and 127 are a two's-complement knob's detents (decodeMidiRelativeCc).
        const bool relative = control->isControlChange && (value == 1 || value == kMidiValueMax);
        m_lastValue.insert(key, value);
        emit learned(key, relative ? MidiControlKind::RelativeKnob : MidiControlKind::AbsoluteKnob);
        return;
    }

    if (control->isControlChange && m_relative.contains(key)) {
        const MidiRelativeCcDecodeResult decoded = decodeMidiRelativeCc(value, m_encoding.value(key));
        m_encoding.insert(key, decoded.encoding);
        emit controlEvent(key, MidiControlKind::RelativeKnob, value, decoded.delta);
        return;
    }

    const auto last = m_lastValue.constFind(key);
    const int delta = last == m_lastValue.constEnd() ? 0 : value - last.value();
    m_lastValue.insert(key, value);
    emit controlEvent(key, MidiControlKind::AbsoluteKnob, value, delta);
}

} // namespace NereusSDR
