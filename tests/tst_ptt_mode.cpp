// =================================================================
// tests/tst_ptt_mode.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-25 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include "core/PttMode.h"

using namespace NereusSDR;

class TestPttMode : public QObject {
    Q_OBJECT
private slots:

    // All 9 real values produce non-empty, non-"?" labels.
    void labelStrings_allNonEmpty()
    {
        const QVector<PttMode> values = {
            PttMode::None,
            PttMode::Manual,
            PttMode::Mic,
            PttMode::Cw,
            PttMode::X2,
            PttMode::Cat,
            PttMode::Vox,
            PttMode::Space,
            PttMode::Tci,
        };
        for (PttMode m : values) {
            const QString label = pttModeLabel(m);
            QVERIFY2(!label.isEmpty(), "label must not be empty");
            QVERIFY2(label != QLatin1String("?"), "label must not be fallback '?'");
        }
    }

    // All 9 real values produce distinct labels.
    void labelStrings_areDistinct()
    {
        const QVector<PttMode> values = {
            PttMode::None,
            PttMode::Manual,
            PttMode::Mic,
            PttMode::Cw,
            PttMode::X2,
            PttMode::Cat,
            PttMode::Vox,
            PttMode::Space,
            PttMode::Tci,
        };
        QSet<QString> seen;
        for (PttMode m : values) {
            const QString label = pttModeLabel(m);
            QVERIFY2(!seen.contains(label),
                     qPrintable(QStringLiteral("duplicate label: ") + label));
            seen.insert(label);
        }
    }

    // Integer values match Thetis enums.cs:346-359 [v2.10.3.13] exactly.
    void integerValues_matchThetis()
    {
        QCOMPARE(static_cast<int>(PttMode::First),  -1);
        QCOMPARE(static_cast<int>(PttMode::None),    0);
        QCOMPARE(static_cast<int>(PttMode::Manual),  1);
        QCOMPARE(static_cast<int>(PttMode::Mic),     2);
        QCOMPARE(static_cast<int>(PttMode::Cw),      3);
        QCOMPARE(static_cast<int>(PttMode::X2),      4);
        QCOMPARE(static_cast<int>(PttMode::Cat),     5);
        QCOMPARE(static_cast<int>(PttMode::Vox),     6);
        QCOMPARE(static_cast<int>(PttMode::Space),   7);
        QCOMPARE(static_cast<int>(PttMode::Tci),     8);
        QCOMPARE(static_cast<int>(PttMode::Last),    9);
    }

    // Sentinel values are present and do not collide with any real value.
    void sentinels_arePresent()
    {
        // First is below all real values; Last is above all real values.
        QVERIFY(static_cast<int>(PttMode::First) < static_cast<int>(PttMode::None));
        QVERIFY(static_cast<int>(PttMode::Last)  > static_cast<int>(PttMode::Tci));

        // Sentinels must not equal any of the 9 real enum values.
        const QVector<PttMode> realValues = {
            PttMode::None, PttMode::Manual, PttMode::Mic, PttMode::Cw,
            PttMode::X2,   PttMode::Cat,    PttMode::Vox, PttMode::Space,
            PttMode::Tci,
        };
        for (PttMode m : realValues) {
            QVERIFY(m != PttMode::First);
            QVERIFY(m != PttMode::Last);
        }
    }

