// =================================================================
// src/gui/meters/AntennaButtonItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs (column range), original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-25 - Saves and restores the grid styling (border, margin,
//                 corner radius, height ratio, fade on RX/TX) after the
//                 older fields. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Column range from Thetis setup.cs (columnRange), which
//                 the editor clamps to. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - On-state logic from Thetis setupButtons (setAntennas):
//                 Rx Ant 1-3 lit only while no receive-only input is in
//                 use, the receive-only and XVTR inputs lit by name, Tx Ant
//                 1-3 from the transmit slice. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
*/
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

// --- From setup.cs ---

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "AntennaButtonItem.h"

#include <algorithm>

#include "core/SkuUiProfile.h"
#include "gui/UnbuiltFeatures.h"
#include "core/HpsdrModel.h"

namespace NereusSDR {

// From Thetis clsAntennaButtonBox (MeterManager.cs:9502+)
static const char* const kAntennaLabels[] = {
    "Rx1", "Rx2", "Rx3", "Aux1", "Aux2",
    "XVTR", "Tx1", "Tx2", "Tx3", "Rx/Tx"
};

namespace {
// From Thetis MeterManager.cs:9651 [v2.10.3.15]:
//   VisibleBits = (1 << 10) - 1; //1023, 10 bits
constexpr uint32_t kNewGridVisibleBits = (1u << 10) - 1;

// "ANT1" .. "ANT3" as port 0-2; anything else -1.
int antennaPort(const QString& name)
{
    if (name == QLatin1String("ANT1")) { return 0; }
    if (name == QLatin1String("ANT2")) { return 1; }
    if (name == QLatin1String("ANT3")) { return 2; }
    return -1;
}
} // namespace

AntennaButtonItem::AntennaButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kAntennaCount);
    setColumns(5);
    setCornerRadius(3.0f);
    setVisibleBits(kNewGridVisibleBits);

    for (int i = 0; i < kAntennaCount; ++i) {
        setupButton(i, QString::fromLatin1(kAntennaLabels[i]));
    }
    // Rx: LimeGreen
    for (int i = 0; i < 3; ++i) { button(i).onColour = QColor(0x00, 0xff, 0x00); }
    // Aux + XVTR: Orange
    button(3).onColour = QColor(0xff, 0xa5, 0x00);
    button(4).onColour = QColor(0xff, 0xa5, 0x00);
    button(5).onColour = QColor(0xff, 0xa5, 0x00);
    // Tx: Red
    for (int i = 6; i < 9; ++i) { button(i).onColour = QColor(0xff, 0x44, 0x44); }
    // Toggle: Yellow
    button(9).onColour = QColor(0xff, 0xff, 0x00);
    // R-R3-49: the XVTR input goes with the transverters, and Rx/Tx has no
    // NereusSDR setting behind it; neither is drawn until built (the saved
    // visibility is untouched).
    setButtonHiddenUntilBuilt(5, !UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters));
    setButtonHiddenUntilBuilt(9, !UnbuiltFeatures::isBuilt(UnbuiltFeature::AntennaRxTxSplit));

    connect(this, &ButtonBoxItem::buttonClicked, this, &AntennaButtonItem::onButtonClicked);
}

void AntennaButtonItem::setActiveRxAntenna(int index)
{
    if (index < 0 || index >= kFirstTxButton) { index = -1; }
    if (m_activeRxAnt == index) { return; }
    if (m_activeRxAnt >= 0) { button(m_activeRxAnt).on = false; }
    m_activeRxAnt = index;
    if (m_activeRxAnt >= 0) { button(m_activeRxAnt).on = true; }
}

void AntennaButtonItem::setActiveTxAntenna(int index)
{
    if (index < 0 || index >= 3) { index = -1; }
    if (m_activeTxAnt == index) { return; }
    if (m_activeTxAnt >= 0) { button(kFirstTxButton + m_activeTxAnt).on = false; }
    m_activeTxAnt = index;
    if (m_activeTxAnt >= 0) { button(kFirstTxButton + m_activeTxAnt).on = true; }
}

