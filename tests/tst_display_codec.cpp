// =================================================================
// tests/tst_display_codec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 3.
//
// These tests deliberately exercise codec state, not just bytes: a dropped
// delta, stale packet, or malformed block must never become a plausible row.
//
// =================================================================

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/session/media/DisplayCodec.h"

using namespace NereusSDR;

namespace {

DisplayCodecFrame makeFrame(quint32 sequence, int trace = 64, int waterfall = 48,
                            int wide = 0)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 42;
    frame.context.contextGeneration = 7;
    frame.context.minDbm = -160.0f;
    frame.context.maxDbm = -40.0f;
    frame.context.traceSamples = static_cast<quint16>(trace);
    frame.context.waterfallSamples = static_cast<quint16>(waterfall);
    frame.context.wideSamples = static_cast<quint16>(wide);
    frame.encoderSequence = sequence;
    frame.producerTimestamp = 1'000'000ULL + sequence;
    frame.waterfallAdvance = true;
    for (int i = 0; i < trace; ++i) {
        frame.traceDbm.append(-159.0f + 118.0f * static_cast<float>(i) / std::max(1, trace - 1));
    }
    for (int i = 0; i < waterfall; ++i) {
        frame.waterfallDbm.append(-145.0f + 80.0f * static_cast<float>((i * 7) % waterfall)
                                  / std::max(1, waterfall - 1));
    }
    for (int i = 0; i < wide; ++i) {
        frame.wideDbm.append(-120.0f + 20.0f * static_cast<float>(i)
                             / std::max(1, wide - 1));
    }
    return frame;
}

void verifyAccepted(DisplayCodecDecoder& decoder, const QByteArray& bytes)
{
    const DisplayCodecDecodeResult decoded = decoder.decode(bytes);
    QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
    QCOMPARE(decoded.reason, DisplayCodecReason::None);
}

float quantum(const DisplayCodecContext& context)
{
    return (context.maxDbm - context.minDbm) / 255.0f;
}

} // namespace

