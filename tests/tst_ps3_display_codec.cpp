// no-port-check: NereusSDR-original bounded PS3 display transport tests.
#include <QtTest>

#include "core/session/Ps3DisplayCodec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace NereusSDR;

namespace {

Ps3Snapshot snapshot(int samples, int corrections, std::uint64_t generation,
                     std::uint64_t sequence)
{
    Ps3Snapshot value;
    value.channelId = 3;
    value.sessionGeneration = generation;
    value.sequence = sequence;
    value.capturedAtUnixMilliseconds = 1'795'000'123'456LL;
    value.sampleCount = samples;
    value.correctionCount = corrections;
    value.phaseReferenceDegrees = -17.25;

    const auto fill = [](std::vector<double>& output, int count, double base) {
        output.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
            output.push_back(base + static_cast<double>(i) / 8192.0);
    };
    fill(value.x, samples, 0.0);
    fill(value.ym, samples, 1.0);
    fill(value.yc, samples, 2.0);
    fill(value.ys, samples, 3.0);
    fill(value.xmCorrection, corrections, 4.0);
    fill(value.ymCorrection, corrections, 5.0);
    fill(value.xaCorrection, corrections, 6.0);
    fill(value.yaCorrection, corrections, 7.0);
    return value;
}

void putU16(QByteArray& bytes, int offset, quint16 value)
{
    bytes[offset] = static_cast<char>(value >> 8);
    bytes[offset + 1] = static_cast<char>(value);
}

void putU32(QByteArray& bytes, int offset, quint32 value)
{
    bytes[offset] = static_cast<char>(value >> 24);
    bytes[offset + 1] = static_cast<char>(value >> 16);
    bytes[offset + 2] = static_cast<char>(value >> 8);
    bytes[offset + 3] = static_cast<char>(value);
}

void putU64(QByteArray& bytes, int offset, quint64 value)
{
    putU32(bytes, offset, static_cast<quint32>(value >> 32));
    putU32(bytes, offset + 4, static_cast<quint32>(value));
}

void compare(const Ps3Snapshot& actual, const Ps3Snapshot& expected)
{
    QCOMPARE(actual.channelId, expected.channelId);
    QCOMPARE(actual.sessionGeneration, expected.sessionGeneration);
    QCOMPARE(actual.sequence, expected.sequence);
    QCOMPARE(actual.capturedAtUnixMilliseconds, expected.capturedAtUnixMilliseconds);
    QCOMPARE(actual.sampleCount, expected.sampleCount);
    QCOMPARE(actual.correctionCount, expected.correctionCount);
    QCOMPARE(actual.phaseReferenceDegrees, expected.phaseReferenceDegrees);
    QVERIFY(actual.x == expected.x);
    QVERIFY(actual.ym == expected.ym);
    QVERIFY(actual.yc == expected.yc);
    QVERIFY(actual.ys == expected.ys);
    QVERIFY(actual.xmCorrection == expected.xmCorrection);
    QVERIFY(actual.ymCorrection == expected.ymCorrection);
    QVERIFY(actual.xaCorrection == expected.xaCorrection);
    QVERIFY(actual.yaCorrection == expected.yaCorrection);
}

} // namespace

