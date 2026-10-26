// no-port-check: NereusSDR-original tests for the container click box's
// actions (container item parity, Task 47).
//
// The behaviour under test is ported from Thetis clsClickBox and
// clsMeter.MouseUp (MeterManager.cs:7597-7652, 29814-29848, 31165-31186
// [v2.10.3.15]) and the bar and signal text HandleIncrement /
// HandleDecrement (MeterManager.cs:21539, 21836): the button mask, going to
// a display group, stepping the group and stepping an item's units.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Created (container item parity, Task
//                                    47). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QCheckBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/ClickBoxItemEditor.h"
#include "gui/meters/ClickBoxItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/SignalTextItem.h"

using namespace NereusSDR;

namespace {

constexpr int kWidth = 200;
constexpr int kHeight = 100;
const QPointF kCentre(kWidth / 2.0, kHeight / 2.0);

void press(MeterWidget& widget, Qt::MouseButton button)
{
    QMouseEvent event(QEvent::MouseButtonPress, kCentre, widget.mapToGlobal(kCentre), button,
                      button, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &event);
}

bool pressItem(ClickBoxItem& box, Qt::MouseButton button)
{
    QMouseEvent event(QEvent::MouseButtonPress, kCentre, kCentre, button, button, Qt::NoModifier);
    return box.handleMousePress(&event, kWidth, kHeight);
}

// A widget with items in groups 1 and 2 and a click box over all of it.
struct GroupedWidget {
    MeterWidget widget;
    ClickBoxItem* box{nullptr};
    TextItem* groupText{nullptr};

    GroupedWidget()
    {
        widget.resize(kWidth, kHeight);
        for (int group : {1, 2}) {
            auto* item = new TextItem();
            item->setRect(0.0f, 0.0f, 0.5f, 0.5f);
            item->setDisplayGroup(group);
            widget.addItem(item);
        }
        groupText = new TextItem();
        groupText->setLabel(TextItem::groupToken());
        widget.addItem(groupText);
        box = new ClickBoxItem();
        box->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(box);
    }
};

} // namespace

