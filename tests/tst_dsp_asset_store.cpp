// no-port-check: tests for the NereusSDR-owned station DSP asset store.

#include <QtTest/QtTest>

#include "core/dsp/DspAssetStore.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <cstring>

using namespace NereusSDR;

class TestDspAssetStore : public QObject
{
    Q_OBJECT

    QByteArray validCorrection() const
    {
        const QString path = QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt");
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    struct PsCorrectionFixture {
        bool accepted{false};
    };
    PsCorrectionFixture importCorrection(DspAssetStore& store) const
    {
        return {store.importBytes(DspAssetKind::Ps3Correction, QStringLiteral("correction"),
                                  validCorrection(), QStringLiteral("radio:alpha")).accepted};
    }

    // The bundled rnnoise model the desktop app and nereusd both ship.
    QByteArray bundledSmallNr3Model() const
    {
        QFile file(QFINDTESTDATA("../third_party/rnnoise/models/Default_small.bin"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

private slots:
    void importsListsResolvesExportsAndRestarts()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QString error;
        DspAssetStore store(root.path());
        QVERIFY2(store.isValid(), qPrintable(store.lastError()));

        const QByteArray bytes = validCorrection();
        const DspAssetImportResult imported = store.importBytes(
            DspAssetKind::Ps3Correction, QStringLiteral("20 metre bench"), bytes,
            QStringLiteral("radio:001122334455"));
        QVERIFY2(imported.accepted, qPrintable(imported.error));
        QVERIFY(imported.record.id.startsWith(QStringLiteral("sha256:")));
        QCOMPARE(imported.record.hashHex.size(), 64);
        QCOMPARE(imported.record.size, qint64(bytes.size()));

        const QList<DspAssetRecord> records = store.assets();
        QCOMPARE(records.size(), 1);
        QVERIFY(records.first().valid);

        const QString path = store.resolvePath(imported.record.id, DspAssetKind::Ps3Correction,
                                               QStringLiteral("radio:001122334455"), &error);
        QVERIFY2(!path.isEmpty(), qPrintable(error));
        QVERIFY(QFileInfo(path).fileName().size() < 64);
        QVERIFY(!path.contains(imported.record.label));

        QBuffer exported;
        QVERIFY(exported.open(QIODevice::WriteOnly));
        QVERIFY2(store.exportAsset(imported.record.id, &exported, &error), qPrintable(error));
        QCOMPARE(exported.data(), bytes);

        DspAssetStore restarted(root.path());
        QVERIFY2(restarted.isValid(), qPrintable(restarted.lastError()));
        QCOMPARE(restarted.assets().size(), 1);
        QCOMPARE(restarted.assets().first().id, imported.record.id);
    }

    void refusesIdentityMismatchTraversalAndTampering()
    {
        QTemporaryDir root;
        DspAssetStore store(root.path());
        const DspAssetImportResult imported = store.importBytes(
            DspAssetKind::Ps3Correction, QStringLiteral("fixture"), validCorrection(),
            QStringLiteral("radio:alpha"));
        QVERIFY(imported.accepted);

        QString error;
        QVERIFY(store.resolvePath(imported.record.id, DspAssetKind::Ps3Correction,
                                  QStringLiteral("radio:beta"), &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("radio"), Qt::CaseInsensitive));

        QVERIFY(store.resolvePath(QStringLiteral("../../etc/passwd"),
                                  DspAssetKind::Ps3Correction, {}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("asset ID"), Qt::CaseInsensitive));

        const QString path = store.resolvePath(imported.record.id, DspAssetKind::Ps3Correction,
                                               QStringLiteral("radio:alpha"), &error);
        QVERIFY(!path.isEmpty());
        QFile tamper(path);
        QVERIFY(tamper.open(QIODevice::Append));
        QCOMPARE(tamper.write("x", 1), qint64(1));
        tamper.close();
        QVERIFY(store.resolvePath(imported.record.id, DspAssetKind::Ps3Correction,
                                  QStringLiteral("radio:alpha"), &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("size"), Qt::CaseInsensitive));

        // A same-size mutation reaches the independent SHA-256 invariant.
        QVERIFY(tamper.open(QIODevice::ReadWrite));
        QVERIFY(tamper.resize(imported.record.size));
        QVERIFY(tamper.seek(imported.record.size / 2));
        const QByteArray original = tamper.read(1);
        QCOMPARE(original.size(), 1);
        QVERIFY(tamper.seek(imported.record.size / 2));
        const char changed = static_cast<char>(original.at(0) ^ 0x01);
        QCOMPARE(tamper.write(&changed, 1), qint64(1));
        tamper.close();
        QVERIFY(store.resolvePath(imported.record.id, DspAssetKind::Ps3Correction,
                                  QStringLiteral("radio:alpha"), &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("hash"), Qt::CaseInsensitive));
    }

    void failedImportLeavesExistingAssetAndNoTemporaryPublication()
    {
        QTemporaryDir root;
        DspAssetStore store(root.path());
        const DspAssetImportResult good = store.importBytes(
            DspAssetKind::Ps3Correction, QStringLiteral("good"), validCorrection());
        QVERIFY(good.accepted);

        const DspAssetImportResult bad = store.importBytes(
            DspAssetKind::Ps3Correction, QStringLiteral("bad"), QByteArray("not a correction"));
        QVERIFY(!bad.accepted);
        QCOMPARE(store.assets().size(), 1);

        const QDir assetsDir(root.path() + QStringLiteral("/dsp-assets/assets"));
        const QStringList residues = assetsDir.entryList(
            {QStringLiteral("*.tmp"), QStringLiteral(".*.tmp"), QStringLiteral("*.part")},
            QDir::Files | QDir::Hidden);
        QVERIFY2(residues.isEmpty(), qPrintable(residues.join(',')));
    }

    void stagedImportEnforcesChunkLimitAndCleansInterruption()
    {
        QTemporaryDir root;
        QString error;
        {
            DspAssetStore store(root.path());
            const QString token = store.beginImport(DspAssetKind::Ps3Correction,
                                                    QStringLiteral("staged"), {}, &error);
            QVERIFY2(!token.isEmpty(), qPrintable(error));
            QVERIFY(!store.appendImport(token, QByteArray(DspAssetStore::kTransferChunkBytes + 1, 'x'),
                                        &error));
            QVERIFY(error.contains(QStringLiteral("64 KiB"), Qt::CaseInsensitive));

            const QString interrupted = store.beginImport(DspAssetKind::Ps3Correction,
                                                          QStringLiteral("interrupted"), {}, &error);
            QVERIFY(!interrupted.isEmpty());
            QVERIFY(store.appendImport(interrupted, QByteArray("partial"), &error));
        }

        DspAssetStore restarted(root.path());
        QVERIFY(restarted.isValid());
        const QDir staging(root.path() + QStringLiteral("/dsp-assets/staging"));
        QCOMPARE(staging.entryList({QStringLiteral("*.part")}, QDir::Files | QDir::Hidden).size(), 0);
    }

    // R-R3-21: NR3 models are a third asset kind with the same import,
    // list, resolve, export and restart path as the other two, and the
    // trial load accepts the bundled model.
    void nr3ModelImportsListsResolvesExportsAndRestarts()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QByteArray bytes = bundledSmallNr3Model();
        QVERIFY(!bytes.isEmpty());
        QVERIFY(bytes.size() <= DspAssetValidation::kMaxNr3ModelBytes);
        QCOMPARE(DspAssetValidation::sizeLimit(DspAssetKind::Nr3Model),
                 qint64(16) * 1024 * 1024);

        DspAssetStore store(root.path());
        QVERIFY2(store.isValid(), qPrintable(store.lastError()));
        const DspAssetImportResult imported = store.importBytes(
            DspAssetKind::Nr3Model, QStringLiteral("Small voice"), bytes);
        QVERIFY2(imported.accepted, qPrintable(imported.error));
        QCOMPARE(imported.record.kind, DspAssetKind::Nr3Model);
        QCOMPARE(imported.record.format, QStringLiteral("RNNoise"));
        QCOMPARE(static_cast<int>(imported.record.kind), 2);
        QCOMPARE(store.label(imported.record.id), QStringLiteral("Small voice"));

        QString error;
        QVERIFY(store.resolvePath(imported.record.id, DspAssetKind::NnrModel, {}, &error).isEmpty());
        const QString path = store.resolvePath(imported.record.id, DspAssetKind::Nr3Model, {}, &error);
        QVERIFY2(!path.isEmpty(), qPrintable(error));
        QVERIFY(path.endsWith(QStringLiteral(".rnn")));

        QBuffer exported;
        QVERIFY(exported.open(QIODevice::WriteOnly));
        QVERIFY2(store.exportAsset(imported.record.id, &exported, &error), qPrintable(error));
        QCOMPARE(exported.data(), bytes);

        // Its own manifest array: a Core built before NR3 models reads only
        // "assets" and would reject the whole manifest on an unknown kind.
        const PsCorrectionFixture correction = importCorrection(store);
        QVERIFY(correction.accepted);
        QFile manifest(root.path() + QStringLiteral("/dsp-assets/assets.json"));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const QJsonObject object = QJsonDocument::fromJson(manifest.readAll()).object();
        const QJsonArray shared = object.value(QStringLiteral("assets")).toArray();
        const QJsonArray nr3 = object.value(QStringLiteral("nr3Assets")).toArray();
        QCOMPARE(shared.size(), 1);
        QCOMPARE(shared.first().toObject().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("ps3-correction"));
        QCOMPARE(nr3.size(), 1);
        QCOMPARE(nr3.first().toObject().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("nr3-model"));

        DspAssetStore restarted(root.path());
        QVERIFY2(restarted.isValid(), qPrintable(restarted.lastError()));
        QCOMPARE(restarted.assets().size(), 2);
        QVERIFY(!restarted.resolvePath(imported.record.id, DspAssetKind::Nr3Model, {}, &error)
                     .isEmpty());
    }

    void nr3ModelTrialLoadRejectsJunkWithPlainReasons()
    {
        const QByteArray model = bundledSmallNr3Model();
        QVERIFY(!model.isEmpty());
        QVERIFY(DspAssetValidation::validateNr3Model(model).accepted);

        const auto text = DspAssetValidation::validateNr3Model(
            QByteArrayLiteral("This is a text file, not a model at all, honestly.............."));
        QVERIFY(!text.accepted);
        QCOMPARE(text.error, QStringLiteral("This file is not an NR3 model."));

        // Right tag, wrong contents: only the trial load can tell.
        QByteArray tagged(4096, '\x5a');
        tagged.replace(0, 4, "DNNw");
        const auto junk = DspAssetValidation::validateNr3Model(tagged);
        QVERIFY(!junk.accepted);
        QCOMPARE(junk.error, QStringLiteral("This file is not an NR3 model this Core can use."));

        const auto truncated = DspAssetValidation::validateNr3Model(model.first(model.size() / 2));
        QVERIFY(!truncated.accepted);
        QCOMPARE(truncated.error, QStringLiteral("This file is not an NR3 model this Core can use."));

        QByteArray oversized(DspAssetValidation::kMaxNr3ModelBytes + 1, '\0');
        oversized.replace(0, 4, "DNNw");
        const auto large = DspAssetValidation::validateNr3Model(oversized);
        QVERIFY(!large.accepted);
        QCOMPARE(large.error, QStringLiteral("The NR3 model is larger than 16 MiB."));

        QTemporaryDir root;
        DspAssetStore store(root.path());
        QVERIFY(!store.importBytes(DspAssetKind::Nr3Model, QStringLiteral("junk"), tagged).accepted);
        const DspAssetImportResult scoped = store.importBytes(
            DspAssetKind::Nr3Model, QStringLiteral("scoped"), model, QStringLiteral("radio:alpha"));
        QVERIFY(!scoped.accepted);
        QCOMPARE(scoped.error, QStringLiteral("NR3 models belong to the Core, not to one radio."));
        QCOMPARE(store.assets().size(), 0);
    }

    // The Rock's nereusd died here at startup. rnnoise_model_from_buffer
    // never sets the model's FILE* and rnnoise_model_free fcloses it when it
    // is not null. glibc hands the block of that size freed last straight
    // back without clearing it (macOS clears freed blocks, so the Mac never
    // saw it), so leave a dirty block of the model's size (32 bytes on
    // 64-bit) just before each trial load.
    void nr3TrialLoadIgnoresStaleHeapBytes()
    {
        const QByteArray model = bundledSmallNr3Model();
        QVERIFY(!model.isEmpty());
        for (int round = 0; round < 4; ++round) {
            void* volatile dirty = std::malloc(32);
            QVERIFY(dirty != nullptr);
            std::memset(dirty, 0xa5, 32);
            std::free(dirty);
            QVERIFY(DspAssetValidation::validateNr3Model(model).accepted);
        }
    }

    void validatesFixedWdspPathCapacitiesInEncodedBytes()
    {
        QString error;
        QVERIFY(DspAssetValidation::validateEncodedPath(QString(510, QLatin1Char('a')), 512,
                                                        &error));
        QVERIFY(!DspAssetValidation::validateEncodedPath(QString(512, QLatin1Char('a')), 512,
                                                         &error));
        QVERIFY(error.contains(QStringLiteral("terminator"), Qt::CaseInsensitive));

        // UTF-8 bytes, rather than UTF-16 code units, are the upstream C-buffer
        // boundary. U+00E9 encodes as two bytes.
        QVERIFY(!DspAssetValidation::validateEncodedPath(QString(128, QChar(0x00e9)), 256,
                                                         &error));
    }
};

QTEST_GUILESS_MAIN(TestDspAssetStore)
#include "tst_dsp_asset_store.moc"
