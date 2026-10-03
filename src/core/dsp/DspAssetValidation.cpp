// =================================================================
// src/core/dsp/DspAssetValidation.cpp  (NereusSDR)
// =================================================================
//
// Ported from TAPR OpenHPSDR-wdsp sources at
// b02d5bac675dd2f33ec2bab2b339f79a597c47dd:
//   wdsp 2.10/Source/nnio.c:35-195 — WDSPNN v1 binary layout
//   wdsp 2.10/Source/nnet.c:889-1187 — accepted NNR tensor names/shapes
//   wdsp 2.10/Source/nurbs_spline.c:779-1099 — PS3 v2 text grammar
//
// The validators add host-side bounds, checked arithmetic, exact architecture
// checks, finite-number checks, duplicate/overlap rejection and exact EOF.
// They do not call WDSP or change live DSP state.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-21 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via OpenAI Codex.
//   2026-09-23 - NR3 (rnnoise) model validator added by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude Code.
//                NereusSDR-original: it calls the public rnnoise API
//                (rnnoise_model_from_buffer, rnnoise_create) and ports no
//                upstream logic.
//   2026-09-24 - isOperatorMessage() added by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code
//                (R-IOS-01): which messages the Core sends as they are.
// =================================================================

/*  nnio.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@pratt.one

*/

/*  nnet.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

/*  nurbs_spline.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@pratt.one

*/

#include "DspAssetValidation.h"

#include <QCryptographicHash>
#include <QHash>

