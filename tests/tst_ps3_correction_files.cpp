// no-port-check: tests for the NereusSDR-owned bounded PS3 v2 validator.

#include <QtTest/QtTest>

#include "core/dsp/DspAssetValidation.h"

#include <QFile>

using namespace NereusSDR;

class TestPs3CorrectionFiles : public QObject
{
    Q_OBJECT

    QByteArray fixture() const
    {
        const QString path = QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt");
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

private slots:
    void acceptsSourceWriterFormat()
    {
        const DspAssetValidationResult result = DspAssetValidation::validatePs3Correction(fixture());
        QVERIFY2(result.accepted, qPrintable(result.error));
        QCOMPARE(result.kind, DspAssetKind::Ps3Correction);
        QCOMPARE(result.format, QStringLiteral("WDSP-PS3-CORRECTION"));
        QCOMPARE(result.version, 2);
        QCOMPARE(result.compatibility, QStringLiteral("wdsp-ps3-v2"));
        QCOMPARE(result.curveCount, 3);
        QCOMPARE(result.branchCount, 3);
        QCOMPARE(result.pointCount, 12);
    }

    void rejectsLegacyVersionUsefully()
    {
        QByteArray bytes = fixture();
        bytes.replace("correction_file_version 2", "correction_file_version 1");
        const DspAssetValidationResult result = DspAssetValidation::validatePs3Correction(bytes);
        QVERIFY(!result.accepted);
        QVERIFY(result.error.contains(QStringLiteral("version 1"), Qt::CaseInsensitive));
        QVERIFY(result.error.contains(QStringLiteral("recalibrate"), Qt::CaseInsensitive));
    }

    void rejectsWrongOrderKeysAndTrailingContent()
    {
        QByteArray bytes = fixture();
        bytes.replace("curve MAG", "curve COS");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("MAG"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("curve_ema_alpha ", "curve_ema_wrong ");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("curve_ema_alpha"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.append("unexpected trailing data\n");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("trailing"), Qt::CaseInsensitive));
    }

    void rejectsBoundsAndNonFiniteValuesBeforeAllocation()
    {
        QByteArray bytes = fixture();
        bytes.replace("curve_ema_pts 256", "curve_ema_pts 255");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("256"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("n_branches 1", "n_branches 17");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("branch"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("branch 0 n_pts 4", "branch 0 n_pts 1615");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("point"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("branch 0 n_pts 4 t_mid 0.5", "branch 0 n_pts 4 t_mid nan");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("finite"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("curve_ema_count 3", "curve_ema_count 2147483648");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("32-bit"), Qt::CaseInsensitive));
    }

    void rejectsBadBranchIndexOrderingAndChecksum()
    {
        QByteArray bytes = fixture();
        bytes.replace("branch 0 n_pts 4", "branch 1 n_pts 4");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("index"), Qt::CaseInsensitive));

        bytes = fixture();
        bytes.replace("checksum 833.25", "checksum 800");
        QVERIFY(DspAssetValidation::validatePs3Correction(bytes).error.contains(
            QStringLiteral("checksum"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(TestPs3CorrectionFiles)
#include "tst_ps3_correction_files.moc"
