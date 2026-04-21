// =================================================================
// tests/tst_radio_status.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source (SWR formula):
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 SWR test vectors from console.cs:6642 [@501e3f5].
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
#include <cmath>

#include "core/RadioStatus.h"

using namespace NereusSDR;

class TestRadioStatus : public QObject {
    Q_OBJECT
private slots:

    // ── Initial state ──────────────────────────────────────────────────────

    void initialState_isZeroAndNone()
    {
        RadioStatus rs;
        QCOMPARE(rs.forwardPowerWatts(), 0.0);
        QCOMPARE(rs.reflectedPowerWatts(), 0.0);
        QCOMPARE(rs.swrRatio(), 1.0);
        QCOMPARE(rs.paTemperatureCelsius(), 0.0);
        QCOMPARE(rs.paCurrentAmps(), 0.0);
        QCOMPARE(rs.activePttSource(), PttSource::None);
        QVERIFY(!rs.isTransmitting());
        QVERIFY(rs.recentPttEvents().isEmpty());
    }

    // ── Setters update state + emit signals ───────────────────────────────

    void setForwardPower_updatesAndEmits()
    {
        RadioStatus rs;
        QSignalSpy spy(&rs, &RadioStatus::powerChanged);
        rs.setForwardPower(100.0);
        QCOMPARE(rs.forwardPowerWatts(), 100.0);
        QCOMPARE(spy.count(), 1);
    }

    void setReflectedPower_updatesAndEmits()
    {
        RadioStatus rs;
        QSignalSpy spy(&rs, &RadioStatus::powerChanged);
        rs.setForwardPower(100.0);  // set fwd first
        spy.clear();
        rs.setReflectedPower(11.1);
        QCOMPARE(rs.reflectedPowerWatts(), 11.1);
        QCOMPARE(spy.count(), 1);
    }

    void setPaTemperature_updatesAndEmits()
    {
        RadioStatus rs;
        QSignalSpy spy(&rs, &RadioStatus::paTemperatureChanged);
        rs.setPaTemperature(45.5);
        QCOMPARE(rs.paTemperatureCelsius(), 45.5);
        QCOMPARE(spy.count(), 1);
    }

    void setPaCurrent_updatesAndEmits()
    {
        RadioStatus rs;
        QSignalSpy spy(&rs, &RadioStatus::paCurrentChanged);
        rs.setPaCurrent(2.5);
        QCOMPARE(rs.paCurrentAmps(), 2.5);
        QCOMPARE(spy.count(), 1);
    }

    void setPaTemperature_idempotentNoExtraSignal()
    {
        RadioStatus rs;
        rs.setPaTemperature(45.5);
        QSignalSpy spy(&rs, &RadioStatus::paTemperatureChanged);
        rs.setPaTemperature(45.5);  // same value
        QCOMPARE(spy.count(), 0);
    }

    // ── SWR computation ───────────────────────────────────────────────────

    void swr_bothZero_isUnity()
    {
        // From Thetis console.cs:6644 [@501e3f5]:
        //   if (adc_fwd == 0 && adc_rev == 0) return 1.0;
        RadioStatus rs;
        QCOMPARE(rs.swrRatio(), 1.0);
    }

    void swr_approx2_forStandardTestVector()
    {
        // Test vector: fwd=100 W, refl=11.111 W → rho ≈ 0.333 → SWR ≈ 2.0
        // This mirrors the ARRL formula equivalent to Thetis console.cs:6642
        // SWR(adc_fwd, adc_rev) [@501e3f5].
        RadioStatus rs;
        rs.setForwardPower(100.0);
        rs.setReflectedPower(11.111);
        double swr = rs.swrRatio();
        // Allow ±0.05 tolerance for floating point
        QVERIFY2(std::abs(swr - 2.0) < 0.05,
                 qPrintable(QStringLiteral("SWR expected ~2.0, got %1").arg(swr)));
    }

    void swr_reflGreaterThanFwd_returns50()
    {
        // From Thetis console.cs:6646 [@501e3f5]:
        //   else if (adc_rev > adc_fwd) return 50.0;
        RadioStatus rs;
        rs.setForwardPower(10.0);
        rs.setReflectedPower(20.0);  // refl > fwd
        QCOMPARE(rs.swrRatio(), 50.0);
    }

    void swr_perfectMatch_isUnity()
    {
        // refl == 0 → rho == 0 → SWR == 1.0
        RadioStatus rs;
        rs.setForwardPower(100.0);
        rs.setReflectedPower(0.0);
        QCOMPARE(rs.swrRatio(), 1.0);
    }

    // ── PTT state machine ─────────────────────────────────────────────────

    void setPttSource_mox_flipsTransmitting()
    {
        RadioStatus rs;
        QSignalSpy spy(&rs, &RadioStatus::pttChanged);
        rs.setActivePttSource(PttSource::Mox);
        QVERIFY(rs.isTransmitting());
        QCOMPARE(rs.activePttSource(), PttSource::Mox);
        QCOMPARE(spy.count(), 1);
    }

    void setPttSource_none_clearsTransmitting()
    {
        RadioStatus rs;
        rs.setActivePttSource(PttSource::Cat);
        QVERIFY(rs.isTransmitting());

        QSignalSpy spy(&rs, &RadioStatus::pttChanged);
        rs.setActivePttSource(PttSource::None);
        QVERIFY(!rs.isTransmitting());
        QCOMPARE(spy.count(), 1);
    }

    void setTransmitting_false_clearsSource()
    {
        RadioStatus rs;
        rs.setActivePttSource(PttSource::Vox);
        QVERIFY(rs.isTransmitting());

        rs.setTransmitting(false);
        QVERIFY(!rs.isTransmitting());
        QCOMPARE(rs.activePttSource(), PttSource::None);
    }

    // ── PTT history cap ───────────────────────────────────────────────────

    void pttHistory_capsAt8()
    {
        RadioStatus rs;

        // Toggle PTT 10 times — should only keep the last 8 events.
        for (int i = 0; i < 10; ++i) {
            rs.setActivePttSource(PttSource::Mox);
            rs.setActivePttSource(PttSource::None);
        }

        QVERIFY2(rs.recentPttEvents().size() <= 8,
                 qPrintable(QStringLiteral("PTT history size %1 exceeds cap of 8")
                                .arg(rs.recentPttEvents().size())));
    }

    void pttHistory_mostRecentFirst()
    {
        RadioStatus rs;
        rs.setActivePttSource(PttSource::Mox);
        rs.setActivePttSource(PttSource::None);
        rs.setActivePttSource(PttSource::Cat);

        const auto& events = rs.recentPttEvents();
        QVERIFY(!events.isEmpty());
        // Most recent event is at index 0.
        QCOMPARE(events.first().source, PttSource::Cat);
    }

    // ── Exciter power ─────────────────────────────────────────────────────

    void setExciterPowerMw_storesValue()
    {
        RadioStatus rs;
        rs.setExciterPowerMw(500);
        QCOMPARE(rs.exciterPowerMw(), 500);
    }
};

QTEST_GUILESS_MAIN(TestRadioStatus)
#include "tst_radio_status.moc"
