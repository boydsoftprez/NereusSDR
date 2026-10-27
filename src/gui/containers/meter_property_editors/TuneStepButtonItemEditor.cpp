#include "TuneStepButtonItemEditor.h"
#include "../../meters/TuneStepButtonItem.h"
#include "models/SliceModel.h"

#include <QCheckBox>

namespace NereusSDR {

TuneStepButtonItemEditor::TuneStepButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildTuneStepSpecific();
}

void TuneStepButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    TuneStepButtonItem* t = qobject_cast<TuneStepButtonItem*>(item);
    if (!t) { return; }
    beginProgrammaticUpdate();
    for (int i = 0; i < m_visible.size(); ++i) {
        m_visible[i]->setChecked(((t->visibleBits() >> i) & 1u) != 0);
    }
    endProgrammaticUpdate();
}

void TuneStepButtonItemEditor::buildTuneStepSpecific()
{
    // One row per step, named as the step list names it (1Hz .. 10MHz).
    addHeader(tr("Visible steps"));
    for (int i = 0; i < TuneStepButtonItem::kStepCount; ++i) {
        auto* check = makeCheckRow(QString::fromLatin1(kTuneStepList[i].name));
        check->setObjectName(QStringLiteral("stepVisible_%1").arg(i));
        m_visible.append(check);
        connect(check, &QCheckBox::toggled, this, [this, i](bool visible) {
            if (isProgrammaticUpdate()) { return; }
            auto* t = qobject_cast<TuneStepButtonItem*>(m_item);
            if (!t) { return; }
            const uint32_t bit = 1u << i;
            t->setVisibleBits(visible ? (t->visibleBits() | bit) : (t->visibleBits() & ~bit));
            notifyChanged();
        });
    }
}

} // namespace NereusSDR