#ifdef HAVE_WDSP
#include "rnnoise.h"
#endif
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {

constexpr qsizetype kNnioHeaderBytes = 32;
constexpr qsizetype kNnioDescriptorBytes = 72;
constexpr int kNnioNameBytes = 40;
constexpr quint32 kNnioTensorCount = 57;
constexpr quint64 kMaxDecodedModelBytes = 128ULL * 1024 * 1024;

struct TensorDescriptor {
    QString name;
    quint32 dtype{0};
    QVector<quint32> shape;
    quint64 elements{0};
    quint64 offset{0};
    quint64 bytes{0};
};

DspAssetValidationResult rejected(DspAssetKind kind, const QByteArray& bytes,
                                  const QString& error)
{
    DspAssetValidationResult result;
    result.kind = kind;
    result.size = bytes.size();
    result.error = error;
    return result;
}

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

bool checkedAdd(quint64 a, quint64 b, quint64* result)
{
    if (b > std::numeric_limits<quint64>::max() - a) {
        return false;
    }
    *result = a + b;
    return true;
}

bool checkedMultiply(quint64 a, quint64 b, quint64* result)
{
    if (a != 0 && b > std::numeric_limits<quint64>::max() / a) {
        return false;
    }
    *result = a * b;
    return true;
}

double tensorValue(const QByteArray& bytes, quint64 dataOffset,
                   const TensorDescriptor& tensor, quint64 element)
{
    const quint64 width = tensor.dtype == 0 ? 4 : 8;
    const qsizetype offset = qsizetype(dataOffset + tensor.offset + element * width);
    if (tensor.dtype == 0) {
        return double(std::bit_cast<float>(readU32(bytes, offset)));
    }
    return std::bit_cast<double>(readU64(bytes, offset));
}

QHash<QString, QVector<quint32>> expectedNnrTensors(const QVector<quint32>& cfg)
{
    const quint32 hid = cfg[5];
    const quint32 order = cfg[6];
    const QVector<quint32> channels{cfg[1], cfg[2], cfg[3], cfg[4]};
    QHash<QString, QVector<quint32>> expected;
    expected.insert(QStringLiteral("cfg"), {7});
    expected.insert(QStringLiteral("lookahead"), {1});

    for (int i = 0; i < 4; ++i) {
        const quint32 cin = i == 0 ? 2 : channels[i - 1];
        const quint32 cout = channels[i];
        const QString prefix = QStringLiteral("enc%1_").arg(i + 1);
        expected.insert(prefix + QStringLiteral("w"), {cout, cin, 2, 5});
        expected.insert(prefix + QStringLiteral("b"), {cout});
        expected.insert(prefix + QStringLiteral("slope"), {cout});
    }

    for (int layer = 1; layer <= 2; ++layer) {
        const QString prefix = QStringLiteral("dp%1").arg(layer);
        const quint32 nch = channels[3];
        expected.insert(prefix + QStringLiteral("_ifwd_wih"), {3 * hid, nch});
        expected.insert(prefix + QStringLiteral("_ifwd_whh"), {3 * hid, hid});
        expected.insert(prefix + QStringLiteral("_ifwd_bih"), {3 * hid});
        expected.insert(prefix + QStringLiteral("_ifwd_bhh"), {3 * hid});
        expected.insert(prefix + QStringLiteral("_ibwd_wih"), {3 * hid, nch});
        expected.insert(prefix + QStringLiteral("_ibwd_whh"), {3 * hid, hid});
        expected.insert(prefix + QStringLiteral("_ibwd_bih"), {3 * hid});
        expected.insert(prefix + QStringLiteral("_ibwd_bhh"), {3 * hid});
        expected.insert(prefix + QStringLiteral("_tgru_wih"), {3 * nch, nch});
        expected.insert(prefix + QStringLiteral("_tgru_whh"), {3 * nch, nch});
        expected.insert(prefix + QStringLiteral("_tgru_bih"), {3 * nch});
        expected.insert(prefix + QStringLiteral("_tgru_bhh"), {3 * nch});
        expected.insert(prefix + QStringLiteral("_ilin_w"), {nch, 2 * hid});
        expected.insert(prefix + QStringLiteral("_ilin_b"), {nch});
        expected.insert(prefix + QStringLiteral("_tlin_w"), {nch, nch});
        expected.insert(prefix + QStringLiteral("_tlin_b"), {nch});
    }

    for (int i = 0; i < 4; ++i) {
        const quint32 cup = i == 0 ? channels[3] : channels[3 - i];
        const quint32 cskip = channels[3 - i];
        const quint32 cin = cup + cskip;
        const quint32 cout = i == 3 ? 2 * order : channels[2 - i];
        const QString prefix = QStringLiteral("dec%1_").arg(i + 1);
        expected.insert(prefix + QStringLiteral("w"), {cin, cout, 2, 5});
        expected.insert(prefix + QStringLiteral("b"), {cout});
        if (i < 3) {
            expected.insert(prefix + QStringLiteral("slope"), {cout});
        }
    }
    return expected;
}

class TokenCursor
{
public:
    explicit TokenCursor(const QByteArray& bytes)
        : m_bytes(bytes)
    {
    }

    bool next(QByteArray* token)
    {
        while (m_offset < m_bytes.size()
               && std::isspace(static_cast<unsigned char>(m_bytes.at(m_offset)))) {
            ++m_offset;
        }
        if (m_offset >= m_bytes.size()) {
            token->clear();
            return false;
        }
        const qsizetype start = m_offset;
        while (m_offset < m_bytes.size()
               && !std::isspace(static_cast<unsigned char>(m_bytes.at(m_offset)))) {
            ++m_offset;
        }
        *token = m_bytes.mid(start, m_offset - start);
        return true;
    }

    bool atEnd()
    {
        QByteArray token;
        return !next(&token);
    }

private:
    const QByteArray& m_bytes;
    qsizetype m_offset{0};
};

bool expectToken(TokenCursor& cursor, const QByteArray& expected, QString* error)
{
    QByteArray actual;
    if (!cursor.next(&actual) || actual != expected) {
        *error = QStringLiteral("Expected '%1' in PS3 correction data")
                     .arg(QString::fromLatin1(expected));
        return false;
    }
    return true;
}

bool readInteger(TokenCursor& cursor, qint64* value, const QString& field, QString* error)
{
    QByteArray token;
    bool ok = false;
    if (!cursor.next(&token)) {
        *error = QStringLiteral("Missing integer for %1").arg(field);
        return false;
    }
    const qlonglong parsed = QString::fromLatin1(token).toLongLong(&ok, 10);
    if (!ok || parsed < std::numeric_limits<int>::min()
        || parsed > std::numeric_limits<int>::max()) {
        *error = QStringLiteral("Invalid 32-bit integer for %1").arg(field);
        return false;
    }
    *value = parsed;
    return true;
}

bool readFinite(TokenCursor& cursor, double* value, const QString& field, QString* error)
{
    QByteArray token;
    bool ok = false;
    if (!cursor.next(&token)) {
        *error = QStringLiteral("Missing finite value for %1").arg(field);
        return false;
    }
    const double parsed = QString::fromLatin1(token).toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) {
        *error = QStringLiteral("%1 must be finite").arg(field);
        return false;
    }
    *value = parsed;
    return true;
}