class TestPs3DisplayCodec : public QObject {
    Q_OBJECT

private slots:
    void maximumFrameRoundTripsOutOfOrderAndLosslessly()
    {
        const auto input = snapshot(Ps3Snapshot::kMaxSampleCount,
                                    Ps3Snapshot::kMaxCorrectionCount,
                                    (std::uint64_t{1} << 60) + 7,
                                    (std::uint64_t{1} << 59) + 11);
        QString error;
        const QList<QByteArray> chunks = Ps3DisplayCodec::encode(input, &error);
        QVERIFY2(!chunks.isEmpty(), qPrintable(error));
        QVERIFY(chunks.size() > 1);
        for (const auto& chunk : chunks)
            QVERIFY(chunk.size() <= Ps3DisplayCodec::kMaxChunkBytes);

        Ps3DisplayAssembler assembler(input.sessionGeneration);
        std::optional<Ps3Snapshot> output;
        for (auto i = chunks.crbegin(); i != chunks.crend(); ++i) {
            error.clear();
            output = assembler.accept(*i, &error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
        }
        QVERIFY(output.has_value());
        compare(*output, input);
    }

    void encoderRejectsInvalidCountsVectorsAndValues()
    {
        QString error;
        auto value = snapshot(4, 4, 1, 1);
        value.x.pop_back();
        QVERIFY(Ps3DisplayCodec::encode(value, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        value = snapshot(4, 4, 1, 1);
        value.ym[2] = std::numeric_limits<double>::quiet_NaN();
        error.clear();
        QVERIFY(Ps3DisplayCodec::encode(value, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("finite"), Qt::CaseInsensitive));

        value = snapshot(4, 4, 1, 1);
        value.sampleCount = Ps3Snapshot::kMaxSampleCount + 1;
        error.clear();
        QVERIFY(Ps3DisplayCodec::encode(value, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void assemblerRejectsMalformedCountsAndNonFiniteValues()
    {
        QString error;
        const auto value = snapshot(4, 4, 9, 2);
        const auto chunks = Ps3DisplayCodec::encode(value, &error);
        QCOMPARE(chunks.size(), 1);

        Ps3DisplayAssembler assembler(value.sessionGeneration);
        QByteArray badCount = chunks.front();
        putU16(badCount, Ps3DisplayCodec::kSampleCountOffset, 5);
        QVERIFY(!assembler.accept(badCount, &error));
        QVERIFY(!error.isEmpty());

        assembler.reset(value.sessionGeneration);
        QByteArray badPhase = chunks.front();
        putU64(badPhase, Ps3DisplayCodec::kPhaseReferenceOffset,
               quint64{0x7ff8000000000000ULL});
        error.clear();
        QVERIFY(!assembler.accept(badPhase, &error));
        QVERIFY(error.contains(QStringLiteral("finite"), Qt::CaseInsensitive));

        assembler.reset(value.sessionGeneration);
        QByteArray badPayload = chunks.front();
        putU64(badPayload, Ps3DisplayCodec::kHeaderBytes,
               quint64{0x7ff8000000000000ULL});
        error.clear();
        QVERIFY(!assembler.accept(badPayload, &error));
        QVERIFY(error.contains(QStringLiteral("finite"), Qt::CaseInsensitive));
    }

    void assemblerRejectsWrongSessionDuplicatesStaleFramesAndResetWork()
    {
        QString error;
        const auto oldFrame = snapshot(Ps3Snapshot::kMaxSampleCount,
                                       Ps3Snapshot::kMaxCorrectionCount, 4, 90);
        const auto oldChunks = Ps3DisplayCodec::encode(oldFrame, &error);
        QVERIFY(oldChunks.size() > 1);

        Ps3DisplayAssembler assembler(4);
        QVERIFY(!assembler.accept(oldChunks.back(), &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!assembler.accept(oldChunks.back(), &error));
        QVERIFY(error.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));

        assembler.reset(5);
        error.clear();
        QVERIFY(!assembler.accept(oldChunks.front(), &error));
        QVERIFY(error.contains(QStringLiteral("session"), Qt::CaseInsensitive));

        const auto current = snapshot(Ps3Snapshot::kMaxSampleCount,
                                      Ps3Snapshot::kMaxCorrectionCount, 5, 1);
        const auto chunks = Ps3DisplayCodec::encode(current, &error);
        std::optional<Ps3Snapshot> decoded;
        for (auto i = chunks.crbegin(); i != chunks.crend(); ++i)
            decoded = assembler.accept(*i, &error);
        QVERIFY(decoded.has_value());
        compare(*decoded, current);

        error.clear();
        QVERIFY(!assembler.accept(chunks.front(), &error));
        QVERIFY(error.contains(QStringLiteral("stale"), Qt::CaseInsensitive));
    }

    void newerSequenceRetiresIncompleteFrame()
    {
        QString error;
        const auto older = snapshot(Ps3Snapshot::kMaxSampleCount,
                                    Ps3Snapshot::kMaxCorrectionCount, 12, 40);
        const auto olderChunks = Ps3DisplayCodec::encode(older, &error);
        QVERIFY(olderChunks.size() > 1);

        Ps3DisplayAssembler assembler(12);
        QVERIFY(!assembler.accept(olderChunks.back(), &error));
        QVERIFY(error.isEmpty());

        const auto newer = snapshot(3, 2, 12, 41);
        const auto newerChunks = Ps3DisplayCodec::encode(newer, &error);
        QCOMPARE(newerChunks.size(), 1);
        const auto decoded = assembler.accept(newerChunks.front(), &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        compare(*decoded, newer);

        error.clear();
        QVERIFY(!assembler.accept(olderChunks.front(), &error));
        QVERIFY(error.contains(QStringLiteral("stale"), Qt::CaseInsensitive));
    }

    void assemblerRejectsOversizeOffsetsGapsAndConflicts()
    {
        QString error;
        const auto value = snapshot(Ps3Snapshot::kMaxSampleCount,
                                    Ps3Snapshot::kMaxCorrectionCount, 7, 3);
        const auto chunks = Ps3DisplayCodec::encode(value, &error);
        QVERIFY(chunks.size() > 1);

        Ps3DisplayAssembler assembler(7);
        QByteArray oversize(Ps3DisplayCodec::kMaxChunkBytes + 1, '\0');
        QVERIFY(!assembler.accept(oversize, &error));
        QVERIFY(error.contains(QStringLiteral("large"), Qt::CaseInsensitive));

        QByteArray gap = chunks.at(1);
        putU32(gap, Ps3DisplayCodec::kChunkOffsetOffset, 1);
        error.clear();
        QVERIFY(!assembler.accept(gap, &error));
        QVERIFY(error.contains(QStringLiteral("offset"), Qt::CaseInsensitive));

        assembler.reset(7);
        QVERIFY(!assembler.accept(chunks.front(), &error));
        QVERIFY(error.isEmpty());
        QByteArray conflict = chunks.at(1);
        putU32(conflict, Ps3DisplayCodec::kChannelIdOffset, 99);
        QVERIFY(!assembler.accept(conflict, &error));
        QVERIFY(error.contains(QStringLiteral("conflict"), Qt::CaseInsensitive));
    }
};

QTEST_APPLESS_MAIN(TestPs3DisplayCodec)
#include "tst_ps3_display_codec.moc"
