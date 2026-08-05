// no-port-check: NereusSDR-original CI gate for init-burst byte-for-byte parity.
//
// Phase 3J-1 Task 4.3: compares TciProtocol::buildInitBurst() against the
// captured (synthetic for now) golden file at
// tests/data/tci/init_burst_anan_g2_rx1.txt.
//
// Strict-protocol prefixes are compared byte-for-byte:
//   protocol:    trx_count:    channels_count:    modulations_list:
//   receive_only:    ready;
// These define the wire-format compatibility surface — any change is
// a parity violation and a release-blocker.
//
// Radio-identifying fields are compared loosely (prefix-only):
//   device:    vfo:    dds:    if:    tx_frequency:    tx_frequency_thetis:
// These vary across radios + slice positions and are not byte-stable across
// captures from different hardware.

#include <QtTest>
#include <QFile>
#include <QTextStream>
#include "core/TciProtocol.h"
#include "core/AppSettings.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciInitBurstGolden : public QObject {
    Q_OBJECT
private:
    static QStringList loadGolden();
    static bool isStrictPrefix(const QString& line);
    // Phase 3J-1 closeout Item 1 (2026-05-12): pin AppSettings to the
    // exact "capture conditions" documented in the golden file header
    // so the test is independent of the user's persisted settings.
    // Without this, the bench-fix flip of TciEmulateExpertSDR3Protocol +
    // TciEmulateSunSDR2Pro defaults to True (commit ccb1c5fb — required
    // for WSJT-X to enter TCI audio mode) would silently invalidate the
    // golden comparison.
    static void pinAppSettingsToGoldenCaptureConditions();
private slots:
    void burst_line_count_matches_golden();
    void burst_strict_prefixes_match_golden_byte_for_byte();
    void burst_loose_prefixes_match_by_kind();
};

void TestTciInitBurstGolden::pinAppSettingsToGoldenCaptureConditions()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("False"));
    s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"),         QStringLiteral("False"));
    s.setValue(QStringLiteral("TciCwluBecomesCw"),             QStringLiteral("False"));
    s.setValue(QStringLiteral("TciSendInitialFrequencyStateOnConnect"), QStringLiteral("True"));
    s.setValue(QStringLiteral("TciTxStreamBufferingMs"),       QStringLiteral("50"));
}

QStringList TestTciInitBurstGolden::loadGolden()
{
    QFile f(QStringLiteral(NEREUS_TEST_DATA_DIR "/tci/init_burst_anan_g2_rx1.txt"));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QTextStream ts(&f);
    QStringList out;
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.startsWith(QLatin1Char('#'))) { continue; }  // skip header comments
        if (line.trimmed().isEmpty()) { continue; }
        out << line;
    }
    return out;
}

bool TestTciInitBurstGolden::isStrictPrefix(const QString& line)
{
    static const QStringList strict = {
        QStringLiteral("protocol:"),
        QStringLiteral("trx_count:"),
        QStringLiteral("channels_count:"),
        QStringLiteral("modulations_list:"),
        QStringLiteral("receive_only:"),
        QStringLiteral("ready;"),
    };
    for (const auto& p : strict) {
        if (line.startsWith(p)) { return true; }
    }
    return false;
}

void TestTciInitBurstGolden::burst_line_count_matches_golden()
{
    pinAppSettingsToGoldenCaptureConditions();
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList ours = p.buildInitBurst();
    const QStringList golden = loadGolden();
    QVERIFY2(!golden.isEmpty(), "Golden file empty or missing — regenerate "
             "tests/data/tci/init_burst_anan_g2_rx1.txt from buildInitBurst() output.");
    QCOMPARE(ours.size(), golden.size());
}

void TestTciInitBurstGolden::burst_strict_prefixes_match_golden_byte_for_byte()
{
    pinAppSettingsToGoldenCaptureConditions();
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList ours = p.buildInitBurst();
    const QStringList golden = loadGolden();
    QVERIFY(!golden.isEmpty());
    const int n = std::min(ours.size(), golden.size());
    for (int i = 0; i < n; ++i) {
        if (isStrictPrefix(ours[i]) || isStrictPrefix(golden[i])) {
            // Byte-for-byte equality required.
            QCOMPARE(ours[i], golden[i]);
        }
    }
}

void TestTciInitBurstGolden::burst_loose_prefixes_match_by_kind()
{
    pinAppSettingsToGoldenCaptureConditions();
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList ours = p.buildInitBurst();
    const QStringList golden = loadGolden();
    QVERIFY(!golden.isEmpty());
    const int n = std::min(ours.size(), golden.size());
    for (int i = 0; i < n; ++i) {
        if (isStrictPrefix(ours[i]) || isStrictPrefix(golden[i])) {
            continue;  // covered by strict test
        }
        // Loose match: extract the prefix (everything up to and including
        // the first ':') and assert prefixes match. Values may vary across
        // radios or Phase 6+ RadioModel wiring.
        const auto extractPrefix = [](const QString& line) -> QString {
            const int colonIdx = line.indexOf(QLatin1Char(':'));
            return colonIdx < 0 ? line : line.left(colonIdx + 1);
        };
        const QString oursPrefix = extractPrefix(ours[i]);
        const QString goldenPrefix = extractPrefix(golden[i]);
        QCOMPARE(oursPrefix, goldenPrefix);
    }
}

QTEST_GUILESS_MAIN(TestTciInitBurstGolden)
#include "tst_tci_init_burst_golden.moc"
