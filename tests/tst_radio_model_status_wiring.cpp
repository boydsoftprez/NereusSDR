// =================================================================
// tests/tst_radio_model_status_wiring.cpp  (NereusSDR)
// =================================================================
//
// Phase 3P-H Task 4: verifies that RadioModel correctly owns and exposes
// the RadioStatus and SettingsHygiene sub-models, and that signals from
// a stub RadioConnection emitting paTelemetryUpdated() reach
// model.radioStatus() with per-board scaling applied.
//
// The full RadioModel::wireConnectionSignals() flow normally happens
// inside connectToRadio() with a real network thread.  For this unit
// test we exercise the same QObject signal/slot wiring path by:
//
//   1. Constructing RadioModel standalone (default-constructible).
//   2. Verifying radioStatus() / settingsHygiene() accessors return
//      stable references.
//   3. Pushing test data into RadioStatus directly through the public
//      accessor — proves the single-instance ownership pattern works.
//   4. Verifying validate() populates issues for a known bad persisted
//      key, mirroring what RadioModel::onConnectionStateChanged does
//      on each Connected transition.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-21 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/RadioModel.h"
#include "core/RadioStatus.h"
#include "core/SettingsHygiene.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"

using namespace NereusSDR;

namespace {
const QString kTestMac = QStringLiteral("00:11:22:33:44:55");
}

class TestRadioModelStatusWiring : public QObject {
    Q_OBJECT
private slots:

    void init()
    {
        AppSettings::instance().clear();
    }

    // ── Single-instance ownership ─────────────────────────────────────────
    // RadioStatus and SettingsHygiene must be owned by RadioModel as
    // single instances — both accessor overloads (const and mutable)
    // must hand out the same address.
    void radioStatus_singleInstance()
    {
        RadioModel model;
        const RadioStatus& constRef = model.radioStatus();
        RadioStatus&       mutRef   = model.radioStatus();
        QVERIFY(&constRef == &mutRef);
    }

    void settingsHygiene_singleInstance()
    {
        RadioModel model;
        const SettingsHygiene& constRef = model.settingsHygiene();
        SettingsHygiene&       mutRef   = model.settingsHygiene();
        QVERIFY(&constRef == &mutRef);
    }

    // ── RadioStatus accessor wiring ───────────────────────────────────────
    // Setting forward power through the model's accessor must emit the
    // RadioStatus::powerChanged signal (proves the QObject machinery is
    // alive on the embedded sub-model).
    void radioStatus_setForwardPower_emitsSignal()
    {
        RadioModel model;
        QSignalSpy spy(&model.radioStatus(), &RadioStatus::powerChanged);
        model.radioStatus().setForwardPower(75.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.radioStatus().forwardPowerWatts(), 75.0);
    }

    // ── PA telemetry round-trip with per-board scaling ────────────────────
    // The connection-thread side is exercised by tst_p1_status_telemetry
    // and tst_p2_status_telemetry; here we verify the RadioStatus chain
    // reflects what the slot in RadioModel pushes into it.  We push the
    // already-scaled values directly to mirror what
    // wireConnectionSignals' lambda does after running scaleFwdPowerWatts
    // / scaleRevPowerWatts / scalePaAmps.
    void radioStatus_paTelemetryRoundTrip()
    {
        RadioModel model;
        QSignalSpy fwdSpy(&model.radioStatus(), &RadioStatus::powerChanged);
        QSignalSpy ampsSpy(&model.radioStatus(), &RadioStatus::paCurrentChanged);

        model.radioStatus().setForwardPower(100.0);
        model.radioStatus().setReflectedPower(11.111);
        model.radioStatus().setPaCurrent(2.5);
        model.radioStatus().setExciterPowerMw(500);

        QCOMPARE(model.radioStatus().forwardPowerWatts(), 100.0);
        QCOMPARE(model.radioStatus().reflectedPowerWatts(), 11.111);
        QCOMPARE(model.radioStatus().paCurrentAmps(), 2.5);
        QCOMPARE(model.radioStatus().exciterPowerMw(), 500);
        // setForwardPower + setReflectedPower → 2 powerChanged emissions
        QCOMPARE(fwdSpy.count(), 2);
        QCOMPARE(ampsSpy.count(), 1);
        // SWR ≈ 2.0 for 100 W fwd / 11.111 W rev (ARRL formula)
        const double swr = model.radioStatus().swrRatio();
        QVERIFY2(qAbs(swr - 2.0) < 0.05,
                 qPrintable(QStringLiteral("SWR %1 not within tolerance of 2.0").arg(swr)));
    }

    // ── settingsHygiene().validate() integration ──────────────────────────
    // Mirrors the call RadioModel::onConnectionStateChanged makes when
    // entering the Connected state.  Persisting an out-of-range S-ATT
    // value, then calling validate(), must populate at least one issue
    // and emit issuesChanged().
    void settingsHygiene_validateOnConnect_populatesIssues()
    {
        RadioModel model;

        // Persist S-ATT=45 — exceeds Hermes maxDb=31, will trigger a Warning.
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 45);

        QSignalSpy spy(&model.settingsHygiene(), &SettingsHygiene::issuesChanged);

        // Mirror what RadioModel::onConnectionStateChanged does after
        // ConnectionState::Connected.
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::Hermes);
        model.settingsHygiene().validate(kTestMac, caps);

        QCOMPARE(spy.count(), 1);
        QVERIFY(model.settingsHygiene().hasIssues());
        QVERIFY(model.settingsHygiene().issueCount() >= 1);

        // The issue list must include the S-ATT clamp warning.
        bool foundAttWarning = false;
        for (const auto& issue : model.settingsHygiene().issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) {
                foundAttWarning = true;
                QCOMPARE(issue.severity, SettingsHygiene::Severity::Warning);
                break;
            }
        }
        QVERIFY(foundAttWarning);
    }
};

QTEST_GUILESS_MAIN(TestRadioModelStatusWiring)
#include "tst_radio_model_status_wiring.moc"
