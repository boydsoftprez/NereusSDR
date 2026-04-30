// tests/tst_speech_processor_page.cpp  (NereusSDR)
//
// Phase 3M-3a-i Batch 5 (Task E) — SpeechProcessorPage TX dashboard.
// Phase 3M-3a-ii Batch 5 (Task E) — Phrot / CFC / CESSB live bindings.
//
// no-port-check: NereusSDR-original test file.  SpeechProcessorPage is a
// NereusSDR-spin (no direct Thetis equivalent) — see the header comment in
// TransmitSetupPages.h "Speech Processor — TX dashboard" for the WDSP TXA
// stage references.
//
// Coverage:
//   1. Page constructs without crash with default RadioModel.
//   2. Initial state: TX EQ status reflects TransmitModel::txEqEnabled (default=false).
//   3. Initial state: Leveler status reflects TransmitModel::txLevelerOn (default=true).
//   4. Toggling TransmitModel::setTxEqEnabled(true) updates the TX EQ label live.
//   5. Toggling TransmitModel::setTxLevelerOn(false) updates the Leveler label live.
//   6. Active profile label reads "Default" before any radio connects.
//   7. Active profile label updates when MicProfileManager::activeProfileChanged emits.
//   8. ALC row is hard-labelled "always-on" (no signal wiring).
//   9. Cross-link buttons emit openSetupRequested(category, page) with the
//      correct setup-page leaf labels (AGC/ALC, CFC, VOX/DEXP).
//   10. Phase Rotator label reflects initial TM defaults — "OFF" because
//       phaseRotatorEnabled defaults to false.
//   11. Phrot label updates live on phaseRotatorEnabledChanged + freq/stages.
//   12. CFC label reflects initial TM default — "OFF" (cfcEnabled=false).
//   13. CFC label updates live on cfcEnabledChanged.
//   14. CESSB off-text shows "(gated on CPDR)" when cpdrOn=false.
//   15. CESSB label updates live on cessbOnChanged + cpdrOnChanged.

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestSpeechProcessorPage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. Construction smoke ─────────────────────────────────────────────────
    void construct_doesNotCrash()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();
        QVERIFY(true);   // Survived construction + show().
    }

    // ── 2. Initial TX EQ state mirrors TransmitModel::txEqEnabled (false default)
    void initial_txEqLabel_readsFromModel()
    {
        RadioModel model;
        QCOMPARE(model.transmitModel().txEqEnabled(), false);  // sanity

        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_TX EQ"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("off"));
    }

    // ── 3. Initial Leveler state mirrors TransmitModel::txLevelerOn (true default)
    void initial_levelerLabel_readsFromModel()
    {
        RadioModel model;
        QCOMPARE(model.transmitModel().txLevelerOn(), true);   // sanity

        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_Leveler"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("enabled"));
    }

    // ── 4. Live: toggling TX EQ flips the dashboard label ────────────────────
    void txEqLabel_updatesOnSignal()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_TX EQ"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("off"));

        model.transmitModel().setTxEqEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("enabled"));

        model.transmitModel().setTxEqEnabled(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("off"));
    }

    // ── 5. Live: toggling Leveler flips the dashboard label ──────────────────
    void levelerLabel_updatesOnSignal()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_Leveler"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("enabled"));

        model.transmitModel().setTxLevelerOn(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("off"));

        model.transmitModel().setTxLevelerOn(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("enabled"));
    }

    // ── 6. Active profile label reads "Default" before any radio connects ────
    //
    // RadioModel constructs MicProfileManager in its ctor (RadioModel.cpp:776
    // [v0.2.3]) but doesn't call setMacAddress() until connectToRadio runs.
    // Per MicProfileManager::activeProfileName (MicProfileManager.cpp:603) the
    // unscoped manager returns "Default".
    void activeProfileLabel_defaultsToDefault()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblActiveProfile"));
        QVERIFY(lbl);
        QCOMPARE(lbl->text(), QStringLiteral("Default"));
    }

    // ── 7. Active profile label updates on MicProfileManager signal ──────────
    void activeProfileLabel_updatesOnSignal()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblActiveProfile"));
        QVERIFY(lbl);

        auto* mgr = model.micProfileManager();
        QVERIFY(mgr);

        // Direct emit — exercises the connection without needing AppSettings
        // round-trip (the label trusts whatever string the signal carries).
        emit mgr->activeProfileChanged(QStringLiteral("DX-Voice"));
        QCoreApplication::processEvents();
        QCOMPARE(lbl->text(), QStringLiteral("DX-Voice"));
    }

    // ── 8. ALC row is statically labelled "always-on" ────────────────────────
    void alcRow_isStaticAlwaysOn()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_ALC"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("always-on"));
    }

    // ── 9. Cross-link buttons emit openSetupRequested ────────────────────────
    //
    // Verifies the deep-link signal contract.  After 3M-3a-ii Batch 5, the
    // Phrot / CFC / CESSB buttons all route to "CFC" (co-located on the CFC
    // Setup page); the Leveler button routes to "AGC/ALC"; AM-SQ/DEXP routes
    // to "VOX/DEXP".
    void crossLinkButtons_emitOpenSetupRequested()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        QSignalSpy spy(&page, &SpeechProcessorPage::openSetupRequested);

        // Leveler row → AGC/ALC.
        auto* btnLev = page.findChild<QPushButton*>(QStringLiteral("btn_Leveler"));
        QVERIFY(btnLev);
        QVERIFY(btnLev->isEnabled());
        btnLev->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toString(), QStringLiteral("AGC/ALC"));

        // Phase Rotator row → CFC (co-located on CFC Setup page).
        auto* btnPhrot = page.findChild<QPushButton*>(QStringLiteral("btn_Phase Rot."));
        QVERIFY(btnPhrot);
        QVERIFY(btnPhrot->isEnabled());
        btnPhrot->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toString(), QStringLiteral("CFC"));

        // CFC row → CFC.
        auto* btnCfc = page.findChild<QPushButton*>(QStringLiteral("btn_CFC"));
        QVERIFY(btnCfc);
        QVERIFY(btnCfc->isEnabled());
        btnCfc->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toString(), QStringLiteral("CFC"));

        // CESSB row → CFC (co-located on CFC Setup page).
        auto* btnCessb = page.findChild<QPushButton*>(QStringLiteral("btn_CESSB"));
        QVERIFY(btnCessb);
        QVERIFY(btnCessb->isEnabled());
        btnCessb->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toString(), QStringLiteral("CFC"));

        // AM-SQ / DEXP row → VOX/DEXP.
        auto* btnDexp = page.findChild<QPushButton*>(QStringLiteral("btn_AM-SQ / DEXP"));
        QVERIFY(btnDexp);
        QVERIFY(btnDexp->isEnabled());
        btnDexp->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(1).toString(), QStringLiteral("VOX/DEXP"));
    }

    // ── 10. Phrot status label initial — "OFF" (default phaseRotatorEnabled=false) ─
    void phrotLabel_defaultsToOff()
    {
        RadioModel model;
        QCOMPARE(model.transmitModel().phaseRotatorEnabled(), false);  // sanity

        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_Phase Rot."));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("OFF"));
    }

    // ── 11. Phrot label updates live on enable + freq + stages ───────────────
    void phrotLabel_updatesOnSignals()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_Phase Rot."));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("OFF"));

        // Enable → "ON · 338 Hz · 8 stages" (defaults).
        model.transmitModel().setPhaseRotatorEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON · 338 Hz · 8 stages"));

        // Tweak freq → label re-renders.
        model.transmitModel().setPhaseRotatorFreqHz(500);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON · 500 Hz · 8 stages"));

        // Tweak stages → label re-renders.
        model.transmitModel().setPhaseRotatorStages(12);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON · 500 Hz · 12 stages"));

        // Disable → "OFF".
        model.transmitModel().setPhaseRotatorEnabled(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("OFF"));
    }

    // ── 12. CFC status label initial — "OFF" (default cfcEnabled=false) ──────
    void cfcLabel_defaultsToOff()
    {
        RadioModel model;
        QCOMPARE(model.transmitModel().cfcEnabled(), false);  // sanity

        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_CFC"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("OFF"));
    }

    // ── 13. CFC label updates live on cfcEnabledChanged ──────────────────────
    void cfcLabel_updatesOnSignal()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_CFC"));
        QVERIFY(state);

        model.transmitModel().setCfcEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON · 10 bands"));

        model.transmitModel().setCfcEnabled(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("OFF"));
    }

    // ── 14. CESSB off-text shows "(gated on CPDR)" when cpdrOn=false ─────────
    //
    // Per WDSP TXA.c:843-851 [v2.10.3.13], CESSB's bp2 stage only activates
    // when CPDR (compressor) run-flag is set.  Surfacing the gating in the
    // off-text helps users understand why flipping CESSB without CPDR is a
    // no-op.  Both cessbOn and cpdrOn default to false, so the initial text
    // is "OFF (gated on CPDR)".
    void cessbLabel_initialOffShowsGate()
    {
        RadioModel model;
        QCOMPARE(model.transmitModel().cessbOn(), false);  // sanity
        QCOMPARE(model.transmitModel().cpdrOn(),  false);  // sanity

        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_CESSB"));
        QVERIFY(state);
        QCOMPARE(state->text(), QStringLiteral("OFF (gated on CPDR)"));
    }

    // ── 15. CESSB label updates live on cessbOn + cpdrOn ─────────────────────
    void cessbLabel_updatesOnSignals()
    {
        RadioModel model;
        SpeechProcessorPage page(&model);
        page.show();

        auto* state = page.findChild<QLabel*>(QStringLiteral("state_CESSB"));
        QVERIFY(state);

        // Initial: OFF, gated.
        QCOMPARE(state->text(), QStringLiteral("OFF (gated on CPDR)"));

        // Enable CPDR → off-text drops the gate suffix.
        model.transmitModel().setCpdrOn(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("OFF"));

        // Enable CESSB → "ON".
        model.transmitModel().setCessbOn(true);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON"));

        // Disable CPDR while CESSB stays on — still "ON" (cessbOn dominates).
        model.transmitModel().setCpdrOn(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("ON"));

        // Disable CESSB → gating returns.
        model.transmitModel().setCessbOn(false);
        QCoreApplication::processEvents();
        QCOMPARE(state->text(), QStringLiteral("OFF (gated on CPDR)"));
    }
};

QTEST_MAIN(TestSpeechProcessorPage)
#include "tst_speech_processor_page.moc"
