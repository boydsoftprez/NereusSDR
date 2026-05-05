/*  gen.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2017, 2021, 2023 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

// =================================================================
// tests/tst_tx_channel_post_gen_tone_mag.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel::setPostGenToneMag wrapper added in
// Issue #175 Task 2.
//
// Spec §5.5; Plan Task 2; Issue #175.
//
// Wraps WDSP SetTXAPostGenToneMag (third_party/wdsp/src/gen.c:800-805
// [v2.10.3.13]).  Used by Task 4 (TransmitModel::setPowerUsingTargetDbm)
// for HL2 sub-step DSP audio-gain modulation.
//
// Source: mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]:
//   if (new_pwr <= 51) {
//       radio.GetDSPTX(0).TXPostGenToneMag = (double)(new_pwr + 40) / 100;
//       new_pwr = 0;
//   } else {
//       radio.GetDSPTX(0).TXPostGenToneMag = 0.9999;
//       new_pwr = (new_pwr - 54) * 2;
//   }
//
// Test strategy: round-trip QCOMPARE (store-and-retrieve) rather than
// smoke-only, because Task 4 reads the cached value back via
// postGenToneMag().  WDSP setter calls fall through the
// `txa[m_channelId].rsmpin.p == nullptr` null-guard (channel was never
// opened via OpenChannel in unit tests).  Without HAVE_WDSP, the stub
// path is exercised.  Either way the setter must store and the getter
// must return the value.
//
// 2 test cases:
//   setPostGenToneMag_storesAndRetrieves — typical HL2 sub-step values
//   setPostGenToneMag_acceptsBounds      — boundary values 0.0 and 1.0
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-04 — New test for Issue #175 Task 2: TxChannel::setPostGenToneMag
//                 WDSP wrapper (round-trip store/retrieve). J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All mi0bot-Thetis source
// cites are in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TestTxChannelPostGenToneMag : public QObject {
    Q_OBJECT

private slots:

    // ── setPostGenToneMag: typical HL2 sub-step values ───────────────────────
    //
    // From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]:
    //   TXPostGenToneMag = (double)(new_pwr + 40) / 100  // sub-step path
    //   TXPostGenToneMag = 0.9999                        // upper path
    //
    // Tests representative values across the HL2 sub-step range (0.4..0.9999).

    void setPostGenToneMag_storesAndRetrieves()
    {
        TxChannel ch(kTxChannelId);

        ch.setPostGenToneMag(0.5);
        QCOMPARE(ch.postGenToneMag(), 0.5);

        ch.setPostGenToneMag(0.4);
        QCOMPARE(ch.postGenToneMag(), 0.4);

        ch.setPostGenToneMag(0.9999);
        QCOMPARE(ch.postGenToneMag(), 0.9999);
    }

    // ── setPostGenToneMag: boundary values ───────────────────────────────────
    //
    // 0.0 (silence) and 1.0 (kMaxToneMag, non-HL2 default).
    // From mi0bot-Thetis console.cs:30039 [v2.10.3.13]:
    //   radio.GetDSPTX(0).TXPostGenToneMag = MAX_TONE_MAG;  // = 0.99999 or 1.0

    void setPostGenToneMag_acceptsBounds()
    {
        TxChannel ch(kTxChannelId);

        ch.setPostGenToneMag(0.0);
        QCOMPARE(ch.postGenToneMag(), 0.0);

        ch.setPostGenToneMag(1.0);
        QCOMPARE(ch.postGenToneMag(), 1.0);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelPostGenToneMag)
#include "tst_tx_channel_post_gen_tone_mag.moc"
