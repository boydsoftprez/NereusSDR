// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// A MIDI device layer with no device. The test holds the shared state, says which
// port names exist, and sends messages as RtMidi's own thread would.
#pragma once

#include "gui/hotkeys/midi/MidiInput.h"

#include <QMutex>
#include <QMutexLocker>

#include <memory>

namespace NereusSDR {

struct FakeMidiPorts {
    mutable QMutex mutex;
    QStringList names;
    QStringList refuse;     // names that exist but won't open
    QString openName;
    MidiPortBackend::MessageHandler handler;
    int opens{0};
    int closes{0};
    int backendsAlive{0};

    void setNames(const QStringList& list)
    {
        const QMutexLocker locker(&mutex);
        names = list;
    }

    QString opened() const
    {
        const QMutexLocker locker(&mutex);
        return openName;
    }

    int alive() const
    {
        const QMutexLocker locker(&mutex);
        return backendsAlive;
    }

    void send(quint8 status, quint8 data1, quint8 data2)
    {
        MidiPortBackend::MessageHandler toCall;
        {
            const QMutexLocker locker(&mutex);
            toCall = handler;
        }
        if (toCall) {
            toCall(MidiMessage{status, data1, data2});
        }
    }
};

class FakeMidiBackend : public MidiPortBackend {
public:
    explicit FakeMidiBackend(std::shared_ptr<FakeMidiPorts> ports)
        : m_ports(std::move(ports))
    {
        const QMutexLocker locker(&m_ports->mutex);
        ++m_ports->backendsAlive;
    }

    ~FakeMidiBackend() override
    {
        close();
        const QMutexLocker locker(&m_ports->mutex);
        --m_ports->backendsAlive;
    }

    QStringList portNames() override
    {
        const QMutexLocker locker(&m_ports->mutex);
        return m_ports->names;
    }

    bool open(const QString& exactName, MessageHandler handler, QString* error) override
    {
        close();
        const QMutexLocker locker(&m_ports->mutex);
        if (m_ports->refuse.contains(exactName)) {
            *error = QStringLiteral("refused by the test");
            return false;
        }
        m_ports->openName = exactName;
        m_ports->handler = std::move(handler);
        ++m_ports->opens;
        m_open = true;
        return true;
    }

    void close() override
    {
        const QMutexLocker locker(&m_ports->mutex);
        if (m_open) {
            m_open = false;
            m_ports->openName.clear();
            m_ports->handler = nullptr;
            ++m_ports->closes;
        }
    }

private:
    std::shared_ptr<FakeMidiPorts> m_ports;
    bool m_open{false};
};

// Sets the factory for one test's life.
class FakeMidiDeviceLayer {
public:
    FakeMidiDeviceLayer()
        : ports(std::make_shared<FakeMidiPorts>())
    {
        MidiInput::setBackendFactoryForTest([p = ports] { return std::make_unique<FakeMidiBackend>(p); });
    }
    ~FakeMidiDeviceLayer() { MidiInput::setBackendFactoryForTest(nullptr); }
    FakeMidiDeviceLayer(const FakeMidiDeviceLayer&) = delete;
    FakeMidiDeviceLayer& operator=(const FakeMidiDeviceLayer&) = delete;

    std::shared_ptr<FakeMidiPorts> ports;
};

} // namespace NereusSDR
