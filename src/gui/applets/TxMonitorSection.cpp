#include "TxMonitorSection.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/VfoWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

// =================================================================
// src/gui/applets/TxMonitorSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the MON toggle, the SPEAKERS /
// PHONES output pair, the reason MON is silent, and the monitor volume
// slider.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-20 - A copy of this row in a container is told the outer
//                 margins the applet's column gives the row (outerMargins);
//                 the applet's own row is told none.
//                 Brian Sutton, AI-assisted via OpenCode.
//   2026-10-19 - Extracted from TxApplet.{h,cpp} as part of splitting the
//                 TX applet into per-concern section objects. Behaviour,
//                 appearance, object names and tooltips are unchanged.
//                 Brian Sutton, AI-assisted via OpenAI Codex.
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

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

namespace NereusSDR {

namespace {

// R-R3-45: the MON output captions, in one place so a rename is one edit.
// They match the receiver flag's output buttons (VfoWidget), in capitals
// like the flag's other buttons.
QString monitorSpeakersCaption()   { return QStringLiteral("SPEAKERS"); }
QString monitorHeadphonesCaption() { return QStringLiteral("PHONES"); }

} // namespace

TxMonitorSection::TxMonitorSection(QWidget* parent, int rowSpacing,
                                   const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxMonitorSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

// MON button row
auto* monRow = new QHBoxLayout;
monRow->setSpacing(4);

m_monBtn = new QPushButton(QStringLiteral("MON"), this);
m_monBtn->setCheckable(true);
m_monBtn->setChecked(false);  // default: OFF — plan §0 row 9 safety rule
m_monBtn->setFixedHeight(22);
m_monBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
// MON button intentionally uses dark-navy/cyan (#001a33 bg / #3399ff border)
// to distinguish from generic blue toggles (kBlueBg=#0070c0/kBlueBorder=#0090e0).
// NereusSDR-original one-off — do NOT snap to Style::kBlueBg / kBlueBorder.
m_monBtn->setStyleSheet(Style::buttonBaseStyle()
    + QStringLiteral("QPushButton:checked {"
                     " background: #001a33;"
                     " border: 1px solid #3399ff;"
                     " color: #ffffff;"
                     "}"));
m_monBtn->setAccessibleName(QStringLiteral("Monitor enable"));
// R-R3-21: MON plays your own transmitted audio as it sounds on the
// air, in the output chosen beside it (R-R3-45).
m_monBtn->setToolTip(QStringLiteral(
    "Monitor: hear your own transmitted audio as it sounds on the air, "
    "on the output chosen next to MON (SPEAKERS or PHONES).\n"
    "MON is off each time NereusSDR starts, for safety."));
monRow->addWidget(m_monBtn, 1);

// R-R3-45: where MON plays, beside MON. An exclusive pair in the
// LEV / EQ / CFC button family; speakers by default. NereusSDR-native:
// Thetis mixes MON into its one output.
const QString outStyle = Style::buttonBaseStyle()
    + Style::greenCheckedStyle();
m_monSpeakersBtn = new QPushButton(monitorSpeakersCaption(), this);
m_monSpeakersBtn->setObjectName(QStringLiteral("TxMonitorSpeakersButton"));
m_monSpeakersBtn->setCheckable(true);
m_monSpeakersBtn->setChecked(true);
m_monSpeakersBtn->setFixedHeight(22);
m_monSpeakersBtn->setStyleSheet(outStyle);
m_monSpeakersBtn->setAccessibleName(QStringLiteral("Monitor on the speakers"));
m_monSpeakersBtn->setToolTip(QStringLiteral(
    "Play your transmit monitor on the speakers"));
monRow->addWidget(m_monSpeakersBtn);

m_monHeadphonesBtn = new QPushButton(monitorHeadphonesCaption(), this);
m_monHeadphonesBtn->setObjectName(QStringLiteral("TxMonitorHeadphonesButton"));
m_monHeadphonesBtn->setCheckable(true);
m_monHeadphonesBtn->setFixedHeight(22);
m_monHeadphonesBtn->setStyleSheet(outStyle);
m_monHeadphonesBtn->setAccessibleName(QStringLiteral("Monitor on the headphones"));
m_monHeadphonesBtn->setToolTip(QStringLiteral(
    "Play your transmit monitor on the headphones"));
monRow->addWidget(m_monHeadphonesBtn);

vbox->addLayout(monRow);

// Why MON is silent with the headphones chosen and none open: the
// receiver flag's own words.
m_monOutputNotice = new QLabel(VfoWidget::headphonesMissingText(), this);
m_monOutputNotice->setObjectName(QStringLiteral("TxMonitorOutputNotice"));
m_monOutputNotice->setWordWrap(true);
m_monOutputNotice->setStyleSheet(
    QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(Style::kAmberText));
m_monOutputNotice->setVisible(false);
vbox->addWidget(m_monOutputNotice);

// Monitor volume slider row
auto* volRow = new QHBoxLayout;
volRow->setSpacing(4);

auto* volLbl = new QLabel(QStringLiteral("Mon Vol:"), this);
volLbl->setFixedWidth(62);
volLbl->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
volRow->addWidget(volLbl);

// Range 0..100 integer; default 50 (model default 0.5f).
m_monitorVolumeSlider = new QSlider(Qt::Horizontal, this);
m_monitorVolumeSlider->setRange(0, 100);
m_monitorVolumeSlider->setValue(50);
m_monitorVolumeSlider->setFixedHeight(18);
m_monitorVolumeSlider->setAccessibleName(QStringLiteral("Monitor volume"));
// R-R3-21: sets the TX monitor's gain (TransmitModel::monitorVolume
// -> AudioEngine::setTxMonitorVolume), the transmitted audio MON
// plays in the output chosen beside it (R-R3-45).
m_monitorVolumeSlider->setToolTip(QStringLiteral(
    "How loud you hear your own transmitted audio while MON is on, "
    "on the output chosen next to MON (SPEAKERS or PHONES). 0 to 100."));
volRow->addWidget(m_monitorVolumeSlider, 1);

m_monitorVolumeValue = new QLabel(QStringLiteral("50"), this);
m_monitorVolumeValue->setFixedWidth(26);
m_monitorVolumeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
m_monitorVolumeValue->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));
volRow->addWidget(m_monitorVolumeValue);

