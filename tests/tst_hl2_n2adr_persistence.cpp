// no-port-check: NereusSDR-original tests for hermes-filter-debug Bug 2.
//
// Covers:
//   * N2ADR filter "hl2IoBoard/n2adrFilter" round-trip via per-MAC
//     setHardwareValue/hardwareValue (the new write/read scope).
//   * Multi-MAC isolation (two HL2s have independent N2ADR settings).
//   * Legacy global → per-MAC migration (AppSettings::migrateLegacyN2adrFilter):
//     - global "True" + saved HL2 radio → per-MAC "True", global key removed.
//     - global key absent → migration is a no-op.
//     - non-HL2 saved radios are skipped (filter doesn't apply to them).
//     - explicit per-MAC value already present is preserved (defensive).
//     - migration is idempotent (calling twice is safe).
//
// Uses QTemporaryDir + direct AppSettings construction so each case operates
// on an isolated in-memory store. No singleton, no QApplication.
//
// hermes-filter-debug 2026-05-01 — JJ Boyd (KG4VCF), AI-assisted.

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

namespace {

// Build a saved-radio entry for the given MAC + board type. The other fields
// don't matter for these tests — we only filter on info.boardType in the
// migration helper.
void saveTestRadio(AppSettings& s, const QString& mac, HPSDRHW board)
{
    RadioInfo info;
    info.macAddress = mac;
    info.address    = QHostAddress(QStringLiteral("192.168.1.10"));
    info.port       = 1024;
    info.boardType  = board;
    info.protocol   = ProtocolVersion::Protocol1;
    info.name       = QStringLiteral("Test Radio");
    s.saveRadio(info, /*pinToMac=*/false, /*autoConnect=*/false);
}

}  // namespace

class TstHl2N2adrPersistence : public QObject {
    Q_OBJECT

private slots:
    // ─────────────────────────────────────────────────────────────────────
    // Round-trip: setHardwareValue → hardwareValue, same MAC, single radio.
    // ─────────────────────────────────────────────────────────────────────
    void perMacRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");

