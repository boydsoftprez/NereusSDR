// =================================================================
// src/core/WidebandFrameAccumulator.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See WidebandFrameAccumulator.h
// for context. State machine + sample decode behaviour port-cited
// to Thetis network.c [v2.10.3.15] inline below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic F Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================
#include "core/WidebandFrameAccumulator.h"

namespace NereusSDR {

WidebandFrameAccumulator::WidebandFrameAccumulator(QObject* parent)
    : QObject(parent)
    , m_frameBuffer(kSamplesPerFrame, 0.0f)
{
}

void WidebandFrameAccumulator::pushPacket(int seq, const QByteArray& payload)
{
    // Thetis network.c:559 [v2.10.3.15]: drop malformed packets.
    // Upstream guards by total recvfrom length (1028 = 4 hdr + 1024 payload);
    // we only see the payload bytes here so the payload-size guard suffices.
    if (payload.size() < kSamplesPerPacket * 2) {
        return;
    }

    // Thetis network.c:574-582 [v2.10.3.15] (wb_state == 0):
    // wait for seq=0 before beginning a frame.
    if (!m_inFrame) {
        if (seq != 0) {
            return;
        }
        m_inFrame = true;
        m_expectedSeq = 0;
        m_frameBuffer.fill(0.0f);
    }

    // Thetis network.c:585-597 [v2.10.3.15] (wb_state == 1):
    // sequence match -> decode + advance; sequence mismatch ->
    // zero-pad the remainder of the frame and emit, then reset
    // (state goes back to 0).
    if (seq != m_expectedSeq) {
        // Remainder of frame is already 0.0f because the per-packet
        // decode only writes its own 512-sample slice; everything
        // past (m_expectedSeq * kSamplesPerPacket) is still the
        // initial fill(0.0f) from when the frame started.
        emitAndReset();
        return;
    }

    // Decode 512 16-bit BE samples into the frame slot for this seq.
    decode512Samples(payload,
                     m_frameBuffer.begin() + (seq * kSamplesPerPacket));

    m_expectedSeq++;
    if (m_expectedSeq >= kPacketsPerFrame) {
        // Thetis network.c:588-589 [v2.10.3.15]: at last packet,
        // return wb_state to 0.
        emitAndReset();
    }
}

void WidebandFrameAccumulator::emitAndReset()
{
    emit frameReady(m_frameBuffer);
    m_inFrame = false;
    m_expectedSeq = 0;
}

void WidebandFrameAccumulator::decode512Samples(const QByteArray& payload,
                                                QVector<float>::iterator out)
{
    // Thetis network.c:566-571 [v2.10.3.15]:
    //   // NOTE:  This code assumes 16-bits per sample ... can add other options as needed.
    //   for (ii = 0, jj = 4; ii < wb_spp; ii++, jj += 2)
    //       wb_buff[ii] = const_1_div_2147483648_ *
    //                     (double)(readbuf[jj + 0] << 24 |
    //                              readbuf[jj + 1] << 16);
    //
    // Upstream places byte 0 in bits 31..24 and byte 1 in bits 23..16
    // of a 32-bit word, leaving the low 16 bits zero, then multiplies
    // by 1/2^31. Because readbuf is unsigned char, byte 0 promotes
    // through int and a value >= 0x80 in byte 0 yields a negative
    // signed int (0x80000000 == INT_MIN), so the result range is
    // [-1.0, +1.0 - 2^-15]. The mathematical equivalent for the upper
    // 16 bits is (int16_BE_sample) / 32768.0, which is what we compute
    // here for clarity.
    //
    // Note: Thetis starts jj at 4 because it walks the full UDP read
    // buffer (skipping the 4-byte wideband packet header). The caller
    // here passes the already-stripped payload (no 4-byte header), so
    // we start at byte 0.
    const char* bytes = payload.constData();
    for (int i = 0; i < kSamplesPerPacket; ++i) {
        const qint16 sample =
            qint16((quint16(quint8(bytes[i * 2])) << 8) |
                   quint16(quint8(bytes[i * 2 + 1])));
        *out++ = float(sample) / 32768.0f;
    }
}

} // namespace NereusSDR
