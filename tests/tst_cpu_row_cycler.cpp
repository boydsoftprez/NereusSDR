// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cpu_row_cycler.cpp  (NereusSDR)
// =================================================================
//
// Parity ruling C9: in a remote window the System tile's CPU row shows this
// computer's CPU and the Core's, each labelled, cycling every 3 s; the
// right-click pins either; a reading above 80% holds the row on it in the
// warning colour until it drops back; with no Core reading the row shows
// this computer only, with the reason in its tooltip.
//
// Modification history (NereusSDR):
//   2026-09-28 : Created for parity ruling C9 by J.J. Boyd (KG4VCF).
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "OperatorWording.h"
#include "gui/RemoteTelemetryController.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/CpuRowCycler.h"
#include "gui/widgets/SystemTile.h"

using namespace NereusSDR;

namespace {

const QString kNotCurrent = QStringLiteral("The Core's CPU reading is not current.");

CpuRowCycler withBoth(double local, double core)
{
    CpuRowCycler c;
    c.setThisComputer(local);
    c.setCore(core, QString());
    return c;
}

} // namespace

class TstCpuRowCycler : public QObject {
    Q_OBJECT

private slots:
    void cyclesEveryThreeSecondsEachLabelled()
    {
        CpuRowCycler c = withBoth(12.0, 34.0);
        QCOMPARE(c.source(), CpuRowCycler::Source::Cycle);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        QCOMPARE(c.row().percent, 12.0);
        QVERIFY(!c.row().core);
        c.advance(1000);
        c.advance(1000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        c.advance(1000);
        QCOMPARE(c.row().label, QStringLiteral("Core"));
        QCOMPARE(c.row().percent, 34.0);
        QVERIFY(c.row().core);
        QVERIFY(!c.row().warning);
        c.advance(3000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
    }

    void rightClickPinsEitherAndIsSaved()
    {
        CpuRowCycler c = withBoth(12.0, 34.0);
        c.setSource(CpuRowCycler::Source::Core);
        for (int i = 0; i < 5; ++i) {
            c.advance(3000);
            QCOMPARE(c.row().label, QStringLiteral("Core"));
        }
        c.setSource(CpuRowCycler::Source::ThisComputer);
        c.advance(6000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        for (CpuRowCycler::Source s : {CpuRowCycler::Source::Cycle,
                                       CpuRowCycler::Source::ThisComputer,
                                       CpuRowCycler::Source::Core}) {
            QCOMPARE(CpuRowCycler::sourceFromKey(CpuRowCycler::sourceKey(s)), s);
        }
        QCOMPARE(CpuRowCycler::sourceKey(CpuRowCycler::Source::Cycle), QStringLiteral("Cycle"));
        QCOMPARE(CpuRowCycler::sourceFromKey(QStringLiteral("nonsense")),
                 CpuRowCycler::Source::Cycle);
    }

    void aHotReadingHoldsTheRowInTheWarningColour()
    {
        CpuRowCycler c = withBoth(12.0, 91.0);
        c.advance(1000);
        QCOMPARE(c.row().label, QStringLiteral("Core"));  // straight to the hot one
        QVERIFY(c.row().warning);
        for (int i = 0; i < 4; ++i) {
            c.advance(3000);
            QCOMPARE(c.row().label, QStringLiteral("Core"));
        }
        // This computer runs hot instead: the row goes to it and holds.
        c.setCore(40.0, QString());
        c.setThisComputer(85.0);
        c.advance(1000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        QVERIFY(c.row().warning);
        c.advance(6000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        // Both hot: it stays where it is.
        c.setCore(90.0, QString());
        c.advance(6000);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        // Back below 80%: cycling again.
        c.setThisComputer(20.0);
        c.setCore(30.0, QString());
        c.advance(3000);
        QCOMPARE(c.row().label, QStringLiteral("Core"));
        QVERIFY(!c.row().warning);
        // Exactly 80% is not hot.
        c.setCore(80.0, QString());
        QVERIFY(!c.row().warning);
        // A pinned hot reading is drawn in the warning colour too.
        c.setSource(CpuRowCycler::Source::ThisComputer);
        c.setThisComputer(95.0);
        QVERIFY(c.row().warning);
    }

    void withNoCoreReadingOnlyThisComputerWithTheReason()
    {
        CpuRowCycler c;
        c.setThisComputer(12.0);
        c.setCore(std::nullopt, kNotCurrent);
        for (int i = 0; i < 3; ++i) {
            c.advance(3000);
            QCOMPARE(c.row().label, QStringLiteral("CPU"));
            QCOMPARE(c.row().toolTip, kNotCurrent);
        }
        c.setSource(CpuRowCycler::Source::Core);
        QCOMPARE(c.row().label, QStringLiteral("CPU"));
        QCOMPARE(c.row().toolTip, kNotCurrent);
        QVERIFY(OperatorWording::isPlain(kNotCurrent));
        // It comes back: the pinned row shows the Core.
        c.setCore(33.0, QString());
        QCOMPARE(c.row().label, QStringLiteral("Core"));
        QVERIFY(!c.row().toolTip.isEmpty());
    }

    // The Core's reading comes from its telemetry, the system or the app
    // share as the row's System/App choice says; absent, the plain reason.
    void theCoresReadingAndWhyItIsMissing()
    {
        RemoteTelemetryView view;
        QString why;
        QVERIFY(!RemoteTelemetryController::coreCpuPercent(view, true, &why));
        QCOMPARE(why, kNotCurrent);
        view.state = RemoteTelemetryView::State::Unsupported;
        QVERIFY(!RemoteTelemetryController::coreCpuPercent(view, true, &why));
        QCOMPARE(why, QStringLiteral("This Core does not report its CPU. Updating the Core may help."));
        view.state = RemoteTelemetryView::State::Current;
        QVERIFY(!RemoteTelemetryController::coreCpuPercent(view, true, &why));
        QCOMPARE(why, QStringLiteral("This Core does not measure its CPU."));
        view.coreHost.systemCpuPercent = 42.0;
        view.coreHost.processCpuPercent = 7.0;
        QCOMPARE(*RemoteTelemetryController::coreCpuPercent(view, true, &why), 42.0);
        QVERIFY(why.isEmpty());
        QCOMPARE(*RemoteTelemetryController::coreCpuPercent(view, false, &why), 7.0);
        for (const QString& w : {kNotCurrent,
                                 QStringLiteral("This Core does not measure its CPU.")}) {
            QVERIFY2(OperatorWording::isPlain(w), qPrintable(w));
        }
    }

    void theTileShowsTheRowAsTheCyclerSaysIt()
    {
        SystemTile tile;
        tile.setCpuPercent(7.0);
        QCOMPARE(tile.cpuRowLabel(), QStringLiteral("CPU"));
        QCOMPARE(tile.cpuRowText(), QStringLiteral("7%"));
        QVERIFY(!tile.cpuRowWarning());

        CpuRowCycler c = withBoth(12.0, 91.0);
        c.advance(1000);
        tile.setCpuRow(c.row());
        QCOMPARE(tile.cpuRowLabel(), QStringLiteral("Core"));
        QCOMPARE(tile.cpuRowText(), QStringLiteral("91%"));
        QVERIFY(tile.cpuRowWarning());
        QVERIFY(tile.cpuRowStyleSheet().contains(QLatin1String(Style::kAmberWarn)));
        QCOMPARE(tile.cpuRowToolTip(), c.row().toolTip);

        c.setCore(std::nullopt, kNotCurrent);
        tile.setCpuRow(c.row());
        QCOMPARE(tile.cpuRowLabel(), QStringLiteral("CPU"));
        QVERIFY(!tile.cpuRowWarning());
        QCOMPARE(tile.cpuRowToolTip(), kNotCurrent);
    }
};

QTEST_MAIN(TstCpuRowCycler)
#include "tst_cpu_row_cycler.moc"
