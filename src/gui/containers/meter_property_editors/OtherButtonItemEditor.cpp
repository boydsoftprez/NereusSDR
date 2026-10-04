// no-port-check: NereusSDR-original editor for existing native action visibility.
// Modification history (NereusSDR):
//   2026-10-03 — Explicit individual control selection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "OtherButtonItemEditor.h"
#include "../ContainerControlCatalog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

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
    for (int i = 0; i < controls.size(); ++i) {
        const uint32_t bit = 1u << int(controls[i].buttonId);
        m_visibility[i]->setChecked(other->visibleBits() & bit);
        if (other->columns() == 1 && other->visibleBits() == bit) { selected = int(controls[i].buttonId); }
    }
    m_singleControl->setCurrentIndex(m_singleControl->findData(selected));
    other->setProperty("containerSingleControl", isSingleContainerControl(other));
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
    addRow(QString(), explanation);
    connect(m_singleControl, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        if (isProgrammaticUpdate()) { return; }
        auto* other = qobject_cast<OtherButtonItem*>(m_item);
        const int id = m_singleControl->currentData().toInt();
        if (!other || id < 0) { return; }
        other->setVisibleBits(1u << id);
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
            const uint32_t bit = 1u << int(id);
            other->setVisibleBits(visible ? other->visibleBits() | bit : other->visibleBits() & ~bit);
            refreshSelection();
            notifyChanged();
        });
    }
}
} // namespace NereusSDR
