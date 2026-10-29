// no-port-check: NereusSDR-original editor for existing native action visibility.
// Modification history (NereusSDR):
//   2026-10-26 - A row whose button's feature is not built is shown
//                 disabled with the reason (disabled, never hidden).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Lists every button NereusSDR offers under the Thetis
//                 options-grid headings, and a Reset layout button.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Visibility by Thetis button number. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Keep the full replacement warning visible in the local form by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Explicit individual control selection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "OtherButtonItemEditor.h"
#include "../ContainerControlCatalog.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/meters/OtherButtonCatalog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QRegularExpression>

namespace NereusSDR {
namespace {

// Ruling D2: the Thetis "VFOs" heading reads "Tuning".
QString sectionTitle(const QString& thetisSection)
{
    if (thetisSection == QLatin1String("VFOs")) { return QStringLiteral("Tuning"); }
    return thetisSection;
}

// Ruling D2 and R-R3-49: a Thetis caption or tooltip in NereusSDR's words.
// VFO A / B (VFOa, VFOb) and RX1 / RX2 name slices A and B; VAC is VAX.
QString operatorWords(QString text)
{
    static const QRegularExpression vfo(QStringLiteral("\\bVFO\\s*([ab])\\b"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx(QStringLiteral("\\bRX\\s*([12])\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression vac(QStringLiteral("\\bVAC\\s*([12])\\b"),
                                        QRegularExpression::CaseInsensitiveOption);
    const auto replaceAll = [&text](const QRegularExpression& pattern, const auto& wordFor) {
        QRegularExpressionMatch match = pattern.match(text);
        while (match.hasMatch()) {
            QString word = wordFor(match.captured(1));
            if (match.capturedStart() == 0) { word[0] = word[0].toUpper(); }
            text.replace(match.capturedStart(), match.capturedLength(), word);
            match = pattern.match(text, match.capturedStart() + word.size());
        }
    };
    replaceAll(vfo, [](const QString& letter) { return QStringLiteral("slice ") + letter.toUpper(); });
    replaceAll(rx, [](const QString& number) {
        return QStringLiteral("slice ") + (number == QLatin1String("1") ? QStringLiteral("A") : QStringLiteral("B"));
    });
    replaceAll(vac, [](const QString& number) { return QStringLiteral("VAX ") + number; });
    return text;
}

} // namespace

QVector<OtherButtonItemEditor::VisibilityRow> OtherButtonItemEditor::visibilityRows(
    const std::function<bool(OtherButtonItem::ButtonId)>& offered)
{
    // Reference: Thetis ucOtherButtonsOptionsGrid.cs:529-755 [v2.10.3.15]
    // (CheckBoxData) lists the buttons in id order under its section rows.
    QVector<VisibilityRow> rows;
    for (int value = 0; value < OtherButtonItem::kSlotCount; ++value) {
        const auto id = static_cast<OtherButtonItem::ButtonId>(value);
        if (!OtherButtonCatalog::isDefined(id) || !offered(id)) { continue; }
        const OtherButtonInfo info = OtherButtonCatalog::info(id);
        rows.append({id, sectionTitle(info.section), operatorWords(info.caption),
                     operatorWords(info.tooltip)});
    }
    return rows;
}

OtherButtonItemEditor::OtherButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildOtherSpecific();
    buildButtonBoxSection();
}
void OtherButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    refreshSelection();
}
void OtherButtonItemEditor::refreshSelection()
{
    auto* other = qobject_cast<OtherButtonItem*>(m_item);
    if (!other) { return; }
    beginProgrammaticUpdate();
    int selected = -1;
    const bool single = isSingleContainerControl(other);
    for (const auto& control : supportedContainerControls()) {
        if (single && other->isButtonShown(control.buttonId)) { selected = int(control.buttonId); }
    }
    for (const auto& [id, check] : std::as_const(m_visibility)) {
        check->setChecked(other->isButtonVisible(id));
    }
    m_singleControl->setCurrentIndex(m_singleControl->findData(selected));
    other->setProperty("containerSingleControl", single);
    endProgrammaticUpdate();
}
void OtherButtonItemEditor::buildOtherSpecific()
{
    addHeader(tr("Control selection"));
    m_singleControl = new QComboBox(this);
    m_singleControl->setObjectName(QStringLiteral("otherSingleControl"));
    m_singleControl->addItem(tr("Group: edit visibility below"), -1);
    const auto controls = supportedContainerControls();
    for (const auto& control : controls) { m_singleControl->addItem(control.title, int(control.buttonId)); }
    addRow(tr("Single control"), m_singleControl);
    auto* explanation = new QLabel(tr("Choosing a single control replaces this group's selection. Add separate controls to arrange them independently."), this);
    explanation->setWordWrap(true);
    QSizePolicy paragraphPolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    paragraphPolicy.setHeightForWidth(true);
    explanation->setSizePolicy(paragraphPolicy);
    // A spanning row lets the form use the paragraph's height for its actual
    // width instead of constraining it to a single field-height row.
    m_form->addRow(explanation);
    connect(m_singleControl, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        if (isProgrammaticUpdate()) { return; }
        auto* other = qobject_cast<OtherButtonItem*>(m_item);
        const int id = m_singleControl->currentData().toInt();
        if (!other || id < 0) { return; }
        other->setOnlyVisible(static_cast<OtherButtonItem::ButtonId>(id));
        other->setColumns(1);
        // Update common Columns and the visibility rows without dispatching.
        setItem(other);
        notifyChanged();
    });

