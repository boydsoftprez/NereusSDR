// =================================================================
// tests/tst_step_attenuator_controller.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: band restore to the radio cases (R-R3-46, R-R3-11), by
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

// R-R3-46: records the attenuator and preamp values sent to the radio.
class RecordingConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit RecordingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    QList<int> attenuator;
    QList<bool> preamp;
    // Level Cal: the Alex attenuator bits (Thetis SetAlexAtten).
    QList<int> alexAtten;

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int dB) override { attenuator.append(dB); }
    void setPreamp(bool on) override { preamp.append(on); }
    void setAlexAtten(int bits) override { alexAtten.append(bits); }
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

} // namespace

class TestStepAttenuatorController : public QObject {
    Q_OBJECT
private slots:

    void singleOverflow_emitsYellow()
    {
        // A single ADC overflow event followed by one tick should raise
        // the counter from 0→1, which is above 0 → Yellow severity.
        // From Thetis console.cs:21378: level > 0 triggers warning text.
        StepAttenuatorController ctrl;
        // Stop the internal timer so only manual tick() calls drive state.
        ctrl.setTickTimerEnabled(false);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);
        qRegisterMetaType<NereusSDR::OverloadLevel>();

        ctrl.onAdcOverflow(0);
        ctrl.tick();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);  // ADC index
        QCOMPARE(spy.first().at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
        QCOMPARE(ctrl.overloadCounter(0), 1);
    }

    void noOverflow_levelDecays()
    {
        // After reaching Yellow, if no further overflow events arrive,
        // each tick decrements the counter by 1 until it reaches 0 (None).
        // From Thetis console.cs:21373-21375.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Raise to Yellow.
        ctrl.onAdcOverflow(0);
        ctrl.tick();
        QCOMPARE(ctrl.overloadCounter(0), 1);
        QCOMPARE(spy.count(), 1);  // None→Yellow

        // One tick with no overflow → counter 1→0 → back to None.
        ctrl.tick();
        QCOMPARE(ctrl.overloadCounter(0), 0);
        QCOMPARE(spy.count(), 2);  // Yellow→None
        QCOMPARE(spy.last().at(1).value<OverloadLevel>(), OverloadLevel::None);
    }

    void sustainedOverflow_escalatesToRed()
    {
        // Sustained overflows across >3 ticks push the level past the
        // red threshold (>3). From Thetis console.cs:21369: red when > 3.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // 4 ticks with overflow: level goes 0→1→2→3→4.
        // Yellow emitted at tick 1 (level 1), Red at tick 4 (level 4 > 3).
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadCounter(0), 4);
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);

        // Should have emitted Yellow on first tick, Red on fourth.
        QVERIFY(spy.count() >= 2);
        QCOMPARE(spy.first().at(1).value<OverloadLevel>(), OverloadLevel::Yellow);
        QCOMPARE(spy.last().at(1).value<OverloadLevel>(), OverloadLevel::Red);
    }

    void levelCapsAtFive()
    {
        // From Thetis console.cs:21366 — counter caps at 5 (despite the
        // comment saying 10). Verify it doesn't exceed kMaxOverloadLevel.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();

        // 10 ticks with continuous overflow.
        for (int i = 0; i < 10; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadCounter(0), 5);
    }

    void redDowngradesToYellow()
    {
        // After reaching Red (level > 3), decay without new overflows
        // should transition Red→Yellow when level drops to 3 (which is
        // not > 3, so Yellow), then Yellow→None when level reaches 0.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Pump to Red (level 5 = capped).
        for (int i = 0; i < 6; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        QCOMPARE(ctrl.overloadCounter(0), 5);
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);
        spy.clear();

        // Decay: 5→4→3. At level 3, severity is Yellow (not > 3).
        ctrl.tick();  // 5→4, still Red
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);

        ctrl.tick();  // 4→3, now Yellow
        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Yellow);
        QVERIFY(spy.count() >= 1);

        // Find the Yellow transition.
        bool foundYellow = false;
        for (const auto& call : spy) {
            if (call.at(1).value<OverloadLevel>() == OverloadLevel::Yellow) {
                foundYellow = true;
                break;
            }
        }
        QVERIFY2(foundYellow, "Expected Red→Yellow transition during decay");
    }

    void multipleAdcsIndependent()
    {
        // Each ADC has its own independent counter. Overflow on ADC 0
        // should not affect ADC 1 or ADC 2.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        qRegisterMetaType<NereusSDR::OverloadLevel>();
        QSignalSpy spy(&ctrl, &StepAttenuatorController::overloadStatusChanged);

        // Only ADC 1 overflows.
        ctrl.onAdcOverflow(1);
        ctrl.tick();

        QCOMPARE(ctrl.overloadCounter(0), 0);
        QCOMPARE(ctrl.overloadCounter(1), 1);
        QCOMPARE(ctrl.overloadCounter(2), 0);

        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::None);
        QCOMPARE(ctrl.overloadLevel(1), OverloadLevel::Yellow);
        QCOMPARE(ctrl.overloadLevel(2), OverloadLevel::None);

        // Signal should only reference ADC 1.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 1);
    }

    void autoAtt_skippedDuringMox()
    {
        // Issue #175 follow-up: auto-att must not run during MOX, otherwise
        // own-TX leakage tripping ADC overflow bumps m_attDb past the
        // intended TX value (e.g. force-31 → 32 → ...).  This test pins
        // the gate added in the m_isMox branch of tick().
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Classic);

        // Engage MOX — fires force-31 path (default ATT-on-TX enabled,
        // PS off, force-when-PS-off enabled, SSB).  m_attDb := 31.
        ctrl.onMoxHardwareFlipped(true);
        const int attDuringTx = ctrl.attenuatorDb();
        QCOMPARE(attDuringTx, 31);

        // Simulate own-TX leakage: 4 ADC overflow ticks would normally
        // push to Red and bump m_attDb past 31.  With the m_isMox gate,
        // tick() must early-return and leave m_attDb at 31.
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.attenuatorDb(), 31);  // Unchanged — no auto-att bump.

        // Disengage MOX — m_attDb restores to the saved RX value (0).
        ctrl.onMoxHardwareFlipped(false);
        QCOMPARE(ctrl.attenuatorDb(), 0);
    }

    void classicAutoAtt_bumpsOnRed()
    {
        // Classic auto-att bumps the step attenuator by the ADC overload
        // level value when Red threshold is exceeded.
        // From Thetis console.cs:21548-21567.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Classic);
        ctrl.setAutoAttUndo(false);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push ADC 0 to Red: 4 consecutive overflow+tick cycles.
        // Level goes 0→1→2→3→4. Red fires at level 4 (> kRedThreshold=3).
        for (int i = 0; i < 4; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        QCOMPARE(ctrl.overloadLevel(0), OverloadLevel::Red);
        QVERIFY2(spy.count() >= 1, "attenuationChanged should fire on Red");
        // Classic bumps by the level value (4 at Red), so ATT > 0.
        int att = spy.last().at(0).toInt();
        QVERIFY2(att > 0, "ATT should be bumped above 0 on Red");
    }

    void adaptiveAttack_rampsGradually()
    {
        // Adaptive mode ramps ATT by 1 dB per tick while Red overload
        // persists. Verify gradual ramp over multiple attack ticks.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // 7 ticks with sustained overflow: first 4 reach Red,
        // ticks 5-7 are 3 attack cycles at +1 dB each.
        for (int i = 0; i < 7; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // Should have at least 3 attenuationChanged emissions (from the
        // 3 attack ticks after reaching Red on tick 4).
        QVERIFY2(spy.count() >= 3, "Expected >= 3 attack emissions");

        // Verify gradual ramp: each emission should be +1 dB from prior.
        for (int i = 1; i < spy.count(); ++i) {
            int prev = spy.at(i - 1).at(0).toInt();
            int curr = spy.at(i).at(0).toInt();
            QCOMPARE(curr, prev + 1);
        }

        // Final ATT should be >= 3 (at least 3 attack ticks after Red).
        int finalAtt = spy.last().at(0).toInt();
        QVERIFY2(finalAtt >= 3, "ATT should be >= 3 after 3+ attack ticks");
    }

    void adaptiveDecay_relaxesAfterHold()
    {
        // Adaptive mode decays ATT by 1 dB per decay interval after the
        // hold period elapses with no further overload. The decay path
        // uses wall-clock time, so we set very short hold/decay values
        // and use QTest::qWait() to advance past them.
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);
        ctrl.setAutoAttUndo(true);
        // Short hold (50ms) and fast decay (1ms per step) for test speed.
        ctrl.setAutoAttHoldSeconds(0.05);   // 50ms hold
        ctrl.setAdaptiveDecayMs(1);         // 1ms decay rate

        QSignalSpy spy(&ctrl, &StepAttenuatorController::attenuationChanged);

        // Push to Red (4 ticks) + 2 more attack ticks = 6 overflow+tick
        // cycles. Level caps at 5. Attack fires on ticks 4-6 (+1 dB each).
        for (int i = 0; i < 6; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }

        // After stopping overflow, the hysteresis counter decays naturally:
        // 5→4 (still Red, attack continues), 4→3 (Yellow, no more attack).
        // So we tick without overflow to let the counter drain below Red.
        // These ticks may produce additional attack emissions while Red.
        for (int i = 0; i < 3; ++i) {
            ctrl.tick();
        }

        // Record peak ATT value — includes any extra attack ticks during
        // counter drain from Red.
        QVERIFY2(spy.count() >= 1, "Should have attack emissions");
        int peakAtt = spy.last().at(0).toInt();
        QVERIFY2(peakAtt >= 2, "Peak ATT should be >= 2 after attack ticks");

        spy.clear();

        // Wait past the hold period (50ms + generous margin).
        QTest::qWait(150);

        // Tick without overflow to trigger decay path.
        // Each tick calls applyAdaptiveAutoAtt(-1) which decays 1 dB if
        // hold has elapsed and decay interval has passed.
        for (int i = 0; i < 20; ++i) {
            ctrl.tick();
            QTest::qWait(5);  // Ensure decay rate (1ms) is satisfied.
        }

        // Verify decay happened: at least one emission with value < peak.
        QVERIFY2(spy.count() >= 1, "Should have decay emissions after hold");
        bool decayed = false;
        for (int i = 0; i < spy.count(); ++i) {
            if (spy.at(i).at(0).toInt() < peakAtt) {
                decayed = true;
                break;
            }
        }
        QVERIFY2(decayed, "ATT should decay below peak after hold period");
    }

    // ─────────────────────────────────────────────────────────────────────
    // Issue #259 regression: enable + value persistence round-trip.
    //
    // Bug surface: user opens Setup → General → Options, sets RX1 Enable
    // and RX2 Enable to 5 dB, closes Nereus, reopens — both controls
    // revert to unchecked / 0 dB.
    //
    // The fix has two halves; this test covers half-A (controller
    // persistence + signal emission) only. The MainWindow / RadioModel
    // disconnect-ordering half is exercised at runtime (Activity Monitor
    // ⌘Q can't be unit-tested without spinning up the full main thread).
    //
    // (a) saveSettings/loadSettings round-trip preserves m_stepAttEnabled.
    // (b) loadSettings emits stepAttEnabledChanged so any UI bound via
    //     connectController() sees the restored value.
    // (c) loadSettings emits attenuationChanged so both the RX1 and RX2
    //     spinboxes (wired in GeneralOptionsPage) update.
    // ─────────────────────────────────────────────────────────────────────
    void persistence_enableRoundTrip_emitsStepAttEnabledChanged()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:01");

        // First controller: simulate the user toggling enable + value, then
        // saving on disconnect.
        //
        // The leading loadSettings tags the controller as loaded for this
        // MAC so saveSettings is allowed to write. Issue #259: an unloaded
        // controller's saveSettings is a no-op to prevent the pre-load
        // clobber path (see persistence_saveBeforeLoad_doesNotClobber).
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);  // tag as loaded
            ctrl.setStepAttEnabled(true);
            ctrl.setAttenuation(5);
            QCOMPARE(ctrl.stepAttEnabled(), true);
            QCOMPARE(ctrl.attenuatorDb(), 5);

            ctrl.saveSettings(mac);
            AppSettings::instance().save();
        }

        // Second controller: simulate fresh app launch + reconnect.
        // setStepAttEnabled(false) FIRST so the load sees a real flip and
        // we can verify both the value AND the signal emission.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.setStepAttEnabled(false);

            QSignalSpy enableSpy(&ctrl,
                &StepAttenuatorController::stepAttEnabledChanged);
            QSignalSpy attSpy(&ctrl,
                &StepAttenuatorController::attenuationChanged);

            ctrl.loadSettings(mac);

            QCOMPARE(ctrl.stepAttEnabled(), true);
            QCOMPARE(ctrl.attenuatorDb(), 5);

            // The fix: loadSettings must emit stepAttEnabledChanged so the
            // checkbox in GeneralOptionsPage flips from default-unchecked
            // to checked. Without this emit, the controller state is right
            // but the UI is stale.
            QVERIFY2(enableSpy.count() >= 1,
                "loadSettings must emit stepAttEnabledChanged for UI sync");
            QCOMPARE(enableSpy.last().at(0).toBool(), true);

            // Existing attenuationChanged emit (already shipping) must keep
            // working so both spinboxes update.
            QVERIFY2(attSpy.count() >= 1,
                "loadSettings must emit attenuationChanged for spinbox sync");
            QCOMPARE(attSpy.last().at(0).toInt(), 5);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Issue #259 regression: saveSettings before loadSettings must NOT
    // clobber the persisted file with constructor defaults.
    //
    // Bench failure mode: RadioModel::connectToRadio calls teardown-
    // Connection when a previous m_connection still exists (auto-reconnect
    // retry, panel-driven reconnect, etc). teardownConnection's
    // saveSettings would fire with m_attDb=0 / m_stepAttEnabled=true
    // before the matching loadSettings, overwriting the user's real
    // persisted state. Confirmed at /tmp/nereus259/run.log:
    //   18:31:11.242 saveSettings m_attDb=0
    //   18:31:15.854 loadSettings DONE m_attDb=0  (reads back the clobber)
    //
    // The gate: m_loadedMac is empty until loadSettings runs for the MAC,
    // and saveSettings short-circuits when m_loadedMac != mac.
    // ─────────────────────────────────────────────────────────────────────
    void persistence_saveBeforeLoad_doesNotClobber()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:03");

        // Session 1: user saved m_attDb=12 cleanly.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.setStepAttEnabled(true);
            ctrl.setAttenuation(12);

            ctrl.loadSettings(mac);  // tag the controller as loaded for this MAC
            // (the values just read are at-defaults / not-yet-saved; loadSettings
            //  is idempotent with respect to in-memory state we just set above)
            ctrl.setAttenuation(12);  // re-apply after load
            ctrl.saveSettings(mac);
            AppSettings::instance().save();
        }

        // Session 2: fresh controller. saveSettings fires (simulating the
        // pre-load teardown clobber path) BEFORE loadSettings runs.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            QCOMPARE(ctrl.attenuatorDb(), 0);   // default
            QCOMPARE(ctrl.settingsLoaded(), false);

            // The bug: this would write m_attDb=0 to disk.
            ctrl.saveSettings(mac);
            AppSettings::instance().save();
        }

        // Session 3: verify the persisted value is still 12, not the
        // accidental zero from Session 2.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            QCOMPARE(ctrl.attenuatorDb(), 12);
            QCOMPARE(ctrl.settingsLoaded(), true);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Issue #259: markSettingsUnloaded() re-arms the gate so a re-connect
    // (even to the same MAC) cannot save until loadSettings has run again.
    // ─────────────────────────────────────────────────────────────────────
    void persistence_markUnloaded_reArmsGate()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:04");

        // Prime: load + write a real value.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            ctrl.setAttenuation(7);
            QVERIFY(ctrl.settingsLoaded());
            ctrl.saveSettings(mac);
            AppSettings::instance().save();
        }

        // Simulate disconnect-then-reconnect race: a fresh controller
        // loads, marks unloaded (disconnect path), then sees a stray
        // saveSettings before the next loadSettings runs. The save must
        // be rejected so the persisted 7 survives.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            QCOMPARE(ctrl.attenuatorDb(), 7);

            ctrl.markSettingsUnloaded();
            QCOMPARE(ctrl.settingsLoaded(), false);

            // Stray save in the unloaded state — must be a no-op.
            ctrl.setAttenuation(0);  // simulate default reset by some code path
            ctrl.saveSettings(mac);
            AppSettings::instance().save();
        }

        // Verify the prior 7 survived the stray save.
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setMaxAttenuation(31);
            ctrl.loadSettings(mac);
            QCOMPARE(ctrl.attenuatorDb(), 7);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Issue #259 regression: a fresh / unsaved MAC must still emit
    // stepAttEnabledChanged on load so the UI doesn't drift away from the
    // controller's default-true.
    // ─────────────────────────────────────────────────────────────────────
    void persistence_loadSignalsOnDefaultEnabled()
    {
        const QString freshMac = QStringLiteral("aa:bb:cc:de:ad:02");

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        // Drop to false so the load → default-True transition is observable.
        ctrl.setStepAttEnabled(false);

        QSignalSpy enableSpy(&ctrl,
            &StepAttenuatorController::stepAttEnabledChanged);

        ctrl.loadSettings(freshMac);

        QCOMPARE(ctrl.stepAttEnabled(), true);
        QVERIFY2(enableSpy.count() >= 1,
            "loadSettings must emit stepAttEnabledChanged even on fresh MAC");
    }

    // ─────────────────────────────────────────────────────────────────────
    // R-R3-46: every setting has a change signal, once per real change, so
    // the Core's mirrored `stepAtt` object can follow all of them.
    // ─────────────────────────────────────────────────────────────────────
    void everySettingSignalsItsChangeOnce()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        QSignalSpy mode(&ctrl, &StepAttenuatorController::autoAttModeChanged);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);
        QCOMPARE(mode.count(), 1);
        QCOMPARE(mode.last().at(0).value<AutoAttMode>(), AutoAttMode::Adaptive);

        // A radio without per-step calibration (the Hermes Lite 2) settles
        // Adaptive to Classic, and says so with the same signal.
        ctrl.setHasStepAttenuatorCal(false);
        ctrl.setAutoAttMode(AutoAttMode::Adaptive);
        QCOMPARE(ctrl.autoAttMode(), AutoAttMode::Classic);
        QCOMPARE(mode.count(), 2);

        QSignalSpy undo(&ctrl, &StepAttenuatorController::autoAttUndoChanged);
        ctrl.setAutoAttUndo(true);
        ctrl.setAutoAttUndo(true);
        QCOMPARE(undo.count(), 1);

        QSignalSpy delay(&ctrl, &StepAttenuatorController::autoUndoDelayChanged);
        ctrl.setAutoUndoDelaySec(9);
        ctrl.setAutoUndoDelaySec(9);
        QCOMPARE(delay.count(), 1);
        QCOMPARE(delay.last().at(0).toInt(), 9);

        QSignalSpy hold(&ctrl, &StepAttenuatorController::autoAttHoldChanged);
        ctrl.setAutoAttHoldSeconds(4.0);
        ctrl.setAutoAttHoldSeconds(4.0);
        QCOMPARE(hold.count(), 1);
        QCOMPARE(hold.last().at(0).toInt(), 4000);
        QCOMPARE(ctrl.adaptiveHoldMs(), 4000);

        QSignalSpy range(&ctrl, &StepAttenuatorController::attenuationRangeChanged);
        ctrl.setMaxAttenuation(61);
        ctrl.setMaxAttenuation(61);
        ctrl.setMinAttenuation(-28);
        QCOMPARE(range.count(), 2);
        QCOMPARE(range.last().at(0).toInt(), -28);
        QCOMPARE(range.last().at(1).toInt(), 61);

        QSignalSpy rx1(&ctrl, &StepAttenuatorController::rx1PreampChanged);
        ctrl.setRx1Preamp(true);
        ctrl.setRx1Preamp(true);
        QCOMPARE(rx1.count(), 1);
        QVERIFY(ctrl.rx1Preamp());

        QSignalSpy reloaded(&ctrl, &StepAttenuatorController::settingsReloaded);
        ctrl.loadSettings(QStringLiteral("aa:bb:cc:de:ad:46"));
        QCOMPARE(reloaded.count(), 1);
    }

    // ─────────────────────────────────────────────────────────────────────
    // R-R3-46 / R-R3-11: the Core saves an operator change shortly after it
    // is made. Off by default, so a local window keeps today's teardown-only
    // save; auto-attenuate's own moves never schedule one.
    // ─────────────────────────────────────────────────────────────────────
    void debouncedSaveWritesTheBandAfterAnOperatorChange()
    {
        auto& s = AppSettings::instance();
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:47");
        const QString bandKey = QStringLiteral("options/stepAtt/rx1Band/")
                                + bandKeyName(Band::Band40m);

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band40m);
        ctrl.loadSettings(mac);
        // Start from known settings whatever an earlier run saved.
        ctrl.setAutoAttUndo(false);
        ctrl.setAutoAttEnabled(false);

        // Default: nothing scheduled.
        ctrl.setAttenuation(11);
        QVERIFY(!ctrl.savePending());

        ctrl.setDebouncedSaveEnabled(true);
        ctrl.setAttenuation(20);
        QVERIFY(ctrl.savePending());
        QTRY_VERIFY(!ctrl.savePending());
        QCOMPARE(s.hardwareValue(mac, bandKey).toInt(), 20);
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("options/stepAtt/rx1Value")).toInt(), 20);

        // Every other operator setting schedules one too.
        ctrl.setAutoAttUndo(true);
        QVERIFY(ctrl.savePending());
        ctrl.flushPendingSave();
        QVERIFY(!ctrl.savePending());
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1Undo")).toString(),
                 QStringLiteral("True"));

        // Auto-attenuate's bump is not an operator change.
        ctrl.setAutoAttEnabled(true);
        ctrl.flushPendingSave();
        for (int i = 0; i < 5; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        QVERIFY(ctrl.autoAttApplied());
        QVERIFY(ctrl.attenuatorDb() > 20);
        QVERIFY(!ctrl.savePending());

        // An unloaded controller never schedules.
        ctrl.markSettingsUnloaded();
        ctrl.setAutoAttUndo(false);
        QVERIFY(!ctrl.savePending());
    }

    // ─────────────────────────────────────────────────────────────────────
    // R-R3-49 (group A fix wave, M6): ATT on TX, its value and Force ATT,
    // changed on the Core from a remote window, are saved at once, not at
    // teardown, so a Core that loses power keeps them.
    // ─────────────────────────────────────────────────────────────────────
    void debouncedSaveWritesAttOnTxAndForceAtt()
    {
        auto& s = AppSettings::instance();
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:49");
        s.clearHardwareValues(mac);

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.loadSettings(mac);
        ctrl.setDebouncedSaveEnabled(true);
        ctrl.flushPendingSave();

        const bool attOnTx = !ctrl.attOnTxEnabled();
        ctrl.setAttOnTxEnabled(attOnTx);
        QVERIFY(ctrl.savePending());
        QTRY_VERIFY(!ctrl.savePending());
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("options/stepAtt/attOnTxEnabled")).toString(),
                 attOnTx ? QStringLiteral("True") : QStringLiteral("False"));

        const bool force = !ctrl.forceAttWhenPsOff();
        ctrl.setForceAttWhenPsOff(force);
        QVERIFY(ctrl.savePending());
        ctrl.flushPendingSave();
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("options/stepAtt/forceAttWhenPsOff")).toString(),
                 force ? QStringLiteral("True") : QStringLiteral("False"));

        const int value = ctrl.attOnTxValue() == 17 ? 18 : 17;
        ctrl.setAttOnTxValue(value);
        QVERIFY(ctrl.savePending());
        ctrl.flushPendingSave();
        StepAttenuatorController reloaded;
        reloaded.setTickTimerEnabled(false);
        reloaded.loadSettings(mac);
        QCOMPARE(reloaded.attOnTxValue(), value);
        QCOMPARE(reloaded.attOnTxEnabled(), attOnTx);
        QCOMPARE(reloaded.forceAttWhenPsOff(), force);
    }

    // ─────────────────────────────────────────────────────────────────────
    // R-R3-46: a band change restores that band's attenuation and preamp
    // and, with setBandRestoreToRadio(true) (the Core and a local window),
    // sends them to the radio, as Thetis's RX1Band setter does
    // (console.cs:17325 [v2.10.3.15]). A band never visited keeps the
    // current setting and sends nothing.
    // ─────────────────────────────────────────────────────────────────────
    void bandChangeRestoresAndSendsTheBandsValues()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        // The band memory is a loaded radio's (R-R3-46 fix wave).
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:10");
        AppSettings::instance().clearHardwareValues(mac);
        ctrl.loadSettings(mac);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setBandRestoreToRadio(true);

        ctrl.setBand(Band::Band20m);
        ctrl.setAttenuation(0);
        ctrl.setPreampMode(PreampMode::Off);
        ctrl.setBand(Band::Band40m);
        ctrl.setAttenuation(20);
        ctrl.setPreampMode(PreampMode::On);

        radio.attenuator.clear();
        radio.preamp.clear();
        ctrl.setBand(Band::Band20m);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        QCOMPARE(ctrl.preampMode(), PreampMode::Off);
        QCOMPARE(radio.attenuator, QList<int>{0});
        QCOMPARE(radio.preamp, QList<bool>{false});

        radio.attenuator.clear();
        radio.preamp.clear();
        ctrl.setBand(Band::Band40m);
        QCOMPARE(radio.attenuator, QList<int>{20});
        QCOMPARE(radio.preamp, QList<bool>{true});

        // Never visited: the current setting stays and nothing is sent.
        radio.attenuator.clear();
        radio.preamp.clear();
        ctrl.setBand(Band::Band17m);
        QCOMPARE(ctrl.attenuatorDb(), 20);
        QCOMPARE(ctrl.preampMode(), PreampMode::On);
        QVERIFY(radio.attenuator.isEmpty());
        QVERIFY(radio.preamp.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // Off (the default), a restored value is shown, not sent.
    void bandRestoreIsNotSentWhenOff()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:11");
        AppSettings::instance().clearHardwareValues(mac);
        ctrl.loadSettings(mac);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        QVERIFY(!ctrl.bandRestoreToRadio());

        ctrl.setBand(Band::Band20m);
        ctrl.setAttenuation(0);
        ctrl.setBand(Band::Band40m);
        ctrl.setAttenuation(20);
        radio.attenuator.clear();
        radio.preamp.clear();
        ctrl.setBand(Band::Band20m);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        QVERIFY(radio.attenuator.isEmpty());
        QVERIFY(radio.preamp.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────
    // R-R3-46 fix wave (review Known 3). A controller that switches radios
    // starts the new radio with that radio's own band memory: nothing of
    // the previous radio's is restored on it or saved under its MAC.
    // ─────────────────────────────────────────────────────────────────────
    void switchingRadiosDropsThePreviousRadiosBandMemory()
    {
        const QString g2 = QStringLiteral("aa:bb:cc:de:ad:20");
        const QString hl2 = QStringLiteral("aa:bb:cc:de:ad:21");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(g2);
        s.clearHardwareValues(hl2);

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setMaxAttenuation(61);
        ctrl.loadSettings(g2);
        ctrl.setBand(Band::Band40m);
        ctrl.setAttenuation(45);
        ctrl.setBand(Band::Band20m);   // 40 m remembers 45 dB on the G2
        ctrl.setAttenuation(10);
        ctrl.saveSettings(g2);

        // The HL2: -28..31 dB, nothing saved yet.
        ctrl.setMinAttenuation(-28);
        ctrl.setMaxAttenuation(31);
        ctrl.loadSettings(hl2);
        const int start = ctrl.attenuatorDb();
        ctrl.setBand(Band::Band40m);   // never used on the HL2
        QCOMPARE(ctrl.attenuatorDb(), start);
        ctrl.saveSettings(hl2);
        QVERIFY(s.hardwareValue(hl2, QStringLiteral("options/stepAtt/rx1Band/40m")).toString()
                != QStringLiteral("45"));
        s.clearHardwareValues(g2);
        s.clearHardwareValues(hl2);
    }

    // Before a radio's settings are loaded the values are the starting
    // defaults; a band change then stores nothing as the band left's memory
    // (it used to become the general-coverage band's).
    void bandChangeBeforeLoadStoresNoMemory()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:22");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        ctrl.setBand(Band::Band40m);   // leaves GEN before any radio loads
        ctrl.loadSettings(mac);
        ctrl.setAttenuation(20);
        ctrl.setBand(Band::GEN);       // never used on this radio
        QCOMPARE(ctrl.attenuatorDb(), 20);
        ctrl.saveSettings(mac);
        s.clearHardwareValues(mac);
    }

    // A restored value stays within the radio's range, as the radio clamps
    // it, both on a band change and when the settings load.
    void restoredAttenuationStaysWithinTheRadiosRange()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:23");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);
        s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx1Band/40m"),
                           QStringLiteral("45"));
        s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx1Band/20m"),
                           QStringLiteral("45"));

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setMinAttenuation(-28);
        ctrl.setMaxAttenuation(31);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setBandRestoreToRadio(true);
        ctrl.setBand(Band::Band20m);
        ctrl.loadSettings(mac);
        QCOMPARE(ctrl.attenuatorDb(), 31);

        radio.attenuator.clear();
        ctrl.setBand(Band::Band10m);
        ctrl.setAttenuation(5);
        radio.attenuator.clear();
        ctrl.setBand(Band::Band40m);
        QCOMPARE(ctrl.attenuatorDb(), 31);
        QCOMPARE(radio.attenuator, QList<int>{31});
        ctrl.setRadioConnection(nullptr);
        s.clearHardwareValues(mac);
    }

    // R-R3-46 follow-up item 3. On a switch to another radio the window
    // connects the new radio and selects the band before it loads the new
    // radio's settings. Between the old radio's teardown and that load a
    // band change must neither restore the old radio's band memory nor
    // send it to the new radio.
    void switchingRadiosSendsNothingBeforeTheNewRadioLoads()
    {
        const QString oldRadio = QStringLiteral("aa:bb:cc:de:ad:30");
        const QString newRadio = QStringLiteral("aa:bb:cc:de:ad:31");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(oldRadio);
        s.clearHardwareValues(newRadio);

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBandRestoreToRadio(true);
        RecordingConnection oldConn;
        ctrl.setRadioConnection(&oldConn);
        ctrl.loadSettings(oldRadio);
        ctrl.setBand(Band::Band40m);
        ctrl.setAttenuation(20);
        ctrl.setBand(Band::Band20m);
        ctrl.setAttenuation(0);
        // Teardown of the old radio (RadioModel::teardownConnection).
        ctrl.saveSettings(oldRadio);
        ctrl.markSettingsUnloaded();

        // The new radio's connect order (MainWindow): connection, band, load.
        RecordingConnection newConn;
        ctrl.setRadioConnection(&newConn);
        ctrl.setBand(Band::Band40m);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        QVERIFY(newConn.attenuator.isEmpty());
        QVERIFY(newConn.preamp.isEmpty());
        ctrl.loadSettings(newRadio);
        ctrl.setBand(Band::Band20m);
        ctrl.setBand(Band::Band40m);
        QVERIFY(!newConn.attenuator.contains(20));
        ctrl.setRadioConnection(nullptr);
        s.clearHardwareValues(oldRadio);
        s.clearHardwareValues(newRadio);
    }

    // Level Cal: with the step attenuator off, classic auto-attenuate steps
    // the preamp setting to the step-attenuator settings and sends each one.
    // From Thetis console.cs:21614-21631 [v2.10.3.15]:
    //   case PreampMode.HPSDR_OFF:
    //   case PreampMode.HPSDR_ON:
    //       pam = PreampMode.SA_MINUS10;
    //   case PreampMode.SA_MINUS10: pam = PreampMode.SA_MINUS20;
    //   case PreampMode.SA_MINUS20: pam = PreampMode.SA_MINUS30;
    void classicAutoAttStepsThePreampToTheStepAttenuatorSettings()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Hermes, HPSDRModel::ANAN100, true);
        ctrl.setStepAttEnabled(false);
        ctrl.setPreampMode(PreampMode::On);
        ctrl.setAutoAttEnabled(true);
        ctrl.setAutoAttMode(AutoAttMode::Classic);
        ctrl.setAutoAttUndo(false);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);

        const PreampMode expected[] = {PreampMode::SaMinus10,
                                       PreampMode::SaMinus20,
                                       PreampMode::SaMinus30,
                                       PreampMode::SaMinus30};
        const int expectedAtt[] = {10, 20, 30, 30};
        for (int i = 0; i < 3; ++i) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
        }
        for (int step = 0; step < 4; ++step) {
            ctrl.onAdcOverflow(0);
            ctrl.tick();
            QCOMPARE(ctrl.preampMode(), expected[step]);
            QVERIFY(!radio.attenuator.isEmpty());
            QCOMPARE(radio.attenuator.last(), expectedAtt[step]);
        }
        QVERIFY(radio.preamp.isEmpty());

        // Turning auto-attenuate off puts the operator's setting back on
        // the radio: On sends 0 dB of step attenuator.
        ctrl.setAutoAttEnabled(false);
        QCOMPARE(ctrl.preampMode(), PreampMode::On);
        QCOMPARE(radio.attenuator.last(), 0);
        ctrl.setRadioConnection(nullptr);
    }

    // Level Cal: each preamp setting drives the step attenuator, the preamp
    // bit and the Alex attenuator as Thetis's RX1PreampMode setter does.
    // From Thetis console.cs:19232-19284 [v2.10.3.15] (the switch) and
    // 19298-19330 (the sends). ANAN-100 with Alex, step attenuator off:
    // the attenuator and the Alex bits follow the table, and a board other
    // than the HPSDR never receives the preamp bit.
    void preampDriveOnAnAlexBoard()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Hermes, HPSDRModel::ANAN100, true);
        ctrl.setStepAttEnabled(false);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        struct Row { PreampMode mode; int att; int alex; };
        const Row rows[] = {
            {PreampMode::On,        0,  0},
            {PreampMode::Minus10,   0,  1},
            {PreampMode::Minus20,   0,  2},
            {PreampMode::Minus30,   0,  3},
            {PreampMode::Minus40,   20, 2},
            {PreampMode::Minus50,   20, 3},
            {PreampMode::SaMinus10, 10, 0},
            {PreampMode::SaMinus20, 20, 0},
            {PreampMode::SaMinus30, 30, 0},
            {PreampMode::Off,       20, 0},
        };
        for (const Row& row : rows) {
            ctrl.setPreampMode(row.mode);
            QVERIFY(!radio.attenuator.isEmpty());
            QVERIFY(!radio.alexAtten.isEmpty());
            QCOMPARE(radio.attenuator.last(), row.att);
            QCOMPARE(radio.alexAtten.last(), row.alex);
        }
        QVERIFY(radio.preamp.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // The HPSDR (Atlas) takes the preamp bit and the Alex bits, and no
    // step attenuator from the preamp setting.
    void preampDriveOnTheHpsdr()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Atlas, HPSDRModel::HPSDR, true);
        ctrl.setStepAttEnabled(false);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setPreampMode(PreampMode::Minus30);
        QCOMPARE(radio.preamp, QList<bool>{true});
        QCOMPARE(radio.alexAtten, QList<int>{3});
        ctrl.setPreampMode(PreampMode::Off);
        QCOMPARE(radio.preamp, (QList<bool>{true, false}));
        QCOMPARE(radio.alexAtten, (QList<int>{3, 0}));
        QVERIFY(radio.attenuator.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // A board without Alex turns the Alex settings into Off, as Thetis
    // does (console.cs:19220-19227 [v2.10.3.15]); the HL2's SA settings
    // drive its step attenuator.
    void preampDriveWithoutAlex()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::HermesLite, HPSDRModel::HERMESLITE, false);
        ctrl.setStepAttEnabled(false);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setPreampMode(PreampMode::SaMinus10);
        QCOMPARE(radio.attenuator.last(), 10);
        ctrl.setPreampMode(PreampMode::Minus20);
        QCOMPARE(ctrl.preampMode(), PreampMode::Off);
        QCOMPARE(radio.attenuator.last(), 20);
        ctrl.setPreampMode(PreampMode::SaMinus30);
        QCOMPARE(radio.attenuator.last(), 30);
        QVERIFY(radio.preamp.isEmpty());
        QCOMPARE(radio.alexAtten.last(), 0);
        ctrl.setRadioConnection(nullptr);
    }

    // Step attenuator on, an Alex board with the 61 dB range: above 31 dB
    // the Alex attenuator takes 30 dB and the step attenuator the value
    // + 2 (console.cs:11031-11056 [v2.10.3.15]); a preamp setting sends
    // only the Alex bits that match the attenuation.
    void stepAttenuatorAbove31UsesTheAlexAttenuator()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Hermes, HPSDRModel::ANAN100, true);
        ctrl.setMaxAttenuation(61);
        QCOMPARE(ctrl.maxAttenuation(), 61);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.setAttenuation(47);
        QCOMPARE(radio.attenuator.last(), 49);
        QCOMPARE(radio.alexAtten.last(), 3);
        ctrl.setAttenuation(20);
        QCOMPARE(radio.attenuator.last(), 20);
        QCOMPARE(radio.alexAtten.last(), 0);
        const int sends = radio.attenuator.size();
        ctrl.setPreampMode(PreampMode::Minus20);
        QCOMPARE(radio.attenuator.size(), sends);
        QCOMPARE(radio.alexAtten.last(), 0);
        QVERIFY(radio.preamp.isEmpty());
        ctrl.setRadioConnection(nullptr);
    }

    // The G2 and the ANAN-10 are outside Thetis's Alex list: their step
    // attenuator stops at 31 whatever range the caller sets, before or
    // after the board is known.
    void boardsOutsideTheAlexListStopAt31()
    {
        StepAttenuatorController g2;
        g2.setTickTimerEnabled(false);
        g2.setBoardIdentity(HPSDRHW::Saturn, HPSDRModel::ANAN_G2, true);
        g2.setMaxAttenuation(61);
        QCOMPARE(g2.maxAttenuation(), 31);
        RecordingConnection radio;
        g2.setRadioConnection(&radio);
        g2.setAttenuation(45);
        QCOMPARE(radio.attenuator.last(), 31);
        QCOMPARE(radio.alexAtten.last(), 0);
        g2.setRadioConnection(nullptr);

        StepAttenuatorController anan10;
        anan10.setTickTimerEnabled(false);
        anan10.setMaxAttenuation(61);
        QSignalSpy range(&anan10, &StepAttenuatorController::attenuationRangeChanged);
        anan10.setBoardIdentity(HPSDRHW::HermesII, HPSDRModel::ANAN10, true);
        QCOMPARE(anan10.maxAttenuation(), 31);
        QCOMPARE(range.count(), 1);
    }

    // Turning the step attenuator off sends the preamp setting's values;
    // turning it on sends the step attenuator's (Thetis RX1StepAttEnabled,
    // console.cs:10952-10972 [v2.10.3.15]). While it is off the step
    // attenuator value is not sent.
    void stepAttenuatorEnableSwitchesWhatIsSent()
    {
        const QString mac = QStringLiteral("aa:bb:cc:de:ad:40");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBoardIdentity(HPSDRHW::Hermes, HPSDRModel::ANAN100, true);
        RecordingConnection radio;
        ctrl.setRadioConnection(&radio);
        ctrl.loadSettings(mac);
        ctrl.setPreampMode(PreampMode::SaMinus20);
        ctrl.setAttenuation(12);
        radio.attenuator.clear();
        radio.alexAtten.clear();
        ctrl.setStepAttEnabled(false);
        QCOMPARE(radio.attenuator, QList<int>{20});
        QCOMPARE(radio.alexAtten.last(), 0);
        ctrl.setAttenuation(15);
        QCOMPARE(radio.attenuator, QList<int>{20});
        ctrl.setStepAttEnabled(true);
        QCOMPARE(radio.attenuator, (QList<int>{20, 15}));
        QCOMPARE(radio.alexAtten.last(), 0);
        ctrl.setRadioConnection(nullptr);
        s.clearHardwareValues(mac);
    }
};

QTEST_MAIN(TestStepAttenuatorController)
#include "tst_step_attenuator_controller.moc"