vbox->addLayout(volRow);
}

// R-R3-45: the MON output pair shows the choice; clicking the checked one
// keeps it, as on the receiver flag.
void TxMonitorSection::showMonitorOutput(bool headphones)
{
    if (m_monSpeakersBtn) {
        QSignalBlocker b(m_monSpeakersBtn);
        m_monSpeakersBtn->setChecked(!headphones);
    }
    if (m_monHeadphonesBtn) {
        QSignalBlocker b(m_monHeadphonesBtn);
        m_monHeadphonesBtn->setChecked(headphones);
    }
    updateMonitorOutputNotice();
}

// R-R3-45: with the headphones chosen and none open, say why MON is silent
// in the flag's words: turned on but not opened, or not set up at all.
void TxMonitorSection::updateMonitorOutputNotice()
{
    if (!m_monOutputNotice) {
        return;
    }
    const bool headphones = m_monHeadphonesBtn && m_monHeadphonesBtn->isChecked();
    m_monOutputNotice->setText(m_headphonesEnabled ? VfoWidget::headphonesNotOpenedText()
                                                   : VfoWidget::headphonesMissingText());
    m_monOutputNotice->setVisible(headphones && !m_headphonesAvailable);
}

void TxMonitorSection::setHeadphonesAvailable(bool available)
{
    m_headphonesAvailable = available;
    updateMonitorOutputNotice();
}

void TxMonitorSection::setHeadphonesEnabled(bool enabled)
{
    m_headphonesEnabled = enabled;
    updateMonitorOutputNotice();
}

QString TxMonitorSection::outputUnavailableReason()
{
    return QStringLiteral(
        "This Core does not send the transmit monitor. Updating the Core may help.");
}

} // namespace NereusSDR
