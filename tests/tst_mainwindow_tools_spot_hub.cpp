// =================================================================
// tests/tst_mainwindow_tools_spot_hub.cpp  (NereusSDR)
// =================================================================
//
// Phase 3J-2 Task H1: verifies the Tools menu wiring contracts for
// Spot Hub + FreeDV Reporter without constructing the full MainWindow
// (which boots WDSP, the audio engine, the network discovery thread,
// and the full applet stack: too heavy and intermittent for unit
// tests; see tests/tst_mainwindow_status_bar_safety.cpp for the
// established pattern of testing MainWindow logic in isolation).
//
// Instead of probing the QMenuBar directly, we pin the contract that
// the H1 open* slots rely on:
//
//   1. SpotHubDialog is constructible from RadioModel's accessor
//      surface alone. If the SpotHubDialog ctor signature drifts or
//      RadioModel drops an accessor, the test fails at compile time.
//   2. FreeDVReporterDialog is constructible from
//      freeDvStationModel() + freeDvReporter() alone. Same drift gate.
//   3. SpotModel::clear() empties every row, which is what
//      Ctrl+Shift+K invokes via MainWindow::buildMenuBar's QShortcut.
//   4. SpotHubDialog::spotsClearedAll() routes into SpotModel::clear()
//      when wired by MainWindow::openSpotHub (the same wiring the
//      slot body performs at lazy-construction time).
//   5. FreeDVReporterDialog::qsyRequested(), messageSendRequested(),
//      and tuneRequested() are emit-able with the right signal
//      signature so the MainWindow lambda can connect them to
//      FreeDVReporterClient + SliceModel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task H1. Original
//                                    implementation for NereusSDR
//                                    with AI-assisted authoring via
//                                    Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/RadioModel.h"
#include "models/SpotModel.h"
#include "models/FreeDVStationModel.h"
#include "core/DxClusterClient.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/FreeDVReporterClient.h"
#include "core/PskReporterClient.h"
#include "core/DxccColorProvider.h"
#include "core/DxSpot.h"
#include "core/FreeDVStation.h"
#include "core/AppSettings.h"
#include "gui/SpotHubDialog.h"
#include "gui/FreeDVReporterDialog.h"

using namespace NereusSDR;

namespace {

// Push a synthetic DxSpot through the DxClusterClient so SpotModel
// ends up with a known row. Mirrors the cluster spot path tested
// directly in tst_radio_model_spot_wiring.
void seedSpot(RadioModel& model, const QString& callsign)
{
    DxSpot spot;
    spot.dxCall      = callsign;
    spot.freqMhz     = 14.250;
    spot.spotterCall = QStringLiteral("W3LPL");
    spot.comment     = QStringLiteral("FT8 strong");
    spot.utcTime     = QTime(18, 24);
    spot.source      = QStringLiteral("Cluster");
    spot.snr         = 12;
    emit model.dxCluster()->spotReceived(spot);
}

}  // namespace

class TestMainWindowToolsSpotHub : public QObject {
    Q_OBJECT
private slots:

    void init()
    {
        AppSettings::instance().clear();
    }

    // ── Contract 1: SpotHubDialog constructible from RadioModel ───────────
    //
    // If this test compiles, it proves the SpotHubDialog ctor signature
    // (7 clients + SpotModel + SpotTableModel + DxccColorProvider +
    // parent) lines up byte-for-byte with the RadioModel accessor return
    // types. The MainWindow::openSpotHub() slot uses exactly this call
    // shape.

    void spotHubDialogConstructibleFromRadioModel()
    {
        RadioModel model;
        SpotHubDialog dlg(
            model.dxCluster(),
            model.rbn(),
            model.wsjtx(),
            model.spotCollector(),
            model.pota(),
            model.freeDvReporter(),
            model.pskReporter(),
            model.spotModel(),
            model.spotTableModel(),
            model.dxccColorProvider(),
            nullptr);
        // No QVERIFY needed - reaching this line means the ctor
        // accepted every accessor return type without an implicit
        // cast or a const mismatch.
        Q_UNUSED(dlg);
    }

    // ── Contract 2: FreeDVReporterDialog constructible from RadioModel ────

    void freeDVReporterDialogConstructibleFromRadioModel()
    {
        RadioModel model;
        FreeDVReporterDialog dlg(
            model.freeDvStationModel(),
            model.freeDvReporter(),
            nullptr);
        Q_UNUSED(dlg);
    }

    // ── Contract 3: SpotModel::clear empties rows (Ctrl+Shift+K body) ─────
    //
    // The Ctrl+Shift+K QShortcut wired in buildMenuBar's tail calls
    // m_radioModel->spotModel()->clear(). This test pins that the call
    // does in fact drop every row + emits spotsCleared exactly once.

