// no-port-check: NereusSDR-original test.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - turning the beam to a spot from the desktop (rotor control
// plan Task 8): the pan's spot menu, the Spot Hub's Bearing column and
// menu, and the auto-turn preference.
//
// Acceptance (plan Task 8):
//   * Without a rotor or a bearing, Turn beam is shown greyed with the
//     reason.
//   * With auto-turn off, tuning never moves the rotor.
//
// No test opens a serial port or turns a real rotor: the commands go to a
// fake RotorCommandSink.
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 8).
//                                    AI-assisted via Anthropic Claude Code.

#include <QAction>
#include <QCheckBox>
#include <QHeaderView>
#include <QMenu>
#include <QSignalSpy>
#include <QTableView>
#include <QtTest>

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/GreatCircle.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/StationRotorController.h"
#include "core/WsjtxClient.h"
#include "core/settings/SettingsScope.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpotHubDialog.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/BandFilterProxy.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"
#include "models/SpotBeamTurner.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"
#include "OperatorWording.h"

#include <cmath>
#include <memory>

using namespace NereusSDR;
using RotorLink::RotorModel;

namespace {

const QString kCall = QStringLiteral("JA1ABC");
const QString kGrid = QStringLiteral("FN31pr");
// A prefix no country holds (Q is not allocated), so cty.dat places nothing.
const QString kUnplaceableCall = QStringLiteral("QQ1ABC");
const QString kCoreTooOld =
    QStringLiteral("This Core does not control a rotor. Updating the Core may help.");

class FakeSink : public RotorCommandSink {
public:
    struct Target {
        double azimuth;
        double elevation;
    };
    QList<Target> targets;
    QList<QPair<QString, bool>> calls;
    bool available = true;
    QString unavailableReason;
    bool accept = true;
    QString refusal = QStringLiteral("The rotor is not connected.");

    int sent() const { return int(targets.size() + calls.size()); }

    bool rotorControlAvailable(QString* reason) const override
    {
        if (!available && reason) { *reason = unavailableReason; }
        return available;
    }
    bool requestRotorTarget(double azimuthDeg, double elevationDeg, QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        targets.append({azimuthDeg, elevationDeg});
        return true;
    }
    bool requestStopRotor(QString*) override { return true; }
    bool requestTurnRotorToCall(const QString& call, bool longPath, QString* reason) override
    {
        if (!accept) { return refuse(reason); }
        calls.append({call, longPath});
        return true;
    }
    bool requestNudgeRotor(Nudge, bool, QString*) override { return true; }
    bool requestConfigureRotor(const Setup&, QString*) override { return true; }
    bool requestRotorPresets(const QString&, QString*) override { return true; }
    quint32 lastRotorCommandId() const override { return 0; }

private:
    bool refuse(QString* reason) const
    {
        if (reason) { *reason = refusal; }
        return false;
    }
};

RotorModel::State connectedRotor()
{
    RotorModel::State s;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.driver = RotorModel::Driver::Gs232b;
    s.label = QStringLiteral("Easy Rotor Control on COM4");
    s.axes = RotorModel::Axes::Azimuth;
    s.positionFresh = true;
    s.azimuthDeg = 47.0;
    return s;
}

// The short-path bearing this window works out for `call` from `grid`.
double localBearing(DxccColorProvider& dxcc, const QString& call, const QString& grid)
{
    const std::optional<GeoPosition> at = dxcc.positionForCallsign(call);
    if (!at) { return -1.0; }
    const std::optional<double> b = GreatCircle::bearingFromGrid(grid, *at);
    return b ? *b : -1.0;
}

QString wholeDegrees(double deg)
{
    int d = static_cast<int>(std::lround(deg));
    if (d >= 360) { d -= 360; }
    return QString::number(d) + QChar(0x00B0);
}

SpectrumWidget::SpotMarker marker(const QString& call, double bearingDeg,
                                  const QString& source = QStringLiteral("Cluster"))
{
    SpectrumWidget::SpotMarker m;
    m.index = 7;
    m.callsign = call;
    m.freqMhz = 14.025;
    m.mode = QStringLiteral("CW");
    m.source = source;
    m.bearingDeg = bearingDeg;
    return m;
}

QStringList actionTexts(const QMenu& menu)
{
    QStringList out;
    for (QAction* a : menu.actions()) {
        out.append(a->isSeparator() ? QStringLiteral("--") : a->text());
    }
    return out;
}

QAction* actionNamed(const QMenu& menu, const QString& objectName)
{
    for (QAction* a : menu.actions()) {
        if (a->objectName() == objectName) { return a; }
    }
    return nullptr;
}

QAction* actionStarting(const QMenu& menu, const QString& prefix)
{
    for (QAction* a : menu.actions()) {
        if (a->text().startsWith(prefix)) { return a; }
    }
    return nullptr;
}

// The Spot Hub with its own clients, as the dialog smoke test makes it.
struct Hub {
    DxClusterClient cluster;
    DxClusterClient rbn;
    WsjtxClient wsjtx;
    SpotCollectorClient spotCollector;
    PotaClient pota;
    FreeDVReporterClient freedv;
    PskReporterClient psk;
    SpotModel spots;
    SpotTableModel table;
    std::unique_ptr<SpotHubDialog> dialog;

