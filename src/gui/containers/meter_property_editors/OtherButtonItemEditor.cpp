// no-port-check: NereusSDR-original editor for existing native action visibility.
// Modification history (NereusSDR):
//   2026-10-25 - Visibility by Thetis button number. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Keep the full replacement warning visible in the local form by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Explicit individual control selection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "OtherButtonItemEditor.h"
#include "../ContainerControlCatalog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFormLayout>

namespace NereusSDR {
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
    const auto controls = supportedContainerControls();
    const bool single = isSingleContainerControl(other);
    for (int i = 0; i < controls.size(); ++i) {
        m_visibility[i]->setChecked(other->isButtonVisible(controls[i].buttonId));
        if (single && other->isButtonShown(controls[i].buttonId)) { selected = int(controls[i].buttonId); }
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
    m_singleControl->addItem(tr("Group — edit visibility below"), -1);
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
    addHeader(tr("Visible controls"));
    for (const auto& control : controls) {
        auto* check = makeCheckRow(control.title);
        check->setObjectName(QStringLiteral("otherVisible_%1").arg(int(control.buttonId)));
        m_visibility.append(check);
        connect(check, &QCheckBox::toggled, this, [this, id = control.buttonId](bool visible) {
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
