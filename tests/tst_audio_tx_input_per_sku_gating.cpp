// tests/tst_audio_tx_input_per_sku_gating.cpp  (NereusSDR)
//
// radio-mic-input Task 22 — Per-SKU mic-jack panel gating helper
// (boardShowsOrionMicJackPanel) that mirrors the Thetis pnlGeneralHardwareORION
// enable map.
//
// no-port-check: test fixture; no Thetis logic ported here.
//
// Thetis source for the parity:
//   setup.cs:19830-20405 [v2.10.3.13+501e3f51] — pnlGeneralHardwareORION.
//     Enable map per HPSDRModel switch:
//       false:  HERMES, ANAN10, ANAN10E, ANAN100, ANAN100B, ANAN100D, REDPITAYA
//       true:   ANAN200D, ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3
//   setup.cs:6181-6189 [v2.10.3.13+501e3f51] — panelSaturnMicInput.Enabled
//     Saturn-only.
//
// Mapping HPSDRModel -> HPSDRHW (boardForModel in HpsdrModel.h):
//   ANAN200D    -> Orion       (true)
//   ANAN7000D   -> OrionMKII   (true)
//   ANAN8000D   -> OrionMKII   (true)
//   ANAN_G2     -> Saturn      (true)
//   ANAN_G2_1K  -> Saturn      (true)
//   ANVELINAPRO3 -> OrionMKII  (true)  -- collapsed into OrionMKII at HW level
//   HERMES, ANAN10, ANAN100 -> Hermes        (false)
//   ANAN10E, ANAN100B       -> HermesII      (false)
//   ANAN100D                -> Angelia       (false)
//   HERMESLITE              -> HermesLite    (false; no mic jack at all)
//   HPSDR (Atlas)           -> Atlas         (not in switch, but per spec false)

#include <QtTest/QtTest>
#include <QApplication>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "gui/setup/AudioTxInputPage.h"

using namespace NereusSDR;

class TestAudioTxInputPerSkuGating : public QObject
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

    // ── Boards that DO show the Orion-style mic-jack panel ────────────────────

    void orionShowsOrionPanel()
    {
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Orion));
    }

    void orionMkiiShowsOrionPanel()
    {
        // OrionMKII is shared by ANAN-7000DLE, ANAN-8000DLE, AnvelinaPro3 in
        // boardForModel(); all three have pnlGeneralHardwareORION.Enabled=true
        // in Thetis.
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::OrionMKII));
    }

    void saturnShowsOrionPanel()
    {
        // ANAN-G2 and ANAN-G2-1K both map to Saturn; both true in Thetis.
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Saturn));
    }

    void saturnMkiiShowsOrionPanel()
    {
        // SaturnMKII is the NereusSDR-original board revision slot; treated
        // as Saturn-class for mic-jack purposes per spec §4.5.
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::SaturnMKII));
    }

    // ── Boards that do NOT show the Orion-style mic-jack panel ───────────────

    void hermesDoesNotShowOrionPanel()
    {
        // HPSDRModel.HERMES / ANAN10 / ANAN100 -> HPSDRHW::Hermes; all false.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Hermes));
    }

    void hermesIIDoesNotShowOrionPanel()
    {
        // HPSDRModel.ANAN10E / ANAN100B -> HPSDRHW::HermesII; both false.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::HermesII));
    }

    void angeliaDoesNotShowOrionPanel()
    {
        // HPSDRModel.ANAN100D -> HPSDRHW::Angelia; false in Thetis.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Angelia));
    }

    void atlasDoesNotShowOrionPanel()
    {
        // HPSDRModel.HPSDR (Atlas/Metis) is not in the Thetis pnlGenHWORION
        // switch at all -> default-false branch per spec §4.5.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Atlas));
    }

    void hermesLiteDoesNotShowOrionPanel()
    {
        // HL2 has no mic jack at all (hasMicJack=false); helper still
        // returns false to match Thetis behaviour for non-Orion families.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::HermesLite));
    }

    void hermesLiteRxOnlyDoesNotShowOrionPanel()
    {
        // NereusSDR-original RX-only kit slot; no mic jack.
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::HermesLiteRxOnly));
    }

    void andromedaDoesNotShowOrionPanel()
    {
        // Andromeda console (Ganymede PA trip) is a NereusSDR-original SKU
        // slot; it is not present in the Thetis enable map, so helper
        // returns false (default branch).
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Andromeda));
    }

    void unknownDoesNotShowOrionPanel()
    {
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Unknown));
    }
};

QTEST_MAIN(TestAudioTxInputPerSkuGating)
#include "tst_audio_tx_input_per_sku_gating.moc"
