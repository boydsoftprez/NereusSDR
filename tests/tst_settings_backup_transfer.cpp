#include <QtTest/QtTest>

#include "core/settings/SettingsBackupTransfer.h"

#include <limits>

using namespace NereusSDR;

class TstSettingsBackupTransfer : public QObject {
    Q_OBJECT
private slots:
    void maximumSnapshotRoundTrip()
    {
        QByteArray snapshot(SettingsBackupTransferSource::kMaxBytes, 'x');
        snapshot[0] = '<';
        snapshot[snapshot.size() - 1] = '>';
        SettingsBackupTransferSource source;
        QString error = QStringLiteral("stale");
        QVERIFY2(SettingsBackupTransferSource::create(snapshot, &source, &error), qPrintable(error));
        QVERIFY(error.isEmpty());
        const auto manifest = source.manifest();
        QCOMPARE(manifest.byteLength, qint64(snapshot.size()));
        QCOMPARE(manifest.sha256.size(), 32);
        SettingsBackupTransferAssembler assembled;
        QVERIFY2(assembled.begin(manifest, &error), qPrintable(error));
        QByteArray hidden = "unchanged";
        QVERIFY(!assembled.completedPayload(&hidden));
        QCOMPARE(hidden, QByteArray("unchanged"));
        for (qint64 offset = 0; offset < manifest.byteLength;) {
            QByteArray chunk;
            QVERIFY2(source.readChunk(offset, 200003, &chunk, &error), qPrintable(error));
            QVERIFY(!chunk.isEmpty());
            QVERIFY(chunk.size() <= SettingsBackupTransferSource::kMaxChunkBytes);
            QVERIFY2(assembled.acceptChunk(offset, chunk, &error), qPrintable(error));
            offset += chunk.size();
        }
        QByteArray result;
        QVERIFY2(assembled.completedPayload(&result, &error), qPrintable(error));
        QCOMPARE(result, snapshot);
        QCOMPARE(assembled.expectedOffset(), manifest.byteLength);
    }

    void borrowedSourceBytesAreSnapshotted()
    {
        char backing[] = {'a', 'b', 'c'};
        const QByteArray borrowed = QByteArray::fromRawData(backing, sizeof(backing));
        SettingsBackupTransferSource source;
        QVERIFY(SettingsBackupTransferSource::create(borrowed, &source));
        const auto manifest = source.manifest();
        backing[0] = 'z';
        backing[1] = 'z';
        QByteArray chunk;
        QVERIFY(source.readChunk(0, 3, &chunk));
        QCOMPARE(chunk, QByteArray("abc"));
        SettingsBackupTransferAssembler assembler;
        QVERIFY(assembler.begin(manifest));
        QVERIFY(assembler.acceptChunk(0, chunk));
        QByteArray completed;
        QVERIFY(assembler.completedPayload(&completed));
        QCOMPARE(completed, QByteArray("abc"));
    }