class TstClickBoxItem : public QObject {
    Q_OBJECT

private slots:
    void newBoxHasThetisDefaults()
    {
        ClickBoxItem box;
        QCOMPARE(box.buttonMask(), kClickBoxThetisDefaults.buttonMask);
        QCOMPARE(box.buttonMask(), Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
        QCOMPARE(box.action(), ClickBoxItem::Action::ChangeGroup);
        QCOMPARE(kClickBoxThetisDefaults.action, ClickBoxItem::Action::ChangeGroup);
        QCOMPARE(box.goToGroup(), 0);
    }

    void rightClickWithRightMaskedOffDoesNothing()
    {
        GroupedWidget g;
        g.box->setButtonMask(Qt::LeftButton | Qt::MiddleButton);
        QSignalSpy spy(g.box, &ClickBoxItem::actionRequested);
        g.widget.setDisplayGroup(1);
        press(g.widget, Qt::RightButton);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(g.widget.displayGroup(), 1);
        // The left button still acts.
        press(g.widget, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(g.widget.displayGroup(), 2);
    }

    void changeGroupStepsAndWrapsAtTheHighestGroupInUse()
    {
        GroupedWidget g;
        QCOMPARE(g.widget.displayGroup(), 0);
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.widget.displayGroup(), 1);
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.widget.displayGroup(), 2);
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.widget.displayGroup(), 0);
        press(g.widget, Qt::RightButton);
        QCOMPARE(g.widget.displayGroup(), 2);
        press(g.widget, Qt::RightButton);
        QCOMPARE(g.widget.displayGroup(), 1);
        // Thetis steps on left and right only: a middle click changes nothing.
        press(g.widget, Qt::MiddleButton);
        QCOMPARE(g.widget.displayGroup(), 1);
    }

    void changeGroupWithNoGroupsStaysOnAll()
    {
        MeterWidget widget;
        widget.resize(kWidth, kHeight);
        auto* box = new ClickBoxItem();
        box->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(box);
        press(widget, Qt::LeftButton);
        QCOMPARE(widget.displayGroup(), 0);
        press(widget, Qt::RightButton);
        QCOMPARE(widget.displayGroup(), 0);
    }

    void goToGroupAnswersEveryMaskedButton()
    {
        GroupedWidget g;
        g.box->setAction(ClickBoxItem::Action::GoToGroup);
        g.box->setGoToGroup(2);
        press(g.widget, Qt::MiddleButton);
        QCOMPARE(g.widget.displayGroup(), 2);
        g.box->setGoToGroup(0);
        press(g.widget, Qt::RightButton);
        QCOMPARE(g.widget.displayGroup(), 0);
        // Thetis clamps to the container's groups.
        g.box->setGoToGroup(9);
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.widget.displayGroup(), 2);
    }

    void groupTextShowsTheShownGroup()
    {
        GroupedWidget g;
        QCOMPARE(g.groupText->displayText(), QStringLiteral("0"));
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.groupText->displayText(), QStringLiteral("1"));
        press(g.widget, Qt::LeftButton);
        QCOMPARE(g.groupText->displayText(), QStringLiteral("2"));

        // A container with no groups shows nothing, as Thetis does.
        MeterWidget plain;
        auto* text = new TextItem();
        text->setLabel(TextItem::groupToken());
        plain.addItem(text);
        QCOMPARE(text->displayText(), QString());
    }

    void cycleUnitsStepsTheSignalTextDbmSUv()
    {
        MeterWidget widget;
        widget.resize(kWidth, kHeight);
        auto* text = new SignalTextItem();
        text->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(text);
        auto* box = new ClickBoxItem();
        box->setRect(0.25f, 0.25f, 0.5f, 0.5f);
        box->setAction(ClickBoxItem::Action::CycleUnits);
        widget.addItem(box);
        QCOMPARE(text->units(), SignalTextItem::Units::Dbm);

        press(widget, Qt::LeftButton);
        QCOMPARE(text->units(), SignalTextItem::Units::SUnits);
        QCOMPARE(text->unitMode(), MeterItem::MeterUnit::S);
        press(widget, Qt::LeftButton);
        QCOMPARE(text->units(), SignalTextItem::Units::Uv);
        QCOMPARE(text->unitMode(), MeterItem::MeterUnit::uV);
        press(widget, Qt::LeftButton);
        QCOMPARE(text->units(), SignalTextItem::Units::Dbm);
        QCOMPARE(text->unitMode(), MeterItem::MeterUnit::dBm);
        // Right steps back, wrapping from dBm to uV.
        press(widget, Qt::RightButton);
        QCOMPARE(text->units(), SignalTextItem::Units::Uv);
        // The display group is untouched.
        QCOMPARE(widget.displayGroup(), 0);
    }

    void cycleUnitsStartsFromASavedSignalTextsUnits()
    {
        MeterWidget widget;
        widget.resize(kWidth, kHeight);
        auto* text = new SignalTextItem();
        text->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(text);
        // A saved record (or the item editor) sets the units alone.
        text->setUnits(SignalTextItem::Units::SUnits);
        auto* box = new ClickBoxItem();
        box->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        box->setAction(ClickBoxItem::Action::CycleUnits);
        widget.addItem(box);
        press(widget, Qt::LeftButton);
        QCOMPARE(text->units(), SignalTextItem::Units::Uv);
    }

    void cycleUnitsChangesOnlyTheTopmostItemWithUnits()
    {
        MeterWidget widget;
        widget.resize(kWidth, kHeight);
        auto* lower = new SignalTextItem();
        lower->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(lower);
        auto* upper = new SignalTextItem();
        upper->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(upper);
        // A text item has no units and is passed over.
        auto* label = new TextItem();
        label->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(label);
        // Not under the box.
        auto* aside = new SignalTextItem();
        aside->setRect(0.0f, 0.0f, 0.1f, 0.1f);
        auto* box = new ClickBoxItem();
        box->setRect(0.5f, 0.5f, 0.5f, 0.5f);
        box->setAction(ClickBoxItem::Action::CycleUnits);
        widget.addItem(box);
        widget.addItem(aside);

        press(widget, Qt::LeftButton);
        QCOMPARE(upper->units(), SignalTextItem::Units::SUnits);
        QCOMPARE(lower->units(), SignalTextItem::Units::Dbm);
        QCOMPARE(aside->units(), SignalTextItem::Units::Dbm);
        QCOMPARE(label->unitMode(), MeterItem::MeterUnit::dBm);
    }

    void cycleUnitsStepsABar()
    {
        MeterWidget widget;
        widget.resize(kWidth, kHeight);
        auto* bar = new BarItem();
        bar->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        widget.addItem(bar);
        auto* box = new ClickBoxItem();
        box->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        box->setAction(ClickBoxItem::Action::CycleUnits);
        widget.addItem(box);
        QCOMPARE(bar->unitMode(), MeterItem::MeterUnit::dBm);
        press(widget, Qt::LeftButton);
        QCOMPARE(bar->unitMode(), MeterItem::MeterUnit::S);
        press(widget, Qt::RightButton);
        QCOMPARE(bar->unitMode(), MeterItem::MeterUnit::dBm);
    }

    void steppedUnitFollowsThetisOrder()
    {
        using U = MeterItem::MeterUnit;
        QCOMPARE(ClickBoxItem::steppedUnit(U::dBm, 1, false), U::S);
        QCOMPARE(ClickBoxItem::steppedUnit(U::S, 1, false), U::uV);
        QCOMPARE(ClickBoxItem::steppedUnit(U::uV, 1, false), U::dBm);
        QCOMPARE(ClickBoxItem::steppedUnit(U::dBm, -1, false), U::uV);
        QCOMPARE(ClickBoxItem::steppedUnit(U::uV, -1, false), U::S);
        // A PBSNR reading has no uV.
        QCOMPARE(ClickBoxItem::steppedUnit(U::S, 1, true), U::dBm);
        QCOMPARE(ClickBoxItem::steppedUnit(U::dBm, -1, true), U::S);
    }

    void aRemovedBoxNoLongerActsOnItsWidget()
    {
        GroupedWidget g;
        g.widget.removeItem(g.box);
        QVERIFY(pressItem(*g.box, Qt::LeftButton));
        QCOMPARE(g.widget.displayGroup(), 0);
    }

    void previewCopiesNeverChangeTheLiveWidget_data()
    {
        QTest::addColumn<int>("action");
        QTest::newRow("go-to-group") << static_cast<int>(ClickBoxItem::Action::GoToGroup);
        QTest::newRow("change-group") << static_cast<int>(ClickBoxItem::Action::ChangeGroup);
        QTest::newRow("cycle-units") << static_cast<int>(ClickBoxItem::Action::CycleUnits);
    }

    void previewCopiesNeverChangeTheLiveWidget()
    {
        QFETCH(int, action);
        GroupedWidget live;
        auto* liveText = new SignalTextItem();
        liveText->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        live.widget.addItem(liveText);
        live.widget.removeItem(live.box);
        live.widget.addItem(live.box);  // topmost again
        live.box->setAction(static_cast<ClickBoxItem::Action>(action));
        live.box->setGoToGroup(2);

        ContainerContentRegistry registry;
        const ContentEntry entry = registry.captureMeterItem(*live.box);
        QWidget root;
        auto* preview = qobject_cast<MeterWidget*>(registry.createPreview(entry, &root));
        QVERIFY(preview);
        QVERIFY(preview->testAttribute(Qt::WA_TransparentForMouseEvents));
        preview->resize(kWidth, kHeight);
        auto* copy = qobject_cast<ClickBoxItem*>(preview->items().value(0));
        QVERIFY(copy);
        QVERIFY(copy != live.box);
        QCOMPARE(static_cast<int>(copy->action()), action);
        QCOMPARE(copy->goToGroup(), 2);

        for (Qt::MouseButton button : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
            pressItem(*copy, button);
            press(*preview, button);
        }
        QCOMPARE(live.widget.displayGroup(), 0);
        QCOMPARE(liveText->units(), SignalTextItem::Units::Dbm);
        QCOMPARE(liveText->unitMode(), MeterItem::MeterUnit::dBm);
        QCOMPARE(live.groupText->displayText(), QStringLiteral("0"));
    }

    void settingsRoundTripAndOlderRecordsLoad()
    {
        ClickBoxItem box;
        box.setRect(0.1f, 0.2f, 0.3f, 0.4f);
        box.setButtonMask(Qt::LeftButton | Qt::MiddleButton);
        box.setAction(ClickBoxItem::Action::GoToGroup);
        box.setGoToGroup(3);
        ClickBoxItem copy;
        QVERIFY(copy.deserialize(box.serialize()));
        QCOMPARE(copy.buttonMask(), Qt::LeftButton | Qt::MiddleButton);
        QCOMPARE(copy.action(), ClickBoxItem::Action::GoToGroup);
        QCOMPARE(copy.goToGroup(), 3);
        QCOMPARE(copy.serialize(), box.serialize());

        // Saved before the action fields: Thetis's defaults.
        ClickBoxItem older;
        older.setAction(ClickBoxItem::Action::CycleUnits);
        QVERIFY(older.deserialize(QStringLiteral("CLICKBOX|0.1|0.2|0.3|0.4|-1|5")));
        QCOMPARE(older.buttonMask(), kClickBoxThetisDefaults.buttonMask);
        QCOMPARE(older.action(), ClickBoxItem::Action::ChangeGroup);
        QCOMPARE(older.goToGroup(), 0);
        QCOMPARE(older.zOrder(), 5);

        // Out of range fields are refused.
        ClickBoxItem bad;
        QVERIFY(!bad.deserialize(QStringLiteral("CLICKBOX|0|0|1|1|-1|0|1|7|0")));
        QVERIFY(!bad.deserialize(QStringLiteral("CLICKBOX|0|0|1|1|-1|0|256|1|0")));
        QVERIFY(!bad.deserialize(QStringLiteral("CLICKBOX|0|0|1|1|-1|0|1|1|16")));
    }

    void editorSetsTheMaskActionAndGroup()
    {
        ClickBoxItem box;
        ClickBoxItemEditor editor;
        editor.setItem(&box);
        auto* right = editor.findChild<QCheckBox*>(QStringLiteral("clickBoxRightButton"));
        auto* action = editor.findChild<QComboBox*>(QStringLiteral("clickBoxAction"));
        auto* group = editor.findChild<QSpinBox*>(QStringLiteral("clickBoxGoToGroup"));
        QVERIFY(right && action && group);
        QVERIFY(right->isChecked());
        QCOMPARE(action->currentData().toInt(), static_cast<int>(ClickBoxItem::Action::ChangeGroup));
        QVERIFY(!group->isEnabled());

        right->setChecked(false);
        QCOMPARE(box.buttonMask(), Qt::LeftButton | Qt::MiddleButton);
        action->setCurrentIndex(action->findData(static_cast<int>(ClickBoxItem::Action::GoToGroup)));
        QCOMPARE(box.action(), ClickBoxItem::Action::GoToGroup);
        QVERIFY(group->isEnabled());
        group->setValue(4);
        QCOMPARE(box.goToGroup(), 4);
        action->setCurrentIndex(action->findData(static_cast<int>(ClickBoxItem::Action::CycleUnits)));
        QCOMPARE(box.action(), ClickBoxItem::Action::CycleUnits);
        QVERIFY(!group->isEnabled());
    }

    void clickBoxIsBuilt()
    {
        QVERIFY(UnbuiltFeatures::isBuilt(UnbuiltFeature::ContainerClickBox));
        ClickBoxItem box;
        QVERIFY(MeterWidget::itemFeatureBuilt(&box));
        ContainerContentRegistry registry;
        QVERIFY(registry.isAvailable(QStringLiteral("CLICKBOX")));
    }
};

QTEST_MAIN(TstClickBoxItem)
#include "tst_click_box_item.moc"
