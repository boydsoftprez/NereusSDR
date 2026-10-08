// no-port-check: NereusSDR-original injected serial device for byte/pin/lifecycle tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "core/cat/CatSerialTransport.h"
#include <functional>
using namespace NereusSDR;
class CatSerialTestDevice final : public CatSerialDevice {
public:
    bool opened{false}; bool cts{true}; bool dsr{true}; bool pinsAvailable{true};
    bool holdOutput{false}; qint64 bufferedOutput{0};
    qint64 pendingBytes() const override { return bufferedOutput; }
    bool refuseOpen{false}; qint64 maximumWrite{4096}; QByteArray output; QByteArray input;
    int opens{0}; int closes{0}; int pinReads{0};
    std::function<void()> onClose;
    std::function<void()> onWrite;
    CatEndpointConfig accepted;
    bool open(const CatEndpointConfig& config, QString& error) override {
        accepted = config; ++opens;
        if (refuseOpen) { error = "injected open failure"; return false; }
        opened = true; return true;
    }
    void close() override { opened = false; bufferedOutput = 0; ++closes; if (onClose) { onClose(); } }
    bool isOpen() const override { return opened; }
    qint64 write(const QByteArray& bytes) override {
        if (onWrite) { onWrite(); }
        if (!opened || maximumWrite < 0) { return -1; }
        const qint64 count = qMin(qint64(bytes.size()), maximumWrite); output += bytes.left(count); if (holdOutput) { bufferedOutput += count; } return count;
    }
    QByteArray read() override { return std::exchange(input, {}); }
    bool pins(bool& sampledCts, bool& sampledDsr, QString& error) override {
        ++pinReads;
        if (!pinsAvailable) { error = "injected pin sampling unavailable"; return false; }
        sampledCts = cts; sampledDsr = dsr; return true;
    }
    void receive(const QByteArray& bytes) { input += bytes; emit readyRead(); }
    void completeWrite() { bufferedOutput = 0; emit bytesWritten(1); }
    void disappear() { emit errorOccurred("injected device disappearance"); }
};
