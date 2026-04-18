// =================================================================
// tests/tst_fm_opt_container_wire.cpp  (NereusSDR)
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
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include "gui/widgets/VfoModeContainers.h"
#include "gui/widgets/ScrollableLabel.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

// Helper to find a named child widget
template<typename T>
static T* findNamed(QWidget* parent, const char* name) {
    return parent->findChild<T*>(QString::fromLatin1(name));
}

class TestFmOptContainerWire : public QObject {
    Q_OBJECT

private slots:
    // ── CTCSS mode round-trip ─────────────────────────────────────────────────

    void ctcssModeStoredInSlice() {
        // Setting via combo → SliceModel stores it
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneModeCmb");
        QVERIFY(cmb != nullptr);

        // Select "CTCSS Decode" (index 2, data value 2)
        cmb->setCurrentIndex(2);
        QCOMPARE(s.fmCtcssMode(), 2);
    }

    void ctcssValueHzStoredInSlice() {
        // Setting tone value combo → SliceModel stores the Hz value
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneValueCmb");
        QVERIFY(cmb != nullptr);

        // Find the 100.0 Hz entry and select it
        const int idx = cmb->findText(QStringLiteral("100.0"));
        QVERIFY(idx >= 0);
        cmb->setCurrentIndex(idx);
        QCOMPARE(s.fmCtcssValueHz(), 100.0);
    }

    void ctcssModeSyncFromSlice() {
        // SliceModel state → combo reflects it after syncFromSlice
        SliceModel s;
        s.setFmCtcssMode(3);  // CTCSS Enc+Dec

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneModeCmb");
        QVERIFY(cmb != nullptr);
        QCOMPARE(cmb->itemData(cmb->currentIndex()).toInt(), 3);
    }

    // ── Offset round-trip ─────────────────────────────────────────────────────

    void offsetStoredInSlice() {
        // Spinning the offset spinbox → SliceModel stores it in Hz
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* spin = findNamed<QSpinBox>(&c, "offsetKhzSpin");
        QVERIFY(spin != nullptr);

        spin->setValue(600);  // 600 kHz
        QCOMPARE(s.fmOffsetHz(), 600000);
    }

    void offsetSyncFromSlice() {
        // SliceModel stores Hz → spinbox shows kHz
        SliceModel s;
        s.setFmOffsetHz(146000);  // 146 kHz (common 2m repeater offset)

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* spin = findNamed<QSpinBox>(&c, "offsetKhzSpin");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 146);
    }

    // ── Simplex button — Thetis-faithful behavior ─────────────────────────────
    //
    // From Thetis console.cs:40412 chkFMTXSimplex_CheckedChanged:
    // Simplex only sets the TX mode enum; it does NOT zero udFMOffset.
    // The repeater offset spinbox retains its value — TX effects are Phase 3M-1.

    void simplexSetsModeSimplex() {
        SliceModel s;
        s.setFmTxMode(FmTxMode::Low);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* simplexBtn = findNamed<QPushButton>(&c, "simplexBtn");
        QVERIFY(simplexBtn != nullptr);
        simplexBtn->click();

        QCOMPARE(s.fmTxMode(), FmTxMode::Simplex);
    }

    void simplexDoesNotZeroOffset() {
        // Source-first: Thetis console.cs:40412 — simplex does not zero offset.
        // The offset spinbox retains its value when simplex is selected.
        SliceModel s;
        s.setFmOffsetHz(600000);
        s.setFmTxMode(FmTxMode::High);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* simplexBtn = findNamed<QPushButton>(&c, "simplexBtn");
        QVERIFY(simplexBtn != nullptr);
        simplexBtn->click();

        // TX mode switches to Simplex
        QCOMPARE(s.fmTxMode(), FmTxMode::Simplex);
        // Offset is preserved — not zeroed (Thetis-faithful)
        QCOMPARE(s.fmOffsetHz(), 600000);
    }

    // ── Reverse toggle — display-only ────────────────────────────────────────

    void reverseToggleStored() {
        SliceModel s;
        s.setFmReverse(false);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* revBtn = findNamed<QPushButton>(&c, "revBtn");
        QVERIFY(revBtn != nullptr);
        revBtn->click();  // toggles checked state

        QVERIFY(s.fmReverse());
    }

    // ── TX direction button mutual exclusion ──────────────────────────────────

    void lowButtonChecksOnlyLow() {
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);

        auto* txLow   = findNamed<QPushButton>(&c, "txLowBtn");
        auto* simplex = findNamed<QPushButton>(&c, "simplexBtn");
        auto* txHigh  = findNamed<QPushButton>(&c, "txHighBtn");
        QVERIFY(txLow && simplex && txHigh);

        txLow->click();
        QVERIFY(txLow->isChecked());
        QVERIFY(!simplex->isChecked());
        QVERIFY(!txHigh->isChecked());
        QCOMPARE(s.fmTxMode(), FmTxMode::Low);
    }

    void highButtonChecksOnlyHigh() {
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);

        auto* txLow   = findNamed<QPushButton>(&c, "txLowBtn");
        auto* simplex = findNamed<QPushButton>(&c, "simplexBtn");
        auto* txHigh  = findNamed<QPushButton>(&c, "txHighBtn");
        QVERIFY(txLow && simplex && txHigh);

        txHigh->click();
        QVERIFY(!txLow->isChecked());
        QVERIFY(!simplex->isChecked());
        QVERIFY(txHigh->isChecked());
        QCOMPARE(s.fmTxMode(), FmTxMode::High);
    }
};

QTEST_MAIN(TestFmOptContainerWire)
#include "tst_fm_opt_container_wire.moc"
