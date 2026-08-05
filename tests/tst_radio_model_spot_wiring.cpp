// =================================================================
// tests/tst_radio_model_spot_wiring.cpp  (NereusSDR)
// =================================================================
//
// Phase 3J-2 Task H2: verifies that RadioModel owns the seven spot
// ingest clients (DxCluster + RBN + WSJT-X + SpotCollector + POTA +
// FreeDV Reporter + PSK Reporter), the three view models (SpotModel +
// FreeDVStationModel + RxDecodeModel), and the DxccColorProvider, and
// that each client's spotReceived(DxSpot) signal lands in
// SpotModel::applySpotStatus via the per-source adapter slot.
//
// The full RadioModel::wireConnectionSignals() flow normally happens
// inside connectToRadio() with a real network thread. For this unit
// test we exercise only the spot-wiring path which is built in the
// RadioModel constructor (no live radio required).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/RadioModel.h"
#include "models/SpotModel.h"
#include "models/FreeDVStationModel.h"
#include "models/RxDecodeModel.h"
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

using namespace NereusSDR;

namespace {

// Helper: build a synthetic DxSpot for a given source label. The fields
// match the shape each real client emits on a parsed packet.
DxSpot makeSpot(const QString& source, const QString& callsign)
{
    DxSpot spot;
    spot.dxCall      = callsign;
    spot.freqMhz     = 14.250;
    spot.spotterCall = QStringLiteral("W3LPL");
    spot.comment     = QStringLiteral("FT8 strong");
    spot.utcTime     = QTime(18, 24);
    spot.source      = source;
    spot.snr         = 12;
    return spot;
}

}  // namespace

class TestRadioModelSpotWiring : public QObject {
    Q_OBJECT
private slots:

    void init()
    {
        AppSettings::instance().clear();
    }

    // ── Accessor surface ──────────────────────────────────────────────────

    void radioModelExposesSpotModelAccessor()
    {
        RadioModel model;
        QVERIFY(model.spotModel() != nullptr);
    }

    void radioModelExposesFreeDvStationModelAccessor()
    {
        RadioModel model;
        QVERIFY(model.freeDvStationModel() != nullptr);
    }

    void radioModelExposesRxDecodeModelAccessor()
    {
        RadioModel model;
        QVERIFY(model.rxDecodeModel() != nullptr);
    }

    void radioModelExposesDxccColorProviderAccessor()
    {
        RadioModel model;
        QVERIFY(model.dxccColorProvider() != nullptr);
    }

    void radioModelExposesAllSixSpotClientAccessors()
    {
        RadioModel model;
        QVERIFY(model.dxCluster()     != nullptr);
        QVERIFY(model.rbn()           != nullptr);
        QVERIFY(model.wsjtx()         != nullptr);
        QVERIFY(model.spotCollector() != nullptr);
        QVERIFY(model.pota()          != nullptr);
        QVERIFY(model.freeDvReporter()!= nullptr);
        QVERIFY(model.pskReporter()   != nullptr);

        // RBN must be a distinct DxClusterClient instance from the
        // primary DX cluster client. Same class, two configurations.
        QVERIFY(model.dxCluster() != model.rbn());
    }

    // ── Per-source adapter wiring ─────────────────────────────────────────
    //
    // Each test emits the client's spotReceived(DxSpot) signal and
    // verifies the corresponding adapter slot pushed a SpotData row
    // into the SpotModel.

    void dxClusterSpotEmitsAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.dxCluster()->spotReceived(
            makeSpot(QStringLiteral("Cluster"), QStringLiteral("W1AW")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().size(), 1);
        const SpotData spot = model.spotModel()->spots().first();
        QCOMPARE(spot.callsign,        QStringLiteral("W1AW"));
        QCOMPARE(spot.rxFreqMhz,       14.250);
        QCOMPARE(spot.source,          QStringLiteral("Cluster"));
        QCOMPARE(spot.spotterCallsign, QStringLiteral("W3LPL"));
    }

    void rbnSpotEmitsAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.rbn()->spotReceived(
            makeSpot(QStringLiteral("RBN"), QStringLiteral("K3LR")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().size(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("RBN"));
    }

    void wsjtxSpotEmitsAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.wsjtx()->spotReceived(
            makeSpot(QStringLiteral("WSJT-X"), QStringLiteral("VK6APH")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("WSJT-X"));
    }

    void wsjtxDecodeAppliesToRxDecodeModel()
    {
        RadioModel model;
        QSignalSpy spy(model.rxDecodeModel(), &RxDecodeModel::decodeAdded);

        // WsjtxClient never emits a separate decodeReceived; the
        // single spotReceived signal also feeds the RxDecodeModel
        // (NereusSDR design: WSJT-X data is "what my radio just
        // heard"). The adapter slot pushes to both sinks.
        emit model.wsjtx()->spotReceived(
            makeSpot(QStringLiteral("WSJT-X"), QStringLiteral("VK6APH")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.rxDecodeModel()->decodes().size(), 1);
        const RxDecode dec = model.rxDecodeModel()->decodes().first();
        QCOMPARE(dec.callsign, QStringLiteral("VK6APH"));
        QCOMPARE(dec.source,   QStringLiteral("WSJT-X"));
        QCOMPARE(dec.snr,      12);
    }

    void potaSpotAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.pota()->spotReceived(
            makeSpot(QStringLiteral("POTA"), QStringLiteral("KE0PAR")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("POTA"));
    }

    void spotCollectorSpotAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.spotCollector()->spotReceived(
            makeSpot(QStringLiteral("SpotCollector"), QStringLiteral("W2RE")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("SpotCollector"));
    }

    void freeDvReporterSpotAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.freeDvReporter()->spotReceived(
            makeSpot(QStringLiteral("FreeDV"), QStringLiteral("AC6V")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("FreeDV"));
    }

    void pskReporterSpotAppliesToSpotModel()
    {
        RadioModel model;
        QSignalSpy spy(model.spotModel(), &SpotModel::spotAdded);

        emit model.pskReporter()->spotReceived(
            makeSpot(QStringLiteral("PSK"), QStringLiteral("N1MM")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.spotModel()->spots().first().source,
                 QStringLiteral("PSK"));
    }

    // ── FreeDV Reporter station signals ───────────────────────────────────

    void freeDvReporterStationAddedAppliesToStationModel()
    {
        RadioModel model;
        QSignalSpy spy(model.freeDvStationModel(),
                       &FreeDVStationModel::stationAdded);

        FreeDVStation info;
        info.callsign   = QStringLiteral("KK7GWY");
        info.gridSquare = QStringLiteral("CN87");
        info.txMode     = QStringLiteral("FreeDV 2020");

        emit model.freeDvReporter()->stationAdded(
            QStringLiteral("sid-001"), info);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.freeDvStationModel()->stationCount(), 1);
        QCOMPARE(model.freeDvStationModel()
                     ->stationBySid(QStringLiteral("sid-001")).callsign,
                 QStringLiteral("KK7GWY"));
    }
};

QTEST_GUILESS_MAIN(TestRadioModelSpotWiring)
#include "tst_radio_model_spot_wiring.moc"