    // Thetis-source names round-trip via pttModeFromString (exact case).
    void thetisNames_roundTripExactCase()
    {
        QCOMPARE(pttModeFromString(QStringLiteral("NONE")),   PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("MANUAL")), PttMode::Manual);
        QCOMPARE(pttModeFromString(QStringLiteral("MIC")),    PttMode::Mic);
        QCOMPARE(pttModeFromString(QStringLiteral("CW")),     PttMode::Cw);
        QCOMPARE(pttModeFromString(QStringLiteral("X2")),     PttMode::X2);
        QCOMPARE(pttModeFromString(QStringLiteral("CAT")),    PttMode::Cat);
        QCOMPARE(pttModeFromString(QStringLiteral("VOX")),    PttMode::Vox);
        QCOMPARE(pttModeFromString(QStringLiteral("SPACE")),  PttMode::Space);
        QCOMPARE(pttModeFromString(QStringLiteral("TCI")),    PttMode::Tci);
    }

    // Thetis-source names round-trip case-insensitively.
    void thetisNames_roundTripCaseInsensitive()
    {
        QCOMPARE(pttModeFromString(QStringLiteral("none")),   PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("manual")), PttMode::Manual);
        QCOMPARE(pttModeFromString(QStringLiteral("mic")),    PttMode::Mic);
        QCOMPARE(pttModeFromString(QStringLiteral("cw")),     PttMode::Cw);
        QCOMPARE(pttModeFromString(QStringLiteral("x2")),     PttMode::X2);
        QCOMPARE(pttModeFromString(QStringLiteral("cat")),    PttMode::Cat);
        QCOMPARE(pttModeFromString(QStringLiteral("vox")),    PttMode::Vox);
        QCOMPARE(pttModeFromString(QStringLiteral("space")),  PttMode::Space);
        QCOMPARE(pttModeFromString(QStringLiteral("tci")),    PttMode::Tci);
    }

    // User-visible labels round-trip via pttModeFromString.
    void userLabels_roundTrip()
    {
        QCOMPARE(pttModeFromString(QStringLiteral("None")),       PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("MOX/Manual")), PttMode::Manual);
        QCOMPARE(pttModeFromString(QStringLiteral("Mic PTT")),    PttMode::Mic);
        QCOMPARE(pttModeFromString(QStringLiteral("CW")),         PttMode::Cw);
        QCOMPARE(pttModeFromString(QStringLiteral("X2")),         PttMode::X2);
        QCOMPARE(pttModeFromString(QStringLiteral("CAT")),        PttMode::Cat);
        QCOMPARE(pttModeFromString(QStringLiteral("VOX")),        PttMode::Vox);
        QCOMPARE(pttModeFromString(QStringLiteral("Space PTT")),  PttMode::Space);
        QCOMPARE(pttModeFromString(QStringLiteral("TCI")),        PttMode::Tci);
    }

    // Unknown strings return PttMode::None (safe fallback).
    void unknownString_returnsPttModeNone()
    {
        QCOMPARE(pttModeFromString(QStringLiteral("")),          PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("garbage")),   PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("FIRST")),     PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("LAST")),      PttMode::None);
        QCOMPARE(pttModeFromString(QStringLiteral("pttmode")),   PttMode::None);
    }

    // pttModeThetisName returns the upstream C# identifier.
    void thetisName_matchesUpstreamIdentifiers()
    {
        QCOMPARE(pttModeThetisName(PttMode::None),   QStringLiteral("NONE"));
        QCOMPARE(pttModeThetisName(PttMode::Manual), QStringLiteral("MANUAL"));
        QCOMPARE(pttModeThetisName(PttMode::Mic),    QStringLiteral("MIC"));
        QCOMPARE(pttModeThetisName(PttMode::Cw),     QStringLiteral("CW"));
        QCOMPARE(pttModeThetisName(PttMode::X2),     QStringLiteral("X2"));
        QCOMPARE(pttModeThetisName(PttMode::Cat),    QStringLiteral("CAT"));
        QCOMPARE(pttModeThetisName(PttMode::Vox),    QStringLiteral("VOX"));
        QCOMPARE(pttModeThetisName(PttMode::Space),  QStringLiteral("SPACE"));
        QCOMPARE(pttModeThetisName(PttMode::Tci),    QStringLiteral("TCI"));
    }

    // pttModeThetisName + pttModeFromString compose to identity for all 9 values.
    void thetisName_roundTripsThroughFromString()
    {
        const QVector<PttMode> values = {
            PttMode::None,
            PttMode::Manual,
            PttMode::Mic,
            PttMode::Cw,
            PttMode::X2,
            PttMode::Cat,
            PttMode::Vox,
            PttMode::Space,
            PttMode::Tci,
        };
        for (PttMode m : values) {
            QCOMPARE(pttModeFromString(pttModeThetisName(m)), m);
        }
    }
};

QTEST_GUILESS_MAIN(TestPttMode)
#include "tst_ptt_mode.moc"