        // Default when nothing persisted.
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("False")).toString(),
                 QStringLiteral("False"));

        // Write-then-read in same instance.
        s.setHardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                           QStringLiteral("True"));
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("False")).toString(),
                 QStringLiteral("True"));

        // Save → reload → still True (the actual on-disk round-trip).
        s.save();
        AppSettings s2(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s2.load();
        QCOMPARE(s2.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                  QStringLiteral("False")).toString(),
                 QStringLiteral("True"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Two HL2s with different settings — they must NOT collide.
    // ─────────────────────────────────────────────────────────────────────
    void multiMacIsolation()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QString macA = QStringLiteral("aa:bb:cc:11:22:33");
        const QString macB = QStringLiteral("dd:ee:ff:44:55:66");

        s.setHardwareValue(macA, QStringLiteral("hl2IoBoard/n2adrFilter"),
                           QStringLiteral("True"));
        s.setHardwareValue(macB, QStringLiteral("hl2IoBoard/n2adrFilter"),
                           QStringLiteral("False"));

        QCOMPARE(s.hardwareValue(macA, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.hardwareValue(macB, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("False"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Legacy global key → per-MAC migration: the core happy path.
    // Reproduces the original buggy state: global "True" written by an old
    // pre-fix Hl2IoBoardTab plus a saved HL2 radio. After migration, the
    // per-MAC key holds "True" and the global key is gone.
    // ─────────────────────────────────────────────────────────────────────
    void legacyGlobalMigrationHl2()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);

        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("True"));
        QVERIFY(s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));

        AppSettings::migrateLegacyN2adrFilter(s);

        // Per-MAC has the migrated value.
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        // Global key is gone.
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }

    // Same as above but the legacy value was "False" — verifies migration
    // copies the actual value, not a hard-coded True.
    void legacyGlobalMigrationCarriesFalse()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("False"));

        AppSettings::migrateLegacyN2adrFilter(s);

        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("False"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Migration must NOT touch non-HL2 saved radios — chkHERCULES on
    // non-HL2 boards switches to "Apollo" semantics and the bit pattern
    // differs (mi0bot setup.cs:14369-14412 [@c26a8a4] non-HERMESLITE branch).
    // We don't propagate the legacy value to non-HL2 MACs.
    // ─────────────────────────────────────────────────────────────────────
    void legacyGlobalMigrationSkipsNonHl2()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QString hl2Mac  = QStringLiteral("aa:bb:cc:11:22:33");
        const QString ananMac = QStringLiteral("dd:ee:ff:44:55:66");
        saveTestRadio(s, hl2Mac,  HPSDRHW::HermesLite);
        saveTestRadio(s, ananMac, HPSDRHW::Orion);  // any non-HL2 board

        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("True"));

        AppSettings::migrateLegacyN2adrFilter(s);

        // HL2 picked it up.
        QCOMPARE(s.hardwareValue(hl2Mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        // ANAN did NOT pick it up — default returned.
        QCOMPARE(s.hardwareValue(ananMac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("DEFAULT")).toString(),
                 QStringLiteral("DEFAULT"));
        // Global is still removed (one-shot).
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // No global key → migration is a no-op (does not touch per-MAC values).
    // ─────────────────────────────────────────────────────────────────────
    void migrationNoGlobalKeyIsNoOp()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);

        // Pre-existing per-MAC value should be untouched.
        s.setHardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                           QStringLiteral("True"));

        AppSettings::migrateLegacyN2adrFilter(s);

        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Defensive: an explicit per-MAC value (from a user who's already on
    // the new schema) wins over the legacy global. We don't overwrite it.
    // ─────────────────────────────────────────────────────────────────────
    void migrationPreservesExplicitPerMac()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);

        // User flipped on per-MAC.  Legacy global says off.
        s.setHardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                           QStringLiteral("True"));
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("False"));

        AppSettings::migrateLegacyN2adrFilter(s);

        // Per-MAC kept its True.
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        // Global still cleared.
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Idempotent: calling twice doesn't break or duplicate anything.
    // ─────────────────────────────────────────────────────────────────────
    void migrationIdempotent()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("True"));

        AppSettings::migrateLegacyN2adrFilter(s);
        AppSettings::migrateLegacyN2adrFilter(s);  // 2nd call: no-op

        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Codex P1 (PR #160 review): preserve the legacy global key when there
    // are NO HL2 saved radios yet.  Without this, a user who enabled N2ADR
    // via the legacy code path but hasn't saved any HL2 (or only has a
    // manual-IP entry whose boardType is still Unknown until first probe)
    // would lose the value at migration — N2ADR comes back disabled after
    // upgrade.  Future migration runs (after they probe an HL2) should
    // still pick it up.
    // ─────────────────────────────────────────────────────────────────────
    void migrationPreservesGlobalWhenNoHl2Saved()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        // No saved HL2 — only a non-HL2 radio (counts as "not seen" for
        // migration purposes since boardType filter excludes it).
        saveTestRadio(s, QStringLiteral("dd:ee:ff:44:55:66"), HPSDRHW::Orion);
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("True"));

        AppSettings::migrateLegacyN2adrFilter(s);

        // Global key STILL present — preserved for a future migration run.
        QVERIFY2(s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")),
                 "Codex P1: legacy global N2ADR key was deleted with no "
                 "HL2 saved — value would be lost on upgrade");
        QCOMPARE(s.value(QStringLiteral("hl2IoBoard/n2adrFilter")).toString(),
                 QStringLiteral("True"));
    }

    // Migration with an Unknown-board manual entry (the manual-IP-before-probe
    // case): also preserves the global because boardType isn't yet HermesLite.
    void migrationPreservesGlobalForUnknownBoardManualEntry()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        saveTestRadio(s, QStringLiteral("manual-192.168.1.10-1024"),
                      HPSDRHW::Unknown);
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("False"));

        AppSettings::migrateLegacyN2adrFilter(s);

        QVERIFY(s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
        QCOMPARE(s.value(QStringLiteral("hl2IoBoard/n2adrFilter")).toString(),
                 QStringLiteral("False"));
    }

    // After an HL2 is later added, a subsequent migration run picks up the
    // preserved global and clears it.  This is the "next launch" recovery
    // path the P1 fix enables.
    void migrationOnNextLaunchPicksUpPreservedGlobal()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("hl2IoBoard/n2adrFilter"),
                   QStringLiteral("True"));

        // First run: no HL2 — global preserved.
        AppSettings::migrateLegacyN2adrFilter(s);
        QVERIFY(s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));

        // User adds an HL2 (e.g. probe completes and boardType is now known).
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        saveTestRadio(s, mac, HPSDRHW::HermesLite);

        // Second run: HL2 present — migration completes and global is gone.
        AppSettings::migrateLegacyN2adrFilter(s);
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("hl2IoBoard/n2adrFilter"),
                                 QStringLiteral("X")).toString(),
                 QStringLiteral("True"));
        QVERIFY(!s.contains(QStringLiteral("hl2IoBoard/n2adrFilter")));
    }
};

QTEST_APPLESS_MAIN(TstHl2N2adrPersistence)
#include "tst_hl2_n2adr_persistence.moc"
