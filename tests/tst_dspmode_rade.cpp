// no-port-check: NereusSDR-original unit-test file. No Thetis logic
// ported in this test file; DSPMode::RADE_U / RADE_L are
// NereusSDR-native extensions to the WDSP-derived DSPMode enum
// (RADE is not a WDSP mode).
// =================================================================
// tests/tst_dspmode_rade.cpp  (NereusSDR)
// =================================================================
//
// Phase 3R Task J1 unit tests: DSPMode::RADE_U and DSPMode::RADE_L
// enum entries round-trip through the SliceModel::modeName /
// modeFromName serialization helpers.
//
// Test cases (3):
//   1. modeName(RADE_U) returns the literal "RADE-U" and
//      modeFromName("RADE-U") returns DSPMode::RADE_U.
//   2. modeName(RADE_L) returns the literal "RADE-L" and
//      modeFromName("RADE-L") returns DSPMode::RADE_L.
//   3. Legacy migration: modeFromName("RADE") (singular, written by
//      pre-fix builds) returns DSPMode::RADE_U so existing persisted
//      per-MAC slice modes do not break on upgrade.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - New test file for Phase 3R Task J1.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-05-11 - Split RADE into RADE-U / RADE-L sidebands; added
//                 legacy migration test for the singular "RADE"
//                 string written by pre-fix builds.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/WdspTypes.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestDspModeRade : public QObject {
    Q_OBJECT
private slots:
    void serializesRadeUpperRoundTrip();
    void serializesRadeLowerRoundTrip();
    void legacySingularRadeMigratesToUpper();
};

void TestDspModeRade::serializesRadeUpperRoundTrip() {
    QCOMPARE(SliceModel::modeName(DSPMode::RADE_U), QStringLiteral("RADE-U"));
    QCOMPARE(SliceModel::modeFromName(QStringLiteral("RADE-U")),
             DSPMode::RADE_U);
}

void TestDspModeRade::serializesRadeLowerRoundTrip() {
    QCOMPARE(SliceModel::modeName(DSPMode::RADE_L), QStringLiteral("RADE-L"));
    QCOMPARE(SliceModel::modeFromName(QStringLiteral("RADE-L")),
             DSPMode::RADE_L);
}

void TestDspModeRade::legacySingularRadeMigratesToUpper() {
    // Pre-fix builds persisted the singular "RADE" string before the
    // sideband split landed. modeFromName must map it to RADE_U so
    // existing slice persistence does not break on upgrade.
    QCOMPARE(SliceModel::modeFromName(QStringLiteral("RADE")),
             DSPMode::RADE_U);
}

QTEST_GUILESS_MAIN(TestDspModeRade)
#include "tst_dspmode_rade.moc"