void AntennaButtonItem::setAntennas(const QString& rxAntenna, const QString& txAntenna,
                                    const std::array<QString, 3>& rxOnlyInputs)
{
    // From Thetis MeterManager.cs:9689 [v2.10.3.15]: every button starts
    // off (SetOn(1, i, false)); setActive* below light at most one receive
    // and one transmit button.
    //
    // From Thetis MeterManager.cs:9708-9717 [v2.10.3.15]:
    //   bool using_xvtr = ((int)rx_band >= (int)Band.VHF0 && (int)rx_band <= (int)Band.VHF13);
    //   bool is_xvtr_rx = false;
    //   ...
    //   bool using_byp_ext1 = !using_xvtr && (GetRXAuxState(rx_band, 0) || GetRXAuxState(rx_band, 1));
    // NereusSDR: the slice names its receive input itself, so the transverter
    // input and the receive-only inputs are found by the radio's names for
    // them (the names a click on buttons 3-4 writes), not from the band.
    const auto isInput = [&rxAntenna, &rxOnlyInputs](size_t n) {
        return !rxAntenna.isEmpty() && rxAntenna == rxOnlyInputs[n];
    };
    const bool isXvtrRx = isInput(2);
    const bool usingBypExt1 = !isXvtrRx && (isInput(0) || isInput(1));

    int rx = -1;
    if (!usingBypExt1 && !isXvtrRx) {
        // From Thetis MeterManager.cs:9721, :9725, :9729 [v2.10.3.15]:
        //   if (!using_byp_ext1 && !is_xvtr_rx) SetOn(1, n, MeterManager.GetRXAntState(rx_band, n));
        rx = antennaPort(rxAntenna);
    } else if (usingBypExt1) {
        // From Thetis MeterManager.cs:9734, :9738 [v2.10.3.15]:
        //   if (!is_xvtr_rx && using_byp_ext1) SetOn(1, 3 + n, MeterManager.GetRXAuxState(rx_band, n));
        rx = isInput(0) ? 3 : 4;
    } else {
        // From Thetis MeterManager.cs:9742 [v2.10.3.15]:
        //   if (!using_byp_ext1 && using_xvtr) SetOn(1, 5, MeterManager.GetRXAuxState(rx_band, 2));
        rx = kXvtrButton;
    }
    setActiveRxAntenna(rx);

    // From Thetis MeterManager.cs:9748-9779 [v2.10.3.15]: Tx Ant n is lit
    // from the transmit band's antenna, SetOn(1, 6 + n,
    // MeterManager.GetTXAntState(tx_band, n)), unless the receive antenna on
    // the transmit band is a port marked do-not-transmit (GetNoTX), which
    // greys it. NereusSDR keeps a port blocked for transmit off a band's
    // transmit antenna (AlexAntennaFacade::setTxAntForBand), so the
    // transmit slice's antenna is never such a port.
    setActiveTxAntenna(antennaPort(txAntenna));

    // From Thetis MeterManager.cs:9781-9799 [v2.10.3.15]: Rx/Tx is lit only
    // while the receive and transmit antennas are swapped (_rxtx_swap).
    // NereusSDR has no swap (UnbuiltFeature::AntennaRxTxSplit), so it is
    // never lit.
    button(kRxTxButton).on = false;
}

void AntennaButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    // Phase 3P-I-a T17 — read-only when the board has no Alex front-end.
    // HL2/Atlas have no antenna relay, so a click would be a no-op at the
    // protocol layer; short-circuit here to stop zombie signal paths
    // (and to avoid visually flipping the button `on` state).
    if (!m_hasAlex) { return; }
    if (btn == Qt::LeftButton) { emit antennaSelected(index); }
}

// Phase 3P-I-b T10 — retarget RX-only button labels (slots 3/4/5) per SKU.
// Indices 3-5 are the RX-only inputs (Thetis "Aux1/Aux2/XVTR"); their labels
// map to SkuUiProfile.rxOnlyLabels per Thetis setup.cs:19846-20375.
// TX (6-8) + TRX (0-2) + Rx/Tx toggle (9) labels stay generic.
void AntennaButtonItem::setHpsdrSku(HPSDRModel sku)
{
    const SkuUiProfile profile = skuUiProfileFor(sku);
    // rxOnlyLabels[0] = wire value 1, maps to button index 3 (Aux1 slot).
    // rxOnlyLabels[1] = wire value 2, maps to button index 4 (Aux2 slot).
    // rxOnlyLabels[2] = wire value 3, maps to button index 5 (XVTR slot).
    for (int i = 0; i < 3; ++i) {
        if (button(3 + i).text != profile.rxOnlyLabels[i]) {
            button(3 + i).text = profile.rxOnlyLabels[i];
        }
    }
}

void AntennaButtonItem::setHasAlex(bool hasAlex)
{
    if (m_hasAlex == hasAlex) { return; }
    m_hasAlex = hasAlex;
    // No explicit repaint hook on MeterItem; the container repaints on
    // its own tick so the state change will become visible on the next
    // frame. If we later add a disabled-render mode, emit a redraw here.
}

QString AntennaButtonItem::serialize() const
{
    return QStringLiteral("ANTENNABTNS|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(visibleBits())
        + serializeGridStyle();
}

bool AntennaButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("ANTENNABTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setVisibleBits(parts[8].toUInt()); }
    deserializeGridStyle(parts, 9);
    return true;
}

namespace {
// From Thetis setup.cs:25834-25836 [v2.10.3.15]: Math.Max(1, max_buttons)
constexpr int kAntennaGridMinColumns = 1;
} // namespace

// From Thetis setup.cs:25834-25836 [v2.10.3.15] (ANTENNA_BUTTONS): up to
// one column per checked button (getTotalColumnsNeededForAntennaButtons,
// setup.cs:32113-32127), at least one.
std::pair<int, int> AntennaButtonItem::columnRange() const
{
    return {kAntennaGridMinColumns, std::max(kAntennaGridMinColumns, checkedButtonCount())};
}

} // namespace NereusSDR
