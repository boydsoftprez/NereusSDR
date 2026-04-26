// tst_radio_model_pa_tripped.cpp
//
// no-port-check: Test file exercises NereusSDR API; Thetis behavior is
// cited in RadioModel.cpp — no C# is translated here.
//
// Unit tests for RadioModel::paTripped() live state and the Ganymede PA
// trip handler. Exercises:
//   - initial_paTripped_isFalse     — default state is clear
//   - ganymedeTripMessage_setsPaTripped  — non-zero tripState latches trip
//   - ganymedeReset_clearsPaTripped — resetGanymedePa() clears the latch
//   - ganymedeAbsent_clearsPaTripped— setGanymedePresent(false) clears latch
//   - ganymedeTripMessage_dropsMoxIfActive — safety MOX drop on trip latch
//                                         per Andromeda.cs:920 [v2.10.3.13]
//
// From Thetis Andromeda/Andromeda.cs:914-948 [v2.10.3.13]
// (CATHandleAmplifierTripMessage + GanymedeResetPressed).
// G8NJJ: handlers for Ganymede 500W PA protection.

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestRadioModelPaTripped : public QObject {
    Q_OBJECT

private slots:
    void initial_paTripped_isFalse();
    void ganymedeTripMessage_setsPaTripped();
    void ganymedeReset_clearsPaTripped();
    void ganymedeAbsent_clearsPaTripped();
    void ganymedeTripMessage_dropsMoxIfActive();

    // Codex P1+P2 follow-ups to PR #139:
    void secondTripWhileMoxReasserted_dropsMoxAgain();
    void radioStatusPowerChanged_doesNotIngest_intoSwrController();
};

// ── Test implementations ─────────────────────────────────────────────────────

void TestRadioModelPaTripped::initial_paTripped_isFalse()
{
    RadioModel model;
    QVERIFY(!model.paTripped());
}

void TestRadioModelPaTripped::ganymedeTripMessage_setsPaTripped()
{
    RadioModel model;
    QSignalSpy spy(&model, &RadioModel::paTrippedChanged);

    model.handleGanymedeTrip(0x01);

    QVERIFY(model.paTripped());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void TestRadioModelPaTripped::ganymedeReset_clearsPaTripped()
{
    RadioModel model;
    model.handleGanymedeTrip(0x01);
    QVERIFY(model.paTripped());

    QSignalSpy spy(&model, &RadioModel::paTrippedChanged);
    model.resetGanymedePa();

    QVERIFY(!model.paTripped());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}

void TestRadioModelPaTripped::ganymedeAbsent_clearsPaTripped()
{
    RadioModel model;
    model.handleGanymedeTrip(0x08); // heatsink trip
    QVERIFY(model.paTripped());

    QSignalSpy spy(&model, &RadioModel::paTrippedChanged);
    model.setGanymedePresent(false);

    QVERIFY(!model.paTripped());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}

void TestRadioModelPaTripped::ganymedeTripMessage_dropsMoxIfActive()
{
    // Safety side-effect from Andromeda.cs:920 [v2.10.3.13]:
    //   if (_ganymede_pa_issue && MOX) MOX = false; //if there is a fault, undo mox if active
    // G8NJJ: handlers for Ganymede 500W PA protection
    RadioModel model;
    model.transmitModel().setMox(true);
    QVERIFY(model.transmitModel().isMox());

    model.handleGanymedeTrip(0x01);
    QVERIFY(model.paTripped());
    QVERIFY(!model.transmitModel().isMox()); // safety drop per Andromeda.cs:920
}

// Codex P2 follow-up to PR #139: a second non-zero trip CAT message while the
// trip latch is already asserted MUST still drop MOX. Previously the
// idempotent-state guard returned before the safety MOX-drop, so a user who
// manually re-keyed mid-fault would stay on TX through the next CAT trip.
void TestRadioModelPaTripped::secondTripWhileMoxReasserted_dropsMoxAgain()
{
    RadioModel model;

    // First trip: MOX on → latches and drops MOX.
    model.transmitModel().setMox(true);
    model.handleGanymedeTrip(0x01);
    QVERIFY(model.paTripped());
    QVERIFY(!model.transmitModel().isMox());

    // User manually re-keys mid-fault.
    model.transmitModel().setMox(true);
    QVERIFY(model.transmitModel().isMox());

    // Second trip CAT message arrives. Trip state is unchanged (still asserted),
    // but the safety MOX-drop must fire again.
    QSignalSpy spy(&model, &RadioModel::paTrippedChanged);
    model.handleGanymedeTrip(0x02); // drain-current trip — different sub-cause, still asserted
    QVERIFY(model.paTripped());
    QVERIFY(!model.transmitModel().isMox()); // dropped again per Andromeda.cs:920
    QCOMPARE(spy.count(), 0); // no transition signal — already tripped
}

// Codex P1 follow-up to PR #139: SwrProtectionController must NOT ingest
// from RadioStatus::powerChanged (which emits twice per hardware sample,
// once per setForwardPower / setReflectedPower setter). PA telemetry is
// routed via the per-sample paTelemetryUpdated handler instead. This
// regression test sets fwd/rev directly through the RadioStatus setters
// and confirms the controller's protectFactor is unchanged — proving the
// powerChanged → ingest connection is no longer in place.
void TestRadioModelPaTripped::radioStatusPowerChanged_doesNotIngest_intoSwrController()
{
    RadioModel model;
    auto& swr = model.swrProt();

    // Enable protection so any ingest would actually move state.
    swr.setEnabled(true);
    swr.setLimit(2.0f);
    swr.setWindBackEnabled(false);

    const float baselineFactor = swr.protectFactor();
    QCOMPARE(baselineFactor, 1.0f);

    // Drive RadioStatus directly with values that, if ingested, would push
    // SWR ~3 (rev/fwd = 0.25 → ρ=0.5 → SWR=3) and engage per-sample foldback.
    auto& status = model.radioStatus();
    status.setForwardPower(100.0);
    status.setReflectedPower(25.0);

    // Repeat to exercise more than the 4-trip debounce. With the bug, this
    // would latch / change protectFactor; without the bug, factor stays at 1.0.
    for (int i = 0; i < 10; ++i) {
        status.setForwardPower(100.0 + i * 0.001); // qFuzzyCompare guard tolerates this
        status.setReflectedPower(25.0 + i * 0.001);
    }

    QCOMPARE(swr.protectFactor(), 1.0f);
    QVERIFY(!swr.highSwr());
    QVERIFY(!swr.windBackLatched());
}

QTEST_MAIN(TestRadioModelPaTripped)
#include "tst_radio_model_pa_tripped.moc"
