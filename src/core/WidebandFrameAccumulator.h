// =================================================================
// src/core/WidebandFrameAccumulator.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Assembles wideband ADC stream
// packets into 32-packet frames. Each packet carries 512 16-bit
// big-endian samples (1024 bytes payload) and frames carry
// 32 * 512 = 16384 samples. Thetis-equivalent behaviour
// (network.c:558-602 [v2.10.3.15]) lives inside the wideband
// thread plus a per-ADC state struct rather than a reusable
// class; this NereusSDR-original wrapper folds the same state
// machine into a Qt object that the P2RadioConnection wideband
// receive path (Task 3) can drive directly. See
// docs/architecture/2026-05-26-phase3f-sub-epic-f-wideband-plan.md
// Task 2 for the design context.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic F Task 2.
//                                    NereusSDR-original 32-packet
//                                    wideband ADC frame accumulator
//                                    with Thetis-faithful sequence
//                                    error handling (zero-pad
//                                    remainder, emit, reset).
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================
#pragma once

#include <QObject>
#include <QByteArray>
#include <QVector>

namespace NereusSDR {

/// Assembles wideband ADC packets into 32-packet frames.
///
/// Each packet: 512 16-bit big-endian samples (1024 bytes payload).
/// Each frame: 32 packets x 512 samples = 16384 floats.
///
/// State machine mirrors Thetis network.c:572-600 [v2.10.3.15]:
///   - State 0 (idle): wait for seq=0 to begin a frame.
///   - State 1 (in-frame): on seq match, decode + advance. On seq
///     mismatch, zero-pad the remainder of the current frame, emit
///     the (now partial) frame, return to state 0.
///   - On reaching the last packet of a frame (seq == 31), emit and
///     return to state 0.
class WidebandFrameAccumulator : public QObject {
    Q_OBJECT
public:
    explicit WidebandFrameAccumulator(QObject* parent = nullptr);

    /// Push one wideband packet's payload into the accumulator.
    /// `payload` must be at least `kSamplesPerPacket * 2` bytes
    /// (1024 bytes for 512 16-bit samples). Shorter payloads are
    /// dropped silently (malformed packet, matches Thetis's
    /// `nrecv != 1028` guard at network.c:559).
    void pushPacket(int sequenceNumber, const QByteArray& payload);

signals:
    /// Emitted when a full (or sequence-error padded) 16384-sample
    /// frame is ready.
    void frameReady(QVector<float> samples);

private:
    static constexpr int kSamplesPerPacket = 512;
    static constexpr int kPacketsPerFrame  = 32;
    static constexpr int kSamplesPerFrame  = kSamplesPerPacket * kPacketsPerFrame;

    int            m_expectedSeq {0};
    QVector<float> m_frameBuffer;  // current frame being assembled
    bool           m_inFrame {false};

    void emitAndReset();
    void decode512Samples(const QByteArray& payload,
                          QVector<float>::iterator out);
};

} // namespace NereusSDR

// QVector<float> is already a Qt built-in metatype via QList<float>
// (Qt6 aliases QVector<T> = QList<T>); no Q_DECLARE_METATYPE needed
// here. QSignalSpy will pick it up at runtime once
// qRegisterMetaType<QVector<float>>() runs (see the test fixture's
// initTestCase()).