bool readEma(TokenCursor& cursor, double* checksum, QString* error)
{
    double alpha = 0;
    double alphaLo = 0;
    double xBoundary = 0;
    double clipLo = 0;
    double clipHi = 0;
    qint64 count = 0;
    qint64 warmup = 0;
    qint64 points = 0;

    if (!expectToken(cursor, "curve_ema_alpha", error)
        || !readFinite(cursor, &alpha, QStringLiteral("curve_ema_alpha"), error)
        || !expectToken(cursor, "curve_ema_alpha_lo", error)
        || !readFinite(cursor, &alphaLo, QStringLiteral("curve_ema_alpha_lo"), error)
        || !expectToken(cursor, "curve_ema_x_bnd", error)
        || !readFinite(cursor, &xBoundary, QStringLiteral("curve_ema_x_bnd"), error)
        || !expectToken(cursor, "curve_ema_clip", error)
        || !readFinite(cursor, &clipLo, QStringLiteral("curve_ema_clip low"), error)
        || !readFinite(cursor, &clipHi, QStringLiteral("curve_ema_clip high"), error)
        || !expectToken(cursor, "curve_ema_count", error)
        || !readInteger(cursor, &count, QStringLiteral("curve_ema_count"), error)
        || !expectToken(cursor, "curve_ema_warmup", error)
        || !readInteger(cursor, &warmup, QStringLiteral("curve_ema_warmup"), error)
        || !expectToken(cursor, "curve_ema_pts", error)
        || !readInteger(cursor, &points, QStringLiteral("curve_ema_pts"), error)) {
        return false;
    }
    if (count < 0 || warmup < 0) {
        *error = QStringLiteral("PS3 EMA count and warmup must be non-negative");
        return false;
    }
    if (points != 256) {
        *error = QStringLiteral("PS3 curve_ema_pts must be exactly 256");
        return false;
    }

    *checksum += alpha + alphaLo + xBoundary + clipLo + clipHi + double(count)
                 + double(warmup);
    for (int i = 0; i < 256; ++i) {
        double value = 0;
        if (!readFinite(cursor, &value, QStringLiteral("curve EMA value %1").arg(i), error)) {
            return false;
        }
        *checksum += value;
    }
    return true;
}

} // namespace

QString dspAssetKindName(DspAssetKind kind)
{
    switch (kind) {
    case DspAssetKind::NnrModel:
        return QStringLiteral("nnr-model");
    case DspAssetKind::Ps3Correction:
        return QStringLiteral("ps3-correction");
    case DspAssetKind::Nr3Model:
        return QStringLiteral("nr3-model");
    }
    return {};
}

bool dspAssetKindFromName(const QString& name, DspAssetKind* kind)
{
    if (!kind) {
        return false;
    }
    if (name == QStringLiteral("nnr-model")) {
        *kind = DspAssetKind::NnrModel;
        return true;
    }
    if (name == QStringLiteral("ps3-correction")) {
        *kind = DspAssetKind::Ps3Correction;
        return true;
    }
    if (name == QStringLiteral("nr3-model")) {
        *kind = DspAssetKind::Nr3Model;
        return true;
    }
    return false;
}

