#include "ClickBoxItemEditor.h"
#include "../../meters/ClickBoxItem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

namespace NereusSDR {

ClickBoxItemEditor::ClickBoxItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ClickBoxItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    // The base class populates position, binding, z-order, onlyWhenRx/Tx,
    // and displayGroup; the rows here are the click box's buttons and action.
    auto* box = qobject_cast<ClickBoxItem*>(item);
    if (!box) { return; }

    beginProgrammaticUpdate();
    const Qt::MouseButtons mask = box->buttonMask();
    m_chkLeft->setChecked(mask.testFlag(Qt::LeftButton));
    m_chkRight->setChecked(mask.testFlag(Qt::RightButton));
    m_chkMiddle->setChecked(mask.testFlag(Qt::MiddleButton));
    const int actionIdx = m_comboAction->findData(static_cast<int>(box->action()));
    m_comboAction->setCurrentIndex(actionIdx >= 0 ? actionIdx : 0);
    m_spinGoToGroup->setValue(box->goToGroup());
    updateGoToGroupEnabled();
    endProgrammaticUpdate();
}

void ClickBoxItemEditor::applyButtonMask()
{
    if (isProgrammaticUpdate()) { return; }
    auto* box = qobject_cast<ClickBoxItem*>(m_item);
    if (!box) { return; }
    Qt::MouseButtons mask;
    if (m_chkLeft->isChecked()) { mask |= Qt::LeftButton; }
    if (m_chkRight->isChecked()) { mask |= Qt::RightButton; }
    if (m_chkMiddle->isChecked()) { mask |= Qt::MiddleButton; }
    box->setButtonMask(mask);
    notifyChanged();
}

void ClickBoxItemEditor::updateGoToGroupEnabled()
{
    // Disabled, never hidden, while another action is chosen.
    const auto action = static_cast<ClickBoxItem::Action>(m_comboAction->currentData().toInt());
    m_spinGoToGroup->setEnabled(action == ClickBoxItem::Action::GoToGroup);
}

void ClickBoxItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Click Box"));

    m_chkLeft = makeCheckRow(QStringLiteral("Left button"));
    m_chkLeft->setObjectName(QStringLiteral("clickBoxLeftButton"));
    m_chkRight = makeCheckRow(QStringLiteral("Right button"));
    m_chkRight->setObjectName(QStringLiteral("clickBoxRightButton"));
    m_chkMiddle = makeCheckRow(QStringLiteral("Middle button"));
    m_chkMiddle->setObjectName(QStringLiteral("clickBoxMiddleButton"));
    for (QCheckBox* check : {m_chkLeft, m_chkRight, m_chkMiddle}) {
        connect(check, &QCheckBox::toggled, this, [this](bool) { applyButtonMask(); });
    }

    m_comboAction = new QComboBox(this);
    m_comboAction->setObjectName(QStringLiteral("clickBoxAction"));
    m_comboAction->addItem(QStringLiteral("Go to group"),
                           static_cast<int>(ClickBoxItem::Action::GoToGroup));
    m_comboAction->addItem(QStringLiteral("Next or previous group"),
                           static_cast<int>(ClickBoxItem::Action::ChangeGroup));
    m_comboAction->addItem(QStringLiteral("Cycle units"),
                           static_cast<int>(ClickBoxItem::Action::CycleUnits));
    m_comboAction->setToolTip(QStringLiteral(
        "Next or previous group and Cycle units: a left click steps forward, "
        "a right click steps back. Cycle units changes the bar or signal text under the box."));
    addRow(QStringLiteral("Action"), m_comboAction);
    connect(m_comboAction, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        updateGoToGroupEnabled();
        if (isProgrammaticUpdate()) { return; }
        auto* box = qobject_cast<ClickBoxItem*>(m_item);
        if (!box) { return; }
        box->setAction(static_cast<ClickBoxItem::Action>(m_comboAction->currentData().toInt()));
        notifyChanged();
    });

    m_spinGoToGroup = makeIntRow(QStringLiteral("Go to group"), 0, ClickBoxItem::kMaxDisplayGroup);
    m_spinGoToGroup->setObjectName(QStringLiteral("clickBoxGoToGroup"));
    m_spinGoToGroup->setToolTip(QStringLiteral("The display group a click shows; 0 shows every group."));
    connect(m_spinGoToGroup, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (isProgrammaticUpdate()) { return; }
        auto* box = qobject_cast<ClickBoxItem*>(m_item);
        if (!box) { return; }
        box->setGoToGroup(value);
        notifyChanged();
    });
    updateGoToGroupEnabled();
}

} // namespace NereusSDR
