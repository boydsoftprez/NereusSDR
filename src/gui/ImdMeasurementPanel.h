#pragma once
//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2026 Richard Samphire (MW0LGE)
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


// Ported from Thetis Project Files/Source/Console/display.cs (tone separation readout).
// Modification history (NereusSDR):
//   2026-10-13: J.J. Boyd (KG4VCF), OpenAI Codex; rail and optional bench inspector.
//   2026-10-25: J.J. Boyd (KG4VCF), Claude Code; tone separation label paired with
//               the implementation's "f0 diff" port; upstream notice added.
//   2026-10-26: J.J. Boyd (KG4VCF), Claude Code; drop the no-port-check
//               marker now that the pair ports display.cs.
#include "gui/ImdMeasurementFrame.h"
#include "gui/NereusGuiExport.h"
#include <QString>
#include <QWidget>
#include <array>
class QLabel;
class QPushButton;
class QGridLayout;
class QResizeEvent;
class QKeyEvent;
namespace NereusSDR {
class NEREUS_GUI_EXPORT ImdMeasurementPanel : public QWidget {
    Q_OBJECT
public:
    enum class Presentation { Rail, Inspector, Window };
    explicit ImdMeasurementPanel(QWidget* parent = nullptr);
    void setPresentation(Presentation presentation);
    void setFrame(const ImdMeasurementFrame& frame);
    void setSelectedProduct(ImdProduct product);
    void setGuidesVisible(bool visible);
    void setInspectorVisible(bool visible);
    void setDisplayDuplexCapability(bool available, const QString& unavailableReason = {});
    ImdProduct selectedProduct() const { return m_selected; }
    const ImdMeasurementFrame& frame() const { return m_frame; }
    static QString statusText(ImdStatus status);
signals:
    void productSelected(ImdProduct product);
    void guidesVisibleChanged(bool visible);
    void inspectorVisibleChanged(bool visible);
    void hideMeasurementsRequested();
    void enableDisplayDuplexRequested();
protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    void refresh();
    void reflow();
    void chooseProduct(ImdProduct product);
    Presentation m_presentation{Presentation::Rail};
    ImdMeasurementFrame m_frame;
    ImdProduct m_selected{ImdProduct::ThirdLower};
    QLabel* m_status;
    QLabel* m_source;
    QLabel* m_third;
    QLabel* m_fifth;
    QLabel* m_selection;
    QLabel* m_reference;
    QLabel* m_toneSeparation;
    QLabel* m_diagnostics;
    QWidget* m_details;
    QWidget* m_controls;
    QPushButton* m_enableDuplex;
    QPushButton* m_guides;
    QPushButton* m_inspector;
    QPushButton* m_hideMeasurements;
    QGridLayout* m_summaryLayout;
    QGridLayout* m_pickerLayout;
    std::array<QPushButton*, 6> m_products;
    std::array<std::array<QLabel*, 3>, 6> m_values;
    bool m_displayDuplexAvailable{false};
    QString m_displayDuplexUnavailableReason;
};
} // namespace NereusSDR