bool dspAssetKindFromInt(qint64 value, DspAssetKind* kind)
{
    switch (value) {
    case static_cast<qint64>(DspAssetKind::NnrModel):
    case static_cast<qint64>(DspAssetKind::Ps3Correction):
    case static_cast<qint64>(DspAssetKind::Nr3Model):
        if (kind) {
            *kind = static_cast<DspAssetKind>(value);
        }
        return true;
    default:
        return false;
    }
}

qint64 DspAssetValidation::sizeLimit(DspAssetKind kind)
{
    switch (kind) {
    case DspAssetKind::NnrModel:
        return kMaxNnrModelBytes;
    case DspAssetKind::Ps3Correction:
        return kMaxPs3CorrectionBytes;
    case DspAssetKind::Nr3Model:
        return kMaxNr3ModelBytes;
    }
    return 0;
}

bool DspAssetValidation::isOperatorMessage(const QString& message)
{
    // The NR3 validator's own sentences below, and DspAssetStore's two
    // that repeat them.
    static const QStringList kOperatorMessages{
        QStringLiteral("The NR3 model is larger than 16 MiB."),
        QStringLiteral("This file is not an NR3 model."),
        QStringLiteral("There was not enough memory to check the NR3 model."),
        QStringLiteral("This file is not an NR3 model this Core can use."),
        QStringLiteral("This Core was built without NR3."),
        QStringLiteral("NR3 models belong to the Core, not to one radio."),
    };
    return kOperatorMessages.contains(message);
}

DspAssetValidationResult DspAssetValidation::validate(DspAssetKind kind,
                                                       const QByteArray& bytes)
{
    switch (kind) {
    case DspAssetKind::NnrModel:
        return validateNnrModel(bytes);
    case DspAssetKind::Ps3Correction:
        return validatePs3Correction(bytes);
    case DspAssetKind::Nr3Model:
        return validateNr3Model(bytes);
    }
    return rejected(kind, bytes, QStringLiteral("Unknown model type."));
}

DspAssetValidationResult DspAssetValidation::validateNr3Model(const QByteArray& bytes)
{
    const DspAssetKind kind = DspAssetKind::Nr3Model;
    if (bytes.size() > kMaxNr3ModelBytes) {
        return rejected(kind, bytes, QStringLiteral("The NR3 model is larger than 16 MiB."));
    }
    // Every rnnoise weight file is a run of 64-byte-headed records, and the
    // first record header starts with the "DNNw" tag (rnnoise nnet.h
    // WeightHead). Checking it first gives a clear answer for an ordinary
    // file picked by mistake before rnnoise sees the bytes at all.
    constexpr qsizetype kRecordHeaderBytes = 64;
    if (bytes.size() < kRecordHeaderBytes || !bytes.startsWith("DNNw")) {
        return rejected(kind, bytes, QStringLiteral("This file is not an NR3 model."));
    }
#ifdef HAVE_WDSP
    // rnnoise_model_from_buffer borrows the bytes; they outlive both calls.
    RNNModel* model = rnnoise_model_from_buffer(bytes.constData(), int(bytes.size()));
    if (!model) {
        return rejected(kind, bytes, QStringLiteral("There was not enough memory to check the NR3 model."));
    }
    // rnnoise_create parses every record with bounds checks and then checks
    // each layer's name and size against the layer sizes NR3 is built with;
    // it returns NULL when any of that fails.
    DenoiseState* trial = rnnoise_create(model);
    const bool usable = trial != nullptr;
    if (trial) {
        rnnoise_destroy(trial);
    }
    rnnoise_model_free(model);
    if (!usable) {
        return rejected(kind, bytes,
                        QStringLiteral("This file is not an NR3 model this Core can use."));
    }

    const quint32 firstVersion = readU32(bytes, 4);

    DspAssetValidationResult result;
    result.accepted = true;
    result.kind = kind;
    result.format = QStringLiteral("RNNoise");
    result.version = int(std::min<quint32>(firstVersion, quint32(std::numeric_limits<int>::max())));
    result.compatibility = QStringLiteral("NR3");
    result.size = bytes.size();
    result.hashHex = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    return result;
#else
    return rejected(kind, bytes, QStringLiteral("This Core was built without NR3."));
#endif
}