    // Reference: Thetis btnOtherButtons_reset_layout (setup.cs:35082-35086
    // [v2.10.3.15]) puts the buttons back in their first order.
    m_resetLayout = new QPushButton(tr("Reset layout"), this);
    m_resetLayout->setObjectName(QStringLiteral("otherResetLayout"));
    m_resetLayout->setToolTip(tr("Put the buttons back in their first order. To move a button, hold "
                                 "Alt (Option on a Mac) and drag it onto another; hold Shift as "
                                 "well when you let go to swap the two."));
    addRow(tr("Button order"), m_resetLayout);
    connect(m_resetLayout, &QPushButton::clicked, this, [this] {
        auto* other = qobject_cast<OtherButtonItem*>(m_item);
        if (!other) { return; }
        other->resetButtonMap();
        notifyChanged();
    });

    addHeader(tr("Visible controls"));
    // Which ids are offered does not depend on the item.
    const OtherButtonItem probe;
    const auto rows = visibilityRows([&probe](OtherButtonItem::ButtonId id) { return probe.isOffered(id); });
    QString section;
    for (const VisibilityRow& row : rows) {
        if (row.section != section) {
            section = row.section;
            addHeader(section);
        }
        auto* check = makeCheckRow(row.caption);
        check->setObjectName(QStringLiteral("otherVisible_%1").arg(int(row.id)));
        // A button whose feature is not built is never drawn: its row is
        // shown disabled with the reason (disabled, never hidden).
        const auto feature = OtherButtonItem::unbuiltFeatureFor(row.id);
        const bool built = !feature || UnbuiltFeatures::isBuilt(*feature);
        const QString tooltip = built ? row.tooltip
            : row.tooltip + QLatin1Char('\n') + UnbuiltFeatures::notBuiltReason();
        check->setEnabled(built);
        check->setToolTip(tooltip);
        if (auto* label = qobject_cast<QLabel*>(m_form->labelForField(check))) {
            // Captions such as "A <> B" are words, never markup.
            label->setTextFormat(Qt::PlainText);
            label->setToolTip(tooltip);
            label->setEnabled(built);
        }
        m_visibility.append({row.id, check});
        connect(check, &QCheckBox::toggled, this, [this, id = row.id](bool visible) {
            if (isProgrammaticUpdate()) { return; }
            auto* other = qobject_cast<OtherButtonItem*>(m_item);
            if (!other) { return; }
            other->setButtonVisible(id, visible);
            refreshSelection();
            notifyChanged();
        });
    }
}
} // namespace NereusSDR
