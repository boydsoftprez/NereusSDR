// no-port-check: tests for the NereusSDR-owned bounded WDSPNN validator.

#include <QtTest/QtTest>

#include "core/dsp/DspAssetValidation.h"

#include <bit>
#include <cmath>
#include <cstring>

extern "C" {
extern const unsigned char nnr_model_0_data[];
extern const unsigned int nnr_model_0_size;
extern const unsigned char nnr_model_1_data[];
extern const unsigned int nnr_model_1_size;
}

using namespace NereusSDR;

namespace {

quint32 readU32(const QByteArray& bytes, qsizetype offset)
{
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16)
           | (quint32(p[3]) << 24);
}

quint64 readU64(const QByteArray& bytes, qsizetype offset)
{
    return quint64(readU32(bytes, offset)) | (quint64(readU32(bytes, offset + 4)) << 32);
}

void writeU32(QByteArray& bytes, qsizetype offset, quint32 value)
{
    for (int i = 0; i < 4; ++i) {
        bytes[offset + i] = char((value >> (8 * i)) & 0xffu);
    }
}

void writeU64(QByteArray& bytes, qsizetype offset, quint64 value)
{
    writeU32(bytes, offset, quint32(value));
    writeU32(bytes, offset + 4, quint32(value >> 32));
}

QByteArray embeddedModel(const unsigned char* data, unsigned int size)
{
    return QByteArray(reinterpret_cast<const char*>(data), qsizetype(size));
}

QByteArray convertF64ModelToF32(const QByteArray& source)
{
    const quint32 count = readU32(source, 12);
    const quint64 oldDataOffset = readU64(source, 16);
    QByteArray converted = source.left(qsizetype(oldDataOffset));
    QByteArray data;

    for (quint32 i = 0; i < count; ++i) {
        const qsizetype descriptor = 32 + qsizetype(i) * 72;
        const quint32 dtype = readU32(source, descriptor + 40);
        const quint32 oldOffset = readU32(source, descriptor + 64);
        const quint32 oldBytes = readU32(source, descriptor + 68);
        if (dtype != 1 || oldBytes % 8u != 0) {
            return {};
        }

        const quint32 newOffset = quint32(data.size());
        const quint32 elements = oldBytes / 8u;
        for (quint32 element = 0; element < elements; ++element) {
            quint64 bits = readU64(source, qsizetype(oldDataOffset + oldOffset) + 8 * element);
            const double value = std::bit_cast<double>(bits);
            const quint32 fbits = std::bit_cast<quint32>(float(value));
            const qsizetype at = data.size();
            data.resize(at + 4);
            writeU32(data, at, fbits);
        }
        writeU32(converted, descriptor + 40, 0);
        writeU32(converted, descriptor + 64, newOffset);
        writeU32(converted, descriptor + 68, elements * 4u);
    }

    writeU64(converted, 24, quint64(data.size()));
    converted.append(data);
    return converted;
}

} // namespace

class TestNnrModelAssets : public QObject
{
    Q_OBJECT

private slots:
    void acceptsBothEmbeddedArchitectures()
    {
        const DspAssetValidationResult standard = DspAssetValidation::validateNnrModel(
            embeddedModel(nnr_model_0_data, nnr_model_0_size));
        QVERIFY2(standard.accepted, qPrintable(standard.error));
        QCOMPARE(standard.kind, DspAssetKind::NnrModel);
        QCOMPARE(standard.format, QStringLiteral("WDSPNN"));
        QCOMPARE(standard.version, 1);
        QCOMPARE(standard.compatibility, QStringLiteral("wdsp-nnr-model-0"));
        QCOMPARE(standard.tensorCount, 57);
        QCOMPARE(standard.numericEncoding, QStringLiteral("f64"));

        const DspAssetValidationResult premium = DspAssetValidation::validateNnrModel(
            embeddedModel(nnr_model_1_data, nnr_model_1_size));
        QVERIFY2(premium.accepted, qPrintable(premium.error));
        QCOMPARE(premium.compatibility, QStringLiteral("wdsp-nnr-model-1"));
        QCOMPARE(premium.tensorCount, 57);
    }

    void acceptsConvertedF32Model()
    {
        const QByteArray converted = convertF64ModelToF32(
            embeddedModel(nnr_model_0_data, nnr_model_0_size));
        const DspAssetValidationResult result = DspAssetValidation::validateNnrModel(converted);
        QVERIFY2(result.accepted, qPrintable(result.error));
        QCOMPARE(result.compatibility, QStringLiteral("wdsp-nnr-model-0"));
        QCOMPARE(result.numericEncoding, QStringLiteral("f32"));
    }

    void rejectsBadMagicAndTruncation()
    {
        QByteArray bad = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        bad[0] = 'X';
        QVERIFY(DspAssetValidation::validateNnrModel(bad).error.contains(QStringLiteral("magic"),
                                                                           Qt::CaseInsensitive));

        bad = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        bad.chop(1);
        QVERIFY(DspAssetValidation::validateNnrModel(bad).error.contains(QStringLiteral("truncated"),
                                                                           Qt::CaseInsensitive));
    }

    void rejectsDuplicateAndOverlappingTensors()
    {
        QByteArray duplicate = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        std::memcpy(duplicate.data() + 32 + 72, duplicate.constData() + 32, 40);
        QVERIFY(DspAssetValidation::validateNnrModel(duplicate).error.contains(
            QStringLiteral("duplicate"), Qt::CaseInsensitive));

        QByteArray overlap = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        writeU32(overlap, 32 + 72 + 64, readU32(overlap, 32 + 64));
        QVERIFY(DspAssetValidation::validateNnrModel(overlap).error.contains(
            QStringLiteral("overlap"), Qt::CaseInsensitive));
    }

    void rejectsOverflowWrongShapeAndNonFiniteWeights()
    {
        QByteArray overflow = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        writeU32(overflow, 32 + 48, 0xffffffffu);
        QVERIFY(DspAssetValidation::validateNnrModel(overflow).error.contains(
            QStringLiteral("extent"), Qt::CaseInsensitive));

        QByteArray wrongShape = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        // cfg is the first descriptor; its rank-1 extent must remain seven.
        writeU32(wrongShape, 32 + 48, 6);
        writeU32(wrongShape, 32 + 68, 48);
        QVERIFY(DspAssetValidation::validateNnrModel(wrongShape).error.contains(
            QStringLiteral("shape"), Qt::CaseInsensitive));

        QByteArray nonFinite = embeddedModel(nnr_model_0_data, nnr_model_0_size);
        const quint64 dataOffset = readU64(nonFinite, 16);
        writeU64(nonFinite, qsizetype(dataOffset), 0x7ff8000000000000ULL);
        QVERIFY(DspAssetValidation::validateNnrModel(nonFinite).error.contains(
            QStringLiteral("finite"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(TestNnrModelAssets)
#include "tst_nnr_model_assets.moc"