DspAssetValidationResult DspAssetValidation::validateNnrModel(const QByteArray& bytes)
{
    const DspAssetKind kind = DspAssetKind::NnrModel;
    if (bytes.size() > kMaxNnrModelBytes) {
        return rejected(kind, bytes, QStringLiteral("NNR model exceeds the 64 MiB limit"));
    }
    if (bytes.size() < kNnioHeaderBytes) {
        return rejected(kind, bytes, QStringLiteral("Truncated WDSPNN header"));
    }
    if (bytes.first(8) != QByteArray("WDSPNN\0\0", 8)) {
        return rejected(kind, bytes, QStringLiteral("Invalid WDSPNN magic header"));
    }

    const quint32 version = readU32(bytes, 8);
    const quint32 tensorCount = readU32(bytes, 12);
    const quint64 dataOffset = readU64(bytes, 16);
    const quint64 dataBytes = readU64(bytes, 24);
    if (version != 1) {
        return rejected(kind, bytes,
                        QStringLiteral("Unsupported WDSPNN version %1; expected version 1")
                            .arg(version));
    }
    if (tensorCount != kNnioTensorCount) {
        return rejected(kind, bytes,
                        QStringLiteral("WDSP NNR architecture requires exactly 57 tensors"));
    }

    quint64 descriptorBytes = 0;
    quint64 descriptorEnd = 0;
    quint64 dataEnd = 0;
    if (!checkedMultiply(tensorCount, quint64(kNnioDescriptorBytes), &descriptorBytes)
        || !checkedAdd(kNnioHeaderBytes, descriptorBytes, &descriptorEnd)
        || !checkedAdd(dataOffset, dataBytes, &dataEnd)) {
        return rejected(kind, bytes, QStringLiteral("WDSPNN header extent overflow"));
    }
    if (dataOffset < descriptorEnd || descriptorEnd > quint64(bytes.size())) {
        return rejected(kind, bytes, QStringLiteral("WDSPNN descriptor/data extents conflict"));
    }
    if (dataEnd > quint64(bytes.size())) {
        return rejected(kind, bytes, QStringLiteral("Truncated WDSPNN tensor data"));
    }

    QVector<TensorDescriptor> tensors;
    tensors.reserve(int(tensorCount));
    QSet<QString> names;
    quint64 decodedBytes = 0;
    bool hasF32 = false;
    bool hasF64 = false;

    for (quint32 i = 0; i < tensorCount; ++i) {
        const qsizetype descriptor = kNnioHeaderBytes + qsizetype(i) * kNnioDescriptorBytes;
        const QByteArray nameField = bytes.mid(descriptor, kNnioNameBytes);
        const qsizetype nul = nameField.indexOf('\0');
        if (nul <= 0) {
            return rejected(kind, bytes,
                            QStringLiteral("WDSPNN tensor %1 has an invalid name").arg(i));
        }
        TensorDescriptor tensor;
        tensor.name = QString::fromLatin1(nameField.constData(), nul);
        if (names.contains(tensor.name)) {
            return rejected(kind, bytes,
                            QStringLiteral("Duplicate WDSPNN tensor '%1'").arg(tensor.name));
        }
        names.insert(tensor.name);

        tensor.dtype = readU32(bytes, descriptor + 40);
        const quint32 rank = readU32(bytes, descriptor + 44);
        if (tensor.dtype > 1) {
            return rejected(kind, bytes,
                            QStringLiteral("Tensor '%1' has unsupported dtype").arg(tensor.name));
        }
        if (rank < 1 || rank > 4) {
            return rejected(kind, bytes,
                            QStringLiteral("Tensor '%1' rank must be 1..4").arg(tensor.name));
        }

        quint64 elements = 1;
        for (quint32 dimension = 0; dimension < rank; ++dimension) {
            const quint32 extent = readU32(bytes, descriptor + 48 + 4 * dimension);
            if (extent == 0 || !checkedMultiply(elements, extent, &elements)) {
                return rejected(kind, bytes,
                                QStringLiteral("Tensor '%1' extent overflow").arg(tensor.name));
            }
            tensor.shape.append(extent);
        }
        tensor.elements = elements;
        tensor.offset = readU32(bytes, descriptor + 64);
        tensor.bytes = readU32(bytes, descriptor + 68);

        quint64 expectedBytes = 0;
        const quint64 encodedWidth = tensor.dtype == 0 ? 4 : 8;
        quint64 tensorEnd = 0;
        quint64 decodedTensorBytes = 0;
        if (!checkedMultiply(elements, encodedWidth, &expectedBytes)
            || expectedBytes != tensor.bytes
            || !checkedAdd(tensor.offset, tensor.bytes, &tensorEnd)
            || tensorEnd > dataBytes
            || !checkedMultiply(elements, 8, &decodedTensorBytes)
            || !checkedAdd(decodedBytes, decodedTensorBytes, &decodedBytes)
            || decodedBytes > kMaxDecodedModelBytes) {
            return rejected(kind, bytes,
                            QStringLiteral("Tensor '%1' has invalid or overflowing extents")
                                .arg(tensor.name));
        }
        hasF32 |= tensor.dtype == 0;
        hasF64 |= tensor.dtype == 1;
        tensors.append(tensor);
    }

    QVector<int> intervalOrder(tensors.size());
    for (int i = 0; i < intervalOrder.size(); ++i) {
        intervalOrder[i] = i;
    }
    std::sort(intervalOrder.begin(), intervalOrder.end(), [&tensors](int a, int b) {
        return tensors[a].offset < tensors[b].offset;
    });
    quint64 previousEnd = 0;
    bool first = true;
    for (int index : intervalOrder) {
        const TensorDescriptor& tensor = tensors[index];
        if (!first && tensor.offset < previousEnd) {
            return rejected(kind, bytes,
                            QStringLiteral("WDSPNN tensor data ranges overlap at '%1'")
                                .arg(tensor.name));
        }
        previousEnd = tensor.offset + tensor.bytes;
        first = false;
    }

    QHash<QString, const TensorDescriptor*> byName;
    for (const TensorDescriptor& tensor : tensors) {
        byName.insert(tensor.name, &tensor);
        for (quint64 element = 0; element < tensor.elements; ++element) {
            if (!std::isfinite(tensorValue(bytes, dataOffset, tensor, element))) {
                return rejected(kind, bytes,
                                QStringLiteral("Tensor '%1' contains a non-finite weight")
                                    .arg(tensor.name));
            }
        }
    }

    const TensorDescriptor* cfgTensor = byName.value(QStringLiteral("cfg"), nullptr);
    if (!cfgTensor || cfgTensor->shape != QVector<quint32>{7}) {
        return rejected(kind, bytes, QStringLiteral("Tensor 'cfg' has the wrong shape"));
    }
    QVector<quint32> cfg;
    for (quint64 i = 0; i < 7; ++i) {
        const double value = tensorValue(bytes, dataOffset, *cfgTensor, i);
        if (value < 0 || value > std::numeric_limits<quint32>::max()
            || std::floor(value) != value) {
            return rejected(kind, bytes, QStringLiteral("Tensor 'cfg' contains invalid values"));
        }
        cfg.append(quint32(value));
    }

    const QVector<quint32> model0{257, 16, 32, 48, 64, 32, 5};
    const QVector<quint32> model1{257, 24, 48, 72, 96, 48, 5};
    QString compatibility;
    if (cfg == model0) {
        compatibility = QStringLiteral("wdsp-nnr-model-0");
    } else if (cfg == model1) {
        compatibility = QStringLiteral("wdsp-nnr-model-1");
    } else {
        return rejected(kind, bytes,
                        QStringLiteral("WDSPNN cfg does not match a supported model architecture"));
    }

    const QHash<QString, QVector<quint32>> expected = expectedNnrTensors(cfg);
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        const TensorDescriptor* tensor = byName.value(it.key(), nullptr);
        if (!tensor) {
            return rejected(kind, bytes,
                            QStringLiteral("Required tensor '%1' is missing").arg(it.key()));
        }
        if (tensor->shape != it.value()) {
            return rejected(kind, bytes,
                            QStringLiteral("Tensor '%1' has the wrong shape").arg(it.key()));
        }
    }
    if (byName.size() != expected.size()) {
        return rejected(kind, bytes, QStringLiteral("WDSPNN contains unknown tensors"));
    }

    const TensorDescriptor* lookahead = byName.value(QStringLiteral("lookahead"), nullptr);
    if (!lookahead || tensorValue(bytes, dataOffset, *lookahead, 0) != 1.0) {
        return rejected(kind, bytes,
                        QStringLiteral("WDSPNN lookahead is incompatible; expected one frame"));
    }

    DspAssetValidationResult result;
    result.accepted = true;
    result.kind = kind;
    result.format = QStringLiteral("WDSPNN");
    result.version = int(version);
    result.compatibility = compatibility;
    result.size = bytes.size();
    result.hashHex = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    result.tensorCount = int(tensorCount);
    result.numericEncoding = hasF32 && hasF64 ? QStringLiteral("mixed-f32-f64")
                                               : hasF32 ? QStringLiteral("f32")
                                                        : QStringLiteral("f64");
    result.decodedBytes = decodedBytes;
    return result;
}

