// Issue #241 — settings persistence crash-safety + corruption recovery.
//
// Verifies the four invariants added in the issue #241 fix:
//
//   1. save() rotates the previous good main file to "<file>.bak" via a
//      .bak.tmp staging step before overwriting main.
//   2. save() writes the new main file atomically (via QSaveFile) so the
//      destination is never observed as a partial overlay — the failure
//      mode reported in #241 (NTFS journal rollback zeroing the leading
//      28 × 4 KB sectors of the file) cannot recur on a clean shutdown
//      path.
//   3. load() detects a corrupt main file (leading-NUL bytes from a
//      journal rollback OR an XML parse failure) and:
//        a) renames it to "<file>.corrupt-YYYYMMDD-HHMMSS",
//        b) restores from "<file>.bak" if a clean parse succeeds,
//        c) otherwise falls through to defaults (m_settings empty).
//   4. After successful recovery from .bak the diagnostic accessors
//      (wasCorruptedOnLoad / preservedCorruptFilePath / recoveredFromBackup)
//      reflect what happened so a future MainWindow notification can show
//      the user the preserved-file path.
//
// Uses QTemporaryDir + AppSettings(filePath) — no singleton, no real
// user-config writes.  TestSandboxInit.cpp guards the singleton path even
// if a stray AppSettings::instance() call slips into a future test.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {

// Writes raw bytes to a path, creating the parent directory if needed.
// Returns true on success. Used to seed corrupt files for the recovery
// scenarios.
bool writeRawBytes(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const qint64 n = f.write(bytes);
    f.close();
    return n == bytes.size();
}

// Returns true iff path exists and starts with NUL byte(s) — the canonical
// shape produced by an NTFS journal rollback.
bool startsWithNul(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(1);
    return head.size() == 1 && head.at(0) == '\0';
}

} // namespace

