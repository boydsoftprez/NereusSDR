// =================================================================
// tests/fakes/FakeAudioBus.h  (NereusSDR)
// =================================================================
//
// Minimal IAudioBus stub for unit tests. Captures every push() into a
// growable byte buffer so callers can assert what was routed where.
// Counts pushes, and lets the test toggle isOpen() to exercise the
// "bus exists but isn't open" branch in AudioEngine::rxBlockReady.
//
// NereusSDR-original test fake — no ported logic, no Thetis attribution
// needed.
// =================================================================

#pragma once

#include "core/IAudioBus.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace NereusSDR {

// Single-threaded test fake — not safe for concurrent push from multiple threads.
class FakeAudioBus : public IAudioBus {
public:
    explicit FakeAudioBus(QString name = QStringLiteral("Fake"))
        : m_name(std::move(name)) {}

    bool open(const AudioFormat& format) override {
        if (!m_openResult) {
            return false;
        }
        m_negotiatedFormat = format;
        m_open = true;
        return true;
    }

    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }

    qint64 push(const char* data, qint64 bytes) override {
        if (!m_open || data == nullptr || bytes <= 0) {
            return 0;
        }
        ++m_pushes;
        m_lastPushBytes = bytes;
        m_buffer.append(data, static_cast<int>(bytes));
        return bytes;
    }

    qint64 pull(char*, qint64) override { return 0; }

    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }

    QString backendName() const override { return m_name; }
    AudioFormat negotiatedFormat() const override { return m_negotiatedFormat; }

    // Test inspectors
    int pushCount() const { return m_pushes; }
    qint64 lastPushBytes() const { return m_lastPushBytes; }
    const QByteArray& buffer() const { return m_buffer; }

    // Force-toggle isOpen() to false so AudioEngine::rxBlockReady's
    // isOpen() guard skips the push.
    void setForceOpen(bool open) { m_open = open; }

    // Drive the failure-path branch in AudioEngine::start() by making the
    // next open() call return false (and leave the bus closed).
    void setOpenResult(bool ok) { m_openResult = ok; }

private:
    QString     m_name;
    AudioFormat m_negotiatedFormat;
    bool        m_open{false};
    bool        m_openResult{true};
    QByteArray  m_buffer;
    int         m_pushes{0};
    qint64      m_lastPushBytes{0};
};

} // namespace NereusSDR
