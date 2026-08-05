// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_mic_profile_rade.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the "RADE" factory MicProfileManager preset
// (Phase 3R Task K1).
//
// RADE is the NereusSDR-native digital voice mode driven by the
// vendored RADE neural codec (third_party/rade), wired up in Phase
// 3R Tasks I1-J4.  Because RADE bypasses the WDSP USB/LSB modulator,
// most of the TXA chain (CFC / CESSB / Phase Rotator / ALC) is
// inactive on the wire.  The "RADE" factory profile encodes that
// bypass philosophy as a MicProfileManager preset so users get a
// sensible TX-side baseline when they switch a slice into RADE mode.
//
// Test surface:
//   - radeProfileExistsInFactoryList:    "RADE" is seeded on first launch.
//   - radeProfileHasEqDisabled:          TXEQEnabled=False, 10 band gains 0.
//   - radeProfileHasLevelerOn:           Lev_On=True with conservative caps.
//   - radeProfileHasAlcOff:              ALC_MaximumGain=0.
//   - radeProfileHasCfcCessbPhRotOff:    All three TXA-chain stages off.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-11 — Phase 3R Task K1: initial test file. NereusSDR-native;
//                 no Thetis upstream (RADE was not a Thetis mode).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

class TstMicProfileRade : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // The "RADE" preset is one of the factory-seeded profiles.  After
    // first-launch seed, profileNames() must contain it alongside the
    // existing 21 (Default + 20 Thetis factory rows).
    void radeProfileExistsInFactoryList()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        const QStringList names = mgr.profileNames();
        QVERIFY2(names.contains(QStringLiteral("RADE")),
                 "RADE factory profile missing from first-launch seed");
    }

    // RADE bypasses the WDSP USB/LSB modulator, so the 10-band TX EQ
    // is irrelevant.  The preset disables the EQ stage and flattens
    // all 10 bands to 0 dB so users do not get a surprise voicing
    // when they switch a slice into RADE.
    void radeProfileHasEqDisabled()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        TransmitModel tm;
        QVERIFY(mgr.setActiveProfile(QStringLiteral("RADE"), &tm));

        QCOMPARE(tm.txEqEnabled(), false);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tm.txEqBand(i), 0);
        }
    }

    // The Leveler stays on because freedv-gui's RADE TX pipeline runs
    // a WebRTC AGC at -9 dBFS target.  NereusSDR's Lev_MaxGain (dB)
    // and Lev_Decay (ms) are the closest equivalent parameters.
    void radeProfileHasLevelerOn()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        TransmitModel tm;
        QVERIFY(mgr.setActiveProfile(QStringLiteral("RADE"), &tm));

        QCOMPARE(tm.txLevelerOn(), true);
        // 2026-05-12: bench feedback dropped Lev_MaxGain 15->6 dB and
        // Lev_Decay 100->300 ms on the RADE preset (see
        // MicProfileManager.cpp:748-755) to stop AGC pumping that was
        // distorting the modem characteristics.  Test updated to match.
        QCOMPARE(tm.txLevelerMaxGain(), 6);
        QCOMPARE(tm.txLevelerDecay(), 300);
    }

    // ALC is a TXA-chain stage that does not apply to RADE (the modem
    // bypasses the USB/LSB modulator entirely).  ALC_MaximumGain=0
    // disables the WDSP ALC envelope follower.
    void radeProfileHasAlcOff()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        TransmitModel tm;
        QVERIFY(mgr.setActiveProfile(QStringLiteral("RADE"), &tm));

        QCOMPARE(tm.txAlcMaxGain(), 0);
    }

    // CFC, CESSB, and Phase Rotator are all TXA-chain stages that
    // RADE bypasses.  The preset turns each off so the radio sees a
    // clean baseband stream from RadeChannel::txEncode.
    void radeProfileHasCfcCessbPhRotOff()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        TransmitModel tm;
        QVERIFY(mgr.setActiveProfile(QStringLiteral("RADE"), &tm));

        QCOMPARE(tm.cfcEnabled(), false);
        QCOMPARE(tm.cessbOn(), false);
        QCOMPARE(tm.phaseRotatorEnabled(), false);
    }
};

QTEST_MAIN(TstMicProfileRade)
#include "tst_mic_profile_rade.moc"
