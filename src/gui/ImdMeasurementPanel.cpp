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
//   2026-10-25: J.J. Boyd (KG4VCF), Claude Code; tone separation reading from the
//               measured fundamentals, the Thetis "f0 diff" value and format.
//   2026-10-25: J.J. Boyd (KG4VCF), Claude Code; each fundamental truncates
//               to whole Hz before the difference, as Thetis's (long) casts do.
//   2026-10-26: J.J. Boyd (KG4VCF), Claude Code; whole-Hz edge plus a
//               truncated offset, as display.cs builds f0l_freq and f0h_freq;
//               no-port-check marker dropped now that the file ports display.cs.
#include "gui/ImdMeasurementPanel.h"
#include "gui/PureSignalViewStyle.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QVBoxLayout>
namespace NereusSDR {
namespace {
const std::array<QString, 6> kNames{QStringLiteral("Tone L"), QStringLiteral("Tone U"),
    QStringLiteral("IMD3 L"), QStringLiteral("IMD3 U"), QStringLiteral("IMD5 L"), QStringLiteral("IMD5 U")};
QString level(float value) { return QString::number(value, 'f', 1); }
QString frequency(double hz) { return QString::number(hz / 1e6, 'f', 6); }
// From Thetis display.cs:5652 [v2.10.3.15]: float diff = (f0h_freq - f0l_freq) / 1000f;
constexpr double kHzPerKilohertz = 1000.0;
// From Thetis display.cs:5695 [v2.10.3.15]: diff.ToString("F3") + " kHz"
constexpr int kToneSeparationDecimals = 3;
}
ImdMeasurementPanel::ImdMeasurementPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ImdMeasurementPanel"));
    setFocusPolicy(Qt::StrongFocus);
    QFont panelFont = font();
    panelFont.setPixelSize(12);
    setFont(PureSignalViewStyle::font(panelFont));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);
    root->setAlignment(Qt::AlignTop);
    m_summaryLayout = new QGridLayout;
    m_summaryLayout->setHorizontalSpacing(14);
    root->addLayout(m_summaryLayout);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ImdStatus"));
    m_status->setWordWrap(true);
    m_source = new QLabel(this);
    m_source->setObjectName(QStringLiteral("ImdSource"));
    m_source->setWordWrap(true);
    m_third = new QLabel(this);
    m_third->setObjectName(QStringLiteral("ImdWorstThird"));
    m_fifth = new QLabel(this);
    m_fifth->setObjectName(QStringLiteral("ImdWorstFifth"));
    m_controls = new QWidget(this);
    m_controls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* controls = new QGridLayout(m_controls);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setHorizontalSpacing(6);
    m_enableDuplex = new QPushButton(QStringLiteral("Enable DUP"), m_controls);
    m_enableDuplex->setObjectName(QStringLiteral("ImdEnableDuplex"));
    m_enableDuplex->setAccessibleName(QStringLiteral("Enable display duplex"));
    m_enableDuplex->setToolTip(QStringLiteral("Keep the receiver spectrum visible while transmitting"));
    m_enableDuplex->setAutoDefault(false);
    QSizePolicy duplexPolicy = m_enableDuplex->sizePolicy();
    duplexPolicy.setRetainSizeWhenHidden(true);
    m_enableDuplex->setSizePolicy(duplexPolicy);
    m_guides = new QPushButton(QStringLiteral("Guides"), m_controls);
    m_guides->setObjectName(QStringLiteral("ImdGuides"));
    m_inspector = new QPushButton(QStringLiteral("Inspect"), m_controls);
    m_inspector->setObjectName(QStringLiteral("ImdInspector"));
    m_hideMeasurements = new QPushButton(QStringLiteral("×"), m_controls);
    m_hideMeasurements->setObjectName(QStringLiteral("ImdHideMeasurements"));
    m_hideMeasurements->setAccessibleName(QStringLiteral("Hide two-tone measurements"));
    m_hideMeasurements->setToolTip(QStringLiteral("Hide two-tone measurements in this window"));
    m_hideMeasurements->setAutoDefault(false);
    m_hideMeasurements->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_hideMeasurements->setMinimumWidth(30);
    m_hideMeasurements->setMaximumWidth(34);
    for (QPushButton* button : {m_guides, m_inspector}) {
        button->setCheckable(true);
        button->setAutoDefault(false);
    }
    controls->addWidget(m_enableDuplex, 0, 0);
    controls->addWidget(m_guides, 0, 1);
    controls->addWidget(m_inspector, 0, 2);
    controls->addWidget(m_hideMeasurements, 0, 3);
    connect(m_enableDuplex, &QPushButton::clicked, this, &ImdMeasurementPanel::enableDisplayDuplexRequested);
    connect(m_guides, &QPushButton::toggled, this, &ImdMeasurementPanel::guidesVisibleChanged);
    connect(m_inspector, &QPushButton::toggled, this, &ImdMeasurementPanel::inspectorVisibleChanged);
    connect(m_hideMeasurements, &QPushButton::clicked, this, &ImdMeasurementPanel::hideMeasurementsRequested);
    m_pickerLayout = new QGridLayout;
    m_pickerLayout->setSpacing(4);
    root->addLayout(m_pickerLayout);
    m_details = new QWidget(this);
    auto* details = new QVBoxLayout(m_details);
    details->setContentsMargins(0, 0, 0, 0);
    details->setSpacing(10);
    details->setAlignment(Qt::AlignTop);
    auto* table = new QGridLayout;
    table->setHorizontalSpacing(7);
    table->setVerticalSpacing(7);
    for (int column = 0; column < 4; ++column) {
        const QStringList headers{QStringLiteral("Product"), QStringLiteral("dBm"), QStringLiteral("dBc"), QStringLiteral("MHz")};
        table->addWidget(new QLabel(headers[column], m_details), 0, column);
    }
    for (int index = 0; index < 6; ++index) {
        m_products[index] = new QPushButton(kNames[index], this);
        m_products[index]->setObjectName(QStringLiteral("ImdProduct%1").arg(index));
        m_products[index]->setCheckable(true);
        m_products[index]->setAutoDefault(false);
        connect(m_products[index], &QPushButton::clicked, this, [this, index]() { chooseProduct(static_cast<ImdProduct>(index)); });
        auto* name = new QLabel(kNames[index], m_details);
        name->setStyleSheet(QStringLiteral("color: %1;").arg(PureSignalViewStyle::product(index).name()));
        table->addWidget(name, index + 1, 0);
        for (int column = 0; column < 3; ++column) {
            m_values[index][column] = new QLabel(m_details);
            m_values[index][column]->setObjectName(QStringLiteral("ImdReading%1_%2").arg(index).arg(column));
            m_values[index][column]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->addWidget(m_values[index][column], index + 1, column + 1);
        }
    }
    details->addLayout(table);
    m_selection = new QLabel(this);
    m_selection->setObjectName(QStringLiteral("ImdSelection"));
    m_selection->setWordWrap(true);
    root->addWidget(m_selection);
    root->addWidget(m_details);
    m_reference = new QLabel(m_details);
    m_reference->setWordWrap(true);
    details->addWidget(m_reference);
    m_toneSeparation = new QLabel(m_details);
    m_toneSeparation->setObjectName(QStringLiteral("ImdToneSeparation"));
    details->addWidget(m_toneSeparation);
    m_diagnostics = new QLabel(m_details);
    m_diagnostics->setWordWrap(true);
    details->addWidget(m_diagnostics);
    auto* legend = new QLabel(QStringLiteral("○ Fundamentals   ◇ Third order   □ Fifth order\nHollow / dashed: expected, unmeasured\nKeys 1–6 select the same products as the buttons."), m_details);
    legend->setWordWrap(true);
    details->addWidget(legend);
    setStyleSheet(QStringLiteral("QWidget#ImdMeasurementPanel { background: %1; color: %2; border-bottom: 1px solid %3; } QLabel { color: %2; background: transparent; } QPushButton { color: %2; background: %1; border: 1px solid %3; padding: 5px 3px; } QPushButton:checked, QPushButton#ImdEnableDuplex { border-color: %4; background: %5; }")
        .arg(PureSignalViewStyle::background(this).name(), PureSignalViewStyle::text().name(), PureSignalViewStyle::border().name(),
             WorkspacePreview::roleColour(QColor("#00b4d8"), "sel-border").name(), WorkspacePreview::roleColour(QColor("#203040"), "sel-bg").name()));
    reflow();
    refresh();
}
QString ImdMeasurementPanel::statusText(ImdStatus status)
{
    switch (status) {
    case ImdStatus::Disabled: return QStringLiteral("Measurements disabled");
    case ImdStatus::WaitingForTx: return QStringLiteral("Waiting for two-tone TX");
    case ImdStatus::DuplexRequired: return QStringLiteral("DUP required for receiver spectrum");
    case ImdStatus::WaitingForSpectrum: return QStringLiteral("Waiting for current receiver spectrum");
    case ImdStatus::FundamentalsUnresolved: return QStringLiteral("Fundamentals unresolved");
    case ImdStatus::ProductsOutsideView: return QStringLiteral("Products outside view · widen span");
    case ImdStatus::ProductsMissing: return QStringLiteral("Products missing · levels unmeasured");
    case ImdStatus::Valid: return QStringLiteral("● Valid peaks");
    }
    return {};
}
void ImdMeasurementPanel::setPresentation(Presentation value)
{
    m_presentation = value;
    m_details->setVisible(value != Presentation::Rail);
    m_controls->setVisible(value == Presentation::Rail);
    reflow();
    refresh();
}
void ImdMeasurementPanel::setFrame(const ImdMeasurementFrame& value) { m_frame = value; refresh(); }
void ImdMeasurementPanel::setSelectedProduct(ImdProduct value)
{
    const int index = static_cast<int>(value);
    if (index < 0 || index >= 6) { return; }
    m_selected = value;
    refresh();
}
void ImdMeasurementPanel::chooseProduct(ImdProduct value) { setSelectedProduct(value); emit productSelected(value); }
void ImdMeasurementPanel::setGuidesVisible(bool visible) { const QSignalBlocker blocker(m_guides); m_guides->setChecked(visible); }
void ImdMeasurementPanel::setInspectorVisible(bool visible) { const QSignalBlocker blocker(m_inspector); m_inspector->setChecked(visible); }
void ImdMeasurementPanel::setDisplayDuplexCapability(bool available, const QString& unavailableReason)
{
    if (m_displayDuplexAvailable == available && m_displayDuplexUnavailableReason == unavailableReason) { return; }
    m_displayDuplexAvailable = available;
    m_displayDuplexUnavailableReason = unavailableReason;
    refresh();
}
void ImdMeasurementPanel::refresh()
{
    m_status->setText(statusText(m_frame.status));
    const bool current = m_frame.context.sourceEligible && m_frame.context.frameSequence > 0
        && (m_frame.status == ImdStatus::Valid || m_frame.status == ImdStatus::ProductsMissing
            || m_frame.status == ImdStatus::ProductsOutsideView || m_frame.status == ImdStatus::FundamentalsUnresolved);
    QString sourceText = QStringLiteral("Receiver spectrum");
    QString sourceToolTip = current ? QStringLiteral("Newly accepted current receiver frame")
                                    : QStringLiteral("Requested source; no current measurement");
    QString statusToolTip;
    const bool duplexRequired = m_frame.status == ImdStatus::DuplexRequired;
    if (duplexRequired && m_displayDuplexAvailable) {
        sourceText = QStringLiteral("Enable DUP · keep receiver spectrum visible during TX");
        sourceToolTip = QStringLiteral("Enable display duplex to keep the receiver spectrum visible while transmitting");
    } else if (duplexRequired && !m_displayDuplexUnavailableReason.isEmpty()) {
        sourceText = m_displayDuplexUnavailableReason;
        sourceToolTip = m_displayDuplexUnavailableReason;
        statusToolTip = m_displayDuplexUnavailableReason;
    }
    m_source->setText(sourceText);
    m_source->setToolTip(sourceToolTip);
    m_status->setToolTip(statusToolTip);
    m_enableDuplex->setEnabled(m_displayDuplexAvailable);
    m_enableDuplex->setToolTip(m_displayDuplexAvailable
            ? QStringLiteral("Keep the receiver spectrum visible while transmitting")
            : m_displayDuplexUnavailableReason);
    m_enableDuplex->setVisible(m_presentation == Presentation::Rail && duplexRequired && m_displayDuplexAvailable);
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(WorkspacePreview::statusColour(QColor("#9db6cd"), m_frame.status == ImdStatus::Valid ? "ok" : "warn").name()));
    const QString summaryTemplate = QStringLiteral("<span style='font-size:11px;color:%1'>%2</span><br><span style='font-size:22px;font-weight:600'>%3</span> <span style='font-size:11px'>dBc</span>");
    m_third->setText(summaryTemplate.arg(PureSignalViewStyle::muted().name(), QStringLiteral("WORST IMD3"), m_frame.summary ? level(-m_frame.summary->worstImd3DBc) : QStringLiteral("–")));
    m_fifth->setText(summaryTemplate.arg(PureSignalViewStyle::muted().name(), QStringLiteral("WORST IMD5"), m_frame.summary ? level(-m_frame.summary->worstImd5DBc) : QStringLiteral("–")));
    for (int index = 0; index < 6; ++index) {
        const QSignalBlocker blocker(m_products[index]);
        m_products[index]->setChecked(index == static_cast<int>(m_selected));
        const auto& peak = m_frame.peaks[index];
        m_values[index][0]->setText(peak ? level(peak->absoluteDbm) : QStringLiteral("–"));
        m_values[index][1]->setText(peak && peak->relativeDbc ? level(*peak->relativeDbc) : QStringLiteral("–"));
        m_values[index][2]->setText(peak ? frequency(peak->frequencyHz) : QStringLiteral("–"));
    }
    const int selected = static_cast<int>(m_selected);
    const auto& peak = m_frame.peaks[selected];
    const auto& expected = m_frame.expectedFrequencyHz[selected];
    QString selection = kNames[selected] + QStringLiteral(" · ");
    if (peak) {
        selection += frequency(peak->frequencyHz) + QStringLiteral(" MHz · ") + level(peak->absoluteDbm) + QStringLiteral(" dBm · ")
            + (peak->relativeDbc ? level(*peak->relativeDbc) : QStringLiteral("–")) + QStringLiteral(" dBc\nMeasured peak");
    } else if (expected) {
        selection += frequency(*expected) + QStringLiteral(" MHz\nExpected position · unmeasured (– dBm · – dBc)");
    } else { selection += QStringLiteral("– MHz · – dBm · – dBc\nNo reliable position"); }
    m_selection->setText(selection);
    m_reference->setText(QStringLiteral("Reference\nPer-product dBc: higher fundamental\nWorst IMD: lower fundamental · single tone"));
    // Measured fundamentals only; an expected position never stands in for a peak.
    const auto& lowerTone = m_frame.peaks[static_cast<int>(ImdProduct::FundamentalLower)];
    const auto& upperTone = m_frame.peaks[static_cast<int>(ImdProduct::FundamentalUpper)];
    QString separation = QStringLiteral("–");
    if (lowerTone && upperTone) {
        // Deliberate: shown whenever both tones are measured, while Thetis waits for all six products (display.cs:5560-5562 ok); the inspector shows each measured reading.
        // From Thetis display.cs:5569-5575 [v2.10.3.15]: a whole-Hz low edge,
        // (long)(m_dCentreFreqRX1 * 1e6) + left_edge, plus each tone's offset
        // truncated by (long)(f_X * hz_per_pixel).
        const qint64 edgeHz = static_cast<qint64>(m_frame.context.firstSampleHz);
        const qint64 lowerHz = edgeHz + static_cast<qint64>(lowerTone->sampleIndex * m_frame.context.sampleStepHz);
        const qint64 upperHz = edgeHz + static_cast<qint64>(upperTone->sampleIndex * m_frame.context.sampleStepHz);
        // From Thetis display.cs:5652 [v2.10.3.15]
        const double separationKHz = static_cast<double>(upperHz - lowerHz) / kHzPerKilohertz;
        separation = QString::number(separationKHz, 'f', kToneSeparationDecimals);
    }
    m_toneSeparation->setText(QStringLiteral("Tone separation  %1 kHz").arg(separation));
    QString diagnostics = QStringLiteral("Derived diagnostics · EMA\nOIP3: – dB    OIP5: – dB");
    if (m_frame.summary) {
        diagnostics = QStringLiteral("Derived diagnostics · EMA\nOIP3: %1 dB    OIP5: %2 dB").arg(level(m_frame.summary->oip3), level(m_frame.summary->oip5));
    }
    m_diagnostics->setText(diagnostics);
    m_details->setVisible(m_presentation != Presentation::Rail);
}
void ImdMeasurementPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::NoModifier && event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6) {
        chooseProduct(static_cast<ImdProduct>(event->key() - Qt::Key_1));
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
void ImdMeasurementPanel::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); reflow(); }
void ImdMeasurementPanel::reflow()
{
    const bool narrow = width() < 520 || m_presentation == Presentation::Inspector;
    m_summaryLayout->addWidget(m_status, 0, 0, 1, narrow ? 2 : 1);
    m_summaryLayout->addWidget(m_source, 1, 0, 1, narrow ? 2 : 1);
    m_summaryLayout->addWidget(m_third, narrow ? 2 : 0, narrow ? 0 : 1, narrow ? 1 : 2, 1);
    m_summaryLayout->addWidget(m_fifth, narrow ? 2 : 0, narrow ? 1 : 2, narrow ? 1 : 2, 1);
    m_summaryLayout->addWidget(m_controls, narrow ? 3 : 0, narrow ? 0 : 3, narrow ? 1 : 2, narrow ? 2 : 1,
        Qt::AlignTop | Qt::AlignRight);
    for (int index = 0; index < 6; ++index) {
        m_pickerLayout->addWidget(m_products[index], narrow ? index / 3 : 0, narrow ? index % 3 : index);
    }
}
} // namespace NereusSDR
