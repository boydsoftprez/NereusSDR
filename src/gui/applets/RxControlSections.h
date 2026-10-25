#pragma once

// =================================================================
// src/gui/applets/RxControlSections.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// The ten RX container sections. Each one is a copy of one job from
// RxApplet (antennas, mode, step, filter, pan, squelch, attenuator, AGC,
// RIT, XIT). The applet keeps the wiring. A container copy wears the
// workspace face; the applet's own panel is not built from these objects.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - The RX Filter face defaults come from NereusButtonFace.h;
//                 the mode list comes from the applet only; Thetis headers
//                 byte for byte at v2.10.3.15. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-23 - RX Filter can hide its width readout and its passband.
//                 Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-23 - RX applet jobs as container sections. Brian Sutton,
//                 AI-assisted via xAI Grok.
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//
// Upstream source 'Project Files/Source/Console/console.resx' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/RxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

#include "gui/NereusGuiExport.h"
#include "gui/meters/NereusButtonFace.h"

#include <QColor>
#include <QJsonObject>
#include <QMargins>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace NereusSDR {

class FilterPassbandWidget;
class TriBtn;

// The margins a copy gives itself: the RX panel column's own padding,
// so a section in a container has the same breathing room the row has
// in the applet. The applet's own panel does not use these objects.
inline QMargins rxCopyMargins()
{
    return QMargins(4, 2, 4, 2);
}

class NEREUS_GUI_EXPORT RxAntennasSection : public QWidget {
    Q_OBJECT
public:
    explicit RxAntennasSection(QWidget* parent = nullptr);
    QPushButton* rxButton() const { return m_rx; }
    QPushButton* txButton() const { return m_tx; }
private:
    QPushButton* m_rx = nullptr;
    QPushButton* m_tx = nullptr;
};

class NEREUS_GUI_EXPORT RxModeSection : public QWidget {
    Q_OBJECT
public:
    explicit RxModeSection(QWidget* parent = nullptr);
    QComboBox* combo() const { return m_combo; }
private:
    QComboBox* m_combo = nullptr;
};

class NEREUS_GUI_EXPORT RxStepSection : public QWidget {
    Q_OBJECT
public:
    explicit RxStepSection(QWidget* parent = nullptr);
    TriBtn* down() const { return m_down; }
    TriBtn* up() const { return m_up; }
    QLabel* label() const { return m_label; }
private:
    TriBtn* m_down = nullptr;
    TriBtn* m_up = nullptr;
    QLabel* m_label = nullptr;
};

class NEREUS_GUI_EXPORT RxFilterSection : public QWidget {
    Q_OBJECT
public:
    explicit RxFilterSection(QWidget* parent = nullptr);
    // Width readout, preset grid, passband. hideTop and hideBottom in
    // `config` hide the width and the passband. columns is 1..32, as the
    // Nereus band buttons. The five colours are that control's face.
    void applyConfig(const QJsonObject& config);
    int columns() const { return m_columns; }
    QString presetButtonStyle() const;
    QLabel* widthLabel() const { return m_width; }
    QGridLayout* grid() const { return m_grid; }
    QVector<QPushButton*>& buttons() { return m_buttons; }
    FilterPassbandWidget* passband() const { return m_passband; }
private:
    QLabel* m_width = nullptr;
    QGridLayout* m_grid = nullptr;
    QVector<QPushButton*> m_buttons;
    FilterPassbandWidget* m_passband = nullptr;
    int m_columns = 3;
    // The Nereus band button defaults (NereusButtonFace.h).
    QColor m_textColor{QColor(NereusButtonFace::kText)};
    QColor m_backgroundTop{QColor(NereusButtonFace::kRestingTop)};
    QColor m_backgroundBottom{QColor(NereusButtonFace::kRestingBottom)};
    QColor m_selectedTop{QColor(NereusButtonFace::kSelectedTop)};
    QColor m_selectedBottom{QColor(NereusButtonFace::kSelectedBottom)};
};

class NEREUS_GUI_EXPORT RxPanSection : public QWidget {
    Q_OBJECT
public:
    explicit RxPanSection(QWidget* parent = nullptr);
    QSlider* slider() const { return m_slider; }
private:
    QSlider* m_slider = nullptr;
};

class NEREUS_GUI_EXPORT RxSquelchSection : public QWidget {
    Q_OBJECT
public:
    explicit RxSquelchSection(QWidget* parent = nullptr);
    QPushButton* button() const { return m_button; }
    QSlider* slider() const { return m_slider; }
private:
    QPushButton* m_button = nullptr;
    QSlider* m_slider = nullptr;
};

class NEREUS_GUI_EXPORT RxAttenuatorSection : public QWidget {
    Q_OBJECT
public:
    explicit RxAttenuatorSection(QWidget* parent = nullptr);
    QLabel* label() const { return m_label; }
    QStackedWidget* stack() const { return m_stack; }
    QComboBox* preampCombo() const { return m_preamp; }
    QSpinBox* stepSpin() const { return m_step; }
    QCheckBox* rx1Preamp() const { return m_rx1Preamp; }
private:
    QLabel* m_label = nullptr;
    QStackedWidget* m_stack = nullptr;
    QComboBox* m_preamp = nullptr;
    QSpinBox* m_step = nullptr;
    QCheckBox* m_rx1Preamp = nullptr;
};

class NEREUS_GUI_EXPORT RxAgcSection : public QWidget {
    Q_OBJECT
public:
    explicit RxAgcSection(QWidget* parent = nullptr);
    QComboBox* combo() const { return m_combo; }
    QSlider* slider() const { return m_slider; }
    QLabel* valueLabel() const { return m_value; }
    QPushButton* autoButton() const { return m_auto; }
    QLabel* infoLabel() const { return m_info; }
private:
    QComboBox* m_combo = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_value = nullptr;
    QPushButton* m_auto = nullptr;
    QLabel* m_info = nullptr;
};

class NEREUS_GUI_EXPORT RxRitSection : public QWidget {
    Q_OBJECT
public:
    explicit RxRitSection(QWidget* parent = nullptr);
    QPushButton* onButton() const { return m_on; }
    QPushButton* zeroButton() const { return m_zero; }
    TriBtn* minus() const { return m_minus; }
    TriBtn* plus() const { return m_plus; }
    QLabel* label() const { return m_label; }
private:
    QPushButton* m_on = nullptr;
    QPushButton* m_zero = nullptr;
    TriBtn* m_minus = nullptr;
    TriBtn* m_plus = nullptr;
    QLabel* m_label = nullptr;
};

class NEREUS_GUI_EXPORT RxXitSection : public QWidget {
    Q_OBJECT
public:
    explicit RxXitSection(QWidget* parent = nullptr);
    QPushButton* onButton() const { return m_on; }
    QPushButton* zeroButton() const { return m_zero; }
    TriBtn* minus() const { return m_minus; }
    TriBtn* plus() const { return m_plus; }
    QLabel* label() const { return m_label; }
private:
    QPushButton* m_on = nullptr;
    QPushButton* m_zero = nullptr;
    TriBtn* m_minus = nullptr;
    TriBtn* m_plus = nullptr;
    QLabel* m_label = nullptr;
};

} // namespace NereusSDR
