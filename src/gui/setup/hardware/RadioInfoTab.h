#pragma once

// =================================================================
// src/gui/setup/hardware/RadioInfoTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 5: test access to
//                 the support info. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

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

#include <QWidget>
#include <QVariant>

class QCheckBox;
class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;
class QFormLayout;
class QFrame;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class RadioInfoTab : public QWidget {
    Q_OBJECT
public:
    explicit RadioInfoTab(RadioModel* model, QWidget* parent = nullptr);
    // Called by HardwarePage when the connected radio changes.
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    // Restore persisted control values (Phase 3I Task 21).
    void restoreSettings(const QMap<QString, QVariant>& settings);

#ifdef NEREUS_BUILD_TESTS
    // Plan Task 5: the Copy Support Info text, which carries the top sample
    // rate for the protocol the radio is running.
    QString supportInfoForTest() const { return m_currentInfo; }
#endif

signals:
    void settingChanged(const QString& key, const QVariant& value);
    // Emitted when the ANAN-8000DLE "Show volts/amps" checkbox is toggled.
    // Only reachable when the connected radio is an ANAN-8000D; forwarded by
    // SetupDialog → MainWindow::setVoltsAmpsVisible().
    void anan8000DleVoltsAmpsChanged(bool visible);

private slots:
    void onSampleRateChanged(int index);
    void onWireSampleRateChanged(double hz);

private:
    void updateReconnectBanner();

    RadioModel*  m_model{nullptr};

    QLabel*      m_boardLabel{nullptr};
    QLabel*      m_protocolLabel{nullptr};
    QLabel*      m_adcCountLabel{nullptr};
    QLabel*      m_maxRxLabel{nullptr};
    QLabel*      m_firmwareLabel{nullptr};
    QLabel*      m_macLabel{nullptr};
    QLabel*      m_ipLabel{nullptr};
    QComboBox*   m_sampleRateRx1Combo{nullptr};
    QFrame*      m_reconnectBanner{nullptr};
    QLabel*      m_reconnectBannerLabel{nullptr};
    int          m_activeWireRate{0}; // last rate reported via wireSampleRateChanged
    QPushButton* m_copySupportInfoButton{nullptr};

    // ANAN-8000DLE only — shown via capability gate in populate()
    QCheckBox*   m_anan8000DleVoltsAmpsToggle{nullptr};

    // Cached for copy-to-clipboard
    QString m_currentInfo;
};

} // namespace NereusSDR