    void clearSpotsEmptiesEveryRow()
    {
        RadioModel model;
        seedSpot(model, QStringLiteral("W1AW"));
        seedSpot(model, QStringLiteral("K3LR"));
        seedSpot(model, QStringLiteral("VK6APH"));
        QCOMPARE(model.spotModel()->spots().size(), 3);

        QSignalSpy clearedSpy(model.spotModel(),
                              &SpotModel::spotsCleared);

        model.spotModel()->clear();

        QCOMPARE(model.spotModel()->spots().size(), 0);
        QCOMPARE(clearedSpy.count(), 1);
    }

    // ── Contract 4: SpotHubDialog::spotsClearedAll routes to clear ────────
    //
    // Mirrors the connect() the open slot does on first construction:
    //   connect(dlg, &SpotHubDialog::spotsClearedAll,
    //           spotModel, &SpotModel::clear);
    // Verifies the upstream signal exists, has the expected (no-arg)
    // signature, and the connection wipes the model.

    void spotHubDialogClearedAllSignalWipesSpotModel()
    {
        RadioModel model;
        SpotHubDialog dlg(
            model.dxCluster(),
            model.rbn(),
            model.wsjtx(),
            model.spotCollector(),
            model.pota(),
            model.freeDvReporter(),
            model.pskReporter(),
            model.spotModel(),
            model.spotTableModel(),
            model.dxccColorProvider(),
            nullptr);
        QObject::connect(&dlg, &SpotHubDialog::spotsClearedAll,
                         model.spotModel(), &SpotModel::clear);

        seedSpot(model, QStringLiteral("W1AW"));
        QCOMPARE(model.spotModel()->spots().size(), 1);

        emit dlg.spotsClearedAll();
        QCOMPARE(model.spotModel()->spots().size(), 0);
    }

    // ── Contract 5: SpotHubDialog::tuneRequested signal shape ─────────────
    //
    // MainWindow::openSpotHub wires tuneRequested(double freqMhz) to a
    // lambda that calls slice->setFrequency(freqMhz * 1e6). The lambda
    // body is exercised when the user double-clicks a row in the Spot
    // List tab. We verify the signal carries a single double argument
    // so the lambda's MHz -> Hz conversion is unambiguous.

    void spotHubDialogTuneRequestedCarriesMhz()
    {
        RadioModel model;
        SpotHubDialog dlg(
            model.dxCluster(),
            model.rbn(),
            model.wsjtx(),
            model.spotCollector(),
            model.pota(),
            model.freeDvReporter(),
            model.pskReporter(),
            model.spotModel(),
            model.spotTableModel(),
            model.dxccColorProvider(),
            nullptr);
        QSignalSpy spy(&dlg, &SpotHubDialog::tuneRequested);
        emit dlg.tuneRequested(14.250);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), 14.250);
    }

    // ── Contract 6: FreeDVReporterDialog QSY signal routes ────────────────
    //
    // openFreeDVReporter wires:
    //   qsyRequested -> FreeDVReporterClient::requestQSY
    //   messageSendRequested -> FreeDVReporterClient::updateMessage
    //   tuneRequested -> active slice tune lambda
    // We don't drive the network socket; we verify the signals exist
    // with the right shape and that the QSignalSpy round-trips. The
    // wire layer (requestQSY / updateMessage themselves) is covered by
    // tst_freedv_reporter_socketio.

    void freeDVReporterDialogQsySignalRoundTrips()
    {
        RadioModel model;
        FreeDVReporterDialog dlg(
            model.freeDvStationModel(),
            model.freeDvReporter(),
            nullptr);
        QSignalSpy spy(&dlg, &FreeDVReporterDialog::qsyRequested);
        emit dlg.qsyRequested(QStringLiteral("sid-001"),
                              14250000ULL,
                              QStringLiteral("CQ test"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("sid-001"));
        QCOMPARE(spy.at(0).at(1).toULongLong(), 14250000ULL);
        QCOMPARE(spy.at(0).at(2).toString(), QStringLiteral("CQ test"));
    }

    void freeDVReporterDialogMessageSendSignalRoundTrips()
    {
        RadioModel model;
        FreeDVReporterDialog dlg(
            model.freeDvStationModel(),
            model.freeDvReporter(),
            nullptr);
        QSignalSpy spy(&dlg,
                       &FreeDVReporterDialog::messageSendRequested);
        emit dlg.messageSendRequested(QStringLiteral("hello world"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(),
                 QStringLiteral("hello world"));
    }

    void freeDVReporterDialogTuneSignalCarriesHz()
    {
        RadioModel model;
        FreeDVReporterDialog dlg(
            model.freeDvStationModel(),
            model.freeDvReporter(),
            nullptr);
        QSignalSpy spy(&dlg, &FreeDVReporterDialog::tuneRequested);
        emit dlg.tuneRequested(14250000ULL);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toULongLong(), 14250000ULL);
    }
};

QTEST_MAIN(TestMainWindowToolsSpotHub)
#include "tst_mainwindow_tools_spot_hub.moc"