    explicit Hub(DxccColorProvider* dxcc)
    {
        dialog = std::make_unique<SpotHubDialog>(&cluster, &rbn, &wsjtx, &spotCollector, &pota,
                                                 &freedv, &psk, &spots, &table, dxcc, nullptr);
        dialog->setTunedCheck([this](double) { return tuneHappens; });
    }
    // What the tuned check answers (MainWindow: an active slice now there).
    bool tuneHappens = true;
    QTableView* view() const { return dialog->findChild<QTableView*>("spotListTable"); }
    // The Spot List's row for `call`, as the operator sees the list (the
    // proxy sorts and filters).
    int viewRowOf(const QString& call) const
    {
        const QAbstractItemModel* m = view()->model();
        for (int r = 0; r < m->rowCount(); ++r) {
            if (m->index(r, SpotTableModel::ColDxCall).data().toString() == call) { return r; }
        }
        return -1;
    }
};

DxSpot dxSpot(const QString& call, double servedBearing)
{
    DxSpot s;
    s.dxCall = call;
    s.spotterCall = QStringLiteral("K1TTT");
    s.comment = QStringLiteral("CW");
    s.freqMhz = 14.025;
    s.utcTime = QTime(14, 2);
    s.source = QStringLiteral("Cluster");
    s.bearingDeg = servedBearing;
    return s;
}

} // namespace

