// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_label.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 12 (R-IOS-08): the Core's label, `<callsign>/<suffix>`,
// per the pairing design section 3.3. Refusals first (bad suffixes and
// callsigns), then parsing, the case-insensitive comparison that keeps
// what was typed for display, the bare callsign, and the default from the
// StationCallsign setting.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/security/StationLabel.h"

using namespace NereusSDR;

class TstStationLabel : public QObject {
    Q_OBJECT

private slots:
    void refusesWhatIsNotALabel_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("blank") << QStringLiteral("   ");
        QTest::newRow("no callsign") << QStringLiteral("/shack");
        QTest::newRow("space in suffix") << QStringLiteral("KG4VCF/the shack");
        QTest::newRow("dot in suffix") << QStringLiteral("KG4VCF/shack.1");
        QTest::newRow("non-ascii suffix") << QStringLiteral("KG4VCF/shäck");
        QTest::newRow("33-character suffix")
            << QStringLiteral("KG4VCF/") + QString(33, QLatin1Char('a'));
        QTest::newRow("punctuation in callsign") << QStringLiteral("KG4.VCF/shack");
        QTest::newRow("callsign too long") << QString(33, QLatin1Char('K'));
    }

    void refusesWhatIsNotALabel()
    {
        QFETCH(QString, text);
        QVERIFY(!StationLabel::parse(text).has_value());
        QVERIFY(!StationLabel::sameLabel(text, text));
    }

    void parsesCallsignAndSuffix()
    {
        const auto label = StationLabel::parse(QStringLiteral(" KG4VCF/Shack "));
        QVERIFY(label.has_value());
        QCOMPARE(label->callsign, QStringLiteral("KG4VCF"));
        QCOMPARE(label->suffix, QStringLiteral("Shack"));
        QCOMPARE(label->display(), QStringLiteral("KG4VCF/Shack"));

        const QString longest = QStringLiteral("KG4VCF/") + QString(32, QLatin1Char('z'));
        QVERIFY(StationLabel::parse(longest).has_value());
        QVERIFY(StationLabel::parse(QStringLiteral("KG4VCF/rack_2-top")).has_value());
    }

    void anEmptySuffixIsTheBareCallsign()
    {
        const auto bare = StationLabel::parse(QStringLiteral("KG4VCF"));
        QVERIFY(bare.has_value());
        QVERIFY(bare->suffix.isEmpty());
        QCOMPARE(bare->display(), QStringLiteral("KG4VCF"));
        const auto trailing = StationLabel::parse(QStringLiteral("KG4VCF/"));
        QVERIFY(trailing.has_value());
        QCOMPARE(trailing->display(), QStringLiteral("KG4VCF"));
        QVERIFY(StationLabel::sameLabel(QStringLiteral("KG4VCF/"), QStringLiteral("kg4vcf")));
    }

    void aPortableCallsignKeepsItsSlash()
    {
        const auto label = StationLabel::parse(QStringLiteral("KG4VCF/P/shack"));
        QVERIFY(label.has_value());
        QCOMPARE(label->callsign, QStringLiteral("KG4VCF/P"));
        QCOMPARE(label->suffix, QStringLiteral("shack"));
    }

    void comparisonIgnoresCaseAndDisplayKeepsIt()
    {
        QVERIFY(StationLabel::sameLabel(QStringLiteral("KG4VCF/Shack"),
                                        QStringLiteral("kg4vcf/shack")));
        QVERIFY(!StationLabel::sameLabel(QStringLiteral("KG4VCF/shack"),
                                         QStringLiteral("KG4VCF/remote")));
        QVERIFY(!StationLabel::sameLabel(QStringLiteral("KG4VCF/shack"),
                                         QStringLiteral("KK7GWY/shack")));
        QCOMPARE(StationLabel::parse(QStringLiteral("kg4vcf/Shack"))->display(),
                 QStringLiteral("kg4vcf/Shack"));
    }

    void theDefaultIsTheStationCallsignSetting()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(!StationLabel::defaultLabel(settings).has_value());
        settings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        const auto label = StationLabel::defaultLabel(settings);
        QVERIFY(label.has_value());
        QCOMPARE(label->display(), QStringLiteral("KG4VCF"));
        QVERIFY(label->suffix.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstStationLabel)
#include "tst_station_label.moc"
