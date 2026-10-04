// no-port-check: NereusSDR-original PS3 applet/session-facade wiring tests.
// =================================================================
// tests/tst_applet_ps_wiring.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-06 — Added Phase 3M-4 PureSignal applet wiring coverage.
//   2026-09-22 — Migrated coverage to the shared local/remote PS3 session
//                 facade and station-owned correction assets.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include <functional>

#include "core/AppSettings.h"
#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/DspAssetDialog.h"
#include "gui/HGauge.h"
#include "gui/applets/PureSignalApplet.h"
#include "gui/applets/TxApplet.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

struct CapturedRequest {
    quint32 id{0};
    Ps3Action action{Ps3Action::OffReset};
    QVariantMap arguments;
};

struct RemotePs3Harness {
    RadioModel radio{RadioModel::Role::Remote};
    PureSignalSessionFacade* facade{radio.pureSignalFacade()};
    QList<CapturedRequest> requests;
    quint32 nextId{40};

    RemotePs3Harness()
    {
        facade->setRemoteRequestHandler(
            [this](Ps3Action action, const QVariantMap& arguments) {
                const quint32 id = nextId++;
                requests.append({id, action, arguments});
                return id;
            });
        facade->setRemoteCapabilities(true, true);
        facade->applyRemoteProperty("available", true);
        facade->applyRemoteProperty("canActuate", true);
    }
};

QString statusJson(const std::function<void(QJsonObject&)>& edit)
{
    RadioModel source;
    source.installPureSignalForTest(nullptr);
    QJsonObject status = QJsonDocument::fromJson(
        source.pureSignalFacade()->statusJson().toUtf8()).object();
    edit(status);
    return QString::fromUtf8(
        QJsonDocument(status).toJson(QJsonDocument::Compact));
}

QPushButton* button(QWidget& parent, const char* objectName)
{
    return parent.findChild<QPushButton*>(QString::fromLatin1(objectName));
}

} // namespace