class TestSpotBeamTurner : public QObject {
    Q_OBJECT

private:
    DxccColorProvider m_dxcc;

private slots:
    void initTestCase() { QVERIFY(m_dxcc.ensureCtyDatLoaded()); }
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), kGrid);
    }
    void cleanup() { AppSettings::instance().clear(); }

    // ── The turner ─────────────────────────────────────────────────

    void aCoreServedBearingTurnsToThatBearing()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);

        const SpotBeamTurner::Action a = turner.turnAction(kCall, 330.4);
        QCOMPARE(a.text, QStringLiteral("Turn beam to JA1ABC (330") + QChar(0x00B0)
                             + QStringLiteral(")"));
        QVERIFY(a.enabled);
        QVERIFY(a.reason.isEmpty());
        QCOMPARE(a.bearingDeg, 330.4);

        QVERIFY(turner.turnBeam(kCall, 330.4));
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 330.4);
        QCOMPARE(sink.targets.at(0).elevation, -1.0);   // elevation left as it is
        QVERIFY(sink.calls.isEmpty());
    }

    // Carried finding (Task 4a): this computer's own spot sources have no
    // Core-served bearing; the window works it out from its cty.dat and the
    // station's grid square, and the turn goes by callsign.
    void aLocalSpotWorksItsBearingOutHere()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);

        const double expected = localBearing(m_dxcc, kCall, kGrid);
        QVERIFY(expected > 320.0 && expected < 345.0);   // Connecticut to Japan
        QVERIFY(std::abs(turner.bearingFor(kCall, -1.0) - expected) <= 0.05);
        const SpotBeamTurner::Action a = turner.turnAction(kCall, -1.0);
        QCOMPARE(a.text, QStringLiteral("Turn beam to JA1ABC (%1)").arg(wholeDegrees(expected)));
        QVERIFY(a.enabled);

        QVERIFY(turner.turnBeam(QStringLiteral("ja1abc"), -1.0));
        QCOMPARE(sink.calls.size(), 1);
        QCOMPARE(sink.calls.at(0).first, kCall);
        QCOMPARE(sink.calls.at(0).second, false);   // short path
        QVERIFY(sink.targets.isEmpty());

        // FreeDvReporter/GridSquare wins over User/GridSquare, as on the Core.
        AppSettings::instance().setValue(QStringLiteral("FreeDvReporter/GridSquare"),
                                         QStringLiteral("JO62"));
        const double fromBerlin = localBearing(m_dxcc, kCall, QStringLiteral("JO62"));
        QVERIFY(std::abs(turner.bearingFor(kCall, -1.0) - fromBerlin) <= 0.05);
    }

    void withoutARotorTurnBeamIsGreyedWithTheReason_data()
    {
        QTest::addColumn<int>("state");
        QTest::addColumn<QString>("reason");
        QTest::newRow("Core too old") << 0 << kCoreTooOld;
        QTest::newRow("no rotor set up") << 1 << StationRotorController::noRotorReason();
        QTest::newRow("not connected") << 2 << StationRotorController::notConnectedReason();
        QTest::newRow("no rotor object") << 3 << StationRotorController::noRotorReason();
    }
    void withoutARotorTurnBeamIsGreyedWithTheReason()
    {
        QFETCH(int, state);
        QFETCH(QString, reason);
        RotorModel rotor;
        RotorModel::State s = connectedRotor();
        FakeSink sink;
        if (state == 0) {
            sink.available = false;
            sink.unavailableReason = kCoreTooOld;
        } else if (state == 1) {
            s.driver = RotorModel::Driver::None;
            s.connectionPhase = TunerModel::ConnectionPhase::Disabled;
        } else if (state == 2) {
            s.connectionPhase = TunerModel::ConnectionPhase::Connecting;
        }
        rotor.setState(s);
        SpotBeamTurner turner(&sink, state == 3 ? nullptr : &rotor, &m_dxcc);

        const SpotBeamTurner::Action a = turner.turnAction(kCall, 330.0);
        QVERIFY(!a.enabled);
        QCOMPARE(a.reason, reason);
        // The bearing still shows in the label.
        QVERIFY(a.text.endsWith(QStringLiteral("(330") + QChar(0x00B0) + QStringLiteral(")")));

        QSignalSpy refused(&turner, &SpotBeamTurner::turnRefused);
        QString why;
        QVERIFY(!turner.turnBeam(kCall, 330.0, &why));
        QCOMPARE(why, reason);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(sink.sent(), 0);
    }

    void withoutABearingTurnBeamIsGreyedWithTheReason()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);

        // A call cty.dat cannot place.
        SpotBeamTurner::Action a = turner.turnAction(kUnplaceableCall, -1.0);
        QVERIFY(!a.enabled);
        QCOMPARE(a.text, QStringLiteral("Turn beam to QQ1ABC"));
        QCOMPARE(a.reason, StationRotorController::callNotPlacedReason());
        QCOMPARE(a.bearingDeg, -1.0);

        // No grid square: the design's reason.
        AppSettings::instance().remove(QStringLiteral("User/GridSquare"));
        a = turner.turnAction(kCall, -1.0);
        QVERIFY(!a.enabled);
        QCOMPARE(a.text, QStringLiteral("Turn beam to JA1ABC"));
        QCOMPARE(a.reason,
                 QStringLiteral("Set your grid square in Setup to turn the beam to spots."));
        // A grid square that cannot be read is the same.
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), QStringLiteral("ZZ"));
        QCOMPARE(turner.turnAction(kCall, -1.0).reason, StationRotorController::noGridReason());
        QVERIFY(!turner.turnBeam(kCall, -1.0));
        QCOMPARE(sink.sent(), 0);

        // A Core-served bearing needs no grid square here.
        QVERIFY(turner.turnAction(kCall, 12.0).enabled);
    }

    void aRefusalIsSaid()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        sink.accept = false;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        QSignalSpy refused(&turner, &SpotBeamTurner::turnRefused);
        QVERIFY(!turner.turnBeam(kCall, 330.0));
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), sink.refusal);
    }

    // ── Auto-turn ──────────────────────────────────────────────────

    void autoTurnIsOffByDefaultAndTuningNeverMovesTheRotor()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);

        QVERIFY(!SpotBeamTurner::turnOnTune());
        QVERIFY(!AppSettings::instance().contains(QString::fromLatin1(SpotBeamTurner::kTurnOnTuneKey)));
        turner.spotTuned(kCall, 330.0);
        turner.spotTuned(kCall, -1.0);
        QCOMPARE(sink.sent(), 0);

        // Written as the strings "True" / "False".
        SpotBeamTurner::setTurnOnTune(true);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Rotor/TurnOnTune")).toString(),
                 QStringLiteral("True"));
        turner.spotTuned(kCall, 330.0);
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 330.0);
        turner.spotTuned(kCall, -1.0);
        QCOMPARE(sink.calls.size(), 1);

        SpotBeamTurner::setTurnOnTune(false);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Rotor/TurnOnTune")).toString(),
                 QStringLiteral("False"));
        turner.spotTuned(kCall, 330.0);
        QCOMPARE(sink.sent(), 2);
    }

    // With auto-turn on but nothing to turn, a tune says nothing.
    void autoTurnWithNoRotorOrBearingIsQuiet()
    {
        RotorModel rotor;
        RotorModel::State s = connectedRotor();
        s.connectionPhase = TunerModel::ConnectionPhase::Connecting;
        rotor.setState(s);
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        SpotBeamTurner::setTurnOnTune(true);
        QSignalSpy refused(&turner, &SpotBeamTurner::turnRefused);
        turner.spotTuned(kCall, 330.0);
        rotor.setState(connectedRotor());
        turner.spotTuned(kUnplaceableCall, -1.0);
        QCOMPARE(sink.sent(), 0);
        QCOMPARE(refused.count(), 0);
    }

    // A window preference, never the Core's.
    void theSettingIsThisComputers()
    {
        QCOMPARE(classifySettingsKey(QStringLiteral("Rotor/TurnOnTune")),
                 SettingsScope::OperatorLocal);
    }

    void thePreferenceIsInSetupOptions()
    {
        GeneralOptionsPage page(nullptr);
        auto* box = page.findChild<QCheckBox*>(QStringLiteral("chkTurnBeamOnTune"));
        QVERIFY(box);
        QCOMPARE(box->text(), QStringLiteral("Turn the beam when I tune to a spot"));
        QVERIFY(!box->isChecked());
        box->setChecked(true);
        QVERIFY(SpotBeamTurner::turnOnTune());
        box->setChecked(false);
        QVERIFY(!SpotBeamTurner::turnOnTune());
        QVERIFY(OperatorWording::isPlain(box->text()));
        QVERIFY(OperatorWording::isPlain(box->toolTip()));
    }

    // ── The pan's spot menu ────────────────────────────────────────

    void thePanMenuOffersTurnBeamUnderTune()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        SpectrumWidget sw;
        sw.setSpotMarkers({marker(kCall, 330.0)});
        sw.setSpotBeamTurner(&turner);

        QMenu menu;
        sw.buildSpotContextMenuForTest(0, menu);
        const QString turnText =
            QStringLiteral("Turn beam to JA1ABC (330") + QChar(0x00B0) + QStringLiteral(")");
        QCOMPARE(actionTexts(menu),
                 (QStringList{QStringLiteral("Tune to JA1ABC"), turnText,
                              QStringLiteral("Copy Callsign"), QStringLiteral("Lookup on QRZ"),
                              QStringLiteral("--"), QStringLiteral("Remove Spot")}));
        QAction* turn = actionNamed(menu, QStringLiteral("spotTurnBeamAction"));
        QVERIFY(turn);
        QVERIFY(turn->isEnabled());

        QSignalSpy tunes(&sw, &SpectrumWidget::frequencyClicked);
        turn->trigger();
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 330.0);
        QCOMPARE(tunes.count(), 0);   // turning is not tuning
    }

    void thePanMenuGreysTurnBeamWithTheReason()
    {
        RotorModel rotor;
        RotorModel::State s = connectedRotor();
        s.connectionPhase = TunerModel::ConnectionPhase::Error;
        rotor.setState(s);
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        SpectrumWidget sw;
        sw.setSpotMarkers({marker(kCall, 330.0), marker(kUnplaceableCall, -1.0)});
        sw.setSpotBeamTurner(&turner);

        QMenu menu;
        sw.buildSpotContextMenuForTest(0, menu);
        QAction* turn = actionNamed(menu, QStringLiteral("spotTurnBeamAction"));
        QVERIFY(turn);
        QVERIFY(turn->isVisible());
        QVERIFY(!turn->isEnabled());
        QCOMPARE(turn->toolTip(), StationRotorController::notConnectedReason());
        QVERIFY(menu.toolTipsVisible());

        // No bearing, rotor connected.
        rotor.setState(connectedRotor());
        QMenu noBearing;
        sw.buildSpotContextMenuForTest(1, noBearing);
        turn = actionNamed(noBearing, QStringLiteral("spotTurnBeamAction"));
        QVERIFY(turn);
        QCOMPARE(turn->text(), QStringLiteral("Turn beam to QQ1ABC"));
        QVERIFY(!turn->isEnabled());
        QCOMPARE(turn->toolTip(), StationRotorController::callNotPlacedReason());

        // A pan with no turner at all: still there, greyed.
        SpectrumWidget bare;
        bare.setSpotMarkers({marker(kCall, 330.0)});
        QMenu bareMenu;
        bare.buildSpotContextMenuForTest(0, bareMenu);
        turn = actionNamed(bareMenu, QStringLiteral("spotTurnBeamAction"));
        QVERIFY(turn);
        QVERIFY(!turn->isEnabled());
        QCOMPARE(turn->toolTip(), StationRotorController::noRotorReason());
        QCOMPARE(sink.sent(), 0);
    }

    void aMemorySpotKeepsItsOneAction()
    {
        SpectrumWidget sw;
        sw.setSpotMarkers({marker(kCall, 330.0, QStringLiteral("Memory"))});
        QMenu menu;
        sw.buildSpotContextMenuForTest(0, menu);
        QCOMPARE(actionTexts(menu), QStringList{QStringLiteral("Apply JA1ABC")});
    }

    // With auto-turn off, no tune on the pan moves the rotor; with it on,
    // each one turns the beam.
    void tuningOnThePanTurnsOnlyWithAutoTurn()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        SpectrumWidget sw;
        sw.setSpotMarkers({marker(kCall, 330.0)});
        sw.setSpotBeamTurner(&turner);
        connect(&sw, &SpectrumWidget::spotTuned, &turner, &SpotBeamTurner::spotTuned);
        QSignalSpy tunes(&sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy tuned(&sw, &SpectrumWidget::spotTuned);

        QMenu menu;
        sw.buildSpotContextMenuForTest(0, menu);
        QAction* tune = actionStarting(menu, QStringLiteral("Tune to"));
        QVERIFY(tune);
        tune->trigger();
        sw.clickSpotForTest(0);
        QCOMPARE(tunes.count(), 2);
        QCOMPARE(tuned.count(), 2);
        QCOMPARE(tuned.at(0).at(0).toString(), kCall);
        QCOMPARE(tuned.at(0).at(1).toDouble(), 330.0);
        QCOMPARE(sink.sent(), 0);

        SpotBeamTurner::setTurnOnTune(true);
        tune->trigger();
        sw.clickSpotForTest(0);
        QCOMPARE(sink.targets.size(), 2);
        QCOMPARE(sink.targets.at(1).azimuth, 330.0);
    }

    // ── The Spot Hub ───────────────────────────────────────────────

    void theSpotHubHasABearingColumn()
    {
        Hub hub(&m_dxcc);
        hub.table.setBearingResolver([this](const QString& call) {
            return localBearing(m_dxcc, call, kGrid);
        });
        QTableView* view = hub.view();
        QVERIFY(view);
        QHeaderView* header = view->horizontalHeader();
        QCOMPARE(hub.table.headerData(SpotTableModel::ColBearing, Qt::Horizontal,
                                      Qt::DisplayRole).toString(),
                 QStringLiteral("Bearing"));
        // Shown right after DX Call.
        QCOMPARE(header->visualIndex(SpotTableModel::ColBearing),
                 header->visualIndex(SpotTableModel::ColDxCall) + 1);
        QVERIFY(!view->isColumnHidden(SpotTableModel::ColBearing));

        hub.table.addSpot(dxSpot(kUnplaceableCall, -1.0));   // row 2: none
        hub.table.addSpot(dxSpot(kCall, -1.0));              // row 1: this computer's
        hub.table.addSpot(dxSpot(QStringLiteral("DL5XYZ"), 45.2));   // row 0: the Core's
        const auto shown = [&hub](int row) {
            return hub.table.data(hub.table.index(row, SpotTableModel::ColBearing),
                                  Qt::DisplayRole).toString();
        };
        QCOMPARE(shown(0), QStringLiteral("45") + QChar(0x00B0));
        QCOMPARE(shown(1), wholeDegrees(localBearing(m_dxcc, kCall, kGrid)));
        QVERIFY(shown(2).isEmpty());
        QCOMPARE(hub.table.data(hub.table.index(0, SpotTableModel::ColBearing), Qt::ToolTipRole)
                     .toString(),
                 QStringLiteral("45") + QChar(0x00B0) + QStringLiteral(" short path, 225")
                     + QChar(0x00B0) + QStringLiteral(" long path"));
        QCOMPARE(hub.table.servedBearingAtRow(0), 45.2);
        QCOMPARE(hub.table.servedBearingAtRow(1), -1.0);
        // Sorts by number.
        QCOMPARE(hub.table.data(hub.table.index(0, SpotTableModel::ColBearing), Qt::UserRole)
                     .toDouble(),
                 45.2);
    }

    void theSpotHubMenuTunesTurnsCopiesAndLooksUp()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        Hub hub(&m_dxcc);
        hub.dialog->setSpotBeamTurner(&turner);
        hub.table.addSpot(dxSpot(kCall, -1.0));
        hub.table.addSpot(dxSpot(QStringLiteral("DL5XYZ"), 45.0));

        QVERIFY(hub.viewRowOf(QStringLiteral("DL5XYZ")) >= 0);
        QVERIFY(hub.viewRowOf(kCall) >= 0);
        QMenu menu;
        hub.dialog->buildSpotListMenu(hub.viewRowOf(QStringLiteral("DL5XYZ")), menu);
        const QString turnText =
            QStringLiteral("Turn beam to DL5XYZ (45") + QChar(0x00B0) + QStringLiteral(")");
        QCOMPARE(actionTexts(menu),
                 (QStringList{QStringLiteral("Tune to DL5XYZ"), turnText, QStringLiteral("--"),
                              QStringLiteral("Copy Callsign"), QStringLiteral("Lookup on QRZ")}));
        QAction* turn = actionNamed(menu, QStringLiteral("spotListTurnBeamAction"));
        QVERIFY(turn && turn->isEnabled());
        turn->trigger();
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 45.0);

        // This computer's spot: by callsign.
        QMenu local;
        hub.dialog->buildSpotListMenu(hub.viewRowOf(kCall), local);
        actionNamed(local, QStringLiteral("spotListTurnBeamAction"))->trigger();
        QCOMPARE(sink.calls.size(), 1);
        QCOMPARE(sink.calls.at(0).first, kCall);

        // Tune from the menu.
        QSignalSpy tunes(hub.dialog.get(), &SpotHubDialog::tuneRequested);
        QSignalSpy tuned(hub.dialog.get(), &SpotHubDialog::spotTuned);
        actionStarting(menu, QStringLiteral("Tune to"))->trigger();
        QCOMPARE(tunes.count(), 1);
        QCOMPARE(tunes.at(0).at(0).toDouble(), 14.025);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.at(0).at(0).toString(), QStringLiteral("DL5XYZ"));
        QCOMPARE(tuned.at(0).at(1).toDouble(), 45.0);
    }

    void theSpotHubGreysTurnBeamWithTheReason()
    {
        FakeSink sink;
        sink.available = false;
        sink.unavailableReason = kCoreTooOld;
        RotorModel rotor;
        rotor.setState(connectedRotor());
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        Hub hub(&m_dxcc);
        hub.table.addSpot(dxSpot(kCall, 330.0));

        // No turner: greyed, no rotor.
        QMenu bare;
        hub.dialog->buildSpotListMenu(0, bare);
        QAction* turn = actionNamed(bare, QStringLiteral("spotListTurnBeamAction"));
        QVERIFY(turn && !turn->isEnabled());
        QCOMPARE(turn->toolTip(), StationRotorController::noRotorReason());

        hub.dialog->setSpotBeamTurner(&turner);
        QMenu menu;
        hub.dialog->buildSpotListMenu(0, menu);
        turn = actionNamed(menu, QStringLiteral("spotListTurnBeamAction"));
        QVERIFY(turn && !turn->isEnabled());
        QCOMPARE(turn->toolTip(), kCoreTooOld);
        QVERIFY(menu.toolTipsVisible());
        QCOMPARE(sink.sent(), 0);
    }

    // Double-click still tunes, and with auto-turn off never turns.
    void doubleClickStillTunesAndTurnsOnlyWithAutoTurn()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        Hub hub(&m_dxcc);
        hub.dialog->setSpotBeamTurner(&turner);
        connect(hub.dialog.get(), &SpotHubDialog::spotTuned, &turner, &SpotBeamTurner::spotTuned);
        hub.table.addSpot(dxSpot(kCall, 330.0));
        auto* proxy = hub.dialog->findChild<BandFilterProxy*>("spotListProxyModel");
        QVERIFY(proxy);
        QSignalSpy tunes(hub.dialog.get(), &SpotHubDialog::tuneRequested);

        emit hub.view()->doubleClicked(proxy->index(0, SpotTableModel::ColFreq));
        QCOMPARE(tunes.count(), 1);
        QCOMPARE(sink.sent(), 0);

        SpotBeamTurner::setTurnOnTune(true);
        emit hub.view()->doubleClicked(proxy->index(0, SpotTableModel::ColFreq));
        QCOMPARE(tunes.count(), 2);
        QCOMPARE(sink.targets.size(), 1);
        QCOMPARE(sink.targets.at(0).azimuth, 330.0);
    }

    // Final review M7: a tune that did not happen (no active slice) never
    // turns the beam, from a double-click or the menu.
    void aTuneThatDidNotHappenNeverTurns()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        SpotBeamTurner::setTurnOnTune(true);
        Hub hub(&m_dxcc);
        hub.tuneHappens = false;
        hub.dialog->setSpotBeamTurner(&turner);
        connect(hub.dialog.get(), &SpotHubDialog::spotTuned, &turner, &SpotBeamTurner::spotTuned);
        hub.table.addSpot(dxSpot(kCall, 330.0));
        auto* proxy = hub.dialog->findChild<BandFilterProxy*>("spotListProxyModel");
        QVERIFY(proxy);
        QSignalSpy tunes(hub.dialog.get(), &SpotHubDialog::tuneRequested);
        QSignalSpy tuned(hub.dialog.get(), &SpotHubDialog::spotTuned);

        emit hub.view()->doubleClicked(proxy->index(0, SpotTableModel::ColFreq));
        QMenu menu;
        hub.dialog->buildSpotListMenu(0, menu);
        actionStarting(menu, QStringLiteral("Tune to"))->trigger();
        QCOMPARE(tunes.count(), 2);
        QCOMPARE(tuned.count(), 0);
        QCOMPARE(sink.sent(), 0);

        // The same tune that happens turns.
        hub.tuneHappens = true;
        emit hub.view()->doubleClicked(proxy->index(0, SpotTableModel::ColFreq));
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(sink.targets.size(), 1);
        SpotBeamTurner::setTurnOnTune(false);
    }

    void everyStringIsPlain()
    {
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeSink sink;
        SpotBeamTurner turner(&sink, &rotor, &m_dxcc);
        for (const QString& s : {turner.turnAction(kCall, 330.0).text,
                                 turner.turnAction(kUnplaceableCall, -1.0).text,
                                 turner.turnAction(kUnplaceableCall, -1.0).reason}) {
            QVERIFY2(OperatorWording::isPlain(s), qPrintable(s));
        }
    }
};

QTEST_MAIN(TestSpotBeamTurner)
#include "tst_spot_beam_turner.moc"
