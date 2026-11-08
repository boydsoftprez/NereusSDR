#pragma once

// =================================================================
// src/gui/hotkeys/midi/MidiInput.h  (NereusSDR)
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
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/midi/MidiProfile.h"
#include "gui/hotkeys/midi/MidiRelativeDecoder.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>

class QTimer;

namespace NereusSDR {

struct MidiMessage {
    quint8 status;
    quint8 data1;
    quint8 data2;
};

// From AetherSDR src/core/MidiControlManager.cpp:325 [@1e0718ad]: pitch bend is
// (d1|d2<<7), over 16383. A control change and a note velocity top out at 127.
inline constexpr int kMidiValueMax = 127;
inline constexpr int kMidiBendMax = 16383;

// One message as the control it came from. value: a CC's 0 to 127, a pitch bend's
// 0 to 16383, a note's velocity, and 0 for a release however it was sent.
struct MidiControlMessage {
    QString controlKey;
    MidiControlKind kind;
    int value;
    bool isControlChange;
};

// Empty for the messages hot keys have no use for.
NEREUS_GUI_EXPORT std::optional<MidiControlMessage> decodeMidiMessage(const MidiMessage& message);
// The most value this control can send: kMidiBendMax or kMidiValueMax.
NEREUS_GUI_EXPORT int midiControlRange(const QString& controlKey);
// A case-insensitive part of the name, leaving out the client:port numbers ALSA
// adds, which can change when the device comes back.
NEREUS_GUI_EXPORT bool midiPortNameMatches(const QString& portName, const QString& wanted);

// What MidiInput needs from the device layer. The real one wraps RtMidiIn; a test
// supplies its own. Everything but the handler runs on the MIDI worker. The handler
// runs on whatever thread the device layer delivers on.
class MidiPortBackend {
public:
    using MessageHandler = std::function<void(const MidiMessage&)>;

    virtual ~MidiPortBackend() = default;
    virtual QStringList portNames() = 0;
    // exactName is one portNames() returned. On failure, error gets the detail for the log.
    virtual bool open(const QString& exactName, MessageHandler handler, QString* error) = 0;
    virtual void close() = 0;
};

class NEREUS_GUI_EXPORT MidiInput : public QObject {
    Q_OBJECT

public:
    using BackendFactory = std::function<std::unique_ptr<MidiPortBackend>()>;

    // Upstream's isPortOpen only reflects open and close calls, so comparing the
    // port list is how an unplug is seen.
    static constexpr int kPortCheckMs = 2000;

    explicit MidiInput(QObject* parent = nullptr);
    ~MidiInput() override;

    // Stays wanted until closePort: if the name isn't there it opens when it comes.
    Q_INVOKABLE void openPort(const QString& name);
    Q_INVOKABLE void closePort();
    Q_INVOKABLE void beginLearn();
    Q_INVOKABLE void cancelLearn();
    // The CCs to decode as relative. The profile knows which; this thread doesn't.
    Q_INVOKABLE void setRelativeControls(const QStringList& controlKeys);

    static QStringList portNames();
    static bool supported();    // false in a build without RtMidi

    // A null factory goes back to the real device layer.
    static void setBackendFactoryForTest(BackendFactory factory);

public slots:
    void checkPorts();                                // the timer's slot; public for tests
    void handleMessage(const NereusSDR::MidiMessage& message);    // where a queued message lands; public for tests

signals:
    // value as in MidiControlMessage. delta: a relative knob's decoded steps, or an
    // absolute control's change since its last message, 0 for its first.
    void controlEvent(QString controlKey, NereusSDR::MidiControlKind kind, int value, int delta);
    void learned(QString controlKey, NereusSDR::MidiControlKind kind);
    // Before connectionChanged(true, ...). The full name of the device that opened.
    void portOpened(QString portName);
    // reason is shown to the user. Empty when connected, and after closePort.
    void connectionChanged(bool connected, QString reason);

private:
    void tryOpen(const QStringList& names);
    void closeDevice();
    void notify(bool connected, const QString& reason);

    std::unique_ptr<MidiPortBackend> m_backend;
    QTimer* m_portTimer;

    QString m_wanted;            // empty when nothing is wanted
    QString m_openName;          // empty when nothing is open
    qsizetype m_openNameCount{0};   // ports with that name, since two controllers can share one
    quint64 m_generation{0};     // a message queued before a close has an old one
    bool m_learning{false};
    bool m_connected{false};
    QString m_reason;

    QSet<QString> m_relative;
    QHash<QString, MidiRelativeCcEncoding> m_encoding;
    QHash<QString, int> m_lastValue;
};

} // namespace NereusSDR
