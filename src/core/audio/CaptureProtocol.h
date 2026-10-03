// =================================================================
// src/core/audio/CaptureProtocol.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Record framing and message codecs
// for the private pipe between NereusSDR and its nereus-audio-capture
// helper process; no Thetis logic.
//
// Wire format (all integers little-endian):
//   record header, 12 bytes:
//     "NCAP"  u8 version  u8 type  u16 reserved(0)  u32 payloadBytes
//   JSON payloads (Hello/Status/Configure/Open/Stop/Shutdown): at most
//     kMaxJsonBytes bytes of UTF-8 JSON, one object with an exact key set.
//   PCM payload, 24-byte PCM header then samples:
//     u32 generation  u32 frameCount  u64 framePosition  u64 sentMonotonicNs
//     frameCount x float32 (mono, 48000 Hz), frameCount in 1..kMaxPcmFrames.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Process and PCM contract).  Requirement R-R3-36.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <deque>
#include <optional>

namespace NereusSDR::CaptureProtocol {

inline constexpr quint8 kVersion = 1;
inline constexpr int kHeaderBytes = 12;          // "NCAP", u8 version, u8 type, u16 reserved(0), u32 payloadBytes, little-endian
inline constexpr int kMaxJsonBytes = 4096;
inline constexpr int kPcmHeaderBytes = 24;       // u32 generation, u32 frameCount, u64 framePosition, u64 sentMonotonicNs
inline constexpr int kMaxPcmFrames = 4800;
inline constexpr int kHelperPcmFrames = 480;
inline constexpr int kSampleRate = 48'000;

// Largest payload any record type may carry (a full PCM record).
inline constexpr int kMaxPcmPayloadBytes = kPcmHeaderBytes + kMaxPcmFrames * 4;

enum class RecordType : quint8 { Hello = 1, Status = 2, Pcm = 3, Configure = 16, Open = 17, Stop = 18, Shutdown = 19 };

struct Record {
    RecordType type = RecordType::Hello;
    QByteArray payload;
};

// Complete record (header plus payload).  Empty QByteArray if the payload
// exceeds the type's bound or the type is not a known RecordType.
QByteArray encodeRecord(RecordType type, const QByteArray& payload);

// Bounded incremental record reader.
//
// append() parses as it goes: complete records move to an internal ready
// queue that next() drains in order, and only the one incomplete record at
// the tail stays in the byte buffer.  Every header is validated as soon as
// its 12 bytes arrive, so bufferedBytes() never exceeds
// kHeaderBytes + kMaxPcmPayloadBytes.  The first malformed header sets a
// sticky error: the buffer and any queued records are discarded, further
// input is ignored and next() returns nullopt forever.
class RecordReader {
public:
    enum class Error { None, BadMagic, BadVersion, BadReserved, UnknownType, Oversize };

    void append(const char* data, qsizetype size);
    std::optional<Record> next();        // one complete record, or nullopt
    Error error() const;                 // sticky; after an error next() returns nullopt forever
    qsizetype bufferedBytes() const;     // never above kHeaderBytes + the largest payload bound

private:
    void fail(Error error);

    QByteArray m_buffer;                 // current incomplete record only
    qsizetype m_expectedTotal = 0;       // header + payload once the header is validated; 0 before
    RecordType m_pendingType = RecordType::Hello;
    std::deque<Record> m_ready;
    Error m_error = Error::None;
};

struct PcmBlock {
    quint32 generation = 0;
    quint64 framePosition = 0;
    quint64 sentMonotonicNs = 0;
    QVector<float> samples;
};

// sentMonotonicNs: std::chrono::steady_clock nanoseconds when the helper wrote the record
// (a host-wide monotonic clock on macOS, Linux and Windows), used only for delay statistics.
//
// Returns a complete Pcm record, or an empty QByteArray when generation is 0,
// frameCount is outside 1..kMaxPcmFrames, samples is null, or any sample is
// not finite.
QByteArray encodePcm(quint32 generation, quint64 framePosition, quint64 sentMonotonicNs, const float* samples, int frameCount);
// Payload of a Pcm record.  Requires generation 1..4294967295, frameCount
// 1..4800, exact payload size and all samples finite.
std::optional<PcmBlock> decodePcm(const QByteArray& payload);

enum class HelperState { Permission, Opening, Ready, Failed, Stopped };
enum class FailReason { None, PermissionDenied, DeviceNotFound, OpenFailed, StartFailed, InputLost, Internal };

inline constexpr int kMaxStringChars = 512;
inline constexpr int kMinNativeRate = 8000;
inline constexpr int kMaxNativeRate = 384000;
inline constexpr int kMaxNativeChannels = 32;

struct Hello {
    int protocol = 0;
    qint64 pid = 0;
    QString build;
};

struct Configure {
    quint32 generation = 0;
    AudioDeviceConfig device;
};

struct Command {                         // Open and Stop
    quint32 generation = 0;
};

// nativeRate / nativeChannels: both 0 means "not known yet" and is allowed
// in every state except Ready; otherwise nativeRate must be in
// kMinNativeRate..kMaxNativeRate and nativeChannels in 1..kMaxNativeChannels.
struct Status {
    quint32 generation = 0;
    HelperState state = HelperState::Stopped;
    QString actualDevice;
    int nativeRate = 0;
    int nativeChannels = 0;
    FailReason reason = FailReason::None;
    QString detail;
};

// encodeX returns a complete record (empty when the message would not pass
// its own decoder); decodeX takes the JSON payload and validates the exact
// key set, value types and ranges.
QByteArray encodeHello(const Hello& hello);
std::optional<Hello> decodeHello(const QByteArray& json);

QByteArray encodeConfigure(const Configure& configure);
std::optional<Configure> decodeConfigure(const QByteArray& json);

QByteArray encodeOpen(const Command& command);
QByteArray encodeStop(const Command& command);
std::optional<Command> decodeCommand(const QByteArray& json);

QByteArray encodeStatus(const Status& status);
std::optional<Status> decodeStatus(const QByteArray& json);

QByteArray encodeShutdown();             // payload is the empty JSON object {}

} // namespace NereusSDR::CaptureProtocol
