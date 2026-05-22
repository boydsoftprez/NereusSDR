// no-port-check: NereusSDR-original helper that prints the current
// buildInitBurst() output to stdout, for one-shot regeneration of the
// synthetic golden at tests/data/tci/init_burst_anan_g2_rx1.txt.
//
// This is NOT a regression test -- the trailing QCOMPARE pass guarantees
// only that the burst is non-empty.  Run manually:
//   cmake --build build --target tst_tci_init_burst_regen
//   ./build/tests/tst_tci_init_burst_regen 2>&1 | grep -v "^[A-Z][a-z]"
// and paste the dds:..ready; block back into the golden file (keep the
// existing # header).
//
// Listed in CMakeLists with the same nereus_add_test macro so the file
// participates in ctest, but the trivial assertion is intentional -- the
// real verification lives in tst_tci_init_burst_golden and the new
// tst_tci_init_burst_live_state.

#include <QtTest>
#include <QTextStream>
#include "core/TciProtocol.h"
#include "core/AppSettings.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciInitBurstRegen : public QObject {
    Q_OBJECT
private slots:
    void dump_current_burst();
};

void TestTciInitBurstRegen::dump_current_burst()
{
    // Mirror the golden test's pin so the dump matches the same capture
    // conditions documented in the golden file header.
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("False"));
    s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"),         QStringLiteral("False"));
    s.setValue(QStringLiteral("TciCwluBecomesCw"),             QStringLiteral("False"));
    s.setValue(QStringLiteral("TciSendInitialFrequencyStateOnConnect"), QStringLiteral("True"));
    s.setValue(QStringLiteral("TciTxStreamBufferingMs"),       QStringLiteral("50"));

    TestMockRadioModel mock;  // mock defaults match the live-state baseline
    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QTextStream out(stdout);
    out << "===BEGIN===" << Qt::endl;
    for (const auto& line : burst) {
        out << line << Qt::endl;
    }
    out << "===END===" << Qt::endl;
    out.flush();

    QVERIFY(!burst.isEmpty());
}

QTEST_GUILESS_MAIN(TestTciInitBurstRegen)
#include "tst_tci_init_burst_regen.moc"