    void sourceRejectsInvalidRequestsWithoutReplacingOutput()
    {
        SettingsBackupTransferSource source;
        QString error;
        QVERIFY(!SettingsBackupTransferSource::create({}, &source, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!SettingsBackupTransferSource::create(
            QByteArray(SettingsBackupTransferSource::kMaxBytes + 1, 'x'), &source, &error));
        QVERIFY(SettingsBackupTransferSource::create("abc", &source, &error));
        QByteArray output = "keep";
        const qint64 huge = std::numeric_limits<qint64>::max();
        for (auto [offset, length] : {std::pair{-1LL, 1LL}, {0LL, 0LL},
                                      {3LL, 1LL}, {huge, huge},
                                      {0LL, huge}}) {
            QVERIFY(!source.readChunk(offset, length, &output, &error));
            QCOMPARE(output, QByteArray("keep"));
            QVERIFY(!error.isEmpty());
        }
        QVERIFY(!source.readChunk(0, 1, nullptr, &error));
        QVERIFY(source.readChunk(1, 200, &output, &error));
        QCOMPARE(output, QByteArray("bc"));
        QVERIFY(error.isEmpty());
        const auto savedManifest = source.manifest();
        QVERIFY(!SettingsBackupTransferSource::create({}, &source, &error));
        QCOMPARE(source.manifest().byteLength, savedManifest.byteLength);
        QCOMPARE(source.manifest().sha256, savedManifest.sha256);
    }

    void invalidBeginPreservesTransferAndOrdering()
    {
        SettingsBackupTransferSource source;
        QVERIFY(SettingsBackupTransferSource::create("abcdef", &source));
        SettingsBackupTransferAssembler assembler;
        QVERIFY(assembler.begin(source.manifest()));
        QVERIFY(assembler.acceptChunk(0, "ab"));
        QString error;
        for (const auto& manifest : {SettingsBackupTransferManifest{0, QByteArray(32, 'a')},
                                     {SettingsBackupTransferSource::kMaxBytes + 1, QByteArray(32, 'a')},
                                     {1, QByteArray(31, 'a')},
                                     {std::numeric_limits<qint64>::max(), QByteArray(32, 'a')}}) {
            QVERIFY(!assembler.begin(manifest, &error));
            QCOMPARE(assembler.expectedOffset(), qint64(2));
            QVERIFY(!error.isEmpty());
        }
        for (auto [offset, chunk] : {std::pair<qint64, QByteArray>{0, "ab"},
                                     {3, "cd"}, {2, {}}, {2, QByteArray(300000, 'x')},
                                     {std::numeric_limits<qint64>::max(), "c"}, {2, "cdefg"}}) {
            QVERIFY(!assembler.acceptChunk(offset, chunk, &error));
            QCOMPARE(assembler.expectedOffset(), qint64(2));
            QVERIFY(!error.isEmpty());
        }
        QVERIFY(assembler.acceptChunk(2, "cdef", &error));
        QByteArray result;
        QVERIFY(assembler.completedPayload(&result, &error));
        QCOMPARE(result, QByteArray("abcdef"));
        QVERIFY(!assembler.acceptChunk(6, "x", &error));
    }

    void checksumFailureClearsBytesAndAllowsRecovery()
    {
        SettingsBackupTransferSource source;
        QVERIFY(SettingsBackupTransferSource::create("abc", &source));
        SettingsBackupTransferAssembler assembler;
        QVERIFY(assembler.begin(source.manifest()));
        QString error;
        QVERIFY(!assembler.acceptChunk(0, "abd", &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(assembler.expectedOffset(), qint64(0));
        QByteArray output = "keep";
        QVERIFY(!assembler.completedPayload(&output, &error));
        QCOMPARE(output, QByteArray("keep"));
        QVERIFY(!assembler.acceptChunk(0, "abc"));
        QVERIFY(assembler.begin(source.manifest()));
        QVERIFY(assembler.acceptChunk(0, "abc"));
        QVERIFY(assembler.completedPayload(&output));
        QCOMPARE(output, QByteArray("abc"));
        assembler.cancel();
        QCOMPARE(assembler.expectedOffset(), qint64(0));
        QVERIFY(!assembler.completedPayload(&output));
        QCOMPARE(output, QByteArray("abc"));
    }

    void separateInstancesAndValidRestart()
    {
        SettingsBackupTransferSource one;
        SettingsBackupTransferSource two;
        QVERIFY(SettingsBackupTransferSource::create("one", &one));
        QVERIFY(SettingsBackupTransferSource::create("two", &two));
        SettingsBackupTransferAssembler first;
        SettingsBackupTransferAssembler second;
        QVERIFY(first.begin(one.manifest()));
        QVERIFY(second.begin(two.manifest()));
        QVERIFY(first.acceptChunk(0, "o"));
        first.cancel();
        QCOMPARE(first.expectedOffset(), qint64(0));
        QVERIFY(!first.acceptChunk(0, "one"));
        QVERIFY(first.begin(one.manifest()));
        QVERIFY(first.acceptChunk(0, "o"));
        QVERIFY(first.begin(two.manifest()));
        QCOMPARE(first.expectedOffset(), qint64(0));
        QVERIFY(first.acceptChunk(0, "two"));
        QVERIFY(second.acceptChunk(0, "two"));
        QByteArray a, b;
        QVERIFY(first.completedPayload(&a));
        QVERIFY(second.completedPayload(&b));
        QCOMPARE(a, QByteArray("two"));
        QCOMPARE(b, QByteArray("two"));
    }
};

QTEST_GUILESS_MAIN(TstSettingsBackupTransfer)
#include "tst_settings_backup_transfer.moc"