class TstAppletPsWiring : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
    }

    void pureSignalApplet_constructsWithExpectedControls()
    {
        RadioModel radio;
        PureSignalApplet applet(&radio);
        QVERIFY(button(applet, "PsAppletCalibrateBtn"));
        QVERIFY(button(applet, "PsAppletAutoCalBtn"));
        QVERIFY(button(applet, "PsAppletSaveBtn"));
        QVERIFY(button(applet, "PsAppletRestoreBtn"));
        QVERIFY(button(applet, "PsAppletTwoToneBtn"));
    }

    void pureSignalApplet_routesActionsThroughSharedFacade()
    {
        RemotePs3Harness harness;
        PureSignalApplet applet(&harness.radio);

        button(applet, "PsAppletCalibrateBtn")->click();
        QCOMPARE(harness.requests.size(), 1);
        QCOMPARE(harness.requests.last().action, Ps3Action::Single);

        auto* automatic = button(applet, "PsAppletAutoCalBtn");
        automatic->click();
        QCOMPARE(harness.requests.size(), 2);
        QCOMPARE(harness.requests.last().action, Ps3Action::StartAutomatic);

        auto* twoTone = button(applet, "PsAppletTwoToneBtn");
        twoTone->click();
        QCOMPARE(harness.requests.size(), 3);
        QCOMPARE(harness.requests.last().action, Ps3Action::SetTwoTone);
        QCOMPARE(harness.requests.last().arguments.value("enabled").toBool(), true);

        // Accepted work stays pending until the station replies. Pending and
        // terminal replies update presentation without replaying the action.
        const CapturedRequest automaticRequest = harness.requests[1];
        harness.facade->receiveRemoteActionResult(
            automaticRequest.id, "ps3.automatic", Ps3ActionPhase::Pending, {}, {});
        QCOMPARE(harness.requests.size(), 3);
        harness.radio.pureSignalSettings()->initializeAutoCalPreference(true);
        harness.facade->receiveRemoteActionResult(
            automaticRequest.id, "ps3.automatic", Ps3ActionPhase::Completed, {}, {});
        QCOMPARE(harness.requests.size(), 3);
        QVERIFY(automatic->isChecked());
    }

    void pureSignalApplet_consumesPs3StatusSemantics()
    {
        RemotePs3Harness harness;
        PureSignalApplet applet(&harness.radio);
        const QString status = statusJson([](QJsonObject& object) {
            object["feedbackLevel"] = 150;              // info[4]
            object["attemptedCalibrations"] = 9;
            object["successfulCalibrations"] = 7;      // info[5]
            object["correctionsApplied"] = true;        // info[14]
            object["correctionSummaryValid"] = true;
            object["correctionGainAtPeak"] = 0.8125;
            object["correctionPhaseSpanDegrees"] = 3.25;
            object["pairedInputValid"] = true;
            object["pumpActive"] = true;
            object["txMonitorPeak"] = 0.3;
            object["feedbackPeak"] = 0.2;
            object["engineState"] = 4;                  // info[15], LCOLLECT
            object["psEnabled"] = true;
            object["mox"] = true;
        });
        QVERIFY(harness.facade->applyRemoteProperty("statusJson", status));

        auto* feedback = applet.findChild<HGauge*>(
            QStringLiteral("PsAppletFeedbackGauge"));
        auto* correction = applet.findChild<HGauge*>(
            QStringLiteral("PsAppletCorrectionGauge"));
        auto* iterations = applet.findChild<QLabel*>(
            QStringLiteral("PsAppletIterationsLabel"));
        auto* feedbackLabel = applet.findChild<QLabel*>(
            QStringLiteral("PsAppletFeedbackDbLabel"));
        QVERIFY(feedback && correction && iterations && feedbackLabel);
        QVERIFY(qAbs(feedback->value() - 150.0 * 100.0 / 255.0) < 0.1);
        QCOMPARE(correction->value(), 100.0);
        QVERIFY(!correction->isUnavailable());
        auto* correctionLabel = applet.findChild<QLabel*>("PsAppletCorrectionDbLabel");
        QVERIFY(correctionLabel);
        QCOMPARE(correctionLabel->text(), QStringLiteral("Correction: Applied"));
        // Scalar telemetry remains available without an added gain/phase display.
        QCOMPARE(harness.facade->statusSnapshot().correctionGainAtPeak, 0.8125);
        QCOMPARE(iterations->text(), QStringLiteral("Calibrations: 7 / 9 attempts"));
        QCOMPARE(feedbackLabel->text(), QStringLiteral("Feedback: 150 (raw)"));
        QVERIFY(button(applet, "PsAppletSaveBtn")->isEnabled());

        for (const char* name : {"PsAppletCalLed", "PsAppletRunLed", "PsAppletFbkLed"}) {
            auto* led = applet.findChild<QLabel*>(QString::fromLatin1(name));
            QVERIFY(led);
            QVERIFY(led->styleSheet().contains(QStringLiteral("#20c060")));
        }
    }

    void correctionStatusDoesNotDependOnOptionalGainTelemetry()
    {
        RemotePs3Harness harness;
        PureSignalApplet applet(&harness.radio);
        auto* correction = applet.findChild<HGauge*>("PsAppletCorrectionGauge");
        auto* label = applet.findChild<QLabel*>("PsAppletCorrectionDbLabel");
        QVERIFY(correction && label);
        const QString legacy = statusJson([](QJsonObject& object) {
            object["feedbackLevel"] = 144;
            object["correctionsApplied"] = true;
            object["hardwarePeak"] = 0.6121;
            object["mox"] = true;
            for (const char* name : {"correctionSummaryValid", "correctionGainAtPeak",
                    "correctionPhaseSpanDegrees", "pairedInputValid", "txMonitorPeak", "feedbackPeak"}) {
                object.remove(name);
            }
        });
        QVERIFY(harness.facade->applyRemoteProperty("statusJson", legacy));
        QVERIFY(harness.facade->statusSnapshot().correctionsApplied);
        QVERIFY(!harness.facade->statusSnapshot().correctionSummaryValid);
        QVERIFY(!correction->isUnavailable());
        QCOMPARE(correction->value(), 100.0);
        QCOMPARE(label->text(), QStringLiteral("Correction: Applied"));
        QVERIFY(!label->text().contains("0.6121"));
        const QString invalid = statusJson([](QJsonObject& object) {
            object["correctionsApplied"] = true;
            object["correctionSummaryValid"] = true;
            object["correctionGainAtPeak"] = -0.5;
            object["correctionPhaseSpanDegrees"] = 2.0;
            object["pairedInputValid"] = true;
            object["txMonitorPeak"] = 0.2;
            object["feedbackPeak"] = -0.1;
        });
        QVERIFY(harness.facade->applyRemoteProperty("statusJson", invalid));
        QVERIFY(!harness.facade->statusSnapshot().correctionSummaryValid);
        QVERIFY(!harness.facade->statusSnapshot().pairedInputValid);
        QVERIFY(!correction->isUnavailable());
        QCOMPARE(correction->value(), 100.0);
        QCOMPARE(label->text(), QStringLiteral("Correction: Applied"));
    }

    void pureSignalApplet_isReadOnlyWithoutRemoteActuationPermission()
    {
        RemotePs3Harness harness;
        harness.facade->setRemoteCapabilities(true, false);
        PureSignalApplet applet(&harness.radio);
        harness.radio.pureSignalSettings()->initializeAutoCalPreference(true);

        auto* automatic = button(applet, "PsAppletAutoCalBtn");
        QVERIFY(automatic->isChecked());
        QVERIFY(!automatic->isEnabled());
        QVERIFY(!button(applet, "PsAppletCalibrateBtn")->isEnabled());
        QVERIFY(!button(applet, "PsAppletTwoToneBtn")->isEnabled());
        QVERIFY(button(applet, "PsAppletRestoreBtn")->isEnabled());
        QCOMPARE(harness.requests.size(), 0);
    }

    void pureSignalApplet_restoreUsesStationAssetId()
    {
        RemotePs3Harness harness;
        PureSignalApplet applet(&harness.radio);
        button(applet, "PsAppletRestoreBtn")->click();
        auto* dialog = applet.findChild<DspAssetDialog*>();
        QVERIFY(dialog);

        emit dialog->restoreCorrectionRequested(QStringLiteral("ps3-asset-42"));
        QCOMPARE(harness.requests.size(), 1);
        QCOMPARE(harness.requests.last().action, Ps3Action::RestoreCorrection);
        QVariantMap expected;
        expected.insert(QStringLiteral("assetId"), QStringLiteral("ps3-asset-42"));
        QCOMPARE(harness.requests.last().arguments, expected);
        dialog->close();
    }

    void contextMenusOpenDialogWithoutActuating()
    {
        RemotePs3Harness harness;
        PureSignalApplet pureSignal(&harness.radio);
        TxApplet tx(&harness.radio);

        QSignalSpy pureSignalOpen(
            &pureSignal, &PureSignalApplet::openPureSignalDialogRequested);
        QSignalSpy txOpen(&tx, &TxApplet::openPureSignalDialogRequested);
        auto* calibrate = button(pureSignal, "PsAppletCalibrateBtn");
        auto* psa = button(tx, "TxAppletPsaBtn");
        emit calibrate->customContextMenuRequested(QPoint{});
        emit psa->customContextMenuRequested(QPoint{});
        QCOMPARE(pureSignalOpen.size(), 1);
        QCOMPARE(txOpen.size(), 1);
        QCOMPARE(harness.requests.size(), 0);
    }

    void txApplet_psaVisibilityAndFacadeActions()
    {
        RemotePs3Harness harness;
        TxApplet applet(&harness.radio);
        auto* psa = button(applet, "TxAppletPsaBtn");
        QVERIFY(psa);

        BoardCapabilities capabilities{};
        // Fix round 1 (minor 5): a known board without PureSignal hides it.
        capabilities.board = HPSDRHW::Atlas;
        capabilities.hasPureSignal = false;
        applet.setBoardCapabilities(capabilities);
        QVERIFY(psa->isHidden());

        capabilities.hasPureSignal = true;
        applet.setBoardCapabilities(capabilities);
        QVERIFY(!psa->isHidden());
        applet.setTransmitPermitted(true);
        QVERIFY(psa->isEnabled());

        psa->click();
        QCOMPARE(harness.requests.size(), 1);
        QCOMPARE(harness.requests.last().action, Ps3Action::StartAutomatic);
        harness.radio.pureSignalSettings()->initializeAutoCalPreference(true);
        QVERIFY(psa->isChecked());

        psa->click();
        QCOMPARE(harness.requests.size(), 2);
        QCOMPARE(harness.requests.last().action, Ps3Action::OffReset);
    }

    void txApplet_psaRepeatedRefusalsRestoreAuthoritativeState()
    {
        RemotePs3Harness harness;
        TxApplet applet(&harness.radio);
        BoardCapabilities capabilities{};
        capabilities.board = HPSDRHW::OrionMKII;
        capabilities.hasPureSignal = true;
        applet.setBoardCapabilities(capabilities);
        applet.setTransmitPermitted(true);
        auto* psa = button(applet, "TxAppletPsaBtn");
        QVERIFY(psa);
        for (int attempt = 0; attempt < 2; ++attempt) {
            psa->click();
            const CapturedRequest request = harness.requests.last();
            QCOMPARE(request.action, Ps3Action::StartAutomatic);
            harness.facade->receiveRemoteActionResult(request.id, "ps3.automatic",
                Ps3ActionPhase::Failed, QStringLiteral("A calibration is pending."), {});
            QVERIFY(!psa->isChecked());
        }
    }

    // Fix round 1 (minor 5): with no radio connected the board is not
    // known (the caps fall back to Unknown): PS-A shows, disabled, with
    // the reason, until the board is known. Only a known board without
    // PureSignal hides it.
    void txApplet_psaShowsDisabledUntilTheBoardIsKnown()
    {
        RemotePs3Harness harness;
        TxApplet applet(&harness.radio);
        applet.setTransmitPermitted(true);
        auto* psa = button(applet, "TxAppletPsaBtn");
        QVERIFY(psa);
        const QString ownTip = psa->toolTip();

        BoardCapabilities capabilities{};
        QCOMPARE(capabilities.board, HPSDRHW::Unknown);
        QVERIFY(!capabilities.hasPureSignal);
        applet.setBoardCapabilities(capabilities);
        QVERIFY(!psa->isHidden());
        QVERIFY(!psa->isEnabled());
        QCOMPARE(psa->toolTip(),
                 QStringLiteral("PureSignal needs a connected radio that supports it."));
        QCOMPARE(psa->accessibleDescription(),
                 QStringLiteral("PureSignal needs a connected radio that supports it."));
        psa->click();
        QCOMPARE(harness.requests.size(), 0);

        capabilities.board = HPSDRHW::Atlas;
        applet.setBoardCapabilities(capabilities);
        QVERIFY(psa->isHidden());

        capabilities.board = HPSDRHW::OrionMKII;
        capabilities.hasPureSignal = true;
        applet.setBoardCapabilities(capabilities);
        QVERIFY(!psa->isHidden());
        QVERIFY(psa->isEnabled());
        QCOMPARE(psa->toolTip(), ownTip);
    }

    void txApplet_psaShowsReadbackButRefusesRemoteActuation()
    {
        RemotePs3Harness harness;
        harness.facade->setRemoteCapabilities(true, false);
        harness.radio.pureSignalSettings()->initializeAutoCalPreference(true);
        TxApplet applet(&harness.radio);
        applet.setTransmitPermitted(true);

        auto* psa = button(applet, "TxAppletPsaBtn");
        QVERIFY(psa->isChecked());
        QVERIFY(!psa->isEnabled());
        QCOMPARE(harness.requests.size(), 0);
    }

    void setPureSignal_reusesRadioModelsFacade()
    {
        RadioModel radio;
        PureSignalSessionFacade* const shared = radio.pureSignalFacade();
        TxChannel tx(1);
        PureSignal coordinator(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        coordinator.setSettings(radio.pureSignalSettings());

        PureSignalApplet pureSignal(&radio);
        TxApplet txApplet(&radio);
        pureSignal.setPureSignal(&coordinator);
        txApplet.setPureSignal(&coordinator);

        QCOMPARE(radio.pureSignalFacade(), shared);
        QVERIFY(shared->available());
        radio.pureSignalSettings()->initializeAutoCalPreference(true);
        QVERIFY(button(pureSignal, "PsAppletAutoCalBtn")->isChecked());
        QVERIFY(button(txApplet, "TxAppletPsaBtn")->isChecked());
    }
};

QTEST_MAIN(TstAppletPsWiring)
#include "tst_applet_ps_wiring.moc"