class TstAppSettingsCorruption : public QObject {
    Q_OBJECT

private slots:
    // ─────────────────────────────────────────────────────────────────────
    // .bak rotation: a second save() copies the prior main file to .bak
    // ─────────────────────────────────────────────────────────────────────
    void bakIsRotatedOnSecondSave()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // First save — only main file should exist (no prior good state to rotate).
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("Marker"), QStringLiteral("v1"));
            s.save();
        }
        QVERIFY(QFileInfo::exists(path));
        QVERIFY2(!QFileInfo::exists(path + QStringLiteral(".bak")),
                 ".bak should not exist after the first save() (no prior good state to rotate)");

        // Second save — main becomes v2 and .bak should hold v1.
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("Marker")).toString(), QStringLiteral("v1"));
            s.setValue(QStringLiteral("Marker"), QStringLiteral("v2"));
            s.save();
        }
        QVERIFY2(QFileInfo::exists(path + QStringLiteral(".bak")),
                 "Second save() should produce a .bak holding the previous good state");

        // Verify .bak content is v1 by loading it directly.
        AppSettings bak(path + QStringLiteral(".bak"));
        bak.load();
        QCOMPARE(bak.value(QStringLiteral("Marker")).toString(), QStringLiteral("v1"));

        // And main is v2.
        AppSettings main(path);
        main.load();
        QCOMPARE(main.value(QStringLiteral("Marker")).toString(), QStringLiteral("v2"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // .bak.tmp staging: no .bak.tmp residue after a normal save()
    // ─────────────────────────────────────────────────────────────────────
    void bakTmpIsCleanedUp()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings s(path);
        s.setValue(QStringLiteral("Marker"), QStringLiteral("v1"));
        s.save();
        s.setValue(QStringLiteral("Marker"), QStringLiteral("v2"));
        s.save();

        QVERIFY2(!QFileInfo::exists(path + QStringLiteral(".bak.tmp")),
                 ".bak.tmp should not be present after a clean save() — it must be renamed to .bak");
    }

    // ─────────────────────────────────────────────────────────────────────
    // Issue #241 main scenario: NTFS-style leading-NUL corruption.
    // Main is preserved as .corrupt-* and settings restored from .bak.
    // ─────────────────────────────────────────────────────────────────────
    void leadingNulCorruptionIsRecoveredFromBak()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Save three times. .bak is rotated to "main-before-this-save" on
        // each save() that finds an existing main file, so:
        //   after save#1: main=v1, no .bak yet
        //   after save#2: main=v2, .bak=v1
        //   after save#3: main=v3, .bak=v2
        // Corrupting main here loses v3 (main was the only place it lived);
        // .bak still holds v2, which is the best possible recovery point.
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("UserPref"), QStringLiteral("v1"));
            s.save();
            s.setValue(QStringLiteral("UserPref"), QStringLiteral("v2"));
            s.save();
            s.setValue(QStringLiteral("UserPref"), QStringLiteral("v3"));
            s.save();
        }
        QVERIFY(QFileInfo::exists(path + QStringLiteral(".bak")));

        // Simulate an NTFS journal rollback over an in-flight write: the
        // first 114,688 bytes (28 × 4 KB) of the main file are zeroed, but
        // the file itself is not deleted. (Issue #241 reported exactly this
        // shape — the actual user file was 608,457 bytes with 114,688
        // leading NULs; we use a smaller buffer here for test speed.)
        QByteArray corrupted(8192, '\0');
        corrupted.append("|0.0|0.0|trailing-garbage|");
        QVERIFY(writeRawBytes(path, corrupted));
        QVERIFY(startsWithNul(path));

        // Load — should detect corruption, preserve as .corrupt-*, restore
        // from .bak with the previous good values intact.
        AppSettings recovered(path);
        recovered.load();

        QVERIFY2(recovered.wasCorruptedOnLoad(),
                 "load() must flag the leading-NUL file as corrupt");
        QVERIFY2(recovered.recoveredFromBackup(),
                 "load() must restore from .bak when main is corrupt and .bak is good");

        const QString preserved = recovered.preservedCorruptFilePath();
        QVERIFY2(!preserved.isEmpty(), "preservedCorruptFilePath() must report the rename target");
        QVERIFY2(preserved.contains(QStringLiteral(".corrupt-")),
                 "Preserved file must use .corrupt-YYYYMMDD-HHMMSS suffix");
        QVERIFY2(QFileInfo::exists(preserved),
                 "Preserved corrupt file must remain on disk for forensic inspection");
        QVERIFY2(!QFileInfo::exists(path) || QFileInfo(path).size() == 0,
                 "Original main path should be moved (or empty) after corrupt-preserve rename");

        // Recovered settings come from .bak, which holds the previous-to-last
        // good save (v2). The most recent save (v3) was in main and is lost
        // along with the corruption — that data only ever existed in one
        // place. .bak is one save behind by design (standard rotation
        // pattern; symmetric to atomic-rename limitations).
        QCOMPARE(recovered.value(QStringLiteral("UserPref")).toString(),
                 QStringLiteral("v2"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Mid-stream XML parse failure: also routes through the .bak path.
    // ─────────────────────────────────────────────────────────────────────
    void midStreamXmlErrorIsRecoveredFromBak()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Save three times so .bak holds a populated previous-good state.
        // (See bakIsRotatedOnSecondSave / leadingNulCorruptionIsRecoveredFromBak
        // for the rotation timing — .bak is one save behind main.)
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("Marker"), QStringLiteral("first"));
            s.save();
            s.setValue(QStringLiteral("Marker"), QStringLiteral("second"));
            s.save();
            s.setValue(QStringLiteral("Marker"), QStringLiteral("third"));
            s.save();
        }

        // Truncate the main file mid-element so QXmlStreamReader trips
        // hasError(). The NUL-prefix check won't catch this — it's the
        // parse-error path, not the journal-rollback path.
        const QByteArray broken =
            QByteArrayLiteral("<?xml version=\"1.0\"?><NereusSDR><Marker>halfwa");
        QVERIFY(writeRawBytes(path, broken));

        AppSettings recovered(path);
        recovered.load();

        QVERIFY(recovered.wasCorruptedOnLoad());
        QVERIFY(recovered.recoveredFromBackup());
        // .bak holds "second" (one save behind the corrupted "third" main).
        QCOMPARE(recovered.value(QStringLiteral("Marker")).toString(),
                 QStringLiteral("second"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // No .bak available + corrupt main → defaults, but corrupt file
    // is still preserved so the user has a forensic copy.
    // ─────────────────────────────────────────────────────────────────────
    void corruptMainWithNoBakFallsThroughToDefaults()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Seed a corrupt main file directly — no prior save(), so no .bak.
        QByteArray corrupted(4096, '\0');
        QVERIFY(writeRawBytes(path, corrupted));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".bak")));

        AppSettings recovered(path);
        recovered.load();

        QVERIFY(recovered.wasCorruptedOnLoad());
        QVERIFY2(!recovered.recoveredFromBackup(),
                 "recoveredFromBackup() must be false when no .bak exists");
        QVERIFY2(!recovered.preservedCorruptFilePath().isEmpty(),
                 "Corrupt file should still be preserved even without a .bak to recover from");
        QVERIFY(QFileInfo::exists(recovered.preservedCorruptFilePath()));

        // No settings carried over — but the next save() will write a fresh
        // file via QSaveFile and won't touch the preserved corrupt copy.
        QVERIFY2(recovered.allKeys().isEmpty(),
                 "load() with corrupt main + no .bak must leave the in-memory store empty");
    }

    // ─────────────────────────────────────────────────────────────────────
    // First-run path is NOT a corruption event (no diagnostics tripped).
    // ─────────────────────────────────────────────────────────────────────
    void missingFileIsNotTreatedAsCorruption()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings fresh(path);
        fresh.load();  // file does not exist

        QVERIFY2(!fresh.wasCorruptedOnLoad(),
                 "Missing settings file is the first-run path, not a corruption event");
        QVERIFY(fresh.preservedCorruptFilePath().isEmpty());
        QVERIFY(!fresh.recoveredFromBackup());
    }

    // ─────────────────────────────────────────────────────────────────────
    // Diagnostics reset between load() calls so they reflect THIS attempt.
    // ─────────────────────────────────────────────────────────────────────
    void diagnosticsResetBetweenLoads()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Build a good .bak then corrupt main.
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("Marker"), QStringLiteral("good"));
            s.save();
            s.setValue(QStringLiteral("Marker"), QStringLiteral("good2"));
            s.save();
        }
        QVERIFY(writeRawBytes(path, QByteArray(2048, '\0')));

        AppSettings inst(path);
        inst.load();
        QVERIFY(inst.wasCorruptedOnLoad());
        QVERIFY(inst.recoveredFromBackup());

        // Now save the recovered state (rewrites a clean main + rotates .bak)
        // and load again. This second load must NOT report the previous
        // corruption — the file is healthy now.
        inst.save();
        inst.load();
        QVERIFY2(!inst.wasCorruptedOnLoad(),
                 "wasCorruptedOnLoad() must reset between load() calls");
        QVERIFY(inst.preservedCorruptFilePath().isEmpty());
        QVERIFY(!inst.recoveredFromBackup());
    }

    // ─────────────────────────────────────────────────────────────────────
    // QSaveFile semantics: a .tmp side-file from QSaveFile must not leak
    // into the directory after a successful save (regression guard for
    // the atomic-write switchover).
    // ─────────────────────────────────────────────────────────────────────
    void noQSaveFileTmpResidueAfterSuccessfulSave()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings s(path);
        s.setValue(QStringLiteral("Marker"), QStringLiteral("once"));
        s.save();

        // QSaveFile picks a hidden temp name like "NereusSDR.settings.XXXXXX"
        // alongside the destination. After commit() these must all be gone.
        QDir d(QFileInfo(path).absolutePath());
        const QStringList residue = d.entryList(
            { QStringLiteral("NereusSDR.settings.??????") }, QDir::Files);
        QVERIFY2(residue.isEmpty(),
                 qPrintable(QStringLiteral("QSaveFile residue left behind: ")
                            + residue.join(QLatin1Char(','))));
    }
};

QTEST_APPLESS_MAIN(TstAppSettingsCorruption)
#include "tst_app_settings_corruption.moc"