DspAssetValidationResult DspAssetValidation::validatePs3Correction(const QByteArray& bytes)
{
    const DspAssetKind kind = DspAssetKind::Ps3Correction;
    if (bytes.size() > kMaxPs3CorrectionBytes) {
        return rejected(kind, bytes, QStringLiteral("PS3 correction exceeds the 1 MiB limit"));
    }
    if (bytes.isEmpty()) {
        return rejected(kind, bytes, QStringLiteral("PS3 correction file is empty"));
    }

    TokenCursor cursor(bytes);
    QString error;
    qint64 version = 0;
    if (!expectToken(cursor, "correction_file_version", &error)
        || !readInteger(cursor, &version, QStringLiteral("correction_file_version"), &error)) {
        return rejected(kind, bytes, error);
    }
    if (version == 1) {
        return rejected(kind, bytes,
                        QStringLiteral("Correction file version 1 (PS2) is unsupported; "
                                       "recalibrate to create a PS3 version 2 file"));
    }
    if (version != 2) {
        return rejected(kind, bytes,
                        QStringLiteral("Unsupported correction file version %1; expected PS3 version 2")
                            .arg(version));
    }

    const QByteArray curveNames[] = {"MAG", "COS", "SIN"};
    double checksum = 0;
    int branchCount = 0;
    int pointCount = 0;
    for (const QByteArray& curveName : curveNames) {
        if (!expectToken(cursor, "curve", &error)
            || !expectToken(cursor, curveName, &error)
            || !readEma(cursor, &checksum, &error)
            || !expectToken(cursor, "n_branches", &error)) {
            return rejected(kind, bytes, error);
        }

        qint64 branches = 0;
        if (!readInteger(cursor, &branches, QStringLiteral("n_branches"), &error)) {
            return rejected(kind, bytes, error);
        }
        if (branches < 1 || branches > kMaxPs3BranchesPerCurve) {
            return rejected(kind, bytes,
                            QStringLiteral("PS3 branch count must be in the range 1..16"));
        }
        checksum += double(branches);
        branchCount += int(branches);
        int curvePoints = 0;

        for (int branch = 0; branch < branches; ++branch) {
            qint64 branchIndex = 0;
            qint64 points = 0;
            double midpoint = 0;
            if (!expectToken(cursor, "branch", &error)
                || !readInteger(cursor, &branchIndex, QStringLiteral("branch index"), &error)
                || !expectToken(cursor, "n_pts", &error)
                || !readInteger(cursor, &points, QStringLiteral("branch point count"), &error)
                || !expectToken(cursor, "t_mid", &error)
                || !readFinite(cursor, &midpoint, QStringLiteral("branch midpoint"), &error)) {
                return rejected(kind, bytes, error);
            }
            if (branchIndex != branch) {
                return rejected(kind, bytes,
                                QStringLiteral("PS3 branch index must match its ordered position"));
            }
            if (midpoint < 0.0 || midpoint > 1.0) {
                return rejected(kind, bytes,
                                QStringLiteral("PS3 branch midpoint must be normalized to 0..1"));
            }
            // The writer uses zero for a branch cleaned below four points;
            // every populated writer branch has at least four points.
            if (points != 0 && points < 4) {
                return rejected(kind, bytes,
                                QStringLiteral("PS3 populated branches require at least four points"));
            }
            if (points < 0 || points > kMaxPs3PointsPerCurve - curvePoints) {
                return rejected(kind, bytes,
                                QStringLiteral("PS3 curve point count exceeds the 1614-point bound"));
            }
            curvePoints += int(points);
            checksum += double(points) + midpoint;

            double previousX = 0;
            for (int point = 0; point < points; ++point) {
                double x = 0;
                double y = 0;
                if (!readFinite(cursor, &x, QStringLiteral("spline x coordinate"), &error)
                    || !readFinite(cursor, &y, QStringLiteral("spline y coordinate"), &error)) {
                    return rejected(kind, bytes, error);
                }
                if (point > 0 && x <= previousX) {
                    return rejected(kind, bytes,
                                    QStringLiteral("PS3 branch x coordinates must be strictly increasing"));
                }
                previousX = x;
                checksum += x + y;
            }
        }
        pointCount += curvePoints;
    }

    double storedChecksum = 0;
    if (!expectToken(cursor, "checksum", &error)
        || !readFinite(cursor, &storedChecksum, QStringLiteral("checksum"), &error)) {
        return rejected(kind, bytes, error);
    }
    const double tolerance = std::abs(checksum) * 1.0e-9 + 1.0e-9;
    if (std::abs(storedChecksum - checksum) > tolerance) {
        return rejected(kind, bytes, QStringLiteral("PS3 correction checksum mismatch"));
    }
    if (!cursor.atEnd()) {
        return rejected(kind, bytes, QStringLiteral("Unexpected trailing PS3 correction data"));
    }

    DspAssetValidationResult result;
    result.accepted = true;
    result.kind = kind;
    result.format = QStringLiteral("WDSP-PS3-CORRECTION");
    result.version = int(version);
    result.compatibility = QStringLiteral("wdsp-ps3-v2");
    result.size = bytes.size();
    result.hashHex = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    result.curveCount = 3;
    result.branchCount = branchCount;
    result.pointCount = pointCount;
    return result;
}

bool DspAssetValidation::validateEncodedPath(const QString& path, qsizetype capacityBytes,
                                             QString* error)
{
    const QByteArray encoded = path.toUtf8();
    if (path.contains(QChar('\0'))) {
        if (error) {
            *error = QStringLiteral("DSP asset path contains an embedded NUL");
        }
        return false;
    }
    if (capacityBytes < 1 || encoded.size() + 1 > capacityBytes) {
        if (error) {
            *error = QStringLiteral("DSP asset path needs %1 encoded bytes including the terminator; "
                                    "the WDSP buffer holds %2")
                         .arg(encoded.size() + 1)
                         .arg(capacityBytes);
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace NereusSDR