class TstDisplayCodec : public QObject
{
    Q_OBJECT

private slots:
    void quantizationBoundAndClipping()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        DisplayCodecFrame frame = makeFrame(1, 5, 5);
        frame.traceDbm = {-220.0f, -160.0f, -123.1f, -40.0f, 15.0f};
        frame.waterfallDbm = {-200.0f, -150.0f, -100.0f, -50.0f, 10.0f};
        const DisplayCodecDecodeResult decoded = decoder.decode(encoder.encode(frame));
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
        QCOMPARE(decoded.reason, DisplayCodecReason::None);
        const float bound = quantum(frame.context) * 0.5f + 1.0e-4f;
        for (int i = 0; i < frame.traceDbm.size(); ++i) {
            const float clipped = std::clamp(frame.traceDbm.at(i), frame.context.minDbm,
                                             frame.context.maxDbm);
            QVERIFY(std::abs(decoded.frame.traceDbm.at(i) - clipped) <= bound);
        }
        QCOMPARE(decoded.frame.traceDbm.first(), frame.context.minDbm);
        QCOMPARE(decoded.frame.traceDbm.last(), frame.context.maxDbm);
    }

    void finiteExtremeQuantizationRangeNeverDecodesNonfinite()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        DisplayCodecFrame frame = makeFrame(0, 3, 3, 3);
        const float largest = std::numeric_limits<float>::max();
        frame.context.minDbm = -largest;
        frame.context.maxDbm = largest;
        frame.traceDbm = {-largest, 0.0f, largest};
        frame.waterfallDbm = {largest, 0.0f, -largest};
        frame.wideDbm = {0.0f, largest, -largest};
        const DisplayCodecDecodeResult decoded = decoder.decode(encoder.encode(frame));
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
        for (float value : decoded.frame.traceDbm) { QVERIFY(std::isfinite(value)); }
        for (float value : decoded.frame.waterfallDbm) { QVERIFY(std::isfinite(value)); }
        for (float value : decoded.frame.wideDbm) { QVERIFY(std::isfinite(value)); }
        QCOMPARE(decoded.frame.traceDbm.first(), -largest);
        QCOMPARE(decoded.frame.traceDbm.last(), largest);
    }

    void deadZoneLongRampHasDocumentedBound()
    {
        DisplayCodecEncoder encoder(2);
        DisplayCodecDecoder decoder;
        DisplayCodecFrame frame = makeFrame(0, 32, 32);
        const float step = quantum(frame.context);
        const float bound = (2.0f + 0.5f) * step + 1.0e-4f;
        for (quint32 sequence = 0; sequence < 500; ++sequence) {
            frame.encoderSequence = sequence;
            frame.producerTimestamp = sequence;
            for (int i = 0; i < frame.traceDbm.size(); ++i) {
                frame.traceDbm[i] = -150.0f + static_cast<float>(sequence) * step * 0.21f
                                  + static_cast<float>(i) * step * 0.05f;
                frame.waterfallDbm[i] = -140.0f + static_cast<float>(sequence) * step * 0.19f
                                      + static_cast<float>(i) * step * 0.03f;
            }
            const DisplayCodecDecodeResult decoded = decoder.decode(encoder.encode(frame));
            QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
            for (int i = 0; i < frame.traceDbm.size(); ++i) {
                QVERIFY(std::abs(decoded.frame.traceDbm.at(i) - frame.traceDbm.at(i)) <= bound);
                QVERIFY(std::abs(decoded.frame.waterfallDbm.at(i) - frame.waterfallDbm.at(i)) <= bound);
            }
        }
    }

    void independentPlanesAndOptionalWideRoundTrip()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        DisplayCodecFrame frame = makeFrame(12, 31, 47, 19);
        frame.waterfallAdvance = false;
        const DisplayCodecDecodeResult decoded = decoder.decode(encoder.encode(frame));
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Accepted);
        QCOMPARE(decoded.frame.traceDbm.size(), 31);
        QCOMPARE(decoded.frame.waterfallDbm.size(), 47);
        QCOMPARE(decoded.frame.wideDbm.size(), 19);
        QVERIFY(!decoded.frame.waterfallAdvance);
        QVERIFY(std::abs(decoded.frame.traceDbm.at(3) - decoded.frame.waterfallDbm.at(3)) > 1.0f);
        QVERIFY(std::abs(decoded.frame.wideDbm.last() - decoded.frame.traceDbm.last()) > 1.0f);
    }

    void stableAndNoiseByteMeasurements()
    {
        DisplayCodecEncoder stableEncoder;
        DisplayCodecFrame stable = makeFrame(0, 4096, 4096, 768);
        for (float& value : stable.traceDbm) { value = -110.0f; }
        for (float& value : stable.waterfallDbm) { value = -110.0f; }
        for (float& value : stable.wideDbm) { value = -110.0f; }
        const QByteArray stableKey = stableEncoder.encode(stable);
        stable.encoderSequence = 1;
        const QByteArray stableDelta = stableEncoder.encode(stable);

        DisplayCodecEncoder noiseEncoder;
        DisplayCodecFrame noise = makeFrame(0, 4096, 4096, 768);
        quint32 state = 0x12345678U;
        auto nextNoise = [&state]() {
            state = state * 1664525U + 1013904223U;
            return -160.0f + static_cast<float>(state & 0xffffU) * 120.0f / 65535.0f;
        };
        for (float& value : noise.traceDbm) { value = nextNoise(); }
        for (float& value : noise.waterfallDbm) { value = nextNoise(); }
        for (float& value : noise.wideDbm) { value = nextNoise(); }
        const QByteArray noiseKey = noiseEncoder.encode(noise);
        noise.encoderSequence = 1;
        for (float& value : noise.traceDbm) { value = nextNoise(); }
        for (float& value : noise.waterfallDbm) { value = nextNoise(); }
        for (float& value : noise.wideDbm) { value = nextNoise(); }
        const QByteArray noiseDelta = noiseEncoder.encode(noise);

        QVERIFY(!stableKey.isEmpty());
        QVERIFY(!stableDelta.isEmpty());
        QVERIFY(!noiseKey.isEmpty());
        QVERIFY(!noiseDelta.isEmpty());
        QVERIFY(stableDelta.size() < noiseDelta.size());
        QVERIFY(noiseKey.size() <= DisplayCodecEncoder::kMaxEncodedBytes);
        DisplayCodecDecoder noiseDecoder;
        verifyAccepted(noiseDecoder, noiseKey);
        verifyAccepted(noiseDecoder, noiseDelta);
        qInfo().nospace() << "DisplayCodec v1 bytes: stable key=" << stableKey.size()
                          << " stable delta=" << stableDelta.size()
                          << " noise key=" << noiseKey.size()
                          << " noise delta=" << noiseDelta.size();
    }

    void gapReorderWrapAndKeyframeRecovery()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        DisplayCodecFrame first = makeFrame(0);
        const QByteArray packet0 = encoder.encode(first);
        verifyAccepted(decoder, packet0);
        DisplayCodecFrame second = makeFrame(1);
        const QByteArray packet1 = encoder.encode(second);
        DisplayCodecFrame third = makeFrame(2);
        const QByteArray packet2 = encoder.encode(third);
        QCOMPARE(decoder.decode(packet2).disposition, DisplayCodecDisposition::NeedKeyframe);
        QCOMPARE(decoder.decode(packet1).disposition, DisplayCodecDisposition::NeedKeyframe);
        DisplayCodecFrame recovery = makeFrame(3);
        const QByteArray recoveryPacket = encoder.encode(recovery, true);
        verifyAccepted(decoder, recoveryPacket);
        QCOMPARE(decoder.decode(packet1).disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoder.decode(packet1).reason, DisplayCodecReason::StaleSequence);

        DisplayCodecEncoder wrappingEncoder;
        DisplayCodecDecoder wrappingDecoder;
        DisplayCodecFrame nearWrap = makeFrame(std::numeric_limits<quint32>::max() - 1U);
        verifyAccepted(wrappingDecoder, wrappingEncoder.encode(nearWrap));
        DisplayCodecFrame max = makeFrame(std::numeric_limits<quint32>::max());
        verifyAccepted(wrappingDecoder, wrappingEncoder.encode(max));
        DisplayCodecFrame wrapped = makeFrame(0);
        verifyAccepted(wrappingDecoder, wrappingEncoder.encode(wrapped));
    }

    void nonconstantGapLatchesBeforeStaleResidualReconstruction()
    {
        auto flatFrame = [](quint32 sequence, float value) {
            DisplayCodecFrame frame = makeFrame(sequence, 16, 16);
            frame.context.minDbm = 0.0f;
            frame.context.maxDbm = 255.0f;
            for (float& sample : frame.traceDbm) { sample = value; }
            for (float& sample : frame.waterfallDbm) { sample = value; }
            return frame;
        };
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        verifyAccepted(decoder, encoder.encode(flatFrame(0, 0.0f)));
        const QByteArray missing = encoder.encode(flatFrame(1, 200.0f));
        const QByteArray arrivedAfterGap = encoder.encode(flatFrame(2, 100.0f));

        // Against q0=0, this packet's residual from the sender's missing
        // q1=200 is -100. It must request a keyframe, not reject because
        // applying -100 to stale q0 would underflow.
        QCOMPARE(decoder.decode(arrivedAfterGap).disposition,
                 DisplayCodecDisposition::NeedKeyframe);
        QCOMPARE(decoder.decode(missing).disposition, DisplayCodecDisposition::NeedKeyframe);
        verifyAccepted(decoder, encoder.encode(flatFrame(3, 80.0f), true));
    }

    void encoderRejectsStaleSequenceAndUngeneratedContextShape()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecFrame first = makeFrame(4);
        QVERIFY(!encoder.encode(first).isEmpty());
        QVERIFY(encoder.encode(first).isEmpty());
        DisplayCodecFrame changedRange = first;
        changedRange.encoderSequence = 5;
        changedRange.context.maxDbm = -30.0f;
        QVERIFY(encoder.encode(changedRange).isEmpty());
        changedRange.context.contextGeneration = 8;
        changedRange.encoderSequence = 0;
        QVERIFY(!encoder.encode(changedRange).isEmpty());
    }

    void malformedPacketsDoNotAdvanceHistory()
    {
        DisplayCodecEncoder encoder;
        DisplayCodecDecoder decoder;
        DisplayCodecFrame key = makeFrame(9);
        verifyAccepted(decoder, encoder.encode(key));
        DisplayCodecFrame delta = makeFrame(10);
        const QByteArray validDelta = encoder.encode(delta);

        QByteArray truncated = validDelta;
        truncated.chop(3);
        QCOMPARE(decoder.decode(truncated).disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoder.decode(truncated).reason, DisplayCodecReason::Malformed);
        verifyAccepted(decoder, validDelta);

        DisplayCodecFrame next = makeFrame(11);
        const QByteArray validNext = encoder.encode(next);
        QByteArray unknownFlags = validNext;
        unknownFlags[5] = static_cast<char>(static_cast<unsigned char>(unknownFlags.at(5)) | 0x80U);
        QCOMPARE(decoder.decode(unknownFlags).reason, DisplayCodecReason::UnknownFlags);
        verifyAccepted(decoder, validNext);

        QByteArray oversized(DisplayCodecEncoder::kMaxEncodedBytes + 1, '\0');
        QCOMPARE(decoder.decode(oversized).reason, DisplayCodecReason::Oversized);
        DisplayCodecFrame nonfinite = makeFrame(12);
        nonfinite.traceDbm[0] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(encoder.encode(nonfinite).isEmpty());

        DisplayCodecFrame blockFrame = makeFrame(12);
        const QByteArray validBlockFrame = encoder.encode(blockFrame);
        QByteArray badBlockCount = validBlockFrame;
        // First plane prefix starts immediately after the fixed 42-byte header.
        badBlockCount[43] = static_cast<char>(static_cast<unsigned char>(badBlockCount.at(43)) + 1U);
        QCOMPARE(decoder.decode(badBlockCount).reason, DisplayCodecReason::Malformed);
        verifyAccepted(decoder, validBlockFrame);

        DisplayCodecEncoder oldEncoder;
        DisplayCodecFrame oldContext = makeFrame(0);
        oldContext.context.contextGeneration = 6;
        const QByteArray oldPacket = oldEncoder.encode(oldContext);
        QCOMPARE(decoder.decode(oldPacket).disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoder.decode(oldPacket).reason, DisplayCodecReason::OldContext);
    }

    // Structure is checked before the endpoint, context, generation,
    // sequence and history rules: a packet that is both malformed and
    // refused rejects as malformed, whatever state the decoder is in, and
    // history is untouched.
    void malformedAndRefusedPacketsRejectAsMalformed()
    {
        DisplayCodecEncoder encoder;
        const QByteArray key = encoder.encode(makeFrame(1));
        const QByteArray delta = encoder.encode(makeFrame(2));
        const QByteArray lost = encoder.encode(makeFrame(3));
        const QByteArray recovery = encoder.encode(makeFrame(4), true);
        QVERIFY(!key.isEmpty() && !delta.isEmpty() && !lost.isEmpty() && !recovery.isEmpty());

        // A delta with a malformed plane (block size code 4), to a fresh
        // decoder that has no history.
        QByteArray badPlaneDelta = delta;
        badPlaneDelta[42] = 4;
        DisplayCodecDecoder fresh;
        DisplayCodecDecodeResult decoded = fresh.decode(badPlaneDelta);
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoded.reason, DisplayCodecReason::Malformed);
        verifyAccepted(fresh, key);

        // A stale delta (sequence 0, older than the accepted 1) cut short
        // by one byte, after a keyframe was accepted.
        QByteArray staleTruncated = delta;
        staleTruncated[16] = 0;
        staleTruncated[17] = 0;
        staleTruncated[18] = 0;
        staleTruncated[19] = 0;
        staleTruncated.chop(1);
        DisplayCodecDecoder afterKey;
        verifyAccepted(afterKey, key);
        decoded = afterKey.decode(staleTruncated);
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoded.reason, DisplayCodecReason::Malformed);
        verifyAccepted(afterKey, delta);

        // A keyframe whose first plane claims one block too many, to a
        // decoder that needs a keyframe after a gap.
        QByteArray badKeyframe = recovery;
        badKeyframe[44] = static_cast<char>(static_cast<unsigned char>(badKeyframe.at(44)) + 1U);
        DisplayCodecDecoder needsKeyframe;
        verifyAccepted(needsKeyframe, key);
        QCOMPARE(needsKeyframe.decode(lost).disposition, DisplayCodecDisposition::NeedKeyframe);
        decoded = needsKeyframe.decode(badKeyframe);
        QCOMPARE(decoded.disposition, DisplayCodecDisposition::Rejected);
        QCOMPARE(decoded.reason, DisplayCodecReason::Malformed);
        verifyAccepted(needsKeyframe, recovery);
    }
};

QTEST_MAIN(TstDisplayCodec)
#include "tst_display_codec.moc"
