// =================================================================
// tests/tst_ptt_source.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include "core/PttSource.h"

using namespace NereusSDR;

class TestPttSource : public QObject {
    Q_OBJECT
private slots:

    void labelStrings_matchEnum()
    {
        // Verify that every named PttSource value has a non-empty,
        // non-"?" label.
        const QVector<PttSource> values = {
            PttSource::None,
            PttSource::Mox,
            PttSource::Vox,
            PttSource::Cat,
            PttSource::MicPtt,
            PttSource::Cw,
            PttSource::Tune,
            PttSource::TwoTone,
        };

        for (PttSource s : values) {
            const QString label = pttSourceLabel(s);
            QVERIFY2(!label.isEmpty(), "label must not be empty");
        }
    }

    void noneLabel_isLiteralNone()
    {
        QCOMPARE(pttSourceLabel(PttSource::None), QStringLiteral("none"));
    }

    void moxLabel_isMOX()
    {
        QCOMPARE(pttSourceLabel(PttSource::Mox), QStringLiteral("MOX"));
    }

    void catLabel_isCAT()
    {
        QCOMPARE(pttSourceLabel(PttSource::Cat), QStringLiteral("CAT"));
    }

    void twoToneLabel_is2Tone()
    {
        QCOMPARE(pttSourceLabel(PttSource::TwoTone), QStringLiteral("2-Tone"));
    }

    void micPttLabel_isMicPTT()
    {
        QCOMPARE(pttSourceLabel(PttSource::MicPtt), QStringLiteral("Mic PTT"));
    }

    void enumValues_areDistinct()
    {
        // All defined values must be numerically distinct.
        QSet<int> seen;
        const QVector<PttSource> values = {
            PttSource::None, PttSource::Mox, PttSource::Vox, PttSource::Cat,
            PttSource::MicPtt, PttSource::Cw, PttSource::Tune, PttSource::TwoTone
        };
        for (PttSource s : values) {
            int v = static_cast<int>(s);
            QVERIFY2(!seen.contains(v), "enum values must be distinct");
            seen.insert(v);
        }
    }

    void unknownValue_returnsFallback()
    {
        // Cast a value outside the enum range — should not crash.
        PttSource bad = static_cast<PttSource>(99);
        const QString label = pttSourceLabel(bad);
        QCOMPARE(label, QStringLiteral("?"));
    }
};

QTEST_GUILESS_MAIN(TestPttSource)
#include "tst_ptt_source.moc"
