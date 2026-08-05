// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - tst_tx_channel_rade_mode_mapping
//
// Pins the Phase 3R K-bench contract: TxChannel::setTxMode maps
// DSPMode::RADE_U -> DSPMode::USB and DSPMode::RADE_L -> DSPMode::LSB
// before calling SetTXAMode. WDSP's modulator stage only accepts modes
// 0..11 (LSB..DRM); passing RADE_U=12 or RADE_L=13 produces no I/Q
// output, which was the user-reported "no RF on the air" symptom.
//
// The test seam lastWdspTxModeForTest() returns the mapped value
// (not the carry m_mode which preserves RADE_U/L for NereusSDR-side
// dispatch). USB/LSB/CW/AM/FM/DIGU/DIGL/SAM/DSB/SPEC/DRM pass through
// unchanged.
//
// Modification history (NereusSDR):
//   Created 2026-05-11 by JJ Boyd / KG4VCF.  Reproducer for the
//   bench-reported "RADE-U + MOX = no RF" symptom from commit
//   b79f577d's K-bench wire-up.
//   AI tooling: Anthropic Claude Code.

#include <QObject>
#include <QtTest>

#include "core/TxChannel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

namespace {

static constexpr int kTxChannelId = 1;

}  // namespace

class TestTxChannelRadeModeMapping : public QObject {
    Q_OBJECT

private slots:

    void radeUpperMapsToUsb() {
        TxChannel ch(kTxChannelId);
        ch.setTxMode(DSPMode::RADE_U);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::USB);
    }

    void radeLowerMapsToLsb() {
        TxChannel ch(kTxChannelId);
        ch.setTxMode(DSPMode::RADE_L);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::LSB);
    }

    void usbPassesThroughUnchanged() {
        TxChannel ch(kTxChannelId);
        ch.setTxMode(DSPMode::USB);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::USB);
    }

    void lsbPassesThroughUnchanged() {
        TxChannel ch(kTxChannelId);
        ch.setTxMode(DSPMode::LSB);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::LSB);
    }

    void allOtherModesPassThroughUnchanged() {
        const DSPMode others[] = {
            DSPMode::DSB,  DSPMode::CWL, DSPMode::CWU, DSPMode::FM,
            DSPMode::AM,   DSPMode::DIGU, DSPMode::SPEC, DSPMode::DIGL,
            DSPMode::SAM,  DSPMode::DRM,
        };
        for (DSPMode m : others) {
            TxChannel ch(kTxChannelId);
            ch.setTxMode(m);
            QCOMPARE(ch.lastWdspTxModeForTest(), m);
        }
    }

    void radeSwitchBackToUsbResetsMapping() {
        TxChannel ch(kTxChannelId);
        ch.setTxMode(DSPMode::RADE_U);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::USB);
        ch.setTxMode(DSPMode::DIGU);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::DIGU);
        ch.setTxMode(DSPMode::RADE_L);
        QCOMPARE(ch.lastWdspTxModeForTest(), DSPMode::LSB);
    }
};

QTEST_GUILESS_MAIN(TestTxChannelRadeModeMapping)
#include "tst_tx_channel_rade_mode_mapping.moc"
