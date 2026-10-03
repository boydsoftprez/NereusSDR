#pragma once
// =================================================================
// src/core/dsp/DspAssetValidation.h  (NereusSDR)
// =================================================================
// Bounded, non-actuating validators for station-owned WDSP assets.
// The source-format derivation and upstream licence notices are in the
// implementation file.
//
// Modification history (NereusSDR):
//   2026-09-21 — Created for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via OpenAI Codex.
//   2026-09-23 - NR3 (rnnoise) model kind added by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QString>

namespace NereusSDR {

// The integer values travel on the wire (dspAssets.beginImport "kind",
// dspAssets.list rows) and must never be renumbered.
enum class DspAssetKind {
    NnrModel = 0,
    Ps3Correction = 1,
    // R-R3-21: rnnoise weight files for NR3 (dspAssetVersion 2).
    Nr3Model = 2,
};

QString dspAssetKindName(DspAssetKind kind);
bool dspAssetKindFromName(const QString& name, DspAssetKind* kind);
// Wire integer to kind; false for any value this build does not know.
bool dspAssetKindFromInt(qint64 value, DspAssetKind* kind);

struct DspAssetValidationResult {
    bool accepted{false};
    DspAssetKind kind{DspAssetKind::NnrModel};
    QString error;
    QString format;
    int version{0};
    QString compatibility;
    qint64 size{0};
    QString hashHex;

    // Format-specific accepted metadata. Unused fields remain zero/empty.
    int tensorCount{0};
    QString numericEncoding;
    quint64 decodedBytes{0};
    int curveCount{0};
    int branchCount{0};
    int pointCount{0};
};

class DspAssetValidation final
{
public:
    static constexpr qint64 kMaxNnrModelBytes = 64LL * 1024 * 1024;
    static constexpr qint64 kMaxPs3CorrectionBytes = 1LL * 1024 * 1024;
    // The bundled Default_large.bin is 3.5 MB; 16 MiB leaves room for
    // larger community models without letting a file grow without bound.
    static constexpr qint64 kMaxNr3ModelBytes = 16LL * 1024 * 1024;
    static constexpr qint64 kTransferChunkBytes = 64LL * 1024;
    static constexpr int kMaxPs3BranchesPerCurve = 16;
    static constexpr int kMaxPs3PointsPerCurve = 1614;

    static DspAssetValidationResult validate(DspAssetKind kind, const QByteArray& bytes);
    static DspAssetValidationResult validateNnrModel(const QByteArray& bytes);
    static DspAssetValidationResult validatePs3Correction(const QByteArray& bytes);
    // Loads the bytes as an rnnoise model and builds one denoiser from it,
    // then frees both. Runs on the caller's thread (the Core's main thread),
    // never the audio thread, and never touches the live NR3 model.
    static DspAssetValidationResult validateNr3Model(const QByteArray& bytes);

    static qint64 sizeLimit(DspAssetKind kind);

    // capacityBytes includes the fixed WDSP C buffer's terminating NUL.
    static bool validateEncodedPath(const QString& path, qsizetype capacityBytes,
                                    QString* error = nullptr);

    // R-IOS-01: true for the validation and store messages written for the
    // operator (the NR3 checks), which the Core sends to an app as they
    // are. Every other message from this class and DspAssetStore is detail
    // for the log; DspAssetService sends a plain sentence in its place.
    static bool isOperatorMessage(const QString& message);
};

} // namespace NereusSDR
