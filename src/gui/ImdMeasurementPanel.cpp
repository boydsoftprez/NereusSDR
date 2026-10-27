// no-port-check: NereusSDR-original cached GUI measurement presentation; no analyzer.
// Modification history (NereusSDR):
//   2026-10-13: J.J. Boyd (KG4VCF), OpenAI Codex; rail and optional bench inspector.
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
